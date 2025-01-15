/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Deniz Ugur
 *			Copyright (c) Motion Spell 2025
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

#if defined(WIN32) || defined(_WIN32_WCE)
#if !defined(__GNUC__)
#pragma comment(lib, "libcaption")
#endif
#endif

enum
{
	CCTYPE_UNK=0,
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

	Bool is_cc_eos;
	u32 cctype;
	caption_frame_t *ccframe;
	sei_t *sei;
	u32 cc_data_offset;

	GF_Fraction vb_time;
	u32 nalu_size_len;

	GF_List *cc_queue;
	GF_List *frame_queue;
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

	// get the codec id
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!prop) return GF_BAD_PARAM;
	u32 codec_id = prop->value.uint;

	// get the stream type
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (!prop) return GF_BAD_PARAM;
	u32 stream_type = prop->value.uint;

	if (stream_type == GF_STREAM_TEXT) {
		ctx->sipid = pid;
		gf_filter_pid_set_framing_mode(pid, GF_TRUE);

		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
		ctx->s_ts = prop ? prop->value.uint : 1000;

		switch (codec_id)
		{
		case GF_CODECID_TX3G:
			ctx->cc_data_offset = 2;
			break;
		case GF_CODECID_WEBVTT:
			ctx->cc_data_offset = 16;
			break;
		default:
			break;
		}
		return GF_OK;
	}

	ctx->vipid = pid;
	gf_filter_pid_copy_properties(ctx->opid, pid);
	gf_filter_pid_set_framing_mode(pid, GF_TRUE);

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	ctx->v_ts = prop ? prop->value.uint : 1000;

	ctx->cctype = CCTYPE_UNK;
	switch (codec_id) {
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
	GF_BitStream *bs;
	libcaption_stauts_t status;
	u8 *sei_data = NULL;
	u32 size;

#define CHECK_OOM(_x) if (!_x) { err = GF_OUT_OF_MEM; goto error; }

	// Create the caption frame
	const u8 *data = gf_filter_pck_get_data(spck, &size);
	if (!ctx->ccframe) GF_SAFEALLOC(ctx->ccframe, caption_frame_t);
	caption_frame_from_text(ctx->ccframe, (const utf8_char_t*)data+ctx->cc_data_offset);

	// Create the SEI from the caption frame
	if (!ctx->sei) GF_SAFEALLOC(ctx->sei, sei_t);
	sei_free(ctx->sei); // also inits the sei
	ctx->sei->timestamp = gf_filter_pck_get_cts(spck) / (double)ctx->s_ts;
	status = sei_from_caption_frame(ctx->sei, ctx->ccframe);
	if (status != LIBCAPTION_OK) {
		err = GF_BAD_PARAM;
		goto error;
	}

	// Render the SEI
	sei_data = gf_malloc(sei_render_size(ctx->sei));
	CHECK_OOM(sei_data);
	size_t sei_render_size = sei_render(ctx->sei, sei_data);

	// Add EBP to the SEI
	// sei_render_size includes nal_type (1 byte)
	u32 nb_bytes_to_add = gf_media_nalu_emulation_bytes_add_count(sei_data+1, sei_render_size-1);
	u32 sei_payload_size = sei_render_size - 1 + nb_bytes_to_add;
	u8 *sei_data_with_epb = gf_malloc(sei_payload_size);
	CHECK_OOM(sei_data_with_epb);
	gf_media_nalu_add_emulation_bytes(sei_data+1, sei_data_with_epb, sei_render_size-1);
	gf_free(sei_data);

	// Prepare the NALU writer
	u8 nhdr_type_len = (ctx->cctype==CCTYPE_HEVC || ctx->cctype==CCTYPE_VVC) ? 2 : 1;
	size_t nal_size = ctx->nalu_size_len + nhdr_type_len + sei_payload_size;
	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	CHECK_OOM(bs);

	// Write the NALU header
	gf_bs_write_int(bs, nal_size - ctx->nalu_size_len, ctx->nalu_size_len*8); // nal_size

	// Write the NALU type
	if (ctx->cctype==CCTYPE_HEVC) {
		gf_bs_write_int(bs, 0, 1);
		gf_bs_write_int(bs, GF_HEVC_NALU_SEI_PREFIX, 6);
		gf_bs_write_int(bs, 0, 6);
		gf_bs_write_int(bs, 1, 3);
	} else if (ctx->cctype==CCTYPE_VVC) {
		gf_bs_write_int(bs, 0, 1);
		gf_bs_write_int(bs, GF_VVC_NALU_SEI_PREFIX, 6);
		gf_bs_write_int(bs, 0, 6);
		gf_bs_write_int(bs, 1, 3);
	} else {
		gf_bs_write_int(bs, 0, 1);
		gf_bs_write_int(bs, 0, 2);
		gf_bs_write_int(bs, GF_AVC_NALU_SEI, 5);
	}

	// Write the SEI
	gf_bs_write_data(bs, sei_data_with_epb, sei_payload_size);
	gf_free(sei_data_with_epb);

	// Write rest of the video data
	const u8 *vdata = gf_filter_pck_get_data(vpck, &size);
	gf_bs_write_data(bs, vdata, size);

	// Check the size
	u32 new_size = nal_size + size;
	gf_assert(new_size == gf_bs_get_position(bs));

	// Create the new video packet
	u8 *new_data = NULL;
	GF_FilterPacket *new_vpck = gf_filter_pck_new_alloc(ctx->opid, new_size, &new_data);
	CHECK_OOM(new_vpck);
	gf_filter_pck_merge_properties(vpck, new_vpck);

	// Copy the data
	gf_bs_get_content(bs, &new_data, &size);
	gf_assert(size == new_size);

	// Send the new packet
	gf_filter_pck_send(new_vpck);

#undef CHECK_OOM

error:
	if (bs) gf_bs_del(bs);

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
		u32 last_video_cts = gf_filter_pck_get_cts(gf_list_get(ctx->frame_queue, 0));
		u32 last_subtitle_cts = gf_filter_pck_get_cts(gf_list_get(ctx->cc_queue, 0));
		if (gf_timestamp_less(last_video_cts, ctx->v_ts, last_subtitle_cts, ctx->s_ts)) {
			// impossible to pair, send video frame
			GF_FilterPacket *vpck = gf_list_pop_front(ctx->frame_queue);
			gf_filter_pck_forward(vpck, ctx->opid);
			gf_filter_pck_unref(vpck);
			continue;
		}

		// we might have a caption for the current video frame, search it
		Bool found = GF_FALSE;
		for (u32 i=0; i<gf_list_count(ctx->cc_queue);i++) {
			GF_FilterPacket *spck = gf_list_get(ctx->cc_queue, i);
			if (gf_timestamp_equal(last_video_cts, ctx->v_ts, gf_filter_pck_get_cts(spck), ctx->s_ts)) {
				found = GF_TRUE;
				ccenc_pair(filter, gf_list_pop_front(ctx->frame_queue), spck);
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
	u32 old_dts = gf_filter_pck_get_dts(gf_list_get(ctx->frame_queue, 0));
	u32 new_dts = gf_filter_pck_get_dts(gf_list_last(ctx->frame_queue));
	if (gf_timestamp_greater_or_equal(new_dts-old_dts, ctx->v_ts, ctx->vb_time.num, ctx->vb_time.den)) {
		// we couldn't pair this video frame with a subtitle frame
		GF_FilterPacket *vpck = gf_list_pop_front(ctx->frame_queue);
		gf_filter_pck_forward(vpck, ctx->opid);
		gf_filter_pck_unref(vpck);

		// we might be able to pair the next video frame with a subtitle frame
		goto retry;
	}

	if (full_flush) {
		// we've tried to pair all video frames with subtitle frames, but we still have video frames left
		while (gf_list_count(ctx->frame_queue)) {
			GF_FilterPacket *vpck = gf_list_pop_front(ctx->frame_queue);
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
			gf_list_add(ctx->cc_queue, spck);
		}
		gf_filter_pid_drop_packet(ctx->sipid);
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
			gf_filter_pck_forward(vpck, ctx->opid);
			gf_filter_pid_drop_packet(ctx->vipid);
			return GF_OK;
		}

		if (!gf_filter_pck_is_blocking_ref(vpck)) {
			gf_filter_pck_ref(&vpck);
			gf_list_add(ctx->frame_queue, vpck);
		}
		gf_filter_pid_drop_packet(ctx->vipid);
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
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_TX3G),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_WEBVTT)
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
	.max_extra_pids = (u32) -1,
	.args = CCEncArgs,
	.flags = GF_FS_REG_EXPLICIT_ONLY,
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
