/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2007-2020
 *			All rights reserved
 *
 *  This file is part of GPAC / JavaScript libgpac Core bindings
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
	ANY CHANGE TO THE API MUST BE REFLECTED IN THE DOCUMENTATION IN gpac/share/doc/idl/storage.js
	(no way to define inline JS doc with doxygen)
*/

#include <gpac/setup.h>

#ifdef GPAC_HAS_QJS
#include <gpac/bitstream.h>
#include <gpac/network.h>

#include "../scenegraph/qjs_common.h"


static JSClassID bitstream_class_id = 0;

typedef struct
{
	GF_BitStream *bs;
	JSValue buf_ref;
} JSBitstream;

#define GET_JSBS \
	JSBitstream *jbs = JS_GetOpaque(obj, bitstream_class_id); \
	GF_BitStream *bs = jbs ? jbs->bs : NULL;

static void js_bs_finalize(JSRuntime *rt, JSValue obj)
{
	GET_JSBS
	if (!bs) return;
	gf_bs_del(bs);
	JS_FreeValueRT(rt, jbs->buf_ref);
}
static void js_bs_gc_mark(JSRuntime *rt, JSValueConst obj, JS_MarkFunc *mark_func)
{
	GET_JSBS
	if (!bs) return;
	JS_MarkValue(rt, jbs->buf_ref, mark_func);
}

JSClassDef bitstreamClass = {
    "Bitstream",
    .finalizer = js_bs_finalize,
	.gc_mark = js_bs_gc_mark
};


static JSValue js_bs_get_u8(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_BitStream *bs = JS_GetOpaque(this_val, bitstream_class_id);
	if (!bs) return JS_EXCEPTION;
	return JS_NewInt32(ctx, gf_bs_read_u8(bs) );
}
static JSValue js_bs_get_s8(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s8 v;
	GF_BitStream *bs = JS_GetOpaque(this_val, bitstream_class_id);
	if (!bs) return JS_EXCEPTION;
	v = (s8) gf_bs_read_u8(bs);
	return JS_NewInt32(ctx, v );
}
static JSValue js_bs_get_u16(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_BitStream *bs = JS_GetOpaque(this_val, bitstream_class_id);
	if (!bs) return JS_EXCEPTION;
	return JS_NewInt32(ctx, gf_bs_read_u16(bs) );
}
static JSValue js_bs_get_s16(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s16 v;
	GF_BitStream *bs = JS_GetOpaque(this_val, bitstream_class_id);
	if (!bs) return JS_EXCEPTION;
	v = (s16) gf_bs_read_u16(bs);
	return JS_NewInt32(ctx, v );
}
static JSValue js_bs_get_u24(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_BitStream *bs = JS_GetOpaque(this_val, bitstream_class_id);
	if (!bs) return JS_EXCEPTION;
	return JS_NewInt32(ctx, gf_bs_read_u24(bs) );
}
static JSValue js_bs_get_s32(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s32 v;
	GF_BitStream *bs = JS_GetOpaque(this_val, bitstream_class_id);
	if (!bs) return JS_EXCEPTION;
	v = (s32) gf_bs_read_u32(bs);
	return JS_NewInt32(ctx, v);
}
static JSValue js_bs_get_u32(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_BitStream *bs = JS_GetOpaque(this_val, bitstream_class_id);
	if (!bs) return JS_EXCEPTION;
	return JS_NewInt32(ctx, gf_bs_read_u32(bs) );
}
static JSValue js_bs_get_u64(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_BitStream *bs = JS_GetOpaque(this_val, bitstream_class_id);
	if (!bs) return JS_EXCEPTION;
	return JS_NewInt64(ctx, gf_bs_read_u64(bs) );
}
static JSValue js_bs_get_s64(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s64 v;
	GF_BitStream *bs = JS_GetOpaque(this_val, bitstream_class_id);
	if (!bs) return JS_EXCEPTION;
	v = (s64) gf_bs_read_u64(bs);
	return JS_NewInt64(ctx, v );
}
static JSValue js_bs_get_float(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_BitStream *bs = JS_GetOpaque(this_val, bitstream_class_id);
	if (!bs) return JS_EXCEPTION;
	return JS_NewFloat64(ctx, gf_bs_read_float(bs) );
}
static JSValue js_bs_get_double(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_BitStream *bs = JS_GetOpaque(this_val, bitstream_class_id);
	if (!bs) return JS_EXCEPTION;
	return JS_NewFloat64(ctx, gf_bs_read_double(bs) );
}
static JSValue js_bs_get_bits(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s32 nb_bits;
	GF_BitStream *bs = JS_GetOpaque(this_val, bitstream_class_id);
	if (!bs || !argc) return JS_EXCEPTION;
	if (JS_ToInt32(ctx, &nb_bits, argv[0])) return JS_EXCEPTION;
	if (nb_bits<=32)
		return JS_NewInt32(ctx, gf_bs_read_int(bs, nb_bits) );
	return JS_NewInt64(ctx, gf_bs_read_long_int(bs, nb_bits) );
}
static JSValue js_bs_get_data(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s32 nb_bytes;
	size_t data_size;
	u8 *data=NULL;
	GF_BitStream *bs = JS_GetOpaque(this_val, bitstream_class_id);
	if (!bs) return JS_EXCEPTION;
	if (argc!=2) return JS_EXCEPTION;
	if (JS_ToInt32(ctx, &nb_bytes, argv[0])) return JS_EXCEPTION;

	if (JS_IsObject(argv[1])) {
		data = JS_GetArrayBuffer(ctx, &data_size, argv[1]);
	}
	if (!data)
		return JS_EXCEPTION;
	if (nb_bytes>data_size)
		return JS_EXCEPTION;

	data_size = gf_bs_read_data(bs, data, nb_bytes);
	return JS_NewInt32(ctx, data_size);
}
static JSValue js_bs_skip_bytes(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s32 nb_bytes;
	GF_BitStream *bs = JS_GetOpaque(this_val, bitstream_class_id);
	if (!bs || !argc) return JS_EXCEPTION;
	if (JS_ToInt32(ctx, &nb_bytes, argv[0])) return JS_EXCEPTION;
	gf_bs_skip_bytes(bs, nb_bytes);
	return JS_UNDEFINED;
}



