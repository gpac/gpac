/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean le Feuvre
 *				Copyright (c) 2005-200X ENST
 *					All rights reserved
 *
 *  This file is part of GPAC / SAF reader module
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
#include <gpac/thread.h>

typedef struct
{
	LPNETCHANNEL ch;
	u32 au_sn, stream_id, ts_res, buffer_min;
	GF_ESD *esd;
} SAFChannel;

enum
{
	SAF_FILE_LOCAL,
	SAF_FILE_REMOTE,
	SAF_LIVE_STREAM
};

typedef struct
{
	GF_ClientService *service;
	GF_List *channels;
	Bool needs_connection;

	u32 saf_type;

	/*file downloader*/
	GF_DownloadSession * dnload;

	/*SAF buffer for both lcoal, remote and live streams*/
	char *saf_data;
	u32 saf_size, alloc_size;

	/*local file playing*/
	GF_Thread *th;
	FILE *stream;
	u32 run_state;
	u32 start_range, end_range;
	Double duration;
	u32 nb_playing;
} SAFIn;

static GFINLINE SAFChannel *saf_get_channel(SAFIn *saf, u32 stream_id, LPNETCHANNEL a_ch)
{
	SAFChannel *ch;
	u32 i=0;
	while ((ch = (SAFChannel *)gf_list_enum(saf->channels, &i))) {
		if (ch->stream_id==stream_id) return ch;
		if (a_ch && (ch->ch==a_ch)) return ch;
	}
	return NULL;
}

static Bool SAF_CanHandleURL(GF_InputService *plug, const char *url)
{
	char *sExt;
	sExt = strrchr(url, '.');
	if (!sExt) return 0;
	if (gf_term_check_extension(plug, "application/x-saf", "saf lsr", "SAF Rich Media", sExt)) return 1;
	return 0;
}

static void SAF_Regulate(SAFIn *read)
{
	GF_NetworkCommand com;
	SAFChannel *ch;

	com.command_type = GF_NET_CHAN_BUFFER_QUERY;
	/*sleep untill the buffer occupancy is too low - note that this work because all streams in this
	demuxer are synchronized*/
	while (read->run_state) {
		u32 min_occ = (u32) -1;
		u32 i=0;
		while ( (ch = (SAFChannel *)gf_list_enum(read->channels, &i))) {
			com.base.on_channel = ch->ch;
			gf_term_on_command(read->service, &com, GF_OK);
			if (com.buffer.occupancy < ch->buffer_min) return;
			if (com.buffer.occupancy) min_occ = MIN(min_occ, com.buffer.occupancy - ch->buffer_min);
		}
		if (min_occ == (u32) -1) break;
		//fprintf(stdout, "Regulating SAF demux - sleeping for %d ms\n", min_occ);
		gf_sleep(min_occ);
	}
}

