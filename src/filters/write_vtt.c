/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2018
 *					All rights reserved
 *
 *  This file is part of GPAC / WebVTT stream to file filter
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


typedef struct
{
	//opts
	Bool exporter, merge;

	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;

	u32 codecid;
	Bool first;

	GF_Fraction duration;
	char *dcd;

	GF_BitStream *bs_w;
	u8 *cues_buffer;
	u32 cues_buffer_size;

	GF_WebVTTParser *parser;

} GF_WebVTTMxCtx;

static void vttmx_write_cue(void *ctx, GF_WebVTTCue *cue);

GF_Err vttmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	GF_WebVTTMxCtx *ctx = gf_filter_get_udta(filter);

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

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (!p) return GF_NOT_SUPPORTED;
	if (ctx->dcd && !strcmp(ctx->dcd, p->value.string)) return GF_OK;
	if (ctx->dcd) gf_free(ctx->dcd);

	if ((p->type==GF_PROP_DATA) || (p->type==GF_PROP_DATA_NO_COPY)) {
		ctx->dcd = gf_malloc(p->value.data.size+1);
		memcpy(ctx->dcd, p->value.data.ptr, p->value.data.size);
		ctx->dcd[p->value.data.size]=0;
	} else {
		ctx->dcd = gf_strdup(p->value.string);
	}
	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("vtt") );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("text/vtt") );
		ctx->first = GF_TRUE;

		if (ctx->exporter) {
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("Exporting %s\n", gf_codecid_name(ctx->codecid)));
		}

	}
	ctx->ipid = pid;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DURATION);
	if (p) ctx->duration = p->value.frac;

	if (ctx->parser) gf_webvtt_parser_del(ctx->parser);
	ctx->parser = NULL;
	if (ctx->merge) {
		ctx->parser = gf_webvtt_parser_new();
		gf_webvtt_parser_cue_callback(ctx->parser, vttmx_write_cue, ctx);
	}
	gf_filter_pid_set_framing_mode(pid, GF_TRUE);
	return GF_OK;
}

void vttmx_timestamp_dump(GF_BitStream *bs, GF_WebVTTTimestamp *ts, Bool dump_hour)
{
	char szTS[200];
	szTS[0] = 0;
	if (dump_hour || ts->hour != 0) {
		sprintf(szTS, "%02u:", ts->hour);
	}
	sprintf(szTS, "%02u:%02u.%03u", ts->min, ts->sec, ts->ms);
	gf_bs_write_data(bs, szTS, (u32) strlen(szTS) );
}

void webvtt_write_cue(GF_BitStream *bs, GF_WebVTTCue *cue)
{
	if (!cue) return;
	if (cue->pre_text) {
		gf_bs_write_data(bs, cue->pre_text, (u32) strlen(cue->pre_text));
		gf_bs_write_data(bs, "\n\n", 2);
	}
	if (cue->id) gf_bs_write_data(bs, cue->id, (u32) strlen(cue->id) );
	if (cue->start.hour || cue->end.hour) {
		vttmx_timestamp_dump(bs, &cue->start, GF_TRUE);
		gf_bs_write_data(bs, " --> ", 5);
		vttmx_timestamp_dump(bs, &cue->end, GF_TRUE);
	} else {
		vttmx_timestamp_dump(bs, &cue->start, GF_FALSE);
		gf_bs_write_data(bs, " --> ", 5);
		vttmx_timestamp_dump(bs, &cue->end, GF_FALSE);
	}
	if (cue->settings) {
		gf_bs_write_data(bs, " ", 1);
		gf_bs_write_data(bs, cue->settings, (u32) strlen(cue->settings));
	}
	gf_bs_write_data(bs, "\n", 1);
	if (cue->text)
		gf_bs_write_data(bs, cue->text, (u32) strlen(cue->text));
	gf_bs_write_data(bs, "\n\n", 2);

	if (cue->post_text) {
		gf_bs_write_data(bs, cue->post_text, (u32) strlen(cue->post_text));
		gf_bs_write_data(bs, "\n\n", 2);
	}
}

static void vttmx_write_cue(void *udta, GF_WebVTTCue *cue)
{
	GF_WebVTTMxCtx *ctx = (GF_WebVTTMxCtx *)udta;
	webvtt_write_cue(ctx->bs_w, cue);
}

