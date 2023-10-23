/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / MPEG-1/2/4(Part2) video reframer filter
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

#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_RFMPGVID)

#define MIN_HDR_STORE	12
typedef struct
{
	u64 pos;
	Double start_time;
} MPGVidIdx;

typedef struct
{
	//filter args
	GF_Fraction fps;
	Double index;
	Bool vfr, importer, notime;

	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;

	GF_BitStream *bs;
	u64 cts, dts, prev_dts;
	u32 width, height;
	GF_Fraction64 duration;
	Double start_range;
	Bool in_seek;
	u32 timescale;

	u32 resume_from;
	GF_Fraction cur_fps;

	Bool is_mpg12, forced_packed;
	GF_M4VParser *vparser;
	GF_M4VDecSpecInfo dsi;
	u32 b_frames;
	Bool is_packed, is_vfr;
	GF_List *pck_queue;
	u64 last_ref_cts;
	Bool frame_started;

	u32 nb_i, nb_p, nb_b, nb_frames, max_b;

	u32 bytes_in_header;
	char *hdr_store;
	u32 hdr_store_size, hdr_store_alloc;

	Bool is_playing;
	Bool is_file, file_loaded;
	Bool initial_play_done;

	Bool input_is_au_start, input_is_au_end;
	Bool recompute_cts, copy_props;

	GF_FilterPacket *src_pck;

	MPGVidIdx *indexes;
	u32 index_alloc_size, index_size;
	u32 bitrate;
	Bool trash_trailer;
} GF_MPGVidDmxCtx;


GF_Err mpgviddmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	Bool was_mpeg12;
	const GF_PropertyValue *p;
	GF_MPGVidDmxCtx *ctx = gf_filter_get_udta(filter);

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
	ctx->cur_fps = ctx->fps;
	if (!ctx->fps.num || !ctx->fps.den) {
		ctx->cur_fps.num = 25000;
		ctx->cur_fps.den = 1000;
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	if (p) {
		ctx->timescale = ctx->cur_fps.num = p->value.uint;
		ctx->cur_fps.den = 0;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_FPS);
		if (p) {
			ctx->cur_fps = p->value.frac;
		}
		p = gf_filter_pid_get_property_str(pid, "nocts");
		if (p && p->value.boolean) ctx->recompute_cts = GF_TRUE;
	}

	was_mpeg12 = ctx->is_mpg12;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (p) {
		switch (p->value.uint) {
		case GF_CODECID_MPEG1:
		case GF_CODECID_MPEG2_422:
		case GF_CODECID_MPEG2_SNR:
		case GF_CODECID_MPEG2_HIGH:
		case GF_CODECID_MPEG2_MAIN:
		case GF_CODECID_MPEG2_SIMPLE:
		case GF_CODECID_MPEG2_SPATIAL:
			ctx->is_mpg12 = GF_TRUE;
			break;
		}
	}
	else {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_MIME);
		if (p && p->value.string && (strstr(p->value.string, "m1v") || strstr(p->value.string, "m2v")) ) ctx->is_mpg12 = GF_TRUE;
		else {
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_FILE_EXT);
			if (p && p->value.string && (strstr(p->value.string, "m1v") || strstr(p->value.string, "m2v")) ) ctx->is_mpg12 = GF_TRUE;
		}
	}

	if (ctx->vparser && (was_mpeg12 != ctx->is_mpg12)) {
		gf_m4v_parser_del_no_bs(ctx->vparser);
		ctx->vparser = NULL;
	}

	if (ctx->timescale && !ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, NULL);
	}
	//if source has no timescale, recompute time
	if (!ctx->timescale) ctx->notime = GF_TRUE;
	else ctx->copy_props = GF_TRUE;

	return GF_OK;
}


static void mpgviddmx_check_dur(GF_Filter *filter, GF_MPGVidDmxCtx *ctx)
{

	FILE *stream;
	GF_BitStream *bs;
	GF_M4VParser *vparser;
	GF_M4VDecSpecInfo dsi;
	GF_Err e;
	u32 probe_size=0;
	u64 duration, cur_dur, rate;
	const GF_PropertyValue *p;
	if (!ctx->opid || ctx->timescale || ctx->file_loaded) return;

	if (ctx->index<0) {
		if (gf_opts_get_bool("temp", "force_indexing")) {
			ctx->index = 1.0;
		} else {
			p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_DOWN_SIZE);
			if (!p || (p->value.longuint > 20000000)) {
				GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[MPGVids] Source file larger than 20M, skipping indexing\n"));
				if (!gf_sys_is_test_mode())
					probe_size = 20000000;
			} else {
				ctx->index = -ctx->index;
			}
		}
	}
	if ((ctx->index<=0) && !probe_size) {
		ctx->duration.num = 1;
		ctx->file_loaded = GF_TRUE;
		return;
	}

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILEPATH);
	if (!p || !p->value.string || !strncmp(p->value.string, "gmem://", 7)) {
		ctx->is_file = GF_FALSE;
		ctx->file_loaded = GF_TRUE;
		return;
	}
	ctx->is_file = GF_TRUE;

	stream = gf_fopen_ex(p->value.string, NULL, "rb", GF_TRUE);
	if (!stream) {
		if (gf_fileio_is_main_thread(p->value.string))
			ctx->file_loaded = GF_TRUE;
		return;
	}

	ctx->index_size = 0;

	bs = gf_bs_from_file(stream, GF_BITSTREAM_READ);

	vparser = gf_m4v_parser_bs_new(bs, ctx->is_mpg12);
	e = gf_m4v_parse_config(vparser, &dsi);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[MPGVid] Could not parse video header - duration  not estimated\n"));
		ctx->file_loaded = GF_TRUE;
		return;
	}

	duration = 0;
	cur_dur = 0;
	while (gf_bs_available(bs)) {
		u8 ftype;
		u32 tinc;
		u64 fsize, start;
		Bool is_coded;
		u64 pos;
		pos = gf_m4v_get_object_start(vparser);
		e = gf_m4v_parse_frame(vparser, &dsi, &ftype, &tinc, &fsize, &start, &is_coded);
		if (e<0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[MPGVid] Could not parse video frame\n"));
			continue;
		}
		if (probe_size && (pos>probe_size) && (ftype==1)) {
			break;
		}

		duration += ctx->cur_fps.den;
		cur_dur += ctx->cur_fps.den;
		//only index at I-frame start
		if (!probe_size && pos && (ftype==1) && (cur_dur >= ctx->index * ctx->cur_fps.num) ) {
			if (!ctx->index_alloc_size) ctx->index_alloc_size = 10;
			else if (ctx->index_alloc_size == ctx->index_size) ctx->index_alloc_size *= 2;
			ctx->indexes = gf_realloc(ctx->indexes, sizeof(MPGVidIdx)*ctx->index_alloc_size);
			ctx->indexes[ctx->index_size].pos = pos;
			ctx->indexes[ctx->index_size].start_time = (Double) (duration-ctx->cur_fps.den);
			ctx->indexes[ctx->index_size].start_time /= ctx->cur_fps.num;
			ctx->index_size ++;
			cur_dur = 0;
		}
	}

	rate = gf_bs_get_position(bs);
	gf_m4v_parser_del(vparser);
	gf_fclose(stream);

	if (!ctx->duration.num || (ctx->duration.num  * ctx->cur_fps.num != duration * ctx->duration.den)) {
		ctx->duration.num = (s32) duration;
		ctx->duration.den = ctx->cur_fps.num;

		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC64(ctx->duration));

		if (duration && !gf_sys_is_test_mode() ) {
			rate *= 8 * ctx->duration.den;
			rate /= ctx->duration.num;
			ctx->bitrate = (u32) rate;
		}
	}

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILE_CACHED);
	if (p && p->value.boolean) ctx->file_loaded = GF_TRUE;
}


