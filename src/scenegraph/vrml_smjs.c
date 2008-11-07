/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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

/*MPEG4 & X3D tags (for node tables & script handling)*/
#include <gpac/nodes_mpeg4.h>
#include <gpac/nodes_x3d.h>

#ifdef GPAC_HAS_SPIDERMONKEY

#ifndef GPAC_DISABLE_SVG
/*SVG tags for script handling*/
#include <gpac/nodes_svg.h>
#endif

#include <gpac/internal/terminal_dev.h>
#include <gpac/modules/js_usr.h>

#include <jsapi.h> 

void gf_sg_script_to_node_field(struct JSContext *c, jsval v, GF_FieldInfo *field, GF_Node *owner, GF_JSField *parent);
jsval gf_sg_script_to_smjs_field(GF_ScriptPriv *priv, GF_FieldInfo *field, GF_Node *parent, Bool no_cache, Bool force_evaluate);

JSBool js_has_instance(JSContext *c, JSObject *obj, jsval val, JSBool *vp);

#define _ScriptMessage(_c, _e, _msg) {	\
		GF_Node *_n = (GF_Node *) JS_GetContextPrivate(_c);	\
		if (_n->sgprivate->scenegraph->script_action) {\
			GF_JSAPIParam par;	\
			par.info.e = (_e);			\
			par.info.msg = (_msg);		\
			_n->sgprivate->scenegraph->script_action(_n->sgprivate->scenegraph->script_action_cbck, GF_JSAPI_OP_MESSAGE, NULL, &par);\
		}	\
		}

Bool ScriptAction(JSContext *c, GF_SceneGraph *scene, u32 type, GF_Node *node, GF_JSAPIParam *param)
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
	JSClass globalClass;
	JSClass browserClass;
	JSClass SFNodeClass;
	JSClass SFVec2fClass;
	JSClass SFVec3fClass;
	JSClass SFRotationClass;
	JSClass SFColorClass;
	JSClass SFImageClass;
	JSClass MFInt32Class;
	JSClass MFBoolClass;
	JSClass MFFloatClass;
	JSClass MFTimeClass;
	JSClass MFVec2fClass;
	JSClass MFVec3fClass;
	JSClass MFRotationClass;
	JSClass MFColorClass;
	JSClass MFStringClass;
	JSClass MFUrlClass;
	JSClass MFNodeClass;

	/*extensions are loaded for the lifetime of the runtime NOT of the context - this avoids nasty
	crashes with multiple contexts in SpiderMonkey (root'ing bug with InitStandardClasses)*/
	GF_List *extensions;
} GF_JSRuntime;

static GF_JSRuntime *js_rt = NULL;




void gf_sg_load_script_extensions(GF_SceneGraph *sg, JSContext *c, JSObject *obj, Bool unload)
{
	u32 i, count;
	count = gf_list_count(js_rt->extensions); 
	for (i=0; i<count; i++) {
		GF_JSUserExtension *ext = (GF_JSUserExtension *) gf_list_get(js_rt->extensions, i);
		ext->load(ext, sg, c, obj, unload);
	}
}



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
	case 0: col->red = val; col->green = t; col->blue = p; break;
	case 1: col->red = q; col->green = val; col->blue = p; break;
	case 2: col->red = p; col->green = val; col->blue = t; break;
	case 3: col->red = p; col->green = q; col->blue = val; break;
	case 4: col->red = t; col->green = p; col->blue = val; break;
	case 5: col->red = val; col->green = p; col->blue = q; break;
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

static void vrml_node_register(GF_Node *node, GF_Node *parent)
{
	if (node) {
		node->sgprivate->flags |= GF_NODE_HAS_BINDING;
		gf_node_register(node, parent);
	}
}

static GFINLINE GF_JSField *NewJSField()
{
	GF_JSField *ptr;
	GF_SAFEALLOC(ptr, GF_JSField)
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
//	GF_JSInterface *ifce = JS_GetInterface(c);
//	if (ifce) ifce->ScriptMessage(ifce->callback, GF_SCRIPT_ERROR, msg);
	GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[JavaScript] Error: %s", msg));
}

static JSBool JSPrint(JSContext *c, JSObject *p, uintN argc, jsval *argv, jsval *rval)
{
	u32 i;
	char buf[5000];

	strcpy(buf, "");
	for (i = 0; i < argc; i++) {
		JSString *str = JS_ValueToString(c, argv[i]);
		if (!str) return JS_FALSE;
		if (i) strcat(buf, " ");
		strcat(buf, JS_GetStringBytes(str));
	}
	_ScriptMessage(c, GF_SCRIPT_INFO, buf);
	return JS_TRUE;
}

static JSBool getName(JSContext *c, JSObject *obj, uintN n, jsval *v, jsval *rval)
{
	JSString *s = JS_NewStringCopyZ(c, "GPAC RichMediaEngine");
	if (!s) return JS_FALSE;
	*rval = STRING_TO_JSVAL(s); 
	return JS_TRUE;
}
static JSBool getVersion(JSContext*c, JSObject*obj, uintN n, jsval *v, jsval *rval)
{
	JSString *s = JS_NewStringCopyZ(c, GPAC_FULL_VERSION);
	if (!s) return JS_FALSE;
	*rval = STRING_TO_JSVAL(s); 
	return JS_TRUE;
}
static JSBool getCurrentSpeed(JSContext *c, JSObject *o, uintN n, jsval *v, jsval *rval)
{
	GF_JSAPIParam par;
	GF_Node *node = JS_GetContextPrivate(c);
	par.time = 0;
	ScriptAction(c, NULL, GF_JSAPI_OP_GET_SPEED, node->sgprivate->scenegraph->RootNode, &par);
	*rval = DOUBLE_TO_JSVAL(JS_NewDouble( c, par.time));
	return JS_TRUE;
}
static JSBool getCurrentFrameRate(JSContext *c, JSObject*o, uintN n, jsval *v, jsval*rval)
{
	GF_JSAPIParam par;
	GF_Node *node = JS_GetContextPrivate(c);
	par.time = 0;
	ScriptAction(c, NULL, GF_JSAPI_OP_GET_FPS, node->sgprivate->scenegraph->RootNode, &par);
	*rval = DOUBLE_TO_JSVAL(JS_NewDouble( c, par.time));
	return JS_TRUE;
}
static JSBool getWorldURL(JSContext*c, JSObject*obj, uintN n, jsval *v, jsval *rval)
{
	GF_JSAPIParam par;
	GF_Node *node = JS_GetContextPrivate(c);
	if (ScriptAction(c, NULL, GF_JSAPI_OP_GET_SCENE_URI, node->sgprivate->scenegraph->RootNode, &par)) {
		JSString *s = JS_NewStringCopyZ(c, par.uri.url);
		if (!s) return JS_FALSE;
		*rval = STRING_TO_JSVAL(s);
		return JS_TRUE;
	}
	return JS_FALSE;
}
static JSBool getScript(JSContext*c, JSObject*obj, uintN n, jsval *v, jsval *rval)
{
	JSObject *an_obj;
	GF_JSField *field;
	GF_ScriptPriv *priv = JS_GetScriptStack(c);
	GF_Node *node = JS_GetContextPrivate(c);

	vrml_node_register(node, NULL);
	field = NewJSField();
	field->field.fieldType = GF_SG_VRML_SFNODE;
	field->temp_node = node;
	field->field.far_ptr = &field->temp_node;

	an_obj = JS_NewObject(priv->js_ctx, &js_rt->SFNodeClass, 0, priv->js_obj);
	field->obj = an_obj;
	JS_SetPrivate(c, an_obj, field);
	*rval = OBJECT_TO_JSVAL(an_obj);
	return JS_TRUE;
}
static JSBool getProto(JSContext*c, JSObject*obj, uintN n, jsval *v, jsval *rval)
{
	JSObject *an_obj;
	GF_JSField *field;
	GF_ScriptPriv *priv = JS_GetScriptStack(c);
	GF_Node *node = JS_GetContextPrivate(c);
	if (!node->sgprivate->scenegraph->pOwningProto) {
		*rval = JSVAL_NULL;
		return JS_TRUE;
	}
	node = (GF_Node *) node->sgprivate->scenegraph->pOwningProto;
	vrml_node_register(node, NULL);
	field = NewJSField();
	field->field.fieldType = GF_SG_VRML_SFNODE;
	field->temp_node = node;
	field->field.far_ptr = &field->temp_node;

	an_obj = JS_NewObject(priv->js_ctx, &js_rt->SFNodeClass, 0, priv->js_obj);
	field->obj = an_obj;
	JS_SetPrivate(c, an_obj, field);
	*rval = OBJECT_TO_JSVAL(an_obj);
	return JS_TRUE;
}
static JSBool getElementById(JSContext*c, JSObject*obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Node *elt;
	JSObject *an_obj;
	GF_JSField *field;
	char *name = NULL;
	u32 ID = 0;
	GF_ScriptPriv *priv = JS_GetScriptStack(c);
	GF_Node *sc = JS_GetContextPrivate(c);
	if (JSVAL_IS_STRING(argv[0])) name = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	else if (JSVAL_IS_INT(argv[0])) ID = JSVAL_TO_INT(argv[0]);

	if (!ID && !name) return JS_FALSE;

	elt = NULL;
	if (ID) elt = gf_sg_find_node(sc->sgprivate->scenegraph, ID);
	else elt = gf_sg_find_node_by_name(sc->sgprivate->scenegraph, name);
	if (!elt) return JS_TRUE;

	vrml_node_register(elt, NULL);
	field = NewJSField();
	field->field.fieldType = GF_SG_VRML_SFNODE;
	field->temp_node = elt;
	field->field.far_ptr = &field->temp_node;

	an_obj = JS_NewObject(c, &js_rt->SFNodeClass, 0, priv->js_obj);
	field->obj = an_obj;
	JS_SetPrivate(c, an_obj, field);
	*rval = OBJECT_TO_JSVAL(an_obj);
	return JS_TRUE;
}

static JSBool replaceWorld(JSContext*c, JSObject*o, uintN n, jsval *v, jsval *rv)
{
	return JS_TRUE;
}
static JSBool addRoute(JSContext*c, JSObject*o, uintN argc, jsval *argv, jsval *rv)
{
	GF_JSField *ptr;
	GF_Node *n1, *n2;
	char *f1, *f2;
	GF_FieldInfo info;
	GF_Route *r;
	u32 f_id1, f_id2;
	if (argc!=4) return JS_FALSE;
	if (!JSVAL_IS_OBJECT(argv[0]) || !JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[0]), &js_rt->SFNodeClass, NULL) ) return JS_FALSE;
	if (!JSVAL_IS_OBJECT(argv[2]) || !JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[2]), &js_rt->SFNodeClass, NULL) ) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[1]) || !JSVAL_IS_STRING(argv[3])) return JS_FALSE;

	ptr = (GF_JSField *) JS_GetPrivate(c, JSVAL_TO_OBJECT(argv[0]));
	assert(ptr->field.fieldType==GF_SG_VRML_SFNODE);
	n1 = * ((GF_Node **)ptr->field.far_ptr);
	ptr = (GF_JSField *) JS_GetPrivate(c, JSVAL_TO_OBJECT(argv[2]));
	assert(ptr->field.fieldType==GF_SG_VRML_SFNODE);
	n2 = * ((GF_Node **)ptr->field.far_ptr);
		
	if (!n1 || !n2) return JS_FALSE;
	f1 = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));
	f2 = JS_GetStringBytes(JSVAL_TO_STRING(argv[3]));
	if (!f1 || !f2) return JS_FALSE;
	
	if (!strnicmp(f1, "_field", 6)) {
		f_id1 = atoi(f1+6);
		if (gf_node_get_field(n1, f_id1, &info) != GF_OK) return JS_FALSE;
	} else {
		if (gf_node_get_field_by_name(n1, f1, &info) != GF_OK) return JS_FALSE;
		f_id1 = info.fieldIndex;
	}
	if (!strnicmp(f2, "_field", 6)) {
		f_id2 = atoi(f2+6);
		if (gf_node_get_field(n2, f_id2, &info) != GF_OK) return JS_FALSE;
	} else {
		if ((n2->sgprivate->tag==TAG_MPEG4_Script) || (n2->sgprivate->tag==TAG_X3D_Script)) {
			GF_FieldInfo src = info;
			if (gf_node_get_field_by_name(n2, f2, &info) != GF_OK) {
				gf_sg_script_field_new(n2, GF_SG_SCRIPT_TYPE_EVENT_IN, src.fieldType, f2);
			}
		} 
		if (gf_node_get_field_by_name(n2, f2, &info) != GF_OK) return JS_FALSE;
		f_id2 = info.fieldIndex;
	}
	r = gf_sg_route_new(n1->sgprivate->scenegraph, n1, f_id1, n2, f_id2);
	if (!r) return JS_FALSE;
	return JS_TRUE;
}
static JSBool deleteRoute(JSContext*c, JSObject*o, uintN argc, jsval *argv, jsval *rv)
{
	GF_JSField *ptr;
	GF_Node *n1, *n2;
	char *f1, *f2;
	GF_FieldInfo info;
	GF_Route *r;
	u32 f_id1, f_id2, i;
	if (argc!=4) return JS_FALSE;
	if (!JSVAL_IS_OBJECT(argv[0]) || !JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[0]), &js_rt->SFNodeClass, NULL) ) return JS_FALSE;
	if (!JSVAL_IS_OBJECT(argv[2]) || !JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[2]), &js_rt->SFNodeClass, NULL) ) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[1]) || !JSVAL_IS_STRING(argv[3])) return JS_FALSE;

	ptr = (GF_JSField *) JS_GetPrivate(c, JSVAL_TO_OBJECT(argv[0]));
	assert(ptr->field.fieldType==GF_SG_VRML_SFNODE);
	n1 = * ((GF_Node **)ptr->field.far_ptr);
	ptr = (GF_JSField *) JS_GetPrivate(c, JSVAL_TO_OBJECT(argv[2]));
	assert(ptr->field.fieldType==GF_SG_VRML_SFNODE);
	n2 = * ((GF_Node **)ptr->field.far_ptr);
		
	if (!n1 || !n2) return JS_FALSE;
	if (!n1->sgprivate->interact) return JS_TRUE;

	f1 = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));
	f2 = JS_GetStringBytes(JSVAL_TO_STRING(argv[3]));
	if (!f1 || !f2) return JS_FALSE;
	
	if (!strnicmp(f1, "_field", 6)) {
		f_id1 = atoi(f1+6);
		if (gf_node_get_field(n1, f_id1, &info) != GF_OK) return JS_FALSE;
	} else {
		if (gf_node_get_field_by_name(n1, f1, &info) != GF_OK) return JS_FALSE;
		f_id1 = info.fieldIndex;
	}
	if (!strnicmp(f2, "_field", 6)) {
		f_id2 = atoi(f2+6);
		if (gf_node_get_field(n2, f_id2, &info) != GF_OK) return JS_FALSE;
	} else {
		if (gf_node_get_field_by_name(n2, f2, &info) != GF_OK) return JS_FALSE;
		f_id2 = info.fieldIndex;
	}

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

static JSBool loadURL(JSContext*c, JSObject*obj, uintN argc, jsval *argv, jsval *rval)
{
	u32 i;
	GF_JSAPIParam par;
	jsval item;
	jsuint len;
	JSObject *p;
	GF_JSField *f;
	M_Script *script = (M_Script *) JS_GetContextPrivate(c);

	if (argc < 1) return JS_FALSE;

	if (JSVAL_IS_STRING(argv[0])) {
		JSString *str = JSVAL_TO_STRING(argv[0]);
		par.uri.url = JS_GetStringBytes(str);
		/*TODO add support for params*/
		par.uri.nb_params = 0;
		if (ScriptAction(c, NULL, GF_JSAPI_OP_LOAD_URL, (GF_Node *)script, &par))
			return JS_TRUE;
		return JS_FALSE;
	}
	if (!JSVAL_IS_OBJECT(argv[0])) return JS_FALSE;
	
	JS_ValueToObject(c, argv[0], &p);

	f = (GF_JSField *) JS_GetPrivate(c, p);
	if (!f || !f->js_list) return JS_FALSE;
	JS_GetArrayLength(c, f->js_list, &len);

	for (i=0; i<len; i++) {
		JS_GetElement(c, f->js_list, (jsint) i, &item);

		if (JSVAL_IS_STRING(item)) {
			JSString *str = JSVAL_TO_STRING(item);
			par.uri.url = JS_GetStringBytes(str);
			/*TODO add support for params*/
			par.uri.nb_params = 0;
			if (ScriptAction(c, NULL, GF_JSAPI_OP_LOAD_URL, (GF_Node*)script, &par) )
				return JS_TRUE;
		}
	}
	return JS_TRUE;
}

