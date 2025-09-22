/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom Paris 2019-2024
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
	u64 last_pcr_plus_one, init_clock, init_pcr;
	u32 pcr_id;
	Bool start_sent;

	//when restamping, gather tune-in packets before first PCR
	GF_FilterPacket *init_pck;

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
	Bool gendts;
	Bool kpad;
	Bool rt;

	//internal
	GF_Filter *filter;
	GF_FilterPid *ipid;

	GF_List *streams;

	GF_M2TS_Demuxer *dmx;

	u8 tsbuf[192];
	GF_BitStream *bsw;
	GF_M2TSSplit_SPTS *out;

	u64 filesize;
	u64 process_clock;
	u32 resched_next;
	u32 ts_pck_size;
	GF_Fraction64 duration;
	Bool initial_play_done;
} GF_M2TSSplitCtx;

static void m2tssplit_on_event(struct tag_m2ts_demux *ts, u32 evt_type, void *par);

void m2tssplit_send_packet(GF_M2TSSplitCtx *ctx, GF_M2TSSplit_SPTS *stream, u8 *data, u32 size, u64 pcr_plus_one)
{
	u8 *buffer;
	GF_FilterPacket *pck;

	if (ctx->gendts) {
		//aggregate everything until we have a PCR
		if (!stream->last_pcr_plus_one && !pcr_plus_one && data) {
			if (stream->init_pck)
				gf_filter_pck_expand(stream->init_pck, size, NULL, &buffer, NULL);
			else
				stream->init_pck = gf_filter_pck_new_alloc(stream->opid, size, &buffer);

			if (buffer)
				memcpy(buffer, data, size);
			return;
		}
		if (pcr_plus_one) {
			if (!stream->last_pcr_plus_one && ctx->rt) {
				stream->init_clock = gf_sys_clock_high_res();
				stream->init_pcr = pcr_plus_one;
			}
			stream->last_pcr_plus_one = pcr_plus_one;
			if (ctx->rt) {
				u64 pck_time = gf_timestamp_rescale(pcr_plus_one - stream->init_pcr, 27000000, 1000000);
				pck_time += stream->init_clock;
				u32 next_time = 0;
				if (pck_time > ctx->process_clock)
					next_time = (u32)(pck_time - ctx->process_clock);
				if (!ctx->resched_next || (ctx->resched_next > next_time))
					ctx->resched_next = next_time;
			}
		}
		if (stream->init_pck) {
			gf_filter_pck_set_framing(stream->init_pck, !stream->start_sent, GF_FALSE);
			stream->start_sent = GF_TRUE;
			gf_filter_pck_set_dts(stream->init_pck, stream->last_pcr_plus_one-1);
			gf_filter_pck_set_cts(stream->init_pck, stream->last_pcr_plus_one-1);
			gf_filter_pck_send(stream->init_pck);
			stream->init_pck = NULL;
		}
	} else {
		pcr_plus_one=0;
	}

	if (ctx->nb_pack) {
		assert (stream->nb_pck<ctx->nb_pack);
		if (data) {
			memcpy(stream->pck_buffer + size*stream->nb_pck, data, size);
			stream->nb_pck++;
			if (stream->nb_pck<ctx->nb_pack) {
				return;
			}
		}
		if (size)
			ctx->ts_pck_size = size;
		else
			size = ctx->ts_pck_size;

		u32 osize = size * stream->nb_pck;
		pck = gf_filter_pck_new_alloc(stream->opid, osize, &buffer);
		if (pck) {
			gf_filter_pck_set_framing(pck, !stream->start_sent, GF_FALSE);
			stream->start_sent = GF_TRUE;
			memcpy(buffer, stream->pck_buffer, osize);

			if (pcr_plus_one) stream->last_pcr_plus_one = pcr_plus_one;
			if (stream->last_pcr_plus_one) {
				gf_filter_pck_set_dts(pck, stream->last_pcr_plus_one-1);
				gf_filter_pck_set_cts(pck, stream->last_pcr_plus_one-1);
			}
			gf_filter_pck_send(pck);
		}
		stream->nb_pck = 0;
		return;
	}

	pck = gf_filter_pck_new_alloc(stream->opid, size, &buffer);
	if (pck) {
		gf_filter_pck_set_framing(pck, !stream->start_sent, GF_FALSE);
		stream->start_sent = GF_TRUE;
		memcpy(buffer, data, size);
		if (pcr_plus_one) stream->last_pcr_plus_one = pcr_plus_one;
		if (stream->last_pcr_plus_one) {
			gf_filter_pck_set_dts(pck, stream->last_pcr_plus_one-1);
			gf_filter_pck_set_cts(pck, stream->last_pcr_plus_one-1);
		}
		gf_filter_pck_send(pck);
	}
}

