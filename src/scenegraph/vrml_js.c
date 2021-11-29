/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2019
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


#ifdef GPAC_HAS_QJS

#include <gpac/internal/compositor_dev.h>
#include <gpac/modules/compositor_ext.h>

#include "qjs_common.h"

typedef struct
{

	u8 is_setup;
	/*set to 1 for proto IS fields*/
	u8 IS_route;
	/*set to 1 for JS route to fun*/
	u8 script_route;

	u32 ID;
	char *name;

	/*scope of this route*/
	GF_SceneGraph *graph;
	u32 lastActivateTime;

	GF_Node *FromNode;
	GF_FieldInfo FromField;

	GF_Node *ToNode;
	GF_FieldInfo ToField;

	JSValue obj;
	JSValue fun;
} GF_RouteToScript;

typedef struct __gf_js_field
{
	GF_FieldInfo field;
	GF_Node *owner;
	JSValue obj;

	/*list of JSValue * for MFFields (except MFNode) or NULL
	we need this to keep the link between an MF slot and the parent node since everything is passed by reference*/
	JSValue *mfvals;
	u32 mfvals_count;

	/*pointer to the SFNode if this is an SFNode or MFNode[i] field */
	GF_Node *node;
	/*when creating MFnode from inside the script, the node list is stored here until attached to an object*/
	GF_ChildNodeItem *temp_list;
	/*only set when not owned by a node, in which case field.far_ptr is also set to this value*/
	void *field_ptr;

	/*context in which the field was created*/
	struct JSContext *js_ctx;
} GF_JSField;

JSValue js_throw_err_msg(JSContext *ctx, s32 err, const char *fmt, ...)
{
    JSValue obj = JS_NewError(ctx);
    if (JS_IsException(obj)) {
        obj = JS_NULL;
	} else {
    	JS_DefinePropertyValueStr(ctx, obj, "code", JS_NewInt32(ctx, err), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
    	if (fmt) {
			char szMsg[2050];
			va_list vl;
			va_start(vl, fmt);
			vsnprintf(szMsg, 2048, fmt, vl);
			va_end(vl);

    		JS_DefinePropertyValueStr(ctx, obj, "message", JS_NewString(ctx, szMsg), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
		}
	}
    return JS_Throw(ctx, obj);
}

JSValue js_throw_err(JSContext *ctx, s32 err)
{
    JSValue obj = JS_NewError(ctx);
    if (JS_IsException(obj)) {
        obj = JS_NULL;
	} else {
    	JS_DefinePropertyValueStr(ctx, obj, "code", JS_NewInt32(ctx, err), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
	}
    return JS_Throw(ctx, obj);
}


#define _ScriptMessage(_c, _msg) {	\
		GF_Node *_n = (GF_Node *) JS_GetContextOpaque(_c);	\
		if (_n->sgprivate->scenegraph->script_action) {\
			GF_JSAPIParam par;	\
			par.info.e = GF_SCRIPT_INFO;			\
			par.info.msg = (_msg);		\
			_n->sgprivate->scenegraph->script_action(_n->sgprivate->scenegraph->script_action_cbck, GF_JSAPI_OP_MESSAGE, NULL, &par);\
		}	\
		}

#ifndef GPAC_DISABLE_PLAYER
static Bool ScriptAction(JSContext *c, GF_SceneGraph *scene, u32 type, GF_Node *node, GF_JSAPIParam *param)
{
	if (!scene) {
		GF_Node *n = (GF_Node *) JS_GetContextOpaque(c);
		scene = n->sgprivate->scenegraph;
	}
	if (scene->script_action)
		return scene->script_action(scene->script_action_cbck, type, node, param);
	return 0;
}
#endif// GPAC_DISABLE_PLAYER

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


#ifndef GPAC_DISABLE_SVG
/*SVG tags for script handling*/
#include <gpac/nodes_svg.h>

GF_Node *dom_get_element(JSContext *c, JSValue obj);
#endif


void gf_sg_script_to_node_field(struct JSContext *c, JSValue v, GF_FieldInfo *field, GF_Node *owner, GF_JSField *parent);
JSValue gf_sg_script_to_qjs_field(GF_ScriptPriv *priv, GF_FieldInfo *field, GF_Node *parent, Bool force_evaluate);

#ifndef GPAC_DISABLE_PLAYER
static void JSScript_NodeModified(GF_SceneGraph *sg, GF_Node *node, GF_FieldInfo *info, GF_Node *script);
#endif


Bool JSScriptFromFile(GF_Node *node, const char *opt_file, Bool no_complain, JSValue *rval);

#ifndef GPAC_DISABLE_SVG
static JSValue vrml_event_add_listener(JSContext *c, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue vrml_event_remove_listener(JSContext *c, JSValueConst this_val, int argc, JSValueConst *argv);
#endif // GPAC_DISABLE_SVG

void do_js_gc(JSContext *c, GF_Node *node)
{
#ifdef FORCE_GC
	node->sgprivate->scenegraph->trigger_gc = GF_TRUE;
#endif

	if (node->sgprivate->scenegraph->trigger_gc) {
		node->sgprivate->scenegraph->trigger_gc = GF_FALSE;
		gf_js_call_gc(c);
	}
}

#ifndef GPAC_DISABLE_VRML

/*MPEG4 & X3D tags (for node tables & script handling)*/
#include <gpac/nodes_mpeg4.h>
#include <gpac/nodes_x3d.h>

static void SFColor_fromHSV(SFColor *col)
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

static void SFColor_toHSV(SFColor *col)
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
	ptr->obj = JS_UNDEFINED;
	return ptr;
}

static GFINLINE M_Script *JS_GetScript(JSContext *c)
{
	return (M_Script *) JS_GetContextOpaque(c);
}
static GFINLINE GF_ScriptPriv *JS_GetScriptStack(JSContext *c)
{
	M_Script *script = (M_Script *) JS_GetContextOpaque(c);
	return script->sgprivate->UserPrivate;
}


static JSValue getName(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return JS_NewString(ctx, "GPAC");
}
static JSValue getVersion(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return JS_NewString(ctx, gf_gpac_version());
}
static JSValue getCurrentSpeed(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSAPIParam par;
	GF_Node *node = JS_GetContextOpaque(ctx);
	par.time = 0;
	ScriptAction(ctx, NULL, GF_JSAPI_OP_GET_SPEED, node->sgprivate->scenegraph->RootNode, &par);
	return JS_NewFloat64(ctx, par.time);
}
static JSValue getCurrentFrameRate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSAPIParam par;
	GF_Node *node = JS_GetContextOpaque(ctx);
	par.time = 0;
	ScriptAction(ctx, NULL, GF_JSAPI_OP_GET_FPS, node->sgprivate->scenegraph->RootNode, &par);
	return JS_NewFloat64(ctx, par.time);
}
static JSValue getWorldURL(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSAPIParam par;
	GF_Node *node = JS_GetContextOpaque(ctx);
	par.uri.url = NULL;
	par.uri.nb_params = 0;
	if (ScriptAction(ctx, NULL, GF_JSAPI_OP_RESOLVE_URI, node->sgprivate->scenegraph->RootNode, &par)) {
		JSValue ret = JS_NewString(ctx, par.uri.url);
		gf_free(par.uri.url);
		return ret;
	}
	return JS_UNDEFINED;
}

static JSValue node_get_binding(GF_ScriptPriv *priv, GF_Node *node)
{
	GF_JSField *field;

	if (!node) return JS_NULL;

	if (node->sgprivate->interact && node->sgprivate->interact->js_binding && node->sgprivate->interact->js_binding->pf) {
		field = node->sgprivate->interact->js_binding->pf;
		assert(JS_IsObject(field->obj));
		assert(JS_GetOpaque(field->obj, SFNodeClass.class_id)!=NULL);
		return field->obj;
	}

	field = NewJSField(priv->js_ctx);
	field->field.fieldType = GF_SG_VRML_SFNODE;
	field->node = node;
	field->field.far_ptr = &field->node;


	node->sgprivate->flags |= GF_NODE_HAS_BINDING;
	gf_node_register(node, NULL);

	field->obj = JS_NewObjectClass(priv->js_ctx, SFNodeClass.class_id);
	JS_SetOpaque(field->obj, field);
	gf_list_add(priv->jsf_cache, field);

	/*remember the object*/
	if (!node->sgprivate->interact) {
		GF_SAFEALLOC(node->sgprivate->interact, struct _node_interactive_ext);
		if (!node->sgprivate->interact) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[VRMLJS] Failed to create interact storage\n"));
			return GF_JS_EXCEPTION(priv->js_ctx);
		}
	}
	if (!node->sgprivate->interact->js_binding) {
		GF_SAFEALLOC(node->sgprivate->interact->js_binding, struct _node_js_binding);
		if (!node->sgprivate->interact->js_binding) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[VRMLJS] Failed to create JS bindings storage\n"));
			return GF_JS_EXCEPTION(priv->js_ctx);
		}
		node->sgprivate->interact->js_binding->fields = gf_list_new();
	}
	node->sgprivate->flags |= GF_NODE_HAS_BINDING;
	node->sgprivate->interact->js_binding->pf = field;
	return field->obj;
}

static JSValue getScript(JSContext *c, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_ScriptPriv *priv = JS_GetScriptStack(c);
	GF_Node *node = JS_GetContextOpaque(c);
	return JS_DupValue(c, node_get_binding(priv, node) );
}

static JSValue loadScript(JSContext *c, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Bool no_complain = 0;
	const char *url;
	GF_Node *node = JS_GetContextOpaque(c);
	JSValue aval = JS_UNDEFINED;
	if (!node || !argc || !JS_IsString(argv[0])) return GF_JS_EXCEPTION(c);

	if ((argc>1) && JS_IsBool(argv[1])) no_complain = (JS_ToBool(c,argv[1])) ? 1 : 0;

	url = JS_ToCString(c, argv[0]);
	if (url) {
		JSScriptFromFile(node, url, no_complain, &aval);
	}
	JS_FreeCString(c, url);
	return aval;
}

static JSValue getProto(JSContext *c, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_ScriptPriv *priv = JS_GetScriptStack(c);
	GF_Node *node = JS_GetContextOpaque(c);

	if (!node->sgprivate->scenegraph->pOwningProto) {
		return JS_NULL;
	}
	node = (GF_Node *) node->sgprivate->scenegraph->pOwningProto;
	return JS_DupValue(c, node_get_binding(priv, node));
}

#ifndef GPAC_DISABLE_SVG
GF_Node *gf_sm_load_svg_from_string(GF_SceneGraph *sg, char *svg_str);
#endif

static JSValue vrml_parse_xml(JSContext *c, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifndef GPAC_DISABLE_SVG
	GF_SceneGraph *sg;
	GF_Node *node;
	const char *str;

	str = JS_ToCString(c, argv[0]);
	if (!str) return JS_TRUE;

	node = JS_GetContextOpaque(c);
	sg = node->sgprivate->scenegraph;

	node = gf_sm_load_svg_from_string(sg, (char *) str);
	JS_FreeCString(c, str);
	return dom_element_construct(c, node);
#else
	return GF_JS_EXCEPTION(c);
#endif
}


static JSValue getElementById(JSContext *c, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Node *elt;
	const char *name = NULL;
	u32 ID = 0;
	GF_ScriptPriv *priv = JS_GetScriptStack(c);
	GF_Node *sc = JS_GetContextOpaque(c);
	if (JS_IsString(argv[0])) name = JS_ToCString(c, argv[0]);
	else if (JS_IsInteger(argv[0])) {
		if (JS_ToInt32(c, &ID, argv[0]))
			return GF_JS_EXCEPTION(c);
	}
	if (!ID && !name) return GF_JS_EXCEPTION(c);

	elt = NULL;
	if (ID) elt = gf_sg_find_node(sc->sgprivate->scenegraph, ID);
	else elt = gf_sg_find_node_by_name(sc->sgprivate->scenegraph, (char *) name);

	JS_FreeCString(c, name);
	return JS_DupValue(c, node_get_binding(priv, elt));
}

static JSValue replaceWorld(JSContext *c, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return JS_TRUE;
}

static void on_route_to_object(GF_Node *node, GF_Route *_r)
{
	JSValue argv[2], rval;
	Double time;
	GF_FieldInfo t_info;
	GF_ScriptPriv *priv;
	JSValue obj;
	GF_RouteToScript *r = (GF_RouteToScript *)_r;
	if (!node)
		return;
	priv = gf_node_get_private(node);
	if (!priv)
		return;

	if (!r->FromNode) {
		JS_FreeValue(priv->js_ctx, r->fun);
		r->fun = JS_UNDEFINED;
		JS_FreeValue(priv->js_ctx, r->obj);
		r->obj = JS_UNDEFINED;
		return;
	}

	obj = r->obj;
	if (JS_IsUndefined(obj) || JS_IsNull(obj))
	 	obj = priv->js_obj;

	memset(&t_info, 0, sizeof(GF_FieldInfo));
	time = gf_node_get_scene_time(node);
	t_info.far_ptr = &time;
	t_info.fieldType = GF_SG_VRML_SFTIME;
	t_info.fieldIndex = -1;
	t_info.name = "timestamp";

	gf_js_lock(priv->js_ctx, 1);
	
	argv[1] = gf_sg_script_to_qjs_field(priv, &t_info, node, 1);
	argv[0] = gf_sg_script_to_qjs_field(priv, &r->FromField, r->FromNode, 1);

	rval = JS_Call(priv->js_ctx, r->fun, obj, 2, argv);
	if (JS_IsException(rval))
		js_dump_error(priv->js_ctx);

	JS_FreeValue(priv->js_ctx, argv[0]);
	JS_FreeValue(priv->js_ctx, argv[1]);
	JS_FreeValue(priv->js_ctx, rval);

	js_std_loop(priv->js_ctx);
	gf_js_lock(priv->js_ctx, 0);

	do_js_gc(priv->js_ctx, node);
}

static JSValue addRoute(JSContext *c, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSField *ptr;
	GF_Node *n1, *n2;
	const char *f1;
	GF_FieldInfo info;
	u32 f_id1, f_id2;
	GF_Err e;
	if (argc!=4) return GF_JS_EXCEPTION(c);
	ptr = (GF_JSField *) JS_GetOpaque(argv[0], SFNodeClass.class_id);
	if (!ptr) return GF_JS_EXCEPTION(c);

	assert(ptr->field.fieldType==GF_SG_VRML_SFNODE);
	n1 = * ((GF_Node **)ptr->field.far_ptr);
	if (!n1) return GF_JS_EXCEPTION(c);
	n2 = NULL;

	if (!JS_IsString(argv[1])) return GF_JS_EXCEPTION(c);
	f1 = JS_ToCString(c, argv[1]);
	if (!f1) return JS_FALSE;
	if (!strnicmp(f1, "_field", 6)) {
		f_id1 = atoi(f1+6);
		e = gf_node_get_field(n1, f_id1, &info);
	} else {
		e = gf_node_get_field_by_name(n1, (char *) f1, &info);
		f_id1 = info.fieldIndex;
	}
	JS_FreeCString(c, f1);
	if (e != GF_OK) return GF_JS_EXCEPTION(c);


	if (!JS_IsObject(argv[2])) return GF_JS_EXCEPTION(c);

	ptr = (GF_JSField *) JS_GetOpaque(argv[2], SFNodeClass.class_id);

	/*regular route*/
	if (ptr && JS_IsString(argv[3]) ) {
		const char *f2;
		GF_Route *r;
		assert(ptr->field.fieldType==GF_SG_VRML_SFNODE);
		n2 = * ((GF_Node **)ptr->field.far_ptr);
		if (!n2) return GF_JS_EXCEPTION(c);

		f2 = JS_ToCString(c, argv[3]);
		if (!f2) return GF_JS_EXCEPTION(c);

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
				if (gf_node_get_field_by_name(n2, (char *) f2, &info) != GF_OK) {
					gf_sg_script_field_new(n2, GF_SG_SCRIPT_TYPE_EVENT_IN, src.fieldType, f2);
				}
			}
			e = gf_node_get_field_by_name(n2, (char *) f2, &info);
			f_id2 = info.fieldIndex;
		}
		JS_FreeCString(c, f2);
		if (e != GF_OK) return GF_JS_EXCEPTION(c);

		r = gf_sg_route_new(n1->sgprivate->scenegraph, n1, f_id1, n2, f_id2);
		if (!r) return GF_JS_EXCEPTION(c);
	}
	/*route to object*/
	else {
		u32 i = 0;
		const char *fun_name;
		JSAtom atom;
		GF_RouteToScript *r = NULL;
		if (!JS_IsFunction(c, argv[3]) ) return GF_JS_EXCEPTION(c);

		atom = JS_ValueToAtom(c, argv[3]);
		fun_name = JS_AtomToCString(c, atom);
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
		JS_FreeCString(c, fun_name);
		JS_FreeAtom(c, atom);

		if ( !r ) {
			GF_SAFEALLOC(r, GF_RouteToScript)
			if (!r) return JS_FALSE;
			r->script_route = 1;
			r->FromNode = n1;
			r->FromField.fieldIndex = f_id1;
			gf_node_get_field(r->FromNode, f_id1, &r->FromField);

			r->ToNode = (GF_Node*)JS_GetScript(c);
			r->ToField.fieldType = GF_SG_VRML_SCRIPT_FUNCTION;
			r->ToField.on_event_in = on_route_to_object;
			r->ToField.eventType = GF_SG_EVENT_IN;
			r->ToField.far_ptr = NULL;
			r->ToField.name = fun_name;

			r->obj = JS_DupValue(c, argv[2]);
			r->fun = JS_DupValue(c, argv[3]);

			r->is_setup = 1;
			r->graph = n1->sgprivate->scenegraph;

			if (!n1->sgprivate->interact) {
				GF_SAFEALLOC(n1->sgprivate->interact, struct _node_interactive_ext);
				if (!n1->sgprivate->interact) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[VRMLJS] Failed to create interact storage\n"));
					return GF_JS_EXCEPTION(c);
				}
			}
			if (!n1->sgprivate->interact->routes) n1->sgprivate->interact->routes = gf_list_new();
			gf_list_add(n1->sgprivate->interact->routes, r);
			gf_list_add(n1->sgprivate->scenegraph->Routes, r);
		}
	}

	return JS_UNDEFINED;
}

