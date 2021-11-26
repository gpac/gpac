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
#include <gpac/main.h>
#include <gpac/thread.h>
#include <gpac/bitstream.h>
#include <gpac/network.h>
#include <gpac/base_coding.h>
#include <gpac/filters.h>

#include "../scenegraph/qjs_common.h"


static void qjs_init_runtime_libc(JSRuntime *rt);
static void qjs_uninit_runtime_libc(JSRuntime *rt);

typedef struct
{
	JSRuntime *js_runtime;
	u32 nb_inst;
	JSContext *ctx;

	GF_Mutex *mx;
	GF_List *allocated_contexts;
} GF_JSRuntime;

static GF_JSRuntime *js_rt = NULL;

JSContext *gf_js_create_context()
{
	JSContext *ctx;
	if (!js_rt) {
		JSRuntime *js_runtime = JS_NewRuntime();
		if (!js_runtime) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[ECMAScript] Cannot allocate ECMAScript runtime\n"));
			return NULL;
		}
		GF_SAFEALLOC(js_rt, GF_JSRuntime);
		if (!js_rt) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[JS] Failed to create script runtime\n"));
			return NULL;
		}
		js_rt->js_runtime = js_runtime;
		js_rt->allocated_contexts = gf_list_new();
		js_rt->mx = gf_mx_new("JavaScript");
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[ECMAScript] ECMAScript runtime allocated %p\n", js_runtime));

		qjs_init_runtime_libc(js_rt->js_runtime);
	}
	js_rt->nb_inst++;

	gf_mx_p(js_rt->mx);

	ctx = JS_NewContext(js_rt->js_runtime);

	gf_list_add(js_rt->allocated_contexts, ctx);
	gf_mx_v(js_rt->mx);

	return ctx;
}

void gf_js_delete_context(JSContext *ctx)
{
	if (!js_rt) return;

	gf_js_call_gc(ctx);

	gf_mx_p(js_rt->mx);
	gf_list_del_item(js_rt->allocated_contexts, ctx);
	JS_FreeContext(ctx);
	gf_mx_v(js_rt->mx);
	gf_js_call_gc(NULL);

	js_rt->nb_inst --;
	if (js_rt->nb_inst == 0) {
		//persistent context, do not delete runtime but perform GC
		if (gf_opts_get_bool("temp", "peristent-jsrt")) {
			JS_RunGC(js_rt->js_runtime);
			return;
		}

		qjs_uninit_runtime_libc(js_rt->js_runtime);
		JS_FreeRuntime(js_rt->js_runtime);
		gf_list_del(js_rt->allocated_contexts);
		gf_mx_del(js_rt->mx);
		gf_free(js_rt);
		js_rt = NULL;
	}
}
GF_EXPORT
void gf_js_delete_runtime()
{
	if (js_rt) {
		JS_FreeRuntime(js_rt->js_runtime);
		gf_list_del(js_rt->allocated_contexts);
		gf_mx_del(js_rt->mx);
		gf_free(js_rt);
		js_rt = NULL;
	}
}

void gf_js_call_gc(JSContext *c)
{
	gf_js_lock(c, 1);
	JS_RunGC(js_rt->js_runtime);
	gf_js_lock(c, 0);
}

Bool gs_js_context_is_valid(JSContext *ctx)
{
	if (gf_list_find(js_rt->allocated_contexts, ctx) < 0)
		return GF_FALSE;
	return GF_TRUE;
}

/*
 * locking/try-locking the JS context
 *
 * */
GF_EXPORT
void gf_js_lock(struct JSContext *cx, Bool LockIt)
{
	if (!js_rt) return;

	if (LockIt) {
		gf_mx_p(js_rt->mx);
	} else {
		gf_mx_v(js_rt->mx);
	}
}

GF_EXPORT
Bool gf_js_try_lock(struct JSContext *cx)
{
	assert(cx);
	if (gf_mx_try_lock(js_rt->mx)) {
		return 1;
	}
	return 0;
}

JSRuntime *gf_js_get_rt()
{
	if (!js_rt) return NULL;
	return js_rt->js_runtime;
}



static JSValue js_print_ex(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, u32 ltool, u32 error_type)
{
    int i=0;
    Bool first=GF_TRUE;
    s32 logl = GF_LOG_INFO;
    JSValue v, g;
    const char *c_logname=NULL;
    const char *log_name = "JS";

    if ((argc>1) && JS_IsNumber(argv[0])) {
		JS_ToInt32(ctx, &logl, argv[0]);
		i=1;
	}

	if (error_type)
		logl = GF_LOG_ERROR;
	g = JS_GetGlobalObject(ctx);
	v = JS_GetPropertyStr(ctx, g, "_gpac_log_name");
	if (!JS_IsUndefined(v) && !JS_IsNull(v)) {
		c_logname = JS_ToCString(ctx, v);
		JS_FreeValue(ctx, v);
		if (c_logname) {
			log_name = c_logname;
			if (!strlen(log_name))
				log_name = NULL;
		}
	}
	JS_FreeValue(ctx, g);

	if (log_name) {
#ifndef GPAC_DISABLE_LOG
		GF_LOG(logl, ltool, ("[%s] ", log_name));
#else
		fprintf(stderr, "[%s] ", log_name);
#endif
	}
	if (error_type==2) {
#ifndef GPAC_DISABLE_LOG
		GF_LOG(logl, ltool, ("Throw "));
#else
		fprintf(stderr, "Throw ");
#endif
	}

    for (; i < argc; i++) {
		const char *str = JS_ToCString(ctx, argv[i]);
        if (!str) return GF_JS_EXCEPTION(ctx);

        if (logl==-1) {
			gf_sys_format_help(stderr, GF_PRINTARG_HIGHLIGHT_FIRST, "%s\n", str);
		} else if (logl==-2) {
			gf_sys_format_help(stderr, 0, "%s\n", str);
		} else if (logl<0) {
			fprintf(stderr, "%s%s", (first) ? "" : " ", str);
		} else {
#ifndef GPAC_DISABLE_LOG
			GF_LOG(logl, ltool, ("%s%s", (first) ? "" : " ", str));
#else
			fprintf(stderr, "%s%s", (first) ? "" : " ", str);
#endif
			if (JS_IsException(argv[i])) {
				js_dump_error_exc(ctx, argv[i]);
			}
		}
        JS_FreeCString(ctx, str);
        first=GF_FALSE;
    }
#ifndef GPAC_DISABLE_LOG
 	GF_LOG(logl, ltool, ("\n"));
#else
	fprintf(stderr, "\n");
#endif
	if (c_logname) JS_FreeCString(ctx, c_logname);
	return JS_UNDEFINED;
}
JSValue js_print(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_print_ex(ctx, this_val, argc, argv, GF_LOG_CONSOLE, GF_FALSE);
}

void js_dump_error_exc(JSContext *ctx, const JSValue exception_val)
{
    Bool is_error;
	u32 err_type = 1;
    is_error = JS_IsError(ctx, exception_val);
    if (!is_error) err_type = 2;

    js_print_ex(ctx, JS_NULL, 1, (JSValueConst *)&exception_val, GF_LOG_SCRIPT, err_type);

    if (is_error) {
        JSValue val = JS_GetPropertyStr(ctx, exception_val, "stack");
        if (!JS_IsUndefined(val)) {
			const char *stack = JS_ToCString(ctx, val);
#ifndef GPAC_DISABLE_LOG
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("%s\n", stack) );
#else
			fprintf(stderr, "%s\n", stack);
#endif
            JS_FreeCString(ctx, stack);
        }
        JS_FreeValue(ctx, val);
    }
}

void js_dump_error(JSContext *ctx)
{
    JSValue exception_val = JS_GetException(ctx);
	js_dump_error_exc(ctx, exception_val);
    JS_FreeValue(ctx, exception_val);
}

#ifdef GPAC_DISABLE_QJS_LIBC
void js_std_loop(JSContext *ctx)
{
	while (1) {
		JSContext *ctx1;
		int err = JS_ExecutePendingJob(JS_GetRuntime(ctx), &ctx1);
		if (err <= 0) {
			if (err < 0) {
				js_dump_error(ctx1);
			}
			break;
		}
	}
}
#endif




#define JS_CGETSET_MAGIC_DEF_ENUM(name, fgetter, fsetter, magic) { name, JS_PROP_CONFIGURABLE|JS_PROP_ENUMERABLE, JS_DEF_CGETSET_MAGIC, magic, .u = { .getset = { .get = { .getter_magic = fgetter }, .set = { .setter_magic = fsetter } } } }


static JSClassID bitstream_class_id = 0;
static JSClassID sha1_class_id = 0;
static JSClassID file_class_id = 0;
static JSClassID amix_class_id = 0;

typedef struct
{
	GF_BitStream *bs;
	JSValue buf_ref;
} JSBitstream;

#define GET_JSBS \
	JSBitstream *jbs = JS_GetOpaque(this_val, bitstream_class_id); \
	GF_BitStream *bs = jbs ? jbs->bs : NULL;


static void js_gpac_free(JSRuntime *rt, void *opaque, void *ptr)
{
	gf_free(ptr);
}

static void js_bs_finalize(JSRuntime *rt, JSValue this_val)
{
	GET_JSBS
	if (!bs) return;
	gf_bs_del(bs);
	JS_FreeValueRT(rt, jbs->buf_ref);
	gf_free(jbs);
}
static void js_bs_gc_mark(JSRuntime *rt, JSValueConst this_val, JS_MarkFunc *mark_func)
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
	GET_JSBS
	if (!bs) return GF_JS_EXCEPTION(ctx);
	return JS_NewInt32(ctx, gf_bs_read_u8(bs) );
}
static JSValue js_bs_get_s8(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s8 v;
	GET_JSBS
	if (!bs) return GF_JS_EXCEPTION(ctx);
	v = (s8) gf_bs_read_u8(bs);
	return JS_NewInt32(ctx, v );
}
static JSValue js_bs_get_u16(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GET_JSBS
	if (!bs) return GF_JS_EXCEPTION(ctx);
	return JS_NewInt32(ctx, gf_bs_read_u16(bs) );
}
static JSValue js_bs_get_u16_le(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GET_JSBS
	if (!bs) return GF_JS_EXCEPTION(ctx);
	return JS_NewInt32(ctx, gf_bs_read_u16_le(bs) );
}
static JSValue js_bs_get_s16(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s16 v;
	GET_JSBS
	if (!bs) return GF_JS_EXCEPTION(ctx);
	v = (s16) gf_bs_read_u16(bs);
	return JS_NewInt32(ctx, v );
}
static JSValue js_bs_get_u24(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GET_JSBS
	if (!bs) return GF_JS_EXCEPTION(ctx);
	return JS_NewInt32(ctx, gf_bs_read_u24(bs) );
}
static JSValue js_bs_get_s32(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s32 v;
	GET_JSBS
	if (!bs) return GF_JS_EXCEPTION(ctx);
	v = (s32) gf_bs_read_u32(bs);
	return JS_NewInt32(ctx, v);
}
static JSValue js_bs_get_u32(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GET_JSBS
	if (!bs) return GF_JS_EXCEPTION(ctx);
	return JS_NewInt32(ctx, gf_bs_read_u32(bs) );
}
static JSValue js_bs_get_u32_le(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GET_JSBS
	if (!bs) return GF_JS_EXCEPTION(ctx);
	return JS_NewInt32(ctx, gf_bs_read_u32_le(bs) );
}
static JSValue js_bs_get_u64(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GET_JSBS
	if (!bs) return GF_JS_EXCEPTION(ctx);
	return JS_NewInt64(ctx, gf_bs_read_u64(bs) );
}
static JSValue js_bs_get_u64_le(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GET_JSBS
	if (!bs) return GF_JS_EXCEPTION(ctx);
	return JS_NewInt64(ctx, gf_bs_read_u64_le(bs) );
}
static JSValue js_bs_get_s64(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s64 v;
	GET_JSBS
	if (!bs) return GF_JS_EXCEPTION(ctx);
	v = (s64) gf_bs_read_u64(bs);
	return JS_NewInt64(ctx, v );
}
static JSValue js_bs_get_float(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GET_JSBS
	if (!bs) return GF_JS_EXCEPTION(ctx);
	return JS_NewFloat64(ctx, gf_bs_read_float(bs) );
}
static JSValue js_bs_get_double(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GET_JSBS
	if (!bs) return GF_JS_EXCEPTION(ctx);
	return JS_NewFloat64(ctx, gf_bs_read_double(bs) );
}
static JSValue js_bs_get_bits(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s32 nb_bits;
	GET_JSBS
	if (!bs || !argc) return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt32(ctx, &nb_bits, argv[0])) return GF_JS_EXCEPTION(ctx);
	if (nb_bits<=32)
		return JS_NewInt32(ctx, gf_bs_read_int(bs, nb_bits) );
	return JS_NewInt64(ctx, gf_bs_read_long_int(bs, nb_bits) );
}

