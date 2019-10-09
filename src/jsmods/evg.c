/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2007-2019
 *			All rights reserved
 *
 *  This file is part of GPAC / JavaScript C modules
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

#ifdef GPAC_HAS_QJS

/*base SVG type*/
#include <gpac/nodes_svg.h>
#include <gpac/nodes_mpeg4.h>
#include <gpac/nodes_x3d.h>
/*dom events*/
#include <gpac/evg.h>

#include "../scenegraph/qjs_common.h"

#include <gpac/internal/compositor_dev.h>

/*for texture from png/rgb*/
#include <gpac/bitstream.h>
#include <gpac/avparse.h>
#include <gpac/network.h>

typedef struct
{
	u32 width, height, pf, stride, stride_uv;
	char *data;
	Bool owns_data;
	Bool center_coords;
	GF_EVGSurface *surface;
	u32 composite_op;
	JSValue alpha_cbk;
	JSContext *ctx;
	JSValue obj;
} GF_JSCanvas;


typedef struct
{
	u32 width, height, pf, stride, stride_uv, nb_comp;
	char *data;
	u32 data_size;
	Bool owns_data;
	u32 flags;
	GF_EVGStencil *stencil;
} GF_JSTexture;


typedef struct
{
	GF_FontManager *fm;
	GF_Path *path;
	char *fontname;
	Double font_size;
	u32 align;
	u32 baseline;
	u32 styles;
	GF_List *spans;
	Bool horizontal;
	Bool flip;
	Double maxWidth;
	Double lineSpacing;
	Fixed min_x, min_y, max_x, max_y, max_w, max_h;
	GF_Font *font;
	Bool path_for_centered;
} GF_JSText;

JSClassID canvas_class_id;
JSClassID path_class_id;
JSClassID mx2d_class_id;
JSClassID colmx_class_id;
JSClassID stencil_class_id;
JSClassID texture_class_id;
JSClassID text_class_id;

static void text_set_path(GF_JSCanvas *canvas, GF_JSText *text);


static void canvas_finalize(JSRuntime *rt, JSValue obj)
{
	GF_JSCanvas *canvas = JS_GetOpaque(obj, canvas_class_id);
	if (!canvas) return;
	JS_FreeValueRT(rt, canvas->alpha_cbk);

	if (canvas->owns_data)
		gf_free(canvas->data);
	if (canvas->surface)
		gf_evg_surface_delete(canvas->surface);
	gf_free(canvas);
}

static void canvas_gc_mark(JSRuntime *rt, JSValueConst obj, JS_MarkFunc *mark_func)
{
	GF_JSCanvas *canvas = JS_GetOpaque(obj, canvas_class_id);
	if (!canvas) return;
	JS_MarkValue(rt, canvas->alpha_cbk, mark_func);
}

JSClassDef canvas_class = {
	"Canvas",
	.finalizer = canvas_finalize,
	.gc_mark = canvas_gc_mark
};

enum
{
	GF_EVG_CENTERED = 0,
	GF_EVG_PATH,
	GF_EVG_MATRIX,
	GF_EVG_CLIPPER,
	GF_EVG_COMPOSITE_OP,
	GF_EVG_ALPHA_FUN,
};

u8 evg_get_alpha(void *cbk, u8 src_alpha, s32 x, s32 y)
{
	u32 res_a=0;
	GF_JSCanvas *canvas = cbk;
	JSValue ret, args[3];
	args[0] = JS_NewInt32(canvas->ctx, src_alpha);
	args[1] = JS_NewInt32(canvas->ctx, x);
	args[2] = JS_NewInt32(canvas->ctx, y);
	ret = JS_Call(canvas->ctx, canvas->alpha_cbk, canvas->obj, 3, args);
	JS_ToInt32(canvas->ctx, &res_a, ret);
	return res_a;

}

static JSValue canvas_constructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	u32 width, height, pf=0, osize;
	size_t data_size=0;
	u8 *data=NULL;
	u32 stride = 0;
	u32 stride_uv = 0;
	GF_JSCanvas *canvas;
	GF_Err e;

	if (argc<3)
		return JS_EXCEPTION;
	if (JS_ToInt32(c, &width, argv[0]))
		return JS_EXCEPTION;
	if (JS_ToInt32(c, &height, argv[1]))
		return JS_EXCEPTION;
	if (JS_IsString(argv[2])) {
		const char *str = JS_ToCString(c, argv[2]);
		pf = gf_pixel_fmt_parse(str);
		JS_FreeCString(c, str);
	} else if (JS_ToInt32(c, &pf, argv[2])) {
		return JS_EXCEPTION;
	}
	if (!width || !height || !pf)
		return JS_EXCEPTION;

	if (argc>3) {
		u32 idx=0;
		if (JS_IsObject(argv[3])) {
			data = JS_GetArrayBuffer(c, &data_size, argv[3]);
			if (!data) return JS_EXCEPTION;
			idx=1;
		} else if (!JS_IsInteger(argv[3]))
			return JS_EXCEPTION;

		if (argc>3+idx) {
			if (JS_ToInt32(c, &stride, argv[3+idx]))
				return JS_EXCEPTION;
			if (argc>4+idx) {
				if (JS_ToInt32(c, &stride_uv, argv[4+idx]))
					return JS_EXCEPTION;
			}
		}
	}
	GF_SAFEALLOC(canvas, GF_JSCanvas);

	if (!canvas)
		return JS_EXCEPTION;

	if (!gf_pixel_get_size_info(pf, width, height, &osize, &stride, &stride_uv, NULL, NULL)) {
		gf_free(canvas);
		return JS_EXCEPTION;
	}
	if (data && (data_size<osize)) {
		gf_free(canvas);
		return JS_EXCEPTION;
	}

	canvas->width = width;
	canvas->height = height;
	canvas->pf = pf;
	canvas->stride = stride;
	canvas->alpha_cbk = JS_UNDEFINED;
	canvas->ctx = c;
	if (data) {
		canvas->data = data;
		canvas->owns_data = GF_FALSE;
	} else {
		canvas->data = gf_malloc(sizeof(u8)*osize);
		canvas->owns_data = GF_TRUE;
	}
	canvas->surface = gf_evg_surface_new(GF_TRUE);
	canvas->center_coords = GF_TRUE;
	if (!canvas->surface)
		e = GF_BAD_PARAM;
	else
		e = gf_evg_surface_attach_to_buffer(canvas->surface, canvas->data, canvas->width, canvas->height, 0, canvas->stride, canvas->pf);
	if (e) {
		if (canvas->owns_data) gf_free(canvas->data);
		gf_evg_surface_delete(canvas->surface);
		gf_free(canvas);
		return JS_EXCEPTION;
	}
	canvas->obj = JS_NewObjectClass(c, canvas_class_id);
	if (JS_IsException(canvas->obj)) return canvas->obj;

	JS_SetOpaque(canvas->obj, canvas);
	return canvas->obj;
}
static JSValue canvas_getProperty(JSContext *c, JSValueConst obj, int magic)
{
	GF_JSCanvas *canvas = JS_GetOpaque(obj, canvas_class_id);
	if (!canvas) return JS_EXCEPTION;
	switch (magic) {
	case GF_EVG_CENTERED: return JS_NewBool(c, canvas->center_coords);
	case GF_EVG_COMPOSITE_OP: return JS_NewInt32(c, canvas->composite_op);
	case GF_EVG_ALPHA_FUN: return JS_DupValue(c, canvas->alpha_cbk);
	}
	return JS_UNDEFINED;
}

Bool canvas_get_irect(JSContext *c, JSValueConst obj, GF_IRect *rc)
{
	JSValue v;
	int res;
	memset(rc, 0, sizeof(GF_IRect));

#define GET_PROP( _n, _f)\
	v = JS_GetPropertyStr(c, obj, _n);\
	res = JS_ToInt32(c, &(rc->_f), v);\
	JS_FreeValue(c, v);\
	if (res) return GF_FALSE;\

	GET_PROP("x", x)
	GET_PROP("y", y)
	GET_PROP("w", width)
	GET_PROP("h", height)
#undef GET_PROP
	return GF_TRUE;
}

static JSValue canvas_setProperty(JSContext *c, JSValueConst obj, JSValueConst value, int magic)
{
	GF_JSCanvas *canvas = JS_GetOpaque(obj, canvas_class_id);
	if (!canvas) return JS_EXCEPTION;
	switch (magic) {
	case GF_EVG_CENTERED:
		canvas->center_coords = JS_ToBool(c, value) ? GF_TRUE : GF_FALSE;
		gf_evg_surface_set_center_coords(canvas->surface, canvas->center_coords);
		return JS_UNDEFINED;

	case GF_EVG_PATH:
		if (JS_IsNull(value)) {
			gf_evg_surface_set_path(canvas->surface, NULL);
		} else {
			GF_Path *gp = JS_GetOpaque(value, path_class_id);
			if (gp)
				gf_evg_surface_set_path(canvas->surface, gp);
			else {
				GF_JSText *text = JS_GetOpaque(value, text_class_id);
				if (text) {
					text_set_path(canvas, text);
				}
			}
		}
		return JS_UNDEFINED;

	case GF_EVG_MATRIX:
		if (JS_IsNull(value)) {
			gf_evg_surface_set_matrix(canvas->surface, NULL);
		} else {
			GF_Matrix2D *mx = JS_GetOpaque(value, mx2d_class_id);
			gf_evg_surface_set_matrix(canvas->surface, mx);
		}
		return JS_UNDEFINED;

	case GF_EVG_CLIPPER:
		if (JS_IsNull(value)) {
			gf_evg_surface_set_clipper(canvas->surface, NULL);
		} else {
			GF_IRect rc;
			canvas_get_irect(c, value, &rc);
			gf_evg_surface_set_clipper(canvas->surface, &rc);
		}
		return JS_UNDEFINED;
	case GF_EVG_COMPOSITE_OP:
		if (JS_ToInt32(c, &canvas->composite_op, value)) return JS_EXCEPTION;
		gf_evg_surface_set_composite_mode(canvas->surface, canvas->composite_op);
		break;
	case GF_EVG_ALPHA_FUN:
		JS_FreeValue(c, canvas->alpha_cbk);
		if (JS_IsNull(value) || !JS_IsFunction(c, value)) {
			canvas->alpha_cbk = JS_UNDEFINED;
			gf_evg_surface_set_alpha_callback(canvas->surface, NULL, NULL);
		} else {
			canvas->alpha_cbk = JS_DupValue(c, value);
			gf_evg_surface_set_alpha_callback(canvas->surface, evg_get_alpha, canvas);
		}
		return JS_UNDEFINED;
	}
	return JS_UNDEFINED;
}


static JSValue canvas_clear_ex(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv, Bool use_float)
{
	u32 i;
	u32 idx=0;
	GF_Err e;
	GF_IRect rc, *irc;
	u32 r=0, g=0, b=0, a=255;
	GF_Color col;
	GF_JSCanvas *canvas = JS_GetOpaque(obj, canvas_class_id);
	if (!canvas)
		return JS_EXCEPTION;

	irc = NULL;
	if (argc && JS_IsObject(argv[0])) {
		irc = &rc;
		idx=1;
		if (!canvas_get_irect(c, argv[0], &rc))
			return JS_EXCEPTION;
	}
	if ((argc>idx) && JS_IsString(argv[idx])) {
		const char *str = JS_ToCString(c, argv[0]);
		col = gf_color_parse(str);
		JS_FreeCString(c, str);
	} else {
		if (argc>4+idx) argc = 4+idx;
		for (i=idx; i<argc; i++) {
			s32 v;
			if (use_float) {
				Double d;
				if (JS_ToFloat64(c, &d, argv[i]))
					return JS_EXCEPTION;
				v = (s32) (d*255);
			} else if (JS_ToInt32(c, &v, argv[i])) {
				return JS_EXCEPTION;
			}

			if (v<0) v = 0;
			else if (v>255) v = 255;

			if (i==idx) r=v;
			else if (i==idx+1) g=v;
			else if (i==idx+2) b=v;
			else a=v;
		}
		col = GF_COL_ARGB(a, r, g, b) ;
	}
	e = gf_evg_surface_clear(canvas->surface, irc, col);
	if (e)
		return JS_EXCEPTION;
	return JS_UNDEFINED;
}
static JSValue canvas_clear(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return canvas_clear_ex(c, obj, argc, argv, GF_FALSE);
}
static JSValue canvas_clearf(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return canvas_clear_ex(c, obj, argc, argv, GF_TRUE);
}


static JSValue canvas_fill(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_EVGStencil *stencil;
	GF_JSTexture *tx;
	GF_JSCanvas *canvas = JS_GetOpaque(obj, canvas_class_id);
	if (!canvas || !argc) return JS_EXCEPTION;
	stencil = JS_GetOpaque(argv[0], stencil_class_id);
	if (stencil) {
		gf_evg_surface_fill(canvas->surface, stencil);
		return JS_UNDEFINED;
	}
	tx = JS_GetOpaque(argv[0], texture_class_id);
	if (tx) {
		gf_evg_surface_fill(canvas->surface, tx->stencil);
		return JS_UNDEFINED;
	}
	return JS_EXCEPTION;
}

static const JSCFunctionListEntry canvas_funcs[] =
{
	JS_CGETSET_MAGIC_DEF("centered", canvas_getProperty, canvas_setProperty, GF_EVG_CENTERED),
	JS_CGETSET_MAGIC_DEF("path", NULL, canvas_setProperty, GF_EVG_PATH),
	JS_CGETSET_MAGIC_DEF("clipper", NULL, canvas_setProperty, GF_EVG_CLIPPER),
	JS_CGETSET_MAGIC_DEF("matrix", NULL, canvas_setProperty, GF_EVG_MATRIX),
	JS_CGETSET_MAGIC_DEF("compositeOperation", canvas_getProperty, canvas_setProperty, GF_EVG_COMPOSITE_OP),
	JS_CGETSET_MAGIC_DEF("on_alpha", canvas_getProperty, canvas_setProperty, GF_EVG_ALPHA_FUN),
	JS_CFUNC_DEF("clear", 0, canvas_clear),
	JS_CFUNC_DEF("clearf", 0, canvas_clearf),
	JS_CFUNC_DEF("fill", 0, canvas_fill),
};

static void mx2d_finalize(JSRuntime *rt, JSValue obj)
{
	GF_Matrix2D *mx = JS_GetOpaque(obj, mx2d_class_id);
	if (!mx) return;
	gf_free(mx);
}
JSClassDef mx2d_class = {
	"Matrix2D",
	.finalizer = mx2d_finalize
};

enum
{
	//these 6 magics map to m[x] of the matrix, DO NOT MODIFY
	MX2D_XX = 0,
	MX2D_XY,
	MX2D_TX,
	MX2D_YX,
	MX2D_YY,
	MX2D_TY,
	MX2D_IDENTITY,
};

static JSValue mx2d_getProperty(JSContext *c, JSValueConst obj, int magic)
{
	GF_Matrix2D *mx = JS_GetOpaque(obj, mx2d_class_id);
	if (!mx) return JS_EXCEPTION;
	if ((magic>=MX2D_XX) && (magic<=MX2D_TY)) {
		return JS_NewFloat64(c, FIX2FLT(mx->m[magic]));
	}
	if (magic==MX2D_IDENTITY)
		return JS_NewBool(c, gf_mx2d_is_identity(*mx));
	return JS_UNDEFINED;
}
static JSValue mx2d_setProperty(JSContext *c, JSValueConst obj, JSValueConst value, int magic)
{
	GF_Matrix2D *mx = JS_GetOpaque(obj, mx2d_class_id);
	if (!mx) return JS_EXCEPTION;
	if ((magic>=MX2D_XX) && (magic<=MX2D_TY)) {
		Double d;
		if (JS_ToFloat64(c, &d, value))
			return JS_EXCEPTION;
		mx->m[magic] = FIX2FLT(d);
		return JS_UNDEFINED;
	}
	if (magic==MX2D_IDENTITY) {
		if (JS_ToBool(c, value))
			gf_mx2d_init(*mx);
		return JS_UNDEFINED;
	}
	return JS_UNDEFINED;
}

