/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2019-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / QuickJS bindings for GF_Filter
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


/*
	ANY CHANGE TO THE API MUST BE REFLECTED IN THE DOCUMENTATION IN gpac/share/doc/idl/jsf.idl
	(no way to define inline JS doc with doxygen)
*/

#include <gpac/filters.h>
#include <gpac/list.h>
#include <gpac/constants.h>
#include <gpac/network.h>

#ifdef GPAC_HAS_QJS

#include <gpac/internal/scenegraph_dev.h>
#include "../scenegraph/qjs_common.h"


//to load session API
#include "../filter_core/filter_session.h"

/*
	Currently unmapped functions

//likely not needed
u32 	gf_filter_count_source_by_protocol (GF_Filter *filter, const char *protocol_scheme, Bool expand_proto, GF_FilterPid *(*enum_pids)(void *udta, u32 *idx), void *udta)
GF_FilterPacket * 	gf_filter_pck_new_frame_interface (GF_FilterPid *PID, GF_FilterFrameInterface *frame_ifce, gf_fsess_packet_destructor destruct)
GF_FilterFrameInterface * 	gf_filter_pck_get_frame_interface (GF_FilterPacket *pck)

//todo
GF_Err 	gf_filter_add_event_listener (GF_Filter *filter, GF_FSEventListener *el)
GF_Err 	gf_filter_remove_event_listener (GF_Filter *filter, GF_FSEventListener *el)
Bool 	gf_filter_forward_gf_event (GF_Filter *filter, GF_Event *evt, Bool consumed, Bool skip_user)
Bool 	gf_filter_send_gf_event (GF_Filter *filter, GF_Event *evt)
Bool 	gf_filter_ui_event (GF_Filter *filter, GF_Event *uievt)
void 	gf_filter_register_opengl_provider (GF_Filter *filter, Bool do_register)
GF_Err 	gf_filter_request_opengl (GF_Filter *filter)

//either not needed or require a dedicated module
GF_DownloadManager *gf_filter_get_download_manager (GF_Filter *filter)


*/

enum
{
	JSF_EVT_INITIALIZE=0,
	JSF_EVT_FINALIZE,
	JSF_EVT_CONFIGURE_PID,
	JSF_EVT_PROCESS,
	JSF_EVT_PROCESS_EVENT,
	JSF_EVT_UPDATE_ARG,
	JSF_EVT_PROBE_URL,
	JSF_EVT_PROBE_DATA,
	JSF_EVT_RECONFIGURE_OUTPUT,
	JSF_EVT_REMOVE_PID,

	JSF_EVT_LAST_DEFINED,

	JSF_FILTER_MAX_PIDS,
	JSF_FILTER_BLOCK_ENABLED,
	JSF_FILTER_OUTPUT_BUFFER,
	JSF_FILTER_OUTPUT_PLAYOUT,
	JSF_FILTER_SEP_ARGS,
	JSF_FILTER_SEP_NAME,
	JSF_FILTER_SEP_LIST,
	JSF_FILTER_DST_ARGS,
	JSF_FILTER_DST_NAME,
	JSF_FILTER_SINKS_DONE,
	JSF_FILTER_REPORTING_ENABLED,

	JSF_FILTER_CAPS_MAX_WIDTH,
	JSF_FILTER_CAPS_MAX_HEIGHT,
	JSF_FILTER_CAPS_MAX_DISPLAY_DEPTH,
	JSF_FILTER_CAPS_MAX_FPS,
	JSF_FILTER_CAPS_MAX_VIEWS,
	JSF_FILTER_CAPS_MAX_CHANNELS,
	JSF_FILTER_CAPS_MAX_SAMPLERATE,
	JSF_FILTER_CAPS_MAX_AUDIO_DEPTH,
	JSF_FILTER_NB_EVTS_QUEUED,
	JSF_FILTER_CLOCK_HINT_TIME,
	JSF_FILTER_CLOCK_HINT_MEDIATIME,
	JSF_FILTER_CONNECTIONS_PENDING,
	JSF_FILTER_INAME,
	JSF_FILTER_REQUIRE_SOURCEID,
	JSF_FILTER_PATH
};

enum
{
	JSF_SETUP_ERROR=0,
	JSF_NOTIF_ERROR,
	JSF_NOTIF_ERROR_AND_DISCONNECT
};


typedef struct
{
	//options
	const char *js;

	GF_Filter *filter;
	Bool is_custom;

	JSContext *ctx;

	Bool initialized;
	JSValue funcs[JSF_EVT_LAST_DEFINED];
	JSValue filter_obj;

	GF_FilterArgs *args;
	u32 nb_args;
	Bool has_wilcard_arg;

	GF_FilterCapability *caps;
	u32 nb_caps;

	GF_List *pids;
	char *log_name;

	GF_List *pck_res;

	Bool unload_session_api;
	Bool disable_filter;
} GF_JSFilterCtx;

enum
{
	JSF_PID_NAME=0,
	JSF_PID_EOS,
	JSF_PID_EOS_SEEN,
	JSF_PID_EOS_RECEIVED,
	JSF_PID_WOULD_BLOCK,
	JSF_PID_SPARSE,
	JSF_PID_FILTER_NAME,
	JSF_PID_FILTER_SRC,
	JSF_PID_FILTER_ARGS,
	JSF_PID_FILTER_SRC_ARGS,
	JSF_PID_FILTER_UNICITY_ARGS,
	JSF_PID_MAX_BUFFER,
	JSF_PID_LOOSE_CONNECT,
	JSF_PID_FRAMING_MODE,
	JSF_PID_BUFFER,
	JSF_PID_IS_FULL,
	JSF_PID_FIRST_EMPTY,
	JSF_PID_FIRST_CTS,
	JSF_PID_NB_PACKETS,
	JSF_PID_TIMESCALE,
	JSF_PID_CLOCK_MODE,
	JSF_PID_DISCARD,
	JSF_PID_SRC_URL,
	JSF_PID_DST_URL,
	JSF_PID_REQUIRE_SOURCEID,
	JSF_PID_RECOMPUTE_DTS,
	JSF_PID_MIN_PCK_DUR,
	JSF_PID_IS_PLAYING,
	JSF_PID_NEXT_TS,
	JSF_PID_HAS_DECODER,
};
typedef struct
{
	GF_JSFilterCtx *jsf;
	GF_FilterPid *pid;
	JSValue jsobj;
	struct _js_pck_ctx *pck_head;
	GF_List *shared_pck;
} GF_JSPidCtx;

enum
{
	GF_JS_PCK_IS_REF = 1,
	GF_JS_PCK_IS_SHARED = 1<<1,
	GF_JS_PCK_IS_OUTPUT = 1<<2,
	GF_JS_PCK_IS_DANGLING = 1<<3,
};

typedef struct _js_pck_ctx
{
	GF_JSPidCtx *jspid;
	GF_FilterPacket *pck;
	JSValue jsobj;
	//shared packet, this is a string or an array buffer
	JSValue ref_val;
	//shared packet callback
	JSValue cbck_val;
	//array buffer
	JSValue data_ab;
	u32 flags;
} GF_JSPckCtx;

typedef enum
{
	JSF_FINST_SOURCE=0,
	JSF_FINST_DEST,
	JSF_FINST_FILTER,
} GF_JSFilterMode;
typedef struct
{
	GF_JSFilterCtx *jsf;
	GF_Filter *filter;
	JSValue filter_obj;
	GF_JSFilterMode fmode;
	JSValue setup_failure_fun;

} GF_JSFilterInstanceCtx;


static JSClassID jsf_filter_class_id;

static void jsf_filter_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func)
{
    GF_JSFilterCtx *jsf = JS_GetOpaque(val, jsf_filter_class_id);
    if (jsf) {
        u32 i;
        for (i=0; i<JSF_EVT_LAST_DEFINED; i++) {
            JS_MarkValue(rt, jsf->funcs[i], mark_func);
		}
    }
}
static JSClassDef jsf_filter_class = {
    "JSFilter",
	.gc_mark = jsf_filter_mark
};

static JSClassID jsf_filter_inst_class_id;

static void jsf_filter_inst_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func)
{
    GF_JSFilterInstanceCtx *f_inst = JS_GetOpaque(val, jsf_filter_inst_class_id);
    if (f_inst) {
		JS_MarkValue(rt, f_inst->setup_failure_fun, mark_func);
    }
}
static void jsf_filter_inst_finalizer(JSRuntime *rt, JSValue val) {
    GF_JSFilterInstanceCtx *f_inst = JS_GetOpaque(val, jsf_filter_inst_class_id);
    if (!f_inst) return;
    JS_FreeValueRT(rt, f_inst->setup_failure_fun);
    gf_free(f_inst);
}
static JSClassDef jsf_filter_inst_class = {
    "FilterInstance",
    .finalizer = jsf_filter_inst_finalizer,
	.gc_mark = jsf_filter_inst_mark
};

static JSClassID jsf_pid_class_id;
static JSClassDef jsf_pid_class = {
    "FilterPid",
};

static JSClassID jsf_event_class_id;
static void jsf_evt_finalizer(JSRuntime *rt, JSValue val)
{
	GF_FilterEvent *evt = JS_GetOpaque(val, jsf_event_class_id);
    if (!evt) return;
    if (evt->base.type==GF_FEVT_USER) {
		if (evt->user_event.event.type==GF_EVENT_SET_CAPTION) {
			if (evt->user_event.event.caption.caption)
				gf_free((char *) evt->user_event.event.caption.caption);
		}
	}
	gf_free(evt);
}
static JSClassDef jsf_event_class = {
    "FilterEvent",
    .finalizer = jsf_evt_finalizer
};

static JSClassID jsf_pck_class_id;

static void jsf_pck_detach_ab(JSContext *ctx, GF_JSPckCtx *pckctx)
{
	if (!JS_IsUndefined(pckctx->data_ab)) {
		JS_DetachArrayBuffer(ctx, pckctx->data_ab);
		JS_FreeValue(ctx, pckctx->data_ab);
		pckctx->data_ab = JS_UNDEFINED;
	}
}

static void jsf_pck_finalizer(JSRuntime *rt, JSValue val)
{
    GF_JSPckCtx *pckctx = JS_GetOpaque(val, jsf_pck_class_id);
    if (!pckctx) return;

	if (!JS_IsUndefined(pckctx->data_ab)) {
		JS_FreeValueRT(rt, pckctx->data_ab);
		pckctx->data_ab = JS_UNDEFINED;
	}
	//dangling packet
	if (pckctx->flags & GF_JS_PCK_IS_DANGLING) {
		//we don't keep a ref on jsobj
		//we may need to destroy the underlying packet if GCed but .discard was never called
		if (pckctx->pck) gf_filter_pck_discard(pckctx->pck);
		//we don't recycle the structure memory (no pid context attached
		gf_free(pckctx);
		return;
	}

    if (pckctx->jspid)
		pckctx->jspid->pck_head = NULL;

	/*we only keep a ref for input packet(s)*/
	if (pckctx->pck && !(pckctx->flags & GF_JS_PCK_IS_OUTPUT))
		JS_FreeValueRT(rt, pckctx->jsobj);


    if (JS_IsUndefined(pckctx->ref_val) && pckctx->jspid && pckctx->jspid->jsf) {
		gf_list_add(pckctx->jspid->jsf->pck_res, pckctx);
		memset(pckctx, 0, sizeof(GF_JSPckCtx));
	}
}

static void jsf_filter_pck_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func)
{
    GF_JSPckCtx *pckctx = JS_GetOpaque(val, jsf_pck_class_id);
    if (!pckctx) return;

	if (!(pckctx->flags & (GF_JS_PCK_IS_OUTPUT|GF_JS_PCK_IS_DANGLING)))
		JS_MarkValue(rt, pckctx->jsobj, mark_func);

    if (!JS_IsUndefined(pckctx->ref_val)) {
		JS_MarkValue(rt, pckctx->ref_val, mark_func);
	}

    if (!JS_IsUndefined(pckctx->data_ab)) {
		JS_MarkValue(rt, pckctx->data_ab, mark_func);
	}
}

static JSClassDef jsf_pck_class = {
    "FilterPacket",
    .finalizer = jsf_pck_finalizer,
	.gc_mark = jsf_filter_pck_mark
};

GF_DownloadManager *jsf_get_download_manager(JSContext *c)
{
	GF_JSFilterCtx *jsf;
	JSValue global = JS_GetGlobalObject(c);

	JSValue filter_obj = JS_GetPropertyStr(c, global, "filter");
	JS_FreeValue(c, global);
	if (JS_IsNull(filter_obj) || JS_IsException(filter_obj)) return NULL;
	jsf = JS_GetOpaque(filter_obj, jsf_filter_class_id);
	JS_FreeValue(c, filter_obj);
	if (!jsf) return NULL;
	return gf_filter_get_download_manager(jsf->filter);
}

GF_FilterSession *jsff_get_session(JSContext *c, JSValue this_val);
struct _gf_ft_mgr *gf_fs_get_font_manager(GF_FilterSession *fsess);

struct _gf_ft_mgr *jsf_get_font_manager(JSContext *c)
{
	JSValue global = JS_GetGlobalObject(c);
	JSValue obj;

	obj = JS_GetPropertyStr(c, global, "session");
	if (!JS_IsNull(obj) && !JS_IsException(obj)) {
		GF_FilterSession *fs = jsff_get_session(c, obj);
		JS_FreeValue(c, obj);
		if (fs) {
			JS_FreeValue(c, global);
			return gf_fs_get_font_manager(fs);
		}
	}

	obj = JS_GetPropertyStr(c, global, "filter");
	JS_FreeValue(c, global);
	if (!JS_IsNull(obj) && !JS_IsException(obj)) {
		GF_JSFilterCtx *jsf = JS_GetOpaque(obj, jsf_filter_class_id);
		JS_FreeValue(c, obj);
		if (jsf)
			return gf_filter_get_font_manager(jsf->filter);
	}
	return NULL;
}

GF_Err jsf_request_opengl(JSContext *c)
{
	GF_JSFilterCtx *jsf;
	JSValue global = JS_GetGlobalObject(c);

	JSValue filter_obj = JS_GetPropertyStr(c, global, "filter");
	JS_FreeValue(c, global);
	if (JS_IsNull(filter_obj) || JS_IsException(filter_obj)) return GF_BAD_PARAM;
	jsf = JS_GetOpaque(filter_obj, jsf_filter_class_id);
	JS_FreeValue(c, filter_obj);

	return gf_filter_request_opengl(jsf->filter);
}
GF_Err jsf_set_gl_active(JSContext *c, Bool do_activate)
{
	GF_JSFilterCtx *jsf;
	JSValue global = JS_GetGlobalObject(c);

	JSValue filter_obj = JS_GetPropertyStr(c, global, "filter");
	JS_FreeValue(c, global);
	if (JS_IsNull(filter_obj) || JS_IsException(filter_obj)) return GF_BAD_PARAM;
	jsf = JS_GetOpaque(filter_obj, jsf_filter_class_id);
	JS_FreeValue(c, filter_obj);

	return gf_filter_set_active_opengl_context(jsf->filter, do_activate);
}

Bool jsf_is_packet(JSContext *c, JSValue obj)
{
	GF_JSPckCtx *pckc = JS_GetOpaque(obj, jsf_pck_class_id);
	if (pckc) return GF_TRUE;
	return GF_FALSE;
}
const GF_FilterPacket *jsf_get_packet(JSContext *c, JSValue obj)
{
	GF_JSPckCtx *pckc = JS_GetOpaque(obj, jsf_pck_class_id);
	if (pckc) return pckc->pck;
	return NULL;
}

GF_Err jsf_get_filter_packet_planes(JSContext *c, JSValue obj, u32 *width, u32 *height, u32 *pf, u32 *stride, u32 *stride_uv, const u8 **data, const u8 **p_u, const u8 **p_v, const u8 **p_a)
{
	u32 cid;
	GF_FilterFrameInterface *frame_ifce;
	const GF_PropertyValue *p;
	GF_JSPckCtx *pckc = JS_GetOpaque(obj, jsf_pck_class_id);
	if (!pckc) return GF_BAD_PARAM;

#define GETPROP(_code, _v, opt)\
	p = gf_filter_pid_get_property(pckc->jspid->pid, _code);\
	if (!opt && !p) return GF_BAD_PARAM;\
	_v = p ? p->value.uint : 0;\

	GETPROP(GF_PROP_PID_CODECID, cid, 0)
	if (cid != GF_CODECID_RAW) return GF_BAD_PARAM;
	GETPROP(GF_PROP_PID_WIDTH, *width, 0)
	GETPROP(GF_PROP_PID_HEIGHT, *height, 0)
	GETPROP(GF_PROP_PID_PIXFMT, *pf, 0)
	GETPROP(GF_PROP_PID_STRIDE, *stride, 1)
	GETPROP(GF_PROP_PID_STRIDE_UV, *stride_uv, 1)
#undef GETPROP

	if (!data || !p_u || !p_v || !p_a) return GF_OK;

	frame_ifce = gf_filter_pck_get_frame_interface(pckc->pck);
	if (!frame_ifce) {
		u32 size=0;
		*data = (u8 *) gf_filter_pck_get_data(pckc->pck, &size);
	} else {
		u32 st_o;
		frame_ifce->get_plane(frame_ifce, 0, data, stride);
		frame_ifce->get_plane(frame_ifce, 1, p_u, stride_uv);
		//todo we need to cleanup alpha frame fetch, how do we differentiate between NV12+alpha (3 planes) and YUV420 ?
		frame_ifce->get_plane(frame_ifce, 2, p_v, &st_o);
		frame_ifce->get_plane(frame_ifce, 3, p_a, &st_o);
	}
	return GF_OK;
}

const char *jsf_get_script_filename(JSContext *c)
{
	GF_JSFilterCtx *jsf;
	JSValue global = JS_GetGlobalObject(c);
	JSValue filter_obj = JS_GetPropertyStr(c, global, "filter");
	JS_FreeValue(c, global);
	if (JS_IsNull(filter_obj) || JS_IsException(filter_obj)) {
		return NULL;
	}

	jsf = JS_GetOpaque(filter_obj, jsf_filter_class_id);
	JS_FreeValue(c, filter_obj);
	if (jsf) return jsf->js;

	JSValue g = JS_GetGlobalObject(c);
	JSValue v = JS_GetPropertyStr(c, g, "_gpac_script_src");
	const char *parent = NULL;
	if (!JS_IsUndefined(v))
		parent = JS_ToCString(c, v);
	JS_FreeValue(c, g);
	JS_FreeCString(c, parent);
	JS_FreeValue(c, v);
	return parent;
}

JSValue jsf_NewProp(JSContext *ctx, const GF_PropertyValue *new_val)
{
	JSValue res;
	u32 i;
	if (!new_val) return JS_NULL;

	switch (new_val->type) {
	case GF_PROP_BOOL:
		return JS_NewBool(ctx, new_val->value.boolean);
	case GF_PROP_UINT:
	case GF_PROP_SINT:
		return JS_NewInt32(ctx, new_val->value.sint);
	case GF_PROP_4CC:
		return JS_NewString(ctx, gf_4cc_to_str(new_val->value.uint) );
	case GF_PROP_LUINT:
		return JS_NewInt64(ctx, new_val->value.longuint);
	case GF_PROP_LSINT:
		return JS_NewInt64(ctx, new_val->value.longsint);
	case GF_PROP_FLOAT:
		return JS_NewFloat64(ctx, FIX2FLT(new_val->value.fnumber));
	case GF_PROP_DOUBLE:
		return JS_NewFloat64(ctx, new_val->value.number);
	case GF_PROP_STRING:
	case GF_PROP_STRING_NO_COPY:
	case GF_PROP_NAME:
		if (!new_val->value.string) return JS_NULL;
		return JS_NewString(ctx, new_val->value.string);
	case GF_PROP_VEC2:
		res = JS_NewObject(ctx);
		JS_SetPropertyStr(ctx, res, "x", JS_NewFloat64(ctx, new_val->value.vec2.x));
		JS_SetPropertyStr(ctx, res, "y", JS_NewFloat64(ctx, new_val->value.vec2.y));
		return res;
	case GF_PROP_VEC2I:
		res = JS_NewObject(ctx);
		JS_SetPropertyStr(ctx, res, "x", JS_NewInt32(ctx, new_val->value.vec2i.x));
		JS_SetPropertyStr(ctx, res, "y", JS_NewInt32(ctx, new_val->value.vec2i.y));
		return res;
	case GF_PROP_VEC3I:
		res = JS_NewObject(ctx);
		JS_SetPropertyStr(ctx, res, "x", JS_NewInt32(ctx, new_val->value.vec3i.x));
		JS_SetPropertyStr(ctx, res, "y", JS_NewInt32(ctx, new_val->value.vec3i.y));
		JS_SetPropertyStr(ctx, res, "z", JS_NewInt32(ctx, new_val->value.vec3i.z));
		return res;
	case GF_PROP_VEC4I:
		res = JS_NewObject(ctx);
		JS_SetPropertyStr(ctx, res, "x", JS_NewInt32(ctx, new_val->value.vec4i.x));
		JS_SetPropertyStr(ctx, res, "y", JS_NewInt32(ctx, new_val->value.vec4i.y));
		JS_SetPropertyStr(ctx, res, "z", JS_NewInt32(ctx, new_val->value.vec4i.z));
		JS_SetPropertyStr(ctx, res, "w", JS_NewInt32(ctx, new_val->value.vec4i.w));
		return res;
	case GF_PROP_FRACTION:
		res = JS_NewObject(ctx);
		JS_SetPropertyStr(ctx, res, "n", JS_NewInt32(ctx, new_val->value.frac.num));
		JS_SetPropertyStr(ctx, res, "d", JS_NewInt32(ctx, new_val->value.frac.den));
		return res;
	case GF_PROP_FRACTION64:
		res = JS_NewObject(ctx);
		JS_SetPropertyStr(ctx, res, "n", JS_NewInt64(ctx, new_val->value.lfrac.num));
		JS_SetPropertyStr(ctx, res, "d", JS_NewInt64(ctx, new_val->value.lfrac.den));
		return res;
	case GF_PROP_UINT_LIST:
		res = JS_NewArray(ctx);
		for (i=0; i<new_val->value.uint_list.nb_items; i++) {
        	JS_SetPropertyUint32(ctx, res, i, JS_NewInt64(ctx, new_val->value.uint_list.vals[i]) );
		}
		return res;
	case GF_PROP_4CC_LIST:
		res = JS_NewArray(ctx);
		for (i=0; i<new_val->value.uint_list.nb_items; i++) {
        	JS_SetPropertyUint32(ctx, res, i, JS_NewString(ctx, gf_4cc_to_str(new_val->value.uint_list.vals[i]) ) );
		}
		return res;
	case GF_PROP_SINT_LIST:
		res = JS_NewArray(ctx);
		for (i=0; i<new_val->value.sint_list.nb_items; i++) {
        	JS_SetPropertyUint32(ctx, res, i, JS_NewInt32(ctx, new_val->value.sint_list.vals[i]) );
		}
		return res;
	case GF_PROP_VEC2I_LIST:
		res = JS_NewArray(ctx);
		for (i=0; i<new_val->value.v2i_list.nb_items; i++) {
			JSValue item = JS_NewObject(ctx);
			JS_SetPropertyStr(ctx, item, "x", JS_NewInt32(ctx, new_val->value.v2i_list.vals[i].x));
			JS_SetPropertyStr(ctx, item, "y", JS_NewInt32(ctx, new_val->value.v2i_list.vals[i].y));
        	JS_SetPropertyUint32(ctx, res, i, item);
		}
		return res;
	case GF_PROP_STRING_LIST:
		res = JS_NewArray(ctx);
		for (i=0; i<new_val->value.string_list.nb_items; i++) {
        	JS_SetPropertyUint32(ctx, res, i, JS_NewString(ctx, new_val->value.string_list.vals[i] ) );
		}
		return res;
	case GF_PROP_DATA:
		return JS_NewArrayBufferCopy(ctx, new_val->value.data.ptr, new_val->value.data.size);

	default:
		if (gf_props_type_is_enum(new_val->type)) {
			return JS_NewString(ctx, gf_props_enum_name(new_val->type, new_val->value.uint));
		}
		return JS_NULL;
	}
}

JSValue jsf_NewPropTranslate(JSContext *ctx, const GF_PropertyValue *prop, u32 p4cc)
{
	const char *cid;
	JSValue res;
	switch (p4cc) {
	case GF_PROP_PID_CODECID:
		cid = gf_codecid_file_ext(prop->value.uint);
		if (!cid) res = GF_JS_EXCEPTION(ctx);
		else {
			char *sep = strchr(cid, '|');
			if (sep)
				res = JS_NewStringLen(ctx, cid, sep-cid);
			else
				res = JS_NewString(ctx, cid);
		}
		break;
	case GF_PROP_PID_STREAM_TYPE:
		res = JS_NewString(ctx, gf_stream_type_name(prop->value.uint));
		break;
	case GF_PROP_PID_CHANNEL_LAYOUT:
		res = JS_NewString(ctx, gf_audio_fmt_get_layout_name(prop->value.longuint));
		break;
	//PCM format and pixel format are usually notified as UINT types
	case GF_PROP_PID_AUDIO_FORMAT:
		res = JS_NewString(ctx, gf_props_enum_name(GF_PROP_PCMFMT, prop->value.uint));
		break;
	case GF_PROP_PID_PIXFMT:
	case GF_PROP_PID_PIXFMT_WRAPPED:
		res = JS_NewString(ctx, gf_props_enum_name(GF_PROP_PIXFMT, prop->value.uint));
		break;

	default:
		res = jsf_NewProp(ctx, prop);
		break;
	}
	return res;
}

GF_Err jsf_ToProp_ex(GF_Filter *filter, JSContext *ctx, JSValue value, u32 p4cc, GF_PropertyValue *prop, u32 prop_type)
{
	u32 type;
	memset(prop, 0, sizeof(GF_PropertyValue));

	type = prop_type ? prop_type : GF_PROP_STRING;
	if (p4cc)
		type = gf_props_4cc_get_type(p4cc);

	if (JS_IsString(value)) {
		const char *val_str = JS_ToCString(ctx, value);
		if (p4cc==GF_PROP_PID_STREAM_TYPE) {
			prop->type = GF_PROP_UINT;
			prop->value.uint = gf_stream_type_by_name(val_str);
		} else if (p4cc==GF_PROP_PID_CODECID) {
			prop->type = GF_PROP_UINT;
			prop->value.uint = gf_codecid_parse(val_str);
		} else if (p4cc==GF_PROP_PID_CHANNEL_LAYOUT) {
			prop->type = GF_PROP_LUINT;
			prop->value.longuint = gf_audio_fmt_get_layout_from_name(val_str);
		} else {
			*prop = gf_props_parse_value(type, NULL, val_str, NULL, filter ? gf_filter_get_sep(filter, GF_FS_SEP_LIST) : ',');
		}
		JS_FreeCString(ctx, val_str);
		if (prop->type==GF_PROP_FORBIDEN) return GF_BAD_PARAM;
		return GF_OK;
	}
	if (JS_IsBool(value)) {
		prop->type = GF_PROP_BOOL;
		prop->value.boolean = JS_ToBool(ctx, value);
	}
	else if (JS_IsInteger(value)) {
		if (!JS_ToInt32(ctx, &prop->value.sint, value)) {
			prop->type = (p4cc && type) ? type : GF_PROP_SINT;
		} else if (!JS_ToInt64(ctx, &prop->value.longsint, value)) {
			prop->type = GF_PROP_LSINT;
		}
	}
	else if (JS_IsNumber(value)) {
		JS_ToFloat64(ctx, &prop->value.number, value);
		prop->type = GF_PROP_DOUBLE;
	}
	else if (JS_IsArray(ctx, value)) {
		u64 len = 0;
		u32 i;
		u32 atype = 0;
		JSValue js_length = JS_GetPropertyStr(ctx, value, "length");
		if (JS_ToIndex(ctx, &len, js_length)) {
			JS_FreeValue(ctx, js_length);
			return GF_BAD_PARAM;
		}
		JS_FreeValue(ctx, js_length);
		for (i=0; i<len; i++) {
			JSValue v = JS_GetPropertyUint32(ctx, value, i);
			if (JS_IsString(v)) {
				if (!i) atype = 1;
				else if (atype!=1) {
					JS_FreeValue(ctx, v);
					return GF_BAD_PARAM;
				}
			} else if (JS_IsInteger(v)) {
				if (!i) atype = 2;
				else if (atype!=2) {
					JS_FreeValue(ctx, v);
					return GF_BAD_PARAM;
				}
			}
			else {
				JS_FreeValue(ctx, v);
				return GF_BAD_PARAM;
			}
			if (!i) {
				if (atype==1) {
					prop->value.uint_list.nb_items = (u32) len;
					prop->value.uint_list.vals = gf_malloc((u32) (sizeof(char *)*len));
				} else {
					prop->value.uint_list.nb_items = (u32) len;
					prop->value.uint_list.vals = gf_malloc((u32) (sizeof(s32)*len));
				}
			}
			if (atype==1) {
				const char *str = JS_ToCString(ctx, v);
				prop->value.string_list.vals[i] = gf_strdup(str);
				JS_FreeCString(ctx, str);
			} else {
				JS_ToInt32(ctx, &prop->value.uint_list.vals[i], v);
			}
			JS_FreeValue(ctx, v);
		}
	}
	else if (JS_IsObject(value)) {
		Bool is_vec4 = 0;
		Bool is_vec3 = 0;
		u32 is_vec2 = 0;
		u32 is_frac = 0;
		Bool is_vec = GF_FALSE;
		GF_PropVec2 val_d;
		GF_PropVec4i val_i;
		GF_Fraction frac;
		GF_Fraction64 frac_l;

		JSValue res = JS_GetPropertyStr(ctx, value, "w");
		if (!JS_IsUndefined(res)) {
			JS_ToInt32(ctx, &val_i.w, res);
			is_vec4 = 1;
		}
		JS_FreeValue(ctx, res);

		res = JS_GetPropertyStr(ctx, value, "z");
		if (!JS_IsUndefined(res)) {
			JS_ToInt32(ctx, &val_i.z, res);
			is_vec3 = 1;
		}
		JS_FreeValue(ctx, res);

		res = JS_GetPropertyStr(ctx, value, "y");
		if (!JS_IsUndefined(res)) {
			is_vec2 = 1;
			if (JS_ToFloat64(ctx, &val_d.y, res)) {
				JS_ToInt32(ctx, &val_i.y, res);
				is_vec2 = 2;
			}
		}
		JS_FreeValue(ctx, res);

		res = JS_GetPropertyStr(ctx, value, "x");
		if (!JS_IsUndefined(res)) {
			is_vec = GF_TRUE;
			if (JS_ToFloat64(ctx, &val_d.x, res)) {
				JS_ToInt32(ctx, &val_i.x, res);
			}
		}
		JS_FreeValue(ctx, res);

		res = JS_GetPropertyStr(ctx, value, "d");
		if (!JS_IsUndefined(res)) {
			is_frac = 2;
			if (JS_ToInt32(ctx, &frac.den, res)) {
				JS_ToInt64(ctx, &frac_l.den, res);
				is_frac = 1;
			}
		}
		JS_FreeValue(ctx, res);
		if (is_frac) {
			res = JS_GetPropertyStr(ctx, value, "n");
			if (!JS_IsUndefined(res)) {
				is_frac = 2;
				if (JS_ToInt32(ctx, &frac.num, res)) {
					JS_ToInt64(ctx, &frac_l.num, res);
					is_frac = 1;
				}
			} else {
				is_frac = 0;
			}
			JS_FreeValue(ctx, res);
		}

		if (is_vec) {
			if (is_vec4) {
				prop->type = GF_PROP_VEC4I;
				prop->value.vec4i = val_i;
			} else if (is_vec3) {
				prop->type = GF_PROP_VEC3I;
				prop->value.vec3i.x = val_i.x;
				prop->value.vec3i.y = val_i.y;
				prop->value.vec3i.z = val_i.z;
			} else if (is_vec2==2) {
				prop->type = GF_PROP_VEC2I;
				prop->value.vec2i.x = val_i.x;
				prop->value.vec2i.y = val_i.y;
			} else if (is_vec2==1) {
				prop->type = GF_PROP_VEC2;
				prop->value.vec2.x = val_d.x;
				prop->value.vec2.y = val_d.y;
			}
		} else if (is_frac) {
			if (is_frac==2) {
				prop->type = GF_PROP_FRACTION;
				prop->value.frac = frac;
			} else {
				prop->type = GF_PROP_FRACTION64;
				prop->value.lfrac = frac_l;
			}
		}
		//try array buffer
		else {
			size_t ab_size;
			u8 *data = JS_GetArrayBuffer(ctx, &ab_size, value);
			if (!data)
				return GF_BAD_PARAM;

			if (prop_type==GF_PROP_CONST_DATA) {
				prop->value.data.ptr = data;
				prop->value.data.size = (u32) ab_size;
				prop->type = GF_PROP_CONST_DATA;
 			} else {
				prop->value.data.ptr = gf_malloc(ab_size);
				memcpy(prop->value.data.ptr, data, ab_size);
				prop->value.data.size = (u32) ab_size;
				prop->type = GF_PROP_DATA;
			}
		}
	} else if (JS_IsNull(value)) {
		prop->value.data.ptr = NULL;
		prop->value.data.size = 0;
		if (prop_type==GF_PROP_CONST_DATA) {
			prop->type = GF_PROP_CONST_DATA;
		} else {
			prop->type = GF_PROP_DATA;
		}
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[JSF] Unmapped JS object type, cannot convert back to property\n"));
		return GF_NOT_SUPPORTED;
	}

	if (prop->type==GF_PROP_STRING) {
		if ((type==GF_PROP_DATA) || (type==GF_PROP_CONST_DATA)) {
			char *str = prop->value.string;
			prop->value.data.ptr = str;
			prop->value.data.size = (u32) strlen(str);
			prop->type = GF_PROP_DATA;
		}
	}
	return GF_OK;
}

