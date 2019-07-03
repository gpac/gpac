/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2017
 *					All rights reserved
 *
 *  This file is part of GPAC / AAC ADTS reframer filter
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

enum
{
	AAC_SIGNAL_NONE=0,
	AAC_SIGNAL_IMPLICIT,
	AAC_SIGNAL_EXPLICIT
};

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
	Double index;
	u32 sbr;
	u32 ps;
	Bool mpeg4;
	Bool ovsbr;

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

	ADTSHeader hdr;
	u32 dts_inc;

	Bool is_playing;
	Bool is_file, file_loaded;
	Bool initial_play_done;

	GF_FilterPacket *src_pck;

	ADTSIdx *indexes;
	u32 index_alloc_size, index_size;

	u8 *adts_buffer;
	u32 adts_buffer_size, adts_buffer_alloc, resume_from;
	u64 byte_offset;
} GF_ADTSDmxCtx;


static Bool adts_dmx_sync_frame_bs(GF_BitStream *bs, ADTSHeader *hdr)
{
	u32 val;
	u64 pos;

	while (gf_bs_available(bs)>7) {
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
		if (!GF_M4ASampleRates[hdr->sr_idx] || (hdr->frame_size < hdr->hdr_size)) {
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

	if (ctx->timescale && !ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, NULL);
		//we don't update copy props on output for now - if we decide we need it, we will need to also force resengin the decoder config
	}

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

	stream = gf_fopen(p->value.string, "rb");
	if (!stream) return;

	ctx->index_size = 0;

	bs = gf_bs_from_file(stream, GF_BITSTREAM_READ);
	duration = 0;
	cur_dur = 0;
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
		if (cur_dur > ctx->index * GF_M4ASampleRates[sr_idx]) {
			if (!ctx->index_alloc_size) ctx->index_alloc_size = 10;
			else if (ctx->index_alloc_size == ctx->index_size) ctx->index_alloc_size *= 2;
			ctx->indexes = gf_realloc(ctx->indexes, sizeof(ADTSIdx)*ctx->index_alloc_size);
			ctx->indexes[ctx->index_size].pos = gf_bs_get_position(bs) - hdr.hdr_size;
			ctx->indexes[ctx->index_size].duration = (Double) duration;
			ctx->indexes[ctx->index_size].duration /= GF_M4ASampleRates[sr_idx];
			ctx->index_size ++;
			cur_dur = 0;
		}

		gf_bs_skip_bytes(bs, hdr.frame_size);
	}
	gf_bs_del(bs);
	gf_fclose(stream);

	if (!ctx->duration.num || (ctx->duration.num  * GF_M4ASampleRates[sr_idx] != duration * ctx->duration.den)) {
		ctx->duration.num = (s32) duration;
		ctx->duration.den = GF_M4ASampleRates[sr_idx];

		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC(ctx->duration));
	}

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILE_CACHED);
	if (p && p->value.boolean) ctx->file_loaded = GF_TRUE;
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CAN_DATAREF, & PROP_BOOL(GF_TRUE ) );
}

