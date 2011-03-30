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

/*base SVG type*/
#include <gpac/nodes_svg.h>
#include <gpac/nodes_mpeg4.h>
#include <gpac/nodes_x3d.h>
/*dom events*/
#include <gpac/events.h>

#include <gpac/download.h>
#include <gpac/network.h>
#include <gpac/options.h>
#include <gpac/xml.h>


#ifdef GPAC_HAS_SPIDERMONKEY


#if !defined(__GNUC__)
# if defined(_WIN32_WCE)
#  pragma comment(lib, "js")
# elif defined (WIN32)
#  pragma comment(lib, "js32")
# endif
#endif

#include <gpac/internal/scenegraph_dev.h>
#include <jsapi.h> 

#include <gpac/modules/js_usr.h>
#include <gpac/internal/terminal_dev.h>
#include <gpac/internal/compositor_dev.h>

typedef struct
{
	u32 nb_loaded;
	GF_Terminal *term;

	JSClass gpacClass;
	JSClass gpacEvtClass;
	JSClass anyClass;

	jsval evt_fun;
	GF_TermEventFilter evt_filter;
	GF_Event *evt;

	JSContext *c;
	JSObject *obj, *evt_obj;
} GF_GPACJSExt;


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


static GF_Terminal *gpac_get_term(JSContext *c, JSObject *obj)
{
	GF_GPACJSExt *ext = (GF_GPACJSExt *)JS_GetPrivate(c, obj);
	return ext ? ext->term : NULL;
}

