/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean le Feuvre
 *				Copyright (c) 2005-200X ENST
 *					All rights reserved
 *
 *  This file is part of GPAC / M2TS reader module
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
#include <gpac/mpegts.h>
#include <gpac/thread.h>
#include <gpac/network.h>
#include <gpac/constants.h>

#ifdef GPAC_HAS_LINUX_DVB
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>

typedef struct {
	u32 freq;
	u16 vpid;
	u16 apid; 
	fe_spectral_inversion_t specInv;
	fe_modulation_t modulation;
	fe_bandwidth_t bandwidth;
	fe_transmit_mode_t TransmissionMode;
	fe_guard_interval_t guardInterval;
	fe_code_rate_t HP_CodeRate;
	fe_code_rate_t LP_CodeRate;
	fe_hierarchy_t hierarchy;

	int ts_fd;
} GF_Tuner;

#define DVB_BUFFER_SIZE 3760							// DVB buffer size 188x20

#endif

#define UDP_BUFFER_SIZE	0x40000

typedef struct {
	char *fragment;
	u32 id;
	/*if only pid is requested*/
	u32 pid;
} M2TSIn_Prog;

typedef struct
{
	GF_ClientService *service;
	GF_M2TS_Demuxer *ts;

	Bool request_all_pids;
	GF_List *requested_progs;
	GF_List *requested_pids;

	/*demuxer thread*/
	GF_Thread *th;
	u32 run_state;
	GF_Mutex *mx;
	/*net playing*/
	GF_Socket *sock;
	
#ifdef GPAC_HAS_LINUX_DVB
	/*dvb playing*/
	GF_Tuner *tuner;
#endif	
	/*local file playing*/
	FILE *file;
	char filename[GF_MAX_PATH];
	u32 start_range, end_range;
	u32 file_size;
	Double duration;
	u32 nb_playing;
	Bool file_regulate;
	u64 pcr_last;
	u32 stb_at_last_pcr;
	u32 nb_pck;

	Bool epg_requested;
	Bool has_eit;
	LPNETCHANNEL eit_channel;
} M2TSIn;

#define M2TS_BUFFER_MAX 400


#ifdef GPAC_HAS_LINUX_DVB

