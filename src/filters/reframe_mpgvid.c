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
	Bool vfr;

	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;

	GF_BitStream *bs;
	u64 cts, dts;
	u32 width, height;
	GF_Fraction duration;
	Double start_range;
	Bool in_seek;
	u32 timescale;

	u32 resume_from;

	Bool is_mpg12, forced_packed;
	GF_M4VParser *vparser;
	GF_M4VDecSpecInfo dsi;
	u32 b_frames;
	Bool is_packed, is_vfr;
	GF_List *pck_queue;
	u64 last_ref_cts;
	Bool frame_started;

	u32 bytes_in_header;
	char *hdr_store;
	u32 hdr_store_size, hdr_store_alloc;

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
		gf_m4v_parser_del_no_bs(ctx->vparser);
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
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[MPGVid] Could not parse video header - duration  not estimated\n"));
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
		if (pos && (ftype==0) && (cur_dur >= ctx->index_dur * ctx->fps.num) ) {
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


static void mpgviddmx_enqueue_or_dispatch(GF_MPGVidDmxCtx *ctx, GF_FilterPacket *pck, Bool flush_ref)
{
	if (ctx->timescale) {
		gf_filter_pck_send(pck);
	}
	//TODO: we are dispacthing frames in "negctts mode", ie we may have DTS>CTS
	//need to signal this for consumers using DTS (eg MPEG-2 TS)
	if (flush_ref && ctx->pck_queue) {
		//send all reference packet queued
		u32 i, count = gf_list_count(ctx->pck_queue);
		for (i=0; i<count; i++) {
			u64 cts;
			GF_FilterPacket *pck = gf_list_get(ctx->pck_queue, i);
			cts = gf_filter_pck_get_cts(pck);
			if (cts != GF_FILTER_NO_TS) {
				//offset the cts of the ref frame to the number of B frames inbetween
				if (ctx->last_ref_cts == cts) {
					cts += ctx->b_frames * ctx->fps.den;
					gf_filter_pck_set_cts(pck, cts);
				} else {
					//shift all other frames (i.e. pending Bs) by 1 frame in the past since we move the ref frame after them
					assert(cts >= ctx->fps.den);
					cts -= ctx->fps.den;
					gf_filter_pck_set_cts(pck, cts);
				}
			}
			gf_filter_pck_send(pck);
		}
		gf_list_reset(ctx->pck_queue);
	}
	if (!pck) return;

	if (!ctx->pck_queue) ctx->pck_queue = gf_list_new();
	gf_list_add(ctx->pck_queue, pck);
}

static void mpgviddmx_check_pid(GF_Filter *filter, GF_MPGVidDmxCtx *ctx, u32 vosh_size, u8 *data)
{
	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, & PROP_UINT(GF_STREAM_VISUAL));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, & PROP_UINT(ctx->fps.num));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FPS, & PROP_FRAC(ctx->fps));

		mpgviddmx_check_dur(filter, ctx);
	}
	if ((ctx->width == ctx->dsi.width) && (ctx->height == ctx->dsi.height)) return;

	mpgviddmx_enqueue_or_dispatch(ctx, NULL, GF_TRUE);

	ctx->width = ctx->dsi.width;
	ctx->height = ctx->dsi.height;
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, & PROP_UINT( ctx->dsi.width));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, & PROP_UINT( ctx->dsi.height));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAR, & PROP_FRAC_INT(ctx->dsi.par_num, ctx->dsi.par_den));

	if (ctx->is_mpg12) {
		u32 PL = ctx->dsi.VideoPL;
		if (!PL) PL = GPAC_OTI_VIDEO_MPEG2_MAIN;
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_OTI, & PROP_UINT(PL));
	} else {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_OTI, & PROP_UINT(GPAC_OTI_VIDEO_MPEG4_PART2));
	}
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PROFILE_LEVEL, & PROP_UINT (ctx->dsi.VideoPL) );

	ctx->b_frames = 0;

	if (vosh_size) {
		u32 i;
		char * dcd = gf_malloc(sizeof(char)*vosh_size);
		memcpy(dcd, data, sizeof(char)*vosh_size);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, & PROP_DATA_NO_COPY(dcd, vosh_size));

		/*remove packed flag if any (VOSH user data)*/
		ctx->is_packed = ctx->is_vfr = ctx->forced_packed = GF_FALSE;
		i=0;
		while (1) {
			char *frame = data;
			while ((i+3<vosh_size)  && ((frame[i]!=0) || (frame[i+1]!=0) || (frame[i+2]!=1))) i++;
			if (i+4>=vosh_size) break;
			if (strncmp(frame+i+4, "DivX", 4)) {
				i += 4;
				continue;
			}
			frame = data + i + 4;
			frame = strchr(frame, 'p');
			if (frame) {
				ctx->forced_packed = GF_TRUE;
				frame[0] = 'n';
			}
			break;
		}
	}
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