static void mpgviddmx_enqueue_or_dispatch(GF_MPGVidDmxCtx *ctx, GF_FilterPacket *pck, Bool flush_ref, Bool is_eos)
{
	if (pck && ctx->trash_trailer) {
		gf_filter_pck_discard(pck);
		return;
	}

	if (!is_eos && (!ctx->width || !ctx->height))
		flush_ref = GF_FALSE;

	//need to signal this for consumers using DTS (eg MPEG-2 TS)
	if (flush_ref && ctx->pck_queue) {
		//send all reference packet queued
		u32 i, count = gf_list_count(ctx->pck_queue);

		for (i=0; i<count; i++) {
			u64 cts;
			GF_FilterPacket *q_pck = gf_list_get(ctx->pck_queue, i);
			u8 carousel = gf_filter_pck_get_carousel_version(q_pck);
			if (!carousel) {
				gf_filter_pck_send(q_pck);
				continue;
			}
			gf_filter_pck_set_carousel_version(q_pck, 0);
			cts = gf_filter_pck_get_cts(q_pck);
			if (cts != GF_FILTER_NO_TS) {
				//offset the cts of the ref frame to the number of B frames in-between
				if (ctx->last_ref_cts == cts) {
					cts += ctx->b_frames * ctx->cur_fps.den;
					gf_filter_pck_set_cts(q_pck, cts);
				} else {
					//shift all other frames (i.e. pending Bs) by 1 frame in the past since we move the ref frame after them
					assert(cts >= ctx->cur_fps.den);
					cts -= ctx->cur_fps.den;
					gf_filter_pck_set_cts(q_pck, cts);
				}
			}
			if (is_eos && (i+1==count)) {
				Bool start, end;
				gf_filter_pck_get_framing(q_pck, &start, &end);
				gf_filter_pck_set_framing(q_pck, start, GF_TRUE);
			}
			gf_filter_pck_send(q_pck);
		}
		gf_list_reset(ctx->pck_queue);
	}
	if (!pck) return;

	if (!ctx->pck_queue) ctx->pck_queue = gf_list_new();
	gf_list_add(ctx->pck_queue, pck);
}

static void mpgviddmx_check_pid(GF_Filter *filter, GF_MPGVidDmxCtx *ctx, u32 vosh_size, u8 *data)
{
	Bool flush_after = GF_FALSE;
	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		mpgviddmx_check_dur(filter, ctx);
	}

	if ((ctx->width == ctx->dsi.width) && (ctx->height == ctx->dsi.height) && !ctx->copy_props) return;

	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
	ctx->copy_props = GF_FALSE;
	if (ctx->duration.num)
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC64(ctx->duration));
	if (!ctx->timescale)
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CAN_DATAREF, & PROP_BOOL(GF_TRUE ) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, & PROP_UINT(GF_STREAM_VISUAL));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, & PROP_UINT(ctx->timescale ? ctx->timescale : ctx->cur_fps.num));

	//if we have a FPS prop, use it
	if (!gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FPS))
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FPS, & PROP_FRAC(ctx->cur_fps));

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, NULL);

	if (ctx->width && ctx->height) {
		mpgviddmx_enqueue_or_dispatch(ctx, NULL, GF_TRUE, GF_FALSE);
	} else {
		flush_after = GF_TRUE;
	}
	ctx->width = ctx->dsi.width;
	ctx->height = ctx->dsi.height;

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, & PROP_UINT( ctx->dsi.width));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, & PROP_UINT( ctx->dsi.height));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAR, & PROP_FRAC_INT(ctx->dsi.par_num, ctx->dsi.par_den));

	if (ctx->is_mpg12) {
		const GF_PropertyValue *cid = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_CODECID);
		u32 PL = 0;
		if (cid) {
			switch (cid->value.uint) {
			case GF_CODECID_MPEG2_MAIN:
			case GF_CODECID_MPEG2_422:
			case GF_CODECID_MPEG2_SNR:
			case GF_CODECID_MPEG2_HIGH:
				//keep same signaling
				PL = cid->value.uint;
				break;
			default:
				break;
			}
		} else {
			u32 prof = (ctx->dsi.VideoPL>>4) & 0x7;
			if (prof==1) PL = GF_CODECID_MPEG2_HIGH;
			else if (prof==2) PL = GF_CODECID_MPEG2_SNR;
			else if (prof==3) PL = GF_CODECID_MPEG2_SNR;
			else if (prof==4) PL = GF_CODECID_MPEG2_MAIN;
			else if (prof==5) PL = GF_CODECID_MPEG2_SIMPLE;
		}

		if (!PL)
			PL = (ctx->dsi.VideoPL == GF_CODECID_MPEG1) ? GF_CODECID_MPEG1 : GF_CODECID_MPEG2_MAIN;
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, & PROP_UINT(PL));
	} else {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, & PROP_UINT(GF_CODECID_MPEG4_PART2));
	}
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PROFILE_LEVEL, & PROP_UINT (ctx->dsi.VideoPL) );

	if (ctx->bitrate) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_BITRATE, & PROP_UINT(ctx->bitrate));
	}

	ctx->b_frames = 0;

	if (vosh_size) {
		u32 i;
		char * dcfg = gf_malloc(sizeof(char)*vosh_size);
		memcpy(dcfg, data, sizeof(char)*vosh_size);

		/*remove packed flag if any (VOSH user data)*/
		ctx->is_packed = ctx->is_vfr = ctx->forced_packed = GF_FALSE;
		i=0;
		while (1) {
			char *frame = dcfg;
			while ((i+3<vosh_size)  && ((frame[i]!=0) || (frame[i+1]!=0) || (frame[i+2]!=1))) i++;
			if (i+4>=vosh_size) break;
			if (strncmp(frame+i+4, "DivX", 4)) {
				i += 4;
				continue;
			}
			frame = memchr(dcfg + i + 4, 'p', vosh_size - i - 4);
			if (frame) {
				ctx->forced_packed = GF_TRUE;
				frame[0] = 'n';
			}
			break;
		}
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, & PROP_DATA_NO_COPY(dcfg, vosh_size));
	}

	if (ctx->is_file && ctx->index) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PLAYBACK_MODE, & PROP_UINT(GF_PLAYBACK_MODE_FASTFORWARD) );
	}

	if (!gf_sys_is_test_mode()) {
		if (ctx->dsi.chroma_fmt)
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_COLR_CHROMAFMT, & PROP_UINT(ctx->dsi.chroma_fmt) );

		if (ctx->is_mpg12)
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_INTERLACED, !ctx->dsi.progresive ? & PROP_BOOL(GF_TRUE) : NULL );
	}

	if (flush_after)
		mpgviddmx_enqueue_or_dispatch(ctx, NULL, GF_TRUE, GF_FALSE);

}

