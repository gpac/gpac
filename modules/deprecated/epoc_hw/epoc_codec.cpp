/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / AAC reader module
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
#include <gpac/constants.h>
#include <gpac/avparse.h>

#include <mmf/server/mmfcodec.h>
#include <mmf/plugin/mmfcodecimplementationuids.hrh>

#define KMMFFourCCCodeEAACP			0x43414520  // ' ' 'E' 'A' 'C' 

#if defined(__SYMBIAN32__) && !defined(__SERIES60_3X__)
//codec configuration UID
#define  KUidMmfCodecAudioSettings	0x10203622
#endif

enum
{
	GF_EPOC_HAS_AMR = 1,
	GF_EPOC_HAS_AMR_WB = 1<<1,
	GF_EPOC_HAS_AAC = 1<<2,
	GF_EPOC_HAS_HEAAC = 1<<3,
	GF_EPOC_HAS_MP3 = 1<<4,
};

typedef struct
{
	u32 caps;
	Bool is_audio;

	u32 sample_rate, out_size, num_samples;
	u8 num_channels;

	const char *codec_name;
	CMMFCodec *dec;

	CMMFPtrBuffer *mmf_in, *mmf_out;
	TPtr8 ptr_in, ptr_out;
} EPOCCodec;

static void EDEC_LoadCaps(GF_BaseDecoder *ifcg)
{
	EPOCCodec *ctx = (EPOCCodec *)ifcg->privateStack;
	CMMFCodec *codec = NULL;
	TInt err;

	ctx->caps = 0;
	/*AMR*/
	TRAP(err, codec = CMMFCodec::NewL(KMMFFourCCCodeAMR, KMMFFourCCCodePCM16));
	if (err==KErrNone) {
		ctx->caps |= GF_EPOC_HAS_AMR;
		delete codec;
	}
	/*AMR-WB*/
	TRAP(err, codec = CMMFCodec::NewL(KMMFFourCCCodeAWB, KMMFFourCCCodePCM16));
	if (err==KErrNone) {
		ctx->caps |= GF_EPOC_HAS_AMR_WB;
		delete codec;
	}
	/*AAC*/
	TRAP(err, codec = CMMFCodec::NewL(KMMFFourCCCodeAAC, KMMFFourCCCodePCM16));
	if (err==KErrNone) {
		ctx->caps |= GF_EPOC_HAS_AAC;
		delete codec;
	}
	/*HE-AAC*/
	TRAP(err, codec = CMMFCodec::NewL(KMMFFourCCCodeEAACP, KMMFFourCCCodePCM16));
	if (err==KErrNone) {
		ctx->caps |= GF_EPOC_HAS_HEAAC;
		delete codec;
	}

	/*MP3*/
	TRAP(err, codec = CMMFCodec::NewL(KMMFFourCCCodeMP3, KMMFFourCCCodePCM16));
	if (err==KErrNone) {
		ctx->caps |= GF_EPOC_HAS_MP3;
		delete codec;
	}

}