static JSBool setDescription(JSContext*c, JSObject*o, uintN argc, jsval *argv, jsval *rv)
{
	GF_JSAPIParam par;
	GF_Node *node = JS_GetContextPrivate(c);
	if (!argc || !JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	par.uri.url = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	ScriptAction(c, NULL, GF_JSAPI_OP_SET_TITLE, node->sgprivate->scenegraph->RootNode, &par);
	return JS_TRUE;
}

static JSBool createVrmlFromString(JSContext*c, JSObject*obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_ScriptPriv *priv;
	GF_FieldInfo field;
	/*BT/VRML from string*/
	GF_List *gf_sm_load_bt_from_string(GF_SceneGraph *in_scene, char *node_str);
	JSString *js_str;
	char *str;
	GF_List *nlist;
	GF_Node *sc_node = JS_GetContextPrivate(c);
	if (argc < 1) return JS_FALSE;

	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	js_str = JSVAL_TO_STRING(argv[0]);
	str = JS_GetStringBytes(js_str);
	nlist = gf_sm_load_bt_from_string(sc_node->sgprivate->scenegraph, str);
	if (!nlist) return JSVAL_NULL;

	priv = JS_GetScriptStack(c);
	memset(&field, 0, sizeof(GF_FieldInfo));
	field.fieldType = GF_SG_VRML_MFNODE;
	field.far_ptr = &nlist;
	*rval = gf_sg_script_to_smjs_field(priv, &field, NULL, 0, 0);

	/*don't forget to unregister all this stuff*/
	while (gf_list_count(nlist)) {
		GF_Node *n = gf_list_get(nlist, 0);
		gf_list_rem(nlist, 0);
		gf_node_unregister(n, NULL);
	}
	gf_list_del(nlist);
	return JS_TRUE;
}

static JSBool getOption(JSContext*c, JSObject*obj, uintN argc, jsval *argv, jsval *rval)
{
	JSString *s;
	GF_JSAPIParam par;
	GF_Node *sc_node = JS_GetContextPrivate(c);
	JSString *js_sec_name, *js_key_name;
	if (argc < 2) return JSVAL_FALSE;
	
	if (!JSVAL_IS_STRING(argv[0])) return JSVAL_FALSE;
	js_sec_name = JSVAL_TO_STRING(argv[0]);
	par.gpac_cfg.section = JS_GetStringBytes(js_sec_name);

	if (!JSVAL_IS_STRING(argv[1])) return JSVAL_FALSE;
	js_key_name = JSVAL_TO_STRING(argv[1]);
	par.gpac_cfg.key = JS_GetStringBytes(js_key_name);
	par.gpac_cfg.key_val = NULL;

	if (!ScriptAction(c, NULL, GF_JSAPI_OP_GET_OPT, sc_node->sgprivate->scenegraph->RootNode, &par))
		return JSVAL_FALSE;

	s = JS_NewStringCopyZ(c, par.gpac_cfg.key_val ? (const char *)par.gpac_cfg.key_val : "");
	if (!s) return JS_FALSE;
	*rval = STRING_TO_JSVAL(s); 
	return JS_TRUE; 
}

static JSBool setOption(JSContext*c, JSObject*obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_JSAPIParam par;
	GF_Node *sc_node = JS_GetContextPrivate(c);
	JSString *js_sec_name, *js_key_name, *js_key_val;
	if (argc < 3) return JSVAL_FALSE;
	
	if (!JSVAL_IS_STRING(argv[0])) return JSVAL_FALSE;
	js_sec_name = JSVAL_TO_STRING(argv[0]);
	par.gpac_cfg.section = JS_GetStringBytes(js_sec_name);

	if (!JSVAL_IS_STRING(argv[1])) return JSVAL_FALSE;
	js_key_name = JSVAL_TO_STRING(argv[1]);
	par.gpac_cfg.key = JS_GetStringBytes(js_key_name);

	if (!JSVAL_IS_STRING(argv[2])) return JSVAL_FALSE;
	js_key_val = JSVAL_TO_STRING(argv[2]);
	par.gpac_cfg.key_val = JS_GetStringBytes(js_key_val);

	if (!ScriptAction(c, NULL, GF_JSAPI_OP_SET_OPT, sc_node->sgprivate->scenegraph->RootNode, &par))
		return JSVAL_FALSE;

	return JSVAL_TRUE;
}

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
	if ((parent->sgprivate->tag == TAG_MPEG4_Script) || (parent->sgprivate->tag == TAG_X3D_Script) ) {
		script_field = 1;
		if ( (GF_Node *) JS_GetContextPrivate(c) == parent) script_field = 2;
	}

	if (script_field!=2) {
		if (field->on_event_in) field->on_event_in(parent);
		else if (script_field && (field->eventType==GF_SG_EVENT_IN) ) {
			gf_sg_script_event_in(parent, field);
			gf_node_changed_internal(parent, field, 0);
			return;
		}
		/*field has changed, set routes...*/
		if (parent->sgprivate->tag == TAG_ProtoNode)gf_sg_proto_check_field_change(parent, field->fieldIndex);
		else {
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

JSBool gf_sg_script_eventout_set_prop(JSContext *c, JSObject *obj, jsval id, jsval *val)
{
	u32 i;
	const char *eventName;
	GF_ScriptPriv *script;
	GF_Node *n;
	GF_ScriptField *sf;
	GF_FieldInfo info;
	JSString *str = JS_ValueToString(c, id);
	if (!str) return JS_FALSE;
	
	eventName = JS_GetStringBytes(str);

	script = JS_GetScriptStack(c);
	if (!script) return JS_FALSE;
	n = (GF_Node *) JS_GetScript(c);

	i=0;
	while ((sf = gf_list_enum(script->fields, &i))) {
		if (!stricmp(sf->name, eventName)) {
			gf_node_get_field(n, sf->ALL_index, &info);
			gf_sg_script_to_node_field(c, *val, &info, n, NULL);
			sf->activate_event_out = 1;
			return JS_TRUE;
		}
	}
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
		sprintf(temp, "%f %f %f", val.red, val.green, val.blue);
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

static void JS_ObjectDestroyed(JSContext *c, JSObject *obj)
{
	GF_ScriptPriv *priv = JS_GetScriptStack(c);

	JS_SetPrivate(c, obj, 0);
	if (priv->js_cache) 
		gf_list_del_item(priv->js_cache, obj);
}


static JSBool field_toString(JSContext *c, JSObject *obj, uintN n, jsval *v, jsval *rval)
{
	u32 i, len;
	jsdouble d;
	char temp[1000];
	char str[5000];
	JSString *s;
	jsval item;
	GF_JSField *f = (GF_JSField *) JS_GetPrivate(c, obj);
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
				JS_ValueToNumber(c, item, &d);
				sprintf(temp, "%g", d);
				strcat(str, temp);
				break;
			case GF_SG_VRML_MFSTRING:
			case GF_SG_VRML_MFURL:
			{
				JSString *j_str = JSVAL_TO_STRING(item);
				char *str_val = JS_GetStringBytes(j_str);
				strcat(str, str_val);
			}
				break;
			default:
				if (JSVAL_IS_OBJECT(item)) {
					GF_JSField *sf = (GF_JSField *) JS_GetPrivate(c, JSVAL_TO_OBJECT(item));
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
	*rval = STRING_TO_JSVAL(s); 
	return JS_TRUE; 
}

static JSBool SFNodeConstructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	u32 tag, ID;
	GF_Node *new_node;
	GF_JSField *field;
	JSString *str;
	GF_Proto *proto;
	GF_SceneGraph *sg;
	char *node_name;
	GF_ScriptPriv *priv = JS_GetScriptStack(c);
	M_Script *sc = JS_GetScript(c);
	if (!argc) {
		field = NewJSField();
		field->field.fieldType = GF_SG_VRML_SFNODE;
		field->temp_node = NULL;
		field->field.far_ptr = &field->temp_node;
		JS_SetPrivate(c, obj, field);
		*rval = OBJECT_TO_JSVAL(obj);
		return JS_TRUE;
	}
	if (!JS_InstanceOf(c, obj, &js_rt->SFNodeClass, NULL) ) return JS_FALSE;


	str = JS_ValueToString(c, argv[0]);
	if (!str) return JS_FALSE;

	ID = 0;
	node_name = JS_GetStringBytes(str);
	if (!strnicmp(node_name, "_proto", 6)) {
		ID = atoi(node_name+6);
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
		if (!proto) return JS_FALSE;
		/* create interface and load code in current graph*/
		new_node = gf_sg_proto_create_instance(sc->sgprivate->scenegraph, proto);
		if (!new_node) return JS_FALSE;
		/*OK, instanciate proto code*/
		if (gf_sg_proto_load_code(new_node) != GF_OK) {
			gf_node_unregister(new_node, NULL);
			return JS_FALSE;
		}
	} else {
		if (sc->sgprivate->tag==TAG_MPEG4_Script) {
			tag = gf_node_mpeg4_type_by_class_name(node_name);
		} else {
			tag = gf_node_x3d_type_by_class_name(node_name);
		}
		if (!tag) goto locate_proto;
		new_node = gf_node_new(sc->sgprivate->scenegraph, tag);
		if (!new_node) return JS_FALSE;
		gf_node_init(new_node);
	}

	vrml_node_register(new_node, NULL);
	field = NewJSField();
	field->field.fieldType = GF_SG_VRML_SFNODE;
	field->temp_node = new_node;
	field->field.far_ptr = &field->temp_node;

	field->obj = obj;
	JS_AddRoot(c, &field->obj);
	if (priv->js_cache) gf_list_add(priv->js_cache, obj);

	JS_SetPrivate(c, obj, field);
	*rval = OBJECT_TO_JSVAL(obj);

	return JS_TRUE;
}
static void node_finalize_ex(JSContext *c, JSObject *obj, Bool is_js_call)
{
	GF_JSField *ptr = (GF_JSField *) JS_GetPrivate(c, obj);
	
	if (is_js_call) 
		JS_ObjectDestroyed(c, obj);
	
	if (ptr) {
		/*num_instances may be 0 if the node is the script being destroyed*/
		if (ptr->temp_node && ptr->temp_node->sgprivate->num_instances) {

			GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[VRML JS] unregistering node %s (%s)\n", gf_node_get_name(ptr->temp_node), gf_node_get_class_name(ptr->temp_node)));
			gf_node_unregister(ptr->temp_node, ptr->owner);
		}
		free(ptr);
	}
}
static void node_finalize(JSContext *c, JSObject *obj)
{
	node_finalize_ex(c, obj, 1);
}

static JSBool node_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	GF_Node *n;
	u32 index;
	JSString *str;
	GF_FieldInfo info;
	GF_JSField *ptr;
	GF_ScriptPriv *priv;

	if (! JS_InstanceOf(c, obj, &js_rt->SFNodeClass, NULL) ) return JS_FALSE;
	ptr = (GF_JSField *) JS_GetPrivate(c, obj);
	assert(ptr->field.fieldType==GF_SG_VRML_SFNODE);
	n = * ((GF_Node **)ptr->field.far_ptr);
	priv = JS_GetScriptStack(c);

	if (n && JSVAL_IS_STRING(id) && ( (str = JSVAL_TO_STRING(id)) != 0) ) {
		char *fieldName = JS_GetStringBytes(str);
		if (!strnicmp(fieldName, "toString", 8)) {
			return JS_TRUE;
		}
		/*fieldID indexing*/
		if (!strnicmp(fieldName, "_field", 6)) {
			index = atoi(fieldName+6);
			if ( gf_node_get_field(n, index, &info) == GF_OK) {
				*vp = gf_sg_script_to_smjs_field(priv, &info, n, 0, 0);
				return JS_TRUE;
			}
		} else if ( gf_node_get_field_by_name(n, fieldName, &info) == GF_OK) {
			*vp = gf_sg_script_to_smjs_field(priv, &info, n, 0, 0);
			return JS_TRUE;
		}
		return JS_TRUE;
    }
	return JS_FALSE;
}

static JSBool node_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	GF_Node *n;
	GF_FieldInfo info;
	u32 index;
	char *fieldname;
	GF_JSField *ptr;
	if (! JS_InstanceOf(c, obj, &js_rt->SFNodeClass, NULL) ) return JS_FALSE;
	ptr = (GF_JSField *) JS_GetPrivate(c, obj);

	/*this is the prototype*/
	if (!ptr) {
		if (! JSVAL_IS_STRING(id)) return JS_FALSE;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[VRML JS] setting new prop/func %s for object prototype\n", JS_GetStringBytes(JSVAL_TO_STRING(id)) ));
		return JS_TRUE;
	}

	assert(ptr->field.fieldType==GF_SG_VRML_SFNODE);
	n = * ((GF_Node **)ptr->field.far_ptr);

	if (n && JSVAL_IS_STRING(id)) {
		JSString *str = JSVAL_TO_STRING(id);
		fieldname = JS_GetStringBytes(str);
		
		/*fieldID indexing*/
		if (!strnicmp(fieldname, "_field", 6)) {
			index = atoi(fieldname+6);
			if ( gf_node_get_field(n, index, &info) != GF_OK) return JS_TRUE;
		} else {
			if (gf_node_get_field_by_name(n, fieldname, &info) != GF_OK) {
				/*VRML style*/
				if (!strnicmp(fieldname, "set_", 4)) {
					fieldname+=4;
					if (gf_node_get_field_by_name(n, fieldname, &info) != GF_OK) return JS_TRUE;
				} else 
					return JS_TRUE;
			}
		}
		if (gf_node_get_tag(n)==TAG_ProtoNode)
			gf_sg_proto_mark_field_loaded(n, &info);

		gf_sg_script_to_node_field(c, *vp, &info, n, ptr);
	}
	return JS_TRUE;
}
static JSBool node_toString(JSContext *c, JSObject *obj, uintN i, jsval *v, jsval *rval)
{
	char str[1000];
	u32 id;
	GF_Node *n;
	JSString *s;
	GF_JSField *f;
	const char *name;
	if (! JS_InstanceOf(c, obj, &js_rt->SFNodeClass, NULL) ) return JS_FALSE;
	f = (GF_JSField *) JS_GetPrivate(c, obj);
	if (!f) return JS_FALSE;

	str[0] = 0;
	n = * ((GF_Node **)f->field.far_ptr);

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
	*rval = STRING_TO_JSVAL(s); 
	return JS_TRUE; 
}

/* Generic field destructor */
static void field_finalize(JSContext *c, JSObject *obj)
{
	GF_JSField *ptr = (GF_JSField *) JS_GetPrivate(c, obj);
	JS_ObjectDestroyed(c, obj);
	if (!ptr) return;

	if (ptr->field_ptr) gf_sg_vrml_field_pointer_del(ptr->field_ptr, ptr->field.fieldType);
	free(ptr);
}





/*SFImage class functions */
static GFINLINE GF_JSField *SFImage_Create(JSContext *c, JSObject *obj, u32 w, u32 h, u32 nbComp, MFInt32 *pixels)
{
	u32 i, len;
	GF_JSField *field;
	SFImage *v;
	field = NewJSField();
	v = gf_sg_vrml_field_pointer_new(GF_SG_VRML_SFIMAGE);
	field->field_ptr = field->field.far_ptr = v;
	field->field.fieldType = GF_SG_VRML_SFIMAGE;
	v->width = w;
	v->height = h;
	v->numComponents = nbComp;
	v->pixels = (u8 *) malloc(sizeof(u8) * nbComp * w * h);
	len = MIN(nbComp * w * h, pixels->count);
	for (i=0; i<len; i++) v->pixels[i] = (u8) pixels->vals[i];
	JS_SetPrivate(c, obj, field);
	return field;
}
static JSBool SFImageConstructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rv)
{
	u32 w, h, nbComp;
	MFInt32 *pixels;
	if (argc<4) return 0;
	if (!JSVAL_IS_INT(argv[0]) || !JSVAL_IS_INT(argv[1]) || !JSVAL_IS_INT(argv[2])) return JS_FALSE;
	if (!JSVAL_IS_OBJECT(argv[3]) || !JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[3]), &js_rt->MFInt32Class, NULL)) return JS_FALSE;
	w = JSVAL_TO_INT(argv[0]);
	h = JSVAL_TO_INT(argv[1]);
	nbComp = JSVAL_TO_INT(argv[2]);
	pixels = (MFInt32 *) ((GF_JSField *) JS_GetPrivate(c, JSVAL_TO_OBJECT(argv[3])))->field.far_ptr;
	SFImage_Create(c, obj, w, h, nbComp, pixels);
	return JS_TRUE;
}
static JSBool image_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	GF_ScriptPriv *priv = JS_GetScriptStack(c);
	GF_JSField *val = (GF_JSField *) JS_GetPrivate(c, obj);
	SFImage *sfi;
	if (!val) return JS_FALSE;
	sfi = (SFImage*)val->field.far_ptr;
	if (JSVAL_IS_INT(id)) {
		switch (JSVAL_TO_INT(id)) {
		case 0: *vp = INT_TO_JSVAL( sfi->width ); break;
		case 1: *vp = INT_TO_JSVAL( sfi->height); break;
		case 2: *vp = INT_TO_JSVAL( sfi->numComponents ); break;
		case 3: 
		{
			u32 i, len;
			JSObject *an_obj = JS_ConstructObject(priv->js_ctx, &js_rt->MFInt32Class, 0, priv->js_obj);
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

static JSBool image_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	u32 ival;
	Bool changed = 0;
	GF_JSField *ptr = (GF_JSField *) JS_GetPrivate(c, obj);
	SFImage *sfi;
	
	/*this is the prototype*/
	if (!ptr) {
		if (! JSVAL_IS_STRING(id)) return JS_FALSE;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[VRML JS] setting new prop/func %s for object prototype\n", JS_GetStringBytes(JSVAL_TO_STRING(id)) ));
		return JS_TRUE;
	}
	sfi = (SFImage*)ptr->field.far_ptr;

	if (JSVAL_IS_INT(id) && JSVAL_TO_INT(id) >= 0 && JSVAL_TO_INT(id) < 4) {
		switch (JSVAL_TO_INT(id)) {
		case 0: 
			ival = JSVAL_TO_INT(vp);
			changed = ! (sfi->width == ival);
			sfi->width = ival;
			if (changed && sfi->pixels) { free(sfi->pixels); sfi->pixels = NULL; }
			break;
		case 1: 
			ival = JSVAL_TO_INT(vp);
			changed = ! (sfi->height == ival);
			sfi->height = ival;
			if (changed && sfi->pixels) { free(sfi->pixels); sfi->pixels = NULL; }
			break;
		case 2: 
			ival = JSVAL_TO_INT(vp);
			changed = ! (sfi->numComponents == ival);
			sfi->numComponents = ival;
			if (changed && sfi->pixels) { free(sfi->pixels); sfi->pixels = NULL; }
			break;
		case 3:
		{
			MFInt32 *pixels;
			u32 len, i;
			if (!JSVAL_IS_OBJECT(*vp) || !JS_InstanceOf(c, JSVAL_TO_OBJECT(*vp), &js_rt->MFInt32Class, NULL)) return JS_FALSE;
			pixels = (MFInt32 *) ((GF_JSField *) JS_GetPrivate(c, JSVAL_TO_OBJECT(*vp)))->field.far_ptr;
			if (sfi->pixels) free(sfi->pixels);
			len = sfi->width*sfi->height*sfi->numComponents;
			sfi->pixels = (char *) malloc(sizeof(char)*len);
			len = MAX(len, pixels->count);
			for (i=0; i<len; i++) sfi->pixels[i] = (u8) pixels->vals[i];
			changed = 1;
			break;
		}
		default: return JS_FALSE;
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
	field = NewJSField();
	v = gf_sg_vrml_field_pointer_new(GF_SG_VRML_SFVEC2F);
	field->field_ptr = field->field.far_ptr = v;
	field->field.fieldType = GF_SG_VRML_SFVEC2F;
	v->x = x;
	v->y = y;
	JS_SetPrivate(c, obj, field);
	return field;
}
static JSBool SFVec2fConstructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rv)
{
	jsdouble x = 0.0, y = 0.0;
	if (argc > 0) JS_ValueToNumber(c, argv[0], &x);
	if (argc > 1) JS_ValueToNumber(c, argv[1], &y);
	SFVec2f_Create(c, obj, FLT2FIX( x), FLT2FIX( y));
	return JS_TRUE;
}
static JSBool vec2f_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	GF_JSField *val = (GF_JSField *) JS_GetPrivate(c, obj);
	if (JSVAL_IS_INT(id)) {
		switch (JSVAL_TO_INT(id)) {
		case 0: *vp = DOUBLE_TO_JSVAL(JS_NewDouble(c, FIX2FLT( ((SFVec2f*)val->field.far_ptr)->x) )); break;
		case 1: *vp = DOUBLE_TO_JSVAL(JS_NewDouble(c, FIX2FLT( ((SFVec2f*)val->field.far_ptr)->y) )); break;
		default: return JS_TRUE;
		}
	}
	return JS_TRUE;
}

static JSBool vec2f_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	jsdouble d;
	Fixed v;
	Bool changed = 0;
	GF_JSField *ptr = (GF_JSField *) JS_GetPrivate(c, obj);

	/*this is the prototype*/
	if (!ptr) {
		if (! JSVAL_IS_STRING(id)) return JS_FALSE;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[VRML JS] setting new prop/func %s for object prototype\n", JS_GetStringBytes(JSVAL_TO_STRING(id)) ));
		return JS_TRUE;
	}

	if (JSVAL_IS_INT(id) && JSVAL_TO_INT(id) >= 0 && JSVAL_TO_INT(id) < 2 && JS_ValueToNumber(c, *vp, &d)) {
		switch (JSVAL_TO_INT(id)) {
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

static JSBool vec2f_add(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	SFVec2f *v1, *v2;
	JSObject *pNew;
	if (argc<=0 || !JSVAL_IS_OBJECT(argv[0]) || !JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[0]), &js_rt->SFVec2fClass, NULL))
		return JS_FALSE;

	v1 = ((GF_JSField *) JS_GetPrivate(c, obj))->field.far_ptr;
    v2 = ((GF_JSField *) JS_GetPrivate(c, JSVAL_TO_OBJECT(argv[0])))->field.far_ptr;
	pNew = JS_NewObject(c, &js_rt->SFVec2fClass, 0, JS_GetParent(c, obj));  
	SFVec2f_Create(c, pNew, v1->x + v2->x, v1->y + v2->y);
	*rval = OBJECT_TO_JSVAL(pNew);
	return JS_TRUE;
}
static JSBool vec2f_subtract(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	SFVec2f *v1, *v2;
	JSObject *pNew;
	if (argc<=0 || !JSVAL_IS_OBJECT(argv[0]) || !JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[0]), &js_rt->SFVec2fClass, NULL))
		return JS_FALSE;

	v1 = ((GF_JSField *) JS_GetPrivate(c, obj))->field.far_ptr;
    v2 = ((GF_JSField *) JS_GetPrivate(c, JSVAL_TO_OBJECT(argv[0])))->field.far_ptr;
	pNew = JS_NewObject(c, &js_rt->SFVec2fClass, 0, JS_GetParent(c, obj));  
	SFVec2f_Create(c, pNew, v1->x - v2->x, v1->y - v2->y);
	*rval = OBJECT_TO_JSVAL(pNew);
	return JS_TRUE;
}
static JSBool vec2f_negate(JSContext *c, JSObject *obj, uintN n, jsval *v, jsval *rval)
{
	SFVec2f *v1;
	JSObject *pNew;
	v1 = ((GF_JSField *) JS_GetPrivate(c, obj))->field.far_ptr;
	pNew = JS_NewObject(c, &js_rt->SFVec2fClass, 0, JS_GetParent(c, obj));  
	SFVec2f_Create(c, pNew, -v1->x , -v1->y );
	*rval = OBJECT_TO_JSVAL(pNew);
	return JS_TRUE;
}
static JSBool vec2f_multiply(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	SFVec2f *v1;
	JSObject *pNew;
	jsdouble d;
	Fixed v;
	if (argc<=0) return JS_FALSE;
	v1 = ((GF_JSField *) JS_GetPrivate(c, obj))->field.far_ptr;
	pNew = JS_NewObject(c, &js_rt->SFVec2fClass, 0, JS_GetParent(c, obj));  
	JS_ValueToNumber(c, argv[0], &d );
	v = FLT2FIX( d);
	SFVec2f_Create(c, pNew, gf_mulfix(v1->x , v), gf_mulfix(v1->y, v) );
	*rval = OBJECT_TO_JSVAL(pNew);
	return JS_TRUE;
}
static JSBool vec2f_divide(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	SFVec2f *v1;
	JSObject *pNew;
	jsdouble d;
	Fixed v;
	if (argc<=0) return JS_FALSE;
	v1 = ((GF_JSField *) JS_GetPrivate(c, obj))->field.far_ptr;
	pNew = JS_NewObject(c, &js_rt->SFVec2fClass, 0, JS_GetParent(c, obj));  
	JS_ValueToNumber(c, argv[0], &d );
	v = FLT2FIX(d);
	SFVec2f_Create(c, pNew, gf_divfix(v1->x, v),  gf_divfix(v1->y, v));
	*rval = OBJECT_TO_JSVAL(pNew);
	return JS_TRUE;
}
static JSBool vec2f_length(JSContext *c, JSObject *obj, uintN n, jsval *val, jsval *rval)
{
	Double res;
	SFVec2f *v1;
	v1 = ((GF_JSField *) JS_GetPrivate(c, obj))->field.far_ptr;
	res = FIX2FLT(gf_v2d_len(v1));
	*rval = DOUBLE_TO_JSVAL(JS_NewDouble(c, res) );
	return JS_TRUE;
}
static JSBool vec2f_normalize(JSContext *c, JSObject *obj, uintN n, jsval *val, jsval *rval)
{
	SFVec2f *v1;
	Fixed res;
	JSObject *pNew;
	v1 = ((GF_JSField *) JS_GetPrivate(c, obj))->field.far_ptr;
	res = gf_v2d_len(v1);
	pNew = JS_NewObject(c, &js_rt->SFVec2fClass, 0, JS_GetParent(c, obj));  
	SFVec2f_Create(c, pNew, gf_divfix(v1->x, res), gf_divfix(v1->y, res) );
	*rval = OBJECT_TO_JSVAL(pNew);
	return JS_TRUE;
}
static JSBool vec2f_dot(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	SFVec2f *v1, *v2;
	if (argc<=0 || !JSVAL_IS_OBJECT(argv[0]) || !JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[0]), &js_rt->SFVec2fClass, NULL))
		return JS_FALSE;

	v1 = ((GF_JSField *) JS_GetPrivate(c, obj))->field.far_ptr;
    v2 = ((GF_JSField *) JS_GetPrivate(c, JSVAL_TO_OBJECT(argv[0])))->field.far_ptr;
	*rval = DOUBLE_TO_JSVAL(JS_NewDouble(c, FIX2FLT( gf_mulfix(v1->x, v2->x) + gf_mulfix(v1->y, v2->y) ) ) );
	return JS_TRUE;
}


