/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2007-2022
 *			All rights reserved
 *
 *  This file is part of GPAC / JavaScript Compositor extensions
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


#include <gpac/setup.h>

#if defined(GPAC_HAS_QJS) && !defined(GPAC_DISABLE_PLAYER)

/*base SVG type*/
#include <gpac/nodes_svg.h>
#include <gpac/nodes_mpeg4.h>
#include <gpac/nodes_x3d.h>
/*dom events*/
#include <gpac/events.h>

#include <gpac/download.h>
#include <gpac/network.h>
#include <gpac/xml.h>


#include <gpac/internal/scenegraph_dev.h>

#include "../scenegraph/qjs_common.h"

#include <gpac/internal/compositor_dev.h>

typedef struct
{
	GF_Compositor *compositor;
	JSValue evt_fun;
	GF_FSEventListener evt_filter;
	GF_Event *evt;

	JSContext *c;
	JSValue evt_filter_obj, evt_obj, scene_obj;

	GF_SystemRTInfo rti;

	//list of config files for storage
	GF_List *storages;

	GF_List *event_queue;
	GF_Mutex *event_mx;

} GF_SCENEJSExt;

enum {
	GJS_OM_PROP_ID=1,
	GJS_OM_PROP_NB_RES,
	GJS_OM_PROP_URL,
	GJS_OM_PROP_DUR,
	GJS_OM_PROP_CLOCK,
	GJS_OM_PROP_DRIFT,
	GJS_OM_PROP_STATUS,
	GJS_OM_PROP_BUFFER,
	GJS_OM_PROP_DB_COUNT,
	GJS_OM_PROP_CB_COUNT,
	GJS_OM_PROP_CB_CAP,
	GJS_OM_PROP_TYPE,
	GJS_OM_PROP_SAMPLERATE,
	GJS_OM_PROP_CHANNELS,
	GJS_OM_PROP_LANG,
	GJS_OM_PROP_WIDTH,
	GJS_OM_PROP_HEIGHT,
	GJS_OM_PROP_PIXELFORMAT,
	GJS_OM_PROP_PAR,
	GJS_OM_PROP_DEC_FRAMES,
	GJS_OM_PROP_DROP_FRAMES,
	GJS_OM_PROP_DEC_TIME_MAX,
	GJS_OM_PROP_DEC_TIME_TOTAL,
	GJS_OM_PROP_AVG_RATE,
	GJS_OM_PROP_MAX_RATE,
	GJS_OM_PROP_SERVICE_HANDLER,
	GJS_OM_PROP_CODEC,
	GJS_OM_PROP_NB_VIEWS,
	GJS_OM_PROP_NB_QUALITIES,
	GJS_OM_PROP_MAX_BUFFER,
	GJS_OM_PROP_MIN_BUFFER,
	GJS_OM_PROP_FRAME_DUR,
	GJS_OM_PROP_NB_IRAP,
	GJS_OM_PROP_IRAP_DEC_TIME,
	GJS_OM_PROP_IRAP_MAX_TIME,
	GJS_OM_PROP_SERVICE_ID,
	GJS_OM_PROP_SELECTED_SERVICE,
	GJS_OM_PROP_BANDWIDTH_DOWN,
	GJS_OM_PROP_NB_HTTP,
	GJS_OM_PROP_TIMESHIFT_DEPTH,
	GJS_OM_PROP_TIMESHIFT_TIME,
	GJS_OM_PROP_IS_ADDON,
	GJS_OM_PROP_MAIN_ADDON_ON,
	GJS_OM_PROP_IS_OVER,
	GJS_OM_PROP_IS_VR_SCENE,
	GJS_OM_PROP_DYNAMIC_SCENE,
	GJS_OM_PROP_SERVICE_NAME,
	GJS_OM_PROP_NTP_DIFF,
	GJS_OM_PROP_MAIN_ADDON_URL,
	GJS_OM_PROP_REVERSE_PLAYBACK,
	GJS_OM_PROP_SCALABLE_ENHANCEMENT,
	GJS_OM_PROP_MAIN_ADDON_MEDIATIME,
	GJS_OM_PROP_DEPENDENT_GROUPS,
	GJS_OM_PROP_DISABLED,
	GJS_OM_PROP_NTP_SENDER_DIFF,
	GJS_OM_PROP_BUFFERING,
};

enum {
	GJS_SCENE_PROP_FULLSCREEN=1,
	GJS_SCENE_PROP_CURRENT_PATH,
	GJS_SCENE_PROP_VOLUME,
	GJS_SCENE_PROP_NAVIGATION,
	GJS_SCENE_PROP_NAVIGATION_TYPE,
	GJS_SCENE_PROP_HARDWARE_YUV,
	GJS_SCENE_PROP_HARDWARE_RGB,
	GJS_SCENE_PROP_HARDWARE_RGBA,
	GJS_SCENE_PROP_HARDWARE_STRETCH,
	GJS_SCENE_PROP_SCREEN_WIDTH,
	GJS_SCENE_PROP_SCREEN_HEIGHT,
	GJS_SCENE_PROP_FPS,
	GJS_SCENE_PROP_CAPTION,
	GJS_SCENE_PROP_FOCUS_HIGHLIGHT,
	GJS_SCENE_PROP_DPI_X,
	GJS_SCENE_PROP_DPI_Y,
	GJS_SCENE_PROP_SENSORS_ACTIVE,
	GJS_SCENE_PROP_SIM_FPS,
	GJS_SCENE_PROP_HAS_OPENGL,
	GJS_SCENE_PROP_ZOOM,
	GJS_SCENE_PROP_TEXT_SEL,
	GJS_SCENE_PROP_DISP_ORIENTATION,

};

enum {
	GJS_EVT_PROP_KEYCODE = 1,
	GJS_EVT_PROP_MOUSE_X,
	GJS_EVT_PROP_MOUSE_Y,
	GJS_EVT_PROP_PICKED,
	GJS_EVT_PROP_WHEEL,
	GJS_EVT_PROP_BUTTON,
	GJS_EVT_PROP_TYPE,
	GJS_EVT_PROP_NAME,
	GJS_EVT_PROP_HWKEY,
	GJS_EVT_PROP_TARGET_URL,
	GJS_EVT_PROP_FILES,
};

static JSClassID scene_class_id = 0;
static JSClassID odm_class_id = 0;
static JSClassID gpacevt_class_id = 0;
static JSClassID any_class_id = 0;

static void scenejs_finalize(JSRuntime *rt, JSValue obj);

static void scenejs_gc_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func)
{
	GF_SCENEJSExt *ext = JS_GetOpaque(val, scene_class_id);
    if (ext) {
		JS_MarkValue(rt, ext->evt_fun, mark_func);
    }
}

JSClassDef sceneClass = {
    "JSSCENE",
    .finalizer = scenejs_finalize,
    .gc_mark = scenejs_gc_mark
};
JSClassDef gpacEvtClass = {
    "GPACEVT"
};
JSClassDef odmClass = {
    "MediaObject"
};
JSClassDef anyClass = {
	"GPACOBJECT"
};


static GF_Compositor *scenejs_get_compositor(JSContext *c, JSValue obj)
{
	GF_SCENEJSExt *ext = JS_GetOpaque(obj, scene_class_id);
	return ext ? ext->compositor : NULL;
}

static JSValue scenejs_getProperty(JSContext *ctx, JSValueConst this_val, int prop_id)
{
	Bool bval;
	JSValue ret;
	char *str;
	GF_SCENEJSExt *ext = JS_GetOpaque(this_val, scene_class_id);
	GF_Compositor *compositor = ext ? ext->compositor : NULL;
	if (!ext || !compositor) return GF_JS_EXCEPTION(ctx);

	switch (prop_id) {

	case GJS_SCENE_PROP_FULLSCREEN:
		return JS_NewBool(ctx, compositor->fullscreen);

	case GJS_SCENE_PROP_CURRENT_PATH:
		str = gf_url_concatenate(compositor->root_scene->root_od->scene_ns->url, "");
		if (!str) str = gf_strdup("");
		ret = JS_NewString(ctx, str);
		gf_free(str);
		return ret;

	case GJS_SCENE_PROP_VOLUME:
		return JS_NewInt32(ctx, gf_sc_get_option(compositor, GF_OPT_AUDIO_VOLUME));

	case GJS_SCENE_PROP_NAVIGATION:
		return JS_NewInt32(ctx, gf_sc_get_option(compositor, GF_OPT_NAVIGATION));

	case GJS_SCENE_PROP_NAVIGATION_TYPE:
		return JS_NewInt32(ctx, gf_sc_get_option(compositor, GF_OPT_NAVIGATION_TYPE) );

	case GJS_SCENE_PROP_HARDWARE_YUV:
		return JS_NewBool(ctx, (compositor->video_out->hw_caps & GF_VIDEO_HW_HAS_YUV) ? 1 : 0 );

	case GJS_SCENE_PROP_HARDWARE_RGB:
		return JS_NewBool(ctx, (compositor->video_out->hw_caps & GF_VIDEO_HW_HAS_RGB) ? 1 : 0 );

	case GJS_SCENE_PROP_HARDWARE_RGBA:
		bval = (compositor->video_out->hw_caps & GF_VIDEO_HW_HAS_RGBA) ? 1 : 0;
#ifndef GPAC_DISABLE_3D
		if (compositor->hybrid_opengl || compositor->is_opengl) bval = 1;
#endif
		return JS_NewBool(ctx, bval);

	case GJS_SCENE_PROP_HARDWARE_STRETCH:
		return JS_NewBool(ctx, (compositor->video_out->hw_caps & GF_VIDEO_HW_HAS_STRETCH) ? 1 : 0 );

	case GJS_SCENE_PROP_SCREEN_WIDTH:
		if (compositor->osize.x)
			return JS_NewInt32(ctx, compositor->osize.x);
		return JS_NewInt32(ctx, compositor->video_out->max_screen_width);

	case GJS_SCENE_PROP_SCREEN_HEIGHT:
		if (compositor->osize.y)
			return JS_NewInt32(ctx, compositor->osize.y);
		return JS_NewInt32(ctx, compositor->video_out->max_screen_height);

	case GJS_SCENE_PROP_FPS:
		return JS_NewFloat64(ctx, gf_sc_get_fps(compositor, 0) );

	case GJS_SCENE_PROP_SIM_FPS:
		return JS_NewFloat64(ctx, ((Double)compositor->fps.den)/compositor->fps.num);

	case GJS_SCENE_PROP_HAS_OPENGL:
		return JS_NewBool(ctx, (compositor->ogl != GF_SC_GLMODE_OFF) ? 1 : 0);

	case GJS_SCENE_PROP_DPI_X:
		return JS_NewInt32(ctx, compositor->video_out->dpi_x);

	case GJS_SCENE_PROP_DPI_Y:
		return JS_NewInt32(ctx, compositor->video_out->dpi_y);

	case GJS_SCENE_PROP_SENSORS_ACTIVE:
		return JS_NewBool(ctx, compositor->orientation_sensors_active);

	case GJS_SCENE_PROP_FOCUS_HIGHLIGHT:
		return JS_NewBool(ctx, !compositor->disable_focus_highlight);

	case GJS_SCENE_PROP_ZOOM:
		if (compositor->root_scene && compositor->root_scene->graph->script_action) {
			GF_JSAPIParam jspar;
			memset(&jspar, 0, sizeof(GF_JSAPIParam));
			compositor->root_scene->graph->script_action(compositor->root_scene->graph->script_action_cbck, GF_JSAPI_OP_GET_SCALE, NULL, &jspar);
#ifdef GPAC_ENABLE_COVERAGE
			if (gf_sys_is_cov_mode()) {
				compositor->root_scene->graph->script_action(compositor->root_scene->graph->script_action_cbck, GF_JSAPI_OP_GET_VIEWPORT, NULL, &jspar);
			}
#endif
			return JS_NewFloat64(ctx, FIX2FLT(jspar.val) );
		} else {
			return JS_NewFloat64(ctx, 1.0 );
		}
		break;
	case GJS_SCENE_PROP_TEXT_SEL:
		str = (char *) gf_sc_get_selected_text(compositor);
		if (!str) str = "";
		return JS_NewString(ctx, str);


	case GJS_SCENE_PROP_DISP_ORIENTATION:
		return JS_NewInt32(ctx, compositor->disp_ori);
	}
	return JS_UNDEFINED;
}


