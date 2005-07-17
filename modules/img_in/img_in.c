/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
 *					All rights reserved
 *
 *  This file is part of GPAC / image format module
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

#include "img_in.h"

enum
{
	IMG_JPEG = 1,
	IMG_PNG,
	IMG_BMP,
};

typedef struct
{
	GF_ClientService *service;
	/*service*/
	u32 srv_type;
	
	FILE *stream;
	u32 img_type;

	u32 pad_bytes;
	Bool es_done, od_done;
	LPNETCHANNEL es_ch, od_ch;

	char *es_data;
	u32 es_data_size;

	char *od_data;
	u32 od_data_size;


	SLHeader sl_hdr;

	/*file downloader*/
	GF_DownloadSession * dnload;
} IMGLoader;


static Bool IMG_CanHandleURL(GF_InputService *plug, const char *url)
{
	char *sExt;
	sExt = strrchr(url, '.');
	if (!sExt) return 0;
	if (gf_term_check_extension(plug, "image/jpeg", "jpeg jpg", "JPEG Images", sExt)) return 1;
	if (gf_term_check_extension(plug, "image/png", "png", "PNG Images", sExt)) return 1;
	if (gf_term_check_extension(plug, "image/bmp", "bmp", "MS Bitmap Images", sExt)) return 1;
	return 0;
}

static Bool jp_is_local(const char *url)
{
	if (!strnicmp(url, "file://", 7)) return 1;
	if (strstr(url, "://")) return 0;
	return 1;
}


void IMG_OnData(void *cbk, char *data, u32 data_size, u32 status, GF_Err e)
{
	const char *szCache;
	IMGLoader *jpl = (IMGLoader *) cbk;

	/*handle service message*/
	gf_term_download_update_stats(jpl->dnload);

	/*wait to get the whole file*/
	if (e == GF_OK) return;
	else if (e==GF_EOS) {
		szCache = gf_dm_sess_get_cache_name(jpl->dnload);
		if (!szCache) e = GF_IO_ERR;
		else {
			jpl->stream = fopen((char *) szCache, "rb");
			if (!jpl->stream) e = GF_SERVICE_ERROR;
			else {
				e = GF_OK;
				fseek(jpl->stream, 0, SEEK_END);
				jpl->es_data_size = ftell(jpl->stream);
				fseek(jpl->stream, 0, SEEK_SET);
			}
		}
	} 
	/*OK confirm*/
	gf_term_on_connect(jpl->service, NULL, e);
}

void jp_download_file(GF_InputService *plug, char *url)
{
	IMGLoader *jpl = (IMGLoader *) plug->priv;

	jpl->dnload = gf_term_download_new(jpl->service, url, 0, IMG_OnData, jpl);
	if (!jpl->dnload) gf_term_on_connect(jpl->service, NULL, GF_NOT_SUPPORTED);
	/*service confirm is done once fetched*/
}

static GF_Err IMG_ConnectService(GF_InputService *plug, GF_ClientService *serv, const char *url)
{
	char *sExt;
	IMGLoader *jpl = plug->priv;

	jpl->service = serv;

	sExt = strrchr(url, '.');
	if (!stricmp(sExt, ".jpeg") || !stricmp(sExt, ".jpg")) jpl->img_type = IMG_JPEG;
	else if (!stricmp(sExt, ".png")) jpl->img_type = IMG_PNG;
	else if (!stricmp(sExt, ".bmp")) jpl->img_type = IMG_BMP;

	if (jpl->dnload) gf_term_download_del(jpl->dnload);
	jpl->dnload = NULL;

	/*remote fetch*/
	if (!jp_is_local(url)) {
		jp_download_file(plug, (char *) url);
		return GF_OK;
	}

	jpl->stream = fopen(url, "rb");
	if (jpl->stream) {
		fseek(jpl->stream, 0, SEEK_END);
		jpl->es_data_size = ftell(jpl->stream);
		fseek(jpl->stream, 0, SEEK_SET);
	}
	gf_term_on_connect(serv, NULL, jpl->stream ? GF_OK : GF_URL_ERROR);
	return GF_OK;
}

static GF_Err IMG_CloseService(GF_InputService *plug)
{
	IMGLoader *jpl = plug->priv;
	if (jpl->stream) fclose(jpl->stream);
	jpl->stream = NULL;
	if (jpl->dnload) gf_term_download_del(jpl->dnload);
	jpl->dnload = NULL;

	gf_term_on_disconnect(jpl->service, NULL, GF_OK);
	return GF_OK;
}

