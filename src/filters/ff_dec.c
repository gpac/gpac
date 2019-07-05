/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2018
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

#define FF_CHECK_PROP_VAL(_name, _val, _type)	if (ctx->_name != _val) { \
		gf_filter_pid_set_property(ctx->out_pid, _type, &PROP_UINT( _val ) );	\
		ctx->_name = _val;	\
	} \

static GF_Err ffdec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove);


typedef struct _gf_ffdec_ctx
{
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
	u32 channels, sample_rate, sample_fmt, channel_layout, bytes_per_sample;
	u32 frame_start;
	u32 nb_samples_already_in_frame;

	//video state
	u32 width, height, pixel_fmt, stride, stride_uv;
	GF_Fraction sar;
	struct SwsContext *sws_ctx;

	GF_List *src_packets;
} GF_FFDecodeCtx;

static GF_Err ffdec_initialize(GF_Filter *filter)
{
	GF_FFDecodeCtx *ctx = (GF_FFDecodeCtx *) gf_filter_get_udta(filter);
	ctx->initialized = GF_TRUE;
	ctx->src_packets = gf_list_new();

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

	if (ctx->owns_context && ctx->decoder) {
		if (ctx->decoder->extradata) gf_free(ctx->decoder->extradata);
		avcodec_close(ctx->decoder);
	}
	return;
}

