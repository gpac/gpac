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
	u32 sov_burstDuration;
	u32 sov_peakBitRate;
	u32 sov_delay_offtime;
	u32 sov_nbSessions;
	char * sop_ip;				// destination ip
	u32 *sop_portTable;			// port of each session
	char **sop_fileTable;		// filename of each session
	u32 *sop_trackNbTable;		// track id of each session
	u32 *sop_looping;			// 1 play in a loop, 0 play once

	// time
	u32 ov_currentTime;	
	u32 sov_timeOffset; // time when the first burts was sent. this time <=> (CTS=0)

	// other param
	u32 sov_numberBurstSent; // toutes sessions confondues
} Simulator;

typedef struct {
	GF_RTPHeader header;
	char *payload;
	u32 payload_len;
} RTP_Packet;

typedef struct {
	/* id */
	int id;
	u32 mediaBitRate; 
	u32 mediaSize;
	u32 mediaDuration;
	u32 nbAccesUnit;
	u32 maxSizeBurst; // max size to fill the burst with the media bitRate
	u32 physicalBitRate; // cf definition in checkConfiguration
	u32 looping; // 1: play the media in a loop, 0 play once

	/* access unit*/
	u32 accessUnitProcess; // number of the AU to process. set to 0 at the beginnning
	u32 nextBurstTime;
	u32 dataLengthInBurst; // total length filled in current burst
	u64 totalLengthSent;   // total length sent to channel

	/* Type */
	GF_RTPChannel *channel; 
	GP_RTPPacketizer *packetizer;
	GF_List *packets_to_send;	
	GF_ISOSample  *accessUnit; // the AU
	GF_ISOFile *mp4File;
	u32 trackNum;

	/* The current packet being formed */
	GF_RTPHeader header;
	char *payload;
	u32 payload_len;
	u32 payload_valid_len;

	/* Management */
	Simulator *global;
	GF_Thread *thread; // thread of session 1 is not used
} RTP_Caller;

//------------------------------------------------------------
// Functions
//------------------------------------------------------------

/*
 * initTimeManagement
 *	is called when the first burst of the session 1 is sent
 *	sets the nextBurstTime 
 *		-> session 1: offset + offTime
 *		-> other: offset + (id -1) * offTime
 */

void initTimeManagement(RTP_Caller *ap_caller)
{	// set the time when CTS = 0
	if(ap_caller->id == 1)
	{
		ap_caller->nextBurstTime = ap_caller->global->sov_timeOffset + ap_caller->global->sov_burstDuration + ap_caller->global->sov_delay_offtime;		
	}
	else
	{
		ap_caller->nextBurstTime = ap_caller->global->sov_timeOffset + (ap_caller->id - 1)* ap_caller->global->sov_burstDuration;
	}
//	fprintf(stdout,"[initMgtTime] Session %u: currentTime %u - nextBurstTime %u\n",ap_caller->id, gf_sys_clock(),ap_caller->nextBurstTime);
}

/*
 * setNextBurstTime
 *	is called in OnPacketDone just before the burst is sent
 *	sets the nextBurstTime of the given session
 */
void setNextBurstTime(RTP_Caller *ap_caller)
{
	ap_caller->nextBurstTime = gf_sys_clock() +  ap_caller->global->sov_burstDuration + ap_caller->global->sov_delay_offtime;
//	fprintf(stdout,"[setNextBurstTime] Session %u: currentTime %u - nextBurstTime %u\n",ap_caller->id, gf_sys_clock(),ap_caller->nextBurstTime);
}

/*
 * callback functions, called by the RTP packetiser 
 *
 */

/*
 * The RTP packetizer is starting a new RTP packet and is giving the header
 */
static void OnNewPacket(void *cbk, GF_RTPHeader *header)
{
	RTP_Caller *caller = cbk;
	if (!header) return;
	memcpy(&caller->header, header, sizeof(GF_RTPHeader));
	//fprintf(stdout, "[ONP] _session %u\n", caller->id);
} /* OnNewPacket */

