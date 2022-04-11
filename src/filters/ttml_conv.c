/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2022
 *					All rights reserved
 *
 *  This file is part of GPAC / TTML to SRT converter filter
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
#include <gpac/internal/media_dev.h>
#include <gpac/color.h>
#include <gpac/xml.h>


typedef struct
{
	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;
	u32 timescale;

	u8 *buf;
	u32 buf_alloc;
	s64 last_end_ts;

	Bool srt_conv;
} TTMLConvCtx;

GF_Err ttmlconv_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	TTMLConvCtx *ctx = gf_filter_get_udta(filter);

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


	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
	}
	ctx->ipid = pid;
	gf_filter_pid_copy_properties(ctx->opid, pid);

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(ctx->srt_conv ? GF_CODECID_SIMPLE_TEXT : GF_CODECID_WEBVTT) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, &PROP_BOOL(GF_TRUE) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL);

	const GF_PropertyValue *p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	ctx->timescale = p ? p->value.uint : 1000;
	return GF_OK;
}

//from filter writegen
GF_XMLNode *ttml_get_body(GF_XMLNode *root);
GF_XMLAttribute *ttml_get_attr(GF_XMLNode *node, char *name);

//from filter txtin
u64 ttml_get_timestamp_ex(char *value, u32 tick_rate, u32 *ttml_fps_num, u32 *ttml_fps_den, u32 *ttml_sfps);
GF_Err ttml_parse_root(GF_XMLNode *root, const char **lang, u32 *tick_rate, u32 *ttml_fps_num, u32 *ttml_fps_den, u32 *ttml_sfps);

static void ttmlconv_dump_node(TTMLConvCtx *ctx, GF_XMLNode *p, FILE *dump)
{
	u32 i;
	GF_XMLNode *child;
	GF_XMLAttribute *att;

	Bool needs_italic = GF_FALSE;
	Bool needs_underlined = GF_FALSE;
	Bool needs_bold = GF_FALSE;
	const char *needs_color = NULL;
	i=0;
	while ( (att = gf_list_enum(p->attributes, &i)) ) {
		const char *sep = strchr(att->name, ':');
		const char *name  = sep ? sep+1 : att->name;
		if (!name) continue;;
		if (!strcmp(name, "fontStyle") && (!strcmp(att->value, "italic") || !strcmp(att->value, "oblique")))
			needs_italic = GF_TRUE;
		if (!strcmp(name, "textDecoration") && !strcmp(att->value, "underlined"))
			needs_underlined = GF_TRUE;
		if (!strcmp(name, "fontWeight") && !strcmp(att->value, "bold"))
			needs_bold = GF_TRUE;
		if (!strcmp(name, "color"))
			needs_color = att->value;
	}
	if (!ctx->srt_conv) needs_color = NULL;

	if (needs_color) fprintf(dump, "<font color=\"%s\">", needs_color);
	if (needs_italic) fprintf(dump, "<i>");
	if (needs_bold) fprintf(dump, "<b>");
	if (needs_underlined) fprintf(dump, "<u>");

	i=0;
	while ( (child = gf_list_enum(p->content, &i)) ) {
		if (child->type == GF_XML_TEXT_TYPE) {
			fprintf(dump, "%s", child->name);
			continue;
		}
		if (child->type != GF_XML_NODE_TYPE) continue;
		if (!strcmp(child->name, "br")) {
			fprintf(dump, "\n");
			continue;
		}
		ttmlconv_dump_node(ctx, child, dump);
	}

	if (needs_underlined) fprintf(dump, "</u>");
	if (needs_bold) fprintf(dump, "</b>");
	if (needs_italic) fprintf(dump, "</i>");
	if (needs_color) fprintf(dump, "</font>");
}