static const JSCFunctionListEntry bitstream_funcs[] = {
	JS_CFUNC_DEF("get_u8", 0, js_bs_get_u8),
	JS_CFUNC_DEF("get_s8", 0, js_bs_get_s8),
	JS_CFUNC_DEF("get_u16", 0, js_bs_get_u16),
	JS_CFUNC_DEF("get_s16", 0, js_bs_get_s16),
	JS_CFUNC_DEF("get_u24", 0, js_bs_get_u24),
	JS_CFUNC_DEF("get_u32", 0, js_bs_get_u32),
	JS_CFUNC_DEF("get_s32", 0, js_bs_get_s32),
	JS_CFUNC_DEF("get_u64", 0, js_bs_get_u64),
	JS_CFUNC_DEF("get_s64", 0, js_bs_get_s64),
	JS_CFUNC_DEF("get_bits", 0, js_bs_get_bits),
	JS_CFUNC_DEF("get_float", 0, js_bs_get_float),
	JS_CFUNC_DEF("get_double", 0, js_bs_get_double),
	JS_CFUNC_DEF("skip", 0, js_bs_skip_bytes),
	JS_CFUNC_DEF("get_data", 0, js_bs_get_data),
};

static JSValue bitstream_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	JSValue anobj;
	JSBitstream *jbs;
	GF_BitStream *bs = NULL;

	if (!argc) {
		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	} else if (argc && JS_IsObject(argv[0])){
		u8 *data;
		size_t data_size;

		data = JS_GetArrayBuffer(ctx, &data_size, argv[0]);
		if (!data) return JS_EXCEPTION;
		if ((argc>1) && JS_ToBool(ctx, argv[1])) {
			bs = gf_bs_new(data, data_size, GF_BITSTREAM_WRITE);
		} else {
			bs = gf_bs_new(data, data_size, GF_BITSTREAM_READ);
		}
	} else {
		return JS_EXCEPTION;
	}
	GF_SAFEALLOC(jbs, JSBitstream);
	if (!jbs) {
		gf_bs_del(bs);
		return js_throw_err(ctx, GF_OUT_OF_MEM);
	}
	anobj = JS_NewObjectClass(ctx, bitstream_class_id);
	if (JS_IsException(anobj)) {
		gf_bs_del(bs);
		gf_free(jbs);
		return anobj;
	}
	jbs->bs = bs;
	jbs->buf_ref = JS_UNDEFINED;
	if (argc)
		jbs->buf_ref = JS_DupValue(ctx, argv[0]);

	JS_SetOpaque(anobj, jbs);
	return anobj;
}

