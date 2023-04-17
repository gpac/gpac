/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom Paris 2019-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / MPEG Transport Stream splitter filter
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <gpac/filters.h>
#include <gpac/constants.h>
#include <gpac/thread.h>
#include <gpac/list.h>
#include <gpac/bitstream.h>

#include <gpac/mpegts.h>

#ifndef GPAC_DISABLE_MPEG2TS

typedef struct
{
	GF_FilterPid *opid;
	u32 pmt_pid;
	u8 pat_pck[192];
	u32 pat_pck_size;

	u8 *pck_buffer;
	u32 nb_pck;
} GF_M2TSSplit_SPTS;


typedef struct
{
	//options
	Bool dvb;
	s32 mux_id;
	Bool avonly;
	u32 nb_pack;

	//internal
	GF_Filter *filter;
	GF_FilterPid *ipid;

	GF_List *streams;

	GF_M2TS_Demuxer *dmx;

	u8 tsbuf[192];
	GF_BitStream *bsw;

} GF_M2TSSplitCtx;



void m2tssplit_send_packet(GF_M2TSSplitCtx *ctx, GF_M2TSSplit_SPTS *stream, u8 *data, u32 size)
{
	u8 *buffer;
	GF_FilterPacket *pck;
	if (ctx->nb_pack) {
		assert (stream->nb_pck<ctx->nb_pack);
		if (data) {
			memcpy(stream->pck_buffer + size*stream->nb_pck, data, size);
			stream->nb_pck++;
			if (stream->nb_pck<ctx->nb_pack) {
				return;
			}
		}
		u32 osize = size*stream->nb_pck;
		pck = gf_filter_pck_new_alloc(stream->opid, osize, &buffer);
		if (pck) {
			gf_filter_pck_set_framing(pck, GF_FALSE, GF_FALSE);
			memcpy(buffer, stream->pck_buffer, osize);
			gf_filter_pck_send(pck);
		}
		stream->nb_pck = 0;
	} else {
		pck = gf_filter_pck_new_alloc(stream->opid, size, &buffer);
		if (pck) {
			gf_filter_pck_set_framing(pck, GF_FALSE, GF_FALSE);
			memcpy(buffer, data, size);
			gf_filter_pck_send(pck);
		}
	}
}

void m2tssplit_flush(GF_M2TSSplitCtx *ctx)
{
	u32 i;
	if (!ctx->nb_pack) return;

	for (i=0; i<gf_list_count(ctx->streams); i++ ) {
		GF_M2TSSplit_SPTS *stream = gf_list_get(ctx->streams, i);
		if (stream->opid && stream->nb_pck)
			m2tssplit_send_packet(ctx, stream, NULL, 0);
	}

}

GF_Err m2tssplit_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_M2TSSplitCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		ctx->ipid = NULL;
		m2tssplit_flush(ctx);
		while (gf_list_count(ctx->streams) ) {
			GF_M2TSSplit_SPTS *st = gf_list_pop_back(ctx->streams);
			if (st->opid) gf_filter_pid_remove(st->opid);
			if (st->pck_buffer) gf_free(st->pck_buffer);
			gf_free(st);
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	ctx->ipid = pid;
	return GF_OK;
}

static Bool m2tssplit_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
//	GF_M2TSSplitCtx *ctx = gf_filter_get_udta(filter);

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		//cancel event
		return GF_TRUE;

	case GF_FEVT_STOP:
		return GF_FALSE;

	case GF_FEVT_SET_SPEED:
		//cancel event
		return GF_TRUE;
	default:
		break;
	}
	//by default don't cancel event - to rework once we have downloading in place
	return GF_FALSE;
}

GF_Err m2tssplit_process(GF_Filter *filter)
{
	GF_M2TSSplitCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck;
	const u8 *data;
	u32 data_size;
	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid))
			m2tssplit_flush(ctx);
		return GF_OK;
	}
	data = gf_filter_pck_get_data(pck, &data_size);
	if (data) {
		gf_m2ts_process_data(ctx->dmx, (u8 *)data, data_size);
	}
	gf_filter_pid_drop_packet(ctx->ipid);
	return GF_OK;
}


