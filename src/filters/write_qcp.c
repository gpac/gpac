/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2018
 *					All rights reserved
 *
 *  This file is part of GPAC / QCP stream to file filter
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

#include <gpac/internal/media_dev.h>


typedef struct
{
	//opts
	Bool exporter, mpeg2;

	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;

	u32 codecid;
	Bool first;

	GF_Fraction duration;

	char GUID[16];
	u32 qcp_type, needs_rate_byte;
	QCPRateTable rtable[8];
	unsigned int *qcp_rates, rt_cnt;	/*contains constants*/
	Bool has_qcp_pad;
	Bool needs_final_pach;
	u32 data_size;
	u32 nb_frames;

} GF_QCPMxCtx;


/*QCP codec GUIDs*/
static const char *QCP_QCELP_GUID_1 = "\x41\x6D\x7F\x5E\x15\xB1\xD0\x11\xBA\x91\x00\x80\x5F\xB4\xB9\x7E";
static const char *QCP_EVRC_GUID = "\x8D\xD4\x89\xE6\x76\x90\xB5\x46\x91\xEF\x73\x6A\x51\x00\xCE\xB4";
static const char *QCP_SMV_GUID = "\x75\x2B\x7C\x8D\x97\xA7\x46\xED\x98\x5E\xD5\x3C\x8C\xC7\x5F\x84";

GF_Err qcpmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 sr, chan, bps;
	const GF_PropertyValue *p;
	GF_QCPMxCtx *ctx = gf_filter_get_udta(filter);

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

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_NUM_CHANNELS);
	if (!p) return GF_NOT_SUPPORTED;
	chan = p->value.uint;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_AUDIO_FORMAT);
	bps = p ? gf_audio_fmt_bit_depth(p->value.uint) : 16;


	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE) );
	}

	switch (ctx->codecid) {
	case GF_CODECID_QCELP:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("qcp") );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("audio/qcp") );
		ctx->qcp_type = 1;
		memcpy(ctx->GUID, QCP_QCELP_GUID_1, sizeof(char)*16);
		break;
	case GF_CODECID_EVRC_PV:
	case GF_CODECID_EVRC:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("evc") );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("audio/qcp") );
		memcpy(ctx->GUID, QCP_EVRC_GUID, sizeof(char)*16);
		ctx->qcp_type = 3;
		break;
	case GF_CODECID_SMV:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("smv") );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("audio/qcp") );
		ctx->qcp_type = 2;
		memcpy(ctx->GUID, QCP_SMV_GUID, sizeof(char)*16);
		break;
	}

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("qcp") );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("audio/qcp") );
	ctx->first = GF_TRUE;

	if (ctx->exporter) {
		GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("Exporting %s - SampleRate %d %d channels %d bits per sample\n", gf_codecid_name(ctx->codecid), sr, chan, bps));
	}

	ctx->ipid = pid;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DURATION);
	if (p) ctx->duration = p->value.frac;

	gf_filter_pid_set_framing_mode(pid, GF_TRUE);

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_MEDIA_DATA_SIZE);
	if (!p) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[QCP] Unknown total media size, cannot write QCP file right away\n"));
		ctx->data_size = 0;
	} else {
		ctx->data_size = (u32) p->value.longuint;
	}
	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_NB_FRAMES);
	if (!p) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[QCP] Unknown total number of media frames, cannot write QCP file\n"));
		ctx->nb_frames = 0;
	} else {
		ctx->nb_frames = (u32) p->value.uint;
	}

	if (!ctx->data_size || !ctx->nb_frames) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DISABLE_PROGRESSIVE, &PROP_BOOL(GF_TRUE) );
		ctx->needs_final_pach = GF_TRUE;
	}

	return GF_OK;
}

