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
#include <gpac/constants.h>
#include <gpac/isomedia.h>
#include <gpac/ietf.h>
#include <gpac/internal/ietf_dev.h>
#include <gpac/config.h>
#include <gpac/thread.h>
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

typedef struct {	
	// config
	u32 burstDuration;
	u32 burstBitRate;
	u32 burstSize;
	u32 offDuration;
	u32 averageBitRate;

	u32 nbSessions;
	char * dest_ip;				// destination ip
	u32 *portTable;			// port of each session
	char **fileTable;		// filename of each session
	u32 *trackNbTable;		// track id of each session
	u32 *loopingTable;			// 1 play in a loop, 0 play once
	struct _rtp_caller *rtps;

	// time
	u32 timelineOrigin; // time when the first burts was sent. this time <=> (CTS=0)

	// other param
	u32 nbBurstSent; // toutes sessions confondues
	u32 nbActiveThread;

	u32 log_level;
} Simulator;

typedef struct {
	GF_RTPHeader header;
	char *payload;
	u32 payload_len;
} RTP_Packet;

typedef struct _rtp_caller {
	u32 id;
	u32 mediaBitRate; 
	u32 mediaSize;
	u32 mediaDuration;
	u32 nbAccessUnits;
	u32 minBurstSize; // max size to fill the burst with the media bitRate
	u32 looping; // 1: play the media in a loop, 0 play once

	Bool processAU;
	u32 accessUnitProcess; // number of the AU to process. set to 0 at the beginnning
	u32 nbBurstSent; // pour cette session
	u32 nextBurstTime;
	u32 dataLengthInBurst; // total length filled in current burst
	s32 drift;

	/* Type */
	GF_RTPChannel *channel; 
	GP_RTPPacketizer *packetizer;
	GF_ISOSample  *accessUnit; // the AU
	GF_ISOFile *mp4File;
	u32 trackNum;

	/* The previous packet which could not be sent in previous burst*/
	RTP_Packet prev_packet;
	/* The current packet being formed */
	RTP_Packet packet;
	u32 ts_offset;
	u32 next_ts;


	/* Management */
	Simulator *global;
	GF_Thread *thread; // thread of session 1 is not used
	Bool run;
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
	RTP_Caller *caller = cbk;
	if (!header) return;
	memcpy(&caller->packet.header, header, sizeof(GF_RTPHeader));
} /* OnNewPacket */

/*
 * The RTP packetiser is done with the current RTP packet
 * the header may have changed since the beginning of the packet (OnNewPacket)
 * 
 */
static void OnPacketDone(void *cbk, GF_RTPHeader *header) 
{
	RTP_Caller *caller = cbk;
	u32 currentPacketSize; // in bits

	/* Is there a remaining packet to send which did not fit into previous burst */
	if (caller->prev_packet.payload) {
		
		gf_rtp_send_packet(caller->channel, &caller->prev_packet.header, 0, 0, caller->prev_packet.payload, caller->prev_packet.payload_len);
		currentPacketSize = (caller->prev_packet.payload_len + RTP_HEADER_SIZE) * 8; // in bits
		caller->dataLengthInBurst+= currentPacketSize; 
		free(caller->prev_packet.payload);				
		caller->prev_packet.payload = NULL;

		if (caller->global->log_level == 1) fprintf(stdout, "  RTP SN %u - TS %u - M %u - Size %u\n", caller->prev_packet.header.SequenceNumber, caller->prev_packet.header.TimeStamp/90, caller->prev_packet.header.Marker, currentPacketSize);
	}

	currentPacketSize = (caller->packet.payload_len + RTP_HEADER_SIZE) * 8; // in bits
	if (caller->dataLengthInBurst + currentPacketSize < caller->global->burstSize ) { // send the RTP packets in gf_list
		
		gf_rtp_send_packet(caller->channel, header, 0, 0, caller->packet.payload, caller->packet.payload_len);
		caller->dataLengthInBurst += currentPacketSize; 
		free(caller->packet.payload);				
		caller->packet.payload = NULL;
		caller->packet.payload_len = 0;
		
		if (caller->global->log_level == 1) fprintf(stdout, "  RTP SN %u - TS %u - M %u - Size %u\n", caller->packet.header.SequenceNumber, caller->packet.header.TimeStamp/90, caller->packet.header.Marker, currentPacketSize);
		
		if (caller->nextBurstTime + caller->global->burstDuration + caller->global->offDuration < caller->packet.header.TimeStamp/caller->packetizer->sl_config.timestampResolution*1000) {
			caller->processAU = 0;
		}
#if 0
		if (caller->drift < -3000) {
			if (caller->dataLengthInBurst > caller->minBurstSize) caller->processAU = 0;
		} else {
			if (caller->dataLengthInBurst > caller->minBurstSize 
				+ 0.1*(caller->global->burstSize - caller->minBurstSize)) caller->processAU = 0;
		}
#endif

	} else {
		fprintf(stdout, "Packet delayed - Buffer overflow - wrong configuration:\nsend %d ms data - should have sent %d \n", caller->nextBurstTime + caller->global->burstDuration + caller->global->offDuration, caller->packet.header.TimeStamp/caller->packetizer->sl_config.timestampResolution*1000);

		memcpy(&caller->prev_packet.header, header, sizeof(GF_RTPHeader));
		caller->prev_packet.payload = caller->packet.payload;
		caller->prev_packet.payload_len = caller->packet.payload_len;
		caller->packet.payload = NULL;
		caller->packet.payload_len = 0;		
		caller->processAU = 0;
	}

} /* OnPacketdone */

