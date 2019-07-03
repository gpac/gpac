/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2017
 *					All rights reserved
 *
 *  This file is part of GPAC / H263 reframer filter
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

typedef struct
{
	u64 pos;
	Double duration;
} H263Idx;

typedef struct
{
	//filter args
	GF_Fraction fps;
	Double index;

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


	u32 bytes_in_header;
	char hdr_store[8];

	Bool is_playing;
	Bool is_file, file_loaded;
	Bool initial_play_done;

	GF_FilterPacket *src_pck;

	H263Idx *indexes;
	u32 index_alloc_size, index_size;
} GF_H263DmxCtx;


GF_Err h263dmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	GF_H263DmxCtx *ctx = gf_filter_get_udta(filter);

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

	if (ctx->timescale && !ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, NULL);
	}
	return GF_OK;
}

#define H263_CACHE_SIZE	4096
static u32 h263dmx_next_start_code_bs(GF_BitStream *bs)
{
	u32 v, bpos;
	unsigned char h263_cache[H263_CACHE_SIZE];
	u64 end, cache_start, load_size;
	u64 start = gf_bs_get_position(bs);

	/*skip 16b header*/
	gf_bs_read_u16(bs);
	bpos = 0;
	load_size = 0;
	cache_start = 0;
	end = 0;
	v = 0xffffffff;
	while (!end) {
		/*refill cache*/
		if (bpos == (u32) load_size) {
			if (!gf_bs_available(bs)) break;
			load_size = gf_bs_available(bs);
			if (load_size>H263_CACHE_SIZE) load_size=H263_CACHE_SIZE;
			bpos = 0;
			cache_start = gf_bs_get_position(bs);
			gf_bs_read_data(bs, (char *) h263_cache, (u32) load_size);
		}
		v = (v<<8) | h263_cache[bpos];
		bpos++;
		if ((v >> (32-22)) == 0x20) end = cache_start+bpos-4;
	}
	gf_bs_seek(bs, start);
	if (!end) end = gf_bs_get_size(bs);
	return (u32) (end-start);
}
static void h263dmx_check_dur(GF_Filter *filter, GF_H263DmxCtx *ctx)
{

	FILE *stream;
	GF_BitStream *bs;
	u64 duration, cur_dur;
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
	duration = 0;
	cur_dur = 0;
	while (gf_bs_available(bs)) {
		u8 type;
		u64 pos = gf_bs_get_position(bs);
		u64 next_pos = pos + h263dmx_next_start_code_bs(bs);
		gf_bs_read_u32(bs);
		type = gf_bs_read_u8(bs);

		if (type & 0x02) type = 0;
		else
			type = 1;


		duration += ctx->fps.den;
		cur_dur += ctx->fps.den;
		//only index at I-frame start
		if (pos && type && (cur_dur > ctx->index * ctx->fps.num) ) {
			if (!ctx->index_alloc_size) ctx->index_alloc_size = 10;
			else if (ctx->index_alloc_size == ctx->index_size) ctx->index_alloc_size *= 2;
			ctx->indexes = gf_realloc(ctx->indexes, sizeof(H263Idx)*ctx->index_alloc_size);
			ctx->indexes[ctx->index_size].pos = pos;
			ctx->indexes[ctx->index_size].duration = (Double) duration;
			ctx->indexes[ctx->index_size].duration /= ctx->fps.num;
			ctx->index_size ++;
			cur_dur = 0;
		}

		gf_bs_seek(bs, next_pos);
	}
	gf_bs_del(bs);
	gf_fclose(stream);

	if (!ctx->duration.num || (ctx->duration.num  * ctx->fps.num != duration * ctx->duration.den)) {
		ctx->duration.num = (s32) duration;
		ctx->duration.den = ctx->fps.num;

		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC(ctx->duration));
	}

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILE_CACHED);
	if (p && p->value.boolean) ctx->file_loaded = GF_TRUE;
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CAN_DATAREF, & PROP_BOOL(GF_TRUE ) );
}

static void h263dmx_check_pid(GF_Filter *filter, GF_H263DmxCtx *ctx, u32 width, u32 height)
{

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		h263dmx_check_dur(filter, ctx);
	}
	if ((ctx->width == width) && (ctx->height == height)) return;

	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, & PROP_UINT(GF_STREAM_VISUAL));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, & PROP_UINT(GF_CODECID_H263));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, & PROP_UINT(ctx->fps.num));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FPS, & PROP_FRAC(ctx->fps));

	if (ctx->duration.num)
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC(ctx->duration));

	ctx->width = width;
	ctx->height = height;
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, & PROP_UINT( width));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, & PROP_UINT( height));

	if (ctx->is_file && ctx->index) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PLAYBACK_MODE, & PROP_UINT(GF_PLAYBACK_MODE_FASTFORWARD) );
	}
}