static JSValue deleteRoute(JSContext *c, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSField *ptr;
	GF_Node *n1, *n2;
	const char *f1, *f2;
	GF_FieldInfo info;
	GF_RouteToScript *rts;
	GF_Err e;
	u32 f_id1, f_id2, i;
	if (argc!=4) return JS_FALSE;

	if (!JS_IsObject(argv[0]) || JS_IsNull(argv[0]))
	 	return GF_JS_EXCEPTION(c);

	ptr = JS_GetOpaque(argv[0], SFNodeClass.class_id);
	if (!ptr) return GF_JS_EXCEPTION(c);
	assert(ptr->field.fieldType==GF_SG_VRML_SFNODE);

	if (JS_IsString(argv[1]) && JS_IsNull(argv[2]) && JS_IsNull(argv[3])) {
		n1 = * ((GF_Node **)ptr->field.far_ptr);
		f1 = JS_ToCString(c, argv[1]);
		if (!strcmp(f1, "ALL")) {
			while (n1->sgprivate->interact && n1->sgprivate->interact->routes && gf_list_count(n1->sgprivate->interact->routes) ) {
				GF_Route *r = gf_list_get(n1->sgprivate->interact->routes, 0);
				gf_sg_route_del(r);
			}
		}
		JS_FreeCString(c, f1);
		return JS_UNDEFINED;
	}
	if (!JS_IsString(argv[1]) || !JS_IsString(argv[3])) return GF_JS_EXCEPTION(c);

	n1 = * ((GF_Node **)ptr->field.far_ptr);

	ptr = JS_GetOpaque(argv[2], SFNodeClass.class_id);
	if (!ptr) return GF_JS_EXCEPTION(c);
	assert(ptr->field.fieldType==GF_SG_VRML_SFNODE);
	n2 = * ((GF_Node **)ptr->field.far_ptr);

	if (!n1 || !n2) return GF_JS_EXCEPTION(c);
	if (!n1->sgprivate->interact) return JS_UNDEFINED;

	f1 = JS_ToCString(c, argv[1]);
	f2 = JS_ToCString(c, argv[3]);
	if (!f1 || !f2) {
		JS_FreeCString(c, f1);
		JS_FreeCString(c, f2);
		return GF_JS_EXCEPTION(c);
	}

	if (!strnicmp(f1, "_field", 6)) {
		f_id1 = atoi(f1+6);
		e = gf_node_get_field(n1, f_id1, &info);
	} else {
		e = gf_node_get_field_by_name(n1, (char *)f1, &info);
		f_id1 = info.fieldIndex;
	}
	JS_FreeCString(c, f1);
	if (e != GF_OK) return GF_JS_EXCEPTION(c);

	if (!strnicmp(f2, "_field", 6)) {
		f_id2 = atoi(f2+6);
		e = gf_node_get_field(n2, f_id2, &info);
	} else {
		e = gf_node_get_field_by_name(n2, (char *)f2, &info);
		f_id2 = info.fieldIndex;
	}
	JS_FreeCString(c, f2);
	if (e != GF_OK) return GF_JS_EXCEPTION(c);

	i=0;
	while ((rts = gf_list_enum(n1->sgprivate->interact->routes, &i))) {
		if (rts->FromField.fieldIndex != f_id1) continue;
		if (rts->ToNode != n2) continue;
		if (rts->ToField.fieldIndex != f_id2) continue;
		JS_FreeValue(c, rts->fun);
		JS_FreeValue(c, rts->obj);
		gf_sg_route_del((GF_Route *) rts);
		return JS_UNDEFINED;
	}
	return JS_UNDEFINED;
}

static JSValue loadURL(JSContext *c, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 i;
	GF_JSAPIParam par;
	GF_JSField *f;
	M_Script *script = (M_Script *) JS_GetContextOpaque(c);

	if (argc < 1) return GF_JS_EXCEPTION(c);

	if (JS_IsString(argv[0])) {
		Bool res;
		par.uri.url = (char *) JS_ToCString(c, argv[0]);
		/*TODO add support for params*/
		par.uri.nb_params = 0;
		res = ScriptAction(c, NULL, GF_JSAPI_OP_LOAD_URL, (GF_Node *)script, &par);
		JS_FreeCString(c, par.uri.url);
		return res ? JS_TRUE : JS_FALSE;
	}
	if (!JS_IsObject(argv[0])) return GF_JS_EXCEPTION(c);

	f = (GF_JSField *) JS_GetOpaque_Nocheck(argv[0]);
	if (!f || !f->mfvals) return GF_JS_EXCEPTION(c);

	for (i=0; i<f->mfvals_count; i++) {
		Bool res=GF_FALSE;
		JSValue item = f->mfvals[i];

		if (JS_IsString(item)) {
			par.uri.url = (char *) JS_ToCString(c, item);
			/*TODO add support for params*/
			par.uri.nb_params = 0;
			res = ScriptAction(c, NULL, GF_JSAPI_OP_LOAD_URL, (GF_Node*)script, &par);
			JS_FreeCString(c, par.uri.url);
		}
		if (res) return JS_TRUE;
	}
	return JS_FALSE;
}

static JSValue setDescription(JSContext *c, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_JSAPIParam par;
	GF_Node *node = JS_GetContextOpaque(c);
	if (!argc || !JS_IsString(argv[0])) return GF_JS_EXCEPTION(c);
	par.uri.url = (char *) JS_ToCString(c, argv[0]);
	ScriptAction(c, NULL, GF_JSAPI_OP_SET_TITLE, node->sgprivate->scenegraph->RootNode, &par);
	JS_FreeCString(c, par.uri.url);
	return JS_TRUE;
}

static JSValue createVrmlFromString(JSContext *c, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifndef GPAC_DISABLE_LOADER_BT
	GF_ScriptPriv *priv;
	GF_FieldInfo field;
	JSValue res;
	/*BT/VRML from string*/
	GF_List *gf_sm_load_bt_from_string(GF_SceneGraph *in_scene, char *node_str, Bool force_wrl);
	const char *str;
	GF_List *nlist;
	GF_Node *sc_node = JS_GetContextOpaque(c);
	if (!sc_node || (argc < 1)) return GF_JS_EXCEPTION(c);

	if (!JS_IsString(argv[0])) return GF_JS_EXCEPTION(c);
	str = JS_ToCString(c, argv[0]);
	nlist = gf_sm_load_bt_from_string(sc_node->sgprivate->scenegraph, (char *)str, 1);
	JS_FreeCString(c, str);
	if (!nlist) return GF_JS_EXCEPTION(c);

	priv = JS_GetScriptStack(c);
	memset(&field, 0, sizeof(GF_FieldInfo));
	field.fieldType = GF_SG_VRML_MFNODE;
	field.far_ptr = &nlist;
	res = gf_sg_script_to_qjs_field(priv, &field, NULL, 0);

	/*don't forget to unregister all this stuff*/
	while (gf_list_count(nlist)) {
		GF_Node *n = gf_list_get(nlist, 0);
		gf_list_rem(nlist, 0);
		gf_node_unregister(n, NULL);
	}
	gf_list_del(nlist);
	return res;
#else
	return GF_JS_EXCEPTION(c);
#endif
}

void gf_node_event_out_proto(GF_Node *node, u32 FieldIndex);

void Script_FieldChanged(JSContext *c, GF_Node *parent, GF_JSField *parent_owner, GF_FieldInfo *field)
{
	u32 script_field;
	u32 i;

	if (!parent && parent_owner) {
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
		if ( (GF_Node *) JS_GetContextOpaque(c) == parent) script_field = 2;
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
		GF_ScriptPriv *priv = parent ? parent->sgprivate->UserPrivate : parent_owner->owner->sgprivate->UserPrivate;
		GF_ScriptField *sf;
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

static JSValue gf_sg_script_eventout_set_prop(JSContext *c, JSValueConst this_val, JSValueConst val, int magic)
{
	u32 i;
	GF_ScriptPriv *script;
	GF_Node *n;
	GF_ScriptField *sf;

	script = JS_GetScriptStack(c);
	if (!script) return GF_JS_EXCEPTION(c);
	n = (GF_Node *) JS_GetScript(c);
	if (!n) return GF_JS_EXCEPTION(c);

	i=0;
	while ((sf = gf_list_enum(script->fields, &i))) {
		if (sf->magic==magic) {
			GF_FieldInfo info;
			gf_node_get_field(n, sf->ALL_index, &info);
			gf_sg_script_to_node_field(c, val, &info, n, NULL);
			sf->activate_event_out = 1;
			return JS_UNDEFINED;
		}
	}
	return GF_JS_EXCEPTION(c);
}


/*generic ToString method*/
static GFINLINE void sffield_toString(char **str, void *f_ptr, u32 fieldType)
{
	char temp[1000];

	switch (fieldType) {
	case GF_SG_VRML_SFVEC2F:
	{
		SFVec2f val = * ((SFVec2f *) f_ptr);
		sprintf(temp, "%f %f", FIX2FLT(val.x), FIX2FLT(val.y));
		gf_dynstrcat(str, temp, NULL);
		break;
	}
	case GF_SG_VRML_SFVEC3F:
	{
		SFVec3f val = * ((SFVec3f *) f_ptr);
		sprintf(temp, "%f %f %f", FIX2FLT(val.x), FIX2FLT(val.y), FIX2FLT(val.z));
		gf_dynstrcat(str, temp, NULL);
		break;
	}
	case GF_SG_VRML_SFROTATION:
	{
		SFRotation val = * ((SFRotation *) f_ptr);
		sprintf(temp, "%f %f %f %f", FIX2FLT(val.x), FIX2FLT(val.y), FIX2FLT(val.z), FIX2FLT(val.q));
		gf_dynstrcat(str, temp, NULL);
		break;
	}
	case GF_SG_VRML_SFCOLOR:
	{
		SFColor val = * ((SFColor *) f_ptr);
		sprintf(temp, "%f %f %f", FIX2FLT(val.red), FIX2FLT(val.green), FIX2FLT(val.blue));
		gf_dynstrcat(str, temp, NULL);
		break;
	}
	case GF_SG_VRML_SFIMAGE:
	{
		SFImage *val = ((SFImage *)f_ptr);
		sprintf(temp, "%dx%dx%d", val->width, val->height, val->numComponents);
		gf_dynstrcat(str, temp, NULL);
		break;
	}

	}
}

static void JS_ObjectDestroyed(JSRuntime *rt, JSValue obj, GF_JSField *ptr, Bool is_js_call)
{
	JS_SetOpaque(obj, NULL);
	if (!ptr) return;

	/*if ptr is a node, remove node binding*/
	if (ptr->node
			&& ptr->node->sgprivate->interact
			&& ptr->node->sgprivate->interact->js_binding
			&& (ptr->node->sgprivate->interact->js_binding->pf == ptr)
	   ) {
		ptr->node->sgprivate->interact->js_binding->pf = NULL;
	}

	/*if ptr is a field, remove field binding from parent*/
	if (ptr->owner && ptr->owner->sgprivate->interact && ptr->owner->sgprivate->interact->js_binding) {
		gf_list_del_item(ptr->owner->sgprivate->interact->js_binding->fields, ptr);
	}

	/*
		If object is still registered, remove it from the js_cache
	*/
	if (!JS_IsUndefined(ptr->obj) && is_js_call) {
		if (ptr->js_ctx) {
			GF_ScriptPriv *priv;
			if (!gs_js_context_is_valid(ptr->js_ctx))
				return;
			priv = JS_GetScriptStack(ptr->js_ctx);
			gf_list_del_item(priv->jsf_cache, ptr);
		}
		ptr->obj = JS_UNDEFINED;
	}
}


static JSValue field_toString(JSContext *c, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 i;
	JSValue item;
	Double d;
	char *str = NULL;
	GF_JSField *f = JS_GetOpaque_Nocheck(this_val);
	if (!f) return JS_FALSE;

	if (gf_sg_vrml_is_sf_field(f->field.fieldType)) {
		sffield_toString(&str, f->field.far_ptr, f->field.fieldType);
	} else {
		if (f->field.fieldType == GF_SG_VRML_MFNODE) return JS_TRUE;

		gf_dynstrcat(&str, "[", NULL);

		for (i=0; i<f->mfvals_count; i++) {
			char temp[1000];
			s32 ival;
			item = f->mfvals[i];
			switch (f->field.fieldType) {
			case GF_SG_VRML_MFBOOL:
				sprintf(temp, "%s", JS_ToBool(c, item) ? "TRUE" : "FALSE");
				gf_dynstrcat(&str, temp, NULL);
				break;
			case GF_SG_VRML_MFINT32:
				JS_ToInt32(c, &ival, item);
				sprintf(temp, "%d", ival);
				gf_dynstrcat(&str, temp, NULL);
				break;
			case GF_SG_VRML_MFFLOAT:
			case GF_SG_VRML_MFTIME:
				JS_ToFloat64(c, &d, item);
				sprintf(temp, "%g", d);
				gf_dynstrcat(&str, temp, NULL);
				break;
			case GF_SG_VRML_MFSTRING:
			case GF_SG_VRML_MFURL:
			{
				char *str_val = (char *) JS_ToCString(c, item);
				gf_dynstrcat(&str, str_val, NULL);
				JS_FreeCString(c, str_val);
			}
			break;
			default:
				if (JS_IsObject(item)) {
					GF_JSField *sf = (GF_JSField *) JS_GetOpaque_Nocheck(item);
					sffield_toString(&str, sf->field.far_ptr, sf->field.fieldType);
				}
				break;
			}
			if (i < f->mfvals_count-1) gf_dynstrcat(&str, ", ", NULL);
		}
		gf_dynstrcat(&str, "]", NULL);
	}
	item = JS_NewString(c, str ? str : "");
	if (str) gf_free(str);
	return item;
}

static JSValue SFNodeConstructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	u32 tag, ID;
	GF_Node *new_node;
	GF_JSField *field;
	GF_Proto *proto;
	GF_SceneGraph *sg;
	char *node_name;
	GF_ScriptPriv *priv = JS_GetScriptStack(c);
	M_Script *sc = JS_GetScript(c);

	if (argc && !JS_IsString(argv[0]))
		return GF_JS_EXCEPTION(c);

	tag = 0;
	if (!argc) {
		JSValue obj = JS_NewObjectClass(c, SFNodeClass.class_id);
		if (JS_IsException(obj)) return obj;
		field = NewJSField(c);
		field->field.fieldType = GF_SG_VRML_SFNODE;
		field->node = NULL;
		field->field.far_ptr = &field->node;
		JS_SetOpaque(obj, field);
		return obj;
	}

	ID = 0;
	node_name = (char *) JS_ToCString(c, argv[0]);
	if (!node_name) return GF_JS_EXCEPTION(c);

	if (!strnicmp(node_name, "_proto", 6)) {
		ID = atoi(node_name+6);
		JS_FreeCString(c, node_name);
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
			JS_FreeCString(c, node_name);
			return JS_FALSE;
		}
		/* create interface and load code in current graph*/
		new_node = gf_sg_proto_create_instance(sc->sgprivate->scenegraph, proto);
		if (!new_node) {
			JS_FreeCString(c, node_name);
			return JS_FALSE;
		}
		/*OK, instantiate proto code*/
		if (gf_sg_proto_load_code(new_node) != GF_OK) {
			gf_node_unregister(new_node, NULL);
			JS_FreeCString(c, node_name);
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
			JS_FreeCString(c, node_name);
			return JS_FALSE;
		}
		gf_node_init(new_node);
	}

	JS_FreeCString(c, node_name);

	return JS_DupValue(c, node_get_binding(priv, new_node));
}
static void node_finalize_ex(JSRuntime *rt, JSValue obj, Bool is_js_call)
{
	GF_JSField *ptr = (GF_JSField *) JS_GetOpaque(obj, SFNodeClass.class_id);
	JS_ObjectDestroyed(rt, obj, ptr, is_js_call);
	if (!ptr) return;

	if (ptr->node
			/*num_instances may be 0 if the node is the script being destroyed*/
			&& ptr->node->sgprivate->num_instances
	   ) {

		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[VRML JS] unregistering node %s (%s)\n", gf_node_get_name(ptr->node), gf_node_get_class_name(ptr->node)));

		gf_node_unregister(ptr->node, NULL);
	}
	gf_free(ptr);
}

static void node_finalize(JSRuntime *rt, JSValue val)
{
	node_finalize_ex(rt, val, GF_TRUE);
	
}

static JSValue node_toString(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	char str[1000];
	u32 id;
	GF_Node *n;
	GF_JSField *f;
	const char *name;

	f = (GF_JSField *) JS_GetOpaque(obj, SFNodeClass.class_id);
	if (!f) return GF_JS_EXCEPTION(c);

	str[0] = 0;
	n = * ((GF_Node **)f->field.far_ptr);
	if (!n) return GF_JS_EXCEPTION(c);

	name = gf_node_get_name_and_id(n, &id);
	if (id) {
		if (name) {
			snprintf(str, 500, "DEF %s ", name);
		} else {
			snprintf(str, 500, "DEF %d ", id - 1);
		}
	}
	strncat(str, gf_node_get_class_name(n), 500);
	return JS_NewString(c, (const char *) str);
}

static JSValue node_getTime(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_Node *n;
	GF_JSField *f;
	f = (GF_JSField *) JS_GetOpaque(obj, SFNodeClass.class_id);
	if (!f) return GF_JS_EXCEPTION(c);
	n = * ((GF_Node **)f->field.far_ptr);
	if (!n) return GF_JS_EXCEPTION(c);
	return JS_NewFloat64(c, gf_node_get_scene_time(n));
}

static JSValue node_getProperty(JSContext *c, JSValueConst obj, JSAtom atom, JSValueConst receiver)
{
	GF_Node *n;
	u32 index;
	JSValue res;
	const char *fieldName;
	GF_FieldInfo info;
	GF_JSField *ptr;
	GF_ScriptPriv *priv;

	ptr = (GF_JSField *) JS_GetOpaque(obj, SFNodeClass.class_id);
	if (!ptr) return GF_JS_EXCEPTION(c);
	assert(ptr->field.fieldType==GF_SG_VRML_SFNODE);
	n = * ((GF_Node **)ptr->field.far_ptr);
	if (!n) return GF_JS_EXCEPTION(c);
	priv = JS_GetScriptStack(c);

	fieldName = JS_AtomToCString(c, atom);
	if (!fieldName) return GF_JS_EXCEPTION(c);

	if (!strcmp(fieldName, "toString")) {
		JS_FreeCString(c, fieldName);
		return JS_DupValue(priv->js_ctx, priv->node_toString_fun);
	}
	if (!strcmp(fieldName, "getTime")) {
		JS_FreeCString(c, fieldName);
		return JS_DupValue(priv->js_ctx, priv->node_getTime_fun);
	}
	if (!strcmp(fieldName, "addEventListener") || !strcmp(fieldName, "addEventListenerNS")) {
		JS_FreeCString(c, fieldName);
		return JS_DupValue(priv->js_ctx, priv->node_addEventListener_fun);
	}
	if (!strcmp(fieldName, "removeEventListener") || !strcmp(fieldName, "removeEventListenerNS")) {
		JS_FreeCString(c, fieldName);
		return JS_DupValue(priv->js_ctx, priv->node_removeEventListener_fun);
	}
	/*fieldID indexing*/
	if (!strnicmp(fieldName, "_field", 6)) {
		index = atoi(fieldName+6);
		if ( gf_node_get_field(n, index, &info) == GF_OK) {
			res = gf_sg_script_to_qjs_field(priv, &info, n, 0);
			JS_FreeCString(c, fieldName);
			return res;
		}
	} else if ( gf_node_get_field_by_name(n, (char *) fieldName, &info) == GF_OK) {
		res = gf_sg_script_to_qjs_field(priv, &info, n, 0);
		JS_FreeCString(c, fieldName);
		return res;
	}

	if (!strcmp(fieldName, "_bounds")) {
		GF_JSAPIParam par;
		par.bbox.is_set = 0;
		if (ScriptAction(c, n->sgprivate->scenegraph, GF_JSAPI_OP_GET_LOCAL_BBOX, (GF_Node *)n, &par) ) {
			JSValue _obj = JS_NewObjectClass(priv->js_ctx, AnyClass.class_id);
			Float x, y, w, h;
			x = y = w = h = 0;
			if (par.bbox.is_set) {
				x = FIX2FLT(par.bbox.min_edge.x);
				y = FIX2FLT(par.bbox.min_edge.y);
				w = FIX2FLT(par.bbox.max_edge.x - par.bbox.min_edge.x);
				h = FIX2FLT(par.bbox.max_edge.y - par.bbox.min_edge.y);
			}
			JS_SetPropertyStr(priv->js_ctx, _obj, "x", JS_NewFloat64(c, x));
			JS_SetPropertyStr(priv->js_ctx, _obj, "y", JS_NewFloat64(c, y));
			JS_SetPropertyStr(priv->js_ctx, _obj, "width", JS_NewFloat64(c, w));
			JS_SetPropertyStr(priv->js_ctx, _obj, "height", JS_NewFloat64(c, h));
			JS_FreeCString(c, fieldName);
			return _obj;
		}
	}
	JS_FreeCString(c, fieldName);
	return JS_UNDEFINED;
}

