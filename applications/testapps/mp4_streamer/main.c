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

GP_RTPPacketizer *gf_rtp_packetizer_create_and_init_from_file(GF_ISOFile *file, 
															  u32 TrackNum,
															  void *cbk_obj, 
															  void (*OnNewPacket)(void *cbk, GF_RTPHeader *header),
															  void (*OnPacketDone)(void *cbk, GF_RTPHeader *header),
															  void (*OnDataReference)(void *cbk, u32 payload_size, u32 offset_from_orig),
															  void (*OnData)(void *cbk, char *data, u32 data_size, Bool is_head),
															  u32 Path_MTU, 
															  u32 max_ptime, 
															  u32 default_rtp_rate, 
															  u32 flags, 
															  u8 PayloadID, 
															  Bool copy_media, 
															  u32 InterleaveGroupID, 
															  u8 InterleaveGroupPriority);
void gf_hinter_format_ttxt_sdp(GP_RTPPacketizer *builder, char *payload_name, char *sdpLine, GF_ISOFile *file, u32 track);

//------------------------------------------------------------
// Define
//------------------------------------------------------------
#define PATHFILE "."
#define RTP_HEADER_SIZE 12 // in bytes (octets)
#define BASE_PAYT		96

//------------------------------------------------------------
// Typedef
//------------------------------------------------------------
enum
{
	LOG_NONE = 0,
	LOG_BURST,
	LOG_AU,
	LOG_PACKET,
};

typedef struct 
{	
	u32 log_level;
	u32 path_mtu;
	struct __tag_rtp_session *session;
	u8 payt;
	
	Bool burst_mode;

	/*burst mode configuration*/
	u32 burstDuration;
	u32 burstBitRate;
	u32 burstSize;
	u32 offDuration;
	u32 averageBitRate;
	u32 cycleDuration;
	u32 nbBurstSent;
} Streamer;

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
	Bool process_burst;
	/*NALU size for H264/AVC*/
	u32 avc_nalu_size;

	struct __tag_rtp_session *session;
} RTP_Stream;


typedef struct __tag_rtp_session
{
	struct __tag_rtp_session *next;

	u32 id;
	u32 minBurstSize; // max size to fill the burst with the media bitRate
	u32 looping; // 1: play the media in a loop, 0 play once
	char *filename;
	GF_ISOFile *mp4File;
	u32 nbBurstSent; // pour cette session
	u32 timelineOrigin; // time when the first burts was sent. this time <=> (CTS=0)
	u32 nextBurstTime;
	u32 dataLengthInBurst; // total bytes filled in current burst
	s32 drift;
	Bool force_mpeg4_generic;

	/*list of streams in session*/
	RTP_Stream *stream;

	/*to sync looping sessions with tracks of # length*/
	u32 duration;

	Streamer *streamer;
} RTP_Session;


static void rtp_flush_channel(RTP_Stream *rtp)
{
	u32 currentPacketSize; 
	RTP_Packet *pck = rtp->pck_queue;
	while (pck) {
		RTP_Packet *tmp = pck;

		gf_rtp_send_packet(rtp->channel, &pck->header, 0, 0, pck->payload, pck->payload_len);
		currentPacketSize = (pck->payload_len + RTP_HEADER_SIZE);
		rtp->session->dataLengthInBurst+= currentPacketSize; 
		if (rtp->session->streamer->log_level == LOG_PACKET) fprintf(stdout, "  RTP SN %u - TS %u - M %u - Size %u\n", pck->header.SequenceNumber, pck->header.TimeStamp, pck->header.Marker, currentPacketSize);

		pck = pck->next;
		free(tmp->payload);
		free(tmp);
	}
	rtp->pck_queue = NULL;
}


/*
 * callback functions, called by the RTP packetiser 
 */

/*
 * The RTP packetizer is starting a new RTP packet and is giving the header
 */
static void burst_on_pck_new(void *cbk, GF_RTPHeader *header)
{
	RTP_Stream *rtp = cbk;
	if (!header) return;
	memcpy(&rtp->packet.header, header, sizeof(GF_RTPHeader));
} /* OnNewPacket */

/*
 * The RTP packetiser is done with the current RTP packet
 * the header may have changed since the beginning of the packet (OnNewPacket)
 * 
 */
