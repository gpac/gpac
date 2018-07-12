/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Romain Bouqueau, Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018
 *					All rights reserved
 *
 *  This file is part of GPAC / AV1 IVF/OBU/annexB reframer filter
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

#include <gpac/avparse.h>
#include <gpac/constants.h>
#include <gpac/filters.h>
#include <gpac/internal/media_dev.h>

typedef struct
{
	u64 pos;
	Double duration;
} AV1Idx;

typedef enum {
	NOT_SET,   /*Section 5*/
	OBUs,   /*Section 5*/
	AnnexB,
	IVF,
	UNSUPPORTED
} AV1BitstreamSyntax;

typedef struct
{
	//filter args
	GF_Fraction fps;
	Double index_dur;
	Bool autofps;

	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;

	AV1BitstreamSyntax bsmode;

	GF_BitStream *bs;
	u64 cts;
	u32 width, height;
	GF_Fraction duration;
	Double start_range;
	Bool in_seek;
	u32 timescale;

	u32 resume_from;

	char *buffer;
	u32 buf_size, alloc_size;

	//ivf header for now
	u32 file_hdr_size;

	Bool is_playing;
	Bool is_file, file_loaded;
	Bool initial_play_done;

	GF_FilterPacket *src_pck;

	AV1Idx *indexes;
	u32 index_alloc_size, index_size;

	AV1State state;
	u32 dsi_crc;
} GF_AV1DmxCtx;


GF_Err av1dmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	GF_AV1DmxCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		ctx->ipid = NULL;
		gf_filter_pid_remove(ctx->opid);
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	ctx->ipid = pid;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	if (p) ctx->timescale = p->value.uint;
	ctx->state.mem_mode = GF_TRUE;
	return GF_OK;
}

GF_Err av1dmx_check_format(GF_Filter *filter, GF_AV1DmxCtx *ctx, GF_BitStream *bs, u32 *last_obu_end)
{
	GF_Err e;
	if (last_obu_end) (*last_obu_end) = 0;
	//probing av1 bs mode
	if (ctx->bsmode != NOT_SET) return GF_OK;

	e = gf_media_aom_parse_ivf_file_header(bs, &ctx->state);
	if (!e) {
		ctx->bsmode = IVF;
		if (ctx->autofps && ctx->state.tb_num && ctx->state.tb_den && ! ( (ctx->state.tb_num<=1) && (ctx->state.tb_den<=1) ) ) {
			ctx->fps.num = ctx->state.tb_num;
			ctx->fps.den = ctx->state.tb_den;
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[AV1Dmx] Detected IVF format FPS %d/%d\n", ctx->fps.num, ctx->fps.den));
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[AV1Dmx] Detected IVF format\n"));
		}
		ctx->file_hdr_size = gf_bs_get_position(bs);
		if (last_obu_end) (*last_obu_end) = gf_bs_get_position(bs);
	} else {
		gf_bs_seek(bs, 0);
		e = aom_av1_parse_temporal_unit_from_annexb(bs, &ctx->state);
		if (!e) {
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[AV1Dmx] Detected Annex B format\n"));
			ctx->bsmode = AnnexB;
			gf_bs_seek(bs, 0);
		} else {
			ObuType obu_type;
			u64 obu_size;

			gf_bs_seek(bs, 0);
			e = aom_av1_parse_temporal_unit_from_section5(bs, &ctx->state);
			if (e && !gf_list_count(ctx->state.frame_state.frame_obus) ) {
				gf_filter_setup_failure(filter, e);
				ctx->bsmode = UNSUPPORTED;
				return e;
			}
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[AV1Dmx] Detected OBUs Section 5 format\n"));
			ctx->bsmode = OBUs;
			av1_reset_frame_state(&ctx->state.frame_state, GF_FALSE);
			gf_bs_seek(bs, 0);

			e = gf_media_aom_av1_parse_obu(bs, &obu_type, &obu_size, NULL, &ctx->state);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[AV1Dmx] Error parsing OBU (Section 5)\n"));
				gf_filter_setup_failure(filter, e);
				ctx->bsmode = UNSUPPORTED;
				return e;
			}
			if (obu_type != OBU_TEMPORAL_DELIMITER) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[AV1Dmx] Error OBU stream start with %s, not a temporal delimiter - NOT SUPPORTED\n", av1_get_obu_name(obu_type) ));
				gf_filter_setup_failure(filter, e);
				ctx->bsmode = UNSUPPORTED;
				return e;
			}
		}
	}
	ctx->state.config = gf_odf_av1_cfg_new();
	return GF_OK;
}


