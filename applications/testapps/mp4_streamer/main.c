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


//------------------------------------------------------------
// Define
//------------------------------------------------------------
#define PATHFILE "."
#define RTP_HEADER_SIZE 12 // in bytes (octets)

//------------------------------------------------------------
// Declaration of static variables
//------------------------------------------------------------
u32 sov_numberBurstSent = 0; // toutes sessions confondues
u32 sov_timeOffset = 0; // time when the first burts was sent. this time <=> (CTS=0)

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
	u32 ov_timeOrigin;			// origin of main.c ( = 0)

	// other param
	u32 sov_numberPacketSent;
	u32 sov_burstSize;
} Simulator;

typedef struct {
	GF_RTPHeader header;
	char *payload;
	u32 payload_len;
} RTP_Packet;

typedef struct {
	/* id */
	int id;
	u32 bitRate; // of the media
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
//
// Functions
//
//------------------------------------------------------------

/*
 * setOffsetTime
 *	sets the offset time, ie the time when the very first burst	was sent
 *	==> allows to syncrhonise the CTS time unit to the system time
 */

void setOffsetTime(u32 av_offsetTime)
{
	sov_timeOffset = av_offsetTime;
}

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
		ap_caller->nextBurstTime = sov_timeOffset +ap_caller->global->sov_burstDuration + ap_caller->global->sov_delay_offtime;		
	}
	else
	{
		ap_caller->nextBurstTime = sov_timeOffset + (ap_caller->id - 1)* ap_caller->global->sov_burstDuration;
	}
	//fprintf(stdout,"mp4 streamer: [initMgtTime] _id %u _currentTime %u _nextBurstTime %u\n",ap_caller->id, gf_sys_clock(),ap_caller->nextBurstTime);

}

/*
 * setNextBurstTime
 *	is called in OnPacketDone just before the burst is sent
 *	sets the nextBurstTime of the given session
 */

void setNextBurstTime(RTP_Caller *ap_caller)
{
	ap_caller->nextBurstTime = gf_sys_clock() +  ap_caller->global->sov_burstDuration + ap_caller->global->sov_delay_offtime;
	//fprintf(stdout,"mp4 streamer: [setNextBurstTime] _id %u _currentTime %u _nextBurstTime %u\n",ap_caller->id, gf_sys_clock(),ap_caller->nextBurstTime);
}


/*
 * callback functions
 *
 *
 */

static void OnNewPacket(void *cbk, GF_RTPHeader *header)
{
	RTP_Caller *caller = cbk;
	if (!header) return;
	memcpy(&caller->header, header, sizeof(GF_RTPHeader));
	//fprintf(stdout, "mp4 streamer: [ONP] _session %u\n", caller->id);
} /* OnNewPacket */

