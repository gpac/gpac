/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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
	if (inst->lib_handle) FreeLibrary(inst->lib_handle);
#else
	if (inst->lib_handle) dlclose(inst->lib_handle);
#endif
	gf_list_del(inst->interfaces);
	gf_free(inst->name);
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
#endif
	
	if (inst->lib_handle) return 1;
	GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("[Core] Load module file %s\n", inst->name));

#ifdef _WIN32_WCE
	sprintf(s_path, "%s%c%s", inst->plugman->dir, GF_PATH_SEPARATOR, inst->name);
	CE_CharToWide(s_path, path);
#else
	sprintf(path, "%s%c%s", inst->plugman->dir, GF_PATH_SEPARATOR, inst->name);
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
		return 0;
	}
#if defined(_WIN32_WCE)
	inst->query_func = (QueryInterfaces) GetProcAddress(inst->lib_handle, _T("QueryInterfaces"));
	inst->load_func = (LoadInterface) GetProcAddress(inst->lib_handle, _T("LoadInterface"));
	inst->destroy_func = (ShutdownInterface) GetProcAddress(inst->lib_handle, _T("ShutdownInterface"));
#else
	inst->query_func = (QueryInterfaces) GetProcAddress(inst->lib_handle, "QueryInterfaces");
	inst->load_func = (LoadInterface) GetProcAddress(inst->lib_handle, "LoadInterface");
	inst->destroy_func = (ShutdownInterface) GetProcAddress(inst->lib_handle, "ShutdownInterface");
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
	inst->query_func = (QueryInterfaces) dlsym(inst->lib_handle, "QueryInterfaces");
	inst->load_func = (LoadInterface) dlsym(inst->lib_handle, "LoadInterface");
	inst->destroy_func = (ShutdownInterface) dlsym(inst->lib_handle, "ShutdownInterface");
#endif
	return 1;
}

void gf_modules_unload_library(ModuleInstance *inst)
{
	if (!inst->lib_handle || gf_list_count(inst->interfaces)) return;
#ifdef WIN32
	FreeLibrary(inst->lib_handle);
#else
	dlclose(inst->lib_handle);
#endif
	inst->lib_handle = NULL;
	inst->load_func = NULL;
	inst->destroy_func = NULL;
	inst->query_func = NULL;
}


Bool enum_modules(void *cbck, char *item_name, char *item_path)
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

	GF_ModuleManager *pm = cbck;

	if (strstr(item_name, "nposmozilla")) return 0;
	if (strncmp(item_name, "gm_", 3)) return 0;
	if (gf_module_is_loaded(pm, item_name) ) return 0;

#if CHECK_MODULE

#ifdef WIN32
	ModuleLib = LoadLibrary(item_path);
	if (!ModuleLib) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] Cannot load module file %s\n", item_name));
		return 0;
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
	
	ModuleLib = dlopen(file, _flags);
	if (!ModuleLib) goto next;

	query_func = (QueryInterface) dlsym(ModuleLib, "QueryInterface");		
	load_func = (LoadInterface) dlsym(ModuleLib, "LoadInterface");		
	del_func = (ShutdownInterface) dlsym(ModuleLib, "ShutdownInterface");		
	dlclose(ModuleLib);
#endif

	if (!load_func || !query_func || !del_func) return 0;
#endif


	GF_SAFEALLOC(inst, ModuleInstance);
	inst->interfaces = gf_list_new();
	inst->plugman = pm;
	inst->name = gf_strdup(item_name);
	gf_list_add(pm->plug_list, inst);
	return 0;
}

