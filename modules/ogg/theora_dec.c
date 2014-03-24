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

#ifdef GPAC_HAS_THEORA

#include <theora/theora.h>

typedef struct
{
    theora_info ti;
	theora_state td;
    theora_comment tc;
    ogg_packet op;
	
	u16 ES_ID;
	Bool has_reconfigured;
} TheoraDec;

#define THEORACTX() TheoraDec *ctx = (TheoraDec *) ((OGGWraper *)ifcg->privateStack)->opaque

static GF_Err THEO_AttachStream(GF_BaseDecoder *ifcg, GF_ESD *esd)
{
    ogg_packet oggpacket;
	GF_BitStream *bs;

	THEORACTX();
	if (ctx->ES_ID) return GF_BAD_PARAM;
	
	if (!esd->decoderConfig->decoderSpecificInfo) return GF_NON_COMPLIANT_BITSTREAM;

	if (esd->decoderConfig->objectTypeIndication != GPAC_OTI_MEDIA_OGG) return GF_NON_COMPLIANT_BITSTREAM;
	if ( (esd->decoderConfig->decoderSpecificInfo->dataLength<9) || strncmp(&esd->decoderConfig->decoderSpecificInfo->data[3], "theora", 6)) return GF_NON_COMPLIANT_BITSTREAM;

	oggpacket.granulepos = -1;
	oggpacket.b_o_s = 1;
	oggpacket.e_o_s = 0;
	oggpacket.packetno = 0;

	ctx->ES_ID = esd->ESID;

    theora_info_init(&ctx->ti);
    theora_comment_init(&ctx->tc);


	bs = gf_bs_new(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, GF_BITSTREAM_READ);
	while (gf_bs_available(bs)) {
		oggpacket.bytes = gf_bs_read_u16(bs);
		oggpacket.packet = gf_malloc(sizeof(char) * oggpacket.bytes);
		gf_bs_read_data(bs, oggpacket.packet, oggpacket.bytes);
		if (theora_decode_header(&ctx->ti, &ctx->tc, &oggpacket) < 0 ) {
			gf_free(oggpacket.packet);
			gf_bs_del(bs);
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		gf_free(oggpacket.packet);
	}
    theora_decode_init(&ctx->td, &ctx->ti);
	gf_bs_del(bs);
	return GF_OK;
}

static GF_Err THEO_DetachStream(GF_BaseDecoder *ifcg, u16 ES_ID)
{
	THEORACTX();
	if (ctx->ES_ID != ES_ID) return GF_BAD_PARAM;

	theora_clear(&ctx->td); 
    theora_info_clear(&ctx->ti);
    theora_comment_clear(&ctx->tc);

	ctx->ES_ID = 0;
	return GF_OK;
}
static GF_Err THEO_GetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability *capability)
{
	THEORACTX();
	switch (capability->CapCode) {
	case GF_CODEC_WIDTH:
		capability->cap.valueInt = ctx->ti.width;
		break;
	case GF_CODEC_HEIGHT:
		capability->cap.valueInt = ctx->ti.height;
		break;
	case GF_CODEC_STRIDE:
		capability->cap.valueInt = ctx->ti.width;
		break;
	case GF_CODEC_FPS:
		capability->cap.valueFloat = (Float) ctx->ti.fps_numerator;
		capability->cap.valueFloat /= ctx->ti.fps_denominator;
		break;
	case GF_CODEC_PIXEL_FORMAT:
		capability->cap.valueInt = GF_PIXEL_YV12;
		break;
	case GF_CODEC_REORDER:
		capability->cap.valueInt = 0;
		break;
	case GF_CODEC_RESILIENT:
		capability->cap.valueInt = 1;
		break;
	case GF_CODEC_OUTPUT_SIZE:
		capability->cap.valueInt = 3*ctx->ti.width * ctx->ti.height / 2;
		break;
	case GF_CODEC_BUFFER_MIN:
		capability->cap.valueInt = 1;
		break;
	case GF_CODEC_BUFFER_MAX:
		capability->cap.valueInt = 4;
		break;
	case GF_CODEC_PADDING_BYTES:
		capability->cap.valueInt = 0;
		break;
	default:
		capability->cap.valueInt = 0;
		break;
	}
	return GF_OK;
}

static GF_Err THEO_SetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability capability)
{
	/*return unsupported to avoid confusion by the player (like SR changing ...) */
	return GF_NOT_SUPPORTED;
}


static GF_Err THEO_ProcessData(GF_MediaDecoder *ifcg, 
		char *inBuffer, u32 inBufferLength,
		u16 ES_ID, u32 *CTS,
		char *outBuffer, u32 *outBufferLength,
		u8 PaddingBits, u32 mmlevel)
{
	ogg_packet op;
	yuv_buffer yuv;
	u32 i;
	char *pYO, *pUO, *pVO;
	unsigned char *pYD, *pUD, *pVD;

	THEORACTX();
	/*not using scalabilty*/
	assert(ctx->ES_ID == ES_ID);

	op.granulepos = -1;
	op.b_o_s = 0;
	op.e_o_s = 0;
	op.packetno = 0;
    op.packet = inBuffer;
    op.bytes = inBufferLength;


	*outBufferLength = 0;

    if (theora_decode_packetin(&ctx->td, &op)<0) return GF_NON_COMPLIANT_BITSTREAM;
	if (mmlevel	== GF_CODEC_LEVEL_SEEK) return GF_OK;
    if (theora_decode_YUVout(&ctx->td, &yuv)<0) return GF_OK;

	pYO = yuv.y;
	pUO = yuv.u;
	pVO = yuv.v;
	pYD = outBuffer;
	pUD = outBuffer + ctx->ti.width * ctx->ti.height;
	pVD = outBuffer + 5 * ctx->ti.width * ctx->ti.height / 4;
	
	for (i=0; i<(u32)yuv.y_height; i++) {
		memcpy(pYD, pYO, sizeof(char) * yuv.y_width);
		pYD += ctx->ti.width;
		pYO += yuv.y_stride;
		if (i%2) continue;

		memcpy(pUD, pUO, sizeof(char) * yuv.uv_width);
		memcpy(pVD, pVO, sizeof(char) * yuv.uv_width);
		pUD += ctx->ti.width/2;
		pVD += ctx->ti.width/2;
		pUO += yuv.uv_stride;
		pVO += yuv.uv_stride;
	}
	*outBufferLength = 3*ctx->ti.width*ctx->ti.height/2;
	return GF_OK;
}



static const char *THEO_GetCodecName(GF_BaseDecoder *dec)
{
	return "Theora Decoder";
}

u32 NewTheoraDecoder(GF_BaseDecoder *ifcd)
{
	TheoraDec *dec;
	GF_SAFEALLOC(dec, TheoraDec);
	((OGGWraper *)ifcd->privateStack)->opaque = dec;
	((OGGWraper *)ifcd->privateStack)->type = OGG_THEORA;
	/*setup our own interface*/	
	ifcd->AttachStream = THEO_AttachStream;
	ifcd->DetachStream = THEO_DetachStream;
	ifcd->GetCapabilities = THEO_GetCapabilities;
	ifcd->SetCapabilities = THEO_SetCapabilities;
	((GF_MediaDecoder*)ifcd)->ProcessData = THEO_ProcessData;
	ifcd->GetName = THEO_GetCodecName;
	return 1;
}

void DeleteTheoraDecoder(GF_BaseDecoder *ifcg)
{
	THEORACTX();
	gf_free(ctx);
}

#endif

