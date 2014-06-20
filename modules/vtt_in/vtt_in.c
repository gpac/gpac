/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2013-
 *					All rights reserved
 *
 *  This file is part of GPAC / VTT Loader/Decoder module
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
#include <gpac/scene_manager.h>
#include <gpac/constants.h>
#include <gpac/modules/js_usr.h>

#ifndef GPAC_DISABLE_VTT

typedef struct
{
	/* Counter-part object in the terminal */
	GF_ClientService *service;
	/* Channel object associated to this object */
	LPNETCHANNEL channel;

	/*file downloader*/
	GF_DownloadSession * dnload;

	Bool needs_connection;

	/* Header for the SL packets created by this module and sent out to the terminal */
	GF_SLHeader sl_hdr;
} VTTIn;

const char * VTT_MIME_TYPES[] = {
	"text/vtt", "vtt", "VTT SubTitles",
	NULL
};

static u32 VTT_RegisterMimeTypes(const GF_InputService *plug) {
	u32 i;
	if (!plug) return 0;
	for (i = 0 ; VTT_MIME_TYPES[i]; i+=3) {
		gf_service_register_mime(plug, VTT_MIME_TYPES[i], VTT_MIME_TYPES[i+1], VTT_MIME_TYPES[i+2]);
	}
	return i/3;
}

static Bool VTT_CanHandleURL(GF_InputService *plug, const char *url)
{
	const char *sExt;
	u32 i;
	if (!plug || !url) return GF_FALSE;
	sExt = strrchr(url, '.');
	if (!sExt) return GF_FALSE;
	for (i = 0 ; VTT_MIME_TYPES[i]; i+=3) {
		if (gf_service_check_mime_register(plug, VTT_MIME_TYPES[i], VTT_MIME_TYPES[i+1], VTT_MIME_TYPES[i+2], sExt)) return GF_TRUE;
	}
	return GF_FALSE;
}

static Bool VTT_is_local(const char *url)
{
	if (!url)	return GF_FALSE;
	if (!strnicmp(url, "file://", 7)) return GF_TRUE;
	if (strstr(url, "://")) return GF_FALSE;
	return GF_TRUE;
}

void VTT_NetIO(void *cbk, GF_NETIO_Parameter *param)
{
	GF_Err e;
	const char *szCache;
	GF_InputService *plug = (GF_InputService *)cbk;
	VTTIn *vttin = (VTTIn *) plug->priv;

	if (!vttin)	return;
	gf_service_download_update_stats(vttin->dnload);

	e = param->error;
	/*done*/
	if (param->msg_type==GF_NETIO_DATA_TRANSFERED) {
		szCache = gf_dm_sess_get_cache_name(vttin->dnload);
		if (!szCache) e = GF_IO_ERR;
		else {
			//e = TTIn_LoadFile(plug, szCache, 1);
		}
	}
	else if (param->msg_type==GF_NETIO_DATA_EXCHANGE) {
		return;
	}

	/*OK confirm*/
	if (vttin->needs_connection) {
		vttin->needs_connection = GF_FALSE;
		gf_service_connect_ack(vttin->service, NULL, e);
		//if (!e && !vttin->od_done) tti_setup_object(vttin);
	}
}

void VTT_download_file(GF_InputService *plug, const char *url)
{
	VTTIn *vttin = (VTTIn *) plug->priv;
	if (!plug || !url)
		return;
	vttin->needs_connection = GF_TRUE;
	vttin->dnload = gf_service_download_new(vttin->service, url, 0, VTT_NetIO, plug);
	if (!vttin->dnload) {
		vttin->needs_connection = GF_FALSE;
		gf_service_connect_ack(vttin->service, NULL, GF_NOT_SUPPORTED);
	} else {
		/*start our download (threaded)*/
		gf_dm_sess_process(vttin->dnload);
	}
	/*service confirm is done once fetched*/
}

static GF_Err VTT_ConnectService(GF_InputService *plug, GF_ClientService *serv, const char *url)
{
	GF_Err e;
	VTTIn *vttin = (VTTIn *)plug->priv;
	if (!plug || !url)
		return GF_BAD_PARAM;
	vttin->service = serv;

	if (vttin->dnload) gf_service_download_del(vttin->dnload);
	vttin->dnload = NULL;

	/*remote fetch*/
	if (!VTT_is_local(url)) {
		VTT_download_file(plug, url);
		return GF_OK;
	} else {
		e = GF_OK;
		//e = TTIn_LoadFile(plug, url, 0);
		gf_service_connect_ack(serv, NULL, e);
		//if (!e && !vttin->od_done) tti_setup_object(vttin);
	}
	return GF_OK;
}