/*SFVec3f class functions */
static GFINLINE GF_JSField *SFVec3f_Create(JSContext *c, JSObject *obj, Fixed x, Fixed y, Fixed z)
{
	GF_JSField *field;
	SFVec3f *v;
	field = NewJSField();
	v = gf_sg_vrml_field_pointer_new(GF_SG_VRML_SFVEC3F);
	field->field_ptr = field->field.far_ptr = v;
	field->field.fieldType = GF_SG_VRML_SFVEC3F;
	v->x = x;
	v->y = y;
	v->z = z;
	JS_SetPrivate(c, obj, field);
	return field;
}
static JSBool SFVec3fConstructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rv)
{
	jsdouble x = 0.0, y = 0.0, z = 0.0;
	if (argc > 0) JS_ValueToNumber(c, argv[0], &x);
	if (argc > 1) JS_ValueToNumber(c, argv[1], &y);
	if (argc > 2) JS_ValueToNumber(c, argv[2], &z);
	SFVec3f_Create(c, obj, FLT2FIX( x), FLT2FIX( y), FLT2FIX( z));
	return JS_TRUE;
}
static JSBool vec3f_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	GF_JSField *val = (GF_JSField *) JS_GetPrivate(c, obj);
	if (JSVAL_IS_INT(id)) {
		switch (JSVAL_TO_INT(id)) {
		case 0: *vp = DOUBLE_TO_JSVAL(JS_NewDouble(c, FIX2FLT( ((SFVec3f*)val->field.far_ptr)->x) )); break;
		case 1: *vp = DOUBLE_TO_JSVAL(JS_NewDouble(c, FIX2FLT( ((SFVec3f*)val->field.far_ptr)->y) )); break;
		case 2: *vp = DOUBLE_TO_JSVAL(JS_NewDouble(c, FIX2FLT( ((SFVec3f*)val->field.far_ptr)->z) )); break;
		default: return JS_TRUE;
		}
	}
	return JS_TRUE;
}
static JSBool vec3f_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	jsdouble d;
	Fixed v;
	Bool changed = 0;
	GF_JSField *ptr = (GF_JSField *) JS_GetPrivate(c, obj);

	/*this is the prototype*/
	if (!ptr) {
		if (! JSVAL_IS_STRING(id)) return JS_FALSE;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[VRML JS] setting new prop/func %s for object prototype\n", JS_GetStringBytes(JSVAL_TO_STRING(id)) ));
		return JS_TRUE;
	}

	if (JSVAL_IS_INT(id) && JSVAL_TO_INT(id) >= 0 && JSVAL_TO_INT(id) < 3 && JS_ValueToNumber(c, *vp, &d)) {
		switch (JSVAL_TO_INT(id)) {
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
		default: return JS_TRUE;
		}
		if (changed) Script_FieldChanged(c, NULL, ptr, NULL);
		return JS_TRUE;
    }
	return JS_FALSE;
}
static JSBool vec3f_add(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	SFVec3f *v1, *v2;
	JSObject *pNew;
	if (argc<=0 || !JSVAL_IS_OBJECT(argv[0]) || !JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[0]), &js_rt->SFVec3fClass, NULL))
		return JS_FALSE;

	v1 = ((GF_JSField *) JS_GetPrivate(c, obj))->field.far_ptr;
    v2 = ((GF_JSField *) JS_GetPrivate(c, JSVAL_TO_OBJECT(argv[0])))->field.far_ptr;
	pNew = JS_NewObject(c, &js_rt->SFVec3fClass, 0, JS_GetParent(c, obj));  
	SFVec3f_Create(c, pNew, v1->x + v2->x, v1->y + v2->y, v1->z + v2->z);
	*rval = OBJECT_TO_JSVAL(pNew);
	return JS_TRUE;
}
static JSBool vec3f_subtract(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	SFVec3f *v1, *v2;
	JSObject *pNew;
	if (argc<=0 || !JSVAL_IS_OBJECT(argv[0]) || !JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[0]), &js_rt->SFVec3fClass, NULL))
		return JS_FALSE;

	v1 = ((GF_JSField *) JS_GetPrivate(c, obj))->field.far_ptr;
    v2 = ((GF_JSField *) JS_GetPrivate(c, JSVAL_TO_OBJECT(argv[0])))->field.far_ptr;
	pNew = JS_NewObject(c, &js_rt->SFVec3fClass, 0, JS_GetParent(c, obj));  
	SFVec3f_Create(c, pNew, v1->x - v2->x, v1->y - v2->y, v1->z - v2->z);
	*rval = OBJECT_TO_JSVAL(pNew);
	return JS_TRUE;
}
static JSBool vec3f_negate(JSContext *c, JSObject *obj, uintN n, jsval *v, jsval *rval)
{
	SFVec3f *v1;
	JSObject *pNew;
	v1 = ((GF_JSField *) JS_GetPrivate(c, obj))->field.far_ptr;
	pNew = JS_NewObject(c, &js_rt->SFVec3fClass, 0, JS_GetParent(c, obj));  
	SFVec3f_Create(c, pNew, -v1->x , -v1->y , -v1->z );
	*rval = OBJECT_TO_JSVAL(pNew);
	return JS_TRUE;
}
static JSBool vec3f_multiply(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	SFVec3f *v1;
	JSObject *pNew;
	jsdouble d;
	Fixed v;
	if (argc<=0) return JS_FALSE;

	v1 = ((GF_JSField *) JS_GetPrivate(c, obj))->field.far_ptr;
	pNew = JS_NewObject(c, &js_rt->SFVec3fClass, 0, JS_GetParent(c, obj));  
	JS_ValueToNumber(c, argv[0], &d );
	v = FLT2FIX(d);
	SFVec3f_Create(c, pNew, gf_mulfix(v1->x, v), gf_mulfix(v1->y, v), gf_mulfix(v1->z, v) );
	*rval = OBJECT_TO_JSVAL(pNew);
	return JS_TRUE;
}
static JSBool vec3f_divide(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	SFVec3f *v1;
	JSObject *pNew;
	jsdouble d;
	Fixed v;
	if (argc<=0) return JS_FALSE;
	v1 = ((GF_JSField *) JS_GetPrivate(c, obj))->field.far_ptr;
	pNew = JS_NewObject(c, &js_rt->SFVec3fClass, 0, JS_GetParent(c, obj));  
	JS_ValueToNumber(c, argv[0], &d );
	v = FLT2FIX(d);
	SFVec3f_Create(c, pNew, gf_divfix(v1->x, v), gf_divfix(v1->y, v), gf_divfix(v1->z, v));
	*rval = OBJECT_TO_JSVAL(pNew);
	return JS_TRUE;
}
static JSBool vec3f_length(JSContext *c, JSObject *obj, uintN n, jsval *val, jsval *rval)
{
	Fixed res;
	SFVec3f *v1;
	v1 = ((GF_JSField *) JS_GetPrivate(c, obj))->field.far_ptr;
	res = gf_vec_len(*v1);
	*rval = DOUBLE_TO_JSVAL(JS_NewDouble(c, FIX2FLT(res)) );
	return JS_TRUE;
}
static JSBool vec3f_normalize(JSContext *c, JSObject *obj, uintN n, jsval *val, jsval *rval)
{
	SFVec3f v1;
	JSObject *pNew;
	v1 = * (SFVec3f *) ((GF_JSField *) JS_GetPrivate(c, obj))->field.far_ptr;
	gf_vec_norm(&v1);
	pNew = JS_NewObject(c, &js_rt->SFVec3fClass, 0, JS_GetParent(c, obj));  
	SFVec3f_Create(c, pNew, v1.x, v1.y, v1.z);
	*rval = OBJECT_TO_JSVAL(pNew);
	return JS_TRUE;
}
static JSBool vec3f_dot(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	SFVec3f v1, v2;
	if (argc<=0 || !JSVAL_IS_OBJECT(argv[0]) || !JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[0]), &js_rt->SFVec3fClass, NULL))
		return JS_FALSE;

	v1 = *(SFVec3f *) ((GF_JSField *) JS_GetPrivate(c, obj))->field.far_ptr;
    v2 = *(SFVec3f *) ((GF_JSField *) JS_GetPrivate(c, JSVAL_TO_OBJECT(argv[0])))->field.far_ptr;
	*rval = DOUBLE_TO_JSVAL(JS_NewDouble(c, FIX2FLT(gf_vec_dot(v1, v2))) );
	return JS_TRUE;
}
static JSBool vec3f_cross(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	SFVec3f v1, v2, v3;
	JSObject *pNew;

	if (argc<=0 || !JSVAL_IS_OBJECT(argv[0]) || !JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[0]), &js_rt->SFVec3fClass, NULL))
		return JS_FALSE;

	v1 = * (SFVec3f *) ((GF_JSField *) JS_GetPrivate(c, obj))->field.far_ptr;
    v2 = * (SFVec3f *) ((GF_JSField *) JS_GetPrivate(c, JSVAL_TO_OBJECT(argv[0])))->field.far_ptr;
	pNew = JS_NewObject(c, &js_rt->SFVec3fClass, 0, JS_GetParent(c, obj));  
	v3 = gf_vec_cross(v1, v2);
	SFVec3f_Create(c, pNew, v3.x, v3.y, v3.z);
	*rval = OBJECT_TO_JSVAL(pNew);
	return JS_TRUE;
}



