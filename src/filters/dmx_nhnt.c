/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2017
 *					All rights reserved
 *
 *  This file is part of GPAC / NHNT demuxer filter
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

#ifndef GPAC_DISABLE_AV_PARSERS
#include <gpac/avparse.h>
#endif


enum
{
	GF_MEDIA_TYPE_NHNL 	= GF_4CC('N','H','n','l'),
	GF_MEDIA_TYPE_NHNT 	= GF_4CC('N','H','n','t')
};

typedef struct
{
	u64 pos;
	Double duration;
} NHNTIdx;

typedef struct
{
	//opts
	Bool reframe;
	Double index;

	GF_FilterPid *ipid;
	GF_FilterPid *opid;

	Double start_range;
	u64 first_dts;

	Bool is_playing;
	GF_Fraction duration;
	Bool need_reassign, in_seek;

	Bool initial_play_done;
	Bool header_parsed;
	u32 sig;
	u32 timescale;

	FILE *mdia;
	GF_BitStream *bs;


	NHNTIdx *indexes;
	u32 index_alloc_size, index_size;
} GF_NHNTDmxCtx;



static void nhntdmx_check_dur(GF_NHNTDmxCtx *ctx)
{
	GF_Fraction dur;
	FILE *stream;
	GF_BitStream *bs;
	u32 sig, timescale;
	u64 dts_prev;
	u32 prev_dur;
	u64 cur_dur=0;
	const GF_PropertyValue *p;

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILEPATH);
	if (!p || !p->value.string) {
		ctx->duration.num = 1;
		return;
	}
	if (ctx->duration.num) return;

	stream = gf_fopen(p->value.string, "rb");
	if (!stream) return;

//	ctx->index_size = 0;

	bs = gf_bs_from_file(stream, GF_BITSTREAM_READ);


	sig = GF_4CC(gf_bs_read_u8(bs), gf_bs_read_u8(bs), gf_bs_read_u8(bs), gf_bs_read_u8(bs));
	if (sig == GF_MEDIA_TYPE_NHNT) sig = 0;
	else if (sig == GF_MEDIA_TYPE_NHNL) sig = 1;
	else {
		gf_bs_del(bs);
		gf_fclose(stream);
		return;
	}
	gf_bs_read_u8(bs); /*version*/
	gf_bs_read_u8(bs); //streamType
	gf_bs_read_u8(bs); //oti
	gf_bs_read_u16(bs);
	gf_bs_read_u24(bs); //bufferSizeDB
	gf_bs_read_u32(bs); //avgBitrate
	gf_bs_read_u32(bs); //maxBitrate
	timescale = gf_bs_read_u32(bs);

	dur.num = 0;
	dts_prev = 0;
	prev_dur = 0;
	while (gf_bs_available(bs)) {
		u64 dts;
		u64 pos  =gf_bs_get_position(bs);
		/*u32 len = */gf_bs_read_u24(bs);
		Bool is_rap = gf_bs_read_int(bs, 1);
		/*Bool is_start = (Bool)*/gf_bs_read_int(bs, 1);
		/*Bool is_end = (Bool)*/gf_bs_read_int(bs, 1);
		/*3 reserved + AU type (2)*/
		gf_bs_read_int(bs, 5);

		if (sig) {
			/*offset = */gf_bs_read_u64(bs);
			/*cts = */gf_bs_read_u64(bs);
			dts = gf_bs_read_u64(bs);
		} else {
			/*offset = */gf_bs_read_u32(bs);
			/*cts = */gf_bs_read_u32(bs);
			dts = gf_bs_read_u32(bs);
		}
		if (!ctx->first_dts)
			ctx->first_dts = 1 + dts;
		prev_dur = (u32) (dts - dts_prev);
		dur.num += prev_dur;
		dts_prev = dts;

		cur_dur += prev_dur;
		//only index at I-frame start
		if (is_rap && (cur_dur >= ctx->index * ctx->timescale) ) {
			if (!ctx->index_alloc_size) ctx->index_alloc_size = 10;
			else if (ctx->index_alloc_size == ctx->index_size) ctx->index_alloc_size *= 2;
			ctx->indexes = gf_realloc(ctx->indexes, sizeof(NHNTIdx)*ctx->index_alloc_size);
			ctx->indexes[ctx->index_size].pos = pos;
			ctx->indexes[ctx->index_size].duration = dur.num;
			ctx->indexes[ctx->index_size].duration /= ctx->timescale;
			ctx->index_size ++;
			cur_dur = 0;
		}

	}
	gf_bs_del(bs);
	gf_fclose(stream);

	dur.num += prev_dur;
	dur.den = timescale;
	if (!ctx->duration.num || (ctx->duration.num * dur.den != dur.num * ctx->duration.den)) {
		ctx->duration = dur;
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC(ctx->duration));
	}
}


