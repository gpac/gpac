/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Cyril Concolato / Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / mp4 simple streamer application
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

#include <gpac/isomedia.h>
#include <gpac/ietf.h>
#include <gpac/config_file.h>
#include <gpac/base_coding.h>
#include <gpac/internal/media_dev.h>
#include <gpac/filestreamer.h>


typedef struct __tag_rtp_pck 
{
	struct __tag_rtp_pck *next;

	GF_RTPHeader header;
	char *payload;
	u32 payload_len;
} RTP_Packet;

typedef struct __tag_rtp_stream
{
	struct __tag_rtp_stream *next;

	u32 current_au;
	u32 nb_aus;

	u32 port;
	GF_RTPChannel *channel; 
	GP_RTPPacketizer *packetizer;

	GF_ISOSample  *au; // the AU
	u32 track;
	u32 sample_duration;
	u32 sample_desc_index;
	/*normalized DTS in micro-sec*/
	u64 microsec_dts;

	/* The previous packet which could not be sent in previous burst*/
	RTP_Packet *pck_queue;
	/* The current packet being formed */
	RTP_Packet packet;
	u32 ts_offset, microsec_ts_offset;
	u32 next_ts;

	Double ts_scale, microsec_ts_scale;
	/*NALU size for H264/AVC*/
	u32 avc_nalu_size;

	struct __file_streamer *streamer;
} RTP_Stream;


struct __file_streamer
{
	u32 path_mtu;
	char *dest_ip;

	u32 looping; // 1: play the media in a loop, 0 play once
	char *filename;
	GF_ISOFile *mp4File;
	u32 timelineOrigin; // time when the first burts was sent. this time <=> (CTS=0)
	Bool force_mpeg4_generic;

	/*list of streams in session*/
	RTP_Stream *stream;

	/*to sync looping sessions with tracks of # length*/
	u32 duration;
} RTP_Session;


static void rtp_flush_channel(RTP_Stream *rtp)
{
	RTP_Packet *pck = rtp->pck_queue;
	while (pck) {
		RTP_Packet *tmp = pck;

		gf_rtp_send_packet(rtp->channel, &pck->header, 0, 0, pck->payload, pck->payload_len);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("RTP SN %u - TS %u - M %u - Size %u\n", pck->header.SequenceNumber, pck->header.TimeStamp, pck->header.Marker, pck->payload_len + 12));

		pck = pck->next;
		free(tmp->payload);
		free(tmp);
	}
	rtp->pck_queue = NULL;
}


/*
 * The RTP packetiser has added data to the current RTP packet
 */
static void on_pck_data(void *cbk, char *data, u32 data_size, Bool is_head) 
{
	RTP_Stream *rtp = cbk;
	if (!data ||!data_size) return;

	if (!rtp->packet.payload_len) {
		rtp->packet.payload = malloc(data_size);
		memcpy(rtp->packet.payload, data, data_size);
		rtp->packet.payload_len = data_size;
	} else {
		rtp->packet.payload = realloc(rtp->packet.payload, rtp->packet.payload_len + data_size);
		if (!is_head) {
			memcpy(rtp->packet.payload+rtp->packet.payload_len, data, data_size);
		} else {
			memmove(rtp->packet.payload+data_size, rtp->packet.payload, rtp->packet.payload_len);
			memcpy(rtp->packet.payload, data, data_size);
		}
		rtp->packet.payload_len += data_size;
	}	

} /* OnData */

/*
 * The RTP packetizer is starting a new RTP packet and is giving the header
 */
static void on_pck_new(void *cbk, GF_RTPHeader *header)
{
	RTP_Stream *rtp = cbk;
	if (!header) return;
	memcpy(&rtp->packet.header, header, sizeof(GF_RTPHeader));
} 

/*
 * The RTP packetiser is done with the current RTP packet
 * the header may have changed since the beginning of the packet (OnNewPacket)
 * 
 */
static void on_pck_done(void *cbk, GF_RTPHeader *header) 
{
	RTP_Stream *rtp = cbk;
	GF_Err e = gf_rtp_send_packet(rtp->channel, header, 0, 0, rtp->packet.payload, rtp->packet.payload_len);
	
#ifndef GPAC_DISABLE_LOG
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("Error %s sending RTP packet\n", gf_error_to_string(e)));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("RTP SN %u - TS %u - M %u - Size %u\n", rtp->packet.header.SequenceNumber, rtp->packet.header.TimeStamp, rtp->packet.header.Marker, rtp->packet.payload_len + 12));
	}
