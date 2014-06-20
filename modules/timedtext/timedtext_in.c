/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / 3GPP/MPEG4 timed text module
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

#include <gpac/modules/service.h>
#include <gpac/modules/codec.h>
#include <gpac/constants.h>

#if !defined(GPAC_DISABLE_ISOM_WRITE) && !defined(GPAC_DISABLE_MEDIA_IMPORT)

#include <gpac/media_tools.h>

typedef struct
{
	GF_ClientService *service;
	Bool od_done;
	Bool needs_connection;
	u32 status;
	LPNETCHANNEL ch;

	GF_SLHeader sl_hdr;

	GF_ISOFile *mp4;
	char *szFile;
	u32 tt_track;
	GF_ISOSample *samp;
	u32 samp_num;

	u32 start_range;
	/*file downloader*/
	GF_DownloadSession * dnload;
} TTIn;


const char * TTIN_MIME_TYPES[] = {
	"x-subtitle/srt", "srt", "SRT SubTitles",
	"x-subtitle/sub", "sub", "SUB SubTitles",
	"x-subtitle/ttxt", "ttxt", "3GPP TimedText",
	NULL
};

static u32 TTIN_RegisterMimeTypes(const GF_InputService *plug) {
	u32 i;
	if (!plug)
		return 0;
	for (i = 0 ; TTIN_MIME_TYPES[i]; i+=3) {
		gf_service_register_mime(plug, TTIN_MIME_TYPES[i], TTIN_MIME_TYPES[i+1], TTIN_MIME_TYPES[i+2]);
	}
	return i/3;
}

static Bool TTIn_CanHandleURL(GF_InputService *plug, const char *url)
{
	char *sExt;
	u32 i;
	if (!plug || !url)
		return 0;
	sExt = strrchr(url, '.');
	if (!sExt) return 0;
	for (i = 0 ; TTIN_MIME_TYPES[i]; i+=3) {
		if (gf_service_check_mime_register(plug, TTIN_MIME_TYPES[i], TTIN_MIME_TYPES[i+1], TTIN_MIME_TYPES[i+2], sExt)) return 1;
	}
	return 0;
}

static Bool TTIn_is_local(const char *url)
{
	if (!url)
		return 0;
	if (!strnicmp(url, "file://", 7)) return 1;
	if (strstr(url, "://")) return 0;
	return 1;
}

static GF_ESD *tti_get_esd(TTIn *tti)
{
	return gf_media_map_esd(tti->mp4, tti->tt_track);
}

static void tti_setup_object(TTIn *tti)
{
	GF_ObjectDescriptor *od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
	GF_ESD *esd = tti_get_esd(tti);
	od->objectDescriptorID = esd->ESID;
	gf_list_add(od->ESDescriptors, esd);
	gf_service_declare_media(tti->service, (GF_Descriptor *)od, 0);
}


GF_Err TTIn_LoadFile(GF_InputService *plug, const char *url, Bool is_cache)
{
	GF_Err e;
	GF_MediaImporter import;
	char szFILE[GF_MAX_PATH];
	TTIn *tti = (TTIn *)plug->priv;
	const char *cache_dir;
	if (!tti || !url)
		return GF_BAD_PARAM;
	cache_dir = gf_modules_get_option((GF_BaseInterface *)plug, "General", "CacheDirectory");
	if (cache_dir && strlen(cache_dir)) {
		if (cache_dir[strlen(cache_dir)-1] != GF_PATH_SEPARATOR) {
			sprintf(szFILE, "%s%csrt_%p_mp4", cache_dir, GF_PATH_SEPARATOR, tti);
		} else {
			sprintf(szFILE, "%ssrt_%p_mp4", cache_dir, tti);
		}
	} else {
		sprintf(szFILE, "%p_temp_mp4", tti);
	}
	tti->mp4 = gf_isom_open(szFILE, GF_ISOM_OPEN_WRITE, NULL);
	if (!tti->mp4) return gf_isom_last_error(NULL);
	if (tti->szFile)
		gf_free(tti->szFile);
	tti->szFile = gf_strdup(szFILE);

	memset(&import, 0, sizeof(GF_MediaImporter));
	import.dest = tti->mp4;
	/*override layout from sub file*/
	import.flags = GF_IMPORT_SKIP_TXT_BOX;
	import.in_name = gf_strdup(url);

	e = gf_media_import(&import);
	if (!e) {
		tti->tt_track = 1;
		gf_isom_text_set_streaming_mode(tti->mp4, 1);
	}
	if (import.in_name)
		gf_free(import.in_name);
	return e;
}

void TTIn_NetIO(void *cbk, GF_NETIO_Parameter *param)
{
	GF_Err e;
	const char *szCache;
	GF_InputService *plug = (GF_InputService *)cbk;
	TTIn *tti = (TTIn *) plug->priv;
	if (!tti)
		return;
	gf_service_download_update_stats(tti->dnload);

	e = param->error;
	/*done*/
	if (param->msg_type==GF_NETIO_DATA_TRANSFERED) {
		szCache = gf_dm_sess_get_cache_name(tti->dnload);
		if (!szCache) e = GF_IO_ERR;
		else {
			e = TTIn_LoadFile(plug, szCache, 1);
		}
	}
	else if (param->msg_type==GF_NETIO_DATA_EXCHANGE)
		return;

	/*OK confirm*/
	if (tti->needs_connection) {
		tti->needs_connection = 0;
		gf_service_connect_ack(tti->service, NULL, e);
		if (!e && !tti->od_done) tti_setup_object(tti);
	}
}