void m2tssplit_flush(GF_M2TSSplitCtx *ctx)
{
	u32 i;
	for (i=0; i<gf_list_count(ctx->streams); i++ ) {
		GF_M2TSSplit_SPTS *stream = gf_list_get(ctx->streams, i);
		if (!stream->opid) continue;
		if (stream->nb_pck)
			m2tssplit_send_packet(ctx, stream, NULL, 0, 0);
		gf_filter_pid_set_eos(stream->opid);
	}
}

typedef struct
{
	GF_M2TSSplitCtx *ctx;
	u64 first_pcr;
	u32 first_pcr_pid;
	u64 last_pcr;
	u32 pck_num;
	Bool abort;
} M2TSDurProber;

static void m2tssplit_on_event_duration_probe(GF_M2TS_Demuxer *ts, u32 evt_type, void *param)
{
	M2TSDurProber *prober = (M2TSDurProber *) ts->user;

	if (evt_type != GF_M2TS_EVT_PCK) return;
	GF_M2TS_TSPCK *tspck = param;
	if (!tspck->pcr_plus_one) return;
	if (!prober->first_pcr_pid) {
		prober->first_pcr_pid = tspck->pid;
		prober->first_pcr = tspck->pcr_plus_one;
		return;
	}
	if (prober->first_pcr_pid != tspck->pid) return;
	prober->last_pcr = tspck->pcr_plus_one;
	prober->pck_num = ts->pck_number;
	if ((tspck->pcr_plus_one < prober->first_pcr) || (tspck->pcr_plus_one - prober->first_pcr > 5*27000000)) {
		prober->abort = GF_TRUE;
	}
}

void m2ts_split_estimate_duration(GF_M2TSSplitCtx *ctx, GF_FilterPid *pid)
{
	if (ctx->duration.num) return;

	const GF_PropertyValue *p = gf_filter_pid_get_property(pid, GF_PROP_PID_FILEPATH);
	if (!p || !p->value.string || !strncmp(p->value.string, "gmem://", 7)) {
		return;
	}

	FILE *stream = gf_fopen(p->value.string, "rb");
	ctx->ipid = pid;

	if (!stream) return;

	M2TSDurProber prober;
	memset(&prober, 0, sizeof(M2TSDurProber));
	prober.ctx = ctx;
	ctx->dmx->seek_mode = GF_TRUE;
	ctx->dmx->user = &prober;
	ctx->dmx->on_event = m2tssplit_on_event_duration_probe;
	while (!gf_feof(stream)) {
		char buf[1880];
		u32 nb_read = (u32) gf_fread(buf, 1880, stream);
		gf_m2ts_process_data(ctx->dmx, buf, nb_read);
		if (prober.abort) break;
	}
	u64 dur = prober.last_pcr - prober.first_pcr;
	u32 size = prober.pck_num * (ctx->dmx->prefix_present ? 192 : 188);
	ctx->filesize = gf_fsize(stream);
	gf_fclose(stream);
	ctx->duration.num = gf_timestamp_rescale(dur, size, ctx->filesize);
	ctx->duration.den = 27000000;

	GF_M2TSRawMode mode = ctx->dmx->raw_mode;
	gf_m2ts_demux_del(ctx->dmx);
	ctx->dmx = gf_m2ts_demux_new();
	ctx->dmx->raw_mode = mode;
	ctx->dmx->user = ctx;
	ctx->dmx->on_event = m2tssplit_on_event;
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

	if (!ctx->ipid)
		m2ts_split_estimate_duration(ctx, pid);

	ctx->ipid = pid;
	if (ctx->dmx->raw_mode==GF_M2TS_RAW_FORWARD) {
		GF_M2TSSplit_SPTS *stream;
		GF_SAFEALLOC(stream, GF_M2TSSplit_SPTS);
		if (!stream) return GF_OUT_OF_MEM;
		if (ctx->nb_pack)
			stream->pck_buffer = gf_malloc(sizeof(char) * ctx->nb_pack * (ctx->dmx->prefix_present ? 192 : 188) );

		gf_list_add(ctx->streams, stream);

		stream->opid = gf_filter_pid_new(filter);
		gf_filter_pid_copy_properties(stream->opid, pid);
		gf_filter_pid_set_property(stream->opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(27000000) );
		if (ctx->duration.num) {
			gf_filter_pid_set_property(stream->opid, GF_PROP_PID_DURATION, &PROP_FRAC64(ctx->duration) );
			gf_filter_pid_set_property(stream->opid, GF_PROP_PID_PLAYBACK_MODE, &PROP_UINT(GF_PLAYBACK_MODE_SEEK) );
		}
		ctx->out = stream;
	}
	return GF_OK;
}

