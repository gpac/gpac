#include "V4Service.h"

#include <gpac/constants.h>

#define V4_PRIVATE_SCENE_OTI 0xFF

extern "C" {

	
V4Channel *V4Service::V4_GetChannel(V4Service *v4service, LPNETCHANNEL ch)
{
	u32 i;
	for (i=0; i<gf_list_count(v4service->channels); i++) {
		V4Channel *v4c = (V4Channel *)gf_list_get(v4service->channels, i);
		if (v4c->ch && v4c->ch==ch) return v4c;
	}
	return NULL;
}

Bool V4Service::V4_RemoveChannel(V4Service *v4service, LPNETCHANNEL ch)
{
	u32 i;
	for (i=0; i<gf_list_count(v4service->channels); i++) {
		V4Channel *v4c = (V4Channel *)gf_list_get(v4service->channels, i);
		if (v4c->ch && v4c->ch==ch) {
			gf_list_rem(v4service->channels, i);
			free(v4c);
			return 1;
		}
	}
	return 0;
}

static Bool V4_CanHandleURL(GF_InputService *plug, const char *url)
{
	return 1;
}

static GF_Err V4_ConnectService (GF_InputService *plug, GF_ClientService *serv, const char *url)
{
	V4Service *v4serv = (V4Service *)plug->priv;
	v4serv->SetService(serv);
	gf_term_on_connect(v4serv->GetService(), NULL, GF_OK);
	return GF_OK;
}

static GF_Err V4_CloseService(GF_InputService *plug)
{
	V4Service *v4serv = (V4Service *)plug->priv;
	gf_term_on_disconnect(v4serv->GetService(), NULL, GF_OK);
	return GF_OK;
}

static GF_Descriptor *V4_GetServiceDesc(GF_InputService *plug, u32 expect_type, const char *sub_url)
{
	V4Service *v4service= (V4Service *) plug->priv;
	GF_ESD *esd;
	GF_InitialObjectDescriptor *iod = (GF_InitialObjectDescriptor *) gf_odf_desc_new(GF_ODF_IOD_TAG);
	iod->scene_profileAndLevel = 1;
	iod->graphics_profileAndLevel = 1;
	iod->OD_profileAndLevel = 1;
	iod->audio_profileAndLevel = 0xFE;
	iod->visual_profileAndLevel = 0xFE;
	iod->objectDescriptorID = 1;

	esd = gf_odf_desc_esd_new(0);
	esd->slConfig->timestampResolution = 1000;
	esd->slConfig->useTimestampsFlag = 1;
	esd->ESID = 0xFFFE;
	esd->decoderConfig->streamType = GF_STREAM_PRIVATE_SCENE;
	esd->decoderConfig->objectTypeIndication = 0x01; //V4_PRIVATE_SCENE_OTI; 
	if (v4service->GetPath()) {
		esd->decoderConfig->decoderSpecificInfo->dataLength = strlen(v4service->GetPath()) + 1;
		esd->decoderConfig->decoderSpecificInfo->data = strdup(v4service->GetPath());
	} 
	gf_list_add(iod->ESDescriptors, esd);
	return (GF_Descriptor *)iod;
}

static GF_Err V4_ConnectChannel(GF_InputService *plug, LPNETCHANNEL channel, const char *url, Bool upstream)
{
	u32 ESID;
	V4Service *v4serv = (V4Service *)plug->priv;
	
	sscanf(url, "ES_ID=%d", &ESID);
	if (!ESID) {
		gf_term_on_connect(v4serv->GetService(), channel, GF_STREAM_NOT_FOUND);
	} else {
		V4Channel *v4c = (V4Channel *)malloc(sizeof(V4Channel));
		memset(v4c, 0, sizeof(V4Channel));
		v4c->ch = channel;
		v4c->ESID = ESID;
		gf_list_add(v4serv->GetChannels(), v4c);
		gf_term_on_connect(v4serv->GetService(), channel, GF_OK);
	}
	return GF_OK;
}

static GF_Err V4_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
	V4Service *v4serv = (V4Service *)plug->priv;
	Bool had_ch;

	had_ch = v4serv->V4_RemoveChannel(v4serv, channel);
	gf_term_on_disconnect(v4serv->GetService(), channel, had_ch ? GF_OK : GF_STREAM_NOT_FOUND);
	return GF_OK;
}

