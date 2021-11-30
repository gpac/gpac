/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2020
 *					All rights reserved
 *
 *  This file is part of GPAC / filters sub-project
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terfsess of the GNU Lesser General Public License as published by
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

#include "filter_session.h"

#include <gpac/internal/scenegraph_dev.h>
#include "../scenegraph/qjs_common.h"

#ifdef GPAC_HAS_QJS



static JSClassID fs_class_id;
typedef struct __jsfs_task
{
	JSValue fun;
	JSValue _obj;
	u32 type;
	JSContext *ctx;
} JSFS_Task;

static void jsfs_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func)
{
    GF_FilterSession *fs = JS_GetOpaque(val, fs_class_id);
    if (fs) {
        u32 i, count=gf_list_count(fs->jstasks);
        for (i=0; i<count; i++) {
			JSFS_Task *task = gf_list_get(fs->jstasks, i);
            JS_MarkValue(rt, task->fun, mark_func);
            JS_MarkValue(rt, task->_obj, mark_func);
		}
		gf_fs_lock_filters(fs, GF_TRUE);
		count = gf_list_count(fs->filters);
        for (i=0; i<count; i++) {
			GF_Filter *f = gf_list_get(fs->filters, i);
			if (!JS_IsUndefined(f->jsval))
				JS_MarkValue(rt, f->jsval, mark_func);
		}
		gf_fs_lock_filters(fs, GF_FALSE);
    }
}

static JSClassDef fs_class = {
    "FilterSession",
	.gc_mark = jsfs_mark
};


static JSClassID fs_f_class_id;
static JSClassDef fs_f_class = {
    "Filter",
};

enum
{
	JSFS_NB_FILTERS = 0,
	JSFS_LAST_TASK,
	JSFS_HTTP_MAX_RATE,
	JSFS_HTTP_RATE,
	JSFS_RMT_SAMPLING,
	JSFS_CONNECTED,
	JSFS_LAST_PROCESS_ERR,
	JSFS_LAST_CONNECT_ERR
};

GF_Filter *jsff_get_filter(JSContext *c, JSValue this_val)
{
	return JS_GetOpaque(this_val, fs_f_class_id);
}

GF_FilterSession *jsff_get_session(JSContext *c, JSValue this_val)
{
	return JS_GetOpaque(this_val, fs_class_id);
}

static JSValue jsfs_new_filter_obj(JSContext *ctx, GF_Filter *f);


static void jsfs_exec_task_custom(JSFS_Task *task, const char *text, GF_Filter *new_filter, GF_Filter *del_filter)
{
	JSValue ret, arg;

	gf_js_lock(task->ctx, GF_TRUE);
	if (text) {
		arg = JS_NewString(task->ctx, text);
	} else if (new_filter) {
		arg = jsfs_new_filter_obj(task->ctx, new_filter);
	} else {
		assert(del_filter);
		arg = JS_DupValue(task->ctx, del_filter->jsval);
	}

	ret = JS_Call(task->ctx, task->fun, task->_obj, 1, &arg);
	JS_FreeValue(task->ctx, arg);

	if (del_filter) {
		JS_SetOpaque(del_filter->jsval, NULL);
		JS_FreeValue(task->ctx, del_filter->jsval);
		del_filter->jsval = JS_UNDEFINED;
	}

	if (JS_IsException(ret)) {
		js_dump_error(task->ctx);
	}
	JS_FreeValue(task->ctx, ret);
	js_std_loop(task->ctx);
	gf_js_lock(task->ctx, GF_FALSE);
}

static void jsfs_rmt_user_callback(void *udta, const char* text)
{
	JSFS_Task *task = udta;
	if (!task) return;
	jsfs_exec_task_custom(task, text, NULL, NULL);
}

static JSValue jsfs_prop_get(JSContext *ctx, JSValueConst this_val, int magic)
{
	GF_FilterSession *fs = JS_GetOpaque(this_val, fs_class_id);
	if (!fs)
		return GF_JS_EXCEPTION(ctx);

	switch (magic) {
	case JSFS_NB_FILTERS:
		return JS_NewInt32(ctx, gf_fs_get_filters_count(fs));
	case JSFS_LAST_TASK:
		return gf_fs_is_last_task(fs) ? JS_TRUE : JS_FALSE;
	case JSFS_CONNECTED:
		return fs->pid_connect_tasks_pending ? JS_FALSE : JS_TRUE;

	case JSFS_HTTP_MAX_RATE:
		if (fs->download_manager)
			return JS_NewInt32(ctx, gf_dm_get_data_rate(fs->download_manager) );
		return JS_NULL;

	case JSFS_HTTP_RATE:
		if (fs->download_manager)
			return JS_NewInt32(ctx, gf_dm_get_global_rate(fs->download_manager) );
		return JS_NULL;
	case JSFS_RMT_SAMPLING:
		return JS_NewBool(ctx, gf_sys_profiler_sampling_enabled() );
	case JSFS_LAST_CONNECT_ERR:
		return JS_NewInt32(ctx, gf_fs_get_last_process_error(fs) );
	case JSFS_LAST_PROCESS_ERR:
		return JS_NewInt32(ctx, gf_fs_get_last_connect_error(fs) );
	}
	return JS_UNDEFINED;
}

static JSValue jsfs_prop_set(JSContext *ctx, JSValueConst this_val, JSValueConst value, int magic)
{
	s32 ival;
	GF_FilterSession *fs = JS_GetOpaque(this_val, fs_class_id);
	if (!fs)
		return GF_JS_EXCEPTION(ctx);

	switch (magic) {
	case JSFS_HTTP_MAX_RATE:
		if (fs->download_manager) {
			if (JS_ToInt32(ctx, &ival, value)) return GF_JS_EXCEPTION(ctx);
			gf_dm_set_data_rate(fs->download_manager, (u32) ival);
		}
		break;
	case JSFS_RMT_SAMPLING:
		gf_sys_profiler_enable_sampling(JS_ToBool(ctx, value) ? GF_TRUE : GF_FALSE);
		break;
	}
	return JS_UNDEFINED;
}

static Bool jsfs_task_exec(GF_FilterSession *fs, void *udta, u32 *timeout_ms)
{
	JSValue ret;
	s32 ret_val;
	Bool do_free=GF_TRUE;
	JSFS_Task *task = udta;
	gf_js_lock(task->ctx, GF_TRUE);
	ret = JS_Call(task->ctx, task->fun, task->_obj, 0, NULL);

	*timeout_ms = 0;
	if (JS_IsException(ret)) {
		js_dump_error(task->ctx);
	}
	else if (JS_IsBool(ret)) {
		if (JS_ToBool(task->ctx, ret))
			do_free = GF_FALSE;
	}
	else if (JS_IsInteger(ret)) {
		JS_ToInt32(task->ctx, (int*)&ret_val, ret);
		if (ret_val>=0) {
			*timeout_ms = ret_val;
			do_free = GF_FALSE;
		}
	}

	JS_FreeValue(task->ctx, ret);
	js_std_loop(task->ctx);
	gf_js_lock(task->ctx, GF_FALSE);

	if (do_free) {
		JS_FreeValue(task->ctx, task->fun);
		JS_FreeValue(task->ctx, task->_obj);
		gf_list_del_item(fs->jstasks, task);
		gf_free(task);
		return GF_FALSE;
	}
	return GF_TRUE;
}