static JSValue js_bs_data_io(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, u32 mode)
{
	s32 nb_bytes=0;
	s64 offset=0;
	size_t data_size;
	u8 *data=NULL;
	GET_JSBS
	if (!bs) return GF_JS_EXCEPTION(ctx);
	if (argc<1) return GF_JS_EXCEPTION(ctx);

	if (JS_IsObject(argv[0])) {
		data = JS_GetArrayBuffer(ctx, &data_size, argv[0]);
		if (argc>1) {
			if (JS_ToInt32(ctx, &nb_bytes, argv[1])) return GF_JS_EXCEPTION(ctx);
			if ((mode==2) && (argc>2)) {
				if (JS_ToInt64(ctx, &offset, argv[2])) return GF_JS_EXCEPTION(ctx);

			}
		}
	}
	if (!data) return GF_JS_EXCEPTION(ctx);

	if (nb_bytes> (s32) data_size) nb_bytes = (s32) data_size;
	else if (!nb_bytes) nb_bytes = (s32) data_size;

	if (mode==1) {
		data_size = gf_bs_write_data(bs, data, nb_bytes);
	} else if (mode==2) {
		GF_Err e = gf_bs_insert_data(bs, data, nb_bytes, offset);
		if (e) js_throw_err(ctx, e);
		return JS_UNDEFINED;
	} else {
		data_size = gf_bs_read_data(bs, data, nb_bytes);
	}
	return JS_NewInt32(ctx, (s32) data_size);
}

static JSValue js_bs_get_data(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_bs_data_io(ctx, this_val, argc, argv, 0);
}
static JSValue js_bs_skip_bytes(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s32 nb_bytes;
	GET_JSBS
	if (!bs || !argc) return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt32(ctx, &nb_bytes, argv[0])) return GF_JS_EXCEPTION(ctx);
	gf_bs_skip_bytes(bs, nb_bytes);
	return JS_UNDEFINED;
}
static JSValue js_bs_is_align(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GET_JSBS
	if (!bs) return GF_JS_EXCEPTION(ctx);
	if (gf_bs_is_align(bs)) return JS_TRUE;
	return JS_FALSE;
}
static JSValue js_bs_align(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GET_JSBS
	if (!bs) return GF_JS_EXCEPTION(ctx);
	gf_bs_align(bs);
	return JS_UNDEFINED;
}
static JSValue js_bs_truncate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GET_JSBS
	if (!bs) return GF_JS_EXCEPTION(ctx);
	gf_bs_truncate(bs);
	return JS_UNDEFINED;
}
static JSValue js_bs_flush(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GET_JSBS
	if (!bs) return GF_JS_EXCEPTION(ctx);
	gf_bs_flush(bs);
	return JS_UNDEFINED;
}
static JSValue js_bs_epb_mode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GET_JSBS
	if (!bs || !argc) return GF_JS_EXCEPTION(ctx);
	gf_bs_enable_emulation_byte_removal(bs, (JS_ToBool(ctx, argv[0])) ? GF_TRUE : GF_FALSE);
	return JS_UNDEFINED;
}

static JSValue js_bs_peek(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u64 byte_offset=0;
	s32 nb_bits;
	GET_JSBS
	if (!bs || !argc) return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt32(ctx, &nb_bits, argv[0])) return GF_JS_EXCEPTION(ctx);
	if (argc>1) {
		if (JS_ToInt64(ctx, &byte_offset, argv[1])) return GF_JS_EXCEPTION(ctx);
	}
	return JS_NewInt32(ctx, gf_bs_peek_bits(bs, nb_bits, byte_offset));
}

static JSValue js_bs_put_val(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, u32 mode)
{
	s64 val;
	GET_JSBS
	if (!bs || !argc) return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt64(ctx, &val, argv[0])) return GF_JS_EXCEPTION(ctx);
	switch (mode) {
	case 0: gf_bs_write_u8(bs, (u32) val); break;
	case 1: gf_bs_write_u8(bs, (u32) val); break;
	case 2: gf_bs_write_u16(bs, (u32) val); break;
	case 3: gf_bs_write_u16_le(bs, (u32) val); break;
	case 4: gf_bs_write_u16(bs, (u32) val); break;
	case 5: gf_bs_write_u24(bs, (u32) val); break;
	case 6: gf_bs_write_u32(bs, (u32) val); break;
	case 7: gf_bs_write_u32_le(bs, (u32) val); break;
	case 8: gf_bs_write_u32(bs, (u32) val); break;
	case 9: gf_bs_write_u64(bs, (u64) val); break;
	case 10: gf_bs_write_u64_le(bs, (u64) val); break;
	case 11: gf_bs_write_u64(bs, (u64) val); break;
	}
	return JS_UNDEFINED;
}
static JSValue js_bs_put_u8(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_bs_put_val(ctx, this_val, argc, argv, 0);
}
static JSValue js_bs_put_s8(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_bs_put_val(ctx, this_val, argc, argv, 1);
}
static JSValue js_bs_put_u16(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_bs_put_val(ctx, this_val, argc, argv, 2);
}
static JSValue js_bs_put_u16_le(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_bs_put_val(ctx, this_val, argc, argv, 3);
}
static JSValue js_bs_put_s16(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_bs_put_val(ctx, this_val, argc, argv, 4);
}
static JSValue js_bs_put_u24(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_bs_put_val(ctx, this_val, argc, argv, 5);
}
static JSValue js_bs_put_u32(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_bs_put_val(ctx, this_val, argc, argv, 6);
}
static JSValue js_bs_put_u32_le(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_bs_put_val(ctx, this_val, argc, argv, 7);
}
static JSValue js_bs_put_s32(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_bs_put_val(ctx, this_val, argc, argv, 8);
}
static JSValue js_bs_put_u64(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_bs_put_val(ctx, this_val, argc, argv, 9);
}
static JSValue js_bs_put_u64_le(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_bs_put_val(ctx, this_val, argc, argv, 10);
}
static JSValue js_bs_put_s64(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_bs_put_val(ctx, this_val, argc, argv, 11);
}
static JSValue js_bs_put_bits(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s64 val;
	s32 nb_bits;
	GET_JSBS
	if (!bs || (argc!=2)) return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt64(ctx, &val, argv[0])) return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt32(ctx, &nb_bits, argv[1])) return GF_JS_EXCEPTION(ctx);
	if (nb_bits<=32)
		gf_bs_write_int(bs, (u32) val, nb_bits);
	else
		gf_bs_write_long_int(bs, val, nb_bits);
	return JS_UNDEFINED;
}
static JSValue js_bs_put_float(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Double val;
	GET_JSBS
	if (!bs || (argc!=1)) return GF_JS_EXCEPTION(ctx);
	if (JS_ToFloat64(ctx, &val, argv[0])) return GF_JS_EXCEPTION(ctx);
	gf_bs_write_float(bs, (Float) val);
	return JS_UNDEFINED;
}
static JSValue js_bs_put_double(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Double val;
	GET_JSBS
	if (!bs || (argc!=1)) return GF_JS_EXCEPTION(ctx);
	if (JS_ToFloat64(ctx, &val, argv[0])) return GF_JS_EXCEPTION(ctx);
	gf_bs_write_double(bs, val);
	return JS_UNDEFINED;
}
static JSValue js_bs_put_data(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_bs_data_io(ctx, this_val, argc, argv, 1);
}
static JSValue js_bs_insert_data(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_bs_data_io(ctx, this_val, argc, argv, 2);
}

static JSValue js_bs_get_content(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u8 *data;
	u32 size;
	JSValue res;
	GET_JSBS
	if (!bs) return GF_JS_EXCEPTION(ctx);
	gf_bs_get_content(bs, &data, &size);

	if (data) {
		res = JS_NewArrayBuffer(ctx, data, size, js_gpac_free, NULL, 0);
	} else {
		res = JS_NULL;
	}
	return res;
}

static JSValue js_bs_transfer(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Bool keep_src=GF_FALSE;
	GF_Err e;
	JSBitstream *jssrcbs;
	GET_JSBS
	if (!bs || !argc) return GF_JS_EXCEPTION(ctx);

	jssrcbs = JS_GetOpaque(argv[0], bitstream_class_id);
	if (!jssrcbs || !jssrcbs->bs) return GF_JS_EXCEPTION(ctx);
	if (argc>1) {
		keep_src = JS_ToBool(ctx, argv[1]);
	}
	e = gf_bs_transfer(bs, jssrcbs->bs, keep_src);
	if (e) return js_throw_err(ctx, e);
	return JS_UNDEFINED;
}

enum
{
	JS_BS_POS=0,
	JS_BS_SIZE,
	JS_BS_BIT_OFFSET,
	JS_BS_BIT_POS,
	JS_BS_AVAILABLE,
	JS_BS_BITS_AVAILABLE,
	JS_BS_REFRESH_SIZE,
};

