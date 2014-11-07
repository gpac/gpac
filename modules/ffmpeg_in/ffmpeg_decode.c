/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / FFMPEG module
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

#include "ffmpeg_in.h"
#include <gpac/avparse.h>
#include <libavcodec/avcodec.h>


#if ((LIBAVCODEC_VERSION_MAJOR == 55) && (LIBAVCODEC_VERSION_MINOR >= 38)) || (LIBAVCODEC_VERSION_MAJOR > 55)
#define HAS_HEVC

#include <libavutil/opt.h>
#endif

#if defined(GPAC_ANDROID) || defined(GPAC_IPHONE)
#define NO_10bit
#endif


/**
 * Allocates data for FFMPEG decoding
 * \param oldBuffer The oldBuffer (freed if not NULL)
 * \param size Size to allocate (will use extra padding for real size)
 * \return The newly allocated buffer
 */
static uint8_t * ffmpeg_realloc_buffer(uint8_t * oldBuffer, u32 size) {
	uint8_t * buffer;
	/* Size of buffer must be larger, see avcodec_decode_video2 documentation */
	u32 allocatedSz = sizeof( char ) * (FF_INPUT_BUFFER_PADDING_SIZE + size);
	if (oldBuffer)
		gf_free(oldBuffer);
	buffer = gf_malloc( allocatedSz );
	if (buffer)
		memset( buffer, 0, allocatedSz);
	return buffer;
}


static AVCodec *ffmpeg_get_codec(u32 codec_4cc)
{
	char name[5];
	AVCodec *codec;
	strcpy(name, gf_4cc_to_str(codec_4cc));

	codec = avcodec_find_decoder_by_name(name);
	if (!codec) {
		strupr(name);
		codec = avcodec_find_decoder_by_name(name);
		if (!codec) {
			strlwr(name);
			codec = avcodec_find_decoder_by_name(name);
		}
	}
	/*custom mapings*/
	if (!codec) {
		if (!stricmp(name, "s263")) codec = avcodec_find_decoder_by_name("h263");
		else if (!stricmp(name, "samr") || !stricmp(name, "amr ")) codec = avcodec_find_decoder_by_name("amr_nb");
		else if (!stricmp(name, "sawb")) codec = avcodec_find_decoder_by_name("amr_wb");
	}
	return codec;
}


static void FFDEC_LoadDSI(FFDec *ffd, GF_BitStream *bs, AVCodec *codec, AVCodecContext *ctx, Bool from_ff_demux)
{
	u32 dsi_size;
	if (!codec) return;

	dsi_size = (u32) gf_bs_available(bs);
	if (!dsi_size) return;

	/*demuxer is ffmpeg, extra data can be copied directly*/
	if (from_ff_demux) {
		if (ctx->extradata)
			gf_free(ctx->extradata);
		ctx->extradata_size = dsi_size;
		ctx->extradata = ffmpeg_realloc_buffer(ctx->extradata, ctx->extradata_size);
		gf_bs_read_data(bs, (char *) ctx->extradata, ctx->extradata_size);
		return;
	}

	switch (codec->id) {
	case CODEC_ID_SVQ3:
	{
		u32 at_type, size;
		size = gf_bs_read_u32(bs);
		/*there should be an 'SMI' entry*/
		at_type = gf_bs_read_u32(bs);
		if (at_type == GF_4CC('S', 'M', 'I', ' ')) {
			if (ctx->extradata)
				gf_free(ctx->extradata);
			ctx->extradata_size = 0x5a + size;
			ctx->extradata = ffmpeg_realloc_buffer(ctx->extradata, ctx->extradata_size);
			strcpy((char *) ctx->extradata, "SVQ3");
			gf_bs_read_data(bs, (char *)ctx->extradata + 0x5a, size);
		}
	}
	break;
	default:
		if (ctx->extradata)
			gf_free(ctx->extradata);
		ctx->extradata_size = dsi_size;
		ctx->extradata = ffmpeg_realloc_buffer(ctx->extradata, ctx->extradata_size);
		gf_bs_read_data(bs, (char *)ctx->extradata, ctx->extradata_size);
		break;
	}
}

