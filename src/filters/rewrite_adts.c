/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2017
 *					All rights reserved
 *
 *  This file is part of GPAC / AAC ADTS write filter
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
#include <gpac/bitstream.h>

#ifndef GPAC_DISABLE_AV_PARSERS
#include <gpac/avparse.h>
#endif


typedef struct
{
	//opts
	Bool exporter, mpeg2;

	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;

	u32 codecid, channels, sr_idx, aac_type;

	Bool is_latm;

	GF_BitStream *bs_w;

#ifndef GPAC_DISABLE_AV_PARSERS
	GF_M4ADecSpecInfo acfg;
#endif
	u32 dsi_crc;
	Bool update_dsi;
	GF_Fraction fdsi;
	u64 last_cts;
	u32 timescale;
} GF_ADTSMxCtx;




GF_Err adtsmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 i, sr;
	const GF_PropertyValue *p;
#ifndef GPAC_DISABLE_AV_PARSERS
	GF_M4ADecSpecInfo acfg;
#endif
	GF_ADTSMxCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		ctx->ipid = NULL;
		gf_filter_pid_remove(ctx->opid);
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!p) return GF_NOT_SUPPORTED;
	ctx->codecid = p->value.uint;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLE_RATE);
	if (!p) return GF_NOT_SUPPORTED;
	sr = p->value.uint;
	for (i=0; i<16; i++) {
		if (GF_M4ASampleRates[i] == (u32) sr) {
			ctx->sr_idx = i;
			break;
		}
	}
	ctx->channels = 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_NUM_CHANNELS);
	if (p)
		ctx->channels = p->value.uint;

#ifndef GPAC_DISABLE_AV_PARSERS
	memset(&acfg, 0, sizeof(GF_M4ADecSpecInfo));
#endif

	ctx->aac_type = 0;
	if (ctx->is_latm) {
		u32 crc;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
		if (!p) return GF_NOT_SUPPORTED;

		crc = gf_crc_32(p->value.data.ptr, p->value.data.size);
		if (crc != ctx->dsi_crc) {
			ctx->dsi_crc = crc;
#ifndef GPAC_DISABLE_AV_PARSERS
			gf_m4a_get_config(p->value.data.ptr, p->value.data.size, &ctx->acfg);
#endif
			ctx->update_dsi = GF_TRUE;
		}

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
		if (!p) return GF_NOT_SUPPORTED;
		ctx->timescale = p->value.uint;

	} else if (ctx->codecid==GF_CODECID_AAC_MPEG4) {
		if (!ctx->mpeg2) {
#ifndef GPAC_DISABLE_AV_PARSERS
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
			if (p) {
				gf_m4a_get_config(p->value.data.ptr, p->value.data.size, &acfg);
				ctx->aac_type = acfg.base_object_type - 1;
			} else {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("RFADTS] no AAC decoder config, assuming AAC-LC\n"));
				ctx->aac_type = GF_M4A_AAC_LC;
			}
		}
#endif
	} else {
		ctx->aac_type = ctx->codecid - GF_CODECID_AAC_MPEG2_MP;
	}

#ifndef GPAC_DISABLE_AV_PARSERS
	if (ctx->channels && acfg.nb_chan && (ctx->channels != acfg.nb_chan)) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("RFADTS] Mismatch betwwen container number of channels (%d) and AAC config (%d), using AAC config\n", ctx->channels, acfg.nb_chan));
		ctx->channels = acfg.nb_chan;
	}
#endif

	if (ctx->channels>7) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("RFADTS] Invalid channel config %d for ADTS, forcing 7\n", ctx->channels));
		ctx->channels = 7;
	} else if (!ctx->channels) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("RFADTS] no channel config found for ADTS, forcing stereo\n"));
	}

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
	}
	ctx->ipid = pid;
	gf_filter_pid_copy_properties(ctx->opid, pid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, &PROP_BOOL(GF_TRUE) );
	if (ctx->is_latm)
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED_LATM, &PROP_BOOL(GF_TRUE) );

	gf_filter_pid_set_framing_mode(ctx->ipid, GF_TRUE);
	return GF_OK;
}



