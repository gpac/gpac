/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2019
 *			All rights reserved
 *
 *  This file is part of GPAC / JavaScript vector graphics bindings
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
	ANY CHANGE TO THE API MUST BE REFLECTED IN THE DOCUMENTATION IN gpac/share/doc/idl/evg.idl
	(no way to define inline JS doc with doxygen)
*/


#include <gpac/setup.h>

#if defined(GPAC_HAS_QJS)

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


#define EVG_GET_FLOAT(_name, _jsval) { Double _v; if (JS_ToFloat64(ctx, &_v, _jsval)) return js_throw_err(ctx, GF_BAD_PARAM); _name = (Float) _v; }
#define CLAMPCOLF(_name) if (_name<0) _name=0; else if (_name>1.0) _name=1.0;

uint8_t *evg_get_array(JSContext *ctx, JSValueConst obj, u32 *size);

//not used, wau too slow - we kept the code for future testing
//#define EVG_USE_JS_SHADER

typedef struct
{
	u32 width, height, pf, stride, stride_uv, nb_comp;
	char *data;
	u32 data_size;
	Bool owns_data;
	u32 flags;
	GF_EVGStencil *stencil;
	JSValue param_fun, obj;
	JSContext *ctx;
} GF_JSTexture;

#define MAX_ATTR_DIM	4
typedef struct
{
	Float values[MAX_ATTR_DIM];
	u32 dim;
	u8 comp_type;
} EVG_VAIRes;

enum
{
	GF_EVG_VAI_VERTEX_INDEX=0,
	GF_EVG_VAI_VERTEX,
	GF_EVG_VAI_PRIMITIVE,
};

typedef struct
{
	u32 prim_idx;
	u32 nb_comp;
	Float *values;
	u32 nb_values;
	JSValue ab;
#ifdef EVG_USE_JS_SHADER
	JSValue res;
#endif
	u32 interp_type;

	Float anchors[3][MAX_ATTR_DIM];
	EVG_VAIRes result;

	Bool normalize;
} EVG_VAI;

typedef struct
{
	u32 nb_comp, att_type;
	Float *values;
	u32 nb_values;
	JSValue ab;
	JSValue res;
	u32 interp_type;

	Bool normalize;
} EVG_VA;

enum
{
	COMP_X = 1,//x or r or s
	COMP_Y = 1<<1,//y or g or t
	COMP_Z = 1<<2, //z or b
	COMP_Q = 1<<3, //w/q or a
	COMP_V2_XY = COMP_X|COMP_Y,
	COMP_V2_XZ = COMP_X|COMP_Z,
	COMP_V2_YZ = COMP_Y|COMP_Z,
	COMP_V3 = COMP_X|COMP_Y|COMP_Z,
	COMP_V4 = COMP_X|COMP_Y|COMP_Z|COMP_Q,
	COMP_BOOL,
	COMP_INT,
	COMP_FLOAT
};

typedef struct
{
	u8 op_type;
	u8 cond_type;
	u8 left_value_type;
	u8 right_value_type;
	s32 left_value, right_value, right_value_second;
	char *uni_name;
	JSValue tx_ref;
	GF_JSTexture *tx;

	//we separate VAI/MX from base value in order to be able to assign a VAI value
	union {
        struct {
			EVG_VAI *vai;
			JSValue ref;
		} vai;
        struct {
			EVG_VA *va;
			JSValue ref;
		} va;
        struct {
			GF_Matrix *mx;
			JSValue ref;
		} mx;
	};

	union {
		Float vec[4];
		s32 ival;
		Bool bval;
	};
} ShaderOp;

enum
{
	GF_EVG_SHADER_FRAGMENT=1,
	GF_EVG_SHADER_VERTEX,
};

typedef struct
{
	char *name;
	union {
		GF_Vec4 vecval;
		s32 ival;
		Bool bval;
	};
	u8 value_type;
} ShaderVar;

typedef struct
{
	u32 mode;
	u32 nb_ops, alloc_ops;
	ShaderOp *ops;
	u32 nb_vars, alloc_vars;
	ShaderVar *vars;
	Bool invalid, disable_early_z;
} EVGShader;

typedef struct
{
	u32 width, height, pf, stride, stride_uv, mem_size;
	char *data;
	Bool owns_data;
	Bool center_coords;
	GF_EVGSurface *surface;
	u32 composite_op;
	JSValue alpha_cbk;
	JSValue frag_shader;
	Bool frag_is_cbk;
	JSValue vert_shader;
	Bool vert_is_cbk;

	JSValue depth_buffer;


	EVGShader *frag, *vert;

	JSContext *ctx;
	JSValue obj;
#ifdef EVG_USE_JS_SHADER
	JSValue frag_obj;
	JSValue vert_obj;
#endif

} GF_JSCanvas;



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
JSClassID canvas3d_class_id;
JSClassID path_class_id;
JSClassID mx2d_class_id;
JSClassID colmx_class_id;
JSClassID stencil_class_id;
JSClassID texture_class_id;
JSClassID text_class_id;
JSClassID matrix_class_id;
#ifdef EVG_USE_JS_SHADER
JSClassID fragment_class_id;
JSClassID vertex_class_id;
JSClassID vaires_class_id;
#endif
JSClassID vai_class_id;
JSClassID va_class_id;
JSClassID shader_class_id;

static void text_set_path(GF_JSCanvas *canvas, GF_JSText *text);

Bool get_color_from_args(JSContext *c, int argc, JSValueConst *argv, u32 idx, Double *a, Double *r, Double *g, Double *b);
static Bool get_color(JSContext *c, JSValueConst obj, Double *a, Double *r, Double *g, Double *b);

static void canvas_finalize(JSRuntime *rt, JSValue obj)
{
	GF_JSCanvas *canvas = JS_GetOpaque(obj, canvas_class_id);
	if (!canvas) return;
	JS_FreeValueRT(rt, canvas->alpha_cbk);
	JS_FreeValueRT(rt, canvas->frag_shader);
	JS_FreeValueRT(rt, canvas->vert_shader);

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
	JS_MarkValue(rt, canvas->frag_shader, mark_func);
	JS_MarkValue(rt, canvas->vert_shader, mark_func);
}

JSClassDef canvas_class = {
	"Canvas",
	.finalizer = canvas_finalize,
	.gc_mark = canvas_gc_mark
};

static void canvas3d_finalize(JSRuntime *rt, JSValue obj)
{
	GF_JSCanvas *canvas = JS_GetOpaque(obj, canvas3d_class_id);
	if (!canvas) return;
	JS_FreeValueRT(rt, canvas->alpha_cbk);
	JS_FreeValueRT(rt, canvas->frag_shader);
	JS_FreeValueRT(rt, canvas->vert_shader);
#ifdef EVG_USE_JS_SHADER
	JS_FreeValueRT(rt, canvas->frag_obj);
	JS_FreeValueRT(rt, canvas->vert_obj);
#endif
	JS_FreeValueRT(rt, canvas->depth_buffer);

	if (canvas->owns_data)
		gf_free(canvas->data);
	if (canvas->surface)
		gf_evg_surface_delete(canvas->surface);
	gf_free(canvas);
}

static void canvas3d_gc_mark(JSRuntime *rt, JSValueConst obj, JS_MarkFunc *mark_func)
{
	GF_JSCanvas *canvas = JS_GetOpaque(obj, canvas3d_class_id);
	if (!canvas) return;
	JS_MarkValue(rt, canvas->alpha_cbk, mark_func);
	JS_MarkValue(rt, canvas->frag_shader, mark_func);
	JS_MarkValue(rt, canvas->vert_shader, mark_func);
#ifdef EVG_USE_JS_SHADER
	JS_MarkValue(rt, canvas->frag_obj, mark_func);
	JS_MarkValue(rt, canvas->vert_obj, mark_func);
#endif
	JS_MarkValue(rt, canvas->depth_buffer, mark_func);
}

JSClassDef canvas3d_class = {
	"Canvas3D",
	.finalizer = canvas3d_finalize,
	.gc_mark = canvas3d_gc_mark
};

enum
{
	GF_EVG_CENTERED = 0,
	GF_EVG_PATH,
	GF_EVG_MATRIX,
	GF_EVG_MATRIX_3D,
	GF_EVG_CLIPPER,
	GF_EVG_COMPOSITE_OP,
	GF_EVG_ALPHA_FUN,
	GF_EVG_FRAG_SHADER,
	GF_EVG_VERT_SHADER,
	GF_EVG_CCW,
	GF_EVG_BACKCULL,
	GF_EVG_MINDEPTH,
	GF_EVG_MAXDEPTH,
	GF_EVG_DEPTHTEST,
	GF_EVG_ANTIALIAS,
	GF_EVG_POINTSIZE,
	GF_EVG_POINTSMOOTH,
	GF_EVG_LINESIZE,
	GF_EVG_CLIP_ZERO,
	GF_EVG_DEPTH_BUFFER,
	GF_EVG_DEPTH_TEST,
	GF_EVG_WRITE_DEPTH,
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

static JSValue canvas_constructor_internal(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv, Bool is_3d)
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
		s32 idx=0;
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

	canvas->mem_size = osize;
	canvas->width = width;
	canvas->height = height;
	canvas->pf = pf;
	canvas->stride = stride;
	canvas->alpha_cbk = JS_UNDEFINED;
	canvas->frag_shader = JS_UNDEFINED;
	canvas->vert_shader = JS_UNDEFINED;
#ifdef EVG_USE_JS_SHADER
	canvas->frag_obj = JS_UNDEFINED;
	canvas->vert_obj = JS_UNDEFINED;
#endif
	canvas->depth_buffer = JS_UNDEFINED;
	canvas->ctx = c;
	if (data) {
		canvas->data = data;
		canvas->owns_data = GF_FALSE;
	} else {
		canvas->data = gf_malloc(sizeof(u8)*osize);
		canvas->owns_data = GF_TRUE;
	}
	if (is_3d) {
		canvas->surface = gf_evg_surface3d_new();
#ifdef EVG_USE_JS_SHADER
		canvas->frag_obj = JS_NewObjectClass(c, fragment_class_id);
		JS_SetOpaque(canvas->frag_obj, NULL);
		canvas->vert_obj = JS_NewObjectClass(c, vertex_class_id);
		JS_SetOpaque(canvas->vert_obj, NULL);
#endif
	} else {
		canvas->surface = gf_evg_surface_new(GF_TRUE);
		canvas->center_coords = GF_TRUE;
	}
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
	canvas->obj = JS_NewObjectClass(c, is_3d ? canvas3d_class_id : canvas_class_id);
	if (JS_IsException(canvas->obj)) return canvas->obj;

	JS_SetOpaque(canvas->obj, canvas);
	return canvas->obj;
}