static GF_Err gf_dvb_tune(GF_Tuner *tuner, char *url, const char *chan_path) {
	struct dmx_pes_filter_params pesFilterParams;
	struct dvb_frontend_parameters frp;
	int demux1, front1;
	FILE *chanfile;
	char line[255], chan_name_t[255];
	char freq_str[255], inv[255], bw[255], lcr[255], hier[255], cr[255], 
		 mod[255], transm[255], gi[255], apid_str[255], vpid_str[255];
	const char *chan_conf = ":%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:";
	char *chan_name;
	char *tmp;
	char frontend_name[100], demux_name[100], dvr_name[100];
	u32 adapter_num;

	chanfile = fopen(chan_path, "r");
	if (!chanfile) return GF_BAD_PARAM;

	chan_name = url+6; // 6 = strlen("dvb://")
	
	// support for multiple frontends
	tmp = strchr(chan_name, '@');
	if (tmp) {
		adapter_num = atoi(tmp+1);
		tmp[0] = 0;
	} else {
		adapter_num = 0;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("Channel name %s\n", chan_name));

	while(!feof(chanfile)) {
		if ( fgets(line, 255, chanfile) != NULL) {
			if (line[0]=='#') continue;
			if (line[0]=='\r') continue;
			if (line[0]=='\n') continue;

			strncpy(chan_name_t, line, index(line, ':')-line); 
			if (strncmp(chan_name,chan_name_t,strlen(chan_name))==0) {
				sscanf(strstr(line,":"), chan_conf, freq_str, inv, bw, lcr, cr, mod, transm, gi, hier, apid_str, vpid_str);
				tuner->freq = (uint32_t) atoi(freq_str);
				tuner->apid = (uint16_t) atoi(apid_str);
				tuner->vpid = (uint16_t) atoi(vpid_str);
				//Inversion
				if(! strcmp(inv, "INVERSION_ON")) tuner->specInv = INVERSION_ON;
				else if(! strcmp(inv, "INVERSION_OFF")) tuner->specInv = INVERSION_OFF;
				else tuner->specInv = INVERSION_AUTO;
				//LP Code Rate
				if(! strcmp(lcr, "FEC_1_2")) tuner->LP_CodeRate =FEC_1_2;
				else if(! strcmp(lcr, "FEC_2_3")) tuner->LP_CodeRate =FEC_2_3;
				else if(! strcmp(lcr, "FEC_3_4")) tuner->LP_CodeRate =FEC_3_4;
				else if(! strcmp(lcr, "FEC_4_5")) tuner->LP_CodeRate =FEC_4_5;
				else if(! strcmp(lcr, "FEC_6_7")) tuner->LP_CodeRate =FEC_6_7;
				else if(! strcmp(lcr, "FEC_8_9")) tuner->LP_CodeRate =FEC_8_9;
				else if(! strcmp(lcr, "FEC_5_6")) tuner->LP_CodeRate =FEC_5_6;
				else if(! strcmp(lcr, "FEC_7_8")) tuner->LP_CodeRate =FEC_7_8;
				else if(! strcmp(lcr, "FEC_NONE")) tuner->LP_CodeRate =FEC_NONE;
				else tuner->LP_CodeRate =FEC_AUTO;
				//HP Code Rate
				if(! strcmp(cr, "FEC_1_2")) tuner->HP_CodeRate =FEC_1_2;
				else if(! strcmp(cr, "FEC_2_3")) tuner->HP_CodeRate =FEC_2_3;
				else if(! strcmp(cr, "FEC_3_4")) tuner->HP_CodeRate =FEC_3_4;
				else if(! strcmp(cr, "FEC_4_5")) tuner->HP_CodeRate =FEC_4_5;
				else if(! strcmp(cr, "FEC_6_7")) tuner->HP_CodeRate =FEC_6_7;
				else if(! strcmp(cr, "FEC_8_9")) tuner->HP_CodeRate =FEC_8_9;
				else if(! strcmp(cr, "FEC_5_6")) tuner->HP_CodeRate =FEC_5_6;
				else if(! strcmp(cr, "FEC_7_8")) tuner->HP_CodeRate =FEC_7_8;
				else if(! strcmp(cr, "FEC_NONE")) tuner->HP_CodeRate =FEC_NONE;
				else tuner->HP_CodeRate =FEC_AUTO;
				//Modulation			
				if(! strcmp(mod, "QAM_128")) tuner->modulation = QAM_128;
				else if(! strcmp(mod, "QAM_256")) tuner->modulation = QAM_256;
				else if(! strcmp(mod, "QAM_64")) tuner->modulation = QAM_64;
				else if(! strcmp(mod, "QAM_32")) tuner->modulation = QAM_32;
				else if(! strcmp(mod, "QAM_16")) tuner->modulation = QAM_16;
				//Bandwidth				
				if(! strcmp(bw, "BANDWIDTH_6_MHZ")) tuner->bandwidth = BANDWIDTH_6_MHZ;
				else if(! strcmp(bw, "BANDWIDTH_7_MHZ")) tuner->bandwidth = BANDWIDTH_7_MHZ;
				else if(! strcmp(bw, "BANDWIDTH_8_MHZ")) tuner->bandwidth = BANDWIDTH_8_MHZ;
				//Transmission Mode
				if(! strcmp(transm, "TRANSMISSION_MODE_2K")) tuner->TransmissionMode = TRANSMISSION_MODE_2K;
				else if(! strcmp(transm, "TRANSMISSION_MODE_8K")) tuner->TransmissionMode = TRANSMISSION_MODE_8K;
				//Guard Interval
				if(! strcmp(gi, "GUARD_INTERVAL_1_32")) tuner->guardInterval = GUARD_INTERVAL_1_32;
				else if(! strcmp(gi, "GUARD_INTERVAL_1_16")) tuner->guardInterval = GUARD_INTERVAL_1_16;
				else if(! strcmp(gi, "GUARD_INTERVAL_1_8")) tuner->guardInterval = GUARD_INTERVAL_1_8;
				else tuner->guardInterval = GUARD_INTERVAL_1_4;
				//Hierarchy			
				if(! strcmp(hier, "HIERARCHY_1")) tuner->hierarchy = HIERARCHY_1;
				else if(! strcmp(hier, "HIERARCHY_2")) tuner->hierarchy = HIERARCHY_2;
				else if(! strcmp(hier, "HIERARCHY_4")) tuner->hierarchy = HIERARCHY_4;				
				else if(! strcmp(hier, "HIERARCHY_AUTO")) tuner->hierarchy = HIERARCHY_AUTO;
				else tuner->hierarchy = HIERARCHY_NONE;
				
				break;
			}
		}
	}
	fclose(chanfile);
        
	sprintf(frontend_name, "/dev/dvb/adapter%d/frontend0", adapter_num);
	sprintf(demux_name, "/dev/dvb/adapter%d/demux0", adapter_num);
	sprintf(dvr_name, "/dev/dvb/adapter%d/dvr0", adapter_num);

	// Open frontend
	if((front1 = open(frontend_name,O_RDWR|O_NONBLOCK)) < 0){
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Cannot open frontend %s.\n", frontend_name));
  		return GF_IO_ERR;
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("Frontend %s opened.\n", frontend_name));
	}
	// Open demuxes
	if ((demux1=open(demux_name, O_RDWR|O_NONBLOCK)) < 0){
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Cannot open demux %s\n", demux_name));
  		return GF_IO_ERR;
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Demux %s opened.\n", demux_name));
	}
	// Set FrontendParameters - DVB-T
	frp.frequency = tuner->freq;
	frp.inversion = tuner->specInv;
	frp.u.ofdm.bandwidth = tuner->bandwidth;
	frp.u.ofdm.code_rate_HP = tuner->HP_CodeRate;
	frp.u.ofdm.code_rate_LP = tuner->LP_CodeRate;
	frp.u.ofdm.constellation = tuner->modulation;
	frp.u.ofdm.transmission_mode = tuner->TransmissionMode;
	frp.u.ofdm.guard_interval = tuner->guardInterval;
	frp.u.ofdm.hierarchy_information = tuner->hierarchy;
	// Set frontend
	if (ioctl(front1, FE_SET_FRONTEND, &frp) < 0){
   		return GF_IO_ERR;
	}
	// Set dumex
	pesFilterParams.pid      = 0x2000;				// Linux-DVB API take PID=2000 for FULL/RAW TS flag
	pesFilterParams.input    = DMX_IN_FRONTEND;
	pesFilterParams.output   = DMX_OUT_TS_TAP;
	pesFilterParams.pes_type = DMX_PES_OTHER;
	pesFilterParams.flags    = DMX_IMMEDIATE_START;
	if (ioctl(demux1, DMX_SET_PES_FILTER, &pesFilterParams) < 0){
  		return GF_IO_ERR;
	}
  /* The following code differs from mplayer and alike because the device is opened in blocking mode */
	if ((tuner->ts_fd = open(dvr_name, O_RDONLY/*|O_NONBLOCK*/)) < 0){
	        return GF_IO_ERR;
 	}	
	return GF_OK;
}
#endif

static Bool M2TS_CanHandleURL(GF_InputService *plug, const char *url)
{
	char *sExt;
	
	if (!strnicmp(url, "udp://", 6) 
		|| !strnicmp(url, "mpegts-udp://", 13) 
		|| !strnicmp(url, "mpegts-tcp://", 13) 
#ifdef GPAC_HAS_LINUX_DVB
		|| !strnicmp(url, "dvb://", 6) 
#endif
	) {
  	return 1;
	}
	
	sExt = strrchr(url, '.');
	if (!sExt) return 0;
	if (gf_term_check_extension(plug, "video/mpeg-2", "ts m2t", "MPEG-2 TS", sExt)) return 1;
	return 0;
}

#ifdef GPAC_HAS_LINUX_DVB
static u32 gf_dvb_get_freq_from_url(const char *channels_config_path, const char *url) 
{
	FILE *channels_config_file;
	char line[255], name[255], *tmp, *channel_name;

	u32 freq;

	/* get rid of trailing @ */
	tmp = strchr(url, '@');
	if (tmp) tmp[0] = 0;

	channel_name = url+6; 

	channels_config_file = fopen(channels_config_path, "r");
	if (!channels_config_file) return GF_BAD_PARAM;

	freq = 0;
	while(!feof(channels_config_file)) {
		if ( fgets(line, 255, channels_config_file) != NULL) {
			if (line[0]=='#') continue;
			if (line[0]=='\r') continue;
			if (line[0]=='\n') continue;

			tmp = strchr(line, ':');
			tmp[0] = 0;
			if (!strcmp(line, channel_name)) {				
				char *tmp2;
				tmp++;
				tmp2 = strchr(tmp, ':');
				if (tmp2) tmp2[0] = 0;
				freq = (u32)atoi(tmp);
				break;
			}
		}
	}
	return freq;
}
#endif

