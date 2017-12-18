/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2017
 *					All rights reserved
 *
 *  This file is part of GPAC / generic stream write filter
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
	Bool exporter, rcfg;

	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;

	u32 codecid;
	Bool split_files;
	u32 sample_num;

	const char *dcfg;
	u32 dcfg_size;

	GF_Fraction duration;
	Bool first;
} GF_GenDumpCtx;




GF_Err gendump_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 cid, chan, sr, bps, w, h;
	const char *name;
	const GF_PropertyValue *p;
	GF_GenDumpCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		ctx->ipid = NULL;
		gf_filter_pid_remove(ctx->opid);
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!p) return GF_NOT_SUPPORTED;
	cid = p->value.uint;

	if (ctx->codecid == cid) {
		return GF_OK;
	}
	ctx->codecid = cid;

	if (!ctx->opid)
		ctx->opid = gf_filter_pid_new(filter);

	ctx->ipid = pid;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLE_RATE);
	sr = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_NUM_CHANNELS);
	chan = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_BPS);
	bps = p ? p->value.uint : 16;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
	w = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
	h = p ? p->value.uint : 0;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (p) {
		ctx->dcfg = p->value.data.ptr;
		ctx->dcfg_size = p->value.data.size;
	}

	name = gf_codecid_name(cid);
	if (ctx->exporter) {
		if (w && h) {
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("Exporting %s - Size %dx%d\n", name, w, h));
		} else if (sr && chan) {
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("Exporting %s - SampleRate %d %d channels %d bits per sample\n", name, sr, chan, bps));
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("Exporting %s\n", name));
		}
	}

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE) );
	switch (cid) {
	case GF_CODECID_PNG:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("png") );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("image/png") );
		ctx->split_files = GF_TRUE;
		break;
	case GF_CODECID_JPEG:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("jpg") );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("image/jpeg") );
		ctx->split_files = GF_TRUE;
		break;
	case GF_CODECID_J2K:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("jp2") );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("image/jp2") );
		ctx->split_files = GF_TRUE;
		break;
	case GF_CODECID_MPEG_AUDIO:
	case GF_CODECID_MPEG2_PART3:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("mp3") );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("audio/mp3") );
		break;
	case GF_CODECID_AMR:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("amr") );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("audio/amr") );
		ctx->dcfg = "#!AMR\n";
		ctx->dcfg_size = 6;
		ctx->rcfg = GF_FALSE;
		break;
	case GF_CODECID_AMR_WB:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("amr") );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("audio/amr") );
		ctx->dcfg = "#!AMR-WB\n";
		ctx->dcfg_size = 9;
		ctx->rcfg = GF_FALSE;
		break;
	case GF_CODECID_SMV:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("smv") );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("audio/smv") );
		ctx->dcfg = "#!SMV\n";
		ctx->dcfg_size = 6;
		ctx->rcfg = GF_FALSE;
		break;
	case GF_CODECID_EVRC_PV:
	case GF_CODECID_EVRC:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("evc") );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("audio/evrc") );
		ctx->dcfg = "#!EVRC\n";
		ctx->dcfg_size = 7;
		ctx->rcfg = GF_FALSE;
		break;

	case GF_CODECID_AC3:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("ac3") );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("audio/ac3") );
		break;
	case GF_CODECID_EAC3:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("eac3") );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("audio/eac3") );
		break;

	case GF_CODECID_FLASH:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("swf") );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("application/swf") );
		break;

	case GF_CODECID_H263:
	case GF_CODECID_S263:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("263") );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("video/h263") );
		break;
	case GF_CODECID_MPEG4_PART2:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("cmp") );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("video/mp4v-es") );
		break;
	case GF_CODECID_MPEG1:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("m1v") );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("video/mp1v-es") );
		break;
	case GF_CODECID_MPEG2_422:
	case GF_CODECID_MPEG2_SNR:
	case GF_CODECID_MPEG2_HIGH:
	case GF_CODECID_MPEG2_MAIN:
	case GF_CODECID_MPEG2_SIMPLE:
	case GF_CODECID_MPEG2_SPATIAL:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("m2v") );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("video/mp2v-es") );
		break;

	case GF_CODECID_SIMPLE_TEXT:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("txt") );
		if (!gf_filter_pid_get_property(pid, GF_PROP_PID_MIME))
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("text/subtitle") );
		break;
	case GF_CODECID_META_TEXT:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("txt") );
		if (!gf_filter_pid_get_property(pid, GF_PROP_PID_MIME))
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("application/text") );
		break;
	case GF_CODECID_META_XML:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("xml") );
		if (!gf_filter_pid_get_property(pid, GF_PROP_PID_MIME))
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("application/text+xml") );
		break;
	case GF_CODECID_SUBS_TEXT:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("txt") );
		if (!gf_filter_pid_get_property(pid, GF_PROP_PID_MIME))
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("text/text") );
		break;
	case GF_CODECID_SUBS_XML:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("xml") );
		if (!gf_filter_pid_get_property(pid, GF_PROP_PID_MIME))
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("text/text+xml") );

		GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("XML Sub streams reaggregation not supported in dumper, dumping all samples\n", name, w, h));
		ctx->split_files = GF_TRUE;
		break;

	default:
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("und") );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("application/octet-string") );
		break;
	}
	ctx->first = GF_TRUE;
	if (ctx->split_files) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_NB_FRAMES);
		if (!p || (p->value.uint>1))
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PCK_FILENUM, &PROP_UINT(0) );
		else
			ctx->split_files = GF_FALSE;
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DURATION);
	if (p) ctx->duration = p->value.frac;
	return GF_OK;
}