static void qcpmx_send_header(GF_QCPMxCtx *ctx, u32 data_size, u32 frame_count)
{
	const GF_PropertyValue *p;
	Bool needs_rate_octet;
	char szName[80];
	u32 i, tot_size, size, sample_size, avg_rate;
	u32 block_size = 160;
	u32 sample_rate = 8000;
	GF_BitStream *bs;
	u8 *output;
	GF_FilterPacket *dst_pck;

	if (ctx->qcp_type==1) {
		ctx->qcp_rates = (unsigned int*)GF_QCELP_RATE_TO_SIZE;
		ctx->rt_cnt = GF_QCELP_RATE_TO_SIZE_NB;
	} else {
		ctx->qcp_rates = (unsigned int*)GF_SMV_EVRC_RATE_TO_SIZE;
		ctx->rt_cnt = GF_SMV_EVRC_RATE_TO_SIZE_NB;
	}

	/*dumps full table...*/
	for (i=0; i<ctx->rt_cnt; i++) {
		ctx->rtable[i].rate_idx = ctx->qcp_rates[2*i];
		ctx->rtable[i].pck_size = ctx->qcp_rates[2*i+1];
	}

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FRAME_SIZE);
	sample_size = p ? p->value.uint : block_size;

	/*check sample format - packetvideo doesn't include rate octet...*/
	needs_rate_octet = (ctx->codecid==GF_CODECID_EVRC_PV) ? GF_TRUE : GF_FALSE;

	if (needs_rate_octet) data_size += frame_count;
	ctx->has_qcp_pad = (data_size % 2) ? GF_TRUE : GF_FALSE;

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_BITRATE);
	if (p) avg_rate = p->value.uint;
	else avg_rate = frame_count ? 8*data_size*sample_rate/frame_count/block_size : 0;

	/*QLCM + fmt + vrat + data*/
	size = tot_size = 4+ 8+150 + 8+8 + 8 + data_size;
	/*pad is included in riff size*/
	if (ctx->has_qcp_pad) {
		tot_size++;
	}
	size += 8;
	size -= data_size;
	dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &output);
	bs = gf_bs_new(output, size, GF_BITSTREAM_WRITE);

	gf_bs_write_data(bs, "RIFF", 4);
	gf_bs_write_u32_le(bs, tot_size);
	gf_bs_write_data(bs, "QLCM", 4);
	gf_bs_write_data(bs, "fmt ", 4);
	gf_bs_write_u32_le(bs, 150);/*fmt chunk size*/
	gf_bs_write_u8(bs, 1);
	gf_bs_write_u8(bs, 0);
	gf_bs_write_data(bs, ctx->GUID, 16);
	gf_bs_write_u16_le(bs, 1);
	memset(szName, 0, 80);
	strcpy(szName, (ctx->qcp_type==1) ? "QCELP-GPACExport" : ((ctx->qcp_type==2) ? "SMV-GPACExport" : "EVRC-GPACExport"));
	gf_bs_write_data(bs, szName, 80);
	gf_bs_write_u16_le(bs, avg_rate);
	gf_bs_write_u16_le(bs, sample_size);
	gf_bs_write_u16_le(bs, block_size);
	gf_bs_write_u16_le(bs, sample_rate);
	gf_bs_write_u16_le(bs, avg_rate);
	gf_bs_write_u32_le(bs, ctx->rt_cnt);
	for (i=0; i<8; i++) {
		if (i<ctx->rt_cnt) {
			/*frame size MINUS rate octet*/
			gf_bs_write_u8(bs, ctx->rtable[i].pck_size - 1);
			gf_bs_write_u8(bs, ctx->rtable[i].rate_idx);
		} else {
			gf_bs_write_u16(bs, 0);
		}
	}
	memset(szName, 0, 80);
	gf_bs_write_data(bs, szName, 20);/*reserved*/
	gf_bs_write_data(bs, "vrat", 4);
	gf_bs_write_u32_le(bs, 8);/*vrat chunk size*/
	gf_bs_write_u32_le(bs, ctx->rt_cnt);
	gf_bs_write_u32_le(bs, frame_count);
	gf_bs_write_data(bs, "data", 4);
	gf_bs_write_u32_le(bs, data_size);/*data chunk size*/

	ctx->needs_rate_byte = needs_rate_octet ? 1 : 0;

	gf_bs_del(bs);

	if (!ctx->first) {
		assert(ctx->needs_final_pach);
		gf_filter_pck_set_framing(dst_pck, GF_FALSE, GF_FALSE);
		gf_filter_pck_set_seek_flag(dst_pck, GF_TRUE);
		gf_filter_pck_set_byte_offset(dst_pck, 0);
	} else {
		gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_FALSE);
		gf_filter_pck_set_byte_offset(dst_pck, GF_FILTER_NO_BO);
		ctx->first = GF_FALSE;
	}

	gf_filter_pck_send(dst_pck);
}

