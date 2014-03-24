/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / XIPH.org module
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


#include "ogg_in.h"

#ifdef GPAC_HAS_VORBIS

#include <vorbis/codec.h>

typedef struct
{
    vorbis_info vi;
	vorbis_dsp_state vd;
	vorbis_block vb;
    vorbis_comment vc;
    ogg_packet op;

	u16 ES_ID;
	Bool has_reconfigured;
} VorbDec;

#define VORBISCTX() VorbDec *ctx = (VorbDec *) ((OGGWraper *)ifcg->privateStack)->opaque

static GF_Err VORB_AttachStream(GF_BaseDecoder *ifcg, GF_ESD *esd)
{
    ogg_packet oggpacket;
	GF_BitStream *bs;

	VORBISCTX();
	if (ctx->ES_ID) return GF_BAD_PARAM;

	if (!esd->decoderConfig->decoderSpecificInfo || !esd->decoderConfig->decoderSpecificInfo->data) return GF_NON_COMPLIANT_BITSTREAM;
	if (esd->decoderConfig->objectTypeIndication != GPAC_OTI_MEDIA_OGG) return GF_NON_COMPLIANT_BITSTREAM;
	if ((esd->decoderConfig->decoderSpecificInfo->dataLength<9) || strncmp(&esd->decoderConfig->decoderSpecificInfo->data[3], "vorbis", 6)) return GF_NON_COMPLIANT_BITSTREAM;

	ctx->ES_ID = esd->ESID;

    vorbis_info_init(&ctx->vi);
    vorbis_comment_init(&ctx->vc);

	oggpacket.granulepos = -1;
	oggpacket.b_o_s = 1;
	oggpacket.e_o_s = 0;
	oggpacket.packetno = 0;

	bs = gf_bs_new(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, GF_BITSTREAM_READ);
	while (gf_bs_available(bs)) {
		oggpacket.bytes = gf_bs_read_u16(bs);
		oggpacket.packet = gf_malloc(sizeof(char) * oggpacket.bytes);
		gf_bs_read_data(bs, oggpacket.packet, oggpacket.bytes);
		if (vorbis_synthesis_headerin(&ctx->vi, &ctx->vc, &oggpacket) < 0 ) {
			gf_free(oggpacket.packet);
			gf_bs_del(bs);
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		gf_free(oggpacket.packet);
	}
	vorbis_synthesis_init(&ctx->vd, &ctx->vi);
	vorbis_block_init(&ctx->vd, &ctx->vb);
	gf_bs_del(bs);

	return GF_OK;
}

static GF_Err VORB_DetachStream(GF_BaseDecoder *ifcg, u16 ES_ID)
{
	VORBISCTX();
	if (ctx->ES_ID != ES_ID) return GF_BAD_PARAM;

	vorbis_block_clear(&ctx->vb);
	vorbis_dsp_clear(&ctx->vd);
    vorbis_info_clear(&ctx->vi);
    vorbis_comment_clear(&ctx->vc);

	ctx->ES_ID = 0;
	return GF_OK;
}
static GF_Err VORB_GetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability *capability)
{
	VORBISCTX();
	switch (capability->CapCode) {
	case GF_CODEC_RESILIENT:
		capability->cap.valueInt = 1;
		break;
	case GF_CODEC_OUTPUT_SIZE:
		capability->cap.valueInt = vorbis_info_blocksize(&ctx->vi, 1) * 2 * ctx->vi.channels;
		break;
	case GF_CODEC_SAMPLERATE:
		capability->cap.valueInt = ctx->vi.rate;
		break;
	case GF_CODEC_NB_CHAN:
		capability->cap.valueInt = ctx->vi.channels;
		break;
	case GF_CODEC_BITS_PER_SAMPLE:
		capability->cap.valueInt = 16;
		break;
	case GF_CODEC_BUFFER_MIN:
		capability->cap.valueInt = 4;
		break;
	case GF_CODEC_BUFFER_MAX:
		capability->cap.valueInt = ctx->vi.rate / 4 / vorbis_info_blocksize(&ctx->vi, 0);
		/*blocks are not of fixed size, so indicate a default CM size*/
		//capability->cap.valueInt = 12 * vorbis_info_blocksize(&ctx->vi, 1) / vorbis_info_blocksize(&ctx->vi, 0);
		break;
	case GF_CODEC_CU_DURATION:
		/*this CANNOT work with vorbis, blocks are not of fixed size*/
		capability->cap.valueInt = 0;
		break;
	case GF_CODEC_PADDING_BYTES:
		capability->cap.valueInt = 0;
		break;
	case GF_CODEC_CHANNEL_CONFIG:
		switch (ctx->vi.channels) {
		case 1: capability->cap.valueInt = GF_AUDIO_CH_FRONT_CENTER; break;
		case 2:
			capability->cap.valueInt = GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT;
			break;
		case 3:
			capability->cap.valueInt = GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT | GF_AUDIO_CH_FRONT_CENTER;
			break;
		case 4:
			capability->cap.valueInt = GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT | GF_AUDIO_CH_BACK_LEFT | GF_AUDIO_CH_BACK_RIGHT;
			break;
		case 5:
			capability->cap.valueInt = GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT | GF_AUDIO_CH_FRONT_CENTER | GF_AUDIO_CH_BACK_LEFT | GF_AUDIO_CH_BACK_RIGHT;
			break;
		case 6:
			capability->cap.valueInt = GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT | GF_AUDIO_CH_FRONT_CENTER | GF_AUDIO_CH_BACK_LEFT | GF_AUDIO_CH_BACK_RIGHT | GF_AUDIO_CH_LFE;
			break;
		}
		break;
	default:
		capability->cap.valueInt = 0;
		break;
	}
	return GF_OK;
}

