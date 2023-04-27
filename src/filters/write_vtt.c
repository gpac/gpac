/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / WebVTT stream unframer filter
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
#include <gpac/webvtt.h>
#include <gpac/internal/media_dev.h>


#ifndef GPAC_DISABLE_VTT

typedef struct
{
	//opts
	Bool exporter, merge_cues;

	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;

	u32 codecid;
	u32 timescale;

	GF_Fraction64 duration;
	s64 delay;

	u8 *cues_buffer;
	u32 cues_buffer_size;

	GF_WebVTTParser *parser;

	GF_FilterPacket *src_pck;
} GF_WebVTTMxCtx;

static void vttmx_write_cue(void *ctx, GF_WebVTTCue *cue);

GF_Err vttmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	GF_WebVTTMxCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		ctx->ipid = NULL;
		if (ctx->opid) {
			gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!p) return GF_NOT_SUPPORTED;
	ctx->codecid = p->value.uint;
	ctx->ipid = pid;


	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		if (ctx->exporter) {
			GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("Exporting %s\n", gf_codecid_name(ctx->codecid)));
		}
	}
	gf_filter_pid_copy_properties(ctx->opid, pid );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, &PROP_BOOL(GF_TRUE) );

	//dec config not ready
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (!p) return GF_OK;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DURATION);
	if (p && (p->value.lfrac.num>0)) ctx->duration = p->value.lfrac;

	if (ctx->parser) gf_webvtt_parser_del(ctx->parser);
	ctx->parser = NULL;
	if (ctx->merge_cues) {
		ctx->parser = gf_webvtt_parser_new();
		gf_webvtt_parser_cue_callback(ctx->parser, vttmx_write_cue, ctx);
	}
	gf_filter_pid_set_framing_mode(pid, GF_TRUE);

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DASH_MODE);

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	ctx->timescale = p ? p->value.uint : 1000;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DELAY);
	ctx->delay = p ? p->value.longsint : 0;
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DELAY, NULL);

	return GF_OK;
}


void webvtt_write_cue(GF_WebVTTMxCtx *ctx, GF_WebVTTCue *cue, Bool write_srt)
{
	u8 *output;
	u32 size;
	if (!cue) return;

	size = cue->text ? (u32) strlen(cue->text) : 0;

	GF_FilterPacket *dst = gf_filter_pck_new_alloc(ctx->opid, size, &output);
	if (!dst) return;
	if (size) memcpy(output, cue->text, size);

	gf_filter_pck_merge_properties(ctx->src_pck, dst);
	u64 start = cue->start.hour*3600 + cue->start.min*60 + cue->start.sec;
	start *= 1000;
	start += cue->start.ms;
	u64 end = cue->end.hour*3600 + cue->end.min*60 + cue->end.sec;
	end *= 1000;
	end += cue->end.ms;
	start = gf_timestamp_rescale(start, 1000, ctx->timescale);
	end = gf_timestamp_rescale(end, 1000, ctx->timescale);
	//override both dts and cts as they can be different from input if cues are merged
	gf_filter_pck_set_dts(dst, start);
	gf_filter_pck_set_cts(dst, start);
	gf_filter_pck_set_duration(dst, (u32) (end-start));

	gf_filter_pck_set_property_str(dst, "vtt_pre", NULL);
	gf_filter_pck_set_property_str(dst, "vtt_cueid", NULL);
	gf_filter_pck_set_property_str(dst, "vtt_settings", NULL);

	if (!write_srt) {
		if (cue->pre_text) {
			gf_filter_pck_set_property_str(dst, "vtt_pre", &PROP_STRING(cue->pre_text) );
		}
		if (cue->id) {
			gf_filter_pck_set_property_str(dst, "vtt_cueid", &PROP_STRING(cue->id) );
		}
		if (cue->settings) {
			gf_filter_pck_set_property_str(dst, "vtt_settings", &PROP_STRING(cue->settings) );
		}
	}

	gf_filter_pck_set_sap(dst, GF_FILTER_SAP_1);
	gf_filter_pck_set_byte_offset(dst, GF_FILTER_NO_BO);
	gf_filter_pck_send(dst);
}

static void vttmx_write_cue(void *udta, GF_WebVTTCue *cue)
{
	GF_WebVTTMxCtx *ctx = (GF_WebVTTMxCtx *)udta;
	webvtt_write_cue(ctx, cue, GF_FALSE);
}