static void av1dmx_check_dur(GF_Filter *filter, GF_AV1DmxCtx *ctx)
{
	FILE *stream;
	GF_Err e;
	GF_BitStream *bs;
	u64 duration, cur_dur;
	AV1State av1state;
	const GF_PropertyValue *p;
	if (!ctx->opid || ctx->timescale || ctx->file_loaded) return;

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILEPATH);
	if (!p || !p->value.string) {
		ctx->is_file = GF_FALSE;
		return;
	}
	ctx->is_file = GF_TRUE;

	stream = gf_fopen(p->value.string, "rb");
	if (!stream) return;

	ctx->index_size = 0;

	bs = gf_bs_from_file(stream, GF_BITSTREAM_READ);

	if (ctx->file_hdr_size) {
		gf_bs_seek(bs, ctx->file_hdr_size);
	}
	memset(&av1state, 0, sizeof(AV1State));
	av1state.skip_frames = GF_TRUE;

	duration = 0;
	cur_dur = 0;
	while (gf_bs_available(bs)) {
		Bool is_sap;
		u64 frame_start = gf_bs_get_position(bs);
		av1_reset_frame_state(&av1state.frame_state, GF_FALSE);

		/*we process each TU and extract only the necessary OBUs*/
		switch (ctx->bsmode) {
		case OBUs:
			e = aom_av1_parse_temporal_unit_from_section5(bs, &av1state);
			break;
		case AnnexB:
			e = aom_av1_parse_temporal_unit_from_annexb(bs, &av1state);
			break;
		case IVF:
			e = aom_av1_parse_temporal_unit_from_ivf(bs, &av1state);
			break;
		default:
			e = GF_NOT_SUPPORTED;
		}
		if (e)
		 	break;

		duration += ctx->fps.den;
		cur_dur += ctx->fps.den;
		is_sap = av1state.frame_state.key_frame ? GF_TRUE : GF_FALSE;

		//only index at I-frame start
		if (frame_start && is_sap && (cur_dur > ctx->index_dur * ctx->fps.num) ) {
			if (!ctx->index_alloc_size) ctx->index_alloc_size = 10;
			else if (ctx->index_alloc_size == ctx->index_size) ctx->index_alloc_size *= 2;
			ctx->indexes = gf_realloc(ctx->indexes, sizeof(AV1Idx)*ctx->index_alloc_size);
			ctx->indexes[ctx->index_size].pos = frame_start;
			ctx->indexes[ctx->index_size].duration = (Double) duration;
			ctx->indexes[ctx->index_size].duration /= ctx->fps.num;
			ctx->index_size ++;
			cur_dur = 0;
		}
	}
	gf_bs_del(bs);
	gf_fclose(stream);
	av1_reset_frame_state(&av1state.frame_state, GF_TRUE);

	if (!ctx->duration.num || (ctx->duration.num  * ctx->fps.num != duration * ctx->duration.den)) {
		ctx->duration.num = (s32) duration;
		ctx->duration.den = ctx->fps.num;

		gf_filter_pid_set_info(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC(ctx->duration));
	}

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILE_CACHED);
	if (p && p->value.boolean) ctx->file_loaded = GF_TRUE;
	//currently not supported because of OBU size field rewrite - could work on some streams but we would
	//need to analyse all OBUs in the stream for that
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CAN_DATAREF, & PROP_BOOL(GF_FALSE) );
}


