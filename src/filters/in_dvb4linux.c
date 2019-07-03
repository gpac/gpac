/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017
 *					All rights reserved
 *
 *  This file is part of GPAC / DVB4Linux input filter
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


#include <gpac/filters.h>
#include <gpac/constants.h>
#include <gpac/network.h>

#ifndef WIN32
//#define GPAC_HAS_LINUX_DVB
//#define GPAC_SIM_LINUX_DVB
#endif


#ifdef GPAC_HAS_LINUX_DVB

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef GPAC_SIM_LINUX_DVB
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#endif

typedef struct
{
	//options
	const char *src;
	const char *chcfg;
	u32 block_size;

	//only one output pid declared
	GF_FilterPid *pid;

	u32 freq;
	u16 vpid;
	u16 apid;

#ifndef GPAC_SIM_LINUX_DVB
	fe_spectral_inversion_t specInv;
	fe_modulation_t modulation;
	fe_bandwidth_t bandwidth;
	fe_transmit_mode_t TransmissionMode;
	fe_guard_interval_t guardInterval;
	fe_code_rate_t HP_CodeRate;
	fe_code_rate_t LP_CodeRate;
	fe_hierarchy_t hierarchy;
#endif

	int demux_fd;

	char *block;

} GF_DVBLinuxCtx;


static GF_Err dvblin_tune(GF_DVBLinuxCtx *ctx)
{
	int demux1, front1;
	FILE *chanfile;
	char line[255];
#ifndef GPAC_SIM_LINUX_DVB
	struct dmx_pes_filter_params pesFilterParams;
	struct dvb_frontend_parameters frp;
	char chan_name_t[255];
	char freq_str[255], inv[255], bw[255], lcr[255], hier[255], cr[255],
	     mod[255], transm[255], gi[255], apid_str[255], vpid_str[255];
	const char *chan_conf = ":%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:";
#endif
	char *chan_name;
	char *tmp;
	char frontend_name[100], demux_name[100], dvr_name[100];
	u32 adapter_num;

	if (!ctx->src) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[DVB4Lin] Missing URL\n"));
		return GF_BAD_PARAM;
	}
	if (!ctx->chcfg) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[DVB4Lin] Missing channels config file\n"));
		return GF_BAD_PARAM;
	}
	chanfile = gf_fopen(ctx->chcfg, "rb");
	if (!chanfile) return GF_BAD_PARAM;

	chan_name = (char *) ctx->src+6; // 6 = strlen("dvb://")

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