static JSValue scenejs_setProperty(JSContext *ctx, JSValueConst this_val, JSValueConst value, int magic)
{
#ifndef GPAC_CONFIG_IOS
	Bool bval;
#endif
	s32 ival;
	Double dval;
	const char *prop_val;
	GF_SCENEJSExt *ext = JS_GetOpaque(this_val, scene_class_id);
	GF_Compositor *compositor = ext ? ext->compositor : NULL;
	if (!ext || !compositor) return GF_JS_EXCEPTION(ctx);

	switch (magic) {
	case GJS_SCENE_PROP_CAPTION:
	{
		GF_Event evt;
		if (!JS_IsString(value)) return GF_JS_EXCEPTION(ctx);
		prop_val = JS_ToCString(ctx, value);
		evt.type = GF_EVENT_SET_CAPTION;
		if (prop_val && !strnicmp(prop_val, "gpac://", 7)) {
			evt.caption.caption = prop_val + 7;
		} else {
			evt.caption.caption = prop_val;
		}
		gf_sc_user_event(compositor, &evt);
		JS_FreeCString(ctx, prop_val);
	}
		break;
	case GJS_SCENE_PROP_FULLSCREEN:
		/*no fullscreen for iOS (always on)*/
#ifndef GPAC_CONFIG_IOS
		bval = JS_ToBool(ctx, value);
		if (compositor->fullscreen != bval) {
			gf_sc_set_option(compositor, GF_OPT_FULLSCREEN, bval);
		}
#endif
		break;
	case GJS_SCENE_PROP_VOLUME:
		if (!JS_ToFloat64(ctx, &dval, value)) {
			gf_sc_set_option(compositor, GF_OPT_AUDIO_VOLUME, (u32) dval);
		} else if (!JS_ToInt32(ctx, &ival, value)) {
			gf_sc_set_option(compositor, GF_OPT_AUDIO_VOLUME, (u32) ival);
		}
		break;

	case GJS_SCENE_PROP_NAVIGATION:
		if (JS_ToInt32(ctx, &ival, value)) return GF_JS_EXCEPTION(ctx);
		gf_sc_set_option(compositor, GF_OPT_NAVIGATION, (u32) ival);
		break;
	case GJS_SCENE_PROP_NAVIGATION_TYPE:
		gf_sc_set_option(compositor, GF_OPT_NAVIGATION_TYPE, 0);
		break;
	case GJS_SCENE_PROP_FOCUS_HIGHLIGHT:
		compositor->disable_focus_highlight = !JS_ToBool(ctx, value);
		break;
	case GJS_SCENE_PROP_SENSORS_ACTIVE:
	{
		GF_Event evt;
		compositor->orientation_sensors_active = JS_ToBool(ctx, value);
		//send activation for sensors
		memset(&evt, 0, sizeof(GF_Event));
		evt.type = GF_EVENT_SENSOR_REQUEST;
		evt.activate_sensor.activate = compositor->orientation_sensors_active;
		evt.activate_sensor.sensor_type = GF_EVENT_SENSOR_ORIENTATION;
		if (gf_sc_send_event(compositor, &evt) == GF_FALSE) {
			compositor->orientation_sensors_active = GF_FALSE;
		}
	}
		break;
	case GJS_SCENE_PROP_DISP_ORIENTATION:
		if (JS_ToInt32(ctx, &ival, value)) return GF_JS_EXCEPTION(ctx);
		if (compositor->video_out && compositor->video_out->ProcessEvent) {
			GF_Event evt;
			memset(&evt, 0, sizeof(GF_Event));
			evt.size.type = GF_EVENT_SET_ORIENTATION;
			evt.size.orientation = ival;
			compositor->video_out->ProcessEvent(compositor->video_out, &evt);
		}
		break;
	}
	return JS_UNDEFINED;
}

static JSValue scenejs_get_option(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *opt = NULL;
	const char *sec_name, *key_name;
	char arg_val[GF_PROP_DUMP_ARG_SIZE];
	s32 idx = -1;
	GF_Compositor *compositor = scenejs_get_compositor(ctx, this_val);
	if (!compositor) return GF_JS_EXCEPTION(ctx);
	if (argc < 2) return GF_JS_EXCEPTION(ctx);
	if (!JS_IsString(argv[0])) return GF_JS_EXCEPTION(ctx);
	if (!JS_IsString(argv[1]) && !JS_IsNumber(argv[1])) return GF_JS_EXCEPTION(ctx);

	sec_name = JS_ToCString(ctx, argv[0]);
	key_name = NULL;
	if (JS_IsNumber(argv[1])) {
		JS_ToInt32(ctx, &idx, argv[1]);
	} else if (JS_IsString(argv[1]) ) {
		key_name = JS_ToCString(ctx, argv[1]);
	}

	if (sec_name && !stricmp(sec_name, "core") && key_name && !strcmp(key_name, "version")) {
		opt = gf_gpac_version();
	} else if (sec_name && key_name) {
		if (!strcmp(sec_name, "Compositor")) {
			opt = gf_filter_get_arg_str(compositor->filter, key_name, arg_val);
		} else {
			opt = gf_opts_get_key(sec_name, key_name);
		}
	} else if (sec_name && (idx>=0)) {
		opt = gf_opts_get_key_name(sec_name, idx);
	} else {
		opt = NULL;
	}

	JS_FreeCString(ctx, key_name);
	JS_FreeCString(ctx, sec_name);

	if (opt) {
		return JS_NewString(ctx, opt);
	}
	return JS_NULL;
}

static JSValue scenejs_set_option(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *sec_name, *key_name, *key_val;
	GF_Compositor *compositor = scenejs_get_compositor(ctx, this_val);
	if (!compositor) return GF_JS_EXCEPTION(ctx);
	if (argc < 3) return GF_JS_EXCEPTION(ctx);
	if (!JS_IsString(argv[0])) return GF_JS_EXCEPTION(ctx);
	if (!JS_IsString(argv[1])) return GF_JS_EXCEPTION(ctx);

	sec_name = JS_ToCString(ctx, argv[0]);
	key_name = JS_ToCString(ctx, argv[1]);
	key_val = NULL;
	if (JS_IsString(argv[2]))
		key_val = JS_ToCString(ctx, argv[2]);

	if (!strcmp(sec_name, "Compositor")) {
		gf_filter_send_update(compositor->filter, NULL, key_name, key_val, 0);
	} else {
		gf_opts_set_key(sec_name, key_name, key_val);
	}
	JS_FreeCString(ctx, sec_name);
	JS_FreeCString(ctx, key_name);
	if (key_val) {
		JS_FreeCString(ctx, key_val);
	}
	return JS_UNDEFINED;
}


static JSValue scenejs_set_back_color(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 r, g, b, a, i;
	GF_Compositor *compositor = scenejs_get_compositor(ctx, this_val);
	if (!compositor) return GF_JS_EXCEPTION(ctx);
	if (argc < 3) return GF_JS_EXCEPTION(ctx);
	r = g = b = 0;
	a = 255;
	for (i=0; i<(u32) argc; i++) {
		Double d;
		u32 v;
		if (! JS_ToFloat64(ctx, &d, argv[i])) {
		} else {
			return GF_JS_EXCEPTION(ctx);
		}
		d*=255;
		v = 0;
		if (d>0) {
			v = (u32) d;
			if (v>255)  v = 255;
		}
		if (i==0) r = v;
		else if (i==1) g = v;
		else if (i==2) b = v;
		else if (i==3) a = v;
	}
	compositor->back_color = compositor->bc = GF_COL_ARGB(a, r, g, b);
	gf_sc_invalidate(compositor, NULL);
	return JS_UNDEFINED;
}


static JSValue scenejs_switch_quality(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Compositor *compositor = scenejs_get_compositor(ctx, this_val);
	if (!compositor) return GF_JS_EXCEPTION(ctx);
	if (argc < 1) return GF_JS_EXCEPTION(ctx);

	if (!JS_IsBool(argv[0]))
		return GF_JS_EXCEPTION(ctx);
	Bool up = JS_ToBool(ctx, argv[0]) ? GF_TRUE : GF_FALSE;
	gf_scene_switch_quality(compositor->root_scene, up);
	return JS_UNDEFINED;
}


#if 0 //unused
static JSValue scenejs_reload(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Event evt;
	GF_Compositor *compositor = scenejs_get_compositor(ctx, this_val);
	if (!compositor) return GF_JS_EXCEPTION(ctx);

	memset(&evt, 0, sizeof(GF_Event));
	evt.type = GF_EVENT_RELOAD;
	gf_filter_ui_event(compositor->filter, &evt);
	return JS_UNDEFINED;
}
#endif

static JSValue scenejs_navigation_supported(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 type;
	GF_Compositor *compositor = scenejs_get_compositor(ctx, this_val);
	if (!compositor) return GF_JS_EXCEPTION(ctx);
	if (argc < 1) return GF_JS_EXCEPTION(ctx);

	if (! JS_IsInteger(argv[0]) ) {
		return JS_NewBool(ctx, 0);
	}
	if (JS_ToInt32(ctx, &type, argv[0]))
		return GF_JS_EXCEPTION(ctx);
	return JS_NewBool(ctx, gf_sc_navigation_supported(compositor, type) ? 1 : 0 );
}

static JSValue scenejs_set_size(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Bool override_size_info = 0;
	u32 w, h;
	GF_Compositor *compositor = scenejs_get_compositor(ctx, this_val);
	if (!compositor) return GF_JS_EXCEPTION(ctx);
	if (argc<2) return GF_JS_EXCEPTION(ctx);

	w = h = 0;
	if (JS_ToInt32(ctx, &w, argv[0]))
		return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt32(ctx, &h, argv[1]))
		return GF_JS_EXCEPTION(ctx);

	if (argc > 2)
		override_size_info = JS_ToBool(ctx, argv[2]);

	if (w && h) {
		GF_Event evt;
		if (override_size_info) {
			compositor->scene_width = w;
			compositor->scene_height = h;
			compositor->has_size_info = 1;
			compositor->recompute_ar = 1;
			return JS_UNDEFINED;
		}
		if (compositor->os_wnd) {
			evt.type = GF_EVENT_SCENE_SIZE;
			evt.size.width = w;
			evt.size.height = h;
			gf_sc_send_event(compositor, &evt);
		} else {
			gf_sc_set_size(compositor, w, h);
		}
	} else if (override_size_info) {
		compositor->has_size_info = 0;
		compositor->recompute_ar = 1;
	}

	return JS_UNDEFINED;
}


static JSValue scenejs_exit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Event evt;
	GF_Compositor *compositor = scenejs_get_compositor(ctx, this_val);
	memset(&evt, 0, sizeof(GF_Event));
	evt.type = GF_EVENT_QUIT;
	if (argc)
		JS_ToInt32(ctx, (s32 *) &evt.message.error, argv[0]);
		
	gf_sc_send_event(compositor, &evt);
	return JS_UNDEFINED;
}

