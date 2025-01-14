/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Deniz Ugur
 *			Copyright (c) Telecom ParisTech 2023-2024
 *					All rights reserved
 *
 *  This file is part of GPAC / Closed captions encoder filter
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
#include <gpac/tools.h>
#include <gpac/avparse.h>
#include <gpac/internal/media_dev.h>

#ifdef GPAC_HAS_LIBCAPTION
#include <caption/mpeg.h>
#include <caption/caption.h>

#if defined(WIN32) || defined(_WIN32_WCE)
#if !defined(__GNUC__)
#pragma comment(lib, "libcaption")
#endif
#endif

enum
{
	CCTYPE_UNK=0,
	CCTYPE_M2V,
	CCTYPE_M4V,
	CCTYPE_OBU,
	CCTYPE_AVC,
	CCTYPE_HEVC,
	CCTYPE_VVC,
};

typedef struct
{
	GF_FilterPid *vipid; // video input pid
	GF_FilterPid *sipid; // subtitle input pid
	GF_FilterPid *opid;
	u32 v_ts, s_ts;

	u32 cctype;
	caption_frame_t *ccframe;
	sei_t *sei;

	GF_Fraction vb_time;
	u32 nalu_size_len;
	GF_List *cc_queue;
	GF_List *frame_queue;

	Bool is_cc_eos;

	GF_BitStream *bs;
} CCEncCtx;


GF_Err ccenc_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	CCEncCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPid *out_pid;
	const GF_PropertyValue *prop;

	if (is_remove) {
		out_pid = gf_filter_pid_get_udta(pid);
		if (out_pid==ctx->opid)
			ctx->opid = NULL;
		if (out_pid)
			gf_filter_pid_remove(out_pid);
		return GF_OK;
	}

	if (!gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		if (!ctx->opid) return GF_OUT_OF_MEM;
		gf_filter_pid_set_udta(pid, ctx->opid);
	}

	// get the stream type
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (!prop) return GF_BAD_PARAM;
	u32 stream_type = prop->value.uint;

	if (stream_type == GF_STREAM_TEXT) {
		ctx->sipid = pid;
		gf_filter_pid_set_framing_mode(pid, GF_TRUE);

		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
		ctx->s_ts = prop ? prop->value.uint : 1000;
		return GF_OK;
	}

	ctx->vipid = pid;
	gf_filter_pid_copy_properties(ctx->opid, pid);

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	ctx->v_ts = prop ? prop->value.uint : 1000;

	ctx->cctype = CCTYPE_UNK;
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!prop) return GF_BAD_PARAM;
	switch (prop->value.uint) {
#ifndef GPAC_DISABLE_AV_PARSERS
	case GF_CODECID_MPEG2_422:
	case GF_CODECID_MPEG2_SNR:
	case GF_CODECID_MPEG2_HIGH:
	case GF_CODECID_MPEG2_MAIN:
	case GF_CODECID_MPEG2_SIMPLE:
	case GF_CODECID_MPEG2_SPATIAL:
		ctx->cctype = CCTYPE_M2V;
		break;
	case GF_CODECID_MPEG4_PART2:
		ctx->cctype = CCTYPE_M4V;
		break;
#endif
	case GF_CODECID_AV1:
		ctx->cctype = CCTYPE_OBU;
		break;
	case GF_CODECID_AVC:
		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
		if (prop) {
			GF_AVCConfig *avcc = gf_odf_avc_cfg_read(prop->value.data.ptr, prop->value.data.size);
			if (avcc) {
				ctx->nalu_size_len = avcc->nal_unit_size;
				gf_odf_avc_cfg_del(avcc);
				ctx->cctype = CCTYPE_AVC;
			} else {
				return GF_NON_COMPLIANT_BITSTREAM;
			}
		} else {
			return GF_OK;
		}
		break;
	case GF_CODECID_HEVC:
		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
		if (prop) {
			GF_HEVCConfig *hvcc = gf_odf_hevc_cfg_read(prop->value.data.ptr, prop->value.data.size, GF_FALSE);
			if (hvcc) {
				ctx->nalu_size_len = hvcc->nal_unit_size;
				gf_odf_hevc_cfg_del(hvcc);
				ctx->cctype = CCTYPE_HEVC;
			} else {
				return GF_NON_COMPLIANT_BITSTREAM;
			}
		} else {
			return GF_OK;
		}
		break;
	case GF_CODECID_VVC:
		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
		if (prop) {
			GF_VVCConfig *vvcc = gf_odf_vvc_cfg_read(prop->value.data.ptr, prop->value.data.size);
			if (vvcc) {
				ctx->nalu_size_len = vvcc->nal_unit_size;
				gf_odf_vvc_cfg_del(vvcc);
				ctx->cctype = CCTYPE_VVC;
			} else {
				return GF_NON_COMPLIANT_BITSTREAM;
			}
		} else {
			return GF_OK;
		}
		break;
	}

	if (ctx->cctype == CCTYPE_UNK) return GF_NOT_SUPPORTED;
	return GF_OK;
}

