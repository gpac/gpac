/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / AAC reader module
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
	Bool is_mp2, no_crc;
	u32 profile, sr_idx, nb_ch, frame_size, hdr_size;
} ADTSHeader;

typedef struct
{
	u64 pos;
	Double duration;
} ADTSIdx;

typedef struct
{
	//filter args
	u32 frame_size;
	Double index_dur;

	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;

	GF_BitStream *bs;
	u64 file_pos, cts;
	u32 sr_idx, nb_ch, is_mp2, profile;
	GF_Fraction duration;
	Double start_range;
	Bool in_seek;
	u32 timescale;

	u32 remaining;
	ADTSHeader hdr;
	char header[10];
	u32 bytes_in_header;

	Bool is_playing;
	Bool is_file, file_loaded;
	Bool initial_play_done;

	ADTSIdx *indexes;
	u32 index_alloc_size, index_size;
} GF_ADTSDmxCtx;


static Bool adts_dmx_sync_frame_bs(GF_BitStream *bs, ADTSHeader *hdr)
{
	u32 val;
	u64 pos;

	while (gf_bs_available(bs)) {
		val = gf_bs_read_u8(bs);
		if (val!=0xFF) continue;
		val = gf_bs_read_int(bs, 4);
		if (val != 0x0F) {
			gf_bs_read_int(bs, 4);
			continue;
		}
		hdr->is_mp2 = (Bool)gf_bs_read_int(bs, 1);
		gf_bs_read_int(bs, 2);
		hdr->no_crc = (Bool)gf_bs_read_int(bs, 1);
		pos = gf_bs_get_position(bs) - 2;

		hdr->profile = 1 + gf_bs_read_int(bs, 2);
		hdr->sr_idx = gf_bs_read_int(bs, 4);
		gf_bs_read_int(bs, 1);
		hdr->nb_ch = gf_bs_read_int(bs, 3);
		gf_bs_read_int(bs, 4);
		hdr->frame_size = gf_bs_read_int(bs, 13);
		gf_bs_read_int(bs, 11);
		gf_bs_read_int(bs, 2);
		hdr->hdr_size = 7;
		if (!hdr->no_crc) {
			gf_bs_read_u16(bs);
			hdr->hdr_size = 9;
		}
		if (hdr->frame_size < hdr->hdr_size) {
			gf_bs_seek(bs, pos+1);
			continue;
		}
		hdr->frame_size -= hdr->hdr_size;

		if (gf_bs_available(bs) == hdr->frame_size) {
			return GF_TRUE;
		}
		if (gf_bs_available(bs) < hdr->frame_size) {
			break;
		}

		gf_bs_skip_bytes(bs, hdr->frame_size);
		val = gf_bs_read_u8(bs);
		if (val!=0xFF) {
			gf_bs_seek(bs, pos+1);
			continue;
		}
		val = gf_bs_read_int(bs, 4);
		if (val!=0x0F) {
			gf_bs_read_int(bs, 4);
			gf_bs_seek(bs, pos+1);
			continue;
		}
		gf_bs_seek(bs, pos+hdr->hdr_size);
		return GF_TRUE;
	}
	return GF_FALSE;
}



GF_Err adts_dmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	GF_ADTSDmxCtx *ctx = gf_filter_get_udta(filter);

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
	return GF_OK;
}

