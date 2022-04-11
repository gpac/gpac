/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2022
 *					All rights reserved
 *
 *  This file is part of GPAC / TrueHD reframer filter
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
} TrueHDIdx;

typedef struct
{
	u32 frame_size;
	u32 time;
	u32 sync;
	u32 format;

	u32 sample_rate;
	Bool mc_6_ch, mc_8_ch;
	u8 ch_2_modif, ch_6_modif, ch_8_modif, ch_6_assign;
	u16 ch_8_assign;
	u16 peak_rate;
} TrueHDHdr;

typedef struct
{
	//filter args
	Double index;

	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;

	GF_BitStream *bs;
	u64 file_pos, cts;
	u32 sample_rate, nb_ch, format;
	GF_Fraction64 duration;
	Double start_range;
	Bool in_seek;
	u32 timescale, frame_dur;

	u8 *truehd_buffer;
	u32 truehd_buffer_size, truehd_buffer_alloc, resume_from;
	u64 byte_offset;

	Bool is_playing;
	Bool is_file, file_loaded;
	Bool initial_play_done;

	GF_FilterPacket *src_pck;

	TrueHDIdx *indexes;
	u32 index_alloc_size, index_size;
	Bool copy_props;
} GF_TrueHDDmxCtx;




GF_Err truehd_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	GF_TrueHDDmxCtx *ctx = gf_filter_get_udta(filter);

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
	if (ctx->timescale) ctx->copy_props = GF_TRUE;
	return GF_OK;
}