static JSValue scenejs_set_3d(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Bool type_3d;
	GF_Compositor *compositor = scenejs_get_compositor(ctx, this_val);
	if (!argc) return GF_JS_EXCEPTION(ctx);
	type_3d = JS_ToBool(ctx, argv[0]);

	if (compositor->inherit_type_3d != type_3d) {
		compositor->inherit_type_3d = type_3d;
		compositor->root_visual_setup = 0;
		gf_sc_reset_graphics(compositor);
	}
	return JS_UNDEFINED;
}

static JSValue scenejs_move_window(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Event evt;
	GF_Compositor *compositor = scenejs_get_compositor(ctx, this_val);
	if (argc < 2) return GF_JS_EXCEPTION(ctx);

	evt.type = GF_EVENT_MOVE;
	evt.move.relative = 1;
	if (JS_ToInt32(ctx, &evt.move.x, argv[0]))
		return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt32(ctx, &evt.move.y, argv[1]))
		return GF_JS_EXCEPTION(ctx);

	if (argc ==3) {
		evt.move.relative = JS_ToBool(ctx, argv[2]);
	}
	compositor->video_out->ProcessEvent(compositor->video_out, &evt);
	return JS_UNDEFINED;
}


#if 0 //unused
static JSValue scenejs_get_scene_time(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_SceneGraph *sg = NULL;
	GF_Compositor *compositor = scenejs_get_compositor(ctx, this_val);
	if (!compositor) return GF_JS_EXCEPTION(ctx);

	if (!argc || !JS_IsObject(argv[0]) ) {
		sg = compositor->root_scene->graph;
	} else {
		GF_Node *n = gf_sg_js_get_node(c, JSVAL_TO_OBJECT(argv[0]));
		sg = n ? n->sgprivate->scenegraph : compositor->root_scene->graph;
	}
	return JS_NewFloat64(ctx, sg->GetSceneTime(sg->userpriv) );
}
#endif

static JSValue scenejs_trigger_gc(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_SceneGraph *sg;
	GF_Compositor *compositor = scenejs_get_compositor(ctx, this_val);
	sg = compositor->root_scene->graph;
	sg->trigger_gc = GF_TRUE;
	return JS_UNDEFINED;
}

static GF_FilterPid *gjs_enum_pids(void *udta, u32 *idx)
{
	GF_Scene *scene = (GF_Scene *)udta;
	GF_ObjectManager *odm = gf_list_get(scene->resources, *idx);
	if (odm) return odm->pid;
	return NULL;
}

static JSValue odm_getProperty(JSContext *ctx, JSValueConst this_val, int magic)
{
	GF_ObjectManager *odm = JS_GetOpaque(this_val, odm_class_id);
	GF_MediaInfo odi;
	char *str;
	GF_Scene *scene;
	Double dval;
	u32 i, count;

	switch (magic) {
	case GJS_OM_PROP_ID:
		return JS_NewInt32(ctx, odm->ID);
	case GJS_OM_PROP_NB_RES:
		return JS_NewInt32(ctx, odm->subscene ? gf_list_count(odm->subscene->resources) : 0);
	case GJS_OM_PROP_URL:
		if (odm->scene_ns)
			return JS_NewString(ctx, odm->scene_ns->url);
		return JS_NULL;
	case GJS_OM_PROP_DUR:
		return JS_NewFloat64(ctx, odm->duration / 1000.0);
	case GJS_OM_PROP_CLOCK:
		gf_odm_get_object_info(odm, &odi);
		return JS_NewFloat64(ctx, odi.current_time);
	case GJS_OM_PROP_DRIFT:
		gf_odm_get_object_info(odm, &odi);
		return JS_NewInt32(ctx, odi.clock_drift);
	case GJS_OM_PROP_STATUS:
		gf_odm_get_object_info(odm, &odi);
		if (odi.status==0) str = "Stopped";
		else if (odi.status==1) str = "Playing";
		else if (odi.status==2) str = "Paused";
		else if (odi.status==3) str = "Not Setup";
		else str = "Setup Failed";
		return JS_NewString(ctx, str);
	case GJS_OM_PROP_BUFFER:
		gf_odm_get_object_info(odm, &odi);
		return JS_NewInt32(ctx, odi.buffer);
	case GJS_OM_PROP_DB_COUNT:
		gf_odm_get_object_info(odm, &odi);
		return JS_NewInt32(ctx, odi.db_unit_count);
	case GJS_OM_PROP_CB_COUNT:
		gf_odm_get_object_info(odm, &odi);
		return JS_NewInt32(ctx, odi.cb_unit_count);
	case GJS_OM_PROP_CB_CAP:
		gf_odm_get_object_info(odm, &odi);
		return JS_NewInt32(ctx, odi.cb_max_count);
	case GJS_OM_PROP_TYPE:
		if (odm->type==GF_STREAM_SCENE) str = "Scene";
		else if (odm->type==GF_STREAM_OD) str = "OD";
		else if (odm->type==GF_STREAM_VISUAL) {
			//handle all sparse video as text (we could check the original stream type)
			if (odm->flags & GF_ODM_IS_SPARSE) str = "Text";
			else str = "Video";
		}
		else if (odm->type==GF_STREAM_AUDIO) str = "Audio";
		else if (odm->type==GF_STREAM_TEXT) str = "Text";
		else if (odm->subscene) str = "Subscene";
		else str = (char*) gf_stream_type_name(odm->type);
		return JS_NewString(ctx, str);
	case GJS_OM_PROP_SAMPLERATE:
		return JS_NewInt32(ctx, odm->mo ? odm->mo->sample_rate : 0);
	case GJS_OM_PROP_CHANNELS:
		return JS_NewInt32(ctx, odm->mo ? odm->mo->num_channels : 0);
	case GJS_OM_PROP_LANG:
		gf_odm_get_object_info(odm, &odi);
		if (odi.lang_code) return JS_NewString(ctx, odi.lang_code);
		else if (odi.lang) return JS_NewString(ctx, gf_4cc_to_str(odi.lang) );
		return JS_NewString(ctx, "und");
	case GJS_OM_PROP_WIDTH:
		return JS_NewInt32(ctx, odm->mo ? odm->mo->width : 0);
	case GJS_OM_PROP_HEIGHT:
		return JS_NewInt32(ctx, odm->mo ? odm->mo->height : 0);
	case GJS_OM_PROP_PIXELFORMAT:
		return JS_NewString(ctx, odm->mo ? gf_4cc_to_str(odm->mo->pixelformat) : "none");
	case GJS_OM_PROP_PAR:
		if (odm->mo && odm->mo->pixel_ar) {
			char szPar[50];
			sprintf(szPar, "%d:%d", (odm->mo->pixel_ar>>16)&0xFF, (odm->mo->pixel_ar)&0xFF );
			return JS_NewString(ctx, szPar);
		} else if (odm->mo && odm->mo->width) {
			return JS_NewString(ctx, "1:1");
		}
		return JS_NULL;
	case GJS_OM_PROP_DEC_FRAMES:
		gf_odm_get_object_info(odm, &odi);
		return JS_NewInt32(ctx, odi.nb_dec_frames);
	case GJS_OM_PROP_DROP_FRAMES:
		gf_odm_get_object_info(odm, &odi);
		return JS_NewInt32(ctx, odi.nb_dropped);
	case GJS_OM_PROP_DEC_TIME_MAX:
		gf_odm_get_object_info(odm, &odi);
		return JS_NewInt32(ctx, odi.max_dec_time);
	case GJS_OM_PROP_DEC_TIME_TOTAL:
		gf_odm_get_object_info(odm, &odi);
		return JS_NewInt64(ctx, odi.total_dec_time);
	case GJS_OM_PROP_AVG_RATE:
		gf_odm_get_object_info(odm, &odi);
		return JS_NewInt32(ctx, odi.avg_bitrate);
	case GJS_OM_PROP_MAX_RATE:
		gf_odm_get_object_info(odm, &odi);
		return JS_NewInt32(ctx, odi.max_bitrate);
	case GJS_OM_PROP_SERVICE_HANDLER:
		gf_odm_get_object_info(odm, &odi);
            return JS_NewString(ctx, odi.service_handler ? odi.service_handler : "unloaded");
	case GJS_OM_PROP_CODEC:
		gf_odm_get_object_info(odm, &odi);
		return JS_NewString(ctx, odi.codec_name ? odi.codec_name : "unloaded");
	case GJS_OM_PROP_NB_VIEWS:
		gf_odm_get_object_info(odm, &odi);
		return JS_NewInt32(ctx, (odi.nb_views>1) ? odi.nb_views : 0);
	case GJS_OM_PROP_NB_QUALITIES:
		//use HAS qualities
		if (odm->pid) {
			u32 nb_qualities = 0;
			GF_PropertyEntry *pe=NULL;
			const GF_PropertyValue *prop = gf_filter_pid_get_info_str(odm->pid, "has:qualities", &pe);
			if (prop) nb_qualities = prop->value.string_list.nb_items;
			gf_filter_release_property(pe);

			if (nb_qualities)
				return JS_NewInt32(ctx, nb_qualities);
		}
		//use input channels
		if (odm->extra_pids) {
			return JS_NewInt32(ctx, 1 + gf_list_count(odm->extra_pids));
		}
		//use number of scalable addons
		if (odm->upper_layer_odm) {
			count = 0;
			while (odm) {
				odm = odm->upper_layer_odm;
				count++;
			}
			return JS_NewInt32(ctx, count);
		}
		return JS_NewInt32(ctx, 1);

	case GJS_OM_PROP_MAX_BUFFER:
		gf_odm_get_object_info(odm, &odi);
		return JS_NewInt32(ctx, odi.max_buffer);
	case GJS_OM_PROP_MIN_BUFFER:
		gf_odm_get_object_info(odm, &odi);
		return JS_NewInt32(ctx, odi.min_buffer);
	case GJS_OM_PROP_FRAME_DUR:
		gf_odm_get_object_info(odm, &odi);
		return JS_NewInt32(ctx, odi.au_duration);
	case GJS_OM_PROP_NB_IRAP:
		gf_odm_get_object_info(odm, &odi);
		return JS_NewInt32(ctx, odi.nb_iraps);
	case GJS_OM_PROP_IRAP_DEC_TIME:
		gf_odm_get_object_info(odm, &odi);
		return JS_NewInt64(ctx, odi.irap_total_dec_time);
	case GJS_OM_PROP_IRAP_MAX_TIME:
		gf_odm_get_object_info(odm, &odi);
		return JS_NewInt32(ctx, odi.irap_max_dec_time);
	case GJS_OM_PROP_SERVICE_ID:
		return JS_NewInt32(ctx, odm->ServiceID);
	case GJS_OM_PROP_SELECTED_SERVICE:
		return JS_NewInt32(ctx,  (!odm->addon && odm->subscene) ? odm->subscene->selected_service_id : odm->parentscene->selected_service_id);
		break;
	case GJS_OM_PROP_BANDWIDTH_DOWN:
        if (odm->scene_ns->source_filter) {
			JSValue ret;
            GF_PropertyEntry *pe=NULL;
            const GF_PropertyValue *prop = gf_filter_get_info(odm->scene_ns->source_filter, GF_PROP_PID_DOWN_RATE, &pe);
            ret = JS_NewInt32(ctx, prop ? prop->value.uint/1000 : 0);
            gf_filter_release_property(pe);
            return ret;
        }
        return JS_NewInt32(ctx, 0);

	case GJS_OM_PROP_NB_HTTP:
		if (odm->scene_ns->source_filter) {
			return JS_NewInt32(ctx, gf_filter_count_source_by_protocol(odm->scene_ns->source_filter, "http", GF_TRUE, gjs_enum_pids, odm->subscene ? odm->subscene : odm->parentscene ) );
		}
		return JS_NewInt32(ctx, 0);
	case GJS_OM_PROP_TIMESHIFT_DEPTH:
		if ((s32) odm->timeshift_depth > 0) {
			return JS_NewFloat64(ctx, ((Double) odm->timeshift_depth) / 1000.0 );
		}
		return JS_NewFloat64(ctx, 0.0);

	case GJS_OM_PROP_TIMESHIFT_TIME:
		dval = 0.0;
		if (!odm->timeshift_depth) {
			return JS_NewInt32(ctx, 0);
		}

		scene = odm->subscene ? odm->subscene : odm->parentscene;
		//we may need to check the main addon for timeshifting
		if (scene->main_addon_selected) {
			count = gf_list_count(scene->resources);
			for (i=0; i < count; i++) {
				GF_ObjectManager *an_odm = gf_list_get(scene->resources, i);
				if (an_odm && an_odm->addon && (an_odm->addon->addon_type==GF_ADDON_TYPE_MAIN)) {
					odm = an_odm;
				}
			}
		}

		if (odm->timeshift_depth) {
			GF_FilterPid *pid = odm->pid;
			if (!pid && odm->subscene) {
				odm = gf_list_get(odm->subscene->resources, 0);
				pid = odm->pid;
			}
			if (pid) {
				GF_PropertyEntry *pe=NULL;
				const GF_PropertyValue *p = gf_filter_pid_get_info(pid, GF_PROP_PID_TIMESHIFT_TIME, &pe);
				if (p) dval = p->value.number;
				gf_filter_release_property(pe);
			}
		} else if (scene->main_addon_selected) {
			GF_Clock *ck = scene->root_od->ck;
			if (ck) {
				u64 now = gf_clock_time_absolute(ck) ;
				u64 live = scene->obj_clock_at_main_activation + gf_sys_clock() - scene->sys_clock_at_main_activation;
				dval = ((Double) live) / 1000.0;
				dval -= ((Double) now) / 1000.0;

				if (dval<0) {
					GF_Event evt;
					memset(&evt, 0, sizeof(evt));
					evt.type = GF_EVENT_TIMESHIFT_UNDERRUN;
					gf_sc_send_event(scene->compositor, &evt);
					dval=0;
				} else if (dval && dval*1000 > scene->timeshift_depth) {
					GF_Event evt;
					memset(&evt, 0, sizeof(evt));
					evt.type = GF_EVENT_TIMESHIFT_OVERFLOW;
					gf_sc_send_event(scene->compositor, &evt);
					dval=scene->timeshift_depth;
					dval/=1000;
				}
			}
		}
		return JS_NewFloat64(ctx, dval);

	case GJS_OM_PROP_IS_ADDON:
		return JS_NewBool(ctx,  (odm->addon || (!odm->subscene && odm->parentscene->root_od->addon)) );

	case GJS_OM_PROP_MAIN_ADDON_ON:
		scene = odm->subscene ? odm->subscene : odm->parentscene;
		return JS_NewBool(ctx,  scene->main_addon_selected );
	case GJS_OM_PROP_IS_OVER:
		scene = odm->subscene ? odm->subscene : odm->parentscene;
		return JS_NewBool(ctx,  gf_sc_is_over(scene->compositor, scene->graph));

	case GJS_OM_PROP_DYNAMIC_SCENE:
		return JS_NewBool(ctx, odm->subscene && odm->subscene->is_dynamic_scene);

	case GJS_OM_PROP_SERVICE_NAME:
		if (odm->pid) {
			const GF_PropertyValue *p = gf_filter_pid_get_property(odm->pid, GF_PROP_PID_SERVICE_NAME);
			if (p && p->value.string)
				return JS_NewString(ctx, p->value.string);
		}
		return JS_NULL;

	case GJS_OM_PROP_NTP_DIFF:
		gf_odm_get_object_info(odm, &odi);
		return JS_NewInt32(ctx, odi.ntp_diff);

	case GJS_OM_PROP_NTP_SENDER_DIFF:
		if (odm->last_drawn_frame_ntp_sender && odm->last_drawn_frame_ntp_receive) {
			s32 diff = gf_net_ntp_diff_ms(odm->last_drawn_frame_ntp_receive, odm->last_drawn_frame_ntp_sender);
			return JS_NewInt32(ctx, diff);
		}
		return JS_NULL;

	case GJS_OM_PROP_MAIN_ADDON_URL:
		scene = odm->subscene ? odm->subscene : odm->parentscene;
		count = gf_list_count(scene->resources);
		for (i=0; i < count; i++) {
			GF_ObjectManager *an_odm = gf_list_get(scene->resources, i);
			if (an_odm && an_odm->addon && (an_odm->addon->addon_type==GF_ADDON_TYPE_MAIN)) {
				if (!strstr(an_odm->addon->url, "://")) {
					char szURL[GF_MAX_PATH];
					sprintf(szURL, "gpac://%s", an_odm->addon->url);
					return JS_NewString(ctx, szURL);
				} else {
					return JS_NewString(ctx, an_odm->addon->url);
				}
			}
		}
		return JS_NULL;

	case GJS_OM_PROP_REVERSE_PLAYBACK:
		if (odm->pid) {
			const GF_PropertyValue *p = gf_filter_pid_get_property(odm->pid, GF_PROP_PID_PLAYBACK_MODE);
			if (p && p->value.uint==GF_PLAYBACK_MODE_REWIND)
				return JS_NewBool(ctx, GF_TRUE);
		}
		return JS_NewBool(ctx, GF_FALSE );

	case GJS_OM_PROP_SCALABLE_ENHANCEMENT:
		if (odm->lower_layer_odm) return JS_TRUE;
		if (odm->subscene && !odm->pid && odm->addon && !gf_list_count(odm->subscene->resources))
			return JS_TRUE;
		return JS_FALSE;

	case GJS_OM_PROP_MAIN_ADDON_MEDIATIME:
		scene = odm->subscene ? odm->subscene : odm->parentscene;
		count = gf_list_count(scene->resources);

		if (! scene->main_addon_selected) count = 0;

		for (i=0; i < count; i++) {
			GF_ObjectManager *an_odm = gf_list_get(scene->resources, i);
			if (an_odm && an_odm->addon && (an_odm->addon->addon_type==GF_ADDON_TYPE_MAIN)) {
				if (an_odm->duration) {
					Double now = gf_clock_time_absolute(scene->root_od->ck) / 1000.0;
					now -= ((Double) an_odm->addon->media_pts) / 90000.0;
					now += ((Double) an_odm->addon->media_timestamp) / an_odm->addon->media_timescale;
					return JS_NewFloat64(ctx, now);
				}
			}
		}
		return JS_NewFloat64(ctx, -1);

	case GJS_OM_PROP_DEPENDENT_GROUPS:
		if (odm->pid) {
			GF_PropertyEntry *pe=NULL;
			const GF_PropertyValue *p = gf_filter_pid_get_info_str(odm->pid, "has:group_deps", &pe);
			if (p) {
				u32 v = p->value.uint;
				gf_filter_release_property(pe);
				return JS_NewInt32(ctx, v);
			}
		}
		return JS_NewInt32(ctx, 0);

	case GJS_OM_PROP_IS_VR_SCENE:
		return JS_NewBool(ctx, odm->subscene && odm->subscene->vr_type);
	case GJS_OM_PROP_DISABLED:
		return JS_NewBool(ctx, odm->redirect_url ? 1 : 0);
	case GJS_OM_PROP_BUFFERING:
		if (odm->parentscene)
			return JS_NewBool(ctx, odm->parentscene->nb_buffering ? 1 : 0);
		return JS_NewBool(ctx, odm->subscene->nb_buffering ? 1 : 0);
	}
	return JS_UNDEFINED;
}