static GF_Err VORB_SetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability capability)
{
	/*return unsupported to avoid confusion by the player (like SR changing ...) */
	return GF_NOT_SUPPORTED;
}


static GFINLINE void vorbis_to_intern(u32 samples, Float **pcm, char *buf, u32 channels)
{
	u32 i, j;
	s32 val;
	ogg_int16_t *ptr, *data = (ogg_int16_t*)buf ;
	Float *mono;

    for (i=0 ; i<channels ; i++) {
		ptr = &data[i];
		if (channels>2) {
			/*center is third in gpac*/
			if (i==1) ptr = &data[2];
			/*right is 2nd in gpac*/
			else if (i==2) ptr = &data[1];
			/*LFE is 4th in gpac*/
			if ((channels==6) && (i>3)) {
				if (i==6) ptr = &data[4];	/*LFE*/
				else ptr = &data[i+1];	/*back l/r*/
			}
		}

		mono = pcm[i];
		for (j=0; j<samples; j++) {
			val = (s32) (mono[j] * 32767.f);
			if (val > 32767) val = 32767;
			if (val < -32768) val = -32768;
			*ptr = val;
			ptr += channels;
		}
    }
}

static GF_Err VORB_ProcessData(GF_MediaDecoder *ifcg,
		char *inBuffer, u32 inBufferLength,
		u16 ES_ID, u32 *CTS,
		char *outBuffer, u32 *outBufferLength,
		u8 PaddingBits, u32 mmlevel)
{
	ogg_packet op;
    Float **pcm;
    u32 samples, total_samples, total_bytes;

	VORBISCTX();
	/*not using scalabilty*/
	assert(ctx->ES_ID == ES_ID);

	op.granulepos = -1;
	op.b_o_s = 0;
	op.e_o_s = 0;
	op.packetno = 0;
    op.packet = inBuffer;
    op.bytes = inBufferLength;


	*outBufferLength = 0;

	if (vorbis_synthesis(&ctx->vb, &op) == 0)
		vorbis_synthesis_blockin(&ctx->vd, &ctx->vb) ;

	/*trust vorbis max block info*/
	total_samples = 0;
	total_bytes = 0;
	while ((samples = vorbis_synthesis_pcmout(&ctx->vd, &pcm)) > 0) {
		vorbis_to_intern(samples, pcm, (char*) outBuffer + total_bytes, ctx->vi.channels);
		total_bytes += samples * 2 * ctx->vi.channels;
		total_samples += samples;
		vorbis_synthesis_read(&ctx->vd, samples);
	}
	*outBufferLength = total_bytes;
	return GF_OK;
}


static const char *VORB_GetCodecName(GF_BaseDecoder *dec)
{
	return "Vorbis Decoder";
}

u32 NewVorbisDecoder(GF_BaseDecoder *ifcd)
{
	VorbDec *dec;
	GF_SAFEALLOC(dec, VorbDec);
	((OGGWraper *)ifcd->privateStack)->opaque = dec;
	((OGGWraper *)ifcd->privateStack)->type = OGG_VORBIS;
	/*setup our own interface*/
	ifcd->AttachStream = VORB_AttachStream;
	ifcd->DetachStream = VORB_DetachStream;
	ifcd->GetCapabilities = VORB_GetCapabilities;
	ifcd->SetCapabilities = VORB_SetCapabilities;
	((GF_MediaDecoder*)ifcd)->ProcessData = VORB_ProcessData;
	ifcd->GetName = VORB_GetCodecName;
	return 1;
}

void DeleteVorbisDecoder(GF_BaseDecoder *ifcg)
{
	VorbDec *ctx;
        if (!ifcg || !ifcg->privateStack)
          return;
        ctx = (VorbDec *) ((OGGWraper *)ifcg->privateStack)->opaque;
        if (ctx){
          gf_free(ctx);
          ((OGGWraper *)ifcg->privateStack)->opaque = NULL;
        }
}

#endif

