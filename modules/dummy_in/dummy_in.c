/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / Dummy input module
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
/*for GF_STREAM_PRIVATE_SCENE definition*/
#include <gpac/constants.h>
#include <gpac/download.h>
#include <gpac/xml.h>

typedef struct
{
	u32 ESID;
	LPNETCHANNEL ch;
	u32 start, end;
} DummyChannel;

typedef struct
{
	/*the service we're responsible for*/
	GF_ClientService *service;
	char *url;
	u32 oti;
	Bool is_views_url;
	GF_List *channels;

	/*file downloader*/
	GF_DownloadSession * dnload;
	Bool is_service_connected;
} DCReader;

DummyChannel *DC_GetChannel(DCReader *read, LPNETCHANNEL ch)
{
	DummyChannel *dc;
	u32 i=0;
	while ((dc = (DummyChannel *)gf_list_enum(read->channels, &i))) {
		if (dc->ch && dc->ch==ch) return dc;
	}
	return NULL;
}

Bool DC_RemoveChannel(DCReader *read, LPNETCHANNEL ch)
{
	DummyChannel *dc;
	u32 i=0;
	while ((dc = (DummyChannel *)gf_list_enum(read->channels, &i))) {
		if (dc->ch && dc->ch==ch) {
			gf_list_rem(read->channels, i-1);
			gf_free(dc);
			return 1;
		}
	}
	return 0;
}

static const char * DC_MIME_TYPES[] = {
	/*  "application/x-xmta", "xmta xmta.gz xmtaz", "MPEG-4 Text (XMT)" */
	"application/x-bt", "bt bt.gz btz", "MPEG-4 Text (BT)",
	"application/x-xmt", "xmt xmt.gz xmtz", "MPEG-4 Text (XMT)",
	"model/vrml", "wrl wrl.gz", "VRML World",
	"x-model/x-vrml", "wrl wrl.gz", "VRML World",
	"model/x3d+vrml", "x3dv x3dv.gz x3dvz", "X3D/VRML World",
	"model/x3d+xml", "x3d x3d.gz x3dz", "X3D/XML World",
	"application/x-shockwave-flash", "swf", "Macromedia Flash Movie",
	"image/svg+xml", "svg svg.gz svgz", "SVG Document",
	"image/x-svgm", "svgm", "SVGM Document",
	"application/x-LASeR+xml", "xsr", "LASeR Document",
	"application/widget", "wgt", "W3C Widget Package",
	"application/x-mpegu-widget", "mgt", "MPEG-U Widget Package",
	NULL
};


static u32 DC_RegisterMimeTypes(const GF_InputService *plug) {
	u32 i;
	if (!plug)
		return 0;
	for (i = 0 ; DC_MIME_TYPES[i] ; i+=3)
		gf_service_register_mime(plug, DC_MIME_TYPES[i], DC_MIME_TYPES[i+1], DC_MIME_TYPES[i+2]);
	return i / 3;
}

Bool DC_CanHandleURL(GF_InputService *plug, const char *url)
{
	char *sExt;
	if (!plug || !url)
		return 0;
	sExt = strrchr(url, '.');
	if (sExt) {
		Bool ok = 0;
		char *cgi_par;
		if (!strnicmp(sExt, ".gz", 3)) sExt = strrchr(sExt, '.');
		if (!strnicmp(url, "rtsp://", 7)) return 0;

		cgi_par = strchr(sExt, '?');
		if (cgi_par) cgi_par[0] = 0;
		{
			u32 i;
			for (i = 0 ; DC_MIME_TYPES[i] ; i+=3)
				if (0 != (ok = gf_service_check_mime_register(plug, DC_MIME_TYPES[i], DC_MIME_TYPES[i+1], DC_MIME_TYPES[i+2], sExt)))
					break;
		}
		if (cgi_par) cgi_par[0] = '?';
		if (ok) return 1;
	}
	/*views:// internal URI*/
	if (!strnicmp(url, "views://", 8))
		return 1;

	if (!strncmp(url, "\\\\", 2)) return 0;

	if (!strnicmp(url, "file://", 7) || !strstr(url, "://")) {
		char *rtype = gf_xml_get_root_type(url, NULL);
		if (rtype) {
			Bool handled = 0;
			if (!strcmp(rtype, "SAFSession")) handled = 1;
			else if (!strcmp(rtype, "XMT-A")) handled = 1;
			else if (!strcmp(rtype, "X3D")) handled = 1;
			else if (!strcmp(rtype, "svg")) handled = 1;
			else if (!strcmp(rtype, "bindings")) handled = 1;
			else if (!strcmp(rtype, "widget")) handled = 1;
			gf_free(rtype);
			return handled;
		}
	}
	return 0;
}

