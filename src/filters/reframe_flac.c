/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2019
 *					All rights reserved
 *
 *  This file is part of GPAC / FLAC reframer filter
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
} FLACIdx;

typedef struct
{
	u32 block_size;
	u32 sample_rate;
} FLACHeader;

typedef struct
{
	//filter args
	Double index;

	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;

	GF_BitStream *bs;
	u64 file_pos, cts, prev_cts;

	GF_Fraction duration;
	Double start_range;
	Bool in_seek;
	u32 timescale;
	Bool is_playing;
	Bool is_file;
	Bool initial_play_done, file_loaded;

	Bool initialized;
	u32 sample_rate, nb_channels, bits_per_sample, block_size;

	u8 *flac_buffer;
	u32 flac_buffer_size, flac_buffer_alloc, resume_from;
	u64 byte_offset;

	GF_FilterPacket *src_pck;

	Bool recompute_cts;
	FLACIdx *indexes;
	u32 index_alloc_size, index_size;
} GF_FLACDmxCtx;




GF_Err flac_dmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	GF_FLACDmxCtx *ctx = gf_filter_get_udta(filter);

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

	p = gf_filter_pid_get_property_str(pid, "nocts");
	if (p && p->value.boolean) ctx->recompute_cts = GF_TRUE;
	else ctx->recompute_cts = GF_FALSE;

	if (ctx->timescale && !ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, NULL);
	}
	return GF_OK;
}

static void flac_dmx_check_dur(GF_Filter *filter, GF_FLACDmxCtx *ctx)
{
	u64 duration;
	s32 prev_sr = -1;
	const GF_PropertyValue *p;
	if (!ctx->opid || ctx->timescale || ctx->file_loaded) return;

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILEPATH);
	if (!p || !p->value.string) {
		ctx->is_file = GF_FALSE;
		return;
	}
	ctx->is_file = GF_TRUE;

	/*todo ...*/
	duration = 0;

	if (!ctx->duration.num || (ctx->duration.num  * prev_sr != duration * ctx->duration.den)) {
		ctx->duration.num = (s32) duration;
		ctx->duration.den = prev_sr ;

		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC(ctx->duration));
	}

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILE_CACHED);
	if (p && p->value.boolean) ctx->file_loaded = GF_TRUE;
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CAN_DATAREF, & PROP_BOOL(GF_TRUE ) );
}

static void flac_dmx_check_pid(GF_Filter *filter, GF_FLACDmxCtx *ctx, u8 *dsi, u32 dsi_size)
{
	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		flac_dmx_check_dur(filter, ctx);
	}
	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, & PROP_UINT( GF_STREAM_AUDIO));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, NULL );
	if (ctx->is_file && ctx->index) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PLAYBACK_MODE, & PROP_UINT(GF_PLAYBACK_MODE_FASTFORWARD) );
	}
	if (ctx->duration.num)
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC(ctx->duration));

	if (!ctx->timescale) gf_filter_pid_set_name(ctx->opid, "audio");

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, & PROP_DATA( dsi, dsi_size ) );

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, & PROP_UINT( GF_CODECID_FLAC ) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, & PROP_UINT(ctx->timescale ? ctx->timescale : ctx->sample_rate));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLE_RATE, & PROP_UINT(ctx->sample_rate));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_NUM_CHANNELS, & PROP_UINT(ctx->nb_channels) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLES_PER_FRAME, & PROP_UINT(ctx->block_size) );

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_AUDIO_BPS, & PROP_UINT(ctx->bits_per_sample) );

}

static Bool flac_dmx_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	u32 i;
	GF_FilterEvent fevt;
	GF_FLACDmxCtx *ctx = gf_filter_get_udta(filter);

	if (evt->base.on_pid != ctx->opid) return GF_TRUE;

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if (!ctx->is_playing) {
			ctx->is_playing = GF_TRUE;
		}
		if (! ctx->is_file) {
			if (evt->play.start_range || ctx->initial_play_done) {
				ctx->flac_buffer_size = 0;
				ctx->resume_from = 0;
			}
			ctx->initial_play_done = GF_TRUE;
			return GF_FALSE;
		}
		flac_dmx_check_dur(filter, ctx);

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
		ctx->flac_buffer_size = 0;
		ctx->resume_from = 0;
		//post a seek
		GF_FEVT_INIT(fevt, GF_FEVT_SOURCE_SEEK, ctx->ipid);
		fevt.seek.start_offset = ctx->file_pos;
		gf_filter_pid_send_event(ctx->ipid, &fevt);

		//cancel event
		return GF_TRUE;

	case GF_FEVT_STOP:
		ctx->is_playing = GF_FALSE;
		if (ctx->src_pck) gf_filter_pck_unref(ctx->src_pck);
		ctx->src_pck = NULL;
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

