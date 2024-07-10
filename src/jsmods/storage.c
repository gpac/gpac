/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2007-2023
 *			All rights reserved
 *
 *  This file is part of GPAC / JavaScript Storage bindings
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
	ANY CHANGE TO THE API MUST BE REFLECTED IN THE DOCUMENTATION IN gpac/share/doc/idl/storage.js
	(no way to define inline JS doc with doxygen)
*/

#include <gpac/setup.h>

#ifdef GPAC_HAS_QJS

#include <gpac/config_file.h>
#include "../scenegraph/qjs_common.h"


static JSClassID storage_class_id = 0;
GF_List *all_storages = NULL;

static void storage_finalize(JSRuntime *rt, JSValue obj)
{
	GF_Config *cfg = JS_GetOpaque(obj, storage_class_id);
	if (!cfg) return;
	if (all_storages) {
		gf_list_del_item(all_storages, cfg);
		if (!gf_list_count(all_storages)) {
			gf_list_del(all_storages);
			all_storages = NULL;
		}
	}
	gf_cfg_del(cfg);
}

JSClassDef storageClass = {
    "Storage",
    .finalizer = storage_finalize,
};


static JSValue js_storage_get_option(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *opt = NULL;
	const char *sec_name, *key_name;
	s32 idx = -1;
	GF_Config *config = JS_GetOpaque(this_val, storage_class_id);
	if (!config) return GF_JS_EXCEPTION(ctx);
	if (argc < 2) return GF_JS_EXCEPTION(ctx);

	if (!JS_IsString(argv[0])) return GF_JS_EXCEPTION(ctx);
	if (!JS_IsString(argv[1]) && !JS_IsInteger(argv[1])) return GF_JS_EXCEPTION(ctx);

	sec_name = JS_ToCString(ctx, argv[0]);
	if (!strcmp(sec_name, "GPAC")) {
		JS_FreeCString(ctx, sec_name);
		return js_throw_err_msg(ctx, GF_BAD_PARAM, "Cannot access section 'GPAC' from script\n");
	}

	key_name = NULL;
	if (JS_IsInteger(argv[1])) {
		JS_ToInt32(ctx, &idx, argv[1]);
	} else if (JS_IsString(argv[1]) ) {
		key_name = JS_ToCString(ctx, argv[1]);
	}

	if (key_name) {
		opt = gf_cfg_get_key(config, sec_name, key_name);
	} else if (idx>=0) {
		opt = gf_cfg_get_key_name(config, sec_name, idx);
	} else {
		opt = NULL;
	}

	JS_FreeCString(ctx, key_name);
	JS_FreeCString(ctx, sec_name);

	if (opt) {
		return JS_NewString(ctx, opt);
	}
	return JS_NULL;
}

static JSValue js_storage_set_option(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *sec_name, *key_name, *key_val;
	GF_Config *config = JS_GetOpaque(this_val, storage_class_id);
	if (!config) return GF_JS_EXCEPTION(ctx);
	if (argc < 3) return GF_JS_EXCEPTION(ctx);

	if (!JS_IsString(argv[0])) return GF_JS_EXCEPTION(ctx);
	if (!JS_IsString(argv[1])) return GF_JS_EXCEPTION(ctx);

	sec_name = JS_ToCString(ctx, argv[0]);
	if (!strcmp(sec_name, "GPAC")) {
		JS_FreeCString(ctx, sec_name);
		return js_throw_err_msg(ctx, GF_BAD_PARAM, "Cannot access section 'GPAC' from script\n");
	}
	key_name = JS_ToCString(ctx, argv[1]);
	key_val = NULL;
	if (JS_IsString(argv[2]))
		key_val = JS_ToCString(ctx, argv[2]);

	gf_cfg_set_key(config, sec_name, key_name, key_val);

	JS_FreeCString(ctx, sec_name);
	JS_FreeCString(ctx, key_name);
	JS_FreeCString(ctx, key_val);
	return JS_UNDEFINED;
}

static JSValue js_storage_save(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	GF_Config *config = JS_GetOpaque(this_val, storage_class_id);
	if (!config) return GF_JS_EXCEPTION(ctx);
	gf_cfg_save(config);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry storage_funcs[] = {
	JS_CFUNC_DEF("get_option", 0, js_storage_get_option),
	JS_CFUNC_DEF("set_option", 0, js_storage_set_option),
	JS_CFUNC_DEF("save", 0, js_storage_save),
};

static JSValue storage_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	char szFile[GF_MAX_PATH];
	JSValue anobj;
	GF_Config *storage = NULL;
	const char *storage_url = NULL;
	u32 i, count;
	u8 hash[20];
	char temp[3];

	if (!JS_IsString(argv[0]) )
		return GF_JS_EXCEPTION(ctx);

	storage_url = JS_ToCString(ctx, argv[0]);
	if (!storage_url) return JS_NULL;

	szFile[0]=0;
	gf_sha1_csum((u8 *)storage_url, (u32) strlen(storage_url), hash);
	for (i=0; i<20; i++) {
		sprintf(temp, "%02X", hash[i]);
		strcat(szFile, temp);
	}
	strcat(szFile, ".cfg");

	count = gf_list_count(all_storages);
	for (i=0; i<count; i++) {
		GF_Config *a_cfg = gf_list_get(all_storages, i);
		const char *cfg_name = gf_cfg_get_filename(a_cfg);

		if (strstr(cfg_name, szFile)) {
			storage = a_cfg;
			break;
		}
	}

	if (!storage) {
		const char *storage_dir = gf_opts_get_key("core", "store-dir");

		storage = gf_cfg_force_new(storage_dir, szFile);
		if (storage) {
			gf_cfg_set_key(storage, "GPAC", "StorageURL", storage_url);
			gf_list_add(all_storages, storage);
		}
	}

	JS_FreeCString(ctx, storage_url);

	anobj = JS_NewObjectClass(ctx, storage_class_id);
	if (JS_IsException(anobj)) return anobj;
	JS_SetOpaque(anobj, storage);
	return anobj;
}

static int js_storage_init(JSContext *c, JSModuleDef *m)
{
	if (!storage_class_id) {
		JS_NewClassID(&storage_class_id);
		JS_NewClass(JS_GetRuntime(c), storage_class_id, &storageClass);

		gf_assert(!all_storages);
		all_storages = gf_list_new();
	}

	JSValue proto = JS_NewObjectClass(c, storage_class_id);
	JS_SetPropertyFunctionList(c, proto, storage_funcs, countof(storage_funcs));
	JS_SetClassProto(c, storage_class_id, proto);

	JSValue ctor = JS_NewCFunction2(c, storage_constructor, "Storage", 1, JS_CFUNC_constructor, 0);
    JS_SetModuleExport(c, m, "Storage", ctor);
	return 0;
}


void qjs_module_init_storage(JSContext *ctx)
{
	JSModuleDef *m;
	m = JS_NewCModule(ctx, "storage", js_storage_init);
	if (!m) return;

	JS_AddModuleExport(ctx, m, "Storage");
	return;
}


#endif