enum
{
	JS_SYS_NB_CORES = 1,
	JS_SYS_SAMPLE_DUR,
	JS_SYS_TOTAL_CPU,
	JS_SYS_PROCESS_CPU,
	JS_SYS_TOTAL_CPU_DIFF,
	JS_SYS_PROCESS_CPU_DIFF,
	JS_SYS_CPU_IDLE,
	JS_SYS_TOTAL_CPU_USAGE,
	JS_SYS_PROCESS_CPU_USAGE,
	JS_SYS_PID,
	JS_SYS_THREADS,
	JS_SYS_PROCESS_MEM,
	JS_SYS_TOTAL_MEM,
	JS_SYS_TOTAL_MEM_AVAIL,
	JS_SYS_GPAC_MEM,

	JS_SYS_BATTERY_ON,
	JS_SYS_BATTERY_CHARGE,
	JS_SYS_BATTERY_PERCENT,
	JS_SYS_BATTERY_LIFETIME,
	JS_SYS_BATTERY_LIFETIME_FULL,
	JS_SYS_HOSTNAME,
	JS_SYS_LAST_WORK_DIR,

};

#define RTI_REFRESH_MS	200
static JSValue js_sys_prop_get(JSContext *ctx, JSValueConst this_val, int magic)
{
	u32 uval;
	Bool bval;
	const char *res;
	GF_SystemRTInfo rti;

	if (magic<=JS_SYS_GPAC_MEM) {
		gf_sys_get_rti(RTI_REFRESH_MS, &rti, 0);
		switch (magic) {
		case JS_SYS_NB_CORES:
			return JS_NewInt32(ctx, rti.nb_cores);
		case JS_SYS_SAMPLE_DUR:
			return JS_NewInt32(ctx, rti.sampling_period_duration);
		case JS_SYS_TOTAL_CPU:
			return JS_NewInt32(ctx, rti.total_cpu_time);
		case JS_SYS_PROCESS_CPU:
			return JS_NewInt32(ctx, rti.process_cpu_time);
		case JS_SYS_TOTAL_CPU_DIFF:
			return JS_NewInt32(ctx, rti.total_cpu_time_diff);
		case JS_SYS_PROCESS_CPU_DIFF:
			return JS_NewInt32(ctx, rti.process_cpu_time_diff);
		case JS_SYS_CPU_IDLE:
			return JS_NewInt32(ctx, rti.cpu_idle_time);
		case JS_SYS_TOTAL_CPU_USAGE:
			return JS_NewInt32(ctx, rti.total_cpu_time);
		case JS_SYS_PROCESS_CPU_USAGE:
			return JS_NewInt32(ctx, rti.process_cpu_usage);
		case JS_SYS_PID:
			return JS_NewInt32(ctx, rti.pid);
		case JS_SYS_THREADS:
			return JS_NewInt32(ctx, rti.thread_count);
		case JS_SYS_PROCESS_MEM:
			return JS_NewInt64(ctx, rti.process_memory);
		case JS_SYS_TOTAL_MEM:
			return JS_NewInt64(ctx, rti.physical_memory);
		case JS_SYS_TOTAL_MEM_AVAIL:
			return JS_NewInt64(ctx, rti.physical_memory_avail);
		case JS_SYS_GPAC_MEM:
			return JS_NewInt64(ctx, rti.gpac_memory);
		}
		return JS_UNDEFINED;
	}

	switch (magic) {
	case JS_SYS_BATTERY_ON:
		bval = GF_FALSE;
		gf_sys_get_battery_state(&bval, NULL, NULL, NULL, NULL);
		return JS_NewBool(ctx, bval);

	case JS_SYS_BATTERY_CHARGE:
		uval = 0;
		gf_sys_get_battery_state(NULL, &uval, NULL, NULL, NULL);
		return JS_NewBool(ctx, uval ? 1 : 0);

	case JS_SYS_BATTERY_PERCENT:
		uval=0;
		gf_sys_get_battery_state(NULL, NULL, &uval, NULL, NULL);
		return JS_NewInt32(ctx, uval);

	case JS_SYS_BATTERY_LIFETIME:
		uval=0;
		gf_sys_get_battery_state(NULL, NULL, NULL, &uval, NULL);
		return JS_NewInt32(ctx, uval);

	case JS_SYS_BATTERY_LIFETIME_FULL:
		uval=0;
		gf_sys_get_battery_state(NULL, NULL, NULL, NULL, &uval);
		return JS_NewInt32(ctx, uval);

	case JS_SYS_HOSTNAME:
		{
			char hostname[100];
			gf_sk_get_host_name((char*)hostname);
			return JS_NewString(ctx, hostname);
		}
		break;
	case JS_SYS_LAST_WORK_DIR:
		res = gf_opts_get_key("General", "LastWorkingDir");
#ifdef WIN32
		if (!res) res = getenv("HOMEPATH");
		if (!res) res = "C:\\";
#elif defined(GPAC_CONFIG_DARWIN)
		if (!res) res = getenv("HOME");
		if (!res) res = "/Users";
#elif defined(GPAC_CONFIG_ANDROID)
		if (!res) res = getenv("EXTERNAL_STORAGE");
		if (!res) res = "/sdcard";
#elif defined(GPAC_CONFIG_IOS)
		if (!res) res = (char *) gf_opts_get_key("General", "iOSDocumentsDir");
#else
		if (!res) res = getenv("HOME");
		if (!res) res = "/home/";
#endif
		return JS_NewString(ctx, res);

	}

	return JS_UNDEFINED;
}