GF_Err nhntdmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_NHNTDmxCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		ctx->ipid = NULL;
		//gf_filter_pid_remove(st->opid);

		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	ctx->ipid = pid;
	gf_filter_pid_set_framing_mode(pid, GF_TRUE);

	return GF_OK;
}

static Bool nhntdmx_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_FilterEvent fevt;
	u64 file_pos = 0;
	u32 i;
	GF_NHNTDmxCtx *ctx = gf_filter_get_udta(filter);

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if (ctx->is_playing && (ctx->start_range ==  evt->play.start_range)) {
			return GF_TRUE;
		}
		nhntdmx_check_dur(ctx);
		ctx->start_range = evt->play.start_range;
		ctx->is_playing = GF_TRUE;

		if (ctx->start_range) {
			for (i=1; i<ctx->index_size; i++) {
				if (ctx->indexes[i].duration>ctx->start_range) {
					file_pos = ctx->indexes[i-1].pos;
					break;
				}
			}
		}

		if (!ctx->initial_play_done) {
			ctx->initial_play_done = GF_TRUE;
			//seek will not change the current source state, don't send a seek
			if (!file_pos)
				return GF_TRUE;
		}
		//post a seek
		ctx->need_reassign = GF_TRUE;
		ctx->in_seek = GF_TRUE;
		ctx->header_parsed = file_pos ? GF_TRUE : GF_FALSE;
		GF_FEVT_INIT(fevt, GF_FEVT_SOURCE_SEEK, ctx->ipid);
		fevt.seek.start_offset = file_pos;
		gf_filter_pid_send_event(ctx->ipid, &fevt);

		//cancel event
		return GF_TRUE;

	case GF_FEVT_STOP:
		ctx->is_playing = GF_FALSE;
		//don't cancel event
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