void TTIn_download_file(GF_InputService *plug, const char *url)
{
	TTIn *tti = (TTIn *) plug->priv;
	if (!plug || !url)
		return;
	tti->needs_connection = 1;
	tti->dnload = gf_service_download_new(tti->service, url, 0, TTIn_NetIO, plug);
	if (!tti->dnload) {
		tti->needs_connection = 0;
		gf_service_connect_ack(tti->service, NULL, GF_NOT_SUPPORTED);
	} else {
		/*start our download (threaded)*/
		gf_dm_sess_process(tti->dnload);
	}
	/*service confirm is done once fetched*/
}

static GF_Err TTIn_ConnectService(GF_InputService *plug, GF_ClientService *serv, const char *url)
{
	GF_Err e;
	TTIn *tti = (TTIn *)plug->priv;
	if (!plug || !url)
		return GF_BAD_PARAM;
	tti->service = serv;

	if (tti->dnload) gf_service_download_del(tti->dnload);
	tti->dnload = NULL;

	/*remote fetch*/
	if (!TTIn_is_local(url)) {
		TTIn_download_file(plug, url);
		return GF_OK;
	}
	e = TTIn_LoadFile(plug, url, 0);
	gf_service_connect_ack(serv, NULL, e);
	if (!e && !tti->od_done) tti_setup_object(tti);
	return GF_OK;
}

static GF_Err TTIn_CloseService(GF_InputService *plug)
{
	TTIn *tti;
	if (!plug)
		return GF_BAD_PARAM;
	tti = (TTIn *)plug->priv;
	if (!tti)
		return GF_BAD_PARAM;
	if (tti->samp)
		gf_isom_sample_del(&tti->samp);
	tti->samp = NULL;
	if (tti->mp4)
		gf_isom_delete(tti->mp4);
	tti->mp4 = NULL;
	if (tti->szFile) {
		gf_delete_file(tti->szFile);
		gf_free(tti->szFile);
		tti->szFile = NULL;
	}
	if (tti->dnload)
		gf_service_download_del(tti->dnload);
	tti->dnload = NULL;
	if (tti->service)
		gf_service_disconnect_ack(tti->service, NULL, GF_OK);
	tti->service = NULL;
	return GF_OK;
}

static GF_Descriptor *TTIn_GetServiceDesc(GF_InputService *plug, u32 expect_type, const char *sub_url)
{
	TTIn *tti;
	if (!plug)
		return NULL;
	tti = (TTIn *)plug->priv;
	if (!tti)
		return NULL;
	/*visual object*/
	switch (expect_type) {
	case GF_MEDIA_OBJECT_UNDEF:
	case GF_MEDIA_OBJECT_UPDATES:
	case GF_MEDIA_OBJECT_TEXT:
	{
		GF_ObjectDescriptor *od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
		GF_ESD *esd = tti_get_esd(tti);
		od->objectDescriptorID = esd->ESID;
		gf_list_add(od->ESDescriptors, esd);
		tti->od_done = 1;
		return (GF_Descriptor *) od;
	}
	default:
		return NULL;
	}
	return NULL;
}

static GF_Err TTIn_ConnectChannel(GF_InputService *plug, LPNETCHANNEL channel, const char *url, Bool upstream)
{
	u32 ES_ID;
	GF_Err e;
	TTIn *tti = (TTIn *)plug->priv;
	if (!tti)
		return GF_BAD_PARAM;
	e = GF_SERVICE_ERROR;
	if (!tti || tti->ch==channel) goto exit;

	e = GF_STREAM_NOT_FOUND;
	ES_ID = 0;
	if (strstr(url, "ES_ID")) sscanf(url, "ES_ID=%ud", &ES_ID);

	if (ES_ID==1) {
		tti->ch = channel;
		e = GF_OK;
	}

exit:
	gf_service_connect_ack(tti->service, channel, e);
	return e;
}

static GF_Err TTIn_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
	TTIn *tti = (TTIn *)plug->priv;
	GF_Err e = GF_STREAM_NOT_FOUND;

	if (tti->ch == channel) {
		tti->ch = NULL;
		e = GF_OK;
	}
	gf_service_disconnect_ack(tti->service, channel, e);
	return GF_OK;
}

