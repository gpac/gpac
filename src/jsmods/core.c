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
#include <gpac/base_coding.h>

#include "../scenegraph/qjs_common.h"

#define JS_CGETSET_MAGIC_DEF_ENUM(name, fgetter, fsetter, magic) { name, JS_PROP_CONFIGURABLE|JS_PROP_ENUMERABLE, JS_DEF_CGETSET_MAGIC, magic, .u = { .getset = { .get = { .getter_magic = fgetter }, .set = { .setter_magic = fsetter } } } }


static JSClassID bitstream_class_id = 0;
static JSClassID sha1_class_id = 0;
static JSClassID file_class_id = 0;

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
	if (!bs) return JS_EXCEPTION;
	return JS_NewInt32(ctx, gf_bs_read_u8(bs) );
}
static JSValue js_bs_get_s8(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s8 v;
	GET_JSBS
	if (!bs) return JS_EXCEPTION;
	v = (s8) gf_bs_read_u8(bs);
	return JS_NewInt32(ctx, v );
}
static JSValue js_bs_get_u16(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GET_JSBS
	if (!bs) return JS_EXCEPTION;
	return JS_NewInt32(ctx, gf_bs_read_u16(bs) );
}
static JSValue js_bs_get_u16_le(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GET_JSBS
	if (!bs) return JS_EXCEPTION;
	return JS_NewInt32(ctx, gf_bs_read_u16_le(bs) );
}
static JSValue js_bs_get_s16(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s16 v;
	GET_JSBS
	if (!bs) return JS_EXCEPTION;
	v = (s16) gf_bs_read_u16(bs);
	return JS_NewInt32(ctx, v );
}
static JSValue js_bs_get_u24(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GET_JSBS
	if (!bs) return JS_EXCEPTION;
	return JS_NewInt32(ctx, gf_bs_read_u24(bs) );
}
static JSValue js_bs_get_s32(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s32 v;
	GET_JSBS
	if (!bs) return JS_EXCEPTION;
	v = (s32) gf_bs_read_u32(bs);
	return JS_NewInt32(ctx, v);
}
static JSValue js_bs_get_u32(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GET_JSBS
	if (!bs) return JS_EXCEPTION;
	return JS_NewInt32(ctx, gf_bs_read_u32(bs) );
}
static JSValue js_bs_get_u32_le(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GET_JSBS
	if (!bs) return JS_EXCEPTION;
	return JS_NewInt32(ctx, gf_bs_read_u32_le(bs) );
}
static JSValue js_bs_get_u64(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GET_JSBS
	if (!bs) return JS_EXCEPTION;
	return JS_NewInt64(ctx, gf_bs_read_u64(bs) );
}
static JSValue js_bs_get_u64_le(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GET_JSBS
	if (!bs) return JS_EXCEPTION;
	return JS_NewInt64(ctx, gf_bs_read_u64_le(bs) );
}
static JSValue js_bs_get_s64(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s64 v;
	GET_JSBS
	if (!bs) return JS_EXCEPTION;
	v = (s64) gf_bs_read_u64(bs);
	return JS_NewInt64(ctx, v );
}
static JSValue js_bs_get_float(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GET_JSBS
	if (!bs) return JS_EXCEPTION;
	return JS_NewFloat64(ctx, gf_bs_read_float(bs) );
}
static JSValue js_bs_get_double(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GET_JSBS
	if (!bs) return JS_EXCEPTION;
	return JS_NewFloat64(ctx, gf_bs_read_double(bs) );
}
static JSValue js_bs_get_bits(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s32 nb_bits;
	GET_JSBS
	if (!bs || !argc) return JS_EXCEPTION;
	if (JS_ToInt32(ctx, &nb_bits, argv[0])) return JS_EXCEPTION;
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
	if (!bs) return JS_EXCEPTION;
	if (argc<1) return JS_EXCEPTION;

	if (JS_IsObject(argv[0])) {
		data = JS_GetArrayBuffer(ctx, &data_size, argv[0]);
		if (argc>1) {
			if (JS_ToInt32(ctx, &nb_bytes, argv[1])) return JS_EXCEPTION;
			if ((mode==2) && (argc>2)) {
				if (JS_ToInt64(ctx, &offset, argv[2])) return JS_EXCEPTION;

			}
		}
	}
	if (!data) return JS_EXCEPTION;

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
	if (!bs || !argc) return JS_EXCEPTION;
	if (JS_ToInt32(ctx, &nb_bytes, argv[0])) return JS_EXCEPTION;
	gf_bs_skip_bytes(bs, nb_bytes);
	return JS_UNDEFINED;
}
static JSValue js_bs_is_align(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GET_JSBS
	if (!bs) return JS_EXCEPTION;
	if (gf_bs_is_align(bs)) return JS_TRUE;
	return JS_FALSE;
}
static JSValue js_bs_align(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GET_JSBS
	if (!bs) return JS_EXCEPTION;
	gf_bs_align(bs);
	return JS_UNDEFINED;
}
static JSValue js_bs_truncate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GET_JSBS
	if (!bs) return JS_EXCEPTION;
	gf_bs_truncate(bs);
	return JS_UNDEFINED;
}
static JSValue js_bs_flush(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GET_JSBS
	if (!bs) return JS_EXCEPTION;
	gf_bs_flush(bs);
	return JS_UNDEFINED;
}
static JSValue js_bs_epb_mode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GET_JSBS
	if (!bs || !argc) return JS_EXCEPTION;
	gf_bs_enable_emulation_byte_removal(bs, (JS_ToBool(ctx, argv[0])) ? GF_TRUE : GF_FALSE);
	return JS_UNDEFINED;
}

