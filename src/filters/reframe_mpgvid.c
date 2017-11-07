/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2017
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

typedef struct
{
	u64 pos;
	Double duration;
} MPGVidIdx;

typedef struct
{
	//filter args
	GF_Fraction fps;
	Double index_dur;

	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;

	GF_BitStream *bs;
	u64 cts;
	u32 width, height;
	GF_Fraction duration;
	Double start_range;
	Bool in_seek;
	u32 timescale;

	u32 resume_from;

	Bool is_mpg12, need_init;
	GF_M4VParser *vparser;
	GF_M4VDecSpecInfo dsi;

	u32 bytes_in_header;
	char hdr_store[8];

	Bool is_playing;
	Bool is_file, file_loaded;
	Bool initial_play_done;

	MPGVidIdx *indexes;
	u32 index_alloc_size, index_size;
} GF_MPGVidDmxCtx;


GF_Err mpgviddmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	Bool was_mpeg12;
	const GF_PropertyValue *p;
	GF_MPGVidDmxCtx *ctx = gf_filter_get_udta(filter);

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

	was_mpeg12 = ctx->is_mpg12;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_OTI);
	if (p) {
		switch (p->value.uint) {
		case GPAC_OTI_VIDEO_MPEG1:
		case GPAC_OTI_VIDEO_MPEG2_422:
		case GPAC_OTI_VIDEO_MPEG2_SNR:
		case GPAC_OTI_VIDEO_MPEG2_HIGH:
		case GPAC_OTI_VIDEO_MPEG2_MAIN:
		case GPAC_OTI_VIDEO_MPEG2_SIMPLE:
		case GPAC_OTI_VIDEO_MPEG2_SPATIAL:
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
		gf_m4v_parser_del(ctx->vparser);
		ctx->vparser = NULL;
	}

	return GF_OK;
}


static void mpgviddmx_check_dur(GF_Filter *filter, GF_MPGVidDmxCtx *ctx)
{

	FILE *stream;
	GF_BitStream *bs;
	GF_M4VParser *vparser;
	GF_M4VDecSpecInfo dsi;
	GF_Err e;
	u64 duration, cur_dur;
	const GF_PropertyValue *p;
	if (!ctx->opid || ctx->timescale || ctx->file_loaded) return;

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILEPATH);
	if (!p || !p->value.string) {
		ctx->is_file = GF_FALSE;
		return;
	}
	ctx->is_file = GF_TRUE;

	stream = gf_fopen(p->value.string, "r");
	if (!stream) return;

	ctx->index_size = 0;

	bs = gf_bs_from_file(stream, GF_BITSTREAM_READ);

	vparser = gf_m4v_parser_bs_new(bs, ctx->is_mpg12);
	e = gf_m4v_parse_config(vparser, &dsi);
	if (e) {
		GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[MPGVid] Could not parse video header - duration  not estimated\n"));
		ctx->file_loaded = GF_TRUE;
		return;
	}

	duration = 0;
	while (gf_bs_available(bs)) {
		u8 ftype;
		u32 tinc;
		u64 fsize, start;
		Bool is_coded;
		u64 pos = gf_bs_get_position(bs);
		pos = gf_m4v_get_object_start(vparser);
		e = gf_m4v_parse_frame(vparser, dsi, &ftype, &tinc, &fsize, &start, &is_coded);

		duration += ctx->fps.den;
		cur_dur += ctx->fps.den;
		//only index at I-frame start
		if (pos && (ftype==0) && (cur_dur > ctx->index_dur * ctx->fps.num) ) {
			if (!ctx->index_alloc_size) ctx->index_alloc_size = 10;
			else if (ctx->index_alloc_size == ctx->index_size) ctx->index_alloc_size *= 2;
			ctx->indexes = gf_realloc(ctx->indexes, sizeof(MPGVidIdx)*ctx->index_alloc_size);
			ctx->indexes[ctx->index_size].pos = pos;
			ctx->indexes[ctx->index_size].duration = duration;
			ctx->indexes[ctx->index_size].duration /= ctx->fps.num;
			ctx->index_size ++;
			cur_dur = 0;
		}
	}
	gf_m4v_parser_del(vparser);
	gf_bs_del(bs);
	gf_fclose(stream);

	if (!ctx->duration.num || (ctx->duration.num  * ctx->fps.num != duration * ctx->duration.den)) {
		ctx->duration.num = duration;
		ctx->duration.den = ctx->fps.num;

		gf_filter_pid_set_info(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC(ctx->duration));
	}

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILE_CACHED);
	if (p && p->value.boolean) ctx->file_loaded = GF_TRUE;
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CAN_DATAREF, & PROP_BOOL(GF_TRUE ) );
}