#ifndef GPAC_SIM_LINUX_DVB
			strncpy(chan_name_t, line, index(line, ':')-line);
			if (strncmp(chan_name,chan_name_t,strlen(chan_name))==0) {
				sscanf(strstr(line,":"), chan_conf, freq_str, inv, bw, lcr, cr, mod, transm, gi, hier, apid_str, vpid_str);
				ctx->freq = (uint32_t) atoi(freq_str);
				ctx->apid = (uint16_t) atoi(apid_str);
				ctx->vpid = (uint16_t) atoi(vpid_str);
				//Inversion
				if(! strcmp(inv, "INVERSION_ON")) ctx->specInv = INVERSION_ON;
				else if(! strcmp(inv, "INVERSION_OFF")) ctx->specInv = INVERSION_OFF;
				else ctx->specInv = INVERSION_AUTO;
				//LP Code Rate
				if(! strcmp(lcr, "FEC_1_2")) ctx->LP_CodeRate =FEC_1_2;
				else if(! strcmp(lcr, "FEC_2_3")) ctx->LP_CodeRate =FEC_2_3;
				else if(! strcmp(lcr, "FEC_3_4")) ctx->LP_CodeRate =FEC_3_4;
				else if(! strcmp(lcr, "FEC_4_5")) ctx->LP_CodeRate =FEC_4_5;
				else if(! strcmp(lcr, "FEC_6_7")) ctx->LP_CodeRate =FEC_6_7;
				else if(! strcmp(lcr, "FEC_8_9")) ctx->LP_CodeRate =FEC_8_9;
				else if(! strcmp(lcr, "FEC_5_6")) ctx->LP_CodeRate =FEC_5_6;
				else if(! strcmp(lcr, "FEC_7_8")) ctx->LP_CodeRate =FEC_7_8;
				else if(! strcmp(lcr, "FEC_NONE")) ctx->LP_CodeRate =FEC_NONE;
				else ctx->LP_CodeRate =FEC_AUTO;
				//HP Code Rate
				if(! strcmp(cr, "FEC_1_2")) ctx->HP_CodeRate =FEC_1_2;
				else if(! strcmp(cr, "FEC_2_3")) ctx->HP_CodeRate =FEC_2_3;
				else if(! strcmp(cr, "FEC_3_4")) ctx->HP_CodeRate =FEC_3_4;
				else if(! strcmp(cr, "FEC_4_5")) ctx->HP_CodeRate =FEC_4_5;
				else if(! strcmp(cr, "FEC_6_7")) ctx->HP_CodeRate =FEC_6_7;
				else if(! strcmp(cr, "FEC_8_9")) ctx->HP_CodeRate =FEC_8_9;
				else if(! strcmp(cr, "FEC_5_6")) ctx->HP_CodeRate =FEC_5_6;
				else if(! strcmp(cr, "FEC_7_8")) ctx->HP_CodeRate =FEC_7_8;
				else if(! strcmp(cr, "FEC_NONE")) ctx->HP_CodeRate =FEC_NONE;
				else ctx->HP_CodeRate =FEC_AUTO;
				//Modulation
				if(! strcmp(mod, "QAM_128")) ctx->modulation = QAM_128;
				else if(! strcmp(mod, "QAM_256")) ctx->modulation = QAM_256;
				else if(! strcmp(mod, "QAM_64")) ctx->modulation = QAM_64;
				else if(! strcmp(mod, "QAM_32")) ctx->modulation = QAM_32;
				else if(! strcmp(mod, "QAM_16")) ctx->modulation = QAM_16;
				//Bandwidth
				if(! strcmp(bw, "BANDWIDTH_6_MHZ")) ctx->bandwidth = BANDWIDTH_6_MHZ;
				else if(! strcmp(bw, "BANDWIDTH_7_MHZ")) ctx->bandwidth = BANDWIDTH_7_MHZ;
				else if(! strcmp(bw, "BANDWIDTH_8_MHZ")) ctx->bandwidth = BANDWIDTH_8_MHZ;
				//Transmission Mode
				if(! strcmp(transm, "TRANSMISSION_MODE_2K")) ctx->TransmissionMode = TRANSMISSION_MODE_2K;
				else if(! strcmp(transm, "TRANSMISSION_MODE_8K")) ctx->TransmissionMode = TRANSMISSION_MODE_8K;
				//Guard Interval
				if(! strcmp(gi, "GUARD_INTERVAL_1_32")) ctx->guardInterval = GUARD_INTERVAL_1_32;
				else if(! strcmp(gi, "GUARD_INTERVAL_1_16")) ctx->guardInterval = GUARD_INTERVAL_1_16;
				else if(! strcmp(gi, "GUARD_INTERVAL_1_8")) ctx->guardInterval = GUARD_INTERVAL_1_8;
				else ctx->guardInterval = GUARD_INTERVAL_1_4;
				//Hierarchy
				if(! strcmp(hier, "HIERARCHY_1")) ctx->hierarchy = HIERARCHY_1;
				else if(! strcmp(hier, "HIERARCHY_2")) ctx->hierarchy = HIERARCHY_2;
				else if(! strcmp(hier, "HIERARCHY_4")) ctx->hierarchy = HIERARCHY_4;
				else if(! strcmp(hier, "HIERARCHY_AUTO")) ctx->hierarchy = HIERARCHY_AUTO;
				else ctx->hierarchy = HIERARCHY_NONE;

				break;
			}
