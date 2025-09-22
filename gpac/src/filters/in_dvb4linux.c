/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2025
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

#ifdef GPAC_HAS_LINUX_DVB

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/version.h>

typedef struct
{
	//options
	const char *src;
	const char *chcfg;
	u32 block_size, csleep;
	const char *dev;
	u32 idx, csidx, timeout;
	GF_PropStringList chans;

	//only one output pid declared
	GF_FilterPid *pid;

	Bool playing;
	u32 freq;
	u32 main_pid;
	int demux, frontend, demux_fd;

	char *block;
	u32 tune_start_time;

} GF_DVBLinuxCtx;

#define MAX_DEV_LEN	255

typedef struct
{
	u32 frequency;
	u32 main_pid;
	char adapter_path[MAX_DEV_LEN];
	u32 frontend_idx;

	fe_delivery_system_t sys_type;
	fe_spectral_inversion_t inversion;
	fe_code_rate_t fec;
	fe_modulation_t modulation;

	//DVB-T
	fe_bandwidth_t bandwidth;
	fe_code_rate_t fec_hp;
	fe_transmit_mode_t  transmission_mode;
	fe_guard_interval_t guard_interval;
	fe_hierarchy_t hierarchy;

	//DVB-S
	u32 dibseq_csidx;
	fe_rolloff_t rolloff;
	u32 symbol_rate;
	Bool pol_v_r;
	Bool hiband;
} GF_DVBParams;

static void dvblin_stop(GF_DVBLinuxCtx *ctx)
{
	if (ctx->demux_fd>=0) close(ctx->demux_fd);
	ctx->demux_fd = -1;
	if (ctx->frontend>=0) close(ctx->frontend);
	ctx->frontend = -1;
	if (ctx->demux>=0) close(ctx->demux);
	ctx->demux = -1;
	//reset freq as frontend is closed
	ctx->freq = 0;
}