/*
 * The RTP packetiser has added data to the current RTP packet
 */
static void OnData(void *cbk, char *data, u32 data_size, Bool is_head) 
{
	RTP_Caller *caller = cbk;
	if (!data ||!data_size) return;

	if (!caller->packet.payload_len) {
		caller->packet.payload = malloc(data_size);
		memcpy(caller->packet.payload, data, data_size);
		caller->packet.payload_len = data_size;
	} else {
		caller->packet.payload = realloc(caller->packet.payload, caller->packet.payload_len + data_size);
		if (!is_head) {
			memcpy(caller->packet.payload+caller->packet.payload_len, data, data_size);
		} else {
			memmove(caller->packet.payload+data_size, caller->packet.payload, caller->packet.payload_len);
			memcpy(caller->packet.payload, data, data_size);
		}
		caller->packet.payload_len += data_size;
	}	

} /* OnData */

GF_Err rtp_init_packetizer(RTP_Caller *caller)
{	
	FILE *sdp_out;
	char filename[30];
	char mediaName[30], payloadName[30];
	char sdpLine[20000];

	caller->packetizer = gf_rtp_packetizer_create_and_init_from_file(caller->mp4File, caller->trackNum, caller,
											OnNewPacket, OnPacketDone, NULL, OnData,
											1500-12, 0, 0, GP_RTP_PCK_SIGNAL_RAP | GP_RTP_PCK_FORCE_MPEG4, 
											BASE_PAYT + caller->id - 1,
											0, 0, 0);

	gf_rtp_builder_get_payload_name(caller->packetizer, payloadName, mediaName);

	sprintf(filename, "session%d.sdp", caller->id);
	sdp_out = fopen(filename, "wt");
	if (sdp_out) {
		sprintf(sdpLine, "v=0");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "o=MP4Streamer 3357474383 1148485440000 IN IP4 %s", caller->global->dest_ip);
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "s=livesession");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "i=This is an MP4 time-sliced Streaming demo");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "u=http://gpac.sourceforge.net");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "e=admin@");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "c=IN IP4 %s", caller->global->dest_ip);
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "t=0 0");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "a=x-copyright: Streamed with GPAC (C)2000-200X - http://gpac.sourceforge.net\n");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "m=%s %d RTP/%s %d", mediaName, caller->global->portTable[caller->id-1], caller->packetizer->slMap.IV_length ? "SAVP" : "AVP", caller->packetizer->PayloadType);
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "a=rtpmap:%d %s/%d", caller->packetizer->PayloadType, payloadName, caller->packetizer->sl_config.timestampResolution);
		fprintf(sdp_out, "%s\n", sdpLine);
		
		if (gf_isom_get_media_type(caller->mp4File, caller->trackNum) == GF_ISOM_MEDIA_VISUAL) {
			u32 w, h;
			w = h = 0;
			gf_isom_get_visual_info(caller->mp4File, caller->trackNum, 1, &w, &h);
			if (caller->packetizer->rtp_payt == GP_RTP_PAYT_H263) {
				sprintf(sdpLine, "a=cliprect:0,0,%d,%d", h, w);
			}
			/*extensions for some mobile phones*/
			sprintf(sdpLine, "a=framesize:%d %d-%d", caller->packetizer->PayloadType, w, h);
		}
		/*AMR*/
		if ((caller->packetizer->rtp_payt == GP_RTP_PAYT_AMR) || (caller->packetizer->rtp_payt == GP_RTP_PAYT_AMR_WB)) {
			sprintf(sdpLine, "a=fmtp:%d octet-align", caller->packetizer->PayloadType);
			fprintf(sdp_out, "%s\n", sdpLine);
		}
		/*Text*/
		else if (caller->packetizer->rtp_payt == GP_RTP_PAYT_3GPP_TEXT) {
			gf_hinter_format_ttxt_sdp(caller->packetizer, payloadName, sdpLine, caller->mp4File, caller->trackNum);
			fprintf(sdp_out, "%s\n", sdpLine);
		}
		/*EVRC/SMV in non header-free mode*/
		else if ((caller->packetizer->rtp_payt == GP_RTP_PAYT_EVRC_SMV) && (caller->packetizer->auh_size>1)) {
			sprintf(sdpLine, "a=fmtp:%d maxptime=%d", caller->packetizer->PayloadType, caller->packetizer->auh_size*20);
			fprintf(sdp_out, "%s\n", sdpLine);
		}
		/*H264/AVC*/
		else if (caller->packetizer->rtp_payt == GP_RTP_PAYT_H264_AVC) {
			GF_AVCConfig *avcc = gf_isom_avc_config_get(caller->mp4File, caller->trackNum, 1);
			sprintf(sdpLine, "a=fmtp:%d profile-level-id=%02X%02X%02X; packetization-mode=1", caller->packetizer->PayloadType, avcc->AVCProfileIndication, avcc->profile_compatibility, avcc->AVCLevelIndication);
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
		else if (caller->packetizer->rtp_payt==GP_RTP_PAYT_MPEG4) {
			GF_DecoderConfig *dcd;
			dcd = gf_isom_get_decoder_config(caller->mp4File, caller->trackNum, 1);

			if (dcd && dcd->decoderSpecificInfo && dcd->decoderSpecificInfo->data) {
				gf_rtp_builder_format_sdp(caller->packetizer, payloadName, sdpLine, dcd->decoderSpecificInfo->data, dcd->decoderSpecificInfo->dataLength);
			} else {
				gf_rtp_builder_format_sdp(caller->packetizer, payloadName, sdpLine, NULL, 0);
			}
			if (dcd) gf_odf_desc_del((GF_Descriptor *)dcd);

			if (caller->packetizer->slMap.IV_length) {
				const char *kms;
				gf_isom_get_ismacryp_info(caller->mp4File, caller->trackNum, 1, NULL, NULL, NULL, NULL, &kms, NULL, NULL, NULL);
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
		else if (caller->packetizer->rtp_payt==GP_RTP_PAYT_LATM) { 
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
			dcd = gf_isom_get_decoder_config(caller->mp4File, caller->trackNum, 1); 
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
 
			gf_rtp_builder_format_sdp(caller->packetizer, payloadName, sdpLine, config_bytes, config_size); 
			fprintf(sdp_out, "%s\n", sdpLine);
			free(config_bytes); 
		}
		fprintf(sdp_out, "\n");

		fclose(sdp_out);

	}
	return GF_OK;
} /* rtp_init_packetizer */

GF_Err rtp_init_channel(GF_RTPChannel **chan, char * dest, int port)
{

	GF_RTSPTransport tr;
	GF_Err res;

	*chan = gf_rtp_new();
	gf_rtp_set_ports(*chan);

	tr.IsUnicast=1;
	tr.Profile="RTP/AVP";//RTSP_PROFILE_RTP_AVP;
	tr.destination = dest;
	tr.source = "0.0.0.0";
	tr.IsRecord = 0;
	tr.Append = 0;
	tr.SSRC = rand();
	
	tr.client_port_first = port; //RTP
	tr.client_port_last  = port+1; //RTCP (not used a priori)

	tr.port_first        = (*chan)->net_info.port_first; //RTP other end 
	tr.port_last         = (*chan)->net_info.port_last; //RTCP other end (not used a priori)

	res = gf_rtp_setup_transport(*chan, &tr, dest);
	if (res !=0) return res;

	gf_rtp_initialize(*chan, 0, 1, 1500, 0, 0, NULL);
	if (res !=0) return res;	

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
	u32 timescale;
	u32 sampleDuration;
	u32 sampleDescIndex;
	u64 ts;
	Double ts_scale;
		
	if (rtp->global->nbBurstSent == 0) {
		u32 i;
		rtp->global->timelineOrigin = gf_sys_clock();
		for (i=1; i<rtp->global->nbSessions; i++) {
			rtp->global->rtps[i].nextBurstTime = rtp->global->timelineOrigin + i*rtp->global->burstDuration;
		}
	}

	timescale = gf_isom_get_media_timescale(rtp->mp4File, rtp->trackNum);

	rtp->drift = (s32)(gf_sys_clock()-rtp->global->timelineOrigin) - (s32) (rtp->next_ts*1000/rtp->packetizer->sl_config.timestampResolution);

	fprintf(stdout, "Time %u - Burst %u - Session %u - Drift %d ms\n", (gf_sys_clock()-rtp->global->timelineOrigin), rtp->global->nbBurstSent, rtp->id, rtp->drift);

	
	ts_scale = rtp->packetizer->sl_config.timestampResolution;
	ts_scale /= timescale;
	
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
												
			gf_rtp_builder_process(rtp->packetizer, rtp->accessUnit->data, rtp->accessUnit->dataLength, 1, 
								   rtp->accessUnit->dataLength, sampleDuration, sampleDescIndex);
			gf_isom_sample_del(&rtp->accessUnit);

			rtp->next_ts = (u32)ts + sampleDuration + rtp->ts_offset;

			rtp->accessUnitProcess ++;
		} else {
			if (rtp->looping) {
				rtp->accessUnitProcess = 0;
				rtp->ts_offset = rtp->next_ts;
			} else {
				rtp->accessUnitProcess = 0;
				rtp->processAU = 0;
			}

		}
	} 
	fprintf(stdout, "  Actual Burst Size %d bits - Actual Bit Rate %d kbps\n", rtp->dataLengthInBurst, rtp->dataLengthInBurst/(rtp->global->burstDuration+rtp->global->offDuration));
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
	char config[1024];		/* cfg file name */
	GF_Config *lp_configFile;	/* Configuration file struct */
	u32 i;						/* counter */

	sprintf(config,"%s",av_argv1); // copy the path 

	fprintf(stdout, "Configuration file: %s \n", config);

	lp_configFile = gf_cfg_new(PATHFILE,config);	
	if(!lp_configFile)
	{
		fprintf(stderr, "ERROR: could not open the file\n");
		return GF_IO_ERR;
	}

	av_simulator->nbSessions = atoi(gf_cfg_get_key(lp_configFile, "GLOBAL", "nbSession"));
	fprintf(stdout, " Number of sessions: %u \n", av_simulator->nbSessions);

	av_simulator->dest_ip = strdup(gf_cfg_get_key(lp_configFile, "GLOBAL", "IP_dest"));
	fprintf(stdout, " Destination IP: %s \n", av_simulator->dest_ip);	
	
	av_simulator->offDuration = atoi(gf_cfg_get_key(lp_configFile, "GLOBAL", "off_duration"));
	fprintf(stdout, " Offtime duration: %u ms \n", av_simulator->offDuration);

	av_simulator->burstDuration = atoi(gf_cfg_get_key(lp_configFile, "GLOBAL", "burst_duration"));
	fprintf(stdout, " Burst duration: %u ms \n", av_simulator->burstDuration);

	av_simulator->burstBitRate = atoi(gf_cfg_get_key(lp_configFile, "GLOBAL", "burst_bitrate"));
	fprintf(stdout, " Burst bit rate: %u kbps \n", av_simulator->burstBitRate);

	av_simulator->burstSize = av_simulator->burstBitRate*av_simulator->burstDuration;
	fprintf(stdout, " Burst size: %u bits \n", av_simulator->burstSize);

	av_simulator->averageBitRate = (av_simulator->burstDuration * av_simulator->burstBitRate)/(av_simulator->offDuration + av_simulator->burstDuration); // kbps
	fprintf(stdout, " Average Bit Rate: %u kbps\n", av_simulator->averageBitRate);

	// for each session:  cfg file
	av_simulator->portTable = (u32 *)malloc(av_simulator->nbSessions * sizeof(u32));
	av_simulator->trackNbTable = (u32 *)malloc(av_simulator->nbSessions * sizeof(u32));
	av_simulator->fileTable = (char **)malloc(av_simulator->nbSessions * sizeof(char *));
	av_simulator->loopingTable = (u32 *)malloc(av_simulator->nbSessions * sizeof(u32));

	for(i=0; i<= av_simulator->nbSessions - 1; i++)
	{
		char sessionName[1024];

		sprintf(sessionName, "SESSION%d",i+1);						
		// retrieve parameters in cfg file
		av_simulator->fileTable[i] = strdup(gf_cfg_get_key(lp_configFile, sessionName, "file"));		
		av_simulator->portTable[i] = atoi(gf_cfg_get_key(lp_configFile, sessionName, "port"));
		av_simulator->trackNbTable[i] = atoi(gf_cfg_get_key(lp_configFile, sessionName, "track_nb"));
		av_simulator->loopingTable[i] = atoi(gf_cfg_get_key(lp_configFile, sessionName, "looping"));
	}
	fprintf(stdout, "\n");

	gf_cfg_del(lp_configFile);
	return 1;
} /* configuration */

