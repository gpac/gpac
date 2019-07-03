/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2018
 *					All rights reserved
 *
 *  This file is part of GPAC / NHNT stream to file filter
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

#include <gpac/internal/isomedia_dev.h>


typedef struct
{
	//opts
	Bool exporter, large;

	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid_nhnt, *opid_mdia, *opid_info;

	u32 codecid;
	u32 streamtype;
	u32 oti;

	const char *dcfg;
	u32 dcfg_size;

	GF_Fraction duration;
	Bool first;

	GF_BitStream *bs;
	u64 mdia_pos;
} GF_NHNTDumpCtx;

GF_Err nhntdump_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 cid, chan, sr, bps, w, h;
	const char *name;
	const GF_PropertyValue *p;
	GF_NHNTDumpCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		ctx->ipid = NULL;
		gf_filter_pid_remove(ctx->opid_nhnt);
		gf_filter_pid_remove(ctx->opid_mdia);
		if (ctx->opid_info) gf_filter_pid_remove(ctx->opid_info);
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

	if (ctx->codecid<GF_CODECID_LAST_MPEG4_MAPPING) ctx->oti = ctx->codecid;
	else {
		ctx->oti = gf_codecid_oti(ctx->codecid);
	}
	if (!ctx->oti) {
		GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("CodecID %s has no mapping to MPEG-4 systems, cannot use NHNT. Use NHML instead\n", gf_4cc_to_str(cid) ));
		return GF_NOT_SUPPORTED;
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	ctx->streamtype = p ? p->value.uint : GF_STREAM_UNKNOWN;

	if (!ctx->opid_nhnt)
		ctx->opid_nhnt = gf_filter_pid_new(filter);

	if (!ctx->opid_mdia)
		ctx->opid_mdia = gf_filter_pid_new(filter);

	ctx->ipid = pid;


	p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLE_RATE);
	sr = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_NUM_CHANNELS);
	chan = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_AUDIO_FORMAT);
	bps = p ? gf_audio_fmt_bit_depth(p->value.uint) : 16;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
	w = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
	h = p ? p->value.uint : 0;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (p) {
		ctx->dcfg = p->value.data.ptr;
		ctx->dcfg_size = p->value.data.size;

		if (!ctx->opid_info)
			ctx->opid_info = gf_filter_pid_new(filter);

	} else if (ctx->opid_info) {
		if (ctx->opid_info) gf_filter_pid_remove(ctx->opid_info);
	}

	name = gf_codecid_name(ctx->codecid);
	if (ctx->exporter) {
		if (w && h) {
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("Exporting %s - Size %dx%d\n", name, w, h));
		} else if (sr && chan) {
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("Exporting %s - SampleRate %d %d channels %d bits per sample\n", name, sr, chan, bps));
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("Exporting %s\n", name));
		}
	}

	gf_filter_pid_set_property(ctx->opid_nhnt, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE) );
	gf_filter_pid_set_property(ctx->opid_nhnt, GF_PROP_PID_FILE_EXT, &PROP_STRING("nhnt") );
	gf_filter_pid_set_property(ctx->opid_nhnt, GF_PROP_PID_MIME, &PROP_STRING("application/x-nhnt") );

	gf_filter_pid_set_property(ctx->opid_mdia, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE) );
	gf_filter_pid_set_property(ctx->opid_mdia, GF_PROP_PID_FILE_EXT, &PROP_STRING("media") );
	gf_filter_pid_set_property(ctx->opid_mdia, GF_PROP_PID_MIME, &PROP_STRING("application/x-nhnt") );

	if (ctx->opid_info) {
		gf_filter_pid_set_property(ctx->opid_info, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE) );
		gf_filter_pid_set_property(ctx->opid_info, GF_PROP_PID_FILE_EXT, &PROP_STRING("info") );
		gf_filter_pid_set_property(ctx->opid_info, GF_PROP_PID_MIME, &PROP_STRING("application/x-nhnt") );
	}

	ctx->first = GF_TRUE;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DURATION);
	if (p) ctx->duration = p->value.frac;
	gf_filter_pid_set_framing_mode(pid, GF_TRUE);
	return GF_OK;
}

