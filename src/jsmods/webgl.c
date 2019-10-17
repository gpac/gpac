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


/*
	ANY CHANGE TO THE API MUST BE REFLECTED IN THE DOCUMENTATION IN gpac/share/doc/idl/webgl.idl
	(no way to define inline JS doc with doxygen)
*/

#include <gpac/setup.h>
#ifdef GPAC_HAS_QJS

#ifndef GPAC_DISABLE_3D

#include "webgl.h"

JSClassID WebGLRenderingContextBase_class_id;

static void webgl_finalize(JSRuntime *rt, JSValue obj)
{
	u32 i, count;
	GF_WebGLContext *glctx = JS_GetOpaque(obj, WebGLRenderingContextBase_class_id);
	if (!glctx) return;
	JS_FreeValueRT(rt, glctx->canvas);
	JS_FreeValueRT(rt, glctx->tex_frame_flush);
	JS_FreeValueRT(rt, glctx->depth_frame_flush);

	count = gf_list_count(glctx->all_objects);
	for (i=0; i<count; i++) {
		GF_WebGLObject *glo = gf_list_get(glctx->all_objects, i);
		glo->par_ctx = NULL;
		if (!JS_IsUndefined(glo->obj))
			JS_FreeValueRT(rt, glo->obj);
	}
	gf_list_del(glctx->all_objects);
	count = gf_list_count(glctx->named_textures);
	for (i=0; i<count; i++) {
		GF_WebGLNamedTexture *named_tx = gf_list_get(glctx->named_textures, i);
		named_tx->par_ctx = NULL;
	}
	gf_list_del(glctx->named_textures);

	glDeleteTextures(1, &glctx->tex_id);
	glDeleteRenderbuffers(1, &glctx->depth_id);
	glDeleteFramebuffers(1, &glctx->fbo_id);
	gf_free(glctx);
}
static void webgl_gc_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func)
{
	u32 i, count;
	GF_WebGLContext *glctx = JS_GetOpaque(val, WebGLRenderingContextBase_class_id);
	if (!glctx) return;
	if (!JS_IsUndefined(glctx->tex_frame_flush))
		JS_MarkValue(rt, glctx->depth_frame_flush, mark_func);
	if (!JS_IsUndefined(glctx->depth_frame_flush))
		JS_MarkValue(rt, glctx->tex_frame_flush, mark_func);
	if (!JS_IsUndefined(glctx->canvas))
		JS_MarkValue(rt, glctx->canvas, mark_func);
	count = gf_list_count(glctx->all_objects);
	for (i=0; i<count; i++) {
		GF_WebGLObject *glo = gf_list_get(glctx->all_objects, i);
		if (!JS_IsUndefined(glo->obj))
			JS_MarkValue(rt, glo->obj, mark_func);
	}
}
JSClassDef WebGLRenderingContextBase_class =
{
	.class_name = "WebGLContext",
	.finalizer = webgl_finalize,
	.gc_mark = webgl_gc_mark,
};

JSClassID WebGLProgram_class_id;
JSClassID WebGLShader_class_id;
JSClassID WebGLBuffer_class_id;
JSClassID WebGLFramebuffer_class_id;
JSClassID WebGLRenderbuffer_class_id;
JSClassID WebGLTexture_class_id;
JSClassID WebGLUniformLocation_class_id;
JSClassID NamedTexture_class_id;

#define DEF_WGLOBJ_CLASS1(_name, _destr) \
static void _name##_finalize(JSRuntime *rt, JSValue obj)\
{\
	GF_WebGLObject *glo = JS_GetOpaque(obj, _name##_class_id);\
	if (!glo) return;\
	if (glo->gl_id) _destr(glo->gl_id);\
	if (glo->par_ctx) gf_list_del_item(glo->par_ctx->all_objects, glo);\
	gf_free(glo);\
}\
JSClassDef _name##_class =\
{\
	.class_name = "#_name",\
	.finalizer = _name##_finalize,\
};

#define DEF_WGLOBJ_CLASS2(_name, _destr) \
static void _name##_finalize(JSRuntime *rt, JSValue obj)\
{\
	GF_WebGLObject *glo = JS_GetOpaque(obj, _name##_class_id);\
	if (!glo) return;\
	if (glo->gl_id) _destr(1, &glo->gl_id);\
	gf_free(glo);\
}\
JSClassDef _name##_class =\
{\
	.class_name = "#_name",\
	.finalizer = _name##_finalize,\
};

#define DEF_WGLOBJ_CLASS3(_name) \
static void _name##_finalize(JSRuntime *rt, JSValue obj)\
{\
	GF_WebGLObject *glo = JS_GetOpaque(obj, _name##_class_id);\
	if (!glo) return;\
	gf_free(glo);\
}\
JSClassDef _name##_class =\
{\
	.class_name = "#_name",\
	.finalizer = _name##_finalize,\
};


DEF_WGLOBJ_CLASS1(WebGLProgram, glDeleteProgram)
DEF_WGLOBJ_CLASS1(WebGLShader, glDeleteShader)
DEF_WGLOBJ_CLASS2(WebGLBuffer, glDeleteBuffers)
DEF_WGLOBJ_CLASS2(WebGLFramebuffer, glDeleteFramebuffers)
DEF_WGLOBJ_CLASS2(WebGLRenderbuffer, glDeleteRenderbuffers)
DEF_WGLOBJ_CLASS2(WebGLTexture, glDeleteTextures)
DEF_WGLOBJ_CLASS3(WebGLUniformLocation)


#undef DEF_WGLOBJ_CLASS1
#undef DEF_WGLOBJ_CLASS2
#undef DEF_WGLOBJ_CLASS3

static void NamedTexture_finalize(JSRuntime *rt, JSValue obj)
{
	GF_WebGLNamedTexture *named_tx = JS_GetOpaque(obj, NamedTexture_class_id);
	if (!named_tx) return;
	if (named_tx->par_ctx)
		gf_list_del_item(named_tx->par_ctx->named_textures, named_tx);

	if (named_tx->nb_textures) glDeleteTextures(named_tx->nb_textures, named_tx->textures);
	gf_free(named_tx);
}
JSClassDef NamedTexture_class =
{
	.class_name = "NamedTexture",
	.finalizer = NamedTexture_finalize
};



Bool WGL_LOAD_INT32_VEC(JSContext *ctx, JSValue val, s32 **values, u32 *v_size, u32 dim)
{
	JSValue v;
	int res;
	u32 i, len;
	if (!JS_IsArray(ctx, val)) return GF_FALSE;
	v = JS_GetPropertyStr(ctx, val, "length");
	res = JS_ToInt32(ctx, &len, v);
	JS_FreeValue(ctx, v);
	if (res) return GF_FALSE;

	if (! *values) {
		*values = gf_malloc(sizeof(s32) * len);
		*v_size = len;
	} else {
		if (len>dim) len=dim;
	}
	for (i=0; i<len; i++) {
		v = JS_GetPropertyUint32(ctx, val, i);
		res = JS_ToInt32(ctx, & ( (*values)[i]), v);
		JS_FreeValue(ctx, v);
		if (res) return GF_FALSE;
	}

	return GF_TRUE;
}

Bool WGL_LOAD_FLOAT_VEC(JSContext *ctx, JSValue val, Float **values, u32 *v_size, u32 dim, Bool is_matrix)
{
	JSValue v;
	int res;
	u32 i, len;
	if (!JS_IsArray(ctx, val)) return GF_FALSE;
	v = JS_GetPropertyStr(ctx, val, "length");
	res = JS_ToInt32(ctx, &len, v);
	JS_FreeValue(ctx, v);
	if (res) return GF_FALSE;

	if (! *values) {
		*values = gf_malloc(sizeof(Float) * len);
		if (is_matrix)
			*v_size = len/dim/dim;
		else
			*v_size = len/dim;
	} else {
		if (len>dim) len = dim;
	}
	for (i=0; i<len; i++) {
		Double aval;
		v = JS_GetPropertyUint32(ctx, val, i);
		res = JS_ToFloat64(ctx, &aval, v);
		JS_FreeValue(ctx, v);
		if (res) return GF_FALSE;
		(*values) [i] = (Float) aval;
	}
	return GF_TRUE;
}

uint8_t *wgl_GetArrayBuffer(JSContext *ctx, u32 *size, JSValueConst obj)
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

static JSValue WebGLRenderingContextBase_getProperty(JSContext *ctx, JSValueConst obj, int magic)
{
	GF_WebGLContext *glc = JS_GetOpaque(obj, WebGLRenderingContextBase_class_id);
	if (!glc) return js_throw_err(ctx, WGL_INVALID_OPERATION);

	switch (magic) {
	case WebGLRenderingContextBase_PROP_canvas:
		return JS_DupValue(ctx, glc->canvas);
	case WebGLRenderingContextBase_PROP_drawingBufferWidth:
		return JS_NewInt32(ctx, glc->width);
	case WebGLRenderingContextBase_PROP_drawingBufferHeight:
		return JS_NewInt32(ctx, glc->height);
	}
	return JS_UNDEFINED;
}

static JSValue webgl_getActiveAttriborUniform(JSContext *ctx, GLuint program, u32 index, Bool is_attr)
{
	char szAttName[1001];
	GLint size, att_len=0;
	GLenum type;
	JSValue info;
	if (is_attr) {
		glGetActiveAttrib(program, index, 1000, &att_len, &size, &type, szAttName);
	} else {
		glGetActiveUniform(program, index, 1000, &att_len, &size, &type, szAttName);
	}
	if (att_len>1000) return js_throw_err(ctx, WGL_INVALID_VALUE);
	szAttName[att_len] = 0;
	info = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, info, "size", JS_NewInt32(ctx, size));
	JS_SetPropertyStr(ctx, info, "type", JS_NewInt32(ctx, type));
	JS_SetPropertyStr(ctx, info, "name", JS_NewString(ctx, szAttName));
	return info;
}
JSValue webgl_getActiveAttrib(JSContext *ctx, GLuint program, u32 index)
{
	return webgl_getActiveAttriborUniform(ctx, program, index, GF_TRUE);
}
JSValue webgl_getActiveUniform(JSContext *ctx, GLuint program, u32 index)
{
	return webgl_getActiveAttriborUniform(ctx, program, index, GF_FALSE);
}
JSValue webgl_getAttachedShaders(JSContext *ctx, GF_WebGLContext *glc, GLuint program)
{
	JSValue ret;
	GLuint shaders[20];
	u32 i, scount=0;
	if (!glc) return js_throw_err(ctx, WGL_INVALID_OPERATION);

	glGetAttachedShaders(program, 20, &scount, shaders);
	if (!scount) return JS_NULL;
	ret = JS_NewArray(ctx);
	for (i=0; i<scount; i++) {
		u32 j, count = gf_list_count(glc->all_objects);
		for (j=0; j<count; j++) {
			GF_WebGLObject *glo = gf_list_get(glc->all_objects, j);
			if (glo->class_id != WebGLShader_class_id) continue;
			if (glo->gl_id == shaders[i]) {
				JS_SetPropertyUint32(ctx, ret, i, JS_DupValue(ctx, glo->obj) );
				break;
			}
		}
	}
	return JS_UNDEFINED;
}

/*WebGL 1.0 autogen code*/
#include "WebGLRenderingContextBase.c"


static JSValue wgl_getContextAttributes(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JSValue ret;
	GF_WebGLContext *glc = JS_GetOpaque(this_val, WebGLRenderingContextBase_class_id);
	if (!glc) return js_throw_err(ctx, WGL_INVALID_OPERATION);

	ret = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, ret, "alpha", JS_NewBool(ctx, glc->actual_attrs.alpha));
	if (glc->actual_attrs.depth== WGL_DEPTH_TEXTURE) {
		JS_SetPropertyStr(ctx, ret, "depth", JS_NewString(ctx, "texture"));
	} else {
		JS_SetPropertyStr(ctx, ret, "depth", JS_NewBool(ctx, glc->actual_attrs.depth));
	}
	JS_SetPropertyStr(ctx, ret, "stencil", JS_NewBool(ctx, glc->actual_attrs.stencil));
	JS_SetPropertyStr(ctx, ret, "antialias", JS_NewBool(ctx, glc->actual_attrs.antialias));
	JS_SetPropertyStr(ctx, ret, "premultipliedAlpha", JS_NewBool(ctx, glc->actual_attrs.premultipliedAlpha));
	JS_SetPropertyStr(ctx, ret, "preserveDrawingBuffer", JS_NewBool(ctx, glc->actual_attrs.preserveDrawingBuffer));
	JS_SetPropertyStr(ctx, ret, "failIfMajorPerformanceCaveat", JS_NewBool(ctx, glc->actual_attrs.failIfMajorPerformanceCaveat));
	JS_SetPropertyStr(ctx, ret, "desynchronized", JS_NewBool(ctx, glc->actual_attrs.desynchronized));
	return ret;
}

static JSValue wgl_isContextLost(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return JS_FALSE;
}
static JSValue wgl_getSupportedExtensions(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *gl_exts;
	GF_WebGLContext *glc = JS_GetOpaque(this_val, WebGLRenderingContextBase_class_id);
	if (!glc) return js_throw_err(ctx, WGL_INVALID_OPERATION);
	gl_exts = (const char *) glGetString(GL_EXTENSIONS);
	return JS_NewString(ctx, gl_exts);
}

static JSValue wgl_getExtension(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *gl_exts, *ext;
	Bool found = GF_FALSE;
	GF_WebGLContext *glc = JS_GetOpaque(this_val, WebGLRenderingContextBase_class_id);
	if (!glc) return js_throw_err(ctx, WGL_INVALID_OPERATION);
	if (!argc) return js_throw_err(ctx, WGL_INVALID_VALUE);
	ext = JS_ToCString(ctx, argv[0]);

	gl_exts = (const char *) glGetString(GL_EXTENSIONS);
	if (strstr(gl_exts, ext)) {
		found = GF_TRUE;
		GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("[WebGL] getExtension not yet implemented, cannot fetch extension for %s\n", ext));
	}

	JS_FreeCString(ctx, ext);
	if (!found) return JS_NULL;
	return JS_NULL;
}

