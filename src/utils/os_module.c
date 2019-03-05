/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / common tools sub-project
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

#include "module_wrap.h"
#include <gpac/network.h>

#if defined(WIN32) || defined(_WIN32_WCE)
#include <windows.h>
#else
#include <sys/stat.h>
#include <dlfcn.h>
#include <dirent.h>
#endif


/*delete all interfaces loaded on object*/
void gf_modules_free_module(ModuleInstance *inst)
{
	void *objinterface;
	while (gf_list_count(inst->interfaces)) {
		objinterface = gf_list_get(inst->interfaces, 0);
		gf_list_rem(inst->interfaces, 0);
		inst->destroy_func(objinterface);
	}

#ifdef WIN32
	if (inst->lib_handle) FreeLibrary((HMODULE)inst->lib_handle);
#else
	if (inst->lib_handle) dlclose(inst->lib_handle);
#endif
	if (inst->interfaces)
		gf_list_del(inst->interfaces);
	inst->interfaces = NULL;

	if (inst->name && !inst->ifce_reg) {
		gf_free(inst->name);
		inst->name = NULL;
	}

	if (inst->dir) {
		gf_free(inst->dir);
		inst->dir = NULL;
	}
	gf_free(inst);
}


Bool gf_modules_load_library(ModuleInstance *inst)
{
#ifdef WIN32
	DWORD res;

#ifdef _WIN32_WCE
	char s_path[GF_MAX_PATH];
	unsigned short path[GF_MAX_PATH];
#else
	char path[GF_MAX_PATH];
#endif

#else
	char path[GF_MAX_PATH];
	s32 _flags;
	const char * error;
#endif

	if (inst->lib_handle) return GF_TRUE;

	if (inst->ifce_reg) {
		inst->query_func = (QueryInterfaces) inst->ifce_reg->QueryInterfaces;
		inst->load_func = (LoadInterface) inst->ifce_reg->LoadInterface;
		inst->destroy_func = (ShutdownInterface) inst->ifce_reg->ShutdownInterface;
		return GF_TRUE;
	}

	GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("[Core] Load module file %s\n", inst->name));

#if _WIN32_WINNT >= 0x0502
	SetDllDirectory(inst->dir);
#endif

#ifdef _WIN32_WCE
	sprintf(s_path, "%s%c%s", inst->dir, GF_PATH_SEPARATOR, inst->name);
	CE_CharToWide(s_path, path);
#else
	sprintf(path, "%s%c%s", inst->dir, GF_PATH_SEPARATOR, inst->name);
#endif

#ifdef WIN32
	inst->lib_handle = LoadLibrary(path);
	if (!inst->lib_handle) {
		res = GetLastError();
#ifdef _WIN32_WCE
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] Cannot load module file %s: error %d\n", s_path, res));
#else
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] Cannot load module file %s: error %d\n", path, res));
#endif
		return GF_FALSE;
	}
#if defined(_WIN32_WCE)
	inst->query_func = (QueryInterfaces) GetProcAddress(inst->lib_handle, _T("QueryInterfaces"));
	inst->load_func = (LoadInterface) GetProcAddress(inst->lib_handle, _T("LoadInterface"));
	inst->destroy_func = (ShutdownInterface) GetProcAddress(inst->lib_handle, _T("ShutdownInterface"));
#else
	inst->query_func = (QueryInterfaces) GetProcAddress((HMODULE)inst->lib_handle, "QueryInterfaces");
	inst->load_func = (LoadInterface) GetProcAddress((HMODULE)inst->lib_handle, "LoadInterface");
	inst->destroy_func = (ShutdownInterface) GetProcAddress((HMODULE)inst->lib_handle, "ShutdownInterface");
#endif

#else

#ifdef RTLD_GLOBAL
	_flags =RTLD_LAZY | RTLD_GLOBAL;
#else
	_flags =RTLD_LAZY;
#endif
	inst->lib_handle = dlopen(path, _flags);
	if (!inst->lib_handle) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] Cannot load module file %s, error is %s\n", path, dlerror()));
		return 0;
	}
	error = dlerror();    /* Clear any existing error */
	if (error)
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Core] Cleaning up previous dlerror %s\n", error));
	inst->query_func = (QueryInterfaces) dlsym(inst->lib_handle, "QueryInterfaces");
	error = dlerror();
	if (error)
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] Cannot resolve symbol QueryInterfaces in module file %s, error is %s\n", path, error));
	inst->load_func = (LoadInterface) dlsym(inst->lib_handle, "LoadInterface");
	error = dlerror();
	if (error)
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] Cannot resolve symbol LoadInterface in module file %s, error is %s\n", path, error));
	inst->destroy_func = (ShutdownInterface) dlsym(inst->lib_handle, "ShutdownInterface");
	error = dlerror();
	if (error)
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] Cannot resolve symbol ShutdownInterface in module file %s, error is %s\n", path, error));
#endif
	GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("[Core] Load module file %s : DONE\n", inst->name));
	return GF_TRUE;
}

void gf_modules_unload_library(ModuleInstance *inst)
{
	if (!inst->lib_handle || gf_list_count(inst->interfaces)) return;
	/*module unloading is disabled*/
	if (inst->plugman->no_unload)
		return;

#ifdef WIN32
	if (strcmp(inst->name, "gm_openhevc_dec.dll"))
		FreeLibrary((HMODULE)inst->lib_handle);
#else
	dlclose(inst->lib_handle);
#endif
	inst->lib_handle = NULL;
	inst->load_func = NULL;
	inst->destroy_func = NULL;
	inst->query_func = NULL;
}