static Bool mpgviddmx_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	u32 i;
	u64 file_pos = 0;
	GF_FilterEvent fevt;
	GF_MPGVidDmxCtx *ctx = gf_filter_get_udta(filter);

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if (!ctx->is_playing) {
			ctx->is_playing = GF_TRUE;
			ctx->cts = 0;
			ctx->bytes_in_header = 0;
		}
		if (! ctx->is_file) {
			if (!ctx->initial_play_done) {
				ctx->initial_play_done = GF_TRUE;
				if (evt->play.start_range>0.1) ctx->resume_from = 0;
 			}
			return GF_FALSE;
		}
		ctx->start_range = evt->play.start_range;
		ctx->in_seek = GF_TRUE;

		if (ctx->start_range) {
			if (ctx->index<0) {
				ctx->index = -ctx->index;
				ctx->file_loaded = GF_FALSE;
				ctx->duration.den = ctx->duration.num = 0;
				mpgviddmx_check_dur(filter, ctx);
			}

			for (i=1; i<ctx->index_size; i++) {
				if ((ctx->indexes[i].start_time > ctx->start_range) || (i+1==ctx->index_size)) {
					ctx->cts = (u64) (ctx->indexes[i-1].start_time * ctx->cur_fps.num);
					file_pos = ctx->indexes[i-1].pos;
					break;
				}
			}
		}
		ctx->dts = ctx->cts;
		if (!ctx->initial_play_done) {
			ctx->initial_play_done = GF_TRUE;
			//seek will not change the current source state, don't send a seek
			if (!file_pos)
				return GF_TRUE;
		}
		ctx->resume_from = 0;
		ctx->bytes_in_header = 0;
		//post a seek
		GF_FEVT_INIT(fevt, GF_FEVT_SOURCE_SEEK, ctx->ipid);
		fevt.seek.start_offset = file_pos;
		gf_filter_pid_send_event(ctx->ipid, &fevt);

		//cancel event
		return GF_TRUE;

	case GF_FEVT_STOP:
		ctx->is_playing = GF_FALSE;
		if (ctx->src_pck) gf_filter_pck_unref(ctx->src_pck);
		ctx->src_pck = NULL;
		if (ctx->pck_queue) {
			while (gf_list_count(ctx->pck_queue)) {
				GF_FilterPacket *pck=gf_list_pop_front(ctx->pck_queue);
				gf_filter_pck_discard(pck);
			}
		}
		ctx->cts = 0;
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

static GFINLINE void mpgviddmx_update_time(GF_MPGVidDmxCtx *ctx)
{
	assert(ctx->cur_fps.num);

	if (!ctx->notime) {
		u64 inc = 3000;
		if (ctx->cur_fps.den && ctx->cur_fps.num) {
			inc = ctx->cur_fps.den;
			if (ctx->cur_fps.num != ctx->timescale) {
				inc *= ctx->timescale;
				inc /= ctx->cur_fps.num;
			}
		}
		ctx->cts += inc;
		ctx->dts += inc;
	} else {
		assert(ctx->cur_fps.den);
		ctx->cts += ctx->cur_fps.den;
		ctx->dts += ctx->cur_fps.den;
	}
}


static s32 mpgviddmx_next_start_code(u8 *data, u32 size)
{
	u32 v, bpos, found;
	s64 start, end;

	bpos = 0;
	found = 0;
	start = 0;
	end = 0;

	v = 0xffffffff;
	while (!end) {
		if (bpos == size)
			return -1;
		v = ( (v<<8) & 0xFFFFFF00) | data[bpos];

		bpos++;
		if ((v & 0xFFFFFF00) == 0x00000100) {
			end = start + bpos - 4;
			found = 1;
			break;
		}
	}
	if (!found)
		return -1;
	assert(end >= start);
	return (s32) (end - start);
}