static JSValue canvas_constructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	return canvas_constructor_internal(c, new_target, argc, argv, GF_FALSE);
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

	case GF_EVG_MATRIX_3D:
		if (JS_IsNull(value)) {
			gf_evg_surface_set_matrix(canvas->surface, NULL);
		} else {
			GF_Matrix *mx = JS_GetOpaque(value, matrix_class_id);
			gf_evg_surface_set_matrix_3d(canvas->surface, mx);
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


static JSValue canvas_clear_ex(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv, Bool use_float, Bool is_3d)
{
	s32 i;
	s32 idx=0;
	GF_Err e;
	GF_IRect rc, *irc;
	u32 r=0, g=0, b=0, a=255;
	GF_Color col;
	GF_JSCanvas *canvas = JS_GetOpaque(obj, is_3d ? canvas3d_class_id : canvas_class_id);
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
	return canvas_clear_ex(c, obj, argc, argv, GF_FALSE, GF_FALSE);
}
static JSValue canvas_clearf(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return canvas_clear_ex(c, obj, argc, argv, GF_TRUE, GF_FALSE);
}

static JSValue canvas_rgb_yuv(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv, Bool to_rgb, Bool is_3d)
{
	GF_Err e;
	Double _r=0, _g=0, _b=0, _a=1.0;
	Float r=0, g=0, b=0, a=1.0;
	Bool as_array = GF_FALSE;
	u32 arg_idx=0;
	Float y, u, v;
	JSValue ret;
	GF_JSCanvas *canvas = JS_GetOpaque(obj, is_3d ? canvas3d_class_id : canvas_class_id);
	if (!canvas || !argc)
		return JS_EXCEPTION;

	if (JS_IsBool(argv[0])) {
		as_array = JS_ToBool(c, argv[0]);
		arg_idx=1;
	}
	if (!get_color_from_args(c, argc, argv, arg_idx, &_a, &_r, &_g, &_b))
		return JS_EXCEPTION;
	r = (Float) _r;
	g = (Float) _g;
	b = (Float) _b;
	a = (Float) _a;
	if (to_rgb) {
		e = gf_evg_yuv_to_rgb_f(canvas->surface, r, g, b, &y, &u, &v);
	} else {
		e = gf_gf_evg_rgb_to_yuv_f(canvas->surface, r, g, b, &y, &u, &v);
	}
	if (e)
		return JS_EXCEPTION;
	if (as_array) {
		ret = JS_NewArray(c);
		JS_SetPropertyStr(c, ret, "length", JS_NewInt32(c, 4) );
		JS_SetPropertyUint32(c, ret, 0, JS_NewFloat64(c, y) );
		JS_SetPropertyUint32(c, ret, 1, JS_NewFloat64(c, u) );
		JS_SetPropertyUint32(c, ret, 2, JS_NewFloat64(c, v) );
		JS_SetPropertyUint32(c, ret, 3, JS_NewFloat64(c, a) );
	} else {
		ret = JS_NewObject(c);
		JS_SetPropertyStr(c, ret, "r", JS_NewFloat64(c, y) );
		JS_SetPropertyStr(c, ret, "g", JS_NewFloat64(c, u) );
		JS_SetPropertyStr(c, ret, "b", JS_NewFloat64(c, v) );
		JS_SetPropertyStr(c, ret, "a", JS_NewFloat64(c, a) );
	}
	return ret;
}

static JSValue canvas_reassign_ex(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv, Bool is_3d)
{
	GF_Err e;
	u8 *data;
	size_t data_size=0;
	GF_JSCanvas *canvas = JS_GetOpaque(obj, is_3d ? canvas3d_class_id : canvas_class_id);
	if (!canvas || !argc) return JS_EXCEPTION;
	if (!JS_IsObject(argv[0])) return JS_EXCEPTION;

	if (canvas->owns_data) {
		gf_free(canvas->data);
		canvas->owns_data = GF_FALSE;
	}
	canvas->data = NULL;
	data = JS_GetArrayBuffer(c, &data_size, argv[0]);
	if (!data || (data_size<canvas->mem_size)) {
		e = GF_BAD_PARAM;
	} else {
		canvas->data = data;
		e = gf_evg_surface_attach_to_buffer(canvas->surface, canvas->data, canvas->width, canvas->height, 0, canvas->stride, canvas->pf);
	}
	if (e) return JS_EXCEPTION;
	return JS_UNDEFINED;
}

static JSValue canvas_reassign(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return canvas_reassign_ex(c, obj, argc, argv, GF_FALSE);
}

static JSValue canvas_toYUV(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return canvas_rgb_yuv(c, obj, argc, argv, GF_FALSE, GF_FALSE);
}

static JSValue canvas_toRGB(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return canvas_rgb_yuv(c, obj, argc, argv, GF_TRUE, GF_FALSE);
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
	JS_CGETSET_MAGIC_DEF("matrix3d", NULL, canvas_setProperty, GF_EVG_MATRIX_3D),
	JS_CGETSET_MAGIC_DEF("compositeOperation", canvas_getProperty, canvas_setProperty, GF_EVG_COMPOSITE_OP),
	JS_CGETSET_MAGIC_DEF("on_alpha", canvas_getProperty, canvas_setProperty, GF_EVG_ALPHA_FUN),
	JS_CFUNC_DEF("clear", 0, canvas_clear),
	JS_CFUNC_DEF("clearf", 0, canvas_clearf),
	JS_CFUNC_DEF("fill", 0, canvas_fill),
	JS_CFUNC_DEF("reassign", 0, canvas_reassign),
	JS_CFUNC_DEF("toYUV", 0, canvas_toYUV),
	JS_CFUNC_DEF("toRGB", 0, canvas_toRGB),
};


static JSValue canvas3d_clear(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return canvas_clear_ex(c, obj, argc, argv, GF_FALSE, GF_TRUE);
}
static JSValue canvas3d_clearf(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return canvas_clear_ex(c, obj, argc, argv, GF_TRUE, GF_TRUE);
}
static JSValue canvas3d_reassign(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return canvas_reassign_ex(c, obj, argc, argv, GF_TRUE);
}

static JSValue canvas3d_toYUV(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return canvas_rgb_yuv(c, obj, argc, argv, GF_FALSE, GF_TRUE);
}

static JSValue canvas3d_toRGB(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return canvas_rgb_yuv(c, obj, argc, argv, GF_TRUE, GF_TRUE);
}

Bool vai_call_lerp(EVG_VAI *vai, GF_EVGFragmentParam *frag);

#ifdef EVG_USE_JS_SHADER
static Bool evg_frag_shader_fun(void *udta, GF_EVGFragmentParam *frag)
{
	Bool frag_valid;
	JSValue res;
	GF_JSCanvas *canvas = (GF_JSCanvas *)udta;
	if (!canvas) return GF_FALSE;

	JS_SetOpaque(canvas->frag_obj, frag);
	res = JS_Call(canvas->ctx, canvas->frag_shader, canvas->obj, 1, &canvas->frag_obj);
	frag_valid = frag->frag_valid ? 1 : 0;
	if (JS_IsException(res)) frag_valid = GF_FALSE;
	else if (!JS_IsUndefined(res)) frag_valid = JS_ToBool(canvas->ctx, res) ? GF_TRUE : GF_FALSE;
	JS_FreeValue(canvas->ctx, res);
	return frag_valid;
}

static Bool evg_vert_shader_fun(void *udta, GF_EVGVertexParam *vertex)
{
	Bool vert_valid=GF_TRUE;
	JSValue res;
	GF_JSCanvas *canvas = (GF_JSCanvas *)udta;
	if (!canvas) return GF_FALSE;

	JS_SetOpaque(canvas->vert_obj, vertex);
	res = JS_Call(canvas->ctx, canvas->frag_shader, canvas->obj, 1, &canvas->vert_obj);
	if (JS_IsException(res)) vert_valid = GF_FALSE;
	else if (!JS_IsUndefined(res)) vert_valid = JS_ToBool(canvas->ctx, res) ? GF_TRUE : GF_FALSE;
	JS_FreeValue(canvas->ctx, res);
	return vert_valid;
}
#endif

static JSValue canvas3d_getProperty(JSContext *c, JSValueConst obj, int magic)
{
	GF_JSCanvas *canvas = JS_GetOpaque(obj, canvas3d_class_id);
	if (!canvas) return JS_EXCEPTION;
	switch (magic) {
	case GF_EVG_FRAG_SHADER: return JS_DupValue(c, canvas->frag_shader);
	case GF_EVG_VERT_SHADER: return JS_DupValue(c, canvas->vert_shader);
	case GF_EVG_DEPTH_BUFFER: return JS_DupValue(c, canvas->depth_buffer);
	}
	return JS_UNDEFINED;
}

static Bool evg_frag_shader_ops(void *udta, GF_EVGFragmentParam *frag);
static Bool evg_vert_shader_ops(void *udta, GF_EVGVertexParam *frag);

static JSValue canvas3d_setProperty(JSContext *ctx, JSValueConst obj, JSValueConst value, int magic)
{
	Float f;
	s32 ival;
	GF_Err e = GF_OK;
	GF_JSCanvas *canvas = JS_GetOpaque(obj, canvas3d_class_id);
	if (!canvas) return JS_EXCEPTION;
	switch (magic) {
	case GF_EVG_CLIPPER:
		if (JS_IsNull(value)) {
			e = gf_evg_surface_set_clipper(canvas->surface, NULL);
		} else {
			GF_IRect rc;
			canvas_get_irect(ctx, value, &rc);
			e = gf_evg_surface_set_clipper(canvas->surface, &rc);
		}
		break;
	case GF_EVG_FRAG_SHADER:
		JS_FreeValue(ctx, canvas->frag_shader);
		canvas->frag_shader = JS_UNDEFINED;
		canvas->frag = NULL;
		if (JS_IsNull(value)) {
			canvas->frag_shader = JS_UNDEFINED;
			e = gf_evg_surface_set_fragment_shader(canvas->surface, NULL, NULL);
#ifdef EVG_USE_JS_SHADER
		} else if (JS_IsFunction(ctx, value)) {
			canvas->frag_shader = JS_DupValue(ctx, value);
			e = gf_evg_surface_set_fragment_shader(canvas->surface, evg_frag_shader_fun, canvas);
#endif
		} else if (JS_IsObject(value)) {
			canvas->frag = JS_GetOpaque(value, shader_class_id);
			if (!canvas->frag || (canvas->frag->mode != GF_EVG_SHADER_FRAGMENT))
				return js_throw_err_msg(ctx, GF_BAD_PARAM, "Invalid fragment shader object");
			canvas->frag_shader = JS_DupValue(ctx, value);
			e = gf_evg_surface_set_fragment_shader(canvas->surface, evg_frag_shader_ops, canvas);
			if (!e) e = gf_evg_surface_disable_early_depth(canvas->surface, canvas->frag->disable_early_z);
		} else {
			canvas->frag_shader = JS_UNDEFINED;
			e = gf_evg_surface_set_fragment_shader(canvas->surface, NULL, NULL);
		}
		break;
	case GF_EVG_VERT_SHADER:
		JS_FreeValue(ctx, canvas->vert_shader);
		canvas->vert_shader = JS_UNDEFINED;
		canvas->vert = NULL;
		if (JS_IsNull(value)) {
			canvas->vert_shader = JS_UNDEFINED;
			e = gf_evg_surface_set_vertex_shader(canvas->surface, NULL, NULL);
#ifdef EVG_USE_JS_SHADER
		} else if (JS_IsFunction(ctx, value)) {
			canvas->vert_shader = JS_DupValue(ctx, value);
			e = gf_evg_surface_set_vertex_shader(canvas->surface, evg_vert_shader_fun, canvas);
#endif
		} else if (JS_IsObject(value)) {
			canvas->vert = JS_GetOpaque(value, shader_class_id);
			if (!canvas->vert || (canvas->vert->mode != GF_EVG_SHADER_VERTEX))
				return js_throw_err_msg(ctx, GF_BAD_PARAM, "Invalid fragment shader object");
			canvas->vert_shader = JS_DupValue(ctx, value);
			e = gf_evg_surface_set_vertex_shader(canvas->surface, evg_vert_shader_ops, canvas);
		} else {
			canvas->frag_shader = JS_UNDEFINED;
			e = gf_evg_surface_set_fragment_shader(canvas->surface, NULL, NULL);
		}
		break;
	case GF_EVG_CCW:
		e = gf_evg_surface_set_ccw(canvas->surface, JS_ToBool(ctx, value) );
		break;
	case GF_EVG_BACKCULL:
		e = gf_evg_surface_set_backcull(canvas->surface, JS_ToBool(ctx, value) );
		break;
	case GF_EVG_ANTIALIAS:
		e = gf_evg_surface_set_antialias(canvas->surface, JS_ToBool(ctx, value) );
		break;

	case GF_EVG_DEPTH_BUFFER:
	{
		Float *depthb;
		u32 dsize;
		JS_FreeValue(ctx, canvas->depth_buffer);
		canvas->depth_buffer = JS_UNDEFINED;
		depthb = (Float *) evg_get_array(ctx, value, &dsize);
		if (!depthb) {
			canvas->depth_buffer = JS_NULL;
			e = gf_evg_surface_set_depth_buffer(canvas->surface, NULL);
		} else {
			u32 req_size = canvas->width*canvas->height*sizeof(Float);
			if (req_size>dsize) {
				e = GF_BAD_PARAM;
				gf_evg_surface_set_depth_buffer(canvas->surface, NULL);
			} else {
				canvas->depth_buffer = JS_DupValue(ctx, value);
				e = gf_evg_surface_set_depth_buffer(canvas->surface, depthb);
			}
		}
		break;
	}

	case GF_EVG_MINDEPTH:
		EVG_GET_FLOAT(f, value);
		e = gf_evg_surface_set_min_depth(canvas->surface, f);
		break;
	case GF_EVG_MAXDEPTH:
		EVG_GET_FLOAT(f, value);
		e = gf_evg_surface_set_max_depth(canvas->surface, f);
		break;
	case GF_EVG_DEPTHTEST:
		if (JS_ToInt32(ctx, &ival, value))
			return js_throw_err(ctx, GF_BAD_PARAM);
		e = gf_evg_set_depth_test(canvas->surface, ival);
		break;
	case GF_EVG_POINTSIZE:
		EVG_GET_FLOAT(f, value);
		e = gf_evg_surface_set_point_size(canvas->surface, f);
		break;
	case GF_EVG_POINTSMOOTH:
		e = gf_evg_surface_set_point_smooth(canvas->surface, JS_ToBool(ctx, value) );
		break;
	case GF_EVG_LINESIZE:
		EVG_GET_FLOAT(f, value);
		e = gf_evg_surface_set_line_size(canvas->surface, f);
		break;
	case GF_EVG_CLIP_ZERO:
		e = gf_evg_surface_set_clip_zero(canvas->surface, JS_ToBool(ctx, value) ? GF_TRUE : GF_FALSE);
		break;
	case GF_EVG_DEPTH_TEST:
		if (JS_ToInt32(ctx, &ival, value)) return JS_EXCEPTION;
		e = gf_evg_set_depth_test(canvas->surface, ival);
		break;
	case GF_EVG_WRITE_DEPTH:
		e = gf_evg_surface_write_depth(canvas->surface, JS_ToBool(ctx, value) ? GF_TRUE : GF_FALSE);
		break;
	}
	if (e) return js_throw_err(ctx, e);

	return JS_UNDEFINED;
}

static JSValue canvas3d_set_matrix(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv, Bool is_proj)
{
	GF_Err e;
	GF_Matrix mx;
	GF_JSCanvas *canvas = JS_GetOpaque(obj, canvas3d_class_id);
	if (!canvas) return JS_EXCEPTION;

	gf_mx_init(mx);
	if (argc) {
		if (JS_IsArray(c, argv[0])) {
			u32 i, len=0;
			JSValue v = JS_GetPropertyStr(c, argv[0], "length");
			JS_ToInt32(c, &len, v);
			JS_FreeValue(c, v);
			if (len < 16) return JS_EXCEPTION;
			for (i=0; i<16; i++) {
				Double val=0;
				v = JS_GetPropertyUint32(c, argv[0], i);
				s32 res = JS_ToFloat64(c, &val, v);
				JS_FreeValue(c, v);
				if (res) return JS_EXCEPTION;
				mx.m[i] =  FLT2FIX(val);
			}
		} else {
			return js_throw_err_msg(c, GF_NOT_SUPPORTED, "only float array currently supported for matrices");
		}
	}
	if (is_proj)
		e = gf_evg_surface_set_projection(canvas->surface, &mx);
	else
		e = gf_evg_surface_set_modelview(canvas->surface, &mx);
	return e ? JS_EXCEPTION : JS_UNDEFINED;
}
static JSValue canvas3d_projection(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return canvas3d_set_matrix(c, obj, argc, argv, GF_TRUE);
}
static JSValue canvas3d_modelview(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return canvas3d_set_matrix(c, obj, argc, argv, GF_FALSE);
}

uint8_t *evg_get_array(JSContext *ctx, JSValueConst obj, u32 *size)
{
	JSValue v;
	/*ArrayBuffer*/
	size_t psize;
	uint8_t *res = JS_GetArrayBuffer(ctx, &psize, obj);
	if (res) {
		*size = (u32) psize;
		return res;
	}
	/*ArrayView*/
	v = JS_GetPropertyStr(ctx, obj, "buffer");
	if (JS_IsUndefined(v)) return NULL;
	res = JS_GetArrayBuffer(ctx, &psize, v);
	JS_FreeValue(ctx, v);
	*size = (u32) psize;
	return res;
}
static JSValue canvas3d_draw_array(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	uint8_t *indices=NULL;
	uint8_t *vertices=NULL;
	u32 idx_size=0, vx_size, nb_comp=3;
	GF_Err e;
	GF_EVGPrimitiveType prim_type=GF_EVG_TRIANGLES;
	GF_JSCanvas *canvas = JS_GetOpaque(obj, canvas3d_class_id);
	if (!canvas || argc<2) return JS_EXCEPTION;

	indices = evg_get_array(c, argv[0], &idx_size);
	vertices = evg_get_array(c, argv[1], &vx_size);
	if (!indices || ! vertices) return JS_EXCEPTION;
	if (argc>2) {
		JS_ToInt32(c, (s32 *)&prim_type, argv[2]);
		if (!prim_type) return JS_EXCEPTION;
		if (argc>3) {
			JS_ToInt32(c, &nb_comp, argv[3]);
			if ((nb_comp<2) || (nb_comp>4)) return JS_EXCEPTION;
		}
	}
	if (vx_size % nb_comp)
		return JS_EXCEPTION;
	idx_size /= sizeof(s32);
	vx_size /= sizeof(Float);
	e = gf_evg_surface_draw_array(canvas->surface, (u32 *)indices, idx_size, (Float *)vertices, vx_size, nb_comp, prim_type);
	if (e) return JS_EXCEPTION;

	return JS_UNDEFINED;
}

static void text_update_path(GF_JSText *txt, Bool for_centered);

static JSValue canvas3d_draw_path(JSContext *ctx, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_Err e = GF_OK;
	Float z = 0;
	GF_JSCanvas *canvas = JS_GetOpaque(obj, canvas3d_class_id);
	if (!canvas || argc<1) return JS_EXCEPTION;
	if (argc>1) {
		EVG_GET_FLOAT(z, argv[1]);
	}

	GF_Path *gp = JS_GetOpaque(argv[0], path_class_id);
	if (gp) {
		e = gf_evg_surface_draw_path(canvas->surface, gp, z);
	} else {
		GF_JSText *text = JS_GetOpaque(argv[0], text_class_id);
		if (text) {
			text_update_path(text, GF_TRUE);
			e = gf_evg_surface_draw_path(canvas->surface, text->path, z);
		}
	}
	if (e) return js_throw_err(ctx, e);
	return JS_UNDEFINED;
}
static JSValue canvas3d_clear_depth(JSContext *ctx, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_Err e;
	Float depth = 1.0;
	GF_JSCanvas *canvas = JS_GetOpaque(obj, canvas3d_class_id);
	if (!canvas) return JS_EXCEPTION;
	if (argc)
		EVG_GET_FLOAT(depth, argv[0]);

	e = gf_evg_surface_clear_depth(canvas->surface, depth);
	if (e) return JS_EXCEPTION;
	return JS_UNDEFINED;
}
static JSValue canvas3d_viewport(JSContext *ctx, JSValueConst obj, int argc, JSValueConst *argv)
{
	s32 x, y, w, h;
	GF_Err e;
	GF_JSCanvas *canvas = JS_GetOpaque(obj, canvas3d_class_id);
	if (!canvas) return JS_EXCEPTION;
	if (argc) {
		if (argc<4) return js_throw_err(ctx, GF_BAD_PARAM);
		if (JS_ToInt32(ctx, &x, argv[0])) return js_throw_err(ctx, GF_BAD_PARAM);;
		if (JS_ToInt32(ctx, &y, argv[1])) return js_throw_err(ctx, GF_BAD_PARAM);;
		if (JS_ToInt32(ctx, &w, argv[2])) return js_throw_err(ctx, GF_BAD_PARAM);;
		if (JS_ToInt32(ctx, &h, argv[3])) return js_throw_err(ctx, GF_BAD_PARAM);;
	} else {
		x = y = 0;
		w = canvas->width;
		h = canvas->height;
	}
	e = gf_evg_surface_viewport(canvas->surface, x, y, w, h);
	if (e) return JS_EXCEPTION;
	return JS_UNDEFINED;
}


enum
{
	EVG_OP_IF=1,
	EVG_OP_ELSE,
	EVG_OP_ELSEIF,
	EVG_OP_END,
	EVG_OP_GOTO,
	EVG_OP_ASSIGN,
	EVG_OP_DISCARD,
	EVG_OP_ADD,
	EVG_OP_SUB,
	EVG_OP_MUL,
	EVG_OP_DIV,
	EVG_OP_NEG,
	EVG_OP_LESS,
	EVG_OP_LESS_EQUAL,
	EVG_OP_GREATER,
	EVG_OP_GREATER_EQUAL,
	EVG_OP_EQUAL,
	EVG_OP_NOT_EQUAL,
	EVG_OP_SAMPLER,
	EVG_OP_SAMPLER_YUV,

	EVG_OP_PRINT,

	EVG_OP_NORMALIZE,
	EVG_OP_LENGTH,
	EVG_OP_DISTANCE,
	EVG_OP_DOT,
	EVG_OP_CROSS,
	EVG_OP_POW,
	EVG_OP_SIN,
	EVG_OP_ASIN,
	EVG_OP_COS,
	EVG_OP_ACOS,
	EVG_OP_TAN,
	EVG_OP_ATAN,
	EVG_OP_LOG,
	EVG_OP_EXP,
	EVG_OP_LOG2,
	EVG_OP_EXP2,
	EVG_OP_SINH,
	EVG_OP_COSH,
	EVG_OP_SQRT,
	EVG_OP_INVERSE_SQRT,
	EVG_OP_ABS,
	EVG_OP_SIGN,
	EVG_OP_FLOOR,
	EVG_OP_CEIL,
	EVG_OP_FRACT,
	EVG_OP_MOD,
	EVG_OP_MIN,
	EVG_OP_MAX,
	EVG_OP_CLAMP,
	EVG_OP_RGB2YUV,
	EVG_OP_YUV2RGB,

	EVG_FIRST_VAR_ID
};

enum
{
	VAR_FRAG_ARGB=1,
	VAR_FRAG_YUV,
	VAR_FRAG_X,
	VAR_FRAG_Y,
	VAR_FRAG_DEPTH,
	VAR_FRAG_W,
	VAR_UNIFORM,
	VAR_VERTEX_IN,
	VAR_VERTEX_OUT,
	VAR_VAI,
	VAR_VA,
	VAR_MATRIX,
};


static GFINLINE Float isqrtf(Float v)
{
	v = sqrtf(v);
	if (v) v = 1 / v;
	return v;
}
static GFINLINE Float signf(Float v)
{
	if (v==0) return 0;
	if (v>0) return 1.0;
	return -1.0;
}
static GFINLINE Float fractf(Float v)
{
	return v - floorf(v);
}
static GFINLINE Float _modf(Float x, Float y)
{
	if (!y) return 0.0;
	return x - y * floorf(x/y);
}



#if defined(WIN32) && !defined(__GNUC__)
# include <intrin.h>
# define GPAC_HAS_SSE2
#else
# ifdef __SSE2__
#  include <emmintrin.h>
#  define GPAC_HAS_SSE2
# endif
#endif

#ifdef GPAC_HAS_SSE2

static Float evg_float_clamp(Float val, Float minval, Float maxval)
{
    _mm_store_ss( &val, _mm_min_ss( _mm_max_ss(_mm_set_ss(val),_mm_set_ss(minval)), _mm_set_ss(maxval) ) );
    return val;
}
#else

#define evg_float_clamp(_val, _minval, _maxval)\
	(_val<_minval) ? _minval : (_val>_maxval) ? _maxval : _val;

#endif


/*

*/

static Bool evg_shader_ops(GF_JSCanvas *canvas, EVGShader *shader, GF_EVGFragmentParam *frag, GF_EVGVertexParam *vert)
{
	u32 op_idx, dim;
	GF_Vec4 tmpl, tmpr;
	GF_Vec4 *left_val, *right_val, *right2_val;
	u32 if_level=0;
	u32 nif_level=0;
	Bool cond_res;

	//assign to dummy values, this will prevent any badly formated shader to assign a value to a NULL left-val or read a null right-val
	tmpl.x = tmpl.y = tmpl.z = tmpl.q = 0;
	left_val = &tmpl;
	tmpr.x = tmpr.y = tmpr.z = tmpr.q = 0;
	right_val = &tmpr;

	for (op_idx=0; op_idx<shader->nb_ops; op_idx++) {
		u32 next_idx, idx, var_idx;
		Bool has_next, norm_result=GF_FALSE;
		u8 right_val_type, left_val_type=0;
		u8 *left_val_type_ptr=NULL;
		ShaderOp *op = &shader->ops[op_idx];

		if (op->op_type==EVG_OP_GOTO) {
			u32 stack_idx = op->left_value;
			if (op->uni_name) stack_idx = op->ival;

			if (!stack_idx || (stack_idx > shader->nb_ops)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[Shader] Invalid goto operation, stack index %d not in stack indices [1, %d]\n", op->left_value, shader->nb_ops));
				shader->invalid = GF_TRUE;
				return GF_FALSE;
			}
			op_idx = stack_idx - 1;
			op = &shader->ops[op_idx];
		}

		if (op->op_type==EVG_OP_ELSE) {
			if (nif_level) {
				if (nif_level==1) {
					nif_level=0;
					if_level++;
				}
			} else if (if_level) {
				if_level--;
				nif_level++;
			}
			continue;
		}
		if (op->op_type==EVG_OP_END) {
			assert(nif_level || if_level);
			if (nif_level) nif_level--;
			else if (if_level) if_level--;
			continue;
		}
		if (nif_level) {
			if (op->op_type==EVG_OP_IF) {
				nif_level++;
			}
			continue;
		}

		dim=4;
		if (op->left_value==VAR_FRAG_ARGB) {
			left_val = &frag->color;
			left_val_type = COMP_V4;
			frag->frag_valid = GF_EVG_FRAG_RGB;
		} else if (op->left_value==VAR_FRAG_YUV) {
			left_val = &frag->color;
			left_val_type = COMP_V4;
			frag->frag_valid = GF_EVG_FRAG_YUV;
		} else if (op->left_value==VAR_FRAG_X) {
			left_val = &tmpl;
			tmpl.x = frag->screen_x;
			left_val_type = COMP_FLOAT;
		} else if (op->left_value==VAR_FRAG_Y) {
			left_val = &tmpl;
			tmpl.x = frag->screen_y;
			left_val_type = COMP_FLOAT;
		} else if (op->left_value==VAR_FRAG_W) {
			left_val = &tmpl;
			tmpl.x = frag->persp_denum;
			left_val_type = COMP_FLOAT;
		} else if (op->left_value==VAR_FRAG_DEPTH) {
			left_val = (GF_Vec4 *) &frag->depth;
			left_val_type = COMP_FLOAT;
		} else if (op->left_value==VAR_VERTEX_IN) {
			left_val = (GF_Vec4 *) &vert->in_vertex;
			left_val_type = COMP_V4;
		} else if (op->left_value==VAR_VERTEX_OUT) {
			left_val = (GF_Vec4 *) &vert->out_vertex;
			left_val_type = COMP_V4;
		} else if (op->left_value==VAR_VAI) {
			left_val = (GF_Vec4 *) &op->vai.vai->anchors[vert->vertex_idx_in_prim];
			left_val_type = COMP_V4;
			norm_result = op->vai.vai->normalize;
		} else if (op->left_value) {
			u32 l_var_idx = op->left_value - EVG_FIRST_VAR_ID-1;
			left_val = &shader->vars[l_var_idx].vecval;
			left_val_type = shader->vars[l_var_idx].value_type;
			left_val_type_ptr = & shader->vars[l_var_idx].value_type;
		}

		if (op->right_value>EVG_FIRST_VAR_ID) {
			var_idx = op->right_value - EVG_FIRST_VAR_ID-1;
			right_val = &shader->vars[var_idx].vecval;
			right_val_type = shader->vars[var_idx].value_type;
		} else if ((op->right_value==VAR_VAI) && op->vai.vai) {
			vai_call_lerp(op->vai.vai, frag);
			dim = MIN(4, op->vai.vai->result.dim);
			right_val = (GF_Vec4 *) &op->vai.vai->result.values[0];
			right_val_type = op->vai.vai->result.comp_type;
		} else if ((op->right_value==VAR_VA) && op->va.va) {
			u32 va_idx, j, nb_v_per_prim=3;
			EVG_VA *va = op->va.va;

			if (vert->ptype == GF_EVG_LINES)
				nb_v_per_prim=2;
			else if (vert->ptype == GF_EVG_POINTS)
				nb_v_per_prim=1;

			if (va->interp_type==GF_EVG_VAI_PRIMITIVE) {
				va_idx = vert->prim_index * va->nb_comp;
			}
			else if (va->interp_type==GF_EVG_VAI_VERTEX_INDEX) {
				va_idx = vert->vertex_idx * va->nb_comp;
			} else {
				va_idx = vert->prim_index * nb_v_per_prim * va->nb_comp;
			}

			if (va_idx+va->nb_comp > va->nb_values)
				return GF_FALSE;

			right_val = &tmpr;
			right_val->x = right_val->y = right_val->z = right_val->q = 0;
			assert(va->nb_comp<=4);
			for (j=0; j<va->nb_comp; j++) {
				((Float *)right_val)[j] = va->values[va_idx+j];
			}
			if (va->normalize) {
				if (va->nb_comp==2) {
					Float len;
					if (!right_val->x) len = ABS(right_val->y);
					else if (!right_val->y) len = ABS(right_val->x);
					else len = sqrtf(right_val->x*right_val->x + right_val->y*right_val->y);
					if (len) {
						right_val->x/=len;
						right_val->y/=len;
					}
				} else {
					gf_vec_norm((GF_Vec *) right_val);
				}

			}
			right_val_type = va->att_type;
		} else if ((op->right_value==VAR_MATRIX) && op->mx.mx) {
			if (op->op_type==EVG_OP_MUL) {
				gf_mx_apply_vec_4x4(op->mx.mx, left_val);
				continue;
			}
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[Shader] Invalid operation for right value matrix\n"));
			shader->invalid = GF_TRUE;
			return GF_FALSE;
		} else if (op->right_value==VAR_FRAG_ARGB) {
			right_val = &frag->color;
			right_val_type = COMP_V4;
		} else if (op->right_value==VAR_FRAG_YUV) {
			right_val = &frag->color;
			right_val_type = COMP_V4;
		} else if (op->right_value==VAR_FRAG_X) {
			right_val = &tmpr;
			tmpr.x = frag->screen_x;
			right_val_type = COMP_FLOAT;
		} else if (op->right_value==VAR_FRAG_Y) {
			right_val = &tmpr;
			tmpr.x = frag->screen_y;
			right_val_type = COMP_FLOAT;
		} else if (op->right_value==VAR_FRAG_W) {
			right_val = &tmpr;
			tmpr.x = frag->persp_denum;
			right_val_type = COMP_FLOAT;
		} else if (op->right_value==VAR_FRAG_DEPTH) {
			right_val = &tmpr;
			tmpr.x = frag->depth;
			right_val_type = COMP_FLOAT;
		} else if (op->right_value==VAR_VERTEX_IN) {
			right_val = &vert->in_vertex;
			right_val_type = COMP_V4;
		} else if (op->right_value==VAR_VERTEX_OUT) {
			right_val = &vert->out_vertex;
			right_val_type = COMP_V4;
		} else if (!op->right_value || (op->right_value==VAR_UNIFORM) ) {
			right_val = (GF_Vec4 *) &op->vec[0];
			right_val_type = op->right_value_type;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[Shader] Invalid right-value in operation %d\n", op_idx));
			shader->invalid = GF_TRUE;
			return GF_FALSE;
		}
		if (!right_val_type) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[Shader] Invalid right-value type in operation %d\n", op_idx));
			shader->invalid = GF_TRUE;
			return GF_FALSE;
		}

#define GET_FIRST_COMP\
					idx=0;\
					while (1) {\
						if (op->right_value_type & (1<<idx))\
							break;\
						if (idx==3)\
							break;\
						idx++;\
					}\

#define GET_NEXT_COMP\
					has_next = GF_FALSE;\
					next_idx = idx;\
					while (1) {\
						if (next_idx==3)\
							break;\
						if (op->right_value_type & (1<< (next_idx+1)) ) {\
							has_next = GF_TRUE;\
							next_idx = idx+1;\
							break;\
						}\
						next_idx++;\
					}\
					if (has_next) idx = next_idx;\

		switch (op->op_type) {
		case EVG_OP_ASSIGN:
			//full assignment
			if (op->left_value_type==COMP_V4) {
				if (left_val_type_ptr) {
					left_val_type = *left_val_type_ptr = right_val_type;
				}

				if (right_val_type==COMP_BOOL) {
					if (left_val_type_ptr) {
						*((Bool *) left_val) = *((Bool *) right_val) ? GF_TRUE : GF_FALSE;
					} else {
						left_val->x = (Float) ( *(Bool *) right_val ? 1.0 : 0.0 );
						left_val->y = left_val->z = left_val->q = left_val->x;
					}
				} else if (right_val_type==COMP_INT) {
					if (left_val_type_ptr || (left_val_type==COMP_INT)) {
						*((s32 *) left_val) = *(s32 *) right_val;
					} else {
						left_val->x = (Float) *(s32 *) right_val;
						left_val->y = left_val->z = left_val->q = left_val->x;
					}
				} else if (right_val_type==COMP_FLOAT) {
					if (left_val_type_ptr) {
						*((Float *) left_val) = *(Float *) right_val;
					} else {
						left_val->x = *(Float *) right_val;
						left_val->y = left_val->z = left_val->q = left_val->x;
					}
				} else if ((right_val_type==COMP_V4) || (op->right_value_type==COMP_V4)) {
					*left_val = *right_val;
					if (dim<4) left_val->q = 1.0;
				} else {
					Float *srcs = (Float *) &right_val->x;

					GET_FIRST_COMP
					if (left_val_type & COMP_X) { left_val->x = srcs[idx]; GET_NEXT_COMP }
					if (left_val_type & COMP_Y) { left_val->y = srcs[idx]; GET_NEXT_COMP }
					if (left_val_type & COMP_Z) { left_val->z = srcs[idx]; GET_NEXT_COMP }
					if (dim<4) left_val->q = 1.0;
					else if (left_val_type & COMP_Q) { left_val->q = srcs[idx]; }
				}
			}
			//partial assignment, only valid for float sources
			else {
				Bool use_const = GF_FALSE;
				Float cval;
				if (right_val_type==COMP_FLOAT) {
					use_const = GF_TRUE;
					cval = right_val->x;
				} else if (right_val_type==COMP_INT) {
					use_const = GF_TRUE;
					cval = (Float) ( *(s32*)right_val);
				}
				if (use_const) {
					if (op->left_value_type & COMP_X) left_val->x = cval;
					if (op->left_value_type & COMP_Y) left_val->y = cval;
					if (op->left_value_type & COMP_Z) left_val->z = cval;
					if (op->left_value_type & COMP_Q) left_val->q = cval;
				} else {
					Float *srcs = (Float *) &right_val->x;

					GET_FIRST_COMP
					if (op->left_value_type & COMP_X) { left_val->x = srcs[idx]; GET_NEXT_COMP }
					if (op->left_value_type & COMP_Y) { left_val->y = srcs[idx]; GET_NEXT_COMP }
					if (op->left_value_type & COMP_Z) { left_val->z = srcs[idx]; GET_NEXT_COMP }
					if (op->left_value_type & COMP_Q) { left_val->q = srcs[idx]; }
				}
			}
			if (norm_result)
				gf_vec_norm((GF_Vec *)left_val);
			break;

#define BASE_OP(_opv, _opv2)\
			if (op->left_value_type==COMP_V4) {\
				if (right_val_type==COMP_INT) {\
					if (left_val_type == COMP_INT) {\
						*((s32 *) left_val) _opv *(s32 *) right_val;\
					} else if (left_val_type == COMP_FLOAT) {\
						left_val->x _opv *(s32 *) right_val;\
					} else {\
						left_val->x _opv *(s32 *) right_val;\
						left_val->y _opv *(s32 *) right_val;\
						left_val->z _opv *(s32 *) right_val;\
						left_val->q _opv *(s32 *) right_val;\
					}\
				} else if (right_val_type==COMP_FLOAT) {\
					if (left_val_type == COMP_INT) {\
						left_val->x = *((s32 *) left_val) _opv2 right_val->x;\
						*left_val_type_ptr = COMP_FLOAT;\
					} else if (left_val_type == COMP_FLOAT) {\
						left_val->x _opv right_val->x;\
					} else {\
						left_val->x _opv right_val->x;\
						left_val->y _opv right_val->x;\
						left_val->z _opv right_val->x;\
						left_val->q _opv right_val->x;\
					}\
				} else if (right_val_type==COMP_BOOL) {\
				} else {\
					Float *srcs = (Float *) &right_val->x;\
					GET_FIRST_COMP\
					if (left_val_type & COMP_X) { left_val->x _opv srcs[idx]; GET_NEXT_COMP } \
					if (left_val_type & COMP_Y) { left_val->y _opv srcs[idx]; GET_NEXT_COMP } \
					if (left_val_type & COMP_Z) { left_val->z _opv srcs[idx]; GET_NEXT_COMP } \
					if (left_val_type & COMP_Q) { left_val->q _opv srcs[idx]; }\
				}\
			}\
			else {\
				Bool use_const = GF_FALSE;\
				Float cval;\
				if (right_val_type==COMP_FLOAT) {\
					use_const = GF_TRUE;\
					cval = right_val->x;\
				} else if (right_val_type==COMP_INT) {\
					use_const = GF_TRUE;\
					cval = (Float) ( *(s32*)right_val);\
				}\
				if (use_const) {\
					if (op->left_value_type & COMP_X) left_val->x _opv cval;\
					if (op->left_value_type & COMP_Y) left_val->y _opv cval;\
					if (op->left_value_type & COMP_Z) left_val->z _opv cval;\
					if (op->left_value_type & COMP_Q) left_val->q _opv cval;\
				} else {\
					Float *srcs = (Float *) &right_val->x;\
					GET_FIRST_COMP\
					if (op->left_value_type & COMP_X) { left_val->x _opv srcs[idx]; GET_NEXT_COMP }\
					if (op->left_value_type & COMP_Y) { left_val->y _opv srcs[idx]; GET_NEXT_COMP }\
					if (op->left_value_type & COMP_Z) { left_val->z _opv srcs[idx]; GET_NEXT_COMP }\
					if (op->left_value_type & COMP_Q) left_val->q _opv srcs[idx];\
				}\
			}\

		case EVG_OP_MUL:
			BASE_OP(*=, *)
			break;
		case EVG_OP_DIV:
			BASE_OP(/=, /)
			break;
		case EVG_OP_ADD:
			BASE_OP(+=, +)
			break;
		case EVG_OP_SUB:
			BASE_OP(-=, -)
			break;
		case EVG_OP_NEG:
			BASE_OP(= !, *)
			break;
		case EVG_OP_IF:
#define BASE_COND(_opv)\
		cond_res=GF_FALSE;\
		if (op->left_value_type==COMP_V4) {\
			if ((right_val_type==COMP_INT) || (right_val_type==COMP_BOOL)) {\
				if (left_val_type == COMP_INT) {\
					cond_res = ( *((s32 *) left_val) _opv *(s32 *) right_val) ? GF_TRUE : GF_FALSE;\
				} else if (left_val_type == COMP_FLOAT) {\
					cond_res = (left_val->x _opv *(s32 *) right_val) ? GF_TRUE : GF_FALSE;\
				} else {\
					cond_res = ( (left_val->x _opv *(s32 *) right_val) && \
							(left_val->y _opv *(s32 *) right_val) && \
							(left_val->z _opv *(s32 *) right_val) && \
							(left_val->q _opv *(s32 *) right_val) ) ? GF_TRUE : GF_FALSE;\
				}\
			} else if (right_val_type==COMP_FLOAT) {\
				if (left_val_type == COMP_INT) {\
					cond_res = ( *((s32 *) left_val) _opv right_val->x) ? GF_TRUE : GF_FALSE;\
				} else if (left_val_type == COMP_FLOAT) {\
					cond_res = (left_val->x _opv right_val->x) ? GF_TRUE : GF_FALSE;\
				} else {\
					cond_res = ( (left_val->x _opv right_val->x) &&\
							(left_val->y _opv right_val->x) &&\
							(left_val->z _opv right_val->x) && \
							(left_val->q _opv right_val->x) ) ? GF_TRUE : GF_FALSE;\
				}\
			} else {\
				cond_res=GF_TRUE;\
				Float *srcs = (Float *) &right_val->x;\
				GET_FIRST_COMP\
				if (left_val_type & COMP_X) { if (! (left_val->x _opv srcs[idx]) ) cond_res = GF_FALSE; GET_NEXT_COMP } \
				if (left_val_type & COMP_Y) { if (! (left_val->y _opv srcs[idx]) ) cond_res = GF_FALSE; GET_NEXT_COMP } \
				if (left_val_type & COMP_Z) { if (! (left_val->z _opv srcs[idx]) ) cond_res = GF_FALSE; GET_NEXT_COMP } \
				if (left_val_type & COMP_Q) { if (! (left_val->q _opv srcs[idx]) ) cond_res = GF_FALSE; }\
			}\
		}\
		else {\
			Bool use_const = GF_FALSE;\
			Float cval;\
			if (right_val_type==COMP_FLOAT) {\
				use_const = GF_TRUE;\
				cval = right_val->x;\
			} else if (right_val_type==COMP_INT) {\
				use_const = GF_TRUE;\
				cval = (Float) ( *(s32*)right_val);\
			}\
			if (use_const) {\
				cond_res=GF_TRUE;\
				if (op->left_value_type & COMP_X) if (! (left_val->x _opv cval) ) cond_res=GF_FALSE;\
				if (op->left_value_type & COMP_Y) if (! (left_val->y _opv cval) ) cond_res=GF_FALSE;\
				if (op->left_value_type & COMP_Z) if (! (left_val->z _opv cval) ) cond_res=GF_FALSE;\
				if (op->left_value_type & COMP_Q) if (! (left_val->q _opv cval) ) cond_res=GF_FALSE;\
			} else {\
				Float *srcs = (Float *) &right_val->x;\
				GET_FIRST_COMP\
				if (op->left_value_type & COMP_X) { if (! (left_val->x _opv srcs[idx]) ) cond_res=GF_FALSE; GET_NEXT_COMP }\
				if (op->left_value_type & COMP_Y) { if (! (left_val->y _opv srcs[idx]) ) cond_res=GF_FALSE; GET_NEXT_COMP }\
				if (op->left_value_type & COMP_Z) { if (! (left_val->z _opv srcs[idx]) ) cond_res=GF_FALSE; GET_NEXT_COMP }\
				if (op->left_value_type & COMP_Q) { if (! (left_val->q _opv srcs[idx]) ) cond_res=GF_FALSE; }\
			}\
		}

			if (op->cond_type==EVG_OP_LESS) { BASE_COND(<) }
			else if (op->cond_type==EVG_OP_LESS_EQUAL) { BASE_COND(<=) }
			else if (op->cond_type==EVG_OP_GREATER) { BASE_COND(>) }
			else if (op->cond_type==EVG_OP_GREATER_EQUAL) { BASE_COND(>=) }
			else if (op->cond_type==EVG_OP_EQUAL) { BASE_COND(==) }
			else if (op->cond_type==EVG_OP_NOT_EQUAL) { BASE_COND(!=) }
			else break;

			if (cond_res) if_level++;
			else nif_level++;

			break;

		case EVG_OP_SAMPLER:
			if (left_val_type_ptr) {
				*left_val_type_ptr = COMP_V4;
			}
			if (op->left_value_type==COMP_V4) {
				gf_evg_stencil_get_pixel_f(op->tx->stencil, right_val->x, right_val->y, &left_val->x, &left_val->y, &left_val->z, &left_val->q);
			} else {
				gf_evg_stencil_get_pixel_f(op->tx->stencil, right_val->x, right_val->y, &tmpr.x, &tmpr.y, &tmpr.z, &tmpr.q);
				if (op->left_value_type & COMP_X) left_val->x = tmpr.x;
				if (op->left_value_type & COMP_Y) left_val->y = tmpr.y;
				if (op->left_value_type & COMP_Z) left_val->z = tmpr.z;
				if (op->left_value_type & COMP_Q) left_val->q = tmpr.q;
			}
			break;

		case EVG_OP_SAMPLER_YUV:
			if (left_val_type_ptr) {
				*left_val_type_ptr = COMP_V4;
			}
			if (op->left_value_type==COMP_V4) {
				gf_evg_stencil_get_pixel_yuv_f(op->tx->stencil, right_val->x, right_val->y, &left_val->x, &left_val->y, &left_val->z, &left_val->q);
			} else {
				gf_evg_stencil_get_pixel_yuv_f(op->tx->stencil, right_val->x, right_val->y, &tmpr.x, &tmpr.y, &tmpr.z, &tmpr.q);
				if (op->left_value_type & COMP_X) left_val->x = tmpr.x;
				if (op->left_value_type & COMP_Y) left_val->y = tmpr.y;
				if (op->left_value_type & COMP_Z) left_val->z = tmpr.z;
				if (op->left_value_type & COMP_Q) left_val->q = tmpr.q;
			}
			break;
		case EVG_OP_DISCARD:
			frag->frag_valid = 0;
			return GF_TRUE;
		case EVG_OP_NORMALIZE:
			if (left_val_type_ptr) {
				*left_val_type_ptr = COMP_V4;
			}
			*left_val = *right_val;
			gf_vec_norm((GF_Vec *)left_val);
			break;
		case EVG_OP_LENGTH:
			if (left_val_type_ptr) {
				*left_val_type_ptr = COMP_FLOAT;
			}
			left_val->x = gf_vec_len_p( (GF_Vec *) right_val);
			break;
		case EVG_OP_DISTANCE:
			if (left_val_type_ptr) {
				*left_val_type_ptr = COMP_FLOAT;
			}
			right2_val = &shader->vars[op->right_value_second - EVG_FIRST_VAR_ID-1].vecval;
			//right2_val_type = shader->vars[op->right_value_second - EVG_FIRST_VAR_ID-1].value_type;
			gf_vec_diff(tmpr, *right_val, *right2_val);
			left_val->x = gf_vec_len_p( (GF_Vec *) &tmpr);
			break;

		case EVG_OP_DOT:
			if (left_val_type_ptr) {
				*left_val_type_ptr = COMP_FLOAT;
			}
			right2_val = &shader->vars[op->right_value_second - EVG_FIRST_VAR_ID-1].vecval;
			//right2_val_type = shader->vars[op->right_value_second - EVG_FIRST_VAR_ID-1].value_type;
			left_val->x = gf_vec_dot_p( (GF_Vec *) right_val, (GF_Vec *) right2_val);
			break;
		case EVG_OP_CROSS:
			if (left_val_type_ptr) {
				*left_val_type_ptr = COMP_V4;
			}
			right2_val = &shader->vars[op->right_value_second - EVG_FIRST_VAR_ID-1].vecval;
			//right2_val_type = shader->vars[op->right_value_second - EVG_FIRST_VAR_ID-1].value_type;
			* (GF_Vec *) left_val = gf_vec_cross_p( (GF_Vec *) right_val, (GF_Vec *) right2_val);
			break;

#define BASE_FUN2(__fun) \
	if (left_val_type_ptr) {\
		*left_val_type_ptr = right_val_type;\
	}\
	var_idx = op->right_value_second - EVG_FIRST_VAR_ID-1;\
	right2_val = &shader->vars[var_idx].vecval;\
	if (right_val_type==COMP_FLOAT) {\
		left_val->x = __fun(right_val->x, right2_val->x);\
	} else {\
		if (right_val_type&COMP_X) {\
			left_val->x = __fun(right_val->x, right2_val->x);\
		}\
		if (right_val_type&COMP_Y) {\
			left_val->y = __fun(right_val->y, right2_val->y);\
		}\
		if (right_val_type&COMP_Z) {\
			left_val->z = __fun(right_val->z, right2_val->z);\
		}\
		if (right_val_type&COMP_Q) {\
			left_val->q = __fun(right_val->z, right2_val->q);\
		}\
	}\


#define BASE_FUN(__fun) \
		if (left_val_type_ptr) {\
			*left_val_type_ptr = right_val_type;\
		}\
		if (right_val_type==COMP_FLOAT) {\
			left_val->x = (Float) __fun(right_val->x);\
		} else {\
			if (right_val_type&COMP_X) {\
				left_val->x = (Float) __fun(right_val->x);\
			}\
			if (right_val_type&COMP_Y) {\
				left_val->y = (Float) __fun(right_val->y);\
			}\
			if (right_val_type&COMP_Z) {\
				left_val->z = (Float) __fun(right_val->z);\
			}\
			if (right_val_type&COMP_Q) {\
				left_val->q = (Float) __fun(right_val->z);\
			}\
		}\

		case EVG_OP_POW:
			BASE_FUN2(powf)
			break;
		case EVG_OP_SIN:
			BASE_FUN(sinf)
			break;
		case EVG_OP_ASIN:
			BASE_FUN(asinf)
			break;
		case EVG_OP_COS:
			BASE_FUN(cosf)
			break;
		case EVG_OP_ACOS:
			BASE_FUN(acosf)
			break;
		case EVG_OP_TAN:
			BASE_FUN(tanf)
			break;
		case EVG_OP_ATAN:
			BASE_FUN2(atan2f)
			break;
		case EVG_OP_LOG:
			BASE_FUN(logf)
			break;
		case EVG_OP_EXP:
			BASE_FUN(expf)
			break;
		case EVG_OP_LOG2:
			BASE_FUN(log2f)
			break;
		case EVG_OP_EXP2:
			BASE_FUN(exp2f)
			break;
		case EVG_OP_SINH:
			BASE_FUN(sinhf)
			break;
		case EVG_OP_COSH:
			BASE_FUN(coshf)
			break;
		case EVG_OP_SQRT:
			BASE_FUN(sqrtf)
			break;
		case EVG_OP_INVERSE_SQRT:
			BASE_FUN(isqrtf)
			break;
		case EVG_OP_ABS:
			BASE_FUN(ABS)
			break;
		case EVG_OP_SIGN:
			BASE_FUN(signf)
			break;
		case EVG_OP_FLOOR:
			BASE_FUN(floorf)
			break;
		case EVG_OP_CEIL:
			BASE_FUN(ceilf)
			break;
		case EVG_OP_FRACT:
			BASE_FUN(fractf)
			break;
		case EVG_OP_MOD:
			BASE_FUN2(_modf)
			break;
		case EVG_OP_MIN:
			BASE_FUN2(MIN)
			break;
		case EVG_OP_MAX:
			BASE_FUN2(MAX)
			break;
		case EVG_OP_CLAMP:
			if (left_val_type_ptr) {
				*left_val_type_ptr = right_val_type;
			}
			var_idx = op->right_value_second - EVG_FIRST_VAR_ID-1;
			right2_val = &shader->vars[var_idx].vecval;
			//right2_val_type = shader->vars[var_idx].value_type;
			if (right_val_type==COMP_FLOAT) {
				left_val->x = evg_float_clamp(left_val->x, right_val->x, right2_val->x);
			} else {
				if (right_val_type&COMP_X) {
					left_val->x = evg_float_clamp(left_val->x, right_val->x, right2_val->x);
				}
				if (right_val_type&COMP_Y) {
					left_val->y = evg_float_clamp(left_val->y, right_val->y, right2_val->x);
				}
				if (right_val_type&COMP_Z) {
					left_val->z = evg_float_clamp(left_val->z, right_val->z, right2_val->x);
				}
				if (right_val_type&COMP_Q) {
					left_val->q = evg_float_clamp(left_val->q, right_val->z, right2_val->x);
				}
			}
			break;
		case EVG_OP_RGB2YUV:
			if (left_val_type_ptr) {
				*left_val_type_ptr = right_val_type;
			}
			left_val->q = right_val->q;
			gf_gf_evg_rgb_to_yuv_f(canvas->surface, right_val->x, right_val->y, right_val->z, &left_val->x, &left_val->y, &left_val->z);
			break;
		case EVG_OP_YUV2RGB:
			if (left_val_type_ptr) {
				*left_val_type_ptr = right_val_type;
			}
			left_val->q = right_val->q;
			gf_evg_yuv_to_rgb_f(canvas->surface, right_val->x, right_val->y, right_val->z, &left_val->x, &left_val->y, &left_val->z);
			break;

		case EVG_OP_PRINT:
			if (op->right_value>EVG_FIRST_VAR_ID) {
				fprintf(stderr, "%s: ", shader->vars[op->right_value - EVG_FIRST_VAR_ID - 1].name);
			}
			if (right_val_type==COMP_FLOAT) {
				fprintf(stderr, "%g\n", right_val->x);
			} else if (right_val_type==COMP_INT) {
				fprintf(stderr, "%d\n", * (s32 *) &right_val);
			} else {
				if (right_val_type&COMP_X) fprintf(stderr, "x=%g ", right_val->x);
				if (right_val_type&COMP_Y) fprintf(stderr, "y=%g ", right_val->y);
				if (right_val_type&COMP_Z) fprintf(stderr, "z=%g ", right_val->z);
				if (right_val_type&COMP_Q) fprintf(stderr, "q=%g ", right_val->q);
				fprintf(stderr, "\n");
			}

			break;
		}
	}
	return GF_TRUE;
}