/*SFRotation class*/
static GFINLINE GF_JSField *SFRotation_Create(JSContext *c, JSObject *obj, Fixed x, Fixed y, Fixed z, Fixed q)
{
	GF_JSField *field;
	SFRotation *v;
	field = NewJSField();
	v = gf_sg_vrml_field_pointer_new(GF_SG_VRML_SFROTATION);
	field->field_ptr = field->field.far_ptr = v;
	field->field.fieldType = GF_SG_VRML_SFROTATION;
	v->x = x;
	v->y = y;
	v->z = z;
	v->q = q;
	JS_SetPrivate(c, obj, field);
	return field;
}
static JSBool SFRotationConstructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rv)
{
	JSObject *an_obj;
	SFVec3f v1, v2;
	Fixed l1, l2, dot;
	jsdouble x = 0.0, y = 0.0, z = 0.0, a = 0.0;
	if (argc == 0) {
		SFRotation_Create(c, obj, FLT2FIX(x), FLT2FIX(y), FIX_ONE, FLT2FIX(a) );
		return JS_TRUE;
	}
	if ((argc>0) && JSVAL_IS_NUMBER(argv[0])) {
		if (argc > 0) JS_ValueToNumber(c, argv[0], &x);
		if (argc > 1) JS_ValueToNumber(c, argv[1], &y);
		if (argc > 2) JS_ValueToNumber(c, argv[2], &z);
		if (argc > 3) JS_ValueToNumber(c, argv[3], &a);
		SFRotation_Create(c, obj, FLT2FIX(x), FLT2FIX(y), FLT2FIX(z), FLT2FIX(a));
		return JS_TRUE;
	}
	if (argc!=2) return JS_FALSE;
	if (!JSVAL_IS_OBJECT(argv[0])) return JS_FALSE;
	an_obj = JSVAL_TO_OBJECT(argv[0]);
	if (! JS_InstanceOf(c, an_obj, &js_rt->SFVec3fClass, NULL)) return JS_FALSE;
	v1 = * (SFVec3f *) ((GF_JSField *) JS_GetPrivate(c, an_obj))->field.far_ptr;
	if (JSVAL_IS_DOUBLE(argv[1])) {
		JS_ValueToNumber(c, argv[1], &a);
		SFRotation_Create(c, obj, v1.x, v1.y, v1.z, FLT2FIX(a));
		return JS_TRUE;
	}

	if (!JSVAL_IS_OBJECT(argv[1])) return JS_FALSE;
	an_obj = JSVAL_TO_OBJECT(argv[1]);
	if (!JS_InstanceOf(c, an_obj, &js_rt->SFVec3fClass, NULL)) return JS_FALSE;
	v2 = * (SFVec3f *) ((GF_JSField *) JS_GetPrivate(c, an_obj))->field.far_ptr;
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

static JSBool rot_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	GF_JSField *val = (GF_JSField *) JS_GetPrivate(c, obj);
	if (JSVAL_IS_INT(id)) {
		switch (JSVAL_TO_INT(id)) {
		case 0: *vp = DOUBLE_TO_JSVAL(JS_NewDouble(c, FIX2FLT( ((SFRotation*)val->field.far_ptr)->x)) ); break;
		case 1: *vp = DOUBLE_TO_JSVAL(JS_NewDouble(c, FIX2FLT( ((SFRotation*)val->field.far_ptr)->y)) ); break;
		case 2: *vp = DOUBLE_TO_JSVAL(JS_NewDouble(c, FIX2FLT( ((SFRotation*)val->field.far_ptr)->z)) ); break;
		case 3: *vp = DOUBLE_TO_JSVAL(JS_NewDouble(c, FIX2FLT( ((SFRotation*)val->field.far_ptr)->q)) ); break;
		default: return JS_TRUE;
		}
	}
	return JS_TRUE;
}
static JSBool rot_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	jsdouble d;
	Fixed v;
	Bool changed = 0;
	GF_JSField *ptr = (GF_JSField *) JS_GetPrivate(c, obj);

	/*this is the prototype*/
	if (!ptr) {
		if (! JSVAL_IS_STRING(id)) return JS_FALSE;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[VRML JS] setting new prop/func %s for object prototype\n", JS_GetStringBytes(JSVAL_TO_STRING(id)) ));
		return JS_TRUE;
	}

	if (JSVAL_IS_INT(id) && JSVAL_TO_INT(id) >= 0 && JSVAL_TO_INT(id) < 4 && JS_ValueToNumber(c, *vp, &d)) {
		switch (JSVAL_TO_INT(id)) {
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
		default: return JS_TRUE;
		}
		if (changed) Script_FieldChanged(c, NULL, ptr, NULL);
		return JS_TRUE;
    }
	return JS_FALSE;
}
static JSBool rot_getAxis(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	SFRotation r;
	JSObject *pNew;
	r = * (SFRotation *) ((GF_JSField *) JS_GetPrivate(c, obj))->field.far_ptr;
	pNew = JS_NewObject(c, &js_rt->SFVec3fClass, 0, JS_GetParent(c, obj));  
	SFVec3f_Create(c, pNew, r.x, r.y, r.z);
	*rval = OBJECT_TO_JSVAL(pNew);
	return JS_TRUE;
}
static JSBool rot_inverse(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	SFRotation r;
	JSObject *pNew;
	r = * (SFRotation *) ((GF_JSField *) JS_GetPrivate(c, obj))->field.far_ptr;
	pNew = JS_NewObject(c, &js_rt->SFRotationClass, 0, JS_GetParent(c, obj));  
	SFRotation_Create(c, pNew, r.x, r.y, r.z, r.q-GF_PI);
	*rval = OBJECT_TO_JSVAL(pNew);
	return JS_TRUE;
}

static JSBool rot_multiply(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	SFRotation r1, r2;
	SFVec4f q1, q2;
	JSObject *pNew;

	if (argc<=0 || !JSVAL_IS_OBJECT(argv[0]) || !JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[0]), &js_rt->SFRotationClass, NULL))
		return JS_FALSE;
	
	r1 = * (SFRotation *) ((GF_JSField *) JS_GetPrivate(c, obj))->field.far_ptr;
	r2 = * (SFRotation *) ((GF_JSField *) JS_GetPrivate(c, JSVAL_TO_OBJECT(argv[0])))->field.far_ptr;
	q1 = gf_quat_from_rotation(r1);
	q2 = gf_quat_from_rotation(r2);
	q1 = gf_quat_multiply(&q1, &q2);
	r1 = gf_quat_to_rotation(&q1);

	pNew = JS_NewObject(c, &js_rt->SFRotationClass, 0, JS_GetParent(c, obj)); 
	SFRotation_Create(c, pNew, r1.x, r1.y, r1.z, r1.q);
	*rval = OBJECT_TO_JSVAL(pNew);
	return JS_TRUE;
}
static JSBool rot_multVec(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	SFVec3f v;
	SFRotation r;
	GF_Matrix mx;
	JSObject *pNew;
	if (argc<=0) return JS_FALSE;

	if (argc<=0 || !JSVAL_IS_OBJECT(argv[0]) || !JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[0]), &js_rt->SFVec3fClass, NULL))
		return JS_FALSE;

	r = *(SFRotation *) ((GF_JSField *) JS_GetPrivate(c, obj))->field.far_ptr;
	v = *(SFVec3f *) ((GF_JSField *) JS_GetPrivate(c, JSVAL_TO_OBJECT(argv[0])))->field.far_ptr;
	gf_mx_init(mx);
	gf_mx_add_rotation(&mx, r.q, r.x, r.y, r.z);
	gf_mx_apply_vec(&mx, &v);
	pNew = JS_NewObject(c, &js_rt->SFVec3fClass, 0, JS_GetParent(c, obj));  
	SFVec3f_Create(c, pNew, v.x, v.y, v.z);
	*rval = OBJECT_TO_JSVAL(pNew);
	return JS_TRUE;
}
static JSBool rot_setAxis(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	SFVec3f v;
	SFRotation r;
	if (argc<=0) return JS_FALSE;

	if (argc<=0 || !JSVAL_IS_OBJECT(argv[0]) || !JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[0]), &js_rt->SFVec3fClass, NULL))
		return JS_FALSE;

	r = *(SFRotation *) ((GF_JSField *) JS_GetPrivate(c, obj))->field.far_ptr;
	v = *(SFVec3f *) ((GF_JSField *) JS_GetPrivate(c, JSVAL_TO_OBJECT(argv[0])))->field.far_ptr;
	r.x = v.x;
	r.y = v.y;
	r.z = v.z;
	return JS_TRUE;
}
static JSBool rot_slerp(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	SFRotation v1, v2, res;
	SFVec4f q1, q2;
	JSObject *pNew;
	jsdouble d;
	if (argc<=1) return JS_FALSE;

	if (!JSVAL_IS_DOUBLE(argv[1]) || !JSVAL_IS_OBJECT(argv[0]) || !JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[0]), &js_rt->SFRotationClass, NULL)) return JS_FALSE;

	v1 = *(SFRotation *) ((GF_JSField *) JS_GetPrivate(c, obj))->field.far_ptr;
	v2 = *(SFRotation *) ((GF_JSField *) JS_GetPrivate(c, JSVAL_TO_OBJECT(argv[0])))->field.far_ptr;
	JS_ValueToNumber(c, argv[1], &d );
	q1 = gf_quat_from_rotation(v1);
	q2 = gf_quat_from_rotation(v2);
	q1 = gf_quat_slerp(q1, q2, FLT2FIX( d));
	res = gf_quat_to_rotation(&q1);
	pNew = JS_NewObject(c, &js_rt->SFRotationClass, 0, JS_GetParent(c, obj));  
	SFRotation_Create(c, pNew, res.x, res.y, res.z, res.q);
	*rval = OBJECT_TO_JSVAL(pNew);
	return JS_TRUE;
}

/* SFColor class functions */
static GFINLINE GF_JSField *SFColor_Create(JSContext *c, JSObject *obj, Fixed r, Fixed g, Fixed b)
{
	GF_JSField *field;
	SFColor *v;
	field = NewJSField();
	v = gf_sg_vrml_field_pointer_new(GF_SG_VRML_SFCOLOR);
	field->field_ptr = field->field.far_ptr = v;
	field->field.fieldType = GF_SG_VRML_SFCOLOR;
	v->red = r;
	v->green = g;
	v->blue = b;
	JS_SetPrivate(c, obj, field);
	return field;
}
static JSBool SFColorConstructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rv)
{
	jsdouble r = 0.0, g = 0.0, b = 0.0;
	if (argc > 0) JS_ValueToNumber(c, argv[0], &r);
	if (argc > 1) JS_ValueToNumber(c, argv[1], &g);
	if (argc > 2) JS_ValueToNumber(c, argv[2], &b);
	SFColor_Create(c, obj, FLT2FIX( r), FLT2FIX( g), FLT2FIX( b));
	return JS_TRUE;
}
static JSBool color_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	GF_JSField *val = (GF_JSField *) JS_GetPrivate(c, obj);
	if (JSVAL_IS_INT(id)) {
		switch (JSVAL_TO_INT(id)) {
		case 0: *vp = DOUBLE_TO_JSVAL(JS_NewDouble(c, FIX2FLT( ((SFColor*)val->field.far_ptr)->red)) ); break;
		case 1: *vp = DOUBLE_TO_JSVAL(JS_NewDouble(c, FIX2FLT( ((SFColor*)val->field.far_ptr)->green)) ); break;
		case 2: *vp = DOUBLE_TO_JSVAL(JS_NewDouble(c, FIX2FLT( ((SFColor*)val->field.far_ptr)->blue)) ); break;
		default: return JS_TRUE;
		}
	}
	return JS_TRUE;
}

static JSBool color_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	jsdouble d;
	Fixed v;
	Bool changed = 0;
	GF_JSField *ptr = (GF_JSField *) JS_GetPrivate(c, obj);
	/*this is the prototype*/
	if (!ptr) {
		if (! JSVAL_IS_STRING(id)) return JS_FALSE;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[VRML JS] setting new prop/func %s for object prototype\n", JS_GetStringBytes(JSVAL_TO_STRING(id)) ));
		return JS_TRUE;
	}

	if (JSVAL_IS_INT(id) && JSVAL_TO_INT(id) >= 0 && JSVAL_TO_INT(id) < 3 && JS_ValueToNumber(c, *vp, &d)) {
		switch (JSVAL_TO_INT(id)) {
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
static JSBool color_setHSV(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rv)
{
	SFColor *v1, hsv;
	jsdouble h, s, v;
	GF_JSField *ptr = (GF_JSField *) JS_GetPrivate(c, obj);
	if (argc != 3) return JS_FALSE;
	v1 = ((GF_JSField *) JS_GetPrivate(c, obj))->field.far_ptr;
	JS_ValueToNumber( c, argv[0], &h);
	JS_ValueToNumber( c, argv[1], &s);
	JS_ValueToNumber( c, argv[2], &v);
	hsv.red = FLT2FIX( h);
	hsv.green = FLT2FIX( s);
	hsv.blue = FLT2FIX( v);
	SFColor_fromHSV(&hsv);
	gf_sg_vrml_field_copy(v1, &hsv, GF_SG_VRML_SFCOLOR);
	Script_FieldChanged(c, NULL, ptr, NULL);
	return JS_TRUE;
}

static JSBool color_getHSV(JSContext *c, JSObject *obj, uintN n, jsval *va, jsval *rval)
{
	SFColor *v1, hsv;
	jsval vec[3];
	JSObject *arr;

	v1 = ((GF_JSField *) JS_GetPrivate(c, obj))->field.far_ptr;
	hsv = *v1;
	SFColor_toHSV(&hsv);
	vec[0] = DOUBLE_TO_JSVAL(JS_NewDouble(c, FIX2FLT(hsv.red)));
	vec[1] = DOUBLE_TO_JSVAL(JS_NewDouble(c, FIX2FLT(hsv.green)));
	vec[2] = DOUBLE_TO_JSVAL(JS_NewDouble(c, FIX2FLT(hsv.blue)));
	arr = JS_NewArrayObject(c, 3, vec);
	*rval = OBJECT_TO_JSVAL(arr);
	return JS_TRUE;
}


static JSBool MFArrayConstructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval, u32 fieldType)
{
	GF_ScriptPriv *priv = JS_GetScriptStack(c);
	GF_JSField *ptr = NewJSField();
	ptr->field.fieldType = fieldType;
	ptr->js_list = JS_NewArrayObject(c, (jsint) argc, argv);
	JS_AddRoot(c, &ptr->js_list);
	gf_list_add(priv->js_cache, obj);

	JS_SetPrivate(c, obj, ptr);
	*rval = OBJECT_TO_JSVAL(obj);
	return obj == 0 ? JS_FALSE : JS_TRUE;
}

static JSBool MFBoolConstructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return MFArrayConstructor(c, obj, argc, argv, rval, GF_SG_VRML_MFBOOL);
}
static JSBool MFInt32Constructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return MFArrayConstructor(c, obj, argc, argv, rval, GF_SG_VRML_MFINT32);
}
static JSBool MFFloatConstructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return MFArrayConstructor(c, obj, argc, argv, rval, GF_SG_VRML_MFFLOAT);
}
static JSBool MFTimeConstructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return MFArrayConstructor(c, obj, argc, argv, rval, GF_SG_VRML_MFTIME);
}
static JSBool MFStringConstructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return MFArrayConstructor(c, obj, argc, argv, rval, GF_SG_VRML_MFSTRING);
}
static JSBool MFURLConstructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return MFArrayConstructor(c, obj, argc, argv, rval, GF_SG_VRML_MFURL);
}
static JSBool MFNodeConstructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return MFArrayConstructor(c, obj, argc, argv, rval, GF_SG_VRML_MFNODE);
}