GF_Err mpgviddmx_process(GF_Filter *filter)
{
	GF_MPGVidDmxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck;
	u64 byte_offset;
	s64 vosh_start = -1;
	s64 vosh_end = -1;
	GF_Err e;
	char *data;
	u8 *start;
	u32 pck_size;
	s32 remain;

	//always reparse duration
	if (!ctx->duration.num)
		mpgviddmx_check_dur(filter, ctx);

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			mpgviddmx_enqueue_or_dispatch(ctx, NULL, GF_TRUE, GF_TRUE);
			if (ctx->opid)
				gf_filter_pid_set_eos(ctx->opid);
			if (ctx->src_pck) gf_filter_pck_unref(ctx->src_pck);
			ctx->src_pck = NULL;
			return GF_EOS;
		}
		return GF_OK;
	}

	data = (char *) gf_filter_pck_get_data(pck, &pck_size);
	byte_offset = gf_filter_pck_get_byte_offset(pck);

	start = data;
	remain = pck_size;

	//input pid was muxed - we flushed pending data , update cts
	if (!ctx->resume_from && ctx->timescale) {
		if (!ctx->notime) {
			u64 ts = gf_filter_pck_get_cts(pck);
			if (ts != GF_FILTER_NO_TS) {
				if (!ctx->cts || !ctx->recompute_cts)
					ctx->cts = ts;
			}
			ts = gf_filter_pck_get_dts(pck);
			if (ts != GF_FILTER_NO_TS) {
				if (!ctx->dts || !ctx->recompute_cts) {
					ctx->dts = ts;
				}

				if (!ctx->prev_dts) ctx->prev_dts = ts;
				else if (ctx->prev_dts != ts) {
					u64 diff = ts;
					diff -= ctx->prev_dts;
					if (!ctx->cur_fps.den)
						ctx->cur_fps.den = (u32) diff;
					else if (ctx->cur_fps.den > diff)
						ctx->cur_fps.den = (u32) diff;

					ctx->prev_dts = ts;
				}
			}
		}

		gf_filter_pck_get_framing(pck, &ctx->input_is_au_start, &ctx->input_is_au_end);
		//we force it to true to deal with broken avi packaging where a video AU is split across several AVI video frames
		ctx->input_is_au_end = GF_FALSE;
		//this will force CTS recomput of each frame
		if (ctx->recompute_cts) ctx->input_is_au_start = GF_FALSE;
		if (ctx->src_pck) gf_filter_pck_unref(ctx->src_pck);
		ctx->src_pck = pck;
		gf_filter_pck_ref_props(&ctx->src_pck);
	}

	//we stored some data to find the complete vosh, aggregate this packet with current one
	if (!ctx->resume_from && ctx->hdr_store_size) {
		if (ctx->hdr_store_alloc < ctx->hdr_store_size + pck_size) {
			ctx->hdr_store_alloc = ctx->hdr_store_size + pck_size;
			ctx->hdr_store = gf_realloc(ctx->hdr_store, sizeof(char)*ctx->hdr_store_alloc);
		}
		memcpy(ctx->hdr_store + ctx->hdr_store_size, data, sizeof(char)*pck_size);
		if (byte_offset != GF_FILTER_NO_BO) {
			if (byte_offset >= ctx->hdr_store_size)
				byte_offset -= ctx->hdr_store_size;
			else
				byte_offset = GF_FILTER_NO_BO;
		}
		ctx->hdr_store_size += pck_size;
		start = data = ctx->hdr_store;
		remain = pck_size = ctx->hdr_store_size;
	}

	if (ctx->resume_from) {
		if (gf_filter_pid_would_block(ctx->opid))
			return GF_OK;

		//resume from data copied internally
		if (ctx->hdr_store_size) {
			assert(ctx->resume_from <= ctx->hdr_store_size);
			start = data = ctx->hdr_store + ctx->resume_from;
			remain = pck_size = ctx->hdr_store_size - ctx->resume_from;
		} else {
			assert(remain >= (s32) ctx->resume_from);
			start += ctx->resume_from;
			remain -= ctx->resume_from;
		}
		ctx->resume_from = 0;
	}

	if (!ctx->bs) {
		ctx->bs = gf_bs_new(start, remain, GF_BITSTREAM_READ);
	} else {
		gf_bs_reassign_buffer(ctx->bs, start, remain);
	}
	if (!ctx->vparser) {
		ctx->vparser = gf_m4v_parser_bs_new(ctx->bs, ctx->is_mpg12);
	}


	while (remain) {
		Bool full_frame;
		u8 *pck_data;
		s32 current;
		u8 sc_type, forced_sc_type=0;
		Bool sc_type_forced = GF_FALSE;
		Bool skip_pck = GF_FALSE;
		u8 ftype;
		u32 tinc;
		u64 size=0;
		u64 fstart=0;
		Bool is_coded;
		u32 bytes_from_store = 0;
		u32 hdr_offset = 0;
		Bool copy_last_bytes = GF_FALSE;

		//not enough bytes to parse start code
		if (remain<5) {
			memcpy(ctx->hdr_store, start, remain);
			ctx->bytes_in_header = remain;
			break;
		}
		current = -1;

		//we have some potential bytes of a start code in the store, copy some more bytes and check if valid start code.
		//if not, dispatch these bytes as continuation of the data
		if (ctx->bytes_in_header) {

			memcpy(ctx->hdr_store + ctx->bytes_in_header, start, MIN_HDR_STORE - ctx->bytes_in_header);
			current = mpgviddmx_next_start_code(ctx->hdr_store, MIN_HDR_STORE);

			//no start code in stored buffer
			if ((current<0) || (current >= (s32) ctx->bytes_in_header) )  {
				if (ctx->opid) {
					dst_pck = gf_filter_pck_new_alloc(ctx->opid, ctx->bytes_in_header, &pck_data);
					if (!dst_pck) return GF_OUT_OF_MEM;

					if (ctx->src_pck) gf_filter_pck_merge_properties(ctx->src_pck, dst_pck);
					gf_filter_pck_set_cts(dst_pck, GF_FILTER_NO_TS);
					gf_filter_pck_set_dts(dst_pck, GF_FILTER_NO_TS);
					gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_NONE);
					memcpy(pck_data, ctx->hdr_store, ctx->bytes_in_header);
					gf_filter_pck_set_framing(dst_pck, GF_FALSE, GF_FALSE);

					if (byte_offset != GF_FILTER_NO_BO) {
						gf_filter_pck_set_byte_offset(dst_pck, byte_offset - ctx->bytes_in_header);
					}

					mpgviddmx_enqueue_or_dispatch(ctx, dst_pck, GF_FALSE, GF_FALSE);
				}

				if (current<0) current = -1;
				else current -= ctx->bytes_in_header;
				ctx->bytes_in_header = 0;
			} else {
				//we have a valid start code, check which byte in our store or in the packet payload is the start code type
				//and remember its location to reinit the parser from there
				hdr_offset = 4 - ctx->bytes_in_header + current;
				//bytes still to dispatch
				bytes_from_store = ctx->bytes_in_header;
				ctx->bytes_in_header = 0;
				if (!hdr_offset) {
					forced_sc_type = ctx->hdr_store[current+3];
				} else {
					forced_sc_type = start[hdr_offset-1];
				}
				sc_type_forced = GF_TRUE;
			}
		}
		//no startcode in store, look for startcode in packet
		if (current == -1) {
			//locate next start code
			current = mpgviddmx_next_start_code(start, remain);
			//no start code, dispatch the block
			if (current<0) {
				u8 b3, b2, b1;
				if (! ctx->frame_started) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[MPGVid] no start code in block and no frame started, discarding data\n" ));
					break;
				}
				size = remain;
				b3 = start[remain-3];
				b2 = start[remain-2];
				b1 = start[remain-1];
				//we may have a startcode at the end of the packet, store it and don't dispatch the last 3 bytes !
				if (!b1 || !b2 || !b3) {
					copy_last_bytes = GF_TRUE;
					assert(size >= 3);
					size -= 3;
					ctx->bytes_in_header = 3;
				}

				dst_pck = gf_filter_pck_new_alloc(ctx->opid, (u32) size, &pck_data);
				if (!dst_pck) return GF_OUT_OF_MEM;

				if (ctx->src_pck) gf_filter_pck_merge_properties(ctx->src_pck, dst_pck);
				memcpy(pck_data, start, (size_t) size);
				gf_filter_pck_set_framing(dst_pck, GF_FALSE, GF_FALSE);
				gf_filter_pck_set_cts(dst_pck, GF_FILTER_NO_TS);
				gf_filter_pck_set_dts(dst_pck, GF_FILTER_NO_TS);

				if (byte_offset != GF_FILTER_NO_BO) {
					gf_filter_pck_set_byte_offset(dst_pck, byte_offset);
				}

				mpgviddmx_enqueue_or_dispatch(ctx, dst_pck, GF_FALSE, GF_FALSE);
				if (copy_last_bytes) {
					memcpy(ctx->hdr_store, start+remain-3, 3);
				}
				break;
			}
		}

		assert(current>=0);

		//if we are in the middle of parsing the vosh, skip over bytes remaining from previous obj not parsed
		if ((vosh_start>=0) && current) {
			assert(remain>=current);
			start += current;
			remain -= current;
			current = 0;
		}
		//also skip if no output pid
		if (!ctx->opid && current) {
			assert(remain>=current);
			start += current;
			remain -= current;
			current = 0;
		}
		//dispatch remaining bytes
		if (current>0) {
			//flush remaining
			dst_pck = gf_filter_pck_new_alloc(ctx->opid, current, &pck_data);
			if (!dst_pck) return GF_OUT_OF_MEM;

			if (ctx->src_pck) gf_filter_pck_merge_properties(ctx->src_pck, dst_pck);
			gf_filter_pck_set_cts(dst_pck, GF_FILTER_NO_TS);
			gf_filter_pck_set_dts(dst_pck, GF_FILTER_NO_TS);
			gf_filter_pck_set_framing(dst_pck, GF_FALSE, GF_FALSE);
			//bytes were partly in store, partly in packet
			if (bytes_from_store) {
				if (byte_offset != GF_FILTER_NO_BO) {
					gf_filter_pck_set_byte_offset(dst_pck, byte_offset - bytes_from_store);
				}
				assert(bytes_from_store>=(u32) current);
				bytes_from_store -= current;
				memcpy(pck_data, ctx->hdr_store, current);
			} else {
				//bytes were only in packet
				if (byte_offset != GF_FILTER_NO_BO) {
					gf_filter_pck_set_byte_offset(dst_pck, byte_offset);
				}
				memcpy(pck_data, start, current);
				assert(remain>=current);
				start += current;
				remain -= current;
				current = 0;
			}
			gf_filter_pck_set_carousel_version(dst_pck, 1);
			mpgviddmx_enqueue_or_dispatch(ctx, dst_pck, GF_FALSE, GF_FALSE);
		}

		//we're align to startcode, stop trashing packets
		ctx->trash_trailer = GF_FALSE;

		//not enough bytes to parse start code
		if (remain<5) {
			memcpy(ctx->hdr_store, start, remain);
			ctx->bytes_in_header = remain;
			break;
		}

		//parse headers
		//we have a start code loaded, eg the data packet does not have a full start code at the beginning
		if (sc_type_forced) {
			gf_bs_reassign_buffer(ctx->bs, start + hdr_offset, remain - hdr_offset);
			sc_type = forced_sc_type;
		} else {
			gf_bs_reassign_buffer(ctx->bs, start, remain);
			gf_bs_read_int(ctx->bs, 24);
			sc_type = gf_bs_read_int(ctx->bs, 8);
		}

		if (ctx->is_mpg12) {
			switch (sc_type) {
			case M2V_SEQ_START_CODE:
			case M2V_EXT_START_CODE:
				gf_bs_reassign_buffer(ctx->bs, start, remain);
				e = gf_m4v_parse_config(ctx->vparser, &ctx->dsi);
				//not enough data, accumulate until we can parse the full header
				if (e==GF_EOS) {
					if (vosh_start<0) vosh_start = 0;
					if (data == ctx->hdr_store) {
						memmove(ctx->hdr_store, start, remain);
						ctx->hdr_store_size = remain;
					} else {
						if (ctx->hdr_store_alloc < ctx->hdr_store_size + pck_size - vosh_start) {
							ctx->hdr_store_alloc = (u32) (ctx->hdr_store_size + pck_size - vosh_start);
							ctx->hdr_store = gf_realloc(ctx->hdr_store, sizeof(char)*ctx->hdr_store_alloc);
						}
						memmove(ctx->hdr_store + ctx->hdr_store_size, data + vosh_start, (size_t) (pck_size - vosh_start) );
						ctx->hdr_store_size += pck_size - (u32) vosh_start;
					}
					gf_filter_pid_drop_packet(ctx->ipid);
					return GF_OK;
				} else if (e != GF_OK) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[MPGVid] Failed to parse VOS header: %s\n", gf_error_to_string(e) ));
				} else if (ctx->dsi.width && ctx->dsi.height) {
					u32 obj_size = (u32) gf_m4v_get_object_start(ctx->vparser);
					if (vosh_start<0) vosh_start = 0;
					vosh_end = start - (u8 *)data + obj_size;
					vosh_end -= vosh_start;
					mpgviddmx_check_pid(filter, ctx,(u32)  vosh_end, data+vosh_start);
				}
				break;
			case M2V_PIC_START_CODE:
				break;
			default:
				break;
			}

		} else {
			u8 PL;
			switch (sc_type) {
			case M4V_VOS_START_CODE:
				ctx->dsi.VideoPL = (u8) gf_bs_read_u8(ctx->bs);
				vosh_start = start - (u8 *)data;
				skip_pck = GF_TRUE;
				assert(remain>=5);
				start += 5;
				remain -= 5;
				break;
			case M4V_VOL_START_CODE:
				gf_bs_reassign_buffer(ctx->bs, start, remain);
				PL = ctx->dsi.VideoPL;
				e = gf_m4v_parse_config(ctx->vparser, &ctx->dsi);
				ctx->dsi.VideoPL = PL;
				//not enough data, accumulate until we can parse the full header
				if (e==GF_EOS) {
					if (vosh_start<0) vosh_start = 0;
					if (data == ctx->hdr_store) {
						memmove(ctx->hdr_store, start, remain);
						ctx->hdr_store_size = remain;
					} else {
						if (ctx->hdr_store_alloc < ctx->hdr_store_size + pck_size - vosh_start) {
							ctx->hdr_store_alloc = (u32) (ctx->hdr_store_size + pck_size - (u32) vosh_start);
							ctx->hdr_store = gf_realloc(ctx->hdr_store, sizeof(char)*ctx->hdr_store_alloc);
						}
						memmove(ctx->hdr_store + ctx->hdr_store_size, data + vosh_start, (size_t) (pck_size - vosh_start) );
						ctx->hdr_store_size += pck_size - (u32) vosh_start;
					}
					gf_filter_pid_drop_packet(ctx->ipid);
					return GF_OK;
				} else if (e != GF_OK) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[MPGVid] Failed to parse VOS header: %s\n", gf_error_to_string(e) ));
				} else {
					u32 obj_size = (u32) gf_m4v_get_object_start(ctx->vparser);
					if (vosh_start<0) vosh_start = 0;
					vosh_end = start - (u8 *)data + obj_size;
					vosh_end -= vosh_start;
					mpgviddmx_check_pid(filter, ctx,(u32)  vosh_end, data+vosh_start);
					skip_pck = GF_TRUE;
					assert(remain>=(s32) obj_size);
					start += obj_size;
					remain -= obj_size;
				}
				break;
			case M4V_VOP_START_CODE:
			case M4V_GOV_START_CODE:
				break;

			case M4V_VO_START_CODE:
			case M4V_VISOBJ_START_CODE:
			default:
				if (vosh_start>=0) {
					skip_pck = GF_TRUE;
					assert(remain>=4);
					start += 4;
					remain -= 4;
				} else if (!ctx->width) {
					gf_bs_reassign_buffer(ctx->bs, start, remain);
					PL = ctx->dsi.VideoPL;
					e = gf_m4v_parse_config(ctx->vparser, &ctx->dsi);
					if (ctx->dsi.width) {
						u32 obj_size = (u32) gf_m4v_get_object_start(ctx->vparser);
						if (vosh_start<0) vosh_start = 0;
						vosh_end = start - (u8 *)data + obj_size;
						vosh_end -= vosh_start;
						mpgviddmx_check_pid(filter, ctx,(u32)  vosh_end, data+vosh_start);
						skip_pck = GF_TRUE;
						assert(remain>=(s32) obj_size);
						start += obj_size;
						remain -= obj_size;
					}
				}
				break;
			}
		}

		if (skip_pck) {
			continue;
		}

		if (!ctx->opid) {
			assert(remain>=4);
			start += 4;
			remain -= 4;
			continue;
		}

		if (!ctx->is_playing) {
			ctx->resume_from = (u32) ((char *)start -  (char *)data);
			return GF_OK;
		}
		//at this point, we no longer reaggregate packets
		ctx->hdr_store_size = 0;

		if (ctx->in_seek) {
			u64 nb_frames_at_seek = (u64) (ctx->start_range * ctx->cur_fps.num);
			if (ctx->cts + ctx->cur_fps.den >= nb_frames_at_seek) {
				//u32 samples_to_discard = (ctx->cts + ctx->dts_inc) - nb_samples_at_seek;
				ctx->in_seek = GF_FALSE;
			}
		}
		//may happen that after all our checks, only 4 bytes are left, continue to store these 4 bytes
		if (remain<5)
			continue;

		//good to go
		gf_m4v_parser_reset(ctx->vparser, sc_type_forced ? forced_sc_type + 1 : 0);
		size = 0;
		e = gf_m4v_parse_frame(ctx->vparser, &ctx->dsi, &ftype, &tinc, &size, &fstart, &is_coded);

		//only for m1v/m2v, for m4v we may have fstart>0 when we strip VO and VISOBJ
		if (ctx->is_mpg12 && fstart && (fstart<remain)) {
			//start code (4 bytes) in header, adjst frame start and size
			if (sc_type_forced) {
				fstart += 4;
				size-=4;
			}
			dst_pck = gf_filter_pck_new_alloc(ctx->opid, (u32) fstart, &pck_data);
			if (!dst_pck) return GF_OUT_OF_MEM;

			if (ctx->src_pck) gf_filter_pck_merge_properties(ctx->src_pck, dst_pck);
			memcpy(pck_data, start, (size_t) fstart);
			gf_filter_pck_set_framing(dst_pck, GF_FALSE, GF_FALSE);
			gf_filter_pck_set_cts(dst_pck, GF_FILTER_NO_TS);
			gf_filter_pck_set_dts(dst_pck, GF_FILTER_NO_TS);

			if (byte_offset != GF_FILTER_NO_BO) {
				gf_filter_pck_set_byte_offset(dst_pck, byte_offset);
				byte_offset+=fstart;
			}

			mpgviddmx_enqueue_or_dispatch(ctx, dst_pck, GF_FALSE, GF_FALSE);

			start += fstart;
			remain -= (s32) fstart;
		}
		//we may have VO or other packets before (fstart is on first of vop/gov/vol/vos)
		else if (fstart && (fstart + size <= remain)) {
			//start code (4 bytes) in header, adjst frame start and size
			if (sc_type_forced) {
				fstart += 4;
				size-=4;
			}
			size += fstart;
		}

		//we skipped bytes already in store + end of start code present in packet, so the size of the first object
		//needs adjustement
		if (bytes_from_store) {
			size += bytes_from_store + hdr_offset;
			if (size > remain) {
				e = GF_NON_COMPLIANT_BITSTREAM;
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[MPGVid] packet too large to process (size %llu remain %d)\n", size, remain ));
				break;
			}
		}

		if ((e == GF_EOS) && !ctx->input_is_au_end) {
			u8 b3 = start[remain-3];
			u8 b2 = start[remain-2];
			u8 b1 = start[remain-1];

			//we may have a startcode at the end of the packet, store it and don't dispatch the last 3 bytes !
			if (!b1 || !b2 || !b3) {
				copy_last_bytes = GF_TRUE;
				assert(size >= 3);
				size -= 3;
				ctx->bytes_in_header = 3;
			}
			full_frame = GF_FALSE;
		} else {
			full_frame = GF_TRUE;
		}

		if (ftype) {
			if (!is_coded) {
				/*if prev is B and we're parsing a packed bitstream discard n-vop*/
				if ((ctx->forced_packed && ctx->b_frames)
					/*policy is to import at variable frame rate, skip*/
					|| ctx->vfr
				) {
					if (ctx->vfr) {
						ctx->is_vfr = GF_TRUE;
						mpgviddmx_update_time(ctx);
					} else {
						ctx->is_packed = GF_TRUE;
					}

					//part of the frame was in store, adjust size
					if (bytes_from_store)
						size-= bytes_from_store + hdr_offset;

					assert(remain>=size);
					start += size;
					remain -= (s32) size;
					//trash all packets until we align to a new startcode
					ctx->trash_trailer = full_frame ? GF_FALSE : GF_TRUE;
					continue;
				}

				/*policy is to keep non coded frame (constant frame rate), add*/
			}

			if (ftype==3) {
				//count number of B-frames since last ref
				ctx->b_frames++;
				ctx->nb_b++;
			} else {
				//flush all pending packets
				mpgviddmx_enqueue_or_dispatch(ctx, NULL, GF_TRUE, GF_FALSE);
				//remember the CTS of the last ref
				ctx->last_ref_cts = ctx->cts;
				if (ctx->max_b < ctx->b_frames) ctx->max_b = ctx->b_frames;

				ctx->b_frames = 0;
				if (ftype==2)
					ctx->nb_p++;
				else
					ctx->nb_i++;
			}
			ctx->nb_frames++;
		} else {
			full_frame = GF_FALSE;
		}

		dst_pck = gf_filter_pck_new_alloc(ctx->opid, (u32) size, &pck_data);
		if (!dst_pck) return GF_OUT_OF_MEM;

		if (ctx->src_pck) gf_filter_pck_merge_properties(ctx->src_pck, dst_pck);
		//bytes come from both our store and the data packet
		if (bytes_from_store) {
			memcpy(pck_data, ctx->hdr_store+current, bytes_from_store);
			assert(size >= bytes_from_store);
			size -= bytes_from_store;
			if (byte_offset != GF_FILTER_NO_BO) {
				gf_filter_pck_set_byte_offset(dst_pck, byte_offset - bytes_from_store);
			}
			memcpy(pck_data + bytes_from_store, start, (size_t) size);
		} else {
			//bytes only come the data packet
			memcpy(pck_data, start, (size_t) size);
			if (byte_offset != GF_FILTER_NO_BO) {
				gf_filter_pck_set_byte_offset(dst_pck, byte_offset + start - (u8 *) data);
			}
		}

		if (ftype) {
			assert(pck_data[0] == 0);
			assert(pck_data[1] == 0);
			assert(pck_data[2] == 1);

			gf_filter_pck_set_framing(dst_pck, GF_TRUE, (full_frame || ctx->input_is_au_end) ? GF_TRUE : GF_FALSE);
			gf_filter_pck_set_cts(dst_pck, ctx->cts);
			gf_filter_pck_set_dts(dst_pck, ctx->dts);

			if (ctx->input_is_au_start) {
				ctx->input_is_au_start = GF_FALSE;
			} else {
				//we use the carousel flag temporarly to indicate the cts must be recomputed
				gf_filter_pck_set_carousel_version(dst_pck, 1);
			}
			gf_filter_pck_set_sap(dst_pck, (ftype==1) ? GF_FILTER_SAP_1 : GF_FILTER_SAP_NONE);
			if (ctx->cur_fps.den > 0) gf_filter_pck_set_duration(dst_pck, ctx->cur_fps.den);
			if (ctx->in_seek) gf_filter_pck_set_seek_flag(dst_pck, GF_TRUE);
			ctx->frame_started = GF_TRUE;

			mpgviddmx_enqueue_or_dispatch(ctx, dst_pck, GF_FALSE, GF_FALSE);

			mpgviddmx_update_time(ctx);
		} else {
			gf_filter_pck_set_framing(dst_pck, GF_FALSE, (full_frame || ctx->input_is_au_end) ? GF_TRUE : GF_FALSE);
			mpgviddmx_enqueue_or_dispatch(ctx, dst_pck, GF_FALSE, GF_FALSE);
		}

		if (!full_frame) {
			if (copy_last_bytes) {
				memcpy(ctx->hdr_store, start+remain-3, 3);
			}
			break;
		}
		assert(remain>=size);
		start += size;
		remain -= (s32) size;
	}
	gf_filter_pid_drop_packet(ctx->ipid);

	return GF_OK;
}