static JSValue js_sys_prop_set(JSContext *ctx, JSValueConst this_val, JSValueConst value, int magic)
{
	const char *prop_val;
	switch (magic) {
	case JS_SYS_LAST_WORK_DIR:
		if (!JS_IsString(value)) return JS_EXCEPTION;
		prop_val = JS_ToCString(ctx, value);
		gf_opts_set_key("General", "LastWorkingDir", prop_val);
		JS_FreeCString(ctx, prop_val);
		break;

	}
	return JS_UNDEFINED;
}

static JSValue js_sys_set_arg_used(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 idx;
	Bool used;
	if (argc < 2) return JS_EXCEPTION;

	if (JS_ToInt32(ctx, &idx, argv[0]))
		return JS_EXCEPTION;
	used = JS_ToBool(ctx, argv[1]);

	gf_sys_mark_arg_used(idx, used);
	return JS_UNDEFINED;
}


typedef struct
{
	JSContext *c;
	JSValue array;
	Bool is_dir;
	u32 idx;
} enum_dir_cbk;

static Bool js_enum_dir_fct(void *cbck, char *file_name, char *file_path, GF_FileEnumInfo *file_info)
{
	u32 i, len;
	char *sep;
	JSValue s;
	JSValue obj;
	enum_dir_cbk *cbk = (enum_dir_cbk*)cbck;

	if (file_name && (file_name[0]=='.')) return 0;

	obj = JS_NewObject(cbk->c);
	JS_SetPropertyStr(cbk->c, obj, "name", JS_NewString(cbk->c, file_name) );

	sep=NULL;
	len = (u32) strlen(file_path);
	for (i=0; i<len; i++) {
		sep = strchr("/\\", file_path[len-i-1]);
		if (sep) {
			sep = file_path+len-i-1;
			break;
		}
	}
	if (sep) {
		sep[0] = '/';
		sep[1] = 0;
		s = JS_NewString(cbk->c, file_path);
	} else {
		s = JS_NewString(cbk->c, file_path);
	}
	JS_SetPropertyStr(cbk->c, obj, "path", s);

	JS_SetPropertyStr(cbk->c, obj, "directory", JS_NewBool(cbk->c, cbk->is_dir) );
	JS_SetPropertyStr(cbk->c, obj, "drive", JS_NewBool(cbk->c, file_info->drive) );
	JS_SetPropertyStr(cbk->c, obj, "hidden", JS_NewBool(cbk->c, file_info->hidden) );
	JS_SetPropertyStr(cbk->c, obj, "system", JS_NewBool(cbk->c, file_info->system) );
	JS_SetPropertyStr(cbk->c, obj, "size", JS_NewInt64(cbk->c, file_info->size) );
	JS_SetPropertyStr(cbk->c, obj, "last_modified", JS_NewInt64(cbk->c, file_info->last_modified) );

	JS_SetPropertyUint32(cbk->c, cbk->array, cbk->idx, obj);
	cbk->idx++;
	return 0;
}

