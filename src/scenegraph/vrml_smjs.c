/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Graph sub-project
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

#include <gpac/internal/scenegraph_dev.h>


#ifdef GPAC_HAS_SPIDERMONKEY

#include <gpac/internal/terminal_dev.h>
#include <gpac/modules/term_ext.h>
#include <gpac/modules/js_usr.h>

#include <gpac/internal/smjs_api.h>


/*fixes for JS > 1.8.0rc1 where GC routines have changed*/
GF_EXPORT
Bool gf_js_add_root(JSContext *cx, void *rp, u32 type)
{
#if (JS_VERSION>=185)
	JSBool ret;
	switch (type) {
	case GF_JSGC_STRING:
		ret = JS_AddStringRoot(cx, rp);
		break;
	case GF_JSGC_OBJECT:
		ret = JS_AddObjectRoot(cx, rp);
		break;
	case GF_JSGC_VAL:
		ret = JS_AddValueRoot(cx, rp);
		break;
	default:
		ret = JS_AddGCThingRoot(cx, rp);
		break;
	}
	return (ret == JS_TRUE) ? 1 : 0;
#else
	return (JS_AddRoot(cx, rp)==JS_TRUE) ? 1 : 0;
#endif
}

GF_EXPORT
Bool gf_js_add_named_root(JSContext *cx, void *rp, u32 type, const char *name)
{
#if (JS_VERSION>=185)
	JSBool ret;
	switch (type) {
	case GF_JSGC_STRING:
		ret = JS_AddNamedStringRoot(cx, rp, name);
		break;
	case GF_JSGC_OBJECT:
		ret = JS_AddNamedObjectRoot(cx, rp, name);
		break;
	case GF_JSGC_VAL:
		ret = JS_AddNamedValueRoot(cx, rp, name);
		break;
	default:
		ret = JS_AddNamedGCThingRoot(cx, rp, name);
		break;
	}
	return (ret == JS_TRUE) ? 1 : 0;
#else
	return (JS_AddNamedRoot(cx, rp, name)==JS_TRUE) ? 1 : 0;
#endif
}


JSObject *gf_sg_js_global_object(JSContext *cx, GF_JSClass *__class)
{
#if (JS_VERSION>=185)
	__class->_class.flags |= JSCLASS_GLOBAL_FLAGS;
#ifdef USE_FFDEV_16
	return JS_NewGlobalObject(cx, &__class->_class, NULL);
#else
	return JS_NewCompartmentAndGlobalObject(cx, &__class->_class, NULL);
#endif

#else
	return JS_NewObject(cx, &__class->_class, 0, 0 );
#endif
}

#if !defined(__GNUC__)
# if defined(_WIN32_WCE)
#  pragma comment(lib, "js32")
# elif defined (_WIN64)
#  pragma comment(lib, "js")
# elif defined (WIN32)
#  pragma comment(lib, "js")
# endif
#endif

/*define this macro to force Garbage Collection after each input to JS (script initialize/shutdown and all eventIn)
on latest SM, GC will crash if called from a different thread than the thread creating the contex, no clue why

for iOS don't force GC (better performances according to Ivica)

NOTE - this is currently disabled, as performing the GC at each root with complex scenes (lots of objects) really decreases performances
*/
#if !defined(GPAC_IPHONE)
# if (JS_VERSION<180)
//#  define FORCE_GC
# endif
#endif


typedef struct
{
	u8 is_setup;
	/*set to true for proto IS fields*/
	u8 IS_route;

	u32 ID;
	char *name;

	/*scope of this route*/
	GF_SceneGraph *graph;
	u32 lastActivateTime;

	GF_Node *FromNode;
	GF_FieldInfo FromField;

	GF_Node *ToNode;
	GF_FieldInfo ToField;

	JSObject *obj;
	jsval fun;
} GF_RouteToScript;


#define _ScriptMessage(_c, _msg) {	\
		GF_Node *_n = (GF_Node *) JS_GetContextPrivate(_c);	\
		if (_n->sgprivate->scenegraph->script_action) {\
			GF_JSAPIParam par;	\
			par.info.e = GF_SCRIPT_INFO;			\
			par.info.msg = (_msg);		\
			_n->sgprivate->scenegraph->script_action(_n->sgprivate->scenegraph->script_action_cbck, GF_JSAPI_OP_MESSAGE, NULL, &par);\
		}	\
		}

static Bool ScriptAction(JSContext *c, GF_SceneGraph *scene, u32 type, GF_Node *node, GF_JSAPIParam *param)
{
	if (!scene) {
		GF_Node *n = (GF_Node *) JS_GetContextPrivate(c);
		scene = n->sgprivate->scenegraph;
	}
	if (scene->script_action)
		return scene->script_action(scene->script_action_cbck, type, node, param);
	return 0;
}

typedef struct
{
	JSRuntime *js_runtime;
	u32 nb_inst;
	JSContext *ctx;

	GF_Mutex *mx;
	GF_JSClass SFNodeClass;

#ifndef GPAC_DISABLE_VRML
	GF_JSClass globalClass;
	GF_JSClass browserClass;
	GF_JSClass SFVec2fClass;
	GF_JSClass SFVec3fClass;
	GF_JSClass SFRotationClass;
	GF_JSClass SFColorClass;
	GF_JSClass SFImageClass;
	GF_JSClass MFInt32Class;
	GF_JSClass MFBoolClass;
	GF_JSClass MFFloatClass;
	GF_JSClass MFTimeClass;
	GF_JSClass MFVec2fClass;
	GF_JSClass MFVec3fClass;
	GF_JSClass MFRotationClass;
	GF_JSClass MFColorClass;
	GF_JSClass MFStringClass;
	GF_JSClass MFUrlClass;
	GF_JSClass MFNodeClass;
	GF_JSClass AnyClass;
#endif

	/*extensions are loaded for the lifetime of the runtime NOT of the context - this avoids nasty
	crashes with multiple contexts in SpiderMonkey (root'ing bug with InitStandardClasses)*/
	GF_List *extensions;

	GF_List *allocated_contexts;
} GF_JSRuntime;

static GF_JSRuntime *js_rt = NULL;

GF_EXPORT
Bool gf_js_remove_root(JSContext *cx, void *rp, u32 type)
{
#if (JS_VERSION>=185)
	switch (type) {
	case GF_JSGC_STRING:
#ifdef USE_FFDEV_15
		if (!cx) JS_RemoveStringRootRT(js_rt->js_runtime, rp);
		else
#endif
			JS_RemoveStringRoot(cx, rp);
		break;
	case GF_JSGC_OBJECT:
#ifdef USE_FFDEV_15
		if (!cx) JS_RemoveObjectRootRT(js_rt->js_runtime, rp);
		else
#endif
			JS_RemoveObjectRoot(cx, rp);
		break;
	case GF_JSGC_VAL:
#ifdef USE_FFDEV_15
		if (!cx) JS_RemoveValueRootRT(js_rt->js_runtime, rp);
		else
#endif
			JS_RemoveValueRoot(cx, (jsval *) rp);
		break;
	default:
		if (cx) JS_RemoveGCThingRoot(cx, rp);
		break;
	}
	return 1;
#else
	return (JS_RemoveRoot(cx, rp)==JS_TRUE) ? 1 : 0;
#endif
}

void gf_sg_load_script_extensions(GF_SceneGraph *sg, JSContext *c, JSObject *obj, Bool unload)
{
	u32 i, count;
	count = gf_list_count(js_rt->extensions);
	for (i=0; i<count; i++) {
		GF_JSUserExtension *ext = (GF_JSUserExtension *) gf_list_get(js_rt->extensions, i);
		ext->load(ext, sg, c, obj, unload);
	}

	//if (js_rt->nb_inst==1)
	{
		GF_Terminal *term;
		GF_JSAPIParam par;

		if (!sg->script_action) return;
		if (!sg->script_action(sg->script_action_cbck, GF_JSAPI_OP_GET_TERM, sg->RootNode, &par))
			return;

		term = par.term;

		count = gf_list_count(term->extensions);
		for (i=0; i<count; i++) {
			GF_TermExt *ext = (GF_TermExt *) gf_list_get(term->extensions, i);
			if (ext->caps & GF_TERM_EXTENSION_JS) {
				GF_TermExtJS te;
				te.ctx = c;
				te.global = obj;
				te.scenegraph = sg;
				te.unload = unload;
				ext->process(ext, GF_TERM_EXT_JSBIND, &te);
			}
		}
	}
}


static void gf_sg_load_script_modules(GF_SceneGraph *sg)
{
	GF_Terminal *term;
	u32 i, count;
	GF_JSAPIParam par;

	js_rt->extensions = gf_list_new();

	if (!sg->script_action) return;
	if (!sg->script_action(sg->script_action_cbck, GF_JSAPI_OP_GET_TERM, sg->RootNode, &par))
		return;

	term = par.term;

	count = gf_modules_get_count(term->user->modules);
	for (i=0; i<count; i++) {
		GF_JSUserExtension *ext = (GF_JSUserExtension *) gf_modules_load_interface(term->user->modules, i, GF_JS_USER_EXT_INTERFACE);
		if (!ext) continue;
		gf_list_add(js_rt->extensions, ext);
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_SCRIPT, ("[ECMAScript] found %d JS extensions for %d modules\n", gf_list_count(js_rt->extensions), count));
}

static void gf_sg_unload_script_modules()
{
	while (gf_list_count(js_rt->extensions)) {
		GF_JSUserExtension *ext = gf_list_last(js_rt->extensions);
		gf_list_rem_last(js_rt->extensions);
		gf_modules_close_interface((GF_BaseInterface *) ext);
	}
	gf_list_del(js_rt->extensions);
}


#ifdef NO_JS_RUNTIMETHREAD
void JS_SetRuntimeThread(JSRuntime *jsr)
{
}
void JS_ClearRuntimeThread(JSRuntime *jsr)
{
}
#endif


#ifdef __SYMBIAN32__
const long MAX_HEAP_BYTES = 256 * 1024L;
#else
const long MAX_HEAP_BYTES = 32*1024*1024L;
#endif
const long STACK_CHUNK_BYTES = 8*1024L;

JSContext *gf_sg_ecmascript_new(GF_SceneGraph *sg)
{
	JSContext *ctx;
	if (!js_rt) {
		JSRuntime *js_runtime = JS_NewRuntime(MAX_HEAP_BYTES);
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
		gf_sg_load_script_modules(sg);
	}
	js_rt->nb_inst++;

	gf_mx_p(js_rt->mx);
#if defined(JS_THREADSAFE) && (JS_VERSION>=185)
	if (gf_mx_get_num_locks(js_rt->mx)==1) {
		JS_SetRuntimeThread(js_rt->js_runtime);
	}
#endif

	ctx = JS_NewContext(js_rt->js_runtime, STACK_CHUNK_BYTES);
	JS_SetOptions(ctx, JS_GetOptions(ctx) | JSOPTION_WERROR);

#if defined(JS_THREADSAFE) && (JS_VERSION>=185)
	JS_ClearContextThread(ctx);
	if (gf_mx_get_num_locks(js_rt->mx)==1) {
		JS_ClearRuntimeThread(js_rt->js_runtime);
	}
#endif

	gf_list_add(js_rt->allocated_contexts, ctx);
	gf_mx_v(js_rt->mx);

	return ctx;
}

void gf_sg_ecmascript_del(JSContext *ctx)
{
#ifdef JS_THREADSAFE
#if (JS_VERSION>=185)
	assert(js_rt);
	JS_SetRuntimeThread(js_rt->js_runtime);
	JS_SetContextThread(ctx);
#endif
#endif

	gf_sg_js_call_gc(ctx);

	gf_list_del_item(js_rt->allocated_contexts, ctx);
	JS_DestroyContext(ctx);
	if (js_rt) {
		js_rt->nb_inst --;
		if (js_rt->nb_inst == 0) {
			JS_DestroyRuntime(js_rt->js_runtime);
			JS_ShutDown();
			gf_sg_unload_script_modules();
			gf_list_del(js_rt->allocated_contexts);
			gf_mx_del(js_rt->mx);
			gf_free(js_rt);
			js_rt = NULL;
		}
	}
}


GF_EXPORT
#if (JS_VERSION>=185)
#if defined(USE_FFDEV_18)
JSBool gf_sg_js_has_instance(JSContext *c, JSHandleObject obj, JSMutableHandleValue __val, JSBool *vp)
#elif defined(USE_FFDEV_15)
JSBool gf_sg_js_has_instance(JSContext *c, JSHandleObject obj,const jsval *val, JSBool *vp)
#else
JSBool gf_sg_js_has_instance(JSContext *c, JSObject *obj,const jsval *val, JSBool *vp)
#endif
#else
JSBool gf_sg_js_has_instance(JSContext *c, JSObject *obj, jsval val, JSBool *vp)
#endif
{
#ifdef USE_FFDEV_18
	jsval       *val = __val._;
#endif
	*vp = JS_FALSE;
#if (JS_VERSION>=185)
	if (val && JSVAL_IS_OBJECT(*val)) {
		JSObject *p = JSVAL_TO_OBJECT(*val);
#else
	if (JSVAL_IS_OBJECT(val)) {
		JSObject *p = JSVAL_TO_OBJECT(val);
#endif
		JSClass *js_class = JS_GET_CLASS(c, obj);
		if (JS_InstanceOf(c, p, js_class, NULL) ) *vp = JS_TRUE;
	}
	return JS_TRUE;
}

#ifndef GPAC_DISABLE_SVG
/*SVG tags for script handling*/
#include <gpac/nodes_svg.h>

GF_Node *dom_get_element(JSContext *c, JSObject *obj);
#endif


JSBool gf_sg_script_to_node_field(struct JSContext *c, jsval v, GF_FieldInfo *field, GF_Node *owner, GF_JSField *parent);
jsval gf_sg_script_to_smjs_field(GF_ScriptPriv *priv, GF_FieldInfo *field, GF_Node *parent, Bool force_evaluate);

static void JSScript_NodeModified(GF_SceneGraph *sg, GF_Node *node, GF_FieldInfo *info, GF_Node *script);

Bool JSScriptFromFile(GF_Node *node, const char *opt_file, Bool no_complain, jsval *rval);

void gf_sg_js_call_gc(JSContext *c)
{
	gf_sg_lock_javascript(c, 1);
#ifdef 	USE_FFDEV_14
	JS_GC(js_rt->js_runtime);
#else
	JS_GC(c);
#endif
	gf_sg_lock_javascript(c, 0);
}

void do_js_gc(JSContext *c, GF_Node *node)
{
#ifdef FORCE_GC
	node->sgprivate->scenegraph->trigger_gc = GF_TRUE;
#endif

	if (node->sgprivate->scenegraph->trigger_gc) {
		node->sgprivate->scenegraph->trigger_gc = GF_FALSE;
		gf_sg_js_call_gc(c);
	}
}


#ifndef GPAC_DISABLE_VRML

/*MPEG4 & X3D tags (for node tables & script handling)*/
#include <gpac/nodes_mpeg4.h>
#include <gpac/nodes_x3d.h>

void SFColor_fromHSV(SFColor *col)
{
	Fixed f, q, t, p, hue, sat, val;
	u32 i;
	hue = col->red;
	sat = col->green;
	val = col->blue;
	if (sat==0) {
		col->red = col->green = col->blue = val;
		return;
	}
	if (hue == FIX_ONE) hue = 0;
	else hue *= 6;
	i = FIX2INT( gf_floor(hue) );
	f = hue-i;
	p = gf_mulfix(val, FIX_ONE - sat);
	q = gf_mulfix(val, FIX_ONE - gf_mulfix(sat,f));
	t = gf_mulfix(val, FIX_ONE - gf_mulfix(sat, FIX_ONE - f));
	switch (i) {
	case 0:
		col->red = val;
		col->green = t;
		col->blue = p;
		break;
	case 1:
		col->red = q;
		col->green = val;
		col->blue = p;
		break;
	case 2:
		col->red = p;
		col->green = val;
		col->blue = t;
		break;
	case 3:
		col->red = p;
		col->green = q;
		col->blue = val;
		break;
	case 4:
		col->red = t;
		col->green = p;
		col->blue = val;
		break;
	case 5:
		col->red = val;
		col->green = p;
		col->blue = q;
		break;
	}
}

void SFColor_toHSV(SFColor *col)
{
	Fixed h, s;
	Fixed _max = MAX(col->red, MAX(col->green, col->blue));
	Fixed _min = MIN(col->red, MAX(col->green, col->blue));

	s = (_max == 0) ? 0 : gf_divfix(_max - _min, _max);
	if (s != 0) {
		Fixed rl = gf_divfix(_max - col->red, _max - _min);
		Fixed gl = gf_divfix(_max - col->green, _max - _min);
		Fixed bl = gf_divfix(_max - col->blue, _max - _min);
		if (_max == col->red) {
			if (_min == col->green) h = 60*(5+bl);
			else h = 60*(1-gl);
		} else if (_max == col->green) {
			if (_min == col->blue) h = 60*(1+rl);
			else h = 60*(3-bl);
		} else {
			if (_min == col->red) h = 60*(3+gl);
			else h = 60*(5-rl);
		}
	} else {
		h = 0;
	}
	col->red = h;
	col->green = s;
	col->blue = _max;
}

static GFINLINE GF_JSField *NewJSField(JSContext *c)
{
	GF_JSField *ptr;
	GF_SAFEALLOC(ptr, GF_JSField);
	if (!ptr) return NULL;
	ptr->js_ctx = c;
	return ptr;
}

static GFINLINE M_Script *JS_GetScript(JSContext *c)
{
	return (M_Script *) JS_GetContextPrivate(c);
}
static GFINLINE GF_ScriptPriv *JS_GetScriptStack(JSContext *c)
{
	M_Script *script = (M_Script *) JS_GetContextPrivate(c);
	return script->sgprivate->UserPrivate;
}

static void script_error(JSContext *c, const char *msg, JSErrorReport *jserr)
{
	if (jserr->linebuf) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[JavaScript] Error: %s - line %d (%s) - file %s\n", msg, jserr->lineno, jserr->linebuf, jserr->filename));
	} else if (jserr->filename) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[JavaScript] Error: %s - line %d - file %s\n", msg, jserr->lineno, jserr->filename));
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[JavaScript] Error: %s - line %d\n", msg, jserr->lineno));
	}
}

static JSBool SMJS_FUNCTION(JSPrint)
{
	SMJS_ARGS
	if (!argc) return JS_FALSE;

	if (JSVAL_IS_STRING(argv[0])) {
		char *str = SMJS_CHARS(c, argv[0]);
		_ScriptMessage(c, str);
		SMJS_FREE(c, str);
	}
	if (JSVAL_IS_INT(argv[0]) && (argc>1) && JSVAL_IS_STRING(argv[1]) ) {
		u32 level = JSVAL_TO_INT(argv[0]);
		char *str = SMJS_CHARS(c, argv[1]);
		if (level > GF_LOG_DEBUG) level = GF_LOG_DEBUG;
		if (str[0] == '[') {
			GF_LOG(level, GF_LOG_CONSOLE, ("%s\n", str));
		} else {
			GF_LOG(level, GF_LOG_CONSOLE, ("[JS] %s\n", str));
		}
		SMJS_FREE(c, str);
	}
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(getName)
{
	JSString *s = JS_NewStringCopyZ(c, "GPAC RichMediaEngine");
	if (!s) return JS_FALSE;
	SMJS_SET_RVAL( STRING_TO_JSVAL(s) );
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(getVersion)
{
	JSString *s = JS_NewStringCopyZ(c, GPAC_FULL_VERSION);
	if (!s) return JS_FALSE;
	SMJS_SET_RVAL( STRING_TO_JSVAL(s) );
	return JS_TRUE;
}
static JSBool SMJS_FUNCTION(getCurrentSpeed)
{
	GF_JSAPIParam par;
	GF_Node *node = JS_GetContextPrivate(c);
	par.time = 0;
	ScriptAction(c, NULL, GF_JSAPI_OP_GET_SPEED, node->sgprivate->scenegraph->RootNode, &par);
	SMJS_SET_RVAL(  JS_MAKE_DOUBLE( c, par.time) );
	return JS_TRUE;
}
static JSBool SMJS_FUNCTION(getCurrentFrameRate)
{
	GF_JSAPIParam par;
	GF_Node *node = JS_GetContextPrivate(c);
	par.time = 0;
	ScriptAction(c, NULL, GF_JSAPI_OP_GET_FPS, node->sgprivate->scenegraph->RootNode, &par);
	SMJS_SET_RVAL( JS_MAKE_DOUBLE( c, par.time) );
	return JS_TRUE;
}
static JSBool SMJS_FUNCTION(getWorldURL)
{
	GF_JSAPIParam par;
	GF_Node *node = JS_GetContextPrivate(c);
	par.uri.url = NULL;
	par.uri.nb_params = 0;
	if (ScriptAction(c, NULL, GF_JSAPI_OP_RESOLVE_URI, node->sgprivate->scenegraph->RootNode, &par)) {
		JSString *s = JS_NewStringCopyZ(c, par.uri.url);
		if (!s) return JS_FALSE;
		SMJS_SET_RVAL( STRING_TO_JSVAL(s) );
		gf_free(par.uri.url);
		return JS_TRUE;
	}
	return JS_FALSE;
}

static JSObject *node_get_binding(GF_ScriptPriv *priv, GF_Node *node, Bool is_constructor)
{
	JSObject *obj;
	GF_JSField *field;

	if (node->sgprivate->interact && node->sgprivate->interact->js_binding && node->sgprivate->interact->js_binding->node) {
		field = node->sgprivate->interact->js_binding->node;
		if (!field->is_rooted) {
			gf_js_add_root(priv->js_ctx, &field->obj, GF_JSGC_OBJECT);
			field->is_rooted=1;
		}
		return field->obj;
	}

	field = NewJSField(priv->js_ctx);
	field->field.fieldType = GF_SG_VRML_SFNODE;
	field->node = node;
	field->field.far_ptr = &field->node;


	node->sgprivate->flags |= GF_NODE_HAS_BINDING;
	gf_node_register(node, NULL);

	obj = JS_NewObject(priv->js_ctx, &js_rt->SFNodeClass._class, 0, 0);
	SMJS_SET_PRIVATE(priv->js_ctx, obj, field);

	field->obj = obj;
	gf_list_add(priv->js_cache, obj);

	/*remember the object*/
	if (!node->sgprivate->interact) {
		GF_SAFEALLOC(node->sgprivate->interact, struct _node_interactive_ext);
		if (!node->sgprivate->interact) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[VRMLJS] Failed to create interact storage\n"));
			return NULL;
		}
	}
	if (!node->sgprivate->interact->js_binding) {
		GF_SAFEALLOC(node->sgprivate->interact->js_binding, struct _node_js_binding);
		if (!node->sgprivate->interact->js_binding) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[VRMLJS] Failed to create JS bindings storage\n"));
			return NULL;
		}
		node->sgprivate->interact->js_binding->fields = gf_list_new();
	}
	node->sgprivate->flags |= GF_NODE_HAS_BINDING;
	node->sgprivate->interact->js_binding->node = field;
	if (!is_constructor) {
		field->is_rooted = 1;
		gf_js_add_root(priv->js_ctx, &field->obj, GF_JSGC_OBJECT);
	}
	return obj;
}