/*
 * The RTP packetiser is done with the current RTP packet
 * the header may have changed since the beginning of the packet (OnNewPacket)
 * 
 */
static void OnPacketDone(void *cbk, GF_RTPHeader *header) 
{
	RTP_Caller *caller = cbk;
	u32 lv_sizeSent;		// taille totale des paquets envoyés pour le burst courant (bits)
	u32 lv_totalLengthFilled =0; // in bits
	u32 i;
	u32 lv_packetCount=0;
	u32 lv_currentPacketSize; // in bits
	RTP_Packet *tmp;
	
	lv_sizeSent = 0;	
	caller->global->ov_currentTime = gf_sys_clock();// gets system clock (current time)	
		
	//fprintf(stdout,"[OPD] _id %u _AU %u _currentTime %u _nextBursTime %u\n",caller->id, caller->accessUnitProcess, caller->global->ov_currentTime);

	GF_SAFEALLOC(tmp, RTP_Packet);
	memcpy(&tmp->header, header, sizeof(GF_RTPHeader));
	tmp->payload = caller->payload;
	caller->payload = NULL;
	tmp->payload_len = caller->payload_len;
	caller->payload_len = 0;
	caller->payload_valid_len = 0;		
	
	lv_packetCount = gf_list_count(caller->packets_to_send);	
	lv_currentPacketSize = (tmp->payload_len + RTP_HEADER_SIZE) * 8; // in bits

	if(lv_packetCount == 0)
	{
		lv_totalLengthFilled += lv_currentPacketSize;
	}
	else
	{
		for (i=0; i<lv_packetCount; i++) 		
		{
			RTP_Packet *lp_buffer;
			lp_buffer = gf_list_get(caller->packets_to_send, i);
			lv_totalLengthFilled += (lp_buffer->payload_len + RTP_HEADER_SIZE) * 8;
		} 
		lv_totalLengthFilled += lv_currentPacketSize;
	}		
			
	if( lv_totalLengthFilled >= caller->maxSizeBurst ) 
	{ // send the RTP packets in gf_list

		if(caller->global->sov_numberBurstSent == 0 && (caller->id == 1)) {
			/*
			 * SPECIFIC TO THE FIRST SESSION 
			 * Check for the first burst sending
			 *	sets the offset time, ie the time when the very first burst	was sent
			 *	==> allows to synchronize the CTS time unit to the system time
			 */
			caller->global->sov_timeOffset = caller->global->ov_currentTime; 
		} else {
			s32 lv_sleep;
			
			lv_sleep = caller->nextBurstTime - caller->global->ov_currentTime;
			if (lv_sleep <0)
			{
				lv_sleep = 0;
				fprintf(stdout,"ERROR sleep <0: nextBurstTime %u CurrentTime %u Offset %u\n",caller->nextBurstTime, caller->global->ov_currentTime, caller->global->sov_timeOffset);
			} else {			
				gf_sleep(lv_sleep); 
			}
			
			setNextBurstTime(caller);
		}
		
		if(lv_packetCount == 0)
		{
			caller->totalLengthSent += (tmp->payload_len)*8; 
			gf_rtp_send_packet(caller->channel, &tmp->header, 0, 0, tmp->payload, tmp->payload_len);
			free(tmp->payload);				
		}
		else
		{
			for (i=0; i<lv_packetCount; i++) 
			{			
				RTP_Packet *rtp = gf_list_get(caller->packets_to_send, i);
				caller->totalLengthSent += (rtp->payload_len)*8; // update the total length					
				gf_rtp_send_packet(caller->channel, &rtp->header, 0, 0, rtp->payload, rtp->payload_len);
				free(rtp->payload);	
			}			
			gf_list_reset(caller->packets_to_send);
		}
		fprintf(stdout, "Burst %u sent - Session %u - %u bits - %u RTP packets\n", caller->global->sov_numberBurstSent, caller->id, lv_totalLengthFilled, lv_packetCount);
		caller->global->sov_numberBurstSent++;

	} // end if		  
	gf_list_add(caller->packets_to_send, tmp); // store the last packet not sent

} /* OnPacketdone */

