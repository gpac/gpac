/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2019-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / ProRes reframer filter
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

#ifndef GPAC_DISABLE_AV_PARSERS

typedef struct
{
	//filter args
	GF_Fraction fps;
	Bool findex, notime;
	char *cid;

	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;

	GF_BitStream *bs;
	u64 cts;
	GF_Fraction64 duration;
	u64 file_size, file_pos;
	Double start_range;
	Bool rewind;
	u32 cur_frame;
	u32 timescale;
	GF_Fraction cur_fps;

	u8 *buffer;
	u32 buf_size, alloc_size;

	GF_ProResFrameInfo cur_cfg;

	Bool is_playing;
	Bool is_file, file_loaded;
	Bool initial_play_done;

	//when source is not a file/pipe/net stream
	GF_FilterPacket *src_pck;

	/*frame index 0/NULL if findex is not set*/
	u32 nb_frames;
	u32 *frame_sizes;

	u32 bitrate;
	Bool copy_props;
} GF_ProResDmxCtx;


GF_Err proresdmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	GF_ProResDmxCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		ctx->ipid = NULL;
		if (ctx->opid) {
			gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	ctx->ipid = pid;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	if (p) ctx->timescale = p->value.uint;

	if (ctx->timescale && !ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, NULL);
	}

	//if source has no timescale, recompute time
	if (!ctx->timescale) {
		ctx->notime = GF_TRUE;
	} else {
		//if we have a FPS prop, use it
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_FPS);
		if (p) ctx->cur_fps = p->value.frac;
		ctx->copy_props = GF_TRUE;
	}
	return GF_OK;
}


static void proresdmx_check_dur(GF_Filter *filter, GF_ProResDmxCtx *ctx)
{
	FILE *stream;
	GF_BitStream *bs;
	u64 duration, rate;
	u32 idx_size;
	const char *filepath=NULL;
	const GF_PropertyValue *p;
	if (!ctx->opid || ctx->timescale || ctx->file_loaded) return;

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILE_CACHED);
	if (p && p->value.boolean) ctx->file_loaded = GF_TRUE;

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILEPATH);
	if (!p || !p->value.string || !strncmp(p->value.string, "gmem://", 7)) {
		ctx->is_file = GF_FALSE;
		ctx->file_loaded = GF_TRUE;
		return;
	}
	filepath = p->value.string;
	ctx->is_file = GF_TRUE;

	if (ctx->findex==1) {
		if (gf_opts_get_bool("temp", "force_indexing")) {
			ctx->findex = 2;
		} else {
			p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_DOWN_SIZE);
			if (!p || (p->value.longuint > 100000000)) {
				GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[ProResDmx] Source file larger than 100M, skipping indexing\n"));
			} else {
				ctx->findex = 2;
			}
		}
	}
	if (!ctx->findex)
		return;

	stream = gf_fopen_ex(filepath, NULL, "rb", GF_TRUE);
	if (!stream) {
		if (gf_fileio_is_main_thread(p->value.string))
			ctx->file_loaded = GF_TRUE;
		return;
	}

	bs = gf_bs_from_file(stream, GF_BITSTREAM_READ);

	idx_size = ctx->nb_frames;
	ctx->nb_frames = 0;
	ctx->file_size = gf_bs_available(bs);

	u64 frame_start = 0;
	duration = 0;
	while (gf_bs_available(bs)) {
		u32 fsize = gf_bs_read_u32(bs);
		u32 fmark = gf_bs_read_u32(bs);
		gf_bs_seek(bs, frame_start + fsize);
		if (fmark != GF_4CC('i','c','p','f'))
			break;

		duration += ctx->cur_fps.den;

		if (!idx_size) idx_size = 10;
		else if (idx_size == ctx->nb_frames) idx_size += 10;
		ctx->frame_sizes = gf_realloc(ctx->frame_sizes, sizeof(u32)*idx_size);
		ctx->frame_sizes[ctx->nb_frames] = fsize;
		ctx->nb_frames++;
		frame_start += fsize;
	}
	rate = gf_bs_get_position(bs);
	gf_bs_del(bs);
	gf_fclose(stream);

	if (!ctx->duration.num || (ctx->duration.num  * ctx->cur_fps.num != duration * ctx->duration.den)) {
		ctx->duration.num = (s32) duration;
		ctx->duration.den = ctx->cur_fps.num;

		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC64(ctx->duration));

		if (duration && (!gf_sys_is_test_mode() || gf_opts_get_bool("temp", "force_indexing"))) {
			rate *= 8 * ctx->duration.den;
			rate /= ctx->duration.num;
			ctx->bitrate = (u32) rate;
		}
	}
}