static GFINLINE void flac_dmx_update_cts(GF_FLACDmxCtx *ctx, u32 nb_samp)
{
	if (ctx->timescale) {
		u64 inc = nb_samp;
		inc *= ctx->timescale;
		inc /= ctx->sample_rate;
		ctx->cts += inc;
	} else {
		ctx->cts += nb_samp;
	}
}


u8 const flac_dmx_crc8_table[256] = {
	0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
	0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
	0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
	0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
	0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
	0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
	0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
	0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
	0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
	0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
	0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
	0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
	0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
	0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
	0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
	0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
	0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
	0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
	0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
	0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
	0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
	0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
	0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
	0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
	0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
	0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
	0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
	0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
	0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
	0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
	0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
	0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
};

u8 flac_dmx_crc8(u8 *data, u32 len)
{
	u8 crc = 0;
	while (len--)
		crc = flac_dmx_crc8_table[crc ^ *data++];
	return crc;
}

static u32 flac_dmx_block_sizes[] =
{
	0, 192, 576, 1152, 2304, 4608, 0, 0, 256, 512, 1024, 2048, 4096, 8192, 16384,  32768
};
static u32 flac_dmx_samplerates[] =
{
	0, 88200, 176400, 192000, 8000, 16000, 22050, 24000, 32000, 44100, 48000, 96000
};

static Bool flac_parse_header(GF_FLACDmxCtx *ctx, char *data, u32 size, FLACHeader *hdr)
{
	u32 block_size, sample_rate, res, top, pos, crc, crc_hdr;

	gf_bs_reassign_buffer(ctx->bs, data, size);
	gf_bs_read_int(ctx->bs, 15);
	/*block_strategy = */gf_bs_read_int(ctx->bs, 1);
	block_size = gf_bs_read_int(ctx->bs, 4);
	sample_rate = gf_bs_read_int(ctx->bs, 4);
	/*u32 channel_layout = */gf_bs_read_int(ctx->bs, 4);
	/*u32 bps = */gf_bs_read_int(ctx->bs, 3);
	gf_bs_read_int(ctx->bs, 1);

	res = gf_bs_read_u8(ctx->bs);
	top = (res & 128) >> 1;
	if ((res & 0xC0) == 0x80 || (res >= 0xFE)) return GF_FALSE;
	while (res & top) {
		s32 tmp = gf_bs_read_u8(ctx->bs);
		tmp -= 128;
		if(tmp>>6)
			return GF_FALSE;
		res = (res<<6) + tmp;
		top <<= 5;
	}
	res &= (top << 1) - 1;

	if (block_size==6) block_size = 1 + gf_bs_read_int(ctx->bs, 8);
	else if (block_size==7) block_size = 1 + gf_bs_read_int(ctx->bs, 16);
	else {
		block_size = flac_dmx_block_sizes[block_size];
	}

#if 0
	if (bps==0) bps = ctx->bits_per_sample;
	else if (bps==1) bps = 8;
	else if (bps==2) bps = 12;
	else if (bps==4) bps = 16;
	else if (bps==5) bps = 20;
	else if (bps==6) bps = 24;
#endif

	if (sample_rate==0) sample_rate = ctx->sample_rate;
	else if ((sample_rate&0xC)==0xC) {
		if (sample_rate==0xC) sample_rate = gf_bs_read_u8(ctx->bs);
		else if (sample_rate==0xD) sample_rate = gf_bs_read_u16(ctx->bs);
		else if (sample_rate==0xE) sample_rate = 10*gf_bs_read_u16(ctx->bs);
	} else {
		sample_rate = flac_dmx_samplerates[sample_rate];
	}

	pos = (u32) gf_bs_get_position(ctx->bs);

	crc = gf_bs_read_u8(ctx->bs);
	crc_hdr = flac_dmx_crc8(data, pos);

	if (crc != crc_hdr) {
		return GF_FALSE;
	}
	hdr->sample_rate = sample_rate;
	hdr->block_size = block_size;
	return GF_TRUE;
}