#endif
	free(rtp->packet.payload);				
	rtp->packet.payload = NULL;
	rtp->packet.payload_len = 0;
}


static GF_Err gf_streamer_setup_sdp(GF_FileStreamer *streamer, char*sdpfilename, char **out_sdp_buffer) 
{	
	RTP_Stream *rtp;
	FILE *sdp_out;
	char filename[GF_MAX_PATH];
	char mediaName[30], payloadName[30];
	char sdpLine[20000];

	strcpy(filename, sdpfilename ? sdpfilename : "videosession.sdp");
	sdp_out = fopen(filename, "wt");
	if (!sdp_out) return GF_IO_ERR;

	if (!out_sdp_buffer) {
		sprintf(sdpLine, "v=0");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "o=MP4Streamer 3357474383 1148485440000 IN IP%d %s", gf_net_is_ipv6(streamer->dest_ip) ? 6 : 4, streamer->dest_ip);
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "s=livesession");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "i=This is an MP4 time-sliced Streaming demo");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "u=http://gpac.sourceforge.net");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "e=admin@");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "c=IN IP%d %s", gf_net_is_ipv6(streamer->dest_ip) ? 6 : 4, streamer->dest_ip);
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "t=0 0");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "a=x-copyright: Streamed with GPAC (C)2000-200X - http://gpac.sourceforge.net\n");
		fprintf(sdp_out, "%s\n", sdpLine);
	}

	rtp = streamer->stream;
	while (rtp) {
		gf_rtp_builder_get_payload_name(rtp->packetizer, payloadName, mediaName);
		
		sprintf(sdpLine, "m=%s %d RTP/%s %d", mediaName, rtp->port, rtp->packetizer->slMap.IV_length ? "SAVP" : "AVP", rtp->packetizer->PayloadType);
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "a=rtpmap:%d %s/%d", rtp->packetizer->PayloadType, payloadName, rtp->packetizer->sl_config.timestampResolution);
		fprintf(sdp_out, "%s\n", sdpLine);
		
		if (gf_isom_get_media_type(streamer->mp4File, rtp->track) == GF_ISOM_MEDIA_VISUAL) {
			u32 w, h;
			w = h = 0;
			gf_isom_get_visual_info(streamer->mp4File, rtp->track, 1, &w, &h);
			if (rtp->packetizer->rtp_payt == GF_RTP_PAYT_H263) {
				sprintf(sdpLine, "a=cliprect:0,0,%d,%d", h, w);
			}
			/*extensions for some mobile phones*/
			sprintf(sdpLine, "a=framesize:%d %d-%d", rtp->packetizer->PayloadType, w, h);
		}
		/*AMR*/
		if ((rtp->packetizer->rtp_payt == GF_RTP_PAYT_AMR) || (rtp->packetizer->rtp_payt == GF_RTP_PAYT_AMR_WB)) {
			sprintf(sdpLine, "a=fmtp:%d octet-align", rtp->packetizer->PayloadType);
			fprintf(sdp_out, "%s\n", sdpLine);
		}
		/*Text*/
		else if (rtp->packetizer->rtp_payt == GF_RTP_PAYT_3GPP_TEXT) {
			gf_hinter_format_ttxt_sdp(rtp->packetizer, payloadName, sdpLine, streamer->mp4File, rtp->track);
			fprintf(sdp_out, "%s\n", sdpLine);
		}
		/*EVRC/SMV in non header-free mode*/
		else if ((rtp->packetizer->rtp_payt == GF_RTP_PAYT_EVRC_SMV) && (rtp->packetizer->auh_size>1)) {
			sprintf(sdpLine, "a=fmtp:%d maxptime=%d", rtp->packetizer->PayloadType, rtp->packetizer->auh_size*20);
			fprintf(sdp_out, "%s\n", sdpLine);
		}
		/*H264/AVC*/
		else if (rtp->packetizer->rtp_payt == GF_RTP_PAYT_H264_AVC) {
			GF_AVCConfig *avcc = gf_isom_avc_config_get(streamer->mp4File, rtp->track, 1);
			sprintf(sdpLine, "a=fmtp:%d profile-level-id=%02X%02X%02X; packetization-mode=1", rtp->packetizer->PayloadType, avcc->AVCProfileIndication, avcc->profile_compatibility, avcc->AVCLevelIndication);
			if (gf_list_count(avcc->pictureParameterSets) || gf_list_count(avcc->sequenceParameterSets)) {
				u32 i, count, b64s;
				char b64[200];
				strcat(sdpLine, "; sprop-parameter-sets=");
				count = gf_list_count(avcc->sequenceParameterSets);
				for (i=0; i<count; i++) {
					GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *)gf_list_get(avcc->sequenceParameterSets, i);
					b64s = gf_base64_encode(sl->data, sl->size, b64, 200);
					b64[b64s]=0;
					strcat(sdpLine, b64);
					if (i+1<count) strcat(sdpLine, ",");
				}
				if (i) strcat(sdpLine, ",");
				count = gf_list_count(avcc->pictureParameterSets);
				for (i=0; i<count; i++) {
					GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *)gf_list_get(avcc->pictureParameterSets, i);
					b64s = gf_base64_encode(sl->data, sl->size, b64, 200);
					b64[b64s]=0;
					strcat(sdpLine, b64);
					if (i+1<count) strcat(sdpLine, ",");
				}
			}
			fprintf(sdp_out, "%s\n", sdpLine);
			rtp->avc_nalu_size = avcc->nal_unit_size;
			gf_odf_avc_cfg_del(avcc);
		}
		/*MPEG-4 decoder config*/
		else if (rtp->packetizer->rtp_payt==GF_RTP_PAYT_MPEG4) {
			GF_DecoderConfig *dcd;
			dcd = gf_isom_get_decoder_config(streamer->mp4File, rtp->track, 1);

			if (dcd && dcd->decoderSpecificInfo && dcd->decoderSpecificInfo->data) {
				gf_rtp_builder_format_sdp(rtp->packetizer, payloadName, sdpLine, dcd->decoderSpecificInfo->data, dcd->decoderSpecificInfo->dataLength);
			} else {
				gf_rtp_builder_format_sdp(rtp->packetizer, payloadName, sdpLine, NULL, 0);
			}
			if (dcd) gf_odf_desc_del((GF_Descriptor *)dcd);

			if (rtp->packetizer->slMap.IV_length) {
				const char *kms;
				gf_isom_get_ismacryp_info(streamer->mp4File, rtp->track, 1, NULL, NULL, NULL, NULL, &kms, NULL, NULL, NULL);
				if (!strnicmp(kms, "(key)", 5) || !strnicmp(kms, "(ipmp)", 6) || !strnicmp(kms, "(uri)", 5)) {
					strcat(sdpLine, "; ISMACrypKey=");
				} else {
					strcat(sdpLine, "; ISMACrypKey=(uri)");
				}
				strcat(sdpLine, kms);
			}

			fprintf(sdp_out, "%s\n", sdpLine);
		}
		/*MPEG-4 Audio LATM*/
		else if (rtp->packetizer->rtp_payt==GF_RTP_PAYT_LATM) { 
			GF_DecoderConfig *dcd;
			GF_BitStream *bs; 
			char *config_bytes; 
			u32 config_size; 

			/* form config string */ 
			bs = gf_bs_new(NULL, 32, GF_BITSTREAM_WRITE); 
			gf_bs_write_int(bs, 0, 1); /* AudioMuxVersion */ 
			gf_bs_write_int(bs, 1, 1); /* all streams same time */ 
			gf_bs_write_int(bs, 0, 6); /* numSubFrames */ 
			gf_bs_write_int(bs, 0, 4); /* numPrograms */ 
			gf_bs_write_int(bs, 0, 3); /* numLayer */ 

			/* audio-specific config */ 
			dcd = gf_isom_get_decoder_config(streamer->mp4File, rtp->track, 1); 
			if (dcd) { 
				gf_bs_write_data(bs, dcd->decoderSpecificInfo->data, dcd->decoderSpecificInfo->dataLength); 
				gf_odf_desc_del((GF_Descriptor *)dcd); 
			} 

			/* other data */ 
			gf_bs_write_int(bs, 0, 3); /* frameLengthType */ 
			gf_bs_write_int(bs, 0xff, 8); /* latmBufferFullness */ 
			gf_bs_write_int(bs, 0, 1); /* otherDataPresent */ 
			gf_bs_write_int(bs, 0, 1); /* crcCheckPresent */ 
			gf_bs_get_content(bs, &config_bytes, &config_size); 
			gf_bs_del(bs); 

			gf_rtp_builder_format_sdp(rtp->packetizer, payloadName, sdpLine, config_bytes, config_size); 
			fprintf(sdp_out, "%s\n", sdpLine);
			free(config_bytes); 
		}
		rtp = rtp->next;
	}
	fprintf(sdp_out, "\n");

	fclose(sdp_out);
	if (out_sdp_buffer) {
		u32 size;
		sdp_out = fopen(filename, "r");
		fseek(sdp_out, 0, SEEK_END);
		size = ftell(sdp_out);
		fseek(sdp_out, 0, SEEK_SET);
		if (*out_sdp_buffer) free(*out_sdp_buffer);
		*out_sdp_buffer = malloc(sizeof(char)*(size+1));
		size = fread(*out_sdp_buffer, 1, size, sdp_out);
		fclose(sdp_out);
		(*out_sdp_buffer)[size]=0;
	}

	return GF_OK;
} 