void m2tssplit_on_event(struct tag_m2ts_demux *ts, u32 evt_type, void *par)
{
	u32 i;
	GF_M2TSSplitCtx *ctx = ts->user;

	if ((evt_type==GF_M2TS_EVT_PAT_FOUND) || (evt_type==GF_M2TS_EVT_PAT_UPDATE)) {
		//todo, purge previous programs if PMT PID changes
		for (i=0; i<gf_list_count(ctx->dmx->programs); i++) {
			GF_M2TS_SectionInfo *sinfo = par;
			GF_M2TSSplit_SPTS *stream=NULL;
			u32 j, pck_size, tot_len=188, crc, offset=5, mux_id;
			u8 *buffer;
			Bool first_pck=GF_FALSE;
			GF_M2TS_Program *prog = gf_list_get(ctx->dmx->programs, i);
			assert(prog->pmt_pid);

			for (j=0; j<gf_list_count(ctx->streams); j++) {
				stream = gf_list_get(ctx->streams, j);
				if (stream->pmt_pid==prog->pmt_pid)
					break;
				stream = NULL;
			}
			if (!stream) {
				GF_SAFEALLOC(stream, GF_M2TSSplit_SPTS);
				if (!stream) return;
				stream->pmt_pid = prog->pmt_pid;
				first_pck = GF_TRUE;
				prog->user = stream;
				if (ctx->nb_pack)
					stream->pck_buffer = gf_malloc(sizeof(char) * ctx->nb_pack * (ctx->dmx->prefix_present ? 192 : 188) );

				gf_list_add(ctx->streams, stream);

				//do not create output stream until we are sure we have AV component in program
			}

			//generate a pat
			gf_bs_seek(ctx->bsw, 0);
			if (ctx->dmx->prefix_present) {
				tot_len += 4;
				offset += 4;
				gf_bs_write_u32(ctx->bsw, 1);
			}
			gf_bs_write_u8(ctx->bsw, 0x47);
			gf_bs_write_int(ctx->bsw,	0, 1);    // error indicator
			gf_bs_write_int(ctx->bsw,	1, 1); // start ind
			gf_bs_write_int(ctx->bsw,	0, 1);	  // transport priority
			gf_bs_write_int(ctx->bsw,	0, 13); // pid
			gf_bs_write_int(ctx->bsw,	0, 2);    // scrambling
			gf_bs_write_int(ctx->bsw,	1, 2);    // we do not use adaptation field for sections
			gf_bs_write_int(ctx->bsw,	0, 4);   // continuity counter
			gf_bs_write_u8(ctx->bsw,	0);   // ptr_field
			gf_bs_write_int(ctx->bsw,	GF_M2TS_TABLE_ID_PAT, 8);
			gf_bs_write_int(ctx->bsw,	1/*use_syntax_indicator*/, 1);
			gf_bs_write_int(ctx->bsw,	0, 1);
			gf_bs_write_int(ctx->bsw,	3, 2);  /* reserved bits are all set */
			gf_bs_write_int(ctx->bsw,	5 + 4 + 4, 12); //syntax indicator section + PAT payload + CRC32
			/*syntax indicator section*/

			if (ctx->mux_id>=0) {
				mux_id = ctx->mux_id;
			} else {
				//we use muxID*255 + streamIndex as the target mux ID if not configured
				mux_id = sinfo->ex_table_id;
				mux_id *= 255;
			}
			mux_id += i;

			gf_bs_write_int(ctx->bsw,	mux_id, 16);
			gf_bs_write_int(ctx->bsw,	3, 2); /* reserved bits are all set */
			gf_bs_write_int(ctx->bsw,	sinfo->version_number, 5);
			gf_bs_write_int(ctx->bsw,	1, 1); /* current_next_indicator = 1: we don't send version in advance */
			gf_bs_write_int(ctx->bsw,	0, 8); //current section number
			gf_bs_write_int(ctx->bsw,	0, 8); //last section number

			/*PAT payload*/
			gf_bs_write_u16(ctx->bsw, prog->number);
			gf_bs_write_int(ctx->bsw, 0x7, 3);	/*reserved*/
			gf_bs_write_int(ctx->bsw, prog->pmt_pid, 13);	/*reserved*/

			pck_size = (u32) gf_bs_get_position(ctx->bsw);
			/*write CRC32 starting from first field in section until last before CRC*/
			crc = gf_crc_32(ctx->tsbuf + offset, pck_size - offset);
			ctx->tsbuf[pck_size] = (crc >> 24) & 0xFF;
			ctx->tsbuf[pck_size+1] = (crc >> 16) & 0xFF;
			ctx->tsbuf[pck_size+2] = (crc  >> 8) & 0xFF;
			ctx->tsbuf[pck_size+3] = crc & 0xFF;
			pck_size += 4;

			//pad the rest to 0xFF
			memset(ctx->tsbuf + pck_size, 0xFF, tot_len - pck_size);
			//copy over for PAT repeat
			memcpy(stream->pat_pck, ctx->tsbuf, tot_len);
			stream->pat_pck_size = tot_len;

			//output new PAT
			if (stream->opid) {
				GF_FilterPacket *pck = gf_filter_pck_new_alloc(stream->opid, tot_len, &buffer);
				if (pck) {
					gf_filter_pck_set_framing(pck, first_pck, GF_FALSE);
					memcpy(buffer, ctx->tsbuf, tot_len);
					gf_filter_pck_send(pck);
				}
			}
		}
		return;
	}
	//resend PAT
	if (evt_type==GF_M2TS_EVT_PAT_REPEAT) {
		for (i=0; i<gf_list_count(ctx->dmx->programs); i++) {
			u8 *buffer;
			GF_M2TS_Program *prog = gf_list_get(ctx->dmx->programs, i);
			GF_M2TSSplit_SPTS *stream = prog->user;
			if (!stream) continue;
			if (!stream->opid) continue;

			GF_FilterPacket *pck = gf_filter_pck_new_alloc(stream->opid, stream->pat_pck_size, &buffer);
			if (pck) {
				gf_filter_pck_set_framing(pck, GF_FALSE, GF_FALSE);
				memcpy(buffer, stream->pat_pck, stream->pat_pck_size);
				gf_filter_pck_send(pck);
			}
		}
		return;
	}
	if ((evt_type==GF_M2TS_EVT_PMT_FOUND) || (evt_type==GF_M2TS_EVT_PMT_UPDATE)) {
		GF_M2TS_Program *prog = par;
		GF_M2TSSplit_SPTS *stream = prog->user;
		u32 known_streams = 0;
		for (i=0; i<gf_list_count(prog->streams); i++) {
			GF_M2TS_ES *es = gf_list_get(prog->streams, i);
			switch (es->stream_type) {
			case GF_M2TS_VIDEO_MPEG1:
			case GF_M2TS_VIDEO_MPEG2:
			case GF_M2TS_VIDEO_DCII:
			case GF_M2TS_VIDEO_MPEG4:
			case GF_M2TS_VIDEO_H264:
			case GF_M2TS_VIDEO_SVC:
			case GF_M2TS_VIDEO_HEVC:
			case GF_M2TS_VIDEO_HEVC_TEMPORAL:
			case GF_M2TS_VIDEO_HEVC_MCTS:
			case GF_M2TS_VIDEO_SHVC:
			case GF_M2TS_VIDEO_SHVC_TEMPORAL:
			case GF_M2TS_VIDEO_MHVC:
			case GF_M2TS_VIDEO_MHVC_TEMPORAL:
			case GF_M2TS_VIDEO_VVC:
			case GF_M2TS_VIDEO_VVC_TEMPORAL:
			case GF_M2TS_VIDEO_VC1:
			case GF_M2TS_AUDIO_MPEG1:
			case GF_M2TS_AUDIO_MPEG2:
			case GF_M2TS_AUDIO_LATM_AAC:
			case GF_M2TS_AUDIO_AAC:
			case GF_CODECID_AAC_MPEG2_MP:
			case GF_CODECID_AAC_MPEG2_LCP:
			case GF_CODECID_AAC_MPEG2_SSRP:
			case GF_M2TS_AUDIO_AC3:
			case GF_M2TS_AUDIO_EC3:
			case GF_M2TS_SYSTEMS_MPEG4_SECTIONS:
			case GF_M2TS_SYSTEMS_MPEG4_PES:
			case GF_M2TS_METADATA_PES:
			case GF_M2TS_METADATA_ID3_HLS:
				known_streams++;
				break;
			default:
				break;
			}
		}
		//if avonly mode and no known streams, do not forward program
		if (ctx->avonly && !known_streams)
			return;
		//good to go, create output and send PAT
		if (!stream->opid) {
			u8 *buffer;

			stream->opid = gf_filter_pid_new(ctx->filter);
			gf_filter_pid_set_property(stream->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE));
			gf_filter_pid_set_property(stream->opid, GF_PROP_PID_MIME, &PROP_STRING("video/mpeg-2"));
			gf_filter_pid_set_property(stream->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("ts"));
			gf_filter_pid_set_property(stream->opid, GF_PROP_PID_SERVICE_ID, &PROP_UINT(prog->number));

			GF_FilterPacket *pck = gf_filter_pck_new_alloc(stream->opid, stream->pat_pck_size, &buffer);
			if (pck) {
				gf_filter_pck_set_framing(pck, GF_TRUE, GF_FALSE);
				memcpy(buffer, stream->pat_pck, stream->pat_pck_size);
				gf_filter_pck_send(pck);
			}
		}
		return;
	}

	if (evt_type==GF_M2TS_EVT_PCK) {
		GF_M2TS_TSPCK *tspck = par;
		Bool do_fwd;
		GF_M2TSSplit_SPTS *stream = tspck->stream ? tspck->stream->program->user : NULL;

		if (stream) {
			if (!stream->opid) return;

			if (ctx->dmx->prefix_present) {
				u8 *data = tspck->data;
				data -= 4;
				m2tssplit_send_packet(ctx, stream, data, 192);
			} else {
				m2tssplit_send_packet(ctx, stream, tspck->data, 188);
			}
			return;
		}

		if (!ctx->dvb) return;

		do_fwd = GF_FALSE;
		switch (tspck->pid) {
		case GF_M2TS_PID_CAT:
		case GF_M2TS_PID_TSDT:
		case GF_M2TS_PID_NIT_ST:
		case GF_M2TS_PID_SDT_BAT_ST:
		case GF_M2TS_PID_EIT_ST_CIT:
		case GF_M2TS_PID_RST_ST:
		case GF_M2TS_PID_TDT_TOT_ST:
		case GF_M2TS_PID_NET_SYNC:
		case GF_M2TS_PID_RNT:
		case GF_M2TS_PID_IN_SIG:
		case GF_M2TS_PID_MEAS:
		case GF_M2TS_PID_DIT:
		case GF_M2TS_PID_SIT:
			do_fwd = GF_TRUE;
		}
		if (do_fwd) {
			u32 count = gf_list_count(ctx->streams);
			for (i=0; i<count; i++) {
				stream = gf_list_get(ctx->streams, i);
				if (!stream->opid) continue;

				if (ctx->dmx->prefix_present) {
					u8 *data = tspck->data;
					data -= 4;
					m2tssplit_send_packet(ctx, stream, data, 192);
				} else {
					m2tssplit_send_packet(ctx, stream, tspck->data, 188);
				}
			}
		}
	}
}