static JSValue js_bs_prop_get(JSContext *ctx, JSValueConst this_val, int magic)
{
	GET_JSBS
	if (!bs) return GF_JS_EXCEPTION(ctx);
	switch (magic) {
	case JS_BS_POS:
		return JS_NewInt64(ctx, gf_bs_get_position(bs));
	case JS_BS_SIZE:
		return JS_NewInt64(ctx, gf_bs_get_size(bs));
	case JS_BS_REFRESH_SIZE:
		return JS_NewInt64(ctx, gf_bs_get_refreshed_size(bs));
	case JS_BS_BIT_OFFSET:
		return JS_NewInt64(ctx, gf_bs_get_bit_offset(bs));
	case JS_BS_BIT_POS:
		return JS_NewInt64(ctx, gf_bs_get_bit_position(bs));
	case JS_BS_AVAILABLE:
		return JS_NewInt64(ctx, gf_bs_available(bs));
	case JS_BS_BITS_AVAILABLE:
		return JS_NewInt32(ctx, gf_bs_bits_available(bs));
	}
	return JS_UNDEFINED;

}
static JSValue js_bs_prop_set(JSContext *ctx, JSValueConst this_val, JSValueConst value, int magic)
{
	s64 ival;
	GET_JSBS
	if (!bs) return GF_JS_EXCEPTION(ctx);

	switch (magic) {
	case JS_BS_POS:
		if (JS_ToInt64(ctx, &ival, value)) return GF_JS_EXCEPTION(ctx);
		gf_bs_seek(bs, ival);
		break;
	}
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry bitstream_funcs[] = {
    JS_CGETSET_MAGIC_DEF_ENUM("pos", js_bs_prop_get, js_bs_prop_set, JS_BS_POS),
    JS_CGETSET_MAGIC_DEF_ENUM("size", js_bs_prop_get, NULL, JS_BS_SIZE),
    JS_CGETSET_MAGIC_DEF_ENUM("bit_offset", js_bs_prop_get, NULL, JS_BS_BIT_OFFSET),
    JS_CGETSET_MAGIC_DEF_ENUM("bit_pos", js_bs_prop_get, NULL, JS_BS_BIT_POS),
    JS_CGETSET_MAGIC_DEF_ENUM("available", js_bs_prop_get, NULL, JS_BS_AVAILABLE),
    JS_CGETSET_MAGIC_DEF_ENUM("bits_available", js_bs_prop_get, NULL, JS_BS_BITS_AVAILABLE),
    JS_CGETSET_MAGIC_DEF_ENUM("refreshed_size", js_bs_prop_get, NULL, JS_BS_REFRESH_SIZE),

	JS_CFUNC_DEF("skip", 0, js_bs_skip_bytes),
	JS_CFUNC_DEF("is_align", 0, js_bs_is_align),
	JS_CFUNC_DEF("align", 0, js_bs_align),
	JS_CFUNC_DEF("truncate", 0, js_bs_truncate),
	JS_CFUNC_DEF("peek", 0, js_bs_peek),
	JS_CFUNC_DEF("flush", 0, js_bs_flush),
	JS_CFUNC_DEF("epb_mode", 0, js_bs_epb_mode),

	JS_CFUNC_DEF("get_u8", 0, js_bs_get_u8),
	JS_CFUNC_DEF("get_s8", 0, js_bs_get_s8),
	JS_CFUNC_DEF("get_u16", 0, js_bs_get_u16),
	JS_CFUNC_DEF("get_u16_le", 0, js_bs_get_u16_le),
	JS_CFUNC_DEF("get_s16", 0, js_bs_get_s16),
	JS_CFUNC_DEF("get_u24", 0, js_bs_get_u24),
	JS_CFUNC_DEF("get_u32", 0, js_bs_get_u32),
	JS_CFUNC_DEF("get_u32_le", 0, js_bs_get_u32_le),
	JS_CFUNC_DEF("get_s32", 0, js_bs_get_s32),
	JS_CFUNC_DEF("get_u64", 0, js_bs_get_u64),
	JS_CFUNC_DEF("get_u64_le", 0, js_bs_get_u64_le),
	JS_CFUNC_DEF("get_s64", 0, js_bs_get_s64),
	JS_CFUNC_DEF("get_bits", 0, js_bs_get_bits),
	JS_CFUNC_DEF("get_float", 0, js_bs_get_float),
	JS_CFUNC_DEF("get_double", 0, js_bs_get_double),
	JS_CFUNC_DEF("get_data", 0, js_bs_get_data),
	JS_CFUNC_DEF("put_u8", 0, js_bs_put_u8),
	JS_CFUNC_DEF("put_s8", 0, js_bs_put_s8),
	JS_CFUNC_DEF("put_u16", 0, js_bs_put_u16),
	JS_CFUNC_DEF("put_u16_le", 0, js_bs_put_u16_le),
	JS_CFUNC_DEF("put_s16", 0, js_bs_put_s16),
	JS_CFUNC_DEF("put_u24", 0, js_bs_put_u24),
	JS_CFUNC_DEF("put_u32", 0, js_bs_put_u32),
	JS_CFUNC_DEF("put_u32_le", 0, js_bs_put_u32_le),
	JS_CFUNC_DEF("put_s32", 0, js_bs_put_s32),
	JS_CFUNC_DEF("put_u64", 0, js_bs_put_u64),
	JS_CFUNC_DEF("put_u64_le", 0, js_bs_put_u64_le),
	JS_CFUNC_DEF("put_s64", 0, js_bs_put_s64),
	JS_CFUNC_DEF("put_bits", 0, js_bs_put_bits),
	JS_CFUNC_DEF("put_float", 0, js_bs_put_float),
	JS_CFUNC_DEF("put_double", 0, js_bs_put_double),
	JS_CFUNC_DEF("put_data", 0, js_bs_put_data),
	JS_CFUNC_DEF("insert_data", 0, js_bs_insert_data),
	JS_CFUNC_DEF("get_content", 0, js_bs_get_content),
	JS_CFUNC_DEF("transfer", 0, js_bs_transfer),
};

static JSValue bitstream_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	JSValue anobj;
	JSBitstream *jbs;
	GF_BitStream *bs = NULL;

	if (!argc) {
		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	} else if (JS_IsObject(argv[0])){
		FILE *f = JS_GetOpaque(argv[0], file_class_id);
		if (f) {
			if ((argc>1) && JS_ToBool(ctx, argv[1])) {
				bs = gf_bs_from_file(f, GF_BITSTREAM_WRITE);
			} else {
				bs = gf_bs_from_file(f, GF_BITSTREAM_READ);
			}
		} else {
			u8 *data;
			size_t data_size;

			data = JS_GetArrayBuffer(ctx, &data_size, argv[0]);
			if (!data) return GF_JS_EXCEPTION(ctx);
			if ((argc>1) && JS_ToBool(ctx, argv[1])) {
				bs = gf_bs_new(data, data_size, GF_BITSTREAM_WRITE);
			} else {
				bs = gf_bs_new(data, data_size, GF_BITSTREAM_READ);
			}
		}
	} else {
		return GF_JS_EXCEPTION(ctx);
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
	JS_SYS_TEST_MODE,
	JS_SYS_OLD_ARCH,
	JS_SYS_LOG_COLOR,
	JS_SYS_QUIET,
	JS_SYS_USERNAME,
	JS_SYS_TIMEZONE,
	JS_SYS_FILES_OPEN,
	JS_SYS_CACHE_DIR,
	JS_SYS_SHARED_DIR,
	JS_SYS_VERSION,
	JS_SYS_VERSION_FULL,
	JS_SYS_COPYRIGHT,
	JS_SYS_V_MAJOR,
	JS_SYS_V_MINOR,
	JS_SYS_V_MICRO,
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

	case JS_SYS_TEST_MODE:
		return JS_NewBool(ctx, gf_sys_is_test_mode() );
	case JS_SYS_OLD_ARCH:
		return JS_NewBool(ctx, gf_sys_old_arch_compat() );
	case JS_SYS_LOG_COLOR:
#ifdef GPAC_DISABLE_LOG
		return JS_NewBool(ctx, GF_FALSE );
#else
		return JS_NewBool(ctx, gf_log_use_color() );
#endif
	case JS_SYS_QUIET:
		return JS_NewBool(ctx, gf_sys_is_quiet() );
	case JS_SYS_USERNAME:
		{
			char username[1024];
			gf_get_user_name((char*)username);
			return JS_NewString(ctx, username);
		}
	case JS_SYS_TIMEZONE:
		return JS_NewInt32(ctx, gf_net_get_timezone());

	case JS_SYS_FILES_OPEN:
		return JS_NewInt32(ctx, gf_file_handles_count());

	case JS_SYS_CACHE_DIR:
		res = gf_get_default_cache_directory();
		if (res) return JS_NewString(ctx, res);
		return JS_NULL;

	case JS_SYS_SHARED_DIR:
	{
		char szDir[GF_MAX_PATH];
		if (! gf_opts_default_shared_directory(szDir)) return JS_NULL;
		return JS_NewString(ctx, szDir);
	}

	case JS_SYS_VERSION:
		return JS_NewString(ctx, GPAC_VERSION );
	case JS_SYS_VERSION_FULL:
		return JS_NewString(ctx, gf_gpac_version() );
	case JS_SYS_COPYRIGHT:
		return JS_NewString(ctx, gf_gpac_copyright() );

	case JS_SYS_V_MAJOR:
		return JS_NewInt32(ctx, GPAC_VERSION_MAJOR );
	case JS_SYS_V_MINOR:
		return JS_NewInt32(ctx, GPAC_VERSION_MINOR );
	case JS_SYS_V_MICRO:
		return JS_NewInt32(ctx, GPAC_VERSION_MICRO );
	}

	return JS_UNDEFINED;
}

static JSValue js_sys_prop_set(JSContext *ctx, JSValueConst this_val, JSValueConst value, int magic)
{
	const char *prop_val;
	switch (magic) {
	case JS_SYS_LAST_WORK_DIR:
		if (!JS_IsString(value)) return GF_JS_EXCEPTION(ctx);
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
	if (argc < 2) return GF_JS_EXCEPTION(ctx);

	if (JS_ToInt32(ctx, &idx, argv[0]))
		return GF_JS_EXCEPTION(ctx);
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

	err = gf_enum_directory(url ? url : dir, 1, js_enum_dir_fct, &cbk, filter);
	if (err) return GF_JS_EXCEPTION(ctx);

	if (!dir_only) {
		cbk.is_dir = 0;
		err = gf_enum_directory(url ? url : dir, 0, js_enum_dir_fct, &cbk, filter);
		if (err) return GF_JS_EXCEPTION(ctx);
	}

	if (url) gf_free(url);
	JS_FreeCString(ctx, dir);
	JS_FreeCString(ctx, filter);
	return cbk.array;
}


static JSValue js_sys_error_string(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Err e;
	if (argc < 1) return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt32(ctx, (int32_t *) &e, argv[0]))
		return GF_JS_EXCEPTION(ctx);
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
	char input[4096], *read;
	u32 len;
//#ifdef GPAC_ENABLE_COVERAGE
    if (argc) {
		return JS_NewString(ctx, "Coverage OK");
	}
//#endif // GPAC_ENABLE_COVERAGE

	read = fgets(input, 4095, stdin);
	if (!read) return JS_NULL;
	input[4095]=0;
	len = (u32) strlen(input);
	if (len && (input[len-1] == '\n')) {
        input[len-1] = 0;
        len--;
	}
	if (!len) return JS_NULL;
	return JS_NewString(ctx, input);
}

static JSValue js_sys_prompt_echo_off(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Bool echo_off;
	if (!argc) return GF_JS_EXCEPTION(ctx);
	echo_off = JS_ToBool(ctx, argv[0]);
	if (argc<2)
		gf_prompt_set_echo_off(echo_off);
	return JS_UNDEFINED;
}
static JSValue js_sys_prompt_code(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s32 code;
	if (!argc) return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt32(ctx, &code, argv[0])) return GF_JS_EXCEPTION(ctx);

	gf_sys_set_console_code(stderr, code);
	return JS_UNDEFINED;
}
static JSValue js_sys_prompt_size(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 w, h;
	JSValue res;
	GF_Err e = gf_prompt_get_size(&w, &h);
	if (e) return JS_NULL;
	res = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, res, "w", JS_NewInt32(ctx, w));
	JS_SetPropertyStr(ctx, res, "h", JS_NewInt32(ctx, h));
	return res;
}

const char *gf_dom_get_friendly_name(u32 key_identifier);

static JSValue js_sys_keyname(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifndef GPAC_DISABLE_SVG
	u32 code;
	if (JS_ToInt32(ctx, &code, argv[0])) return GF_JS_EXCEPTION(ctx);
	return JS_NewString(ctx, gf_dom_get_friendly_name(code));
#else
	return js_throw_err(ctx, GF_NOT_SUPPORTED);
#endif
}

GF_EventType gf_dom_event_type_by_name(const char *name);

static JSValue js_sys_evt_by_name(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifndef GPAC_DISABLE_SVG
	if (!argc) return GF_JS_EXCEPTION(ctx);
	const char *name = JS_ToCString(ctx, argv[0]);
	if (!name) return GF_JS_EXCEPTION(ctx);
	JSValue res = JS_NewInt32(ctx, gf_dom_event_type_by_name(name));
	JS_FreeCString(ctx, name);
	return res;
#else
	return js_throw_err(ctx, GF_NOT_SUPPORTED);
#endif

}


static JSValue js_sys_gc(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JS_RunGC(JS_GetRuntime(ctx));
    return JS_UNDEFINED;
}

static JSValue js_sys_clock(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return JS_NewInt32(ctx, gf_sys_clock() );
}
static JSValue js_sys_clock_high_res(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return JS_NewInt64(ctx, gf_sys_clock_high_res() );
}
static JSValue js_sys_4cc_to_str(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u64 val;
	if (!argc) return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt64(ctx, &val, argv[0])) return GF_JS_EXCEPTION(ctx);
	return JS_NewString(ctx, gf_4cc_to_str((u32) val) );
}

static JSValue js_sys_rand_init(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Bool reset=GF_FALSE;
	if (argc) reset = JS_ToBool(ctx, argv[0]);
	gf_rand_init(reset);
	return JS_UNDEFINED;
}
static JSValue js_sys_rand(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return JS_NewInt32(ctx, gf_rand());
}
static JSValue js_sys_rand64(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u64 lrand = (u64) gf_rand();
	lrand<<=32;
	lrand |= gf_rand();
	return JS_NewInt64(ctx, lrand);
}

static JSValue js_sys_getenv(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *str, *val;
	if (!argc) return GF_JS_EXCEPTION(ctx);
	str = JS_ToCString(ctx, argv[0]);
	if (!str) return GF_JS_EXCEPTION(ctx);
	val = getenv(str);
	JS_FreeCString(ctx, str);
	return val ? JS_NewString(ctx, val) : JS_NULL;
}

static JSValue js_sys_get_utc(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 y, mo, d, h, m, s;
	if (!argc) return JS_NewInt64(ctx, gf_net_get_utc() );
	if (argc==1) {
		u64 time;
		const char *date = JS_ToCString(ctx, argv[0]);
		if (!date) return GF_JS_EXCEPTION(ctx);
		time = gf_net_parse_date(date);
		JS_FreeCString(ctx, date);
		return JS_NewInt64(ctx, time);
	}

	if (argc != 6) return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt32(ctx, &y, argv[0])) return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt32(ctx, &mo, argv[1])) return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt32(ctx, &d, argv[2])) return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt32(ctx, &h, argv[3])) return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt32(ctx, &m, argv[4])) return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt32(ctx, &s, argv[5])) return GF_JS_EXCEPTION(ctx);

	return JS_NewInt64(ctx, gf_net_get_utc_ts(y, mo, d, h, m, s) );
}

static JSValue js_sys_get_ntp(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 sec, frac;
	gf_net_get_ntp(&sec, &frac);
	JSValue ret = JS_NewObject(ctx);
	if (JS_IsException(ret)) return ret;
	JS_SetPropertyStr(ctx, ret, "n", JS_NewInt64(ctx, sec));
	JS_SetPropertyStr(ctx, ret, "d", JS_NewInt64(ctx, frac));
	return ret;
}

static JSValue js_sys_ntp_shift(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JSValue v;
	u64 sec, frac, shift;
	s64 frac_usec;
	if ((argc<2) || !JS_IsObject(argv[0]))
		return GF_JS_EXCEPTION(ctx);

	v = JS_GetPropertyStr(ctx, argv[0], "n");
	if (JS_IsNull(v)) return GF_JS_EXCEPTION(ctx);
	JS_ToInt64(ctx, &sec, v);
	JS_FreeValue(ctx, v);

	v = JS_GetPropertyStr(ctx, argv[0], "d");
	if (JS_IsNull(v)) return GF_JS_EXCEPTION(ctx);
	JS_ToInt64(ctx, &frac, v);
	JS_FreeValue(ctx, v);

	JS_ToInt64(ctx, &shift, argv[1]);
	frac_usec = (s64) (frac * 1000000) / 0xFFFFFFFFULL;
	frac_usec += shift;
	while (frac_usec<0) {
		frac_usec += 1000000;
		sec -= 1;
	}
	while (frac_usec>1000000) {
		frac_usec -= 1000000;
		sec += 1;
	}
	frac = (frac_usec * 0xFFFFFFFFULL) / 1000000;
	v = JS_NewObject(ctx);
	if (JS_IsException(v)) return v;
	JS_SetPropertyStr(ctx, v, "n", JS_NewInt64(ctx, sec));
	JS_SetPropertyStr(ctx, v, "d", JS_NewInt64(ctx, frac));
	return v;
}