static JSBool SMJS_FUNCTION(getScript)
{
	GF_ScriptPriv *priv = JS_GetScriptStack(c);
	GF_Node *node = JS_GetContextPrivate(c);
	JSObject *an_obj = node_get_binding(priv, node, 0);

	if (an_obj) SMJS_SET_RVAL( OBJECT_TO_JSVAL(an_obj) );
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(loadScript)
{
	Bool no_complain = 0;
	char *url;
	GF_Node *node = JS_GetContextPrivate(c);
	SMJS_ARGS
	jsval aval;
	if (!argc || !JSVAL_IS_STRING(argv[0])) return JS_TRUE;

	if ((argc>1) && JSVAL_IS_BOOLEAN(argv[1])) no_complain = (JSVAL_TO_BOOLEAN(argv[1])==JS_TRUE) ? 1 : 0;

	url = SMJS_CHARS(c, argv[0]);
	if (url) {
		JSScriptFromFile(node, url, no_complain, &aval);
		SMJS_SET_RVAL(aval);
	}
	SMJS_FREE(c, url);
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(getProto)
{
	JSObject *an_obj;
	GF_ScriptPriv *priv = JS_GetScriptStack(c);
	GF_Node *node = JS_GetContextPrivate(c);
	SMJS_SET_RVAL( JSVAL_NULL);
	if (!node->sgprivate->scenegraph->pOwningProto) {
		return JS_TRUE;
	}
	node = (GF_Node *) node->sgprivate->scenegraph->pOwningProto;

	an_obj = node_get_binding(priv, node, 0);
	if (an_obj) SMJS_SET_RVAL( OBJECT_TO_JSVAL(an_obj) );
	return JS_TRUE;
}

#ifndef GPAC_DISABLE_SVG
char *js_get_utf8(jsval val);
jsval dom_element_construct(JSContext *c, GF_Node *n);
#endif

static JSBool SMJS_FUNCTION(vrml_parse_xml)
{
#ifndef GPAC_DISABLE_SVG
	GF_SceneGraph *sg;
	GF_Node *node;
	char *str;
	GF_Node *gf_sm_load_svg_from_string(GF_SceneGraph *sg, char *svg_str);
	SMJS_ARGS

	str = js_get_utf8(argv[0]);
	if (!str) return JS_TRUE;

	node = JS_GetContextPrivate(c);
	sg = node->sgprivate->scenegraph;

	node = gf_sm_load_svg_from_string(sg, str);
	gf_free(str);
	SMJS_SET_RVAL( dom_element_construct(c, node) );
#endif
	return JS_TRUE;
}


static JSBool SMJS_FUNCTION(getElementById)
{
	GF_Node *elt;
	JSObject *an_obj;
	char *name = NULL;
	u32 ID = 0;
	GF_ScriptPriv *priv = JS_GetScriptStack(c);
	GF_Node *sc = JS_GetContextPrivate(c);
	SMJS_ARGS
	if (JSVAL_IS_STRING(argv[0])) name = SMJS_CHARS(c, argv[0]);
	else if (JSVAL_IS_INT(argv[0])) ID = JSVAL_TO_INT(argv[0]);

	if (!ID && !name) return JS_FALSE;

	elt = NULL;
	if (ID) elt = gf_sg_find_node(sc->sgprivate->scenegraph, ID);
	else elt = gf_sg_find_node_by_name(sc->sgprivate->scenegraph, name);

	SMJS_FREE(c, name);
	if (!elt) return JS_TRUE;

	an_obj = node_get_binding(priv, elt, 0);
	if (an_obj) SMJS_SET_RVAL( OBJECT_TO_JSVAL(an_obj) );
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(replaceWorld)
{
	return JS_TRUE;
}

static void on_route_to_object(GF_Node *node, GF_Route *_r)
{
	jsval argv[2], rval;
	Double time;
	GF_FieldInfo t_info;
	GF_ScriptPriv *priv;
	JSObject *obj;
	GF_RouteToScript *r = (GF_RouteToScript *)_r;
	if (!node) return;
	priv = gf_node_get_private(node);
	if (!priv) return;

	if (!r->FromNode) {
		if (r->obj) {
//			gf_js_remove_root(priv->js_ctx, &r->obj, GF_JSGC_OBJECT);
			r->obj=NULL;
		}
		if ( ! JSVAL_IS_VOID(r->fun)) {
//			gf_js_remove_root(priv->js_ctx, &r->fun, GF_JSGC_OBJECT);
			r->fun=JSVAL_NULL;
		}
		return;
	}

	obj = (JSObject *) r->obj;
	if (!obj) obj = priv->js_obj;

	memset(&t_info, 0, sizeof(GF_FieldInfo));
	time = gf_node_get_scene_time(node);
	t_info.far_ptr = &time;
	t_info.fieldType = GF_SG_VRML_SFTIME;
	t_info.fieldIndex = -1;
	t_info.name = "timestamp";

	gf_sg_lock_javascript(priv->js_ctx, 1);

	argv[1] = gf_sg_script_to_smjs_field(priv, &t_info, node, 1);
	argv[0] = gf_sg_script_to_smjs_field(priv, &r->FromField, r->FromNode, 1);

	/*protect args*/
	if (JSVAL_IS_GCTHING(argv[0])) gf_js_add_root(priv->js_ctx, &argv[0], GF_JSGC_VAL);
	if (JSVAL_IS_GCTHING(argv[1])) gf_js_add_root(priv->js_ctx, &argv[1], GF_JSGC_VAL);

	JS_CallFunctionValue(priv->js_ctx, obj, (jsval) r->fun, 2, argv, &rval);

	/*release args*/
	if (JSVAL_IS_GCTHING(argv[0])) gf_js_remove_root(priv->js_ctx, &argv[0], GF_JSGC_VAL);
	if (JSVAL_IS_GCTHING(argv[1])) gf_js_remove_root(priv->js_ctx, &argv[1], GF_JSGC_VAL);

	/*check any pending exception if outer-most event*/
	if ( (JS_IsRunning(priv->js_ctx)==JS_FALSE) && JS_IsExceptionPending(priv->js_ctx)) {
		JS_ReportPendingException(priv->js_ctx);
		JS_ClearPendingException(priv->js_ctx);
	}

	gf_sg_lock_javascript(priv->js_ctx, 0);

	do_js_gc(priv->js_ctx, node);
}

static JSBool SMJS_FUNCTION(addRoute)
{
	GF_JSField *ptr;
	GF_Node *n1, *n2;
	char *f1, *f2;
	GF_FieldInfo info;
	u32 f_id1, f_id2;
	GF_Err e;
	SMJS_ARGS
	if (argc!=4) return JS_FALSE;

	if (!JSVAL_IS_OBJECT(argv[0]) || !GF_JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[0]), &js_rt->SFNodeClass, NULL) ) return JS_FALSE;

	ptr = (GF_JSField *) SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(argv[0]));
	assert(ptr->field.fieldType==GF_SG_VRML_SFNODE);
	n1 = * ((GF_Node **)ptr->field.far_ptr);
	if (!n1) return JS_FALSE;
	n2 = NULL;

	if (!JSVAL_IS_STRING(argv[1])) return JS_FALSE;
	f1 = SMJS_CHARS(c, argv[1]);
	if (!f1) return JS_FALSE;
	if (!strnicmp(f1, "_field", 6)) {
		f_id1 = atoi(f1+6);
		e = gf_node_get_field(n1, f_id1, &info);
	} else {
		e = gf_node_get_field_by_name(n1, f1, &info);
		f_id1 = info.fieldIndex;
	}
	SMJS_FREE(c, f1);
	if (e != GF_OK) return JS_FALSE;


	if (!JSVAL_IS_OBJECT(argv[2])) return JS_FALSE;

	/*regular route*/
	if (GF_JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[2]), &js_rt->SFNodeClass, NULL) && JSVAL_IS_STRING(argv[3]) ) {
		GF_Route *r;
		ptr = (GF_JSField *) SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(argv[2]));
		assert(ptr->field.fieldType==GF_SG_VRML_SFNODE);
		n2 = * ((GF_Node **)ptr->field.far_ptr);
		if (!n2) return JS_FALSE;

		f2 = SMJS_CHARS(c, argv[3]);
		if (!f2) return JS_FALSE;

		if (!strnicmp(f2, "_field", 6)) {
			f_id2 = atoi(f2+6);
			e = gf_node_get_field(n2, f_id2, &info);
		} else {
			if ((n2->sgprivate->tag==TAG_MPEG4_Script)
#ifndef GPAC_DISABLE_X3D
			        || (n2->sgprivate->tag==TAG_X3D_Script)
#endif
			   ) {
				GF_FieldInfo src = info;
				if (gf_node_get_field_by_name(n2, f2, &info) != GF_OK) {
					gf_sg_script_field_new(n2, GF_SG_SCRIPT_TYPE_EVENT_IN, src.fieldType, f2);
				}
			}
			e = gf_node_get_field_by_name(n2, f2, &info);
			f_id2 = info.fieldIndex;
		}
		SMJS_FREE(c, f2);
		if (e != GF_OK) return JS_FALSE;

		r = gf_sg_route_new(n1->sgprivate->scenegraph, n1, f_id1, n2, f_id2);
		if (!r) return JS_FALSE;
	}
	/*route to object*/
	else {
		u32 i = 0;
		const char *fun_name;
		GF_RouteToScript *r = NULL;
		if (!JSVAL_IS_OBJECT(argv[3]) || !JS_ObjectIsFunction(c, JSVAL_TO_OBJECT(argv[3])) ) return JS_FALSE;

		fun_name = JS_GetFunctionName( JS_ValueToFunction(c, argv[3] ) );
		if (fun_name && n1->sgprivate->interact && n1->sgprivate->interact->routes ) {
			while ( (r = (GF_RouteToScript*)gf_list_enum(n1->sgprivate->interact->routes, &i) )) {
				if ( (r->FromNode == n1)
				        && (r->FromField.fieldIndex == f_id1)
				        && (r->ToNode == (GF_Node*)JS_GetScript(c))
				        && !stricmp(r->ToField.name, fun_name)
				   )
					break;
			}
		}

		if ( !r ) {
			GF_SAFEALLOC(r, GF_RouteToScript)
			if (!r) return JS_FALSE;
			r->FromNode = n1;
			r->FromField.fieldIndex = f_id1;
			gf_node_get_field(r->FromNode, f_id1, &r->FromField);

			r->ToNode = (GF_Node*)JS_GetScript(c);
			r->ToField.fieldType = GF_SG_VRML_SCRIPT_FUNCTION;
			r->ToField.on_event_in = on_route_to_object;
			r->ToField.eventType = GF_SG_EVENT_IN;
			r->ToField.far_ptr = NULL;
			r->ToField.name = fun_name;

			r->obj = JSVAL_TO_OBJECT( argv[2] ) ;
//			gf_js_add_root(c, & r->obj, GF_JSGC_OBJECT);

			r->fun = argv[3];
//			gf_js_add_root(c, &r->fun, GF_JSGC_OBJECT);

			r->is_setup = 1;
			r->graph = n1->sgprivate->scenegraph;

			if (!n1->sgprivate->interact) {
				GF_SAFEALLOC(n1->sgprivate->interact, struct _node_interactive_ext);
				if (!n1->sgprivate->interact) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[VRMLJS] Failed to create interact storage\n"));
					return JS_FALSE;
				}
			}
			if (!n1->sgprivate->interact->routes) n1->sgprivate->interact->routes = gf_list_new();
			gf_list_add(n1->sgprivate->interact->routes, r);
			gf_list_add(n1->sgprivate->scenegraph->Routes, r);
		}
	}

	return JS_TRUE;
}


static JSBool SMJS_FUNCTION(deleteRoute)
{
	GF_JSField *ptr;
	GF_Node *n1, *n2;
	char *f1, *f2;
	GF_FieldInfo info;
	GF_Route *r;
	GF_Err e;
	u32 f_id1, f_id2, i;
	SMJS_ARGS
	if (argc!=4) return JS_FALSE;

	if (!JSVAL_IS_OBJECT(argv[0]) || JSVAL_IS_NULL(argv[0]) || !GF_JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[0]), &js_rt->SFNodeClass, NULL) ) return JS_FALSE;

	if (JSVAL_IS_STRING(argv[1]) && JSVAL_IS_NULL(argv[2]) && JSVAL_IS_NULL(argv[3])) {
		ptr = (GF_JSField *) SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(argv[0]));
		assert(ptr->field.fieldType==GF_SG_VRML_SFNODE);
		n1 = * ((GF_Node **)ptr->field.far_ptr);
		f1 = SMJS_CHARS(c, argv[1]);
		if (!strcmp(f1, "ALL")) {
			while (n1->sgprivate->interact && n1->sgprivate->interact->routes && gf_list_count(n1->sgprivate->interact->routes) ) {
				r = gf_list_get(n1->sgprivate->interact->routes, 0);
				gf_sg_route_del(r);
			}
		}
		SMJS_FREE(c, f1);
		return JS_TRUE;
	}

	if (!JSVAL_IS_OBJECT(argv[2]) || !GF_JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[2]), &js_rt->SFNodeClass, NULL) ) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[1]) || !JSVAL_IS_STRING(argv[3])) return JS_FALSE;

	ptr = (GF_JSField *) SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(argv[0]));
	assert(ptr->field.fieldType==GF_SG_VRML_SFNODE);
	n1 = * ((GF_Node **)ptr->field.far_ptr);
	ptr = (GF_JSField *) SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(argv[2]));
	assert(ptr->field.fieldType==GF_SG_VRML_SFNODE);
	n2 = * ((GF_Node **)ptr->field.far_ptr);

	if (!n1 || !n2) return JS_FALSE;
	if (!n1->sgprivate->interact) return JS_TRUE;

	f1 = SMJS_CHARS(c, argv[1]);
	f2 = SMJS_CHARS(c, argv[3]);
	if (!f1 || !f2) {
		SMJS_FREE(c, f1);
		SMJS_FREE(c, f2);
		return JS_FALSE;
	}

	if (!strnicmp(f1, "_field", 6)) {
		f_id1 = atoi(f1+6);
		e = gf_node_get_field(n1, f_id1, &info);
	} else {
		e = gf_node_get_field_by_name(n1, f1, &info);
		f_id1 = info.fieldIndex;
	}
	SMJS_FREE(c, f1);
	if (e != GF_OK) return JS_FALSE;

	if (!strnicmp(f2, "_field", 6)) {
		f_id2 = atoi(f2+6);
		e = gf_node_get_field(n2, f_id2, &info);
	} else {
		e = gf_node_get_field_by_name(n2, f2, &info);
		f_id2 = info.fieldIndex;
	}
	SMJS_FREE(c, f2);
	if (e != GF_OK) return JS_FALSE;

	i=0;
	while ((r = gf_list_enum(n1->sgprivate->interact->routes, &i))) {
		if (r->FromField.fieldIndex != f_id1) continue;
		if (r->ToNode != n2) continue;
		if (r->ToField.fieldIndex != f_id2) continue;
		gf_sg_route_del(r);
		return JS_TRUE;
	}
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(loadURL)
{
	u32 i;
	GF_JSAPIParam par;
	jsval item;
	jsuint len;
	JSObject *p;
	GF_JSField *f;
	SMJS_ARGS
	M_Script *script = (M_Script *) JS_GetContextPrivate(c);

	if (argc < 1) return JS_FALSE;

	if (JSVAL_IS_STRING(argv[0])) {
		Bool res;
		par.uri.url = SMJS_CHARS(c, argv[0]);
		/*TODO add support for params*/
		par.uri.nb_params = 0;
		res = ScriptAction(c, NULL, GF_JSAPI_OP_LOAD_URL, (GF_Node *)script, &par);
		SMJS_FREE(c, par.uri.url);
		return res ? JS_TRUE : JS_FALSE;
	}
	if (!JSVAL_IS_OBJECT(argv[0])) return JS_FALSE;

	JS_ValueToObject(c, argv[0], &p);

	f = (GF_JSField *) SMJS_GET_PRIVATE(c, p);
	if (!f || !f->js_list) return JS_FALSE;
	JS_GetArrayLength(c, f->js_list, &len);

	for (i=0; i<len; i++) {
		JS_GetElement(c, f->js_list, (jsint) i, &item);

		if (JSVAL_IS_STRING(item)) {
			Bool res;
			par.uri.url = SMJS_CHARS(c, item);
			/*TODO add support for params*/
			par.uri.nb_params = 0;
			res = ScriptAction(c, NULL, GF_JSAPI_OP_LOAD_URL, (GF_Node*)script, &par);
			SMJS_FREE(c, par.uri.url);
			if (res) return JS_TRUE;
		}
	}
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(setDescription)
{
	GF_JSAPIParam par;
	GF_Node *node = JS_GetContextPrivate(c);
	SMJS_ARGS
	if (!argc || !JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	par.uri.url = SMJS_CHARS(c, argv[0]);
	ScriptAction(c, NULL, GF_JSAPI_OP_SET_TITLE, node->sgprivate->scenegraph->RootNode, &par);
	SMJS_FREE(c, par.uri.url);
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(createVrmlFromString)
{
#ifndef GPAC_DISABLE_LOADER_BT
	GF_ScriptPriv *priv;
	GF_FieldInfo field;
	/*BT/VRML from string*/
	GF_List *gf_sm_load_bt_from_string(GF_SceneGraph *in_scene, char *node_str, Bool force_wrl);
	char *str;
	GF_List *nlist;
	SMJS_ARGS
	GF_Node *sc_node = JS_GetContextPrivate(c);
	if (argc < 1) return JS_FALSE;

	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	str = SMJS_CHARS(c, argv[0]);
	nlist = gf_sm_load_bt_from_string(sc_node->sgprivate->scenegraph, str, 1);
	SMJS_FREE(c, str);
	if (!nlist) return JS_FALSE;

	priv = JS_GetScriptStack(c);
	memset(&field, 0, sizeof(GF_FieldInfo));
	field.fieldType = GF_SG_VRML_MFNODE;
	field.far_ptr = &nlist;
	SMJS_SET_RVAL( gf_sg_script_to_smjs_field(priv, &field, NULL, 0) );

	/*don't forget to unregister all this stuff*/
	while (gf_list_count(nlist)) {
		GF_Node *n = gf_list_get(nlist, 0);
		gf_list_rem(nlist, 0);
		gf_node_unregister(n, NULL);
	}
	gf_list_del(nlist);
	return JS_TRUE;
#else
	return JS_FALSE;
#endif
}

void gf_node_event_out_proto(GF_Node *node, u32 FieldIndex);

void Script_FieldChanged(JSContext *c, GF_Node *parent, GF_JSField *parent_owner, GF_FieldInfo *field)
{
	GF_ScriptPriv *priv;
	u32 script_field;
	u32 i;
	GF_ScriptField *sf;


	if (!parent) {
		parent = parent_owner->owner;
		field = &parent_owner->field;
	}
	if (!parent) return;

	script_field = 0;
	if ((parent->sgprivate->tag == TAG_MPEG4_Script)
#ifndef GPAC_DISABLE_X3D
	        || (parent->sgprivate->tag == TAG_X3D_Script)
#endif
	   ) {
		script_field = 1;
		if ( (GF_Node *) JS_GetContextPrivate(c) == parent) script_field = 2;
	}

	if (script_field!=2) {
		if (field->on_event_in) field->on_event_in(parent, NULL);
		else if (script_field && (field->eventType==GF_SG_EVENT_IN) ) {
			gf_sg_script_event_in(parent, field);
			gf_node_changed_internal(parent, field, 0);
			return;
		}
		/*field has changed, set routes...*/
		if (parent->sgprivate->tag == TAG_ProtoNode) {
			GF_ProtoInstance *inst = (GF_ProtoInstance *)parent;
			gf_sg_proto_propagate_event(parent, field->fieldIndex, (GF_Node*)JS_GetScript(c));
			/* Node exposedField can also be routed to another field */
			gf_node_event_out_proto(parent, field->fieldIndex);

			//hardcoded protos be implemented in ways not inspecting the node_dirty propagation scheme (eg defining an SFNode in their interface, not linked with the graph).
			//in this case handle the node as a regular one
			if (inst->flags & GF_SG_PROTO_HARDCODED) {
				gf_node_changed_internal(parent, field, 0);
			}
		} else {
			gf_node_event_out(parent, field->fieldIndex);
			gf_node_changed_internal(parent, field, 0);
		}
		return;
	}
	/*otherwise mark field if eventOut*/
	if (parent_owner || parent) {
		priv = parent ? parent->sgprivate->UserPrivate : parent_owner->owner->sgprivate->UserPrivate;
		i=0;
		while ((sf = gf_list_enum(priv->fields, &i))) {
			if (sf->ALL_index == field->fieldIndex) {
				/*queue eventOut*/
				if (sf->eventType == GF_SG_EVENT_OUT) {
					sf->activate_event_out = 1;
				}
			}
		}
	}
}

SMJS_FUNC_PROP_SET( gf_sg_script_eventout_set_prop)

u32 i;
char *eventName;
GF_ScriptPriv *script;
GF_Node *n;
GF_ScriptField *sf;
GF_FieldInfo info;
jsval idval;
JSString *str;
JS_IdToValue(c, id, &idval);
if (! SMJS_ID_IS_STRING(id)) return JS_FALSE;
str = SMJS_ID_TO_STRING(id);
if (!str) return JS_FALSE;
/*avoids gcc warning*/
if (!obj) obj=NULL;

script = JS_GetScriptStack(c);
if (!script) return JS_FALSE;
n = (GF_Node *) JS_GetScript(c);

eventName = SMJS_CHARS_FROM_STRING(c, str);
i=0;
while ((sf = gf_list_enum(script->fields, &i))) {
	if (!stricmp(sf->name, eventName)) {
		gf_node_get_field(n, sf->ALL_index, &info);
		gf_sg_script_to_node_field(c, *vp, &info, n, NULL);
		sf->activate_event_out = 1;
		SMJS_FREE(c, eventName);
		return JS_TRUE;
	}
}
SMJS_FREE(c, eventName);
return JS_FALSE;
}


/*generic ToString method*/
static GFINLINE void sffield_toString(char *str, void *f_ptr, u32 fieldType)
{
	char temp[1000];

	switch (fieldType) {
	case GF_SG_VRML_SFVEC2F:
	{
		SFVec2f val = * ((SFVec2f *) f_ptr);
		sprintf(temp, "%f %f", FIX2FLT(val.x), FIX2FLT(val.y));
		strcat(str, temp);
		break;
	}
	case GF_SG_VRML_SFVEC3F:
	{
		SFVec3f val = * ((SFVec3f *) f_ptr);
		sprintf(temp, "%f %f %f", FIX2FLT(val.x), FIX2FLT(val.y), FIX2FLT(val.z));
		strcat(str, temp);
		break;
	}
	case GF_SG_VRML_SFVEC4F:
	{
		SFVec4f val = * ((SFVec4f *) f_ptr);
		sprintf(temp, "%f %f %f %f", FIX2FLT(val.x), FIX2FLT(val.y), FIX2FLT(val.z), FIX2FLT(val.q));
		strcat(str, temp);
		break;
	}
	case GF_SG_VRML_SFROTATION:
	{
		SFRotation val = * ((SFRotation *) f_ptr);
		sprintf(temp, "%f %f %f %f", FIX2FLT(val.x), FIX2FLT(val.y), FIX2FLT(val.z), FIX2FLT(val.q));
		strcat(str, temp);
		break;
	}
	case GF_SG_VRML_SFCOLOR:
	{
		SFColor val = * ((SFColor *) f_ptr);
		sprintf(temp, "%f %f %f", FIX2FLT(val.red), FIX2FLT(val.green), FIX2FLT(val.blue));
		strcat(str, temp);
		break;
	}
	case GF_SG_VRML_SFIMAGE:
	{
		SFImage *val = ((SFImage *)f_ptr);
		sprintf(temp, "%dx%dx%d", val->width, val->height, val->numComponents);
		strcat(str, temp);
		break;
	}

	}
}

static void JS_ObjectDestroyed(JSContext *c, JSObject *obj, GF_JSField *ptr, Bool is_js_call)
{
	SMJS_SET_PRIVATE(c, obj, NULL);

	if (ptr) {
		/*if ptr is a node, remove node binding*/
		if (ptr->node
		        && ptr->node->sgprivate->interact
		        && ptr->node->sgprivate->interact->js_binding
		        && (ptr->node->sgprivate->interact->js_binding->node == ptr)
		   ) {
			ptr->node->sgprivate->interact->js_binding->node = NULL;
		}

		/*if ptr is a field, remove field binding from parent*/
		if (ptr->owner && ptr->owner->sgprivate->interact && ptr->owner->sgprivate->interact->js_binding) {
			gf_list_del_item(ptr->owner->sgprivate->interact->js_binding->fields, ptr);
		}


		/*
			If object is still registered, remove it from the js_cache

						!!! WATCHOUT !!!
		SpiderMonkey GC may call an object finalizer on ANY context, not the one in which the object
		was created !! We therefore keep a pointer to the parent context to remove GC'ed objects from the context cache
		*/
		if (ptr->obj && is_js_call) {
			GF_ScriptPriv *priv;
			if (ptr->js_ctx) {
				if (gf_list_find(js_rt->allocated_contexts, ptr->js_ctx) < 0)
					return;
				c = ptr->js_ctx;
			}
			priv = JS_GetScriptStack(c);
			gf_list_del_item(priv->js_cache, obj);
		}
	}
}


static JSBool SMJS_FUNCTION(field_toString)
{
	u32 i;
	jsuint len;
	jsdouble d;
	char temp[1000];
	char str[5000];
	JSString *s;
	jsval item;
	SMJS_OBJ
	GF_JSField *f = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);
	if (!f) return JS_FALSE;

	strcpy(str, "");

	if (gf_sg_vrml_is_sf_field(f->field.fieldType)) {
		sffield_toString(str, f->field.far_ptr, f->field.fieldType);
	} else {
		if (f->field.fieldType == GF_SG_VRML_MFNODE) return JS_TRUE;

		strcat(str, "[");
		JS_GetArrayLength(c, f->js_list, &len);
		for (i=0; i<len; i++) {
			JS_GetElement(c, f->js_list, (jsint) i, &item);
			switch (f->field.fieldType) {
			case GF_SG_VRML_MFBOOL:
				sprintf(temp, "%s", JSVAL_TO_BOOLEAN(item) ? "TRUE" : "FALSE");
				strcat(str, temp);
				break;
			case GF_SG_VRML_MFINT32:
				sprintf(temp, "%d", JSVAL_TO_INT(item));
				strcat(str, temp);
				break;
			case GF_SG_VRML_MFFLOAT:
			case GF_SG_VRML_MFTIME:
				SMJS_GET_NUMBER(item, d);
				sprintf(temp, "%g", d);
				strcat(str, temp);
				break;
			case GF_SG_VRML_MFSTRING:
			case GF_SG_VRML_MFURL:
			{
				char *str_val = SMJS_CHARS(c, item);
				strcat(str, str_val);
				SMJS_FREE(c, str_val);
			}
			break;
			default:
				if (JSVAL_IS_OBJECT(item)) {
					GF_JSField *sf = (GF_JSField *) SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(item));
					sffield_toString(str, sf->field.far_ptr, sf->field.fieldType);
				}
				break;
			}
			if (i<len-1) strcat(str, ", ");
		}
		strcat(str, "]");
	}
	s = JS_NewStringCopyZ(c, str);
	if (!s) return JS_FALSE;
	SMJS_SET_RVAL( STRING_TO_JSVAL(s) );
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(SFNodeConstructor)
{
	u32 tag, ID;
	GF_Node *new_node;
	GF_JSField *field;
	GF_Proto *proto;
	GF_SceneGraph *sg;
	char *node_name;
	GF_ScriptPriv *priv = JS_GetScriptStack(c);
	M_Script *sc = JS_GetScript(c);
	SMJS_ARGS
	SMJS_OBJ_CONSTRUCTOR(&js_rt->SFNodeClass)
	tag = 0;
	if (!argc) {
		field = NewJSField(c);
		field->field.fieldType = GF_SG_VRML_SFNODE;
		field->node = NULL;
		field->field.far_ptr = &field->node;
		SMJS_SET_PRIVATE(c, obj, field);
		SMJS_SET_RVAL( OBJECT_TO_JSVAL(obj) );
		return JS_TRUE;
	}
	if (!GF_JS_InstanceOf(c, obj, &js_rt->SFNodeClass, NULL) ) return JS_FALSE;

	ID = 0;
	node_name = SMJS_CHARS(c, argv[0]);
	if (!strnicmp(node_name, "_proto", 6)) {
		ID = atoi(node_name+6);
		SMJS_FREE(c, node_name);
		node_name = NULL;

locate_proto:

		/*locate proto in current graph and all parents*/
		sg = sc->sgprivate->scenegraph;
		while (1) {
			proto = gf_sg_find_proto(sg, ID, node_name);
			if (proto) break;
			if (!sg->parent_scene) break;
			sg = sg->parent_scene;
		}
		if (!proto) {
			SMJS_FREE(c, node_name);
			return JS_FALSE;
		}
		/* create interface and load code in current graph*/
		new_node = gf_sg_proto_create_instance(sc->sgprivate->scenegraph, proto);
		if (!new_node) {
			SMJS_FREE(c, node_name);
			return JS_FALSE;
		}
		/*OK, instantiate proto code*/
		if (gf_sg_proto_load_code(new_node) != GF_OK) {
			gf_node_unregister(new_node, NULL);
			SMJS_FREE(c, node_name);
			return JS_FALSE;
		}
	} else {
		switch (sc->sgprivate->tag) {
		case TAG_MPEG4_Script:
			tag = gf_node_mpeg4_type_by_class_name(node_name);
			break;
#ifndef GPAC_DISABLE_X3D
		case TAG_X3D_Script:
			tag = gf_node_x3d_type_by_class_name(node_name);
			break;
#endif
		}
		if (!tag) goto locate_proto;
		new_node = gf_node_new(sc->sgprivate->scenegraph, tag);
		if (!new_node) {
			SMJS_FREE(c, node_name);
			return JS_FALSE;
		}
		gf_node_init(new_node);
	}

	SMJS_FREE(c, node_name);

	obj = node_get_binding(priv, new_node, 1);
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(obj) );

	return JS_TRUE;
}
static void node_finalize_ex(JSContext *c, JSObject * obj, Bool is_js_call)
{
	GF_JSField *ptr = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);