static JSValue mx2d_multiply(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_Matrix2D *mx = JS_GetOpaque(obj, mx2d_class_id);
	if (!mx || !argc) return JS_EXCEPTION;
	GF_Matrix2D *amx = JS_GetOpaque(argv[0], mx2d_class_id);
	if (!mx) return JS_EXCEPTION;
	if ((argc>1) && JS_ToBool(c, argv[1]))
		gf_mx2d_pre_multiply(mx, amx);
	else
		gf_mx2d_add_matrix(mx, amx);
	return JS_DupValue(c, obj);
}

static JSValue mx2d_translate(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	Double tx, ty;
	GF_Matrix2D *mx = JS_GetOpaque(obj, mx2d_class_id);
	if (!mx || (argc<1)) return JS_EXCEPTION;

	if (JS_IsObject(argv[0])) {
		JSValue v;
#define GETD(_arg, _name, _res)\
		if (! JS_IsObject(_arg)) return JS_EXCEPTION;\
		v = JS_GetPropertyStr(c, _arg, _name);\
		JS_ToFloat64(c, &_res, v);\
		JS_FreeValue(c, v);\

		GETD(argv[0], "x", tx);
		GETD(argv[0], "y", ty);
#undef GETD

	} else if (argc==2) {
		if (JS_ToFloat64(c, &tx, argv[0])) return JS_EXCEPTION;
		if (JS_ToFloat64(c, &ty, argv[1])) return JS_EXCEPTION;
	} else {
		return JS_EXCEPTION;
	}
	gf_mx2d_add_translation(mx, FLT2FIX(tx), FLT2FIX(ty));
	return JS_DupValue(c, obj);
}
static JSValue mx2d_rotate(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	Double cx, cy, a;
	GF_Matrix2D *mx = JS_GetOpaque(obj, mx2d_class_id);
	if (!mx || (argc<3)) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &cx, argv[0])) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &cy, argv[1])) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &a, argv[2])) return JS_EXCEPTION;
	gf_mx2d_add_rotation(mx, FLT2FIX(cx), FLT2FIX(cy), FLT2FIX(a) );
	return JS_DupValue(c, obj);
}
static JSValue mx2d_scale(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	Double sx, sy;
	GF_Matrix2D *mx = JS_GetOpaque(obj, mx2d_class_id);
	if (!mx || (argc<2)) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &sx, argv[0])) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &sy, argv[1])) return JS_EXCEPTION;
	if (argc==2) {
		gf_mx2d_add_scale(mx, FLT2FIX(sx), FLT2FIX(sy));
		return JS_DupValue(c, obj);
	} else if (argc==5) {
		Double cx, cy, a;
		if (JS_ToFloat64(c, &cx, argv[2])) return JS_EXCEPTION;
		if (JS_ToFloat64(c, &cy, argv[3])) return JS_EXCEPTION;
		if (JS_ToFloat64(c, &a, argv[4])) return JS_EXCEPTION;
		gf_mx2d_add_scale_at(mx, FLT2FIX(sx), FLT2FIX(sy), FLT2FIX(cx), FLT2FIX(cy), FLT2FIX(a));
		return JS_DupValue(c, obj);
	}
	return JS_EXCEPTION;
}

static JSValue mx2d_skew(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	Double sx, sy;
	GF_Matrix2D *mx = JS_GetOpaque(obj, mx2d_class_id);
	if (!mx || (argc<2)) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &sx, argv[0])) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &sy, argv[1])) return JS_EXCEPTION;
	gf_mx2d_add_skew(mx, FLT2FIX(sx), FLT2FIX(sy));
	return JS_DupValue(c, obj);
}
static JSValue mx2d_skew_x(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	Double s;
	GF_Matrix2D *mx = JS_GetOpaque(obj, mx2d_class_id);
	if (!mx || !argc) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &s, argv[0])) return JS_EXCEPTION;
	gf_mx2d_add_skew_x(mx, FLT2FIX(s));
	return JS_DupValue(c, obj);
}
static JSValue mx2d_skew_y(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	Double s;
	GF_Matrix2D *mx = JS_GetOpaque(obj, mx2d_class_id);
	if (!mx || !argc) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &s, argv[0])) return JS_EXCEPTION;
	gf_mx2d_add_skew_y(mx, FLT2FIX(s));
	return JS_DupValue(c, obj);
}

static JSValue mx2d_apply(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	Fixed x, y, w, h;
	Double d;
	JSValue v;
	int res;
	u32 is_rect=0;
	GF_Matrix2D *mx = JS_GetOpaque(obj, mx2d_class_id);
	if (!mx || !argc || !JS_IsObject(argv[0])) return JS_EXCEPTION;
	v = JS_GetPropertyStr(c, argv[0], "x");
	res = JS_ToFloat64(c, &d, v);
	JS_FreeValue(c, v);
	if (res) return JS_EXCEPTION;
	x = FLT2FIX(d);

	v = JS_GetPropertyStr(c, argv[0], "y");
	res = JS_ToFloat64(c, &d, v);
	JS_FreeValue(c, v);
	if (res) return JS_EXCEPTION;
	y = FLT2FIX(d);

	v = JS_GetPropertyStr(c, argv[0], "w");
	if (!JS_IsUndefined(v) && JS_IsNumber(v)) {
		res = JS_ToFloat64(c, &d, v);
		JS_FreeValue(c, v);
		if (res) return JS_EXCEPTION;
		is_rect++;
		w = FLT2FIX(d);
	}
	JS_FreeValue(c, v);

	v = JS_GetPropertyStr(c, argv[0], "h");
	if (!JS_IsUndefined(v) && JS_IsNumber(v)) {
		res = JS_ToFloat64(c, &d, v);
		JS_FreeValue(c, v);
		if (res) return JS_EXCEPTION;
		is_rect++;
		h = FLT2FIX(d);
	}
	JS_FreeValue(c, v);

	if (is_rect) {
		GF_Rect rc;
		rc.x = x;
		rc.y = y;
		rc.width = w;
		rc.height = h;
		gf_mx2d_apply_rect(mx, &rc);
		v = JS_NewObject(c);
		JS_SetPropertyStr(c, v, "x", JS_NewFloat64(c, FIX2FLT(rc.x)));
		JS_SetPropertyStr(c, v, "y", JS_NewFloat64(c, FIX2FLT(rc.y)));
		JS_SetPropertyStr(c, v, "w", JS_NewFloat64(c, FIX2FLT(rc.width)));
		JS_SetPropertyStr(c, v, "h", JS_NewFloat64(c, FIX2FLT(rc.height)));
		return v;

	} else {
		GF_Point2D pt;
		pt.x = x;
		pt.y = y;
		gf_mx2d_apply_point(mx, &pt);
		v = JS_NewObject(c);
		JS_SetPropertyStr(c, v, "x", JS_NewFloat64(c, FIX2FLT(pt.x)));
		JS_SetPropertyStr(c, v, "y", JS_NewFloat64(c, FIX2FLT(pt.y)));
		return v;
	}
	return JS_EXCEPTION;
}

static JSValue mx2d_inverse(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_Matrix2D *mx = JS_GetOpaque(obj, mx2d_class_id);
	if (!mx) return JS_EXCEPTION;
	gf_mx2d_inverse(mx);
	return JS_DupValue(c, obj);
}

static JSValue mx2d_copy(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_Matrix2D *nmx;
	JSValue nobj;
	GF_Matrix2D *mx = JS_GetOpaque(obj, mx2d_class_id);
	if (!mx) return JS_EXCEPTION;
	GF_SAFEALLOC(nmx, GF_Matrix2D);
	if (!nmx) return JS_EXCEPTION;
	gf_mx2d_copy(*nmx, *mx);
	nobj = JS_NewObjectClass(c, mx2d_class_id);
	JS_SetOpaque(nobj, nmx);
	return nobj;
}

static JSValue mx2d_decompose_ex(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv, u32 opt)
{
	GF_Point2D scale;
	GF_Point2D translate;
	Fixed rotate;
	GF_Matrix2D *mx = JS_GetOpaque(obj, mx2d_class_id);
	if (!mx) return JS_EXCEPTION;
	if (!gf_mx2d_decompose(mx, &scale, &rotate, &translate))
		return JS_NULL;
	if (opt==0) {
		JSValue nobj = JS_NewObject(c);
		JS_SetPropertyStr(c, nobj, "x", JS_NewFloat64(c, FIX2FLT(scale.x)) );
		JS_SetPropertyStr(c, nobj, "y", JS_NewFloat64(c, FIX2FLT(scale.y)) );
		return nobj;
	}
	if (opt==1) {
		JSValue nobj = JS_NewObject(c);
		JS_SetPropertyStr(c, nobj, "x", JS_NewFloat64(c, FIX2FLT(translate.x)) );
		JS_SetPropertyStr(c, nobj, "y", JS_NewFloat64(c, FIX2FLT(translate.y)) );
		return nobj;
	}
	if (opt==2) {
		return JS_NewFloat64(c, FIX2FLT(rotate) );
	}
	return JS_EXCEPTION;
}

static JSValue mx2d_get_scale(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return mx2d_decompose_ex(c, obj, argc, argv, 0);
}
static JSValue mx2d_get_translate(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return mx2d_decompose_ex(c, obj, argc, argv, 1);
}
static JSValue mx2d_get_rotate(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return mx2d_decompose_ex(c, obj, argc, argv, 2);
}

static const JSCFunctionListEntry mx2d_funcs[] =
{
	JS_CGETSET_MAGIC_DEF("xx", mx2d_getProperty, mx2d_setProperty, MX2D_XX),
	JS_CGETSET_MAGIC_DEF("xy", mx2d_getProperty, mx2d_setProperty, MX2D_XY),
	JS_CGETSET_MAGIC_DEF("tx", mx2d_getProperty, mx2d_setProperty, MX2D_TX),
	JS_CGETSET_MAGIC_DEF("yx", mx2d_getProperty, mx2d_setProperty, MX2D_YX),
	JS_CGETSET_MAGIC_DEF("yy", mx2d_getProperty, mx2d_setProperty, MX2D_YY),
	JS_CGETSET_MAGIC_DEF("ty", mx2d_getProperty, mx2d_setProperty, MX2D_TY),
	JS_CGETSET_MAGIC_DEF("identity", mx2d_getProperty, mx2d_setProperty, MX2D_IDENTITY),
	JS_CFUNC_DEF("get_scale", 0, mx2d_get_scale),
	JS_CFUNC_DEF("get_translate", 0, mx2d_get_translate),
	JS_CFUNC_DEF("get_rotate", 0, mx2d_get_rotate),
	JS_CFUNC_DEF("inverse", 0, mx2d_inverse),
	JS_CFUNC_DEF("copy", 0, mx2d_copy),
	JS_CFUNC_DEF("add", 0, mx2d_multiply),
	JS_CFUNC_DEF("translate", 0, mx2d_translate),
	JS_CFUNC_DEF("rotate", 0, mx2d_rotate),
	JS_CFUNC_DEF("scale", 0, mx2d_scale),
	JS_CFUNC_DEF("skew", 0, mx2d_skew),
	JS_CFUNC_DEF("skew_x", 0, mx2d_skew_x),
	JS_CFUNC_DEF("skew_y", 0, mx2d_skew_y),
	JS_CFUNC_DEF("apply", 0, mx2d_apply),
};

static JSValue mx2d_constructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	JSValue obj;
	GF_Matrix2D *mx;
	GF_SAFEALLOC(mx, GF_Matrix2D);
	if (!mx) return JS_EXCEPTION;
	mx->m[MX2D_XX] = mx->m[MX2D_YY] = FIX_ONE;
	obj = JS_NewObjectClass(c, mx2d_class_id);
	JS_SetOpaque(obj, mx);
	if ((argc==1) && JS_IsObject(argv[0])) {
		GF_Matrix2D *amx = JS_GetOpaque(argv[0], mx2d_class_id);
		if (amx) gf_mx2d_copy(*mx, *amx);
	}
	else if (argc==6) {
		u32 i;
		Double d;
		for (i=0; i<6; i++) {
			if (JS_ToFloat64(c, &d, argv[i])) return JS_EXCEPTION;
			mx->m[i] = FLT2FIX(d);
		}
	}
	return obj;
}

static void colmx_finalize(JSRuntime *rt, JSValue obj)
{
	GF_ColorMatrix *mx = JS_GetOpaque(obj, colmx_class_id);
	if (!mx) return;
	gf_free(mx);
}
JSClassDef colmx_class = {
	"ColorMatrix",
	.finalizer = colmx_finalize
};

static JSValue colmx_constructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	JSValue obj;
	GF_ColorMatrix *cmx;
	GF_SAFEALLOC(cmx, GF_ColorMatrix);
	if (!cmx) return JS_EXCEPTION;
	gf_cmx_init(cmx);
	obj = JS_NewObjectClass(c, colmx_class_id);
	JS_SetOpaque(obj, cmx);
	if ((argc==1) && JS_IsObject(argv[0])) {
		GF_ColorMatrix *acmx = JS_GetOpaque(argv[0], colmx_class_id);
		if (acmx) gf_cmx_copy(cmx, acmx);
	}
	else if (argc==20) {
		u32 i;
		Double d;
		for (i=0; i<20; i++) {
			if (JS_ToFloat64(c, &d, argv[i])) return JS_EXCEPTION;
			cmx->m[i] = FLT2FIX(d);
		}
		cmx->identity = 0;
	}
	return obj;
}
enum
{
	//these 6 magics map to m[x] of the matrix, DO NOT MODIFY
	CMX_MRR = 0,
	CMX_MRG,
	CMX_MRB,
	CMX_MRA,
	CMX_TR,
	CMX_MGR,
	CMX_MGG,
	CMX_MGB,
	CMX_MGA,
	CMX_TG,
	CMX_MBR,
	CMX_MBG,
	CMX_MBB,
	CMX_MBA,
	CMX_TB,
	CMX_MAR,
	CMX_MAG,
	CMX_MAB,
	CMX_MAA,
	CMX_TA,
	CMX_IDENTITY,
};

static JSValue colmx_getProperty(JSContext *c, JSValueConst obj, int magic)
{
	GF_ColorMatrix *cmx = JS_GetOpaque(obj, colmx_class_id);
	if (!cmx) return JS_EXCEPTION;
	if ((magic>=CMX_MRR) && (magic<=CMX_TA)) {
		return JS_NewFloat64(c, FIX2FLT(cmx->m[magic]));
	}
	if (magic==CMX_IDENTITY)
		return JS_NewBool(c, cmx->identity);
	return JS_UNDEFINED;
}
static JSValue colmx_setProperty(JSContext *c, JSValueConst obj, JSValueConst value, int magic)
{
	GF_ColorMatrix *cmx = JS_GetOpaque(obj, colmx_class_id);
	if (!cmx) return JS_EXCEPTION;
	if ((magic>=CMX_MRR) && (magic<=CMX_TA)) {
		Double d;
		if (JS_ToFloat64(c, &d, value))
			return JS_EXCEPTION;
		cmx->m[magic] = FIX2FLT(d);
		cmx->identity = GF_FALSE;
		return JS_UNDEFINED;
	}
	return JS_UNDEFINED;
}