GF_Err flac_dmx_process(GF_Filter *filter)
{
	GF_FLACDmxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck;
	u8 *data, *output;
	u8 *start;
	Bool final_flush=GF_FALSE;
	u32 pck_size, remain, prev_pck_size;
	u64 cts = GF_FILTER_NO_TS;
	FLACHeader hdr;

	//always reparse duration
	if (!ctx->duration.num)
		flac_dmx_check_dur(filter, ctx);

	if (ctx->opid && !ctx->is_playing)
		return GF_OK;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			if (!ctx->flac_buffer_size) {
				gf_filter_pid_set_eos(ctx->opid);
				if (ctx->src_pck) gf_filter_pck_unref(ctx->src_pck);
				ctx->src_pck = NULL;
				return GF_EOS;
			}
			final_flush = GF_TRUE;
		} else {
			return GF_OK;
		}
	}

	prev_pck_size = ctx->flac_buffer_size;
	if (pck && !ctx->resume_from) {
		data = (char *) gf_filter_pck_get_data(pck, &pck_size);

		if (ctx->byte_offset != GF_FILTER_NO_BO) {
			u64 byte_offset = gf_filter_pck_get_byte_offset(pck);
			if (!ctx->flac_buffer_size) {
				ctx->byte_offset = byte_offset;
			} else if (ctx->byte_offset + ctx->flac_buffer_size != byte_offset) {
				ctx->byte_offset = GF_FILTER_NO_BO;
				if ((byte_offset != GF_FILTER_NO_BO) && (byte_offset>ctx->flac_buffer_size) ) {
					ctx->byte_offset = byte_offset - ctx->flac_buffer_size;
				}
			}
		}

		if (ctx->flac_buffer_size + pck_size > ctx->flac_buffer_alloc) {
			ctx->flac_buffer_alloc = ctx->flac_buffer_size + pck_size;
			ctx->flac_buffer = gf_realloc(ctx->flac_buffer, ctx->flac_buffer_alloc);
		}
		memcpy(ctx->flac_buffer + ctx->flac_buffer_size, data, pck_size);
		ctx->flac_buffer_size += pck_size;
	}

	//input pid sets some timescale - we flushed pending data , update cts
	if (ctx->timescale && pck) {
		cts = gf_filter_pck_get_cts(pck);
	}

	if (cts == GF_FILTER_NO_TS) {
		//avoids updating cts
		prev_pck_size = 0;
	}

	remain = ctx->flac_buffer_size;
	start = ctx->flac_buffer;

	if (ctx->resume_from) {
		start += ctx->resume_from - 1;
		remain -= ctx->resume_from - 1;
		ctx->resume_from = 0;
	}

	while (remain>2) {
		u32 next_frame=0, nb_samp;
		u32 cur_size = remain-2;
		u8 *cur_buf = start+2;
		u8 *hdr_start = NULL;

		if (final_flush) {
			next_frame = remain;
		} else {
			while (cur_size) {
				//wait till we have a frame header
				hdr_start = memchr(cur_buf, 0xFF, cur_size);
				if (!hdr_start) break;
				next_frame = (u32) (hdr_start-start);
				if (next_frame == remain)
					break;

				if ((hdr_start[1]&0xFC) == 0xF8) {
					if (flac_parse_header(ctx, hdr_start, (u32) remain - next_frame, &hdr))
						break;
				}
				cur_buf = hdr_start+1;
				cur_size = (u32) (cur_buf - start);
				assert(cur_size<=remain);
				cur_size = remain - cur_size;
				hdr_start = NULL;
			}
			if (!hdr_start) break;
			if (next_frame == remain)
				break;
		}


		if (!ctx->initialized) {
			u32 size = next_frame;
			u32 dsi_end = 0;
			//we have a header
			gf_bs_reassign_buffer(ctx->bs, ctx->flac_buffer, size);
			u32 magic = gf_bs_read_u32(ctx->bs);
			if (magic != GF_4CC('f','L','a','C')) {

			}
			while (gf_bs_available(ctx->bs)) {
				Bool last = gf_bs_read_int(ctx->bs, 1);
				u32 type = gf_bs_read_int(ctx->bs, 7);
				u32 len = gf_bs_read_int(ctx->bs, 24);

				if (type==0) {
					u16 min_block_size = gf_bs_read_u16(ctx->bs);
					u16 max_block_size = gf_bs_read_u16(ctx->bs);
					/*u32 min_frame_size = */gf_bs_read_u24(ctx->bs);
					/*u32 max_frame_size = */gf_bs_read_u24(ctx->bs);
					ctx->sample_rate = gf_bs_read_int(ctx->bs, 20);
					ctx->nb_channels = 1 + gf_bs_read_int(ctx->bs, 3);
					ctx->bits_per_sample = 1 + gf_bs_read_int(ctx->bs, 5);
					if (min_block_size==max_block_size) ctx->block_size = min_block_size;
					else ctx->block_size = 0;

					//ignore the rest
					/*total samples*/gf_bs_read_long_int(ctx->bs, 36);
					gf_bs_skip_bytes(ctx->bs, 16);
					dsi_end = (u32) gf_bs_get_position(ctx->bs);

				} else {
					//ignore the rest for now
					//TODO: expose metadata, pictures and co
					gf_bs_skip_bytes(ctx->bs, len);
				}
				if (last) break;
			}
			flac_dmx_check_pid(filter, ctx, ctx->flac_buffer+4, dsi_end-4);
			remain -= size;
			start += size;
			ctx->initialized = GF_TRUE;
			if (!ctx->is_playing) break;
			continue;
		}

		//we have a next frame, check we are synchronize
		if ((start[0] != 0xFF) && ((start[1]&0xFC) != 0xF8)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[FLACDmx] invalid frame, droping %d bytes and resyncing\n", next_frame));
			start += next_frame;
			remain -= next_frame;
			continue;
		}

		flac_parse_header(ctx,start, next_frame, &hdr);
		if (hdr.sample_rate != ctx->sample_rate) {
			ctx->sample_rate = hdr.sample_rate;
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLE_RATE, & PROP_UINT(ctx->sample_rate));
		}

		nb_samp = hdr.block_size;

		if (ctx->in_seek) {
			u64 nb_samples_at_seek = (u64) (ctx->start_range * ctx->sample_rate);
			if (ctx->cts + nb_samp >= nb_samples_at_seek) {
				//u32 samples_to_discard = (ctx->cts + nb_samp ) - nb_samples_at_seek;
				ctx->in_seek = GF_FALSE;
			}
		}

		if (ctx->timescale && !prev_pck_size && (cts != GF_FILTER_NO_TS) ) {
			ctx->cts = cts;
			cts = GF_FILTER_NO_TS;
		}

		if (!ctx->in_seek) {
			dst_pck = gf_filter_pck_new_alloc(ctx->opid, next_frame, &output);
			memcpy(output, start, next_frame);

			gf_filter_pck_set_cts(dst_pck, ctx->cts);
			if (!ctx->timescale || (ctx->timescale==ctx->sample_rate) )
				gf_filter_pck_set_duration(dst_pck, nb_samp);
			else {
				gf_filter_pck_set_duration(dst_pck, (nb_samp * ctx->timescale) / ctx->sample_rate);
			}
			gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1);
			gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_TRUE);

			if (ctx->byte_offset != GF_FILTER_NO_BO) {
				gf_filter_pck_set_byte_offset(dst_pck, ctx->byte_offset);
			}
			gf_filter_pck_send(dst_pck);
		}
		flac_dmx_update_cts(ctx, nb_samp);

		assert (start[0] == 0xFF);
		assert((start[1]&0xFC) == 0xF8);

		start += next_frame;
		assert(remain >= next_frame);
		remain -= next_frame;

	}

	if (!pck) {
		ctx->flac_buffer_size = 0;
		return flac_dmx_process(filter);
	} else {
		if (remain < ctx->flac_buffer_size) {
			memmove(ctx->flac_buffer, start, remain);
		}
		ctx->flac_buffer_size = remain;
		gf_filter_pid_drop_packet(ctx->ipid);
	}
	return GF_OK;
}