static GF_Err truehd_parse_frame(GF_BitStream *bs, TrueHDHdr *hdr)
{
	memset(hdr, 0, sizeof(TrueHDHdr));

	u32 avail = (u32) gf_bs_available(bs);
	//we need 8 bytes for base header (up to sync marker)
	if (avail<8) {
		hdr->frame_size = 0;
		return GF_OK;
	}

	/*u8 nibble = */gf_bs_read_int(bs, 4);
	hdr->frame_size = 2 * gf_bs_read_int(bs, 12);
	hdr->time = gf_bs_read_u16(bs);
	hdr->sync = gf_bs_read_u32(bs);
	if (hdr->sync != 0xF8726FBA) {
		hdr->sync = 0;
		return GF_OK;
	}
	avail-=8;
	//we need 12 bytes until peak rate - to update if we decide to parse more
	if (avail < 12) {
		hdr->frame_size = 0;
		return GF_OK;
	}

	hdr->format = gf_bs_peek_bits(bs, 32, 0);
	u8 sr_idx = gf_bs_read_int(bs, 4);
	switch (sr_idx) {
	case 0: hdr->sample_rate = 48000; break;
	case 1: hdr->sample_rate = 96000; break;
	case 2: hdr->sample_rate = 192000; break;
	case 8: hdr->sample_rate = 44100; break;
	case 9: hdr->sample_rate = 88200; break;
	case 10: hdr->sample_rate = 176400; break;
	default:
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	hdr->mc_6_ch = gf_bs_read_int(bs, 1);
	hdr->mc_8_ch = gf_bs_read_int(bs, 1);
	gf_bs_read_int(bs, 2);
	hdr->ch_2_modif = gf_bs_read_int(bs, 2);
	hdr->ch_6_modif = gf_bs_read_int(bs, 2);
	hdr->ch_6_assign = gf_bs_read_int(bs, 5);
	hdr->ch_8_modif = gf_bs_read_int(bs, 2);
	hdr->ch_8_assign = gf_bs_read_int(bs, 13);

	u16 sig = gf_bs_read_u16(bs);
	if (sig != 0xB752) {
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	gf_bs_read_u16(bs);
	gf_bs_read_u16(bs);
	gf_bs_read_int(bs, 1);
	hdr->peak_rate = gf_bs_read_int(bs, 15);

	return GF_OK;
}

static u32 truehd_frame_dur(u32 sample_rate)
{

	switch (sample_rate) {
	case 48000:
	case 96000:
	case 192000:
		return sample_rate / 1200;
	case 44100:
	case 88200:
	case 176400:
		return sample_rate * 2 / 2205;
	default:
		return 0;
	}
}
static void truehd_check_dur(GF_Filter *filter, GF_TrueHDDmxCtx *ctx)
{
	FILE *stream;
	GF_BitStream *bs;
	u64 duration, cur_dur;
	s32 sr = -1;
	u32 frame_dur = 0;
	const GF_PropertyValue *p;
	if (!ctx->opid || ctx->timescale || ctx->file_loaded) return;

	if (ctx->index<=0) {
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
	duration = 0;
	cur_dur = 0;
	while (	gf_bs_available(bs) > 8 ) {
		TrueHDHdr hdr;
		u64 pos = gf_bs_get_position(bs);
		GF_Err e = truehd_parse_frame(bs, &hdr);
		if (e) break;

		if (hdr.sync) {
			if ((sr>=0) && (sr != hdr.sample_rate)) {
				duration *= hdr.sample_rate;
				duration /= sr;

				cur_dur *= hdr.sample_rate;
				cur_dur /= sr;
			}
			sr = hdr.sample_rate;
			frame_dur = truehd_frame_dur(hdr.sample_rate);
		}

		duration += frame_dur;
		cur_dur += frame_dur;

		if (hdr.sync && (cur_dur > ctx->index * sr)) {
			if (!ctx->index_alloc_size) ctx->index_alloc_size = 10;
			else if (ctx->index_alloc_size == ctx->index_size) ctx->index_alloc_size *= 2;
			ctx->indexes = gf_realloc(ctx->indexes, sizeof(TrueHDIdx)*ctx->index_alloc_size);
			ctx->indexes[ctx->index_size].pos = gf_bs_get_position(bs);
			ctx->indexes[ctx->index_size].duration = (Double) duration;
			ctx->indexes[ctx->index_size].duration /= sr;
			ctx->index_size ++;
			cur_dur = 0;
		}

		if (!hdr.frame_size) break;
		gf_bs_seek(bs, pos + hdr.frame_size);
	}
	gf_bs_del(bs);
	gf_fclose(stream);

	if (!ctx->duration.num || (ctx->duration.num  * sr != duration * ctx->duration.den)) {
		ctx->duration.num = (s32) duration;
		ctx->duration.den = sr;

		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC64(ctx->duration));
	}

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILE_CACHED);
	if (p && p->value.boolean) ctx->file_loaded = GF_TRUE;
}

static void truehd_check_pid(GF_Filter *filter, GF_TrueHDDmxCtx *ctx, TrueHDHdr *hdr)
{
	u8 *data;
	u32 size, max_rate;
	GF_BitStream *bs;
	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		truehd_check_dur(filter, ctx);
	}
	if ((ctx->sample_rate == hdr->sample_rate) && (ctx->format == hdr->format) && !ctx->copy_props)
		return;

	ctx->frame_dur = truehd_frame_dur(hdr->sample_rate);
	ctx->copy_props = GF_FALSE;

	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, & PROP_UINT( GF_STREAM_AUDIO));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLES_PER_FRAME, & PROP_UINT(ctx->frame_dur) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, & PROP_BOOL(GF_FALSE) );

	if (ctx->duration.num)
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC64(ctx->duration));
	if (!ctx->timescale)
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CAN_DATAREF, & PROP_BOOL(GF_TRUE ) );

	if (hdr->ch_2_modif==1) {
		ctx->nb_ch = 1;
	} else {
		ctx->nb_ch = 2;
		if (hdr->ch_6_assign) {
			ctx->nb_ch = 0;
			if (hdr->ch_6_assign & 1) ctx->nb_ch += 2;
			if (hdr->ch_6_assign & 1<<1) ctx->nb_ch += 1;
			if (hdr->ch_6_assign & 1<<2) ctx->nb_ch += 1;
			if (hdr->ch_6_assign & 1<<3) ctx->nb_ch += 2;
			if (hdr->ch_6_assign & 1<<4) ctx->nb_ch += 2;
		}
		if (hdr->ch_8_assign) {
			ctx->nb_ch = 0;
			if (hdr->ch_8_assign & 1) ctx->nb_ch += 2;
			if (hdr->ch_8_assign & 1<<1) ctx->nb_ch += 1;
			if (hdr->ch_8_assign & 1<<2) ctx->nb_ch += 1;
			if (hdr->ch_8_assign & 1<<3) ctx->nb_ch += 2;
			if (hdr->ch_8_assign & 1<<4) ctx->nb_ch += 2;
			if (hdr->ch_8_assign & 1<<5) ctx->nb_ch += 2;
			if (hdr->ch_8_assign & 1<<6) ctx->nb_ch += 2;
			if (hdr->ch_8_assign & 1<<7) ctx->nb_ch += 1;
			if (hdr->ch_8_assign & 1<<8) ctx->nb_ch += 1;
			if (hdr->ch_8_assign & 1<<9) ctx->nb_ch += 2;
			if (hdr->ch_8_assign & 1<<10) ctx->nb_ch += 2;
			if (hdr->ch_8_assign & 1<<11) ctx->nb_ch += 1;
			if (hdr->ch_8_assign & 1<<12) ctx->nb_ch += 1;
		}
	}

	if (!ctx->timescale) {
		//we change sample rate, change cts
		if (ctx->cts && (ctx->sample_rate != hdr->sample_rate)) {
			ctx->cts = gf_timestamp_rescale(ctx->cts, ctx->sample_rate, hdr->sample_rate);
		}
	}
	ctx->sample_rate = hdr->sample_rate;
	ctx->format = hdr->format;

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, & PROP_UINT(ctx->timescale ? ctx->timescale : ctx->sample_rate));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLE_RATE, & PROP_UINT(ctx->sample_rate));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_NUM_CHANNELS, & PROP_UINT(ctx->nb_ch) );

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, & PROP_UINT(GF_CODECID_TRUEHD) );

	max_rate = hdr->peak_rate;
	max_rate *= hdr->sample_rate;
	max_rate /= 16;
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_BITRATE, & PROP_UINT(max_rate) );

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_bs_write_u32(bs, hdr->format);
	gf_bs_write_int(bs, hdr->peak_rate, 15);
	gf_bs_write_int(bs, 0, 1);
	gf_bs_write_u32(bs, 0);
	gf_bs_get_content(bs, &data, &size);
	gf_bs_del(bs);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, & PROP_DATA_NO_COPY(data, size) );

	if (ctx->is_file && ctx->index) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PLAYBACK_MODE, & PROP_UINT(GF_PLAYBACK_MODE_FASTFORWARD) );
	}
}

