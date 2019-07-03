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
	Double index;
	Bool importer;
	Bool deps;
	
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
	GF_Fraction cur_fps;

	u32 resume_from;

	char *buffer;
	u32 buf_size, alloc_size;

	//ivf header for now
	u32 file_hdr_size;

	Bool is_av1;
	Bool is_vp9;
	Bool is_vpX;
	u32 codecid;
	GF_VPConfig *vp_cfg;

	Bool is_playing;
	Bool is_file, file_loaded;
	Bool initial_play_done;

	GF_FilterPacket *src_pck;

	AV1Idx *indexes;
	u32 index_alloc_size, index_size;

	AV1State state;
	u32 dsi_crc;

	Bool pts_from_file;
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
	if (ctx->timescale && !ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, NULL);
	}
	if (ctx->timescale) {
		//if we have a FPS prop, use it
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_FPS);
		if (p) ctx->cur_fps = p->value.frac;
	}
	return GF_OK;
}

GF_Err av1dmx_check_format(GF_Filter *filter, GF_AV1DmxCtx *ctx, GF_BitStream *bs, u32 *last_obu_end)
{
	GF_Err e;
	if (last_obu_end) (*last_obu_end) = 0;
	//probing av1 bs mode
	if (ctx->bsmode != NOT_SET) return GF_OK;


	if (!ctx->state.config)
		ctx->state.config = gf_odf_av1_cfg_new();

	ctx->is_av1 = ctx->is_vp9 = ctx->is_vpX = GF_FALSE;
	ctx->codecid = 0;
	if (ctx->vp_cfg) gf_odf_vp_cfg_del(ctx->vp_cfg);
	ctx->vp_cfg = NULL;
	ctx->cur_fps = ctx->fps;
	if (!ctx->fps.num*ctx->fps.den) {
		ctx->cur_fps.num = 25000;
		ctx->cur_fps.den = 1000;
	}

	ctx->pts_from_file = GF_FALSE;
	if (gf_media_probe_ivf(bs)) {
		u32 width = 0, height = 0;
		u32 codec_fourcc = 0, frame_rate = 0, time_scale = 0, num_frames = 0;
		ctx->bsmode = IVF;

		e = gf_media_parse_ivf_file_header(bs, &width, &height, &codec_fourcc, &frame_rate, &time_scale, &num_frames);
		if (e) return e;

		switch (codec_fourcc) {
		case GF_4CC('A', 'V', '0', '1'):
			ctx->is_av1 = GF_TRUE;
			ctx->codecid = GF_CODECID_AV1;
			break;
		case GF_4CC('V', 'P', '9', '0'):
			ctx->is_vp9 = GF_TRUE;
			ctx->codecid = GF_CODECID_VP9;
			ctx->vp_cfg = gf_odf_vp_cfg_new();
			break;
		case GF_4CC('V', 'P', '8', '0'):
			ctx->codecid = GF_CODECID_VP8;
			ctx->vp_cfg = gf_odf_vp_cfg_new();
			break;
		case GF_4CC('V', 'P', '1', '0'):
			ctx->codecid = GF_CODECID_VP10;
			ctx->vp_cfg = gf_odf_vp_cfg_new();
			break;
		default:
			ctx->codecid = codec_fourcc;
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[IVF] Unsupported codec FourCC %s, import might be uncomplete or broken\n", gf_4cc_to_str(codec_fourcc) ));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		if (ctx->vp_cfg && !ctx->is_vp9) {
			ctx->is_vpX = GF_TRUE;
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[IVF] %s parsing not implemented, import might be uncomplete or broken\n", gf_4cc_to_str(codec_fourcc) ));
			ctx->vp_cfg->profile = 1;
			ctx->vp_cfg->level = 10;
			ctx->vp_cfg->bit_depth = 8;
			//leave the rest as 0
		}

		ctx->state.width = ctx->state.width < width ? width : ctx->state.width;
		ctx->state.height = ctx->state.height < height ? height : ctx->state.height;
		ctx->state.tb_num = frame_rate; //time_base.numerator
		ctx->state.tb_den = time_scale; //time_base.denominator

		if (!ctx->fps.num*ctx->fps.den && ctx->state.tb_num && ctx->state.tb_den && ! ( (ctx->state.tb_num<=1) && (ctx->state.tb_den<=1) ) ) {
			ctx->cur_fps.num = ctx->state.tb_num;
			ctx->cur_fps.den = ctx->state.tb_den;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_PARSER, ("[AV1Dmx] Detected IVF format FPS %d/%d\n", ctx->cur_fps.num, ctx->cur_fps.den));
			ctx->pts_from_file = GF_TRUE;
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_PARSER, ("[AV1Dmx] Detected IVF format\n"));
		}
		ctx->file_hdr_size = (u32) gf_bs_get_position(bs);
		if (last_obu_end) (*last_obu_end) = (u32) gf_bs_get_position(bs);
		return GF_OK;
	} else if (gf_media_aom_probe_annexb(bs)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[AV1Dmx] Detected Annex B format\n"));
		ctx->bsmode = AnnexB;
	} else {
		gf_bs_seek(bs, 0);
		e = aom_av1_parse_temporal_unit_from_section5(bs, &ctx->state);
		if (e && !gf_list_count(ctx->state.frame_state.frame_obus) ) {
			gf_filter_setup_failure(filter, e);
			ctx->bsmode = UNSUPPORTED;
			return e;
		}
		if (ctx->state.obu_type != OBU_TEMPORAL_DELIMITER) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[AV1Dmx] Error OBU stream start with %s, not a temporal delimiter - NOT SUPPORTED\n", av1_get_obu_name(ctx->state.obu_type) ));
			gf_filter_setup_failure(filter, e);
			ctx->bsmode = UNSUPPORTED;
			return e;
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[AV1Dmx] Detected OBUs Section 5 format\n"));
		ctx->bsmode = OBUs;

		av1_reset_state(&ctx->state, GF_FALSE);
		gf_bs_seek(bs, 0);
	}
	ctx->is_av1 = GF_TRUE;
	ctx->codecid = GF_CODECID_AV1;
	return GF_OK;
}