static JSValue jsfs_post_task(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JSFS_Task *task;
	const char *tname = NULL;
	GF_FilterSession *fs = JS_GetOpaque(this_val, fs_class_id);
    if (!fs || !argc) return GF_JS_EXCEPTION(ctx);
	if (!JS_IsFunction(ctx, argv[0]) ) return GF_JS_EXCEPTION(ctx);

	GF_SAFEALLOC(task, JSFS_Task);
	if (!task) return GF_JS_EXCEPTION(ctx);
	task->ctx = ctx;

	if (argc>1) {
		tname = JS_ToCString(ctx, argv[1]);
	}
	task->fun = JS_DupValue(ctx, argv[0]);
	task->_obj = JS_DupValue(ctx, this_val);
	gf_list_add(fs->jstasks, task);

	gf_fs_post_user_task(fs, jsfs_task_exec, task, tname ? tname : "task");
	if (tname)
		JS_FreeCString(ctx, tname);

    return JS_UNDEFINED;
}

static JSValue jsfs_abort(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 flush_type = GF_FS_FLUSH_NONE;
	GF_FilterSession *fs = JS_GetOpaque(this_val, fs_class_id);
    if (!fs) return GF_JS_EXCEPTION(ctx);
	if (argc) {
		JS_ToInt32(ctx, &flush_type, argv[0]);
	}
	gf_fs_abort(fs, flush_type);
	return JS_UNDEFINED;
}
static JSValue jsfs_lock_filters(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Bool do_lock;
	GF_FilterSession *fs = JS_GetOpaque(this_val, fs_class_id);
    if (!fs || !argc) return GF_JS_EXCEPTION(ctx);
	if (JS_IsBool(argv[0])) do_lock = JS_ToBool(ctx, argv[0]);
	else return GF_JS_EXCEPTION(ctx);

	gf_fs_lock_filters(fs, do_lock);
	return JS_UNDEFINED;
}


static JSValue jsfs_rmt_send(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *msg;
	GF_FilterSession *fs = JS_GetOpaque(this_val, fs_class_id);
    if (!fs ||!argc) return GF_JS_EXCEPTION(ctx);
    msg = JS_ToCString(ctx, argv[0]);
    if (!msg) return GF_JS_EXCEPTION(ctx);
	gf_sys_profiler_send(msg);
	JS_FreeCString(ctx, msg);
	return JS_UNDEFINED;
}


void jsfs_on_filter_created(GF_Filter *new_filter)
{
	new_filter->jsval = JS_UNDEFINED;
	if (!new_filter->session->new_f_task) return;
	jsfs_exec_task_custom(new_filter->session->new_f_task, NULL, new_filter, NULL);
}

void jsfs_on_filter_destroyed(GF_Filter *del_filter)
{
	if (! JS_IsUndefined(del_filter->jsval)) {
		void *p = JS_GetOpaque(del_filter->jsval, fs_f_class_id);
		if (!p) return;
		if (del_filter->session->del_f_task) {
			jsfs_exec_task_custom(del_filter->session->del_f_task, NULL, NULL, del_filter);
		} else {
			JSRuntime *gf_js_get_rt();
			JSRuntime *rt = gf_js_get_rt();
			if (rt) {
				gf_js_lock(NULL, GF_TRUE);
				JS_FreeValueRT(rt, del_filter->jsval);
				gf_js_lock(NULL, GF_FALSE);
			}
		}
		del_filter->jsval = JS_UNDEFINED;
	}
}

Bool jsfs_on_event(GF_FilterSession *fs, GF_Event *evt)
{
	GF_FilterEvent fevt;
	JSValue js_init_evt_obj(JSContext *ctx, const GF_FilterEvent *evt);

	JSValue arg, ret;
	Bool res;
	assert(fs->on_evt_task);
	gf_js_lock(fs->on_evt_task->ctx, GF_TRUE);

	memset(&fevt, 0, sizeof(GF_FilterEvent));
	fevt.user_event.event = *evt;
	fevt.base.type = GF_FEVT_USER;

	arg = js_init_evt_obj(fs->on_evt_task->ctx, &fevt);

	ret = JS_Call(fs->on_evt_task->ctx, fs->on_evt_task->fun, fs->on_evt_task->_obj, 1, &arg);
	JS_SetOpaque(arg, NULL);
	JS_FreeValue(fs->on_evt_task->ctx, arg);

	if (JS_IsException(ret)) {
		js_dump_error(fs->on_evt_task->ctx);
	}
	fevt.user_event.event.type = evt->type;
	*evt = fevt.user_event.event;
	res = JS_ToBool(fs->on_evt_task->ctx, ret) ? GF_TRUE : GF_FALSE;
	if (!res && (evt->type==GF_EVENT_COPY_TEXT) && evt->clipboard.text) {
		gf_free(evt->clipboard.text);
		evt->clipboard.text = NULL;
	}
	JS_FreeValue(fs->on_evt_task->ctx, ret);
	js_std_loop(fs->on_evt_task->ctx);
	gf_js_lock(fs->on_evt_task->ctx, GF_FALSE);
	return res;
}


static JSValue jsfs_set_fun_callback(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, u32 cbk_type)
{
	JSFS_Task *task = NULL;
	u32 i, count;
	Bool is_rem = GF_FALSE;
	GF_FilterSession *fs = JS_GetOpaque(this_val, fs_class_id);
    if (!fs || !argc) return GF_JS_EXCEPTION(ctx);

    if (JS_IsNull(argv[0]))
		is_rem = GF_TRUE;
	else if (!JS_IsFunction(ctx, argv[0]) )
		return GF_JS_EXCEPTION(ctx);

	if (cbk_type==2) {
		task = fs->new_f_task;
	} else if (cbk_type==3) {
		task = fs->del_f_task;
	} else if (cbk_type==4) {
		task = fs->on_evt_task;
	} else {
		count = gf_list_count(fs->jstasks);
		for (i=0; i<count; i++) {
			task = gf_list_get(fs->jstasks, i);
			if (task->type==1) break;
			task = NULL;
		}
	}
	if (is_rem) {
		if (task) {
			JS_FreeValue(ctx, task->fun);
			JS_FreeValue(ctx, task->_obj);
			gf_list_del_item(fs->jstasks, task);
			gf_free(task);
		}
		if (cbk_type == 1)
			gf_sys_profiler_set_callback(task, NULL);
		else if (cbk_type == 2)
			fs->new_f_task = NULL;
		else if (cbk_type == 3)
			fs->del_f_task = NULL;
		else if (cbk_type == 4)
			fs->on_evt_task = NULL;

		return JS_UNDEFINED;
	}

	if (task) {
		JS_FreeValue(ctx, task->fun);
		JS_FreeValue(ctx, task->_obj);
	} else {
		GF_SAFEALLOC(task, JSFS_Task);
		if (!task) return GF_JS_EXCEPTION(ctx);
		gf_list_add(fs->jstasks, task);
		task->type = cbk_type;
		task->ctx = ctx;
	}
	task->fun = JS_DupValue(ctx, argv[0]);
	task->_obj = JS_DupValue(ctx, this_val);

	if (cbk_type == 1) {
		gf_sys_profiler_set_callback(task, jsfs_rmt_user_callback);
#ifdef GPAC_ENABLE_COVERAGE
		if (gf_sys_is_cov_mode()) {
			jsfs_rmt_user_callback(task, "test");
		}
#endif
	}
	else if (cbk_type == 2)
		fs->new_f_task = task;
	else if (cbk_type == 3)
		fs->del_f_task = task;
	else if (cbk_type == 4)
		fs->on_evt_task = task;
    return JS_UNDEFINED;
}