GF_Err nhntdmx_process(GF_Filter *filter)
{
	GF_NHNTDmxCtx *ctx = gf_filter_get_udta(filter);
	const GF_PropertyValue *p;
	GF_FilterPacket *pck;
	u32 pkt_size;
	Bool start, end;
	const char *data;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) gf_filter_pid_set_eos(ctx->opid);
		return GF_OK;
	}
	data = gf_filter_pck_get_data(pck, &pkt_size);
	gf_filter_pck_get_framing(pck, &start, &end);
	//for now we only work with complete files
	assert(end);

	if (!ctx->bs) {
		ctx->bs = gf_bs_new(data, pkt_size, GF_BITSTREAM_READ);
	} else if (ctx->need_reassign) {
		ctx->need_reassign = GF_FALSE;
		gf_bs_reassign_buffer(ctx->bs, data, pkt_size);
	}

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILEPATH);
	if (!p) {
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_NOT_SUPPORTED;
	}

	if (!ctx->header_parsed) {
		if (!ctx->opid) {
			char *ext;
			char szMedia[1000];
			char *dsi;
			u32 dsi_size;
			u32 val, oti;
			u64 media_size;

			strcpy(szMedia, p->value.string);
			ext = strrchr(szMedia, '.');
			if (ext) ext[0] = 0;
			strcat(szMedia, ".media");
			ctx->mdia = gf_fopen(szMedia, "rb");
			if (!ctx->mdia) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[NHNT] Cannot find MEDIA file %s", szMedia));
				gf_filter_pid_drop_packet(ctx->ipid);
				return GF_URL_ERROR;
			}

			ctx->sig = GF_4CC(gf_bs_read_u8(ctx->bs), gf_bs_read_u8(ctx->bs), gf_bs_read_u8(ctx->bs), gf_bs_read_u8(ctx->bs));
			if (ctx->sig == GF_MEDIA_TYPE_NHNT) ctx->sig = 0;
			else if (ctx->sig == GF_MEDIA_TYPE_NHNL) ctx->sig = 1;
			else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[NHNT] Invalid NHNT signature %s\n", gf_4cc_to_str(ctx->sig) ));
				gf_filter_pid_drop_packet(ctx->ipid);
				return GF_NON_COMPLIANT_BITSTREAM;
			}
			ctx->opid = gf_filter_pid_new(filter);

			//we change the file path of the pid to point to the media stream, not the nhnt stream
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILEPATH, & PROP_STRING(szMedia));

			gf_fseek(ctx->mdia, 0, SEEK_END);
			media_size = gf_ftell(ctx->mdia);
			gf_fseek(ctx->mdia, 0, SEEK_SET);
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DOWN_SIZE, & PROP_LONGUINT(media_size) );

			/*version*/
			gf_bs_read_u8(ctx->bs);

			val = gf_bs_read_u8(ctx->bs);
			if (val == GF_STREAM_OD) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[NHNT] OD stream detected, might result in broken import\n"));
			}
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(val));

			oti = gf_bs_read_u8(ctx->bs);
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(oti));

			gf_bs_read_u16(ctx->bs);
			/*val = */gf_bs_read_u24(ctx->bs); //bufferSizeDB
			val = gf_bs_read_u32(ctx->bs); //avgBitrate
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_BITRATE, &PROP_UINT(val));
			/*val = */gf_bs_read_u32(ctx->bs); //maxBitrate
			ctx->timescale = gf_bs_read_u32(ctx->bs);
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(ctx->timescale));

			strcpy(szMedia, p->value.string);
			ext = strrchr(szMedia, '.');
			if (ext) ext[0] = 0;
			strcat(szMedia, ".info");

			dsi = NULL;
			if ( gf_file_load_data(szMedia, (u8 **) &dsi, &dsi_size) != GF_OK) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[NHNT] Failed to read decoder config\n"));
			} else {

#ifndef GPAC_DISABLE_AV_PARSERS
				if (oti==GF_CODECID_MPEG4_PART2) {
					GF_M4VDecSpecInfo cfg;
					GF_Err e = gf_m4v_get_config(dsi, dsi_size, &cfg);
					if (e>=0) {
						gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(cfg.width) );
						gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(cfg.height) );
						gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PROFILE_LEVEL, &PROP_UINT(cfg.VideoPL) );
						gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAR, &PROP_FRAC_INT(cfg.par_num, cfg.par_den) );
					}
				}
