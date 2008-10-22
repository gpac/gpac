/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / BIFS decoder module
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

#include <gpac/internal/terminal_dev.h>
#include <gpac/bifs.h>
#include <gpac/constants.h>

typedef struct 
{
	GF_InlineScene *pScene;
	GF_Terminal *app;
	GF_BifsDecoder *codec;
	u32 PL, nb_streams;
} BIFSPriv;



static GF_Err BIFS_GetCapabilities(GF_BaseDecoder *plug, GF_CodecCapability *cap)
{
	cap->cap.valueInt = 0;
	return GF_NOT_SUPPORTED;
}

static GF_Err BIFS_SetCapabilities(GF_BaseDecoder *plug, const GF_CodecCapability capability)
{
	return GF_OK;
}

GF_Err BIFS_AttachScene(GF_SceneDecoder *plug, GF_InlineScene *scene, Bool is_scene_decoder)
{
	BIFSPriv *priv = (BIFSPriv *)plug->privateStack;
	if (priv->codec) return GF_BAD_PARAM;
	priv->pScene = scene;
	priv->app = scene->root_od->term;
	
	priv->codec = gf_bifs_decoder_new(scene->graph, 0);
	/*ignore all size info on anim streams*/
	if (!is_scene_decoder) gf_bifs_decoder_ignore_size_info(priv->codec);
	return GF_OK;
}

GF_Err BIFS_ReleaseScene(GF_SceneDecoder *plug)
{
	BIFSPriv *priv = (BIFSPriv *)plug->privateStack;
	if (!priv->codec || priv->nb_streams) return GF_BAD_PARAM;
	gf_bifs_decoder_del(priv->codec);
	priv->codec = NULL;
	return GF_OK;
}

static GF_Err BIFS_AttachStream(GF_BaseDecoder *plug, GF_ESD *esd)
{
	BIFSPriv *priv = (BIFSPriv *)plug->privateStack;
	GF_Err e;
	if (esd->decoderConfig->upstream) return GF_NOT_SUPPORTED;
	if (!esd->decoderConfig->decoderSpecificInfo) return GF_BAD_PARAM;
	e = gf_bifs_decoder_configure_stream(priv->codec, esd->ESID, esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, esd->decoderConfig->objectTypeIndication);
	if (!e) priv->nb_streams++;
	return e;
}

static GF_Err BIFS_DetachStream(GF_BaseDecoder *plug, u16 ES_ID)
{
	GF_Err e;
	BIFSPriv *priv = (BIFSPriv *)plug->privateStack;
	e = gf_bifs_decoder_remove_stream(priv->codec, ES_ID);
	if (e) return e;
	priv->nb_streams--;
	return GF_OK;
}

static GF_Err BIFS_ProcessData(GF_SceneDecoder*plug, char *inBuffer, u32 inBufferLength, 
								u16 ES_ID, u32 AU_time, u32 mmlevel)
{
	GF_Err e = GF_OK;
	BIFSPriv *priv = (BIFSPriv *)plug->privateStack;

	e = gf_bifs_decode_au(priv->codec, ES_ID, inBuffer, inBufferLength, ((Double)AU_time)/1000.0);

	/*if scene not attached do it*/
	gf_inline_attach_to_compositor(priv->pScene);
	return e;
}

Bool BIFS_CanHandleStream(GF_BaseDecoder *ifce, u32 StreamType, u32 ObjectType, char *decSpecInfo, u32 decSpecInfoSize, u32 PL)
{
	BIFSPriv *priv = (BIFSPriv *)ifce->privateStack;
	if (StreamType!=GF_STREAM_SCENE) return 0;
	switch (ObjectType) {
	case 0x00:
		return 1;
	case GPAC_OTI_SCENE_BIFS:
	case GPAC_OTI_SCENE_BIFS_V2:
	/*Streams with this value with a StreamType indicating a systems stream (values 1,2,3, 6, 7, 8, 9) 
		shall be treated as if the ObjectTypeIndication had been set to 0x01*/
	case 0xFF:	
		priv->PL = PL;
		return 1;
	default:
		return 0;
	}
}


void DeleteBIFSDec(GF_BaseDecoder *plug)
{
	BIFSPriv *priv = (BIFSPriv *)plug->privateStack;
	/*in case something went wrong*/
	if (priv->codec) gf_bifs_decoder_del(priv->codec);
	free(priv);
	free(plug);
}

GF_BaseDecoder *NewBIFSDec()
{
	BIFSPriv *priv;
	GF_SceneDecoder *tmp;
	
	GF_SAFEALLOC(tmp, GF_SceneDecoder);
	if (!tmp) return NULL;
	GF_SAFEALLOC(priv, BIFSPriv);
	priv->codec = NULL;
	tmp->privateStack = priv;
	tmp->AttachStream = BIFS_AttachStream;
	tmp->DetachStream = BIFS_DetachStream;
	tmp->GetCapabilities = BIFS_GetCapabilities;
	tmp->SetCapabilities = BIFS_SetCapabilities;
	tmp->ProcessData = BIFS_ProcessData;
	tmp->AttachScene = BIFS_AttachScene;
	tmp->CanHandleStream = BIFS_CanHandleStream;
	tmp->ReleaseScene = BIFS_ReleaseScene;
	GF_REGISTER_MODULE_INTERFACE(tmp, GF_SCENE_DECODER_INTERFACE, "GPAC BIFS Decoder", "gpac distribution")
	return (GF_BaseDecoder *) tmp;
}

GF_EXPORT
Bool QueryInterface(u32 InterfaceType)
{
	switch (InterfaceType) {
	case GF_SCENE_DECODER_INTERFACE:
		return 1;
	default:
		return 0;
	}
}

GF_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	switch (InterfaceType) {
	case GF_SCENE_DECODER_INTERFACE:
		return (GF_BaseInterface *)NewBIFSDec();
	default:
		return NULL;
	}
}

GF_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_SCENE_DECODER_INTERFACE:
		DeleteBIFSDec((GF_BaseDecoder *)ifce);
		break;
	}
}