static GF_Err TTIn_ServiceCommand(GF_InputService *plug, GF_NetworkCommand *com)
{
	TTIn *tti = (TTIn *)plug->priv;
	if (!tti)
		return GF_BAD_PARAM;
	if (!com->base.on_channel) return GF_NOT_SUPPORTED;
	switch (com->command_type) {
	case GF_NET_CHAN_SET_PADDING:
		gf_isom_set_sample_padding(tti->mp4, tti->tt_track, com->pad.padding_bytes);
		return GF_OK;
	case GF_NET_CHAN_DURATION:
		com->duration.duration = (Double) (s64) gf_isom_get_media_duration(tti->mp4, tti->tt_track);
		com->duration.duration /= gf_isom_get_media_timescale(tti->mp4, tti->tt_track);
		return GF_OK;
	case GF_NET_CHAN_PLAY:
		tti->start_range = (com->play.start_range>0) ? (u32) (com->play.start_range * 1000) : 0;
		if (tti->ch == com->base.on_channel) {
			tti->samp_num = 0;
			if (tti->samp) gf_isom_sample_del(&tti->samp);
		}
		return GF_OK;
	case GF_NET_CHAN_STOP:
		return GF_OK;
	default:
		return GF_OK;
	}
}


static GF_Err TTIn_ChannelGetSLP(GF_InputService *plug, LPNETCHANNEL channel, char **out_data_ptr, u32 *out_data_size, GF_SLHeader *out_sl_hdr, Bool *sl_compressed, GF_Err *out_reception_status, Bool *is_new_data)
{
	TTIn *tti = (TTIn *)plug->priv;

	*out_reception_status = GF_OK;
	*sl_compressed = 0;
	*is_new_data = 0;

	memset(&tti->sl_hdr, 0, sizeof(GF_SLHeader));
	tti->sl_hdr.randomAccessPointFlag = 1;
	tti->sl_hdr.compositionTimeStampFlag = 1;
	tti->sl_hdr.accessUnitStartFlag = tti->sl_hdr.accessUnitEndFlag = 1;

	/*fetching es data*/
	if (tti->ch == channel) {
		if (tti->samp_num>=gf_isom_get_sample_count(tti->mp4, tti->tt_track)) {
			*out_reception_status = GF_EOS;
			return GF_OK;
		}

		if (!tti->samp) {
			u32 di;
			if (tti->start_range) {
				u32 di;
				*out_reception_status = gf_isom_get_sample_for_movie_time(tti->mp4, tti->tt_track, tti->start_range, &di, GF_ISOM_SEARCH_SYNC_BACKWARD, &tti->samp, &tti->samp_num);
				tti->start_range = 0;
			} else {
				tti->samp = gf_isom_get_sample(tti->mp4, tti->tt_track, tti->samp_num+1, &di);
			}
			if (!tti->samp) {
				*out_reception_status = GF_CORRUPTED_DATA;
				return GF_OK;
			}
			*is_new_data = 1;
		}
		tti->sl_hdr.compositionTimeStamp = tti->sl_hdr.decodingTimeStamp = tti->samp->DTS;
		*out_data_ptr = tti->samp->data;
		*out_data_size = tti->samp->dataLength;
		*out_sl_hdr = tti->sl_hdr;
		return GF_OK;
	}
	return GF_STREAM_NOT_FOUND;
}

static GF_Err TTIn_ChannelReleaseSLP(GF_InputService *plug, LPNETCHANNEL channel)
{
	TTIn *tti = (TTIn *)plug->priv;

	if (tti->ch == channel) {
		if (!tti->samp) return GF_BAD_PARAM;
		gf_isom_sample_del(&tti->samp);
		tti->samp = NULL;
		tti->samp_num++;
		return GF_OK;
	}
	return GF_OK;
}


void *NewTTReader()
{
	TTIn *priv;
	GF_InputService *plug;
	GF_SAFEALLOC(plug, GF_InputService);
	GF_REGISTER_MODULE_INTERFACE(plug, GF_NET_CLIENT_INTERFACE, "GPAC SubTitle Reader", "gpac distribution")

	plug->RegisterMimeTypes = TTIN_RegisterMimeTypes;
	plug->CanHandleURL = TTIn_CanHandleURL;
	plug->CanHandleURLInService = NULL;
	plug->ConnectService = TTIn_ConnectService;
	plug->CloseService = TTIn_CloseService;
	plug->GetServiceDescriptor = TTIn_GetServiceDesc;
	plug->ConnectChannel = TTIn_ConnectChannel;
	plug->DisconnectChannel = TTIn_DisconnectChannel;
	plug->ChannelGetSLP = TTIn_ChannelGetSLP;
	plug->ChannelReleaseSLP = TTIn_ChannelReleaseSLP;
	plug->ServiceCommand = TTIn_ServiceCommand;

	GF_SAFEALLOC(priv, TTIn);
	plug->priv = priv;
	return plug;
}

void DeleteTTReader(void *ifce)
{
	TTIn *tti;
	GF_InputService *plug = (GF_InputService *) ifce;
	if (!plug)
		return;
	tti = (TTIn *)plug->priv;
	if (tti) {
		TTIn_CloseService(plug);
		gf_free(tti);
	}
	plug->priv = NULL;
	gf_free(plug);
}

#endif /* !defined(GPAC_DISABLE_ISOM_WRITE) && !defined(GPAC_DISABLE_MEDIA_IMPORT)*/
