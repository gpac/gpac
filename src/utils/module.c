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
#include <gpac/config_file.h>
#include <gpac/tools.h>
#include <gpac/network.h>


static void load_all_modules(GF_ModuleManager *mgr)
{
#define LOAD_PLUGIN(	__name	)	{ \
	GF_InterfaceRegister *gf_register_module_##__name();	\
	pr = gf_register_module_##__name();\
	if (!pr) {\
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Failed to statically load module ##__name\n"));\
	} else {\
		gf_list_add(mgr->plugin_registry, pr);	\
	}	\
	}	

#ifdef GPAC_STATIC_MODULES
	GF_InterfaceRegister *pr;

	LOAD_PLUGIN(aac_in);
    LOAD_PLUGIN(ac3);
#ifdef GPAC_HAS_ALSA
    LOAD_PLUGIN(alsa);
#endif
    LOAD_PLUGIN(audio_filter);
	LOAD_PLUGIN(bifs);
#ifndef GPAC_DISABLE_SMGR	
	LOAD_PLUGIN(ctx_load);
#endif
#ifdef GPAC_HAS_DIRECTFB
	LOAD_PLUGIN(directfb_out);
#endif
	LOAD_PLUGIN(dummy_in);
#ifdef GPAC_HAS_DIRECTX
	LOAD_PLUGIN(dx_out);
#endif
#ifdef GPAC_HAS_FFMPEG
	LOAD_PLUGIN(ffmpeg);
#endif
#ifdef GPAC_HAS_FREENECT
	LOAD_PLUGIN(freenect);
#endif
#ifdef GPAC_HAS_FREETYPE
	LOAD_PLUGIN(ftfont);
#endif
#ifdef GPAC_HAS_SPIDERMONKEY
	LOAD_PLUGIN(gpac_js);
#endif
	LOAD_PLUGIN(img_in);
    LOAD_PLUGIN(isma_ea);
	LOAD_PLUGIN(isom);
#ifdef GPAC_HAS_JACK
	LOAD_PLUGIN(jack);
#endif
#ifndef GPAC_DISABLE_SVG
	LOAD_PLUGIN(laser);
#endif	
	LOAD_PLUGIN(mp3_in);
    LOAD_PLUGIN(mpd_in);
#ifndef GPAC_DISABLE_MEDIA_IMPORT
	LOAD_PLUGIN(mpegts_in);
#endif
#ifdef GPAC_HAS_SPIDERMONKEY
    LOAD_PLUGIN(mse_in);
#endif	
	LOAD_PLUGIN(odf_dec);
#ifdef GPAC_HAS_OGG
    LOAD_PLUGIN(ogg_in);
#endif
#ifdef GPAC_HAS_OPENHEVC
	LOAD_PLUGIN(openhevc);
#endif
#ifdef GPAC_HAS_OPENSVC
	LOAD_PLUGIN(opensvc);
#endif
#ifndef GPAC_DISABLE_LOADER_BT
	LOAD_PLUGIN(osd);
#endif
#ifdef GPAC_HAS_OSS
	LOAD_PLUGIN(oss);
#endif
#ifdef GPAC_HAS_PULSEAUDIO
	LOAD_PLUGIN(pulseaudio);
#endif
	LOAD_PLUGIN(raw_out);	
#ifdef GPAC_HAS_FFMPEG
	//    LOAD_PLUGIN(redirect_av);    
#endif
	LOAD_PLUGIN(rtp_in);
    LOAD_PLUGIN(saf_in);
#ifdef GPAC_HAS_SDL
    LOAD_PLUGIN(sdl_out);
#endif
	LOAD_PLUGIN(soft_raster);
#if !defined(GPAC_DISABLE_SMGR) && !defined(GPAC_DISABLE_SVG)	
	LOAD_PLUGIN(svg_in);
#endif		
	LOAD_PLUGIN(timedtext);			
    LOAD_PLUGIN(validator);
#ifdef GPAC_HAS_WAVEOUT
	LOAD_PLUGIN(wave_out);
#endif
#ifndef GPAC_DISABLE_SVG
	LOAD_PLUGIN(widgetman);
#endif
#ifdef GPAC_HAS_X11
	LOAD_PLUGIN(x11_out);
#endif
#ifdef GPAC_HAS_XVID
    LOAD_PLUGIN(xvid);
#endif
	


				
	//todo fix project for iOS
#ifdef GPAC_IPHONE
//    LOAD_PLUGIN(ios_cam);
//    LOAD_PLUGIN(ios_mpegv);
#endif


#endif //GPAC_STATIC_MODULES

#undef LOAD_PLUGIN
	
}


