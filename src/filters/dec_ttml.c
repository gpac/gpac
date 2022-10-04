/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2020-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / TTML decoder filter
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

#if !defined(GPAC_DISABLE_SVG) && defined(GPAC_HAS_QJS)

#include <gpac/internal/scenegraph_dev.h>
#include <gpac/internal/compositor_dev.h>
#include <gpac/nodes_svg.h>
#include <gpac/xml.h>

#include "../scenegraph/qjs_common.h"

typedef struct
{
	//opts
	char *script;
	char *color, *font;
	Float fontSize, lineSpacing;
	u32 txtw, txth, valign;

	GF_FilterPid *ipid, *opid;

	/* Boolean indicating the internal graph is registered with the compositor */
	Bool graph_registered;
	/* Boolean indicating the stream is playing */
	Bool is_playing;

	/* Parent scene to which this TTML stream is linked */
	GF_Scene *scene;
	/* root object manager of parent scene (used to get clock info)*/
	GF_ObjectManager *odm;

	/* Scene graph for the subtitle content */
	GF_SceneGraph *scenegraph;

	Bool update_args;

	GF_BitStream *subs_bs;
	s32 notify_clock;
	s32 txtx, txty;
	u32 fsize, vp_w, vp_h;

	s64 delay;
	u32 timescale;
} GF_TTMLDec;

void ttmldec_update_size_info(GF_TTMLDec *ctx)
{
	u32 w, h;
	GF_FieldInfo info;
	Bool has_size;
	char szVB[100];
	GF_Node *root = gf_sg_get_root_node(ctx->scenegraph);
	if (!root) return;

	has_size = gf_sg_get_scene_size_info(ctx->scene->graph, &w, &h);
	/*no size info is given in main scene, override by associated video size if any, or by text track size*/
	if (!has_size) {
		const GF_PropertyValue *p;
		p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_WIDTH);
		if (p) w = p->value.uint;
		p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_HEIGHT);
		if (p) h = p->value.uint;

		if (!w) w = ctx->txtw;
		if (!h) h = ctx->txth;

		gf_sg_set_scene_size_info(ctx->scenegraph, w, h, GF_TRUE);
		gf_scene_force_size(ctx->scene, w, h);
	}

	gf_sg_set_scene_size_info(ctx->scenegraph, w, h, GF_TRUE);
	ctx->vp_w = w;
	ctx->vp_h = h;

	sprintf(szVB, "0 0 %d %d", w, h);
	gf_node_get_attribute_by_tag(root, TAG_SVG_ATT_viewBox, GF_TRUE, GF_FALSE, &info);
	gf_svg_parse_attribute(root, &info, szVB, 0);

	/*apply*/
	gf_sg_set_scene_size_info(ctx->scenegraph, w, h, GF_TRUE);

	sprintf(szVB, "0 0 %d %d", w, h);
	gf_node_get_attribute_by_tag(root, TAG_SVG_ATT_viewBox, GF_TRUE, GF_FALSE, &info);
	gf_svg_parse_attribute(root, &info, szVB, 0);

}

void ttmldec_setup_scene(GF_TTMLDec *ctx)
{
	GF_Node *n, *root;
	GF_FieldInfo info;

	ctx->scenegraph = gf_sg_new_subscene(ctx->scene->graph);

	if (!ctx->scenegraph) return;
	gf_sg_add_namespace(ctx->scenegraph, "http://www.w3.org/2000/svg", NULL);
	gf_sg_add_namespace(ctx->scenegraph, "http://www.w3.org/1999/xlink", "xlink");
	gf_sg_add_namespace(ctx->scenegraph, "http://www.w3.org/2001/xml-events", "ev");
	gf_sg_set_scene_size_info(ctx->scenegraph, 800, 600, GF_TRUE);

	/* modify the scene with an Inline/Animation pointing to the TTML Renderer */
	n = root = gf_node_new(ctx->scenegraph, TAG_SVG_svg);
	gf_node_register(root, NULL);
	gf_sg_set_root_node(ctx->scenegraph, root);
	gf_node_get_attribute_by_name(n, "xmlns", 0, GF_TRUE, GF_FALSE, &info);
	gf_svg_parse_attribute(n, &info, "http://www.w3.org/2000/svg", 0);
	ttmldec_update_size_info(ctx);
	gf_node_init(n);

	n = gf_node_new(ctx->scenegraph, TAG_SVG_script);
	gf_node_register(n, root);
	gf_node_list_add_child(&((GF_ParentNode *)root)->children, n);

	gf_node_get_attribute_by_tag(n, TAG_XLINK_ATT_href, GF_TRUE, GF_FALSE, &info);
	if (strstr(ctx->script, ":\\")) {
		gf_svg_parse_attribute(n, &info, (char *) ctx->script, 0);
	} else {
		char szPath[GF_MAX_PATH];
		strcpy(szPath, "file://");
		strcat(szPath, ctx->script);
		gf_svg_parse_attribute(n, &info, (char *) szPath, 0);
	}

	gf_node_init(n);

}