static void mpgviddmx_check_pid(GF_Filter *filter, GF_MPGVidDmxCtx *ctx, u32 width, u32 height)
{

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, & PROP_UINT(GF_STREAM_VISUAL));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_OTI, & PROP_UINT(GPAC_OTI_VIDEO_H263));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, & PROP_UINT(ctx->fps.num));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FPS, & PROP_FRAC(ctx->fps));

		mpgviddmx_check_dur(filter, ctx);
	}
	if ((ctx->width == width) && (ctx->height == height)) return;
	ctx->width = width;
	ctx->height = height;
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, & PROP_UINT( width));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, & PROP_UINT( height));
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
			return GF_FALSE;
		}
		ctx->start_range = evt->play.start_range;
		ctx->in_seek = GF_TRUE;

		if (ctx->start_range) {
			for (i=1; i<ctx->index_size; i++) {
				if (ctx->indexes[i].duration>ctx->start_range) {
					ctx->cts = ctx->indexes[i-1].duration * ctx->fps.num;
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

static GFINLINE void mpgviddmx_update_cts(GF_MPGVidDmxCtx *ctx)
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
		if (bpos == size) return -1;
		v = ( (v<<8) & 0xFFFFFF00) | data[bpos];

		bpos++;
		if ((v & 0xFFFFFF00) == 0x00000100) {
			end = start + bpos - 4;
			found = 1;
			break;
		}
	}
	if (!found) return -1;
	return (s32) (end - start);
}


GF_Err mpgviddmx_process(GF_Filter *filter)
{
	GF_MPGVidDmxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck;
	u64 byte_offset;
	s64 vosh_start = -1;
	GF_Err e;
	char *data;
	u8 *start;
	Bool first_frame_found = GF_FALSE;
	u32 pck_size;
	s32 remain;

	//always reparse duration
	if (!ctx->duration.num)
		mpgviddmx_check_dur(filter, ctx);

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}

	data = (char *) gf_filter_pck_get_data(pck, &pck_size);
	byte_offset = gf_filter_pck_get_byte_offset(pck);

	start = data;
	remain = pck_size;


	if (ctx->bytes_in_header) {
#if 0
		if (ctx->bytes_in_header + remain < 7) {
			memcpy(ctx->header + ctx->bytes_in_header, start, remain);
			ctx->bytes_in_header += remain;
			gf_filter_pid_drop_packet(ctx->ipid);
			return GF_OK;
		}
		alread_sync = 7 - ctx->bytes_in_header;
		memcpy(ctx->header + ctx->bytes_in_header, start, alread_sync);
		start += alread_sync;
		remain -= alread_sync;
		ctx->bytes_in_header = 0;
		alread_sync = GF_TRUE;
#endif

	}
	//input pid sets some timescale - we flushed pending data , update cts
	else if (ctx->timescale) {
		u64 cts = gf_filter_pck_get_cts(pck);
		if (cts != GF_FILTER_NO_TS)
			ctx->cts = cts;
	}

	if (ctx->resume_from) {
		if (gf_filter_pid_would_block(ctx->opid))
			return GF_OK;
		start += ctx->resume_from;
		remain -= ctx->resume_from;
		ctx->resume_from = 0;
	}

	if (!ctx->bs) {
		ctx->bs = gf_bs_new(start, remain, GF_BITSTREAM_READ);
	} else {
		gf_bs_reassign_buffer(ctx->bs, start, remain);
	}
	if (!ctx->vparser) {
		ctx->vparser = gf_m4v_parser_bs_new(ctx->bs, ctx->is_mpg12);
		ctx->need_init = GF_TRUE;
	}

	while (remain) {
		u32 size=0;
		Bool full_frame;
		char *pck_data;
		s32 current, next;
		u32 w, h;
		u8 sc_type;

		//not enough bytes
		if (remain<5) {
			memcpy(ctx->hdr_store, start, remain);
			ctx->bytes_in_header = remain;
			break;
		}

		if (ctx->bytes_in_header) {
			assert(!first_frame_found);

			memcpy(ctx->hdr_store + ctx->bytes_in_header, start, 8 - ctx->bytes_in_header);
			current = mpgviddmx_next_start_code(ctx->hdr_store, 8);

			//no start code in stored buffer
			if (current<0) {
				dst_pck = gf_filter_pck_new_alloc(ctx->opid, ctx->bytes_in_header, &pck_data);
				memcpy(pck_data, ctx->hdr_store, ctx->bytes_in_header);
				gf_filter_pck_set_framing(dst_pck, GF_FALSE, GF_FALSE);
				gf_filter_pck_set_cts(dst_pck, ctx->cts);
				gf_filter_pck_set_duration(dst_pck, ctx->fps.den);
				if (ctx->in_seek) gf_filter_pck_set_seek_flag(dst_pck, GF_TRUE);

				if (byte_offset != GF_FILTER_NO_BO) {
					gf_filter_pck_set_byte_offset(dst_pck, byte_offset - ctx->bytes_in_header);
				}
				gf_filter_pck_send(dst_pck);
				ctx->bytes_in_header = 0;

				current = mpgviddmx_next_start_code(start, remain);
			}
		} else {
			//locate next start code
			current = mpgviddmx_next_start_code(start, remain);
			assert(current>=0);
		}


		if (current<0) {
			//not enough bytes to process start code !!
			assert(0);
		}

		if (current>0) {
			assert(!first_frame_found);
			//flush remaining
			dst_pck = gf_filter_pck_new_alloc(ctx->opid, current, &pck_data);
			if (ctx->bytes_in_header) {
				if (byte_offset != GF_FILTER_NO_BO) {
					gf_filter_pck_set_byte_offset(dst_pck, byte_offset - ctx->bytes_in_header);
				}
				ctx->bytes_in_header -= current;
				memcpy(pck_data, ctx->hdr_store, current);
			} else {
				if (byte_offset != GF_FILTER_NO_BO) {
					gf_filter_pck_set_byte_offset(dst_pck, byte_offset);
				}
				memcpy(pck_data, start, current);
				start += current;
				remain -= current;
			}
			gf_filter_pck_set_framing(dst_pck, GF_FALSE, GF_TRUE);
			gf_filter_pck_set_cts(dst_pck, ctx->cts);
			gf_filter_pck_set_duration(dst_pck, ctx->fps.den);
			if (ctx->in_seek) gf_filter_pck_set_seek_flag(dst_pck, GF_TRUE);
			gf_filter_pck_send(dst_pck);

			mpgviddmx_update_cts(ctx);
			first_frame_found = GF_TRUE;
		}

		//parse headers
		w = h = 0;

		gf_bs_reassign_buffer(ctx->bs, start, remain);
		gf_bs_read_int(ctx->bs, 24);
		sc_type = gf_bs_read_int(ctx->bs, 8);

		if (ctx->is_mpg12) {

		} else {
			switch (sc_type) {
			case M4V_VOS_START_CODE:
				ctx->dsi.VideoPL = (u8) gf_bs_read_u8(ctx->bs);
				vosh_start = start - data;
				break;
			case M4V_VOL_START_CODE:
				gf_bs_reassign_buffer(ctx->bs, start, remain);
				break;
			}
		}

		mpgviddmx_check_pid(filter, ctx, w, h);


		if (!ctx->is_playing) {
			ctx->resume_from = (char *)start -  (char *)data;
			return GF_OK;
		}

		if (ctx->in_seek) {
			u64 nb_frames_at_seek = ctx->start_range * ctx->fps.num;
			if (ctx->cts + ctx->fps.den >= nb_frames_at_seek) {
				//u32 samples_to_discard = (ctx->cts + ctx->dts_inc) - nb_samples_at_seek;
				ctx->in_seek = GF_FALSE;
			}
		}

		//good to go
		next = mpgviddmx_next_start_code(start+1, remain-1);

		if (next>0) {
			size = next+1 + ctx->bytes_in_header;
			full_frame = GF_TRUE;
		} else {
			u8 b3 = start[remain-3];
			u8 b2 = start[remain-2];
			u8 b1 = start[remain-1];
			//we may have a startcode here !
			if (!b1 || !b2 || !b3) {
				memcpy(ctx->hdr_store, start+remain-3, 3);
				remain -= 3;
				ctx->bytes_in_header = 3;
			}
			size = remain;
			full_frame = GF_FALSE;
		}

		dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &pck_data);
		if (ctx->bytes_in_header && current) {
			memcpy(pck_data, ctx->hdr_store+current, ctx->bytes_in_header);
			size -= ctx->bytes_in_header;
			ctx->bytes_in_header = 0;
			if (byte_offset != GF_FILTER_NO_BO) {
				gf_filter_pck_set_byte_offset(dst_pck, byte_offset + ctx->bytes_in_header);
			}
			memcpy(pck_data, start, size);
		} else {
			memcpy(pck_data, start, size);
			if (byte_offset != GF_FILTER_NO_BO) {
				gf_filter_pck_set_byte_offset(dst_pck, byte_offset + start - (u8 *) data);
			}
		}

		gf_filter_pck_set_framing(dst_pck, GF_TRUE, full_frame);
		gf_filter_pck_set_cts(dst_pck, ctx->cts);
		gf_filter_pck_set_sap(dst_pck, (start[4]&0x02) ? 0 : 1);
		gf_filter_pck_set_duration(dst_pck, ctx->fps.den);
		if (ctx->in_seek) gf_filter_pck_set_seek_flag(dst_pck, GF_TRUE);
		gf_filter_pck_send(dst_pck);

		first_frame_found = GF_TRUE;
		start += size;
		remain -= size;
		if (!full_frame) break;
		mpgviddmx_update_cts(ctx);


		//don't demux too much of input, abort when we would block. This avoid dispatching
		//a huge number of frames in a single call
		if (gf_filter_pid_would_block(ctx->opid)) {
			ctx->resume_from = (char *)start -  (char *)data;
			return GF_OK;
		}
	}
	gf_filter_pid_drop_packet(ctx->ipid);

	return GF_OK;
}