static void adts_dmx_check_pid(GF_Filter *filter, GF_ADTSDmxCtx *ctx)
{
	GF_BitStream *dsi;
	GF_M4ADecSpecInfo acfg;
	u8 *dsi_b;
	u32 i, sbr_sr_idx, dsi_s, sr, sbr_sr, codecid, timescale=0;

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
		adts_dmx_check_dur(filter, ctx);
	}

	if ((ctx->sr_idx == ctx->hdr.sr_idx) && (ctx->nb_ch == ctx->hdr.nb_ch)
		&& (ctx->is_mp2 == ctx->hdr.is_mp2) && (ctx->profile == ctx->hdr.profile) ) return;

	//copy properties at init or reconfig
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, & PROP_UINT( GF_STREAM_AUDIO));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, & PROP_UINT( GF_CODECID_AAC_MPEG4));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLES_PER_FRAME, & PROP_UINT(ctx->frame_size) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, & PROP_BOOL(GF_FALSE) );
	if (ctx->is_file && ctx->index) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PLAYBACK_MODE, & PROP_UINT(GF_PLAYBACK_MODE_FASTFORWARD) );
	}

	if (ctx->duration.num)
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC(ctx->duration));


	ctx->is_mp2 = ctx->hdr.is_mp2;
	ctx->nb_ch = ctx->hdr.nb_ch;
	ctx->profile = ctx->hdr.profile;

	sr = GF_M4ASampleRates[ctx->hdr.sr_idx];
	if (!ctx->timescale) {
		//we change sample rate, change cts
		if (ctx->cts && (ctx->sr_idx != ctx->hdr.sr_idx)) {
			ctx->cts *= sr;
			ctx->cts /= GF_M4ASampleRates[ctx->sr_idx];
		}
	}
	ctx->sr_idx = ctx->hdr.sr_idx;

	/*keep MPEG-2 AAC codecid even for HE-SBR (that's correct according to latest MPEG-4 audio spec)*/
	codecid = ctx->hdr.is_mp2 ? ctx->hdr.profile+GF_CODECID_AAC_MPEG2_MP-1 : GF_CODECID_AAC_MPEG4;
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, & PROP_UINT(codecid) );

	//force explicit SBR if explicit PS
	if (ctx->ps==AAC_SIGNAL_EXPLICIT) {
		ctx->sbr = AAC_SIGNAL_EXPLICIT;
	}
	/*no provision for explicit indication of MPEG-2 AAC through MPEG-4 PLs, so force implicit*/
	if (ctx->hdr.is_mp2) {
		if (ctx->sbr == AAC_SIGNAL_EXPLICIT) ctx->sbr = AAC_SIGNAL_IMPLICIT;
		if (ctx->ps == AAC_SIGNAL_EXPLICIT) ctx->ps = AAC_SIGNAL_IMPLICIT;
	}

	dsi = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	ctx->dts_inc = ctx->frame_size;

	sbr_sr = sr;
	sbr_sr_idx = ctx->hdr.sr_idx;
	if (!ctx->ovsbr) {
		for (i=0; i<16; i++) {
			if (GF_M4ASampleRates[i] == (u32) 2*sr) {
				sbr_sr_idx = i;
				sbr_sr = 2*sr;
				break;
			}
		}
	}

	memset(&acfg, 0, sizeof(GF_M4ADecSpecInfo));
	acfg.base_object_type = ctx->hdr.profile;
	acfg.base_sr = sr;
	acfg.nb_chan = gf_m4a_get_channel_cfg(ctx->hdr.nb_ch);
	acfg.sbr_object_type = 0;
	if (ctx->sbr==AAC_SIGNAL_EXPLICIT) {
		acfg.has_sbr = GF_TRUE;
		acfg.base_object_type = 5;
		acfg.sbr_object_type = ctx->hdr.profile;

		/*for better interop, always store using full SR when using explict signaling*/
		ctx->dts_inc *= 2;
		sr = sbr_sr;
	} else if (ctx->sbr==AAC_SIGNAL_IMPLICIT) {
		acfg.has_sbr = GF_TRUE;
	}
	if (ctx->ps==AAC_SIGNAL_EXPLICIT) {
		acfg.has_ps = GF_TRUE;
		acfg.base_object_type = 29;
	} else if (ctx->ps==AAC_SIGNAL_IMPLICIT) {
		acfg.has_ps = GF_TRUE;
	}

	acfg.audioPL = gf_m4a_get_profile(&acfg);
	/*explicit SBR or PS signal (non backward-compatible)*/
	if (ctx->ps==AAC_SIGNAL_EXPLICIT) {
		gf_bs_write_int(dsi, 29, 5);
		gf_bs_write_int(dsi, ctx->hdr.sr_idx, 4);
		gf_bs_write_int(dsi, ctx->hdr.nb_ch, 4);
		gf_bs_write_int(dsi, sbr_sr ? sbr_sr_idx : ctx->hdr.sr_idx, 4);
		gf_bs_write_int(dsi, ctx->hdr.profile, 5);
	}
	/*explicit SBR signal (non backward-compatible)*/
	else if (ctx->sbr==AAC_SIGNAL_EXPLICIT) {
		gf_bs_write_int(dsi, 5, 5);
		gf_bs_write_int(dsi, ctx->hdr.sr_idx, 4);
		gf_bs_write_int(dsi, ctx->hdr.nb_ch, 4);
		gf_bs_write_int(dsi, sbr_sr ? sbr_sr_idx : ctx->hdr.sr_idx, 4);
		gf_bs_write_int(dsi, ctx->hdr.profile, 5);
	} else {
		/*regular AAC*/
		gf_bs_write_int(dsi, ctx->hdr.profile, 5);
		gf_bs_write_int(dsi, ctx->hdr.sr_idx, 4);
		gf_bs_write_int(dsi, ctx->hdr.nb_ch, 4);
		gf_bs_align(dsi);
		/*implicit AAC SBR signal*/
		if (ctx->sbr==AAC_SIGNAL_IMPLICIT) {
			gf_bs_write_int(dsi, 0x2b7, 11); /*sync extension type*/
			gf_bs_write_int(dsi, 5, 5);	/*SBR objectType*/
			gf_bs_write_int(dsi, 1, 1);	/*SBR present flag*/
			gf_bs_write_int(dsi, sbr_sr_idx, 4);
		}
		if (ctx->ps==AAC_SIGNAL_IMPLICIT) {
			gf_bs_write_int(dsi, 0x548, 11); /*sync extension type*/
			gf_bs_write_int(dsi, 1, 1);	/* PS present flag */
		}
	}
	gf_bs_align(dsi);

	gf_bs_get_content(dsi, &dsi_b, &dsi_s);
	gf_bs_del(dsi);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, & PROP_DATA_NO_COPY(dsi_b, dsi_s) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PROFILE_LEVEL, & PROP_UINT (acfg.audioPL) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLE_RATE, & PROP_UINT(sr));

	timescale = sr;
	if (ctx->ovsbr) timescale = 2*sr;

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, & PROP_UINT(ctx->timescale ? ctx->timescale : timescale));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_NUM_CHANNELS, & PROP_UINT(ctx->nb_ch) );

}