static int node_setProperty(JSContext *c, JSValueConst obj, JSAtom atom, JSValueConst value, JSValueConst receiver, int flags)
{
	GF_Node *n;
	GF_FieldInfo info;
	u32 index;
	const char *fieldname;
	Bool isOK = GF_FALSE;
	GF_JSField *ptr;

	ptr = (GF_JSField *) JS_GetOpaque(obj, SFNodeClass.class_id);
	if (! ptr) return -1;
	fieldname = JS_AtomToCString(c, atom);
	if (!fieldname) return -1;

	assert(ptr->field.fieldType==GF_SG_VRML_SFNODE);
	n = * ((GF_Node **)ptr->field.far_ptr);

	/*fieldID indexing*/
	if (!strnicmp(fieldname, "_field", 6)) {
		index = atoi(fieldname+6);
		JS_FreeCString(c, fieldname);
		if ( gf_node_get_field(n, index, &info) == GF_OK) {
			isOK = GF_TRUE;
		}
	} else {
		if (gf_node_get_field_by_name(n, (char *)fieldname, &info) == GF_OK) {
			isOK = GF_TRUE;
		} else {
			/*VRML style*/
			if (!strnicmp(fieldname, "set_", 4)) {
				if (gf_node_get_field_by_name(n, (char *) fieldname + 4, &info) == GF_OK) {
					isOK = GF_TRUE;
				}
			}
		}
	}
	if (!isOK) {
		JS_FreeCString(c, fieldname);
		JS_DefinePropertyValue(c, obj, atom, JS_DupValue(c, value), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
		return 0;
	}

	JS_FreeCString(c, fieldname);

	if (gf_node_get_tag(n)==TAG_ProtoNode)
		gf_sg_proto_mark_field_loaded(n, &info);

	gf_sg_script_to_node_field(c, value, &info, n, ptr);
	return 1;
}


/* Generic field destructor */
static void field_finalize(JSRuntime *rt, JSValue obj)
{
	GF_JSField *ptr = (GF_JSField *) JS_GetOpaque_Nocheck(obj);
	JS_ObjectDestroyed(rt, obj, ptr, 1);
	if (!ptr) return;

	if (ptr->field_ptr) gf_sg_vrml_field_pointer_del(ptr->field_ptr, ptr->field.fieldType);
	gf_free(ptr);
}



/*SFImage class functions */
static GFINLINE GF_JSField *SFImage_Create(JSContext *c, JSValue obj, u32 w, u32 h, u32 nbComp, MFInt32 *pixels)
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
	JS_SetOpaque(obj, field);
	return field;
}

static JSValue SFImageConstructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	u32 w, h, nbComp;
	MFInt32 *pixels;
	JSValue obj;
	if (argc<4) return GF_JS_EXCEPTION(c);
	if (!JS_IsInteger(argv[0]) || !JS_IsInteger(argv[1]) || !JS_IsInteger(argv[2]) || !JS_IsObject(argv[3]) )
		return GF_JS_EXCEPTION(c);

	pixels = JS_GetOpaque(argv[3], MFInt32Class.class_id);
	if (!pixels) return GF_JS_EXCEPTION(c);

	obj = JS_NewObjectClass(c, SFImageClass.class_id);
	if (JS_IsException(obj)) return obj;
	JS_ToInt32(c, &w, argv[0]);
	JS_ToInt32(c, &h, argv[1]);
	JS_ToInt32(c, &nbComp, argv[2]);
	SFImage_Create(c, obj, w, h, nbComp, pixels);
	return obj;
}

static JSValue image_getProperty(JSContext *c, JSValueConst this_val, int magic)
{
	SFImage *sfi;
	GF_ScriptPriv *priv = JS_GetScriptStack(c);
	GF_JSField *val = (GF_JSField *) JS_GetOpaque(this_val, SFImageClass.class_id);
	if (!val) return GF_JS_EXCEPTION(c);
	sfi = (SFImage*)val->field.far_ptr;

	switch (magic) {
	case 0: return JS_NewInt32(c, sfi->width);
	case 1: return JS_NewInt32(c, sfi->height);
	case 2: return JS_NewInt32(c, sfi->numComponents);
	case 3:
	{
		u32 i, len;
		JSValue an_obj = JS_NewObjectClass(c, MFInt32Class.class_id);
		len = sfi->width*sfi->height*sfi->numComponents;
		for (i=0; i<len; i++) {
			JS_SetPropertyUint32(priv->js_ctx, an_obj, i, JS_NewInt32(c, sfi->pixels[i]) );
		}
	}
		break;
	default:
		break;
	}
	return JS_UNDEFINED;
}

static JSValue image_setProperty(JSContext *c, JSValueConst obj, JSValueConst value, int magic)
{
	u32 ival;
	Bool changed = 0;
	SFImage *sfi;
	GF_JSField *ptr = (GF_JSField *) JS_GetOpaque(obj, SFImageClass.class_id);
	if (!ptr) return GF_JS_EXCEPTION(c);

	sfi = (SFImage*)ptr->field.far_ptr;
	switch (magic) {
	case 0:
		JS_ToInt32(c, &ival, value);
		changed = ! (sfi->width == ival);
		sfi->width = ival;
		if (changed && sfi->pixels) {
			gf_free(sfi->pixels);
			sfi->pixels = NULL;
		}
		break;
	case 1:
		JS_ToInt32(c, &ival, value);
		changed = ! (sfi->height == ival);
		sfi->height = ival;
		if (changed && sfi->pixels) {
			gf_free(sfi->pixels);
			sfi->pixels = NULL;
		}
		break;
	case 2:
		JS_ToInt32(c, &ival, value);
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
		GF_JSField *sf;
		u32 len, i;
		sf = JS_GetOpaque(value, MFInt32Class.class_id);
		if (!sf) return GF_JS_EXCEPTION(c);
		pixels = (MFInt32 *) sf->field.far_ptr;
		if (sfi->pixels) gf_free(sfi->pixels);
		len = sfi->width*sfi->height*sfi->numComponents;
		sfi->pixels = (unsigned char *) gf_malloc(sizeof(char)*len);
		len = MAX(len, pixels->count);
		for (i=0; i<len; i++) sfi->pixels[i] = (u8) pixels->vals[i];
		changed = 1;
		break;
	}
	default:
		return JS_UNDEFINED;
	}
	if (changed) Script_FieldChanged(c, NULL, ptr, NULL);
	return JS_UNDEFINED;
}

/*SFVec2f class functions */
static GFINLINE GF_JSField *SFVec2f_Create(JSContext *c, JSValue obj, Fixed x, Fixed y)
{
	GF_JSField *field;
	SFVec2f *v;
	field = NewJSField(c);
	v = gf_sg_vrml_field_pointer_new(GF_SG_VRML_SFVEC2F);
	field->field_ptr = field->field.far_ptr = v;
	field->field.fieldType = GF_SG_VRML_SFVEC2F;
	v->x = x;
	v->y = y;
	JS_SetOpaque(obj, field);
	return field;
}

static JSValue SFVec2fConstructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	Double x = 0.0, y = 0.0;
	JSValue obj = JS_NewObjectClass(c, SFVec2fClass.class_id);

	if (argc > 0) JS_ToFloat64(c, &x, argv[0]);
	if (argc > 1) JS_ToFloat64(c, &y, argv[1]);
	SFVec2f_Create(c, obj, FLT2FIX( x), FLT2FIX( y));
	return obj;
}

static JSValue vec2f_getProperty(JSContext *c, JSValueConst obj, int magic)
{
	GF_JSField *val = (GF_JSField *) JS_GetOpaque(obj, SFVec2fClass.class_id);
	if (!val) return GF_JS_EXCEPTION(c);

	switch (magic) {
	case 0:
		return JS_NewFloat64(c, FIX2FLT( ((SFVec2f*)val->field.far_ptr)->x));
	case 1:
		return JS_NewFloat64(c, FIX2FLT( ((SFVec2f*)val->field.far_ptr)->y));
	default:
		break;
	}
	return JS_UNDEFINED;
}

static JSValue vec2f_setProperty(JSContext *c, JSValueConst obj, JSValueConst value, int magic)
{
	Double d;
	Fixed v;
	Bool changed = 0;
	GF_JSField *ptr = (GF_JSField *) JS_GetOpaque(obj, SFVec2fClass.class_id);
	if (!ptr) return GF_JS_EXCEPTION(c);

	if (JS_ToFloat64(c, &d, value)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[VRML JS] Value is not a number while assigning SFVec2f\n"));
		return JS_FALSE;
	}
	switch (magic) {
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
		return JS_UNDEFINED;
	}
	if (changed) Script_FieldChanged(c, NULL, ptr, NULL);
	return JS_UNDEFINED;
}

static JSValue vec2f_operand(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv, u32 op)
{
	SFVec2f *v1, *v2=NULL;
	Double d = 0.0;
	JSValue pNew;
	Fixed v;
	GF_JSField *p1 = (GF_JSField *) JS_GetOpaque(obj, SFVec2fClass.class_id);
	if (!p1) return GF_JS_EXCEPTION(c);
	GF_JSField *p2 = NULL;

	if (argc) {
		if (JS_IsObject(argv[0])) {
			p2 = (GF_JSField *) JS_GetOpaque(argv[0], SFVec2fClass.class_id);
		} else if (JS_ToFloat64(c, &d, argv[0]))
			return GF_JS_EXCEPTION(c);
	}

	v1 = ((GF_JSField *) p1)->field.far_ptr;
	if (p2)
		v2 = ((GF_JSField *) p2)->field.far_ptr;

	switch (op) {
	case 5:
		return JS_NewFloat64(c, FIX2FLT(gf_v2d_len(v1)) );
	case 7:
		if (!p2) return GF_JS_EXCEPTION(c);
		return JS_NewFloat64(c, FIX2FLT( gf_mulfix(v1->x, v2->x) + gf_mulfix(v1->y, v2->y) ) );
	case 0:
	case 1:
		if (!p2) return GF_JS_EXCEPTION(c);
	}

	pNew = JS_NewObjectClass(c, SFVec2fClass.class_id);
	switch (op) {
	case 0:
		SFVec2f_Create(c, pNew, v1->x + v2->x, v1->y + v2->y);
		break;
	case 1:
		SFVec2f_Create(c, pNew, v1->x - v2->x, v1->y - v2->y);
		break;
	case 2:
		SFVec2f_Create(c, pNew, -v1->x , -v1->y );
		break;
	case 3:
		v = FLT2FIX(d);
		SFVec2f_Create(c, pNew, gf_mulfix(v1->x , v), gf_mulfix(v1->y, v) );
		break;
	case 4:
		v = FLT2FIX(d);
		SFVec2f_Create(c, pNew, gf_divfix(v1->x, v),  gf_divfix(v1->y, v));
		break;
	case 6:
		v = gf_v2d_len(v1);
		SFVec2f_Create(c, pNew, gf_divfix(v1->x, v), gf_divfix(v1->y, v) );
		break;
	}
	return pNew;
}

static JSValue vec2f_add(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return vec2f_operand(c, obj, argc, argv, 0);
}
static JSValue vec2f_subtract(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return vec2f_operand(c, obj, argc, argv, 1);
}
static JSValue vec2f_negate(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return vec2f_operand(c, obj, argc, argv, 2);
}
static JSValue vec2f_multiply(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return vec2f_operand(c, obj, argc, argv, 3);
}
static JSValue vec2f_divide(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return vec2f_operand(c, obj, argc, argv, 4);
}
static JSValue vec2f_length(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return vec2f_operand(c, obj, argc, argv, 5);
}
static JSValue vec2f_normalize(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return vec2f_operand(c, obj, argc, argv, 6);
}
static JSValue vec2f_dot(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return vec2f_operand(c, obj, argc, argv, 7);
}


/*SFVec3f class functions */
static GFINLINE GF_JSField *SFVec3f_Create(JSContext *c, JSValue obj, Fixed x, Fixed y, Fixed z)
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
	JS_SetOpaque(obj, field);
	return field;
}

static JSValue SFVec3fConstructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	Double x = 0.0, y = 0.0, z = 0.0;
	JSValue obj = JS_NewObjectClass(c, SFVec3fClass.class_id);

	if (argc > 0) JS_ToFloat64(c, &x, argv[0]);
	if (argc > 1) JS_ToFloat64(c, &y, argv[1]);
	if (argc > 2) JS_ToFloat64(c, &z, argv[2]);
	SFVec3f_Create(c, obj, FLT2FIX( x), FLT2FIX( y), FLT2FIX( z));
	return obj;
}

static JSValue vec3f_getProperty(JSContext *c, JSValueConst obj, int magic)
{
	GF_JSField *val = (GF_JSField *) JS_GetOpaque(obj, SFVec3fClass.class_id);
	if (!val) return GF_JS_EXCEPTION(c);

	switch (magic) {
	case 0:
		return JS_NewFloat64(c, FIX2FLT( ((SFVec3f*)val->field.far_ptr)->x));
	case 1:
		return JS_NewFloat64(c, FIX2FLT( ((SFVec3f*)val->field.far_ptr)->y));
	case 2:
		return JS_NewFloat64(c, FIX2FLT( ((SFVec3f*)val->field.far_ptr)->z));
	}
	return JS_UNDEFINED;
}

static JSValue vec3f_setProperty(JSContext *c, JSValueConst obj, JSValueConst value, int magic)
{
	Double d;
	Fixed v;
	Bool changed = 0;
	GF_JSField *ptr = (GF_JSField *) JS_GetOpaque(obj, SFVec3fClass.class_id);
	if (!ptr) return GF_JS_EXCEPTION(c);

	if (JS_ToFloat64(c, &d, value)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[VRML JS] Value is not a number while assigning SFVec3f\n"));
		return JS_FALSE;
	}

	switch (magic) {
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
		return JS_UNDEFINED;
	}
	if (changed) Script_FieldChanged(c, NULL, ptr, NULL);
	return JS_UNDEFINED;
}


static JSValue vec3f_operand(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv, u32 op)
{
	SFVec3f vec, wvec, *v1, *v2;
	Double d=0;
	JSValue pNew;
	Fixed v;
	GF_JSField *p1 = (GF_JSField *) JS_GetOpaque(obj, SFVec3fClass.class_id);
	if (!p1) return GF_JS_EXCEPTION(c);
	GF_JSField *p2 = NULL;

	if (argc) {
		if (JS_IsObject(argv[0])) {
			p2 = (GF_JSField *) JS_GetOpaque(argv[0], SFVec3fClass.class_id);
		} else if (JS_ToFloat64(c, &d, argv[0]))
			return GF_JS_EXCEPTION(c);
	}

	v1 = ((GF_JSField *) p1)->field.far_ptr;
	if (p2)
		v2 = ((GF_JSField *) p2)->field.far_ptr;

	switch (op) {
	case 0:
	case 1:
	case 8:
		if (!p2) return GF_JS_EXCEPTION(c);
	case 5:
		return JS_NewFloat64(c, FIX2FLT(gf_vec_len(*v1)) );
	case 7:
		vec = *v1;
		if (!p2) return GF_JS_EXCEPTION(c);
		wvec = *v2;
		return JS_NewFloat64(c, FIX2FLT(gf_vec_dot(vec, wvec)) );
	}

	pNew = JS_NewObjectClass(c, SFVec3fClass.class_id);
	switch (op) {
	case 0:
		SFVec3f_Create(c, pNew, v1->x + v2->x, v1->y + v2->y, v1->z + v2->z);
		break;
	case 1:
		SFVec3f_Create(c, pNew, v1->x - v2->x, v1->y - v2->y, v1->z - v2->z);
		break;
	case 2:
		SFVec3f_Create(c, pNew, -v1->x , -v1->y , -v1->z );
		break;
	case 3:
		v = FLT2FIX(d);
		SFVec3f_Create(c, pNew, gf_mulfix(v1->x, v), gf_mulfix(v1->y, v), gf_mulfix(v1->z, v) );
		break;
	case 4:
		v = FLT2FIX(d);
		SFVec3f_Create(c, pNew, gf_divfix(v1->x, v), gf_divfix(v1->y, v), gf_divfix(v1->z, v));
		break;
	case 6:
		vec = *v1;
		gf_vec_norm(&vec);
		SFVec3f_Create(c, pNew, vec.x, vec.y, vec.z);
		break;
	case 8:
		vec = gf_vec_cross(*v1, *v2);
		SFVec3f_Create(c, pNew, vec.x, vec.y, vec.z);
		break;
	}
	return pNew;
}

static JSValue vec3f_add(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return vec3f_operand(c, obj, argc, argv, 0);
}
static JSValue vec3f_subtract(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return vec3f_operand(c, obj, argc, argv, 1);
}
static JSValue vec3f_negate(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return vec3f_operand(c, obj, argc, argv, 2);
}
static JSValue vec3f_multiply(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return vec3f_operand(c, obj, argc, argv, 3);
}
static JSValue vec3f_divide(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return vec3f_operand(c, obj, argc, argv, 4);
}
static JSValue vec3f_length(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return vec3f_operand(c, obj, argc, argv, 5);
}
static JSValue vec3f_normalize(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return vec3f_operand(c, obj, argc, argv, 6);
}
static JSValue vec3f_dot(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return vec3f_operand(c, obj, argc, argv, 7);
}
static JSValue vec3f_cross(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return vec3f_operand(c, obj, argc, argv, 8);
}


/*SFRotation class*/
static GFINLINE GF_JSField *SFRotation_Create(JSContext *c, JSValue obj, Fixed x, Fixed y, Fixed z, Fixed q)
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
	JS_SetOpaque(obj, field);
	return field;
}