#endif

		}
	}
	gf_fclose(chanfile);

	sprintf(frontend_name, "/dev/dvb/adapter%d/frontend0", adapter_num);
	sprintf(demux_name, "/dev/dvb/adapter%d/demux0", adapter_num);
	sprintf(dvr_name, "/dev/dvb/adapter%d/dvr0", adapter_num);

	// Open frontend
	if((front1 = open(frontend_name,O_RDWR|O_NONBLOCK)) < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Cannot open frontend %s.\n", frontend_name));
		return GF_IO_ERR;
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("Frontend %s opened.\n", frontend_name));
	}
	// Open demuxes
	if ((demux1=open(demux_name, O_RDWR|O_NONBLOCK)) < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Cannot open demux %s\n", demux_name));
		return GF_IO_ERR;
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Demux %s opened.\n", demux_name));
	}
	// Set FrontendParameters - DVB-T
#ifndef GPAC_SIM_LINUX_DVB
	frp.frequency = ctx->freq;
	frp.inversion = ctx->specInv;
	frp.u.ofdm.bandwidth = ctx->bandwidth;
	frp.u.ofdm.code_rate_HP = ctx->HP_CodeRate;
	frp.u.ofdm.code_rate_LP = ctx->LP_CodeRate;
	frp.u.ofdm.constellation = ctx->modulation;
	frp.u.ofdm.transmission_mode = ctx->TransmissionMode;
	frp.u.ofdm.guard_interval = ctx->guardInterval;
	frp.u.ofdm.hierarchy_information = ctx->hierarchy;
	// Set frontend
	if (ioctl(front1, FE_SET_FRONTEND, &frp) < 0) {
		return GF_IO_ERR;
	}

	// Set dumex
	pesFilterParams.pid      = 0x2000;				// Linux-DVB API take PID=2000 for FULL/RAW TS flag
	pesFilterParams.input    = DMX_IN_FRONTEND;
	pesFilterParams.output   = DMX_OUT_TS_TAP;
	pesFilterParams.pes_type = DMX_PES_OTHER;
	pesFilterParams.flags    = DMX_IMMEDIATE_START;
	if (ioctl(demux1, DMX_SET_PES_FILTER, &pesFilterParams) < 0) {
		return GF_IO_ERR;
	}
	/* The following code differs from mplayer and alike because the device is opened in blocking mode */
	if ((ctx->demux_fd = open(dvr_name, O_RDONLY/*|O_NONBLOCK*/)) < 0) {
		return GF_IO_ERR;
	}
#endif

	return GF_OK;
}

static u32 gf_dvblin_get_freq_from_url(GF_DVBLinuxCtx *ctx, const char *url)
{
	FILE *chcfgig_file;
	char line[255], *tmp, *channel_name;

	u32 freq;

	/* get rid of trailing @ */
	tmp = strchr(url, '@');
	if (tmp) tmp[0] = 0;

	channel_name = (char *)url+6;

	chcfgig_file = gf_fopen(ctx->chcfg, "rb");
	if (!chcfgig_file) return GF_BAD_PARAM;

	freq = 0;
	while(!feof(chcfgig_file)) {
		if ( fgets(line, 255, chcfgig_file) != NULL) {
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

GF_Err dvblin_setup_demux(GF_DVBLinuxCtx *ctx)
{
	GF_Err e = GF_OK;

	if (!ctx->chcfg) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[DVB4Lin] Missing channels config file\n"));
		return GF_BAD_PARAM;
	}
	if (strnicmp(ctx->src, "dvb://", 6)) return GF_NOT_SUPPORTED;

	if ((ctx->freq != 0) && (ctx->freq == gf_dvblin_get_freq_from_url(ctx, ctx->src)) ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[M2TSDemux] Tuner already tuned to that frequency\n"));
		return GF_OK;
	}

	e = dvblin_tune(ctx);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[M2TSDemux] Unable to tune to frequency\n"));
		return GF_SERVICE_ERROR;
	}
	return GF_OK;
}




GF_Err dvblin_initialize(GF_Filter *filter)
{
	GF_Err e = GF_OK;
	GF_DVBLinuxCtx *ctx = (GF_DVBLinuxCtx *) gf_filter_get_udta(filter);

	if (!ctx || !ctx->src) return GF_BAD_PARAM;
	e = dvblin_setup_demux(ctx);

	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[DVBLinux] Failed to open %s\n", ctx->src));
		gf_filter_setup_failure(filter, e);
		return GF_URL_ERROR;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("[DVBLinux] opening %s\n", ctx->src));

	ctx->block = gf_malloc(ctx->block_size +1);
	return GF_OK;
}