void DC_NetIO(void *cbk, GF_NETIO_Parameter *param)
{
	GF_Err e;
	DCReader *read = (DCReader *) cbk;

	/*handle service message*/
	gf_service_download_update_stats(read->dnload);

	e = param->error;

	if (param->msg_type==GF_NETIO_DATA_TRANSFERED) {
	} else if (param->msg_type==GF_NETIO_PARSE_HEADER) {
		if (!strcmp(param->name, "Content-Type")) {
			if (strstr(param->value, "application/x-bt")) read->oti = GPAC_OTI_PRIVATE_SCENE_GENERIC;
			if (strstr(param->value, "application/x-xmt")) read->oti = GPAC_OTI_PRIVATE_SCENE_GENERIC;
			if (strstr(param->value, "model/vrml")) read->oti = GPAC_OTI_PRIVATE_SCENE_GENERIC;
			if (strstr(param->value, "model/x3d+vrml")) read->oti = GPAC_OTI_PRIVATE_SCENE_GENERIC;
			if (strstr(param->value, "application/x-shockwave-flash")) read->oti = GPAC_OTI_PRIVATE_SCENE_GENERIC;
			if (strstr(param->value, "image/svg+xml")) read->oti = GPAC_OTI_PRIVATE_SCENE_SVG;
			if (strstr(param->value, "image/x-svgm")) read->oti = GPAC_OTI_PRIVATE_SCENE_SVG;
			if (strstr(param->value, "application/x-LASeR+xml")) read->oti = GPAC_OTI_PRIVATE_SCENE_GENERIC;
			if (strstr(param->value, "application/widget")) read->oti = GPAC_OTI_PRIVATE_SCENE_WGT;
			if (strstr(param->value, "application/x-mpegu-widget")) read->oti = GPAC_OTI_PRIVATE_SCENE_WGT;
		}
		return;
	} else if (!e && (param->msg_type!=GF_NETIO_DATA_EXCHANGE)) return;

	if (!e && !read->oti)
		return;

	/*OK confirm*/
	if (!read->is_service_connected) {
		if (!gf_dm_sess_get_cache_name(read->dnload)) e = GF_IO_ERR;
		gf_service_connect_ack(read->service, NULL, e);
		read->is_service_connected = 1;
	}
}

void DC_DownloadFile(GF_InputService *plug, char *url)
{
	DCReader *read = (DCReader *) plug->priv;

	read->dnload = gf_service_download_new(read->service, url, 0, DC_NetIO, read);
	if (!read->dnload) {
		gf_service_connect_ack(read->service, NULL, GF_NOT_SUPPORTED);
	} else {
		/*start our download (threaded)*/
		gf_dm_sess_process(read->dnload);
	}
}