static JSValue gjs_odm_get_quality(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_ObjectManager *odm = JS_GetOpaque(this_val, odm_class_id);
	const GF_PropertyValue *prop;
	GF_PropertyEntry *pe=NULL;
	char *qdesc;
	const char *id="", *mime="", *codec="";
	u32 sr=0, ch=0, w=0, h=0, bw=0, par_n=1, par_d=1, tile_adaptation_mode=0,dependent_group_index=0;
	Bool ilced=GF_FALSE, disabled=GF_FALSE, selected=GF_FALSE, automatic=GF_FALSE;
	Double fps=30.0;
	s32 idx;
	s32 dep_idx=0;

	if (!odm) return GF_JS_EXCEPTION(ctx);
	if (argc<1) return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt32(ctx, &idx, argv[0]))
		return GF_JS_EXCEPTION(ctx);

	if ((argc>=2) && JS_ToInt32(ctx, &dep_idx, argv[1]))
		return GF_JS_EXCEPTION(ctx);

	if (!odm->pid) return JS_NULL;

	if (dep_idx) {
		char szName[100];
		sprintf(szName, "has:deps_%d_qualities", dep_idx);
		prop = gf_filter_pid_get_info_str(odm->pid, szName, &pe);
	} else {
		prop = gf_filter_pid_get_info_str(odm->pid, "has:qualities", &pe);
	}
	if (!prop || (prop->type!=GF_PROP_STRING_LIST)) {
		gf_filter_release_property(pe);
		return JS_NULL;
	}
	qdesc = NULL;

	if (idx < (s32) prop->value.string_list.nb_items)
		qdesc = prop->value.string_list.vals[idx];

	if (!qdesc) {
		gf_filter_release_property(pe);
		return JS_NULL;
	}
	JSValue a = JS_NewObject(ctx);
	if (JS_IsException(a))
		return a;

	while (qdesc) {
		char *sep=strstr(qdesc, "::");
		if (sep) sep[0]=0;

		if (!strncmp(qdesc, "id=", 3)) {
			id = NULL;
			JS_SetPropertyStr(ctx, a, "ID", JS_NewString(ctx, qdesc+3) );
		} else if (!strncmp(qdesc, "mime=", 5)) {
			mime = NULL;
			JS_SetPropertyStr(ctx, a, "mime", JS_NewString(ctx, qdesc+5));
		} else if (!strncmp(qdesc, "codec=", 6)) {
			codec = NULL;
			JS_SetPropertyStr(ctx, a, "codec", JS_NewString(ctx, qdesc+6));
		}
		else if (!strncmp(qdesc, "bw=", 3)) bw = atoi(qdesc+3);
		else if (!strncmp(qdesc, "w=", 2)) w = atoi(qdesc+2);
		else if (!strncmp(qdesc, "h=", 2)) h = atoi(qdesc+2);
		else if (!strncmp(qdesc, "sr=", 3)) sr = atoi(qdesc+3);
		else if (!strncmp(qdesc, "ch=", 3)) ch = atoi(qdesc+3);
		else if (!strcmp(qdesc, "interlaced")) ilced = GF_TRUE;
		else if (!strcmp(qdesc, "disabled")) disabled = GF_TRUE;
		else if (!strcmp(qdesc, "selected")) selected = GF_TRUE;
		else if (!strncmp(qdesc, "fps=", 4)) {
			u32 fd=25, fn=1;
			if (sscanf(qdesc, "fps=%d/%d", &fn, &fd) != 2) {
				fd=1;
				sscanf(qdesc, "fps=%d", &fn);
			}
			fps = ((Double)fn) / fd;
		}
		else if (!strncmp(qdesc, "sar=", 4)) {
			sscanf(qdesc, "sar=%d/%d", &par_n, &par_d);
		}
		if (!sep) break;
		sep[0]=':';
		qdesc = sep+2;
	}

	if (!dep_idx) {
		prop = gf_filter_pid_get_info_str(odm->pid, "has:selected", &pe);
		if (prop && (prop->value.uint==idx))
			selected = GF_TRUE;
		prop = gf_filter_pid_get_info_str(odm->pid, "has:auto", &pe);
		if (prop && prop->value.boolean)
			automatic = GF_TRUE;
		prop = gf_filter_pid_get_info_str(odm->pid, "has:tilemode", &pe);
		if (prop) tile_adaptation_mode = prop->value.uint;
	} else if (dep_idx>0) {
		prop = gf_filter_pid_get_info_str(odm->pid, "has:deps_selected", &pe);
		if (prop && (prop->type==GF_PROP_SINT_LIST)) {
			if ((u32) dep_idx<=prop->value.sint_list.nb_items) {
				s32 sel = prop->value.sint_list.vals[dep_idx-1];
				if ((sel>=0) && (idx==sel)) {
					selected = GF_TRUE;
				}
			}
		}
	}

	gf_filter_release_property(pe);

	if (id)
		JS_SetPropertyStr(ctx, a, "ID", JS_NewString(ctx, id));
	if (mime)
		JS_SetPropertyStr(ctx, a, "mime", JS_NewString(ctx, mime));
	if (codec)
		JS_SetPropertyStr(ctx, a, "codec", JS_NewString(ctx, codec));

	JS_SetPropertyStr(ctx, a, "width", JS_NewInt32(ctx, w));
	JS_SetPropertyStr(ctx, a, "height", JS_NewInt32(ctx, h));
	JS_SetPropertyStr(ctx, a, "bandwidth", JS_NewInt32(ctx, bw));
	JS_SetPropertyStr(ctx, a, "interlaced", JS_NewBool(ctx, ilced));
	JS_SetPropertyStr(ctx, a, "fps", JS_NewFloat64(ctx, fps) );
	JS_SetPropertyStr(ctx, a, "samplerate", JS_NewInt32(ctx, sr));
	JS_SetPropertyStr(ctx, a, "channels", JS_NewInt32(ctx, ch));
	JS_SetPropertyStr(ctx, a, "par_num", JS_NewInt32(ctx, par_n));
	JS_SetPropertyStr(ctx, a, "par_den", JS_NewInt32(ctx, par_d));
	JS_SetPropertyStr(ctx, a, "disabled", JS_NewBool(ctx, disabled));
	JS_SetPropertyStr(ctx, a, "is_selected", JS_NewBool(ctx, selected));
	JS_SetPropertyStr(ctx, a, "automatic", JS_NewBool(ctx, automatic));
	if (!dep_idx) {
		JS_SetPropertyStr(ctx, a, "tile_mode", JS_NewInt32(ctx, tile_adaptation_mode));
		JS_SetPropertyStr(ctx, a, "dependent_groups", JS_NewInt32(ctx, dependent_group_index));
	}

	return a;
}

