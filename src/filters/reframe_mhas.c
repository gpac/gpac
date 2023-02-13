/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2020-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / MHAS reframer filter
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

#if !defined(GPAC_DISABLE_AV_PARSERS)


static u32 USACSampleRates[] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350, 0, 0,
	57600, 51200, 40000, 38400, 34150, 28800, 25600, 20000, 19200, 17075, 14400, 12800, 9600};

static u32 nb_usac_sr = GF_ARRAY_LENGTH(USACSampleRates);


typedef struct
{
	u64 pos;
	Double duration;
} MHASIdx;

typedef struct
{
	//filter args
	Double index;
	Bool mpha;
	u32 pcksync;
	Bool nosync;

	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;

	GF_BitStream *bs;
	u64 file_pos, cts, prev_cts;

	GF_Fraction64 duration;
	Double start_range;
	Bool in_seek;
	u32 timescale;
	Bool is_playing;
	Bool is_file;
	Bool initial_play_done, file_loaded;

	Bool initialized;

	u8 *mhas_buffer;
	u32 mhas_buffer_size, mhas_buffer_alloc, resume_from;
	u64 byte_offset;
	Bool buffer_too_small;

	GF_FilterPacket *src_pck;

	Bool recompute_cts;
	MHASIdx *indexes;
	u32 index_alloc_size, index_size;

	u32 sample_rate, frame_len, PL;
	s32 cicp_layout_idx, num_speakers;
	u32 nb_frames;

	u32 nb_unknown_pck;
	u32 bitrate;
	Bool copy_props;
	Bool is_sync;
} GF_MHASDmxCtx;




GF_Err mhas_dmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	GF_MHASDmxCtx *ctx = gf_filter_get_udta(filter);

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

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	if (p) {
		ctx->timescale = p->value.uint;
		//if stream comes from TS or other muxed source unframed, force initial sync check
		if (!ctx->ipid) {
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_UNFRAMED);
			if (p && p->value.boolean)
				ctx->nosync = GF_TRUE;
		}
	}
	ctx->ipid = pid;
	p = gf_filter_pid_get_property_str(pid, "nocts");
	if (p && p->value.boolean) ctx->recompute_cts = GF_TRUE;
	else ctx->recompute_cts = GF_FALSE;

	if (ctx->timescale && !ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, NULL);
	}
	if (ctx->timescale) ctx->copy_props = GF_TRUE;
	return GF_OK;
}