GF_Err gf_streamer_write_sdp(GF_FileStreamer *streamer, char*sdpfilename) 
{
	return gf_streamer_setup_sdp(streamer, sdpfilename, NULL);
}

GF_Err gf_streamer_get_sdp(GF_FileStreamer *streamer, char **out_sdp_buffer) 
{
	return gf_streamer_setup_sdp(streamer, NULL, out_sdp_buffer);
}

static GF_Err rtp_init_channel(RTP_Stream *rtp, u32 path_mtu, char * dest, int port, int ttl, char *ifce_addr)
{
	GF_RTSPTransport tr;
	GF_Err res;

	rtp->channel = gf_rtp_new();
	gf_rtp_set_ports(rtp->channel, 0);

	memset(&tr, 0, sizeof(GF_RTSPTransport));

	tr.IsUnicast = gf_sk_is_multicast_address(dest) ? 0 : 1;
	tr.Profile="RTP/AVP";
	tr.destination = dest;
	tr.source = "0.0.0.0";
	tr.IsRecord = 0;
	tr.Append = 0;
	tr.SSRC = rand();
	tr.TTL = ttl;

	tr.port_first = port;
	tr.port_last = port+1;
	if (tr.IsUnicast) {
		tr.client_port_first = port;
		tr.client_port_last  = port+1;
	} else {
		tr.source = dest;
	}

	res = gf_rtp_setup_transport(rtp->channel, &tr, dest);
	if (res !=0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("Cannot setup RTP transport info: %s\n", gf_error_to_string(res) ));
		return res;
	}

	res = gf_rtp_initialize(rtp->channel, 0, 1, path_mtu, 0, 0, ifce_addr);
	if (res !=0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("Cannot initialize RTP sockets: %s\n", gf_error_to_string(res) ));
		return res;
	}
	return GF_OK;
} 