static Bool M2TS_CanHandleURLInService(GF_InputService *plug, const char *url)
{
	Bool ret = 0;
	M2TSIn *m2ts = (M2TSIn *)plug->priv;

#ifdef GPAC_HAS_LINUX_DVB
	if (!stricmp(url, "dvb://EPG")) return 1;
	if (!strnicmp(url, "dvb://", 6)) {		
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[DVBIn] Checking reuse of the same tuner for %s\n", url));
		const char *chan_conf = gf_modules_get_option((GF_BaseInterface *)plug, "DVB", "ChannelsFile");
		if (!chan_conf) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[DVBIn] Cannot locate channel configuration file\n"));
			ret = 0;
		}

		/* if the tuner is already tuned to the same frequence, nothing needs to be done */
		else if (m2ts->tuner->freq != 0) {
			char *frag = strchr(url, '#');
			if (frag) frag[0] = 0;
			if (m2ts->tuner->freq == gf_dvb_get_freq_from_url(chan_conf, url)) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[DVBIn] Reusing the same tuner for %s\n", url));
				ret = 1;
			}
			if (frag) frag[0] = '#';
		} 
	} else 
#endif
	if (!strnicmp(url, "udp://", 6) 
		|| !strnicmp(url, "mpegts-udp://", 13) 
		|| !strnicmp(url, "mpegts-tcp://", 13))
	{	
		/* TODO: check IP address ...*/
		ret = 0;
	} else {
		char *frag = strchr(url, '#');
		if (frag) frag[0] = 0;
		if (!strlen(url) || !strcmp(url, m2ts->filename)) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[DVBIn] Reusing the same input file for %s\n", url));
 			ret = 1;
		}
		if (frag) frag[0] = '#';
	}
	return ret;
}

static GF_ObjectDescriptor *MP2TS_GetOD(M2TSIn *m2ts, GF_M2TS_PES *stream, char *dsi, u32 dsi_size, u32 *streamType)
{
	GF_ObjectDescriptor *od;
	GF_ESD *esd;

	/*create a stream description for this channel*/
	esd = gf_odf_desc_esd_new(0);
	esd->ESID = stream->pid;
	/*ASSIGN PCR here*/
	esd->OCRESID = stream->program->pcr_pid;
	switch (stream->stream_type) {
	case GF_M2TS_VIDEO_MPEG1:
		esd->decoderConfig->streamType = GF_STREAM_VISUAL;
		esd->decoderConfig->objectTypeIndication = 0x6A;
		break;
	case GF_M2TS_VIDEO_MPEG2:
		esd->decoderConfig->streamType = GF_STREAM_VISUAL;
		esd->decoderConfig->objectTypeIndication = 0x65;
		break;
	case GF_M2TS_VIDEO_MPEG4:
		esd->decoderConfig->streamType = GF_STREAM_VISUAL;
		esd->decoderConfig->objectTypeIndication = 0x20;
		break;
	case GF_M2TS_VIDEO_H264:
		esd->decoderConfig->streamType = GF_STREAM_VISUAL;
		esd->decoderConfig->objectTypeIndication = 0x21;
		break;
	case GF_M2TS_AUDIO_MPEG1:
		esd->decoderConfig->streamType = GF_STREAM_AUDIO;
		esd->decoderConfig->objectTypeIndication = 0x6B;
		break;
	case GF_M2TS_AUDIO_MPEG2:
		esd->decoderConfig->streamType = GF_STREAM_AUDIO;
		esd->decoderConfig->objectTypeIndication = 0x69;
		break;
	case GF_M2TS_AUDIO_LATM_AAC:
	case GF_M2TS_AUDIO_AAC:
		if (!dsi) {
			/*discard regulate until we fetch the AAC config*/
			m2ts->file_regulate = 0;
			/*turn on parsing*/
			gf_m2ts_set_pes_framing(stream, GF_M2TS_PES_FRAMING_DEFAULT);
			gf_odf_desc_del((GF_Descriptor *)esd);
			return NULL;
		}
		esd->decoderConfig->streamType = GF_STREAM_AUDIO;
		esd->decoderConfig->objectTypeIndication = 0x40;
		break;
	case GF_M2TS_SYSTEMS_MPEG4_SECTIONS:
	default:
		gf_odf_desc_del((GF_Descriptor *)esd);
		return NULL;
	}
	esd->decoderConfig->bufferSizeDB = 0;

	/*we only use AUstart indicator*/
	esd->slConfig->useAccessUnitStartFlag = 1;
	esd->slConfig->useAccessUnitEndFlag = 0;
	esd->slConfig->useRandomAccessPointFlag = 1;
	esd->slConfig->AUSeqNumLength = 0;
	esd->slConfig->timestampResolution = 90000;
	
	/*decoder config*/
	if (dsi) {
		esd->decoderConfig->decoderSpecificInfo->data = malloc(sizeof(char)*dsi_size);
		memcpy(esd->decoderConfig->decoderSpecificInfo->data, dsi, sizeof(char)*dsi_size);
		esd->decoderConfig->decoderSpecificInfo->dataLength = dsi_size;
	}

	/*declare object to terminal*/
	od = (GF_ObjectDescriptor*)gf_odf_desc_new(GF_ODF_OD_TAG);
	gf_list_add(od->ESDescriptors, esd);
	od->objectDescriptorID = stream->pid;
	if (streamType) *streamType = esd->decoderConfig->streamType;
	return od;
}

static void MP2TS_DeclareStream(M2TSIn *m2ts, GF_M2TS_PES *stream, char *dsi, u32 dsi_size)
{
	GF_ObjectDescriptor *od = MP2TS_GetOD(m2ts, stream, dsi, dsi_size, NULL);
	if (!od) return;
	/*declare but don't regenerate scene*/
	gf_term_add_media(m2ts->service, (GF_Descriptor*)od, 1);
}

static void MP2TS_SetupProgram(M2TSIn *m2ts, GF_M2TS_Program *prog, Bool regenerate_scene, Bool no_declare)
{
	u32 i, count;

	count = gf_list_count(prog->streams);
#ifdef GPAC_HAS_LINUX_DVB
	if (m2ts->tuner) {
		Bool found = 0;
		for (i=0; i<count; i++) {
			GF_M2TS_PES *pes = gf_list_get(prog->streams, i);
			if (pes->pid==m2ts->tuner->vpid) found = 1;
			else if (pes->pid==m2ts->tuner->apid) found = 1;
		}
		if (!found) return;
	}
#endif	
	m2ts->file_regulate = no_declare ? 0 : 1;

	for (i=0; i<count; i++) {
		GF_M2TS_ES *es = gf_list_get(prog->streams, i);
		if (es->pid==prog->pmt_pid) continue;
		/*move to skip mode for all PES until asked for playback*/
		if (!(es->flags & GF_M2TS_ES_IS_SECTION) && !es->user)
			gf_m2ts_set_pes_framing((GF_M2TS_PES *)es, GF_M2TS_PES_FRAMING_SKIP);
		if (!prog->pmt_iod && !no_declare) MP2TS_DeclareStream(m2ts, (GF_M2TS_PES *)es, NULL, 0);
	}
	/*force scene regeneration*/
	if (regenerate_scene) 
		gf_term_add_media(m2ts->service, NULL, 0);
}

