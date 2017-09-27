/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017
 *					All rights reserved
 *
 *  This file is part of GPAC / ffmpeg decode filters
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
#include <gpac/list.h>
#include <gpac/constants.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/dict.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>

#define FF_CHECK_PROP(_name, _ffname, _type)	if (ffdec->_name != ffdec->codec_ctx->_ffname) { \
		gf_filter_pid_set_property(ffdec->out_pid, _type, &PROP_UINT( ffdec->codec_ctx->_ffname ) );	\
		ffdec->_name = ffdec->codec_ctx->_ffname;	\
	} \

#define FF_CHECK_PROP_VAL(_name, _val, _type)	if (ffdec->_name != _val) { \
		gf_filter_pid_set_property(ffdec->out_pid, _type, &PROP_UINT( _val ) );	\
		ffdec->_name = _val;	\
	} \



typedef struct _gf_ffdec_ctx
{
	//internal data
	Bool initialized;

	Bool owns_context;
	AVCodecContext *codec_ctx;
	//decode options
	AVDictionary *options;

	GF_FilterPid *in_pid, *out_pid;
	//media type
	u32 type;
	//CRC32 of extra_data
	u32 extra_data_crc;

	GF_Err (*process)(GF_Filter *filter, struct _gf_ffdec_ctx *ffdec);

	u32 flush_done;

	//for now we don't share the data
	AVFrame *frame;
	//audio state
	u32 channels, sample_rate, sample_fmt, channel_layout;
	u32 frame_start;

	//video state
	u32 width, height, pixel_fmt, stride, stride_uv;
	GF_Fraction sar;
	struct SwsContext *sws_ctx;
} GF_FFDecodeCtx;

static GF_Err ffdec_initialize(GF_Filter *filter)
{
	GF_FFDecodeCtx *ffd = (GF_FFDecodeCtx *) gf_filter_get_udta(filter);
	ffd->initialized = GF_TRUE;
	return GF_OK;
}

static void ffdec_finalize(GF_Filter *filter)
{
	GF_FFDecodeCtx *ffdec = (GF_FFDecodeCtx *) gf_filter_get_udta(filter);
	if (ffdec->options) av_dict_free(&ffdec->options);
	if (ffdec->frame) av_frame_free(&ffdec->frame);
	if (ffdec->sws_ctx) sws_freeContext(ffdec->sws_ctx);

	if (ffdec->owns_context && ffdec->codec_ctx) {
		if (ffdec->codec_ctx->extradata) gf_free(ffdec->codec_ctx->extradata);
		avcodec_close(ffdec->codec_ctx);
	}
	return;
}

static u32 ffdec_gpac_convert_pix_fmt(u32 pix_fmt)
{
	switch (pix_fmt) {
	case PIX_FMT_YUV420P: return GF_PIXEL_YV12;
	case PIX_FMT_YUV420P10LE: return  GF_PIXEL_YV12_10;
	case PIX_FMT_YUV422P: return GF_PIXEL_YUV422;
	case PIX_FMT_YUV422P10LE: return GF_PIXEL_YUV422_10;
	case PIX_FMT_YUV444P: return GF_PIXEL_YUV444;
	case PIX_FMT_YUV444P10LE: return GF_PIXEL_YUV444_10;
	case PIX_FMT_RGBA: return GF_PIXEL_RGBA;
	case PIX_FMT_RGB24: return GF_PIXEL_RGB_24;
	case PIX_FMT_BGR24: return GF_PIXEL_BGR_24;
	//force output in RGB24
	case PIX_FMT_YUVJ420P: return GF_PIXEL_RGB_24;
	case PIX_FMT_YUVJ422P: return GF_PIXEL_RGB_24;
	case PIX_FMT_YUVJ444P: return GF_PIXEL_RGB_24;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[FFDec] Unsupported pixel format %d\n", pix_fmt));
	}
	return 0;
}

