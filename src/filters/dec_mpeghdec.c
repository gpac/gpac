/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2023
 *					All rights reserved
 *
 *  This file is part of GPAC / MPEG-H Audio decoder using IIS MPEGHDecoder filter
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

#include <gpac/filters.h>
#include <gpac/constants.h>

#ifdef GPAC_HAS_MPEGHDECODER

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#endif

#if !defined(__GNUC__)
# if defined(_WIN32_WCE) || defined (WIN32)
#  pragma comment(lib, "SYS")
#  pragma comment(lib, "FDK")
#  pragma comment(lib, "DRCdec")
#  pragma comment(lib, "PCMutils")
#  pragma comment(lib, "MpeghDec")
#  pragma comment(lib, "IGFdec")
#  pragma comment(lib, "ArithCoding")
#  pragma comment(lib, "FormatConverter")
#  pragma comment(lib, "UIManager")
#  pragma comment(lib, "gVBAPRenderer")
#  pragma comment(lib, "MpegTPDec")
# endif
#endif

#ifndef bool
typedef int bool;
#endif

#include <mpeghdecoder.h>
#include <gpac/constants.h>
#include <gpac/avparse.h>


typedef struct
{
	u32 cicp_idx;
	HANDLE_MPEGH_DECODER_CONTEXT codec;

	u32 sample_rate, timescale;
	s64 delay;
	u8 num_channels;

	GF_FilterPid *ipid, *opid;
	u32 cfg_crc;
	u32 osize;
	u32 flush_state;
	GF_List *src_pcks;
} MPEGHDecCtx;

#define MPEGH_MAX_FRAME_SIZE	3072

static GF_Err mpegh_dec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p, *dsi;
	u32 codec_id;
	MPEGHDecCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		if (ctx->opid) {
			gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
		}
		ctx->ipid = NULL;
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
	}
	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, pid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(GF_AUDIO_FMT_S32) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL );
	ctx->ipid = pid;


	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	codec_id = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	ctx->timescale = p ? p->value.uint : 1000;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLE_RATE);
	ctx->sample_rate = p ? p->value.uint : 44100;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_NUM_CHANNELS);
	ctx->num_channels = p ? p->value.uint : 1;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DELAY);
	ctx->delay = p ? p->value.longsint : 0;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_NO_PRIMING);
	//force re-priming
	if (p && !p->value.boolean) {
		ctx->cfg_crc = 0;
	}

	if (codec_id==GF_CODECID_MPHA) {
		dsi = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
		if (dsi && dsi->value.data.ptr && dsi->value.data.size) {
			u32 ex_crc = gf_crc_32(dsi->value.data.ptr, dsi->value.data.size);
			if (ctx->cfg_crc == ex_crc)
				return GF_OK;
			ctx->cfg_crc = ex_crc;
			//do not reset, use API
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[MPEGHDec] Reconfiguring but no DSI set, skipping\n"));
			return GF_OK;
		}
	} else {
		dsi = NULL;
	}

	if (!ctx->codec)
		ctx->codec = mpeghdecoder_init(ctx->cicp_idx);

	if (!ctx->codec) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[MPEGHDec] Error initializing decoder\n"));
		return GF_IO_ERR;
	}

	//init dec
	if (dsi) {
		//remove first 5 bytes of mhaC
		if (dsi->value.data.size>5) {
			MPEGH_DECODER_ERROR err = mpeghdecoder_setMhaConfig(ctx->codec, dsi->value.data.ptr+5, dsi->value.data.size-5);
			if (err != MPEGH_DEC_OK) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[MPEGHDec] Failed to set decoder params\n"));
				return GF_NON_COMPLIANT_BITSTREAM;
			}
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[MPEGHDec] Invalid decoder config, at least 5 bytes expected got %d\n", dsi->value.data.size));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
	}
	ctx->osize = MPEGH_MAX_FRAME_SIZE * ctx->num_channels;

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(ctx->sample_rate) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(ctx->num_channels) );

	gf_filter_pid_set_framing_mode(pid, GF_TRUE);
	return GF_OK;
}