static JSValue colmx_multiply(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_ColorMatrix *cmx = JS_GetOpaque(obj, colmx_class_id);
	if (!cmx || !argc) return JS_EXCEPTION;
	GF_ColorMatrix *with = JS_GetOpaque(argv[0], colmx_class_id);
	if (!cmx) return JS_EXCEPTION;
	gf_cmx_multiply(cmx, with);
	return JS_DupValue(c, obj);
}

static Bool get_color(JSContext *c, JSValueConst obj, Double *a, Double *r, Double *g, Double *b)
{
	JSValue v;
	int res;
	v = JS_GetPropertyStr(c, obj, "r");
	res = JS_ToFloat64(c, r, v);
	JS_FreeValue(c, v);
	if (res) return GF_FALSE;
	v = JS_GetPropertyStr(c, obj, "g");
	res = JS_ToFloat64(c, g, v);
	JS_FreeValue(c, v);
	if (res) return GF_FALSE;
	v = JS_GetPropertyStr(c, obj, "b");
	res = JS_ToFloat64(c, b, v);
	JS_FreeValue(c, v);
	if (res) return GF_FALSE;
	v = JS_GetPropertyStr(c, obj, "a");
	JS_ToFloat64(c, a, v);
	JS_FreeValue(c, v);

	if (*r<0) *r=0;
	if (*g<0) *g=0;
	if (*b<0) *b=0;
	if (*a<0) *a=0;

	return GF_TRUE;
}
static JSValue colmx_apply_color(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv, Bool use_int)
{
	GF_Color col;
	JSValue nobj;
	Double r=0, g=0, b=0, a=1.0;
	GF_ColorMatrix *cmx = JS_GetOpaque(obj, colmx_class_id);
	if (!cmx || !argc) return JS_EXCEPTION;
	if (JS_IsString(argv[0])) {
		const char *str = JS_ToCString(c, argv[0]);
		col = gf_color_parse(str);
		JS_FreeCString(c, str);

		if (!use_int) {
			a = GF_COL_A(col);
			r = GF_COL_R(col);
			g = GF_COL_G(col);
			b = GF_COL_B(col);
			r/=255;
			g/=255;
			b/=255;
			a/=255;
		}

	} else if (JS_IsObject(argv[0])) {
		if (!get_color(c, argv[0], &a, &r, &g, &b))
			return JS_EXCEPTION;
		if (use_int) {
			r*=255;
			g*=255;
			b*=255;
			a*=255;
			if (a>255) a=255;
			if (r>255) r=255;
			if (g>255) g=255;
			if (b>255) b=255;
			col = GF_COL_ARGB(a, r, g, b);
		}
	} else {
		return JS_EXCEPTION;
	}
	if (use_int) {
		GF_Color res = gf_cmx_apply(cmx, col);
		nobj = JS_NewObject(c);
		JS_SetPropertyStr(c, nobj, "r", JS_NewInt32(c, GF_COL_R(res)) );
		JS_SetPropertyStr(c, nobj, "g", JS_NewInt32(c, GF_COL_G(res)) );
		JS_SetPropertyStr(c, nobj, "b", JS_NewInt32(c, GF_COL_B(res)) );
		JS_SetPropertyStr(c, nobj, "a", JS_NewInt32(c, GF_COL_A(res)) );
		return nobj;
	} else {
		Fixed fr=FLT2FIX(r), fg=FLT2FIX(g), fb=FLT2FIX(b), fa=FLT2FIX(a);

		gf_cmx_apply_fixed(cmx, &fa, &fr, &fg, &fb);
		nobj = JS_NewObject(c);
		JS_SetPropertyStr(c, nobj, "r", JS_NewFloat64(c, FIX2FLT(fr)) );
		JS_SetPropertyStr(c, nobj, "g", JS_NewFloat64(c, FIX2FLT(fg)) );
		JS_SetPropertyStr(c, nobj, "b", JS_NewFloat64(c, FIX2FLT(fb)) );
		JS_SetPropertyStr(c, nobj, "a", JS_NewFloat64(c, FIX2FLT(fa)) );
		return nobj;
	}
	return JS_EXCEPTION;
}
static JSValue colmx_apply(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return colmx_apply_color(c, obj, argc, argv, GF_TRUE);
}
static JSValue colmx_applyf(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return colmx_apply_color(c, obj, argc, argv, GF_FALSE);
}

static const JSCFunctionListEntry colmx_funcs[] =
{
	JS_CGETSET_MAGIC_DEF("rr", colmx_getProperty, colmx_setProperty, CMX_MRR),
	JS_CGETSET_MAGIC_DEF("rg", colmx_getProperty, colmx_setProperty, CMX_MRG),
	JS_CGETSET_MAGIC_DEF("rb", colmx_getProperty, colmx_setProperty, CMX_MRB),
	JS_CGETSET_MAGIC_DEF("ra", colmx_getProperty, colmx_setProperty, CMX_MRA),
	JS_CGETSET_MAGIC_DEF("tr", colmx_getProperty, colmx_setProperty, CMX_TR),
	JS_CGETSET_MAGIC_DEF("gr", colmx_getProperty, colmx_setProperty, CMX_MGR),
	JS_CGETSET_MAGIC_DEF("gg", colmx_getProperty, colmx_setProperty, CMX_MGG),
	JS_CGETSET_MAGIC_DEF("gb", colmx_getProperty, colmx_setProperty, CMX_MGB),
	JS_CGETSET_MAGIC_DEF("ga", colmx_getProperty, colmx_setProperty, CMX_MGA),
	JS_CGETSET_MAGIC_DEF("tg", colmx_getProperty, colmx_setProperty, CMX_TG),
	JS_CGETSET_MAGIC_DEF("br", colmx_getProperty, colmx_setProperty, CMX_MBR),
	JS_CGETSET_MAGIC_DEF("bg", colmx_getProperty, colmx_setProperty, CMX_MBG),
	JS_CGETSET_MAGIC_DEF("bb", colmx_getProperty, colmx_setProperty, CMX_MBB),
	JS_CGETSET_MAGIC_DEF("ba", colmx_getProperty, colmx_setProperty, CMX_MBA),
	JS_CGETSET_MAGIC_DEF("tb", colmx_getProperty, colmx_setProperty, CMX_TB),
	JS_CGETSET_MAGIC_DEF("ar", colmx_getProperty, colmx_setProperty, CMX_MAR),
	JS_CGETSET_MAGIC_DEF("ag", colmx_getProperty, colmx_setProperty, CMX_MAG),
	JS_CGETSET_MAGIC_DEF("ab", colmx_getProperty, colmx_setProperty, CMX_MAB),
	JS_CGETSET_MAGIC_DEF("aa", colmx_getProperty, colmx_setProperty, CMX_MAA),
	JS_CGETSET_MAGIC_DEF("ta", colmx_getProperty, colmx_setProperty, CMX_TA),
	JS_CGETSET_MAGIC_DEF("identity", colmx_getProperty, NULL, CMX_IDENTITY),

	JS_CFUNC_DEF("multiply", 0, colmx_multiply),
	JS_CFUNC_DEF("apply", 0, colmx_apply),
	JS_CFUNC_DEF("applyf", 0, colmx_applyf),
};



static void path_finalize(JSRuntime *rt, JSValue obj)
{
	GF_Path *path = JS_GetOpaque(obj, path_class_id);
	if (!path) return;
	gf_path_del(path);
}
JSClassDef path_class = {
	"Path",
	.finalizer = path_finalize
};

static JSValue path_close_reset(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv, u32 opt)
{
	GF_Path *gp = JS_GetOpaque(obj, path_class_id);
	if (!gp || (argc<2)) return JS_EXCEPTION;
	if (opt==0) {
		gf_path_close(gp);
		return JS_DupValue(c, obj);
	}
	if (opt==1) {
		gf_path_reset(gp);
		return JS_DupValue(c, obj);
	}
	if (opt==2) {
		JSValue nobj = JS_NewObjectClass(c, path_class_id);
		if (JS_IsException(nobj)) return nobj;
		JS_SetOpaque(nobj, gf_path_clone(gp));
		gf_path_reset(gp);
		return nobj;
	}
	return JS_EXCEPTION;
}
static JSValue path_reset(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return path_close_reset(c, obj, argc, argv, 1);
}
static JSValue path_close(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return path_close_reset(c, obj, argc, argv, 0);
}
static JSValue path_clone(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return path_close_reset(c, obj, argc, argv, 2);
}
static JSValue path_line_move(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv, Bool is_line)
{
	Double x=0, y=0;
	GF_Err e;
	GF_Path *gp = JS_GetOpaque(obj, path_class_id);
	if (!gp || (argc<2)) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &x, argv[0])) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &y, argv[1])) return JS_EXCEPTION;
	if (is_line)
		e = gf_path_add_line_to(gp, FLT2FIX(x), FLT2FIX(y));
	else
		e = gf_path_add_move_to(gp, FLT2FIX(x), FLT2FIX(y));
	if (e) return JS_EXCEPTION;
	return JS_DupValue(c, obj);
}
static JSValue path_line_to(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return path_line_move(c, obj, argc, argv, GF_TRUE);
}
static JSValue path_move_to(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return path_line_move(c, obj, argc, argv, GF_FALSE);
}
static JSValue path_cubic_to(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	Double c1_x=0, c1_y=0, c2_x=0, c2_y=0, x=0, y=0;
	GF_Err e;
	GF_Path *gp = JS_GetOpaque(obj, path_class_id);
	if (!gp || (argc<6)) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &c1_x, argv[0])) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &c1_y, argv[1])) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &c2_x, argv[2])) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &c2_y, argv[3])) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &x, argv[4])) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &y, argv[5])) return JS_EXCEPTION;
	e = gf_path_add_cubic_to(gp, FLT2FIX(c1_x), FLT2FIX(c1_y), FLT2FIX(c2_x), FLT2FIX(c2_y), FLT2FIX(x), FLT2FIX(y));
	if (e) return JS_EXCEPTION;
	return JS_DupValue(c, obj);
}
static JSValue path_quadratic_to(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	Double c_x=0, c_y=0, x=0, y=0;
	GF_Err e;
	GF_Path *gp = JS_GetOpaque(obj, path_class_id);
	if (!gp || (argc<4)) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &c_x, argv[0])) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &c_y, argv[1])) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &x, argv[2])) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &y, argv[3])) return JS_EXCEPTION;
	e = gf_path_add_quadratic_to(gp, FLT2FIX(c_x), FLT2FIX(c_y), FLT2FIX(x), FLT2FIX(y));
	if (e) return JS_EXCEPTION;
	return JS_DupValue(c, obj);
}

static JSValue path_rect(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	Double ox=0, oy=0, w=0, h=0;
	u32 idx=0;
	GF_Err e;

	GF_Path *gp = JS_GetOpaque(obj, path_class_id);
	if (!gp || (argc<3)) return JS_EXCEPTION;

	if (JS_IsObject(argv[0])) {
		JSValue v;
#define GETD(_arg, _name, _res)\
		if (! JS_IsObject(_arg)) return JS_EXCEPTION;\
		v = JS_GetPropertyStr(c, _arg, _name);\
		JS_ToFloat64(c, &_res, v);\
		JS_FreeValue(c, v);\

		GETD(argv[0], "x", ox);
		GETD(argv[0], "y", oy);
#undef GETD

		idx=1;
	} else if (argc>=4) {
		if (JS_ToFloat64(c, &ox, argv[0])) return JS_EXCEPTION;
		if (JS_ToFloat64(c, &oy, argv[1])) return JS_EXCEPTION;
		idx=2;
	} else {
		return JS_EXCEPTION;
	}
	if (JS_ToFloat64(c, &w, argv[idx])) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &h, argv[idx+1])) return JS_EXCEPTION;
	if ((argc>idx+2) && JS_ToBool(c, argv[idx+2])) {
		e = gf_path_add_rect_center(gp, FLT2FIX(ox), FLT2FIX(oy), FLT2FIX(w), FLT2FIX(h));
	} else {
		e = gf_path_add_rect(gp, FLT2FIX(ox), FLT2FIX(oy), FLT2FIX(w), FLT2FIX(h));
	}
	if (e) return JS_EXCEPTION;
	return JS_DupValue(c, obj);
}

static JSValue path_ellipse(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	Double cx=0, cy=0, a_axis=0, b_axis=0;
	GF_Err e;
	u32 idx=0;
	GF_Path *gp = JS_GetOpaque(obj, path_class_id);
	if (!gp) return JS_EXCEPTION;
	if (argc==3) {
		JSValue v;
#define GETD(_arg, _name, _res)\
		if (! JS_IsObject(_arg)) return JS_EXCEPTION;\
		v = JS_GetPropertyStr(c, _arg, _name);\
		JS_ToFloat64(c, &_res, v);\
		JS_FreeValue(c, v);\

		GETD(argv[0], "x", cx);
		GETD(argv[0], "y", cy);
#undef GETD

		idx=1;
	} else if (argc==4) {
		if (JS_ToFloat64(c, &cx, argv[0])) return JS_EXCEPTION;
		if (JS_ToFloat64(c, &cy, argv[1])) return JS_EXCEPTION;
		idx=2;
	} else {
		return JS_EXCEPTION;
	}
	if (JS_ToFloat64(c, &a_axis, argv[idx])) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &b_axis, argv[idx+1])) return JS_EXCEPTION;
	e = gf_path_add_ellipse(gp, FLT2FIX(cx), FLT2FIX(cy), FLT2FIX(a_axis), FLT2FIX(b_axis));
	if (e) return JS_EXCEPTION;
	return JS_DupValue(c, obj);
}

static JSValue path_bezier(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_Err e;
	u32 i;
	GF_Point2D *pts;
	GF_Path *gp = JS_GetOpaque(obj, path_class_id);
	if (!gp || (argc<3)) return JS_EXCEPTION;
	pts = gf_malloc(sizeof(GF_Point2D)* argc);
	memset(pts, 0, sizeof(GF_Point2D)* argc);
	for (i=0; i<argc; i++) {
		JSValue v;
		Double d;
		if (! JS_IsObject(argv[i]))
			continue;
		v = JS_GetPropertyStr(c, argv[i], "x");
		JS_ToFloat64(c, &d, v);
		pts[i].x = FLT2FIX(d);
		JS_FreeValue(c, v);
		v = JS_GetPropertyStr(c, argv[i], "y");
		JS_ToFloat64(c, &d, v);
		pts[i].x = FLT2FIX(d);
		JS_FreeValue(c, v);
	}
	e = gf_path_add_bezier(gp, pts, argc);
	gf_free(pts);
	if (e) return JS_EXCEPTION;
	return JS_DupValue(c, obj);
}

static JSValue path_arc_bifs(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	Double end_x=0, end_y=0, fa_x=0, fa_y=0, fb_x=0, fb_y=0;
	Bool cw_flag = GF_FALSE;
	GF_Err e;
	GF_Path *gp = JS_GetOpaque(obj, path_class_id);
	if (!gp || (argc<6)) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &end_x, argv[0])) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &end_y, argv[1])) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &fa_x, argv[2])) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &fa_y, argv[3])) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &fb_x, argv[4])) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &fb_y, argv[5])) return JS_EXCEPTION;
	if (argc>6) cw_flag = JS_ToBool(c, argv[6]);

	e = gf_path_add_svg_arc_to(gp, FLT2FIX(end_x), FLT2FIX(end_y), FLT2FIX(fa_x), FLT2FIX(fa_y), FLT2FIX(fb_x), FLT2FIX(fb_y),  cw_flag);
	if (e) return JS_EXCEPTION;
	return JS_DupValue(c, obj);
}


