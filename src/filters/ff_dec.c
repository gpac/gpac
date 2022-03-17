/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / ffmpeg decode filter
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

#include <gpac/setup.h>

#ifdef GPAC_HAS_FFMPEG

#include "ff_common.h"

#include <libswscale/swscale.h>

#define FF_CHECK_PROP(_name, _ffname, _type)	if (ctx->_name != ctx->decoder->_ffname) { \
		gf_filter_pid_set_property(ctx->out_pid, _type, &PROP_UINT( (u32) ctx->decoder->_ffname ) );	\
		ctx->_name = (u32) ctx->decoder->_ffname;	\
	} \

#define FF_CHECK_PROPL(_name, _ffname, _type)	if (ctx->_name != ctx->decoder->_ffname) { \
	gf_filter_pid_set_property(ctx->out_pid, _type, &PROP_LONGUINT( (u32) ctx->decoder->_ffname ) );	\
	ctx->_name = (u32) ctx->decoder->_ffname;	\
} \

#define FF_CHECK_PROP_VAL(_name, _val, _type)	if (ctx->_name != _val) { \
		gf_filter_pid_set_property(ctx->out_pid, _type, &PROP_UINT( _val ) );	\
		ctx->_name = _val;	\
	} \

static GF_Err ffdec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove);


typedef struct _gf_ffdec_ctx
{
	GF_PropStringList ffcmap;

	//internal data
	Bool initialized;

	Bool owns_context;
	AVCodecContext *decoder;
	//decode options
	AVDictionary *options;

	Bool reconfig_pending;

	GF_FilterPid *in_pid, *out_pid;
	//media type
	u32 type;
	//CRC32 of extra_data
	u32 extra_data_crc;

	GF_Err (*process)(GF_Filter *filter, struct _gf_ffdec_ctx *ctx);

	u32 flush_done;

	//for now we don't share the data
	AVFrame *frame;
	//audio state
	u32 channels, sample_rate, sample_fmt, bytes_per_sample;
	u64 channel_layout;
#if (LIBAVCODEC_VERSION_MAJOR < 59)
	u32 frame_start;
#endif
	u32 nb_samples_already_in_frame;

	//video state
	u32 width, height, pixel_fmt, stride, stride_uv;
	GF_Fraction sar;
	struct SwsContext *sws_ctx;

	GF_List *src_packets;

	//only used to check decoder output change
	u32 o_ff_pfmt;
	Bool force_full_range;
	Bool drop_non_refs;

	s64 first_cts_plus_one;
	s64 delay;
	u32 ts_offset;

	Bool force_rap_wait;

#if (LIBAVCODEC_VERSION_MAJOR < 59)
	AVPacket pkt;
#else
	AVPacket *pkt;
#endif

} GF_FFDecodeCtx;

static GF_Err ffdec_initialize(GF_Filter *filter)
{
	GF_FFDecodeCtx *ctx = (GF_FFDecodeCtx *) gf_filter_get_udta(filter);
	ctx->initialized = GF_TRUE;
	ctx->src_packets = gf_list_new();

#if (LIBAVCODEC_VERSION_MAJOR >= 59)
	ctx->pkt = av_packet_alloc();
#endif

	ffmpeg_setup_logs(GF_LOG_CODEC);
	return GF_OK;
}

static void ffdec_finalize(GF_Filter *filter)
{
	GF_FFDecodeCtx *ctx = (GF_FFDecodeCtx *) gf_filter_get_udta(filter);

	if (ctx->options) av_dict_free(&ctx->options);
	if (ctx->frame) av_frame_free(&ctx->frame);
	if (ctx->sws_ctx) sws_freeContext(ctx->sws_ctx);

	while (gf_list_count(ctx->src_packets)) {
		GF_FilterPacket *pck = gf_list_pop_back(ctx->src_packets);
		gf_filter_pck_unref(pck);
	}
	gf_list_del(ctx->src_packets);

#if (LIBAVCODEC_VERSION_MAJOR >= 59)
	av_packet_free(&ctx->pkt);
#endif

	if (ctx->owns_context && ctx->decoder) {
		avcodec_free_context(&ctx->decoder);
	}
	return;
}

static void ffdec_check_pix_fmt_change(struct _gf_ffdec_ctx *ctx, u32 pix_fmt)
{
	if (ctx->pixel_fmt != pix_fmt) {
		gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_PIXFMT, &PROP_UINT(pix_fmt));
		ctx->pixel_fmt = pix_fmt;
		ctx->force_full_range = GF_FALSE;
		if (ctx->decoder->color_range==AVCOL_RANGE_JPEG)
			ctx->force_full_range = GF_TRUE;
		else if (ffmpeg_pixfmt_is_fullrange(ctx->decoder->pix_fmt))
			ctx->force_full_range = GF_TRUE;
		else
			ctx->force_full_range = GF_FALSE;

		if (ctx->force_full_range)
			gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_COLR_RANGE, &PROP_BOOL(GF_TRUE) );
		else
			gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_COLR_RANGE, NULL );

		if (ctx->decoder->color_primaries!=AVCOL_PRI_UNSPECIFIED)
			gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_COLR_PRIMARIES, &PROP_UINT(ctx->decoder->color_primaries) );
		else
			gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_COLR_PRIMARIES, NULL );

		if (ctx->decoder->colorspace!=AVCOL_SPC_UNSPECIFIED)
			gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_COLR_MX, &PROP_UINT(ctx->decoder->colorspace) );
		else
			gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_COLR_MX, NULL );

		if (ctx->decoder->color_trc!=AVCOL_TRC_UNSPECIFIED)
			gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_COLR_TRANSFER, &PROP_UINT(ctx->decoder->color_trc) );
		else
			gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_COLR_TRANSFER, NULL );
	}
}

