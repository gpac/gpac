/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Telecom ParisTech 2010-
 *				Author(s):	Jean Le Feuvre
 *					All rights reserved
 *
 *  This file is part of GPAC / OpenSVC Decoder module
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


#include <gpac/modules/codec.h>
#include <gpac/avparse.h>
#include <gpac/constants.h>


#if (defined(WIN32) || defined(_WIN32_WCE)) && !defined(__GNUC__)
#  pragma comment(lib, "OpenSVCDecoder")
#endif

#include <OpenSVCDecoder/SVCDecoder_ietr_api.h>
#include <OpenSVCDecoder/ParseAU.h>

typedef struct
{
	u16 ES_ID;
	u32 width, stride, height, out_size, pixel_ar, layer;
	Bool first_frame;

	u32 nalu_size_length;

	/*OpenSVC things*/
	void *codec;
	int InitParseAU;
	int svc_init_done;
	int save_Width;
	int save_Height;
	int CurrDqId;
	int MaxDqId;
	int DqIdTable [8];
    int TemporalId;
    int TemporalCom;
} OSVCDec;

static GF_Err OSVC_AttachStream(GF_BaseDecoder *ifcg, GF_ESD *esd)
{
	u32 i, count;
	s32 res;
	OPENSVCFRAME Picture;
	int Layer[4];
	OSVCDec *ctx = (OSVCDec*) ifcg->privateStack;

	/*not supported in this version*/
	if (esd->dependsOnESID) return GF_NOT_SUPPORTED;
	
	ctx->ES_ID = esd->ESID;
	ctx->width = ctx->height = ctx->out_size = 0;
	
	if (esd->decoderConfig->decoderSpecificInfo && esd->decoderConfig->decoderSpecificInfo->data) {
		GF_AVCConfig *cfg = gf_odf_avc_cfg_read(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength);
		if (!cfg) return GF_NON_COMPLIANT_BITSTREAM;
		ctx->nalu_size_length = cfg->nal_unit_size;
		if (SVCDecoder_init(&ctx->codec) == SVC_STATUS_ERROR) return GF_IO_ERR;

		/*decode all NALUs*/
		count = gf_list_count(cfg->sequenceParameterSets);
        SetCommandLayer(Layer, 255, 0, &i, 0);//bufindex can be reset without pb
		for (i=0; i<count; i++) {
			u32 w, h, par_n, par_d;
			GF_AVCConfigSlot *slc = gf_list_get(cfg->sequenceParameterSets, i);

			gf_avc_get_sps_info(slc->data, slc->size, &slc->id, &w, &h, &par_n, &par_d);
			/*by default use the base layer*/
			if (!i) {
				if ((ctx->width<w) || (ctx->height<h)) {
					ctx->width = w;
					ctx->height = h;
					if ( ((s32)par_n>0) && ((s32)par_d>0) )
						ctx->pixel_ar = (par_n<<16) || par_d;
				}
			}
			res = decodeNAL(ctx->codec, slc->data, slc->size, &Picture, Layer);
			if (res<0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[SVC Decoder] Error decoding SPS %d\n", res));
			}
		}

		count = gf_list_count(cfg->pictureParameterSets);
		for (i=0; i<count; i++) {
			GF_AVCConfigSlot *slc = gf_list_get(cfg->pictureParameterSets, i);
			res = decodeNAL(ctx->codec, slc->data, slc->size, &Picture, Layer);
			if (res<0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[SVC Decoder] Error decoding PPS %d\n", res));
			}
		}

		gf_odf_avc_cfg_del(cfg);
	} else {
		ctx->nalu_size_length = 0;
		if (SVCDecoder_init(&ctx->codec) == SVC_STATUS_ERROR) return GF_IO_ERR;
	}
	ctx->stride = ctx->width + 32;
	ctx->layer = 0;
	ctx->CurrDqId = ctx->layer;
	ctx->out_size = ctx->stride * ctx->height * 3 / 2;
	return GF_OK;
}
static GF_Err OSVC_DetachStream(GF_BaseDecoder *ifcg, u16 ES_ID)
{
	OSVCDec *ctx = (OSVCDec*) ifcg->privateStack;

	if (ctx->codec) SVCDecoder_close(ctx->codec);
	ctx->codec = NULL;
	ctx->width = ctx->height = ctx->out_size = 0;
	return GF_OK;
}
static GF_Err OSVC_GetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability *capability)
{
	OSVCDec *ctx = (OSVCDec*) ifcg->privateStack;

	switch (capability->CapCode) {
	case GF_CODEC_RESILIENT:
		capability->cap.valueInt = 1;
		break;
	case GF_CODEC_WIDTH:
		capability->cap.valueInt = ctx->width;
		break;
	case GF_CODEC_HEIGHT:
		capability->cap.valueInt = ctx->height;
		break;
	case GF_CODEC_STRIDE:
		capability->cap.valueInt = ctx->stride;
		break;
	case GF_CODEC_PAR:
		capability->cap.valueInt = ctx->pixel_ar;
		break;
	case GF_CODEC_OUTPUT_SIZE:
		capability->cap.valueInt = ctx->out_size;
		break;
	case GF_CODEC_PIXEL_FORMAT:
		capability->cap.valueInt = GF_PIXEL_YV12;
		break;
	case GF_CODEC_BUFFER_MIN:
		capability->cap.valueInt = 1;
		break;
	case GF_CODEC_BUFFER_MAX:
		capability->cap.valueInt = 4;
		break;
	case GF_CODEC_PADDING_BYTES:
		capability->cap.valueInt = 32;
		break;
	case GF_CODEC_REORDER:
		capability->cap.valueInt = 1;
		break;
	/*not known at our level...*/
	case GF_CODEC_CU_DURATION:
	default:
		capability->cap.valueInt = 0;
		break;
	}
	return GF_OK;
}
static GF_Err OSVC_SetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability capability)
{
	OSVCDec *ctx = (OSVCDec*) ifcg->privateStack;
	switch (capability.CapCode) {
	case GF_CODEC_MEDIA_SWITCH_QUALITY:
		if (capability.cap.valueInt) {
			// set layer up (command=1)
			UpdateLayer( ctx->DqIdTable, &ctx->CurrDqId, &ctx->TemporalCom, &ctx->TemporalId, ctx->MaxDqId, 1 );
		} else {
			// set layer down (command=0)
			UpdateLayer( ctx->DqIdTable, &ctx->CurrDqId, &ctx->TemporalCom, &ctx->TemporalId, ctx->MaxDqId, 0 );
		}
		return GF_OK;
	}
	/*return unsupported to avoid confusion by the player (like color space changing ...) */
	return GF_NOT_SUPPORTED;

}