static Bool adts_dmx_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	u32 i;
	GF_FilterEvent fevt;
	GF_ADTSDmxCtx *ctx = gf_filter_get_udta(filter);

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if (!ctx->is_playing) {
			ctx->is_playing = GF_TRUE;
			ctx->cts = 0;
		}
		if (! ctx->is_file) {
			if (evt->play.start_range || ctx->initial_play_done) {
				ctx->adts_buffer_size = 0;
				ctx->resume_from = 0;
			}

			ctx->initial_play_done = GF_TRUE;
			return GF_FALSE;
		}
		ctx->start_range = evt->play.start_range;
		ctx->in_seek = GF_TRUE;
		ctx->file_pos = 0;
		if (ctx->start_range) {
			for (i=1; i<ctx->index_size; i++) {
				if (ctx->indexes[i].duration>ctx->start_range) {
					ctx->cts = (u64) (ctx->indexes[i-1].duration * GF_M4ASampleRates[ctx->sr_idx]);
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
		ctx->resume_from = 0;
		ctx->adts_buffer_size = 0;
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
	assert(ctx->dts_inc);

	if (ctx->timescale) {
		u64 inc = ctx->dts_inc;
		inc *= ctx->timescale;
		inc /= GF_M4ASampleRates[ctx->sr_idx];
		ctx->cts += inc;
	} else {
		ctx->cts += ctx->dts_inc;
	}
}

GF_Err adts_dmx_process(GF_Filter *filter)
{
	GF_ADTSDmxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck;
	u8 *data, *output;
	u8 *start;
	u32 pck_size, remain, prev_pck_size;
	u64 cts = GF_FILTER_NO_TS;

	//always reparse duration
	if (!ctx->duration.num)
		adts_dmx_check_dur(filter, ctx);

	if (ctx->opid && !ctx->is_playing)
		return GF_OK;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			if (!ctx->adts_buffer_size) {
				gf_filter_pid_set_eos(ctx->opid);
				if (ctx->src_pck) gf_filter_pck_unref(ctx->src_pck);
				ctx->src_pck = NULL;
				return GF_EOS;
			}
		} else {
			return GF_OK;
		}
	}

	prev_pck_size = ctx->adts_buffer_size;
	if (pck && !ctx->resume_from) {
		data = (char *) gf_filter_pck_get_data(pck, &pck_size);
		if (!pck_size) {
			gf_filter_pid_drop_packet(ctx->ipid);
			return GF_OK;
		}

		if (ctx->byte_offset != GF_FILTER_NO_BO) {
			u64 byte_offset = gf_filter_pck_get_byte_offset(pck);
			if (!ctx->adts_buffer_size) {
				ctx->byte_offset = byte_offset;
			} else if (ctx->byte_offset + ctx->adts_buffer_size != byte_offset) {
				ctx->byte_offset = GF_FILTER_NO_BO;
				if ((byte_offset != GF_FILTER_NO_BO) && (byte_offset>ctx->adts_buffer_size) ) {
					ctx->byte_offset = byte_offset - ctx->adts_buffer_size;
				}
			}
		}

		if (ctx->adts_buffer_size + pck_size > ctx->adts_buffer_alloc) {
			ctx->adts_buffer_alloc = ctx->adts_buffer_size + pck_size;
			ctx->adts_buffer = gf_realloc(ctx->adts_buffer, ctx->adts_buffer_alloc);
		}
		memcpy(ctx->adts_buffer + ctx->adts_buffer_size, data, pck_size);
		ctx->adts_buffer_size += pck_size;
	}

	//input pid sets some timescale - we flushed pending data , update cts
	if (ctx->timescale && pck) {
		cts = gf_filter_pck_get_cts(pck);
	}

	if (cts == GF_FILTER_NO_TS) {
		//avoids updating cts
		prev_pck_size = 0;
	}

	remain = ctx->adts_buffer_size;
	start = ctx->adts_buffer;

	if (ctx->resume_from) {
		start += ctx->resume_from - 1;
		remain -= ctx->resume_from - 1;
		ctx->resume_from = 0;
	}

	while (remain) {
		u8 *sync;
		u32 sync_pos, size, offset, bytes_to_drop=0;

		sync = memchr(start, 0xFF, remain);
		sync_pos = (u32) (sync ? sync - start : remain);

		//couldn't find sync byte in this packet
		if (remain - sync_pos < 7) {
			break;
		}

		//not sync !
		if ((remain - sync_pos <= 1) || ((sync[1] & 0xF0) != 0xF0) ) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[ADTSDmx] invalid ADTS sync bytes, resyncing\n"));
			goto drop_byte;
		}
		if (!ctx->bs) {
			ctx->bs = gf_bs_new(sync + 1, remain - sync_pos - 1, GF_BITSTREAM_READ);
		} else {
			gf_bs_reassign_buffer(ctx->bs, sync + 1, remain - sync_pos - 1);
		}

		//ok parse header
		gf_bs_read_int(ctx->bs, 4);

		ctx->hdr.is_mp2 = (Bool)gf_bs_read_int(ctx->bs, 1);
		if (ctx->mpeg4) ctx->hdr.is_mp2 = 0;

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

		if (!ctx->hdr.frame_size || !GF_M4ASampleRates[ctx->hdr.sr_idx] || !ctx->hdr.nb_ch) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[ADTSDmx] invalid ADTS frame, resyncing\n"));
			goto drop_byte;
		}

		//ready to send packet
		if (ctx->hdr.frame_size + 1 < remain) {
			u32 next_frame = ctx->hdr.frame_size;
			//make sure we are sync!
			if ((sync[next_frame] !=0xFF) || ((sync[next_frame+1] & 0xF0) !=0xF0) ) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[ADTSDmx] invalid ADTS frame, resyncing\n"));
				goto drop_byte;
			}
		}
		//otherwise wait for next frame, unless if end of stream
		else if (pck) {
			if (ctx->timescale && !prev_pck_size &&  (cts != GF_FILTER_NO_TS) ) {
				ctx->cts = cts;
			}
			break;
		}

		adts_dmx_check_pid(filter, ctx);

		if (!ctx->is_playing) {
			ctx->resume_from = 1 + ctx->adts_buffer_size - remain;
			return GF_OK;
		}

		size = ctx->hdr.frame_size - ctx->hdr.hdr_size;
		offset = ctx->hdr.hdr_size;

		if (ctx->in_seek) {
			u64 nb_samples_at_seek = (u64) (ctx->start_range * GF_M4ASampleRates[ctx->sr_idx]);
			if (ctx->cts + ctx->dts_inc >= nb_samples_at_seek) {
				//u32 samples_to_discard = (ctx->cts + ctx->dts_inc) - nb_samples_at_seek;
				ctx->in_seek = GF_FALSE;
			}
		}

		bytes_to_drop = ctx->hdr.frame_size;
		if (ctx->timescale && !prev_pck_size &&  (cts != GF_FILTER_NO_TS) ) {
			ctx->cts = cts;
			cts = GF_FILTER_NO_TS;
		}

		if (!ctx->in_seek) {
			dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &output);
			if (ctx->src_pck) gf_filter_pck_merge_properties(ctx->src_pck, dst_pck);

			memcpy(output, sync + offset, size);
			gf_filter_pck_set_dts(dst_pck, ctx->cts);
			gf_filter_pck_set_cts(dst_pck, ctx->cts);
			gf_filter_pck_set_duration(dst_pck, ctx->dts_inc);
			gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_TRUE);
			gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1);

			if (ctx->byte_offset != GF_FILTER_NO_BO) {
				gf_filter_pck_set_byte_offset(dst_pck, ctx->byte_offset + ctx->hdr.hdr_size);
			}

			gf_filter_pck_send(dst_pck);
		}
		adts_dmx_update_cts(ctx);


		//truncated last frame
		if (bytes_to_drop>remain) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[ADTSDmx] truncated ADTS frame!\n"));
			bytes_to_drop=remain;
		}