GF_Err jsf_ToProp(GF_Filter *filter, JSContext *ctx, JSValue value, u32 p4cc, GF_PropertyValue *prop)
{
	return jsf_ToProp_ex(filter, ctx, value, p4cc, prop, 0);

}
static JSValue jsf_filter_prop_set(JSContext *ctx, JSValueConst this_val, JSValueConst value, int magic)
{
	u32 ival;
	GF_FilterSessionCaps caps;
	GF_JSFilterCtx *jsf = JS_GetOpaque(this_val, jsf_filter_class_id);
	if (!jsf)
		return GF_JS_EXCEPTION(ctx);

	if (magic < JSF_EVT_LAST_DEFINED) {
		if (JS_IsFunction(ctx, value) || JS_IsUndefined(value) || JS_IsNull(value)) {
			JS_FreeValue(ctx, jsf->funcs[magic]);
			jsf->funcs[magic] = JS_DupValue(ctx, value);
		}
    	return JS_UNDEFINED;
	}
	switch (magic) {
	case JSF_FILTER_MAX_PIDS:
		if (JS_ToInt32(ctx, &ival, value))
			return GF_JS_EXCEPTION(ctx);
		gf_filter_set_max_extra_input_pids(jsf->filter, ival);
		break;

	case JSF_FILTER_CAPS_MAX_WIDTH:
		gf_filter_get_session_caps(jsf->filter, &caps);
		if (JS_ToInt32(ctx, &caps.max_screen_width, value)) return GF_JS_EXCEPTION(ctx);
		gf_filter_set_session_caps(jsf->filter, &caps);
		break;
	case JSF_FILTER_CAPS_MAX_HEIGHT:
		gf_filter_get_session_caps(jsf->filter, &caps);
		if (JS_ToInt32(ctx, &caps.max_screen_height, value)) return GF_JS_EXCEPTION(ctx);
		gf_filter_set_session_caps(jsf->filter, &caps);
		break;
	case JSF_FILTER_CAPS_MAX_DISPLAY_DEPTH:
		gf_filter_get_session_caps(jsf->filter, &caps);
		if (JS_ToInt32(ctx, &caps.max_screen_bpp, value)) return GF_JS_EXCEPTION(ctx);
		gf_filter_set_session_caps(jsf->filter, &caps);
		break;
	case JSF_FILTER_CAPS_MAX_FPS:
		gf_filter_get_session_caps(jsf->filter, &caps);
		if (JS_ToInt32(ctx, &caps.max_screen_fps, value)) return GF_JS_EXCEPTION(ctx);
		gf_filter_set_session_caps(jsf->filter, &caps);
		break;
	case JSF_FILTER_CAPS_MAX_VIEWS:
		gf_filter_get_session_caps(jsf->filter, &caps);
		if (JS_ToInt32(ctx, &caps.max_screen_nb_views, value)) return GF_JS_EXCEPTION(ctx);
		gf_filter_set_session_caps(jsf->filter, &caps);
		break;
	case JSF_FILTER_CAPS_MAX_CHANNELS:
		gf_filter_get_session_caps(jsf->filter, &caps);
		if (JS_ToInt32(ctx, &caps.max_audio_channels, value)) return GF_JS_EXCEPTION(ctx);
		gf_filter_set_session_caps(jsf->filter, &caps);
		break;
	case JSF_FILTER_CAPS_MAX_SAMPLERATE:
		gf_filter_get_session_caps(jsf->filter, &caps);
		if (JS_ToInt32(ctx, &caps.max_audio_sample_rate, value)) return GF_JS_EXCEPTION(ctx);
		gf_filter_set_session_caps(jsf->filter, &caps);
		break;
	case JSF_FILTER_CAPS_MAX_AUDIO_DEPTH:
		gf_filter_get_session_caps(jsf->filter, &caps);
		if (JS_ToInt32(ctx, &caps.max_audio_bit_depth, value)) return GF_JS_EXCEPTION(ctx);
		gf_filter_set_session_caps(jsf->filter, &caps);
		break;

	case JSF_FILTER_INAME:
	{
		const char *val = JS_ToCString(ctx, value);
		if (jsf->filter->iname) gf_free(jsf->filter->iname);
		if (val)
			jsf->filter->iname = gf_strdup(val);
		else
			jsf->filter->iname = NULL;
		JS_FreeCString(ctx, val);
	}
		break;
	case JSF_FILTER_REQUIRE_SOURCEID:
		if (JS_ToBool(ctx, value))
			gf_filter_require_source_id(jsf->filter);
		break;
	}
    return JS_UNDEFINED;
}

static JSValue jsf_filter_prop_get(JSContext *ctx, JSValueConst this_val, int magic)
{
	u32 ival;
	s64 lival;
	Double dval;
	char *str;
	char szSep[2];
	GF_FilterSessionCaps caps;
	JSValue res;
	GF_JSFilterCtx *jsf = JS_GetOpaque(this_val, jsf_filter_class_id);
	if (!jsf)
		return GF_JS_EXCEPTION(ctx);

	if (magic<JSF_EVT_LAST_DEFINED)
		return JS_DupValue(jsf->ctx, jsf->funcs[magic]);

	switch (magic) {
	case JSF_FILTER_MAX_PIDS:
		return JS_NewInt32(ctx, gf_filter_get_max_extra_input_pids(jsf->filter));
	case JSF_FILTER_BLOCK_ENABLED:
    	return JS_NewBool(jsf->ctx, gf_filter_block_enabled(jsf->filter));
	case JSF_FILTER_OUTPUT_BUFFER:
		gf_filter_get_output_buffer_max(jsf->filter, &ival, NULL);
    	return JS_NewInt32(jsf->ctx, ival);
	case JSF_FILTER_OUTPUT_PLAYOUT:
		gf_filter_get_output_buffer_max(jsf->filter, NULL, &ival);
    	return JS_NewInt32(jsf->ctx, ival);

	case JSF_FILTER_SEP_ARGS:
		szSep[1]=0;
		szSep[0] = gf_filter_get_sep(jsf->filter, GF_FS_SEP_ARGS);
    	return JS_NewString(jsf->ctx, szSep);
	case JSF_FILTER_SEP_NAME:
		szSep[1]=0;
		szSep[0] = gf_filter_get_sep(jsf->filter, GF_FS_SEP_NAME);
    	return JS_NewString(jsf->ctx, szSep);
	case JSF_FILTER_SEP_LIST:
		szSep[1]=0;
		szSep[0] = gf_filter_get_sep(jsf->filter, GF_FS_SEP_LIST);
    	return JS_NewString(jsf->ctx, szSep);

	case JSF_FILTER_DST_ARGS:
		str = (char *) gf_filter_get_dst_args(jsf->filter);
    	return str ? JS_NewString(jsf->ctx, str) : JS_NULL;
	case JSF_FILTER_DST_NAME:
		str = gf_filter_get_dst_name(jsf->filter);
		res = str ? JS_NewString(jsf->ctx, str) : JS_NULL;
		if (str) gf_free(str);
		return res;
	case JSF_FILTER_SINKS_DONE:
    	return JS_NewBool(jsf->ctx, gf_filter_all_sinks_done(jsf->filter) );
	case JSF_FILTER_REPORTING_ENABLED:
    	return JS_NewBool(jsf->ctx, gf_filter_reporting_enabled(jsf->filter) );

	case JSF_FILTER_CAPS_MAX_WIDTH:
		gf_filter_get_session_caps(jsf->filter, &caps);
		return JS_NewInt32(ctx, caps.max_screen_width);
	case JSF_FILTER_CAPS_MAX_HEIGHT:
		gf_filter_get_session_caps(jsf->filter, &caps);
		return JS_NewInt32(ctx, caps.max_screen_height);
	case JSF_FILTER_CAPS_MAX_DISPLAY_DEPTH:
		gf_filter_get_session_caps(jsf->filter, &caps);
		return JS_NewInt32(ctx, caps.max_screen_bpp);
	case JSF_FILTER_CAPS_MAX_FPS:
		gf_filter_get_session_caps(jsf->filter, &caps);
		return JS_NewInt32(ctx, caps.max_screen_fps);
	case JSF_FILTER_CAPS_MAX_VIEWS:
		gf_filter_get_session_caps(jsf->filter, &caps);
		return JS_NewInt32(ctx, caps.max_screen_nb_views);
	case JSF_FILTER_CAPS_MAX_CHANNELS:
		gf_filter_get_session_caps(jsf->filter, &caps);
		return JS_NewInt32(ctx, caps.max_audio_channels);
	case JSF_FILTER_CAPS_MAX_SAMPLERATE:
		gf_filter_get_session_caps(jsf->filter, &caps);
		return JS_NewInt32(ctx, caps.max_audio_sample_rate);
	case JSF_FILTER_CAPS_MAX_AUDIO_DEPTH:
		gf_filter_get_session_caps(jsf->filter, &caps);
		return JS_NewInt32(ctx, caps.max_audio_bit_depth);

	case JSF_FILTER_NB_EVTS_QUEUED:
		return JS_NewInt32(ctx, gf_filter_get_num_events_queued(jsf->filter) );
	case JSF_FILTER_CLOCK_HINT_TIME:
		gf_filter_get_clock_hint(jsf->filter, &lival, NULL);
		return JS_NewInt64(ctx, lival);
	case JSF_FILTER_CLOCK_HINT_MEDIATIME:
	{
		GF_Fraction64 frac;
		gf_filter_get_clock_hint(jsf->filter, NULL, &frac);
		if (!frac.den) return JS_NULL;
		dval = ((Double)frac.num) / frac.den;
	}
		return JS_NewFloat64(ctx, dval);
	case JSF_FILTER_CONNECTIONS_PENDING:
		return JS_NewBool(ctx, gf_filter_connections_pending(jsf->filter) );
	case JSF_FILTER_INAME:
		if (jsf->filter->iname) return JS_NewString(ctx, jsf->filter->iname);
		return JS_NULL;
	case JSF_FILTER_PATH:
	{
		char c=0;
		char *path = (char *) jsf_get_script_filename(ctx);
		if (!path) return JS_NULL;
		char *sep = strrchr(path, '/');
		if (!sep) sep = strrchr(path, '\\');
		if (!sep)
			return JS_NewString(ctx, "./");

		c = sep[1];
		sep[1] = 0;
		res = JS_NewString(ctx, path);
		sep[1] = c;
		return res;
	}
	}

    return JS_UNDEFINED;
}

static JSValue jsf_filter_set_arg(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JSValue v;
	const char *desc=NULL;
	const char *name=NULL;
	const char *def = NULL;
	const char *min_enum = NULL;
	u32 arg_flags=0;
	u32 type = 0;
	Bool is_wildcard=GF_FALSE;
	GF_JSFilterCtx *jsf = JS_GetOpaque(this_val, jsf_filter_class_id);
    if (!jsf || !argc) return GF_JS_EXCEPTION(ctx);

	v = JS_GetPropertyStr(ctx, argv[0], "name");
	if (!JS_IsUndefined(v)) name = JS_ToCString(ctx, v);
	JS_FreeValue(ctx, v);
	if (!name) return GF_JS_EXCEPTION(ctx);

	if (!strcmp(name, "*")) {
		if (jsf->has_wilcard_arg) return JS_UNDEFINED;
		is_wildcard = GF_TRUE;
		jsf->has_wilcard_arg = GF_TRUE;
	}

	v = JS_GetPropertyStr(ctx, argv[0], "desc");
	if (!JS_IsUndefined(v)) desc = JS_ToCString(ctx, v);
	JS_FreeValue(ctx, v);
	if (!desc) {
	 	JS_FreeCString(ctx, name);
	 	return GF_JS_EXCEPTION(ctx);
	}

	v = JS_GetPropertyStr(ctx, argv[0], "type");
	if (!JS_IsUndefined(v))
		JS_ToInt32(ctx, &type, v);
	JS_FreeValue(ctx, v);
	if (!type) {
		if (is_wildcard) {
			type = GF_PROP_STRING;
		} else {
			JS_FreeCString(ctx, name);
			JS_FreeCString(ctx, desc);
			return GF_JS_EXCEPTION(ctx);
		}
	}

	v = JS_GetPropertyStr(ctx, argv[0], "def");
	if (!JS_IsUndefined(v)) def = JS_ToCString(ctx, v);
	JS_FreeValue(ctx, v);

	v = JS_GetPropertyStr(ctx, argv[0], "minmax_enum");
	if (!JS_IsUndefined(v)) min_enum = JS_ToCString(ctx, v);
	JS_FreeValue(ctx, v);

	v = JS_GetPropertyStr(ctx, argv[0], "hint");
	if (!JS_IsUndefined(v)) {
		const char *hint = JS_ToCString(ctx, v);
		if (hint && !strcmp(hint, "expert")) arg_flags = GF_FS_ARG_HINT_EXPERT;
		else if (hint && !strcmp(hint, "advanced")) arg_flags = GF_FS_ARG_HINT_ADVANCED;
		else if (hint && !strcmp(hint, "hide")) arg_flags = GF_FS_ARG_HINT_HIDE;

		JS_FreeCString(ctx, hint);
	}
	JS_FreeValue(ctx, v);

	jsf->args = gf_realloc(jsf->args, sizeof(GF_FilterArgs)*(jsf->nb_args+2));
	memset(&jsf->args[jsf->nb_args], 0, 2*sizeof(GF_FilterArgs));
	jsf->args[jsf->nb_args].arg_name = gf_strdup(name);
	jsf->args[jsf->nb_args].arg_desc = desc ? gf_strdup(desc) : NULL;
	jsf->args[jsf->nb_args].arg_default_val = def ? gf_strdup(def) : NULL;
	jsf->args[jsf->nb_args].min_max_enum = min_enum ? gf_strdup(min_enum) : NULL;
	jsf->args[jsf->nb_args].arg_type = type;
	jsf->args[jsf->nb_args].offset_in_private = -1;
	jsf->args[jsf->nb_args].flags = arg_flags;

	jsf->nb_args ++;

	gf_filter_define_args(jsf->filter, jsf->args);

	JS_FreeCString(ctx, name);
	JS_FreeCString(ctx, desc);
	JS_FreeCString(ctx, def);
	JS_FreeCString(ctx, min_enum);
    return JS_UNDEFINED;
}


static JSValue jsf_filter_set_cap(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 prop_id = 0;
	Bool is_output=GF_FALSE;
	Bool is_inputoutput=GF_FALSE;
	Bool is_excluded=GF_FALSE;
	Bool is_static=GF_FALSE;
	Bool is_loaded_filter_only=GF_FALSE;
	Bool is_optional=GF_FALSE;
	GF_PropertyValue p;
	GF_JSFilterCtx *jsf = JS_GetOpaque(this_val, jsf_filter_class_id);
    if (!jsf) return GF_JS_EXCEPTION(ctx);
	jsf->disable_filter = GF_FALSE;

	memset(&p, 0, sizeof(GF_PropertyValue));
    if (argc) {
		u32 prop_type;
    	JSValue v;
		const char *name = NULL;

		v = JS_GetPropertyStr(ctx, argv[0], "id");
		if (!JS_IsUndefined(v)) name = JS_ToCString(ctx, v);
		JS_FreeValue(ctx, v);
		if (!name) return GF_JS_EXCEPTION(ctx);
		prop_id = gf_props_get_id(name);
		JS_FreeCString(ctx, name);
		if (!prop_id)
			return js_throw_err(ctx, GF_BAD_PARAM);
		prop_type = gf_props_4cc_get_type(prop_id);

		name = NULL;
		v = JS_GetPropertyStr(ctx, argv[0], "value");
		if (!JS_IsUndefined(v)) name = JS_ToCString(ctx, v);
		JS_FreeValue(ctx, v);

		if (prop_id==GF_PROP_PID_STREAM_TYPE) {
			p.type = GF_PROP_UINT;
			p.value.uint = gf_stream_type_by_name(name);
		} else if (prop_id==GF_PROP_PID_CODECID) {
			p.type = GF_PROP_UINT;
			p.value.uint = gf_codecid_parse(name);
		} else {
			p = gf_props_parse_value(prop_type, NULL, name, NULL, gf_filter_get_sep(jsf->filter, GF_FS_SEP_LIST) );
		}
		JS_FreeCString(ctx, name);

#define GETB(_v, _n)\
		v = JS_GetPropertyStr(ctx, argv[0], _n);\
		if (!JS_IsUndefined(v)) _v = JS_ToBool(ctx, v);\
		JS_FreeValue(ctx, v);\

		GETB(is_output, "output");
		GETB(is_inputoutput, "inout");
		GETB(is_excluded, "excluded");
		GETB(is_loaded_filter_only, "loaded_filter_only");
		GETB(is_static, "static");
		GETB(is_optional, "optional");
#undef GETB

	}

	jsf->caps = gf_realloc(jsf->caps, sizeof(GF_FilterCapability)*(jsf->nb_caps+1));
	memset(&jsf->caps[jsf->nb_caps], 0, sizeof(GF_FilterCapability));
	if (prop_id) {
		GF_FilterCapability *cap = &jsf->caps[jsf->nb_caps];
		cap->code = prop_id;
		cap->val = p;
		cap->flags = GF_CAPFLAG_IN_BUNDLE;
		if (is_inputoutput) {
			cap->flags |= GF_CAPFLAG_INPUT|GF_CAPFLAG_OUTPUT;
		} else if (is_output) {
			cap->flags |= GF_CAPFLAG_OUTPUT;
		} else {
			cap->flags |= GF_CAPFLAG_INPUT;
		}
		if (is_excluded)
			cap->flags |= GF_CAPFLAG_EXCLUDED;
		if (is_static)
			cap->flags |= GF_CAPFLAG_STATIC;
		if (is_loaded_filter_only)
			cap->flags |= GF_CAPFLAG_LOADED_FILTER;
		if (is_optional)
			cap->flags |= GF_CAPFLAG_OPTIONAL;
	}
	jsf->nb_caps ++;
	gf_filter_override_caps(jsf->filter, jsf->caps, jsf->nb_caps);
	return JS_UNDEFINED;
}

static JSValue jsf_filter_set_desc(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSFilterCtx *jsf = JS_GetOpaque(this_val, jsf_filter_class_id);
    if (!jsf) return GF_JS_EXCEPTION(ctx);
	const char *str = JS_ToCString(ctx, argv[0]);
    if (!str) return GF_JS_EXCEPTION(ctx);
	gf_filter_set_description(jsf->filter, str);
	JS_FreeCString(ctx, str);
    return JS_UNDEFINED;
}

static JSValue jsf_filter_set_version(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSFilterCtx *jsf = JS_GetOpaque(this_val, jsf_filter_class_id);
    if (!jsf) return GF_JS_EXCEPTION(ctx);
	const char *str = JS_ToCString(ctx, argv[0]);
    if (!str) return GF_JS_EXCEPTION(ctx);
	gf_filter_set_version(jsf->filter, str);
	JS_FreeCString(ctx, str);
    return JS_UNDEFINED;
}

static JSValue jsf_filter_set_author(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSFilterCtx *jsf = JS_GetOpaque(this_val, jsf_filter_class_id);
    if (!jsf) return GF_JS_EXCEPTION(ctx);
	const char *str = JS_ToCString(ctx, argv[0]);
    if (!str) return GF_JS_EXCEPTION(ctx);
	gf_filter_set_author(jsf->filter, str);
	JS_FreeCString(ctx, str);
    return JS_UNDEFINED;
}


static JSValue jsf_filter_set_help(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSFilterCtx *jsf = JS_GetOpaque(this_val, jsf_filter_class_id);
    if (!jsf) return GF_JS_EXCEPTION(ctx);
	const char *str = JS_ToCString(ctx, argv[0]);
    if (!str) return GF_JS_EXCEPTION(ctx);
	gf_filter_set_help(jsf->filter, str);
	JS_FreeCString(ctx, str);
    return JS_UNDEFINED;
}

static JSValue jsf_filter_set_name(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSFilterCtx *jsf = JS_GetOpaque(this_val, jsf_filter_class_id);
    if (!jsf) return GF_JS_EXCEPTION(ctx);
    if (jsf->log_name) gf_free(jsf->log_name);
    jsf->log_name = NULL;
    if (argc) {
    	JSValue global;
		const char *str = JS_ToCString(ctx, argv[0]);
		if (!str) return GF_JS_EXCEPTION(ctx);
    	jsf->log_name = gf_strdup(str);
		JS_FreeCString(ctx, str);
		gf_filter_set_name(jsf->filter, jsf->log_name);

		global = JS_GetGlobalObject(ctx);
    	JS_SetPropertyStr(ctx, global, "_gpac_log_name", JS_NewString(ctx, jsf->log_name));
    	JS_FreeValue(ctx, global);
    }
    return JS_UNDEFINED;
}


static JSValue jsf_filter_new_pid(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSPidCtx *pctx;
	GF_JSFilterCtx *jsf = JS_GetOpaque(this_val, jsf_filter_class_id);
    if (!jsf) return GF_JS_EXCEPTION(ctx);
    GF_FilterPid *opid = gf_filter_pid_new(jsf->filter);
	if (!opid) return GF_JS_EXCEPTION(ctx);
	jsf->disable_filter = GF_FALSE;

	GF_SAFEALLOC(pctx, GF_JSPidCtx);
	if (!pctx)
		return js_throw_err(ctx, GF_OUT_OF_MEM);
	gf_list_add(jsf->pids, pctx);
	pctx->jsf = jsf;
	pctx->pid = opid;
	pctx->jsobj = JS_NewObjectClass(ctx, jsf_pid_class_id);
	//keep ref to value we destroy it upon pid removal or filter desctruction
	pctx->jsobj = JS_DupValue(ctx, pctx->jsobj);
	JS_SetOpaque(pctx->jsobj, pctx);
	gf_filter_pid_set_udta(pctx->pid, pctx);
	return pctx->jsobj;
}

static JSValue jsf_filter_send_event(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Bool upstream = GF_FALSE;
	GF_JSFilterCtx *jsf = JS_GetOpaque(this_val, jsf_filter_class_id);
	GF_JSFilterInstanceCtx *jsfi = JS_GetOpaque(this_val, jsf_filter_inst_class_id);
    if (!jsf && !jsfi) return GF_JS_EXCEPTION(ctx);
	GF_FilterEvent *evt = JS_GetOpaque(argv[0], jsf_event_class_id);
    if (!evt) return GF_JS_EXCEPTION(ctx);
    if (argc>1) {
		upstream = JS_ToBool(ctx, argv[1]);
	}

	if (jsf)
		gf_filter_send_event(jsf->filter, evt, upstream);
	else
		gf_filter_send_event(jsfi->filter, evt, upstream);
    return JS_UNDEFINED;
}

static JSValue jsf_filter_get_info(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JSValue res;
	const char *name=NULL;
	const GF_PropertyValue *prop;
	GF_Filter *filter;
	GF_PropertyEntry *pe = NULL;
	GF_JSFilterCtx *jsf = JS_GetOpaque(this_val, jsf_filter_class_id);
	GF_JSFilterInstanceCtx *jsfi = JS_GetOpaque(this_val, jsf_filter_inst_class_id);
    if (!jsf && !jsfi) return GF_JS_EXCEPTION(ctx);
    name = JS_ToCString(ctx, argv[0]);
	if (!name) return GF_JS_EXCEPTION(ctx);
	filter = jsf ? jsf->filter : jsfi->filter;

	if ((argc>1) && JS_ToBool(ctx, argv[1])) {
		prop = gf_filter_get_info_str(filter, name, &pe);
		JS_FreeCString(ctx, name);
		if (!prop) return JS_NULL;
		res = jsf_NewProp(ctx, prop);
		JS_SetPropertyStr(ctx, res, "type", JS_NewInt32(ctx, prop->type));
	} else {
		u32 p4cc = gf_props_get_id(name);
		JS_FreeCString(ctx, name);
		if (!p4cc)
			return js_throw_err(ctx, GF_BAD_PARAM);
		prop = gf_filter_get_info(filter, p4cc, &pe);
		if (!prop) return JS_NULL;
		res = jsf_NewPropTranslate(ctx, prop, p4cc);
	}
	gf_filter_release_property(pe);
    return res;
}



static JSValue jsf_filter_is_supported_mime(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *mime=NULL;
	JSValue res;
	GF_JSFilterCtx *jsf = JS_GetOpaque(this_val, jsf_filter_class_id);
    if (!jsf || !argc) return GF_JS_EXCEPTION(ctx);
    mime = JS_ToCString(ctx, argv[0]);
	if (!mime) return GF_JS_EXCEPTION(ctx);
	res = JS_NewBool(ctx, gf_filter_is_supported_mime(jsf->filter, mime));
	JS_FreeCString(ctx, mime);
	return res;
}

static JSValue jsf_filter_is_supported_source(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *src=NULL;
	const char *parent=NULL;
	JSValue res;
	GF_JSFilterCtx *jsf = JS_GetOpaque(this_val, jsf_filter_class_id);
    if (!jsf || !argc) return GF_JS_EXCEPTION(ctx);
    src = JS_ToCString(ctx, argv[0]);
	if (!src) return GF_JS_EXCEPTION(ctx);
	if (argc>1) {
		parent = JS_ToCString(ctx, argv[1]);
		if (!parent) {
			JS_FreeCString(ctx, src);
			return GF_JS_EXCEPTION(ctx);
		}
	}
	res = JS_NewBool(ctx, gf_filter_is_supported_source(jsf->filter, src, parent) );
	JS_FreeCString(ctx, src);
	JS_FreeCString(ctx, parent);
	return res;
}

static JSValue jsf_filter_update_status(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *status;
    u32 pc=0;
	GF_JSFilterCtx *jsf = JS_GetOpaque(this_val, jsf_filter_class_id);
    if (!jsf || (argc<1))  return GF_JS_EXCEPTION(ctx);

    status = JS_ToCString(ctx, argv[0]);
	if (argc>1)
		JS_ToInt32(ctx, &pc, argv[1]);
    gf_filter_update_status(jsf->filter, pc, (char*) status);
	JS_FreeCString(ctx, status);
	return JS_UNDEFINED;
}

static JSValue jsf_filter_reschedule_in(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    u32 rt_delay=0;
	GF_JSFilterCtx *jsf = JS_GetOpaque(this_val, jsf_filter_class_id);
    if (!jsf) return GF_JS_EXCEPTION(ctx);

    if (argc) {
		if (JS_ToInt32(ctx, &rt_delay, argv[0]))
			return GF_JS_EXCEPTION(ctx);

		gf_filter_ask_rt_reschedule(jsf->filter, rt_delay);
	} else {
		gf_filter_post_process_task(jsf->filter);
	}
	return JS_UNDEFINED;
}

typedef struct
{
	JSValue func;
	JSValue obj;
	GF_JSFilterCtx *jsf;
} JS_ScriptTask;

Bool jsf_task_exec(GF_Filter *filter, void *callback, u32 *reschedule_ms)
{
	s32 fun_result = -1;
	JS_ScriptTask *task = (JS_ScriptTask *) callback;
	JSValue ret;
	gf_js_lock(task->jsf->ctx, GF_TRUE);

	if (JS_IsUndefined(task->obj)) {
		ret = JS_Call(task->jsf->ctx, task->func, task->jsf->filter_obj, 0, NULL);
	} else {
		ret = JS_Call(task->jsf->ctx, task->func, task->obj, 0, NULL);
	}

	if (JS_IsException(ret)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[%s] Error processing JS-defined task\n", task->jsf->log_name));
		js_dump_error(task->jsf->ctx);
	}
	else if (JS_IsInteger(ret)) {
		JS_ToInt32(task->jsf->ctx, &fun_result, ret);
	}
	JS_FreeValue(task->jsf->ctx, ret);

	gf_js_lock(task->jsf->ctx, GF_FALSE);
	js_std_loop(task->jsf->ctx);

	if (fun_result>=0) {
		*reschedule_ms = (u32) fun_result;
		return GF_TRUE;
	}
	if (!JS_IsUndefined(task->obj)) {
		JS_FreeValue(task->jsf->ctx, task->obj);
	}
	JS_FreeValue(task->jsf->ctx, task->func);
	gf_free(task);
	return GF_FALSE;
}
static JSValue jsf_filter_post_task(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JS_ScriptTask *task;
	GF_JSFilterCtx *jsf = JS_GetOpaque(this_val, jsf_filter_class_id);
    if (!jsf || !argc) return GF_JS_EXCEPTION(ctx);
	jsf->disable_filter = GF_FALSE;
	if (!JS_IsFunction(ctx, argv[0]))
		return GF_JS_EXCEPTION(ctx);
	if ((argc>1) && !JS_IsObject(argv[1]))
		return GF_JS_EXCEPTION(ctx);

	GF_SAFEALLOC(task, JS_ScriptTask);
	if (!task)
		return js_throw_err(ctx, GF_OUT_OF_MEM);
	task->jsf = jsf;
	task->func = JS_DupValue(ctx, argv[0]);
	task->obj = JS_UNDEFINED;
	if (argc>1)
		task->obj = JS_DupValue(ctx, argv[1]);

	gf_filter_post_task(jsf->filter, jsf_task_exec, task, "jsf_task");
	return JS_UNDEFINED;
}