static JSBool gpac_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	const char *res;
	char *prop_name;
	GF_Terminal *term = gpac_get_term(c, obj);
	if (!term) return JS_FALSE;

	if (!JSVAL_IS_STRING(id)) return JS_TRUE;
	prop_name = JS_GetStringBytes(JSVAL_TO_STRING(id));
	if (!prop_name) return JS_FALSE;

	if (!strcmp(prop_name, "last_working_directory")) {
		res = gf_cfg_get_key(term->user->config, "General", "LastWorkingDir");
		if (!res) res = gf_cfg_get_key(term->user->config, "General", "ModulesDirectory");
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(c, res)); 
		return JS_TRUE;
	}
	if (!strcmp(prop_name, "scale_x")) {
		*vp = DOUBLE_TO_JSVAL( JS_NewDouble(c, FIX2FLT(term->compositor->scale_x)) );
		return JS_TRUE;
	}
	if (!strcmp(prop_name, "scale_y")) {
		*vp = DOUBLE_TO_JSVAL( JS_NewDouble(c, FIX2FLT(term->compositor->scale_y)) );
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
	if (!strcmp(prop_name, "rectangular_textures")) {
		Bool any_size = 0;
#ifndef GPAC_DISABLE_3D
		if (term->compositor->gl_caps.npot_texture || term->compositor->gl_caps.rect_texture)
			any_size = 1;
#endif
		*vp = BOOLEAN_TO_JSVAL( any_size ? JS_TRUE : JS_FALSE );
		return JS_TRUE;
	}
	if (!strcmp(prop_name, "batteryOn")) {
		Bool on_battery = 0;
		gf_sys_get_battery_state(&on_battery, NULL, NULL, NULL, NULL);
		*vp = BOOLEAN_TO_JSVAL( on_battery ? JS_TRUE : JS_FALSE );
		return JS_TRUE;
	}
	if (!strcmp(prop_name, "batteryCharging")) {
		Bool on_charge = 0;
		gf_sys_get_battery_state(NULL, &on_charge, NULL, NULL, NULL);
		*vp = BOOLEAN_TO_JSVAL( on_charge ? JS_TRUE : JS_FALSE );
		return JS_TRUE;
	}
	if (!strcmp(prop_name, "batteryPercent")) {
		u32 level = 0;
		gf_sys_get_battery_state(NULL, NULL, &level, NULL, NULL);
		*vp = INT_TO_JSVAL( level );
		return JS_TRUE;
	}
	if (!strcmp(prop_name, "batteryLifeTime")) {
		u32 level = 0;
		gf_sys_get_battery_state(NULL, NULL, NULL, &level, NULL);
		*vp = INT_TO_JSVAL( level );
		return JS_TRUE;
	}
	if (!strcmp(prop_name, "batteryFullLifeTime")) {
		u32 level = 0;
		gf_sys_get_battery_state(NULL, NULL, NULL, NULL, &level);
		*vp = INT_TO_JSVAL( level );
		return JS_TRUE;
	}
	if (!strcmp(prop_name, "hostname")) {
		char hostname[100];
		gf_sk_get_host_name((char*)hostname);
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(c, hostname)); 
		return JS_TRUE;
	}
	if (!strcmp(prop_name, "fullscreen")) {
		*vp = BOOLEAN_TO_JSVAL( term->compositor->fullscreen ? JS_TRUE : JS_FALSE); 
		return JS_TRUE;
	}
	if (!strcmp(prop_name, "current_path")) {
		char *url = gf_url_concatenate(term->root_scene->root_od->net_service->url, "");
		if (!url) url = gf_strdup("");
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(c, url)); 
		gf_free(url);
		return JS_TRUE;
	}
	if (!strcmp(prop_name, "volume")) {
		*vp = INT_TO_JSVAL( gf_term_get_option(term, GF_OPT_AUDIO_VOLUME));
		return JS_TRUE;
	}
	if (!strcmp(prop_name, "navigation")) {
		*vp = INT_TO_JSVAL( gf_term_get_option(term, GF_OPT_NAVIGATION));
		return JS_TRUE;
	}
	if (!strcmp(prop_name, "navigation_type")) {
		*vp = INT_TO_JSVAL( gf_term_get_option(term, GF_OPT_NAVIGATION_TYPE) );
		return JS_TRUE;
	}
	return JS_TRUE;
}
static JSBool gpac_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	char *prop_name, *prop_val;
	GF_Terminal *term = gpac_get_term(c, obj);
	if (!term) return JS_FALSE;

	if (!JSVAL_IS_STRING(id)) return JS_TRUE;
	prop_name = JS_GetStringBytes(JSVAL_TO_STRING(id));

	if (!strcmp(prop_name, "last_working_directory")) {
		if (!JSVAL_IS_STRING(*vp)) return JS_FALSE;
		prop_val = JS_GetStringBytes(JSVAL_TO_STRING(*vp));
		gf_cfg_set_key(term->user->config, "General", "LastWorkingDir", prop_val);
		return JS_TRUE;
	}
	if (!strcmp(prop_name, "caption")) {
		GF_Event evt;
		if (!JSVAL_IS_STRING(*vp)) return JS_FALSE;
		evt.type = GF_EVENT_SET_CAPTION;
		evt.caption.caption = JS_GetStringBytes(JSVAL_TO_STRING(*vp));
		gf_term_user_event(term, &evt);
		return JS_TRUE;
	}
	if (!strcmp(prop_name, "fullscreen")) {
		Bool res = (JSVAL_TO_BOOLEAN(*vp)==JS_TRUE) ? 1 : 0;
		if (term->compositor->fullscreen != res) {
			gf_term_set_option(term, GF_OPT_FULLSCREEN, res);
		}
		return JS_TRUE;
	}
	if (!strcmp(prop_name, "volume")) {
		if (JSVAL_IS_NUMBER(*vp)) {
			jsdouble d;
			JS_ValueToNumber(c, *vp, &d);
			gf_term_set_option(term, GF_OPT_AUDIO_VOLUME, (u32) d);
		} else if (JSVAL_IS_INT(*vp)) {
			gf_term_set_option(term, GF_OPT_AUDIO_VOLUME, JSVAL_TO_INT(*vp));
		}
		return JS_TRUE;
	}
	if (!strcmp(prop_name, "navigation")) {
		gf_term_set_option(term, GF_OPT_NAVIGATION, JSVAL_TO_INT(*vp) );
		return JS_TRUE;
	}
	if (!strcmp(prop_name, "navigation_type")) {
		gf_term_set_option(term, GF_OPT_NAVIGATION_TYPE, 0);
		return JS_TRUE;
	}

	return JS_TRUE;
}