static GF_Err dvblin_get_channel_params(GF_DVBLinuxCtx *ctx, char *chan_name, GF_DVBParams *params)
{
	char vpid_str[255], apid_str[255];
	Bool fwd_all=GF_FALSE, use_freq=GF_FALSE;
	u32 for_chan_idx=0, chan_idx=0;
	Bool freq_found = GF_FALSE;

	memset(params, 0, sizeof(GF_DVBParams));
	ctx->freq = 0;

	params->frontend_idx = ctx->idx;
	strncpy(params->adapter_path, ctx->dev, MAX_DEV_LEN-1);
	params->adapter_path[MAX_DEV_LEN-1] = 0;
	params->dibseq_csidx = ctx->csidx;

	FILE *chanfile = gf_fopen(ctx->chcfg, "rb");
	if (!chanfile) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DVB] Error opening channels configuration file\n"));
		return GF_IO_ERR;
	}
	if (chan_name[0]=='+') {
		chan_name++;
		fwd_all = GF_TRUE;
	}
	if (chan_name[0]=='@') {
		chan_name++;
		fwd_all = GF_TRUE;
		use_freq = GF_TRUE;
	}
	if (chan_name[0]=='=') {
		chan_name++;
		for_chan_idx = atoi(chan_name);
	}

	while (!gf_feof(chanfile)) {
		char line[255];
		if ( gf_fgets(line, 255, chanfile) == NULL) break;

		if (line[0]=='\r') continue;
		if (line[0]=='\n') continue;
		//parse commands
		if (line[0] == '#') {
			char *delim, *cfg = strstr(line, "dev=");
			if (cfg) {
				cfg+=4;
				delim = strchr(cfg, ' ');
				if (delim) delim[0] = 0;
				strncpy(params->adapter_path, cfg, MAX_DEV_LEN-1);
				params->adapter_path[MAX_DEV_LEN-1] = 0;
				if (delim) delim[0] = ' ';
			}
			cfg = strstr(line, "idx=");
			if (cfg) {
				cfg+=4;
				delim = strchr(cfg, ' ');
				if (delim) delim[0] = 0;
				params->frontend_idx = atoi(cfg);
				if (delim) delim[0] = ' ';
			}
			cfg = strstr(line, "csidx=");
			if (cfg) {
				cfg+=6;
				delim = strchr(cfg, ' ');
				if (delim) delim[0] = 0;
				params->dibseq_csidx = atoi(cfg);
				if (delim) delim[0] = ' ';
			}
			continue;
		}
		//extract channel name and frequency
		char *freq_str = strchr(line, ':');
		if (!freq_str) continue;
		freq_str[0] = 0;
		freq_str = freq_str+1;
		char *settings = strchr(freq_str, ':');
		if (!settings) continue;
		settings[0] = 0;
		settings++;

		u32 old_freq = ctx->freq;
		if (!gf_strict_atoui(freq_str, &ctx->freq)) continue;
		if (!ctx->freq) continue;
		//tune on channel or frequency index
		if (for_chan_idx) {
			if (use_freq) {
				if (ctx->freq == old_freq)
					continue;
				chan_idx++;
			} else {
				chan_idx++;
			}
			if (for_chan_idx > chan_idx) continue;
			if (for_chan_idx < chan_idx) break;
		}
		//tune on freq
		else if (use_freq) {
			if (strcmp(chan_name, freq_str)) continue;
		}
		//tune on name
		else {
			if (strnicmp(chan_name, line, strlen(chan_name))) continue;
		}

		//DVB-T
		if (!strncmp(settings, "INVERSION_", 10)) {
			u32 k;
			char inv[255], bw[255], lcr[255], hier[255], cr[255], mod[255], transm[255], gi[255];
			const char *chan_conf_dvbt = "%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:%255[^:]:";

			sscanf(settings, chan_conf_dvbt, inv, bw, lcr, cr, mod, transm, gi, hier, vpid_str, apid_str);

			//Inversion
			if(! strcmp(inv, "INVERSION_ON")) params->inversion = INVERSION_ON;
			else if(! strcmp(inv, "INVERSION_OFF")) params->inversion = INVERSION_OFF;
			else params->inversion = INVERSION_AUTO;

			for (k=0; k<2; k++) {
				u32 *p_cr = k ? &params->fec_hp : &params->fec;
				char *use_cr = k ? cr : lcr;

				if (!strcmp(use_cr, "FEC_1_2")) *p_cr = FEC_1_2;
				else if(!strcmp(use_cr, "FEC_2_3")) *p_cr = FEC_2_3;
				else if(!strcmp(use_cr, "FEC_3_4")) *p_cr = FEC_3_4;
				else if(!strcmp(use_cr, "FEC_4_5")) *p_cr = FEC_4_5;
				else if(!strcmp(use_cr, "FEC_6_7")) *p_cr = FEC_6_7;
				else if(!strcmp(use_cr, "FEC_8_9")) *p_cr = FEC_8_9;
				else if(!strcmp(use_cr, "FEC_5_6")) *p_cr = FEC_5_6;
				else if(!strcmp(use_cr, "FEC_7_8")) *p_cr = FEC_7_8;
				else if(!strcmp(use_cr, "FEC_NONE")) *p_cr = FEC_NONE;
				else *p_cr = FEC_AUTO;
			}
			//Modulation
			if(! strcmp(mod, "QAM_128")) params->modulation = QAM_128;
			else if(! strcmp(mod, "QAM_256")) params->modulation = QAM_256;
			else if(! strcmp(mod, "QAM_64")) params->modulation = QAM_64;
			else if(! strcmp(mod, "QAM_32")) params->modulation = QAM_32;
			else if(! strcmp(mod, "QAM_16")) params->modulation = QAM_16;
			//Bandwidth
			if(! strcmp(bw, "BANDWIDTH_6_MHZ")) params->bandwidth = BANDWIDTH_6_MHZ;
			else if(! strcmp(bw, "BANDWIDTH_7_MHZ")) params->bandwidth = BANDWIDTH_7_MHZ;
			else if(! strcmp(bw, "BANDWIDTH_8_MHZ")) params->bandwidth = BANDWIDTH_8_MHZ;
			//Transmission Mode
			if(! strcmp(transm, "TRANSMISSION_MODE_2K")) params->transmission_mode = TRANSMISSION_MODE_2K;
			else if(! strcmp(transm, "TRANSMISSION_MODE_8K")) params->transmission_mode = TRANSMISSION_MODE_8K;
			//Guard Interval
			if(! strcmp(gi, "GUARD_INTERVAL_1_32")) params->guard_interval = GUARD_INTERVAL_1_32;
			else if(! strcmp(gi, "GUARD_INTERVAL_1_16")) params->guard_interval = GUARD_INTERVAL_1_16;
			else if(! strcmp(gi, "GUARD_INTERVAL_1_8")) params->guard_interval = GUARD_INTERVAL_1_8;
			else params->guard_interval = GUARD_INTERVAL_1_4;
			//Hierarchy
			if(! strcmp(hier, "HIERARCHY_1")) params->hierarchy = HIERARCHY_1;
			else if(! strcmp(hier, "HIERARCHY_2")) params->hierarchy = HIERARCHY_2;
			else if(! strcmp(hier, "HIERARCHY_4")) params->hierarchy = HIERARCHY_4;
			else if(! strcmp(hier, "HIERARCHY_AUTO")) params->hierarchy = HIERARCHY_AUTO;
			else params->hierarchy = HIERARCHY_NONE;

			params->sys_type = SYS_DVBT;
			params->frequency = ctx->freq;
			freq_found = GF_TRUE;
			break;
		}

		//DVB-S
		if (!strncmp(settings, "v", 1) || !strncmp(settings, "h", 1)) {
			char pol, sat_name[255];
			u32 fec, mod, rolloff, dvbs_type, symbol_rate;
			const char *chan_conf_dvbs = "%cC%uM%uO%uS%d:%255[^:]:%u:%255[^:]:%255[^:]:\n";
			sscanf(settings, chan_conf_dvbs, &pol, &fec, &mod, &rolloff, &dvbs_type, sat_name, &symbol_rate, vpid_str, apid_str);

			GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DVB] S/S2 parameters: freq %u pol %c FEC %u mod %u rolloff %u rate %u stype %u\n",
				ctx->freq, pol, fec, mod, rolloff, symbol_rate, dvbs_type));

