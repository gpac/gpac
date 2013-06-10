/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2010-2012
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
#include <gpac/internal/media_dev.h>


#if (defined(WIN32) || defined(_WIN32_WCE)) && !defined(__GNUC__)
#  pragma comment(lib, "OpenSVCDecoder")
#endif

#include <OpenSVCDecoder/SVCDecoder_ietr_api.h>

typedef struct
{
	u16 ES_ID;
	u16 baseES_ID;
	u32 width, stride, height, out_size, pixel_ar;

	u32 nalu_size_length;
	Bool init_layer_set;
	Bool state_found;

	/*OpenSVC things*/
	void *codec;
	int CurrentDqId;
	int MaxDqId;
	int DqIdTable[8];
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

	/*todo: we should check base layer of this stream is indeed our base layer*/ 	
	if (!ctx->ES_ID) {
		ctx->ES_ID = esd->ESID;
	 	ctx->width = ctx->height = ctx->out_size = 0;
		if (!esd->dependsOnESID) ctx->baseES_ID = esd->ESID;
	}

	if (esd->decoderConfig->decoderSpecificInfo && esd->decoderConfig->decoderSpecificInfo->data) {
		GF_AVCConfig *cfg = gf_odf_avc_cfg_read(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength);
		if (!cfg) return GF_NON_COMPLIANT_BITSTREAM;
		if (!esd->dependsOnESID) {
			ctx->nalu_size_length = cfg->nal_unit_size;
			if (SVCDecoder_init(&ctx->codec) == SVC_STATUS_ERROR) return GF_IO_ERR;
		}

		/*decode all NALUs*/
		count = gf_list_count(cfg->sequenceParameterSets);
        SetCommandLayer(Layer, 255, 0, &i, 0);//bufindex can be reset without pb
		for (i=0; i<count; i++) {
			u32 w=0, h=0, par_n=0, par_d=0;
			GF_AVCConfigSlot *slc = gf_list_get(cfg->sequenceParameterSets, i);

#ifndef GPAC_DISABLE_AV_PARSERS
			gf_avc_get_sps_info(slc->data, slc->size, &slc->id, &w, &h, &par_n, &par_d);
#endif
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
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[SVC Decoder] Attach: SPS id=\"%d\" code=\"%d\" size=\"%d\"\n", slc->id, slc->data[0] & 0x1F, slc->size));
		}

		count = gf_list_count(cfg->pictureParameterSets);
		for (i=0; i<count; i++) {
			u32 sps_id, pps_id;
			GF_AVCConfigSlot *slc = gf_list_get(cfg->pictureParameterSets, i);
			gf_avc_get_pps_info(slc->data, slc->size, &pps_id, &sps_id);
			res = decodeNAL(ctx->codec, slc->data, slc->size, &Picture, Layer);
			if (res<0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[SVC Decoder] Error decoding PPS %d\n", res));
			}
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[SVC Decoder] Attach: PPS id=\"%d\" code=\"%d\" size=\"%d\" sps_id=\"%d\"\n", pps_id, slc->data[0] & 0x1F, slc->size, sps_id));
		}
		ctx->state_found = 1;
		gf_odf_avc_cfg_del(cfg);
	} else {
		if (ctx->nalu_size_length) {
			return GF_NOT_SUPPORTED;
		}
		ctx->nalu_size_length = 0;
		if (!esd->dependsOnESID) {
			if (SVCDecoder_init(&ctx->codec) == SVC_STATUS_ERROR) return GF_IO_ERR;
		}
		ctx->pixel_ar = (1<<16) || 1;
	}
	ctx->stride = ctx->width + 32;
	ctx->CurrentDqId = ctx->MaxDqId = 0;
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
			if (ctx->CurrentDqId < ctx->MaxDqId)
				// set layer up (command=1)
				UpdateLayer( ctx->DqIdTable, &ctx->CurrentDqId, &ctx->TemporalCom, &ctx->TemporalId, ctx->MaxDqId, 1 );
		} else {
			if (ctx->CurrentDqId > 0)
				// set layer down (command=0)
				UpdateLayer( ctx->DqIdTable, &ctx->CurrentDqId, &ctx->TemporalCom, &ctx->TemporalId, ctx->MaxDqId, 0 );
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
	u32 i, nalu_size, sc_size;
	u8 *ptr;
	OSVCDec *ctx = (OSVCDec*) ifcg->privateStack;
	u32 curMaxDqId = ctx->MaxDqId;

	if (!ES_ID || !ctx->codec) {
		*outBufferLength = 0;
		return GF_OK;
	}
	if (*outBufferLength < ctx->out_size) {
		*outBufferLength = ctx->out_size;
		return GF_BUFFER_TOO_SMALL;
	}

	ctx->MaxDqId = GetDqIdMax(inBuffer, inBufferLength, ctx->nalu_size_length, ctx->DqIdTable, ctx->nalu_size_length ? 1 : 0);
	if (!ctx->init_layer_set) {
		//AVC stream in a h264 file 
		if (ctx->MaxDqId == -1) 
			ctx->MaxDqId = 0;
		
		ctx->CurrentDqId = ctx->MaxDqId;
		ctx->init_layer_set = 1;
	}
	if (curMaxDqId != ctx->MaxDqId)
		ctx->CurrentDqId = ctx->MaxDqId;
	/*decode only current layer*/
	SetCommandLayer(Layer, ctx->MaxDqId, ctx->CurrentDqId, &ctx->TemporalCom, ctx->TemporalId);

	got_pic = 0;
	nalu_size = 0;
	ptr = inBuffer;

	if (!ctx->nalu_size_length) {
		u32 size;
		sc_size = 0;
		size = gf_media_nalu_next_start_code(inBuffer, inBufferLength, &sc_size);
		if (sc_size) {
			ptr += size+sc_size;
			assert(inBufferLength >= size+sc_size);
			inBufferLength -= size+sc_size;
		} else {
			/*no annex-B start-code found, discard */
			*outBufferLength = 0;
			return GF_OK;
		}
	}

	while (inBufferLength) {
		if (ctx->nalu_size_length) {
			for (i=0; i<ctx->nalu_size_length; i++) {
				nalu_size = (nalu_size<<8) + ptr[i];
			}
			ptr += ctx->nalu_size_length;
		} else {
			u32 sc_size;
			nalu_size = gf_media_nalu_next_start_code(ptr, inBufferLength, &sc_size);
		}
#ifndef GPAC_DISABLE_LOG
		switch (ptr[0] & 0x1F) {
			case GF_AVC_NALU_SEQ_PARAM:
			case GF_AVC_NALU_SVC_SUBSEQ_PARAM:
				{
					u32 sps_id;
					gf_avc_get_sps_info((char *)ptr, nalu_size, &sps_id, NULL, NULL, NULL, NULL);
					GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[SVC Decoder] ES%d: SPS id=\"%d\" code=\"%d\" size=\"%d\"\n", ES_ID, sps_id, ptr[0] & 0x1F, nalu_size));
				}
				break;
			case GF_AVC_NALU_PIC_PARAM:
				{
					u32 sps_id, pps_id;
					gf_avc_get_pps_info((char *)ptr, nalu_size, &pps_id, &sps_id);
					GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[SVC Decoder] ES%d: PPS id=\"%d\" code=\"%d\" size=\"%d\" sps_id=\"%d\"\n", ES_ID, pps_id, ptr[0] & 0x1F, nalu_size, sps_id));
				}
				break;
			default:
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[SVC Decoder] ES%d: NALU code=\"%d\" size=\"%d\"\n", ES_ID, ptr[0] & 0x1F, nalu_size));
		}