static JSValue path_arc_svg(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	Double end_x=0, end_y=0, r_x=0, r_y=0, x_axis_rotation=0;
	Bool large_arc_flag=GF_FALSE, sweep_flag=GF_FALSE;
	GF_Err e;
	GF_Path *gp = JS_GetOpaque(obj, path_class_id);
	if (!gp || (argc<4)) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &end_x, argv[0])) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &end_y, argv[1])) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &r_x, argv[2])) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &r_y, argv[3])) return JS_EXCEPTION;
	if (argc>4) {
		if (JS_ToFloat64(c, &x_axis_rotation, argv[4])) return JS_EXCEPTION;
		if (argc>5) large_arc_flag = JS_ToBool(c, argv[5]);
		if (argc>6) sweep_flag = JS_ToBool(c, argv[6]);
	}
	e = gf_path_add_svg_arc_to(gp, FLT2FIX(end_x), FLT2FIX(end_y), FLT2FIX(r_x), FLT2FIX(r_y), FLT2FIX(x_axis_rotation), large_arc_flag, sweep_flag);

	if (e) return JS_EXCEPTION;
	return JS_DupValue(c, obj);
}

static JSValue path_arc(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	Double radius=0, start=0, end=0;
	u32 close=0;
	GF_Err e;
	GF_Path *gp = JS_GetOpaque(obj, path_class_id);
	if (!gp || (argc<3)) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &radius, argv[0])) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &start, argv[1])) return JS_EXCEPTION;
	if (JS_ToFloat64(c, &end, argv[2])) return JS_EXCEPTION;
	if (argc>3)
		if (JS_ToInt32(c, &close, argv[3])) return JS_EXCEPTION;
	e = gf_path_add_arc(gp, FLT2FIX(radius), FLT2FIX(start), FLT2FIX(end), close);
	if (e) return JS_EXCEPTION;
	return JS_DupValue(c, obj);
}

static JSValue path_subpath(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_Err e;
	GF_Matrix2D *mx=NULL;
	GF_Path *gp = JS_GetOpaque(obj, path_class_id);
	if (!gp || !argc) return JS_EXCEPTION;
	GF_Path *subgp = JS_GetOpaque(argv[0], path_class_id);
	if (!gp) return JS_EXCEPTION;
	if (argc>1)
		mx = JS_GetOpaque(argv[1], mx2d_class_id);
	e = gf_path_add_subpath(gp, subgp, mx);
	if (e) return JS_EXCEPTION;
	return JS_DupValue(c, obj);
}

static JSValue path_flatten(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_Path *gp = JS_GetOpaque(obj, path_class_id);
	if (!gp) return JS_EXCEPTION;
	gf_path_flatten(gp);
	return JS_DupValue(c, obj);
}
static JSValue path_get_flat(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	JSValue nobj;
	GF_Path *gp_flat;
	GF_Path *gp = JS_GetOpaque(obj, path_class_id);
	if (!gp) return JS_EXCEPTION;
	gp_flat = gf_path_get_flatten(gp);
	if (!gp) return JS_NULL;
	nobj = JS_NewObjectClass(c, path_class_id);
	if (JS_IsException(nobj)) return nobj;
	JS_SetOpaque(nobj, gp_flat);
	return nobj;
}

static JSValue path_point_over(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	Double x, y;
	GF_Path *gp = JS_GetOpaque(obj, path_class_id);
	if (!gp || !argc) return JS_EXCEPTION;
	if (argc==1) {
		Double d;
		int res;
		JSValue v;
		if (!JS_IsObject(argv[0])) return JS_EXCEPTION;
#define GETIT(_arg, _name, _var) \
		v = JS_GetPropertyStr(c, _arg, _name);\
		res = JS_ToFloat64(c, &d, v);\
		JS_FreeValue(c, v);\
		if (res) return JS_EXCEPTION;\
		_var = FLT2FIX(d);\

		GETIT(argv[0], "x", x)
		GETIT(argv[0], "y", y)
#undef GETIT
	} else if (argc==2) {
		if (JS_ToFloat64(c, &x, argv[0])) return JS_EXCEPTION;
		if (JS_ToFloat64(c, &y, argv[0])) return JS_EXCEPTION;
	} else {
		return JS_EXCEPTION;
	}
	return JS_NewBool(c, gf_path_point_over(gp, FLT2FIX(x), FLT2FIX(y)));
}


static JSValue path_outline(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_Path *outline;
	GF_PenSettings pen;
	GF_DashSettings dash;
	JSValue v, dashes;
	Double d;
	int i, res;
	GF_Path *gp = JS_GetOpaque(obj, path_class_id);
	if (!gp || !argc || !JS_IsObject(argv[0]) ) return JS_EXCEPTION;
	memset(&pen, 0, sizeof(GF_PenSettings));

#define GETIT_F(_arg, _name, _var) \
		v = JS_GetPropertyStr(c, _arg, _name);\
		if (!JS_IsUndefined(v)) {\
			res = JS_ToFloat64(c, &d, v);\
			JS_FreeValue(c, v);\
			if (res) return JS_EXCEPTION;\
			_var = FLT2FIX(d);\
		}\

#define GETIT_I(_arg, _name, _var) \
		v = JS_GetPropertyStr(c, _arg, _name);\
		if (!JS_IsUndefined(v)) {\
			res = JS_ToInt32(c, &i, v);\
			JS_FreeValue(c, v);\
			if (res) return JS_EXCEPTION;\
			_var = i;\
		}\

	GETIT_F(argv[0], "width", pen.width);
	GETIT_F(argv[0], "miter", pen.miterLimit);
	GETIT_F(argv[0], "offset", pen.dash_offset);
	GETIT_F(argv[0], "length", pen.path_length);
	GETIT_I(argv[0], "cap", pen.cap);
	GETIT_I(argv[0], "join", pen.join);
	GETIT_I(argv[0], "align", pen.align);
	GETIT_I(argv[0], "dash", pen.dash);


#undef GETTIT_F
#undef GETTIT_I

	dash.num_dash = 0;
	dash.dashes = NULL;
	dashes = JS_GetPropertyStr(c, argv[0], "dashes");
	if (JS_IsArray(c, dashes) && !JS_IsNull(v)) {
		u32 i;
		v = JS_GetPropertyStr(c, dashes, "length");
		JS_ToInt32(c, &dash.num_dash, v);
		JS_FreeValue(c, v);
		pen.dash_set = &dash;
		dash.dashes = gf_malloc(sizeof(Fixed)*dash.num_dash);
		for (i=0; i<dash.num_dash; i++) {
			v = JS_GetPropertyUint32(c, dashes, i);
			JS_ToFloat64(c, &d, v);
			dash.dashes[i] = FLT2FIX(d);
			JS_FreeValue(c, v);
		}
	}
	JS_FreeValue(c, dashes);

	outline = gf_path_get_outline(gp, pen);
	if (dash.dashes) gf_free(dash.dashes);

	if (!outline) return JS_EXCEPTION;
	v = JS_NewObjectClass(c, path_class_id);
	JS_SetOpaque(v, outline);
	return v;
}

enum
{
	PATH_EMPTY=0,
	PATH_ZERO_NONZERO,
	PATH_BOUNDS,
	PATH_CONTROL_BOUNDS,
};


static JSValue path_bounds_ex(JSContext *c, GF_Path *gp, Bool is_ctrl)
{
	JSValue nobj;
	GF_Err e;
	GF_Rect rc;
	if (is_ctrl)
	 	e = gf_path_get_control_bounds(gp, &rc);
	else
		e = gf_path_get_bounds(gp, &rc);
	if (e) return JS_EXCEPTION;
	nobj = JS_NewObject(c);
	JS_SetPropertyStr(c, nobj, "x", JS_NewFloat64(c, rc.x));
	JS_SetPropertyStr(c, nobj, "y", JS_NewFloat64(c, rc.y));
	JS_SetPropertyStr(c, nobj, "w", JS_NewFloat64(c, rc.width));
	JS_SetPropertyStr(c, nobj, "h", JS_NewFloat64(c, rc.height));
	return nobj;
}
static JSValue path_getProperty(JSContext *c, JSValueConst obj, int magic)
{
	GF_Path *gp = JS_GetOpaque(obj, path_class_id);
	if (!gp) return JS_EXCEPTION;
	switch (magic) {
	case PATH_EMPTY: return JS_NewBool(c, gf_path_is_empty(gp));
	case PATH_ZERO_NONZERO: return JS_NewBool(c, gp->flags & GF_PATH_FILL_ZERO_NONZERO);
	case PATH_BOUNDS: return path_bounds_ex(c, gp, GF_FALSE);
	case PATH_CONTROL_BOUNDS: return path_bounds_ex(c, gp, GF_TRUE);
	}
	return JS_UNDEFINED;
}
static JSValue path_setProperty(JSContext *c, JSValueConst obj, JSValueConst value, int magic)
{
	GF_Path *gp = JS_GetOpaque(obj, path_class_id);
	if (!gp) return JS_EXCEPTION;
	switch (magic) {
	case PATH_ZERO_NONZERO:
	 	if (JS_ToBool(c, value))
	 		gp->flags |= GF_PATH_FILL_ZERO_NONZERO;
		else
	 		gp->flags &= ~GF_PATH_FILL_ZERO_NONZERO;
	}
	return JS_UNDEFINED;
}
static JSValue path_constructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	JSValue obj;
	GF_Path *path = gf_path_new();
	if (!path) return JS_EXCEPTION;
	obj = JS_NewObjectClass(c, path_class_id);
	if (JS_IsException(obj)) return obj;
	JS_SetOpaque(obj, path);
	return obj;
}
static const JSCFunctionListEntry path_funcs[] =
{
	JS_CGETSET_MAGIC_DEF("empty", path_getProperty, NULL, PATH_EMPTY),
	JS_CGETSET_MAGIC_DEF("zero_fill", path_getProperty, path_setProperty, PATH_ZERO_NONZERO),
	JS_CGETSET_MAGIC_DEF("bounds", path_getProperty, NULL, PATH_BOUNDS),
	JS_CGETSET_MAGIC_DEF("ctrl_bounds", path_getProperty, NULL, PATH_CONTROL_BOUNDS),

	JS_CFUNC_DEF("point_over", 0, path_point_over),
	JS_CFUNC_DEF("get_flatten", 0, path_get_flat),
	JS_CFUNC_DEF("flatten", 0, path_flatten),
	JS_CFUNC_DEF("add_path", 0, path_subpath),
	JS_CFUNC_DEF("arc", 0, path_arc),
	JS_CFUNC_DEF("arc_svg", 0, path_arc_svg),
	JS_CFUNC_DEF("arc_bifs", 0, path_arc_bifs),
	JS_CFUNC_DEF("bezier", 0, path_bezier),
	JS_CFUNC_DEF("ellipse", 0, path_ellipse),
	JS_CFUNC_DEF("rectangle", 0, path_rect),
	JS_CFUNC_DEF("quadratic_to", 0, path_quadratic_to),
	JS_CFUNC_DEF("cubic_to", 0, path_cubic_to),
	JS_CFUNC_DEF("line_to", 0, path_line_to),
	JS_CFUNC_DEF("move_to", 0, path_move_to),
	JS_CFUNC_DEF("clone", 0, path_clone),
	JS_CFUNC_DEF("reset", 0, path_reset),
	JS_CFUNC_DEF("close", 0, path_close),
	JS_CFUNC_DEF("outline", 0, path_outline),

};


static void stencil_finalize(JSRuntime *rt, JSValue obj)
{
	GF_EVGStencil *stencil = JS_GetOpaque(obj, stencil_class_id);
	if (!stencil) return;
	gf_evg_stencil_delete(stencil);
}
JSClassDef stencil_class = {
	"Stencil",
	.finalizer = stencil_finalize
};

enum
{
	STENCIL_CMX,
	STENCIL_MAT,
	STENCIL_GRADMOD,
};

static JSValue stencil_set_linear(JSContext *c, GF_EVGStencil *stencil, int argc, JSValueConst *argv)
{
	int res;
	JSValue v;
	Double d;
	Fixed start_x=0, start_y=0, end_x=0, end_y=0;
	u32 idx=0;
	if (argc<2) return JS_EXCEPTION;

#define GETIT(_arg, _name, _var) \
		v = JS_GetPropertyStr(c, _arg, _name);\
		res = JS_ToFloat64(c, &d, v);\
		JS_FreeValue(c, v);\
		if (res) return JS_EXCEPTION;\
		_var = FLT2FIX(d);\

	if (JS_IsObject(argv[0])) {
		GETIT(argv[0], "x", start_x)
		GETIT(argv[0], "y", start_y)
		idx=1;
	} else {
		if (JS_ToFloat64(c, &d, argv[0])) return JS_EXCEPTION;
		start_x = FLT2FIX(d);
		if (JS_ToFloat64(c, &d, argv[1])) return JS_EXCEPTION;
		start_y = FLT2FIX(d);
		idx=2;
	}
	if (argc<=idx) {
		end_x = start_x;
		end_y = start_y;
		start_x = 0;
		start_y = 0;
	} else if (JS_IsObject(argv[idx])) {
		GETIT(argv[idx], "x", end_x)
		GETIT(argv[idx], "y", end_y)
		idx=1;
	} else if (argc>idx+1) {
		if (JS_ToFloat64(c, &d, argv[idx])) return JS_EXCEPTION;
		end_x = FLT2FIX(d);
		if (JS_ToFloat64(c, &d, argv[idx+1])) return JS_EXCEPTION;
		end_y = FLT2FIX(d);
	}
#undef GETIT
	gf_evg_stencil_set_linear_gradient(stencil, start_x, start_y, end_x, end_y);
	return JS_UNDEFINED;
}

static JSValue stencil_set_radial(JSContext *c, GF_EVGStencil *stencil, int argc, JSValueConst *argv)
{
	int res;
	JSValue v;
	Double d;
	Fixed cx=0, cy=0, fx=0, fy=0, rx=0, ry=0;
	u32 idx=0;
	if (argc<3) return JS_EXCEPTION;

#define GETIT(_arg, _name, _var) \
		v = JS_GetPropertyStr(c, _arg, _name);\
		res = JS_ToFloat64(c, &d, v);\
		JS_FreeValue(c, v);\
		if (res) return JS_EXCEPTION;\
		_var = FLT2FIX(d);\

	if (JS_IsObject(argv[0])) {
		GETIT(argv[0], "x", cx)
		GETIT(argv[0], "y", cy)
		idx+=1;
	} else {
		if (JS_ToFloat64(c, &d, argv[0])) return JS_EXCEPTION;
		cx = FLT2FIX(d);
		if (JS_ToFloat64(c, &d, argv[1])) return JS_EXCEPTION;
		cy = FLT2FIX(d);
		idx+=2;
	}

	if (JS_IsObject(argv[idx])) {
		GETIT(argv[idx], "x", fx)
		GETIT(argv[idx], "y", fy)
		idx+=1;
	} else if (argc>idx+1) {
		if (JS_ToFloat64(c, &d, argv[idx])) return JS_EXCEPTION;
		fx = FLT2FIX(d);
		if (JS_ToFloat64(c, &d, argv[idx+1])) return JS_EXCEPTION;
		fy = FLT2FIX(d);
		idx+=2;
	}

	if (JS_IsObject(argv[idx])) {
		GETIT(argv[idx], "x", rx)
		GETIT(argv[idx], "y", ry)
		idx+=1;
	} else if (argc>idx+1) {
		if (JS_ToFloat64(c, &d, argv[idx])) return JS_EXCEPTION;
		rx = FLT2FIX(d);
		if (JS_ToFloat64(c, &d, argv[idx+1])) return JS_EXCEPTION;
		ry = FLT2FIX(d);
		idx+=2;
	}
#undef GETIT
	gf_evg_stencil_set_radial_gradient(stencil, cx, cy, fx, fy, rx, ry);
	return JS_UNDEFINED;
}