static void av1dmx_check_dur(GF_Filter *filter, GF_AV1DmxCtx *ctx)
{
	FILE *stream;
	GF_Err e;
	GF_BitStream *bs;
	u64 duration, cur_dur, last_cdur;
	AV1State av1state;
	const char *filepath=NULL;
	const GF_PropertyValue *p;
	if (!ctx->opid || ctx->timescale || ctx->file_loaded) return;

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILE_CACHED);
	if (p && p->value.boolean) ctx->file_loaded = GF_TRUE;

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILEPATH);
	if (!p || !p->value.string) {
		ctx->is_file = GF_FALSE;
		ctx->file_loaded = GF_FALSE;
		return;
	}
	filepath = p->value.string;
	ctx->is_file = GF_TRUE;

	if (ctx->index<0) {
		p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_DOWN_SIZE);
		if (!p || (p->value.longuint > 100000000)) {
			GF_LOG(GF_LOG_INFO, GF_LOG_PARSER, ("[VP9Dmx] Source file larger than 100M, skipping indexing\n"));
		} else {
			ctx->index = -ctx->index;
		}
	}
	if (ctx->index<=0)
		return;

	stream = gf_fopen(filepath, "rb");
	if (!stream) return;

	ctx->index_size = 0;

	bs = gf_bs_from_file(stream, GF_BITSTREAM_READ);

	if (ctx->file_hdr_size) {
		gf_bs_seek(bs, ctx->file_hdr_size);
	}
	memset(&av1state, 0, sizeof(AV1State));
	av1state.skip_frames = GF_TRUE;
	av1state.config = gf_odf_av1_cfg_new();

	duration = 0;
	cur_dur = last_cdur = 0;
	while (gf_bs_available(bs)) {
		Bool is_sap=GF_FALSE;
		u64 pts = GF_FILTER_NO_TS;
		u64 frame_start = gf_bs_get_position(bs);
		av1_reset_state(&av1state, GF_FALSE);

		/*we process each TU and extract only the necessary OBUs*/
		switch (ctx->bsmode) {
		case OBUs:
			e = aom_av1_parse_temporal_unit_from_section5(bs, &av1state);
			break;
		case AnnexB:
			e = aom_av1_parse_temporal_unit_from_annexb(bs, &av1state);
			break;
		case IVF:
			if (ctx->is_av1) {
				e = aom_av1_parse_temporal_unit_from_ivf(bs, &av1state);
			} else {
				u64 frame_size;
				e = gf_media_parse_ivf_frame_header(bs, &frame_size, &pts);
				if (!e) gf_bs_skip_bytes(bs, frame_size);
		 		is_sap = GF_TRUE;
			}
			break;
		default:
			e = GF_NOT_SUPPORTED;
		}
		if (e)
		 	break;

		if (pts != GF_FILTER_NO_TS) {
			duration = pts;
			cur_dur = pts - last_cdur;
		} else {
			duration += ctx->cur_fps.den;
			cur_dur += ctx->cur_fps.den;
		}
		if (av1state.frame_state.key_frame)
		 	is_sap = GF_TRUE;

		//only index at I-frame start
		if (frame_start && is_sap && (cur_dur > ctx->index * ctx->cur_fps.num) ) {
			if (!ctx->index_alloc_size) ctx->index_alloc_size = 10;
			else if (ctx->index_alloc_size == ctx->index_size) ctx->index_alloc_size *= 2;
			ctx->indexes = gf_realloc(ctx->indexes, sizeof(AV1Idx)*ctx->index_alloc_size);
			ctx->indexes[ctx->index_size].pos = frame_start;
			ctx->indexes[ctx->index_size].duration = (Double) duration;
			ctx->indexes[ctx->index_size].duration /= ctx->cur_fps.num;
			ctx->index_size ++;
			last_cdur = cur_dur;
			cur_dur = 0;
		}
	}
	gf_bs_del(bs);
	gf_fclose(stream);
	av1_reset_state(&av1state, GF_TRUE);
	gf_odf_av1_cfg_del(av1state.config);

	if (!ctx->duration.num || (ctx->duration.num  * ctx->cur_fps.num != duration * ctx->duration.den)) {
		ctx->duration.num = (s32) duration;
		ctx->duration.den = ctx->cur_fps.num;

		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC(ctx->duration));
	}

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

			if (ctx->index<0) {
				ctx->index = -ctx->index;
				ctx->file_loaded = GF_FALSE;
				ctx->duration.den = ctx->duration.num = 0;
				GF_LOG(GF_LOG_INFO, GF_LOG_PARSER, ("[AV1/VP9Demx] Play request from %d, building index\n", ctx->start_range));
				av1dmx_check_dur(filter, ctx);
			}

			for (i=1; i<ctx->index_size; i++) {
				if (ctx->indexes[i].duration>ctx->start_range) {
					ctx->cts = (u64) (ctx->indexes[i-1].duration * ctx->cur_fps.num);
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
	assert(ctx->cur_fps.num);
	assert(ctx->cur_fps.den);

	if (ctx->timescale) {
		u64 inc = ctx->cur_fps.den;
		inc *= ctx->timescale;
		inc /= ctx->cur_fps.num;
		ctx->cts += inc;
	} else {
		ctx->cts += ctx->cur_fps.den;
	}
}

static void av1dmx_check_pid(GF_Filter *filter, GF_AV1DmxCtx *ctx)
{
	u8 *dsi;
	u32 dsi_size, crc;

	//no config or no config change
	if (ctx->is_av1 && !gf_list_count(ctx->state.frame_state.header_obus)) return;

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		av1dmx_check_dur(filter, ctx);
	}
	dsi = NULL;
	dsi_size = 0;

	if (ctx->vp_cfg) {
		gf_odf_vp_cfg_write(ctx->vp_cfg, &dsi, &dsi_size, ctx->vp_cfg->codec_initdata_size ? GF_TRUE : GF_FALSE);
	} else if (ctx->is_av1) {
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

		if (!ctx->fps.num*ctx->fps.den && ctx->state.tb_num && ctx->state.tb_den && ! ( (ctx->state.tb_num<=1) && (ctx->state.tb_den<=1) ) ) {
			ctx->cur_fps.num = ctx->state.tb_num;
			ctx->cur_fps.den = ctx->state.tb_den;
		}

	}
	crc = gf_crc_32(dsi, dsi_size);

	if (crc == ctx->dsi_crc) {
		gf_free(dsi);
		return;
	}
	ctx->dsi_crc = crc;

	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, & PROP_UINT(GF_STREAM_VISUAL));

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, & PROP_UINT(ctx->codecid));

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, & PROP_UINT(ctx->cur_fps.num));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FPS, & PROP_FRAC(ctx->cur_fps));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, & PROP_UINT(ctx->state.width));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, & PROP_UINT(ctx->state.height));

	if (ctx->duration.num)
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC(ctx->duration));

	if (dsi && dsi_size)
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, & PROP_DATA_NO_COPY(dsi, dsi_size));

	if (ctx->is_file && ctx->index) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PLAYBACK_MODE, & PROP_UINT(GF_PLAYBACK_MODE_FASTFORWARD) );
	}

	if (ctx->is_av1) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_COLR_PRIMARIES, & PROP_UINT(ctx->state.color_primaries) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_COLR_TRANSFER, & PROP_UINT(ctx->state.transfer_characteristics) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_COLR_MX, & PROP_UINT(ctx->state.matrix_coefficients) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_COLR_RANGE, & PROP_BOOL(ctx->state.color_range) );
	}
}