static void mhas_dmx_check_dur(GF_Filter *filter, GF_MHASDmxCtx *ctx)
{
	GF_Fraction64 duration;
	FILE *stream;
	GF_BitStream *bs;
	u32 frame_len, cur_dur;
	Bool mhas_sap;
	u64 mhas_last_cfg, rate;
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
	duration.num = duration.den = 0;
	frame_len = cur_dur = 0;
	mhas_last_cfg = 0;

	while (gf_bs_available(bs)) {
		u32 sync_code = gf_bs_peek_bits(bs, 24, 0);
		if (sync_code == 0xC001A5) {
			break;
		}
		gf_bs_skip_bytes(bs, 1);
	}
	while (gf_bs_available(bs)) {
		u64 mhas_pck_start, pay_start, parse_end, mhas_size;
		u32 mhas_type;

		mhas_pck_start = gf_bs_get_position(bs);
		mhas_type = (u32) gf_mpegh_escaped_value(bs, 3, 8, 8);
		/*mhas_label = */gf_mpegh_escaped_value(bs, 2, 8, 32);
		mhas_size = gf_mpegh_escaped_value(bs, 11, 24, 24);

		pay_start = (u32) gf_bs_get_position(bs);

		if (!gf_bs_available(bs) ) break;
		if (mhas_size > gf_bs_available(bs)) break;

		mhas_sap = 0;
		//frame
		if (mhas_type==2) {
			mhas_sap = gf_bs_read_int(bs, 1);
			if (!mhas_last_cfg) mhas_sap = 0;
		//config
		} else if (mhas_type==1) {
			/*u32 pl = */gf_bs_read_u8(bs);
			u32 idx = gf_bs_read_int(bs, 5);
			if (idx==0x1f)
				duration.den = gf_bs_read_int(bs, 24);
			else if (idx < nb_usac_sr) {
				duration.den = USACSampleRates[idx];
			}
			idx = gf_bs_read_int(bs, 3);
			if ((idx==0) || (idx==2) ) frame_len = 768;
			else frame_len = 1024;

			mhas_last_cfg = mhas_pck_start;
		}
		//audio truncation
		else if (mhas_type==17) {
			Bool isActive = gf_bs_read_int(bs, 1);
			/*Bool ati_reserved = */gf_bs_read_int(bs, 1);
			Bool trunc_from_begin = gf_bs_read_int(bs, 1);
			u32 nb_trunc_samples = gf_bs_read_int(bs, 13);
			if (isActive && !trunc_from_begin) {
				duration.num -= nb_trunc_samples;
			}
		}
		gf_bs_align(bs);
		parse_end = (u32) gf_bs_get_position(bs) - pay_start;
		//remaining of packet payload
		gf_bs_skip_bytes(bs, mhas_size - parse_end);

		//mhas_sap only set for frames
		if (mhas_sap && duration.den && (cur_dur >= ctx->index * duration.den) ) {
			if (!ctx->index_alloc_size) ctx->index_alloc_size = 10;
			else if (ctx->index_alloc_size == ctx->index_size) ctx->index_alloc_size *= 2;
			ctx->indexes = gf_realloc(ctx->indexes, sizeof(MHASIdx)*ctx->index_alloc_size);
			ctx->indexes[ctx->index_size].pos = mhas_last_cfg;
			ctx->indexes[ctx->index_size].duration = ((Double) duration.num) / duration.den;
			ctx->index_size ++;
			cur_dur = 0;
		}
		if (mhas_type==2) {
			duration.num += frame_len;
			cur_dur += frame_len;
			mhas_last_cfg = 0;
		}
	}

	rate = gf_bs_get_position(bs);
	gf_bs_del(bs);
	gf_fclose(stream);

	if (!ctx->duration.num || (ctx->duration.num  * duration.den != duration.num * ctx->duration.den)) {
		ctx->duration = duration;
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC64(ctx->duration));

		if (duration.num && !gf_sys_is_test_mode() ) {
			rate *= 8 * ctx->duration.den;
			rate /= ctx->duration.num;
			ctx->bitrate = (u32) rate;
		}
	}

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILE_CACHED);
	if (p && p->value.boolean) ctx->file_loaded = GF_TRUE;
}

static void mhas_dmx_check_pid(GF_Filter *filter, GF_MHASDmxCtx *ctx, u32 PL, u32 sample_rate, u32 frame_len, s32 CICPspeakerLayoutIdx, s32 numSpeakers, u8 *dsi, u32 dsi_size)
{
	u32 nb_channels;
	u64 chan_layout;
	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		mhas_dmx_check_dur(filter, ctx);
	} else {
		if ((ctx->frame_len == frame_len)
			&& (ctx->PL == PL)
			&& (ctx->sample_rate == sample_rate)
			&& (ctx->cicp_layout_idx == CICPspeakerLayoutIdx)
			&& (ctx->num_speakers == numSpeakers)
			&& !ctx->copy_props
		) {
			return;
		}
	}
	ctx->frame_len = frame_len;
	ctx->PL = PL;
	ctx->sample_rate = sample_rate;
	ctx->cicp_layout_idx = CICPspeakerLayoutIdx;
	ctx->num_speakers = numSpeakers;
	ctx->copy_props = GF_FALSE;

	chan_layout = 0;
	nb_channels = 0;
	if (CICPspeakerLayoutIdx>=0) {
		chan_layout = gf_audio_fmt_get_layout_from_cicp(CICPspeakerLayoutIdx);
		nb_channels = gf_audio_fmt_get_num_channels_from_layout(chan_layout);
	} else if (numSpeakers>=0) {
		nb_channels = numSpeakers;
	}

	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, & PROP_UINT( GF_STREAM_AUDIO));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, NULL );
	if (ctx->is_file && ctx->index) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PLAYBACK_MODE, & PROP_UINT(GF_PLAYBACK_MODE_FASTFORWARD) );
	}
	if (ctx->duration.num)
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC64(ctx->duration));
	if (!ctx->timescale)
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CAN_DATAREF, & PROP_BOOL(GF_TRUE ) );

	if (!ctx->timescale) gf_filter_pid_set_name(ctx->opid, "audio");

	if (ctx->mpha) {
		u8 *data = gf_malloc(sizeof(u8) * (dsi_size+5) );
		if (!data) return;
		data[0] = 1;
		data[1] = PL;
		data[2] = CICPspeakerLayoutIdx;
		data[3] = dsi_size>>8;
		data[4] = dsi_size&0xFF;
		memcpy(data+5, dsi, dsi_size);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, & PROP_UINT( GF_CODECID_MPHA ) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, & PROP_DATA_NO_COPY( data, (dsi_size+5) ) );
	} else {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, & PROP_UINT( GF_CODECID_MHAS ) );
	}

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, & PROP_UINT(ctx->timescale ? ctx->timescale : ctx->sample_rate));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLE_RATE, & PROP_UINT(ctx->sample_rate));
	if (chan_layout)
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CHANNEL_LAYOUT, & PROP_LONGUINT(chan_layout) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_NUM_CHANNELS, & PROP_UINT(nb_channels) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLES_PER_FRAME, & PROP_UINT(ctx->frame_len) );

	if (ctx->bitrate) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_BITRATE, & PROP_UINT(ctx->bitrate));
	}
}