static void mpgviddmx_finalize(GF_Filter *filter)
{
	GF_MPGVidDmxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->bs) gf_bs_del(ctx->bs);
	if (ctx->indexes) gf_free(ctx->indexes);
}

static const GF_FilterCapability MPGVidDmxInputs[] =
{
	CAP_INC_STRING(GF_PROP_PID_MIME, "video/mp4v-es|video/x-mpgv-es"),
	{},
	CAP_INC_STRING(GF_PROP_PID_FILE_EXT, "cmp|m1v|m2v"),
	{},
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_VIDEO_MPEG4_PART2),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_VIDEO_MPEG1),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_VIDEO_MPEG2_SPATIAL),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_VIDEO_MPEG2_SNR),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_VIDEO_MPEG2_SIMPLE),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_VIDEO_MPEG2_MAIN),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_VIDEO_MPEG2_HIGH),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_VIDEO_MPEG2_422),
	CAP_INC_BOOL(GF_PROP_PID_UNFRAMED, GF_TRUE),
};


static const GF_FilterCapability MPGVidDmxOutputs[] =
{
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_VIDEO_MPEG4_PART2),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_VIDEO_MPEG1),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_VIDEO_MPEG2_SPATIAL),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_VIDEO_MPEG2_SNR),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_VIDEO_MPEG2_SIMPLE),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_VIDEO_MPEG2_MAIN),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_VIDEO_MPEG2_HIGH),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_VIDEO_MPEG2_422),
	{}
};

#define OFFS(_n)	#_n, offsetof(GF_MPGVidDmxCtx, _n)
static const GF_FilterArgs MPGVidDmxArgs[] =
{
	{ OFFS(fps), "import frame rate", GF_PROP_FRACTION, "15000/1000", NULL, GF_FALSE},
	{ OFFS(index_dur), "indexing window length", GF_PROP_DOUBLE, "1.0", NULL, GF_FALSE},
	{}
};


GF_FilterRegister MPGVidDmxRegister = {
	.name = "reframe_mpgvid",
	.description = "MPEG-1/2/4 (Part2) Video Demux",
	.private_size = sizeof(GF_MPGVidDmxCtx),
	.args = MPGVidDmxArgs,
	.finalize = mpgviddmx_finalize,
	INCAPS(MPGVidDmxInputs),
	OUTCAPS(MPGVidDmxOutputs),
	.configure_pid = mpgviddmx_configure_pid,
	.process = mpgviddmx_process,
	.process_event = mpgviddmx_process_event
};


const GF_FilterRegister *mpgviddmx_register(GF_FilterSession *session)
{
	return &MPGVidDmxRegister;
}