static Bool evg_frag_shader_ops(void *udta, GF_EVGFragmentParam *frag)
{
	GF_JSCanvas *canvas = (GF_JSCanvas *)udta;
	if (!canvas->frag || canvas->frag->invalid) return GF_FALSE;
	return evg_shader_ops(canvas, canvas->frag, frag, NULL);
}
static Bool evg_vert_shader_ops(void *udta, GF_EVGVertexParam *vert)
{
	GF_JSCanvas *canvas = (GF_JSCanvas *)udta;
	if (!canvas->vert || canvas->vert->invalid) return GF_FALSE;
	return evg_shader_ops(canvas, canvas->vert, NULL, vert);
}

static void shader_reset(JSRuntime *rt, EVGShader *shader)
{
	u32 i;
	for (i=0; i<shader->nb_ops; i++) {
		if (shader->ops[i].right_value==VAR_VAI) {
			JS_FreeValueRT(rt, shader->ops[i].vai.ref);
		}
		else if (shader->ops[i].right_value==VAR_MATRIX) {
			JS_FreeValueRT(rt, shader->ops[i].mx.ref);
		}
		else if (shader->ops[i].right_value==VAR_VA) {
			JS_FreeValueRT(rt, shader->ops[i].va.ref);
		}
		else if (shader->ops[i].left_value==VAR_VAI) {
			JS_FreeValueRT(rt, shader->ops[i].vai.ref);
		}
		else if (shader->ops[i].left_value==VAR_MATRIX) {
			JS_FreeValueRT(rt, shader->ops[i].mx.ref);
		}

		if (shader->ops[i].uni_name) {
			gf_free(shader->ops[i].uni_name);
			shader->ops[i].uni_name = NULL;
		}
		if (shader->ops[i].op_type==EVG_OP_SAMPLER) {
			JS_FreeValueRT(rt, shader->ops[i].tx_ref);
			shader->ops[i].tx_ref = JS_UNDEFINED;
		}
		shader->ops[i].right_value = 0;
	}
	shader->nb_ops = 0;
	for (i=0; i<shader->nb_vars; i++) {
		if (shader->vars[i].name) gf_free(shader->vars[i].name);
		shader->vars[i].name = NULL;
	}
	shader->nb_vars = 0;
	shader->invalid = GF_FALSE;
	shader->disable_early_z = GF_FALSE;
}
static void shader_finalize(JSRuntime *rt, JSValue obj)
{
	EVGShader *shader = JS_GetOpaque(obj, shader_class_id);
	if (!shader) return;
	shader_reset(rt, shader);
	gf_free(shader->ops);
	gf_free(shader->vars);
	gf_free(shader);
}