#if DVB_API_VERSION >= 5
			params->sys_type = dvbs_type ? SYS_DVBS2 : SYS_DVBS;
#else
			params->sys_type = SYS_DVBS;
#endif
			switch (pol) {
			case 'h':
			case 'l':
				params->pol_v_r = GF_FALSE;
				break;
			case 'v':
			case 'r':
				params->pol_v_r = GF_TRUE;
				break;
			default:
				continue;
			}
			switch (fec) {
			case 12: params->fec = FEC_1_2; break;
			case 23: params->fec = FEC_2_3; break;
			case 34: params->fec = FEC_3_4; break;
			case 35: params->fec = FEC_3_5; break;
			case 45: params->fec = FEC_4_5; break;
			case 56: params->fec = FEC_5_6; break;
			case 67: params->fec = FEC_6_7; break;
			case 78: params->fec = FEC_7_8; break;
			case 89: params->fec = FEC_8_9; break;
			case 910: params->fec = FEC_4_5; break;
			case 0: params->fec = FEC_NONE; break;
			default:
				continue;
			}
			switch (mod) {
			case 16: params->modulation = QAM_16; break;
			case 32: params->modulation = QAM_32; break;
			case 64: params->modulation = QAM_64; break;
			case 128: params->modulation = QAM_128; break;
			case 256: params->modulation = QAM_256; break;
#if 0
			case 512: params->modulation = QAM_512; break;
			case 1024: params->modulation = QAM_1024; break;
			case 4096: params->modulation = QAM_4096; break;
#endif
			case 998: params->modulation = QAM_AUTO; break;
			case 2: params->modulation = QPSK; break;
			case 5: params->modulation = PSK_8; break;
			case 6: params->modulation = APSK_16; break;
			case 7: params->modulation = APSK_32; break;
			case 10: params->modulation = VSB_8; break;
			case 11: params->modulation = VSB_16; break;
			case 12: params->modulation = DQPSK; break;
			default:
				continue;
			}

			switch (rolloff) {
			case 20: params->rolloff = ROLLOFF_20; break;
			case 25: params->rolloff = ROLLOFF_25; break;
			case 30: params->rolloff = ROLLOFF_35; break;
			default: params->rolloff = ROLLOFF_AUTO; break;
			}

			if (ctx->freq < 11700) {
				params->frequency = abs(ctx->freq*1000 - 9750000);
				params->hiband = GF_FALSE;
				//tone_mode = SEC_TONE_OFF;
			} else {
				params->frequency = (ctx->freq * 1000)-10600000;
				params->hiband = GF_TRUE;
				//tone_mode = SEC_TONE_ON;
			}
			params->symbol_rate = symbol_rate * 1000;
			params->inversion = INVERSION_AUTO;
			freq_found = GF_TRUE;
			break;
		}
		//todo, cable and ATSC - no test env yet
		//todo, other config formats ?
	}
	gf_fclose(chanfile);

	if (!freq_found) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DVB] Cannot find channel configuration for %s\n", ctx->src+6));
		ctx->freq = 0;
		return GF_URL_ERROR;
	}

	params->main_pid = 0;
	if (!vpid_str[0] && apid_str[0])
		strcpy(vpid_str, apid_str);

	if (!fwd_all && vpid_str[0]) {
		char *s = strchr(vpid_str, '=');
		if (s) s[0] = 0;
		s = strchr(vpid_str, '+');
		if (s) s[0] = 0;
		params->main_pid = (u16) atoi(vpid_str);
	}
	return GF_OK;
}

#if DVB_API_VERSION >= 5
#define MAX_DVB_PROPS	15

