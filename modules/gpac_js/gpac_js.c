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
#include <gpac/term_info.h>

#define GPAC_JS_RTI_REFRESH_RATE	200

typedef struct
{
	u32 nb_loaded;
	GF_Terminal *term;

	GF_JSClass gpacClass;
	GF_JSClass gpacEvtClass;
	GF_JSClass odmClass;
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
s32 prop_id;
GF_Terminal *term = gpac_get_term(c, obj);
if (!term) return JS_FALSE;

if (!SMJS_ID_IS_INT(id)) return JS_TRUE;
prop_id = SMJS_ID_TO_INT(id);

switch (prop_id) {
case -1: //"last_working_directory"
	res = gf_cfg_get_key(term->user->config, "General", "LastWorkingDir");
	if (!res) res = gf_cfg_get_key(term->user->config, "General", "ModulesDirectory");
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(c, res));
	break;

case -2: //"scale_x"))
	*vp = DOUBLE_TO_JSVAL( JS_NewDouble(c, FIX2FLT(term->compositor->scale_x)) );
	break;

case -3: //"scale_y"
	*vp = DOUBLE_TO_JSVAL( JS_NewDouble(c, FIX2FLT(term->compositor->scale_y)) );
	break;

case -4: //"translation_x"
	*vp = DOUBLE_TO_JSVAL( JS_NewDouble(c, FIX2FLT(term->compositor->trans_x)) );
	break;

case -5: //"translation_y"
	*vp = DOUBLE_TO_JSVAL( JS_NewDouble(c, FIX2FLT(term->compositor->trans_y)) );
	break;

case -6: //"rectangular_textures"
{
	Bool any_size = GF_FALSE;
#ifndef GPAC_DISABLE_3D
	if (term->compositor->gl_caps.npot_texture || term->compositor->gl_caps.rect_texture)
		any_size = GF_TRUE;
#endif
	*vp = BOOLEAN_TO_JSVAL( any_size ? JS_TRUE : JS_FALSE );
}
break;

case -7: //"batteryOn"
{
	Bool on_battery = GF_FALSE;
	gf_sys_get_battery_state(&on_battery, NULL, NULL, NULL, NULL);
	*vp = BOOLEAN_TO_JSVAL( on_battery ? JS_TRUE : JS_FALSE );
}
break;

case -8: //	"batteryCharging"
{
	u32 on_charge = 0;
	gf_sys_get_battery_state(NULL, &on_charge, NULL, NULL, NULL);
	*vp = BOOLEAN_TO_JSVAL( on_charge ? JS_TRUE : JS_FALSE );
}
break;

case -9: //	"batteryPercent"
{
	u32 level = 0;
	gf_sys_get_battery_state(NULL, NULL, &level, NULL, NULL);
	*vp = INT_TO_JSVAL( level );
}
break;

case -10: //"batteryLifeTime"
{
	u32 level = 0;
	gf_sys_get_battery_state(NULL, NULL, NULL, &level, NULL);
	*vp = INT_TO_JSVAL( level );
}
break;

case -11: //"batteryFullLifeTime"
{
	u32 level = 0;
	gf_sys_get_battery_state(NULL, NULL, NULL, NULL, &level);
	*vp = INT_TO_JSVAL( level );
}
break;

case -12: //"hostname"
{
	char hostname[100];
	gf_sk_get_host_name((char*)hostname);
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(c, hostname));
}
break;

case -13: //"fullscreen"
	*vp = BOOLEAN_TO_JSVAL( term->compositor->fullscreen ? JS_TRUE : JS_FALSE);
	break;

case -14: //"current_path"
{
	char *url = gf_url_concatenate(term->root_scene->root_od->net_service->url, "");
	if (!url) url = gf_strdup("");
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(c, url));
	gf_free(url);
}
break;

case -15: //"volume"
	*vp = INT_TO_JSVAL( gf_term_get_option(term, GF_OPT_AUDIO_VOLUME));
	break;

case -16: //"navigation"
	*vp = INT_TO_JSVAL( gf_term_get_option(term, GF_OPT_NAVIGATION));
	break;

case -17: //"navigation_type"
	*vp = INT_TO_JSVAL( gf_term_get_option(term, GF_OPT_NAVIGATION_TYPE) );
	break;

case -18: //"hardware_yuv"
	*vp = INT_TO_JSVAL( (term->compositor->video_out->hw_caps & GF_VIDEO_HW_HAS_YUV) ? 1 : 0 );
	break;

case -19: //"hardware_rgb"
	*vp = INT_TO_JSVAL( (term->compositor->video_out->hw_caps & GF_VIDEO_HW_HAS_RGB) ? 1 : 0 );
	break;