static JSValue stencil_set_points(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_StencilType type;
	GF_EVGStencil *stencil = JS_GetOpaque(obj, stencil_class_id);
	if (!stencil) return JS_EXCEPTION;

	type = gf_evg_stencil_type(stencil);
	if (type==GF_STENCIL_LINEAR_GRADIENT)
		return stencil_set_linear(c, stencil, argc, argv);
	if (type==GF_STENCIL_RADIAL_GRADIENT)
		return stencil_set_radial(c, stencil, argc, argv);
	return JS_EXCEPTION;
}

Bool get_color_from_args(JSContext *c, int argc, JSValueConst *argv, u32 idx, Double *a, Double *r, Double *g, Double *b)
{
	if (argc<idx) return GF_FALSE;
	if (JS_IsString(argv[idx])) {
		GF_Color col;
		const char *str = JS_ToCString(c, argv[idx]);
		col = gf_color_parse(str);
		JS_FreeCString(c, str);
	} else if (JS_IsObject(argv[idx])) {
		if (!get_color(c, argv[idx], a, r, g, b)) {
			return GF_FALSE;
		}
	} else if (argc>idx) {
		if (JS_ToFloat64(c, r, argv[idx]))
			return GF_FALSE;
		if (argc>idx+1) {
			if (JS_ToFloat64(c, g, argv[idx+1]))
				return GF_FALSE;
			if (argc>idx+2) {
				if (JS_ToFloat64(c, b, argv[idx+2]))
					return GF_FALSE;
				if (argc>idx+3) {
					if (JS_ToFloat64(c, a, argv[idx+3]))
						return GF_FALSE;
				}
			}
		}
	}
	return GF_TRUE;
}

static JSValue stencil_set_stop_ex(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv, Bool use_int)
{
	Double pos=0;
	GF_StencilType type;
	GF_Color col;
	GF_EVGStencil *stencil = JS_GetOpaque(obj, stencil_class_id);
	if (!stencil) return JS_EXCEPTION;
	type = gf_evg_stencil_type(stencil);
	if ((type!=GF_STENCIL_LINEAR_GRADIENT) && (type!=GF_STENCIL_RADIAL_GRADIENT)) return JS_EXCEPTION;
	if (!argc) {
		gf_evg_stencil_set_gradient_interpolation(stencil, NULL, NULL, 0);
		return JS_UNDEFINED;
	}
	if (JS_ToFloat64(c, &pos, argv[0])) return JS_EXCEPTION;

	if (JS_IsString(argv[1])) {
		const char *str = JS_ToCString(c, argv[1]);
		col = gf_color_parse(str);
		JS_FreeCString(c, str);
	} else {
		Double r=0, g=0, b=0, a=1.0;
		if (!get_color_from_args(c, argc, argv, 1, &a, &r, &g, &b)) {
			return JS_EXCEPTION;
		}
		if (!use_int) {
			a *= 255;
			r *= 255;
			g *= 255;
			b *= 255;
		}
		col = GF_COL_ARGB(a, r, g, b);
	}
	gf_evg_stencil_push_gradient_interpolation(stencil, FLT2FIX(pos), col);
	return JS_UNDEFINED;
}
static JSValue stencil_set_stop(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return stencil_set_stop_ex(c, obj, argc, argv, GF_TRUE);
}
static JSValue stencil_set_stopf(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return stencil_set_stop_ex(c, obj, argc, argv, GF_FALSE);
}

static JSValue stencil_set_color_ex(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv, Bool use_int)
{
	Double r=0, g=0, b=0, a=1.0;
	GF_StencilType type;
	GF_EVGStencil *stencil = JS_GetOpaque(obj, stencil_class_id);
	if (!stencil) return JS_EXCEPTION;
	type = gf_evg_stencil_type(stencil);
	if (type!=GF_STENCIL_SOLID) return JS_EXCEPTION;

	if (JS_IsString(argv[0])) {
		GF_Color col;
		const char *str = JS_ToCString(c, argv[0]);
		col = gf_color_parse(str);
		JS_FreeCString(c, str);
		gf_evg_stencil_set_brush_color(stencil, col);
		return JS_UNDEFINED;
	} else if (!get_color_from_args(c, argc, argv, 0, &a, &r, &g, &b)) {
		return JS_EXCEPTION;
	}
	if (!use_int) {
		r*=255;
		g*=255;
		b*=255;
		a*=255;
	}
	gf_evg_stencil_set_brush_color(stencil, GF_COL_ARGB(a, r, g, b));
	return JS_UNDEFINED;
}

static JSValue stencil_set_color(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return stencil_set_color_ex(c, obj, argc, argv, GF_TRUE);
}
static JSValue stencil_set_colorf(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return stencil_set_color_ex(c, obj, argc, argv, GF_FALSE);
}

static JSValue stencil_set_alpha_ex(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv, Bool use_int)
{
	Double a=1.0;
	GF_EVGStencil *stencil = JS_GetOpaque(obj, stencil_class_id);
	if (!stencil) {
		GF_JSTexture *tx = JS_GetOpaque(obj, texture_class_id);
		if (!tx || !tx->stencil)
			return JS_EXCEPTION;
		stencil = tx->stencil;
	}

	if (argc) {
		JS_ToFloat64(c, &a, argv[0]);
	}
	if (a<0) a=0;
	if (!use_int) {
		a*=255;
	}
	if (a>255) a = 255;
	gf_evg_stencil_set_alpha(stencil, a);
	return JS_UNDEFINED;
}

static JSValue stencil_set_alpha(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return stencil_set_alpha_ex(c, obj, argc, argv, GF_TRUE);
}
static JSValue stencil_set_alphaf(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return stencil_set_alpha_ex(c, obj, argc, argv, GF_FALSE);
}

static JSValue stencil_setProperty(JSContext *c, JSValueConst obj, JSValueConst value, int magic)
{
	u32 v;
	GF_EVGStencil *stencil = JS_GetOpaque(obj, stencil_class_id);
	if (!stencil) return JS_EXCEPTION;

	switch (magic) {
	case STENCIL_CMX:
		if (JS_IsNull(value)) {
			gf_evg_stencil_set_color_matrix(stencil, NULL);
		} else {
			GF_ColorMatrix *cmx = JS_GetOpaque(value, colmx_class_id);
			gf_evg_stencil_set_color_matrix(stencil, cmx);
		}
		return JS_UNDEFINED;
	case STENCIL_GRADMOD:
		if (JS_ToInt32(c, &v, value)) return JS_EXCEPTION;
		gf_evg_stencil_set_gradient_mode(stencil, v);
		return JS_UNDEFINED;
	case STENCIL_MAT:
		if (JS_IsNull(value)) {
			gf_evg_stencil_set_matrix(stencil, NULL);
		} else {
			GF_Matrix2D *mx = JS_GetOpaque(value, mx2d_class_id);
			gf_evg_stencil_set_matrix(stencil, mx);
		}
		return JS_UNDEFINED;
	}
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry stencil_funcs[] =
{
	JS_CGETSET_MAGIC_DEF("pad", NULL, stencil_setProperty, STENCIL_GRADMOD),
	JS_CGETSET_MAGIC_DEF("cmx", NULL, stencil_setProperty, STENCIL_CMX),
	JS_CGETSET_MAGIC_DEF("mx", NULL, stencil_setProperty, STENCIL_MAT),
	JS_CFUNC_DEF("set_color", 0, stencil_set_color),
	JS_CFUNC_DEF("set_colorf", 0, stencil_set_colorf),
	JS_CFUNC_DEF("set_alpha", 0, stencil_set_alpha),
	JS_CFUNC_DEF("set_alphaf", 0, stencil_set_alphaf),
	JS_CFUNC_DEF("set_points", 0, stencil_set_points),
	JS_CFUNC_DEF("set_stop", 0, stencil_set_stop),
	JS_CFUNC_DEF("set_stopf", 0, stencil_set_stopf),
};


static JSValue stencil_constructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv, GF_StencilType type)
{
	JSValue obj;
	GF_EVGStencil *stencil = gf_evg_stencil_new(type);
	if (!stencil) return JS_EXCEPTION;
	obj = JS_NewObjectClass(c, stencil_class_id);
	if (JS_IsException(obj)) return obj;
	JS_SetOpaque(obj, stencil);
	return obj;
}
static JSValue solid_brush_constructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	return stencil_constructor(c, new_target, argc, argv, GF_STENCIL_SOLID);
}
static JSValue linear_gradient_constructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	return stencil_constructor(c, new_target, argc, argv, GF_STENCIL_LINEAR_GRADIENT);
}
static JSValue radial_gradient_constructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	return stencil_constructor(c, new_target, argc, argv, GF_STENCIL_RADIAL_GRADIENT);
}

static void texture_reset(GF_JSTexture *tx)
{
	if (tx->owns_data && tx->data) {
		gf_free(tx->data);
	}
	tx->data = NULL;
	tx->data_size = 0;
	tx->owns_data = GF_FALSE;
}

static void texture_finalize(JSRuntime *rt, JSValue obj)
{
	GF_JSTexture *tx = JS_GetOpaque(obj, texture_class_id);
	if (!tx) return;
	texture_reset(tx);
	if (tx->stencil)
		gf_evg_stencil_delete(tx->stencil);

	gf_free(tx);
}

JSClassDef texture_class = {
	"Texture",
	.finalizer = texture_finalize
};

enum
{
	TX_MAPPING = 0,
	TX_FILTER,
	TX_CMX,
	TX_REPEAT_S,
	TX_REPEAT_T,
	TX_FLIP,
	TX_MAT,
	TX_WIDTH,
	TX_HEIGHT,
	TX_NB_COMP,
	TX_PIXFMT,
	TX_DATA,
};

static JSValue texture_getProperty(JSContext *c, JSValueConst obj, int magic)
{
	GF_JSTexture *tx = JS_GetOpaque(obj, texture_class_id);
	if (!tx || !tx->stencil) return JS_EXCEPTION;
	switch (magic) {
	case TX_REPEAT_S:
		return JS_NewBool(c, (tx->flags & GF_TEXTURE_REPEAT_S));
	case TX_REPEAT_T:
		return JS_NewBool(c, (tx->flags & GF_TEXTURE_REPEAT_T));
	case TX_FLIP:
		return JS_NewBool(c, (tx->flags & GF_TEXTURE_FLIP));
	case TX_WIDTH:
		return JS_NewInt32(c, tx->width);
	case TX_HEIGHT:
		return JS_NewInt32(c, tx->height);
	case TX_PIXFMT:
		return JS_NewInt32(c, tx->pf);
	case TX_NB_COMP:
		return JS_NewInt32(c, tx->nb_comp);
	case TX_DATA:
		if (tx->owns_data)
			return JS_NewArrayBuffer(c, (u8 *) tx->data, tx->data_size, NULL, NULL, GF_TRUE);
		return JS_NULL;
	}
	return JS_UNDEFINED;
}
static JSValue texture_setProperty(JSContext *c, JSValueConst obj, JSValueConst value, int magic)
{
	u32 v;
	GF_JSTexture *tx = JS_GetOpaque(obj, texture_class_id);
	if (!tx || !tx->stencil) return JS_EXCEPTION;

	switch (magic) {
	case TX_FILTER:
		if (JS_ToInt32(c, &v, value)) return JS_EXCEPTION;
		gf_evg_stencil_set_filter(tx->stencil, v);
		return JS_UNDEFINED;
	case TX_CMX:
		if (JS_IsNull(value)) {
			gf_evg_stencil_set_color_matrix(tx->stencil, NULL);
		} else {
			GF_ColorMatrix *cmx = JS_GetOpaque(value, colmx_class_id);
			gf_evg_stencil_set_color_matrix(tx->stencil, cmx);
		}
		return JS_UNDEFINED;
	case TX_REPEAT_S:
		if (JS_ToBool(c, value)) tx->flags |= GF_TEXTURE_REPEAT_S;
		else tx->flags &= ~GF_TEXTURE_REPEAT_S;
		gf_evg_stencil_set_mapping(tx->stencil, tx->flags);
		return JS_UNDEFINED;
	case TX_REPEAT_T:
		if (JS_ToBool(c, value)) tx->flags |= GF_TEXTURE_REPEAT_T;
		else tx->flags &= ~GF_TEXTURE_REPEAT_T;
		gf_evg_stencil_set_mapping(tx->stencil, tx->flags);
		return JS_UNDEFINED;
	case TX_FLIP:
		if (JS_ToBool(c, value)) tx->flags |= GF_TEXTURE_FLIP;
		else tx->flags &= ~GF_TEXTURE_FLIP;
		gf_evg_stencil_set_mapping(tx->stencil, tx->flags);
		return JS_UNDEFINED;
	case TX_MAT:
		if (JS_IsNull(value)) {
			gf_evg_stencil_set_matrix(tx->stencil, NULL);
		} else {
			GF_Matrix2D *mx = JS_GetOpaque(value, mx2d_class_id);
			gf_evg_stencil_set_matrix(tx->stencil, mx);
		}
		return JS_UNDEFINED;
	}
	return JS_UNDEFINED;
}


#define min_f(a, b, c)  (fminf(a, fminf(b, c)))
#define max_f(a, b, c)  (fmaxf(a, fmaxf(b, c)))

void rgb2hsv(const u8 src_r, const u8 src_g, const u8 src_b, u8 *dst_h, u8 *dst_s, u8 *dst_v)
{
    float h, s, v; // h:0-360.0, s:0.0-1.0, v:0.0-1.0
    float r = src_r / 255.0f;
    float g = src_g / 255.0f;
    float b = src_b / 255.0f;

    float max = max_f(r, g, b);
    float min = min_f(r, g, b);

    v = max;
    if (max == 0.0f) {
        s = 0;
        h = 0;
    } else if (max - min == 0.0f) {
        s = 0;
        h = 0;
    } else {
        s = (max - min) / max;

        if (max == r) {
            h = 60 * ((g - b) / (max - min)) + 0;
        } else if (max == g) {
            h = 60 * ((b - r) / (max - min)) + 120;
        } else {
            h = 60 * ((r - g) / (max - min)) + 240;
        }
    }
    if (h < 0) h += 360.0f;

    *dst_h = (u8)(h / 2);   // dst_h : 0-180
    *dst_s = (u8)(s * 255); // dst_s : 0-255
    *dst_v = (u8)(v * 255); // dst_v : 0-255
}

void hsv2rgb(u8 src_h, u8 src_s, u8 src_v, u8 *dst_r, u8 *dst_g, u8 *dst_b)
{
	float r, g, b; // 0.0-1.0
	float h = src_h *   2.0f; // 0-360
	float s = src_s / 255.0f; // 0.0-1.0
	float v = src_v / 255.0f; // 0.0-1.0

	int   hi = (int)(h / 60.0f) % 6;
	float f  = (h / 60.0f) - hi;
	float p  = v * (1.0f - s);
	float q  = v * (1.0f - s * f);
	float t  = v * (1.0f - s * (1.0f - f));

	switch(hi) {
	case 0: r = v; g = t; b = p; break;
	case 1: r = q; g = v; b = p; break;
	case 2: r = p; g = v; b = t; break;
	case 3: r = p; g = q; b = v; break;
	case 4: r = t; g = p; b = v; break;
	case 5: r = v; g = p; b = q; break;
	}

	*dst_r = (u8)(r * 255); // dst_r : 0-255
	*dst_g = (u8)(g * 255); // dst_r : 0-255
	*dst_b = (u8)(b * 255); // dst_r : 0-255
}