static JSValue jsfs_set_rmt_fun(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return jsfs_set_fun_callback(ctx, this_val, argc, argv, 1);
}
static JSValue jsfs_set_new_filter_fun(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return jsfs_set_fun_callback(ctx, this_val, argc, argv, 2);
}
static JSValue jsfs_set_del_filter_fun(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return jsfs_set_fun_callback(ctx, this_val, argc, argv, 3);
}
static JSValue jsfs_set_event_fun(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return jsfs_set_fun_callback(ctx, this_val, argc, argv, 4);
}

enum
{
	JSFF_NAME = 0,
	JSFF_TYPE,
	JSFF_ID,
	JSFF_NB_IPID,
	JSFF_NB_OPID,
	JSFF_STATUS,
	JSFF_ALIAS,
	JSFF_ARGS,
	JSFF_DYNAMIC,
	JSFF_REMOVED,
	JSFF_PCK_DONE,
	JSFF_BYTES_DONE,
	JSFF_TIME,
	JSFF_PCK_SENT_IFCE,
	JSFF_PCK_SENT,
	JSFF_BYTES_SENT,
	JSFF_NB_TASKS,
	JSFF_NB_ERRORS,
	JSFF_REPORT_UPDATED,
	JSFF_CLASS,
	JSFF_CODEC,
	JSFF_STREAMTYPE,
	JSFF_INAME,
	JSFF_EVENT_TARGET,
	JSFF_LAST_TS_SENT,
	JSFF_LAST_TS_DROP,
};


static JSValue jsfs_f_prop_get(JSContext *ctx, JSValueConst this_val, int magic)
{
	const char *val_s;
	Bool val_b;
	JSValue res;
	GF_FilterStats stats;
	GF_Filter *f = JS_GetOpaque(this_val, fs_f_class_id);
	if (!f)
		return GF_JS_EXCEPTION(ctx);

	switch (magic) {
	case JSFF_NAME:
		return JS_NewString(ctx, gf_filter_get_name(f) );
	case JSFF_TYPE:
		return JS_NewString(ctx, f->freg->name );
	case JSFF_ID:
		val_s = gf_filter_get_id(f);
		if (!val_s) return JS_NULL;
		return JS_NewString(ctx, val_s);
	case JSFF_NB_IPID:
		return JS_NewInt32(ctx, gf_filter_get_ipid_count(f) );
	case JSFF_NB_OPID:
		return JS_NewInt32(ctx, gf_filter_get_opid_count(f) );
	case JSFF_STATUS:
		return JS_NewString(ctx, f->status_str ? f->status_str : "");
	case JSFF_ALIAS:
		return JS_NewBool(ctx, f->multi_sink_target ? 1 : 0);
	case JSFF_ARGS:
		return JS_NewString(ctx, f->src_args ? f->src_args : "");
	case JSFF_DYNAMIC:
		return JS_NewBool(ctx, f->dynamic_filter);
	case JSFF_REMOVED:
		return JS_NewBool(ctx, (f->removed || f->finalized));

	case JSFF_PCK_DONE:
		return JS_NewInt64(ctx, f->nb_pck_processed);
	case JSFF_BYTES_DONE:
		return JS_NewInt64(ctx, f->nb_bytes_processed);
	case JSFF_TIME:
		return JS_NewInt64(ctx, f->time_process);
	case JSFF_PCK_SENT_IFCE:
		return JS_NewInt64(ctx, f->nb_hw_pck_sent);
	case JSFF_PCK_SENT:
		return JS_NewInt64(ctx, f->nb_pck_sent);
	case JSFF_BYTES_SENT:
		return JS_NewInt64(ctx, f->nb_bytes_sent);
	case JSFF_NB_TASKS:
		return JS_NewInt64(ctx, f->nb_tasks_done);
	case JSFF_NB_ERRORS:
		return JS_NewInt64(ctx, f->nb_errors);
	case JSFF_REPORT_UPDATED:
		val_b = f->report_updated;
		f->report_updated = GF_FALSE;
		return JS_NewBool(ctx, val_b);
	case JSFF_CLASS:
		gf_filter_get_stats(f, &stats);
		switch (stats.type) {
		case GF_FS_STATS_FILTER_RAWIN: return JS_NewString(ctx, "rawin");
		case GF_FS_STATS_FILTER_DEMUX: return JS_NewString(ctx, "demuxer");
		case GF_FS_STATS_FILTER_DECODE: return JS_NewString(ctx, "decoder");
		case GF_FS_STATS_FILTER_ENCODE: return JS_NewString(ctx, "encoder");
		case GF_FS_STATS_FILTER_MUX: return JS_NewString(ctx, "muxer");
		case GF_FS_STATS_FILTER_RAWOUT: return JS_NewString(ctx, "rawout");
		case GF_FS_STATS_FILTER_MEDIA_SINK: return JS_NewString(ctx, "mediasink");
		case GF_FS_STATS_FILTER_MEDIA_SOURCE: return JS_NewString(ctx, "mediasource");
		default: return JS_NewString(ctx, "unknown");
		}
	case JSFF_CODEC:
		gf_filter_get_stats(f, &stats);
		if (stats.codecid) {
			return JS_NewString(ctx, gf_codecid_name(stats.codecid));
		}
		return JS_NewString(ctx, "unknown");
	case JSFF_STREAMTYPE:
		gf_filter_get_stats(f, &stats);
		if (stats.stream_type) {
			return JS_NewString(ctx, gf_stream_type_name(stats.stream_type));
		}
		return JS_NewString(ctx, "unknown");

	case JSFF_LAST_TS_SENT:
		gf_filter_get_stats(f, &stats);
		if (!stats.last_ts_sent.den) return JS_NULL;
		res = JS_NewObject(ctx);
		JS_SetPropertyStr(ctx, res, "n", JS_NewInt64(ctx, stats.last_ts_sent.num));
		JS_SetPropertyStr(ctx, res, "d", JS_NewInt64(ctx, stats.last_ts_sent.den));
		return res;

	case JSFF_LAST_TS_DROP:
		gf_filter_get_stats(f, &stats);
		if (!stats.last_ts_drop.den) return JS_NULL;
		res = JS_NewObject(ctx);
		JS_SetPropertyStr(ctx, res, "n", JS_NewInt64(ctx, stats.last_ts_drop.num));
		JS_SetPropertyStr(ctx, res, "d", JS_NewInt64(ctx, stats.last_ts_drop.den));
		return res;

	case JSFF_INAME:
		if (f->iname) return JS_NewString(ctx, f->iname);
		return JS_NULL;
	case JSFF_EVENT_TARGET:
		return JS_NewBool(ctx, f->event_target);
	}
	return JS_UNDEFINED;
}