static JSValue jsf_filter_notify_failure(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Err e;
	s32 error_type=0;
	GF_JSFilterCtx *jsf = JS_GetOpaque(this_val, jsf_filter_class_id);
    if (!jsf || !argc) return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt32(ctx, (int *) &e, argv[0]))
		return GF_JS_EXCEPTION(ctx);

	if (e==GF_OK) return JS_UNDEFINED;

	if ((argc>1) && JS_ToInt32(ctx, &error_type, argv[1]))
		return GF_JS_EXCEPTION(ctx);
	if (error_type==JSF_NOTIF_ERROR_AND_DISCONNECT)
		gf_filter_notification_failure(jsf->filter, e, GF_TRUE);
	else if (error_type==JSF_NOTIF_ERROR)
		gf_filter_notification_failure(jsf->filter, e, GF_FALSE);
	else
		gf_filter_setup_failure(jsf->filter, e);
	return JS_UNDEFINED;
}

static JSValue jsf_filter_hint_clock(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s64 time_in_us=0;
	GF_Fraction64 media_timestamp;
	GF_JSFilterCtx *jsf = JS_GetOpaque(this_val, jsf_filter_class_id);
    if (!jsf || (argc<2))  return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt64(ctx, &time_in_us, argv[0]))
		return GF_JS_EXCEPTION(ctx);
	if (argc==2) {
		Double t=0;
		if (JS_ToFloat64(ctx, &t, argv[0]))
			return GF_JS_EXCEPTION(ctx);
		media_timestamp.den = 1000000;
		media_timestamp.num = (s64) (t*media_timestamp.den);
	} else {
		if (JS_ToInt64(ctx, &media_timestamp.num, argv[0]))
			return GF_JS_EXCEPTION(ctx);
		if (JS_ToInt64(ctx, &media_timestamp.den, argv[0]))
			return GF_JS_EXCEPTION(ctx);
	}
	gf_filter_hint_single_clock(jsf->filter, time_in_us, media_timestamp);
	return JS_UNDEFINED;
}

static JSValue jsf_filter_send_update(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *filter_id=NULL;
	const char *arg_name=NULL;
	const char *arg_val=NULL;
	GF_EventPropagateType prop_mask = 0;
	JSValue ret = JS_UNDEFINED;
	GF_JSFilterCtx *jsf = JS_GetOpaque(this_val, jsf_filter_class_id);
	GF_JSFilterInstanceCtx *jsfi = JS_GetOpaque(this_val, jsf_filter_inst_class_id);
    if ((!jsf && !jsfi) || (argc!=4))  return GF_JS_EXCEPTION(ctx);

	filter_id = JS_ToCString(ctx, argv[0]);
	arg_name = JS_ToCString(ctx, argv[1]);
	arg_val = JS_ToCString(ctx, argv[2]);
	if (!arg_name || !arg_val || JS_ToInt32(ctx, (s32 *) &prop_mask, argv[3])) {
		ret = GF_JS_EXCEPTION(ctx);
		goto exit;
	}
	if (jsf)
		gf_filter_send_update(jsf->filter, filter_id, arg_name, arg_val, prop_mask);
	else
		gf_filter_send_update(jsfi->filter, filter_id, arg_name, arg_val, prop_mask);

exit:
	JS_FreeCString(ctx, filter_id);
	JS_FreeCString(ctx, arg_name);
	JS_FreeCString(ctx, arg_val);
	return ret;
}

static JSValue jsf_filter_load_filter(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, GF_JSFilterMode mode)
{
	const char *url=NULL;
	const char *parent=NULL;
	GF_Err e=GF_OK;
	Bool inherit_args = GF_FALSE;
	GF_JSFilterInstanceCtx *f_ctx;
	GF_JSFilterCtx *jsf = JS_GetOpaque(this_val, jsf_filter_class_id);
    if (!jsf || !argc) return GF_JS_EXCEPTION(ctx);
    url = JS_ToCString(ctx, argv[0]);
	if (!url) return GF_JS_EXCEPTION(ctx);

	if ((mode==JSF_FINST_SOURCE) && (argc>1)) {
		parent = JS_ToCString(ctx, argv[1]);
		if (!parent) {
			JS_FreeCString(ctx, url);
			return GF_JS_EXCEPTION(ctx);
		}
		if (argc>2) {
			inherit_args = JS_ToBool(ctx, argv[2]);
		}
	}
	GF_SAFEALLOC(f_ctx, GF_JSFilterInstanceCtx);
	if (!f_ctx) {
		JS_FreeCString(ctx, url);
		JS_FreeCString(ctx, parent);
		return js_throw_err(ctx, GF_OUT_OF_MEM);
	}

	f_ctx->fmode = mode;
	if (mode==JSF_FINST_SOURCE) {
		f_ctx->filter = gf_filter_connect_source(jsf->filter, url, parent, inherit_args, &e);
	} else if (mode==JSF_FINST_DEST) {
		f_ctx->filter = gf_filter_connect_destination(jsf->filter, url, &e);
	} else {
		f_ctx->filter = gf_filter_load_filter(jsf->filter, url, &e);
	}
	JS_FreeCString(ctx, url);
	JS_FreeCString(ctx, parent);
	if (!f_ctx->filter) {
		gf_free(f_ctx);
		return js_throw_err(ctx, e);
	}
	f_ctx->jsf = jsf;
	f_ctx->setup_failure_fun = JS_UNDEFINED;
	f_ctx->filter_obj = JS_NewObjectClass(ctx, jsf_filter_inst_class_id);
	JS_SetOpaque(f_ctx->filter_obj, f_ctx);
	return f_ctx->filter_obj;
}

static JSValue jsf_filter_add_source(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return jsf_filter_load_filter(ctx, this_val, argc, argv, JSF_FINST_SOURCE);

}
static JSValue jsf_filter_add_dest(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return jsf_filter_load_filter(ctx, this_val, argc, argv, JSF_FINST_DEST);
}
static JSValue jsf_filter_add_filter(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return jsf_filter_load_filter(ctx, this_val, argc, argv, JSF_FINST_FILTER);
}

static JSValue jsf_filter_lock(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSFilterCtx *jsf = JS_GetOpaque(this_val, jsf_filter_class_id);
	Bool do_lock;
    if (!jsf || !argc) return GF_JS_EXCEPTION(ctx);
    do_lock = JS_ToBool(ctx, argv[0]) ? GF_TRUE : GF_FALSE;
	gf_filter_lock(jsf->filter, do_lock);
	return JS_UNDEFINED;
}
static JSValue jsf_filter_lock_all(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSFilterCtx *jsf = JS_GetOpaque(this_val, jsf_filter_class_id);
	Bool do_lock;
    if (!jsf || !argc) return GF_JS_EXCEPTION(ctx);
    do_lock = JS_ToBool(ctx, argv[0]) ? GF_TRUE : GF_FALSE;
	gf_filter_lock_all(jsf->filter, do_lock);
	return JS_UNDEFINED;
}
static JSValue jsf_filter_make_sticky(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSFilterCtx *jsf = JS_GetOpaque(this_val, jsf_filter_class_id);
    if (!jsf) return GF_JS_EXCEPTION(ctx);
	gf_filter_make_sticky(jsf->filter);
	return JS_UNDEFINED;
}
static JSValue jsf_filter_prevent_blocking(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSFilterCtx *jsf = JS_GetOpaque(this_val, jsf_filter_class_id);
    if (!jsf || !argc) return GF_JS_EXCEPTION(ctx);
	gf_filter_prevent_blocking(jsf->filter, JS_ToBool(ctx, argv[0]) ? GF_TRUE : GF_FALSE);
	return JS_UNDEFINED;
}
static JSValue jsf_filter_block_eos(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSFilterCtx *jsf = JS_GetOpaque(this_val, jsf_filter_class_id);
    if (!jsf || !argc) return GF_JS_EXCEPTION(ctx);
	gf_filter_block_eos(jsf->filter, JS_ToBool(ctx, argv[0]) ? GF_TRUE : GF_FALSE);
	return JS_UNDEFINED;
}


static JSValue jsf_filter_abort(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSFilterCtx *jsf = JS_GetOpaque(this_val, jsf_filter_class_id);
    if (!jsf) return GF_JS_EXCEPTION(ctx);
	gf_filter_abort(jsf->filter);
	return JS_UNDEFINED;
}



static JSValue jsf_filter_set_source_internal(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, Bool use_restricted)
{
	GF_Err e=GF_OK;
	const char *source_id=NULL;
	GF_JSFilterCtx *jsf = JS_GetOpaque(this_val, jsf_filter_class_id);
	GF_JSFilterInstanceCtx *jsfi = JS_GetOpaque(this_val, jsf_filter_inst_class_id);
    if (!jsf && !jsfi) return GF_JS_EXCEPTION(ctx);

	GF_JSFilterCtx *f_from = JS_GetOpaque(argv[0], jsf_filter_class_id);
	GF_JSFilterInstanceCtx *fi_from = JS_GetOpaque(argv[0], jsf_filter_inst_class_id);
    if (!f_from && !fi_from)  return GF_JS_EXCEPTION(ctx);

    source_id = NULL;
    if (argc>1) {
		source_id = JS_ToCString(ctx, argv[1]);
		if (!source_id) return GF_JS_EXCEPTION(ctx);
		if (!source_id[0]) {
			JS_FreeCString(ctx, source_id);
			source_id = NULL;
		}
	}

	if (use_restricted)
		e = gf_filter_set_source_restricted(jsfi ? jsfi->filter : jsf->filter, fi_from ? fi_from->filter : f_from->filter, source_id);
	else
		e = gf_filter_set_source(jsfi ? jsfi->filter : jsf->filter, fi_from ? fi_from->filter : f_from->filter, source_id);

	JS_FreeCString(ctx, source_id);
	if (e) return js_throw_err(ctx, e);
	return JS_UNDEFINED;
}

static JSValue jsf_filter_set_source(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return jsf_filter_set_source_internal(ctx, this_val, argc, argv, GF_FALSE);
}

static JSValue jsf_filter_set_source_restricted(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return jsf_filter_set_source_internal(ctx, this_val, argc, argv, GF_TRUE);
}

static JSValue jsf_filter_reset_source(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSFilterCtx *jsf = JS_GetOpaque(this_val, jsf_filter_class_id);
	GF_JSFilterInstanceCtx *jsfi = JS_GetOpaque(this_val, jsf_filter_inst_class_id);
	if (!jsf && !jsfi) return GF_JS_EXCEPTION(ctx);
	gf_filter_reset_source(jsfi ? jsfi->jsf->filter : jsf->filter);
	return JS_UNDEFINED;
}

static JSValue jsf_filter_set_blocking(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSFilterCtx *jsf = JS_GetOpaque(this_val, jsf_filter_class_id);
	if (!jsf) return GF_JS_EXCEPTION(ctx);
	if (!argc) return GF_JS_EXCEPTION(ctx);
	gf_filter_set_blocking(jsf->filter, JS_ToBool(ctx, argv[0] ));
	return JS_UNDEFINED;
}


static const JSCFunctionListEntry jsf_filter_funcs[] = {
    JS_CGETSET_MAGIC_DEF("initialize", jsf_filter_prop_get, jsf_filter_prop_set, JSF_EVT_INITIALIZE),
    JS_CGETSET_MAGIC_DEF("finalize", jsf_filter_prop_get, jsf_filter_prop_set, JSF_EVT_FINALIZE),
    JS_CGETSET_MAGIC_DEF("configure_pid", jsf_filter_prop_get, jsf_filter_prop_set, JSF_EVT_CONFIGURE_PID),
    JS_CGETSET_MAGIC_DEF("process", jsf_filter_prop_get, jsf_filter_prop_set, JSF_EVT_PROCESS),
    JS_CGETSET_MAGIC_DEF("process_event", jsf_filter_prop_get, jsf_filter_prop_set, JSF_EVT_PROCESS_EVENT),
    JS_CGETSET_MAGIC_DEF("update_arg", jsf_filter_prop_get, jsf_filter_prop_set, JSF_EVT_UPDATE_ARG),
    JS_CGETSET_MAGIC_DEF("probe_data", jsf_filter_prop_get, jsf_filter_prop_set, JSF_EVT_PROBE_DATA),
    JS_CGETSET_MAGIC_DEF("probe_url", jsf_filter_prop_get, jsf_filter_prop_set, JSF_EVT_PROBE_URL),
    JS_CGETSET_MAGIC_DEF("reconfigure_output", jsf_filter_prop_get, jsf_filter_prop_set, JSF_EVT_RECONFIGURE_OUTPUT),
    JS_CGETSET_MAGIC_DEF("remove_pid", jsf_filter_prop_get, jsf_filter_prop_set, JSF_EVT_REMOVE_PID),
    JS_CGETSET_MAGIC_DEF("max_pids", jsf_filter_prop_get, jsf_filter_prop_set, JSF_FILTER_MAX_PIDS),
    JS_CGETSET_MAGIC_DEF("block_enabled", jsf_filter_prop_get, NULL, JSF_FILTER_BLOCK_ENABLED),
    JS_CGETSET_MAGIC_DEF("output_buffer", jsf_filter_prop_get, NULL, JSF_FILTER_OUTPUT_BUFFER),
    JS_CGETSET_MAGIC_DEF("output_playout", jsf_filter_prop_get, NULL, JSF_FILTER_OUTPUT_PLAYOUT),
    JS_CGETSET_MAGIC_DEF("sep_args", jsf_filter_prop_get, NULL, JSF_FILTER_SEP_ARGS),
    JS_CGETSET_MAGIC_DEF("sep_name", jsf_filter_prop_get, NULL, JSF_FILTER_SEP_NAME),
    JS_CGETSET_MAGIC_DEF("sep_list", jsf_filter_prop_get, NULL, JSF_FILTER_SEP_LIST),
    JS_CGETSET_MAGIC_DEF("dst_args", jsf_filter_prop_get, NULL, JSF_FILTER_DST_ARGS),
    JS_CGETSET_MAGIC_DEF("dst_name", jsf_filter_prop_get, NULL, JSF_FILTER_DST_NAME),
    JS_CGETSET_MAGIC_DEF("sinks_done", jsf_filter_prop_get, NULL, JSF_FILTER_SINKS_DONE),
    JS_CGETSET_MAGIC_DEF("reports_on", jsf_filter_prop_get, NULL, JSF_FILTER_REPORTING_ENABLED),
    JS_CGETSET_MAGIC_DEF("max_screen_width", jsf_filter_prop_get, jsf_filter_prop_set, JSF_FILTER_CAPS_MAX_WIDTH),
    JS_CGETSET_MAGIC_DEF("max_screen_height", jsf_filter_prop_get, jsf_filter_prop_set, JSF_FILTER_CAPS_MAX_HEIGHT),
    JS_CGETSET_MAGIC_DEF("max_screen_depth", jsf_filter_prop_get, jsf_filter_prop_set, JSF_FILTER_CAPS_MAX_DISPLAY_DEPTH),
    JS_CGETSET_MAGIC_DEF("max_screen_fps", jsf_filter_prop_get, jsf_filter_prop_set, JSF_FILTER_CAPS_MAX_FPS),
    JS_CGETSET_MAGIC_DEF("max_screen_views", jsf_filter_prop_get, jsf_filter_prop_set, JSF_FILTER_CAPS_MAX_VIEWS),
    JS_CGETSET_MAGIC_DEF("max_audio_channels", jsf_filter_prop_get, jsf_filter_prop_set, JSF_FILTER_CAPS_MAX_CHANNELS),
    JS_CGETSET_MAGIC_DEF("max_audio_samplerate", jsf_filter_prop_get, jsf_filter_prop_set, JSF_FILTER_CAPS_MAX_SAMPLERATE),
    JS_CGETSET_MAGIC_DEF("max_audio_depth", jsf_filter_prop_get, jsf_filter_prop_set, JSF_FILTER_CAPS_MAX_AUDIO_DEPTH),
    JS_CGETSET_MAGIC_DEF("events_queued", jsf_filter_prop_get, NULL, JSF_FILTER_NB_EVTS_QUEUED),
    JS_CGETSET_MAGIC_DEF("clock_hint_us", jsf_filter_prop_get, NULL, JSF_FILTER_CLOCK_HINT_TIME),
    JS_CGETSET_MAGIC_DEF("clock_hint_mediatime", jsf_filter_prop_get, NULL, JSF_FILTER_CLOCK_HINT_MEDIATIME),
    JS_CGETSET_MAGIC_DEF("connections_pending", jsf_filter_prop_get, NULL, JSF_FILTER_CONNECTIONS_PENDING),
	JS_CGETSET_MAGIC_DEF("iname", jsf_filter_prop_get, jsf_filter_prop_set, JSF_FILTER_INAME),
    JS_CGETSET_MAGIC_DEF("require_source_id", NULL, jsf_filter_prop_set, JSF_FILTER_REQUIRE_SOURCEID),
	JS_CGETSET_MAGIC_DEF("jspath", jsf_filter_prop_get, NULL, JSF_FILTER_PATH),

    JS_CFUNC_DEF("set_desc", 0, jsf_filter_set_desc),
    JS_CFUNC_DEF("set_version", 0, jsf_filter_set_version),
    JS_CFUNC_DEF("set_author", 0, jsf_filter_set_author),
    JS_CFUNC_DEF("set_help", 0, jsf_filter_set_help),
    JS_CFUNC_DEF("set_arg", 0, jsf_filter_set_arg),
    JS_CFUNC_DEF("set_cap", 0, jsf_filter_set_cap),
    JS_CFUNC_DEF("set_name", 0, jsf_filter_set_name),
    JS_CFUNC_DEF("new_pid", 0, jsf_filter_new_pid),
    JS_CFUNC_DEF("send_event", 0, jsf_filter_send_event),
    JS_CFUNC_DEF("get_info", 0, jsf_filter_get_info),
    JS_CFUNC_DEF("is_supported_mime", 0, jsf_filter_is_supported_mime),
    JS_CFUNC_DEF("update_status", 0, jsf_filter_update_status),
    JS_CFUNC_DEF("reschedule", 0, jsf_filter_reschedule_in),
    JS_CFUNC_DEF("post_task", 0, jsf_filter_post_task),
    JS_CFUNC_DEF("notify_failure", 0, jsf_filter_notify_failure),
    JS_CFUNC_DEF("is_supported_source", 0, jsf_filter_is_supported_source),
    JS_CFUNC_DEF("hint_clock", 0, jsf_filter_hint_clock),
    JS_CFUNC_DEF("send_update", 0, jsf_filter_send_update),
    JS_CFUNC_DEF("add_source", 0, jsf_filter_add_source),
    JS_CFUNC_DEF("add_destination", 0, jsf_filter_add_dest),
    JS_CFUNC_DEF("add_filter", 0, jsf_filter_add_filter),
    JS_CFUNC_DEF("lock", 0, jsf_filter_lock),
    JS_CFUNC_DEF("lock_all", 0, jsf_filter_lock_all),
    JS_CFUNC_DEF("make_sticky", 0, jsf_filter_make_sticky),
	JS_CFUNC_DEF("prevent_blocking", 1, jsf_filter_prevent_blocking),
	JS_CFUNC_DEF("block_eos", 1, jsf_filter_block_eos),
    JS_CFUNC_DEF("abort", 0, jsf_filter_abort),
    JS_CFUNC_DEF("set_source", 0, jsf_filter_set_source),
    JS_CFUNC_DEF("set_source_restricted", 0, jsf_filter_set_source_restricted),
    JS_CFUNC_DEF("reset_source", 0, jsf_filter_reset_source),
    JS_CFUNC_DEF("set_blocking", 0, jsf_filter_set_blocking),
};



static JSValue jsf_filter_remove(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSFilterInstanceCtx *jsfi = JS_GetOpaque(this_val, jsf_filter_inst_class_id);
	if (!jsfi) return GF_JS_EXCEPTION(ctx);
	if (jsfi->fmode==JSF_FINST_SOURCE)
		gf_filter_remove_src(jsfi->jsf->filter, jsfi->filter);
	return JS_UNDEFINED;
}



static JSValue jsf_filter_has_pid_connections_pending(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Filter *stop_at = NULL;
	GF_JSFilterInstanceCtx *jsfi = JS_GetOpaque(this_val, jsf_filter_inst_class_id);
	if (!jsfi) return GF_JS_EXCEPTION(ctx);
	if (argc && (JS_IsNull(argv[0]) || JS_IsObject(argv[0]) ) )  {
		GF_JSFilterInstanceCtx *fi_stop = JS_GetOpaque(this_val, jsf_filter_inst_class_id);
		GF_JSFilterCtx *f_stop = JS_GetOpaque(this_val, jsf_filter_class_id);
		if (fi_stop) stop_at = fi_stop->filter;
		else if (f_stop) stop_at = f_stop->filter;
	} else if (jsfi->fmode==JSF_FINST_SOURCE) {
		stop_at = jsfi->jsf->filter;
	}
	return JS_NewBool(ctx, gf_filter_has_pid_connection_pending(jsfi->filter, stop_at) );
}

static Bool jsf_on_setup_error(GF_Filter *f, void *on_setup_error_udta, GF_Err e)
{
	Bool res = GF_FALSE;
	JSValue ret, argv[1];
	GF_JSFilterInstanceCtx *f_inst = on_setup_error_udta;
	JSContext *ctx = f_inst->jsf->ctx;
	gf_js_lock(ctx, GF_TRUE);

	argv[0] = JS_NewInt32(ctx, e);

	ret = JS_Call(ctx, f_inst->setup_failure_fun, f_inst->filter_obj, 0, NULL);
	if (JS_IsBool(ret) && JS_ToBool(ctx, ret)) res = GF_TRUE;

	JS_FreeValue(ctx, argv[0]);
	JS_FreeValue(ctx, ret);
	gf_js_lock(ctx, GF_FALSE);
	js_std_loop(ctx);
	return res;
}

enum
{
	JSFI_SETUP_FAILURE = 0,
	JSFI_NAME,
	JSFI_TYPE,
	JSFI_INAME,
};

static JSValue jsf_filter_inst_prop_get(JSContext *ctx, JSValueConst this_val, int magic)
{
	GF_JSFilterInstanceCtx *f_inst = JS_GetOpaque(this_val, jsf_filter_inst_class_id);
    if (!f_inst) return GF_JS_EXCEPTION(ctx);

	switch (magic) {
	case JSFI_NAME:
		return JS_NewString(ctx, gf_filter_get_name(f_inst->filter));
	case JSFI_TYPE:
		return JS_NewString(ctx, f_inst->filter->freg->name);
	case JSFI_INAME:
		return f_inst->filter->iname ? JS_NewString(ctx, f_inst->filter->iname) : JS_NULL;
	}
	return JS_UNDEFINED;
}

static JSValue jsf_filter_inst_prop_set(JSContext *ctx, JSValueConst this_val, JSValueConst value, int magic)
{
	const char *str;
	GF_JSFilterInstanceCtx *f_inst = JS_GetOpaque(this_val, jsf_filter_inst_class_id);
    if (!f_inst) return GF_JS_EXCEPTION(ctx);

	switch (magic) {
	case JSFI_SETUP_FAILURE:
		if (JS_IsFunction(ctx, value) || JS_IsUndefined(value) || JS_IsNull(value)) {
			if (JS_IsUndefined(f_inst->setup_failure_fun) && !JS_IsUndefined(value)) {

				gf_filter_set_setup_failure_callback(f_inst->jsf->filter, f_inst->filter, jsf_on_setup_error, f_inst);
			}
			JS_FreeValue(ctx, f_inst->setup_failure_fun);
			f_inst->setup_failure_fun = JS_DupValue(ctx, value);
		}
    	return JS_UNDEFINED;

	case JSFI_INAME:
		str = JS_ToCString(ctx, value);
		if (f_inst->filter->iname) gf_free(f_inst->filter->iname);
		f_inst->filter->iname = str ? gf_strdup(str) : NULL;
		JS_FreeCString(ctx, str);
		break;
	}
	return JS_UNDEFINED;
}

static JSValue jsf_filter_inst_get_arg(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *arg_name=NULL;
	JSValue res;
	GF_JSFilterInstanceCtx *f_inst = JS_GetOpaque(this_val, jsf_filter_inst_class_id);
    if (!f_inst || !argc) return GF_JS_EXCEPTION(ctx);
    arg_name = JS_ToCString(ctx, argv[0]);
	if (!arg_name) return GF_JS_EXCEPTION(ctx);
	if ((argc>1) && JS_ToBool(ctx, argv[1])) {
		char dump[GF_PROP_DUMP_ARG_SIZE];
		const char *arg_val = gf_filter_get_arg_str(f_inst->filter, arg_name, dump);
		res = arg_val ? JS_NewString(ctx, dump) : JS_NULL;
	} else {
		GF_PropertyValue p;
		if (gf_filter_get_arg(f_inst->filter, arg_name, &p))
			res = jsf_NewProp(ctx, &p);
		else
			res = JS_NULL;
	}
	JS_FreeCString(ctx, arg_name);
	return res;
}


static JSValue jsf_filter_inst_disable_probe(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSFilterInstanceCtx *jsfi = JS_GetOpaque(this_val, jsf_filter_inst_class_id);
    if (!jsfi) return GF_JS_EXCEPTION(ctx);
	gf_filter_disable_probe(jsfi->filter);
	return JS_UNDEFINED;
}
static JSValue jsf_filter_inst_disable_inputs(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSFilterInstanceCtx *jsfi = JS_GetOpaque(this_val, jsf_filter_inst_class_id);
    if (!jsfi) return GF_JS_EXCEPTION(ctx);
    gf_filter_disable_inputs(jsfi->filter);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry jsf_filter_inst_funcs[] = {
	JS_CGETSET_MAGIC_DEF("on_setup_failure", NULL, jsf_filter_inst_prop_set, JSFI_SETUP_FAILURE),
	JS_CGETSET_MAGIC_DEF("name", jsf_filter_inst_prop_get, NULL, JSFI_NAME),
	JS_CGETSET_MAGIC_DEF("type", jsf_filter_inst_prop_get, NULL, JSFI_TYPE),
	JS_CGETSET_MAGIC_DEF("iname", jsf_filter_inst_prop_get, jsf_filter_inst_prop_set, JSFI_INAME),
    JS_CFUNC_DEF("send_event", 0, jsf_filter_send_event),
    JS_CFUNC_DEF("get_info", 0, jsf_filter_get_info),
    JS_CFUNC_DEF("send_update", 0, jsf_filter_send_update),
    JS_CFUNC_DEF("set_source", 0, jsf_filter_set_source),
    JS_CFUNC_DEF("set_source_restricted", 0, jsf_filter_set_source_restricted),
    JS_CFUNC_DEF("remove", 0, jsf_filter_remove),
    JS_CFUNC_DEF("has_pid_connections_pending", 0, jsf_filter_has_pid_connections_pending),
    JS_CFUNC_DEF("get_arg", 0, jsf_filter_inst_get_arg),
    JS_CFUNC_DEF("disable_probe", 0, jsf_filter_inst_disable_probe),
    JS_CFUNC_DEF("disable_inputs", 0, jsf_filter_inst_disable_inputs),
    JS_CFUNC_DEF("reset_source", 0, jsf_filter_reset_source),
};


static JSValue jsf_pid_set_prop(JSContext *ctx, JSValueConst this_val, JSValueConst value, int magic)
{
	u32 ival;
	GF_Err e = GF_OK;
	const char *str=NULL;
	GF_JSPidCtx *pctx = JS_GetOpaque(this_val, jsf_pid_class_id);
    if (!pctx) return GF_JS_EXCEPTION(ctx);
	switch (magic) {
	case JSF_PID_NAME:
		str = JS_ToCString(ctx, value);
		gf_filter_pid_set_name(pctx->pid, str);
    	break;
	case JSF_PID_EOS:
		if (JS_ToBool(ctx, value))
			gf_filter_pid_set_eos(pctx->pid);
    	break;
	case JSF_PID_MAX_BUFFER:
		if (JS_ToInt32(ctx, &ival, value))
			return GF_JS_EXCEPTION(ctx);
		gf_filter_pid_set_max_buffer(pctx->pid, ival);
    	break;
	case JSF_PID_LOOSE_CONNECT:
		if (JS_ToBool(ctx, value))
			gf_filter_pid_set_loose_connect(pctx->pid);
    	break;
	case JSF_PID_FRAMING_MODE:
		gf_filter_pid_set_framing_mode(pctx->pid, JS_ToBool(ctx, value) );
    	break;
	case JSF_PID_CLOCK_MODE:
		gf_filter_pid_set_clock_mode(pctx->pid, JS_ToBool(ctx, value) );
		break;
	case JSF_PID_DISCARD:
		gf_filter_pid_set_discard(pctx->pid, JS_ToBool(ctx, value) );
		break;
	case JSF_PID_REQUIRE_SOURCEID:
		if (JS_ToBool(ctx, value))
			gf_filter_pid_require_source_id(pctx->pid);
		break;
	case JSF_PID_RECOMPUTE_DTS:
		gf_filter_pid_recompute_dts(pctx->pid, JS_ToBool(ctx, value));
		break;
	}

	if (str)
		JS_FreeCString(ctx, str);
	if (e) return js_throw_err(ctx, e);
    return JS_UNDEFINED;
}

static JSValue jsf_pid_get_prop(JSContext *ctx, JSValueConst this_val, int magic)
{
	u64 dur;
	char *str;
	GF_JSPidCtx *pctx = JS_GetOpaque(this_val, jsf_pid_class_id);
    if (!pctx) return GF_JS_EXCEPTION(ctx);
	switch (magic) {
	case JSF_PID_NAME:
		return JS_NewString(ctx, gf_filter_pid_get_name(pctx->pid) );
	case JSF_PID_EOS:
		return JS_NewBool (ctx, gf_filter_pid_is_eos(pctx->pid) );
	case JSF_PID_EOS_SEEN:
		return JS_NewBool (ctx, gf_filter_pid_has_seen_eos(pctx->pid) );
	case JSF_PID_EOS_RECEIVED:
		return JS_NewBool (ctx, gf_filter_pid_eos_received(pctx->pid) );
	case JSF_PID_WOULD_BLOCK:
		return JS_NewBool(ctx, gf_filter_pid_would_block(pctx->pid) );
	case JSF_PID_SPARSE:
		return JS_NewBool(ctx, gf_filter_pid_is_sparse(pctx->pid) );
	case JSF_PID_FILTER_NAME:
		return JS_NewString(ctx, gf_filter_pid_get_filter_name(pctx->pid) );
	case JSF_PID_FILTER_SRC:
		return JS_NewString(ctx, gf_filter_pid_get_source_filter_name(pctx->pid) );
	case JSF_PID_FILTER_ARGS:
		return JS_NewString(ctx, gf_filter_pid_get_args(pctx->pid) );
	case JSF_PID_FILTER_SRC_ARGS:
		return JS_NewString(ctx, gf_filter_pid_orig_src_args(pctx->pid, GF_FALSE) );
	case JSF_PID_FILTER_UNICITY_ARGS:
		return JS_NewString(ctx, gf_filter_pid_orig_src_args(pctx->pid, GF_TRUE) );
	case JSF_PID_MAX_BUFFER:
		return JS_NewInt32(ctx, gf_filter_pid_get_max_buffer(pctx->pid) );
	case JSF_PID_BUFFER:
		return JS_NewInt64(ctx, gf_filter_pid_query_buffer_duration(pctx->pid, GF_FALSE) );
	case JSF_PID_IS_FULL:
		dur = gf_filter_pid_query_buffer_duration(pctx->pid, GF_TRUE);
		return JS_NewBool(ctx, !dur ? 0 : 1);
	case JSF_PID_FIRST_EMPTY:
		return JS_NewBool (ctx, gf_filter_pid_first_packet_is_empty(pctx->pid) );
	case JSF_PID_FIRST_CTS:
		if (gf_filter_pid_get_first_packet_cts(pctx->pid, &dur))
			return JS_NewInt64(ctx, dur);
		else
			return JS_NULL;
	case JSF_PID_NB_PACKETS:
		return JS_NewInt32(ctx, gf_filter_pid_get_packet_count(pctx->pid) );
	case JSF_PID_TIMESCALE:
		return JS_NewInt32(ctx, gf_filter_pid_get_timescale(pctx->pid) );
	case JSF_PID_SRC_URL:
		str = gf_filter_pid_get_source(pctx->pid);
		if (str) {
			JSValue res = JS_NewString(ctx, str);
			gf_free(str);
			return res;
		}
		return JS_NULL;
	case JSF_PID_DST_URL:
		str = gf_filter_pid_get_destination(pctx->pid);
		if (str) {
			JSValue res = JS_NewString(ctx, str);
			gf_free(str);
			return res;
		}
		return JS_NULL;
	case JSF_PID_MIN_PCK_DUR:
		return JS_NewInt32(ctx, gf_filter_pid_get_min_pck_duration(pctx->pid) );
	case JSF_PID_IS_PLAYING:
		return JS_NewBool(ctx, gf_filter_pid_is_playing(pctx->pid) );
	case JSF_PID_NEXT_TS:
		dur = gf_filter_pid_get_next_ts(pctx->pid);
		if (dur==GF_FILTER_NO_TS) return JS_NULL;
		return JS_NewInt64(ctx, dur);

	case JSF_PID_HAS_DECODER:
		return JS_NewBool(ctx, gf_filter_pid_has_decoder(pctx->pid) );
	}
    return JS_UNDEFINED;
}

static JSValue jsf_pid_send_event(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSPidCtx *pctx = JS_GetOpaque(this_val, jsf_pid_class_id);
    if (!pctx) return GF_JS_EXCEPTION(ctx);
	GF_FilterEvent *evt = JS_GetOpaque(argv[0], jsf_event_class_id);
    if (!evt) return GF_JS_EXCEPTION(ctx);
    evt->base.on_pid = pctx->pid;
    if (evt->base.type == GF_FEVT_PLAY) {
		gf_filter_pid_init_play_event(pctx->pid, evt, evt->play.start_range, evt->play.speed, pctx->jsf->log_name);
	}
	gf_filter_pid_send_event(pctx->pid, evt);
    return JS_UNDEFINED;
}

static JSValue jsf_pid_enum_properties(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s32 idx;
	u32 p4cc;
	const char *pname;
	JSValue res;
	const GF_PropertyValue *prop;
	GF_JSPidCtx *pctx = JS_GetOpaque(this_val, jsf_pid_class_id);
    if (!pctx) return GF_JS_EXCEPTION(ctx);
    if (JS_ToInt32(ctx, &idx, argv[0]))
		return GF_JS_EXCEPTION(ctx);

	if ((argc>1) && JS_ToBool(ctx, argv[1])) {
		prop = gf_filter_pid_enum_info(pctx->pid, &idx, &p4cc, &pname);
	} else {
		prop = gf_filter_pid_enum_properties(pctx->pid, &idx, &p4cc, &pname);
	}
	if (!prop) return JS_NULL;
	if (!pname) pname = gf_props_4cc_get_name(p4cc);
	if (!pname) return GF_JS_EXCEPTION(ctx);

	res = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, res, "name", JS_NewString(ctx, pname));
    JS_SetPropertyStr(ctx, res, "type", JS_NewInt32(ctx, prop->type));
    JS_SetPropertyStr(ctx, res, "value", jsf_NewProp(ctx, prop));
    return res;
}

