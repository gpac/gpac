/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean le Feuvre
 *			Copyright (c) 2007-200X ENST
 *			All rights reserved
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

/*base SVG type*/
#include <gpac/nodes_svg_da.h>
/*dom events*/
#include <gpac/events.h>

#include <gpac/download.h>
#include <gpac/network.h>
#include <gpac/xml.h>


#ifdef GPAC_HAS_SPIDERMONKEY

#include <gpac/internal/terminal_dev.h>
#include <gpac/internal/compositor_dev.h>

#include <jsapi.h> 

typedef struct
{
	u32 nb_inst;
	JSClass gpacClass;
} GF_GPACRuntime;

static GF_GPACRuntime *gpac_rt = NULL;


#define _SETUP_CLASS(the_class, cname, flag, getp, setp, fin)	\
	memset(&the_class, 0, sizeof(the_class));	\
	the_class.name = cname;	\
	the_class.flags = flag;	\
	the_class.addProperty = JS_PropertyStub;	\
	the_class.delProperty = JS_PropertyStub;	\
	the_class.getProperty = getp;	\
	the_class.setProperty = setp;	\
	the_class.enumerate = JS_EnumerateStub;	\
	the_class.resolve = JS_ResolveStub;		\
	the_class.convert = JS_ConvertStub;		\
	the_class.finalize = fin;


static JSBool gpac_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	const char *res;
	char *prop_name;
	GF_Terminal *term = (GF_Terminal *)JS_GetPrivate(c, obj);
	if (!term) return JS_FALSE;

	if (!JSVAL_IS_STRING(id)) return JS_TRUE;
	prop_name = JS_GetStringBytes(JSVAL_TO_STRING(id));
	if (!prop_name) return JS_FALSE;

	if (!strcmp(prop_name, "last_working_directory")) {
		res = gf_cfg_get_key(term->user->config, "General", "LastWorkingDir");
		if (!res) res = "";
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(c, res)); 
		return JS_TRUE;
	}
	if (!strcmp(prop_name, "scale_x")) {
		*vp = DOUBLE_TO_JSVAL( JS_NewDouble(c, FIX2FLT(term->compositor->zoom) * FIX2FLT(term->compositor->scale_x)) );
		return JS_TRUE;
	}
	if (!strcmp(prop_name, "scale_y")) {
		*vp = DOUBLE_TO_JSVAL( JS_NewDouble(c, FIX2FLT(term->compositor->zoom) * FIX2FLT(term->compositor->scale_y)) );
		return JS_TRUE;
	}
	if (!strcmp(prop_name, "translation_x")) {
		*vp = DOUBLE_TO_JSVAL( JS_NewDouble(c, FIX2FLT(term->compositor->trans_x)) );
		return JS_TRUE;
	}
	if (!strcmp(prop_name, "translation_y")) {
		*vp = DOUBLE_TO_JSVAL( JS_NewDouble(c, FIX2FLT(term->compositor->trans_y)) );
		return JS_TRUE;
	}

	return JS_TRUE;
}
static JSBool gpac_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	char *prop_name, *prop_val;
	GF_Terminal *term = (GF_Terminal *)JS_GetPrivate(c, obj);
	if (!term) return JS_FALSE;

	if (!JSVAL_IS_STRING(id)) return JS_TRUE;
	prop_name = JS_GetStringBytes(JSVAL_TO_STRING(id));

	if (!strcmp(prop_name, "last_working_directory")) {
		if (!JSVAL_IS_STRING(*vp)) return JS_FALSE;
		prop_val = JS_GetStringBytes(JSVAL_TO_STRING(*vp));
		gf_cfg_set_key(term->user->config, "General", "LastWorkingDir", prop_val);
		return JS_TRUE;
	}

	return JS_TRUE;
}

static JSBool gpac_getOption(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	const char *opt;
	JSString *js_sec_name, *js_key_name, *s;
	GF_Terminal *term = (GF_Terminal *)JS_GetPrivate(c, obj);
	if (!term) return JS_FALSE;

	if (argc < 2) return JSVAL_FALSE;
	
	if (!JSVAL_IS_STRING(argv[0])) return JSVAL_FALSE;
	js_sec_name = JSVAL_TO_STRING(argv[0]);

	if (!JSVAL_IS_STRING(argv[1])) return JSVAL_FALSE;
	js_key_name = JSVAL_TO_STRING(argv[1]);

	opt = gf_cfg_get_key(term->user->config, JS_GetStringBytes(js_sec_name), JS_GetStringBytes(js_key_name));
	s = JS_NewStringCopyZ(c, opt ? opt : "");
	if (!s) return JS_FALSE;
	*rval = STRING_TO_JSVAL(s); 
	return JS_TRUE; 
}

static JSBool gpac_setOption(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSString *js_sec_name, *js_key_name, *js_key_val;
	GF_Terminal *term = (GF_Terminal *)JS_GetPrivate(c, obj);
	if (!term) return JS_FALSE;
	if (argc < 3) return JSVAL_FALSE;
	
	if (!JSVAL_IS_STRING(argv[0])) return JSVAL_FALSE;
	js_sec_name = JSVAL_TO_STRING(argv[0]);

	if (!JSVAL_IS_STRING(argv[1])) return JSVAL_FALSE;
	js_key_name = JSVAL_TO_STRING(argv[1]);

	if (!JSVAL_IS_STRING(argv[2])) return JSVAL_FALSE;
	js_key_val = JSVAL_TO_STRING(argv[2]);

	gf_cfg_set_key(term->user->config, JS_GetStringBytes(js_sec_name), JS_GetStringBytes(js_key_name), JS_GetStringBytes(js_key_val));

	return JSVAL_TRUE;
}

