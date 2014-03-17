/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / AC3 reader module
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
#include <gpac/avparse.h>
#include <gpac/constants.h>

#ifndef GPAC_DISABLE_AV_PARSERS

typedef struct
{
	GF_ClientService *service;

	Bool is_remote;
	
	FILE *stream;
	u32 duration;

	Bool needs_connection;
	u32 pad_bytes;
	Bool done;
	u32 is_inline;
	LPNETCHANNEL ch;

	unsigned char *data;
	u32 data_size;
	GF_SLHeader sl_hdr;

	u32 sample_rate, nb_ch;
	Double start_range, end_range;
	u32 current_time, nb_samp;
	/*file downloader*/
	GF_DownloadSession * dnload;

	Bool is_live;
	char prev_data[1000];
	u32 prev_size;

	char *icy_name;
	char *icy_genre;
	char *icy_track_name;
} AC3Reader;

static const char * AC3_MIMES[] = { "audio/ac3", "audio/x-ac3", NULL };

static const char * AC3_EXTS = "ac3";

static const char * AC3_DESC = "AC3 Music";

static u32 AC3_RegisterMimeTypes(const GF_InputService *plug)
{
    u32 i;
    for (i = 0 ; AC3_MIMES[i]; i++)
      gf_term_register_mime_type(plug, AC3_MIMES[i], AC3_EXTS, AC3_DESC);
    return i;
}

static Bool AC3_CanHandleURL(GF_InputService *plug, const char *url)
{
	char *sExt;
	u32 i;
	sExt = strrchr(url, '.');
	for (i = 0 ; AC3_MIMES[i]; i++){
	  if (gf_term_check_extension(plug, AC3_MIMES[i], AC3_EXTS, AC3_DESC, sExt)) return 1;
	}
	return 0;
}

static Bool ac3_is_local(const char *url)
{
	if (!strnicmp(url, "file://", 7)) return 1;
	if (strstr(url, "://")) return 0;
	return 1;
}

static GF_ESD *AC3_GetESD(AC3Reader *read)
{
	GF_ESD *esd;
	esd = gf_odf_desc_esd_new(0);
	esd->decoderConfig->streamType = GF_STREAM_AUDIO;
	esd->decoderConfig->objectTypeIndication = GPAC_OTI_AUDIO_AC3;
	esd->ESID = 1;
	esd->OCRESID = 1;
	esd->slConfig->timestampResolution = read->sample_rate;
	if (read->is_live) esd->slConfig->useAccessUnitEndFlag = esd->slConfig->useAccessUnitStartFlag = 1;
	return esd;
}

static void AC3_SetupObject(AC3Reader *read)
{
	GF_ESD *esd;
	GF_ObjectDescriptor *od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
	od->objectDescriptorID = 1;
	esd = AC3_GetESD(read);
	esd->OCRESID = 0;
	gf_list_add(od->ESDescriptors, esd);
	gf_term_add_media(read->service, (GF_Descriptor*)od, 0);
}


static Bool AC3_ConfigureFromFile(AC3Reader *read)
{
	Bool sync;
	GF_BitStream *bs;
	GF_AC3Header hdr;
	if (!read->stream) return 0;
	bs = gf_bs_from_file(read->stream, GF_BITSTREAM_READ);

	
	sync = gf_ac3_parser_bs(bs, &hdr, 1);
	if (!sync) {
		gf_bs_del(bs);
		return 0;
	}
	read->nb_ch = hdr.channels;
	read->sample_rate = hdr.sample_rate;
	read->duration = 0;
	
	if (!read->is_remote) {
		read->duration = 1536;
		gf_bs_skip_bytes(bs, hdr.framesize);
		while (gf_ac3_parser_bs(bs, &hdr, 0)) {
			read->duration += 1536;
			gf_bs_skip_bytes(bs, hdr.framesize);
		}
	}
	gf_bs_del(bs);
	gf_f64_seek(read->stream, 0, SEEK_SET);
	return 1;
}

static void AC3_RegulateDataRate(AC3Reader *read)
{
	GF_NetworkCommand com;

	memset(&com, 0, sizeof(GF_NetworkCommand));
	com.command_type = GF_NET_CHAN_BUFFER_QUERY;
	com.base.on_channel = read->ch;
	while (read->ch) {
		gf_term_on_command(read->service, &com, GF_OK);
		if (com.buffer.occupancy < com.buffer.max) break;
		gf_sleep(2);
	}
}

