/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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


static void FFDEC_LoadDSI(FFDec *ffd, GF_BitStream *bs, Bool from_ff_demux)
{
	u32 dsi_size;
	if (!ffd->codec) return;

	dsi_size = (u32) gf_bs_available(bs);
	if (!dsi_size) return;

	/*demuxer is ffmpeg, extra data can be copied directly*/
	if (from_ff_demux) {
		free(ffd->ctx->extradata);
		ffd->ctx->extradata_size = dsi_size;
		ffd->ctx->extradata = (uint8_t*) malloc(sizeof(char)*ffd->ctx->extradata_size);
		gf_bs_read_data(bs, ffd->ctx->extradata, ffd->ctx->extradata_size);
		return;
	}

	switch (ffd->codec->id) {
	case CODEC_ID_SVQ3:
	{
		u32 at_type, size;
		size = gf_bs_read_u32(bs);
		/*there should be an 'SMI' entry*/
		at_type = gf_bs_read_u32(bs);
		if (at_type == GF_4CC('S', 'M', 'I', ' ')) {
			free(ffd->ctx->extradata);
			ffd->ctx->extradata_size = 0x5a + size;
			ffd->ctx->extradata = (uint8_t*) malloc(sizeof(char)*ffd->ctx->extradata_size);
			strcpy(ffd->ctx->extradata, "SVQ3");
			gf_bs_read_data(bs, (unsigned char *)ffd->ctx->extradata + 0x5a, size);
		}
	}
		break;
	default:
		free(ffd->ctx->extradata);
		ffd->ctx->extradata_size = dsi_size;
		ffd->ctx->extradata = (uint8_t*) malloc(sizeof(char)*ffd->ctx->extradata_size);
		gf_bs_read_data(bs, ffd->ctx->extradata, ffd->ctx->extradata_size);
		break;
	}
}