static GF_Err ffdec_process_video(GF_Filter *filter, struct _gf_ffdec_ctx *ctx)
{
	AVPacket *pkt;
	AVFrame *frame;
	u32 dst_stride[5];
	u8 *dst_planes[5];
	u32 pix_out;
	Bool is_eos=GF_FALSE;
	s32 res;
	s32 gotpic;
	u64 out_cts;
	const char *data = NULL;
	Bool seek_flag = GF_FALSE;
	u32 i, count, ff_pfmt;
	u32 size=0, outsize, stride, stride_uv, uv_height, nb_planes;
	u8 *out_buffer;
	GF_FilterPacket *pck_src;
	GF_FilterPacket *dst_pck;
	GF_FilterPacket *pck = gf_filter_pid_get_packet(ctx->in_pid);

	if (pck && ctx->force_rap_wait) {
		GF_FilterSAPType sap = gf_filter_pck_get_sap(pck);
		if (sap && (sap<=GF_FILTER_SAP_4)) {
			ctx->force_rap_wait = GF_FALSE;
		} else {
			gf_filter_pid_drop_packet(ctx->in_pid);
			return GF_OK;
		}
	}
	if (ctx->reconfig_pending) {
		pck = NULL;
	} else if (!pck) {
		is_eos = gf_filter_pid_is_eos(ctx->in_pid);
		if (!is_eos) return GF_OK;
	}

	if (pck && ctx->drop_non_refs && !gf_filter_pck_get_sap(pck)) {
		gf_filter_pid_drop_packet(ctx->in_pid);
		return GF_OK;
	}
    //we don't own the codec and we're in end of stream, don't try to decode (the context might have been closed)
    if (!pck && !ctx->owns_context) {
        gf_filter_pid_set_eos(ctx->out_pid);
        return GF_EOS;
    }

	frame = ctx->frame;

	FF_INIT_PCK(ctx, pkt)

	if (pck) {
		data = gf_filter_pck_get_data(pck, &size);

		if (!size) {
			gf_filter_pid_drop_packet(ctx->in_pid);
			FF_RELEASE_PCK(pkt)
			return GF_OK;
		}

		pck_src = pck;
		gf_filter_pck_ref_props(&pck_src);
		if (pck_src) gf_list_add(ctx->src_packets, pck_src);

		//seems ffmpeg is not properly handling the decoding after a flush, we close and reopen the codec
		if (ctx->flush_done) {
#if 0
			AVDictionary *options = NULL;
			const AVCodec *codec = ctx->decoder->codec;
			avcodec_free_context(&ctx->decoder);

			av_dict_copy(&options, ctx->options, 0);
			avcodec_open2(ctx->decoder, codec, &options );
			if (options) av_dict_free(&options);
#else
			avcodec_flush_buffers(ctx->decoder);
#endif
			ctx->flush_done = GF_FALSE;
		}

		pkt->dts = gf_filter_pck_get_dts(pck);
		pkt->pts = gf_filter_pck_get_cts(pck);
		pkt->duration = gf_filter_pck_get_duration(pck);
		if (gf_filter_pck_get_sap(pck)>0)
			pkt->flags = AV_PKT_FLAG_KEY;
	}
	pkt->data = (uint8_t*)data;
	pkt->size = size;

	/*TOCHECK: for AVC bitstreams after ISMA decryption, in case (as we do) the decryption DRM tool
	doesn't put back nalu size, we have to do it ourselves, but we can't modify input data...*/

	gotpic=0;
#if (LIBAVCODEC_VERSION_MAJOR < 59)
	res = avcodec_decode_video2(ctx->decoder, frame, &gotpic, pkt);
#else
	res = avcodec_send_packet(ctx->decoder, pkt);
	switch (res) {
	case AVERROR(EAGAIN):
		if (pck_src) {
			gf_list_del_item(ctx->src_packets, pck_src);
			gf_filter_pck_unref(pck_src);
		}
		FF_RELEASE_PCK(pkt);
		//input full but output available, go on but don't discard input
		pck = NULL;
		break;
	case 0:
	case AVERROR_EOF:
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FFDec] PID %s failed to send frame PTS "LLU": %s\n", gf_filter_pid_get_name(ctx->in_pid), pkt->pts, av_err2str(res) ));
		break;
	}
	gotpic = 0;
	res = avcodec_receive_frame(ctx->decoder, frame);
	switch (res) {
	case AVERROR(EAGAIN):
		res = 0;
		break;
	case 0:
		gotpic = 1;
		break;
	case AVERROR_EOF:
		res = 0;
		break;
	}