static GF_Err ttmldec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	GF_TTMLDec *ctx = (GF_TTMLDec *)gf_filter_get_udta(filter);

	if (is_remove) {
		if (ctx->opid) {
			gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
		}
		ctx->ipid = NULL;
		return GF_OK;
	}
	//TODO: we need to cleanup cap checking upon reconfigure
	if (ctx->ipid && !gf_filter_pid_check_caps(pid)) return GF_NOT_SUPPORTED;
	assert(!ctx->ipid || (ctx->ipid == pid));

	ctx->ipid = pid;
	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
	}

	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_TEXT));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW));

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DELAY);
	ctx->delay = p ? p->value.longsint : 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	ctx->timescale = p ? p->value.uint : 1000;

	return GF_OK;
}


static void ttmldec_toggle_display(GF_TTMLDec *ctx)
{
	if (!ctx->scenegraph) return;

	if (ctx->is_playing) {
		if (!ctx->graph_registered) {
			ttmldec_update_size_info(ctx);
			gf_scene_register_extra_graph(ctx->scene, ctx->scenegraph, GF_FALSE);
			ctx->graph_registered = GF_TRUE;
		}
	 } else {
		if (ctx->graph_registered) {
			gf_scene_register_extra_graph(ctx->scene, ctx->scenegraph, GF_TRUE);
			ctx->graph_registered = GF_FALSE;
		}
	}
}

static Bool ttmldec_process_event(GF_Filter *filter, const GF_FilterEvent *com)
{
	GF_TTMLDec *ctx = gf_filter_get_udta(filter);

	//check for scene attach
	switch (com->base.type) {
	case GF_FEVT_ATTACH_SCENE:
		break;
	case GF_FEVT_RESET_SCENE:
		if (ctx->opid != com->attach_scene.on_pid) return GF_TRUE;
		ctx->is_playing = GF_FALSE;
		ttmldec_toggle_display(ctx);

		gf_sg_del(ctx->scenegraph);
		ctx->scenegraph = NULL;
		ctx->scene = NULL;
		return GF_TRUE;
	case GF_FEVT_PLAY:
		ctx->is_playing = GF_TRUE;
		ttmldec_toggle_display(ctx);
		return GF_FALSE;
	case GF_FEVT_STOP:
		ctx->is_playing = GF_FALSE;
		ttmldec_toggle_display(ctx);
		return GF_FALSE;
	default:
		return GF_FALSE;
	}
	if (ctx->opid != com->attach_scene.on_pid) return GF_TRUE;

	ctx->odm = com->attach_scene.object_manager;
	ctx->scene = ctx->odm->subscene ? ctx->odm->subscene : ctx->odm->parentscene;

	/*timedtext cannot be a root scene object*/
	if (ctx->odm->subscene) {
		ctx->odm = NULL;
		ctx->scene = NULL;
	 } else {
		 ttmldec_setup_scene(ctx);
		 ttmldec_toggle_display(ctx);
	 }
	 return GF_TRUE;
}

void js_dump_error(JSContext *ctx);