static GF_Descriptor *IMG_GetServiceDesc(GF_InputService *plug, u32 expect_type, const char *sub_url)
{
	GF_ESD *esd;
	IMGLoader *jpl = plug->priv;
	GF_ObjectDescriptor *od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);

	jpl->srv_type = expect_type;

	od->objectDescriptorID = 1;
	/*visual object*/
	if (expect_type==GF_MEDIA_OBJECT_VIDEO) {
		esd = gf_odf_desc_esd_new(0);
		esd->slConfig->timestampResolution = 1000;
		esd->decoderConfig->streamType = GF_STREAM_VISUAL;
		if (jpl->img_type == IMG_JPEG) esd->decoderConfig->objectTypeIndication = 0x6c;
		else if (jpl->img_type == IMG_PNG) esd->decoderConfig->objectTypeIndication = 0x6d;
		else if (jpl->img_type == IMG_BMP) esd->decoderConfig->objectTypeIndication = GPAC_BMP_OTI;
		gf_list_add(od->ESDescriptors, esd);
		esd->ESID = 3;

		return (GF_Descriptor *) od;
	}
	/*inline scene*/
	/*OD ESD*/
	esd = gf_odf_desc_esd_new(0);
	esd->slConfig->timestampResolution = 1000;
	esd->decoderConfig->streamType = GF_STREAM_OD;
	esd->decoderConfig->objectTypeIndication = GPAC_STATIC_OD_OTI;
	esd->ESID = 1;
	gf_list_add(od->ESDescriptors, esd);
	return (GF_Descriptor *) od;
}

static GF_Err IMG_ConnectChannel(GF_InputService *plug, LPNETCHANNEL channel, const char *url, Bool upstream)
{
	u32 ES_ID;
	GF_Err e;
	IMGLoader *jpl = plug->priv;

	e = GF_SERVICE_ERROR;
	if ((jpl->es_ch==channel) || (jpl->od_ch==channel)) goto exit;

	e = GF_OK;
	if (strstr(url, "ES_ID")) {
		sscanf(url, "ES_ID=%d", &ES_ID);
	}
	/*URL setup*/
	else if (!jpl->es_ch && IMG_CanHandleURL(plug, url)) ES_ID = 3;

	switch (ES_ID) {
	case 1:
		jpl->od_ch = channel;
		break;
	case 3:
		jpl->es_ch = channel;
		break;
	}

exit:
	gf_term_on_connect(jpl->service, channel, e);
	return e;
}

static GF_Err IMG_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
	IMGLoader *jpl = plug->priv;

	if (jpl->es_ch == channel) {
		jpl->es_ch = NULL;
	} else if (jpl->od_ch == channel) {
		jpl->od_ch = NULL;
	}
	gf_term_on_disconnect(jpl->service, channel, GF_OK);
	return GF_OK;
}

static GF_Err IMG_ServiceCommand(GF_InputService *plug, GF_NetworkCommand *com)
{
	IMGLoader *jpl = plug->priv;

	if (!com->base.on_channel) return GF_NOT_SUPPORTED;
	switch (com->command_type) {
	case GF_NET_CHAN_SET_PADDING:
		jpl->pad_bytes = com->pad.padding_bytes;
		return GF_OK;
	case GF_NET_CHAN_DURATION:
		com->duration.duration = 0;
		return GF_OK;
	case GF_NET_CHAN_PLAY:
		/*note we don't handle range since we're only dealing with images*/
		if (jpl->es_ch == com->base.on_channel) { jpl->es_done = 0; }
		else if (jpl->od_ch == com->base.on_channel) { jpl->od_done = 0; }
		return GF_OK;
	case GF_NET_CHAN_STOP:
		return GF_OK;
	default:
		return GF_OK;
	}
}


