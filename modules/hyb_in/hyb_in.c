/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Romain Bouqueau
 *			Copyright (c) Telecom ParisTech 2010-20xx
 *					All rights reserved
 *
 *  This file is part of GPAC / Hybrid Media input module
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

#include "hyb_in.h"

/*register by hand existing masters*/
extern GF_HYBMEDIA master_fm_mmbtools;
extern GF_HYBMEDIA master_fm_fake_pull;
extern GF_HYBMEDIA master_fm_fake_push;

GF_HYBMEDIA* hyb_masters[] = {
	&master_fm_fake_pull,
	&master_fm_fake_push,
#ifdef GPAC_ANDROID
	&master_fm_mmbtools,
#endif
};

typedef struct {
	/*GPAC Service object (i.e. how this module is seen by the terminal)*/
	GF_ClientService *service;

	/*This Hybrid media architecture assumes there is a master media*/
	GF_HYBMEDIA* master;

} GF_HYB_In;


static u32 HYB_RegisterMimeTypes(const GF_InputService *plug)
{
	if (!plug)
		return 0;

	return 1;
}

static Bool HYB_CanHandleURL(GF_InputService *plug, const char *url)
{
	u32 i;
	const size_t nb_masters = sizeof(hyb_masters) / sizeof(GF_HYBMEDIA*);

	for (i=0; i<nb_masters; i++) {
		if (hyb_masters[i]->CanHandleURL(url))
			return 1;
	}

	return 0;
}

static GF_Err hybmedia_sanity_check(GF_HYBMEDIA *master)
{
	/*these checks need to be upgraded when the interface changes*/
	if (master->data_mode != HYB_PUSH && master->data_mode != HYB_PULL)
		return GF_BAD_PARAM;

	if (!master->name) goto error_exit;
	if (!master->CanHandleURL) goto error_exit;
	if (!master->GetOD) goto error_exit;
	if (!master->Connect) goto error_exit;
	if (!master->Disconnect) goto error_exit;

	if (master->data_mode == HYB_PUSH) {
		if (!master->SetState) goto error_exit;
	}

	if (master->data_mode == HYB_PULL) {
		if (!master->GetData) goto error_exit;
		if (!master->ReleaseData) goto error_exit;
	}

	return GF_OK;

error_exit:
	return GF_SERVICE_ERROR;
}

static GF_Err HYB_ConnectService(GF_InputService *plug, GF_ClientService *serv, const char *url)
{
	u32 i;
	GF_Err e = GF_OK;
	const size_t nb_masters = sizeof(hyb_masters) / sizeof(GF_HYBMEDIA*);

	GF_HYB_In *hyb_in = (GF_HYB_In*)plug->priv;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[HYB_IN] Received Connection request from service %p for %s\n", serv, url));

	if (!hyb_in || !serv || !url) return GF_BAD_PARAM;
	hyb_in->service = serv;

	/*choose the master service*/
	for (i=0; i<nb_masters; i++) {
		if (hyb_masters[i]->CanHandleURL(url)) {
			hyb_in->master = hyb_masters[i];
			break;
		}
	}
	assert(hyb_in->master);

	/*sanity check about the master*/
	e = hybmedia_sanity_check(hyb_in->master);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[HYB_IN] Error - object \"%s\" failed the sanity checks\n", hyb_in->master->name));
		gf_term_on_connect(hyb_in->service, NULL, e);
		return e;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[HYB_IN] Selected master object \"%s\" for URL: %s\n", hyb_in->master->name, url));

	/*connect the master*/
	e = hyb_in->master->Connect(hyb_in->master, hyb_in->service, url);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[HYB_IN] Error - cannot connect service, wrong URL %s\n", url));
		gf_term_on_connect(hyb_in->service, NULL, GF_BAD_PARAM);
		return e;
	}
	gf_term_on_connect(hyb_in->service, NULL, GF_OK);
	gf_term_add_media(hyb_in->service, (GF_Descriptor*)hyb_in->master->GetOD(), 0);

	return GF_OK;
}

static GF_Err HYB_CloseService(GF_InputService *plug)
{
	GF_Err e;
	GF_HYB_In *hyb_in = (GF_HYB_In*)plug->priv;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[HYB_IN] Received Close Service (%p) request from terminal\n", ((GF_HYB_In*)plug->priv)->service));

	/*force to stop and disconnect the master*/
	hyb_in->master->SetState(hyb_in->master, GF_NET_CHAN_STOP);
	e = hyb_in->master->Disconnect(hyb_in->master);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[HYB_IN] Error - cannot disconnect service %p\n", hyb_in->service));
		gf_term_on_connect(hyb_in->service, NULL, GF_BAD_PARAM);
		return e;
	}

	gf_term_on_disconnect(hyb_in->service, NULL, GF_OK);

	return GF_OK;
}

static GF_Descriptor *HYB_GetServiceDesc(GF_InputService *plug, u32 expect_type, const char *sub_url)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[HYB_IN] Received Service Description request from terminal for %s\n", sub_url));

	return NULL;
}