// ---------------------------------------------------------------------------------------------------

/*
 * timelineManagement
 *	called by a thread or in the main() for the 1st session
 *	1 thread <=> 1 session
 *
 */

GF_Err timelineManagement(void *caller)
{
	RTP_Caller *rtp = caller;
	s32 sleepDuration;		/* in ms */
	u32 currentTime;			/* in ms */

	/*wait till the timeline origin is set by the first packetizer */
	if (rtp->id != 1) 
		while (!rtp->global->timelineOrigin)
			gf_sleep(20);

	do {
		currentTime = gf_sys_clock();
		sleepDuration = rtp->nextBurstTime - currentTime; 
		if (sleepDuration <= 0) {
			// begin the process now because sleepDuration is negative
			sendBurst(rtp);
		} else {		
			gf_sleep(sleepDuration);
			sendBurst(rtp);
		}			
		rtp->dataLengthInBurst = 0; 
		rtp->nextBurstTime = rtp->global->timelineOrigin + (rtp->id-1)*rtp->global->burstDuration +
							 (rtp->global->burstDuration+rtp->global->offDuration)*rtp->nbBurstSent;
		if (rtp->accessUnitProcess == 0 && rtp->looping == 0) rtp->run = 0;
	} while (rtp->run);

	rtp->global->nbActiveThread--;
	return GF_OK;

} /* timelineManagement */