static JSValue wgl_getBufferParameter(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 target = 0;
	u32 pname = 0;
	GLint params;
	WGL_CHECK_CONTEXT
	if (argc<2) return js_throw_err(ctx, WGL_INVALID_VALUE);
	WGL_GET_U32(target, argv[0]);
	WGL_GET_U32(pname, argv[1]);
	glGetBufferParameteriv(target, pname, &params);
	return JS_NewInt32(ctx, params);
}
static JSValue wgl_getParameter(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s32 ints[4];
	u32 i, count;
	JSValue ret;
	Float floats[4];
	GLboolean bools[4];
	u32 pname = 0;
	u32 nb_floats = 0;
	u32 nb_ints = 0;
	u32 nb_bools = 0;
	const char *str;
	GF_WebGLContext *glc = JS_GetOpaque(this_val, WebGLRenderingContextBase_class_id);
	if (!glc) return js_throw_err(ctx, WGL_INVALID_OPERATION);
	if (argc<1) return js_throw_err(ctx, WGL_INVALID_VALUE);
	WGL_GET_U32(pname, argv[0]);

	switch (pname) {
	//Floatx2
	case GL_ALIASED_LINE_WIDTH_RANGE:
	case GL_ALIASED_POINT_SIZE_RANGE:
	case GL_DEPTH_RANGE:
		glGetFloatv(pname, floats);
		nb_floats = 2;
		break;
	//WebGLBuffer
	case GL_ARRAY_BUFFER_BINDING:
	case GL_ELEMENT_ARRAY_BUFFER_BINDING:
		glGetIntegerv(pname, ints);
		if (!ints[0]) return JS_NULL;
		count = gf_list_count(glc->all_objects);
		for (i=0; i<count; i++) {
			GF_WebGLObject *ob = gf_list_get(glc->all_objects, i);
			if (ob->class_id==WebGLBuffer_class_id) {
				if (ob->gl_id==ints[0]) return JS_DupValue(ctx, ob->obj);
			}
		}
		return js_throw_err(ctx, WGL_INVALID_VALUE);

	//Floatx4
	case GL_BLEND_COLOR:
	case GL_COLOR_CLEAR_VALUE:
		glGetFloatv(pname, floats);
		nb_floats = 4;
		break;
	//boolx4
	case GL_COLOR_WRITEMASK:
		glGetBooleanv(pname, bools);
		nb_bools = 4;
		break;

	//uint array
	case GL_COMPRESSED_TEXTURE_FORMATS:
	{
		glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, ints);
		s32 *tx_ints = gf_malloc(sizeof(s32)*ints[0]);
		glGetIntegerv(pname, tx_ints);
		ret = JS_NewArray(ctx);
		for (i=0; i<ints[0]; i++) {
			JS_SetPropertyUint32(ctx, ret, i, JS_NewInt32(ctx, tx_ints[i]));
		}
		gf_free(ints);
		return ret;
	}
		break;
	//WebGLProgram
	case GL_CURRENT_PROGRAM:
		glGetIntegerv(pname, ints);
		if (!ints[0]) return JS_NULL;
		count = gf_list_count(glc->all_objects);
		for (i=0; i<count; i++) {
			GF_WebGLObject *ob = gf_list_get(glc->all_objects, i);
			if (ob->class_id==WebGLProgram_class_id) {
				if (ob->gl_id==ints[0]) return JS_DupValue(ctx, ob->obj);
			}
		}
		return js_throw_err(ctx, WGL_INVALID_VALUE);

	//WebGLFramebuffer
	case GL_FRAMEBUFFER_BINDING:
		glGetIntegerv(pname, ints);
		if (!ints[0]) return JS_NULL;
		count = gf_list_count(glc->all_objects);
		for (i=0; i<count; i++) {
			GF_WebGLObject *ob = gf_list_get(glc->all_objects, i);
			if (ob->class_id==WebGLFramebuffer_class_id) {
				if (ob->gl_id==ints[0]) return JS_DupValue(ctx, ob->obj);
			}
		}
		return js_throw_err(ctx, WGL_INVALID_VALUE);

	//WebGLRenderbuffer
	case GL_RENDERBUFFER_BINDING:
		glGetIntegerv(pname, ints);
		if (!ints[0]) return JS_NULL;
		count = gf_list_count(glc->all_objects);
		for (i=0; i<count; i++) {
			GF_WebGLObject *ob = gf_list_get(glc->all_objects, i);
			if (ob->class_id==WebGLRenderbuffer_class_id) {
				if (ob->gl_id==ints[0]) return JS_DupValue(ctx, ob->obj);
			}
		}
		return js_throw_err(ctx, WGL_INVALID_VALUE);

	//WebGLTexture
	case GL_TEXTURE_BINDING_2D:
	case GL_TEXTURE_BINDING_CUBE_MAP:
		glGetIntegerv(pname, ints);
		if (!ints[0]) return JS_NULL;
		count = gf_list_count(glc->all_objects);
		for (i=0; i<count; i++) {
			GF_WebGLObject *ob = gf_list_get(glc->all_objects, i);
			if (ob->class_id==WebGLTexture_class_id) {
				if (ob->gl_id==ints[0]) return JS_DupValue(ctx, ob->obj);
			}
		}
		//todo: do we want to expose
		return js_throw_err(ctx, WGL_INVALID_VALUE);

	//bool
	case GL_BLEND:
	case GL_CULL_FACE:
	case GL_DEPTH_TEST:
	case GL_DEPTH_WRITEMASK:
	case GL_DITHER:
	case GL_POLYGON_OFFSET_FILL:
	case GL_SAMPLE_ALPHA_TO_COVERAGE:
	case GL_SAMPLE_COVERAGE:
	case GL_SAMPLE_COVERAGE_INVERT:
	case GL_SCISSOR_TEST:
	case GL_STENCIL_TEST:
#if 0
	case GL_UNPACK_FLIP_Y_WEBGL:
	case GL_UNPACK_PREMULTIPLY_ALPHA_WEBGL:
#endif
		glGetBooleanv(pname, bools);
		return bools[0] ? JS_TRUE : JS_FALSE;

	//floats
	case GL_DEPTH_CLEAR_VALUE:
	case GL_LINE_WIDTH:
	case GL_POLYGON_OFFSET_FACTOR:
	case GL_POLYGON_OFFSET_UNITS:
	case GL_SAMPLE_COVERAGE_VALUE:
		glGetFloatv(pname, floats);
		return JS_NewFloat64(ctx, floats[0]);
	//intx2
	case GL_MAX_VIEWPORT_DIMS:
		glGetIntegerv(pname, ints);
		nb_ints = 2;
		break;
	//intx4
	case GL_SCISSOR_BOX:
	case GL_VIEWPORT:
		glGetIntegerv(pname, ints);
		nb_ints = 4;
		break;
	//strings
	case GL_RENDERER:
	case GL_SHADING_LANGUAGE_VERSION:
	case GL_VENDOR:
	case GL_VERSION:
		str = glGetString(pname);
		return JS_NewString(ctx, str ? str : "");

	//all the rest is enum or int
	default:
		glGetIntegerv(pname, ints);
		return JS_NewInt32(ctx, ints[0]);
		break;
	}

	if (nb_floats) {
		ret = JS_NewArray(ctx);
		for (i=0; i<nb_floats; i++) {
			JS_SetPropertyUint32(ctx, ret, i, JS_NewFloat64(ctx, floats[i]));
		}
		return ret;
	}

	if (nb_ints) {
		ret = JS_NewArray(ctx);
		for (i=0; i<nb_ints; i++) {
			JS_SetPropertyUint32(ctx, ret, i, JS_NewInt32(ctx, ints[i]));
		}
		return ret;
	}

	if (nb_bools) {
		ret = JS_NewArray(ctx);
		for (i=0; i<nb_bools; i++) {
			JS_SetPropertyUint32(ctx, ret, i, JS_NewBool(ctx, bools[i]));
		}
		return ret;
	}
	return JS_UNDEFINED;
}