static GF_Err ffdec_process_video(GF_Filter *filter, struct _gf_ffdec_ctx *ctx)
{
	AVPacket pkt;
	AVFrame *frame;
	AVPicture pict;
	Bool is_eos=GF_FALSE;
	s32 res;
	s32 gotpic;
	const char *data = NULL;
	Bool seek_flag = GF_FALSE;
	u32 i, count;
	u32 size=0, pix_fmt, outsize, pix_out, stride, stride_uv, uv_height, nb_planes;
	u8 *out_buffer;
	GF_FilterPacket *pck_src;
	GF_FilterPacket *dst_pck;
	GF_FilterPacket *pck = gf_filter_pid_get_packet(ctx->in_pid);

	if (ctx->reconfig_pending) {
		pck = NULL;
	} else if (!pck) {
		is_eos = gf_filter_pid_is_eos(ctx->in_pid);
		if (!is_eos) return GF_OK;
	}

	frame = ctx->frame;

	av_init_packet(&pkt);

	if (pck) {
		data = gf_filter_pck_get_data(pck, &size);

		if (!size) {
			gf_filter_pid_drop_packet(ctx->in_pid);
			return GF_OK;
		}

		pck_src = pck;
		gf_filter_pck_ref_props(&pck_src);
		if (pck_src) gf_list_add(ctx->src_packets, pck_src);

		//seems ffmpeg is not properly handling the decoding after a flush, we close and reopen the codec
		if (ctx->flush_done) {
			const AVCodec *codec = ctx->decoder->codec;
			avcodec_close(ctx->decoder);
			avcodec_open2(ctx->decoder, codec, NULL );
			ctx->flush_done = GF_FALSE;
		}

		pkt.dts = gf_filter_pck_get_dts(pck);
		pkt.pts = gf_filter_pck_get_cts(pck);
		pkt.duration = gf_filter_pck_get_duration(pck);
		if (gf_filter_pck_get_sap(pck)>0)
			pkt.flags = AV_PKT_FLAG_KEY;
	}
	pkt.data = (uint8_t*)data;
	pkt.size = size;

	/*TOCHECK: for AVC bitstreams after ISMA decryption, in case (as we do) the decryption DRM tool
	doesn't put back nalu size, we have to do it ourselves, but we can't modify input data...*/

	gotpic=0;
	res = avcodec_decode_video2(ctx->decoder, frame, &gotpic, &pkt);
	if (pck) gf_filter_pid_drop_packet(ctx->in_pid);

	if (!gotpic) {
		if (is_eos) {
			ctx->flush_done = GF_TRUE;
			gf_filter_pid_set_eos(ctx->out_pid);
			return GF_EOS;
		}
		if (ctx->reconfig_pending) {
			if (ctx->decoder->extradata) gf_free(ctx->decoder->extradata);
			ctx->decoder->extradata = NULL;
			avcodec_close(ctx->decoder);
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
				GF_FilterPacket *pck = gf_list_pop_back(ctx->src_packets);
				gf_filter_pck_unref(pck);
			}
			GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[FFDec] PID %s reconfigure pending and all frames flushed, reconfguring\n", gf_filter_pid_get_name(ctx->in_pid) ));
			return ffdec_configure_pid(filter, ctx->in_pid, GF_FALSE);
		}
	}

	if (res < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FFDec] PID %s failed to decode frame PTS "LLU": %s\n", gf_filter_pid_get_name(ctx->in_pid), pkt.pts, av_err2str(res) ));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	if (!gotpic) return GF_OK;

	pix_fmt = ffmpeg_pixfmt_to_gpac(ctx->decoder->pix_fmt);
	if (!pix_fmt) pix_fmt = GF_PIXEL_RGB;

	//update all props
	FF_CHECK_PROP_VAL(pixel_fmt, pix_fmt, GF_PROP_PID_PIXFMT)
	FF_CHECK_PROP(width, width, GF_PROP_PID_WIDTH)
	FF_CHECK_PROP(height, height, GF_PROP_PID_HEIGHT)

	stride = stride_uv = uv_height = nb_planes = 0;
	if (! gf_pixel_get_size_info(pix_fmt, ctx->width, ctx->height, &outsize, &stride, &stride_uv, &nb_planes, &uv_height) ) {
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

	memset(&pict, 0, sizeof(pict));


	pck_src = NULL;
	count = gf_list_count(ctx->src_packets);
	for (i=0; i<count; i++) {
		pck_src = gf_list_get(ctx->src_packets, i);
		if (gf_filter_pck_get_cts(pck_src) == frame->pkt_pts) break;
		pck_src = NULL;
	}

	seek_flag = GF_FALSE;
	if (pck_src) {
		seek_flag = gf_filter_pck_get_seek_flag(pck_src);
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

	if (pck_src) {
		if (dst_pck) gf_filter_pck_merge_properties(pck_src, dst_pck);
		gf_list_del_item(ctx->src_packets, pck_src);
		gf_filter_pck_unref(pck_src);
	} else {
		if (dst_pck) gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1);
	}
	if (!dst_pck) return GF_OUT_OF_MEM;


	//TODO: cleanup, we should not convert pixel format in the decoder but through filters !
	switch (ctx->pixel_fmt) {
	case GF_PIXEL_RGB:
		pict.data[0] =  (uint8_t *)out_buffer;
		pict.linesize[0] = 3*ctx->width;
		pix_out = AV_PIX_FMT_RGB24;
		break;
	case GF_PIXEL_RGBA:
		pict.data[0] =  (uint8_t *)out_buffer;
		pict.linesize[0] = 4*ctx->width;
		pix_out = AV_PIX_FMT_RGBA;
		break;
	case GF_PIXEL_YUV:
	case GF_PIXEL_YUV_10:
		pict.data[0] =  (uint8_t *)out_buffer;
		pict.data[1] =  (uint8_t *)out_buffer + ctx->stride * ctx->height;
		pict.data[2] =  (uint8_t *)pict.data[1] + ctx->stride_uv * uv_height;
		pict.linesize[0] = ctx->stride;
		pict.linesize[1] = pict.linesize[2] = ctx->stride_uv;
		if (ctx->pixel_fmt == GF_PIXEL_YUV_10)
			pix_out = AV_PIX_FMT_YUV420P10LE;
		else
			pix_out = AV_PIX_FMT_YUV420P;
		break;

	case GF_PIXEL_YUV422:
	case GF_PIXEL_YUV422_10:
		pict.data[0] =  (uint8_t *)out_buffer;
		pict.data[1] =  (uint8_t *)out_buffer + ctx->stride * ctx->height;
		pict.data[2] =  (uint8_t *)pict.data[1] + ctx->stride_uv * ctx->height;
		pict.linesize[0] = ctx->stride;
		pict.linesize[1] = pict.linesize[2] = ctx->stride_uv;
		if (ctx->pixel_fmt == GF_PIXEL_YUV422_10)
			pix_out = AV_PIX_FMT_YUV422P10LE;
		else
			pix_out = AV_PIX_FMT_YUV422P;
		break;

	case GF_PIXEL_YUV444:
	case GF_PIXEL_YUV444_10:
		pict.data[0] =  (uint8_t *)out_buffer;
		pict.data[1] =  (uint8_t *)out_buffer + ctx->stride * ctx->height;
		pict.data[2] =  (uint8_t *)out_buffer + 2*ctx->stride * ctx->height;
		pict.linesize[0] = pict.linesize[1] = pict.linesize[2] = ctx->stride;
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
	                                   ctx->decoder->width, ctx->decoder->height, ctx->decoder->pix_fmt,
	                                   ctx->width, ctx->height, pix_out, SWS_BICUBIC, NULL, NULL, NULL);
	if (ctx->sws_ctx) {
		sws_scale(ctx->sws_ctx, (const uint8_t * const*)frame->data, frame->linesize, 0, ctx->height, pict.data, pict.linesize);
	}

	gf_filter_pck_set_seek_flag(dst_pck, GF_FALSE);

	if (frame->interlaced_frame)
		gf_filter_pck_set_interlaced(dst_pck, frame->top_field_first ? 2 : 1);

	gf_filter_pck_send(dst_pck);
	return GF_OK;
}