static GF_Err FFDEC_AttachStream(GF_BaseDecoder *plug, GF_ESD *esd)
{
	u32 codec_id = 0;
	int gotpic;
	GF_BitStream *bs;
	AVCodecContext **ctx;
	AVCodec **codec;
	AVFrame **frame;
	const char *sOpt;

#ifndef GPAC_DISABLE_AV_PARSERS
	GF_M4VDecSpecInfo dsi;
	GF_Err e;
#endif
	FFDec *ffd = (FFDec *)plug->privateStack;
	if (esd->decoderConfig->upstream) return GF_NOT_SUPPORTED;
	if (!ffd->oti) return GF_NOT_SUPPORTED;



	/*locate any auxiliary video data descriptor on this stream*/
	if (esd->dependsOnESID) {
		u32 i = 0;
		GF_Descriptor *d = NULL;
		if (esd->dependsOnESID != ffd->base_ES_ID) return GF_NOT_SUPPORTED;
		while ((d = gf_list_enum(esd->extensionDescriptors, &i))) {
			if (d->tag == GF_ODF_AUX_VIDEO_DATA) break;
		}
		if (!d) return GF_NOT_SUPPORTED;

		ffd->depth_ES_ID = esd->ESID;
		ctx = &ffd->depth_ctx;
		codec = &ffd->depth_codec;
		frame = &ffd->depth_frame;
	} else {
		if (ffd->base_ES_ID) return GF_NOT_SUPPORTED;
		ffd->base_ES_ID = esd->ESID;
		ctx = &ffd->base_ctx;
		codec = &ffd->base_codec;
		frame = &ffd->base_frame;
	}
	if (!(*ctx)) {

#ifdef USE_AVCTX3
		*ctx = avcodec_alloc_context3(NULL);
#else
		*ctx = avcodec_alloc_context();
#endif
	}

	/*private FFMPEG DSI*/
	if (ffd->oti == GPAC_OTI_MEDIA_FFMPEG) {
		bs = gf_bs_new(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, GF_BITSTREAM_READ);
		codec_id = gf_bs_read_u32(bs);
		if (ffd->st==GF_STREAM_AUDIO) {
			(*ctx)->codec_type = AVMEDIA_TYPE_AUDIO;
			(*ctx)->sample_rate = gf_bs_read_u32(bs);
			(*ctx)->channels = gf_bs_read_u16(bs);
			(*ctx)->frame_size = gf_bs_read_u16(bs);
			/*bits_per_sample */gf_bs_read_u8(bs);
			/*num_frames_per_au*/ gf_bs_read_u8(bs);

			/*ffmpeg specific*/
			(*ctx)->block_align = gf_bs_read_u16(bs);
			(*ctx)->bit_rate = gf_bs_read_u32(bs);
			(*ctx)->codec_tag = gf_bs_read_u32(bs);
		} else if (ffd->st==GF_STREAM_VISUAL) {
			(*ctx)->codec_type = AVMEDIA_TYPE_VIDEO;
			(*ctx)->width = gf_bs_read_u16(bs);
			(*ctx)->height = gf_bs_read_u16(bs);
			(*ctx)->bit_rate = gf_bs_read_u32(bs);
			(*ctx)->codec_tag = gf_bs_read_u32(bs);
			ffd->raw_pix_fmt = gf_bs_read_u32(bs);
		}

		*codec = avcodec_find_decoder(codec_id);
		FFDEC_LoadDSI(ffd, bs, *codec, *ctx, 1);
		gf_bs_del(bs);
	}
	/*private QT DSI*/
	else if (ffd->oti == GPAC_OTI_MEDIA_GENERIC) {
		bs = gf_bs_new(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, GF_BITSTREAM_READ);
		codec_id = gf_bs_read_u32(bs);
		if (ffd->st==GF_STREAM_AUDIO) {
			(*ctx)->codec_type = AVMEDIA_TYPE_AUDIO;
			(*ctx)->sample_rate = gf_bs_read_u32(bs);
			(*ctx)->channels = gf_bs_read_u16(bs);
			(*ctx)->frame_size = gf_bs_read_u16(bs);
			/*bits_per_sample */ gf_bs_read_u8(bs);
			/*num_frames_per_au*/ gf_bs_read_u8(bs);
			/*just in case...*/
			if (codec_id == GF_4CC('a', 'm', 'r', ' ')) {
				(*ctx)->sample_rate = 8000;
				(*ctx)->channels = 1;
				(*ctx)->frame_size = 160;
			}
		} else if (ffd->st==GF_STREAM_VISUAL) {
			(*ctx)->codec_type = AVMEDIA_TYPE_VIDEO;
			(*ctx)->width = gf_bs_read_u16(bs);
			(*ctx)->height = gf_bs_read_u16(bs);
		}
		(*codec) = ffmpeg_get_codec(codec_id);
		FFDEC_LoadDSI(ffd, bs, *codec, *ctx, 0);
		gf_bs_del(bs);
	}
	/*use std MPEG-4 st/oti*/
	else {
		u32 codec_id = 0;
		if (ffd->st==GF_STREAM_VISUAL) {
			(*ctx)->codec_type = AVMEDIA_TYPE_VIDEO;
			switch (ffd->oti) {
			case GPAC_OTI_VIDEO_MPEG4_PART2:
				codec_id = CODEC_ID_MPEG4;
				break;
			case GPAC_OTI_VIDEO_AVC:
				codec_id = CODEC_ID_H264;
				break;
#ifdef HAS_HEVC
			case GPAC_OTI_VIDEO_HEVC:
				codec_id = AV_CODEC_ID_HEVC;
				break;
#endif
			case GPAC_OTI_VIDEO_MPEG1:
			case GPAC_OTI_VIDEO_MPEG2_SIMPLE:
			case GPAC_OTI_VIDEO_MPEG2_MAIN:
			case GPAC_OTI_VIDEO_MPEG2_SNR:
			case GPAC_OTI_VIDEO_MPEG2_SPATIAL:
			case GPAC_OTI_VIDEO_MPEG2_HIGH:
			case GPAC_OTI_VIDEO_MPEG2_422:
				codec_id = CODEC_ID_MPEG2VIDEO;
				break;
			case GPAC_OTI_IMAGE_JPEG:
				codec_id = CODEC_ID_MJPEG;
				ffd->is_image=1;
				break;
			case 0xFF:
				codec_id = CODEC_ID_SVQ3;
				break;
			}
		} else if (ffd->st==GF_STREAM_AUDIO) {
			(*ctx)->codec_type = AVMEDIA_TYPE_AUDIO;
			switch (ffd->oti) {
			case GPAC_OTI_AUDIO_MPEG2_PART3:
			case GPAC_OTI_AUDIO_MPEG1:
				(*ctx)->frame_size = 1152;
				codec_id = CODEC_ID_MP2;
				break;
			case GPAC_OTI_AUDIO_AC3:
				codec_id = CODEC_ID_AC3;
				break;
			case GPAC_OTI_AUDIO_EAC3:
				codec_id = CODEC_ID_EAC3;
				break;
			}
		}
		else if ((ffd->st==GF_STREAM_ND_SUBPIC) && (ffd->oti==0xe0)) {
			codec_id = CODEC_ID_DVD_SUBTITLE;
		}
		*codec = avcodec_find_decoder(codec_id);
	}
	/*should never happen*/
	if (! (*codec)) return GF_OUT_OF_MEM;

	/*setup MPEG-4 video streams*/
	if (ffd->st==GF_STREAM_VISUAL) {
		/*for all MPEG-4 variants get size*/
		if ((ffd->oti==GPAC_OTI_VIDEO_MPEG4_PART2) || (ffd->oti == GPAC_OTI_VIDEO_AVC) || (ffd->oti == GPAC_OTI_VIDEO_HEVC)) {
			/*if not set this may be a remap of non-mpeg4 transport (eg, transport on MPEG-TS) where
			the DSI is carried in-band*/
			if (esd->decoderConfig->decoderSpecificInfo && esd->decoderConfig->decoderSpecificInfo->data) {

				/*for regular MPEG-4, try to decode and if this fails try H263 decoder at first frame*/
				if (ffd->oti==GPAC_OTI_VIDEO_MPEG4_PART2) {
#ifndef GPAC_DISABLE_AV_PARSERS
					e = gf_m4v_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &dsi);
					if (e) return e;
					if (dsi.width%2) dsi.width++;
					if (dsi.height%2) dsi.height++;
					(*ctx)->width = dsi.width;
					(*ctx)->height = dsi.height;
					if (!dsi.width && !dsi.height) ffd->check_short_header = 1;
					ffd->previous_par = (dsi.par_num<<16) | dsi.par_den;
					ffd->no_par_update = 1;
#endif
				} else if (ffd->oti==GPAC_OTI_VIDEO_AVC) {
					ffd->check_h264_isma = 1;
				}

				/*setup dsi for FFMPEG context BEFORE attaching decoder (otherwise not proper init)*/
				(*ctx)->extradata = ffmpeg_realloc_buffer((*ctx)->extradata, esd->decoderConfig->decoderSpecificInfo->dataLength + 8);
				if ((*ctx)->extradata) {
					memcpy((*ctx)->extradata, esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength);
					(*ctx)->extradata_size = esd->decoderConfig->decoderSpecificInfo->dataLength;
				} else {
					/* out of mem ? */
					(*ctx)->extradata_size = 0;
				}
			}
		}
#if !defined(FF_API_AVFRAME_LAVC) || defined (GPAC_IPHONE)
		*frame = avcodec_alloc_frame();
#else
		*frame = av_frame_alloc();
#endif

	}