static JSValue SFRotationConstructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	GF_JSField *f;
	SFVec3f v1, v2;
	Fixed l1, l2, dot;
	Double x = 0.0, y = 0.0, z = 0.0, a = 0.0;
	JSValue obj = JS_NewObjectClass(c, SFRotationClass.class_id);

	if (!argc) {
		SFRotation_Create(c, obj, FLT2FIX(x), FLT2FIX(y), FIX_ONE, FLT2FIX(a) );
		return obj;
	}
	if (JS_IsNumber(argv[0])) {
		if (argc > 0) JS_ToFloat64(c, &x, argv[0]);
		if (argc > 1) JS_ToFloat64(c, &y, argv[1]);
		if (argc > 2) JS_ToFloat64(c, &z, argv[2]);
		if (argc > 3) JS_ToFloat64(c, &a, argv[3]);
		SFRotation_Create(c, obj, FLT2FIX(x), FLT2FIX(y), FLT2FIX(z), FLT2FIX(a));
		return obj;
	}


	if (argc!=2) return GF_JS_EXCEPTION(c);
	if (!JS_IsObject(argv[0])) return GF_JS_EXCEPTION(c);
	f = JS_GetOpaque(argv[0], SFVec3fClass.class_id);
	if (!f) return GF_JS_EXCEPTION(c);

	v1 = * (SFVec3f *) (f)->field.far_ptr;
	if (JS_IsNumber(argv[1])) {
		JS_ToFloat64(c, &a, argv[1]);
		SFRotation_Create(c, obj, v1.x, v1.y, v1.z, FLT2FIX(a));
		return obj;
	}

	if (!JS_IsObject(argv[1])) return JS_FALSE;
	f = JS_GetOpaque(argv[1], SFVec3fClass.class_id);
	if (!f) return GF_JS_EXCEPTION(c);
	v2 = * (SFVec3f *) (f)->field.far_ptr;
	l1 = gf_vec_len(v1);
	l2 = gf_vec_len(v2);
	dot = gf_divfix(gf_vec_dot(v1, v2), gf_mulfix(l1, l2) );
	a = gf_atan2(gf_sqrt(FIX_ONE - gf_mulfix(dot, dot)), dot);
	SFRotation_Create(c, obj, gf_mulfix(v1.y, v2.z) - gf_mulfix(v2.y, v1.z),
	                  gf_mulfix(v1.z, v2.x) - gf_mulfix(v2.z, v1.x),
	                  gf_mulfix(v1.x, v2.y) - gf_mulfix(v2.x, v1.y),
	                  FLT2FIX(a));
	return obj;
}

static JSValue rot_getProperty(JSContext *c, JSValueConst obj, int magic)
{
	GF_JSField *val = (GF_JSField *) JS_GetOpaque(obj, SFRotationClass.class_id);
	if (!val) return GF_JS_EXCEPTION(c);
	switch (magic) {
	case 0:
		return JS_NewFloat64(c, FIX2FLT( ((SFRotation*)val->field.far_ptr)->x));
	case 1:
		return JS_NewFloat64(c, FIX2FLT( ((SFRotation*)val->field.far_ptr)->y));
	case 2:
		return JS_NewFloat64(c, FIX2FLT( ((SFRotation*)val->field.far_ptr)->z));
	case 3:
		return JS_NewFloat64(c, FIX2FLT( ((SFRotation*)val->field.far_ptr)->q));
	}
	return JS_UNDEFINED;
}

static JSValue rot_setProperty(JSContext *c, JSValueConst obj, JSValueConst value, int magic)
{
	Double d;
	Fixed v;
	Bool changed = 0;
	GF_JSField *ptr = (GF_JSField *) JS_GetOpaque(obj, SFRotationClass.class_id);
	if (!ptr) return GF_JS_EXCEPTION(c);

	if (JS_ToFloat64(c, &d, value)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[VRML JS] Value is not a number while assigning SFRotation\n"));
		return JS_FALSE;
	}

	switch (magic) {
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
		return JS_UNDEFINED;
	}
	if (changed) Script_FieldChanged(c, NULL, ptr, NULL);
	return JS_UNDEFINED;
}

static JSValue rot_getAxis(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	SFRotation r;
	JSValue pNew;
	GF_JSField *p = JS_GetOpaque(obj, SFRotationClass.class_id);
	if (!p) return GF_JS_EXCEPTION(c);
	r = * (SFRotation *) (p)->field.far_ptr;
	pNew = JS_NewObjectClass(c, SFVec3fClass.class_id);
	SFVec3f_Create(c, pNew, r.x, r.y, r.z);
	return pNew;
}
static JSValue rot_inverse(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	SFRotation r;
	JSValue pNew;
	GF_JSField *p = JS_GetOpaque(obj, SFRotationClass.class_id);
	if (!p) return GF_JS_EXCEPTION(c);
	r = * (SFRotation *) (p)->field.far_ptr;
	pNew = JS_NewObjectClass(c, SFRotationClass.class_id);
	SFRotation_Create(c, pNew, r.x, r.y, r.z, r.q-GF_PI);
	return pNew;
}

static JSValue rot_multiply(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	SFRotation r1, r2;
	SFVec4f q1, q2;
	JSValue pNew;

	if (argc<=0 || !JS_IsObject(argv[0]))
		return GF_JS_EXCEPTION(c);
	GF_JSField *p = JS_GetOpaque(obj, SFRotationClass.class_id);
	if (!p) return GF_JS_EXCEPTION(c);
	r1 = * (SFRotation *) (p)->field.far_ptr;
	p = JS_GetOpaque(argv[0], SFRotationClass.class_id);
	if (!p) return GF_JS_EXCEPTION(c);
	r2 = * (SFRotation *) (p)->field.far_ptr;

	q1 = gf_quat_from_rotation(r1);
	q2 = gf_quat_from_rotation(r2);
	q1 = gf_quat_multiply(&q1, &q2);
	r1 = gf_quat_to_rotation(&q1);

	pNew = JS_NewObjectClass(c, SFRotationClass.class_id);
	SFRotation_Create(c, pNew, r1.x, r1.y, r1.z, r1.q);
	return pNew;
}

static JSValue rot_multVec(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	SFVec3f v;
	SFRotation r;
	GF_Matrix mx;
	JSValue pNew;
	if (argc<=0 || !JS_IsObject(argv[0]))
		return GF_JS_EXCEPTION(c);

	GF_JSField *p = JS_GetOpaque(obj, SFRotationClass.class_id);
	if (!p) return GF_JS_EXCEPTION(c);
	r = * (SFRotation *) (p)->field.far_ptr;

	p = JS_GetOpaque(argv[0], SFVec3fClass.class_id);
	if (!p) return GF_JS_EXCEPTION(c);
	v = * (SFVec3f *) (p)->field.far_ptr;

	gf_mx_init(mx);
	gf_mx_add_rotation(&mx, r.q, r.x, r.y, r.z);
	gf_mx_apply_vec(&mx, &v);
	pNew = JS_NewObjectClass(c, SFVec3fClass.class_id);
	SFVec3f_Create(c, pNew, v.x, v.y, v.z);
	return pNew;
}
static JSValue rot_setAxis(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	SFVec3f v;
	SFRotation *r;
	GF_JSField *ptr;
	if (argc<=0 || !JS_IsObject(argv[0]))
		return GF_JS_EXCEPTION(c);
	ptr = JS_GetOpaque(obj, SFRotationClass.class_id);
	if (!ptr) return GF_JS_EXCEPTION(c);
	r = (SFRotation *) (ptr)->field.far_ptr;

	GF_JSField *p = JS_GetOpaque(argv[0], SFVec3fClass.class_id);
	if (!p) return GF_JS_EXCEPTION(c);
	v = * (SFVec3f *) (p)->field.far_ptr;

	r->x = v.x;
	r->y = v.y;
	r->z = v.z;
	Script_FieldChanged(c, NULL, ptr, NULL);
	return JS_TRUE;
}
static JSValue rot_slerp(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	SFRotation v1, v2, res;
	SFVec4f q1, q2;
	JSValue pNew;
	Double d;
	GF_JSField *p;

	if ((argc<2) || !JS_IsObject(argv[0]))
		return GF_JS_EXCEPTION(c);
	p = JS_GetOpaque(obj, SFRotationClass.class_id);
	if (!p) return GF_JS_EXCEPTION(c);
	v1 = * (SFRotation *) (p)->field.far_ptr;

	p = JS_GetOpaque(argv[0], SFRotationClass.class_id);
	if (!p) return GF_JS_EXCEPTION(c);
	v2 = * (SFRotation *) (p)->field.far_ptr;

	if (JS_ToFloat64(c, &d, argv[1])) return GF_JS_EXCEPTION(c);

	q1 = gf_quat_from_rotation(v1);
	q2 = gf_quat_from_rotation(v2);
	q1 = gf_quat_slerp(q1, q2, FLT2FIX( d));
	res = gf_quat_to_rotation(&q1);
	pNew = JS_NewObjectClass(c, SFRotationClass.class_id);
	SFRotation_Create(c, pNew, res.x, res.y, res.z, res.q);
	return pNew;
}

/* SFColor class functions */
static GFINLINE GF_JSField *SFColor_Create(JSContext *c, JSValue obj, Fixed r, Fixed g, Fixed b)
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
	JS_SetOpaque(obj, field);
	return field;
}
static JSValue SFColorConstructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	Double r = 0.0, g = 0.0, b = 0.0;
	JSValue obj = JS_NewObjectClass(c, SFColorClass.class_id);

	if (argc > 0) JS_ToFloat64(c, &r, argv[0]);
	if (argc > 1) JS_ToFloat64(c, &g, argv[1]);
	if (argc > 2) JS_ToFloat64(c, &b, argv[2]);
	SFColor_Create(c, obj, FLT2FIX( r), FLT2FIX( g), FLT2FIX( b));
	return obj;
}
static JSValue color_getProperty(JSContext *c, JSValueConst obj, int magic)
{
	GF_JSField *val = (GF_JSField *) JS_GetOpaque(obj, SFColorClass.class_id);
	if (!val) return GF_JS_EXCEPTION(c);
	switch (magic) {
	case 0:
		return JS_NewFloat64(c, FIX2FLT( ((SFColor*)val->field.far_ptr)->red));
	case 1:
		return JS_NewFloat64(c, FIX2FLT( ((SFColor*)val->field.far_ptr)->green));
		break;
	case 2:
		return JS_NewFloat64(c, FIX2FLT( ((SFColor*)val->field.far_ptr)->blue));
		break;
	default:
		return JS_UNDEFINED;
	}
	return JS_UNDEFINED;
}

static JSValue color_setProperty(JSContext *c, JSValueConst obj, JSValueConst value, int magic)
{
	Double d;
	Fixed v;
	Bool changed = 0;
	GF_JSField *ptr = (GF_JSField *) JS_GetOpaque(obj, SFColorClass.class_id);
	if (!ptr) return GF_JS_EXCEPTION(c);

	if (JS_ToFloat64(c, &d, value)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[VRML JS] Value is not a number while assigning SFColor\n"));
		return JS_FALSE;
	}

	switch (magic) {
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
		return JS_UNDEFINED;
	}
	if (changed) Script_FieldChanged(c, NULL, ptr, NULL);
	return JS_UNDEFINED;
}

static JSValue color_setHSV(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	SFColor *v1, hsv;
	Double h=0, s=0, v=0;
	GF_JSField *ptr = (GF_JSField *) JS_GetOpaque(obj, SFColorClass.class_id);
	if (!ptr) return GF_JS_EXCEPTION(c);
	if (argc != 3) return JS_FALSE;

	v1 = (ptr)->field.far_ptr;
	if (JS_ToFloat64(c, &h, argv[0])) return GF_JS_EXCEPTION(c);
	if (JS_ToFloat64(c, &s, argv[1])) return GF_JS_EXCEPTION(c);
	if (JS_ToFloat64(c, &v, argv[2])) return GF_JS_EXCEPTION(c);

	hsv.red = FLT2FIX( h);
	hsv.green = FLT2FIX( s);
	hsv.blue = FLT2FIX( v);
	SFColor_fromHSV(&hsv);
	gf_sg_vrml_field_copy(v1, &hsv, GF_SG_VRML_SFCOLOR);
	Script_FieldChanged(c, NULL, ptr, NULL);
	return JS_UNDEFINED;
}

static JSValue color_getHSV(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	SFColor *v1, hsv;
	JSValue arr;

	GF_JSField *ptr = (GF_JSField *) JS_GetOpaque(obj, SFColorClass.class_id);
	if (!ptr) return GF_JS_EXCEPTION(c);

	v1 = (ptr)->field.far_ptr;
	hsv = *v1;
	SFColor_toHSV(&hsv);
	arr = JS_NewArray(c);
	if (JS_IsException(arr)) return arr;

	JS_SetPropertyUint32(c, arr, 0, JS_NewFloat64(c, FIX2FLT(hsv.red)) );
	JS_SetPropertyUint32(c, arr, 1, JS_NewFloat64(c, FIX2FLT(hsv.green)) );
	JS_SetPropertyUint32(c, arr, 2, JS_NewFloat64(c, FIX2FLT(hsv.blue)) );
	return arr;
}

static JSValue genmf_Constructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv, JSClassID mf_class_id, JSClassID sf_class_id, u32 fieldType)
{
	u32 i;
	GF_JSField *ptr;
	JSValue obj = JS_NewObjectClass(c, mf_class_id);
	ptr = NewJSField(c);
	ptr->field.fieldType = fieldType;
	ptr->obj = obj;
	JS_SetOpaque(obj, ptr);

	if (!argc || (fieldType==GF_SG_VRML_MFNODE))  return obj;
	ptr->mfvals = gf_realloc(ptr->mfvals, sizeof(JSValue)*argc);
	ptr->mfvals_count = argc;
	for (i=0; i<(u32) argc; i++) {
		if (sf_class_id) {
			if (JS_IsObject(argv[i]) && JS_GetOpaque(argv[i], sf_class_id)) {
				ptr->mfvals[i] = JS_DupValue(c, argv[i]);
			} else {
				ptr->mfvals[i] = JS_NewObjectClass(c, sf_class_id);
			}
		} else {
			ptr->mfvals[i] = JS_DupValue(c, argv[i]);
		}
	}
	//don't dup value since this may go directly in script
	return obj;
}

static JSValue MFBoolConstructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	return genmf_Constructor(c, new_target, argc, argv, MFBoolClass.class_id, 0, GF_SG_VRML_MFBOOL);
}
static JSValue MFInt32Constructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	return genmf_Constructor(c, new_target, argc, argv, MFInt32Class.class_id, 0, GF_SG_VRML_MFINT32);
}
static JSValue MFFloatConstructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	return genmf_Constructor(c, new_target, argc, argv, MFFloatClass.class_id, 0, GF_SG_VRML_MFFLOAT);
}
static JSValue MFTimeConstructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	return genmf_Constructor(c, new_target, argc, argv, MFTimeClass.class_id, 0, GF_SG_VRML_MFTIME);
}
static JSValue MFStringConstructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	return genmf_Constructor(c, new_target, argc, argv, MFStringClass.class_id, 0, GF_SG_VRML_MFSTRING);
}
static JSValue MFURLConstructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	return genmf_Constructor(c, new_target, argc, argv, MFUrlClass.class_id, 0, GF_SG_VRML_MFURL);
}
static JSValue MFNodeConstructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	return genmf_Constructor(c, new_target, argc, argv, MFNodeClass.class_id, 0, GF_SG_VRML_MFNODE);
}