static GF_Err ffdec_process_video(GF_Filter *filter, struct _gf_ffdec_ctx *ffdec)
{
	AVPacket pkt;
	AVFrame *frame;
	AVPicture pict;
	Bool is_eos=GF_FALSE;
	s32 res;
	s32 gotpic;
	const char *data = NULL;
	Bool seek_flag = GF_FALSE;
	u8 sap_type = 0;
	u32 pck_duration = 0;
	u32 size=0, pix_fmt, outsize, pix_out, stride;
	char *out_buffer;
	GF_FilterPacket *dst_pck;
	GF_FilterPacket *pck = gf_filter_pid_get_packet(ffdec->in_pid);

	if (!pck) {
		is_eos = gf_filter_pid_is_eos(ffdec->in_pid);
		if (!is_eos) return GF_OK;
	}
	frame = ffdec->frame;

	av_init_packet(&pkt);

	if (pck) data = gf_filter_pck_get_data(pck, &size);

	if (!is_eos) {
		u64 flags;
		seek_flag = gf_filter_pck_get_seek_flag(pck);
		//copy over SAP and duration in dts - alternatively we could ref_inc the packet and pass its adress here
		flags = gf_filter_pck_get_sap(pck);

		//seems ffmpeg is not properly handling the decoding after a flush, we close and reopen the codec
		if (ffdec->flush_done) {
			const AVCodec *codec = ffdec->codec_ctx->codec;
			avcodec_close(ffdec->codec_ctx);
			avcodec_open2(ffdec->codec_ctx, codec, NULL );
			ffdec->flush_done = GF_FALSE;
		}

		flags <<= 16;
		flags |= seek_flag;
		flags <<= 32;
		flags |= gf_filter_pck_get_duration(pck);
		pkt.dts = flags;
		pkt.pts = gf_filter_pck_get_cts(pck);
	}
	pkt.data = (uint8_t*)data;
	pkt.size = size;

	/*TOCHECK: for AVC bitstreams after ISMA decryption, in case (as we do) the decryption DRM tool
	doesn't put back nalu size, we have to do it ourselves, but we can't modify input data...*/

	gotpic=0;
	res = avcodec_decode_video2(ffdec->codec_ctx, frame, &gotpic, &pkt);

	//not end of stream, or no more data to flush
	if (!is_eos || !gotpic) {
		if (pck) gf_filter_pid_drop_packet(ffdec->in_pid);
		if (is_eos) {
			ffdec->flush_done = GF_TRUE;
			gf_filter_pid_set_eos(ffdec->out_pid);
			return GF_EOS;
		}
	}

	if (res < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[FFDecode] PID %s failed to decode frame PTS "LLU": %s\n", gf_filter_pid_get_name(ffdec->in_pid), pkt.pts, av_err2str(res) ));
		return GF_NON_COMPLIANT_BITSTREAM;
		/*TODO: check if we want to switch to H263 (ffmpeg MPEG-4 codec doesn't understand short headers)
		haven't seen such bitstream in a while, for now the feature is droped*/
	}
	if (!gotpic) return GF_OK;

	pix_fmt = ffdec_gpac_convert_pix_fmt(ffdec->codec_ctx->pix_fmt);
	if (!pix_fmt) {
		return GF_NOT_SUPPORTED;
	}
	//update all props
	FF_CHECK_PROP_VAL(pixel_fmt, pix_fmt, GF_PROP_PID_PIXFMT)
	FF_CHECK_PROP(width, width, GF_PROP_PID_WIDTH)
	FF_CHECK_PROP(height, height, GF_PROP_PID_HEIGHT)

	switch (pix_fmt) {
	case GF_PIXEL_RGB_32:
	case GF_PIXEL_BGR_32:
	case GF_PIXEL_ARGB:
	case GF_PIXEL_RGBA:
		stride = ffdec->width * 4;
		outsize = stride * ffdec->height;
		break;
	case GF_PIXEL_RGB_24:
	case GF_PIXEL_BGR_24:
		stride = ffdec->width * 3;
		outsize = stride * ffdec->height;
		break;
	case GF_PIXEL_YV12:
		stride = ffdec->width;
		outsize = 3*stride * ffdec->height / 2;
		break;
	case GF_PIXEL_YV12_10:
		stride = ffdec->width*2;
		outsize = 3*stride * ffdec->height / 2;
		break;
	case GF_PIXEL_YUV422:
		stride = ffdec->width;
		outsize = stride * ffdec->height * 2;
		break;
	case GF_PIXEL_YUV422_10:
		stride = ffdec->width*2;
		outsize = stride * ffdec->height * 2;
		break;
	case GF_PIXEL_YUV444:
		stride = ffdec->width;
		outsize = stride * ffdec->height * 3;
		break;
	case GF_PIXEL_YUV444_10:
		stride = ffdec->width*2;
		outsize = stride * ffdec->height * 3;
		break;
	default:
		return GF_NOT_SUPPORTED;
	}

	FF_CHECK_PROP_VAL(stride, stride, GF_PROP_PID_STRIDE)
	if (ffdec->sar.num * ffdec->codec_ctx->sample_aspect_ratio.den != ffdec->sar.den * ffdec->codec_ctx->sample_aspect_ratio.num) {
		ffdec->sar.num = ffdec->codec_ctx->sample_aspect_ratio.num;
		ffdec->sar.den = ffdec->codec_ctx->sample_aspect_ratio.den;

		gf_filter_pid_set_property(ffdec->out_pid, GF_PROP_PID_SAR, &PROP_FRAC( ffdec->sar.num, ffdec->sar.den ) );
	}

	memset(&pict, 0, sizeof(pict));

	//copy over SAP and duration indication
	seek_flag = GF_FALSE;
	sap_type = 0;
	if (frame->pkt_dts) {
		u32 flags = frame->pkt_dts>>32;
		seek_flag = flags & 0xFFFF;
		sap_type = (flags>>16) & 0xFFFF;
		pck_duration = (frame->pkt_dts & 0xFFFFFFFFUL);
	}
	//this was a seek frame, do not dispatch
	if (seek_flag)
		return GF_OK;

	dst_pck = gf_filter_pck_new_alloc(ffdec->out_pid, outsize, &out_buffer);
	if (!dst_pck) return GF_OUT_OF_MEM;

	switch (ffdec->pixel_fmt) {
	case GF_PIXEL_RGB_24:
		pict.data[0] =  (uint8_t *)out_buffer;
		pict.linesize[0] = 3*ffdec->width;
		pix_out = PIX_FMT_RGB24;
		break;
	case GF_PIXEL_RGBA:
		pict.data[0] =  (uint8_t *)out_buffer;
		pict.linesize[0] = 4*ffdec->width;
		pix_out = PIX_FMT_RGBA;
		break;
	case GF_PIXEL_YV12:
		pict.data[0] =  (uint8_t *)out_buffer;
		pict.data[1] =  (uint8_t *)out_buffer + ffdec->stride * ffdec->height;
		pict.data[2] =  (uint8_t *)out_buffer + 5 * ffdec->stride * ffdec->height / 4;
		pict.linesize[0] = ffdec->stride;
		pict.linesize[1] = pict.linesize[2] = ffdec->stride/2;
		pix_out = PIX_FMT_YUV420P;
		break;
	case GF_PIXEL_YUV422:
		pict.data[0] =  (uint8_t *)out_buffer;
		pict.data[1] =  (uint8_t *)out_buffer + ffdec->stride * ffdec->height;
		pict.data[2] =  (uint8_t *)out_buffer + 3*ffdec->stride * ffdec->height/2;
		pict.linesize[0] = ffdec->stride;
		pict.linesize[1] = pict.linesize[2] = ffdec->stride/2;
		pix_out = PIX_FMT_YUV422P;
		break;
	case GF_PIXEL_YUV444:
		pict.data[0] =  (uint8_t *)out_buffer;
		pict.data[1] =  (uint8_t *)out_buffer + ffdec->stride * ffdec->height;
		pict.data[2] =  (uint8_t *)out_buffer + 2*ffdec->stride * ffdec->height;
		pict.linesize[0] = pict.linesize[1] = pict.linesize[2] = ffdec->stride;
		pix_out = PIX_FMT_YUV444P;
		break;
	case GF_PIXEL_YV12_10:
		pict.data[0] =  (uint8_t *)out_buffer;
		pict.data[1] =  (uint8_t *)out_buffer + ffdec->stride * ffdec->height;
		pict.data[2] =  (uint8_t *)out_buffer + 5 * ffdec->stride * ffdec->height / 4;
		pict.linesize[0] = ffdec->stride;
		pict.linesize[1] = pict.linesize[2] = ffdec->stride/2;
		pix_out = PIX_FMT_YUV420P10LE;
		break;
	case GF_PIXEL_YUV422_10:
		pict.data[0] =  (uint8_t *)out_buffer;
		pict.data[1] =  (uint8_t *)out_buffer + ffdec->stride * ffdec->height;
		pict.data[2] =  (uint8_t *)out_buffer + 3*ffdec->stride * ffdec->height/2;
		pict.linesize[0] = ffdec->stride;
		pict.linesize[1] = pict.linesize[2] = ffdec->stride/2;
		pix_out = PIX_FMT_YUV422P10LE;
	case GF_PIXEL_YUV444_10:
		pict.data[0] =  (uint8_t *)out_buffer;
		pict.data[1] =  (uint8_t *)out_buffer + ffdec->stride * ffdec->height;
		pict.data[2] =  (uint8_t *)out_buffer + 2*ffdec->stride * ffdec->height;
		pict.linesize[0] = pict.linesize[1] = pict.linesize[2] = ffdec->stride;
		pix_out = PIX_FMT_YUV444P10LE;
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[FFDecode] Unsupported pixel format %s\n", av_get_pix_fmt_name(ffdec->codec_ctx->pix_fmt) ));

		gf_filter_pck_discard(dst_pck);

		return GF_NOT_SUPPORTED;
	}

	ffdec->sws_ctx = sws_getCachedContext(ffdec->sws_ctx,
	                                   ffdec->codec_ctx->width, ffdec->codec_ctx->height, ffdec->codec_ctx->pix_fmt,
	                                   ffdec->width, ffdec->height, pix_out, SWS_BICUBIC, NULL, NULL, NULL);
	if (ffdec->sws_ctx) {
		sws_scale(ffdec->sws_ctx, (const uint8_t * const*)frame->data, frame->linesize, 0, ffdec->height, pict.data, pict.linesize);
	}

	gf_filter_pck_set_cts(dst_pck, frame->pkt_pts);
	//copy over SAP and duration indication
	gf_filter_pck_set_duration(dst_pck, pck_duration);
	gf_filter_pck_set_sap(dst_pck, sap_type);


	if (frame->interlaced_frame)
		gf_filter_pck_set_interlaced(dst_pck, frame->top_field_first ? 2 : 1);

	gf_filter_pck_send(dst_pck);
	return GF_OK;
}


