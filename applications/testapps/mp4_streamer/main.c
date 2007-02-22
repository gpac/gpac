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

//------------------------------------------------------------
// Includes
//------------------------------------------------------------
	// Third

	// Project
#include <gpac/setup.h>
#include <gpac/constants.h>
#include <gpac/isomedia.h>
#include <gpac/ietf.h>
#include <gpac/internal/ietf_dev.h>
#include <gpac/config.h>
#include <gpac/mpeg4_odf.h>
#include <gpac/media_tools.h>

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
typedef struct {	
	u32 burstDuration;
	u32 burstBitRate;
	u32 burstSize;
	u32 offDuration;
	u32 averageBitRate;

	u32 cycleDuration;

	u32 nbSessions;
	char * dest_ip;
	struct _rtp_caller *rtps;

	// other param
	u32 nbBurstSent; // toutes sessions confondues
	Bool run;

	u32 log_level;
	u32 path_mtu;
} Simulator;

typedef struct {
	GF_RTPHeader header;
	char *payload;
	u32 payload_len;
} RTP_Packet;

typedef struct _rtp_caller {
	u32 id;
	u32 mediaTimescale; 
	u32 mediaBitRate; 
	u32 mediaSize;
	u32 mediaDuration;
	u32 nbAccessUnits;
	u32 minBurstSize; // max size to fill the burst with the media bitRate
	u32 looping; // 1: play the media in a loop, 0 play once

	Bool processAU;
	u32 accessUnitProcess; // number of the AU to process. set to 0 at the beginnning
	u32 nbBurstSent; // pour cette session
	u32 timelineOrigin; // time when the first burts was sent. this time <=> (CTS=0)
	u32 nextBurstTime;
	u32 dataLengthInBurst; // total length filled in current burst
	s32 drift;

	u32 port;
	GF_RTPChannel *channel; 
	GP_RTPPacketizer *packetizer;

	GF_ISOSample  *accessUnit; // the AU
	char *filename;
	GF_ISOFile *mp4File;
	u32 trackNum;

	/* The previous packet which could not be sent in previous burst*/
	RTP_Packet prev_packet;
	/* The current packet being formed */
	RTP_Packet packet;
	u32 ts_offset;
	u32 next_ts;

	Bool force_mpeg4_generic;

	/* Management */
	Simulator *global;
} RTP_Caller;

//------------------------------------------------------------
// Functions
//------------------------------------------------------------

Bool has_input();
u8 get_a_char();
void set_echo_off(Bool echo_off);

/*
 * callback functions, called by the RTP packetiser 
 */

/*
 * The RTP packetizer is starting a new RTP packet and is giving the header
 */
static void OnNewPacket(void *cbk, GF_RTPHeader *header)
{
	RTP_Caller *rtp = cbk;
	if (!header) return;
	memcpy(&rtp->packet.header, header, sizeof(GF_RTPHeader));
} /* OnNewPacket */

/*
 * The RTP packetiser is done with the current RTP packet
 * the header may have changed since the beginning of the packet (OnNewPacket)
 * 
 */