static JSValue jsfs_f_prop_set(JSContext *ctx, JSValueConst this_val, JSValueConst value, int magic)
{
	const char *s_val;
#ifdef GPAC_ENABLE_COVERAGE
	GF_Filter *f = jsff_get_filter(ctx, this_val);
#else
	GF_Filter *f = JS_GetOpaque(this_val, fs_f_class_id);
#endif
	if (!f)
		return GF_JS_EXCEPTION(ctx);

	switch (magic) {
	case JSFF_INAME:
		s_val = JS_ToCString(ctx, value);
		if (f->iname) gf_free(f->iname);
		f->iname = s_val ? gf_strdup(s_val) : NULL;
		JS_FreeCString(ctx, s_val);
		break;
	}
	return JS_UNDEFINED;
}


static JSValue jsff_is_destroyed(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Filter *f = JS_GetOpaque(this_val, fs_f_class_id);
	if (!f)
		return JS_TRUE;
	return JS_FALSE;
}

JSValue jsf_NewProp(JSContext *ctx, const GF_PropertyValue *new_val);
JSValue jsf_NewPropTranslate(JSContext *ctx, const GF_PropertyValue *prop, u32 p4cc);

static JSValue jsff_enum_pid_props(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, Bool is_output)
{
	u32 idx;
	GF_FilterPid *pid;
	const char *pname=NULL;
	GF_Filter *f = JS_GetOpaque(this_val, fs_f_class_id);
	if (!f || (argc!=2) )
		return GF_JS_EXCEPTION(ctx);

	if (JS_IsString(argv[1])) {
		pname = JS_ToCString(ctx, argv[1]);
		if (!pname) return GF_JS_EXCEPTION(ctx);
	}
	else if (!JS_IsFunction(ctx, argv[1]))
		return GF_JS_EXCEPTION(ctx);

	if (JS_ToInt32(ctx, &idx, argv[0]))
		return GF_JS_EXCEPTION(ctx);

	if (is_output) {
		pid = gf_filter_get_opid(f, idx);
		if (!pid) return GF_JS_EXCEPTION(ctx);
	} else {
		pid = gf_filter_get_ipid(f, idx);
		if (!pid) return GF_JS_EXCEPTION(ctx);
	}

	if (pname) {
		const GF_PropertyValue *p;

		if (!strcmp(pname, "buffer")) {
			JS_FreeCString(ctx, pname);
			return JS_NewInt64(ctx, gf_filter_pid_query_buffer_duration(pid, GF_FALSE) );
		}
		if (!strcmp(pname, "buffer_total")) {
			JS_FreeCString(ctx, pname);
			return JS_NewInt64(ctx, gf_filter_pid_query_buffer_duration(pid, GF_TRUE) );
		}
		if (!strcmp(pname, "name")) {
			JS_FreeCString(ctx, pname);
			return JS_NewString(ctx, pid->pid->name);
		}
		if (!strcmp(pname, "eos")) {
			JS_FreeCString(ctx, pname);
			return JS_NewBool(ctx, gf_filter_pid_is_eos(pid) );
		}
		u32 p4cc = gf_props_get_id(pname);
		if (p4cc)
			p = gf_filter_pid_get_property(pid, p4cc);
		else
			p = gf_filter_pid_get_property_str(pid, pname);

		JS_FreeCString(ctx, pname);
		if (p && p4cc)
			return jsf_NewPropTranslate(ctx, p, p4cc);

		return jsf_NewProp(ctx, p);
	}

	idx = 0;
	while (1) {
		u32 prop_4cc;
		const char *prop_name;
		JSValue args[3], ret;
		const GF_PropertyValue *prop = gf_filter_pid_enum_properties(pid, &idx, &prop_4cc, &prop_name);
		if (!prop) break;
		if (prop->type==GF_PROP_POINTER) continue;

		args[0] = JS_NewString(ctx, prop_name ? prop_name : gf_props_4cc_get_name(prop_4cc) );
		args[1] = JS_NewString(ctx, gf_props_get_type_name(prop->type) );
		if (prop_4cc) {
			args[2] = jsf_NewPropTranslate(ctx, prop, prop_4cc);
		} else {
			args[2] = jsf_NewProp(ctx, prop);
		}
		ret = JS_Call(ctx, argv[1], this_val, 3, args);
		if (JS_IsException(ret)) {
			js_dump_error(ctx);
		}
		JS_FreeValue(ctx, ret);
		JS_FreeValue(ctx, args[0]);
		JS_FreeValue(ctx, args[1]);
		JS_FreeValue(ctx, args[2]);
	}
	return JS_UNDEFINED;
}
static JSValue jsff_enum_ipid_props(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return jsff_enum_pid_props(ctx, this_val, argc, argv, GF_FALSE);
}
static JSValue jsff_enum_opid_props(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return jsff_enum_pid_props(ctx, this_val, argc, argv, GF_TRUE);
}


static JSValue jsff_get_pid_source(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 idx;
	GF_FilterPid *pid;
	GF_FilterPidInst *ipid;
	GF_Filter *f = JS_GetOpaque(this_val, fs_f_class_id);
	if (!f || (argc!=1) )
		return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt32(ctx, &idx, argv[0]))
		return GF_JS_EXCEPTION(ctx);

	pid = gf_filter_get_ipid(f, idx);
	if (!pid) return JS_NULL;

	ipid = (GF_FilterPidInst *)pid;
	return jsfs_new_filter_obj(ctx, ipid->pid->filter);
}

static JSValue jsff_get_pid_sinks(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 idx;
	JSValue ret;
	GF_FilterPid *pid;
	GF_Filter *f = JS_GetOpaque(this_val, fs_f_class_id);
	if (!f || (argc!=1) )
		return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt32(ctx, &idx, argv[0]))
		return GF_JS_EXCEPTION(ctx);

	pid = gf_filter_get_opid(f, idx);
	if (!pid) return JS_NULL;

	ret = JS_NewArray(ctx);
	JS_SetPropertyStr(ctx, ret, "length", JS_NewInt32(ctx, pid->num_destinations) );

	for (idx=0; idx<pid->num_destinations; idx++) {
		GF_FilterPidInst *pid_inst = gf_list_get(pid->destinations, idx);
		JS_SetPropertyUint32(ctx, ret, idx, jsfs_new_filter_obj(ctx, pid_inst->filter) );
	}
	return ret;
}