static JSValue js_sys_sleep(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 sleep_for=0;
	if (argc==1) {
		if (JS_ToInt32(ctx, &sleep_for, argv[0]))
			return GF_JS_EXCEPTION(ctx);
	}
	gf_sleep(sleep_for);
	return JS_UNDEFINED;
}


static JSValue js_sys_exit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 rval=0;
	if (argc==2) {
		return JS_UNDEFINED;
	}
	if (argc==1) {
		if (JS_ToInt32(ctx, &rval, argv[0]))
			return GF_JS_EXCEPTION(ctx);
	}
	exit(rval);
	return JS_UNDEFINED;
}
static JSValue js_sys_crc32(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const u8 *data;
	size_t data_size;

	if (!argc) return GF_JS_EXCEPTION(ctx);
	if (JS_IsString(argv[0])) {
		u32 crc=0;
		const char *str = JS_ToCString(ctx, argv[0]);
		if (str) {
			crc = gf_crc_32(str, (u32) strlen(str) );
			JS_FreeCString(ctx, str);
		}
		return JS_NewInt32(ctx, crc );
	} else {
		data = JS_GetArrayBuffer(ctx, &data_size, argv[0] );
		if (!data) return GF_JS_EXCEPTION(ctx);
		return JS_NewInt32(ctx, gf_crc_32(data, (u32) data_size) );
	}
	return GF_JS_EXCEPTION(ctx);
}

static JSValue js_sys_sha1(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u8 csum[GF_SHA1_DIGEST_SIZE];
	const u8 *data;
	size_t data_size;

	if (!argc) return GF_JS_EXCEPTION(ctx);
	if (JS_IsString(argv[0])) {
		const char *filename = JS_ToCString(ctx, argv[0]);
		if (!filename) return GF_JS_EXCEPTION(ctx);
		gf_sha1_file(filename, csum);
		JS_FreeCString(ctx, filename);
		return JS_NewArrayBufferCopy(ctx, csum, GF_SHA1_DIGEST_SIZE);
	} else if (JS_IsObject(argv[0])) {
		data = JS_GetArrayBuffer(ctx, &data_size, argv[0] );
		if (!data) return GF_JS_EXCEPTION(ctx);

		gf_sha1_csum((u8 *) data, (u32) data_size, csum);
		return JS_NewArrayBufferCopy(ctx, csum, GF_SHA1_DIGEST_SIZE);
	}
	return GF_JS_EXCEPTION(ctx);
}


static JSValue js_sys_file_data(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *filename;
	u8 *data;
	u32 data_size;
	Bool as_utf8 = GF_FALSE;
	GF_Err e;
	JSValue res;
	if (!argc || !JS_IsString(argv[0])) return GF_JS_EXCEPTION(ctx);
	filename = JS_ToCString(ctx, argv[0]);
	if (!filename) return GF_JS_EXCEPTION(ctx);
	if ((argc>1) && JS_ToBool(ctx, argv[1]))
		as_utf8 = GF_TRUE;

	e = gf_file_load_data(filename, &data, &data_size);
	if (e) {
		res = js_throw_err_msg(ctx, e, "Failed to load file %s", filename);
	} else if (as_utf8) {
		if (!data || gf_utf8_is_legal(data, data_size)) {
			res = JS_NewString(ctx, data ? (const char *) data : "");
		} else {
			res = js_throw_err_msg(ctx, GF_NON_COMPLIANT_BITSTREAM, "Invalid UTF8 data in file %s", filename);
		}
		if (data)
			gf_free(data);
	} else {
		res = JS_NewArrayBuffer(ctx, data, data_size, js_gpac_free, NULL, 0);
	}
	JS_FreeCString(ctx, filename);
	return res;
}

/* load and evaluate a file */
static JSValue js_sys_load_script(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *filename;
    u8 *data=NULL;
    u32 data_size;
    JSValue res;
    GF_Err e;
	char *full_url = NULL;

	if (!argc || !JS_IsString(argv[0])) return GF_JS_EXCEPTION(ctx);
    filename = JS_ToCString(ctx, argv[0]);
    if (!filename) return GF_JS_EXCEPTION(ctx);

	if ((argc>1) && JS_ToBool(ctx, argv[1]) ) {
		const char *par_url = jsf_get_script_filename(ctx);
		full_url = gf_url_concatenate(par_url, filename);
		JS_FreeCString(ctx, filename);
		filename = full_url;
	}

	e = gf_file_load_data(filename, &data, &data_size);
	if (e) {
		res = js_throw_err_msg(ctx, e, "Failed to load file %s", filename);
	} else if (data) {
		if (!gf_utf8_is_legal(data, data_size)) {
			res = js_throw_err_msg(ctx, e, "Script file %s is not UTF-8", filename);
		} else {
			res = JS_Eval(ctx, (char *)data, data_size, filename, JS_EVAL_TYPE_GLOBAL);
		}
	} else {
		res = JS_UNDEFINED;
	}
    if (data) gf_free(data);

    if (full_url)
		gf_free(full_url);
    else
		JS_FreeCString(ctx, filename);
    return res;
}


static JSValue js_sys_compress_ex(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, Bool is_decomp)
{
	const u8 *data;
	size_t data_size;
	u32 out_size=0;
	u8 *out_ptr = NULL;
	JSValue res;
	GF_Err e;
	if (!argc || !JS_IsObject(argv[0])) return GF_JS_EXCEPTION(ctx);
	data = JS_GetArrayBuffer(ctx, &data_size, argv[0] );
	if (!data) return GF_JS_EXCEPTION(ctx);
	if (is_decomp) {
		e = gf_gz_decompress_payload((u8*) data, (u32) data_size, &out_ptr, &out_size);
	} else {
		e = gf_gz_compress_payload_ex((u8 **)&data, (u32) data_size, &out_size, 0, GF_FALSE, &out_ptr);
	}

	if (e) return js_throw_err(ctx, e);
	res = JS_NewArrayBuffer(ctx, out_ptr, out_size, js_gpac_free, NULL, 0);
	return res;
}
static JSValue js_sys_compress(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_sys_compress_ex(ctx, this_val, argc, argv, GF_FALSE);
}
static JSValue js_sys_decompress(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_sys_compress_ex(ctx, this_val, argc, argv, GF_TRUE);
}

static JSValue js_sys_url_cat(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *parent;
	const char *src;
	char *url;
	JSValue res;
	if (argc<2) return GF_JS_EXCEPTION(ctx);
	parent = JS_ToCString(ctx, argv[0]);
	src = JS_ToCString(ctx, argv[1]);
	url = gf_url_concatenate(parent, src);
	if (url) {
		res = JS_NewString(ctx, url);
		gf_free(url);
	} else {
		res = JS_NewString(ctx, src);
	}
	JS_FreeCString(ctx, parent);
	JS_FreeCString(ctx, src);
	return res;
}


static JSValue js_sys_rect_union_ex(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, Bool is_inter)
{
	GF_Rect rc1, rc2;
	Double fval;
	JSValue v;
	int res;
	if (argc<2) return GF_JS_EXCEPTION(ctx);

#define GET_PROP(_obj, _n, _f)\
	_f = 0;\
	v = JS_GetPropertyStr(ctx, _obj, _n);\
	res = JS_ToFloat64(ctx, &fval, v);\
	if (!res) _f = FLT2FIX(fval);\
	JS_FreeValue(ctx, v);\

	GET_PROP(argv[0], "x", rc1.x)
	GET_PROP(argv[0], "y", rc1.y)
	GET_PROP(argv[0], "w", rc1.width)
	GET_PROP(argv[0], "h", rc1.height)
	GET_PROP(argv[1], "x", rc2.x)
	GET_PROP(argv[1], "y", rc2.y)
	GET_PROP(argv[1], "w", rc2.width)
	GET_PROP(argv[1], "h", rc2.height)

#undef GET_PROP

	if (is_inter) {
		gf_rect_intersect(&rc1, &rc2);
	} else {
		gf_rect_union(&rc1, &rc2);
	}
	v = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, v, "x", JS_NewFloat64(ctx, FIX2FLT(rc1.x) ));
	JS_SetPropertyStr(ctx, v, "y", JS_NewFloat64(ctx, FIX2FLT(rc1.y) ));
	JS_SetPropertyStr(ctx, v, "w", JS_NewFloat64(ctx, FIX2FLT(rc1.width) ));
	JS_SetPropertyStr(ctx, v, "h", JS_NewFloat64(ctx, FIX2FLT(rc1.height) ));
	return v;
}
static JSValue js_sys_rect_union(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_sys_rect_union_ex(ctx, this_val, argc, argv, GF_FALSE);
}
static JSValue js_sys_rect_intersect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_sys_rect_union_ex(ctx, this_val, argc, argv, GF_TRUE);
}

enum
{
	OPT_RMDIR,
	OPT_MKDIR,
	OPT_DIREXISTS,
	OPT_DIRCLEAN,
	OPT_FILEBASENAME,
	OPT_FILEEXT,
	OPT_FILEDEL,
	OPT_FILEMODTIME,
	OPT_FILEEXISTS,
	OPT_FILEMOVE,
};

static JSValue js_sys_file_opt(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, u32 file_opt)
{
	const char *dirname, *newfile;
	char *ext;
	GF_Err e;
	JSValue res;
	if (!argc || !JS_IsString(argv[0])) return GF_JS_EXCEPTION(ctx);
	dirname = JS_ToCString(ctx, argv[0]);
	if (!dirname) return GF_JS_EXCEPTION(ctx);

	res = JS_UNDEFINED;
	switch (file_opt) {
	case OPT_RMDIR:
		e = gf_rmdir(dirname);
		if (e) res = js_throw_err_msg(ctx, e, "Failed to remove dir %s", dirname);
		break;
	case OPT_MKDIR:
		e = gf_mkdir(dirname);
		if (e) res = js_throw_err_msg(ctx, e, "Failed to create dir %s", dirname);
		break;
	case OPT_DIREXISTS:
		res = JS_NewBool(ctx, gf_dir_exists(dirname) );
		break;
	case OPT_DIRCLEAN:
		e = gf_dir_cleanup(dirname);
		if (e) res = js_throw_err_msg(ctx, e, "Failed to clean dir %s", dirname);
		break;
	case OPT_FILEBASENAME:
		res = JS_NewString(ctx, gf_file_basename(dirname) );
		break;
	case OPT_FILEEXT:
		ext = gf_file_ext_start(dirname);
		res = ext ? JS_NewString(ctx, ext) : JS_NULL;
		break;
	case OPT_FILEDEL:
		e = gf_file_delete(dirname);
		if (e) res = js_throw_err_msg(ctx, e, "Failed to delete file %s", dirname);
		break;
	case OPT_FILEMODTIME:
		res = JS_NewInt64(ctx, gf_file_modification_time(dirname) );
		break;
	case OPT_FILEEXISTS:
		res = JS_NewBool(ctx, gf_file_exists(dirname) );
		break;
	case OPT_FILEMOVE:
		newfile = (argc<2) ? NULL : JS_ToCString(ctx, argv[1]);
		if (!newfile) res = js_throw_err_msg(ctx, GF_BAD_PARAM, "Missing new file name");
		else {
			e = gf_file_move(dirname, newfile);
			if (e) res = js_throw_err_msg(ctx, e, "Failed to move file %s to %s", dirname, newfile);
			JS_FreeCString(ctx, newfile);
		}
		break;
	}

	JS_FreeCString(ctx, dirname);
	return res;
}

static JSValue js_sys_rmdir(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_sys_file_opt(ctx, this_val, argc, argv, OPT_RMDIR);
}
static JSValue js_sys_mkdir(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_sys_file_opt(ctx, this_val, argc, argv, OPT_MKDIR);
}
static JSValue js_sys_dir_exists(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_sys_file_opt(ctx, this_val, argc, argv, OPT_DIREXISTS);
}
static JSValue js_sys_dir_clean(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_sys_file_opt(ctx, this_val, argc, argv, OPT_DIRCLEAN);
}
static JSValue js_sys_basename(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_sys_file_opt(ctx, this_val, argc, argv, OPT_FILEBASENAME);
}
static JSValue js_sys_file_ext(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_sys_file_opt(ctx, this_val, argc, argv, OPT_FILEEXT);
}
static JSValue js_sys_file_exists(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_sys_file_opt(ctx, this_val, argc, argv, OPT_FILEEXISTS);
}
static JSValue js_sys_file_del(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_sys_file_opt(ctx, this_val, argc, argv, OPT_FILEDEL);
}
static JSValue js_sys_file_modtime(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_sys_file_opt(ctx, this_val, argc, argv, OPT_FILEMODTIME);
}
static JSValue js_sys_file_move(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_sys_file_opt(ctx, this_val, argc, argv, OPT_FILEMOVE);
}