static void SAF_NetIO(void *cbk, GF_NETIO_Parameter *param)
{
	GF_Err e;
	Bool is_rap, go;
	SAFChannel *ch;
	u32 cts, au_sn, au_size, bs_pos, type, i, stream_id;
	GF_BitStream *bs;
	GF_SLHeader sl_hdr;
	
	SAFIn *read = (SAFIn *) cbk;

	e = param->error;
	/*done*/
	if (param->msg_type==GF_NETIO_DATA_TRANSFERED) {
		if (read->stream && (read->saf_type==SAF_FILE_REMOTE)) read->saf_type = SAF_FILE_LOCAL;
		return;
	} else {
		/*handle service message*/
		gf_term_download_update_stats(read->dnload);
		if (param->msg_type!=GF_NETIO_DATA_EXCHANGE) {
			if (e<0) {
				if (read->needs_connection) {
					read->needs_connection = 0;
					gf_term_on_connect(read->service, NULL, e);
				}
				return;
			}
			if (read->needs_connection) {
				u32 total_size;
				gf_dm_sess_get_stats(read->dnload, NULL, NULL, &total_size, NULL, NULL, NULL);
				if (!total_size) read->saf_type = SAF_LIVE_STREAM;
			}
			return;
		}
	}
	if (!param->size) return;

	if (!read->run_state) return;

	if (read->alloc_size < read->saf_size + param->size) {
		read->saf_data = (char*)realloc(read->saf_data, sizeof(char)*(read->saf_size + param->size) );
		read->alloc_size = read->saf_size + param->size;
	}
	memcpy(read->saf_data + read->saf_size, param->data, sizeof(char)*param->size);
	read->saf_size += param->size;

	/*first AU not complete yet*/
	if (read->saf_size<10) return;

	bs = gf_bs_new(read->saf_data, read->saf_size, GF_BITSTREAM_READ);
	bs_pos = 0;

	go = 1;
	while (go) {
		u32 avail = (u32) gf_bs_available(bs);
		bs_pos = (u32) gf_bs_get_position(bs);

		if (avail<10) break;

		is_rap = gf_bs_read_int(bs, 1);
		au_sn = gf_bs_read_int(bs, 15);
		gf_bs_read_int(bs, 2);
		cts = gf_bs_read_int(bs, 30);
		au_size = gf_bs_read_int(bs, 16);
		avail-=8;

		if (au_size > avail) break;
		assert(au_size>=2);

		is_rap = 1;

		type = gf_bs_read_int(bs, 4);
		stream_id = gf_bs_read_int(bs, 12);
		au_size -= 2;

		ch = saf_get_channel(read, stream_id, NULL);
		switch (type) {
		case 1:
		case 2:
		case 7:
			if (ch) {
				gf_bs_skip_bytes(bs, au_size);
			} else {
				SAFChannel *first = (SAFChannel *)gf_list_get(read->channels, 0);
				GF_SAFEALLOC(ch, SAFChannel);
				ch->stream_id = stream_id;
				ch->esd = gf_odf_desc_esd_new(0);
				ch->esd->ESID = stream_id;
				ch->esd->OCRESID = first ? first->stream_id : stream_id;
				ch->esd->slConfig->useRandomAccessPointFlag = 1;
				ch->esd->slConfig->AUSeqNumLength = 0;
				ch->esd->decoderConfig->objectTypeIndication = gf_bs_read_u8(bs);
				ch->esd->decoderConfig->streamType = gf_bs_read_u8(bs);
				ch->ts_res = ch->esd->slConfig->timestampResolution = gf_bs_read_u24(bs);
				ch->esd->decoderConfig->bufferSizeDB = gf_bs_read_u16(bs);
				au_size -= 7;
				if ((ch->esd->decoderConfig->objectTypeIndication == 0xFF) && (ch->esd->decoderConfig->streamType == 0xFF) ) {
					u16 mimeLen = gf_bs_read_u16(bs);
					gf_bs_skip_bytes(bs, mimeLen);
					au_size -= mimeLen+2;
				}
				if (type==7) {
					u16 urlLen = gf_bs_read_u16(bs);
					ch->esd->URLString = (char*)malloc(sizeof(char)*(urlLen+1));
					gf_bs_read_data(bs, ch->esd->URLString, urlLen);
					ch->esd->URLString[urlLen] = 0;
					au_size -= urlLen+2;
				}
				if (au_size) {
					ch->esd->decoderConfig->decoderSpecificInfo->dataLength = au_size;
					ch->esd->decoderConfig->decoderSpecificInfo->data = (char*)malloc(sizeof(char)*au_size);
					gf_bs_read_data(bs, ch->esd->decoderConfig->decoderSpecificInfo->data, au_size);
				}
				if (ch->esd->decoderConfig->streamType==4) ch->buffer_min=100;
				else if (ch->esd->decoderConfig->streamType==5) ch->buffer_min=400;
				else ch->buffer_min=0;

				if (read->needs_connection && (ch->esd->decoderConfig->streamType==GF_STREAM_SCENE)) {
					gf_list_add(read->channels, ch);
					read->needs_connection = 0;
					gf_term_on_connect(read->service, NULL, GF_OK);
				} else if (read->needs_connection) {
					gf_odf_desc_del((GF_Descriptor *) ch->esd);
					free(ch);
					ch = NULL;
				} else {
					GF_ObjectDescriptor *od;
					gf_list_add(read->channels, ch);

					od = (GF_ObjectDescriptor*)gf_odf_desc_new(GF_ODF_OD_TAG);
					gf_list_add(od->ESDescriptors, ch->esd);
					ch->esd = NULL;
					od->objectDescriptorID = ch->stream_id;
					gf_term_add_media(read->service, (GF_Descriptor*)od, 0);

				}
			}
			break;
		case 4:
			if (ch) {
				bs_pos = (u32) gf_bs_get_position(bs);
				memset(&sl_hdr, 0, sizeof(GF_SLHeader));
				sl_hdr.accessUnitLength = au_size;
				sl_hdr.AU_sequenceNumber = au_sn;
				sl_hdr.compositionTimeStampFlag = 1;
				sl_hdr.compositionTimeStamp = cts;
				sl_hdr.randomAccessPointFlag = is_rap;
				if (read->start_range && (read->start_range*ch->ts_res>cts*1000)) {
					sl_hdr.compositionTimeStamp = read->start_range*ch->ts_res/1000;
				}
				gf_term_on_sl_packet(read->service, ch->ch, read->saf_data+bs_pos, au_size, &sl_hdr, GF_OK);
			}
			gf_bs_skip_bytes(bs, au_size);
			break;
		case 3:
			if (ch) gf_term_on_sl_packet(read->service, ch->ch, NULL, 0, NULL, GF_EOS);
			break;
		case 5:
			go = 0;
			read->run_state = 0;
			i=0;
			while ((ch = (SAFChannel *)gf_list_enum(read->channels, &i))) {
				gf_term_on_sl_packet(read->service, ch->ch, NULL, 0, NULL, GF_EOS);
			}
			break;
		}
	}

	gf_bs_del(bs);
	if (bs_pos) {
		u32 remain = read->saf_size - bs_pos;
		if (remain) memmove(read->saf_data, read->saf_data+bs_pos, sizeof(char)*remain);
		read->saf_size = remain;
	}
	SAF_Regulate(read);
}