#endif


	if (pck) gf_filter_pid_drop_packet(ctx->in_pid);


	if (!gotpic) {
		FF_RELEASE_PCK(pkt);
		if (is_eos) {
			ctx->flush_done = GF_TRUE;
			gf_filter_pid_set_eos(ctx->out_pid);
			return GF_EOS;
		}
		if (ctx->reconfig_pending) {
			GF_Err e;
			avcodec_free_context(&ctx->decoder);
			ctx->decoder = NULL;
			ctx->reconfig_pending = GF_FALSE;
			//these properties are checked after decode, when we reconfigure we copy props from input to output
			//so we need to make sure we retrigger pid config even if these did not change
			ctx->pixel_fmt = 0;
			ctx->width = 0;
			ctx->height = 0;
			ctx->stride = 0;
			ctx->stride_uv = 0;
			ctx->sar.num = ctx->sar.den = 0;
			while (gf_list_count(ctx->src_packets)) {
				GF_FilterPacket *ref_pck = gf_list_pop_back(ctx->src_packets);
				gf_filter_pck_unref(ref_pck);
			}
			GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[FFDec] PID %s reconfigure pending and all frames flushed, reconfiguring\n", gf_filter_pid_get_name(ctx->in_pid) ));
			e = ffdec_configure_pid(filter, ctx->in_pid, GF_FALSE);
			if (e==GF_NOT_SUPPORTED)
				return GF_PROFILE_NOT_SUPPORTED;
			return e;
		}
	}

	if (res < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FFDec] PID %s failed to decode frame PTS "LLU": %s\n", gf_filter_pid_get_name(ctx->in_pid), pkt->pts, av_err2str(res) ));
		FF_RELEASE_PCK(pkt);
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	FF_RELEASE_PCK(pkt);
	if (!gotpic) return GF_OK;

	if (ctx->decoder->pix_fmt != ctx->o_ff_pfmt) {
		u32 pix_fmt = ffmpeg_pixfmt_to_gpac(ctx->decoder->pix_fmt, GF_FALSE);
		ctx->o_ff_pfmt = ctx->decoder->pix_fmt;
		if (!pix_fmt) {
			pix_fmt = GF_PIXEL_RGB;
		}
		ffdec_check_pix_fmt_change(ctx, pix_fmt);
	}
	//update all props
	FF_CHECK_PROP(width, width, GF_PROP_PID_WIDTH)
	FF_CHECK_PROP(height, height, GF_PROP_PID_HEIGHT)

	stride = stride_uv = uv_height = nb_planes = 0;
	if (! gf_pixel_get_size_info(ctx->pixel_fmt, ctx->width, ctx->height, &outsize, &stride, &stride_uv, &nb_planes, &uv_height) ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FFDec] PID %s failed to query pixelformat size infon", gf_filter_pid_get_name(ctx->in_pid) ));
		return GF_NOT_SUPPORTED;
	}

	FF_CHECK_PROP_VAL(stride, stride, GF_PROP_PID_STRIDE)
	FF_CHECK_PROP_VAL(stride_uv, stride_uv, GF_PROP_PID_STRIDE_UV)
	if (ctx->sar.num * ctx->decoder->sample_aspect_ratio.den != ctx->sar.den * ctx->decoder->sample_aspect_ratio.num) {
		ctx->sar.num = ctx->decoder->sample_aspect_ratio.num;
		ctx->sar.den = ctx->decoder->sample_aspect_ratio.den;

		gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_SAR, &PROP_FRAC( ctx->sar ) );
	}

	pck_src = NULL;
	count = gf_list_count(ctx->src_packets);
	for (i=0; i<count; i++) {
		u64 cts;
		pck_src = gf_list_get(ctx->src_packets, i);
		cts = gf_filter_pck_get_cts(pck_src);
		if (cts == frame->pts)
			break;
#if (LIBAVCODEC_VERSION_MAJOR < 59)
		if (cts == frame->pkt_pts)
			break;
#endif
		pck_src = NULL;
	}

	seek_flag = GF_FALSE;
	if (pck_src) {
		seek_flag = gf_filter_pck_get_seek_flag(pck_src);
		out_cts = gf_filter_pck_get_cts(pck_src);
	} else {
		out_cts = frame->pts;
	}
	//this was a seek frame, do not dispatch
	if (seek_flag) {
		if (pck_src) {
			gf_list_del_item(ctx->src_packets, pck_src);
			gf_filter_pck_unref(pck_src);
		}
		return GF_OK;
	}

	dst_pck = gf_filter_pck_new_alloc(ctx->out_pid, outsize, &out_buffer);
	if (!dst_pck) return GF_OUT_OF_MEM;

	if (pck_src) {
		gf_filter_pck_merge_properties(pck_src, dst_pck);
		gf_filter_pck_set_dependency_flags(dst_pck, 0);
		gf_list_del_item(ctx->src_packets, pck_src);
		gf_filter_pck_unref(pck_src);
	} else {
		gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1);
	}

    //rewrite dts and pts to PTS value
    gf_filter_pck_set_dts(dst_pck, out_cts);
    gf_filter_pck_set_cts(dst_pck, out_cts);

	ff_pfmt = ctx->decoder->pix_fmt;
	if (ff_pfmt==AV_PIX_FMT_YUVJ420P) {
		ff_pfmt = AV_PIX_FMT_YUV420P;
		if (!ctx->force_full_range) {
			ctx->force_full_range = GF_TRUE;
			gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_COLR_RANGE, &PROP_BOOL(GF_TRUE));
		}
	}

	memset(&dst_planes, 0, sizeof(u8 *)*5);
	memset(&dst_stride, 0, sizeof(u32)*5);

	//TODO: cleanup, we should not convert pixel format in the decoder but through filters !
	switch (ctx->pixel_fmt) {
	case GF_PIXEL_RGB:
		dst_planes[0] =  (uint8_t *)out_buffer;
		dst_stride[0] = 3*ctx->width;
		pix_out = AV_PIX_FMT_RGB24;
		break;
	case GF_PIXEL_RGBA:
		dst_planes[0] =  (uint8_t *)out_buffer;
		dst_stride[0] = 4*ctx->width;
		pix_out = AV_PIX_FMT_RGBA;
		break;
	case GF_PIXEL_YUV:
	case GF_PIXEL_YUV_10:
		dst_planes[0] =  (uint8_t *)out_buffer;
		dst_planes[1] =  (uint8_t *)out_buffer + ctx->stride * ctx->height;
		dst_planes[2] =  (uint8_t *)dst_planes[1] + ctx->stride_uv * uv_height;
		dst_stride[0] = ctx->stride;
		dst_stride[1] = dst_stride[2] = ctx->stride_uv;
		if (ctx->pixel_fmt == GF_PIXEL_YUV_10)
			pix_out = AV_PIX_FMT_YUV420P10LE;
		else
			pix_out = AV_PIX_FMT_YUV420P;
		break;

	case GF_PIXEL_YUV422:
	case GF_PIXEL_YUV422_10:
		dst_planes[0] =  (uint8_t *)out_buffer;
		dst_planes[1] =  (uint8_t *)out_buffer + ctx->stride * ctx->height;
		dst_planes[2] =  (uint8_t *)dst_planes[1] + ctx->stride_uv * ctx->height;
		dst_stride[0] = ctx->stride;
		dst_stride[1] = dst_stride[2] = ctx->stride_uv;
		if (ctx->pixel_fmt == GF_PIXEL_YUV422_10)
			pix_out = AV_PIX_FMT_YUV422P10LE;
		else
			pix_out = AV_PIX_FMT_YUV422P;
		break;

	case GF_PIXEL_YUV444:
	case GF_PIXEL_YUV444_10:
		dst_planes[0] =  (uint8_t *)out_buffer;
		dst_planes[1] =  (uint8_t *)out_buffer + ctx->stride * ctx->height;
		dst_planes[2] =  (uint8_t *)out_buffer + 2*ctx->stride * ctx->height;
		dst_stride[0] = dst_stride[1] = dst_stride[2] = ctx->stride;
		if (ctx->pixel_fmt == GF_PIXEL_YUV444_10)
			pix_out = AV_PIX_FMT_YUV444P10LE;
		else
			pix_out = AV_PIX_FMT_YUV444P;
		break;

	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FFDec] Unsupported pixel format %s, patch welcome\n", av_get_pix_fmt_name(ctx->decoder->pix_fmt) ));

		gf_filter_pck_discard(dst_pck);

		return GF_NOT_SUPPORTED;
	}

	ctx->sws_ctx = sws_getCachedContext(ctx->sws_ctx,
	                                   ctx->decoder->width, ctx->decoder->height, ff_pfmt,
	                                   ctx->width, ctx->height, pix_out, SWS_BICUBIC, NULL, NULL, NULL);
	if (ctx->sws_ctx) {
		sws_scale(ctx->sws_ctx, (const uint8_t * const*)frame->data, frame->linesize, 0, ctx->height, dst_planes, dst_stride);
	}


	gf_filter_pck_set_seek_flag(dst_pck, GF_FALSE);

	if (frame->interlaced_frame)
		gf_filter_pck_set_interlaced(dst_pck, frame->top_field_first ? 2 : 1);

	gf_filter_pck_send(dst_pck);
	return GF_OK;
}