#define DVB_SET_PROP(_cmd, _val) {\
	gf_fatal_assert(num_dvb_props+1<MAX_DVB_PROPS); \
	dvb_props[num_dvb_props].cmd = _cmd;\
	dvb_props[num_dvb_props].u.data = _val; \
	num_dvb_props++;\
	}


static GF_Err send_diseqc(GF_DVBLinuxCtx *ctx, struct dvb_diseqc_master_cmd *cmd, fe_sec_voltage_t voltage, fe_sec_tone_mode_t tone_mode, fe_sec_mini_cmd_t mini_c)
{
	if (ioctl(ctx->frontend, FE_SET_TONE, SEC_TONE_OFF) < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DVB] Cannot set tone off\n"));
		return GF_IO_ERR;
	}

	if (ioctl(ctx->frontend, FE_SET_VOLTAGE, voltage) < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DVB] Cannot set voltage\n"));
		return GF_IO_ERR;
	}
	gf_sleep(ctx->csleep);

	if (ioctl(ctx->frontend, FE_DISEQC_SEND_MASTER_CMD, cmd) < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DVB] Cannot send diseqc master command\n"));
		return GF_IO_ERR;
	}
	gf_sleep(ctx->csleep);

	if (ioctl(ctx->frontend, FE_DISEQC_SEND_BURST, mini_c) < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DVB] Cannot send diseqc mini command\n"));
		return GF_IO_ERR;
	}
	gf_sleep(ctx->csleep);

	if (ioctl(ctx->frontend, FE_SET_TONE, tone_mode) < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DVB] Cannot set tone\n"));
		return GF_IO_ERR;
	}
	gf_sleep(ctx->csleep);
	return GF_OK;
}
#endif // DVB_API_VERSION >= 5


static GF_Err dvblin_tune(GF_DVBLinuxCtx *ctx)
{
	Bool is_path=GF_TRUE;
	char dev_path[GF_MAX_PATH];
	GF_DVBParams params;
#if DVB_API_VERSION >= 5
	struct dtv_property dvb_props[MAX_DVB_PROPS];
	u32 num_dvb_props = 0;
	struct dtv_properties dvb_cfg;
#else
	struct dvb_frontend_parameters frp;
	memset(&frp, 0, sizeof(struct dvb_frontend_parameters));
#endif



	GF_Err e = dvblin_get_channel_params(ctx, (char *) ctx->src+6, &params);
	if (e) return e;

	is_path = strchr(params.adapter_path, '/') ? GF_TRUE : GF_FALSE;

	sprintf(dev_path, "%s%s/frontend%d", is_path ? "" : "/dev/dvb/adapter", params.adapter_path, params.frontend_idx);

	// Open frontend
	if((ctx->frontend = open(dev_path,O_RDWR|O_NONBLOCK)) < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DVB] Cannot open frontend %s\n", dev_path));
		return GF_IO_ERR;
	}

#if DVB_API_VERSION >= 5
	DVB_SET_PROP(DTV_CLEAR, 0);
	DVB_SET_PROP(DTV_DELIVERY_SYSTEM, params.sys_type);
	DVB_SET_PROP(DTV_FREQUENCY, params.frequency);
	DVB_SET_PROP(DTV_MODULATION, params.modulation);
	DVB_SET_PROP(DTV_INVERSION, params.inversion);
#else
	frp.frequency = ctx->freq;
	frp.inversion = params.inversion;