static void OnPacketDone(void *cbk, GF_RTPHeader *header) 
{
	RTP_Caller *caller = cbk;
	u32 lv_nextBurstTime;	// date du prochain envoie du burst normalement	
	u32 lv_sizeSent;		// taille totale des paquets envoyés pour le burst courant (bits)
	
	lv_sizeSent = 0;	
	lv_nextBurstTime = caller->nextBurstTime; // time of next burst to play	
	caller->global->ov_currentTime = gf_sys_clock();// gets system clock (current time)	
		
	//fprintf(stdout,"[OPD] _id %u _AU %u _currentTime %u _nextBursTime %u\n",caller->id, caller->accessUnitProcess, caller->global->ov_currentTime, lv_nextBurstTime);

	/*
	 * Check for the first burst sending
	 * ==> allows to set the time of the first sending and start threads
	 *
	 */

	if(sov_numberBurstSent == 0 && (caller->id == 1))
	{		
		u32 lv_totalLengthFilled =0; // in bits
		u32 i;
		u32 lv_packetCount=0;
		u32 lv_currentPacketSize; // in bits
		
		RTP_Packet *tmp;
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
			for (i=0;i< lv_packetCount;i++) 		
			{
				RTP_Packet *lp_buffer;
				lp_buffer = gf_list_get(caller->packets_to_send, i);
				lv_totalLengthFilled += (lp_buffer->payload_len + RTP_HEADER_SIZE) * 8;
			} 
			lv_totalLengthFilled += lv_currentPacketSize;
		}
		
		if( lv_totalLengthFilled >= caller->maxSizeBurst ) 
		{ // send the RTP packets in gf_list
			setOffsetTime(caller->global->ov_currentTime ); /* SPECIFIC TO THE FIRST SESSION */
			if(lv_packetCount == 0) // hardly ever happens...
			{
				caller->totalLengthSent += (tmp->payload_len * 8); 
				gf_rtp_send_packet(caller->channel, &tmp->header, 0, 0, tmp->payload, tmp->payload_len);
				free(tmp->payload);				
			}
			else
			{
				for (i=0;i<= lv_packetCount -1 ;i++) 
				{
					RTP_Packet *rtp = gf_list_get(caller->packets_to_send, i);
					caller->totalLengthSent += (rtp->payload_len * 8) ; // update the total length
					gf_rtp_send_packet(caller->channel, &rtp->header, 0, 0, rtp->payload, rtp->payload_len);				
					free(rtp->payload);
				}			
				gf_list_reset(caller->packets_to_send);
			}
			sov_numberBurstSent++;			
			fprintf(stdout, "[OPD SEN] 1st burstSent _pktSize %u _totSize %u _nbBurstSent %u _offtime %u\n", lv_currentPacketSize, lv_totalLengthFilled, sov_numberBurstSent, gf_sys_clock() );
		
		} // end if		  
		else // store
		{
			//fprintf(stdout, "[OPD STO] 1st rtp store _pktSize %u _totSize %u\n", lv_currentPacketSize, lv_totalLengthFilled);
		}
		gf_list_add(caller->packets_to_send, tmp); // store the last packet not sent
						
	} // end if first packet of session n°1
	else
	{
		u32 lv_totalLengthFilled =0; // in bits
		u32 i;
		u32 lv_packetCount=0;
		u32 lv_currentPacketSize; // in bits
		
		RTP_Packet *tmp;
		GF_SAFEALLOC(tmp, RTP_Packet);
		memcpy(&tmp->header, header, sizeof(GF_RTPHeader));
		tmp->payload = caller->payload;
		caller->payload = NULL;
		tmp->payload_len = caller->payload_len;
		caller->payload_len = 0;
		caller->payload_valid_len = 0;		
		
		lv_packetCount = gf_list_count(caller->packets_to_send);	
		lv_currentPacketSize = (tmp->payload_len + RTP_HEADER_SIZE )*8;
		if(lv_packetCount == 0)
		{
			lv_totalLengthFilled += lv_currentPacketSize;
		}
		else
		{
			for (i=0;i< lv_packetCount;i++) 		
			{
				RTP_Packet *lp_buffer;
				lp_buffer = gf_list_get(caller->packets_to_send, i);
				lv_totalLengthFilled += (lp_buffer->payload_len + RTP_HEADER_SIZE)*8 ;
			} 
			lv_totalLengthFilled += lv_currentPacketSize;
		}		
				
		if( lv_totalLengthFilled >= caller->maxSizeBurst ) 
		{ // send the RTP packets in gf_list
			u32 lv_sleep;
			
			lv_sleep = caller->nextBurstTime - caller->global->ov_currentTime;
			if ((int)lv_sleep <0)
			{
				lv_sleep = 0;
				fprintf(stdout,"mp4 streamer: ERROR sleep <0: nextBurstTime %u CurrentTime %u Offset %u\n",caller->nextBurstTime, caller->global->ov_currentTime, sov_timeOffset  );
			}
			
			gf_sleep(lv_sleep); // sleep and send the burst
			
			setNextBurstTime(caller); // time management
			
			if(lv_packetCount == 0)
			{
				caller->totalLengthSent += (tmp->payload_len)*8; 
				gf_rtp_send_packet(caller->channel, &tmp->header, 0, 0, tmp->payload, tmp->payload_len);
				free(tmp->payload);				
			}
			else
			{
				for (i=0;i<= lv_packetCount -1 ;i++) 
				{			
					RTP_Packet *rtp = gf_list_get(caller->packets_to_send, i);
					caller->totalLengthSent += (rtp->payload_len)*8; // update the total length					
					gf_rtp_send_packet(caller->channel, &rtp->header, 0, 0, rtp->payload, rtp->payload_len);
					free(rtp->payload);	
				}			
				gf_list_reset(caller->packets_to_send);
			}
			sov_numberBurstSent++;
			fprintf(stdout, "[OPD SEN] _id %u _pktSize %u _totSize %u _nbBurstSent %u _nbPkt in Burst %u\n", caller->id, lv_currentPacketSize, lv_totalLengthFilled, sov_numberBurstSent,lv_packetCount);
	
		} // end if		  
		else // store
		{
			//fprintf(stdout, "[OPD STO] _id %u _pktSize %u _totSize %u _nbPkt in Burst %u \n", caller->id, lv_currentPacketSize, lv_totalLengthFilled, lv_packetCount);
		}
		gf_list_add(caller->packets_to_send, tmp); // store the last packet not sent
	}

} /* OnPacketdone */

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