case -20: //"hardware_rgba"
{
	u32 has_rgba = (term->compositor->video_out->hw_caps & GF_VIDEO_HW_HAS_RGBA) ? 1 : 0;
#ifndef GPAC_DISABLE_3D
	if (term->compositor->hybrid_opengl || term->compositor->is_opengl) has_rgba = 1;
#endif
	*vp = INT_TO_JSVAL( has_rgba  );
}
break;

case -21: //"hardware_stretch"
	*vp = INT_TO_JSVAL( (term->compositor->video_out->hw_caps & GF_VIDEO_HW_HAS_STRETCH) ? 1 : 0 );
	break;

case -22: //"screen_width"
	*vp = INT_TO_JSVAL( term->compositor->video_out->max_screen_width);
	break;

case -23: //"screen_height"
	*vp = INT_TO_JSVAL( term->compositor->video_out->max_screen_height);
	break;

case -24: //"http_max_bitrate"
	*vp = INT_TO_JSVAL( gf_dm_get_data_rate(term->downloader));
	break;

case -25: //"http_bitrate"
	*vp = INT_TO_JSVAL( gf_dm_get_global_rate(term->downloader));
	break;

case -26: //"fps"
	*vp = DOUBLE_TO_JSVAL(JS_NewDouble(c, gf_sc_get_fps(term->compositor, 0) ) );
	break;

case -27: //"cpu_load" || "cpu"
{
	GF_GPACJSExt *ext = (GF_GPACJSExt *)SMJS_GET_PRIVATE(c, obj);
	gf_sys_get_rti(ext->rti_refresh_rate, &ext->rti, 0);
	*vp = INT_TO_JSVAL(ext->rti.process_cpu_usage);
}
break;

case -28: //"nb_cores"
{
	GF_GPACJSExt *ext = (GF_GPACJSExt *)SMJS_GET_PRIVATE(c, obj);
	gf_sys_get_rti(ext->rti_refresh_rate, &ext->rti, 0);
	*vp = INT_TO_JSVAL(ext->rti.nb_cores);
}
break;

case -29: //"system_memory"
{
	GF_GPACJSExt *ext = (GF_GPACJSExt *)SMJS_GET_PRIVATE(c, obj);
	gf_sys_get_rti(ext->rti_refresh_rate, &ext->rti, 0);
	*vp = DOUBLE_TO_JSVAL(JS_NewDouble(c, (Double) ext->rti.physical_memory ) );
}
break;

case -30: //"memory"
{
	GF_GPACJSExt *ext = (GF_GPACJSExt *)SMJS_GET_PRIVATE(c, obj);
	gf_sys_get_rti(ext->rti_refresh_rate, &ext->rti, 0);
	*vp = INT_TO_JSVAL(ext->rti.process_memory);
}
break;

case -31: //"argc"
	*vp = INT_TO_JSVAL(gf_sys_get_argc() );
	break;

case -36://"dpi_x"
	*vp = INT_TO_JSVAL(term->compositor->video_out->dpi_x);
	break;

case -37://"dpi_y"
	*vp = INT_TO_JSVAL(term->compositor->video_out->dpi_y);
	break;
}

return JS_TRUE;
}


static SMJS_FUNC_PROP_SET( gpac_setProperty)

s32 prop_id;
char *prop_val;
GF_Terminal *term = gpac_get_term(c, obj);
if (!term) return JS_FALSE;

if (!SMJS_ID_IS_INT(id)) return JS_TRUE;
prop_id = SMJS_ID_TO_INT(id);