GF_Err av1dmx_parse_ivf(GF_Filter *filter, GF_AV1DmxCtx *ctx)
{
	GF_Err e;
	u32 pck_size;
	u64 frame_size = 0, pts = GF_FILTER_NO_TS;
	GF_FilterPacket *pck;
	u64 pos, pos_ivf_hdr;
	u8 *output;

	pos_ivf_hdr = gf_bs_get_position(ctx->bs);
	e = gf_media_parse_ivf_frame_header(ctx->bs, &frame_size, &pts);
	if (e) return e;

	pos = gf_bs_get_position(ctx->bs);
	if (gf_bs_available(ctx->bs) < frame_size) {
		gf_bs_seek(ctx->bs, pos_ivf_hdr);
		return GF_EOS;
	}

	pck_size = (u32)(gf_bs_get_position(ctx->bs) - pos);
	assert(pck_size == frame_size);

	//check pid state
	av1dmx_check_pid(filter, ctx);

	if (!ctx->opid) {
		return GF_OK;
	}

	if (!ctx->is_playing) {
		gf_bs_seek(ctx->bs, pos_ivf_hdr);
		return GF_EOS;
	}

	pck = gf_filter_pck_new_alloc(ctx->opid, pck_size, &output);
	if (!pck) {
		gf_bs_seek(ctx->bs, pos_ivf_hdr);
		return GF_OUT_OF_MEM;
	}
	if (ctx->src_pck) gf_filter_pck_merge_properties(ctx->src_pck, pck);

	if (ctx->pts_from_file) {
		gf_filter_pck_set_cts(pck, pts);
	} else {
		gf_filter_pck_set_cts(pck, ctx->cts);
	}
	//no clue what is inside the packet, signal as SAP1
	gf_filter_pck_set_sap(pck, GF_FILTER_SAP_1);

	gf_bs_seek(ctx->bs, pos);
	gf_bs_read_data(ctx->bs, output, pck_size);
	gf_filter_pck_send(pck);

	av1dmx_update_cts(ctx);
	return GF_OK;
}