GF_Err adtsmx_process(GF_Filter *filter)
{
	GF_ADTSMxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck;
	u8 *data, *output;
	u32 pck_size, size;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}

	data = (char *) gf_filter_pck_get_data(pck, &pck_size);

	if (ctx->is_latm) {
		u32 asize;

		size = pck_size+20;
		dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &output);

		if (!ctx->bs_w) ctx->bs_w = gf_bs_new(output, size, GF_BITSTREAM_WRITE);
		else gf_bs_reassign_buffer(ctx->bs_w, output, size);

		gf_bs_write_int(ctx->bs_w, 0x2B7, 11);
		gf_bs_write_int(ctx->bs_w, 0, 13);

		if (ctx->fdsi.num && ctx->fdsi.den) {
			u64 ts = gf_filter_pck_get_cts(pck);
			if (ts != GF_FILTER_NO_TS) {
				if ((ts - ctx->last_cts) * ctx->fdsi.den >= ctx->timescale * ctx->fdsi.num) {
					ctx->last_cts = ts;
					ctx->update_dsi = GF_TRUE;
				}
			}
		}

		/*same mux config = 0 (refresh aac config)*/
		if (ctx->update_dsi) {
			gf_bs_write_int(ctx->bs_w, 0, 1);
			/*mux config */
			gf_bs_write_int(ctx->bs_w, 0, 1);/*audio mux version = 0*/
			gf_bs_write_int(ctx->bs_w, 1, 1);/*allStreamsSameTimeFraming*/
			gf_bs_write_int(ctx->bs_w, 0, 6);/*numSubFrames*/
			gf_bs_write_int(ctx->bs_w, 0, 4);/*numProgram*/
			gf_bs_write_int(ctx->bs_w, 0, 3);/*numLayer prog 1*/

			gf_m4a_write_config_bs(ctx->bs_w, &ctx->acfg);

			gf_bs_write_int(ctx->bs_w, 0, 3);/*frameLengthType*/
			gf_bs_write_int(ctx->bs_w, 0, 8);/*latmBufferFullness*/
			gf_bs_write_int(ctx->bs_w, 0, 1);/*other data present*/
			gf_bs_write_int(ctx->bs_w, 0, 1);/*crcCheckPresent*/

			ctx->update_dsi = 0;
		} else {
			gf_bs_write_int(ctx->bs_w, 1, 1);
		}
		/*write payloadLengthInfo*/
		asize = pck_size;
		while (1) {
			if (asize>=255) {
				gf_bs_write_int(ctx->bs_w, 255, 8);
				asize -= 255;
			} else {
				gf_bs_write_int(ctx->bs_w, asize, 8);
				break;
			}
		}
		gf_bs_write_data(ctx->bs_w, data, pck_size);
		gf_bs_align(ctx->bs_w);

		size = (u32) gf_bs_get_position(ctx->bs_w);
		gf_filter_pck_truncate(dst_pck, size);

		/*rewrite LATM frame header*/
		output[1] |= ((size-3) >> 8 ) & 0x1F;
		output[2] = (size-3) & 0xFF;

	} else {
		size = pck_size+7;
		dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &output);

		if (!ctx->bs_w) ctx->bs_w = gf_bs_new(output, size, GF_BITSTREAM_WRITE);
		else gf_bs_reassign_buffer(ctx->bs_w, output, size);

		gf_bs_write_int(ctx->bs_w, 0xFFF, 12);/*sync*/
		gf_bs_write_int(ctx->bs_w, (ctx->mpeg2==1) ? 1 : 0, 1);/*mpeg2 aac*/
		gf_bs_write_int(ctx->bs_w, 0, 2); /*layer*/
		gf_bs_write_int(ctx->bs_w, 1, 1); /* protection_absent*/
		gf_bs_write_int(ctx->bs_w, ctx->aac_type, 2);
		gf_bs_write_int(ctx->bs_w, ctx->sr_idx, 4);
		gf_bs_write_int(ctx->bs_w, 0, 1);
		gf_bs_write_int(ctx->bs_w, ctx->channels, 3);
		gf_bs_write_int(ctx->bs_w, 0, 4);
		gf_bs_write_int(ctx->bs_w, 7+pck_size, 13);
		gf_bs_write_int(ctx->bs_w, 0x7FF, 11);
		gf_bs_write_int(ctx->bs_w, 0, 2);
		memcpy(output+7, data, pck_size);
	}

	gf_filter_pck_merge_properties(pck, dst_pck);
	gf_filter_pck_set_byte_offset(dst_pck, GF_FILTER_NO_BO);


	gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_TRUE);

	gf_filter_pck_send(dst_pck);
	gf_filter_pid_drop_packet(ctx->ipid);
	return GF_OK;
}