JSContext *ttmldec_get_js_context(GF_TTMLDec *ctx)
{
	GF_SceneGraph *sg = ctx->scenegraph->RootNode->sgprivate->scenegraph;
	JSContext *svg_script_get_context(GF_SceneGraph *sg);
	JSContext *c = svg_script_get_context(sg);

	if ((ctx->txtx != ctx->scene->compositor->subtx)
		|| (ctx->txty != ctx->scene->compositor->subty)
		|| (ctx->fsize != ctx->scene->compositor->subfs)
	) {
		ctx->txtx = ctx->scene->compositor->subtx;
		ctx->txty = ctx->scene->compositor->subty;
		ctx->fsize = ctx->scene->compositor->subfs;
		ctx->update_args = GF_TRUE;
	}


	if (ctx->update_args) {
		JSValue global = JS_GetGlobalObject(c);

		u32 fs = (u32) MAX(ctx->fsize, ctx->fontSize);
		u32 def_font_size = ctx->scene->compositor->subfs;
		if (!def_font_size) {
			if (ctx->vp_h > 2000) def_font_size = 80;
			else if (ctx->vp_h > 700) def_font_size = 40;
			else def_font_size = 20;
		}

		if (!fs || (def_font_size>fs)) {
			fs = def_font_size;
		}

		JS_SetPropertyStr(c, global, "fontSize", JS_NewFloat64(c, fs));
		JS_SetPropertyStr(c, global, "fontFamily", JS_NewString(c, ctx->font));
		JS_SetPropertyStr(c, global, "textColor", JS_NewString(c, ctx->color));
		JS_SetPropertyStr(c, global, "lineSpaceFactor", JS_NewFloat64(c, ctx->lineSpacing));
		JS_SetPropertyStr(c, global, "xOffset", JS_NewFloat64(c, ctx->txtx));
		JS_SetPropertyStr(c, global, "yOffset", JS_NewFloat64(c, ctx->txty));
		JS_SetPropertyStr(c, global, "v_align", JS_NewInt32(c, ctx->valign));

		JS_FreeValue(c, global);
		ctx->update_args = GF_FALSE;
	}
	return c;
}



