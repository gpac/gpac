/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Cyril Concolato - Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2013-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / WebVTT decoder filter
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

#if !defined(GPAC_DISABLE_VTT) && !defined(GPAC_DISABLE_SVG) && defined(GPAC_HAS_QJS)

#include <gpac/internal/isomedia_dev.h>
#include <gpac/internal/media_dev.h>
#include <gpac/internal/scenegraph_dev.h>
#include <gpac/internal/compositor_dev.h>
#include <gpac/nodes_svg.h>
#include <gpac/webvtt.h>

#include "../scenegraph/qjs_common.h"

typedef struct
{
	//opts
	char *script;
	char *color, *font;
	Float fontSize, lineSpacing;
	u32 txtw, txth;

	GF_FilterPid *ipid, *opid;

	/* config of the VTT stream - not used by the renderer for now */
	char *dsi;
	//CRC of the config string
	u32 dsi_crc;

	/* Boolean indicating the internal graph is registered with the compositor */
	Bool graph_registered;
	/* Boolean indicating the stream is playing */
	Bool is_playing;

	/* Scene to which this WebVTT stream is linked */
	GF_Scene *scene;
	/* object manager corresponding to the output pid declared*/
	GF_ObjectManager *odm;

	GF_List *cues;

	/* Scene graph for the subtitle content */
	GF_SceneGraph *scenegraph;

	u32 timescale;
	s64 delay;
	u64 cue_end;

	Bool update_args;
	s32 txtx, txty;
	u32 fsize;
	u32 vp_w, vp_h;
} GF_VTTDec;

void vttd_update_size_info(GF_VTTDec *ctx)
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
		else if (h<=3*ctx->fontSize) h *= 2;

		gf_sg_set_scene_size_info(ctx->scenegraph, w, h, GF_TRUE);
		gf_scene_force_size(ctx->scene, w, h);
	}

	gf_sg_set_scene_size_info(ctx->scenegraph, w, h, GF_TRUE);

	sprintf(szVB, "0 0 %d %d", w, h);
	gf_node_get_attribute_by_tag(root, TAG_SVG_ATT_viewBox, GF_TRUE, GF_FALSE, &info);
	gf_svg_parse_attribute(root, &info, szVB, 0);

	/*apply*/
	gf_sg_set_scene_size_info(ctx->scenegraph, w, h, GF_TRUE);

	sprintf(szVB, "0 0 %d %d", w, h);
	gf_node_get_attribute_by_tag(root, TAG_SVG_ATT_viewBox, GF_TRUE, GF_FALSE, &info);
	gf_svg_parse_attribute(root, &info, szVB, 0);
	ctx->vp_w = w;
	ctx->vp_h = h;
}

void vttd_setup_scene(GF_VTTDec *ctx)
{
	GF_Node *n, *root;
	GF_FieldInfo info;

	ctx->scenegraph = gf_sg_new_subscene(ctx->scene->graph);

	if (!ctx->scenegraph) return;
	gf_sg_add_namespace(ctx->scenegraph, "http://www.w3.org/2000/svg", NULL);
	gf_sg_add_namespace(ctx->scenegraph, "http://www.w3.org/1999/xlink", "xlink");
	gf_sg_add_namespace(ctx->scenegraph, "http://www.w3.org/2001/xml-events", "ev");
	gf_sg_set_scene_size_info(ctx->scenegraph, 800, 600, GF_TRUE);

	/* modify the scene with an Inline/Animation pointing to the VTT Renderer */
	n = root = gf_node_new(ctx->scenegraph, TAG_SVG_svg);
	gf_node_register(root, NULL);
	gf_sg_set_root_node(ctx->scenegraph, root);
	gf_node_get_attribute_by_name(n, "xmlns", 0, GF_TRUE, GF_FALSE, &info);
	gf_svg_parse_attribute(n, &info, "http://www.w3.org/2000/svg", 0);
	vttd_update_size_info(ctx);
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

static GF_Err vttd_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 entry_type;
	GF_BitStream *bs;
	u32 dsi_crc;
	const GF_PropertyValue *p;
	GF_VTTDec *ctx = (GF_VTTDec *)gf_filter_get_udta(filter);

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

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DELAY);
	ctx->delay = p ? p->value.longsint : 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	ctx->timescale = p ? p->value.uint: 1000;

	ctx->ipid = pid;
	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
	}

	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_TEXT));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW));

	//decoder config not yet ready
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (!p) return GF_OK;

	dsi_crc = gf_crc_32(p->value.data.ptr, p->value.data.size);
	if (dsi_crc == ctx->dsi_crc) return GF_OK;
	ctx->dsi_crc = dsi_crc;

	//parse DSI
	bs = gf_bs_new(p->value.data.ptr, p->value.data.size, GF_BITSTREAM_READ);
	entry_type = gf_bs_read_u32(bs);
	if (entry_type == GF_ISOM_BOX_TYPE_WVTT) {
		GF_Box *b;
		gf_isom_box_parse(&b, bs);
		ctx->dsi = ((GF_StringBox *)b)->string;
		((GF_StringBox *)b)->string = NULL;
		gf_isom_box_del(b);
	} else {
		ctx->dsi = gf_malloc( p->value.data.size + 1);
		memcpy(ctx->dsi, p->value.data.ptr, p->value.data.size);
		ctx->dsi[p->value.data.size] = 0;
	}
	gf_bs_del(bs);

	return GF_OK;
}