u32 SAF_Run(void *_p)
{
	GF_NETIO_Parameter par;
	char data[1024];
	SAFIn *read = (SAFIn *)_p;

	par.msg_type = GF_NETIO_DATA_EXCHANGE;
	par.data = data;

	fseek(read->stream, 0, SEEK_SET);
	read->saf_size=0;
	read->run_state = 1;
	while (read->run_state && !feof(read->stream) ) {
		par.size = fread(data, 1, 1024, read->stream);
		if (!par.size) break;
		SAF_NetIO(read, &par);
	}
	read->run_state = 2;
	return 0;
}

static void SAF_DownloadFile(GF_InputService *plug, char *url)
{
	SAFIn *read = (SAFIn*) plug->priv;

	read->dnload = gf_term_download_new(read->service, url, 0, SAF_NetIO, read);
	if (!read->dnload) {
		read->needs_connection = 0;
		gf_term_on_connect(read->service, NULL, GF_NOT_SUPPORTED);
	}
	/*service confirm is done once fetched*/
}

typedef struct
{
	u32 stream_id;
	u32 ts_res;
} StreamInfo;

static void SAF_CheckFile(SAFIn *read)
{
	u32 nb_streams, i, cts, au_size, au_type, stream_id, ts_res;
	GF_BitStream *bs;
	StreamInfo si[1024];
	fseek(read->stream, 0, SEEK_SET);
	bs = gf_bs_from_file(read->stream, GF_BITSTREAM_READ);

	nb_streams=0;
	while (gf_bs_available(bs)) {
		gf_bs_read_u16(bs);
		gf_bs_read_int(bs, 2);
		cts = gf_bs_read_int(bs, 30);
		au_size = gf_bs_read_int(bs, 16);
		au_type = gf_bs_read_int(bs, 4);
		stream_id = gf_bs_read_int(bs, 12);
		au_size-=2;
		ts_res = 0;
		for (i=0; i<nb_streams; i++) { if (si[i].stream_id==stream_id) ts_res = si[i].ts_res; }
		if (!ts_res) {
			if ((au_type==1) || (au_type==2) || (au_type==7)) {
				gf_bs_read_u16(bs);
				ts_res = gf_bs_read_u24(bs);
				au_size -= 5;
				si[nb_streams].stream_id = stream_id;
				si[nb_streams].ts_res = ts_res;
				nb_streams++;
			}
		}
		if (ts_res && (au_type==4)) {
			Double ts = cts;
			ts /= ts_res;
			if (ts>read->duration) read->duration = ts;
		}
		gf_bs_skip_bytes(bs, au_size);
	}
	gf_bs_del(bs);
	fseek(read->stream, 0, SEEK_SET);
}