static void MP2TS_SendPacket(M2TSIn *m2ts, GF_M2TS_PES_PCK *pck)
{
	GF_SLHeader slh;

	if (!pck->stream->user) return;

	if (!pck->stream->program->first_dts && pck->PTS) {
		pck->stream->program->first_dts = (pck->DTS ? pck->DTS : pck->PTS) - m2ts->start_range * 90;
	}

	memset(&slh, 0, sizeof(GF_SLHeader));
	slh.accessUnitStartFlag = (pck->flags & GF_M2TS_PES_PCK_AU_START) ? 1 : 0;
	if (slh.accessUnitStartFlag) {
		if (pck->PTS < pck->stream->program->first_dts) return;
		slh.compositionTimeStampFlag = 1;
		slh.compositionTimeStamp = pck->PTS - pck->stream->program->first_dts;
		if (pck->DTS) {
			slh.decodingTimeStampFlag = 1;
			slh.decodingTimeStamp = pck->DTS - pck->stream->program->first_dts;
		}
		slh.randomAccessPointFlag = (pck->flags & GF_M2TS_PES_PCK_RAP) ? 1 : 0;
	}
	gf_term_on_sl_packet(m2ts->service, pck->stream->user, pck->data, pck->data_len, &slh, GF_OK);
}

static GFINLINE void MP2TS_SendSLPacket(M2TSIn *m2ts, GF_M2TS_SL_PCK *pck)
{
	gf_term_on_sl_packet(m2ts->service, pck->stream->user, pck->data, pck->data_len, NULL, GF_OK);
}

static GF_ObjectDescriptor *M2TS_GenerateEPG_OD(M2TSIn *m2ts) 
{
	/* declaring a special stream for displaying eit */
	GF_ObjectDescriptor *od;
	GF_ESD *esd;

	/*create a stream description for this channel*/
	esd = gf_odf_desc_esd_new(0);
	esd->ESID = GF_M2TS_PID_EIT_ST_CIT;
	esd->OCRESID = GF_M2TS_PID_EIT_ST_CIT;
	esd->decoderConfig->streamType = GF_STREAM_PRIVATE_SCENE;
	esd->decoderConfig->objectTypeIndication = GPAC_OTI_PRIVATE_SCENE_EPG;
	esd->decoderConfig->bufferSizeDB = 0;

	/*we only use AUstart indicator
	esd->slConfig->useAccessUnitStartFlag = 1;
	esd->slConfig->useAccessUnitEndFlag = 0;
	esd->slConfig->useRandomAccessPointFlag = 1;
	esd->slConfig->AUSeqNumLength = 0;
	esd->slConfig->timestampResolution = 90000;*/

	/*declare object to terminal*/
	od = (GF_ObjectDescriptor*)gf_odf_desc_new(GF_ODF_OD_TAG);
	gf_list_add(od->ESDescriptors, esd);
	od->objectDescriptorID = GF_M2TS_PID_EIT_ST_CIT;	
	return od;
}

static void M2TS_FlushRequested(M2TSIn *m2ts)
{
	u32 i, j, req_prog_count, count, prog_id, found;

	gf_mx_p(m2ts->mx);

	found = 0;
	count = gf_list_count(m2ts->requested_pids);
	for (i=0; i<count; i++) {
		M2TSIn_Prog *req_pid = gf_list_get(m2ts->requested_pids, i);
		GF_M2TS_ES *es = m2ts->ts->ess[req_pid->pid];
		if (es==NULL) continue;

		/*move to skip mode for all PES until asked for playback*/
		if (!(es->flags & GF_M2TS_ES_IS_SECTION) && !es->user) 
			gf_m2ts_set_pes_framing((GF_M2TS_PES *)es, GF_M2TS_PES_FRAMING_SKIP);
		MP2TS_DeclareStream(m2ts, (GF_M2TS_PES *)es, NULL, 0);
		gf_list_rem(m2ts->requested_pids, i);
		free(req_pid);
		i--;
		count--;
		found++;
	}
	req_prog_count = gf_list_count(m2ts->requested_progs);
	for (i = 0; i < req_prog_count; i++) {
		M2TSIn_Prog *req_prog = gf_list_get(m2ts->requested_progs, i);
		prog_id = atoi(req_prog->fragment);
		count = gf_list_count(m2ts->ts->SDTs);
		for (j=0; j<count; j++) {
			GF_M2TS_SDT *sdt = gf_list_get(m2ts->ts->SDTs, j);
			if (!stricmp(sdt->service, req_prog->fragment)) req_prog->id = sdt->service_id;
			else if (sdt->service_id==prog_id)  req_prog->id = sdt->service_id;
		}
		if (req_prog->id) {
			GF_M2TS_Program *ts_prog;
			count = gf_list_count(m2ts->ts->programs);
			for (j=0; j<count; j++) {
				ts_prog = gf_list_get(m2ts->ts->programs, j);
				if (ts_prog->number==req_prog->id) {
					MP2TS_SetupProgram(m2ts, ts_prog, 0, 0);
					found++;
					free(req_prog->fragment);
					free(req_prog);
					gf_list_rem(m2ts->requested_progs, i);
					req_prog_count--;
					i--;
					break;
				}
			}
		}
	}

	if (m2ts->epg_requested) {
		if (!m2ts->has_eit) {
			GF_ObjectDescriptor *od = M2TS_GenerateEPG_OD(m2ts);
			/*declare but don't regenerate scene*/
			gf_term_add_media(m2ts->service, (GF_Descriptor*)od, 0);
			m2ts->has_eit = 1;
		}
	} else {
		/*force scene regeneration only when EPG is not requested*/
		if (found)
			gf_term_add_media(m2ts->service, NULL, 0);
	}

	gf_mx_v(m2ts->mx);
}