GF_Err ttmlconv_process(GF_Filter *filter)
{
	TTMLConvCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck;
	u8 *data;
	u64 start_ts, end_ts;
	u32 pck_size;
	GF_Err e = GF_OK;
	GF_DOMParser *dom;
	u32 txt_size, nb_children;
	u32 i, j;
	FILE *dump = NULL;
	u32 tick_rate=0, ttml_fps_num=0, ttml_fps_den=0, ttml_sfps=0;
	const char *lang;
	GF_XMLNode *root, *p, *body;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}

	data = (char *) gf_filter_pck_get_data(pck, &pck_size);
	if (!data || !pck_size) {
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_OK;
	}


	start_ts = gf_filter_pck_get_cts(pck);
	end_ts = start_ts + gf_filter_pck_get_duration(pck);

	start_ts = gf_timestamp_rescale(start_ts, ctx->timescale, 1000);
	end_ts = gf_timestamp_rescale(end_ts, ctx->timescale, 1000);

	const GF_PropertyValue *subs = gf_filter_pck_get_property(pck, GF_PROP_PCK_SUBS);

	if (subs) {
		if (subs->value.data.size < 14)
			return GF_NON_COMPLIANT_BITSTREAM;
		txt_size = subs->value.data.ptr[4];
		txt_size <<= 8;
		txt_size |= subs->value.data.ptr[5];
		txt_size <<= 8;
		txt_size |= subs->value.data.ptr[6];
		txt_size <<= 8;
		txt_size |= subs->value.data.ptr[7];
		if (txt_size>pck_size)
			return GF_NON_COMPLIANT_BITSTREAM;
	} else {
		txt_size = pck_size;
	}
	if (ctx->buf_alloc < txt_size + 2) {
		ctx->buf = gf_realloc(ctx->buf, txt_size+2);
		if (!ctx->buf) return GF_OUT_OF_MEM;
		ctx->buf_alloc = txt_size+2;
	}
	memcpy(ctx->buf, data, txt_size);
	ctx->buf[txt_size] = 0;
	ctx->buf[txt_size+1] = 0;

	dom = gf_xml_dom_new();
	if (!dom) return GF_OUT_OF_MEM;
	e = gf_xml_dom_parse_string(dom, ctx->buf);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[XML] Invalid TTML doc: %s\n\tXML text was:\n%s", gf_xml_dom_get_error(dom), ctx->buf));
		goto exit;
	}
	root = gf_xml_dom_get_root(dom);
	//parse root att
	e = ttml_parse_root(root, &lang, &tick_rate, &ttml_fps_num, &ttml_fps_den, &ttml_sfps);
	if (e) goto exit;

	u32 div_idx=0;
	body = ttml_get_body(root);
	nb_children = body ? gf_list_count(body->content) : 0;
	for (i=0; i<nb_children; i++) {
		GF_XMLAttribute *div_reg = NULL;
		GF_XMLNode *div = gf_list_get(body->content, i);
		if (div->type) continue;
		if (strcmp(div->name, "div")) continue;

		div_idx++;
		div_reg = ttml_get_attr(div, "region");

		j=0;
		while ( (p = gf_list_enum(div->content, &j)) ) {
			if (p->type) continue;
			if (strcmp(p->name, "p")) continue;

			GF_XMLAttribute *start = ttml_get_attr(p, "begin");
			GF_XMLAttribute *end = ttml_get_attr(p, "end");
			if (!end || !start) continue;

			u64 p_end_ts = ttml_get_timestamp_ex(end->value, tick_rate, &ttml_fps_num, &ttml_fps_den, &ttml_sfps);
			u64 p_start_ts = ttml_get_timestamp_ex(start->value, tick_rate, &ttml_fps_num, &ttml_fps_den, &ttml_sfps);
			p_start_ts = gf_timestamp_rescale(p_start_ts, 1000, ctx->timescale);
			p_end_ts = gf_timestamp_rescale(p_end_ts, 1000, ctx->timescale);

			//wrong timing in doc, don't process
			if (p_end_ts < p_start_ts) continue;
			//p.end less than packet start time, don't send
			if (p_end_ts < start_ts) continue;
			//p.start greater than packet end time, stop sending
			if (p_start_ts > end_ts) break;

			if (ctx->last_end_ts && (ctx->last_end_ts >= p_end_ts+1)) continue;
			ctx->last_end_ts = p_end_ts+1;

			dump = gf_file_temp(NULL);
			if (!dump) {
				e = GF_OUT_OF_MEM;
				goto exit;
			}
			ttmlconv_dump_node(ctx, p, dump);
			u32 size = gf_ftell(dump);
			gf_fseek(dump, 0, SEEK_SET);
			u8 *output;
			dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &output);
			if (!dst_pck) {
				e = GF_OUT_OF_MEM;
				goto exit;
			}
			gf_fread(output, size, dump);
			gf_fclose(dump);
			dump=NULL;

			gf_filter_pck_merge_properties(pck, dst_pck);
			gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1);
			gf_filter_pck_set_byte_offset(dst_pck, GF_FILTER_NO_BO);
			gf_filter_pck_set_cts(dst_pck, p_start_ts);
			gf_filter_pck_set_duration(dst_pck, p_end_ts-p_start_ts);
			gf_filter_pck_send(dst_pck);
		}
	}

exit:
	gf_filter_pid_drop_packet(ctx->ipid);
	gf_xml_dom_del(dom);
	if (dump) gf_fclose(dump);
	return e;
}

static void ttmlconv_finalize(GF_Filter *filter)
{
	TTMLConvCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->buf) gf_free(ctx->buf);
}

static const GF_FilterCapability TTMLConvCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_SUBS_XML),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_WEBVTT),
	CAP_BOOL(GF_CAPS_OUTPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
};


GF_FilterRegister TTMLConvRegister = {
	.name = "ttml2vtt",
	GF_FS_SET_DESCRIPTION("TTML to WebVTT")
	GF_FS_SET_HELP("This filter converts TTML frames to unframed WebVTT\n."
	"\n"
	"Conversion is quite limited: only the first div is analyzed and only basic styling is implemented.\n"
	)
	.private_size = sizeof(TTMLConvCtx),
	.finalize = ttmlconv_finalize,
	SETCAPS(TTMLConvCaps),
	.configure_pid = ttmlconv_configure_pid,
	.process = ttmlconv_process,
	//lower priority so that we always favor ttml2srt when converting ttml to tx3g
	.priority = 128
};


const GF_FilterRegister *ttmlconv_register(GF_FilterSession *session)
{
	return &TTMLConvRegister;
}

static GF_Err ttmlconv2_initialize(GF_Filter *filter)
{
	TTMLConvCtx *ctx = gf_filter_get_udta(filter);
	ctx->srt_conv = GF_TRUE;
	return GF_OK;
}

static const GF_FilterCapability TTMLConv2Caps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_SUBS_XML),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_SIMPLE_TEXT),
	CAP_BOOL(GF_CAPS_OUTPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
};


GF_FilterRegister TTMLConv2Register = {
	.name = "ttml2srt",
	GF_FS_SET_DESCRIPTION("TTML to SRT")
	GF_FS_SET_HELP("This filter converts TTML frames to unframed SRT\n."
	"\n"
	"Conversion is quite limited: only the first div is analyzed and only basic styling is implemented.\n"
	)
	.private_size = sizeof(TTMLConvCtx),
	.initialize = ttmlconv2_initialize,
	.finalize = ttmlconv_finalize,
	SETCAPS(TTMLConv2Caps),
	.configure_pid = ttmlconv_configure_pid,
	.process = ttmlconv_process
};


const GF_FilterRegister *ttmlconv2_register(GF_FilterSession *session)
{
	return &TTMLConv2Register;
}