static void ccenc_pair(GF_Filter *filter, GF_FilterPacket *vpck, GF_FilterPacket *spck)
{
	CCEncCtx *ctx = gf_filter_get_udta(filter);
	GF_Err err = GF_OK;
	libcaption_stauts_t status;

	// Create the caption frame
	const u8 *data = gf_filter_pck_get_data(spck, NULL);
	if (!ctx->ccframe) GF_SAFEALLOC(ctx->ccframe, caption_frame_t);
	status = caption_frame_from_text(ctx->ccframe, (const utf8_char_t*)data);
	if (status != LIBCAPTION_OK) {
		err = GF_BAD_PARAM;
		goto error;
	}

	// Create the SEI from the caption frame
	if (!ctx->sei) GF_SAFEALLOC(ctx->sei, sei_t);
	sei_free(ctx->sei); // also inits the sei
	ctx->sei->timestamp = gf_filter_pck_get_cts(vpck) / (double)ctx->v_ts;
	status = sei_from_caption_frame(ctx->sei, ctx->ccframe);
	if (status != LIBCAPTION_OK) {
		err = GF_BAD_PARAM;
		goto error;
	}

	// Create the NALU from the SEI
	u8 nhdr_extra = (ctx->cctype==CCTYPE_HEVC || ctx->cctype==CCTYPE_VVC) ? 1 : 0;
	size_t nal_size = sei_render_size(ctx->sei) + nhdr_extra + ctx->nalu_size_len;
	u8 *nal_data = gf_malloc(nal_size);
	if (!nal_data) {
		err = GF_OUT_OF_MEM;
		goto error;
	}
	memset(nal_data, 0, nal_size);
	sei_render(ctx->sei, nal_data + nhdr_extra + ctx->nalu_size_len);

	// Modify the NALU header
	((u32*)nal_data)[0] = (u32)(nal_size - ctx->nalu_size_len);
	if (ctx->cctype==CCTYPE_HEVC) {
		((u16*)nal_data)[2] = (u16)GF_HEVC_NALU_SEI_PREFIX;
	} else if (ctx->cctype==CCTYPE_VVC) {
		((u16*)nal_data)[2] = (u16)GF_VVC_NALU_SEI_PREFIX;
	} else {
		nal_data[4] = GF_AVC_NALU_SEI;
	}

	// Get the video data
	u32 vsize;
	const u8 *vdata = gf_filter_pck_get_data(vpck, &vsize);

	// Create the new video packet
	u8* new_data = NULL;
	GF_FilterPacket *new_vpck = gf_filter_pck_new_alloc(ctx->opid, vsize + nal_size, &new_data);
	if (!new_vpck) {
		err = GF_OUT_OF_MEM;
		goto error;
	}
	gf_filter_pck_merge_properties(vpck, new_vpck);

	// Prepend the NALU to the video data
	memcpy(new_data, nal_data, nal_size);
	memcpy(new_data + nal_size, vdata, vsize);

error:
	if (nal_data) gf_free(nal_data);

	// if we have an error, just forward the video frame
	if (err != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[ccenc] Error encountered while encoding caption, forwarding video frame: %s\n", gf_error_to_string(err)));
		gf_filter_pck_forward(vpck, ctx->opid);
	}

	gf_filter_pck_unref(vpck);
	gf_filter_pck_unref(spck);
}