static void adts_dmx_check_dur(GF_Filter *filter, GF_ADTSDmxCtx *ctx)
{
	FILE *stream;
	GF_BitStream *bs;
	ADTSHeader hdr;
	u64 duration, cur_dur;
	s32 sr_idx = -1;
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
	duration = 0;
	while (adts_dmx_sync_frame_bs(bs, &hdr)) {
		if ((sr_idx>=0) && (sr_idx != hdr.sr_idx)) {
			duration *= GF_M4ASampleRates[hdr.sr_idx];
			duration /= GF_M4ASampleRates[sr_idx];

			cur_dur *= GF_M4ASampleRates[hdr.sr_idx];
			cur_dur /= GF_M4ASampleRates[sr_idx];
		}
		sr_idx = hdr.sr_idx;
		duration += ctx->frame_size;
		cur_dur += ctx->frame_size;
		if (cur_dur > ctx->index_dur * GF_M4ASampleRates[sr_idx]) {
			if (!ctx->index_alloc_size) ctx->index_alloc_size = 10;
			else if (ctx->index_alloc_size == ctx->index_size) ctx->index_alloc_size *= 2;
			ctx->indexes = gf_realloc(ctx->indexes, sizeof(ADTSIdx)*ctx->index_alloc_size);
			ctx->indexes[ctx->index_size].pos = gf_bs_get_position(bs) - hdr.hdr_size;
			ctx->indexes[ctx->index_size].duration = duration;
			ctx->indexes[ctx->index_size].duration /= GF_M4ASampleRates[sr_idx];
			ctx->index_size ++;
			cur_dur = 0;
		}

		gf_bs_skip_bytes(bs, hdr.frame_size);
	}
	gf_bs_del(bs);
	gf_fclose(stream);

	if (!ctx->duration.num || (ctx->duration.num  * GF_M4ASampleRates[sr_idx] != duration * ctx->duration.den)) {
		ctx->duration.num = duration;
		ctx->duration.den = GF_M4ASampleRates[sr_idx];

		gf_filter_pid_set_info(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC(ctx->duration.num, ctx->duration.den));
	}

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILE_CACHED);
	if (p && p->value.boolean) ctx->file_loaded = GF_TRUE;
}

static void adts_dmx_check_pid(GF_Filter *filter, GF_ADTSDmxCtx *ctx)
{
	GF_BitStream *dsi;
	char *dsi_b;
	u32 i, sbr_sr_idx, sbr_oti, dsi_s, sr;

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, & PROP_UINT( GF_STREAM_AUDIO));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLES_PER_FRAME, & PROP_UINT(ctx->frame_size) );

		adts_dmx_check_dur(filter, ctx);
	}
	if ((ctx->sr_idx == ctx->hdr.sr_idx) && (ctx->nb_ch == ctx->hdr.nb_ch)
		&& (ctx->is_mp2 == ctx->hdr.is_mp2) && (ctx->profile == ctx->hdr.profile) ) return;


	ctx->is_mp2 = ctx->hdr.is_mp2;
	ctx->nb_ch = ctx->hdr.nb_ch;
	ctx->profile = ctx->hdr.profile;

	sr = GF_M4ASampleRates[ctx->hdr.sr_idx];
	//we change sample rate, change cts
	if (ctx->cts && (ctx->sr_idx != ctx->hdr.sr_idx)) {
		ctx->cts *= sr;
		ctx->cts /= GF_M4ASampleRates[ctx->sr_idx];
	}
	ctx->sr_idx = ctx->hdr.sr_idx;

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, & PROP_UINT(sr));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLE_RATE, & PROP_UINT(sr));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_NUM_CHANNELS, & PROP_UINT(ctx->nb_ch) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_OTI, & PROP_UINT(ctx->is_mp2 ? ctx->profile+GPAC_OTI_AUDIO_AAC_MPEG2_MP : GPAC_OTI_AUDIO_AAC_MPEG4) );

	dsi = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	/*write as regular AAC*/
	gf_bs_write_int(dsi, ctx->profile, 5);
	gf_bs_write_int(dsi, ctx->sr_idx, 4);
	gf_bs_write_int(dsi, ctx->nb_ch, 4);
	gf_bs_align(dsi);

	sbr_sr_idx = 0;
	sbr_oti = 0;
	/*always signal implicit SBR for <=24000,  in case it's used*/
	if (sr <= 24000) {
		sbr_sr_idx = ctx->sr_idx;
		for (i=0; i<16; i++) {
			if (GF_M4ASampleRates[i] == (u32) 2 * sr) {
				sbr_sr_idx = i;
				break;
			}
		}
	}
	gf_bs_write_int(dsi, 0x2b7, 11);
	gf_bs_write_int(dsi, sbr_oti, 5);
	gf_bs_write_int(dsi, 1, 1);
	gf_bs_write_int(dsi, sbr_sr_idx, 4);

	/* always signal implicit PS in case it's used ??? */
	gf_bs_write_int(dsi, 0x548, 11);
	gf_bs_write_int(dsi, 1, 1);

	gf_bs_align(dsi);
	gf_bs_get_content(dsi, &dsi_b, &dsi_s);
	gf_bs_del(dsi);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, & PROP_DATA_NO_COPY(dsi_b, dsi_s) );
}