static GF_Err VTT_CloseService(GF_InputService *plug)
{
	VTTIn *vttin;
	if (!plug)	return GF_BAD_PARAM;

	vttin = (VTTIn *)plug->priv;
	if (!vttin)	return GF_BAD_PARAM;

	//if (vttin->szFile) {
	//	gf_delete_file(vttin->szFile);
	//	gf_free(vttin->szFile);
	//}
	//vttin->szFile = NULL;

	if (vttin->dnload) {
		gf_service_download_del(vttin->dnload);
	}
	vttin->dnload = NULL;

	if (vttin->service) {
		gf_service_disconnect_ack(vttin->service, NULL, GF_OK);
	}
	vttin->service = NULL;

	return GF_OK;
}

static GF_Descriptor *VTT_GetServiceDesc(GF_InputService *plug, u32 expect_type, const char *sub_url)
{
	VTTIn *vttin;
	if (!plug)	return NULL;
	vttin = (VTTIn *)plug->priv;
	if (!vttin)	return NULL;
	/*visual object*/
	switch (expect_type) {
	case GF_MEDIA_OBJECT_UNDEF:
	case GF_MEDIA_OBJECT_UPDATES:
	case GF_MEDIA_OBJECT_TEXT:
	{
		GF_ObjectDescriptor *od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
		//GF_ESD *esd = tti_get_esd(vttin);
		//od->objectDescriptorID = esd->ESID;
		//gf_list_add(od->ESDescriptors, esd);
		//vttin->od_done = 1;
		return (GF_Descriptor *) od;
	}
	default:
		return NULL;
	}
	return NULL;
}

static GF_Err VTT_ConnectChannel(GF_InputService *plug, LPNETCHANNEL channel, const char *url, Bool upstream)
{
	u32 ES_ID;
	GF_Err e;
	VTTIn *vttin = (VTTIn *)plug->priv;
	if (!vttin)	return GF_BAD_PARAM;
	e = GF_SERVICE_ERROR;
	if (!vttin || vttin->channel==channel) goto exit;

	e = GF_STREAM_NOT_FOUND;
	ES_ID = 0;
	if (strstr(url, "ES_ID")) sscanf(url, "ES_ID=%ud", &ES_ID);

	if (ES_ID==1) {
		vttin->channel = channel;
		e = GF_OK;
	}

exit:
	gf_service_connect_ack(vttin->service, channel, e);
	return e;
}

static GF_Err VTT_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
	VTTIn *vttin = (VTTIn *)plug->priv;
	GF_Err e = GF_STREAM_NOT_FOUND;

	if (!vttin)	return GF_BAD_PARAM;

	if (vttin->channel == channel) {
		vttin->channel = NULL;
		e = GF_OK;
	}
	gf_service_disconnect_ack(vttin->service, channel, e);
	return GF_OK;
}

static GF_Err VTT_ServiceCommand(GF_InputService *plug, GF_NetworkCommand *com)
{
	//VTTIn *vttin = (VTTIn *)plug->priv;

	//if (!vttin)	return GF_BAD_PARAM;
	//if (!com->base.on_channel) return GF_NOT_SUPPORTED;
	//switch (com->command_type) {
	//case GF_NET_CHAN_SET_PADDING:
	//	gf_isom_set_sample_padding(vttin->mp4, vttin->tt_track, com->pad.padding_bytes);
	//	return GF_OK;
	//case GF_NET_CHAN_DURATION:
	//	com->duration.duration = (Double) (s64) gf_isom_get_media_duration(vttin->mp4, vttin->tt_track);
	//	com->duration.duration /= gf_isom_get_media_timescale(vttin->mp4, vttin->tt_track);
	//	return GF_OK;
	//case GF_NET_CHAN_PLAY:
	//	vttin->start_range = (com->play.start_range>0) ? (u32) (com->play.start_range * 1000) : 0;
	//	if (vttin->channel == com->base.on_channel) {
	//		vttin->samp_num = 0;
	//		if (vttin->samp) gf_isom_sample_del(&vttin->samp);
	//	}
	//	return GF_OK;
	//case GF_NET_CHAN_STOP:
	//	return GF_OK;
	//default:
	//	return GF_OK;
	//}
	return GF_OK;
}

