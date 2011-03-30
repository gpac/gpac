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
#include <gpac/config_file.h>
#include <gpac/tools.h>


#if defined(TARGET_OS_IPHONE) || defined(TARGET_IPHONE_SIMULATOR)
#include <gpac/modules/video_out.h>
#include <gpac/modules/audio_out.h>
#include "sdl_out.h"

/*hack I'm not proud of. If you're young and innocent never do that*/
void* (*SDL_Module_Load_Video) (void);
void* (*SDL_Module_Load_Audio) (void);
GF_EXPORT void gf_iphone_set_sdl_video_module(void* (*SDL_Module) (void)) {
	SDL_Module_Load_Video = SDL_Module;
}
GF_EXPORT void gf_iphone_set_sdl_audio_module(void* (*SDL_Module) (void)) {
	SDL_Module_Load_Audio = SDL_Module;
}
#endif


GF_EXPORT
GF_ModuleManager *gf_modules_new(const char *directory, GF_Config *config)
{
	GF_ModuleManager *tmp;
	if (!directory || !strlen(directory) || (strlen(directory) > GF_MAX_PATH)) return NULL;

	GF_SAFEALLOC(tmp, GF_ModuleManager);
	if (!tmp) return NULL;
	strcpy(tmp->dir, directory);

	/*remove the final delimiter*/
	if (tmp->dir[strlen(tmp->dir)-1] == GF_PATH_SEPARATOR) tmp->dir[strlen(tmp->dir)-1] = 0;

	tmp->plug_list = gf_list_new();
	if (!tmp->plug_list) {
		gf_free(tmp);
		return NULL;
	}
	tmp->cfg = config;
	gf_modules_refresh(tmp);
	return tmp;
}

GF_EXPORT
void gf_modules_del(GF_ModuleManager *pm)
{
	ModuleInstance *inst;
	if (!pm) return;
	/*unload all modules*/
	while (gf_list_count(pm->plug_list)) {
		inst = (ModuleInstance *) gf_list_get(pm->plug_list, 0);
		gf_modules_free_module(inst);
		gf_list_rem(pm->plug_list, 0);
	}
	gf_list_del(pm->plug_list);
	gf_free(pm);
}

Bool gf_module_is_loaded(GF_ModuleManager *pm, char *filename)
{
	u32 i = 0;
	ModuleInstance *inst;
	while ( (inst = (ModuleInstance *) gf_list_enum(pm->plug_list, &i) ) ) {
		if (!strcmp(inst->name, filename)) return 1;
	}
	return 0;
}

GF_EXPORT
u32 gf_modules_get_count(GF_ModuleManager *pm)
{
	if (!pm) return 0;
	return gf_list_count(pm->plug_list);
}


GF_EXPORT
GF_BaseInterface *gf_modules_load_interface(GF_ModuleManager *pm, u32 whichplug, u32 InterfaceFamily)
{
	const char *opt;
	char szKey[32];
	ModuleInstance *inst;
	GF_BaseInterface *ifce;

	if (!pm) return NULL;
	inst = (ModuleInstance *) gf_list_get(pm->plug_list, whichplug);
	if (!inst) return NULL;
        GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Core] Load interface...%s\n", inst->name));
	/*look in cache*/
        if (!pm->cfg){
            GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] No pm->cfg has been set !!!\n"));
            return NULL;
        }
        opt = gf_cfg_get_key(pm->cfg, "PluginsCache", inst->name);
	if (opt) {
                const char * ifce_str = gf_4cc_to_str(InterfaceFamily);
		snprintf(szKey, 32, "%s:yes", ifce_str ? ifce_str : "(null)");
		if (!strstr(opt, szKey)){
		  return NULL;
                }
	}
	if (!gf_modules_load_library(inst)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] Cannot load library %s\n", inst->name));
		gf_cfg_set_key(pm->cfg, "PluginsCache", inst->name, "Invalid Plugin");
		return NULL;
	}
	if (!inst->query_func) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] Library %s missing GPAC export symbols\n", inst->name));
		gf_cfg_set_key(pm->cfg, "PluginsCache", inst->name, "Invalid Plugin");
		goto err_exit;
	}

	/*build cache*/
	if (!opt) {
		u32 i;
		Bool found = 0;
		char *key;
		const u32 *si = inst->query_func();
		if (!si) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("[Core] GPAC module %s has no supported interfaces - disabling\n", inst->name));
			gf_cfg_set_key(pm->cfg, "PluginsCache", inst->name, "Invalid Plugin");
			goto err_exit;
		}
		i=0;
		while (si[i]) i++;

		key = gf_malloc(sizeof(char) * 10 * i);
		key[0] = 0;
		i=0;
		while (si[i]) {
			snprintf(szKey, 32, "%s:yes ", gf_4cc_to_str(si[i]));
			strcat(key, szKey);
			if (InterfaceFamily==si[i]) found = 1;
			i++;
		}
		gf_cfg_set_key(pm->cfg, "PluginsCache", inst->name, key);
		gf_free(key);
		if (!found) goto err_exit;
	}

	if (!inst->query_func || !inst->query_func(InterfaceFamily) ) goto err_exit;
	ifce = (GF_BaseInterface *) inst->load_func(InterfaceFamily);
	/*sanity check*/
	if (!ifce) goto err_exit;
