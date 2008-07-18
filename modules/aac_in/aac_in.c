/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
 *					All rights reserved
 *
 *  This file is part of GPAC / AAC reader module
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

	u32 sample_rate, oti, sr_idx, nb_ch, prof;
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
} AACReader;

typedef struct
{
	Bool is_mp2, no_crc;
	u32 profile, sr_idx, nb_ch, frame_size, hdr_size;
} ADTSHeader;

static Bool AAC_CanHandleURL(GF_InputService *plug, const char *url)
{
	char *sExt;
	sExt = strrchr(url, '.');
	if (!sExt) return 0;
	if (gf_term_check_extension(plug, "audio/x-m4a", "aac", "MPEG-4 AAC Music", sExt)) return 1;
	if (gf_term_check_extension(plug, "audio/aac", "aac", "MPEG-4 AAC Music", sExt)) return 1;
	if (gf_term_check_extension(plug, "audio/aacp", "aac", "MPEG-4 AACPlus Music", sExt)) return 1;
	return 0;
}

static Bool aac_is_local(const char *url)
{
	if (!strnicmp(url, "file://", 7)) return 1;
	if (strstr(url, "://")) return 0;
	return 1;
}

static GF_ESD *AAC_GetESD(AACReader *read)
{
	GF_BitStream *dsi;
	GF_ESD *esd;
	u32 i, sbr_sr_idx;

	esd = gf_odf_desc_esd_new(0);
	esd->decoderConfig->streamType = GF_STREAM_AUDIO;
	esd->decoderConfig->objectTypeIndication = read->oti;
	esd->ESID = 1;
	esd->OCRESID = 1;
	esd->slConfig->timestampResolution = read->sample_rate;
	if (read->is_live) esd->slConfig->useAccessUnitEndFlag = esd->slConfig->useAccessUnitStartFlag = 1;
	dsi = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	/*write as regular AAC*/
	gf_bs_write_int(dsi, read->prof, 5);
	gf_bs_write_int(dsi, read->sr_idx, 4);
	gf_bs_write_int(dsi, read->nb_ch, 4);
	gf_bs_align(dsi);

	/*always signal implicit S	BR in case it's used*/
	sbr_sr_idx = read->sr_idx;
	for (i=0; i<16; i++) {
		if (GF_M4ASampleRates[i] == (u32) 2*read->sample_rate) {
			sbr_sr_idx = i;
			break;
		}
	}
	gf_bs_write_int(dsi, 0x2b7, 11);
	gf_bs_write_int(dsi, 5, 5);
	gf_bs_write_int(dsi, 1, 1);
	gf_bs_write_int(dsi, sbr_sr_idx, 4);

	gf_bs_align(dsi);
	gf_bs_get_content(dsi, &esd->decoderConfig->decoderSpecificInfo->data, &esd->decoderConfig->decoderSpecificInfo->dataLength);
	gf_bs_del(dsi);
	return esd;
}

static void AAC_SetupObject(AACReader *read)
{
	GF_ESD *esd;
	GF_ObjectDescriptor *od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
	od->objectDescriptorID = 1;
	esd = AAC_GetESD(read);
	esd->OCRESID = 0;
	gf_list_add(od->ESDescriptors, esd);
	gf_term_add_media(read->service, (GF_Descriptor*)od, 0);
}

static Bool ADTS_SyncFrame(GF_BitStream *bs, Bool is_complete, ADTSHeader *hdr)
{
	u32 val, pos, start_pos;

	start_pos = (u32) gf_bs_get_position(bs);
	while (gf_bs_available(bs)) {
		val = gf_bs_read_u8(bs);
		if (val!=0xFF) continue;
		val = gf_bs_read_int(bs, 4);
		if (val != 0x0F) {
			gf_bs_read_int(bs, 4);
			continue;
		}
		hdr->is_mp2 = gf_bs_read_int(bs, 1);
		gf_bs_read_int(bs, 2);
		hdr->no_crc = gf_bs_read_int(bs, 1);
		pos = (u32) gf_bs_get_position(bs) - 2;

		hdr->profile = 1 + gf_bs_read_int(bs, 2);
		hdr->sr_idx = gf_bs_read_int(bs, 4);
		gf_bs_read_int(bs, 1);
		hdr->nb_ch = gf_bs_read_int(bs, 3);
		gf_bs_read_int(bs, 4);
		hdr->frame_size = gf_bs_read_int(bs, 13);
		gf_bs_read_int(bs, 11);
		gf_bs_read_int(bs, 2);
		hdr->hdr_size = 7;
		if (!hdr->no_crc) {
			gf_bs_read_u16(bs);
			hdr->hdr_size = 9;
		}
		if (hdr->frame_size < hdr->hdr_size) {
			gf_bs_seek(bs, pos+1);
			continue;
		}
		hdr->frame_size -= hdr->hdr_size;
		if (is_complete && (gf_bs_available(bs) == hdr->frame_size)) return 1;
		else if (gf_bs_available(bs) <= hdr->frame_size) break;

		gf_bs_skip_bytes(bs, hdr->frame_size);
		val = gf_bs_read_u8(bs);
		if (val!=0xFF) {
			gf_bs_seek(bs, pos+1);
			continue;
		}
		val = gf_bs_read_int(bs, 4);
		if (val!=0x0F) {
			gf_bs_read_int(bs, 4);
			gf_bs_seek(bs, pos+1);
			continue;
		}
		gf_bs_seek(bs, pos+hdr->hdr_size);
		return 1;
	}
	gf_bs_seek(bs, start_pos);
	return 0;
}