static GF_Err EDEC_AttachStream(GF_BaseDecoder *ifcg, GF_ESD *esd)
{
	RArray<TInt> configParams;
	GF_M4ADecSpecInfo a_cfg;
	Bool aac_sbr_upsample;
	TInt err;
	EPOCCodec *ctx = (EPOCCodec *)ifcg->privateStack;
	if (esd->dependsOnESID) return GF_NOT_SUPPORTED;
	if (ctx->dec) return GF_BAD_PARAM;


	/*audio decs*/
	switch (esd->decoderConfig->objectTypeIndication) {
	/*MPEG2 aac*/
	case GPAC_OTI_AUDIO_AAC_MPEG2_MP:
	case GPAC_OTI_AUDIO_AAC_MPEG2_LCP:
	case GPAC_OTI_AUDIO_AAC_MPEG2_SSRP:
	/*MPEG4 aac*/
	case GPAC_OTI_AUDIO_AAC_MPEG4:
		if (!esd->decoderConfig->decoderSpecificInfo || !esd->decoderConfig->decoderSpecificInfo->data) return GF_NON_COMPLIANT_BITSTREAM;
		if (gf_m4a_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &a_cfg) != GF_OK) return GF_NON_COMPLIANT_BITSTREAM;

		aac_sbr_upsample = 0;
#if !defined(__SYMBIAN32__) || defined(__SERIES60_3X__)
		if (a_cfg.has_sbr && (ctx->caps & GF_EPOC_HAS_HEAAC)) {
			TRAP(err, ctx->dec = CMMFCodec::NewL(KMMFFourCCCodeEAACP, KMMFFourCCCodePCM16));
			if (err != KErrNone) {
				a_cfg.has_sbr = 0;
				goto retry_no_sbr;
			}
			aac_sbr_upsample = (a_cfg.base_sr<=24000) ? 1 : 0;

			configParams.Append(a_cfg.base_sr);						//  0: Input Sample Frequency
			configParams.Append(a_cfg.nb_chan);						//  1: Num Channels [1|2]
			configParams.Append(1);									//  2: Input Profile Object type [1 - LC, 3 - LTP]
			configParams.Append(aac_sbr_upsample ? 2048 : 1024);	//  3: Output Frame Size
			configParams.Append(1024);								//  4: Input Frame Len [1024, 960]
			configParams.Append(a_cfg.base_sr);						//  5: Input Sample Rate
			configParams.Append(0);									//  6: 0
			configParams.Append(aac_sbr_upsample ? 0 : 1);			//  7: Down Sample Mode [0|1]
			configParams.Append(16);								//  8: Sample resolution, 8Khz (8-bit PCM) or 16Khz (16-bit)
			configParams.Append(a_cfg.sbr_sr);						//  9: Output Sample Frequency
			configParams.Append(5);									// 10: Extension Object Type

			TRAP(err, ctx->dec->ConfigureL(TUid::Uid(KUidMmfCodecAudioSettings), (TDesC8&) configParams));
			if (err != KErrNone) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[EPOC Decoder] Failed to configure HE-AAC decoder (error %d) - retrying with AAC\n", err));
				configParams.Reset();
				goto retry_no_sbr;
			}
			ctx->codec_name = "EPOC HE-AAC Decoder";
			ctx->num_channels = a_cfg.nb_chan;
			ctx->num_samples = aac_sbr_upsample ? 2048 : 1024;
			ctx->sample_rate = a_cfg.sbr_sr;
		} else
#endif
		{
retry_no_sbr:
			TRAP(err, ctx->dec = CMMFCodec::NewL(KMMFFourCCCodeAAC, KMMFFourCCCodePCM16));
			if (err != KErrNone) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[EPOC Decoder] Unable to load native codec: error %d\n", err));
				return GF_IO_ERR;
			}
			configParams.Append(a_cfg.base_sr); // Input Sample Rate
			configParams.Append(a_cfg.nb_chan);       // Num Channels [1|2]
			configParams.Append((a_cfg.base_object_type==GF_M4A_AAC_LC) ? 1 : 3);       // AAC Input Profile [1 - LC, 3 - LTP]
			configParams.Append(1024);       // Input Frame Len [1024, 960]
			configParams.Append(0);                    // AAC Down Mixing [0-none | 1 mono | 2 stereo]
			configParams.Append(0);                    // Aac output channels selection {0 - none, 1 - 1, 2 - 2}
			configParams.Append(0);                    // Aac decimation factor {0 - none, 2 - decimation by 2, 4 - decimation by 4}
			configParams.Append(0);                    // Aac concealment - It can be {0 - none, 1 - basic}
			configParams.Append(16);     // Sample resolution - It can be {16 - 16-bit resolution}
			configParams.Append(0);                    // Sample Rate Conversion 0 : none

			TRAP(err, ctx->dec->ConfigureL(TUid::Uid(KUidMmfCodecAudioSettings), (TDesC8&) configParams));
			if (err != KErrNone) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[EPOC Decoder] Failed to configure AAC decoder (error %d)\n", err));
				return GF_NON_COMPLIANT_BITSTREAM;
			}
			ctx->codec_name = "EPOC AAC Decoder";
			ctx->num_channels = a_cfg.nb_chan;
			ctx->num_samples = 1024;
			ctx->sample_rate = a_cfg.base_sr;
		}
		ctx->out_size = ctx->num_channels * ctx->num_samples * 2;
		break;
	/*non-mpeg4 codecs*/
	case GPAC_OTI_MEDIA_GENERIC:
		if (!esd->decoderConfig->decoderSpecificInfo || esd->decoderConfig->decoderSpecificInfo->dataLength<4) return GF_BAD_PARAM;
		if (!strnicmp(esd->decoderConfig->decoderSpecificInfo->data, "samr", 4) || !strnicmp(esd->decoderConfig->decoderSpecificInfo->data, "amr ", 4)) {
			TRAP(err, ctx->dec = CMMFCodec::NewL(KMMFFourCCCodeAMR, KMMFFourCCCodePCM16));
			if (err != KErrNone) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[EPOC Decoder] Unable to load native codec: error %d\n", err));
				return GF_IO_ERR;
			}
			ctx->is_audio = 1;
			ctx->num_channels = 1;
			ctx->num_samples = 160;
			ctx->sample_rate = 8000;
			ctx->out_size = ctx->num_channels * ctx->num_samples * 2;
			ctx->codec_name = "EPOC AMR Decoder";
		}
		else if (!strnicmp(esd->decoderConfig->decoderSpecificInfo->data, "sawb", 4)) {
			TRAP(err, ctx->dec = CMMFCodec::NewL(KMMFFourCCCodeAWB, KMMFFourCCCodePCM16));
			if (err != KErrNone) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[EPOC Decoder] Unable to load native codec: error %d\n", err));
				return GF_IO_ERR;
			}
			ctx->is_audio = 1;
			ctx->num_channels = 1;
			ctx->num_samples = 320;
			ctx->sample_rate = 16000;
			ctx->out_size = ctx->num_channels * ctx->num_samples * 2;
			ctx->codec_name = "EPOC AMR-Wideband Decoder";
		}
		break;
	default:
		return GF_BAD_PARAM;
	}

	ctx->mmf_in = CMMFPtrBuffer::NewL();
	ctx->mmf_out = CMMFPtrBuffer::NewL();

	return GF_OK;
}