static JSValue js_bs_peek(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u64 byte_offset=0;
	s32 nb_bits;
	GET_JSBS
	if (!bs || !argc) return JS_EXCEPTION;
	if (JS_ToInt32(ctx, &nb_bits, argv[0])) return JS_EXCEPTION;
	if (argc>1) {
		if (JS_ToInt64(ctx, &byte_offset, argv[1])) return JS_EXCEPTION;
	}
	return JS_NewInt32(ctx, gf_bs_peek_bits(bs, nb_bits, byte_offset));
}

static JSValue js_bs_put_val(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, u32 mode)
{
	s64 val;
	GET_JSBS
	if (!bs || !argc) return JS_EXCEPTION;
	if (JS_ToInt64(ctx, &val, argv[0])) return JS_EXCEPTION;
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
	if (!bs || (argc!=2)) return JS_EXCEPTION;
	if (JS_ToInt64(ctx, &val, argv[0])) return JS_EXCEPTION;
	if (JS_ToInt32(ctx, &nb_bits, argv[1])) return JS_EXCEPTION;
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
	if (!bs || (argc!=1)) return JS_EXCEPTION;
	if (JS_ToFloat64(ctx, &val, argv[0])) return JS_EXCEPTION;
	gf_bs_write_float(bs, (Float) val);
	return JS_UNDEFINED;
}
static JSValue js_bs_put_double(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Double val;
	GET_JSBS
	if (!bs || (argc!=1)) return JS_EXCEPTION;
	if (JS_ToFloat64(ctx, &val, argv[0])) return JS_EXCEPTION;
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
	if (!bs) return JS_EXCEPTION;
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
	if (!bs || !argc) return JS_EXCEPTION;

	jssrcbs = JS_GetOpaque(argv[0], bitstream_class_id);
	if (!jssrcbs || !jssrcbs->bs) return JS_EXCEPTION;
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
	if (!bs) return JS_EXCEPTION;
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
	if (!bs) return JS_EXCEPTION;

	switch (magic) {
	case JS_BS_POS:
		if (JS_ToInt64(ctx, &ival, value)) return JS_EXCEPTION;
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
			if (!data) return JS_EXCEPTION;
			if ((argc>1) && JS_ToBool(ctx, argv[1])) {
				bs = gf_bs_new(data, data_size, GF_BITSTREAM_WRITE);
			} else {
				bs = gf_bs_new(data, data_size, GF_BITSTREAM_READ);
			}
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
		return JS_NewBool(ctx, gf_log_use_color() );
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
	if (!argc) return JS_EXCEPTION;
	echo_off = JS_ToBool(ctx, argv[0]);
	if (argc<2)
		gf_prompt_set_echo_off(echo_off);
	return JS_UNDEFINED;
}
static JSValue js_sys_prompt_code(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s32 code;
	if (!argc) return JS_EXCEPTION;
	if (JS_ToInt32(ctx, &code, argv[0])) return JS_EXCEPTION;

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
	if (!argc) return JS_EXCEPTION;
	if (JS_ToInt64(ctx, &val, argv[0])) return JS_EXCEPTION;
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
	if (!argc) return JS_EXCEPTION;
	str = JS_ToCString(ctx, argv[0]);
	if (!str) return JS_EXCEPTION;
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
		if (!date) return JS_EXCEPTION;
		time = gf_net_parse_date(date);
		JS_FreeCString(ctx, date);
		return JS_NewInt64(ctx, time);
	}

	if (argc != 6) return JS_EXCEPTION;
	if (JS_ToInt32(ctx, &y, argv[0])) return JS_EXCEPTION;
	if (JS_ToInt32(ctx, &mo, argv[1])) return JS_EXCEPTION;
	if (JS_ToInt32(ctx, &d, argv[2])) return JS_EXCEPTION;
	if (JS_ToInt32(ctx, &h, argv[3])) return JS_EXCEPTION;
	if (JS_ToInt32(ctx, &m, argv[4])) return JS_EXCEPTION;
	if (JS_ToInt32(ctx, &s, argv[5])) return JS_EXCEPTION;

	return JS_NewInt64(ctx, gf_net_get_utc_ts(y, mo, d, h, m, s) );
}