static GF_Err FFDEC_AttachStream(GF_BaseDecoder *plug, GF_ESD *esd)
{
	u32 codec_id;
	int gotpic;
	GF_BitStream *bs;
	GF_M4VDecSpecInfo dsi;
	GF_Err e;
	FFDec *ffd = (FFDec *)plug->privateStack;
	if (ffd->ES_ID || esd->dependsOnESID || esd->decoderConfig->upstream) return GF_NOT_SUPPORTED;
	if (!ffd->oti) return GF_NOT_SUPPORTED;
	ffd->ES_ID = esd->ESID;

	ffd->ctx = avcodec_alloc_context();
	
	/*private FFMPEG DSI*/
	if (ffd->oti == GPAC_OTI_MEDIA_FFMPEG) {
		bs = gf_bs_new(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, GF_BITSTREAM_READ);
		codec_id = gf_bs_read_u32(bs);
		if (ffd->st==GF_STREAM_AUDIO) {
			ffd->ctx->codec_type = CODEC_TYPE_AUDIO;
			ffd->ctx->sample_rate = gf_bs_read_u32(bs);
			ffd->ctx->channels = gf_bs_read_u16(bs);
			ffd->ctx->frame_size = gf_bs_read_u16(bs);
			/*bits_per_sample */gf_bs_read_u8(bs);
			/*num_frames_per_au*/ gf_bs_read_u8(bs);

			/*ffmpeg specific*/
			ffd->ctx->block_align = gf_bs_read_u16(bs);
		} else if (ffd->st==GF_STREAM_VISUAL) {
			ffd->ctx->codec_type = CODEC_TYPE_VIDEO;
			ffd->ctx->width = gf_bs_read_u16(bs);
			ffd->ctx->height = gf_bs_read_u16(bs);
		}
		ffd->ctx->bit_rate = gf_bs_read_u32(bs);

		ffd->ctx->codec_tag = gf_bs_read_u32(bs);

		ffd->codec = avcodec_find_decoder(codec_id);
		FFDEC_LoadDSI(ffd, bs, 1);
		gf_bs_del(bs);
	} 
	/*private QT DSI*/
	else if (ffd->oti == GPAC_OTI_MEDIA_GENERIC) {
		bs = gf_bs_new(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, GF_BITSTREAM_READ);
		codec_id = gf_bs_read_u32(bs);
		if (ffd->st==GF_STREAM_AUDIO) {
			ffd->ctx->codec_type = CODEC_TYPE_AUDIO;
			ffd->ctx->sample_rate = gf_bs_read_u32(bs);
			ffd->ctx->channels = gf_bs_read_u16(bs);
			ffd->ctx->frame_size = gf_bs_read_u16(bs);
			/*bits_per_sample */ gf_bs_read_u8(bs);
			/*num_frames_per_au*/ gf_bs_read_u8(bs);
			/*just in case...*/
			if (codec_id == GF_4CC('a', 'm', 'r', ' ')) {
			  ffd->ctx->sample_rate = 8000;
			  ffd->ctx->channels = 1;
			  ffd->ctx->frame_size = 160;
			}
		} else if (ffd->st==GF_STREAM_VISUAL) {
			ffd->ctx->codec_type = CODEC_TYPE_VIDEO;
			ffd->ctx->width = gf_bs_read_u16(bs);
			ffd->ctx->height = gf_bs_read_u16(bs);
		}
		ffd->codec = ffmpeg_get_codec(codec_id);
		FFDEC_LoadDSI(ffd, bs, 0);
		gf_bs_del(bs);
	}
	/*use std MPEG-4 st/oti*/
	else {
		u32 codec_id = 0;
		if (ffd->st==GF_STREAM_VISUAL) {
			ffd->ctx->codec_type = CODEC_TYPE_VIDEO;
			switch (ffd->oti) {
			case 0x20:
				codec_id = CODEC_ID_MPEG4;
				break;
			case 0x21:
				codec_id = CODEC_ID_H264;
				/*ffmpeg H264/AVC needs that*/
				//ffd->ctx->codec_tag = 0x31637661;
				break;
			case 0x6A:
			case 0x60:
			case 0x61:
			case 0x62:
			case 0x63:
			case 0x64:
			case 0x65:
				codec_id = CODEC_ID_MPEG2VIDEO;
				break;
			case 0x6C:
				codec_id = CODEC_ID_MJPEG;
				break;
			case 0xFF:
				codec_id = CODEC_ID_SVQ3;
				break;
			}
		} else if (ffd->st==GF_STREAM_AUDIO) {
			ffd->ctx->codec_type = CODEC_TYPE_AUDIO;
			switch (ffd->oti) {
			case 0x69:
			case 0x6B:
				ffd->ctx->frame_size = 1152;
				codec_id = CODEC_ID_MP2;
				break;
			}
		}
		else if ((ffd->st==GF_STREAM_ND_SUBPIC) && (ffd->oti==0xe0)) {
			codec_id = CODEC_ID_DVD_SUBTITLE;
		}
		ffd->codec = avcodec_find_decoder(codec_id);
	}
	/*should never happen*/
	if (!ffd->codec) return GF_OUT_OF_MEM;

	/*setup MPEG-4 video streams*/
	if ((ffd->st==GF_STREAM_VISUAL)) {
		/*for all MPEG-4 variants get size*/
		if ((ffd->oti==0x20) || (ffd->oti == 0x21)) {
			/*if not set this may be a remap of non-mpeg4 transport (eg, transport on MPEG-TS) where
			the DSI is carried in-band*/
			if (esd->decoderConfig->decoderSpecificInfo->data) {

				/*for regular MPEG-4, try to decode and if this fails try H263 decoder at first frame*/
				if (ffd->oti==0x20) {
					e = gf_m4v_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &dsi);
					if (e) return e;
					ffd->ctx->width = dsi.width;
					ffd->ctx->height = dsi.height;
					if (!dsi.width && !dsi.height) ffd->check_short_header = 1;
					ffd->previous_par = (dsi.par_num<<16) | dsi.par_den;
					ffd->no_par_update = 1;
				} else if (ffd->oti==0x21) {
					ffd->check_h264_isma = 1;
				}

				/*setup dsi for FFMPEG context BEFORE attaching decoder (otherwise not proper init)*/
				ffd->ctx->extradata = malloc(sizeof(char)*esd->decoderConfig->decoderSpecificInfo->dataLength);
				memcpy(ffd->ctx->extradata, esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength);
				ffd->ctx->extradata_size = esd->decoderConfig->decoderSpecificInfo->dataLength;
			}
		}
		ffd->frame = avcodec_alloc_frame();
	}

	if (avcodec_open(ffd->ctx, ffd->codec)<0) return GF_NON_COMPLIANT_BITSTREAM;

	/*setup audio streams*/
	if (ffd->st==GF_STREAM_AUDIO) {
		if ((ffd->codec->type == CODEC_ID_MP3LAME) || (ffd->codec->type == CODEC_ID_MP2)) {
			ffd->ctx->frame_size = (ffd->ctx->sample_rate > 24000) ? 1152 : 576;
		}
		/*may be 0 (cfg not known yet)*/
		ffd->out_size = ffd->ctx->channels * ffd->ctx->frame_size * 2 /*16 / 8*/;
		if (!ffd->ctx->sample_rate) ffd->ctx->sample_rate = 44100;
		if (!ffd->ctx->channels) ffd->ctx->channels = 2;
	} else {
		switch (ffd->codec->id) {
		case CODEC_ID_MJPEG:
		case CODEC_ID_MJPEGB:
		case CODEC_ID_LJPEG:
#if (LIBAVCODEC_VERSION_INT > ((51<<16)+(20<<8)+0) )
		case CODEC_ID_GIF:
#endif
			ffd->pix_fmt = GF_PIXEL_RGB_24; 
			break;
		case CODEC_ID_DVD_SUBTITLE:
			ffd->frame = avcodec_alloc_frame();
			avcodec_decode_video(ffd->ctx, ffd->frame, &gotpic, esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength);
			ffd->pix_fmt = GF_PIXEL_YV12; 
			break;
		default:
			ffd->pix_fmt = GF_PIXEL_YV12; 
			break;
		}
		ffd->out_size = ffd->ctx->width * ffd->ctx->height * 3;

		ffd->out_pix_fmt = ffd->pix_fmt;
#if defined(_WIN32_WCE) ||  defined(__SYMBIAN32__)
#else
//		if (ffd->pix_fmt == GF_PIXEL_YV12) ffd->out_pix_fmt = GF_PIXEL_RGB_24;
#endif

		if (ffd->out_pix_fmt != GF_PIXEL_RGB_24) ffd->out_size /= 2;
	}
	return GF_OK;
}
static GF_Err FFDEC_DetachStream(GF_BaseDecoder *plug, u16 ES_ID)
{
	FFDec *ffd = (FFDec *)plug->privateStack;
	if (!ffd->ES_ID) return GF_BAD_PARAM;
	ffd->ES_ID = 0;

	if (ffd->ctx) {
		if (ffd->ctx->extradata) free(ffd->ctx->extradata);
		avcodec_close(ffd->ctx);
		ffd->ctx = NULL;
	}
#ifdef FFMPEG_SWSCALE
	if (ffd->sws_ctx) { sws_freeContext(ffd->sws_ctx); ffd->sws_ctx = NULL; }
#endif
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
	}

	if (!ffd->ctx) {
		capability->cap.valueInt = 0;
		return GF_OK;
	}

	/*caps valid only if stream attached*/
	switch (capability->CapCode) {
	case GF_CODEC_OUTPUT_SIZE:
		capability->cap.valueInt = ffd->out_size;
		break;
	case GF_CODEC_SAMPLERATE:
		capability->cap.valueInt = ffd->ctx->sample_rate;
		break;
	case GF_CODEC_NB_CHAN:
		capability->cap.valueInt = ffd->ctx->channels;
		break;
	case GF_CODEC_BITS_PER_SAMPLE:
		capability->cap.valueInt = 16;
		break;
	case GF_CODEC_BUFFER_MIN:
		capability->cap.valueInt = (ffd->st==GF_STREAM_AUDIO) ? 4 : 1;
		break;
	case GF_CODEC_BUFFER_MAX:
	  /*for audio let the systems engine decide since we may have very large block size (1 sec with some QT movies)*/
		capability->cap.valueInt = (ffd->st==GF_STREAM_AUDIO) ? 0 : 4;
		break;
	/*by default AAC access unit lasts num_samples (timescale being sampleRate)*/
	case GF_CODEC_CU_DURATION:
		capability->cap.valueInt =  (ffd->st==GF_STREAM_AUDIO) ? ffd->ctx->frame_size : 0;
		break;
	case GF_CODEC_WIDTH:
		capability->cap.valueInt = ffd->ctx->width;
		break;
	case GF_CODEC_HEIGHT:
		capability->cap.valueInt = ffd->ctx->height;
		break;
	case GF_CODEC_STRIDE:
		capability->cap.valueInt = ffd->ctx->width;
		if (ffd->out_pix_fmt==GF_PIXEL_RGB_24) capability->cap.valueInt *= 3;
		break;
	case GF_CODEC_FPS:
		capability->cap.valueFloat = 30.0f;
		break;
	case GF_CODEC_PAR:
		capability->cap.valueInt = ffd->previous_par;
		break;
	case GF_CODEC_PIXEL_FORMAT:
		if (ffd->ctx->width) capability->cap.valueInt = ffd->out_pix_fmt;
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
		if (ffd->ctx->channels==1) {
			capability->cap.valueInt = GF_AUDIO_CH_FRONT_CENTER;
		} else {
			capability->cap.valueInt = GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT;
		}
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
	switch (capability.CapCode) {
	case GF_CODEC_WAIT_RAP:
		ffd->frame_start = 0;
		if (ffd->st==GF_STREAM_VISUAL) avcodec_flush_buffers(ffd->ctx);
		return GF_OK;
	default:
		/*return unsupported to avoid confusion by the player (like color space changing ...) */
		return GF_NOT_SUPPORTED;
	}
}