static GF_Err ffdec_process_audio(GF_Filter *filter, struct _gf_ffdec_ctx *ctx)
{
	s32 gotpic;
#if (LIBAVCODEC_VERSION_MAJOR < 59)
	s32 len;
#endif
	AVPacket *pkt;
	s32 res, in_size, i;
	u32 samples_to_trash;
	u32 output_size, prev_afmt;
	Bool is_eos=GF_FALSE;
	u8 *data;
	AVFrame *frame;
	GF_FilterPacket *dst_pck, *src_pck;
	GF_FilterPacket *pck;

decode_next:
	pck = gf_filter_pid_get_packet(ctx->in_pid);
	in_size = 0;
	
	if (ctx->reconfig_pending) {
		pck = NULL;
	} else if (!pck) {
		is_eos = gf_filter_pid_is_eos(ctx->in_pid);
		if (!is_eos) return GF_OK;
	}

	FF_INIT_PCK(ctx, pkt)

	if (pck) pkt->data = (uint8_t *) gf_filter_pck_get_data(pck, &in_size);

	if (pck) {
		src_pck = pck;
		gf_filter_pck_ref_props(&src_pck);
		if (src_pck) gf_list_add(ctx->src_packets, src_pck);

		if (!pkt->data) {
			gf_filter_pid_drop_packet(ctx->in_pid);
			return GF_OK;
		}

		pkt->pts = gf_filter_pck_get_cts(pck);
		pkt->dts = gf_filter_pck_get_dts(pck);

		pkt->size = in_size;
#if (LIBAVCODEC_VERSION_MAJOR < 59)
		if ((s32) ctx->frame_start > pkt->size) ctx->frame_start = 0;
		//seek to last byte consumed by the previous decode4()
		else if (ctx->frame_start) {
			pkt->data += ctx->frame_start;
			pkt->size -= ctx->frame_start;
		}
#endif

		pkt->duration = gf_filter_pck_get_duration(pck);
		if (gf_filter_pck_get_sap(pck)>0)
			pkt->flags = AV_PKT_FLAG_KEY;

		if (!ctx->first_cts_plus_one)
			ctx->first_cts_plus_one = pkt->pts + 1;

	} else {
		pkt->size = 0;
	}

	prev_afmt = ctx->decoder->sample_fmt;
	frame = ctx->frame;

#if (LIBAVCODEC_VERSION_MAJOR < 59)
	len = res = avcodec_decode_audio4(ctx->decoder, frame, &gotpic, pkt);
#else
	res = avcodec_send_packet(ctx->decoder, pkt);
	switch (res) {
	case AVERROR(EAGAIN):
		if (src_pck) {
			gf_list_del_item(ctx->src_packets, src_pck);
			gf_filter_pck_unref(src_pck);
		}
		av_packet_unref(pkt);
		return GF_OK;
	case 0:
	case AVERROR_EOF:
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FFDec] PID %s failed to decode frame PTS "LLU": %s\n", gf_filter_pid_get_name(ctx->in_pid), pkt->pts, av_err2str(res) ));
		break;
	}

	gotpic = 0;
	res = avcodec_receive_frame(ctx->decoder, frame);
	switch (res) {
	case AVERROR(EAGAIN):
		res = 0;
		break;
	case 0:
		gotpic = 1;
		break;
	case AVERROR_EOF:
		res = 0;
		break;
	default:
		break;
	}
dispatch_next:
#endif

	samples_to_trash = 0;
	if ( gotpic && (ctx->delay<0)) {
		if (pkt->pts + 1 - ctx->first_cts_plus_one + ctx->delay < 0) {
			if (pkt->duration + ctx->delay < 0) {
				frame->nb_samples = 0;
				samples_to_trash = 0;
				ctx->delay += pkt->duration;
			} else {
				samples_to_trash = (u32) -ctx->delay;
			}
			
			if (!samples_to_trash || (samples_to_trash > (u32) frame->nb_samples) ) {
				frame->nb_samples = 0;
				samples_to_trash = 0;
			}
			gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_DELAY, NULL);
		} else {
			gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_DELAY, NULL);
			ctx->delay = 0;
		}
	}

	//this will handle eos as well
	if ((res<0) || !gotpic) {
#if (LIBAVCODEC_VERSION_MAJOR < 59)
		ctx->frame_start = 0;
#endif
		if (pck) gf_filter_pid_drop_packet(ctx->in_pid);
		if (pkt->size && (res<0)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FFDec] PID %s failed to decode frame PTS "LLU": %s\n", gf_filter_pid_get_name(ctx->in_pid), pkt->pts, av_err2str(res) ));
			FF_RELEASE_PCK(pkt);
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		FF_RELEASE_PCK(pkt);
		if (is_eos) {
			gf_filter_pid_set_eos(ctx->out_pid);
			return GF_EOS;
		}
		if (ctx->reconfig_pending) {
			GF_Err e;
			avcodec_free_context(&ctx->decoder);
			ctx->decoder = NULL;
			ctx->reconfig_pending = GF_FALSE;
			//these properties are checked after decode, when we reconfigure we copy props from input to output
			//so we need to make sure we retrigger pid config even if these did not change
			ctx->sample_fmt = 0;
			ctx->sample_rate = 0;
			ctx->channels = 0;
			ctx->channel_layout = 0;
			GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[FFDec] PID %s reconfigure pending and all frames flushed, reconfiguring\n", gf_filter_pid_get_name(ctx->in_pid) ));
			e = ffdec_configure_pid(filter, ctx->in_pid, GF_FALSE);
			if (e==GF_NOT_SUPPORTED)
				return GF_PROFILE_NOT_SUPPORTED;
			return e;
		}
		return GF_OK;
	}
	FF_RELEASE_PCK(pkt);


	FF_CHECK_PROP(channels, channels, GF_PROP_PID_NUM_CHANNELS)
	FF_CHECK_PROPL(channel_layout, channel_layout, GF_PROP_PID_CHANNEL_LAYOUT)
	FF_CHECK_PROP(sample_rate, sample_rate, GF_PROP_PID_SAMPLE_RATE)

	if (prev_afmt != ctx->decoder->sample_fmt) {
		ctx->sample_fmt = ffmpeg_audio_fmt_to_gpac(ctx->decoder->sample_fmt);
		gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(ctx->sample_fmt) );
		ctx->bytes_per_sample = gf_audio_fmt_bit_depth(ctx->sample_fmt) / 8;
	}

	if (pck) {
		const GF_PropertyValue *er = gf_filter_pck_get_property(pck, GF_PROP_PCK_END_RANGE);
		if (er && er->value.boolean) {
			u32 timescale = gf_filter_pck_get_timescale(pck);
			u64 odur = gf_filter_pck_get_duration(pck);
			if (timescale != ctx->sample_rate) {
				odur = gf_timestamp_rescale(odur, timescale, ctx->sample_rate);
			}
			if (odur < frame->nb_samples) {
				frame->nb_samples = (int) odur;
			}
		}
	}

	output_size = (frame->nb_samples - samples_to_trash) * ctx->channels * ctx->bytes_per_sample;
	dst_pck = gf_filter_pck_new_alloc(ctx->out_pid, output_size, &data);
	if (!dst_pck) return GF_OUT_OF_MEM;

	switch (frame->format) {
	case AV_SAMPLE_FMT_U8P:
	case AV_SAMPLE_FMT_S16P:
	case AV_SAMPLE_FMT_S32P:
	case AV_SAMPLE_FMT_FLTP:
	case AV_SAMPLE_FMT_DBLP:
		for (i=0; (u32) i< ctx->channels; i++) {
			char *inputChannel = frame->extended_data[i] + samples_to_trash * ctx->bytes_per_sample;
			memcpy(data, inputChannel, ctx->bytes_per_sample * (frame->nb_samples-samples_to_trash) );
			data += ctx->bytes_per_sample * (frame->nb_samples-samples_to_trash);
		}
		break;
	default:
		memcpy(data, ctx->frame->data[0] + samples_to_trash * ctx->bytes_per_sample, ctx->bytes_per_sample * (frame->nb_samples - samples_to_trash) * ctx->channels);
		break;
	}

	//we don't follow the same approach as in video, we assume the codec works with one in one out
	//and use the first entry in src packets to match in order to copy the properties
	//a nicer approach would be to count delay frames (number of frames used to initialize)
	//and backmerge properties from the last packet in to the last-nb_init_frames
	src_pck = gf_list_get(ctx->src_packets, 0);

	if (src_pck) {
		gf_filter_pck_merge_properties(src_pck, dst_pck);
		gf_filter_pck_set_dependency_flags(dst_pck, 0);
		gf_list_rem(ctx->src_packets, 0);
		gf_filter_pck_unref(src_pck);
	}

	if (output_size) {
		if (frame->pts != AV_NOPTS_VALUE) {
			u64 pts = frame->pts;
			u32 timescale = gf_filter_pck_get_timescale(pck);
			if (ctx->nb_samples_already_in_frame) {
				if (ctx->sample_rate == timescale) {
					pts += ctx->nb_samples_already_in_frame;
				}
			}
			if (pts >= ctx->ts_offset) pts -= ctx->ts_offset;
			else pts = 0;
			gf_filter_pck_set_cts(dst_pck, pts);
			gf_filter_pck_set_dts(dst_pck, pts);
		}
		gf_filter_pck_send(dst_pck);
	} else {
		gf_filter_pck_discard(dst_pck);
	}