static JSValue js_sys_crc32(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const u8 *data;
	size_t data_size;

	if (!argc || !JS_IsObject(argv[0])) return JS_EXCEPTION;
	data = JS_GetArrayBuffer(ctx, &data_size, argv[0] );
	if (!data) return JS_EXCEPTION;
	return JS_NewInt32(ctx, gf_crc_32(data, (u32) data_size) );
}

static JSValue js_sys_sha1(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u8 csum[GF_SHA1_DIGEST_SIZE];
	const u8 *data;
	size_t data_size;

	if (!argc) return JS_EXCEPTION;
	if (JS_IsString(argv[0])) {
		const char *filename = JS_ToCString(ctx, argv[0]);
		if (!filename) return JS_EXCEPTION;
		gf_sha1_file(filename, csum);
		JS_FreeCString(ctx, filename);
		return JS_NewArrayBufferCopy(ctx, csum, GF_SHA1_DIGEST_SIZE);
	} else if (JS_IsObject(argv[0])) {
		data = JS_GetArrayBuffer(ctx, &data_size, argv[0] );
		if (!data) return JS_EXCEPTION;

		gf_sha1_csum((u8 *) data, (u32) data_size, csum);
		return JS_NewArrayBufferCopy(ctx, csum, GF_SHA1_DIGEST_SIZE);
	}
	return JS_EXCEPTION;
}


static JSValue js_sys_file_data(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *filename;
	u8 *data;
	u32 data_size;
	GF_Err e;
	JSValue res;
	if (!argc || !JS_IsString(argv[0])) return JS_EXCEPTION;
	filename = JS_ToCString(ctx, argv[0]);
	if (!filename) return JS_EXCEPTION;

	e = gf_file_load_data(filename, &data, &data_size);
	if (!data) res = js_throw_err_msg(ctx, e, "Failed to load file %s", filename);
	else res = JS_NewArrayBuffer(ctx, data, data_size, js_gpac_free, NULL, 0);
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
	if (!argc || !JS_IsObject(argv[0])) return JS_EXCEPTION;
	data = JS_GetArrayBuffer(ctx, &data_size, argv[0] );
	if (!data) return JS_EXCEPTION;
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
	if (!argc || !JS_IsString(argv[0])) return JS_EXCEPTION;
	dirname = JS_ToCString(ctx, argv[0]);
	if (!dirname) return JS_EXCEPTION;

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
		e = gf_cleanup_dir(dirname);
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
	if (argc!=2) return JS_EXCEPTION;
	sec = JS_ToCString(ctx, argv[0]);
	if (!sec) return JS_EXCEPTION;
	key = JS_ToCString(ctx, argv[1]);
	if (!key) {
		JS_FreeCString(ctx, sec);
		return JS_EXCEPTION;
	}
	val = gf_opts_get_key_restricted(sec, key);
	res = val ? JS_NewString(ctx, val) : JS_NULL;
	JS_FreeCString(ctx, sec);
	JS_FreeCString(ctx, key);
	return res;
}