/*
 * The RTP packetiser has added data to the current RTP packet
 */
static void OnData(void *cbk, char *data, u32 data_size, Bool is_head) 
{
	RTP_Caller *caller = cbk;
	if (!data ||!data_size) return;

	if (!caller->payload_len) {
		caller->payload = malloc(data_size);
		memcpy(caller->payload, data, data_size);
		caller->payload_len = caller->payload_valid_len = data_size;
	} else {
		if (caller->payload_valid_len + data_size > caller->payload_len) {
			caller->payload = realloc(caller->payload, caller->payload_valid_len + data_size);
			caller->payload_len = caller->payload_valid_len + data_size;
		} 
		if (!is_head) {
			memcpy(caller->payload+caller->payload_valid_len, data, data_size);
		} else {
			memmove(caller->payload+data_size, caller->payload, caller->payload_valid_len);
			memcpy(caller->payload, data, data_size);
		}
		caller->payload_valid_len += data_size;
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
											1500, 0, 0, GP_RTP_PCK_SIGNAL_RAP | GP_RTP_PCK_FORCE_MPEG4, 
											BASE_PAYT + caller->id - 1,
											0, 0, 0);

	gf_rtp_builder_get_payload_name(caller->packetizer, payloadName, mediaName);

	sprintf(filename, "session%d.sdp", caller->id);
	sdp_out = fopen(filename, "wt");
	if (sdp_out) {
		sprintf(sdpLine, "v=0");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "o=MP4Streamer 3357474383 1148485440000 IN IP4 %s", caller->global->sop_ip);
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "s=livesession");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "i=This is an MP4 time-sliced Streaming demo");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "u=http://gpac.sourceforge.net");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "e=admin@");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "c=IN IP4 %s", caller->global->sop_ip);
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "t=0 0");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "a=x-copyright: Streamed with GPAC (C)2000-200X - http://gpac.sourceforge.net\n");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "m=%s %d RTP/%s %d", mediaName, caller->global->sop_portTable[caller->id-1], caller->packetizer->slMap.IV_length ? "SAVP" : "AVP", caller->packetizer->PayloadType);
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

void paquetisation(RTP_Caller *ap_caller, GF_ISOFile *ap_mp4)
{
	u32 lv_accesUnitLength;	// in bits
	u32 lv_totalLengthFilled; // in bits
	u32 lv_timescale;
	u32 lv_sampleDuration;
	u32 lv_sampleDescIndex;
	u64 lv_ts;
	u32 lv_auCount;
		
	lv_accesUnitLength = 0;
	lv_totalLengthFilled = 0;
	lv_timescale = gf_isom_get_media_timescale(ap_mp4, ap_caller->trackNum);
	lv_auCount= gf_isom_get_sample_count(ap_mp4, ap_caller->trackNum);
		
	while (lv_totalLengthFilled <= ap_caller->maxSizeBurst)
	{			
		if(ap_caller->accessUnitProcess + 1 <= lv_auCount) // attention overflow acceUnit number <= get_sample_count
		{
			ap_caller->accessUnit = gf_isom_get_sample(ap_mp4, ap_caller->trackNum, ap_caller->accessUnitProcess + 1, &lv_sampleDescIndex);
			lv_accesUnitLength =  ap_caller->accessUnit->dataLength * 8;
			
			if( (lv_totalLengthFilled + lv_accesUnitLength) <= ap_caller->maxSizeBurst ) // OK
			{
				lv_totalLengthFilled += lv_accesUnitLength;	

				// process AU till burst size + 1 AU
				ap_caller->accessUnitProcess ++;
				lv_sampleDuration = gf_isom_get_sample_duration(ap_mp4, ap_caller->trackNum, ap_caller->accessUnitProcess + 1);
					
				lv_ts = (u64) ((1000 * (s64) (ap_caller->accessUnit->DTS + ap_caller->accessUnit->CTS_Offset)) / lv_timescale);
				ap_caller->packetizer->sl_header.compositionTimeStamp = lv_ts;
								
				lv_ts = (u64) ((1000 * ap_caller->accessUnit->DTS) / lv_timescale);
				ap_caller->packetizer->sl_header.decodingTimeStamp = lv_ts;
									
				ap_caller->packetizer->sl_header.randomAccessPointFlag = ap_caller->accessUnit->IsRAP;
												
				gf_rtp_builder_process(ap_caller->packetizer, ap_caller->accessUnit->data, ap_caller->accessUnit->dataLength, 1, 
					ap_caller->accessUnit->dataLength, lv_sampleDuration, lv_sampleDescIndex);
				gf_isom_sample_del(&ap_caller->accessUnit);
			}
			else // forget
			{
				lv_totalLengthFilled += ap_caller->maxSizeBurst; // to quit the loop
			}
		} // end if exist sample
		else
		{
			lv_totalLengthFilled += ap_caller->maxSizeBurst;
		}
	} // end while
	
} /* paquetisation */