void gf_streamer_reset(GF_FileStreamer *streamer, Bool is_loop)
{
	RTP_Stream *rtp;
	if (!streamer) return;
	rtp = streamer->stream;
	while (rtp) {
		if (is_loop) {
			Double scale = rtp->packetizer->sl_config.timestampResolution/1000.0;
			rtp->ts_offset += (u32) (streamer->duration * scale);
			rtp->microsec_ts_offset = (u32) (rtp->ts_offset*(1000000.0/rtp->packetizer->sl_config.timestampResolution)) + streamer->timelineOrigin;
		} else {
			rtp->ts_offset += 0;
			rtp->microsec_ts_offset = 0;
		}
		rtp->current_au = 0;
		rtp = rtp->next;
	}
	if (is_loop) streamer->timelineOrigin = 0;
}

GF_Err gf_streamer_send_next_packet(GF_FileStreamer *streamer, s32 send_ahead_delay, s32 max_sleep_time) 
{
	GF_Err e = GF_OK;
	RTP_Stream *rtp, *to_send;
	u32 time;
	s32 diff;
	u64 min_ts;

	if (!streamer) return GF_BAD_PARAM;
	
	/*browse all sessions and locate most mature stream*/
	to_send = NULL;
	min_ts = (u64) -1;

	time = gf_sys_clock();

	/*init session timeline - all sessions are sync'ed for packet scheduling purposes*/
	if (!streamer->timelineOrigin)
		streamer->timelineOrigin = time*1000;

	rtp = streamer->stream;
	while (rtp) {
		/*load next AU*/
		if (!rtp->au) {
			if (rtp->current_au >= rtp->nb_aus) {
				Double scale;
				if (!streamer->looping) {
					rtp = rtp->next;
					continue;
				}
				/*increment ts offset*/
				scale = rtp->packetizer->sl_config.timestampResolution/1000.0;
				rtp->ts_offset += (u32) (streamer->duration * scale);
				rtp->microsec_ts_offset = (u32) (rtp->ts_offset*(1000000.0/rtp->packetizer->sl_config.timestampResolution)) + streamer->timelineOrigin;
				rtp->current_au = 0;
			}

			rtp->au = gf_isom_get_sample(streamer->mp4File, rtp->track, rtp->current_au + 1, &rtp->sample_desc_index);
			rtp->current_au ++;
			if (rtp->au) {
				u64 ts;
				rtp->sample_duration = gf_isom_get_sample_duration(streamer->mp4File, rtp->track, rtp->current_au);
				rtp->sample_duration = (u32)(rtp->sample_duration*rtp->ts_scale);

				rtp->microsec_dts = (u64) (rtp->microsec_ts_scale * (s64) (rtp->au->DTS)) + rtp->microsec_ts_offset + streamer->timelineOrigin;

				ts = (u64) (rtp->ts_scale * (s64) (rtp->au->DTS));
				rtp->packetizer->sl_header.decodingTimeStamp = ts + rtp->ts_offset;
										
				ts = (u64) (rtp->ts_scale * (s64) (rtp->au->DTS+rtp->au->CTS_Offset));
				rtp->packetizer->sl_header.compositionTimeStamp = ts + rtp->ts_offset;
					
				rtp->packetizer->sl_header.randomAccessPointFlag = rtp->au->IsRAP;
			}
		}

		/*check timing*/
		if (rtp->au) {
			if (min_ts > rtp->microsec_dts) {
				min_ts = rtp->microsec_dts;
				to_send = rtp;
			}
		}

		rtp = rtp->next;
	}

	/*no input data ...*/
	if( !to_send) return GF_EOS;
	min_ts /= 1000;

	if (max_sleep_time) {
		diff = ((u32) min_ts) - gf_sys_clock();	
		if (diff>max_sleep_time) 
			return GF_OK;
	}

	/*sleep until TS is mature*/
	while (1) {
		diff = ((u32) min_ts) - gf_sys_clock();
		
		if (diff > send_ahead_delay) {
			gf_sleep(1);
		} else {
			if (diff<0) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_RTP, ("WARNING: RTP session %s stream %d - sending packet %d ms too late\n", streamer->filename, to_send->track, -diff));
			}
			break;
		}
	}

	/*send packets*/

	/*unpack nal units*/
	if (to_send->avc_nalu_size) {
		u32 v, size;
		u32 remain = to_send->au->dataLength;
		char *ptr = to_send->au->data;

		to_send->packetizer->sl_header.accessUnitStartFlag = 1;
		to_send->packetizer->sl_header.accessUnitEndFlag = 0;
		while (remain) {
			size = 0;
			v = to_send->avc_nalu_size;
			while (v) {
				size |= (u8) *ptr;
				ptr++;
				remain--;
				v-=1;
				if (v) size<<=8;
			}
			remain -= size;
			to_send->packetizer->sl_header.accessUnitEndFlag = remain ? 0 : 1;
			e = gf_rtp_builder_process(to_send->packetizer, ptr, size, (u8) !remain, to_send->au->dataLength, to_send->sample_duration, (u8) to_send->sample_desc_index );
			ptr += size;
			to_send->packetizer->sl_header.accessUnitStartFlag = 0;
		}
	} else {
		e = gf_rtp_builder_process(to_send->packetizer, to_send->au->data, to_send->au->dataLength, (u8) 1, to_send->au->dataLength, to_send->sample_duration, (u8) to_send->sample_desc_index);
	}
	/*delete sample*/
	gf_isom_sample_del(&to_send->au);

	return e;
}