static JSValue wgl_getFramebufferAttachmentParameter(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 target = 0;
	u32 attachment = 0;
	u32 pname = 0;
	GLint params;
	GF_WebGLContext *glc = JS_GetOpaque(this_val, WebGLRenderingContextBase_class_id);
	if (!glc) return js_throw_err(ctx, WGL_INVALID_OPERATION);
	if (argc<3) return js_throw_err(ctx, WGL_INVALID_VALUE);
	WGL_GET_U32(target, argv[0]);
	WGL_GET_U32(attachment, argv[1]);
	WGL_GET_U32(pname, argv[2]);
	glGetFramebufferAttachmentParameteriv (target, attachment, pname, &params);
	if (pname==GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME) {
		u32 i, count;
		if (!params) return JS_NULL;
		count = gf_list_count(glc->all_objects);
		for (i=0; i<count; i++) {
			GF_WebGLObject *ob = gf_list_get(glc->all_objects, i);
			if (ob->class_id==WebGLTexture_class_id) {
				if (ob->gl_id == params) return JS_DupValue(ctx, ob->obj);
			} else if (ob->class_id==WebGLRenderbuffer_class_id) {
				if (ob->gl_id == params) return JS_DupValue(ctx, ob->obj);
			}
		}
		return js_throw_err(ctx, WGL_INVALID_VALUE);
	}
	return JS_NewInt32(ctx, params);
}
static JSValue wgl_getProgramParameter(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GLint program=0;
	u32 pname = 0;
	GLint params;
	WGL_CHECK_CONTEXT
	if (argc<2) return js_throw_err(ctx, WGL_INVALID_VALUE);
	WGL_GET_GLID(program, argv[0], WebGLProgram_class_id);
	WGL_GET_U32(pname, argv[1]);
	glGetProgramiv(program, pname, &params);
	switch (pname) {
	case GL_DELETE_STATUS:
	case GL_LINK_STATUS:
	case GL_VALIDATE_STATUS:
		return params ? JS_TRUE : JS_FALSE;
	default:
		break;
	}
	return JS_NewInt32(ctx, params);
}
static JSValue wgl_getRenderbufferParameter(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 target = 0;
	u32 pname = 0;
	GLint params;
	WGL_CHECK_CONTEXT
	if (argc<2) return js_throw_err(ctx, WGL_INVALID_VALUE);
	WGL_GET_U32(target, argv[0]);
	WGL_GET_U32(pname, argv[1]);
	glGetRenderbufferParameteriv(target, pname, &params);
	return JS_NewInt32(ctx, params);
}
static JSValue wgl_getShaderParameter(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GLint shader=0;
	u32 pname = 0;
	GLint params;
	WGL_CHECK_CONTEXT
	if (argc<2) return js_throw_err(ctx, WGL_INVALID_VALUE);
	WGL_GET_GLID(shader, argv[0], WebGLShader_class_id);
	WGL_GET_U32(pname, argv[1]);
	glGetShaderiv(shader, pname, &params);
	switch (pname) {
	case GL_DELETE_STATUS:
	case GL_COMPILE_STATUS:
		return params ? JS_TRUE : JS_FALSE;
	default:
		break;
	}
	return JS_NewInt32(ctx, params);
}
static JSValue wgl_getShaderPrecisionFormat(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JSValue ret;
	u32 shader_type = 0;
	u32 prec_type = 0;
	GLint range[2];
	GLint precision;
	WGL_CHECK_CONTEXT
	if (argc<2) return js_throw_err(ctx, WGL_INVALID_VALUE);
	WGL_GET_U32(shader_type, argv[0]);
	WGL_GET_U32(prec_type, argv[1]);

#if defined(GPAC_USE_GLES2)
	glGetShaderPrecisionFormat(shader_type, prec_type, range, &precision);
#else
	range[0] = 0;
	range[1] = 128;
	precision = 128;
#endif
	ret = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, ret, "rangeMin", JS_NewInt32(ctx, range[0]));
	JS_SetPropertyStr(ctx, ret, "rangeMax", JS_NewInt32(ctx, range[1]));
	JS_SetPropertyStr(ctx, ret, "precision", JS_NewInt32(ctx, precision));
	return ret;
}
static JSValue wgl_getInfoLog(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, u32 type)
{
	JSValue ret;
	int info_length;
	char *info;
	GLint program_shader=0;
	WGL_CHECK_CONTEXT
	if (argc<1) return js_throw_err(ctx, WGL_INVALID_VALUE);
	if (type==0) {
		WGL_GET_GLID(program_shader, argv[0], WebGLProgram_class_id);
		glGetProgramiv(program_shader, GL_INFO_LOG_LENGTH, &info_length);
	} else {
		WGL_GET_GLID(program_shader, argv[0], WebGLShader_class_id);
		if (type==1) {
			glGetShaderiv(program_shader, GL_INFO_LOG_LENGTH, &info_length);
		} else {
			glGetShaderiv(program_shader, GL_SHADER_SOURCE_LENGTH, &info_length);
		}
	}
	info = gf_malloc(sizeof(char)*(info_length+1));
	if (type==0) {
		glGetProgramInfoLog(program_shader, info_length, &info_length, info);
	} else if (type==1) {
		glGetShaderInfoLog(program_shader, info_length, &info_length, info);
	} else {
		glGetShaderSource(program_shader, info_length, &info_length, info);
	}
	info[info_length] = 0;
	ret = JS_NewString(ctx, info);
	gf_free(info);
	return ret;
}