void vttmx_parser_flush(GF_WebVTTMxCtx *ctx)
{
	u64 duration = 0;
	if (ctx->duration.num && ctx->duration.den) {
		duration = ctx->duration.num;
		duration *= 1000;
		duration /= ctx->duration.den;
	}
	gf_webvtt_parser_finalize(ctx->parser, duration);
	gf_webvtt_parser_del(ctx->parser);
	ctx->parser = NULL;

}
GF_Err vttmx_process(GF_Filter *filter)
{
	GF_WebVTTMxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck;
	u8 *data;
	u64 start_ts, end_ts;
	u32 i, pck_size;
	GF_List *cues;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			gf_filter_pid_set_eos(ctx->opid);
			if (ctx->parser) {
				vttmx_parser_flush(ctx);
			}
			if (ctx->src_pck) {
				gf_filter_pck_unref(ctx->src_pck);
				ctx->src_pck = NULL;
			}
			return GF_EOS;
		}
		return GF_OK;
	}

	data = (char *) gf_filter_pck_get_data(pck, &pck_size);

	start_ts = gf_filter_pck_get_cts(pck);
	end_ts = start_ts + gf_filter_pck_get_duration(pck);

	if ((s64) start_ts > -ctx->delay) start_ts += ctx->delay;
	else start_ts = 0;
	if ((s64) end_ts > -ctx->delay) end_ts += ctx->delay;
	else end_ts = 0;

	start_ts = gf_timestamp_rescale(start_ts, ctx->timescale, 1000);
	end_ts = gf_timestamp_rescale(end_ts, ctx->timescale, 1000);

	if (ctx->src_pck) gf_filter_pck_unref(ctx->src_pck);
	ctx->src_pck = pck;
	gf_filter_pck_ref_props(&ctx->src_pck);

	cues = gf_webvtt_parse_cues_from_data(data, pck_size, start_ts, end_ts);
	if (ctx->parser) {
		gf_webvtt_merge_cues(ctx->parser, start_ts, cues);
	} else {
		for (i = 0; i < gf_list_count(cues); i++) {
			GF_WebVTTCue *cue = (GF_WebVTTCue *)gf_list_get(cues, i);
			vttmx_write_cue(ctx, cue);
			gf_webvtt_cue_del(cue);
		}
	}
	gf_list_del(cues);

	if (ctx->exporter) {
		u64 ts = gf_filter_pck_get_cts(pck);
		gf_set_progress("Exporting", ts*ctx->duration.den, ctx->duration.num * ctx->timescale);
	}

	gf_filter_pid_drop_packet(ctx->ipid);

	return GF_OK;
}

static void vttmx_finalize(GF_Filter *filter)
{
	GF_WebVTTMxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->cues_buffer) gf_free(ctx->cues_buffer);

	if (ctx->parser) gf_webvtt_parser_del(ctx->parser);

	if (ctx->src_pck) {
		gf_filter_pck_unref(ctx->src_pck);
		ctx->src_pck = NULL;
	}

}

static const GF_FilterCapability WebVTTMxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_WEBVTT),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_BOOL(GF_CAPS_OUTPUT, GF_PROP_PID_UNFRAMED, GF_TRUE),
};


#define OFFS(_n)	#_n, offsetof(GF_WebVTTMxCtx, _n)
static const GF_FilterArgs WebVTTMxArgs[] =
{
	{ OFFS(exporter), "compatibility with old exporter, displays export results", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(merge_cues), "merge VTT cues (undo ISOBMFF cue split)", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_ADVANCED},
	{0}
};


GF_FilterRegister WebVTTMxRegister = {
	.name = "ufvtt",
	GF_FS_SET_DESCRIPTION("WebVTT unframer")
	GF_FS_SET_HELP("This filter converts a single ISOBMFF WebVTT stream to its unframed format.")
	.private_size = sizeof(GF_WebVTTMxCtx),
	.args = WebVTTMxArgs,
	.finalize = vttmx_finalize,
	SETCAPS(WebVTTMxCaps),
	.configure_pid = vttmx_configure_pid,
	.process = vttmx_process
};


const GF_FilterRegister *ufvtt_register(GF_FilterSession *session)
{
	return &WebVTTMxRegister;
}
#else
const GF_FilterRegister *ufvtt_register(GF_FilterSession *session)
{
	return NULL;
}

#endif /*GPAC_DISABLE_VTT*/