static void array_finalize_ex(JSRuntime *rt, JSValue obj, Bool is_js_call)
{
	u32 i;
	GF_JSField *ptr = JS_GetOpaque_Nocheck(obj);

	JS_ObjectDestroyed(rt, obj, ptr, 1);
	if (!ptr) return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[VRML JS] unregistering MFField %s\n", ptr->field.name));

	/*MF values*/
	if (ptr->mfvals) {
		for (i=0; i<ptr->mfvals_count; i++)
			JS_FreeValueRT(rt, ptr->mfvals[i] );
		gf_free(ptr->mfvals);
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

static void array_finalize(JSRuntime *rt, JSValue obj)
{
	array_finalize_ex(rt, obj, GF_TRUE);
}


static JSValue array_getElement(JSContext *c, JSValueConst obj, JSAtom atom, JSValueConst receiver)
{
	u32 idx;
	GF_JSField *ptr = JS_GetOpaque_Nocheck(obj);

	if (!JS_AtomIsArrayIndex(c, &idx, atom)) {
		JSValue ret = JS_UNDEFINED;
		const char *str = JS_AtomToCString(c, atom);
		if (!str) return ret;

		if (!strcmp(str, "length")) {
			GF_JSField *f_ptr = JS_GetOpaque_Nocheck(obj);
			if (!f_ptr) {
				ret = GF_JS_EXCEPTION(c);
			} else if (f_ptr->field.fieldType==GF_SG_VRML_MFNODE) {
				ret = JS_NewInt32(c, gf_node_list_get_count(*(GF_ChildNodeItem **)f_ptr->field.far_ptr) );
			} else {
				ret = JS_NewInt32(c, f_ptr->mfvals_count);
			}
		} else if (!strcmp(str, "toString")) {
			ret = JS_NewCFunction(c, field_toString, "toString", 0);
		}
		JS_FreeCString(c, str);
		return ret;
	}

	if (ptr->field.fieldType==GF_SG_VRML_MFNODE) {
		GF_Node *node = gf_node_list_get_child(*(GF_ChildNodeItem **)ptr->field.far_ptr, idx);
		if (!node) return JS_NULL;
		return JS_DupValue(c, node_get_binding(JS_GetScriptStack(c), node));
	} else {
		JSValue val;
		if (idx>=ptr->mfvals_count) return JS_NULL;
		val = ptr->mfvals[idx];
//		GF_JSField *sf = JS_GetOpaque_Nocheck(val);
		return JS_DupValue(c, val);
	}
	return JS_NULL;
}


static int array_setLength(JSContext *c, GF_JSField *ptr, JSValueConst value)
{
	u32 len, i, sftype, old_len;
	GF_JSClass *the_sf_class;

	if (!JS_IsInteger(value) || !ptr) return -1;
	len=-1;
	JS_ToInt32(c, &len, value);
	if ((s32)len<0) return -1;

	if (!len) {
		if (ptr->field.fieldType==GF_SG_VRML_MFNODE) {
			gf_node_unregister_children(ptr->owner, *(GF_ChildNodeItem**)ptr->field.far_ptr);
			*(GF_ChildNodeItem**)ptr->field.far_ptr = NULL;
		} else {
			gf_sg_vrml_mf_reset(ptr->field.far_ptr, ptr->field.fieldType);
		}
		while (ptr->mfvals_count) {
			ptr->mfvals_count--;
			JS_FreeValue(c, ptr->mfvals[ptr->mfvals_count]);
		}
		gf_free(ptr->mfvals);
		ptr->mfvals = NULL;
		return 1;
	}

	old_len = ptr->mfvals_count;
	ptr->mfvals_count = len;
	if (len == old_len) return 1;

	the_sf_class = NULL;
	switch (ptr->field.fieldType) {
	case GF_SG_VRML_MFVEC2F:
		the_sf_class = &SFVec2fClass;
		break;
	case GF_SG_VRML_MFVEC3F:
		the_sf_class = &SFVec3fClass;
		break;
	case GF_SG_VRML_MFCOLOR:
		the_sf_class = &SFColorClass;
		break;
	case GF_SG_VRML_MFROTATION:
		the_sf_class = &SFRotationClass;
		break;
	case GF_SG_VRML_MFNODE:
	{
		u32 nb_nodes = gf_node_list_get_count(*(GF_ChildNodeItem**)ptr->field.far_ptr);
		while (len < nb_nodes) {
			GF_Node *n = gf_node_list_del_child_idx((GF_ChildNodeItem**)ptr->field.far_ptr, nb_nodes - 1);
			if (n) gf_node_unregister(n, ptr->owner);
			nb_nodes--;
		}
		if (len>nb_nodes) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[VRML] MFARRAY EXPANSION NOT SUPPORTED!!!\n"));
		}
	}
		return 1;
	}

	ptr->mfvals = gf_realloc(ptr->mfvals, sizeof(JSValue)*ptr->mfvals_count);
	sftype = gf_sg_vrml_get_sf_type(ptr->field.fieldType);
	for (i=old_len; i<len; i++) {
		JSValue a_val;
		if (the_sf_class) {
			GF_JSField *slot;
			a_val = JS_NewObjectClass(c, the_sf_class->class_id);
			slot = SFVec2f_Create(c, a_val, 0, 0);
			if (slot) {
				slot->owner = ptr->owner;
			}
		} else {
			switch (sftype) {
			case GF_SG_VRML_SFBOOL:
				a_val = JS_FALSE;
				break;
			case GF_SG_VRML_SFFLOAT:
			case GF_SG_VRML_SFTIME:
				a_val = JS_NewFloat64(c, 0);
				break;
			case GF_SG_VRML_SFSTRING:
			case GF_SG_VRML_SFURL:
				a_val = JS_NewString(c, "");
				break;
			case GF_SG_VRML_SFINT32:
			default:
				a_val = JS_NewInt32(c, 0);
				break;
			}
		}
		ptr->mfvals[i] = a_val;
		gf_sg_vrml_mf_append(ptr->field.far_ptr, ptr->field.fieldType, NULL);
	}
	return 1;
}

//this could be overloaded for each MF type...
static int array_setElement(JSContext *c, JSValueConst obj, JSAtom atom, JSValueConst value, JSValueConst receiver, int flags)
{
	u32 ind;
	u32 len;
	Double d;
	s32 ival;
	GF_JSField *from;
	GF_JSClass *the_sf_class = NULL;
	char *str_val;
	void *sf_slot;
	Bool is_append = 0;
	GF_JSField *ptr = (GF_JSField *) JS_GetOpaque_Nocheck(obj);
	if (!ptr) return -1;

	if (!JS_AtomIsArrayIndex(c, &ind, atom)) {
		int ret = 0;
		const char *str = JS_AtomToCString(c, atom);
		if (str && !strcmp(str, "length")) {
			ret = array_setLength(c, ptr, value);
		}
		JS_FreeCString(c, str);
		return ret;
	}
	if (ptr->field.fieldType!=GF_SG_VRML_MFNODE) {
		len = ptr->mfvals_count;
	} else {
		len = gf_node_list_get_count(*(GF_ChildNodeItem **)ptr->field.far_ptr);
	}
	if (gf_sg_vrml_is_sf_field(ptr->field.fieldType))
		return -1;

	switch (ptr->field.fieldType) {
	case GF_SG_VRML_MFVEC2F:
		the_sf_class = &SFVec2fClass;
		break;
	case GF_SG_VRML_MFVEC3F:
		the_sf_class = &SFVec3fClass;
		break;
	case GF_SG_VRML_MFCOLOR:
		the_sf_class = &SFColorClass;
		break;
	case GF_SG_VRML_MFROTATION:
		the_sf_class = &SFRotationClass;
		break;
	}
	/*dynamic expend*/
	if (ind>=len) {
		is_append = 1;
	}
	if (is_append && (ptr->field.fieldType!=GF_SG_VRML_MFNODE)) {

		ptr->mfvals = gf_realloc(ptr->mfvals, sizeof(JSValue)*(ind+1));
		ptr->mfvals_count = ind+1;

		while (len<ind) {
			JSValue a_val;
			switch (ptr->field.fieldType) {
			case GF_SG_VRML_MFBOOL:
				a_val = JS_NewBool(c, 0);
				break;
			case GF_SG_VRML_MFINT32:
				a_val = JS_NewInt32(c, 0);
				break;
			case GF_SG_VRML_MFFLOAT:
			case GF_SG_VRML_MFTIME:
				a_val = JS_NewFloat64(c, 0);
				break;
			case GF_SG_VRML_MFSTRING:
			case GF_SG_VRML_MFURL:
				a_val = JS_NewString(c, "");
				break;
			case GF_SG_VRML_MFVEC2F:
			case GF_SG_VRML_MFVEC3F:
			case GF_SG_VRML_MFCOLOR:
			case GF_SG_VRML_MFROTATION:
				a_val = JS_NewObjectClass(c, the_sf_class->class_id);
				break;
			default:
				a_val = JS_NULL;
				break;
			}

			gf_sg_vrml_mf_insert(ptr->field.far_ptr, ptr->field.fieldType, &sf_slot, len);
			ptr->mfvals[len] = a_val;

			len++;
		}
		if (ptr->field.far_ptr)
			gf_sg_vrml_mf_insert(ptr->field.far_ptr, ptr->field.fieldType, &sf_slot, ind);

		ptr->mfvals[ind] = JS_NULL;
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
		if (JS_IsUndefined(value)) return -1;
		if (JS_IsNull(value) ) return -1;
		if (!JS_GetOpaque(value, SFNodeClass.class_id) ) return -1;
	} else if (the_sf_class) {
		if (JS_IsUndefined(value)) return -1;
		if (!JS_GetOpaque(value, the_sf_class->class_id) ) return -1;
	} else if (ptr->field.fieldType==GF_SG_VRML_MFBOOL) {
		if (!JS_IsBool(value)) return -1;
	} else if (ptr->field.fieldType==GF_SG_VRML_MFINT32) {
		if (!JS_IsInteger(value)) return -1;
	} else if (ptr->field.fieldType==GF_SG_VRML_MFFLOAT) {
		if (!JS_IsNumber(value)) return -1;
	} else if (ptr->field.fieldType==GF_SG_VRML_MFTIME) {
		if (!JS_IsNumber(value)) return -1;
	} else if (ptr->field.fieldType==GF_SG_VRML_MFSTRING) {
		if (!JS_IsString(value)) return -1;
	} else if (ptr->field.fieldType==GF_SG_VRML_MFURL) {
		if (!JS_IsString(value)) return -1;
	}


	/*rewrite MFNode entry*/
	if (ptr->field.fieldType==GF_SG_VRML_MFNODE) {
		GF_Node *prev_n, *new_n;

		if (!ptr->owner) return 1;

		/*get new node*/
		from = (GF_JSField *) JS_GetOpaque(value, SFNodeClass.class_id);
		new_n = *(GF_Node**)from->field.far_ptr;
		prev_n = NULL;

		if (!is_append) {
			/*get and delete previous node if any, but unregister later*/
			prev_n = gf_node_list_del_child_idx( (GF_ChildNodeItem **)ptr->field.far_ptr, ind);
		}

		if (new_n) {
			gf_node_list_insert_child( (GF_ChildNodeItem **)ptr->field.far_ptr , new_n, ind);
			gf_node_register(new_n, ptr->owner);

			/*node created from script and inserted in the tree, create binding*/
			node_get_binding(JS_GetScriptStack(c), new_n);
		}
		/*unregister previous node*/
		if (prev_n) gf_node_unregister(prev_n, ptr->owner);

		Script_FieldChanged(c, NULL, ptr, NULL);
		return 1;
	}

	//since this value comes from the script, ref count it
	JS_FreeValue(c, ptr->mfvals[ind]);
	ptr->mfvals[ind] = JS_DupValue(c, value);

	if (!ptr->owner) return 1;
	if (!ptr->field.far_ptr) return -1;

	/*rewrite MF slot*/
	switch (ptr->field.fieldType) {
	case GF_SG_VRML_MFBOOL:
		((MFBool *)ptr->field.far_ptr)->vals[ind] = (Bool) JS_ToBool(c, value);
		break;
	case GF_SG_VRML_MFINT32:
		JS_ToInt32(c, &ival, value);
		((MFInt32 *)ptr->field.far_ptr)->vals[ind] = ival;
		break;
	case GF_SG_VRML_MFFLOAT:
		JS_ToFloat64(c, &d, value);
		((MFFloat *)ptr->field.far_ptr)->vals[ind] = FLT2FIX(d);
		break;
	case GF_SG_VRML_MFTIME:
		JS_ToFloat64(c, &d, value);
		((MFTime *)ptr->field.far_ptr)->vals[ind] = d;
		break;
	case GF_SG_VRML_MFSTRING:
		if (((MFString *)ptr->field.far_ptr)->vals[ind]) {
			gf_free(((MFString *)ptr->field.far_ptr)->vals[ind]);
			((MFString *)ptr->field.far_ptr)->vals[ind] = NULL;
		}
		str_val = (char *) JS_ToCString(c, value);
		((MFString *)ptr->field.far_ptr)->vals[ind] = gf_strdup(str_val);
		JS_FreeCString(c, str_val);
		break;

	case GF_SG_VRML_MFURL:
		if (((MFURL *)ptr->field.far_ptr)->vals[ind].url) {
			gf_free(((MFURL *)ptr->field.far_ptr)->vals[ind].url);
			((MFURL *)ptr->field.far_ptr)->vals[ind].url = NULL;
		}
		str_val =  (char *) JS_ToCString(c, value);
		((MFURL *)ptr->field.far_ptr)->vals[ind].url = gf_strdup(str_val);
		((MFURL *)ptr->field.far_ptr)->vals[ind].OD_ID = 0;
		JS_FreeCString(c, str_val);
		break;

	case GF_SG_VRML_MFVEC2F:
		from = (GF_JSField *) JS_GetOpaque(value, the_sf_class->class_id);
		gf_sg_vrml_field_copy(& ((MFVec2f *)ptr->field.far_ptr)->vals[ind], from->field.far_ptr, from->field.fieldType);
		break;
	case GF_SG_VRML_MFVEC3F:
		from = (GF_JSField *) JS_GetOpaque(value, the_sf_class->class_id);
		gf_sg_vrml_field_copy(& ((MFVec3f *)ptr->field.far_ptr)->vals[ind], from->field.far_ptr, from->field.fieldType);
		break;
	case GF_SG_VRML_MFROTATION:
		from = (GF_JSField *) JS_GetOpaque(value, the_sf_class->class_id);
		gf_sg_vrml_field_copy(& ((MFRotation *)ptr->field.far_ptr)->vals[ind], from->field.far_ptr, from->field.fieldType);
		break;
	case GF_SG_VRML_MFCOLOR:
		from = (GF_JSField *) JS_GetOpaque(value, the_sf_class->class_id);
		gf_sg_vrml_field_copy(& ((MFColor *)ptr->field.far_ptr)->vals[ind], from->field.far_ptr, from->field.fieldType);
		break;
	}

	Script_FieldChanged(c, NULL, ptr, NULL);
	return 1;
}


/* MFVec2f class constructor */
static JSValue MFVec2fConstructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	return genmf_Constructor(c, new_target, argc, argv, MFVec2fClass.class_id, SFVec2fClass.class_id, GF_SG_VRML_MFVEC2F);
}

/* MFVec3f class constructor */
static JSValue MFVec3fConstructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	return genmf_Constructor(c, new_target, argc, argv, MFVec3fClass.class_id, SFVec3fClass.class_id, GF_SG_VRML_MFVEC3F);
}

/* MFRotation class constructor */
static JSValue MFRotationConstructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	return genmf_Constructor(c, new_target, argc, argv, MFRotationClass.class_id, SFRotationClass.class_id, GF_SG_VRML_MFROTATION);
}

/*MFColor class constructor */
static JSValue MFColorConstructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	return genmf_Constructor(c, new_target, argc, argv, MFColorClass.class_id, SFColorClass.class_id, GF_SG_VRML_MFCOLOR);
}

#ifndef GPAC_DISABLE_SVG

JSValue gf_sg_js_event_add_listener(JSContext *c, JSValueConst this_val, int argc, JSValueConst *argv, GF_Node *node);
JSValue gf_sg_js_event_remove_listener(JSContext *c, JSValueConst this_val, int argc, JSValueConst *argv, GF_Node *node);

static JSValue vrml_event_add_listener(JSContext *c, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Node *node;
	GF_JSField *ptr = (GF_JSField *) JS_GetOpaque(this_val, SFNodeClass.class_id);
	if (!ptr)
		return GF_JS_EXCEPTION(c);
	assert(ptr->field.fieldType==GF_SG_VRML_SFNODE);
	node = * ((GF_Node **)ptr->field.far_ptr);
	return gf_sg_js_event_add_listener(c, this_val, argc, argv, node);
}

static JSValue vrml_event_remove_listener(JSContext *c, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Node *node;
	GF_JSField *ptr = (GF_JSField *) JS_GetOpaque(this_val, SFNodeClass.class_id);
	if (!ptr) return GF_JS_EXCEPTION(c);
	assert(ptr->field.fieldType==GF_SG_VRML_SFNODE);
	node = * ((GF_Node **)ptr->field.far_ptr);
	return gf_sg_js_event_remove_listener(c, this_val, argc, argv, node);
}
#endif


static const JSCFunctionListEntry Browser_funcs[] =
{
	JS_CFUNC_DEF("getName", 0, getName),
	JS_CFUNC_DEF("getVersion", 0, getVersion),
	JS_CFUNC_DEF("getCurrentSpeed", 0, getCurrentSpeed),
	JS_CFUNC_DEF("getCurrentFrameRate", 0, getCurrentFrameRate),
	JS_CFUNC_DEF("getWorldURL", 0, getWorldURL),
	JS_CFUNC_DEF("replaceWorld", 1, replaceWorld),
	JS_CFUNC_DEF("addRoute", 4, addRoute),
	JS_CFUNC_DEF("deleteRoute", 4, deleteRoute),
	JS_CFUNC_DEF("loadURL", 1, loadURL),
	JS_CFUNC_DEF("createVrmlFromString", 1, createVrmlFromString),
	JS_CFUNC_DEF("setDescription", 1, setDescription),
	JS_CFUNC_DEF("print", 1, js_print),
	JS_CFUNC_DEF("alert", 1, js_print),
	JS_CFUNC_DEF("getScript", 0, getScript),
	JS_CFUNC_DEF("getProto", 0, getProto),
	JS_CFUNC_DEF("loadScript", 1, loadScript),
	JS_CFUNC_DEF("getElementById", 1, getElementById),
};

static const JSCFunctionListEntry SFNode_funcs[] =
{
	JS_CFUNC_DEF("toString", 0, node_toString),
};

static JSClassExoticMethods SFNode_exotic =
{
	.get_property = node_getProperty,
	.set_property = node_setProperty,
};

static const JSCFunctionListEntry SFVec2f_funcs[] =
{
	JS_CGETSET_MAGIC_DEF("x", vec2f_getProperty, vec2f_setProperty, 0),
	JS_CGETSET_MAGIC_DEF("y", vec2f_getProperty, vec2f_setProperty, 1),
	JS_CFUNC_DEF("add", 1, vec2f_add),
	JS_CFUNC_DEF("divide", 1, vec2f_divide),
	JS_CFUNC_DEF("dot", 1, vec2f_dot),
	JS_CFUNC_DEF("length", 0, vec2f_length),
	JS_CFUNC_DEF("multiply", 1, vec2f_multiply),
	JS_CFUNC_DEF("normalize", 0, vec2f_normalize),
	JS_CFUNC_DEF("subtract", 1, vec2f_subtract),
	JS_CFUNC_DEF("negate", 0, vec2f_negate),
	JS_CFUNC_DEF("toString", 0, field_toString),
};


static const JSCFunctionListEntry SFVec3f_funcs[] =
{
	JS_CGETSET_MAGIC_DEF("x", vec3f_getProperty, vec3f_setProperty, 0),
	JS_CGETSET_MAGIC_DEF("y", vec3f_getProperty, vec3f_setProperty, 1),
	JS_CGETSET_MAGIC_DEF("z", vec3f_getProperty, vec3f_setProperty, 2),

	JS_CFUNC_DEF("add", 1, vec3f_add),
	JS_CFUNC_DEF("divide", 1, vec3f_divide),
	JS_CFUNC_DEF("dot", 1, vec3f_dot),
	JS_CFUNC_DEF("length", 0, vec3f_length),
	JS_CFUNC_DEF("multiply", 1, vec3f_multiply),
	JS_CFUNC_DEF("normalize", 0, vec3f_normalize),
	JS_CFUNC_DEF("subtract", 1, vec3f_subtract),
	JS_CFUNC_DEF("cross", 1, vec3f_cross),
	JS_CFUNC_DEF("negate", 0, vec3f_negate),
	JS_CFUNC_DEF("toString", 0, field_toString),
};

static const JSCFunctionListEntry SFRotation_funcs[] =
{
	JS_CGETSET_MAGIC_DEF("xAxis", rot_getProperty, rot_setProperty, 0),
	JS_CGETSET_MAGIC_DEF("yAxis", rot_getProperty, rot_setProperty, 1),
	JS_CGETSET_MAGIC_DEF("zAxis", rot_getProperty, rot_setProperty, 2),
	JS_CGETSET_MAGIC_DEF("angle", rot_getProperty, rot_setProperty, 3),
	JS_CFUNC_DEF("getAxis", 1, rot_getAxis),
	JS_CFUNC_DEF("inverse", 1, rot_inverse),
	JS_CFUNC_DEF("multiply", 1, rot_multiply),
	JS_CFUNC_DEF("multVec", 0, rot_multVec),
	JS_CFUNC_DEF("setAxis", 1, rot_setAxis),
	JS_CFUNC_DEF("slerp", 0, rot_slerp),
	JS_CFUNC_DEF("toString", 0, field_toString),
};

static const JSCFunctionListEntry SFColor_funcs[] =
{
	JS_CGETSET_MAGIC_DEF("r", color_getProperty, color_setProperty, 0),
	JS_CGETSET_MAGIC_DEF("g", color_getProperty, color_setProperty, 1),
	JS_CGETSET_MAGIC_DEF("b", color_getProperty, color_setProperty, 2),
	JS_CFUNC_DEF("setHSV", 3, color_setHSV),
	JS_CFUNC_DEF("getHSV", 0, color_getHSV),
	JS_CFUNC_DEF("toString", 0, field_toString),
};


static const JSCFunctionListEntry SFImage_funcs[] =
{
	JS_CGETSET_MAGIC_DEF("x", image_getProperty, image_setProperty, 0),
	JS_CGETSET_MAGIC_DEF("y", image_getProperty, image_setProperty, 1),
	JS_CGETSET_MAGIC_DEF("comp", image_getProperty, image_setProperty, 2),
	JS_CGETSET_MAGIC_DEF("array", image_getProperty, image_setProperty, 3),
	JS_CFUNC_DEF("toString", 0, field_toString),
};

static const JSCFunctionListEntry MFArray_funcs[] =
{
	//ignored, kept for MSVC
	JS_CGETSET_MAGIC_DEF("length", NULL, NULL, 0),
};
static JSClassExoticMethods MFArray_exotic =
{
	.get_property = array_getElement,
	.set_property = array_setElement,
};