static GF_Err ffdec_process_audio(GF_Filter *filter, struct _gf_ffdec_ctx *ffdec)
{
	AVPacket pkt;
	s32 gotpic;
	s32 len, in_size;
	u32 output_size;
	Bool is_eos=GF_FALSE;
	char *data;
	AVFrame *frame;
	GF_FilterPacket *dst_pck;

	GF_FilterPacket *pck = gf_filter_pid_get_packet(ffdec->in_pid);

	if (!pck) {
		is_eos = gf_filter_pid_is_eos(ffdec->in_pid);
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
		if (ffdec->frame_start > pkt.size) ffdec->frame_start = 0;
		//seek to last byte consumed by the previous decode4()
		else if (ffdec->frame_start) {
			pkt.data += ffdec->frame_start;
			pkt.size -= ffdec->frame_start;
		}
	} else {
		pkt.size = 0;
	}

	frame = ffdec->frame;
	len = avcodec_decode_audio4(ffdec->codec_ctx, frame, &gotpic, &pkt);

	//this will handle eos as well
	if ((len<0) || !gotpic) {
		ffdec->frame_start = 0;
		if (pck) gf_filter_pid_drop_packet(ffdec->in_pid);
		if (len<0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[FFDecode] PID %s failed to decode frame PTS "LLU": %s\n", gf_filter_pid_get_name(ffdec->in_pid), pkt.pts, av_err2str(len) ));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		if (is_eos) {
			gf_filter_pid_set_eos(ffdec->out_pid);
			return GF_EOS;
		}
		return GF_OK;
	}

	FF_CHECK_PROP(channels, channels, GF_PROP_PID_NUM_CHANNELS)
	FF_CHECK_PROP(channel_layout, channel_layout, GF_PROP_PID_CHANNEL_LAYOUT)
	FF_CHECK_PROP(sample_rate, sample_rate, GF_PROP_PID_SAMPLE_RATE)

	output_size = frame->nb_samples*ffdec->channels*2;//always in s16 fmt
	dst_pck = gf_filter_pck_new_alloc(ffdec->out_pid, output_size, &data);

	if (frame->format==AV_SAMPLE_FMT_FLTP) {
		s32 i, j;
		s16 *output = (s16 *) data;
		for (j=0; j<ffdec->channels; j++) {
			Float* inputChannel = (Float*)frame->extended_data[j];
			for (i=0 ; i<frame->nb_samples ; i++) {
				Float sample = inputChannel[i];
				if (sample<-1.0f) sample=-1.0f;
				else if (sample>1.0f) sample=1.0f;

				output[i*ffdec->channels + j] = (int16_t) (sample * GF_SHORT_MAX );
			}
		}
	} else if (frame->format==AV_SAMPLE_FMT_S16P) {
		s32 i, j;
		s16 *output = (s16 *) data;
		for (j=0; j<ffdec->channels; j++) {
			s16* inputChannel = (s16*)frame->extended_data[j];
			for (i=0 ; i<frame->nb_samples ; i++) {
				Float sample = inputChannel[i];
				output[i*ffdec->channels + j] = (int16_t) (sample );
			}
		}
	} else if (frame->format==AV_SAMPLE_FMT_U8) {
		u32 i, size = frame->nb_samples * ffdec->channels;
		s16 *output = (s16 *) data;
		s8 *input = (s8 *) frame->data[0];
		for (i=0; i<size; i++) {
			output [i] = input[i] * 128;
		}
	} else if (frame->format==AV_SAMPLE_FMT_S32) {
		u32 i, shift, size = frame->nb_samples * ffdec->channels;
		s16 *output = (s16 *) data;
		s32 *input = (s32*) frame->data[0];
		shift = 1<<31;
		for (i=0; i<size; i++) {
			output [i] = input[i] * shift;
		}
	} else if (frame->format==AV_SAMPLE_FMT_FLT) {
		u32 i, size = frame->nb_samples * ffdec->channels;
		s16 *output = (s16 *) data;
		Float *input = (Float *) frame->data[0];
		for (i=0; i<size; i++) {
			Float sample = input[i];
			if (sample<-1.0f) sample=-1.0f;
			else if (sample>1.0f) sample=1.0f;
			output [i] = (int16_t) (sample * GF_SHORT_MAX);
		}
	} else if (frame->format==AV_SAMPLE_FMT_S16) {
		memcpy(data, frame->data[0], sizeof(char) * frame->nb_samples * ffdec->channels*2);
	} else if (frame->nb_samples) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FFDec] Raw Audio format %d not supported\n", frame->format ));
	}

	if (frame->pkt_pts != AV_NOPTS_VALUE)
		gf_filter_pck_set_cts(dst_pck, frame->pkt_pts);

	//copy over SAP and duration indication
	if (frame->pkt_dts) {
		u32 sap = frame->pkt_dts>>32;
		gf_filter_pck_set_duration(dst_pck, frame->pkt_dts & 0xFFFFFFFFUL);
		gf_filter_pck_set_sap(dst_pck, sap);
	}

	gf_filter_pck_send(dst_pck);

	frame->nb_samples = 0;

	ffdec->frame_start += len;
	//done with this input packet
	if (in_size <= ffdec->frame_start) {
		ffdec->frame_start = 0;
		gf_filter_pid_drop_packet(ffdec->in_pid);
		return GF_OK;
	}
	//still some data to decode in packet, don't drop it
	//todo: check if frame->pkt_pts or frame->pts is updated by ffmpeg, otherwise do it ourselves !
	GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FFDec] Code not yet tested !\n" ));
	return GF_OK;
}