static GF_Err ttmldec_process(GF_Filter *filter)
{
	GF_Err e = GF_OK;
	const GF_PropertyValue *subs;
	GF_FilterPacket *pck;
	const u8 *pck_data;
	char *pck_alloc=NULL, *ttml_doc;
	u64 cts;
	u32 obj_time, dur;
	u32 pck_size;
	JSValue fun_val;
	JSValue global;
	JSContext *c;
	GF_TTMLDec *ctx = (GF_TTMLDec *) gf_filter_get_udta(filter);

	if (!ctx->scene) {
		if (ctx->is_playing) {
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}

	c = ttmldec_get_js_context(ctx);
	if (!c) {
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_SERVICE_ERROR;
	}

	//object clock shall be valid
	if (!ctx->odm->ck)
		return GF_OK;

	cts = gf_filter_pck_get_cts( pck );
	s64 delay = ctx->scene->compositor->subd;
	if (delay)
		delay = gf_timestamp_rescale_signed(delay, 1000, ctx->timescale);
	delay += ctx->delay;

	if (delay>=0) cts += delay;
	else if (cts > (u64) -delay) cts -= -delay;
	else cts = 0;

	gf_odm_check_buffering(ctx->odm, ctx->ipid);
	cts = gf_timestamp_to_clocktime(cts, ctx->timescale);
	dur = gf_timestamp_rescale(gf_filter_pck_get_duration(pck), ctx->timescale, 1000);

	//we still process any frame before our clock time even when buffering
	obj_time = gf_clock_time(ctx->odm->ck);

	if (ctx->notify_clock>0) {
		gf_js_lock(c, GF_TRUE);
		global = JS_GetGlobalObject(c);
		fun_val = JS_GetPropertyStr(c, global, "on_ttml_clock");
		if (JS_IsFunction(c, fun_val) ) {
			JSValue ret, argv[1];
			argv[0] = JS_NewInt64(c, gf_clock_time_absolute(ctx->odm->ck) );

			ret = JS_Call(c, fun_val, global, 1, argv);
			if (JS_IsException(ret)) {
				js_dump_error(c);
				e = GF_BAD_PARAM;
			}

			if (JS_IsNumber(ret)) {
				JS_ToInt32(c, &ctx->notify_clock, ret);
			} else {
				ctx->notify_clock = 0;
			}

			JS_FreeValue(c, ret);
			JS_FreeValue(c, argv[0]);
		}
		JS_FreeValue(c, global);
		JS_FreeValue(c, fun_val);
		gf_js_lock(c, GF_FALSE);
		if (e) return e;
	}

	if (!gf_sc_check_sys_frame(ctx->scene, ctx->odm, ctx->ipid, filter, cts, dur))
		return GF_OK;

	pck_data = gf_filter_pck_get_data(pck, &pck_size);
	subs = gf_filter_pck_get_property(pck, GF_PROP_PCK_SUBS);

	ttml_doc = (char *) pck_data;
	if (subs) {
		Bool first = GF_TRUE;
		gf_bs_reassign_buffer(ctx->subs_bs, subs->value.data.ptr, subs->value.data.size);
		while (gf_bs_available(ctx->subs_bs)) {
			/*u32 flags = */gf_bs_read_u32(ctx->subs_bs);
			u32 subs_size = gf_bs_read_u32(ctx->subs_bs);
			/*u32 reserved = */gf_bs_read_u32(ctx->subs_bs);
			/*u8 priority = */gf_bs_read_u8(ctx->subs_bs);
			/*u8 discardable = */gf_bs_read_u8(ctx->subs_bs);

			if (first) {
				first = GF_FALSE;
				if (subs_size>pck_size) {
					gf_filter_pid_drop_packet(ctx->ipid);
					GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[TTMLDec] Invalid subsample size %d for packet size %d\n", subs_size, pck_size));
					return GF_NON_COMPLIANT_BITSTREAM;
				}
				pck_alloc = gf_malloc(sizeof(char)*(subs_size+2));
				memcpy(pck_alloc, pck_data, sizeof(char)*subs_size);
				pck_alloc[subs_size] = 0;
				pck_alloc[subs_size+1] = 0;
				ttml_doc = pck_alloc;
			}
		}
	} else {
		//we cannot assume the doc ends with 0
		pck_alloc = gf_malloc(sizeof(char)*(pck_size+2));
		memcpy(pck_alloc, ttml_doc, sizeof(char)*pck_size);
		pck_alloc[pck_size] = 0;
		pck_alloc[pck_size+1] = 0;
		ttml_doc = pck_alloc;
	}

	GF_DOMParser *dom = gf_xml_dom_new();
	gf_xml_dom_parse_string(dom, ttml_doc);


	gf_js_lock(c, GF_TRUE);
	global = JS_GetGlobalObject(c);
	fun_val = JS_GetPropertyStr(c, global, "on_ttml_sample");
	if (!JS_IsFunction(c, fun_val) ) {
		e = GF_BAD_PARAM;
	} else {
		JSValue ret, argv[3];
		Double scene_ck;
		GF_SceneGraph *doc;

		doc = gf_sg_new();
		doc->reference_count = 1;
		e = gf_sg_init_from_xml_node(doc, gf_xml_dom_get_root(dom));
		argv[0] = dom_document_construct_external(c, doc);

		argv[1] = JS_NewInt64(c, obj_time);
		scene_ck = gf_filter_pck_get_duration(pck);
		scene_ck /= ctx->timescale;
		argv[2] = JS_NewFloat64(c, scene_ck);

		ret = JS_Call(c, fun_val, global, 3, argv);
		if (JS_IsException(ret)) {
			js_dump_error(c);
			e = GF_BAD_PARAM;
		}
		if (JS_IsNumber(ret)) {
			JS_ToInt32(c, &ctx->notify_clock, ret);
		} else {
			ctx->notify_clock = 0;
		}

		doc->reference_count --;
		JS_FreeValue(c, ret);
		JS_FreeValue(c, argv[0]);
		JS_FreeValue(c, argv[1]);
		JS_FreeValue(c, argv[2]);
	}
	JS_FreeValue(c, global);
	JS_FreeValue(c, fun_val);

	gf_js_lock(c, GF_FALSE);


	gf_xml_dom_del(dom);

	if (pck_alloc) gf_free(pck_alloc);
	gf_filter_pid_drop_packet(ctx->ipid);

	if (ctx->notify_clock>0) {
		if (ctx->scene && ctx->scene->compositor->player)
			gf_filter_ask_rt_reschedule(filter, ctx->notify_clock*1000);
		else
			gf_filter_post_process_task(filter);
	}
	return e;
}

static GF_Err ttmldec_update_arg(GF_Filter *filter, const char *arg_name, const GF_PropertyValue *new_val)
{
	GF_TTMLDec *ctx = gf_filter_get_udta(filter);
	ctx->update_args = GF_TRUE;
	return GF_OK;
}

static GF_Err ttmldec_initialize(GF_Filter *filter)
{
	GF_TTMLDec *ctx = gf_filter_get_udta(filter);

	if (!ctx->script) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[TTMLDec] TTML JS renderer script not set\n"));
		return GF_BAD_PARAM;
	} else if (!gf_file_exists(ctx->script)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[TTMLDec] TTML JS renderer script %s not found\n", ctx->script));
		return GF_URL_ERROR;
	}
	ctx->subs_bs = gf_bs_new((u8 *)ctx, 1, GF_BITSTREAM_READ);

	ctx->update_args = GF_TRUE;
#ifdef GPAC_ENABLE_COVERAGE
	ttmldec_update_arg(filter, NULL, NULL);
#endif
	return GF_OK;
}

