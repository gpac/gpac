/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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
#include <gpac/avparse.h>

enum
{
	IMG_JPEG = 1,
	IMG_PNG,
	IMG_BMP,
	IMG_PNGD,
	IMG_PNGDS,
	IMG_PNGS,
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

	Bool is_inline;
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
	esd->ESID = 1;

	if (read->img_type == IMG_BMP)
		esd->decoderConfig->objectTypeIndication = GPAC_BMP_OTI;
	else {
		u8 OTI=0;
		GF_BitStream *bs = gf_bs_from_file(read->stream, GF_BITSTREAM_READ);
#ifndef GPAC_DISABLE_AV_PARSERS
		u32 mtype, w, h;
		gf_img_parse(bs, &OTI, &mtype, &w, &h, &esd->decoderConfig->decoderSpecificInfo->data, &esd->decoderConfig->decoderSpecificInfo->dataLength);
#endif
		gf_bs_del(bs);

		if (!OTI) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[IMG_IN] Unable to guess format image - assigning from extension\n"));
			if (read->img_type==IMG_JPEG) OTI = GPAC_OTI_IMAGE_JPEG;
			else if (read->img_type==IMG_PNG) OTI = GPAC_OTI_IMAGE_PNG;
		}
		esd->decoderConfig->objectTypeIndication = OTI;

		if (read->img_type == IMG_PNGD) {
			GF_Descriptor *d = gf_odf_desc_new(GF_ODF_AUX_VIDEO_DATA);
			((GF_AuxVideoDescriptor*)d)->aux_video_type = 1;
			gf_list_add(esd->extensionDescriptors, d);
		}
		else if (read->img_type == IMG_PNGDS) {
			GF_Descriptor *d = gf_odf_desc_new(GF_ODF_AUX_VIDEO_DATA);
			((GF_AuxVideoDescriptor*)d)->aux_video_type = 2;
			gf_list_add(esd->extensionDescriptors, d);
		}
		else if (read->img_type == IMG_PNGS) {
			GF_Descriptor *d = gf_odf_desc_new(GF_ODF_AUX_VIDEO_DATA);
			((GF_AuxVideoDescriptor*)d)->aux_video_type = 3;
			gf_list_add(esd->extensionDescriptors, d);
		}
	}
	return esd;
}

const char * IMG_MIME_TYPES[] = {
	"image/jpeg", "jpeg jpg", "JPEG Images",
	"image/jp2", "jp2", "JPEG2000 Images",
	"image/png", "png", "PNG Images",
	"image/bmp", "bmp", "MS Bitmap Images",
	"image/x-png+depth", "pngd", "PNG+Depth Images",
	"image/x-png+depth+mask", "pngds", "PNG+Depth+Mask Images",
	"image/x-png+stereo", "pngs", "Stereo PNG Images",
	NULL
};

static u32 IMG_RegisterMimeTypes(const GF_InputService *plug) {
	u32 i;
	if (!plug) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("IMG_RegisterMimeTypes : plug is NULL !!\n"));
	}
	for (i = 0 ; IMG_MIME_TYPES[i]; i+=3)
		gf_service_register_mime(plug, IMG_MIME_TYPES[i], IMG_MIME_TYPES[i+1], IMG_MIME_TYPES[i+2]);
	return i/3;
}


static Bool IMG_CanHandleURL(GF_InputService *plug, const char *url)
{
	char *sExt;
	u32 i;
	GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("IMG_CanHandleURL(%s)\n", url));
	if (!plug || !url)
		return GF_FALSE;
	sExt = strrchr(url, '.');
	for (i = 0 ; IMG_MIME_TYPES[i]; i+=3) {
		if (gf_service_check_mime_register(plug, IMG_MIME_TYPES[i], IMG_MIME_TYPES[i+1], IMG_MIME_TYPES[i+2], sExt))
			return GF_TRUE;
	}
	return GF_FALSE;
}

static Bool jp_is_local(const char *url)
{
	if (!strnicmp(url, "file://", 7)) return GF_TRUE;
	if (strstr(url, "://")) return GF_FALSE;
	return GF_TRUE;
}


static void IMG_SetupObject(IMGLoader *read)
{
	if (!read->ch) {
		GF_ObjectDescriptor *od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
		GF_ESD *esd = IMG_GetESD(read);
		od->objectDescriptorID = 1;
		gf_list_add(od->ESDescriptors, esd);
		gf_service_declare_media(read->service, (GF_Descriptor *)od, GF_FALSE);
	}
}