//#defined FFDEC_SUB_SUPPORT
#ifdef FFDEC_SUB_SUPPORT
static GF_Err ffdec_process_subtitle(GF_Filter *filter, struct _gf_ffdec_ctx *ffdec)
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

	GF_FilterPacket *pck = gf_filter_pid_get_packet(ffdec->in_pid);

	if (!pck) {
		is_eos = gf_filter_pid_is_eos(ffdec->in_pid);
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
		if (ffdec->frame_start > pkt.size) ffdec->frame_start = 0;
		//seek to last byte consumed by the previous decode4()
		else if (ffdec->frame_start) {
			pkt.data += ffdec->frame_start;
			pkt.size -= ffdec->frame_start;
		}
	} else {
		pkt.size = 0;
	}

	memset(&subs, 0, sizeof(AVSubtitle));
	frame = ffdec->frame;
	len = avcodec_decode_subtitle2(ffdec->codec_ctx, &subs, &gotpic, &pkt);

	//this will handle eos as well
	if ((len<0) || !gotpic) {
		ffdec->frame_start = 0;
		if (pck) gf_filter_pid_drop_packet(ffdec->in_pid);
		if (len<0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[FFDecode] PID %s failed to decode frame PTS "LLU": %s\n", gf_filter_pid_get_name(ffdec->in_pid), pkt.pts, av_err2str(len) ));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		if (is_eos) {
			gf_filter_pid_set_eos(ffdec->out_pid);
			return GF_EOS;
		}
		return GF_OK;
	}

	avsubtitle_free(&subs);
	return GF_OK;
}
#endif