static Bool mhas_dmx_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	u32 i;
	GF_FilterEvent fevt;
	GF_MHASDmxCtx *ctx = gf_filter_get_udta(filter);

	if (evt->base.on_pid != ctx->opid) return GF_TRUE;

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if (!ctx->is_playing) {
			ctx->is_playing = GF_TRUE;
		}
		if (! ctx->is_file) {
			if (evt->play.start_range || ctx->initial_play_done) {
				ctx->mhas_buffer_size = 0;
				ctx->resume_from = 0;
			}
			ctx->initial_play_done = GF_TRUE;
			return GF_FALSE;
		}
		mhas_dmx_check_dur(filter, ctx);

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
		ctx->mhas_buffer_size = 0;
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

static GFINLINE void mhas_dmx_update_cts(GF_MHASDmxCtx *ctx)
{
	if (ctx->timescale) {
		u64 inc = ctx->frame_len;
		inc *= ctx->timescale;
		inc /= ctx->sample_rate;
		ctx->cts += inc;
	} else {
		ctx->cts += ctx->frame_len;
	}
}

#ifndef GPAC_DISABLE_LOG
static const char *mhas_pck_name(u32 pck_type)
{
	switch (pck_type) {
	case 0: return "FILL_DATA";
	case 1: return "MPEGH3DACFG";
	case 2: return "MPEGH3DAFRAME";
	case 3: return "AUDIOSCENEINFO";
	case 6: return "SYNC";
	case 7: return "SYNCGAP";
	case 8: return "MARKER";
	case 9: return "CRC16";
	case 10: return "CRC32";
	case 11: return "DESCRIPTOR";
	case 12: return "USERINTERACTION";
	case 13: return "LOUDNESS_DRC";
	case 14: return "BUFFERINFO";
	case 15: return "GLOBAL_CRC16";
	case 16: return "GLOBAL_CRC32";
	case 17: return "AUDIOTRUNCATION";
	case 18: return "GENDATA";
	case 4:
	case 5:
	default:
		return "ISOReserved";
	}
	return "error";
}
#endif