static Bool AAC_ConfigureFromFile(AACReader *read)
{
	Bool sync;
	GF_BitStream *bs;
	ADTSHeader hdr;
	if (!read->stream) return 0;
	bs = gf_bs_from_file(read->stream, GF_BITSTREAM_READ);

	sync = ADTS_SyncFrame(bs, !read->is_remote, &hdr);
	if (!sync) {
		gf_bs_del(bs);
		return 0;
	}
	read->nb_ch = hdr.nb_ch;
	read->prof = hdr.profile;
	read->sr_idx = hdr.sr_idx;
	read->oti = hdr.is_mp2 ? read->prof+0x66 : 0x40;
	read->sample_rate = GF_M4ASampleRates[read->sr_idx];

	read->duration = 0;
	
	if (!read->is_remote) {
		read->duration = 1024;
		gf_bs_skip_bytes(bs, hdr.frame_size);
		while (ADTS_SyncFrame(bs, !read->is_remote, &hdr)) {
			read->duration += 1024;
			gf_bs_skip_bytes(bs, hdr.frame_size);
		}
	}
	gf_bs_del(bs);
	fseek(read->stream, 0, SEEK_SET);
	return 1;
}

static void AAC_RegulateDataRate(AACReader *read)
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

static void AAC_OnLiveData(AACReader *read, char *data, u32 data_size)
{
	u32 pos;
	Bool sync;
	GF_BitStream *bs;
	ADTSHeader hdr;
	
	read->data = realloc(read->data, sizeof(char)*(read->data_size+data_size) );
	memcpy(read->data + read->data_size, data, sizeof(char)*data_size);
	read->data_size += data_size;

	if (read->needs_connection) {
		read->needs_connection = 0;
		bs = gf_bs_new(read->data, read->data_size, GF_BITSTREAM_READ);
		sync = ADTS_SyncFrame(bs, 0, &hdr);
		gf_bs_del(bs);
		if (!sync) return;
		read->nb_ch = hdr.nb_ch;
		read->prof = hdr.profile;
		read->sr_idx = hdr.sr_idx;
		read->oti = hdr.is_mp2 ? read->prof+0x66-1 : 0x40;
		read->sample_rate = GF_M4ASampleRates[read->sr_idx];
		read->is_live = 1;
		memset(&read->sl_hdr, 0, sizeof(GF_SLHeader));
		gf_term_on_connect(read->service, NULL, GF_OK);
		AAC_SetupObject(read);
	}
	if (!read->ch) return;

	/*need a full adts header*/
	if (read->data_size<=7) return;

	bs = gf_bs_new(read->data, read->data_size, GF_BITSTREAM_READ);
	hdr.frame_size = pos = 0;
	while (ADTS_SyncFrame(bs, 0, &hdr)) {
		pos = (u32) gf_bs_get_position(bs);
		read->sl_hdr.accessUnitStartFlag = 1;
		read->sl_hdr.accessUnitEndFlag = 1;
		read->sl_hdr.AU_sequenceNumber++;
		read->sl_hdr.compositionTimeStampFlag = 1;
		read->sl_hdr.compositionTimeStamp += 1024;
		gf_term_on_sl_packet(read->service, read->ch, read->data + pos, hdr.frame_size, &read->sl_hdr, GF_OK);
		gf_bs_skip_bytes(bs, hdr.frame_size);
	}

	pos = (u32) gf_bs_get_position(bs);
	gf_bs_del(bs);

	if (pos) {
		char *d;
		read->data_size -= pos;
		d = malloc(sizeof(char) * read->data_size);
		memcpy(d, read->data + pos, sizeof(char) * read->data_size);
		free(read->data);
		read->data = d;
	}
	AAC_RegulateDataRate(read);
}