static void shader_gc_mark(JSRuntime *rt, JSValueConst obj, JS_MarkFunc *mark_func)
{
	u32 i;
	EVGShader *shader = JS_GetOpaque(obj, shader_class_id);
	if (!shader) return;
	for (i=0; i<shader->nb_ops; i++) {
		if (shader->ops[i].tx)
			JS_MarkValue(rt, shader->ops[i].tx_ref, mark_func);

		if (shader->ops[i].right_value==VAR_VAI) {
			JS_MarkValue(rt, shader->ops[i].vai.ref, mark_func);
		}
		else if (shader->ops[i].right_value==VAR_MATRIX) {
			JS_MarkValue(rt, shader->ops[i].mx.ref, mark_func);
		}
		else if (shader->ops[i].right_value==VAR_VA) {
			JS_MarkValue(rt, shader->ops[i].va.ref, mark_func);
		}
		else if (shader->ops[i].left_value==VAR_VAI) {
			JS_MarkValue(rt, shader->ops[i].vai.ref, mark_func);
		}
		else if (shader->ops[i].left_value==VAR_MATRIX) {
			JS_MarkValue(rt, shader->ops[i].mx.ref, mark_func);
		}
	}
}

JSClassDef shader_class = {
	.class_name = "Shader",
	.finalizer = shader_finalize,
	.gc_mark = shader_gc_mark
};

static u32 get_builtin_var_name(EVGShader *shader, const char *val_name)
{
	u32 i;
	if (shader->mode==GF_EVG_SHADER_FRAGMENT) {
		if (!strcmp(val_name, "fragColor") || !strcmp(val_name, "fragRGBA")) return VAR_FRAG_ARGB;
		if (!strcmp(val_name, "fragYUVA")) return VAR_FRAG_YUV;
		if (!strcmp(val_name, "fragDepth") || !strcmp(val_name, "fragZ")) return VAR_FRAG_DEPTH;
		if (!strcmp(val_name, "fragX")) return VAR_FRAG_X;
		if (!strcmp(val_name, "fragY")) return VAR_FRAG_Y;
		if (!strcmp(val_name, "fragW")) return VAR_FRAG_W;
	}
	if (shader->mode==GF_EVG_SHADER_VERTEX) {
		if (!strcmp(val_name, "vertex")) return VAR_VERTEX_IN;
		if (!strcmp(val_name, "vertexOut")) return VAR_VERTEX_OUT;
	}

	if (val_name[0] == '.') return VAR_UNIFORM;

	for (i=0; i<shader->nb_vars; i++) {
		if (!strcmp(shader->vars[i].name, val_name)) {
			return EVG_FIRST_VAR_ID+i+1;
		}
	}
	return 0;
}
static u8 get_value_type(const char *comp)
{
	if (!strcmp(comp, "x") || !strcmp(comp, "r") || !strcmp(comp, "s")) return COMP_X;
	if (!strcmp(comp, "y") || !strcmp(comp, "g") || !strcmp(comp, "t")) return COMP_Y;
	if (!strcmp(comp, "z") || !strcmp(comp, "b")) return COMP_Z;
	if (!strcmp(comp, "q") || !strcmp(comp, "a")) return COMP_Q;
	if (!strcmp(comp, "xyz") || !strcmp(comp, "rgb")) return COMP_V3;
	if (!strcmp(comp, "xy") || !strcmp(comp, "rg") || !strcmp(comp, "st")) return COMP_V2_XY;
	if (!strcmp(comp, "xz") || !strcmp(comp, "rb")) return COMP_V2_XZ;
	if (!strcmp(comp, "yz") || !strcmp(comp, "gb")) return COMP_V2_YZ;
	return COMP_V4;
}


static JSValue shader_push(JSContext *ctx, JSValueConst obj, int argc, JSValueConst *argv)
{
	const char *val_name=NULL, *arg_str;
	const char *op_name;
	char *uni_name=NULL;
	Bool dual_right_val = GF_FALSE;
	u32 var_idx=0;
	u32 left_op_idx=0, right_op_idx=0, right_op2_idx=0;
	u8 left_value_type=COMP_V4, right_value_type=COMP_V4;
	u8 op_type=0;
	u8 cond_type=0;
	ShaderOp new_op;
	EVGShader *shader = JS_GetOpaque(obj, shader_class_id);
	if (!shader) return JS_EXCEPTION;
	shader->invalid = GF_FALSE;
	if (!argc) {
		shader_reset(JS_GetRuntime(ctx), shader);
		return JS_UNDEFINED;
	}

	memset(&new_op, 0, sizeof(ShaderOp));
	new_op.op_type = 0;
	new_op.tx_ref = JS_UNDEFINED;

	if (JS_IsObject(argv[0])) {
		new_op.vai.vai = JS_GetOpaque(argv[0], vai_class_id);
		if (!new_op.vai.vai) {
			shader->invalid = GF_TRUE;
			return js_throw_err_msg(ctx, GF_BAD_PARAM, "invalid left-operand value, must be a VAI");
		}
		new_op.vai.ref = JS_DupValue(ctx, argv[0]);
		left_op_idx = VAR_VAI;
	} else {

		arg_str = JS_ToCString(ctx, argv[0]);
		if (!arg_str) {
			shader->invalid = GF_TRUE;
			return js_throw_err_msg(ctx, GF_BAD_PARAM, "invalid left-operand value, must be a string");
		}
		val_name = arg_str;
		while ((val_name[0]==' ') || (val_name[0]=='\t'))
			val_name++;

		if (!strcmp(val_name, "if")) {
			op_type = EVG_OP_IF;
			JS_FreeCString(ctx, arg_str);
			arg_str = JS_ToCString(ctx, argv[1]);
			if (!arg_str) {
				shader->invalid = GF_TRUE;
				return js_throw_err_msg(ctx, GF_BAD_PARAM, "invalid if first var, must be a string");
			}
			var_idx=1;
			val_name = arg_str;
		}
		else if (!strcmp(val_name, "else")) {
			op_type = EVG_OP_ELSE;
			JS_FreeCString(ctx, arg_str);
			goto op_parsed;
		}
		else if (!strcmp(val_name, "discard")) {
			if (shader->mode==GF_EVG_SHADER_VERTEX) {
				shader->invalid = GF_TRUE;
				return js_throw_err_msg(ctx, GF_BAD_PARAM, "discard is invalid in vertex shader");
			}
			op_type = EVG_OP_DISCARD;
			JS_FreeCString(ctx, arg_str);
			goto op_parsed;
		}
		else if (!strcmp(val_name, "elseif")) {
			op_type = EVG_OP_ELSEIF;
			var_idx=1;
		}
		else if (!strcmp(val_name, "goto")) {
			op_type = EVG_OP_GOTO;
			JS_FreeCString(ctx, arg_str);

			if (JS_IsString(argv[1])) {
				arg_str = JS_ToCString(ctx, argv[1]);
				if (arg_str[0] != '.') {
					shader->invalid = GF_TRUE;
					return js_throw_err_msg(ctx, GF_BAD_PARAM, "invalid goto var, must be a uniform string");
				}
				right_op_idx = VAR_UNIFORM;
				uni_name = gf_strdup(arg_str+1);
				JS_FreeCString(ctx, arg_str);
			} else if (JS_ToInt32(ctx, &left_op_idx, argv[1]) ) {
				shader->invalid = GF_TRUE;
				return js_throw_err_msg(ctx, GF_BAD_PARAM, "invalid goto var, must be a number greater than 1, 1 being the first instruction in the stack");
			}
			goto op_parsed;
		}
		else if (!strcmp(val_name, "end")) {
			op_type = EVG_OP_END;
			JS_FreeCString(ctx, arg_str);
			goto op_parsed;
		}
		else if (!strcmp(val_name, "print")) {
			op_type = EVG_OP_PRINT;
			JS_FreeCString(ctx, arg_str);
			var_idx=-1;
			goto parse_right_val;
		}
		else if (!strcmp(val_name, "toRGB")) {
			op_type = EVG_OP_YUV2RGB;
			JS_FreeCString(ctx, arg_str);
			var_idx=-1;
			goto parse_right_val;
		}
		else if (!strcmp(val_name, "toYUV")) {
			op_type = EVG_OP_RGB2YUV;
			JS_FreeCString(ctx, arg_str);
			var_idx=-1;
			goto parse_right_val;
		}

		char *sep = strchr(val_name, '.');
		if (sep) sep[0] = 0;
		left_op_idx = get_builtin_var_name(shader, val_name);
		if (!left_op_idx) {
			if (shader->alloc_vars <= shader->nb_vars) {
				shader->alloc_vars = shader->nb_vars+1;
				shader->vars = gf_realloc(shader->vars, sizeof(ShaderVar)*shader->alloc_vars);
			}
			shader->vars[shader->nb_vars].name = gf_strdup(val_name);
			shader->nb_vars++;
			left_op_idx = EVG_FIRST_VAR_ID + shader->nb_vars;
		}
		if (sep) {
			left_value_type = get_value_type(sep+1);
			sep[0] = '.';
		}
		JS_FreeCString(ctx, arg_str);
	}

	op_name = JS_ToCString(ctx, argv[var_idx+1]);
	if (!op_name) {
		shader->invalid = GF_TRUE;
		return js_throw_err_msg(ctx, GF_BAD_PARAM, "invalid operand type, must be a string");
	}
	if (!strcmp(op_name, "=")) {
		op_type = EVG_OP_ASSIGN;
		if (left_op_idx==VAR_FRAG_DEPTH)
			shader->disable_early_z = GF_TRUE;
	} else if (!strcmp(op_name, "+=")) {
		op_type = EVG_OP_ADD;
		if (left_op_idx==VAR_FRAG_DEPTH)
			shader->disable_early_z = GF_TRUE;
	} else if (!strcmp(op_name, "-=")) {
		op_type = EVG_OP_SUB;
		if (left_op_idx==VAR_FRAG_DEPTH)
			shader->disable_early_z = GF_TRUE;
	} else if (!strcmp(op_name, "*=")) {
		op_type = EVG_OP_MUL;
		if (left_op_idx==VAR_FRAG_DEPTH)
			shader->disable_early_z = GF_TRUE;
	} else if (!strcmp(op_name, "/=")) {
		op_type = EVG_OP_DIV;
		if (left_op_idx==VAR_FRAG_DEPTH)
			shader->disable_early_z = GF_TRUE;
	} else if (!strcmp(op_name, "=!")) {
		op_type = EVG_OP_NEG;
		if (left_op_idx==VAR_FRAG_DEPTH)
			shader->disable_early_z = GF_TRUE;
	} else if (!strcmp(op_name, "<")) {
		cond_type = EVG_OP_LESS;
	} else if (!strcmp(op_name, "<=")) {
		cond_type = EVG_OP_LESS_EQUAL;
	} else if (!strcmp(op_name, ">")) {
		cond_type = EVG_OP_GREATER;
	} else if (!strcmp(op_name, ">=")) {
		cond_type = EVG_OP_GREATER_EQUAL;
	} else if (!strcmp(op_name, "==")) {
		cond_type = EVG_OP_EQUAL;
	} else if (!strcmp(op_name, "!=")) {
		cond_type = EVG_OP_NOT_EQUAL;
	} else if (!strcmp(op_name, "sampler") || !strcmp(op_name, "samplerYUV") ) {
		new_op.tx = JS_GetOpaque(argv[var_idx+2], texture_class_id);
		if (!strcmp(op_name, "samplerYUV")) op_type = EVG_OP_SAMPLER_YUV;
		else op_type = EVG_OP_SAMPLER;

		if (!new_op.tx) {
			JS_FreeCString(ctx, val_name);
			shader->invalid = GF_TRUE;
			return js_throw_err_msg(ctx, GF_BAD_PARAM, "invalid texture object for 2D sampler");
		}
		new_op.tx_ref = argv[var_idx+2];
		var_idx++;
	} else if (!strcmp(op_name, "normalize")) {
		op_type = EVG_OP_NORMALIZE;
	} else if (!strcmp(op_name, "length")) {
		op_type = EVG_OP_LENGTH;
	} else if (!strcmp(op_name, "distance")) {
		op_type = EVG_OP_DISTANCE;
		dual_right_val = GF_TRUE;
	} else if (!strcmp(op_name, "dot")) {
		op_type = EVG_OP_DOT;
		dual_right_val = GF_TRUE;
	} else if (!strcmp(op_name, "cross")) {
		op_type = EVG_OP_CROSS;
		dual_right_val = GF_TRUE;
	} else if (!strcmp(op_name, "pow")) {
		op_type = EVG_OP_POW;
		dual_right_val = GF_TRUE;
	} else if (!strcmp(op_name, "sin")) {
		op_type = EVG_OP_SIN;
	} else if (!strcmp(op_name, "asin")) {
		op_type = EVG_OP_ASIN;
	} else if (!strcmp(op_name, "cos")) {
		op_type = EVG_OP_COS;
	} else if (!strcmp(op_name, "acos")) {
		op_type = EVG_OP_ACOS;
	} else if (!strcmp(op_name, "tan")) {
		op_type = EVG_OP_TAN;
	} else if (!strcmp(op_name, "atan")) {
		op_type = EVG_OP_ATAN;
		dual_right_val = GF_TRUE;
	} else if (!strcmp(op_name, "log")) {
		op_type = EVG_OP_LOG;
	} else if (!strcmp(op_name, "exp")) {
		op_type = EVG_OP_EXP;
	} else if (!strcmp(op_name, "log2")) {
		op_type = EVG_OP_LOG2;
	} else if (!strcmp(op_name, "exp2")) {
		op_type = EVG_OP_EXP2;
	} else if (!strcmp(op_name, "sinh")) {
		op_type = EVG_OP_SINH;
	} else if (!strcmp(op_name, "cosh")) {
		op_type = EVG_OP_COSH;
	} else if (!strcmp(op_name, "sqrt")) {
		op_type = EVG_OP_SQRT;
	} else if (!strcmp(op_name, "inversesqrt")) {
		op_type = EVG_OP_INVERSE_SQRT;
	} else if (!strcmp(op_name, "abs")) {
		op_type = EVG_OP_ABS;
	} else if (!strcmp(op_name, "sign")) {
		op_type = EVG_OP_SIGN;
	} else if (!strcmp(op_name, "floor")) {
		op_type = EVG_OP_FLOOR;
	} else if (!strcmp(op_name, "ceil")) {
		op_type = EVG_OP_CEIL;
	} else if (!strcmp(op_name, "fract")) {
		op_type = EVG_OP_FRACT;
	} else if (!strcmp(op_name, "mod")) {
		op_type = EVG_OP_MOD;
		dual_right_val = GF_TRUE;
	} else if (!strcmp(op_name, "min")) {
		op_type = EVG_OP_MIN;
		dual_right_val = GF_TRUE;
	} else if (!strcmp(op_name, "max")) {
		op_type = EVG_OP_MAX;
		dual_right_val = GF_TRUE;
	} else if (!strcmp(op_name, "clamp")) {
		op_type = EVG_OP_CLAMP;
		dual_right_val = GF_TRUE;
	} else {
		JS_FreeCString(ctx, val_name);
		shader->invalid = GF_TRUE;
		return js_throw_err_msg(ctx, GF_BAD_PARAM, "invalid operand type, must be a string");
	}
	JS_FreeCString(ctx, op_name);

parse_right_val:
	if (JS_IsString(argv[var_idx+2])) {
		val_name = JS_ToCString(ctx, argv[var_idx+2]);
		if (!val_name) {
			shader->invalid = GF_TRUE;
			return js_throw_err_msg(ctx, GF_BAD_PARAM, "invalid right-operand value, must be a string");
		}

		char *sep = strchr(val_name+1, '.');
		if (sep) sep[0] = 0;
		right_op_idx = get_builtin_var_name(shader, val_name);
		if (right_op_idx==VAR_UNIFORM) {
			uni_name = gf_strdup(val_name+1);
		}
		if (sep) sep[0] = '.';

		if (!right_op_idx) {
			JSValue ret = js_throw_err_msg(ctx, GF_BAD_PARAM, "invalid right-operand value, undefined variable %s", val_name);
			JS_FreeCString(ctx, val_name);
			shader->invalid = GF_TRUE;
			if (uni_name) gf_free(uni_name);
			return ret;
		}
		if (sep) {
			right_value_type = get_value_type(sep+1);
		}
		JS_FreeCString(ctx, val_name);
	}
	else if (JS_IsArray(ctx, argv[var_idx+2])) {
		u32 idx, length;
		JSValue comp = JS_GetPropertyStr(ctx, argv[var_idx+2], "length");
		if (JS_ToInt32(ctx, &length, comp)) return JS_EXCEPTION;
		JS_FreeValue(ctx, comp);
		if (length>4) length=4;
		right_value_type = 0;
		for (idx=0;idx<length; idx++) {
			comp = JS_GetPropertyUint32(ctx, argv[var_idx+2], idx);
			EVG_GET_FLOAT(new_op.vec[idx], comp);
			right_value_type |= 1<<idx;
			JS_FreeValue(ctx, comp);
		}
	}
	else if (JS_IsObject(argv[var_idx+2])) {
		EVG_VAI *vai = JS_GetOpaque(argv[var_idx+2], vai_class_id);
		GF_Matrix *mx = JS_GetOpaque(argv[var_idx+2], matrix_class_id);
		EVG_VA *va = (shader->mode==GF_EVG_SHADER_VERTEX) ? JS_GetOpaque(argv[var_idx+2], va_class_id) : NULL;

		if (vai) {
			new_op.vai.vai = vai;
			new_op.vai.ref = JS_DupValue(ctx, argv[var_idx+2]);
			right_op_idx = VAR_VAI;
		} else if (mx) {
			new_op.mx.mx = mx;
			new_op.mx.ref = JS_DupValue(ctx, argv[var_idx+2]);
			right_op_idx = VAR_MATRIX;
		} else if (va) {
			new_op.va.va = va;
			new_op.va.ref = JS_DupValue(ctx, argv[var_idx+2]);
			right_op_idx = VAR_VA;
		} else {
			shader->invalid = GF_TRUE;
			if (uni_name) gf_free(uni_name);
			return js_throw_err_msg(ctx, GF_BAD_PARAM, "unknown object type for right operand");
		}
	}
	else if (JS_IsBool(argv[var_idx+2])) {
		new_op.bval = JS_ToBool(ctx, argv[var_idx+2]) ? GF_TRUE : GF_FALSE;
		right_value_type = COMP_BOOL;
	}
	else if (JS_IsNumber(argv[var_idx+2])) {
		if (JS_IsInteger(argv[var_idx+2])) {
			JS_ToInt32(ctx, &new_op.ival, argv[var_idx+2]);
			right_value_type = COMP_INT;
		} else {
			Double v;
			JS_ToFloat64(ctx, &v, argv[var_idx+2]);
			new_op.vec[0] = (Float) v;
			right_value_type = COMP_FLOAT;
		}
	}

	if (dual_right_val) {
		Bool is_ok=GF_FALSE;
		if ((u32) argc<=var_idx+3) {}
		else {
			val_name = JS_ToCString(ctx, argv[var_idx+3]);
			if (val_name) {
				right_op2_idx = get_builtin_var_name(shader, val_name);
				if (right_op2_idx >= EVG_FIRST_VAR_ID) {
					is_ok=GF_TRUE;
				}
			}
			JS_FreeCString(ctx, val_name);
		}
		if (!is_ok) {
			JSValue ret = js_throw_err_msg(ctx, GF_BAD_PARAM, "invalid second right-operand value, only local variable allowed");
			JS_FreeCString(ctx, val_name);
			shader->invalid = GF_TRUE;
			if (uni_name) gf_free(uni_name);
			return ret;
		}
	}

op_parsed:

	if (dual_right_val && !right_op2_idx) {
		shader->invalid = GF_TRUE;
		if (uni_name) gf_free(uni_name);
		return js_throw_err_msg(ctx, GF_BAD_PARAM, "invalid second right-operand value, only local variable allowed");
	}
	new_op.op_type = op_type;
	if (!new_op.op_type) {
		shader->invalid = GF_TRUE;
		if (uni_name) gf_free(uni_name);
		return js_throw_err_msg(ctx, GF_BAD_PARAM, "unknown operation type");
	}
	new_op.cond_type = cond_type;
	new_op.left_value = left_op_idx;
	new_op.right_value = right_op_idx;
	new_op.left_value_type = left_value_type;
	new_op.right_value_type = right_value_type;
	new_op.right_value_second = right_op2_idx;
	if (new_op.tx) {
		new_op.tx_ref = JS_DupValue(ctx, new_op.tx_ref);
	}

	if (shader->alloc_ops <= shader->nb_ops) {
		shader->alloc_ops = shader->nb_ops+1;
		shader->ops = gf_realloc(shader->ops, sizeof(ShaderOp)*shader->alloc_ops);
		shader->ops[shader->nb_ops].uni_name = NULL;
	}
	if (shader->ops[shader->nb_ops].uni_name) {
		gf_free(shader->ops[shader->nb_ops].uni_name);
		shader->ops[shader->nb_ops].uni_name = NULL;
	}

	shader->ops[shader->nb_ops] = new_op;
	shader->ops[shader->nb_ops].uni_name = uni_name;
	shader->nb_ops++;
	return JS_NewInt32(ctx, shader->nb_ops);
}