static GF_Err ffdec_process_audio(GF_Filter *filter, struct _gf_ffdec_ctx *ctx)
{
	AVPacket pkt;
	s32 gotpic;
	s32 len, in_size, i;
	u32 output_size;
	Bool is_eos=GF_FALSE;
	u8 *data;
	AVFrame *frame;
	GF_FilterPacket *dst_pck, *src_pck;
	GF_FilterPacket *pck = gf_filter_pid_get_packet(ctx->in_pid);

	if (ctx->reconfig_pending) {
		pck = NULL;
	} else if (!pck) {
		is_eos = gf_filter_pid_is_eos(ctx->in_pid);
		if (!is_eos) return GF_OK;
	}

	av_init_packet(&pkt);
	if (pck) pkt.data = (uint8_t *) gf_filter_pck_get_data(pck, &in_size);

	if (pck) {
		src_pck = pck;
		gf_filter_pck_ref_props(&src_pck);
		if (src_pck) gf_list_add(ctx->src_packets, src_pck);

		if (!pkt.data) {
			gf_filter_pid_drop_packet(ctx->in_pid);
			return GF_OK;
		}

		pkt.pts = gf_filter_pck_get_cts(pck);
		pkt.dts = gf_filter_pck_get_dts(pck);

		pkt.size = in_size;
		if ((s32) ctx->frame_start > pkt.size) ctx->frame_start = 0;
		//seek to last byte consumed by the previous decode4()
		else if (ctx->frame_start) {
			pkt.data += ctx->frame_start;
			pkt.size -= ctx->frame_start;
		}
		pkt.duration = gf_filter_pck_get_duration(pck);
		if (gf_filter_pck_get_sap(pck)>0)
			pkt.flags = AV_PKT_FLAG_KEY;

	} else {
		pkt.size = 0;
	}

	frame = ctx->frame;
	len = avcodec_decode_audio4(ctx->decoder, frame, &gotpic, &pkt);

	//this will handle eos as well
	if ((len<0) || !gotpic) {
		ctx->frame_start = 0;
		if (pck) gf_filter_pid_drop_packet(ctx->in_pid);
		if (pkt.size && (len<0)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FFDec] PID %s failed to decode frame PTS "LLU": %s\n", gf_filter_pid_get_name(ctx->in_pid), pkt.pts, av_err2str(len) ));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		if (is_eos) {
			gf_filter_pid_set_eos(ctx->out_pid);
			return GF_EOS;
		}
		if (ctx->reconfig_pending) {
			if (ctx->decoder->extradata) gf_free(ctx->decoder->extradata);
			ctx->decoder->extradata = NULL;
			avcodec_close(ctx->decoder);
			ctx->decoder = NULL;
			ctx->reconfig_pending = GF_FALSE;
			//these properties are checked after decode, when we reconfigure we copy props from input to output
			//so we need to make sure we retrigger pid config even if these did not change
			ctx->sample_fmt = 0;
			ctx->sample_rate = 0;
			ctx->channels = 0;
			ctx->channel_layout = 0;
			GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[FFDec] PID %s reconfigure pending and all frames flushed, reconfguring\n", gf_filter_pid_get_name(ctx->in_pid) ));
			return ffdec_configure_pid(filter, ctx->in_pid, GF_FALSE);
		}
		return GF_OK;
	}

	FF_CHECK_PROP(channels, channels, GF_PROP_PID_NUM_CHANNELS)
	FF_CHECK_PROP(channel_layout, channel_layout, GF_PROP_PID_CHANNEL_LAYOUT)
	FF_CHECK_PROP(sample_rate, sample_rate, GF_PROP_PID_SAMPLE_RATE)

	output_size = frame->nb_samples*ctx->channels*ctx->bytes_per_sample;
	dst_pck = gf_filter_pck_new_alloc(ctx->out_pid, output_size, &data);

	switch (frame->format) {
	case AV_SAMPLE_FMT_U8P:
	case AV_SAMPLE_FMT_S16P:
	case AV_SAMPLE_FMT_S32P:
	case AV_SAMPLE_FMT_FLTP:
	case AV_SAMPLE_FMT_DBLP:
		for (i=0; (u32) i< ctx->channels; i++) {
			char *inputChannel = frame->extended_data[i];
			memcpy(data, inputChannel, ctx->bytes_per_sample * frame->nb_samples);
			data += ctx->bytes_per_sample * frame->nb_samples;
		}
		break;
	default:
		memcpy(data, ctx->frame->data[0], ctx->bytes_per_sample * frame->nb_samples * ctx->channels);
		break;
	}

	//we don't follow the same approach as in video, we assume the codec works with one in one out
	//and use the first entry in src packets to match in order to copy the properties
	//a nicer approach would be to count delay frames (number of frames used to initialize)
	//and backmerge properties from the last packet in to the last-nb_init_frames
	src_pck = gf_list_get(ctx->src_packets, 0);

	if (src_pck) {
		gf_filter_pck_merge_properties(src_pck, dst_pck);
		gf_list_rem(ctx->src_packets, 0);
		gf_filter_pck_unref(src_pck);
	}

	if (frame->pkt_pts != AV_NOPTS_VALUE) {
		u64 pts = frame->pkt_pts;
		u32 timescale = gf_filter_pck_get_timescale(pck);
		if (ctx->nb_samples_already_in_frame) {
			if (ctx->sample_rate == timescale) {
				pts += ctx->nb_samples_already_in_frame;
			}
		}
		gf_filter_pck_set_cts(dst_pck, pts);
	}
	if (frame->pkt_dts != AV_NOPTS_VALUE) {
		gf_filter_pck_set_dts(dst_pck, frame->pkt_dts);
	}


	gf_filter_pck_send(dst_pck);

	ctx->frame_start += len;
	//done with this input packet
	if (in_size <= (s32) ctx->frame_start) {
		frame->nb_samples = 0;
		ctx->frame_start = 0;
		ctx->nb_samples_already_in_frame = 0;
		gf_filter_pid_drop_packet(ctx->in_pid);
		return GF_OK;
	}
	//still some data to decode in packet, don't drop it
	//todo: check if frame->pkt_pts or frame->pts is updated by ffmpeg, otherwise do it ourselves !
	GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FFDec] Code not yet tested  - frame PTS was "LLU" - nb samples dec %d\n", frame->pkt_pts, frame->nb_samples));
	ctx->nb_samples_already_in_frame += frame->nb_samples;
	frame->nb_samples = 0;

	return ffdec_process_audio(filter, ctx);
}