#if (LIBAVCODEC_VERSION_MAJOR < 59)
	ctx->frame_start += len;
	//done with this input packet
	if (in_size <= (s32) ctx->frame_start) {
		frame->nb_samples = 0;
		ctx->frame_start = 0;
		ctx->nb_samples_already_in_frame = 0;
		gf_filter_pid_drop_packet(ctx->in_pid);

		if (gf_filter_pid_would_block(ctx->out_pid))
			return GF_OK;

		//if space available in output, decode right away - needed for audio formats with very short frames
		//avoid recursion
		goto decode_next;
	}
	//still some data to decode in packet, don't drop it
	//todo: check if frame->pkt_pts or frame->pts is updated by ffmpeg, otherwise do it ourselves !
	GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FFDec] Code not yet tested  - frame PTS was "LLU" - nb samples dec %d\n", frame->pkt_pts, frame->nb_samples));
	ctx->nb_samples_already_in_frame += frame->nb_samples;
	frame->nb_samples = 0;
#else
	//if multiple frames in packet, send them
	gotpic = 0;
	res = avcodec_receive_frame(ctx->decoder, frame);
	if (res==0) {
		gotpic = 1;
		goto dispatch_next;
	}
	if (pck) gf_filter_pid_drop_packet(ctx->in_pid);
#endif

	//avoid recursion
	goto decode_next;

//	return ffdec_process_audio(filter, ctx);
}

#ifdef FF_SUB_SUPPORT
static GF_Err ffdec_process_subtitle(GF_Filter *filter, struct _gf_ffdec_ctx *ctx)
{
	AVPacket *pkt;
	AVSubtitle subs;
	s32 gotpic;
	s32 len, in_size;
	Bool is_eos=GF_FALSE;

	GF_FilterPacket *pck = gf_filter_pid_get_packet(ctx->in_pid);

	if (!pck) {
		is_eos = gf_filter_pid_is_eos(ctx->in_pid);
		if (!is_eos) return GF_OK;
	}
	FF_INIT_PCK(ctx, pkt);

	if (pck) pkt->data = (uint8_t *) gf_filter_pck_get_data(pck, &in_size);

	if (!is_eos) {
		u64 dts;
		pkt->pts = gf_filter_pck_get_cts(pck);

		//copy over SAP and duration in dts
		dts = gf_filter_pck_get_sap(pck);
		dts <<= 32;
		dts |= gf_filter_pck_get_duration(pck);
		pkt->dts = dts;

		pkt->size = in_size;
	} else {
		pkt->size = 0;
	}

	memset(&subs, 0, sizeof(AVSubtitle));
	len = avcodec_decode_subtitle2(ctx->decoder, &subs, &gotpic, pkt);

	//this will handle eos as well
	if ((len<0) || !gotpic) {
		FF_RELEASE_PCK(pkt)
		if (pck) gf_filter_pid_drop_packet(ctx->in_pid);
		if (len<0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FFDec] PID %s failed to decode frame PTS "LLU": %s\n", gf_filter_pid_get_name(ctx->in_pid), pkt->pts, av_err2str(len) ));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		if (is_eos) {
			gf_filter_pid_set_eos(ctx->out_pid);
			return GF_EOS;
		}
		return GF_OK;
	}
	//TODO - do we want to remap to TX3G/other and handle the rendering some place else, or do we do the rendering here ?


	avsubtitle_free(&subs);
	if (pck) gf_filter_pid_drop_packet(ctx->in_pid);
	FF_RELEASE_PCK(pkt)
	return GF_OK;
}
#endif

static GF_Err ffdec_process(GF_Filter *filter)
{
	GF_FFDecodeCtx *ctx = (GF_FFDecodeCtx *) gf_filter_get_udta(filter);
	if (gf_filter_pid_would_block(ctx->out_pid))
		return GF_OK;

	return ctx->process(filter, ctx);
}

static const GF_FilterCapability FFDecodeAnnexBCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_BOOL(GF_CAPS_INPUT, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_VVC),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_AVC),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};