static u16 check_next_port(GF_FileStreamer *streamer, u16 first_port)
{
	RTP_Stream *rtp = streamer->stream;
	while (rtp) {
		if (rtp->port==first_port) {
			return check_next_port(streamer, (u16) (first_port+2) );
		}
		rtp = rtp->next;
	}
	return first_port;
}

GF_FileStreamer *gf_streamer_new(const char *file_name, const char *ip_dest, u16 port, Bool loop, Bool force_mpeg4, u32 path_mtu, u32 ttl, char *ifce_addr)
{
	GF_FileStreamer *streamer;
	GF_Err e = GF_OK;
	const char *opt = NULL;
	const char *dest_ip;
	GF_Config *configFile = NULL;	
	u32 i;	
	u8 payt;			
	GF_ISOFile *file;
	RTP_Stream *rtp, *prev_stream;
	u16 first_port;
	u32 nb_tracks;
	u32 sess_data_size;

	if (!ip_dest) ip_dest = "127.0.0.1";
	if (!port) port = 7000;
	if (!path_mtu) path_mtu = 1450;

	GF_SAFEALLOC(streamer, GF_FileStreamer);
	streamer->path_mtu = path_mtu;
	streamer->dest_ip = strdup(ip_dest);

	dest_ip = ip_dest;
	payt = 96;


	file = gf_isom_open(file_name, GF_ISOM_OPEN_READ, NULL);
	if (!file) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("Error opening file %s: %s\n", opt, gf_error_to_string(gf_isom_last_error(NULL))));
		return NULL;
	}

	streamer->mp4File = file;
	streamer->looping = loop;
	streamer->force_mpeg4_generic = force_mpeg4;
	first_port = port;

	sess_data_size = 0;
	prev_stream = NULL;
	
	nb_tracks = gf_isom_get_track_count(streamer->mp4File);
	for (i=0; i<nb_tracks; i++) {
		u32 mediaTimescale; 
		u32 mediaSize;
		u32 mediaDuration;
		u32 flags = 0;

		switch (gf_isom_get_media_type(streamer->mp4File, i+1)) {
		case GF_ISOM_MEDIA_VISUAL:
		case GF_ISOM_MEDIA_AUDIO:
		case GF_ISOM_MEDIA_TEXT:
		case GF_ISOM_MEDIA_OD:
		case GF_ISOM_MEDIA_SCENE:
			break;
		default:
			continue;
		}

		GF_SAFEALLOC(rtp, RTP_Stream);
		if (prev_stream) prev_stream->next = rtp;
		else streamer->stream = rtp;
		prev_stream = rtp;

		rtp->streamer = streamer;
		rtp->track = i+1;

		rtp->nb_aus = gf_isom_get_sample_count(streamer->mp4File, rtp->track);
		mediaTimescale = gf_isom_get_media_timescale(streamer->mp4File, rtp->track);
		mediaDuration = (u32)(gf_isom_get_media_duration(streamer->mp4File, rtp->track)*1000/mediaTimescale); // ms
		mediaSize = (u32)gf_isom_get_media_data_size(streamer->mp4File, rtp->track);

		sess_data_size += mediaSize;
		if (mediaDuration > streamer->duration) streamer->duration = mediaDuration;

		rtp->port = check_next_port(streamer, first_port);
		first_port = rtp->port+2;

		e = rtp_init_channel(rtp, streamer->path_mtu + 12, (char *) dest_ip, rtp->port, ttl, ifce_addr);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("Could not initialize RTP Channel: %s\n", gf_error_to_string(e)));
			goto exit;
		}		
		
		/*init packetizer*/
		if (streamer->force_mpeg4_generic) flags = GP_RTP_PCK_SIGNAL_RAP | GP_RTP_PCK_FORCE_MPEG4;

		rtp->packetizer = gf_rtp_packetizer_create_and_init_from_file(streamer->mp4File, rtp->track, rtp,
													on_pck_new, on_pck_done, NULL, on_pck_data,
													streamer->path_mtu, 0, 0, flags, 
													payt, 0, 0, 0);
		if (!rtp->packetizer) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("Could not initialize Packetizer\n"));
			e = GF_SERVICE_ERROR;
			goto exit;
		}
		payt++;

		rtp->ts_scale = rtp->packetizer->sl_config.timestampResolution;
		rtp->ts_scale /= gf_isom_get_media_timescale(streamer->mp4File, rtp->track);

		rtp->microsec_ts_scale = 1000000;
		rtp->microsec_ts_scale /= gf_isom_get_media_timescale(streamer->mp4File, rtp->track);
	}
	return streamer;

exit:
	free(streamer);
	return NULL;
} 

void gf_streamer_del(GF_FileStreamer *streamer) 
{
	RTP_Stream *rtp = streamer->stream;
	while (rtp) {
		RTP_Stream *tmp = rtp;
		rtp_flush_channel(rtp);
		if (rtp->au) gf_isom_sample_del(&rtp->au);
		if (rtp->channel) gf_rtp_del(rtp->channel);
		if (rtp->packetizer) gf_rtp_builder_del(rtp->packetizer);
		if (rtp->packet.payload) free(rtp->packet.payload);	
		rtp = rtp->next;
		free(tmp);
	}
	if (streamer->mp4File) gf_isom_close(streamer->mp4File);
	free(streamer->dest_ip);
	free(streamer);
}