static Bool proresdmx_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	u32 i;
	u64 file_pos = 0;
	GF_FilterEvent fevt;
	GF_ProResDmxCtx *ctx = gf_filter_get_udta(filter);

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

		if (ctx->start_range) {
			ctx->cur_frame = 0;

			if (ctx->findex==1) {
				ctx->findex = 2;
				ctx->file_loaded = GF_FALSE;
				ctx->duration.den = ctx->duration.num = 0;
				GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[ProResDmx] Play request from %d, building index\n", ctx->start_range));
				proresdmx_check_dur(filter, ctx);
			}
			if ((evt->play.speed<0) && (ctx->start_range<0)) {
				ctx->start_range = (Double) ctx->duration.num;
				ctx->start_range /= ctx->duration.den;
			}

			for (i=0; i<ctx->nb_frames; i++) {
				Double t = ctx->cur_frame * ctx->cur_fps.den / ctx->cur_fps.num;
				ctx->cts = ctx->cur_frame * ctx->cur_fps.den;
				if (t>=ctx->start_range) {
					break;
				}
				if (i+1==ctx->nb_frames)
					break;
				ctx->cur_frame++;
				file_pos += ctx->frame_sizes[i];
			}
		}

		ctx->rewind = (ctx->nb_frames && (evt->play.speed<0)) ? GF_TRUE : GF_FALSE;

		if (!ctx->initial_play_done) {
			ctx->initial_play_done = GF_TRUE;
			//seek will not change the current source state, don't send a seek
			if (!file_pos)
				return GF_TRUE;
		}
		ctx->buf_size = 0;
		ctx->file_pos = file_pos;

		//post a seek
		GF_FEVT_INIT(fevt, GF_FEVT_SOURCE_SEEK, ctx->ipid);
		fevt.seek.start_offset = file_pos;
		gf_filter_pid_send_event(ctx->ipid, &fevt);

		//cancel event
		return GF_TRUE;

	case GF_FEVT_STOP:
		//don't cancel event
		ctx->is_playing = GF_FALSE;
		ctx->cts = 0;
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

static GFINLINE void proresdmx_update_cts(GF_ProResDmxCtx *ctx)
{
	u64 inc;
	assert(ctx->cur_fps.num);
	assert(ctx->cur_fps.den);

	if (ctx->timescale) {
		inc = ctx->cur_fps.den;
		inc *= ctx->timescale;
		inc /= ctx->cur_fps.num;
	} else {
		inc = ctx->cur_fps.den;
	}
	if (ctx->rewind)
		ctx->cts -= inc;
	else
		ctx->cts += inc;
}

