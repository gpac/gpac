/*
 *			GPAC - Multimedia Framework C SDK
 *
 *				Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2010
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Management sub-project
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

#ifndef GPAC_JSAPI
#define GPAC_JSAPI

#include <gpac/setup.h>

#ifdef GPAC_ANDROID
#ifndef XP_UNIX
#define XP_UNIX
#endif
#endif

#if defined(DEBUG) && defined(GPAC_CONFIG_DARWIN)
#undef DEBUG
#endif

#include <jsapi.h>

#ifndef JS_VERSION
#define JS_VERSION 170
#endif


typedef struct
{
	JSClass _class;
	JSObject *_proto;
} GF_JSClass;



/*new APIs*/
#if (JS_VERSION>=185)

#ifdef USE_FFDEV_18
#define USE_FFDEV_17
#endif

#ifdef USE_FFDEV_17
#define USE_FFDEV_16
#endif

#ifdef USE_FFDEV_16
#define USE_FFDEV_15
#endif

#ifdef USE_FFDEV_15
#define JSVAL_IS_OBJECT(__v) JSVAL_IS_OBJECT_OR_NULL_IMPL(JSVAL_TO_IMPL(__v))
#define USE_FFDEV_14
#endif

#ifdef USE_FFDEV_14
#define USE_FFDEV_12

#define JS_FinalizeStub	NULL
#ifdef USE_FFDEV_16
#define JS_NEW_OBJ_FOR_CONS(__c, __classp, __args)	JS_NewObjectForConstructor(__c, &(__classp)->_class, __args)
#else
#define JS_NEW_OBJ_FOR_CONS(__c, __classp, __args)	JS_NewObjectForConstructor(__c, __classp, __args)
#endif

#else
#define JS_NEW_OBJ_FOR_CONS(__c, __classp, __args)	JS_NewObjectForConstructor(__c, __args)
#endif

#ifdef USE_FFDEV_12
typedef unsigned uintN;
typedef uint32_t jsuint;
typedef int jsint;
typedef double jsdouble;
#endif

#define JS_NewDouble(c, v)	v
#define JS_PropertyStub_forSetter JS_StrictPropertyStub

#if defined(USE_FFDEV_17)

#define SMJS_DECL_FUNC_PROP_SET(func_name) JSBool func_name(JSContext *c, JSHandleObject __hobj, JSHandleId __hid, JSBool strict, JSMutableHandleValue __vp) 
#define SMJS_FUNC_PROP_SET(func_name) SMJS_DECL_FUNC_PROP_SET(func_name) { JSObject *obj = *(__hobj._); jsid id = *(__hid._); jsval *vp = __vp._;
#define SMJS_FUNC_PROP_SET_NOVP(func_name) SMJS_DECL_FUNC_PROP_SET(func_name) { JSObject *obj = *(__hobj._); jsid id = *(__hid._);

#define SMJS_DECL_FUNC_PROP_GET(func_name) JSBool func_name(JSContext *c, JSHandleObject __hobj, JSHandleId __hid, JSMutableHandleValue __vp)
#define SMJS_FUNC_PROP_GET(func_name) SMJS_DECL_FUNC_PROP_GET( func_name ) { JSObject *obj = *(__hobj._); jsid id = *(__hid._); jsval *vp = __vp._;
#define SMJS_CALL_PROP_STUB() JS_PropertyStub(c, __hobj, __hid, __vp)
#define DECL_FINALIZE(func_name) void func_name(JSFreeOp *fop, JSObject *obj) { void *c = NULL;

#define SMJS_FUNCTION_SPEC(__name, __fun, __argc) JS_FS(__name, __fun, __argc, 0)
#define SMJS_PROPERTY_SPEC(__name, __tinyid, __flags, __getter, __setter) \
    {__name, __tinyid, __flags, JSOP_WRAPPER(__getter), JSOP_WRAPPER(__setter)}


#else

#ifdef USE_FFDEV_15

#define SMJS_DECL_FUNC_PROP_SET(func_name) JSBool func_name(JSContext *c, JSHandleObject __hobj, JSHandleId __hid, JSBool strict, jsval *vp) 
#define SMJS_FUNC_PROP_SET(func_name) SMJS_DECL_FUNC_PROP_SET(func_name) { JSObject *obj = *(__hobj._); jsid id = *(__hid._);
#define SMJS_DECL_FUNC_PROP_GET(func_name) JSBool func_name(JSContext *c, JSHandleObject __hobj, JSHandleId __hid, jsval *vp)
#define SMJS_FUNC_PROP_GET(func_name) SMJS_DECL_FUNC_PROP_GET( func_name ) { JSObject *obj = *(__hobj._); jsid id = *(__hid._);
#define SMJS_CALL_PROP_STUB() JS_PropertyStub(c, __hobj, __hid, vp)
#define DECL_FINALIZE(func_name) void func_name(JSFreeOp *fop, JSObject *obj) { void *c = NULL;

#else