static JSValue jsff_all_args(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 idx, a_idx;
	JSValue res;
	Bool val_only = GF_TRUE;
	const GF_FilterArgs *args;
	GF_Filter *f = JS_GetOpaque(this_val, fs_f_class_id);
	if (!f)
		return GF_JS_EXCEPTION(ctx);

	if (argc && JS_IsBool(argv[0]) && JS_ToBool(ctx, argv[0]))
		val_only = GF_FALSE;

	res = JS_NewArray(ctx);
	idx=0;
	a_idx=0;
	args = f->instance_args ? f->instance_args : f->freg->args;
	while (args && args[idx].arg_name) {
		const GF_FilterArgs *arg = &args[idx];
		GF_PropertyValue p;
		JSValue aval;
		if (arg->flags & GF_FS_ARG_HINT_HIDE) {
			idx++;
			continue;
		}

		aval = JS_NewObject(ctx);
		JS_SetPropertyStr(ctx, aval, "name", JS_NewString(ctx, arg->arg_name) );
		if (f->instance_args) {
			char szArgName[100];
			char *arg_start;
			snprintf(szArgName, 100, "%c%s%c", f->session->sep_args, arg->arg_name, f->session->sep_name);
			arg_start = f->orig_args ? strstr(f->orig_args, szArgName) : NULL;
			if (arg_start) {
				char *arg_end;
				arg_start += strlen(szArgName);
				arg_start = gf_strdup(arg_start);
				arg_end = strchr(arg_start, f->session->sep_args);
				if (arg_end) arg_end[0] = 0;
				JS_SetPropertyStr(ctx, aval, "value", JS_NewString(ctx, arg_start) );
				gf_free(arg_start);
			} else {
				snprintf(szArgName, 100, "%c%s", f->session->sep_args, arg->arg_name);
				arg_start = f->orig_args ? strstr(f->orig_args, szArgName) : NULL;
				if (arg_start && (arg->arg_type==GF_PROP_BOOL)) {
					p.type = GF_PROP_BOOL;
					p.value.boolean = GF_TRUE;
					JS_SetPropertyStr(ctx, aval, "value", jsf_NewProp(ctx, &p) );
				} else {
					snprintf(szArgName, 100, "%c%c%s", f->session->sep_args, f->session->sep_neg, arg->arg_name);
					arg_start = f->orig_args ? strstr(f->orig_args, szArgName) : NULL;
					if (arg_start && (arg->arg_type==GF_PROP_BOOL)) {
						p.type = GF_PROP_BOOL;
						p.value.boolean = GF_FALSE;
						JS_SetPropertyStr(ctx, aval, "value", jsf_NewProp(ctx, &p) );
					} else {
						if (arg->arg_default_val) {
							gf_props_parse_value(arg->arg_type, arg->arg_name, arg->arg_default_val, arg->min_max_enum, f->session->sep_list);
							JS_SetPropertyStr(ctx, aval, "value", jsf_NewProp(ctx, &p) );
						} else {
							JS_SetPropertyStr(ctx, aval, "value", JS_NULL);
						}
					}
				}
			}
		} else {
			gf_filter_get_arg(f, arg->arg_name, &p);
			JS_SetPropertyStr(ctx, aval, "value", jsf_NewProp(ctx, &p) );
		}
		if (!val_only) {
			if (arg->arg_desc)
				JS_SetPropertyStr(ctx, aval, "desc", JS_NewString(ctx, arg->arg_desc) );
			if (arg->min_max_enum)
				JS_SetPropertyStr(ctx, aval, "min_max_enum", JS_NewString(ctx, arg->min_max_enum) );
			if (arg->arg_default_val)
				JS_SetPropertyStr(ctx, aval, "default", JS_NewString(ctx, arg->arg_default_val) );
			JS_SetPropertyStr(ctx, aval, "update", JS_NewBool(ctx, (arg->flags & GF_FS_ARG_UPDATE) ) );
			JS_SetPropertyStr(ctx, aval, "update_sync", JS_NewBool(ctx, (arg->flags & GF_FS_ARG_UPDATE_SYNC) ) );
			if (arg->flags & GF_FS_ARG_HINT_ADVANCED) {
				JS_SetPropertyStr(ctx, aval, "hint", JS_NewString(ctx, "advanced") );
			}
			else if (arg->flags & GF_FS_ARG_HINT_EXPERT) {
				JS_SetPropertyStr(ctx, aval, "hint", JS_NewString(ctx, "expert") );
			}
		}
		JS_SetPropertyUint32(ctx, res, idx, aval);
		idx++;
		a_idx++;
	}
	return res;
}

static JSValue jsff_get_arg(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 idx;
	const char *aname=NULL;
	const GF_FilterArgs *args;
	GF_Filter *f = JS_GetOpaque(this_val, fs_f_class_id);
	if (!f || !argc)
		return GF_JS_EXCEPTION(ctx);

	aname = JS_ToCString(ctx, argv[0]);
	if (!aname) return GF_JS_EXCEPTION(ctx);

	idx=0;
	args = f->freg->args;
	while (args && args[idx].arg_name) {
		const GF_FilterArgs *arg = &args[idx];
		GF_PropertyValue p;
		if (strcmp(arg->arg_name, aname)) {
			idx++;
			continue;
		}
		if (gf_filter_get_arg(f, arg->arg_name, &p)) {
			JS_FreeCString(ctx, aname);
			return jsf_NewProp(ctx, &p);
		}
		break;
	}
	JS_FreeCString(ctx, aname);
	return JS_NULL;
}


GF_Err jsf_ToProp_ex(GF_Filter *filter, JSContext *ctx, JSValue value, u32 p4cc, GF_PropertyValue *prop, u32 prop_type);

static JSValue jsff_update(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *aname;
	GF_Filter *f = JS_GetOpaque(this_val, fs_f_class_id);
	if (!f || (argc!=2) )
		return GF_JS_EXCEPTION(ctx);

	aname = JS_ToCString(ctx, argv[0]);
	if (!aname) return GF_JS_EXCEPTION(ctx);
	if (JS_IsString(argv[1])) {
		const char *aval = JS_ToCString(ctx, argv[1]);
		if (!aval) {
			JS_FreeCString(ctx, aname);
			return GF_JS_EXCEPTION(ctx);
		}
		gf_fs_send_update(f->session, NULL, f, aname, aval, 0);
		JS_FreeCString(ctx, aval);
		JS_FreeCString(ctx, aname);
		return JS_UNDEFINED;
	} else {
		char szDump[GF_PROP_DUMP_ARG_SIZE];
		GF_PropertyValue p;
		GF_Err e;
		if (! gf_filter_get_arg(f, aname, &p)) {
			JSValue err = js_throw_err_msg(ctx, GF_BAD_PARAM, "Argument %s not defined in filter %s", aname, f->freg->name);
			JS_FreeCString(ctx, aname);
			return err;
		}
		e = jsf_ToProp_ex(f, ctx, argv[1], 0, &p, p.type);
		if (e) {
			JSValue err = js_throw_err_msg(ctx, GF_BAD_PARAM, "Failed to parse argument %s", aname);
			JS_FreeCString(ctx, aname);
			return err;
		}
		gf_props_dump_val(&p, szDump, GF_PROP_DUMP_DATA_PTR, NULL);
		gf_fs_send_update(f->session, NULL, f, aname, szDump, 0);
		gf_props_reset_single(&p);
	}

	JS_FreeCString(ctx, aname);
	return JS_UNDEFINED;
}

static JSValue jsff_lock(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Filter *f = JS_GetOpaque(this_val, fs_f_class_id);
	if (!f || (argc!=1) )
		return GF_JS_EXCEPTION(ctx);

	if (JS_ToBool(ctx, argv[0]))
		gf_filter_lock(f, GF_TRUE);
	else
		gf_filter_lock(f, GF_FALSE);

	return JS_UNDEFINED;
}

static JSValue jsff_remove(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Filter *f = JS_GetOpaque(this_val, fs_f_class_id);
	if (!f)
		return GF_JS_EXCEPTION(ctx);

	gf_filter_remove(f);
	return JS_UNDEFINED;
}