static void adtsmx_finalize(GF_Filter *filter)
{
	GF_ADTSMxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->bs_w) gf_bs_del(ctx->bs_w);
}

static const GF_FilterCapability ADTSMxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG4),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT_OPT, GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_MP),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT_OPT, GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_LCP),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT_OPT, GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_SSRP),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_BOOL(GF_CAPS_OUTPUT, GF_PROP_PID_UNFRAMED, GF_TRUE),
};


#define OFFS(_n)	#_n, offsetof(GF_ADTSMxCtx, _n)
static const GF_FilterArgs ADTSMxArgs[] =
{
	{ OFFS(mpeg2), "signal as MPEG2 AAC\n"
	"- auto: selects based on AAC profile\n"
	"- no: always signals as MPEG-4 AAC\n"
	"- yes: always signals as MPEG-2 AAC"
	"", GF_PROP_UINT, "auto", "auto|no|yes", GF_FS_ARG_HINT_ADVANCED},
	{0}
};


GF_FilterRegister ADTSMxRegister = {
	.name = "ufadts",
	GF_FS_SET_DESCRIPTION("ADTS writer")
	GF_FS_SET_HELP("This filter converts AAC streams into ADTS encapsulated data.")
	.private_size = sizeof(GF_ADTSMxCtx),
	.args = ADTSMxArgs,
	.finalize = adtsmx_finalize,
	SETCAPS(ADTSMxCaps),
	.configure_pid = adtsmx_configure_pid,
	.process = adtsmx_process
};


const GF_FilterRegister *adtsmx_register(GF_FilterSession *session)
{
	return &ADTSMxRegister;
}

static GF_Err latmmx_initialize(GF_Filter*filter)
{
	GF_ADTSMxCtx *ctx = gf_filter_get_udta(filter);
	ctx->is_latm = GF_TRUE;
	return GF_OK;
}

#define OFFS(_n)	#_n, offsetof(GF_ADTSMxCtx, _n)
static const GF_FilterArgs LATMMxArgs[] =
{
	{ OFFS(fdsi), "set delay between two LATM Audio Config", GF_PROP_FRACTION, "0", NULL, GF_FS_ARG_HINT_ADVANCED},
	{0}
};


static const GF_FilterCapability LATMMxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG4),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_MP),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_LCP),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_SSRP),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_BOOL(GF_CAPS_OUTPUT, GF_PROP_PID_UNFRAMED_LATM, GF_TRUE),
};

GF_FilterRegister LATMMxRegister = {
	.name = "uflatm",
	GF_FS_SET_DESCRIPTION("Raw AAC to LATM writer")
	GF_FS_SET_HELP("This filter converts AAC streams into LATM encapsulated data.")
	.private_size = sizeof(GF_ADTSMxCtx),
	.args = LATMMxArgs,
	.initialize = latmmx_initialize,
	.finalize = adtsmx_finalize,
	SETCAPS(LATMMxCaps),
	.configure_pid = adtsmx_configure_pid,
	.process = adtsmx_process
};


const GF_FilterRegister *latm_mx_register(GF_FilterSession *session)
{
	return &LATMMxRegister;
}