static void M2TS_OnEvent(GF_M2TS_Demuxer *ts, u32 evt_type, void *param)
{
	M2TSIn *m2ts = (M2TSIn *) ts->user;
	switch (evt_type) {
	case GF_M2TS_EVT_PAT_UPDATE:
		break;
	case GF_M2TS_EVT_PAT_FOUND:
		/* In case the TS has one program, wait for the PMT to send connect, in case of IOD in PMT */
		if (gf_list_count(m2ts->ts->programs) != 1)
			gf_term_on_connect(m2ts->service, NULL, GF_OK);
		break;
	case GF_M2TS_EVT_PMT_FOUND:
		if (gf_list_count(m2ts->ts->programs) == 1)
			gf_term_on_connect(m2ts->service, NULL, GF_OK);
		
		/*do not declare if  single program was requested for playback*/
		MP2TS_SetupProgram(m2ts, param, m2ts->request_all_pids, m2ts->request_all_pids ? 0 : 1);

		M2TS_FlushRequested(m2ts);
		break;
	case GF_M2TS_EVT_PMT_REPEAT:
	case GF_M2TS_EVT_PMT_UPDATE:
		M2TS_FlushRequested(m2ts);
		break;
	case GF_M2TS_EVT_SDT_REPEAT:
	case GF_M2TS_EVT_SDT_UPDATE:
	case GF_M2TS_EVT_SDT_FOUND:
		M2TS_FlushRequested(m2ts);
		break;
	case GF_M2TS_EVT_DVB_GENERAL:
		if (m2ts->eit_channel) {
			GF_M2TS_SL_PCK *pck = (GF_M2TS_SL_PCK *)param;
			gf_term_on_sl_packet(m2ts->service, m2ts->eit_channel, pck->data, pck->data_len, NULL, GF_OK);
		}
		break;
	case GF_M2TS_EVT_PES_PCK:
		MP2TS_SendPacket(m2ts, param);
		break;
	case GF_M2TS_EVT_SL_PCK:
		MP2TS_SendSLPacket(m2ts, param);
		break;
	case GF_M2TS_EVT_AAC_CFG:
	{
		GF_M2TS_PES_PCK *pck = (GF_M2TS_PES_PCK*)param;
		if (!pck->stream->first_dts) {
			gf_m2ts_set_pes_framing(pck->stream, GF_M2TS_PES_FRAMING_SKIP);
			MP2TS_DeclareStream(m2ts, pck->stream, pck->data, pck->data_len);
			m2ts->file_regulate = 1;
			/*force scene regeneration*/
			gf_term_add_media(m2ts->service, NULL, 0);
		}
	}
		break;
	case GF_M2TS_EVT_PES_PCR:
		if (m2ts->file_regulate) {
			u64 pcr = ((GF_M2TS_PES_PCK *) param)->PTS;
			u32 stb = gf_sys_clock();
			if (m2ts->pcr_last) {
				s32 diff;
				u64 pcr_diff = (pcr - m2ts->pcr_last);
				pcr_diff /= 27000;
				diff = (u32) pcr_diff - (stb - m2ts->stb_at_last_pcr);
				if (diff>0 && (diff<1000) ) {
					/*query buffer level, don't sleep if too low*/
					GF_NetworkCommand com;
					com.command_type = GF_NET_BUFFER_QUERY;
					gf_term_on_command(m2ts->service, &com, GF_OK);
					if (com.buffer.occupancy) gf_sleep(diff);
				
					m2ts->nb_pck = 0;
					m2ts->pcr_last = pcr;
					m2ts->stb_at_last_pcr = gf_sys_clock();
				}
			} else {
				m2ts->pcr_last = pcr;
				m2ts->stb_at_last_pcr = gf_sys_clock();
			}
		}
		break;
	}
}


u32 M2TS_Run(void *_p)
{
	GF_Err e;
	char data[UDP_BUFFER_SIZE];
#ifdef GPAC_HAS_LINUX_DVB
	char dvbts[DVB_BUFFER_SIZE];
#endif
	u32 size, i;
	M2TSIn *m2ts = _p;

	m2ts->run_state = 1;
	m2ts->ts->on_event = M2TS_OnEvent;
	gf_m2ts_reset_parsers(m2ts->ts);

#ifdef GPAC_HAS_LINUX_DVB
	if (m2ts->tuner) {
		// in case of DVB
		while (m2ts->run_state) {
			s32 ts_size = read(m2ts->tuner->ts_fd, dvbts, DVB_BUFFER_SIZE);
			if (ts_size>0) gf_m2ts_process_data(m2ts->ts, dvbts, (u32) ts_size);
		}	
	} else
#endif
	 if (m2ts->sock) {
		Bool first_run, is_rtp;
		first_run = 1;
		is_rtp = 0;
		while (m2ts->run_state) {
			size = 0;
			/*m2ts chunks by chunks*/
			e = gf_sk_receive(m2ts->sock, data, UDP_BUFFER_SIZE, 0, &size);
			if (!size || e) {
				gf_sleep(1);
				continue;
			}
			if (first_run) {
				first_run = 0;
				/*FIXME: we assume only simple RTP packaging (no CSRC nor extensions)*/
				if ((data[0] != 0x47) && ((data[1] & 0x7F) == 33) ) {
					is_rtp = 1;
				}
			}
			/*process chunk*/
			if (is_rtp) {
				gf_m2ts_process_data(m2ts->ts, data+12, size-12);
			} else {
				gf_m2ts_process_data(m2ts->ts, data, size);
			}
		}
	} else {
		u32 pos = 0;
		if (m2ts->start_range && m2ts->duration) {
			Double perc = m2ts->start_range / (1000 * m2ts->duration);
			pos = (u32) (s64) (perc * m2ts->file_size);
			/*align to TS packet size*/
			while (pos%188) pos++;
			if (pos>=m2ts->file_size) {
				m2ts->start_range = 0;
				pos = 0;
			}
		}
		fseek(m2ts->file, pos, SEEK_SET);
		while (m2ts->run_state && !feof(m2ts->file) ) {
			/*m2ts chunks by chunks*/
			size = fread(data, 1, 188, m2ts->file);
			if (!size) break;
			/*process chunk*/
			gf_m2ts_process_data(m2ts->ts, data, size);

			m2ts->nb_pck++;
			/*if asked to regulate, wait until we get a play request*/
			while (m2ts->run_state && !m2ts->nb_playing && m2ts->file_regulate) {
				gf_sleep(50);
				continue;
			}
		}
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("\nEOS reached\n"));
		if (m2ts->nb_playing) {
			for (i=0; i<GF_M2TS_MAX_STREAMS; i++) {
				GF_M2TS_PES *pes = (GF_M2TS_PES *)m2ts->ts->ess[i];
				if (!pes || (pes->pid==pes->program->pmt_pid)) continue;
				if (!pes->user || !pes->reframe) continue;
				gf_term_on_sl_packet(m2ts->service, pes->user, NULL, 0, NULL, GF_EOS);
				gf_m2ts_set_pes_framing(pes, GF_M2TS_PES_FRAMING_SKIP);
			}
		}

	}
	m2ts->run_state = 2;
	return 0;
}

