/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean le Feuvre
 *				Copyright (c) 2005-200X ENST
 *					All rights reserved
 *
 *  This file is part of GPAC / LASeR decoder module
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
#include <gpac/laser.h>
#include <gpac/constants.h>

#ifndef GPAC_DISABLE_SVG

typedef struct 
{
	GF_InlineScene *pScene;
	GF_Terminal *app;
	GF_LASeRCodec *codec;
	u32 PL, nb_streams;
} LSRPriv;



static GF_Err LSR_GetCapabilities(GF_BaseDecoder *plug, GF_CodecCapability *cap)
{
	cap->cap.valueInt = 0;
	return GF_NOT_SUPPORTED;
}

static GF_Err LSR_SetCapabilities(GF_BaseDecoder *plug, const GF_CodecCapability capability)
{
	return GF_OK;
}

GF_Err LSR_AttachScene(GF_SceneDecoder *plug, GF_InlineScene *scene, Bool is_scene_decoder)
{
	LSRPriv *priv = (LSRPriv *)plug->privateStack;
	if (priv->codec) return GF_BAD_PARAM;
	priv->pScene = scene;
	priv->app = scene->root_od->term;
	
	priv->codec = gf_laser_decoder_new(scene->graph);
	/*attach the clock*/
	gf_laser_decoder_set_clock(priv->codec, gf_inline_get_time, scene);
	return GF_OK;
}

GF_Err LSR_ReleaseScene(GF_SceneDecoder *plug)
{
	LSRPriv *priv = (LSRPriv *)plug->privateStack;
	if (!priv->codec || priv->nb_streams) return GF_BAD_PARAM;
	gf_laser_decoder_del(priv->codec);
	priv->codec = NULL;
	return GF_OK;
}

static GF_Err LSR_AttachStream(GF_BaseDecoder *plug, GF_ESD *esd)
{
	LSRPriv *priv = (LSRPriv *)plug->privateStack;
	GF_Err e;
	if (esd->decoderConfig->upstream) return GF_NOT_SUPPORTED;
	e = gf_laser_decoder_configure_stream(priv->codec, esd->ESID, esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength);
	if (!e) priv->nb_streams++;
	return e;
}

static GF_Err LSR_DetachStream(GF_BaseDecoder *plug, u16 ES_ID)
{
	GF_Err e;
	LSRPriv *priv = (LSRPriv *)plug->privateStack;
	e = gf_laser_decoder_remove_stream(priv->codec, ES_ID);
	if (e) return e;
	priv->nb_streams--;
	return GF_OK;
}

static GF_Err LSR_ProcessData(GF_SceneDecoder*plug, char *inBuffer, u32 inBufferLength, 
								u16 ES_ID, u32 AU_time, u32 mmlevel)
{
	GF_Err e = GF_OK;
	LSRPriv *priv = (LSRPriv *)plug->privateStack;

	e = gf_laser_decode_au(priv->codec, ES_ID, inBuffer, inBufferLength);

	/*if scene not attached do it*/
	gf_inline_attach_to_compositor(priv->pScene);
	return e;
}

Bool LSR_CanHandleStream(GF_BaseDecoder *ifce, u32 StreamType, u32 ObjectType, char *decSpecInfo, u32 decSpecInfoSize, u32 PL)
{
	if ((StreamType==GF_STREAM_SCENE) && (ObjectType == GPAC_OTI_SCENE_LASER)) return 1;
	return 0;
}


void DeleteLSRDec(GF_BaseDecoder *plug)
{
	LSRPriv *priv = (LSRPriv *)plug->privateStack;
	/*in case something went wrong*/
	if (priv->codec) gf_laser_decoder_del(priv->codec);
	free(priv);
	free(plug);
}

GF_BaseDecoder *NewLSRDec()
{
	LSRPriv *priv;
	GF_SceneDecoder *tmp;
	
	GF_SAFEALLOC(tmp, GF_SceneDecoder);
	if (!tmp) return NULL;
	GF_SAFEALLOC(priv, LSRPriv);
	priv->codec = NULL;
	tmp->privateStack = priv;
	tmp->AttachStream = LSR_AttachStream;
	tmp->DetachStream = LSR_DetachStream;
	tmp->GetCapabilities = LSR_GetCapabilities;
	tmp->SetCapabilities = LSR_SetCapabilities;
	tmp->ProcessData = LSR_ProcessData;
	tmp->AttachScene = LSR_AttachScene;
	tmp->CanHandleStream = LSR_CanHandleStream;
	tmp->ReleaseScene = LSR_ReleaseScene;
	GF_REGISTER_MODULE_INTERFACE(tmp, GF_SCENE_DECODER_INTERFACE, "GPAC LASeR Decoder", "gpac distribution")
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
		return (GF_BaseInterface *)NewLSRDec();
	default:
		return NULL;
	}
}

GF_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_SCENE_DECODER_INTERFACE:
		DeleteLSRDec((GF_BaseDecoder *)ifce);
		break;
	}
}

#else

GF_EXPORT
Bool QueryInterface(u32 InterfaceType)
{
	return 0;
}

GF_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	return NULL;
}

GF_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
}
#endif