static Bool av1dmx_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	u32 i;
	u64 file_pos = 0;
	GF_FilterEvent fevt;
	GF_AV1DmxCtx *ctx = gf_filter_get_udta(filter);

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if (!ctx->is_playing) {
			ctx->is_playing = GF_TRUE;
			ctx->cts = 0;
		}
		if (! ctx->is_file) {
			ctx->buf_size = 0;
			return GF_FALSE;
		}
		ctx->start_range = evt->play.start_range;
		ctx->in_seek = GF_TRUE;

		if (ctx->start_range) {
			for (i=1; i<ctx->index_size; i++) {
				if (ctx->indexes[i].duration>ctx->start_range) {
					ctx->cts = (u64) (ctx->indexes[i-1].duration * ctx->fps.num);
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
		ctx->buf_size = 0;
		if (!file_pos)
			file_pos = ctx->file_hdr_size;

		//post a seek
		GF_FEVT_INIT(fevt, GF_FEVT_SOURCE_SEEK, ctx->ipid);
		fevt.seek.start_offset = file_pos;
		gf_filter_pid_send_event(ctx->ipid, &fevt);

		//cancel event
		return GF_TRUE;

	case GF_FEVT_STOP:
		//don't cancel event
		ctx->is_playing = GF_FALSE;
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

static GFINLINE void av1dmx_update_cts(GF_AV1DmxCtx *ctx)
{
	assert(ctx->fps.num);
	assert(ctx->fps.den);

	if (ctx->timescale) {
		u64 inc = ctx->fps.den;
		inc *= ctx->timescale;
		inc /= ctx->fps.num;
		ctx->cts += inc;
	} else {
		ctx->cts += ctx->fps.den;
	}
}

static void av1dmx_check_pid(GF_Filter *filter, GF_AV1DmxCtx *ctx)
{
	char *dsi;
	u32 dsi_size, crc;
	Bool first = GF_FALSE;

	//no config or no config change
	if (!gf_list_count(ctx->state.frame_state.header_obus)) return;

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		av1dmx_check_dur(filter, ctx);
		first = GF_TRUE;
	}

	//first or config changed, compute dsi
	while (gf_list_count(ctx->state.config->obu_array)) {
		GF_AV1_OBUArrayEntry *a = (GF_AV1_OBUArrayEntry*) gf_list_pop_back(ctx->state.config->obu_array);
		if (a->obu) gf_free(a->obu);
		gf_free(a);
	}
	dsi = NULL;
	dsi_size = 0;
	while (gf_list_count(ctx->state.frame_state.header_obus)) {
		GF_AV1_OBUArrayEntry *a = (GF_AV1_OBUArrayEntry*) gf_list_get(ctx->state.frame_state.header_obus, 0);
		gf_list_add(ctx->state.config->obu_array, a);
		gf_list_rem(ctx->state.frame_state.header_obus, 0);
	}
	gf_odf_av1_cfg_write(ctx->state.config, &dsi, &dsi_size);
	crc = gf_crc_32(dsi, dsi_size);

	if (crc == ctx->dsi_crc) {
		gf_free(dsi);
		return;
	}
	ctx->dsi_crc = crc;

	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, & PROP_UINT(GF_STREAM_VISUAL));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, & PROP_UINT(GF_CODECID_AV1));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, & PROP_UINT(ctx->fps.num));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FPS, & PROP_FRAC(ctx->fps));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, & PROP_UINT(ctx->state.width));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, & PROP_UINT(ctx->state.height));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, & PROP_DATA_NO_COPY(dsi, dsi_size));
}