static GF_Err IMG_ChannelGetSLP(GF_InputService *plug, LPNETCHANNEL channel, char **out_data_ptr, u32 *out_data_size, SLHeader *out_sl_hdr, Bool *sl_compressed, GF_Err *out_reception_status, Bool *is_new_data)
{
	IMGLoader *jpl = plug->priv;

	*out_reception_status = GF_OK;
	*sl_compressed = 0;
	*is_new_data = 0;

	memset(&jpl->sl_hdr, 0, sizeof(SLHeader));
	jpl->sl_hdr.randomAccessPointFlag = 1;
	jpl->sl_hdr.compositionTimeStampFlag = 1;
	*out_sl_hdr = jpl->sl_hdr;

	/*fetching es data*/
	if (jpl->es_ch == channel) {
		if (jpl->es_done) {
			*out_reception_status = GF_EOS;
			return GF_OK;
		}
		if (!jpl->es_data) {
			if (!jpl->stream) {
				*out_data_ptr = NULL;
				*out_data_size = 0;
				return GF_OK;
			}
			*is_new_data = 1;
			fseek(jpl->stream, 0, SEEK_SET);
			jpl->es_data = malloc(sizeof(char) * (jpl->es_data_size + jpl->pad_bytes));
			fread(jpl->es_data, sizeof(char) * jpl->es_data_size, 1, jpl->stream);
			fseek(jpl->stream, 0, SEEK_SET);
			if (jpl->pad_bytes) memset(jpl->es_data + jpl->es_data_size, 0, sizeof(char) * jpl->pad_bytes);

		}
		*out_data_ptr = jpl->es_data;
		*out_data_size = jpl->es_data_size;
		return GF_OK;
	}
	if (jpl->od_ch == channel) {
		GF_ODCodec *codec;
		GF_ObjectDescriptor *od;
		GF_ODUpdate *odU;
		GF_ESD *esd;
		if (jpl->od_done) {
			*out_reception_status = GF_EOS;
			return GF_OK;
		}
		if (!jpl->od_data) {
			*is_new_data = 1;
			odU = (GF_ODUpdate *) gf_odf_com_new(GF_ODF_OD_UPDATE_TAG);
			od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
			od->objectDescriptorID = 3;
			esd = gf_odf_desc_esd_new(0);
			esd->slConfig->timestampResolution = 1000;
			esd->ESID = 3;
			esd->decoderConfig->streamType = GF_STREAM_VISUAL;
			if (jpl->img_type==IMG_JPEG) esd->decoderConfig->objectTypeIndication = 0x6c;
			else if (jpl->img_type==IMG_PNG) esd->decoderConfig->objectTypeIndication = 0x6d;
			else if (jpl->img_type==IMG_BMP) esd->decoderConfig->objectTypeIndication = GPAC_BMP_OTI;
			gf_list_add(od->ESDescriptors, esd);
			gf_list_add(odU->objectDescriptors, od);
			codec = gf_odf_codec_new();
			gf_odf_codec_add_com(codec, (GF_ODCom *)odU);
			gf_odf_codec_encode(codec);
			gf_odf_codec_get_au(codec, &jpl->od_data, &jpl->od_data_size);
			gf_odf_codec_del(codec);
		}
		*out_data_ptr = jpl->od_data;
		*out_data_size = jpl->od_data_size;
		return GF_OK;
	}

	return GF_STREAM_NOT_FOUND;
}

static GF_Err IMG_ChannelReleaseSLP(GF_InputService *plug, LPNETCHANNEL channel)
{
	IMGLoader *jpl = plug->priv;

	if (jpl->es_ch == channel) {
		if (!jpl->es_data) return GF_BAD_PARAM;
		free(jpl->es_data);
		jpl->es_data = NULL;
		jpl->es_done = 1;
		return GF_OK;
	}
	if (jpl->od_ch == channel) {
		if (!jpl->od_data) return GF_BAD_PARAM;
		free(jpl->od_data);
		jpl->od_data = NULL;
		jpl->od_done = 1;
		return GF_OK;
	}
	return GF_OK;
}


void *NewLoaderInterface()
{
	IMGLoader *priv;
	GF_InputService *plug = malloc(sizeof(GF_InputService));
	memset(plug, 0, sizeof(GF_InputService));
	GF_REGISTER_MODULE_INTERFACE(plug, GF_NET_CLIENT_INTERFACE, "GPAC Image Reader", "gpac distribution")

	plug->CanHandleURL = IMG_CanHandleURL;
	plug->CanHandleURLInService = NULL;
	plug->ConnectService = IMG_ConnectService;
	plug->CloseService = IMG_CloseService;
	plug->GetServiceDescriptor = IMG_GetServiceDesc;
	plug->ConnectChannel = IMG_ConnectChannel;
	plug->DisconnectChannel = IMG_DisconnectChannel;
	plug->ChannelGetSLP = IMG_ChannelGetSLP;
	plug->ChannelReleaseSLP = IMG_ChannelReleaseSLP;
	plug->ServiceCommand = IMG_ServiceCommand;

	priv = malloc(sizeof(IMGLoader));
	memset(priv, 0, sizeof(IMGLoader));
	plug->priv = priv;
	return plug;
}

void DeleteLoaderInterface(void *ifce)
{
	GF_InputService *plug = (GF_InputService *) ifce;
	IMGLoader *jpl = plug->priv;
	free(jpl);
	free(plug);
}
