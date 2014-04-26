/*
 *          GPAC - Multimedia Framework C SDK
 *
 *          Authors: Cyril Concolato
 *          Copyright (c) Telecom ParisTech 2007-2012
 *          All rights reserved
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
#include <gpac/internal/scenegraph_dev.h>
#include <gpac/webvtt.h>

#ifdef GPAC_HAS_SPIDERMONKEY

#include <gpac/internal/smjs_api.h>

GF_EXPORT
GF_Err gf_webvtt_js_addCue(GF_Node *node, const char *id,
                           const char *start, const char *end,
                           const char *settings,
                           const char *payload)
{
	GF_Err e;
	JSBool found;
	JSContext *c = node->sgprivate->scenegraph->svg_js->js_ctx;
	JSObject *global = node->sgprivate->scenegraph->svg_js->global;
	jsval fun_val;

	gf_sg_lock_javascript(c, GF_TRUE);
	found = JS_LookupProperty(c, global, "addCue", &fun_val);
	if (!found || JSVAL_IS_VOID(fun_val) || !JSVAL_IS_OBJECT(fun_val) ) {
		e = GF_BAD_PARAM;
	} else {
		JSBool ret;
		uintN attr;
		ret = JS_GetPropertyAttributes(c, global, "addCue", &attr, &found);
		if (ret == JS_TRUE && found == JS_TRUE) {
			jsval rval;
			jsval argv[5];
			argv[0] = STRING_TO_JSVAL( JS_NewStringCopyZ(c, (id ? id : "")) );
			argv[1] = STRING_TO_JSVAL( JS_NewStringCopyZ(c, (start ? start : "")) );
			argv[2] = STRING_TO_JSVAL( JS_NewStringCopyZ(c, (end ? end : "")) );
			argv[3] = STRING_TO_JSVAL( JS_NewStringCopyZ(c, (settings ? settings : "")) );
			argv[4] = STRING_TO_JSVAL( JS_NewStringCopyZ(c, (payload ? payload : "")) );

			ret = JS_CallFunctionValue(c, global, fun_val, 5, argv, &rval);
			//ret = JS_CallFunctionName(c, global, "addCue", 5, argv, &rval);
			if (ret == JS_TRUE) {
				e = GF_OK;
			} else {
				e = GF_BAD_PARAM;
			}
		} else {
			e = GF_BAD_PARAM;
		}
	}
	gf_sg_lock_javascript(c, GF_FALSE);
	return e;
}

GF_EXPORT
GF_Err gf_webvtt_js_removeCues(GF_Node *node)
{
	GF_Err e;
	JSBool found;
	JSContext *c = node->sgprivate->scenegraph->svg_js->js_ctx;
	JSObject *global = node->sgprivate->scenegraph->svg_js->global;
	jsval fun_val;

	gf_sg_lock_javascript(c, GF_TRUE);
	found = JS_LookupProperty(c, global, "removeCues", &fun_val);
	if (!found || JSVAL_IS_VOID(fun_val) || !JSVAL_IS_OBJECT(fun_val) ) {
		e = GF_BAD_PARAM;
	} else {
		JSBool ret;
		uintN attr;
		ret = JS_GetPropertyAttributes(c, global, "removeCues", &attr, &found);
		if (ret == JS_TRUE && found == JS_TRUE) {
			jsval rval;
			ret = JS_CallFunctionValue(c, global, fun_val, 0, NULL, &rval);
			if (ret == JS_TRUE) {
				e = GF_OK;
			} else {
				e = GF_BAD_PARAM;
			}
		} else {
			e = GF_BAD_PARAM;
		}
	}
	gf_sg_lock_javascript(c, GF_FALSE);
	return e;
}

#else
GF_EXPORT
GF_Err gf_webvtt_js_addCue(GF_Node *node, const char *id,
                           const char *start, const char *end,
                           const char *settings,
                           const char *payload)
{
	return GF_BAD_PARAM;
}

GF_EXPORT
GF_Err gf_webvtt_js_removeCues()
{
	return GF_BAD_PARAM;
}
#endif  /*GPAC_HAS_SPIDERMONKEY*/