static GF_Err mpgviddmx_initialize(GF_Filter *filter)
{
	GF_MPGVidDmxCtx *ctx = gf_filter_get_udta(filter);
	ctx->hdr_store_size = 0;
	ctx->hdr_store_alloc = MIN_HDR_STORE;
	ctx->hdr_store = gf_malloc(sizeof(char)*ctx->hdr_store_alloc);
	return GF_OK;
}

static void mpgviddmx_finalize(GF_Filter *filter)
{
	GF_MPGVidDmxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->bs) gf_bs_del(ctx->bs);
	if (ctx->vparser) gf_m4v_parser_del_no_bs(ctx->vparser);
	if (ctx->indexes) gf_free(ctx->indexes);
	if (ctx->hdr_store) gf_free(ctx->hdr_store);
	if (ctx->pck_queue) {
		while (gf_list_count(ctx->pck_queue)) {
			GF_FilterPacket *pck = gf_list_pop_back(ctx->pck_queue);
			gf_filter_pck_discard(pck);
		}
		gf_list_del(ctx->pck_queue);
	}
	if (ctx->src_pck) gf_filter_pck_unref(ctx->src_pck);
	if (ctx->importer) {
		GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("%s Import results: %d VOPs (%d Is - %d Ps - %d Bs)\n", ctx->is_mpg12 ? "MPEG-1/2" : "MPEG-4 (Part 2)", ctx->nb_frames, ctx->nb_i, ctx->nb_p, ctx->nb_b));
		if (ctx->nb_b) {
			GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("\t%d max consecutive B-frames%s\n", ctx->max_b, ctx->is_packed ? " - packed bitstream" : "" ));
		}
		if (ctx->is_vfr && ctx->nb_b && ctx->is_packed) {
			GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("Warning: Mix of non-coded frames: packed bitstream and encoder skipped - unpredictable timing\n"));
		}
	}
}