static JSValue gjs_odm_get_srd(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_ObjectManager *odm = JS_GetOpaque(this_val, odm_class_id);
	s32 x, y, w, h;

	if (!odm) return GF_JS_EXCEPTION(ctx);

	x = y = w = h = 0;
	if (argc) {
		u32 srdw, srdh;
		const GF_PropertyValue *p;
		GF_PropertyEntry *pe=NULL;
	 	s32 dep_idx;
	 	if (JS_ToInt32(ctx, &dep_idx, argv[0]) )
	 		return GF_JS_EXCEPTION(ctx);
		if (dep_idx<=0)
	 		return GF_JS_EXCEPTION(ctx);
		dep_idx--;
		p = gf_filter_pid_get_info_str(odm->pid, "has:groups_srd", &pe);
		if (p && (p->type==GF_PROP_STRING_LIST)) {
			char *srd;
			if ((u32) dep_idx>=p->value.string_list.nb_items) {
				gf_filter_release_property(pe);
				return GF_JS_EXCEPTION(ctx);
			}
			srd = p->value.string_list.vals[dep_idx];
			sscanf(srd, "%dx%dx%dx%d@%dx%d", &x, &y, &w, &h, &srdw, &srdh);
		}
		gf_filter_release_property(pe);
	} else if (odm && odm->mo && odm->mo->srd_w && odm->mo->srd_h) {
		x = odm->mo->srd_x;
		y = odm->mo->srd_y;
		w = odm->mo->srd_w;
		h = odm->mo->srd_h;
	}

	if (w && h) {
		JSValue a = JS_NewObject(ctx);
		if (JS_IsException(a)) return a;

		JS_SetPropertyStr(ctx, a, "x", JS_NewInt32(ctx, x));
		JS_SetPropertyStr(ctx, a, "y", JS_NewInt32(ctx, y));
		JS_SetPropertyStr(ctx, a, "w", JS_NewInt32(ctx, w));
		JS_SetPropertyStr(ctx, a, "h", JS_NewInt32(ctx, h));
		return a;
	}
	return JS_NULL;
}

GF_Filter *jsff_get_filter(JSContext *c, JSValue this_val);

static JSValue gjs_odm_in_parent_chain(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Bool res;
	GF_Filter *f;
	GF_Scene *scene;
	GF_ObjectManager *odm = JS_GetOpaque(this_val, odm_class_id);
	if (!odm || !argc) return GF_JS_EXCEPTION(ctx);

	f = jsff_get_filter(ctx, argv[0]);
	if (!f) return GF_JS_EXCEPTION(ctx);

	scene = odm->subscene ? odm->subscene : odm->parentscene;
	if (!scene) return GF_JS_EXCEPTION(ctx);
	if (gf_filter_in_parent_chain(f, scene->compositor->filter))
		return JS_FALSE;

	if (odm->pid) {
		res = gf_filter_pid_is_filter_in_parents(odm->pid, f);
	} else {
		res = gf_filter_in_parent_chain(f, odm->scene_ns->source_filter);
	}
	return res ? JS_TRUE : JS_FALSE;
}

static JSValue gjs_odm_select_quality(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s32 idx = -1;
	s32 tile_mode = -1;
	u32 dep_idx = 0;
	GF_FilterEvent evt;
	GF_ObjectManager *odm = JS_GetOpaque(this_val, odm_class_id);

	if (!odm || !odm->pid) return GF_JS_EXCEPTION(ctx);

	if ((argc>=1) && JS_IsString(argv[0])) {
		const char *ID = JS_ToCString(ctx, argv[0]);
		if (!strcmp(ID, "auto")) {
			idx = -1;
		} else {
			idx = atoi(ID);
		}
		JS_FreeCString(ctx, ID);

		if (argc>=2) {
			if (JS_ToInt32(ctx, &dep_idx, argv[1]))
				return GF_JS_EXCEPTION(ctx);
		}
	}
	else if ((argc==1) && JS_IsInteger(argv[0])) {
		if (JS_ToInt32(ctx, &tile_mode, argv[0]))
			return GF_JS_EXCEPTION(ctx);
		if (tile_mode<0)
			return JS_UNDEFINED;
	}

	GF_FEVT_INIT(evt, GF_FEVT_QUALITY_SWITCH, odm->pid);

	if (tile_mode>=0) {
		evt.quality_switch.set_tile_mode_plus_one = 1 + tile_mode;
	} else {
		evt.quality_switch.dependent_group_index = dep_idx;
		evt.quality_switch.q_idx = idx;
	}
	gf_filter_pid_send_event(odm->pid, &evt);
	return JS_UNDEFINED;
}

static JSValue gjs_odm_disable_main_addon(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_ObjectManager *odm = JS_GetOpaque(this_val, odm_class_id);
	if (!odm) return GF_JS_EXCEPTION(ctx);
	if (!odm->subscene || !odm->subscene->main_addon_selected) return JS_UNDEFINED;

	gf_scene_resume_live(odm->subscene);
	return JS_UNDEFINED;
}

static JSValue gjs_odm_select_service(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 sid;
	GF_ObjectManager *odm = JS_GetOpaque(this_val, odm_class_id);
	if (!odm) return GF_JS_EXCEPTION(ctx);
	if (argc<1) return GF_JS_EXCEPTION(ctx);

	if (JS_ToInt32(ctx, &sid, argv[0]))
		return GF_JS_EXCEPTION(ctx);

	gf_scene_set_service_id(odm->subscene ? odm->subscene : odm->parentscene, sid);
	return JS_UNDEFINED;
}

static JSValue gjs_odm_select(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_ObjectManager *odm = JS_GetOpaque(this_val, odm_class_id);
	if (!odm) return GF_JS_EXCEPTION(ctx);

#ifndef GPAC_DISABLE_PLAYER
	gf_scene_select_object(odm->parentscene, odm);
#endif
	return JS_UNDEFINED;
}

static JSValue gjs_odm_get_resource(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_ObjectManager *an_odm = NULL;
	u32 idx;
	GF_ObjectManager *odm = JS_GetOpaque(this_val, odm_class_id);

	if (!odm) return GF_JS_EXCEPTION(ctx);
	if (argc<1) return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt32(ctx, &idx, argv[0])) return GF_JS_EXCEPTION(ctx);

	if (odm->subscene) {
		an_odm = gf_list_get(odm->subscene->resources, idx);
	}
	if (an_odm && an_odm->scene_ns) {
		JSValue anobj = JS_NewObjectClass(ctx, odm_class_id);
		JS_SetOpaque(anobj, an_odm);
		return anobj;
	}
	return JS_NULL;
}

static JSValue gjs_odm_addon_layout(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 pos, size;
	GF_ObjectManager *odm = JS_GetOpaque(this_val, odm_class_id);
	if (!odm) return GF_JS_EXCEPTION(ctx);
	if (argc<2) return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt32(ctx, &pos, argv[0])) return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt32(ctx, &size, argv[1])) return GF_JS_EXCEPTION(ctx);
	if (odm->subscene)
		gf_scene_set_addon_layout_info(odm->subscene, pos, size);
	return JS_UNDEFINED;
}

static void do_enable_addon(GF_ObjectManager *odm, char *addon_url, Bool enable_if_defined, Bool disable_if_defined )
{
	if (addon_url) {
		GF_AssociatedContentLocation addon_info;
		memset(&addon_info, 0, sizeof(GF_AssociatedContentLocation));
		addon_info.external_URL = addon_url;
		addon_info.timeline_id = -100;
		addon_info.enable_if_defined = enable_if_defined;
		addon_info.disable_if_defined = disable_if_defined;
		gf_scene_register_associated_media(odm->subscene ? odm->subscene : odm->parentscene, &addon_info);
	}
}

static JSValue gjs_odm_enable_addon(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Bool do_disable = GF_FALSE;
	const char *addon_url = NULL;
	GF_ObjectManager *odm = JS_GetOpaque(this_val, odm_class_id);
	if (!odm || !argc) return GF_JS_EXCEPTION(ctx);

	if (! JS_IsString(argv[0]) ) {
#ifdef GPAC_ENABLE_COVERAGE
		if (gf_sys_is_cov_mode()) {
			do_enable_addon(odm, NULL, GF_TRUE, GF_FALSE);
		}
#endif
		return JS_UNDEFINED;
	}
	if (argc==2)
		do_disable = JS_ToBool(ctx, argv[1]);
	addon_url = JS_ToCString(ctx, argv[0]);
	if (addon_url) {
		do_enable_addon(odm, (char *) addon_url, GF_TRUE, do_disable);
	}
	JS_FreeCString(ctx, addon_url);
	return JS_UNDEFINED;
}