static void M2TS_OnEventPCR(GF_M2TS_Demuxer *ts, u32 evt_type, void *param)
{
	if (evt_type==GF_M2TS_EVT_PES_PCR) {
		M2TSIn *m2ts = ts->user;
		GF_M2TS_PES_PCK *pck = param;
		if (!m2ts->nb_playing) {
			m2ts->nb_playing = pck->stream->pid;
			m2ts->end_range = (u32) (pck->PTS / 90);
		} else if (m2ts->nb_playing == pck->stream->pid) {
			m2ts->start_range = (u32) (pck->PTS / 90);
		}
	}
}

#ifdef GPAC_HAS_LINUX_DVB
void M2TS_SetupDVB(GF_InputService *plug, M2TSIn *m2ts, char *url)
{
	GF_Err e = GF_OK;
	char *str;
	const char *chan_conf;

	if (strnicmp(url, "dvb://", 6)) {
		e = GF_NOT_SUPPORTED;
		goto exit;
	}

	chan_conf = gf_modules_get_option((GF_BaseInterface *)plug, "DVB", "ChannelsFile");
	if (!chan_conf) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[DVBIn] Cannot locate channel configuration file\n"));
		e = GF_SERVICE_ERROR;
		goto exit;
	}
	
	if (!m2ts->tuner) GF_SAFEALLOC(m2ts->tuner, GF_Tuner);

	if (m2ts->tuner->freq != 0 && m2ts->tuner->freq == gf_dvb_get_freq_from_url(chan_conf, url)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[DVBIn] Tuner already tuned to that frequency\n"));
		goto exit;
	} 

	e = gf_dvb_tune(m2ts->tuner, url, chan_conf);
	if (e) goto exit;

	m2ts->th = gf_th_new("MPEG-2 TS Demux");
	/*start playing for tune-in*/
	gf_th_run(m2ts->th, M2TS_Run, m2ts);

exit:
	if (e) gf_term_on_connect(m2ts->service, NULL, e);
}
#endif

void M2TS_SetupLive(GF_InputService *plug, M2TSIn *m2ts, char *url)
{
	GF_Err e = GF_OK;
	char *str;
	u16 port;
	u32 sock_type = 0;

	if (!strnicmp(url, "udp://", 6) || !strnicmp(url, "mpegts-udp://", 13)) {
		sock_type = GF_SOCK_TYPE_UDP;
	} else if (!strnicmp(url, "mpegts-tcp://", 13) ) {
		sock_type = GF_SOCK_TYPE_TCP;
	} else {
		e = GF_NOT_SUPPORTED;
		goto exit;
	}

	url = strchr(url, ':');
	url += 3;

	m2ts->sock = gf_sk_new(sock_type);
	if (!m2ts->sock) { e = GF_IO_ERR; goto exit; }

	/*setup port and src*/
	port = 1234;
	str = strrchr(url, ':');
	/*take care of IPv6 address*/
	if (str && strchr(str, ']')) str = strchr(url, ':');
	if (str) {
		port = atoi(str+1);
		str[0] = 0;
	}

	/*do we have a source ?*/
	if (strlen(url) && strcmp(url, "localhost") ) {
		const char *mob_ip = NULL;
		const char *mob_on = gf_modules_get_option((GF_BaseInterface*)plug, "Network", "MobileIPEnabled");
		if (mob_on && !strcmp(mob_on, "yes"))
			mob_ip = gf_modules_get_option((GF_BaseInterface*)plug, "Network", "MobileIP");

		if (gf_sk_is_multicast_address(url)) {
			const char *mcast_ifce = gf_modules_get_option((GF_BaseInterface *) plug, "Network", "DefaultMCastInterface");
			if (mcast_ifce) mob_ip = mcast_ifce;

			gf_sk_setup_multicast(m2ts->sock, url, port, 0, 0, (char*)mob_ip);
		} else {
			gf_sk_bind(m2ts->sock, (char*)mob_ip, port, url, 0, GF_SOCK_REUSE_PORT);
		}
	}
	if (str) str[0] = ':';

	gf_sk_set_buffer_size(m2ts->sock, 0, UDP_BUFFER_SIZE);
	gf_sk_set_block_mode(m2ts->sock, 0);

	m2ts->th = gf_th_new("MPEG-2 TS Demux");
	gf_th_set_priority(m2ts->th, GF_THREAD_PRIORITY_HIGHEST);
	/*start playing for tune-in*/
	gf_th_run(m2ts->th, M2TS_Run, m2ts);

exit:
	if (e) {
		gf_term_on_connect(m2ts->service, NULL, e);
	}
}

void M2TS_SetupFile(M2TSIn *m2ts, char *url)
{
#if 0
	char data[188];
	u32 size, fsize;
	s32 nb_rwd;
#endif
	if (m2ts->file && !strcmp(m2ts->filename, url)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEGTSIn] TS already being processed\n"));
		return;
	} 

	m2ts->file = fopen(url, "rb");
	if (!m2ts->file) {
		gf_term_on_connect(m2ts->service, NULL, GF_URL_ERROR);
		return;
	}
	strcpy(m2ts->filename, url);

	fseek(m2ts->file, 0, SEEK_END);
	m2ts->file_size = ftell(m2ts->file);
	
#if 0
	/* 
		estimate duration by reading the end of the file
		m2ts->end_range is initialized to the PTS of the last TS packet
		m2ts->nb_playing is initialized to the PID of the last TS packet
	*/
	m2ts->nb_playing = 0;
	m2ts->ts->on_event = M2TS_OnEventPCR;
	m2ts->end_range = 0;
	nb_rwd = 1;
	fsize = m2ts->file_size;
	while (fsize % 188) fsize--;
	while (1) {
		fseek(m2ts->file, fsize - 188 * nb_rwd, SEEK_SET);
		/*m2ts chunks by chunks*/
		size = fread(data, 1, 188, m2ts->file);
		if (!size) break;
		/*process chunk*/
		gf_m2ts_process_data(m2ts->ts, data, size);
		if (m2ts->nb_playing) break;
		nb_rwd ++;
	}

	/* 
	   reset of the file 
	   initialization of m2ts->start_range to the PTS of the first TS packet with PID = m2ts->nb_playing 
	*/
	fseek(m2ts->file, 0, SEEK_SET);
	gf_m2ts_reset_parsers(m2ts->ts);
	m2ts->start_range = 0;
	while (1) {
		/*m2ts chunks by chunks*/
		size = fread(data, 1, 188, m2ts->file);
		if (!size) break;
		/*process chunk*/
		gf_m2ts_process_data(m2ts->ts, data, size);
		if (m2ts->start_range) break;
	}
	m2ts->duration = (m2ts->end_range - m2ts->start_range) / 300000.0;
	gf_m2ts_demux_del(m2ts->ts);

	/* Creation of the real demuxer for playback */
	m2ts->ts = gf_m2ts_demux_new();
	m2ts->ts->user = m2ts;