static JSValue wgl_getProgramInfoLog(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return wgl_getInfoLog(ctx, this_val, argc, argv, 0);
}
static JSValue wgl_getShaderInfoLog(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return wgl_getInfoLog(ctx, this_val, argc, argv, 1);
}
static JSValue wgl_getShaderSource(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return wgl_getInfoLog(ctx, this_val, argc, argv, 2);
}
static JSValue wgl_getTexParameter(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 target = 0;
	u32 pname = 0;
	GLint params;
	WGL_CHECK_CONTEXT
	if (argc<2) return js_throw_err(ctx, WGL_INVALID_VALUE);
	WGL_GET_U32(target, argv[0]);
	WGL_GET_U32(pname, argv[1]);
	glGetTexParameteriv(target, pname, &params);
	return JS_NewInt32(ctx, params);
}
static JSValue wgl_getUniform(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_throw_err_msg(ctx, WGL_INVALID_VALUE, "[WebGL] getUniform not yet implemented, patch welcome");
}
static JSValue wgl_getVertexAttrib(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JSValue ret;
	u32 i, count;
	u32 index = 0;
	u32 pname = 0;
	Float floats[4];
	s32 ints[4];
	GF_WebGLContext *glc = JS_GetOpaque(this_val, WebGLRenderingContextBase_class_id);
	if (!glc) return js_throw_err(ctx, WGL_INVALID_OPERATION);
	if (argc<2) return js_throw_err(ctx, WGL_INVALID_VALUE);
	WGL_GET_U32(index, argv[0]);
	WGL_GET_U32(pname, argv[1]);
	switch (pname) {
	case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
		glGetVertexAttribiv(index, pname, ints);
		if (!ints[0]) return JS_NULL;
		count = gf_list_count(glc->all_objects);
		for (i=0; i<count; i++) {
			GF_WebGLObject *ob = gf_list_get(glc->all_objects, i);
			if (ob->class_id==WebGLBuffer_class_id) {
				if (ob->gl_id==ints[0]) return JS_DupValue(ctx, ob->obj);
			}
		}
		return js_throw_err(ctx, WGL_INVALID_VALUE);
	case GL_CURRENT_VERTEX_ATTRIB:
		glGetVertexAttribfv(index, pname, floats);
		ret = JS_NewArray(ctx);
		for (i=0; i<4; i++)
			JS_SetPropertyUint32(ctx, ret, i, JS_NewFloat64(ctx, floats[i]));
		return ret;
	case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
	case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
		glGetVertexAttribiv(index, pname, ints);
		return ints[0] ? JS_TRUE : JS_FALSE;
	case GL_VERTEX_ATTRIB_ARRAY_SIZE:
	case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
	case GL_VERTEX_ATTRIB_ARRAY_TYPE:
		glGetVertexAttribiv(index, pname, ints);
		return JS_NewInt32(ctx, ints[0]);
	}
	return js_throw_err(ctx, WGL_INVALID_VALUE);
}
static JSValue wgl_getVertexAttribOffset(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 index = 0;
	u32 pname = 0;
	void *ptr = NULL;
	WGL_CHECK_CONTEXT
	if (argc<2) return js_throw_err(ctx, WGL_INVALID_VALUE);
	WGL_GET_U32(index, argv[0]);
	WGL_GET_U32(pname, argv[1]);
	glGetVertexAttribPointerv(index, pname, &ptr);
	return JS_NewInt64(ctx, (u64) ptr);
}
static JSValue wgl_readPixels(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s32 x = 0;
	s32 y = 0;
	u32 width = 0;
	u32 height = 0;
	u32 format = 0;
	u32 type = 0;
	u32 req_size=0;
	u8 *ab;
	u32 ab_size;
	WGL_CHECK_CONTEXT
	if (argc<7) return js_throw_err(ctx, WGL_INVALID_VALUE);
	WGL_GET_S32(x, argv[0]);
	WGL_GET_S32(y, argv[1]);
	WGL_GET_U32(width, argv[2]);
	WGL_GET_U32(height, argv[3]);
	WGL_GET_U32(format, argv[4]);
	WGL_GET_U32(type, argv[5]);
	ab = wgl_GetArrayBuffer(ctx, &ab_size, argv[6]);
	if (!ab) return js_throw_err(ctx, WGL_INVALID_VALUE);

	req_size = width * height;
	if (format==GL_RGB) {
		req_size *= 3;
	} else if (format==GL_RGBA) {
		req_size *= 4;
	} else {
		return js_throw_err(ctx, WGL_INVALID_OPERATION);
	}
	if (type==GL_UNSIGNED_BYTE) {
	} else if (type==GL_UNSIGNED_SHORT) {
		req_size *= 2;
	} else if (type==GL_FLOAT) {
		req_size *= 4;
	} else {
		return js_throw_err(ctx, WGL_INVALID_OPERATION);
	}
	if (req_size>ab_size) {
		return js_throw_err(ctx, WGL_INVALID_OPERATION);
	}
	glReadPixels(x, y, width, height, format, type, ab);

	return JS_UNDEFINED;
}

GF_WebGLNamedTexture *wgl_locate_named_tx(GF_WebGLContext *glc, char *name)
{
	u32 i, count = gf_list_count(glc->named_textures);
	for (i=0; i<count; i++) {
		GF_WebGLNamedTexture *tx = gf_list_get(glc->named_textures, i);
		if (!strcmp(tx->tx_name, name)) return tx;
	}
	return NULL;
}

static JSValue wgl_shaderSource(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GLint shader=0;
	Bool has_gptx=GF_FALSE;
	char *source, *gf_source=NULL;
	const char *final_source;
	u32 len = 0;
	u32 count, i;
	GF_WebGLContext *glc = JS_GetOpaque(this_val, WebGLRenderingContextBase_class_id);
	if (!glc) return js_throw_err(ctx, WGL_INVALID_OPERATION);
	if (argc<2) return js_throw_err(ctx, WGL_INVALID_VALUE);
	WGL_GET_GLID(shader, argv[0], WebGLShader_class_id);
	source = (char *) JS_ToCString(ctx, argv[1]);
	if (!source) return js_throw_err(ctx, WGL_INVALID_VALUE);

	count = gf_list_count(glc->named_textures);
	for (i=0; i<count; i++) {
		u32 namelen;
		GF_WebGLNamedTexture *named_tx = gf_list_get(glc->named_textures, i);
		char *a_source = source;
		namelen = strlen(named_tx->tx_name);

		while (a_source) {
			char *loc = strstr(source, "texture2D");
			if (!loc) break;
			loc += 9;
			while (loc[0] && strchr(" (\n\t", loc[0])) loc++;
			if (strchr(" ,)\n\t", loc[namelen])) {
				has_gptx = GF_TRUE;
				break;
			}
			a_source = loc;
		}
		if (has_gptx) break;
	}

	if (has_gptx) {
		char *o_src;
		char *main_fn = NULL;
		char *start = strstr(source, "void main(");
		if (!start) {
			JS_FreeCString(ctx, source);
			return js_throw_err_msg(ctx, WGL_INVALID_VALUE, "Missing main in shader");
		}
		start[0] = 0;
		//strip all sample2D declarations for GPACTextures
		o_src = source;
		while (o_src) {
			char c;
			Bool found = GF_FALSE;
			char *sep, *start = strstr(o_src, "uniform sampler2D ");
			if (!start) {
				gf_dynstrcat(&gf_source, o_src, NULL);
				break;
			}
			sep = start + strlen("uniform sampler2D ");
			for (i=0; i<count; i++) {
				u32 namelen;
				GF_WebGLNamedTexture *named_tx = gf_list_get(glc->named_textures, i);
				namelen = strlen(named_tx->tx_name);
				if (strncmp(sep, named_tx->tx_name, namelen)) continue;

				if (strchr(" \n\t;", sep[namelen])) {
					sep += namelen;
					while (sep[0] && strchr(" ;", sep[0])) sep++;
					start[0] = 0;
					gf_dynstrcat(&gf_source, o_src, NULL);
					start[0] = 'u';
					o_src = sep;
					found = GF_TRUE;
					break;
				}
				if (found) break;
			}
			if (found) continue;
			c = sep[0];
			sep[0] = 0;
			gf_dynstrcat(&gf_source, o_src, NULL);
			sep[0] = c;
			o_src = sep;
		}

		start[0] = 'v';

		while (start && start[0]) {
			char c;
			char *next;
			GF_WebGLNamedTexture *named_tx;
			char *end_gfTx;
			char *has_gfTx = strstr(start, "texture2D");
			if (!has_gfTx) {
				gf_dynstrcat(&main_fn, start, NULL);
				break;
			}

			end_gfTx = has_gfTx + strlen("texture2D");
			while (end_gfTx[0] && strchr(" (\n", end_gfTx[0]))
				end_gfTx++;
			next = end_gfTx;
			while (next[0] && !strchr(" (,)\n", next[0]))
				next++;

			c = next[0];
			next[0] = 0;
			named_tx = wgl_locate_named_tx(glc, end_gfTx);
			if (!named_tx || !named_tx->pix_fmt) {
				JSValue ret = js_throw_err_msg(ctx, WGL_INVALID_VALUE, "NamedTexture %s undefined, cannot create shader", has_gfTx);
				JS_FreeCString(ctx, source);
				gf_free(gf_source);
				return ret;
			}
			next[0] = c;

			has_gfTx[0] = 0;
			gf_dynstrcat(&main_fn, start, NULL);
			has_gfTx[0] = 'g';

			gf_dynstrcat(&main_fn, named_tx->tx_name, NULL);
			gf_dynstrcat(&main_fn, "_sample(", NULL);
			while (next[0] && strchr(" ,", next[0]))
				next++;

			wgl_insert_fragment_shader(named_tx, &gf_source);

			start = next;
		}
		gf_dynstrcat(&gf_source, main_fn, NULL);
		gf_free(main_fn);
		final_source = gf_source;
	} else {
		final_source = source;
	}
	len = (u32) strlen(final_source);
	glShaderSource(shader, 1, &final_source, &len);

	JS_FreeCString(ctx, source);
	if (gf_source) gf_free(gf_source);
	return JS_UNDEFINED;
}