static JSValue js_sys_set_opt(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *sec, *key, *val;
	if (argc<2) return JS_EXCEPTION;
	sec = JS_ToCString(ctx, argv[0]);
	if (!sec) return JS_EXCEPTION;
	key = JS_ToCString(ctx, argv[1]);
	if (!key) {
		JS_FreeCString(ctx, sec);
		return JS_EXCEPTION;
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
	if (!argc) return JS_EXCEPTION;

	if (is_dec) {
		u32 len;
		const char *str = JS_ToCString(ctx, argv[0]);
		if (!str) return JS_EXCEPTION;
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
		if (!data) return JS_EXCEPTION;
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
	if (!argc) return JS_EXCEPTION;
	if (JS_ToInt64(ctx, &val, argv[0])) return JS_EXCEPTION;
	return JS_NewInt32(ctx, gf_ntohl((u32) val));
}
static JSValue js_sys_ntohs(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s64 val;
	if (!argc) return JS_EXCEPTION;
	if (JS_ToInt64(ctx, &val, argv[0])) return JS_EXCEPTION;
	return JS_NewInt32(ctx, gf_ntohs((u16) val));
}
static JSValue js_sys_htonl(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s64 val;
	if (!argc) return JS_EXCEPTION;
	if (JS_ToInt64(ctx, &val, argv[0])) return JS_EXCEPTION;
	return JS_NewInt32(ctx, gf_htonl((u32) val));
}
static JSValue js_sys_htons(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s64 val;
	if (!argc) return JS_EXCEPTION;
	if (JS_ToInt64(ctx, &val, argv[0])) return JS_EXCEPTION;
	return JS_NewInt32(ctx, gf_htons((u16) val));
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

    JS_CFUNC_DEF("gc", 0, js_sys_gc),

	JS_CFUNC_DEF("enum_directory", 0, js_sys_enum_directory),
	JS_CFUNC_DEF("clock_ms", 0, js_sys_clock),
	JS_CFUNC_DEF("clock_us", 0, js_sys_clock_high_res),
	JS_CFUNC_DEF("fcc_to_str", 0, js_sys_4cc_to_str),
	JS_CFUNC_DEF("rand_init", 0, js_sys_rand_init),
	JS_CFUNC_DEF("rand", 0, js_sys_rand),
	JS_CFUNC_DEF("rand64", 0, js_sys_rand64),
	JS_CFUNC_DEF("getenv", 0, js_sys_getenv),
	JS_CFUNC_DEF("get_utc", 0, js_sys_get_utc),
	JS_CFUNC_DEF("crc32", 0, js_sys_crc32),
	JS_CFUNC_DEF("sha1", 0, js_sys_sha1),
	JS_CFUNC_DEF("load_file", 0, js_sys_file_data),
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
	if (!argc) return JS_EXCEPTION;
	data = JS_GetArrayBuffer(ctx, &data_size, argv[0]);
	gf_sha1_update(sha1, (u8 *) data, (u32) data_size);

	return JS_UNDEFINED;
}
static JSValue js_sha1_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u8 output[GF_SHA1_DIGEST_SIZE];
	GF_SHA1Context *sha1 = JS_GetOpaque(this_val, sha1_class_id);
	if (!sha1) return JS_EXCEPTION;
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
	if (!f) return JS_EXCEPTION;

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
	if (!f) return JS_EXCEPTION;

	switch (magic) {
	case JS_FILE_POS:
		if (JS_ToInt64(ctx, &lival, value)) return JS_EXCEPTION;
		gf_fseek(f, lival, SEEK_SET);
		break;
	}
	return JS_UNDEFINED;
}
static JSValue js_file_flush(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	FILE *f = JS_GetOpaque(this_val, file_class_id);
	if (!f) return JS_EXCEPTION;
	gf_fflush(f);
	return JS_UNDEFINED;
}
static JSValue js_file_close(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	FILE *f = JS_GetOpaque(this_val, file_class_id);
	if (!f) return JS_EXCEPTION;
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
	if (!f) return JS_EXCEPTION;
	if (!argc) return JS_EXCEPTION;

	data = JS_GetArrayBuffer(ctx, &size, argv[0]);
	if (!data) return JS_EXCEPTION;
	if (argc>1) {
		if (JS_ToInt32(ctx, &nb_bytes, argv[1])) return JS_EXCEPTION;
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
	if (!f) return JS_EXCEPTION;

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
	if (!f) return JS_EXCEPTION;

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
	if (!f) return JS_EXCEPTION;
	if (!argc) return JS_EXCEPTION;

	data = JS_GetArrayBuffer(ctx, &size, argv[0]);
	if (!data) return JS_EXCEPTION;
	if (argc>1) {
		if (JS_ToInt32(ctx, &nb_bytes, argv[1])) return JS_EXCEPTION;
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
	if (!f || !argc) return JS_EXCEPTION;

	string = JS_ToCString(ctx, argv[0]);
	if (!string) return JS_EXCEPTION;
	gf_fputs(string, f);
	JS_FreeCString(ctx, string);
	return JS_UNDEFINED;
}

static JSValue js_file_putc(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	int val=0;
	FILE *f = JS_GetOpaque(this_val, file_class_id);
	if (!f || !argc) return JS_EXCEPTION;

	if (JS_IsString(argv[0])) {
		const char *string = JS_ToCString(ctx, argv[0]);
		if (!string) return JS_EXCEPTION;
		val = string[0];
		JS_FreeCString(ctx, string);
	} else {
		if (JS_ToInt32(ctx, &val, argv[0])) return JS_EXCEPTION;
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
	return;
}


#endif