GF_Err m2tssplit_initialize(GF_Filter *filter)
{
	GF_M2TSSplitCtx *ctx = gf_filter_get_udta(filter);
	ctx->streams = gf_list_new();
	ctx->dmx = gf_m2ts_demux_new();
	ctx->dmx->on_event = m2tssplit_on_event;
	ctx->dmx->split_mode = GF_TRUE;
	ctx->dmx->user = ctx;
	ctx->filter = filter;
	ctx->bsw = gf_bs_new(ctx->tsbuf, 192, GF_BITSTREAM_WRITE);
	if (ctx->nb_pack<=1)
		ctx->nb_pack = 0;
	return GF_OK;
}

void m2tssplit_finalize(GF_Filter *filter)
{
	GF_M2TSSplitCtx *ctx = gf_filter_get_udta(filter);

	while (gf_list_count(ctx->streams)) {
		GF_M2TSSplit_SPTS *st = gf_list_pop_back(ctx->streams);
		if (st->pck_buffer) gf_free(st->pck_buffer);
		gf_free(st);
	}
	gf_list_del(ctx->streams);
	gf_bs_del(ctx->bsw);
	gf_m2ts_demux_del(ctx->dmx);
}

static const GF_FilterCapability M2TSSplitCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_FILE_EXT, "ts|m2t|mts|dmb|trp"),
	CAP_STRING(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_MIME, "video/mpeg-2|video/mp2t|video/mpeg"),
};