static JSBool gpac_getOption(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	const char *opt, *sec_name;
	JSString *js_sec_name, *js_key_name, *s;
	GF_Terminal *term = gpac_get_term(c, obj);
	if (!term) return JS_FALSE;

	if (argc < 2) return JSVAL_FALSE;
	
	if (!JSVAL_IS_STRING(argv[0])) return JSVAL_FALSE;
	js_sec_name = JSVAL_TO_STRING(argv[0]);

	if (!JSVAL_IS_STRING(argv[1])) return JSVAL_FALSE;
	js_key_name = JSVAL_TO_STRING(argv[1]);

	sec_name = JS_GetStringBytes(js_sec_name);
	if (!stricmp(sec_name, "audiofilters")) {
		if (!term->compositor->audio_renderer->filter_chain.enable_filters) return JS_TRUE;
		if (!term->compositor->audio_renderer->filter_chain.filters->filter->GetOption) return JS_TRUE;
		opt = term->compositor->audio_renderer->filter_chain.filters->filter->GetOption(term->compositor->audio_renderer->filter_chain.filters->filter, JS_GetStringBytes(js_key_name));
	} else {
		opt = gf_cfg_get_key(term->user->config, sec_name, JS_GetStringBytes(js_key_name));
	}
	s = JS_NewStringCopyZ(c, opt ? opt : "");
	if (!s) return JS_FALSE;
	*rval = STRING_TO_JSVAL(s); 
	return JS_TRUE; 
}

static JSBool gpac_setOption(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *js_sec_name, *js_key_name, *js_key_val;
	GF_Terminal *term = gpac_get_term(c, obj);
	if (!term) return JS_FALSE;
	if (argc < 3) return JSVAL_FALSE;
	
	if (!JSVAL_IS_STRING(argv[0])) return JSVAL_FALSE;
	js_sec_name = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));


	if (!JSVAL_IS_STRING(argv[1])) return JSVAL_FALSE;
	js_key_name = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));


	if (!JSVAL_IS_STRING(argv[2])) return JSVAL_FALSE;
	js_key_val = JS_GetStringBytes(JSVAL_TO_STRING(argv[2]));

	if (!stricmp(js_sec_name, "audiofilters")) {
		if (!term->compositor->audio_renderer->filter_chain.enable_filters) return JS_TRUE;
		if (!term->compositor->audio_renderer->filter_chain.filters->filter->SetOption) return JS_TRUE;
		term->compositor->audio_renderer->filter_chain.filters->filter->SetOption(term->compositor->audio_renderer->filter_chain.filters->filter, js_key_name, js_key_val);
	} else {
		gf_cfg_set_key(term->user->config, js_sec_name, js_key_name, js_key_val);
		if (!strcmp(js_sec_name, "Audio") && !strcmp(js_key_name, "Filter")) {
			gf_sc_reload_audio_filters(term->compositor);
		}
	}
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
	u32 i, len;
	char *sep;
	JSString *s;
	jsuint idx;
	jsval v;
	JSObject *obj;
	enum_dir_cbk *cbk = (enum_dir_cbk*)cbck;

	obj = JS_NewObject(cbk->c, 0, 0, 0);
	s = JS_NewStringCopyZ(cbk->c, file_name);
	JS_DefineProperty(cbk->c, obj, "name", STRING_TO_JSVAL(s), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);

	sep=NULL;
	len = strlen(file_path);
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
		s = JS_NewStringCopyZ(cbk->c, file_path);
	} else {
		s = JS_NewStringCopyZ(cbk->c, file_path);
	}
	JS_DefineProperty(cbk->c, obj, "path", STRING_TO_JSVAL(s), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(cbk->c, obj, "directory", BOOLEAN_TO_JSVAL(cbk->is_dir ? JS_TRUE : JS_FALSE), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);

	JS_GetArrayLength(cbk->c, cbk->array, &idx);
	v = OBJECT_TO_JSVAL(obj);
	JS_SetElement(cbk->c, cbk->array, idx, &v);
	return 0;
}