void dvblin_finalize(GF_Filter *filter)
{
	GF_DVBLinuxCtx *ctx = (GF_DVBLinuxCtx *) gf_filter_get_udta(filter);
#ifndef GPAC_SIM_LINUX_DVB
	if (ctx->demux_fd) close(ctx->demux_fd);
#endif
	if (ctx->block) gf_free(ctx->block);
}

GF_FilterProbeScore dvblin_probe_url(const char *url, const char *mime_type)
{
	if (!strnicmp(url, "dvb://", 6)) return GF_FPROBE_SUPPORTED;
	return GF_FPROBE_NOT_SUPPORTED;
}


static Bool dvblin_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_DVBLinuxCtx *ctx = (GF_DVBLinuxCtx *) gf_filter_get_udta(filter);

	if (!evt->base.on_pid) return GF_FALSE;
	if (evt->base.on_pid != ctx->pid) return GF_FALSE;

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		dvblin_setup_demux(ctx);
		return GF_TRUE;
	case GF_FEVT_STOP:
#ifndef GPAC_SIM_LINUX_DVB
		if (ctx->demux_fd) close(ctx->demux_fd);
		ctx->demux_fd = 0;
#endif
		return GF_TRUE;
	default:
		break;
	}
	return GF_FALSE;
}

static GF_Err dvblin_process(GF_Filter *filter)
{
	u32 nb_read=0;
	GF_FilterPacket *dst_pck;
	u8 *out_data;
	GF_DVBLinuxCtx *ctx = (GF_DVBLinuxCtx *) gf_filter_get_udta(filter);

	if (!ctx->freq) return GF_EOS;

#ifndef GPAC_SIM_LINUX_DVB
	nb_read = read(ctx->demux_fd, ctx->block, ctx->block_size);
#endif
	if (!nb_read) return GF_OK;

	dst_pck = gf_filter_pck_new_alloc(ctx->pid, nb_read, &out_data);
	memcpy(out_data, ctx->block, nb_read);
	gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_TRUE);
	gf_filter_pck_send(dst_pck);
	return GF_OK;
}



#define OFFS(_n)	#_n, offsetof(GF_DVBLinuxCtx, _n)

static const GF_FilterArgs DVBLinuxArgs[] =
{
	{ OFFS(src), "location of source content", GF_PROP_NAME, NULL, NULL, 0},
	{ OFFS(block_size), "block size used to read file", GF_PROP_UINT, "65536", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(chcfg), "path to channels.conf file", GF_PROP_NAME, NULL, NULL, 0},
	{}
};

GF_FilterRegister DVBLinuxRegister = {
	.name = "dvbin",
	GF_FS_SET_DESCRIPTION("DVB for Linux")
	GF_FS_SET_HELP("Experimental DVB support for linux, requires a channel config file through [-chcfg]()")
	.private_size = sizeof(GF_DVBLinuxCtx),
	.args = DVBLinuxArgs,
	.initialize = dvblin_initialize,
	.finalize = dvblin_finalize,
	.process = dvblin_process,
	.process_event = dvblin_process_event,
	.probe_url = dvblin_probe_url
};

#endif

const GF_FilterRegister *dvblin_register(GF_FilterSession *session)
{
#if defined(GPAC_HAS_LINUX_DVB) && !defined(GPAC_SIM_LINUX_DVB)
	return &DVBLinuxRegister;
#else
	return NULL;
#endif

}