GF_Err av1dmx_process_buffer(GF_Filter *filter, GF_AV1DmxCtx *ctx, const char *data, u32 data_size, Bool is_copy)
{
	u32 pos = 0;
	u32 last_obu_end = 0;
	GF_Err e = GF_OK;

	if (!ctx->bs) ctx->bs = gf_bs_new(data, data_size, GF_BITSTREAM_READ);
	else gf_bs_reassign_buffer(ctx->bs, data, data_size);

	e = av1dmx_check_format(filter, ctx, ctx->bs, &last_obu_end);
	if (e) return e;

	while (gf_bs_available(ctx->bs)) {
		u32 pck_size;
		GF_FilterPacket *pck;
		char *output;

		av1_reset_frame_state(&ctx->state.frame_state, GF_FALSE);
		ctx->state.frame_obus_size = 0;

		pos = gf_bs_get_position(ctx->bs);

		/*we process each TU and extract only the necessary OBUs*/
		switch (ctx->bsmode) {
		case OBUs:
			e = aom_av1_parse_temporal_unit_from_section5(ctx->bs, &ctx->state);
			break;
		case AnnexB:
			e = aom_av1_parse_temporal_unit_from_annexb(ctx->bs, &ctx->state);
			break;
		case IVF:
			e = aom_av1_parse_temporal_unit_from_ivf(ctx->bs, &ctx->state);
			break;
		default:
			e = GF_NOT_SUPPORTED;
		}
		if (e==GF_EOS) {
			e = GF_OK;
			break;
		}
		if (e)
		 	break;

		//check pid state
		av1dmx_check_pid(filter, ctx);

		if (!ctx->opid) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[AV1Dmx] output pid not configured (no sequence header yet ?), skiping OBU\n"));
			last_obu_end = (u32) gf_bs_get_position(ctx->bs);
			continue;
		}
		if (!ctx->is_playing)
			break;

		gf_bs_get_content_no_truncate(ctx->state.bs, &ctx->state.frame_obus, &ctx->state.frame_obus_size, &ctx->state.frame_obus_alloc);
		pck_size = ctx->state.frame_obus_size;
		ctx->state.frame_obus_size = 0;
		if (!pck_size) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[AV1Dmx] no frame OBU, skiping OBU\n"));
			last_obu_end = (u32) gf_bs_get_position(ctx->bs);
			continue;
		}

		pck = gf_filter_pck_new_alloc(ctx->opid, pck_size, &output);
		if (ctx->src_pck) gf_filter_pck_merge_properties(ctx->src_pck, pck);

		gf_filter_pck_set_cts(pck, ctx->cts);
		gf_filter_pck_set_sap(pck, ctx->state.frame_state.key_frame ? GF_FILTER_SAP_1 : 0);
		memcpy(output, ctx->state.frame_obus, pck_size);

		gf_filter_pck_send(pck);

		last_obu_end = (u32) gf_bs_get_position(ctx->bs);

		av1dmx_update_cts(ctx);
	}

	if (is_copy && last_obu_end) {
		assert(ctx->buf_size>=last_obu_end);
		memmove(ctx->buffer, ctx->buffer+last_obu_end, sizeof(char) * (ctx->buf_size-last_obu_end));
		ctx->buf_size -= last_obu_end;
	}
	return e;
}