static GF_Err V4_ServiceCommand(GF_InputService *plug, GF_NetworkCommand *com)
{
	V4Service *v4serv = (V4Service *)plug->priv;
	V4Channel *v4c;

	if (!com->base.on_channel) return GF_OK;

	v4c = v4serv->V4_GetChannel(v4serv, com->base.on_channel);
	if (!v4c) return GF_STREAM_NOT_FOUND;

	switch (com->command_type) {
	case GF_NET_CHAN_SET_PULL: return GF_OK;
	case GF_NET_CHAN_INTERACTIVE: return GF_OK;
	case GF_NET_CHAN_SET_PADDING: return GF_NOT_SUPPORTED;
	case GF_NET_CHAN_BUFFER:
		com->buffer.max = com->buffer.min = 0;
		return GF_OK;
	case GF_NET_CHAN_DURATION:
		/*this is not made for updates, use undefined duration*/
		com->duration.duration = 0;
		return GF_OK;
	case GF_NET_CHAN_PLAY:
		v4c->start = (u32) (1000 * com->play.start_range);
		v4c->end = (u32) (1000 * com->play.end_range);
		return GF_OK;
	case GF_NET_CHAN_STOP:
		return GF_OK;
	case GF_NET_CHAN_CONFIG: return GF_OK;
	case GF_NET_CHAN_GET_DSI:
		com->get_dsi.dsi = NULL;
		com->get_dsi.dsi_len = 0;
		return GF_OK;
	}
	return GF_OK;
}

static GF_Err V4_ChannelGetSLP(GF_InputService *plug, LPNETCHANNEL channel, char **out_data_ptr, u32 *out_data_size, GF_SLHeader *out_sl_hdr, Bool *sl_compressed, GF_Err *out_reception_status, Bool *is_new_data)
{
	V4Channel *v4c;
	V4Service *v4serv = (V4Service *)plug->priv;
	v4c = v4serv->V4_GetChannel(v4serv, channel);
	if (!v4c) return GF_STREAM_NOT_FOUND;

	memset(out_sl_hdr, 0, sizeof(GF_SLHeader));
	out_sl_hdr->compositionTimeStampFlag = 1;
	out_sl_hdr->compositionTimeStamp = v4c->start;
	out_sl_hdr->accessUnitStartFlag = 1;
	*sl_compressed = 0;
	*out_reception_status = GF_OK;
	*is_new_data = 1;
	return GF_OK;	return GF_OK;
}

static GF_Err V4_ChannelReleaseSLP(GF_InputService *plug, LPNETCHANNEL channel)
{
	return GF_OK;
}

static Bool V4_CanHandleURLInService(GF_InputService *plug, const char *url)
{
	return 0;
}

} // extern "C"

V4Service::V4Service(const char *path) 
{

	m_pNetClient = (GF_InputService *)malloc(sizeof(GF_InputService));
	m_pNetClient->priv = this;

	m_pNetClient->CanHandleURL			= V4_CanHandleURL;

	m_pNetClient->ConnectService		= V4_ConnectService;
	m_pNetClient->CloseService			= V4_CloseService;
	
	m_pNetClient->ConnectChannel		= V4_ConnectChannel;
	m_pNetClient->DisconnectChannel		= V4_DisconnectChannel;
	
	m_pNetClient->GetServiceDescriptor = V4_GetServiceDesc;
	m_pNetClient->ServiceCommand		= V4_ServiceCommand;

	m_pNetClient->ChannelGetSLP			= V4_ChannelGetSLP;
	m_pNetClient->ChannelReleaseSLP		= V4_ChannelReleaseSLP;

	m_pNetClient->CanHandleURLInService = V4_CanHandleURLInService;

	channels = gf_list_new();
	if (path) m_path = strdup(path);
}

V4Service::~V4Service() 
{
	if (m_path) free(m_path);
	gf_list_del(channels);
	free(m_pNetClient);
}