static void vttd_toggle_display(GF_VTTDec *ctx)
{
	if (!ctx->scenegraph) return;

	if (ctx->is_playing) {
		if (!ctx->graph_registered) {
			vttd_update_size_info(ctx);
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

static Bool vttd_process_event(GF_Filter *filter, const GF_FilterEvent *com)
{
	GF_VTTDec *ctx = gf_filter_get_udta(filter);

	//check for scene attach
	switch (com->base.type) {
	case GF_FEVT_ATTACH_SCENE:
		break;
	case GF_FEVT_RESET_SCENE:
		if (ctx->opid != com->attach_scene.on_pid) return GF_TRUE;
		ctx->is_playing = GF_FALSE;
		vttd_toggle_display(ctx);

		gf_sg_del(ctx->scenegraph);
		ctx->scenegraph = NULL;
		ctx->scene = NULL;
		return GF_TRUE;
	case GF_FEVT_PLAY:
		ctx->is_playing = GF_TRUE;
		vttd_toggle_display(ctx);
		return GF_FALSE;
	case GF_FEVT_STOP:
		ctx->is_playing = GF_FALSE;
		vttd_toggle_display(ctx);
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
		 vttd_setup_scene(ctx);
		 vttd_toggle_display(ctx);
	 }
	 return GF_TRUE;
}

void js_dump_error(JSContext *ctx);

JSContext *vtt_script_get_context(GF_VTTDec *ctx, GF_SceneGraph *sg)
{
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

		JS_FreeValue(c, global);
		ctx->update_args = GF_FALSE;
	}
	return c;
}


static GF_Err vttd_js_add_cue(GF_VTTDec *ctx, GF_Node *node, const char *id, const char *start, const char *end, const char *settings, const char *payload, char *comment)
{
	GF_Err e=GF_OK;
	JSValue fun_val;
	JSValue global;
	JSContext *c = vtt_script_get_context(ctx, node->sgprivate->scenegraph);
	if (!c) return GF_SERVICE_ERROR;

	gf_js_lock(c, GF_TRUE);
	global = JS_GetGlobalObject(c);
	fun_val = JS_GetPropertyStr(c, global, "addCue");
	if (!JS_IsFunction(c, fun_val) ) {
		e = GF_BAD_PARAM;
	} else {
		JSValue ret, argv[6];

		argv[0] = JS_NewString(c, id ? id : "");
		argv[1] = JS_NewString(c, start ? start : "");
		argv[2] = JS_NewString(c, end ? end : "");
		argv[3] = JS_NewString(c, settings ? settings : "");
		argv[4] = JS_NewString(c, payload ? payload : "");
		argv[5] = JS_NewString(c, comment ? comment : "");

		ret = JS_Call(c, fun_val, global, 5, argv);
		if (JS_IsException(ret)) {
			js_dump_error(c);
			e = GF_BAD_PARAM;
		}
		JS_FreeValue(c, ret);
		JS_FreeValue(c, argv[0]);
		JS_FreeValue(c, argv[1]);
		JS_FreeValue(c, argv[2]);
		JS_FreeValue(c, argv[3]);
		JS_FreeValue(c, argv[4]);
		JS_FreeValue(c, argv[5]);
	}
	JS_FreeValue(c, global);
	JS_FreeValue(c, fun_val);

	gf_js_lock(c, GF_FALSE);
	return e;
}

static GF_Err vttd_js_remove_cues(GF_VTTDec *ctx, GF_Node *node)
{
	GF_Err e = GF_OK;
	JSValue fun_val;
	JSValue global;
	JSContext *c = vtt_script_get_context(ctx, node->sgprivate->scenegraph);
	if (!c) return GF_SERVICE_ERROR;

	gf_js_lock(c, GF_TRUE);
	global = JS_GetGlobalObject(c);
	fun_val = JS_GetPropertyStr(c, global, "removeCues");
	if (!JS_IsFunction(c, fun_val) ) {
		e = GF_BAD_PARAM;
	} else {
		JSValue ret = JS_Call(c, fun_val, global, 0, NULL);
		if (JS_IsException(ret)) {
			js_dump_error(c);
			e = GF_BAD_PARAM;
		}
		JS_FreeValue(c, ret);
	}
	JS_FreeValue(c, global);
	JS_FreeValue(c, fun_val);
	gf_js_lock(c, GF_FALSE);
	return e;
}

static GF_Err vttd_process(GF_Filter *filter)
{
	GF_Err e = GF_OK;
	GF_FilterPacket *pck;
	GF_List *cues;
	const char *pck_data;
	u64 cts;
	u32 pck_size;
	GF_VTTDec *ctx = (GF_VTTDec *) gf_filter_get_udta(filter);

	if (!ctx->scene) {
		if (ctx->is_playing) {
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck && !ctx->cue_end) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}

	//object clock shall be valid
	if (!ctx->odm->ck)
		return GF_OK;

	if (ctx->cue_end) {
		u32 obj_time = gf_clock_time(ctx->odm->ck);
		if (gf_clock_diff(ctx->odm->ck, obj_time, (u32) ctx->cue_end)<=0) {
			vttd_js_remove_cues(ctx, ctx->scenegraph->RootNode);
			ctx->cue_end = 0;
		}
		if (!pck) {
			if (ctx->cue_end && gf_filter_pid_is_eos(ctx->ipid))
				gf_sc_sys_frame_pending(ctx->scene->compositor, (u32) ctx->cue_end, obj_time, filter);
			return GF_OK;
		}
	}

	cts = gf_filter_pck_get_cts( pck );
	s64 delay = ctx->scene->compositor->subd;
	if (delay)
		delay = gf_timestamp_rescale_signed(delay, 1000, ctx->timescale);
	delay += ctx->delay;

	if (delay>=0) cts += delay;
	else if (cts > (u64) -delay) cts -= -delay;
	else cts = 0;
	cts = gf_timestamp_to_clocktime(cts, ctx->timescale);

	u32 dur = gf_timestamp_rescale( gf_filter_pck_get_duration(pck), ctx->timescale, 1000);
	if (!gf_sc_check_sys_frame(ctx->scene, ctx->odm, ctx->ipid, filter, cts, dur))
		return GF_OK;

	pck_data = gf_filter_pck_get_data(pck, &pck_size);

	ctx->cue_end = cts + gf_timestamp_rescale(gf_filter_pck_get_duration(pck), ctx->timescale, 1000);

	cues = gf_webvtt_parse_cues_from_data(pck_data, pck_size, 0, 0);
	vttd_js_remove_cues(ctx, ctx->scenegraph->RootNode);
	if (gf_list_count(cues)) {
		while (gf_list_count(cues)) {
			char start[100], end[100];
			GF_WebVTTCue *cue = (GF_WebVTTCue *)gf_list_get(cues, 0);
			gf_list_rem(cues, 0);
			sprintf(start, "%02d:%02d:%02d.%03d", cue->start.hour, cue->start.min, cue->start.sec, cue->start.ms);
			sprintf(end, "%02d:%02d:%02d.%03d", cue->end.hour, cue->end.min, cue->end.sec, cue->end.ms);
			vttd_js_add_cue(ctx, ctx->scenegraph->RootNode, cue->id, start, end, cue->settings, cue->text, cue->pre_text);

			gf_webvtt_cue_del(cue);
		}
	}
	gf_list_del(cues);
	gf_filter_pid_drop_packet(ctx->ipid);
	return e;
}

static GF_Err vtt_update_arg(GF_Filter *filter, const char *arg_name, const GF_PropertyValue *new_val)
{
	GF_VTTDec *ctx = gf_filter_get_udta(filter);
	ctx->update_args = GF_TRUE;
	return GF_OK;
}

static GF_Err vttd_initialize(GF_Filter *filter)
{
	GF_VTTDec *ctx = gf_filter_get_udta(filter);

	if (!ctx->script) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[VTTDec] WebVTT JS renderer script not set\n"));
		return GF_BAD_PARAM;
	} else if (!gf_file_exists(ctx->script)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[VTTDec] WebVTT JS renderer script %s not found\n", ctx->script));
		return GF_URL_ERROR;
	}
	ctx->cues = gf_list_new();
	ctx->update_args = GF_TRUE;
#ifdef GPAC_ENABLE_COVERAGE
	vtt_update_arg(filter, NULL, NULL);
#endif
	return GF_OK;
}

void vttd_finalize(GF_Filter *filter)
{
	GF_VTTDec *ctx = (GF_VTTDec *) gf_filter_get_udta(filter);

	if (ctx->scenegraph) {
		if (ctx->graph_registered) {
			gf_scene_register_extra_graph(ctx->scene, ctx->scenegraph, GF_TRUE);
		}
		gf_sg_del(ctx->scenegraph);
	}

	if (ctx->cues) gf_list_del(ctx->cues);
	if (ctx->dsi) gf_free(ctx->dsi);
}

#define OFFS(_n)	#_n, offsetof(GF_VTTDec, _n)
static const GF_FilterArgs VTTDecArgs[] =
{
	{ OFFS(script), "location of WebVTT SVG JS renderer", GF_PROP_STRING, "$GSHARE/scripts/webvtt-renderer.js", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(font), "font", GF_PROP_STRING, "SANS", NULL, GF_FS_ARG_HINT_ADVANCED|GF_FS_ARG_UPDATE},
	{ OFFS(fontSize), "font size", GF_PROP_FLOAT, "20", NULL, GF_FS_ARG_HINT_ADVANCED|GF_FS_ARG_UPDATE},
	{ OFFS(color), "text color", GF_PROP_STRING, "white", NULL, GF_FS_ARG_HINT_ADVANCED|GF_FS_ARG_UPDATE},
	{ OFFS(lineSpacing), "line spacing as scaling factor to font size", GF_PROP_FLOAT, "1.0", NULL, GF_FS_ARG_HINT_ADVANCED|GF_FS_ARG_UPDATE},
	{ OFFS(txtw), "default width in standalone rendering", GF_PROP_UINT, "400", NULL, 0},
	{ OFFS(txth), "default height in standalone rendering", GF_PROP_UINT, "200", NULL, 0},
	{0}
};

static const GF_FilterCapability VTTDecCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_ISOM_SUBTYPE_WVTT),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

GF_FilterRegister VTTDecRegister = {
	.name = "vttdec",
	GF_FS_SET_DESCRIPTION("WebVTT decoder")
	GF_FS_SET_HELP("This filter decodes WebVTT streams into a SVG scene graph of the compositor filter.\n"
	"The scene graph creation is done through JavaScript.\n"
	"The filter options are used to override the JS global variables of the WebVTT renderer.")
	.private_size = sizeof(GF_VTTDec),
	.flags = GF_FS_REG_MAIN_THREAD,
	.args = VTTDecArgs,
	.priority = 1,
	SETCAPS(VTTDecCaps),
	.initialize = vttd_initialize,
	.finalize = vttd_finalize,
	.process = vttd_process,
	.configure_pid = vttd_configure_pid,
	.process_event = vttd_process_event,
	.update_arg = vtt_update_arg
};

#endif

const GF_FilterRegister *vttdec_register(GF_FilterSession *session)
{
#if !defined(GPAC_DISABLE_VTT) && !defined(GPAC_DISABLE_SVG) && defined(GPAC_HAS_QJS)
	return &VTTDecRegister;
#else
	return NULL;
#endif
}