static u32 payload_type = 96;

GF_Err rtp_init_packetizer(RTP_Caller *caller)
{	
	char sdp_fmt[5000];

	caller->packetizer = gf_rtp_builder_new(GP_RTP_PAYT_MPEG4, NULL, GP_RTP_PCK_SIGNAL_RAP, caller, OnNewPacket, OnPacketDone, NULL, OnData);
	gf_rtp_builder_init(caller->packetizer, payload_type, 1500, 0, 
		/*stream type*/5, /*objecttypeindication */107, 
		0, 0, 0, 0, 0, 0, 0, NULL); 
	payload_type++;

	/* regarder hint_track.c pour créer un SDP entier */
	gf_rtp_builder_format_sdp(caller->packetizer, "mpeg4-generic", sdp_fmt, NULL, 0);
	//fprintf(stdout, "init_packetizer [SDP]: %s\n", sdp_fmt);

	caller->packetizer->rtp_header.Version = 2;
	caller->packetizer->rtp_header.SSRC = rand();//RandomNumber identifiant la source

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
	int lv_idSession;	
	u32 lv_accesUnitLength;	// in bits
	u32 lv_totalLengthFilled; // in bits
	u32 lv_timescale;
	u32 lv_sampleDuration;
	u8 lv_sampleDescIndex;
	u64 lv_ts;
	u32 lv_auCount;
		
	lv_accesUnitLength = 0;
	lv_totalLengthFilled = 0;
	lv_idSession = ap_caller->id;
	lv_timescale = gf_isom_get_media_timescale(ap_mp4, ap_caller->global->sop_trackNbTable[lv_idSession-1]);
	lv_auCount= gf_isom_get_sample_count(ap_mp4, ap_caller->global->sop_trackNbTable[lv_idSession-1]);
		
	while (lv_totalLengthFilled <= ap_caller->maxSizeBurst)
	{			
		if(ap_caller->accessUnitProcess + 1 <= lv_auCount) // attention overflow acceUnit number <= get_sample_count
		{
			ap_caller->accessUnit = gf_isom_get_sample(ap_mp4, ap_caller->global->sop_trackNbTable[lv_idSession-1], ap_caller->accessUnitProcess + 1, &lv_sampleDescIndex);			
			lv_accesUnitLength =  ap_caller->accessUnit->dataLength * 8;
			
			/*
			// solution 1
			if( (lv_totalLengthFilled + lv_accesUnitLength) <= ap_caller->maxSizeBurst ) // OK
			{
				lv_totalLengthFilled += lv_accesUnitLength;									
			}
			else // forget
			{
				lv_totalLengthFilled += ap_caller->maxSizeBurst; // to quit the loop
			}

			// process AU till burst size + 1 AU
			ap_caller->accessUnitProcess ++;
			lv_sampleDuration = gf_isom_get_sample_duration(ap_mp4, ap_caller->global->sop_trackNbTable[lv_idSession-1], ap_caller->accessUnitProcess + 1);
				
			lv_ts = (u64) ((90000 * (s64) (ap_caller->accessUnit->DTS + ap_caller->accessUnit->CTS_Offset)) / lv_timescale);
			ap_caller->packetizer->sl_header.compositionTimeStamp = lv_ts;
							
			lv_ts = (u64) ((90000 * ap_caller->accessUnit->DTS) / lv_timescale);
			ap_caller->packetizer->sl_header.decodingTimeStamp = lv_ts;
								
			ap_caller->packetizer->sl_header.randomAccessPointFlag = ap_caller->accessUnit->IsRAP;
											
			gf_rtp_builder_process(ap_caller->packetizer, ap_caller->accessUnit->data, ap_caller->accessUnit->dataLength, 1, 
				ap_caller->accessUnit->dataLength, lv_sampleDuration, lv_sampleDescIndex);
			gf_isom_sample_del(&ap_caller->accessUnit);
			*/
			if( (lv_totalLengthFilled + lv_accesUnitLength) <= ap_caller->maxSizeBurst ) // OK
			{
				lv_totalLengthFilled += lv_accesUnitLength;	

				// process AU till burst size + 1 AU
				ap_caller->accessUnitProcess ++;
				lv_sampleDuration = gf_isom_get_sample_duration(ap_mp4, ap_caller->global->sop_trackNbTable[lv_idSession-1], ap_caller->accessUnitProcess + 1);
					
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

	//fprintf(stdout, "*********************************END PAQUETISATION Session %u* accesunit process %u********************************\n", lv_idSession, ap_caller->accessUnitProcess );
	
} /* paquetisation */

// ---------------------------------------------------------------------------------------------------

/*
 * configuration:
 *	retrieves the parameters in the configuration file
 *
 */

int configuration(Simulator *av_simulator, char *av_argv1)
{
	char lv_config[1024];		/* cfg file name */
	GF_Config *lp_configFile;	/* Configuration file struct */
	u32 i;						/* counter */

	sprintf(lv_config,"%s",av_argv1); // copy the path 
	fprintf(stdout, "[configuration]---------------read the param---------------\n");
	fprintf(stdout, "[configuration] file name: %s \n", lv_config);

	lp_configFile = gf_cfg_new(PATHFILE,lv_config);	
	if(!lp_configFile)
	{
		fprintf(stderr, "mp4 streamer ERROR: could not open the file\n");
		return GF_IO_ERR;
	}

	av_simulator->sov_nbSessions = gf_cfg_get_section_count(lp_configFile) -1;
	fprintf(stdout, "[configuration] nbSessions du fichier de config = %u \n", av_simulator->sov_nbSessions);

	av_simulator->sop_ip = gf_cfg_get_key(lp_configFile, "GLOBAL", "IP_dest");
	fprintf(stdout, "[configuration] IP destinataire= %s \n", av_simulator->sop_ip);	
	
	av_simulator->sov_delay_offtime = atoi(gf_cfg_get_key(lp_configFile, "GLOBAL", "delay_offtime"));
	fprintf(stdout, "[configuration] delay offtime = %u ms \n", av_simulator->sov_delay_offtime);

	av_simulator->sov_burstDuration = atoi(gf_cfg_get_key(lp_configFile, "GLOBAL", "burst_duration"));
	fprintf(stdout, "[configuration] burst duration= %u ms \n", av_simulator->sov_burstDuration);

	av_simulator->sov_peakBitRate = atoi(gf_cfg_get_key(lp_configFile, "GLOBAL", "peak_bit_rate"));
	fprintf(stdout, "[configuration] peak bit rate= %u kbps \n", av_simulator->sov_peakBitRate);

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
		fprintf(stdout, "[configuration] _id %u _port %u _file %s _track %u \n", 
			i+1, av_simulator->sop_portTable[i], av_simulator->sop_fileTable[i], av_simulator->sop_trackNbTable[i]);
	}

	av_simulator->sov_numberPacketSent = 0;
	av_simulator->sov_burstSize = av_simulator->sov_burstDuration * av_simulator->sov_peakBitRate; // size in bit
	return 1;
} /* configuration */

// ---------------------------------------------------------------------------------------------------

/*
 * timelineManagement
 *	called by a thread or in the main() for the 1st session
 *	1 thread <=> 1 session
 *
 */

void timelineManagement(void *ap_caller)
{
	u32 lv_sleepDuration;		/* in ms */
	int lv_sleepDurationInt;	/* in ms */
	u32 lv_currentTime;			/* in ms */
	u64 lv_totalLengthSent = 0;	/* in bits */
	u64 lv_mediaSize = 0;		/* in bits */
	u32 lv_idSession;	

	RTP_Caller *lp_rtp = ap_caller;
	
	// gets the size of the media
	lv_idSession = lp_rtp->id;	
	lv_mediaSize = gf_isom_get_media_data_size(lp_rtp->mp4File, lp_rtp->global->sop_trackNbTable[lv_idSession-1]) *8 ; // size in bytes *8 = in bits	
	//fprintf(stdout, "mp4 streamer: id %u, length of the media: "LLD" bits trackBb %u\n", lv_idSession,  lv_mediaSize,lp_rtp->global->sop_trackNbTable[lv_idSession-1] );


	/*
	 * Normal use
	 *
	 */

	if(lp_rtp->looping != 1) // no looping
	{
		while((int)(lp_rtp->accessUnitProcess) <= (int)(lp_rtp->nbAccesUnit))
		//while(lv_totalLengthSent <= lv_mediaSize) // --> does not work... stops the sending without any reasons
		{
			fprintf(stdout, "[MANAGEMENT TIMELINE]__________________________________\n");
			fprintf(stdout, "[timeMgt] _id %u _AU %u \n", lp_rtp->id, lp_rtp->accessUnitProcess);

			lv_currentTime = gf_sys_clock();
			lv_sleepDuration = lp_rtp->nextBurstTime - 1000 - lv_currentTime; // (can be negative)
			lv_sleepDurationInt = (int) lv_sleepDuration;

			if(lv_sleepDurationInt <= 0) // begin the process now because sleepDuration is negative
			{
				//fprintf(stdout, "mp4 streamer: [timeMgt] pkt _id %u _nextBT %u _currentTime %u \n",lp_rtp->id, lp_rtp->nextBurstTime, lv_currentTime);
				paquetisation(lp_rtp, lp_rtp->mp4File);
			}
			else // positive
			{		
				lv_sleepDuration = (u32) lv_sleepDurationInt;
				//fprintf(stdout,"mp4 streamer: [timeMgt] sleep _id %u _dur %u _nextBT %u _currentTime %u\n",lp_rtp->id, lv_sleepDuration, lp_rtp->nextBurstTime, lv_currentTime);
				gf_sleep(lv_sleepDuration);
				paquetisation(lp_rtp, lp_rtp->mp4File);
			}
			lv_totalLengthSent += lp_rtp->totalLengthSent;		
			fprintf(stdout, "[timeMgt] _id %u _totalLengthSent "LLD"\n",lp_rtp->id,lv_totalLengthSent );		
		}
	} // end if no looping
	else
	{
		/*
		 * Infinite loop ==> to send the media "en boucle"
		 *
		 */

		while(1)
		{
			while((int)(lp_rtp->accessUnitProcess) <= (int)(lp_rtp->nbAccesUnit))		
			{
				//fprintf(stdout, "[MANAGEMENT TIMELINE]__________________________________\n");
				//fprintf(stdout, "[timeMgt] _id %u _AU %u \n", lp_rtp->id, lp_rtp->accessUnitProcess);

				lv_currentTime = gf_sys_clock();
				lv_sleepDuration = lp_rtp->nextBurstTime - 1000 - lv_currentTime; // (can be negative)
				lv_sleepDurationInt = (int) lv_sleepDuration;

				if(lv_sleepDurationInt <= 0) // begin the process now because sleepDuration is negative
				{
					//fprintf(stdout, "mp4 streamer: [timeMgt] pkt _id %u _nextBT %u _currentTime %u \n",lp_rtp->id, lp_rtp->nextBurstTime, lv_currentTime);
					paquetisation(lp_rtp, lp_rtp->mp4File);
				}
				else // positive
				{		
					lv_sleepDuration = (u32) lv_sleepDurationInt;
					//fprintf(stdout,"mp4 streamer: [timeMgt] sleep _id %u _dur %u _nextBT %u _currentTime %u\n",lp_rtp->id, lv_sleepDuration, lp_rtp->nextBurstTime, lv_currentTime);
					gf_sleep(lv_sleepDuration);
					paquetisation(lp_rtp, lp_rtp->mp4File);
				}
				lv_totalLengthSent += lp_rtp->totalLengthSent;		
				if(lp_rtp->accessUnitProcess == lp_rtp->nbAccesUnit)
				{
					lp_rtp->accessUnitProcess += lp_rtp->nbAccesUnit;
				}
				
				//fprintf(stdout, "[timeMgt] _id %u _totalLengthSent "LLD"\n",lp_rtp->id,lv_totalLengthSent );		
			}
			//reset parameters					
			lp_rtp->accessUnitProcess = 0; // number of the AU to process. set to 0 at the beginnning
			lp_rtp->nextBurstTime = gf_sys_clock() + lp_rtp->global->sov_burstDuration + lp_rtp->global->sov_delay_offtime;
			lp_rtp->dataLengthInBurst = 0; // total length filled in current burst
			lp_rtp->totalLengthSent = 0;   // total length sent to channel
			lp_rtp->nbAccesUnit = gf_isom_get_sample_count(lp_rtp->mp4File, lp_rtp->global->sop_trackNbTable[lp_rtp->id -1]);
		} // end while 1
	} // end else looping

} /* timelineManagement */

// ---------------------------------------------------------------------------------------------------

/*
 * checkConfig
 *	checks if the parameters the user has choosen
 *	matches with the play of the media
 */

int checkConfig(RTP_Caller *ap_caller)
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
	lv_mediaDuration = gf_isom_get_track_duration(lp_caller->mp4File, lv_trackNumber); // 
	lv_mediasize = gf_isom_get_media_data_size(lp_caller->mp4File, lv_trackNumber) * 8; // (gf_isom_get_media_data_size en octets)
	
	lp_caller->mediaSize = (u32)lv_mediasize; // in bits
	lp_caller->mediaDuration = (u32)lv_mediaDuration; // in ms
	lp_caller->bitRate =  (u32)lv_mediasize / (u32)lv_mediaDuration ; // in bps
	lp_caller->maxSizeBurst = lp_caller->bitRate * ((u32)lv_burstDuration + (u32)lv_delay_offtime ); // bps * ms ==> bits
	//lp_caller->maxSizeBurst = lp_caller->bitRate * ((u32)lv_burstDuration); // bps * ms ==> bits
	lp_caller->physicalBitRate = lv_physicalBitRate;
	lp_caller->looping = lp_caller->global->sop_looping[lp_caller->id -1];
	
	fprintf(stdout, "[INFO MEDIA] id number %u \n", lp_caller->id);
	//fprintf(stdout, "[INFO MEDIA: burst duration %u (ms) offtime %u (ms) peak rate %u (kbps)\n", lv_burstDuration, lv_delay_offtime, lv_peakRate);
	fprintf(stdout, "[INFO MEDIA] media duration "LLD" (ms) media size "LLD" (bits) max burst size %u (bits) \n",lv_mediaDuration, lv_mediasize, lp_caller->maxSizeBurst);
	fprintf(stdout, "[INFO MEDIA] compute: mediaBitRate %u (kbps) physical max bitrate %u (kbps) \n", lp_caller->bitRate, lv_physicalBitRate );
		
	// condition: media bit rate <= max physical bit rate	
	if(lv_physicalBitRate <= lp_caller->bitRate ) // comparison in kbps	
	{
		fprintf(stderr, "mp4 streamer: CFG WARNING: The media %u you will receive may not be play continuously\n", lp_caller->id);
		fprintf(stderr, " since you do not send enough data in a burst. Increase the peak bit rate\n");
		fprintf(stderr, " Please reconfigure your cfg file\n");
		return GF_IO_ERR;
	}
	
	return 1;

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

	fprintf(stdout, "/****************************************************** \n");
	fprintf(stdout, " * \n");
	fprintf(stdout, " * MP 4 streamer Programm\n");
	fprintf(stdout, " * Project: MIX 2006\n");
	fprintf(stdout, " * Students: Anh-Vu BUI and Xiangfeng LIU\n");
	fprintf(stdout, " * \n");
	fprintf(stdout, " *****************************************************/ \n");
	/*
	 * Initialization
	 *
	 */
	gf_sys_init();				/* Init */
	memset(&lv_global, 0, sizeof(Simulator));
	lv_global.ov_timeOrigin = gf_sys_clock();
				
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
		lp_caller[i].id = i+1;	
		lp_caller[i].accessUnitProcess = 0;
		lp_caller[i].nextBurstTime = 0;
		lp_caller[i].dataLengthInBurst = 0;
		lp_caller[i].totalLengthSent = 0;
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
		lp_caller[i].global = &lv_global;		
		lp_caller[i].thread = gf_th_new();	
	}

	/*
	 * read the mp4 files
	 */
	fprintf(stdout, "mp4 streamer: _____________________read mp 4 files_______________________ \n");
	for(i=0; i<= lv_global.sov_nbSessions -1; i++)
	{		
		lp_caller[i].mp4File = gf_isom_open(lv_global.sop_fileTable[i], GF_ISOM_OPEN_READ, NULL);
		//fprintf(stdout, "mp4 streamer: file of session %u opened successfully\n", i);
		if (!lp_caller[i].mp4File)
		{
			fprintf(stderr, "mp4 streamer ERROR: Could not open input MP4 file of session %u\n", i);
			return GF_IO_ERR;
		} 
	} /* end read file*/

	/*
	 * Check the configuration of the network
	 */

	fprintf(stdout, "mp4 streamer: _______________________check config________________________ \n");
	// nb_service * burst duration <= off time
	if(lv_global.sov_nbSessions * lv_global.sov_burstDuration > lv_global.sov_delay_offtime )
	{
		fprintf(stderr, "mp4 streamer CFG ERROR: nb service * burst duration > off time\n");
		fprintf(stderr, "mp4 streamer CFG ERROR: please reconfigure your cfg file\n");
		return GF_IO_ERR;
	}
	// burst 
	for(i=0; i<= lv_global.sov_nbSessions -1; i++)
	{
		checkConfig(&lp_caller[i]);
	}
	
	for(i=0; i<= lv_global.sov_nbSessions -1; i++)
	{
		if (lv_global.sop_trackNbTable[i] > gf_isom_get_track_count(lp_caller[i].mp4File)) 
		{
				fprintf(stderr, "mp4 streamer ERROR: session %u: Track > track count \n", i);
				return GF_IO_ERR;			
		}				
		lp_caller[i].nbAccesUnit = gf_isom_get_sample_count(lp_caller[i].mp4File, lp_caller[i].global->sop_trackNbTable[i]);
	} /* end init */
		
	fprintf(stdout, "[MANAGEMENT TIMELINE]_FIRST_SESSION_________________________\n");
	fprintf(stdout, "[timeMgt] _id %u _AU %u \n", lp_caller[0].id, lp_caller[0].accessUnitProcess);
	paquetisation(&lp_caller[0], lp_caller[0].mp4File);// 0 =>first session		

	//fprintf(stdout, "[MANAGEMENT TIMELINE]_NEXT_SESSION_________________________\n");
	for(i=1; i<= lv_global.sov_nbSessions -1; i++) // except session 1
	{
		initTimeManagement(&lp_caller[i]);
		gf_th_run(lp_caller[i].thread, timelineManagement, &lp_caller[i] );
	}		
	initTimeManagement(&lp_caller[0]);
	timelineManagement(&lp_caller[0]);

	fprintf(stdout, "___________END THREAD MAIN__________wait 30 sec before closing_____\n");
	gf_sleep(30000); // pour éviter que les autres threads ne soient détruits.

	/*
	 * Desallocation 
	 *
	 */
	
	fprintf(stdout, "mp4 streamer: ________________________desallocation_________________________\n");
	for(i=0; i<= lv_global.sov_nbSessions -1; i++)
	{
		//fprintf(stdout, "mp4 streamer: desallocation 1\n");
		free(lp_caller[i].payload);	
		
		//fprintf(stdout, "mp4 streamer: desallocation 2\n");
		gf_list_del(lp_caller[i].payload);

		//fprintf(stdout, "mp4 streamer: desallocation 3\n");
		gf_rtp_del(lp_caller[i].channel);

		//fprintf(stdout, "mp4 streamer: desallocation 4\n");
		gf_rtp_builder_del(lp_caller[i].packetizer);

		//fprintf(stdout, "mp4 streamer: desallocation 5\n");
		if (lp_caller[i].mp4File) gf_isom_close(lp_caller[i].mp4File);
	}	
	
	gf_sys_close();

	fprintf(stdout, "mp4 streamer: _________________________the end____________________________\n");
	return e;

} /* end main */