static Bool truehd_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	u32 i;
	GF_FilterEvent fevt;
	GF_TrueHDDmxCtx *ctx = gf_filter_get_udta(filter);

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if (!ctx->is_playing) {
			ctx->is_playing = GF_TRUE;
			ctx->cts = 0;
			ctx->truehd_buffer_size = 0;
			ctx->resume_from = 0;
		}
		if (! ctx->is_file) {
			return GF_FALSE;
		}
		ctx->start_range = evt->play.start_range;
		ctx->in_seek = GF_TRUE;
		ctx->file_pos = 0;
		if (ctx->start_range) {
			for (i=1; i<ctx->index_size; i++) {
				if (ctx->indexes[i].duration>ctx->start_range) {
					ctx->cts = (u64) (ctx->indexes[i-1].duration * ctx->sample_rate);
					ctx->file_pos = ctx->indexes[i-1].pos;
					break;
				}
			}
		}
		if (!ctx->initial_play_done) {
			ctx->initial_play_done = GF_TRUE;
			//seek will not change the current source state, don't send a seek
			if (!ctx->file_pos)
				return GF_TRUE;
		}
		ctx->truehd_buffer_size = 0;
		ctx->resume_from = 0;
		//post a seek
		GF_FEVT_INIT(fevt, GF_FEVT_SOURCE_SEEK, ctx->ipid);
		fevt.seek.start_offset = ctx->file_pos;
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

static GFINLINE void truehd_update_cts(GF_TrueHDDmxCtx *ctx, TrueHDHdr *hdr)
{
	if (ctx->timescale) {
		u64 inc = ctx->frame_dur;
		inc *= ctx->timescale;
		inc /= ctx->sample_rate;
		ctx->cts += inc;
	} else {
		ctx->cts += ctx->frame_dur;
	}
}