u32 gf_m4v_parser_get_obj_type(GF_M4VParser *m4v);

static const char * mpgvdmx_probe_data(const u8 *data, u32 size, GF_FilterProbeScore *score)
{
	GF_M4VParser *parser;
	u8 ftype;
	u32 tinc, nb_frames, o_start=0;
	u64 fsize, start;
	Bool is_coded;
	GF_Err e;
	GF_M4VDecSpecInfo dsi;

	memset(&dsi, 0, sizeof(GF_M4VDecSpecInfo));
	parser = gf_m4v_parser_new((char*)data, size, GF_FALSE);
	nb_frames = 0;
	while (1) {
		u32 otype;
		ftype = 0;
		is_coded = GF_FALSE;
		e = gf_m4v_parse_frame(parser, &dsi, &ftype, &tinc, &fsize, &start, &is_coded);
		if (!nb_frames && start) o_start = (u32) start;

		otype = gf_m4v_parser_get_obj_type(parser);
		switch (otype) {
		case M4V_VOL_START_CODE:
		case M4V_VOP_START_CODE:
		case M4V_VISOBJ_START_CODE:
		case M4V_VOS_START_CODE:
		case M4V_GOV_START_CODE:
		case M4V_UDTA_START_CODE:
			break;
		default:
			otype = 0;
		}

		//if start is more than 4 (start-code size), we have garbage at the beginning, do not parse
		//except if we have a valid object VOS
		if (!nb_frames && (start>4) && !otype) {
			break;
		}
		if (is_coded) nb_frames++;
		if (e==GF_EOS) {
			e = GF_OK;
			//special case if the only frame we have is not coded
			if (otype == M4V_VOP_START_CODE) {
				if (!nb_frames) nb_frames++;
				is_coded = 1;
			}

			if (is_coded) nb_frames++;
			break;
		}
		if (ftype>2) break;
		if (e) break;
		nb_frames++;
	}
	gf_m4v_parser_del(parser);
	if ((e==GF_OK) && (nb_frames>1)) {
		*score = o_start ? GF_FPROBE_MAYBE_NOT_SUPPORTED : GF_FPROBE_MAYBE_SUPPORTED;
		return "video/mp4v-es";
	}

	memset(&dsi, 0, sizeof(GF_M4VDecSpecInfo));
	parser = gf_m4v_parser_new((char*)data, size, GF_TRUE);
	nb_frames = 0;
	while (1) {
		ftype = 0;
		is_coded = GF_FALSE;
		e = gf_m4v_parse_frame(parser, &dsi, &ftype, &tinc, &fsize, &start, &is_coded);

		//if start is more than 4 (start-code size), we have garbage at the beginning, do not parse
		if (!nb_frames && (start>4) ) {
			break;
		}
		if (is_coded) nb_frames++;
		if (e==GF_EOS) {
			if (is_coded) nb_frames++;
			e = GF_OK;
			break;
		}
		if (ftype>2) break;
		if (e) break;
		nb_frames++;
	}
	gf_m4v_parser_del(parser);
	if ((e==GF_OK) && (nb_frames>1)) {
		*score = o_start ? GF_FPROBE_MAYBE_NOT_SUPPORTED : GF_FPROBE_MAYBE_SUPPORTED;
		return "video/mpgv-es";
	}
	return NULL;
}