static void proresdmx_check_pid(GF_Filter *filter, GF_ProResDmxCtx *ctx, GF_ProResFrameInfo *finfo)
{
	Bool same_cfg = GF_TRUE;
	u32 codec_id;
	u64 bitrate;
	GF_Fraction fps;

#define CHECK_CFG(_name) if (ctx->cur_cfg._name!=finfo->_name) same_cfg = GF_FALSE;

	CHECK_CFG(width)
	CHECK_CFG(height)
	CHECK_CFG(chroma_format)
	CHECK_CFG(interlaced_mode)
	CHECK_CFG(aspect_ratio_information)
	CHECK_CFG(framerate_code)
	CHECK_CFG(color_primaries)
	CHECK_CFG(transfer_characteristics)
	CHECK_CFG(matrix_coefficients)
	CHECK_CFG(alpha_channel_type)

#undef CHECK_CFG

	if (same_cfg && !ctx->copy_props) return;
	fps.num = fps.den = 0;
	switch (finfo->framerate_code) {
	case 1: fps.num = 24000; fps.den = 1001; break;
	case 2: fps.num = 24; fps.den = 1; break;
	case 3: fps.num = 24; fps.den = 1; break;
	case 4: fps.num = 30000; fps.den = 1001; break;
	case 5: fps.num = 30; fps.den = 1; break;
	case 6: fps.num = 50; fps.den = 1; break;
	case 7: fps.num = 60000; fps.den = 1001; break;
	case 8: fps.num = 60; fps.den = 1; break;
	case 9: fps.num = 100; fps.den = 1; break;
	case 10: fps.num = 120000; fps.den = 1001; break;
	case 11: fps.num = 120; fps.den = 1; break;
	}

	ctx->cur_fps = ctx->fps;
	if (!ctx->fps.num || !ctx->fps.den) {
		if (fps.num && fps.den) {
			ctx->cur_fps = fps;
		} else {
			ctx->cur_fps.num = 25000;
			ctx->cur_fps.den = 1000;
		}
	}

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		proresdmx_check_dur(filter, ctx);
	}

	bitrate = 0;
	if (ctx->nb_frames && ctx->file_size && ctx->cur_fps.den) {
		bitrate = ctx->file_size * 8;
		bitrate *= ctx->cur_fps.num;
		bitrate /= ctx->nb_frames * ctx->cur_fps.den;
	}
	memcpy(&ctx->cur_cfg, finfo, sizeof(GF_ProResFrameInfo));
	if (ctx->cid && (strlen(ctx->cid)>4)) {
		codec_id = GF_4CC(ctx->cid[0],ctx->cid[1],ctx->cid[2],ctx->cid[3]);
	} else if (finfo->chroma_format==3) {
		codec_id = GF_CODECID_AP4H;
	} else {
		codec_id = GF_CODECID_APCH;
	}

	ctx->copy_props = GF_FALSE;
	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, & PROP_UINT(GF_STREAM_VISUAL));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, & PROP_UINT(codec_id));

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, & PROP_UINT(ctx->cur_fps.num));

	//if we have a FPS prop, use it
	if (!gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FPS))
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FPS, & PROP_FRAC(ctx->cur_fps));

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, & PROP_UINT(finfo->width));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, & PROP_UINT(finfo->height));
	switch (finfo->alpha_channel_type) {
	case 1:
	case 2:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_ALPHA, & PROP_BOOL(GF_TRUE));
		break;
	default:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_ALPHA, NULL);
		break;
	}

	switch (finfo->aspect_ratio_information) {
	case 0:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAR, NULL);
		break;
	case 1:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAR, & PROP_FRAC_INT(1, 1) );
		break;
	case 2:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAR, & PROP_FRAC_INT(4, 3) );
		break;
	case 3:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAR, & PROP_FRAC_INT(16, 9) );
		break;
	default:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAR, NULL);
		break;
	}

	if (ctx->duration.num)
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC64(ctx->duration));
	if (!ctx->timescale)
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CAN_DATAREF, & PROP_BOOL(GF_TRUE ) );

	if (ctx->bitrate) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_BITRATE, & PROP_UINT(ctx->bitrate));
	}

	/*prores is all intra, we support rewind*/
	if (ctx->is_file && ctx->findex) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PLAYBACK_MODE, & PROP_UINT(GF_PLAYBACK_MODE_REWIND) );
	}
	if (bitrate && !gf_sys_is_test_mode())
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_BITRATE, & PROP_UINT((u32)bitrate) );

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_COLR_PRIMARIES, & PROP_UINT(finfo->color_primaries) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_COLR_TRANSFER, & PROP_UINT(finfo->transfer_characteristics) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_COLR_MX, & PROP_UINT(finfo->matrix_coefficients) );

	//set interlaced or remove interlaced property
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_INTERLACED, ctx->cur_cfg.interlaced_mode ? & PROP_UINT(GF_TRUE) : NULL);
}



GF_Err proresdmx_process_buffer(GF_Filter *filter, GF_ProResDmxCtx *ctx, const u8 *data, u32 data_size, Bool is_copy)
{
	u32 last_frame_end = 0;
	GF_Err e = GF_OK;

	if (!ctx->bs) ctx->bs = gf_bs_new(data, data_size, GF_BITSTREAM_READ);
	else gf_bs_reassign_buffer(ctx->bs, data, data_size);

	while (gf_bs_available(ctx->bs)) {
		u8 *output;
		GF_FilterPacket *pck;
		GF_ProResFrameInfo finfo;
		e = gf_media_prores_parse_bs(ctx->bs, &finfo);

		if (e) {
			break;
		}
		proresdmx_check_pid(filter, ctx, &finfo);

		if (!ctx->is_playing && ctx->opid)
			break;

		if (gf_bs_available(ctx->bs)<finfo.frame_size)
			break;

		pck = gf_filter_pck_new_alloc(ctx->opid, finfo.frame_size, &output);
		if (!pck) break;
		gf_bs_read_data(ctx->bs, output, finfo.frame_size);

		if (ctx->src_pck) gf_filter_pck_merge_properties(ctx->src_pck, pck);

		gf_filter_pck_set_dts(pck, ctx->cts);
		gf_filter_pck_set_cts(pck, ctx->cts);
		gf_filter_pck_set_sap(pck, GF_FILTER_SAP_1);
		gf_filter_pck_set_byte_offset(pck, ctx->file_pos);

		gf_filter_pck_send(pck);
		proresdmx_update_cts(ctx);
		last_frame_end = (u32) gf_bs_get_position(ctx->bs);

		if (ctx->rewind) {
			ctx->buf_size = 0;
			last_frame_end = 0;
			assert(ctx->cur_frame);
			ctx->cur_frame--;
			if (!ctx->cur_frame) {
				if (ctx->opid)
					gf_filter_pid_set_eos(ctx->opid);
			} else {
				GF_FilterEvent fevt;
				ctx->file_pos -= ctx->frame_sizes[ctx->cur_frame];
				//post a seek
				GF_FEVT_INIT(fevt, GF_FEVT_SOURCE_SEEK, ctx->ipid);
				fevt.seek.start_offset = ctx->file_pos;
				gf_filter_pid_send_event(ctx->ipid, &fevt);
			}
			break;
		} else {
			ctx->file_pos += finfo.frame_size;
		}
	}

	if (is_copy && last_frame_end) {
		assert(ctx->buf_size >= last_frame_end);
		memmove(ctx->buffer, ctx->buffer+last_frame_end, sizeof(char) * (ctx->buf_size-last_frame_end));
		ctx->buf_size -= last_frame_end;
	}
	if (e==GF_EOS) return GF_OK;
	if (e==GF_BUFFER_TOO_SMALL) return GF_OK;
	return e;
}