static JSValue texture_RGB_HSV(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv, Bool toHSV)
{
	JSValue nobj;
	u32 i, j;
	GF_JSTexture *tx_hsv;
	GF_JSTexture *tx = JS_GetOpaque(obj, texture_class_id);
	if (!tx || !tx->stencil) return JS_EXCEPTION;

	GF_SAFEALLOC(tx_hsv, GF_JSTexture);
	tx_hsv->width = tx->width;
	tx_hsv->height = tx->height;
	tx_hsv->pf = GF_PIXEL_RGB;
	tx_hsv->nb_comp = 3;
	gf_pixel_get_size_info(tx_hsv->pf, tx_hsv->width, tx_hsv->height, &tx_hsv->data_size, &tx_hsv->stride, &tx_hsv->stride_uv, NULL, NULL);
	tx_hsv->data = malloc(sizeof(char)*tx_hsv->data_size);
	tx_hsv->owns_data = GF_TRUE;

	for (j=0; j<tx_hsv->height; j++) {
		u8 *dst = tx_hsv->data + j*tx_hsv->stride;
		for (i=0; i<tx_hsv->width; i++) {
			u32 col = gf_evg_stencil_get_pixel(tx->stencil, i, j);
			u8 a = GF_COL_A(col);
			u8 r = GF_COL_R(col);
			u8 g = GF_COL_G(col);
			u8 b = GF_COL_B(col);
			if (!a) {
				r = g = b = 0;
			} else if (toHSV) {
				rgb2hsv(r, g, b, &r, &g, &b);
			} else {
				hsv2rgb(r, g, b, &r, &g, &b);
			}
			dst[0] = r;
			dst[1] = g;
			dst[2] = b;
			dst += 3;
		}
	}
	tx_hsv->stencil = gf_evg_stencil_new(GF_STENCIL_TEXTURE);
	gf_evg_stencil_set_texture(tx_hsv->stencil, tx_hsv->data, tx_hsv->width, tx_hsv->height, tx_hsv->stride, tx_hsv->pf);
	nobj = JS_NewObjectClass(c, texture_class_id);
	JS_SetOpaque(nobj, tx_hsv);
	return nobj;
}
static JSValue texture_toHSV(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return texture_RGB_HSV(c, obj, argc, argv, GF_TRUE);
}
static JSValue texture_toRGB(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return texture_RGB_HSV(c, obj, argc, argv, GF_FALSE);
}

static JSValue texture_split(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	JSValue nobj;
	u32 i, j, idx, pix_shift=0;
	GF_IRect src;
	GF_JSTexture *tx_split;
	GF_JSTexture *tx = JS_GetOpaque(obj, texture_class_id);
	if (!tx || !tx->stencil || !argc) return JS_EXCEPTION;

	if (JS_ToInt32(c, &idx, argv[0])) return JS_EXCEPTION;
	if (idx>=tx->nb_comp) return JS_EXCEPTION;

	src.x = src.y = 0;
	src.width = tx->width;
	src.height = tx->height;
	if (argc>1) {
		JSValue v;
		int res;
		if (!JS_IsObject(argv[1])) return JS_EXCEPTION;

#define GETIT(_name, _var) \
		v = JS_GetPropertyStr(c, argv[1], _name);\
		res = JS_ToInt32(c, &(src._var), v);\
		JS_FreeValue(c, v);\
		if (res) return JS_EXCEPTION;\
		if (src._var<0) return JS_EXCEPTION;\

		GETIT("x", x)
		GETIT("y", y)
		GETIT("w", width)
		GETIT("h", height)
#undef GETIT
	}

	GF_SAFEALLOC(tx_split, GF_JSTexture);
	tx_split->width = src.width;
	tx_split->height = src.height;
	tx_split->pf = GF_PIXEL_GREYSCALE;
	tx_split->nb_comp = 1;
	tx_split->stride = tx_split->width;
	tx_split->data_size = tx_split->width * tx_split->height;
	tx_split->data = malloc(sizeof(char) * tx_split->data_size);
	tx_split->owns_data = GF_TRUE;

	pix_shift = 0;
	if (idx==0) {
		pix_shift = 16; //R component
	} else if (idx==1) {
		if ((tx->pf == GF_PIXEL_ALPHAGREY) || (tx->pf == GF_PIXEL_GREYALPHA)) pix_shift = 24; //alpha
		else pix_shift = 8; //green
	} else if (idx==2) {
		pix_shift = 0; //blue
	} else if (idx==3) {
		pix_shift = 24; //alpha
	}
	for (j=0; j<tx_split->height; j++) {
		u8 *dst = tx_split->data + j*tx_split->stride;
		for (i=0; i<tx_split->width; i++) {
			u32 col = gf_evg_stencil_get_pixel(tx->stencil, src.x + i, src.y + j);
			*dst++ = (col >> pix_shift) & 0xFF;
		}
	}
	tx_split->stencil = gf_evg_stencil_new(GF_STENCIL_TEXTURE);
	gf_evg_stencil_set_texture(tx_split->stencil, tx_split->data, tx_split->width, tx_split->height, tx_split->stride, tx_split->pf);
	nobj = JS_NewObjectClass(c, texture_class_id);
	JS_SetOpaque(nobj, tx_split);
	return nobj;
}
static JSValue texture_conv(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	JSValue v, k, nobj;
	u32 i, j, kw=0, kh=0, kl=0, hkh, hkw;
	Double *kdata;
	s32 knorm=0;
	GF_JSTexture *tx_conv;
	GF_JSTexture *tx = JS_GetOpaque(obj, texture_class_id);
	if (!tx || !tx->stencil || !argc) return JS_EXCEPTION;

	if (!JS_IsObject(argv[0]))
		return JS_EXCEPTION;

	v = JS_GetPropertyStr(c, argv[0], "w");
	JS_ToInt32(c, &kw, v);
	JS_FreeValue(c, v);
	v = JS_GetPropertyStr(c, argv[0], "h");
	JS_ToInt32(c, &kh, v);
	JS_FreeValue(c, v);
	v = JS_GetPropertyStr(c, argv[0], "norm");
	if (!JS_IsUndefined(v))
		JS_ToInt32(c, &knorm, v);
	JS_FreeValue(c, v);
	if (!kh || !kw)
		return JS_EXCEPTION;
	if (!(kh%2) || !(kw%2))
		return JS_EXCEPTION;
	k = JS_GetPropertyStr(c, argv[0], "k");
	if (JS_IsUndefined(k))
		return JS_EXCEPTION;

	v = JS_GetPropertyStr(c, k, "length");
	JS_ToInt32(c, &kl, v);
	JS_FreeValue(c, v);
	if (kl < kw * kh) {
		JS_FreeValue(c, k);
		return JS_EXCEPTION;
	}
	kl = kw*kh;
	kdata = gf_malloc(sizeof(Double)*kl);
	for (j=0; j<kh; j++) {
		for (i=0; i<kw; i++) {
			u32 idx = j*kw + i;
			v = JS_GetPropertyUint32(c, k, idx);
			JS_ToFloat64(c, &kdata[idx] , v);
			JS_FreeValue(c, v);
		}
	}
	JS_FreeValue(c, k);

	GF_SAFEALLOC(tx_conv, GF_JSTexture);
	tx_conv->width = tx->width;
	tx_conv->height = tx->height;
	tx_conv->pf = GF_PIXEL_RGB;
	tx_conv->nb_comp = 3;
	gf_pixel_get_size_info(tx_conv->pf, tx_conv->width, tx_conv->height, &tx_conv->data_size, &tx_conv->stride, &tx_conv->stride_uv, NULL, NULL);
	tx_conv->data = malloc(sizeof(char)*tx_conv->data_size);
	tx_conv->owns_data = GF_TRUE;

	hkh = kh/2;
	hkw = kw/2;
	for (j=0; j<tx_conv->height; j++) {
		u8 *dst = tx_conv->data + j*tx_conv->stride;
		for (i=0; i<tx_conv->width; i++) {
			u32 k, l, nb_pix=0;
			u32 kr = 0;
			u32 kg = 0;
			u32 kb = 0;

			for (k=0; k<kh; k++) {
				if (j+k < hkh) continue;
				if (j+k >= tx_conv->height + hkh) continue;

				for (l=0; l<kw; l++) {
					u32 kv;
					if (i+l < hkw) continue;
					else if (i+l >= tx_conv->width + hkw) continue;

					u32 col = gf_evg_stencil_get_pixel(tx->stencil, i+l-hkw, j+k-hkh);
					kv = kdata[k*kw + l];
					kr += kv * (s32) GF_COL_R(col);
					kg += kv * (s32) GF_COL_G(col);
					kb += kv * (s32) GF_COL_B(col);
					nb_pix++;
				}
			}

			if (nb_pix!=kl) {
				u32 n = knorm ? knorm : 1;
				kr = (kr * kl / n / nb_pix);
				kg = (kg * kl / n / nb_pix);
				kb = (kb * kl / n / nb_pix);
			} else if (knorm) {
				kr /= knorm;
				kg /= knorm;
				kb /= knorm;
			}
#define SET_CLAMP(_d, _s)\
			if (_s<0) _d = 0;\
			else if (_s>255) _d = 255;\
			else _d = (u8) _s;

			SET_CLAMP(dst[0], kr)
			SET_CLAMP(dst[1], kg)
			SET_CLAMP(dst[2], kb)

			dst += 3;
		}
	}
	gf_free(kdata);

	tx_conv->stencil = gf_evg_stencil_new(GF_STENCIL_TEXTURE);
	gf_evg_stencil_set_texture(tx_conv->stencil, tx_conv->data, tx_conv->width, tx_conv->height, tx_conv->stride, tx_conv->pf);
	nobj = JS_NewObjectClass(c, texture_class_id);
	JS_SetOpaque(nobj, tx_conv);
	return nobj;
}


const char *jsf_get_script_filename(JSContext *c);

static GF_Err texture_load_file(JSContext *c, GF_JSTexture *tx, const char *fileName, Bool rel_to_script)
{
	u8 *data;
	u32 size;
	GF_Err e;
	GF_BitStream *bs;
	char *full_url = NULL;
	u8 *dsi=NULL;
	u32 codecid, width, height, pf, dsi_len, osize;

	if (rel_to_script) {
		const char *par_url = jsf_get_script_filename(c);
		full_url = gf_url_concatenate(par_url, fileName);
		fileName = full_url;
	}
	if (!gf_file_exists(fileName) || (gf_file_load_data(fileName, &data, &size) != GF_OK)) {
		if (full_url) gf_free(full_url);
		return GF_URL_ERROR;
	}
	if (full_url) gf_free(full_url);
	bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
	gf_img_parse(bs, &codecid, &width, &height, &dsi, &dsi_len);
	gf_bs_del(bs);

	e = GF_NOT_SUPPORTED;
	if (codecid==GF_CODECID_PNG) {
		e = gf_img_png_dec(data, size, &width, &height, &pf, NULL, &osize);
		if (e ==GF_BUFFER_TOO_SMALL) {
			tx->owns_data = GF_TRUE;
			tx->data_size = osize;
			tx->data = gf_malloc(sizeof(char) * osize);
			e = gf_img_png_dec(data, size, &width, &height, &pf, tx->data, &osize);
			if (e==GF_OK) {
				tx->width = width;
				tx->height = height;
				tx->pf = pf;
			}
		}
	}
	if (codecid==GF_CODECID_JPEG) {
		e = gf_img_jpeg_dec(data, size, &width, &height, &pf, NULL, &osize, 0);
		if (e ==GF_BUFFER_TOO_SMALL) {
			tx->owns_data = GF_TRUE;
			tx->data_size = osize;
			tx->data = gf_malloc(sizeof(char) * osize);
			e = gf_img_jpeg_dec(data, size, &width, &height, &pf, tx->data, &osize, 0);
			if (e==GF_OK) {
				tx->width = width;
				tx->height = height;
				tx->pf = pf;
			}
		}
	}
	gf_free(data);
	if (dsi) gf_free(dsi);

	if (e != GF_OK) {
		return e;
	}
	tx->nb_comp = gf_pixel_get_nb_comp(tx->pf);
	gf_pixel_get_size_info(tx->pf, tx->width, tx->height, NULL, &tx->stride, NULL, NULL, NULL);
	e = gf_evg_stencil_set_texture(tx->stencil, tx->data, tx->width, tx->height, tx->stride, tx->pf);
	if (e != GF_OK) {
		return e;
	}
	return GF_OK;
}

static const JSCFunctionListEntry texture_funcs[] =
{
	JS_CGETSET_MAGIC_DEF("filtering", NULL, texture_setProperty, TX_FILTER),
	JS_CGETSET_MAGIC_DEF("cmx", NULL, texture_setProperty, TX_CMX),
	JS_CGETSET_MAGIC_DEF("mx", NULL, texture_setProperty, TX_MAT),
	JS_CGETSET_MAGIC_DEF("repeat_s", texture_getProperty, texture_setProperty, TX_REPEAT_S),
	JS_CGETSET_MAGIC_DEF("repeat_t", texture_getProperty, texture_setProperty, TX_REPEAT_T),
	JS_CGETSET_MAGIC_DEF("flip", texture_getProperty, texture_setProperty, TX_FLIP),
	JS_CGETSET_MAGIC_DEF("width", texture_getProperty, NULL, TX_WIDTH),
	JS_CGETSET_MAGIC_DEF("height", texture_getProperty, NULL, TX_HEIGHT),
	JS_CGETSET_MAGIC_DEF("pixfmt", texture_getProperty, NULL, TX_PIXFMT),
	JS_CGETSET_MAGIC_DEF("comp", texture_getProperty, NULL, TX_NB_COMP),
	JS_CGETSET_MAGIC_DEF("data", texture_getProperty, NULL, TX_DATA),

	JS_CFUNC_DEF("set_alpha", 0, stencil_set_alpha),
	JS_CFUNC_DEF("set_alphaf", 0, stencil_set_alphaf),
	JS_CFUNC_DEF("to_HSV", 0, texture_toHSV),
	JS_CFUNC_DEF("to_RGB", 0, texture_toRGB),
	JS_CFUNC_DEF("conv", 0, texture_conv),
	JS_CFUNC_DEF("split", 0, texture_split),
};

GF_Err jsf_get_filter_packet_planes(JSContext *c, JSValue obj, u32 *width, u32 *height, u32 *pf, u32 *stride, u32 *stride_uv, u8 **data, u8 **p_u, u8 **p_v, u8 **p_a);
Bool jsf_is_packet(JSContext *c, JSValue obj);

static JSValue texture_constructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	JSValue obj;
	u32 width=0, height=0, pf=0, stride=0, stride_uv=0;
	size_t data_size=0;
	u8 *data=NULL;
	u8 *p_u=NULL;
	u8 *p_v=NULL;
	u8 *p_a=NULL;
	GF_JSTexture *tx;
	GF_SAFEALLOC(tx, GF_JSTexture);
	if (!tx) return JS_EXCEPTION;
	tx->stencil = gf_evg_stencil_new(GF_STENCIL_TEXTURE);
	if (!tx->stencil) {
		gf_free(tx);
		return JS_EXCEPTION;
	}
	if (!argc) goto done;

	if (JS_IsString(argv[0])) {
		GF_Err e;
		Bool rel_to_script = GF_FALSE;
		const char *str = JS_ToCString(c, argv[0]);
		if (argc>1) rel_to_script = JS_ToBool(c, argv[1]);
		e = texture_load_file(c, tx, str, rel_to_script);
		JS_FreeCString(c, str);
		if (e) goto error;
		goto done;
	}
	if (JS_IsObject(argv[0])) {
		//create from cnavas object
		GF_JSCanvas *canvas = JS_GetOpaque(argv[0], canvas_class_id);
		if (canvas) {
			width = canvas->width;
			height = canvas->height;
			stride = canvas->stride;
			stride_uv = canvas->stride_uv;
			data = canvas->data;
			pf = canvas->pf;
		}
		//create from filter packet
		else if (jsf_is_packet(c, argv[0])) {
			GF_Err e = jsf_get_filter_packet_planes(c, argv[0], &width, &height, &pf, &stride, &stride_uv, (u8 **)&data, &p_u, &p_v, &p_a);
			if (e) goto error;
		} else {
			goto error;
		}
	}
	//arraybuffer
	else {
		if (argc<4) goto error;
		if (JS_ToInt32(c, &width, argv[0])) goto error;
		if (JS_ToInt32(c, &height, argv[1])) goto error;
		if (JS_IsString(argv[2])) {
			const char *str = JS_ToCString(c, argv[2]);
			pf = gf_pixel_fmt_parse(str);
			JS_FreeCString(c, str);
		} else if (JS_ToInt32(c, &pf, argv[2])) {
			goto error;
		}
		if (JS_IsObject(argv[3])) {
			data = JS_GetArrayBuffer(c, &data_size, argv[3]);
		}
		if (!width || !height || !pf || !data)
			goto error;

		if (argc>4) {
			if (JS_ToInt32(c, &stride, argv[4]))
				goto error;
			if (argc>5) {
				if (JS_ToInt32(c, &stride_uv, argv[5]))
					goto error;
			}
		}
	}

	tx->owns_data = GF_FALSE;
	tx->width = width;
	tx->height = height;
	tx->pf = pf;
	tx->stride = stride;
	tx->stride_uv = stride_uv;
	tx->data = data;
	if (p_u || p_v) {
		if (gf_evg_stencil_set_texture_planes(tx->stencil, tx->width, tx->height, tx->pf, data, tx->stride, p_u, p_v, tx->stride_uv, p_a, tx->stride) != GF_OK)
			goto error;
	} else {
		if (gf_evg_stencil_set_texture(tx->stencil, tx->data, tx->width, tx->height, tx->stride, tx->pf) != GF_OK)
			goto error;
	}