static JSValue jsf_pid_get_property_ex(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, Bool is_info)
{
	JSValue res;
	const char *name=NULL;
	const GF_PropertyValue *prop;
	GF_PropertyEntry *pe = NULL;
	GF_JSPidCtx *pctx = JS_GetOpaque(this_val, jsf_pid_class_id);
    if (!pctx) return GF_JS_EXCEPTION(ctx);
    name = JS_ToCString(ctx, argv[0]);
	if (!name) return GF_JS_EXCEPTION(ctx);
	if ((argc>1) && JS_ToBool(ctx, argv[1])) {
		if (is_info) {
			prop = gf_filter_pid_get_info_str(pctx->pid, name, &pe);
		} else {
			prop = gf_filter_pid_get_property_str(pctx->pid, name);
		}
		JS_FreeCString(ctx, name);
		if (!prop) return JS_NULL;
		res = jsf_NewProp(ctx, prop);
		JS_SetPropertyStr(ctx, res, "type", JS_NewInt32(ctx, prop->type));
	} else {
		u32 p4cc = gf_props_get_id(name);
		JS_FreeCString(ctx, name);
		if (!p4cc) return GF_JS_EXCEPTION(ctx);
		if (is_info) {
			prop = gf_filter_pid_get_info(pctx->pid, p4cc, &pe);
		} else {
			prop = gf_filter_pid_get_property(pctx->pid, p4cc);
		}
		if (!prop && !is_info) {
			if ((p4cc==GF_PROP_PID_MIRROR) || (p4cc==GF_PROP_PID_ROTATE)) {
				prop = gf_filter_pid_get_property(pctx->pid, GF_PROP_PID_ISOM_TRACK_MATRIX);
				if (prop) {
					GF_Err gf_prop_matrix_decompose(const GF_PropertyValue *p, u32 *flip_mode, u32 *rot_mode);
					u32 flip_mode, rot_mode;
					if (gf_prop_matrix_decompose(prop, &flip_mode, &rot_mode)==GF_OK) {
						if (p4cc==GF_PROP_PID_MIRROR) return JS_NewInt32(ctx, flip_mode);
						else return JS_NewInt32(ctx, rot_mode);
					}
				}
			}
			return JS_NULL;
		}

		res = jsf_NewPropTranslate(ctx, prop, p4cc);
	}
	gf_filter_release_property(pe);
    return res;
}
static JSValue jsf_pid_get_property(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return jsf_pid_get_property_ex(ctx, this_val, argc, argv, GF_FALSE);
}
static JSValue jsf_pid_get_info(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return jsf_pid_get_property_ex(ctx, this_val, argc, argv, GF_TRUE);
}


static JSValue jsf_pid_get_packet(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JSValue res;
	GF_FilterPacket *pck;
	GF_JSPckCtx *pckctx;
	GF_JSPidCtx *pctx = JS_GetOpaque(this_val, jsf_pid_class_id);
    if (!pctx) return GF_JS_EXCEPTION(ctx);
	if (!pctx->jsf->filter->in_process)
		return js_throw_err_msg(ctx, GF_BAD_PARAM, "Filter %s attempt to query packet outside process callback not allowed!\n", pctx->jsf->filter->name);
		
    pck = gf_filter_pid_get_packet(pctx->pid);
	if (!pck) return JS_NULL;

	if (pctx->pck_head) {
		pckctx = pctx->pck_head;
		assert(pckctx->pck == pck);
		return JS_DupValue(ctx, pckctx->jsobj);
	}

	res = JS_NewObjectClass(ctx, jsf_pck_class_id);
	pckctx = gf_list_pop_back(pctx->jsf->pck_res);
	if (!pckctx) {
		GF_SAFEALLOC(pckctx, GF_JSPckCtx);
		if (!pckctx) return js_throw_err(ctx, GF_OUT_OF_MEM);
	}
	memset(pckctx, 0, sizeof(GF_JSPckCtx));
	pckctx->jspid = pctx;
	pckctx->pck = pck;
	pckctx->jsobj = JS_DupValue(ctx, res);
	pckctx->ref_val = JS_UNDEFINED;
	pckctx->data_ab = JS_UNDEFINED;
	pctx->pck_head = pckctx;

	JS_SetOpaque(res, pckctx);
    return res;
}
static JSValue jsf_pid_drop_packet(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSPckCtx *pckctx;
	GF_JSPidCtx *pctx = JS_GetOpaque(this_val, jsf_pid_class_id);
    if (!pctx) return GF_JS_EXCEPTION(ctx);
	if (!pctx->jsf->filter->in_process)
		return js_throw_err_msg(ctx, GF_BAD_PARAM, "Filter %s attempt to drop packet outside process callback not allowed!\n", pctx->jsf->filter->name);

	if (!pctx->pck_head) {
		if (gf_filter_pid_get_packet_count(pctx->pid)) {
    		gf_filter_pid_drop_packet(pctx->pid);
		}
		return JS_UNDEFINED;
	}

	pckctx = pctx->pck_head;
	pckctx->pck = NULL;
	pctx->pck_head = NULL;
	JS_FreeValue(ctx, pckctx->jsobj);
	pckctx->jsobj = JS_UNDEFINED;
    gf_filter_pid_drop_packet(pctx->pid);
    return JS_UNDEFINED;
}

static JSValue jsf_pid_is_filter_in_parents(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSPidCtx *pctx = JS_GetOpaque(this_val, jsf_pid_class_id);
    if (!pctx || !argc) return GF_JS_EXCEPTION(ctx);
	GF_JSFilterCtx *f_ctx = JS_GetOpaque(argv[0], jsf_filter_class_id);
	GF_JSFilterInstanceCtx *fi_ctx = JS_GetOpaque(argv[0], jsf_filter_inst_class_id);
    if (!f_ctx && !fi_ctx) return GF_JS_EXCEPTION(ctx);
	return JS_NewBool(ctx, gf_filter_pid_is_filter_in_parents(pctx->pid, f_ctx ? f_ctx->filter : fi_ctx->filter));
}

static JSValue jsf_pid_get_buffer_occupancy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JSValue res;
	Bool in_final_flush;
	u32 max_units, nb_pck, max_dur, dur;
	GF_JSPidCtx *pctx = JS_GetOpaque(this_val, jsf_pid_class_id);
    if (!pctx) return GF_JS_EXCEPTION(ctx);

	in_final_flush = !gf_filter_pid_get_buffer_occupancy(pctx->pid, &max_units, &nb_pck, &max_dur, &dur);
	res = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, res, "max_units", JS_NewInt32(ctx, max_units));
    JS_SetPropertyStr(ctx, res, "nb_pck", JS_NewInt32(ctx, nb_pck));
    JS_SetPropertyStr(ctx, res, "max_dur", JS_NewInt32(ctx, max_dur));
    JS_SetPropertyStr(ctx, res, "dur", JS_NewInt32(ctx, dur));
    JS_SetPropertyStr(ctx, res, "final_flush", JS_NewBool(ctx, in_final_flush));
    return res;
}
static JSValue jsf_pid_clear_eos(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSPidCtx *pctx = JS_GetOpaque(this_val, jsf_pid_class_id);
    if (!pctx || !argc) return GF_JS_EXCEPTION(ctx);
	gf_filter_pid_clear_eos(pctx->pid, JS_ToBool(ctx, argv[0]));
	return JS_UNDEFINED;
}
static JSValue jsf_pid_check_caps(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSPidCtx *pctx = JS_GetOpaque(this_val, jsf_pid_class_id);
    if (!pctx) return GF_JS_EXCEPTION(ctx);
	return JS_NewBool(ctx, gf_filter_pid_check_caps(pctx->pid));
}
static JSValue jsf_pid_discard_block(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSPidCtx *pctx = JS_GetOpaque(this_val, jsf_pid_class_id);
    if (!pctx) return GF_JS_EXCEPTION(ctx);
    gf_filter_pid_discard_block(pctx->pid);
	return JS_UNDEFINED;
}
static JSValue jsf_pid_allow_direct_dispatch(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSPidCtx *pctx = JS_GetOpaque(this_val, jsf_pid_class_id);
    if (!pctx) return GF_JS_EXCEPTION(ctx);
    gf_filter_pid_allow_direct_dispatch(pctx->pid);
	return JS_UNDEFINED;
}
static JSValue jsf_pid_resolve_file_template(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 fileidx=0;
	GF_Err e;
	char szFinal[GF_MAX_PATH];
	const char *templ, *suffix=NULL;
	GF_JSPidCtx *pctx = JS_GetOpaque(this_val, jsf_pid_class_id);
    if (!pctx || !argc) return GF_JS_EXCEPTION(ctx);

 	templ = JS_ToCString(ctx, argv[0]);
 	if (!templ)
    	return GF_JS_EXCEPTION(ctx);

	if ((argc>=2) && JS_ToInt32(ctx, &fileidx, argv[1])) {
		JS_FreeCString(ctx, templ);
    	return GF_JS_EXCEPTION(ctx);
	}

	if (argc==3)
 		suffix = JS_ToCString(ctx, argv[2]);

	e = gf_filter_pid_resolve_file_template(pctx->pid, (char *)templ, szFinal, fileidx, suffix);
	JS_FreeCString(ctx, templ);

	if (e)
    	return js_throw_err(ctx, e);
	return JS_NewString(ctx, szFinal);
}

static JSValue jsf_pid_query_caps(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *name=NULL;
	const GF_PropertyValue *prop;
	GF_JSPidCtx *pctx = JS_GetOpaque(this_val, jsf_pid_class_id);
    if (!pctx || !argc) return GF_JS_EXCEPTION(ctx);

	name = JS_ToCString(ctx, argv[0]);
	if (!name) return GF_JS_EXCEPTION(ctx);

	if ((argc>1) && JS_ToBool(ctx, argv[1])) {
		JSValue res;
		prop = gf_filter_pid_caps_query_str(pctx->pid, name);
		JS_FreeCString(ctx, name);
		if (!prop) return JS_NULL;
		res = jsf_NewProp(ctx, prop);
		JS_SetPropertyStr(ctx, res, "type", JS_NewInt32(ctx, prop->type));
	} else {
		u32 p4cc = gf_props_get_id(name);
		JS_FreeCString(ctx, name);
		if (!p4cc)
			return js_throw_err(ctx, GF_BAD_PARAM);

		prop = gf_filter_pid_caps_query(pctx->pid, p4cc);
		if (!prop) return JS_NULL;
		return jsf_NewPropTranslate(ctx, prop, p4cc);
	}
	return JS_UNDEFINED;
}

static JSValue jsf_pid_get_statistics(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JSValue res;
	u32 mode;
	GF_Err e;
	GF_FilterPidStatistics stats;
	GF_JSPidCtx *pctx = JS_GetOpaque(this_val, jsf_pid_class_id);
    if (!pctx || !argc) return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt32(ctx, &mode, argv[0]))
		return GF_JS_EXCEPTION(ctx);

	e = gf_filter_pid_get_statistics(pctx->pid, &stats, mode);
	if (e)
    	return js_throw_err(ctx, e);

	res = JS_NewObject(ctx);
#define SET_PROP32(_val)\
    JS_SetPropertyStr(ctx, res, #_val, JS_NewInt32(ctx, stats._val));
#define SET_PROP64(_val)\
    JS_SetPropertyStr(ctx, res, #_val, JS_NewInt64(ctx, stats._val));
#define SET_PROPB(_val)\
    JS_SetPropertyStr(ctx, res, #_val, JS_NewBool(ctx, stats._val));

	SET_PROPB(disconnected)
	SET_PROP32(average_process_rate)
	SET_PROP32(max_process_rate)
	SET_PROP32(average_bitrate)
	SET_PROP32(max_bitrate)
	SET_PROP32(nb_processed)
	SET_PROP32(max_process_time)
	SET_PROP64(total_process_time)
	SET_PROP64(first_process_time)
	SET_PROP64(last_process_time)
	SET_PROP32(min_frame_dur)
	SET_PROP32(nb_saps)
	SET_PROP32(max_sap_process_time)
	SET_PROP64(total_sap_process_time)
	SET_PROP64(max_buffer_time)
	SET_PROP64(max_playout_time)
	SET_PROP64(min_playout_time)
	SET_PROP64(buffer_time)
	SET_PROP32(nb_buffer_units)
    return res;
}

void jsf_pck_shared_del(GF_Filter *filter, GF_FilterPid *PID, GF_FilterPacket *pck)
{
	u32 i, count;
	GF_JSPidCtx *pctx = gf_filter_pid_get_udta(PID);

	gf_js_lock(pctx->jsf->ctx, GF_TRUE);

	count = gf_list_count(pctx->shared_pck);
	for (i=0; i<count; i++) {
		GF_JSPckCtx *pckc = gf_list_get(pctx->shared_pck, i);
		if (pckc->pck == pck) {
			if (!JS_IsUndefined(pckc->cbck_val)) {
				JSValue args[2];
				args[0] = JS_DupValue(pctx->jsf->ctx, pctx->jsobj);
				args[1] = JS_DupValue(pctx->jsf->ctx, pckc->jsobj);
				JSValue res = JS_Call(pctx->jsf->ctx, pckc->cbck_val, pctx->jsobj, 1, args);
				JS_FreeValue(pctx->jsf->ctx, args[0]);
				JS_FreeValue(pctx->jsf->ctx, args[1]);

				JS_FreeValue(pctx->jsf->ctx, res);
				JS_FreeValue(pctx->jsf->ctx, pckc->cbck_val);
				JS_FreeValue(pctx->jsf->ctx, pckc->jsobj);
				pckc->cbck_val = JS_UNDEFINED;
			}
			JS_FreeValue(pctx->jsf->ctx, pckc->ref_val);
			pckc->ref_val = JS_UNDEFINED;
			jsf_pck_detach_ab(pctx->jsf->ctx, pckc);
			memset(pckc, 0, sizeof(GF_JSPckCtx));
			gf_list_add(pctx->jsf->pck_res, pckc);
			gf_list_rem(pctx->shared_pck, i);
			break;
		}
	}
	gf_js_lock(pctx->jsf->ctx, GF_FALSE);
}

typedef struct
{
	GF_JSPidCtx *pctx;
	GF_JSPckCtx *pck_ctx;
	JSValue fun;
	GF_FilterFrameInterface f_ifce;
} JSGLFIfce;

static void jsf_pck_gl_ifce_del(GF_Filter *filter, GF_FilterPid *PID, GF_FilterPacket *pck)
{
	GF_JSPidCtx *pctx = gf_filter_pid_get_udta(PID);
	gf_js_lock(pctx->jsf->ctx, GF_TRUE);
	GF_FilterFrameInterface *f_ifce = gf_filter_pck_get_frame_interface(pck);
	if (f_ifce) {
		JSGLFIfce *jsgl = f_ifce->user_data;
		JS_FreeValue(pctx->jsf->ctx, jsgl->fun);
		JS_FreeValue(pctx->jsf->ctx, jsgl->pck_ctx->jsobj);
		gf_free(jsgl);
	}
	gf_js_lock(pctx->jsf->ctx, GF_FALSE);

	jsf_pck_shared_del(filter, PID, pck);
}

#if !defined(GPAC_DISABLE_3D) && !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)
Bool wgl_texture_get_id(JSContext *ctx, JSValueConst txval, u32 *tx_id);
#endif

static GF_Err jsf_pck_gl_get_texture(GF_FilterFrameInterface *frame, u32 plane_idx, u32 *gl_tex_format, u32 *gl_tex_id, GF_Matrix_unexposed * texcoordmatrix)
{
	JSValue arg[3], ret, v;
	GF_Err e = GF_OK;
	JSGLFIfce *jsgl = frame->user_data;
	JSContext *ctx = jsgl->pctx->jsf->ctx;
	if (!gl_tex_format || !gl_tex_id)
		return GF_NOT_SUPPORTED;

	gf_js_lock(ctx, GF_TRUE);

	arg[0] = JS_DupValue(ctx, jsgl->pctx->jsobj);
	arg[1] = JS_DupValue(ctx, jsgl->pck_ctx->jsobj);
	arg[2] = JS_NewInt32(ctx, plane_idx);
	ret = JS_Call(ctx, jsgl->fun, jsgl->pctx->jsobj, 3, arg);
	if (JS_IsException(ret)) {
		js_dump_error(ctx);
		e = GF_NOT_SUPPORTED;
	} else if (JS_IsNull(ret) || !JS_IsObject(ret)) {
		e = GF_NOT_SUPPORTED;
	} else {
		v = JS_GetPropertyStr(ctx, ret, "id");
		if (JS_IsObject(v)) {
#if !defined(GPAC_DISABLE_3D) && !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)
			if (! wgl_texture_get_id(ctx, v, gl_tex_id))
#endif
				e = GF_NOT_SUPPORTED;
		} else if (JS_ToInt32(ctx, gl_tex_id, v)) {
			e = GF_NOT_SUPPORTED;
		}
		JS_FreeValue(ctx, v);
		v = JS_GetPropertyStr(ctx, ret, "fmt");
		if (JS_ToInt32(ctx, gl_tex_format, v))
			e = GF_NOT_SUPPORTED;
		JS_FreeValue(ctx, v);

		//todo matrix
	}
	JS_FreeValue(ctx, arg[0]);
	JS_FreeValue(ctx, arg[1]);
	JS_FreeValue(ctx, ret);
	gf_js_lock(ctx, GF_FALSE);
	return e;
}


#if !defined(GPAC_DISABLE_3D) && !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)
JSValue webgl_get_frame_interface(JSContext *ctx, int argc, JSValueConst *argv, GF_FilterPid *for_pid, gf_fsess_packet_destructor *pck_del, GF_FilterFrameInterface **f_ifce);
#endif

static JSValue jsf_pid_new_packet(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	size_t ab_size;
	u8 *data, *ab_data;
	JSValue obj;
	Bool use_shared=GF_FALSE;
	Bool use_gl_ifce=GF_FALSE;
	GF_JSPckCtx *pckc;
	GF_JSPidCtx *pctx = JS_GetOpaque(this_val, jsf_pid_class_id);

    if (!pctx) return GF_JS_EXCEPTION(ctx);
	if (!pctx->jsf->filter->in_process)
		return js_throw_err_msg(ctx, GF_BAD_PARAM, "Filter %s attempt to create a new packet outside process callback not allowed!\n", pctx->jsf->filter->name);


	pckc = gf_list_pop_back(pctx->jsf->pck_res);
	if (!pckc) {
		GF_SAFEALLOC(pckc, GF_JSPckCtx);
		if (!pckc)
			return js_throw_err(ctx, GF_OUT_OF_MEM);
	}
	obj = JS_NewObjectClass(ctx, jsf_pck_class_id);
    if (JS_IsException(obj)) {
    	gf_list_add(pctx->jsf->pck_res, pckc);
		return GF_JS_EXCEPTION(ctx);
	}
	JS_SetOpaque(obj, pckc);
	pckc->jspid = pctx;
	pckc->jsobj = obj;
	pckc->cbck_val = JS_UNDEFINED;
	pckc->ref_val = JS_UNDEFINED;
	pckc->data_ab = JS_UNDEFINED;

	if (argc>1) {
		if (JS_IsFunction(ctx, argv[0])) {
			use_gl_ifce = GF_TRUE;
		} else {
			use_shared = JS_ToBool(ctx, argv[1]);
		}
	}

	if (!argc) {
		pckc->pck = gf_filter_pck_new_alloc(pctx->pid, 0, NULL);
		goto pck_done;
	}
	/*string or true alloc*/
	if (JS_IsString(argv[0]) || JS_IsInteger(argv[0]) ) {
		u32 len;
		const char *str = NULL;
		if (JS_IsInteger(argv[0]) ) {
			if (use_shared)
				return js_throw_err(ctx, GF_BAD_PARAM);
			if (JS_ToInt32(ctx, &len, argv[0])) {
    			gf_list_add(pctx->jsf->pck_res, pckc);
				return GF_JS_EXCEPTION(ctx);
			}
		} else {
 			str = JS_ToCString(ctx, argv[0]);
			len = (u32) strlen(str);
		}
		if (use_shared) {
			pckc->pck = gf_filter_pck_new_shared(pctx->pid, str, len, jsf_pck_shared_del);
			pckc->ref_val = JS_DupValue(ctx, argv[0]);
			JS_FreeCString(ctx, str);
			if (!pctx->shared_pck) pctx->shared_pck = gf_list_new();
			gf_list_add(pctx->shared_pck, pckc);
			pckc->flags = GF_JS_PCK_IS_SHARED;
			if ((argc>2) && JS_IsFunction(ctx, argv[2])) {
				pckc->cbck_val = JS_DupValue(ctx, argv[2]);
				//dup packet obj, it must be kept alive until destructor is called
				pckc->jsobj = JS_DupValue(ctx, pckc->jsobj);
			}
		} else {
			u8 *pdata=NULL;
			pckc->pck = gf_filter_pck_new_alloc(pctx->pid, len, &pdata);
			if (str) {
				memcpy(pdata, str, len);
				JS_FreeCString(ctx, str);
			}
		}
		goto pck_done;
	}
	//check packet reference
	GF_JSPckCtx *pckc_ref = JS_GetOpaque(argv[0], jsf_pck_class_id);
	if (pckc_ref) {
		if (use_shared) {
			pckc->pck = gf_filter_pck_new_ref(pctx->pid, 0, 0, pckc_ref->pck);
			if ((argc>2) && JS_IsFunction(ctx, argv[2]))
				pckc->cbck_val = JS_DupValue(ctx, argv[2]);
		} else {
			u8 *new_data;
			if ((argc>2) && JS_ToBool(ctx, argv[2])) {
				pckc->pck = gf_filter_pck_new_copy(pctx->pid, pckc_ref->pck, &new_data);
			} else {
				pckc->pck = gf_filter_pck_new_clone(pctx->pid, pckc_ref->pck, &new_data);
			}
		}
		goto pck_done;
	}
	//check array buffer

	if (!JS_IsObject(argv[0])) {
		JS_FreeValue(ctx, obj);
		return GF_JS_EXCEPTION(ctx);
	}
	ab_data = JS_IsArrayBuffer(ctx, argv[0]) ? JS_GetArrayBuffer(ctx, &ab_size, argv[0]) : NULL;
	//this is an array buffer
	if (ab_data) {
		if (use_shared) {
			pckc->pck = gf_filter_pck_new_shared(pctx->pid, ab_data, (u32) ab_size, jsf_pck_shared_del);
			pckc->ref_val = JS_DupValue(ctx, argv[0]);
			if (!pctx->shared_pck) pctx->shared_pck = gf_list_new();
			gf_list_add(pctx->shared_pck, pckc);
			pckc->flags = GF_JS_PCK_IS_SHARED;
			if ((argc>2) && JS_IsFunction(ctx, argv[2])) {
				pckc->cbck_val = JS_DupValue(ctx, argv[2]);
				//dup packet obj, it must be kept alive until destructor is called
				pckc->jsobj = JS_DupValue(ctx, pckc->jsobj);
			}
		} else {
			pckc->pck = gf_filter_pck_new_alloc(pctx->pid, (u32) ab_size, &data);
			if (!data) {
				JS_FreeValue(ctx, obj);
				return js_throw_err(ctx, GF_OUT_OF_MEM);
			}
			memcpy(data, ab_data, ab_size);
		}
		goto pck_done;
	}
	/*try GL interface*/
	if (use_gl_ifce) {
		JSGLFIfce *gl_ifce;
		GF_SAFEALLOC(gl_ifce, JSGLFIfce);
		gl_ifce->pctx = pctx;
		gl_ifce->pck_ctx = pckc;
		gl_ifce->fun = JS_DupValue(ctx, argv[0] );
		gl_ifce->f_ifce.user_data = gl_ifce;
		gl_ifce->f_ifce.get_gl_texture = jsf_pck_gl_get_texture;
		gl_ifce->f_ifce.flags = GF_FRAME_IFCE_BLOCKING;
		pckc->pck = gf_filter_pck_new_frame_interface(pctx->pid, &gl_ifce->f_ifce, jsf_pck_gl_ifce_del);
		if (!pctx->shared_pck) pctx->shared_pck = gf_list_new();
		gf_list_add(pctx->shared_pck, pckc);
		//dup packet obj since se need it in the fetch texture callback
		pckc->jsobj = JS_DupValue(ctx, pckc->jsobj);
		if ((argc>1) && JS_IsFunction(ctx, argv[1])) {
			pckc->cbck_val = JS_DupValue(ctx, argv[1]);
			//dup packet obj, it must be kept alive until destructor is called
			pckc->jsobj = JS_DupValue(ctx, pckc->jsobj);
			if ((argc>2) && JS_ToBool(ctx, argv[2])) {
				pckc->flags = GF_JS_PCK_IS_SHARED;
			}
		}
		if ((argc>1) && JS_IsBool(argv[1]) && JS_ToBool(ctx, argv[1])) {
			pckc->flags = GF_JS_PCK_IS_SHARED;
		}
		goto pck_done;
	}
	/*try WebGL canvas*/
#if !defined(GPAC_DISABLE_3D) && !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)
	gf_fsess_packet_destructor pck_del = NULL;
	GF_FilterFrameInterface *f_ifce = NULL;
	JSValue res = webgl_get_frame_interface(ctx, argc, argv, pctx->pid, &pck_del, &f_ifce);
	if (!JS_IsNull(res)) {
		if (JS_IsException(res)) {
			JS_FreeValue(ctx, obj);
			return res;
		}
		assert(f_ifce);
		pckc->pck = gf_filter_pck_new_frame_interface(pctx->pid, f_ifce, pck_del);
		goto pck_done;
	}
#endif

	JS_FreeValue(ctx, obj);
	return GF_JS_EXCEPTION(ctx);


pck_done:
	if (!pckc->pck) {
		JS_FreeValue(ctx, obj);
		return js_throw_err(ctx, GF_OUT_OF_MEM);
	}
	pckc->flags |= GF_JS_PCK_IS_OUTPUT;
	return obj;
}

static JSValue jsf_pid_get_clock_info(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 timescale;
	u64 val;
	GF_FilterClockType cktype;
	JSValue res;
	GF_JSPidCtx *pctx = JS_GetOpaque(this_val, jsf_pid_class_id);
    if (!pctx) return GF_JS_EXCEPTION(ctx);
    cktype = gf_filter_pid_get_clock_info(pctx->pid, &val, &timescale);
	res = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, res, "type", JS_NewInt32(ctx, cktype));
    JS_SetPropertyStr(ctx, res, "timescale", JS_NewInt32(ctx, timescale));
    JS_SetPropertyStr(ctx, res, "value", JS_NewInt64(ctx, val));
	return res;
}


static JSValue jsf_pid_set_property_ex(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, u32 mode)
{
	GF_Err e;
	GF_PropertyValue prop;
	const GF_PropertyValue *the_prop = NULL;
	const char *name=NULL;
	GF_JSPidCtx *pctx = JS_GetOpaque(this_val, jsf_pid_class_id);
    if (!pctx) return GF_JS_EXCEPTION(ctx);
    name = JS_ToCString(ctx, argv[0]);
	if (!name) return GF_JS_EXCEPTION(ctx);

	if ((argc>2) && JS_ToBool(ctx, argv[2])) {
		if (!JS_IsNull(argv[1])) {
			e = jsf_ToProp(pctx->jsf->filter, ctx, argv[1], 0, &prop);
			JS_FreeCString(ctx, name);
			if (e)
				return js_throw_err(ctx, e);
			the_prop = &prop;
		}
		if (mode==1) {
			e = gf_filter_pid_set_info_dyn(pctx->pid, (char *) name, &prop);
		} else if (mode==2) {
			e = gf_filter_pid_negociate_property_dyn(pctx->pid, (char *) name, &prop);
		} else {
			e = gf_filter_pid_set_property_dyn(pctx->pid, (char *) name, &prop);
		}
	} else {
		u32 p4cc = gf_props_get_id(name);
		JS_FreeCString(ctx, name);
		if (!p4cc) return GF_JS_EXCEPTION(ctx);
		if (!JS_IsNull(argv[1])) {
			e = jsf_ToProp(pctx->jsf->filter, ctx, argv[1], p4cc, &prop);
			if (e)
				return js_throw_err(ctx, e);

			the_prop = &prop;
		}
		if (mode==1) {
			e = gf_filter_pid_set_info(pctx->pid, p4cc, the_prop);
		} else if (mode==2) {
			e = gf_filter_pid_negociate_property(pctx->pid, p4cc, the_prop);
		} else {
			e = gf_filter_pid_set_property(pctx->pid, p4cc, the_prop);
		}
	}

	if (the_prop)
		gf_props_reset_single(&prop);
	if (e) return js_throw_err(ctx, e);
    return JS_UNDEFINED;
}
static JSValue jsf_pid_set_property(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return jsf_pid_set_property_ex(ctx, this_val, argc, argv, 0);
}
static JSValue jsf_pid_set_info(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return jsf_pid_set_property_ex(ctx, this_val, argc, argv, 1);
}
static JSValue jsf_pid_negociate_prop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return jsf_pid_set_property_ex(ctx, this_val, argc, argv, 2);
}