static GF_Err VTT_ChannelGetSLP(GF_InputService *plug, LPNETCHANNEL channel, char **out_data_ptr, u32 *out_data_size, GF_SLHeader *out_sl_hdr, Bool *sl_compressed, GF_Err *out_reception_status, Bool *is_new_data)
{
	//VTTIn *vttin = (VTTIn *)plug->priv;

	//*out_reception_status = GF_OK;
	//*sl_compressed = GF_FALSE;
	//*is_new_data = GF_FALSE;

	//memset(&vttin->sl_hdr, 0, sizeof(GF_SLHeader));
	//vttin->sl_hdr.randomAccessPointFlag = 1;
	//vttin->sl_hdr.compositionTimeStampFlag = 1;
	//vttin->sl_hdr.accessUnitStartFlag = vttin->sl_hdr.accessUnitEndFlag = 1;

	///*fetching es data*/
	//if (vttin->channel == channel) {
	//	if (vttin->samp_num>=gf_isom_get_sample_count(vttin->mp4, vttin->tt_track)) {
	//		*out_reception_status = GF_EOS;
	//		return GF_OK;
	//	}

	//	if (!vttin->samp) {
	//		u32 di;
	//		if (vttin->start_range) {
	//			u32 di;
	//			*out_reception_status = gf_isom_get_sample_for_movie_time(vttin->mp4, vttin->tt_track, vttin->start_range, &di, GF_ISOM_SEARCH_SYNC_BACKWARD, &vttin->samp, &vttin->samp_num);
	//			vttin->start_range = 0;
	//		} else {
	//			vttin->samp = gf_isom_get_sample(vttin->mp4, vttin->tt_track, vttin->samp_num+1, &di);
	//		}
	//		if (!vttin->samp) {
	//			*out_reception_status = GF_CORRUPTED_DATA;
	//			return GF_OK;
	//		}
	//		*is_new_data = 1;
	//	}
	//	vttin->sl_hdr.compositionTimeStamp = vttin->sl_hdr.decodingTimeStamp = vttin->samp->DTS;
	//	*out_data_ptr = vttin->samp->data;
	//	*out_data_size = vttin->samp->dataLength;
	//	*out_sl_hdr = vttin->sl_hdr;
	//	return GF_OK;
	//}
	return GF_STREAM_NOT_FOUND;
}

static GF_Err VTT_ChannelReleaseSLP(GF_InputService *plug, LPNETCHANNEL channel)
{
	//VTTIn *vttin = (VTTIn *)plug->priv;
	//if (vttin->channel == channel) {
	//	if (!vttin->samp) return GF_BAD_PARAM;
	//	gf_isom_sample_del(&vttin->samp);
	//	vttin->samp = NULL;
	//	vttin->samp_num++;
	//	return GF_OK;
	//}
	return GF_OK;
}

static void *NewVTTInput()
{
	VTTIn *priv;
	GF_InputService *plug;
	GF_SAFEALLOC(plug, GF_InputService);
	GF_REGISTER_MODULE_INTERFACE(plug, GF_NET_CLIENT_INTERFACE, "GPAC SubTitle Reader", "gpac distribution")

	plug->RegisterMimeTypes = VTT_RegisterMimeTypes;
	plug->CanHandleURL = VTT_CanHandleURL;
	plug->CanHandleURLInService = NULL;
	plug->ConnectService = VTT_ConnectService;
	plug->CloseService = VTT_CloseService;
	plug->GetServiceDescriptor = VTT_GetServiceDesc;
	plug->ConnectChannel = VTT_ConnectChannel;
	plug->DisconnectChannel = VTT_DisconnectChannel;
	plug->ChannelGetSLP = VTT_ChannelGetSLP;
	plug->ChannelReleaseSLP = VTT_ChannelReleaseSLP;
	plug->ServiceCommand = VTT_ServiceCommand;

	GF_SAFEALLOC(priv, VTTIn);
	plug->priv = priv;
	return plug;
}

void DeleteVTTInput(void *ifce)
{
	VTTIn *vttin;
	GF_InputService *plug = (GF_InputService *)ifce;
	if (!plug)
		return;
	vttin = (VTTIn *)plug->priv;
	if (vttin) {
		VTT_CloseService(plug);
		gf_free(vttin);
	}
	plug->priv = NULL;
	gf_free(plug);
}

void *NewVTTDec();
void DeleteVTTDec(GF_BaseDecoder *plug);
//GF_JSUserExtension *NewVTTJS();
//void DeleteVTTJS(GF_BaseInterface *ifce);

/*interface create*/
GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	switch (InterfaceType) {
	case GF_SCENE_DECODER_INTERFACE:
		return (GF_BaseInterface *)NewVTTDec();
	case GF_NET_CLIENT_INTERFACE:
		return (GF_BaseInterface *)NewVTTInput();
//	case GF_JS_USER_EXT_INTERFACE: return (GF_BaseInterface *)NewVTTJS();
	default:
		return NULL;
	}
}


/*interface destroy*/
GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_SCENE_DECODER_INTERFACE:
		DeleteVTTDec((GF_BaseDecoder *)ifce);
		break;
	case GF_NET_CLIENT_INTERFACE:
		DeleteVTTInput(ifce);
		break;
		//case GF_JS_USER_EXT_INTERFACE:
		//	DeleteVTTJS(ifce);
		//	break;
	}
}

#else


/*interface create*/
GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	return NULL;
}


/*interface destroy*/
GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
}

#endif

/*interface query*/
GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
#if !defined(GPAC_DISABLE_VTT)
		GF_SCENE_DECODER_INTERFACE,
		GF_NET_CLIENT_INTERFACE,
//		GF_JS_USER_EXT_INTERFACE,
#endif
		0
	};
	return si;
}

GPAC_MODULE_STATIC_DELARATION( vtt_in )