#ifndef USE_FFDEV_15
	JS_ObjectDestroyed(c, obj, ptr, is_js_call);
#endif

	if (ptr) {
		JS_GetScript(ptr->js_ctx ? ptr->js_ctx : c);
		if (ptr->node
		        /*num_instances may be 0 if the node is the script being destroyed*/
		        && ptr->node->sgprivate->num_instances
		   ) {

			GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[VRML JS] unregistering node %s (%s)\n", gf_node_get_name(ptr->node), gf_node_get_class_name(ptr->node)));
			//gf_node_unregister(ptr->node, (ptr->node==parent) ? NULL : parent);
			gf_node_unregister(ptr->node, NULL);
		}
		gf_free(ptr);
	}
}

static DECL_FINALIZE(node_finalize)

node_finalize_ex(c, obj, 1);
}

static SMJS_FUNC_PROP_GET(node_getProperty)

GF_Node *n;
u32 index;
JSString *str;
GF_FieldInfo info;
GF_JSField *ptr;
GF_ScriptPriv *priv;

if (! GF_JS_InstanceOf(c, obj, &js_rt->SFNodeClass, NULL) )
	return JS_FALSE;
ptr = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);
assert(ptr->field.fieldType==GF_SG_VRML_SFNODE);
n = * ((GF_Node **)ptr->field.far_ptr);
priv = JS_GetScriptStack(c);

if (n && SMJS_ID_IS_STRING(id) && ( (str = SMJS_ID_TO_STRING(id)) != 0) ) {
	char *fieldName = SMJS_CHARS_FROM_STRING(c, str);
	if (!strnicmp(fieldName, "toString", 8)) {
		SMJS_FREE(c, fieldName);
		return JS_TRUE;
	}
	/*fieldID indexing*/
	if (!strnicmp(fieldName, "_field", 6)) {
		index = atoi(fieldName+6);
		if ( gf_node_get_field(n, index, &info) == GF_OK) {
			*vp = gf_sg_script_to_smjs_field(priv, &info, n, 0);
			SMJS_FREE(c, fieldName);
			return JS_TRUE;
		}
	} else if ( gf_node_get_field_by_name(n, fieldName, &info) == GF_OK) {
		*vp = gf_sg_script_to_smjs_field(priv, &info, n, 0);
		SMJS_FREE(c, fieldName);
		return JS_TRUE;
	}

	if (!strcmp(fieldName, "_bounds")) {
		GF_JSAPIParam par;
		par.bbox.is_set = 0;
		if (ScriptAction(c, n->sgprivate->scenegraph, GF_JSAPI_OP_GET_LOCAL_BBOX, (GF_Node *)n, &par) ) {
			JSObject *_obj = JS_NewObject(priv->js_ctx, &js_rt->AnyClass._class, 0, 0);
			Float x, y, w, h;
			x = y = w = h = 0;
			if (par.bbox.is_set) {
				x = FIX2FLT(par.bbox.min_edge.x);
				y = FIX2FLT(par.bbox.min_edge.y);
				w = FIX2FLT(par.bbox.max_edge.x - par.bbox.min_edge.x);
				h = FIX2FLT(par.bbox.max_edge.y - par.bbox.min_edge.y);
			}
			JS_DefineProperty(priv->js_ctx, _obj, "x", JS_MAKE_DOUBLE(c, x), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT );
			JS_DefineProperty(priv->js_ctx, _obj, "y", JS_MAKE_DOUBLE(c, y), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT );
			JS_DefineProperty(priv->js_ctx, _obj, "width", JS_MAKE_DOUBLE(c, w), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT );
			JS_DefineProperty(priv->js_ctx, _obj, "height", JS_MAKE_DOUBLE(c, h), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT );
			*vp = OBJECT_TO_JSVAL(_obj);
			SMJS_FREE(c, fieldName);
			return JS_TRUE;
		}
	}
	SMJS_FREE(c, fieldName);
	return JS_TRUE;
}

return JS_FALSE;
}

static SMJS_FUNC_PROP_SET( node_setProperty)

GF_Node *n;
GF_FieldInfo info;
u32 index;
char *fieldname;
GF_JSField *ptr;

if (! GF_JS_InstanceOf(c, obj, &js_rt->SFNodeClass, NULL) ) return JS_FALSE;
ptr = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);

/*this is the prototype*/
if (!ptr) {
	if (! SMJS_ID_IS_STRING(id)) return JS_FALSE;
	return JS_TRUE;
}

assert(ptr->field.fieldType==GF_SG_VRML_SFNODE);
n = * ((GF_Node **)ptr->field.far_ptr);

if (n && SMJS_ID_IS_STRING(id)) {
	JSString *str = SMJS_ID_TO_STRING(id);
	fieldname = SMJS_CHARS_FROM_STRING(c, str);

	/*fieldID indexing*/
	if (!strnicmp(fieldname, "_field", 6)) {
		index = atoi(fieldname+6);
		SMJS_FREE(c, fieldname);
		if ( gf_node_get_field(n, index, &info) != GF_OK) {
			SMJS_FREE(c, fieldname);
			return JS_TRUE;
		}
	} else {
		if (gf_node_get_field_by_name(n, fieldname, &info) != GF_OK) {
			/*VRML style*/
			if (!strnicmp(fieldname, "set_", 4)) {
				if (gf_node_get_field_by_name(n, fieldname + 4, &info) != GF_OK) {
					SMJS_FREE(c, fieldname);
					return JS_TRUE;
				}
			} else {
				SMJS_FREE(c, fieldname);
				return JS_TRUE;
			}
		}
	}
	SMJS_FREE(c, fieldname);

	if (gf_node_get_tag(n)==TAG_ProtoNode)
		gf_sg_proto_mark_field_loaded(n, &info);

	gf_sg_script_to_node_field(c, *vp, &info, n, ptr);
}
return JS_TRUE;
}
static JSBool SMJS_FUNCTION(node_toString)
{
	char str[1000];
	u32 id;
	GF_Node *n;
	JSString *s;
	GF_JSField *f;
	const char *name;
	SMJS_OBJ
	if (! GF_JS_InstanceOf(c, obj, &js_rt->SFNodeClass, NULL) ) return JS_FALSE;
	f = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);
	if (!f) return JS_FALSE;

	str[0] = 0;
	n = * ((GF_Node **)f->field.far_ptr);
	if (!n) return JS_FALSE;

	name = gf_node_get_name_and_id(n, &id);
	if (id) {
		if (name) {
			sprintf(str , "DEF %s ", name);
		} else {
			sprintf(str , "DEF %d ", id - 1);
		}
	}
	strcat(str, gf_node_get_class_name(n));
	s = JS_NewStringCopyZ(c, (const char *) str);
	if (!s) return JS_FALSE;
	SMJS_SET_RVAL( STRING_TO_JSVAL(s) );
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(node_getTime)
{
	GF_Node *n;
	GF_JSField *f;
	SMJS_OBJ
	if (! GF_JS_InstanceOf(c, obj, &js_rt->SFNodeClass, NULL) ) return JS_FALSE;
	f = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);
	if (!f) return JS_FALSE;

	n = * ((GF_Node **)f->field.far_ptr);
	if (!n) return JS_FALSE;
	SMJS_SET_RVAL( JS_MAKE_DOUBLE(c, gf_node_get_scene_time(n)) );
	return JS_TRUE;
}

/* Generic field destructor */
static DECL_FINALIZE(field_finalize)

GF_JSField *ptr = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);
JS_ObjectDestroyed(c, obj, ptr, 1);
if (!ptr) return;

if (ptr->field_ptr) gf_sg_vrml_field_pointer_del(ptr->field_ptr, ptr->field.fieldType);
gf_free(ptr);
}





/*SFImage class functions */
static GFINLINE GF_JSField *SFImage_Create(JSContext *c, JSObject *obj, u32 w, u32 h, u32 nbComp, MFInt32 *pixels)
{
	u32 i, len;
	GF_JSField *field;
	SFImage *v;
	field = NewJSField(c);
	v = gf_sg_vrml_field_pointer_new(GF_SG_VRML_SFIMAGE);
	field->field_ptr = field->field.far_ptr = v;
	field->field.fieldType = GF_SG_VRML_SFIMAGE;
	v->width = w;
	v->height = h;
	v->numComponents = nbComp;
	v->pixels = (u8 *) gf_malloc(sizeof(u8) * nbComp * w * h);
	len = MIN(nbComp * w * h, pixels->count);
	for (i=0; i<len; i++) v->pixels[i] = (u8) pixels->vals[i];
	SMJS_SET_PRIVATE(c, obj, field);
	return field;
}
static JSBool SMJS_FUNCTION(SFImageConstructor)
{
	u32 w, h, nbComp;
	MFInt32 *pixels;
	SMJS_ARGS
	SMJS_OBJ_CONSTRUCTOR(&js_rt->SFImageClass)
	if (argc<4) return 0;
	if (!JSVAL_IS_INT(argv[0]) || !JSVAL_IS_INT(argv[1]) || !JSVAL_IS_INT(argv[2])) return JS_FALSE;
	if (!JSVAL_IS_OBJECT(argv[3]) || !GF_JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[3]), &js_rt->MFInt32Class, NULL)) return JS_FALSE;
	w = JSVAL_TO_INT(argv[0]);
	h = JSVAL_TO_INT(argv[1]);
	nbComp = JSVAL_TO_INT(argv[2]);
	pixels = (MFInt32 *) ((GF_JSField *) SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(argv[3])))->field.far_ptr;
	SFImage_Create(c, obj, w, h, nbComp, pixels);
	return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(image_getProperty)

GF_ScriptPriv *priv = JS_GetScriptStack(c);
GF_JSField *val = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);
SFImage *sfi;
if (!val) return JS_FALSE;
sfi = (SFImage*)val->field.far_ptr;
if (SMJS_ID_IS_INT(id)) {
	switch (SMJS_ID_TO_INT(id)) {
	case 0:
		*vp = INT_TO_JSVAL( sfi->width );
		break;
	case 1:
		*vp = INT_TO_JSVAL( sfi->height);
		break;
	case 2:
		*vp = INT_TO_JSVAL( sfi->numComponents );
		break;
	case 3:
	{
		u32 i, len;
		JSObject *an_obj = SMJS_CONSTRUCT_OBJECT(priv->js_ctx, &js_rt->MFInt32Class, priv->js_obj);
		len = sfi->width*sfi->height*sfi->numComponents;
		for (i=0; i<len; i++) {
			jsval newVal = INT_TO_JSVAL(sfi->pixels[i]);
			JS_SetElement(priv->js_ctx, an_obj, (jsint) i, &newVal);
		}
	}
	break;
	default:
		return JS_TRUE;
	}
}
return JS_TRUE;
}

static SMJS_FUNC_PROP_SET( image_setProperty)

u32 ival;
Bool changed = 0;
GF_JSField *ptr = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);
SFImage *sfi;

/*this is the prototype*/
if (!ptr) {
	if (! SMJS_ID_IS_STRING(id)) return JS_FALSE;
	return JS_TRUE;
}
sfi = (SFImage*)ptr->field.far_ptr;

if (SMJS_ID_IS_INT(id) && SMJS_ID_TO_INT(id) >= 0 && SMJS_ID_TO_INT(id) < 4) {
	switch (SMJS_ID_TO_INT(id)) {
	case 0:
		ival = JSVAL_TO_INT(*vp);
		changed = ! (sfi->width == ival);
		sfi->width = ival;
		if (changed && sfi->pixels) {
			gf_free(sfi->pixels);
			sfi->pixels = NULL;
		}
		break;
	case 1:
		ival =  JSVAL_TO_INT(*vp);
		changed = ! (sfi->height == ival);
		sfi->height = ival;
		if (changed && sfi->pixels) {
			gf_free(sfi->pixels);
			sfi->pixels = NULL;
		}
		break;
	case 2:
		ival =  JSVAL_TO_INT(*vp);
		changed = ! (sfi->numComponents == ival);
		sfi->numComponents = ival;
		if (changed && sfi->pixels) {
			gf_free(sfi->pixels);
			sfi->pixels = NULL;
		}
		break;
	case 3:
	{
		MFInt32 *pixels;
		u32 len, i;
		if (!JSVAL_IS_OBJECT(*vp) || !GF_JS_InstanceOf(c, JSVAL_TO_OBJECT(*vp), &js_rt->MFInt32Class, NULL)) return JS_FALSE;
		pixels = (MFInt32 *) ((GF_JSField *) SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(*vp)))->field.far_ptr;
		if (sfi->pixels) gf_free(sfi->pixels);
		len = sfi->width*sfi->height*sfi->numComponents;
		sfi->pixels = (unsigned char *) gf_malloc(sizeof(char)*len);
		len = MAX(len, pixels->count);
		for (i=0; i<len; i++) sfi->pixels[i] = (u8) pixels->vals[i];
		changed = 1;
		break;
	}
	default:
		return JS_FALSE;
	}
	if (changed) Script_FieldChanged(c, NULL, ptr, NULL);
	return JS_TRUE;
}
return JS_FALSE;
}

/*SFVec2f class functions */
static GFINLINE GF_JSField *SFVec2f_Create(JSContext *c, JSObject *obj, Fixed x, Fixed y)
{
	GF_JSField *field;
	SFVec2f *v;
	field = NewJSField(c);
	v = gf_sg_vrml_field_pointer_new(GF_SG_VRML_SFVEC2F);
	field->field_ptr = field->field.far_ptr = v;
	field->field.fieldType = GF_SG_VRML_SFVEC2F;
	v->x = x;
	v->y = y;
	SMJS_SET_PRIVATE(c, obj, field);
	return field;
}
static JSBool SMJS_FUNCTION(SFVec2fConstructor)
{
	jsdouble x = 0.0, y = 0.0;
	SMJS_ARGS
	SMJS_OBJ_CONSTRUCTOR(&js_rt->SFVec2fClass)
	if (argc > 0) SMJS_GET_NUMBER(argv[0], x);
	if (argc > 1) SMJS_GET_NUMBER(argv[1], y);

	SFVec2f_Create(c, obj, FLT2FIX( x), FLT2FIX( y));
	return JS_TRUE;
}
static SMJS_FUNC_PROP_GET(vec2f_getProperty)

GF_JSField *val = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);

if (SMJS_ID_IS_INT(id)) {
	switch (SMJS_ID_TO_INT(id)) {
	case 0:
		*vp = JS_MAKE_DOUBLE(c, FIX2FLT( ((SFVec2f*)val->field.far_ptr)->x));
		break;
	case 1:
		*vp = JS_MAKE_DOUBLE(c, FIX2FLT( ((SFVec2f*)val->field.far_ptr)->y));
		break;
	default:
		return JS_TRUE;
	}
}
return JS_TRUE;
}

static SMJS_FUNC_PROP_SET(vec2f_setProperty)

jsdouble d;
Fixed v;
Bool changed = 0;
GF_JSField *ptr = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);

/*this is the prototype*/
if (!ptr) {
	if (! SMJS_ID_IS_STRING(id)) return JS_FALSE;
	return JS_TRUE;
}

if (SMJS_ID_IS_INT(id)) {
	if (! JSVAL_IS_NUMBER(*vp)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[VRML JS] Value is not a number while assigning SFVec2f\n"));
		return JS_FALSE;
	}

	SMJS_GET_NUMBER(*vp, d);

	switch (SMJS_ID_TO_INT(id)) {
	case 0:
		v = FLT2FIX( d);
		changed = ! ( ((SFVec2f*)ptr->field.far_ptr)->x == v);
		((SFVec2f*)ptr->field.far_ptr)->x = v;
		break;
	case 1:
		v = FLT2FIX( d);
		changed = ! ( ((SFVec2f*)ptr->field.far_ptr)->y == v);
		((SFVec2f*)ptr->field.far_ptr)->y = v;
		break;
	default:
		return JS_TRUE;
	}
	if (changed) Script_FieldChanged(c, NULL, ptr, NULL);
	return JS_TRUE;
}
return JS_FALSE;
}

static JSBool SMJS_FUNCTION(vec2f_add)
{
	SFVec2f *v1, *v2;
	JSObject *pNew;
	SMJS_ARGS
	SMJS_OBJ
	if (argc<=0 || !JSVAL_IS_OBJECT(argv[0]) || !GF_JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[0]), &js_rt->SFVec2fClass, NULL))
		return JS_FALSE;

	v1 = ((GF_JSField *) SMJS_GET_PRIVATE(c, obj))->field.far_ptr;
	v2 = ((GF_JSField *) SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(argv[0])))->field.far_ptr;
	pNew = JS_NewObject(c, &js_rt->SFVec2fClass._class, 0, SMJS_GET_PARENT(c, obj));
	SFVec2f_Create(c, pNew, v1->x + v2->x, v1->y + v2->y);
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(pNew) );
	return JS_TRUE;
}
static JSBool SMJS_FUNCTION(vec2f_subtract)
{
	SFVec2f *v1, *v2;
	JSObject *pNew;
	SMJS_ARGS
	SMJS_OBJ
	if (argc<=0 || !JSVAL_IS_OBJECT(argv[0]) || !GF_JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[0]), &js_rt->SFVec2fClass, NULL))
		return JS_FALSE;

	v1 = ((GF_JSField *) SMJS_GET_PRIVATE(c, obj))->field.far_ptr;
	v2 = ((GF_JSField *) SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(argv[0])))->field.far_ptr;
	pNew = JS_NewObject(c, &js_rt->SFVec2fClass._class, 0, SMJS_GET_PARENT(c, obj));
	SFVec2f_Create(c, pNew, v1->x - v2->x, v1->y - v2->y);
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(pNew) );
	return JS_TRUE;
}
static JSBool SMJS_FUNCTION(vec2f_negate)
{
	SFVec2f *v1;
	JSObject *pNew;
	SMJS_OBJ
	v1 = ((GF_JSField *) SMJS_GET_PRIVATE(c, obj))->field.far_ptr;
	pNew = JS_NewObject(c, &js_rt->SFVec2fClass._class, 0, SMJS_GET_PARENT(c, obj));
	SFVec2f_Create(c, pNew, -v1->x , -v1->y );
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(pNew) );
	return JS_TRUE;
}
static JSBool SMJS_FUNCTION(vec2f_multiply)
{
	SFVec2f *v1;
	JSObject *pNew;
	jsdouble d;
	Fixed v;
	SMJS_ARGS
	SMJS_OBJ
	if (argc<=0) return JS_FALSE;
	v1 = ((GF_JSField *) SMJS_GET_PRIVATE(c, obj))->field.far_ptr;
	pNew = JS_NewObject(c, &js_rt->SFVec2fClass._class, 0, SMJS_GET_PARENT(c, obj));
	SMJS_GET_NUMBER(argv[0], d );
	v = FLT2FIX( d);
	SFVec2f_Create(c, pNew, gf_mulfix(v1->x , v), gf_mulfix(v1->y, v) );
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(pNew) );
	return JS_TRUE;
}
static JSBool SMJS_FUNCTION(vec2f_divide)
{
	SFVec2f *v1;
	JSObject *pNew;
	jsdouble d;
	Fixed v;
	SMJS_ARGS
	SMJS_OBJ
	if (argc<=0) return JS_FALSE;
	v1 = ((GF_JSField *) SMJS_GET_PRIVATE(c, obj))->field.far_ptr;
	pNew = JS_NewObject(c, &js_rt->SFVec2fClass._class, 0, SMJS_GET_PARENT(c, obj));
	SMJS_GET_NUMBER(argv[0], d );
	v = FLT2FIX(d);
	SFVec2f_Create(c, pNew, gf_divfix(v1->x, v),  gf_divfix(v1->y, v));
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(pNew) );
	return JS_TRUE;
}
static JSBool SMJS_FUNCTION(vec2f_length)
{
	Double res;
	SFVec2f *v1;
	SMJS_OBJ
	v1 = ((GF_JSField *) SMJS_GET_PRIVATE(c, obj))->field.far_ptr;
	res = FIX2FLT(gf_v2d_len(v1));
	SMJS_SET_RVAL( JS_MAKE_DOUBLE(c, res) );
	return JS_TRUE;
}
static JSBool SMJS_FUNCTION(vec2f_normalize)
{
	SFVec2f *v1;
	Fixed res;
	JSObject *pNew;
	SMJS_OBJ
	v1 = ((GF_JSField *) SMJS_GET_PRIVATE(c, obj))->field.far_ptr;
	res = gf_v2d_len(v1);
	pNew = JS_NewObject(c, &js_rt->SFVec2fClass._class, 0, SMJS_GET_PARENT(c, obj));
	SFVec2f_Create(c, pNew, gf_divfix(v1->x, res), gf_divfix(v1->y, res) );
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(pNew) );
	return JS_TRUE;
}
static JSBool SMJS_FUNCTION(vec2f_dot)
{
	SFVec2f *v1, *v2;
	SMJS_OBJ
	SMJS_ARGS
	if (argc<=0 || !JSVAL_IS_OBJECT(argv[0]) || !GF_JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[0]), &js_rt->SFVec2fClass, NULL))
		return JS_FALSE;

	v1 = ((GF_JSField *) SMJS_GET_PRIVATE(c, obj))->field.far_ptr;
	v2 = ((GF_JSField *) SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(argv[0])))->field.far_ptr;
	SMJS_SET_RVAL( JS_MAKE_DOUBLE(c, FIX2FLT( gf_mulfix(v1->x, v2->x) + gf_mulfix(v1->y, v2->y) ) ) );
	return JS_TRUE;
}


/*SFVec3f class functions */
static GFINLINE GF_JSField *SFVec3f_Create(JSContext *c, JSObject *obj, Fixed x, Fixed y, Fixed z)
{
	GF_JSField *field;
	SFVec3f *v;
	field = NewJSField(c);
	v = gf_sg_vrml_field_pointer_new(GF_SG_VRML_SFVEC3F);
	field->field_ptr = field->field.far_ptr = v;
	field->field.fieldType = GF_SG_VRML_SFVEC3F;
	v->x = x;
	v->y = y;
	v->z = z;
	SMJS_SET_PRIVATE(c, obj, field);
	return field;
}
static JSBool SMJS_FUNCTION(SFVec3fConstructor)
{
	SMJS_ARGS
	jsdouble x = 0.0, y = 0.0, z = 0.0;
	SMJS_OBJ_CONSTRUCTOR(&js_rt->SFVec3fClass)
	if (argc > 0) SMJS_GET_NUMBER(argv[0], x);
	if (argc > 1) SMJS_GET_NUMBER(argv[1], y);
	if (argc > 2) SMJS_GET_NUMBER(argv[2], z);
	SFVec3f_Create(c, obj, FLT2FIX( x), FLT2FIX( y), FLT2FIX( z));
	return JS_TRUE;
}
static SMJS_FUNC_PROP_GET(vec3f_getProperty)

GF_JSField *val = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);
if (SMJS_ID_IS_INT(id)) {
	switch (SMJS_ID_TO_INT(id)) {
	case 0:
		*vp = JS_MAKE_DOUBLE(c, FIX2FLT( ((SFVec3f*)val->field.far_ptr)->x) );
		break;
	case 1:
		*vp = JS_MAKE_DOUBLE(c, FIX2FLT( ((SFVec3f*)val->field.far_ptr)->y) );
		break;
	case 2:
		*vp = JS_MAKE_DOUBLE(c, FIX2FLT( ((SFVec3f*)val->field.far_ptr)->z) );
		break;
	default:
		return JS_TRUE;
	}
}
return JS_TRUE;
}
static SMJS_FUNC_PROP_SET( vec3f_setProperty )

jsdouble d;
Fixed v;
Bool changed = 0;
GF_JSField *ptr = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);

/*this is the prototype*/
if (!ptr) {
	if (! SMJS_ID_IS_STRING(id)) return JS_FALSE;
	return JS_TRUE;
}

if (! JSVAL_IS_NUMBER(*vp)) {
	GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[VRML JS] Value is not a number while assigning SFVec3f\n"));
	return JS_FALSE;
}