static const GF_FilterCapability MPGVidDmxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "cmp|m1v|m2v|m4v"),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "video/mp4v-es|video/mpgv-es"),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_MPEG4_PART2),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_MPEG1),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_MPEG2_SIMPLE),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_MPEG2_SPATIAL),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_MPEG2_SNR),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_MPEG2_MAIN),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_MPEG2_HIGH),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_MPEG2_422),
	CAP_BOOL(GF_CAPS_OUTPUT_STATIC_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG4_PART2),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG1),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG2_SPATIAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG2_SNR),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG2_SIMPLE),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG2_MAIN),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG2_HIGH),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG2_422),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
};


#define OFFS(_n)	#_n, offsetof(GF_MPGVidDmxCtx, _n)
static const GF_FilterArgs MPGVidDmxArgs[] =
{
	{ OFFS(fps), "import frame rate (0 default to FPS from bitstream or 25 Hz)", GF_PROP_FRACTION, "0/1000", NULL, 0},
	{ OFFS(index), "indexing window length. If 0, bitstream is not probed for duration. A negative value skips the indexing if the source file is larger than 20M (slows down importers) unless a play with start range > 0 is issued", GF_PROP_DOUBLE, "-1.0", NULL, 0},
	{ OFFS(vfr), "set variable frame rate import", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(importer), "compatibility with old importer, displays import results", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(notime), "ignore input timestamps, rebuild from 0", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{0}
};


GF_FilterRegister MPGVidDmxRegister = {
	.name = "rfmpgvid",
	GF_FS_SET_DESCRIPTION("M1V/M2V/M4V reframer")
	GF_FS_SET_HELP("This filter parses MPEG-1/2 and MPEG-4 part 2 video files/data and outputs corresponding video PID and frames.\n"
		"Note: The filter uses negative CTS offsets: CTS is correct, but some frames may have DTS greater than CTS.")
	.private_size = sizeof(GF_MPGVidDmxCtx),
	.args = MPGVidDmxArgs,
	.initialize = mpgviddmx_initialize,
	.finalize = mpgviddmx_finalize,
	SETCAPS(MPGVidDmxCaps),
	.configure_pid = mpgviddmx_configure_pid,
	.process = mpgviddmx_process,
	.probe_data = mpgvdmx_probe_data,
	.process_event = mpgviddmx_process_event
};


const GF_FilterRegister *rfmpgvid_register(GF_FilterSession *session)
{
	return &MPGVidDmxRegister;
}
#else
const GF_FilterRegister *rfmpgvid_register(GF_FilterSession *session)
{
	return NULL;
}
#endif // #if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_RFMPGVID)