#ifdef FF_SUB_SUPPORT
static GF_Err ffdec_process_subtitle(GF_Filter *filter, struct _gf_ffdec_ctx *ctx)
{
	AVPacket pkt;
	AVSubtitle subs;
	s32 gotpic;
	s32 len, in_size;
	u32 output_size;
	Bool is_eos=GF_FALSE;
	char *data;
	AVFrame *frame;
	GF_FilterPacket *dst_pck;

	GF_FilterPacket *pck = gf_filter_pid_get_packet(ctx->in_pid);

	if (!pck) {
		is_eos = gf_filter_pid_is_eos(ctx->in_pid);
		if (!is_eos) return GF_OK;
	}
	av_init_packet(&pkt);
	if (pck) pkt.data = (uint8_t *) gf_filter_pck_get_data(pck, &in_size);

	if (!is_eos) {
		u64 dts;
		pkt.pts = gf_filter_pck_get_cts(pck);

		//copy over SAP and duration in dts
		dts = gf_filter_pck_get_sap(pck);
		dts <<= 32;
		dts |= gf_filter_pck_get_duration(pck);
		pkt.dts = dts;

		pkt.size = in_size;
		if (ctx->frame_start > pkt.size) ctx->frame_start = 0;
		//seek to last byte consumed by the previous decode4()
		else if (ctx->frame_start) {
			pkt.data += ctx->frame_start;
			pkt.size -= ctx->frame_start;
		}
	} else {
		pkt.size = 0;
	}

	memset(&subs, 0, sizeof(AVSubtitle));
	frame = ctx->frame;
	len = avcodec_decode_subtitle2(ctx->decoder, &subs, &gotpic, &pkt);

	//this will handle eos as well
	if ((len<0) || !gotpic) {
		ctx->frame_start = 0;
		if (pck) gf_filter_pid_drop_packet(ctx->in_pid);
		if (len<0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FFDec] PID %s failed to decode frame PTS "LLU": %s\n", gf_filter_pid_get_name(ctx->in_pid), pkt.pts, av_err2str(len) ));
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

static GF_Err ffdec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	s32 res;
	u32 type=0, gpac_codecid=0;
	const GF_PropertyValue *prop;
	GF_FFDecodeCtx *ctx = (GF_FFDecodeCtx *) gf_filter_get_udta(filter);

	//disconnect of src pid (not yet supported)
	if (is_remove) {
		//one in one out, this is simple
		if (ctx->out_pid) gf_filter_pid_remove(ctx->out_pid);
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
	ctx->width = prop ? prop->value.uint : 320;
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
	ctx->height = prop ? prop->value.uint : 240;


	if (gpac_codecid == GF_CODECID_FFMPEG) {
		AVCodec *codec=NULL;
		prop = gf_filter_pid_get_property(pid, GF_FFMPEG_DECODER_CONFIG);
		if (!prop || !prop->value.ptr) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FFDec] PID %s codec context not exposed by demuxer !\n", gf_filter_pid_get_name(pid) ));
			return GF_SERVICE_ERROR;
		}
		ctx->decoder = prop->value.ptr;
		codec = avcodec_find_decoder(ctx->decoder->codec_id);
		if (!codec) return GF_NOT_SUPPORTED;

		res = avcodec_open2(ctx->decoder, codec, NULL );
		if (res < 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FFDec] PID %s failed to open codec context: %s\n", gf_filter_pid_get_name(pid), av_err2str(res) ));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
	}
	//we reconfigure the stream
	else {
		AVCodec *codec=NULL;
		u32 codec_id, ff_codectag=0;
		if (ctx->decoder) {
			u32 codec_id = ffmpeg_codecid_from_gpac(gpac_codecid, NULL);
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
		if (codec_id) codec = avcodec_find_decoder(codec_id);
		if (!codec) return GF_NOT_SUPPORTED;


		ctx->decoder = avcodec_alloc_context3(NULL);
		if (! ctx->decoder) return GF_OUT_OF_MEM;
		ctx->owns_context = GF_TRUE;
		if (ff_codectag)
			ctx->decoder->codec_tag = ff_codectag;

		ffmpeg_set_enc_dec_flags(ctx->options, ctx->decoder);

		//we may have a dsi here!
		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
		if (prop && prop->value.data.ptr && prop->value.data.size) {
			//looks loke ffmpeg wants the fLaC keyword
			if (gpac_codecid==GF_CODECID_FLAC) {
				ctx->decoder->extradata_size = prop->value.data.size+4;
				ctx->decoder->extradata = gf_malloc(sizeof(char) * prop->value.data.size+4);
				memcpy(ctx->decoder->extradata, "fLaC", 4);
				memcpy(ctx->decoder->extradata+4, prop->value.data.ptr, prop->value.data.size);
			} else {
				ctx->decoder->extradata_size = prop->value.data.size;
				ctx->decoder->extradata = gf_malloc(sizeof(char) * prop->value.data.size);
				memcpy(ctx->decoder->extradata, prop->value.data.ptr, prop->value.data.size);
			}
			ctx->extra_data_crc = gf_crc_32(prop->value.data.ptr, prop->value.data.size);
		}

		res = avcodec_open2(ctx->decoder, codec, NULL );
		if (res < 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FFDec] PID %s failed to open codec context: %s\n", gf_filter_pid_get_name(pid), av_err2str(res) ));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
	}

	AVDictionaryEntry *prev_e = NULL;
	while (1) {
		prev_e = av_dict_get(ctx->options, "", prev_e, AV_DICT_IGNORE_SUFFIX);
		if (!prev_e) break;
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[FFDEC] meta-filter option %s=%s set but not used, see gpac -h ffdec and gpac -h ffdec:%s for allowed options\n", prev_e->key, prev_e->value, ctx->decoder->codec->priv_class->class_name));
		gf_filter_report_unused_meta_option(filter, prev_e->key);
	}


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
		ctx->process = ffdec_process_video;
		//for some streams, we don't have w/h/pixfmt after opening the decoder
		//to make sure we are not confusing potential filters expecting them, init to default values
		if (ctx->decoder->pix_fmt>=0) {
			pix_fmt = ffmpeg_pixfmt_to_gpac(ctx->decoder->pix_fmt);
			if (!pix_fmt) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[FFDec] Unsupported pixel format %d, defaulting to RGB\n", pix_fmt));
				pix_fmt = GF_PIXEL_RGB;
			}
			FF_CHECK_PROP_VAL(pixel_fmt, pix_fmt, GF_PROP_PID_PIXFMT)
		} else {
			ctx->pixel_fmt = GF_PIXEL_YUV;
			gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_PIXFMT, &PROP_UINT( ctx->pixel_fmt) );
		}

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
		ctx->sample_fmt = ffmpeg_audio_fmt_to_gpac(ctx->decoder->sample_fmt);
		gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(ctx->sample_fmt) );
		ctx->bytes_per_sample = gf_audio_fmt_bit_depth(ctx->sample_fmt) / 8;

		//override PID props with what decoder gives us
		if (ctx->decoder->channels) {
			FF_CHECK_PROP(channels, channels, GF_PROP_PID_NUM_CHANNELS)
		}
		if (ctx->decoder->channel_layout) {
			u32 ch_lay = ffmpeg_channel_layout_to_gpac((u32) ctx->decoder->channel_layout);
			if (ctx->channel_layout != ch_lay) {
				gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_CHANNEL_LAYOUT, &PROP_UINT(ch_lay ) );
				ctx->channel_layout = ch_lay;
			}
		}
		if (ctx->decoder->sample_rate) {
			FF_CHECK_PROP(sample_rate, sample_rate, GF_PROP_PID_SAMPLE_RATE)
		}
		if (!ctx->frame)
			ctx->frame = av_frame_alloc();

		if (ctx->sample_fmt) {
			gf_filter_pid_set_property(ctx->out_pid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT( ctx->sample_fmt) );
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

static const GF_FilterCapability FFDecodeCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_SVC),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_LHVC),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_TILE_BASE, GF_TRUE),
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
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_ISOM_SUBTYPE_TX3G),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_ISOM_SUBTYPE_WVTT),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_ISOM_SUBTYPE_STPP),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_ISOM_SUBTYPE_STXT),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
#endif

};