static void array_finalize_ex(JSContext *c, JSObject *obj, Bool is_js_call)
{
	GF_JSField *ptr = (GF_JSField *) JS_GetPrivate(c, obj);
	if (is_js_call)
		JS_ObjectDestroyed(c, obj);
	if (!ptr) return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[VRML JS] unregistering MFField %s\n", ptr->field.name));

	if (ptr->js_list) JS_RemoveRoot(c, &ptr->js_list);

	/*MFNode*/
	if (ptr->temp_list) {
		gf_node_unregister_children(ptr->owner, ptr->temp_list);
	} 
	if (ptr->field_ptr) {
		gf_sg_vrml_field_pointer_del(ptr->field_ptr, ptr->field.fieldType);
	}
	free(ptr);
}
static void array_finalize(JSContext *c, JSObject *obj)
{
	array_finalize_ex(c, obj, 1);
}

JSBool array_getElement(JSContext *c, JSObject *obj, jsval id, jsval *rval)
{
	u32 i;
	GF_JSField *ptr = (GF_JSField *) JS_GetPrivate(c, obj);
	if (JSVAL_IS_INT(id)) {
		i = JSVAL_TO_INT(id);
		JS_GetElement(c, ptr->js_list, (jsint) i, rval);
	}
	return JS_TRUE;
}


//this could be overloaded for each MF type...
JSBool array_setElement(JSContext *c, JSObject *obj, jsval id, jsval *rval)
{
	u32 ind, len;
	jsdouble d;
	GF_JSField *from;
	JSBool ret;
	JSClass *the_sf_class = NULL;
	JSString *str;
	char *str_val;
	void *sf_slot;
	GF_JSField *ptr = (GF_JSField *) JS_GetPrivate(c, obj);
	ind = JSVAL_TO_INT(id);

	ret = JS_GetArrayLength(c, ptr->js_list, &len);
	if (ret==JS_FALSE) return JS_FALSE;

	if (gf_sg_vrml_is_sf_field(ptr->field.fieldType)) return JS_FALSE;

	switch (ptr->field.fieldType) {
	case GF_SG_VRML_MFVEC2F: the_sf_class = &js_rt->SFVec2fClass; break;
	case GF_SG_VRML_MFVEC3F: the_sf_class = &js_rt->SFVec3fClass; break;
	case GF_SG_VRML_MFCOLOR: the_sf_class = &js_rt->SFColorClass; break;
	case GF_SG_VRML_MFROTATION: the_sf_class = &js_rt->SFRotationClass; break;
	}
	/*dynamic expend*/
	if (ind>=len) {
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
				a_val = DOUBLE_TO_JSVAL( JS_NewDouble(c, 0) );
				break;
			case GF_SG_VRML_MFSTRING:
			case GF_SG_VRML_MFURL:
				a_val = STRING_TO_JSVAL( JS_NewStringCopyZ(c, "") );
				break;
			case GF_SG_VRML_MFVEC2F: 
			case GF_SG_VRML_MFVEC3F: 
			case GF_SG_VRML_MFCOLOR:
			case GF_SG_VRML_MFROTATION: 
				a_val = OBJECT_TO_JSVAL( JS_ConstructObject(c, the_sf_class, 0, obj) );
				break;
			default: 
				a_val = INT_TO_JSVAL(0); 
				break;
			}

			if (ptr->field.fieldType!=GF_SG_VRML_MFNODE)
				gf_sg_vrml_mf_insert(ptr->field.far_ptr, ptr->field.fieldType, &sf_slot, len);

			JS_SetElement(c, ptr->js_list, len, &a_val);
			len++;
		}
		if (ptr->field.fieldType!=GF_SG_VRML_MFNODE)
			gf_sg_vrml_mf_insert(ptr->field.far_ptr, ptr->field.fieldType, &sf_slot, ind);
	}

	/*assign object*/
	if (ptr->field.fieldType==GF_SG_VRML_MFNODE) {
		if (JSVAL_IS_VOID(*rval) || !JS_InstanceOf(c, JSVAL_TO_OBJECT(*rval), &js_rt->SFNodeClass, NULL) ) return JS_FALSE;
	} else if (the_sf_class) {
		if (JSVAL_IS_VOID(*rval)) return JS_FALSE;
		if (!JS_InstanceOf(c, JSVAL_TO_OBJECT(*rval), the_sf_class, NULL) ) return JS_FALSE;
	} else if (ptr->field.fieldType==GF_SG_VRML_MFBOOL) {
		if (!JSVAL_IS_BOOLEAN(*rval)) return JS_FALSE;
	} else if (ptr->field.fieldType==GF_SG_VRML_MFINT32) {
		if (!JSVAL_IS_INT(*rval)) return JS_FALSE;
	} else if (ptr->field.fieldType==GF_SG_VRML_MFFLOAT) {
		if (!JSVAL_IS_NUMBER(*rval)) return JS_FALSE;
	} else if (ptr->field.fieldType==GF_SG_VRML_MFTIME) {
		if (!JSVAL_IS_NUMBER(*rval)) return JS_FALSE;
	} else if (ptr->field.fieldType==GF_SG_VRML_MFSTRING) {
		if (!JSVAL_IS_STRING(*rval)) return JS_FALSE;
	} else if (ptr->field.fieldType==GF_SG_VRML_MFURL) {
		if (!JSVAL_IS_STRING(*rval)) return JS_FALSE;
	}
	ret = JS_SetElement(c, ptr->js_list, ind, rval);
	if (ret==JS_FALSE) return JS_FALSE;

	if (!ptr->owner) return JS_TRUE;

	/*rewrite MFNode entry*/
	if (ptr->field.fieldType==GF_SG_VRML_MFNODE) {
		GF_Node *prev_n, *new_n;
		/*get and delete previous node if any, but unregister later*/
		prev_n = gf_node_list_del_child_idx( (GF_ChildNodeItem **)ptr->field.far_ptr, ind);

		/*get new node*/
		from = (GF_JSField *) JS_GetPrivate(c, JSVAL_TO_OBJECT(*rval));
		new_n = *(GF_Node**)from->field.far_ptr;
		if (new_n) {
			gf_node_list_insert_child( (GF_ChildNodeItem **)ptr->field.far_ptr , new_n, ind);
			vrml_node_register(new_n, ptr->owner);
		}
		/*unregister previous node*/
		if (prev_n) gf_node_unregister(prev_n, ptr->owner);

		Script_FieldChanged(c, NULL, ptr, NULL);
		return JS_TRUE;
	}

	/*rewrite MF slot*/
	switch (ptr->field.fieldType) {
	case GF_SG_VRML_MFBOOL:
		((MFBool *)ptr->field.far_ptr)->vals[ind] = (Bool) JSVAL_TO_BOOLEAN(*rval);
		break;
	case GF_SG_VRML_MFINT32:
		((MFInt32 *)ptr->field.far_ptr)->vals[ind] = (s32) JSVAL_TO_INT(*rval);
		break;
	case GF_SG_VRML_MFFLOAT:
		JS_ValueToNumber(c, *rval, &d);
		((MFFloat *)ptr->field.far_ptr)->vals[ind] = FLT2FIX(d);
		break;
	case GF_SG_VRML_MFTIME:
		JS_ValueToNumber(c, *rval, &d);
		((MFTime *)ptr->field.far_ptr)->vals[ind] = d;
		break;
	case GF_SG_VRML_MFSTRING:
		if (((MFString *)ptr->field.far_ptr)->vals[ind]) {
			free(((MFString *)ptr->field.far_ptr)->vals[ind]);
			((MFString *)ptr->field.far_ptr)->vals[ind] = NULL;
		}
		str = JSVAL_IS_STRING(*rval) ? JSVAL_TO_STRING(*rval) : JS_ValueToString(c, *rval);
		str_val = JS_GetStringBytes(str);
		((MFString *)ptr->field.far_ptr)->vals[ind] = strdup(str_val);
		break;

	case GF_SG_VRML_MFURL:
		if (((MFURL *)ptr->field.far_ptr)->vals[ind].url) {
			free(((MFURL *)ptr->field.far_ptr)->vals[ind].url);
			((MFURL *)ptr->field.far_ptr)->vals[ind].url = NULL;
		}
		str = JSVAL_IS_STRING(*rval) ? JSVAL_TO_STRING(*rval) : JS_ValueToString(c, *rval);
		str_val = JS_GetStringBytes(str);
		((MFURL *)ptr->field.far_ptr)->vals[ind].url = strdup(str_val);
		((MFURL *)ptr->field.far_ptr)->vals[ind].OD_ID = 0;
		break;

	case GF_SG_VRML_MFVEC2F:
	case GF_SG_VRML_MFVEC3F:
	case GF_SG_VRML_MFROTATION:
	case GF_SG_VRML_MFCOLOR:
		from = (GF_JSField *) JS_GetPrivate(c, JSVAL_TO_OBJECT(*rval));
		gf_sg_vrml_field_copy(& ((GenMFField *)ptr->field.far_ptr)->array[ind], from->field.far_ptr, from->field.fieldType);
		break;
	}

	Script_FieldChanged(c, NULL, ptr, NULL);
	return JS_TRUE;
}

JSBool array_setLength(JSContext *c, JSObject *obj, jsval v, jsval *val)
{
	u32 len, i, sftype;
	JSBool ret;
	JSClass *the_sf_class;
	GF_JSField *ptr = (GF_JSField *) JS_GetPrivate(c, obj);
	if (!JSVAL_IS_INT(*val) || JSVAL_TO_INT(*val) < 0) return JS_FALSE;
	len = JSVAL_TO_INT(*val);
	ret = JS_SetArrayLength(c, ptr->js_list, len);
	if (ret==JS_FALSE) return ret;

	if (!len) {
		if (ptr->field.fieldType==GF_SG_VRML_MFNODE) {
			gf_node_unregister_children(ptr->owner, *(GF_ChildNodeItem**)ptr->field.far_ptr);
			*(GF_ChildNodeItem**)ptr->field.far_ptr = NULL;
		} else {
			gf_sg_vrml_mf_reset(ptr->field.far_ptr, ptr->field.fieldType);
		}
		return JS_TRUE;
	}

#if 0
	/*insert till index if needed*/
	if (ptr->field.fieldType != GF_SG_VRML_MFNODE) {
		if (!ptr->field.far_ptr) ptr->field_ptr = ptr->field.far_ptr = gf_sg_vrml_field_pointer_new(ptr->field.fieldType);
		gf_sg_vrml_mf_reset(ptr->field.far_ptr, ptr->field.fieldType);
		gf_sg_vrml_mf_alloc(ptr->field.far_ptr, ptr->field.fieldType, len);
		if (ptr->field_ptr) ptr->field_ptr = ptr->field.far_ptr;
	}
#endif

	the_sf_class = NULL;
	switch (ptr->field.fieldType) {
	case GF_SG_VRML_MFVEC2F: the_sf_class = &js_rt->SFVec2fClass; break;
	case GF_SG_VRML_MFVEC3F: the_sf_class = &js_rt->SFVec3fClass; break;
	case GF_SG_VRML_MFCOLOR: the_sf_class = &js_rt->SFColorClass; break;
	case GF_SG_VRML_MFROTATION: the_sf_class = &js_rt->SFRotationClass; break;
	case GF_SG_VRML_MFNODE: the_sf_class = &js_rt->SFNodeClass; break;
	}
	sftype = gf_sg_vrml_get_sf_type(ptr->field.fieldType);
	for (i=0; i<len; i++) {
		jsval a_val;
		if (the_sf_class) {
			JSObject *an_obj = JS_ConstructObject(c, the_sf_class, 0, obj);
			a_val = OBJECT_TO_JSVAL(an_obj );
		} else {
			switch (sftype) {
			case GF_SG_VRML_SFBOOL: a_val = BOOLEAN_TO_JSVAL(0); break;
			case GF_SG_VRML_SFINT32: a_val = INT_TO_JSVAL(0); break;
			case GF_SG_VRML_SFFLOAT: 
			case GF_SG_VRML_SFTIME: 
				a_val = DOUBLE_TO_JSVAL( JS_NewDouble(c, 0) );
				break;
			case GF_SG_VRML_SFSTRING:
			case GF_SG_VRML_SFURL:
				a_val = STRING_TO_JSVAL( JS_NewStringCopyZ(c, "") );
				break;
			default: a_val = INT_TO_JSVAL(0); break;
			}
		}
		JS_SetElement(c, ptr->js_list, i, &a_val);
	}
	return JS_TRUE;
}

JSBool array_getLength(JSContext *c, JSObject *obj, jsval v, jsval *val)
{
	jsuint len;
	GF_JSField *ptr = (GF_JSField *) JS_GetPrivate(c, obj);
	JSBool ret = JS_GetArrayLength(c, ptr->js_list, &len);
	*val = INT_TO_JSVAL(len);
	return ret;
}


/* MFVec2f class constructor */
static JSBool MFVec2fConstructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	jsval val;
	JSObject *item;
	u32 i;
	GF_JSField *ptr = NewJSField();
	ptr->js_list = JS_NewArrayObject(c, 0, 0);
	JS_SetArrayLength(c, ptr->js_list, argc);
	JS_SetPrivate(c, obj, ptr);

	for (i=0; i<argc; i++) {
		if (!JSVAL_IS_OBJECT(argv[i]) || !JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[i]), &js_rt->SFVec2fClass, NULL) ) {
			item = JS_ConstructObject(c, &js_rt->SFVec2fClass, 0, obj);
			val = OBJECT_TO_JSVAL(item);
			JS_SetElement(c, ptr->js_list, i, &val);
		} else {
			JS_SetElement(c, ptr->js_list, i, &argv[i]);
		}
	}
	*rval = OBJECT_TO_JSVAL(obj);
	return obj == 0 ? JS_FALSE : JS_TRUE;
}

/* MFVec3f class constructor */
static JSBool MFVec3fConstructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	jsval val;
	JSObject *item;
	u32 i;
	GF_JSField *ptr = NewJSField();
	ptr->field.fieldType = GF_SG_VRML_MFVEC3F;
	ptr->js_list = JS_NewArrayObject(c, (jsint) argc, argv);
	JS_SetArrayLength(c, ptr->js_list, argc);
	JS_SetPrivate(c, obj, ptr);

	for (i=0; i<argc; i++) {
		if (!JSVAL_IS_OBJECT(argv[i]) || !JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[i]), &js_rt->SFVec3fClass, NULL) ) {
			item = JS_ConstructObject(c, &js_rt->SFVec3fClass, 0, obj);
			val = OBJECT_TO_JSVAL(item);
			JS_SetElement(c, ptr->js_list, i, &val);
		} else {
			JS_SetElement(c, ptr->js_list, i, &argv[i]);
		}
	}
	*rval = OBJECT_TO_JSVAL(obj);
	return obj == 0 ? JS_FALSE : JS_TRUE;
}

/* MFRotation class constructor */
static JSBool MFRotationConstructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	jsval val;
	JSObject *item;
	u32 i;
	GF_JSField *ptr = NewJSField();
	ptr->field.fieldType = GF_SG_VRML_MFROTATION;
	ptr->js_list = JS_NewArrayObject(c, 0, 0);
	JS_SetArrayLength(c, ptr->js_list, argc);
	JS_SetPrivate(c, obj, ptr);

	for (i=0; i<argc; i++) {
		if (!JSVAL_IS_OBJECT(argv[i]) || !JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[i]), &js_rt->SFRotationClass, NULL) ) {
			item = JS_ConstructObject(c, &js_rt->SFVec3fClass, 0, obj);
			val = OBJECT_TO_JSVAL(item);
			JS_SetElement(c, ptr->js_list, i, &val);
		} else {
			JS_SetElement(c, ptr->js_list, i, &argv[i]);
		}
	}
	*rval = OBJECT_TO_JSVAL(obj);
	return obj == 0 ? JS_FALSE : JS_TRUE;
}

/*MFColor class constructor */
static JSBool MFColorConstructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	jsval val;
	JSObject *item;
	u32 i;
	GF_JSField *ptr = NewJSField();
	ptr->field.fieldType = GF_SG_VRML_MFCOLOR;
	ptr->js_list = JS_NewArrayObject(c, 0, 0);
	JS_SetArrayLength(c, ptr->js_list, argc);
	JS_SetPrivate(c, obj, ptr);

	for (i=0; i<argc; i++) {
		if (!JSVAL_IS_OBJECT(argv[i]) || !JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[i]), &js_rt->SFColorClass, NULL) ) {
			item = JS_ConstructObject(c, &js_rt->SFColorClass, 0, obj);
			val = OBJECT_TO_JSVAL(item);
			JS_SetElement(c, ptr->js_list, i, &val);
		} else {
			JS_SetElement(c, ptr->js_list, i, &argv[i]);
		}
	}
	*rval = OBJECT_TO_JSVAL(obj);
	return obj == 0 ? JS_FALSE : JS_TRUE;
}