static void field_gc_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func)
{
    GF_JSField *jsf = JS_GetOpaque_Nocheck(val);
	if (!jsf) return;
	if (!JS_IsUndefined(jsf->obj) && jsf->owner) {
		JS_MarkValue(rt, jsf->obj, mark_func);
	}
	if (jsf->node && jsf->node->sgprivate->interact->routes) {
		u32 i=0;
		GF_RouteToScript *r;
		while ( (r = gf_list_enum(jsf->node->sgprivate->interact->routes, &i))) {
			if (r->script_route) {
				JS_MarkValue(rt, r->fun, mark_func);
				JS_MarkValue(rt, r->obj, mark_func);
			}
		}
	}
	if (jsf->mfvals) {
		u32 i;
		assert(jsf->mfvals_count);
		for (i=0; i<jsf->mfvals_count; i++)
    		JS_MarkValue(rt, jsf->mfvals[i], mark_func);
	}
}

#define SETUP_JSCLASS_BASIC(_class, _name) \
	/*classes are global to runtime*/\
	if (!_class.class_id) {\
		JS_NewClassID(&(_class.class_id)); \
		_class.class.class_name = _name; \
		JS_NewClass(js_runtime, _class.class_id, &(_class.class));\
	}\

#define SETUP_JSCLASS(_class, _name, _proto_funcs, _construct, _finalize, _exotic) \
	/*classes are global to runtime*/\
	if (!_class.class_id) {\
		JS_NewClassID(&(_class.class_id)); \
		_class.class.class_name = _name; \
		_class.class.finalizer = _finalize;\
		_class.class.exotic = _exotic;\
		_class.class.gc_mark = field_gc_mark;\
		JS_NewClass(js_runtime, _class.class_id, &(_class.class));\
	}\
	{ \
	JSValue _proto_obj = JS_NewObjectClass(sc->js_ctx, _class.class_id);\
    	JS_SetPropertyFunctionList(sc->js_ctx, _proto_obj, _proto_funcs, countof(_proto_funcs));\
    	JS_SetClassProto(sc->js_ctx, _class.class_id, _proto_obj);\
	sc->_class = JS_NewCFunction2(sc->js_ctx, _construct, _name, 1, JS_CFUNC_constructor, 0);\
	JS_SetPropertyStr(sc->js_ctx, sc->js_obj, _name, sc->_class);\
	}\

static void vrml_js_init_api(GF_ScriptPriv *sc, GF_Node *script)
{
	sc->js_obj = JS_GetGlobalObject(sc->js_ctx);
	JSRuntime *js_runtime = JS_GetRuntime(sc->js_ctx);
	//init all our classes
	SETUP_JSCLASS_BASIC(globalClass, "global");
	SETUP_JSCLASS_BASIC(AnyClass, "AnyClass");
	SETUP_JSCLASS_BASIC(browserClass, "Browser");

	SETUP_JSCLASS(SFNodeClass, "SFNode", SFNode_funcs, SFNodeConstructor, node_finalize, &SFNode_exotic);
	SETUP_JSCLASS(SFVec2fClass, "SFVec2f", SFVec2f_funcs, SFVec2fConstructor, field_finalize, NULL);
	SETUP_JSCLASS(SFVec3fClass, "SFVec3f", SFVec3f_funcs, SFVec3fConstructor, field_finalize, NULL);
	SETUP_JSCLASS(SFRotationClass, "SFRotation", SFRotation_funcs, SFRotationConstructor, field_finalize, NULL);
	SETUP_JSCLASS(SFColorClass, "SFColor", SFColor_funcs, SFColorConstructor, field_finalize, NULL);
	SETUP_JSCLASS(SFImageClass, "SFImage", SFImage_funcs, SFImageConstructor, field_finalize, NULL);

	SETUP_JSCLASS(MFInt32Class, "MFInt32", MFArray_funcs, MFInt32Constructor, array_finalize, &MFArray_exotic);
	SETUP_JSCLASS(MFBoolClass, "MFBool", MFArray_funcs, MFBoolConstructor, array_finalize, &MFArray_exotic);
	SETUP_JSCLASS(MFTimeClass, "MFTime", MFArray_funcs, MFTimeConstructor, array_finalize, &MFArray_exotic);
	SETUP_JSCLASS(MFFloatClass, "MFFloat", MFArray_funcs, MFFloatConstructor, array_finalize, &MFArray_exotic);
	SETUP_JSCLASS(MFUrlClass, "MFUrl", MFArray_funcs, MFURLConstructor, array_finalize, &MFArray_exotic);
	SETUP_JSCLASS(MFStringClass, "MFString", MFArray_funcs, MFStringConstructor, array_finalize, &MFArray_exotic);
	SETUP_JSCLASS(MFNodeClass, "MFNode", MFArray_funcs, MFNodeConstructor, array_finalize, &MFArray_exotic);
	SETUP_JSCLASS(MFVec2fClass, "MFVec2f", MFArray_funcs, MFVec2fConstructor, array_finalize, &MFArray_exotic);
	SETUP_JSCLASS(MFVec3fClass , "MFVec3f", MFArray_funcs, MFVec3fConstructor, array_finalize, &MFArray_exotic);
	SETUP_JSCLASS(MFRotationClass, "MFRotation", MFArray_funcs, MFRotationConstructor, array_finalize, &MFArray_exotic);
	SETUP_JSCLASS(MFColorClass, "MFColor", MFArray_funcs, MFColorConstructor, array_finalize, &MFArray_exotic);


    JS_SetPropertyStr(sc->js_ctx, sc->js_obj, "print", JS_NewCFunction(sc->js_ctx, js_print, "print", 1));
    JS_SetPropertyStr(sc->js_ctx, sc->js_obj, "alert", JS_NewCFunction(sc->js_ctx, js_print, "print", 1));
    JS_SetPropertyStr(sc->js_ctx, sc->js_obj, "parseXML", JS_NewCFunction(sc->js_ctx, vrml_parse_xml, "parseXML", 1));

	/*remember pointer to scene graph!!*/
	JS_SetOpaque(sc->js_obj, script->sgprivate->scenegraph);

	JS_SetPropertyStr(sc->js_ctx, sc->js_obj, "FALSE", JS_FALSE);
	JS_SetPropertyStr(sc->js_ctx, sc->js_obj, "TRUE", JS_TRUE);
	JS_SetPropertyStr(sc->js_ctx, sc->js_obj, "NULL", JS_NULL);
	JS_SetPropertyStr(sc->js_ctx, sc->js_obj, "_this", JS_NULL);
//	JS_DefineProperty(sc->js_ctx, sc->js_obj, "_this", PRIVATE_TO_JSVAL(script), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT );

	JSValue browser = JS_NewObjectClass(sc->js_ctx, browserClass.class_id);
    JS_SetPropertyFunctionList(sc->js_ctx, browser, Browser_funcs, countof(Browser_funcs));
	JS_SetPropertyStr(sc->js_ctx, sc->js_obj, "Browser", browser);

	/*TODO ? SFVec3f / MFVec4f support */

	sc->node_toString_fun = JS_NewCFunction(sc->js_ctx, node_toString, "toString", 0);
	sc->node_getTime_fun = JS_NewCFunction(sc->js_ctx, node_getTime, "getTime", 0);
#ifndef GPAC_DISABLE_SVG
	sc->node_addEventListener_fun = JS_NewCFunction(sc->js_ctx, vrml_event_add_listener, "addEventListener", 0);
	sc->node_removeEventListener_fun = JS_NewCFunction(sc->js_ctx, vrml_event_remove_listener, "removeEventListener", 0);
#endif
}



void gf_sg_script_to_node_field(JSContext *c, JSValue val, GF_FieldInfo *field, GF_Node *owner, GF_JSField *parent)
{
	Double d;
	Bool changed;
	const char *str_val;
	GF_JSField *p, *from;
	u32 len;
	s32 ival;
	JSValue item;
	u32 i;

	if (JS_IsUndefined(val)) return;
	if ((field->fieldType != GF_SG_VRML_SFNODE) && JS_IsNull(val)) return;


	switch (field->fieldType) {
	case GF_SG_VRML_SFBOOL:
		*((SFBool *) field->far_ptr) = JS_ToBool(c,val);
		Script_FieldChanged(c, owner, parent, field);
		return;

	case GF_SG_VRML_SFINT32:
		if (!JS_ToInt32(c, ((SFInt32 *) field->far_ptr), val)) {
			Script_FieldChanged(c, owner, parent, field);
		return;

	case GF_SG_VRML_SFFLOAT:
		if (!JS_ToFloat64(c, &d, val)) {
			 *((SFFloat *) field->far_ptr) = FLT2FIX( d);
			Script_FieldChanged(c, owner, parent, field);
		}
		return;

	case GF_SG_VRML_SFTIME:
		if (!JS_ToFloat64(c, &d, val)) {
			*((SFTime *) field->far_ptr) = (Double) d;
			Script_FieldChanged(c, owner, parent, field);
		}
		return;
	}
	case GF_SG_VRML_SFSTRING:
	{
		SFString *s = (SFString*)field->far_ptr;
		const char *sval = JS_ToCString(c, val);

		/*we do filter strings since rebuilding a text is quite slow, so let's avoid killing the compositors*/
		if (!s->buffer || strcmp(sval, s->buffer)) {
			if ( s->buffer) gf_free(s->buffer);
			s->buffer = gf_strdup(sval);
			Script_FieldChanged(c, owner, parent, field);
		}
		JS_FreeCString(c, sval);
		return;
	}
	case GF_SG_VRML_SFURL:
		str_val = JS_ToCString(c, val);
		if (((SFURL*)field->far_ptr)->url) gf_free(((SFURL*)field->far_ptr)->url);
		((SFURL*)field->far_ptr)->url = gf_strdup(str_val);
		((SFURL*)field->far_ptr)->OD_ID = 0;
		Script_FieldChanged(c, owner, parent, field);
		JS_FreeCString(c, str_val);
		return;

	case GF_SG_VRML_MFSTRING:
	{
		GF_JSField *src = JS_GetOpaque(val, MFStringClass.class_id);
		gf_sg_vrml_mf_reset(field->far_ptr, field->fieldType);
		if (src && src->mfvals_count) {
			gf_sg_vrml_mf_alloc(field->far_ptr, field->fieldType, src->mfvals_count);
			for (i=0; i<src->mfvals_count; i++) {
				str_val = JS_ToCString(c, src->mfvals[i]);
				((MFString*)field->far_ptr)->vals[i] = gf_strdup(str_val);
				JS_FreeCString(c, str_val);
			}
		} else {
			gf_sg_vrml_mf_alloc(field->far_ptr, field->fieldType, 1);
			str_val = JS_ToCString(c, val);
			((MFString*)field->far_ptr)->vals[0] = gf_strdup(str_val);
			JS_FreeCString(c, str_val);
		}
		Script_FieldChanged(c, owner, parent, field);
	}
		return;

	case GF_SG_VRML_MFURL:
		gf_sg_vrml_mf_reset(field->far_ptr, field->fieldType);
		gf_sg_vrml_mf_alloc(field->far_ptr, field->fieldType, 1);
		str_val = JS_ToCString(c, val);
		((MFURL*)field->far_ptr)->vals[0].url = gf_strdup(str_val);
		((MFURL*)field->far_ptr)->vals[0].OD_ID = 0;
		Script_FieldChanged(c, owner, parent, field);
		JS_FreeCString(c, str_val);
		return;

	default:
		break;
	}

	//from here we must have an object
	if (! JS_IsObject(val)) return;

	switch (field->fieldType) {
	case GF_SG_VRML_SFVEC2F:
		p = (GF_JSField *) JS_GetOpaque(val, SFVec2fClass.class_id);
		if (p) {
			gf_sg_vrml_field_copy(field->far_ptr, p->field.far_ptr, GF_SG_VRML_SFVEC2F);
			Script_FieldChanged(c, owner, parent, field);
		}
		return;

	case GF_SG_VRML_SFVEC3F:
		p = (GF_JSField *) JS_GetOpaque(val, SFVec3fClass.class_id);
		if (p) {
			gf_sg_vrml_field_copy(field->far_ptr, p->field.far_ptr, GF_SG_VRML_SFVEC3F);
			Script_FieldChanged(c, owner, parent, field);
		}
		return;

	case GF_SG_VRML_SFROTATION:
		p = (GF_JSField *) JS_GetOpaque(val, SFRotationClass.class_id);
		if (p) {
			gf_sg_vrml_field_copy(field->far_ptr, p->field.far_ptr, GF_SG_VRML_SFROTATION);
			Script_FieldChanged(c, owner, parent, field);
		}
		return;

	case GF_SG_VRML_SFCOLOR:
		p = (GF_JSField *) JS_GetOpaque(val, SFColorClass.class_id);
		if (p) {
			gf_sg_vrml_field_copy(field->far_ptr, p->field.far_ptr, GF_SG_VRML_SFCOLOR);
			Script_FieldChanged(c, owner, parent, field);
		}
		return;
	case GF_SG_VRML_SFIMAGE:
		p = (GF_JSField *) JS_GetOpaque(val, SFImageClass.class_id);
		if (p) {
			gf_sg_vrml_field_copy(field->far_ptr, p->field.far_ptr, GF_SG_VRML_SFIMAGE);
			Script_FieldChanged(c, owner, parent, field);
		}
		return;

	case GF_SG_VRML_SFNODE:
		/*replace object*/
		if (*((GF_Node**)field->far_ptr)) {
			gf_node_unregister(*((GF_Node**)field->far_ptr), owner);
			*((GF_Node**)field->far_ptr) = NULL;
		}

		p = (GF_JSField *) JS_GetOpaque(val, SFNodeClass.class_id);
		if (JS_IsNull(val)) {
			Script_FieldChanged(c, owner, parent, field);
		} else if (p) {
			GF_Node *n = * (GF_Node**) (p)->field.far_ptr;
			* ((GF_Node **)field->far_ptr) = n;
			gf_node_register(n, owner);
			Script_FieldChanged(c, owner, parent, field);
		}
		return;
	default:
		break;
	}

	p = (GF_JSField *) JS_GetOpaque_Nocheck(val);
	if (!p) return;

	len = p->mfvals_count;
	/*special handling for MF node, reset list first*/
	if (field->fieldType == GF_SG_VRML_MFNODE) {
		GF_Node *child;
		GF_ChildNodeItem *last = NULL;
		gf_node_unregister_children(owner, * (GF_ChildNodeItem **) field->far_ptr);
		* (GF_ChildNodeItem **) field->far_ptr = NULL;

		for (i=0; i<len; i++) {
			item = p->mfvals[i];
			from = (GF_JSField *) JS_GetOpaque(item, SFNodeClass.class_id);
			if (!from) continue;

			child = * ((GF_Node**)from->field.far_ptr);

			gf_node_list_add_child_last( (GF_ChildNodeItem **) field->far_ptr , child, &last);
			gf_node_register(child, owner);
		}
		Script_FieldChanged(c, owner, parent, field);
		/*and mark the field as changed*/
		JSScript_NodeModified(owner->sgprivate->scenegraph, owner, field, NULL);
		return;
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
		item = p->mfvals[i];
		switch (field->fieldType) {
		case GF_SG_VRML_MFBOOL:
			((MFBool*)field->far_ptr)->vals[i] = (Bool) JS_ToBool(c,item);
			break;
		case GF_SG_VRML_MFINT32:
			if (JS_ToInt32(c, &ival, item)) {
				((MFInt32 *)field->far_ptr)->vals[i] = ival;
			}
			break;
		case GF_SG_VRML_MFFLOAT:
			if (JS_ToFloat64(c, &d, item)) {
				((MFFloat *)field->far_ptr)->vals[i] = FLT2FIX( d);
			}
			break;
		case GF_SG_VRML_MFTIME:
			if (JS_ToFloat64(c, &d, item)) {
				((MFTime *)field->far_ptr)->vals[i] = d;
			}
			break;
		case GF_SG_VRML_MFSTRING:
		{
			MFString *mfs = (MFString *) field->far_ptr;
			str_val = JS_ToCString(c, item);
			if (!mfs->vals[i] || strcmp(str_val, mfs->vals[i]) ) {
				if (mfs->vals[i]) gf_free(mfs->vals[i]);
				mfs->vals[i] = gf_strdup(str_val);
				changed = 1;
			}
			JS_FreeCString(c, str_val);
		}
		break;
		case GF_SG_VRML_MFURL:
		{
			MFURL *mfu = (MFURL *) field->far_ptr;
			if (mfu->vals[i].url) gf_free(mfu->vals[i].url);
			str_val = JS_ToCString(c, item);
			mfu->vals[i].url = gf_strdup(str_val);
			mfu->vals[i].OD_ID = 0;
			JS_FreeCString(c, str_val);
		}
		break;

		case GF_SG_VRML_MFVEC2F:
			from = (GF_JSField *) JS_GetOpaque(item, SFVec2fClass.class_id);
			if (from) {
				gf_sg_vrml_field_copy(& ((MFVec2f*)field->far_ptr)->vals[i], from->field.far_ptr, GF_SG_VRML_SFVEC2F);
			}
			break;
		case GF_SG_VRML_MFVEC3F:
			from = (GF_JSField *) JS_GetOpaque(item, SFVec3fClass.class_id);
			if (from) {
				gf_sg_vrml_field_copy(& ((MFVec3f*)field->far_ptr)->vals[i], from->field.far_ptr, GF_SG_VRML_SFVEC3F);
			}
			break;
		case GF_SG_VRML_MFROTATION:
			from = (GF_JSField *) JS_GetOpaque(item, SFRotationClass.class_id);
			if (from) {
				gf_sg_vrml_field_copy(& ((MFRotation*)field->far_ptr)->vals[i], from->field.far_ptr, GF_SG_VRML_SFROTATION);
			}
			break;
		case GF_SG_VRML_MFCOLOR:
			from = (GF_JSField *) JS_GetOpaque(item, SFColorClass.class_id);
			if (from) {
				gf_sg_vrml_field_copy(& ((MFColor*)field->far_ptr)->vals[i], from->field.far_ptr, GF_SG_VRML_SFCOLOR);
			}
			break;

		default:
			break;
		}
	}
	if (changed) Script_FieldChanged(c, owner, parent, field);
}