static GF_Err ffdec_process(GF_Filter *filter)
{
	GF_FFDecodeCtx *ffdec = (GF_FFDecodeCtx *) gf_filter_get_udta(filter);
	if (gf_filter_pid_would_block(ffdec->out_pid))
		return GF_OK;
	return ffdec->process(filter, ffdec);
}

static u32 ff_gpac_oti_to_codec_id(u32 oti)
{
	switch (oti) {
	case GPAC_OTI_AUDIO_MPEG1:
		return CODEC_ID_MP2;
	case GPAC_OTI_AUDIO_MPEG2_PART3:
		return CODEC_ID_MP3;
	case GPAC_OTI_AUDIO_AAC_MPEG4:
		return CODEC_ID_AAC;
	case GPAC_OTI_AUDIO_AC3:
		return CODEC_ID_AC3;
	case GPAC_OTI_AUDIO_AMR:
		return CODEC_ID_AMR_NB;
	case GPAC_OTI_AUDIO_AMR_WB:
		return CODEC_ID_AMR_WB;

	case GPAC_OTI_VIDEO_MPEG4_PART2:
		return CODEC_ID_MPEG4;
	case GPAC_OTI_VIDEO_AVC:
		return CODEC_ID_H264;
	case GPAC_OTI_VIDEO_MPEG1:
		return CODEC_ID_MPEG1VIDEO;
	case GPAC_OTI_VIDEO_MPEG2_MAIN:
		return CODEC_ID_MPEG2VIDEO;
	case GPAC_OTI_VIDEO_H263:
		return CODEC_ID_H263;

	case GPAC_OTI_IMAGE_JPEG:
		return CODEC_ID_MJPEG;
	case GPAC_OTI_IMAGE_PNG:
		return CODEC_ID_PNG;
	case GPAC_OTI_IMAGE_JPEG_2000:
		return CODEC_ID_JPEG2000;

	default:
		return 0;
	}
}