void IMG_NetIO(void *cbk, GF_NETIO_Parameter *param)
{
	GF_Err e;
	const char *szCache;
	IMGLoader *read = (IMGLoader *) cbk;
	if (!read->dnload) return;

	/*handle service message*/
	gf_service_download_update_stats(read->dnload);

	e = param->error;
	/*wait to get the whole file*/
	if (!e && (param->msg_type!=GF_NETIO_DATA_TRANSFERED)) return;
	if ((e==GF_EOS) && (param->msg_type==GF_NETIO_DATA_EXCHANGE)) return;

	if (param->msg_type==GF_NETIO_DATA_TRANSFERED) {
		szCache = gf_dm_sess_get_cache_name(read->dnload);
		if (!szCache) e = GF_IO_ERR;
		else {
			if (read->stream) gf_fclose(read->stream);
			read->stream = gf_fopen((char *) szCache, "rb");
			if (!read->stream) e = GF_SERVICE_ERROR;
			else {
				e = GF_OK;
				gf_fseek(read->stream, 0, SEEK_END);
				read->data_size = (u32) gf_ftell(read->stream);
				gf_fseek(read->stream, 0, SEEK_SET);
			}
		}
	}
	/*OK confirm*/
	gf_service_connect_ack(read->service, NULL, e);
	if (!e) IMG_SetupObject(read);
}

void jp_download_file(GF_InputService *plug, const char *url)
{
	IMGLoader *read = (IMGLoader *) plug->priv;

	read->dnload = gf_service_download_new(read->service, url, 0, IMG_NetIO, read);
	if (!read->dnload) {
		gf_service_connect_ack(read->service, NULL, GF_NOT_SUPPORTED);
	} else {
		/*start our download (threaded)*/
		gf_dm_sess_process(read->dnload);
	}
	/*service confirm is done once fetched*/
}

static GF_Err IMG_ConnectService(GF_InputService *plug, GF_ClientService *serv, const char *url)
{
	char *sExt, *frag;
	IMGLoader *read = (IMGLoader *)plug->priv;

	read->service = serv;
	if (!url)
		return GF_BAD_PARAM;

	frag = strrchr(url, '#');
	if (frag) frag[0] = 0;

	sExt = strrchr(url, '.');
	if (!stricmp(sExt, ".jpeg") || !stricmp(sExt, ".jpg")) read->img_type = IMG_JPEG;
	else if (!stricmp(sExt, ".png")) read->img_type = IMG_PNG;
	else if (!stricmp(sExt, ".pngd")) read->img_type = IMG_PNGD;
	else if (!stricmp(sExt, ".pngds")) read->img_type = IMG_PNGDS;
	else if (!stricmp(sExt, ".pngs")) read->img_type = IMG_PNGS;
	else if (!stricmp(sExt, ".bmp")) read->img_type = IMG_BMP;

	if (read->dnload) gf_service_download_del(read->dnload);
	read->dnload = NULL;

	/*remote fetch*/
	if (!jp_is_local(url)) {
		jp_download_file(plug, url);
		if (frag) frag[0] = '#';
		return GF_OK;
	}

	read->stream = gf_fopen(url, "rb");
	if (read->stream) {
		gf_fseek(read->stream, 0, SEEK_END);
		read->data_size = (u32) gf_ftell(read->stream);
		gf_fseek(read->stream, 0, SEEK_SET);
	}
	if (frag) frag[0] = '#';
	gf_service_connect_ack(serv, NULL, read->stream ? GF_OK : GF_URL_ERROR);
	if (read->stream && read->is_inline) IMG_SetupObject(read);
	return GF_OK;
}

static GF_Err IMG_CloseService(GF_InputService *plug)
{
	IMGLoader *read;
	if (!plug)
		return GF_BAD_PARAM;
	read = (IMGLoader *)plug->priv;
	if (!read)
		return GF_BAD_PARAM;
	if (read->stream) gf_fclose(read->stream);
	read->stream = NULL;
	if (read->dnload) gf_service_download_del(read->dnload);
	read->dnload = NULL;
	if (read->service)
		gf_service_disconnect_ack(read->service, NULL, GF_OK);
	return GF_OK;
}