GF_Err av1dmx_parse_vp9(GF_Filter *filter, GF_AV1DmxCtx *ctx)
{
	Bool key_frame = GF_FALSE;
	u64 frame_size = 0, pts = 0;
	u64 pos, pos_ivf_hdr;
	u32 width = 0, height = 0, renderWidth, renderHeight;
	u32 num_frames_in_superframe = 0, superframe_index_size = 0, i = 0;
	u32 frame_sizes[VP9_MAX_FRAMES_IN_SUPERFRAME];
	u8 *output;
	GF_Err e;

	pos_ivf_hdr = gf_bs_get_position(ctx->bs);
	e = gf_media_parse_ivf_frame_header(ctx->bs, &frame_size, &pts);
	if (e) return e;

	pos = gf_bs_get_position(ctx->bs);
	if (gf_bs_available(ctx->bs) < frame_size) {
		gf_bs_seek(ctx->bs, pos_ivf_hdr);
		return GF_EOS;
	}

	/*check if it is a superframe*/
	if (gf_media_vp9_parse_superframe(ctx->bs, frame_size, &num_frames_in_superframe, frame_sizes, &superframe_index_size) != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[VP9Dmx] Error parsing superframe structure\n"));
		return e;
	}

	for (i = 0; i < num_frames_in_superframe; ++i) {
		u64 pos2 = gf_bs_get_position(ctx->bs);
		if (gf_media_vp9_parse_sample(ctx->bs, ctx->vp_cfg, &key_frame, &width, &height, &renderWidth, &renderHeight) != GF_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[VP9Dmx] Error parsing frame\n"));
			return e;
		}
		e = gf_bs_seek(ctx->bs, pos2 + frame_sizes[i]);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[VP9Dmx] Seek bad param (offset "LLU") (1)", pos2 + frame_sizes[i]));
			return e;
		}
	}
	if (gf_bs_get_position(ctx->bs) + superframe_index_size != pos + frame_size) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[VP9Dmx] Inconsistent IVF frame size of "LLU" bytes.\n", frame_size));
		GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("      Detected %d frames (+ %d bytes for the superframe index):\n", num_frames_in_superframe, superframe_index_size));
		for (i = 0; i < num_frames_in_superframe; ++i) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("         superframe %d, size is %u bytes\n", i, frame_sizes[i]));
		}
		GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("\n"));
	}
	e = gf_bs_seek(ctx->bs, pos + frame_size);
	if (e) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[VP9Dmx] Seek bad param (offset "LLU") (2)", pos + frame_size));
		return e;
	}

	u32 pck_size = (u32)(gf_bs_get_position(ctx->bs) - pos);
	assert(pck_size == frame_size);

	//check pid state
	av1dmx_check_pid(filter, ctx);

	if (!ctx->opid) {
		return GF_OK;
	}

	if (!ctx->is_playing) {
		gf_bs_seek(ctx->bs, pos_ivf_hdr);
		return GF_EOS;
	}

	GF_FilterPacket *pck = gf_filter_pck_new_alloc(ctx->opid, pck_size, &output);
	if (!pck) {
		gf_bs_seek(ctx->bs, pos_ivf_hdr);
		return GF_OUT_OF_MEM;
	}
	if (ctx->src_pck) gf_filter_pck_merge_properties(ctx->src_pck, pck);

	gf_filter_pck_set_cts(pck, ctx->cts);
	if (key_frame) {
		gf_filter_pck_set_sap(pck, GF_FILTER_SAP_1);
	}
	gf_bs_seek(ctx->bs, pos);
	gf_bs_read_data(ctx->bs, output, pck_size);
	gf_filter_pck_send(pck);

	av1dmx_update_cts(ctx);
	return GF_OK;
}