GF_EXPORT
GF_ModuleManager *gf_modules_new(const char *directory, GF_Config *config)
{
	GF_ModuleManager *tmp;
	u32 loadedModules;
	const char *opt;
	u32 num_dirs = 0;

	if (!config) return NULL;

	/* Try to resolve directory from config file */
	GF_SAFEALLOC(tmp, GF_ModuleManager);
	if (!tmp) return NULL;
	tmp->cfg = config;
	gf_modules_get_module_directories(tmp, &num_dirs);
	if (!num_dirs) return NULL;

	/* Initialize module list */
	tmp->plug_list = gf_list_new();
	if (!tmp->plug_list) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("OUT OF MEMORY, cannot create list of modules !!!\n"));
		gf_free(tmp);
		return NULL;
	}
	tmp->plugin_registry = gf_list_new();
	if (!tmp->plugin_registry) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("OUT OF MEMORY, cannot create list of static module registers !!!\n"));
		gf_list_del(tmp->plug_list);
		gf_free(tmp);
		return NULL;
	}

	opt = gf_cfg_get_key(config, "Systems", "ModuleUnload");
	if (opt && !strcmp(opt, "no")) {
		tmp->no_unload = GF_TRUE;
	}
	load_all_modules(tmp);

	loadedModules = gf_modules_refresh(tmp);
	GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("Loaded %d modules from directory %s.\n", loadedModules, directory));

	return tmp;
}

GF_EXPORT
void gf_modules_del(GF_ModuleManager *pm)
{
	u32 i;
	ModuleInstance *inst;
	if (!pm) return;
	/*unload all modules*/
	while (gf_list_count(pm->plug_list)) {
		inst = (ModuleInstance *) gf_list_get(pm->plug_list, 0);
		gf_modules_free_module(inst);
		gf_list_rem(pm->plug_list, 0);
	}

	/* Delete module directories*/
	for (i = 0; i < pm->num_dirs; i++){
		gf_free((void*)pm->dirs[i]);
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
	return GF_FALSE;
}

GF_EXPORT
u32 gf_modules_get_count(GF_ModuleManager *pm)
{
	if (!pm) return 0;
	return gf_list_count(pm->plug_list);
}

GF_EXPORT
const char **gf_modules_get_module_directories(GF_ModuleManager *pm, u32* num_dirs){
	char* directories;
	char* tmp_dirs;
	char * pch;
	if (!pm) return NULL;
	if (pm->num_dirs > 0 ){
		*num_dirs = pm->num_dirs;
		return pm->dirs;
	}
	if (!pm->cfg) return NULL;
	
	/* Get directory from config file */
	directories = (char*)gf_cfg_get_key(pm->cfg, "General", "ModulesDirectory");
	if (! directories) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Module directory not found - check the configuration file exit and the \"ModulesDirectory\" key is set\n"));
		return NULL;
	}

	tmp_dirs = gf_strdup(directories);
	pch = strtok (tmp_dirs,";");

	while (pch != NULL)
	{
		if (pm->num_dirs == MAX_MODULE_DIRS){
			GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("Reach maximum number of module directories  - check the configuration file and the \"ModulesDirectory\" key.\n"));
			break;
		}

		pm->dirs[pm->num_dirs] = gf_strdup(pch);
		pm->num_dirs++;
		pch = strtok (NULL, ";");
	}
	gf_free(tmp_dirs);
	*num_dirs = pm->num_dirs;
	return pm->dirs;
}

GF_EXPORT
GF_BaseInterface *gf_modules_load_interface(GF_ModuleManager *pm, u32 whichplug, u32 InterfaceFamily)
{
	const char *opt;
	char szKey[32];
	ModuleInstance *inst;
	GF_BaseInterface *ifce;

	if (!pm){
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] gf_modules_load_interface() : No Module Manager set\n"));
		return NULL;
	}
	inst = (ModuleInstance *) gf_list_get(pm->plug_list, whichplug);
	if (!inst){
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] gf_modules_load_interface() : no module %d exist.\n", whichplug));
		return NULL;
	}
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
		Bool found = GF_FALSE;
		char *key;
		const u32 *si = inst->query_func();
		if (!si) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("[Core] GPAC module %s has no supported interfaces - disabling\n", inst->name));
			gf_cfg_set_key(pm->cfg, "PluginsCache", inst->name, "Invalid Plugin");
			goto err_exit;
		}
		i=0;
		while (si[i]) i++;

		key = (char*)gf_malloc(sizeof(char) * 10 * i);
		key[0] = 0;
		i=0;
		while (si[i]) {
			snprintf(szKey, 32, "%s:yes ", gf_4cc_to_str(si[i]));
			strcat(key, szKey);
			if (InterfaceFamily==si[i]) found = GF_TRUE;
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
	if (!pm || !plug_name || !pm->plug_list || !pm->cfg){
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] gf_modules_load_interface_by_name has bad parameters pm=%p, plug_name=%s.\n", pm, plug_name));
		return NULL;
	}
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
	GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("[Core] Plugin %s of type %d not found in cache, searching for it...\n", plug_name, InterfaceFamily));
	for (i=0; i<count; i++) {
		ifce = gf_modules_load_interface(pm, i, InterfaceFamily);
		if (!ifce) continue;
		if (ifce->module_name && !strnicmp(ifce->module_name, plug_name, MIN(strlen(ifce->module_name), strlen(plug_name)) )) {
			/*update cache entry*/
			gf_cfg_set_key(pm->cfg, "PluginsCache", plug_name, ((ModuleInstance*)ifce->HPLUG)->name);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Core] Added plugin cache %s for %s\n", plug_name, ((ModuleInstance*)ifce->HPLUG)->name));
			return ifce;
		}
		gf_modules_close_interface(ifce);
	}
	GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("[Core] Plugin %s not found in %d modules.\n", plug_name, count));
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
	return gf_cfg_get_ikey(cfg, secName, keyName);
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