static JSValue wgl_texImage2D(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 target = 0;
	s32 level = 0;
	s32 internalformat = 0;
	u32 width = 0;
	u32 height = 0;
	s32 border = 0;
	u32 format = 0;
	u32 type = 0;
	u32 pixfmt;
	u32 stride;
	u32 stride_uv;
	u8 *p_u, *p_v, *p_a;
	u8 *pix_buf;
	u32 pix_buf_size=0;

	GF_WebGLContext *glc = JS_GetOpaque(this_val, WebGLRenderingContextBase_class_id);
	if (!glc) return js_throw_err(ctx, WGL_INVALID_OPERATION);

	if (argc<6) return js_throw_err(ctx, WGL_INVALID_VALUE);

	WGL_GET_U32(target, argv[0]);
	WGL_GET_S32(level, argv[1]);
	WGL_GET_S32(internalformat, argv[2]);
	/*from texture object*/
	if (JS_IsObject(argv[5])) {

		WGL_GET_U32(format, argv[3]);
		WGL_GET_U32(type, argv[4]);
	}
	/*from array buffer*/
	else {
		if (argc==8) {

		} else if ((argc<9) || (!JS_IsObject(argv[8]) && !JS_IsNull(argv[8]))) {
			return js_throw_err(ctx, WGL_INVALID_VALUE);
		}

		WGL_GET_U32(width, argv[3]);
		WGL_GET_U32(height, argv[4]);
		WGL_GET_S32(border, argv[5]);
		WGL_GET_U32(format, argv[6]);
		WGL_GET_U32(type, argv[7]);
		pix_buf = wgl_GetArrayBuffer(ctx, &pix_buf_size, argv[8]);

		glTexImage2D(target, level, internalformat, width, height, border, format, type, pix_buf);
		return JS_UNDEFINED;
	}

	/*bound texture is a named texture, use tx.upload() */
	if (glc->bound_named_texture) {
		return wgl_named_texture_upload(ctx, argv[5], glc->bound_named_texture);
	}

	/*check if this is an EVG texture*/
	if (js_evg_get_texture_info(ctx, argv[5], &width, &height, &pixfmt, &pix_buf, &stride, &p_u, &p_v, &stride_uv, &p_a)) {
		switch (pixfmt) {
		case GF_PIXEL_GREYSCALE:
			format = GL_LUMINANCE;
			type = GL_TEXTURE_2D;
			break;
		case GF_PIXEL_ALPHAGREY:
		case GF_PIXEL_GREYALPHA:
			format = GL_LUMINANCE_ALPHA;
			type = GL_UNSIGNED_BYTE;
			break;
		case GF_PIXEL_RGB:
			format = GL_RGB;
			type = GL_UNSIGNED_BYTE;
			break;
		case GF_PIXEL_RGBA:
			format = GL_RGBA;
			type = GL_UNSIGNED_BYTE;
			break;
		default:
			return js_throw_err_msg(ctx, WGL_INVALID_ENUM, "[WebGL] Pixel format %s not yet mapped to texImage2D", gf_pixel_fmt_name(pixfmt) );
		}
		internalformat = GL_RGBA;
		target = GL_TEXTURE_2D;
		glTexImage2D(target, level, internalformat, width, height, border, format, type, pix_buf);
		return JS_UNDEFINED;
	}
	/*otherwise not supported*/
	return js_throw_err(ctx, WGL_INVALID_OPERATION);
}

static JSValue wgl_texSubImage2D(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 target = 0;
	s32 level = 0;
	s32 xoffset = 0;
	s32 yoffset = 0;
	u32 width = 0;
	u32 height = 0;
	u32 format = 0;
	u32 type = 0;
	u8 *pix_buf;
	u32 pix_buf_size=0;
	WGL_CHECK_CONTEXT
	if (argc<7) return js_throw_err(ctx, WGL_INVALID_VALUE);

	WGL_GET_U32(target, argv[0]);
	WGL_GET_S32(level, argv[1]);
	WGL_GET_S32(xoffset, argv[2]);
	WGL_GET_S32(yoffset, argv[3]);
	/*from texture object*/
	if (JS_IsObject(argv[6])) {

		WGL_GET_U32(format, argv[4]);
		WGL_GET_U32(type, argv[5]);
	}
	/*from array buffer*/
	else {
		if ((argc<9) || !JS_IsObject(argv[8])) {
			return js_throw_err(ctx, WGL_INVALID_VALUE);
		}

		WGL_GET_U32(width, argv[4]);
		WGL_GET_U32(height, argv[5]);
		WGL_GET_U32(format, argv[6]);
		WGL_GET_U32(type, argv[7]);
		pix_buf = wgl_GetArrayBuffer(ctx, &pix_buf_size, argv[8]);
		if (!pix_buf) return js_throw_err(ctx, WGL_INVALID_VALUE);

		glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pix_buf);
		return JS_UNDEFINED;
	}
	return js_throw_err(ctx, WGL_INVALID_OPERATION);
}

static JSValue wgl_useProgram(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JSValue ret_val_js = JS_UNDEFINED;
	GLuint program = 0;
	GF_WebGLContext *glc = JS_GetOpaque(this_val, WebGLRenderingContextBase_class_id);
	if (!glc) return js_throw_err(ctx, WGL_INVALID_OPERATION);
	if (argc<1) return js_throw_err(ctx, WGL_INVALID_VALUE);
	WGL_GET_GLID(program, argv[0], WebGLProgram_class_id);
	glUseProgram(program);
	glc->active_program = program;
	return ret_val_js;
}
static JSValue wgl_activeTexture(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JSValue ret_val_js = JS_UNDEFINED;
	u32 texture = 0;
	GF_WebGLContext *glc = JS_GetOpaque(this_val, WebGLRenderingContextBase_class_id);
	if (!glc) return js_throw_err(ctx, WGL_INVALID_OPERATION);
	if (argc<1) return js_throw_err(ctx, WGL_INVALID_VALUE);
	WGL_GET_U32(texture, argv[0]);
	glActiveTexture(texture);
	glc->active_texture = texture;
	return ret_val_js;
}

static JSValue wgl_createTexture(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JSValue ret_val_js = JS_UNDEFINED;
	GF_WebGLContext *glc = JS_GetOpaque(this_val, WebGLRenderingContextBase_class_id);
	if (!glc) return js_throw_err(ctx, WGL_INVALID_VALUE);

	if (argc && JS_IsString(argv[0])) {
		GF_WebGLNamedTexture *named_tx;
		const char *tx_name;
		tx_name = JS_ToCString(ctx, argv[0]);
		if (!tx_name) return js_throw_err(ctx, WGL_INVALID_VALUE);

		GF_SAFEALLOC(named_tx, GF_WebGLNamedTexture);
		named_tx->par_ctx = glc;
		named_tx->tx_name = gf_strdup(tx_name);
		JS_FreeCString(ctx, tx_name);
		ret_val_js = JS_NewObjectClass(ctx, NamedTexture_class_id);
		JS_SetOpaque(ret_val_js, named_tx);
		gf_list_add(glc->named_textures, named_tx);
	} else {
		GF_WebGLObject *wglo;
		GF_SAFEALLOC(wglo, GF_WebGLObject);
		if (!wglo) return js_throw_err(ctx, WGL_OUT_OF_MEMORY);
		wglo->par_ctx = JS_GetOpaque(this_val, WebGLRenderingContextBase_class_id);
		glGenTextures(1, &wglo->gl_id);
		ret_val_js = JS_NewObjectClass(ctx, WebGLTexture_class_id);
		JS_SetOpaque(ret_val_js, wglo);
		wglo->obj = JS_DupValue(ctx, ret_val_js);
		wglo->class_id = WebGLTexture_class_id;
		gf_list_add(wglo->par_ctx->all_objects, wglo);
	}
	return ret_val_js;
}

JSValue wgl_bind_named_texture(JSContext *c, GF_WebGLContext *glc, GF_WebGLNamedTexture *named_tx);

static JSValue wgl_bindTexture(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_WebGLObject *tx = NULL;
	GF_WebGLNamedTexture *named_tx = NULL;
	JSValue ret_val_js = JS_UNDEFINED;
	u32 target = 0;
	GF_WebGLContext *glc = JS_GetOpaque(this_val, WebGLRenderingContextBase_class_id);
	if (!glc|| (argc<2)) return js_throw_err(ctx, WGL_INVALID_VALUE);
	glc->bound_named_texture = NULL;

	WGL_GET_U32(target, argv[0]);
	tx = JS_GetOpaque(argv[1], WebGLTexture_class_id);
	if (!tx)
		named_tx = JS_GetOpaque(argv[1], NamedTexture_class_id);
	if (!tx && !named_tx) {
		return js_throw_err(ctx, WGL_INVALID_VALUE);
	}
	if (tx) {
		glBindTexture(target, tx->gl_id);
		return ret_val_js;
	}
	glc->bound_named_texture = named_tx;
	return wgl_bind_named_texture(ctx, glc, named_tx);
}