#endif


	if ((params.sys_type == SYS_DVBS)
#if DVB_API_VERSION >= 5
		|| (params.sys_type == SYS_DVBS2)
#endif
	) {
#if DVB_API_VERSION >= 5
		GF_Err e;
		struct dvb_diseqc_master_cmd cmd;
		fe_sec_voltage_t voltage = params.pol_v_r ? SEC_VOLTAGE_13 : SEC_VOLTAGE_18;
		fe_sec_tone_mode_t tone_mode = params.hiband ? SEC_TONE_ON : SEC_TONE_OFF;
		fe_sec_mini_cmd_t mini_cmd = (params.dibseq_csidx % 2) ? SEC_MINI_B : SEC_MINI_A;

		cmd.msg_len = 4;
		cmd.msg[0] = 0xe0; //cmd type: first from master
		cmd.msg[1] = 0x10; //adress type: any
		cmd.msg[2] = 0x39; //for uncommitted switches
		cmd.msg[3] = 0xf0;

		e = send_diseqc(ctx, &cmd, voltage, tone_mode, mini_cmd);
		if (e) {
			dvblin_stop(ctx);
			return e;
		}

		cmd.msg_len = 4;
		cmd.msg[2] = 0x38; //for committed switches
		cmd.msg[3] = 0xf0;
		cmd.msg[3] |= (params.dibseq_csidx * 4) & 0x0f;
		cmd.msg[3] |= (voltage == SEC_VOLTAGE_18) ? 2 : 0;
		cmd.msg[3] |= (tone_mode == SEC_TONE_ON) ? 1 : 0;

		e = send_diseqc(ctx, &cmd, voltage, tone_mode, mini_cmd);
		if (e) {
			dvblin_stop(ctx);
			return e;
		}
fprintf(stderr, "Using csidx %u v18 %u toneOn %u freq %u (%u)\n", params.dibseq_csidx, (voltage == SEC_VOLTAGE_18) ? 1 : 0, (tone_mode == SEC_TONE_ON)  ? 1 : 0, ctx->freq, params.frequency);

		DVB_SET_PROP(DTV_SYMBOL_RATE, params.symbol_rate);
		DVB_SET_PROP(DTV_INNER_FEC, params.fec);
		DVB_SET_PROP(DTV_ROLLOFF, params.rolloff);
		DVB_SET_PROP(DTV_PILOT, PILOT_AUTO);
#else
		frp.u.qpsk.symbol_rate = params.symbol_rate;
		frp.u.qpsk.fec_inner = params.fec;
#endif
	}
	//DVB-T for now
	else if ((params.sys_type == SYS_DVBT)
#if DVB_API_VERSION >= 5
		|| (params.sys_type == SYS_DVBT2)
#endif
	) {
fprintf(stderr, "TNT using dev %s (%u) type %u freq %u mod %u bw %u fec %u %u tr %u guard %u hierachy %u\n",
	params.adapter_path, params.frontend_idx, params.sys_type, params.frequency, params.modulation, params.bandwidth,
	params.fec_hp, params.fec, params.transmission_mode, params.guard_interval, params.hierarchy
	);

#if DVB_API_VERSION >= 5
		u32 bw_hz = 0;
		if (params.bandwidth == BANDWIDTH_6_MHZ) bw_hz = 6000000;
		else if (params.bandwidth == BANDWIDTH_7_MHZ) bw_hz = 7000000;
		else if (params.bandwidth == BANDWIDTH_8_MHZ) bw_hz = 8000000;

		DVB_SET_PROP(DTV_BANDWIDTH_HZ, bw_hz);
		DVB_SET_PROP(DTV_CODE_RATE_HP, params.fec_hp);
		DVB_SET_PROP(DTV_CODE_RATE_LP, params.fec);
		DVB_SET_PROP(DTV_TRANSMISSION_MODE, params.transmission_mode);
		DVB_SET_PROP(DTV_GUARD_INTERVAL, params.guard_interval);
		DVB_SET_PROP(DTV_HIERARCHY, params.hierarchy);
#else
		frp.u.ofdm.bandwidth = params.bandwidth;
		frp.u.ofdm.code_rate_HP = params.fec_hp;
		frp.u.ofdm.code_rate_LP = params.fec;
		frp.u.ofdm.constellation = params.modulation;
		frp.u.ofdm.transmission_mode = params.transmission_mode;
		frp.u.ofdm.guard_interval = params.guard_interval;
		frp.u.ofdm.hierarchy_information = params.hierarchy;
#endif
	} else {
		dvblin_stop(ctx);
		return GF_NOT_SUPPORTED;
	}

	int ret_val;
#if DVB_API_VERSION >= 5
	DVB_SET_PROP(DTV_TUNE, 0);
	dvb_cfg.num = num_dvb_props;
	dvb_cfg.props = dvb_props;
	ret_val = ioctl(ctx->frontend, FE_SET_PROPERTY, &dvb_cfg);
#else
	ret_val = ioctl(ctx->frontend, FE_SET_FRONTEND, &frp);
#endif
	if (ret_val < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DVB] Cannot tune frontend %s\n", dev_path));
		dvblin_stop(ctx);
		return GF_IO_ERR;
	}

	// Open demuxer
	sprintf(dev_path, "%s%s/demux%d", is_path ? "" : "/dev/dvb/adapter", params.adapter_path, params.frontend_idx);
	if ((ctx->demux = open(dev_path, O_RDWR|O_NONBLOCK)) < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DVB] Cannot open demux %s\n", dev_path));
		dvblin_stop(ctx);
		return GF_IO_ERR;
	}


	// Set filter to none (we always want the full TS for now)
	struct dmx_pes_filter_params pes_filter;
	memset(&pes_filter, 0, sizeof(struct dmx_pes_filter_params));
	pes_filter.pid      = 0x2000;
	pes_filter.input    = DMX_IN_FRONTEND;
	pes_filter.output   = DMX_OUT_TS_TAP;
	pes_filter.pes_type = DMX_PES_OTHER;
	pes_filter.flags    = DMX_IMMEDIATE_START;

	if (ioctl(ctx->demux, DMX_SET_PES_FILTER, &pes_filter) < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DVB] Cannot set pes filter params\n"));
		return GF_IO_ERR;
	}
	//open recorder
	sprintf(dev_path, "%s%s/dvr%d", is_path ? "" : "/dev/dvb/adapter", params.adapter_path, params.frontend_idx);
	if ((ctx->demux_fd = open(dev_path, O_RDONLY|O_NONBLOCK)) < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DVB] Cannot open demux file descriptor %s\n", dev_path));
		dvblin_stop(ctx);
		return GF_IO_ERR;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[DVB] Tune-in done for demux %s\n", dev_path));
	ctx->tune_start_time = gf_sys_clock();
	return GF_OK;
}

