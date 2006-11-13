/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
 *					All rights reserved
 *
 *  This file is part of GPAC / AMR decoder module
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



/*decoder Interface*/
#include <gpac/modules/codec.h>
#include <gpac/modules/service.h>
#include <gpac/constants.h>

#include "amr_nb/sp_dec.h"
#include "amr_nb/d_homing.h"


/*default size in CU of composition memory for audio*/
#define DEFAULT_AUDIO_CM_SIZE			12
/*default critical size in CU of composition memory for audio*/
#define DEFAULT_AUDIO_CM_TRIGGER		4

typedef struct
{
	u32 sample_rate, out_size, num_samples;
	u8 num_channels;
	/*no support for scalability in AMR*/
	u16 ES_ID;
	u32 cb_size, cb_trig;

	u32 frame_count;

	/*AMR NB state vars*/
	Speech_Decode_FrameState * speech_decoder_state;
    enum RXFrameType rx_type;
    enum Mode mode;
   
	s16 reset_flag, reset_flag_old;

} AMRDec;

#define AMRCTX() AMRDec *ctx = (AMRDec *) ifcg->privateStack


static GF_Err AMR_AttachStream(GF_BaseDecoder *ifcg, u16 ES_ID, char *decSpecInfo, u32 decSpecInfoSize, u16 DependsOnES_ID, u32 objectTypeIndication, Bool UpStream)
{
	GF_BitStream *bs;
	char name[5];
	u32 codec_4cc;
	AMRCTX();
	if (ctx->ES_ID && ctx->ES_ID!=ES_ID) return GF_NOT_SUPPORTED;

	bs = gf_bs_new(decSpecInfo, decSpecInfoSize, GF_BITSTREAM_READ);
	codec_4cc = gf_bs_read_u32(bs);
	gf_bs_del(bs);
	strcpy(name, gf_4cc_to_str(codec_4cc));
	
	if (stricmp(name, "samr") && stricmp(name, "amr ")) return GF_NOT_SUPPORTED;

	ctx->frame_count = 0;
	ctx->reset_flag = 0;
	ctx->reset_flag_old = 1;
	ctx->mode = 0;
	ctx->rx_type = 0;
	ctx->speech_decoder_state = NULL;
	if (Speech_Decode_Frame_init(&ctx->speech_decoder_state, "Decoder")) return GF_IO_ERR;
	ctx->num_channels = 1;
	ctx->num_samples = 160;
	ctx->sample_rate = 8000;
	
	/*max possible frames in a sample are seen in MP4, that's 15*/
	ctx->out_size = 15 * 2 * ctx->num_samples * ctx->num_channels;
	ctx->ES_ID = ES_ID;

	return GF_OK;
}