GF_Err av1dmx_parse_av1(GF_Filter *filter, GF_AV1DmxCtx *ctx)
{
	GF_Err e;
	u32 pck_size;
	GF_FilterPacket *pck;
	u8 *output;

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

	if (e) return e;

	//check pid state
	av1dmx_check_pid(filter, ctx);

	if (!ctx->opid) {
		if (ctx->state.obu_type != OBU_TEMPORAL_DELIMITER) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[AV1Dmx] output pid not configured (no sequence header yet ?), skiping OBU\n"));
		}
		av1_reset_state(&ctx->state, GF_FALSE);
		return GF_OK;
	}
	if (!ctx->is_playing) {
		av1_reset_state(&ctx->state, GF_FALSE);
		return GF_EOS;
	}

	gf_bs_get_content_no_truncate(ctx->state.bs, &ctx->state.frame_obus, &pck_size, &ctx->state.frame_obus_alloc);

	if (!pck_size) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[AV1Dmx] no frame OBU, skiping OBU\n"));
		return GF_OK;
	}

	pck = gf_filter_pck_new_alloc(ctx->opid, pck_size, &output);
	if (ctx->src_pck) gf_filter_pck_merge_properties(ctx->src_pck, pck);

	gf_filter_pck_set_cts(pck, ctx->cts);
	if (ctx->is_vp9) {
	} else {
		gf_filter_pck_set_sap(pck, ctx->state.frame_state.key_frame ? GF_FILTER_SAP_1 : 0);
	}
	memcpy(output, ctx->state.frame_obus, pck_size);

	if (ctx->deps) {
		u8 flags = 0;
		//dependsOn
		flags = ( ctx->state.frame_state.key_frame) ? 2 : 1;
		flags <<= 2;
		//dependedOn
	 	flags |= ctx->state.frame_state.refresh_frame_flags ? 1 : 2;
		flags <<= 2;
		//hasRedundant
	 	//flags |= ctx->has_redundant ? 1 : 2;
	 	gf_filter_pck_set_dependency_flags(pck, flags);
	}

	gf_filter_pck_send(pck);

	av1dmx_update_cts(ctx);
	av1_reset_state(&ctx->state, GF_FALSE);

	return GF_OK;
}