void AAC_NetIO(void *cbk, GF_NETIO_Parameter *param)
{
	GF_Err e;
	const char *szCache;
	u32 total_size, bytes_done;
	AACReader *read = (AACReader *) cbk;

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

	/*data fetching or EOS*/
	if (e >= GF_OK) {
		if (read->needs_connection) {
			gf_dm_sess_get_stats(read->dnload, NULL, NULL, &total_size, NULL, NULL, NULL);
			if (!total_size) read->is_live = 1;
		}
		if (read->is_live) {
			if (!e) AAC_OnLiveData(read, param->data, param->size);
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
				if (!AAC_ConfigureFromFile(read)) {
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
		if (!e) AAC_SetupObject(read);
	}
}

void aac_download_file(GF_InputService *plug, char *url)
{
	AACReader *read = (AACReader*) plug->priv;

	read->needs_connection = 1;

	read->dnload = gf_term_download_new(read->service, url, 0, AAC_NetIO, read);
	if (!read->dnload ) {
		read->needs_connection = 0;
		gf_term_on_connect(read->service, NULL, GF_NOT_SUPPORTED);
	}
	/*service confirm is done once fetched*/
}


static GF_Err AAC_ConnectService(GF_InputService *plug, GF_ClientService *serv, const char *url)
{
	char szURL[2048];
	char *ext;
	GF_Err reply;
	AACReader *read = plug->priv;
	read->service = serv;

	if (read->dnload) gf_term_download_del(read->dnload);
	read->dnload = NULL;

	strcpy(szURL, url);
	ext = strrchr(szURL, '#');
	if (ext) ext[0] = 0;

	/*remote fetch*/
	read->is_remote = !aac_is_local(szURL);
	if (read->is_remote) {
		aac_download_file(plug, (char *) szURL);
		return GF_OK;
	}

	reply = GF_OK;
	read->stream = fopen(szURL, "rb");
	if (!read->stream) {
		reply = GF_URL_ERROR;
	} else if (!AAC_ConfigureFromFile(read)) {
		fclose(read->stream);
		read->stream = NULL;
		reply = GF_NOT_SUPPORTED;
	}
	gf_term_on_connect(serv, NULL, reply);
	if (!reply && read->is_inline ) AAC_SetupObject(read);
	return GF_OK;
}

static GF_Err AAC_CloseService(GF_InputService *plug)
{
	AACReader *read = plug->priv;
	if (read->stream) fclose(read->stream);
	read->stream = NULL;
	if (read->dnload) gf_term_download_del(read->dnload);
	read->dnload = NULL;

	if (read->data) free(read->data);
	read->data = NULL;
	gf_term_on_disconnect(read->service, NULL, GF_OK);
	return GF_OK;
}

static GF_Descriptor *AAC_GetServiceDesc(GF_InputService *plug, u32 expect_type, const char *sub_url)
{
	AACReader *read = plug->priv;
	/*since we don't handle multitrack in aac, we don't need to check sub_url, only use expected type*/

	/*override default*/
	if (expect_type==GF_MEDIA_OBJECT_UNDEF) expect_type=GF_MEDIA_OBJECT_AUDIO;

	/*audio object*/
	if (expect_type==GF_MEDIA_OBJECT_AUDIO) {
		GF_ESD *esd;
		GF_ObjectDescriptor *od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
		od->objectDescriptorID = 1;
		esd = AAC_GetESD(read);
		esd->OCRESID = 0;
		gf_list_add(od->ESDescriptors, esd);
		return (GF_Descriptor *) od;
	}
	read->is_inline = 1;
	/*inline scene: no specific service*/
	return NULL;
}

static GF_Err AAC_ConnectChannel(GF_InputService *plug, LPNETCHANNEL channel, const char *url, Bool upstream)
{
	u32 ES_ID;
	GF_Err e;
	AACReader *read = plug->priv;

	e = GF_SERVICE_ERROR;
	if (read->ch==channel) goto exit;

	e = GF_STREAM_NOT_FOUND;
	if (strstr(url, "ES_ID")) {
		sscanf(url, "ES_ID=%d", &ES_ID);
	}
	/*URL setup*/
	else if (!read->ch && AAC_CanHandleURL(plug, url)) ES_ID = 1;

	if (ES_ID==1) {
		read->ch = channel;
		e = GF_OK;
	}

exit:
	gf_term_on_connect(read->service, channel, e);
	return e;
}

static GF_Err AAC_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
	AACReader *read = plug->priv;

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

static GF_Err AAC_ServiceCommand(GF_InputService *plug, GF_NetworkCommand *com)
{
	AACReader *read = plug->priv;


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
		if (read->stream) fseek(read->stream, 0, SEEK_SET);

		if (read->ch == com->base.on_channel) { 
			read->done = 0; 
			/*PLAY after complete download, estimate duration*/
			if (!read->is_remote && !read->duration) {
				AAC_ConfigureFromFile(read);
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


static GF_Err AAC_ChannelGetSLP(GF_InputService *plug, LPNETCHANNEL channel, char **out_data_ptr, u32 *out_data_size, GF_SLHeader *out_sl_hdr, Bool *sl_compressed, GF_Err *out_reception_status, Bool *is_new_data)
{
	u32 pos, start_from;
	Bool sync;
	GF_BitStream *bs;
	ADTSHeader hdr;
	AACReader *read = plug->priv;

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
		pos = ftell(read->stream);
		sync = ADTS_SyncFrame(bs, !read->is_remote, &hdr);
		if (!sync) {
			gf_bs_del(bs);
			if (!read->dnload) {
				*out_reception_status = GF_EOS;
				read->done = 1;
			} else {
				fseek(read->stream, pos, SEEK_SET);
				*out_reception_status = GF_OK;
			}
			return GF_OK;
		}

		if (!hdr.frame_size) {
			gf_bs_del(bs);
			*out_reception_status = GF_EOS;
			read->done = 1;
			return GF_OK;
		}
		read->data_size = hdr.frame_size;
		read->nb_samp = 1024;
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

		read->data = malloc(sizeof(char) * (read->data_size+read->pad_bytes));
		gf_bs_read_data(bs, read->data, read->data_size);
		if (read->pad_bytes) memset(read->data + read->data_size, 0, sizeof(char) * read->pad_bytes);
		gf_bs_del(bs);
	}
	*out_sl_hdr = read->sl_hdr;
	*out_data_ptr = read->data;
	*out_data_size = read->data_size;
	return GF_OK;
}

static GF_Err AAC_ChannelReleaseSLP(GF_InputService *plug, LPNETCHANNEL channel)
{
	AACReader *read = plug->priv;

	if (read->ch == channel) {
		if (!read->data) return GF_BAD_PARAM;
		free(read->data);
		read->data = NULL;
		read->current_time += read->nb_samp;
		return GF_OK;
	}
	return GF_BAD_PARAM;
}

GF_InputService *AAC_Load()
{
	AACReader *reader;
	GF_InputService *plug = malloc(sizeof(GF_InputService));
	memset(plug, 0, sizeof(GF_InputService));
	GF_REGISTER_MODULE_INTERFACE(plug, GF_NET_CLIENT_INTERFACE, "GPAC AAC Reader", "gpac distribution")

	plug->CanHandleURL = AAC_CanHandleURL;
	plug->ConnectService = AAC_ConnectService;
	plug->CloseService = AAC_CloseService;
	plug->GetServiceDescriptor = AAC_GetServiceDesc;
	plug->ConnectChannel = AAC_ConnectChannel;
	plug->DisconnectChannel = AAC_DisconnectChannel;
	plug->ServiceCommand = AAC_ServiceCommand;
	/*we do support pull mode*/
	plug->ChannelGetSLP = AAC_ChannelGetSLP;
	plug->ChannelReleaseSLP = AAC_ChannelReleaseSLP;

	reader = malloc(sizeof(AACReader));
	memset(reader, 0, sizeof(AACReader));
	plug->priv = reader;
	return plug;
}

void AAC_Delete(void *ifce)
{
	GF_InputService *plug = (GF_InputService *) ifce;
	AACReader *read = plug->priv;
	free(read);
	free(plug);
}

GF_EXPORT
Bool QueryInterface(u32 InterfaceType) 
{
	if (InterfaceType == GF_NET_CLIENT_INTERFACE) return 1;
#ifdef GPAC_HAS_FAAD
	if (InterfaceType == GF_MEDIA_DECODER_INTERFACE) return 1;
#endif
	return 0;
}

#ifdef GPAC_HAS_FAAD
GF_BaseDecoder *NewFAADDec();
void DeleteFAADDec(GF_BaseDecoder *ifcg);
#endif

GF_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType) 
{
	if (InterfaceType == GF_NET_CLIENT_INTERFACE) return (GF_BaseInterface *)AAC_Load();
#ifdef GPAC_HAS_FAAD
	if (InterfaceType == GF_MEDIA_DECODER_INTERFACE) return (GF_BaseInterface *)NewFAADDec();
#endif
	return NULL;
}

GF_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
#ifdef GPAC_HAS_FAAD
	case GF_MEDIA_DECODER_INTERFACE:
		DeleteFAADDec((GF_BaseDecoder *)ifce);
		break;
#endif
	case GF_NET_CLIENT_INTERFACE:
		AAC_Delete(ifce);
		break;
	}
}