static GF_Err EDEC_DetachStream(GF_BaseDecoder *ifcg, u16 ES_ID)
{
	EPOCCodec *ctx = (EPOCCodec *)ifcg->privateStack;

	if (ctx->mmf_in) {
		delete ctx->mmf_in;
		ctx->mmf_in = NULL;
	}
	if (ctx->mmf_out) {
		delete ctx->mmf_out;
		ctx->mmf_out = NULL;
	}
	if (ctx->dec) {
		delete ctx->dec;
		ctx->dec = NULL;
	}
	return GF_OK;
}
static GF_Err EDEC_GetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability *capability)
{
	EPOCCodec *ctx = (EPOCCodec *)ifcg->privateStack;

	switch (capability->CapCode) {
	/*not tested yet*/
	case GF_CODEC_RESILIENT:
		capability->cap.valueInt = 1;
		break;
	case GF_CODEC_OUTPUT_SIZE:
		capability->cap.valueInt = ctx->out_size;
		break;
	case GF_CODEC_SAMPLERATE:
		capability->cap.valueInt = ctx->sample_rate;
		break;
	case GF_CODEC_NB_CHAN:
		capability->cap.valueInt = ctx->num_channels;
		break;
	case GF_CODEC_BITS_PER_SAMPLE:
		capability->cap.valueInt = 16;
		break;
	case GF_CODEC_BUFFER_MIN:
		capability->cap.valueInt = 4;
		break;
	case GF_CODEC_BUFFER_MAX:
		capability->cap.valueInt = 12;
		break;
	case GF_CODEC_CU_DURATION:
		capability->cap.valueInt = ctx->num_samples;
		break;
	/*to refine, it seems that 4 bytes padding is not enough on all streams ?*/
	case GF_CODEC_PADDING_BYTES:
		capability->cap.valueInt = 4;
		break;
	case GF_CODEC_CHANNEL_CONFIG:
		capability->cap.valueInt = (ctx->num_channels==1) ? GF_AUDIO_CH_FRONT_CENTER : (GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT);
		break;
	default:
		capability->cap.valueInt = 0;
		break;
	}
	return GF_OK;
}
static GF_Err EDEC_SetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability capability)
{
	/*return unsupported to avoid confusion by the player (like SR changing ...) */
	return GF_NOT_SUPPORTED;
}


static GF_Err EDEC_ProcessData(GF_MediaDecoder *ifcg,
                               char *inBuffer, u32 inBufferLength,
                               u16 ES_ID, u32 *CTS,
                               char *outBuffer, u32 *outBufferLength,
                               u8 PaddingBits, u32 mmlevel)
{
	TCodecProcessResult res;
	EPOCCodec *ctx = (EPOCCodec *)ifcg->privateStack;

	if (*outBufferLength < ctx->out_size) {
		*outBufferLength = ctx->out_size;
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("AMR buffer too small\n"));
		return GF_BUFFER_TOO_SMALL;
	}

	ctx->ptr_in.Set((TUint8*)inBuffer, inBufferLength, inBufferLength);
	ctx->mmf_in->SetPtr(ctx->ptr_in);
	ctx->ptr_out.Set((TUint8*)outBuffer, *outBufferLength, *outBufferLength);
	ctx->mmf_out->SetPtr(ctx->ptr_out);

	TRAPD(e, res = ctx->dec->ProcessL(*ctx->mmf_in, *ctx->mmf_out));
	if (res.iStatus==TCodecProcessResult::EProcessError) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[EPOC Decoder] Decode failed - error %d\n", res.iStatus));
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	return GF_OK;
}