drop_byte:
		if (!bytes_to_drop) {
			bytes_to_drop = 1;
		}
		start += bytes_to_drop;
		remain -= bytes_to_drop;

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
		ctx->adts_buffer_size = 0;
		return adts_dmx_process(filter);
	} else {
		if (remain) {
			memmove(ctx->adts_buffer, start, remain);
		}
		ctx->adts_buffer_size = remain;
		gf_filter_pid_drop_packet(ctx->ipid);
	}
	return GF_OK;
}

static void adts_dmx_finalize(GF_Filter *filter)
{
	GF_ADTSDmxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->bs) gf_bs_del(ctx->bs);
	if (ctx->indexes) gf_free(ctx->indexes);
	if (ctx->adts_buffer) gf_free(ctx->adts_buffer);
}

static const char *adts_dmx_probe_data(const u8 *data, u32 size, GF_FilterProbeScore *score)
{
	u32 nb_frames=0, next_pos=0, max_consecutive_frames=0;
	ADTSHeader prev_hdr;
	GF_BitStream *bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
	memset(&prev_hdr, 0, sizeof(ADTSHeader));
	while (gf_bs_available(bs)) {
		ADTSHeader hdr;
		u32 pos;
		if (!adts_dmx_sync_frame_bs(bs, &hdr)) break;
		if ((hdr.hdr_size!=7) && (hdr.hdr_size!=9)) continue;
		if (!hdr.nb_ch) continue;
		pos = (u32) gf_bs_get_position(bs);
		if ((next_pos + hdr.hdr_size == pos) && (hdr.sr_idx==prev_hdr.sr_idx) && (hdr.nb_ch==prev_hdr.nb_ch) ) {
			nb_frames++;
			if (max_consecutive_frames<nb_frames) max_consecutive_frames = nb_frames;
			if (max_consecutive_frames>5)
				break;
		} else {
			nb_frames=1;
		}
		prev_hdr = hdr;
		gf_bs_skip_bytes(bs, hdr.frame_size);
		next_pos = (u32) gf_bs_get_position(bs);
	}
	gf_bs_del(bs);
	if (max_consecutive_frames>=4) {
		*score = GF_FPROBE_MAYBE_SUPPORTED;
		return "audio/aac";
	}
	return NULL;
}

