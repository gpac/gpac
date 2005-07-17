/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
 *					All rights reserved
 *
 *  This file is part of GPAC / OD decoder module
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
#include <gpac/constants.h>

typedef struct 
{
	u16 *ESIDs;
	u32 streamCount;
	GF_InlineScene *scene;
	u32 PL;
} ODPriv;

static GF_Err ODF_GetCapabilities(GF_BaseDecoder *plug, GF_CodecCapability *cap)
{
	cap->cap.valueInt = 0;
	return GF_NOT_SUPPORTED;
}

static GF_Err ODF_SetCapabilities(GF_BaseDecoder *plug, const GF_CodecCapability capability)
{
	return GF_OK;
}


static GF_Err ODF_AttachScene(GF_SceneDecoder *plug, GF_InlineScene *scene, Bool is_inline_scene)
{
	ODPriv *priv = plug->privateStack;
	if (priv->scene) return GF_BAD_PARAM;
	priv->scene = scene;
	return GF_OK;
}

static void ODS_SetupOD(GF_InlineScene *is, GF_ObjectDescriptor *od)
{
	GF_ObjectManager *odm, *parent;
	odm = gf_is_find_odm(is, od->objectDescriptorID);
	/*remove the old OD*/
	if (odm) gf_odm_disconnect(odm, 1);
	odm = gf_odm_new();
	odm->OD = od;
	odm->term = is->root_od->term;
	odm->parentscene = is;
	gf_list_add(is->ODlist, odm);

	/*locate service owner*/
	parent = is->root_od;
	while (parent->remote_OD) parent = parent->remote_OD;
	gf_odm_setup_object(odm, parent->net_service);
}

static GF_Err ODS_ODUpdate(ODPriv *priv, GF_ODUpdate *odU)
{
	GF_ObjectDescriptor *od;
	u32 count;

	/*extract all our ODs and compare with what we already have...*/
	count = gf_list_count(odU->objectDescriptors);
	if (count > 255) return GF_ODF_INVALID_DESCRIPTOR;

	while (count) {
		od = gf_list_get(odU->objectDescriptors, 0);
		gf_list_rem(odU->objectDescriptors, 0);
		count--;
		ODS_SetupOD(priv->scene, od);
	}
	return GF_OK;
}



static GF_Err ODS_RemoveOD(ODPriv *priv, GF_ODRemove *odR)
{
	u32 i;
	for (i=0; i< odR->NbODs; i++) {
		GF_ObjectManager *odm = gf_is_find_odm(priv->scene, odR->OD_ID[i]);
		if (odm) gf_odm_disconnect(odm, 1);
	}
	return GF_OK;
}

static GF_Err ODS_UpdateESD(ODPriv *priv, GF_ESDUpdate *ESDs)
{
	GF_ESD *esd, *prev;
	GF_ObjectManager *odm;
	u32 count, i;

	odm = gf_is_find_odm(priv->scene, ESDs->ODID);
	/*spec: "ignore"*/
	if (!odm) return GF_OK;
	/*spec: "ES_DescriptorUpdate shall not be applied on object descriptors that have set URL_Flag to '1' (see 8.6.3)."*/
	if (odm->remote_OD) return GF_NON_COMPLIANT_BITSTREAM;

	count = gf_list_count(ESDs->ESDescriptors);

	while (count) {
		esd = gf_list_get(ESDs->ESDescriptors, 0);
		/*spec: "ES_Descriptors with ES_IDs that have already been received within the same name scope shall be ignored."*/
		prev = NULL;
		for (i=0; i<gf_list_count(odm->OD->ESDescriptors); i++) {
			prev = gf_list_get(odm->OD->ESDescriptors, i);
			if (prev->ESID == esd->ESID) break;
			prev = NULL;
		}

		if (prev) {
			gf_odf_desc_del((GF_Descriptor *)esd);
		} else {
			/*and register new stream*/
			gf_list_add(odm->OD->ESDescriptors, esd);
			gf_odm_setup_es(odm, esd, odm->net_service);
		}

		/*remove the desc from the AU*/
		gf_list_rem(ESDs->ESDescriptors, 0);
		count--;
	}
	/*resetup object since a new ES has been inserted 
	(typically an empty object first sent, then a stream added - cf how ogg demuxer works)*/
	gf_is_setup_object(priv->scene, odm);
	return GF_OK;
}


static GF_Err ODS_RemoveESD(ODPriv *priv, GF_ESDRemove *ESDs)
{
	GF_ObjectManager *odm;
	u32 i;
	odm = gf_is_find_odm(priv->scene, ESDs->ODID);
	/*spec: "ignore"*/
	if (!odm) return GF_OK;
	/*spec: "ES_DescriptorRemove shall not be applied on object descriptors that have set URL_Flag to '1' (see 8.6.3)."*/
	if (odm->remote_OD) return GF_NON_COMPLIANT_BITSTREAM;

	for (i=0; i<ESDs->NbESDs; i++) {
		/*blindly call remove*/
		gf_odm_remove_es(odm, ESDs->ES_ID[i]);
	}
	return GF_OK;
}

static GF_Err ODF_AttachStream(GF_BaseDecoder *plug, 
									 u16 ES_ID, 
									 unsigned char *decSpecInfo, 
									 u32 decSpecInfoSize, 
									 u16 DependsOnES_ID,
									 u32 objectTypeIndication, 
									 Bool Upstream)
{
	ODPriv *priv = plug->privateStack;
	
	if (Upstream) return GF_OK;
	//warning, we only support one stream ...
	if (priv->streamCount) return GF_NOT_SUPPORTED;
	priv->ESIDs = malloc(sizeof(u16));
	priv->ESIDs[0] = ES_ID;
	priv->streamCount ++;
	//that's it, OD doesn't use decoderSpecificInfo
	return GF_OK;
}