static JSValue jsff_insert_filter(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *fname, *link_args;
	GF_Filter *new_f;
	GF_Err e;
	Bool is_source = GF_FALSE;
	GF_Filter *f = JS_GetOpaque(this_val, fs_f_class_id);
	if (!f || !argc)
		return GF_JS_EXCEPTION(ctx);

	fname = JS_ToCString(ctx, argv[0]);
	if (!fname) return GF_JS_EXCEPTION(ctx);
	link_args = NULL;
	if (argc>1) {
		link_args = JS_ToCString(ctx, argv[1]);
		if (!link_args) {
			JS_FreeCString(ctx, fname);
			return GF_JS_EXCEPTION(ctx);
		}
	}
	gf_fs_lock_filters(f->session, GF_TRUE);
	if (!strncmp(fname, "src=", 4)) {
		new_f = gf_fs_load_source(f->session, fname+4, NULL, NULL, &e);
		is_source = GF_TRUE;
	} else if (!strncmp(fname, "dst=", 4)) {
		new_f = gf_fs_load_destination(f->session, fname+4, NULL, NULL, &e);
	} else {
		new_f = gf_fs_load_filter(f->session, fname, &e);
	}

	if (!new_f) {
		JSValue ret = js_throw_err_msg(ctx, e, "Cannot load filter %s: %s\n", fname, gf_error_to_string(e));
		JS_FreeCString(ctx, fname);
		if (link_args) JS_FreeCString(ctx, link_args);
		gf_fs_lock_filters(f->session, GF_FALSE);
		return ret;
	}
	if (is_source)
		gf_filter_set_source_restricted(new_f, (GF_Filter *) f, link_args);
	else
		gf_filter_set_source(new_f, (GF_Filter *) f, link_args);

	gf_fs_lock_filters(f->session, GF_FALSE);
	
	//reconnect outputs of source
	gf_filter_reconnect_output((GF_Filter *) f);

	JS_FreeCString(ctx, fname);
	if (link_args) JS_FreeCString(ctx, link_args);

	return jsfs_new_filter_obj(ctx, new_f);
}

static JSValue jsff_bind(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Filter *f = JS_GetOpaque(this_val, fs_f_class_id);
	if (!f || !argc)
		return GF_JS_EXCEPTION(ctx);
	if (!JS_IsObject(argv[0]) && !JS_IsNull(argv[0]))
		return GF_JS_EXCEPTION(ctx);
	if (!f->freg)
		return GF_JS_EXCEPTION(ctx);

	if (!strcmp(f->freg->name, "dashin")) {
		JSValue dashdmx_bind_js(GF_Filter *f, JSContext *jsctx, JSValueConst obj);
		return dashdmx_bind_js(f, ctx, argv[0]);
	}

	return js_throw_err_msg(ctx, GF_BAD_PARAM, "filter class %s has no JS bind capabilities", f->freg->name);
}

#define JS_CGETSET_MAGIC_DEF_ENUM(name, fgetter, fsetter, magic) { name, JS_PROP_CONFIGURABLE|JS_PROP_ENUMERABLE, JS_DEF_CGETSET_MAGIC, magic, .u = { .getset = { .get = { .getter_magic = fgetter }, .set = { .setter_magic = fsetter } } } }

static const JSCFunctionListEntry fs_f_funcs[] = {
	JS_CGETSET_MAGIC_DEF_ENUM("name", jsfs_f_prop_get, NULL, JSFF_NAME),
	JS_CGETSET_MAGIC_DEF_ENUM("type", jsfs_f_prop_get, NULL, JSFF_TYPE),
	JS_CGETSET_MAGIC_DEF_ENUM("ID", jsfs_f_prop_get, NULL, JSFF_ID),
	JS_CGETSET_MAGIC_DEF_ENUM("nb_ipid", jsfs_f_prop_get, NULL, JSFF_NB_IPID),
	JS_CGETSET_MAGIC_DEF_ENUM("nb_opid", jsfs_f_prop_get, NULL, JSFF_NB_OPID),
	JS_CGETSET_MAGIC_DEF_ENUM("status", jsfs_f_prop_get, NULL, JSFF_STATUS),
	JS_CGETSET_MAGIC_DEF_ENUM("alias", jsfs_f_prop_get, NULL, JSFF_ALIAS),
	JS_CGETSET_MAGIC_DEF_ENUM("args", jsfs_f_prop_get, NULL, JSFF_ARGS),
	JS_CGETSET_MAGIC_DEF_ENUM("dynamic", jsfs_f_prop_get, NULL, JSFF_DYNAMIC),
	JS_CGETSET_MAGIC_DEF_ENUM("done", jsfs_f_prop_get, NULL, JSFF_REMOVED),
	JS_CGETSET_MAGIC_DEF_ENUM("pck_done", jsfs_f_prop_get, NULL, JSFF_PCK_DONE),
	JS_CGETSET_MAGIC_DEF_ENUM("bytes_done", jsfs_f_prop_get, NULL, JSFF_BYTES_DONE),
	JS_CGETSET_MAGIC_DEF_ENUM("time", jsfs_f_prop_get, NULL, JSFF_TIME),
	JS_CGETSET_MAGIC_DEF_ENUM("pck_sent", jsfs_f_prop_get, NULL, JSFF_PCK_SENT),
	JS_CGETSET_MAGIC_DEF_ENUM("pck_ifce_sent", jsfs_f_prop_get, NULL, JSFF_PCK_SENT_IFCE),
	JS_CGETSET_MAGIC_DEF_ENUM("bytes_sent", jsfs_f_prop_get, NULL, JSFF_BYTES_SENT),
	JS_CGETSET_MAGIC_DEF_ENUM("tasks", jsfs_f_prop_get, NULL, JSFF_NB_TASKS),
	JS_CGETSET_MAGIC_DEF_ENUM("errors", jsfs_f_prop_get, NULL, JSFF_NB_ERRORS),
	JS_CGETSET_MAGIC_DEF_ENUM("report_updated", jsfs_f_prop_get, NULL, JSFF_REPORT_UPDATED),
	JS_CGETSET_MAGIC_DEF_ENUM("class", jsfs_f_prop_get, NULL, JSFF_CLASS),
	JS_CGETSET_MAGIC_DEF_ENUM("streamtype", jsfs_f_prop_get, NULL, JSFF_STREAMTYPE),
	JS_CGETSET_MAGIC_DEF_ENUM("codec", jsfs_f_prop_get, NULL, JSFF_CODEC),
	JS_CGETSET_MAGIC_DEF_ENUM("iname", jsfs_f_prop_get, jsfs_f_prop_set, JSFF_INAME),
	JS_CGETSET_MAGIC_DEF_ENUM("event_target", jsfs_f_prop_get, NULL, JSFF_EVENT_TARGET),
	JS_CGETSET_MAGIC_DEF_ENUM("last_ts_sent", jsfs_f_prop_get, NULL, JSFF_LAST_TS_SENT),
	JS_CGETSET_MAGIC_DEF_ENUM("last_ts_drop", jsfs_f_prop_get, NULL, JSFF_LAST_TS_DROP),

	JS_CFUNC_DEF("is_destroyed", 0, jsff_is_destroyed),
	JS_CFUNC_DEF("ipid_props", 0, jsff_enum_ipid_props),
	JS_CFUNC_DEF("opid_props", 0, jsff_enum_opid_props),
	JS_CFUNC_DEF("ipid_source", 0, jsff_get_pid_source),
	JS_CFUNC_DEF("opid_sinks", 0, jsff_get_pid_sinks),
	JS_CFUNC_DEF("all_args", 0, jsff_all_args),
	JS_CFUNC_DEF("get_arg", 0, jsff_get_arg),
	JS_CFUNC_DEF("update", 0, jsff_update),
	JS_CFUNC_DEF("remove", 0, jsff_remove),
	JS_CFUNC_DEF("insert", 0, jsff_insert_filter),
	JS_CFUNC_DEF("bind", 0, jsff_bind),
	JS_CFUNC_DEF("lock", 0, jsff_lock),
};

