/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / MP3 reader module
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
#include <gpac/avparse.h>
#include <gpac/constants.h>
#include <gpac/modules/codec.h>


typedef struct
{
	GF_ClientService *service;

	u32 needs_connection;
	Bool is_remote;
	
	FILE *stream;
	u32 duration;

	u32 pad_bytes;
	Bool done;
	LPNETCHANNEL ch;

	char *data;
	u32 data_size;

	GF_SLHeader sl_hdr;

	Bool is_inline;

	u32 sample_rate, oti;
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
} MP3Reader;


static Bool MP3_CanHandleURL(GF_InputService *plug, const char *url)
{
	char *sExt;
	sExt = strrchr(url, '.');
	if (!sExt) return 0;
	if (gf_term_check_extension(plug, "audio/mpeg", "mp2 mp3 mpga mpega", "MP3 Music", sExt)) return 1;
	if (gf_term_check_extension(plug, "audio/x-mpeg", "mp2 mp3 mpga mpega", "MP3 Music", sExt)) return 1;
	return 0;
}

static Bool mp3_is_local(const char *url)
{
	if (!strnicmp(url, "file://", 7)) return 1;
	if (strstr(url, "://")) return 0;
	return 1;
}


static GF_ESD *MP3_GetESD(MP3Reader *read)
{
	GF_ESD *esd = gf_odf_desc_esd_new(0);
	esd->slConfig->timestampResolution = read->sample_rate;
	esd->decoderConfig->streamType = GF_STREAM_AUDIO;
	esd->decoderConfig->objectTypeIndication = read->oti;
	esd->ESID = 1;
	return esd;
}

static void mp3_setup_object(MP3Reader *read)
{
	if (read->is_inline) {
		GF_ESD *esd;
		GF_ObjectDescriptor *od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
		od->objectDescriptorID = 1;
		esd = MP3_GetESD(read);
		gf_list_add(od->ESDescriptors, esd);
		gf_term_add_media(read->service, (GF_Descriptor*)od, 0);
	}
}


static Bool MP3_ConfigureFromFile(MP3Reader *read)
{
	u32 hdr, size, pos;
	if (!read->stream) return 0;

	hdr = gf_mp3_get_next_header(read->stream);
	if (!hdr) return 0;
	read->sample_rate = gf_mp3_sampling_rate(hdr);
	read->oti = gf_mp3_object_type_indication(hdr);
	fseek(read->stream, 0, SEEK_SET);
	if (!read->oti) return 0;

	/*we don't have the full file...*/
	if (read->is_remote) return	1;

	return 1;

	read->duration = gf_mp3_window_size(hdr);
	size = gf_mp3_frame_size(hdr);
	pos = ftell(read->stream);
	fseek(read->stream, pos + size - 4, SEEK_SET);
	while (1) {
		hdr = gf_mp3_get_next_header(read->stream);
		if (!hdr) break;
		read->duration += gf_mp3_window_size(hdr);
		size = gf_mp3_frame_size(hdr);
		pos = ftell(read->stream);
		fseek(read->stream, pos + size - 4, SEEK_SET);
	}
	fseek(read->stream, 0, SEEK_SET);
	return 1;
}

static void MP3_RegulateDataRate(MP3Reader *read)
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

static void MP3_OnLiveData(MP3Reader *read, char *data, u32 data_size)
{
	u32 hdr, size, pos;

	if (read->needs_connection) {
		hdr = gf_mp3_get_next_header_mem(data, data_size, &pos);
		if (!hdr) return;
		read->sample_rate = gf_mp3_sampling_rate(hdr);
		read->oti = gf_mp3_object_type_indication(hdr);
		read->is_live = 1;
		memset(&read->sl_hdr, 0, sizeof(GF_SLHeader));

		read->needs_connection = 0;
		gf_term_on_connect(read->service, NULL, GF_OK);
		mp3_setup_object(read);
	}
	if (!data_size) return;

	read->data = realloc(read->data, sizeof(char)*(read->data_size+data_size) );
	memcpy(read->data + read->data_size, data, sizeof(char)*data_size);
	read->data_size += data_size;
	if (!read->ch) return;

	
	data = read->data;
	data_size = read->data_size;

	while (1) {
		hdr = gf_mp3_get_next_header_mem(data, data_size, &pos);

		if (hdr) size = gf_mp3_frame_size(hdr);

		/*not enough data, copy over*/
		if (!hdr || (pos+size>data_size)) {
			char *d = malloc(sizeof(char) * data_size);
			memcpy(d, data, sizeof(char) * data_size);
			free(read->data);
			read->data = d;
			read->data_size = data_size;

			MP3_RegulateDataRate(read);
			return;
		}

		read->sl_hdr.accessUnitStartFlag = 1;
		read->sl_hdr.accessUnitEndFlag = 1;
		read->sl_hdr.AU_sequenceNumber++;
		read->sl_hdr.compositionTimeStampFlag = 1;
		read->sl_hdr.compositionTimeStamp += gf_mp3_window_size(hdr);
		gf_term_on_sl_packet(read->service, read->ch, data + pos, size, &read->sl_hdr, GF_OK);
		data += pos + size;
		assert(data_size>=pos+size);
		data_size -= pos+size;
	}
}