switch (prop_id) {
case -1: //"last_working_directory"
	if (!JSVAL_IS_STRING(*vp)) {
		return JS_FALSE;
	}
	prop_val = SMJS_CHARS(c, *vp);
	gf_cfg_set_key(term->user->config, "General", "LastWorkingDir", prop_val);
	SMJS_FREE(c, prop_val);
	break;
case -32://"caption"
{
	GF_Event evt;
	if (!JSVAL_IS_STRING(*vp)) {
		return JS_FALSE;
	}
	evt.type = GF_EVENT_SET_CAPTION;
	evt.caption.caption = SMJS_CHARS(c, *vp);
	gf_term_user_event(term, &evt);
	SMJS_FREE(c, (char*)evt.caption.caption);
}
break;
case -13://"fullscreen"
{
	/*no fullscreen for iOS (always on)*/
#ifndef GPAC_IPHONE
	Bool res = (JSVAL_TO_BOOLEAN(*vp)==JS_TRUE) ? 1 : 0;
	if (term->compositor->fullscreen != res) {
		gf_term_set_option(term, GF_OPT_FULLSCREEN, res);
	}
#endif
}
break;
case -15: //"volume"
	if (JSVAL_IS_NUMBER(*vp)) {
		jsdouble d;
		SMJS_GET_NUMBER(*vp, d);
		gf_term_set_option(term, GF_OPT_AUDIO_VOLUME, (u32) d);
	} else if (JSVAL_IS_INT(*vp)) {
		gf_term_set_option(term, GF_OPT_AUDIO_VOLUME, JSVAL_TO_INT(*vp));
	}
	break;

case -16: //"navigation"
	gf_term_set_option(term, GF_OPT_NAVIGATION, JSVAL_TO_INT(*vp) );
	break;
case -17: //"navigation_type"
	gf_term_set_option(term, GF_OPT_NAVIGATION_TYPE, 0);
	break;
case -33: //"disable_hardware_blit"
	term->compositor->disable_hardware_blit = JSVAL_TO_INT(*vp) ? 1 : 0;
	gf_sc_set_option(term->compositor, GF_OPT_REFRESH, 0);
	break;
case -34: //"disable_composite_blit"
{
	Bool new_val = JSVAL_TO_INT(*vp) ? 1 : 0;
	if (new_val != term->compositor->disable_composite_blit) {
		term->compositor->disable_composite_blit = new_val;
		term->compositor->rebuild_offscreen_textures = 1;
		gf_sc_set_option(term->compositor, GF_OPT_REFRESH, 0);
	}
}
break;
case -24:  //"http_max_bitrate"
{
	u32 new_rate = JSVAL_TO_INT(*vp);
	gf_dm_set_data_rate(term->downloader, new_rate);
}
break;
case -35: //"focus_highlight"
	term->compositor->disable_focus_highlight = JSVAL_TO_BOOLEAN(*vp) ? 0 : 1;
	break;
}
return JS_TRUE;
}