#if 0
static u32 gf_dvblin_get_freq_from_url(GF_DVBLinuxCtx *ctx, const char *url)
{
	FILE *chc_file;
	char line[255], *tmp, *channel_name;
	u32 freq;

	channel_name = (char *)url+6;

	chc_file = gf_fopen(ctx->chcfg, "rb");
	if (!chc_file) return 0;

	freq = 0;
	while(!gf_feof(chc_file)) {
		if ( gf_fgets(line, 255, chc_file) != NULL) {
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
	gf_fclose(chc_file);
	return freq;
}
#endif

GF_Err dvblin_setup_demux(GF_DVBLinuxCtx *ctx)
{
	GF_Err e = GF_OK;

#if 0
	//not supported yet
	if ((ctx->freq != 0) && (ctx->freq == gf_dvblin_get_freq_from_url(ctx, ctx->src)) ) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DVB] Tuner already tuned to that frequency\n"));
		return GF_OK;
	}
#endif

	e = dvblin_tune(ctx);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DVB] Unable to tune to frequency\n"));
		return GF_SERVICE_ERROR;
	}
	return GF_OK;
}


void dvblin_finalize(GF_Filter *filter)
{
	GF_DVBLinuxCtx *ctx = (GF_DVBLinuxCtx *) gf_filter_get_udta(filter);
	dvblin_stop(ctx);
	if (ctx->block) gf_free(ctx->block);
}

static Bool dvblin_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_DVBLinuxCtx *ctx = (GF_DVBLinuxCtx *) gf_filter_get_udta(filter);

	if (!evt->base.on_pid) return GF_FALSE;
	if (evt->base.on_pid != ctx->pid) return GF_FALSE;

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if (ctx->playing) return GF_TRUE;
		dvblin_setup_demux(ctx);
		ctx->playing = GF_TRUE;
		gf_filter_post_process_task(filter);
		return GF_TRUE;
	case GF_FEVT_STOP:
		dvblin_stop(ctx);
		ctx->playing = GF_FALSE;
		return GF_TRUE;
	default:
		break;
	}
	return GF_FALSE;
}

static GF_Err dvblin_process(GF_Filter *filter)
{
	GF_FilterPacket *dst_pck;
	Bool first;
	u32 nb_pck=50;
	u8 *out_data;
	GF_DVBLinuxCtx *ctx = (GF_DVBLinuxCtx *) gf_filter_get_udta(filter);

	if (!ctx->freq || !ctx->playing) return GF_EOS;

restart:
	first=GF_FALSE;

	s32 nb_read = read(ctx->demux_fd, ctx->block, ctx->block_size);
	if (nb_read<=0) {
		if ((errno == EAGAIN) || (errno==EINTR)) {
			if (ctx->tune_start_time && (gf_sys_clock() - ctx->tune_start_time >= ctx->timeout)) {
				gf_filter_setup_failure(filter, GF_IP_UDP_TIMEOUT);
				dvblin_stop(ctx);
				return GF_EOS;
			}

			gf_filter_ask_rt_reschedule(filter, 5000);
			return GF_OK;
		}
		return GF_IO_ERR;
	}
	ctx->tune_start_time=0;
	if (!ctx->pid) {
		GF_Err e = gf_filter_pid_raw_new(filter, ctx->src, GF_FALSE, "video/mp2t", "ts", ctx->block, nb_read, GF_TRUE, &ctx->pid);
		if (e) {
			gf_filter_setup_failure(filter, e);
			return e;
		}
		first = GF_TRUE;
		if (ctx->main_pid) {
			gf_filter_pid_set_property_str(ctx->pid, "filter_pid", &PROP_UINT(ctx->main_pid));
		}
	}

	dst_pck = gf_filter_pck_new_alloc(ctx->pid, nb_read, &out_data);
	if (!dst_pck) return GF_OUT_OF_MEM;
	memcpy(out_data, ctx->block, nb_read);
	gf_filter_pck_set_framing(dst_pck, first, GF_FALSE);
	gf_filter_pck_send(dst_pck);

	nb_pck--;
	if (nb_pck)
		goto restart;
	return GF_OK;
}

