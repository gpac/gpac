/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2007-2012
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


#include <gpac/setup.h>

#ifdef GPAC_HAS_SPIDERMONKEY

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


#if defined(GPAC_ANDROID) && !defined(XP_UNIX)
#define XP_UNIX
#endif

#if !defined(__GNUC__)
# if defined(_WIN32_WCE)
#  pragma comment(lib, "js32")
# elif defined (_WIN64)
#  pragma comment(lib, "js")
# elif defined (WIN32)
#  pragma comment(lib, "js")
# endif
#endif

#include <gpac/internal/scenegraph_dev.h>
#include <gpac/internal/smjs_api.h>

#include <gpac/modules/js_usr.h>
#include <gpac/internal/terminal_dev.h>
#include <gpac/internal/compositor_dev.h>

#define GPAC_JS_RTI_REFRESH_RATE	200

typedef struct
{
	u32 nb_loaded;
	GF_Terminal *term;

	GF_JSClass gpacClass;
	GF_JSClass gpacEvtClass;
	GF_JSClass anyClass;

	jsval evt_fun;
	GF_TermEventFilter evt_filter;
	GF_Event *evt;

	JSContext *c;
	JSObject *evt_filter_obj, *evt_obj;
	JSObject *gpac_obj;

	u32 rti_refresh_rate;
	GF_SystemRTInfo rti;
} GF_GPACJSExt;


static GF_Terminal *gpac_get_term(JSContext *c, JSObject *obj)
{
	GF_GPACJSExt *ext = (GF_GPACJSExt *)SMJS_GET_PRIVATE(c, obj);
	return ext ? ext->term : NULL;
}

static SMJS_FUNC_PROP_GET( gpac_getProperty)

const char *res;
char *prop_name;
GF_Terminal *term = gpac_get_term(c, obj);
if (!term) return JS_FALSE;

if (!SMJS_ID_IS_STRING(id)) return JS_TRUE;
prop_name = SMJS_CHARS_FROM_STRING(c, SMJS_ID_TO_STRING(id));
if (!prop_name) return JS_FALSE;