GF_Err truehd_process(GF_Filter *filter)
{
	GF_TrueHDDmxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck;
	u8 *output;
	u8 *start;
	GF_Err e = GF_OK;
	u32 pck_size, remain, prev_pck_size;
	u64 cts = GF_FILTER_NO_TS;

	//always reparse duration
	if (!ctx->duration.num)
		truehd_check_dur(filter, ctx);

	if (ctx->opid && !ctx->is_playing)
		return GF_OK;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			if (!ctx->truehd_buffer_size) {
				if (ctx->opid)
					gf_filter_pid_set_eos(ctx->opid);
				if (ctx->src_pck) gf_filter_pck_unref(ctx->src_pck);
				ctx->src_pck = NULL;
				return GF_EOS;
			}
		} else {
			return GF_OK;
		}
	}

	prev_pck_size = ctx->truehd_buffer_size;
	if (pck && !ctx->resume_from) {
		const u8 *data = gf_filter_pck_get_data(pck, &pck_size);
		if (!pck_size) {
			gf_filter_pid_drop_packet(ctx->ipid);
			return GF_OK;
		}

		if (ctx->byte_offset != GF_FILTER_NO_BO) {
			u64 byte_offset = gf_filter_pck_get_byte_offset(pck);
			if (!ctx->truehd_buffer_size) {
				ctx->byte_offset = byte_offset;
			} else if (ctx->byte_offset + ctx->truehd_buffer_size != byte_offset) {
				ctx->byte_offset = GF_FILTER_NO_BO;
				if ((byte_offset != GF_FILTER_NO_BO) && (byte_offset>ctx->truehd_buffer_size) ) {
					ctx->byte_offset = byte_offset - ctx->truehd_buffer_size;
				}
			}
		}

		if (ctx->truehd_buffer_size + pck_size > ctx->truehd_buffer_alloc) {
			ctx->truehd_buffer_alloc = ctx->truehd_buffer_size + pck_size;
			ctx->truehd_buffer = gf_realloc(ctx->truehd_buffer, ctx->truehd_buffer_alloc);
			if (!ctx->truehd_buffer) return GF_OUT_OF_MEM;
		}
		memcpy(ctx->truehd_buffer + ctx->truehd_buffer_size, data, pck_size);
		ctx->truehd_buffer_size += pck_size;
	}

	//input pid sets some timescale - we flushed pending data , update cts
	if (ctx->timescale && pck) {
		cts = gf_filter_pck_get_cts(pck);
		//init cts at first packet
		if (!ctx->cts && (cts != GF_FILTER_NO_TS))
			ctx->cts = cts;
	}

	if (cts == GF_FILTER_NO_TS) {
		//avoids updating cts
		prev_pck_size = 0;
	}

	remain = ctx->truehd_buffer_size;
	start = ctx->truehd_buffer;

	if (ctx->resume_from) {
		start += ctx->resume_from - 1;
		remain -= ctx->resume_from - 1;
		ctx->resume_from = 0;
	}

	if (!ctx->bs) {
		ctx->bs = gf_bs_new(start, remain, GF_BITSTREAM_READ);
	} else {
		gf_bs_reassign_buffer(ctx->bs, start, remain);
	}
	while (remain > 8) {
		u8 *frame;
		TrueHDHdr hdr;
		u32 frame_start, bytes_to_drop=0;

		frame_start = (u32) gf_bs_get_position(ctx->bs);
		e = truehd_parse_frame(ctx->bs, &hdr);
		if (e) {
			remain = 0;
			break;
		}
		if (!hdr.frame_size) {
			remain = 0;
			break;
		}

		//frame not complete, wait
		if (remain < frame_start + hdr.frame_size)
			break;

		if (hdr.sync)
			truehd_check_pid(filter, ctx, &hdr);

		if (!ctx->is_playing) {
			ctx->resume_from = 1 + ctx->truehd_buffer_size - remain;
			return GF_OK;
		}

		frame = start + frame_start;

		if (ctx->in_seek) {
			u64 nb_samples_at_seek = (u64) (ctx->start_range * hdr.sample_rate);
			if (ctx->cts + ctx->frame_dur >= nb_samples_at_seek) {
				//u32 samples_to_discard = (ctx->cts + ctx->dts_inc) - nb_samples_at_seek;
				ctx->in_seek = GF_FALSE;
			}
		}

		bytes_to_drop = hdr.frame_size;
		if (ctx->timescale && !prev_pck_size &&  (cts != GF_FILTER_NO_TS) ) {
			//trust input CTS if diff is more than one sec
			if ((cts > ctx->cts + ctx->timescale) || (ctx->cts > cts + ctx->timescale))
				ctx->cts = cts;
			cts = GF_FILTER_NO_TS;
		}

		if (!ctx->in_seek) {
			dst_pck = gf_filter_pck_new_alloc(ctx->opid, hdr.frame_size, &output);
			if (!dst_pck) return GF_OUT_OF_MEM;
			if (ctx->src_pck) gf_filter_pck_merge_properties(ctx->src_pck, dst_pck);

			memcpy(output, frame, hdr.frame_size);
			gf_filter_pck_set_dts(dst_pck, ctx->cts);
			gf_filter_pck_set_cts(dst_pck, ctx->cts);
			if (hdr.sync)
				gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1);
			gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_TRUE);

			if (ctx->byte_offset != GF_FILTER_NO_BO) {
				gf_filter_pck_set_byte_offset(dst_pck, ctx->byte_offset + hdr.frame_size);
			}

			gf_filter_pck_send(dst_pck);
		}
		truehd_update_cts(ctx, &hdr);

		//truncated last frame
		if (bytes_to_drop > remain) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[TrueHDDmx] truncated TrueHD frame!\n"));
			bytes_to_drop = remain;
		}

		if (!bytes_to_drop) {
			bytes_to_drop = 1;
		}
		start += bytes_to_drop;
		remain -= bytes_to_drop;
		gf_bs_reassign_buffer(ctx->bs, start, remain);

		if (prev_pck_size) {
			if (prev_pck_size > bytes_to_drop) prev_pck_size -= bytes_to_drop;
			else {
				prev_pck_size=0;
				if (ctx->src_pck) gf_filter_pck_unref(ctx->src_pck);
				ctx->src_pck = pck;
				if (pck)
					gf_filter_pck_ref_props(&ctx->src_pck);
			}
		}
		if (ctx->byte_offset != GF_FILTER_NO_BO)
			ctx->byte_offset += bytes_to_drop;
	}

	if (!pck) {
		ctx->truehd_buffer_size = 0;
		return truehd_process(filter);
	} else {
		if (remain) {
			memmove(ctx->truehd_buffer, start, remain);
		}
		ctx->truehd_buffer_size = remain;
		gf_filter_pid_drop_packet(ctx->ipid);
	}
	return e;
}