static void gf_sg_script_update_cached_object(GF_ScriptPriv *priv, JSValue obj, GF_JSField *jsf, GF_FieldInfo *field, GF_Node *parent)
{
	u32 i;

	if ((jsf->field.fieldType != GF_SG_VRML_MFNODE) && ! gf_sg_vrml_is_sf_field(jsf->field.fieldType)) {
		for (i=0; i<jsf->mfvals_count; i++) {
			JS_FreeValue(priv->js_ctx, jsf->mfvals[i]);
			jsf->mfvals[i] = JS_NULL;
		}
	}

	/*we need to rebuild MF types where SF is a native type.*/
	GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[VRML JS] Recomputing cached jsobj\n") );
	switch (jsf->field.fieldType) {
	case GF_SG_VRML_MFBOOL:
	{
		MFBool *f = (MFBool *) field->far_ptr;
		for (i=0; i<f->count; i++) {
			jsf->mfvals[i] = JS_NewBool(priv->js_ctx, f->vals[i]);
		}
	}
	break;
	case GF_SG_VRML_MFINT32:
	{
		MFInt32 *f = (MFInt32 *) field->far_ptr;
		for (i=0; i<f->count; i++) {
			jsf->mfvals[i] = JS_NewInt32(priv->js_ctx, f->vals[i]);
		}
	}
	break;
	case GF_SG_VRML_MFFLOAT:
	{
		MFFloat *f = (MFFloat *) field->far_ptr;
		for (i=0; i<f->count; i++) {
			jsf->mfvals[i] = JS_NewFloat64(priv->js_ctx, FIX2FLT(f->vals[i]));
		}
	}
	break;
	case GF_SG_VRML_MFTIME:
	{
		MFTime *f = (MFTime *) field->far_ptr;
		for (i=0; i<f->count; i++) {
			jsf->mfvals[i] = JS_NewFloat64(priv->js_ctx, f->vals[i]);
		}
	}
	break;
	case GF_SG_VRML_MFSTRING:
	{
		MFString *f = (MFString *) field->far_ptr;
		for (i=0; i<f->count; i++) {
			jsf->mfvals[i] = JS_NewString(priv->js_ctx, f->vals[i]);
		}
	}
	break;
	case GF_SG_VRML_MFURL:
	{
		MFURL *f = (MFURL *) field->far_ptr;
		for (i=0; i<f->count; i++) {
			if (f->vals[i].OD_ID > 0) {
				char msg[30];
				sprintf(msg, "od:%d", f->vals[i].OD_ID);
				jsf->mfvals[i] = JS_NewString(priv->js_ctx, (const char *) msg);
			} else {
				jsf->mfvals[i] = JS_NewString(priv->js_ctx, f->vals[i].url);
			}
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
		u32 j, count;
		JSValue val;
		/*1: find all existing objs for each node*/
		val = JS_GetPropertyStr(priv->js_ctx, jsf->js_list, "length");
		JS_ToInt32(priv->js_ctx, &count, val);
		JS_FreeValue(priv->js_ctx, val);

		/*this may introduce bugs when a child is being replaced through an update command, but it is way
		too costly to handle in script*/
		if (gf_node_list_get_count(f)==count) return;

		JS_SetPropertyStr(priv->js_ctx, jsf->js_list, "length", JS_NewInt32(priv->js_ctx, 0));
		count = 0;
		while (f) {
			GF_JSField *slot = NULL;
			/*first look in the original array*/
			for (j=0; j<count; j++) {
				val = JS_GetPropertyUint32(priv->js_ctx, jsf->js_list, j);
				slot = JS_GetOpaque(val, SFNodeClass.class_id);
				if (slot && (slot->node==f->node)) {
					JS_SetPropertyUint32(priv->js_ctx, jsf->js_list, count, JS_DupValue(priv->js_ctx, val));
					count++;
					break;
				}
				JS_FreeValue(priv->js_ctx, val);
				slot = NULL;
			}
			if (!slot) {
				JS_SetPropertyUint32(priv->js_ctx, jsf->js_list, count, node_get_binding(priv, f->node, 0) );
				count++;
			}
			f = f->next;
		}
		JS_SetPropertyStr(priv->js_ctx, jsf->js_list, "length", JS_NewInt32(priv->js_ctx, count));
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
 
#define SETUP_MF_FIELD(_class)	\
		obj = JS_CallConstructor(priv->js_ctx, priv->_class, 0, NULL);\
		if (JS_IsException(obj) ) return obj; \
		jsf = (GF_JSField *) JS_GetOpaque(obj, _class.class_id);	\
		jsf->owner = parent;		\
		if (parent) gf_node_get_field(parent, field->fieldIndex, &jsf->field);	\
 

static GF_JSClass *get_sf_class(u32 mftype)
{
	switch (mftype) {
	case GF_SG_VRML_MFNODE: return &SFNodeClass;
	case GF_SG_VRML_MFVEC2F: return &SFVec2fClass;
	case GF_SG_VRML_MFVEC3F: return &SFVec3fClass;
	case GF_SG_VRML_MFROTATION: return &SFRotationClass;
	case GF_SG_VRML_MFCOLOR: return &SFColorClass;
	case GF_SG_VRML_MFIMAGE: return &SFImageClass;
	}
	return NULL;
}

JSValue gf_sg_script_to_qjs_field(GF_ScriptPriv *priv, GF_FieldInfo *field, GF_Node *parent, Bool force_evaluate)
{
	u32 i;
	GF_JSField *jsf = NULL;
	GF_JSField *slot = NULL;
	JSValue obj;
	Bool was_found = GF_FALSE;
	/*native types*/
	switch (field->fieldType) {
	case GF_SG_VRML_SFBOOL:
		return JS_NewBool(priv->js_ctx, * ((SFBool *) field->far_ptr) );
	case GF_SG_VRML_SFINT32:
		return JS_NewInt32(priv->js_ctx, * ((SFInt32 *) field->far_ptr));
	case GF_SG_VRML_SFFLOAT:
		return JS_NewFloat64(priv->js_ctx, FIX2FLT(* ((SFFloat *) field->far_ptr) ));
	case GF_SG_VRML_SFTIME:
		return JS_NewFloat64(priv->js_ctx, * ((SFTime *) field->far_ptr));
	case GF_SG_VRML_SFSTRING:
		return JS_NewString(priv->js_ctx, ((SFString *) field->far_ptr)->buffer);
	case GF_SG_VRML_SFURL:
	{
		SFURL *url = (SFURL *)field->far_ptr;
		if (url->OD_ID > 0) {
			char msg[30];
			sprintf(msg, "od:%d", url->OD_ID);
			return JS_NewString(priv->js_ctx, (const char *) msg);
		} else {
			return JS_NewString(priv->js_ctx, (const char *) url->url);
		}
	}
	}

	obj = JS_NULL;

	/*look into object bank in case we already have this object*/
	if (parent && parent->sgprivate->interact && parent->sgprivate->interact->js_binding) {
		i=0;
		while ((jsf = gf_list_enum(parent->sgprivate->interact->js_binding->fields, &i))) {
			was_found = GF_TRUE;
			obj = jsf->obj;

			if (
			    /*make sure we use the same JS context*/
			    (jsf->js_ctx == priv->js_ctx)
			    && (jsf->owner == parent)
			    && (jsf->field.far_ptr==field->far_ptr)
			) {
				Bool do_rebuild = GF_FALSE;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[VRML JS] found cached jsobj (field %s) in script %s bank (%d entries)\n", field->name, gf_node_get_log_name((GF_Node*)JS_GetScript(priv->js_ctx)), gf_list_count(priv->jsf_cache) ) );
				if (!force_evaluate && !jsf->field.NDTtype)
					return JS_DupValue(priv->js_ctx, jsf->obj);

				switch (field->fieldType) {
				//we need to rewrite these
				case GF_SG_VRML_MFVEC2F:
				case GF_SG_VRML_MFVEC3F:
				case GF_SG_VRML_MFROTATION:
				case GF_SG_VRML_MFCOLOR:
					if (force_evaluate) {
						while (jsf->mfvals_count) {
							JS_FreeValue(priv->js_ctx, jsf->mfvals[i]);
							jsf->mfvals_count--;
						}
						gf_free(jsf->mfvals);
						jsf->mfvals = NULL;
						do_rebuild = GF_TRUE;
						break;
					}
				default:
					break;
				}
				if (do_rebuild) {
					break;
				}

				gf_sg_script_update_cached_object(priv, jsf->obj, jsf, field, parent);
				return JS_DupValue(priv->js_ctx, jsf->obj);
			}
			was_found = GF_FALSE;
			obj = JS_NULL;
		}
	}

	if (!was_found) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[VRML JS] creating jsobj %s.%s\n", gf_node_get_name(parent), field->name) );
	}

	switch (field->fieldType) {
	case GF_SG_VRML_SFVEC2F:
		SETUP_FIELD
		obj = JS_NewObjectClass(priv->js_ctx, SFVec2fClass.class_id);
		break;
	case GF_SG_VRML_SFVEC3F:
		SETUP_FIELD
		obj = JS_NewObjectClass(priv->js_ctx, SFVec3fClass.class_id);
		break;
	case GF_SG_VRML_SFROTATION:
		SETUP_FIELD
		obj = JS_NewObjectClass(priv->js_ctx, SFRotationClass.class_id);
		break;
	case GF_SG_VRML_SFCOLOR:
		SETUP_FIELD
		obj = JS_NewObjectClass(priv->js_ctx, SFColorClass.class_id);
		break;
	case GF_SG_VRML_SFIMAGE:
		SETUP_FIELD
		obj = JS_NewObjectClass(priv->js_ctx, SFImageClass.class_id);
		break;
	case GF_SG_VRML_SFNODE:
		if (! *(GF_Node**) field->far_ptr)
			return JS_NULL;

		obj = node_get_binding(priv, *(GF_Node**) field->far_ptr);
		jsf = JS_GetOpaque(obj, SFNodeClass.class_id);
		if (!jsf->owner)
			jsf->owner = parent;
		else
			return JS_DupValue(priv->js_ctx, obj);
			//return obj;
		break;


	case GF_SG_VRML_MFBOOL:
	{
		MFBool *f = (MFBool *) field->far_ptr;
		SETUP_MF_FIELD(MFBoolClass)
		jsf->mfvals_count = f->count;
		jsf->mfvals = gf_realloc(jsf->mfvals, sizeof(JSValue)*f->count);
		for (i = 0; i<f->count; i++) {
			jsf->mfvals[i] = JS_NewBool(priv->js_ctx, f->vals[i]);
		}
		break;
	}
	case GF_SG_VRML_MFINT32:
	{
		MFInt32 *f = (MFInt32 *) field->far_ptr;
		SETUP_MF_FIELD(MFInt32Class)
		jsf->mfvals_count = f->count;
		jsf->mfvals = gf_realloc(jsf->mfvals, sizeof(JSValue)*f->count);
		for (i=0; i<f->count; i++) {
			jsf->mfvals[i] = JS_NewInt32(priv->js_ctx, f->vals[i]);
		}
		break;
	}
	case GF_SG_VRML_MFFLOAT:
	{
		MFFloat *f = (MFFloat *) field->far_ptr;
		SETUP_MF_FIELD(MFFloatClass)
		jsf->mfvals_count = f->count;
		jsf->mfvals = gf_realloc(jsf->mfvals, sizeof(JSValue)*f->count);
		for (i=0; i<f->count; i++) {
			jsf->mfvals[i] = JS_NewFloat64(priv->js_ctx, FIX2FLT(f->vals[i]) );
		}
		break;
	}
	case GF_SG_VRML_MFTIME:
	{
		MFTime *f = (MFTime *) field->far_ptr;
		SETUP_MF_FIELD(MFTimeClass)
		jsf->mfvals_count = f->count;
		jsf->mfvals = gf_realloc(jsf->mfvals, sizeof(JSValue)*f->count);
		for (i=0; i<f->count; i++) {
			jsf->mfvals[i] = JS_NewFloat64(priv->js_ctx, f->vals[i] );
		}
		break;
	}
	case GF_SG_VRML_MFSTRING:
	{
		MFString *f = (MFString *) field->far_ptr;
		SETUP_MF_FIELD(MFStringClass)
		jsf->mfvals_count = f->count;
		jsf->mfvals = gf_realloc(jsf->mfvals, sizeof(JSValue)*f->count);
		for (i=0; i<f->count; i++) {
			char *str = f->vals[i];
			if (!str) str= "";
			jsf->mfvals[i] = JS_NewString(priv->js_ctx, str );
		}
		break;
	}
	case GF_SG_VRML_MFURL:
	{
		MFURL *f = (MFURL *) field->far_ptr;
		SETUP_MF_FIELD(MFUrlClass)
		jsf->mfvals_count = f->count;
		jsf->mfvals = gf_realloc(jsf->mfvals, sizeof(JSValue)*f->count);
		for (i=0; i<f->count; i++) {
			if (f->vals[i].OD_ID > 0) {
				char msg[30];
				sprintf(msg, "od:%d", f->vals[i].OD_ID);
				jsf->mfvals[i] = JS_NewString(priv->js_ctx, (const char *) msg);
			} else {
				jsf->mfvals[i] = JS_NewString(priv->js_ctx, f->vals[i].url);
			}
		}
		break;
	}

	case GF_SG_VRML_MFVEC2F:
	{
		MFVec2f *f = (MFVec2f *) field->far_ptr;
		if (!was_found) {
			SETUP_MF_FIELD(MFVec2fClass)
		}
		jsf->mfvals_count = f->count;
		jsf->mfvals = gf_realloc(jsf->mfvals, sizeof(JSValue)*f->count);
		for (i=0; i<f->count; i++) {
			JSValue pf = JS_NewObjectClass(priv->js_ctx, SFVec2fClass.class_id);
			slot = SFVec2f_Create(priv->js_ctx, pf, f->vals[i].x, f->vals[i].y);
			slot->owner = parent;
			jsf->mfvals[i] = pf;
		}
		break;
	}
	case GF_SG_VRML_MFVEC3F:
	{
		MFVec3f *f = (MFVec3f *) field->far_ptr;
		if (!was_found) {
			SETUP_MF_FIELD(MFVec3fClass)
		}
		jsf->mfvals_count = f->count;
		jsf->mfvals = gf_realloc(jsf->mfvals, sizeof(JSValue)*f->count);
		for (i=0; i<f->count; i++) {
			JSValue pf = JS_NewObjectClass(priv->js_ctx, SFVec3fClass.class_id);
			slot = SFVec3f_Create(priv->js_ctx, pf, f->vals[i].x, f->vals[i].y, f->vals[i].z);
			slot->owner = parent;
			jsf->mfvals[i] = pf;
		}
		break;
	}
	case GF_SG_VRML_MFROTATION:
	{
		MFRotation *f = (MFRotation*) field->far_ptr;
		if (!was_found) {
			SETUP_MF_FIELD(MFRotationClass)
		}
		jsf->mfvals_count = f->count;
		jsf->mfvals = gf_realloc(jsf->mfvals, sizeof(JSValue)*f->count);
		for (i=0; i<f->count; i++) {
			JSValue pf = JS_NewObjectClass(priv->js_ctx, SFRotationClass.class_id);
			slot = SFRotation_Create(priv->js_ctx, pf, f->vals[i].x, f->vals[i].y, f->vals[i].z, f->vals[i].q);
			slot->owner = parent;
			jsf->mfvals[i] = pf;
		}
		break;
	}
	case GF_SG_VRML_MFCOLOR:
	{
		MFColor *f = (MFColor *) field->far_ptr;
		if (!was_found) {
			SETUP_MF_FIELD(MFColorClass)
		}
		jsf->mfvals_count = f->count;
		jsf->mfvals = gf_realloc(jsf->mfvals, sizeof(JSValue)*f->count);
		for (i=0; i<f->count; i++) {
			JSValue pf = JS_NewObjectClass(priv->js_ctx, SFColorClass.class_id);
			slot = SFColor_Create(priv->js_ctx, pf, f->vals[i].red, f->vals[i].green, f->vals[i].blue);
			slot->owner = parent;
			jsf->mfvals[i] = pf;
		}
		break;
	}

	case GF_SG_VRML_MFNODE:
	{
//		GF_ChildNodeItem *f = * ((GF_ChildNodeItem **)field->far_ptr);
		SETUP_MF_FIELD(MFNodeClass)
		assert(!jsf->mfvals);
		jsf->mfvals_count = 0;
		/*we don't get the binding until the node is requested, and we don't store it in the MFVals value*/
		break;
	}

	//not supported
	default:
		return JS_NULL;
	}
	if (JS_IsNull(obj))
		return obj;

	if (!jsf) {
		jsf = JS_GetOpaque_Nocheck(obj);
		assert(jsf);
	}
	//store field associated with object if needed
	JS_SetOpaque(obj, jsf);
	if (!was_found) {
		jsf->obj = obj;
	}
	//for createVRMLFromString only, no parent
	if (!parent)
		return obj;

	assert(jsf->owner);

	/*obj corresponding to an existing field/node, store it and prevent GC on object*/
	/*remember the object*/
	if (!parent->sgprivate->interact) GF_SAFEALLOC(parent->sgprivate->interact, struct _node_interactive_ext);
	if (!parent->sgprivate->interact) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[VRMLJS] Failed to create interact storage\n"));
		return JS_NULL;
	}
	if (!parent->sgprivate->interact->js_binding) {
		GF_SAFEALLOC(parent->sgprivate->interact->js_binding, struct _node_js_binding);
		if (!parent->sgprivate->interact->js_binding) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[VRMLJS] Failed to create JS bindings storage\n"));
			return JS_NULL;
		}
		parent->sgprivate->interact->js_binding->fields = gf_list_new();
		if (!parent->sgprivate->interact->js_binding->fields) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[VRMLJS] Failed to create JS bindings storage\n"));
			return JS_NULL;
		}
	}

	if ( gf_list_find(parent->sgprivate->interact->js_binding->fields, jsf) < 0) {
		assert(jsf->owner == parent);
		gf_list_add(parent->sgprivate->interact->js_binding->fields, jsf);
	}

	if (!was_found) {
		if (gf_list_find(priv->jsf_cache, jsf)<0)
			gf_list_add(priv->jsf_cache, jsf);
	} else {
		assert (gf_list_find(priv->jsf_cache, jsf)>=0);
	}

	/*our JS Array object (MFXXX) are always rooted and added to the cache upon construction*/
	if (jsf->mfvals) {
		GF_JSClass *sf_class;
		u32 count = jsf->mfvals_count;

		sf_class = get_sf_class(jsf->field.fieldType);
		if (!sf_class) count=0;

		for (i=0; i<count; i++) {
			JSValue item = jsf->mfvals[i];
			GF_JSField *afield = JS_GetOpaque(item, sf_class->class_id);
			if (afield) {
				if (afield->owner != parent) {
					continue;
				}
				if ( gf_list_find(parent->sgprivate->interact->js_binding->fields, afield) < 0) {
					gf_list_add(parent->sgprivate->interact->js_binding->fields, afield);
				}
			}
		}
	}
	parent->sgprivate->flags |= GF_NODE_HAS_BINDING;
	return JS_DupValue(priv->js_ctx, jsf->obj);
}

static void JS_ReleaseRootObjects(GF_ScriptPriv *priv)
{
	JSRuntime *js_runtime = gf_js_get_rt();

	/*pop the list rather than walk through it since unprotecting an element could trigger GC which in turn could modify this list content*/
	while (gf_list_count(priv->jsf_cache)) {
		JSValue obj;
		GF_JSField *jsf = gf_list_pop_back(priv->jsf_cache);
		assert(jsf);

		/*				!!! WARNING !!!

		GC is handled at the JSRuntime level, not at the JSContext level.
		Objects may not be finalized until the runtime is destroyed/GC'ed, which is not what we want.
		We therefore destroy by hand all SFNode (obj rooted) and MFNode (for js_list)
		*/

		obj = jsf->obj;
		jsf->obj = JS_UNDEFINED;
		if (jsf->mfvals && js_runtime)
			array_finalize_ex(js_runtime, obj, 0);
		else if (jsf->node && js_runtime)
			node_finalize_ex(js_runtime, obj, 0);
		else
			jsf->js_ctx=NULL;

		JS_FreeValue(priv->js_ctx, obj);
	}
}