static JSValue wgl_getUniformLocation(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JSValue ret_val_js = JS_UNDEFINED;
	GLuint program = 0;
	GLint uni_loc=0;
	const char * name = 0;
	GF_WebGLContext *glc = JS_GetOpaque(this_val, WebGLRenderingContextBase_class_id);
	if (!glc) return js_throw_err(ctx, WGL_INVALID_OPERATION);
	if (argc<2) return js_throw_err(ctx, WGL_INVALID_VALUE);
	WGL_GET_GLID(program, argv[0], WebGLProgram_class_id);
	WGL_GET_STRING(name, argv[1]);
	GF_WebGLObject *wglo;

	uni_loc = glGetUniformLocation(program, name);
	if (uni_loc<0) {
		u32 i, count = gf_list_count(glc->named_textures);
		uni_loc = -1;
		for (i=0; i<count; i++) {
			GF_WebGLNamedTexture *named_tx = gf_list_get(glc->named_textures, i);
			if (!strcmp(named_tx->tx_name, name)) {
				uni_loc = -2;
				break;
			}
		}
		if (uni_loc==-1)
			return JS_NULL;
	}
	GF_SAFEALLOC(wglo, GF_WebGLObject);
	if (!wglo) return js_throw_err(ctx, WGL_OUT_OF_MEMORY);
	wglo->par_ctx = JS_GetOpaque(this_val, WebGLRenderingContextBase_class_id);
	wglo->gl_id = uni_loc;

	ret_val_js = JS_NewObjectClass(ctx, WebGLUniformLocation_class_id);
	JS_SetOpaque(ret_val_js, wglo);
	wglo->obj = JS_DupValue(ctx, ret_val_js);
	wglo->class_id = WebGLUniformLocation_class_id;
	gf_list_add(wglo->par_ctx->all_objects, wglo);
	JS_FreeCString(ctx, name);
	return ret_val_js;
}
void webgl_pck_tex_depth_del(GF_Filter *filter, GF_FilterPid *PID, GF_FilterPacket *pck, Bool is_depth)
{
	JSValue *fun = NULL;
	GF_FilterFrameInterface *f_ifce = gf_filter_pck_get_frame_interface(pck);
	if (!f_ifce) return;
	GF_WebGLContext *glc = (GF_WebGLContext *)f_ifce->user_data;
	if (!glc) return;

	if (is_depth)
		fun = &glc->depth_frame_flush;
	else
		fun = &glc->tex_frame_flush;

	if (!JS_IsUndefined(*fun)) {
		JSValue ret = JS_Call(glc->ctx, *fun, JS_UNDEFINED, 0, NULL);
		JS_FreeValue(glc->ctx, ret);

		JS_FreeValue(glc->ctx, *fun);
		*fun = JS_UNDEFINED;
	}
}
void webgl_pck_tex_del(GF_Filter *filter, GF_FilterPid *PID, GF_FilterPacket *pck)
{
	webgl_pck_tex_depth_del(filter, PID, pck, GF_FALSE);
}

void webgl_pck_depth_del(GF_Filter *filter, GF_FilterPid *PID, GF_FilterPacket *pck)
{
	webgl_pck_tex_depth_del(filter, PID, pck, GF_TRUE);
}

JSValue webgl_get_frame_interface(JSContext *ctx, int argc, JSValueConst *argv, gf_fsess_packet_destructor *pck_del, GF_FilterFrameInterface **f_ifce)
{
	JSValue *frame_flush_fun = NULL;
	GF_WebGLContext *glc;
	if (argc<2) return js_throw_err(ctx, WGL_INVALID_VALUE);
	if (!JS_IsFunction(ctx, argv[1])) return js_throw_err(ctx, WGL_INVALID_VALUE);

	glc = JS_GetOpaque(argv[0], WebGLRenderingContextBase_class_id);
	if (!glc) return js_throw_err(ctx, WGL_INVALID_OPERATION);

	if ((argc>=2) && JS_ToBool(ctx, argv[2]) ) {
		if (glc->actual_attrs.depth != WGL_DEPTH_TEXTURE)
			return js_throw_err(ctx, WGL_INVALID_OPERATION);

		*pck_del = webgl_pck_depth_del;
		frame_flush_fun = &glc->depth_frame_flush;
		*f_ifce = &glc->depth_f_ifce;
	} else {
		*pck_del = webgl_pck_tex_del;
		frame_flush_fun = &glc->tex_frame_flush;
		*f_ifce = &glc->tex_f_ifce;
	}
	if (!JS_IsUndefined(*frame_flush_fun))
		return js_throw_err(ctx, WGL_INVALID_OPERATION);

	JS_FreeValue(ctx, *frame_flush_fun);
	glc->tex_frame_flush = JS_UNDEFINED;
	if (JS_IsFunction(ctx, argv[1])) {
		*frame_flush_fun = JS_DupValue(ctx, argv[1]);
	}
	return JS_TRUE;
}

static GF_Err webgl_get_texture(GF_FilterFrameInterface *ifce, u32 plane_idx, u32 *gl_tex_format, u32 *gl_tex_id, GF_Matrix *texcoordmatrix)
{
	GF_WebGLContext *glc = ifce->user_data;
	if (!glc) return GF_BAD_PARAM;
	if (plane_idx) return GF_BAD_PARAM;
	*gl_tex_id = glc->tex_id;
	*gl_tex_format = GL_TEXTURE_2D;
	return GF_OK;
}
static GF_Err webgl_get_depth(GF_FilterFrameInterface *ifce, u32 plane_idx, u32 *gl_tex_format, u32 *gl_tex_id, GF_Matrix *texcoordmatrix)
{
	GF_WebGLContext *glc = ifce->user_data;
	if (!glc) return GF_BAD_PARAM;
	if (plane_idx) return GF_BAD_PARAM;
	*gl_tex_id = glc->depth_id;
	*gl_tex_format = GL_TEXTURE_2D;
	return GF_OK;
}
static JSValue webgl_setup_fbo(JSContext *ctx, GF_WebGLContext *glc, u32 width, u32 height)
{
	glc->width = width;
	glc->height = height;
	/*create fbo*/
	glGenTextures(1, &glc->tex_id);
	if (!glc->tex_id) {
		gf_free(glc);
		return js_throw_err_msg(ctx, GF_IO_ERR, "Failed to create OpenGL texture %d", glGetError() );
	}
	glBindTexture(GL_TEXTURE_2D, glc->tex_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, glc->actual_attrs.alpha ? GL_RGBA : GL_RGB, width, height, 0, glc->actual_attrs.alpha ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, NULL);

	glGenFramebuffers(1, &glc->fbo_id);
	if (!glc->fbo_id) {
		glDeleteTextures(1, &glc->tex_id);
		gf_free(glc);
		return js_throw_err_msg(ctx, GF_IO_ERR, "Failed to create OpenGL Framebuffer %d", glGetError() );
	}
	glBindFramebuffer(GL_FRAMEBUFFER, glc->fbo_id);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, glc->tex_id, 0);

	if (glc->actual_attrs.depth != WGL_DEPTH_NO) {
		if (glc->actual_attrs.depth==WGL_DEPTH_TEXTURE) {
			glGenTextures(1, &glc->depth_id);
			if (!glc->depth_id) {
				glDeleteTextures(1, &glc->tex_id);
				glDeleteFramebuffers(1, &glc->fbo_id);
				gf_free(glc);
				return js_throw_err_msg(ctx, GF_IO_ERR, "Failed to create OpenGL depth texture %d", glGetError() );
			}
			glBindTexture(GL_TEXTURE_2D, glc->depth_id);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexImage2D(GL_TEXTURE_2D, 0,
#if defined(GPAC_CONFIG_IOS) ||  defined(GPAC_CONFIG_ANDROID)
					GL_DEPTH_COMPONENT16,
#else
					GL_DEPTH_COMPONENT24,
#endif
					width, height, 0, GL_DEPTH_COMPONENT, GL_INT, NULL);

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, glc->depth_id, 0);

		} else {
			glGenRenderbuffers(1, &glc->depth_id);
			if (!glc->depth_id) {
				glDeleteTextures(1, &glc->tex_id);
				glDeleteFramebuffers(1, &glc->fbo_id);
				gf_free(glc);
				return js_throw_err_msg(ctx, GF_IO_ERR, "Failed to create OpenGL depth renderbuffer %d", glGetError() );
			}

			glBindRenderbuffer(GL_RENDERBUFFER, glc->depth_id);
#if defined(GPAC_CONFIG_IOS) ||  defined(GPAC_CONFIG_ANDROID)
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
#else
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
#endif
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, glc->depth_id);
		}
	}
   	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
   	switch (status) {
   	case GL_FRAMEBUFFER_COMPLETE:
   		break;
	default:
		glDeleteTextures(1, &glc->tex_id);
		glDeleteRenderbuffers(1, &glc->depth_id);
		glDeleteFramebuffers(1, &glc->fbo_id);
		gf_free(glc);
		return js_throw_err_msg(ctx, GF_IO_ERR, "Failed to initialize OpenGL FBO:  %08x", status );
   }

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return JS_UNDEFINED;
}
static JSValue webgl_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	JSValue res;
	u32 idx=0;
	u32 width = 0;
	u32 height = 0;
	GF_Err e;
	GF_WebGLContext *glc;
	JSValue v;
	Bool fake_canvas = GF_FALSE;

	if (argc && JS_IsObject(argv[0])) {
		v = JS_GetPropertyStr(ctx, argv[0], "width");
		JS_ToInt32(ctx, &width, v);
		JS_FreeValue(ctx, v);

		v = JS_GetPropertyStr(ctx, argv[0], "height");
		JS_ToInt32(ctx, &height, v);
		JS_FreeValue(ctx, v);
		idx=1;
		fake_canvas = GF_TRUE;
	} else if (argc>=2) {
		WGL_GET_S32(width, argv[0]);
		WGL_GET_S32(height, argv[1]);
		idx=2;
	}
	if (!width || !height) {
		return js_throw_err_msg(ctx, GF_BAD_PARAM, "Invalid width/height %dx%d while creating OpenGL context", width, height);
	}

	e = jsf_request_opengl(ctx);
	if (e) return js_throw_err_msg(ctx, e, "Failed to load OpenGL");
	GF_SAFEALLOC(glc, GF_WebGLContext);
	if (!glc) return js_throw_err_msg(ctx, GF_OUT_OF_MEM, "Failed to allocate OpenGL context");
	glc->creation_attrs.alpha = GF_TRUE;
	glc->creation_attrs.depth = WGL_DEPTH_YES;
	glc->creation_attrs.antialias = GF_TRUE;
	glc->creation_attrs.premultipliedAlpha = GF_TRUE;
	if ((argc>idx) && JS_IsObject(argv[idx])) {
#define GET_BOOL(_opt)\
		v = JS_GetPropertyStr(ctx, argv[idx], #_opt);\
		if (!JS_IsUndefined(v)) glc->creation_attrs._opt = JS_ToBool(ctx, v);\
		JS_FreeValue(ctx, v);\

		GET_BOOL(alpha)
		GET_BOOL(stencil)
		GET_BOOL(antialias)
		GET_BOOL(premultipliedAlpha)
		GET_BOOL(preserveDrawingBuffer)
		GET_BOOL(failIfMajorPerformanceCaveat)
		GET_BOOL(desynchronized)
#undef GET_BOOL

		v = JS_GetPropertyStr(ctx, argv[idx], "depth");
		if (!JS_IsUndefined(v)) {
			if (JS_IsString(v)) {
				const char *depth_mode = JS_ToCString(ctx, v);
				if (!strcmp(depth_mode, "false") || !strcmp(depth_mode, "no"))
					glc->creation_attrs.depth = WGL_DEPTH_NO;
				else if (!strcmp(depth_mode, "true") || !strcmp(depth_mode, "yes"))
					glc->creation_attrs.depth = WGL_DEPTH_YES;
				else if (!strcmp(depth_mode, "texture"))
					glc->creation_attrs.depth = WGL_DEPTH_TEXTURE;
				JS_FreeCString(ctx, depth_mode);
			} else {
				glc->creation_attrs.depth = JS_ToBool(ctx, v) ? WGL_DEPTH_YES : WGL_DEPTH_NO;
			}
		}
		JS_FreeValue(ctx, v);

		v = JS_GetPropertyStr(ctx, argv[idx], "powerPreference");
		if (!JS_IsUndefined(v)) {
			const char *str = JS_ToCString(ctx, v);
			if (str && !strcmp(str, "low-power")) glc->creation_attrs.powerPreference = WGL_POWER_LOWPOWER;
			else if (str && !strcmp(str, "high-performance")) glc->creation_attrs.powerPreference = WGL_POWER_HIGHPERF;
		}
		JS_FreeValue(ctx, v);
	}
	glc->actual_attrs = glc->creation_attrs;

	res = webgl_setup_fbo(ctx, glc, width, height);
	if (!JS_IsUndefined(res)) return res;

	//not supported
	glc->actual_attrs.preserveDrawingBuffer = GF_FALSE;

	v = JS_NewObjectClass(ctx, WebGLRenderingContextBase_class_id);
	JS_SetOpaque(v, glc);

	glc->tex_f_ifce.blocking = GF_TRUE;
	glc->tex_f_ifce.get_gl_texture = webgl_get_texture;
	glc->tex_f_ifce.user_data = glc;
	glc->tex_frame_flush = JS_UNDEFINED;

	glc->depth_f_ifce.blocking = GF_TRUE;
	glc->depth_f_ifce.get_gl_texture = webgl_get_depth;
	glc->depth_f_ifce.user_data = glc;
	glc->depth_frame_flush = JS_UNDEFINED;

	if (fake_canvas) {
		glc->canvas = JS_DupValue(ctx, argv[0]);
	} else {
		glc->canvas = JS_NewObject(ctx);
		JS_SetPropertyStr(ctx, glc->canvas, "width", JS_NewInt32(ctx, width));
		JS_SetPropertyStr(ctx, glc->canvas, "height", JS_NewInt32(ctx, height));
	}
	JS_SetPropertyStr(ctx, glc->canvas, "clientWidth", JS_NewInt32(ctx, width));
	JS_SetPropertyStr(ctx, glc->canvas, "clientHeight", JS_NewInt32(ctx, height));
	glc->ctx = ctx;
	glc->all_objects = gf_list_new();
	glc->named_textures = gf_list_new();

	return v;
}