if (SMJS_ID_IS_INT(id) && SMJS_ID_TO_INT(id) >= 0 && SMJS_ID_TO_INT(id) < 3) {

	SMJS_GET_NUMBER(*vp, d);

	switch (SMJS_ID_TO_INT(id)) {
	case 0:
		v = FLT2FIX( d);
		changed = ! ( ((SFVec3f*)ptr->field.far_ptr)->x == v);
		((SFVec3f*)ptr->field.far_ptr)->x = v;
		break;
	case 1:
		v = FLT2FIX( d);
		changed = ! ( ((SFVec3f*)ptr->field.far_ptr)->y == v);
		((SFVec3f*)ptr->field.far_ptr)->y = v;
		break;
	case 2:
		v = FLT2FIX( d);
		changed = ! ( ((SFVec3f*)ptr->field.far_ptr)->z == v);
		((SFVec3f*)ptr->field.far_ptr)->z = v;
		break;
	default:
		return JS_TRUE;
	}
	if (changed) Script_FieldChanged(c, NULL, ptr, NULL);
	return JS_TRUE;
}
return JS_FALSE;
}
static JSBool SMJS_FUNCTION(vec3f_add)
{
	SFVec3f *v1, *v2;
	JSObject *pNew;
	SMJS_ARGS
	SMJS_OBJ
	if (argc<=0 || !JSVAL_IS_OBJECT(argv[0]) || !GF_JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[0]), &js_rt->SFVec3fClass, NULL))
		return JS_FALSE;

	v1 = ((GF_JSField *) SMJS_GET_PRIVATE(c, obj))->field.far_ptr;
	v2 = ((GF_JSField *) SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(argv[0])))->field.far_ptr;
	pNew = JS_NewObject(c, &js_rt->SFVec3fClass._class, 0, SMJS_GET_PARENT(c, obj));
	SFVec3f_Create(c, pNew, v1->x + v2->x, v1->y + v2->y, v1->z + v2->z);
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(pNew) );
	return JS_TRUE;
}
static JSBool SMJS_FUNCTION(vec3f_subtract)
{
	SFVec3f *v1, *v2;
	JSObject *pNew;
	SMJS_ARGS
	SMJS_OBJ
	if (argc<=0 || !JSVAL_IS_OBJECT(argv[0]) || !GF_JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[0]), &js_rt->SFVec3fClass, NULL))
		return JS_FALSE;

	v1 = ((GF_JSField *) SMJS_GET_PRIVATE(c, obj))->field.far_ptr;
	v2 = ((GF_JSField *) SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(argv[0])))->field.far_ptr;
	pNew = JS_NewObject(c, &js_rt->SFVec3fClass._class, 0, SMJS_GET_PARENT(c, obj));
	SFVec3f_Create(c, pNew, v1->x - v2->x, v1->y - v2->y, v1->z - v2->z);
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(pNew) );
	return JS_TRUE;
}
static JSBool SMJS_FUNCTION(vec3f_negate)
{
	SFVec3f *v1;
	JSObject *pNew;
	SMJS_OBJ
	v1 = ((GF_JSField *) SMJS_GET_PRIVATE(c, obj))->field.far_ptr;
	pNew = JS_NewObject(c, &js_rt->SFVec3fClass._class, 0, SMJS_GET_PARENT(c, obj));
	SFVec3f_Create(c, pNew, -v1->x , -v1->y , -v1->z );
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(pNew) );
	return JS_TRUE;
}
static JSBool SMJS_FUNCTION(vec3f_multiply)
{
	SFVec3f *v1;
	JSObject *pNew;
	jsdouble d;
	Fixed v;
	SMJS_OBJ
	SMJS_ARGS
	if (argc<=0) return JS_FALSE;

	v1 = ((GF_JSField *) SMJS_GET_PRIVATE(c, obj))->field.far_ptr;
	pNew = JS_NewObject(c, &js_rt->SFVec3fClass._class, 0, SMJS_GET_PARENT(c, obj));
	SMJS_GET_NUMBER(argv[0], d );
	v = FLT2FIX(d);
	SFVec3f_Create(c, pNew, gf_mulfix(v1->x, v), gf_mulfix(v1->y, v), gf_mulfix(v1->z, v) );
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(pNew) );
	return JS_TRUE;
}
static JSBool SMJS_FUNCTION(vec3f_divide)
{
	SFVec3f *v1;
	JSObject *pNew;
	jsdouble d;
	Fixed v;
	SMJS_OBJ
	SMJS_ARGS
	if (argc<=0) return JS_FALSE;
	v1 = ((GF_JSField *) SMJS_GET_PRIVATE(c, obj))->field.far_ptr;
	pNew = JS_NewObject(c, &js_rt->SFVec3fClass._class, 0, SMJS_GET_PARENT(c, obj));
	SMJS_GET_NUMBER(argv[0], d );
	v = FLT2FIX(d);
	SFVec3f_Create(c, pNew, gf_divfix(v1->x, v), gf_divfix(v1->y, v), gf_divfix(v1->z, v));
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(pNew) );
	return JS_TRUE;
}
static JSBool SMJS_FUNCTION(vec3f_length)
{
	Fixed res;
	SFVec3f *v1;
	SMJS_OBJ
	v1 = ((GF_JSField *) SMJS_GET_PRIVATE(c, obj))->field.far_ptr;
	res = gf_vec_len(*v1);
	SMJS_SET_RVAL( JS_MAKE_DOUBLE(c, FIX2FLT(res) ) );
	return JS_TRUE;
}
static JSBool SMJS_FUNCTION(vec3f_normalize)
{
	SFVec3f v1;
	JSObject *pNew;
	SMJS_OBJ
	v1 = * (SFVec3f *) ((GF_JSField *) SMJS_GET_PRIVATE(c, obj))->field.far_ptr;
	gf_vec_norm(&v1);
	pNew = JS_NewObject(c, &js_rt->SFVec3fClass._class, 0, SMJS_GET_PARENT(c, obj));
	SFVec3f_Create(c, pNew, v1.x, v1.y, v1.z);
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(pNew) );
	return JS_TRUE;
}
static JSBool SMJS_FUNCTION(vec3f_dot)
{
	SFVec3f v1, v2;
	SMJS_OBJ
	SMJS_ARGS
	if (argc<=0 || !JSVAL_IS_OBJECT(argv[0]) || !GF_JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[0]), &js_rt->SFVec3fClass, NULL))
		return JS_FALSE;

	v1 = *(SFVec3f *) ((GF_JSField *) SMJS_GET_PRIVATE(c, obj))->field.far_ptr;
	v2 = *(SFVec3f *) ((GF_JSField *) SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(argv[0])))->field.far_ptr;
	SMJS_SET_RVAL( JS_MAKE_DOUBLE(c, FIX2FLT(gf_vec_dot(v1, v2)) ) );
	return JS_TRUE;
}
static JSBool SMJS_FUNCTION(vec3f_cross)
{
	SFVec3f v1, v2, v3;
	JSObject *pNew;
	SMJS_ARGS
	SMJS_OBJ
	if (argc<=0 || !JSVAL_IS_OBJECT(argv[0]) || !GF_JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[0]), &js_rt->SFVec3fClass, NULL))
		return JS_FALSE;

	v1 = * (SFVec3f *) ((GF_JSField *) SMJS_GET_PRIVATE(c, obj))->field.far_ptr;
	v2 = * (SFVec3f *) ((GF_JSField *) SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(argv[0])))->field.far_ptr;
	pNew = JS_NewObject(c, &js_rt->SFVec3fClass._class, 0, SMJS_GET_PARENT(c, obj));
	v3 = gf_vec_cross(v1, v2);
	SFVec3f_Create(c, pNew, v3.x, v3.y, v3.z);
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(pNew) );
	return JS_TRUE;
}



/*SFRotation class*/
static GFINLINE GF_JSField *SFRotation_Create(JSContext *c, JSObject *obj, Fixed x, Fixed y, Fixed z, Fixed q)
{
	GF_JSField *field;
	SFRotation *v;
	field = NewJSField(c);
	v = gf_sg_vrml_field_pointer_new(GF_SG_VRML_SFROTATION);
	field->field_ptr = field->field.far_ptr = v;
	field->field.fieldType = GF_SG_VRML_SFROTATION;
	v->x = x;
	v->y = y;
	v->z = z;
	v->q = q;
	SMJS_SET_PRIVATE(c, obj, field);
	return field;
}
static JSBool SMJS_FUNCTION(SFRotationConstructor)
{
	JSObject *an_obj;
	SFVec3f v1, v2;
	Fixed l1, l2, dot;
	SMJS_ARGS
	jsdouble x = 0.0, y = 0.0, z = 0.0, a = 0.0;
	SMJS_OBJ_CONSTRUCTOR(&js_rt->SFRotationClass)

	if (argc == 0) {
		SFRotation_Create(c, obj, FLT2FIX(x), FLT2FIX(y), FIX_ONE, FLT2FIX(a) );
		return JS_TRUE;
	}
	if ((argc>0) && JSVAL_IS_NUMBER(argv[0])) {
		if (argc > 0) SMJS_GET_NUMBER(argv[0], x);
		if (argc > 1) SMJS_GET_NUMBER(argv[1], y);
		if (argc > 2) SMJS_GET_NUMBER(argv[2], z);
		if (argc > 3) SMJS_GET_NUMBER(argv[3], a);
		SFRotation_Create(c, obj, FLT2FIX(x), FLT2FIX(y), FLT2FIX(z), FLT2FIX(a));
		return JS_TRUE;
	}
	if (argc!=2) return JS_FALSE;
	if (!JSVAL_IS_OBJECT(argv[0])) return JS_FALSE;
	an_obj = JSVAL_TO_OBJECT(argv[0]);
	if (! GF_JS_InstanceOf(c, an_obj, &js_rt->SFVec3fClass, NULL)) return JS_FALSE;
	v1 = * (SFVec3f *) ((GF_JSField *) SMJS_GET_PRIVATE(c, an_obj))->field.far_ptr;
	if (JSVAL_IS_DOUBLE(argv[1])) {
		SMJS_GET_NUMBER(argv[1], a);
		SFRotation_Create(c, obj, v1.x, v1.y, v1.z, FLT2FIX(a));
		return JS_TRUE;
	}

	if (!JSVAL_IS_OBJECT(argv[1])) return JS_FALSE;
	an_obj = JSVAL_TO_OBJECT(argv[1]);
	if (!GF_JS_InstanceOf(c, an_obj, &js_rt->SFVec3fClass, NULL)) return JS_FALSE;
	v2 = * (SFVec3f *) ((GF_JSField *) SMJS_GET_PRIVATE(c, an_obj))->field.far_ptr;
	l1 = gf_vec_len(v1);
	l2 = gf_vec_len(v2);
	dot = gf_divfix(gf_vec_dot(v1, v2), gf_mulfix(l1, l2) );
	a = gf_atan2(gf_sqrt(FIX_ONE - gf_mulfix(dot, dot)), dot);
	SFRotation_Create(c, obj, gf_mulfix(v1.y, v2.z) - gf_mulfix(v2.y, v1.z),
	                  gf_mulfix(v1.z, v2.x) - gf_mulfix(v2.z, v1.x),
	                  gf_mulfix(v1.x, v2.y) - gf_mulfix(v2.x, v1.y),
	                  FLT2FIX(a));
	return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(rot_getProperty)

GF_JSField *val = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);
if (SMJS_ID_IS_INT(id)) {
	switch (SMJS_ID_TO_INT(id)) {
	case 0:
		*vp = JS_MAKE_DOUBLE(c, FIX2FLT( ((SFRotation*)val->field.far_ptr)->x));
		break;
	case 1:
		*vp = JS_MAKE_DOUBLE(c, FIX2FLT( ((SFRotation*)val->field.far_ptr)->y));
		break;
	case 2:
		*vp = JS_MAKE_DOUBLE(c, FIX2FLT( ((SFRotation*)val->field.far_ptr)->z));
		break;
	case 3:
		*vp = JS_MAKE_DOUBLE(c, FIX2FLT( ((SFRotation*)val->field.far_ptr)->q));
		break;
	default:
		return JS_TRUE;
	}
}
return JS_TRUE;
}
static SMJS_FUNC_PROP_SET( rot_setProperty )

jsdouble d;
Fixed v;
Bool changed = 0;
GF_JSField *ptr = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);

/*this is the prototype*/
if (!ptr) {
	if (! SMJS_ID_IS_STRING(id)) return JS_FALSE;
	return JS_TRUE;
}

if (! JSVAL_IS_NUMBER(*vp)) {
	GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[VRML JS] Value is not a number while assigning SFVec3f\n"));
	return JS_FALSE;
}
if (SMJS_ID_IS_INT(id) && SMJS_ID_TO_INT(id) >= 0 && SMJS_ID_TO_INT(id) < 4 ) {
	SMJS_GET_NUMBER(*vp, d);

	switch (SMJS_ID_TO_INT(id)) {
	case 0:
		v = FLT2FIX(d);
		changed = ! ( ((SFRotation*)ptr->field.far_ptr)->x == v);
		((SFRotation*)ptr->field.far_ptr)->x = v;
		break;
	case 1:
		v = FLT2FIX(d);
		changed = ! ( ((SFRotation*)ptr->field.far_ptr)->y == v);
		((SFRotation*)ptr->field.far_ptr)->y = v;
		break;
	case 2:
		v = FLT2FIX(d);
		changed = ! ( ((SFRotation*)ptr->field.far_ptr)->z == v);
		((SFRotation*)ptr->field.far_ptr)->z = v;
		break;
	case 3:
		v = FLT2FIX(d);
		changed = ! ( ((SFRotation*)ptr->field.far_ptr)->q == v);
		((SFRotation*)ptr->field.far_ptr)->q = v;
		break;
	default:
		return JS_TRUE;
	}
	if (changed) Script_FieldChanged(c, NULL, ptr, NULL);
	return JS_TRUE;
}
return JS_FALSE;
}
static JSBool SMJS_FUNCTION(rot_getAxis)
{
	SFRotation r;
	JSObject *pNew;
	SMJS_OBJ
	r = * (SFRotation *) ((GF_JSField *) SMJS_GET_PRIVATE(c, obj))->field.far_ptr;
	pNew = JS_NewObject(c, &js_rt->SFVec3fClass._class, 0, SMJS_GET_PARENT(c, obj));
	SFVec3f_Create(c, pNew, r.x, r.y, r.z);
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(pNew) );
	return JS_TRUE;
}
static JSBool SMJS_FUNCTION(rot_inverse)
{
	SFRotation r;
	JSObject *pNew;
	SMJS_OBJ
	r = * (SFRotation *) ((GF_JSField *) SMJS_GET_PRIVATE(c, obj))->field.far_ptr;
	pNew = JS_NewObject(c, &js_rt->SFRotationClass._class, 0, SMJS_GET_PARENT(c, obj));
	SFRotation_Create(c, pNew, r.x, r.y, r.z, r.q-GF_PI);
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(pNew) );
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(rot_multiply)
{
	SFRotation r1, r2;
	SFVec4f q1, q2;
	JSObject *pNew;
	SMJS_OBJ
	SMJS_ARGS

	if (argc<=0 || !JSVAL_IS_OBJECT(argv[0]) || !GF_JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[0]), &js_rt->SFRotationClass, NULL))
		return JS_FALSE;

	r1 = * (SFRotation *) ((GF_JSField *) SMJS_GET_PRIVATE(c, obj))->field.far_ptr;
	r2 = * (SFRotation *) ((GF_JSField *) SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(argv[0])))->field.far_ptr;
	q1 = gf_quat_from_rotation(r1);
	q2 = gf_quat_from_rotation(r2);
	q1 = gf_quat_multiply(&q1, &q2);
	r1 = gf_quat_to_rotation(&q1);

	pNew = JS_NewObject(c, &js_rt->SFRotationClass._class, 0, SMJS_GET_PARENT(c, obj));
	SFRotation_Create(c, pNew, r1.x, r1.y, r1.z, r1.q);
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(pNew) );
	return JS_TRUE;
}
static JSBool SMJS_FUNCTION(rot_multVec)
{
	SFVec3f v;
	SFRotation r;
	GF_Matrix mx;
	JSObject *pNew;
	SMJS_OBJ
	SMJS_ARGS
	if (argc<=0) return JS_FALSE;

	if (argc<=0 || !JSVAL_IS_OBJECT(argv[0]) || !GF_JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[0]), &js_rt->SFVec3fClass, NULL))
		return JS_FALSE;

	r = *(SFRotation *) ((GF_JSField *) SMJS_GET_PRIVATE(c, obj))->field.far_ptr;
	v = *(SFVec3f *) ((GF_JSField *) SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(argv[0])))->field.far_ptr;
	gf_mx_init(mx);
	gf_mx_add_rotation(&mx, r.q, r.x, r.y, r.z);
	gf_mx_apply_vec(&mx, &v);
	pNew = JS_NewObject(c, &js_rt->SFVec3fClass._class, 0, SMJS_GET_PARENT(c, obj));
	SFVec3f_Create(c, pNew, v.x, v.y, v.z);
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(pNew) );
	return JS_TRUE;
}
static JSBool SMJS_FUNCTION(rot_setAxis)
{
	SFVec3f v;
	SFRotation *r;
	GF_JSField *ptr;
	SMJS_OBJ
	SMJS_ARGS
	if (argc<=0) return JS_FALSE;

	if (argc<=0 || !JSVAL_IS_OBJECT(argv[0]) || !GF_JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[0]), &js_rt->SFVec3fClass, NULL))
		return JS_FALSE;

	ptr = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);
	r = (SFRotation *) ptr->field.far_ptr;

	v = *(SFVec3f *) ((GF_JSField *) SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(argv[0])))->field.far_ptr;

	r->x = v.x;
	r->y = v.y;
	r->z = v.z;
	Script_FieldChanged(c, NULL, ptr, NULL);
	return JS_TRUE;
}
static JSBool SMJS_FUNCTION(rot_slerp)
{
	SFRotation v1, v2, res;
	SFVec4f q1, q2;
	JSObject *pNew;
	jsdouble d;
	SMJS_ARGS
	SMJS_OBJ
	if (argc<=1) return JS_FALSE;

	if (!JSVAL_IS_DOUBLE(argv[1]) || !JSVAL_IS_OBJECT(argv[0]) || !GF_JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[0]), &js_rt->SFRotationClass, NULL)) return JS_FALSE;

	v1 = *(SFRotation *) ((GF_JSField *) SMJS_GET_PRIVATE(c, obj))->field.far_ptr;
	v2 = *(SFRotation *) ((GF_JSField *) SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(argv[0])))->field.far_ptr;
	SMJS_GET_NUMBER(argv[1], d );
	q1 = gf_quat_from_rotation(v1);
	q2 = gf_quat_from_rotation(v2);
	q1 = gf_quat_slerp(q1, q2, FLT2FIX( d));
	res = gf_quat_to_rotation(&q1);
	pNew = JS_NewObject(c, &js_rt->SFRotationClass._class, 0, SMJS_GET_PARENT(c, obj));
	SFRotation_Create(c, pNew, res.x, res.y, res.z, res.q);
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(pNew) );
	return JS_TRUE;
}

/* SFColor class functions */
static GFINLINE GF_JSField *SFColor_Create(JSContext *c, JSObject *obj, Fixed r, Fixed g, Fixed b)
{
	GF_JSField *field;
	SFColor *v;
	field = NewJSField(c);
	v = gf_sg_vrml_field_pointer_new(GF_SG_VRML_SFCOLOR);
	field->field_ptr = field->field.far_ptr = v;
	field->field.fieldType = GF_SG_VRML_SFCOLOR;
	v->red = r;
	v->green = g;
	v->blue = b;
	SMJS_SET_PRIVATE(c, obj, field);
	return field;
}
static JSBool SMJS_FUNCTION(SFColorConstructor)
{
	SMJS_ARGS
	jsdouble r = 0.0, g = 0.0, b = 0.0;
	SMJS_OBJ_CONSTRUCTOR(&js_rt->SFColorClass)
	if (argc > 0) SMJS_GET_NUMBER(argv[0], r);
	if (argc > 1) SMJS_GET_NUMBER(argv[1], g);
	if (argc > 2) SMJS_GET_NUMBER(argv[2], b);
	SFColor_Create(c, obj, FLT2FIX( r), FLT2FIX( g), FLT2FIX( b));
	return JS_TRUE;
}
static SMJS_FUNC_PROP_GET( color_getProperty )

GF_JSField *val = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);
if (SMJS_ID_IS_INT(id)) {
	switch (SMJS_ID_TO_INT(id)) {
	case 0:
		*vp = JS_MAKE_DOUBLE(c, FIX2FLT( ((SFColor*)val->field.far_ptr)->red));
		break;
	case 1:
		*vp = JS_MAKE_DOUBLE(c, FIX2FLT( ((SFColor*)val->field.far_ptr)->green));
		break;
	case 2:
		*vp = JS_MAKE_DOUBLE(c, FIX2FLT( ((SFColor*)val->field.far_ptr)->blue));
		break;
	default:
		return JS_TRUE;
	}
}
return JS_TRUE;
}

static SMJS_FUNC_PROP_SET(color_setProperty)

jsdouble d;
Fixed v;
Bool changed = 0;
GF_JSField *ptr = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);
/*this is the prototype*/
if (!ptr) {
	if (! SMJS_ID_IS_STRING(id)) return JS_FALSE;
	return JS_TRUE;
}

if (! JSVAL_IS_NUMBER(*vp)) {
	GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[VRML JS] Value is not a number while assigning SFVec3f\n"));
	return JS_FALSE;
}
if (SMJS_ID_IS_INT(id) && SMJS_ID_TO_INT(id) >= 0 && SMJS_ID_TO_INT(id) < 3 ) {
	SMJS_GET_NUMBER(*vp, d);
	switch (SMJS_ID_TO_INT(id)) {
	case 0:
		v = FLT2FIX(d);
		changed = ! ( ((SFColor*)ptr->field.far_ptr)->red == v);
		((SFColor*)ptr->field.far_ptr)->red = v;
		break;
	case 1:
		v = FLT2FIX(d);
		changed = ! ( ((SFColor*)ptr->field.far_ptr)->green == v);
		((SFColor*)ptr->field.far_ptr)->green = v;
		break;
	case 2:
		v = FLT2FIX(d);
		changed = ! ( ((SFColor*)ptr->field.far_ptr)->blue == v);
		((SFColor*)ptr->field.far_ptr)->blue = v;
		break;
	default:
		return JS_TRUE;
	}
	if (changed) Script_FieldChanged(c, NULL, ptr, NULL);
	return JS_TRUE;
}
return JS_FALSE;
}
static JSBool SMJS_FUNCTION(color_setHSV)
{
	SFColor *v1, hsv;
	jsdouble h, s, v;
	SMJS_OBJ
	SMJS_ARGS
	GF_JSField *ptr = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);
	if (argc != 3) return JS_FALSE;
	v1 = ((GF_JSField *) SMJS_GET_PRIVATE(c, obj))->field.far_ptr;
	SMJS_GET_NUMBER(argv[0], h);
	SMJS_GET_NUMBER(argv[1], s);
	SMJS_GET_NUMBER(argv[2], v);
	hsv.red = FLT2FIX( h);
	hsv.green = FLT2FIX( s);
	hsv.blue = FLT2FIX( v);
	SFColor_fromHSV(&hsv);
	gf_sg_vrml_field_copy(v1, &hsv, GF_SG_VRML_SFCOLOR);
	Script_FieldChanged(c, NULL, ptr, NULL);
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(color_getHSV)
{
	SFColor *v1, hsv;
	jsval vec[3];
	JSObject *arr;
	SMJS_OBJ
	v1 = ((GF_JSField *) SMJS_GET_PRIVATE(c, obj))->field.far_ptr;
	hsv = *v1;
	SFColor_toHSV(&hsv);
	vec[0] = JS_MAKE_DOUBLE(c, FIX2FLT(hsv.red));
	vec[1] = JS_MAKE_DOUBLE(c, FIX2FLT(hsv.green));
	vec[2] = JS_MAKE_DOUBLE(c, FIX2FLT(hsv.blue));
	arr = JS_NewArrayObject(c, 3, vec);
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(arr) );
	return JS_TRUE;
}

static void setup_js_array(JSContext *c, JSObject *obj, GF_JSField *ptr, uintN argc, jsval *argv)
{
//	GF_ScriptPriv *priv = JS_GetScriptStack(c);
	ptr->obj = obj;
	ptr->js_list = JS_NewArrayObject(c, (jsint) argc, argv);

	/*
		gf_js_add_root(c, &ptr->js_list, GF_JSGC_OBJECT);
		ptr->is_rooted = 1;
		gf_list_add(priv->js_cache, obj);
	*/
}

#define MFARRAY_CONSTRUCTOR(__classp, _fieldType)	\
	GF_JSField *ptr;	\
	SMJS_ARGS	\
	SMJS_OBJ_CONSTRUCTOR(__classp)	\
	ptr = NewJSField(c);	\
	ptr->field.fieldType = _fieldType;	\
	setup_js_array(c, obj, ptr, (jsint) argc, argv);	\
	SMJS_SET_PRIVATE(c, obj, ptr);	\
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(obj) );	\
	return obj == 0 ? JS_FALSE : JS_TRUE;	\
 
static JSBool SMJS_FUNCTION(MFBoolConstructor)
{
	MFARRAY_CONSTRUCTOR(&js_rt->MFBoolClass, GF_SG_VRML_MFBOOL);
}
static JSBool SMJS_FUNCTION(MFInt32Constructor)
{
	MFARRAY_CONSTRUCTOR(&js_rt->MFInt32Class, GF_SG_VRML_MFINT32);
}
static JSBool SMJS_FUNCTION(MFFloatConstructor)
{
	MFARRAY_CONSTRUCTOR(&js_rt->MFFloatClass, GF_SG_VRML_MFFLOAT);
}
static JSBool SMJS_FUNCTION(MFTimeConstructor)
{
	MFARRAY_CONSTRUCTOR(&js_rt->MFTimeClass, GF_SG_VRML_MFTIME);
}
static JSBool SMJS_FUNCTION(MFStringConstructor)
{
	MFARRAY_CONSTRUCTOR(&js_rt->MFStringClass, GF_SG_VRML_MFSTRING);
}
static JSBool SMJS_FUNCTION(MFURLConstructor)
{
	MFARRAY_CONSTRUCTOR(&js_rt->MFUrlClass, GF_SG_VRML_MFURL);
}
static JSBool SMJS_FUNCTION(MFNodeConstructor)
{
	MFARRAY_CONSTRUCTOR(&js_rt->MFNodeClass, GF_SG_VRML_MFNODE);
}