static JSBool gpac_enum_directory(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Err err;
	enum_dir_cbk cbk;
	char *url = NULL;
	char *dir = "D:";
	char *filter = NULL;
	Bool dir_only = 0;
	Bool browse_root = 0;

	if ((argc >= 1) && JSVAL_IS_STRING(argv[0])) {
		JSString *js_dir = JSVAL_TO_STRING(argv[0]);
		dir = JS_GetStringBytes(js_dir);
		if (!strcmp(dir, "/")) browse_root = 1;
	}
	if ((argc >= 2) && JSVAL_IS_STRING(argv[1])) {
		JSString *js_fil = JSVAL_TO_STRING(argv[1]);
		filter = JS_GetStringBytes(js_fil);
		if (!strcmp(filter, "dir")) {
			dir_only = 1;
			filter = NULL;
		} else if (!strlen(filter)) filter=NULL;
	}
	if ((argc >= 3) && JSVAL_IS_BOOLEAN(argv[2])) {
		if (JSVAL_TO_BOOLEAN(argv[2])==JS_TRUE) {
			url = gf_url_concatenate(dir, "..");
			if (!strcmp(url, "..") || (url[0]==0)) {
				if ((dir[1]==':') && ((dir[2]=='/') || (dir[2]=='\\')) ) browse_root = 1;
				else if (!strcmp(dir, "/")) browse_root = 1;
			}
		}
	}

	if (!strlen(dir) && (!url || !strlen(url))) browse_root = 1;

	if (browse_root) {
		cbk.c = c;
		cbk.array = JS_NewArrayObject(c, 0, 0);
		cbk.is_dir = 1;
		gf_enum_directory("/", 1, enum_dir_fct, &cbk, NULL);
		*rval = OBJECT_TO_JSVAL(cbk.array);
		if (url) gf_free(url);
		return JS_TRUE;
	}

	cbk.c = c;
	cbk.array = JS_NewArrayObject(c, 0, 0);

	cbk.is_dir = 1;
	err = gf_enum_directory(url ? url : dir, 1, enum_dir_fct, &cbk, NULL);
	if (err==GF_IO_ERR) {
		GF_Terminal *term = gpac_get_term(c, obj);
		/*try to concatenate with service url*/
		char *an_url = gf_url_concatenate(term->root_scene->root_od->net_service->url, url ? url : dir);
		gf_free(url);
		url = an_url;
		gf_enum_directory(url ? url : dir, 1, enum_dir_fct, &cbk, NULL);
	}

	if (!dir_only) {
		cbk.is_dir = 0;
		err = gf_enum_directory(url ? url : dir, 0, enum_dir_fct, &cbk, filter);
		if (dir_only && (err==GF_IO_ERR)) {
			GF_Terminal *term = gpac_get_term(c, obj);
			/*try to concatenate with service url*/
			char *an_url = gf_url_concatenate(term->root_scene->root_od->net_service->url, url ? url : dir);
			gf_free(url);
			url = an_url;
			gf_enum_directory(url ? url : dir, 0, enum_dir_fct, &cbk, filter);
		}
	}

	*rval = OBJECT_TO_JSVAL(cbk.array);
	if (url) gf_free(url);
	return JS_TRUE;
}

static JSBool gpac_set_size(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	Bool override_size_info = 0;
	u32 w, h;
	jsdouble d;
	GF_Terminal *term = gpac_get_term(c, obj);
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
		GF_Event evt;
		if (override_size_info) {
			term->compositor->scene_width = w;
			term->compositor->scene_height = h;
			term->compositor->has_size_info = 1;
		}
		if (term->user->os_window_handler) {
			evt.type = GF_EVENT_SCENE_SIZE;
			evt.size.width = w;
			evt.size.height = h;
			gf_term_send_event(term, &evt);
		} else {
			gf_sc_set_size(term->compositor, w, h);
		}
	} else if (override_size_info) {
		term->compositor->has_size_info = 0;
		term->compositor->recompute_ar = 1;
	}

	return JS_TRUE;
}

static JSBool gpac_get_horizontal_dpi(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Terminal *term = gpac_get_term(c, obj);
	if (term) *rval = INT_TO_JSVAL(term->compositor->video_out->dpi_x);
	return JS_TRUE;
}

static JSBool gpac_get_vertical_dpi(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Terminal *term = gpac_get_term(c, obj);
	if (term) *rval = INT_TO_JSVAL(term->compositor->video_out->dpi_y);	
	return JS_TRUE;
}

static JSBool gpac_get_screen_width(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Terminal *term = gpac_get_term(c, obj);
	*rval = INT_TO_JSVAL(term->compositor->video_out->max_screen_width);
	return JS_TRUE;
}

static JSBool gpac_get_screen_height(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Terminal *term = gpac_get_term(c, obj);
	*rval = INT_TO_JSVAL(term->compositor->video_out->max_screen_height);
	return JS_TRUE;
}

static JSBool gpac_exit(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Event evt;
	GF_Terminal *term = gpac_get_term(c, obj);
	evt.type = GF_EVENT_QUIT;
	gf_term_send_event(term, &evt);
	return JS_TRUE;
}