GF_Err av1dmx_process_buffer(GF_Filter *filter, GF_AV1DmxCtx *ctx, const char *data, u32 data_size, Bool is_copy)
{
	u32 last_obu_end = 0;
	GF_Err e = GF_OK;

	if (!ctx->bs) ctx->bs = gf_bs_new(data, data_size, GF_BITSTREAM_READ);
	else gf_bs_reassign_buffer(ctx->bs, data, data_size);

	//check ivf vs obu vs annexB
	e = av1dmx_check_format(filter, ctx, ctx->bs, &last_obu_end);
	if (e) return e;

	while (gf_bs_available(ctx->bs)) {

		if (ctx->is_vp9) {
			e = av1dmx_parse_vp9(filter, ctx);
		} else if (ctx->is_av1) {
			e = av1dmx_parse_av1(filter, ctx);
		} else {
			e = av1dmx_parse_ivf(filter, ctx);
		}

		if (e!=GF_EOS)
			last_obu_end = (u32) gf_bs_get_position(ctx->bs);

		if (e) {
			break;
		}
	}

	if (is_copy && last_obu_end) {
		assert(ctx->buf_size>=last_obu_end);
		memmove(ctx->buffer, ctx->buffer+last_obu_end, sizeof(char) * (ctx->buf_size-last_obu_end));
		ctx->buf_size -= last_obu_end;
	}
	if (e==GF_EOS) return GF_OK;
	if (e==GF_BUFFER_TOO_SMALL) return GF_OK;
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

	if (!ctx->is_playing && ctx->opid)
		return GF_OK;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			//flush
			while (ctx->buf_size) {
				u32 buf_size = ctx->buf_size;
				e = av1dmx_process_buffer(filter, ctx, ctx->buffer, ctx->buf_size, GF_TRUE);
				if (e) break;
				if (buf_size == ctx->buf_size) {
					break;
				}
			}
			ctx->buf_size = 0;
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

	av1_reset_state(&ctx->state, GF_TRUE);
	if (ctx->state.config) gf_odf_av1_cfg_del(ctx->state.config);
	if (ctx->state.bs) gf_bs_del(ctx->state.bs);
	if (ctx->state.frame_obus) gf_free(ctx->state.frame_obus);
	if (ctx->buffer) gf_free(ctx->buffer);

	if (ctx->vp_cfg) gf_odf_vp_cfg_del(ctx->vp_cfg);
}

