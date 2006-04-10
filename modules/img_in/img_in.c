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
	Bool done;
	LPNETCHANNEL ch;

	char *data;
	u32 data_size;

	GF_SLHeader sl_hdr;

	/*file downloader*/
	GF_DownloadSession * dnload;
} IMGLoader;


GF_ESD *IMG_GetESD(IMGLoader *read)
{
	GF_ESD *esd = gf_odf_desc_esd_new(0);
	esd->slConfig->timestampResolution = 1000;
	esd->decoderConfig->streamType = GF_STREAM_VISUAL;
	if (read->img_type == IMG_JPEG) esd->decoderConfig->objectTypeIndication = 0x6c;
	else if (read->img_type == IMG_PNG) esd->decoderConfig->objectTypeIndication = 0x6d;
	else if (read->img_type == IMG_BMP) esd->decoderConfig->objectTypeIndication = GPAC_BMP_OTI;
	esd->ESID = 1;
	return esd;
}

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


static void IMG_SetupObject(IMGLoader *read)
{
	GF_ObjectDescriptor *od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
	GF_ESD *esd = IMG_GetESD(read);
	od->objectDescriptorID = 1;
	gf_list_add(od->ESDescriptors, esd);
	gf_term_add_media(read->service, (GF_Descriptor *)od, 0);
}


void IMG_OnData(void *cbk, char *data, u32 data_size, u32 status, GF_Err e)
{
	const char *szCache;
	IMGLoader *read = (IMGLoader *) cbk;

	/*handle service message*/
	gf_term_download_update_stats(read->dnload);

	/*wait to get the whole file*/
	if (e == GF_OK) return;
	else if (e==GF_EOS) {
		szCache = gf_dm_sess_get_cache_name(read->dnload);
		if (!szCache) e = GF_IO_ERR;
		else {
			read->stream = fopen((char *) szCache, "rb");
			if (!read->stream) e = GF_SERVICE_ERROR;
			else {
				e = GF_OK;
				fseek(read->stream, 0, SEEK_END);
				read->data_size = ftell(read->stream);
				fseek(read->stream, 0, SEEK_SET);
			}
		}
	} 
	/*OK confirm*/
	gf_term_on_connect(read->service, NULL, e);
	if (!e) IMG_SetupObject(read);
}

void jp_download_file(GF_InputService *plug, char *url)
{
	IMGLoader *read = (IMGLoader *) plug->priv;

	read->dnload = gf_term_download_new(read->service, url, 0, IMG_OnData, read);
	if (!read->dnload) gf_term_on_connect(read->service, NULL, GF_NOT_SUPPORTED);
	/*service confirm is done once fetched*/
}

static GF_Err IMG_ConnectService(GF_InputService *plug, GF_ClientService *serv, const char *url)
{
	char *sExt;
	IMGLoader *read = plug->priv;

	read->service = serv;

	sExt = strrchr(url, '.');
	if (!stricmp(sExt, ".jpeg") || !stricmp(sExt, ".jpg")) read->img_type = IMG_JPEG;
	else if (!stricmp(sExt, ".png")) read->img_type = IMG_PNG;
	else if (!stricmp(sExt, ".bmp")) read->img_type = IMG_BMP;

	if (read->dnload) gf_term_download_del(read->dnload);
	read->dnload = NULL;

	/*remote fetch*/
	if (!jp_is_local(url)) {
		jp_download_file(plug, (char *) url);
		return GF_OK;
	}

	read->stream = fopen(url, "rb");
	if (read->stream) {
		fseek(read->stream, 0, SEEK_END);
		read->data_size = ftell(read->stream);
		fseek(read->stream, 0, SEEK_SET);
	}
	gf_term_on_connect(serv, NULL, read->stream ? GF_OK : GF_URL_ERROR);
	if (read->stream) IMG_SetupObject(read);
	return GF_OK;
}

static GF_Err IMG_CloseService(GF_InputService *plug)
{
	IMGLoader *read = plug->priv;
	if (read->stream) fclose(read->stream);
	read->stream = NULL;
	if (read->dnload) gf_term_download_del(read->dnload);
	read->dnload = NULL;
	gf_term_on_disconnect(read->service, NULL, GF_OK);
	return GF_OK;
}