enum {
	GF_FFMPEG_DECODER_CONFIG = GF_4CC('f','f','D','C'),
};

static GF_Err ffdec_config_input(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	s32 res;
	u32 type=0, oti=0;
	const GF_PropertyValue *prop;
	GF_FFDecodeCtx  *ffdec = (GF_FFDecodeCtx *) gf_filter_get_udta(filter);

	//disconnect of src pid (not yet supported)
	if (is_remove) {
		//one in one out, this is simple
		if (ffdec->out_pid) gf_filter_pid_remove(ffdec->out_pid);
		return GF_OK;
	}

	//check our PID: streamtype and OTI
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (!prop) return GF_NOT_SUPPORTED;

	type = prop->value.uint;
	switch (type) {
	case GF_STREAM_AUDIO:
	case GF_STREAM_VISUAL:
#ifdef FFDEC_SUB_SUPPORT
	case GF_STREAM_TEXT:
#endif
		break;
	default:
		return GF_NOT_SUPPORTED;
	}
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_OTI);
	if (!prop) return GF_NOT_SUPPORTED;
	oti = prop->value.uint;


	//initial config or update
	if (!ffdec->in_pid || (ffdec->in_pid==pid)) {
		ffdec->in_pid = pid;
		if (!ffdec->type) ffdec->type = type;
		else if (ffdec->type != type) {
			return GF_NOT_SUPPORTED;
		}
	} else {
		//only one input pid in ffdec
		if (ffdec->in_pid) return GF_REQUIRES_NEW_INSTANCE;
	}



	if (oti == GPAC_OTI_MEDIA_FFMPEG) {
		AVCodec *codec=NULL;
		prop = gf_filter_pid_get_property(pid, GF_FFMPEG_DECODER_CONFIG);
		if (!prop || !prop->value.ptr) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[FFDecode] PID %s codec context not exposed by demuxer !\n", gf_filter_pid_get_name(pid) ));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		ffdec->codec_ctx = prop->value.ptr;
		codec = avcodec_find_decoder(ffdec->codec_ctx->codec_id);
		if (!codec) return GF_NOT_SUPPORTED;

		res = avcodec_open2(ffdec->codec_ctx, codec, NULL );
		if (res < 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[FFDecode] PID %s failed to open codec context: %s\n", gf_filter_pid_get_name(pid), av_err2str(res) ));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
	}
	//we reconfigure the stream
	else if (ffdec->codec_ctx) {
		u32 ex_crc=0;
		u32 codec_id = ff_gpac_oti_to_codec_id(oti);

		//TODO: flush decoder to dispatch internally pending frames and create a new decoder
		if (ffdec->codec_ctx->codec->id != codec_id) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[FFDecode] Cannot switch codec type on the fly, not yet supported !\n" ));
			return GF_NOT_SUPPORTED;
		}
		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
		if (prop && prop->value.data && prop->data_len) {
			ex_crc = gf_crc_32(prop->value.data, prop->data_len);
		}
		if (ex_crc != ffdec->extra_data_crc) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[FFDecode] Cannot switch codec config on the fly, not yet supported !\n" ));
			return GF_NOT_SUPPORTED;
		}
	} else {
		AVCodec *codec=NULL;
		u32 codec_id = ff_gpac_oti_to_codec_id(oti);
		if (codec_id) codec = avcodec_find_decoder(codec_id);
		if (!codec) return GF_NOT_SUPPORTED;


		ffdec->codec_ctx = avcodec_alloc_context3(NULL);
		if (! ffdec->codec_ctx) return GF_OUT_OF_MEM;
		ffdec->owns_context = GF_TRUE;

		//we may have a dsi here!
		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
		if (prop && prop->value.data && prop->data_len) {
			ffdec->codec_ctx->extradata_size = prop->data_len;
			ffdec->codec_ctx->extradata = gf_malloc(sizeof(char) * (FF_INPUT_BUFFER_PADDING_SIZE + prop->data_len));
			memcpy(ffdec->codec_ctx->extradata, prop->value.data, prop->data_len);
			memset(ffdec->codec_ctx->extradata + sizeof(char) * prop->data_len, 0, sizeof(char) * FF_INPUT_BUFFER_PADDING_SIZE);

			ffdec->extra_data_crc = gf_crc_32(prop->value.data, prop->data_len);
		}

		res = avcodec_open2(ffdec->codec_ctx, codec, NULL );
		if (res < 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[FFDecode] PID %s failed to open codec context: %s\n", gf_filter_pid_get_name(pid), av_err2str(res) ));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
	}

	//we're good to go, declare our output pid
	ffdec->in_pid = pid;
	if (!ffdec->out_pid) {
		char szCodecName[1000];
		ffdec->out_pid = gf_filter_pid_new(filter);

		//to change once we implement on-the-fly codec change
		sprintf(szCodecName, "ffdec:%s", ffdec->codec_ctx->codec->name ? ffdec->codec_ctx->codec->name : "unknown");
		gf_filter_set_name(filter, szCodecName);
		gf_filter_pid_set_framing_mode(ffdec->in_pid, GF_TRUE);
	}
	//copy props it at init config or at reconfig
	if (ffdec->out_pid) {
		gf_filter_pid_reset_properties(ffdec->out_pid);
		gf_filter_pid_copy_properties(ffdec->out_pid, ffdec->in_pid);
		gf_filter_pid_set_property(ffdec->out_pid, GF_PROP_PID_OTI, &PROP_UINT(GPAC_OTI_RAW_MEDIA_STREAM) );

		gf_filter_pid_set_name(ffdec->out_pid, gf_filter_pid_get_name(ffdec->in_pid) );
	}

	if (type==GF_STREAM_VISUAL) {
		u32 pix_fmt;
		ffdec->process = ffdec_process_video;

		if (ffdec->codec_ctx->pix_fmt>=0) {
			pix_fmt = ffdec_gpac_convert_pix_fmt(ffdec->codec_ctx->pix_fmt);
			if (pix_fmt) {
				FF_CHECK_PROP_VAL(pixel_fmt, pix_fmt, GF_PROP_PID_PIXFMT)
			}
		}
		if (ffdec->codec_ctx->width) {
			FF_CHECK_PROP(width, width, GF_PROP_PID_WIDTH)
		}
		if (ffdec->codec_ctx->height) {
			FF_CHECK_PROP(height, height, GF_PROP_PID_HEIGHT)
		}
		if (ffdec->codec_ctx->sample_aspect_ratio.num && ffdec->codec_ctx->sample_aspect_ratio.den) {
			ffdec->sar.num = ffdec->codec_ctx->sample_aspect_ratio.num;
			ffdec->sar.den = ffdec->codec_ctx->sample_aspect_ratio.den;
			gf_filter_pid_set_property(ffdec->out_pid, GF_PROP_PID_SAR, &PROP_FRAC( ffdec->sar.num, ffdec->sar.den ) );
		}
		if (!ffdec->frame)
			ffdec->frame = av_frame_alloc();

	} else if (type==GF_STREAM_AUDIO) {
		ffdec->process = ffdec_process_audio;
		//for now we convert everything to s16, to be updated later on
		ffdec->sample_fmt = GF_AUDIO_FMT_S16;
		gf_filter_pid_set_property(ffdec->out_pid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(GF_AUDIO_FMT_S16) );

		//override PID props with what decoder gives us
		if (ffdec->codec_ctx->channels) {
			FF_CHECK_PROP(channels, channels, GF_PROP_PID_NUM_CHANNELS)
		}
		//TODO - we might want to align with FFMPEG for the layout of our channels, only the first channels are for now
		if (ffdec->codec_ctx->channel_layout) {
			FF_CHECK_PROP(channel_layout, channel_layout, GF_PROP_PID_CHANNEL_LAYOUT)
		}
		if (ffdec->codec_ctx->sample_rate) {
			FF_CHECK_PROP(sample_rate, sample_rate, GF_PROP_PID_SAMPLE_RATE)
		}
		if (!ffdec->frame)
			ffdec->frame = av_frame_alloc();
	} else {
#ifdef FFDEC_SUB_SUPPORT
		ffdec->process = ffdec_process_subtitle;
#endif
	}
	return GF_OK;
}