static const char * av1dmx_probe_data(const u8 *data, u32 size, GF_FilterProbeScore *score)
{
	GF_BitStream *bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
	Bool res = GF_FALSE;
	u32 lt;
	const char *mime = "video/av1";
	lt = gf_log_get_tool_level(GF_LOG_CODING);
	gf_log_set_tool_level(GF_LOG_CODING, GF_LOG_QUIET);

	res = gf_media_probe_ivf(bs);
	if (res) {
		*score = GF_FPROBE_SUPPORTED;
		mime = "video/x-ivf";
	} else {
		res = gf_media_aom_probe_annexb(bs);
		if (res) *score = GF_FPROBE_SUPPORTED;
		else {
			AV1State state;
			GF_Err e;
			u32 nb_units = 0;

			memset(&state, 0, sizeof(AV1State));
			state.config = gf_odf_av1_cfg_new();
			while (gf_bs_available(bs)) {
				e = aom_av1_parse_temporal_unit_from_section5(bs, &state);
				if (e==GF_OK) {
					if (!nb_units || gf_list_count(state.frame_state.header_obus) || gf_list_count(state.frame_state.frame_obus))
						nb_units++;
					else
						break;
				} else {
					break;
				}
				av1_reset_state(&state, GF_FALSE);
				if (nb_units>2) break;
			}
			av1_reset_state(&state, GF_TRUE);
			gf_odf_av1_cfg_del(state.config);
			if (nb_units>2) {
				res = GF_TRUE;
				*score = GF_FPROBE_MAYBE_SUPPORTED;
			}
		}
	}

	gf_log_set_tool_level(GF_LOG_CODING, lt);

	gf_bs_del(bs);
	if (res) return mime;
	return NULL;
}

static const GF_FilterCapability AV1DmxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "video/x-ivf|video/av1"),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_AV1),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_VP9),
	CAP_BOOL(GF_CAPS_OUTPUT_STATIC_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "ivf|obu|av1b|av1"),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AV1),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_VP9),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
};

#define OFFS(_n)	#_n, offsetof(GF_AV1DmxCtx, _n)
static const GF_FilterArgs AV1DmxArgs[] =
{
	{ OFFS(fps), "import frame rate (0 default to FPS from from bitstream or 25 Hz)", GF_PROP_FRACTION, "0/1000", NULL, 0},
	{ OFFS(index), "indexing window length. If 0, bitstream is not probed for duration. A negative value skips the indexing if the source file is larger than 100M (slows down importers) unless a play with start range > 0 is issued, otherwise uses the positive value", GF_PROP_DOUBLE, "-1.0", NULL, 0},

	{ OFFS(importer), "compatibility with old importer", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(deps), "import samples dependencies information", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{0}
};


GF_FilterRegister AV1DmxRegister = {
	.name = "rfav1",
	GF_FS_SET_DESCRIPTION("AV1/IVF/VP9 reframer")
	GF_FS_SET_HELP("This filter parses AV1 OBU, AV1 AnnexB or IVF with AV1 or VP9 files/data and outputs corresponding visual PID and frames.")
	.private_size = sizeof(GF_AV1DmxCtx),
	.args = AV1DmxArgs,
	.finalize = av1dmx_finalize,
	SETCAPS(AV1DmxCaps),
	.configure_pid = av1dmx_configure_pid,
	.process = av1dmx_process,
	.probe_data = av1dmx_probe_data,
	.process_event = av1dmx_process_event
};


const GF_FilterRegister *av1dmx_register(GF_FilterSession *session)
{
	return &AV1DmxRegister;
}