static JSValue wgl_activate_gl(JSContext *ctx, GF_WebGLContext *glc, Bool activate)
{
	if (activate) {
		u32 i, count;
		GL_CHECK_ERR()
		jsf_set_gl_active(ctx);
		glBindFramebuffer(GL_FRAMEBUFFER, glc->fbo_id);

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		switch (status) {
		case GL_FRAMEBUFFER_COMPLETE:
			break;
		default:
			return js_throw_err_msg(ctx, GF_IO_ERR, "Failed to bind OpenGL FBO:  %08x", status );
		}
		count = gf_list_count(glc->named_textures);
		for (i=0; i<count; i++) {
			GF_WebGLNamedTexture *named_tx = gf_list_get(glc->named_textures, i);
			named_tx->frame_ifce = NULL;
		}
		glc->active_texture = 0;
		glc->active_program = 0;
		glc->bound_named_texture = NULL;
	} else {
#if!defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X) && !defined(GPAC_USE_GLES2)
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
#endif
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glDisable(GL_TEXTURE_2D);
		GL_CHECK_ERR()
	}
	return JS_UNDEFINED;
}
static JSValue wgl_activate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Bool activate;
	GF_WebGLContext *glc = JS_GetOpaque(this_val, WebGLRenderingContextBase_class_id);
	if (!glc|| (argc<1)) return js_throw_err(ctx, WGL_INVALID_VALUE);
	WGL_GET_BOOL(activate, argv[0]);
	return wgl_activate_gl(ctx, glc, activate);
}

static JSValue wgl_resize(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JSValue res;
	u32 width, height;
	GF_WebGLContext *glc = JS_GetOpaque(this_val, WebGLRenderingContextBase_class_id);
	if (!glc|| (argc<2)) return js_throw_err(ctx, WGL_INVALID_VALUE);
	WGL_GET_U32(width, argv[0]);
	WGL_GET_U32(height, argv[1]);
	if (!width || !height) return js_throw_err(ctx, WGL_INVALID_VALUE);

	res = wgl_activate_gl(ctx, glc, GF_FALSE);
	if (!JS_IsUndefined(res)) return res;

	glDeleteTextures(1, &glc->tex_id);
	glc->tex_id = 0;
	glDeleteRenderbuffers(1, &glc->depth_id);
	glc->depth_id = 0;
	glDeleteFramebuffers(1, &glc->fbo_id);
	glc->fbo_id = 0;

	res = webgl_setup_fbo(ctx, glc, width, height);
	if (!JS_IsUndefined(res)) return res;

	return wgl_activate_gl(ctx, glc, GF_TRUE);
}



static JSValue wgl_named_tx_upload(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_WebGLNamedTexture *named_tx = JS_GetOpaque(this_val, NamedTexture_class_id);
	if (!named_tx|| (argc<1)) return js_throw_err(ctx, WGL_INVALID_VALUE);
	return wgl_named_texture_upload(ctx, argv[0], named_tx);
}
static JSValue wgl_named_tx_reconfigure(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_WebGLNamedTexture *named_tx = JS_GetOpaque(this_val, NamedTexture_class_id);
	if (!named_tx) return js_throw_err(ctx, WGL_INVALID_VALUE);

	if (named_tx->nb_textures) {
		glDeleteTextures(named_tx->nb_textures, named_tx->textures);
		named_tx->nb_textures = 0;
	}
	named_tx->width = 0;
	named_tx->height = 0;
	named_tx->pix_fmt = 0;
	named_tx->stride = 0;
	named_tx->uv_stride = 0;
	named_tx->internal_textures = GF_FALSE;
	named_tx->shader_attached = GF_FALSE;
	named_tx->uniform_setup = GF_FALSE;
	return JS_UNDEFINED;
}


/*GPAC extensions*/
static const JSCFunctionListEntry webgl_funcs[] =
{
	JS_CFUNC_DEF("activate", 0, wgl_activate),
	JS_CFUNC_DEF("resize", 0, wgl_resize),
};

enum
{
	WGL_GPTX_NB_TEXTURES = 0,
	WGL_GPTX_GLTX_IN,
	WGL_GPTX_NAME,
	WGL_GPTX_PBO,
};

static JSValue wgl_named_tx_getProperty(JSContext *ctx, JSValueConst obj, int magic)
{
	GF_WebGLNamedTexture *named_tx = JS_GetOpaque(obj, NamedTexture_class_id);
	if (!named_tx) return js_throw_err(ctx, WGL_INVALID_OPERATION);

	switch (magic) {
	case WGL_GPTX_NB_TEXTURES:
		return JS_NewInt32(ctx, named_tx->nb_textures);
	case WGL_GPTX_GLTX_IN:
		return named_tx->internal_textures ? JS_FALSE : JS_TRUE;
	case WGL_GPTX_NAME:
		return JS_NewString(ctx, named_tx->tx_name);
	case WGL_GPTX_PBO:
		return named_tx->use_pbo ? JS_TRUE : JS_FALSE;
	}
	return JS_UNDEFINED;
}

