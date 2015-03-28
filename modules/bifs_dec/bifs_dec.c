/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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

#ifndef GPAC_DISABLE_BIFS

typedef struct
{
	GF_Scene *pScene;
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

GF_Err BIFS_AttachScene(GF_SceneDecoder *plug, GF_Scene *scene, Bool is_scene_decoder)
{
	BIFSPriv *priv = (BIFSPriv *)plug->privateStack;
	if (priv->codec) return GF_BAD_PARAM;
	priv->pScene = scene;
	priv->app = scene->root_od->term;

	priv->codec = gf_bifs_decoder_new(scene->graph, 0);
	gf_bifs_decoder_set_extraction_path(priv->codec, (char *) gf_modules_get_option((GF_BaseInterface *)plug, "General", "CacheDirectory"), scene->root_od->net_service->url);
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

static GF_Err BIFS_ProcessData(GF_SceneDecoder*plug, const char *inBuffer, u32 inBufferLength,
                               u16 ES_ID, u32 AU_time, u32 mmlevel)
{
	Double ts_offset;
	s32 time;
	GF_Err e = GF_OK;
	BIFSPriv *priv = (BIFSPriv *)plug->privateStack;

	time = (s32) AU_time;
	ts_offset = ((Double)time)/1000.0;
	e = gf_bifs_decode_au(priv->codec, ES_ID, inBuffer, inBufferLength, ts_offset);

	/*if scene not attached do it*/
	if (e == GF_OK) {
		gf_scene_attach_to_compositor(priv->pScene);
	}
	return e;
}

static u32 BIFS_CanHandleStream(GF_BaseDecoder *ifce, u32 StreamType, GF_ESD *esd, u8 PL)
{
	BIFSPriv *priv = (BIFSPriv *)ifce->privateStack;
	if (StreamType!=GF_STREAM_SCENE) return GF_CODEC_NOT_SUPPORTED;
	/*media type query*/
	if (!esd) return GF_CODEC_STREAM_TYPE_SUPPORTED;

	switch (esd->decoderConfig->objectTypeIndication) {
	case GPAC_OTI_SCENE_BIFS:
	case GPAC_OTI_SCENE_BIFS_V2:
	/*Streams with this value with a StreamType indicating a systems stream (values 1,2,3, 6, 7, 8, 9)
		shall be treated as if the ObjectTypeIndication had been set to 0x01*/
	case 0xFF:
		priv->PL = PL;
		return GF_CODEC_SUPPORTED;
	default:
		return GF_CODEC_NOT_SUPPORTED;
	}
}


void DeleteBIFSDec(GF_BaseDecoder *plug)
{
	BIFSPriv *priv;
	if (!plug)
		return;
	priv = (BIFSPriv *)plug->privateStack;
	if (priv) {
		/*in case something went wrong*/
		if (priv->codec) gf_bifs_decoder_del(priv->codec);
		priv->codec = NULL;
		gf_free(priv);
		plug->privateStack = NULL;
	}
	gf_free(plug);
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


#endif /*GPAC_DISABLE_BIFS*/


GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
#ifndef GPAC_DISABLE_BIFS
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
#ifndef GPAC_DISABLE_BIFS
	case GF_SCENE_DECODER_INTERFACE:
		return (GF_BaseInterface *)NewBIFSDec();
#endif
	default:
		return NULL;
	}
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
#ifndef GPAC_DISABLE_BIFS
	case GF_SCENE_DECODER_INTERFACE:
		DeleteBIFSDec((GF_BaseDecoder *)ifce);
		break;
#endif
	}
}

GPAC_MODULE_STATIC_DECLARATION( bifs )