static void OnPacketDone(void *cbk, GF_RTPHeader *header) 
{
	GF_Err e;
	RTP_Caller *rtp = cbk;
	u32 currentPacketSize; // in bits

	/* Is there a remaining packet to send which did not fit into the previous burst ? */
	if (rtp->prev_packet.payload) {
		
		gf_rtp_send_packet(rtp->channel, &rtp->prev_packet.header, 0, 0, rtp->prev_packet.payload, rtp->prev_packet.payload_len);
		currentPacketSize = (rtp->prev_packet.payload_len + RTP_HEADER_SIZE) * 8; // in bits
		rtp->dataLengthInBurst+= currentPacketSize; 
		free(rtp->prev_packet.payload);				
		rtp->prev_packet.payload = NULL;

		if (rtp->global->log_level == LOG_PACKET) fprintf(stdout, "  RTP SN %u - TS %u - M %u - Size %u\n", rtp->prev_packet.header.SequenceNumber, rtp->prev_packet.header.TimeStamp, rtp->prev_packet.header.Marker, currentPacketSize);
	}

	currentPacketSize = (rtp->packet.payload_len + RTP_HEADER_SIZE) * 8; // in bits
	if (rtp->dataLengthInBurst + currentPacketSize < rtp->global->burstSize 
		&& (rtp->nextBurstTime + rtp->global->cycleDuration)*rtp->packetizer->sl_config.timestampResolution > rtp->next_ts*1000) { 
		
		if (rtp->nbBurstSent == 0 && rtp->dataLengthInBurst == 0) rtp->timelineOrigin = gf_sys_clock();

		e = gf_rtp_send_packet(rtp->channel, header, 0, 0, rtp->packet.payload, rtp->packet.payload_len);
		if (e) fprintf(stdout, "Error %s sending RTP packet\n", gf_error_to_string(e));
		rtp->dataLengthInBurst += currentPacketSize; 
		free(rtp->packet.payload);				
		rtp->packet.payload = NULL;
		rtp->packet.payload_len = 0;
		
		if (rtp->global->log_level == LOG_PACKET) fprintf(stdout, "  RTP SN %u - TS %u - M %u - Size %u\n", rtp->packet.header.SequenceNumber, rtp->packet.header.TimeStamp, rtp->packet.header.Marker, currentPacketSize);		

	} else {
		if (rtp->dataLengthInBurst + currentPacketSize > rtp->global->burstSize) {
			if (rtp->global->log_level >= LOG_BURST) fprintf(stdout, "  Packet (%u ms) delayed due to buffer overflow\n", rtp->next_ts*1000/rtp->packetizer->sl_config.timestampResolution);
		} else {
			if (rtp->global->log_level==LOG_PACKET)fprintf(stdout, "  Packet (%u ms) delayed to avoid drift\n", rtp->next_ts*1000/rtp->packetizer->sl_config.timestampResolution);
		}

		memcpy(&rtp->prev_packet.header, header, sizeof(GF_RTPHeader));
		rtp->prev_packet.payload = rtp->packet.payload;
		rtp->prev_packet.payload_len = rtp->packet.payload_len;
		rtp->packet.payload = NULL;
		rtp->packet.payload_len = 0;		
		rtp->processAU = 0;
	}

} /* OnPacketdone */

/*
 * The RTP packetiser has added data to the current RTP packet
 */