static JSValue gjs_odm_declare_addon(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *addon_url = NULL;
	GF_ObjectManager *odm = JS_GetOpaque(this_val, odm_class_id);
	if (!odm || !argc) return GF_JS_EXCEPTION(ctx);
	if (! JS_IsString(argv[0]) ) return GF_JS_EXCEPTION(ctx);

	addon_url = JS_ToCString(ctx, argv[0]);
	if (addon_url && strcmp(addon_url, "gtest")) {
		do_enable_addon(odm, (char *)addon_url, GF_FALSE, GF_FALSE);
	}
	JS_FreeCString(ctx, addon_url);
	return JS_UNDEFINED;
}

static JSValue scenejs_get_object_manager(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JSValue anobj;
	u32 i, count;
	GF_ObjectManager *odm = NULL;
	const char *service_url = NULL;
	GF_SCENEJSExt *sjs = JS_GetOpaque(this_val, scene_class_id);
	GF_Compositor *compositor = scenejs_get_compositor(ctx, this_val);
	GF_Scene *scene = compositor->root_scene;
	if (!sjs) return GF_JS_EXCEPTION(ctx);

	if (JS_IsString(argv[0]) ) {
		const char *url;
		char *an_url;
		u32 url_len;
		url = service_url = JS_ToCString(ctx, argv[0]);
		if (!service_url) {
			return JS_NULL;
		}
		if (!strncmp(service_url, "gpac://", 7)) url = service_url + 7;
		if (!strncmp(service_url, "file://", 7)) url = service_url + 7;
		url_len = (u32) strlen(url);
		an_url = strchr(url, '#');
		if (an_url) url_len -= (u32) strlen(an_url);

		count = gf_list_count(scene->resources);
		for (i=0; i<count; i++) {
			odm = gf_list_get(scene->resources, i);
			if (odm->scene_ns) {
				an_url = odm->scene_ns->url;
				if (!strncmp(an_url, "gpac://", 7)) an_url = an_url + 7;
				if (!strncmp(an_url, "file://", 7)) an_url = an_url + 7;
				if (!strncmp(an_url, url, url_len))
					break;
			}
			odm = NULL;
		}
	}

	JS_FreeCString(ctx, service_url);

	if (!odm) return JS_NULL;

	anobj = JS_NewObjectClass(ctx, odm_class_id);
	if (JS_IsException(anobj)) return anobj;

	JS_SetOpaque(anobj, odm);
	return anobj;
}


static JSValue gpacevt_getProperty(JSContext *ctx, JSValueConst this_val, int magic)
{
	GF_SCENEJSExt *sjs = JS_GetOpaque(this_val, gpacevt_class_id);
	if (!sjs || !sjs->evt) return GF_JS_EXCEPTION(ctx);
	GF_Event *evt = sjs->evt;

	switch (magic) {
	case GJS_EVT_PROP_KEYCODE:
#ifndef GPAC_DISABLE_SVG
		return JS_NewString(ctx, gf_dom_get_key_name(evt->key.key_code) );
#else
		return JS_NULL;
#endif

	case GJS_EVT_PROP_MOUSE_X:
		return JS_NewInt32(ctx, evt->mouse.x);
	case GJS_EVT_PROP_MOUSE_Y:
		return JS_NewInt32(ctx, evt->mouse.y);
	case GJS_EVT_PROP_PICKED:
		if (sjs->compositor->hit_appear) return JS_NewBool(ctx, 1);
		else if (gf_list_count(sjs->compositor->previous_sensors) ) return JS_NewBool(ctx, 1);
		else if (sjs->compositor->text_selection) return JS_NewBool(ctx, 1);
		else return JS_NewBool(ctx, 0);

	case GJS_EVT_PROP_WHEEL:
		return JS_NewFloat64(ctx, FIX2FLT(evt->mouse.wheel_pos));
	case GJS_EVT_PROP_BUTTON:
		return JS_NewInt32(ctx,  evt->mouse.button);
	case GJS_EVT_PROP_TYPE:
		return JS_NewInt32(ctx, evt->type);
	case GJS_EVT_PROP_NAME:
#ifndef GPAC_DISABLE_SVG
		return JS_NewString(ctx, gf_dom_event_get_name(evt->type) );
#else
		return JS_NULL;
#endif

	case GJS_EVT_PROP_HWKEY:
		return JS_NewInt32(ctx, evt->key.hw_code);
	case GJS_EVT_PROP_TARGET_URL:
		return JS_NewString(ctx, evt->navigate.to_url);
	case GJS_EVT_PROP_FILES:
	{
		u32 i, idx;
		JSValue files_array = JS_NewArray(ctx);
		idx=0;
		for (i=0; i<evt->open_file.nb_files; i++) {
			if (evt->open_file.files[i]) {
				JS_SetPropertyUint32(ctx, files_array, idx, JS_NewString(ctx, evt->open_file.files[i]) );
				idx++;
			}
		}
		return files_array;
	}
	}
	return JS_UNDEFINED;
}

static Bool gjs_event_filter_process(GF_SCENEJSExt *sjs, GF_Event *evt)
{
	s32 res;
	JSValue rval;

	sjs->evt = evt;
	JS_SetOpaque(sjs->evt_obj, sjs);
	rval = JS_Call(sjs->c, sjs->evt_fun, sjs->evt_filter_obj, 1, &sjs->evt_obj);
	JS_SetOpaque(sjs->evt_obj, NULL);
	sjs->evt = NULL;

	res = 0;
	if (JS_IsBool(rval))
		res = JS_ToBool(sjs->c, rval);
	else if (JS_IsNumber(rval))
		JS_ToInt32(sjs->c, &res, rval);

	JS_FreeValue(sjs->c, rval);
	return res ? GF_TRUE : GF_FALSE;
}

static Bool gjs_event_filter(void *udta, GF_Event *evt, Bool consumed_by_compositor)
{
	u32 lock_fail;
	Bool res;
	GF_SCENEJSExt *sjs = (GF_SCENEJSExt *)udta;
	if (consumed_by_compositor) return 0;

	if (sjs->evt != NULL) return 0;

	lock_fail=0;
	res = gf_mx_try_lock(sjs->compositor->mx);
	if (!res) {
		lock_fail=1;
	} else {
		res = gf_js_try_lock(sjs->c);
		if (!res) lock_fail=2;
	}
	if (lock_fail) {
		GF_Event *evt_clone;
		gf_mx_p(sjs->event_mx);
		evt_clone = gf_malloc(sizeof(GF_Event));
		memcpy(evt_clone, evt, sizeof(GF_Event));
		gf_list_add(sjs->event_queue, evt_clone);
		GF_LOG(GF_LOG_INFO, GF_LOG_COMPOSE, ("[SCENEJS] Couldn't lock % mutex, queing event\n", (lock_fail==2) ? "JavaScript" : "Compositor"));
		gf_mx_v(sjs->event_mx);

		if (lock_fail==2){
			gf_mx_v(sjs->compositor->mx);
		}
		return 0;
	}

	gf_mx_p(sjs->event_mx);
	while (gf_list_count(sjs->event_queue)) {
		GF_Event *an_evt = (GF_Event *) gf_list_pop_front(sjs->event_queue);
		gjs_event_filter_process(sjs, an_evt);
		gf_free(an_evt);
	}
	gf_mx_v(sjs->event_mx);

	res = gjs_event_filter_process(sjs, evt);

	gf_mx_v(sjs->compositor->mx);
	gf_js_lock(sjs->c, 0);
	return res;
}

static JSValue scenejs_set_event_filter(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_SCENEJSExt *sjs = JS_GetOpaque(this_val, scene_class_id);
	if (!sjs || !argc)
		return GF_JS_EXCEPTION(ctx);

	if (!JS_IsNull(argv[0]) && !JS_IsUndefined(argv[0]) && !JS_IsFunction(ctx, argv[0]))
		return GF_JS_EXCEPTION(ctx);

	JS_FreeValue(sjs->c, sjs->evt_fun);
	sjs->evt_fun = JS_DupValue(ctx, argv[0]);
	sjs->evt_filter_obj = this_val;
	sjs->c = ctx;
	sjs->evt_filter.udta = sjs;
	sjs->evt_filter.on_event = gjs_event_filter;

	gf_filter_add_event_listener(sjs->compositor->filter, &sjs->evt_filter);
	return JS_UNDEFINED;
}

GF_Node *gf_sg_js_get_node(struct JSContext *c, JSValue obj);

static JSValue scenejs_set_focus(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Node *elt;
	GF_Compositor *compositor = scenejs_get_compositor(ctx, this_val);
	if (!compositor || !argc) return GF_JS_EXCEPTION(ctx);

	if (JS_IsNull(argv[0])) {
		gf_sc_focus_switch_ring(compositor, 0, NULL, 0);
		return JS_UNDEFINED;
	}

	if (JS_IsString(argv[0])) {
		const char *focus_type = JS_ToCString(ctx, argv[0]);
		if (!stricmp(focus_type, "previous")) {
			gf_sc_focus_switch_ring(compositor, 1, NULL, 0);
		}
		else if (!stricmp(focus_type, "next")) {
			gf_sc_focus_switch_ring(compositor, 0, NULL, 0);
		}
		JS_FreeCString(ctx, focus_type);
	} else if (JS_IsObject(argv[0])) {
		elt = gf_sg_js_get_node(ctx, argv[0]);
		if (!elt) return GF_JS_EXCEPTION(ctx);
		gf_sc_focus_switch_ring(compositor, 0, elt, 2);
	}
	return JS_UNDEFINED;
}

#if 0 //unused
static JSValue scenejs_get_scene(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Node *elt;
	u32 w, h;
	JSValue scene_obj;
#ifndef GPAC_DISABLE_SCENEGRAPH
	GF_SceneGraph *sg;
#endif
	GF_Scene *scene=NULL;
	GF_SCENEJSExt *sjs = (GF_SCENEJSExt *)JS_GetOpaque(this_val, scene_class_id);
	if (!sjs || !argc || !JS_IsObject(argv[0])) return GF_JS_EXCEPTION(ctx);

	elt = gf_sg_js_get_node(ctx, argv[0]);
	if (!elt) return GF_JS_EXCEPTION(ctx);
	switch (elt->sgprivate->tag) {
#ifndef GPAC_DISABLE_VRML
	case TAG_MPEG4_Inline:
		scene = (GF_Scene *)gf_node_get_private(elt);
		break;
#endif

#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Inline:
		scene = (GF_Scene *)gf_node_get_private(elt);
		break;
#endif

#ifndef GPAC_DISABLE_SVG
	case TAG_SVG_animation:
		sg = gf_sc_animation_get_scenegraph(elt);
		scene = (GF_Scene *)gf_sg_get_private(sg);
		break;
#endif
	default:
		return GF_JS_EXCEPTION(ctx);
	}
	if (!scene) return GF_JS_EXCEPTION(ctx);


	scene_obj = JS_NewObjectClass(ctx, any_class_id);
	if (JS_IsException(scene_obj)) return scene_obj;
	JS_SetOpaque(scene_obj, scene);
	gf_sg_get_scene_size_info(scene->graph, &w, &h);
	JS_SetPropertyStr(ctx, scene_obj, "width", JS_NewInt32(ctx, w));
	JS_SetPropertyStr(ctx, scene_obj, "height", JS_NewInt32(ctx, h));
	JS_SetPropertyStr(ctx, scene_obj, "connected", JS_NewBool(ctx, scene->graph ? 1 : 0));
	return scene_obj;
}
#endif