#ifdef HAS_HEVC
	if (ffd->oti == GPAC_OTI_VIDEO_HEVC) {
		GF_SystemRTInfo rti;
		u32 nb_threads, detected_nb_threads = 1;
		sOpt = gf_modules_get_option((GF_BaseInterface *)plug, "OpenHEVC", "ThreadingType");
		if (sOpt && !strcmp(sOpt, "wpp")) av_opt_set(*ctx, "thread_type", "slice", 0);
		else if (sOpt && !strcmp(sOpt, "frame+wpp")) av_opt_set(*ctx, "thread_type", "frameslice", 0);
		else {
			av_opt_set(*ctx, "thread_type", "frame", 0);
			if (!sOpt) gf_modules_set_option((GF_BaseInterface *)plug, "OpenHEVC", "ThreadingType", "frame");
		}
		if (gf_sys_get_rti(0, &rti, 0) ) {
			detected_nb_threads = (rti.nb_cores>1) ? rti.nb_cores-1 : 1;
		}
		sOpt = gf_modules_get_option((GF_BaseInterface *)plug, "OpenHEVC", "NumThreads");
		if (!sOpt) {
			char szO[100];
			sprintf(szO, "%d", detected_nb_threads);
			gf_modules_set_option((GF_BaseInterface *)plug, "OpenHEVC", "NumThreads", szO);
			nb_threads = detected_nb_threads;
		} else {
			nb_threads = atoi(sOpt);
		}
		if (nb_threads > detected_nb_threads) {
			GF_LOG(GF_LOG_CODEC, GF_LOG_WARNING, ("[HEVC@ffmpeg] Initializing with %d threads but only %d available cores detected on the system\n", nb_threads, rti.nb_cores));
		} else {
			GF_LOG(GF_LOG_CODEC, GF_LOG_INFO, ("[HEVC@ffmpeg] Initializing with %d threads\n", nb_threads));
		}
		fprintf(stderr, "[HEVC@ffmpeg] Initializing with %d threads\n", nb_threads);
		av_opt_set_int(*ctx, "threads", nb_threads, 0);

		/* Set the decoder id */
		//av_opt_set_int(openHevcContext->c->priv_data, "decoder-id", i, 0);

		sOpt = gf_modules_get_option((GF_BaseInterface *)plug, "OpenHEVC", "CBUnits");
		if (!sOpt) gf_modules_set_option((GF_BaseInterface *)plug, "OpenHEVC", "CBUnits", "4");
		if (sOpt) ffd->output_cb_size = atoi(sOpt);
	}
#endif //HAS_HEVC
	if (!ffd->output_cb_size) ffd->output_cb_size = 4;

	if (codec_id == CODEC_ID_RAWVIDEO) {
		(*ctx)->codec_id = CODEC_ID_RAWVIDEO;
		(*ctx)->pix_fmt = ffd->raw_pix_fmt;
		if ((*ctx)->extradata && strstr((char *) (*ctx)->extradata, "BottomUp")) ffd->flipped = 1;
	} else {
#ifdef USE_AVCTX3
		if (avcodec_open2((*ctx), (*codec), NULL )<0) return GF_NON_COMPLIANT_BITSTREAM;
#else
		if (avcodec_open((*ctx), (*codec) )<0) return GF_NON_COMPLIANT_BITSTREAM;
#endif
	}
	/*setup audio streams*/
	if (ffd->st==GF_STREAM_AUDIO) {
		if ((*codec)->id == CODEC_ID_MP2) {
			(*ctx)->frame_size = ((*ctx)->sample_rate > 24000) ? 1152 : 576;
		}
		/*may be 0 (cfg not known yet)*/
		ffd->out_size = (*ctx)->channels * (*ctx)->frame_size * 2 /*16 / 8*/;
		if (!(*ctx)->sample_rate) (*ctx)->sample_rate = 44100;
		if (!(*ctx)->channels) (*ctx)->channels = 2;

#if defined(USE_AVCTX3)

#if !defined(FF_API_AVFRAME_LAVC) || defined (GPAC_IPHONE)
		ffd->audio_frame = avcodec_alloc_frame();
#else
		ffd->audio_frame = av_frame_alloc();
#endif

#endif

	} else {
		switch ((*codec)->id) {
		case CODEC_ID_MJPEG:
		case CODEC_ID_MJPEGB:
		case CODEC_ID_LJPEG:
#if (LIBAVCODEC_VERSION_INT > AV_VERSION_INT(51, 20, 0))
		case CODEC_ID_GIF:
#endif
		case CODEC_ID_RAWVIDEO:
			ffd->pix_fmt = GF_PIXEL_RGB_24;
			break;

		case CODEC_ID_DVD_SUBTITLE:
#if !defined(FF_API_AVFRAME_LAVC) || defined (GPAC_IPHONE)
			*frame = avcodec_alloc_frame();
#else
			*frame = av_frame_alloc();
#endif

#ifdef USE_AVCODEC2
			{
				AVPacket pkt;
				av_init_packet(&pkt);
				pkt.data = (uint8_t *) esd->decoderConfig->decoderSpecificInfo->data;
				pkt.size = esd->decoderConfig->decoderSpecificInfo->dataLength;
				avcodec_decode_video2((*ctx), *frame, &gotpic, &pkt);
			}
#else
			avcodec_decode_video((*ctx), *frame, &gotpic, esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength);
#endif
			ffd->pix_fmt = GF_PIXEL_YV12;
			break;
		default:
			ffd->pix_fmt = GF_PIXEL_YV12;
			break;
		}

		ffd->out_pix_fmt = ffd->pix_fmt;

		if (ffd->out_pix_fmt == GF_PIXEL_YV12) {
			ffd->stride = (*ctx)->width;
			if (ffd->depth_codec) {
				ffd->out_size = (*ctx)->width * (*ctx)->height * 5 / 2;
				ffd->out_pix_fmt = GF_PIXEL_YUVD;
				ffd->yuv_size = (*ctx)->width * (*ctx)->height * 3 / 2;
			} else {
				ffd->out_size = (*ctx)->width * (*ctx)->height * 3 / 2;
			}
		} else {
			ffd->out_size = (*ctx)->width * (*ctx)->height * 3;
		}
	}


	sOpt = gf_modules_get_option((GF_BaseInterface *)plug, "Systems", "Output8bit");
	if (!sOpt) gf_modules_set_option((GF_BaseInterface *)plug, "Systems", "Output8bit", (ffd->display_bpp>8) ? "no" : "yes");
	if (sOpt && !strcmp(sOpt, "yes")) ffd->output_as_8bit = 1;

	if (ffd->output_as_8bit && (ffd->stride > (u32) (*ctx)->width)) {
		ffd->stride /=2;
		ffd->out_size /= 2;
		ffd->conv_to_8bit = 1;
	}

	return GF_OK;
}

static GF_Err FFDEC_DetachStream(GF_BaseDecoder *plug, u16 ES_ID)
{
	AVCodecContext **ctx;
	AVCodec **codec;
#ifdef FFMPEG_SWSCALE
	struct SwsContext **sws;
#endif
	FFDec *ffd = (FFDec *)plug->privateStack;

	if (ffd->base_ES_ID==ES_ID) {
		ffd->base_ES_ID = 0;
		codec = &ffd->base_codec;
		ctx = &ffd->base_ctx;
#ifdef FFMPEG_SWSCALE
		sws = &ffd->base_sws;
#endif
	} else if (ffd->depth_ES_ID==ES_ID) {
		ffd->depth_ES_ID = 0;
		codec = &ffd->depth_codec;
		ctx = &ffd->depth_ctx;
#ifdef FFMPEG_SWSCALE
		sws = &ffd->depth_sws;
#endif
	} else {
		return GF_OK;
	}

	if (*ctx) {
		if ((*ctx)->extradata) gf_free((*ctx)->extradata);
		(*ctx)->extradata = NULL;
		if ((*ctx)->codec) avcodec_close((*ctx));
		*ctx = NULL;
	}
	*codec = NULL;

#if defined(USE_AVCTX3)
	if (ffd->audio_frame) {
		av_free(ffd->audio_frame);
	}
#endif

#ifdef FFMPEG_SWSCALE
	if (*sws) {
		sws_freeContext(*sws);
		*sws = NULL;
	}
#endif
	if (ffd->conv_buffer) {
		gf_free(ffd->conv_buffer);
		ffd->conv_buffer = NULL;
	}
	return GF_OK;
}