if (!strcmp(prop_name, "last_working_directory")) {
	res = gf_cfg_get_key(term->user->config, "General", "LastWorkingDir");
	if (!res) res = gf_cfg_get_key(term->user->config, "General", "ModulesDirectory");
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(c, res));
}
else if (!strcmp(prop_name, "scale_x")) {
	*vp = DOUBLE_TO_JSVAL( JS_NewDouble(c, FIX2FLT(term->compositor->scale_x)) );
}
else if (!strcmp(prop_name, "scale_y")) {
	*vp = DOUBLE_TO_JSVAL( JS_NewDouble(c, FIX2FLT(term->compositor->scale_y)) );
}
else if (!strcmp(prop_name, "translation_x")) {
	*vp = DOUBLE_TO_JSVAL( JS_NewDouble(c, FIX2FLT(term->compositor->trans_x)) );
}
else if (!strcmp(prop_name, "translation_y")) {
	*vp = DOUBLE_TO_JSVAL( JS_NewDouble(c, FIX2FLT(term->compositor->trans_y)) );
}
else if (!strcmp(prop_name, "rectangular_textures")) {
	Bool any_size = GF_FALSE;
#ifndef GPAC_DISABLE_3D
	if (term->compositor->gl_caps.npot_texture || term->compositor->gl_caps.rect_texture)
		any_size = GF_TRUE;
#endif
	*vp = BOOLEAN_TO_JSVAL( any_size ? JS_TRUE : JS_FALSE );
}
else if (!strcmp(prop_name, "batteryOn")) {
	Bool on_battery = GF_FALSE;
	gf_sys_get_battery_state(&on_battery, NULL, NULL, NULL, NULL);
	*vp = BOOLEAN_TO_JSVAL( on_battery ? JS_TRUE : JS_FALSE );
}
else if (!strcmp(prop_name, "batteryCharging")) {
	u32 on_charge = 0;
	gf_sys_get_battery_state(NULL, &on_charge, NULL, NULL, NULL);
	*vp = BOOLEAN_TO_JSVAL( on_charge ? JS_TRUE : JS_FALSE );
}
else if (!strcmp(prop_name, "batteryPercent")) {
	u32 level = 0;
	gf_sys_get_battery_state(NULL, NULL, &level, NULL, NULL);
	*vp = INT_TO_JSVAL( level );
}
else if (!strcmp(prop_name, "batteryLifeTime")) {
	u32 level = 0;
	gf_sys_get_battery_state(NULL, NULL, NULL, &level, NULL);
	*vp = INT_TO_JSVAL( level );
}
else if (!strcmp(prop_name, "batteryFullLifeTime")) {
	u32 level = 0;
	gf_sys_get_battery_state(NULL, NULL, NULL, NULL, &level);
	*vp = INT_TO_JSVAL( level );
}
else if (!strcmp(prop_name, "hostname")) {
	char hostname[100];
	gf_sk_get_host_name((char*)hostname);
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(c, hostname));
}
else if (!strcmp(prop_name, "fullscreen")) {
	*vp = BOOLEAN_TO_JSVAL( term->compositor->fullscreen ? JS_TRUE : JS_FALSE);
}
else if (!strcmp(prop_name, "current_path")) {
	char *url = gf_url_concatenate(term->root_scene->root_od->net_service->url, "");
	if (!url) url = gf_strdup("");
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(c, url));
	gf_free(url);
}
else if (!strcmp(prop_name, "volume")) {
	*vp = INT_TO_JSVAL( gf_term_get_option(term, GF_OPT_AUDIO_VOLUME));
}
else if (!strcmp(prop_name, "navigation")) {
	*vp = INT_TO_JSVAL( gf_term_get_option(term, GF_OPT_NAVIGATION));
}
else if (!strcmp(prop_name, "navigation_type")) {
	*vp = INT_TO_JSVAL( gf_term_get_option(term, GF_OPT_NAVIGATION_TYPE) );
}
else if (!strcmp(prop_name, "hardware_yuv")) {
	*vp = INT_TO_JSVAL( (term->compositor->video_out->hw_caps & GF_VIDEO_HW_HAS_YUV) ? 1 : 0 );
}
else if (!strcmp(prop_name, "hardware_rgb")) {
	*vp = INT_TO_JSVAL( (term->compositor->video_out->hw_caps & GF_VIDEO_HW_HAS_RGB) ? 1 : 0 );
}
else if (!strcmp(prop_name, "hardware_rgba")) {
	u32 has_rgba = (term->compositor->video_out->hw_caps & GF_VIDEO_HW_HAS_RGBA) ? 1 : 0;
#ifndef GPAC_DISABLE_3D
	if (term->compositor->hybrid_opengl || term->compositor->is_opengl) has_rgba = 1;
#endif
	*vp = INT_TO_JSVAL( has_rgba  );
}
else if (!strcmp(prop_name, "hardware_stretch")) {
	*vp = INT_TO_JSVAL( (term->compositor->video_out->hw_caps & GF_VIDEO_HW_HAS_STRETCH) ? 1 : 0 );
}
else if (!strcmp(prop_name, "screen_width")) {
	*vp = INT_TO_JSVAL( term->compositor->video_out->max_screen_width);
}
else if (!strcmp(prop_name, "screen_height")) {
	*vp = INT_TO_JSVAL( term->compositor->video_out->max_screen_height);
}
else if (!strcmp(prop_name, "http_bitrate")) {
	*vp = INT_TO_JSVAL( gf_dm_get_data_rate(term->downloader)*8/1024);
}
else if (!strcmp(prop_name, "fps")) {
	Double fps = gf_sc_get_fps(term->compositor, 0);
	*vp = DOUBLE_TO_JSVAL(JS_NewDouble(c, fps) );
}
else if (!strcmp(prop_name, "cpu_load")) {
	GF_GPACJSExt *ext = (GF_GPACJSExt *)SMJS_GET_PRIVATE(c, obj);
	gf_sys_get_rti(ext->rti_refresh_rate, &ext->rti, 0);
	*vp = INT_TO_JSVAL(ext->rti.process_cpu_usage);
}
else if (!strcmp(prop_name, "memory")) {
	GF_GPACJSExt *ext = (GF_GPACJSExt *)SMJS_GET_PRIVATE(c, obj);
	gf_sys_get_rti(ext->rti_refresh_rate, &ext->rti, 0);
	*vp = INT_TO_JSVAL(ext->rti.process_memory);
}


SMJS_FREE(c, prop_name);
return JS_TRUE;
}


static SMJS_FUNC_PROP_SET( gpac_setProperty)

char *prop_name, *prop_val;
GF_Terminal *term = gpac_get_term(c, obj);
if (!term) return JS_FALSE;

if (!SMJS_ID_IS_STRING(id)) return JS_TRUE;
prop_name = SMJS_CHARS_FROM_STRING(c, SMJS_ID_TO_STRING(id));

if (!strcmp(prop_name, "last_working_directory")) {
	if (!JSVAL_IS_STRING(*vp)) {
		SMJS_FREE(c, prop_name);
		return JS_FALSE;
	}
	prop_val = SMJS_CHARS(c, *vp);
	gf_cfg_set_key(term->user->config, "General", "LastWorkingDir", prop_val);
	SMJS_FREE(c, prop_val);
}
else if (!strcmp(prop_name, "caption")) {
	GF_Event evt;
	if (!JSVAL_IS_STRING(*vp)) {
		SMJS_FREE(c, prop_name);
		return JS_FALSE;
	}
	evt.type = GF_EVENT_SET_CAPTION;
	evt.caption.caption = SMJS_CHARS(c, *vp);
	gf_term_user_event(term, &evt);
	SMJS_FREE(c, (char*)evt.caption.caption);
}
else if (!strcmp(prop_name, "fullscreen")) {
	/*no fullscreen for iOS (always on)*/
#ifndef GPAC_IPHONE
	Bool res = (JSVAL_TO_BOOLEAN(*vp)==JS_TRUE) ? 1 : 0;
	if (term->compositor->fullscreen != res) {
		gf_term_set_option(term, GF_OPT_FULLSCREEN, res);
	}
#endif
}
else if (!strcmp(prop_name, "volume")) {
	if (JSVAL_IS_NUMBER(*vp)) {
		jsdouble d;
		JS_ValueToNumber(c, *vp, &d);
		gf_term_set_option(term, GF_OPT_AUDIO_VOLUME, (u32) d);
	} else if (JSVAL_IS_INT(*vp)) {
		gf_term_set_option(term, GF_OPT_AUDIO_VOLUME, JSVAL_TO_INT(*vp));
	}
}
else if (!strcmp(prop_name, "navigation")) {
	gf_term_set_option(term, GF_OPT_NAVIGATION, JSVAL_TO_INT(*vp) );
}
else if (!strcmp(prop_name, "navigation_type")) {
	gf_term_set_option(term, GF_OPT_NAVIGATION_TYPE, 0);
}
else if (!strcmp(prop_name, "disable_hardware_blit")) {
	term->compositor->disable_hardware_blit = JSVAL_TO_INT(*vp) ? 1 : 0;
	gf_sc_set_option(term->compositor, GF_OPT_REFRESH, 0);
}
else if (!strcmp(prop_name, "disable_composite_blit")) {
	Bool new_val = JSVAL_TO_INT(*vp) ? 1 : 0;
	if (new_val != term->compositor->disable_composite_blit) {
		term->compositor->disable_composite_blit = new_val;
		term->compositor->rebuild_offscreen_textures = 1;
		gf_sc_set_option(term->compositor, GF_OPT_REFRESH, 0);
	}
}
else if (!strcmp(prop_name, "http_bitrate")) {
	u32 new_rate = JSVAL_TO_INT(*vp);
	gf_dm_set_data_rate(term->downloader, new_rate);
}