static void AC3_OnLiveData(AC3Reader *read, const char *data, u32 data_size)
{
	u64 pos;
	Bool sync;
	GF_BitStream *bs;
	GF_AC3Header hdr;
	
	read->data = gf_realloc(read->data, sizeof(char)*(read->data_size+data_size) );
	memcpy(read->data + read->data_size, data, sizeof(char)*data_size);
	read->data_size += data_size;

	if (read->needs_connection) {
		read->needs_connection = 0;
		bs = gf_bs_new((char *) read->data, read->data_size, GF_BITSTREAM_READ);
		sync = gf_ac3_parser_bs(bs, &hdr, 1);
		gf_bs_del(bs);
		if (!sync) return;
		read->nb_ch = hdr.channels;
		read->sample_rate = hdr.sample_rate;
		read->is_live = 1;
		memset(&read->sl_hdr, 0, sizeof(GF_SLHeader));
		gf_term_on_connect(read->service, NULL, GF_OK);
		AC3_SetupObject(read);
	}
	if (!read->ch) return;

	/*need a full ac3 header*/
	if (read->data_size<=7) return;

	bs = gf_bs_new((char *) read->data, read->data_size, GF_BITSTREAM_READ);
	hdr.framesize = 0;
	pos = 0;
	while (gf_ac3_parser_bs(bs, &hdr, 0)) {
		pos = gf_bs_get_position(bs);
		read->sl_hdr.accessUnitStartFlag = 1;
		read->sl_hdr.accessUnitEndFlag = 1;
		read->sl_hdr.AU_sequenceNumber++;
		read->sl_hdr.compositionTimeStampFlag = 1;
		read->sl_hdr.compositionTimeStamp += 1536;
		gf_term_on_sl_packet(read->service, read->ch, (char *) read->data + pos, hdr.framesize, &read->sl_hdr, GF_OK);
		gf_bs_skip_bytes(bs, hdr.framesize);
	}

	pos = gf_bs_get_position(bs);
	gf_bs_del(bs);

	if (pos) {
		u8 *d;
		read->data_size -= (u32) pos;
		d = gf_malloc(sizeof(char) * read->data_size);
		memcpy(d, read->data + pos, sizeof(char) * read->data_size);
		gf_free(read->data);
		read->data = d;
	}
	AC3_RegulateDataRate(read);
}

void AC3_NetIO(void *cbk, GF_NETIO_Parameter *param)
{
	GF_Err e;
	const char *szCache;
	u32 total_size, bytes_done;
	AC3Reader *read = (AC3Reader *) cbk;

	e = param->error;
	/*done*/
	if (param->msg_type==GF_NETIO_DATA_TRANSFERED) {
		if (read->stream) {
			read->is_remote = 0;
			e = GF_EOS;
		} else if (!read->needs_connection) {
			return;
		}
	} else if (param->msg_type==GF_NETIO_PARSE_HEADER) {
		if (!strcmp(param->name, "icy-name")) {
			if (read->icy_name) gf_free(read->icy_name);
			read->icy_name = gf_strdup(param->value);
		}
		if (!strcmp(param->name, "icy-genre")) {
			if (read->icy_genre) gf_free(read->icy_genre);
			read->icy_genre = gf_strdup(param->value);
		}
		if (!strcmp(param->name, "icy-meta")) {
			GF_NetworkCommand com;
			char *meta;
			if (read->icy_track_name) gf_free(read->icy_track_name);
			read->icy_track_name = NULL;
			meta = param->value;
			while (meta && meta[0]) {
				char *sep = strchr(meta, ';');
				if (sep) sep[0] = 0;
	
				if (!strnicmp(meta, "StreamTitle=", 12)) {
					read->icy_track_name = gf_strdup(meta+12);
				}
				if (!sep) break;
				sep[0] = ';';
				meta = sep+1;
			}

			com.base.command_type = GF_NET_SERVICE_INFO;
			gf_term_on_command(read->service, &com, GF_OK);
		}
		return;
	} else {
		/*handle service message*/
		gf_term_download_update_stats(read->dnload);
		if (param->msg_type!=GF_NETIO_DATA_EXCHANGE) return;
	}

	/*data fetching or EOS*/
	if (e >= GF_OK) {
		if (read->needs_connection) {
			gf_dm_sess_get_stats(read->dnload, NULL, NULL, &total_size, NULL, NULL, NULL);
			if (!total_size) read->is_live = 1;
		}
		if (read->is_live) {
			if (!e) AC3_OnLiveData(read, param->data, param->size);
			return;
		}
		if (read->stream) return;

		/*open service*/
		szCache = gf_dm_sess_get_cache_name(read->dnload);
		if (!szCache) e = GF_IO_ERR;
		else {
			read->stream = gf_f64_open((char *) szCache, "rb");
			if (!read->stream) e = GF_SERVICE_ERROR;
			else {
				/*if full file at once (in cache) parse duration*/
				if (e==GF_EOS) read->is_remote = 0;
				e = GF_OK;
				/*not enough data*/
				if (!AC3_ConfigureFromFile(read)) {
					/*get amount downloaded and check*/
					gf_dm_sess_get_stats(read->dnload, NULL, NULL, NULL, &bytes_done, NULL, NULL);
					if (bytes_done>10*1024) {
						e = GF_CORRUPTED_DATA;
					} else {
						fclose(read->stream);
						read->stream = NULL;
						return;
					}
				}
			}
		}
	}
	/*OK confirm*/
	if (read->needs_connection) {
		read->needs_connection = 0;
		gf_term_on_connect(read->service, NULL, e);
		if (!e) AC3_SetupObject(read);
	}
}