void vttmx_parser_flush(GF_WebVTTMxCtx *ctx)
{
	GF_FilterPacket *dst_pck;
	u32 size;
	u8 *output;
	u64 duration = ctx->duration.num;
	duration *= 1000;
	duration /= ctx->duration.den;

	if (!ctx->bs_w) ctx->bs_w = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	else gf_bs_reassign_buffer(ctx->bs_w, ctx->cues_buffer, ctx->cues_buffer_size);

	gf_webvtt_parser_finalize(ctx->parser, duration);

	if (gf_bs_get_position(ctx->bs_w)) {
		gf_bs_get_content_no_truncate(ctx->bs_w, &ctx->cues_buffer, &size, &ctx->cues_buffer_size);
		dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &output);
		memcpy(output, ctx->cues_buffer, size);

		gf_filter_pck_set_byte_offset(dst_pck, GF_FILTER_NO_BO);

		gf_filter_pck_set_framing(dst_pck, GF_FALSE, GF_TRUE);
		gf_filter_pck_send(dst_pck);
	}


	gf_webvtt_parser_del(ctx->parser);
	ctx->parser = NULL;

}
GF_Err vttmx_process(GF_Filter *filter)
{
	GF_WebVTTMxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck;
	u8 *data, *output;
	u64 start_ts;
	u32 i, pck_size, size, timescale;
	GF_List *cues;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			gf_filter_pid_set_eos(ctx->opid);
			if (ctx->parser) {
				vttmx_parser_flush(ctx);
			}
			return GF_EOS;
		}
		return GF_OK;
	}

	data = (char *) gf_filter_pck_get_data(pck, &pck_size);

	if (ctx->first && ctx->dcd) {
		size = (u32) strlen(ctx->dcd)+2;
		dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &output);
		memcpy(output, ctx->dcd, size-2);
		output[size-2] = '\n';
		output[size-1] = '\n';
		gf_filter_pck_merge_properties(pck, dst_pck);
		gf_filter_pck_set_byte_offset(dst_pck, GF_FILTER_NO_BO);
		gf_filter_pck_set_framing(dst_pck, ctx->first, GF_FALSE);
		ctx->first = GF_FALSE;
		gf_filter_pck_send(dst_pck);
	}

	if (!ctx->bs_w) ctx->bs_w = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	else gf_bs_reassign_buffer(ctx->bs_w, ctx->cues_buffer, ctx->cues_buffer_size);

	start_ts = gf_filter_pck_get_cts(pck);
	start_ts *= 1000;
	timescale = gf_filter_pck_get_timescale(pck);
	if (!timescale) timescale=1000;
	start_ts /= timescale;

	cues = gf_webvtt_parse_cues_from_data(data, pck_size, start_ts);
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

	gf_bs_get_content_no_truncate(ctx->bs_w, &ctx->cues_buffer, &size, &ctx->cues_buffer_size);
	if (size) {
		dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &output);
		memcpy(output, ctx->cues_buffer, size);

		gf_filter_pck_merge_properties(pck, dst_pck);
		gf_filter_pck_set_byte_offset(dst_pck, GF_FILTER_NO_BO);


		gf_filter_pck_set_framing(dst_pck, ctx->first, GF_FALSE);
		ctx->first = GF_FALSE;

		gf_filter_pck_send(dst_pck);
	}

	if (ctx->exporter) {
		u32 timescale = gf_filter_pck_get_timescale(pck);
		u64 ts = gf_filter_pck_get_cts(pck);
		gf_set_progress("Exporting", ts*ctx->duration.den, ctx->duration.num*timescale);
	}

	gf_filter_pid_drop_packet(ctx->ipid);

	return GF_OK;
}

static void vttmx_finalize(GF_Filter *filter)
{
	GF_WebVTTMxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->bs_w) gf_bs_del(ctx->bs_w);
	if (ctx->dcd) gf_free(ctx->dcd);
	if (ctx->cues_buffer) gf_free(ctx->cues_buffer);

	if (ctx->parser) gf_webvtt_parser_del(ctx->parser);
}

static const GF_FilterCapability WebVTTMxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_WEBVTT),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "vtt"),
};


#define OFFS(_n)	#_n, offsetof(GF_WebVTTMxCtx, _n)
static const GF_FilterArgs WebVTTMxArgs[] =
{
	{ OFFS(exporter), "compatibility with old exporter, displays export results", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(merge), "merge VTT cue if needed", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{0}
};


GF_FilterRegister WebVTTMxRegister = {
	.name = "writevtt",
	GF_FS_SET_DESCRIPTION("WebVTT writer")
	GF_FS_SET_HELP("This filter converts a single stream to a WebVTT output file.")
	.private_size = sizeof(GF_WebVTTMxCtx),
	.args = WebVTTMxArgs,
	.finalize = vttmx_finalize,
	SETCAPS(WebVTTMxCaps),
	.configure_pid = vttmx_configure_pid,
	.process = vttmx_process
};


const GF_FilterRegister *vttmx_register(GF_FilterSession *session)
{
	return &WebVTTMxRegister;
}