GF_Err mhas_dmx_process(GF_Filter *filter)
{
	GF_MHASDmxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *in_pck;
	u8 *output;
	u8 *start;
	Bool final_flush=GF_FALSE;
	u32 pck_size, remain, prev_pck_size;
	u64 cts = GF_FILTER_NO_TS;
	u32 au_start = 0;
	u32 consumed = 0;
	u32 nb_trunc_samples = 0;
	Bool trunc_from_begin = 0;
	Bool has_cfg = 0;

	//always reparse duration
	if (!ctx->duration.num)
		mhas_dmx_check_dur(filter, ctx);

	if (ctx->opid && !ctx->is_playing)
		return GF_OK;

	in_pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!in_pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			if (!ctx->mhas_buffer_size) {
				if (ctx->opid)
					gf_filter_pid_set_eos(ctx->opid);
				if (ctx->src_pck) gf_filter_pck_unref(ctx->src_pck);
				ctx->src_pck = NULL;
				return GF_EOS;
			}
			final_flush = GF_TRUE;
		} else if (!ctx->resume_from) {
			return GF_OK;
		}
	}

	prev_pck_size = ctx->mhas_buffer_size;
	if (ctx->resume_from)
		in_pck = NULL;

	if (in_pck) {
		u8 *data = (u8 *) gf_filter_pck_get_data(in_pck, &pck_size);

		if (ctx->byte_offset != GF_FILTER_NO_BO) {
			u64 byte_offset = gf_filter_pck_get_byte_offset(in_pck);
			if (!ctx->mhas_buffer_size) {
				ctx->byte_offset = byte_offset;
			} else if (ctx->byte_offset + ctx->mhas_buffer_size != byte_offset) {
				ctx->byte_offset = GF_FILTER_NO_BO;
				if ((byte_offset != GF_FILTER_NO_BO) && (byte_offset>ctx->mhas_buffer_size) ) {
					ctx->byte_offset = byte_offset - ctx->mhas_buffer_size;
				}
			}
		}

		if (ctx->mhas_buffer_size + pck_size > ctx->mhas_buffer_alloc) {
			ctx->mhas_buffer_alloc = ctx->mhas_buffer_size + pck_size;
			ctx->mhas_buffer = gf_realloc(ctx->mhas_buffer, ctx->mhas_buffer_alloc);
		}
		memcpy(ctx->mhas_buffer + ctx->mhas_buffer_size, data, pck_size);
		ctx->mhas_buffer_size += pck_size;
	}

	//input pid sets some timescale - we flushed pending data , update cts
	if (ctx->timescale && in_pck) {
		cts = gf_filter_pck_get_cts(in_pck);
		//init cts at first packet
		if (!ctx->cts && (cts != GF_FILTER_NO_TS))
			ctx->cts = cts;
	}

	if (cts == GF_FILTER_NO_TS) {
		//avoids updating cts
		prev_pck_size = 0;
	}

	remain = ctx->mhas_buffer_size;
	start = ctx->mhas_buffer;

	if (ctx->resume_from) {
		start += ctx->resume_from - 1;
		remain -= ctx->resume_from - 1;
		ctx->resume_from = 0;
	}

	while (ctx->nosync && (remain>3)) {
		//wait till we have a frame header
		u8 *hdr_start = memchr(start, 0xC0, remain);
		if (!hdr_start) {
			remain=0;
			break;
		}
		if ((hdr_start[1]==0x01) && (hdr_start[2]==0xA5)) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[MHASDmx] Sync found !\n"));
			ctx->nosync = GF_FALSE;
			break;
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[MHASDmx] not sync, skipping byte\n"));
		start++;
		remain--;
	}
	if (ctx->nosync)
		goto skip;

	gf_bs_reassign_buffer(ctx->bs, start, remain);
	ctx->buffer_too_small = GF_FALSE;

	//MHAS packet
	while (remain > consumed) {
		u32 pay_start, parse_end, mhas_size, mhas_label;
		Bool mhas_sap = 0;
		u32 mhas_type;
		if (!ctx->is_playing && ctx->opid) {
			ctx->resume_from = 1;
			consumed = 0;
			break;
		}

		mhas_type = (u32) gf_mpegh_escaped_value(ctx->bs, 3, 8, 8);
		mhas_label = (u32) gf_mpegh_escaped_value(ctx->bs, 2, 8, 32);
		mhas_size = (u32) gf_mpegh_escaped_value(ctx->bs, 11, 24, 24);

		if (ctx->buffer_too_small)
			break;


		if (mhas_type>18) {
			ctx->nb_unknown_pck++;
			if (ctx->nb_unknown_pck > ctx->pcksync) {
				GF_LOG(ctx->is_sync ? GF_LOG_WARNING : GF_LOG_DEBUG, GF_LOG_MEDIA, ("[MHASDmx] %d packets of unknown type, considering sync was lost\n"));
				ctx->is_sync = GF_FALSE;
				consumed = 0;
				ctx->nosync = GF_TRUE;
				ctx->nb_unknown_pck = 0;
				break;
			}
		} else if (!mhas_size) {
			GF_LOG(ctx->is_sync ? GF_LOG_WARNING : GF_LOG_DEBUG, GF_LOG_MEDIA, ("[MHASDmx] MHAS packet with 0 payload size, considering sync was lost\n"));
			ctx->is_sync = GF_FALSE;
			consumed = 0;
			ctx->nosync = GF_TRUE;
			ctx->nb_unknown_pck = 0;
			break;
		}

		pay_start = (u32) gf_bs_get_position(ctx->bs);

		if (ctx->buffer_too_small) break;
		if (mhas_size > gf_bs_available(ctx->bs)) {
			//incomplete frame, keep in buffer
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[MHASDmx] incomplete packet type %d %s label "LLU" size "LLU" - keeping in buffer\n", mhas_type, mhas_pck_name(mhas_type), mhas_label, mhas_size));
			break;
		}
		ctx->is_sync = GF_TRUE;

		//frame
		if (mhas_type==2) {
			mhas_sap = gf_bs_peek_bits(ctx->bs, 1, 0);
			ctx->nb_unknown_pck = 0;
		}
		//config
		else if (mhas_type==1) {
			s32 CICPspeakerLayoutIdx = -1;
			s32 numSpeakers = -1;
			u32 sr = 0;
			u32 frame_len;
			u32 pl = gf_bs_read_u8(ctx->bs);
			u32 idx = gf_bs_read_int(ctx->bs, 5);
			if (idx==0x1f)
				sr = gf_bs_read_int(ctx->bs, 24);
			else if (idx < nb_usac_sr) {
				sr = USACSampleRates[idx];
			}
			ctx->nb_unknown_pck = 0;
			idx = gf_bs_read_int(ctx->bs, 3);
			if ((idx==0) || (idx==2) ) frame_len = 768;
			else frame_len = 1024;
			gf_bs_read_int(ctx->bs, 1);
			gf_bs_read_int(ctx->bs, 1);

			//speaker config
			u32 speakerLayoutType = gf_bs_read_int(ctx->bs, 2);
			if (speakerLayoutType == 0) {
				CICPspeakerLayoutIdx = gf_bs_read_int(ctx->bs, 6);
			} else {
				numSpeakers = (s32) gf_mpegh_escaped_value(ctx->bs, 5, 8, 16) + 1;
				//TODO ...
			}

			mhas_dmx_check_pid(filter, ctx, pl, sr, frame_len, CICPspeakerLayoutIdx, numSpeakers, start + pay_start, (u32) mhas_size);

			has_cfg = GF_TRUE;
		}
		//audio truncation
		else if (mhas_type==17) {
			Bool isActive = gf_bs_read_int(ctx->bs, 1);
			/*Bool ati_reserved = */gf_bs_read_int(ctx->bs, 1);
			trunc_from_begin = gf_bs_read_int(ctx->bs, 1);
			nb_trunc_samples = gf_bs_read_int(ctx->bs, 13);
			if (!isActive) {
				nb_trunc_samples = 0;
			}
		}
		//sync, syncgap
		else if ((mhas_type==6) || (mhas_type==7)) {
			ctx->nb_unknown_pck = 0;
		}
#if 0
		//MARKER
		else if (mhas_type==8) {
			u8 marker_type = gf_bs_read_u8(ctx->bs);
			//config reload force
			if (marker_type==0x01) {}
			//SAP
			else if (marker_type==0x02) {
				has_marker = GF_TRUE;
			}
		}
#endif

		gf_bs_align(ctx->bs);
		parse_end = (u32) gf_bs_get_position(ctx->bs) - pay_start;
		//remaining of packet payload
		gf_bs_skip_bytes(ctx->bs, mhas_size - parse_end);

		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[MHASDmx] MHAS Packet type %d %s label "LLU" size "LLU"\n", mhas_type, mhas_pck_name(mhas_type), mhas_label, mhas_size));

		if (ctx->timescale && !prev_pck_size && (cts != GF_FILTER_NO_TS) ) {
			ctx->cts = cts;
			cts = GF_FILTER_NO_TS;
		}

		//frame
		if ((mhas_type==2) && ctx->opid) {
			GF_FilterPacket *dst;
			u64 pck_dur = ctx->frame_len;


			u32 au_size;
			if (ctx->mpha) {
				au_start = pay_start;
				au_size = mhas_size;
			} else {
				au_size = (u32) gf_bs_get_position(ctx->bs) - au_start;
			}

			if (nb_trunc_samples) {
				if (trunc_from_begin) {
					if (!ctx->nb_frames) {
						s64 offset = trunc_from_begin;
						if (ctx->timescale) {
							offset *= ctx->timescale;
							offset /= ctx->sample_rate;
						}
						gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DELAY , &PROP_LONGSINT( -offset));
					}
				} else {
					pck_dur -= nb_trunc_samples;
				}
				nb_trunc_samples = 0;
			}

			if (ctx->timescale) {
				pck_dur *= ctx->timescale;
				pck_dur /= ctx->sample_rate;
			}

			dst = gf_filter_pck_new_alloc(ctx->opid, au_size, &output);
			if (!dst) break;
			if (ctx->src_pck) gf_filter_pck_merge_properties(ctx->src_pck, dst);

			memcpy(output, start + au_start, au_size);
			if (!has_cfg)
				mhas_sap = 0;

			if (mhas_sap) {
				gf_filter_pck_set_sap(dst, GF_FILTER_SAP_1);
			}
			gf_filter_pck_set_dts(dst, ctx->cts);
			gf_filter_pck_set_cts(dst, ctx->cts);
			gf_filter_pck_set_duration(dst, (u32) pck_dur);
			if (ctx->byte_offset != GF_FILTER_NO_BO) {
				u64 offset = (u64) (start - ctx->mhas_buffer);
				offset += ctx->byte_offset + au_start;
				gf_filter_pck_set_byte_offset(dst, offset);
			}
 			GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[MHASDmx] Send AU CTS "LLU" size %d dur %d sap %d\n", ctx->cts, au_size, (u32) pck_dur, mhas_sap));
			gf_filter_pck_send(dst);

			au_start += au_size;
			consumed = au_start;
			ctx->nb_frames ++;

			mhas_dmx_update_cts(ctx);
			has_cfg = 0;

			if (prev_pck_size) {
				u64 next_pos = (u64) (start + au_start - ctx->mhas_buffer);
				//next will be in new packet
				if (prev_pck_size <= next_pos) {
					prev_pck_size = 0;
					if (ctx->src_pck) gf_filter_pck_unref(ctx->src_pck);
					ctx->src_pck = in_pck;
					if (in_pck)
						gf_filter_pck_ref_props(&ctx->src_pck);

					if (ctx->timescale && (cts != GF_FILTER_NO_TS) ) {
						ctx->cts = cts;
						cts = GF_FILTER_NO_TS;
					}
				}
			}
			if (remain==consumed)
				break;

			if (gf_filter_pid_would_block(ctx->opid)) {
				ctx->resume_from = 1;
				final_flush = GF_FALSE;
				break;
			}
		}
	}
	if (consumed) {
		assert(remain>=consumed);
		remain -= consumed;
		start += consumed;
	}