static JSBool gpac_set_3d(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	u32 type_3d = 0;
	GF_Terminal *term = gpac_get_term(c, obj);
	if (argc && JSVAL_IS_INT(argv[0])) type_3d = JSVAL_TO_INT(argv[0]);
	if (term->compositor->inherit_type_3d != type_3d) {
		term->compositor->inherit_type_3d = type_3d;
		term->compositor->root_visual_setup = 0;
		term->compositor->reset_graphics = 1;
	}
	return JS_TRUE;
}

static JSBool gpac_move_window(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Event evt;
	GF_Terminal *term = gpac_get_term(c, obj);
	if (argc < 2) return JS_TRUE;
	if (!JSVAL_IS_INT(argv[0]) || !JSVAL_IS_INT(argv[1])) return JS_TRUE;
	
	evt.type = GF_EVENT_MOVE;
	evt.move.relative = 1;
	evt.move.x = JSVAL_TO_INT(argv[0]);
	evt.move.y = JSVAL_TO_INT(argv[1]);
	term->compositor->video_out->ProcessEvent(term->compositor->video_out, &evt);
	
	return JS_TRUE;
}


static JSBool gpac_error_string(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	const char *str;
	/*GF_Terminal *term = gpac_get_term(c, obj);*/
	if (argc < 1) return JS_TRUE;
	if (!JSVAL_IS_INT(argv[0]) ) return JS_TRUE;
	str = gf_error_to_string(JSVAL_TO_INT(argv[0]));
	
	*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(c, str));

	return JS_TRUE;
}

static JSBool gpac_get_scene_time(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_SceneGraph *sg = NULL;
	GF_Terminal *term = gpac_get_term(c, obj);
	if (!argc || !JSVAL_IS_OBJECT(argv[0])) {
		sg = term->root_scene->graph;
	} else {
		GF_Node *n = gf_sg_js_get_node(c, JSVAL_TO_OBJECT(argv[0]));
		sg = n ? n->sgprivate->scenegraph : term->root_scene->graph;
	}
	*rval = DOUBLE_TO_JSVAL( JS_NewDouble(c, sg->GetSceneTime(sg->userpriv) ) );

	return JS_TRUE;
}

static JSBool gpac_migrate_url(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *url;
	u32 i, count;
	GF_NetworkCommand com;
	GF_Terminal *term = gpac_get_term(c, obj);
	if (!argc || !JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	
	url = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	if (!url) return JS_FALSE;
	
	count = gf_list_count(term->root_scene->resources);
	for (i=0; i<count; i++) {
		GF_ObjectManager *odm = gf_list_get(term->root_scene->resources, i);
		if (!odm->net_service) continue;
		if (!strstr(url, odm->net_service->url)) continue;

		memset(&com, 0, sizeof(GF_NetworkCommand));
		com.base.command_type = GF_NET_SERVICE_MIGRATION_INFO;
		odm->net_service->ifce->ServiceCommand(odm->net_service->ifce, &com);
		if (com.migrate.data) {
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(c, com.migrate.data));
		} else {
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(c, odm->net_service->url));
		}
		break;		
	}
	return JS_TRUE;
}

static JSBool gpacevt_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	GF_GPACJSExt *gjs = JS_GetPrivate(c, obj);
	GF_Event *evt = gjs->evt;
	if (!evt) return 0;

	if (JSVAL_IS_INT(id)) {
		switch (JSVAL_TO_INT(id)) {
		case 0:
			*vp = INT_TO_JSVAL(evt->type);
			break;
		case 1:
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(c, gf_dom_get_key_name(evt->key.key_code) )); 
			break;
		case 2:
			*vp = INT_TO_JSVAL(evt->mouse.x); 
			break;
		case 3:
			*vp = INT_TO_JSVAL(evt->mouse.y); 
			break;
		case 4:
			if (gjs->term->compositor->hit_appear) *vp = BOOLEAN_TO_JSVAL(JS_TRUE);
			else if (gf_list_count(gjs->term->compositor->previous_sensors) ) *vp = BOOLEAN_TO_JSVAL(JS_TRUE);
			else if (gjs->term->compositor->text_selection) *vp = BOOLEAN_TO_JSVAL(JS_TRUE);
			else *vp = BOOLEAN_TO_JSVAL(JS_FALSE);
			break;
		case 5:
			*vp = DOUBLE_TO_JSVAL( JS_NewDouble(c, FIX2FLT(evt->mouse.wheel_pos)) );
			break;
		case 6:
			*vp = INT_TO_JSVAL( evt->mouse.button);
			break;
		}
	} else if (JSVAL_IS_STRING(id)) {
		char *name = JS_GetStringBytes(JSVAL_TO_STRING(id));
		if (!strcmp(name, "target_url")) {
			*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, evt->navigate.to_url) );
		}
	}

	return JS_TRUE;
}