static Bool adts_dmx_process_event(GF_Filter *filter, GF_FilterEvent *evt)
{
	u32 i;
	GF_FilterEvent fevt;
	GF_ADTSDmxCtx *ctx = gf_filter_get_udta(filter);

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if (!ctx->is_playing) {
			ctx->is_playing = GF_TRUE;
			ctx->cts = 0;
			ctx->remaining = 0;
			ctx->bytes_in_header = 0;
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
					ctx->cts = ctx->indexes[i-1].duration * GF_M4ASampleRates[ctx->sr_idx];
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
		//post a seek
		GF_FEVT_INIT(fevt, GF_FEVT_SOURCE_SEEK, ctx->ipid);
		fevt.seek.start_offset = ctx->file_pos;
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

static GFINLINE void adts_dmx_update_cts(GF_ADTSDmxCtx *ctx)
{
	if (ctx->timescale) {
		u64 inc = ctx->frame_size;
		inc *= ctx->timescale;
		inc /= GF_M4ASampleRates[ctx->sr_idx];
		ctx->cts += inc;
	} else {
		ctx->cts += ctx->frame_size;
	}
}

GF_Err adts_dmx_process(GF_Filter *filter)
{
	GF_ADTSDmxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck;
	GF_Err e;
	u64 byte_offset;
	char *data, *output;
	u8 *start;
	u64 src_cts;
	u32 pck_size, remain;
	u32 alread_sync = 0;

	//always reparse duration
	if (!ctx->duration.num)
		adts_dmx_check_dur(filter, ctx);

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			gf_filter_pid_set_eos(ctx->opid);
			assert(ctx->remaining == 0);
			return GF_EOS;
		}
		return GF_OK;
	}

	data = gf_filter_pck_get_data(pck, &pck_size);
	byte_offset = gf_filter_pck_get_byte_offset(pck);

	start = data;
	remain = pck_size;

	//flush not previously dispatched data
	if (ctx->remaining) {
		u32 to_send = ctx->remaining;
		if (ctx->remaining > pck_size) {
			to_send = pck_size;
			ctx->remaining -= pck_size;
		} else {
			ctx->remaining = 0;
		}
		if (!ctx->in_seek) {
			dst_pck = gf_filter_pck_new_alloc(ctx->opid, to_send, &output);
			memcpy(output, data, to_send);

			gf_filter_pck_set_cts(dst_pck, ctx->cts);
			gf_filter_pck_set_framing(dst_pck, GF_FALSE, ctx->remaining ? GF_FALSE : GF_TRUE);

			gf_filter_pck_send(dst_pck);
		}

		if (ctx->remaining) {
			gf_filter_pid_drop_packet(ctx->ipid);
			return GF_OK;
		}
		adts_dmx_update_cts(ctx);
		start += to_send;
		remain -= to_send;
	}

	if (ctx->bytes_in_header) {
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
	}
	//input pid sets some timescale - we flushed pending data , update cts
	else if (ctx->timescale) {
		u64 cts = gf_filter_pck_get_cts(pck);
		if (ctx != GF_FILTER_NO_TS)
			ctx->cts = ctx;
	}

	while (remain) {
		u8 *sync;
		u32 sync_pos, size, offset;

		if (alread_sync) {
			gf_bs_reassign_buffer(ctx->bs, ctx->header+1, 6);
			sync = start;
			sync_pos = 0;
		} else {
			sync = memchr(start, 0xFF, remain);
			sync_pos = sync ? sync - start : remain;

			//couldn't find sync byte in this packet
			if (!sync) {
				GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[ADTSDmx] Could not find sync word in packet, droping\n"));
				break;
			}
			if (remain - sync_pos < 7) {
				memcpy(ctx->header, sync, remain - sync_pos);
				ctx->bytes_in_header = remain - sync_pos;
				break;
			}

			//not sync !
			if ((remain - sync_pos <= 1) || (sync[1] & 0xF0 != 0xF0) ) {
				start ++;
				remain --;
				continue;
			}
			if (!ctx->bs) {
				ctx->bs = gf_bs_new(sync + 1, remain - sync_pos - 1, GF_BITSTREAM_READ);
			} else {
				gf_bs_reassign_buffer(ctx->bs, sync + 1, remain - sync_pos - 1);
			}
		}
		//ok parse header
		gf_bs_read_int(ctx->bs, 4);

		ctx->hdr.is_mp2 = (Bool)gf_bs_read_int(ctx->bs, 1);
		gf_bs_read_int(ctx->bs, 2);
		ctx->hdr.no_crc = (Bool)gf_bs_read_int(ctx->bs, 1);

		ctx->hdr.profile = 1 + gf_bs_read_int(ctx->bs, 2);
		ctx->hdr.sr_idx = gf_bs_read_int(ctx->bs, 4);
		gf_bs_read_int(ctx->bs, 1);
		ctx->hdr.nb_ch = gf_bs_read_int(ctx->bs, 3);
		gf_bs_read_int(ctx->bs, 4);
		ctx->hdr.frame_size = gf_bs_read_int(ctx->bs, 13);
		gf_bs_read_int(ctx->bs, 11);
		gf_bs_read_int(ctx->bs, 2);
		ctx->hdr.hdr_size = 7;

		if (!ctx->hdr.no_crc) {
			ctx->hdr.hdr_size = 9;
		}

		//ready to send packet
		if (ctx->hdr.frame_size < remain) {
			u32 next_frame = ctx->hdr.frame_size;
			if (alread_sync) {
				next_frame = ctx->hdr.frame_size - 7;
			}
			//make sure we are sync!
			if ((sync[next_frame] !=0xFF) || (sync[next_frame+1] & 0xF0 !=0xF0) ) {
				if (alread_sync) {
					assert(memchr(ctx->header+1, 0xFF, 8) == NULL);
					alread_sync = 0;
				} else {
					start++;
					remain--;
				}
				continue;
			}
		}

		adts_dmx_check_pid(filter, ctx);

		if (!ctx->is_playing) return GF_OK;

		size = ctx->hdr.frame_size - ctx->hdr.hdr_size;
		offset = ctx->hdr.hdr_size;
		if (alread_sync) {
			offset = ctx->hdr.no_crc ? 0 : 2;
		}
		if (size + offset > remain) {
			size = remain - offset;
			ctx->remaining = ctx->hdr.frame_size - ctx->hdr.hdr_size - size;
		}

		if (ctx->in_seek) {
			u64 nb_samples_at_seek = ctx->start_range * GF_M4ASampleRates[ctx->sr_idx];
			if (ctx->cts + ctx->frame_size >= nb_samples_at_seek) {
				u32 samples_to_discard = (ctx->cts + ctx->frame_size) - nb_samples_at_seek;
				ctx->in_seek = GF_FALSE;
			}
		}
		if (!ctx->in_seek) {

//			dst_pck = gf_filter_pck_new_ref(ctx->opid, sync + hdr.hdr_size, hdr.frame_size-hdr.hdr_size, pck);
			dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &output);

			memcpy(output, sync + offset, size);

			gf_filter_pck_set_cts(dst_pck, ctx->cts);
			gf_filter_pck_set_duration(dst_pck, ctx->frame_size);
			gf_filter_pck_set_framing(dst_pck, GF_TRUE, ctx->remaining ? GF_FALSE : GF_TRUE);

			if (byte_offset != GF_FILTER_NO_BO) {
				u64 boffset = byte_offset;
				if (alread_sync) {
					boffset -= (7-alread_sync);
				} else {
					boffset += (char *) (sync + offset) - data;
				}
				gf_filter_pck_set_byte_offset(dst_pck, boffset);
			}

			gf_filter_pck_send(dst_pck);
		}
		start += offset+size;
		remain -= offset+size;
		alread_sync = 0;
		if (ctx->remaining) break;
		adts_dmx_update_cts(ctx);
	}
	gf_filter_pid_drop_packet(ctx->ipid);

	return GF_OK;
}