static void array_finalize_ex(JSContext *c, JSObject *obj, Bool is_js_call)
{
	GF_JSField *ptr = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);

#ifndef USE_FFDEV_15
	JS_ObjectDestroyed(c, obj, ptr, 1);
#endif

	if (!ptr) return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[VRML JS] unregistering MFField %s\n", ptr->field.name));

	if (ptr->js_list && ptr->is_rooted) {
		gf_js_remove_root(c, &ptr->js_list, GF_JSGC_OBJECT);
	}

	/*MFNode*/
	if (ptr->temp_list) {
		gf_node_unregister_children(ptr->owner, ptr->temp_list);
	}
	if (ptr->field_ptr) {
		gf_sg_vrml_field_pointer_del(ptr->field_ptr, ptr->field.fieldType);
	}
	gf_free(ptr);
}

static DECL_FINALIZE(array_finalize)

array_finalize_ex(c, obj, 1);
}

static SMJS_FUNC_PROP_GET( array_getElement )

u32 i;
GF_JSField *ptr = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);
if (SMJS_ID_IS_INT(id)) {
	i = SMJS_ID_TO_INT(id);
	if (ptr->field.fieldType==GF_SG_VRML_MFNODE) {
		GF_Node *node = gf_node_list_get_child(*(GF_ChildNodeItem **)ptr->field.far_ptr, i);
		JSObject *anobj = node ? node_get_binding(JS_GetScriptStack(c), node, 0) : NULL;
		if (anobj) *vp = OBJECT_TO_JSVAL(anobj);
	} else {
		JS_GetElement(c, ptr->js_list, (jsint) i, vp);
	}
}
return JS_TRUE;
}


//this could be overloaded for each MF type...
static SMJS_FUNC_PROP_SET(array_setElement)

u32 ind;
jsuint len;
jsdouble d;
GF_JSField *from;
JSBool ret;
GF_JSClass *the_sf_class = NULL;
JSString *str;
char *str_val;
void *sf_slot;
Bool is_append = 0;
GF_JSField *ptr = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);
ind = SMJS_ID_TO_INT(id);

ret = JS_GetArrayLength(c, ptr->js_list, &len);
if (ret==JS_FALSE) return JS_FALSE;

if (!ptr->js_list && gf_sg_vrml_is_sf_field(ptr->field.fieldType)) return JS_FALSE;


switch (ptr->field.fieldType) {
case GF_SG_VRML_MFVEC2F:
	the_sf_class = &js_rt->SFVec2fClass;
	break;
case GF_SG_VRML_MFVEC3F:
	the_sf_class = &js_rt->SFVec3fClass;
	break;
case GF_SG_VRML_MFCOLOR:
	the_sf_class = &js_rt->SFColorClass;
	break;
case GF_SG_VRML_MFROTATION:
	the_sf_class = &js_rt->SFRotationClass;
	break;
}
/*dynamic expend*/
if (ind>=len) {
	is_append = 1;
	ret = JS_SetArrayLength(c, ptr->js_list, len+1);
	if (ret==JS_FALSE) return JS_FALSE;
	while (len<ind) {
		jsval a_val;
		switch (ptr->field.fieldType) {
		case GF_SG_VRML_MFBOOL:
			a_val = BOOLEAN_TO_JSVAL(0);
			break;
		case GF_SG_VRML_MFINT32:
			a_val = INT_TO_JSVAL(0);
			break;
		case GF_SG_VRML_MFFLOAT:
		case GF_SG_VRML_MFTIME:
			a_val = JS_MAKE_DOUBLE(c, 0);
			break;
		case GF_SG_VRML_MFSTRING:
		case GF_SG_VRML_MFURL:
			a_val = STRING_TO_JSVAL( JS_NewStringCopyZ(c, "") );
			break;
		case GF_SG_VRML_MFVEC2F:
		case GF_SG_VRML_MFVEC3F:
		case GF_SG_VRML_MFCOLOR:
		case GF_SG_VRML_MFROTATION:
			a_val = OBJECT_TO_JSVAL( SMJS_CONSTRUCT_OBJECT(c, the_sf_class, obj) );
			break;
		default:
			a_val = INT_TO_JSVAL(0);
			break;
		}

		if (ptr->field.fieldType!=GF_SG_VRML_MFNODE) {
			gf_sg_vrml_mf_insert(ptr->field.far_ptr, ptr->field.fieldType, &sf_slot, len);
			JS_SetElement(c, ptr->js_list, len, &a_val);
		}
		len++;
	}
	if (ptr->field.far_ptr && (ptr->field.fieldType!=GF_SG_VRML_MFNODE))
		gf_sg_vrml_mf_insert(ptr->field.far_ptr, ptr->field.fieldType, &sf_slot, ind);
}

if (ptr->field.far_ptr && (ptr->field.fieldType!=GF_SG_VRML_MFNODE)) {
	u32 items = ((GenMFField *)ptr->field.far_ptr)->count;
	while (ind>=items) {
		gf_sg_vrml_mf_insert(ptr->field.far_ptr, ptr->field.fieldType, &sf_slot, ind);
		items++;
	}
}

/*assign object*/
if (ptr->field.fieldType==GF_SG_VRML_MFNODE) {
	JSObject *o;
	if (JSVAL_IS_VOID(*vp)) return JS_FALSE;
	if (JSVAL_IS_NULL(*vp) ) return JS_FALSE;
	o = JSVAL_TO_OBJECT(*vp);
	if (!GF_JS_InstanceOf(c, o, &js_rt->SFNodeClass, NULL) ) return JS_FALSE;
} else if (the_sf_class) {
	if (JSVAL_IS_VOID(*vp)) return JS_FALSE;
	if (!GF_JS_InstanceOf(c, JSVAL_TO_OBJECT(*vp), the_sf_class, NULL) ) return JS_FALSE;
} else if (ptr->field.fieldType==GF_SG_VRML_MFBOOL) {
	if (!JSVAL_IS_BOOLEAN(*vp)) return JS_FALSE;
} else if (ptr->field.fieldType==GF_SG_VRML_MFINT32) {
	if (!JSVAL_IS_INT(*vp)) return JS_FALSE;
} else if (ptr->field.fieldType==GF_SG_VRML_MFFLOAT) {
	if (!JSVAL_IS_NUMBER(*vp)) return JS_FALSE;
} else if (ptr->field.fieldType==GF_SG_VRML_MFTIME) {
	if (!JSVAL_IS_NUMBER(*vp)) return JS_FALSE;
} else if (ptr->field.fieldType==GF_SG_VRML_MFSTRING) {
	if (!JSVAL_IS_STRING(*vp)) return JS_FALSE;
} else if (ptr->field.fieldType==GF_SG_VRML_MFURL) {
	if (!JSVAL_IS_STRING(*vp)) return JS_FALSE;
}


/*rewrite MFNode entry*/
if (ptr->field.fieldType==GF_SG_VRML_MFNODE) {
	GF_Node *prev_n, *new_n;

	if (!ptr->owner) return JS_TRUE;

	/*get new node*/
	from = (GF_JSField *) SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(*vp));
	new_n = *(GF_Node**)from->field.far_ptr;
	prev_n = NULL;

	if (!is_append) {
		/*get and delete previous node if any, but unregister later*/
		prev_n = gf_node_list_del_child_idx( (GF_ChildNodeItem **)ptr->field.far_ptr, ind);
	}

	if (new_n) {
		gf_node_list_insert_child( (GF_ChildNodeItem **)ptr->field.far_ptr , new_n, ind);
		gf_node_register(new_n, ptr->owner);

		/*node created from script and inserted in the tree, root it*/
		if (!from->is_rooted)
			node_get_binding(JS_GetScriptStack(c), new_n, 0);
	}
	/*unregister previous node*/
	if (prev_n) gf_node_unregister(prev_n, ptr->owner);

	Script_FieldChanged(c, NULL, ptr, NULL);
	return JS_TRUE;
}

ret = JS_SetElement(c, ptr->js_list, ind, vp);
if (ret==JS_FALSE) return JS_FALSE;

if (!ptr->owner) return JS_TRUE;
if (!ptr->field.far_ptr) return JS_FALSE;

/*rewrite MF slot*/
switch (ptr->field.fieldType) {
case GF_SG_VRML_MFBOOL:
	((MFBool *)ptr->field.far_ptr)->vals[ind] = (Bool) JSVAL_TO_BOOLEAN(*vp);
	break;
case GF_SG_VRML_MFINT32:
	((MFInt32 *)ptr->field.far_ptr)->vals[ind] = (s32) JSVAL_TO_INT(*vp);
	break;
case GF_SG_VRML_MFFLOAT:
	SMJS_GET_NUMBER(*vp, d);
	((MFFloat *)ptr->field.far_ptr)->vals[ind] = FLT2FIX(d);
	break;
case GF_SG_VRML_MFTIME:
	SMJS_GET_NUMBER(*vp, d);
	((MFTime *)ptr->field.far_ptr)->vals[ind] = d;
	break;
case GF_SG_VRML_MFSTRING:
	if (((MFString *)ptr->field.far_ptr)->vals[ind]) {
		gf_free(((MFString *)ptr->field.far_ptr)->vals[ind]);
		((MFString *)ptr->field.far_ptr)->vals[ind] = NULL;
	}
	str = JSVAL_IS_STRING(*vp) ? JSVAL_TO_STRING(*vp) : JS_ValueToString(c, *vp);
	str_val = SMJS_CHARS_FROM_STRING(c, str);
	((MFString *)ptr->field.far_ptr)->vals[ind] = gf_strdup(str_val);
	SMJS_FREE(c, str_val);
	break;

case GF_SG_VRML_MFURL:
	if (((MFURL *)ptr->field.far_ptr)->vals[ind].url) {
		gf_free(((MFURL *)ptr->field.far_ptr)->vals[ind].url);
		((MFURL *)ptr->field.far_ptr)->vals[ind].url = NULL;
	}
	str = JSVAL_IS_STRING(*vp) ? JSVAL_TO_STRING(*vp) : JS_ValueToString(c, *vp);
	str_val = SMJS_CHARS_FROM_STRING(c, str);
	((MFURL *)ptr->field.far_ptr)->vals[ind].url = gf_strdup(str_val);
	((MFURL *)ptr->field.far_ptr)->vals[ind].OD_ID = 0;
	SMJS_FREE(c, str_val);
	break;

case GF_SG_VRML_MFVEC2F:
	from = (GF_JSField *) SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(*vp));
	gf_sg_vrml_field_copy(& ((MFVec2f *)ptr->field.far_ptr)->vals[ind], from->field.far_ptr, from->field.fieldType);
	break;
case GF_SG_VRML_MFVEC3F:
	from = (GF_JSField *) SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(*vp));
	gf_sg_vrml_field_copy(& ((MFVec3f *)ptr->field.far_ptr)->vals[ind], from->field.far_ptr, from->field.fieldType);
	break;
case GF_SG_VRML_MFROTATION:
	from = (GF_JSField *) SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(*vp));
	gf_sg_vrml_field_copy(& ((MFRotation *)ptr->field.far_ptr)->vals[ind], from->field.far_ptr, from->field.fieldType);
	break;
case GF_SG_VRML_MFCOLOR:
	from = (GF_JSField *) SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(*vp));
	gf_sg_vrml_field_copy(& ((MFColor *)ptr->field.far_ptr)->vals[ind], from->field.far_ptr, from->field.fieldType);
	break;
}

Script_FieldChanged(c, NULL, ptr, NULL);
return JS_TRUE;
}

static SMJS_FUNC_PROP_SET( array_setLength)

u32 len, i, sftype, old_len;
JSBool ret;
GF_JSClass *the_sf_class;
GF_JSField *ptr = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);
if (!JSVAL_IS_INT(*vp) || JSVAL_TO_INT(*vp) < 0) return JS_FALSE;
/*avoids gcc warning*/
#ifndef GPAC_CONFIG_DARWIN
if (!id) id=0;
#endif
len = JSVAL_TO_INT(*vp);


if (!len) {
	if (ptr->field.fieldType==GF_SG_VRML_MFNODE) {
		gf_node_unregister_children(ptr->owner, *(GF_ChildNodeItem**)ptr->field.far_ptr);
		*(GF_ChildNodeItem**)ptr->field.far_ptr = NULL;
	} else {
		gf_sg_vrml_mf_reset(ptr->field.far_ptr, ptr->field.fieldType);
	}
	JS_SetArrayLength(c, ptr->js_list, 0);
	Script_FieldChanged(c, NULL, ptr, NULL);
	return JS_TRUE;
}

ret = JS_GetArrayLength(c, ptr->js_list, &old_len);
if (ret==JS_FALSE) return ret;

ret = JS_SetArrayLength(c, ptr->js_list, len);
if (ret==JS_FALSE) return ret;

the_sf_class = NULL;
switch (ptr->field.fieldType) {
case GF_SG_VRML_MFVEC2F:
	the_sf_class = &js_rt->SFVec2fClass;
	break;
case GF_SG_VRML_MFVEC3F:
	the_sf_class = &js_rt->SFVec3fClass;
	break;
case GF_SG_VRML_MFCOLOR:
	the_sf_class = &js_rt->SFColorClass;
	break;
case GF_SG_VRML_MFROTATION:
	the_sf_class = &js_rt->SFRotationClass;
	break;
case GF_SG_VRML_MFNODE:
{
	u32 c = gf_node_list_get_count(*(GF_ChildNodeItem**)ptr->field.far_ptr);
	while (len < c) {
		GF_Node *n = gf_node_list_del_child_idx((GF_ChildNodeItem**)ptr->field.far_ptr, c-1);
		if (n) gf_node_unregister(n, ptr->owner);
		c--;
	}
	if (len>c) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[VRML] MFARRAY EXPANSION NOT SUPPORTED!!!\n"));
	}
}
return JS_TRUE;
}

sftype = gf_sg_vrml_get_sf_type(ptr->field.fieldType);
for (i=old_len; i<len; i++) {
	jsval a_val;
	if (the_sf_class) {
		JSObject *an_obj = SMJS_CONSTRUCT_OBJECT(c, the_sf_class, obj);
		a_val = OBJECT_TO_JSVAL(an_obj );
	} else {
		switch (sftype) {
		case GF_SG_VRML_SFBOOL:
			a_val = BOOLEAN_TO_JSVAL(0);
			break;
		case GF_SG_VRML_SFINT32:
			a_val = INT_TO_JSVAL(0);
			break;
		case GF_SG_VRML_SFFLOAT:
		case GF_SG_VRML_SFTIME:
			a_val = JS_MAKE_DOUBLE(c, 0);
			break;
		case GF_SG_VRML_SFSTRING:
		case GF_SG_VRML_SFURL:
			a_val = STRING_TO_JSVAL( JS_NewStringCopyZ(c, "") );
			break;
		default:
			a_val = INT_TO_JSVAL(0);
			break;
		}
	}
	JS_SetElement(c, ptr->js_list, i, &a_val);
}
return JS_TRUE;
}

static SMJS_FUNC_PROP_GET( array_getLength)

JSBool ret;
jsuint len;
GF_JSField *ptr = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);
/*avoids gcc warning*/
#ifndef GPAC_CONFIG_DARWIN
if (!id) id=0;
#endif
if (ptr->field.fieldType==GF_SG_VRML_MFNODE) {
	len = gf_node_list_get_count(*(GF_ChildNodeItem **)ptr->field.far_ptr);
	ret = JS_TRUE;
} else {
	ret = JS_GetArrayLength(c, ptr->js_list, &len);
}
*vp = INT_TO_JSVAL(len);
return ret;
}


/* MFVec2f class constructor */
static JSBool SMJS_FUNCTION(MFVec2fConstructor)
{
	jsval val;
	JSObject *item;
	u32 i;
	SMJS_ARGS
	GF_JSField *ptr = NewJSField(c);
	SMJS_OBJ_CONSTRUCTOR(&js_rt->MFVec2fClass)

	setup_js_array(c, obj, ptr, 0, 0);
	JS_SetArrayLength(c, ptr->js_list, argc);
	SMJS_SET_PRIVATE(c, obj, ptr);

	for (i=0; i<argc; i++) {
		if (!JSVAL_IS_OBJECT(argv[i]) || !GF_JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[i]), &js_rt->SFVec2fClass, NULL) ) {
			item = SMJS_CONSTRUCT_OBJECT(c, &js_rt->SFVec2fClass, obj);
			val = OBJECT_TO_JSVAL(item);
			JS_SetElement(c, ptr->js_list, i, &val);
		} else {
			JS_SetElement(c, ptr->js_list, i, &argv[i]);
		}
	}
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(obj) );
	return obj == 0 ? JS_FALSE : JS_TRUE;
}

/* MFVec3f class constructor */
static JSBool SMJS_FUNCTION(MFVec3fConstructor)
{
	jsval val;
	JSObject *item;
	u32 i;
	SMJS_ARGS
	GF_JSField *ptr;
	SMJS_OBJ_CONSTRUCTOR(&js_rt->MFVec3fClass)

	ptr = NewJSField(c);
	ptr->field.fieldType = GF_SG_VRML_MFVEC3F;
	setup_js_array(c, obj, ptr, 0, 0);
	JS_SetArrayLength(c, ptr->js_list, argc);
	SMJS_SET_PRIVATE(c, obj, ptr);

	for (i=0; i<argc; i++) {
		if (!JSVAL_IS_OBJECT(argv[i]) || !GF_JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[i]), &js_rt->SFVec3fClass, NULL) ) {
			item = SMJS_CONSTRUCT_OBJECT(c, &js_rt->SFVec3fClass, obj);
			val = OBJECT_TO_JSVAL(item);
			JS_SetElement(c, ptr->js_list, i, &val);
		} else {
			JS_SetElement(c, ptr->js_list, i, &argv[i]);
		}
	}
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(obj) );
	return obj == 0 ? JS_FALSE : JS_TRUE;
}

/* MFRotation class constructor */
static JSBool SMJS_FUNCTION(MFRotationConstructor)
{
	jsval val;
	JSObject *item;
	u32 i;
	SMJS_ARGS
	GF_JSField *ptr;
	SMJS_OBJ_CONSTRUCTOR(&js_rt->MFRotationClass)

	ptr = NewJSField(c);
	ptr->field.fieldType = GF_SG_VRML_MFROTATION;
	setup_js_array(c, obj, ptr, 0, 0);
	JS_SetArrayLength(c, ptr->js_list, argc);
	SMJS_SET_PRIVATE(c, obj, ptr);

	for (i=0; i<argc; i++) {
		if (!JSVAL_IS_OBJECT(argv[i]) || !GF_JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[i]), &js_rt->SFRotationClass, NULL) ) {
			item = SMJS_CONSTRUCT_OBJECT(c, &js_rt->SFVec3fClass, obj);
			val = OBJECT_TO_JSVAL(item);
			JS_SetElement(c, ptr->js_list, i, &val);
		} else {
			JS_SetElement(c, ptr->js_list, i, &argv[i]);
		}
	}
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(obj) );
	return obj == 0 ? JS_FALSE : JS_TRUE;
}

/*MFColor class constructor */
static JSBool SMJS_FUNCTION(MFColorConstructor)
{
	jsval val;
	JSObject *item;
	u32 i;
	SMJS_ARGS
	GF_JSField *ptr;
	SMJS_OBJ_CONSTRUCTOR(&js_rt->MFColorClass)

	ptr = NewJSField(c);
	ptr->field.fieldType = GF_SG_VRML_MFCOLOR;
	setup_js_array(c, obj, ptr, 0, 0);
	JS_SetArrayLength(c, ptr->js_list, argc);
	SMJS_SET_PRIVATE(c, obj, ptr);

	for (i=0; i<argc; i++) {
		if (!JSVAL_IS_OBJECT(argv[i]) || !GF_JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[i]), &js_rt->SFColorClass, NULL) ) {
			item = SMJS_CONSTRUCT_OBJECT(c, &js_rt->SFColorClass, obj);
			val = OBJECT_TO_JSVAL(item);
			JS_SetElement(c, ptr->js_list, i, &val);
		} else {
			JS_SetElement(c, ptr->js_list, i, &argv[i]);
		}
	}
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(obj) );
	return obj == 0 ? JS_FALSE : JS_TRUE;
}

#ifndef GPAC_DISABLE_SVG

/*CHECK IF THIS WORKS WITH NEW API*/
JSBool SMJS_FUNCTION_EXT(gf_sg_js_event_add_listener, GF_Node *vrml_node);
JSBool SMJS_FUNCTION_EXT(gf_sg_js_event_remove_listener, GF_Node *vrml_node);

JSBool SMJS_FUNCTION(vrml_event_add_listener)
{
	GF_Node *node;
	GF_JSField *ptr;
	SMJS_OBJ
	if (! GF_JS_InstanceOf(c, obj, &js_rt->SFNodeClass, NULL) ) return JS_FALSE;
	ptr = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);
	assert(ptr->field.fieldType==GF_SG_VRML_SFNODE);
	node = * ((GF_Node **)ptr->field.far_ptr);

	return gf_sg_js_event_add_listener(SMJS_CALL_ARGS, node);
}
JSBool SMJS_FUNCTION(vrml_event_remove_listener)
{
	GF_Node *node;
	GF_JSField *ptr;
	SMJS_OBJ
	if (! GF_JS_InstanceOf(c, obj, &js_rt->SFNodeClass, NULL) ) return JS_FALSE;
	ptr = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);
	assert(ptr->field.fieldType==GF_SG_VRML_SFNODE);
	node = * ((GF_Node **)ptr->field.far_ptr);

	return gf_sg_js_event_remove_listener(SMJS_CALL_ARGS, node);
}
#endif

static JSBool SMJS_FUNCTION(vrml_dom3_not_implemented)
{
	return JS_FALSE;
}