static GF_Err SAF_ConnectService(GF_InputService *plug, GF_ClientService *serv, const char *url)
{
	char szURL[2048];
	char *ext;
	SAFIn *read = (SAFIn *)plug->priv;
	read->service = serv;

	if (read->dnload) gf_term_download_del(read->dnload);
	read->dnload = NULL;

	strcpy(szURL, url);
	ext = strrchr(szURL, '#');
	if (ext) ext[0] = 0;

	read->needs_connection = 1;
	read->duration = 0;

	read->saf_type = SAF_FILE_LOCAL;
	/*remote fetch*/
	if (strnicmp(url, "file://", 7) && strstr(url, "://")) {
		read->saf_type = SAF_FILE_REMOTE;
		SAF_DownloadFile(plug, (char *) szURL);
		return GF_OK;
	}

	read->stream = fopen(szURL, "rb");
	if (!read->stream) {
		gf_term_on_connect(serv, NULL, GF_URL_ERROR);
		return GF_OK;
	}
	SAF_CheckFile(read);
	read->th = gf_th_new("SAFDemux");
	/*start playing for tune-in*/
	gf_th_run(read->th, SAF_Run, read);
	return GF_OK;
}

static GF_Err SAF_CloseService(GF_InputService *plug)
{
	SAFIn *read = (SAFIn *)plug->priv;

	if (read->th) {
		if (read->run_state == 1) {
			read->run_state=0;
			while (read->run_state!=2) gf_sleep(0);
		}
		gf_th_del(read->th);
		read->th = NULL;
	}

	if (read->stream) fclose(read->stream);
	read->stream = NULL;
	if (read->dnload) gf_term_download_del(read->dnload);
	read->dnload = NULL;
	gf_term_on_disconnect(read->service, NULL, GF_OK);
	return GF_OK;
}

static GF_Descriptor *SAF_GetServiceDesc(GF_InputService *plug, u32 expect_type, const char *sub_url)
{
	u32 i=0;
	SAFChannel *root;
	SAFIn *read = (SAFIn *)plug->priv;
	GF_ObjectDescriptor *od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_IOD_TAG);

	od->objectDescriptorID = 1;

	while ( (root = (SAFChannel *)gf_list_enum(read->channels, &i))) {
		if (root->esd && (root->esd->decoderConfig->streamType==GF_STREAM_SCENE)) break;
	}
	if (!root) return NULL;

	/*inline scene*/
	gf_list_add(od->ESDescriptors, root->esd);
	root->esd = NULL;
	return (GF_Descriptor *) od;
}

static GF_Err SAF_ConnectChannel(GF_InputService *plug, LPNETCHANNEL channel, const char *url, Bool upstream)
{
	u32 ES_ID;
	SAFChannel *ch;
	GF_Err e;
	SAFIn *read = (SAFIn *)plug->priv;


	ch = saf_get_channel(read, 0, channel);
	if (ch) e = GF_SERVICE_ERROR;

	e = GF_STREAM_NOT_FOUND;
	if (strstr(url, "ES_ID")) {
		sscanf(url, "ES_ID=%d", &ES_ID);
		ch = saf_get_channel(read, ES_ID, NULL);
		if (ch && !ch->ch) {
			ch->ch = channel;
			e = GF_OK;
		}
	}

	gf_term_on_connect(read->service, channel, e);
	return e;
}