skip:

	if (remain < ctx->mhas_buffer_size) {
		memmove(ctx->mhas_buffer, start, remain);
		//update byte offset
		if (ctx->byte_offset != GF_FILTER_NO_BO)
			ctx->byte_offset += ctx->mhas_buffer_size - remain;
	}
	ctx->mhas_buffer_size = remain;
	if (final_flush)
		ctx->mhas_buffer_size = 0;

	if (!ctx->mhas_buffer_size) {
		if (ctx->src_pck) gf_filter_pck_unref(ctx->src_pck);
		ctx->src_pck = NULL;
	}

	if (in_pck)
		gf_filter_pid_drop_packet(ctx->ipid);

	return GF_OK;
}

static void mhas_buffer_too_small(void *udta)
{
	GF_MHASDmxCtx *ctx = (GF_MHASDmxCtx *) udta;
	ctx->buffer_too_small = GF_TRUE;
}

static GF_Err mhas_dmx_initialize(GF_Filter *filter)
{
	GF_MHASDmxCtx *ctx = gf_filter_get_udta(filter);
	ctx->bs = gf_bs_new((u8 *)ctx, 1, GF_BITSTREAM_READ);
	gf_bs_set_eos_callback(ctx->bs, mhas_buffer_too_small, ctx);
	return GF_OK;
}
static void mhas_dmx_finalize(GF_Filter *filter)
{
	GF_MHASDmxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->bs) gf_bs_del(ctx->bs);
	if (ctx->indexes) gf_free(ctx->indexes);
	if (ctx->mhas_buffer) gf_free(ctx->mhas_buffer);
	if (ctx->src_pck) gf_filter_pck_unref(ctx->src_pck);
}