static JSValue js_sys_enum_directory(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Err err;
	enum_dir_cbk cbk;
	char *url = NULL;
	const char *dir = NULL;
	const char *filter = NULL;
	Bool dir_only = 0;
	Bool browse_root = 0;

	if ((argc >= 1) && JS_IsString(argv[0])) {
		dir = JS_ToCString(ctx, argv[0]);
		if (!strcmp(dir, "/")) browse_root = 1;
	}
	if ((argc >= 2) && JS_IsString(argv[1])) {
		filter = JS_ToCString(ctx, argv[1]);
		if (!strcmp(filter, "dir")) {
			dir_only = 1;
			filter = NULL;
		} else if (!strlen(filter)) {
			JS_FreeCString(ctx, filter);
			filter=NULL;
		}
	}
	if ((argc >= 3) && JS_IsBool(argv[2])) {
		if (JS_ToBool(ctx, argv[2])) {
			url = gf_url_concatenate(dir, "..");
			if (dir && ( !strcmp(url, "..") || (url[0]==0) )) {
				if ((dir[1]==':') && ((dir[2]=='/') || (dir[2]=='\\')) ) browse_root = 1;
				else if (!strcmp(dir, "/")) browse_root = 1;
			}
			if (!strcmp(url, "/")) browse_root = 1;
		}
	}

	if ( (!dir || !strlen(dir) ) && (!url || !strlen(url))) browse_root = 1;

	if (browse_root) {
		cbk.c = ctx;
		cbk.idx = 0;
		cbk.array = JS_NewArray(ctx);
		if (JS_IsException(cbk.array)) return cbk.array;
		cbk.is_dir = 1;
		gf_enum_directory("/", 1, js_enum_dir_fct, &cbk, NULL);
		if (url) gf_free(url);
		JS_FreeCString(ctx, dir);
		JS_FreeCString(ctx, filter);
		return cbk.array;
	}

	cbk.c = ctx;
	cbk.array = JS_NewArray(ctx);
	if (JS_IsException(cbk.array)) return cbk.array;
	cbk.is_dir = 1;
	cbk.idx = 0;

#if 0
	compositor = scenejs_get_compositor(ctx, this_val);

	/*concatenate with service url*/
	char *an_url = gf_url_concatenate(compositor->root_scene->root_od->scene_ns->url, url ? url : dir);
	if (an_url) {
		gf_free(url);
		url = an_url;
	}
#endif

	err = gf_enum_directory(url ? url : dir, 1, js_enum_dir_fct, &cbk, NULL);
	if (err) return JS_EXCEPTION;

	if (!dir_only) {
		cbk.is_dir = 0;
		err = gf_enum_directory(url ? url : dir, 0, js_enum_dir_fct, &cbk, filter);
		if (err) return JS_EXCEPTION;
	}

	if (url) gf_free(url);
	JS_FreeCString(ctx, dir);
	JS_FreeCString(ctx, filter);
	return cbk.array;
}