void gf_sg_script_init_sm_api(GF_ScriptPriv *sc, GF_Node *script)
{
	JS_SETUP_CLASS(js_rt->globalClass, "global", JSCLASS_HAS_PRIVATE,
	               JS_PropertyStub, JS_PropertyStub_forSetter, JS_FinalizeStub);


	JS_SETUP_CLASS(js_rt->AnyClass, "AnyClass", JSCLASS_HAS_PRIVATE,
	               JS_PropertyStub, JS_PropertyStub_forSetter, JS_FinalizeStub);

	JS_SETUP_CLASS(js_rt->browserClass , "Browser", 0,
	               JS_PropertyStub, JS_PropertyStub_forSetter, JS_FinalizeStub);

#if 1
	JS_SETUP_CLASS(js_rt->SFNodeClass, "SFNode", JSCLASS_HAS_PRIVATE,
	               node_getProperty, node_setProperty, node_finalize);
#else
	/*only used to debug JS_SETUP_CLASS at each of the numerous changes of JSAPI ............ */
	memset(&js_rt->SFNodeClass, 0, sizeof(js_rt->SFNodeClass));
	js_rt->SFNodeClass._class.name = "SFNode";
	js_rt->SFNodeClass._class.flags = JSCLASS_HAS_PRIVATE;
	js_rt->SFNodeClass._class.addProperty = JS_PropertyStub;
	js_rt->SFNodeClass._class.delProperty = JS_PropertyStub;
	js_rt->SFNodeClass._class.getProperty = node_getProperty;
	js_rt->SFNodeClass._class.setProperty = node_setProperty;
	js_rt->SFNodeClass._class.enumerate = JS_EnumerateStub;
	js_rt->SFNodeClass._class.resolve = JS_ResolveStub;
	js_rt->SFNodeClass._class.convert = JS_ConvertStub;
	js_rt->SFNodeClass._class.finalize = node_finalize;
	js_rt->SFNodeClass._class.hasInstance = gf_sg_js_has_instance;
#endif

	JS_SETUP_CLASS(js_rt->SFVec2fClass , "SFVec2f", JSCLASS_HAS_PRIVATE,
	               vec2f_getProperty, vec2f_setProperty, field_finalize);

	JS_SETUP_CLASS(js_rt->SFVec3fClass , "SFVec3f", JSCLASS_HAS_PRIVATE,
	               vec3f_getProperty, vec3f_setProperty, field_finalize);

	JS_SETUP_CLASS(js_rt->SFRotationClass , "SFRotation", JSCLASS_HAS_PRIVATE,
	               rot_getProperty, rot_setProperty,  field_finalize);

	JS_SETUP_CLASS(js_rt->SFColorClass , "SFColor", JSCLASS_HAS_PRIVATE,
	               color_getProperty, color_setProperty, field_finalize);

	JS_SETUP_CLASS(js_rt->SFImageClass , "SFImage", JSCLASS_HAS_PRIVATE,
	               image_getProperty, image_setProperty, field_finalize);

	JS_SETUP_CLASS(js_rt->MFInt32Class , "MFInt32", JSCLASS_HAS_PRIVATE,
	               array_getElement,  array_setElement, array_finalize);

	JS_SETUP_CLASS(js_rt->MFBoolClass , "MFBool", JSCLASS_HAS_PRIVATE,
	               array_getElement,  array_setElement, array_finalize);

	JS_SETUP_CLASS(js_rt->MFTimeClass , "MFTime", JSCLASS_HAS_PRIVATE,
	               array_getElement,  array_setElement,  array_finalize);

	JS_SETUP_CLASS(js_rt->MFFloatClass , "MFFloat", JSCLASS_HAS_PRIVATE,
	               array_getElement,  array_setElement,  array_finalize);

	JS_SETUP_CLASS(js_rt->MFUrlClass , "MFUrl", JSCLASS_HAS_PRIVATE,
	               array_getElement,  array_setElement,  array_finalize);

	JS_SETUP_CLASS(js_rt->MFStringClass , "MFString", JSCLASS_HAS_PRIVATE,
	               array_getElement,  array_setElement,  array_finalize);

	JS_SETUP_CLASS(js_rt->MFNodeClass , "MFNode", JSCLASS_HAS_PRIVATE,
	               array_getElement,  array_setElement,  array_finalize);

	JS_SETUP_CLASS(js_rt->MFVec2fClass , "MFVec2f", JSCLASS_HAS_PRIVATE,
	               array_getElement,  array_setElement,  array_finalize);

	JS_SETUP_CLASS(js_rt->MFVec3fClass , "MFVec3f", JSCLASS_HAS_PRIVATE,
	               array_getElement,  array_setElement,  array_finalize);

	JS_SETUP_CLASS(js_rt->MFRotationClass , "MFRotation", JSCLASS_HAS_PRIVATE,
	               array_getElement,  array_setElement,  array_finalize);

	JS_SETUP_CLASS(js_rt->MFColorClass , "MFColor", JSCLASS_HAS_PRIVATE,
	               array_getElement,  array_setElement,  array_finalize);

	JS_SetErrorReporter(sc->js_ctx, script_error);

	sc->js_obj = gf_sg_js_global_object(sc->js_ctx, &js_rt->globalClass);

	JS_InitStandardClasses(sc->js_ctx, sc->js_obj);
	{
		JSFunctionSpec globalFunctions[] = {
			SMJS_FUNCTION_SPEC("print", JSPrint, 0),
			SMJS_FUNCTION_SPEC("alert", JSPrint, 0),
			SMJS_FUNCTION_SPEC("parseXML", vrml_parse_xml, 0),
			SMJS_FUNCTION_SPEC(0, 0, 0)
		};
		JS_DefineFunctions(sc->js_ctx, sc->js_obj, globalFunctions );
	}
	/*remember pointer to scene graph!!*/
	SMJS_SET_PRIVATE(sc->js_ctx, sc->js_obj, script->sgprivate->scenegraph);


	JS_DefineProperty(sc->js_ctx, sc->js_obj, "FALSE", BOOLEAN_TO_JSVAL(JS_FALSE), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT );
	JS_DefineProperty(sc->js_ctx, sc->js_obj, "TRUE", BOOLEAN_TO_JSVAL(JS_TRUE), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT );
	JS_DefineProperty(sc->js_ctx, sc->js_obj, "NULL", JSVAL_NULL, 0, 0, JSPROP_READONLY | JSPROP_PERMANENT );
	JS_DefineProperty(sc->js_ctx, sc->js_obj, "_this", PRIVATE_TO_JSVAL(script), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT );

	sc->js_browser = JS_DefineObject(sc->js_ctx, sc->js_obj, "Browser", &js_rt->browserClass._class, 0, 0 );
	JS_DefineProperty(sc->js_ctx, sc->js_browser, "_this", PRIVATE_TO_JSVAL(script), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT );
	{
		JSFunctionSpec browserFunctions[] = {
			SMJS_FUNCTION_SPEC("getName", getName, 0),
			SMJS_FUNCTION_SPEC("getVersion", getVersion, 0),
			SMJS_FUNCTION_SPEC("getCurrentSpeed", getCurrentSpeed, 0),
			SMJS_FUNCTION_SPEC("getCurrentFrameRate", getCurrentFrameRate, 0),
			SMJS_FUNCTION_SPEC("getWorldURL", getWorldURL, 0),
			SMJS_FUNCTION_SPEC("replaceWorld", replaceWorld, 1),
			SMJS_FUNCTION_SPEC("addRoute", addRoute, 4),
			SMJS_FUNCTION_SPEC("deleteRoute", deleteRoute, 4),
			SMJS_FUNCTION_SPEC("loadURL", loadURL, 1),
			SMJS_FUNCTION_SPEC("createVrmlFromString", createVrmlFromString, 1),
			SMJS_FUNCTION_SPEC("setDescription", setDescription, 1),
			SMJS_FUNCTION_SPEC("print",           JSPrint,          1),
			SMJS_FUNCTION_SPEC("getScript",  getScript,          0),
			SMJS_FUNCTION_SPEC("getProto",  getProto,          0),
			SMJS_FUNCTION_SPEC("loadScript",  loadScript,          1),
			SMJS_FUNCTION_SPEC("getElementById",  getElementById,   1),
			SMJS_FUNCTION_SPEC(0, 0, 0)
		};
		JS_DefineFunctions(sc->js_ctx, sc->js_browser, browserFunctions);
	}

	{
		JSFunctionSpec SFNodeMethods[] = {
			SMJS_FUNCTION_SPEC("toString", node_toString, 0),
			SMJS_FUNCTION_SPEC("getTime", node_getTime, 0),
#ifndef GPAC_DISABLE_SVG
			SMJS_FUNCTION_SPEC("addEventListenerNS", vrml_event_add_listener, 4),
			SMJS_FUNCTION_SPEC("removeEventListenerNS", vrml_event_remove_listener, 4),
			SMJS_FUNCTION_SPEC("addEventListener", vrml_event_add_listener, 3),
			SMJS_FUNCTION_SPEC("removeEventListener", vrml_event_remove_listener, 3),
			SMJS_FUNCTION_SPEC("dispatchEvent", vrml_dom3_not_implemented, 1),
#endif
			SMJS_FUNCTION_SPEC(0, 0, 0)
		};
		JSPropertySpec SFNodeProps[] = {
			SMJS_PROPERTY_SPEC("__dummy",       0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0)
		};
		GF_JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->SFNodeClass, SFNodeConstructor, 1, SFNodeProps, SFNodeMethods, 0, 0);
	}
	{
		JSPropertySpec SFVec2fProps[] = {
			SMJS_PROPERTY_SPEC("x",       0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("y",       1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0)
		};
		JSFunctionSpec SFVec2fMethods[] = {
			SMJS_FUNCTION_SPEC("add",			vec2f_add,      1),
			SMJS_FUNCTION_SPEC("divide",          vec2f_divide,   1),
			SMJS_FUNCTION_SPEC("dot",             vec2f_dot,      1),
			SMJS_FUNCTION_SPEC("length",          vec2f_length,   0),
			SMJS_FUNCTION_SPEC("multiply",        vec2f_multiply, 1),
			SMJS_FUNCTION_SPEC("normalize",       vec2f_normalize,0),
			SMJS_FUNCTION_SPEC("subtract",        vec2f_subtract, 1),
			SMJS_FUNCTION_SPEC("negate",          vec2f_negate,   0),
			SMJS_FUNCTION_SPEC("toString",        field_toString, 0),
			SMJS_FUNCTION_SPEC(0, 0, 0)
		};
		GF_JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->SFVec2fClass, SFVec2fConstructor, 0, SFVec2fProps, SFVec2fMethods, 0, 0);
	}
	{
		JSPropertySpec SFVec3fProps[] = {
			SMJS_PROPERTY_SPEC("x",       0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("y",       1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("z",       2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0)
		};
		JSFunctionSpec SFVec3fMethods[] = {
			SMJS_FUNCTION_SPEC("add",             vec3f_add,      1),
			SMJS_FUNCTION_SPEC("divide",          vec3f_divide,   1),
			SMJS_FUNCTION_SPEC("dot",             vec3f_dot,      1),
			SMJS_FUNCTION_SPEC("length",          vec3f_length,   0),
			SMJS_FUNCTION_SPEC("multiply",        vec3f_multiply, 1),
			SMJS_FUNCTION_SPEC("normalize",       vec3f_normalize,0),
			SMJS_FUNCTION_SPEC("subtract",        vec3f_subtract, 1),
			SMJS_FUNCTION_SPEC("cross",			vec3f_cross,	1),
			SMJS_FUNCTION_SPEC("negate",          vec3f_negate,   0),
			SMJS_FUNCTION_SPEC("toString",        field_toString,	0),
			SMJS_FUNCTION_SPEC(0, 0, 0)
		};
		GF_JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->SFVec3fClass, SFVec3fConstructor, 0, SFVec3fProps, SFVec3fMethods, 0, 0);
	}
	{
		JSPropertySpec SFRotationProps[] = {
			SMJS_PROPERTY_SPEC("xAxis",       0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("yAxis",       1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("zAxis",       2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("angle",   3,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0)
		};
		JSFunctionSpec SFRotationMethods[] = {
			SMJS_FUNCTION_SPEC("getAxis",         rot_getAxis,      1),
			SMJS_FUNCTION_SPEC("inverse",         rot_inverse,   1),
			SMJS_FUNCTION_SPEC("multiply",        rot_multiply,      1),
			SMJS_FUNCTION_SPEC("multVec",         rot_multVec,   0),
			SMJS_FUNCTION_SPEC("setAxis",			rot_setAxis, 1),
			SMJS_FUNCTION_SPEC("slerp",			rot_slerp,0),
			SMJS_FUNCTION_SPEC("toString",        field_toString,	0),
			SMJS_FUNCTION_SPEC(0, 0, 0)
		};
		GF_JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->SFRotationClass, SFRotationConstructor, 0, SFRotationProps, SFRotationMethods, 0, 0);
	}
	{
		JSPropertySpec SFColorProps[] = {
			SMJS_PROPERTY_SPEC("r",       0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("g",       1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("b",       2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0)
		};
		JSFunctionSpec SFColorMethods[] = {
			SMJS_FUNCTION_SPEC("setHSV",          color_setHSV,   3),
			SMJS_FUNCTION_SPEC("getHSV",          color_getHSV,   0),
			SMJS_FUNCTION_SPEC("toString",        field_toString,       0),
			SMJS_FUNCTION_SPEC(0, 0, 0)
		};
		GF_JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->SFColorClass, SFColorConstructor, 0, SFColorProps, SFColorMethods, 0, 0);
	}
	{
		JSPropertySpec SFImageProps[] = {
			SMJS_PROPERTY_SPEC("x",       0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("y",       1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("comp",    2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("array",   3,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0)
		};
		GF_JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->SFImageClass, SFImageConstructor, 0, SFImageProps, 0, 0, 0);
	}

	{
		JSPropertySpec MFArrayProp[] = {
			SMJS_PROPERTY_SPEC( "length", 0, JSPROP_PERMANENT | JSPROP_SHARED, array_getLength, array_setLength ),
			SMJS_PROPERTY_SPEC( "assign", 1, JSPROP_PERMANENT | JSPROP_SHARED, array_getElement, array_setElement),
			SMJS_PROPERTY_SPEC( 0, 0, 0, 0, 0 )
		};
		JSFunctionSpec MFArrayMethods[] = {
			SMJS_FUNCTION_SPEC("toString",        field_toString,       0),
			SMJS_FUNCTION_SPEC(0, 0, 0)
		};

		GF_JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->MFInt32Class, MFInt32Constructor, 0, MFArrayProp, MFArrayMethods, 0, 0);
		GF_JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->MFBoolClass, MFBoolConstructor, 0, MFArrayProp, MFArrayMethods, 0, 0);
		GF_JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->MFFloatClass, MFFloatConstructor, 0, MFArrayProp, MFArrayMethods, 0, 0);
		GF_JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->MFTimeClass, MFTimeConstructor, 0, MFArrayProp, MFArrayMethods, 0, 0);
		GF_JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->MFStringClass, MFStringConstructor, 0, MFArrayProp, MFArrayMethods, 0, 0);
		GF_JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->MFUrlClass, MFURLConstructor, 0, MFArrayProp, MFArrayMethods, 0, 0);
		GF_JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->MFVec2fClass, MFVec2fConstructor, 0, MFArrayProp, MFArrayMethods, 0, 0);
		GF_JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->MFVec3fClass, MFVec3fConstructor, 0, MFArrayProp, MFArrayMethods, 0, 0);
		GF_JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->MFRotationClass, MFRotationConstructor, 0, MFArrayProp, MFArrayMethods, 0, 0);
		GF_JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->MFColorClass, MFColorConstructor, 0, MFArrayProp, MFArrayMethods, 0, 0);
		GF_JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->MFNodeClass, MFNodeConstructor, 0, MFArrayProp, MFArrayMethods, 0, 0);
	}

	/*
		cant get any doc specifying if these are supposed to be supported in MPEG4Script...
		GF_JS_InitClass(sc->js_ctx, sc->js_obj, 0, &SFVec4fClass, SFVec4fConstructor, 0, SFVec4fProps, SFVec4fMethods, 0, 0);
		GF_JS_InitClass(sc->js_ctx, sc->js_obj, 0, &MFVec4fClass, MFVec4fCons, 0, MFArrayProp, 0, 0, 0);
	*/

}



JSBool gf_sg_script_to_node_field(JSContext *c, jsval val, GF_FieldInfo *field, GF_Node *owner, GF_JSField *parent)
{
	jsdouble d;
	Bool changed;
	JSObject *obj;
	GF_JSField *p, *from;
	jsuint len;
	jsval item;
	u32 i;

	if (JSVAL_IS_VOID(val)) return JS_TRUE;
	if ((field->fieldType != GF_SG_VRML_SFNODE) && JSVAL_IS_NULL(val)) return JS_TRUE;


	switch (field->fieldType) {
	case GF_SG_VRML_SFBOOL:
	{
		if (JSVAL_IS_BOOLEAN(val)) {
			*((SFBool *) field->far_ptr) = JSVAL_TO_BOOLEAN(val);
			Script_FieldChanged(c, owner, parent, field);
		}
		return JS_TRUE;
	}
	case GF_SG_VRML_SFINT32:
	{
		if (JSVAL_IS_INT(val) ) {
			* ((SFInt32 *) field->far_ptr) = JSVAL_TO_INT(val);
			Script_FieldChanged(c, owner, parent, field);
		} else if (JSVAL_IS_NUMBER(val) ) {
			SMJS_GET_NUMBER(val, d );
			*((SFInt32 *) field->far_ptr) = (s32) d;
			Script_FieldChanged(c, owner, parent, field);
		}
		return JS_TRUE;
	}
	case GF_SG_VRML_SFFLOAT:
	{
		if (JSVAL_IS_NUMBER(val) ) {
			SMJS_GET_NUMBER(val, d );
			*((SFFloat *) field->far_ptr) = FLT2FIX( d);
			Script_FieldChanged(c, owner, parent, field);
		}
		return JS_TRUE;
	}
	case GF_SG_VRML_SFTIME:
	{
		if (JSVAL_IS_NUMBER(val) ) {
			SMJS_GET_NUMBER(val, d );
			*((SFTime *) field->far_ptr) = (Double) d;
			Script_FieldChanged(c, owner, parent, field);
		}
		return JS_TRUE;
	}
	case GF_SG_VRML_SFSTRING:
	{
		SFString *s = (SFString*)field->far_ptr;
		JSString *str = JSVAL_IS_STRING(val) ? JSVAL_TO_STRING(val) : JS_ValueToString(c, val);
		char *str_val = SMJS_CHARS_FROM_STRING(c, str);
		/*we do filter strings since rebuilding a text is quite slow, so let's avoid killing the compositors*/
		if (!s->buffer || strcmp(str_val, s->buffer)) {
			if ( s->buffer) gf_free(s->buffer);
			s->buffer = gf_strdup(str_val);
			Script_FieldChanged(c, owner, parent, field);
		}
		SMJS_FREE(c, str_val);
		return JS_TRUE;
	}
	case GF_SG_VRML_SFURL:
	{
		char *str_val;
		JSString *str = JSVAL_IS_STRING(val) ? JSVAL_TO_STRING(val) : JS_ValueToString(c, val);
		if (((SFURL*)field->far_ptr)->url) gf_free(((SFURL*)field->far_ptr)->url);
		str_val = SMJS_CHARS_FROM_STRING(c, str);
		((SFURL*)field->far_ptr)->url = gf_strdup(str_val);
		((SFURL*)field->far_ptr)->OD_ID = 0;
		Script_FieldChanged(c, owner, parent, field);
		SMJS_FREE(c, str_val);
		return JS_TRUE;
	}
	case GF_SG_VRML_MFSTRING:
		if (JSVAL_IS_STRING(val)) {
			char *str_val;
			JSString *str = JSVAL_TO_STRING(val);
			gf_sg_vrml_mf_reset(field->far_ptr, field->fieldType);
			gf_sg_vrml_mf_alloc(field->far_ptr, field->fieldType, 1);
			str_val = SMJS_CHARS_FROM_STRING(c, str);
			((MFString*)field->far_ptr)->vals[0] = gf_strdup(str_val);
			Script_FieldChanged(c, owner, parent, field);
			SMJS_FREE(c, str_val);
			return JS_TRUE;
		}
	case GF_SG_VRML_MFURL:
		if (JSVAL_IS_STRING(val)) {
			char *str_val;
			JSString *str = JSVAL_TO_STRING(val);
			gf_sg_vrml_mf_reset(field->far_ptr, field->fieldType);
			gf_sg_vrml_mf_alloc(field->far_ptr, field->fieldType, 1);
			str_val = SMJS_CHARS_FROM_STRING(c, str);
			((MFURL*)field->far_ptr)->vals[0].url = gf_strdup(str_val);
			((MFURL*)field->far_ptr)->vals[0].OD_ID = 0;
			Script_FieldChanged(c, owner, parent, field);
			SMJS_FREE(c, str_val);
			return JS_TRUE;
		}

	default:
		break;
	}

	//from here we must have an object
	if (! JSVAL_IS_OBJECT(val)) return JS_FALSE;
	obj = JSVAL_TO_OBJECT(val) ;

	switch (field->fieldType) {
	case GF_SG_VRML_SFVEC2F:
	{
		if (GF_JS_InstanceOf(c, obj, &js_rt->SFVec2fClass, NULL) ) {
			p = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);
			gf_sg_vrml_field_copy(field->far_ptr, p->field.far_ptr, GF_SG_VRML_SFVEC2F);
			Script_FieldChanged(c, owner, parent, field);
		}
		return JS_TRUE;
	}
	case GF_SG_VRML_SFVEC3F:
	{
		if (GF_JS_InstanceOf(c, obj, &js_rt->SFVec3fClass, NULL) ) {
			p = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);
			gf_sg_vrml_field_copy(field->far_ptr, p->field.far_ptr, GF_SG_VRML_SFVEC3F);
			Script_FieldChanged(c, owner, parent, field);
		}
		return JS_TRUE;
	}
	case GF_SG_VRML_SFROTATION:
	{
		if ( GF_JS_InstanceOf(c, obj, &js_rt->SFRotationClass, NULL) ) {
			p = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);
			gf_sg_vrml_field_copy(field->far_ptr, p->field.far_ptr, GF_SG_VRML_SFROTATION);
			Script_FieldChanged(c, owner, parent, field);
		}
		return JS_TRUE;
	}
	case GF_SG_VRML_SFCOLOR:
	{
		if (GF_JS_InstanceOf(c, obj, &js_rt->SFColorClass, NULL) ) {
			p = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);
			gf_sg_vrml_field_copy(field->far_ptr, p->field.far_ptr, GF_SG_VRML_SFCOLOR);
			Script_FieldChanged(c, owner, parent, field);
		}
		return JS_TRUE;
	}
	case GF_SG_VRML_SFNODE:
	{
		/*replace object*/
		if (*((GF_Node**)field->far_ptr)) {
			gf_node_unregister(*((GF_Node**)field->far_ptr), owner);
			*((GF_Node**)field->far_ptr) = NULL;
		}

		if (JSVAL_IS_NULL(val)) {
			Script_FieldChanged(c, owner, parent, field);
		} else if (GF_JS_InstanceOf(c, obj, &js_rt->SFNodeClass, NULL) ) {
			GF_Node *n = * (GF_Node**) ((GF_JSField *) SMJS_GET_PRIVATE(c, obj))->field.far_ptr;
			* ((GF_Node **)field->far_ptr) = n;
			gf_node_register(n, owner);
			Script_FieldChanged(c, owner, parent, field);
		}
		return JS_TRUE;
	}
	case GF_SG_VRML_SFIMAGE:
	{
		if ( GF_JS_InstanceOf(c, obj, &js_rt->SFImageClass, NULL) ) {
			p = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);
			gf_sg_vrml_field_copy(field->far_ptr, p->field.far_ptr, GF_SG_VRML_SFIMAGE);
			Script_FieldChanged(c, owner, parent, field);
		}
		return JS_TRUE;
	}
	default:
		break;
	}

	//from here we handle only MF fields
	if ( !GF_JS_InstanceOf(c, obj, &js_rt->MFBoolClass, NULL)
	        && !GF_JS_InstanceOf(c, obj, &js_rt->MFInt32Class, NULL)
	        && !GF_JS_InstanceOf(c, obj, &js_rt->MFFloatClass, NULL)
	        && !GF_JS_InstanceOf(c, obj, &js_rt->MFTimeClass, NULL)
	        && !GF_JS_InstanceOf(c, obj, &js_rt->MFStringClass, NULL)
	        && !GF_JS_InstanceOf(c, obj, &js_rt->MFUrlClass, NULL)
	        && !GF_JS_InstanceOf(c, obj, &js_rt->MFVec2fClass, NULL)
	        && !GF_JS_InstanceOf(c, obj, &js_rt->MFVec3fClass, NULL)
	        && !GF_JS_InstanceOf(c, obj, &js_rt->MFRotationClass, NULL)
	        && !GF_JS_InstanceOf(c, obj, &js_rt->MFColorClass, NULL)
	        && !GF_JS_InstanceOf(c, obj, &js_rt->MFNodeClass, NULL)
	        /*
	        		&& !GF_JS_InstanceOf(c, obj, &MFVec4fClass, NULL)
	        */
	   ) return JS_TRUE;


	p = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);
	JS_GetArrayLength(c, p->js_list, &len);

	/*special handling for MF node, reset list first*/
	if (GF_JS_InstanceOf(c, obj, &js_rt->MFNodeClass, NULL)) {
		GF_Node *child;
		GF_ChildNodeItem *last = NULL;
		gf_node_unregister_children(owner, * (GF_ChildNodeItem **) field->far_ptr);
		* (GF_ChildNodeItem **) field->far_ptr = NULL;

		for (i=0; i<len; i++) {
			JSObject *node_obj;
			JS_GetElement(c, p->js_list, (jsint) i, &item);
			if (JSVAL_IS_NULL(item)) break;
			if (!JSVAL_IS_OBJECT(item)) break;
			node_obj = JSVAL_TO_OBJECT(item);
			if ( !GF_JS_InstanceOf(c, node_obj, &js_rt->SFNodeClass, NULL)) break;
			from = (GF_JSField *) SMJS_GET_PRIVATE(c, node_obj);

			child = * ((GF_Node**)from->field.far_ptr);

			gf_node_list_add_child_last( (GF_ChildNodeItem **) field->far_ptr , child, &last);
			gf_node_register(child, owner);
		}
		Script_FieldChanged(c, owner, parent, field);
		/*and mark the field as changed*/
		JSScript_NodeModified(owner->sgprivate->scenegraph, owner, field, NULL);
		return JS_TRUE;
	}

	/*again, check text changes*/
	changed = (field->fieldType == GF_SG_VRML_MFSTRING) ? 0 : 1;
	/*gf_realloc*/
	if (len != ((GenMFField *)field->far_ptr)->count) {
		gf_sg_vrml_mf_reset(field->far_ptr, field->fieldType);
		gf_sg_vrml_mf_alloc(field->far_ptr, field->fieldType, len);
		changed = 1;
	}
	/*assign each slot*/
	for (i=0; i<len; i++) {
		JS_GetElement(c, p->js_list, (jsint) i, &item);

		switch (field->fieldType) {
		case GF_SG_VRML_MFBOOL:
			if (JSVAL_IS_BOOLEAN(item)) {
				((MFBool*)field->far_ptr)->vals[i] = (Bool) JSVAL_TO_BOOLEAN(item);
			}
			break;
		case GF_SG_VRML_MFINT32:
			if (JSVAL_IS_INT(item)) {
				((MFInt32 *)field->far_ptr)->vals[i] = (s32) JSVAL_TO_INT(item);
			}
			break;
		case GF_SG_VRML_MFFLOAT:
			if (JSVAL_IS_NUMBER(item)) {
				SMJS_GET_NUMBER(item, d);
				((MFFloat *)field->far_ptr)->vals[i] = FLT2FIX( d);
			}
			break;
		case GF_SG_VRML_MFTIME:
			if (JSVAL_IS_NUMBER(item)) {
				SMJS_GET_NUMBER(item, d);
				((MFTime *)field->far_ptr)->vals[i] = d;
			}
			break;
		case GF_SG_VRML_MFSTRING:
		{
			MFString *mfs = (MFString *) field->far_ptr;
			JSString *str = JSVAL_IS_STRING(item) ? JSVAL_TO_STRING(item) : JS_ValueToString(c, item);
			char *str_val = SMJS_CHARS_FROM_STRING(c, str);
			if (!mfs->vals[i] || strcmp(str_val, mfs->vals[i]) ) {
				if (mfs->vals[i]) gf_free(mfs->vals[i]);
				mfs->vals[i] = gf_strdup(str_val);
				changed = 1;
			}
			SMJS_FREE(c, str_val);
		}
		break;
		case GF_SG_VRML_MFURL:
		{
			char *str_val;
			MFURL *mfu = (MFURL *) field->far_ptr;
			JSString *str = JSVAL_IS_STRING(item) ? JSVAL_TO_STRING(item) : JS_ValueToString(c, item);
			if (mfu->vals[i].url) gf_free(mfu->vals[i].url);
			str_val = SMJS_CHARS_FROM_STRING(c, str);
			mfu->vals[i].url = gf_strdup(str_val);
			mfu->vals[i].OD_ID = 0;
			SMJS_FREE(c, str_val);
		}
		break;

		case GF_SG_VRML_MFVEC2F:
			if ( JSVAL_IS_OBJECT(item) && GF_JS_InstanceOf(c, JSVAL_TO_OBJECT(item), &js_rt->SFVec2fClass, NULL) ) {
				from = (GF_JSField *) SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(item));
				gf_sg_vrml_field_copy(& ((MFVec2f*)field->far_ptr)->vals[i], from->field.far_ptr, GF_SG_VRML_SFVEC2F);
			}
			break;
		case GF_SG_VRML_MFVEC3F:
			if ( JSVAL_IS_OBJECT(item) && GF_JS_InstanceOf(c, JSVAL_TO_OBJECT(item), &js_rt->SFVec3fClass, NULL) ) {
				from = (GF_JSField *) SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(item));
				gf_sg_vrml_field_copy(& ((MFVec3f*)field->far_ptr)->vals[i], from->field.far_ptr, GF_SG_VRML_SFVEC3F);
			}
			break;
		case GF_SG_VRML_MFROTATION:
			if ( JSVAL_IS_OBJECT(item) && GF_JS_InstanceOf(c, JSVAL_TO_OBJECT(item), &js_rt->SFRotationClass, NULL) ) {
				from = (GF_JSField *) SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(item));
				gf_sg_vrml_field_copy(& ((MFRotation*)field->far_ptr)->vals[i], from->field.far_ptr, GF_SG_VRML_SFROTATION);
			}
			break;
		case GF_SG_VRML_MFCOLOR:
			if ( JSVAL_IS_OBJECT(item) && GF_JS_InstanceOf(c, JSVAL_TO_OBJECT(item), &js_rt->SFColorClass, NULL) ) {
				from = (GF_JSField *) SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(item));
				gf_sg_vrml_field_copy(& ((MFColor*)field->far_ptr)->vals[i], from->field.far_ptr, GF_SG_VRML_SFCOLOR);
			}
			break;

		default:
			return JS_TRUE;
		}
	}
	if (changed) Script_FieldChanged(c, owner, parent, field);
	return JS_TRUE;
}