#define SMJS_DECL_FUNC_PROP_SET(func_name) JSBool func_name(JSContext *c, JSObject *obj, jsid id, JSBool strict, jsval *vp)
#define SMJS_FUNC_PROP_SET(func_name) SMJS_DECL_FUNC_PROP_SET( func_name) {
#define SMJS_DECL_FUNC_PROP_GET(func_name) JSBool func_name(JSContext *c, JSObject *obj, jsid id, jsval *vp) 
#define SMJS_FUNC_PROP_GET(func_name) SMJS_DECL_FUNC_PROP_GET(func_name) { 
#define SMJS_CALL_PROP_STUB() JS_PropertyStub(c, obj, id, vp)
#define DECL_FINALIZE(func_name) void func_name(JSContext *c, JSObject *obj) {

#endif

#define SMJS_FUNC_PROP_SET_NOVP	SMJS_FUNC_PROP_SET

#define SMJS_FUNCTION_SPEC(__name, __fun, __argc) {__name, __fun, __argc, 0}
#define SMJS_PROPERTY_SPEC(__name, __tinyid, __flags, __getter, __setter) \
    {__name, __tinyid, __flags, __getter, __setter}

#endif

#define SMJS_FUNCTION(__name) __name(JSContext *c, uintN argc, jsval *argsvp)
#define SMJS_FUNCTION_EXT(__name, __ext) __name(JSContext *c, uintN argc, jsval *argsvp, __ext)
#define SMJS_ARGS	jsval *argv = JS_ARGV(c, argsvp);
#define SMJS_OBJ	JSObject *obj = JS_THIS_OBJECT(c, argsvp);
#define SMJS_SET_RVAL(__rval) JS_SET_RVAL(c, argsvp, __rval)
#define SMJS_GET_RVAL & JS_RVAL(c, argsvp)
#define SMJS_CALL_ARGS	c, argc, argsvp
#define SMJS_DECL_RVAL jsval *rval = & JS_RVAL(c, argsvp);

#define SMJS_CHARS_FROM_STRING(__c, __jsstr)	(char *) JS_EncodeString(__c, __jsstr)
#define SMJS_CHARS(__c, __val)	SMJS_CHARS_FROM_STRING(__c, JSVAL_TO_STRING(__val))
#define SMJS_FREE(__c, __str)	if (__str) JS_free(__c, __str)


#define SMJS_OBJ_CONSTRUCTOR(__classp)	\
	JSObject *obj = JS_NEW_OBJ_FOR_CONS(c, __classp, argsvp);	\
	SMJS_SET_RVAL(OBJECT_TO_JSVAL(obj));\

#define JS_GetFunctionName(_v) (JS_GetFunctionId(_v)!=NULL) ? SMJS_CHARS_FROM_STRING(c, JS_GetFunctionId(_v)) : NULL

#define SMJS_ID_IS_STRING	JSID_IS_STRING
#define SMJS_ID_TO_STRING		JSID_TO_STRING
#define SMJS_ID_IS_INT	JSID_IS_INT
#define SMJS_ID_TO_INT		JSID_TO_INT

#ifndef JS_THREADSAFE
#define JS_THREADSAFE
#endif


#ifdef USE_FFDEV_12
#define JS_ClearContextThread(__ctx)
#define JS_SetContextThread(__ctx)
#define SMJS_GET_PRIVATE(__ctx, __obj)	JS_GetPrivate(__obj)
#define SMJS_SET_PRIVATE(__ctx, __obj, __val)	JS_SetPrivate(__obj, __val)
#define SMJS_GET_PARENT(__ctx, __obj)	JS_GetParent(__obj)


#ifdef USE_FFDEV_15
#define JS_GET_CLASS(__ctx, __obj) JS_GetClass(* __obj._)
#else
#define JS_GET_CLASS(__ctx, __obj) JS_GetClass(__obj)
#endif

#ifdef USE_FFDEV_16
#define SMJS_CONSTRUCT_OBJECT(__ctx, __class, __parent)	JS_New(__ctx, JS_GetConstructor(__ctx, (__class)->_proto), 0, NULL)
#else
#define SMJS_CONSTRUCT_OBJECT(__ctx, __class, __parent)	JS_ConstructObject(__ctx, &(__class)->_class, __parent)
#endif

#else

#define SMJS_GET_PRIVATE(__ctx, __obj)	JS_GetPrivate(__ctx, __obj)
#define SMJS_SET_PRIVATE(__ctx, __obj, __val)	JS_SetPrivate(__ctx, __obj, __val)
#define SMJS_GET_PARENT(__ctx, __obj)	JS_GetParent(__ctx, __obj)

#ifdef USE_FFDEV_11
#define JS_ClearContextThread(__ctx)
#define JS_SetContextThread(__ctx)
#define SMJS_CONSTRUCT_OBJECT(__ctx, __class, __parent)	JS_ConstructObject(__ctx, &(__class)->_class, __parent)
#else
#define SMJS_CONSTRUCT_OBJECT(__ctx, __class, __parent)	JS_ConstructObject(__ctx, &(__class)->_class, 0, __parent)
#endif

#endif


#else

/* Windows with extra-libs */