static GF_Err mpegh_dec_process(GF_Filter *filter)
{
	MPEGHDecCtx *ctx = gf_filter_get_udta(filter);
	u32 pck_size;
	Bool is_eos = GF_FALSE;
	MPEGH_DECODER_ERROR err;
	GF_FilterPacket *dst_pck;
	GF_FilterPacket *pck = gf_filter_pid_get_packet(ctx->ipid);

	if (!pck) {
		is_eos = gf_filter_pid_is_eos(ctx->ipid);
		if (!is_eos) return GF_OK;
		//eos
		if (!ctx->flush_state) {
			err = mpeghdecoder_flushAndGet(ctx->codec);
			if (err != MPEGH_DEC_OK) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[MPEGHDec] Error flushing: %d\n", err));
			}
			ctx->flush_state = 1;
		} else if (ctx->flush_state == 2) {
			return GF_EOS;
		}
	} else {
		const u8 *data = gf_filter_pck_get_data(pck, &pck_size);
		u64 ts = gf_filter_pck_get_cts(pck);
		Bool is_seek = gf_filter_pck_get_seek_flag(pck);
		if (is_seek)
			mpeghdecoder_flush(ctx->codec);

		ts = gf_timestamp_rescale(ts, ctx->timescale, 1000000000);
		err = mpeghdecoder_process(ctx->codec, data, pck_size, ts);

		if (err == MPEGH_DEC_NEEDS_RESTART) {
			mpeghdecoder_destroy(ctx->codec);
			ctx->codec = NULL;
			//reconfig and keep packet
			return mpegh_dec_configure_pid(filter, ctx->ipid, GF_FALSE);
		}
		if (err != MPEGH_DEC_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[MPEGHDec] Error queuing frame: %d\n", err));
		}

		if (!is_seek) {
			gf_filter_pck_ref_props(&pck);
			gf_list_add(ctx->src_pcks, pck);
		}

		gf_filter_pid_drop_packet(ctx->ipid);
	}

	while (1) {
		u8 *output;
		MPEGH_DECODER_OUTPUT_INFO mphInfo;

		dst_pck = gf_filter_pck_new_alloc(ctx->opid, ctx->osize*4, &output);
		if (!dst_pck) return GF_OUT_OF_MEM;

		err = mpeghdecoder_getSamples(ctx->codec, (s32*) output, ctx->osize, &mphInfo);
		if (err != MPEGH_DEC_OK) {
			gf_filter_pck_discard(dst_pck);
			if (err == MPEGH_DEC_FEED_DATA) {
				if (is_eos) {
					gf_filter_pid_set_eos(ctx->opid);
					ctx->flush_state = 2;
					return GF_EOS;
				}
				return GF_OK;
			}
			if (err==MPEGH_DEC_BUFFER_ERROR) {
				if (ctx->osize == MPEGH_MAX_FRAME_SIZE*24) return GF_NON_COMPLIANT_BITSTREAM;
				ctx->osize += MPEGH_MAX_FRAME_SIZE;
				continue;
			}
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		if (!mphInfo.numSamplesPerChannel) {
			gf_filter_pck_discard(dst_pck);
			return GF_OK;
		}

		if (mphInfo.sampleRate != ctx->sample_rate) {
			ctx->sample_rate = mphInfo.sampleRate;
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(ctx->sample_rate) );
		}
		if (mphInfo.numChannels != ctx->num_channels) {
			ctx->num_channels = mphInfo.numChannels;
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(ctx->num_channels) );
		}
		gf_filter_pck_truncate(dst_pck, 4*mphInfo.numChannels*mphInfo.numSamplesPerChannel);

		//look for src with same timestamp, copy over properties
		u64 ts = gf_timestamp_rescale(mphInfo.pts, 1000000000, ctx->timescale);
		while (1) {
			pck = gf_list_get(ctx->src_pcks, 0);
			if (!pck) break;
			u64 src_ts = gf_filter_pck_get_cts(pck);
			s64 diff = gf_timestamp_rescale_signed( (s64) src_ts - (s64) ts, ctx->timescale, 1000);
			if (diff > -10) {
				if (!diff) ts = src_ts;
				break;
			}
			//ts too old, purge
			gf_filter_pck_unref(pck);
			gf_list_pop_front(ctx->src_pcks);
			pck = NULL;
		}
		if (pck)
			gf_filter_pck_merge_properties(pck, dst_pck);

		gf_filter_pck_set_cts(dst_pck, ts);
		gf_filter_pck_set_dts(dst_pck, ts);
		gf_filter_pck_send(dst_pck);
	}
	return GF_OK;
}

static Bool mpegh_dec_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	if (evt->base.type==GF_FEVT_STOP) {
		MPEGHDecCtx *ctx = gf_filter_get_udta(filter);
		if (ctx->codec) mpeghdecoder_flush(ctx->codec);
		ctx->flush_state = 0;
		while (gf_list_count(ctx->src_pcks)) {
			GF_FilterPacket *pck = gf_list_pop_back(ctx->src_pcks);
			gf_filter_pck_unref(pck);
		}
	}
	return GF_FALSE;
}

static GF_Err mpegh_dec_initialize(GF_Filter *filter)
{
	MPEGHDecCtx *ctx = gf_filter_get_udta(filter);
	ctx->src_pcks = gf_list_new();
	return GF_OK;
}

static void mpegh_dec_finalize(GF_Filter *filter)
{
	MPEGHDecCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->codec) mpeghdecoder_destroy(ctx->codec);
	while (gf_list_count(ctx->src_pcks)) {
		GF_FilterPacket *pck = gf_list_pop_back(ctx->src_pcks);
		gf_filter_pck_unref(pck);
	}
	gf_list_del(ctx->src_pcks);
}

#define OFFS(_n)	#_n, offsetof(MPEGHDecCtx, _n)
static GF_FilterArgs MPEGHDArgs[] =
{
	{ OFFS(cicp_idx), "target output layout CICP index (see gpac -h layouts)", GF_PROP_UINT, "2", NULL, 0},
	{0}
};

static const GF_FilterCapability MPEGHDCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPHA),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MHAS),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

GF_FilterRegister MPEGHDRegister = {
	.name = "mpeghdec",
	GF_FS_SET_DESCRIPTION("MPEG-H Audio decoder")
	GF_FS_SET_HELP("This filter decodes MPEG-H audio streams through IIS MpeghDecoder library.")
	.private_size = sizeof(MPEGHDecCtx),
	SETCAPS(MPEGHDCaps),
	.args = MPEGHDArgs,
	.configure_pid = mpegh_dec_configure_pid,
	.initialize = mpegh_dec_initialize,
	.finalize = mpegh_dec_finalize,
	.process = mpegh_dec_process,
	.process_event = mpegh_dec_process_event,
};

const GF_FilterRegister *mpeghdec_register(GF_FilterSession *session)
{
	return &MPEGHDRegister;
}
#else
const GF_FilterRegister *mpeghdec_register(GF_FilterSession *session)
{
	return NULL;
}
#endif //GPAC_HAS_MPEGHDECODER