/*refresh modules - note we don't check for deleted modules but since we've open them the OS should forbid delete*/
u32 gf_modules_refresh(GF_ModuleManager *pm)
{
	if (!pm) return 0;

#ifdef WIN32
	gf_enum_directory(pm->dir, 0, enum_modules, pm, ".dll");
#elif defined(__APPLE__)
#if defined(TARGET_OS_IPHONE) || defined(TARGET_IPHONE_SIMULATOR)
	{
		ModuleInstance *inst;		
		GF_SAFEALLOC(inst, ModuleInstance);
		inst->interfaces = gf_list_new();
		inst->plugman = pm;
		inst->name = gf_strdup("gm_sdl_out.dylib");
		gf_list_add(pm->plug_list, inst);
	}
	{
		ModuleInstance *inst;		
		GF_SAFEALLOC(inst, ModuleInstance);
		inst->interfaces = gf_list_new();
		inst->plugman = pm;
		inst->name = gf_strdup("gm_soft_raster.dylib");
		gf_list_add(pm->plug_list, inst);
	}
	{
		ModuleInstance *inst;		
		GF_SAFEALLOC(inst, ModuleInstance);
		inst->interfaces = gf_list_new();
		inst->plugman = pm;
		inst->name = gf_strdup("gm_dummy_in.dylib");
		gf_list_add(pm->plug_list, inst);
	}
	{
		ModuleInstance *inst;		
		GF_SAFEALLOC(inst, ModuleInstance);
		inst->interfaces = gf_list_new();
		inst->plugman = pm;
		inst->name = gf_strdup("gm_ctx_load.dylib");
		gf_list_add(pm->plug_list, inst);
	}
	{
		ModuleInstance *inst;		
		GF_SAFEALLOC(inst, ModuleInstance);
		inst->interfaces = gf_list_new();
		inst->plugman = pm;
		inst->name = gf_strdup("gm_svg_in.dylib");
		gf_list_add(pm->plug_list, inst);
	}
	{
		ModuleInstance *inst;		
		GF_SAFEALLOC(inst, ModuleInstance);
		inst->interfaces = gf_list_new();
		inst->plugman = pm;
		inst->name = gf_strdup("gm_mp3_in.dylib");
		gf_list_add(pm->plug_list, inst);
	}
	{
		ModuleInstance *inst;		
		GF_SAFEALLOC(inst, ModuleInstance);
		inst->interfaces = gf_list_new();
		inst->plugman = pm;
		inst->name = gf_strdup("gm_aac_in.dylib");
		gf_list_add(pm->plug_list, inst);
	}
	{
		ModuleInstance *inst;		
		GF_SAFEALLOC(inst, ModuleInstance);
		inst->interfaces = gf_list_new();
		inst->plugman = pm;
		inst->name = gf_strdup("gm_img_in.dylib");
		gf_list_add(pm->plug_list, inst);
	}	
	{
		ModuleInstance *inst;		
		GF_SAFEALLOC(inst, ModuleInstance);
		inst->interfaces = gf_list_new();
		inst->plugman = pm;
		inst->name = gf_strdup("gm_bifs_dec.dylib");
		gf_list_add(pm->plug_list, inst);
	}
	{
		ModuleInstance *inst;		
		GF_SAFEALLOC(inst, ModuleInstance);
		inst->interfaces = gf_list_new();
		inst->plugman = pm;
		inst->name = gf_strdup("gm_gpac_js.dylib");
		gf_list_add(pm->plug_list, inst);
	}
	{
		ModuleInstance *inst;		
		GF_SAFEALLOC(inst, ModuleInstance);
		inst->interfaces = gf_list_new();
		inst->plugman = pm;
		inst->name = gf_strdup("gm_isom_in.dylib");
		gf_list_add(pm->plug_list, inst);
	}
	{
		ModuleInstance *inst;		
		GF_SAFEALLOC(inst, ModuleInstance);
		inst->interfaces = gf_list_new();
		inst->plugman = pm;
		inst->name = gf_strdup("gm_laser_dec.dylib");
		gf_list_add(pm->plug_list, inst);
	}
	{
		ModuleInstance *inst;		
		GF_SAFEALLOC(inst, ModuleInstance);
		inst->interfaces = gf_list_new();
		inst->plugman = pm;
		inst->name = gf_strdup("gm_odf_dec.dylib");
		gf_list_add(pm->plug_list, inst);
	}
	{
		ModuleInstance *inst;		
		GF_SAFEALLOC(inst, ModuleInstance);
		inst->interfaces = gf_list_new();
		inst->plugman = pm;
		inst->name = gf_strdup("gm_rtp_in.dylib");
		gf_list_add(pm->plug_list, inst);
	}
	{
		ModuleInstance *inst;		
		GF_SAFEALLOC(inst, ModuleInstance);
		inst->interfaces = gf_list_new();
		inst->plugman = pm;
		inst->name = gf_strdup("gm_mpegts_in.dylib");
		gf_list_add(pm->plug_list, inst);
	}
	{
		ModuleInstance *inst;		
		GF_SAFEALLOC(inst, ModuleInstance);
		inst->interfaces = gf_list_new();
		inst->plugman = pm;
		inst->name = gf_strdup("gm_ft_font.dylib");
		gf_list_add(pm->plug_list, inst);
	}
	{
		ModuleInstance *inst;		
		GF_SAFEALLOC(inst, ModuleInstance);
		inst->interfaces = gf_list_new();
		inst->plugman = pm;
		inst->name = gf_strdup("gm_widgetman.dylib");
		gf_list_add(pm->plug_list, inst);
	}
	{
		ModuleInstance *inst;		
		GF_SAFEALLOC(inst, ModuleInstance);
		inst->interfaces = gf_list_new();
		inst->plugman = pm;
		inst->name = gf_strdup("gm_mpd_in.dylib");
		gf_list_add(pm->plug_list, inst);
	}
#else
	gf_enum_directory(pm->dir, 0, enum_modules, pm, ".dylib");
#endif
#else
	gf_enum_directory(pm->dir, 0, enum_modules, pm, ".so");
#endif

	return gf_list_count(pm->plug_list);
}