static Bool gjs_event_filter(void *udta, GF_Event *evt, Bool consumed_by_compositor)
{
	Bool res;
	jsval argv[1], rval;
	GF_GPACJSExt *gjs = (GF_GPACJSExt *)udta;
	if (consumed_by_compositor) return 0;

	if (gjs->evt != NULL) return 0;
	gf_sg_lock_javascript(gjs->c, 1);
	
	rval = JSVAL_VOID;
	gjs->evt = evt;
	JS_SetPrivate(gjs->c, gjs->evt_obj, gjs);
	argv[0] = OBJECT_TO_JSVAL(gjs->evt_obj);
	JS_CallFunctionValue(gjs->c, gjs->obj, gjs->evt_fun, 1, argv, &rval);
	JS_SetPrivate(gjs->c, gjs->evt_obj, NULL);
	gjs->evt = NULL;

	if (rval==JSVAL_VOID) {
		res = 0;
	} else if (JSVAL_IS_BOOLEAN(rval) ) {
		res = (JSVAL_TO_BOOLEAN(rval)==JS_TRUE) ? 1 : 0;
	} else if (JSVAL_IS_INT(rval) ) {
		res = (JSVAL_TO_INT(rval)) ? 1 : 0;
	}

	gf_sg_lock_javascript(gjs->c, 0);
	return res;
}

static JSBool gpac_set_event_filter(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_GPACJSExt *gjs = (GF_GPACJSExt *)JS_GetPrivate(c, obj);
	if (!argc || !JSVAL_IS_OBJECT(argv[0])) return JS_FALSE;
	if (gjs->evt_fun) return JS_TRUE;

	gjs->evt_fun = argv[0];
	gjs->obj = obj;
	gjs->c = c;
	gjs->evt_filter.udta = gjs;
	gjs->evt_filter.on_event = gjs_event_filter;
	gf_term_add_event_filter(gjs->term, &gjs->evt_filter);
	return JS_TRUE;
}

static JSBool gpac_set_focus(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Node *elt;
	GF_Terminal *term = gpac_get_term(c, obj);
	if (!argc) return JS_FALSE;

	if (JSVAL_IS_STRING(argv[0])) {
		char *focus_type = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
		if (!stricmp(focus_type, "previous")) {
			gf_sc_focus_switch_ring(term->compositor, 1, NULL, 0);
		} 
		else if (!stricmp(focus_type, "next")) {
			gf_sc_focus_switch_ring(term->compositor, 0, NULL, 0);
		}
	} else if (JSVAL_IS_OBJECT(argv[0])) {
		elt = gf_sg_js_get_node(c, JSVAL_TO_OBJECT(argv[0]));
		if (!elt) return JS_TRUE;

		gf_sc_focus_switch_ring(term->compositor, 0, elt, 2);
	}

	return JS_TRUE;
}