static JSValue jsf_pid_ignore_blocking(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Bool do_ignore = GF_TRUE;
	GF_JSPidCtx *pctx = JS_GetOpaque(this_val, jsf_pid_class_id);
    if (!pctx) return GF_JS_EXCEPTION(ctx);
    if (argc) do_ignore = JS_ToBool(ctx, argv[0]) ? GF_TRUE : GF_FALSE;
    gf_filter_pid_ignore_blocking(pctx->pid, do_ignore);
	return JS_UNDEFINED;

}

static JSValue jsf_pid_remove(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSPidCtx *pctx = JS_GetOpaque(this_val, jsf_pid_class_id);
    if (!pctx) return GF_JS_EXCEPTION(ctx);
    if (pctx->pid) {
		gf_filter_pid_remove(pctx->pid);
		pctx->pid = NULL;
    }
    JS_SetOpaque(this_val, NULL);
	return JS_UNDEFINED;
}

static JSValue jsf_pid_reset_props(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Err e;
	GF_JSPidCtx *pctx = JS_GetOpaque(this_val, jsf_pid_class_id);
    if (!pctx) return GF_JS_EXCEPTION(ctx);
    e = gf_filter_pid_reset_properties(pctx->pid);
	if (e) return js_throw_err(ctx, e);
	return JS_UNDEFINED;
}

static JSValue jsf_pid_copy_props(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Err e;
	GF_JSPidCtx *pctx_this = JS_GetOpaque(this_val, jsf_pid_class_id);
    if (!pctx_this || !argc) return GF_JS_EXCEPTION(ctx);
	GF_JSPidCtx *pctx_from = JS_GetOpaque(argv[0], jsf_pid_class_id);
    if (!pctx_from) return GF_JS_EXCEPTION(ctx);
    e = gf_filter_pid_copy_properties(pctx_this->pid, pctx_from->pid);
	if (e) return js_throw_err(ctx, e);
    return JS_UNDEFINED;
}

static JSValue jsf_pid_forward(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Err e;
	GF_JSPckCtx *pckc;
	GF_JSPidCtx *pctx = JS_GetOpaque(this_val, jsf_pid_class_id);
    if (!pctx || !argc) return GF_JS_EXCEPTION(ctx);
    if (!JS_IsObject(argv[0])) return GF_JS_EXCEPTION(ctx);
	pckc = JS_GetOpaque(argv[0], jsf_pck_class_id);
    if (!pckc || !pckc->pck) return GF_JS_EXCEPTION(ctx);
    e = gf_filter_pck_forward(pckc->pck, pctx->pid);
	if (e) return js_throw_err(ctx, e);
    return JS_UNDEFINED;
}


static const JSCFunctionListEntry jsf_pid_funcs[] = {
    JS_CGETSET_MAGIC_DEF("name", jsf_pid_get_prop, jsf_pid_set_prop, JSF_PID_NAME),
    JS_CGETSET_MAGIC_DEF("eos", jsf_pid_get_prop, jsf_pid_set_prop, JSF_PID_EOS),
    JS_CGETSET_MAGIC_DEF("eos_seen", jsf_pid_get_prop, NULL, JSF_PID_EOS_SEEN),
    JS_CGETSET_MAGIC_DEF("eos_received", jsf_pid_get_prop, NULL, JSF_PID_EOS_RECEIVED),
    JS_CGETSET_MAGIC_DEF("would_block", jsf_pid_get_prop, NULL, JSF_PID_WOULD_BLOCK),
    JS_CGETSET_MAGIC_DEF("sparse", jsf_pid_get_prop, NULL, JSF_PID_SPARSE),
    JS_CGETSET_MAGIC_DEF("filter_name", jsf_pid_get_prop, NULL, JSF_PID_FILTER_NAME),
    JS_CGETSET_MAGIC_DEF("src_name", jsf_pid_get_prop, NULL, JSF_PID_FILTER_SRC),
    JS_CGETSET_MAGIC_DEF("args", jsf_pid_get_prop, NULL, JSF_PID_FILTER_ARGS),
    JS_CGETSET_MAGIC_DEF("src_args", jsf_pid_get_prop, NULL, JSF_PID_FILTER_SRC_ARGS),
    JS_CGETSET_MAGIC_DEF("unicity_args", jsf_pid_get_prop, NULL, JSF_PID_FILTER_UNICITY_ARGS),
    JS_CGETSET_MAGIC_DEF("max_buffer", jsf_pid_get_prop, jsf_pid_set_prop, JSF_PID_MAX_BUFFER),
    JS_CGETSET_MAGIC_DEF("loose_connect", NULL, jsf_pid_set_prop, JSF_PID_LOOSE_CONNECT),
    JS_CGETSET_MAGIC_DEF("framing", NULL, jsf_pid_set_prop, JSF_PID_FRAMING_MODE),
    JS_CGETSET_MAGIC_DEF("buffer", jsf_pid_get_prop, NULL, JSF_PID_BUFFER),
    JS_CGETSET_MAGIC_DEF("full", jsf_pid_get_prop, NULL, JSF_PID_IS_FULL),
    JS_CGETSET_MAGIC_DEF("first_empty", jsf_pid_get_prop, NULL, JSF_PID_FIRST_EMPTY),
    JS_CGETSET_MAGIC_DEF("first_cts", jsf_pid_get_prop, NULL, JSF_PID_FIRST_CTS),
    JS_CGETSET_MAGIC_DEF("nb_pck_queued", jsf_pid_get_prop, NULL, JSF_PID_NB_PACKETS),
    JS_CGETSET_MAGIC_DEF("timescale", jsf_pid_get_prop, NULL, JSF_PID_TIMESCALE),
    JS_CGETSET_MAGIC_DEF("clock_mode", NULL, jsf_pid_set_prop, JSF_PID_CLOCK_MODE),
    JS_CGETSET_MAGIC_DEF("discard", NULL, jsf_pid_set_prop, JSF_PID_DISCARD),
    JS_CGETSET_MAGIC_DEF("src_url", jsf_pid_get_prop, NULL, JSF_PID_SRC_URL),
    JS_CGETSET_MAGIC_DEF("dst_url", jsf_pid_get_prop, NULL, JSF_PID_DST_URL),
    JS_CGETSET_MAGIC_DEF("require_source_id", NULL, jsf_pid_set_prop, JSF_PID_REQUIRE_SOURCEID),
    JS_CGETSET_MAGIC_DEF("recompute_dts", NULL, jsf_pid_set_prop, JSF_PID_RECOMPUTE_DTS),
    JS_CGETSET_MAGIC_DEF("min_pck_dur", jsf_pid_get_prop, NULL, JSF_PID_MIN_PCK_DUR),
    JS_CGETSET_MAGIC_DEF("playing", jsf_pid_get_prop, NULL, JSF_PID_IS_PLAYING),
    JS_CGETSET_MAGIC_DEF("next_ts", jsf_pid_get_prop, NULL, JSF_PID_NEXT_TS),
    JS_CGETSET_MAGIC_DEF("has_decoder", jsf_pid_get_prop, NULL, JSF_PID_HAS_DECODER),
    JS_CFUNC_DEF("send_event", 0, jsf_pid_send_event),
    JS_CFUNC_DEF("enum_properties", 0, jsf_pid_enum_properties),
    JS_CFUNC_DEF("get_prop", 0, jsf_pid_get_property),
    JS_CFUNC_DEF("get_info", 0, jsf_pid_get_info),
    JS_CFUNC_DEF("get_packet", 0, jsf_pid_get_packet),
    JS_CFUNC_DEF("drop_packet", 0, jsf_pid_drop_packet),
    JS_CFUNC_DEF("is_filter_in_parents", 0, jsf_pid_is_filter_in_parents),
    JS_CFUNC_DEF("get_buffer_occupancy", 0, jsf_pid_get_buffer_occupancy),
    JS_CFUNC_DEF("clear_eos", 0, jsf_pid_clear_eos),
    JS_CFUNC_DEF("check_caps", 0, jsf_pid_check_caps),
    JS_CFUNC_DEF("discard_block", 0, jsf_pid_discard_block),
    JS_CFUNC_DEF("allow_direct_dispatch", 0, jsf_pid_allow_direct_dispatch),
    JS_CFUNC_DEF("resolve_file_template", 0, jsf_pid_resolve_file_template),
    JS_CFUNC_DEF("query_caps", 0, jsf_pid_query_caps),
    JS_CFUNC_DEF("get_stats", 0, jsf_pid_get_statistics),
    JS_CFUNC_DEF("get_clock_info", 0, jsf_pid_get_clock_info),
    JS_CFUNC_DEF("set_prop", 0, jsf_pid_set_property),
    JS_CFUNC_DEF("set_info", 0, jsf_pid_set_info),
    JS_CFUNC_DEF("new_packet", 0, jsf_pid_new_packet),
    JS_CFUNC_DEF("remove", 0, jsf_pid_remove),
    JS_CFUNC_DEF("reset_props", 0, jsf_pid_reset_props),
    JS_CFUNC_DEF("copy_props", 0, jsf_pid_copy_props),
    JS_CFUNC_DEF("forward", 0, jsf_pid_forward),
    JS_CFUNC_DEF("negociate_prop", 0, jsf_pid_negociate_prop),
    JS_CFUNC_DEF("ignore_blocking", 0, jsf_pid_ignore_blocking),
};

enum
{
	JSF_EVENT_TYPE,
	JSF_EVENT_NAME,
	/*PLAY event*/
	JSF_EVENT_START_RANGE,
	JSF_EVENT_SPEED,
	JSF_EVENT_HW_BUFFER_RESET,
	JSF_EVENT_INITIAL_BROADCAST_PLAY,
	JSF_EVENT_TIMESTAMP_BASED,
	JSF_EVENT_FULL_FILE_ONLY,
	JSF_EVENT_FORCE_DASH_SEG_SWITCH,
	JSF_EVENT_FROM_PCK,
	/*source switch*/
	JSF_EVENT_START_OFFSET,
	JSF_EVENT_END_OFFSET,
	JSF_EVENT_SOURCE_SWITCH,
	JSF_EVENT_SKIP_CACHE_EXPIRATION,
	JSF_EVENT_HINT_BLOCK_SIZE,
	/*segment size*/
	JSF_EVENT_SEG_URL,
	JSF_EVENT_SEG_IS_INIT,
	JSF_EVENT_MEDIA_START_RANGE,
	JSF_EVENT_MEDIA_END_RANGE,
	JSF_EVENT_IDX_START_RANGE,
	JSF_EVENT_IDX_END_RANGE,
	/*quality switch*/
	JSF_EVENT_SWITCH_UP,
	JSF_EVENT_SWITCH_GROUP_IDX,
	JSF_EVENT_SWITCH_QUALITY_IDX,
	JSF_EVENT_SWITCH_TILE_MODE,
	JSF_EVENT_SWITCH_QUALITY_DEGRADATION,
	/*visibility hint*/
	JSF_EVENT_VIS_MIN_X,
	JSF_EVENT_VIS_MIN_Y,
	JSF_EVENT_VIS_MAX_X,
	JSF_EVENT_VIS_MAX_Y,
	JSF_EVENT_VIS_IS_GAZE,
	/*buffer requirements*/
	JSF_EVENT_BUFREQ_MAX_BUFFER_US,
	JSF_EVENT_BUFREQ_MAX_PLAYOUT_US,
	JSF_EVENT_BUFREQ_MIN_PLAYOUT_US,
	JSF_EVENT_BUFREQ_PID_ONLY,

	JSF_EVENT_USER_TYPE,
	JSF_EVENT_USER_KEYCODE,
	JSF_EVENT_USER_KEYNAME,
	JSF_EVENT_USER_KEYMODS,
	JSF_EVENT_USER_TEXT_CHAR,
	JSF_EVENT_USER_MOUSE_X,
	JSF_EVENT_USER_MOUSE_Y,
	JSF_EVENT_USER_WHEEL,
	JSF_EVENT_USER_BUTTON,
	JSF_EVENT_USER_HWKEY,
	JSF_EVENT_USER_DROPFILES,
	JSF_EVENT_USER_TEXT,
	JSF_EVENT_USER_MT_ROTATION,
	JSF_EVENT_USER_MT_PINCH,
	JSF_EVENT_USER_MT_FINGERS,
	JSF_EVENT_USER_WIDTH,
	JSF_EVENT_USER_HEIGHT,
	JSF_EVENT_USER_SHOWTYPE,
	JSF_EVENT_USER_MOVE_X,
	JSF_EVENT_USER_MOVE_Y,
	JSF_EVENT_USER_MOVE_RELATIVE,
	JSF_EVENT_USER_MOVE_ALIGN_X,
	JSF_EVENT_USER_MOVE_ALIGN_Y,
	JSF_EVENT_USER_CAPTION,
};

static Bool jsf_check_evt(u32 evt_type, u8 ui_type, int magic)
{
	if (magic==JSF_EVENT_TYPE) return GF_TRUE;
	if (magic==JSF_EVENT_NAME) return GF_TRUE;
	switch (evt_type) {
	case GF_FEVT_PLAY:
		switch (magic) {
		case JSF_EVENT_START_RANGE:
		case JSF_EVENT_SPEED:
		case JSF_EVENT_HW_BUFFER_RESET:
		case JSF_EVENT_INITIAL_BROADCAST_PLAY:
		case JSF_EVENT_TIMESTAMP_BASED:
		case JSF_EVENT_FULL_FILE_ONLY:
		case JSF_EVENT_FORCE_DASH_SEG_SWITCH:
		case JSF_EVENT_FROM_PCK:
			return GF_TRUE;
		default:
			return GF_FALSE;
		}
		break;
	case GF_FEVT_SET_SPEED:
		return (magic == JSF_EVENT_SPEED) ? GF_TRUE : GF_FALSE;
	case GF_FEVT_SOURCE_SWITCH:
		switch (magic) {
		case JSF_EVENT_START_OFFSET:
		case JSF_EVENT_END_OFFSET:
		case JSF_EVENT_SOURCE_SWITCH:
		case JSF_EVENT_SKIP_CACHE_EXPIRATION:
		case JSF_EVENT_HINT_BLOCK_SIZE:
			return GF_TRUE;
		default:
			return GF_FALSE;
		}
		break;
	case GF_FEVT_SEGMENT_SIZE:
		switch (magic) {
		case JSF_EVENT_SEG_URL:
		case JSF_EVENT_SEG_IS_INIT:
		case JSF_EVENT_MEDIA_START_RANGE:
		case JSF_EVENT_MEDIA_END_RANGE:
		case JSF_EVENT_IDX_START_RANGE:
		case JSF_EVENT_IDX_END_RANGE:
			return GF_TRUE;
		default:
			return GF_FALSE;
		}
		break;
	case GF_FEVT_QUALITY_SWITCH:
		switch (magic) {
		case JSF_EVENT_SWITCH_UP:
		case JSF_EVENT_SWITCH_GROUP_IDX:
		case JSF_EVENT_SWITCH_QUALITY_IDX:
		case JSF_EVENT_SWITCH_TILE_MODE:
		case JSF_EVENT_SWITCH_QUALITY_DEGRADATION:
			return GF_TRUE;
		default:
			return GF_FALSE;
		}
		break;
	case GF_FEVT_VISIBILITY_HINT:
		switch (magic) {
		case JSF_EVENT_VIS_MIN_X:
		case JSF_EVENT_VIS_MIN_Y:
		case JSF_EVENT_VIS_MAX_X:
		case JSF_EVENT_VIS_MAX_Y:
		case JSF_EVENT_VIS_IS_GAZE:
			return GF_TRUE;
		default:
			return GF_FALSE;
		}
		break;
	case GF_FEVT_BUFFER_REQ:
		switch (magic) {
		case JSF_EVENT_BUFREQ_MAX_BUFFER_US:
		case JSF_EVENT_BUFREQ_MAX_PLAYOUT_US:
		case JSF_EVENT_BUFREQ_MIN_PLAYOUT_US:
		case JSF_EVENT_BUFREQ_PID_ONLY:
			return GF_TRUE;
		default:
			return GF_FALSE;
		}
		break;
	case GF_FEVT_USER:
		if (magic==JSF_EVENT_USER_TYPE)
			return GF_TRUE;

		switch (ui_type) {
		case GF_EVENT_CLICK:
		case GF_EVENT_MOUSEUP:
		case GF_EVENT_MOUSEDOWN:
		case GF_EVENT_MOUSEOVER:
		case GF_EVENT_MOUSEOUT:
		case GF_EVENT_MOUSEMOVE:
		case GF_EVENT_MOUSEWHEEL:
			switch (magic) {
			case JSF_EVENT_USER_MOUSE_X:
			case JSF_EVENT_USER_MOUSE_Y:
			case JSF_EVENT_USER_WHEEL:
			case JSF_EVENT_USER_BUTTON:
				return GF_TRUE;
			default:
				break;
			}
			return GF_FALSE;
		case GF_EVENT_MULTITOUCH:
			switch (magic) {
			case JSF_EVENT_USER_MOUSE_X:
			case JSF_EVENT_USER_MOUSE_Y:
			case JSF_EVENT_USER_MT_ROTATION:
			case JSF_EVENT_USER_MT_PINCH:
			case JSF_EVENT_USER_MT_FINGERS:
				return GF_TRUE;
			default:
				break;
			}
			return GF_FALSE;

		case GF_EVENT_KEYUP:
		case GF_EVENT_KEYDOWN:
		case GF_EVENT_LONGKEYPRESS:
		case GF_EVENT_TEXTINPUT:
			switch (magic) {
			case JSF_EVENT_USER_KEYCODE:
			case JSF_EVENT_USER_KEYNAME:
			case JSF_EVENT_USER_KEYMODS:
			case JSF_EVENT_USER_HWKEY:
			case JSF_EVENT_USER_TEXT_CHAR:
			case JSF_EVENT_USER_DROPFILES:
				return GF_TRUE;
			default:
				break;
			}
			return GF_FALSE;
		case GF_EVENT_DROPFILE:
			switch (magic) {
			case JSF_EVENT_USER_DROPFILES:
				return GF_TRUE;
			default:
				break;
			}
			return GF_FALSE;
		case GF_EVENT_PASTE_TEXT:
		case GF_EVENT_COPY_TEXT:
			switch (magic) {
			case JSF_EVENT_USER_TEXT:
				return GF_TRUE;
			default:
				break;
			}
			return GF_FALSE;
		case GF_EVENT_SIZE:
			switch (magic) {
			case JSF_EVENT_USER_WIDTH:
			case JSF_EVENT_USER_HEIGHT:
				return GF_TRUE;
			default:
				break;
			}
			return GF_FALSE;
		case GF_EVENT_MOVE:
			switch (magic) {
			case JSF_EVENT_USER_MOVE_X:
			case JSF_EVENT_USER_MOVE_Y:
			case JSF_EVENT_USER_MOVE_RELATIVE:
			case JSF_EVENT_USER_MOVE_ALIGN_X:
			case JSF_EVENT_USER_MOVE_ALIGN_Y:
				return GF_TRUE;
			default:
				break;
			}
			return GF_FALSE;
		case GF_EVENT_SET_CAPTION:
			if (magic==JSF_EVENT_USER_CAPTION) return GF_TRUE;
			return GF_FALSE;
		}
	}
	return GF_FALSE;
}