static JSBool SMJS_FUNCTION(gpac_getOption)
{
	const char *opt = NULL;
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
    } else {
        opt = NULL;
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


static JSBool SMJS_FUNCTION(gpac_get_arg)
{
	SMJS_OBJ
	SMJS_ARGS
	GF_Terminal *term = gpac_get_term(c, obj);
	if (!term) return JS_FALSE;

	if (argc < 1) return JS_FALSE;

	if (JSVAL_IS_INT(argv[0])) {
		u32 idx = JSVAL_TO_INT(argv[0]);
		const char *arg = gf_sys_get_arg(idx);
		if (arg) {
			SMJS_SET_RVAL( STRING_TO_JSVAL(JS_NewStringCopyZ(c, arg)) );
		} else {
			SMJS_SET_RVAL( JSVAL_NULL );
		}
		return JS_TRUE;
	} else {
		return JS_FALSE;
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
		SMJS_GET_NUMBER(argv[0], d);
		w = (u32) d;
	}
	if ((argc >= 2) && JSVAL_IS_NUMBER(argv[1])) {
		SMJS_GET_NUMBER(argv[1], d);
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

static SMJS_FUNC_PROP_GET( odm_getProperty)

	GF_ObjectManager *odm = (GF_ObjectManager *)SMJS_GET_PRIVATE(c, obj);
	GF_MediaInfo odi;
	char *str;
	if (!SMJS_ID_IS_INT(id)) return JS_TRUE;

	gf_term_get_object_info(odm->term, odm, &odi);

	switch (SMJS_ID_TO_INT(id)) {
	case -1:
		*vp = INT_TO_JSVAL(odi.od->objectDescriptorID);
		break;
	case -2:
		*vp = INT_TO_JSVAL(odm->subscene ? gf_list_count(odm->subscene->resources) : 0);
		break;
	case -3:
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(c, odi.service_url));
		break;
	case -4:
		*vp = DOUBLE_TO_JSVAL( JS_NewDouble(c, odi.duration) );
		break;
	case -5:
		*vp = DOUBLE_TO_JSVAL( JS_NewDouble(c, odi.current_time) );
		break;
	case -6:
		*vp = INT_TO_JSVAL( odi.clock_drift);
		break;
	case -7:
		if (odi.status==0) str = "Stopped";
		else if (odi.status==1) str = "Playing";
		else if (odi.status==2) str = "Paused";
		else if (odi.status==3) str = "Not Setup";
		else str = "Setup Failed";
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(c, str));
		break;
	case -8:
		*vp = INT_TO_JSVAL( odi.buffer);
		break;
	case -9:
		*vp = INT_TO_JSVAL( odi.db_unit_count);
		break;
	case -10:
		*vp = INT_TO_JSVAL( odi.cb_unit_count);
		break;
	case -11:
		*vp = INT_TO_JSVAL( odi.cb_max_count);
		break;
	case -12:
		if (odi.od_type==GF_STREAM_SCENE) str = "Scene";
		else if (odi.od_type==GF_STREAM_OD) str = "Object Descriptor";
		else if (odi.od_type==GF_STREAM_VISUAL) str = "Video";
		else if (odi.od_type==GF_STREAM_AUDIO) str = "Audio";
		else if (odi.od_type==GF_STREAM_TEXT) str = "Text";
		else if (odm->subscene) str = "Subscene";
		else str = "Unknow";
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(c, str));
		break;
	case -13:
		*vp = INT_TO_JSVAL( odi.sample_rate);
		break;
	case -14:
		*vp = INT_TO_JSVAL( odi.num_channels);
		break;
	case -15:
		*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, gf_4cc_to_str(odi.lang) ) );
		break;
	case -16:
		*vp = INT_TO_JSVAL( odi.width);
		break;
	case -17:
		*vp = INT_TO_JSVAL( odi.height);
		break;
	case -18:
		*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, gf_4cc_to_str(odi.pixelFormat) ) );
		break;
	case -19:
		if (odi.par) {
			char szPar[50];
			sprintf(szPar, "%d:%d", (odi.par>>16)&0xFF, (odi.par)&0xFF );
			*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, szPar ) );
		} else {
			*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, "1:1" ) );
		}
		break;
	case -20:
		*vp = INT_TO_JSVAL(odi.nb_dec_frames);
		break;
	case -21:
		*vp = INT_TO_JSVAL(odi.nb_droped);
		break;
	case -22:
		*vp = INT_TO_JSVAL(odi.max_dec_time);
		break;
	case -23:
		*vp = INT_TO_JSVAL(odi.total_dec_time);
		break;
	case -24:
		*vp = INT_TO_JSVAL(odi.avg_bitrate);
		break;
	case -25:
		*vp = INT_TO_JSVAL(odi.max_bitrate);
		break;
	case -26:
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(c, odi.service_handler));
		break;
	case -27:
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(c, odi.codec_name));
		break;
	case -28:
	{
		//first check network
		GF_NetworkCommand com;
		memset(&com, 0, sizeof(GF_NetworkCommand));
		com.base.command_type = GF_NET_SERVICE_QUALITY_QUERY;
		com.base.on_channel = gf_list_get(odm->channels, 0);

		gf_term_service_command(odm->net_service, &com);
		if (com.quality_query.index) {
			*vp = INT_TO_JSVAL(com.quality_query.index);
			break;
		}

	#if 0
		//otherwise use input channels
		if (odm->codec->type==GF_STREAM_VISUAL) {
			*vp = INT_TO_JSVAL(gf_list_count(odm->codec->inChannels));
		} else {
			*vp = INT_TO_JSVAL(0);
		}
	#endif
	}
	break;
	case -29:
		*vp = INT_TO_JSVAL(odi.max_buffer);
		break;
	case -30:
		*vp = INT_TO_JSVAL(odi.min_buffer);
		break;
	case -31:
		*vp = INT_TO_JSVAL(odi.au_duration);
		break;
	case -32:
		*vp = INT_TO_JSVAL(odi.nb_iraps);
		break;
	case -33:
		*vp = INT_TO_JSVAL(odi.irap_total_dec_time);
		break;
	case -34:
		*vp = INT_TO_JSVAL(odi.irap_max_dec_time);
		break;
	case -35:
		*vp = INT_TO_JSVAL(odi.od ? odi.od->ServiceID : 0);
		break;
	case -36:
		*vp = INT_TO_JSVAL( (!odm->addon && odm->subscene) ? odm->subscene->selected_service_id : odm->parentscene->selected_service_id);
		break;
	case -37:
	{
		GF_NetworkCommand com;
		memset(&com, 0, sizeof(GF_NetworkCommand));
		com.base.command_type = GF_NET_GET_STATS;
		com.base.on_channel = gf_list_get(odm->channels, 0);
		gf_term_service_command(odm->net_service, &com);
		*vp = INT_TO_JSVAL(com.net_stats.bw_down);
	}
	break;
	case -38:
		*vp = INT_TO_JSVAL(gf_list_count(odm->net_service->dnloads) );
		break;
	case -39:
		if ((s32) odm->timeshift_depth >= 0) {
			*vp = DOUBLE_TO_JSVAL( JS_NewDouble(c, ((Double) odm->timeshift_depth) / 1000.0 ) );
		} else {
			*vp = DOUBLE_TO_JSVAL( JS_NewDouble(c, -1) );
		}
		break;
	case -40:
	{
		GF_NetworkCommand com;
		GF_Scene *scene;

		if (!odm->timeshift_depth) {
			*vp = INT_TO_JSVAL(0);
			break;
		}

		scene = odm->subscene ? odm->subscene : odm->parentscene;

		memset(&com, 0, sizeof(GF_NetworkCommand));
		com.base.command_type = GF_NET_GET_TIMESHIFT;

		//we may need to check the main addon for timeshifting
		if (scene->main_addon_selected) {
			u32 i, count = gf_list_count(scene->resources);
			for (i=0; i < count; i++) {
				GF_ObjectManager *an_odm = gf_list_get(scene->resources, i);
				if (an_odm && an_odm->addon && (an_odm->addon->addon_type==GF_ADDON_TYPE_MAIN)) {
					odm = an_odm;
				}
			}
		}

		//can be NULL
		com.base.on_channel = gf_list_get(odm->channels, 0);
		gf_term_service_command(odm->net_service, &com);
		*vp = INT_TO_JSVAL(com.timeshift.time);
	}
	break;
	case -41:
		*vp = BOOLEAN_TO_JSVAL( (odm->addon || (!odm->subscene && odm->parentscene->root_od->addon)) ? JS_TRUE : JS_FALSE);
		break;
	case -42:
	{
		GF_Scene *scene = odm->subscene ? odm->subscene : odm->parentscene;
		*vp = BOOLEAN_TO_JSVAL( scene->main_addon_selected ? JS_TRUE : JS_FALSE);
	}
	break;
	case -43:
	{
		GF_Scene *scene = odm->subscene ? odm->subscene : odm->parentscene;
		*vp = BOOLEAN_TO_JSVAL( gf_sc_is_over(odm->term->compositor, scene->graph) ? JS_TRUE : JS_FALSE);
	}
	break;
	case -44:
	{
		GF_Channel *ch = gf_list_get(odm->channels, 0);
		*vp = BOOLEAN_TO_JSVAL((ch && ch->is_pulling) ? JS_TRUE : JS_FALSE);
	}
	break;
	case -45:
		*vp = BOOLEAN_TO_JSVAL(odm->subscene && odm->subscene->is_dynamic_scene ? JS_TRUE : JS_FALSE);
		break;
	case -46:
	{
		GF_NetworkCommand com;
		memset(&com, 0, sizeof(GF_NetworkCommand));
		com.base.command_type = GF_NET_SERVICE_INFO;
		com.info.service_id = odi.od->ServiceID;
		gf_term_service_command(odm->net_service, &com);
		if (com.info.name) {
			*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, com.info.name ) );
		} else {
			*vp = JSVAL_NULL;
		}
	}
	break;
	case -47:
		*vp = INT_TO_JSVAL( odi.ntp_diff);
		break;

	case -48:
	{
		GF_Scene *scene = odm->subscene ? odm->subscene : odm->parentscene;
		u32 i, count = gf_list_count(scene->resources);
		*vp = JSVAL_NULL;
		for (i=0; i < count; i++) {
			GF_ObjectManager *an_odm = gf_list_get(scene->resources, i);
			if (an_odm && an_odm->addon && (an_odm->addon->addon_type==GF_ADDON_TYPE_MAIN)) {
				if (!strstr(an_odm->addon->url, "://")) {
					char szURL[GF_MAX_PATH];
					sprintf(szURL, "gpac://%s", an_odm->addon->url);
					*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, szURL) );
				} else {
					*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, an_odm->addon->url) );
				}
			}
		}
	}
	break;
	}