static GF_Err ffdec_update_arg(GF_Filter *filter, const char *arg_name, const GF_PropertyValue *arg_val)
{
	s32 res;
	GF_FFDecodeCtx *ffd = gf_filter_get_udta(filter);

	//initial parsing of arguments
	if (!ffd->initialized) {
		switch (arg_val->type) {
		case GF_PROP_STRING:
			res = av_dict_set(&ffd->options, arg_name, arg_val->value.string, 0);
			if (res<0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[FFDecode] Failed to set option %s:%s\n", arg_name, arg_val ));
			}
			break;
		default:
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[FFDecode] Failed to set option %s:%s, unrecognized type %d\n", arg_name, arg_val, arg_val->type ));
			return GF_NOT_SUPPORTED;
		}
		return GF_OK;
	}
	//updates of arguments, not supported for ffmpeg decoders
	return GF_NOT_SUPPORTED;
}

static const GF_FilterCapability FFDecodeInputs[] =
{
	{.code=GF_PROP_PID_STREAM_TYPE, PROP_UINT(GF_STREAM_VISUAL)},
	{.code=GF_PROP_PID_OTI, PROP_UINT(GPAC_OTI_RAW_MEDIA_STREAM), .exclude=GF_TRUE},
	
	{.code=GF_PROP_PID_STREAM_TYPE, PROP_UINT(GF_STREAM_AUDIO), .start=GF_TRUE},
	{.code=GF_PROP_PID_OTI, PROP_UINT(GPAC_OTI_RAW_MEDIA_STREAM), .exclude=GF_TRUE},

#ifdef FFDEC_SUB_SUPPORT
	{.code=GF_PROP_PID_STREAM_TYPE, PROP_UINT(GF_STREAM_TEXT), .start=GF_TRUE},
	{.code=GF_PROP_PID_OTI, PROP_UINT(GPAC_OTI_RAW_MEDIA_STREAM), .exclude=GF_TRUE},
#endif

	{}
};