GF_Err DC_ConnectService(GF_InputService *plug, GF_ClientService *serv, const char *url)
{
	DCReader *read = (DCReader *) plug->priv;
	FILE *test;
	char *tmp, *ext;

	if (!read || !serv || !url) return GF_BAD_PARAM;

	if (read->dnload) gf_service_download_del(read->dnload);
	read->dnload = NULL;

	read->url = gf_strdup(url);
	ext = strchr(read->url, '#');
	if (ext) {
		char *anext;
		ext[0] = 0;
		anext = strrchr(read->url, '.');
		ext[0] = '#';
		ext = anext;
	} else {
		ext = strrchr(read->url, '.');
	}
	if (ext && !stricmp(ext, ".gz")) {
		char *anext;
		ext[0] = 0;
		anext = strrchr(read->url, '.');
		ext[0] = '.';
		ext = anext;
	}
	read->service = serv;

	if (!strnicmp(url, "views://", 8)) {
		read->is_views_url = 1;
		gf_service_connect_ack(serv, NULL, GF_OK);
		read->is_service_connected = 1;
		return GF_OK;
	}

	if (ext) {
		char *cgi_par = NULL;
		ext += 1;
		if (ext) {
			tmp = strchr(ext, '#');
			if (tmp) tmp[0] = 0;
			/* Warning the '?' sign should not be present in local files but it is convenient to have it
			   to test web content locally */
			cgi_par = strchr(ext, '?');
			if (cgi_par) cgi_par[0] = 0;
		}
		if (!stricmp(ext, "bt") || !stricmp(ext, "btz") || !stricmp(ext, "bt.gz")
		        || !stricmp(ext, "xmta")
		        || !stricmp(ext, "xmt") || !stricmp(ext, "xmt.gz") || !stricmp(ext, "xmtz")
		        || !stricmp(ext, "wrl") || !stricmp(ext, "wrl.gz")
		        || !stricmp(ext, "x3d") || !stricmp(ext, "x3d.gz") || !stricmp(ext, "x3dz")
		        || !stricmp(ext, "x3dv") || !stricmp(ext, "x3dv.gz") || !stricmp(ext, "x3dvz")
		        || !stricmp(ext, "swf")
		   )
			read->oti = GPAC_OTI_PRIVATE_SCENE_GENERIC;

		else if (!stricmp(ext, "svg") || !stricmp(ext, "svgz") || !stricmp(ext, "svg.gz")) {
			read->oti = GPAC_OTI_PRIVATE_SCENE_SVG;
		}
		/*XML LASeR*/
		else if (!stricmp(ext, "xsr"))
			read->oti = GPAC_OTI_PRIVATE_SCENE_LASER;
		else if (!stricmp(ext, "xbl"))
			read->oti = GPAC_OTI_PRIVATE_SCENE_XBL;
		else if (!stricmp(ext, "wgt") || !stricmp(ext, "mgt"))
			read->oti = GPAC_OTI_PRIVATE_SCENE_WGT;

		if (cgi_par) cgi_par[0] = '?';
	}

	if (!read->oti && (!strnicmp(url, "file://", 7) || !strstr(url, "://"))) {
		char *rtype = gf_xml_get_root_type(url, NULL);
		if (rtype) {
			if (!strcmp(rtype, "SAFSession")) read->oti = GPAC_OTI_PRIVATE_SCENE_LASER;
			else if (!strcmp(rtype, "svg")) read->oti = GPAC_OTI_PRIVATE_SCENE_SVG;
			else if (!strcmp(rtype, "XMT-A")) read->oti = GPAC_OTI_PRIVATE_SCENE_GENERIC;
			else if (!strcmp(rtype, "X3D")) read->oti = GPAC_OTI_PRIVATE_SCENE_GENERIC;
			else if (!strcmp(rtype, "bindings")) read->oti = GPAC_OTI_PRIVATE_SCENE_XBL;
			else if (!strcmp(rtype, "widget")) read->oti = GPAC_OTI_PRIVATE_SCENE_WGT;
			gf_free(rtype);
		}
	}

	/*remote fetch*/
	if (!strnicmp(url, "file://", 7)) {
		url += 7;
	}
	else if (strstr(url, "://")) {
		DC_DownloadFile(plug, read->url);
		return GF_OK;
	}

	test = gf_f64_open(read->url, "rt");
	if (!test) {
		gf_service_connect_ack(serv, NULL, GF_URL_ERROR);
		return GF_OK;
	}
	fclose(test);
	if (!read->is_service_connected) {
		gf_service_connect_ack(serv, NULL, GF_OK);
		read->is_service_connected = 1;
	}
	return GF_OK;
}