static GF_Err FFDEC_GetCapabilities(GF_BaseDecoder *plug, GF_CodecCapability *capability)
{
	FFDec *ffd = (FFDec *)plug->privateStack;

	/*base caps*/
	switch (capability->CapCode) {
	/*ffmpeg seems quite reliable*/
	case GF_CODEC_RESILIENT:
		capability->cap.valueInt = 1;
		return GF_OK;
	case GF_CODEC_PADDING_BYTES:
		capability->cap.valueInt = FF_INPUT_BUFFER_PADDING_SIZE;
		return GF_OK;
	case GF_CODEC_REORDER:
		capability->cap.valueInt = 1;
		return GF_OK;
	case GF_CODEC_DIRECT_OUTPUT:
		capability->cap.valueBool = GF_TRUE;
		return GF_OK;
	case GF_CODEC_WANTS_THREAD:
		capability->cap.valueBool= GF_TRUE;
		break;
	}

	if (!ffd->base_ctx) {
		capability->cap.valueInt = 0;
		return GF_OK;
	}

	/*caps valid only if stream attached*/
	switch (capability->CapCode) {
	case GF_CODEC_OUTPUT_SIZE:
		capability->cap.valueInt = ffd->out_size;
		break;
	case GF_CODEC_SAMPLERATE:
		capability->cap.valueInt = ffd->base_ctx->sample_rate;
		break;
	case GF_CODEC_NB_CHAN:
		capability->cap.valueInt = ffd->base_ctx->channels;
		break;
	case GF_CODEC_BITS_PER_SAMPLE:
		capability->cap.valueInt = 16;
		break;
	case GF_CODEC_BUFFER_MIN:
		capability->cap.valueInt = (ffd->st==GF_STREAM_AUDIO) ? 4 : 1;
		break;
	case GF_CODEC_BUFFER_MAX:
		/*for audio let the systems engine decide since we may have very large block size (1 sec with some QT movies)*/
		capability->cap.valueInt = (ffd->st==GF_STREAM_AUDIO) ? 0 : (ffd->is_image ? 1 : ffd->output_cb_size);
		break;
	/*by default AAC access unit lasts num_samples (timescale being sampleRate)*/
	case GF_CODEC_CU_DURATION:
		capability->cap.valueInt =  (ffd->st==GF_STREAM_AUDIO) ? ffd->base_ctx->frame_size : 0;
		break;
	case GF_CODEC_WIDTH:
		capability->cap.valueInt = ffd->base_ctx->width;
		break;
	case GF_CODEC_HEIGHT:
		capability->cap.valueInt = ffd->base_ctx->height;
		break;
	case GF_CODEC_STRIDE:
		if (ffd->out_pix_fmt==GF_PIXEL_RGB_24)
			capability->cap.valueInt = ffd->stride*3;
		else if (ffd->conv_buffer)
			capability->cap.valueInt = ffd->base_ctx->width;
		else
			capability->cap.valueInt = ffd->stride;
		break;
	case GF_CODEC_FPS:
		capability->cap.valueFloat = 30.0f;
		break;
	case GF_CODEC_PAR:
		capability->cap.valueInt = ffd->previous_par;
		break;
	case GF_CODEC_PIXEL_FORMAT:
		if (ffd->base_ctx->width) capability->cap.valueInt = ffd->out_pix_fmt;
		if (ffd->conv_buffer) capability->cap.valueInt = GF_PIXEL_YV12;
		break;
	/*ffmpeg performs frame reordering internally*/
	case GF_CODEC_REORDER:
		capability->cap.valueInt = 1;
		break;
	case GF_CODEC_WAIT_RAP:
		//ffd->ctx->hurry_up = 5;
		break;
	case GF_CODEC_CHANNEL_CONFIG:
		/*currently unused in ffmpeg*/
		if (ffd->base_ctx->channels==1) {
			capability->cap.valueInt = GF_AUDIO_CH_FRONT_CENTER;
		} else {
			capability->cap.valueInt = GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT;
		}
		break;

	case GF_CODEC_PADDING_BYTES:
		capability->cap.valueInt = FF_INPUT_BUFFER_PADDING_SIZE;
		break;
	default:
		capability->cap.valueInt = 0;
		break;
	}
	return GF_OK;
}

static GF_Err FFDEC_SetCapabilities(GF_BaseDecoder *plug, GF_CodecCapability capability)
{
	FFDec *ffd = (FFDec *)plug->privateStack;
	assert(plug);
	assert( ffd );
	switch (capability.CapCode) {
	case GF_CODEC_DISPLAY_BPP:
		ffd->display_bpp = capability.cap.valueInt;
		return GF_OK;
	case GF_CODEC_WAIT_RAP:
		ffd->frame_start = 0;
		if (ffd->st==GF_STREAM_VISUAL) {
			if (ffd->base_ctx && ffd->base_ctx->codec) avcodec_flush_buffers(ffd->base_ctx);
			if (ffd->depth_ctx && ffd->depth_ctx->codec) avcodec_flush_buffers(ffd->depth_ctx);
		}
		return GF_OK;
	case GF_CODEC_DIRECT_OUTPUT:
		ffd->direct_output = GF_TRUE;
		return GF_OK;
	default:
		/*return unsupported to avoid confusion by the player (like color space changing ...) */
		return GF_NOT_SUPPORTED;
	}
}