static void JS_PreDestroy(GF_Node *node)
{
	GF_SceneGraph *scene;
	GF_ScriptPriv *priv = node->sgprivate->UserPrivate;
	if (!priv) return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[Script] Destroying script node %s", gf_node_get_log_name(node) ));

	/*"shutdown" is no longer supported, as it is typically called when one of a parent node is destroyed through
	a GC call.*/

	gf_js_lock(priv->js_ctx, 1);

	JS_FreeValue(priv->js_ctx, priv->the_event);

	JS_FreeValue(priv->js_ctx, priv->node_toString_fun);
	JS_FreeValue(priv->js_ctx, priv->node_getTime_fun);
#ifndef GPAC_DISABLE_SVG
	JS_FreeValue(priv->js_ctx, priv->node_addEventListener_fun);
	JS_FreeValue(priv->js_ctx, priv->node_removeEventListener_fun);
#endif

	/*unprotect all cached objects from GC*/
	JS_ReleaseRootObjects(priv);

#ifndef GPAC_DISABLE_SVG
	gf_sg_js_dom_pre_destroy(JS_GetRuntime(priv->js_ctx), node->sgprivate->scenegraph, NULL);
#endif

	JS_FreeValue(priv->js_ctx, priv->js_obj);


	scene = JS_GetContextOpaque(priv->js_ctx);
	if (scene && scene->__reserved_null) {
		GF_Node *n = JS_GetContextOpaque(priv->js_ctx);
		scene = n->sgprivate->scenegraph;
	}
	if (scene && scene->attached_session) {
		void gf_fs_unload_js_api(JSContext *c, GF_FilterSession *fs);

		gf_fs_unload_js_api(priv->js_ctx, scene->attached_session);
	}


	gf_js_lock(priv->js_ctx, 0);

	gf_js_delete_context(priv->js_ctx);

#ifndef GPAC_DISABLE_SVG
	dom_js_unload();
#endif

	gf_list_del(priv->jsf_cache);

	priv->js_ctx = NULL;

	/*unregister script from parent scene (cf base_scenegraph::sg_reset) */
	gf_list_del_item(node->sgprivate->scenegraph->scripts, node);
}


static void JS_InitScriptFields(GF_ScriptPriv *priv, GF_Node *sc)
{
	u32 i;
	GF_ScriptField *sf;
	GF_FieldInfo info;
	JSValue val;
	JSAtom atom;

	i=0;
	while ((sf = gf_list_enum(priv->fields, &i))) {
		switch (sf->eventType) {
		case GF_SG_EVENT_IN:
			//nothing to do
			break;
		case GF_SG_EVENT_OUT:
			gf_node_get_field(sc, sf->ALL_index, &info);
			//do not assign a value, eventOut are write-only fields
			/*create a setter function for this field to be notified whenever modified*/
			sf->magic = i;
            JSValue setter = JS_NewCFunction2(priv->js_ctx, (JSCFunction *) gf_sg_script_eventout_set_prop, sf->name, 1, JS_CFUNC_setter_magic, sf->magic);

			atom = JS_NewAtom(priv->js_ctx, sf->name);
			JS_DefinePropertyGetSet(priv->js_ctx, priv->js_obj, atom, JS_UNDEFINED, setter, 0);
			JS_FreeAtom(priv->js_ctx, atom);
			break;
		default:
			/*get field value and define in in global scope*/
			gf_node_get_field(sc, sf->ALL_index, &info);
			val = gf_sg_script_to_qjs_field(priv, &info, sc, 0);
			JS_SetPropertyStr(priv->js_ctx, priv->js_obj, (const char *) sf->name, val);
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
	JSValue fval, rval;
	Double time;
	JSValue argv[2];
	GF_ScriptField *sf;
	GF_FieldInfo t_info;
	GF_ScriptPriv *priv;
	u32 i;
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

	gf_js_lock(priv->js_ctx, 1);

	fval = JS_GetPropertyStr(priv->js_ctx, priv->js_obj, sf->name);
	if (JS_IsUndefined(fval) || JS_IsNull(fval) || !JS_IsFunction(priv->js_ctx, fval)) {
		gf_js_lock(priv->js_ctx, 0);
		JS_FreeValue(priv->js_ctx, fval);
		return;
	}

	argv[0] = gf_sg_script_to_qjs_field(priv, in_field, node, 1);

	memset(&t_info, 0, sizeof(GF_FieldInfo));
	t_info.far_ptr = &sf->last_route_time;
	t_info.fieldType = GF_SG_VRML_SFTIME;
	t_info.fieldIndex = -1;
	t_info.name = "timestamp";
	argv[1] = gf_sg_script_to_qjs_field(priv, &t_info, node, 1);

	rval = JS_Call(priv->js_ctx, fval, priv->js_obj, 2, argv);
	if (JS_IsException(rval)) {
		js_dump_error(priv->js_ctx);
	}

	JS_FreeValue(priv->js_ctx, fval);
	JS_FreeValue(priv->js_ctx, rval);
	JS_FreeValue(priv->js_ctx, argv[0]);
	JS_FreeValue(priv->js_ctx, argv[1]);

	js_std_loop(priv->js_ctx);
	gf_js_lock(priv->js_ctx, 0);

	gf_js_vrml_flush_event_out(node, priv);

	do_js_gc(priv->js_ctx, node);
}


static Bool vrml_js_load_script(M_Script *script, char *file, Bool primary_script, JSValue *rval)
{
	char *jsscript;
	u32 fsize;
	Bool success = 1;
	u32 flags = JS_EVAL_TYPE_GLOBAL;
	JSValue ret;
	GF_Err e;
	GF_ScriptPriv *priv = (GF_ScriptPriv *) script->sgprivate->UserPrivate;

	*rval = JS_UNDEFINED;

	e = gf_file_load_data(file, (u8 **) &jsscript, &fsize);
	if (e) {
		return GF_FALSE;
	}

 	if (!gf_opts_get_bool("core", "no-js-mods") && JS_DetectModule((const char *) jsscript, fsize )) {
		flags = JS_EVAL_TYPE_MODULE;
		priv->use_strict = GF_TRUE;
	}
	//we use JS_EVAL_TYPE_MODULE only for GUI at the current time. However the GUI script is still old and not
	//compliant to strict mode, using a lot of global functions
	//the code below is correct (we should always stick to strict if modules) but deactivated until we rewrite the GUI
//	if (priv->use_strict)
//		flags = JS_EVAL_TYPE_MODULE;


	ret = JS_Eval(priv->js_ctx, jsscript, fsize, file, flags);
	if (JS_IsException(ret)) {
		success = 0;
		js_dump_error(priv->js_ctx);
	}
	if (!success || primary_script)
		JS_FreeValue(priv->js_ctx, ret);
	else
		*rval = ret;

	if (success && primary_script) {
		JSValue fun = JS_GetPropertyStr(priv->js_ctx, priv->js_obj, "initialize");
		if (JS_IsFunction(priv->js_ctx, fun)) {
			ret = JS_Call(priv->js_ctx, fun, priv->js_obj, 0, NULL);
			if (JS_IsException(ret)) {
				js_dump_error(priv->js_ctx);
			}
			gf_js_vrml_flush_event_out((GF_Node *)script, priv);
			JS_FreeValue(priv->js_ctx, ret);
		}
		JS_FreeValue(priv->js_ctx, fun);
	}
	gf_free(jsscript);
	if (success)
		js_std_loop(priv->js_ctx);
	return success;
}

/*fetches each listed URL and attempts to load the script - this is SYNCHRONOUS*/
Bool JSScriptFromFile(GF_Node *node, const char *opt_file, Bool no_complain, JSValue *rval)
{
	GF_JSAPIParam par;
	u32 i;
	GF_DownloadManager *dnld_man;
	GF_Err e;
	M_Script *script = (M_Script *)node;

	e = GF_SCRIPT_ERROR;

	par.dnld_man = NULL;
	ScriptAction(NULL, node->sgprivate->scenegraph, GF_JSAPI_OP_GET_DOWNLOAD_MANAGER, NULL, &par);
	if (!par.dnld_man) return 0;
	dnld_man = par.dnld_man;

	for (i=0; i<script->url.count; i++) {
		char *url;
		const char *ext;
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

		ext = gf_file_ext_start(url);
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
	u32 i;
	u32 flags = JS_EVAL_TYPE_GLOBAL;
	Bool local_script;
	JSValue rval;
	M_Script *script = (M_Script *)node;
	GF_ScriptPriv *priv = (GF_ScriptPriv *) node->sgprivate->UserPrivate;

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
	priv->js_ctx = gf_js_create_context();
	if (!priv->js_ctx) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[VRML JS] Cannot allocate ECMAScript context for node\n"));
		return;
	}

	gf_js_lock(priv->js_ctx, 1);

	JS_SetContextOpaque(priv->js_ctx, node);
	vrml_js_init_api(priv, node);

	qjs_init_all_modules(priv->js_ctx, GF_TRUE, GF_TRUE);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[VRML JS] Built-in classes initialized\n"));
#ifndef GPAC_DISABLE_SVG
	/*initialize DOM*/
	dom_js_load(node->sgprivate->scenegraph, priv->js_ctx);
	/*create event object, and remember it*/
	priv->the_event = dom_js_define_event(priv->js_ctx);
	priv->the_event = JS_DupValue(priv->js_ctx, priv->the_event);
#endif
	qjs_module_init_xhr_global(priv->js_ctx, priv->js_obj);

	priv->jsf_cache = gf_list_new();

	/*setup fields interfaces*/
	JS_InitScriptFields(priv, node);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[VRML JS] script fields initialized\n"));

	priv->JS_PreDestroy = JS_PreDestroy;
	priv->JS_EventIn = JS_EventIn;

	if (!local_script) {
		JSScriptFromFile(node, NULL, 0, &rval);
		gf_js_lock(priv->js_ctx, 0);
		return;
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[VRML JS] Evaluating script %s\n", str));

 	if (!gf_opts_get_bool("core", "no-js-mods") && JS_DetectModule((const char *) str, (u32) strlen(str) ))
		flags = JS_EVAL_TYPE_MODULE;

	JSValue ret = JS_Eval(priv->js_ctx, str, (u32) strlen(str), "inline_script", flags);
	if (!JS_IsException(ret)) {
		JSValue fval = JS_GetPropertyStr(priv->js_ctx, priv->js_obj, "initialize");
		if (JS_IsFunction(priv->js_ctx, fval) && !JS_IsUndefined(fval)) {
			JSValue r, args[2];
			args[0] = JS_TRUE;
			args[1] = JS_NewFloat64(priv->js_ctx, gf_node_get_scene_time(node) );
			r = JS_Call(priv->js_ctx, fval, priv->js_obj, 2, args);
			if (JS_IsException(r)) {
				js_dump_error(priv->js_ctx);
			}
			JS_FreeValue(priv->js_ctx, r);
			JS_FreeValue(priv->js_ctx, args[0]);
			JS_FreeValue(priv->js_ctx, args[1]);

			gf_js_vrml_flush_event_out(node, priv);
		}
		JS_FreeValue(priv->js_ctx, fval);
	} else {
		js_dump_error(priv->js_ctx);
	}

	JS_FreeValue(priv->js_ctx, ret);
	js_std_loop(priv->js_ctx);

	gf_js_lock(priv->js_ctx, 0);

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


#ifndef GPAC_DISABLE_PLAYER
static void JSScript_NodeModified(GF_SceneGraph *sg, GF_Node *node, GF_FieldInfo *info, GF_Node *script)
{
	u32 i;
	GF_JSField *jsf;
	/*this is REPLACE NODE signaling*/
	if (script) {
		u32 count;
		GF_ScriptPriv *priv = gf_node_get_private(script);
		count = gf_list_count(priv->jsf_cache);

		for (i=0; i<count; i++) {
			jsf = gf_list_get(priv->jsf_cache, i);
			assert(jsf);
			if (jsf->node && (jsf->node==node)) {
				//detach node and its js binding
				jsf->node = NULL;
				node->sgprivate->interact->js_binding->pf = NULL;
				node->sgprivate->interact->js_binding->obj = JS_UNDEFINED;
			}
		}
		return;
	}

	if (!info) {
		/*handle DOM case*/
		if ((node->sgprivate->tag>=GF_NODE_FIRST_PARENT_NODE_TAG)
		        && node->sgprivate->interact
		        && node->sgprivate->interact->js_binding
		        && !JS_IsUndefined(node->sgprivate->interact->js_binding->obj))
		{

			if (gf_list_del_item(sg->objects, node->sgprivate->interact->js_binding)>=0) {
#ifndef GPAC_DISABLE_SVG
				void svg_free_node_binding(struct __tag_svg_script_ctx *svg_js, GF_Node *node);
				svg_free_node_binding(sg->svg_js, node);
#endif
			}
			return;
		}
		/*this is unregister signaling*/
		if (!node->sgprivate->parents && node->sgprivate->interact && node->sgprivate->interact->js_binding && node->sgprivate->interact->js_binding->pf) {
			GF_JSField *field = node->sgprivate->interact->js_binding->pf;
			/*if this is the last occurence of the obj (no longer in JS), remove node binding*/
			if (JS_VALUE_HAS_REF_COUNT(field->obj)) {
				JSRefCountHeader *p = (JSRefCountHeader *)JS_VALUE_GET_PTR(field->obj);
				if (p->ref_count==1) {
					//store obj before calling JS_ObjectDestroyed which will set field->obj to JS_UNDEFINED
					JSValue obj = field->obj;
					JS_ObjectDestroyed(JS_GetRuntime(field->js_ctx), obj, field, 1);
					JS_FreeValue(field->js_ctx, obj);
					gf_free(field);
					assert( node->sgprivate->interact->js_binding->pf==NULL);
					//unregister node since we destroyed the binding
					gf_node_unregister(node, NULL);
				}
			}
			return;
		}

		/*final destroy*/
		if (!node->sgprivate->num_instances) {
			i=0;
			while (node->sgprivate->interact && node->sgprivate->interact->js_binding && (jsf = gf_list_enum(node->sgprivate->interact->js_binding->fields, &i))) {
				jsf->owner = NULL;

				if (jsf->mfvals) {
					u32 j;

					if (jsf->field.fieldType != GF_SG_VRML_MFNODE) {
						u32 count = jsf->mfvals_count;
						GF_JSClass *sf_class = get_sf_class(jsf->field.fieldType);

						if (!sf_class) count=0;

						for (j=0; j<count; j++) {
							JSValue item = jsf->mfvals[j];
							GF_JSField *afield = JS_GetOpaque(item, sf_class->class_id);
							if (afield) {
								afield->owner = NULL;
							}
						}
					}
					while (jsf->mfvals_count) {
						jsf->mfvals_count--;
						JS_FreeValue(jsf->js_ctx, jsf->mfvals[jsf->mfvals_count]);
					}
					gf_free(jsf->mfvals);
					jsf->mfvals = NULL;
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
#endif

GF_EXPORT
void gf_sg_handle_dom_event_for_vrml(GF_Node *node, GF_DOM_Event *event, GF_Node *observer)
{
#ifndef GPAC_DISABLE_SVG
	GF_ScriptPriv *priv;
	Bool prev_type;
	//JSBool ret = JS_FALSE;
	GF_DOM_Event *prev_event = NULL;
	SVG_handlerElement *hdl;
	JSValue ret;
	JSValue evt;
	JSValue argv[1];

	hdl = (SVG_handlerElement *) node;
	if (!hdl->js_data) return;
	if (!JS_IsFunction(hdl->js_data->ctx, hdl->js_data->fun_val) && !JS_IsUndefined(hdl->js_data->evt_listen_obj)) {
		return;
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_INTERACT, ("[DOM Events] Executing script code from VRML handler\n"));

	priv = JS_GetScriptStack(hdl->js_data->ctx);
	gf_js_lock(priv->js_ctx, 1);

	prev_event = JS_GetOpaque_Nocheck(priv->the_event);
	/*break loops*/
	if (prev_event && (prev_event->type==event->type) && (prev_event->target==event->target)) {
		gf_js_lock(priv->js_ctx, 0);
		return;
	}

	evt = gf_dom_new_event(priv->js_ctx);
	if (JS_IsUndefined(evt) || JS_IsNull(evt)) {
		gf_js_lock(priv->js_ctx, 0);
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[DOM Events] Cannot create JavaScript dom event for event type %d\n", event->type));
		return;
	}

	prev_type = event->is_vrml;
	event->is_vrml = 1;
	JS_SetOpaque(priv->the_event, event);

	JS_SetOpaque(evt, event);
	argv[0] = evt;

	if (JS_IsFunction(hdl->js_data->ctx, hdl->js_data->fun_val)) {
		JSValue listenObj = priv->js_obj;
		if (!JS_IsNull(hdl->js_data->evt_listen_obj) && !JS_IsUndefined(hdl->js_data->evt_listen_obj))
			listenObj = hdl->js_data->evt_listen_obj;

		ret = JS_Call(hdl->js_data->ctx, hdl->js_data->fun_val, listenObj, 1, argv);
	} else {
		JSValue fun = JS_GetPropertyStr(priv->js_ctx, hdl->js_data->evt_listen_obj, "handleEvent");
		if (JS_IsFunction(priv->js_ctx, fun)) {
			ret = JS_Call(priv->js_ctx, fun, hdl->js_data->evt_listen_obj, 1, argv);
		} else {
			ret = JS_UNDEFINED;
		}
		JS_FreeValue(priv->js_ctx, fun);
	}
	if (JS_IsException(ret)) {
		js_dump_error(priv->js_ctx);
	}
	JS_FreeValue(priv->js_ctx, ret);
	JS_FreeValue(priv->js_ctx, evt);

	event->is_vrml = prev_type;
	JS_SetOpaque(priv->the_event, prev_event);

	js_std_loop(priv->js_ctx);
	gf_js_lock(priv->js_ctx, 0);

#endif
}

#endif	/*GPAC_DISABLE_VRML*/

#endif /*GPAC_HAS_QJS*/



/*set JavaScript interface*/
void gf_sg_set_script_action(GF_SceneGraph *scene, gf_sg_script_action script_act, void *cbk)
{
	if (!scene) return;
	scene->script_action = script_act;
	scene->script_action_cbck = cbk;

#if defined(GPAC_HAS_QJS) && !defined(GPAC_DISABLE_VRML)
	scene->script_load = JSScript_Load;
	scene->on_node_modified = JSScript_NodeModified;
#endif

}

#ifdef GPAC_HAS_QJS

GF_EXPORT
GF_Node *gf_sg_js_get_node(JSContext *c, JSValue obj)
{
#ifndef GPAC_DISABLE_VRML
	GF_JSField *ptr = (GF_JSField *) JS_GetOpaque(obj, SFNodeClass.class_id);
	if (ptr && (ptr->field.fieldType==GF_SG_VRML_SFNODE))
		return * ((GF_Node **)ptr->field.far_ptr);
#endif

#ifndef GPAC_DISABLE_SVG
	{
		Bool has_p = 0;
		JSValue ns = JS_GetPropertyStr(c, obj, "namespaceURI");
		if (!JS_IsNull(ns) && !JS_IsUndefined(ns)) has_p = 1;
		JS_FreeValue(c, ns);
		if (has_p) return dom_get_element(c, obj);
	}
#endif
	return NULL;
}

#endif

GF_EXPORT
Bool gf_sg_has_scripting()
{
#ifdef GPAC_HAS_QJS
	return 1;
#else
	return 0;
#endif
}
