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


#include <gpac/filters.h>
/*for GF_STREAM_PRIVATE_SCENE definition*/
#include <gpac/constants.h>
#include <gpac/download.h>
#include <gpac/xml.h>

typedef struct
{
	//options
	const char *src;
	u32 buffer_size;

	//internal

	u32 oti;
	Bool is_views_url;

	//only one output pid declared
	GF_FilterPid *pid;

#ifdef FILTER_FIXME
	/*file downloader*/
	GF_DownloadSession * dnload;
#endif

	Bool is_service_connected;

	FILE *file;
	u64 file_pos;
	u64 file_size;
	u32 start, end;
} GF_FileInCtx;


#ifdef FILTER_FIXME
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
		return GF_FALSE;
	sExt = strrchr(url, '.');
	if (sExt) {
		Bool ok = GF_FALSE;
		char *cgi_par;
		if (!strnicmp(sExt, ".gz", 3)) sExt = strrchr(sExt, '.');
		if (!strnicmp(url, "rtsp://", 7)) return GF_FALSE;

		cgi_par = strchr(sExt, '?');
		if (cgi_par) cgi_par[0] = 0;
		{
			u32 i;
			for (i = 0 ; DC_MIME_TYPES[i] ; i+=3)
				if (0 != (ok = gf_service_check_mime_register(plug, DC_MIME_TYPES[i], DC_MIME_TYPES[i+1], DC_MIME_TYPES[i+2], sExt)))
					break;
		}
		if (cgi_par) cgi_par[0] = '?';
		if (ok) return GF_TRUE;
	}
	/*views:// internal URI*/
	if (!strnicmp(url, "views://", 8))
		return GF_TRUE;

	if (!strncmp(url, "\\\\", 2)) return GF_FALSE;

	if (!strnicmp(url, "file://", 7) || !strstr(url, "://")) {
		char *rtype = gf_xml_get_root_type(url, NULL);
		if (rtype) {
			Bool handled = GF_FALSE;
			if (!strcmp(rtype, "SAFSession")) handled = GF_TRUE;
			else if (!strcmp(rtype, "XMT-A")) handled = GF_TRUE;
			else if (!strcmp(rtype, "X3D")) handled = GF_TRUE;
			else if (!strcmp(rtype, "svg")) handled = GF_TRUE;
			else if (!strcmp(rtype, "bindings")) handled = GF_TRUE;
			else if (!strcmp(rtype, "widget")) handled = GF_TRUE;
			gf_free(rtype);
			return handled;
		}
	}
	return GF_FALSE;
}

void DC_NetIO(void *cbk, GF_NETIO_Parameter *param)
{
	GF_Err e;
	GF_FileInCtx *read = (GF_FileInCtx *) cbk;

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
		if (e>0) e = GF_OK;
		gf_service_connect_ack(read->service, NULL, e);
		read->is_service_connected = GF_TRUE;
	}
}

void DC_DownloadFile(GF_InputService *plug, char *url)
{
	GF_FileInCtx *read = (GF_FileInCtx *) plug->priv;

	read->dnload = gf_service_download_new(read->service, url, 0, DC_NetIO, read);
	if (!read->dnload) {
		gf_service_connect_ack(read->service, NULL, GF_NOT_SUPPORTED);
	} else {
		/*start our download (threaded)*/
		gf_dm_sess_process(read->dnload);
	}
}

#endif