static JSValue js_sys_error_string(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Err e;
	if (argc < 1) return JS_EXCEPTION;
	if (JS_ToInt32(ctx, (int32_t *) &e, argv[0]))
		return JS_EXCEPTION;
	return JS_NewString(ctx, gf_error_to_string(e) );
}

static JSValue js_sys_prompt_input(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	char in_char[2];

    if (!gf_prompt_has_input())
		return JS_NULL;
	in_char[0] = gf_prompt_get_char();
	in_char[1] = 0;
	return JS_NewString(ctx, in_char);
}

static JSValue js_sys_prompt_string(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	char input[4096];

    if (argc) {
#ifdef GPAC_ENABLE_COVERAGE
		jsfs_rmt_user_callback(NULL, "my text");
#endif
		return JS_NewString(ctx, "Coverage OK");
	}

	if (1 > scanf("%4095s", input)) {
		return js_throw_err_msg(ctx, GF_IO_ERR, "Cannot read string from prompt");
	}
	return JS_NewString(ctx, input);
}

#define JS_CGETSET_MAGIC_DEF_ENUM(name, fgetter, fsetter, magic) { name, JS_PROP_CONFIGURABLE|JS_PROP_ENUMERABLE, JS_DEF_CGETSET_MAGIC, magic, .u = { .getset = { .get = { .getter_magic = fgetter }, .set = { .setter_magic = fsetter } } } }