// ---------------------------------------------------------------------------------------------------

/*
 * configuration:
 *	retrieves the parameters from the configuration file
 *
 */

int configuration(Simulator *av_simulator, char *av_argv1)
{
	char lv_config[1024];		/* cfg file name */
	GF_Config *lp_configFile;	/* Configuration file struct */
	u32 i;						/* counter */

	sprintf(lv_config,"%s",av_argv1); // copy the path 

	fprintf(stdout, "Configuration file: %s \n", lv_config);

	lp_configFile = gf_cfg_new(PATHFILE,lv_config);	
	if(!lp_configFile)
	{
		fprintf(stderr, "ERROR: could not open the file\n");
		return GF_IO_ERR;
	}

	av_simulator->sov_nbSessions = atoi(gf_cfg_get_key(lp_configFile, "GLOBAL", "nbSession"));
	fprintf(stdout, " Number of sessions: %u \n", av_simulator->sov_nbSessions);

	av_simulator->sop_ip = gf_cfg_get_key(lp_configFile, "GLOBAL", "IP_dest");
	fprintf(stdout, " Destination IP: %s \n", av_simulator->sop_ip);	
	
	av_simulator->sov_delay_offtime = atoi(gf_cfg_get_key(lp_configFile, "GLOBAL", "delay_offtime"));
	fprintf(stdout, " Offtime duration: %u ms \n", av_simulator->sov_delay_offtime);

	av_simulator->sov_burstDuration = atoi(gf_cfg_get_key(lp_configFile, "GLOBAL", "burst_duration"));
	fprintf(stdout, " Burst duration: %u ms \n", av_simulator->sov_burstDuration);

	av_simulator->sov_peakBitRate = atoi(gf_cfg_get_key(lp_configFile, "GLOBAL", "peak_bit_rate"));
	fprintf(stdout, " Burst bit rate: %u kbps \n", av_simulator->sov_peakBitRate);

	// for each session:  cfg file
	av_simulator->sop_portTable = (u32 *)malloc(av_simulator->sov_nbSessions * sizeof(u32));
	av_simulator->sop_trackNbTable = (u32 *)malloc(av_simulator->sov_nbSessions * sizeof(u32));
	av_simulator->sop_fileTable = (char **)malloc(av_simulator->sov_nbSessions * sizeof(char *));
	av_simulator->sop_looping = (u32 *)malloc(av_simulator->sov_nbSessions * sizeof(u32));

	for(i=0; i<= av_simulator->sov_nbSessions - 1; i++)
	{
		char lv_sessionName[1024];

		sprintf(lv_sessionName, "SESSION%d",i+1);		
		av_simulator->sop_fileTable[i] = (char *) malloc(512);
				
		// retrieve parameters in cfg file
		av_simulator->sop_fileTable[i] = gf_cfg_get_key(lp_configFile, lv_sessionName, "file");		
		av_simulator->sop_portTable[i] = atoi(gf_cfg_get_key(lp_configFile, lv_sessionName, "port"));
		av_simulator->sop_trackNbTable[i] = atoi(gf_cfg_get_key(lp_configFile, lv_sessionName, "track_nb"));
		av_simulator->sop_looping[i] = atoi(gf_cfg_get_key(lp_configFile, lv_sessionName, "looping"));
		fprintf(stdout, " Session %u: File %s - Track %u - Destination Port %u \n", 
			i+1, av_simulator->sop_fileTable[i], av_simulator->sop_trackNbTable[i], av_simulator->sop_portTable[i]);
	}
	fprintf(stdout, "\n");

	return 1;
} /* configuration */