SMJS_FREE(c, prop_name);
return JS_TRUE;
}

static JSBool SMJS_FUNCTION(gpac_getOption)
{
	const char *opt;
	char *sec_name, *key_name;
	s32 idx = -1;
	JSString *s;
	SMJS_OBJ
	SMJS_ARGS
	GF_Terminal *term = gpac_get_term(c, obj);
	if (!term) return JS_FALSE;

	if (argc < 2) return JS_FALSE;

	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[1]) && !JSVAL_IS_INT(argv[1])) return JS_FALSE;

	sec_name = SMJS_CHARS(c, argv[0]);
	key_name = NULL;
	if (JSVAL_IS_INT(argv[1])) {
		idx = JSVAL_TO_INT(argv[1]);
	} else if (JSVAL_IS_STRING(argv[1]) ) {
		key_name = SMJS_CHARS(c, argv[1]);
	}

	if (!stricmp(sec_name, "audiofilters")) {
		if (!term->compositor->audio_renderer->filter_chain.enable_filters
		        || !term->compositor->audio_renderer->filter_chain.filters->filter->GetOption) {
			SMJS_FREE(c, key_name);
			SMJS_FREE(c, sec_name);
			return JS_TRUE;
		}
		opt = term->compositor->audio_renderer->filter_chain.filters->filter->GetOption(term->compositor->audio_renderer->filter_chain.filters->filter, key_name);
	} else if (key_name) {
		opt = gf_cfg_get_key(term->user->config, sec_name, key_name);
	} else if (idx>=0) {
		opt = gf_cfg_get_key_name(term->user->config, sec_name, idx);
	}
	if (key_name) {
		SMJS_FREE(c, key_name);
	}
	SMJS_FREE(c, sec_name);

	if (opt) {
		s = JS_NewStringCopyZ(c, opt);
		SMJS_SET_RVAL( STRING_TO_JSVAL(s) );
	} else {
		SMJS_SET_RVAL( JSVAL_NULL );
	}
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(gpac_setOption)
{
	char *sec_name, *key_name, *key_val;
	SMJS_OBJ
	SMJS_ARGS
	GF_Terminal *term = gpac_get_term(c, obj);
	if (!term) return JS_FALSE;
	if (argc < 3) return JS_FALSE;

	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[1])) return JS_FALSE;

	sec_name = SMJS_CHARS(c, argv[0]);
	key_name = SMJS_CHARS(c, argv[1]);
	key_val = NULL;
	if (JSVAL_IS_STRING(argv[2]))
		key_val = SMJS_CHARS(c, argv[2]);

	if (!stricmp(sec_name, "audiofilters")) {
		if (term->compositor->audio_renderer->filter_chain.enable_filters
		        && term->compositor->audio_renderer->filter_chain.filters->filter->SetOption) {
			term->compositor->audio_renderer->filter_chain.filters->filter->SetOption(term->compositor->audio_renderer->filter_chain.filters->filter, key_name, key_val);
		}
	} else {
		gf_cfg_set_key(term->user->config, sec_name, key_name, key_val);
		if (!strcmp(sec_name, "Audio") && !strcmp(key_name, "Filter")) {
			gf_sc_reload_audio_filters(term->compositor);
		}
	}
	SMJS_FREE(c, sec_name);
	SMJS_FREE(c, key_name);
	if (key_val) {
		SMJS_FREE(c, key_val);
	}
	return JS_TRUE;
}

typedef struct
{
	JSContext *c;
	JSObject *array;
	Bool is_dir;
} enum_dir_cbk;