typedef struct 
{
	JSContext *c;
	JSObject *array;
	Bool is_dir;
} enum_dir_cbk;

static Bool enum_dir_fct(void *cbck, char *file_name, char *file_path)
{
	char *sep;
	JSString *s;
	jsuint idx;
	jsval v;
	JSObject *obj;
	enum_dir_cbk *cbk = (enum_dir_cbk*)cbck;

	obj = JS_NewObject(cbk->c, 0, 0, 0);
	s = JS_NewStringCopyZ(cbk->c, file_name);
	JS_DefineProperty(cbk->c, obj, "name", STRING_TO_JSVAL(s), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	sep = strrchr(file_path, '\\');
	if (!sep) sep = strrchr(file_path, '/');
	if (sep) sep[1] = 0;
	s = JS_NewStringCopyZ(cbk->c, file_path);
	if (sep) sep[1] = '/';
	JS_DefineProperty(cbk->c, obj, "path", STRING_TO_JSVAL(s), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(cbk->c, obj, "directory", BOOLEAN_TO_JSVAL(cbk->is_dir ? JS_TRUE : JS_FALSE), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);

	JS_GetArrayLength(cbk->c, cbk->array, &idx);
	v = OBJECT_TO_JSVAL(obj);
	JS_SetElement(cbk->c, cbk->array, idx, &v);
	return 0;
}

static JSBool gpac_enum_directory(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	enum_dir_cbk cbk;
	char *url = NULL;
	char *dir = "D:\\";
	if ((argc >= 1) && JSVAL_IS_STRING(argv[0])) {
		JSString *js_dir = JSVAL_TO_STRING(argv[0]);
		dir = JS_GetStringBytes(js_dir);
	}
	if ((argc >= 2) && JSVAL_IS_BOOLEAN(argv[1])) {
		if (JSVAL_TO_BOOLEAN(argv[1])==JS_TRUE)
			url = gf_url_concatenate(dir, "..");
	}
	cbk.c = c;
	cbk.array = JS_NewArrayObject(c, 0, 0);

	cbk.is_dir = 1;
	gf_enum_directory(url ? url : dir, 1, enum_dir_fct, &cbk, NULL);
	cbk.is_dir = 0;
	gf_enum_directory(url ? url : dir, 0, enum_dir_fct, &cbk, NULL);
	*rval = OBJECT_TO_JSVAL(cbk.array);
	if (url) free(url);
	return JS_TRUE;
}

static JSBool gpac_set_size(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	Bool override_size_info = 0;
	u32 w, h;
	jsdouble d;
	char *url = NULL;
	GF_Terminal *term = (GF_Terminal *)JS_GetPrivate(c, obj);
	if (!term) return JS_FALSE;

	w = h = 0;
	if ((argc >= 1) && JSVAL_IS_NUMBER(argv[0])) {
		JS_ValueToNumber(c, argv[0], &d);
		w = (u32) d;
	}
	if ((argc >= 2) && JSVAL_IS_NUMBER(argv[1])) {
		JS_ValueToNumber(c, argv[1], &d);
		h = (u32) d;
	}
	if ((argc >= 3) && JSVAL_IS_BOOLEAN(argv[2]) && (JSVAL_TO_BOOLEAN(argv[2])==JS_TRUE) ) 
		override_size_info = 1;

	if (w && h) {
		if (override_size_info) {
			term->compositor->scene_width = w;
			term->compositor->scene_height = h;
			term->compositor->has_size_info = 1;
		}
		gf_term_set_size(term, w, h);
	}

	return JS_TRUE;
}

JSObject *gpac_js_load(GF_SceneGraph *scene, JSContext *c, JSObject *global)
{
	JSObject *obj;
	if (!scene) return NULL;

	if (!gpac_rt) {
		GF_SAFEALLOC(gpac_rt, GF_GPACRuntime);
		_SETUP_CLASS(gpac_rt->gpacClass, "Node", JSCLASS_HAS_PRIVATE, gpac_getProperty, gpac_setProperty, JS_FinalizeStub);
	}
	gpac_rt->nb_inst++;

	{
		GF_JSAPIParam par;
		JSPropertySpec gpacClassProps[] = {
			{0}
		};
		JSFunctionSpec gpacClassFuncs[] = {
			{"getOption",		gpac_getOption, 3},
			{"setOption",		gpac_setOption, 4},
			{"enum_directory",	gpac_enum_directory, 1},
			{"set_size",		gpac_set_size, 1},
			{0}
		};
		JS_InitClass(c, global, 0, &gpac_rt->gpacClass, 0, 0, gpacClassProps, gpacClassFuncs, 0, 0);
		obj = JS_DefineObject(c, global, "gpac", &gpac_rt->gpacClass, 0, 0);

	
		ScriptAction(c, NULL, GF_JSAPI_OP_GET_TERM, scene->RootNode, &par);
		JS_SetPrivate(c, obj, par.term);
	}
	return obj;
}


void gpac_js_unload(JSObject *obj)
{
	if (!gpac_rt || !obj) return;
	gpac_rt->nb_inst--;
	if (!gpac_rt->nb_inst) {
		free(gpac_rt);
		gpac_rt = NULL;
	}
}

#endif