static JSValue scenejs_show_keyboard(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Compositor *compositor = scenejs_get_compositor(ctx, this_val);
	if (!compositor || !argc) return GF_JS_EXCEPTION(ctx);

	GF_Event evt;
	memset(&evt, 0, sizeof(GF_Event));
	evt.type = JS_ToBool(ctx, argv[0]) ? GF_EVENT_TEXT_EDITING_START : GF_EVENT_TEXT_EDITING_END;
	gf_sc_user_event(compositor, &evt);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry scenejs_evt_funcs[] = {
	JS_CGETSET_MAGIC_DEF("keycode", gpacevt_getProperty, NULL, GJS_EVT_PROP_KEYCODE),
	JS_CGETSET_MAGIC_DEF("mouse_x", gpacevt_getProperty, NULL, GJS_EVT_PROP_MOUSE_X),
	JS_CGETSET_MAGIC_DEF("mouse_y", gpacevt_getProperty, NULL, GJS_EVT_PROP_MOUSE_Y),
	JS_CGETSET_MAGIC_DEF("picked", gpacevt_getProperty, NULL, GJS_EVT_PROP_PICKED),
	JS_CGETSET_MAGIC_DEF("wheel", gpacevt_getProperty, NULL, GJS_EVT_PROP_WHEEL),
	JS_CGETSET_MAGIC_DEF("button", gpacevt_getProperty, NULL, GJS_EVT_PROP_BUTTON),
	JS_CGETSET_MAGIC_DEF("type", gpacevt_getProperty, NULL, GJS_EVT_PROP_TYPE),
	JS_CGETSET_MAGIC_DEF("name", gpacevt_getProperty, NULL, GJS_EVT_PROP_NAME),
	JS_CGETSET_MAGIC_DEF("hwkey", gpacevt_getProperty, NULL, GJS_EVT_PROP_HWKEY),
	JS_CGETSET_MAGIC_DEF("url", gpacevt_getProperty, NULL, GJS_EVT_PROP_TARGET_URL),
	JS_CGETSET_MAGIC_DEF("dropfiles", gpacevt_getProperty, NULL, GJS_EVT_PROP_FILES),
};

static const JSCFunctionListEntry scenejs_funcs[] = {
	JS_CGETSET_MAGIC_DEF("fullscreen", scenejs_getProperty, scenejs_setProperty, GJS_SCENE_PROP_FULLSCREEN),
	JS_CGETSET_MAGIC_DEF("current_path", scenejs_getProperty, NULL, GJS_SCENE_PROP_CURRENT_PATH),
	JS_CGETSET_MAGIC_DEF("volume", scenejs_getProperty, scenejs_setProperty, GJS_SCENE_PROP_VOLUME),
	JS_CGETSET_MAGIC_DEF("navigation", scenejs_getProperty, scenejs_setProperty, GJS_SCENE_PROP_NAVIGATION),
	JS_CGETSET_MAGIC_DEF("navigation_type", scenejs_getProperty, scenejs_setProperty, GJS_SCENE_PROP_NAVIGATION_TYPE),
	JS_CGETSET_MAGIC_DEF("hardware_yuv", scenejs_getProperty, NULL, GJS_SCENE_PROP_HARDWARE_YUV),
	JS_CGETSET_MAGIC_DEF("hardware_rgb", scenejs_getProperty, NULL, GJS_SCENE_PROP_HARDWARE_RGB),
	JS_CGETSET_MAGIC_DEF("hardware_rgba", scenejs_getProperty, NULL, GJS_SCENE_PROP_HARDWARE_RGBA),
	JS_CGETSET_MAGIC_DEF("hardware_stretch", scenejs_getProperty, NULL, GJS_SCENE_PROP_HARDWARE_STRETCH),
	JS_CGETSET_MAGIC_DEF("screen_width", scenejs_getProperty, NULL, GJS_SCENE_PROP_SCREEN_WIDTH),
	JS_CGETSET_MAGIC_DEF("screen_height", scenejs_getProperty, NULL, GJS_SCENE_PROP_SCREEN_HEIGHT),
	JS_CGETSET_MAGIC_DEF("fps", scenejs_getProperty, NULL, GJS_SCENE_PROP_FPS),
	JS_CGETSET_MAGIC_DEF("sim_fps", scenejs_getProperty, NULL, GJS_SCENE_PROP_SIM_FPS),
	JS_CGETSET_MAGIC_DEF("has_opengl", scenejs_getProperty, NULL, GJS_SCENE_PROP_HAS_OPENGL),
	JS_CGETSET_MAGIC_DEF("caption", scenejs_getProperty, scenejs_setProperty, GJS_SCENE_PROP_CAPTION),
	JS_CGETSET_MAGIC_DEF("focus_highlight", scenejs_getProperty, scenejs_setProperty, GJS_SCENE_PROP_FOCUS_HIGHLIGHT),
	JS_CGETSET_MAGIC_DEF("dpi_x", scenejs_getProperty, NULL, GJS_SCENE_PROP_DPI_X),
	JS_CGETSET_MAGIC_DEF("dpi_y", scenejs_getProperty, NULL, GJS_SCENE_PROP_DPI_Y),
	JS_CGETSET_MAGIC_DEF("sensors_active", scenejs_getProperty, scenejs_setProperty, GJS_SCENE_PROP_SENSORS_ACTIVE),
	JS_CGETSET_MAGIC_DEF("zoom", scenejs_getProperty, NULL, GJS_SCENE_PROP_ZOOM),
	JS_CGETSET_MAGIC_DEF("text_selection", scenejs_getProperty, NULL, GJS_SCENE_PROP_TEXT_SEL),
	JS_CGETSET_MAGIC_DEF("orientation", scenejs_getProperty, scenejs_setProperty, GJS_SCENE_PROP_DISP_ORIENTATION),
	JS_CFUNC_DEF("get_option", 0, scenejs_get_option),
	JS_CFUNC_DEF("set_option", 0, scenejs_set_option),
	JS_CFUNC_DEF("set_size", 0, scenejs_set_size),
	JS_CFUNC_DEF("exit", 0, scenejs_exit),
	JS_CFUNC_DEF("set_3d", 0, scenejs_set_3d),
	JS_CFUNC_DEF("move_window", 0, scenejs_move_window),
	JS_CFUNC_DEF("set_event_filter", 0, scenejs_set_event_filter),
	JS_CFUNC_DEF("set_focus", 0, scenejs_set_focus),
	JS_CFUNC_DEF("show_keyboard", 0, scenejs_show_keyboard),
	JS_CFUNC_DEF("trigger_gc", 0, scenejs_trigger_gc),
	JS_CFUNC_DEF("get_object_manager", 0, scenejs_get_object_manager),
	JS_CFUNC_DEF("switch_quality", 0, scenejs_switch_quality),
	JS_CFUNC_DEF("navigation_supported", 0, scenejs_navigation_supported),
	JS_CFUNC_DEF("set_back_color", 0, scenejs_set_back_color)
};

static const JSCFunctionListEntry odm_funcs[] = {
	JS_CGETSET_MAGIC_DEF("ID", odm_getProperty, NULL, GJS_OM_PROP_ID),
	JS_CGETSET_MAGIC_DEF("nb_resources", odm_getProperty, NULL, GJS_OM_PROP_NB_RES),
	JS_CGETSET_MAGIC_DEF("service_url", odm_getProperty, NULL, GJS_OM_PROP_URL),
	JS_CGETSET_MAGIC_DEF("duration", odm_getProperty, NULL, GJS_OM_PROP_DUR),
	JS_CGETSET_MAGIC_DEF("clock_time", odm_getProperty, NULL, GJS_OM_PROP_CLOCK),
	JS_CGETSET_MAGIC_DEF("clock_drift", odm_getProperty, NULL, GJS_OM_PROP_DRIFT),
	JS_CGETSET_MAGIC_DEF("status", odm_getProperty, NULL, GJS_OM_PROP_STATUS),
	JS_CGETSET_MAGIC_DEF("buffer", odm_getProperty, NULL, GJS_OM_PROP_BUFFER),
	JS_CGETSET_MAGIC_DEF("db_unit_count", odm_getProperty, NULL, GJS_OM_PROP_DB_COUNT),
	JS_CGETSET_MAGIC_DEF("cb_unit_count", odm_getProperty, NULL, GJS_OM_PROP_CB_COUNT),
	JS_CGETSET_MAGIC_DEF("cb_capacity", odm_getProperty, NULL, GJS_OM_PROP_CB_CAP),
	JS_CGETSET_MAGIC_DEF("type", odm_getProperty, NULL, GJS_OM_PROP_TYPE),
	JS_CGETSET_MAGIC_DEF("samplerate", odm_getProperty, NULL, GJS_OM_PROP_SAMPLERATE),
	JS_CGETSET_MAGIC_DEF("channels", odm_getProperty, NULL, GJS_OM_PROP_CHANNELS),
	JS_CGETSET_MAGIC_DEF("lang", odm_getProperty, NULL, GJS_OM_PROP_LANG),
	JS_CGETSET_MAGIC_DEF("width", odm_getProperty, NULL, GJS_OM_PROP_WIDTH),
	JS_CGETSET_MAGIC_DEF("height", odm_getProperty, NULL, GJS_OM_PROP_HEIGHT),
	JS_CGETSET_MAGIC_DEF("pixelformt", odm_getProperty, NULL, GJS_OM_PROP_PIXELFORMAT),
	JS_CGETSET_MAGIC_DEF("par", odm_getProperty, NULL, GJS_OM_PROP_PAR),
	JS_CGETSET_MAGIC_DEF("dec_frames", odm_getProperty, NULL, GJS_OM_PROP_DEC_FRAMES),
	JS_CGETSET_MAGIC_DEF("drop_frames", odm_getProperty, NULL, GJS_OM_PROP_DROP_FRAMES),
	JS_CGETSET_MAGIC_DEF("max_dec_time", odm_getProperty, NULL, GJS_OM_PROP_DEC_TIME_MAX),
	JS_CGETSET_MAGIC_DEF("total_dec_time", odm_getProperty, NULL, GJS_OM_PROP_DEC_TIME_TOTAL),
	JS_CGETSET_MAGIC_DEF("avg_bitrate", odm_getProperty, NULL, GJS_OM_PROP_AVG_RATE),
	JS_CGETSET_MAGIC_DEF("max_bitrate", odm_getProperty, NULL, GJS_OM_PROP_MAX_RATE),
	JS_CGETSET_MAGIC_DEF("service_handler", odm_getProperty, NULL, GJS_OM_PROP_SERVICE_HANDLER),
	JS_CGETSET_MAGIC_DEF("codec", odm_getProperty, NULL, GJS_OM_PROP_CODEC),
	JS_CGETSET_MAGIC_DEF("nb_views", odm_getProperty, NULL, GJS_OM_PROP_NB_VIEWS),
	JS_CGETSET_MAGIC_DEF("nb_qualities", odm_getProperty, NULL, GJS_OM_PROP_NB_QUALITIES),
	JS_CGETSET_MAGIC_DEF("max_buffer", odm_getProperty, NULL, GJS_OM_PROP_MAX_BUFFER),
	JS_CGETSET_MAGIC_DEF("min_buffer", odm_getProperty, NULL, GJS_OM_PROP_MIN_BUFFER),
	JS_CGETSET_MAGIC_DEF("frame_duration", odm_getProperty, NULL, GJS_OM_PROP_FRAME_DUR),
	JS_CGETSET_MAGIC_DEF("irap_frames", odm_getProperty, NULL, GJS_OM_PROP_NB_IRAP),
	JS_CGETSET_MAGIC_DEF("irap_dec_time", odm_getProperty, NULL, GJS_OM_PROP_IRAP_DEC_TIME),
	JS_CGETSET_MAGIC_DEF("irap_max_time", odm_getProperty, NULL, GJS_OM_PROP_IRAP_MAX_TIME),
	JS_CGETSET_MAGIC_DEF("service_id", odm_getProperty, NULL, GJS_OM_PROP_SERVICE_ID),
	JS_CGETSET_MAGIC_DEF("selected_service", odm_getProperty, NULL, GJS_OM_PROP_SELECTED_SERVICE),
	JS_CGETSET_MAGIC_DEF("bandwidth_down", odm_getProperty, NULL, GJS_OM_PROP_BANDWIDTH_DOWN),
	JS_CGETSET_MAGIC_DEF("nb_http", odm_getProperty, NULL, GJS_OM_PROP_NB_HTTP),
	JS_CGETSET_MAGIC_DEF("timeshift_depth", odm_getProperty, NULL, GJS_OM_PROP_TIMESHIFT_DEPTH),
	JS_CGETSET_MAGIC_DEF("timeshift_time", odm_getProperty, NULL, GJS_OM_PROP_TIMESHIFT_TIME),
	JS_CGETSET_MAGIC_DEF("is_addon", odm_getProperty, NULL, GJS_OM_PROP_IS_ADDON),
	JS_CGETSET_MAGIC_DEF("main_addon_on", odm_getProperty, NULL, GJS_OM_PROP_MAIN_ADDON_ON),
	JS_CGETSET_MAGIC_DEF("is_over", odm_getProperty, NULL, GJS_OM_PROP_IS_OVER),
	JS_CGETSET_MAGIC_DEF("dynamic_scene", odm_getProperty, NULL, GJS_OM_PROP_DYNAMIC_SCENE),
	JS_CGETSET_MAGIC_DEF("service_name", odm_getProperty, NULL, GJS_OM_PROP_SERVICE_NAME),
	JS_CGETSET_MAGIC_DEF("ntp_diff", odm_getProperty, NULL, GJS_OM_PROP_NTP_DIFF),
	JS_CGETSET_MAGIC_DEF("ntp_sender_diff", odm_getProperty, NULL, GJS_OM_PROP_NTP_SENDER_DIFF),
	JS_CGETSET_MAGIC_DEF("main_addon_url", odm_getProperty, NULL, GJS_OM_PROP_MAIN_ADDON_URL),
	JS_CGETSET_MAGIC_DEF("reverse_playback_supported", odm_getProperty, NULL, GJS_OM_PROP_REVERSE_PLAYBACK),
	JS_CGETSET_MAGIC_DEF("scalable_enhancement", odm_getProperty, NULL, GJS_OM_PROP_SCALABLE_ENHANCEMENT),
	JS_CGETSET_MAGIC_DEF("main_addon_media_time", odm_getProperty, NULL, GJS_OM_PROP_MAIN_ADDON_MEDIATIME),
	JS_CGETSET_MAGIC_DEF("dependent_groups", odm_getProperty, NULL, GJS_OM_PROP_DEPENDENT_GROUPS),
	JS_CGETSET_MAGIC_DEF("vr_scene", odm_getProperty, NULL, GJS_OM_PROP_IS_VR_SCENE),
	JS_CGETSET_MAGIC_DEF("disabled", odm_getProperty, NULL, GJS_OM_PROP_DISABLED),
	JS_CGETSET_MAGIC_DEF("buffering", odm_getProperty, NULL, GJS_OM_PROP_BUFFERING),

	JS_CFUNC_DEF("declare_addon", 0, gjs_odm_declare_addon),
	JS_CFUNC_DEF("enable_addon", 0, gjs_odm_enable_addon),
	JS_CFUNC_DEF("addon_layout", 0, gjs_odm_addon_layout),
	JS_CFUNC_DEF("get_resource", 0, gjs_odm_get_resource),
	JS_CFUNC_DEF("get_quality", 0, gjs_odm_get_quality),
	JS_CFUNC_DEF("select_service", 0, gjs_odm_select_service),
	JS_CFUNC_DEF("select_quality", 0, gjs_odm_select_quality),
	JS_CFUNC_DEF("disable_main_addon", 0, gjs_odm_disable_main_addon),
	JS_CFUNC_DEF("select", 0, gjs_odm_select),
	JS_CFUNC_DEF("get_srd", 0, gjs_odm_get_srd),
	JS_CFUNC_DEF("in_parent_chain", 0, gjs_odm_in_parent_chain),
};

#include "../filter_core/filter_session.h"

static void scenejs_finalize(JSRuntime *rt, JSValue obj)
{
	GF_SCENEJSExt *sjs = JS_GetOpaque(obj, scene_class_id);
	if (!sjs) return;

	JS_SetOpaque(obj, NULL);

	while (gf_list_count(sjs->storages)) {
		GF_Config *cfg = (GF_Config *) gf_list_pop_back(sjs->storages);
		gf_cfg_discard_changes(cfg);
		gf_cfg_del(cfg);
	}
	gf_list_del(sjs->storages);

	while (gf_list_count(sjs->event_queue)) {
		GF_Event *evt = (GF_Event *) gf_list_pop_back(sjs->event_queue);
		gf_free(evt);
	}
	gf_list_del(sjs->event_queue);
	gf_mx_del(sjs->event_mx);

	if (sjs->compositor && sjs->compositor->filter) {
		gf_fs_unload_script(sjs->compositor->filter->session, NULL);
	}
	/*if we destroy the script context holding the gpac event filter (only one for the time being), remove the filter*/
	JS_FreeValueRT(rt, sjs->evt_fun);
	if (sjs->evt_filter.udta) {
		if (sjs->compositor)
			gf_filter_remove_event_listener(sjs->compositor->filter, &sjs->evt_filter);
		sjs->evt_filter.udta = NULL;
	}

	gf_free(sjs);
}

static int js_scene_init(JSContext *c, JSModuleDef *m)
{
	GF_JSAPIParam par;
	GF_SCENEJSExt *sjs;
	GF_SAFEALLOC(sjs, GF_SCENEJSExt);
	GF_SceneGraph *scene;
	if (!sjs) {
		return -1;
	}
	sjs->storages = gf_list_new();
	sjs->event_queue = gf_list_new();
	sjs->event_mx = gf_mx_new("GPACJSEvt");
	sjs->evt_fun = JS_UNDEFINED;
	sjs->scene_obj = JS_UNDEFINED;
	sjs->evt_obj = JS_UNDEFINED;

	scene = JS_GetContextOpaque(c);
	if (!scene) return -1;
	if (scene->__reserved_null) {
		GF_Node *n = JS_GetContextOpaque(c);
		scene = n->sgprivate->scenegraph;
	}

	if (!scene_class_id) {
		JS_NewClassID(&scene_class_id);
		JS_NewClass(JS_GetRuntime(c), scene_class_id, &sceneClass);

		JS_NewClassID(&odm_class_id);
		JS_NewClass(JS_GetRuntime(c), odm_class_id, &odmClass);
	}
	JSValue proto = JS_NewObjectClass(c, odm_class_id);
	JS_SetPropertyFunctionList(c, proto, odm_funcs, countof(odm_funcs));
	JS_SetClassProto(c, odm_class_id, proto);

	JS_NewClassID(&gpacevt_class_id);
	JS_NewClass(JS_GetRuntime(c), gpacevt_class_id, &gpacEvtClass);

	JS_NewClassID(&any_class_id);
	JS_NewClass(JS_GetRuntime(c), any_class_id, &anyClass);

	JSValue global = JS_GetGlobalObject(c);

	sjs->scene_obj = JS_NewObjectClass(c, scene_class_id);
	JS_SetPropertyFunctionList(c, sjs->scene_obj, scenejs_funcs, countof(scenejs_funcs));
	JS_SetOpaque(sjs->scene_obj, sjs);
//	JS_SetPropertyStr(c, global, "gpac", sjs->scene_obj);

	if (scene->script_action) {
		if (scene->script_action(scene->script_action_cbck, GF_JSAPI_OP_GET_COMPOSITOR, scene->RootNode, &par)) {
			sjs->compositor = par.compositor;
		}
	}
	if (sjs->compositor && sjs->compositor->filter) {
		GF_Err gf_fs_load_js_api(JSContext *c, GF_FilterSession *fs);
		GF_FilterSession *fs = sjs->compositor->filter->session;

		//don't check error code, this may fail if global JS has been set but the script may still run
		if (gf_fs_load_js_api(c, fs) == GF_OK) {
			scene->attached_session = fs;
		}
	}

	sjs->evt_obj = JS_NewObjectClass(c, gpacevt_class_id);
	JS_SetPropertyFunctionList(c, sjs->evt_obj, scenejs_evt_funcs, countof(scenejs_evt_funcs));
	JS_SetOpaque(sjs->evt_obj, NULL);
	JS_SetPropertyStr(c, global, "gpacevt", sjs->evt_obj);

#define DECLARE_CONST(_val) \
	JS_SetPropertyStr(c, global, #_val, JS_NewInt32(c, _val));

	DECLARE_CONST(GF_EVENT_CLICK);
	DECLARE_CONST(GF_EVENT_MOUSEUP);
	DECLARE_CONST(GF_EVENT_MOUSEDOWN);
	DECLARE_CONST(GF_EVENT_MOUSEMOVE);
	DECLARE_CONST(GF_EVENT_MOUSEWHEEL);
	DECLARE_CONST(GF_EVENT_DBLCLICK);
	DECLARE_CONST(GF_EVENT_KEYUP);
	DECLARE_CONST(GF_EVENT_KEYDOWN);
	DECLARE_CONST(GF_EVENT_TEXTINPUT);
	DECLARE_CONST(GF_EVENT_CONNECT);
	DECLARE_CONST(GF_EVENT_NAVIGATE_INFO);
	DECLARE_CONST(GF_EVENT_NAVIGATE);
	DECLARE_CONST(GF_EVENT_DROPFILE);
	DECLARE_CONST(GF_EVENT_ADDON_DETECTED);
	DECLARE_CONST(GF_EVENT_QUALITY_SWITCHED);
	DECLARE_CONST(GF_EVENT_TIMESHIFT_DEPTH);
	DECLARE_CONST(GF_EVENT_TIMESHIFT_UPDATE);
	DECLARE_CONST(GF_EVENT_TIMESHIFT_OVERFLOW);
	DECLARE_CONST(GF_EVENT_TIMESHIFT_UNDERRUN);
	DECLARE_CONST(GF_EVENT_QUIT);
	DECLARE_CONST(GF_EVENT_MAIN_ADDON_STATE);
	DECLARE_CONST(GF_EVENT_SCENE_SIZE);

	DECLARE_CONST(GF_NAVIGATE_NONE);
	DECLARE_CONST(GF_NAVIGATE_WALK);
	DECLARE_CONST(GF_NAVIGATE_FLY);
	DECLARE_CONST(GF_NAVIGATE_PAN);
	DECLARE_CONST(GF_NAVIGATE_GAME);
	DECLARE_CONST(GF_NAVIGATE_SLIDE);
	DECLARE_CONST(GF_NAVIGATE_EXAMINE);
	DECLARE_CONST(GF_NAVIGATE_ORBIT);
	DECLARE_CONST(GF_NAVIGATE_VR);

	DECLARE_CONST(GF_NAVIGATE_TYPE_NONE);
	DECLARE_CONST(GF_NAVIGATE_TYPE_2D);
	DECLARE_CONST(GF_NAVIGATE_TYPE_3D);

	JS_FreeValue(c, global);

    JS_SetModuleExport(c, m, "scene", sjs->scene_obj);
	return 0;
}


void qjs_module_init_scenejs(JSContext *ctx)
{
	JSModuleDef *m;
	m = JS_NewCModule(ctx, "scenejs", js_scene_init);
	if (!m) return;

	JS_AddModuleExport(ctx, m, "scene");
	return;
}


#endif