static Bool enum_dir_fct(void *cbck, char *file_name, char *file_path, GF_FileEnumInfo *file_info)
{
	u32 i, len;
	char *sep;
	JSString *s;
	jsuint idx;
	jsval v;
	JSObject *obj;
	enum_dir_cbk *cbk = (enum_dir_cbk*)cbck;

	if (file_name && (file_name[0]=='.')) return 0;

	obj = JS_NewObject(cbk->c, 0, 0, 0);
	s = JS_NewStringCopyZ(cbk->c, file_name);
	JS_DefineProperty(cbk->c, obj, "name", STRING_TO_JSVAL(s), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);

	sep=NULL;
	len = (u32) strlen(file_path);
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
	JS_DefineProperty(cbk->c, obj, "drive", BOOLEAN_TO_JSVAL(file_info->drive ? JS_TRUE : JS_FALSE), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(cbk->c, obj, "hidden", BOOLEAN_TO_JSVAL(file_info->hidden ? JS_TRUE : JS_FALSE), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(cbk->c, obj, "system", BOOLEAN_TO_JSVAL(file_info->system ? JS_TRUE : JS_FALSE), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(cbk->c, obj, "size", INT_TO_JSVAL(file_info->size), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(cbk->c, obj, "last_modified", INT_TO_JSVAL(file_info->last_modified), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);


	JS_GetArrayLength(cbk->c, cbk->array, &idx);
	v = OBJECT_TO_JSVAL(obj);
	JS_SetElement(cbk->c, cbk->array, idx, &v);
	return 0;
}

static JSBool SMJS_FUNCTION(gpac_enum_directory)
{
	GF_Err err;
	enum_dir_cbk cbk;
	char *url = NULL;
	char *dir = NULL;
	char *filter = NULL;
	char *an_url;
	Bool dir_only = 0;
	Bool browse_root = 0;
	GF_Terminal *term;
	SMJS_OBJ
	SMJS_ARGS

	if ((argc >= 1) && JSVAL_IS_STRING(argv[0])) {
		dir = SMJS_CHARS(c, argv[0]);
		if (!strcmp(dir, "/")) browse_root = 1;
	}
	if ((argc >= 2) && JSVAL_IS_STRING(argv[1])) {
		filter = SMJS_CHARS(c, argv[1]);
		if (!strcmp(filter, "dir")) {
			dir_only = 1;
			filter = NULL;
		} else if (!strlen(filter)) {
			SMJS_FREE(c, filter);
			filter=NULL;
		}
	}
	if ((argc >= 3) && JSVAL_IS_BOOLEAN(argv[2])) {
		if (JSVAL_TO_BOOLEAN(argv[2])==JS_TRUE) {
			url = gf_url_concatenate(dir, "..");
			if (!strcmp(url, "..") || (url[0]==0)) {
				if ((dir[1]==':') && ((dir[2]=='/') || (dir[2]=='\\')) ) browse_root = 1;
				else if (!strcmp(dir, "/")) browse_root = 1;
			}
			if (!strcmp(url, "/")) browse_root = 1;
		}
	}

	if ( (!dir || !strlen(dir) ) && (!url || !strlen(url))) browse_root = 1;

	if (browse_root) {
		cbk.c = c;
		cbk.array = JS_NewArrayObject(c, 0, 0);
		cbk.is_dir = 1;
		gf_enum_directory("/", 1, enum_dir_fct, &cbk, NULL);
		SMJS_SET_RVAL( OBJECT_TO_JSVAL(cbk.array) );
		if (url) gf_free(url);
		SMJS_FREE(c, dir);
		SMJS_FREE(c, filter);
		return JS_TRUE;
	}

	cbk.c = c;
	cbk.array = JS_NewArrayObject(c, 0, 0);

	cbk.is_dir = 1;

	term = gpac_get_term(c, obj);
	/*concatenate with service url*/
	an_url = gf_url_concatenate(term->root_scene->root_od->net_service->url, url ? url : dir);
	if (an_url) {
		gf_free(url);
		url = an_url;
	}
	err = gf_enum_directory(url ? url : dir, 1, enum_dir_fct, &cbk, NULL);

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

	SMJS_SET_RVAL( OBJECT_TO_JSVAL(cbk.array) );
	if (url) gf_free(url);
	SMJS_FREE(c, dir);
	SMJS_FREE(c, filter);
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(gpac_set_size)
{
	Bool override_size_info = 0;
	u32 w, h;
	jsdouble d;
	SMJS_OBJ
	SMJS_ARGS
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

static JSBool SMJS_FUNCTION(gpac_get_horizontal_dpi)
{
	SMJS_OBJ
	GF_Terminal *term = gpac_get_term(c, obj);
	if (term) SMJS_SET_RVAL( INT_TO_JSVAL(term->compositor->video_out->dpi_x) );
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(gpac_get_vertical_dpi)
{
	SMJS_OBJ
	GF_Terminal *term = gpac_get_term(c, obj);
	if (term) SMJS_SET_RVAL( INT_TO_JSVAL(term->compositor->video_out->dpi_y) );
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(gpac_get_screen_width)
{
	SMJS_OBJ
	GF_Terminal *term = gpac_get_term(c, obj);
	SMJS_SET_RVAL( INT_TO_JSVAL(term->compositor->video_out->max_screen_width));
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(gpac_get_screen_height)
{
	SMJS_OBJ
	GF_Terminal *term = gpac_get_term(c, obj);
	SMJS_SET_RVAL( INT_TO_JSVAL(term->compositor->video_out->max_screen_height));
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(gpac_exit)
{
	GF_Event evt;
	SMJS_OBJ
	GF_Terminal *term = gpac_get_term(c, obj);
	memset(&evt, 0, sizeof(GF_Event));
	evt.type = GF_EVENT_QUIT;
	gf_term_send_event(term, &evt);
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(gpac_set_3d)
{
	u32 type_3d = 0;
	SMJS_OBJ
	SMJS_ARGS
	GF_Terminal *term = gpac_get_term(c, obj);
	if (argc && JSVAL_IS_INT(argv[0])) type_3d = JSVAL_TO_INT(argv[0]);
	if (term->compositor->inherit_type_3d != type_3d) {
		term->compositor->inherit_type_3d = type_3d;
		term->compositor->root_visual_setup = 0;
		gf_sc_reset_graphics(term->compositor);
	}
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(gpac_move_window)
{
	SMJS_OBJ
	SMJS_ARGS
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


static JSBool SMJS_FUNCTION(gpac_error_string)
{
	const char *str;
	SMJS_ARGS

	if (argc < 1) return JS_TRUE;
	if (!JSVAL_IS_INT(argv[0]) ) return JS_TRUE;
	str = gf_error_to_string(JSVAL_TO_INT(argv[0]));

	SMJS_SET_RVAL( STRING_TO_JSVAL(JS_NewStringCopyZ(c, str)) );
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(gpac_get_scene_time)
{
	SMJS_OBJ
	SMJS_ARGS
	GF_SceneGraph *sg = NULL;
	GF_Terminal *term = gpac_get_term(c, obj);
	if (!argc || !JSVAL_IS_OBJECT(argv[0])) {
		sg = term->root_scene->graph;
	} else {
		GF_Node *n = gf_sg_js_get_node(c, JSVAL_TO_OBJECT(argv[0]));
		sg = n ? n->sgprivate->scenegraph : term->root_scene->graph;
	}
	SMJS_SET_RVAL( DOUBLE_TO_JSVAL( JS_NewDouble(c, sg->GetSceneTime(sg->userpriv) ) ) );

	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(gpac_trigger_gc)
{
	SMJS_OBJ
	GF_SceneGraph *sg = NULL;
	GF_Terminal *term = gpac_get_term(c, obj);

	sg = term->root_scene->graph;
	sg->trigger_gc = GF_TRUE;
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(gpac_migrate_url)
{
	char *url;
	u32 i, count;
	GF_NetworkCommand com;
	SMJS_OBJ
	SMJS_ARGS
	GF_Terminal *term = gpac_get_term(c, obj);
	if (!argc || !JSVAL_IS_STRING(argv[0])) return JS_FALSE;

	url = SMJS_CHARS(c, argv[0]);
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
			SMJS_SET_RVAL( STRING_TO_JSVAL(JS_NewStringCopyZ(c, com.migrate.data)) );
		} else {
			SMJS_SET_RVAL( STRING_TO_JSVAL(JS_NewStringCopyZ(c, odm->net_service->url)) );
		}
		break;
	}
	SMJS_FREE(c, url);
	return JS_TRUE;
}


static JSBool SMJS_FUNCTION(gjs_odm_addon_layout)
{
	u32 pos, size;
	SMJS_OBJ
	SMJS_ARGS
	GF_ObjectManager *odm = (GF_ObjectManager *)SMJS_GET_PRIVATE(c, obj);

	if (argc<2) return JS_TRUE;
	if (! SMJS_ID_IS_INT(argv[0]) ) return JS_TRUE;
	if (! SMJS_ID_IS_INT(argv[1]) ) return JS_TRUE;
	pos = SMJS_ID_TO_INT(argv[0]);
	size = SMJS_ID_TO_INT(argv[1]);

	gf_scene_set_addon_layout_info(odm->subscene, pos, size);
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(gjs_odm_declare_addon)
{
	char *addon_url = NULL;
	SMJS_OBJ
	SMJS_ARGS
	GF_ObjectManager *odm = (GF_ObjectManager *)SMJS_GET_PRIVATE(c, obj);

	if (! SMJS_ID_IS_STRING(argv[0]) ) return JS_TRUE;

	addon_url = SMJS_CHARS(c, argv[0]);
	if (addon_url) {
		GF_AssociatedContentLocation addon_info;
		memset(&addon_info, 0, sizeof(GF_AssociatedContentLocation));
		addon_info.external_URL = addon_url;
		addon_info.timeline_id = -100;
		addon_info.enable_if_defined = 1;
		gf_scene_register_associated_media(odm->subscene ? odm->subscene : odm->parentscene, &addon_info);
	}
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(gpac_get_object_manager)
{
	u32 i, count;
	GF_ObjectManager *odm = NULL;
	const char *service_url;
	SMJS_OBJ
	SMJS_ARGS
	GF_GPACJSExt *gjs = (GF_GPACJSExt *)SMJS_GET_PRIVATE(c, obj);
	GF_Terminal *term = gpac_get_term(c, obj);
	GF_Scene *scene = term->root_scene;

	if (SMJS_ID_IS_STRING(argv[0]) ) {
		service_url = SMJS_CHARS(c, argv[0]);
		if (!service_url) {
			SMJS_SET_RVAL(JSVAL_NULL);
			return JS_TRUE;
		}
		if (!strncmp(service_url, "gpac://", 7)) service_url += 7;
		count = gf_list_count(scene->resources);
		for (i=0; i<count; i++) {
			odm = gf_list_get(scene->resources, i);
			if (odm->net_service && !strcmp(odm->net_service->url, service_url)) break;
			odm = NULL;
		}
	}

	if (!odm) {
		SMJS_SET_RVAL(JSVAL_NULL);
		return JS_TRUE;
	}

	obj = JS_NewObject(c, &gjs->anyClass._class, 0, 0);
	SMJS_SET_PRIVATE(c, obj, odm);

	JS_DefineFunction(c, obj, "declare_addon", gjs_odm_declare_addon, 1, 0);
	JS_DefineFunction(c, obj, "enable_addon", gjs_odm_declare_addon, 1, 0);
	JS_DefineFunction(c, obj, "addon_layout", gjs_odm_addon_layout, 1, 0);

	SMJS_SET_RVAL( OBJECT_TO_JSVAL(obj) );
	return JS_TRUE;
}


static SMJS_FUNC_PROP_GET( gpacevt_getProperty)

GF_GPACJSExt *gjs = SMJS_GET_PRIVATE(c, obj);
GF_Event *evt = gjs->evt;
if (!evt) return 0;

if (SMJS_ID_IS_INT(id)) {
	switch (SMJS_ID_TO_INT(id)) {
	case -1:
#ifndef GPAC_DISABLE_SVG
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(c, gf_dom_get_key_name(evt->key.key_code) ));
#endif
		break;
	case -2:
		*vp = INT_TO_JSVAL(evt->mouse.x);
		break;
	case -3:
		*vp = INT_TO_JSVAL(evt->mouse.y);
		break;
	case -4:
		if (gjs->term->compositor->hit_appear) *vp = BOOLEAN_TO_JSVAL(JS_TRUE);
		else if (gf_list_count(gjs->term->compositor->previous_sensors) ) *vp = BOOLEAN_TO_JSVAL(JS_TRUE);
		else if (gjs->term->compositor->text_selection) *vp = BOOLEAN_TO_JSVAL(JS_TRUE);
		else *vp = BOOLEAN_TO_JSVAL(JS_FALSE);
		break;
	case -5:
		*vp = DOUBLE_TO_JSVAL( JS_NewDouble(c, FIX2FLT(evt->mouse.wheel_pos)) );
		break;
	case -6:
		*vp = INT_TO_JSVAL( evt->mouse.button);
		break;
	case -7:
		*vp = INT_TO_JSVAL(evt->type);
		break;
	case -8:
#ifndef GPAC_DISABLE_SVG
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(c, gf_dom_event_get_name(evt->type) ));
#endif
		break;
	}
} else if (SMJS_ID_IS_STRING(id)) {
	char *name = SMJS_CHARS_FROM_STRING(c, SMJS_ID_TO_STRING(id));
	if (!strcmp(name, "target_url")) {
		*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, evt->navigate.to_url) );
	}
	else if (!strcmp(name, "files")) {
		u32 i, idx;
		jsval v;
		JSObject *files_array = JS_NewArrayObject(c, 0, NULL);
		for (i=0; i<evt->open_file.nb_files; i++) {
			if (evt->open_file.files[i]) {
				JS_GetArrayLength(c, files_array, &idx);
				v = STRING_TO_JSVAL( JS_NewStringCopyZ(c, evt->open_file.files[i]) );
				JS_SetElement(c, files_array, idx, &v);
			}
		}
		*vp = OBJECT_TO_JSVAL(files_array);
	}
	SMJS_FREE(c, name);
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
	res = 0;
	while (gjs->nb_loaded) {
		res = gf_sg_try_lock_javascript(gjs->c);
		if (res) break;
	}
	if (!res) return 0;

	rval = JSVAL_VOID;
	gjs->evt = evt;
	SMJS_SET_PRIVATE(gjs->c, gjs->evt_obj, gjs);
	argv[0] = OBJECT_TO_JSVAL(gjs->evt_obj);
	rval = JSVAL_VOID;
	JS_CallFunctionValue(gjs->c, gjs->evt_filter_obj, gjs->evt_fun, 1, argv, &rval);
	SMJS_SET_PRIVATE(gjs->c, gjs->evt_obj, NULL);
	gjs->evt = NULL;

	res = 0;
	if (JSVAL_IS_BOOLEAN(rval) ) {
		res = (JSVAL_TO_BOOLEAN(rval)==JS_TRUE) ? 1 : 0;
	} else if (JSVAL_IS_INT(rval) ) {
		res = (JSVAL_TO_INT(rval)) ? 1 : 0;
	}

	gf_sg_lock_javascript(gjs->c, 0);
	return res;
}

static JSBool SMJS_FUNCTION(gpac_set_event_filter)
{
	SMJS_OBJ
	SMJS_ARGS
	GF_GPACJSExt *gjs = (GF_GPACJSExt *)SMJS_GET_PRIVATE(c, obj);
	if (!argc || !JSVAL_IS_OBJECT(argv[0])) return JS_FALSE;

	if (! JSVAL_IS_NULL(gjs->evt_fun) ) return JS_TRUE;

	gjs->evt_fun = argv[0];
	gjs->evt_filter_obj = obj;
	gjs->c = c;
	gjs->evt_filter.udta = gjs;
	gjs->evt_filter.on_event = gjs_event_filter;
	gf_term_add_event_filter(gjs->term, &gjs->evt_filter);
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(gpac_set_focus)
{
	GF_Node *elt;
	SMJS_OBJ
	SMJS_ARGS
	GF_Terminal *term = gpac_get_term(c, obj);
	if (!argc) return JS_FALSE;

	if (JSVAL_IS_NULL(argv[0])) {
		gf_sc_focus_switch_ring(term->compositor, 0, NULL, 0);
		return JS_TRUE;
	}

	if (JSVAL_IS_STRING(argv[0])) {
		char *focus_type = SMJS_CHARS(c, argv[0]);
		if (!stricmp(focus_type, "previous")) {
			gf_sc_focus_switch_ring(term->compositor, 1, NULL, 0);
		}
		else if (!stricmp(focus_type, "next")) {
			gf_sc_focus_switch_ring(term->compositor, 0, NULL, 0);
		}
		SMJS_FREE(c, focus_type);
	} else if (JSVAL_IS_OBJECT(argv[0])) {
		elt = gf_sg_js_get_node(c, JSVAL_TO_OBJECT(argv[0]));
		if (!elt) return JS_TRUE;

		gf_sc_focus_switch_ring(term->compositor, 0, elt, 2);
	}

	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(gpac_get_scene)
{
	GF_Node *elt;
	u32 w, h;
	JSObject *scene_obj;
	GF_SceneGraph *sg;
	GF_Scene *scene=NULL;
	SMJS_OBJ
	SMJS_ARGS
	GF_GPACJSExt *gjs = (GF_GPACJSExt *)SMJS_GET_PRIVATE(c, obj);
	if (!gjs || !argc || !JSVAL_IS_OBJECT(argv[0])) return JS_FALSE;

	elt = gf_sg_js_get_node(c, JSVAL_TO_OBJECT(argv[0]));
	if (!elt) return JS_TRUE;
	switch (elt->sgprivate->tag) {
	case TAG_MPEG4_Inline:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Inline:
#endif
		scene = (GF_Scene *)gf_node_get_private(elt);
		break;
#ifndef GPAC_DISABLE_SVG
	case TAG_SVG_animation:
		sg = gf_sc_animation_get_scenegraph(elt);
		scene = (GF_Scene *)gf_sg_get_private(sg);
		break;
#endif
	default:
		return JS_TRUE;
	}
	if (!scene) return JS_TRUE;


	scene_obj = JS_NewObject(c, &gjs->anyClass._class, 0, 0);
	SMJS_SET_PRIVATE(c, scene_obj, scene);
	gf_sg_get_scene_size_info(scene->graph, &w, &h);
	JS_DefineProperty(c, scene_obj, "width", INT_TO_JSVAL(w), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(c, scene_obj, "height", INT_TO_JSVAL(h), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(c, scene_obj, "connected", BOOLEAN_TO_JSVAL(scene->graph ? JS_TRUE : JS_FALSE), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(scene_obj) );
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(gpac_show_keyboard)
{
	SMJS_OBJ
	SMJS_ARGS
	GF_Terminal *term = gpac_get_term(c, obj);
	if (!argc) return JS_FALSE;

	if (JSVAL_IS_BOOLEAN(argv[0])) {
		Bool show = JSVAL_TO_BOOLEAN(argv[0])==JS_TRUE;
		GF_Event evt;
		memset(&evt, 0, sizeof(GF_Event));
		evt.type = show ? GF_EVENT_TEXT_EDITING_START : GF_EVENT_TEXT_EDITING_END;
		gf_term_user_event(term, &evt);
	}
	return JS_TRUE;
}

static void gjs_load(GF_JSUserExtension *jsext, GF_SceneGraph *scene, JSContext *c, JSObject *global, Bool unload)
{
	GF_GPACJSExt *gjs;
	GF_JSAPIParam par;

	JSPropertySpec gpacEvtClassProps[] = {
		SMJS_PROPERTY_SPEC("keycode",			 -1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("mouse_x",			 -2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("mouse_y",			 -3,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("picked",			 -4,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("wheel",			 -5,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("button",			-6,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("type",			 -7,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("name",			 -8,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0)
	};
	JSFunctionSpec gpacEvtClassFuncs[] = {
		SMJS_FUNCTION_SPEC(0, 0, 0)
	};

	JSPropertySpec gpacClassProps[] = {
		SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0)
	};
	JSFunctionSpec gpacClassFuncs[] = {
		SMJS_FUNCTION_SPEC("getOption",		gpac_getOption, 3),
		SMJS_FUNCTION_SPEC("setOption",		gpac_setOption, 4),
		SMJS_FUNCTION_SPEC("get_option",		gpac_getOption, 3),
		SMJS_FUNCTION_SPEC("set_option",		gpac_setOption, 4),
		SMJS_FUNCTION_SPEC("enum_directory",	gpac_enum_directory, 1),
		SMJS_FUNCTION_SPEC("set_size",		gpac_set_size, 1),
		SMJS_FUNCTION_SPEC("get_horizontal_dpi",	gpac_get_horizontal_dpi, 0),
		SMJS_FUNCTION_SPEC("get_vertical_dpi",	gpac_get_vertical_dpi, 0),
		SMJS_FUNCTION_SPEC("get_screen_width",	gpac_get_screen_width, 0),
		SMJS_FUNCTION_SPEC("get_screen_height",	gpac_get_screen_height, 0),
		SMJS_FUNCTION_SPEC("exit",				gpac_exit, 0),
		SMJS_FUNCTION_SPEC("set_3d",				gpac_set_3d, 1),
		SMJS_FUNCTION_SPEC("move_window",			gpac_move_window, 2),
		SMJS_FUNCTION_SPEC("get_scene_time",		gpac_get_scene_time, 1),
		SMJS_FUNCTION_SPEC("migrate_url",			gpac_migrate_url, 1),
		SMJS_FUNCTION_SPEC("set_event_filter",	gpac_set_event_filter, 1),
		SMJS_FUNCTION_SPEC("set_focus",			gpac_set_focus, 1),
		SMJS_FUNCTION_SPEC("get_scene",			gpac_get_scene, 1),
		SMJS_FUNCTION_SPEC("error_string",		gpac_error_string, 1),
		SMJS_FUNCTION_SPEC("show_keyboard",		gpac_show_keyboard, 1),
		SMJS_FUNCTION_SPEC("trigger_gc",		gpac_trigger_gc, 1),
		SMJS_FUNCTION_SPEC("get_object_manager",		gpac_get_object_manager, 1),


		SMJS_FUNCTION_SPEC(0, 0, 0)
	};

	gjs = jsext->udta;
	/*nothing to do on unload*/
	if (unload) {
		gjs->nb_loaded--;
		/*if we destroy the script context holding the gpac event filter (only one for the time being), remove the filter*/
		if ((gjs->c==c) && gjs->evt_filter.udta) {
			gf_term_remove_event_filter(gjs->term, &gjs->evt_filter);
			gjs->evt_filter.udta = NULL;
		}
		return;
	}

	gjs->nb_loaded++;

	if (!scene) return;

	JS_SETUP_CLASS(gjs->gpacClass, "GPAC", JSCLASS_HAS_PRIVATE, gpac_getProperty, gpac_setProperty, JS_FinalizeStub);

	if (!gjs->gpac_obj) {
		GF_JS_InitClass(c, global, 0, &gjs->gpacClass, 0, 0, gpacClassProps, gpacClassFuncs, 0, 0);
		gjs->gpac_obj = JS_DefineObject(c, global, "gpac", &gjs->gpacClass._class, 0, 0);

		if (scene->script_action) {
			if (scene->script_action(scene->script_action_cbck, GF_JSAPI_OP_GET_TERM, scene->RootNode, &par)) {
				gjs->term = par.term;
			}
		}
		SMJS_SET_PRIVATE(c, gjs->gpac_obj, gjs);
	} else {
		JS_DefineProperty(c, global, "gpac", OBJECT_TO_JSVAL(gjs->gpac_obj), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	}

	if (!gjs->evt_obj) {
		JS_SETUP_CLASS(gjs->gpacEvtClass, "GPACEVT", JSCLASS_HAS_PRIVATE, gpacevt_getProperty, JS_PropertyStub_forSetter, JS_FinalizeStub);
		GF_JS_InitClass(c, global, 0, &gjs->gpacEvtClass, 0, 0, gpacEvtClassProps, gpacEvtClassFuncs, 0, 0);
		gjs->evt_obj = JS_DefineObject(c, global, "gpacevt", &gjs->gpacEvtClass._class, 0, 0);

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
		DECLARE_GPAC_CONST(GF_EVENT_DROPFILE);
		DECLARE_GPAC_CONST(GF_EVENT_ADDON_DETECTED);

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

		JS_SETUP_CLASS(gjs->anyClass, "GPACOBJECT", JSCLASS_HAS_PRIVATE, JS_PropertyStub, JS_PropertyStub_forSetter, JS_FinalizeStub);
		GF_JS_InitClass(c, global, 0, &gjs->anyClass, 0, 0, 0, 0, 0, 0);

		gjs->evt_fun = JSVAL_NULL;
	}
}





GF_JSUserExtension *gjs_new()
{
	GF_JSUserExtension *dr;
	GF_GPACJSExt *gjs;
	dr = gf_malloc(sizeof(GF_JSUserExtension));
	memset(dr, 0, sizeof(GF_JSUserExtension));
	GF_REGISTER_MODULE_INTERFACE(dr, GF_JS_USER_EXT_INTERFACE, "GPAC JavaScript Bindings", "gpac distribution");

	GF_SAFEALLOC(gjs, GF_GPACJSExt);
	gjs->rti_refresh_rate = GPAC_JS_RTI_REFRESH_RATE;
	gjs->evt_fun = JSVAL_NULL;
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


GPAC_MODULE_EXPORT
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

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
#ifdef GPAC_HAS_SPIDERMONKEY
	if (InterfaceType == GF_JS_USER_EXT_INTERFACE) return (GF_BaseInterface *)gjs_new();
#endif
	return NULL;
}

GPAC_MODULE_EXPORT
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

GPAC_MODULE_STATIC_DELARATION( gpac_js )