#endif

	/* reinitialization for seek */
	m2ts->end_range = m2ts->start_range = 0;
	m2ts->nb_playing = 0;

	m2ts->th = gf_th_new("MPEG-2 TS Demux");
	/*start playing for tune-in*/
	gf_th_run(m2ts->th, M2TS_Run, m2ts);
}

static GF_Err M2TS_ConnectService(GF_InputService *plug, GF_ClientService *serv, const char *url)
{
	char szURL[2048];
	char *frag;
	M2TSIn *m2ts = plug->priv;
	m2ts->service = serv;

	strcpy(szURL, url);
	frag = strrchr(szURL, '#');
	if (frag) frag[0] = 0;

	m2ts->file_regulate = 0;
	m2ts->duration = 0;

	if (!strnicmp(url, "udp://", 6)
		|| !strnicmp(url, "mpegts-udp://", 13) 
		|| !strnicmp(url, "mpegts-tcp://", 13) 
		) {
		M2TS_SetupLive(plug, m2ts, (char *) szURL);
	} 
#ifdef GPAC_HAS_LINUX_DVB
	else if (!strnicmp(url, "dvb://", 6)) {
		// DVB Setup
		M2TS_SetupDVB(plug, m2ts, (char *) szURL);
	} 
#endif
	else {
		M2TS_SetupFile(m2ts, (char *) szURL);
	}
	return GF_OK;
}

static GF_Err M2TS_CloseService(GF_InputService *plug)
{
	M2TSIn *m2ts = plug->priv;

	GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("destroying TSin\n"));
	if (m2ts->th) {
		if (m2ts->run_state == 1) {
			m2ts->run_state = 0;
			while (m2ts->run_state!=2) gf_sleep(0);
		}
		gf_th_del(m2ts->th);
		m2ts->th = NULL;
	}

	if (m2ts->file) fclose(m2ts->file);
	m2ts->file = NULL;
	gf_term_on_disconnect(m2ts->service, NULL, GF_OK);
	return GF_OK;
}

static GF_Descriptor *M2TS_GetServiceDesc(GF_InputService *plug, u32 expect_type, const char *sub_url)
{
	M2TSIn *m2ts = plug->priv;
	GF_Descriptor *desc = NULL;
	char *frag;

	frag = sub_url ? strrchr(sub_url, '#') : NULL;
	/*we have been requested the entire TS*/
	if (!stricmp(sub_url, "dvb://EPG")) {
		m2ts->epg_requested = 1;
	} else if (!frag) {
		m2ts->request_all_pids = 1;
	} else {
		M2TSIn_Prog *prog;
		frag++;

		/*we need exclusive access*/
		gf_mx_p(m2ts->mx);
		if (!strnicmp(frag, "pid=", 4)) {
			GF_SAFEALLOC(prog, M2TSIn_Prog);
			prog->pid = atoi(frag+4);
			gf_list_add(m2ts->requested_pids, prog);
		} else if (!strnicmp(frag, "EPG", 3)) {
			m2ts->epg_requested = 1;
		} else {
			u32 i, count;
			count = gf_list_count(m2ts->requested_progs);
			prog = NULL;
			for (i=0; i<count; i++) {
				prog = gf_list_get(m2ts->requested_progs, i);
				if (!strcmp(prog->fragment, frag))
					break;
				prog = NULL;
			}
			if (!prog) {
				GF_SAFEALLOC(prog, M2TSIn_Prog);
				gf_list_add(m2ts->requested_progs, prog);
				prog->fragment = strdup(frag);
			}
		}
		gf_mx_v(m2ts->mx);
	}

	if (expect_type==GF_MEDIA_OBJECT_SCENE) {	
		if (gf_list_count(m2ts->ts->programs) == 1) {
			GF_M2TS_Program *prog = gf_list_get(m2ts->ts->programs, 0);
			if (prog->pmt_iod) {
				gf_odf_desc_copy((GF_Descriptor *)prog->pmt_iod, &desc);
				return desc;
			}
		} 
		if (m2ts->epg_requested) {
			GF_ObjectDescriptor *od = M2TS_GenerateEPG_OD(m2ts);
			m2ts->epg_requested = 0;
			return (GF_Descriptor *)od;
		} else {
			/*returning an empty IOD means "no scene description", let the terminal handle all media objects*/
			desc = gf_odf_desc_new(GF_ODF_IOD_TAG);
			((GF_ObjectDescriptor *) desc)->objectDescriptorID = 1;
			return desc;
		}
	}

	return NULL;
}

static GF_Err M2TS_ConnectChannel(GF_InputService *plug, LPNETCHANNEL channel, const char *url, Bool upstream)
{
	u32 ES_ID;
	GF_Err e;
	M2TSIn *m2ts = plug->priv;

	e = GF_STREAM_NOT_FOUND;
	if (strstr(url, "ES_ID")) {
		sscanf(url, "ES_ID=%d", &ES_ID);
		
		/* In case there is a real IOD, we need to translate PID into ESID */
		if (gf_list_count(m2ts->ts->programs) == 1) {
			GF_M2TS_Program *prog = gf_list_get(m2ts->ts->programs, 0);
			if (prog->pmt_iod) {
				u32 i;
				for (i=0; i<GF_M2TS_MAX_STREAMS; i++) {
					GF_M2TS_PES *pes = (GF_M2TS_PES *)m2ts->ts->ess[i];
					if (!pes || (pes->pid==pes->program->pmt_pid)) continue;
					if (pes->mpeg4_es_id == ES_ID) {
						if (pes->user) {
							e = GF_SERVICE_ERROR;
							gf_term_on_connect(m2ts->service, channel, e);
							return e;
						} else {
							pes->user = channel;
							e = GF_OK;
							gf_term_on_connect(m2ts->service, channel, e);
							return e;
						}
					}
				}
			}
		} 

		if (ES_ID == 18) {
			e = GF_OK; /* 18 is the PID of EIT packets */
			m2ts->eit_channel = channel;
		} else if ((ES_ID<GF_M2TS_MAX_STREAMS) && m2ts->ts->ess[ES_ID]) {
			GF_M2TS_PES *pes = (GF_M2TS_PES *)m2ts->ts->ess[ES_ID];
			if (pes->user) {
				e = GF_SERVICE_ERROR;
			} else {
				pes->user = channel;
				e = GF_OK;
			}
		}
	}
	gf_term_on_connect(m2ts->service, channel, e);
	return e;
}