return JS_TRUE;
}

static JSBool SMJS_FUNCTION(gjs_odm_get_quality)
{
	GF_NetworkCommand com;
	SMJS_OBJ
	SMJS_ARGS
	GF_ObjectManager *odm = (GF_ObjectManager *)SMJS_GET_PRIVATE(c, obj);

	if (!odm) return JS_TRUE;
	if (argc<1) return JS_TRUE;
	if (! JSVAL_IS_INT(argv[0]) ) return JS_TRUE;

	memset(&com, 0, sizeof(GF_NetworkCommand));
	com.base.command_type = GF_NET_SERVICE_QUALITY_QUERY;
	com.quality_query.index = 1 + JSVAL_TO_INT(argv[0]);
	com.base.on_channel = gf_list_get(odm->channels, 0);

	if (gf_term_service_command(odm->net_service, &com) == GF_OK) {
		JSObject *a = JS_NewObject(c, NULL, NULL, NULL);

		JS_DefineProperty(c, a, "ID", STRING_TO_JSVAL(JS_NewStringCopyZ(c, com.quality_query.ID ? com.quality_query.ID : "")) , 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, a, "mime", STRING_TO_JSVAL(JS_NewStringCopyZ(c, com.quality_query.mime ? com.quality_query.mime : "")) , 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, a, "codec", STRING_TO_JSVAL(JS_NewStringCopyZ(c, com.quality_query.codec ? com.quality_query.codec : "")) , 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, a, "width", INT_TO_JSVAL(com.quality_query.width), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, a, "height", INT_TO_JSVAL(com.quality_query.height), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, a, "bandwidth", INT_TO_JSVAL(com.quality_query.bandwidth), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, a, "interlaced", BOOLEAN_TO_JSVAL(com.quality_query.interlaced), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, a, "fps", DOUBLE_TO_JSVAL( JS_NewDouble(c, com.quality_query.fps) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, a, "samplerate", INT_TO_JSVAL(com.quality_query.sample_rate), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, a, "channels", INT_TO_JSVAL(com.quality_query.nb_channels), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, a, "par_num", INT_TO_JSVAL(com.quality_query.par_num), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, a, "par_den", INT_TO_JSVAL(com.quality_query.par_den), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, a, "disabled", BOOLEAN_TO_JSVAL(com.quality_query.disabled), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, a, "is_selected", BOOLEAN_TO_JSVAL(com.quality_query.is_selected), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, a, "automatic", BOOLEAN_TO_JSVAL(com.quality_query.automatic), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);

		SMJS_SET_RVAL( OBJECT_TO_JSVAL(a) );
	} else {
		SMJS_SET_RVAL(JSVAL_NULL);
	}

	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(gjs_odm_select_quality)
{
	GF_NetworkCommand com;
	char *ID = NULL;
	SMJS_OBJ
	SMJS_ARGS
	GF_ObjectManager *odm = (GF_ObjectManager *)SMJS_GET_PRIVATE(c, obj);

	if (!odm) return JS_TRUE;

	if ((argc==1) && JSVAL_IS_STRING(argv[0])) {
		ID = SMJS_CHARS(c, argv[0]);
	}
	if (!ID) {
		return JS_TRUE;
	}

	memset(&com, 0, sizeof(GF_NetworkCommand));
	com.base.command_type = GF_NET_SERVICE_QUALITY_SWITCH;
	com.base.on_channel = gf_list_get(odm->channels, 0);
	if (!strcmp(ID, "auto")) {
		com.switch_quality.set_auto = 1;
	} else {
		com.switch_quality.ID = ID;
	}

	gf_term_service_command(odm->net_service, &com);

	SMJS_FREE(c, ID);
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(gjs_odm_disable_main_addon)
{
	SMJS_OBJ
	GF_ObjectManager *odm = (GF_ObjectManager *)SMJS_GET_PRIVATE(c, obj);

	if (!odm || !odm->subscene || !odm->subscene->main_addon_selected) return JS_TRUE;

	gf_scene_resume_live(odm->subscene);
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(gjs_odm_select_service)
{
	u32 sid;
	SMJS_OBJ
	SMJS_ARGS
	GF_ObjectManager *odm = (GF_ObjectManager *)SMJS_GET_PRIVATE(c, obj);

	if (!odm) return JS_TRUE;
	if (argc<1) return JS_TRUE;
	if (! JSVAL_IS_INT(argv[0]) ) return JS_TRUE;
	sid = JSVAL_TO_INT(argv[0]);

	gf_term_select_service(odm->term, odm->subscene ? odm : odm->parentscene->root_od, sid);
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(gjs_odm_get_resource)
{
	GF_ObjectManager *an_odm = NULL;
	u32 idx;
	SMJS_OBJ
	SMJS_ARGS
	GF_ObjectManager *odm = (GF_ObjectManager *)SMJS_GET_PRIVATE(c, obj);

	if (!odm) return JS_TRUE;
	if (argc<1) return JS_TRUE;
	if (! JSVAL_IS_INT(argv[0]) ) return JS_TRUE;
	idx = JSVAL_TO_INT(argv[0]);

	if (odm->subscene) {
		an_odm = gf_list_get(odm->subscene->resources, idx);
	}
	if (an_odm && an_odm->net_service) {
		JSObject *anobj;
		JSClass *_class = JS_GET_CLASS(c, obj);
		anobj = JS_NewObject(c, _class, 0, 0);
		SMJS_SET_PRIVATE(c, anobj, an_odm);
		SMJS_SET_RVAL( OBJECT_TO_JSVAL(anobj) );
	} else {
		SMJS_SET_RVAL(JSVAL_NULL);
	}
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(gjs_odm_addon_layout)
{
	u32 pos, size;
	SMJS_OBJ
	SMJS_ARGS
	GF_ObjectManager *odm = (GF_ObjectManager *)SMJS_GET_PRIVATE(c, obj);

	if (argc<2) return JS_TRUE;
	if (! JSVAL_IS_INT(argv[0]) ) return JS_TRUE;
	if (! JSVAL_IS_INT(argv[1]) ) return JS_TRUE;
	pos = JSVAL_TO_INT(argv[0]);
	size = JSVAL_TO_INT(argv[1]);

	gf_scene_set_addon_layout_info(odm->subscene, pos, size);
	return JS_TRUE;
}

static void do_enable_addon(GF_ObjectManager *odm, char *addon_url, Bool enable_if_defined )
{
	GF_AssociatedContentLocation addon_info;
	memset(&addon_info, 0, sizeof(GF_AssociatedContentLocation));
	addon_info.external_URL = addon_url;
	addon_info.timeline_id = -100;
	addon_info.enable_if_defined = enable_if_defined;
	gf_scene_register_associated_media(odm->subscene ? odm->subscene : odm->parentscene, &addon_info);
}

static JSBool SMJS_FUNCTION(gjs_odm_enable_addon)
{
	char *addon_url = NULL;
	SMJS_OBJ
	SMJS_ARGS
	GF_ObjectManager *odm = (GF_ObjectManager *)SMJS_GET_PRIVATE(c, obj);

	if (! JSVAL_IS_STRING(argv[0]) ) return JS_TRUE;

	addon_url = SMJS_CHARS(c, argv[0]);
	if (addon_url) {
		do_enable_addon(odm, addon_url, GF_TRUE);
	}
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(gjs_odm_declare_addon)
{
	char *addon_url = NULL;
	SMJS_OBJ
	SMJS_ARGS
	GF_ObjectManager *odm = (GF_ObjectManager *)SMJS_GET_PRIVATE(c, obj);

	if (! JSVAL_IS_STRING(argv[0]) ) return JS_TRUE;

	addon_url = SMJS_CHARS(c, argv[0]);
	if (addon_url) {
		do_enable_addon(odm, addon_url, GF_FALSE);
	}
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(gpac_get_object_manager)
{
	JSObject *anobj;
	u32 i, count;
	GF_ObjectManager *odm = NULL;
	char *service_url = NULL;
	SMJS_OBJ
	SMJS_ARGS
	GF_GPACJSExt *gjs = (GF_GPACJSExt *)SMJS_GET_PRIVATE(c, obj);
	GF_Terminal *term = gpac_get_term(c, obj);
	GF_Scene *scene = term->root_scene;

	if (JSVAL_IS_STRING(argv[0]) ) {
		char *url;
		url = service_url = SMJS_CHARS(c, argv[0]);
		if (!service_url) {
			SMJS_SET_RVAL(JSVAL_NULL);
			return JS_TRUE;
		}
		if (!strncmp(service_url, "gpac://", 7)) url = service_url + 7;
		count = gf_list_count(scene->resources);
		for (i=0; i<count; i++) {
			odm = gf_list_get(scene->resources, i);
			if (odm->net_service && !strcmp(odm->net_service->url, url)) break;
			odm = NULL;
		}
	}

	SMJS_FREE(c, service_url);

	if (!odm) {
		SMJS_SET_RVAL(JSVAL_NULL);
		return JS_TRUE;
	}

	anobj = JS_NewObject(c, &gjs->odmClass._class, 0, 0);
	SMJS_SET_PRIVATE(c, anobj, odm);

	SMJS_SET_RVAL( OBJECT_TO_JSVAL(anobj) );
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

		SMJS_PROPERTY_SPEC("last_working_directory",	-1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED , 0, 0),
		SMJS_PROPERTY_SPEC("scale_x",					-2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("scale_y",					-3,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("translation_x",				-4,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("translation_y",				-5,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("rectangular_textures",		-6,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("batteryOn",					-7,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("batteryCharging",			-8,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("batteryPercent",			-9,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("batteryLifeTime",			-10,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("batteryFullLifeTime",		-11,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("hostname",					-12,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("fullscreen",				-13,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
		SMJS_PROPERTY_SPEC("current_path",				-14,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("volume",					-15,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED , 0, 0),
		SMJS_PROPERTY_SPEC("navigation",				-16,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED , 0, 0),
		SMJS_PROPERTY_SPEC("navigation_type",			-17,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED , 0, 0),
		SMJS_PROPERTY_SPEC("hardware_yuv",				-18,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("hardware_rgb",				-19,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("hardware_rgba",				-20,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("hardware_stretch",			-21,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("screen_width",				-22,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("screen_height",				-23,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("http_max_bitrate",			-24,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED , 0, 0),
		SMJS_PROPERTY_SPEC("http_bitrate",				-25,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("fps",						-26,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("cpu_load",					-27,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("cpu",						-27,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("nb_cores",					-28,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("system_memory",				-29,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("memory",					-30,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("argc",						-31,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("caption",					-32,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
		SMJS_PROPERTY_SPEC("disable_hardware_blit",		-33,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
		SMJS_PROPERTY_SPEC("disable_composite_blit",	-34,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
		SMJS_PROPERTY_SPEC("focus_highlight",			-35,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
		SMJS_PROPERTY_SPEC("dpi_x",						-36,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("dpi_y",						-37,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),

		SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0)
	};
	JSFunctionSpec gpacClassFuncs[] = {
		SMJS_FUNCTION_SPEC("getOption",		gpac_getOption, 3),
		SMJS_FUNCTION_SPEC("setOption",		gpac_setOption, 4),
		SMJS_FUNCTION_SPEC("get_option",		gpac_getOption, 3),
		SMJS_FUNCTION_SPEC("set_option",		gpac_setOption, 4),
		SMJS_FUNCTION_SPEC("get_arg",		gpac_get_arg, 1),
		SMJS_FUNCTION_SPEC("enum_directory",	gpac_enum_directory, 1),
		SMJS_FUNCTION_SPEC("set_size",		gpac_set_size, 1),
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

	JSPropertySpec odmClassProps[] = {
		SMJS_PROPERTY_SPEC("ID",				-1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("nb_resources",			-2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("service_url",		-3,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("duration",			-4,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("clock_time",		-5,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("clock_drift",		-6,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("status",			-7,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("buffer",			-8,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("db_unit_count",		-9,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("cb_unit_count",		-10,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("cb_capacity",		-11,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("type",				-12,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("samplerate",		-13,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("channels",			-14,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("lang",				-15,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("width",				-16,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("height",			-17,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("pixelformt",		-18,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("par",				-19,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("dec_frames",		-20,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("drop_frames",		-21,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("max_dec_time",		-22,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("total_dec_time",	-23,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("avg_bitrate",		-24,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("max_bitrate",		-25,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("service_handler",	-26,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("codec",				-27,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("nb_qualities",		-28,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("max_buffer",		-29,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("min_buffer",		-30,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("frame_duration",	-31,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("irap_frames",		-32,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("irap_dec_time",		-33,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("irap_max_time",		-34,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("service_id",		-35,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("selected_service",	-36,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("bandwidth_down",	-37,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("nb_http",			-38,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("timeshift_depth",	-39,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("timeshift_time",	-40,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("is_addon",			-41,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("main_addon_on",		-42,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("is_over",			-43,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("is_pulling",		-44,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("dynamic_scene",		-45,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("service_name",		-46,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("ntp_diff",			-47,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
		SMJS_PROPERTY_SPEC("main_addon_url",	-48,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),


		SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0)
	};

	JSFunctionSpec odmClassFuncs[] = {
		SMJS_FUNCTION_SPEC("declare_addon", gjs_odm_declare_addon, 1),
		SMJS_FUNCTION_SPEC("enable_addon", gjs_odm_enable_addon, 1),
		SMJS_FUNCTION_SPEC("addon_layout", gjs_odm_addon_layout, 1),
		SMJS_FUNCTION_SPEC("get_resource", gjs_odm_get_resource, 1),
		SMJS_FUNCTION_SPEC("get_quality", gjs_odm_get_quality, 1),
		SMJS_FUNCTION_SPEC("select_service", gjs_odm_select_service, 1),
		SMJS_FUNCTION_SPEC("select_quality", gjs_odm_select_quality, 1),
		SMJS_FUNCTION_SPEC("disable_main_addon", gjs_odm_disable_main_addon, 1),

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

		JS_SETUP_CLASS(gjs->odmClass, "ODM", JSCLASS_HAS_PRIVATE, odm_getProperty, JS_PropertyStub_forSetter, JS_FinalizeStub);
		GF_JS_InitClass(c, global, 0, &gjs->odmClass, 0, 0, odmClassProps, odmClassFuncs, 0, 0);
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
		DECLARE_GPAC_CONST(GF_EVENT_DBLCLICK);
		DECLARE_GPAC_CONST(GF_EVENT_KEYUP);
		DECLARE_GPAC_CONST(GF_EVENT_KEYDOWN);
		DECLARE_GPAC_CONST(GF_EVENT_TEXTINPUT);
		DECLARE_GPAC_CONST(GF_EVENT_CONNECT);
		DECLARE_GPAC_CONST(GF_EVENT_NAVIGATE_INFO);
		DECLARE_GPAC_CONST(GF_EVENT_NAVIGATE);
		DECLARE_GPAC_CONST(GF_EVENT_DROPFILE);
		DECLARE_GPAC_CONST(GF_EVENT_ADDON_DETECTED);
		DECLARE_GPAC_CONST(GF_EVENT_QUALITY_SWITCHED);
		DECLARE_GPAC_CONST(GF_EVENT_TIMESHIFT_DEPTH);
		DECLARE_GPAC_CONST(GF_EVENT_TIMESHIFT_UPDATE);
		DECLARE_GPAC_CONST(GF_EVENT_TIMESHIFT_OVERFLOW);
		DECLARE_GPAC_CONST(GF_EVENT_TIMESHIFT_UNDERRUN);
		DECLARE_GPAC_CONST(GF_EVENT_QUIT);

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

GPAC_MODULE_STATIC_DECLARATION( gpac_js )