// ---------------------------------------------------------------------------------------------------

/*
 * checkConfig
 *	checks if the parameters the user has choosen
 *	matches with the play of the media
 */

GF_Err checkConfig(RTP_Caller *caller)
{	
	RTP_Caller *rtp = caller;

	rtp->mediaDuration = (u32)(gf_isom_get_track_duration(rtp->mp4File, rtp->trackNum)/gf_isom_get_timescale(rtp->mp4File)*1000); // ms
	rtp->mediaSize = (u32)gf_isom_get_media_data_size(rtp->mp4File, rtp->trackNum) * 8; // (gf_isom_get_media_data_size en octets)
	
	rtp->mediaBitRate =  rtp->mediaSize / rtp->mediaDuration; // in bps
	rtp->minBurstSize = rtp->mediaBitRate * (rtp->global->burstDuration+rtp->global->offDuration); // bps * ms ==> bits
	rtp->looping = rtp->global->loopingTable[rtp->id -1];
	
	fprintf(stdout, " Session %u:\n", rtp->id);
	fprintf(stdout, "  File %s - Track %u - Destination Port %u \n", rtp->global->fileTable[rtp->id-1], rtp->trackNum, rtp->global->portTable[rtp->id-1]);
	fprintf(stdout, "  Size %u bits - Duration %u ms - Bit Rate %u kbps\n", rtp->mediaSize, rtp->mediaDuration, rtp->mediaBitRate);
	fprintf(stdout, "  Min Burst Size %u bits\n", rtp->minBurstSize);
		
	if (rtp->global->averageBitRate <= rtp->mediaBitRate) {
		fprintf(stderr, "The media %u you will receive will not play continuously\n", rtp->id);
		fprintf(stderr, " since you do not send enough data in a burst. Increase the burst bit rate\n");
		fprintf(stderr, " Please reconfigure your cfg file\n");
		return GF_BAD_PARAM;
	}
	
	return GF_OK;

} /* checkConfig */