#ifndef GPAC_DISABLE_SVG
JSBool dom_event_add_listener_ex(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval, GF_Node *vrml_node);
JSBool dom_event_remove_listener_ex(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval, GF_Node *vrml_node);

JSBool vrml_event_add_listener(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Node *node;
	GF_JSField *ptr;
	if (! JS_InstanceOf(c, obj, &js_rt->SFNodeClass, NULL) ) return JS_FALSE;
	ptr = (GF_JSField *) JS_GetPrivate(c, obj);
	assert(ptr->field.fieldType==GF_SG_VRML_SFNODE);
	node = * ((GF_Node **)ptr->field.far_ptr);
	
	return dom_event_add_listener_ex(c, obj, argc, argv, rval, node);
}
JSBool vrml_event_remove_listener(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Node *node;
	GF_JSField *ptr;
	if (! JS_InstanceOf(c, obj, &js_rt->SFNodeClass, NULL) ) return JS_FALSE;
	ptr = (GF_JSField *) JS_GetPrivate(c, obj);
	assert(ptr->field.fieldType==GF_SG_VRML_SFNODE);
	node = * ((GF_Node **)ptr->field.far_ptr);

	return dom_event_remove_listener_ex(c, obj, argc, argv, rval, node);
}
#endif

static JSBool vrml_dom3_not_implemented(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{	
	return JS_FALSE;
}


void gf_sg_script_init_sm_api(GF_ScriptPriv *sc, GF_Node *script)
{
	/*GCC port: classes are declared within code since JS_PropertyStub and co are exported symbols
	from JS runtime lib, so with non-constant addresses*/
	JS_SETUP_CLASS(js_rt->globalClass, "global", JSCLASS_HAS_PRIVATE, 
		JS_PropertyStub, JS_PropertyStub, JS_FinalizeStub);

	JS_SETUP_CLASS(js_rt->browserClass , "Browser", 0,
		JS_PropertyStub, JS_PropertyStub, JS_FinalizeStub);

	JS_SETUP_CLASS(js_rt->SFNodeClass, "SFNode", JSCLASS_HAS_PRIVATE,
		node_getProperty, node_setProperty, node_finalize);

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

	sc->js_obj = JS_NewObject(sc->js_ctx, &js_rt->globalClass, 0, 0 );
	JS_InitStandardClasses(sc->js_ctx, sc->js_obj);
	{
		JSFunctionSpec globalFunctions[] = {
			{"print",           JSPrint,          0},
			{ 0 }
		};
		JS_DefineFunctions(sc->js_ctx, sc->js_obj, globalFunctions );
	}
	/*remember pointer to scene graph!!*/
	JS_SetPrivate(sc->js_ctx, sc->js_obj, script->sgprivate->scenegraph);


	JS_DefineProperty(sc->js_ctx, sc->js_obj, "FALSE", BOOLEAN_TO_JSVAL(0), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT );
	JS_DefineProperty(sc->js_ctx, sc->js_obj, "TRUE", BOOLEAN_TO_JSVAL(1), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT );
	JS_DefineProperty(sc->js_ctx, sc->js_obj, "_this", PRIVATE_TO_JSVAL(script), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT );

	sc->js_browser = JS_DefineObject(sc->js_ctx, sc->js_obj, "Browser", &js_rt->browserClass, 0, 0 );
	JS_DefineProperty(sc->js_ctx, sc->js_browser, "_this", PRIVATE_TO_JSVAL(script), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT );
	{
		JSFunctionSpec browserFunctions[] = {
		  {"getName", getName, 0},
		  {"getVersion", getVersion, 0},
		  {"getCurrentSpeed", getCurrentSpeed, 0},
		  {"getCurrentFrameRate", getCurrentFrameRate, 0},
		  {"getWorldURL", getWorldURL, 0},
		  {"replaceWorld", replaceWorld, 0},
		  {"addRoute", addRoute, 0},
		  {"deleteRoute", deleteRoute, 0},
		  {"loadURL", loadURL, 0},
		  {"createVrmlFromString", createVrmlFromString, 0},
		  {"setDescription", setDescription, 0},
		  {"print",           JSPrint,          0},
		  {"getOption",  getOption,          0},
		  {"setOption",  setOption,          0},
		  {"getScript",  getScript,          0},
		  {"getProto",  getProto,          0},
		  {"getElementById",  getElementById,          0},
		  {0}
		};
		JS_DefineFunctions(sc->js_ctx, sc->js_browser, browserFunctions);
	}

	{
		JSFunctionSpec SFNodeMethods[] = {
			{"toString", node_toString, 0},
#ifndef GPAC_DISABLE_SVG
			{"addEventListenerNS", vrml_event_add_listener, 4},
			{"removeEventListenerNS", vrml_event_remove_listener, 4},
			{"addEventListener", vrml_event_add_listener, 3},
			{"removeEventListener", vrml_event_remove_listener, 3},
			{"dispatchEvent", vrml_dom3_not_implemented, 1},
#endif
			{0}
		};
		JSPropertySpec SFNodeProps[] = {
			{"__dummy",       0,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{0}
		};
		JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->SFNodeClass, SFNodeConstructor, 1, SFNodeProps, SFNodeMethods, 0, 0);
	}
	{
		JSPropertySpec SFVec2fProps[] = {
			{"x",       0,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{"y",       1,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{0}
		};
		JSFunctionSpec SFVec2fMethods[] = {
			{"add",             vec2f_add,      1},
			{"divide",          vec2f_divide,   1},
			{"dot",             vec2f_dot,      1},
			{"length",          vec2f_length,   0},
			{"multiply",        vec2f_multiply, 1},
			{"normalize",       vec2f_normalize,0},
			{"subtract",        vec2f_subtract, 1},
			{"negate",          vec2f_negate,   0},
			{"toString",        field_toString,       0},
			{0}
		};
		JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->SFVec2fClass, SFVec2fConstructor, 0, SFVec2fProps, SFVec2fMethods, 0, 0);
	}
	{
		JSPropertySpec SFVec3fProps[] = {
			{"x",       0,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{"y",       1,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{"z",       2,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{0}
		};
		JSFunctionSpec SFVec3fMethods[] = {
			{"add",             vec3f_add,      1},
			{"divide",          vec3f_divide,   1},
			{"dot",             vec3f_dot,      1},
			{"length",          vec3f_length,   0},
			{"multiply",        vec3f_multiply, 1},
			{"normalize",       vec3f_normalize,0},
			{"subtract",        vec3f_subtract, 1},
			{"cross",			vec3f_cross,	1},
			{"negate",          vec3f_negate,   0},
			{"toString",        field_toString,	0},
			{0}
		};
		JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->SFVec3fClass, SFVec3fConstructor, 0, SFVec3fProps, SFVec3fMethods, 0, 0);
	}
	{
		JSPropertySpec SFRotationProps[] = {
			{"xAxis",       0,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{"yAxis",       1,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{"zAxis",       2,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{"angle",   3,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{0}
		};
		JSFunctionSpec SFRotationMethods[] = {
			{"getAxis",         rot_getAxis,      1},
			{"inverse",         rot_inverse,   1},
			{"multiply",        rot_multiply,      1},
			{"multVec",         rot_multVec,   0},
			{"setAxis",			rot_setAxis, 1},
			{"slerp",			rot_slerp,0},
			{"toString",        field_toString,	0},
			{0}
		};
		JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->SFRotationClass, SFRotationConstructor, 0, SFRotationProps, SFRotationMethods, 0, 0);
	}
	{
		JSPropertySpec SFColorProps[] = {
			{"r",       0,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{"g",       1,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{"b",       2,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{0}
		};
		JSFunctionSpec SFColorMethods[] = {
			{"setHSV",          color_setHSV,   3, 0, 0},
			{"getHSV",          color_getHSV,   0, 0, 0},
			{"toString",        field_toString,       0, 0, 0},
			{0, 0, 0, 0, 0}
		};
		JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->SFColorClass, SFColorConstructor, 0, SFColorProps, SFColorMethods, 0, 0);
	}
	{
		JSPropertySpec SFImageProps[] = {
			{"x",       0,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{"y",       1,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{"comp",    2,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{"array",   3,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{0}
		};
		JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->SFImageClass, SFImageConstructor, 0, SFImageProps, 0, 0, 0);
	}

	{
		JSPropertySpec MFArrayProp[] = {
			{ "length", 0, JSPROP_PERMANENT, array_getLength, array_setLength },
			{ "assign", 0, JSPROP_PERMANENT, array_getElement, array_setElement},
			{ 0, 0, 0, 0, 0 } 
		};
		JSFunctionSpec MFArrayMethods[] = {
			{"toString",        field_toString,       0},
			{0}
		};

		JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->MFInt32Class, MFInt32Constructor, 0, MFArrayProp, MFArrayMethods, 0, 0);
		JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->MFBoolClass, MFBoolConstructor, 0, MFArrayProp, MFArrayMethods, 0, 0);
		JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->MFFloatClass, MFFloatConstructor, 0, MFArrayProp, MFArrayMethods, 0, 0);
		JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->MFTimeClass, MFTimeConstructor, 0, MFArrayProp, MFArrayMethods, 0, 0);
		JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->MFStringClass, MFStringConstructor, 0, MFArrayProp, MFArrayMethods, 0, 0);
		JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->MFUrlClass, MFURLConstructor, 0, MFArrayProp, MFArrayMethods, 0, 0);
		JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->MFVec2fClass, MFVec2fConstructor, 0, MFArrayProp, MFArrayMethods, 0, 0);
		JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->MFVec3fClass, MFVec3fConstructor, 0, MFArrayProp, MFArrayMethods, 0, 0);
		JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->MFRotationClass, MFRotationConstructor, 0, MFArrayProp, MFArrayMethods, 0, 0);
		JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->MFColorClass, MFColorConstructor, 0, MFArrayProp, MFArrayMethods, 0, 0);
		JS_InitClass(sc->js_ctx, sc->js_obj, 0, &js_rt->MFNodeClass, MFNodeConstructor, 0, MFArrayProp, MFArrayMethods, 0, 0);
	}

/*
	cant get any doc specifying if these are supposed to be supported in MPEG4Script...
	JS_InitClass(sc->js_ctx, sc->js_obj, 0, &SFVec4fClass, SFVec4fConstructor, 0, SFVec4fProps, SFVec4fMethods, 0, 0);
	JS_InitClass(sc->js_ctx, sc->js_obj, 0, &MFVec4fClass, MFVec4fCons, 0, MFArrayProp, 0, 0, 0);
*/

}



void gf_sg_script_to_node_field(JSContext *c, jsval val, GF_FieldInfo *field, GF_Node *owner, GF_JSField *parent)
{
	jsdouble d;
	Bool changed;
	JSObject *obj;
	GF_JSField *p, *from;
	jsuint len;
	jsval item;
	u32 i;

	if (JSVAL_IS_VOID(val)) return;
	if ((field->fieldType != GF_SG_VRML_SFNODE) && JSVAL_IS_NULL(val)) return;


	switch (field->fieldType) {
	case GF_SG_VRML_SFBOOL:
	{
		if (JSVAL_IS_BOOLEAN(val)) {
			*((SFBool *) field->far_ptr) = JSVAL_TO_BOOLEAN(val);
			Script_FieldChanged(c, owner, parent, field);
		}
		return;
	}
	case GF_SG_VRML_SFINT32:
	{
		if (JSVAL_IS_INT(val) ) {
			* ((SFInt32 *) field->far_ptr) = JSVAL_TO_INT(val);
			Script_FieldChanged(c, owner, parent, field);
		} else if (JSVAL_IS_NUMBER(val) ) {
			JS_ValueToNumber(c, val, &d );
			*((SFInt32 *) field->far_ptr) = (s32) d;
			Script_FieldChanged(c, owner, parent, field);
		}
		return;
	}
	case GF_SG_VRML_SFFLOAT:
	{
		if (JSVAL_IS_NUMBER(val) ) {
			JS_ValueToNumber(c, val, &d );
			*((SFFloat *) field->far_ptr) = FLT2FIX( d);
			Script_FieldChanged(c, owner, parent, field);
		}
		return;
    }
	case GF_SG_VRML_SFTIME:
	{
		if (JSVAL_IS_NUMBER(val) ) {
			JS_ValueToNumber(c, val, &d );
			*((SFTime *) field->far_ptr) = (Double) d;
			Script_FieldChanged(c, owner, parent, field);
		}
		return;
    }
	case GF_SG_VRML_SFSTRING:
	{
		SFString *s = (SFString*)field->far_ptr;
		JSString *str = JSVAL_IS_STRING(val) ? JSVAL_TO_STRING(val) : JS_ValueToString(c, val);
		char *str_val = JS_GetStringBytes(str);
		/*we do filter strings since rebuilding a text is quite slow, so let's avoid killing the compositors*/
		if (!s->buffer || strcmp(str_val, s->buffer)) {
			if ( s->buffer) free(s->buffer);
			s->buffer = strdup(str_val);
			Script_FieldChanged(c, owner, parent, field);
		}
		return;
	}
	case GF_SG_VRML_SFURL:
	{
		JSString *str = JSVAL_IS_STRING(val) ? JSVAL_TO_STRING(val) : JS_ValueToString(c, val);
		if (((SFURL*)field->far_ptr)->url) free(((SFURL*)field->far_ptr)->url);
		((SFURL*)field->far_ptr)->url = strdup(JS_GetStringBytes(str));
		((SFURL*)field->far_ptr)->OD_ID = 0;
		Script_FieldChanged(c, owner, parent, field);
		return;
	}
	case GF_SG_VRML_MFSTRING:
		if (JSVAL_IS_STRING(val)) {
			JSString *str = JSVAL_TO_STRING(val);
			gf_sg_vrml_mf_reset(field->far_ptr, field->fieldType);
			gf_sg_vrml_mf_alloc(field->far_ptr, field->fieldType, 1);
			((MFString*)field->far_ptr)->vals[0] = strdup(JS_GetStringBytes(str));
			Script_FieldChanged(c, owner, parent, field);
			return;
		}
	case GF_SG_VRML_MFURL:
		if (JSVAL_IS_STRING(val)) {
			JSString *str = JSVAL_TO_STRING(val);
			gf_sg_vrml_mf_reset(field->far_ptr, field->fieldType);
			gf_sg_vrml_mf_alloc(field->far_ptr, field->fieldType, 1);
			((MFURL*)field->far_ptr)->vals[0].url = strdup(JS_GetStringBytes(str));
			((MFURL*)field->far_ptr)->vals[0].OD_ID = 0;
			Script_FieldChanged(c, owner, parent, field);
			return;
		}

	default:
		break;
	}

	//from here we must have an object
	if (! JSVAL_IS_OBJECT(val)) return;
	obj = JSVAL_TO_OBJECT(val) ;

	switch (field->fieldType) {
	case GF_SG_VRML_SFVEC2F:
	{
		if (JS_InstanceOf(c, obj, &js_rt->SFVec2fClass, NULL) ) {
			p = (GF_JSField *) JS_GetPrivate(c, obj);
			gf_sg_vrml_field_copy(field->far_ptr, p->field.far_ptr, GF_SG_VRML_SFVEC2F);
			Script_FieldChanged(c, owner, parent, field);
		}
		return;
	}
	case GF_SG_VRML_SFVEC3F:
	{
		if (JS_InstanceOf(c, obj, &js_rt->SFVec3fClass, NULL) ) {
			p = (GF_JSField *) JS_GetPrivate(c, obj);
			gf_sg_vrml_field_copy(field->far_ptr, p->field.far_ptr, GF_SG_VRML_SFVEC3F);
			Script_FieldChanged(c, owner, parent, field);
		}
		return;
	}
	case GF_SG_VRML_SFROTATION:
	{
		if ( JS_InstanceOf(c, obj, &js_rt->SFRotationClass, NULL) ) {
			p = (GF_JSField *) JS_GetPrivate(c, obj);
			gf_sg_vrml_field_copy(field->far_ptr, p->field.far_ptr, GF_SG_VRML_SFROTATION);
			Script_FieldChanged(c, owner, parent, field);
		}
		return;
	}
	case GF_SG_VRML_SFCOLOR:
	{
		if (JS_InstanceOf(c, obj, &js_rt->SFColorClass, NULL) ) {
			p = (GF_JSField *) JS_GetPrivate(c, obj);
			gf_sg_vrml_field_copy(field->far_ptr, p->field.far_ptr, GF_SG_VRML_SFCOLOR);
			Script_FieldChanged(c, owner, parent, field);
		}
		return;
	}
	case GF_SG_VRML_SFNODE:
	{
		/*replace object*/
		if (*((GF_Node**)field->far_ptr)) 
			gf_node_unregister(*((GF_Node**)field->far_ptr), owner);

		if (JSVAL_IS_NULL(val)) {
			field->far_ptr = NULL;
			Script_FieldChanged(c, owner, parent, field);
		} else if (JS_InstanceOf(c, obj, &js_rt->SFNodeClass, NULL) ) {
			GF_Node *n = * (GF_Node**) ((GF_JSField *) JS_GetPrivate(c, obj))->field.far_ptr;
			* ((GF_Node **)field->far_ptr) = n;
			vrml_node_register(n, owner);
			Script_FieldChanged(c, owner, parent, field);
		}
		return;
	}
	case GF_SG_VRML_SFIMAGE:
	{
		if ( JS_InstanceOf(c, obj, &js_rt->SFImageClass, NULL) ) {
			p = (GF_JSField *) JS_GetPrivate(c, obj);
			gf_sg_vrml_field_copy(field->far_ptr, p->field.far_ptr, GF_SG_VRML_SFIMAGE);
			Script_FieldChanged(c, owner, parent, field);
		}
		return;
	}
	default:
		break;
	}

	//from here we handle only MF fields 
	if ( !JS_InstanceOf(c, obj, &js_rt->MFBoolClass, NULL)
		&& !JS_InstanceOf(c, obj, &js_rt->MFInt32Class, NULL)
		&& !JS_InstanceOf(c, obj, &js_rt->MFFloatClass, NULL)
		&& !JS_InstanceOf(c, obj, &js_rt->MFTimeClass, NULL)
		&& !JS_InstanceOf(c, obj, &js_rt->MFStringClass, NULL)
		&& !JS_InstanceOf(c, obj, &js_rt->MFUrlClass, NULL)
		&& !JS_InstanceOf(c, obj, &js_rt->MFVec2fClass, NULL)
		&& !JS_InstanceOf(c, obj, &js_rt->MFVec3fClass, NULL)
		&& !JS_InstanceOf(c, obj, &js_rt->MFRotationClass, NULL)
		&& !JS_InstanceOf(c, obj, &js_rt->MFColorClass, NULL)
		&& !JS_InstanceOf(c, obj, &js_rt->MFNodeClass, NULL)
/*
		&& !JS_InstanceOf(c, obj, &MFVec4fClass, NULL)
*/
		) return;


	p = (GF_JSField *) JS_GetPrivate(c, obj);
	JS_GetArrayLength(c, p->js_list, &len);

	/*special handling for MF node, reset list first*/
	if (JS_InstanceOf(c, obj, &js_rt->MFNodeClass, NULL)) {
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
			if ( !JS_InstanceOf(c, node_obj, &js_rt->SFNodeClass, NULL)) break;
			from = (GF_JSField *) JS_GetPrivate(c, node_obj);

			child = * ((GF_Node**)from->field.far_ptr);

			gf_node_list_add_child_last( (GF_ChildNodeItem **) field->far_ptr , child, &last);
			vrml_node_register(child, owner);
		}
		Script_FieldChanged(c, owner, parent, field);
		return;
	}
	
	/*again, check text changes*/
	changed = (field->fieldType == GF_SG_VRML_MFSTRING) ? 0 : 1;
	/*realloc*/
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
				JS_ValueToNumber(c, item, &d);
				((MFFloat *)field->far_ptr)->vals[i] = FLT2FIX( d);
			}
			break;
		case GF_SG_VRML_MFTIME:
			if (JSVAL_IS_NUMBER(item)) {
				JS_ValueToNumber(c, item, &d);
				((MFTime *)field->far_ptr)->vals[i] = d;
			}
			break;
		case GF_SG_VRML_MFSTRING:
		{
			MFString *mfs = (MFString *) field->far_ptr;
			JSString *str = JSVAL_IS_STRING(item) ? JSVAL_TO_STRING(item) : JS_ValueToString(c, item);
			char *str_val = JS_GetStringBytes(str);
			if (!mfs->vals[i] || strcmp(str_val, mfs->vals[i]) ) {
				if (mfs->vals[i]) free(mfs->vals[i]);
				mfs->vals[i] = strdup(str_val);
				changed = 1;
			}
		}
			break;
		case GF_SG_VRML_MFURL:
		{
			MFURL *mfu = (MFURL *) field->far_ptr;
			JSString *str = JSVAL_IS_STRING(item) ? JSVAL_TO_STRING(item) : JS_ValueToString(c, item);
			if (mfu->vals[i].url) free(mfu->vals[i].url);
			mfu->vals[i].url = strdup(JS_GetStringBytes(str));
			mfu->vals[i].OD_ID = 0;
		}
			break;

		case GF_SG_VRML_MFVEC2F:
			if ( JSVAL_IS_OBJECT(item) && JS_InstanceOf(c, JSVAL_TO_OBJECT(item), &js_rt->SFVec2fClass, NULL) ) {
				from = (GF_JSField *) JS_GetPrivate(c, JSVAL_TO_OBJECT(item));
				gf_sg_vrml_field_copy(& ((MFVec2f*)field->far_ptr)->vals[i], from->field.far_ptr, GF_SG_VRML_SFVEC2F);
			}
			break;
		case GF_SG_VRML_MFVEC3F:
			if ( JSVAL_IS_OBJECT(item) && JS_InstanceOf(c, JSVAL_TO_OBJECT(item), &js_rt->SFVec3fClass, NULL) ) {
				from = (GF_JSField *) JS_GetPrivate(c, JSVAL_TO_OBJECT(item));
				gf_sg_vrml_field_copy(& ((MFVec3f*)field->far_ptr)->vals[i], from->field.far_ptr, GF_SG_VRML_SFVEC3F);
			}
			break;
		case GF_SG_VRML_MFROTATION:
			if ( JSVAL_IS_OBJECT(item) && JS_InstanceOf(c, JSVAL_TO_OBJECT(item), &js_rt->SFRotationClass, NULL) ) {
				from = (GF_JSField *) JS_GetPrivate(c, JSVAL_TO_OBJECT(item));
				gf_sg_vrml_field_copy(& ((MFRotation*)field->far_ptr)->vals[i], from->field.far_ptr, GF_SG_VRML_SFROTATION);
			}
			break;
		case GF_SG_VRML_MFCOLOR:
			if ( JSVAL_IS_OBJECT(item) && JS_InstanceOf(c, JSVAL_TO_OBJECT(item), &js_rt->SFColorClass, NULL) ) {
				SFColor *col;
				from = (GF_JSField *) JS_GetPrivate(c, JSVAL_TO_OBJECT(item));
				col = (SFColor *)from->field.far_ptr;
				gf_sg_vrml_field_copy(& ((MFColor*)field->far_ptr)->vals[i], from->field.far_ptr, GF_SG_VRML_SFCOLOR);
			}
			break;

		default:
			return;
		}
	}
	if (changed) Script_FieldChanged(c, owner, parent, field);
}