static JSValue js_sys_get_opt(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *sec, *key, *val;
	JSValue res;
	if (argc!=2) return GF_JS_EXCEPTION(ctx);
	sec = JS_ToCString(ctx, argv[0]);
	if (!sec) return GF_JS_EXCEPTION(ctx);
	key = JS_ToCString(ctx, argv[1]);
	if (!key) {
		JS_FreeCString(ctx, sec);
		return GF_JS_EXCEPTION(ctx);
	}
	val = gf_opts_get_key(sec, key);
	res = val ? JS_NewString(ctx, val) : JS_NULL;
	JS_FreeCString(ctx, sec);
	JS_FreeCString(ctx, key);
	return res;
}

static JSValue js_sys_set_opt(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *sec, *key, *val;
	if (argc<2) return GF_JS_EXCEPTION(ctx);
	sec = JS_ToCString(ctx, argv[0]);
	if (!sec) return GF_JS_EXCEPTION(ctx);
	key = JS_ToCString(ctx, argv[1]);
	if (!key) {
		JS_FreeCString(ctx, sec);
		return GF_JS_EXCEPTION(ctx);
	}
	val = NULL;
	if (argc>2)
		val = JS_ToCString(ctx, argv[2]);

	gf_opts_set_key(sec, key, val);

	JS_FreeCString(ctx, sec);
	JS_FreeCString(ctx, key);
	if (val)
		JS_FreeCString(ctx, val);
	return JS_UNDEFINED;
}
static JSValue js_sys_discard_opts(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	gf_opts_discard_changes();
	return JS_UNDEFINED;
}

static JSValue js_sys_basecode_ex(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, Bool is_dec, Bool is_16)
{
	u32 out_size=0;
	u8 *out_ptr = NULL;
	JSValue res;
	GF_Err e;
	if (!argc) return GF_JS_EXCEPTION(ctx);

	if (is_dec) {
		u32 len;
		const char *str = JS_ToCString(ctx, argv[0]);
		if (!str) return GF_JS_EXCEPTION(ctx);
		len = (u32) strlen(str);
		out_ptr = gf_malloc(sizeof(u8) * len);
		if (!out_ptr) {
			e = GF_OUT_OF_MEM;
		} else if (is_16) {
			out_size = gf_base16_decode((u8*) str, (u32) len, out_ptr, len);
			e = out_size ? GF_OK : GF_NON_COMPLIANT_BITSTREAM;
		} else {
			out_size = gf_base64_decode((u8*) str, (u32) len, out_ptr, len);
			e = out_size ? GF_OK : GF_NON_COMPLIANT_BITSTREAM;
		}
		JS_FreeCString(ctx, str);
	} else {
		const u8 *data;
		size_t data_size;
		data = JS_GetArrayBuffer(ctx, &data_size, argv[0] );
		if (!data) return GF_JS_EXCEPTION(ctx);
		out_ptr = gf_malloc(sizeof(u8) * (1 + data_size * 2) );
		if (!out_ptr) {
			e = GF_OUT_OF_MEM;
		} else if (is_16) {
			out_size = gf_base16_encode((u8*) data, (u32) data_size, out_ptr, 1 + (u32) data_size * 2);
			e = out_size ? GF_OK : GF_NON_COMPLIANT_BITSTREAM;
		} else {
			out_size = gf_base64_encode((u8*) data, (u32) data_size, out_ptr, 1 + (u32) data_size * 2);
			e = out_size ? GF_OK : GF_NON_COMPLIANT_BITSTREAM;
		}
	}

	if (e) return js_throw_err(ctx, e);
	if (is_dec) {
		res = JS_NewArrayBuffer(ctx, out_ptr, out_size, js_gpac_free, NULL, 0);
	} else {
		out_ptr[out_size] = 0;
		res = JS_NewString(ctx, out_ptr);
		gf_free(out_ptr);
	}
	return res;
}
static JSValue js_sys_base64enc(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_sys_basecode_ex(ctx, this_val, argc, argv, GF_FALSE, GF_FALSE);
}
static JSValue js_sys_base64dec(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_sys_basecode_ex(ctx, this_val, argc, argv, GF_TRUE, GF_FALSE);
}
static JSValue js_sys_base16enc(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_sys_basecode_ex(ctx, this_val, argc, argv, GF_FALSE, GF_TRUE);
}
static JSValue js_sys_base16dec(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_sys_basecode_ex(ctx, this_val, argc, argv, GF_TRUE, GF_TRUE);
}
static JSValue js_sys_ntohl(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s64 val;
	if (!argc) return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt64(ctx, &val, argv[0])) return GF_JS_EXCEPTION(ctx);
	return JS_NewInt32(ctx, gf_ntohl((u32) val));
}
static JSValue js_sys_ntohs(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s64 val;
	if (!argc) return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt64(ctx, &val, argv[0])) return GF_JS_EXCEPTION(ctx);
	return JS_NewInt32(ctx, gf_ntohs((u16) val));
}
static JSValue js_sys_htonl(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s64 val;
	if (!argc) return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt64(ctx, &val, argv[0])) return GF_JS_EXCEPTION(ctx);
	return JS_NewInt32(ctx, gf_htonl((u32) val));
}
static JSValue js_sys_htons(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s64 val;
	if (!argc) return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt64(ctx, &val, argv[0])) return GF_JS_EXCEPTION(ctx);
	return JS_NewInt32(ctx, gf_htons((u16) val));
}

GF_Err jsf_ToProp_ex(GF_Filter *filter, JSContext *ctx, JSValue value, u32 p4cc, GF_PropertyValue *prop, u32 prop_type);

static JSValue js_pixfmt_size(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 w, h, osize;
	GF_PropertyValue prop;
	if (argc<3) return GF_JS_EXCEPTION(ctx);
	GF_Err e = jsf_ToProp_ex(NULL, ctx, argv[0], 0, &prop, GF_PROP_PIXFMT);
	if (e) return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt32(ctx, &w, argv[1])) return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt32(ctx, &h, argv[2])) return GF_JS_EXCEPTION(ctx);

	if (!gf_pixel_get_size_info(prop.value.uint, w, h, &osize, NULL, NULL, NULL, NULL))
		return js_throw_err_msg(ctx, GF_BAD_PARAM, "Unknown pixel format %d\n", prop.value.uint);
	return JS_NewInt32(ctx, osize);
}

static JSValue js_pixfmt_transparent(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_PropertyValue prop;
	if (!argc) return GF_JS_EXCEPTION(ctx);
	GF_Err e = jsf_ToProp_ex(NULL, ctx, argv[0], 0, &prop, GF_PROP_PIXFMT);
	if (e) return GF_JS_EXCEPTION(ctx);

	return JS_NewBool(ctx, 	gf_pixel_fmt_is_transparent(prop.value.uint) );
}

static JSValue js_pixfmt_yuv(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_PropertyValue prop;
	if (!argc) return GF_JS_EXCEPTION(ctx);
	GF_Err e = jsf_ToProp_ex(NULL, ctx, argv[0], 0, &prop, GF_PROP_PIXFMT);
	if (e) return GF_JS_EXCEPTION(ctx);

	return JS_NewBool(ctx, gf_pixel_fmt_is_yuv(prop.value.uint) );
}

static JSValue js_pcmfmt_depth(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_PropertyValue prop;
	if (argc!=1) return GF_JS_EXCEPTION(ctx);
	GF_Err e = jsf_ToProp_ex(NULL, ctx, argv[0], 0, &prop, GF_PROP_PCMFMT);
	if (e) return GF_JS_EXCEPTION(ctx);
	return JS_NewInt32(ctx, gf_audio_fmt_bit_depth(prop.value.uint)/8 );
}

#include <gpac/color.h>

static JSValue js_color_lerp(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *str;
	u32 col1, col2;
	char szCol[12];
	Double interp, minterp, a1, r1, g1, b1, a2, r2, g2, b2;
	u8 r, g, b, a;

	if (argc!=3)
		return GF_JS_EXCEPTION(ctx);
	if (JS_ToFloat64(ctx, &interp, argv[0]))
		return GF_JS_EXCEPTION(ctx);

	if (interp<0) interp=0;
	else if (interp>1) interp=1;
	minterp = 1-interp;

	str = JS_ToCString(ctx, argv[1]);
	if (!str) return GF_JS_EXCEPTION(ctx);
	col1 = gf_color_parse(str);
	JS_FreeCString(ctx, str);

	str = JS_ToCString(ctx, argv[2]);
	if (!str) return GF_JS_EXCEPTION(ctx);
	col2 = gf_color_parse(str);
	JS_FreeCString(ctx, str);

	a1 = GF_COL_A(col1) / 255.0;
	a2 = GF_COL_A(col2) / 255.0;
	r1 = GF_COL_R(col1) / 255.0;
	r2 = GF_COL_R(col2) / 255.0;
	g1 = GF_COL_G(col1) / 255.0;
	g2 = GF_COL_G(col2) / 255.0;
	b1 = GF_COL_B(col1) / 255.0;
	b2 = GF_COL_B(col2) / 255.0;

	a = (u8) ((a1 * minterp + a2 * interp) * 255);
	r = (u8) ((r1 * minterp + r2 * interp) * 255);
	g = (u8) ((g1 * minterp + g2 * interp) * 255);
	b = (u8) ((b1 * minterp + b2 * interp) * 255);

	if (!a && !r && !g && !b)
		return JS_NewString(ctx, "none");

	sprintf(szCol, "0x%02X%02X%02X%02X", a, r, g, b);
	return JS_NewString(ctx, szCol);
}

static JSValue js_color_get_component(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *str;
	u32 comp_idx, color;
	Double comp;
	if (argc!=2)
		return GF_JS_EXCEPTION(ctx);

	str = JS_ToCString(ctx, argv[0]);
	if (!str) return GF_JS_EXCEPTION(ctx);
	color = gf_color_parse(str);
	JS_FreeCString(ctx, str);

	if (JS_ToInt32(ctx, &comp_idx, argv[1]))
		return GF_JS_EXCEPTION(ctx);

	switch (comp_idx) {
	case 0: comp = GF_COL_A(color); break;
	case 1: comp = GF_COL_R(color); break;
	case 2: comp = GF_COL_G(color); break;
	case 3: comp = GF_COL_B(color); break;
	default: return GF_JS_EXCEPTION(ctx);
	}
	comp /= 255.0;
	return JS_NewFloat64(ctx, comp);
}

typedef struct
{
	u64 last_sample_time, frame_ts;
	u32 samples_used, consumed, nb_samples, channels;
	u32 fade;
	JSValue jspid;
	Double volume;
	u8 *data;
	size_t data_size;
} PidMix;

typedef struct
{
	u32 channels, samples_gap, fade_len;
	Double *chan_buf;
	u32 max_pid_channels;
	Double *in_chan_buf;
	u32 nb_inputs;
	PidMix *inputs;

	u32 sample_size;
	Double (*get_sample)(u8 *data);
	void (*set_sample)(u8 *data, Double val);
} AMixCtx;