static void ccenc_flush(GF_Filter *filter, Bool full_flush)
{
	CCEncCtx *ctx = gf_filter_get_udta(filter);
	if (gf_list_count(ctx->cc_queue)==0 && gf_list_count(ctx->frame_queue)==0) return;

retry:
	// try to pair video and subtitle data, gracefully
	while (gf_list_count(ctx->cc_queue) && gf_list_count(ctx->frame_queue)) {
		// if we have a video frame with cts lower than the minimum subtitle cts, send it
		u32 last_video_cts = gf_filter_pck_get_cts(gf_list_last(ctx->frame_queue));
		u32 last_subtitle_cts = gf_filter_pck_get_cts(gf_list_last(ctx->cc_queue));
		if (gf_timestamp_less(last_video_cts, ctx->v_ts, last_subtitle_cts, ctx->s_ts)) {
			// impossible to pair, send video frame
			GF_FilterPacket *vpck = gf_list_pop_back(ctx->frame_queue);
			gf_filter_pck_forward(vpck, ctx->opid);
			gf_filter_pck_unref(vpck);
			continue;
		}

		// we might have a caption for the current video frame, search it
		Bool found = GF_FALSE;
		for (u32 i=gf_list_count(ctx->cc_queue); i--;) {
			GF_FilterPacket *spck = gf_list_get(ctx->cc_queue, i);
			if (gf_timestamp_equal(last_video_cts, ctx->v_ts, gf_filter_pck_get_cts(spck), ctx->s_ts)) {
				found = GF_TRUE;
				ccenc_pair(filter, gf_list_pop_back(ctx->frame_queue), spck);
				gf_list_del_item(ctx->cc_queue, spck);
				break;
			}
		}

		// we couldn't pair this video frame with a subtitle frame gracefully
		if (!found) break;
	}

	// if there are no more video frames, there is nothing to do
	if (gf_list_count(ctx->frame_queue)==0) return;

	// flush the video frames based on buffer time
	u32 last_dts = gf_filter_pck_get_dts(gf_list_last(ctx->frame_queue));
	u32 first_dts = gf_filter_pck_get_dts(gf_list_get(ctx->frame_queue, 0));
	if (gf_timestamp_greater_or_equal(first_dts-last_dts, ctx->v_ts, ctx->vb_time.num, ctx->vb_time.den)) {
		// we couldn't pair this video frame with a subtitle frame
		GF_FilterPacket *vpck = gf_list_pop_back(ctx->frame_queue);
		gf_filter_pck_forward(vpck, ctx->opid);
		gf_filter_pck_unref(vpck);

		// we might be able to pair the next video frame with a subtitle frame
		goto retry;
	}

	if (full_flush) {
		// we've tried to pair all video frames with subtitle frames, but we still have video frames left
		while (gf_list_count(ctx->frame_queue)) {
			GF_FilterPacket *vpck = gf_list_pop_back(ctx->frame_queue);
			gf_filter_pck_forward(vpck, ctx->opid);
			gf_filter_pck_unref(vpck);
		}
	}
}