#define OFFS(_n)	#_n, offsetof(GF_M2TSSplitCtx, _n)
static const GF_FilterArgs M2TSSplitArgs[] =
{
	{ OFFS(dvb), "forward all packets from global DVB PIDs", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(mux_id), "set initial ID of output mux; the first program will use mux_id, the second mux_id+1, etc. If not set, this value will be set to sourceMuxId*255", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(avonly), "do not forward programs with no AV component", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(nb_pack), "pack N packets before sending", GF_PROP_UINT, "10", NULL, GF_FS_ARG_HINT_ADVANCED},

	{0}
};



GF_FilterRegister M2TSSplitRegister = {
	.name = "tssplit",
	GF_FS_SET_DESCRIPTION("MPEG Transport Stream splitter")
	GF_FS_SET_HELP("This filter splits an MPEG-2 transport stream into several single program transport streams.\n"
	"Only the PAT table is rewritten, other tables (PAT, PMT) and streams (PES) are forwarded as is.\n"
	"If [-dvb]() is set, global DVB tables of the input multiplex are forwarded to each output mux; otherwise these tables are discarded.")
	.flags = GF_FS_REG_EXPLICIT_ONLY,
	.private_size = sizeof(GF_M2TSSplitCtx),
	.initialize = m2tssplit_initialize,
	.finalize = m2tssplit_finalize,
	.args = M2TSSplitArgs,
	SETCAPS(M2TSSplitCaps),
	.configure_pid = m2tssplit_configure_pid,
	.process = m2tssplit_process,
	.process_event = m2tssplit_process_event,
};

const GF_FilterRegister *tssplit_register(GF_FilterSession *session)
{
	return &M2TSSplitRegister;
}

#else
const GF_FilterRegister *tssplit_register(GF_FilterSession *session)
{
	return NULL;
}

#endif /*GPAC_DISABLE_MPEG2TS*/