static JSValue js_audio_mix(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 nb_samples, i, j, k, nb_src, fade_length=0;
	u64 audio_time;
	JSValue v;
	s32 res;
	size_t ab_size;
	u8 *mix_ab;
	u32 max_chan = 0;
	PidMix *pids;
	AMixCtx *mix = JS_GetOpaque(this_val, amix_class_id);
	if (!mix) return GF_JS_EXCEPTION(ctx);

	if (argc != 3) return GF_JS_EXCEPTION(ctx);

	if (JS_ToInt64(ctx, &audio_time, argv[0])) return GF_JS_EXCEPTION(ctx);
	mix_ab = JS_GetArrayBuffer(ctx, &ab_size, argv[1]);
	if (!mix_ab) return GF_JS_EXCEPTION(ctx);
	nb_samples = (u32) (ab_size / mix->channels / mix->sample_size);

	v = JS_GetPropertyStr(ctx, argv[2], "length");
	if (JS_IsException(v)) return GF_JS_EXCEPTION(ctx);

	if (JS_ToInt32(ctx, &nb_src, v)) {
		JS_FreeValue(ctx, v);
		return GF_JS_EXCEPTION(ctx);
	}
	JS_FreeValue(ctx, v);

	if (mix->nb_inputs < nb_src) {
		mix->inputs = gf_realloc(mix->inputs, sizeof(PidMix) * nb_src);
		if (!mix->inputs) return js_throw_err(ctx, GF_OUT_OF_MEM);
		mix->nb_inputs = nb_src;
	}
	pids = mix->inputs;

	//update current input states
	for (i=0; i<nb_src; i++) {
		PidMix *pid = &pids[i];
		pid->jspid = JS_GetPropertyUint32(ctx, argv[2], i);

		v = JS_GetPropertyStr(ctx, pid->jspid, "last_sample_time");
		res = JS_ToInt64(ctx, &pid->last_sample_time, v);
		JS_FreeValue(ctx, v);
		//assume first sample if not set
		if (res) pid->last_sample_time = 0;

		v = JS_GetPropertyStr(ctx, pid->jspid, "frame_ts");
		res = JS_ToInt64(ctx, &pid->frame_ts, v);
		JS_FreeValue(ctx, v);
		//we must have a frame timestamp in audio timescale
		if (res) return GF_JS_EXCEPTION(ctx);

		v = JS_GetPropertyStr(ctx, pid->jspid, "samples_used");
		res = JS_ToInt32(ctx, &pid->samples_used, v);
		JS_FreeValue(ctx, v);
		//if not set, assume first sample
		if (res) pid->samples_used = 0;

		v = JS_GetPropertyStr(ctx, pid->jspid, "channels");
		res = JS_ToInt32(ctx, &pid->channels, v);
		JS_FreeValue(ctx, v);
		//we must have the number of channels for this source
		if (res || !pid->channels) return GF_JS_EXCEPTION(ctx);

		//we must have the audio data
		v = JS_GetPropertyStr(ctx, pid->jspid, "data");
		pid->data = JS_GetArrayBuffer(ctx, &pid->data_size, v);
		JS_FreeValue(ctx, v);

		v = JS_GetPropertyStr(ctx, pid->jspid, "nb_samples");
		res = JS_ToInt32(ctx, &pid->nb_samples, v);
		JS_FreeValue(ctx, v);
		//if not set, derive from data size
		if (res) {
			pid->nb_samples = (u32) (pid->data_size / pid->channels / mix->sample_size);
		}
		v = JS_GetPropertyStr(ctx, pid->jspid, "fade");
		res = JS_ToInt32(ctx, &pid->fade, v);
		JS_FreeValue(ctx, v);
		if (res) pid->fade = 0;
		if (pid->fade) fade_length++;

		v = JS_GetPropertyStr(ctx, pid->jspid, "volume");
		res = JS_ToFloat64(ctx, &pid->volume, v);
		JS_FreeValue(ctx, v);
		if (res) pid->volume = 1.0;

		if (pid->channels>max_chan)
			max_chan = pid->channels;

		pid->consumed = 0;
	}
	if (max_chan > mix->max_pid_channels) {
		mix->in_chan_buf = gf_realloc(mix->in_chan_buf, sizeof(Double) * max_chan);
		if (!mix->in_chan_buf) return js_throw_err(ctx, GF_OUT_OF_MEM);
		mix->max_pid_channels = max_chan;
	}
	if (fade_length) {
		fade_length = MIN(mix->fade_len, nb_samples);
	}

	for (i=0; i<nb_samples; i++) {
		u32 active=0;
		memset(mix->chan_buf, 0, sizeof(Double) * mix->channels);

		for (j=0; j<nb_src; j++) {
			PidMix *pid = &pids[j];

			//for the time being, as soon as we start writing audio we no longer check sync
			if (!pid->last_sample_time) {
				if (pid->frame_ts + pid->samples_used  > audio_time + i) {
					continue;
				}
				pid->last_sample_time = 1; //pid.frame_ts + pid.sample_used;
				JS_SetPropertyStr(ctx, pid->jspid, "last_sample_time", JS_NewInt64(ctx, pid->last_sample_time) );
			} else {
				//audio discontinuity
				if (pid->frame_ts + pid->samples_used  > audio_time + i + mix->samples_gap) {
					continue;
				}
			}

			u32 s_pos = pid->samples_used + pid->consumed;
			if (s_pos >= pid->nb_samples) {
				assert(0);
				GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[AVMix] error mixing\n"));
			}
			u32 pos = s_pos * pid->channels;
			u8 *buf = pid->data + mix->sample_size * pos;
			for (k=0; k<pid->channels; k++) {
				mix->in_chan_buf[k] = mix->get_sample(buf);
				buf += mix->sample_size;
			}

			active += 1;

			Double fade_scale = 1;
			if (pid->fade) {
				//fade from 0 to 1 starting at pos=0 up to fade_length
				if (pid->fade==1) {
					if (s_pos<fade_length) {
						fade_scale = s_pos/fade_length;
					} else {
						pid->fade = 0;
						JS_SetPropertyStr(ctx, pid->jspid, "fade", JS_NewInt32(ctx, pid->fade) );
					}
				}
				//fade from 1 to 0 starting at pos=nb_samples-fade_length up to nb_samples
				else if (pid->fade==2) {
					if (s_pos>nb_samples-fade_length ) {
						fade_scale = (nb_samples - s_pos)/fade_length;
					}
				}
			}
			fade_scale *= pid->volume;

			//todo , proper down/up mix ...
			for (k=0; k<mix->channels; k++) {
				Double s_val;
				if (pid->channels < k )
					s_val = mix->in_chan_buf[0];
				else
				s_val = mix->in_chan_buf[k];

				mix->chan_buf[k] += s_val * fade_scale;
			}
			pid->consumed ++;
		}

		u8 *dst = mix_ab + mix->sample_size * (i * mix->channels);
		for (j=0; j< mix->channels; j++) {
			Double sval = active ? (mix->chan_buf[j] / active) : 0;
			mix->set_sample(dst, sval);
			dst += mix->sample_size;
		}
	}

	//update pid values
	for (i=0; i<nb_src; i++) {
		PidMix *pid = &pids[i];
		pid->samples_used += pid->consumed;
		JS_SetPropertyStr(ctx, pid->jspid, "samples_used", JS_NewInt32(ctx, pid->samples_used));

		JS_FreeValue(ctx, pid->jspid);
	}
	return JS_UNDEFINED;
}

static Double amix_get_s16(u8 *data)
{
	u16 val = data[1];
	val <<= 8;
	val |= data[0];
	return ((Double) (s16) val) / 65535;

}
static void amix_set_s16(u8 *data, Double val)
{
	val *= 65535;
	u16 res = (u16) val;
	data[0] = res & 0xFF;
	data[1] = (res>>8) & 0xFF;
}
static Double amix_get_s32(u8 *data)
{
	u32 val = data[3];
	val <<= 8;
	val |= data[2];
	val <<= 8;
	val |= data[1];
	val <<= 8;
	val |= data[0];
	return ((Double) (s32) val) / 0xFFFFFFFF;

}
static void amix_set_s32(u8 *data, Double val)
{
	val *= 0xFFFFFFFF;
	u32 res = (u32) val;
	data[0] = res & 0xFF;
	data[1] = (res>>8) & 0xFF;
	data[2] = (res>>16) & 0xFF;
	data[3] = (res>>24) & 0xFF;
}
static Double amix_get_flt(u8 *data)
{
	return (Double) *(Float *)data;
}
static void amix_set_flt(u8 *data, Double val)
{
	*(Float *)data = (Float) val;
}
static Double amix_get_dbl(u8 *data)
{
	return *(Double *)data;
}
static void amix_set_dbl(u8 *data, Double val)
{
	*(Double *)data = val;
}


static void js_amix_finalize(JSRuntime *rt, JSValue obj)
{
	AMixCtx *mix = JS_GetOpaque(obj, amix_class_id);
	if (!mix) return;

	if (mix->chan_buf) gf_free(mix->chan_buf);
	if (mix->inputs) gf_free(mix->inputs);
	if (mix->in_chan_buf) gf_free(mix->in_chan_buf);
	gf_free(mix);
}

JSClassDef amixClass = {
    "FILE",
    .finalizer = js_amix_finalize,
};

static const JSCFunctionListEntry amix_funcs[] = {
	JS_CFUNC_DEF("mix", 0, js_audio_mix),
};

static JSValue amix_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	u32 a_fmt=0;
	u32 channels, samples_gap, fade_len;
	AMixCtx *mix;
	JSValue anobj;

	if (argc != 4) return js_throw_err(ctx, GF_BAD_PARAM);

	const char *fmt = JS_ToCString(ctx, argv[0]);
	if (!fmt) return GF_JS_EXCEPTION(ctx);
	//we only support these formats for the time being, all in packed mode
	if (!strcmp(fmt, "s16")) a_fmt = 1;
	else if (!strcmp(fmt, "s32")) a_fmt = 2;
	else if (!strcmp(fmt, "flt")) a_fmt = 3;
	else if (!strcmp(fmt, "dbl")) a_fmt = 4;
	JS_FreeCString(ctx, fmt);
	if (!a_fmt) return GF_JS_EXCEPTION(ctx);

	if (JS_ToInt32(ctx, &channels, argv[1])) return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt32(ctx, &samples_gap, argv[2])) return GF_JS_EXCEPTION(ctx);
	if (JS_ToInt32(ctx, &fade_len, argv[3])) return GF_JS_EXCEPTION(ctx);
	if (!channels) return js_throw_err(ctx, GF_BAD_PARAM);

	GF_SAFEALLOC(mix, AMixCtx);
	mix->channels = channels;
	mix->samples_gap = samples_gap;
	mix->fade_len = fade_len;
	mix->chan_buf = gf_malloc(sizeof(Double) * channels);
	if (!mix->chan_buf) {
		gf_free(mix);
		return js_throw_err(ctx, GF_OUT_OF_MEM);
	}

	switch (a_fmt) {
	case 1:
		mix->sample_size = 2;
		mix->get_sample = amix_get_s16;
		mix->set_sample = amix_set_s16;
		break;
	case 2:
		mix->sample_size = 4;
		mix->get_sample = amix_get_s32;
		mix->set_sample = amix_set_s32;
		break;
	case 3:
		mix->sample_size = 4;
		mix->get_sample = amix_get_flt;
		mix->set_sample = amix_set_flt;
		break;
	case 4:
		mix->sample_size = 8;
		mix->get_sample = amix_get_dbl;
		mix->set_sample = amix_set_dbl;
		break;
	}

	anobj = JS_NewObjectClass(ctx, amix_class_id);
	if (JS_IsException(anobj)) {
		gf_free(mix);
		return anobj;
	}
	JS_SetOpaque(anobj, mix);
	return anobj;
}

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
	JS_CGETSET_MAGIC_DEF_ENUM("username", js_sys_prop_get, NULL, JS_SYS_USERNAME),

	JS_CGETSET_MAGIC_DEF_ENUM("test_mode", js_sys_prop_get, NULL, JS_SYS_TEST_MODE),
	JS_CGETSET_MAGIC_DEF_ENUM("old_arch", js_sys_prop_get, NULL, JS_SYS_OLD_ARCH),
	JS_CGETSET_MAGIC_DEF_ENUM("log_color", js_sys_prop_get, NULL, JS_SYS_LOG_COLOR),

	JS_CGETSET_MAGIC_DEF_ENUM("quiet", js_sys_prop_get, NULL, JS_SYS_QUIET),
	JS_CGETSET_MAGIC_DEF_ENUM("timezone", js_sys_prop_get, NULL, JS_SYS_TIMEZONE),
	JS_CGETSET_MAGIC_DEF_ENUM("nb_files_open", js_sys_prop_get, NULL, JS_SYS_FILES_OPEN),
	JS_CGETSET_MAGIC_DEF_ENUM("cache_dir", js_sys_prop_get, NULL, JS_SYS_CACHE_DIR),
	JS_CGETSET_MAGIC_DEF_ENUM("shared_dir", js_sys_prop_get, NULL, JS_SYS_SHARED_DIR),
	JS_CGETSET_MAGIC_DEF_ENUM("version", js_sys_prop_get, NULL, JS_SYS_VERSION),
	JS_CGETSET_MAGIC_DEF_ENUM("version_full", js_sys_prop_get, NULL, JS_SYS_VERSION_FULL),
	JS_CGETSET_MAGIC_DEF_ENUM("copyright", js_sys_prop_get, NULL, JS_SYS_COPYRIGHT),
	JS_CGETSET_MAGIC_DEF_ENUM("version_major", js_sys_prop_get, NULL, JS_SYS_V_MAJOR),
	JS_CGETSET_MAGIC_DEF_ENUM("version_minor", js_sys_prop_get, NULL, JS_SYS_V_MINOR),
	JS_CGETSET_MAGIC_DEF_ENUM("version_micro", js_sys_prop_get, NULL, JS_SYS_V_MICRO),

	JS_CFUNC_DEF("set_arg_used", 0, js_sys_set_arg_used),
	JS_CFUNC_DEF("error_string", 0, js_sys_error_string),
    JS_CFUNC_DEF("prompt_input", 0, js_sys_prompt_input),
    JS_CFUNC_DEF("prompt_string", 0, js_sys_prompt_string),
    JS_CFUNC_DEF("prompt_echo_off", 0, js_sys_prompt_echo_off),
    JS_CFUNC_DEF("prompt_code", 0, js_sys_prompt_code),
    JS_CFUNC_DEF("prompt_size", 0, js_sys_prompt_size),

    JS_CFUNC_DEF("keyname", 0, js_sys_keyname),
    JS_CFUNC_DEF("get_event_type", 0, js_sys_evt_by_name),
    JS_CFUNC_DEF("gc", 0, js_sys_gc),

	JS_CFUNC_DEF("enum_directory", 0, js_sys_enum_directory),
	JS_CFUNC_DEF("clock_ms", 0, js_sys_clock),
	JS_CFUNC_DEF("clock_us", 0, js_sys_clock_high_res),
	JS_CFUNC_DEF("sleep", 0, js_sys_sleep),
	JS_CFUNC_DEF("exit", 0, js_sys_exit),
	JS_CFUNC_DEF("fcc_to_str", 0, js_sys_4cc_to_str),
	JS_CFUNC_DEF("rand_init", 0, js_sys_rand_init),
	JS_CFUNC_DEF("rand", 0, js_sys_rand),
	JS_CFUNC_DEF("rand64", 0, js_sys_rand64),
	JS_CFUNC_DEF("getenv", 0, js_sys_getenv),
	JS_CFUNC_DEF("get_utc", 0, js_sys_get_utc),
	JS_CFUNC_DEF("get_ntp", 0, js_sys_get_ntp),
	JS_CFUNC_DEF("ntp_shift", 0, js_sys_ntp_shift),

	JS_CFUNC_DEF("crc32", 0, js_sys_crc32),
	JS_CFUNC_DEF("sha1", 0, js_sys_sha1),
	JS_CFUNC_DEF("load_file", 0, js_sys_file_data),
	JS_CFUNC_DEF("load_script", 0, js_sys_load_script),
	JS_CFUNC_DEF("compress", 0, js_sys_compress),
	JS_CFUNC_DEF("decompress", 0, js_sys_decompress),
	JS_CFUNC_DEF("rmdir", 0, js_sys_rmdir),
	JS_CFUNC_DEF("mkdir", 0, js_sys_mkdir),
	JS_CFUNC_DEF("dir_exists", 0, js_sys_dir_exists),
	JS_CFUNC_DEF("dir_clean", 0, js_sys_dir_clean),
	JS_CFUNC_DEF("basename", 0, js_sys_basename),
	JS_CFUNC_DEF("file_ext", 0, js_sys_file_ext),
	JS_CFUNC_DEF("file_exists", 0, js_sys_file_exists),
	JS_CFUNC_DEF("del", 0, js_sys_file_del),
	JS_CFUNC_DEF("mod_time", 0, js_sys_file_modtime),
	JS_CFUNC_DEF("move", 0, js_sys_file_move),
	JS_CFUNC_DEF("get_opt", 0, js_sys_get_opt),
	JS_CFUNC_DEF("set_opt", 0, js_sys_set_opt),
	JS_CFUNC_DEF("discard_opts", 0, js_sys_discard_opts),

	JS_CFUNC_DEF("base64enc", 0, js_sys_base64enc),
	JS_CFUNC_DEF("base64dec", 0, js_sys_base64dec),
	JS_CFUNC_DEF("base16enc", 0, js_sys_base16enc),
	JS_CFUNC_DEF("base16dec", 0, js_sys_base16dec),

	JS_CFUNC_DEF("ntohl", 0, js_sys_ntohl),
	JS_CFUNC_DEF("ntohs", 0, js_sys_ntohs),
	JS_CFUNC_DEF("htonl", 0, js_sys_htonl),
	JS_CFUNC_DEF("htons", 0, js_sys_htons),

	JS_CFUNC_DEF("pixfmt_size", 0, js_pixfmt_size),
	JS_CFUNC_DEF("pixfmt_transparent", 0, js_pixfmt_transparent),
	JS_CFUNC_DEF("pixfmt_yuv", 0, js_pixfmt_yuv),
	JS_CFUNC_DEF("pcmfmt_depth", 0, js_pcmfmt_depth),
	JS_CFUNC_DEF("color_lerp", 0, js_color_lerp),
	JS_CFUNC_DEF("color_component", 0, js_color_get_component),
	JS_CFUNC_DEF("url_cat", 0, js_sys_url_cat),
	JS_CFUNC_DEF("rect_union", 0, js_sys_rect_union),
	JS_CFUNC_DEF("rect_intersect", 0, js_sys_rect_intersect),

	JS_CFUNC_DEF("_avmix_audio", 0, js_audio_mix),
};