static JSValue wgl_named_tx_setProperty(JSContext *ctx, JSValueConst obj, JSValueConst value, int magic)
{
	GF_WebGLNamedTexture *named_tx = JS_GetOpaque(obj, NamedTexture_class_id);
	if (!named_tx) return js_throw_err(ctx, WGL_INVALID_OPERATION);

	switch (magic) {
	case WGL_GPTX_PBO:
		named_tx->use_pbo = JS_ToBool(ctx, value) ? GF_TRUE : GF_FALSE;
		break;
	}
	return JS_UNDEFINED;
}
/*GPAC extensions*/
static const JSCFunctionListEntry webgl_named_tx_funcs[] =
{
	JS_CGETSET_MAGIC_DEF("nb_textures", wgl_named_tx_getProperty, NULL, WGL_GPTX_NB_TEXTURES),
	JS_CGETSET_MAGIC_DEF("is_gl_input", wgl_named_tx_getProperty, wgl_named_tx_setProperty, WGL_GPTX_GLTX_IN),
	JS_CGETSET_MAGIC_DEF("name", wgl_named_tx_getProperty, wgl_named_tx_setProperty, WGL_GPTX_NAME),
	JS_CGETSET_MAGIC_DEF("pbo", wgl_named_tx_getProperty, NULL, WGL_GPTX_PBO),
	JS_CFUNC_DEF("reconfigure", 0, wgl_named_tx_reconfigure),
	JS_CFUNC_DEF("upload", 0, wgl_named_tx_upload),
};



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

JSClassID matrix_class_id;

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
		return res;	}
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
	WGL_GET_FLOAT(_x, v);\
	JS_FreeValue(ctx, v);\
	v = JS_GetPropertyStr(ctx, _arg, "y");\
	WGL_GET_FLOAT(_y, v);\
	JS_FreeValue(ctx, v);\
	v = JS_GetPropertyStr(ctx, _arg, "z");\
	WGL_GET_FLOAT(_z, v);\
	JS_FreeValue(ctx, v);\
}

#define WGL_GET_VEC3F(_v, _arg)\
{\
	JSValue v;\
	Double _f;\
	v = JS_GetPropertyStr(ctx, _arg, "x");\
	WGL_GET_FLOAT(_f, v);\
	_v.x = FLT2FIX(_f);\
	JS_FreeValue(ctx, v);\
	v = JS_GetPropertyStr(ctx, _arg, "y");\
	WGL_GET_FLOAT(_f, v);\
	_v.y = FLT2FIX(_f);\
	JS_FreeValue(ctx, v);\
	v = JS_GetPropertyStr(ctx, _arg, "z");\
	WGL_GET_FLOAT(_f, v);\
	_v.z = FLT2FIX(_f);\
	JS_FreeValue(ctx, v);\
}

#define WGL_GET_VEC4(_x, _y, _z, _q, _arg)\
{\
	JSValue v;\
	v = JS_GetPropertyStr(ctx, _arg, "x");\
	WGL_GET_FLOAT(_x, v);\
	JS_FreeValue(ctx, v);\
	v = JS_GetPropertyStr(ctx, _arg, "y");\
	WGL_GET_FLOAT(_y, v);\
	JS_FreeValue(ctx, v);\
	v = JS_GetPropertyStr(ctx, _arg, "z");\
	WGL_GET_FLOAT(_z, v);\
	JS_FreeValue(ctx, v);\
	v = JS_GetPropertyStr(ctx, _arg, "w");\
	if (JS_IsUndefined(v)) v = JS_GetPropertyStr(ctx, _arg, "q");\
	if (JS_IsUndefined(v)) v = JS_GetPropertyStr(ctx, _arg, "angle");\
	WGL_GET_FLOAT(_q, v);\
	JS_FreeValue(ctx, v);\
}


static JSValue mx_translate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Double vx, vy, vz;
	GF_Matrix *mx = JS_GetOpaque(this_val, matrix_class_id);
	if (!mx || !argc) return JS_EXCEPTION;
	if (!JS_IsObject(argv[0])) {
		if (argc<3) return JS_EXCEPTION;
		WGL_GET_FLOAT(vx, argv[0])
		WGL_GET_FLOAT(vy, argv[1])
		WGL_GET_FLOAT(vz, argv[2])
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
		WGL_GET_FLOAT(vx, argv[0])
		WGL_GET_FLOAT(vy, argv[1])
		WGL_GET_FLOAT(vz, argv[2])
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
		WGL_GET_FLOAT(vx, argv[0])
		WGL_GET_FLOAT(vy, argv[1])
		WGL_GET_FLOAT(vz, argv[2])
		WGL_GET_FLOAT(angle, argv[3])
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
		WGL_GET_FLOAT(width, v);
		JS_FreeValue(ctx, v);
		v = JS_GetPropertyStr(ctx, argv[0], "height");
		WGL_GET_FLOAT(height, v);
		JS_FreeValue(ctx, v);
		v = JS_GetPropertyStr(ctx, argv[0], "x");
		WGL_GET_FLOAT(x, v);
		JS_FreeValue(ctx, v);
		v = JS_GetPropertyStr(ctx, argv[0], "y");
		WGL_GET_FLOAT(y, v);
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
	WGL_GET_FLOAT(left, argv[0])
	WGL_GET_FLOAT(right, argv[1])
	WGL_GET_FLOAT(bottom, argv[2])
	WGL_GET_FLOAT(top, argv[3])
	WGL_GET_FLOAT(z_near, argv[4])
	WGL_GET_FLOAT(z_far, argv[5])
	gf_mx_ortho(mx, FLT2FIX(left), FLT2FIX(right), FLT2FIX(bottom), FLT2FIX(top), FLT2FIX(z_near), FLT2FIX(z_far));
	return JS_DupValue(ctx, this_val);
}
static JSValue mx_perspective(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Double fov, ar, z_near, z_far;
	GF_Matrix *mx = JS_GetOpaque(this_val, matrix_class_id);
	if (!mx || (argc<4)) return JS_EXCEPTION;
	WGL_GET_FLOAT(fov, argv[0])
	WGL_GET_FLOAT(ar, argv[1])
	WGL_GET_FLOAT(z_near, argv[2])
	WGL_GET_FLOAT(z_far, argv[3])
	gf_mx_perspective(mx, FLT2FIX(fov), FLT2FIX(ar), FLT2FIX(z_near), FLT2FIX(z_far));
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

static int js_webgl_load_module(JSContext *c, JSModuleDef *m)
{
	JSValue ctor;
	JSValue proto;
	JSRuntime *rt = JS_GetRuntime(c);

	if (!WebGLRenderingContextBase_class_id) {
#define INITCLASS(_name)\
		JS_NewClassID(& _name##_class_id);\
		JS_NewClass(rt, _name##_class_id, & _name##_class);\

		INITCLASS(WebGLRenderingContextBase)
		INITCLASS(WebGLProgram)
		INITCLASS(WebGLShader)
		INITCLASS(WebGLBuffer)
		INITCLASS(WebGLFramebuffer)
		INITCLASS(WebGLRenderbuffer)
		INITCLASS(WebGLTexture)
		INITCLASS(WebGLUniformLocation)
		INITCLASS(NamedTexture)

		INITCLASS(matrix)

#undef INITCLASS

	}
	proto = JS_NewObject(c);
    JS_SetPropertyFunctionList(c, proto, webgl_funcs, countof(webgl_funcs));
    JS_SetPropertyFunctionList(c, proto, WebGLRenderingContextBase_funcs, countof(WebGLRenderingContextBase_funcs));
    JS_SetClassProto(c, WebGLRenderingContextBase_class_id, proto);

	proto = JS_NewObject(c);
    JS_SetPropertyFunctionList(c, proto, webgl_named_tx_funcs, countof(webgl_named_tx_funcs));
    JS_SetClassProto(c, NamedTexture_class_id, proto);

	proto = JS_NewObject(c);
    JS_SetPropertyFunctionList(c, proto, mx_funcs, countof(mx_funcs));
    JS_SetClassProto(c, matrix_class_id, proto);

	/*export constructors*/
	ctor = JS_NewCFunction2(c, webgl_constructor, "WebGLContext", 1, JS_CFUNC_constructor, 0);
    JS_SetModuleExport(c, m, "WebGLContext", ctor);

	/*export constructors*/
	ctor = JS_NewCFunction2(c, mx_constructor, "Matrix", 1, JS_CFUNC_constructor, 0);
    JS_SetModuleExport(c, m, "Matrix", ctor);
	return 0;
}

void qjs_module_init_webgl(JSContext *ctx)
{
    JSModuleDef *m;
    m = JS_NewCModule(ctx, "webgl", js_webgl_load_module);
    if (!m)
        return;
	JS_AddModuleExport(ctx, m, "WebGLContext");
	JS_AddModuleExport(ctx, m, "Matrix");
}

#else
#include "../scenegraph/qjs_common.h"
void qjs_module_init_webgl(JSContext *ctx)
{
}
#endif // GPAC_DISABLE_3D


#endif //GPAC_HAS_QJS