static GF_Err HYB_ConnectChannel(GF_InputService *plug, LPNETCHANNEL channel, const char *url, Bool upstream)
{
	GF_HYB_In *hyb_in;
	GF_HYBMEDIA *master;

	if (!plug || !plug->priv)
		return GF_SERVICE_ERROR;

	hyb_in = (GF_HYB_In*)plug->priv;
	master = (GF_HYBMEDIA*)hyb_in->master;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[HYB_IN] Received Channel Connection request from service %p for %s\n", channel, url));

	master->channel = channel;
	gf_term_on_connect(hyb_in->service, channel, GF_OK);

	return GF_OK;
}

static GF_Err HYB_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
	GF_HYB_In *hyb_in = (GF_HYB_In*)plug->priv;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[HYB_IN] Received Channel Disconnect Service (%p) request from terminal\n", hyb_in->service));

	gf_term_on_disconnect(hyb_in->service, channel, GF_OK);
	hyb_in->master->channel = NULL;

	return GF_OK;
}

static GF_Err HYB_ServiceCommand(GF_InputService *plug, GF_NetworkCommand *com)
{
	GF_HYB_In *hyb_in = (GF_HYB_In*)plug->priv;

	switch (com->command_type) {
	case GF_NET_CHAN_SET_SPEED:
		/*not implemented for push mode*/
		assert(hyb_in->master->data_mode == HYB_PULL);
		return GF_OK;
	case GF_NET_CHAN_INTERACTIVE:
		return GF_NOT_SUPPORTED;
	case GF_NET_CHAN_BUFFER:
		com->buffer.max = com->buffer.min = 0;
		return GF_OK;
	case GF_NET_CHAN_DURATION:
		com->duration.duration = 0;
		return GF_OK;
	case GF_NET_CHAN_PLAY:
	case GF_NET_CHAN_STOP:
	case GF_NET_CHAN_PAUSE:
	case GF_NET_CHAN_RESUME:
		if (hyb_in->master->data_mode == HYB_PUSH)
			return hyb_in->master->SetState(hyb_in->master, com->command_type);
		return GF_OK;
	case GF_NET_CHAN_SET_PULL:
		if (hyb_in->master->data_mode == HYB_PULL)
			return GF_OK;
		else
			return GF_NOT_SUPPORTED; /*we're in push mode*/
	}

	return GF_OK;
}

static Bool HYB_CanHandleURLInService(GF_InputService *plug, const char *url)
{
	return 0;
}

static GF_Err HYB_ChannelGetSLP(GF_InputService *plug, LPNETCHANNEL channel, char **out_data_ptr, u32 *out_data_size, GF_SLHeader *out_sl_hdr, Bool *sl_compressed, GF_Err *out_reception_status, Bool *is_new_data)
{
	GF_HYB_In *hyb_in = (GF_HYB_In*)plug->priv;
	assert(hyb_in->master->data_mode == HYB_PULL && hyb_in->master->GetData && hyb_in->master->ReleaseData);

	assert(((GF_HYB_In*)plug->priv)->master->channel == channel);
	hyb_in->master->GetData(hyb_in->master, out_data_ptr, out_data_size, out_sl_hdr);
	*sl_compressed = 0;
	*out_reception_status = GF_OK;
	*is_new_data = 1;
	return GF_OK;
}

static GF_Err HYB_ChannelReleaseSLP(GF_InputService *plug, LPNETCHANNEL channel)
{
	GF_HYB_In *hyb_in = (GF_HYB_In*)plug->priv;
	assert(((GF_HYB_In*)plug->priv)->master->channel == channel);
	return GF_OK;
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
	GF_HYB_In *hyb_in;
	GF_InputService *plug;
	if (InterfaceType != GF_NET_CLIENT_INTERFACE) return NULL;

	GF_SAFEALLOC(plug, GF_InputService);
	GF_REGISTER_MODULE_INTERFACE(plug, GF_NET_CLIENT_INTERFACE, "GPAC HYBRID MEDIA Loader", "gpac distribution")
	plug->RegisterMimeTypes=	HYB_RegisterMimeTypes;
	plug->CanHandleURL=			HYB_CanHandleURL;
	plug->ConnectService=		HYB_ConnectService;
	plug->CloseService=			HYB_CloseService;
	plug->GetServiceDescriptor=	HYB_GetServiceDesc;
	plug->ConnectChannel=		HYB_ConnectChannel;
	plug->DisconnectChannel=	HYB_DisconnectChannel;
	plug->ServiceCommand=		HYB_ServiceCommand;
	plug->CanHandleURLInService=HYB_CanHandleURLInService;
	plug->ChannelGetSLP=		HYB_ChannelGetSLP;
	plug->ChannelReleaseSLP=	HYB_ChannelReleaseSLP;

	GF_SAFEALLOC(hyb_in, GF_HYB_In);
	plug->priv = hyb_in;

	return (GF_BaseInterface *)plug;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	GF_LOG(GF_LOG_MEDIA, GF_LOG_ERROR, ("DeleteLoaderInterface %p: 1\n", ifce));
	if (ifce->InterfaceType == GF_NET_CLIENT_INTERFACE) {
		GF_InputService *plug = (GF_InputService*)ifce;
		GF_HYB_In *hyb_in = (GF_HYB_In*)plug->priv;
		gf_free(hyb_in);
		plug->priv = NULL;
		gf_free(plug);
		GF_LOG(GF_LOG_MEDIA, GF_LOG_ERROR, ("DeleteLoaderInterface %p: 2\n", ifce));
	}
}

GPAC_MODULE_STATIC_DELARATION( hyb_in )