static GF_Descriptor *IMG_GetServiceDesc(GF_InputService *plug, u32 expect_type, const char *sub_url)
{
	GF_ESD *esd;
	IMGLoader *read = plug->priv;

	/*override default*/
	if (expect_type==GF_MEDIA_OBJECT_UNDEF) expect_type=GF_MEDIA_OBJECT_VIDEO;
	read->srv_type = expect_type;

	/*visual object*/
	if (expect_type==GF_MEDIA_OBJECT_VIDEO) {
		GF_ObjectDescriptor *od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
		od->objectDescriptorID = 1;
		esd = IMG_GetESD(read);
		gf_list_add(od->ESDescriptors, esd);
		return (GF_Descriptor *) od;
	}
	return NULL;
}

static GF_Err IMG_ConnectChannel(GF_InputService *plug, LPNETCHANNEL channel, const char *url, Bool upstream)
{
	u32 ES_ID;
	GF_Err e;
	IMGLoader *read = plug->priv;

	e = GF_SERVICE_ERROR;
	if (read->ch==channel) goto exit;

	e = GF_STREAM_NOT_FOUND;
	if (strstr(url, "ES_ID")) {
		sscanf(url, "ES_ID=%d", &ES_ID);
	}
	/*URL setup*/
	else if (!read->ch && IMG_CanHandleURL(plug, url)) ES_ID = 1;

	if (ES_ID==1) {
		read->ch = channel;
		e = GF_OK;
	}

exit:
	gf_term_on_connect(read->service, channel, e);
	return e;
}

static GF_Err IMG_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
	GF_Err e = GF_STREAM_NOT_FOUND;
	IMGLoader *read = plug->priv;

	if (read->ch == channel) {
		read->ch = NULL;
		e = GF_OK;
	}
	gf_term_on_disconnect(read->service, channel, e);
	return GF_OK;
}

static GF_Err IMG_ServiceCommand(GF_InputService *plug, GF_NetworkCommand *com)
{
	IMGLoader *read = plug->priv;

	if (!com->base.on_channel) return GF_NOT_SUPPORTED;
	switch (com->command_type) {
	case GF_NET_CHAN_SET_PADDING:
		read->pad_bytes = com->pad.padding_bytes;
		return GF_OK;
	case GF_NET_CHAN_DURATION:
		com->duration.duration = 0;
		return GF_OK;
	case GF_NET_CHAN_PLAY:
		/*note we don't handle range since we're only dealing with images*/
		if (read->ch == com->base.on_channel) { read->done = 0; }
		return GF_OK;
	case GF_NET_CHAN_STOP:
		return GF_OK;
	default:
		return GF_OK;
	}
}


static GF_Err IMG_ChannelGetSLP(GF_InputService *plug, LPNETCHANNEL channel, char **out_data_ptr, u32 *out_data_size, GF_SLHeader *out_sl_hdr, Bool *sl_compressed, GF_Err *out_reception_status, Bool *is_new_data)
{
	IMGLoader *read = plug->priv;

	*out_reception_status = GF_OK;
	*sl_compressed = 0;
	*is_new_data = 0;

	memset(&read->sl_hdr, 0, sizeof(GF_SLHeader));
	read->sl_hdr.randomAccessPointFlag = 1;
	read->sl_hdr.compositionTimeStampFlag = 1;
	*out_sl_hdr = read->sl_hdr;

	/*fetching es data*/
	if (read->ch == channel) {
		if (read->done) {
			*out_reception_status = GF_EOS;
			return GF_OK;
		}
		if (!read->data) {
			if (!read->stream) {
				*out_data_ptr = NULL;
				*out_data_size = 0;
				return GF_OK;
			}
			*is_new_data = 1;
			fseek(read->stream, 0, SEEK_SET);
			read->data = malloc(sizeof(char) * (read->data_size + read->pad_bytes));
			fread(read->data, sizeof(char) * read->data_size, 1, read->stream);
			fseek(read->stream, 0, SEEK_SET);
			if (read->pad_bytes) memset(read->data + read->data_size, 0, sizeof(char) * read->pad_bytes);

		}
		*out_data_ptr = read->data;
		*out_data_size = read->data_size;
		return GF_OK;
	}
	return GF_STREAM_NOT_FOUND;
}

static GF_Err IMG_ChannelReleaseSLP(GF_InputService *plug, LPNETCHANNEL channel)
{
	IMGLoader *read = plug->priv;

	if (read->ch == channel) {
		if (!read->data) return GF_BAD_PARAM;
		free(read->data);
		read->data = NULL;
		read->done = 1;
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
	IMGLoader *read = plug->priv;
	free(read);
	free(plug);
}