// ---------------------------------------------------------------------------------------------------

/*
 * timelineManagement
 *	called by a thread or in the main() for the 1st session
 *	1 thread <=> 1 session
 *
 */

GF_Err timelineManagement(void *ap_caller)
{
	s32 lv_sleepDuration;		/* in ms */
	u32 lv_currentTime;			/* in ms */
	u64 lv_totalLengthSent = 0;	/* in bits */
	u64 lv_mediaSize = 0;		/* in bits */

	RTP_Caller *lp_rtp = ap_caller;
	
	lv_mediaSize = gf_isom_get_media_data_size(lp_rtp->mp4File, lp_rtp->trackNum) *8 ; // size in bytes *8 = in bits	

	do {
		while(lp_rtp->accessUnitProcess <= lp_rtp->nbAccesUnit)		
		{
			lv_currentTime = gf_sys_clock();
			fprintf(stdout, "Time %u - ", lv_currentTime);
			lv_sleepDuration = lp_rtp->nextBurstTime - 1000 - lv_currentTime; // (can be negative)

			if(lv_sleepDuration <= 0) // begin the process now because sleepDuration is negative
			{
				paquetisation(lp_rtp, lp_rtp->mp4File);
			}
			else // positive
			{		
				gf_sleep(lv_sleepDuration);
				paquetisation(lp_rtp, lp_rtp->mp4File);
			}
			lv_totalLengthSent += lp_rtp->totalLengthSent;		
			if(lp_rtp->accessUnitProcess == lp_rtp->nbAccesUnit)
			{
				lp_rtp->accessUnitProcess += lp_rtp->nbAccesUnit;
			}
			
		}
		//reset parameters					
		lp_rtp->accessUnitProcess = 0; // number of the AU to process. set to 0 at the beginnning
		lp_rtp->nextBurstTime = gf_sys_clock() + lp_rtp->global->sov_burstDuration + lp_rtp->global->sov_delay_offtime;
		lp_rtp->dataLengthInBurst = 0; // total length filled in current burst
		lp_rtp->totalLengthSent = 0;   // total length sent to channel
		lp_rtp->nbAccesUnit = gf_isom_get_sample_count(lp_rtp->mp4File, lp_rtp->global->sop_trackNbTable[lp_rtp->id -1]);

	} while (lp_rtp->looping);

	return GF_OK;

} /* timelineManagement */

// ---------------------------------------------------------------------------------------------------

/*
 * checkConfig
 *	checks if the parameters the user has choosen
 *	matches with the play of the media
 */