static Bool h263dmx_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	u32 i;
	u64 file_pos = 0;
	GF_FilterEvent fevt;
	GF_H263DmxCtx *ctx = gf_filter_get_udta(filter);

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

static GFINLINE void h263dmx_update_cts(GF_H263DmxCtx *ctx)
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


static s32 h263dmx_next_start_code(u8 *data, u32 size)
{
	u32 v, bpos;
	s64 end;
	s64 start = 0;

	/*skip 16b header*/
	start+=2;
	bpos = 0;
	end = 0;
	v = 0xffffffff;
	while (!end) {
		if (bpos == size) return -1;
		v = (v<<8) | data[bpos];
		bpos++;
		if ((v >> (32-22)) == 0x20) end = start + bpos - 4;
	}

	return (s32) (end-start);
}

static void h263_get_pic_size(GF_BitStream *bs, u32 fmt, u32 *w, u32 *h)
{
	switch (fmt) {
	case 1:
		*w = 128;
		*h = 96;
		break;
	case 2:
		*w = 176;
		*h = 144;
		break;
	case 3:
		*w = 352;
		*h = 288;
		break;
	case 4:
		*w = 704;
		*h = 576;
		break;
	case 5:
		*w = 1409;
		*h = 1152 ;
		break;
	default:
		*w = *h = 0;
		break;
	}
}

GF_Err h263dmx_process(GF_Filter *filter)
{
	GF_H263DmxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck;
	u64 byte_offset;
	char *data;
	u8 *start;
	Bool first_frame_found = GF_FALSE;
	u32 pck_size;
	s32 remain;

	//always reparse duration
	if (!ctx->duration.num)
		h263dmx_check_dur(filter, ctx);

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
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
		if (ctx->src_pck) gf_filter_pck_unref(ctx->src_pck);
		ctx->src_pck = pck;
		gf_filter_pck_ref_props(&ctx->src_pck);
	}

	if (ctx->resume_from) {
		if (gf_filter_pid_would_block(ctx->opid))
			return GF_OK;
		start += ctx->resume_from;
		remain -= ctx->resume_from;
		ctx->resume_from = 0;
	}

	while (remain) {
		u32 size=0;
		Bool full_frame;
		u8 *pck_data;
		s32 current, next;
		u32 fmt, w, h;

		//not enough bytes
		if (remain<5) {
			memcpy(ctx->hdr_store, start, remain);
			ctx->bytes_in_header = remain;
			break;
		}

		if (ctx->bytes_in_header) {
			if (first_frame_found) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[H263Dmx] corrupted frame!\n"));
			}

			memcpy(ctx->hdr_store + ctx->bytes_in_header, start, 8 - ctx->bytes_in_header);
			current = h263dmx_next_start_code(ctx->hdr_store, 8);

			//no start code in stored buffer
			if (current<0) {
				dst_pck = gf_filter_pck_new_alloc(ctx->opid, ctx->bytes_in_header, &pck_data);
				if (ctx->src_pck) gf_filter_pck_merge_properties(ctx->src_pck, dst_pck);

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

				current = h263dmx_next_start_code(start, remain);
			}
		} else {
			//locate next start code
			current = h263dmx_next_start_code(start, remain);
		}


		if (current<0) {
			//not enough bytes to process start code !!
			break;
		}

		if (current>0) {
			if (!ctx->opid) {
				if (ctx->bytes_in_header) {
					ctx->bytes_in_header -= current;
				} else {
					start += current;
					remain -= current;
				}
				GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[H263Dmx] garbage before first frame!\n"));
				continue;
			}
			if (first_frame_found) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[H263Dmx] corrupted frame!\n"));
			}
			//flush remaining
			dst_pck = gf_filter_pck_new_alloc(ctx->opid, current, &pck_data);
			if (ctx->src_pck) gf_filter_pck_merge_properties(ctx->src_pck, dst_pck);

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

			h263dmx_update_cts(ctx);
		}

		if (ctx->bytes_in_header) {
			gf_bs_reassign_buffer(ctx->bs, ctx->hdr_store+current, 8-current);
		} else if (!ctx->bs) {
			ctx->bs = gf_bs_new(start, remain, GF_BITSTREAM_READ);
		} else {
			gf_bs_reassign_buffer(ctx->bs, start, remain);
		}
		/*parse header*/
		gf_bs_read_int(ctx->bs, 22);
		gf_bs_read_int(ctx->bs, 8);
		/*spare+0+split_screen_indicator+document_camera_indicator+freeze_picture_release*/
		gf_bs_read_int(ctx->bs, 5);

		fmt = gf_bs_read_int(ctx->bs, 3);
		h263_get_pic_size(ctx->bs, fmt, &w, &h);

		h263dmx_check_pid(filter, ctx, w, h);

		if (!ctx->is_playing) {
			ctx->resume_from = (u32) ( (char *)start -  (char *)data );
			return GF_OK;
		}

		if (ctx->in_seek) {
			u64 nb_frames_at_seek = (u64) (ctx->start_range * ctx->fps.num);
			if (ctx->cts + ctx->fps.den >= nb_frames_at_seek) {
				//u32 samples_to_discard = (ctx->cts + ctx->dts_inc) - nb_samples_at_seek;
				ctx->in_seek = GF_FALSE;
			}
		}

		//good to go
		next = h263dmx_next_start_code(start+1, remain-1);

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
		if (ctx->src_pck) gf_filter_pck_merge_properties(ctx->src_pck, dst_pck);
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
		gf_filter_pck_set_sap(dst_pck, (start[4]&0x02) ? GF_FILTER_SAP_NONE : GF_FILTER_SAP_1);
		gf_filter_pck_set_duration(dst_pck, ctx->fps.den);
		if (ctx->in_seek) gf_filter_pck_set_seek_flag(dst_pck, GF_TRUE);
		gf_filter_pck_send(dst_pck);

		first_frame_found = GF_TRUE;
		start += size;
		remain -= size;
		if (!full_frame) break;
		h263dmx_update_cts(ctx);


		//don't demux too much of input, abort when we would block. This avoid dispatching
		//a huge number of frames in a single call
		if (gf_filter_pid_would_block(ctx->opid)) {
			ctx->resume_from = (u32) ( (char *)start -  (char *)data);
			return GF_OK;
		}
	}
	gf_filter_pid_drop_packet(ctx->ipid);

	return GF_OK;
}