static void adts_dmx_finalize(GF_Filter *filter)
{
	GF_ADTSDmxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->bs) gf_bs_del(ctx->bs);
	if (ctx->indexes) gf_free(ctx->indexes);
}

static const GF_FilterCapability ADTSDmxInputs[] =
{
	{.code=GF_PROP_PID_MIME, PROP_STRING("audio/x-m4a"), .start=GF_TRUE},
	{.code=GF_PROP_PID_MIME, PROP_STRING("audio/aac"), .start=GF_TRUE},
	{.code=GF_PROP_PID_MIME, PROP_STRING("audio/aacp"), .start=GF_TRUE},
	{.code=GF_PROP_PID_MIME, PROP_STRING("audio/x-aac"), .start=GF_TRUE},
	{.code=GF_PROP_PID_FILE_EXT, PROP_STRING("aac"), .start=GF_TRUE},
	{}
};


static const GF_FilterCapability ADTSDmxOutputs[] =
{
	{.code= GF_PROP_PID_STREAM_TYPE, PROP_UINT(GF_STREAM_AUDIO)},
	{.code= GF_PROP_PID_OTI, PROP_UINT( GPAC_OTI_AUDIO_AAC_MPEG4 )},
	
	{.code= GF_PROP_PID_STREAM_TYPE, PROP_UINT(GF_STREAM_AUDIO)},
	{.code= GF_PROP_PID_OTI, PROP_UINT( GPAC_OTI_AUDIO_AAC_MPEG2_MP )},
	
	{.code= GF_PROP_PID_STREAM_TYPE, PROP_UINT(GF_STREAM_AUDIO)},
	{.code= GF_PROP_PID_OTI, PROP_UINT( GPAC_OTI_AUDIO_AAC_MPEG2_LCP )},
	
	{.code= GF_PROP_PID_STREAM_TYPE, PROP_UINT(GF_STREAM_AUDIO)},
	{.code= GF_PROP_PID_OTI, PROP_UINT( GPAC_OTI_AUDIO_AAC_MPEG2_SSRP )},
	
	{}
};

#define OFFS(_n)	#_n, offsetof(GF_ADTSDmxCtx, _n)
static const GF_FilterArgs ADTSDmxArgs[] =
{
	{ OFFS(frame_size), "size of AAC frame in audio samples", GF_PROP_UINT, "1024", NULL, GF_FALSE},
	{ OFFS(index_dur), "indexing window length", GF_PROP_DOUBLE, "1.0", NULL, GF_FALSE},
	{}
};


GF_FilterRegister ADTSDmxRegister = {
	.name = "reframe_adts",
	.description = "ADTS Demux",
	.private_size = sizeof(GF_ADTSDmxCtx),
	.args = ADTSDmxArgs,
	.finalize = adts_dmx_finalize,
	.input_caps = ADTSDmxInputs,
	.output_caps = ADTSDmxOutputs,
	.configure_pid = adts_dmx_configure_pid,
	.process = adts_dmx_process,
	.process_event = adts_dmx_process_event
};


const GF_FilterRegister *adts_dmx_register(GF_FilterSession *session)
{
	return &ADTSDmxRegister;
}