void MP3_NetIO(void *cbk, GF_NETIO_Parameter *param)
{
	GF_Err e;
	const char *szCache;
	u32 total_size, bytes_done;
	MP3Reader *read = (MP3Reader *) cbk;

	e = param->error;
	/*done*/
	if (param->msg_type==GF_NETIO_DATA_TRANSFERED) {
		if (read->stream) {
			read->is_remote = 0;
			e = GF_EOS;
		} else {
			return;
		}
	} 
	else if (param->msg_type==GF_NETIO_PARSE_HEADER) {
		if (!strcmp(param->name, "icy-name")) {
			if (read->icy_name) free(read->icy_name);
			read->icy_name = strdup(param->value);
		}
		if (!strcmp(param->name, "icy-genre")) {
			if (read->icy_genre) free(read->icy_genre);
			read->icy_genre = strdup(param->value);
		}
		if (!strcmp(param->name, "icy-meta")) {
			GF_NetworkCommand com;
			char *meta;
			if (read->icy_track_name) free(read->icy_track_name);
			read->icy_track_name = NULL;
			meta = param->value;
			while (meta && meta[0]) {
				char *sep = strchr(meta, ';');
				if (sep) sep[0] = 0;
	
				if (!strnicmp(meta, "StreamTitle=", 12)) {
					read->icy_track_name = strdup(meta+12);
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

	if (e >= GF_OK) {
		if (read->needs_connection) {
			gf_dm_sess_get_stats(read->dnload, NULL, NULL, &total_size, NULL, NULL, NULL);
			if (!total_size) read->is_live = 1;
		}
		/*looks like a live stream*/
		if (read->is_live) {
			if (!e) MP3_OnLiveData(read, param->data, param->size);
			return;
		}

		if (read->stream) return;

		/*open service*/
		szCache = gf_dm_sess_get_cache_name(read->dnload);
		if (!szCache) e = GF_IO_ERR;
		else {
			read->stream = fopen((char *) szCache, "rb");
			if (!read->stream) e = GF_SERVICE_ERROR;
			else {
				/*if full file at once (in cache) parse duration*/
				if (e==GF_EOS) read->is_remote = 0;
				e = GF_OK;
				/*not enough data*/
				if (!MP3_ConfigureFromFile(read)) {
					gf_dm_sess_get_stats(read->dnload, NULL, NULL, NULL, &bytes_done, NULL, NULL);
					/*bad data - there's likely some ID3 around...*/
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
		if (!e) mp3_setup_object(read);
	}
}

void mp3_download_file(GF_InputService *plug, char *url)
{
	MP3Reader *read = (MP3Reader*) plug->priv;

	read->needs_connection = 1;

	read->dnload = gf_term_download_new(read->service, url, 0, MP3_NetIO, read);
	if (!read->dnload) {
		read->needs_connection = 0;
		gf_term_on_connect(read->service, NULL, GF_NOT_SUPPORTED);
	}
	/*service confirm is done once fetched*/
}


static GF_Err MP3_ConnectService(GF_InputService *plug, GF_ClientService *serv, const char *url)
{
	char szURL[2048];
	char *ext;
	GF_Err reply;
	MP3Reader *read = plug->priv;
	read->service = serv;

	if (read->dnload) gf_term_download_del(read->dnload);
	read->dnload = NULL;

	strcpy(szURL, url);
	ext = strrchr(szURL, '#');
	if (ext) ext[0] = 0;

	/*remote fetch*/
	read->is_remote = !mp3_is_local(szURL);
	if (read->is_remote) {
		mp3_download_file(plug, (char *) szURL);
		return GF_OK;
	}

	reply = GF_OK;
	read->stream = fopen(szURL, "rb");
	if (!read->stream) {
		reply = GF_URL_ERROR;
	} else if (!MP3_ConfigureFromFile(read)) {
		fclose(read->stream);
		read->stream = NULL;
		reply = GF_NOT_SUPPORTED;
	}
	gf_term_on_connect(serv, NULL, reply);
	if (!reply) mp3_setup_object(read);
	return GF_OK;
}

static GF_Err MP3_CloseService(GF_InputService *plug)
{
	MP3Reader *read = plug->priv;
	if (read->stream) fclose(read->stream);
	read->stream = NULL;

	if (read->dnload) gf_term_download_del(read->dnload);
	read->dnload = NULL;

	if (read->data) free(read->data);
	read->data = NULL;
	gf_term_on_disconnect(read->service, NULL, GF_OK);
	return GF_OK;
}

static GF_Descriptor *MP3_GetServiceDesc(GF_InputService *plug, u32 expect_type, const char *sub_url)
{
	MP3Reader *read = plug->priv;

	/*override default*/
	if (expect_type==GF_MEDIA_OBJECT_UNDEF) expect_type=GF_MEDIA_OBJECT_AUDIO;
	
	/*audio object*/
	if (expect_type==GF_MEDIA_OBJECT_AUDIO) {
		GF_ObjectDescriptor *od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
		GF_ESD *esd = MP3_GetESD(read);
		od->objectDescriptorID = 1;
		gf_list_add(od->ESDescriptors, esd);
		return (GF_Descriptor *) od;
	}
	read->is_inline = 1;
	return NULL;
}

static GF_Err MP3_ConnectChannel(GF_InputService *plug, LPNETCHANNEL channel, const char *url, Bool upstream)
{
	u32 ES_ID;
	GF_Err e;
	MP3Reader *read = plug->priv;

	e = GF_SERVICE_ERROR;
	if (read->ch==channel) goto exit;

	e = GF_STREAM_NOT_FOUND;
	if (strstr(url, "ES_ID")) {
		sscanf(url, "ES_ID=%d", &ES_ID);
	}
	/*URL setup*/
	else if (!read->ch && MP3_CanHandleURL(plug, url)) ES_ID = 1;

	if (ES_ID==1) {
		read->ch = channel;
		e = GF_OK;
	}

exit:
	gf_term_on_connect(read->service, channel, e);
	return e;
}

static GF_Err MP3_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
	MP3Reader *read = plug->priv;
	GF_Err e = GF_STREAM_NOT_FOUND;
	if (read->ch == channel) {
		read->ch = NULL;
		if (read->data) free(read->data);
		read->data = NULL;
		e = GF_OK;
	}
	gf_term_on_disconnect(read->service, channel, e);
	return GF_OK;
}

static GF_Err MP3_ServiceCommand(GF_InputService *plug, GF_NetworkCommand *com)
{
	MP3Reader *read = plug->priv;

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
			if (com->buffer.max<2000) com->buffer.max = 2000;
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
		if (read->stream) fseek(read->stream, 0, SEEK_SET);

		if (read->ch == com->base.on_channel) { 
			read->done = 0; 
			/*PLAY after complete download, estimate duration*/
			if (!read->is_remote && !read->duration) {
				MP3_ConfigureFromFile(read);
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


static GF_Err MP3_ChannelGetSLP(GF_InputService *plug, LPNETCHANNEL channel, char **out_data_ptr, u32 *out_data_size, GF_SLHeader *out_sl_hdr, Bool *sl_compressed, GF_Err *out_reception_status, Bool *is_new_data)
{
	u32 pos, hdr, start_from;
	MP3Reader *read = plug->priv;

	if (read->ch != channel) return GF_STREAM_NOT_FOUND;

	*out_reception_status = GF_OK;
	*sl_compressed = 0;
	*is_new_data = 0;

	memset(&read->sl_hdr, 0, sizeof(GF_SLHeader));
	read->sl_hdr.randomAccessPointFlag = 1;
	read->sl_hdr.compositionTimeStampFlag = 1;

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
		*is_new_data = 1;

		pos = ftell(read->stream);
		hdr = gf_mp3_get_next_header(read->stream);
		if (!hdr) {
			if (!read->dnload) {
				*out_reception_status = GF_EOS;
				read->done = 1;
			} else {
				fseek(read->stream, pos, SEEK_SET);
				*out_reception_status = GF_OK;
			}
			return GF_OK;
		}
		read->data_size = gf_mp3_frame_size(hdr);
		if (!read->data_size) {
			*out_reception_status = GF_EOS;
			read->done = 1;
			return GF_OK;
		}


		/*we're seeking*/
		if (read->start_range && read->duration) {
			read->current_time = 0;
			start_from = (u32) (read->start_range * read->sample_rate);
			fseek(read->stream, 0, SEEK_SET);
			while (read->current_time<start_from) {
				hdr = gf_mp3_get_next_header(read->stream);
				if (!hdr) {
					read->start_range = 0;
					*out_reception_status = GF_SERVICE_ERROR;
					return GF_OK;
				}
				read->current_time += gf_mp3_window_size(hdr);
				read->data_size = gf_mp3_frame_size(hdr);
				fseek(read->stream, read->data_size-4, SEEK_CUR);
			}
			read->start_range = 0;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[MP3Demux] Seeking to frame size %d - TS %d - file pos %d\n", read->data_size, read->current_time, ftell(read->stream)));
		}

		read->sl_hdr.compositionTimeStamp = read->current_time;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[MP3Demux] Found new frame size %d - TS %d - file pos %d\n", read->data_size, read->current_time, ftell(read->stream)));

		read->current_time += gf_mp3_window_size(hdr);

		read->data = malloc(sizeof(char) * (read->data_size+read->pad_bytes));
		read->data[0] = (hdr >> 24) & 0xFF;
		read->data[1] = (hdr >> 16) & 0xFF;
		read->data[2] = (hdr >> 8) & 0xFF;
		read->data[3] = (hdr ) & 0xFF;
		/*end of file*/
		if (fread(&read->data[4], 1, read->data_size - 4, read->stream) != read->data_size-4) {
			free(read->data);
			read->data = NULL;
			if (read->is_remote) {
				fseek(read->stream, pos, SEEK_SET);
				*out_reception_status = GF_OK;
			} else {
				*out_reception_status = GF_EOS;
			}
			return GF_OK;
		}
		if (read->pad_bytes) memset(read->data + read->data_size, 0, sizeof(char) * read->pad_bytes);
	}
	*out_sl_hdr = read->sl_hdr;
	*out_data_ptr = read->data;
	*out_data_size = read->data_size;
	return GF_OK;
}

static GF_Err MP3_ChannelReleaseSLP(GF_InputService *plug, LPNETCHANNEL channel)
{
	MP3Reader *read = plug->priv;

	if (read->ch == channel) {
		if (!read->data) return GF_BAD_PARAM;
		free(read->data);
		read->data = NULL;
		return GF_OK;
	}
	return GF_OK;
}

GF_InputService *MP3_Load()
{
	MP3Reader *reader;
	GF_InputService *plug = malloc(sizeof(GF_InputService));
	memset(plug, 0, sizeof(GF_InputService));
	GF_REGISTER_MODULE_INTERFACE(plug, GF_NET_CLIENT_INTERFACE, "GPAC MP3 Reader", "gpac distribution")

	plug->CanHandleURL = MP3_CanHandleURL;
	plug->ConnectService = MP3_ConnectService;
	plug->CloseService = MP3_CloseService;
	plug->GetServiceDescriptor = MP3_GetServiceDesc;
	plug->ConnectChannel = MP3_ConnectChannel;
	plug->DisconnectChannel = MP3_DisconnectChannel;
	plug->ServiceCommand = MP3_ServiceCommand;
	/*we do support pull mode*/
	plug->ChannelGetSLP = MP3_ChannelGetSLP;
	plug->ChannelReleaseSLP = MP3_ChannelReleaseSLP;

	reader = malloc(sizeof(MP3Reader));
	memset(reader, 0, sizeof(MP3Reader));
	plug->priv = reader;
	return plug;
}

void MP3_Delete(void *ifce)
{
	GF_InputService *plug = (GF_InputService *) ifce;
	MP3Reader *read = plug->priv;
	free(read);
	free(plug);
}


#ifdef GPAC_HAS_MAD
GF_BaseDecoder *NewMADDec();
void DeleteMADDec(GF_BaseDecoder *ifcg);
#endif

GF_EXPORT
Bool QueryInterface(u32 InterfaceType) 
{
	if (InterfaceType == GF_NET_CLIENT_INTERFACE) return 1;
#ifdef GPAC_HAS_MAD
	if (InterfaceType == GF_MEDIA_DECODER_INTERFACE) return 1;
#endif
	return 0;
}

GF_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType) 
{
	if (InterfaceType == GF_NET_CLIENT_INTERFACE) return (GF_BaseInterface *)MP3_Load();
#ifdef GPAC_HAS_MAD
	if (InterfaceType == GF_MEDIA_DECODER_INTERFACE) return (GF_BaseInterface *)NewMADDec();
#endif
	return NULL;
}

GF_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
#ifdef GPAC_HAS_MAD
	case GF_MEDIA_DECODER_INTERFACE:
		DeleteMADDec((GF_BaseDecoder *) ifce);
		break;
#endif
	case GF_NET_CLIENT_INTERFACE:
		MP3_Delete(ifce);
		break;
	}
}