static Bool m2tssplit_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	u64 file_pos;
	u32 i;
	GF_FilterEvent fevt;
	GF_M2TSSplitCtx *ctx = gf_filter_get_udta(filter);

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		//cancel event if not file-based, mux or duation unknown
		if ((gf_list_count(ctx->streams)>1) || !ctx->filesize || !ctx->duration.num)
			return GF_TRUE;

		if (evt->play.hint_start_offset || evt->play.hint_end_offset) {
			file_pos = evt->play.hint_start_offset;
		} else {
			file_pos = (u64) (ctx->filesize * evt->play.start_range);
			file_pos *= ctx->duration.den;
			file_pos /= ctx->duration.num;
			if (file_pos > ctx->filesize) return GF_TRUE;
		}
		//round down to packet boundary
		file_pos /= ctx->dmx->prefix_present ? 192 : 188;
		file_pos *= ctx->dmx->prefix_present ? 192 : 188;

		if (!ctx->initial_play_done) {
			ctx->initial_play_done = GF_TRUE;
			//seek will not change the current source state, don't send a seek
			if (!file_pos) return GF_TRUE;
		}
		//post a seek
		GF_FEVT_INIT(fevt, GF_FEVT_SOURCE_SEEK, ctx->ipid);
		fevt.seek.start_offset = file_pos;
		gf_filter_pid_send_event(ctx->ipid, &fevt);
		return GF_TRUE;

	case GF_FEVT_STOP:
		if (gf_list_count(ctx->streams)>1)
			return GF_TRUE;

		//reset streams
		for (i=0;i<gf_list_count(ctx->streams); i++) {
			GF_M2TSSplit_SPTS *st = gf_list_get(ctx->streams, i);
			st->nb_pck = 0;
			//reset clock otherwise we would dispatch first packets before PCR with the previous PCR
			st->last_pcr_plus_one = 0;
			if (st->init_pck) {
				gf_filter_pck_discard(st->init_pck);
				st->init_pck = NULL;
			}
		}
		gf_m2ts_reset_parsers(ctx->dmx);
		return GF_FALSE;

	case GF_FEVT_SET_SPEED:
		//cancel event
		return GF_TRUE;
	case GF_FEVT_NETWORK_HINT:
		if (evt->net_hint.mtu_size) {
			u32 new_pack = evt->net_hint.mtu_size / (ctx->dmx->prefix_present ? 194 : 188);
			if (!new_pack) new_pack = 1;
			if (new_pack==ctx->nb_pack) return GF_TRUE;

			for (i=0;i<gf_list_count(ctx->streams); i++) {
				GF_M2TSSplit_SPTS *st = gf_list_get(ctx->streams, i);
				if (st->nb_pck>=new_pack)
					m2tssplit_send_packet(ctx, st, NULL, 0, 0);

				st->pck_buffer = gf_realloc(st->pck_buffer, new_pack * (ctx->dmx->prefix_present ? 192 : 188) );
			}
			ctx->nb_pack = new_pack;
		}
		return GF_TRUE;
	default:
		break;
	}
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
		if (ctx->rt) {
			ctx->process_clock = gf_sys_clock_high_res();
		}
		gf_m2ts_process_data(ctx->dmx, (u8 *)data, data_size);
	}
	gf_filter_pid_drop_packet(ctx->ipid);
	if (ctx->resched_next) {
		gf_filter_ask_rt_reschedule(filter, ctx->resched_next);
		ctx->resched_next = 0;
	}
	return GF_OK;
}