static GF_Err flac_dmx_initialize(GF_Filter *filter)
{
	GF_FLACDmxCtx *ctx = gf_filter_get_udta(filter);
	ctx->bs = gf_bs_new((u8 *)ctx, 1, GF_BITSTREAM_READ);
	return GF_OK;
}
static void flac_dmx_finalize(GF_Filter *filter)
{
	GF_FLACDmxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->bs) gf_bs_del(ctx->bs);
	if (ctx->indexes) gf_free(ctx->indexes);
	if (ctx->flac_buffer) gf_free(ctx->flac_buffer);
	if (ctx->src_pck) gf_filter_pck_unref(ctx->src_pck);
}


static const char *flac_dmx_probe_data(const u8 *data, u32 size, GF_FilterProbeScore *score)
{
	if ((size>4) && !strncmp(data, "fLaC", 4)) {
		*score = GF_FPROBE_SUPPORTED;
		return "audio/flac";
	}
	return NULL;
}

static const GF_FilterCapability FLACDmxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "audio/flac"),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_FLAC),
	CAP_BOOL(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "flac"),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_FLAC),
	CAP_BOOL(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_FLAC),
	CAP_BOOL(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
};



#define OFFS(_n)	#_n, offsetof(GF_FLACDmxCtx, _n)
static const GF_FilterArgs FLACDmxArgs[] =
{
	{ OFFS(index), "indexing window length", GF_PROP_DOUBLE, "1.0", NULL, 0},
	{0}
};


GF_FilterRegister FLACDmxRegister = {
	.name = "rfflac",
	GF_FS_SET_DESCRIPTION("FLAC reframer")
	GF_FS_SET_HELP("This filter parses FLAC files/data and outputs corresponding audio PID and frames.")
	.private_size = sizeof(GF_FLACDmxCtx),
	.args = FLACDmxArgs,
	.finalize = flac_dmx_finalize,
	.initialize = flac_dmx_initialize,
	SETCAPS(FLACDmxCaps),
	.configure_pid = flac_dmx_configure_pid,
	.process = flac_dmx_process,
	.probe_data = flac_dmx_probe_data,
	.process_event = flac_dmx_process_event
};


const GF_FilterRegister *flac_dmx_register(GF_FilterSession *session)
{
	return &FLACDmxRegister;
}