void ac3_download_file(GF_InputService *plug, char *url)
{
	AC3Reader *read = (AC3Reader*) plug->priv;

	read->needs_connection = 1;

	read->dnload = gf_term_download_new(read->service, url, 0, AC3_NetIO, read);
	if (!read->dnload ) {
		read->needs_connection = 0;
		gf_term_on_connect(read->service, NULL, GF_NOT_SUPPORTED);
	} else {
		/*start our download (threaded)*/
		gf_dm_sess_process(read->dnload);
	}
	/*service confirm is done once fetched*/
}


static GF_Err AC3_ConnectService(GF_InputService *plug, GF_ClientService *serv, const char *url)
{
	char szURL[2048];
	char *ext;
	GF_Err reply;
	AC3Reader *read = plug->priv;
	read->service = serv;

	if (read->dnload) gf_term_download_del(read->dnload);
	read->dnload = NULL;

	strcpy(szURL, url);
	ext = strrchr(szURL, '#');
	if (ext) ext[0] = 0;

	/*remote fetch*/
	read->is_remote = !ac3_is_local(szURL);
	if (read->is_remote) {
		ac3_download_file(plug, (char *) szURL);
		return GF_OK;
	}

	reply = GF_OK;
	read->stream = gf_f64_open(szURL, "rb");
	if (!read->stream) {
		reply = GF_URL_ERROR;
	} else if (!AC3_ConfigureFromFile(read)) {
		fclose(read->stream);
		read->stream = NULL;
		reply = GF_NOT_SUPPORTED;
	}
	gf_term_on_connect(serv, NULL, reply);
	if (!reply && read->is_inline ) AC3_SetupObject(read);
	return GF_OK;
}

static GF_Err AC3_CloseService(GF_InputService *plug)
{
	AC3Reader *read = plug->priv;
	if (read->stream) fclose(read->stream);
	read->stream = NULL;
	if (read->dnload) gf_term_download_del(read->dnload);
	read->dnload = NULL;

	if (read->data) gf_free(read->data);
	read->data = NULL;
	gf_term_on_disconnect(read->service, NULL, GF_OK);
	return GF_OK;
}

static GF_Descriptor *AC3_GetServiceDesc(GF_InputService *plug, u32 expect_type, const char *sub_url)
{
	AC3Reader *read = plug->priv;
	/*since we don't handle multitrack in ac3, we don't need to check sub_url, only use expected type*/

	/*override default*/
	if (expect_type==GF_MEDIA_OBJECT_UNDEF) expect_type=GF_MEDIA_OBJECT_AUDIO;

	/*audio object*/
	if (expect_type==GF_MEDIA_OBJECT_AUDIO) {
		GF_ESD *esd;
		GF_ObjectDescriptor *od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
		od->objectDescriptorID = 1;
		esd = AC3_GetESD(read);
		esd->OCRESID = 0;
		gf_list_add(od->ESDescriptors, esd);
		return (GF_Descriptor *) od;
	}
	read->is_inline = 1;
	/*inline scene: no specific service*/
	return NULL;
}

static GF_Err AC3_ConnectChannel(GF_InputService *plug, LPNETCHANNEL channel, const char *url, Bool upstream)
{
	u32 ES_ID=0;
	GF_Err e;
	AC3Reader *read = plug->priv;

	e = GF_SERVICE_ERROR;
	if (read->ch==channel) goto exit;

	e = GF_STREAM_NOT_FOUND;
	if (strstr(url, "ES_ID")) {
		sscanf(url, "ES_ID=%ud", &ES_ID);
	}
	/*URL setup*/
	else if (!read->ch && AC3_CanHandleURL(plug, url)) ES_ID = 1;

	if (ES_ID==1) {
		read->ch = channel;
		e = GF_OK;
	}

exit:
	gf_term_on_connect(read->service, channel, e);
	return e;
}