static GF_Err FFDEC_ProcessData(GF_MediaDecoder *plug,
                                char *inBuffer, u32 inBufferLength,
                                u16 ES_ID, u32 *CTS,
                                char *outBuffer, u32 *outBufferLength,
                                u8 PaddingBits, u32 mmlevel)
{
#ifdef USE_AVCODEC2
	AVPacket pkt;
#endif
	AVPicture pict;
	u32 pix_out;
	s32 w, h, gotpic, stride;
	u32 outsize;
	AVCodecContext *ctx;
	AVCodec **codec;
	AVFrame *frame;
#ifdef FFMPEG_SWSCALE
	struct SwsContext **cached_sws;
#endif
	FFDec *ffd = plug->privateStack;
#ifdef FFMPEG_SWSCALE
	cached_sws = &(ffd->base_sws);
#endif

	if (!ES_ID || (ffd->base_ES_ID==ES_ID)) {
		ctx = ffd->base_ctx;
		codec = &ffd->base_codec;
		frame = ffd->base_frame;
	} else if (ffd->depth_ES_ID==ES_ID) {
		ctx = ffd->depth_ctx;
		codec = &ffd->depth_codec;
		frame = ffd->depth_frame;
	} else {
		return GF_BAD_PARAM;
	}


	/*WARNING: this breaks H264 (and maybe others) decoding, disabled for now*/
#if 0
	if (!ctx->hurry_up) {
		switch (mmlevel) {
		case GF_CODEC_LEVEL_SEEK:
		case GF_CODEC_LEVEL_DROP:
			/*skip as much as possible*/
			ctx->hurry_up = 5;
			break;
		case GF_CODEC_LEVEL_VERY_LATE:
		case GF_CODEC_LEVEL_LATE:
			/*skip B-frames*/
			ctx->hurry_up = 1;
			break;
		default:
			ctx->hurry_up = 0;
			break;
		}
	}
#endif

#ifdef USE_AVCODEC2
	av_init_packet(&pkt);
	pkt.data = (uint8_t *)inBuffer;
	pkt.size = inBufferLength;
#endif
	/*audio stream*/
	if (ffd->st==GF_STREAM_AUDIO) {
		s32 len;
		u32 buf_size = (*outBufferLength);

		(*outBufferLength) = 0;

		/*seeking don't decode*/
		if (!inBuffer || (mmlevel == GF_CODEC_LEVEL_SEEK)) {
			*outBufferLength = 0;
			ffd->frame_start = 0;
			return GF_OK;
		}
		if (ffd->frame_start>inBufferLength) ffd->frame_start = 0;
		//seek to last byte consumed by the previous decode4()
		else if (ffd->frame_start) {
			pkt.data += ffd->frame_start;
			pkt.size -= ffd->frame_start;
		}
redecode:

#if defined(USE_AVCTX3)
		len = avcodec_decode_audio4(ctx, ffd->audio_frame, &gotpic, &pkt);
		if (gotpic) {
			//int inputDataSize = av_samples_get_buffer_size(NULL, ctx->channels, ffd->audio_frame->nb_samples, ctx->sample_fmt, 1);
			gotpic = ffd->audio_frame->nb_samples * 2 * ctx->channels;
			
		}
#elif defined(USE_AVCODEC2)
		gotpic = 192000;
		len = avcodec_decode_audio3(ctx, (short *)ffd->audio_buf, &gotpic, &pkt);
#else
		gotpic = AVCODEC_MAX_AUDIO_FRAME_SIZE;
		len = avcodec_decode_audio2(ctx, (short *)ffd->audio_buf, &gotpic, inBuffer + ffd->frame_start, inBufferLength - ffd->frame_start);
#endif
		if (len<0) {
			ffd->frame_start = 0;
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		if (gotpic<0) {
			ffd->frame_start = 0;
			return GF_OK;
		}

		/*first config*/
		if (!ffd->out_size) {
			u32 bpp = 2;

			if (ctx->channels * ctx->frame_size * bpp < gotpic) ctx->frame_size = gotpic / (bpp * ctx->channels);

			ffd->out_size = ctx->channels * ctx->frame_size * bpp;
		}
		if (ffd->out_size < (u32) gotpic) {
			/*looks like relying on frame_size is not a good idea for all codecs, so we use gotpic*/
			(*outBufferLength) = ffd->out_size = gotpic;
			return GF_BUFFER_TOO_SMALL;
		}
		if (ffd->out_size > buf_size) {
			/*don't use too small output chunks otherwise we'll never have enough when mixing - we could
			also request more slots in the composition memory but let's not waste mem*/
			if (ffd->out_size < (u32) 576*ctx->channels) ffd->out_size=ctx->channels*576;
			(*outBufferLength) = ffd->out_size;
			return GF_BUFFER_TOO_SMALL;
		}

#if defined(USE_AVCTX3)
		if (ffd->audio_frame->format==AV_SAMPLE_FMT_FLTP) {
			s32 i, j;
			s16 *output = (s16 *) outBuffer;
			for (j=0; j<ctx->channels; j++) {
				Float* inputChannel = (Float*)ffd->audio_frame->extended_data[j];
				for (i=0 ; i<ffd->audio_frame->nb_samples ; i++) {
					Float sample = inputChannel[i];
					if (sample<-1.0f) sample=-1.0f;
					else if (sample>1.0f) sample=1.0f;

					output[i*ctx->channels + j] = (int16_t) (sample * GF_SHORT_MAX );
				}
			}
		} else if (ffd->audio_frame->format==AV_SAMPLE_FMT_U8) {
			u32 i, size = ffd->audio_frame->nb_samples * ctx->channels;
			s16 *output = (s16 *) outBuffer;
			s8 *input = (s8 *) ffd->audio_frame->data;
			for (i=0;i<size;i++) {
				output [i] = input[i] * 128;
			}
		} else if (ffd->audio_frame->format==AV_SAMPLE_FMT_S32) {
			u32 i, shift, size = ffd->audio_frame->nb_samples * ctx->channels;
			s16 *output = (s16 *) outBuffer;
			s32 *input = (s32*) ffd->audio_frame->data;
			shift = 1<<31;
			for (i=0;i<size;i++) {
				output [i] = input[i] * shift;
			}
		} else if (ffd->audio_frame->format==AV_SAMPLE_FMT_S16) {
			memcpy(outBuffer, ffd->audio_frame->data, sizeof(char) * ffd->audio_frame->nb_samples * ctx->channels*2);
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[FFMPEG Decoder] Raw Audio format %d not supoorted\n", ffd->audio_frame->format ));
		}
#else
		/*we're sure to have at least gotpic bytes available in output*/
		memcpy(outBuffer, ffd->audio_buf, sizeof(char) * gotpic);
#endif

		(*outBufferLength) += gotpic;
		outBuffer += gotpic;

#if defined(USE_AVCTX3)
		ffd->audio_frame->nb_samples = 0;
#endif

		ffd->frame_start += len;
		if (inBufferLength <= ffd->frame_start) {
			ffd->frame_start = 0;
			return GF_OK;
		}
		/*still space go on*/
		if ((*outBufferLength)+ctx->frame_size<ffd->out_size) goto redecode;

		/*more frames in the current sample*/
		return GF_PACKED_FRAMES;
	}


	if ( ctx->codec_id == CODEC_ID_RAWVIDEO) {
		if (*outBufferLength != ffd->out_size) {
			*outBufferLength = ffd->out_size;
			return GF_BUFFER_TOO_SMALL;
		}
		if (inBufferLength) {
			*outBufferLength = ffd->out_size;
			assert(inBufferLength==ffd->out_size);

			if (ffd->raw_pix_fmt==PIX_FMT_BGR24) {
				s32 i, j;
				for (j=0; j<ctx->height; j++) {
					u8 *src = (u8 *) inBuffer + j*3*ctx->width;
					u8 *dst = (u8 *)outBuffer + j*3*ctx->width;
					if (ffd->flipped) {
						dst = (u8 *)outBuffer + (ctx->height-j-1) * 3*ctx->width;
					}
					for (i=0; i<ctx->width; i++) {
						dst[0] = src[2];
						dst[1] = src[1];
						dst[2] = src[2];
						src += 3;
						dst += 3;
					}
				}
			} else {
				memcpy(outBuffer, inBuffer, ffd->out_size);
			}
		} else {
			*outBufferLength = 0;
		}
		return GF_OK;
	}

	*outBufferLength = 0;
	/*visual stream*/
	w = ctx->width;
	h = ctx->height;

	/*we have a valid frame not yet dispatched*/
	if (ffd->had_pic) {
		ffd->had_pic = 0;
		gotpic = 1;
	} else {
		if (ffd->check_h264_isma) {
			/*for AVC bitstreams after ISMA decryption, in case (as we do) the decryption DRM tool
			doesn't put back nalu size, do it ourselves...*/
			if (inBuffer && !inBuffer[0] && !inBuffer[1] && !inBuffer[2] && (inBuffer[3]==0x01)) {
				u32 nalu_size;
				char *start, *end, *bufferEnd;

				start = inBuffer;
				end = inBuffer + 4;
				bufferEnd = inBuffer + inBufferLength;
				/* FIXME : SOUCHAY : not sure of exact behaviour, but old one was reading non-allocated memory */
				while ((end+3) < bufferEnd) {
					if (!end[0] && !end[1] && !end[2] && (end[3]==0x01)) {
						nalu_size = (u32) (end - start - 4);
						start[0] = (nalu_size>>24)&0xFF;
						start[1] = (nalu_size>>16)&0xFF;
						start[2] = (nalu_size>>8)&0xFF;
						start[3] = (nalu_size)&0xFF;
						start = end;
						end = start+4;
						continue;
					}
					end++;
				}
				nalu_size = (u32) ((inBuffer+inBufferLength) - start - 4);
				start[0] = (nalu_size>>24)&0xFF;
				start[1] = (nalu_size>>16)&0xFF;
				start[2] = (nalu_size>>8)&0xFF;
				start[3] = (nalu_size)&0xFF;
				ffd->check_h264_isma = 2;
			}
			/*if we had ISMA E&A and lost it this is likely due to a pck loss - do NOT switch back to regular*/
			else if (ffd->check_h264_isma == 1) {
				ffd->check_h264_isma = GF_FALSE;
			}
		}

#ifdef USE_AVCODEC2
		if (avcodec_decode_video2(ctx, frame, &gotpic, &pkt) < 0) {
#else
		if (avcodec_decode_video(ctx, frame, &gotpic, inBuffer, inBufferLength) < 0) {
#endif
			if (!ffd->check_short_header) {
				return GF_NON_COMPLIANT_BITSTREAM;
			}

			/*switch to H263 (ffmpeg MPEG-4 codec doesn't understand short headers)*/
			{
				u32 old_codec = (*codec)->id;
				ffd->check_short_header = GF_FALSE;
				/*OK we loose the DSI stored in the codec context, but H263 doesn't need any, and if we're
				here this means the DSI was broken, so no big deal*/
				avcodec_close(ctx);
				*codec = avcodec_find_decoder(CODEC_ID_H263);

#ifdef USE_AVCTX3
				if (! (*codec) || (avcodec_open2(ctx, *codec, NULL)<0)) return GF_NON_COMPLIANT_BITSTREAM;
#else
				if (! (*codec) || (avcodec_open(ctx, *codec)<0)) return GF_NON_COMPLIANT_BITSTREAM;
#endif

#if USE_AVCODEC2
				if (avcodec_decode_video2(ctx, frame, &gotpic, &pkt) < 0) {
#else
				if (avcodec_decode_video(ctx, frame, &gotpic, inBuffer, inBufferLength) < 0) {
#endif
					/*nope, stay in MPEG-4*/
					avcodec_close(ctx);
					*codec = avcodec_find_decoder(old_codec);
					assert(*codec);
#ifdef USE_AVCTX3
					avcodec_open2(ctx, *codec, NULL);
#else
					avcodec_open(ctx, *codec);
#endif
					return GF_NON_COMPLIANT_BITSTREAM;
				}
			}
		}

		/*
			if (!gotpic && (!ctx->width || !ctx->height) ) {
				ctx->width = w;
				ctx->height = h;
				return GF_OK;
			}
		*/
		/*some streams use odd width/height frame values*/
		if (ffd->out_pix_fmt == GF_PIXEL_YV12) {
			if (ctx->width%2) ctx->width++;
			if (ctx->height%2) ctx->height++;
		}
	}

	/*we have a picture and need resize, do it*/
	if (gotpic && ffd->needs_output_resize) {
		ffd->needs_output_resize=0;
		ffd->had_pic = 1;
		*outBufferLength = ffd->out_size;
		return GF_BUFFER_TOO_SMALL;
	}

	stride = frame->linesize[0];
#ifndef NO_10bit
	if ((ctx->pix_fmt == PIX_FMT_YUV420P10LE) && ffd->output_as_8bit && (frame->linesize[0] >= 2*w) )  {
		ffd->conv_to_8bit = 1;
		stride=w;
	}
#endif

	/*recompute outsize in case on-the-fly change*/
	if ((w != ctx->width) || (h != ctx->height)
	        || (ffd->direct_output && (stride != ffd->stride))
	        || ((ffd->out_pix_fmt==GF_PIXEL_YV12) && (ctx->pix_fmt != PIX_FMT_YUV420P) && !ffd->output_as_8bit )
	        //need to realloc the conversion buffer
	        || (ffd->conv_to_8bit && !ffd->conv_buffer && ffd->direct_output)
	   ) {

		ffd->stride = (!ffd->conv_to_8bit && ffd->direct_output) ? frame->linesize[0] : ctx->width;
		if (ffd->out_pix_fmt == GF_PIXEL_RGB_24) {
			outsize = ctx->width * ctx->height * 3;
		}
#ifndef NO_10bit
		//this YUV format is handled natively in GPAC
		else if ((ctx->pix_fmt == PIX_FMT_YUV420P10LE) && !ffd->output_as_8bit) {
			ffd->stride = ffd->direct_output ? frame->linesize[0] : ctx->width*2;
			outsize = ffd->stride * ctx->height * 3 / 2;
			ffd->out_pix_fmt = GF_PIXEL_YV12_10;
		}
#endif
		//the rest will be YUV
		else {
			outsize = ffd->stride * ctx->height * 3 / 2;
			ffd->out_pix_fmt = GF_PIXEL_YV12;
		}

		if (ffd->depth_codec) {
			outsize = 5 * ctx->width * ctx->height / 2;
			ffd->yuv_size = 3 * ctx->width * ctx->height / 2;
		}
		ffd->out_size = outsize;

		if (!ffd->no_par_update && ctx->sample_aspect_ratio.num && ctx->sample_aspect_ratio.den) {
			ffd->previous_par = (ctx->sample_aspect_ratio.num<<16) | ctx->sample_aspect_ratio.den;
		}

		/*we didn't get any picture: wait for a picture before resizing output buffer, otherwise we will have no
		video in the output buffer*/
		if (!gotpic) {
			ffd->needs_output_resize = 1;
			return GF_OK;
		}
		*outBufferLength = ffd->out_size;
		if (ffd->check_h264_isma) {
			inBuffer[0] = inBuffer[1] = inBuffer[2] = 0;
			inBuffer[3] = 1;
		}
#ifdef FFMPEG_SWSCALE
		if (*cached_sws) {
			sws_freeContext(*cached_sws);
			*cached_sws = NULL;
		}
#endif
		ffd->had_pic = 1;

		if (ffd->conv_to_8bit && ffd->direct_output) {
			ffd->conv_buffer = gf_realloc(ffd->conv_buffer, sizeof(char)*ffd->out_size);
		}

		return GF_BUFFER_TOO_SMALL;
	}
	/*check PAR in case on-the-fly change*/
	if (!ffd->no_par_update && ctx->sample_aspect_ratio.num && ctx->sample_aspect_ratio.den) {
		u32 new_par = (ctx->sample_aspect_ratio.num<<16) | ctx->sample_aspect_ratio.den;
		if (ffd->previous_par && (new_par != ffd->previous_par)) {
			ffd->previous_par = new_par;

			if (!gotpic) {
				ffd->needs_output_resize = 1;
				return GF_OK;
			}
			*outBufferLength = ffd->out_size;
			ffd->had_pic = 1;
			return GF_BUFFER_TOO_SMALL;
		}
	}

//	if (mmlevel	== GF_CODEC_LEVEL_SEEK) return GF_OK;

	if (!gotpic) return GF_OK;

#if (LIBAVCODEC_VERSION_MAJOR>52)
	//fixme - investigate this, happens in some dash cases
	if ((frame->width!=ctx->width) || (frame->height!=ctx->height))  {
		*outBufferLength = 0;
		return GF_OK;
	}
#endif

	if (ffd->direct_output && !ffd->conv_to_8bit) {
		*outBufferLength = ffd->out_size;
		return GF_OK;
	}

	if (ffd->conv_to_8bit) {
		GF_VideoSurface dst;
		memset(&dst, 0, sizeof(GF_VideoSurface));
		dst.width = ctx->width;
		dst.height = ctx->height;
		dst.pitch_y = ctx->width;
		dst.video_buffer = ffd->direct_output ? ffd->conv_buffer : outBuffer;
		dst.pixel_format = GF_PIXEL_YV12;

		gf_color_write_yv12_10_to_yuv(&dst, (u8 *) frame->data[0], frame->data[1], frame->data[2], frame->linesize[0], ctx->width, ctx->height, NULL, 0);
		*outBufferLength = ffd->out_size;
		return GF_OK;
	}


	if (ES_ID == ffd->depth_ES_ID) {
		s32 i;
		u8 *pYO, *pYD;

		pYO = frame->data[0];
		pYD = (u8 *) outBuffer+ffd->yuv_size;
		for (i=0; i<ctx->height; i++) {
			memcpy(pYD, pYO, sizeof(char) * ctx->width);
			pYD += ctx->width;
			pYO += frame->linesize[0];
		}
		*outBufferLength = ffd->out_size;
		return GF_OK;
	}

#if defined(_WIN32_WCE) || defined(__SYMBIAN32__)
	if (ffd->pix_fmt==GF_PIXEL_RGB_24) {
		memcpy(outBuffer, frame->data[0], sizeof(char)*3*ctx->width);
	} else {
		s32 i;
		char *pYO, *pUO, *pVO;
		unsigned char *pYD, *pUD, *pVD;
		pYO = frame->data[0];
		pUO = frame->data[1];
		pVO = frame->data[2];
		pYD = outBuffer;
		pUD = outBuffer + ctx->width * ctx->height;
		pVD = outBuffer + 5 * ctx->width * ctx->height / 4;


		for (i=0; i<ctx->height; i++) {
			memcpy(pYD, pYO, sizeof(char) * ctx->width);
			pYD += ctx->width;
			pYO += frame->linesize[0];
			if (i%2) continue;

			memcpy(pUD, pUO, sizeof(char) * ctx->width/2);
			memcpy(pVD, pVO, sizeof(char) * ctx->width/2);
			pUD += ctx->width/2;
			pVD += ctx->width/2;
			pUO += frame->linesize[1];
			pVO += frame->linesize[2];
		}
		*outBufferLength = ffd->out_size;
	}
#else

	memset(&pict, 0, sizeof(pict));
	if (ffd->out_pix_fmt==GF_PIXEL_RGB_24) {
		pict.data[0] =  (uint8_t *)outBuffer;
		pict.linesize[0] = 3*ctx->width;
		pix_out = PIX_FMT_RGB24;
	} else {
		pict.data[0] =  (uint8_t *)outBuffer;
		pict.data[1] =  (uint8_t *)outBuffer + ffd->stride * ctx->height;
		pict.data[2] =  (uint8_t *)outBuffer + 5 * ffd->stride * ctx->height / 4;
		pict.linesize[0] = ffd->stride;
		pict.linesize[1] = pict.linesize[2] = ffd->stride/2;
		pix_out = PIX_FMT_YUV420P;
#ifndef NO_10bit
		//this YUV format is handled natively in GPAC
		if (ctx->pix_fmt==PIX_FMT_YUV420P10LE) {
			pix_out = PIX_FMT_YUV420P10LE;
		}
#endif
		if (!mmlevel && frame->interlaced_frame) {
			avpicture_deinterlace((AVPicture *) frame, (AVPicture *) frame, ctx->pix_fmt, ctx->width, ctx->height);
		}
	}
	pict.data[3] = 0;
	pict.linesize[3] = 0;
#ifndef FFMPEG_SWSCALE
	img_convert(&pict, pix_out, (AVPicture *) frame, ctx->pix_fmt, ctx->width, ctx->height);
#else
	*cached_sws = sws_getCachedContext(*cached_sws,
	                                   ctx->width, ctx->height, ctx->pix_fmt,
	                                   ctx->width, ctx->height, pix_out,
	                                   SWS_BICUBIC, NULL, NULL, NULL);
	if ((*cached_sws)) {
#if LIBSWSCALE_VERSION_INT < AV_VERSION_INT(0, 9, 0)
		int sz = sws_scale((*cached_sws), frame->data, frame->linesize, 0, ctx->height, pict.data, pict.linesize);
#else
		int sz = sws_scale((*cached_sws), (const uint8_t * const*)frame->data, frame->linesize, 0, ctx->height, pict.data, pict.linesize);
#endif
		assert( sz > 0 );
	}
#endif

	*outBufferLength = ffd->out_size;
#endif

	return GF_OK;
}

static GF_Err FFDEC_GetOutputBuffer(GF_MediaDecoder *ifcg, u16 ES_ID, u8 **pY_or_RGB, u8 **pU, u8 **pV)
{
	FFDec *ffd = ifcg->privateStack;
	AVFrame *frame;


	if (ffd->conv_buffer) {
		*pY_or_RGB = (u8 *) ffd->conv_buffer;
		*pU = (u8 *) ffd->conv_buffer + ffd->stride * ffd->base_ctx->height;
		*pV = (u8 *) ffd->conv_buffer + 5*ffd->stride * ffd->base_ctx->height/4;
		return GF_OK;
	}

	if (ES_ID && (ffd->depth_ES_ID==ES_ID)) {
		frame = ffd->depth_frame;
		*pY_or_RGB = frame->data[0];
	} else {
		frame = ffd->base_frame;
		*pY_or_RGB = frame->data[0];
		*pU = frame->data[1];
		*pV = frame->data[2];
	}
	return GF_OK;
}

static u32 FFDEC_CanHandleStream(GF_BaseDecoder *plug, u32 StreamType, GF_ESD *esd, u8 PL)
{
	GF_BitStream *bs;
	u32 codec_id;
	Bool check_4cc;
	FFDec *ffd = plug->privateStack;

	/*media type query*/
	if (!esd) {
		if ((StreamType==GF_STREAM_VISUAL) || (StreamType==GF_STREAM_AUDIO)) return GF_CODEC_STREAM_TYPE_SUPPORTED;
		return GF_CODEC_NOT_SUPPORTED;
	}

	/*store types*/
	ffd->oti = esd->decoderConfig->objectTypeIndication;
	ffd->st = StreamType;

	codec_id = 0;
	check_4cc = 0;

	/*private from FFMPEG input*/
	if (ffd->oti == GPAC_OTI_MEDIA_FFMPEG) {
		bs = gf_bs_new(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, GF_BITSTREAM_READ);
		codec_id = gf_bs_read_u32(bs);
		gf_bs_del(bs);
	}
	/*private from IsoMedia input*/
	else if (ffd->oti == GPAC_OTI_MEDIA_GENERIC) {
		bs = gf_bs_new(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, GF_BITSTREAM_READ);
		codec_id = gf_bs_read_u32(bs);
		check_4cc = 1;
		gf_bs_del(bs);
	}
	else if (StreamType==GF_STREAM_AUDIO) {
		/*std MPEG-2 audio*/
		switch (ffd->oti) {
		case GPAC_OTI_AUDIO_MPEG2_PART3:
		case GPAC_OTI_AUDIO_MPEG1:
			codec_id = CODEC_ID_MP2;
			break;
		case GPAC_OTI_AUDIO_AC3:
			codec_id = CODEC_ID_AC3;
			break;
		case GPAC_OTI_AUDIO_EAC3:
			codec_id = CODEC_ID_EAC3;
			break;
		}
	}

	/*std MPEG-4 visual*/
	else if (StreamType==GF_STREAM_VISUAL) {

		/*fixme - we should use some priority rather than declare ffmpeg can't handle svc*/
		if (esd->decoderConfig->objectTypeIndication == GPAC_OTI_VIDEO_AVC) {
			if (esd->decoderConfig->decoderSpecificInfo && esd->decoderConfig->decoderSpecificInfo->data) {
				Bool is_svc = 0;
				u32 i, count;
				GF_AVCConfig *cfg = gf_odf_avc_cfg_read(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength);
				if (!cfg) return GF_CODEC_SUPPORTED;

				if (esd->has_ref_base)
					is_svc = 1;

				/*decode all NALUs*/
				count = gf_list_count(cfg->sequenceParameterSets);
				for (i=0; i<count; i++) {
					GF_AVCConfigSlot *slc = gf_list_get(cfg->sequenceParameterSets, i);
					u8 nal_type = slc->data[0] & 0x1F;

					if (nal_type==GF_AVC_NALU_SVC_SUBSEQ_PARAM) {
						is_svc = 1;
						break;
					}
				}
				gf_odf_avc_cfg_del(cfg);
				return (is_svc || esd->decoderConfig->rvc_config || esd->decoderConfig->predefined_rvc_config) ? GF_CODEC_MAYBE_SUPPORTED : GF_CODEC_SUPPORTED;
			}
			if (esd->decoderConfig->rvc_config || esd->decoderConfig->predefined_rvc_config || esd->has_ref_base) return GF_CODEC_MAYBE_SUPPORTED;
			return GF_CODEC_SUPPORTED;
		}

		switch (ffd->oti) {
		/*MPEG-4 v1 simple profile*/
		case GPAC_OTI_VIDEO_MPEG4_PART2:
			codec_id = CODEC_ID_MPEG4;
			break;
		/*H264 (not std OTI, just the way we use it internally)*/
		case GPAC_OTI_VIDEO_AVC:
			codec_id = CODEC_ID_H264;
			break;
#ifdef HAS_HEVC
		case GPAC_OTI_VIDEO_HEVC:
			codec_id = AV_CODEC_ID_HEVC;
			break;
#endif
		/*MPEG1 video*/
		case GPAC_OTI_VIDEO_MPEG1:
		/*MPEG2 video*/
		case GPAC_OTI_VIDEO_MPEG2_SIMPLE:
		case GPAC_OTI_VIDEO_MPEG2_MAIN:
		case GPAC_OTI_VIDEO_MPEG2_SNR:
		case GPAC_OTI_VIDEO_MPEG2_SPATIAL:
		case GPAC_OTI_VIDEO_MPEG2_HIGH:
		case GPAC_OTI_VIDEO_MPEG2_422:
			codec_id = CODEC_ID_MPEG2VIDEO;
			break;
		/*JPEG*/
		case GPAC_OTI_IMAGE_JPEG:
			codec_id = CODEC_ID_MJPEG;
			/*return maybe supported as FFMPEG JPEG decoder has some issues with many files, so let's use it only if no
			other dec is available*/
			if (avcodec_find_decoder(codec_id) != NULL)
				return GF_CODEC_MAYBE_SUPPORTED;

			return GF_CODEC_NOT_SUPPORTED;
		default:
			return GF_CODEC_NOT_SUPPORTED;
		}
	}
	/*NeroDigital DVD subtitles*/
	else if ((StreamType==GF_STREAM_ND_SUBPIC) && (ffd->oti==0xe0))
		return GF_CODEC_SUPPORTED;

	if (!codec_id) return GF_CODEC_NOT_SUPPORTED;

	if (check_4cc && (ffmpeg_get_codec(codec_id) != NULL)) {
		if (esd->decoderConfig->rvc_config || esd->decoderConfig->predefined_rvc_config) return GF_CODEC_MAYBE_SUPPORTED;
		return GF_CODEC_SUPPORTED;
	}

	if (avcodec_find_decoder(codec_id) != NULL) {
		if (esd->decoderConfig->rvc_config || esd->decoderConfig->predefined_rvc_config) return GF_CODEC_MAYBE_SUPPORTED;

		//for HEVC return MAYBE supported to fallback to openHEVC if present (more optimized for now)
#ifdef HAS_HEVC
		if (codec_id == AV_CODEC_ID_HEVC)
			return GF_CODEC_MAYBE_SUPPORTED;
#endif

		return GF_CODEC_SUPPORTED;
	}

	return GF_CODEC_NOT_SUPPORTED;
}

static const char *FFDEC_GetCodecName(GF_BaseDecoder *dec)
{
	FFDec *ffd;
	if (!dec)
		return NULL;
	ffd = dec->privateStack;
	if (ffd && ffd->base_codec) {
		sprintf(ffd->szCodec, "FFMPEG %s - version %s", ffd->base_codec->name ? ffd->base_codec->name : "unknown", LIBAVCODEC_IDENT);
		return ffd->szCodec;
	}
	return NULL;
}


void *FFDEC_Load()
{
	GF_MediaDecoder *ptr;
	FFDec *priv;

	/* Note for valgrind : those two functions cause a leak in valgrind */
	GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[FFMPEG Decoder] Registering all ffmpeg codecs...\n") );
#ifdef FF_API_AVCODE_INIT /*commit ffmpeg 3211932c513338566b31d990d06957e15a644d13*/
	avcodec_init();
#endif
	avcodec_register_all();
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[FFMPEG Decoder] Done registering all ffmpeg codecs.\n") );


	GF_SAFEALLOC(ptr , GF_MediaDecoder);
	GF_SAFEALLOC(priv , FFDec);
	ptr->privateStack = priv;

	ptr->AttachStream = FFDEC_AttachStream;
	ptr->DetachStream = FFDEC_DetachStream;
	ptr->GetCapabilities = FFDEC_GetCapabilities;
	ptr->SetCapabilities = FFDEC_SetCapabilities;
	ptr->CanHandleStream = FFDEC_CanHandleStream;
	ptr->GetName = FFDEC_GetCodecName;
	ptr->ProcessData = FFDEC_ProcessData;
	ptr->GetOutputBuffer = FFDEC_GetOutputBuffer;

	GF_REGISTER_MODULE_INTERFACE(ptr, GF_MEDIA_DECODER_INTERFACE, "FFMPEG decoder", "gpac distribution");
	return (GF_BaseInterface *) ptr;
}

void FFDEC_Delete(void *ifce)
{
	GF_BaseDecoder *dec = ifce;
	FFDec *ffd;
	if (!ifce)
		return;
	ffd = dec->privateStack;
	dec->privateStack = NULL;
	if (ffd) {
		if (ffd->base_ctx && ffd->base_ctx->codec) avcodec_close(ffd->base_ctx);
		ffd->base_ctx = NULL;
		if (ffd->depth_ctx && ffd->depth_ctx->codec) avcodec_close(ffd->depth_ctx);
		ffd->depth_ctx = NULL;
#ifdef FFMPEG_SWSCALE
		if (ffd->base_sws) sws_freeContext(ffd->base_sws);
		ffd->base_sws = NULL;
		if (ffd->depth_sws) sws_freeContext(ffd->base_sws);
		ffd->depth_sws = NULL;
#endif
		gf_free(ffd);
	}
	gf_free(dec);
}