static GF_Descriptor *IMG_GetServiceDesc(GF_InputService *plug, u32 expect_type, const char *sub_url)
{
	GF_ESD *esd;
	IMGLoader *read = (IMGLoader *)plug->priv;

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
	read->is_inline = GF_TRUE;
	return NULL;
}

static GF_Err IMG_ConnectChannel(GF_InputService *plug, LPNETCHANNEL channel, const char *url, Bool upstream)
{
	u32 ES_ID=0;
	GF_Err e;
	IMGLoader *read;
	if (!plug)
		return GF_OK;
	read = (IMGLoader *)plug->priv;

	e = GF_SERVICE_ERROR;
	if (read->ch==channel) goto exit;
	if (!url)
		goto exit;
	e = GF_STREAM_NOT_FOUND;
	if (strstr(url, "ES_ID")) {
		sscanf(url, "ES_ID=%ud", &ES_ID);
	}
	/*URL setup*/
	else if (!read->ch && IMG_CanHandleURL(plug, url)) ES_ID = 1;

	if (ES_ID==1) {
		read->ch = channel;
		e = GF_OK;
	}

exit:
	gf_service_connect_ack(read->service, channel, e);
	return e;
}

static GF_Err IMG_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
	GF_Err e = GF_STREAM_NOT_FOUND;
	IMGLoader *read = (IMGLoader *)plug->priv;

	if (read->ch == channel) {
		read->ch = NULL;
		e = GF_OK;
	}
	gf_service_disconnect_ack(read->service, channel, e);
	return GF_OK;
}

static GF_Err IMG_ServiceCommand(GF_InputService *plug, GF_NetworkCommand *com)
{
	IMGLoader *read = (IMGLoader *)plug->priv;

	if (com->command_type==GF_NET_SERVICE_HAS_AUDIO) return GF_NOT_SUPPORTED;

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
		if (read->ch == com->base.on_channel) {
			read->done = GF_FALSE;
		}
		return GF_OK;
	case GF_NET_CHAN_STOP:
		return GF_OK;
	default:
		return GF_OK;
	}
}


static GF_Err IMG_ChannelGetSLP(GF_InputService *plug, LPNETCHANNEL channel, char **out_data_ptr, u32 *out_data_size, GF_SLHeader *out_sl_hdr, Bool *sl_compressed, GF_Err *out_reception_status, Bool *is_new_data)
{
	IMGLoader *read = (IMGLoader *)plug->priv;

	*out_reception_status = GF_OK;
	*sl_compressed = GF_FALSE;
	*is_new_data = GF_FALSE;

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
			*is_new_data = GF_TRUE;
			gf_fseek(read->stream, 0, SEEK_SET);
			read->data = (char*) gf_malloc(sizeof(char) * (read->data_size + read->pad_bytes));
			read->data_size = (u32) fread(read->data, sizeof(char), read->data_size, read->stream);
			if ((s32) read->data_size<0) return GF_IO_ERR;
			gf_fseek(read->stream, 0, SEEK_SET);
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
	IMGLoader *read = (IMGLoader *)plug->priv;

	if (read->ch == channel) {
		if (!read->data) return GF_BAD_PARAM;
		gf_free(read->data);
		read->data = NULL;
		read->done = GF_TRUE;
		return GF_OK;
	}
	return GF_OK;
}


void *NewLoaderInterface()
{
	IMGLoader *priv;
	GF_InputService *plug;
	GF_SAFEALLOC(plug, GF_InputService);
	if (!plug) return NULL;
	GF_REGISTER_MODULE_INTERFACE(plug, GF_NET_CLIENT_INTERFACE, "GPAC Image Reader", "gpac distribution")

	GF_SAFEALLOC(priv, IMGLoader);
	if (!priv) {
		gf_free(plug);
		return NULL;
	}
	plug->priv = priv;

	plug->RegisterMimeTypes = IMG_RegisterMimeTypes;
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

	return plug;
}

void DeleteLoaderInterface(void *ifce)
{
	IMGLoader *read;
	GF_InputService *plug = (GF_InputService *) ifce;
	if (!plug)
		return;
	read = (IMGLoader *)plug->priv;
	if (read)
		gf_free(read);
	plug->priv = NULL;
	gf_free(plug);
}