static void gf_sg_script_update_cached_object(GF_ScriptPriv *priv, JSObject *obj, GF_JSField *jsf, GF_FieldInfo *field, GF_Node *parent)
{
	u32 i;
	jsval newVal;
	JSString *s;

	/*we need to rebuild MF types where SF is a native type.*/
	GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[VRML JS] Recomputing cached jsobj\n") );
	switch (jsf->field.fieldType) {
	case GF_SG_VRML_MFBOOL:
	{
		MFBool *f = (MFBool *) field->far_ptr;
		JS_SetArrayLength(priv->js_ctx, jsf->js_list, 0);
		JS_SetArrayLength(priv->js_ctx, jsf->js_list, f->count);
		for (i=0; i<f->count; i++) {
			newVal = BOOLEAN_TO_JSVAL(f->vals[i]);
			JS_SetElement(priv->js_ctx, jsf->js_list, (jsint) i, &newVal);
		}
	}
	break;
	case GF_SG_VRML_MFINT32:
	{
		MFInt32 *f = (MFInt32 *) field->far_ptr;
		JS_SetArrayLength(priv->js_ctx, jsf->js_list, 0);
		JS_SetArrayLength(priv->js_ctx, jsf->js_list, f->count);
		for (i=0; i<f->count; i++) {
			newVal = INT_TO_JSVAL(f->vals[i]);
			JS_SetElement(priv->js_ctx, jsf->js_list, (jsint) i, &newVal);
		}
	}
	break;
	case GF_SG_VRML_MFFLOAT:
	{
		MFFloat *f = (MFFloat *) field->far_ptr;
		JS_SetArrayLength(priv->js_ctx, jsf->js_list, 0);
		JS_SetArrayLength(priv->js_ctx, jsf->js_list, f->count);
		for (i=0; i<f->count; i++) {
			newVal = JS_MAKE_DOUBLE(priv->js_ctx, FIX2FLT(f->vals[i]));
			JS_SetElement(priv->js_ctx, jsf->js_list, (jsint) i, &newVal);
		}
	}
	break;
	case GF_SG_VRML_MFTIME:
	{
		MFTime *f = (MFTime *) field->far_ptr;
		JS_SetArrayLength(priv->js_ctx, jsf->js_list, 0);
		JS_SetArrayLength(priv->js_ctx, jsf->js_list, f->count);
		for (i=0; i<f->count; i++) {
			newVal = JS_MAKE_DOUBLE(priv->js_ctx, f->vals[i]);
			JS_SetElement(priv->js_ctx, jsf->js_list, (jsint) i, &newVal);
		}
	}
	break;
	case GF_SG_VRML_MFSTRING:
	{
		MFString *f = (MFString *) field->far_ptr;
		JS_SetArrayLength(priv->js_ctx, jsf->js_list, 0);
		JS_SetArrayLength(priv->js_ctx, jsf->js_list, f->count);
		for (i=0; i<f->count; i++) {
			s = JS_NewStringCopyZ(priv->js_ctx, f->vals[i]);
			newVal = STRING_TO_JSVAL( s );
			JS_SetElement(priv->js_ctx, jsf->js_list, (jsint) i, &newVal);
		}
	}
	break;
	case GF_SG_VRML_MFURL:
	{
		MFURL *f = (MFURL *) field->far_ptr;
		JS_SetArrayLength(priv->js_ctx, jsf->js_list, 0);
		JS_SetArrayLength(priv->js_ctx, jsf->js_list, f->count);
		for (i=0; i<f->count; i++) {
			if (f->vals[i].OD_ID > 0) {
				char msg[30];
				sprintf(msg, "od:%d", f->vals[i].OD_ID);
				s = JS_NewStringCopyZ(priv->js_ctx, (const char *) msg);
			} else {
				s = JS_NewStringCopyZ(priv->js_ctx, f->vals[i].url);
			}
			newVal = STRING_TO_JSVAL(s);
			JS_SetElement(priv->js_ctx, jsf->js_list, (jsint) i, &newVal);
		}
	}
	break;
	/*
		MFNode is tricky because in VRML/MPEG-4, SFNode are assigned by referenced, not copy.
		We therefore need to make sure we reuse existing SFNode object rather than
		blindly recreating them
	*/
	case GF_SG_VRML_MFNODE:
	{
#if 0
		GF_ChildNodeItem *f = *(GF_ChildNodeItem **) field->far_ptr;
		u32 j;
		jsuint count;
		GF_List *temp_objs = gf_list_new();

		/*1: find all existing objs for each node*/
		JS_GetArrayLength(priv->js_ctx, jsf->js_list, &count);

		/*this may introduce bugs when a child is being replaced through an update command, but it is way
		too costly to handle in script*/
		if (gf_node_list_get_count(f)==count) return;

		fprintf(stderr, "rewriting MFNode cache\n");
		while (f) {
			slot = NULL;
			/*first look in the original array*/
			for (j=0; j<count; j++) {
				JSObject *an_obj;
				JS_GetElement(priv->js_ctx, jsf->js_list, (jsint) j, &newVal);
				an_obj = JSVAL_TO_OBJECT(newVal);
				if (an_obj) slot = SMJS_GET_PRIVATE(priv->js_ctx, an_obj);
				if (slot && (slot->node==f->node)) {
					gf_list_add(temp_objs, an_obj);
					break;
				}
				slot = NULL;
			}
			if (!slot) {
				JSObject *an_obj = node_get_binding(priv, f->node, 0);
				gf_list_add(temp_objs, an_obj);
			}
			f = f->next;
		}
		/*2- and rewrite the final array*/
		count = gf_list_count(temp_objs);
		if (JS_SetArrayLength(priv->js_ctx, jsf->js_list, count) != JS_TRUE) return;
		for (j=0; j<count; j++) {
			JSObject *an_obj = gf_list_get(temp_objs, j);
			slot = SMJS_GET_PRIVATE(priv->js_ctx, an_obj);
			newVal = OBJECT_TO_JSVAL(an_obj);
			JS_SetElement(priv->js_ctx, jsf->js_list, (jsint) j, &newVal);
		}
		gf_list_del(temp_objs);
#endif
	}
	break;
	}
	jsf->field.NDTtype = 0;
}



#define SETUP_FIELD	\
		jsf = NewJSField(priv->js_ctx);	\
		jsf->owner = parent;	\
		if(parent) gf_node_get_field(parent, field->fieldIndex, &jsf->field);	\
 
#define SETUP_MF_FIELD	\
		if (!obj) return JSVAL_NULL; \
		jsf = (GF_JSField *) SMJS_GET_PRIVATE(priv->js_ctx, obj);	\
		jsf->owner = parent;		\
		if (parent) gf_node_get_field(parent, field->fieldIndex, &jsf->field);	\
 


jsval gf_sg_script_to_smjs_field(GF_ScriptPriv *priv, GF_FieldInfo *field, GF_Node *parent, Bool force_evaluate)
{
	u32 i;
	JSObject *obj = NULL;
	GF_JSField *jsf = NULL;
	GF_JSField *slot = NULL;
	jsval newVal;
	JSString *s;

	/*native types*/
	switch (field->fieldType) {
	case GF_SG_VRML_SFBOOL:
		return BOOLEAN_TO_JSVAL( * ((SFBool *) field->far_ptr) );
	case GF_SG_VRML_SFINT32:
		return INT_TO_JSVAL(  * ((SFInt32 *) field->far_ptr));
	case GF_SG_VRML_SFFLOAT:
		return JS_MAKE_DOUBLE(priv->js_ctx, FIX2FLT(* ((SFFloat *) field->far_ptr) ));
	case GF_SG_VRML_SFTIME:
		return JS_MAKE_DOUBLE(priv->js_ctx, * ((SFTime *) field->far_ptr));
	case GF_SG_VRML_SFSTRING:
	{
		s = JS_NewStringCopyZ(priv->js_ctx, ((SFString *) field->far_ptr)->buffer);
		return STRING_TO_JSVAL( s );
	}
	case GF_SG_VRML_SFURL:
	{
		SFURL *url = (SFURL *)field->far_ptr;
		if (url->OD_ID > 0) {
			char msg[30];
			sprintf(msg, "od:%d", url->OD_ID);
			s = JS_NewStringCopyZ(priv->js_ctx, (const char *) msg);
		} else {
			s = JS_NewStringCopyZ(priv->js_ctx, (const char *) url->url);
		}
		return STRING_TO_JSVAL( s );
	}
	}

	obj =  NULL;

	/*look into object bank in case we already have this object*/
	if (parent && parent->sgprivate->interact && parent->sgprivate->interact->js_binding) {
		i=0;
		while ((jsf = gf_list_enum(parent->sgprivate->interact->js_binding->fields, &i))) {
			obj = jsf->obj;
			if (
			    /*make sure we use the same JS context*/
			    (jsf->js_ctx == priv->js_ctx)
			    && (jsf->owner == parent)
			    && (jsf->field.far_ptr==field->far_ptr)
			) {
				Bool do_rebuild = 0;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[VRML JS] found cached jsobj %p (field %s) in script %s bank (%d entries)\n", obj, field->name, gf_node_get_log_name((GF_Node*)JS_GetScript(priv->js_ctx)), gf_list_count(priv->js_cache) ) );
				if (!force_evaluate && !jsf->field.NDTtype) return OBJECT_TO_JSVAL(obj);

				switch (field->fieldType) {
				//we need to rewrite these
				case GF_SG_VRML_MFVEC2F:
				case GF_SG_VRML_MFVEC3F:
				case GF_SG_VRML_MFROTATION:
				case GF_SG_VRML_MFCOLOR:
					if (force_evaluate) {
						do_rebuild = 1;
						break;
					}
				default:
					break;
				}
				if (do_rebuild) {
					JS_SetArrayLength(priv->js_ctx, jsf->js_list, 0);
					break;
				}

				gf_sg_script_update_cached_object(priv, obj, jsf, field, parent);
				return OBJECT_TO_JSVAL(obj);
			}
			obj = NULL;
		}
	}

	if (!obj) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[VRML JS] creating jsobj %s.%s\n", gf_node_get_name(parent), field->name) );
	}

	switch (field->fieldType) {
	case GF_SG_VRML_SFVEC2F:
		SETUP_FIELD
		obj = JS_NewObject(priv->js_ctx, &js_rt->SFVec2fClass._class, 0, priv->js_obj);
		break;
	case GF_SG_VRML_SFVEC3F:
		SETUP_FIELD
		obj = JS_NewObject(priv->js_ctx, &js_rt->SFVec3fClass._class, 0, priv->js_obj);
		break;
	case GF_SG_VRML_SFROTATION:
		SETUP_FIELD
		obj = JS_NewObject(priv->js_ctx, &js_rt->SFRotationClass._class, 0, priv->js_obj);
		break;
	case GF_SG_VRML_SFCOLOR:
		SETUP_FIELD
		obj = JS_NewObject(priv->js_ctx, &js_rt->SFColorClass._class, 0, priv->js_obj);
		break;
	case GF_SG_VRML_SFIMAGE:
		SETUP_FIELD
		obj = JS_NewObject(priv->js_ctx, &js_rt->SFImageClass._class, 0, priv->js_obj);
		break;
	case GF_SG_VRML_SFNODE:
		if (! *(GF_Node**) field->far_ptr)
			return JSVAL_NULL;

		obj = node_get_binding(priv, *(GF_Node**) field->far_ptr, 0);
		jsf = SMJS_GET_PRIVATE(priv->js_ctx, obj);
		if (!jsf->owner)
			jsf->owner = parent;
		else
			return OBJECT_TO_JSVAL(obj);
		break;


	case GF_SG_VRML_MFBOOL:
	{
		MFBool *f = (MFBool *) field->far_ptr;
		obj = SMJS_CONSTRUCT_OBJECT(priv->js_ctx, &js_rt->MFBoolClass, priv->js_obj);
		SETUP_MF_FIELD
		for (i = 0; i<f->count; i++) {
			jsval newVal = BOOLEAN_TO_JSVAL(f->vals[i]);
			JS_SetElement(priv->js_ctx, jsf->js_list, (jsint) i, &newVal);
		}
		break;
	}
	case GF_SG_VRML_MFINT32:
	{
		MFInt32 *f = (MFInt32 *) field->far_ptr;
		obj = SMJS_CONSTRUCT_OBJECT(priv->js_ctx, &js_rt->MFInt32Class, priv->js_obj);
		SETUP_MF_FIELD
		for (i=0; i<f->count; i++) {
			jsval newVal = INT_TO_JSVAL(f->vals[i]);
			JS_SetElement(priv->js_ctx, jsf->js_list, (jsint) i, &newVal);
		}
		break;
	}
	case GF_SG_VRML_MFFLOAT:
	{
		MFFloat *f = (MFFloat *) field->far_ptr;
		obj = SMJS_CONSTRUCT_OBJECT(priv->js_ctx, &js_rt->MFFloatClass, priv->js_obj);
		SETUP_MF_FIELD
		for (i=0; i<f->count; i++) {
			jsval newVal = JS_MAKE_DOUBLE(priv->js_ctx, FIX2FLT(f->vals[i]));
			JS_SetElement(priv->js_ctx, jsf->js_list, (jsint) i, &newVal);
		}
		break;
	}
	case GF_SG_VRML_MFTIME:
	{
		MFTime *f = (MFTime *) field->far_ptr;
		obj = SMJS_CONSTRUCT_OBJECT(priv->js_ctx, &js_rt->MFTimeClass, priv->js_obj);
		SETUP_MF_FIELD
		for (i=0; i<f->count; i++) {
			jsval newVal = JS_MAKE_DOUBLE(priv->js_ctx, f->vals[i]);
			JS_SetElement(priv->js_ctx, jsf->js_list, (jsint) i, &newVal);
		}
		break;
	}
	case GF_SG_VRML_MFSTRING:
	{
		MFString *f = (MFString *) field->far_ptr;
		obj = SMJS_CONSTRUCT_OBJECT(priv->js_ctx, &js_rt->MFStringClass, priv->js_obj);
		SETUP_MF_FIELD
		for (i=0; i<f->count; i++) {
			s = JS_NewStringCopyZ(priv->js_ctx, f->vals[i]);
			newVal = STRING_TO_JSVAL( s );
			JS_SetElement(priv->js_ctx, jsf->js_list, (jsint) i, &newVal);
		}
		break;
	}
	case GF_SG_VRML_MFURL:
	{
		MFURL *f = (MFURL *) field->far_ptr;
		obj = SMJS_CONSTRUCT_OBJECT(priv->js_ctx, &js_rt->MFUrlClass, priv->js_obj);
		SETUP_MF_FIELD
		for (i=0; i<f->count; i++) {
			if (f->vals[i].OD_ID > 0) {
				char msg[30];
				sprintf(msg, "od:%d", f->vals[i].OD_ID);
				s = JS_NewStringCopyZ(priv->js_ctx, (const char *) msg);
			} else {
				s = JS_NewStringCopyZ(priv->js_ctx, f->vals[i].url);
			}
			newVal = STRING_TO_JSVAL( s );
			JS_SetElement(priv->js_ctx, jsf->js_list, (jsint) i, &newVal);
		}
		break;
	}

	case GF_SG_VRML_MFVEC2F:
	{
		MFVec2f *f = (MFVec2f *) field->far_ptr;
		if (!obj) {
			obj = SMJS_CONSTRUCT_OBJECT(priv->js_ctx, &js_rt->MFVec2fClass, priv->js_obj);
			SETUP_MF_FIELD
		}
		for (i=0; i<f->count; i++) {
			JSObject *pf = JS_NewObject(priv->js_ctx, &js_rt->SFVec2fClass._class, 0, obj);
			newVal = OBJECT_TO_JSVAL(pf);
			slot = SFVec2f_Create(priv->js_ctx, pf, f->vals[i].x, f->vals[i].y);
			slot->owner = parent;
			JS_SetElement(priv->js_ctx, jsf->js_list, (jsint) i, &newVal);
		}
		break;
	}
	case GF_SG_VRML_MFVEC3F:
	{
		MFVec3f *f = (MFVec3f *) field->far_ptr;
		if (!obj) {
			obj = SMJS_CONSTRUCT_OBJECT(priv->js_ctx, &js_rt->MFVec3fClass, priv->js_obj);
			SETUP_MF_FIELD
		}
		for (i=0; i<f->count; i++) {
			JSObject *pf = JS_NewObject(priv->js_ctx, &js_rt->SFVec3fClass._class, 0, obj);
			newVal = OBJECT_TO_JSVAL(pf);
			slot = SFVec3f_Create(priv->js_ctx, pf, f->vals[i].x, f->vals[i].y, f->vals[i].z);
			slot->owner = parent;
			JS_SetElement(priv->js_ctx, jsf->js_list, (jsint) i, &newVal);
		}
		break;
	}
	case GF_SG_VRML_MFROTATION:
	{
		MFRotation *f = (MFRotation*) field->far_ptr;
		if (!obj) {
			obj = SMJS_CONSTRUCT_OBJECT(priv->js_ctx, &js_rt->MFRotationClass, priv->js_obj);
			SETUP_MF_FIELD
		}
		for (i=0; i<f->count; i++) {
			JSObject *pf = JS_NewObject(priv->js_ctx, &js_rt->SFRotationClass._class, 0, obj);
			newVal = OBJECT_TO_JSVAL(pf);
			slot = SFRotation_Create(priv->js_ctx, pf, f->vals[i].x, f->vals[i].y, f->vals[i].z, f->vals[i].q);
			slot->owner = parent;
			JS_SetElement(priv->js_ctx, jsf->js_list, (jsint) i, &newVal);
		}
		break;
	}
	case GF_SG_VRML_MFCOLOR:
	{
		MFColor *f = (MFColor *) field->far_ptr;
		if (!obj) {
			obj = SMJS_CONSTRUCT_OBJECT(priv->js_ctx, &js_rt->MFColorClass, priv->js_obj);
			SETUP_MF_FIELD
		}
		for (i=0; i<f->count; i++) {
			JSObject *pf = JS_NewObject(priv->js_ctx, &js_rt->SFColorClass._class, 0, obj);
			newVal = OBJECT_TO_JSVAL(pf);
			slot = SFColor_Create(priv->js_ctx, pf, f->vals[i].red, f->vals[i].green, f->vals[i].blue);
			slot->owner = parent;
			JS_SetElement(priv->js_ctx, jsf->js_list, (jsint) i, &newVal);
		}
		break;
	}

	case GF_SG_VRML_MFNODE:
	{
		u32 size;
		GF_ChildNodeItem *f = * ((GF_ChildNodeItem **)field->far_ptr);
		obj = SMJS_CONSTRUCT_OBJECT(priv->js_ctx, &js_rt->MFNodeClass, priv->js_obj);
		SETUP_MF_FIELD
		size = gf_node_list_get_count(f);

		if (JS_SetArrayLength(priv->js_ctx, jsf->js_list, size) != JS_TRUE) return JSVAL_NULL;
		/*get binding for all objects*/
		while (f) {
			node_get_binding(priv, f->node, 0);
			f = f->next;
		}

#if 0
		i=0;
		while (f) {
			JSObject *pf;
			slot = NULL;

			n = f->node;
			pf = node_get_binding(priv, n, 0);

			if (n->sgprivate->tag == TAG_ProtoNode) {
				GF_ProtoInstance *proto_inst = (GF_ProtoInstance *) n;
				if (!proto_inst->RenderingNode && !(proto_inst->flags&GF_SG_PROTO_LOADED) )
					gf_sg_proto_instantiate(proto_inst);
			}

			slot = SMJS_GET_PRIVATE(priv->js_ctx, pf);
			if (!slot->owner) slot->owner = parent;

			newVal = OBJECT_TO_JSVAL(pf);
			JS_SetElement(priv->js_ctx, jsf->js_list, (jsint) i, &newVal);
			f = f->next;
			i++;
		}
#endif
		break;
	}

	//not supported
	default:
		return JSVAL_NULL;
	}

	if (!obj) return JSVAL_NULL;
	//store field associated with object if needed
	if (jsf) {
		SMJS_SET_PRIVATE(priv->js_ctx, obj, jsf);
		/*if this is the obj corresponding to an existing field/node, store it and prevent GC on object*/
		if (parent) {

			/*remember the object*/
			if (!parent->sgprivate->interact) GF_SAFEALLOC(parent->sgprivate->interact, struct _node_interactive_ext);
			if (!parent->sgprivate->interact) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[VRMLJS] Failed to create interact storage\n"));
				return JSVAL_NULL;
			}
			if (!parent->sgprivate->interact->js_binding) {
				GF_SAFEALLOC(parent->sgprivate->interact->js_binding, struct _node_js_binding);
				if (!parent->sgprivate->interact->js_binding) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[VRMLJS] Failed to create JS bindings storage\n"));
					return JSVAL_NULL;
				}
				parent->sgprivate->interact->js_binding->fields = gf_list_new();
			}

			if ( gf_list_find(parent->sgprivate->interact->js_binding->fields, jsf) < 0) {
				assert(jsf->owner == parent);
				gf_list_add(parent->sgprivate->interact->js_binding->fields, jsf);
			}


			if (!jsf->obj) {
				jsf->obj = obj;
				assert(gf_list_find(priv->js_cache, obj)<0);
				gf_list_add(priv->js_cache, obj);
			}

			/*our JS Array object (MFXXX) are always rooted and added to the cache upon construction*/
			if (jsf->js_list) {
				u32 i;
				jsuint count;

				JS_GetArrayLength(jsf->js_ctx, jsf->js_list, &count);

				for (i=0; i<count; i++) {
					jsval item;
					JS_GetElement(jsf->js_ctx, jsf->js_list, (jsint) i, &item);
					if (JSVAL_IS_OBJECT(item)) {
						GF_JSField *afield = SMJS_GET_PRIVATE(jsf->js_ctx, JSVAL_TO_OBJECT(item));
						if (afield->owner != parent) continue;

						if ( gf_list_find(parent->sgprivate->interact->js_binding->fields, afield) < 0) {
							gf_list_add(parent->sgprivate->interact->js_binding->fields, afield);
						}
					}
				}
			}
			parent->sgprivate->flags |= GF_NODE_HAS_BINDING;
		}
	}
	return OBJECT_TO_JSVAL(obj);

}

static void JS_ReleaseRootObjects(GF_ScriptPriv *priv)
{
	while (gf_list_count(priv->js_cache)) {
		GF_JSField *jsf;
		/*we don't walk through the list since unprotecting an element could trigger GC which in turn could modify this list content*/
		JSObject *obj = gf_list_last(priv->js_cache);
		gf_list_rem_last(priv->js_cache);
		jsf = (GF_JSField *) SMJS_GET_PRIVATE(priv->js_ctx, obj);
		if (!jsf) continue;

		/*				!!! WARNING !!!

		SpiderMonkey GC is handled at the JSRuntime level, not at the JSContext level.
		Objects may not be finalized until the runtime is destroyed/GC'ed, which is not what we want.
		We therefore destroy by hand all SFNode/MFNode
		*/

		if (jsf->is_rooted) {
			if (jsf->js_list) {
				gf_js_remove_root(priv->js_ctx, &jsf->js_list, GF_JSGC_OBJECT);
			} else {
				gf_js_remove_root(priv->js_ctx, &jsf->obj, GF_JSGC_OBJECT);
			}
			jsf->is_rooted=0;
		}
		jsf->obj = NULL;

		if (jsf->js_list)
			array_finalize_ex(priv->js_ctx, obj, 0);
		else if (jsf->node)
			node_finalize_ex(priv->js_ctx, obj, 0);
		else
			jsf->js_ctx=NULL;
	}
}