static GF_Err AC3_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
	AC3Reader *read = plug->priv;

	GF_Err e = GF_STREAM_NOT_FOUND;
	if (read->ch == channel) {
		read->ch = NULL;
		if (read->data) gf_free(read->data);
		read->data = NULL;
		e = GF_OK;
	}
	gf_term_on_disconnect(read->service, channel, e);
	return GF_OK;
}

static GF_Err AC3_ServiceCommand(GF_InputService *plug, GF_NetworkCommand *com)
{
	AC3Reader *read = plug->priv;


	if (com->base.command_type==GF_NET_SERVICE_INFO) {
		com->info.name = read->icy_track_name ? read->icy_track_name : read->icy_name;
		com->info.comment = read->icy_genre;
		return GF_OK;
	}

	if (!com->base.on_channel) {
		/*if live session we may cache*/
		if (read->is_live && (com->command_type==GF_NET_IS_CACHABLE)) return GF_OK;
		return GF_NOT_SUPPORTED;
	}
	switch (com->command_type) {
	case GF_NET_CHAN_SET_PULL:
		if ((read->ch == com->base.on_channel) && read->is_live) return GF_NOT_SUPPORTED;
		return GF_OK;
	case GF_NET_CHAN_INTERACTIVE:
		if ((read->ch == com->base.on_channel) && read->is_live) return GF_NOT_SUPPORTED;
		return GF_OK;
	case GF_NET_CHAN_BUFFER:
		if ((read->ch == com->base.on_channel) && read->is_live) {
			if (com->buffer.max<1000) com->buffer.max = 1000;
			com->buffer.min = com->buffer.max/2;
		}
		return GF_OK;
	case GF_NET_CHAN_SET_PADDING:
		read->pad_bytes = com->pad.padding_bytes;
		return GF_OK;
	case GF_NET_CHAN_DURATION:
		com->duration.duration = read->duration;
		com->duration.duration /= read->sample_rate;
		return GF_OK;
	case GF_NET_CHAN_PLAY:
		read->start_range = com->play.start_range;
		read->end_range = com->play.end_range;
		read->current_time = 0;
		if (read->stream) gf_f64_seek(read->stream, 0, SEEK_SET);

		if (read->ch == com->base.on_channel) { 
			read->done = 0; 
			/*PLAY after complete download, estimate duration*/
			if (!read->is_remote && !read->duration) {
				AC3_ConfigureFromFile(read);
				if (read->duration) {
					GF_NetworkCommand rcfg;
					rcfg.base.on_channel = read->ch;
					rcfg.base.command_type = GF_NET_CHAN_DURATION;
					rcfg.duration.duration = read->duration;
					rcfg.duration.duration /= read->sample_rate;
					gf_term_on_command(read->service, &rcfg, GF_OK);
				}
			}
		}
		return GF_OK;
	case GF_NET_CHAN_STOP:
		return GF_OK;
	default:
		return GF_OK;
	}
}


static GF_Err AC3_ChannelGetSLP(GF_InputService *plug, LPNETCHANNEL channel, char **out_data_ptr, u32 *out_data_size, GF_SLHeader *out_sl_hdr, Bool *sl_compressed, GF_Err *out_reception_status, Bool *is_new_data)
{
	u64 pos, start_from;
	Bool sync;
	GF_BitStream *bs;
	GF_AC3Header hdr;
	AC3Reader *read = plug->priv;

	*out_reception_status = GF_OK;
	*sl_compressed = 0;
	*is_new_data = 0;

	memset(&read->sl_hdr, 0, sizeof(GF_SLHeader));
	read->sl_hdr.randomAccessPointFlag = 1;
	read->sl_hdr.compositionTimeStampFlag = 1;

	if (read->ch != channel) return GF_STREAM_NOT_FOUND;

	/*fetching es data*/
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
		bs = gf_bs_from_file(read->stream, GF_BITSTREAM_READ);
		*is_new_data = 1;

fetch_next:
		pos = gf_f64_tell(read->stream);
		sync = gf_ac3_parser_bs(bs, &hdr, 0);
		if (!sync) {
			gf_bs_del(bs);
			if (!read->dnload) {
				*out_reception_status = GF_EOS;
				read->done = 1;
			} else {
				gf_f64_seek(read->stream, pos, SEEK_SET);
				*out_reception_status = GF_OK;
			}
			return GF_OK;
		}

		if (!hdr.framesize) {
			gf_bs_del(bs);
			*out_reception_status = GF_EOS;
			read->done = 1;
			return GF_OK;
		}
		read->data_size = hdr.framesize;
		read->nb_samp = 1536;
		/*we're seeking*/
		if (read->start_range && read->duration) {
			start_from = (u32) (read->start_range * read->sample_rate);
			if (read->current_time + read->nb_samp < start_from) {
				read->current_time += read->nb_samp;
				goto fetch_next;
			} else {
				read->start_range = 0;
			}
		}
		
		read->sl_hdr.compositionTimeStamp = read->current_time;

		read->data = gf_malloc(sizeof(char) * (read->data_size+read->pad_bytes));
		gf_bs_read_data(bs, (char *) read->data, read->data_size);
		if (read->pad_bytes) memset(read->data + read->data_size, 0, sizeof(char) * read->pad_bytes);
		gf_bs_del(bs);
	}
	*out_sl_hdr = read->sl_hdr;
	*out_data_ptr =(char *) read->data;
	*out_data_size = read->data_size;
	return GF_OK;
}