GF_Err av1dmx_process(GF_Filter *filter)
{
	GF_Err e;
	GF_AV1DmxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck;
	char *data;
	u32 pck_size;

	if (ctx->bsmode == UNSUPPORTED) return GF_EOS;

	//always reparse duration
	if (!ctx->duration.num)
		av1dmx_check_dur(filter, ctx);

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			//flush
			if (ctx->buf_size) {
				av1dmx_process_buffer(filter, ctx, ctx->buffer, ctx->buf_size, GF_TRUE);
				ctx->buf_size = 0;
			}
			if (ctx->opid)
				gf_filter_pid_set_eos(ctx->opid);
			if (ctx->src_pck) gf_filter_pck_unref(ctx->src_pck);
			ctx->src_pck = NULL;
			return GF_EOS;
		}
		return GF_OK;
	}

	if (ctx->opid) {
		if (!ctx->is_playing || gf_filter_pid_would_block(ctx->opid))
			return GF_OK;
	}

	data = (char *) gf_filter_pck_get_data(pck, &pck_size);

	//input pid sets some timescale - we flushed pending data , update cts
	if (ctx->timescale) {
		Bool start, end;
		u64 cts;

		e = GF_OK;

		gf_filter_pck_get_framing(pck, &start, &end);
		//middle or end of frame, reaggregation
		if (!start) {
			if (ctx->alloc_size < ctx->buf_size + pck_size) {
				ctx->alloc_size = ctx->buf_size + pck_size;
				ctx->buffer = gf_realloc(ctx->buffer, ctx->alloc_size);
			}
			memcpy(ctx->buffer+ctx->buf_size, data, pck_size);
			ctx->buf_size += pck_size;

			//end of frame, process av1
			if (end) {
				e = av1dmx_process_buffer(filter, ctx, ctx->buffer, ctx->buf_size, GF_TRUE);
			}
			ctx->buf_size=0;
			gf_filter_pid_drop_packet(ctx->ipid);
			return e;
		}
		//flush of pending frame (might have lost something)
		if (ctx->buf_size) {
			e = av1dmx_process_buffer(filter, ctx, ctx->buffer, ctx->buf_size, GF_TRUE);
			ctx->buf_size = 0;
			if (e) return e;
		}

		//begining of a new frame
		cts = gf_filter_pck_get_cts(pck);
		if (cts != GF_FILTER_NO_TS)
			ctx->cts = cts;
		if (ctx->src_pck) gf_filter_pck_unref(ctx->src_pck);
		ctx->src_pck = pck;
		gf_filter_pck_ref_props(&ctx->src_pck);
		ctx->buf_size = 0;

		if (!end) {
			if (ctx->alloc_size < ctx->buf_size + pck_size) {
				ctx->alloc_size = ctx->buf_size + pck_size;
				ctx->buffer = gf_realloc(ctx->buffer, ctx->alloc_size);
			}
			memcpy(ctx->buffer+ctx->buf_size, data, pck_size);
			ctx->buf_size += pck_size;
			gf_filter_pid_drop_packet(ctx->ipid);
			return GF_OK;
		}
		assert(start && end);
		//process
		e = av1dmx_process_buffer(filter, ctx, data, pck_size, GF_FALSE);

		gf_filter_pid_drop_packet(ctx->ipid);
		return e;
	}

	//not from framed stream, copy buffer
	if (ctx->alloc_size < ctx->buf_size + pck_size) {
		ctx->alloc_size = ctx->buf_size + pck_size;
		ctx->buffer = gf_realloc(ctx->buffer, ctx->alloc_size);
	}
	memcpy(ctx->buffer+ctx->buf_size, data, pck_size);
	ctx->buf_size += pck_size;
	e = av1dmx_process_buffer(filter, ctx, ctx->buffer, ctx->buf_size, GF_TRUE);
	gf_filter_pid_drop_packet(ctx->ipid);
	return e;
}

static void av1dmx_finalize(GF_Filter *filter)
{
	GF_AV1DmxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->bs) gf_bs_del(ctx->bs);
	if (ctx->indexes) gf_free(ctx->indexes);

	av1_reset_frame_state(&ctx->state.frame_state, GF_TRUE);
	if (ctx->state.config) gf_odf_av1_cfg_del(ctx->state.config);
	if (ctx->state.bs) gf_bs_del(ctx->state.bs);
	if (ctx->state.frame_obus) gf_free(ctx->state.frame_obus);
	if (ctx->buffer) gf_free(ctx->buffer);
}

static const GF_FilterCapability AV1DmxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "video/av1"),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_AV1),
	CAP_BOOL(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_UNFRAMED, GF_FALSE),
	{0},
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "ivf|obu|av1b"),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AV1),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
};

#define OFFS(_n)	#_n, offsetof(GF_AV1DmxCtx, _n)
static const GF_FilterArgs AV1DmxArgs[] =
{
	{ OFFS(fps), "import frame rate", GF_PROP_FRACTION, "25/1", NULL, GF_FALSE},
	{ OFFS(autofps), "detect FPS from bitstream, fallback to fps option if not possible", GF_PROP_BOOL, "true", NULL, GF_FALSE},
	{ OFFS(index_dur), "indexing window length", GF_PROP_DOUBLE, "1.0", NULL, GF_FALSE},
	{0}
};


GF_FilterRegister AV1DmxRegister = {
	.name = "rfav1",
	.description = "AV1 reframer",
	.private_size = sizeof(GF_AV1DmxCtx),
	.args = AV1DmxArgs,
	.finalize = av1dmx_finalize,
	SETCAPS(AV1DmxCaps),
	.configure_pid = av1dmx_configure_pid,
	.process = av1dmx_process,
	.process_event = av1dmx_process_event
};


const GF_FilterRegister *av1dmx_register(GF_FilterSession *session)
{
	return &AV1DmxRegister;
}
