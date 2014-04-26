/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2012
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

#ifndef GPAC_DISABLE_LASER

typedef struct
{
	GF_Scene *pScene;
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

GF_Err LSR_AttachScene(GF_SceneDecoder *plug, GF_Scene *scene, Bool is_scene_decoder)
{
	LSRPriv *priv = (LSRPriv *)plug->privateStack;
	if (priv->codec) return GF_BAD_PARAM;
	priv->pScene = scene;
	priv->app = scene->root_od->term;

	priv->codec = gf_laser_decoder_new(scene->graph);
	/*attach the clock*/
	gf_laser_decoder_set_clock(priv->codec, gf_scene_get_time, scene);
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

static GF_Err LSR_ProcessData(GF_SceneDecoder*plug, const char *inBuffer, u32 inBufferLength,
                              u16 ES_ID, u32 AU_time, u32 mmlevel)
{
	GF_Err e = GF_OK;
	LSRPriv *priv = (LSRPriv *)plug->privateStack;

	e = gf_laser_decode_au(priv->codec, ES_ID, inBuffer, inBufferLength);

	/*if scene not attached do it*/
	gf_scene_attach_to_compositor(priv->pScene);
	return e;
}

static u32 LSR_CanHandleStream(GF_BaseDecoder *ifce, u32 StreamType, GF_ESD *esd, u8 PL)
{
	if (StreamType!=GF_STREAM_SCENE) return GF_CODEC_NOT_SUPPORTED;
	/*media type query*/
	if (!esd) return GF_CODEC_STREAM_TYPE_SUPPORTED;
	if (esd->decoderConfig->objectTypeIndication == GPAC_OTI_SCENE_LASER) return GF_CODEC_SUPPORTED;
	return GF_CODEC_NOT_SUPPORTED;
}


void DeleteLSRDec(GF_BaseDecoder *plug)
{
	LSRPriv *priv;
	if (!plug)
		return;
	priv = (LSRPriv *)plug->privateStack;
	if (priv) {
		/*in case something went wrong*/
		if (priv->codec)
			gf_laser_decoder_del(priv->codec);
		priv->codec = NULL;
		gf_free(priv);
		plug->privateStack = NULL;
	}
	gf_free(plug);
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

#endif


GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
#ifndef GPAC_DISABLE_LASER
		GF_SCENE_DECODER_INTERFACE,
#endif
		0
	};
	return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	switch (InterfaceType) {
#ifndef GPAC_DISABLE_LASER
	case GF_SCENE_DECODER_INTERFACE:
		return (GF_BaseInterface *)NewLSRDec();
#endif
	default:
		return NULL;
	}
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
#ifndef GPAC_DISABLE_LASER
	case GF_SCENE_DECODER_INTERFACE:
		DeleteLSRDec((GF_BaseDecoder *)ifce);
		break;
#endif
	}
}


GPAC_MODULE_STATIC_DELARATION( laser )