static JSValue shader_update(JSContext *ctx, JSValueConst obj, int argc, JSValueConst *argv)
{
	u32 i;
	EVGShader *shader = JS_GetOpaque(obj, shader_class_id);
	if (!shader) return JS_EXCEPTION;
	if (shader->invalid) return JS_EXCEPTION;

	for (i=0; i<shader->nb_ops; i++) {
		JSValue v;
		ShaderOp *op = &shader->ops[i];
		if (!op->uni_name) continue;
		v = JS_GetPropertyStr(ctx, obj, op->uni_name);
		if (JS_IsUndefined(v)) return js_throw_err_msg(ctx, GF_BAD_PARAM, "uniform %s cannot be found in shader", op->uni_name);
		if (JS_IsBool(v)) {
			op->right_value_type = COMP_BOOL;
			op->bval = JS_ToBool(ctx, v) ? GF_TRUE : GF_FALSE;
		}
		else if (JS_IsNumber(v)) {
			if (JS_IsInteger(v)) {
				op->right_value_type = COMP_INT;
				if (JS_ToInt32(ctx, &op->ival, v)) return JS_EXCEPTION;
			} else {
				op->right_value_type = COMP_FLOAT;
				EVG_GET_FLOAT(op->vec[0], v)
			}
		}
		else if (JS_IsArray(ctx, v)) {
			u32 idx, length;
			JSValue comp = JS_GetPropertyStr(ctx, v, "length");
			if (JS_ToInt32(ctx, &length, comp)) return JS_EXCEPTION;
			JS_FreeValue(ctx, comp);
			if (length>4) length=4;
			op->right_value_type = 0;
			for (idx=0;idx<length; idx++) {
				comp = JS_GetPropertyUint32(ctx, v, idx);
				EVG_GET_FLOAT(op->vec[idx], comp);
				op->right_value_type |= 1<<idx;
				JS_FreeValue(ctx, comp);
			}
		}
		else if (JS_IsObject(v)) {
			u32 data_size;
			Float *vals;
			u32 idx;
			u8 *data = evg_get_array(ctx, v, &data_size);
			if (data) {
				if (data_size%4) return JS_EXCEPTION;
				vals = (Float*)data;
				data_size/= sizeof(Float);
				if (data_size>4) data_size=4;
				op->right_value_type = 0;
				for (idx=0;idx<data_size; idx++) {
					op->vec[idx] = vals[idx];
					op->right_value_type |= 1<<idx;
				}
			} else {
				JSValue comp;
				op->right_value_type = 0;
#define GET_COMP(_name, _idx, _mask)\
				comp = JS_GetPropertyStr(ctx, v, _name);\
				if (!JS_IsUndefined(comp)) { EVG_GET_FLOAT(op->vec[_idx], comp); op->right_value_type |= _mask; }\
				JS_FreeValue(ctx, comp);\

				GET_COMP("x", 0, COMP_X);
				GET_COMP("r", 0, COMP_X);
				GET_COMP("s", 0, COMP_X);

				GET_COMP("y", 1, COMP_Y);
				GET_COMP("g", 1, COMP_Y);
				GET_COMP("t", 1, COMP_Y);

				GET_COMP("z", 2, COMP_Z);
				GET_COMP("b", 2, COMP_Z);

				GET_COMP("q", 3, COMP_Q);
				GET_COMP("w", 3, COMP_Q);
				GET_COMP("a", 3, COMP_Q);

			}
		}
		JS_FreeValue(ctx, v);

	}
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry shader_funcs[] =
{
	JS_CFUNC_DEF("push", 0, shader_push),
	JS_CFUNC_DEF("update", 0, shader_update),
};

static JSValue canvas3d_new_shader(JSContext *ctx, JSValueConst obj, int argc, JSValueConst *argv)
{
	EVGShader *shader;
	u32 mode;
	JSValue res;
	GF_JSCanvas *canvas = JS_GetOpaque(obj, canvas3d_class_id);
	if (!canvas) return JS_EXCEPTION;
	if (!argc) return JS_EXCEPTION;
	JS_ToInt32(ctx, &mode, argv[0]);
	GF_SAFEALLOC(shader, EVGShader);
	if (!shader)
		return js_throw_err(ctx, GF_OUT_OF_MEM);
	shader->mode = mode;
	res = JS_NewObjectClass(ctx, shader_class_id);
	JS_SetOpaque(res, shader);
	return res;
}

static const JSCFunctionListEntry canvas3d_funcs[] =
{
	JS_CGETSET_MAGIC_DEF("clipper", NULL, canvas3d_setProperty, GF_EVG_CLIPPER),
	JS_CGETSET_MAGIC_DEF("fragment", canvas3d_getProperty, canvas3d_setProperty, GF_EVG_FRAG_SHADER),
	JS_CGETSET_MAGIC_DEF("vertex", canvas3d_getProperty, canvas3d_setProperty, GF_EVG_VERT_SHADER),
	JS_CGETSET_MAGIC_DEF("ccw", NULL, canvas3d_setProperty, GF_EVG_CCW),
	JS_CGETSET_MAGIC_DEF("backcull", NULL, canvas3d_setProperty, GF_EVG_BACKCULL),
	JS_CGETSET_MAGIC_DEF("antialias", NULL, canvas3d_setProperty, GF_EVG_ANTIALIAS),
	JS_CGETSET_MAGIC_DEF("min_depth", NULL, canvas3d_setProperty, GF_EVG_MINDEPTH),
	JS_CGETSET_MAGIC_DEF("max_depth", NULL, canvas3d_setProperty, GF_EVG_MAXDEPTH),
	JS_CGETSET_MAGIC_DEF("point_size", NULL, canvas3d_setProperty, GF_EVG_POINTSIZE),
	JS_CGETSET_MAGIC_DEF("point_smooth", NULL, canvas3d_setProperty, GF_EVG_POINTSMOOTH),
	JS_CGETSET_MAGIC_DEF("line_size", NULL, canvas3d_setProperty, GF_EVG_LINESIZE),
	JS_CGETSET_MAGIC_DEF("clip_zero", NULL, canvas3d_setProperty, GF_EVG_CLIP_ZERO),
	JS_CGETSET_MAGIC_DEF("depth_test", NULL, canvas3d_setProperty, GF_EVG_DEPTH_TEST),
	JS_CGETSET_MAGIC_DEF("write_depth", NULL, canvas3d_setProperty, GF_EVG_WRITE_DEPTH),
	JS_CGETSET_MAGIC_DEF("depth_buffer", canvas3d_getProperty, canvas3d_setProperty, GF_EVG_DEPTH_BUFFER),

	JS_CFUNC_DEF("clear", 0, canvas3d_clear),
	JS_CFUNC_DEF("clearf", 0, canvas3d_clearf),
	JS_CFUNC_DEF("reassign", 0, canvas3d_reassign),
	JS_CFUNC_DEF("projection", 0, canvas3d_projection),
	JS_CFUNC_DEF("modelview", 0, canvas3d_modelview),
	JS_CFUNC_DEF("draw_array", 0, canvas3d_draw_array),
	JS_CFUNC_DEF("draw_path", 0, canvas3d_draw_path),
	JS_CFUNC_DEF("clear_depth", 0, canvas3d_clear_depth),
	JS_CFUNC_DEF("viewport", 0, canvas3d_viewport),
	JS_CFUNC_DEF("new_shader", 0, canvas3d_new_shader),
	JS_CFUNC_DEF("toYUV", 0, canvas3d_toYUV),
	JS_CFUNC_DEF("toRGB", 0, canvas3d_toRGB),
};

static JSValue canvas3d_constructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	return canvas_constructor_internal(c, new_target, argc, argv, GF_TRUE);
}

#ifdef EVG_USE_JS_SHADER

JSClassDef fragment_class = {
	.class_name = "Fragment",
};

enum {
	EVG_FRAG_SCREENX,
	EVG_FRAG_SCREENY,
	EVG_FRAG_DEPTH,
	EVG_FRAG_R,
	EVG_FRAG_G,
	EVG_FRAG_B,
	EVG_FRAG_A,
	EVG_FRAG_Y,
	EVG_FRAG_U,
	EVG_FRAG_V,
	EVG_FRAG_RGBA,
	EVG_FRAG_RGB,
	EVG_FRAG_YUV,
	EVG_FRAG_YUVA,
};

static JSValue fragment_getProperty(JSContext *c, JSValueConst obj, int magic)
{
	GF_EVGFragmentParam *frag = JS_GetOpaque(obj, fragment_class_id);
	if (!frag) return JS_EXCEPTION;
	switch (magic) {
	case EVG_FRAG_SCREENX: return JS_NewFloat64(c, frag->screen_x);
	case EVG_FRAG_SCREENY: return JS_NewFloat64(c, frag->screen_y);
	case EVG_FRAG_DEPTH: return JS_NewFloat64(c, frag->depth);
	}
	return JS_UNDEFINED;
}
static JSValue fragment_setProperty(JSContext *ctx, JSValueConst obj, JSValueConst value, int magic)
{
	EVG_VAIRes *vr;
	GF_EVGFragmentType frag_type = GF_EVG_FRAG_RGB;

	GF_EVGFragmentParam *frag = JS_GetOpaque(obj, fragment_class_id);
	if (!frag) return JS_EXCEPTION;
	switch (magic) {
	case EVG_FRAG_DEPTH:
		EVG_GET_FLOAT(frag->depth, value)
		break;
	case EVG_FRAG_Y:
		frag_type = GF_EVG_FRAG_YUV;
	case EVG_FRAG_R:
		EVG_GET_FLOAT(frag->color.x, value)
		CLAMPCOLF(frag->color.x)
		frag->frag_valid = frag_type;
		break;
	case EVG_FRAG_U:
		frag_type = GF_EVG_FRAG_YUV;
	case EVG_FRAG_G:
		EVG_GET_FLOAT(frag->color.y, value)
		CLAMPCOLF(frag->color.y)
		frag->frag_valid = frag_type;
		break;
	case EVG_FRAG_V:
		frag_type = GF_EVG_FRAG_YUV;
	case EVG_FRAG_B:
		EVG_GET_FLOAT(frag->color.z, value)
		CLAMPCOLF(frag->color.z)
		frag->frag_valid = frag_type;
		break;
	case EVG_FRAG_A:
		EVG_GET_FLOAT(frag->color.q, value)
		CLAMPCOLF(frag->color.q)
		if (!frag->frag_valid)
			frag->frag_valid = GF_EVG_FRAG_RGB;
		break;
	case EVG_FRAG_YUVA:
		frag_type = GF_EVG_FRAG_YUV;
	case EVG_FRAG_RGBA:
		vr = JS_GetOpaque(value, vaires_class_id);
		if (vr) {
			frag->color.x = vr->values[0];
			frag->color.y = vr->values[1];
			frag->color.z = vr->values[2];
			frag->color.q = vr->values[3];
			frag->frag_valid = frag_type;
		} else {
			Double a, r, g, b;
			a=1.0;
			if (!get_color_from_args(ctx, 1, &value, 0, &a, &r, &g, &b))
				return JS_EXCEPTION;
			frag->color.q = (Float) a;
			frag->color.x = (Float) r;
			frag->color.y = (Float) g;
			frag->color.z = (Float) b;
			frag->frag_valid = GF_EVG_FRAG_RGB;
		}
		break;
	case EVG_FRAG_YUV:
		frag_type = GF_EVG_FRAG_YUV;
	case EVG_FRAG_RGB:
		vr = JS_GetOpaque(value, vaires_class_id);
		if (vr) {
			frag->color.x = vr->values[0];
			frag->color.y = vr->values[1];
			frag->color.z = vr->values[2];
			frag->frag_valid = frag_type;
		} else
			return JS_EXCEPTION;
	default:
		return JS_UNDEFINED;
	}
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry fragment_funcs[] =
{
	JS_CGETSET_MAGIC_DEF("x", fragment_getProperty, NULL, EVG_FRAG_SCREENX),
	JS_CGETSET_MAGIC_DEF("y", fragment_getProperty, NULL, EVG_FRAG_SCREENY),
	JS_CGETSET_MAGIC_DEF("z", fragment_getProperty, fragment_setProperty, EVG_FRAG_DEPTH),
	JS_ALIAS_DEF("depth", "z"),
	JS_CGETSET_MAGIC_DEF("r", NULL, fragment_setProperty, EVG_FRAG_R),
	JS_CGETSET_MAGIC_DEF("g", NULL, fragment_setProperty, EVG_FRAG_G),
	JS_CGETSET_MAGIC_DEF("b", NULL, fragment_setProperty, EVG_FRAG_B),
	JS_CGETSET_MAGIC_DEF("a", NULL, fragment_setProperty, EVG_FRAG_A),
	JS_ALIAS_DEF("Y", "r"),
	JS_ALIAS_DEF("U", "g"),
	JS_ALIAS_DEF("V", "b"),
	JS_CGETSET_MAGIC_DEF("rgba", NULL, fragment_setProperty, EVG_FRAG_RGBA),
	JS_CGETSET_MAGIC_DEF("rgb", NULL, fragment_setProperty, EVG_FRAG_RGB),
	JS_CGETSET_MAGIC_DEF("yuv", NULL, fragment_setProperty, EVG_FRAG_YUV),
	JS_CGETSET_MAGIC_DEF("yuva", NULL, fragment_setProperty, EVG_FRAG_YUVA),
};

JSClassDef vertex_class = {
	.class_name = "Vertex",
};

enum {
	EVG_VERTEX_IN,
	EVG_VERTEX_OUT,
};

static JSValue vertex_getProperty(JSContext *c, JSValueConst obj, int magic)
{
	JSValue res;
	GF_EVGVertexParam *vert= JS_GetOpaque(obj, vertex_class_id);
	if (!vert) return JS_EXCEPTION;
	switch (magic) {
	case EVG_VERTEX_IN: 
		res = JS_NewObject(c);
		JS_SetPropertyStr(c, res, "x", JS_NewFloat64(c, vert->in_vertex.x));
		JS_SetPropertyStr(c, res, "y", JS_NewFloat64(c, vert->in_vertex.y));
		JS_SetPropertyStr(c, res, "z", JS_NewFloat64(c, vert->in_vertex.z));
		JS_SetPropertyStr(c, res, "q", JS_NewFloat64(c, vert->in_vertex.q));
		return res;
	}
	return JS_UNDEFINED;
}
static JSValue vertex_setProperty(JSContext *ctx, JSValueConst obj, JSValueConst value, int magic)
{
	Double _f;
	JSValue v;
	GF_EVGVertexParam *vert= JS_GetOpaque(obj, vertex_class_id);
	if (!vert) return JS_EXCEPTION;
	switch (magic) {
	case EVG_VERTEX_OUT:
		v = JS_GetPropertyStr(ctx, value, "x");
		EVG_GET_FLOAT(_f, v);
		vert->out_vertex.x = FLT2FIX(_f);
		v = JS_GetPropertyStr(ctx, value, "y");
		EVG_GET_FLOAT(_f, v);
		vert->out_vertex.y = FLT2FIX(_f);
		v = JS_GetPropertyStr(ctx, value, "z");
		EVG_GET_FLOAT(_f, v);
		vert->out_vertex.z = FLT2FIX(_f);
		v = JS_GetPropertyStr(ctx, value, "q");
		EVG_GET_FLOAT(_f, v);
		vert->out_vertex.q = FLT2FIX(_f);

		break;
	default:
		return JS_UNDEFINED;
	}
	return JS_UNDEFINED;
}
static const JSCFunctionListEntry vertex_funcs[] =
{
	JS_CGETSET_MAGIC_DEF("vertex", vertex_getProperty, NULL, EVG_VERTEX_IN),
	JS_CGETSET_MAGIC_DEF("vertexOut", NULL, vertex_setProperty, EVG_VERTEX_OUT),
};
#endif // EVG_USE_JS_SHADER

Bool vai_call_lerp(EVG_VAI *vai, GF_EVGFragmentParam *frag)
{
	u32 i;
	
	//different primitive, setup inperpolation points
	if (frag->prim_index != vai->prim_idx) {
		u32 idx;
		vai->prim_idx = frag->prim_index;
		//no values, this is a VAI filled by the vertex shader
		if (!vai->values) {
		} else if (vai->interp_type==GF_EVG_VAI_PRIMITIVE) {
			idx = frag->prim_index * vai->nb_comp;
			if (idx+vai->nb_comp > vai->nb_values)
				return GF_FALSE;

			for (i=0; i<vai->nb_comp; i++) {
				vai->result.values[i] = vai->values[idx+i];
			}
		} else if (frag->ptype!=GF_EVG_POINTS) {
			u32 nb_v_per_prim = 3;
			if (frag->ptype==GF_EVG_LINES)
				nb_v_per_prim=2;

			if (vai->interp_type==GF_EVG_VAI_VERTEX_INDEX) {
				idx = frag->idx1 * vai->nb_comp;
			} else {
				idx = frag->prim_index * nb_v_per_prim * vai->nb_comp;
			}
			if (idx+vai->nb_comp > vai->nb_values)
				return GF_FALSE;
			for (i=0; i<vai->nb_comp; i++) {
				vai->anchors[0][i] = vai->values[idx+i];
			}

			if (vai->interp_type==GF_EVG_VAI_VERTEX_INDEX) {
				idx = frag->idx2 * vai->nb_comp;
			} else {
				idx = (frag->prim_index * nb_v_per_prim + 1) * vai->nb_comp;
			}
			if (idx+vai->nb_comp > vai->nb_values)
				return GF_FALSE;
			for (i=0; i<vai->nb_comp; i++) {
				vai->anchors[1][i] = vai->values[idx+i];
			}
			if (frag->ptype!=GF_EVG_LINES) {
				if (vai->interp_type==GF_EVG_VAI_VERTEX_INDEX) {
					idx = frag->idx3 * vai->nb_comp;
				} else {
					idx = (frag->prim_index * nb_v_per_prim + 2) * vai->nb_comp;
				}
				if (idx+vai->nb_comp > vai->nb_values)
					return GF_FALSE;
				for (i=0; i<vai->nb_comp; i++) {
					vai->anchors[2][i] = vai->values[idx+i];
				}
			}
		}
	}
	if (vai->interp_type==GF_EVG_VAI_PRIMITIVE) {
		return GF_TRUE;
	}

	if (frag->ptype==GF_EVG_LINES) {
		for (i=0; i<vai->nb_comp; i++) {
			Float v = frag->pbc1 * vai->anchors[0][i] + frag->pbc2 * vai->anchors[1][i];
			vai->result.values[i] = v / frag->persp_denum;
		}
	} else {
		for (i=0; i<vai->nb_comp; i++) {
			Float v = (Float) ( frag->pbc1 * vai->anchors[0][i] + frag->pbc2 * vai->anchors[1][i] + frag->pbc3 * vai->anchors[2][i] );
			vai->result.values[i] = v / frag->persp_denum;
		}
	}
	if (vai->normalize) {
		if (vai->nb_comp==2) {
			Float len;
			if (!vai->result.values[0]) len = ABS(vai->result.values[1]);
			else if (!vai->result.values[1]) len = ABS(vai->result.values[0]);
			else len = sqrtf(vai->result.values[0]*vai->result.values[0] + vai->result.values[1]*vai->result.values[1]);
			if (len) {
				vai->result.values[0]/=len;
				vai->result.values[1]/=len;
			}
		} else {
			gf_vec_norm((GF_Vec *) &vai->result.values[0]);
		}
	}
	return GF_TRUE;
}

#ifdef EVG_USE_JS_SHADER
static JSValue vai_lerp(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	EVG_VAI *vai = JS_GetOpaque(obj, vai_class_id);
	if (!vai || !argc) return JS_EXCEPTION;
	GF_EVGFragmentParam *frag = JS_GetOpaque(argv[0], fragment_class_id);
	if (!frag) return JS_EXCEPTION;

	if (!vai_call_lerp(vai, frag))
		return JS_EXCEPTION;
	return JS_DupValue(c, vai->res);
}
#endif

static JSValue vai_setProperty(JSContext *c, JSValueConst obj, JSValueConst value, int magic)
{
	EVG_VAI *vai = JS_GetOpaque(obj, vai_class_id);
	if (!vai) return JS_EXCEPTION;
	switch (magic) {
	case 0:
		vai->normalize = JS_ToBool(c, value) ? GF_TRUE : GF_FALSE;
		break;
	}
	return JS_UNDEFINED;
}

static JSValue vai_constructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	EVG_VAI *vai;
	JSValue obj;
	u8 *data=NULL;
	u32 data_size=0;
	u32 nb_comp;
	s32 interp_type = GF_EVG_VAI_VERTEX_INDEX;
	if (argc<1)
		return js_throw_err_msg(c, GF_BAD_PARAM, "Missing parameter for VertexAttribInterpolator");

	if (argc>=2) {
		data = evg_get_array(c, argv[0], &data_size);
		if (!data) return JS_EXCEPTION;
		if (JS_ToInt32(c, &nb_comp, argv[1])) return JS_EXCEPTION;
		if (nb_comp>MAX_ATTR_DIM)
			return js_throw_err_msg(c, GF_BAD_PARAM, "Dimension too big, max is %d", MAX_ATTR_DIM);

		if (data_size % sizeof(Float)) return JS_EXCEPTION;
		data_size /= sizeof(Float);
		if (data_size % nb_comp) return JS_EXCEPTION;

		if (argc>2) {
			if (JS_ToInt32(c, &interp_type, argv[2])) return JS_EXCEPTION;
		}
	} else {
		if (JS_ToInt32(c, &nb_comp, argv[0])) return JS_EXCEPTION;
	}

	GF_SAFEALLOC(vai, EVG_VAI);
	if (!vai)
		return js_throw_err(c, GF_OUT_OF_MEM);
	vai->nb_comp = nb_comp;
	vai->values = (Float *)data;
	vai->nb_values = data_size;
	if (data)
		vai->ab = JS_DupValue(c, argv[0]);

	vai->prim_idx = (u32) -1;
	vai->interp_type = interp_type;

#ifdef EVG_USE_JS_SHADER
	vai->res = JS_NewObjectClass(c, vaires_class_id);
	JS_SetOpaque(vai->res, &vai->result);
	JS_SetPropertyStr(c, vai->res, "length", JS_NewInt32(c, nb_comp));
#endif
	vai->result.dim = nb_comp;
	if (nb_comp==1) vai->result.comp_type = COMP_X;
	else if (nb_comp==2) vai->result.comp_type = COMP_V2_XY;
	else if (nb_comp==3) vai->result.comp_type = COMP_V3;
	else vai->result.comp_type = COMP_V4;

	obj = JS_NewObjectClass(c, vai_class_id);
	JS_SetOpaque(obj, vai);
	return obj;
}
static void vai_finalize(JSRuntime *rt, JSValue obj)
{
	EVG_VAI *vai = JS_GetOpaque(obj, vai_class_id);
	if (!vai) return;
	JS_FreeValueRT(rt, vai->ab);
#ifdef EVG_USE_JS_SHADER
	JS_FreeValueRT(rt, vai->res);
#endif
	gf_free(vai);
}