GF_Err nhntdump_process(GF_Filter *filter)
{
	GF_NHNTDumpCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck;
	u8 *data;
	u8 *output;
	u32 size, pck_size;
	u64 dts, cts;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			gf_filter_pid_set_eos(ctx->opid_nhnt);
			gf_filter_pid_set_eos(ctx->opid_mdia);
			if (ctx->opid_info) gf_filter_pid_set_eos(ctx->opid_info);
			return GF_EOS;
		}
		return GF_OK;
	}

	if (ctx->first) {
		u32 nhnt_hdr_size = 4+1+1+1+2+3+4+4+4;
		const GF_PropertyValue *p;

		dst_pck = gf_filter_pck_new_alloc(ctx->opid_nhnt, nhnt_hdr_size, &output);
		if (!ctx->bs) ctx->bs = gf_bs_new(output, nhnt_hdr_size, GF_BITSTREAM_WRITE);
		else gf_bs_reassign_buffer(ctx->bs, output, nhnt_hdr_size);

		/*write header*/
		/*'NHnt' format*/
		gf_bs_write_data(ctx->bs, ctx->large ? "NHnl" : "NHnt", 4);
		/*version 1*/
		gf_bs_write_u8(ctx->bs, 0);
		/*streamType*/
		gf_bs_write_u8(ctx->bs, ctx->streamtype);
		/*OTI*/
		gf_bs_write_u8(ctx->bs, ctx->oti);
		/*reserved*/
		gf_bs_write_u16(ctx->bs, 0);
		/*bufferDB*/
		//p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_DB_SIZE);
		gf_bs_write_u24(ctx->bs, 0);
		/*avg BitRate*/
		p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_BITRATE);
		gf_bs_write_u32(ctx->bs, p ? p->value.uint : 0);
		/*max bitrate*/
		//p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_MAX_RATE);
		gf_bs_write_u32(ctx->bs, 0);
		/*timescale*/
		p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_TIMESCALE);
		gf_bs_write_u32(ctx->bs, p ? p->value.uint : 1000);

		gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_FALSE);
		gf_filter_pck_send(dst_pck);

		if (ctx->opid_info) {
			dst_pck = gf_filter_pck_new_shared(ctx->opid_info, ctx->dcfg, ctx->dcfg_size, NULL);
			gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_TRUE);
			gf_filter_pck_set_readonly(dst_pck);
			gf_filter_pck_send(dst_pck);
		}
	}

	//get media data
	data = (char *) gf_filter_pck_get_data(pck, &pck_size);

	//nhnt data size
	size = 3 + 1 + 3*(ctx->large ? 8 : 4);
	dst_pck = gf_filter_pck_new_alloc(ctx->opid_nhnt, size, &output);
	//send nhnt data
	gf_bs_reassign_buffer(ctx->bs, output, size);

	/*dump nhnt info*/
	gf_bs_write_u24(ctx->bs, pck_size);
	gf_bs_write_int(ctx->bs, gf_filter_pck_get_sap(pck) ? 1 : 0, 1);
	/*AU start & end flag always true*/
	gf_bs_write_int(ctx->bs, 1, 1);
	gf_bs_write_int(ctx->bs, 1, 1);
	/*reserved*/
	gf_bs_write_int(ctx->bs, 0, 3);
	gf_bs_write_int(ctx->bs, gf_filter_pck_get_sap(pck) ? 0 : 1, 2);

	dts = gf_filter_pck_get_dts(pck);
	cts = gf_filter_pck_get_cts(pck);
	if (ctx->large) {
		gf_bs_write_u64(ctx->bs, ctx->mdia_pos);
		gf_bs_write_u64(ctx->bs, cts);
		gf_bs_write_u64(ctx->bs, dts);
	} else {
		gf_bs_write_u32(ctx->bs, (u32) ctx->mdia_pos);
		gf_bs_write_u32(ctx->bs, (u32) cts);
		gf_bs_write_u32(ctx->bs, (u32) dts);
	}
	ctx->mdia_pos += pck_size;
	gf_filter_pck_set_framing(dst_pck, GF_FALSE, GF_FALSE);
	gf_filter_pck_send(dst_pck);

	//send data packet
	dst_pck = gf_filter_pck_new_ref(ctx->opid_mdia, data, pck_size, pck);
	gf_filter_pck_merge_properties(pck, dst_pck);
	//keep byte offset ?
//	gf_filter_pck_set_byte_offset(dst_pck, GF_FILTER_NO_BO);

	gf_filter_pck_set_framing(dst_pck, ctx->first, GF_FALSE);
	gf_filter_pck_send(dst_pck);

	ctx->first = GF_FALSE;

	if (ctx->exporter) {
		u32 timescale = gf_filter_pck_get_timescale(pck);
		u64 ts = gf_filter_pck_get_cts(pck);
		gf_set_progress("Exporting", ts*ctx->duration.den, ctx->duration.num*timescale);
	}

	gf_filter_pid_drop_packet(ctx->ipid);

	return GF_OK;
}

static void nhntdump_finalize(GF_Filter *filter)
{
	GF_NHNTDumpCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->bs) gf_bs_del(ctx->bs);
}

static const GF_FilterCapability NHNTDumpCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "nhnt"),
};

#define OFFS(_n)	#_n, offsetof(GF_NHNTDumpCtx, _n)
static const GF_FilterArgs NHNTDumpArgs[] =
{
	{ OFFS(exporter), "compatibility with old exporter, displays export results", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(large), "use large file mode", GF_PROP_BOOL, "false", NULL, 0},
	{0}
};


GF_FilterRegister NHNTDumpRegister = {
	.name = "nhntw",
	GF_FS_SET_DESCRIPTION("NHNT writer")
	GF_FS_SET_HELP("This filter converts a single stream to an NHNT output file.\n"
	"NHNT documentation is available at https://github.com/gpac/gpac/wiki/NHNT-Format\n")
	.private_size = sizeof(GF_NHNTDumpCtx),
	.args = NHNTDumpArgs,
	.finalize = nhntdump_finalize,
	SETCAPS(NHNTDumpCaps),
	.configure_pid = nhntdump_configure_pid,
	.process = nhntdump_process
};

const GF_FilterRegister *nhntdump_register(GF_FilterSession *session)
{
	return &NHNTDumpRegister;
}