static const char *EDEC_GetCodecName(GF_BaseDecoder *ifcg)
{
	EPOCCodec *ctx = (EPOCCodec *)ifcg->privateStack;
	return ctx->codec_name;
}

static u32 EDEC_CanHandleStream(GF_BaseDecoder *ifcg, u32 StreamType, GF_ESD *esd, u8 PL)
{
	char *dsi;
	GF_M4ADecSpecInfo a_cfg;
	EPOCCodec *ctx = (EPOCCodec *)ifcg->privateStack;

	/*audio decs*/
	if (StreamType == GF_STREAM_AUDIO) {
		/*media type query*/
		if (!esd) return GF_CODEC_STREAM_TYPE_SUPPORTED;
		dsi = esd->decoderConfig->decoderSpecificInfo ? esd->decoderConfig->decoderSpecificInfo->data : NULL;
		switch (esd->decoderConfig->objectTypeIndication) {
		/*MPEG2 aac*/
		case GPAC_OTI_AUDIO_AAC_MPEG2_MP:
		case GPAC_OTI_AUDIO_AAC_MPEG2_LCP:
		case GPAC_OTI_AUDIO_AAC_MPEG2_SSRP:
		/*MPEG4 aac*/
		case GPAC_OTI_AUDIO_AAC_MPEG4:
			if (!dsi) return GF_CODEC_NOT_SUPPORTED;
			if (gf_m4a_get_config(dsi, esd->decoderConfig->decoderSpecificInfo->dataLength, &a_cfg) != GF_OK) return GF_CODEC_MAYBE_SUPPORTED;
			switch (a_cfg.base_object_type) {
			/*only LTP and LC supported*/
			case GF_M4A_AAC_LC:
			case GF_M4A_AAC_LTP:
				if ((ctx->caps & GF_EPOC_HAS_AAC) || (ctx->caps & GF_EPOC_HAS_HEAAC) ) return GF_CODEC_SUPPORTED;
			default:
				break;
			}
			break;
		/*MPEG1 audio*/
		case GPAC_OTI_AUDIO_MPEG2_PART3:
		/*MPEG2 audio*/
		case GPAC_OTI_AUDIO_MPEG1:
			/* NOT SUPPORTED YET if (ctx->caps & GF_EPOC_HAS_MP3) return 1; */
			break;
		/*non-mpeg4 codecs*/
		case GPAC_OTI_MEDIA_GENERIC:
			if (!dsi) return GF_CODEC_NOT_SUPPORTED;
			if (esd->decoderConfig->decoderSpecificInfo->data < 4) return GF_CODEC_NOT_SUPPORTED;
			if (!strnicmp(dsi, "samr", 4) || !strnicmp(dsi, "amr ", 4)) {
				if (ctx->caps & GF_EPOC_HAS_AMR) return GF_CODEC_SUPPORTED;
			}
			if (!strnicmp(dsi, "sawb", 4)) {
				if (ctx->caps & GF_EPOC_HAS_AMR_WB) return GF_CODEC_SUPPORTED;
			}
			break;
		default:
			return GF_CODEC_NOT_SUPPORTED;
		}
	}
	return GF_CODEC_NOT_SUPPORTED;
}

#ifdef __cplusplus
extern "C" {
#endif

GF_BaseDecoder *EPOC_codec_new()
{
	GF_MediaDecoder *ifce;
	EPOCCodec *ctx;

	GF_SAFEALLOC(ifce, GF_MediaDecoder);
	GF_REGISTER_MODULE_INTERFACE(ifce, GF_MEDIA_DECODER_INTERFACE, "EPOC Native Decoder", "gpac distribution")

	GF_SAFEALLOC(ctx, EPOCCodec);
	ifce->privateStack = ctx;

	/*setup our own interface*/
	ifce->AttachStream = EDEC_AttachStream;
	ifce->DetachStream = EDEC_DetachStream;
	ifce->GetCapabilities = EDEC_GetCapabilities;
	ifce->SetCapabilities = EDEC_SetCapabilities;
	ifce->ProcessData = EDEC_ProcessData;
	ifce->CanHandleStream = EDEC_CanHandleStream;
	ifce->GetName = EDEC_GetCodecName;
	EDEC_LoadCaps((GF_BaseDecoder*)ifce);
	return (GF_BaseDecoder *) ifce;
}

void EPOC_codec_del(GF_BaseDecoder *ifcg)
{
	EPOCCodec *ctx = (EPOCCodec *)ifcg->privateStack;

	gf_free(ctx);
	gf_free(ifcg);
}


#ifdef __cplusplus
}
#endif