static void OnData(void *cbk, char *data, u32 data_size, Bool is_head) 
{
	RTP_Caller *rtp = cbk;
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

GF_Err rtp_init_packetizer(RTP_Caller *rtp)
{	
	u32 flags = 0;
	FILE *sdp_out;
	char filename[30];
	char mediaName[30], payloadName[30];
	char sdpLine[20000];

	if (rtp->force_mpeg4_generic) flags = GP_RTP_PCK_SIGNAL_RAP | GP_RTP_PCK_FORCE_MPEG4;
	rtp->packetizer = gf_rtp_packetizer_create_and_init_from_file(rtp->mp4File, rtp->trackNum, rtp,
											OnNewPacket, OnPacketDone, NULL, OnData,
											rtp->global->path_mtu, 0, 0, flags, 
											BASE_PAYT + rtp->id - 1,
											0, 0, 0);

	gf_rtp_builder_get_payload_name(rtp->packetizer, payloadName, mediaName);

	sprintf(filename, "session%d.sdp", rtp->id);
	sdp_out = fopen(filename, "wt");
	if (sdp_out) {
		sprintf(sdpLine, "v=0");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "o=MP4Streamer 3357474383 1148485440000 IN IP4 %s", rtp->global->dest_ip);
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "s=livesession");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "i=This is an MP4 time-sliced Streaming demo");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "u=http://gpac.sourceforge.net");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "e=admin@");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "c=IN IP4 %s", rtp->global->dest_ip);
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "t=0 0");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "a=x-copyright: Streamed with GPAC (C)2000-200X - http://gpac.sourceforge.net\n");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "m=%s %d RTP/%s %d", mediaName, rtp->port, rtp->packetizer->slMap.IV_length ? "SAVP" : "AVP", rtp->packetizer->PayloadType);
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "a=rtpmap:%d %s/%d", rtp->packetizer->PayloadType, payloadName, rtp->packetizer->sl_config.timestampResolution);
		fprintf(sdp_out, "%s\n", sdpLine);
		
		if (gf_isom_get_media_type(rtp->mp4File, rtp->trackNum) == GF_ISOM_MEDIA_VISUAL) {
			u32 w, h;
			w = h = 0;
			gf_isom_get_visual_info(rtp->mp4File, rtp->trackNum, 1, &w, &h);
			if (rtp->packetizer->rtp_payt == GP_RTP_PAYT_H263) {
				sprintf(sdpLine, "a=cliprect:0,0,%d,%d", h, w);
			}
			/*extensions for some mobile phones*/
			sprintf(sdpLine, "a=framesize:%d %d-%d", rtp->packetizer->PayloadType, w, h);
		}
		/*AMR*/
		if ((rtp->packetizer->rtp_payt == GP_RTP_PAYT_AMR) || (rtp->packetizer->rtp_payt == GP_RTP_PAYT_AMR_WB)) {
			sprintf(sdpLine, "a=fmtp:%d octet-align", rtp->packetizer->PayloadType);
			fprintf(sdp_out, "%s\n", sdpLine);
		}
		/*Text*/
		else if (rtp->packetizer->rtp_payt == GP_RTP_PAYT_3GPP_TEXT) {
			gf_hinter_format_ttxt_sdp(rtp->packetizer, payloadName, sdpLine, rtp->mp4File, rtp->trackNum);
			fprintf(sdp_out, "%s\n", sdpLine);
		}
		/*EVRC/SMV in non header-free mode*/
		else if ((rtp->packetizer->rtp_payt == GP_RTP_PAYT_EVRC_SMV) && (rtp->packetizer->auh_size>1)) {
			sprintf(sdpLine, "a=fmtp:%d maxptime=%d", rtp->packetizer->PayloadType, rtp->packetizer->auh_size*20);
			fprintf(sdp_out, "%s\n", sdpLine);
		}
		/*H264/AVC*/
		else if (rtp->packetizer->rtp_payt == GP_RTP_PAYT_H264_AVC) {
			GF_AVCConfig *avcc = gf_isom_avc_config_get(rtp->mp4File, rtp->trackNum, 1);
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
			gf_odf_avc_cfg_del(avcc);
		}
		/*MPEG-4 decoder config*/
		else if (rtp->packetizer->rtp_payt==GP_RTP_PAYT_MPEG4) {
			GF_DecoderConfig *dcd;
			dcd = gf_isom_get_decoder_config(rtp->mp4File, rtp->trackNum, 1);

			if (dcd && dcd->decoderSpecificInfo && dcd->decoderSpecificInfo->data) {
				gf_rtp_builder_format_sdp(rtp->packetizer, payloadName, sdpLine, dcd->decoderSpecificInfo->data, dcd->decoderSpecificInfo->dataLength);
			} else {
				gf_rtp_builder_format_sdp(rtp->packetizer, payloadName, sdpLine, NULL, 0);
			}
			if (dcd) gf_odf_desc_del((GF_Descriptor *)dcd);

			if (rtp->packetizer->slMap.IV_length) {
				const char *kms;
				gf_isom_get_ismacryp_info(rtp->mp4File, rtp->trackNum, 1, NULL, NULL, NULL, NULL, &kms, NULL, NULL, NULL);
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
		else if (rtp->packetizer->rtp_payt==GP_RTP_PAYT_LATM) { 
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
			dcd = gf_isom_get_decoder_config(rtp->mp4File, rtp->trackNum, 1); 
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
		fprintf(sdp_out, "\n");

		fclose(sdp_out);

	}
	return GF_OK;
} /* rtp_init_packetizer */

GF_Err rtp_init_channel(RTP_Caller *rtp, u32 path_mtu, char * dest, int port)
{
	GF_RTSPTransport tr;
	GF_Err res;

	rtp->channel = gf_rtp_new();
	gf_rtp_set_ports(rtp->channel);

	tr.IsUnicast = gf_sk_is_multicast_address(dest) ? 0 : 1;
	tr.Profile="RTP/AVP";
	tr.destination = dest;
	tr.source = "0.0.0.0";
	tr.IsRecord = 0;
	tr.Append = 0;
	tr.SSRC = rand();

	if (tr.IsUnicast) {
		tr.client_port_first = port; //RTP
		tr.client_port_last  = port+1; //RTCP (not used a priori)

		tr.port_first        = rtp->channel->net_info.port_first; //RTP other end 
		tr.port_last         = rtp->channel->net_info.port_last; //RTCP other end (not used a priori)
	} else {
		tr.port_first        = port;
		tr.port_last         = port+1;
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

void sendBurst(RTP_Caller *rtp)
{
	u32 sampleDuration;
	u32 sampleDescIndex;
	u64 ts;
	u32 time;

	Double ts_scale;
		
	if (rtp->nbBurstSent == 0) {
		rtp->timelineOrigin = gf_sys_clock();
	}

	time = gf_sys_clock();
	rtp->drift = (s32)(time -rtp->timelineOrigin) - (s32) (rtp->next_ts*1000/rtp->packetizer->sl_config.timestampResolution);

	if (rtp->global->log_level >= LOG_BURST) fprintf(stdout, "Time %u - Burst %u - Session %u - Session Time %u - TS %d - Drift %d ms\n", time, rtp->global->nbBurstSent, rtp->id, time-rtp->timelineOrigin, rtp->next_ts*1000/rtp->packetizer->sl_config.timestampResolution, rtp->drift);
	else fprintf(stdout, "Time %u - Burst %u - Session %u (Time %u) - TS %d - Drift %d ms\r", time, rtp->global->nbBurstSent, rtp->id, time-rtp->timelineOrigin, rtp->next_ts*1000/rtp->packetizer->sl_config.timestampResolution, rtp->drift);

	
	ts_scale = rtp->packetizer->sl_config.timestampResolution;
	ts_scale /= rtp->mediaTimescale;
	
	rtp->processAU = 1;
	while (rtp->processAU) {			
		if (rtp->accessUnitProcess < rtp->nbAccessUnits) {
			// attention overflow acceUnit number <= get_sample_count
			rtp->accessUnit = gf_isom_get_sample(rtp->mp4File, rtp->trackNum, rtp->accessUnitProcess + 1, &sampleDescIndex);
			
			sampleDuration = gf_isom_get_sample_duration(rtp->mp4File, rtp->trackNum, rtp->accessUnitProcess + 1);
			sampleDuration = (u32)(sampleDuration*ts_scale);
					
			ts = (u64) (ts_scale * (s64) (rtp->accessUnit->DTS));
			rtp->packetizer->sl_header.decodingTimeStamp = ts + rtp->ts_offset;
									
			ts = (u64) (ts_scale * (s64) (rtp->accessUnit->DTS+rtp->accessUnit->CTS_Offset));
			rtp->packetizer->sl_header.compositionTimeStamp = ts + rtp->ts_offset;
				
			rtp->packetizer->sl_header.randomAccessPointFlag = rtp->accessUnit->IsRAP;
						
			if (rtp->global->log_level >= LOG_AU) fprintf(stdout, "Processing AU %d - DTS %u - CTS %u\n", rtp->accessUnitProcess, rtp->packetizer->sl_header.decodingTimeStamp, rtp->packetizer->sl_header.compositionTimeStamp);

			gf_rtp_builder_process(rtp->packetizer, rtp->accessUnit->data, rtp->accessUnit->dataLength, 1, 
								   rtp->accessUnit->dataLength, sampleDuration, sampleDescIndex);
			gf_isom_sample_del(&rtp->accessUnit);

			rtp->next_ts = (u32)ts + sampleDuration + rtp->ts_offset;

			rtp->accessUnitProcess ++;
		} else {
			rtp->accessUnitProcess = 0;
			if (rtp->looping) {
				rtp->ts_offset = rtp->next_ts;
			} else {
				rtp->processAU = 0;
			}

		}
	} 
	if (rtp->global->log_level >= LOG_BURST) fprintf(stdout, "  Actual Burst Size %d bits - Actual Bit Rate %d kbps\n", rtp->dataLengthInBurst, rtp->dataLengthInBurst/(rtp->global->burstDuration+rtp->global->offDuration));
	rtp->nbBurstSent++;
	rtp->global->nbBurstSent++;
	
} /* sendBurst */

// ---------------------------------------------------------------------------------------------------

/*
 * configuration:
 *	retrieves the parameters from the configuration file
 *
 */

int configuration(Simulator *av_simulator, char *av_argv1)
{
	GF_Err e;
	const char *opt;
	char config[1024];		
	GF_Config *configFile;	
	u32 i;					

	sprintf(config,"%s",av_argv1); 

	fprintf(stdout, "Configuration file: %s \n", config);

	configFile = gf_cfg_new(PATHFILE,config);	
	if (!configFile) {
		fprintf(stderr, "ERROR: could not open the file\n");
		return GF_IO_ERR;
	}

	opt = gf_cfg_get_key(configFile, "GLOBAL", "nbSession");
	av_simulator->nbSessions = opt ? atoi(opt) : 0;
	fprintf(stdout, " Number of sessions: %u \n", av_simulator->nbSessions);

	opt = gf_cfg_get_key(configFile, "GLOBAL", "IP_dest");
	av_simulator->dest_ip = strdup(opt ? opt : "127.0.0.1");
	fprintf(stdout, " Destination IP: %s \n", av_simulator->dest_ip);	

	opt = gf_cfg_get_key(configFile, "GLOBAL", "off_duration");
	av_simulator->offDuration = opt ? atoi(opt) : 0;
	fprintf(stdout, " Offtime duration: %u ms \n", av_simulator->offDuration);

	opt = gf_cfg_get_key(configFile, "GLOBAL", "burst_duration");
	av_simulator->burstDuration = opt ? atoi(opt) : 1000;
	fprintf(stdout, " Burst duration: %u ms \n", av_simulator->burstDuration);

	opt = gf_cfg_get_key(configFile, "GLOBAL", "burst_bitrate");
	av_simulator->burstBitRate = opt ? atoi(opt) : 500;
	fprintf(stdout, " Burst bit rate: %u kbps \n", av_simulator->burstBitRate);

	opt = gf_cfg_get_key(configFile, "GLOBAL", "path_mtu");
	if (opt) av_simulator->path_mtu = atoi(opt);
	if (!av_simulator->path_mtu) av_simulator->path_mtu = 1450;
	fprintf(stdout, " Path MTU: %u bytes \n", av_simulator->path_mtu);

	av_simulator->burstSize = av_simulator->burstBitRate*av_simulator->burstDuration;
	fprintf(stdout, " Burst size: %u bits \n", av_simulator->burstSize);

	av_simulator->averageBitRate = (av_simulator->burstDuration * av_simulator->burstBitRate)/(av_simulator->offDuration + av_simulator->burstDuration); // kbps
	fprintf(stdout, " Average Bit Rate: %u kbps\n", av_simulator->averageBitRate);

	av_simulator->rtps = (RTP_Caller *)malloc(av_simulator->nbSessions * sizeof(RTP_Caller));

	for(i=0; i<av_simulator->nbSessions; i++)	{
		RTP_Caller *rtp;
		char sessionName[1024];

		sprintf(sessionName, "SESSION%d",i+1);						
		rtp = &av_simulator->rtps[i];
		memset(rtp, 0, sizeof(RTP_Caller));

		rtp->global = av_simulator;		
		rtp->id = i+1;	

		// retrieve parameters in cfg file
		opt = gf_cfg_get_key(configFile, sessionName, "file");
		rtp->filename = opt ? strdup(opt) : NULL;	

		opt = gf_cfg_get_key(configFile, sessionName, "track_nb");
		rtp->trackNum = opt ? atoi(opt) : 0;
		opt = gf_cfg_get_key(configFile, sessionName, "port");
		rtp->port = opt ? atoi(opt) : 7000+2*i;
		opt = gf_cfg_get_key(configFile, sessionName, "looping");
		rtp->looping = opt ? atoi(opt) : 1;

		opt = gf_cfg_get_key(configFile, sessionName, "force_mpeg4-generic");
		rtp->force_mpeg4_generic = opt ? atoi(opt) : 0;

		rtp->mp4File = gf_isom_open(rtp->filename, GF_ISOM_OPEN_READ, NULL);
		if (!rtp->mp4File) {
			fprintf(stderr, "ERROR: Could not open input MP4 file %s of session %u\n", rtp->filename, i);
			return GF_IO_ERR;
		}
		if (rtp->trackNum > gf_isom_get_track_count(rtp->mp4File)) {
			fprintf(stderr, "ERROR: session %u: Track > track count \n", i);
		}				
		rtp->nbAccessUnits = gf_isom_get_sample_count(rtp->mp4File, rtp->trackNum);
		rtp->mediaTimescale = gf_isom_get_media_timescale(rtp->mp4File, rtp->trackNum);
		rtp->mediaDuration = (u32)(gf_isom_get_track_duration(rtp->mp4File, rtp->trackNum)/gf_isom_get_timescale(rtp->mp4File)*1000); // ms
		rtp->mediaSize = (u32)gf_isom_get_media_data_size(rtp->mp4File, rtp->trackNum) * 8; // (gf_isom_get_media_data_size en octets)
	
		rtp->mediaBitRate =  rtp->mediaSize / rtp->mediaDuration; // in bps
		rtp->minBurstSize = rtp->mediaBitRate * (rtp->global->burstDuration+rtp->global->offDuration); // bps * ms ==> bits
	
		e = rtp_init_channel(rtp, av_simulator->path_mtu+12, av_simulator->dest_ip, rtp->port);
		if (e) {
			fprintf(stderr, "Could not initialize RTP Channel\n");
		}			
		e = rtp_init_packetizer(&av_simulator->rtps[i]);
		if (e) {
			fprintf(stderr, "Could not initialize Packetizer \n");
		}

		fprintf(stdout, " Session %u:\n", rtp->id);
		fprintf(stdout, "  File %s - Track %u - Destination Port %u \n", rtp->filename, rtp->trackNum, rtp->port);
		fprintf(stdout, "  Size %u bits - Duration %u ms - Bit Rate %u kbps\n", rtp->mediaSize, rtp->mediaDuration, rtp->mediaBitRate);
		fprintf(stdout, "  Min Burst Size %u bits\n", rtp->minBurstSize);

	}
	fprintf(stdout, "\n");

	gf_cfg_del(configFile);
	return 1;
} /* configuration */

// ---------------------------------------------------------------------------------------------------

GF_Err handleSessions(Simulator *global)
{
	u32 i;
	s32 sleepDuration;
	u32 time;

	global->cycleDuration = 0;
	for (i=0; i<global->nbSessions; i++) {
		global->rtps[i].nextBurstTime = global->cycleDuration;
		global->cycleDuration += global->burstDuration;
	}
	global->cycleDuration += global->offDuration;

	i = 0;
	global->run = 1;
	while (global->run) {
		RTP_Caller *rtp;
		rtp = &global->rtps[i];

		if (rtp->looping || rtp->nbBurstSent == 0 || rtp->accessUnitProcess != 0) {
			sendBurst(rtp);		
		} 
		rtp->dataLengthInBurst = 0; 
		rtp->nextBurstTime += global->cycleDuration;

		if (has_input()) {
			char c;
			c = get_a_char();

			switch (c) {
			case 'q':
				global->run = 0;
				break;

			default:
				break;
			}
		}

		if (i == global->nbSessions-1) {
			i = 0;
		} else {
			i++;
		}
		time = gf_sys_clock();
		sleepDuration = global->rtps[i].nextBurstTime - (time - global->rtps[i].timelineOrigin);
		if (sleepDuration > 0) {
			gf_sleep(sleepDuration);
		}
	}
	return GF_OK;
}

void usage(char *argv0) 
{
	fprintf(stderr, "usage: %s configurationFile \n", argv0);
}

int main(int argc, char **argv)
{
	Simulator global;		/* Simulator metadata (from cfg file) */
	GF_Err e = GF_OK;			/* Error Message */	
	char *cfg = NULL;
	u32 i;						

	if (argc < 2) {
		usage(argv[0]);
		return 0;
	}

	memset(&global, 0, sizeof(Simulator));
	global.log_level = LOG_BURST;
	global.path_mtu = 1450;

	for (i=1; i<argc; i++) {
		if (argv[i][0]=='-') {
			if (!stricmp(argv[i], "-q") || !stricmp(argv[i], "--quiet")) global.log_level = LOG_NONE;
			else if (!strnicmp(argv[i], "-v=", 3)) {
				if (!stricmp(argv[i]+3, "au")) global.log_level = LOG_AU;
				else if (!stricmp(argv[i]+3, "rtp")) global.log_level = LOG_PACKET;
			}
			else if (!strnicmp(argv[i], "--verbose=", 10)) {
				if (!stricmp(argv[i]+10, "au")) global.log_level = LOG_AU;
				else if (!stricmp(argv[i]+10, "rtp")) global.log_level = LOG_PACKET;
			}
		} else {
			cfg = argv[i];
		}
	}

	if (!cfg) {
		usage(argv[0]);
		return 0;
	}

	fprintf(stdout, "Time-sliced MP4 Streamer - (c) ENST 2006-200X\n");
	fprintf(stdout, "Developed within the Multimedia group\n");
	fprintf(stdout, "with the help of the following students: \n");
	fprintf(stdout, "   MIX 2006: Anh-Vu BUI and Xiangfeng LIU \n");
	fprintf(stdout, "\n");

	/*
	 * Initialization
	 *
	 */
	gf_sys_init();
	configuration(&global, cfg);
	  

	handleSessions(&global);

	/*
	 * Desallocation 
	 *
	 */
	
	for(i=0; i<global.nbSessions; i++)
	{
		if (global.rtps[i].channel) gf_rtp_del(global.rtps[i].channel);
		if (global.rtps[i].packetizer) gf_rtp_builder_del(global.rtps[i].packetizer);
		if (global.rtps[i].packet.payload) free(global.rtps[i].packet.payload);	
		if (global.rtps[i].prev_packet.payload) free(global.rtps[i].prev_packet.payload);	
		if (global.rtps[i].filename) free(global.rtps[i].filename);
		if (global.rtps[i].mp4File) gf_isom_close(global.rtps[i].mp4File);
	}	
	free(global.rtps);
	free(global.dest_ip);
	
	gf_sys_close();

	fprintf(stdout, "done\n");
	return GF_OK;

} /* end main */

	/*seems OK under mingw also*/
#ifdef WIN32
#include <conio.h>
#include <windows.h>
Bool has_input()
{
	return kbhit();
}
u8 get_a_char()
{
	return getchar();
}
void set_echo_off(Bool echo_off) 
{
	DWORD flags;
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	GetConsoleMode(hStdin, &flags);
	if (echo_off) flags &= ~ENABLE_ECHO_INPUT;
	else flags |= ENABLE_ECHO_INPUT;
	SetConsoleMode(hStdin, flags);
}
#else
/*linux kbhit/getchar- borrowed on debian mailing lists, (author Mike Brownlow)*/
#include <termios.h>

static struct termios t_orig, t_new;
static s32 ch_peek = -1;

void init_keyboard()
{
	tcgetattr(0, &t_orig);
	t_new = t_orig;
	t_new.c_lflag &= ~ICANON;
	t_new.c_lflag &= ~ECHO;
	t_new.c_lflag &= ~ISIG;
	t_new.c_cc[VMIN] = 1;
	t_new.c_cc[VTIME] = 0;
	tcsetattr(0, TCSANOW, &t_new);
}
void close_keyboard(Bool new_line)
{
	tcsetattr(0,TCSANOW, &t_orig);
	if (new_line) fprintf(stdout, "\n");
}

void set_echo_off(Bool echo_off) 
{ 
	init_keyboard();
	if (echo_off) t_orig.c_lflag &= ~ECHO;
	else t_orig.c_lflag |= ECHO;
	close_keyboard(0);
}

Bool has_input()
{
	u8 ch;
	s32 nread;

	init_keyboard();
	if (ch_peek != -1) return 1;
	t_new.c_cc[VMIN]=0;
	tcsetattr(0, TCSANOW, &t_new);
	nread = read(0, &ch, 1);
	t_new.c_cc[VMIN]=1;
	tcsetattr(0, TCSANOW, &t_new);
	if(nread == 1) {
		ch_peek = ch;
		return 1;
	}
	close_keyboard(0);
	return 0;
}

u8 get_a_char()
{
	u8 ch;
	if (ch_peek != -1) {
		ch = ch_peek;
		ch_peek = -1;
		close_keyboard(1);
		return ch;
	}
	read(0,&ch,1);
	close_keyboard(1);
	return ch;
}

#endif