static GF_Err OSVC_ProcessData(GF_MediaDecoder *ifcg, 
		char *inBuffer, u32 inBufferLength,
		u16 ES_ID,
		char *outBuffer, u32 *outBufferLength,
		u8 PaddingBits, u32 mmlevel)
{

	s32 got_pic;
	OPENSVCFRAME pic;
    int Layer[4];
	OSVCDec *ctx = (OSVCDec*) ifcg->privateStack;

	if (!ES_ID || (ES_ID!=ctx->ES_ID) || !ctx->codec) {
		*outBufferLength = 0;
		return GF_OK;
	}
	if (*outBufferLength < ctx->out_size) {
		*outBufferLength = ctx->out_size;
		return GF_BUFFER_TOO_SMALL;
	}

	got_pic = 0;
	if (ctx->nalu_size_length) {
		u32 i, nalu_size = 0;
		u8 *ptr = inBuffer;

		ctx->MaxDqId = GetDqIdMax(inBuffer, inBufferLength, ctx->nalu_size_length, ctx->DqIdTable, 1);
		if(!ctx->InitParseAU){
			if (ctx->MaxDqId == -1) {
				//AVC stream in a h264 file 
				ctx->MaxDqId = 0;
			} else {
				//Firts time only, we parse the first AU to know the file configuration
				//does not need to ba called again ever after, unless SPS or PPS changed
				ParseAuPlayers(ctx->codec, inBuffer, inBufferLength, ctx->nalu_size_length, 1);
			}
			ctx->InitParseAU = 1;
		}
        SetCommandLayer(Layer, ctx->MaxDqId, ctx->CurrDqId, &ctx->TemporalCom, ctx->TemporalId);

		while (inBufferLength) {
			for (i=0; i<ctx->nalu_size_length; i++) {
				nalu_size = (nalu_size<<8) + ptr[i];
			}
			ptr += ctx->nalu_size_length;

			if (!got_pic) 
				got_pic = decodeNAL(ctx->codec, ptr, nalu_size, &pic, Layer);
			else
				decodeNAL(ctx->codec, ptr, nalu_size, &pic, Layer);

			ptr += nalu_size;
			if (inBufferLength < nalu_size + ctx->nalu_size_length) break;

			inBufferLength -= nalu_size + ctx->nalu_size_length;
		}
	} else {
	}
	if (got_pic!=1) return GF_OK;

	if ((pic.Width != ctx->width) || (pic.Height!=ctx->height)) {
		ctx->width = pic.Width;
		ctx->stride = pic.Width + 32;
		ctx->height = pic.Height;
		ctx->out_size = ctx->stride * ctx->height * 3 / 2;

		/*always force layer resize*/
		*outBufferLength = ctx->out_size;
		return GF_BUFFER_TOO_SMALL;
	}
	*outBufferLength = ctx->out_size;
	memcpy(outBuffer, pic.pY[0], ctx->stride*ctx->height);
	memcpy(outBuffer + ctx->stride * ctx->height, pic.pU[0], ctx->stride*ctx->height/4);
	memcpy(outBuffer + 5*ctx->stride * ctx->height/4, pic.pV[0], ctx->stride*ctx->height/4);

	return GF_OK;
}