#endif

				gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(dsi, dsi_size) );
			}

			if (ctx->reframe)
				gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, &PROP_BOOL(ctx->reframe) );

			nhntdmx_check_dur(ctx);
			ctx->header_parsed = GF_TRUE;
		} else {
			/*version*/
			gf_bs_skip_bytes(ctx->bs, 4+1+1+1+2+3+4+4+4);
			ctx->header_parsed = GF_TRUE;
		}
	}

	if (!ctx->is_playing) return GF_OK;

	while (gf_bs_available(ctx->bs) >= 16 ) {
		GF_FilterPacket *dst_pck;
		u8 *data;
		u64 dts, cts, offset;
		u32 res, len = gf_bs_read_u24(ctx->bs);
		Bool is_rap = gf_bs_read_int(ctx->bs, 1);
		Bool is_start = (Bool)gf_bs_read_int(ctx->bs, 1);
		Bool is_end = (Bool)gf_bs_read_int(ctx->bs, 1);
		/*3 reserved + AU type (2)*/
		gf_bs_read_int(ctx->bs, 5);

		if (ctx->sig) {
			offset = gf_bs_read_u64(ctx->bs);
			cts = gf_bs_read_u64(ctx->bs);
			dts = gf_bs_read_u64(ctx->bs);
		} else {
			offset = gf_bs_read_u32(ctx->bs);
			cts = gf_bs_read_u32(ctx->bs);
			dts = gf_bs_read_u32(ctx->bs);
		}

		if (!ctx->first_dts)
			ctx->first_dts = 1 + dts;

		if (ctx->in_seek) {
			Double now = (Double) (dts - (ctx->first_dts-1) );
			now /= ctx->timescale;
			if (now >= ctx->start_range) ctx->in_seek = GF_FALSE;
		}
		gf_fseek(ctx->mdia, offset, SEEK_SET);

		dst_pck = gf_filter_pck_new_alloc(ctx->opid, len, &data);
		res = (u32) fread(data, 1, len, ctx->mdia);
		if (res != len) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[NHNT] Read failure, expecting %d bytes got %d", len, res));
		}
		gf_filter_pck_set_framing(dst_pck, is_start, is_end);
		if (is_rap)
			gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1);
		gf_filter_pck_set_dts(dst_pck, dts);
		gf_filter_pck_set_cts(dst_pck, cts);
		gf_filter_pck_set_byte_offset(dst_pck, offset);
		if (ctx->in_seek) gf_filter_pck_set_seek_flag(dst_pck, GF_TRUE);
		gf_filter_pck_send(dst_pck);

		if (gf_filter_pid_would_block(ctx->opid))
			return GF_OK;
	}
	gf_filter_pid_drop_packet(ctx->ipid);
	return GF_OK;
}

GF_Err nhntdmx_initialize(GF_Filter *filter)
{
//	GF_NHNTDmxCtx *ctx = gf_filter_get_udta(filter);
	return GF_OK;
}

void nhntdmx_finalize(GF_Filter *filter)
{
	GF_NHNTDmxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->mdia) gf_fclose(ctx->mdia);
	if (ctx->bs) gf_bs_del(ctx->bs);
	if (ctx->indexes) gf_free(ctx->indexes);
}


#define OFFS(_n)	#_n, offsetof(GF_NHNTDmxCtx, _n)
static const GF_FilterArgs GF_NHNTDmxArgs[] =
{
	{ OFFS(reframe), "force reparsing of referenced content", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(index), "indexing window length", GF_PROP_DOUBLE, "1.0", NULL, 0},
	{0}
};


static const GF_FilterCapability NHNTDmxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "application/x-nhnt"),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_SCENE),
	{0},
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "nhnt"),
};


GF_FilterRegister NHNTDmxRegister = {
	.name = "nhntr",
	GF_FS_SET_DESCRIPTION("NHNT reader")
	GF_FS_SET_HELP("This filter reads NHNT files/data to produce a media PID and frames.\n"
	"NHNT documentation is available at https://github.com/gpac/gpac/wiki/NHNT-Format\n")
	.private_size = sizeof(GF_NHNTDmxCtx),
	.args = GF_NHNTDmxArgs,
	.initialize = nhntdmx_initialize,
	.finalize = nhntdmx_finalize,
	SETCAPS(NHNTDmxCaps),
	.configure_pid = nhntdmx_configure_pid,
	.process = nhntdmx_process,
	.process_event = nhntdmx_process_event
};

const GF_FilterRegister *nhntdmx_register(GF_FilterSession *session)
{
	return &NHNTDmxRegister;
}