GF_Err gendump_process(GF_Filter *filter)
{
	GF_GenDumpCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck;
	char *data;
	u32 pck_size;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}
	ctx->sample_num++;

	if (ctx->dcfg_size && gf_filter_pck_get_sap(pck)) {
		dst_pck = gf_filter_pck_new_shared(ctx->opid, ctx->dcfg, ctx->dcfg_size, NULL);
		gf_filter_pck_merge_properties(pck, dst_pck);
		gf_filter_pck_set_framing(dst_pck, ctx->first, GF_FALSE);
		ctx->first = GF_FALSE;
		gf_filter_pck_send(dst_pck);
		if (!ctx->rcfg && !ctx->split_files) {
			ctx->dcfg_size = 0;
			ctx->dcfg = NULL;
		}
	}
	data = (char *) gf_filter_pck_get_data(pck, &pck_size);

	dst_pck = gf_filter_pck_new_ref(ctx->opid, data, pck_size, pck);
	gf_filter_pck_merge_properties(pck, dst_pck);
	//keep byte offset
//	gf_filter_pck_set_byte_offset(dst_pck, GF_FILTER_NO_BO);

	if (ctx->split_files) {
		gf_filter_pck_set_property(dst_pck, GF_PROP_PCK_FILENUM, &PROP_UINT(ctx->sample_num) );
		gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_TRUE);
	} else {
		gf_filter_pck_set_framing(dst_pck, ctx->first, GF_FALSE);
		ctx->first = GF_FALSE;
	}

	gf_filter_pck_send(dst_pck);

	if (ctx->exporter) {
		u32 timescale = gf_filter_pck_get_timescale(pck);
		u64 ts = gf_filter_pck_get_cts(pck);
		gf_set_progress("Exporting", ts*ctx->duration.den, ctx->duration.num*timescale);
	}

	gf_filter_pid_drop_packet(ctx->ipid);

	return GF_OK;
}

static const GF_FilterCapability GenDumpInputs[] =
{
	CAP_EXC_BOOL(GF_PROP_PID_UNFRAMED, GF_TRUE),
	//we include everything, only specifies excluded ones

	//AAC needs rewrite
	CAP_EXC_UINT(GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG4),
	CAP_EXC_UINT(GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_MP),
	CAP_EXC_UINT(GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_LCP),
	CAP_EXC_UINT(GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_SSRP),

	//NALU needs rewrite
	CAP_EXC_UINT(GF_PROP_PID_CODECID, GF_CODECID_AVC),
	CAP_EXC_UINT(GF_PROP_PID_CODECID, GF_CODECID_AVC_PS),
	CAP_EXC_UINT(GF_PROP_PID_CODECID, GF_CODECID_SVC),
	CAP_EXC_UINT(GF_PROP_PID_CODECID, GF_CODECID_MVC),
	CAP_EXC_UINT(GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_EXC_UINT(GF_PROP_PID_CODECID, GF_CODECID_LHVC),
	//systems streams not dumpable to file
	CAP_EXC_UINT(GF_PROP_PID_CODECID, GF_CODECID_BIFS),
	CAP_EXC_UINT(GF_PROP_PID_CODECID, GF_CODECID_BIFS_V2),
	CAP_EXC_UINT(GF_PROP_PID_CODECID, GF_CODECID_OD_V1),
	CAP_EXC_UINT(GF_PROP_PID_CODECID, GF_CODECID_OD_V2),
	//this one need QCP
	CAP_EXC_UINT(GF_PROP_PID_CODECID, GF_CODECID_QCELP),

	//other not supported (yet)
	CAP_EXC_UINT(GF_PROP_PID_CODECID, GF_CODECID_THEORA),
	CAP_EXC_UINT(GF_PROP_PID_CODECID, GF_CODECID_VORBIS),
};


static const GF_FilterCapability GenDumpOutputs[] =
{
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	{}
};

#define OFFS(_n)	#_n, offsetof(GF_GenDumpCtx, _n)
static const GF_FilterArgs GenDumpArgs[] =
{
	{ OFFS(exporter), "compatibility with old exporter, displays export results", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(rcfg), "Force repeating decoder config at each I-frame", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{}
};


GF_FilterRegister GenDumpRegister = {
	.name = "write_gen",
	.description = "Generic single stream output",
	.private_size = sizeof(GF_GenDumpCtx),
	.args = GenDumpArgs,
	INCAPS(GenDumpInputs),
	OUTCAPS(GenDumpOutputs),
	.configure_pid = gendump_configure_pid,
	.process = gendump_process
};


const GF_FilterRegister *gendump_register(GF_FilterSession *session)
{
	return &GenDumpRegister;
}