static const GF_FilterCapability FFDecodeOutputs[] =
{
	{.code= GF_PROP_PID_STREAM_TYPE, PROP_UINT(GF_STREAM_AUDIO)},
	{.code= GF_PROP_PID_OTI, PROP_UINT( GPAC_OTI_RAW_MEDIA_STREAM )},
	
	{.code= GF_PROP_PID_STREAM_TYPE, PROP_UINT(GF_STREAM_VISUAL), .start=GF_TRUE},
	{.code= GF_PROP_PID_OTI, PROP_UINT( GPAC_OTI_RAW_MEDIA_STREAM )},

	{}
};

GF_FilterRegister FFDecodeRegister = {
	.name = "ffdec",
	.description = "FFMPEG decoder "LIBAVCODEC_IDENT,
	.private_size = sizeof(GF_FFDecodeCtx),
	.input_caps = FFDecodeInputs,
	.output_caps = FFDecodeOutputs,
	.initialize = ffdec_initialize,
	.finalize = ffdec_finalize,
	.configure_pid = ffdec_config_input,
	.process = ffdec_process,
	.update_arg = ffdec_update_arg
};


static const GF_FilterArgs FFDecodeArgs[] =
{
	{ "*", -1, "Any possible args defined for AVCodecContext and sub-classes", GF_PROP_UINT, NULL, NULL, GF_FALSE, GF_TRUE},
	{}
};

void ffmpeg_initialize();
GF_FilterArgs ffmpeg_arg_translate(const struct AVOption *opt);
void ffmpeg_expand_registry(GF_FilterSession *session, GF_FilterRegister *orig_reg, u32 type);
void ffmpeg_registry_free(GF_FilterSession *session, GF_FilterRegister *reg, u32 nb_skip_begin);

void ffdec_regfree(GF_FilterSession *session, GF_FilterRegister *reg)
{
	ffmpeg_registry_free(session, reg, 0);
}

const GF_FilterRegister *ffdec_register(GF_FilterSession *session)
{
	GF_FilterArgs *args;
	u32 i=0, idx=0;
	Bool load_meta_filters = session ? GF_TRUE : GF_FALSE;
	AVCodecContext *ctx;
	const struct AVOption *opt;

	ffmpeg_initialize();

	//by default no need to load option descriptions, everything is handled by av_set_opt in update_args
	if (!load_meta_filters) {
		FFDecodeRegister.args = FFDecodeArgs;
		return &FFDecodeRegister;
	}

	FFDecodeRegister.registry_free = ffdec_regfree;
	ctx = avcodec_alloc_context3(NULL);

	idx=0;
	while (ctx->av_class->option) {
		opt = &ctx->av_class->option[idx];
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
	while (ctx->av_class->option) {
		opt = &ctx->av_class->option[idx];
		if (!opt || !opt->name) break;
		if (opt->flags & AV_OPT_FLAG_DECODING_PARAM) {
			args[i] = ffmpeg_arg_translate(opt);
			i++;
		}
		idx++;
	}
	args[i+1] = (GF_FilterArgs) { "*", -1, "Options depend on codec type, check individual filter syntax", GF_PROP_STRING, NULL, NULL, GF_FALSE};

	avcodec_free_context(&ctx);

	ffmpeg_expand_registry(session, &FFDecodeRegister, 1);

	return &FFDecodeRegister;
}