#define SMJS_DECL_FUNC_PROP_SET(func_name) JSBool func_name(JSContext *c, JSObject *obj, jsval id, jsval *vp)
#define SMJS_FUNC_PROP_SET(func_name) SMJS_DECL_FUNC_PROP_SET(func_name) {
#define SMJS_FUNC_PROP_SET_NOVP	SMJS_FUNC_PROP_SET
#define SMJS_DECL_FUNC_PROP_GET(func_name) JSBool func_name(JSContext *c, JSObject *obj, jsval id, jsval *vp) 
#define SMJS_FUNC_PROP_GET(func_name) SMJS_DECL_FUNC_PROP_GET( func_name) { 
#define DECL_FINALIZE(func_name) void func_name(JSContext *c, JSObject *obj) {

#define SMJS_CALL_PROP_STUB() JS_PropertyStub(c, obj, id, vp)
#define SMJS_PROP_SETTER jsval id
#define SMJS_PROP_GETTER jsval id
#define JS_PropertyStub_forSetter JS_PropertyStub
#define SMJS_FUNCTION_SPEC(__name, __fun, __argc) {__name, __fun, __argc, 0, 0}
#define SMJS_PROPERTY_SPEC(__name, __tinyid, __flags, __getter, __setter) {__name, __tinyid, __flags, __getter, __setter}
#define SMJS_FUNCTION(__name) __name(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
#define SMJS_FUNCTION_EXT(__name, __ext) __name(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval, __ext)
#define SMJS_ARGS
#define SMJS_OBJ	
#define SMJS_OBJ_CONSTRUCTOR(__classp)
#define SMJS_GET_RVAL rval
#define SMJS_SET_RVAL(__rval) *rval = __rval
#define SMJS_CALL_ARGS	c, obj, argc, argv, rval
#define SMJS_DECL_RVAL

#define SMJS_CHARS_FROM_STRING(__c, __str)	JS_GetStringBytes(__str)
#define SMJS_CHARS(__c, __val)	JS_GetStringBytes(JSVAL_TO_STRING(__val))
#define SMJS_FREE(__c, __str)

#define SMJS_ID_IS_STRING	JSVAL_IS_STRING
#define SMJS_ID_TO_STRING		JSVAL_TO_STRING
#define SMJS_ID_IS_INT	JSVAL_IS_INT
#define SMJS_ID_TO_INT		JSVAL_TO_INT

#define SMJS_CONSTRUCT_OBJECT(__ctx, __class, __parent)	JS_ConstructObject(__ctx, &(__class)->_class, 0, __parent)
#define SMJS_GET_PRIVATE(__ctx, __obj)	JS_GetPrivate(__ctx, __obj)
#define SMJS_SET_PRIVATE(__ctx, __obj, __val)	JS_SetPrivate(__ctx, __obj, __val)
#define SMJS_GET_PARENT(__ctx, __obj)	JS_GetParent(__ctx, __obj)

#endif


#ifdef __cplusplus
extern "C" {
#endif

#if (JS_VERSION>=185)
#if defined(USE_FFDEV_18)
JSBool gf_sg_js_has_instance(JSContext *cx, JSHandleObject obj, JSMutableHandleValue vp, JSBool *bp);
#elif defined(USE_FFDEV_15)
JSBool gf_sg_js_has_instance(JSContext *c, JSHandleObject obj,const jsval *val, JSBool *vp);
#else
JSBool gf_sg_js_has_instance(JSContext *c, JSObject *obj,const jsval *val, JSBool *vp);
#endif
#else
JSBool gf_sg_js_has_instance(JSContext *c, JSObject *obj, jsval val, JSBool *vp);
#endif

#define JS_SETUP_CLASS(the_class, cname, flag, getp, setp, fin)	\
	memset(&the_class, 0, sizeof(the_class));	\
	the_class._class.name = cname;	\
	the_class._class.flags = flag;	\
	the_class._class.addProperty = JS_PropertyStub;	\
	the_class._class.delProperty = JS_PropertyStub;	\
	the_class._class.getProperty = getp;	\
	the_class._class.setProperty = setp;	\
	the_class._class.enumerate = JS_EnumerateStub;	\
	the_class._class.resolve = JS_ResolveStub;		\
	the_class._class.convert = JS_ConvertStub;		\
	the_class._class.finalize = (JSFinalizeOp) fin;	\
	the_class._class.hasInstance = gf_sg_js_has_instance;


#define JS_MAKE_DOUBLE(__c, __double)	DOUBLE_TO_JSVAL(JS_NewDouble(__c, __double) ) 


#define GF_JS_InstanceOf(_a, _b, __class, _d) JS_InstanceOf(_a, _b, & (__class)->_class, NULL)

#define GF_JS_InitClass(cx, obj, parent_proto, clasp, constructor, nargs, ps, fs, static_ps, static_fs) \
		(clasp)->_proto = JS_InitClass(cx, obj, parent_proto, &(clasp)->_class, constructor, nargs, ps, fs, static_ps, static_fs);

JSObject *gf_sg_js_global_object(JSContext *cx, GF_JSClass *__class);

#ifdef __cplusplus
}
#endif

#endif //GPAC_JSAPI