GF_Err qcpmx_process(GF_Filter *filter)
{
	GF_QCPMxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck;
	u8 *data, *output;
	u32 pck_size, size;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			if (ctx->needs_final_pach) {
				qcpmx_send_header(ctx, ctx->data_size, ctx->nb_frames);
				ctx->needs_final_pach = GF_FALSE;
			}
			if (ctx->has_qcp_pad) {
				dst_pck = gf_filter_pck_new_alloc(ctx->opid, 1, &output);
				output[0] = 0;
				gf_filter_pck_set_framing(dst_pck, GF_FALSE, GF_TRUE);
				ctx->has_qcp_pad = GF_FALSE;
				gf_filter_pck_send(dst_pck);
			}
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}

	if (ctx->first) {
		qcpmx_send_header(ctx, ctx->data_size, ctx->nb_frames);
	}

	data = (char *) gf_filter_pck_get_data(pck, &pck_size);

	size = pck_size;
	ctx->data_size += pck_size;
	ctx->nb_frames ++;

	/*fix rate octet for QCP*/
	if (ctx->needs_rate_byte) {
		u32 j;
		u32 rate_found = 0;
		size ++;

		for (j=0; j<ctx->rt_cnt; j++) {
			if (ctx->qcp_rates[2*j+1] == pck_size) {
				rate_found = ctx->qcp_rates[2*j];
				break;
			}
		}
		if (!rate_found) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[QCP] Frame size %d not in rate table, ignoring frame\n", pck_size));
			gf_filter_pid_drop_packet(ctx->ipid);
			return GF_NON_COMPLIANT_BITSTREAM;
		}

		dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &output);
		output[0] = rate_found;
		memcpy(output+1, data, pck_size);
	} else {
		dst_pck = gf_filter_pck_new_ref(ctx->opid, data, size, pck);
	}

	gf_filter_pck_merge_properties(pck, dst_pck);
	gf_filter_pck_set_byte_offset(dst_pck, GF_FILTER_NO_BO);

	gf_filter_pck_set_framing(dst_pck, ctx->first, GF_FALSE);
	ctx->first = GF_FALSE;

	gf_filter_pck_send(dst_pck);

	if (ctx->exporter) {
		u32 timescale = gf_filter_pck_get_timescale(pck);
		u64 ts = gf_filter_pck_get_cts(pck);
		gf_set_progress("Exporting", ts*ctx->duration.den, ctx->duration.num*timescale);
	}

	gf_filter_pid_drop_packet(ctx->ipid);

	return GF_OK;
}

static const GF_FilterCapability QCPMxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_QCELP),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_EVRC),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_EVRC_PV),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_SMV),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "qcp"),
};


#define OFFS(_n)	#_n, offsetof(GF_QCPMxCtx, _n)
static const GF_FilterArgs QCPMxArgs[] =
{
	{ OFFS(exporter), "compatibility with old exporter, displays export results", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{0}
};


GF_FilterRegister QCPMxRegister = {
	.name = "writeqcp",
	GF_FS_SET_DESCRIPTION("QCP writer")
	GF_FS_SET_HELP("This filter converts a single stream to a QCP output file.")
	.private_size = sizeof(GF_QCPMxCtx),
	.args = QCPMxArgs,
	SETCAPS(QCPMxCaps),
	.configure_pid = qcpmx_configure_pid,
	.process = qcpmx_process
};


const GF_FilterRegister *qcpmx_register(GF_FilterSession *session)
{
	return &QCPMxRegister;
}