static void js_sha1_finalize(JSRuntime *rt, JSValue obj)
{
	GF_SHA1Context *sha1 = JS_GetOpaque(obj, sha1_class_id);
	if (!sha1) return;
	gf_free(sha1);
}

JSClassDef sha1Class = {
    "SHA1",
    .finalizer = js_sha1_finalize,
};

static JSValue js_sha1_push(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const u8 *data;
	size_t data_size;
	GF_SHA1Context *sha1 = JS_GetOpaque(this_val, sha1_class_id);
	if (!sha1) {
		sha1 = gf_sha1_starts();
		if (!sha1) {
			return js_throw_err(ctx, GF_OUT_OF_MEM);
		}
		JS_SetOpaque(this_val, sha1);
	}
	if (!argc) return GF_JS_EXCEPTION(ctx);
	data = JS_GetArrayBuffer(ctx, &data_size, argv[0]);
	gf_sha1_update(sha1, (u8 *) data, (u32) data_size);

	return JS_UNDEFINED;
}
static JSValue js_sha1_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u8 output[GF_SHA1_DIGEST_SIZE];
	GF_SHA1Context *sha1 = JS_GetOpaque(this_val, sha1_class_id);
	if (!sha1) return GF_JS_EXCEPTION(ctx);
	gf_sha1_finish(sha1, output);
	JS_SetOpaque(this_val, NULL);
	return JS_NewArrayBufferCopy(ctx, output, GF_SHA1_DIGEST_SIZE);
}

static const JSCFunctionListEntry sha1_funcs[] = {
	JS_CFUNC_DEF("push", 0, js_sha1_push),
	JS_CFUNC_DEF("get", 0, js_sha1_get),
};

static JSValue sha1_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	JSValue anobj;
	GF_SHA1Context *sha1;

	sha1 = gf_sha1_starts();
	if (!sha1) {
		return js_throw_err(ctx, GF_OUT_OF_MEM);
	}
	anobj = JS_NewObjectClass(ctx, sha1_class_id);
	if (JS_IsException(anobj)) {
		gf_free(sha1);
		return anobj;
	}
	JS_SetOpaque(anobj, sha1);
	return anobj;
}


static void js_file_finalize(JSRuntime *rt, JSValue obj)
{
	FILE *f = JS_GetOpaque(obj, file_class_id);
	if (!f) return;
	gf_fclose(f);
}

JSClassDef fileClass = {
    "FILE",
    .finalizer = js_file_finalize,
};

enum
{
	JS_FILE_POS = 0,
	JS_FILE_EOF,
	JS_FILE_ERROR,
	JS_FILE_SIZE,
	JS_FILE_IS_GFIO,
};

static JSValue js_file_prop_get(JSContext *ctx, JSValueConst this_val, int magic)
{
	FILE *f = JS_GetOpaque(this_val, file_class_id);
	if (!f) return GF_JS_EXCEPTION(ctx);

	switch (magic) {
	case JS_FILE_POS:
		return JS_NewInt64(ctx, gf_ftell(f));
	case JS_FILE_EOF:
		return JS_NewBool(ctx, gf_feof(f));
	case JS_FILE_ERROR:
		return JS_NewInt32(ctx, gf_ferror(f) );
	case JS_FILE_SIZE:
		return JS_NewInt64(ctx, gf_fsize(f) );
	case JS_FILE_IS_GFIO:
		return JS_NewBool(ctx, gf_fileio_check(f));
	}
	return JS_UNDEFINED;
}

static JSValue js_file_prop_set(JSContext *ctx, JSValueConst this_val, JSValueConst value, int magic)
{
	s64 lival;
	FILE *f = JS_GetOpaque(this_val, file_class_id);
	if (!f) return GF_JS_EXCEPTION(ctx);

	switch (magic) {
	case JS_FILE_POS:
		if (JS_ToInt64(ctx, &lival, value)) return GF_JS_EXCEPTION(ctx);
		gf_fseek(f, lival, SEEK_SET);
		break;
	}
	return JS_UNDEFINED;
}
static JSValue js_file_flush(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	FILE *f = JS_GetOpaque(this_val, file_class_id);
	if (!f) return GF_JS_EXCEPTION(ctx);
	gf_fflush(f);
	return JS_UNDEFINED;
}
static JSValue js_file_close(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	FILE *f = JS_GetOpaque(this_val, file_class_id);
	if (!f) return GF_JS_EXCEPTION(ctx);
	gf_fclose(f);
	JS_SetOpaque(this_val, NULL);
	return JS_UNDEFINED;
}
static JSValue js_file_read(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const u8 *data;
	size_t size;
	u32 read;
	s32 nb_bytes=0;
	FILE *f = JS_GetOpaque(this_val, file_class_id);
	if (!f) return GF_JS_EXCEPTION(ctx);
	if (!argc) return GF_JS_EXCEPTION(ctx);

	data = JS_GetArrayBuffer(ctx, &size, argv[0]);
	if (!data) return GF_JS_EXCEPTION(ctx);
	if (argc>1) {
		if (JS_ToInt32(ctx, &nb_bytes, argv[1])) return GF_JS_EXCEPTION(ctx);
	}
	if (!nb_bytes) nb_bytes = (s32) size;
	else if (nb_bytes > (s32) size) nb_bytes = (s32) size;

	read = (u32) gf_fread((void *) data, nb_bytes, f);
	return JS_NewInt64(ctx, read);
}
static JSValue js_file_gets(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	char *data=NULL;
	char temp[1025];
	JSValue res;
	FILE *f = JS_GetOpaque(this_val, file_class_id);
	if (!f) return GF_JS_EXCEPTION(ctx);

	temp[1024] = 0;
	while (1) {
		u32 rlen;
		temp[0] = 0;
		const char *read = gf_fgets(temp, 1024, f);
		if (!read) break;
		gf_dynstrcat(&data, temp, NULL);
		if (!data) return js_throw_err(ctx, GF_OUT_OF_MEM);
		rlen = (u32) strlen(temp);
		if (rlen <= 1024)
			break;
		if (temp[rlen-1] == '\n')
			break;
	}
	res = JS_NewString(ctx, data);
	gf_free(data);
	return res;
}

static JSValue js_file_getc(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	char res[2];
	FILE *f = JS_GetOpaque(this_val, file_class_id);
	if (!f) return GF_JS_EXCEPTION(ctx);

	res[0] = gf_fgetc(f);
	res[1] = 0;
	return JS_NewString(ctx, res);
}

static JSValue js_file_write(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const u8 *data;
	size_t size;
	u32 read;
	s32 nb_bytes=0;
	FILE *f = JS_GetOpaque(this_val, file_class_id);
	if (!f) return GF_JS_EXCEPTION(ctx);
	if (!argc) return GF_JS_EXCEPTION(ctx);

	data = JS_GetArrayBuffer(ctx, &size, argv[0]);
	if (!data) return GF_JS_EXCEPTION(ctx);
	if (argc>1) {
		if (JS_ToInt32(ctx, &nb_bytes, argv[1])) return GF_JS_EXCEPTION(ctx);
	}
	if (!nb_bytes) nb_bytes = (u32) size;
	else if (nb_bytes > (s32) size) nb_bytes = (u32) size;

	read = (u32) gf_fwrite((void *) data, nb_bytes, f);
	return JS_NewInt64(ctx, read);
}
static JSValue js_file_puts(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *string;
	FILE *f = JS_GetOpaque(this_val, file_class_id);
	if (!f || !argc) return GF_JS_EXCEPTION(ctx);

	string = JS_ToCString(ctx, argv[0]);
	if (!string) return GF_JS_EXCEPTION(ctx);
	gf_fputs(string, f);
	JS_FreeCString(ctx, string);
	return JS_UNDEFINED;
}

static JSValue js_file_putc(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	int val=0;
	FILE *f = JS_GetOpaque(this_val, file_class_id);
	if (!f || !argc) return GF_JS_EXCEPTION(ctx);

	if (JS_IsString(argv[0])) {
		const char *string = JS_ToCString(ctx, argv[0]);
		if (!string) return GF_JS_EXCEPTION(ctx);
		val = string[0];
		JS_FreeCString(ctx, string);
	} else {
		if (JS_ToInt32(ctx, &val, argv[0])) return GF_JS_EXCEPTION(ctx);
	}
	gf_fputc(val, f);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry file_funcs[] = {
    JS_CGETSET_MAGIC_DEF_ENUM("pos", js_file_prop_get, js_file_prop_set, JS_FILE_POS),
    JS_CGETSET_MAGIC_DEF_ENUM("eof", js_file_prop_get, NULL, JS_FILE_EOF),
    JS_CGETSET_MAGIC_DEF_ENUM("error", js_file_prop_get, NULL, JS_FILE_ERROR),
    JS_CGETSET_MAGIC_DEF_ENUM("size", js_file_prop_get, NULL, JS_FILE_SIZE),
    JS_CGETSET_MAGIC_DEF_ENUM("gfio", js_file_prop_get, NULL, JS_FILE_IS_GFIO),

	JS_CFUNC_DEF("flush", 0, js_file_flush),
	JS_CFUNC_DEF("close", 0, js_file_close),
	JS_CFUNC_DEF("read", 0, js_file_read),
	JS_CFUNC_DEF("gets", 0, js_file_gets),
	JS_CFUNC_DEF("getc", 0, js_file_getc),
	JS_CFUNC_DEF("write", 0, js_file_write),
	JS_CFUNC_DEF("puts", 0, js_file_puts),
	JS_CFUNC_DEF("putc", 0, js_file_putc),
};

static JSValue file_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	JSValue anobj;
	FILE *f=NULL;

	if (!argc) {
		f = gf_file_temp(NULL);
		if (!f) {
			return js_throw_err(ctx, GF_OUT_OF_MEM);
		}
	} else {
		GF_Err e = GF_OK;
		const char *name=NULL, *mode=NULL, *parent_io=NULL;
		name = JS_ToCString(ctx, argv[0] );
		if (argc>1) {
			mode = JS_ToCString(ctx, argv[1] );
			if (argc>2) {
				parent_io = JS_ToCString(ctx, argv[2] );
			}
		}

		if (!name || !mode) {
			e = GF_BAD_PARAM;
		} else {
			f = gf_fopen_ex(name, parent_io, mode);
			if (!f) e = GF_URL_ERROR;
		}
		if (name) JS_FreeCString(ctx, name);
		if (mode) JS_FreeCString(ctx, mode);
		if (parent_io) JS_FreeCString(ctx, parent_io);

		if (e) {
			return js_throw_err(ctx, e);
		}
	}

	anobj = JS_NewObjectClass(ctx, file_class_id);
	if (JS_IsException(anobj)) {
		gf_fclose(f);
		return anobj;
	}
	JS_SetOpaque(anobj, f);
	return anobj;
}