GF_Err file_in_initialize(GF_Filter *filter)
{
	GF_FileInCtx *ctx = (GF_FileInCtx *) gf_filter_get_udta(filter);
	char *tmp, *ext;

	if (!ctx || !ctx->src) return GF_BAD_PARAM;

#ifdef FILTER_FIXME
	if (read->dnload) gf_service_download_del(read->dnload);
	read->dnload = NULL;
#endif

	ext = strchr(ctx->src, '#');
	if (ext) {
		char *anext;
		ext[0] = 0;
		anext = strrchr(ctx->src, '.');
		ext[0] = '#';
		ext = anext;
	} else {
		ext = strrchr(ctx->src, '.');
	}
	if (ext && !stricmp(ext, ".gz")) {
		char *anext;
		ext[0] = 0;
		anext = strrchr(ctx->src, '.');
		ext[0] = '.';
		ext = anext;
	}

	if (!strnicmp(ctx->src, "views://", 8)) {
		ctx->is_views_url = GF_TRUE;
		//gf_service_connect_ack(serv, NULL, GF_OK);
		ctx->is_service_connected = GF_TRUE;
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
			ctx->oti = GPAC_OTI_PRIVATE_SCENE_GENERIC;

		else if (!stricmp(ext, "svg") || !stricmp(ext, "svgz") || !stricmp(ext, "svg.gz")) {
			ctx->oti = GPAC_OTI_PRIVATE_SCENE_SVG;
		}
		/*XML LASeR*/
		else if (!stricmp(ext, "xsr"))
			ctx->oti = GPAC_OTI_PRIVATE_SCENE_LASER;
		else if (!stricmp(ext, "xbl"))
			ctx->oti = GPAC_OTI_PRIVATE_SCENE_XBL;
		else if (!stricmp(ext, "wgt") || !stricmp(ext, "mgt"))
			ctx->oti = GPAC_OTI_PRIVATE_SCENE_WGT;

		if (cgi_par) cgi_par[0] = '?';
	}

	if (!ctx->oti && (!strnicmp(ctx->src, "file://", 7) || !strstr(ctx->src, "://"))) {
		char *rtype = gf_xml_get_root_type(ctx->src, NULL);
		if (rtype) {
			if (!strcmp(rtype, "SAFSession")) ctx->oti = GPAC_OTI_PRIVATE_SCENE_LASER;
			else if (!strcmp(rtype, "svg")) ctx->oti = GPAC_OTI_PRIVATE_SCENE_SVG;
			else if (!strcmp(rtype, "XMT-A")) ctx->oti = GPAC_OTI_PRIVATE_SCENE_GENERIC;
			else if (!strcmp(rtype, "X3D")) ctx->oti = GPAC_OTI_PRIVATE_SCENE_GENERIC;
			else if (!strcmp(rtype, "bindings")) ctx->oti = GPAC_OTI_PRIVATE_SCENE_XBL;
			else if (!strcmp(rtype, "widget")) ctx->oti = GPAC_OTI_PRIVATE_SCENE_WGT;
			gf_free(rtype);
		}
	}

	/*remote fetch*/
	if (!strnicmp(ctx->src, "file://", 7)) {
	}
	else if (strstr(ctx->src, "://")) {
#ifdef FILTER_FIXME
		DC_DownloadFile(plug, read->url);
		return GF_OK;
#else
		gf_filter_setup_failure(filter, GF_NOT_SUPPORTED);
		return GF_NOT_SUPPORTED;
#endif
	}

	ctx->file = gf_fopen(ctx->src, "rt");
	if (!ctx->file) {
		gf_filter_setup_failure(filter, GF_URL_ERROR);
		return GF_URL_ERROR;
	}
	gf_fseek(ctx->file, 0, SEEK_END);
	ctx->file_size = gf_ftell(ctx->file);
	gf_fseek(ctx->file, 0, SEEK_SET);

	if (!ctx->is_service_connected) {
		ctx->is_service_connected = GF_TRUE;
	}

	//declare pid
	ctx->pid = gf_filter_pid_new(filter);

	gf_filter_pid_set_property(ctx->pid, GF_PROP_PID_ID, &PROP_UINT(0xFFFE));
	gf_filter_pid_set_property(ctx->pid, GF_PROP_PID_CLOCK_ID, &PROP_UINT(0xFFFE));
	gf_filter_pid_set_property(ctx->pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_PRIVATE_SCENE));
	gf_filter_pid_set_property(ctx->pid, GF_PROP_PID_OTI, &PROP_UINT(ctx->oti));
	gf_filter_pid_set_property(ctx->pid, GF_PROP_PID_TIMESCALE, &PROP_UINT(1000));
	gf_filter_pid_set_property(ctx->pid, GF_PROP_PID_IN_IOD, &PROP_BOOL(GF_TRUE));

	return GF_OK;
}