GF_FilterRegister FFDecodeRegister = {
	.name = "ffdec",
	.version = LIBAVCODEC_IDENT,
	GF_FS_SET_DESCRIPTION("FFMPEG decoder")
	GF_FS_SET_HELP("See FFMPEG documentation (https://ffmpeg.org/documentation.html) for more detailed info on decoder options")
	.private_size = sizeof(GF_FFDecodeCtx),
	SETCAPS(FFDecodeCaps),
	.initialize = ffdec_initialize,
	.finalize = ffdec_finalize,
	.configure_pid = ffdec_configure_pid,
	.process = ffdec_process,
	.update_arg = ffdec_update_arg,
	//use middle priorty, so that hardware decs/other native impl in gpac can take over if needed
	//don't use lowest one since we use this for scalable codecs
	.priority = 128

};


static const GF_FilterArgs FFDecodeArgs[] =
{
	{ "*", -1, "any possible args defined for AVCodecContext and sub-classes. See `gpac -hx ffdec` and `gpac -hx ffdec:*`", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_META},
	{0}
};

void ffdec_regfree(GF_FilterSession *session, GF_FilterRegister *reg)
{
	ffmpeg_register_free(session, reg, 0);
}

const GF_FilterRegister *ffdec_register(GF_FilterSession *session)
{
	GF_FilterArgs *args;
	u32 i=0, idx=0;
	Bool load_meta_filters = session ? GF_TRUE : GF_FALSE;
	AVCodecContext *dec_ctx;
	const struct AVOption *opt;

	ffmpeg_initialize();

	//by default no need to load option descriptions, everything is handled by av_set_opt in update_args
	if (!load_meta_filters) {
		FFDecodeRegister.args = FFDecodeArgs;
		FFDecodeRegister.register_free = NULL;
		return &FFDecodeRegister;
	}

	FFDecodeRegister.register_free = ffdec_regfree;
	dec_ctx = avcodec_alloc_context3(NULL);

	idx=0;
	while (dec_ctx->av_class->option) {
		opt = &dec_ctx->av_class->option[idx];
		if (!opt || !opt->name) break;
		if (opt->flags & AV_OPT_FLAG_DECODING_PARAM)
			i++;
		idx++;
	}
	i+=1;

	args = gf_malloc(sizeof(GF_FilterArgs)*(i+1));
	memset(args, 0, sizeof(GF_FilterArgs)*(i+1));
	FFDecodeRegister.args = args;
	i=0;
	idx=0;
	while (dec_ctx->av_class->option) {
		opt = &dec_ctx->av_class->option[idx];
		if (!opt || !opt->name) break;
		if (opt->flags & AV_OPT_FLAG_DECODING_PARAM) {
			args[i] = ffmpeg_arg_translate(opt);
			i++;
		}
		idx++;
	}
	args[i+1] = (GF_FilterArgs) { "*", -1, "options depend on codec type, check individual filter syntax", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_EXPERT};

#if (LIBAVCODEC_VERSION_MAJOR >= 58) && (LIBAVCODEC_VERSION_MINOR>=20)
	avcodec_free_context(&dec_ctx);
#else
	av_free(dec_ctx);
#endif

	ffmpeg_expand_register(session, &FFDecodeRegister, FF_REG_TYPE_DECODE);

	return &FFDecodeRegister;
}

#else
#include <gpac/filters.h>
const GF_FilterRegister *ffdec_register(GF_FilterSession *session)
{
	return NULL;
}
#endif