void ttmldec_finalize(GF_Filter *filter)
{
	GF_TTMLDec *ctx = (GF_TTMLDec *) gf_filter_get_udta(filter);

	if (ctx->scenegraph) {
		ctx->is_playing = GF_FALSE;
		ttmldec_toggle_display(ctx);
		gf_sg_del(ctx->scenegraph);
	}
	gf_bs_del(ctx->subs_bs);
}

#define OFFS(_n)	#_n, offsetof(GF_TTMLDec, _n)
static const GF_FilterArgs TTMLDecArgs[] =
{
	{ OFFS(script), "location of TTML SVG JS renderer", GF_PROP_STRING, "$GSHARE/scripts/ttml-renderer.js", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(font), "font", GF_PROP_STRING, "SANS", NULL, GF_FS_ARG_HINT_ADVANCED|GF_FS_ARG_UPDATE},
	{ OFFS(fontSize), "font size", GF_PROP_FLOAT, "20", NULL, GF_FS_ARG_HINT_ADVANCED|GF_FS_ARG_UPDATE},
	{ OFFS(color), "text color", GF_PROP_STRING, "white", NULL, GF_FS_ARG_HINT_ADVANCED|GF_FS_ARG_UPDATE},
	{ OFFS(valign), "vertical alignment\n"
	"- bottom: align text at bottom of text area\n"
	"- center: align text at center of text area\n"
	"- top: align text at top of text area", GF_PROP_UINT, "bottom", "bottom|center|top", GF_FS_ARG_HINT_ADVANCED|GF_FS_ARG_UPDATE},
	{ OFFS(lineSpacing), "line spacing as scaling factor to font size", GF_PROP_FLOAT, "1.0", NULL, GF_FS_ARG_HINT_ADVANCED|GF_FS_ARG_UPDATE},
	{ OFFS(txtw), "default width in standalone rendering", GF_PROP_UINT, "400", NULL, 0},
	{ OFFS(txth), "default height in standalone rendering", GF_PROP_UINT, "200", NULL, 0},
	{0}
};

static const GF_FilterCapability TTMLDecCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_SUBS_XML),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

GF_FilterRegister TTMLDecRegister = {
	.name = "ttmldec",
	GF_FS_SET_DESCRIPTION("TTML decoder")
	GF_FS_SET_HELP("This filter decodes TTML streams into a SVG scene graph of the compositor filter.\n"
		"The scene graph creation is done through JavaScript.\n"
		"The filter options are used to override the JS global variables of the TTML renderer.")
	.private_size = sizeof(GF_TTMLDec),
	.flags = GF_FS_REG_MAIN_THREAD,
	.args = TTMLDecArgs,
	.priority = 1,
	SETCAPS(TTMLDecCaps),
	.initialize = ttmldec_initialize,
	.finalize = ttmldec_finalize,
	.process = ttmldec_process,
	.configure_pid = ttmldec_configure_pid,
	.process_event = ttmldec_process_event,
	.update_arg = ttmldec_update_arg
};

#endif

const GF_FilterRegister *ttmldec_register(GF_FilterSession *session)
{
#if !defined(GPAC_DISABLE_SVG) && defined(GPAC_HAS_QJS)
	return &TTMLDecRegister;
#else
	return NULL;
#endif
}