static GF_Err FFDEC_ProcessData(GF_MediaDecoder *plug, 
		char *inBuffer, u32 inBufferLength,
		u16 ES_ID,
		char *outBuffer, u32 *outBufferLength,
		u8 PaddingBits, u32 mmlevel)
{

	s32 gotpic;
	u32 outsize;
	FFDec *ffd = plug->privateStack;

	/*WARNING: this breaks H264 (and maybe others) decoding, disabled for now*/
#if 1
	if (!ffd->ctx->hurry_up) {
		switch (mmlevel) {
		case GF_CODEC_LEVEL_SEEK:
		case GF_CODEC_LEVEL_DROP:
			/*skip as much as possible*/
			ffd->ctx->hurry_up = 5;
			break;
		case GF_CODEC_LEVEL_VERY_LATE:
		case GF_CODEC_LEVEL_LATE:
			/*skip B-frames*/
			ffd->ctx->hurry_up = 1;
			break;
		default:
			ffd->ctx->hurry_up = 0;
			break;
		}
	}
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

redecode:
		len = avcodec_decode_audio(ffd->ctx, (short *)ffd->audio_buf, &gotpic, inBuffer + ffd->frame_start, inBufferLength - ffd->frame_start);

		if (len<0) { ffd->frame_start = 0; return GF_NON_COMPLIANT_BITSTREAM; }
		if (gotpic<0) { ffd->frame_start = 0; return GF_OK; }

		ffd->ctx->hurry_up = 0;

		if (ffd->ctx->frame_size < gotpic) ffd->ctx->frame_size = gotpic;

		/*first config*/
		if (!ffd->out_size) {
			ffd->out_size = ffd->ctx->channels * ffd->ctx->frame_size * 2 /* 16 / 8 */;
		}
		if (ffd->out_size < (u32) gotpic) {
			/*looks like relying on frame_size is not a good idea for all codecs, so we use gotpic*/
			(*outBufferLength) = ffd->out_size = gotpic;
			return GF_BUFFER_TOO_SMALL;
		}
		if (ffd->out_size > buf_size) {
			/*don't use too small output chunks otherwise we'll never have enough when mixing - we could 
			also request more slots in the composition memory but let's not waste mem*/
			if (ffd->out_size < (u32) 576*ffd->ctx->channels) ffd->out_size=ffd->ctx->channels*576;
			(*outBufferLength) = ffd->out_size;
			return GF_BUFFER_TOO_SMALL;
		}
		
		/*we're sure to have at least gotpic bytes available in output*/
		memcpy(outBuffer, ffd->audio_buf, sizeof(char) * gotpic);
		(*outBufferLength) += gotpic;
		outBuffer += gotpic;

		ffd->frame_start += len;
		if (inBufferLength <= ffd->frame_start) {
			ffd->frame_start = 0;
			return GF_OK;
		}
		/*still space go on*/
		if ((*outBufferLength)+ffd->ctx->frame_size<ffd->out_size) goto redecode;

		/*more frames in the current sample*/
		return GF_PACKED_FRAMES;
	} else {
		s32 w = ffd->ctx->width;
		s32 h = ffd->ctx->height;

		if (ffd->check_h264_isma) {
			/*for AVC bitstreams after ISMA decryption, in case (as we do) the decryption DRM tool 
			doesn't put back nalu size, do it ourselves...*/
			if (inBuffer && !inBuffer[0] && !inBuffer[1] && !inBuffer[2] && (inBuffer[3]==0x01)) {
				u32 nalu_size;
				u32 remain = inBufferLength;
				char *start, *end;

				start = inBuffer;
				end = inBuffer + 4;
				while (remain>4) {
					if (!end[0] && !end[1] && !end[2] && (end[3]==0x01)) {
						nalu_size = end - start - 4;
						start[0] = (nalu_size>>24)&0xFF;
						start[1] = (nalu_size>>16)&0xFF;
						start[2] = (nalu_size>>8)&0xFF;
						start[3] = (nalu_size)&0xFF;
						start = end;
						end = start+4;
						continue;
					}
					end++;
					remain--;
				}
				nalu_size = (inBuffer+inBufferLength) - start - 4;
				start[0] = (nalu_size>>24)&0xFF;
				start[1] = (nalu_size>>16)&0xFF;
				start[2] = (nalu_size>>8)&0xFF;
				start[3] = (nalu_size)&0xFF;
				ffd->check_h264_isma = 2;
			}
			/*if we had ISMA E&A and lost it this is likely due to a pck loss - do NOT switch back to regular*/
			else if (ffd->check_h264_isma == 1) {
				ffd->check_h264_isma = 0;
			}
		}

		if (avcodec_decode_video(ffd->ctx, ffd->frame, &gotpic, inBuffer, inBufferLength) < 0) {
			if (!ffd->check_short_header) {
				return GF_NON_COMPLIANT_BITSTREAM;
			}

			/*switch to H263 (ffmpeg MPEG-4 codec doesn't understand short headers)*/
			{
				u32 old_codec = ffd->codec->id;
				ffd->check_short_header = 0;
				/*OK we loose the DSI stored in the codec context, but H263 doesn't need any, and if we're
				here this means the DSI was broken, so no big deal*/
				avcodec_close(ffd->ctx);
				ffd->codec = avcodec_find_decoder(CODEC_ID_H263);
				if (!ffd->codec || (avcodec_open(ffd->ctx, ffd->codec)<0)) return GF_NON_COMPLIANT_BITSTREAM;
				if (avcodec_decode_video(ffd->ctx, ffd->frame, &gotpic, inBuffer, inBufferLength) < 0) {
					/*nope, stay in MPEG-4*/
					avcodec_close(ffd->ctx);
					ffd->codec = avcodec_find_decoder(old_codec);
					assert(ffd->codec);
					avcodec_open(ffd->ctx, ffd->codec);
					return GF_NON_COMPLIANT_BITSTREAM;
				}
			}
		}
		ffd->ctx->hurry_up = 0;
		/*recompute outsize in case on-the-fly change*/
		if ((w != ffd->ctx->width) || (h != ffd->ctx->height)) {
			outsize = ffd->ctx->width * ffd->ctx->height * 3;
			if (ffd->out_pix_fmt != GF_PIXEL_RGB_24) outsize /= 2;
			ffd->out_size = outsize;
			*outBufferLength = ffd->out_size;
			if (ffd->check_h264_isma) {
				inBuffer[0] = inBuffer[1] = inBuffer[2] = 0;
				inBuffer[3] = 1;
			}
#ifdef FFMPEG_SWSCALE
			if (ffd->sws_ctx) { sws_freeContext(ffd->sws_ctx); ffd->sws_ctx = NULL; }
#endif
			return GF_BUFFER_TOO_SMALL;
		}
		/*check PAR in case on-the-fly change*/
		if (!ffd->no_par_update && ffd->ctx->sample_aspect_ratio.num && ffd->ctx->sample_aspect_ratio.den) {
			u32 new_par = (ffd->ctx->sample_aspect_ratio.num<<16) | ffd->ctx->sample_aspect_ratio.den;
			if (new_par != ffd->previous_par) {
				ffd->previous_par = new_par;
				*outBufferLength = ffd->out_size;
				return GF_BUFFER_TOO_SMALL;
			}
		}

		*outBufferLength = 0;
		if (mmlevel	== GF_CODEC_LEVEL_SEEK) return GF_OK;

		if (gotpic) {
#if defined(_WIN32_WCE) ||  defined(__SYMBIAN32__)
			if (ffd->pix_fmt==GF_PIXEL_RGB_24) {
				memcpy(outBuffer, ffd->frame->data[0], sizeof(char)*3*ffd->ctx->width);
			} else {
				s32 i;
				char *pYO, *pUO, *pVO;
				unsigned char *pYD, *pUD, *pVD;
				pYO = ffd->frame->data[0];
				pUO = ffd->frame->data[1];
				pVO = ffd->frame->data[2];
				pYD = outBuffer;
				pUD = outBuffer + ffd->ctx->width * ffd->ctx->height;
				pVD = outBuffer + 5 * ffd->ctx->width * ffd->ctx->height / 4;
				

				for (i=0; i<ffd->ctx->height; i++) {
					memcpy(pYD, pYO, sizeof(char) * ffd->ctx->width);
					pYD += ffd->ctx->width;
					pYO += ffd->frame->linesize[0];
					if (i%2) continue;

					memcpy(pUD, pUO, sizeof(char) * ffd->ctx->width/2);
					memcpy(pVD, pVO, sizeof(char) * ffd->ctx->width/2);
					pUD += ffd->ctx->width/2;
					pVD += ffd->ctx->width/2;
					pUO += ffd->frame->linesize[1];
					pVO += ffd->frame->linesize[2];
				}
				*outBufferLength = ffd->out_size;
			}
#else
			AVPicture pict;
			u32 pix_out;
			memset(&pict, 0, sizeof(pict));
			if (ffd->out_pix_fmt==GF_PIXEL_RGB_24) {
				pict.data[0] = outBuffer;
				pict.linesize[0] = 3*ffd->ctx->width;
				pix_out = PIX_FMT_RGB24;
			} else {
				pict.data[0] = outBuffer;
				pict.data[1] = outBuffer + ffd->ctx->width * ffd->ctx->height;
				pict.data[2] = outBuffer + 5 * ffd->ctx->width * ffd->ctx->height / 4;
				pict.linesize[0] = ffd->ctx->width;
				pict.linesize[1] = pict.linesize[2] = ffd->ctx->width/2;
				pix_out = PIX_FMT_YUV420P;
				if (!mmlevel && ffd->frame->interlaced_frame) {
					avpicture_deinterlace((AVPicture *) ffd->frame, (AVPicture *) ffd->frame, ffd->ctx->pix_fmt, ffd->ctx->width, ffd->ctx->height);
				}

			}
			pict.data[3] = 0;
			pict.linesize[3] = 0;
#ifndef FFMPEG_SWSCALE
			img_convert(&pict, pix_out, (AVPicture *) ffd->frame, ffd->ctx->pix_fmt, ffd->ctx->width, ffd->ctx->height);
#else
			if (!ffd->sws_ctx) 
				ffd->sws_ctx = sws_getContext(ffd->ctx->width, ffd->ctx->height,
		                        ffd->ctx->pix_fmt, ffd->ctx->width, ffd->ctx->height, pix_out, SWS_BICUBIC, 
        	                NULL, NULL, NULL);
			
			if (ffd->sws_ctx)
				sws_scale(ffd->sws_ctx, ffd->frame->data, ffd->frame->linesize, 0, ffd->ctx->height->codec->height, pict.data, pict.linesize);

#endif

			*outBufferLength = ffd->out_size;
#endif
		}
	}
	return GF_OK;
}

