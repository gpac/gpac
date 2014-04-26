/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / RTP input module
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

#include "rtp_in.h"

#ifndef GPAC_DISABLE_STREAMING


void RP_SDPFromData(RTPClient *rtp, char *s_url, RTPStream *stream)
{
	char *url;
	char buf[2000];
	u32 size;

	url = strstr(s_url, ",");
	if (!url) {
		gf_term_on_connect(rtp->service, NULL, GF_URL_ERROR);
		return;
	}
	url += 1;
	if (strstr(url, ";base64")) {
		//decode
		size = gf_base64_decode(url, (u32) strlen(url), buf, 2000);
		buf[size] = 0;
		url = buf;
	}
	RP_LoadSDP(rtp, url, (u32) strlen(url), stream);
}

void RP_SDPFromFile(RTPClient *rtp, char *file_name, RTPStream *stream)
{
	FILE *_sdp;
	char *sdp_buf;
	u32 sdp_size;

	sdp_buf = NULL;

	if (file_name && strstr(file_name, "file://")) file_name += strlen("file://");
	if (!file_name || !(_sdp = gf_f64_open(file_name, "rt")) ) {
		gf_term_on_connect(rtp->service, NULL, GF_URL_ERROR);
		return;
	}

	gf_f64_seek(_sdp, 0, SEEK_END);
	sdp_size = (u32) gf_f64_tell(_sdp);
	gf_f64_seek(_sdp, 0, SEEK_SET);
	sdp_buf = (char*)gf_malloc(sdp_size);
	if (1 > fread(sdp_buf, 1, sdp_size, _sdp)) {
		gf_term_on_connect(rtp->service, NULL, GF_URL_ERROR);
	} else {
		RP_LoadSDP(rtp, sdp_buf, sdp_size, stream);
	}
	fclose(_sdp);
	gf_free(sdp_buf);
}

void SDP_NetIO(void *cbk, GF_NETIO_Parameter *param)
{
	GF_Err e;
	RTPClient *rtp = (RTPClient *)cbk;
	SDPFetch *sdp = rtp->sdp_temp;

	gf_term_download_update_stats(rtp->dnload);

	e = param->error;
	switch (param->msg_type) {
	case GF_NETIO_GET_METHOD:
		if (sdp->original_url)
			param->name = "POST";
		return;
	case GF_NETIO_GET_CONTENT:
		if (sdp->original_url) {
			char szBody[4096], *opt;
			opt = (char *) gf_modules_get_option((GF_BaseInterface *) gf_term_get_service_interface(rtp->service), "Network", "MobileIP");
			sprintf(szBody, "ipadd\n%s\n\nurl\n%s\n\n", opt, sdp->original_url);
			param->data = szBody;
			param->size = (u32) strlen(szBody);
		}
		return;
	case GF_NETIO_DATA_TRANSFERED:
		if (sdp->original_url) {
			u32 sdp_size;
			e = gf_dm_sess_get_stats(rtp->dnload, NULL, NULL, &sdp_size, NULL, NULL, NULL);
			if (sdp_size) {
				const char *szFile = gf_dm_sess_get_cache_name(rtp->dnload);
				if (!szFile) {
					e = GF_SERVICE_ERROR;
				} else {
					e = GF_OK;
					RP_SDPFromFile(rtp, (char *) szFile, sdp->chan);
					gf_free(sdp->remote_url);
					if (sdp->original_url) gf_free(sdp->original_url);
					gf_free(sdp);
					rtp->sdp_temp = NULL;
					return;
				}
			}
		}
		break;
	default:
		if (e == GF_OK) return;
	}

	if (sdp->original_url) {
		char *url = sdp->original_url;
		gf_free(sdp->remote_url);
		gf_free(sdp);
		rtp->sdp_temp = NULL;
		gf_term_on_message(rtp->service, e, "Error fetching session state - restarting");
		RP_ConnectServiceEx(gf_term_get_service_interface(rtp->service), rtp->service, url, 1);
		gf_free(url);
		return;
	}

	/*error*/
	if (sdp->chan) {
		gf_term_on_connect(rtp->service, sdp->chan->channel, e);
	} else {
		gf_term_on_connect(rtp->service, NULL, e);
		rtp->sdp_temp = NULL;
	}
	gf_free(sdp->remote_url);
	if (sdp->original_url) gf_free(sdp->original_url);
	gf_free(sdp);
	rtp->sdp_temp = NULL;
}

void RP_FetchSDP(RTPClient *rtp, char *url, RTPStream *stream, char *original_url)
{
	SDPFetch *sdp;
	/*if local URL get file*/
	if (strstr(url, "data:application/sdp")) {
		RP_SDPFromData(rtp, url, stream);
		return;
	}
	if (!strnicmp(url, "file://", 7) || !strstr(url, "://")) {
		RP_SDPFromFile(rtp, url, stream);
		return;
	}

	sdp = (SDPFetch*)gf_malloc(sizeof(SDPFetch));
	memset(sdp, 0, sizeof(SDPFetch));
	sdp->client = rtp;
	sdp->remote_url = gf_strdup(url);
	sdp->chan = stream;
	if (original_url) {
		sdp->original_url = gf_strdup(original_url);
	}

	/*otherwise setup download*/
	if (rtp->dnload) gf_term_download_del(rtp->dnload);
	rtp->dnload = NULL;

	rtp->sdp_temp = sdp;
	rtp->dnload = gf_term_download_new(rtp->service, url, 0, SDP_NetIO, rtp);
	if (!rtp->dnload) {
		gf_term_on_connect(rtp->service, NULL, GF_NOT_SUPPORTED);
	} else {
		/*start our download (threaded)*/
		gf_dm_sess_process(rtp->dnload);
	}
	/*service confirm is done once fetched*/
}

#endif /*GPAC_DISABLE_STREAMING*/