static JSValue jsf_event_set_prop(JSContext *ctx, JSValueConst this_val, JSValueConst value, int magic)
{
	GF_Err e = GF_OK;
	u32 ival;
	Double dval;
	const char *str=NULL;
	GF_FilterEvent *evt = JS_GetOpaque(this_val, jsf_event_class_id);
    if (!evt) return GF_JS_EXCEPTION(ctx);
	if (!jsf_check_evt(evt->base.type, evt->user_event.event.type, magic))
		return GF_JS_EXCEPTION(ctx);

	switch (magic) {
	case JSF_EVENT_TYPE:
		return GF_JS_EXCEPTION(ctx);
	/*PLAY*/
	case JSF_EVENT_START_RANGE:
		return JS_ToFloat64(ctx, &evt->play.start_range, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;
	case JSF_EVENT_SPEED:
		return JS_ToFloat64(ctx, &evt->play.speed, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;
	case JSF_EVENT_HW_BUFFER_RESET:
		evt->play.hw_buffer_reset = JS_ToBool(ctx, value);
		return JS_UNDEFINED;
	case JSF_EVENT_INITIAL_BROADCAST_PLAY:
		evt->play.initial_broadcast_play = JS_ToBool(ctx, value);
		return JS_UNDEFINED;
	case JSF_EVENT_TIMESTAMP_BASED:
		if (JS_ToInt32(ctx, &ival, value)) return GF_JS_EXCEPTION(ctx);
		evt->play.timestamp_based = (u8) ival;
		return JS_UNDEFINED;
	case JSF_EVENT_FULL_FILE_ONLY:
		evt->play.full_file_only = JS_ToBool(ctx, value);
		return JS_UNDEFINED;
	case JSF_EVENT_FORCE_DASH_SEG_SWITCH:
		evt->play.forced_dash_segment_switch = JS_ToBool(ctx, value);
		return JS_UNDEFINED;
	case JSF_EVENT_FROM_PCK:
		return JS_ToInt32(ctx, &evt->play.from_pck, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;
	/*source switch*/
	case JSF_EVENT_START_OFFSET:
		return JS_ToInt64(ctx, &evt->seek.start_offset, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;
	case JSF_EVENT_END_OFFSET:
		return JS_ToInt64(ctx, &evt->seek.end_offset, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;
	case JSF_EVENT_SOURCE_SWITCH:
		/*TODO check leak!*/
		evt->seek.source_switch = JS_ToCString(ctx, value);
		return JS_UNDEFINED;
	case JSF_EVENT_SKIP_CACHE_EXPIRATION:
		evt->seek.skip_cache_expiration = JS_ToBool(ctx, value);
		return JS_UNDEFINED;
	case JSF_EVENT_HINT_BLOCK_SIZE:
		return JS_ToInt32(ctx, &evt->seek.hint_block_size, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;
	/*segment size*/
	case JSF_EVENT_SEG_URL:
		/*TODO check leak!*/
		evt->seg_size.seg_url = JS_ToCString(ctx, value);
		return JS_UNDEFINED;
	case JSF_EVENT_SEG_IS_INIT:
		evt->seg_size.is_init = JS_ToBool(ctx, value) ? 1 : 0;
		return JS_UNDEFINED;
	case JSF_EVENT_MEDIA_START_RANGE:
		return JS_ToInt64(ctx, &evt->seg_size.media_range_start, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;
	case JSF_EVENT_MEDIA_END_RANGE:
		return JS_ToInt64(ctx, &evt->seg_size.media_range_end, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;
	case JSF_EVENT_IDX_START_RANGE:
		return JS_ToInt64(ctx, &evt->seg_size.idx_range_start, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;
	case JSF_EVENT_IDX_END_RANGE:
		return JS_ToInt64(ctx, &evt->seg_size.idx_range_end, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;
	/*quality switch*/
	case JSF_EVENT_SWITCH_UP:
		evt->quality_switch.up = JS_ToBool(ctx, value);
		return JS_UNDEFINED;
	case JSF_EVENT_SWITCH_GROUP_IDX:
		return JS_ToInt32(ctx, &evt->quality_switch.dependent_group_index, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;
	case JSF_EVENT_SWITCH_QUALITY_IDX:
		return JS_ToInt32(ctx, &evt->quality_switch.q_idx, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;
	case JSF_EVENT_SWITCH_TILE_MODE:
		return JS_ToInt32(ctx, &evt->quality_switch.set_tile_mode_plus_one, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;
	case JSF_EVENT_SWITCH_QUALITY_DEGRADATION:
		return JS_ToInt32(ctx, &evt->quality_switch.quality_degradation, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;
	/*visibility hint*/
	case JSF_EVENT_VIS_MIN_X: return JS_ToInt32(ctx, &evt->visibility_hint.min_x, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;
	case JSF_EVENT_VIS_MIN_Y: return JS_ToInt32(ctx, &evt->visibility_hint.min_y, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;
	case JSF_EVENT_VIS_MAX_X: return JS_ToInt32(ctx, &evt->visibility_hint.max_x, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;
	case JSF_EVENT_VIS_MAX_Y: return JS_ToInt32(ctx, &evt->visibility_hint.max_y, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;
	case JSF_EVENT_VIS_IS_GAZE:
		evt->visibility_hint.is_gaze = JS_ToBool(ctx, value);
		return JS_UNDEFINED;
	/*buffer reqs*/
	case JSF_EVENT_BUFREQ_MAX_BUFFER_US: return JS_ToInt32(ctx, &evt->buffer_req.max_buffer_us, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;
	case JSF_EVENT_BUFREQ_MAX_PLAYOUT_US: return JS_ToInt32(ctx, &evt->buffer_req.max_playout_us, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;
	case JSF_EVENT_BUFREQ_MIN_PLAYOUT_US: return JS_ToInt32(ctx, &evt->buffer_req.min_playout_us, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;
	case JSF_EVENT_BUFREQ_PID_ONLY:
		evt->buffer_req.pid_only = JS_ToBool(ctx, value);
		return JS_UNDEFINED;

	case JSF_EVENT_USER_TYPE:
		if (JS_ToInt32(ctx, &ival, value)) return GF_JS_EXCEPTION(ctx);
		evt->user_event.event.type = (u8) ival;
		return JS_UNDEFINED;


	case JSF_EVENT_USER_MOUSE_X:
		if (evt->user_event.event.type==GF_EVENT_MULTITOUCH) {
			if (JS_ToFloat64(ctx, &dval, value)) return GF_JS_EXCEPTION(ctx);
			evt->user_event.event.mtouch.x = FLT2FIX(dval);
			return JS_UNDEFINED;
		}
		return JS_ToInt32(ctx, &evt->user_event.event.mouse.x, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;
	case JSF_EVENT_USER_MOUSE_Y:
		if (evt->user_event.event.type==GF_EVENT_MULTITOUCH) {
			if (JS_ToFloat64(ctx, &dval, value)) return GF_JS_EXCEPTION(ctx);
			evt->user_event.event.mtouch.y = FLT2FIX(dval);
			return JS_UNDEFINED;
		}
		return JS_ToInt32(ctx, &evt->user_event.event.mouse.y, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;

	case JSF_EVENT_USER_WHEEL:
		if (JS_ToFloat64(ctx, &dval, value)) return GF_JS_EXCEPTION(ctx);
		evt->user_event.event.mouse.wheel_pos = FLT2FIX(dval);
		return JS_UNDEFINED;
	case JSF_EVENT_USER_BUTTON: return JS_ToInt32(ctx, &evt->user_event.event.mouse.button, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;
	case JSF_EVENT_USER_HWKEY: return JS_ToInt32(ctx, &evt->user_event.event.key.hw_code, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;
	case JSF_EVENT_USER_KEYCODE: return JS_ToInt32(ctx, &evt->user_event.event.key.key_code, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;
	case JSF_EVENT_USER_KEYMODS: return JS_ToInt32(ctx, &evt->user_event.event.key.flags, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;

	case JSF_EVENT_USER_MT_ROTATION:
		if (JS_ToFloat64(ctx, &dval, value)) return GF_JS_EXCEPTION(ctx);
		evt->user_event.event.mtouch.rotation = FLT2FIX(dval);
		return JS_UNDEFINED;
	case JSF_EVENT_USER_MT_PINCH:
		if (JS_ToFloat64(ctx, &dval, value)) return GF_JS_EXCEPTION(ctx);
		evt->user_event.event.mtouch.pinch = FLT2FIX(dval);
		return JS_UNDEFINED;
	case JSF_EVENT_USER_MT_FINGERS:
		return JS_ToInt32(ctx, &evt->user_event.event.mtouch.num_fingers, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;

	case JSF_EVENT_USER_TEXT:
	{
		str = JS_ToCString(ctx, value);
		evt->user_event.event.clipboard.text = gf_strdup(str ? str : "");
		if (str) JS_FreeCString(ctx, str);
		return JS_UNDEFINED;
	}

	case JSF_EVENT_USER_WIDTH: return JS_ToInt32(ctx, &evt->user_event.event.size.width, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;
	case JSF_EVENT_USER_HEIGHT: return JS_ToInt32(ctx, &evt->user_event.event.size.height, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;
	case JSF_EVENT_USER_SHOWTYPE: return JS_ToInt32(ctx, &evt->user_event.event.show.show_type, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;
	case JSF_EVENT_USER_MOVE_X: return JS_ToInt32(ctx, &evt->user_event.event.move.x, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;
	case JSF_EVENT_USER_MOVE_Y: return JS_ToInt32(ctx, &evt->user_event.event.move.y, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;
	case JSF_EVENT_USER_MOVE_RELATIVE: return JS_ToInt32(ctx, &evt->user_event.event.move.relative, value) ? GF_JS_EXCEPTION(ctx) : JS_UNDEFINED;
	case JSF_EVENT_USER_MOVE_ALIGN_X:
	case JSF_EVENT_USER_MOVE_ALIGN_Y:
		if (JS_ToInt32(ctx, &ival, value)) return GF_JS_EXCEPTION(ctx);
		if (magic==JSF_EVENT_USER_MOVE_ALIGN_X)
			evt->user_event.event.move.align_x = ival;
		else
			evt->user_event.event.move.align_x = ival;
		return JS_UNDEFINED;

	case JSF_EVENT_USER_CAPTION:
	{
		str = JS_ToCString(ctx, value);
		evt->user_event.event.caption.caption = gf_strdup(str ? str : "");
		if (str) JS_FreeCString(ctx, str);
		return JS_UNDEFINED;
	}
	}

	if (str)
		JS_FreeCString(ctx, str);
	if (e) return js_throw_err(ctx, e);
    return JS_UNDEFINED;
}

static JSValue jsf_event_get_prop(JSContext *ctx, JSValueConst this_val, int magic)
{
	GF_FilterEvent *evt = JS_GetOpaque(this_val, jsf_event_class_id);
    if (!evt) return GF_JS_EXCEPTION(ctx);
	if (!jsf_check_evt(evt->base.type, evt->user_event.event.type, magic))
		return GF_JS_EXCEPTION(ctx);
	switch (magic) {
	case JSF_EVENT_TYPE: return JS_NewInt32(ctx, evt->base.type);
	case JSF_EVENT_NAME:
		if (evt->base.type==GF_FEVT_USER) {
			const char *ename=NULL;
			switch (evt->user_event.event.type) {
			case GF_EVENT_CLICK: ename = "click"; break;
			case GF_EVENT_MOUSEUP: ename = "mouseup"; break;
			case GF_EVENT_MOUSEDOWN: ename = "mousedown"; break;
			case GF_EVENT_MOUSEMOVE: ename = "mousemove"; break;
			case GF_EVENT_MOUSEWHEEL: ename = "mousewheel"; break;
			case GF_EVENT_DBLCLICK: ename = "dblclick"; break;
			case GF_EVENT_MULTITOUCH: ename = "touch"; break;
			case GF_EVENT_KEYUP: ename = "keyup"; break;
			case GF_EVENT_KEYDOWN: ename = "keydown"; break;
			case GF_EVENT_TEXTINPUT: ename = "text"; break;
			case GF_EVENT_DROPFILE: ename = "dropfile"; break;
			case GF_EVENT_TIMESHIFT_DEPTH: ename = "timeshift_depth"; break;
			case GF_EVENT_TIMESHIFT_UPDATE: ename = "timeshift_update"; break;
			case GF_EVENT_TIMESHIFT_OVERFLOW: ename = "timeshift_overflow"; break;
			case GF_EVENT_TIMESHIFT_UNDERRUN: ename = "timeshift_underrun"; break;
			case GF_EVENT_PASTE_TEXT: ename = "paste_text"; break;
			case GF_EVENT_COPY_TEXT: ename = "copy_text"; break;
			case GF_EVENT_SIZE: ename = "size"; break;
			case GF_EVENT_SHOWHIDE: ename = "showhide"; break;
			case GF_EVENT_MOVE: ename = "move"; break;
			case GF_EVENT_SET_CAPTION: ename = "caption"; break;
			case GF_EVENT_REFRESH: ename = "refresh"; break;
			case GF_EVENT_QUIT: ename = "quit"; break;
			case GF_EVENT_VIDEO_SETUP: ename = "video_setup"; break;
			default:
				ename = "unknown"; break;
			}
			return JS_NewString(ctx, ename);
		}
		return JS_NewString(ctx, gf_filter_event_name(evt->base.type));
	/*PLAY*/
	case JSF_EVENT_START_RANGE: return JS_NewFloat64(ctx, evt->play.start_range);
	case JSF_EVENT_SPEED: return JS_NewFloat64(ctx, evt->play.speed);
	case JSF_EVENT_HW_BUFFER_RESET: return JS_NewBool(ctx, evt->play.hw_buffer_reset);
	case JSF_EVENT_INITIAL_BROADCAST_PLAY: return JS_NewBool(ctx, evt->play.initial_broadcast_play);
	case JSF_EVENT_TIMESTAMP_BASED: return JS_NewInt32(ctx, evt->play.timestamp_based);
	case JSF_EVENT_FULL_FILE_ONLY: return JS_NewBool(ctx, evt->play.full_file_only);
	case JSF_EVENT_FORCE_DASH_SEG_SWITCH: return JS_NewBool(ctx, evt->play.forced_dash_segment_switch);
	case JSF_EVENT_FROM_PCK: return JS_NewInt32(ctx, evt->play.from_pck);
	/*source switch*/
	case JSF_EVENT_START_OFFSET: return JS_NewInt64(ctx, evt->seek.start_offset);
	case JSF_EVENT_END_OFFSET: return JS_NewInt64(ctx, evt->seek.end_offset);
	case JSF_EVENT_SOURCE_SWITCH: return JS_NewString(ctx, evt->seek.source_switch);
	case JSF_EVENT_SKIP_CACHE_EXPIRATION: return JS_NewBool(ctx, evt->seek.skip_cache_expiration);
	case JSF_EVENT_HINT_BLOCK_SIZE: return JS_NewInt32(ctx, evt->seek.hint_block_size);
	/*segment size*/
	case JSF_EVENT_SEG_URL: return JS_NewString(ctx, evt->seg_size.seg_url);
	case JSF_EVENT_SEG_IS_INIT: return JS_NewBool(ctx, evt->seg_size.is_init);
	case JSF_EVENT_MEDIA_START_RANGE: return JS_NewInt64(ctx, evt->seg_size.media_range_start);
	case JSF_EVENT_MEDIA_END_RANGE: return JS_NewInt64(ctx, evt->seg_size.media_range_end);
	case JSF_EVENT_IDX_START_RANGE: return JS_NewInt64(ctx, evt->seg_size.media_range_start);
	case JSF_EVENT_IDX_END_RANGE: return JS_NewInt64(ctx, evt->seg_size.idx_range_end);
	/*quality switch*/
	case JSF_EVENT_SWITCH_UP: return JS_NewBool(ctx, evt->quality_switch.up);
	case JSF_EVENT_SWITCH_GROUP_IDX: return JS_NewInt32(ctx, evt->quality_switch.dependent_group_index);
	case JSF_EVENT_SWITCH_QUALITY_IDX: return JS_NewInt32(ctx, evt->quality_switch.q_idx);
	case JSF_EVENT_SWITCH_TILE_MODE: return JS_NewInt32(ctx, evt->quality_switch.set_tile_mode_plus_one);
	case JSF_EVENT_SWITCH_QUALITY_DEGRADATION: return JS_NewInt32(ctx, evt->quality_switch.quality_degradation);
	/*visibility hint*/
	case JSF_EVENT_VIS_MIN_X: return JS_NewInt32(ctx, evt->visibility_hint.min_x);
	case JSF_EVENT_VIS_MIN_Y: return JS_NewInt32(ctx, evt->visibility_hint.min_y);
	case JSF_EVENT_VIS_MAX_X: return JS_NewInt32(ctx, evt->visibility_hint.max_x);
	case JSF_EVENT_VIS_MAX_Y: return JS_NewInt32(ctx, evt->visibility_hint.max_y);
	case JSF_EVENT_VIS_IS_GAZE: return JS_NewBool(ctx, evt->visibility_hint.is_gaze);
	/*buffer reqs*/
	case JSF_EVENT_BUFREQ_MAX_BUFFER_US: return JS_NewInt32(ctx, evt->buffer_req.max_buffer_us);
	case JSF_EVENT_BUFREQ_MAX_PLAYOUT_US: return JS_NewInt32(ctx, evt->buffer_req.max_playout_us);
	case JSF_EVENT_BUFREQ_MIN_PLAYOUT_US: return JS_NewInt32(ctx, evt->buffer_req.min_playout_us);
	case JSF_EVENT_BUFREQ_PID_ONLY: return JS_NewBool(ctx, evt->buffer_req.pid_only);
	/*user event*/
	case JSF_EVENT_USER_TYPE: return JS_NewInt32(ctx, evt->user_event.event.type);
	case JSF_EVENT_USER_KEYCODE: return JS_NewInt32(ctx, evt->user_event.event.key.key_code);
	case JSF_EVENT_USER_KEYMODS:
		return JS_NewInt32(ctx, evt->user_event.event.key.flags);
	case JSF_EVENT_USER_KEYNAME:
#ifndef GPAC_DISABLE_SVG
		return JS_NewString(ctx, gf_dom_get_key_name(evt->user_event.event.key.key_code) );
#else
		return JS_NULL;
#endif

	case JSF_EVENT_USER_TEXT_CHAR:
	{
		u32 unic = evt->user_event.event.character.unicode_char;
		char szSTR[5];
		memset(szSTR, 0, 5);
		if (unic<0x80) {
			szSTR[0] = (char) unic&0xFF;
		} else if (unic<0x0800) {
			szSTR[0] = ((char) (unic>>6)&0x1F) | 0xC0;
			szSTR[1] = ((char) (unic) & 0x3F) | 0x80;
		} else if (unic<0x010000) {
			szSTR[0] = ((char) (unic>>12)&0x0F) | 0xE0;
			szSTR[1] = ((char) (unic>>6) & 0x3F) | 0x80;
			szSTR[2] = ((char) (unic) & 0x3F) | 0x80;
		} else {
			szSTR[0] = ((char) (unic>>24)&0x07) | 0xF0;
			szSTR[1] = ((char) (unic>>12) & 0x3F) | 0x80;
			szSTR[2] = ((char) (unic>>6) & 0x3F) | 0x80;
			szSTR[3] = ((char) (unic) & 0x3F) | 0x80;
		}
		return JS_NewString(ctx, szSTR);
	}

	case JSF_EVENT_USER_MOUSE_X:
		if (evt->user_event.event.type==GF_EVENT_MULTITOUCH)
			return JS_NewFloat64(ctx, FIX2FLT(evt->user_event.event.mtouch.x) );
		return JS_NewInt32(ctx, evt->user_event.event.mouse.x);
	case JSF_EVENT_USER_MOUSE_Y:
		if (evt->user_event.event.type==GF_EVENT_MULTITOUCH)
			return JS_NewFloat64(ctx, FIX2FLT(evt->user_event.event.mtouch.y) );
		return JS_NewInt32(ctx, evt->user_event.event.mouse.y);
	case JSF_EVENT_USER_WHEEL: return JS_NewFloat64(ctx, FIX2FLT(evt->user_event.event.mouse.wheel_pos));
	case JSF_EVENT_USER_BUTTON: return JS_NewInt32(ctx,  evt->user_event.event.mouse.button);
	case JSF_EVENT_USER_HWKEY: return JS_NewInt32(ctx, evt->user_event.event.key.hw_code);
	case JSF_EVENT_USER_DROPFILES:
	{
		u32 i, idx;
		JSValue files_array = JS_NewArray(ctx);
		idx=0;
		for (i=0; i<evt->user_event.event.open_file.nb_files; i++) {
			if (evt->user_event.event.open_file.files[i]) {
				JS_SetPropertyUint32(ctx, files_array, idx, JS_NewString(ctx, evt->user_event.event.open_file.files[i]) );
				idx++;
			}
		}
		return files_array;
	}
	case JSF_EVENT_USER_TEXT:
		return JS_NewString(ctx, evt->user_event.event.clipboard.text ? evt->user_event.event.clipboard.text : "");

	case JSF_EVENT_USER_MT_ROTATION:
		return JS_NewFloat64(ctx, FIX2FLT(evt->user_event.event.mtouch.rotation) );
	case JSF_EVENT_USER_MT_PINCH:
		return JS_NewFloat64(ctx, FIX2FLT(evt->user_event.event.mtouch.pinch) );
	case JSF_EVENT_USER_MT_FINGERS:
		return JS_NewInt32(ctx, evt->user_event.event.mtouch.num_fingers);
	case JSF_EVENT_USER_WIDTH: return JS_NewInt32(ctx, evt->user_event.event.size.width);
	case JSF_EVENT_USER_HEIGHT: return JS_NewInt32(ctx, evt->user_event.event.size.height);
	case JSF_EVENT_USER_SHOWTYPE: return JS_NewInt32(ctx, evt->user_event.event.show.show_type);

	case JSF_EVENT_USER_MOVE_X: return JS_NewInt32(ctx, evt->user_event.event.move.x);
	case JSF_EVENT_USER_MOVE_Y: return JS_NewInt32(ctx, evt->user_event.event.move.y);
	case JSF_EVENT_USER_MOVE_RELATIVE: return JS_NewInt32(ctx, evt->user_event.event.move.relative);
	case JSF_EVENT_USER_MOVE_ALIGN_X: return JS_NewInt32(ctx, evt->user_event.event.move.align_x);
	case JSF_EVENT_USER_MOVE_ALIGN_Y: return JS_NewInt32(ctx, evt->user_event.event.move.align_y);

	case JSF_EVENT_USER_CAPTION:
		return JS_NewString(ctx, evt->user_event.event.caption.caption ? evt->user_event.event.caption.caption : "");
	}
    return JS_UNDEFINED;
}

GF_FilterEvent *jsf_get_event(JSContext *ctx, JSValueConst this_val)
{
	GF_FilterEvent *evt = JS_GetOpaque(this_val, jsf_event_class_id);
	return evt;
}

static const JSCFunctionListEntry jsf_event_funcs[] =
{
    JS_CGETSET_MAGIC_DEF("type", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_TYPE),
    JS_CGETSET_MAGIC_DEF("name", jsf_event_get_prop, NULL, JSF_EVENT_NAME),
    /*PLAY event*/
    JS_CGETSET_MAGIC_DEF("start_range", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_START_RANGE),
    JS_CGETSET_MAGIC_DEF("speed", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_SPEED),
    JS_CGETSET_MAGIC_DEF("hw_buffer_reset", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_HW_BUFFER_RESET),
    JS_CGETSET_MAGIC_DEF("initial_broadcast_play", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_INITIAL_BROADCAST_PLAY),
    JS_CGETSET_MAGIC_DEF("timestamp_based", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_TIMESTAMP_BASED),
    JS_CGETSET_MAGIC_DEF("full_file_only", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_FULL_FILE_ONLY),
    JS_CGETSET_MAGIC_DEF("forced_dash_segment_switch", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_FORCE_DASH_SEG_SWITCH),
    JS_CGETSET_MAGIC_DEF("from_pck", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_FROM_PCK),
	/*source switch*/
    JS_CGETSET_MAGIC_DEF("start_offset", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_START_OFFSET),
    JS_CGETSET_MAGIC_DEF("end_offset", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_END_OFFSET),
    JS_CGETSET_MAGIC_DEF("switch_url", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_SOURCE_SWITCH),
    JS_CGETSET_MAGIC_DEF("skip_cache_exp", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_SKIP_CACHE_EXPIRATION),
    JS_CGETSET_MAGIC_DEF("hint_block_size", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_HINT_BLOCK_SIZE),
	/*segment size*/
    JS_CGETSET_MAGIC_DEF("seg_url", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_SEG_URL),
    JS_CGETSET_MAGIC_DEF("is_init", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_SEG_IS_INIT),
    JS_CGETSET_MAGIC_DEF("media_start_range", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_MEDIA_START_RANGE),
    JS_CGETSET_MAGIC_DEF("media_end_range", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_MEDIA_END_RANGE),
    JS_CGETSET_MAGIC_DEF("index_start_range", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_IDX_START_RANGE),
    JS_CGETSET_MAGIC_DEF("index_end_range", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_IDX_END_RANGE),
	/*quality switch*/
    JS_CGETSET_MAGIC_DEF("up", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_SWITCH_UP),
    JS_CGETSET_MAGIC_DEF("dependent_group_index", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_SWITCH_GROUP_IDX),
    JS_CGETSET_MAGIC_DEF("q_idx", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_SWITCH_QUALITY_IDX),
    JS_CGETSET_MAGIC_DEF("set_tile_mode_plus_one", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_SWITCH_TILE_MODE),
    JS_CGETSET_MAGIC_DEF("quality_degradation", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_SWITCH_QUALITY_DEGRADATION),
    /*visibility hint*/
    JS_CGETSET_MAGIC_DEF("min_x", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_VIS_MIN_X),
    JS_CGETSET_MAGIC_DEF("min_y", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_VIS_MIN_Y),
    JS_CGETSET_MAGIC_DEF("max_x", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_VIS_MAX_X),
    JS_CGETSET_MAGIC_DEF("max_y", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_VIS_MAX_Y),
    JS_CGETSET_MAGIC_DEF("is_gaze", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_VIS_IS_GAZE),
    /*buffer reqs*/
    JS_CGETSET_MAGIC_DEF("max_buffer_us", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_BUFREQ_MAX_BUFFER_US),
    JS_CGETSET_MAGIC_DEF("max_playout_us", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_BUFREQ_MAX_PLAYOUT_US),
    JS_CGETSET_MAGIC_DEF("min_playout_us", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_BUFREQ_MIN_PLAYOUT_US),
    JS_CGETSET_MAGIC_DEF("pid_only", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_BUFREQ_PID_ONLY),
    /*ui events*/
    JS_CGETSET_MAGIC_DEF("ui_type", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_USER_TYPE),
    JS_CGETSET_MAGIC_DEF("keycode", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_USER_KEYCODE),
    JS_CGETSET_MAGIC_DEF("keyname", jsf_event_get_prop, NULL, JSF_EVENT_USER_KEYNAME),
    JS_CGETSET_MAGIC_DEF("keymods", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_USER_KEYMODS),
    JS_CGETSET_MAGIC_DEF("char", jsf_event_get_prop, NULL, JSF_EVENT_USER_TEXT_CHAR),
    JS_CGETSET_MAGIC_DEF("mouse_x", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_USER_MOUSE_X),
    JS_CGETSET_MAGIC_DEF("mouse_y", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_USER_MOUSE_Y),
    JS_CGETSET_MAGIC_DEF("wheel", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_USER_WHEEL),
    JS_CGETSET_MAGIC_DEF("button", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_USER_BUTTON),
    JS_CGETSET_MAGIC_DEF("hwkey", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_USER_HWKEY),
    JS_CGETSET_MAGIC_DEF("dropfiles", jsf_event_get_prop, NULL, JSF_EVENT_USER_DROPFILES),
    JS_CGETSET_MAGIC_DEF("clipboard", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_USER_TEXT),

    JS_CGETSET_MAGIC_DEF("mt_x", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_USER_MOUSE_X),
    JS_CGETSET_MAGIC_DEF("mt_y", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_USER_MOUSE_Y),
    JS_CGETSET_MAGIC_DEF("mt_rotate", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_USER_MT_ROTATION),
    JS_CGETSET_MAGIC_DEF("mt_pinch", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_USER_MT_PINCH),
    JS_CGETSET_MAGIC_DEF("mt_fingers", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_USER_MT_FINGERS),

    JS_CGETSET_MAGIC_DEF("width", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_USER_WIDTH),
    JS_CGETSET_MAGIC_DEF("height", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_USER_HEIGHT),
    JS_CGETSET_MAGIC_DEF("showtype", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_USER_SHOWTYPE),
    JS_CGETSET_MAGIC_DEF("move_x", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_USER_MOVE_X),
    JS_CGETSET_MAGIC_DEF("move_y", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_USER_MOVE_Y),
    JS_CGETSET_MAGIC_DEF("move_relative", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_USER_MOVE_RELATIVE),
    JS_CGETSET_MAGIC_DEF("move_alignx", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_USER_MOVE_ALIGN_X),
    JS_CGETSET_MAGIC_DEF("move_aligny", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_USER_MOVE_ALIGN_Y),
    JS_CGETSET_MAGIC_DEF("caption", jsf_event_get_prop, jsf_event_set_prop, JSF_EVENT_USER_CAPTION),
};

static JSValue jsf_event_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	GF_FilterEvent *evt;
    JSValue obj;
    s32 type;
	if (argc!=1)
		return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt32(ctx, &type, argv[0]))
		return GF_JS_EXCEPTION(ctx);
	if (!type)
    	return GF_JS_EXCEPTION(ctx);
    obj = JS_NewObjectClass(ctx, jsf_event_class_id);
    if (JS_IsException(obj)) return obj;

	GF_SAFEALLOC(evt, GF_FilterEvent);
    if (!evt) {
        JS_FreeValue(ctx, obj);
		return js_throw_err(ctx, GF_OUT_OF_MEM);
    }
    evt->base.type = type;
    if (type==GF_FEVT_PLAY)
    	evt->play.speed = 1.0;
    JS_SetOpaque(obj, evt);
    return obj;
}

enum
{
	JSF_PCK_START = 0,
	JSF_PCK_END,
	JSF_PCK_DTS,
	JSF_PCK_CTS,
	JSF_PCK_DUR,
	JSF_PCK_SAP,
	JSF_PCK_TIMESCALE,
	JSF_PCK_INTERLACED,
	JSF_PCK_SEEK,
	JSF_PCK_CORRUPTED,
	JSF_PCK_BYTE_OFFSET,
	JSF_PCK_ROLL,
	JSF_PCK_CRYPT,
	JSF_PCK_CLOCK_TYPE,
	JSF_PCK_CAROUSEL,
	JSF_PCK_SEQNUM,
	JSF_PCK_BLOCKING_REF,
	JSF_PCK_DEPENDS_ON,
	JSF_PCK_DEPENDED_ON,
	JSF_PCK_IS_LEADING,
	JSF_PCK_HAS_REDUNDANT,
	JSF_PCK_SIZE,
	JSF_PCK_DATA,
	JSF_PCK_FRAME_IFCE,
	JSF_PCK_FRAME_IFCE_GL,
	JSF_PCK_HAS_PROPERTIES,
};

static JSValue jsf_pck_set_prop(JSContext *ctx, JSValueConst this_val, JSValueConst value, int magic)
{
	GF_Err e = GF_OK;
	u32 ival, flags;
	u64 lival;
	Bool a1, a2;
	const char *str=NULL;
	GF_FilterPacket *pck;
	GF_JSPckCtx *pckctx = JS_GetOpaque(this_val, jsf_pck_class_id);
    if (!pckctx || !pckctx->pck) return GF_JS_EXCEPTION(ctx);
    pck = pckctx->pck;

    switch (magic) {
	case JSF_PCK_START:
		gf_filter_pck_get_framing(pck, &a1, &a2);
		gf_filter_pck_set_framing(pck, JS_ToBool(ctx, value), a2);
		break;
	case JSF_PCK_END:
		gf_filter_pck_get_framing(pck, &a1, &a2);
		gf_filter_pck_set_framing(pck, a1, JS_ToBool(ctx, value));
		break;
	case JSF_PCK_DTS:
		if (JS_IsNull(value))
			gf_filter_pck_set_dts(pck, GF_FILTER_NO_TS);
		else {
			if (JS_ToInt64(ctx, &lival, value)) return GF_JS_EXCEPTION(ctx);
			gf_filter_pck_set_dts(pck, lival);
		}
		break;
	case JSF_PCK_CTS:
		if (JS_IsNull(value))
			gf_filter_pck_set_cts(pck, GF_FILTER_NO_TS);
		else {
			if (JS_ToInt64(ctx, &lival, value)) return GF_JS_EXCEPTION(ctx);
			gf_filter_pck_set_cts(pck, lival);
		}
		break;
	case JSF_PCK_DUR:
		if (JS_ToInt32(ctx, &ival, value)) return GF_JS_EXCEPTION(ctx);
		gf_filter_pck_set_duration(pck, ival);
		break;
	case JSF_PCK_SAP:
		if (JS_ToInt32(ctx, &ival, value)) return GF_JS_EXCEPTION(ctx);
		gf_filter_pck_set_sap(pck, ival);
		break;
	case JSF_PCK_INTERLACED:
		if (JS_ToInt32(ctx, &ival, value)) return GF_JS_EXCEPTION(ctx);
		gf_filter_pck_set_interlaced(pck, ival);
		break;
	case JSF_PCK_SEEK:
		gf_filter_pck_set_seek_flag(pck, JS_ToBool(ctx, value));
		break;
	case JSF_PCK_CORRUPTED:
		gf_filter_pck_set_corrupted(pck, JS_ToBool(ctx, value));
		break;
	case JSF_PCK_BYTE_OFFSET:
		if (JS_IsNull(value))
			gf_filter_pck_set_byte_offset(pck, GF_FILTER_NO_BO);
		else {
			if (JS_ToInt64(ctx, &lival, value)) return GF_JS_EXCEPTION(ctx);
			gf_filter_pck_set_byte_offset(pck, lival);
		}
		break;
	case JSF_PCK_ROLL:
		if (JS_ToInt32(ctx, &ival, value)) return GF_JS_EXCEPTION(ctx);
		gf_filter_pck_set_roll_info(pck, (s16) ival);
		break;
	case JSF_PCK_CRYPT:
		if (JS_ToInt32(ctx, &ival, value)) return GF_JS_EXCEPTION(ctx);
		gf_filter_pck_set_crypt_flags(pck, ival);
		break;
	case JSF_PCK_CLOCK_TYPE:
		if (JS_ToInt32(ctx, &ival, value)) return GF_JS_EXCEPTION(ctx);
		gf_filter_pck_set_clock_type(pck, ival);
		break;
	case JSF_PCK_CAROUSEL:
		if (JS_ToInt32(ctx, &ival, value)) return GF_JS_EXCEPTION(ctx);
		gf_filter_pck_set_carousel_version(pck, ival);
		break;
	case JSF_PCK_SEQNUM:
		if (JS_ToInt32(ctx, &ival, value)) return GF_JS_EXCEPTION(ctx);
		gf_filter_pck_set_seq_num(pck, ival);
		break;
	case JSF_PCK_IS_LEADING:
		if (JS_ToInt32(ctx, &ival, value)) return GF_JS_EXCEPTION(ctx);
		flags = gf_filter_pck_get_dependency_flags(pck);
		flags &= 0x3F;
		flags |= (ival & 0x3)<<6;
		gf_filter_pck_set_seq_num(pck, flags);
		break;
	case JSF_PCK_DEPENDS_ON:
		if (JS_ToInt32(ctx, &ival, value)) return GF_JS_EXCEPTION(ctx);
		flags = gf_filter_pck_get_dependency_flags(pck);
		flags &= 0xCF;
		flags |= (ival & 0x3)<<4;
		gf_filter_pck_set_seq_num(pck, flags);
		break;
	case JSF_PCK_DEPENDED_ON:
		if (JS_ToInt32(ctx, &ival, value)) return GF_JS_EXCEPTION(ctx);
		flags = gf_filter_pck_get_dependency_flags(pck);
		flags &= 0xF3;
		flags |= (ival & 0x3)<<2;
		gf_filter_pck_set_seq_num(pck, flags);
		break;
	case JSF_PCK_HAS_REDUNDANT:
		if (JS_ToInt32(ctx, &ival, value)) return GF_JS_EXCEPTION(ctx);
		flags = gf_filter_pck_get_dependency_flags(pck);
		flags &= 0xFC;
		flags |= (ival & 0x3);
		gf_filter_pck_set_seq_num(pck, flags);
		break;
	}
	if (str)
		JS_FreeCString(ctx, str);
	if (e) return js_throw_err(ctx, e);
    return JS_UNDEFINED;
}

static JSValue jsf_pck_get_prop(JSContext *ctx, JSValueConst this_val, int magic)
{
	Bool a1, a2;
	u64 lival;
	u32 ival;
	GF_FilterPacket *pck;
	GF_JSPckCtx *pckctx = JS_GetOpaque(this_val, jsf_pck_class_id);
    if (!pckctx || !pckctx->pck) return GF_JS_EXCEPTION(ctx);
    pck = pckctx->pck;

	switch (magic) {
	case JSF_PCK_START:
		gf_filter_pck_get_framing(pck, &a1, &a2);
		return JS_NewBool(ctx, a1);
	case JSF_PCK_END:
		gf_filter_pck_get_framing(pck, &a1, &a2);
		return JS_NewBool(ctx, a2);
	case JSF_PCK_DTS:
		lival = gf_filter_pck_get_dts(pck);
		if (lival==GF_FILTER_NO_TS) return JS_NULL;
		return JS_NewInt64(ctx, lival);
	case JSF_PCK_CTS:
		lival = gf_filter_pck_get_cts(pck);
		if (lival==GF_FILTER_NO_TS) return JS_NULL;
		return JS_NewInt64(ctx, lival);
	case JSF_PCK_DUR:
		return JS_NewInt32(ctx, gf_filter_pck_get_duration(pck));
	case JSF_PCK_SAP:
		return JS_NewInt32(ctx, gf_filter_pck_get_sap(pck));
	case JSF_PCK_TIMESCALE:
		return JS_NewInt32(ctx, gf_filter_pck_get_timescale(pck));
	case JSF_PCK_INTERLACED:
		return JS_NewInt32(ctx, gf_filter_pck_get_interlaced(pck));
	case JSF_PCK_SEEK:
		return JS_NewBool(ctx, gf_filter_pck_get_seek_flag(pck));
	case JSF_PCK_CORRUPTED:
		return JS_NewBool(ctx, gf_filter_pck_get_corrupted(pck));
	case JSF_PCK_BYTE_OFFSET:
		lival = gf_filter_pck_get_byte_offset(pck);
		if (lival==GF_FILTER_NO_TS) return JS_NULL;
		return JS_NewInt64(ctx, lival);
	case JSF_PCK_ROLL:
		return JS_NewInt32(ctx, gf_filter_pck_get_roll_info(pck));
	case JSF_PCK_CRYPT:
		return JS_NewInt32(ctx, gf_filter_pck_get_crypt_flags(pck));
	case JSF_PCK_CLOCK_TYPE:
		return JS_NewInt32(ctx, gf_filter_pck_get_clock_type(pck));
	case JSF_PCK_CAROUSEL:
		return JS_NewInt32(ctx, gf_filter_pck_get_carousel_version(pck));
	case JSF_PCK_SEQNUM:
		return JS_NewInt32(ctx, gf_filter_pck_get_seq_num(pck));
	case JSF_PCK_BLOCKING_REF:
		return JS_NewBool(ctx, gf_filter_pck_is_blocking_ref(pck));
	case JSF_PCK_IS_LEADING:
		ival = gf_filter_pck_get_dependency_flags(pck);
		return JS_NewBool(ctx, (ival>>6) & 0x3);
	case JSF_PCK_DEPENDS_ON:
		ival = gf_filter_pck_get_dependency_flags(pck);
		return JS_NewBool(ctx, (ival>>4) & 0x3);
	case JSF_PCK_DEPENDED_ON:
		ival = gf_filter_pck_get_dependency_flags(pck);
		return JS_NewBool(ctx, (ival>>2) & 0x3);
	case JSF_PCK_HAS_REDUNDANT:
		ival = gf_filter_pck_get_dependency_flags(pck);
		return JS_NewBool(ctx, (ival) & 0x3);
	case JSF_PCK_SIZE:
		gf_filter_pck_get_data(pck, &ival);
		return JS_NewInt32(ctx, ival);
	case JSF_PCK_DATA:
		if (JS_IsUndefined(pckctx->data_ab)) {
			const u8 *data = gf_filter_pck_get_data(pck, &ival);
			if (!data) return JS_NULL;
			pckctx->data_ab = JS_NewArrayBuffer(ctx, (u8 *) data, ival, NULL, NULL, 0/*1*/);
		}
		return JS_DupValue(ctx, pckctx->data_ab);
	case JSF_PCK_FRAME_IFCE:
		if (gf_filter_pck_get_frame_interface(pck) != NULL)
			return JS_NewBool(ctx, 1);
		else return JS_NewBool(ctx, 0);
	case JSF_PCK_FRAME_IFCE_GL:
	{
		GF_FilterFrameInterface *fifce  = gf_filter_pck_get_frame_interface(pck);
		if (fifce && fifce->get_gl_texture)
			return JS_NewBool(ctx, 1);
		return JS_NewBool(ctx, 0);
	}

	case JSF_PCK_HAS_PROPERTIES:
		return JS_NewBool(ctx, gf_filter_pck_has_properties(pck) );
	}
    return JS_UNDEFINED;
}

static JSValue jsf_pck_set_readonly(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_FilterPacket *pck;
	GF_JSPckCtx *pckctx = JS_GetOpaque(this_val, jsf_pck_class_id);
    if (!pckctx || !pckctx->pck) return GF_JS_EXCEPTION(ctx);
    pck = pckctx->pck;

	gf_filter_pck_set_readonly(pck);
    return JS_UNDEFINED;
}
static JSValue jsf_pck_enum_properties(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s32 idx;
	u32 p4cc;
	const char *pname;
	JSValue res;
	const GF_PropertyValue *prop;
	GF_FilterPacket *pck;
	GF_JSPckCtx *pckctx = JS_GetOpaque(this_val, jsf_pck_class_id);
    if (!pckctx || !pckctx->pck) return GF_JS_EXCEPTION(ctx);
    pck = pckctx->pck;

    if (JS_ToInt32(ctx, &idx, argv[0]))
		return GF_JS_EXCEPTION(ctx);

	prop = gf_filter_pck_enum_properties(pck, &idx, &p4cc, &pname);
	if (!prop) return JS_NULL;
	if (!pname) pname = gf_props_4cc_get_name(p4cc);
	if (!pname) return GF_JS_EXCEPTION(ctx);

	res = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, res, "name", JS_NewString(ctx, pname));
    JS_SetPropertyStr(ctx, res, "type", JS_NewInt32(ctx, prop->type));
    JS_SetPropertyStr(ctx, res, "value", jsf_NewProp(ctx, prop));
    return res;
}

static JSValue jsf_pck_get_property(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JSValue res;
	const char *name=NULL;
	const GF_PropertyValue *prop;
	GF_FilterPacket *pck;
	GF_JSPckCtx *pckctx = JS_GetOpaque(this_val, jsf_pck_class_id);
    if (!pckctx || !pckctx->pck) return GF_JS_EXCEPTION(ctx);
    pck = pckctx->pck;

    name = JS_ToCString(ctx, argv[0]);
	if (!name) return GF_JS_EXCEPTION(ctx);
	if ((argc>1) && JS_ToBool(ctx, argv[1])) {
		prop = gf_filter_pck_get_property_str(pck, name);
		JS_FreeCString(ctx, name);
		if (!prop) return JS_NULL;
		res = jsf_NewProp(ctx, prop);
		JS_SetPropertyStr(ctx, res, "type", JS_NewInt32(ctx, prop->type));
	} else {
		u32 p4cc = gf_props_get_id(name);
		JS_FreeCString(ctx, name);
		if (!p4cc)
			return js_throw_err(ctx, GF_BAD_PARAM);

		prop = gf_filter_pck_get_property(pck, p4cc);
		if (!prop) return JS_NULL;
		res = jsf_NewPropTranslate(ctx, prop, p4cc);
	}
    return res;
}

static JSValue jsf_pck_ref(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Bool is_ref_props = GF_FALSE;
	GF_JSPckCtx *ref_pckctx;
	GF_JSPckCtx *pckctx = JS_GetOpaque(this_val, jsf_pck_class_id);
    if (!pckctx || !pckctx->pck) return GF_JS_EXCEPTION(ctx);

    if (argc && JS_ToBool(ctx, argv[0])) is_ref_props = GF_TRUE;

	ref_pckctx = gf_list_pop_back(pckctx->jspid->jsf->pck_res);
	if (!ref_pckctx) {
		GF_SAFEALLOC(ref_pckctx, GF_JSPckCtx);
		if (!ref_pckctx)
			return js_throw_err(ctx, GF_OUT_OF_MEM);
	}
	memcpy(ref_pckctx, pckctx, sizeof(GF_JSPckCtx));
	if (is_ref_props)
		gf_filter_pck_ref_props(&ref_pckctx->pck);
	else {
		gf_filter_pck_ref(&ref_pckctx->pck);
	}
	ref_pckctx->flags = GF_JS_PCK_IS_REF;
	ref_pckctx->jsobj = JS_NewObjectClass(ctx, jsf_pck_class_id);
	ref_pckctx->data_ab = JS_UNDEFINED;
	ref_pckctx->ref_val = JS_UNDEFINED;
	JS_SetOpaque(ref_pckctx->jsobj, ref_pckctx);
	return JS_DupValue(ctx, ref_pckctx->jsobj);
}

static JSValue jsf_pck_unref(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSPidCtx *jspid;
	GF_JSPckCtx *pckctx = JS_GetOpaque(this_val, jsf_pck_class_id);
    if (!pckctx || !pckctx->pck) return GF_JS_EXCEPTION(ctx);
 	if (!(pckctx->flags & GF_JS_PCK_IS_REF))
		return js_throw_err_msg(ctx, GF_BAD_PARAM, "Attempt to unref a non-reference packet");

	gf_filter_pck_unref(pckctx->pck);
	pckctx->pck = NULL;
	JS_FreeValue(ctx, pckctx->jsobj);
	JS_FreeValue(ctx, pckctx->data_ab);
	JS_SetOpaque(this_val, NULL);
	jspid = pckctx->jspid;
	memset(pckctx, 0, sizeof(GF_JSPckCtx));
	gf_list_add(jspid->jsf->pck_res, pckctx);
	return JS_UNDEFINED;
}

static JSValue jsf_pck_send(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_FilterPacket *pck;
	GF_JSPckCtx *pckctx = JS_GetOpaque(this_val, jsf_pck_class_id);
    if (!pckctx || !pckctx->pck) return GF_JS_EXCEPTION(ctx);
	if (! pckctx->jspid->jsf->filter->in_process)
		return js_throw_err_msg(ctx, GF_BAD_PARAM, "Filter %s attempt to send packet outside process callback not allowed!\n", pckctx->jspid->jsf->filter->name);

    pck = pckctx->pck;
    if (!JS_IsUndefined(pckctx->data_ab)) {
    	JS_FreeValue(ctx, pckctx->data_ab);
    	pckctx->data_ab = JS_UNDEFINED;
	}
	gf_filter_pck_send(pck);
	JS_SetOpaque(this_val, NULL);
	if (!(pckctx->flags & GF_JS_PCK_IS_SHARED)) {
		if (pckctx->jspid) {
			gf_list_add(pckctx->jspid->jsf->pck_res, pckctx);
			memset(pckctx, 0, sizeof(GF_JSPckCtx));
		}
	}
	return JS_UNDEFINED;
}
static JSValue jsf_pck_discard(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_FilterPacket *pck;
	GF_JSPckCtx *pckctx = JS_GetOpaque(this_val, jsf_pck_class_id);
    if (!pckctx || !pckctx->pck) return GF_JS_EXCEPTION(ctx);
    pck = pckctx->pck;
    pckctx->pck = NULL;
	gf_filter_pck_discard(pck);
	return JS_UNDEFINED;
}

static JSValue jsf_pck_set_property(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Err e;
	GF_PropertyValue prop;
	const char *name=NULL;
	GF_FilterPacket *pck;
	GF_JSPckCtx *pckctx = JS_GetOpaque(this_val, jsf_pck_class_id);
    if (!pckctx || !pckctx->pck) return GF_JS_EXCEPTION(ctx);
    pck = pckctx->pck;

    name = JS_ToCString(ctx, argv[0]);
	if (!name) return GF_JS_EXCEPTION(ctx);

	if ((argc>2) && JS_ToBool(ctx, argv[2])) {
		if (!JS_IsNull(argv[2])) {
			e = jsf_ToProp(pckctx->jspid->jsf->filter, ctx, argv[1], 0, &prop);
			JS_FreeCString(ctx, name);
			if (e) return js_throw_err(ctx, e);
			e = gf_filter_pck_set_property_dyn(pck, (char *) name, &prop);
			gf_props_reset_single(&prop);
		} else {
			e = gf_filter_pck_set_property_dyn(pck, (char *) name, NULL);
		}
	} else {
		u32 p4cc = gf_props_get_id(name);
		if (!p4cc) {
			JSValue ret;
			JS_FreeCString(ctx, name);
			ret = js_throw_err_msg(ctx, GF_BAD_PARAM, "Urecognized builtin property name %s\n", name);
			return ret;
		}
		JS_FreeCString(ctx, name);
		if ( ((p4cc==GF_PROP_PCK_SENDER_NTP) || (p4cc==GF_PROP_PCK_RECEIVER_NTP))
			&& JS_IsBool(argv[1]) && JS_ToBool(ctx, argv[1])
		) {
			e = gf_filter_pck_set_property(pck, p4cc, &PROP_LONGUINT(gf_net_get_ntp_ts()) );
		}
		else if (JS_IsNull(argv[1])) {
			e = gf_filter_pck_set_property(pck, p4cc, NULL);
		}
		else {
			e = jsf_ToProp(pckctx->jspid->jsf->filter, ctx, argv[1], p4cc, &prop);
			if (e) return js_throw_err(ctx, e);
			if ( ((p4cc==GF_PROP_PCK_SENDER_NTP) || (p4cc==GF_PROP_PCK_RECEIVER_NTP)) && (prop.type==GF_PROP_FRACTION)) {
				u64 ntp = (u32) prop.value.frac.num;
				ntp <<= 32;
				ntp |= prop.value.frac.den;
				prop.type = GF_PROP_LUINT;
				prop.value.longuint = ntp;
			}

			e = gf_filter_pck_set_property(pck, p4cc, &prop);
			gf_props_reset_single(&prop);
		}
	}
	if (e) return js_throw_err(ctx, e);
    return JS_UNDEFINED;
}

static JSValue jsf_pck_append_data(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u8 *data_start=NULL;
	u32 data_size=0;
	u8 *new_start;
	GF_Err e;
	GF_FilterPacket *pck;
	GF_JSPckCtx *pckctx = JS_GetOpaque(this_val, jsf_pck_class_id);
    if (!pckctx || !pckctx->pck || !argc) return GF_JS_EXCEPTION(ctx);
    pck = pckctx->pck;

	if (JS_IsString(argv[0])  || JS_IsInteger(argv[0])) {
    	u32 len;
    	const char *str = NULL;

		if (JS_IsInteger(argv[0])) {
			JS_ToInt32(ctx, &len, argv[0]);
		} else {
			str = JS_ToCString(ctx, argv[0]);
			if (!str) return GF_JS_EXCEPTION(ctx);
			len = (u32) strlen(str);
		}

		e = gf_filter_pck_expand(pck, len, &data_start, &new_start, &data_size);
		if (!new_start || e) {
			if (str) JS_FreeCString(ctx, str);
			return js_throw_err(ctx, e);
		}
		if (str) {
			memcpy(new_start, str, len);
			JS_FreeCString(ctx, str);
		}

		jsf_pck_detach_ab(ctx, pckctx);
		return JS_NewArrayBuffer(ctx, (u8 *) new_start, len, NULL, NULL, 0/*1*/);
	}

	if (!JS_IsObject(argv[0])) return GF_JS_EXCEPTION(ctx);

	size_t ab_size;
	u8 *data = JS_GetArrayBuffer(ctx, &ab_size, argv[0]);
	if (!data)
		return GF_JS_EXCEPTION(ctx);

	e = gf_filter_pck_expand(pck, (u32) ab_size, &data_start, &new_start, &data_size);
	if (!new_start || e) {
		return js_throw_err(ctx, e);
	}
	memcpy(new_start, data, ab_size);

	jsf_pck_detach_ab(ctx, pckctx);

	return JS_NewArrayBuffer(ctx, (u8 *) new_start, ab_size, NULL, NULL, 0/*1*/);
}

static JSValue jsf_pck_truncate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 len=0;
	GF_Err e;
	GF_FilterPacket *pck;
	GF_JSPckCtx *pckctx = JS_GetOpaque(this_val, jsf_pck_class_id);
    if (!pckctx || !pckctx->pck) return GF_JS_EXCEPTION(ctx);
    pck = pckctx->pck;
	if (argc) {
		JS_ToInt32(ctx, &len, argv[0]);
	}
	e = gf_filter_pck_truncate(pck, len);
	if (e) return js_throw_err(ctx, e);

	jsf_pck_detach_ab(ctx, pckctx);
    return JS_UNDEFINED;
}
static JSValue jsf_pck_copy_props(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Err e;
	GF_JSPckCtx *pck_dst = JS_GetOpaque(this_val, jsf_pck_class_id);
	if (!pck_dst || !pck_dst->pck || !argc)
		return GF_JS_EXCEPTION(ctx);
	GF_JSPckCtx *pck_from = JS_GetOpaque(argv[0], jsf_pck_class_id);
    if (!pck_from || !pck_from->pck)
    	return GF_JS_EXCEPTION(ctx);
    e = gf_filter_pck_merge_properties(pck_from->pck, pck_dst->pck);
	if (e) return js_throw_err(ctx, e);
    return JS_UNDEFINED;
}

static JSValue jsf_pck_clone(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JSValue res;
	GF_FilterPacket *cloned;
	GF_JSPckCtx *pck_cached = NULL;
	GF_JSPckCtx *pck_src = JS_GetOpaque(this_val, jsf_pck_class_id);
	if (!pck_src || !pck_src->pck)
		return GF_JS_EXCEPTION(ctx);
	if (argc) {
		pck_cached = JS_GetOpaque(argv[0], jsf_pck_class_id);
		if (pck_cached && !pck_cached->pck)
			return GF_JS_EXCEPTION(ctx);
    }
	cloned = gf_filter_pck_dangling_copy(pck_src->pck, pck_cached ? pck_cached->pck : NULL);
	if (!cloned) return js_throw_err(ctx, GF_OUT_OF_MEM);

	if (pck_cached) {
		pck_cached->pck = cloned;
		return JS_DupValue(ctx, pck_cached->jsobj);
	}
	res = JS_NewObjectClass(ctx, jsf_pck_class_id);
	pck_cached = gf_list_pop_back(pck_src->jspid->jsf->pck_res);
	if (!pck_cached) {
		GF_SAFEALLOC(pck_cached, GF_JSPckCtx);
		if (!pck_cached) return js_throw_err(ctx, GF_OUT_OF_MEM);
	}
	memset(pck_cached, 0, sizeof(GF_JSPckCtx));
	pck_cached->pck = cloned;
	pck_cached->jsobj = res;
	pck_cached->jspid = NULL;
	pck_cached->ref_val = JS_UNDEFINED;
	pck_cached->data_ab = JS_UNDEFINED;
	pck_cached->flags = GF_JS_PCK_IS_DANGLING;
	JS_SetOpaque(res, pck_cached);
    return res;
}


static const JSCFunctionListEntry jsf_pck_funcs[] =
{
    JS_CGETSET_MAGIC_DEF("start", jsf_pck_get_prop, jsf_pck_set_prop, JSF_PCK_START),
    JS_CGETSET_MAGIC_DEF("end", jsf_pck_get_prop, jsf_pck_set_prop, JSF_PCK_END),
    JS_CGETSET_MAGIC_DEF("dts", jsf_pck_get_prop, jsf_pck_set_prop, JSF_PCK_DTS),
    JS_CGETSET_MAGIC_DEF("cts", jsf_pck_get_prop, jsf_pck_set_prop, JSF_PCK_CTS),
    JS_CGETSET_MAGIC_DEF("dur", jsf_pck_get_prop, jsf_pck_set_prop, JSF_PCK_DUR),
    JS_CGETSET_MAGIC_DEF("sap", jsf_pck_get_prop, jsf_pck_set_prop, JSF_PCK_SAP),
    JS_CGETSET_MAGIC_DEF("timescale", jsf_pck_get_prop, NULL, JSF_PCK_TIMESCALE),
    JS_CGETSET_MAGIC_DEF("interlaced", jsf_pck_get_prop, jsf_pck_set_prop, JSF_PCK_INTERLACED),
    JS_CGETSET_MAGIC_DEF("corrupted", jsf_pck_get_prop, jsf_pck_set_prop, JSF_PCK_CORRUPTED),
    JS_CGETSET_MAGIC_DEF("seek", jsf_pck_get_prop, jsf_pck_set_prop, JSF_PCK_SEEK),
    JS_CGETSET_MAGIC_DEF("byte_offset", jsf_pck_get_prop, jsf_pck_set_prop, JSF_PCK_BYTE_OFFSET),
    JS_CGETSET_MAGIC_DEF("roll", jsf_pck_get_prop, jsf_pck_set_prop, JSF_PCK_ROLL),
    JS_CGETSET_MAGIC_DEF("crypt", jsf_pck_get_prop, jsf_pck_set_prop, JSF_PCK_CRYPT),
    JS_CGETSET_MAGIC_DEF("clock_type", jsf_pck_get_prop, jsf_pck_set_prop, JSF_PCK_CLOCK_TYPE),
    JS_CGETSET_MAGIC_DEF("carousel", jsf_pck_get_prop, jsf_pck_set_prop, JSF_PCK_CAROUSEL),
    JS_CGETSET_MAGIC_DEF("seqnum", jsf_pck_get_prop, jsf_pck_set_prop, JSF_PCK_SEQNUM),
    JS_CGETSET_MAGIC_DEF("blocking_ref", jsf_pck_get_prop, NULL, JSF_PCK_BLOCKING_REF),
    JS_CGETSET_MAGIC_DEF("is_leading", jsf_pck_get_prop, jsf_pck_set_prop, JSF_PCK_IS_LEADING),
    JS_CGETSET_MAGIC_DEF("depends_on", jsf_pck_get_prop, jsf_pck_set_prop, JSF_PCK_DEPENDS_ON),
    JS_CGETSET_MAGIC_DEF("depended_on", jsf_pck_get_prop, jsf_pck_set_prop, JSF_PCK_DEPENDED_ON),
    JS_CGETSET_MAGIC_DEF("redundant", jsf_pck_get_prop, jsf_pck_set_prop, JSF_PCK_HAS_REDUNDANT),
    JS_CGETSET_MAGIC_DEF("size", jsf_pck_get_prop, NULL, JSF_PCK_SIZE),
    JS_CGETSET_MAGIC_DEF("data", jsf_pck_get_prop, NULL, JSF_PCK_DATA),
    JS_CGETSET_MAGIC_DEF("frame_ifce", jsf_pck_get_prop, NULL, JSF_PCK_FRAME_IFCE),
    JS_CGETSET_MAGIC_DEF("frame_ifce_gl", jsf_pck_get_prop, NULL, JSF_PCK_FRAME_IFCE_GL),
    JS_CGETSET_MAGIC_DEF("has_properties", jsf_pck_get_prop, NULL, JSF_PCK_HAS_PROPERTIES),

    JS_CFUNC_DEF("set_readonly", 0, jsf_pck_set_readonly),
    JS_CFUNC_DEF("enum_properties", 0, jsf_pck_enum_properties),
    JS_CFUNC_DEF("get_prop", 0, jsf_pck_get_property),
    JS_CFUNC_DEF("ref", 0, jsf_pck_ref),
    JS_CFUNC_DEF("unref", 0, jsf_pck_unref),
    JS_CFUNC_DEF("send", 0, jsf_pck_send),
    JS_CFUNC_DEF("discard", 0, jsf_pck_discard),
    JS_CFUNC_DEF("set_prop", 0, jsf_pck_set_property),
    JS_CFUNC_DEF("append", 0, jsf_pck_append_data),
    JS_CFUNC_DEF("truncate", 0, jsf_pck_truncate),
    JS_CFUNC_DEF("copy_props", 0, jsf_pck_copy_props),
    JS_CFUNC_DEF("clone", 0, jsf_pck_clone),
};


static GF_Err jsfilter_process(GF_Filter *filter)
{
	JSValue ret;
	GF_Err e = GF_OK;
	GF_JSFilterCtx *jsf = gf_filter_get_udta(filter);
	if (!jsf) return GF_BAD_PARAM;

	gf_js_lock(jsf->ctx, GF_TRUE);
	ret = JS_Call(jsf->ctx, jsf->funcs[JSF_EVT_PROCESS], jsf->filter_obj, 0, NULL);
	if (JS_IsException(ret)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[%s] Error processing\n", jsf->log_name));
		js_dump_error(jsf->ctx);
		e = GF_BAD_PARAM;
	}
	else if (JS_IsInteger(ret))
		JS_ToInt32(jsf->ctx, (int*)&e, ret);

	JS_FreeValue(jsf->ctx, ret);
	gf_js_lock(jsf->ctx, GF_FALSE);

	js_std_loop(jsf->ctx);
	return e;
}



static GF_Err jsfilter_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	JSValue ret;
	GF_Err e = GF_OK;
	GF_JSFilterCtx *jsf = gf_filter_get_udta(filter);
	GF_JSPidCtx *pctx;

	if (!jsf) return GF_BAD_PARAM;

	pctx = gf_filter_pid_get_udta(pid);

	if (is_remove) {
		assert(pctx);
		gf_js_lock(jsf->ctx, GF_TRUE);
		ret = JS_Call(jsf->ctx, jsf->funcs[JSF_EVT_REMOVE_PID], jsf->filter_obj, 1, &pctx->jsobj);
		if (JS_IsException(ret)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[%s] Error removing pid\n", jsf->log_name));
			js_dump_error(jsf->ctx);
			e = GF_BAD_PARAM;
		}
		else if (JS_IsInteger(ret))
			JS_ToInt32(jsf->ctx, (int*)&e, ret);

		//reset first packet obj if set
		if (pctx->pck_head) {
			JS_FreeValue(jsf->ctx, pctx->pck_head->jsobj);
			//might be set to NULL while freeing above obj
			if (pctx->pck_head) {
				pctx->pck_head->jsobj = JS_UNDEFINED;
				pctx->pck_head->jspid = NULL;
			}
		}
		JS_FreeValue(jsf->ctx, ret);
		//force cleanup of all refs
		gf_js_call_gc(jsf->ctx);

		JS_SetOpaque(pctx->jsobj, NULL);
		JS_FreeValue(jsf->ctx, pctx->jsobj);
		gf_list_del_item(jsf->pids, pctx);
		gf_filter_pid_set_udta(pid, NULL);
		gf_free(pctx);
		gf_js_lock(jsf->ctx, GF_FALSE);
		js_std_loop(jsf->ctx);
		return e;
	}

	gf_js_lock(jsf->ctx, GF_TRUE);

	if (!pctx) {
		GF_SAFEALLOC(pctx, GF_JSPidCtx);
		if (!pctx) return GF_OUT_OF_MEM;
		
		pctx->jsf = jsf;
		pctx->pid = pid;
		pctx->jsobj = JS_NewObjectClass(jsf->ctx, jsf_pid_class_id);
		gf_filter_pid_set_udta(pid, pctx);
		gf_list_add(jsf->pids, pctx);
		JS_SetOpaque(pctx->jsobj, pctx);
	}

	ret = JS_Call(jsf->ctx, jsf->funcs[JSF_EVT_CONFIGURE_PID], jsf->filter_obj, 1, &pctx->jsobj);
	if (JS_IsException(ret)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[%s] Error configure pid\n", jsf->log_name));
		js_dump_error(jsf->ctx);
		e = GF_BAD_PARAM;
	}
	else if (JS_IsInteger(ret))
			JS_ToInt32(jsf->ctx, (int*)&e, ret);
	JS_FreeValue(jsf->ctx, ret);

	gf_js_lock(jsf->ctx, GF_FALSE);
	js_std_loop(jsf->ctx);
	return e;
}

void js_load_constants(JSContext *ctx, JSValue global_obj)
{
    JSValue val;

    val = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, val, "log", JS_NewCFunction(ctx, js_print, "log", 1));
    JS_SetPropertyStr(ctx, global_obj, "console", val);

#define DEF_CONST( _val ) \
    JS_SetPropertyStr(ctx, global_obj, #_val, JS_NewInt32(ctx, _val));

	DEF_CONST(GF_LOG_ERROR)
	DEF_CONST(GF_LOG_WARNING)
	DEF_CONST(GF_LOG_INFO)
	DEF_CONST(GF_LOG_DEBUG)

	DEF_CONST(GF_PROP_BOOL)
	DEF_CONST(GF_PROP_UINT)
	DEF_CONST(GF_PROP_SINT)
	DEF_CONST(GF_PROP_LUINT)
	DEF_CONST(GF_PROP_LSINT)
	DEF_CONST(GF_PROP_FRACTION)
	DEF_CONST(GF_PROP_FRACTION64)
	DEF_CONST(GF_PROP_FLOAT)
	DEF_CONST(GF_PROP_DOUBLE)
	DEF_CONST(GF_PROP_VEC2I)
	DEF_CONST(GF_PROP_VEC2)
	DEF_CONST(GF_PROP_VEC3I)
	DEF_CONST(GF_PROP_VEC4I)
	DEF_CONST(GF_PROP_STRING)
	DEF_CONST(GF_PROP_STRING)
	DEF_CONST(GF_PROP_STRING_NO_COPY)
	DEF_CONST(GF_PROP_DATA)
	DEF_CONST(GF_PROP_NAME)
	DEF_CONST(GF_PROP_DATA_NO_COPY)
	DEF_CONST(GF_PROP_CONST_DATA)
	DEF_CONST(GF_PROP_POINTER)
	DEF_CONST(GF_PROP_STRING_LIST)
	DEF_CONST(GF_PROP_UINT_LIST)
	DEF_CONST(GF_PROP_SINT_LIST)
	DEF_CONST(GF_PROP_VEC2I_LIST)
	DEF_CONST(GF_PROP_4CC)
	DEF_CONST(GF_PROP_4CC_LIST)

	DEF_CONST(GF_PROP_PIXFMT)
	DEF_CONST(GF_PROP_PCMFMT)
	DEF_CONST(GF_PROP_CICP_COL_PRIM)
	DEF_CONST(GF_PROP_CICP_COL_TFC)
	DEF_CONST(GF_PROP_CICP_COL_MX)


	DEF_CONST(GF_FEVT_PLAY)
	DEF_CONST(GF_FEVT_SET_SPEED)
	DEF_CONST(GF_FEVT_STOP)
	DEF_CONST(GF_FEVT_PAUSE)
	DEF_CONST(GF_FEVT_RESUME)
	DEF_CONST(GF_FEVT_SOURCE_SEEK)
	DEF_CONST(GF_FEVT_SOURCE_SWITCH)
	DEF_CONST(GF_FEVT_SEGMENT_SIZE)
	DEF_CONST(GF_FEVT_QUALITY_SWITCH)
	DEF_CONST(GF_FEVT_VISIBILITY_HINT)
	DEF_CONST(GF_FEVT_INFO_UPDATE)
	DEF_CONST(GF_FEVT_BUFFER_REQ)
	DEF_CONST(GF_FEVT_CAPS_CHANGE)
	DEF_CONST(GF_FEVT_CONNECT_FAIL)
	DEF_CONST(GF_FEVT_USER)

	DEF_CONST(GF_STATS_LOCAL)
	DEF_CONST(GF_STATS_LOCAL_INPUTS)
	DEF_CONST(GF_STATS_DECODER_SINK)
	DEF_CONST(GF_STATS_DECODER_SOURCE)
	DEF_CONST(GF_STATS_ENCODER_SINK)
	DEF_CONST(GF_STATS_ENCODER_SOURCE)
	DEF_CONST(GF_STATS_SINK)

	DEF_CONST(GF_FILTER_CLOCK_NONE)
	DEF_CONST(GF_FILTER_CLOCK_PCR)
	DEF_CONST(GF_FILTER_CLOCK_PCR_DISC)

	DEF_CONST(GF_FILTER_SAP_NONE)
	DEF_CONST(GF_FILTER_SAP_1)
	DEF_CONST(GF_FILTER_SAP_2)
	DEF_CONST(GF_FILTER_SAP_3)
	DEF_CONST(GF_FILTER_SAP_4)
	DEF_CONST(GF_FILTER_SAP_4_PROL)

	DEF_CONST(GF_STATS_LOCAL)
	DEF_CONST(GF_STATS_LOCAL_INPUTS)
	DEF_CONST(GF_STATS_DECODER_SINK)
	DEF_CONST(GF_STATS_DECODER_SOURCE)
	DEF_CONST(GF_STATS_ENCODER_SINK)
	DEF_CONST(GF_STATS_ENCODER_SOURCE)
	DEF_CONST(GF_STATS_SINK)

	DEF_CONST(GF_EOS);
	DEF_CONST(GF_OK)
	DEF_CONST(GF_BAD_PARAM)
	DEF_CONST(GF_OUT_OF_MEM)
	DEF_CONST(GF_IO_ERR)
	DEF_CONST(GF_NOT_SUPPORTED)
	DEF_CONST(GF_CORRUPTED_DATA)
	DEF_CONST(GF_SG_UNKNOWN_NODE)
	DEF_CONST(GF_SG_INVALID_PROTO)
	DEF_CONST(GF_SCRIPT_ERROR)
	DEF_CONST(GF_BUFFER_TOO_SMALL)
	DEF_CONST(GF_NON_COMPLIANT_BITSTREAM)
	DEF_CONST(GF_FILTER_NOT_FOUND)
	DEF_CONST(GF_URL_ERROR)
	DEF_CONST(GF_SERVICE_ERROR)
	DEF_CONST(GF_REMOTE_SERVICE_ERROR)
	DEF_CONST(GF_STREAM_NOT_FOUND)
	DEF_CONST(GF_ISOM_INVALID_FILE)
	DEF_CONST(GF_ISOM_INCOMPLETE_FILE)
	DEF_CONST(GF_ISOM_INVALID_MEDIA)
	DEF_CONST(GF_ISOM_INVALID_MODE)
	DEF_CONST(GF_ISOM_UNKNOWN_DATA_REF)
	DEF_CONST(GF_ODF_INVALID_DESCRIPTOR)
	DEF_CONST(GF_ODF_FORBIDDEN_DESCRIPTOR)
	DEF_CONST(GF_ODF_INVALID_COMMAND)
	DEF_CONST(GF_BIFS_UNKNOWN_VERSION)
	DEF_CONST(GF_IP_ADDRESS_NOT_FOUND)
	DEF_CONST(GF_IP_CONNECTION_FAILURE)
	DEF_CONST(GF_IP_NETWORK_FAILURE)
	DEF_CONST(GF_IP_CONNECTION_CLOSED)
	DEF_CONST(GF_IP_NETWORK_EMPTY)
	DEF_CONST(GF_IP_UDP_TIMEOUT)
	DEF_CONST(GF_AUTHENTICATION_FAILURE)
	DEF_CONST(GF_SCRIPT_NOT_READY)
	DEF_CONST(GF_INVALID_CONFIGURATION)
	DEF_CONST(GF_NOT_FOUND)
	DEF_CONST(GF_PROFILE_NOT_SUPPORTED)
	DEF_CONST(GF_REQUIRES_NEW_INSTANCE)
	DEF_CONST(GF_FILTER_NOT_SUPPORTED)

	DEF_CONST(JSF_SETUP_ERROR)
	DEF_CONST(JSF_NOTIF_ERROR)
	DEF_CONST(JSF_NOTIF_ERROR_AND_DISCONNECT)

	DEF_CONST(GF_FILTER_UPDATE_DOWNSTREAM)
	DEF_CONST(GF_FILTER_UPDATE_UPSTREAM)

	DEF_CONST(GF_EVENT_CLICK)
	DEF_CONST(GF_EVENT_MOUSEUP)
	DEF_CONST(GF_EVENT_MOUSEDOWN)
	DEF_CONST(GF_EVENT_MOUSEMOVE)
	DEF_CONST(GF_EVENT_MOUSEWHEEL)
	DEF_CONST(GF_EVENT_DBLCLICK)
	DEF_CONST(GF_EVENT_MULTITOUCH)
	DEF_CONST(GF_EVENT_KEYUP)
	DEF_CONST(GF_EVENT_KEYDOWN)
	DEF_CONST(GF_EVENT_TEXTINPUT)
	DEF_CONST(GF_EVENT_DROPFILE)
	DEF_CONST(GF_EVENT_TIMESHIFT_DEPTH)
	DEF_CONST(GF_EVENT_TIMESHIFT_UPDATE)
	DEF_CONST(GF_EVENT_TIMESHIFT_OVERFLOW)
	DEF_CONST(GF_EVENT_TIMESHIFT_UNDERRUN)
	DEF_CONST(GF_EVENT_PASTE_TEXT)
	DEF_CONST(GF_EVENT_COPY_TEXT)
	DEF_CONST(GF_EVENT_SIZE)
	DEF_CONST(GF_EVENT_SHOWHIDE)
	DEF_CONST(GF_EVENT_MOVE)
	DEF_CONST(GF_EVENT_SET_CAPTION)
	DEF_CONST(GF_EVENT_REFRESH)
	DEF_CONST(GF_EVENT_QUIT)

	DEF_CONST(GF_KEY_UNIDENTIFIED)
	DEF_CONST(GF_KEY_ACCEPT)
	DEF_CONST(GF_KEY_AGAIN)
	DEF_CONST(GF_KEY_ALLCANDIDATES)
	DEF_CONST(GF_KEY_ALPHANUM)
	DEF_CONST(GF_KEY_ALT)
	DEF_CONST(GF_KEY_ALTGRAPH)
	DEF_CONST(GF_KEY_APPS)
	DEF_CONST(GF_KEY_ATTN)
	DEF_CONST(GF_KEY_BROWSERBACK)
	DEF_CONST(GF_KEY_BROWSERFAVORITES)
	DEF_CONST(GF_KEY_BROWSERFORWARD)
	DEF_CONST(GF_KEY_BROWSERHOME)
	DEF_CONST(GF_KEY_BROWSERREFRESH)
	DEF_CONST(GF_KEY_BROWSERSEARCH)
	DEF_CONST(GF_KEY_BROWSERSTOP)
	DEF_CONST(GF_KEY_CAPSLOCK)
	DEF_CONST(GF_KEY_CLEAR)
	DEF_CONST(GF_KEY_CODEINPUT)
	DEF_CONST(GF_KEY_COMPOSE)
	DEF_CONST(GF_KEY_CONTROL)
	DEF_CONST(GF_KEY_CRSEL)
	DEF_CONST(GF_KEY_CONVERT)
	DEF_CONST(GF_KEY_COPY)
	DEF_CONST(GF_KEY_CUT)
	DEF_CONST(GF_KEY_DOWN)
	DEF_CONST(GF_KEY_END)
	DEF_CONST(GF_KEY_ENTER)
	DEF_CONST(GF_KEY_ERASEEOF)
	DEF_CONST(GF_KEY_EXECUTE)
	DEF_CONST(GF_KEY_EXSEL)
	DEF_CONST(GF_KEY_F1)
	DEF_CONST(GF_KEY_F2)
	DEF_CONST(GF_KEY_F3)
	DEF_CONST(GF_KEY_F4)
	DEF_CONST(GF_KEY_F5)
	DEF_CONST(GF_KEY_F6)
	DEF_CONST(GF_KEY_F7)
	DEF_CONST(GF_KEY_F8)
	DEF_CONST(GF_KEY_F9)
	DEF_CONST(GF_KEY_F10)
	DEF_CONST(GF_KEY_F11)
	DEF_CONST(GF_KEY_F12)
	DEF_CONST(GF_KEY_F13)
	DEF_CONST(GF_KEY_F14)
	DEF_CONST(GF_KEY_F15)
	DEF_CONST(GF_KEY_F16)
	DEF_CONST(GF_KEY_F17)
	DEF_CONST(GF_KEY_F18)
	DEF_CONST(GF_KEY_F19)
	DEF_CONST(GF_KEY_F20)
	DEF_CONST(GF_KEY_F21)
	DEF_CONST(GF_KEY_F22)
	DEF_CONST(GF_KEY_F23)
	DEF_CONST(GF_KEY_F24)
	DEF_CONST(GF_KEY_FINALMODE)
	DEF_CONST(GF_KEY_FIND)
	DEF_CONST(GF_KEY_FULLWIDTH)
	DEF_CONST(GF_KEY_HALFWIDTH)
	DEF_CONST(GF_KEY_HANGULMODE)
	DEF_CONST(GF_KEY_HANJAMODE)
	DEF_CONST(GF_KEY_HELP)
	DEF_CONST(GF_KEY_HIRAGANA)
	DEF_CONST(GF_KEY_HOME)
	DEF_CONST(GF_KEY_INSERT)
	DEF_CONST(GF_KEY_JAPANESEHIRAGANA)
	DEF_CONST(GF_KEY_JAPANESEKATAKANA)
	DEF_CONST(GF_KEY_JAPANESEROMAJI)
	DEF_CONST(GF_KEY_JUNJAMODE)
	DEF_CONST(GF_KEY_KANAMODE)
	DEF_CONST(GF_KEY_KANJIMODE)
	DEF_CONST(GF_KEY_KATAKANA)
	DEF_CONST(GF_KEY_LAUNCHAPPLICATION1)
	DEF_CONST(GF_KEY_LAUNCHAPPLICATION2)
	DEF_CONST(GF_KEY_LAUNCHMAIL)
	DEF_CONST(GF_KEY_LEFT)
	DEF_CONST(GF_KEY_META)
	DEF_CONST(GF_KEY_MEDIANEXTTRACK)
	DEF_CONST(GF_KEY_MEDIAPLAYPAUSE)
	DEF_CONST(GF_KEY_MEDIAPREVIOUSTRACK)
	DEF_CONST(GF_KEY_MEDIASTOP)
	DEF_CONST(GF_KEY_MODECHANGE)
	DEF_CONST(GF_KEY_NONCONVERT)
	DEF_CONST(GF_KEY_NUMLOCK)
	DEF_CONST(GF_KEY_PAGEDOWN)
	DEF_CONST(GF_KEY_PAGEUP)
	DEF_CONST(GF_KEY_PASTE)
	DEF_CONST(GF_KEY_PAUSE)
	DEF_CONST(GF_KEY_PLAY)
	DEF_CONST(GF_KEY_PREVIOUSCANDIDATE)
	DEF_CONST(GF_KEY_PRINTSCREEN)
	DEF_CONST(GF_KEY_PROCESS)
	DEF_CONST(GF_KEY_PROPS)
	DEF_CONST(GF_KEY_RIGHT)
	DEF_CONST(GF_KEY_ROMANCHARACTERS)
	DEF_CONST(GF_KEY_SCROLL)
	DEF_CONST(GF_KEY_SELECT)
	DEF_CONST(GF_KEY_SELECTMEDIA)
	DEF_CONST(GF_KEY_SHIFT)
	DEF_CONST(GF_KEY_STOP)
	DEF_CONST(GF_KEY_UP)
	DEF_CONST(GF_KEY_UNDO)
	DEF_CONST(GF_KEY_VOLUMEDOWN)
	DEF_CONST(GF_KEY_VOLUMEMUTE)
	DEF_CONST(GF_KEY_VOLUMEUP)
	DEF_CONST(GF_KEY_WIN)
	DEF_CONST(GF_KEY_ZOOM)
	DEF_CONST(GF_KEY_BACKSPACE)
	DEF_CONST(GF_KEY_TAB)
	DEF_CONST(GF_KEY_CANCEL)
	DEF_CONST(GF_KEY_ESCAPE)
	DEF_CONST(GF_KEY_SPACE)
	DEF_CONST(GF_KEY_EXCLAMATION)
	DEF_CONST(GF_KEY_QUOTATION)
	DEF_CONST(GF_KEY_NUMBER)
	DEF_CONST(GF_KEY_DOLLAR)
	DEF_CONST(GF_KEY_AMPERSAND)
	DEF_CONST(GF_KEY_APOSTROPHE)
	DEF_CONST(GF_KEY_LEFTPARENTHESIS)
	DEF_CONST(GF_KEY_RIGHTPARENTHESIS)
	DEF_CONST(GF_KEY_STAR)
	DEF_CONST(GF_KEY_PLUS)
	DEF_CONST(GF_KEY_COMMA)
	DEF_CONST(GF_KEY_HYPHEN)
	DEF_CONST(GF_KEY_FULLSTOP)
	DEF_CONST(GF_KEY_SLASH)
	DEF_CONST(GF_KEY_0)
	DEF_CONST(GF_KEY_1)
	DEF_CONST(GF_KEY_2)
	DEF_CONST(GF_KEY_3)
	DEF_CONST(GF_KEY_4)
	DEF_CONST(GF_KEY_5)
	DEF_CONST(GF_KEY_6)
	DEF_CONST(GF_KEY_7)
	DEF_CONST(GF_KEY_8)
	DEF_CONST(GF_KEY_9)
	DEF_CONST(GF_KEY_COLON)
	DEF_CONST(GF_KEY_SEMICOLON)
	DEF_CONST(GF_KEY_LESSTHAN)
	DEF_CONST(GF_KEY_EQUALS)
	DEF_CONST(GF_KEY_GREATERTHAN)
	DEF_CONST(GF_KEY_QUESTION)
	DEF_CONST(GF_KEY_AT)
	DEF_CONST(GF_KEY_A)
	DEF_CONST(GF_KEY_B)
	DEF_CONST(GF_KEY_C)
	DEF_CONST(GF_KEY_D)
	DEF_CONST(GF_KEY_E)
	DEF_CONST(GF_KEY_F)
	DEF_CONST(GF_KEY_G)
	DEF_CONST(GF_KEY_H)
	DEF_CONST(GF_KEY_I)
	DEF_CONST(GF_KEY_J)
	DEF_CONST(GF_KEY_K)
	DEF_CONST(GF_KEY_L)
	DEF_CONST(GF_KEY_M)
	DEF_CONST(GF_KEY_N)
	DEF_CONST(GF_KEY_O)
	DEF_CONST(GF_KEY_P)
	DEF_CONST(GF_KEY_Q)
	DEF_CONST(GF_KEY_R)
	DEF_CONST(GF_KEY_S)
	DEF_CONST(GF_KEY_T)
	DEF_CONST(GF_KEY_U)
	DEF_CONST(GF_KEY_V)
	DEF_CONST(GF_KEY_W)
	DEF_CONST(GF_KEY_X)
	DEF_CONST(GF_KEY_Y)
	DEF_CONST(GF_KEY_Z)
	DEF_CONST(GF_KEY_LEFTSQUAREBRACKET)
	DEF_CONST(GF_KEY_BACKSLASH)
	DEF_CONST(GF_KEY_RIGHTSQUAREBRACKET)
	DEF_CONST(GF_KEY_CIRCUM)
	DEF_CONST(GF_KEY_UNDERSCORE)
	DEF_CONST(GF_KEY_GRAVEACCENT)
	DEF_CONST(GF_KEY_LEFTCURLYBRACKET)
	DEF_CONST(GF_KEY_PIPE)
	DEF_CONST(GF_KEY_RIGHTCURLYBRACKET)
	DEF_CONST(GF_KEY_DEL)
	DEF_CONST(GF_KEY_INVERTEXCLAMATION)
	DEF_CONST(GF_KEY_DEADGRAVE)
	DEF_CONST(GF_KEY_DEADEACUTE)
	DEF_CONST(GF_KEY_DEADCIRCUM)
	DEF_CONST(GF_KEY_DEADTILDE)
	DEF_CONST(GF_KEY_DEADMACRON)
	DEF_CONST(GF_KEY_DEADBREVE)
	DEF_CONST(GF_KEY_DEADABOVEDOT)
	DEF_CONST(GF_KEY_DEADDIARESIS)
	DEF_CONST(GF_KEY_DEADRINGABOVE)
	DEF_CONST(GF_KEY_DEADDOUBLEACUTE)
	DEF_CONST(GF_KEY_DEADCARON)
	DEF_CONST(GF_KEY_DEADCEDILLA)
	DEF_CONST(GF_KEY_DEADOGONEK)
	DEF_CONST(GF_KEY_DEADIOTA)
	DEF_CONST(GF_KEY_EURO)
	DEF_CONST(GF_KEY_DEADVOICESOUND)
	DEF_CONST(GF_KEY_DEADSEMIVOICESOUND)
	DEF_CONST(GF_KEY_CHANNELUP)
	DEF_CONST(GF_KEY_CHANNELDOWN)
	DEF_CONST(GF_KEY_TEXT)
	DEF_CONST(GF_KEY_INFO)
	DEF_CONST(GF_KEY_EPG)
	DEF_CONST(GF_KEY_RECORD)
	DEF_CONST(GF_KEY_BEGINPAGE)
	DEF_CONST(GF_KEY_CELL_SOFT1)
	DEF_CONST(GF_KEY_CELL_SOFT2)
	DEF_CONST(GF_KEY_JOYSTICK)

	DEF_CONST(GF_KEY_MOD_SHIFT)
	DEF_CONST(GF_KEY_MOD_CTRL)
	DEF_CONST(GF_KEY_MOD_ALT)
	DEF_CONST(GF_KEY_EXT_NUMPAD)
	DEF_CONST(GF_KEY_EXT_LEFT)
	DEF_CONST(GF_KEY_EXT_RIGHT)


    JS_SetPropertyStr(ctx, global_obj, "print", JS_NewCFunction(ctx, js_print, "print", 1));
    JS_SetPropertyStr(ctx, global_obj, "alert", JS_NewCFunction(ctx, js_print, "alert", 1));


	//initialize filter event class
	JS_NewClassID(&jsf_event_class_id);
	JS_NewClass(JS_GetRuntime(ctx), jsf_event_class_id, &jsf_event_class);
	JSValue evt_proto = JS_NewObjectClass(ctx, jsf_event_class_id);
    JS_SetPropertyFunctionList(ctx, evt_proto, jsf_event_funcs, countof(jsf_event_funcs));
    JS_SetClassProto(ctx, jsf_event_class_id, evt_proto);

	JSValue evt_ctor = JS_NewCFunction2(ctx, jsf_event_constructor, "FilterEvent", 1, JS_CFUNC_constructor, 0);
    JS_SetPropertyStr(ctx, global_obj, "FilterEvent", evt_ctor);
}

static GF_Err jsfilter_initialize_ex(GF_Filter *filter, JSContext *custom_ctx)
{
	u8 *buf;
	u32 buf_len;
	u32 flags = JS_EVAL_TYPE_GLOBAL;
    JSValue ret;
    JSValue global_obj;
    u32 i;
    JSRuntime *rt;
	GF_JSFilterCtx *jsf = gf_filter_get_udta(filter);

	if (custom_ctx) {
		GF_SAFEALLOC(jsf, GF_JSFilterCtx);
		filter->filter_udta = jsf;
	}

	jsf->filter = filter;
	jsf->pids = gf_list_new();
	jsf->pck_res = gf_list_new();
	jsf->log_name = gf_strdup(custom_ctx ? filter->name : "JSF");

	if (custom_ctx) {
		jsf->ctx = custom_ctx;
		jsf->is_custom = GF_TRUE;
		global_obj = JS_GetGlobalObject(jsf->ctx);
	} else {
		if (!jsf->js) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[JSF] Missing script file\n"));
			return GF_BAD_PARAM;
		}
		if (!gf_file_exists(jsf->js)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[JSF] Script file %s does not exist\n", jsf->js));
			return GF_BAD_PARAM;
		}
		jsf->filter_obj = JS_UNDEFINED;

		jsf->ctx = gf_js_create_context();
		if (!jsf->ctx) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[JSF] Failed to load QuickJS context\n"));
			return GF_IO_ERR;
		}
		JS_SetContextOpaque(jsf->ctx, jsf);
		global_obj = JS_GetGlobalObject(jsf->ctx);
		js_load_constants(jsf->ctx, global_obj);
	}
	rt = JS_GetRuntime(jsf->ctx);



	//initialize filter class and create a single filter object in global scope
	JS_NewClassID(&jsf_filter_class_id);
	JS_NewClass(rt, jsf_filter_class_id, &jsf_filter_class);

	jsf->filter_obj = JS_NewObjectClass(jsf->ctx, jsf_filter_class_id);
    JS_SetPropertyFunctionList(jsf->ctx, jsf->filter_obj, jsf_filter_funcs, countof(jsf_filter_funcs));
    JS_SetOpaque(jsf->filter_obj, jsf);
    if (!custom_ctx)
		JS_SetPropertyStr(jsf->ctx, global_obj, "filter", jsf->filter_obj);

	//initialize filter instance class
	JS_NewClassID(&jsf_filter_inst_class_id);
	JS_NewClass(rt, jsf_filter_inst_class_id, &jsf_filter_inst_class);
	JSValue finst_proto = JS_NewObjectClass(jsf->ctx, jsf_filter_inst_class_id);
    JS_SetPropertyFunctionList(jsf->ctx, finst_proto, jsf_filter_inst_funcs, countof(jsf_filter_inst_funcs));
    JS_SetClassProto(jsf->ctx, jsf_filter_inst_class_id, finst_proto);

	//initialize filter pid class
	JS_NewClassID(&jsf_pid_class_id);
	JS_NewClass(rt, jsf_pid_class_id, &jsf_pid_class);
	JSValue pid_proto = JS_NewObjectClass(jsf->ctx, jsf_pid_class_id);
    JS_SetPropertyFunctionList(jsf->ctx, pid_proto, jsf_pid_funcs, countof(jsf_pid_funcs));
    JS_SetClassProto(jsf->ctx, jsf_pid_class_id, pid_proto);


	//initialize filter packet class
	JS_NewClassID(&jsf_pck_class_id);
	JS_NewClass(rt, jsf_pck_class_id, &jsf_pck_class);
	JSValue pck_proto = JS_NewObjectClass(jsf->ctx, jsf_pck_class_id);
    JS_SetPropertyFunctionList(jsf->ctx, pck_proto, jsf_pck_funcs, countof(jsf_pck_funcs));
    JS_SetClassProto(jsf->ctx, jsf_pck_class_id, pck_proto);

    if (!custom_ctx)
		JS_SetPropertyStr(jsf->ctx, global_obj, "_gpac_log_name", JS_NewString(jsf->ctx, gf_file_basename(jsf->js) ) );

    JS_FreeValue(jsf->ctx, global_obj);

    if (custom_ctx) return GF_OK;


	//load script
	GF_Err e = gf_file_load_data(jsf->js, &buf, &buf_len);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[JSF] Error loading script file %s: %s\n", jsf->js, gf_error_to_string(e) ));
		return e;
	}
	if (!strstr(buf, "session.") && !strstr(buf, "filter.") ) {
		gf_free(buf);
		return GF_FILTER_NOT_FOUND;
	}

	if (strstr(buf, "session.")) {
		GF_Err gf_fs_load_js_api(JSContext *c, GF_FilterSession *fs);
//		GF_FilterSession *fs = sjs->compositor->filter->session;

		e = gf_fs_load_js_api(jsf->ctx, filter->session);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[JSF] Error loading session API: %s\n", gf_error_to_string(e) ));
			gf_free(buf);
			return e;
		}
		jsf->unload_session_api = GF_TRUE;
	}

 	for (i=0; i<JSF_EVT_LAST_DEFINED; i++) {
        jsf->funcs[i] = JS_UNDEFINED;
    }


 	if (!gf_opts_get_bool("core", "no-js-mods") && JS_DetectModule((char *)buf, buf_len)) {
		qjs_init_all_modules(jsf->ctx, GF_FALSE, GF_FALSE);
		flags = JS_EVAL_TYPE_MODULE;
	}
	jsf->disable_filter = GF_TRUE;
	Bool temp_assign = GF_FALSE;
	if (!jsf->filter->session->js_ctx) {
		jsf->filter->session->js_ctx = jsf->ctx;
		temp_assign = GF_TRUE;
	}
	ret = JS_Eval(jsf->ctx, (char *)buf, buf_len, jsf->js, flags);
	gf_free(buf);
	if (temp_assign)
		jsf->filter->session->js_ctx = NULL;

	if (JS_IsException(ret)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[JSF] Error loading script %s\n", jsf->js));
        js_dump_error(jsf->ctx);
		JS_FreeValue(jsf->ctx, ret);
		return GF_BAD_PARAM;
	}
	JS_FreeValue(jsf->ctx, ret);
	js_std_loop(jsf->ctx);
	return GF_OK;
}


static GF_Err jsfilter_initialize(GF_Filter *filter)
{
	return jsfilter_initialize_ex(filter, NULL);
}

static void jsfilter_finalize(GF_Filter *filter)
{
	u32 i, count;
	GF_JSFilterCtx *jsf = gf_filter_get_udta(filter);
	if (!jsf->ctx) return;

	gf_js_lock(jsf->ctx, GF_TRUE);

	//reset references but do not destroy PIDs yet
	count = gf_list_count(jsf->pids);
	for (i=0; i<count; i++) {
		GF_JSPidCtx *pctx = gf_list_get(jsf->pids, i);
		JS_FreeValue(jsf->ctx, pctx->jsobj);
		if (pctx->shared_pck) {
			while (gf_list_count(pctx->shared_pck)) {
				GF_JSPckCtx *pckc = gf_list_pop_back(pctx->shared_pck);
				JS_FreeValue(jsf->ctx, pckc->ref_val);
				pckc->ref_val = JS_UNDEFINED;
				jsf_pck_detach_ab(jsf->ctx, pckc);

				//do not free here since pck->jsobj may already have been GCed/destroyed
			}
		}
	}

	for (i=0; i<JSF_EVT_LAST_DEFINED; i++) {
		JS_FreeValue(jsf->ctx, jsf->funcs[i]);
	}

	JS_SetOpaque(jsf->filter_obj, NULL);

	if (jsf->is_custom)
		JS_FreeValue(jsf->ctx, jsf->filter_obj);

	gf_js_lock(jsf->ctx, GF_FALSE);

	if (jsf->unload_session_api)
		gf_fs_unload_script(filter->session, jsf->ctx);

	if (!jsf->is_custom) {
		u32 i, count;
		//we created the context, detach all other filters jsvals
		gf_mx_p(jsf->filter->session->filters_mx);
		count = gf_list_count(jsf->filter->session->filters);
		for (i=0; i<count; i++) {
			GF_Filter *a_f = gf_list_get(jsf->filter->session->filters, i);
			if (a_f == jsf->filter) continue;;
			jsfs_on_filter_destroyed(a_f);
		}
		if (!JS_IsUndefined(jsf->filter->jsval)) {
			JS_FreeValue(jsf->ctx, jsf->filter->jsval);
			jsf->filter->jsval = JS_UNDEFINED;
		}
		gf_mx_v(jsf->filter->session->filters_mx);
		gf_js_delete_context(jsf->ctx);
	} else {
		gf_js_call_gc(jsf->ctx);
	}

	while (gf_list_count(jsf->pids)) {
		GF_JSPidCtx *pctx = gf_list_pop_back(jsf->pids);
		if (pctx->shared_pck)
			gf_list_del(pctx->shared_pck);
		gf_free(pctx);
	}
	gf_list_del(jsf->pids);

	if (jsf->log_name) gf_free(jsf->log_name);

	while (gf_list_count(jsf->pck_res)) {
		GF_JSPckCtx *pck = gf_list_pop_back(jsf->pck_res);
		gf_free(pck);
	}
	gf_list_del(jsf->pck_res);

	if (jsf->args) {
		for (i=0; i<jsf->nb_args; i++) {
			if (jsf->args[i].arg_default_val)
				gf_free((char *) jsf->args[i].arg_default_val);
			if (jsf->args[i].arg_desc)
				gf_free((char *) jsf->args[i].arg_desc);
			if (jsf->args[i].arg_name)
				gf_free((char *) jsf->args[i].arg_name);
			if (jsf->args[i].min_max_enum)
				gf_free((char *) jsf->args[i].min_max_enum);
		}
		gf_free(jsf->args);
	}

	if (jsf->caps) gf_free(jsf->caps);
}



static GF_Err jsfilter_update_arg(GF_Filter *filter, const char *arg_name, const GF_PropertyValue *new_val)
{
	GF_JSFilterCtx *jsf = gf_filter_get_udta(filter);
	JSValue ret, val;
	const GF_FilterArgs *the_arg = NULL;
	u32 i;
	GF_Err e = GF_OK;
	if (!jsf->ctx)
		return GF_OK;

	if (!arg_name && !new_val) {
		gf_js_lock(jsf->ctx, GF_TRUE);
		if (JS_IsFunction(jsf->ctx, jsf->funcs[JSF_EVT_INITIALIZE]) ) {
			ret = JS_Call(jsf->ctx, jsf->funcs[JSF_EVT_INITIALIZE], jsf->filter_obj, 0, NULL);
			if (JS_IsException(ret)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[%s] Error initializing filter\n", jsf->log_name));
				js_dump_error(jsf->ctx);
				JS_FreeValue(jsf->ctx, ret);
				gf_js_lock(jsf->ctx, GF_FALSE);
				return GF_BAD_PARAM;
			}
			if (JS_IsInteger(ret))
				JS_ToInt32(jsf->ctx, (int*)&e, ret);

			JS_FreeValue(jsf->ctx, ret);
		}
		jsf->initialized = GF_TRUE;
		//we still call initialize even in help-only mode to properly print links and cap bundles
		if (gf_opts_get_bool("temp", "helponly"))
			jsf->disable_filter = GF_TRUE;

		if (gf_opts_get_key("temp", "gpac-help") || gf_opts_get_bool("temp", "gendoc")) {
			js_std_loop(jsf->ctx);
			jsf->disable_filter = GF_FALSE;
		}
		//filter object not used (no new_pid, post_task or set_cap), disable it
		if (jsf->disable_filter) {
			JSAtom prop;
			JSValue global_obj = JS_GetGlobalObject(jsf->ctx);
			prop = JS_NewAtom(jsf->ctx, "filter");
			JS_DeleteProperty(jsf->ctx, global_obj, prop, 0);
			JS_FreeValue(jsf->ctx, global_obj);
			JS_FreeAtom(jsf->ctx, prop);
			filter->disabled = GF_FILTER_DISABLED_HIDE;
			jsf->filter_obj = JS_UNDEFINED;
		}
		gf_js_lock(jsf->ctx, GF_FALSE);
		return e;
	}
	if (!arg_name || (jsf->initialized && jsf->disable_filter))
		return GF_OK;

	for (i=0; i<jsf->nb_args; i++) {
		if (jsf->args[i].arg_name && !strcmp(jsf->args[i].arg_name, arg_name)) {
			the_arg = &jsf->args[i];
			break;
		}
	}
	if (!the_arg && !jsf->has_wilcard_arg) return GF_OK;

	gf_js_lock(jsf->ctx, GF_TRUE);

	val = jsf_NewProp(jsf->ctx, new_val);
	if (!jsf->initialized) {
    	JS_SetPropertyStr(jsf->ctx, jsf->filter_obj, arg_name, val);
		gf_js_lock(jsf->ctx, GF_FALSE);
		return GF_OK;
	}

	JSValue args[2];
	args[0] = JS_NewString(jsf->ctx, arg_name);
	args[1] = val;
	ret = JS_Call(jsf->ctx, jsf->funcs[JSF_EVT_UPDATE_ARG], jsf->filter_obj, 2, args);
	if (JS_IsException(ret)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[%s] Error updating arg\n", jsf->log_name));
        js_dump_error(jsf->ctx);
		e = GF_BAD_PARAM;
	}
	if (JS_IsInteger(ret))
		JS_ToInt32(jsf->ctx, (int*)&e, ret);
	JS_FreeValue(jsf->ctx, ret);
	JS_FreeValue(jsf->ctx, args[0]);
	JS_FreeValue(jsf->ctx, args[1]);
	gf_js_lock(jsf->ctx, GF_FALSE);
	return e;
}

JSValue js_init_evt_obj(JSContext *ctx, const GF_FilterEvent *evt)
{
	JSValue v = JS_NewObjectClass(ctx, jsf_event_class_id);
	JS_SetOpaque(v, (void *) evt);
	return v;
}

static Bool jsfilter_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	JSValue argv[2];
	JSValue ret;
	Bool canceled=GF_TRUE;
	GF_JSFilterCtx *jsf = gf_filter_get_udta(filter);
	if (!jsf) return GF_TRUE;

	if (!JS_IsFunction(jsf->ctx, jsf->funcs[JSF_EVT_PROCESS_EVENT]))
		return GF_FALSE;

	gf_js_lock(jsf->ctx, GF_TRUE);
	if (evt->base.on_pid) {
		GF_JSPidCtx *pctx = gf_filter_pid_get_udta(evt->base.on_pid);
		argv[0] = pctx ? JS_DupValue(jsf->ctx, pctx->jsobj) : JS_NULL;
	} else {
		argv[0] = JS_NULL;
	}
	argv[1] = js_init_evt_obj(jsf->ctx, evt);

	ret = JS_Call(jsf->ctx, jsf->funcs[JSF_EVT_PROCESS_EVENT], jsf->filter_obj, 2, argv);
	if (JS_IsException(ret)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[%s] Error processing event\n", jsf->log_name));
		js_dump_error(jsf->ctx);
	}
	else {
		canceled = JS_ToBool(jsf->ctx, ret);
	}
	JS_FreeValue(jsf->ctx, ret);
	JS_FreeValue(jsf->ctx, argv[0]);
	JS_SetOpaque(argv[1], NULL);
	JS_FreeValue(jsf->ctx, argv[1]);
	gf_js_lock(jsf->ctx, GF_FALSE);

	js_std_loop(jsf->ctx);
	return canceled;
}

/*static GF_Err jsfilter_reconfigure_output(GF_Filter *filter, GF_FilterPid *PID)
{
	return GF_NOT_SUPPORTED;
}

static GF_FilterProbeScore jsfilter_probe_url(const char *url, const char *mime)
{
	return GF_FPROBE_NOT_SUPPORTED;
}

static const char * jsfilter_probe_data(const u8 *data, u32 size, GF_FilterProbeScore *score)
{
	return NULL;
}
*/

#define OFFS(_n)	#_n, offsetof(GF_JSFilterCtx, _n)
static GF_FilterArgs JSFilterArgs[] =
{
	{ OFFS(js), "location of script source", GF_PROP_NAME, NULL, NULL, 0},
	{ "*", -1, "any possible options defined for the script (see `gpac -hx jsf:js=$YOURSCRIPT` or `gpac -hx $YOURSCRIPT`)", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_META},
	{0}
};

static const GF_FilterCapability JSFilterCaps[] =
{
       CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_UNKNOWN),
       CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_UNKNOWN),
};

GF_FilterRegister JSFilterRegister = {
	.name = "jsf",
	GF_FS_SET_DESCRIPTION("JavaScript filter")
	GF_FS_SET_HELP("This filter runs a javascript file specified in [-js]() defining a new JavaScript filter.\n"
	"  \n"
	"For more information on how to use JS filters, please check https://wiki.gpac.io/jsfilter\n")
	.private_size = sizeof(GF_JSFilterCtx),
	.flags = GF_FS_REG_SCRIPT,
	.args = JSFilterArgs,
	SETCAPS(JSFilterCaps),
	.initialize = jsfilter_initialize,
	.finalize = jsfilter_finalize,
	.configure_pid = jsfilter_configure_pid,
	.process = jsfilter_process,
	.update_arg = jsfilter_update_arg,
	.process_event = jsfilter_process_event,
//	.probe_url = jsfilter_probe_url,
//	.probe_data = jsfilter_probe_data
//	.reconfigure_output = jsfilter_reconfigure_output
};


const GF_FilterRegister *jsfilter_register(GF_FilterSession *session)
{
	return &JSFilterRegister;
}


JSValue jsfilter_initialize_custom(GF_Filter *filter, JSContext *ctx)
{
	GF_JSFilterCtx *jsf;
	GF_Err e = jsfilter_initialize_ex(filter, ctx);
	if (e) return js_throw_err(ctx, e);
	jsf = gf_filter_get_udta(filter);
	((GF_FilterRegister *) filter->freg)->finalize = jsfilter_finalize;
	((GF_FilterRegister *) filter->freg)->process = jsfilter_process;
	((GF_FilterRegister *) filter->freg)->configure_pid = jsfilter_configure_pid;
	((GF_FilterRegister *) filter->freg)->process_event = jsfilter_process_event;
//	((GF_FilterRegister *) filter->freg)->reconfigure_output = jsfilter_reconfigure_output;
//	((GF_FilterRegister *) filter->freg)->probe_data = jsfilter_probe_data;
	//signal reg is script, so we don't free the filter reg caps as with custom filters
	((GF_FilterRegister *) filter->freg)->flags |= GF_FS_REG_SCRIPT;
	return JS_DupValue(ctx, jsf->filter_obj);
}

#else

const GF_FilterRegister *jsfilter_register(GF_FilterSession *session)
{
	return NULL;
}

#endif