static Bool FFDEC_CanHandleStream(GF_BaseDecoder *plug, u32 StreamType, u32 ObjectType, char *decSpecInfo, u32 decSpecInfoSize, u32 PL)
{
	GF_BitStream *bs;
	u32 codec_id;
	Bool check_4cc;
	FFDec *ffd = plug->privateStack;

	if (!ObjectType) {
		if ((StreamType==GF_STREAM_VISUAL) || (StreamType==GF_STREAM_AUDIO)) return 1;
		return 0;
	}

	/*store types*/
	ffd->oti = ObjectType;
	ffd->st = StreamType;

	codec_id = 0;
	check_4cc = 0;
	
	/*private from FFMPEG input*/
	if (ObjectType == GPAC_OTI_MEDIA_FFMPEG) {
		bs = gf_bs_new(decSpecInfo, decSpecInfoSize, GF_BITSTREAM_READ);
		codec_id = gf_bs_read_u32(bs);
		gf_bs_del(bs);
	}
	/*private from IsoMedia input*/
	else if (ObjectType == GPAC_OTI_MEDIA_GENERIC) {
		bs = gf_bs_new(decSpecInfo, decSpecInfoSize, GF_BITSTREAM_READ);
		codec_id = gf_bs_read_u32(bs);
		check_4cc = 1;
		gf_bs_del(bs);
	}
	else if (StreamType==GF_STREAM_AUDIO) {
		/*std MPEG-2 audio*/
		if ((ObjectType==0x69) || (ObjectType==0x6B)) codec_id = CODEC_ID_MP2;
		/*std AC3 audio*/
		//if (ObjectType==0xA5) codec_id = CODEC_ID_AC3;
	} 
	
	/*std MPEG-4 visual*/
	else if (StreamType==GF_STREAM_VISUAL) {
		switch (ObjectType) {
		/*MPEG-4 v1 simple profile*/
		case 0x20: codec_id = CODEC_ID_MPEG4; break;
		/*H264 (not std OTI, just the way we use it internally)*/
		case 0x21: codec_id = CODEC_ID_H264; break;
		/*MPEG1 video*/
		case 0x6A:
		/*MPEG2 video*/
		case 0x60:
		case 0x61:
		case 0x62:
		case 0x63:
		case 0x64:
		case 0x65:
			codec_id = CODEC_ID_MPEG2VIDEO; break;
		/*JPEG*/
		case 0x6C:
			return 0; /*I'm having troubles with ffmpeg & jpeg, it appears to crash randomly*/
			return 1;
		default:
			return 0;
		}
	}
	/*NeroDigital DVD subtitles*/
	else if ((StreamType==GF_STREAM_ND_SUBPIC) && (ObjectType==0xe0)) 
		return 1;

	if (!codec_id) return 0;
	if (check_4cc && (ffmpeg_get_codec(codec_id) != NULL)) return 1;
	if (avcodec_find_decoder(codec_id) != NULL) return 1;
	return 0;
}

static char szCodec[100];
static const char *FFDEC_GetCodecName(GF_BaseDecoder *dec)
{
	FFDec *ffd = dec->privateStack;
	if (ffd->codec) {
		sprintf(szCodec, "FFMPEG %s", ffd->codec->name ? ffd->codec->name : "unknown");
		return szCodec;
	}
	return NULL;
}


void *FFDEC_Load()
{
	GF_MediaDecoder *ptr;
	FFDec *priv;

    avcodec_init();
	avcodec_register_all();

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

	GF_REGISTER_MODULE_INTERFACE(ptr, GF_MEDIA_DECODER_INTERFACE, "FFMPEG decoder", "gpac distribution");
	return (GF_BaseInterface *) ptr;
}

void FFDEC_Delete(void *ifce)
{
	GF_BaseDecoder *dec = ifce;
	FFDec *ffd = dec->privateStack;

	if (ffd->ctx) avcodec_close(ffd->ctx);
	free(ffd);
	free(dec);

}