static const GF_FilterCapability ADTSDmxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "audio/x-m4a|audio/aac|audio/aacp|audio/x-aac"),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG4),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_MP),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_LCP),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_SSRP),
	//we explitely set this one to prevent adts->latm reframer connection
	CAP_BOOL(GF_CAPS_OUTPUT_STATIC_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "aac"),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG4),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
};


#define OFFS(_n)	#_n, offsetof(GF_ADTSDmxCtx, _n)
static const GF_FilterArgs ADTSDmxArgs[] =
{
	{ OFFS(frame_size), "size of AAC frame in audio samples", GF_PROP_UINT, "1024", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(index), "indexing window length", GF_PROP_DOUBLE, "1.0", NULL, 0},
	{ OFFS(mpeg4), "force signaling as MPEG-4 AAC", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(ovsbr), "force oversampling SBR (does not multiply timescales by 2)", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(sbr), "set SBR signaling\n"\
				"- no: no SBR signaling at all\n"\
				"- imp: backward-compatible SBR signaling (audio signaled as AAC-LC)\n"\
				"- exp: explicit SBR signaling (audio signaled as AAC-SBR)"\
				, GF_PROP_UINT, "no", "no|imp|exp", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(ps), "set PS signaling\n"\
				"- no: no PS signaling at all\n"\
				"- imp: backward-compatible PS signaling (audio signaled as AAC-LC)\n"\
				"- exp: explicit PS signaling (audio signaled as AAC-PS)"\
				, GF_PROP_UINT, "no", "no|imp|exp", GF_FS_ARG_HINT_ADVANCED},
	{0}
};


GF_FilterRegister ADTSDmxRegister = {
	.name = "rfadts",
	GF_FS_SET_DESCRIPTION("ADTS reframer")
	GF_FS_SET_HELP("This filter parses AAC files/data and outputs corresponding audio PID and frames.")
	.private_size = sizeof(GF_ADTSDmxCtx),
	.args = ADTSDmxArgs,
	.finalize = adts_dmx_finalize,
	SETCAPS(ADTSDmxCaps),
	.configure_pid = adts_dmx_configure_pid,
	.process = adts_dmx_process,
	.probe_data = adts_dmx_probe_data,
	.process_event = adts_dmx_process_event
};


const GF_FilterRegister *adts_dmx_register(GF_FilterSession *session)
{
	return &ADTSDmxRegister;
}