static const char *mhas_dmx_probe_data(const u8 *data, u32 size, GF_FilterProbeScore *score)
{
	s32 sync_pos = -1;
	GF_BitStream *bs;
	u32 nb_mhas_cfg = 0;
	u32 nb_mhas_frames = 0;
	u32 nb_mhas_unknown = 0;
	const u8 *ptr = data;
	while (ptr) {
		u32 pos = (u32) (ptr - data);
		const u8 *sync_start = memchr(ptr, 0xC0, size - pos);
		if (!sync_start) return NULL;
		if ((sync_start[1]== 0x01) && (sync_start[2]==0xA5)) {
			sync_pos = pos;
			break;
		}
		ptr = sync_start+1;
	}
	if (sync_pos<0) return NULL;
	bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
	gf_bs_skip_bytes(bs, sync_pos);

	while (gf_bs_available(bs)) {
		u32 type = (u32) gf_mpegh_escaped_value(bs, 3, 8, 8);
		/*u64 label = */gf_mpegh_escaped_value(bs, 2, 8, 32);
		u64 mh_size = gf_mpegh_escaped_value(bs, 11, 24, 24);
		if (mh_size > gf_bs_available(bs))
			break;
		//MHAS config
		if (type==1) nb_mhas_cfg++;
		else if (type==2) nb_mhas_frames++;
		else if (type>18) nb_mhas_unknown++;
		gf_bs_skip_bytes(bs, mh_size);
	}
	gf_bs_del(bs);
	if (!nb_mhas_unknown && nb_mhas_cfg && nb_mhas_frames) {
		*score = GF_FPROBE_SUPPORTED;
		return "audio/mpegh";
	}
	return NULL;
}