GF_Err DC_CloseService(GF_InputService *plug)
{
	DCReader *read = (DCReader *) plug->priv;
	if (read->dnload) gf_service_download_del(read->dnload);
	read->dnload = NULL;
	gf_service_disconnect_ack(read->service, NULL, GF_OK);
	return GF_OK;
}

/*Dummy input just send a file name, no multitrack to handle so we don't need to check sub_url nor expected type*/
static GF_Descriptor *DC_GetServiceDesc(GF_InputService *plug, u32 expect_type, const char *sub_url)
{
	u32 size = 0;
	char *uri;
	GF_ESD *esd;
	GF_BitStream *bs;
	DCReader *read = (DCReader *) plug->priv;
	GF_InitialObjectDescriptor *iod = (GF_InitialObjectDescriptor *) gf_odf_desc_new(GF_ODF_IOD_TAG);
	iod->scene_profileAndLevel = 1;
	iod->graphics_profileAndLevel = 1;
	iod->OD_profileAndLevel = 1;
	iod->audio_profileAndLevel = 0xFE;
	iod->visual_profileAndLevel = 0xFE;
	iod->objectDescriptorID = 1;

	if (read->is_views_url) {
		iod->URLString = gf_strdup(read->url);
		return (GF_Descriptor *)iod;
	}

	esd = gf_odf_desc_esd_new(0);
	esd->slConfig->timestampResolution = 1000;
	esd->slConfig->useTimestampsFlag = 1;
	esd->ESID = 0xFFFE;
	esd->decoderConfig->streamType = GF_STREAM_PRIVATE_SCENE;
	esd->decoderConfig->objectTypeIndication = read->oti;
	if (read->dnload) {
		uri = (char *) gf_dm_sess_get_cache_name(read->dnload);
		gf_dm_sess_get_stats(read->dnload, NULL, NULL, &size, NULL, NULL, NULL);
	} else {
		FILE *f = gf_f64_open(read->url, "rt");
		gf_f64_seek(f, 0, SEEK_END);
		size = (u32) gf_f64_tell(f);
		fclose(f);
		uri = read->url;
	}
	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_bs_write_u32(bs, size);
	gf_bs_write_data(bs, uri, (u32) strlen(uri));
	gf_bs_get_content(bs, &esd->decoderConfig->decoderSpecificInfo->data, &esd->decoderConfig->decoderSpecificInfo->dataLength);
	gf_bs_del(bs);

	gf_list_add(iod->ESDescriptors, esd);
	return (GF_Descriptor *)iod;
}


GF_Err DC_ServiceCommand(GF_InputService *plug, GF_NetworkCommand *com)
{
	DCReader *read = (DCReader *) plug->priv;
	DummyChannel *dc;

	if (!com->base.on_channel) return GF_NOT_SUPPORTED;

	dc = DC_GetChannel(read, com->base.on_channel);
	if (!dc) return GF_STREAM_NOT_FOUND;

	switch (com->command_type) {
	case GF_NET_CHAN_SET_PULL:
		return GF_OK;
	case GF_NET_CHAN_INTERACTIVE:
		return GF_OK;
	/*since data is file-based, no padding is needed (decoder plugin will handle it itself)*/
	case GF_NET_CHAN_SET_PADDING:
		return GF_OK;
	case GF_NET_CHAN_BUFFER:
		com->buffer.max = com->buffer.min = 0;
		return GF_OK;
	case GF_NET_CHAN_DURATION:
		/*this module is not made for updates, use undefined duration*/
		com->duration.duration = 0;
		return GF_OK;
	case GF_NET_CHAN_PLAY:
		dc->start = (u32) (1000 * com->play.start_range);
		dc->end = (u32) (1000 * com->play.end_range);
		return GF_OK;
	case GF_NET_CHAN_STOP:
		return GF_OK;
	case GF_NET_CHAN_CONFIG:
		return GF_OK;
	case GF_NET_CHAN_GET_DSI:
		com->get_dsi.dsi = NULL;
		com->get_dsi.dsi_len = 0;
		return GF_OK;
	default:
		break;
	}
	return GF_OK;
}