static GF_Err ODF_DetachStream(GF_BaseDecoder *plug, u16 ES_ID)
{
	ODPriv *priv = plug->privateStack;

	//warning, we only support one stream ...
	if (!priv->streamCount) return GF_BAD_PARAM;
	free(priv->ESIDs);
	priv->ESIDs = NULL;
	priv->streamCount = 0;

	return GF_OK;
}


static GF_Err ODF_ProcessData(GF_SceneDecoder *plug, unsigned char *inBuffer, u32 inBufferLength, 
							u16 ES_ID, u32 AU_time, u32 mmlevel)
{
	GF_Err e;
	GF_ODCom *com;
	GF_ODCodec *oddec;
	ODPriv *priv = plug->privateStack;
	
	oddec = gf_odf_codec_new();

	e = gf_odf_codec_set_au(oddec, inBuffer, inBufferLength);
	if (!e)  e = gf_odf_codec_decode(oddec);
	if (e) goto err_exit;

	//3- process all the commands in this AU, in order
	while (1) {
		com = gf_odf_codec_get_com(oddec);
		if (!com) break;

		//ok, we have a command
		switch (com->tag) {
		case GF_ODF_OD_UPDATE_TAG:
			e = ODS_ODUpdate(priv, (GF_ODUpdate *) com);
			break;
		case GF_ODF_OD_REMOVE_TAG:
			e = ODS_RemoveOD(priv, (GF_ODRemove *) com);
			break;
		case GF_ODF_ESD_UPDATE_TAG:
			e = ODS_UpdateESD(priv, (GF_ESDUpdate *)com);
			break;
		case GF_ODF_ESD_REMOVE_TAG:
			e = ODS_RemoveESD(priv, (GF_ESDRemove *)com);
			break;
		case GF_ODF_IPMP_UPDATE_TAG:
#if 0
		{
			GF_IPMPUpdate *ipmpU = (GF_IPMPUpdate *)com;
			while (gf_list_count(ipmpU->IPMPDescList)) {
				GF_IPMP_Descriptor *ipmp = gf_list_get(ipmpU->IPMPDescList, 0);
				gf_list_rem(ipmpU->IPMPDescList, 0);
				IS_UpdateIPMP(priv->scene, ipmp);
			}
			e = GF_OK;
		}
#else
			e = GF_NOT_SUPPORTED;
#endif
			break;
		case GF_ODF_IPMP_REMOVE_TAG:
			e = GF_NOT_SUPPORTED;
			break;
		/*should NEVER exist outside the file format*/
		case GF_ODF_ESD_REMOVE_REF_TAG:
			e = GF_NON_COMPLIANT_BITSTREAM;
			break;
		default:
			if (com->tag >= GF_ODF_COM_ISO_BEGIN_TAG && com->tag <= GF_ODF_COM_ISO_END_TAG) {
				e = GF_ODF_FORBIDDEN_DESCRIPTOR;
			} else {
				/*we don't process user commands*/
				e = GF_OK;
			}
			break;
		}
		gf_odf_com_del(&com);
		if (e) break;
	}

err_exit:
	gf_odf_codec_del(oddec);
	return e;
}


Bool ODF_CanHandleStream(GF_BaseDecoder *ifce, u32 StreamType, u32 ObjectType, unsigned char *decSpecInfo, u32 decSpecInfoSize, u32 PL)
{
	ODPriv *priv = ifce->privateStack;
	if (StreamType==GF_STREAM_OD) {
		priv->PL = PL;
		return 1;
	}
	return 0;
}


void DeleteODDec(GF_BaseDecoder *plug)
{
	ODPriv *priv = plug->privateStack;
	if (priv->ESIDs) free(priv->ESIDs);
	free(priv);
	free(plug);
}

GF_BaseDecoder *NewODDec()
{
	GF_SceneDecoder *tmp;
	ODPriv *priv;
	
	GF_SAFEALLOC(tmp, sizeof(GF_SceneDecoder));
	if (!tmp) return NULL;
	GF_SAFEALLOC(priv, sizeof(ODPriv));

	tmp->privateStack = priv;
	tmp->AttachStream = ODF_AttachStream;
	tmp->DetachStream = ODF_DetachStream;
	tmp->GetCapabilities = ODF_GetCapabilities;
	tmp->SetCapabilities = ODF_SetCapabilities;
	tmp->ProcessData = ODF_ProcessData;
	tmp->AttachScene = ODF_AttachScene;
	tmp->CanHandleStream = ODF_CanHandleStream;

	GF_REGISTER_MODULE_INTERFACE(tmp, GF_SCENE_DECODER_INTERFACE, "GPAC OD Decoder", "gpac distribution")
	return (GF_BaseDecoder *) tmp;
}


Bool QueryInterface(u32 InterfaceType)
{
	switch (InterfaceType) {
	case GF_SCENE_DECODER_INTERFACE:
		return 1;
	default:
		return 0;
	}
}

GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	switch (InterfaceType) {
	case GF_SCENE_DECODER_INTERFACE:
		return (GF_BaseInterface *)NewODDec();
	default:
		return NULL;
	}
}

void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_SCENE_DECODER_INTERFACE:
		DeleteODDec((GF_BaseDecoder *)ifce);
		break;
	}
}