static void vai_gc_mark(JSRuntime *rt, JSValueConst obj, JS_MarkFunc *mark_func)
{
	EVG_VAI *vai = JS_GetOpaque(obj, vai_class_id);
	if (!vai) return;
	JS_MarkValue(rt, vai->ab, mark_func);
#ifdef EVG_USE_JS_SHADER
	JS_MarkValue(rt, vai->res, mark_func);
#endif
}

JSClassDef vai_class = {
	.class_name = "VertexAttribInterpolator",
	.finalizer = vai_finalize,
	.gc_mark = vai_gc_mark
};
static const JSCFunctionListEntry vai_funcs[] =
{
	JS_CGETSET_MAGIC_DEF("normalize", NULL, vai_setProperty, 0),
#ifdef EVG_USE_JS_SHADER
	JS_CFUNC_DEF("lerp", 0, vai_lerp),
#endif
};

static JSValue va_constructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	EVG_VA *va;
	JSValue obj;
	u8 *data=NULL;
	u32 data_size;
	u32 nb_comp;
	s32 interp_type = GF_EVG_VAI_VERTEX_INDEX;
	if (argc<2)
		return js_throw_err_msg(c, GF_BAD_PARAM, "Missing parameter / data for VertexAttrib");

	data = evg_get_array(c, argv[0], &data_size);
	if (!data) return JS_EXCEPTION;
	if (JS_ToInt32(c, &nb_comp, argv[1])) return JS_EXCEPTION;
	if (nb_comp>MAX_ATTR_DIM)
		return js_throw_err_msg(c, GF_BAD_PARAM, "Dimension too big, max is %d", MAX_ATTR_DIM);

	if (data_size % sizeof(Float)) return JS_EXCEPTION;
	data_size /= sizeof(Float);
	if (data_size % nb_comp) return JS_EXCEPTION;

	if (argc>2) {
		if (JS_ToInt32(c, &interp_type, argv[2])) return JS_EXCEPTION;
	}

	GF_SAFEALLOC(va, EVG_VA);
	if (!va)
		return js_throw_err(c, GF_OUT_OF_MEM);
	va->nb_comp = nb_comp;
	va->values = (Float *)data;
	va->nb_values = data_size;
	va->ab = JS_DupValue(c, argv[0]);
	va->interp_type = interp_type;
	if (va->nb_comp==1) va->att_type = COMP_FLOAT;
	else if (va->nb_comp==2) va->att_type = COMP_V2_XY;
	else if (va->nb_comp==3) va->att_type = COMP_V3;
	else va->att_type = COMP_V4;

	obj = JS_NewObjectClass(c, va_class_id);
	JS_SetOpaque(obj, va);
	return obj;
}

static JSValue va_setProperty(JSContext *c, JSValueConst obj, JSValueConst value, int magic)
{
	EVG_VA *va = JS_GetOpaque(obj, va_class_id);
	if (!va) return JS_EXCEPTION;
	switch (magic) {
	case 0:
		va->normalize = JS_ToBool(c, value) ? GF_TRUE : GF_FALSE;
		break;
	}
	return JS_UNDEFINED;
}
static void va_finalize(JSRuntime *rt, JSValue obj)
{
	EVG_VA *va = JS_GetOpaque(obj, va_class_id);
	if (!va) return;
	JS_FreeValueRT(rt, va->ab);
	gf_free(va);
}

static void va_gc_mark(JSRuntime *rt, JSValueConst obj, JS_MarkFunc *mark_func)
{
	EVG_VA *va = JS_GetOpaque(obj, va_class_id);
	if (!va) return;
	JS_MarkValue(rt, va->ab, mark_func);
}

JSClassDef va_class = {
	.class_name = "VertexAttrib",
	.finalizer = va_finalize,
	.gc_mark = va_gc_mark
};
static const JSCFunctionListEntry va_funcs[] =
{
	JS_CGETSET_MAGIC_DEF("normalize", NULL, va_setProperty, 0),
};

#ifdef EVG_USE_JS_SHADER

static JSValue vaires_getProperty(JSContext *c, JSValueConst obj, int magic)
{
	EVG_VAIRes *vr = JS_GetOpaque(obj, vaires_class_id);
	if (!vr) return JS_EXCEPTION;
	if (magic<MAX_ATTR_DIM) {
		return JS_NewFloat64(c, vr->values[magic]);
	}
	return JS_UNDEFINED;
}
static const JSCFunctionListEntry vaires_funcs[] =
{
	JS_CGETSET_MAGIC_DEF("x", vaires_getProperty, NULL, 0),
	JS_CGETSET_MAGIC_DEF("y", vaires_getProperty, NULL, 1),
	JS_CGETSET_MAGIC_DEF("z", vaires_getProperty, NULL, 2),
	JS_CGETSET_MAGIC_DEF("w", vaires_getProperty, NULL, 3),
	JS_ALIAS_DEF("r", "x"),
	JS_ALIAS_DEF("g", "y"),
	JS_ALIAS_DEF("b", "z"),
	JS_ALIAS_DEF("a", "w"),
	JS_ALIAS_DEF("s", "x"),
	JS_ALIAS_DEF("t", "y"),
};

JSClassDef vaires_class = {
	.class_name = "VAIResult",
};

#endif //EVG_USE_JS_SHADER

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
	Fixed x, y, w=0, h=0;
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
	if (!nmx)
		return js_throw_err(c, GF_OUT_OF_MEM);
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
	if (!mx)
		return js_throw_err(c, GF_OUT_OF_MEM);
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
	if (!cmx)
		return js_throw_err(c, GF_OUT_OF_MEM);
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
	if (JS_IsArray(c, obj)) {
		u32 i, len;
		v = JS_GetPropertyStr(c, obj, "length");
		res = JS_ToInt32(c, &len, v);
		JS_FreeValue(c, v);
		if (res) return GF_FALSE;
		if (len>4) len=4;
		for (i=0; i<len; i++) {
			Double d;
			v = JS_GetPropertyUint32(c, obj, i);
			res = JS_ToFloat64(c, &d, v);
			if (res) return GF_FALSE;
			JS_FreeValue(c, v);
			if (!i) *r = d;
			else if (i==1) *g = d;
			else if (i==2) *b = d;
			else *a = d;
		}
		return GF_TRUE;
	}
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
	if (!gp) return JS_EXCEPTION;
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
	s32 idx=0;
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
	s32 i;
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