static void JS_PreDestroy(GF_Node *node)
{
#if 0
	jsval fval, rval;
#endif
	GF_ScriptPriv *priv = node->sgprivate->UserPrivate;
	if (!priv) return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[Script] Destroying script node %s", gf_node_get_log_name(node) ));

	/*"shutdown" is no longer supported, as it is typically called when one of a parent node is destroyed through
	a GC call. Calling JS_LookupProperty or JS_CallFunctionValue when GC is running will crash SpiderMonkey*/
#if 0
	if (JS_LookupProperty(priv->js_ctx, priv->js_obj, "shutdown", &fval))
		if (! JSVAL_IS_VOID(fval))
			JS_CallFunctionValue(priv->js_ctx, priv->js_obj, fval, 0, NULL, &rval);
#endif

	gf_sg_lock_javascript(priv->js_ctx, 1);

	if (priv->the_event) gf_js_remove_root(priv->js_ctx, &priv->the_event, GF_JSGC_OBJECT);

	/*unprotect all cached objects from GC*/
	JS_ReleaseRootObjects(priv);

	gf_sg_load_script_extensions(node->sgprivate->scenegraph, priv->js_ctx, priv->js_obj, 1);

#ifndef GPAC_DISABLE_SVG
	gf_sg_js_dom_pre_destroy(priv->js_ctx, node->sgprivate->scenegraph, NULL);
#endif

	gf_sg_lock_javascript(priv->js_ctx, 0);

	gf_sg_ecmascript_del(priv->js_ctx);

#ifndef GPAC_DISABLE_SVG
	dom_js_unload();
#endif

	gf_list_del(priv->js_cache);

	priv->js_ctx = NULL;

	/*unregister script from parent scene (cf base_scenegraph::sg_reset) */
	gf_list_del_item(node->sgprivate->scenegraph->scripts, node);
}


static void JS_InitScriptFields(GF_ScriptPriv *priv, GF_Node *sc)
{
	u32 i;
	GF_ScriptField *sf;
	GF_FieldInfo info;
	jsval val;

	i=0;
	while ((sf = gf_list_enum(priv->fields, &i))) {

		switch (sf->eventType) {
		case GF_SG_EVENT_IN:
			gf_node_get_field(sc, sf->ALL_index, &info);
			/*val = */gf_sg_script_to_smjs_field(priv, &info, sc, 0);
			break;
		case GF_SG_EVENT_OUT:
			gf_node_get_field(sc, sf->ALL_index, &info);
			val = gf_sg_script_to_smjs_field(priv, &info, sc, 0);
			/*for native types directly modified*/
			JS_DefineProperty(priv->js_ctx, priv->js_obj, (const char *) sf->name, val, 0, gf_sg_script_eventout_set_prop, JSPROP_PERMANENT );
			break;
		default:
			gf_node_get_field(sc, sf->ALL_index, &info);
			val = gf_sg_script_to_smjs_field(priv, &info, sc, 0);
			JS_DefineProperty(priv->js_ctx, priv->js_obj, (const char *) sf->name, val, 0, 0, JSPROP_PERMANENT);
			break;
		}
	}
}

GF_EXPORT
void gf_js_vrml_flush_event_out(GF_Node *node, GF_ScriptPriv *priv)
{
	u32 i;
	GF_ScriptField *sf;

	/*flush event out*/
	i=0;
	while ((sf = gf_list_enum(priv->fields, &i))) {
		if (sf->activate_event_out) {
			sf->activate_event_out = 0;
			gf_node_event_out(node, sf->ALL_index);
#ifndef GPAC_DISABLE_SVG
			if (node->sgprivate->interact && node->sgprivate->interact->dom_evt) {
				GF_FieldInfo info;
				if (gf_node_get_field(node, sf->ALL_index, &info)==GF_OK)
					gf_node_changed(node, &info);
			}
#endif
		}
	}
}

static void JS_EventIn(GF_Node *node, GF_FieldInfo *in_field)
{
	jsval fval, rval;
	Double time;
	jsval argv[2];
	GF_ScriptField *sf;
	GF_FieldInfo t_info;
	GF_ScriptPriv *priv;
	u32 i;
	uintN attr;
	JSBool found;
	priv = node->sgprivate->UserPrivate;

	/*no support for change of static fields*/
	if (in_field->fieldIndex<3) return;

	i = (node->sgprivate->tag==TAG_MPEG4_Script) ? 3 : 4;
	sf = gf_list_get(priv->fields, in_field->fieldIndex - i);
	time = gf_node_get_scene_time(node);

	/*
	if (sf->last_route_time == time) return;
	*/
	sf->last_route_time = time;

	gf_sg_lock_javascript(priv->js_ctx, 1);

	//locate function
	if (!JS_LookupProperty(priv->js_ctx, priv->js_obj, sf->name, &fval) || JSVAL_IS_VOID(fval) ||
	        !JS_GetPropertyAttributes(priv->js_ctx, priv->js_obj, sf->name, &attr, &found) || found != JS_TRUE ) {
		gf_sg_lock_javascript(priv->js_ctx, 0);
		return;
	}

	argv[0] = gf_sg_script_to_smjs_field(priv, in_field, node, 1);

	memset(&t_info, 0, sizeof(GF_FieldInfo));
	t_info.far_ptr = &sf->last_route_time;
	t_info.fieldType = GF_SG_VRML_SFTIME;
	t_info.fieldIndex = -1;
	t_info.name = "timestamp";
	argv[1] = gf_sg_script_to_smjs_field(priv, &t_info, node, 1);

	/*protect args*/
	if (JSVAL_IS_GCTHING(argv[0])) gf_js_add_root(priv->js_ctx, &argv[0], GF_JSGC_VAL);
	if (JSVAL_IS_GCTHING(argv[1])) gf_js_add_root(priv->js_ctx, &argv[1], GF_JSGC_VAL);

	JS_CallFunctionValue(priv->js_ctx, priv->js_obj, fval, 2, argv, &rval);

	/*release args*/
	if (JSVAL_IS_GCTHING(argv[0])) gf_js_remove_root(priv->js_ctx, &argv[0], GF_JSGC_VAL);
	if (JSVAL_IS_GCTHING(argv[1])) gf_js_remove_root(priv->js_ctx, &argv[1], GF_JSGC_VAL);

	/*check any pending exception if outer-most event*/
	if ( (JS_IsRunning(priv->js_ctx)==JS_FALSE) && JS_IsExceptionPending(priv->js_ctx)) {
		JS_ReportPendingException(priv->js_ctx);
		JS_ClearPendingException(priv->js_ctx);
	}

	gf_sg_lock_javascript(priv->js_ctx, 0);

	gf_js_vrml_flush_event_out(node, priv);

	do_js_gc(priv->js_ctx, node);
}


static Bool vrml_js_load_script(M_Script *script, char *file, Bool primary_script, jsval *rval)
{
	FILE *jsf;
	char *jsscript;
	u64 fsize;
	Bool success = 1;
	JSBool ret;
	jsval fval;
	GF_ScriptPriv *priv = (GF_ScriptPriv *) script->sgprivate->UserPrivate;
	uintN attr;
	JSBool found;

	jsf = gf_fopen(file, "rb");
	if (!jsf) return 0;

	gf_fseek(jsf, 0, SEEK_END);
	fsize = gf_ftell(jsf);
	gf_fseek(jsf, 0, SEEK_SET);
	jsscript = gf_malloc(sizeof(char)*(size_t)(fsize+1));
	fsize = fread(jsscript, sizeof(char), (size_t)fsize, jsf);
	gf_fclose(jsf);
	jsscript[fsize] = 0;

	*rval = JSVAL_NULL;
	ret = JS_EvaluateScript(priv->js_ctx, priv->js_obj, jsscript, (u32) (sizeof(char)*fsize), file, 0, rval);
	if (ret==JS_FALSE) success = 0;

	if (success && primary_script
	        && JS_LookupProperty(priv->js_ctx, priv->js_obj, "initialize", &fval) && !JSVAL_IS_VOID(fval)
	        && JS_GetPropertyAttributes(priv->js_ctx, priv->js_obj, "initialize", &attr, &found) && found == JS_TRUE) {

		JS_CallFunctionValue(priv->js_ctx, priv->js_obj, fval, 0, NULL, rval);
		gf_js_vrml_flush_event_out((GF_Node *)script, priv);
	}
	gf_free(jsscript);
	return success;
}

/*fetches each listed URL and attempts to load the script - this is SYNCHRONOUS*/
Bool JSScriptFromFile(GF_Node *node, const char *opt_file, Bool no_complain, jsval *rval)
{
	GF_JSAPIParam par;
	u32 i;
	GF_DownloadManager *dnld_man;
	char *url;
	GF_Err e;
	const char *ext;
	M_Script *script = (M_Script *)node;

	e = GF_SCRIPT_ERROR;
	*rval = JSVAL_NULL;

	par.dnld_man = NULL;
	ScriptAction(NULL, node->sgprivate->scenegraph, GF_JSAPI_OP_GET_DOWNLOAD_MANAGER, NULL, &par);
	if (!par.dnld_man) return 0;
	dnld_man = par.dnld_man;

	for (i=0; i<script->url.count; i++) {
		char *_url = script->url.vals[i].script_text;
		if (opt_file) {
			if (strnicmp(_url+4, "script:", 7) && strnicmp(_url+5, "script:", 5)) {
				_url = gf_url_concatenate(script->url.vals[i].script_text, opt_file);
			} else {
				_url = gf_strdup(opt_file);
			}
		}
		par.uri.url = _url;
		par.uri.nb_params = 0;
		ScriptAction(NULL, node->sgprivate->scenegraph, GF_JSAPI_OP_RESOLVE_URI, node, &par);
		if (opt_file) gf_free(_url);
		url = (char *)par.uri.url;

		ext = strrchr(url, '.');
		if (ext && strnicmp(ext, ".js", 3)) {
			gf_free(url);
			continue;
		}

		if (!strstr(url, "://") || !strnicmp(url, "file://", 7)) {
			Bool res = vrml_js_load_script(script, url, opt_file ? 0 : 1, rval);
			gf_free(url);
			if (res) return 1;
			if (no_complain) return 0;
		} else {
			GF_DownloadSession *sess = gf_dm_sess_new(dnld_man, url, GF_NETIO_SESSION_NOT_THREADED, NULL, NULL, &e);
			if (sess) {
				e = gf_dm_sess_process(sess);
				if (e==GF_OK) {
					const char *szCache = gf_dm_sess_get_cache_name(sess);
					if (!vrml_js_load_script(script, (char *) szCache, opt_file ? 0 : 1, rval))
						e = GF_SCRIPT_ERROR;
				}
				gf_dm_sess_del(sess);
			}
			gf_free(url);
			if (!e) return 1;
		}
	}

	par.info.e = GF_SCRIPT_ERROR;
	par.info.msg = "Cannot fetch script";
	ScriptAction(NULL, node->sgprivate->scenegraph, GF_JSAPI_OP_MESSAGE, NULL, &par);
	return 0;
}


static void JSScript_LoadVRML(GF_Node *node)
{
	char *str;
	JSBool ret;
	u32 i;
	Bool local_script;
	jsval rval, fval;
	M_Script *script = (M_Script *)node;
	GF_ScriptPriv *priv = (GF_ScriptPriv *) node->sgprivate->UserPrivate;
	uintN attr;
	JSBool found;

	if (!priv || priv->is_loaded) return;
	if (!script->url.count) return;
	priv->is_loaded = 1;

	/*register script width parent scene (cf base_scenegraph::sg_reset) */
	gf_list_add(node->sgprivate->scenegraph->scripts, node);

	str = NULL;
	for (i=0; i<script->url.count; i++) {
		str = script->url.vals[i].script_text;
		while (strchr("\n\t ", str[0])) str++;

		if (!strnicmp(str, "javascript:", 11)) str += 11;
		else if (!strnicmp(str, "vrmlscript:", 11)) str += 11;
		else if (!strnicmp(str, "ecmascript:", 11)) str += 11;
		else if (!strnicmp(str, "mpeg4script:", 12)) str += 12;
		else str = NULL;
		if (str) break;
	}
	local_script = str ? 1 : 0;

	/*lock runtime and set current thread before creating the context*/
	priv->js_ctx = gf_sg_ecmascript_new(node->sgprivate->scenegraph);
	if (!priv->js_ctx) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[VRML JS] Cannot allocate ECMAScript context for node\n"));
		return;
	}

	gf_sg_lock_javascript(priv->js_ctx, 1);

	JS_SetContextPrivate(priv->js_ctx, node);
	gf_sg_script_init_sm_api(priv, node);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[VRML JS] Built-in classes initialized\n"));
#ifndef GPAC_DISABLE_SVG
	/*initialize DOM*/
	dom_js_load(node->sgprivate->scenegraph, priv->js_ctx, priv->js_obj);
	/*create event object, and remember it*/
	priv->the_event = dom_js_define_event(priv->js_ctx, priv->js_obj);
	gf_js_add_root(priv->js_ctx, &priv->the_event, GF_JSGC_OBJECT);
#endif

	gf_sg_load_script_extensions(node->sgprivate->scenegraph, priv->js_ctx, priv->js_obj, 0);

	priv->js_cache = gf_list_new();

	/*setup fields interfaces*/
	JS_InitScriptFields(priv, node);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[VRML JS] script fields initialized\n"));

	priv->JS_PreDestroy = JS_PreDestroy;
	priv->JS_EventIn = JS_EventIn;

	if (!local_script) {
		JSScriptFromFile(node, NULL, 0, &rval);
		gf_sg_lock_javascript(priv->js_ctx, 0);
		return;
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[VRML JS] Evaluating script %s\n", str));

	ret = JS_EvaluateScript(priv->js_ctx, priv->js_obj, str, (u32) strlen(str), 0, 0, &rval);
	if (ret==JS_TRUE) {
		/*call initialize if present*/
		if (JS_LookupProperty(priv->js_ctx, priv->js_obj, "initialize", &fval) && !JSVAL_IS_VOID(fval)
		        && JS_GetPropertyAttributes(priv->js_ctx, priv->js_obj, "initialize", &attr, &found) && found == JS_TRUE)

			JS_CallFunctionValue(priv->js_ctx, priv->js_obj, fval, 0, NULL, &rval);
		gf_js_vrml_flush_event_out(node, priv);
	}

	gf_sg_lock_javascript(priv->js_ctx, 0);

	do_js_gc(priv->js_ctx, node);
}

static void JSScript_Load(GF_Node *node)
{
#ifndef GPAC_DISABLE_SVG
	void JSScript_LoadSVG(GF_Node *node);
#endif

	switch (node->sgprivate->tag) {
	case TAG_MPEG4_Script:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Script:
#endif
		JSScript_LoadVRML(node);
		break;
#ifndef GPAC_DISABLE_SVG
	case TAG_SVG_script:
	case TAG_SVG_handler:
		JSScript_LoadSVG(node);
		break;
#endif
	default:
		break;
	}
}


static void JSScript_NodeModified(GF_SceneGraph *sg, GF_Node *node, GF_FieldInfo *info, GF_Node *script)
{
	u32 i;
	GF_JSField *jsf;
	/*this is REPLACE NODE signaling*/
	if (script) {
		u32 count;
		GF_ScriptPriv *priv = gf_node_get_private(script);
		count = gf_list_count(priv->js_cache);
		for (i=0; i<count; i++) {
			JSObject *obj = gf_list_get(priv->js_cache, i);
			jsf = SMJS_GET_PRIVATE(priv->js_ctx, obj);
			if (jsf->node && (jsf->node==node)) {
				jsf->node = NULL;
				/*Ivica patch*/
				node->sgprivate->interact->js_binding->node = NULL;
			}
		}
		return;
	}

	if (!info) {
		/*handle DOM case*/
		if ((node->sgprivate->tag>=GF_NODE_FIRST_PARENT_NODE_TAG)
		        && node->sgprivate->interact
		        && node->sgprivate->interact->js_binding
		        && node->sgprivate->interact->js_binding->node)
		{

			if (gf_list_del_item(sg->objects, node->sgprivate->interact->js_binding->node)>=0) {
#ifndef GPAC_DISABLE_SVG
				gf_js_remove_root(sg->svg_js->js_ctx, &(node->sgprivate->interact->js_binding->node), GF_JSGC_OBJECT);
				if (sg->svg_js->in_script)
					sg->svg_js->force_gc = 1;
				else
				{
					gf_sg_js_call_gc(sg->svg_js->js_ctx);
					//invalid with firefox>7: JS_ClearNewbornRoots(sg->svg_js->js_ctx);
				}
#endif
			}
			return;
		}
		/*this is unregister signaling*/

#if 0
		if (node->sgprivate->parents) {
			switch (node->sgprivate->parents->node->sgprivate->tag) {
			case TAG_MPEG4_Script:
#ifndef GPAC_DISABLE_X3D
			case TAG_X3D_Script:
#endif
				/*node is no longer used in the graph, remove root and let the script handle it, */
				if (node->sgprivate->interact->js_binding->node) {
					GF_JSField *field = node->sgprivate->interact->js_binding->node;
					if (field->is_rooted) {
						gf_js_remove_root(field->js_ctx, &field->obj);
						field->is_rooted = 0;
					}
				}
				break;
			}
			return;
		}
#else
		if (!node->sgprivate->parents && node->sgprivate->interact && node->sgprivate->interact->js_binding && node->sgprivate->interact->js_binding->node) {
			GF_JSField *field = node->sgprivate->interact->js_binding->node;
			if (field->is_rooted) {
				gf_js_remove_root(field->js_ctx, &field->obj, GF_JSGC_OBJECT);
				field->is_rooted = 0;
			}
			return;
		}
#endif

		/*final destroy*/
		if (!node->sgprivate->num_instances) {
			i=0;
			while (node->sgprivate->interact && node->sgprivate->interact->js_binding && (jsf = gf_list_enum(node->sgprivate->interact->js_binding->fields, &i))) {
				jsf->owner = NULL;

				if (jsf->js_list) {
					u32 j;

					if (jsf->field.fieldType != GF_SG_VRML_MFNODE) {
						jsuint count;
						JS_GetArrayLength(jsf->js_ctx, jsf->js_list, &count);

						for (j=0; j<count; j++) {
							jsval item;
							JS_GetElement(jsf->js_ctx, jsf->js_list, (jsint) j, &item);
							if (JSVAL_IS_OBJECT(item)) {
								GF_JSField *afield = SMJS_GET_PRIVATE(jsf->js_ctx, JSVAL_TO_OBJECT(item));
								afield->owner = NULL;
							}
						}
					}

					if (jsf->is_rooted) gf_js_remove_root(jsf->js_ctx, &jsf->js_list, GF_JSGC_OBJECT);
					jsf->js_list = NULL;
				}
			}
		}
		return;
	}
	/*this is field modification signaling*/
	if (!node->sgprivate->interact || !node->sgprivate->interact->js_binding) return;
	i=0;
	while ((jsf = gf_list_enum(node->sgprivate->interact->js_binding->fields, &i))) {
		if ((jsf->field.fieldIndex == info->fieldIndex) && (jsf->field.fieldType == info->fieldType)) {
			jsf->field.NDTtype = 1;

			/*if field is a script field, rewrite it right away because script fields are exposed
			as global variables within the script

			We also have a special value '-1' for the NDT for addChildren and removeChildren*/

#if 0
			if ((info->NDTtype == (u32) -1) || (node == (GF_Node*)JS_GetScript(jsf->js_ctx))) {
				gf_sg_script_update_cached_object( JS_GetScriptStack(jsf->js_ctx), jsf->obj, jsf, &jsf->field, node);
			}
#endif
			return;
		}
	}
}

GF_EXPORT
void gf_sg_handle_dom_event_for_vrml(GF_Node *node, GF_DOM_Event *event, GF_Node *observer)
{
#ifndef GPAC_DISABLE_SVG
	GF_ScriptPriv *priv;
	Bool prev_type;
	//JSBool ret = JS_FALSE;
	GF_DOM_Event *prev_event = NULL;
	SVG_handlerElement *hdl;
	jsval rval;
	JSObject *evt;
	jsval argv[1];

	hdl = (SVG_handlerElement *) node;
	if (!hdl->js_fun_val && !hdl->evt_listen_obj) return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_INTERACT, ("[DOM Events] Executing script code from VRML handler\n"));

	priv = JS_GetScriptStack(hdl->js_context);
	gf_sg_lock_javascript(priv->js_ctx, 1);

	prev_event = SMJS_GET_PRIVATE(priv->js_ctx, priv->the_event);
	/*break loops*/
	if (prev_event && (prev_event->type==event->type) && (prev_event->target==event->target)) {
		gf_sg_lock_javascript(priv->js_ctx, 0);
		return;
	}

	evt = gf_dom_new_event(priv->js_ctx);
	if (!evt) {
		gf_sg_lock_javascript(priv->js_ctx, 0);
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[DOM Events] Cannot create JavaScript dom event for event type %d\n", event->type));
		return;
	}

	prev_type = event->is_vrml;
	event->is_vrml = 1;
	SMJS_SET_PRIVATE(priv->js_ctx, priv->the_event, event);

	SMJS_SET_PRIVATE(priv->js_ctx, evt, event);
	argv[0] = OBJECT_TO_JSVAL(evt);

	if (hdl->js_fun_val) {
		jsval funval = (jsval ) hdl->js_fun_val;
		/*ret = */JS_CallFunctionValue(priv->js_ctx, hdl->evt_listen_obj ? (JSObject *)hdl->evt_listen_obj: priv->js_obj, funval, 1, argv, &rval);
	} else {
		/*ret = */JS_CallFunctionName(priv->js_ctx, hdl->evt_listen_obj, "handleEvent", 1, argv, &rval);
	}

	/*check any pending exception if outer-most event*/
	if (!prev_event && JS_IsExceptionPending(priv->js_ctx)) {
		JS_ReportPendingException(priv->js_ctx);
		JS_ClearPendingException(priv->js_ctx);
	}

	event->is_vrml = prev_type;
	SMJS_SET_PRIVATE(priv->js_ctx, priv->the_event, prev_event);

	gf_sg_lock_javascript(priv->js_ctx, 0);

#endif
}

#endif	/*GPAC_DISABLE_VRML*/

#endif /*GPAC_HAS_SPIDERMONKEY*/



/*set JavaScript interface*/
void gf_sg_set_script_action(GF_SceneGraph *scene, gf_sg_script_action script_act, void *cbk)
{
	if (!scene) return;
	scene->script_action = script_act;
	scene->script_action_cbck = cbk;

#if defined(GPAC_HAS_SPIDERMONKEY) && !defined(GPAC_DISABLE_VRML)
	scene->script_load = JSScript_Load;
	scene->on_node_modified = JSScript_NodeModified;
#endif

}

#ifdef GPAC_HAS_SPIDERMONKEY

GF_EXPORT
GF_Node *gf_sg_js_get_node(JSContext *c, JSObject *obj)
{
#ifndef GPAC_DISABLE_VRML
	if (js_rt && GF_JS_InstanceOf(c, obj, &js_rt->SFNodeClass, NULL) ) {
		GF_JSField *ptr = (GF_JSField *) SMJS_GET_PRIVATE(c, obj);
		if (ptr->field.fieldType==GF_SG_VRML_SFNODE) return * ((GF_Node **)ptr->field.far_ptr);
	}
#endif

#ifndef GPAC_DISABLE_SVG
	{
		JSBool has_p = 0;
		if (JS_HasProperty(c, obj, "namespaceURI", &has_p)) {
			if (has_p==JS_TRUE) return dom_get_element(c, obj);
		}
	}
#endif
	return NULL;
}

#endif

GF_EXPORT
Bool gf_sg_has_scripting()
{
#ifdef GPAC_HAS_SPIDERMONKEY
	return 1;
#else
	return 0;
#endif
}


#ifdef GPAC_HAS_SPIDERMONKEY

/*
 * locking/try-locking the JS context
 * we need to test whether the calling thread already has the lock on the script context
 * if this is not the case (i.e. first lock on mutex), we switch JS context threads and
 * call begin/end requests. Nesting begin/end request in a reentrant way crashes JS
 * (mozilla doc is wrong here)
 *
 * */
GF_EXPORT
void gf_sg_lock_javascript(struct JSContext *cx, Bool LockIt)
{
	if (!js_rt) return;

	if (LockIt) {
		gf_mx_p(js_rt->mx);
#ifdef JS_THREADSAFE
		assert(cx);
		if (gf_mx_get_num_locks(js_rt->mx)==1) {
#if (JS_VERSION>=185)
			JS_SetRuntimeThread(js_rt->js_runtime);
			JS_SetContextThread(cx);
#endif
			JS_BeginRequest(cx);
		}
#endif
	} else {
#ifdef JS_THREADSAFE
		assert(cx);
		if (gf_mx_get_num_locks(js_rt->mx)==1) {
			JS_EndRequest(cx);
#if (JS_VERSION>=185)
			JS_ClearContextThread(cx);
			JS_ClearRuntimeThread(js_rt->js_runtime);
#endif
		}
#endif
		gf_mx_v(js_rt->mx);
	}
}

GF_EXPORT
Bool gf_sg_try_lock_javascript(struct JSContext *cx)
{
	assert(cx);
	if (gf_mx_try_lock(js_rt->mx)) {
#ifdef JS_THREADSAFE
		if (gf_mx_get_num_locks(js_rt->mx)==1) {
#if (JS_VERSION>=185)
			JS_SetRuntimeThread(js_rt->js_runtime);
			JS_SetContextThread(cx);
#endif
			JS_BeginRequest(cx);
		}
#endif
		return 1;
	}
	return 0;
}

#endif /* GPAC_HAS_SPIDERMONKEY */

GF_Err gf_scene_execute_script(GF_SceneGraph *sg, const char *com)
{
#if defined(GPAC_HAS_SPIDERMONKEY) && !defined(GPAC_DISABLE_SVG)
	u32 tag;
	GF_Err e;
	GF_Node *root = gf_sg_get_root_node(sg);
	if (root) {
		tag = gf_node_get_tag(root);
		if (tag >= GF_NODE_RANGE_FIRST_SVG) {
			GF_SVGJS *svg_js = sg->svg_js;
			Bool ret = svg_js->script_execute(sg, (char *)com, NULL);
			return (ret == GF_TRUE ? GF_OK : GF_BAD_PARAM);
		} else {
			e = GF_NOT_SUPPORTED;
			return e;
		}
	}
	return GF_BAD_PARAM;
#else
	return GF_NOT_SUPPORTED;
#endif
}