static JSBool gpac_get_scene(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Node *elt;
	u32 w, h;
	JSObject *scene_obj;
	GF_SceneGraph *sg;
	GF_Scene *scene=NULL;
	GF_GPACJSExt *gjs = (GF_GPACJSExt *)JS_GetPrivate(c, obj);
	if (!gjs || !argc || !JSVAL_IS_OBJECT(argv[0])) return JS_FALSE;

	elt = gf_sg_js_get_node(c, JSVAL_TO_OBJECT(argv[0]));
	if (!elt) return JS_TRUE;
	switch (elt->sgprivate->tag) {
	case TAG_MPEG4_Inline:
	case TAG_X3D_Inline:
		scene = (GF_Scene *)gf_node_get_private(elt);
		break;
	case TAG_SVG_animation:
		sg = gf_sc_animation_get_scenegraph(elt);
		scene = (GF_Scene *)gf_sg_get_private(sg);
		break;
	default:
		return JS_TRUE;
	}
	if (!scene) return JS_TRUE;


	scene_obj = JS_NewObject(c, &gjs->anyClass, 0, 0);
	JS_SetPrivate(c, scene_obj, scene);
	gf_sg_get_scene_size_info(scene->graph, &w, &h);
	JS_DefineProperty(c, scene_obj, "width", INT_TO_JSVAL(w), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(c, scene_obj, "height", INT_TO_JSVAL(h), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);	
	JS_DefineProperty(c, scene_obj, "connected", BOOLEAN_TO_JSVAL(scene->graph ? JS_TRUE : JS_FALSE), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);	
	*rval = OBJECT_TO_JSVAL(scene_obj);
	return JS_TRUE;
}
static void gjs_load(GF_JSUserExtension *jsext, GF_SceneGraph *scene, JSContext *c, JSObject *global, Bool unload)
{
	GF_GPACJSExt *gjs;
	JSObject *obj;

	GF_JSAPIParam par;

	JSPropertySpec gpacEvtClassProps[] = {
		{"type",			 0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY, 0, 0},
		{"keycode",			 1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY, 0, 0},
		{"mouse_x",			 2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY, 0, 0},
		{"mouse_y",			 3,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY, 0, 0},
		{"picked",			 4,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY, 0, 0},
		{"wheel",			 5,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY, 0, 0},
		{"button",			 6,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY, 0, 0},
		{0, 0, 0, 0, 0}
	};
	JSFunctionSpec gpacEvtClassFuncs[] = {
		{0, 0, 0, 0, 0}
	};

	JSPropertySpec gpacClassProps[] = {
		{0, 0, 0, 0, 0}
	};
	JSFunctionSpec gpacClassFuncs[] = {
		{"getOption",		gpac_getOption, 3, 0, 0},
		{"setOption",		gpac_setOption, 4, 0, 0},
		{"enum_directory",	gpac_enum_directory, 1, 0, 0},
		{"set_size",		gpac_set_size, 1, 0, 0},
		{"get_horizontal_dpi",	gpac_get_horizontal_dpi, 0, 0, 0},
		{"get_vertical_dpi",	gpac_get_vertical_dpi, 0, 0, 0},
		{"get_screen_width",	gpac_get_screen_width, 0, 0, 0},
		{"get_screen_height",	gpac_get_screen_height, 0, 0, 0},
		{"exit",				gpac_exit, 0, 0, 0},
		{"set_3d",				gpac_set_3d, 1, 0, 0},
		{"move_window",			gpac_move_window, 2, 0, 0},
		{"get_scene_time",		gpac_get_scene_time, 1, 0, 0},
		{"migrate_url",			gpac_migrate_url, 1, 0, 0},
		{"set_event_filter",	gpac_set_event_filter, 1, 0, 0},
		{"set_focus",			gpac_set_focus, 1, 0, 0},
		{"get_scene",			gpac_get_scene, 1, 0, 0},
		{"error_string",		gpac_error_string, 1, 0, 0},
		

		{0, 0, 0, 0, 0}
	};

	gjs = jsext->udta;
	/*nothing to do on unload*/
	if (unload) {
		gjs->nb_loaded--;
		if (!gjs->nb_loaded && gjs->evt_filter.udta) {
			gf_term_remove_event_filter(gjs->term, &gjs->evt_filter);
		}
		return;
	}
	if (gjs->nb_loaded) {
		gjs->nb_loaded++;
		return;
	}
	gjs->nb_loaded++;

	if (!scene) return;


	_SETUP_CLASS(gjs->gpacClass, "GPAC", JSCLASS_HAS_PRIVATE, gpac_getProperty, gpac_setProperty, JS_FinalizeStub);

	JS_InitClass(c, global, 0, &gjs->gpacClass, 0, 0, gpacClassProps, gpacClassFuncs, 0, 0);
	obj = JS_DefineObject(c, global, "gpac", &gjs->gpacClass, 0, 0);

	if (scene->script_action) {
		if (scene->script_action(scene->script_action_cbck, GF_JSAPI_OP_GET_TERM, scene->RootNode, &par)) {
			gjs->term = par.term;
		}
	}
	JS_SetPrivate(c, obj, gjs);


	_SETUP_CLASS(gjs->gpacEvtClass, "GPACEVT", JSCLASS_HAS_PRIVATE, gpacevt_getProperty, JS_PropertyStub, JS_FinalizeStub);
	JS_InitClass(c, global, 0, &gjs->gpacEvtClass, 0, 0, gpacEvtClassProps, gpacEvtClassFuncs, 0, 0);
	gjs->evt_obj = JS_DefineObject(c, global, "gpacevt", &gjs->gpacEvtClass, 0, 0);

#define DECLARE_GPAC_CONST(name) \
	JS_DefineProperty(c, global, #name, INT_TO_JSVAL(name), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);

	DECLARE_GPAC_CONST(GF_EVENT_CLICK);
	DECLARE_GPAC_CONST(GF_EVENT_MOUSEUP);
	DECLARE_GPAC_CONST(GF_EVENT_MOUSEDOWN);
	DECLARE_GPAC_CONST(GF_EVENT_MOUSEMOVE);
	DECLARE_GPAC_CONST(GF_EVENT_MOUSEWHEEL);
	DECLARE_GPAC_CONST(GF_EVENT_KEYUP);
	DECLARE_GPAC_CONST(GF_EVENT_KEYDOWN);
	DECLARE_GPAC_CONST(GF_EVENT_TEXTINPUT);
	DECLARE_GPAC_CONST(GF_EVENT_CONNECT);
	DECLARE_GPAC_CONST(GF_EVENT_NAVIGATE_INFO);
	DECLARE_GPAC_CONST(GF_EVENT_NAVIGATE);

	DECLARE_GPAC_CONST(GF_NAVIGATE_NONE);
	DECLARE_GPAC_CONST(GF_NAVIGATE_WALK);
	DECLARE_GPAC_CONST(GF_NAVIGATE_FLY);
	DECLARE_GPAC_CONST(GF_NAVIGATE_PAN);
	DECLARE_GPAC_CONST(GF_NAVIGATE_GAME);
	DECLARE_GPAC_CONST(GF_NAVIGATE_SLIDE);
	DECLARE_GPAC_CONST(GF_NAVIGATE_EXAMINE);
	DECLARE_GPAC_CONST(GF_NAVIGATE_ORBIT);
	DECLARE_GPAC_CONST(GF_NAVIGATE_VR);

	DECLARE_GPAC_CONST(GF_NAVIGATE_TYPE_NONE);
	DECLARE_GPAC_CONST(GF_NAVIGATE_TYPE_2D);
	DECLARE_GPAC_CONST(GF_NAVIGATE_TYPE_3D);
	
	_SETUP_CLASS(gjs->anyClass, "GPACOBJECT", JSCLASS_HAS_PRIVATE, JS_PropertyStub, JS_PropertyStub, JS_FinalizeStub);
	JS_InitClass(c, global, 0, &gjs->anyClass, 0, 0, 0, 0, 0, 0);
}





GF_JSUserExtension *gjs_new()
{
	GF_JSUserExtension *dr;
	GF_GPACJSExt *gjs;
	dr = gf_malloc(sizeof(GF_JSUserExtension));
	memset(dr, 0, sizeof(GF_JSUserExtension));
	GF_REGISTER_MODULE_INTERFACE(dr, GF_JS_USER_EXT_INTERFACE, "GPAC JavaScript Bindings", "gpac distribution");

	GF_SAFEALLOC(gjs, GF_GPACJSExt);
	dr->load = gjs_load;
	dr->udta = gjs;
	return dr;
}


void gjs_delete(GF_BaseInterface *ifce)
{
	GF_JSUserExtension *dr = (GF_JSUserExtension *) ifce;
	GF_GPACJSExt *gjs = dr->udta;
	gf_free(gjs);
	gf_free(dr);
}

#endif


GF_EXPORT
const u32 *QueryInterfaces() 
{
	static u32 si [] = {
#ifdef GPAC_HAS_SPIDERMONKEY
		GF_JS_USER_EXT_INTERFACE,
#endif
		0
	};
	return si;
}

GF_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType) 
{
#ifdef GPAC_HAS_SPIDERMONKEY
	if (InterfaceType == GF_JS_USER_EXT_INTERFACE) return (GF_BaseInterface *)gjs_new();
#endif
	return NULL;
}

GF_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
#ifdef GPAC_HAS_SPIDERMONKEY
	case GF_JS_USER_EXT_INTERFACE:
		gjs_delete(ifce);
		break;
#endif
	}
}