static void h263dmx_finalize(GF_Filter *filter)
{
	GF_H263DmxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->bs) gf_bs_del(ctx->bs);
	if (ctx->indexes) gf_free(ctx->indexes);
}

static const char * h263dmx_probe_data(const u8 *data, u32 size, GF_FilterProbeScore *score)
{
	u32 nb_frames=0;
	u32 max_nb_frames=0;
	u32 prev_fmt=0;
	s32 current = h263dmx_next_start_code((u8*)data, size);
	while (size && (current>=0) && (current< (s32) size)) {
		u32 fmt=0;
		data += current;
		size -= current;

		GF_BitStream *bs = gf_bs_new((u8 *)data, size, GF_BITSTREAM_READ);

		/*parse header*/
		gf_bs_read_int(bs, 22);
		gf_bs_read_int(bs, 8);
		gf_bs_read_int(bs, 5);

		fmt = gf_bs_read_int(bs, 3);
		gf_bs_del(bs);

		if (fmt>=1 && (fmt<=5)) {
			if (!prev_fmt || (prev_fmt==fmt)) {
				nb_frames++;
			} else {
				if (nb_frames>max_nb_frames) {
					max_nb_frames = nb_frames;
				}
			}
			prev_fmt=fmt;
		} else {
			nb_frames=0;
			break;
		}
		current = h263dmx_next_start_code((u8*)data+1, size-1);
		if (current<=0) break;
		current++;
		if ((s32) size < current) break;
	}
	if (nb_frames>max_nb_frames) {
		max_nb_frames = nb_frames;
	}
	if (max_nb_frames) {
		*score = max_nb_frames>4 ? GF_FPROBE_SUPPORTED : GF_FPROBE_MAYBE_SUPPORTED;
		return "video/h263";
	}
	return NULL;
}

static const GF_FilterCapability H263DmxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "video/h263"),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_S263),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_H263),
	CAP_BOOL(GF_CAPS_OUTPUT_STATIC_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "263|h263"),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_S263),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_H263),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
};

#define OFFS(_n)	#_n, offsetof(GF_H263DmxCtx, _n)
static const GF_FilterArgs H263DmxArgs[] =
{
	{ OFFS(fps), "import frame rate", GF_PROP_FRACTION, "15000/1000", NULL, 0},
	{ OFFS(index), "indexing window length", GF_PROP_DOUBLE, "1.0", NULL, 0},
	{0}
};


GF_FilterRegister H263DmxRegister = {
	.name = "rfh263",
	GF_FS_SET_DESCRIPTION("H263 reframer")
	GF_FS_SET_HELP("This filter parses H263 files/data and outputs corresponding visual PID and frames.")
	.private_size = sizeof(GF_H263DmxCtx),
	.args = H263DmxArgs,
	.finalize = h263dmx_finalize,
	SETCAPS(H263DmxCaps),
	.configure_pid = h263dmx_configure_pid,
	.process = h263dmx_process,
	.probe_data = h263dmx_probe_data,
	.process_event = h263dmx_process_event
};


const GF_FilterRegister *h263dmx_register(GF_FilterSession *session)
{
	return &H263DmxRegister;
}