GF_Err proresdmx_process(GF_Filter *filter)
{
	GF_Err e;
	GF_ProResDmxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck;
	char *data;
	u32 pck_size;

	if (!ctx->is_playing && ctx->opid)
		return GF_OK;

	if (ctx->rewind && !ctx->cur_frame) {
		gf_filter_pid_set_discard(ctx->ipid, GF_TRUE);
		return GF_OK;
	}

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			//flush
			while (ctx->buf_size) {
				u32 buf_size = ctx->buf_size;
				e = proresdmx_process_buffer(filter, ctx, ctx->buffer, ctx->buf_size, GF_TRUE);
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
				e = proresdmx_process_buffer(filter, ctx, ctx->buffer, ctx->buf_size, GF_TRUE);
			}
			ctx->buf_size=0;
			gf_filter_pid_drop_packet(ctx->ipid);
			return e;
		}
		//flush of pending frame (might have lost something)
		if (ctx->buf_size) {
			e = proresdmx_process_buffer(filter, ctx, ctx->buffer, ctx->buf_size, GF_TRUE);
			ctx->buf_size = 0;
			if (e) return e;
		}

		//beginning of a new frame
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
		e = proresdmx_process_buffer(filter, ctx, data, pck_size, GF_FALSE);

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
	e = proresdmx_process_buffer(filter, ctx, ctx->buffer, ctx->buf_size, GF_TRUE);
	gf_filter_pid_drop_packet(ctx->ipid);
	return e;
}

static void proresdmx_finalize(GF_Filter *filter)
{
	GF_ProResDmxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->bs) gf_bs_del(ctx->bs);
	if (ctx->frame_sizes) gf_free(ctx->frame_sizes);
	if (ctx->buffer) gf_free(ctx->buffer);
}

static const char * proresdmx_probe_data(const u8 *data, u32 size, GF_FilterProbeScore *score)
{
	if (size<8) return NULL;

	if ((data[4] == 'i') && (data[5] == 'c') && (data[6] == 'p') && (data[7] == 'f')) {
		*score = GF_FPROBE_SUPPORTED;
		return "video/prores";
	}
	return NULL;
}

static const GF_FilterCapability ProResDmxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "prores"),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "video/prores"),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_APCN),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_APCO),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_APCH),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_APCS),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_AP4X),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_AP4H),
	CAP_BOOL(GF_CAPS_OUTPUT_STATIC_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_APCN),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_APCO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_APCH),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_APCS),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AP4X),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AP4H),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
};

#define OFFS(_n)	#_n, offsetof(GF_ProResDmxCtx, _n)
static const GF_FilterArgs ProResDmxArgs[] =
{
	{ OFFS(fps), "import frame rate (0 default to FPS from bitstream or 25 Hz)", GF_PROP_FRACTION, "0/1000", NULL, 0},
	{ OFFS(findex), "index frames. If true, filter will be able to work in rewind mode", GF_PROP_BOOL, "true", NULL, 0},
	{ OFFS(cid), "set QT 4CC for the imported media. If not set, default is 'ap4h' for YUV444 and 'apch' for YUV422", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(notime), "ignore input timestamps, rebuild from 0", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{0}
};


GF_FilterRegister ProResDmxRegister = {
	.name = "rfprores",
	GF_FS_SET_DESCRIPTION("ProRes reframer")
	GF_FS_SET_HELP("This filter parses ProRes raw files/data and outputs corresponding visual PID and frames.")
	.private_size = sizeof(GF_ProResDmxCtx),
	.args = ProResDmxArgs,
	.finalize = proresdmx_finalize,
	SETCAPS(ProResDmxCaps),
	.configure_pid = proresdmx_configure_pid,
	.process = proresdmx_process,
	.probe_data = proresdmx_probe_data,
	.process_event = proresdmx_process_event
};


const GF_FilterRegister *rfprores_register(GF_FilterSession *session)
{
	return &ProResDmxRegister;
}

#else
const GF_FilterRegister *rfprores_register(GF_FilterSession *session)
{
	return NULL;
}
#endif // GPAC_DISABLE_AV_PARSERS