#define SETUP_FIELD	\
		jsf = NewJSField();	\
		jsf->owner = parent;	\
		if(parent) gf_node_get_field(parent, field->fieldIndex, &jsf->field);	\

#define SETUP_MF_FIELD	\
		jsf = (GF_JSField *) JS_GetPrivate(priv->js_ctx, obj);	\
		jsf->owner = parent;		\
		if (parent) gf_node_get_field(parent, field->fieldIndex, &jsf->field);	\



jsval gf_sg_script_to_smjs_field(GF_ScriptPriv *priv, GF_FieldInfo *field, GF_Node *parent, Bool skip_cache, Bool force_evaluate)
{
	u32 i;
	JSObject *obj = NULL;
	GF_JSField *jsf = NULL;
	GF_JSField *slot = NULL;
	GF_Node *n;
	jsdouble *d;
	jsval newVal;
	JSString *s;

	/*native types*/
	switch (field->fieldType) {
    case GF_SG_VRML_SFBOOL:
		return BOOLEAN_TO_JSVAL( * ((SFBool *) field->far_ptr) );
	case GF_SG_VRML_SFINT32:
		return INT_TO_JSVAL(  * ((SFInt32 *) field->far_ptr));
    case GF_SG_VRML_SFFLOAT:
		d = JS_NewDouble(priv->js_ctx, FIX2FLT(* ((SFFloat *) field->far_ptr) ));
		return DOUBLE_TO_JSVAL(d);
	case GF_SG_VRML_SFTIME:
		d = JS_NewDouble(priv->js_ctx, * ((SFTime *) field->far_ptr));
		return DOUBLE_TO_JSVAL(d);
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


	/*look into object bank in case we already have this object*/
	if (parent && !skip_cache && priv->js_cache) {
		i=0;
		while ((obj = gf_list_enum(priv->js_cache, &i))) {
			jsf = (GF_JSField *) JS_GetPrivate(priv->js_ctx, obj);
			if (jsf 
				&& (jsf->owner==parent) 
				&& (jsf->field.fieldIndex == field->fieldIndex)
				/*type check needed for MFNode entries*/
				&& (jsf->field.fieldType==field->fieldType) 
			) {

				GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[VRML JS] found cached jsobj 0x%08x (field %s) in script bank 0x%08x\n", obj, field->name, priv->js_cache) );
				if (!force_evaluate && !jsf->reevaluate) return OBJECT_TO_JSVAL(obj);

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
						newVal = DOUBLE_TO_JSVAL(JS_NewDouble(priv->js_ctx, FIX2FLT(f->vals[i])));
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
						newVal = DOUBLE_TO_JSVAL(JS_NewDouble(priv->js_ctx, f->vals[i]));
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
				case GF_SG_VRML_MFNODE:
					{
						GF_ChildNodeItem *f = *(GF_ChildNodeItem **) field->far_ptr;
						u32 count = 0;
						while (f) {
							count++;
							f = f->next;
						}
						/*this will destroy the different objects*/
						JS_SetArrayLength(priv->js_ctx, jsf->js_list, 0);
						if (JS_SetArrayLength(priv->js_ctx, jsf->js_list, count) != JS_TRUE) return JSVAL_NULL;

						count = 0;
						f = *(GF_ChildNodeItem **) field->far_ptr;
						while (f) {
							JSObject *pf = JS_NewObject(priv->js_ctx, &js_rt->SFNodeClass, 0, obj);
							slot = NewJSField();
							slot->owner = parent;
							slot->temp_node = f->node;
							slot->field.far_ptr = & slot->temp_node;
							slot->field.fieldType = GF_SG_VRML_SFNODE;
							vrml_node_register(f->node, parent);
							JS_SetPrivate(priv->js_ctx, pf, slot);

							newVal = OBJECT_TO_JSVAL(pf);
							JS_SetElement(priv->js_ctx, jsf->js_list, (jsint) count, &newVal);
							count++;

							f = f->next;
						}
					}
					break;
				}
				jsf->reevaluate = 0;
				return OBJECT_TO_JSVAL(obj);
			}
		}
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[VRML JS] creating jsobj %s.%s\n", gf_node_get_name(parent), field->name) );

	switch (field->fieldType) {
    case GF_SG_VRML_SFVEC2F:
		SETUP_FIELD
		obj = JS_NewObject(priv->js_ctx, &js_rt->SFVec2fClass, 0, priv->js_obj);
		break;
    case GF_SG_VRML_SFVEC3F:
		SETUP_FIELD
		obj = JS_NewObject(priv->js_ctx, &js_rt->SFVec3fClass, 0, priv->js_obj);
		break;
    case GF_SG_VRML_SFROTATION:
		SETUP_FIELD
		obj = JS_NewObject(priv->js_ctx, &js_rt->SFRotationClass, 0, priv->js_obj);
		break;
    case GF_SG_VRML_SFCOLOR:
		SETUP_FIELD
		obj = JS_NewObject(priv->js_ctx, &js_rt->SFColorClass, 0, priv->js_obj);
		break;
    case GF_SG_VRML_SFIMAGE:
		SETUP_FIELD
		obj = JS_NewObject(priv->js_ctx, &js_rt->SFImageClass, 0, priv->js_obj);
		break;
	case GF_SG_VRML_SFNODE:
		SETUP_FIELD
		obj = JS_NewObject(priv->js_ctx, &js_rt->SFNodeClass, 0, priv->js_obj);
		break;


	case GF_SG_VRML_MFBOOL:
	{
		MFBool *f = (MFBool *) field->far_ptr;
		obj = JS_ConstructObject(priv->js_ctx, &js_rt->MFBoolClass, 0, priv->js_obj);
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
		obj = JS_ConstructObject(priv->js_ctx, &js_rt->MFInt32Class, 0, priv->js_obj);
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
		obj = JS_ConstructObject(priv->js_ctx, &js_rt->MFFloatClass, 0, priv->js_obj);
		SETUP_MF_FIELD
		for (i=0; i<f->count; i++) {
			jsval newVal = DOUBLE_TO_JSVAL(JS_NewDouble(priv->js_ctx, FIX2FLT(f->vals[i])));
			JS_SetElement(priv->js_ctx, jsf->js_list, (jsint) i, &newVal);
		}
		break;
	}
	case GF_SG_VRML_MFTIME:
	{
		MFTime *f = (MFTime *) field->far_ptr;
		obj = JS_ConstructObject(priv->js_ctx, &js_rt->MFTimeClass, 0, priv->js_obj);
		SETUP_MF_FIELD
		for (i=0; i<f->count; i++) {
			jsval newVal = DOUBLE_TO_JSVAL( JS_NewDouble(priv->js_ctx, f->vals[i]) );
			JS_SetElement(priv->js_ctx, jsf->js_list, (jsint) i, &newVal);
		}
		break;
	}
	case GF_SG_VRML_MFSTRING:
	{
		MFString *f = (MFString *) field->far_ptr;
		obj = JS_ConstructObject(priv->js_ctx, &js_rt->MFStringClass, 0, priv->js_obj);
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
		obj = JS_ConstructObject(priv->js_ctx, &js_rt->MFUrlClass, 0, priv->js_obj);
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
		obj = JS_ConstructObject(priv->js_ctx, &js_rt->MFVec2fClass, 0, priv->js_obj);
		SETUP_MF_FIELD
		for (i=0; i<f->count; i++) {
			JSObject *pf = JS_NewObject(priv->js_ctx, &js_rt->SFVec2fClass, 0, obj);
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
		obj = JS_ConstructObject(priv->js_ctx, &js_rt->MFVec3fClass, 0, priv->js_obj);
		SETUP_MF_FIELD
		for (i=0; i<f->count; i++) {
			JSObject *pf = JS_NewObject(priv->js_ctx, &js_rt->SFVec3fClass, 0, obj);
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
		obj = JS_ConstructObject(priv->js_ctx, &js_rt->MFRotationClass, 0, priv->js_obj);
		SETUP_MF_FIELD
		for (i=0; i<f->count; i++) {
			JSObject *pf = JS_NewObject(priv->js_ctx, &js_rt->SFRotationClass, 0, obj);
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
		obj = JS_ConstructObject(priv->js_ctx, &js_rt->MFColorClass, 0, priv->js_obj);
		SETUP_MF_FIELD
		for (i=0; i<f->count; i++) {
			JSObject *pf = JS_NewObject(priv->js_ctx, &js_rt->SFColorClass, 0, obj);
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
		obj = JS_ConstructObject(priv->js_ctx, &js_rt->MFNodeClass, 0, priv->js_obj);
		SETUP_MF_FIELD
		size = gf_node_list_get_count(f);

		if (JS_SetArrayLength(priv->js_ctx, jsf->js_list, size) != JS_TRUE) return JSVAL_NULL;
		i=0;
		while (f) {
			JSObject *pf = JS_NewObject(priv->js_ctx, &js_rt->SFNodeClass, 0, obj);
			n = f->node;
			if (n->sgprivate->tag == TAG_ProtoNode) {
				GF_ProtoInstance *proto_inst = (GF_ProtoInstance *) n;
				if (!proto_inst->RenderingNode) 
					gf_sg_proto_instanciate(proto_inst);
			}
			slot = NewJSField();
			slot->owner = parent;
			slot->temp_node = n;
			slot->field.far_ptr = & slot->temp_node;
			slot->field.fieldType = GF_SG_VRML_SFNODE;
			slot->field.fieldIndex = jsf->field.fieldIndex;

			vrml_node_register(n, parent);
			JS_SetPrivate(priv->js_ctx, pf, slot);

			/*if this is the obj corresponding to an existing field/node, store it and prevent GC on object*/
			if (!skip_cache && priv->js_cache) {
				slot->obj = pf;
				JS_AddRoot(priv->js_ctx, &slot->obj);
				gf_list_add(priv->js_cache, pf);
			}


			newVal = OBJECT_TO_JSVAL(pf);
			JS_SetElement(priv->js_ctx, jsf->js_list, (jsint) i, &newVal);
			f = f->next;
			i++;
		}
		break;
	}

	//not supported
    default:
		return JSVAL_NULL;
    }

	if (!obj) return JSVAL_NULL;
	//store field associated with object if needed
	if (jsf) {
		if (parent) parent->sgprivate->flags |= GF_NODE_HAS_BINDING;
		
		JS_SetPrivate(priv->js_ctx, obj, jsf);
		/*if this is the obj corresponding to an existing field/node, store it and prevent GC on object*/
		if (parent && !skip_cache && priv->js_cache) {
			jsf->obj = obj;
			JS_AddRoot(priv->js_ctx, &jsf->obj);
			gf_list_add(priv->js_cache, obj);

			if (jsf->js_list)
				JS_AddRoot(priv->js_ctx, &jsf->js_list);
		}
	}
	return OBJECT_TO_JSVAL(obj);

}

static void JS_ReleaseRootObjects(GF_ScriptPriv *priv)
{
	if (priv->js_cache) {
		u32 i, count = gf_list_count(priv->js_cache);
		for (i=0; i<count; i++) {
			GF_JSField *jsf;
			JSObject *obj = gf_list_get(priv->js_cache, i);
			jsf = (GF_JSField *) JS_GetPrivate(priv->js_ctx, obj);

			/*				!!! WARNING !!!

			SpiderMonkey GC is handled at the JSRuntime level, not at the JSContext level. 
			Objects may not be finalized until the runtime is destroyed/GC'ed, which is not what we want. 
			We therefore destroy by hand all SFNode/MFNode
			*/
			if (jsf->obj) {
				JS_RemoveRoot(priv->js_ctx, &jsf->obj);
				jsf->obj = NULL;

				if (jsf->temp_node) {
					node_finalize_ex(priv->js_ctx, obj, 0);
					JS_SetPrivate(priv->js_ctx, obj, NULL);
				} 
			}
			if (jsf->js_list) {
				JS_RemoveRoot(priv->js_ctx, &jsf->js_list);
				jsf->js_list = NULL;
				if (jsf->temp_list) {
					array_finalize_ex(priv->js_ctx, obj, 0);
					JS_SetPrivate(priv->js_ctx, obj, NULL);
				}
			}
		}
	}
}

static void JS_PreDestroy(GF_Node *node)
{
	jsval fval, rval;
	GF_ScriptPriv *priv = node->sgprivate->UserPrivate;
	if (!priv) return;
	
	if (JS_LookupProperty(priv->js_ctx, priv->js_obj, "shutdown", &fval))
		if (! JSVAL_IS_VOID(fval))
			JS_CallFunctionValue(priv->js_ctx, priv->js_obj, fval, 0, NULL, &rval);

	/*unprotect all cached objects from GC*/
	JS_ReleaseRootObjects(priv);

	gf_sg_load_script_extensions(node->sgprivate->scenegraph, priv->js_ctx, priv->js_obj, 1);
	gf_sg_ecmascript_del(priv->js_ctx);

#ifndef GPAC_DISABLE_SVG
	dom_js_unload();
#endif
	
	if (priv->js_cache) gf_list_del(priv->js_cache);
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
			val = gf_sg_script_to_smjs_field(priv, &info, sc, 0, 0);
			break;
		case GF_SG_EVENT_OUT:
			gf_node_get_field(sc, sf->ALL_index, &info);
			val = gf_sg_script_to_smjs_field(priv, &info, sc, 0, 0);
			/*for native types directly modified*/
			JS_DefineProperty(priv->js_ctx, priv->js_obj, (const char *) sf->name, val, 0, gf_sg_script_eventout_set_prop, JSPROP_PERMANENT );
			break;
		default:
			gf_node_get_field(sc, sf->ALL_index, &info);
			val = gf_sg_script_to_smjs_field(priv, &info, sc, 0, 0);
			JS_DefineProperty(priv->js_ctx, priv->js_obj, (const char *) sf->name, val, 0, 0, JSPROP_PERMANENT);
			break;
		}
    }
}

static void flush_event_out(GF_Node *node, GF_ScriptPriv *priv)
{
	u32 i;
	GF_ScriptField *sf;

	/*flush event out*/
	i=0;
	while ((sf = gf_list_enum(priv->fields, &i))) {
		if (sf->activate_event_out) {
			sf->activate_event_out = 0;
			gf_node_event_out(node, sf->ALL_index);
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

	//locate function
	if (! JS_LookupProperty(priv->js_ctx, priv->js_obj, sf->name, &fval)) return;
	if (JSVAL_IS_VOID(fval)) return;

	argv[0] = gf_sg_script_to_smjs_field(priv, in_field, node, 0, 1);

	memset(&t_info, 0, sizeof(GF_FieldInfo));
	t_info.far_ptr = &sf->last_route_time;
	t_info.fieldType = GF_SG_VRML_SFTIME;
	t_info.fieldIndex = -1;
	t_info.name = "timestamp";
	argv[1] = gf_sg_script_to_smjs_field(priv, &t_info, node, 0, 1);

	/*protect args*/
	if (JSVAL_IS_GCTHING(argv[0])) JS_AddRoot(priv->js_ctx, &argv[0]);
	if (JSVAL_IS_GCTHING(argv[1])) JS_AddRoot(priv->js_ctx, &argv[1]);

	JS_CallFunctionValue(priv->js_ctx, priv->js_obj, fval, 2, argv, &rval);

	/*release args*/
	if (JSVAL_IS_GCTHING(argv[0])) JS_RemoveRoot(priv->js_ctx, &argv[0]);
	if (JSVAL_IS_GCTHING(argv[1])) JS_RemoveRoot(priv->js_ctx, &argv[1]);
	
	flush_event_out(node, priv);
	/*perform JS cleanup*/
	JS_MaybeGC(priv->js_ctx);
}

void JSScriptFromFile(GF_Node *node);

static Bool vrml_js_load_script(M_Script *script, char *file)
{
	FILE *jsf;
	char *jsscript;
	u32 fsize;
	Bool success = 1;
	JSBool ret;
	jsval rval, fval;
	GF_ScriptPriv *priv = (GF_ScriptPriv *) script->sgprivate->UserPrivate;

	jsf = fopen(file, "rb");
	if (!jsf) return 0;

	fseek(jsf, 0, SEEK_END);
	fsize = ftell(jsf);
	fseek(jsf, 0, SEEK_SET);
	jsscript = malloc(sizeof(char)*(fsize+1));
	fread(jsscript, sizeof(char)*fsize, 1, jsf);
	fclose(jsf);
	jsscript[fsize] = 0;

	ret = JS_EvaluateScript(priv->js_ctx, priv->js_obj, jsscript, sizeof(char)*fsize, 0, 0, &rval);
	if (ret==JS_FALSE) success = 0;

	if (success && JS_LookupProperty(priv->js_ctx, priv->js_obj, "initialize", &fval)) {
		if (! JSVAL_IS_VOID(fval)) {
			JS_CallFunctionValue(priv->js_ctx, priv->js_obj, fval, 0, NULL, &rval);
			flush_event_out((GF_Node *)script, priv);
		}
	}
	free(jsscript);
	return success;
}


static void JS_NetIO(void *cbck, GF_NETIO_Parameter *param)
{
	GF_Err e;
	JSFileDownload *jsdnload = (JSFileDownload *)cbck;
	M_Script *script = (M_Script *)jsdnload->node;

	e = param->error;
	if (param->msg_type==GF_NETIO_DATA_TRANSFERED) {
		const char *szCache = gf_dm_sess_get_cache_name(jsdnload->sess);
		if (!vrml_js_load_script(script, (char *) szCache))
			e = GF_SCRIPT_ERROR;
		else
			e = GF_OK;
	} else if (!e) return;

	/*destroy current download session (ie, destroy ourselves)*/
	gf_dm_sess_del(jsdnload->sess);
	free(jsdnload);

	if (e) {
		u32 i;
		if (script->url.count<=1) {
			GF_JSAPIParam par;
			par.info.e = e;
			par.info.msg = "Cannot fetch script";
			ScriptAction(NULL, script->sgprivate->scenegraph, GF_JSAPI_OP_MESSAGE, NULL, &par);
			return;
		}
		free(script->url.vals[0].script_text);
		for (i=0; i<script->url.count-1; i++) 
			script->url.vals[i].script_text = script->url.vals[i+1].script_text;
		script->url.vals[script->url.count-1].script_text = NULL;
		script->url.count -= 1;
		JSScriptFromFile((GF_Node *)script);
	}
}

void JSScriptFromFile(GF_Node *node)
{
	u32 i;
	GF_Err e;
	char szExt[50], *ext;
	M_Script *script = (M_Script *)node;
	Bool can_dnload = 0;

	for (i=0; i<script->url.count; i++) {
		ext = strrchr(script->url.vals[i].script_text, '.');
		if (!ext) break;
		strcpy(szExt, ext);
		strlwr(szExt);
		if (strcmp(szExt, ".js")) continue;
		can_dnload = 1;
		break;
	}
	if (!can_dnload) return;

	e = GF_SCRIPT_ERROR;

	if (!vrml_js_load_script(script, script->url.vals[0].script_text)) {
		GF_JSAPIParam par;
		char *url;
		GF_Err e;
		JSFileDownload *jsdnload;

		ScriptAction(NULL, node->sgprivate->scenegraph, GF_JSAPI_OP_GET_SCENE_URI, node, &par);
		url = NULL;
		if (par.uri.url) url = gf_url_concatenate(par.uri.url, script->url.vals[0].script_text);
		if (!url) url = strdup(script->url.vals[0].script_text);

		par.dnld_man = NULL;
		ScriptAction(NULL, node->sgprivate->scenegraph, GF_JSAPI_OP_GET_DOWNLOAD_MANAGER, NULL, &par);
		if (!par.dnld_man) return;

		if (!strstr(url, "://") || !strnicmp(url, "file://", 7)) {
			free(url);

retry:
			if (script->url.count<=1) {
				GF_JSAPIParam par;
				par.info.e = e;
				par.info.msg = "Cannot fetch script";
				ScriptAction(NULL, script->sgprivate->scenegraph, GF_JSAPI_OP_GET_DCCI, NULL, &par);
				return;
			}
			free(script->url.vals[0].script_text);
			for (i=0; i<script->url.count-1; i++) 
				script->url.vals[i].script_text = script->url.vals[i+1].script_text;
			script->url.vals[script->url.count-1].script_text = NULL;
			script->url.count -= 1;
			JSScriptFromFile((GF_Node *)script);
		} else {
			GF_SAFEALLOC(jsdnload, JSFileDownload);
			jsdnload->node = node;
			jsdnload->sess = gf_dm_sess_new(par.dnld_man, script->url.vals[0].script_text, 0, JS_NetIO, jsdnload, &e);
			free(url);
			if (!jsdnload->sess) {
				free(jsdnload);
				goto retry;
			}
		}
	}
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

	priv->js_ctx = gf_sg_ecmascript_new(node->sgprivate->scenegraph);
	if (!priv->js_ctx) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[VRML JS] Cannot allocate ECMAScript context for node\n"));
		return;
	}

	JS_SetContextPrivate(priv->js_ctx, node);
	gf_sg_script_init_sm_api(priv, node);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[VRML JS] Built-in classes initialized\n"));
#ifndef GPAC_DISABLE_SVG
	/*initialize DOM*/
	dom_js_load(node->sgprivate->scenegraph, priv->js_ctx, priv->js_obj);
	/*create event object, and remember it*/
	priv->event = dom_js_define_event(priv->js_ctx, priv->js_obj);
#endif

	gf_sg_load_script_extensions(node->sgprivate->scenegraph, priv->js_ctx, priv->js_obj, 0);

	priv->js_cache = gf_list_new();

	/*setup fields interfaces*/
	JS_InitScriptFields(priv, node);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[VRML JS] script fields initialized\n"));

	priv->JS_PreDestroy = JS_PreDestroy;
	priv->JS_EventIn = JS_EventIn;

	if (!local_script) {
		JSScriptFromFile(node);
		return;
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[VRML JS] Evaluating script %s\n", str));

	ret = JS_EvaluateScript(priv->js_ctx, priv->js_obj, str, strlen(str), 0, 0, &rval);
	if (ret==JS_FALSE) {
		return;
	}

	/*call initialize if present*/
	if (! JS_LookupProperty(priv->js_ctx, priv->js_obj, "initialize", &fval)) return;
	if (JSVAL_IS_VOID(fval)) return;
	JS_CallFunctionValue(priv->js_ctx, priv->js_obj, fval, 0, NULL, &rval);

	flush_event_out(node, priv);
	/*perform JS cleanup*/
	JS_MaybeGC(priv->js_ctx);
}

static void JSScript_Load(GF_Node *node)
{
	switch (node->sgprivate->tag) {
	case TAG_MPEG4_Script:
	case TAG_X3D_Script:
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


#ifdef __SYMBIAN32__
const long MAX_HEAP_BYTES = 256 * 1024L;
#else
const long MAX_HEAP_BYTES = 4*1024 * 1024L;
#endif
const long STACK_CHUNK_BYTES = 8*1024L;

JSContext *gf_sg_ecmascript_new(GF_SceneGraph *sg)
{
	if (!js_rt) {
		JSRuntime *js_runtime = JS_NewRuntime(MAX_HEAP_BYTES);
		if (!js_runtime) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[ECMAScript] Cannot allocate ECMAScript runtime\n"));
			return NULL;
		}
		GF_SAFEALLOC(js_rt, GF_JSRuntime);
		js_rt->js_runtime = js_runtime;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[ECMAScript] ECMAScript runtime allocated 0x%08x\n", js_runtime));
		gf_sg_load_script_modules(sg);
	}
	js_rt->nb_inst++;
	return JS_NewContext(js_rt->js_runtime, STACK_CHUNK_BYTES);
}

void gf_sg_ecmascript_del(JSContext *ctx)
{
	JS_SetGlobalObject(ctx, NULL);
	JS_DestroyContext(ctx);
	if (js_rt) {
		js_rt->nb_inst --;
		if (js_rt->nb_inst == 0) {
			JS_DestroyRuntime(js_rt->js_runtime);
			JS_ShutDown();
			gf_sg_unload_script_modules();
			free(js_rt);
			js_rt = NULL;
		}
	}
}

static void JSScript_NodeModified(GF_SceneGraph *sg, GF_Node *node, GF_FieldInfo *info)
{
	JSObject *obj = NULL;
	GF_ScriptPriv *priv;
	u32 i, count, j;

	count = gf_list_count(sg->scripts);
	for (i=0; i<count; i++) {
		GF_Node *p = gf_list_get(sg->scripts, i);
		if ((p->sgprivate->tag!=TAG_MPEG4_Script) && (p->sgprivate->tag!=TAG_X3D_Script))
			continue;

		priv = (GF_ScriptPriv *)gf_node_get_private(p);
		if (!priv->js_cache) continue;

		j=0;
		while ((obj = gf_list_enum(priv->js_cache, &j))) {
			GF_JSField *jsf = (GF_JSField *) JS_GetPrivate(priv->js_ctx, obj);
			if (jsf 
				&& (jsf->owner==node) 
				&& (jsf->field.fieldIndex == info->fieldIndex)
				&& (jsf->field.fieldType == info->fieldType)) {
				jsf->reevaluate = 1;
			}
		}
	}
}

void gf_sg_handle_dom_event_for_vrml(GF_Node *node, GF_DOM_Event *event, GF_Node *observer)
{
	GF_ScriptPriv *priv;
	Bool prev_type;
	JSBool ret = JS_FALSE;
	GF_DOM_Event *prev_event = NULL;
	SVG_handlerElement *hdl;
	jsval rval;

	hdl = (SVG_handlerElement *) node;
	if (!hdl->js_fun_val && !hdl->evt_listen_obj) return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_INTERACT, ("[DOM Events] Executing script code from VRML handler\n"));

	priv = JS_GetScriptStack(hdl->js_context);
	prev_event = JS_GetPrivate(priv->js_ctx, priv->event);
	/*break loops*/
	if (prev_event && (prev_event->type==event->type) && (prev_event->target==event->target)) 
		return;

	prev_type = event->is_vrml;
	event->is_vrml = 1;
	JS_SetPrivate(priv->js_ctx, priv->event, event);


	{
		JSObject *evt;
		jsval argv[1];
		evt = gf_dom_new_event(priv->js_ctx);
		JS_SetPrivate(priv->js_ctx, evt, event);
		argv[0] = OBJECT_TO_JSVAL(evt);
		
		if (hdl->js_fun_val) {
			jsval funval = (JSWord) hdl->js_fun_val;
			ret = JS_CallFunctionValue(priv->js_ctx, hdl->evt_listen_obj ? (JSObject *)hdl->evt_listen_obj: priv->js_obj, funval, 1, argv, &rval);
		} else {
			ret = JS_CallFunctionName(priv->js_ctx, hdl->evt_listen_obj, "handleEvent", 1, argv, &rval);
		}
	}
	
	event->is_vrml = prev_type;
	JS_SetPrivate(priv->js_ctx, priv->event, prev_event);

}

#endif


/*set JavaScript interface*/
void gf_sg_set_script_action(GF_SceneGraph *scene, gf_sg_script_action script_act, void *cbk)
{
	if (!scene) return;
	scene->script_action = script_act;
	scene->script_action_cbck = cbk;

#ifdef GPAC_HAS_SPIDERMONKEY
	scene->script_load = JSScript_Load;
	scene->on_node_modified = JSScript_NodeModified;
#endif

}