static GFINLINE void mpgviddmx_update_time(GF_MPGVidDmxCtx *ctx)
{
	assert(ctx->fps.num);
	assert(ctx->fps.den);

	if (ctx->timescale) {
		u64 inc = ctx->fps.den;
		inc *= ctx->timescale;
		inc /= ctx->fps.num;
		ctx->cts += inc;
		ctx->dts += inc;
	} else {
		ctx->cts += ctx->fps.den;
		ctx->dts += ctx->fps.den;
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
			mpgviddmx_enqueue_or_dispatch(ctx, NULL, GF_TRUE);
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}

	data = (char *) gf_filter_pck_get_data(pck, &pck_size);
	byte_offset = gf_filter_pck_get_byte_offset(pck);

	start = data;
	remain = pck_size;

	//input pid sets some timescale - we flushed pending data , update cts
	if (ctx->timescale) {
		u64 ts = gf_filter_pck_get_cts(pck);
		if (ts != GF_FILTER_NO_TS)
			ctx->cts = ts;
		ts = gf_filter_pck_get_dts(pck);
		if (ts != GF_FILTER_NO_TS)
			ctx->dts = ts;
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
		char *pck_data;
		s32 current;
		u32 w, h;
		u8 sc_type, forced_sc_type;
		Bool sc_type_forced = GF_FALSE;
		Bool skip_pck = GF_FALSE;
		u8 ftype;
		u32 tinc;
		u64 size=0;
		u64 fstart;
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

			memcpy(ctx->hdr_store + ctx->bytes_in_header, start, 8 - ctx->bytes_in_header);
			current = mpgviddmx_next_start_code(ctx->hdr_store, 8);

			//no start code in stored buffer
			if (current<0) {
				dst_pck = gf_filter_pck_new_alloc(ctx->opid, ctx->bytes_in_header, &pck_data);
				memcpy(pck_data, ctx->hdr_store, ctx->bytes_in_header);
				gf_filter_pck_set_framing(dst_pck, GF_FALSE, GF_FALSE);

				if (byte_offset != GF_FILTER_NO_BO) {
					gf_filter_pck_set_byte_offset(dst_pck, byte_offset - ctx->bytes_in_header);
				}

				mpgviddmx_enqueue_or_dispatch(ctx, dst_pck, GF_FALSE);
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
		//no starcode in store, look for startcode in packet
		if (current == -1) {
			//locate next start code
			current = mpgviddmx_next_start_code(start, remain);
			//no start code, dispatch the block
			if (current<0) {
				u8 b3, b2, b1;
				if (! ctx->frame_started) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[MPGVid] no start code in block and no frame started, discarding data\n" ));
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

				dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &pck_data);
				memcpy(pck_data, start, size);
				gf_filter_pck_set_framing(dst_pck, GF_FALSE, GF_FALSE);

				if (byte_offset != GF_FILTER_NO_BO) {
					gf_filter_pck_set_byte_offset(dst_pck, byte_offset);
				}

				mpgviddmx_enqueue_or_dispatch(ctx, dst_pck, GF_FALSE);
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
			gf_filter_pck_set_framing(dst_pck, GF_FALSE, GF_TRUE);
			//bytes were partly in store, partly in packet
			if (bytes_from_store) {
				if (byte_offset != GF_FILTER_NO_BO) {
					gf_filter_pck_set_byte_offset(dst_pck, byte_offset - bytes_from_store);
				}
				assert(bytes_from_store>=current);
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

			mpgviddmx_enqueue_or_dispatch(ctx, dst_pck, GF_FALSE);
		}

		//parse headers
		w = h = 0;

		//we have a start code loaded, eg the data packet does not have a full start code at the begining
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
					if (ctx->hdr_store_alloc < ctx->hdr_store_size + pck_size - vosh_start) {
						ctx->hdr_store_alloc = ctx->hdr_store_size + pck_size - vosh_start;
						ctx->hdr_store = gf_realloc(ctx->hdr_store, sizeof(char)*ctx->hdr_store_alloc);
					}
					memcpy(ctx->hdr_store + ctx->hdr_store_size, data + vosh_start, sizeof(char)*(pck_size - vosh_start) );
					ctx->hdr_store_size += pck_size - vosh_start;
					gf_filter_pid_drop_packet(ctx->ipid);
					return GF_OK;
				} else if (e != GF_OK) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[MPGVid] Failed to parse VOS header: %s\n", gf_error_to_string(e) ));
				} else {
					mpgviddmx_check_pid(filter, ctx, 0, NULL);
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
					if (ctx->hdr_store_alloc < ctx->hdr_store_size + pck_size - vosh_start) {
						ctx->hdr_store_alloc = ctx->hdr_store_size + pck_size - vosh_start;
						ctx->hdr_store = gf_realloc(ctx->hdr_store, sizeof(char)*ctx->hdr_store_alloc);
					}
					memcpy(ctx->hdr_store + ctx->hdr_store_size, data + vosh_start, sizeof(char)*(pck_size - vosh_start) );
					ctx->hdr_store_size += pck_size - vosh_start;
					gf_filter_pid_drop_packet(ctx->ipid);
					return GF_OK;
				} else if (e != GF_OK) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[MPGVid] Failed to parse VOS header: %s\n", gf_error_to_string(e) ));
				} else {
					u32 size = gf_m4v_get_object_start(ctx->vparser);
					vosh_end = start - (u8 *)data + size;
					vosh_end -= vosh_start;
					mpgviddmx_check_pid(filter, ctx, vosh_end, data+vosh_start);
					skip_pck = GF_TRUE;
					assert(remain>=size);
					start += size;
					remain -= size;
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
				}
				break;
			}
		}

		if (skip_pck) {
			continue;
		}

		if (!ctx->is_playing) {
			ctx->resume_from = (char *)start -  (char *)data;
			return GF_OK;
		}
		//at this point, we no longer reaggregate packets
		ctx->hdr_store_size = 0;

		if (ctx->in_seek) {
			u64 nb_frames_at_seek = ctx->start_range * ctx->fps.num;
			if (ctx->cts + ctx->fps.den >= nb_frames_at_seek) {
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
		e = gf_m4v_parse_frame(ctx->vparser, ctx->dsi, &ftype, &tinc, &size, &fstart, &is_coded);
		assert(!fstart);

		//we skipped bytes already in store + end of start code present in packet, so the size of the first object
		//needs adjustement
		if (bytes_from_store) {
			size += bytes_from_store + hdr_offset;
		}

		if (e == GF_EOS) {
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

		if (!is_coded) {
			/*if prev is B and we're parsing a packed bitstream discard n-vop*/
			if (ctx->forced_packed && ctx->b_frames) {
				ctx->is_packed = GF_TRUE;
				start += size;
				remain -= size;
				continue;
			}
			/*policy is to import at variable frame rate, skip*/
			if (ctx->vfr) {
				ctx->is_vfr = GF_TRUE;
				mpgviddmx_update_time(ctx);
				start += size;
				remain -= size;
				continue;
			}
			/*policy is to keep non coded frame (constant frame rate), add*/
		}

		if (ftype==2) {
			//count number of B-frames since last ref
			ctx->b_frames++;
		} else {
			//flush all pending packets
			mpgviddmx_enqueue_or_dispatch(ctx, NULL, GF_TRUE);
			//remeber the CTS of the last ref
			ctx->last_ref_cts = ctx->cts;
			ctx->b_frames = 0;
		}

		dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &pck_data);
		//bytes come from both our store and the data packet
		if (bytes_from_store) {
			memcpy(pck_data, ctx->hdr_store+current, bytes_from_store);
			assert(size >= bytes_from_store);
			size -= bytes_from_store;
			if (byte_offset != GF_FILTER_NO_BO) {
				gf_filter_pck_set_byte_offset(dst_pck, byte_offset - bytes_from_store);
			}
			memcpy(pck_data + bytes_from_store, start, size);
		} else {
			//bytes only come the data packet
			memcpy(pck_data, start, size);
			if (byte_offset != GF_FILTER_NO_BO) {
				gf_filter_pck_set_byte_offset(dst_pck, byte_offset + start - (u8 *) data);
			}
		}
		assert(pck_data[0] == 0);
		assert(pck_data[1] == 0);
		assert(pck_data[2] == 0x01);

		gf_filter_pck_set_framing(dst_pck, GF_TRUE, full_frame);
		gf_filter_pck_set_cts(dst_pck, ctx->cts);
		gf_filter_pck_set_dts(dst_pck, ctx->dts);
		gf_filter_pck_set_sap(dst_pck, ftype ? 0 : 1);
		gf_filter_pck_set_duration(dst_pck, ctx->fps.den);
		if (ctx->in_seek) gf_filter_pck_set_seek_flag(dst_pck, GF_TRUE);
		ctx->frame_started = GF_TRUE;

		mpgviddmx_enqueue_or_dispatch(ctx, dst_pck, GF_FALSE);

		mpgviddmx_update_time(ctx);

		if (!full_frame) {
			if (copy_last_bytes) {
				memcpy(ctx->hdr_store, start+remain-3, 3);
			}
			break;
		}
		assert(remain>=size);
		start += size;
		remain -= size;


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

static GF_Err mpgviddmx_initialize(GF_Filter *filter)
{
	GF_MPGVidDmxCtx *ctx = gf_filter_get_udta(filter);
	ctx->hdr_store_size = 0;
	ctx->hdr_store_alloc = 8;
	ctx->hdr_store = gf_malloc(sizeof(char)*8);
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
	{ OFFS(fps), "import frame rate", GF_PROP_FRACTION, "25000/1000", NULL, GF_FALSE},
	{ OFFS(index_dur), "indexing window length", GF_PROP_DOUBLE, "1.0", NULL, GF_FALSE},
	{ OFFS(vfr), "set variable frame rate import", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{}
};


GF_FilterRegister MPGVidDmxRegister = {
	.name = "reframe_mpgvid",
	.description = "MPEG-1/2/4 (Part2) Video Demux",
	.private_size = sizeof(GF_MPGVidDmxCtx),
	.args = MPGVidDmxArgs,
	.initialize = mpgviddmx_initialize,
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