static void burst_on_pck_done(void *cbk, GF_RTPHeader *header) 
{
	GF_Err e;
	s64 burst_time, rtp_ts;
	RTP_Stream *rtp = cbk;
	u32 currentPacketSize; // in bits


	currentPacketSize = (rtp->packet.payload_len + RTP_HEADER_SIZE);
	burst_time = (s64) rtp->packetizer->sl_config.timestampResolution * (rtp->session->nextBurstTime + rtp->session->streamer->cycleDuration);
	rtp_ts = (s64) rtp->next_ts*1000;

	if (rtp->session->dataLengthInBurst + currentPacketSize < rtp->session->streamer->burstSize 
		&& (burst_time - rtp_ts > 0) ) { 
		
		e = gf_rtp_send_packet(rtp->channel, header, 0, 0, rtp->packet.payload, rtp->packet.payload_len);
		if (e) 
			fprintf(stdout, "Error %s sending RTP packet\n", gf_error_to_string(e));
		rtp->session->dataLengthInBurst += currentPacketSize; 
		free(rtp->packet.payload);				
		rtp->packet.payload = NULL;
		rtp->packet.payload_len = 0;
		
		if (rtp->session->streamer->log_level == LOG_PACKET) fprintf(stdout, "  RTP SN %u - TS %u - M %u - Size %u\n", rtp->packet.header.SequenceNumber, rtp->packet.header.TimeStamp, rtp->packet.header.Marker, currentPacketSize);

	} else {
		RTP_Packet *pck;
		if (rtp->session->dataLengthInBurst + currentPacketSize > rtp->session->streamer->burstSize) {
			if (rtp->session->streamer->log_level >= LOG_BURST) 
				fprintf(stdout, "  Packet (TS %u) delayed due to buffer overflow\n", rtp->next_ts);
		} else {
			if (rtp->session->streamer->log_level==LOG_PACKET)
				fprintf(stdout, "  Packet (TS %u) delayed to avoid drift\n", rtp->next_ts);
		}

		GF_SAFEALLOC(pck, RTP_Packet);
		memcpy(&pck->header, header, sizeof(GF_RTPHeader));
		pck->payload = rtp->packet.payload;
		pck->payload_len = rtp->packet.payload_len;
		rtp->packet.payload = NULL;
		rtp->packet.payload_len = 0;		
		rtp->process_burst = 0;
		if (rtp->pck_queue) {
			RTP_Packet *first = rtp->pck_queue;
			while (first->next) first = first->next;
			first->next = pck;
		} else {
			rtp->pck_queue = pck;
		}
	}

} /* OnPacketdone */

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
	if (e) 
		fprintf(stdout, "Error %s sending RTP packet\n", gf_error_to_string(e));
	free(rtp->packet.payload);				

	if (rtp->session->streamer->log_level == LOG_PACKET) 
		fprintf(stdout, "  RTP SN %u - TS %u - M %u - Size %u\n", rtp->packet.header.SequenceNumber, rtp->packet.header.TimeStamp, rtp->packet.header.Marker, rtp->packet.payload_len + RTP_HEADER_SIZE);

	rtp->packet.payload = NULL;
	rtp->packet.payload_len = 0;
}


GF_Err rtp_init_packetizer(RTP_Stream *rtp, char *dest_ip)
{	
	u32 flags = 0;

	if (rtp->session->force_mpeg4_generic) flags = GP_RTP_PCK_SIGNAL_RAP | GP_RTP_PCK_FORCE_MPEG4;

	if (rtp->session->streamer->burst_mode) {
		rtp->packetizer = gf_rtp_packetizer_create_and_init_from_file(rtp->session->mp4File, rtp->track, rtp,
												burst_on_pck_new, burst_on_pck_done, NULL, on_pck_data,
												rtp->session->streamer->path_mtu, 0, 0, flags, 
												rtp->session->streamer->payt, 0, 0, 0);
	} else {
		rtp->packetizer = gf_rtp_packetizer_create_and_init_from_file(rtp->session->mp4File, rtp->track, rtp,
												on_pck_new, on_pck_done, NULL, on_pck_data,
												rtp->session->streamer->path_mtu, 0, 0, flags, 
												rtp->session->streamer->payt, 0, 0, 0);
	}

	rtp->session->streamer->payt++;

	rtp->ts_scale = rtp->packetizer->sl_config.timestampResolution;
	rtp->ts_scale /= gf_isom_get_media_timescale(rtp->session->mp4File, rtp->track);

	rtp->microsec_ts_scale = 1000000;
	rtp->microsec_ts_scale /= gf_isom_get_media_timescale(rtp->session->mp4File, rtp->track);
	return GF_OK;
}