static const JSCFunctionListEntry sys_funcs[] = {
    JS_CGETSET_MAGIC_DEF_ENUM("nb_cores", js_sys_prop_get, NULL, JS_SYS_NB_CORES),
    JS_CGETSET_MAGIC_DEF_ENUM("sampling_period_duration", js_sys_prop_get, NULL, JS_SYS_SAMPLE_DUR),
    JS_CGETSET_MAGIC_DEF_ENUM("total_cpu_time", js_sys_prop_get, NULL, JS_SYS_TOTAL_CPU),
    JS_CGETSET_MAGIC_DEF_ENUM("process_cpu_time", js_sys_prop_get, NULL, JS_SYS_PROCESS_CPU),
    JS_CGETSET_MAGIC_DEF_ENUM("total_cpu_time_diff", js_sys_prop_get, NULL, JS_SYS_TOTAL_CPU_DIFF),
    JS_CGETSET_MAGIC_DEF_ENUM("process_cpu_time_diff", js_sys_prop_get, NULL, JS_SYS_PROCESS_CPU_DIFF),
    JS_CGETSET_MAGIC_DEF_ENUM("cpu_idle_time", js_sys_prop_get, NULL, JS_SYS_CPU_IDLE),
    JS_CGETSET_MAGIC_DEF_ENUM("total_cpu_usage", js_sys_prop_get, NULL, JS_SYS_TOTAL_CPU_USAGE),
    JS_CGETSET_MAGIC_DEF_ENUM("process_cpu_usage", js_sys_prop_get, NULL, JS_SYS_PROCESS_CPU_USAGE),
    JS_CGETSET_MAGIC_DEF_ENUM("pid", js_sys_prop_get, NULL, JS_SYS_PID),
    JS_CGETSET_MAGIC_DEF_ENUM("thread_count", js_sys_prop_get, NULL, JS_SYS_THREADS),
    JS_CGETSET_MAGIC_DEF_ENUM("process_memory", js_sys_prop_get, NULL, JS_SYS_PROCESS_MEM),
    JS_CGETSET_MAGIC_DEF_ENUM("physical_memory", js_sys_prop_get, NULL, JS_SYS_TOTAL_MEM),
    JS_CGETSET_MAGIC_DEF_ENUM("physical_memory_avail", js_sys_prop_get, NULL, JS_SYS_TOTAL_MEM_AVAIL),
    JS_CGETSET_MAGIC_DEF_ENUM("gpac_memory", js_sys_prop_get, NULL, JS_SYS_GPAC_MEM),

	JS_CGETSET_MAGIC_DEF_ENUM("last_wdir", js_sys_prop_get, js_sys_prop_set, JS_SYS_LAST_WORK_DIR),
	JS_CGETSET_MAGIC_DEF_ENUM("batteryOn", js_sys_prop_get, NULL, JS_SYS_BATTERY_ON),
	JS_CGETSET_MAGIC_DEF_ENUM("batteryCharging", js_sys_prop_get, NULL, JS_SYS_BATTERY_CHARGE),
	JS_CGETSET_MAGIC_DEF_ENUM("batteryPercent", js_sys_prop_get, NULL, JS_SYS_BATTERY_PERCENT),
	JS_CGETSET_MAGIC_DEF_ENUM("batteryLifeTime", js_sys_prop_get, NULL, JS_SYS_BATTERY_LIFETIME),
	JS_CGETSET_MAGIC_DEF_ENUM("batteryFullLifeTime", js_sys_prop_get, NULL, JS_SYS_BATTERY_LIFETIME_FULL),
	JS_CGETSET_MAGIC_DEF_ENUM("hostname", js_sys_prop_get, NULL, JS_SYS_HOSTNAME),


	JS_CFUNC_DEF("set_arg_used", 0, js_sys_set_arg_used),
	JS_CFUNC_DEF("error_string", 0, js_sys_error_string),
    JS_CFUNC_DEF("prompt_input", 0, js_sys_prompt_input),
    JS_CFUNC_DEF("prompt_string", 0, js_sys_prompt_string),
	JS_CFUNC_DEF("enum_directory", 0, js_sys_enum_directory),
};


static int js_gpaccore_init(JSContext *ctx, JSModuleDef *m)
{
	if (!bitstream_class_id) {
		JS_NewClassID(&bitstream_class_id);
		JS_NewClass(JS_GetRuntime(ctx), bitstream_class_id, &bitstreamClass);
	}

	JSValue proto = JS_NewObjectClass(ctx, bitstream_class_id);
	JS_SetPropertyFunctionList(ctx, proto, bitstream_funcs, countof(bitstream_funcs));
	JS_SetClassProto(ctx, bitstream_class_id, proto);

	JSValue ctor = JS_NewCFunction2(ctx, bitstream_constructor, "Bitstream", 1, JS_CFUNC_constructor, 0);
    JS_SetModuleExport(ctx, m, "Bitstream", ctor);

	JSValue core_o = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, core_o, sys_funcs, countof(sys_funcs));
    JS_SetModuleExport(ctx, m, "Sys", core_o);

	JSValue args = JS_NewArray(ctx);
    u32 i, nb_args = gf_sys_get_argc();
    for (i=0; i<nb_args; i++) {
        JS_SetPropertyUint32(ctx, args, i, JS_NewString(ctx, gf_sys_get_arg(i)));
    }
    JS_SetPropertyStr(ctx, core_o, "args", args);


	return 0;
}


void qjs_module_init_gpaccore(JSContext *ctx)
{
	JSModuleDef *m;
	m = JS_NewCModule(ctx, "gpaccore", js_gpaccore_init);
	if (!m) return;

	JS_AddModuleExport(ctx, m, "Bitstream");
	JS_AddModuleExport(ctx, m, "Sys");
	return;
}


#endif