static GF_Err SAF_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
	SAFChannel *ch;
	SAFIn *read = (SAFIn *)plug->priv;

	GF_Err e = GF_STREAM_NOT_FOUND;
	ch = saf_get_channel(read, 0, channel);
	if (ch) {
		gf_list_del_item(read->channels, ch);
		if (ch->esd) gf_odf_desc_del((GF_Descriptor*)ch->esd);
		free(ch);
		e = GF_OK;
	}
	gf_term_on_disconnect(read->service, channel, e);
	return GF_OK;
}

static GF_Err SAF_ServiceCommand(GF_InputService *plug, GF_NetworkCommand *com)
{
	SAFIn *read = (SAFIn *)plug->priv;

	if (!com->base.on_channel) return GF_NOT_SUPPORTED;
	switch (com->command_type) {
	case GF_NET_CHAN_SET_PULL:
		return GF_NOT_SUPPORTED;
	case GF_NET_CHAN_INTERACTIVE:
		return GF_OK;
	case GF_NET_CHAN_BUFFER:
		return GF_OK;
	case GF_NET_CHAN_DURATION:
		com->duration.duration = read->duration;
		return GF_OK;
	case GF_NET_CHAN_PLAY:
		if (!read->nb_playing) {
			read->start_range = (u32) (com->play.start_range*1000);
			read->end_range = (u32) (com->play.end_range*1000);
			/*start demuxer*/
			if ((read->saf_type == SAF_FILE_LOCAL) && (read->run_state!=1)) {
				gf_th_run(read->th, SAF_Run, read);
			}
		}
		read->nb_playing++;
		return GF_OK;
	case GF_NET_CHAN_STOP:
		assert(read->nb_playing);
		read->nb_playing--;
		/*stop demuxer*/
		if (!read->nb_playing && (read->run_state==1)) {
			read->run_state=0;
			while (read->run_state!=2) gf_sleep(2);
		}
		return GF_OK;
	default:
		return GF_OK;
	}
}


GF_InputService *NewSAFReader()
{
	SAFIn *reader;
	GF_InputService *plug;
	GF_SAFEALLOC(plug, GF_InputService);
	GF_REGISTER_MODULE_INTERFACE(plug, GF_NET_CLIENT_INTERFACE, "GPAC SAF Reader", "gpac distribution")

	plug->CanHandleURL = SAF_CanHandleURL;
	plug->ConnectService = SAF_ConnectService;
	plug->CloseService = SAF_CloseService;
	plug->GetServiceDescriptor = SAF_GetServiceDesc;
	plug->ConnectChannel = SAF_ConnectChannel;
	plug->DisconnectChannel = SAF_DisconnectChannel;
	plug->ServiceCommand = SAF_ServiceCommand;

	GF_SAFEALLOC(reader, SAFIn);
	reader->channels = gf_list_new();
	plug->priv = reader;
	return plug;
}

void DeleteSAFReader(void *ifce)
{
	GF_InputService *plug = (GF_InputService *) ifce;
	SAFIn *read = (SAFIn *)plug->priv;

	while (gf_list_count(read->channels)) {
		SAFChannel *ch = (SAFChannel *)gf_list_last(read->channels);
		gf_list_rem_last(read->channels);
		if (ch->esd) gf_odf_desc_del((GF_Descriptor *) ch->esd);
		free(ch);
	}
	gf_list_del(read->channels);
	if (read->saf_data) free(read->saf_data);
	free(read);
	free(plug);
}


GF_EXPORT
Bool QueryInterface(u32 InterfaceType)
{
	switch (InterfaceType) {
	case GF_NET_CLIENT_INTERFACE: return 1;
	default: return 0;
	}
}

GF_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	switch (InterfaceType) {
	case GF_NET_CLIENT_INTERFACE: return (GF_BaseInterface *) NewSAFReader();
	default: return NULL;
	}
}

GF_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_NET_CLIENT_INTERFACE:  DeleteSAFReader(ifce); break;
	}
}