done:
	tx->nb_comp = gf_pixel_get_nb_comp(tx->pf);
	gf_pixel_get_size_info(tx->pf, tx->width, tx->height, NULL, NULL, NULL, NULL, NULL);
	obj = JS_NewObjectClass(c, texture_class_id);
	if (JS_IsException(obj)) return obj;
	JS_SetOpaque(obj, tx);
	return obj;

error:
	if (tx->stencil) gf_evg_stencil_delete(tx->stencil);
	gf_free(tx);
	return JS_EXCEPTION;
}

static void text_reset(GF_JSText *txt)
{
	if (txt->path) gf_path_del(txt->path);
	txt->path = NULL;
	while (gf_list_count(txt->spans)) {
		GF_TextSpan *s = gf_list_pop_back(txt->spans);
		gf_font_manager_delete_span(txt->fm, s);
	}
	txt->min_x = txt->min_y = txt->max_x = txt->max_y = txt->max_w = txt->max_h = 0;
}
static void text_finalize(JSRuntime *rt, JSValue obj)
{
	GF_JSText *txt = JS_GetOpaque(obj, text_class_id);
	if (!txt) return;
	text_reset(txt);
	if (txt->fontname) gf_free(txt->fontname);
	gf_list_del(txt->spans);
	gf_free(txt);
}

enum{
	TXT_FONT=0,
	TXT_FONTSIZE,
	TXT_ALIGN,
	TXT_BASELINE,
	TXT_HORIZ,
	TXT_FLIP,
	TXT_UNDERLINED,
	TXT_BOLD,
	TXT_ITALIC,
	TXT_SMALLCAP,
	TXT_STRIKEOUT,
	TXT_MAX_WIDTH,
	TXT_LINESPACING,
};

enum
{
	TXT_BL_TOP=0,
	TXT_BL_HANGING,
	TXT_BL_MIDDLE,
	TXT_BL_ALPHABETIC,
	TXT_BL_IDEOGRAPHIC,
	TXT_BL_BOTTOM,
};
enum
{
	TXT_AL_START=0,
	TXT_AL_END,
	TXT_AL_LEFT,
	TXT_AL_RIGHT,
	TXT_AL_CENTER
};

static JSValue text_getProperty(JSContext *c, JSValueConst obj, int magic)
{
	GF_JSText *txt = JS_GetOpaque(obj, text_class_id);
	if (!txt) return JS_EXCEPTION;
	switch (magic) {
	case TXT_FONT: return JS_NewString(c, txt->fontname);
	case TXT_BASELINE: return JS_NewInt32(c, txt->baseline);
	case TXT_ALIGN: return JS_NewInt32(c, txt->align);
	case TXT_FONTSIZE: return JS_NewFloat64(c, txt->font_size);
	case TXT_HORIZ: return JS_NewBool(c, txt->horizontal);
	case TXT_FLIP: return JS_NewBool(c, txt->flip);
	case TXT_MAX_WIDTH: return JS_NewFloat64(c, txt->maxWidth);
	case TXT_LINESPACING: return JS_NewFloat64(c, txt->lineSpacing);
	case TXT_BOLD: return JS_NewBool(c, txt->styles & GF_FONT_WEIGHT_100);
	case TXT_ITALIC: return JS_NewBool(c, txt->styles & GF_FONT_ITALIC);
	case TXT_UNDERLINED: return JS_NewBool(c, txt->styles & GF_FONT_UNDERLINED);
	case TXT_SMALLCAP: return JS_NewBool(c, txt->styles & GF_FONT_SMALLCAPS);
	case TXT_STRIKEOUT: return JS_NewBool(c, txt->styles & GF_FONT_STRIKEOUT);
	}
	return JS_UNDEFINED;
}

static void text_update_path(GF_JSText *txt, Bool for_centered)
{
	Fixed cy, fs, ascent, descent, scale_x, ls;
	u32 i, nb_lines;

	if ((txt->path_for_centered == for_centered) && txt->path) {
		return;
	}
	if (txt->path) gf_path_del(txt->path);
	txt->path_for_centered = for_centered;

	cy = FLT2FIX((txt->font_size * txt->font->baseline) / txt->font->em_size);

	fs = FLT2FIX(txt->font_size);
	ascent = FLT2FIX((txt->font_size*txt->font->ascent) / txt->font->em_size);
	if (txt->lineSpacing)
		ls = FLT2FIX((txt->lineSpacing*txt->font->line_spacing) / txt->font->em_size);
	else
		ls = FLT2FIX((txt->font_size*txt->font->line_spacing) / txt->font->em_size);
	descent = -FLT2FIX((txt->font_size*txt->font->descent) / txt->font->em_size);
	scale_x = 0;
	if (txt->maxWidth && txt->max_w && (txt->max_w > txt->maxWidth)) {
		scale_x = gf_divfix(FLT2FIX(txt->maxWidth), INT2FIX(txt->max_w) );
	}

	if (txt->baseline==TXT_BL_TOP) {
		cy = ascent;
	} else if (txt->baseline==TXT_BL_BOTTOM) {
		cy = -descent;
	} else if (txt->baseline==TXT_BL_MIDDLE) {
		Fixed mid = (ascent + descent)/2;
		cy = mid;
	}

	txt->path = gf_path_new();
	nb_lines = gf_list_count(txt->spans);
	for (i=0; i<nb_lines; i++) {
		Fixed cx=0;
		u32 flags;
		GF_Path *path;
		GF_Matrix2D mx;
		GF_TextSpan *span = gf_list_get(txt->spans, i);
		if ((txt->align == TXT_AL_LEFT)
		|| ((txt->align == TXT_AL_START) && !(span->flags & GF_TEXT_SPAN_RIGHT_TO_LEFT))
		|| ((txt->align == TXT_AL_END) && (span->flags & GF_TEXT_SPAN_RIGHT_TO_LEFT))
		) {
			cx = 0;
		}
		else if ((txt->align == TXT_AL_RIGHT)
		|| ((txt->align == TXT_AL_END) && !(span->flags & GF_TEXT_SPAN_RIGHT_TO_LEFT))
		|| ((txt->align == TXT_AL_START) && (span->flags & GF_TEXT_SPAN_RIGHT_TO_LEFT))
		) {
			cx = txt->max_w - span->bounds.width;
		} else if (txt->align == TXT_AL_CENTER) {
			cx = (txt->max_w - span->bounds.width) / 2;
		}
		gf_mx2d_init(mx);

		gf_mx2d_add_translation(&mx, cx, cy);
		if (scale_x)
			gf_mx2d_add_scale(&mx, scale_x, FIX_ONE);

		flags = span->flags;
		if (txt->path_for_centered) {
			if (txt->flip)
				span->flags |= GF_TEXT_SPAN_FLIP;
		} else {
			if (!txt->flip)
				span->flags |= GF_TEXT_SPAN_FLIP;
		}
		if (txt->horizontal)
			span->flags |= GF_TEXT_SPAN_HORIZONTAL;

		path = gf_font_span_create_path(span);
		gf_path_add_subpath(txt->path, path, &mx);
		gf_path_del(path);
		span->flags = flags;

		if (txt->path_for_centered)
			cy -= ls;
		else
			cy += ls;
	}
}
static void text_set_path(GF_JSCanvas *canvas, GF_JSText *txt)
{
	text_update_path(txt, canvas->center_coords);
	gf_evg_surface_set_path(canvas->surface, txt->path);
}

static GF_TextSpan * text_set_text_line(GF_JSText *txt, GF_Font *font, JSContext *c, JSValueConst value)
{
	const char *str = JS_ToCString(c, value);
	GF_TextSpan *span = gf_font_manager_create_span(txt->fm, font, (char *) str, FLT2FIX(txt->font_size), GF_FALSE, GF_FALSE, GF_FALSE, NULL, GF_FALSE, 0, NULL);

	JS_FreeCString(c, str);

	if (!span) return NULL;

	gf_list_add(txt->spans, span);
	return span;
}

static JSValue text_get_path(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	JSValue nobj;
	Bool is_center = GF_TRUE;
	GF_JSText *txt = JS_GetOpaque(obj, text_class_id);
	if (!txt) return JS_EXCEPTION;
	if (argc) is_center = JS_ToBool(c, argv[0]);

	text_update_path(txt, is_center);
	if (!txt->path) return JS_NULL;
	nobj = JS_NewObjectClass(c, path_class_id);
	JS_SetOpaque(nobj, gf_path_clone(txt->path));
	return nobj;
}

static JSValue text_set_text(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_JSText *txt = JS_GetOpaque(obj, text_class_id);
	if (!txt) return JS_EXCEPTION;
	text_reset(txt);
	if (!argc) return JS_UNDEFINED;

	txt->font = gf_font_manager_set_font(txt->fm, &txt->fontname, 1, txt->styles);

	if (JS_IsArray(c, argv[0])) {
		u32 i, nb_lines;
		JSValue v = JS_GetPropertyStr(c, argv[0], "length");
		JS_ToInt32(c, &nb_lines, v);
		JS_FreeValue(c, v);

		for (i=0; i<nb_lines; i++) {
			GF_TextSpan *span;
			v = JS_GetPropertyUint32(c, argv[0], i);
			span = text_set_text_line(txt, txt->font, c, v);
			JS_FreeValue(c, v);
			if (!span) continue;

			gf_font_manager_refresh_span_bounds(span);
			if (!txt->max_h && !txt->max_w) {
				txt->max_w = span->bounds.width;
				txt->max_h = span->bounds.height;
				txt->min_x = span->bounds.x;
				txt->min_y = span->bounds.y;
				txt->max_x = txt->min_x + span->bounds.width;
				txt->max_y = txt->min_y + span->bounds.x;
			} else {
				if (txt->min_x > span->bounds.x)
					txt->min_x = span->bounds.x;
				if (txt->min_y > span->bounds.y)
					txt->min_y = span->bounds.y;
				if (txt->max_w > span->bounds.width)
					txt->max_w = span->bounds.width;
				if (txt->max_h > span->bounds.height)
					txt->max_h = span->bounds.height;
				if (txt->max_x < span->bounds.x + span->bounds.width)
					txt->max_x = span->bounds.x + span->bounds.width;
				if (txt->max_y < span->bounds.y + span->bounds.height)
					txt->max_y = span->bounds.y + span->bounds.height;
			}
		}
	} else {
		GF_TextSpan *span = text_set_text_line(txt, txt->font, c, argv[0]);
		gf_font_manager_refresh_span_bounds(span);
		txt->max_w = span->bounds.width;
		txt->max_h = span->bounds.height;
		txt->min_x = span->bounds.x;
		txt->min_y = span->bounds.y;
		txt->max_x = txt->min_x + span->bounds.width;
		txt->max_y = txt->min_y + span->bounds.x;
	}

	return JS_UNDEFINED;
}

static JSValue text_setProperty(JSContext *c, JSValueConst obj, JSValueConst value, int magic)
{
	const char *str;
	GF_JSText *txt = JS_GetOpaque(obj, text_class_id);
	if (!txt) return JS_EXCEPTION;

	switch (magic) {
	case TXT_FONT:
		str = JS_ToCString(c, value);
		if (txt->fontname) gf_free(txt->fontname);
		txt->fontname = str ? gf_strdup(str) : NULL;
		JS_FreeCString(c, str);
		break;
	case TXT_BASELINE:
		if (JS_ToInt32(c, &txt->baseline, value)) return JS_EXCEPTION;
		return JS_UNDEFINED;
	case TXT_ALIGN:
		if (JS_ToInt32(c, &txt->align, value)) return JS_EXCEPTION;
		return JS_UNDEFINED;
	case TXT_FONTSIZE:
		if (JS_ToFloat64(c, &txt->font_size, value)) return JS_EXCEPTION;
		break;
	case TXT_HORIZ:
		txt->horizontal = JS_ToBool(c, value);
		break;
	case TXT_FLIP:
		txt->flip = JS_ToBool(c, value);
		break;
#define TOG_FLAG(_val) \
		if (JS_ToBool(c, value)) \
			txt->styles |= _val;\
		else\
			txt->styles &= ~_val;\


	case TXT_UNDERLINED:
		TOG_FLAG(GF_FONT_UNDERLINED);
		break;
	case TXT_ITALIC:
		TOG_FLAG(GF_FONT_ITALIC);
		break;
	case TXT_BOLD:
		TOG_FLAG(GF_FONT_WEIGHT_100);
		break;
	case TXT_STRIKEOUT:
		TOG_FLAG(GF_FONT_STRIKEOUT);
		break;
	case TXT_SMALLCAP:
		TOG_FLAG(GF_FONT_SMALLCAPS);
		break;
#undef TOG_FLAG

	case TXT_MAX_WIDTH:
		JS_ToFloat64(c, &txt->maxWidth, value);
		break;
	case TXT_LINESPACING:
		JS_ToFloat64(c, &txt->lineSpacing, value);
		break;
	}
	return JS_UNDEFINED;
}

static JSValue text_measure(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	JSValue res;
	GF_JSText *txt = JS_GetOpaque(obj, text_class_id);
	if (!txt) return JS_EXCEPTION;
	res = JS_NewObject(c);
	JS_SetPropertyStr(c, res, "width", JS_NewFloat64(c, txt->max_w) );
	JS_SetPropertyStr(c, res, "height", JS_NewFloat64(c, txt->max_h) );
	if (txt->font) {
		JS_SetPropertyStr(c, res, "em_size", JS_NewInt32(c, txt->font->em_size) );
		JS_SetPropertyStr(c, res, "ascent", JS_NewInt32(c, txt->font->ascent) );
		JS_SetPropertyStr(c, res, "descent", JS_NewInt32(c, txt->font->descent) );
		JS_SetPropertyStr(c, res, "line_spacing", JS_NewInt32(c, txt->font->line_spacing) );
		JS_SetPropertyStr(c, res, "underlined", JS_NewInt32(c, txt->font->underline) );
		JS_SetPropertyStr(c, res, "baseline", JS_NewInt32(c, txt->font->baseline) );
		JS_SetPropertyStr(c, res, "max_advance_h", JS_NewInt32(c, txt->font->max_advance_h) );
		JS_SetPropertyStr(c, res, "max_advance_v", JS_NewInt32(c, txt->font->max_advance_v) );
	}
	return res;
}
struct _gf_ft_mgr *jsf_get_font_manager(JSContext *c);