GF_Err DC_ConnectChannel(GF_InputService *plug, LPNETCHANNEL channel, const char *url, Bool upstream)
{
	u32 ESID;
	DCReader *read = (DCReader *) plug->priv;

	sscanf(url, "ES_ID=%ud", &ESID);
	if (!ESID) {
		gf_service_connect_ack(read->service, channel, GF_STREAM_NOT_FOUND);
	} else {
		DummyChannel *dc;
		GF_SAFEALLOC(dc, DummyChannel);
		dc->ch = channel;
		dc->ESID = ESID;
		gf_list_add(read->channels, dc);
		gf_service_connect_ack(read->service, channel, GF_OK);
	}
	return GF_OK;
}

GF_Err DC_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
	Bool had_ch;
	DCReader *read = (DCReader *) plug->priv;

	had_ch = DC_RemoveChannel(read, channel);
	gf_service_disconnect_ack(read->service, channel, had_ch ? GF_OK : GF_STREAM_NOT_FOUND);
	return GF_OK;
}

GF_Err DC_ChannelGetSLP(GF_InputService *plug, LPNETCHANNEL channel, char **out_data_ptr, u32 *out_data_size, GF_SLHeader *out_sl_hdr, Bool *sl_compressed, GF_Err *out_reception_status, Bool *is_new_data)
{
	DummyChannel *dc;
	DCReader *read = (DCReader *) plug->priv;
	dc = DC_GetChannel(read, channel);
	if (!dc) return GF_STREAM_NOT_FOUND;

	memset(out_sl_hdr, 0, sizeof(GF_SLHeader));
	out_sl_hdr->compositionTimeStampFlag = 1;
	out_sl_hdr->compositionTimeStamp = dc->start;
	out_sl_hdr->accessUnitStartFlag = 1;
	*sl_compressed = 0;
	*out_reception_status = GF_OK;
	*is_new_data = 1;
	return GF_OK;
}

GF_Err DC_ChannelReleaseSLP(GF_InputService *plug, LPNETCHANNEL channel)
{
	return GF_OK;
}

Bool DC_CanHandleURLInService(GF_InputService *plug, const char *url)
{
	return 0;
}

GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
		GF_NET_CLIENT_INTERFACE,
		0
	};
	return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	DCReader *read;
	GF_InputService *plug;
	if (InterfaceType != GF_NET_CLIENT_INTERFACE) return NULL;

	GF_SAFEALLOC(plug, GF_InputService);
	memset(plug, 0, sizeof(GF_InputService));
	GF_REGISTER_MODULE_INTERFACE(plug, GF_NET_CLIENT_INTERFACE, "GPAC Dummy Loader", "gpac distribution")

	plug->RegisterMimeTypes = DC_RegisterMimeTypes;
	plug->CanHandleURL = DC_CanHandleURL;
	plug->ConnectService = DC_ConnectService;
	plug->CloseService = DC_CloseService;
	plug->GetServiceDescriptor = DC_GetServiceDesc;
	plug->ConnectChannel = DC_ConnectChannel;
	plug->DisconnectChannel = DC_DisconnectChannel;
	plug->ServiceCommand = DC_ServiceCommand;
	plug->CanHandleURLInService = DC_CanHandleURLInService;
	plug->ChannelGetSLP = DC_ChannelGetSLP;
	plug->ChannelReleaseSLP = DC_ChannelReleaseSLP;
	GF_SAFEALLOC(read, DCReader);
	read->channels = gf_list_new();
	plug->priv = read;
	return (GF_BaseInterface *)plug;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *bi)
{
	GF_InputService *ifcn = (GF_InputService*)bi;
	if (ifcn->InterfaceType==GF_NET_CLIENT_INTERFACE) {
		DCReader *read = (DCReader*)ifcn->priv;
		assert(!gf_list_count(read->channels));
		gf_list_del(read->channels);
		if( read->url) gf_free(read->url);
		gf_free(read);
		gf_free(bi);
	}
}

GPAC_MODULE_STATIC_DELARATION( dummy_in )