static JSValue jsfs_new_filter_obj(JSContext *ctx, GF_Filter *f)
{
	if (JS_IsUndefined(f->jsval)) {
		f->jsval = JS_NewObjectClass(ctx, fs_f_class_id);
		JS_SetPropertyFunctionList(ctx, f->jsval, fs_f_funcs, countof(fs_f_funcs));
		JS_SetOpaque(f->jsval, f);
	}
	return JS_DupValue(ctx, f->jsval);
}

static JSValue jsfs_get_filter(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 idx;
	GF_Filter *f = NULL;
	GF_FilterSession *fs = JS_GetOpaque(this_val, fs_class_id);
    if (!fs || !argc) return GF_JS_EXCEPTION(ctx);

    if (JS_IsString(argv[0])) {
		const char *iname = JS_ToCString(ctx, argv[0]);
		if (iname) {
			u32 i, count;
			gf_fs_lock_filters(fs, GF_TRUE);
			count=gf_list_count(fs->filters);
			for (i=0; i<count; i++) {
				f = gf_list_get(fs->filters, i);
				if (f->iname && !strcmp(f->iname, iname)) break;
				f = NULL;
			}
			gf_fs_lock_filters(fs, GF_FALSE);
		}
		JS_FreeCString(ctx, iname);
	} else {
		if (JS_ToInt32(ctx, &idx, argv[0]))
			return GF_JS_EXCEPTION(ctx);

		f = gf_list_get(fs->filters, idx);
	}
	if (!f) return GF_JS_EXCEPTION(ctx);

	return jsfs_new_filter_obj(ctx, f);
}


static JSValue jsfs_add_filter(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *fname, *link_args;
	GF_Filter *new_f;
	GF_Err e;
	Bool is_source = GF_FALSE;
	GF_Filter *link_from = NULL;
	GF_FilterSession *fs = JS_GetOpaque(this_val, fs_class_id);
	if (!fs || !argc)
		return GF_JS_EXCEPTION(ctx);

	fname = JS_ToCString(ctx, argv[0]);
	if (!fname) return GF_JS_EXCEPTION(ctx);

	link_args = NULL;
	if (argc>1) {
		link_from = JS_GetOpaque(argv[1], fs_f_class_id);
		if (!link_from) {
			JS_FreeCString(ctx, fname);
			return GF_JS_EXCEPTION(ctx);
		}
		if (argc>2) {
			link_args = JS_ToCString(ctx, argv[2]);
		}
	}

	gf_fs_lock_filters(fs, GF_TRUE);

	if (!strncmp(fname, "src=", 4)) {
		new_f = gf_fs_load_source(fs, fname+4, NULL, NULL, &e);
		is_source = GF_TRUE;
	} else if (!strncmp(fname, "dst=", 4)) {
		new_f = gf_fs_load_destination(fs, fname+4, NULL, NULL, &e);
	} else {
		new_f = gf_fs_load_filter(fs, fname, &e);
	}

	if (!new_f) {
		JSValue ret = js_throw_err_msg(ctx, e, "Cannot load filter %s: %s\n", fname, gf_error_to_string(e));
		JS_FreeCString(ctx, fname);
		if (link_args) JS_FreeCString(ctx, link_args);
		gf_fs_lock_filters(fs, GF_FALSE);
		return ret;
	}
	JS_FreeCString(ctx, fname);
	if (link_from) {
		if (is_source)
			gf_filter_set_source_restricted(link_from, new_f, link_args);
		else
			gf_filter_set_source(new_f, link_from, link_args);
	}

	if (link_args) JS_FreeCString(ctx, link_args);
	gf_fs_lock_filters(fs, GF_FALSE);

	return jsfs_new_filter_obj(ctx, new_f);
}

Bool gf_fs_fire_event(GF_FilterSession *fs, GF_Filter *f, GF_FilterEvent *evt, Bool upstream);

static JSValue jsfs_fire_event(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Bool upstream = GF_FALSE;
	GF_Filter *f = NULL;
	Bool ret=GF_FALSE;
	GF_FilterEvent *jsf_get_event(JSContext *ctx, JSValueConst this_val);
	GF_FilterSession *fs = JS_GetOpaque(this_val, fs_class_id);
	GF_FilterEvent *evt;
	if (!fs || !argc)
		return GF_JS_EXCEPTION(ctx);

	evt = jsf_get_event(ctx, argv[0]);
	if (!evt) return GF_JS_EXCEPTION(ctx);

	if (argc>1) {
		f = jsff_get_filter(ctx, argv[1]);
		if (argc>2) upstream = JS_ToBool(ctx, argv[2]);
	}
	ret = gf_fs_fire_event(fs, f, evt, upstream);
	return JS_NewBool(ctx, ret);
}

static JSValue jsfs_reporting(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Bool report_on;
	GF_FilterSession *fs = JS_GetOpaque(this_val, fs_class_id);
	if (!fs || !argc)
		return GF_JS_EXCEPTION(ctx);
	report_on = JS_ToBool(ctx, argv[0]);
	gf_fs_enable_reporting(fs, report_on);
	return JS_UNDEFINED;
}

JSValue jsfilter_initialize_custom(GF_Filter *filter, JSContext *ctx);

static JSValue jsfs_new_filter(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Filter *f;
	GF_Err e;
	const char *name = NULL;
	GF_FilterSession *fs = JS_GetOpaque(this_val, fs_class_id);
	if (!fs) return GF_JS_EXCEPTION(ctx);
	if (argc) name = JS_ToCString(ctx, argv[0]);

	f = gf_fs_new_filter(fs, name, &e);
	if (name) JS_FreeCString(ctx, name);
	if (!f) return js_throw_err(ctx, e);

	return jsfilter_initialize_custom(f, ctx);
}