static GF_Err ffdec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	s32 res;
	u32 type=0, gpac_codecid=0;
	AVDictionary *options = NULL;
	const GF_PropertyValue *prop;
	const AVCodec *codec=NULL;
	Bool unwrap_extra_data = GF_FALSE;
	GF_FFDecodeCtx *ctx = (GF_FFDecodeCtx *) gf_filter_get_udta(filter);

	//disconnect of src pid (not yet supported)
	if (is_remove) {
		//one in one out, this is simple
		if (ctx->out_pid) {
			gf_filter_pid_remove(ctx->out_pid);
			ctx->out_pid = NULL;
		}
		return GF_OK;
	}

	//check our PID: streamtype and codecid
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (!prop) return GF_NOT_SUPPORTED;

	type = prop->value.uint;
	switch (type) {
	case GF_STREAM_AUDIO:
	case GF_STREAM_VISUAL:
#ifdef FF_SUB_SUPPORT
	case GF_STREAM_TEXT:
#endif
		break;
	default:
		return GF_NOT_SUPPORTED;
	}
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!prop) return GF_NOT_SUPPORTED;
	gpac_codecid = prop->value.uint;
	if (gpac_codecid==GF_CODECID_RAW)
		return GF_NOT_SUPPORTED;

	if (gf_sys_is_test_mode() && (gpac_codecid == GF_CODECID_MPEG_AUDIO))
		gpac_codecid = GF_CODECID_MPEG2_PART3;

	//initial config or update
	if (!ctx->in_pid || (ctx->in_pid==pid)) {
		ctx->in_pid = pid;
		if (!ctx->type) ctx->type = type;
		else if (ctx->type != type) {
			return GF_NOT_SUPPORTED;
		}
	} else {
		//only one input pid in ctx
		if (ctx->in_pid) return GF_REQUIRES_NEW_INSTANCE;
	}

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
	ctx->width = prop ? prop->value.uint : 0;
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
	ctx->height = prop ? prop->value.uint : 0;
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLE_RATE);
	ctx->sample_rate = prop ? prop->value.uint : 0;
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_NUM_CHANNELS);
	ctx->channels = prop ? prop->value.uint : 0;

	if (gpac_codecid==GF_CODECID_VVC) {
		u32 codec_id = ffmpeg_codecid_from_gpac(gpac_codecid, NULL);
		codec = codec_id ? avcodec_find_decoder(codec_id) : NULL;
		//first version of libvvdec+ffmpeg only supported annexB, request ufnalu adaptation filter
		if (codec && codec->name && strstr(codec->name, "vvdec") && gf_opts_get_bool("core", "vvdec-annexb")) {
			//vvdec currently requires bootstrap on SAP
			if (!ctx->decoder)
				ctx->force_rap_wait = GF_TRUE;

			prop = gf_filter_pid_get_property(pid, GF_PROP_PID_UNFRAMED);
			if (!prop || !prop->value.boolean) {
				gf_filter_override_caps(filter, FFDecodeAnnexBCaps, GF_ARRAY_LENGTH(FFDecodeAnnexBCaps));
				gf_filter_pid_negociate_property(ctx->in_pid, GF_PROP_PID_UNFRAMED, &PROP_BOOL(GF_TRUE) );
				return GF_OK;
			}
		}
	}

	if (gpac_codecid == GF_CODECID_FFMPEG) {
		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_FFMPEG_CODEC_ID);
		if (prop && (prop->type==GF_PROP_POINTER)) {
			ctx->decoder = prop->value.ptr;
			if (!ctx->decoder) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FFDec] PID %s codec context not exposed by demuxer !\n", gf_filter_pid_get_name(pid) ));
				return GF_SERVICE_ERROR;
			}
			codec = avcodec_find_decoder(ctx->decoder->codec_id);
			ctx->owns_context = GF_FALSE;
		} else if (prop && (prop->type==GF_PROP_UINT)) {
			ctx->decoder = avcodec_alloc_context3(NULL);
			if (! ctx->decoder) return GF_OUT_OF_MEM;
			codec = avcodec_find_decoder(prop->value.uint);
			ctx->owns_context = GF_TRUE;
		}

		if (!prop) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FFDec] PID %s codec context not exposed by demuxer !\n", gf_filter_pid_get_name(pid) ));
			return GF_SERVICE_ERROR;
		}

		if (!codec) {
			gf_filter_set_name(filter, "ffdec");
			return GF_NOT_SUPPORTED;
		}
	}
	//we reconfigure the stream
	else {
		u32 codec_id, ff_codectag=0;

        if (!ctx->owns_context) {
            ctx->decoder = NULL;
        }
		if (ctx->decoder) {
			codec_id = ffmpeg_codecid_from_gpac(gpac_codecid, NULL);
			//same codec, same config, don't reinit
			if (ctx->decoder->codec->id == codec_id) {
				u32 cfg_crc=0;
				prop = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
				if (prop && prop->value.data.ptr && prop->value.data.size) {
					cfg_crc = gf_crc_32(prop->value.data.ptr, prop->value.data.size);
				}
				if (cfg_crc == ctx->extra_data_crc) {
					goto reuse_codec_context;
				}
			}

            
			//we could further optimize by detecting we have the same codecid and injecting the extradata
			//but this is not 100% reliable, and will require parsing AVC/HEVC config
			//since this seems to work properly with decoder close/open, we keep it as is
			ctx->reconfig_pending = GF_TRUE;
			GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[FFDec] PID %s reconfigure detected, flushing frame\n", gf_filter_pid_get_name(pid) ));
			return GF_OK;
		}

		codec_id = ffmpeg_codecid_from_gpac(gpac_codecid, &ff_codectag);
		//specific remaps
		if (!codec_id) {
			switch (gpac_codecid) {
			case GF_CODECID_MVC: codec_id = AV_CODEC_ID_H264; break;
			}

		}
		if (codec_id)
			codec = avcodec_find_decoder(codec_id);

		if (!codec_id && ctx->ffcmap.nb_items) {
			char szFmt[20], szFmt2[20];
			u32 i, l1, l2;
			sprintf(szFmt, "%d@", gpac_codecid);
			l1 = (u32) strlen(szFmt);
			sprintf(szFmt2, "%s@", gf_4cc_to_str(gpac_codecid));
			l2 = (u32) strlen(szFmt2);
			for (i=0; i<ctx->ffcmap.nb_items; i++) {
				char *fmt = NULL;
				Bool unwrap_dsi = GF_FALSE;
				if (!strncmp(ctx->ffcmap.vals[i], szFmt, l1)) fmt = ctx->ffcmap.vals[i] + l1;
				else if (!strncmp(ctx->ffcmap.vals[i], szFmt2, l2)) fmt = ctx->ffcmap.vals[i] + l2;
				else continue;

				if (fmt[0]=='+') {
					unwrap_dsi = GF_TRUE;
					fmt++;
				}
				codec = avcodec_find_decoder_by_name(fmt);
				if (codec) {
					unwrap_extra_data = unwrap_dsi;
					break;
				}
			}
		}
		if (!codec && gpac_codecid) codec = avcodec_find_decoder_by_name( gf_4cc_to_str(gpac_codecid));

		if (!codec) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FFDec] No decoder found for codec %s\n", gf_codecid_name(gpac_codecid) ));
			gf_filter_set_name(filter, "ffdec");
			return GF_NOT_SUPPORTED;
		}

		ctx->decoder = avcodec_alloc_context3(NULL);
		if (! ctx->decoder) return GF_OUT_OF_MEM;
		ctx->owns_context = GF_TRUE;
		if (ff_codectag)
			ctx->decoder->codec_tag = ff_codectag;
	}

	if (ctx->owns_context) {

		ffmpeg_set_enc_dec_flags(ctx->options, ctx->decoder);

		//for some raw codecs
		if (ctx->width && ctx->height) {
			ctx->decoder->width = ctx->width;
			ctx->decoder->height = ctx->height;
		}
		if (ctx->sample_rate && ctx->channels) {
			ctx->decoder->sample_rate = ctx->sample_rate;
			ctx->decoder->channels = ctx->channels;
		}
	}

	//we may have a dsi here!
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (prop
		&& ((prop->type==GF_PROP_DATA) || (prop->type==GF_PROP_CONST_DATA))
		&& prop->value.data.ptr
		&& prop->value.data.size
	) {
		u8 *dsi = prop->value.data.ptr;
		u32 dsi_size = prop->value.data.size;
		if (unwrap_extra_data && (dsi_size>8)) {
			u32 size = dsi[0]; size<<=8;
			size |= dsi[1]; size<<=8;
			size |= dsi[2]; size<<=8;
			size |= dsi[3];
			if (size == dsi_size) {
				dsi += 8;
				dsi_size -= 8;
			}
		}
		GF_Err e = ffmpeg_extradata_from_gpac(gpac_codecid, dsi, dsi_size, &ctx->decoder->extradata, &ctx->decoder->extradata_size);
		if (e) return e;

		ctx->extra_data_crc = gf_crc_32(ctx->decoder->extradata, ctx->decoder->extradata_size);
	}

	//by default let libavcodec decide - if single thread is required, let the user define -threads option
	if (codec->capabilities & AV_CODEC_CAP_AUTO_THREADS)
		ctx->decoder->thread_count = 0;

	//clone options (in case we need to destroy/recreate the codec) and open codec
	av_dict_copy(&options, ctx->options, 0);
	res = avcodec_open2(ctx->decoder, codec, &options);
	if (res < 0) {
		if (options) av_dict_free(&options);
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FFDec] PID %s failed to open codec context: %s\n", gf_filter_pid_get_name(pid), av_err2str(res) ));
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	ffmpeg_report_options(filter, options, ctx->options);

	//we're good to go, declare our output pid
	ctx->in_pid = pid;
	if (!ctx->out_pid) {
		char szCodecName[1000];
		ctx->out_pid = gf_filter_pid_new(filter);

		//to change once we implement on-the-fly codec change
		sprintf(szCodecName, "ffdec:%s", ctx->decoder->codec->name ? ctx->decoder->codec->name : "unknown");
		gf_filter_set_name(filter, szCodecName);
		gf_filter_pid_set_framing_mode(ctx->in_pid, GF_TRUE);
	}

