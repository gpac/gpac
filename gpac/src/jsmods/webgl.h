/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2019
 *			All rights reserved
 *
 *  This file is part of GPAC / WebGL JavaScript extension
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

#include "../scenegraph/qjs_common.h"
#include "../compositor/gl_inc.h"
#include <gpac/filters.h>

typedef enum {
	WGL_POWER_DEFAULT=0,
	WGL_POWER_LOWPOWER,
	WGL_POWER_HIGHPERF,
} WebGLPowerPreference;

typedef enum {
	WGL_DEPTH_NO=0,
	WGL_DEPTH_YES,
	WGL_DEPTH_TEXTURE,
} WebGLDepthAttach;

typedef struct
{
	Bool alpha;
    WebGLDepthAttach depth;
    Bool stencil;
    Bool antialias;
    Bool premultipliedAlpha;
    Bool preserveDrawingBuffer;
    WebGLPowerPreference powerPreference;
    Bool failIfMajorPerformanceCaveat;
    Bool desynchronized;
    Bool primary;
} GF_WebGLContextAttributes;

typedef struct
{
	GF_FilterFrameInterface tex_f_ifce;
	GF_FilterFrameInterface depth_f_ifce;
	/*internal*/
	GF_WebGLContextAttributes creation_attrs;
	GF_WebGLContextAttributes actual_attrs;
	GLint tex_id, depth_id, stencil_id, fbo_id;
	u32 width, height;

	JSValue canvas;

	JSContext *ctx;
	JSValue tex_frame_flush;
	JSValue depth_frame_flush;

	GF_List *all_objects;
	GF_List *named_textures;
	GLint active_program;
	GLint active_texture;

	struct _wgl_named_texture *bound_named_texture;
	struct __wgl_object *bound_texture;
	u32 bound_texture_target;

	u32 fetch_required_pfmt, pix_stride;
	u8 *pix_data, *pix_line;
} GF_WebGLContext;

typedef struct __wgl_object
{
	GLuint gl_id;
	GF_WebGLContext *par_ctx;
	JSValue obj; //object reference
	JSClassID class_id; //object class
	u32 tx_height; //for textures
	u8 flip_y; //for textures
} GF_WebGLObject;


typedef struct _wgl_named_texture
{
	GF_WebGLContext *par_ctx;
	char *tx_name;
	GF_GLTextureWrapper tx;

	u8 shader_attached;
	u8 flip_y;
} GF_WebGLNamedTexture;

Bool js_evg_get_texture_info(JSContext *ctx, JSValue this_obj, u32 *width, u32 *height, u32 *pixfmt, u8 **p_data, u32 *stride, u8 **p_u, u8 **p_v, u32 *stride_uv, u8 **p_a);

Bool js_evg_is_texture(JSContext *ctx, JSValue this_obj);


typedef GLuint WebGLContextAttributes;
typedef GF_WebGLObject *WebGLProgram;
typedef GF_WebGLObject *WebGLShader;
typedef GF_WebGLObject *WebGLBuffer;
typedef GF_WebGLObject *WebGLFramebuffer;
typedef GF_WebGLObject *WebGLRenderbuffer;
typedef GF_WebGLObject *WebGLTexture;
typedef GF_WebGLObject *WebGLUniformLocation;

#define WGL_NO_ERROR	0
#define WGL_INVALID_ENUM 0x0500
#define WGL_INVALID_VALUE 0x0501
#define WGL_INVALID_OPERATION 0x0502
#define WGL_OUT_OF_MEMORY 0x0505

#define GL_UNPACK_FLIP_Y_WEBGL	0x9240


enum
{
	WebGLRenderingContextBase_PROP_canvas,
	WebGLRenderingContextBase_PROP_drawingBufferWidth,
	WebGLRenderingContextBase_PROP_drawingBufferHeight,
};


#define WGL_GET_BOOL(_name, _jsval) _name = JS_ToBool(ctx, _jsval)
#define WGL_GET_U32(_name, _jsval) if (JS_ToInt32(ctx, &_name, _jsval)) return js_throw_err(ctx, WGL_INVALID_VALUE)
#define WGL_GET_S32(_name, _jsval) if (JS_ToInt32(ctx, &_name, _jsval)) return js_throw_err(ctx, WGL_INVALID_VALUE)
#define WGL_GET_U64(_name, _jsval) if (JS_ToInt64(ctx, &_name, _jsval)) return js_throw_err(ctx, WGL_INVALID_VALUE)
#define WGL_GET_S64(_name, _jsval) if (JS_ToInt64(ctx, &_name, _jsval)) return js_throw_err(ctx, WGL_INVALID_VALUE)
#define WGL_GET_S16(_name, _jsval) { s32 _v; if (JS_ToInt32(ctx, &_v, _jsval)) return js_throw_err(ctx, WGL_INVALID_VALUE); _name = (s16) _v; }
#define WGL_GET_U16(_name, _jsval) { s32 _v; if (JS_ToInt32(ctx, &_v, _jsval)) return js_throw_err(ctx, WGL_INVALID_VALUE); _name = (u16) _v; }
#define WGL_GET_FLOAT(_name, _jsval) { Double _v; if (JS_ToFloat64(ctx, &_v, _jsval)) return js_throw_err(ctx, WGL_INVALID_VALUE); _name = (Float) _v; }
#define WGL_GET_FLOAT_CLAMP(_name, _jsval) { Double _v; if (JS_ToFloat64(ctx, &_v, _jsval)) return js_throw_err(ctx, WGL_INVALID_VALUE); _name = (Float) (_v>1.0 ? 1.0 : (_v<0 ? 0.0 : _v) ); }

#define WGL_GET_GLID(_name, _jsval, _class_id) {\
	if (!JS_IsNull(_jsval)) {\
		GF_WebGLObject *_gl_obj = JS_GetOpaque(_jsval, _class_id);\
		if (!_gl_obj) return js_throw_err(ctx, WGL_INVALID_VALUE);\
		if (_gl_obj->par_ctx != JS_GetOpaque(this_val, WebGLRenderingContextBase_class_id)) return js_throw_err(ctx, WGL_INVALID_OPERATION);\
		_name = _gl_obj->gl_id;\
	} else {\
		_name = 0;\
	}\
}

#define WGL_GET_STRING(_name, _jsval) _name = JS_ToCString(ctx, _jsval)

#define WGL_CHECK_CONTEXT \
	if (!JS_GetOpaque(this_val, WebGLRenderingContextBase_class_id)) return js_throw_err(ctx, WGL_INVALID_OPERATION);


#endif