static const JSCFunctionListEntry fs_funcs[] = {
    JS_CGETSET_MAGIC_DEF_ENUM("nb_filters", jsfs_prop_get, NULL, JSFS_NB_FILTERS),
    JS_CGETSET_MAGIC_DEF_ENUM("last_task", jsfs_prop_get, NULL, JSFS_LAST_TASK),
	JS_CGETSET_MAGIC_DEF("http_max_bitrate", jsfs_prop_get, jsfs_prop_set, JSFS_HTTP_MAX_RATE),
	JS_CGETSET_MAGIC_DEF("http_bitrate", jsfs_prop_get, NULL, JSFS_HTTP_RATE),
	JS_CGETSET_MAGIC_DEF("rmt_sampling", jsfs_prop_get, jsfs_prop_set, JSFS_RMT_SAMPLING),
	JS_CGETSET_MAGIC_DEF("connected", jsfs_prop_get, NULL, JSFS_CONNECTED),
	JS_CGETSET_MAGIC_DEF("last_process_error", jsfs_prop_get, NULL, JSFS_LAST_PROCESS_ERR),
	JS_CGETSET_MAGIC_DEF("last_connect_error", jsfs_prop_get, NULL, JSFS_LAST_CONNECT_ERR),

    JS_CFUNC_DEF("post_task", 0, jsfs_post_task),
    JS_CFUNC_DEF("abort", 0, jsfs_abort),
    JS_CFUNC_DEF("get_filter", 0, jsfs_get_filter),
    JS_CFUNC_DEF("lock_filters", 0, jsfs_lock_filters),
    JS_CFUNC_DEF("rmt_send", 0, jsfs_rmt_send),
    JS_CFUNC_DEF("set_rmt_fun", 0, jsfs_set_rmt_fun),
    JS_CFUNC_DEF("set_new_filter_fun", 0, jsfs_set_new_filter_fun),
    JS_CFUNC_DEF("set_del_filter_fun", 0, jsfs_set_del_filter_fun),
    JS_CFUNC_DEF("set_event_fun", 0, jsfs_set_event_fun),
    JS_CFUNC_DEF("add_filter", 0, jsfs_add_filter),
    JS_CFUNC_DEF("fire_event", 0, jsfs_fire_event),
    JS_CFUNC_DEF("reporting", 0, jsfs_reporting),
    JS_CFUNC_DEF("new_filter", 0, jsfs_new_filter),

};

void gf_fs_unload_js_api(JSContext *c, GF_FilterSession *fs)
{
	u32 i, count;
	gf_mx_p(fs->filters_mx);
	count = gf_list_count(fs->filters);
	//detach all script objects, the context having created them is about to be destroyed
	//not doing so would result in potential crashes during final destruction of filter(s)
	for (i=0; i<count; i++) {
		GF_Filter *f = gf_list_get(fs->filters, i);
		if (!JS_IsUndefined(f->jsval)) {
			JS_SetOpaque(f->jsval, NULL);
			JS_FreeValue(c, f->jsval);
			f->jsval = JS_UNDEFINED;
		}
	}
	gf_mx_v(fs->filters_mx);
}

GF_Err gf_fs_load_js_api(JSContext *c, GF_FilterSession *fs)
{
	JSValue fs_obj;
	JSRuntime *rt;
	JSValue global_obj;

	if (fs->js_ctx) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[JSFS] FilterSession API already loaded by another script, cannot load twice\n"));
		return GF_NOT_SUPPORTED;
	}

	rt = JS_GetRuntime(c);
	global_obj = JS_GetGlobalObject(c);

	js_load_constants(c, global_obj);

#define DEF_CONST( _val ) \
    JS_SetPropertyStr(c, global_obj, #_val, JS_NewInt32(c, _val));

	DEF_CONST(GF_FS_FLUSH_NONE)
	DEF_CONST(GF_FS_FLUSH_ALL)
	DEF_CONST(GF_FS_FLUSH_FAST)

	if (!fs->jstasks) {
		fs->jstasks = gf_list_new();
		if (!fs->jstasks) return GF_OUT_OF_MEM;
	}
	

	//initialize filter class and create a single filter object in global scope
	JS_NewClassID(&fs_class_id);
	JS_NewClass(rt, fs_class_id, &fs_class);

	JS_NewClassID(&fs_f_class_id);
	JS_NewClass(rt, fs_f_class_id, &fs_f_class);


	fs_obj = JS_NewObjectClass(c, fs_class_id);
    JS_SetPropertyFunctionList(c, fs_obj, fs_funcs, countof(fs_funcs));
    JS_SetOpaque(fs_obj, fs);
	JS_SetPropertyStr(c, global_obj, "session", fs_obj);

    JS_FreeValue(c, global_obj);
    return GF_OK;
}

GF_EXPORT
GF_Err gf_fs_load_script(GF_FilterSession *fs, const char *jsfile)
{
	GF_Err e;
    JSValue global_obj;
	u8 *buf;
	u32 buf_len;
	u32 flags = JS_EVAL_TYPE_GLOBAL;
    JSValue ret;
    JSContext *ctx;


	if (!fs) return GF_BAD_PARAM;
	if (fs->js_ctx) return GF_NOT_SUPPORTED;

#ifdef GPAC_HAS_QJS
	ctx = gf_js_create_context();
	if (!ctx) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[JSF] Failed to load QuickJS context\n"));
		return GF_IO_ERR;
	}
	JS_SetContextOpaque(ctx, fs);

    global_obj = JS_GetGlobalObject(ctx);
	gf_fs_load_js_api(ctx, fs);
	fs->js_ctx = ctx;

	JS_SetPropertyStr(fs->js_ctx, global_obj, "_gpac_log_name", JS_NewString(fs->js_ctx, gf_file_basename(jsfile) ) );
    JS_FreeValue(fs->js_ctx, global_obj);


	//load script
	if (!strncmp(jsfile, "$GSHARE/", 8)) {
		char szPath[GF_MAX_PATH];
		if (gf_opts_default_shared_directory(szPath)) {
			strcat(szPath, jsfile + 7);
			e = gf_file_load_data(szPath, &buf, &buf_len);
		} else {
			e = GF_NOT_FOUND;
		}
	} else {
		e = gf_file_load_data(jsfile, &buf, &buf_len);
	}
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[JSF] Error loading script file %s: %s\n", jsfile, gf_error_to_string(e) ));
		return e;
	}

 	if (!gf_opts_get_bool("core", "no-js-mods") && JS_DetectModule((char *)buf, buf_len)) {
		qjs_init_all_modules(fs->js_ctx, GF_FALSE, GF_FALSE);
		flags = JS_EVAL_TYPE_MODULE;
	}

	ret = JS_Eval(fs->js_ctx, (char *)buf, buf_len, jsfile, flags);
	gf_free(buf);

	if (JS_IsException(ret)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[JSF] Error loading script %s\n", jsfile));
        js_dump_error(fs->js_ctx);
		JS_FreeValue(fs->js_ctx, ret);
		return GF_BAD_PARAM;
	}
	JS_FreeValue(fs->js_ctx, ret);
	js_std_loop(fs->js_ctx);
	return GF_OK;
#else
	return GF_NOT_SUPPORTED;
#endif

}

void gf_fs_unload_script(GF_FilterSession *fs, void *js_ctx)
{
	u32 i, count=gf_list_count(fs->jstasks);

	for (i=0; i<count; i++) {
		JSFS_Task *task = gf_list_get(fs->jstasks, i);
		if (js_ctx && (task->ctx != js_ctx))
			continue;

		gf_js_lock(js_ctx, GF_TRUE);
		JS_FreeValue(task->ctx, task->fun);
		JS_FreeValue(task->ctx, task->_obj);
		gf_js_lock(js_ctx, GF_FALSE);

		gf_free(task);
		gf_list_rem(fs->jstasks, i);
		i--;
		count--;
	}

	if (fs->js_ctx) {
		gf_js_delete_context(fs->js_ctx);
		fs->js_ctx = NULL;
	}
	if (!js_ctx || !gf_list_count(fs->jstasks)) {
		gf_list_del(fs->jstasks);
		fs->jstasks = NULL;
	}
}

#else
GF_EXPORT
GF_Err gf_fs_load_script(GF_FilterSession *fs, const char *jsfile)
{
	return GF_NOT_SUPPORTED;
}
void gf_fs_unload_script(GF_FilterSession *fs, void *js_ctx)
{

}

#endif