static void truehd_finalize(GF_Filter *filter)
{
	GF_TrueHDDmxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->bs) gf_bs_del(ctx->bs);
	if (ctx->truehd_buffer) gf_free(ctx->truehd_buffer);
	if (ctx->indexes) gf_free(ctx->indexes);
}

static const char *truehd_probe_data(const u8 *data, u32 size, GF_FilterProbeScore *score)
{
	u32 nb_frames=0;
	GF_BitStream *bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
	while (gf_bs_available(bs) > 8 ) {
		TrueHDHdr hdr;
		u64 pos = gf_bs_get_position(bs);
		GF_Err e = truehd_parse_frame(bs, &hdr);
		if (e || !hdr.frame_size) {
			nb_frames = 0;
			break;
		}
		if (hdr.sync) nb_frames++;
		gf_bs_seek(bs, pos + hdr.frame_size);
	}
	gf_bs_del(bs);
	if (nb_frames) {
		*score = GF_FPROBE_MAYBE_SUPPORTED;
		return "audio/truehd";
	}
	return NULL;
}

static const GF_FilterCapability TrueHDDmxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "mlp|thd|truehd"),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "audio/truehd|audio/x-truehd"),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_TRUEHD),
	CAP_BOOL(GF_CAPS_OUTPUT_STATIC_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_TRUEHD),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
};

#define OFFS(_n)	#_n, offsetof(GF_TrueHDDmxCtx, _n)
static const GF_FilterArgs TrueHDDmxArgs[] =
{
	{ OFFS(index), "indexing window length", GF_PROP_DOUBLE, "1.0", NULL, 0},
	{0}
};


GF_FilterRegister TrueHDDmxRegister = {
	.name = "rftruehd",
	GF_FS_SET_DESCRIPTION("TrueHD reframer")
	GF_FS_SET_HELP("This filter parses Dolby TrueHD files/data and outputs corresponding audio PID and frames.")
	.private_size = sizeof(GF_TrueHDDmxCtx),
	.args = TrueHDDmxArgs,
	.finalize = truehd_finalize,
	SETCAPS(TrueHDDmxCaps),
	.configure_pid = truehd_configure_pid,
	.process = truehd_process,
	.probe_data = truehd_probe_data,
	.process_event = truehd_process_event
};


const GF_FilterRegister *truehd_register(GF_FilterSession *session)
{
	return &TrueHDDmxRegister;
}