static u32 OSVC_CanHandleStream(GF_BaseDecoder *dec, u32 StreamType, GF_ESD *esd, u8 PL)
{
	if (StreamType != GF_STREAM_VISUAL) return GF_CODEC_NOT_SUPPORTED;

	/*media type query*/
	if (!esd) return GF_CODEC_STREAM_TYPE_SUPPORTED;

	switch (esd->decoderConfig->objectTypeIndication) {
	case GPAC_OTI_VIDEO_AVC:
		if (esd->decoderConfig->decoderSpecificInfo && esd->decoderConfig->decoderSpecificInfo->data) {
			Bool is_svc = 0;
			u32 i, count;
			GF_AVCConfig *cfg = gf_odf_avc_cfg_read(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength);
			if (!cfg) return GF_CODEC_NOT_SUPPORTED;

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
			return is_svc ? GF_CODEC_SUPPORTED : GF_CODEC_MAYBE_SUPPORTED;
		}
		return GF_CODEC_MAYBE_SUPPORTED;
	}
	return GF_CODEC_NOT_SUPPORTED;
}

static const char *OSVC_GetCodecName(GF_BaseDecoder *dec)
{
	return "OpenSVC Decoder";
}

GF_BaseDecoder *NewOSVCDec()
{
	GF_MediaDecoder *ifcd;
	OSVCDec *dec;
	
	GF_SAFEALLOC(ifcd, GF_MediaDecoder);
	GF_SAFEALLOC(dec, OSVCDec);
	GF_REGISTER_MODULE_INTERFACE(ifcd, GF_MEDIA_DECODER_INTERFACE, "OpenSVC Decoder", "gpac distribution")

	ifcd->privateStack = dec;

	/*setup our own interface*/	
	ifcd->AttachStream = OSVC_AttachStream;
	ifcd->DetachStream = OSVC_DetachStream;
	ifcd->GetCapabilities = OSVC_GetCapabilities;
	ifcd->SetCapabilities = OSVC_SetCapabilities;
	ifcd->GetName = OSVC_GetCodecName;
	ifcd->CanHandleStream = OSVC_CanHandleStream;
	ifcd->ProcessData = OSVC_ProcessData;
	return (GF_BaseDecoder *) ifcd;
}

void DeleteOSVCDec(GF_BaseDecoder *ifcg)
{
	OSVCDec *ctx = (OSVCDec*) ifcg->privateStack;
	gf_free(ctx);
	gf_free(ifcg);
}

const u32 *QueryInterfaces() 
{
	static u32 si [] = {
#ifndef GPAC_DISABLE_AV_PARSERS
		GF_MEDIA_DECODER_INTERFACE,
#endif
		0
	};
	return si; 
}

GF_BaseInterface *LoadInterface(u32 InterfaceType) 
{
#ifndef GPAC_DISABLE_AV_PARSERS
	if (InterfaceType == GF_MEDIA_DECODER_INTERFACE) return (GF_BaseInterface *)NewOSVCDec();
#endif
	return NULL;
}

void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
#ifndef GPAC_DISABLE_AV_PARSERS
	case GF_MEDIA_DECODER_INTERFACE: 
		DeleteOSVCDec((GF_BaseDecoder*)ifce);
		break;
#endif
	}
}