GF_Err checkConfig(RTP_Caller *ap_caller)
{	
	u32 lv_burstDuration;	/* ms */
	u32 lv_delay_offtime;	/* ms */
	u32 lv_peakRate;		/* in kbps */
	u32 lv_physicalBitRate;	/* in kbps */
	u64 lv_mediaDuration;	/* in ms */
	u64 lv_mediasize;		/* in octet (bytes) --> to convert in bits*/
	u32 lv_calcul =0;
	u32 lv_trackNumber;		
	RTP_Caller *lp_caller= ap_caller;

	lv_burstDuration = lp_caller->global->sov_burstDuration; // ms
	lv_delay_offtime = lp_caller->global->sov_delay_offtime; // ms
	lv_peakRate = lp_caller->global->sov_peakBitRate; // kbps
	lv_physicalBitRate = (lv_burstDuration * lv_peakRate)/(lv_delay_offtime + lv_burstDuration); // kbps

	lv_trackNumber = lp_caller->global->sop_trackNbTable[lp_caller->id -1]; 
	lv_mediaDuration = gf_isom_get_track_duration(lp_caller->mp4File, lv_trackNumber)/gf_isom_get_timescale(lp_caller->mp4File)*1000; // ms
	lv_mediasize = gf_isom_get_media_data_size(lp_caller->mp4File, lv_trackNumber) * 8; // (gf_isom_get_media_data_size en octets)
	
	lp_caller->mediaSize = (u32)lv_mediasize; // in bits
	lp_caller->mediaDuration = (u32)lv_mediaDuration; // in ms
	lp_caller->mediaBitRate =  (u32)lv_mediasize / (u32)lv_mediaDuration ; // in bps
	lp_caller->maxSizeBurst = lp_caller->mediaBitRate * ((u32)lv_burstDuration + (u32)lv_delay_offtime ); // bps * ms ==> bits
	lp_caller->physicalBitRate = lv_physicalBitRate;
	lp_caller->looping = lp_caller->global->sop_looping[lp_caller->id -1];
	
	fprintf(stdout, " Session %u: "LLD" bits / "LLD" ms = %u kbps\n", lp_caller->id, lv_mediasize, lv_mediaDuration, lp_caller->mediaBitRate);
//	fprintf(stdout, "[INFO MEDIA] Session burst size %u bits - Global max bitrate %u (kbps) \n", lp_caller->maxSizeBurst, lp_caller->physicalBitRate);
		
	// condition: media bit rate <= max physical bit rate	
	if (lp_caller->physicalBitRate <= lp_caller->mediaBitRate) // comparison in kbps	
	{
		fprintf(stderr, "CFG WARNING: The media %u you will receive may not be play continuously\n", lp_caller->id);
		fprintf(stderr, " since you do not send enough data in a burst. Increase the peak bit rate\n");
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
	Simulator lv_global;		/* Simulator metadata (from cfg file) */
	RTP_Caller *lp_caller;		/* RTP Caller struct for each session */
	GF_Err e = GF_OK;			/* Error Message */	
	u32 i;						/* incrementation */
	u32 lv_run = 1;				/* 1 process running, 0 stopping process*/

	// process of cts parameters
	u32 *lp_sessionNumberToProcess;		// table of the session to process: 0 de not process, 1 process the AU
	u64 *lp_sessionNumberCTS;
	u64 lv_ctsMin = (u64)-1;
	u32 lv_countCTS = 0;				// count the number of session which have the same cts

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
	memset(&lv_global, 0, sizeof(Simulator));
				
	/*
	 * Configuration File
	 *
	 */ 		
	configuration(&lv_global, argv[1]);
	  
	/*
	 *	Allocation
	 *
	 */	
	lp_caller = (RTP_Caller *)malloc(lv_global.sov_nbSessions * sizeof(RTP_Caller));
	lp_sessionNumberToProcess = (u32 *)malloc(lv_global.sov_nbSessions * sizeof(u32));
	lp_sessionNumberCTS = (u64 *)malloc(lv_global.sov_nbSessions * sizeof(u64));

	for(i=0; i<= lv_global.sov_nbSessions - 1; i++)
	{	
		lp_sessionNumberToProcess[i] = 0;
		lp_sessionNumberCTS[i] = 100000;
		
		memset(&lp_caller[i], 0, sizeof(RTP_Caller)); /* Allocation */		
		lp_caller[i].global = &lv_global;		
		lp_caller[i].id = i+1;	
		lp_caller[i].accessUnitProcess = 0;
		lp_caller[i].nextBurstTime = 0;
		lp_caller[i].dataLengthInBurst = 0;
		lp_caller[i].totalLengthSent = 0;

		lp_caller[i].mp4File = gf_isom_open(lv_global.sop_fileTable[i], GF_ISOM_OPEN_READ, NULL);
		//fprintf(stdout, "file of session %u opened successfully\n", i);
		if (!lp_caller[i].mp4File)
		{
			fprintf(stderr, "ERROR: Could not open input MP4 file of session %u\n", i);
			return GF_IO_ERR;
		} 

		lp_caller[i].trackNum = lv_global.sop_trackNbTable[i];
	
		e = rtp_init_channel(&lp_caller[i].channel, lv_global.sop_ip, lv_global.sop_portTable[i]);
		if (e) 
		{
			fprintf(stderr, "Could not initialize RTP Channel\n");
		}			
		e = rtp_init_packetizer(&lp_caller[i]);
		if (e) 
		{
			fprintf(stderr, "Could not initialize Packetizer \n");
		}
		lp_caller[i].packets_to_send = gf_list_new();
		// lp_caller[i].accessUnit don't need to initialize
		// lp_caller[i].header
		// lp_caller[i].payload
		// lp_caller[i].payload_len
		// lp_caller[i].payload_valid_len
		lp_caller[i].thread = gf_th_new();	
	}
	  
	/*
	 * Check the configuration of the network
	 */

	// nb_service * burst duration <= off time
	if((lv_global.sov_nbSessions -1) * lv_global.sov_burstDuration > lv_global.sov_delay_offtime )
	{
		fprintf(stderr, "CFG ERROR: nb sessions * burst duration > off time\n");
		fprintf(stderr, "CFG ERROR: please reconfigure your cfg file\n");
		return GF_IO_ERR;
	}
	// burst 

	fprintf(stdout, "Media information:\n");
	for(i=0; i<= lv_global.sov_nbSessions -1; i++)
	{
		if (checkConfig(&lp_caller[i])) exit(-1);
	}
	fprintf(stdout, "=> Streaming is possible\n\n");

	fprintf(stdout, "Streaming information:\n");
	for(i=0; i<= lv_global.sov_nbSessions -1; i++)
	{
		if (lv_global.sop_trackNbTable[i] > gf_isom_get_track_count(lp_caller[i].mp4File)) 
		{
				fprintf(stderr, "ERROR: session %u: Track > track count \n", i);
				return GF_IO_ERR;			
		}				
		lp_caller[i].nbAccesUnit = gf_isom_get_sample_count(lp_caller[i].mp4File, lp_caller[i].global->sop_trackNbTable[i]);
	} /* end init */
		
	for(i=0; i< (lv_global.sov_nbSessions-1); i++) // except last session 
	{
		initTimeManagement(&lp_caller[i]);
		gf_th_run(lp_caller[i].thread, timelineManagement, &lp_caller[i] );
	}		
	initTimeManagement(&lp_caller[lv_global.sov_nbSessions-1]);
	timelineManagement(&lp_caller[lv_global.sov_nbSessions-1]);

	fprintf(stdout, "___________END THREAD MAIN__________wait 30 sec before closing_____\n");
	gf_sleep(30000); // pour éviter que les autres threads ne soient détruits.

	/*
	 * Desallocation 
	 *
	 */
	
	fprintf(stdout, "________________________desallocation_________________________\n");
	for(i=0; i<= lv_global.sov_nbSessions -1; i++)
	{
		free(lp_caller[i].payload);	
		gf_rtp_del(lp_caller[i].channel);
		gf_rtp_builder_del(lp_caller[i].packetizer);
		if (lp_caller[i].mp4File) gf_isom_close(lp_caller[i].mp4File);
	}	
	
	gf_sys_close();

	fprintf(stdout, "_________________________the end____________________________\n");
	return e;

} /* end main */