GF_Err rtp_setup_sdp(RTP_Session *session, char *dest_ip)
{	
	RTP_Stream *rtp;
	FILE *sdp_out;
	char filename[30];
	char mediaName[30], payloadName[30];
	char sdpLine[20000];

	sprintf(filename, "session%d.sdp", session->id);
	sdp_out = fopen(filename, "wt");
	if (!sdp_out) return GF_IO_ERR;

	sprintf(sdpLine, "v=0");
	fprintf(sdp_out, "%s\n", sdpLine);
	sprintf(sdpLine, "o=MP4Streamer 3357474383 1148485440000 IN IP%d %s", gf_net_is_ipv6(dest_ip) ? 6 : 4, dest_ip);
	fprintf(sdp_out, "%s\n", sdpLine);
	sprintf(sdpLine, "s=livesession");
	fprintf(sdp_out, "%s\n", sdpLine);
	sprintf(sdpLine, "i=This is an MP4 time-sliced Streaming demo");
	fprintf(sdp_out, "%s\n", sdpLine);
	sprintf(sdpLine, "u=http://gpac.sourceforge.net");
	fprintf(sdp_out, "%s\n", sdpLine);
	sprintf(sdpLine, "e=admin@");
	fprintf(sdp_out, "%s\n", sdpLine);
	sprintf(sdpLine, "c=IN IP%d %s", gf_net_is_ipv6(dest_ip) ? 6 : 4, dest_ip);
	fprintf(sdp_out, "%s\n", sdpLine);
	sprintf(sdpLine, "t=0 0");
	fprintf(sdp_out, "%s\n", sdpLine);
	sprintf(sdpLine, "a=x-copyright: Streamed with GPAC (C)2000-200X - http://gpac.sourceforge.net\n");
	fprintf(sdp_out, "%s\n", sdpLine);

	rtp = session->stream;
	while (rtp) {

		gf_rtp_builder_get_payload_name(rtp->packetizer, payloadName, mediaName);
		
		sprintf(sdpLine, "m=%s %d RTP/%s %d", mediaName, rtp->port, rtp->packetizer->slMap.IV_length ? "SAVP" : "AVP", rtp->packetizer->PayloadType);
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "a=rtpmap:%d %s/%d", rtp->packetizer->PayloadType, payloadName, rtp->packetizer->sl_config.timestampResolution);
		fprintf(sdp_out, "%s\n", sdpLine);
		
		if (gf_isom_get_media_type(rtp->session->mp4File, rtp->track) == GF_ISOM_MEDIA_VISUAL) {
			u32 w, h;
			w = h = 0;
			gf_isom_get_visual_info(rtp->session->mp4File, rtp->track, 1, &w, &h);
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
			gf_hinter_format_ttxt_sdp(rtp->packetizer, payloadName, sdpLine, rtp->session->mp4File, rtp->track);
			fprintf(sdp_out, "%s\n", sdpLine);
		}
		/*EVRC/SMV in non header-free mode*/
		else if ((rtp->packetizer->rtp_payt == GF_RTP_PAYT_EVRC_SMV) && (rtp->packetizer->auh_size>1)) {
			sprintf(sdpLine, "a=fmtp:%d maxptime=%d", rtp->packetizer->PayloadType, rtp->packetizer->auh_size*20);
			fprintf(sdp_out, "%s\n", sdpLine);
		}
		/*H264/AVC*/
		else if (rtp->packetizer->rtp_payt == GF_RTP_PAYT_H264_AVC) {
			GF_AVCConfig *avcc = gf_isom_avc_config_get(rtp->session->mp4File, rtp->track, 1);
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
			dcd = gf_isom_get_decoder_config(rtp->session->mp4File, rtp->track, 1);

			if (dcd && dcd->decoderSpecificInfo && dcd->decoderSpecificInfo->data) {
				gf_rtp_builder_format_sdp(rtp->packetizer, payloadName, sdpLine, dcd->decoderSpecificInfo->data, dcd->decoderSpecificInfo->dataLength);
			} else {
				gf_rtp_builder_format_sdp(rtp->packetizer, payloadName, sdpLine, NULL, 0);
			}
			if (dcd) gf_odf_desc_del((GF_Descriptor *)dcd);

			if (rtp->packetizer->slMap.IV_length) {
				const char *kms;
				gf_isom_get_ismacryp_info(rtp->session->mp4File, rtp->track, 1, NULL, NULL, NULL, NULL, &kms, NULL, NULL, NULL);
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
			dcd = gf_isom_get_decoder_config(rtp->session->mp4File, rtp->track, 1); 
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

	return GF_OK;
} /* rtp_init_packetizer */

GF_Err rtp_init_channel(RTP_Stream *rtp, u32 path_mtu, char * dest, int port)
{
	GF_RTSPTransport tr;
	GF_Err res;

	rtp->channel = gf_rtp_new();
	gf_rtp_set_ports(rtp->channel, 0);

	tr.IsUnicast = gf_sk_is_multicast_address(dest) ? 0 : 1;
	tr.Profile="RTP/AVP";
	tr.destination = dest;
	tr.source = "0.0.0.0";
	tr.IsRecord = 0;
	tr.Append = 0;
	tr.SSRC = rand();

	tr.port_first        = port;
	tr.port_last         = port+1;
	if (tr.IsUnicast) {
		tr.client_port_first = port;
		tr.client_port_last  = port+1;
	} else {
		tr.source = dest;
	}

	res = gf_rtp_setup_transport(rtp->channel, &tr, dest);
	if (res !=0) {
		fprintf(stdout, "Cannot setup RTP transport info\n");
		return res;
	}

	res = gf_rtp_initialize(rtp->channel, 0, 1, 1500, 0, 0, NULL);
	if (res !=0) {
		fprintf(stdout, "Cannot initialize RTP sockets\n");
		return res;
	}

	return GF_OK;
} /* rtp_init_channel */

// ---------------------------------------------------------------------------------------------------

/* 
 * paquetization of a burst
 *	process the AUs till the burst size is reached.
 *
 */

void burst_process_session(RTP_Session *session)
{
	RTP_Stream *rtp, *to_send;
	Bool first = 1;
	u32 time;
		
	time = gf_sys_clock();
	if (!session->timelineOrigin) session->timelineOrigin = time;


	rtp = session->stream;
	while (rtp) {
		rtp->process_burst = 1;
		rtp = rtp->next;
	}

	rtp = NULL;
	while (1) {
		u64 min_ts = (u64) -1;
		to_send = NULL;

		/*for each stream, locate next time*/
		rtp = session->stream;
		while (rtp) {
			/*channel is no longer active in current burst*/
			if (!rtp->process_burst) {
				rtp = rtp->next;
				continue;
			}
			/*flush prev stream if needed*/
			rtp_flush_channel(rtp);

			/*load next AU*/
			if (!rtp->au) {
				if (rtp->current_au >= rtp->nb_aus) {
					if (!session->looping) {
						rtp->process_burst = 0;
						rtp = rtp->next;
						continue;
					}
					rtp->process_burst = 1;
					rtp->ts_offset = rtp->next_ts;
					rtp->microsec_ts_offset = (u32) (rtp->next_ts*(1000000.0/rtp->packetizer->sl_config.timestampResolution)) + session->timelineOrigin;
					rtp->current_au = 0;
				}
				if (rtp->current_au + 1==rtp->nb_aus) {
					rtp->current_au = rtp->current_au;
				}

				rtp->au = gf_isom_get_sample(session->mp4File, rtp->track, rtp->current_au + 1, &rtp->sample_desc_index);
				rtp->current_au ++;
				if (rtp->au) {
					u64 ts;
					rtp->sample_duration = gf_isom_get_sample_duration(session->mp4File, rtp->track, rtp->current_au);
					rtp->sample_duration = (u32)(rtp->sample_duration*rtp->ts_scale);
					
					rtp->microsec_dts = (u64) (rtp->microsec_ts_scale * (s64) (rtp->au->DTS)) + rtp->microsec_ts_offset + session->timelineOrigin;
					ts = (u64) (rtp->ts_scale * (s64) (rtp->au->DTS));
	
					rtp->packetizer->sl_header.decodingTimeStamp = ts + rtp->ts_offset;
											
					ts = (u64) (rtp->ts_scale * (s64) (rtp->au->DTS+rtp->au->CTS_Offset));
					rtp->packetizer->sl_header.compositionTimeStamp = ts + rtp->ts_offset;
						
					rtp->packetizer->sl_header.randomAccessPointFlag = rtp->au->IsRAP;
				}
			}
			if (rtp->au) {
				if (min_ts > rtp->microsec_dts) {
					min_ts = rtp->microsec_dts;
					to_send = rtp;
				}
			}
			rtp = rtp->next;
		}

		/*burst is full or no packet to write due to timing*/
		if (!to_send) break;

		rtp = to_send;
		/*compute drift*/
		if (first) {
			first = 0;
			session->drift = (s32) ((s64)(time - session->timelineOrigin) - ((s64) rtp->next_ts*1000/rtp->packetizer->sl_config.timestampResolution));
			if (session->streamer->log_level >= LOG_BURST) fprintf(stdout, "Time %u - Burst %u - Session %u (Time %u) - TS %d - Drift %d ms\n", time, session->streamer->nbBurstSent, session->id, time - session->timelineOrigin, rtp->next_ts, session->drift);
			else {
 				fprintf(stdout, "Time %u - Burst %u - Session %u (Time %u) - TS %d - Drift %d ms\r", time, session->streamer->nbBurstSent, session->id, time - session->timelineOrigin, rtp->next_ts, session->drift);
				fflush(stdout);
			}

		}

		if (session->streamer->log_level >= LOG_AU) fprintf(stdout, "Sess %d - stream %d - Processing AU %d - DTS "LLD" - CTS "LLD"\n", session->id, rtp->track, rtp->current_au, rtp->packetizer->sl_header.decodingTimeStamp, rtp->packetizer->sl_header.compositionTimeStamp);

		/*unpack nal units*/
		if (rtp->avc_nalu_size) {
			u32 v, size;
			u32 remain = rtp->au->dataLength;
			char *ptr = rtp->au->data;

			rtp->packetizer->sl_header.accessUnitStartFlag = 1;
			rtp->packetizer->sl_header.accessUnitEndFlag = 0;
			while (remain) {
				size = 0;
				v = rtp->avc_nalu_size;
				while (v) {
					size |= (u8) *ptr;
					ptr++;
					remain--;
					v-=1;
					if (v) size<<=8;
				}
				remain -= size;
				rtp->packetizer->sl_header.accessUnitEndFlag = remain ? 0 : 1;
				gf_rtp_builder_process(rtp->packetizer, ptr, size, (u8) !remain, rtp->au->dataLength, rtp->sample_duration, (u8) rtp->sample_desc_index );
				ptr += size;
				rtp->packetizer->sl_header.accessUnitStartFlag = 0;
			}
		} else {
			gf_rtp_builder_process(rtp->packetizer, rtp->au->data, rtp->au->dataLength, (u8) 1, rtp->au->dataLength, rtp->sample_duration, (u8) rtp->sample_desc_index);
		}
			
		rtp->next_ts = (u32)(rtp->packetizer->sl_header.decodingTimeStamp + rtp->sample_duration);
		/*OK delete sample*/
		gf_isom_sample_del(&rtp->au);
	}

	if (session->streamer->log_level >= LOG_BURST)
		fprintf(stdout, "  Actual Burst Size %d bytes - Actual Bit Rate %d kbps\n", session->dataLengthInBurst, 8*session->dataLengthInBurst/(session->streamer->burstDuration+session->streamer->offDuration));
	session->nbBurstSent++;
	session->streamer->nbBurstSent++;
	
}


void process_sessions(Streamer *streamer)
{
	RTP_Session *session;
	RTP_Stream *rtp, *to_send;
	u32 time;
	s32 diff;
	u64 min_ts;
		
	time = gf_sys_clock();
	/*browse all sessions and locate most mature stream*/
	to_send = NULL;
	min_ts = (u64) -1;
	session = streamer->session;
	while (session) {
		/*init session timeline - all sessions are sync'ed for packet scheduling purposes*/
		if (!session->timelineOrigin) session->timelineOrigin = time*1000;
		rtp = session->stream;
		while (rtp) {
			/*load next AU*/
			if (!rtp->au) {
				if (rtp->current_au >= rtp->nb_aus) {
					Double scale;
					if (!rtp->session->looping) {
						rtp = rtp->next;
						continue;
					}
					/*increment ts offset*/
					scale = rtp->packetizer->sl_config.timestampResolution/1000.0;
					rtp->ts_offset += (u32) (session->duration * scale);
					rtp->microsec_ts_offset = (u32) (rtp->ts_offset*(1000000.0/rtp->packetizer->sl_config.timestampResolution)) + session->timelineOrigin;
					rtp->current_au = 0;
				}
				if (rtp->current_au + 1==rtp->nb_aus) {
					rtp->current_au = rtp->current_au;
				}

				rtp->au = gf_isom_get_sample(rtp->session->mp4File, rtp->track, rtp->current_au + 1, &rtp->sample_desc_index);
				rtp->current_au ++;
				if (rtp->au) {
					u64 ts;
					rtp->sample_duration = gf_isom_get_sample_duration(rtp->session->mp4File, rtp->track, rtp->current_au);
					rtp->sample_duration = (u32)(rtp->sample_duration*rtp->ts_scale);

					rtp->microsec_dts = (u64) (rtp->microsec_ts_scale * (s64) (rtp->au->DTS)) + rtp->microsec_ts_offset + session->timelineOrigin;

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
		session = session->next;
	}

	/*no input data ...*/
	if( !to_send) return;
	min_ts /= 1000;

	/*sleep until TS is mature*/
	while (1) {
		diff = (u32) (min_ts) - gf_sys_clock();
		if (diff > 2) {
			//fprintf(stdout, "RTP session %d stream %d - sleeping %d ms\n", to_send->session->id, to_send->track, diff);
			gf_sleep(1);
		} else {
			if (diff<0) fprintf(stdout, "WARNING: RTP session %d stream %d - sending packet %d ms too late\n", to_send->session->id, to_send->track, -diff);
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
			gf_rtp_builder_process(to_send->packetizer, ptr, size, (u8) !remain, to_send->au->dataLength, to_send->sample_duration, (u8) to_send->sample_desc_index );
			ptr += size;
			to_send->packetizer->sl_header.accessUnitStartFlag = 0;
		}
	} else {
		gf_rtp_builder_process(to_send->packetizer, to_send->au->data, to_send->au->dataLength, (u8) 1, to_send->au->dataLength, to_send->sample_duration, (u8) to_send->sample_desc_index);
	}
	/*delete sample*/
	gf_isom_sample_del(&to_send->au);

}



// ---------------------------------------------------------------------------------------------------


u16 check_next_port(Streamer *streamer, u16 first_port)
{
	RTP_Session *session = streamer->session;
	while (session) {
		RTP_Stream *rtp = session->stream;
		while (rtp) {
			if (rtp->port==first_port) {
				return check_next_port(streamer, (u16) (first_port+2) );
			}
			rtp = rtp->next;
		}
		session = session->next;
	}
	return first_port;
}

/*
 * configuration:
 *	retrieves the parameters from the configuration file
 *
 */

GF_Err configuration(Streamer *streamer, char *cfg_file, char *src_file, char *ip_dest, u16 port, Bool loop, Bool force_mpeg4)
{
	GF_Err e = GF_OK;
	RTP_Session *session, *last_sess;
	u32 nb_sessions;
	const char *opt = NULL;
	const char *dest_ip;
	GF_Config *configFile = NULL;	
	u32 i, j;					

	if (cfg_file) {
		fprintf(stdout, "Configuration file: %s \n", cfg_file);

		configFile = gf_cfg_new(PATHFILE, cfg_file);	
		if (!configFile) {
			fprintf(stderr, "ERROR: could not open the file\n");
			return GF_IO_ERR;
		}

		opt = gf_cfg_get_key(configFile, "GLOBAL", "nbSession");
		nb_sessions = opt ? atoi(opt) : 0;
		fprintf(stdout, " Number of sessions: %u \n", nb_sessions);

		opt = gf_cfg_get_key(configFile, "GLOBAL", "IP_dest");
		dest_ip = opt ? opt : "127.0.0.1";
		fprintf(stdout, " Destination IP: %s \n", dest_ip);	


		opt = gf_cfg_get_key(configFile, "GLOBAL", "path_mtu");
		if (opt) streamer->path_mtu = atoi(opt);
		if (!streamer->path_mtu) streamer->path_mtu = 1450;
		fprintf(stdout, " Path MTU: %u bytes \n", streamer->path_mtu);

		/*configure burst mode*/
		opt = gf_cfg_get_key(configFile, "GLOBAL", "burst_mode");
		streamer->burst_mode = (opt && !strcmp(opt, "yes")) ? 1 : 0;
		if (streamer->burst_mode) {
			opt = gf_cfg_get_key(configFile, "GLOBAL", "off_duration");
			streamer->offDuration = opt ? atoi(opt) : 0;
			fprintf(stdout, " Offtime duration: %u ms \n", streamer->offDuration);

			opt = gf_cfg_get_key(configFile, "GLOBAL", "burst_duration");
			streamer->burstDuration = opt ? atoi(opt) : 1000;
			fprintf(stdout, " Burst duration: %u ms \n", streamer->burstDuration);

			opt = gf_cfg_get_key(configFile, "GLOBAL", "burst_bitrate");
			streamer->burstBitRate = opt ? atoi(opt) : 500;
			fprintf(stdout, " Burst bit rate: %u kbps \n", streamer->burstBitRate);

			streamer->burstSize = streamer->burstBitRate*1024*streamer->burstDuration/1000/8;
			streamer->averageBitRate = (streamer->burstDuration * streamer->burstBitRate)/(streamer->offDuration + streamer->burstDuration); 

			fprintf(stdout, " Burst mode enabled - burst size: %d bytes - average bit rate %d kbps\n", streamer->burstSize, streamer->averageBitRate);
		}
	} else {
		fprintf(stdout, " Path MTU: %u bytes \n", streamer->path_mtu);
		dest_ip = ip_dest;
		fprintf(stdout, " Destination IP: %s Port %d\n", dest_ip, port);	
		nb_sessions = 1;
	}

	streamer->payt = BASE_PAYT;

	last_sess = NULL;
	for(i=0; i<nb_sessions; i++)	{
		GF_ISOFile *file;
		RTP_Stream *rtp, *prev_stream;
		u16 first_port;
		u32 nb_tracks;
		u32 sess_data_size;
		u16 sess_path_mtu;
		char *sess_dest_ip;
		char sessionName[1024];

		if (cfg_file) {
			sprintf(sessionName, "SESSION%d",i+1);
			opt = gf_cfg_get_key(configFile, sessionName, "file");
			/*session doesn't exist*/
			if (!opt) {
				fprintf(stderr, "No configuration info for session %d - skipping\n", i+1);
				continue;
			}
			file = gf_isom_open(opt, GF_ISOM_OPEN_READ, NULL);
			if (!file) {
				fprintf(stderr, "Error opening file %s of session %u: %s - skipping\n", opt, i+1, gf_error_to_string(gf_isom_last_error(NULL)));
				continue;
			}
		} else {
			file = gf_isom_open(src_file, GF_ISOM_OPEN_READ, NULL);
			if (!file) {
				fprintf(stderr, "Error opening file %s of session %u: %s - skipping\n", opt, i+1, gf_error_to_string(gf_isom_last_error(NULL)));
				return GF_IO_ERR;
			}
		}

		GF_SAFEALLOC(session, RTP_Session);
		if (last_sess) {
			last_sess->next = session;
		} else {
			streamer->session = session;
		}
		last_sess = session;

		session->streamer = streamer;		
		session->id = i+1;	
		session->mp4File = file;

		if (cfg_file) {
			opt = gf_cfg_get_key(configFile, sessionName, "port");
			first_port = opt ? atoi(opt) : 7000+4*i;

			opt = gf_cfg_get_key(configFile, sessionName, "looping");
			session->looping = opt ? atoi(opt) : 1;

			opt = gf_cfg_get_key(configFile, sessionName, "force_mpeg4-generic");
			session->force_mpeg4_generic = opt ? atoi(opt) : 0;

			opt = gf_cfg_get_key(configFile, sessionName, "path_mtu");
			sess_path_mtu = opt ? atoi(opt) : 0;
			if (!opt) sess_path_mtu = streamer->path_mtu;

			opt = gf_cfg_get_key(configFile, sessionName, "IP_dest");
			sess_dest_ip = (char *) (opt ? opt : dest_ip);
		} else {
			sess_path_mtu = streamer->path_mtu;
			sess_dest_ip = (char *) dest_ip;
			session->looping = loop;
			session->force_mpeg4_generic = force_mpeg4;
			first_port = port;
		}

		sess_data_size = 0;
		prev_stream = NULL;
		nb_tracks = gf_isom_get_track_count(session->mp4File);
		for (j=0;j<nb_tracks;j++) {
			u32 mediaTimescale; 
			u32 mediaSize;
			u32 mediaDuration;

			switch (gf_isom_get_media_type(session->mp4File, j+1)) {
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
			else session->stream = rtp;
			prev_stream = rtp;

			rtp->session = session;
			rtp->track = j+1;

			rtp->nb_aus = gf_isom_get_sample_count(session->mp4File, rtp->track);
			mediaTimescale = gf_isom_get_media_timescale(session->mp4File, rtp->track);
			mediaDuration = (u32)(gf_isom_get_media_duration(session->mp4File, rtp->track)*1000/mediaTimescale); // ms
			mediaSize = (u32)gf_isom_get_media_data_size(session->mp4File, rtp->track);

			sess_data_size += mediaSize;
			if (mediaDuration > session->duration) session->duration = mediaDuration;

			rtp->port = check_next_port(streamer, first_port);
			first_port = rtp->port+2;

			e = rtp_init_channel(rtp, sess_path_mtu+12, sess_dest_ip, rtp->port);
			if (e) {
				fprintf(stderr, "Could not initialize RTP Channel: %s\n", gf_error_to_string(e));
				goto exit;
			}			
			e = rtp_init_packetizer(rtp, sess_dest_ip);
			if (e) {
				fprintf(stderr, "Could not initialize Packetizer: %s\n", gf_error_to_string(e));
				goto exit;
			}
		}
		rtp_setup_sdp(session, sess_dest_ip);

		if (streamer->burst_mode) {
			session->minBurstSize = (sess_data_size * (streamer->burstDuration + streamer->offDuration)) / session->duration; // bps * ms ==> bits
			fprintf(stdout, "Session %u: %d ms - avg bitrate %d kbps - Min Burst Size %d bytes\n", session->id, session->duration, sess_data_size/session->duration, session->minBurstSize);
		}
	}

	/*configure burst*/
	if (streamer->burst_mode) {
		streamer->cycleDuration = 0;
		session = streamer->session;
		while (session) {
			session->nextBurstTime = streamer->cycleDuration;
			streamer->cycleDuration += streamer->burstDuration;
			session = session->next;
		}
		streamer->cycleDuration += streamer->offDuration;
	}

	fprintf(stdout, "\n");

exit:
	if (cfg_file) gf_cfg_del(configFile);
	return e;
} /* configuration */

// ---------------------------------------------------------------------------------------------------


void usage() 
{
	fprintf(stdout, "Usage: MP4Streamer [options] file\n"
					"with options being one fo the following:\n"
					"-cfg:      file is a configuration file for all sessions\n"
					"\n"
					"Options used for simple streaming:\n"
					"-port=NUM:    port to use. Default is 7000\n"
					"-mtu=NUM:     network path MTU to use. Default is 1450\n"
					"-dst=ADD:     destination IP address. Default is 127.0.0.1\n"
					"-mpeg4:       forces usage of mpeg4-generic payload format. Default is off\n"
					"-noloop:      disables session looping. Default is off\n"
		);
}

Bool check_exit()
{
	if (gf_prompt_has_input()) {
		char c;
		c = (char) gf_prompt_get_char();
		if (c=='q') return 1;
	}
	return 0;
}

int main(int argc, char **argv)
{
	Streamer streamer;		/* Streamer metadata (from cfg file) */
	char *cfg = NULL;
	RTP_Session *session;
	u32 i;
	s32 sleepDuration;
	u32 time;
	char *ip_dest = "127.0.0.1";
	u16 port = 7000;
	Bool loop = 1;
	Bool force_mpeg4 = 0;
	char *file_name = NULL;


	if (argc < 2) {
		usage(argv[0]);
		return 0;
	}

	gf_log_set_tools(0xFFFFFFFF);
	gf_log_set_level(GF_LOG_ERROR);

	memset(&streamer, 0, sizeof(Streamer));
	streamer.log_level = LOG_BURST;
	streamer.path_mtu = 1450;

	for (i=1; i<(u32) argc; i++) {
		char *arg = argv[i];
		if (arg[0]=='-') {
			if (!stricmp(arg, "-q") || !stricmp(arg, "--quiet")) streamer.log_level = LOG_NONE;
			else if (!strnicmp(arg, "-v=", 3)) {
				if (!stricmp(arg+3, "au")) streamer.log_level = LOG_AU;
				else if (!stricmp(arg+3, "rtp")) streamer.log_level = LOG_PACKET;
			}
			else if (!strnicmp(arg, "-cfg=", 5)) cfg = arg+5;
			else if (!strnicmp(arg, "-port=", 6)) port = atoi(arg+6);
			else if (!strnicmp(arg, "-mtu=", 5)) streamer.path_mtu = atoi(arg+5);
			else if (!strnicmp(arg, "-dst=", 5)) ip_dest = arg+5;
			else if (!stricmp(arg, "-noloop")) loop = 0;
			else if (!stricmp(arg, "-mpeg4")) force_mpeg4 = 1;
		} else {
			file_name = arg;
		}
	}

	if (!cfg && !file_name) {
		usage(argv[0]);
		return 0;
	}

	fprintf(stdout, "Time-sliced MP4 Streamer - (c) ENST 2006-200X\n");
	fprintf(stdout, "Developed within the Multimedia group\n");
	fprintf(stdout, "with the help of the following students: \n");
	fprintf(stdout, "   MIX 2006: Anh-Vu BUI and Xiangfeng LIU \n");
	fprintf(stdout, "\n");

	if (!cfg) fprintf(stdout, "Configuring streaming of file %s to %s:%d - loop %s\n", file_name, ip_dest, port, loop?"enabled":"disabled");
	/*
	 * Initialization
	 *
	 */
	gf_sys_init();
	if (configuration(&streamer, cfg, file_name, ip_dest, port, loop, force_mpeg4) != GF_OK) {
		goto err_exit;
	}

	if (streamer.burst_mode) {
		session = streamer.session;
		while (1) {
			burst_process_session(session);		
			session->dataLengthInBurst = 0; 
			session->nextBurstTime += streamer.cycleDuration;
			session = session->next;
			if (!session) session = streamer.session;

			time = gf_sys_clock();
			sleepDuration = session->nextBurstTime - (time - session->timelineOrigin);
			if (sleepDuration > 0) {
				gf_sleep(sleepDuration);
			}

			if (check_exit()) break;
		} 
	} else {
		while (1) {
			process_sessions(&streamer);		
			if (check_exit()) break;
		}
	}		

err_exit:
	/*
	 * Desallocation 
	 *
	 */
	session = streamer.session;
	while (session) {
		RTP_Session *_s = session;
		RTP_Stream *rtp = session->stream;
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
		if (session->mp4File) gf_isom_close(session->mp4File);
		session = session->next;
		free(_s);
	}	
	gf_sys_close();
	fprintf(stdout, "done\n");
	return GF_OK;

} /* end main */