GF_Err ccenc_process(GF_Filter *filter)
{
	CCEncCtx *ctx = gf_filter_get_udta(filter);

	if (gf_filter_connections_pending(filter))
		return GF_OK;

	if (!ctx->vipid || !ctx->sipid) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[ccenc] Missing %s input\n", ctx->vipid ? "subtitle" : "video"));
		return GF_BAD_PARAM;
	}

	// check if we have subtitle data
	GF_FilterPacket *spck = gf_filter_pid_get_packet(ctx->sipid);
	if (spck) {
		if (!gf_filter_pck_is_blocking_ref(spck)) {
			gf_filter_pck_ref(&spck);
			gf_filter_pid_drop_packet(ctx->sipid);
			gf_list_add(ctx->cc_queue, spck);
		}
	}
	else if (gf_filter_pid_is_eos(ctx->sipid)) {
		ctx->is_cc_eos = GF_TRUE;
	}

	// check if we have video data
	GF_FilterPacket *vpck = gf_filter_pid_get_packet(ctx->vipid);
	if (vpck) {
		// if no more subtitle data, we can just forward video data
		if (ctx->is_cc_eos && gf_list_count(ctx->cc_queue)==0) {
			ccenc_flush(filter, GF_TRUE);
			gf_filter_pid_drop_packet(ctx->vipid);
			return gf_filter_pck_forward(vpck, ctx->opid);
		}

		if (!gf_filter_pck_is_blocking_ref(vpck)) {
			gf_filter_pck_ref(&vpck);
			gf_filter_pid_drop_packet(ctx->vipid);
			gf_list_add(ctx->frame_queue, vpck);
		}
	}
	else if (gf_filter_pid_is_eos(ctx->vipid)) {
		ccenc_flush(filter, GF_TRUE);
		if (gf_list_count(ctx->cc_queue)>0) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("[ccenc] EOS reached on video input, but subtitle data is still available. Discarding remaining subtitle data\n"));
			while (gf_list_count(ctx->cc_queue)) {
				GF_FilterPacket *pck = gf_list_pop_back(ctx->cc_queue);
				gf_filter_pck_unref(pck);
			}
		}
		gf_filter_pid_set_eos(ctx->opid);
		return GF_EOS;
	}

	// TODO: gf_filter_pck_is_blocking_ref. If blocking, then try to pair the two here, otherwise warn and send the video frame
	if ((vpck && gf_filter_pck_is_blocking_ref(vpck)) || (spck && gf_filter_pck_is_blocking_ref(spck))) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("[ccenc] Blocking reference detected. This is not supported yet\n"));
		return GF_NOT_SUPPORTED;
	}

	// Try to flush after we tried to get both video and subtitle data
	ccenc_flush(filter, GF_FALSE);

	return GF_OK;
}

static GF_Err ccenc_initialize(GF_Filter *filter)
{
	CCEncCtx *ctx = gf_filter_get_udta(filter);
	ctx->is_cc_eos = GF_FALSE;
	ctx->cc_queue = gf_list_new();
	if (!ctx->cc_queue) return GF_OUT_OF_MEM;
	ctx->frame_queue = gf_list_new();
	if (!ctx->frame_queue) return GF_OUT_OF_MEM;
	return GF_OK;
}

static void ccenc_finalize(GF_Filter *filter)
{
	CCEncCtx *ctx = gf_filter_get_udta(filter);
	gf_assert(gf_list_count(ctx->cc_queue)==0);
	gf_list_del(ctx->cc_queue);
	gf_assert(gf_list_count(ctx->frame_queue)==0);
	gf_list_del(ctx->frame_queue);
	if (ctx->ccframe) gf_free(ctx->ccframe);
	if (ctx->sei) {
		sei_free(ctx->sei);
		gf_free(ctx->sei);
	}
	if (ctx->bs) gf_bs_del(ctx->bs);
}

static const GF_FilterCapability CCEncCaps[] =
{
	// supported video input
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),

	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_AVC),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_VVC),

	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_BOOL(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),

	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_TEXT_MPEG4),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_TX3G),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_SUBS_TEXT),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_SIMPLE_TEXT),
	{0},
};

#define OFFS(_n)	#_n, offsetof(CCEncCtx, _n)
static const GF_FilterArgs CCEncArgs[] =
{
	{ OFFS(vb_time), "time to hold video packets if no caption is available", GF_PROP_FRACTION, "1/1", NULL, GF_FS_ARG_HINT_EXPERT},
	{0}
};

GF_FilterRegister CCEncRegister = {
	.name = "ccenc",
	GF_FS_SET_DESCRIPTION("Closed-Caption encoder")
	GF_FS_SET_HELP("This filter encodes Closed Captions to the video stream.\n"
	"Supported video media types are MPEG2, AVC, HEVC, VVC and AV1 streams.\n"
	"\nOnly a subset of CEA 608/708 is supported.")
	.private_size = sizeof(CCEncCtx),
	.args = CCEncArgs,
	.flags = GF_FS_REG_DYNAMIC_REDIRECT,
	SETCAPS(CCEncCaps),
	.initialize = ccenc_initialize,
	.finalize = ccenc_finalize,
	.process = ccenc_process,
	.configure_pid = ccenc_configure_pid,
	.hint_class_type = GF_FS_CLASS_SUBTITLE
};

const GF_FilterRegister *ccenc_register(GF_FilterSession *session)
{
	return &CCEncRegister;
}
#else //GPAC_HAS_LIBCAPTION
const GF_FilterRegister *ccenc_register(GF_FilterSession *session)
{
	return NULL;
}
#endif // GPAC_DISABLE_CCDEC