GF_Err file_in_finalize(GF_Filter *filter)
{
	GF_FileInCtx *ctx = (GF_FileInCtx *) gf_filter_get_udta(filter);
#ifdef FILTER_FIXME
	if (ctx->dnload) gf_service_download_del(ctx->dnload);
	ctx->dnload = NULL;
#endif
	return GF_OK;
}

static Bool file_in_process_event(GF_Filter *filter, GF_FilterEvent *com)
{
	GF_FileInCtx *ctx = (GF_FileInCtx *) gf_filter_get_udta(filter);

	if (!com->base.on_pid) return GF_FALSE;
	if (com->base.on_pid != ctx->pid) return GF_FALSE;

	switch (com->base.type) {
	case GF_FEVT_PLAY:
		ctx->start = (u32) (1000 * com->play.start_range);
		ctx->end = (u32) (1000 * com->play.end_range);
		ctx->file_pos = 0;
		gf_fseek(ctx->file, 0, SEEK_SET);
		return GF_TRUE;
	case GF_FEVT_STOP:
		return GF_TRUE;
	default:
		break;
	}
	return GF_FALSE;
}

static GF_Err isoffin_process(GF_Filter *filter)
{
	Bool is_start, is_end;
	u32 nb_read, to_read;
	GF_FilterPacket *pck;
	char *pck_data;
	GF_FileInCtx *ctx = (GF_FileInCtx *) gf_filter_get_udta(filter);

	if (gf_filter_pid_would_block(ctx->pid))
		return GF_OK;

	is_start = ctx->file_pos ? GF_FALSE : GF_TRUE;
	to_read = ctx->file_size - ctx->file_pos;
	if (to_read > ctx->buffer_size) {
		to_read = ctx->buffer_size;
		is_end = GF_FALSE;
	} else {
		is_end = GF_TRUE;
	}

	pck = gf_filter_pck_new_alloc(ctx->pid, to_read, &pck_data);
	if (!pck) return GF_OK;

	gf_filter_pck_set_cts(pck, ctx->start);

	nb_read = fread(pck_data, 1, to_read, ctx->file);

	if (nb_read != to_read) return GF_IO_ERR;
	if (!ctx->file_pos) is_start = GF_TRUE;

	gf_filter_pck_set_cts(pck, ctx->start);

	memset(out_sl_hdr, 0, sizeof(GF_SLHeader));
	out_sl_hdr->compositionTimeStampFlag = 1;
	out_sl_hdr->compositionTimeStamp = dc->start;
	out_sl_hdr->accessUnitStartFlag = 1;
	*sl_compressed = GF_FALSE;
	*out_reception_status = GF_OK;
	*is_new_data = GF_TRUE;
	return GF_OK;
}

Bool DC_CanHandleURLInService(GF_InputService *plug, const char *url)
{
	return GF_FALSE;
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
	GF_FileInCtx *read;
	GF_InputService *plug;
	if (InterfaceType != GF_NET_CLIENT_INTERFACE) return NULL;

	GF_SAFEALLOC(plug, GF_InputService);
	if (!plug) return NULL;
	GF_SAFEALLOC(read, GF_FileInCtx);
	if (!read) {
		gf_free(plug);
		return NULL;
	}
	read->channels = gf_list_new();
	plug->priv = read;

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

	return (GF_BaseInterface *)plug;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *bi)
{
	GF_InputService *ifcn = (GF_InputService*)bi;
	if (ifcn->InterfaceType==GF_NET_CLIENT_INTERFACE) {
		GF_FileInCtx *read = (GF_FileInCtx*)ifcn->priv;
		assert(!gf_list_count(read->channels));
		gf_list_del(read->channels);
		if( read->url) gf_free(read->url);
		gf_free(read);
		gf_free(bi);
	}
}

GPAC_MODULE_STATIC_DECLARATION( dummy_in )