static int js_gpaccore_init(JSContext *ctx, JSModuleDef *m)
{
	JSValue proto, ctor;
	if (!bitstream_class_id) {
		JS_NewClassID(&bitstream_class_id);
		JS_NewClass(JS_GetRuntime(ctx), bitstream_class_id, &bitstreamClass);

		JS_NewClassID(&sha1_class_id);
		JS_NewClass(JS_GetRuntime(ctx), sha1_class_id, &sha1Class);

		JS_NewClassID(&file_class_id);
		JS_NewClass(JS_GetRuntime(ctx), file_class_id, &fileClass);

		JS_NewClassID(&amix_class_id);
		JS_NewClass(JS_GetRuntime(ctx), amix_class_id, &amixClass);
	}

	JSValue core_o = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, core_o, sys_funcs, countof(sys_funcs));
    JS_SetModuleExport(ctx, m, "Sys", core_o);

	JSValue args = JS_NewArray(ctx);
    u32 i, nb_args = gf_sys_get_argc();
    for (i=0; i<nb_args; i++) {
        JS_SetPropertyUint32(ctx, args, i, JS_NewString(ctx, gf_sys_get_arg(i)));
    }
    JS_SetPropertyStr(ctx, core_o, "args", args);

#define DEF_CONST( _val ) \
    JS_SetPropertyStr(ctx, core_o, #_val, JS_NewInt32(ctx, _val));

	DEF_CONST(GF_CONSOLE_RESET)
	DEF_CONST(GF_CONSOLE_RED)
	DEF_CONST(GF_CONSOLE_GREEN)
	DEF_CONST(GF_CONSOLE_BLUE)
	DEF_CONST(GF_CONSOLE_YELLOW)
	DEF_CONST(GF_CONSOLE_CYAN)
	DEF_CONST(GF_CONSOLE_WHITE)
	DEF_CONST(GF_CONSOLE_MAGENTA)
	DEF_CONST(GF_CONSOLE_CLEAR)
	DEF_CONST(GF_CONSOLE_SAVE)
	DEF_CONST(GF_CONSOLE_RESTORE)
	DEF_CONST(GF_CONSOLE_BOLD)
	DEF_CONST(GF_CONSOLE_ITALIC)
	DEF_CONST(GF_CONSOLE_UNDERLINED)
	DEF_CONST(GF_CONSOLE_STRIKE)

#undef DEF_CONST


	//bitstream constructor
	proto = JS_NewObjectClass(ctx, bitstream_class_id);
	JS_SetPropertyFunctionList(ctx, proto, bitstream_funcs, countof(bitstream_funcs));
	JS_SetClassProto(ctx, bitstream_class_id, proto);
	ctor = JS_NewCFunction2(ctx, bitstream_constructor, "Bitstream", 1, JS_CFUNC_constructor, 0);
    JS_SetModuleExport(ctx, m, "Bitstream", ctor);

	//sha1 constructor
	proto = JS_NewObjectClass(ctx, sha1_class_id);
	JS_SetPropertyFunctionList(ctx, proto, sha1_funcs, countof(sha1_funcs));
	JS_SetClassProto(ctx, sha1_class_id, proto);
	ctor = JS_NewCFunction2(ctx, sha1_constructor, "SHA1", 1, JS_CFUNC_constructor, 0);
    JS_SetModuleExport(ctx, m, "SHA1", ctor);

	//FILE constructor
	proto = JS_NewObjectClass(ctx, file_class_id);
	JS_SetPropertyFunctionList(ctx, proto, file_funcs, countof(file_funcs));
	JS_SetClassProto(ctx, file_class_id, proto);
	ctor = JS_NewCFunction2(ctx, file_constructor, "File", 1, JS_CFUNC_constructor, 0);
    JS_SetModuleExport(ctx, m, "File", ctor);

	//amix constructor
	proto = JS_NewObjectClass(ctx, amix_class_id);
	JS_SetPropertyFunctionList(ctx, proto, amix_funcs, countof(amix_funcs));
	JS_SetClassProto(ctx, amix_class_id, proto);
	ctor = JS_NewCFunction2(ctx, amix_constructor, "AudioMixer", 1, JS_CFUNC_constructor, 0);
    JS_SetModuleExport(ctx, m, "AudioMixer", ctor);

	return 0;
}


void qjs_module_init_gpaccore(JSContext *ctx)
{
	JSModuleDef *m;
	m = JS_NewCModule(ctx, "gpaccore", js_gpaccore_init);
	if (!m) return;

	JS_AddModuleExport(ctx, m, "Sys");
	JS_AddModuleExport(ctx, m, "Bitstream");
	JS_AddModuleExport(ctx, m, "SHA1");
	JS_AddModuleExport(ctx, m, "File");
	JS_AddModuleExport(ctx, m, "AudioMixer");
	return;
}

void qjs_module_init_scenejs(JSContext *ctx);
void qjs_module_init_storage(JSContext *ctx);
void qjs_module_init_xhr(JSContext *c);
void qjs_module_init_evg(JSContext *c);
void qjs_module_init_webgl(JSContext *c);
void qjs_module_init_gpaccore(JSContext *c);

#ifndef GPAC_DISABLE_QJS_LIBC
#include "../quickjs/quickjs-libc.h"
void qjs_module_init_qjs_libc(JSContext *ctx)
{
#ifdef CONFIG_BIGNUM
    if (bignum_ext) {
        JS_AddIntrinsicBigFloat(ctx);
        JS_AddIntrinsicBigDecimal(ctx);
        JS_AddIntrinsicOperators(ctx);
        JS_EnableBignumExt(ctx, TRUE);
    }
#endif
    /* system modules */
    js_init_module_std(ctx, "std");
    js_init_module_os(ctx, "os");
}
#endif // GPAC_DISABLE_QJS_LIBC


void qjs_init_all_modules(JSContext *ctx, Bool no_webgl, Bool for_vrml)
{
	//init modules
	qjs_module_init_gpaccore(ctx);

#ifndef GPAC_DISABLE_QJS_LIBC
	qjs_module_init_qjs_libc(ctx);
#endif

	//vrml, init scene JS but do not init xhr (defined in DOM JS)
	if (for_vrml) {
#if !defined(GPAC_DISABLE_PLAYER)
		qjs_module_init_scenejs(ctx);
#endif
	} else {
		qjs_module_init_xhr(ctx);
	}
	qjs_module_init_evg(ctx);
	qjs_module_init_storage(ctx);

	if (!no_webgl && !for_vrml)
		qjs_module_init_webgl(ctx);
}


int js_module_set_import_meta(JSContext *ctx, JSValueConst func_val, JS_BOOL use_realpath, JS_BOOL is_main);


#ifndef GPAC_STATIC_BUILD

#if defined(WIN32) || defined(_WIN32_WCE)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

typedef JSModuleDef *(JSInitModuleFunc)(JSContext *ctx, const char *module_name);

static JSModuleDef *qjs_module_loader_dyn_lib(JSContext *ctx,
                                        const char *module_name)
{
	JSModuleDef *m=NULL;
	void *hd;
	JSInitModuleFunc *init;
	char *filename;

	if (!strchr(module_name, '/') || !strchr(module_name, '\\')) {
		/* must add a '/' so that the DLL is not searched in the system library paths */
		filename = gf_malloc(strlen(module_name) + 2 + 1);
		if (!filename) return NULL;
		strcpy(filename, "./");
		strcpy(filename + 2, module_name);
	} else {
		filename = (char *)module_name;
	}

	/* load dynamic lib */
#ifdef WIN32
	hd = LoadLibrary(filename);
#else
	hd = dlopen(filename, RTLD_NOW | RTLD_LOCAL);
#endif

	if (filename != module_name)
		gf_free(filename);

	if (!hd) {
		JS_ThrowReferenceError(ctx, "could not load module filename '%s' as shared library", module_name);
		return NULL;
	}

#ifdef WIN32
	init = (JSInitModuleFunc *) GetProcAddress(hd, "js_init_module");
#else
	init = (JSInitModuleFunc *) dlsym(hd, "js_init_module");
#endif

	if (!init) {
		JS_ThrowReferenceError(ctx, "could not load module filename '%s': js_init_module not found", module_name);
	} else {
		m = init(ctx, module_name);
		if (!m) {
			JS_ThrowReferenceError(ctx, "could not load module filename '%s': initialization error", module_name);
		}
	}
#ifdef WIN32
	FreeLibrary(hd);
#else
	dlclose(hd);
#endif

	return m;
}

#endif // GPAC_STATIC_BUILD

JSModuleDef *qjs_module_loader(JSContext *ctx, const char *module_name, void *opaque)
{
	JSModuleDef *m;
	const char *fext = gf_file_ext_start(module_name);

	if (fext && (!strcmp(fext, ".so") || !strcmp(fext, ".dll") || !strcmp(fext, ".dylib")) )  {
#ifndef GPAC_STATIC_BUILD
		m = qjs_module_loader_dyn_lib(ctx, module_name);
#else
		JS_ThrowReferenceError(ctx, "could not load module filename '%s', dynamic library loading disabled in build", module_name);
		m = NULL;
#endif
	} else {
		u32 buf_len;
		u8 *buf;
		JSValue func_val;
		char *url;
		const char *par_url = jsf_get_script_filename(ctx);
		url = gf_url_concatenate(par_url, module_name);

		GF_Err e = gf_file_load_data(url ? url : module_name, &buf, &buf_len);
		if (url) gf_free(url);

		if (e != GF_OK) {
			JS_ThrowReferenceError(ctx, "could not load module filename '%s': %s", module_name, gf_error_to_string(e) );
			return NULL;
		}
		/* compile the module */
		func_val = JS_Eval(ctx, buf ? (char *) buf : "", buf_len, module_name, JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
		gf_free(buf);
		if (JS_IsException(func_val))
			return NULL;
		/* XXX: could propagate the exception */
		js_module_set_import_meta(ctx, func_val, GF_TRUE, GF_FALSE);
		/* the module is already referenced, so we must free it */
		m = JS_VALUE_GET_PTR(func_val);
		JS_FreeValue(ctx, func_val);
	}
	return m;
}

#ifndef GPAC_DISABLE_QJS_LIBC

static JSContext *JS_NewWorkerContext(JSRuntime *rt)
{
	JSContext *ctx = JS_NewContext(rt);
	if (!ctx)
        return NULL;

	JSValue global_obj = JS_GetGlobalObject(ctx);
	js_load_constants(ctx, global_obj);
	JS_FreeValue(ctx, global_obj);

	//disable WebGL and scene.js modules for workers
	qjs_init_all_modules(ctx, GF_TRUE, GF_FALSE);
	return ctx;
}

void js_promise_rejection_tracker(JSContext *ctx, JSValueConst promise, JSValueConst reason, JS_BOOL is_handled, void *opaque)
{
    if (!is_handled) {
        GF_LOG(GF_LOG_WARNING, GF_LOG_CONSOLE, ("Possibly unhandled promise rejection: "));
        js_dump_error_exc(ctx, reason);
    }
}
#endif

static void qjs_init_runtime_libc(JSRuntime *rt)
{
 	if (gf_opts_get_bool("core", "no-js-mods"))
		return;

    /* module loader */
	JS_SetModuleLoaderFunc(rt, NULL, qjs_module_loader, NULL);

#ifndef GPAC_DISABLE_QJS_LIBC

    js_std_set_worker_new_context_func(JS_NewWorkerContext);
    js_std_init_handlers(rt);


    if (gf_opts_get_bool("core", "unhandled-rejection")) {
        JS_SetHostPromiseRejectionTracker(rt, js_promise_rejection_tracker, NULL);
    }
#endif

}

static void qjs_uninit_runtime_libc(JSRuntime *rt)
{
 	if (gf_opts_get_bool("core", "no-js-mods"))
		return;

#ifndef GPAC_DISABLE_QJS_LIBC
	js_std_free_handlers(rt);
#endif

}

#endif