reuse_codec_context:
	//copy props it at init config or at reconfig
	if (ctx->out_pid) {
		gf_filter_pid_copy_properties(ctx->out_pid, ctx->in_pid);
		gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW) );
		gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_DECODER_CONFIG, NULL );
	}

	if (type==GF_STREAM_VISUAL) {
		u32 pix_fmt;
		ctx->force_full_range = GF_FALSE;
		gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_COLR_RANGE, NULL );
		gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_COLR_MX, NULL );
		gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_COLR_TRANSFER, NULL );
		gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_COLR_PRIMARIES, NULL );

		ctx->process = ffdec_process_video;
		//for some streams, we don't have w/h/pixfmt after opening the decoder
		//to make sure we are not confusing potential filters expecting them, init to default values
		if (ctx->decoder->pix_fmt>=0) {
			ctx->o_ff_pfmt = ctx->decoder->pix_fmt;
			pix_fmt = ffmpeg_pixfmt_to_gpac(ctx->decoder->pix_fmt, GF_FALSE);
			if (!pix_fmt) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[FFDec] Unsupported pixel format %d, defaulting to RGB\n", ctx->decoder->pix_fmt));
				pix_fmt = GF_PIXEL_RGB;
			}
		} else {
			pix_fmt = GF_PIXEL_YUV;
			ctx->o_ff_pfmt = AV_PIX_FMT_YUV420P;
		}
		ffdec_check_pix_fmt_change(ctx, pix_fmt);

		if (ctx->decoder->width) {
			FF_CHECK_PROP(width, width, GF_PROP_PID_WIDTH)
		} else {
			gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_WIDTH, &PROP_UINT( ctx->width) );
		}
		if (ctx->decoder->height) {
			FF_CHECK_PROP(height, height, GF_PROP_PID_HEIGHT)
		} else {
			gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_HEIGHT, &PROP_UINT( ctx->height) );
		}
		if (ctx->decoder->sample_aspect_ratio.num && ctx->decoder->sample_aspect_ratio.den) {
			ctx->sar.num = ctx->decoder->sample_aspect_ratio.num;
			ctx->sar.den = ctx->decoder->sample_aspect_ratio.den;
			gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_SAR, &PROP_FRAC( ctx->sar) );
		}
		if (!ctx->frame)
			ctx->frame = av_frame_alloc();

		if (ctx->pixel_fmt) {
			gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_PIXFMT, &PROP_UINT( ctx->pixel_fmt) );
		}

	} else if (type==GF_STREAM_AUDIO) {
		ctx->process = ffdec_process_audio;
		if (ctx->decoder->sample_fmt != AV_SAMPLE_FMT_NONE) {
			ctx->sample_fmt = ffmpeg_audio_fmt_to_gpac(ctx->decoder->sample_fmt);
			gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(ctx->sample_fmt) );
			ctx->bytes_per_sample = gf_audio_fmt_bit_depth(ctx->sample_fmt) / 8;
		}

		//override PID props with what decoder gives us
		if (ctx->decoder->channels) {
			ctx->channels = 0;
			FF_CHECK_PROP(channels, channels, GF_PROP_PID_NUM_CHANNELS)
		}
		if (ctx->decoder->channel_layout) {
			u64 ch_lay = ffmpeg_channel_layout_to_gpac(ctx->decoder->channel_layout);
			if (ctx->channel_layout != ch_lay) {
				gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_CHANNEL_LAYOUT, &PROP_LONGUINT(ch_lay ) );
				ctx->channel_layout = ch_lay;
			}
		}
		if (ctx->decoder->sample_rate) {
			ctx->sample_rate = 0;
			FF_CHECK_PROP(sample_rate, sample_rate, GF_PROP_PID_SAMPLE_RATE)
		}
		if (!ctx->frame)
			ctx->frame = av_frame_alloc();

		if (ctx->sample_fmt) {
			gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT( ctx->sample_fmt) );
		}

		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_NO_PRIMING);
		if (prop && prop->value.boolean) {
			ctx->delay = 0;
		} else {
			prop = gf_filter_pid_get_property(pid, GF_PROP_PID_DELAY);
			ctx->delay = prop ? prop->value.longsint : 0;
			ctx->first_cts_plus_one = 0;

			if (ctx->delay<0)
				ctx->ts_offset = (u32) -ctx->delay;
			else
				ctx->ts_offset = 0;
		}

	} else {
#ifdef FF_SUB_SUPPORT
		ctx->process = ffdec_process_subtitle;
#endif
	}
	return GF_OK;
}