static GF_Err AC3_ChannelReleaseSLP(GF_InputService *plug, LPNETCHANNEL channel)
{
	AC3Reader *read = plug->priv;

	if (read->ch == channel) {
		if (!read->data) return GF_BAD_PARAM;
		gf_free(read->data);
		read->data = NULL;
		read->current_time += read->nb_samp;
		return GF_OK;
	}
	return GF_BAD_PARAM;
}

GF_InputService *AC3_Load()
{
	AC3Reader *reader;
	GF_InputService *plug = gf_malloc(sizeof(GF_InputService));
	memset(plug, 0, sizeof(GF_InputService));
	GF_REGISTER_MODULE_INTERFACE(plug, GF_NET_CLIENT_INTERFACE, "GPAC AC3 Reader", "gpac distribution")

	plug->RegisterMimeTypes = AC3_RegisterMimeTypes;
	plug->CanHandleURL = AC3_CanHandleURL;
	plug->ConnectService = AC3_ConnectService;
	plug->CloseService = AC3_CloseService;
	plug->GetServiceDescriptor = AC3_GetServiceDesc;
	plug->ConnectChannel = AC3_ConnectChannel;
	plug->DisconnectChannel = AC3_DisconnectChannel;
	plug->ServiceCommand = AC3_ServiceCommand;
	/*we do support pull mode*/
	plug->ChannelGetSLP = AC3_ChannelGetSLP;
	plug->ChannelReleaseSLP = AC3_ChannelReleaseSLP;

	reader = gf_malloc(sizeof(AC3Reader));
	memset(reader, 0, sizeof(AC3Reader));
	plug->priv = reader;
	return plug;
}

void AC3_Delete(void *ifce)
{
	GF_InputService *plug = (GF_InputService *) ifce;
	AC3Reader *read = plug->priv;
	gf_free(read);
	gf_free(plug);
}

#endif /*GPAC_DISABLE_AV_PARSERS*/

#ifdef GPAC_HAS_LIBA52
GF_BaseDecoder *NewAC3Dec();
void DeleteAC3Dec(GF_BaseDecoder *ifcg);
#endif

GPAC_MODULE_EXPORT
const u32 *QueryInterfaces() 
{
static u32 si [] = {
#ifndef GPAC_DISABLE_AV_PARSERS
	GF_NET_CLIENT_INTERFACE,
#endif
#ifdef GPAC_HAS_LIBA52
	GF_MEDIA_DECODER_INTERFACE,
#endif
	0
};

	return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType) 
{
#ifndef GPAC_DISABLE_AV_PARSERS
	if (InterfaceType == GF_NET_CLIENT_INTERFACE) return (GF_BaseInterface *)AC3_Load();
#endif
#ifdef GPAC_HAS_LIBA52
	if (InterfaceType == GF_MEDIA_DECODER_INTERFACE) return (GF_BaseInterface *)NewAC3Dec();
#endif
	return NULL;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
#ifdef GPAC_HAS_LIBA52
	case GF_MEDIA_DECODER_INTERFACE:
		DeleteAC3Dec((GF_BaseDecoder *)ifce);
		break;
#endif
#ifndef GPAC_DISABLE_AV_PARSERS
	case GF_NET_CLIENT_INTERFACE:
		AC3_Delete(ifce);
		break;
#endif
	}
}

GPAC_MODULE_STATIC_DELARATION( ac3 )