static void m2tssplit_on_event(struct tag_m2ts_demux *ts, u32 evt_type, void *par)
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
			gf_assert(prog->pmt_pid);

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
			case GF_M2TS_METADATA_ID3_KLVA:
			case GF_M2TS_SCTE35_SPLICE_INFO_SECTIONS:
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
			if (ctx->duration.num) {
				gf_filter_pid_set_property(stream->opid, GF_PROP_PID_DURATION, &PROP_FRAC64(ctx->duration) );
			}
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
		GF_M2TSSplit_SPTS *stream;
		if (ctx->out)
			stream = ctx->out;
		else
			stream = tspck->stream ? tspck->stream->program->user : NULL;

		if (ctx->gendts && tspck->pcr_plus_one) {
			if (!stream->pcr_id)
				stream->pcr_id = tspck->pid;
			else if (tspck->pid!=stream->pcr_id)
				tspck->pcr_plus_one = 0;
		}

		if (stream) {
			if (!stream->opid) return;
			if (!ctx->kpad && (tspck->pid==0x1FFF))
				return;

			if (ctx->dmx->prefix_present) {
				u8 *data = tspck->data;
				data -= 4;
				m2tssplit_send_packet(ctx, stream, data, 192, tspck->pcr_plus_one);
			} else {
				m2tssplit_send_packet(ctx, stream, tspck->data, 188, tspck->pcr_plus_one);
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
					m2tssplit_send_packet(ctx, stream, data, 192, 0);
				} else {
					m2tssplit_send_packet(ctx, stream, tspck->data, 188, 0);
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
	ctx->dmx->raw_mode = GF_M2TS_RAW_SPLIT;
	ctx->dmx->user = ctx;
	ctx->filter = filter;
	ctx->bsw = gf_bs_new(ctx->tsbuf, 192, GF_BITSTREAM_WRITE);
	if (ctx->nb_pack<=1)
		ctx->nb_pack = 0;

	if (ctx->rt)
		ctx->gendts = GF_TRUE;
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
	{ OFFS(gendts), "generate timestamps on output packets based on PCR", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(kpad), "keep padding (null) TS packets", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(rt), "enable real-time regulation", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{0}
};



GF_FilterRegister M2TSSplitRegister = {
	.name = "tssplit",
	GF_FS_SET_DESCRIPTION("MPEG-2 TS splitter")
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
	.hint_class_type = GF_FS_CLASS_STREAM
};

const GF_FilterRegister *tssplit_register(GF_FilterSession *session)
{
	return &M2TSSplitRegister;
}

static const GF_FilterCapability M2TSGenDTSCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_FILE_EXT, "ts|m2t|mts|dmb|trp"),
	CAP_STRING(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_MIME, "video/mpeg-2|video/mp2t|video/mpeg"),
	CAP_UINT(GF_CAPS_OUTPUT|GF_CAPFLAG_PRESENT, GF_PROP_PID_TIMESCALE, 0)
};


#define OFFS(_n)	#_n, offsetof(GF_M2TSSplitCtx, _n)
static const GF_FilterArgs M2TSGenDTSArgs[] =
{
	{ OFFS(nb_pack), "pack N packets before sending", GF_PROP_UINT, "10", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(kpad), "keep padding (null) TS packets", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(rt), "enable real-time regulation", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{0}
};

GF_Err m2ts_gendts_initialize(GF_Filter *filter)
{
	GF_Err e = m2tssplit_initialize(filter);
	if (e) return e;
	GF_M2TSSplitCtx *ctx = gf_filter_get_udta(filter);
	ctx->gendts = GF_TRUE;
	ctx->dmx->raw_mode = GF_M2TS_RAW_FORWARD;
	return GF_OK;
}


GF_FilterRegister M2TSRestampRegister = {
	.name = "tsgendts",
	GF_FS_SET_DESCRIPTION("MPEG-2 TS timestamper")
	GF_FS_SET_HELP("This filter restamps input MPEG-2 transport stream based on PCR.\n")
	.flags = GF_FS_REG_HIDE_WEIGHT,
	.private_size = sizeof(GF_M2TSSplitCtx),
	.initialize = m2ts_gendts_initialize,
	.finalize = m2tssplit_finalize,
	.args = M2TSGenDTSArgs,
	SETCAPS(M2TSGenDTSCaps),
	.configure_pid = m2tssplit_configure_pid,
	.process = m2tssplit_process,
	.process_event = m2tssplit_process_event,
	.hint_class_type = GF_FS_CLASS_STREAM
};

const GF_FilterRegister *tsgendts_register(GF_FilterSession *session)
{
	return &M2TSRestampRegister;
}

#else
const GF_FilterRegister *tssplit_register(GF_FilterSession *session)
{
	return NULL;
}
const GF_FilterRegister *tsgendts_register(GF_FilterSession *session)
{
	return NULL;
}

#endif /*GPAC_DISABLE_MPEG2TS*/