GF_FilterProbeScore dvblin_probe_url(const char *url, const char *mime_type)
{
	if (!strnicmp(url, "dvb://", 6)) return GF_FPROBE_SUPPORTED;
	return GF_FPROBE_NOT_SUPPORTED;
}

static GF_Err dvbin_list_channels(GF_FilterRegister *for_reg, void *_for_ctx);

GF_Err dvblin_initialize(GF_Filter *filter)
{
	GF_DVBLinuxCtx *ctx = (GF_DVBLinuxCtx *) gf_filter_get_udta(filter);

	if (!ctx || !ctx->src) return GF_BAD_PARAM;
	if (strnicmp(ctx->src, "dvb://", 6)) return GF_BAD_PARAM;

	if (!gf_file_exists(ctx->chcfg)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DVB] Missing channel configuration file %s\n", ctx->chcfg));
		return GF_BAD_PARAM;
	}
	if (!stricmp(ctx->src, "dvb://@chlist")) {
		ctx->freq = 0;
		return dvbin_list_channels(NULL, ctx);
	}

	ctx->demux = ctx->frontend = ctx->demux_fd = -1;
	GF_Err e = dvblin_setup_demux(ctx);

	if (e) return e;

	ctx->block = gf_malloc(ctx->block_size +1);
	//auto play
	ctx->playing = GF_TRUE;
	return GF_OK;
}

#else
static GF_Err dvblin_process(GF_Filter *filter)
{
	return GF_EOS;
}
#endif //GPAC_HAS_LINUX_DVB

#ifdef GPAC_HAS_LINUX_DVB
#define OFFS(_n)	#_n, offsetof(GF_DVBLinuxCtx, _n)
#else
#define OFFS(_n)	#_n, -1
#endif

static const GF_FilterArgs DVBLinuxArgs[] =
{
	{ OFFS(src), "URL of source content", GF_PROP_NAME, NULL, NULL, 0},
	{ OFFS(block_size), "block size used to read device", GF_PROP_UINT, "65536", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(chcfg), "path to channels configuration file", GF_PROP_NAME, "$GCFG/channels.conf", NULL, 0},
	{ OFFS(dev), "path to DVB adapter - if first character is a number, this is the device index", GF_PROP_STRING, "0", NULL, 0},
	{ OFFS(idx), "frontend index", GF_PROP_UINT, "0", NULL, 0},
	{ OFFS(timeout), "timeout in ms before tune failure", GF_PROP_UINT, "5000", NULL, 0},
	{ OFFS(csleep), "config sleep in ms between DiSEqC commands", GF_PROP_UINT, "15", NULL, 0},
	{ OFFS(csidx), "committed switch index for DiSEqC", GF_PROP_UINT, "0", NULL, 0},
	{ OFFS(chans), "list of all channels, only pupulated for dvb://@chlist URL", GF_PROP_STRING_LIST, NULL, NULL, 0},
	{0}
};

static const GF_FilterCapability DVBLinuxCaps[] =
{
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
};

GF_FilterRegister DVBLinuxRegister = {
	.name = "dvbin",
	GF_FS_SET_DESCRIPTION("DVB for Linux")
	GF_FS_SET_HELP("This filter reads raw MPEG-2 TS from DVB-T/T2 and DVB-S/S2 cards on linux.\n"
	"\n"
	"The URL scheme used is `dvb://` with the following syntaxes:\n"
	"- `dvb://CHAN`: tunes to channel `CHAN` in the channel configuration file.\n"
	"- `dvb://+CHAN`: tunes to multiplex contaning channel `CHAN` and expose all programs.\n"
	"- `dvb://=N`: tunes to the `N-th` channel in the channel configuration file.\n"
	"- `dvb://@FREQ`: tunes to frequency `FREQ` and exposes all channels in multiplex.\n"
	"- `dvb://@=N`: tunes to `N-th` frequency and exposes all channels in multiplex.\n"
	"- `dvb://@chlist`: populates the [-chans]() option with available channels in the configuration file and do nothing else.\n"
	"\n"
	"When tuning by channel name `CHAN`, the first entry in the channel configuration file starting with `CHAN` will be used.\n"
	"\n"
	"The channel configuration file is set through [-chcfg](). The expected format is VDR as produced by `w_scan`, with a syntax extended for comment lines, starting with `#`.\n"
	"Within a comment line, the following keywords can be used to override defaults:\n"
	" - `dev=N`: set the adapter index (`N` integer) or full path (`N` string).\n"
	" - `idx=K`: set the frontend index `K`.\n"
	" - `csidx=S`: set the committed switch index for DiSEqC.\n"
	"\n"
	"To view the default channels, use `gpac -hx dvbin`.\n"
	)
	.args = DVBLinuxArgs,
	SETCAPS(DVBLinuxCaps),
#ifdef GPAC_HAS_LINUX_DVB
	.initialize = dvblin_initialize,
	.private_size = sizeof(GF_DVBLinuxCtx),
	.finalize = dvblin_finalize,
	.process = dvblin_process,
	.process_event = dvblin_process_event,
	.probe_url = dvblin_probe_url,
#else
	.process = dvblin_process,
#endif
	.hint_class_type = GF_FS_CLASS_MM_IO
};