static GF_Err ffdec_update_arg(GF_Filter *filter, const char *arg_name, const GF_PropertyValue *arg_val)
{
	s32 res;
	GF_FFDecodeCtx *ctx = gf_filter_get_udta(filter);

	//initial parsing of arguments
	if (!ctx->initialized) {
		switch (arg_val->type) {
		case GF_PROP_STRING:
			res = av_dict_set(&ctx->options, arg_name, arg_val->value.string, 0);
			if (res<0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FFDec] Failed to set option %s:%s\n", arg_name, arg_val ));
			}
			break;
		default:
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FFDec] Failed to set option %s:%s, unrecognized type %d\n", arg_name, arg_val, arg_val->type ));
			return GF_NOT_SUPPORTED;
		}
		return GF_OK;
	}
	//updates of arguments, not supported for ffmpeg decoders
	return GF_NOT_SUPPORTED;
}

static Bool ffdec_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_FFDecodeCtx *ctx = (GF_FFDecodeCtx *) gf_filter_get_udta(filter);

	if ((evt->base.type==GF_FEVT_PLAY) || (evt->base.type==GF_FEVT_SET_SPEED) || (evt->base.type==GF_FEVT_RESUME)) {
		ctx->drop_non_refs = evt->play.drop_non_ref;
	}
	//play request, detach all pending source packets and trigger a reconfig to start from a clean state
	else if (evt->base.type==GF_FEVT_STOP) {
		while (gf_list_count(ctx->src_packets)) {
			GF_FilterPacket *pck = gf_list_pop_back(ctx->src_packets);
			gf_filter_pck_unref(pck);
			//for video, this will reset the decoder
			ctx->flush_done = GF_TRUE;
		}
	}

	return GF_FALSE;
}

static const GF_FilterCapability FFDecodeCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_SVC),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_LHVC),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_HEVC_TILES),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_VVC_SUBPIC),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_TILE_BASE, GF_TRUE),
	{ .code=GF_PROP_PID_SCALABLE, .val={.type=GF_PROP_BOOL, .value.boolean = GF_TRUE}, .flags=(GF_CAPS_INPUT_OPT), .priority=255 },
	{0},
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),

#ifdef FF_SUB_SUPPORT
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_TEXT_MPEG4),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_TX3G),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_WEBVTT),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_SUBS_XML),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_SIMPLE_TEXT),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
#endif

};

GF_FilterRegister FFDecodeRegister = {
	.name = "ffdec",
	.version = LIBAVCODEC_IDENT,
	GF_FS_SET_DESCRIPTION("FFMPEG decoder")
	GF_FS_SET_HELP("This filter decodes audio and video streams using FFMPEG.\n"
	"See FFMPEG documentation (https://ffmpeg.org/documentation.html) for more details.\n"
	"To list all supported decoders for your GPAC build, use `gpac -h ffdec:*`.\n"
	"\n"
	"Options can be passed from prompt using `--OPT=VAL`\n"
	"The default threading mode is to let libavcodec decide how many threads to use. To enforce single thread, use `--threads=1`\n"
	"\n"
	"# Codec Map\n"
	"The [-ffcmap]() option allows specifying FFMPEG codecs for codecs not supported by GPAC.\n"
	"Each entry in the list is formatted as `GID@name` or `GID@+name`, with:\n"
	"- GID: 4CC or 32 bit identifier of codec ID, as indicated by `gpac -i source inspect:full`\n"
	"- name: FFMPEG codec name\n"
	"- `+': is set and extra data is set and formatted as an ISOBMFF box, removes box header\n"
	"\n"
	"EX gpac -i source --ffcmap=BKV1@binkvideo vout\n"
	"This will map an ISOBMFF track declared with coding type `BKV1` to binkvideo.\n"
	)
	.private_size = sizeof(GF_FFDecodeCtx),
	SETCAPS(FFDecodeCaps),
	.initialize = ffdec_initialize,
	.finalize = ffdec_finalize,
	.configure_pid = ffdec_configure_pid,
	.process = ffdec_process,
	.update_arg = ffdec_update_arg,
	.process_event = ffdec_process_event,
	.flags = GF_FS_REG_META,
	//use middle priorty, so that hardware decs/other native impl in gpac can take over if needed
	//don't use lowest one since we use this for scalable codecs
	.priority = 128

};


#define OFFS(_n)	#_n, offsetof(GF_FFDecodeCtx, _n)
static const GF_FilterArgs FFDecodeArgs[] =
{
	{ OFFS(ffcmap), "codec map", GF_PROP_STRING_LIST, NULL, NULL, 0},
	{ "*", -1, "any possible options defined for AVCodecContext and sub-classes. See `gpac -hx ffdec` and `gpac -hx ffdec:*`", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_META},
	{0}
};

const GF_FilterRegister *ffdec_register(GF_FilterSession *session)
{
	ffmpeg_build_register(session, &FFDecodeRegister, FFDecodeArgs, 1, FF_REG_TYPE_DECODE);
	return &FFDecodeRegister;
}

#else
#include <gpac/filters.h>
const GF_FilterRegister *ffdec_register(GF_FilterSession *session)
{
	return NULL;
}
#endif