static const GF_FilterCapability MHASDmxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "mhas"),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "audio/mpegh"),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_MHAS),
	CAP_BOOL(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_MHAS),
	CAP_BOOL(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
};



#define OFFS(_n)	#_n, offsetof(GF_MHASDmxCtx, _n)
static const GF_FilterArgs MHASDmxArgs[] =
{
	{ OFFS(index), "indexing window length", GF_PROP_DOUBLE, "1.0", NULL, 0},
	{ OFFS(mpha), "demultiplex MHAS and only forward audio frames", GF_PROP_BOOL, "false", NULL, 0},
	{ OFFS(pcksync), "number of unknown packets to tolerate before considering sync is lost", GF_PROP_UINT, "4", NULL, 0},
	{ OFFS(nosync), "initial sync state", GF_PROP_BOOL, "true", NULL, 0},

	{0}
};


GF_FilterRegister MHASDmxRegister = {
	.name = "rfmhas",
	GF_FS_SET_DESCRIPTION("MPEH-H Audio Stream reframer")
	GF_FS_SET_HELP("This filter parses MHAS files/data and outputs corresponding audio PID and frames.\n"
		"By default, the filter expects a MHAS stream with SYNC packets set, otherwise tune-in will fail. Using [-nosync]()=false can help parsing bitstreams with no SYNC packets.\n"
		"The default behavior is to dispatch a framed MHAS bitstream. To demultiplex into a raw MPEG-H Audio, use [-mpha]().\n"
		)
	.private_size = sizeof(GF_MHASDmxCtx),
	.args = MHASDmxArgs,
	.finalize = mhas_dmx_finalize,
	.initialize = mhas_dmx_initialize,
	SETCAPS(MHASDmxCaps),
	.configure_pid = mhas_dmx_configure_pid,
	.process = mhas_dmx_process,
	.probe_data = mhas_dmx_probe_data,
	.process_event = mhas_dmx_process_event
};


const GF_FilterRegister *mhas_dmx_register(GF_FilterSession *session)
{
	return &MHASDmxRegister;
}

#else

const GF_FilterRegister *mhas_dmx_register(GF_FilterSession *session)
{
	return NULL;
}
#endif //#if !defined(GPAC_DISABLE_AV_PARSERS)