#if !defined(GPAC_DISABLE_DOC)
void dvbin_cleanreg(GF_FilterSession *session, GF_FilterRegister *freg)
{
	gf_free((char*)freg->help);
}
#endif

#if defined(GPAC_HAS_LINUX_DVB) || !defined(GPAC_DISABLE_DOC)
static GF_Err dvbin_list_channels(GF_FilterRegister *for_reg, void *_for_ctx)
{
#if defined(GPAC_HAS_LINUX_DVB)
	GF_DVBLinuxCtx *for_ctx = _for_ctx;
#endif

	FILE *chcfg = NULL;
	if (for_reg) {
		//list all available channels
		char szPATH[GF_MAX_PATH];
		gf_sys_solve_path("$GCFG/channels.conf", szPATH);

		chcfg = gf_fopen(szPATH, "rb");
	}
#if defined(GPAC_HAS_LINUX_DVB)
	else {
		chcfg = gf_fopen(for_ctx->chcfg, "rb");
	}
#endif
	if (!chcfg) return GF_URL_ERROR;

	char *all_channels = NULL;
	while(!gf_feof(chcfg)) {
		char line[255];
		if ( gf_fgets(line, 255, chcfg) == NULL) break;
		if (line[0]=='#') continue;
		if (line[0]=='\r') continue;
		if (line[0]=='\n') continue;

		char *s1 = strchr(line, ':');
		if (!s1) continue;
		char *s2 = strchr(s1+1, ':');
		if (!s2) continue;
		s1[0] = s2[0] = 0;
		s2 = strchr(line, '|');
		if (s2) s2[0] = 0;
		s2 = strchr(line, ';');
		if (s2) s2[0] = 0;

		if (for_reg) {
			if (!all_channels) {
				all_channels = gf_strdup(DVBLinuxRegister.help);
				gf_dynstrcat(&all_channels, "\nAvailable channels in default config:\n", NULL);
			}
			gf_dynstrcat(&all_channels, "* ", NULL);
			gf_dynstrcat(&all_channels, line, NULL);
			gf_dynstrcat(&all_channels, " (freq ", NULL);
			gf_dynstrcat(&all_channels, s1+1, NULL);
			gf_dynstrcat(&all_channels, ")\n", NULL);
			continue;
		}

#if defined(GPAC_HAS_LINUX_DVB)
		for_ctx->chans.vals = gf_realloc(for_ctx->chans.vals, sizeof(char **)*(for_ctx->chans.nb_items+1));
		for_ctx->chans.vals[for_ctx->chans.nb_items] = gf_strdup(line);
		for_ctx->chans.nb_items++;
#endif
	}
	gf_fclose(chcfg);

	if (all_channels) {
		DVBLinuxRegister.help = all_channels;
		DVBLinuxRegister.register_free = dvbin_cleanreg;
	}
	return GF_OK;
}
#endif

const GF_FilterRegister *dvbin_register(GF_FilterSession *session)
{
#if !defined(GPAC_HAS_LINUX_DVB)
	if (!gf_opts_get_bool("temp", "gendoc") && !gf_opts_get_bool("temp", "helponly"))
		return NULL;
#ifdef GPAC_CONFIG_EMSCRIPTEN
	return NULL;
#endif
	DVBLinuxRegister.version = "! Warning: DVB4Linux NOT AVAILABLE IN THIS BUILD !";

#endif

	if (gf_opts_get_bool("temp", "get_proto_schemes")) {
		gf_opts_set_key("temp_in_proto", DVBLinuxRegister.name, "dvb");
	}
	if (!gf_opts_get_bool("temp", "helpexpert")) {
		return &DVBLinuxRegister;
	}

#ifndef GPAC_DISABLE_DOC
	dvbin_list_channels(&DVBLinuxRegister, NULL);
#endif

	return &DVBLinuxRegister;
}