#endif
		if (!ctx->state_found) {
			u8 nal_type = (ptr[0] & 0x1F) ;
			switch (nal_type) {
			case GF_AVC_NALU_SEQ_PARAM:
			case GF_AVC_NALU_PIC_PARAM:
				if (ctx->baseES_ID == ES_ID)
					ctx->state_found = 1;
				break;
			}
		}

		if (ctx->state_found) {
			if (!got_pic) 
				got_pic = decodeNAL(ctx->codec, ptr, nalu_size, &pic, Layer);
			else
				decodeNAL(ctx->codec, ptr, nalu_size, &pic, Layer);
		}

		ptr += nalu_size;
		if (ctx->nalu_size_length) {
			if (inBufferLength < nalu_size + ctx->nalu_size_length) break;
			inBufferLength -= nalu_size + ctx->nalu_size_length;
		} else {
			if (!sc_size || (inBufferLength < nalu_size + sc_size)) break;
			inBufferLength -= nalu_size + sc_size;
			ptr += sc_size;
		}
	}

	if (got_pic!=1) {
		*outBufferLength = 0;
		return GF_OK;
	}

	if ((curMaxDqId != ctx->MaxDqId) || (pic.Width != ctx->width) || (pic.Height!=ctx->height)) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[SVC Decoder] Resizing from %dx%d to %dx%d\n", ctx->width, ctx->height, pic.Width, pic.Height ));
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
	case GPAC_OTI_VIDEO_SVC:
		if (esd->decoderConfig->decoderSpecificInfo && esd->decoderConfig->decoderSpecificInfo->data) {
			Bool is_svc = 0;
			u32 i, count;
			GF_AVCConfig *cfg = gf_odf_avc_cfg_read(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength);
			if (!cfg) return GF_CODEC_NOT_SUPPORTED;

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
			return is_svc ? GF_CODEC_SUPPORTED : GF_CODEC_MAYBE_SUPPORTED;
		}
		return esd->has_ref_base ? GF_CODEC_SUPPORTED : GF_CODEC_MAYBE_SUPPORTED;
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

GPAC_MODULE_EXPORT
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

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType) 
{
#ifndef GPAC_DISABLE_AV_PARSERS
	if (InterfaceType == GF_MEDIA_DECODER_INTERFACE) return (GF_BaseInterface *)NewOSVCDec();
#endif
	return NULL;
}

GPAC_MODULE_EXPORT
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

GPAC_MODULE_STATIC_DELARATION( opensvc )