static GF_M2TS_PES *M2TS_GetChannel(M2TSIn *m2ts, LPNETCHANNEL channel)
{
	u32 i;
	for (i=0; i<GF_M2TS_MAX_STREAMS; i++) {
		GF_M2TS_PES *pes = (GF_M2TS_PES *)m2ts->ts->ess[i];
		if (!pes || (pes->pid==pes->program->pmt_pid)) continue;
		if (pes->user == channel) return pes;
	}
	return NULL;
}
static GF_Err M2TS_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
	M2TSIn *m2ts = plug->priv;
	GF_Err e = GF_STREAM_NOT_FOUND;
	GF_M2TS_PES *pes = M2TS_GetChannel(m2ts, channel);
	if (pes) {
		pes->user = NULL;
		e = GF_OK;
	}
	gf_term_on_disconnect(m2ts->service, channel, e);
	return GF_OK;
}

static GF_Err M2TS_ServiceCommand(GF_InputService *plug, GF_NetworkCommand *com)
{
	GF_M2TS_PES *pes;
	M2TSIn *m2ts = plug->priv;

	if (com->command_type==GF_NET_SERVICE_HAS_AUDIO) {
		char *frag = strchr(com->audio.base_url, '#');
		if (frag && !strnicmp(frag, "#pid=", 5)) return GF_NOT_SUPPORTED;
		return GF_OK;
	}
	if (!com->base.on_channel) return GF_NOT_SUPPORTED;
	switch (com->command_type) {
	/*we cannot pull complete AUs from the stream*/
	case GF_NET_CHAN_SET_PULL:
		return GF_NOT_SUPPORTED;
	/*we cannot seek stream by stream*/
	case GF_NET_CHAN_INTERACTIVE:
		return GF_NOT_SUPPORTED;
	case GF_NET_CHAN_BUFFER:
		com->buffer.max = M2TS_BUFFER_MAX;
		com->buffer.min = 0;
		return GF_OK;
	case GF_NET_CHAN_DURATION:
		com->duration.duration = m2ts->duration;
		return GF_OK;
	case GF_NET_CHAN_PLAY:
		pes = M2TS_GetChannel(m2ts, com->base.on_channel);
		if (!pes) {
			if (com->base.on_channel == m2ts->eit_channel) {
				return GF_OK;
			}
			return GF_STREAM_NOT_FOUND;
		}
		gf_m2ts_set_pes_framing(pes, GF_M2TS_PES_FRAMING_DEFAULT);
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Setting default reframing\n"));
		/*this is a multplex, only trigger the play command for the first stream activated*/
		if (!m2ts->nb_playing) {
			m2ts->start_range = (u32) (com->play.start_range*1000);
			m2ts->end_range = (com->play.end_range>0) ? (u32) (com->play.end_range*1000) : 0;
			/*start demuxer*/
			if (m2ts->run_state!=1) {
				gf_th_run(m2ts->th, M2TS_Run, m2ts);
			}
		}
		m2ts->nb_playing++;
		return GF_OK;
	case GF_NET_CHAN_STOP:
		pes = M2TS_GetChannel(m2ts, com->base.on_channel);
		if (!pes) {
			if (com->base.on_channel == m2ts->eit_channel) {
				return GF_OK;
			}
			return GF_STREAM_NOT_FOUND;
		}
		gf_m2ts_set_pes_framing(pes, GF_M2TS_PES_FRAMING_SKIP);
		/*FIXME HOORIBLE HACK*/
		return GF_OK;
		assert(m2ts->nb_playing);
		m2ts->nb_playing--;
		/*stop demuxer*/
		if (!m2ts->nb_playing && (m2ts->run_state==1)) {
			m2ts->run_state=0;
			while (m2ts->run_state!=2) gf_sleep(2);
		}
		return GF_OK;
	default:
		return GF_OK;
	}
}


GF_InputService *NewM2TSReader()
{
	M2TSIn *reader;
	GF_InputService *plug = malloc(sizeof(GF_InputService));
	memset(plug, 0, sizeof(GF_InputService));
	GF_REGISTER_MODULE_INTERFACE(plug, GF_NET_CLIENT_INTERFACE, "GPAC MPEG-2 TS Reader", "gpac distribution")

	plug->CanHandleURL = M2TS_CanHandleURL;
	plug->CanHandleURLInService = M2TS_CanHandleURLInService;
	plug->ConnectService = M2TS_ConnectService;
	plug->CloseService = M2TS_CloseService;
	plug->GetServiceDescriptor = M2TS_GetServiceDesc;
	plug->ConnectChannel = M2TS_ConnectChannel;
	plug->DisconnectChannel = M2TS_DisconnectChannel;
	plug->ServiceCommand = M2TS_ServiceCommand;

	reader = malloc(sizeof(M2TSIn));
	memset(reader, 0, sizeof(M2TSIn));
	plug->priv = reader;
	reader->requested_progs = gf_list_new();
	reader->requested_pids = gf_list_new();
	reader->ts = gf_m2ts_demux_new();
	reader->ts->on_event = M2TS_OnEvent;
	reader->ts->user = reader;
	reader->mx = gf_mx_new("MPEG2 Demux");
	return plug;
}

void DeleteM2TSReader(void *ifce)
{
	u32 i, count;

	GF_InputService *plug = (GF_InputService *) ifce;
	M2TSIn *m2ts = plug->priv;

	count = gf_list_count(m2ts->requested_progs);
	for (i = 0; i < count; i++) {
		M2TSIn_Prog *prog = gf_list_get(m2ts->requested_progs, i);
		free(prog->fragment);
		free(prog);
	}
	gf_list_del(m2ts->requested_progs);
	count = gf_list_count(m2ts->requested_pids);
	for (i = 0; i < count; i++) {
		M2TSIn_Prog *prog = gf_list_get(m2ts->requested_pids, i);
		free(prog);
	}
	gf_list_del(m2ts->requested_pids);
	gf_m2ts_demux_del(m2ts->ts);
	gf_mx_del(m2ts->mx);
	free(m2ts);
	free(plug);
}


Bool QueryInterface(u32 InterfaceType)
{
	switch (InterfaceType) {
	case GF_NET_CLIENT_INTERFACE: return 1;
	default: return 0;
	}
}

GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	switch (InterfaceType) {
	case GF_NET_CLIENT_INTERFACE: return (GF_BaseInterface *) NewM2TSReader();
	default: return NULL;
	}
}

void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_NET_CLIENT_INTERFACE:  DeleteM2TSReader(ifce); break;
	}
}