static JSValue path_add_path(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
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
		v = JS_GetPropertyStr(c, dashes, "length");
		JS_ToInt32(c, &dash.num_dash, v);
		JS_FreeValue(c, v);
		pen.dash_set = &dash;
		dash.dashes = gf_malloc(sizeof(Fixed)*dash.num_dash);
		for (i=0; i<(int) dash.num_dash; i++) {
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
	JS_CFUNC_DEF("add_path", 0, path_add_path),
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
	s32 idx=0;
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
	s32 idx=0;
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
	} else if (argc>idx+1) {
		if (JS_ToFloat64(c, &d, argv[idx])) return JS_EXCEPTION;
		rx = FLT2FIX(d);
		if (JS_ToFloat64(c, &d, argv[idx+1])) return JS_EXCEPTION;
		ry = FLT2FIX(d);
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
	if (argc<(s32) idx) return GF_FALSE;
	if (JS_IsString(argv[idx])) {
		GF_Color col;
		const char *str = JS_ToCString(c, argv[idx]);
		col = gf_color_parse(str);
		JS_FreeCString(c, str);
		*a = ((Double)GF_COL_A(col)) / 255;
		*r = ((Double)GF_COL_R(col)) / 255;
		*g = ((Double)GF_COL_G(col)) / 255;
		*b = ((Double)GF_COL_B(col)) / 255;
	} else if (JS_IsObject(argv[idx])) {
		if (!get_color(c, argv[idx], a, r, g, b)) {
			return GF_FALSE;
		}
	} else if (argc>(s32) idx) {
		if (JS_ToFloat64(c, r, argv[idx]))
			return GF_FALSE;
		if (argc>(s32)idx+1) {
			if (JS_ToFloat64(c, g, argv[idx+1]))
				return GF_FALSE;
			if (argc>(s32) idx+2) {
				if (JS_ToFloat64(c, b, argv[idx+2]))
					return GF_FALSE;
				if (argc>(s32)idx+3) {
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
	gf_evg_stencil_set_alpha(stencil, (u8) a);
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
	JS_FreeValueRT(rt, tx->param_fun);
	gf_free(tx);
}

static void texture_gc_mark(JSRuntime *rt, JSValueConst obj, JS_MarkFunc *mark_func)
{
	GF_JSTexture *tx = JS_GetOpaque(obj, texture_class_id);
	if (!tx) return;
	JS_MarkValue(rt, tx->param_fun, mark_func);
}

JSClassDef texture_class = {
	"Texture",
	.finalizer = texture_finalize,
	.gc_mark = texture_gc_mark
};

enum
{
	TX_MAPPING = 0,
	TX_FILTER,
	TX_CMX,
	TX_REPEAT_S,
	TX_REPEAT_T,
	TX_FLIP_X,
	TX_FLIP_Y,
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
	case TX_FLIP_X:
		return JS_NewBool(c, (tx->flags & GF_TEXTURE_FLIP_X));
	case TX_FLIP_Y:
		return JS_NewBool(c, (tx->flags & GF_TEXTURE_FLIP_Y));
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
	case TX_FLIP_X:
		if (JS_ToBool(c, value)) tx->flags |= GF_TEXTURE_FLIP_X;
		else tx->flags &= ~GF_TEXTURE_FLIP_X;
		gf_evg_stencil_set_mapping(tx->stencil, tx->flags);
		return JS_UNDEFINED;
	case TX_FLIP_Y:
		if (JS_ToBool(c, value)) tx->flags |= GF_TEXTURE_FLIP_Y;
		else tx->flags &= ~GF_TEXTURE_FLIP_Y;
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
	case 5:
	default:
		r = v; g = p; b = q; break;
	}

	*dst_r = (u8)(r * 255); // dst_r : 0-255
	*dst_g = (u8)(g * 255); // dst_r : 0-255
	*dst_b = (u8)(b * 255); // dst_r : 0-255
}

enum
{
	EVG_CONV_RGB_TO_HSV=0,
	EVG_CONV_HSV_TO_RGB,
	EVG_CONV_YUV_TO_RGB,
	EVG_CONV_RGB_TO_YUV,
};

static JSValue texture_convert(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv, u32 conv_type)
{
	JSValue nobj;
	u32 i, j, dst_pf, nb_comp;
	GF_JSCanvas *canvas=NULL;
	GF_JSTexture *tx_hsv;
	GF_JSTexture *tx = JS_GetOpaque(obj, texture_class_id);
	if (!tx || !tx->stencil) return JS_EXCEPTION;
	if (argc) {
		canvas = JS_GetOpaque(argv[0], canvas_class_id);
		if (!canvas)
			canvas = JS_GetOpaque(argv[0], canvas3d_class_id);
	}
	if ((conv_type==EVG_CONV_YUV_TO_RGB) || (conv_type==EVG_CONV_RGB_TO_YUV)) {
		if (!canvas) return js_throw_err_msg(c, GF_BAD_PARAM, "Missing canvas parameter for RBG/YUV conversion");
	}

	switch (tx->pf) {
	case GF_PIXEL_ARGB:
	case GF_PIXEL_RGBA:
	case GF_PIXEL_BGRA:
	case GF_PIXEL_ABGR:
	case GF_PIXEL_ALPHAGREY:
	case GF_PIXEL_GREYALPHA:
		dst_pf = GF_PIXEL_RGBA;
		nb_comp = 4;
		break;
	default:
		dst_pf = GF_PIXEL_RGB;
		nb_comp = 3;
		break;
	}

	GF_SAFEALLOC(tx_hsv, GF_JSTexture);
	if (!tx_hsv)
		return js_throw_err(c, GF_OUT_OF_MEM);
	tx_hsv->width = tx->width;
	tx_hsv->height = tx->height;
	tx_hsv->pf = dst_pf;
	tx_hsv->nb_comp = nb_comp;
	gf_pixel_get_size_info(tx_hsv->pf, tx_hsv->width, tx_hsv->height, &tx_hsv->data_size, &tx_hsv->stride, &tx_hsv->stride_uv, NULL, NULL);
	tx_hsv->data = gf_malloc(sizeof(char)*tx_hsv->data_size);
	tx_hsv->owns_data = GF_TRUE;

	for (j=0; j<tx_hsv->height; j++) {
		u8 *dst = tx_hsv->data + j*tx_hsv->stride;
		for (i=0; i<tx_hsv->width; i++) {
			u8 a, r, g, b;
			u32 col = gf_evg_stencil_get_pixel(tx->stencil, i, j);

			if (conv_type == EVG_CONV_RGB_TO_HSV ) {
				a = GF_COL_A(col);
				r = GF_COL_R(col);
				g = GF_COL_G(col);
				b = GF_COL_B(col);
				rgb2hsv(r, g, b, &r, &g, &b);
			} else if (conv_type == EVG_CONV_HSV_TO_RGB ) {
				a = GF_COL_A(col);
				r = GF_COL_R(col);
				g = GF_COL_G(col);
				b = GF_COL_B(col);
				hsv2rgb(r, g, b, &r, &g, &b);
			} else if (conv_type == EVG_CONV_RGB_TO_YUV ) {
				col = gf_evg_argb_to_ayuv(canvas->surface, col);
				a = GF_COL_A(col);
				r = GF_COL_R(col);
				g = GF_COL_G(col);
				b = GF_COL_B(col);
			} else {
				col = gf_evg_ayuv_to_argb(canvas->surface, col);
				a = GF_COL_A(col);
				r = GF_COL_R(col);
				g = GF_COL_G(col);
				b = GF_COL_B(col);
			}
			dst[0] = r;
			dst[1] = g;
			dst[2] = b;
			if (nb_comp==4) {
				dst[3] = a;
				dst += 4;
			} else {
				dst += 3;
			}
		}
	}
	tx_hsv->stencil = gf_evg_stencil_new(GF_STENCIL_TEXTURE);
	gf_evg_stencil_set_texture(tx_hsv->stencil, tx_hsv->data, tx_hsv->width, tx_hsv->height, tx_hsv->stride, tx_hsv->pf);
	nobj = JS_NewObjectClass(c, texture_class_id);
	JS_SetOpaque(nobj, tx_hsv);
	return nobj;
}
static JSValue texture_rgb2hsv(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return texture_convert(c, obj, argc, argv, EVG_CONV_RGB_TO_HSV);
}
static JSValue texture_hsv2rgb(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return texture_convert(c, obj, argc, argv, EVG_CONV_HSV_TO_RGB);
}
static JSValue texture_rgb2yuv(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return texture_convert(c, obj, argc, argv, EVG_CONV_RGB_TO_YUV);
}
static JSValue texture_yuv2rgb(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return texture_convert(c, obj, argc, argv, EVG_CONV_YUV_TO_RGB);
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
	if (!tx_split)
		return js_throw_err(c, GF_OUT_OF_MEM);
	tx_split->width = src.width;
	tx_split->height = src.height;
	tx_split->pf = GF_PIXEL_GREYSCALE;
	tx_split->nb_comp = 1;
	tx_split->stride = tx_split->width;
	tx_split->data_size = tx_split->width * tx_split->height;
	tx_split->data = gf_malloc(sizeof(char) * tx_split->data_size);
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
static JSValue texture_convolution(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	JSValue v, kernv, nobj;
	u32 i, j, kw=0, kh=0, kl=0, hkh, hkw;
	s32 *kdata;
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
	kernv = JS_GetPropertyStr(c, argv[0], "k");
	if (JS_IsUndefined(kernv))
		return JS_EXCEPTION;

	v = JS_GetPropertyStr(c, kernv, "length");
	JS_ToInt32(c, &kl, v);
	JS_FreeValue(c, v);
	if (kl < kw * kh) {
		JS_FreeValue(c, kernv);
		return JS_EXCEPTION;
	}
	kl = kw*kh;
	kdata = gf_malloc(sizeof(s32)*kl);
	for (j=0; j<kh; j++) {
		for (i=0; i<kw; i++) {
			u32 idx = j*kw + i;
			v = JS_GetPropertyUint32(c, kernv, idx);
			JS_ToInt32(c, &kdata[idx] , v);
			JS_FreeValue(c, v);
		}
	}
	JS_FreeValue(c, kernv);

	GF_SAFEALLOC(tx_conv, GF_JSTexture);
	if (!tx_conv)
		return js_throw_err(c, GF_OUT_OF_MEM);
	tx_conv->width = tx->width;
	tx_conv->height = tx->height;
	tx_conv->pf = GF_PIXEL_RGB;
	tx_conv->nb_comp = 3;
	gf_pixel_get_size_info(tx_conv->pf, tx_conv->width, tx_conv->height, &tx_conv->data_size, &tx_conv->stride, &tx_conv->stride_uv, NULL, NULL);
	tx_conv->data = gf_malloc(sizeof(char)*tx_conv->data_size);
	tx_conv->owns_data = GF_TRUE;

	hkh = kh/2;
	hkw = kw/2;
	for (j=0; j<tx_conv->height; j++) {
		u8 *dst = tx_conv->data + j*tx_conv->stride;
		for (i=0; i<tx_conv->width; i++) {
			u32 k, l, nb_pix=0;
			s32 kr = 0;
			s32 kg = 0;
			s32 kb = 0;

			for (k=0; k<kh; k++) {
				if (j+k < hkh) continue;
				if (j+k >= tx_conv->height + hkh) continue;

				for (l=0; l<kw; l++) {
					s32 kv;
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
				if (nb_pix) n *= nb_pix;
				kr = (kr * kl / n);
				kg = (kg * kl / n);
				kb = (kb * kl / n);
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


static JSValue texture_update(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_Err e;
	u32 width=0, height=0, pf=0, stride=0, stride_uv=0;
	u8 *data=NULL;
	u8 *p_u=NULL;
	u8 *p_v=NULL;
	u8 *p_a=NULL;

	GF_JSTexture *tx = JS_GetOpaque(obj, texture_class_id);
	if (!tx || !tx->stencil || !argc) return JS_EXCEPTION;

	if (JS_IsObject(argv[0])) {
		//create from canvas object
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
			e = jsf_get_filter_packet_planes(c, argv[0], &width, &height, &pf, &stride, &stride_uv, (const u8 **)&data, (const u8 **)&p_u, (const u8 **)&p_v, (const u8 **)&p_a);
			if (e) return js_throw_err(c, e);
		} else {
			return js_throw_err(c, GF_BAD_PARAM);
		}
	} else {
		return js_throw_err(c, GF_BAD_PARAM);
	}

	tx->owns_data = GF_FALSE;
	tx->width = width;
	tx->height = height;
	tx->pf = pf;
	tx->stride = stride;
	tx->stride_uv = stride_uv;
	tx->data = data;
	if (p_u || p_v) {
		e = gf_evg_stencil_set_texture_planes(tx->stencil, tx->width, tx->height, tx->pf, data, tx->stride, p_u, p_v, tx->stride_uv, p_a, tx->stride);
		 if (e) return js_throw_err(c, e);
	} else {
		assert(data);
		e = gf_evg_stencil_set_texture(tx->stencil, tx->data, tx->width, tx->height, tx->stride, tx->pf);
		if (e) return js_throw_err(c, e);
	}

	if (tx->pf) {
		tx->nb_comp = gf_pixel_get_nb_comp(tx->pf);
		gf_pixel_get_size_info(tx->pf, tx->width, tx->height, NULL, NULL, NULL, NULL, NULL);
	}
	return JS_UNDEFINED;
}

static JSValue texture_get_pixel_internal(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv, Bool is_float)
{
	Bool as_array=GF_FALSE;
	JSValue ret;
	Double x, y;
	Float r, g, b, a;

	GF_JSTexture *tx = JS_GetOpaque(obj, texture_class_id);
	if (!tx || !tx->stencil || (argc<2) ) return JS_EXCEPTION;

	if (is_float) {
		if (JS_ToFloat64(c, &x, argv[0])) return JS_EXCEPTION;
		if (JS_ToFloat64(c, &y, argv[1])) return JS_EXCEPTION;
	} else {
		s32 _x, _y;
		if (JS_ToInt32(c, &_x, argv[0])) return JS_EXCEPTION;
		if (JS_ToInt32(c, &_y, argv[1])) return JS_EXCEPTION;
		x = ((Float)_x) / tx->width;
		y = ((Float)_y) / tx->height;
	}
	if ((argc>2) && JS_ToBool(c, argv[2]))
		as_array = GF_TRUE;

	gf_evg_stencil_get_pixel_f(tx->stencil, (Float) x, (Float) y, &r, &g, &b, &a);

	if (as_array) {
		ret = JS_NewArray(c);
		JS_SetPropertyStr(c, ret, "length", JS_NewInt32(c, 4) );
		JS_SetPropertyUint32(c, ret, 0, JS_NewFloat64(c, r) );
		JS_SetPropertyUint32(c, ret, 1, JS_NewFloat64(c, g) );
		JS_SetPropertyUint32(c, ret, 2, JS_NewFloat64(c, b) );
		JS_SetPropertyUint32(c, ret, 3, JS_NewFloat64(c, a) );
	} else {
		ret = JS_NewObject(c);
		JS_SetPropertyStr(c, ret, "r", JS_NewFloat64(c, r) );
		JS_SetPropertyStr(c, ret, "g", JS_NewFloat64(c, g) );
		JS_SetPropertyStr(c, ret, "b", JS_NewFloat64(c, b) );
		JS_SetPropertyStr(c, ret, "a", JS_NewFloat64(c, a) );
	}
	return ret;
}
static JSValue texture_get_pixel(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return texture_get_pixel_internal(c, obj, argc, argv, GF_FALSE);
}

static JSValue texture_get_pixelf(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return texture_get_pixel_internal(c, obj, argc, argv, GF_TRUE);
}

static GF_Err texture_load_data(JSContext *c, GF_JSTexture *tx, u8 *data, u32 size)
{
	GF_Err e;
	GF_BitStream *bs;
	u8 *dsi=NULL;
	u32 codecid, width, height, pf, dsi_len, osize;

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
static GF_Err texture_load_file(JSContext *c, GF_JSTexture *tx, const char *fileName, Bool rel_to_script)
{
	u8 *data;
	u32 size;
	GF_Err e;
	char *full_url = NULL;

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
	e = texture_load_data(c, tx, data, size);
	gf_free(data);
	return e;
}

static const JSCFunctionListEntry texture_funcs[] =
{
	JS_CGETSET_MAGIC_DEF("filtering", NULL, texture_setProperty, TX_FILTER),
	JS_CGETSET_MAGIC_DEF("cmx", NULL, texture_setProperty, TX_CMX),
	JS_CGETSET_MAGIC_DEF("mx", NULL, texture_setProperty, TX_MAT),
	JS_CGETSET_MAGIC_DEF("repeat_s", texture_getProperty, texture_setProperty, TX_REPEAT_S),
	JS_CGETSET_MAGIC_DEF("repeat_t", texture_getProperty, texture_setProperty, TX_REPEAT_T),
	JS_CGETSET_MAGIC_DEF("flip_h", texture_getProperty, texture_setProperty, TX_FLIP_X),
	JS_CGETSET_MAGIC_DEF("flip_v", texture_getProperty, texture_setProperty, TX_FLIP_Y),
	JS_CGETSET_MAGIC_DEF("width", texture_getProperty, NULL, TX_WIDTH),
	JS_CGETSET_MAGIC_DEF("height", texture_getProperty, NULL, TX_HEIGHT),
	JS_CGETSET_MAGIC_DEF("pixfmt", texture_getProperty, NULL, TX_PIXFMT),
	JS_CGETSET_MAGIC_DEF("comp", texture_getProperty, NULL, TX_NB_COMP),
	JS_CGETSET_MAGIC_DEF("data", texture_getProperty, NULL, TX_DATA),

	JS_CFUNC_DEF("set_alpha", 0, stencil_set_alpha),
	JS_CFUNC_DEF("set_alphaf", 0, stencil_set_alphaf),
	JS_CFUNC_DEF("rgb2hsv", 0, texture_rgb2hsv),
	JS_CFUNC_DEF("hsv2rgb", 0, texture_hsv2rgb),
	JS_CFUNC_DEF("rgb2yuv", 0, texture_rgb2yuv),
	JS_CFUNC_DEF("yuv2rgb", 0, texture_yuv2rgb),
	JS_CFUNC_DEF("convolution", 0, texture_convolution),
	JS_CFUNC_DEF("split", 0, texture_split),
	JS_CFUNC_DEF("update", 0, texture_update),
	JS_CFUNC_DEF("get_pixelf", 0, texture_get_pixelf),
	JS_CFUNC_DEF("get_pixel", 0, texture_get_pixel),

};


static void evg_param_tex_callback(void *cbk, u32 x, u32 y, Float *r, Float *g, Float *b, Float *a)
{
	Double compv;
	JSValue ret, v, argv[2];
	GF_JSTexture *tx = (GF_JSTexture *)cbk;
	argv[0] = JS_NewInt32(tx->ctx, x);
	argv[1] = JS_NewInt32(tx->ctx, y);
	ret = JS_Call(tx->ctx, tx->param_fun, tx->obj, 2, argv);
	JS_FreeValue(tx->ctx, argv[0]);
	JS_FreeValue(tx->ctx, argv[1]);

	*r = *g = *b = 0.0;
	*a = 1.0;
#define GETCOMP(_comp)\
	v = JS_GetPropertyStr(tx->ctx, ret, #_comp);\
	if (!JS_IsUndefined(v)) {\
		JS_ToFloat64(tx->ctx, &compv, v);\
		JS_FreeValue(tx->ctx, v);\
		*_comp = (Float) compv;\
	}\

	GETCOMP(r)
	GETCOMP(g)
	GETCOMP(b)
	GETCOMP(a)

#undef GETCOMP
	JS_FreeValue(tx->ctx, ret);
}

static JSValue texture_constructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	Bool use_screen_coords=GF_FALSE;
	JSValue obj;
	JSValue tx_fun=JS_UNDEFINED;
	u32 width=0, height=0, pf=0, stride=0, stride_uv=0;
	size_t data_size=0;
	u8 *data=NULL;
	u8 *p_u=NULL;
	u8 *p_v=NULL;
	u8 *p_a=NULL;
	GF_JSTexture *tx;
	GF_SAFEALLOC(tx, GF_JSTexture);
	if (!tx)
		return js_throw_err(c, GF_OUT_OF_MEM);
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
		if (e) {
			if (tx->stencil) gf_evg_stencil_delete(tx->stencil);
			gf_free(tx);
			return js_throw_err_msg(c, e, "Failed to load texture: %s", gf_error_to_string(e));
		}
		goto done;
	}
	if (JS_IsObject(argv[0])) {

		//create from canvas object
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
			GF_Err e = jsf_get_filter_packet_planes(c, argv[0], &width, &height, &pf, &stride, &stride_uv, (const u8 **)&data, (const u8 **)&p_u, (const u8 **)&p_v, (const u8 **)&p_a);
			if (e) goto error;

			if (!stride) {
				gf_pixel_get_size_info(pf, width, height, NULL, &stride, &stride_uv, NULL, NULL);
			}
		} else {
			data = JS_GetArrayBuffer(c, &data_size, argv[0]);
			if (data) {
				GF_Err e = texture_load_data(c, tx, data, (u32) data_size);
				if (e) {
					if (tx->stencil) gf_evg_stencil_delete(tx->stencil);
					gf_free(tx);
					return js_throw_err_msg(c, e, "Failed to load texture: %s", gf_error_to_string(e));
				}
				goto done;
			}
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
		if (JS_IsFunction(c, argv[3])) {
			tx_fun = argv[3];
		}
		else if (JS_IsObject(argv[3])) {
			data = JS_GetArrayBuffer(c, &data_size, argv[3]);
		}
		if (!width || !height || !pf || (!data && JS_IsUndefined(tx_fun)))
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
	} else if (data) {
		if (gf_evg_stencil_set_texture(tx->stencil, tx->data, tx->width, tx->height, tx->stride, tx->pf) != GF_OK)
			goto error;
	} else {
		use_screen_coords = stride ? GF_TRUE : GF_FALSE;
		if (gf_evg_stencil_set_texture_parametric(tx->stencil, tx->width, tx->height, tx->pf, evg_param_tex_callback, tx, use_screen_coords) != GF_OK)
			goto error;
		tx->param_fun = JS_DupValue(c, tx_fun);
		tx->ctx = c;
	}

done:
	if (tx->pf) {
		tx->nb_comp = gf_pixel_get_nb_comp(tx->pf);
		gf_pixel_get_size_info(tx->pf, tx->width, tx->height, NULL, NULL, NULL, NULL, NULL);
	}
	obj = JS_NewObjectClass(c, texture_class_id);
	if (JS_IsException(obj)) return obj;
	JS_SetOpaque(obj, tx);
	tx->obj = obj;
	return obj;

error:
	if (tx->stencil) gf_evg_stencil_delete(tx->stencil);
	gf_free(tx);
	return js_throw_err_msg(c, GF_BAD_PARAM, "Failed to create texture");
}

Bool js_evg_is_texture(JSContext *ctx, JSValue this_obj)
{
	GF_JSTexture *tx = JS_GetOpaque(this_obj, texture_class_id);
	if (!tx) return GF_FALSE;
	return GF_TRUE;
}

Bool js_evg_get_texture_info(JSContext *ctx, JSValue this_obj, u32 *width, u32 *height, u32 *pixfmt, u8 **p_data, u32 *stride, u8 **p_u, u8 **p_v, u32 *stride_uv, u8 **p_a)
{
	GF_JSTexture *tx = JS_GetOpaque(this_obj, texture_class_id);
	if (!tx) return GF_FALSE;
	if (width) *width = tx->width;
	if (height) *height = tx->height;
	if (pixfmt) *pixfmt = tx->pf;
	if (stride) *stride = tx->stride;
	if (stride_uv) *stride_uv = tx->stride_uv;
	if (!tx->data) return GF_TRUE;
	if (p_data) *p_data = tx->data;
	if (p_u) *p_u = NULL;
	if (p_v) *p_v = NULL;
	if (p_a) *p_a = NULL;
	return GF_TRUE;
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
	Fixed cy, ascent, descent, scale_x, ls;
	u32 i, nb_lines;

	if ((txt->path_for_centered == for_centered) && txt->path) {
		return;
	}
	if (txt->path) gf_path_del(txt->path);
	txt->path = NULL;
	if (!txt->font)
		return;

	txt->path_for_centered = for_centered;

	cy = FLT2FIX((txt->font_size * txt->font->baseline) / txt->font->em_size);

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

static void text_set_text_from_value(GF_JSText *txt, GF_Font *font, JSContext *c, JSValueConst value)
{
	const char *str = JS_ToCString(c, value);
	char *start = (char *) str;
	while (start) {
		GF_TextSpan *span;
		char *nline = strchr(str, '\n');
		if (nline) nline[0] = 0;
		span = gf_font_manager_create_span(txt->fm, font, (char *) str, FLT2FIX(txt->font_size), GF_FALSE, GF_FALSE, GF_FALSE, NULL, GF_FALSE, 0, NULL);
		if (span) {
			if (txt->horizontal)
				span->flags |= GF_TEXT_SPAN_HORIZONTAL;
			gf_list_add(txt->spans, span);
		}

		if (!nline) break;
		nline[0] = '\n';
		start = nline + 1;
	}
	JS_FreeCString(c, str);
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
	s32 i, j, nb_lines;
	GF_JSText *txt = JS_GetOpaque(obj, text_class_id);
	if (!txt) return JS_EXCEPTION;
	text_reset(txt);
	if (!argc) return JS_UNDEFINED;

	txt->font = gf_font_manager_set_font(txt->fm, &txt->fontname, 1, txt->styles);
	if (!txt->font) return js_throw_err_msg(c, GF_NOT_FOUND, "Font %s not found and no default font available - check your GPAC configuration", txt->fontname);

	for (i=0; i<argc; i++) {
		if (JS_IsArray(c, argv[i])) {
			JSValue v = JS_GetPropertyStr(c, argv[i], "length");
			JS_ToInt32(c, &nb_lines, v);
			JS_FreeValue(c, v);

			for (j=0; j<nb_lines; j++) {
				v = JS_GetPropertyUint32(c, argv[i], j);
				text_set_text_from_value(txt, txt->font, c, v);
				JS_FreeValue(c, v);
			}
		} else {
			text_set_text_from_value(txt, txt->font, c, argv[i]);
		}
	}

	nb_lines = gf_list_count(txt->spans);
	for (i=0; i<nb_lines; i++) {
		GF_TextSpan *span = gf_list_get(txt->spans, i);
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
			if (txt->max_w < span->bounds.width)
				txt->max_w = span->bounds.width;
			if (txt->max_h < span->bounds.height)
				txt->max_h = span->bounds.height;
			if (txt->max_x < span->bounds.x + span->bounds.width)
				txt->max_x = span->bounds.x + span->bounds.width;
			if (txt->max_y < span->bounds.y + span->bounds.height)
				txt->max_y = span->bounds.y + span->bounds.height;
		}
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
	if (!txt)
		return js_throw_err(c, GF_OUT_OF_MEM);
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



enum
{
	MX_PROP_IDENTITY=0,
	MX_PROP_YAW,
	MX_PROP_PITCH,
	MX_PROP_ROLL,
	MX_PROP_TRANLATE,
	MX_PROP_SCALE,
	MX_PROP_ROTATE,
	MX_PROP_SHEAR,
	MX_PROP_M,
};

static void mx_finalize(JSRuntime *rt, JSValue obj)
{
	GF_Matrix *mx = JS_GetOpaque(obj, matrix_class_id);
	if (mx) gf_free(mx);
}

JSClassDef matrix_class = {
	.class_name = "Matrix",
	.finalizer = mx_finalize
};

#define MAKEVEC(_v) \
		res = JS_NewObject(ctx);\
		JS_SetPropertyStr(ctx, res, "x", JS_NewFloat64(ctx, FIX2FLT(_v.x) ));\
		JS_SetPropertyStr(ctx, res, "y", JS_NewFloat64(ctx, FIX2FLT(_v.y) ));\
		JS_SetPropertyStr(ctx, res, "z", JS_NewFloat64(ctx, FIX2FLT(_v.z) ));\
		return res;\

#define MAKEVEC4(_v) \
		res = JS_NewObject(ctx);\
		JS_SetPropertyStr(ctx, res, "x", JS_NewFloat64(ctx, FIX2FLT(_v.x) ));\
		JS_SetPropertyStr(ctx, res, "y", JS_NewFloat64(ctx, FIX2FLT(_v.y) ));\
		JS_SetPropertyStr(ctx, res, "z", JS_NewFloat64(ctx, FIX2FLT(_v.z) ));\
		JS_SetPropertyStr(ctx, res, "w", JS_NewFloat64(ctx, FIX2FLT(_v.q) ));\
		return res;\

#define MAKERECT(_v) \
		res = JS_NewObject(ctx);\
		JS_SetPropertyStr(ctx, res, "x", JS_NewFloat64(ctx, FIX2FLT(_v.x) ));\
		JS_SetPropertyStr(ctx, res, "y", JS_NewFloat64(ctx, FIX2FLT(_v.y) ));\
		JS_SetPropertyStr(ctx, res, "width", JS_NewFloat64(ctx, FIX2FLT(_v.width) ));\
		JS_SetPropertyStr(ctx, res, "height", JS_NewFloat64(ctx, FIX2FLT(_v.height) ));\
		return res;\

static JSValue mx_getProperty(JSContext *ctx, JSValueConst this_val, int magic)
{
	JSValue res;
	Fixed yaw, pitch, roll;
	GF_Vec tr, sc, sh;
	GF_Vec4 ro;
	u32 i;
	GF_Matrix *mx = JS_GetOpaque(this_val, matrix_class_id);
	if (!mx) return JS_EXCEPTION;
	switch (magic) {
	case MX_PROP_IDENTITY: return JS_NewBool(ctx, gf_mx2d_is_identity(*mx));
	case MX_PROP_YAW:
		gf_mx_get_yaw_pitch_roll(mx, &yaw, &pitch, &roll);
		return JS_NewFloat64(ctx, FIX2FLT(yaw));
	case MX_PROP_PITCH:
		gf_mx_get_yaw_pitch_roll(mx, &yaw, &pitch, &roll);
		return JS_NewFloat64(ctx, FIX2FLT(pitch));
	case MX_PROP_ROLL:
		gf_mx_get_yaw_pitch_roll(mx, &yaw, &pitch, &roll);
		return JS_NewFloat64(ctx, FIX2FLT(roll));

	case MX_PROP_TRANLATE:
		gf_mx_decompose(mx, &tr, &sc, &ro, &sh);
		MAKEVEC(tr)

	case MX_PROP_SCALE:
		gf_mx_decompose(mx, &tr, &sc, &ro, &sh);
		MAKEVEC(sc)

	case MX_PROP_ROTATE:
		gf_mx_decompose(mx, &tr, &sc, &ro, &sh);
		MAKEVEC4(ro)

	case MX_PROP_SHEAR:
		gf_mx_decompose(mx, &tr, &sc, &ro, &sh);
		MAKEVEC(sh)

	case MX_PROP_M:
		res = JS_NewArray(ctx);
		for (i=0; i<16; i++)
			JS_SetPropertyUint32(ctx, res, i, JS_NewFloat64(ctx, mx->m[i]));
		return res;
	}
	return JS_UNDEFINED;
}

static JSValue mx_setProperty(JSContext *ctx, JSValueConst this_val, JSValueConst value, int magic)
{
	JSValue v;
	u32 i, len;
	GF_Matrix *mx = JS_GetOpaque(this_val, matrix_class_id);
	if (!mx) return JS_EXCEPTION;


	switch (magic) {
	case MX_PROP_IDENTITY:
	 	gf_mx_init(*mx);
	 	break;
	case MX_PROP_M:
		if (!JS_IsArray(ctx, value)) return JS_EXCEPTION;
		v = JS_GetPropertyStr(ctx, value, "length");
		len=0;
		JS_ToInt32(ctx, &len, v);
		JS_FreeValue(ctx, v);
		if (len != 16) return JS_EXCEPTION;
		for (i=0; i<len; i++) {
			Double _d;
			v = JS_GetPropertyUint32(ctx, value, i);
			JS_ToFloat64(ctx, &_d, v);
			JS_FreeValue(ctx, v);
			mx->m[i] = FLT2FIX(_d);
		}
	 	break;
	}
	return JS_UNDEFINED;
}


static JSValue mx_copy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Matrix *mx = JS_GetOpaque(this_val, matrix_class_id);
	if (!mx || !argc) return JS_EXCEPTION;
	GF_Matrix *mx2 = JS_GetOpaque(argv[0], matrix_class_id);
	if (!mx2) return JS_EXCEPTION;
	gf_mx_copy(*mx, *mx2);
	return JS_DupValue(ctx, this_val);
}
static JSValue mx_equal(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Matrix *mx = JS_GetOpaque(this_val, matrix_class_id);
	if (!mx || !argc) return JS_EXCEPTION;
	GF_Matrix *mx2 = JS_GetOpaque(argv[0], matrix_class_id);
	if (!mx2) return JS_EXCEPTION;
	if (gf_mx_equal(mx, mx2)) return JS_TRUE;
	return JS_FALSE;
}

#define WGL_GET_VEC3(_x, _y, _z, _arg)\
{\
	JSValue v;\
	v = JS_GetPropertyStr(ctx, _arg, "x");\
	EVG_GET_FLOAT(_x, v);\
	JS_FreeValue(ctx, v);\
	v = JS_GetPropertyStr(ctx, _arg, "y");\
	EVG_GET_FLOAT(_y, v);\
	JS_FreeValue(ctx, v);\
	v = JS_GetPropertyStr(ctx, _arg, "z");\
	EVG_GET_FLOAT(_z, v);\
	JS_FreeValue(ctx, v);\
}

#define WGL_GET_VEC3F(_v, _arg)\
{\
	JSValue v;\
	Double _f;\
	v = JS_GetPropertyStr(ctx, _arg, "x");\
	EVG_GET_FLOAT(_f, v);\
	_v.x = FLT2FIX(_f);\
	JS_FreeValue(ctx, v);\
	v = JS_GetPropertyStr(ctx, _arg, "y");\
	EVG_GET_FLOAT(_f, v);\
	_v.y = FLT2FIX(_f);\
	JS_FreeValue(ctx, v);\
	v = JS_GetPropertyStr(ctx, _arg, "z");\
	EVG_GET_FLOAT(_f, v);\
	_v.z = FLT2FIX(_f);\
	JS_FreeValue(ctx, v);\
}

#define WGL_GET_VEC4(_x, _y, _z, _q, _arg)\
{\
	JSValue v;\
	v = JS_GetPropertyStr(ctx, _arg, "x");\
	EVG_GET_FLOAT(_x, v);\
	JS_FreeValue(ctx, v);\
	v = JS_GetPropertyStr(ctx, _arg, "y");\
	EVG_GET_FLOAT(_y, v);\
	JS_FreeValue(ctx, v);\
	v = JS_GetPropertyStr(ctx, _arg, "z");\
	EVG_GET_FLOAT(_z, v);\
	JS_FreeValue(ctx, v);\
	v = JS_GetPropertyStr(ctx, _arg, "w");\
	if (JS_IsUndefined(v)) v = JS_GetPropertyStr(ctx, _arg, "q");\
	if (JS_IsUndefined(v)) v = JS_GetPropertyStr(ctx, _arg, "angle");\
	EVG_GET_FLOAT(_q, v);\
	JS_FreeValue(ctx, v);\
}


static JSValue mx_translate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Double vx, vy, vz;
	GF_Matrix *mx = JS_GetOpaque(this_val, matrix_class_id);
	if (!mx || !argc) return JS_EXCEPTION;
	if (!JS_IsObject(argv[0])) {
		if (argc<3) return JS_EXCEPTION;
		EVG_GET_FLOAT(vx, argv[0])
		EVG_GET_FLOAT(vy, argv[1])
		EVG_GET_FLOAT(vz, argv[2])
	} else {
		WGL_GET_VEC3(vx, vy, vz, argv[0])
	}
	gf_mx_add_translation(mx, FLT2FIX(vx), FLT2FIX(vy), FLT2FIX(vz));
	return JS_DupValue(ctx, this_val);
}
static JSValue mx_scale(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Double vx, vy, vz;
	GF_Matrix *mx = JS_GetOpaque(this_val, matrix_class_id);
	if (!mx || !argc) return JS_EXCEPTION;
	if (!JS_IsObject(argv[0])) {
		if (argc<3) return JS_EXCEPTION;
		EVG_GET_FLOAT(vx, argv[0])
		EVG_GET_FLOAT(vy, argv[1])
		EVG_GET_FLOAT(vz, argv[2])
	} else {
		WGL_GET_VEC3(vx, vy, vz, argv[0])
	}
	gf_mx_add_scale(mx, FLT2FIX(vx), FLT2FIX(vy), FLT2FIX(vz));
	return JS_DupValue(ctx, this_val);
}
static JSValue mx_rotate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Double vx, vy, vz, angle;
	GF_Matrix *mx = JS_GetOpaque(this_val, matrix_class_id);
	if (!mx || !argc) return JS_EXCEPTION;
	if (!JS_IsObject(argv[0])) {
		if (argc<4) return JS_EXCEPTION;
		EVG_GET_FLOAT(vx, argv[0])
		EVG_GET_FLOAT(vy, argv[1])
		EVG_GET_FLOAT(vz, argv[2])
		EVG_GET_FLOAT(angle, argv[3])
	} else {
		WGL_GET_VEC4(vx, vy, vz, angle, argv[0])
	}
	gf_mx_add_rotation(mx, FLT2FIX(angle), FLT2FIX(vx), FLT2FIX(vy), FLT2FIX(vz));
	return JS_DupValue(ctx, this_val);
}
static JSValue mx_add(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Matrix *mx = JS_GetOpaque(this_val, matrix_class_id);
	if (!mx || !argc) return JS_EXCEPTION;
	GF_Matrix *mx2 = JS_GetOpaque(argv[0], matrix_class_id);
	if (!mx2) return JS_EXCEPTION;
	if ((argc>1) && JS_ToBool(ctx, argv[1])) {
		gf_mx_add_matrix_4x4(mx, mx2);
	} else {
		gf_mx_add_matrix(mx, mx2);
	}
	return JS_DupValue(ctx, this_val);
}

static JSValue mx_inverse(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Matrix *mx = JS_GetOpaque(this_val, matrix_class_id);
	if (!mx) return JS_EXCEPTION;
	if (argc && JS_ToBool(ctx, argv[0])) {
		gf_mx_inverse_4x4(mx);
	} else {
		gf_mx_inverse(mx);
	}
	return JS_DupValue(ctx, this_val);
}

static JSValue mx_transpose(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Matrix *mx = JS_GetOpaque(this_val, matrix_class_id);
	if (!mx) return JS_EXCEPTION;
	gf_mx_transpose(mx);
	return JS_DupValue(ctx, this_val);
}

static JSValue mx_apply(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Fixed width, height, x, y;
	GF_Vec pt;
	GF_Vec4 pt4;
	JSValue v, res;
	GF_Matrix *mx = JS_GetOpaque(this_val, matrix_class_id);
	if (!mx || !argc) return JS_EXCEPTION;
	if (!JS_IsObject(argv[0])) return JS_EXCEPTION;

	/*try rect*/
	v = JS_GetPropertyStr(ctx, argv[0], "width");
	if (!JS_IsUndefined(v)) {
		GF_Rect rc;
		EVG_GET_FLOAT(width, v);
		JS_FreeValue(ctx, v);
		v = JS_GetPropertyStr(ctx, argv[0], "height");
		EVG_GET_FLOAT(height, v);
		JS_FreeValue(ctx, v);
		v = JS_GetPropertyStr(ctx, argv[0], "x");
		EVG_GET_FLOAT(x, v);
		JS_FreeValue(ctx, v);
		v = JS_GetPropertyStr(ctx, argv[0], "y");
		EVG_GET_FLOAT(y, v);
		JS_FreeValue(ctx, v);
		rc.x = FLT2FIX(x);
		rc.y = FLT2FIX(y);
		rc.width = FLT2FIX(width);
		rc.height = FLT2FIX(height);
		gf_mx_apply_rect(mx, &rc);
		MAKERECT(rc)
	}
	JS_FreeValue(ctx, v);

	v = JS_GetPropertyStr(ctx, argv[0], "q");
	if (JS_IsUndefined(v)) v = JS_GetPropertyStr(ctx, argv[0], "w");
	if (JS_IsUndefined(v)) v = JS_GetPropertyStr(ctx, argv[0], "angle");
	if (!JS_IsUndefined(v)) {
		Double _v;
		if (JS_ToFloat64(ctx, &_v, v)) return JS_EXCEPTION;
		pt4.q = FLT2FIX(_v);

		WGL_GET_VEC3F(pt4, argv[0])
		gf_mx_apply_vec_4x4(mx, &pt4);
		MAKEVEC4(pt4)
	}
	JS_FreeValue(ctx, v);

	/*try bbox ?*/

	/*try vec*/
	WGL_GET_VEC3F(pt, argv[0])
	gf_mx_apply_vec(mx, &pt);
	MAKEVEC(pt)

/*

void gf_mx_apply_vec(GF_Matrix *mx, GF_Vec *pt);
void gf_mx_apply_rect(GF_Matrix *_this, GF_Rect *rc);
void gf_mx_apply_bbox(GF_Matrix *mx, GF_BBox *b);
void gf_mx_apply_bbox_sphere(GF_Matrix *mx, GF_BBox *box);
void gf_mx_apply_vec_4x4(GF_Matrix *mx, GF_Vec4 *vec);
*/
	return JS_UNDEFINED;
}

static JSValue mx_ortho(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Double left, right, bottom, top, z_near, z_far;
	GF_Matrix *mx = JS_GetOpaque(this_val, matrix_class_id);
	if (!mx || (argc<6)) return JS_EXCEPTION;
	EVG_GET_FLOAT(left, argv[0])
	EVG_GET_FLOAT(right, argv[1])
	EVG_GET_FLOAT(bottom, argv[2])
	EVG_GET_FLOAT(top, argv[3])
	EVG_GET_FLOAT(z_near, argv[4])
	EVG_GET_FLOAT(z_far, argv[5])
	if ((argc>6) && JS_ToBool(ctx, argv[6])) {
		gf_mx_ortho_reverse_z(mx, FLT2FIX(left), FLT2FIX(right), FLT2FIX(bottom), FLT2FIX(top), FLT2FIX(z_near), FLT2FIX(z_far));
	} else {
		gf_mx_ortho(mx, FLT2FIX(left), FLT2FIX(right), FLT2FIX(bottom), FLT2FIX(top), FLT2FIX(z_near), FLT2FIX(z_far));
	}
	return JS_DupValue(ctx, this_val);
}
static JSValue mx_perspective(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Double fov, ar, z_near, z_far;
	GF_Matrix *mx = JS_GetOpaque(this_val, matrix_class_id);
	if (!mx || (argc<4)) return JS_EXCEPTION;
	EVG_GET_FLOAT(fov, argv[0])
	EVG_GET_FLOAT(ar, argv[1])
	EVG_GET_FLOAT(z_near, argv[2])
	EVG_GET_FLOAT(z_far, argv[3])
	if ((argc>4) && JS_ToBool(ctx, argv[4])) {
		gf_mx_perspective_reverse_z(mx, FLT2FIX(fov), FLT2FIX(ar), FLT2FIX(z_near), FLT2FIX(z_far));
	} else {
		gf_mx_perspective(mx, FLT2FIX(fov), FLT2FIX(ar), FLT2FIX(z_near), FLT2FIX(z_far));
	}
	return JS_DupValue(ctx, this_val);
}
static JSValue mx_lookat(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Vec pos, target, up;
	GF_Matrix *mx = JS_GetOpaque(this_val, matrix_class_id);
	if (!mx || (argc<3) ) return JS_EXCEPTION;

	WGL_GET_VEC3F(pos, argv[0]);
	WGL_GET_VEC3F(target, argv[1]);
	WGL_GET_VEC3F(up, argv[2]);

	gf_mx_lookat(mx, pos, target, up);
	return JS_DupValue(ctx, this_val);
}

static const JSCFunctionListEntry mx_funcs[] =
{
	JS_CGETSET_MAGIC_DEF("identity", mx_getProperty, mx_setProperty, MX_PROP_IDENTITY),
	JS_CGETSET_MAGIC_DEF("m", mx_getProperty, mx_setProperty, MX_PROP_M),
	JS_CGETSET_MAGIC_DEF("yaw", mx_getProperty, NULL, MX_PROP_YAW),
	JS_CGETSET_MAGIC_DEF("pitch", mx_getProperty, NULL, MX_PROP_PITCH),
	JS_CGETSET_MAGIC_DEF("roll", mx_getProperty, NULL, MX_PROP_ROLL),
	JS_CGETSET_MAGIC_DEF("dec_translate", mx_getProperty, NULL, MX_PROP_TRANLATE),
	JS_CGETSET_MAGIC_DEF("dec_scale", mx_getProperty, NULL, MX_PROP_SCALE),
	JS_CGETSET_MAGIC_DEF("dec_rotate", mx_getProperty, NULL, MX_PROP_ROTATE),
	JS_CGETSET_MAGIC_DEF("dec_shear", mx_getProperty, NULL, MX_PROP_SHEAR),
	JS_CFUNC_DEF("copy", 0, mx_copy),
	JS_CFUNC_DEF("equal", 0, mx_equal),
	JS_CFUNC_DEF("translate", 0, mx_translate),
	JS_CFUNC_DEF("scale", 0, mx_scale),
	JS_CFUNC_DEF("rotate", 0, mx_rotate),
	JS_CFUNC_DEF("add", 0, mx_add),
	JS_CFUNC_DEF("inverse", 0, mx_inverse),
	JS_CFUNC_DEF("transpose", 0, mx_transpose),
	JS_CFUNC_DEF("apply", 0, mx_apply),
	JS_CFUNC_DEF("ortho", 0, mx_ortho),
	JS_CFUNC_DEF("perspective", 0, mx_perspective),
	JS_CFUNC_DEF("lookat", 0, mx_lookat),
};
/*
void gf_mx_rotate_vector(GF_Matrix *mx, GF_Vec *pt);
*/

static JSValue mx_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	JSValue res;
	GF_Matrix *mx;
	GF_SAFEALLOC(mx, GF_Matrix);
	if (!mx)
		return js_throw_err(ctx, GF_OUT_OF_MEM);
	gf_mx_init(*mx);
	res = JS_NewObjectClass(ctx, matrix_class_id);
	JS_SetOpaque(res, mx);
	if (argc) {
		GF_Matrix *from = JS_GetOpaque(argv[0], matrix_class_id);
		if (from) {
			gf_mx_copy(*mx, *from);
		} else if (argc>=3) {
			GF_Vec x_axis, y_axis, z_axis;
			WGL_GET_VEC3F(x_axis, argv[0])
			WGL_GET_VEC3F(y_axis, argv[1])
			WGL_GET_VEC3F(z_axis, argv[2])
			gf_mx_rotation_matrix_from_vectors(mx, x_axis, y_axis,z_axis);
		}
	}
	return res;
}


static JSValue evg_pixel_size(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 pfmt=0;
	if (!argc) return js_throw_err_msg(ctx, GF_BAD_PARAM, "missing pixel format parameter");
	if (JS_IsString(argv[0])) {
		const char *s = JS_ToCString(ctx, argv[0]);
		if (s) {
			pfmt = gf_pixel_fmt_parse(s);
			JS_FreeCString(ctx, s);
		}
	} else if (JS_IsNumber(argv[0])) {
		JS_ToInt32(ctx, &pfmt, argv[0]);
	}
	if (!pfmt) return js_throw_err_msg(ctx, GF_BAD_PARAM, "missing pixel format parameter");
	return JS_NewInt32(ctx, gf_pixel_get_bytes_per_pixel(pfmt));
}


static int js_evg_load_module(JSContext *c, JSModuleDef *m)
{
	JSValue ctor;
	JSValue proto;
	JSValue global;

	if (!canvas_class_id) {
		JSRuntime *rt = JS_GetRuntime(c);

		JS_NewClassID(&canvas_class_id);
		JS_NewClass(rt, canvas_class_id, &canvas_class);

		JS_NewClassID(&path_class_id);
		JS_NewClass(rt, path_class_id, &path_class);

		JS_NewClassID(&mx2d_class_id);
		JS_NewClass(rt, mx2d_class_id, &mx2d_class);

		JS_NewClassID(&colmx_class_id);
		JS_NewClass(rt, colmx_class_id, &colmx_class);

		JS_NewClassID(&stencil_class_id);
		JS_NewClass(rt, stencil_class_id, &stencil_class);

		JS_NewClassID(&texture_class_id);
		JS_NewClass(rt, texture_class_id, &texture_class);

		JS_NewClassID(&text_class_id);
		JS_NewClass(rt, text_class_id, &text_class);

		JS_NewClassID(&matrix_class_id);
		JS_NewClass(rt, matrix_class_id, &matrix_class);

		JS_NewClassID(&canvas3d_class_id);
		JS_NewClass(rt, canvas3d_class_id, &canvas3d_class);

		JS_NewClassID(&shader_class_id);
		JS_NewClass(rt, shader_class_id, &shader_class);

		JS_NewClassID(&vai_class_id);
		JS_NewClass(rt, vai_class_id, &vai_class);

		JS_NewClassID(&va_class_id);
		JS_NewClass(rt, va_class_id, &va_class);

#ifdef EVG_USE_JS_SHADER
		JS_NewClassID(&fragment_class_id);
		JS_NewClass(rt, fragment_class_id, &fragment_class);

		JS_NewClassID(&vertex_class_id);
		JS_NewClass(rt, vertex_class_id, &vertex_class);

		JS_NewClassID(&vaires_class_id);
		JS_NewClass(rt, vaires_class_id, &vaires_class);
#endif// EVG_USE_JS_SHADER

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

	proto = JS_NewObject(c);
    JS_SetPropertyFunctionList(c, proto, mx_funcs, countof(mx_funcs));
    JS_SetClassProto(c, matrix_class_id, proto);

	proto = JS_NewObject(c);
    JS_SetPropertyFunctionList(c, proto, canvas3d_funcs, countof(canvas3d_funcs));
    JS_SetClassProto(c, canvas3d_class_id, proto);

#ifdef EVG_USE_JS_SHADER
	proto = JS_NewObject(c);
    JS_SetPropertyFunctionList(c, proto, fragment_funcs, countof(fragment_funcs));
    JS_SetClassProto(c, fragment_class_id, proto);

	proto = JS_NewObject(c);
    JS_SetPropertyFunctionList(c, proto, vertex_funcs, countof(vertex_funcs));
    JS_SetClassProto(c, vertex_class_id, proto);

	proto = JS_NewObject(c);
	JS_SetPropertyFunctionList(c, proto, vaires_funcs, countof(vaires_funcs));
	JS_SetClassProto(c, vaires_class_id, proto);
#endif

	proto = JS_NewObject(c);
    JS_SetPropertyFunctionList(c, proto, shader_funcs, countof(shader_funcs));
    JS_SetClassProto(c, shader_class_id, proto);

	proto = JS_NewObject(c);
    JS_SetPropertyFunctionList(c, proto, vai_funcs, countof(vai_funcs));
    JS_SetClassProto(c, vai_class_id, proto);

	proto = JS_NewObject(c);
    JS_SetPropertyFunctionList(c, proto, va_funcs, countof(va_funcs));
    JS_SetClassProto(c, va_class_id, proto);


	global = JS_GetGlobalObject(c);
	JS_SetPropertyStr(c, global, "GF_GRADIENT_MODE_PAD", JS_NewInt32(c, GF_GRADIENT_MODE_PAD));
	JS_SetPropertyStr(c, global, "GF_GRADIENT_MODE_STREAD", JS_NewInt32(c, GF_GRADIENT_MODE_SPREAD));
	JS_SetPropertyStr(c, global, "GF_GRADIENT_MODE_REPEAT", JS_NewInt32(c, GF_GRADIENT_MODE_REPEAT));

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

	JS_SetPropertyStr(c, global, "GF_EVG_POINTS", JS_NewInt32(c, GF_EVG_POINTS));
	JS_SetPropertyStr(c, global, "GF_EVG_POLYGON", JS_NewInt32(c, GF_EVG_POLYGON));
	JS_SetPropertyStr(c, global, "GF_EVG_LINES", JS_NewInt32(c, GF_EVG_LINES));
	JS_SetPropertyStr(c, global, "GF_EVG_TRIANGLES", JS_NewInt32(c, GF_EVG_TRIANGLES));
	JS_SetPropertyStr(c, global, "GF_EVG_QUADS", JS_NewInt32(c, GF_EVG_QUADS));
	JS_SetPropertyStr(c, global, "GF_EVG_LINE_STRIP", JS_NewInt32(c, GF_EVG_LINE_STRIP));
	JS_SetPropertyStr(c, global, "GF_EVG_TRIANGLE_STRIP", JS_NewInt32(c, GF_EVG_TRIANGLE_STRIP));
	JS_SetPropertyStr(c, global, "GF_EVG_TRIANGLE_FAN", JS_NewInt32(c, GF_EVG_TRIANGLE_FAN));

	JS_SetPropertyStr(c, global, "GF_EVG_SHADER_FRAGMENT", JS_NewInt32(c, GF_EVG_SHADER_FRAGMENT));
	JS_SetPropertyStr(c, global, "GF_EVG_SHADER_VERTEX", JS_NewInt32(c, GF_EVG_SHADER_VERTEX));

	JS_SetPropertyStr(c, global, "GF_EVGDEPTH_NEVER", JS_NewInt32(c, GF_EVGDEPTH_NEVER));
	JS_SetPropertyStr(c, global, "GF_EVGDEPTH_ALWAYS", JS_NewInt32(c, GF_EVGDEPTH_ALWAYS));
	JS_SetPropertyStr(c, global, "GF_EVGDEPTH_ALWAYS", JS_NewInt32(c, GF_EVGDEPTH_ALWAYS));
	JS_SetPropertyStr(c, global, "GF_EVGDEPTH_EQUAL", JS_NewInt32(c, GF_EVGDEPTH_EQUAL));
	JS_SetPropertyStr(c, global, "GF_EVGDEPTH_NEQUAL", JS_NewInt32(c, GF_EVGDEPTH_NEQUAL));
	JS_SetPropertyStr(c, global, "GF_EVGDEPTH_LESS", JS_NewInt32(c, GF_EVGDEPTH_LESS));
	JS_SetPropertyStr(c, global, "GF_EVGDEPTH_LESS_EQUAL", JS_NewInt32(c, GF_EVGDEPTH_LESS_EQUAL));
	JS_SetPropertyStr(c, global, "GF_EVGDEPTH_GREATER", JS_NewInt32(c, GF_EVGDEPTH_GREATER));
	JS_SetPropertyStr(c, global, "GF_EVGDEPTH_GREATER_EQUAL", JS_NewInt32(c, GF_EVGDEPTH_GREATER_EQUAL));

	JS_SetPropertyStr(c, global, "GF_EVG_VAI_VERTEX_INDEX", JS_NewInt32(c, GF_EVG_VAI_VERTEX_INDEX));
	JS_SetPropertyStr(c, global, "GF_EVG_VAI_VERTEX", JS_NewInt32(c, GF_EVG_VAI_VERTEX));
	JS_SetPropertyStr(c, global, "GF_EVG_VAI_PRIMITIVE", JS_NewInt32(c, GF_EVG_VAI_PRIMITIVE));

	JS_SetPropertyStr(c, global, "GF_EVGDEPTH_DISABLE", JS_NewInt32(c, GF_EVGDEPTH_DISABLE));
	JS_SetPropertyStr(c, global, "GF_EVGDEPTH_NEVER", JS_NewInt32(c, GF_EVGDEPTH_NEVER));
	JS_SetPropertyStr(c, global, "GF_EVGDEPTH_ALWAYS", JS_NewInt32(c, GF_EVGDEPTH_ALWAYS));
	JS_SetPropertyStr(c, global, "GF_EVGDEPTH_EQUAL", JS_NewInt32(c, GF_EVGDEPTH_EQUAL));
	JS_SetPropertyStr(c, global, "GF_EVGDEPTH_NEQUAL", JS_NewInt32(c, GF_EVGDEPTH_NEQUAL));
	JS_SetPropertyStr(c, global, "GF_EVGDEPTH_LESS", JS_NewInt32(c, GF_EVGDEPTH_LESS));
	JS_SetPropertyStr(c, global, "GF_EVGDEPTH_LESS_EQUAL", JS_NewInt32(c, GF_EVGDEPTH_LESS_EQUAL));
	JS_SetPropertyStr(c, global, "GF_EVGDEPTH_GREATER", JS_NewInt32(c, GF_EVGDEPTH_GREATER));
	JS_SetPropertyStr(c, global, "GF_EVGDEPTH_GREATER_EQUAL", JS_NewInt32(c, GF_EVGDEPTH_GREATER_EQUAL));

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
	ctor = JS_NewCFunction2(c, canvas3d_constructor, "Canvas3D", 1, JS_CFUNC_constructor, 0);
    JS_SetModuleExport(c, m, "Canvas3D", ctor);
	ctor = JS_NewCFunction2(c, text_constructor, "Text", 1, JS_CFUNC_constructor, 0);
    JS_SetModuleExport(c, m, "Text", ctor);
	ctor = JS_NewCFunction2(c, mx_constructor, "Matrix", 1, JS_CFUNC_constructor, 0);
    JS_SetModuleExport(c, m, "Matrix", ctor);
	ctor = JS_NewCFunction2(c, vai_constructor, "VertexAttribInterpolator", 1, JS_CFUNC_constructor, 0);
    JS_SetModuleExport(c, m, "VertexAttribInterpolator", ctor);
	ctor = JS_NewCFunction2(c, va_constructor, "VertexAttrib", 1, JS_CFUNC_constructor, 0);
    JS_SetModuleExport(c, m, "VertexAttrib", ctor);

	ctor = JS_NewCFunction2(c, evg_pixel_size, "PixelSize", 1, JS_CFUNC_generic, 0);
    JS_SetModuleExport(c, m, "PixelSize", ctor);

	return 0;
}

void qjs_module_init_evg(JSContext *ctx)
{
	JSModuleDef *m;
	m = JS_NewCModule(ctx, "evg", js_evg_load_module);
	if (!m) return;

	JS_AddModuleExport(ctx, m, "Canvas");
	JS_AddModuleExport(ctx, m, "Path");
	JS_AddModuleExport(ctx, m, "Matrix2D");
	JS_AddModuleExport(ctx, m, "ColorMatrix");
	JS_AddModuleExport(ctx, m, "SolidBrush");
	JS_AddModuleExport(ctx, m, "LinearGradient");
	JS_AddModuleExport(ctx, m, "RadialGradient");
	JS_AddModuleExport(ctx, m, "Texture");
	JS_AddModuleExport(ctx, m, "Text");
	JS_AddModuleExport(ctx, m, "Matrix");
	JS_AddModuleExport(ctx, m, "Canvas3D");
    JS_AddModuleExport(ctx, m, "VertexAttribInterpolator");
    JS_AddModuleExport(ctx, m, "VertexAttrib");
    JS_AddModuleExport(ctx, m, "PixelSize");
    return;
}

#else
void qjs_module_init_evg(void *ctx)
{

}
#endif