static const JSCFunctionListEntry text_funcs[] =
{
	JS_CGETSET_MAGIC_DEF("font", text_getProperty, text_setProperty, TXT_FONT),
	JS_CGETSET_MAGIC_DEF("fontsize", text_getProperty, text_setProperty, TXT_FONTSIZE),
	JS_CGETSET_MAGIC_DEF("align", text_getProperty, text_setProperty, TXT_ALIGN),
	JS_CGETSET_MAGIC_DEF("baseline", text_getProperty, text_setProperty, TXT_BASELINE),
	JS_CGETSET_MAGIC_DEF("horizontal", text_getProperty, text_setProperty, TXT_HORIZ),
	JS_CGETSET_MAGIC_DEF("flip", text_getProperty, text_setProperty, TXT_FLIP),
	JS_CGETSET_MAGIC_DEF("underline", text_getProperty, text_setProperty, TXT_UNDERLINED),
	JS_CGETSET_MAGIC_DEF("bold", text_getProperty, text_setProperty, TXT_BOLD),
	JS_CGETSET_MAGIC_DEF("italic", text_getProperty, text_setProperty, TXT_ITALIC),
	JS_CGETSET_MAGIC_DEF("maxWidth", text_getProperty, text_setProperty, TXT_MAX_WIDTH),
	JS_CGETSET_MAGIC_DEF("lineSpacing", text_getProperty, text_setProperty, TXT_LINESPACING),
	JS_CFUNC_DEF("set_text", 0, text_set_text),
	JS_CFUNC_DEF("measure", 0, text_measure),
	JS_CFUNC_DEF("get_path", 0, text_get_path),
};

JSClassDef text_class = {
	"Text",
	.finalizer = text_finalize
};
static JSValue text_constructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	JSValue obj;
	GF_JSText *txt;
	GF_SAFEALLOC(txt, GF_JSText);
	if (!txt) return JS_EXCEPTION;
	txt->fm = jsf_get_font_manager(c);

	if (!txt->fm) {
		gf_free(txt);
		return JS_EXCEPTION;
	}
	txt->spans = gf_list_new();
	if (!txt->spans) {
		gf_free(txt);
		return JS_EXCEPTION;
	}
	if (argc) {
		const char *str = JS_ToCString(c, argv[0]);
		if (str) txt->fontname = gf_strdup(str);
		JS_FreeCString(c, str);
	}
	txt->font_size = 12.0;
	txt->horizontal = GF_TRUE;
	txt->align = TXT_AL_START;
	txt->baseline = TXT_BL_ALPHABETIC;

	obj = JS_NewObjectClass(c, text_class_id);
	if (JS_IsException(obj)) return obj;
	JS_SetOpaque(obj, txt);
	return obj;
}

static int js_evg_load_module(JSContext *c, JSModuleDef *m)
{
	JSValue ctor;
	JSValue proto;
	JSValue global;

	if (!canvas_class_id) {
		JS_NewClassID(&canvas_class_id);
		JS_NewClass(JS_GetRuntime(c), canvas_class_id, &canvas_class);

		JS_NewClassID(&path_class_id);
		JS_NewClass(JS_GetRuntime(c), path_class_id, &path_class);

		JS_NewClassID(&mx2d_class_id);
		JS_NewClass(JS_GetRuntime(c), mx2d_class_id, &mx2d_class);

		JS_NewClassID(&colmx_class_id);
		JS_NewClass(JS_GetRuntime(c), colmx_class_id, &colmx_class);

		JS_NewClassID(&stencil_class_id);
		JS_NewClass(JS_GetRuntime(c), stencil_class_id, &stencil_class);

		JS_NewClassID(&texture_class_id);
		JS_NewClass(JS_GetRuntime(c), texture_class_id, &texture_class);

		JS_NewClassID(&text_class_id);
		JS_NewClass(JS_GetRuntime(c), text_class_id, &text_class);

	}
	proto = JS_NewObject(c);
    JS_SetPropertyFunctionList(c, proto, canvas_funcs, countof(canvas_funcs));
    JS_SetClassProto(c, canvas_class_id, proto);

	proto = JS_NewObject(c);
    JS_SetPropertyFunctionList(c, proto, path_funcs, countof(path_funcs));
    JS_SetClassProto(c, path_class_id, proto);

	proto = JS_NewObject(c);
    JS_SetPropertyFunctionList(c, proto, mx2d_funcs, countof(mx2d_funcs));
    JS_SetClassProto(c, mx2d_class_id, proto);

	proto = JS_NewObject(c);
    JS_SetPropertyFunctionList(c, proto, colmx_funcs, countof(colmx_funcs));
    JS_SetClassProto(c, colmx_class_id, proto);

	proto = JS_NewObject(c);
    JS_SetPropertyFunctionList(c, proto, stencil_funcs, countof(stencil_funcs));
    JS_SetClassProto(c, stencil_class_id, proto);

	proto = JS_NewObject(c);
    JS_SetPropertyFunctionList(c, proto, texture_funcs, countof(texture_funcs));
    JS_SetClassProto(c, texture_class_id, proto);

	proto = JS_NewObject(c);
    JS_SetPropertyFunctionList(c, proto, text_funcs, countof(text_funcs));
    JS_SetClassProto(c, text_class_id, proto);

	global = JS_GetGlobalObject(c);
	JS_SetPropertyStr(c, global, "GF_GRADIENT_MODE_PAD", JS_NewInt32(c, GF_GRADIENT_MODE_PAD));
	JS_SetPropertyStr(c, global, "GF_GRADIENT_MODE_STREAD", JS_NewInt32(c, GF_GRADIENT_MODE_SPREAD));
	JS_SetPropertyStr(c, global, "GF_GRADIENT_MODE_REAPEAT", JS_NewInt32(c, GF_GRADIENT_MODE_REPEAT));

	JS_SetPropertyStr(c, global, "GF_TEXTURE_FILTER_HIGH_SPEED", JS_NewInt32(c, GF_TEXTURE_FILTER_HIGH_SPEED));
	JS_SetPropertyStr(c, global, "GF_TEXTURE_FILTER_MID", JS_NewInt32(c, GF_TEXTURE_FILTER_MID));
	JS_SetPropertyStr(c, global, "GF_TEXTURE_FILTER_HIGH_QUALITY", JS_NewInt32(c, GF_TEXTURE_FILTER_HIGH_QUALITY));

	JS_SetPropertyStr(c, global, "GF_PATH2D_ARC_OPEN", JS_NewInt32(c, GF_PATH2D_ARC_OPEN));
	JS_SetPropertyStr(c, global, "GF_PATH2D_ARC_OPEN", JS_NewInt32(c, GF_PATH2D_ARC_OPEN));
	JS_SetPropertyStr(c, global, "GF_PATH2D_ARC_PIE", JS_NewInt32(c, GF_PATH2D_ARC_PIE));

	JS_SetPropertyStr(c, global, "GF_PATH_LINE_CENTER", JS_NewInt32(c, GF_PATH_LINE_CENTER));
	JS_SetPropertyStr(c, global, "GF_PATH_LINE_INSIDE", JS_NewInt32(c, GF_PATH_LINE_INSIDE));
	JS_SetPropertyStr(c, global, "GF_PATH_LINE_OUTSIDE", JS_NewInt32(c, GF_PATH_LINE_OUTSIDE));
	JS_SetPropertyStr(c, global, "GF_LINE_CAP_FLAT", JS_NewInt32(c, GF_LINE_CAP_FLAT));
	JS_SetPropertyStr(c, global, "GF_LINE_CAP_ROUND", JS_NewInt32(c, GF_LINE_CAP_ROUND));
	JS_SetPropertyStr(c, global, "GF_LINE_CAP_SQUARE", JS_NewInt32(c, GF_LINE_CAP_SQUARE));
	JS_SetPropertyStr(c, global, "GF_LINE_CAP_TRIANGLE", JS_NewInt32(c, GF_LINE_CAP_TRIANGLE));
	JS_SetPropertyStr(c, global, "GF_LINE_JOIN_MITER", JS_NewInt32(c, GF_LINE_JOIN_MITER));
	JS_SetPropertyStr(c, global, "GF_LINE_JOIN_ROUND", JS_NewInt32(c, GF_LINE_JOIN_ROUND));
	JS_SetPropertyStr(c, global, "GF_LINE_JOIN_BEVEL", JS_NewInt32(c, GF_LINE_JOIN_BEVEL));
	JS_SetPropertyStr(c, global, "GF_LINE_JOIN_MITER_SVG", JS_NewInt32(c, GF_LINE_JOIN_MITER_SVG));
	JS_SetPropertyStr(c, global, "GF_DASH_STYLE_PLAIN", JS_NewInt32(c, GF_DASH_STYLE_PLAIN));
	JS_SetPropertyStr(c, global, "GF_DASH_STYLE_DASH", JS_NewInt32(c, GF_DASH_STYLE_DASH));
	JS_SetPropertyStr(c, global, "GF_DASH_STYLE_DOT", JS_NewInt32(c, GF_DASH_STYLE_DOT));
	JS_SetPropertyStr(c, global, "GF_DASH_STYLE_DASH_DOT", JS_NewInt32(c, GF_DASH_STYLE_DASH_DOT));
	JS_SetPropertyStr(c, global, "GF_DASH_STYLE_DASH_DASH_DOT", JS_NewInt32(c, GF_DASH_STYLE_DASH_DASH_DOT));
	JS_SetPropertyStr(c, global, "GF_DASH_STYLE_DASH_DOT_DOT", JS_NewInt32(c, GF_DASH_STYLE_DASH_DOT_DOT));
	JS_SetPropertyStr(c, global, "GF_DASH_STYLE_SVG", JS_NewInt32(c, GF_DASH_STYLE_SVG));

	JS_SetPropertyStr(c, global, "GF_TEXT_BASELINE_TOP", JS_NewInt32(c, TXT_BL_TOP));
	JS_SetPropertyStr(c, global, "GF_TEXT_BASELINE_HANGING", JS_NewInt32(c, TXT_BL_HANGING));
	JS_SetPropertyStr(c, global, "GF_TEXT_BASELINE_MIDDLE", JS_NewInt32(c, TXT_BL_MIDDLE));
	JS_SetPropertyStr(c, global, "GF_TEXT_BASELINE_ALPHABETIC", JS_NewInt32(c, TXT_BL_ALPHABETIC));
	JS_SetPropertyStr(c, global, "GF_TEXT_BASELINE_IDEOGRAPHIC", JS_NewInt32(c, TXT_BL_IDEOGRAPHIC));
	JS_SetPropertyStr(c, global, "GF_TEXT_BASELINE_BOTTOM", JS_NewInt32(c, TXT_BL_BOTTOM));

	JS_SetPropertyStr(c, global, "GF_TEXT_ALIGN_START", JS_NewInt32(c, TXT_AL_START));
	JS_SetPropertyStr(c, global, "GF_TEXT_ALIGN_END", JS_NewInt32(c, TXT_AL_END));
	JS_SetPropertyStr(c, global, "GF_TEXT_ALIGN_LEFT", JS_NewInt32(c, TXT_AL_LEFT));
	JS_SetPropertyStr(c, global, "GF_TEXT_ALIGN_RIGHT", JS_NewInt32(c, TXT_AL_RIGHT));
	JS_SetPropertyStr(c, global, "GF_TEXT_ALIGN_CENTER", JS_NewInt32(c, TXT_AL_CENTER));

	JS_SetPropertyStr(c, global, "GF_EVG_SRC_ATOP", JS_NewInt32(c, GF_EVG_SRC_ATOP));
	JS_SetPropertyStr(c, global, "GF_EVG_SRC_IN", JS_NewInt32(c, GF_EVG_SRC_IN));
	JS_SetPropertyStr(c, global, "GF_EVG_SRC_OUT", JS_NewInt32(c, GF_EVG_SRC_OUT));
	JS_SetPropertyStr(c, global, "GF_EVG_SRC_OVER", JS_NewInt32(c, GF_EVG_SRC_OVER));
	JS_SetPropertyStr(c, global, "GF_EVG_DST_ATOP", JS_NewInt32(c, GF_EVG_DST_ATOP));
	JS_SetPropertyStr(c, global, "GF_EVG_DST_IN", JS_NewInt32(c, GF_EVG_DST_IN));
	JS_SetPropertyStr(c, global, "GF_EVG_DST_OUT", JS_NewInt32(c, GF_EVG_DST_OUT));
	JS_SetPropertyStr(c, global, "GF_EVG_DST_OVER", JS_NewInt32(c, GF_EVG_DST_OVER));
	JS_SetPropertyStr(c, global, "GF_EVG_LIGHTER", JS_NewInt32(c, GF_EVG_LIGHTER));
	JS_SetPropertyStr(c, global, "GF_EVG_COPY", JS_NewInt32(c, GF_EVG_COPY));
	JS_SetPropertyStr(c, global, "GF_EVG_XOR", JS_NewInt32(c, GF_EVG_XOR));
	JS_FreeValue(c, global);


	/*export constructors*/
	ctor = JS_NewCFunction2(c, canvas_constructor, "Canvas", 1, JS_CFUNC_constructor, 0);
    JS_SetModuleExport(c, m, "Canvas", ctor);
	ctor = JS_NewCFunction2(c, path_constructor, "Path", 1, JS_CFUNC_constructor, 0);
    JS_SetModuleExport(c, m, "Path", ctor);
	ctor = JS_NewCFunction2(c, mx2d_constructor, "Matrix2D", 1, JS_CFUNC_constructor, 0);
    JS_SetModuleExport(c, m, "Matrix2D", ctor);
	ctor = JS_NewCFunction2(c, colmx_constructor, "ColorMatrix", 1, JS_CFUNC_constructor, 0);
    JS_SetModuleExport(c, m, "ColorMatrix", ctor);
	ctor = JS_NewCFunction2(c, solid_brush_constructor, "SolidBrush", 1, JS_CFUNC_constructor, 0);
    JS_SetModuleExport(c, m, "SolidBrush", ctor);
	ctor = JS_NewCFunction2(c, linear_gradient_constructor, "LinearGradient", 1, JS_CFUNC_constructor, 0);
    JS_SetModuleExport(c, m, "LinearGradient", ctor);
	ctor = JS_NewCFunction2(c, radial_gradient_constructor, "RadialGradient", 1, JS_CFUNC_constructor, 0);
    JS_SetModuleExport(c, m, "RadialGradient", ctor);
	ctor = JS_NewCFunction2(c, texture_constructor, "Texture", 1, JS_CFUNC_constructor, 0);
    JS_SetModuleExport(c, m, "Texture", ctor);

	ctor = JS_NewCFunction2(c, text_constructor, "Text", 1, JS_CFUNC_constructor, 0);
    JS_SetModuleExport(c, m, "Text", ctor);
	return 0;
}

void evg_js_init_module(JSContext *ctx)
{
    JSModuleDef *m;
    m = JS_NewCModule(ctx, "evg", js_evg_load_module);
    if (!m)
        return;
	JS_AddModuleExport(ctx, m, "Canvas");
	JS_AddModuleExport(ctx, m, "Path");
	JS_AddModuleExport(ctx, m, "Matrix2D");
	JS_AddModuleExport(ctx, m, "ColorMatrix");
	JS_AddModuleExport(ctx, m, "SolidBrush");
	JS_AddModuleExport(ctx, m, "LinearGradient");
	JS_AddModuleExport(ctx, m, "RadialGradient");
	JS_AddModuleExport(ctx, m, "Texture");
	JS_AddModuleExport(ctx, m, "Text");
    return;
}


#endif