static Bool enum_modules(void *cbck, char *item_name, char *item_path, GF_FileEnumInfo *file_info)
{
	ModuleInstance *inst;
#if CHECK_MODULE
	QueryInterface query_func;
	LoadInterface load_func;
	ShutdownInterface del_func;
#ifdef WIN32
	HMODULE ModuleLib;
#else
	void *ModuleLib;
	s32 _flags;
#endif

#endif

	GF_ModuleManager *pm = (GF_ModuleManager*)cbck;

	if (strstr(item_name, "nposmozilla")) return GF_FALSE;
	if (strncmp(item_name, "gm_", 3) && strncmp(item_name, "libgm_", 6)) return GF_FALSE;
	if (gf_module_is_loaded(pm, item_name) ) return GF_FALSE;

#if CHECK_MODULE

#ifdef WIN32
	ModuleLib = LoadLibrary(item_path);
	if (!ModuleLib) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] Cannot load module file %s\n", item_name));
		return GF_FALSE;
	}

#ifdef _WIN32_WCE
	query_func = (QueryInterface) GetProcAddress(ModuleLib, _T("QueryInterface"));
	load_func = (LoadInterface) GetProcAddress(ModuleLib, _T("LoadInterface"));
	del_func = (ShutdownInterface) GetProcAddress(ModuleLib, _T("ShutdownInterface"));
#else
	query_func = (QueryInterface) GetProcAddress(ModuleLib, "QueryInterface");
	load_func = (LoadInterface) GetProcAddress(ModuleLib, "LoadInterface");
	del_func = (ShutdownInterface) GetProcAddress(ModuleLib, "ShutdownInterface");
#endif
	FreeLibrary(ModuleLib);

#else

#ifdef RTLD_GLOBAL
	_flags =RTLD_LAZY | RTLD_GLOBAL;
#else
	_flags =RTLD_LAZY;
#endif

	ModuleLib = dlopen(item_name, _flags);
	if (!ModuleLib) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] Cannot load module file %s, error is %s\n", item_name, dlerror()));
		goto next;
	}

	query_func = (QueryInterface) dlsym(ModuleLib, "QueryInterface");
	load_func = (LoadInterface) dlsym(ModuleLib, "LoadInterface");
	del_func = (ShutdownInterface) dlsym(ModuleLib, "ShutdownInterface");
	dlclose(ModuleLib);
#endif

	if (!load_func || !query_func || !del_func) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CORE,
		       ("[Core] Could not find some signatures in module %s: QueryInterface=%p, LoadInterface=%p, ShutdownInterface=%p\n",
		        item_name, load_func, query_func, del_func));
		return GF_FALSE;
	}
#endif

	GF_SAFEALLOC(inst, ModuleInstance);
	if (!inst) return GF_FALSE;
	inst->interfaces = gf_list_new();
	if (!inst->interfaces) {
		gf_free(inst);
		return GF_FALSE;
	}
	inst->plugman = pm;
	inst->name = gf_strdup(item_name);
	inst->dir = gf_strdup(item_path);
	gf_url_get_resource_path(item_path, inst->dir);
	GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("[Core] Added module %s.\n", inst->name));
	gf_list_add(pm->plug_list, inst);
	return GF_FALSE;
}

static void load_static_modules(GF_ModuleManager *pm)
{
	ModuleInstance *inst;
	u32 i, count;
	count = gf_list_count(pm->plugin_registry);
	for (i=0; i<count; i++) {
		GF_InterfaceRegister *ifce_reg = (GF_InterfaceRegister*)gf_list_get(pm->plugin_registry, i);
		if (gf_module_is_loaded(pm, (char *) ifce_reg->name) ) continue;

		GF_SAFEALLOC(inst, ModuleInstance);
		if (!inst) continue;
		inst->interfaces = gf_list_new();
		if (!inst->interfaces) {
			gf_free(inst);
			continue;
		}
		inst->plugman = pm;
		inst->name = (char *) ifce_reg->name;
		inst->ifce_reg = ifce_reg;
		GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("[Core] Added static module %s.\n", inst->name));
		gf_list_add(pm->plug_list, inst);
	}
}

/*refresh modules - note we don't check for deleted modules but since we've open them the OS should forbid delete*/
GF_EXPORT
u32 gf_modules_refresh(GF_ModuleManager *pm)
{
	u32 i;
	if (!pm) return 0;

	/*load all static modules*/
	load_static_modules(pm);

	for (i =0; i < pm->num_dirs; i++) {
#ifdef WIN32
		gf_enum_directory(pm->dirs[i], GF_FALSE, enum_modules, pm, ".dll");
#elif defined(__APPLE__)
#if defined(TARGET_OS_IPHONE) || defined(TARGET_IPHONE_SIMULATOR)
		/*we are in static build for modules by default*/
#else
		gf_enum_directory(pm->dirs[i], 0, enum_modules, pm, ".dylib");
#endif
#else
		GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("Refreshing list of modules in directory %s...\n", pm->dirs[i]));

#if defined(GPAC_CONFIG_WIN32)
		gf_enum_directory(pm->dirs[i], 0, enum_modules, pm, ".dll");
#else
		gf_enum_directory(pm->dirs[i], 0, enum_modules, pm, ".so");
#endif

#endif
	}

	return gf_list_count(pm->plug_list);
}