// ---------------------------------------------------------------------------------------------------

/*
 * usage prints the usage of this module when it is not well-used
 *
 */

void usage(char *argv0) 
{
	fprintf(stderr, "usage: %s configurationFile \n", argv0);
} /* usage */

// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
// MAIN
// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------

/*
 *  Main 
 *  The argument passed are:
 *		1- the configuration file
 *
 */

int main(int argc, char **argv)
{
	/*
	 * Declarations of local variables
	 *
	 */
	Simulator global;		/* Simulator metadata (from cfg file) */
	Bool mainrun;
	GF_Err e = GF_OK;			/* Error Message */	
	u32 i;						

	// test 
	if (argc < 2) 
	{
		usage(argv[0]);
		return GF_IO_ERR;
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
	memset(&global, 0, sizeof(Simulator));
				
	/*
	 * Configuration File
	 *
	 */ 		
	configuration(&global, argv[1]);
	  
	/*
	 *	Allocation
	 *
	 */	
	global.rtps = (RTP_Caller *)malloc(global.nbSessions * sizeof(RTP_Caller));
	
	for(i=0; i<= global.nbSessions - 1; i++)
	{			
		memset(&global.rtps[i], 0, sizeof(RTP_Caller)); /* Allocation */		
		global.rtps[i].global = &global;		
		global.rtps[i].id = i+1;	
		global.rtps[i].accessUnitProcess = 0;
		global.rtps[i].nextBurstTime = 0;
		global.rtps[i].dataLengthInBurst = 0;
		global.rtps[i].thread = gf_th_new();	
		global.rtps[i].run = 1;
		global.rtps[i].mp4File = gf_isom_open(global.fileTable[i], GF_ISOM_OPEN_READ, NULL);
		if (!global.rtps[i].mp4File) {
			fprintf(stderr, "ERROR: Could not open input MP4 file of session %u\n", i);
			return GF_IO_ERR;
		} 
		global.rtps[i].trackNum = global.trackNbTable[i];

		e = rtp_init_channel(&global.rtps[i].channel, global.dest_ip, global.portTable[i]);
		if (e) {
			fprintf(stderr, "Could not initialize RTP Channel\n");
		}			
		e = rtp_init_packetizer(&global.rtps[i]);
		if (e) {
			fprintf(stderr, "Could not initialize Packetizer \n");
		}
	}
	  
	/*
	 * Check the configuration of the network
	 */

	// nb_service * burst duration <= off time
	if((global.nbSessions -1) * global.burstDuration > global.offDuration )
	{
		fprintf(stderr, "CFG ERROR: nb sessions * burst duration > off time\n");
		fprintf(stderr, "CFG ERROR: please reconfigure your cfg file\n");
		return GF_IO_ERR;
	}
	// burst 

	fprintf(stdout, "Media information:\n");
	for(i=0; i<= global.nbSessions -1; i++)
	{
		if (checkConfig(&global.rtps[i])) exit(-1);
	}
	fprintf(stdout, "=> Streaming is possible\n\n");

	fprintf(stdout, "Streaming information:\n");
	for(i=0; i<= global.nbSessions -1; i++)
	{
		if (global.trackNbTable[i] > gf_isom_get_track_count(global.rtps[i].mp4File)) 
		{
				fprintf(stderr, "ERROR: session %u: Track > track count \n", i);
				return GF_IO_ERR;			
		}				
		global.rtps[i].nbAccessUnits = gf_isom_get_sample_count(global.rtps[i].mp4File, global.rtps[i].trackNum);
	} /* end init */
		
	for(i=0; i<global.nbSessions; i++) // except last session 
	{	
		gf_th_run(global.rtps[i].thread, timelineManagement, &global.rtps[i] );
		global.nbActiveThread++;
	}		

	mainrun = 1;
	while (mainrun) {
		char c;
		if (!has_input()) {
			if (global.nbActiveThread == 0) mainrun = 0;
			gf_sleep(200);
		}

		c = get_a_char();

		switch (c) {
		case 'q':
			mainrun = 0;
			break;

		default:
			break;
		}
	}
	
	for (i = 0; i<global.nbSessions; i++) {
		global.rtps[i].run = 0;
	}

	while (global.nbActiveThread) {
		gf_sleep(20);
	}
	/*
	 * Desallocation 
	 *
	 */
	
	for(i=0; i<global.nbSessions; i++)
	{
		if (global.rtps[i].packet.payload) free(global.rtps[i].packet.payload);	
		if (global.rtps[i].prev_packet.payload) free(global.rtps[i].prev_packet.payload);	
		gf_rtp_del(global.rtps[i].channel);
		gf_rtp_builder_del(global.rtps[i].packetizer);
		if (global.rtps[i].mp4File) gf_isom_close(global.rtps[i].mp4File);
		gf_th_del(global.rtps[i].thread);
		free(global.fileTable[i]);
	}	
	free(global.rtps);
	free(global.loopingTable);
	free(global.portTable);
	free(global.fileTable);
	free(global.trackNbTable);
	free(global.dest_ip);
	
	gf_sys_close();

	fprintf(stdout, "done\n");
	return e;

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