static GF_Err AMR_DetachStream(GF_BaseDecoder *ifcg, u16 ES_ID)
{
	AMRCTX();
	if (ES_ID != ctx->ES_ID) return GF_BAD_PARAM;
	Speech_Decode_Frame_exit(&ctx->speech_decoder_state);
	ctx->speech_decoder_state = NULL;
	ctx->ES_ID = 0;
	ctx->sample_rate = ctx->out_size = ctx->num_samples = 0;
	ctx->num_channels = 0;
	return GF_OK;
}
static GF_Err AMR_GetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability *capability)
{
	AMRCTX();
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
		capability->cap.valueInt = ctx->cb_trig;
		break;
	case GF_CODEC_BUFFER_MAX:
		capability->cap.valueInt = ctx->cb_size;
		break;
	/*FIXME: get exact sampling window*/
	case GF_CODEC_CU_DURATION:
		capability->cap.valueInt = ctx->num_samples;
		break;
	case GF_CODEC_PADDING_BYTES:
		capability->cap.valueInt = 4;
		break;
	case GF_CODEC_CHANNEL_CONFIG:
		if (ctx->num_channels==1) {
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

static GF_Err AMR_SetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability capability)
{
	/*return unsupported to avoid confusion by the player (like SR changing ...) */
	return GF_NOT_SUPPORTED;
}


/* frame size in serial bitstream file (frame type + serial stream + flags) */
#define SERIAL_FRAMESIZE (270)

static GF_Err AMR_ProcessData(GF_MediaDecoder *ifcg, 
		char *inBuffer, u32 inBufferLength,
		u16 ES_ID,
		char *outBuffer, u32 *outBufferLength,
		u8 PaddingBits, u32 mmlevel)
{
    u32 offset = 0;
    u8 toc, q, ft;
    s16 *synth;
    u8 *packed_bits;
    s16 serial[SERIAL_FRAMESIZE];
    s32 i;
	AMRCTX();

	/*not using scalabilty*/
	assert(ctx->ES_ID == ES_ID);

	/*if late or seeking don't decode (each frame is a RAP)*/
	/*	switch (mmlevel) {
	case GF_CODEC_LEVEL_SEEK:
	case GF_CODEC_LEVEL_DROP:
		*outBufferLength = 0;
		return GF_OK;
	default:
		break;
	}
	*/
	if (ctx->out_size > *outBufferLength) {
		*outBufferLength = ctx->out_size;
		return GF_BUFFER_TOO_SMALL;
	}
	
    synth = (s16 *) outBuffer;
	*outBufferLength = 0;

    while (inBufferLength) {
        toc = inBuffer[0];
        /* read rest of the frame based on ToC byte */
        q = (toc >> 2) & 0x01;
        ft = (toc >> 3) & 0x0F;

		packed_bits = inBuffer + 1;
		offset = 1 + GF_AMR_FRAME_SIZE[ft];

		/*Unsort and unpack bits*/
		ctx->rx_type = UnpackBits(q, ft, packed_bits, &ctx->mode, &serial[1]);
		ctx->frame_count++;

		if (ctx->rx_type == RX_NO_DATA) {
			ctx->mode = ctx->speech_decoder_state->prev_mode;
		} else {
			ctx->speech_decoder_state->prev_mode = ctx->mode;
		}
    
		/* if homed: check if this frame is another homing frame */
		if (ctx->reset_flag_old == 1) {
			/* only check until end of first subframe */
			ctx->reset_flag = decoder_homing_frame_test_first(&serial[1], ctx->mode);
		}
		/* produce encoder homing frame if homed & input=decoder homing frame */
		if ((ctx->reset_flag != 0) && (ctx->reset_flag_old != 0)) {
			for (i = 0; i < L_FRAME; i++) {
				synth[i] = EHF_MASK;
			}
		} else {     
			/* decode frame */
			Speech_Decode_Frame(ctx->speech_decoder_state, ctx->mode, &serial[1], ctx->rx_type, synth);
		}

		*outBufferLength += 160*2;
		synth += 160;
    
		/* if not homed: check whether current frame is a homing frame */
		if (ctx->reset_flag_old == 0) {
			ctx->reset_flag = decoder_homing_frame_test(&serial[1], ctx->mode);
		}
		/* reset decoder if current frame is a homing frame */
		if (ctx->reset_flag != 0) {
			Speech_Decode_Frame_reset(ctx->speech_decoder_state);
		}
		ctx->reset_flag_old = ctx->reset_flag;

		if (inBufferLength < offset) return GF_OK;
		inBufferLength -= offset;
		inBuffer += offset;

		if (*outBufferLength > ctx->out_size) return GF_NON_COMPLIANT_BITSTREAM;
    }
	return GF_OK;
}


static u32 AMR_CanHandleStream(GF_BaseDecoder *dec, u32 StreamType, u32 ObjectType, char *decSpecInfo, u32 decSpecInfoSize, u32 PL)
{
	GF_BitStream *bs;
	char name[5];
	u32 codec_4cc;

	/*we handle audio only*/
	if (!ObjectType) return (StreamType==GF_STREAM_AUDIO) ? 1 : 0;

	/*audio dec*/
	if (!decSpecInfo || (StreamType != GF_STREAM_AUDIO) || (ObjectType != GPAC_EXTRA_CODECS_OTI)) return 0;
	bs = gf_bs_new(decSpecInfo, decSpecInfoSize, GF_BITSTREAM_READ);
	codec_4cc = gf_bs_read_u32(bs);
	gf_bs_del(bs);

	strcpy(name, gf_4cc_to_str(codec_4cc));

	if (!stricmp(name, "samr") || !stricmp(name, "amr ")) return 1;
	return 0;
}


static const char *AMR_GetCodecName(GF_BaseDecoder *dec)
{
	return "3GPP Fixed-Point AMR NB";
}

GF_MediaDecoder *NewAMRDecoder()
{
	AMRDec *dec;
	GF_MediaDecoder *ifce;
	GF_SAFEALLOC(ifce , GF_MediaDecoder);
	dec = malloc(sizeof(AMRDec));
	memset(dec, 0, sizeof(AMRDec));
	ifce->privateStack = dec;
	ifce->CanHandleStream = AMR_CanHandleStream;

	dec->cb_size = DEFAULT_AUDIO_CM_SIZE;
	dec->cb_trig = DEFAULT_AUDIO_CM_TRIGGER;

	/*setup our own interface*/	
	ifce->AttachStream = AMR_AttachStream;
	ifce->DetachStream = AMR_DetachStream;
	ifce->GetCapabilities = AMR_GetCapabilities;
	ifce->SetCapabilities = AMR_SetCapabilities;
	ifce->ProcessData = AMR_ProcessData;
	ifce->GetName = AMR_GetCodecName;

	GF_REGISTER_MODULE_INTERFACE(ifce, GF_MEDIA_DECODER_INTERFACE, "AMR-NB 3GPP decoder", "gpac distribution");

	return ifce;
}

void DeleteAMRDecoder(GF_BaseDecoder *ifcg)
{
	AMRCTX();
	free(ctx);
	free(ifcg);
}

Bool QueryInterface(u32 InterfaceType)
{
	switch (InterfaceType) {
	case GF_MEDIA_DECODER_INTERFACE: return 1;
	case GF_NET_CLIENT_INTERFACE: return 1;
	default: return 0;
	}
}

GF_InputService *NewAESReader();
void DeleteAESReader(void *ifce);

GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	switch (InterfaceType) {
	case GF_MEDIA_DECODER_INTERFACE: return (GF_BaseInterface *) NewAMRDecoder();
	case GF_NET_CLIENT_INTERFACE: return (GF_BaseInterface *) NewAESReader();
	default: return NULL;
	}
}

void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_MEDIA_DECODER_INTERFACE: DeleteAMRDecoder((GF_BaseDecoder *)ifce); break;
	case GF_NET_CLIENT_INTERFACE:  DeleteAESReader(ifce); break;
	}
}