#if defined(TARGET_OS_IPHONE) || defined(TARGET_IPHONE_SIMULATOR)
	if (!strcmp(inst->name, "gm_sdl_out.dylib")) {
		if (InterfaceFamily == GF_VIDEO_OUTPUT_INTERFACE) {
			ifce = SDL_Module_Load_Video();
			fprintf(stderr, "***         Loading SDL Video: 0x%p ***\n", ifce);
		} else if (InterfaceFamily == GF_AUDIO_OUTPUT_INTERFACE) {
			ifce = SDL_Module_Load_Audio();
			fprintf(stderr, "***         Loading SDL Audio: 0x%p ***\n", ifce);
		}
	}
#endif
	if (!ifce->module_name || (ifce->InterfaceType != InterfaceFamily)) {
		inst->destroy_func(ifce);
		goto err_exit;
	}
	gf_list_add(inst->interfaces, ifce);
	/*keep track of parent*/
	ifce->HPLUG = inst;
        GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Core] Load interface %s DONE.\n", inst->name));
	return ifce;

err_exit:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Core] Load interface %s exit label, freing library...\n", inst->name));
	gf_modules_unload_library(inst);
        GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Core] Load interface %s EXIT.\n", inst->name));
	return NULL;
}


GF_EXPORT
GF_BaseInterface *gf_modules_load_interface_by_name(GF_ModuleManager *pm, const char *plug_name, u32 InterfaceFamily)
{
	const char *file_name;
	u32 i, count;
	GF_BaseInterface *ifce;
	count = gf_list_count(pm->plug_list);

	/*look for cache entry*/
	file_name = gf_cfg_get_key(pm->cfg, "PluginsCache", plug_name);
	if (file_name) {
		for (i=0; i<count; i++) {
			ModuleInstance *inst = (ModuleInstance *) gf_list_get(pm->plug_list, i);
			if (!strcmp(inst->name,  file_name)) {
				ifce = gf_modules_load_interface(pm, i, InterfaceFamily);
				if (ifce) return ifce;
			}
		}
	}

	for (i=0; i<count; i++) {
		ifce = gf_modules_load_interface(pm, i, InterfaceFamily);
		if (!ifce) continue;
		if (ifce->module_name && !stricmp(ifce->module_name, plug_name)) {
			/*update cache entry*/
			gf_cfg_set_key(pm->cfg, "PluginsCache", plug_name, ((ModuleInstance*)ifce->HPLUG)->name);
			return ifce;
		}
		gf_modules_close_interface(ifce);
	}
	return NULL;
}

GF_EXPORT
GF_Err gf_modules_close_interface(GF_BaseInterface *ifce)
{
	ModuleInstance *par;
	s32 i;
	if (!ifce) return GF_BAD_PARAM;
	par = (ModuleInstance *) ifce->HPLUG;

	if (!par || !ifce->InterfaceType) return GF_BAD_PARAM;

	i = gf_list_find(par->plugman->plug_list, par);
	if (i<0) return GF_BAD_PARAM;

	i = gf_list_find(par->interfaces, ifce);
	if (i<0) return GF_BAD_PARAM;
	gf_list_rem(par->interfaces, (u32) i);
	par->destroy_func(ifce);
	gf_modules_unload_library(par);
	//GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Core] interface %s unloaded\n", ifce->module_name));
	return GF_OK;
}

GF_EXPORT
const char *gf_modules_get_option(GF_BaseInterface *ifce, const char *secName, const char *keyName)
{
	GF_Config *cfg;
	if (!ifce || !ifce->HPLUG) return NULL;
	cfg = ((ModuleInstance *)ifce->HPLUG)->plugman->cfg;
	if (!cfg) return NULL;
	return gf_cfg_get_key(cfg, secName, keyName);
}

GF_EXPORT
GF_Err gf_modules_set_option(GF_BaseInterface *ifce, const char *secName, const char *keyName, const char *keyValue)
{
	GF_Config *cfg;
	if (!ifce || !ifce->HPLUG) return GF_BAD_PARAM;
	cfg = ((ModuleInstance *)ifce->HPLUG)->plugman->cfg;
	if (!cfg) return GF_NOT_SUPPORTED;
	return gf_cfg_set_key(cfg, secName, keyName, keyValue);
}

GF_EXPORT
GF_Config *gf_modules_get_config(GF_BaseInterface *ifce)
{
	if (!ifce || !ifce->HPLUG) return NULL;
	return ((ModuleInstance *)ifce->HPLUG)->plugman->cfg;
}

GF_EXPORT
const char *gf_modules_get_file_name(GF_ModuleManager *pm, u32 i)
{
	ModuleInstance *inst = (ModuleInstance *) gf_list_get(pm->plug_list, i);
	if (!inst) return NULL;
	return inst->name;
}

GF_EXPORT
const char *gf_module_get_file_name(GF_BaseInterface *ifce)
{
	ModuleInstance *inst = (ModuleInstance *) ifce->HPLUG;
	if (!inst) return NULL;
	return inst->name;
}

