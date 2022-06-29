/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2022
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



#ifndef GPAC_MODULE_CUSTOM_LOAD
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

#ifdef GPAC_HAS_SDL
	LOAD_PLUGIN(sdl_out);
#endif

#ifdef GPAC_HAS_FREETYPE
	LOAD_PLUGIN(ftfont);
#endif
#ifdef GPAC_HAS_ALSA
	LOAD_PLUGIN(alsa);
#endif

#ifdef GPAC_HAS_DIRECTFB
	LOAD_PLUGIN(directfb_out);
#endif

#ifdef GPAC_HAS_DIRECTX
	LOAD_PLUGIN(dx_out);
#endif

#ifdef GPAC_HAS_JACK
	LOAD_PLUGIN(jack);
#endif

#ifdef GPAC_HAS_OSS
	LOAD_PLUGIN(oss);
#endif

#ifdef GPAC_HAS_PULSEAUDIO
	LOAD_PLUGIN(pulseaudio);
#endif

#ifndef GPAC_DISABLE_PLAYER
	LOAD_PLUGIN(validator);
#endif


#ifdef GPAC_HAS_WAVEOUT
	LOAD_PLUGIN(wave_out);
#endif

#ifdef GPAC_HAS_X11
	LOAD_PLUGIN(x11_out);
#endif

	//todo fix project for iOS
#ifdef GPAC_CONFIG_IOS
	//these do not compile with xcode 4.2
//    LOAD_PLUGIN(ios_cam);
//    LOAD_PLUGIN(ios_mpegv);
#endif

#endif //GPAC_STATIC_MODULES

#undef LOAD_PLUGIN

}
#endif //GPAC_MODULE_CUSTOM_LOAD

GF_ModuleManager *gpac_modules_static = NULL;

GF_EXPORT
GF_Err gf_module_load_static(GF_InterfaceRegister *(*register_module)())
{
	GF_InterfaceRegister *pr;
	GF_Err rc;
	if (register_module == NULL)
		return GF_OK;

	pr = register_module();
	if (!pr) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Failed to statically loaded module\n"));
		return GF_NOT_SUPPORTED;
	}

	rc = gf_list_add(gpac_modules_static->plugin_registry, pr);
	if (rc != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Failed to statically loaded module\n"));
		return rc;
	}
	return GF_OK;
}

u32 gf_modules_refresh(GF_ModuleManager *pm);

static void gf_modules_check_load()
{
	if (gpac_modules_static->needs_load) {
		gpac_modules_static->needs_load = GF_FALSE;
#ifndef GPAC_MODULE_CUSTOM_LOAD
		load_all_modules(gpac_modules_static);
#endif
		gf_modules_refresh(gpac_modules_static);
		GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("Loaded %d modules.\n", gf_modules_count() ));
	}
}


void gf_modules_refresh_module_directories()
{
	char* directories;
	char* tmp_dirs;
	char * pch;
	u32 i;
	GF_ModuleManager *pm = gpac_modules_static;
	if (!pm) return;

	for (i=0; i<pm->num_dirs; i++) {
		gf_free(pm->dirs[i]);
	}
	pm->num_dirs = 0;

	//default module directory
	directories = (char*)gf_opts_get_key("core", "module-dir");
	if (directories) {
		pm->dirs[0] = gf_strdup(directories);
		pm->num_dirs = 1;
	}

	/* User-defined directories*/
	directories = (char*)gf_opts_get_key("core", "mod-dirs");
	if (! directories) {
		if (!pm->num_dirs) {
#ifndef GPAC_CONFIG_IOS
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("Modules directories not found - check the \"module-dir\" key is set in the \"core\" section\n"));
#endif
		}
		return;
	}

	tmp_dirs = directories;
	pch = strtok (tmp_dirs,";");

	while (pch != NULL) {
		if (pm->num_dirs == MAX_MODULE_DIRS) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("Reach maximum number of module directories %d.\n", MAX_MODULE_DIRS));
			break;
		}
		pm->dirs[pm->num_dirs] = gf_strdup(pch);
		pm->num_dirs++;
		pch = strtok (NULL, ";");
	}
}


/*!
\brief module manager construtcor
 *
 *Constructs a module manager object.
\param directory absolute path to the directory where the manager shall look for modules
\param cfgFile GPAC configuration file handle. If this is NULL, the modules won't be able to share the configuration
 *file with the rest of the GPAC framework.
\return the module manager object
*/
void gf_modules_new(GF_Config *config)
{
	const char *opt;
	if (!config) return;
	if (gpac_modules_static) return;

	/* Try to resolve directory from config file */
	GF_SAFEALLOC(gpac_modules_static, GF_ModuleManager);
	if (!gpac_modules_static) return;
	gpac_modules_static->cfg = config;
	gpac_modules_static->mutex = gf_mx_new("Module Manager");
	gf_modules_refresh_module_directories();

	/* Initialize module list */
	gpac_modules_static->plug_list = gf_list_new();
	if (!gpac_modules_static->plug_list) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("OUT OF MEMORY, cannot create list of modules !!!\n"));
		gf_free(gpac_modules_static);
		gpac_modules_static = NULL;
		return;
	}
	gpac_modules_static->plugin_registry = gf_list_new();
	if (!gpac_modules_static->plugin_registry) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("OUT OF MEMORY, cannot create list of static module registers !!!\n"));
		gf_list_del(gpac_modules_static->plug_list);
		gf_free(gpac_modules_static);
		gpac_modules_static = NULL;
		return;
	}

	gpac_modules_static->no_unload = !gf_opts_get_bool("core", "mod-reload");

	opt = gf_opts_get_key("core", "version");
	if (!opt || strcmp(opt, gf_gpac_version())) {
		gf_cfg_del_section(config, "PluginsCache");
		gf_opts_set_key("core", "version", gf_gpac_version());
	}

	gpac_modules_static->needs_load = GF_TRUE;
}


void gf_module_reload_dirs()
{
	if (!gpac_modules_static) return;
	gf_modules_refresh_module_directories();
	gpac_modules_static->needs_load = GF_TRUE;
}


/*!
\brief module manager destructor
 *
 *Destroys the module manager
\param pm the module manager
 */
void gf_modules_del()
{
	u32 i;
	GF_ModuleManager *pm = gpac_modules_static;
	if (!pm) return;

	gpac_modules_static = NULL;

	/*unload all modules*/
	while (gf_list_count(pm->plug_list)) {
		ModuleInstance *inst = (ModuleInstance *) gf_list_get(pm->plug_list, 0);
		gf_modules_free_module(inst);
		gf_list_rem(pm->plug_list, 0);
	}
	gf_list_del(pm->plug_list);

	/* Delete module directories*/
	for (i = 0; i < pm->num_dirs; i++) {
		gf_free((void*)pm->dirs[i]);
	}

	/*remove all static modules registry*/
	while (gf_list_count(pm->plugin_registry)) {
		GF_InterfaceRegister *reg  = (GF_InterfaceRegister *) gf_list_get(pm->plugin_registry, 0);
		gf_free(reg);
		gf_list_rem(pm->plugin_registry, 0);
	}

	if (pm->plugin_registry) gf_list_del(pm->plugin_registry);
	gf_mx_del(pm->mutex);
	gf_free(pm);
}

Bool gf_module_is_loaded(GF_ModuleManager *pm, char *filename)
{
	u32 i = 0;
	ModuleInstance *inst;
	while ( (inst = (ModuleInstance *) gf_list_enum(pm->plug_list, &i) ) ) {
		if (!strcmp(inst->name, filename)) return GF_TRUE;
	}
	return GF_FALSE;
}

GF_EXPORT
u32 gf_modules_count()
{
	if (!gpac_modules_static) return 0;
	gf_modules_check_load();
	return gf_list_count(gpac_modules_static->plug_list);
}


GF_EXPORT
GF_BaseInterface *gf_modules_load(u32 whichplug, u32 InterfaceFamily)
{
	const char *opt;
	char szKey[32];
	ModuleInstance *inst;
	GF_BaseInterface *ifce;
	GF_ModuleManager *pm = gpac_modules_static;

	if (!pm) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] gf_modules_load() : No Module Manager set\n"));
		return NULL;
	}

	gf_mx_p(pm->mutex);

	gf_modules_check_load();

	inst = (ModuleInstance *) gf_list_get(pm->plug_list, whichplug);
	if (!inst) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] gf_modules_load() : no module %d exist.\n", whichplug));
		gf_mx_v(pm->mutex);
		return NULL;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Core] Load interface...%s\n", inst->name));
	/*look in cache*/
	if (!pm->cfg) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] No pm->cfg has been set !!!\n"));
		gf_mx_v(pm->mutex);
		return NULL;
	}
	opt = gf_cfg_get_key(pm->cfg, "PluginsCache", inst->name);
	if (opt) {
		const char * ifce_str = gf_4cc_to_str(InterfaceFamily);
		snprintf(szKey, 32, "%s:yes", ifce_str ? ifce_str : "(null)");
		if (!strstr(opt, szKey)) {
			gf_mx_v(pm->mutex);
			return NULL;
		}
	}
	if (!gf_modules_load_library(inst)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] Cannot load library %s\n", inst->name));
		gf_cfg_set_key(pm->cfg, "PluginsCache", inst->name, "invalid");
		gf_mx_v(pm->mutex);
		return NULL;
	}
	if (!inst->query_func) {
		if (!inst->filterreg_func) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] Library %s missing GPAC export symbols\n", inst->name));
			gf_cfg_set_key(pm->cfg, "PluginsCache", inst->name, "invalid");
		}
		goto err_exit;
	}

	/*build cache*/
	if (!opt) {
		u32 i;
		const int maxKeySize = 32;
		Bool found = GF_FALSE;
		char *key;
		const u32 *si = inst->query_func();
		if (!si) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("[Core] GPAC module %s has no supported interfaces - disabling\n", inst->name));
			gf_cfg_set_key(pm->cfg, "PluginsCache", inst->name, "invalid");
			goto err_exit;
		}
		i=0;
		while (si[i]) i++;

		key = (char*)gf_malloc(sizeof(char) * maxKeySize * i);
		key[0] = 0;
		i=0;
		while (si[i]) {
			snprintf(szKey, maxKeySize, "%s:yes ", gf_4cc_to_str(si[i]));
			strcat(key, szKey);
			if (InterfaceFamily==si[i]) found = GF_TRUE;
			i++;
		}
		strcat(key, inst->filterreg_func ? "GFR1:yes" : "GFR1:no");

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
	gf_mx_v(pm->mutex);
	return ifce;

err_exit:
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Core] Load interface %s exit label, freing library...\n", inst->name));
	gf_modules_unload_library(inst);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Core] Load interface %s EXIT.\n", inst->name));
	gf_mx_v(pm->mutex);
	return NULL;
}

void *gf_modules_load_filter(u32 whichplug, void *fsess)
{
	const char *opt;
	ModuleInstance *inst;
	void *freg = NULL;
	GF_ModuleManager *pm = gpac_modules_static;
	if (!pm) return NULL;
	gf_modules_check_load();

	inst = (ModuleInstance *) gf_list_get(pm->plug_list, whichplug);
	if (!inst) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] gf_modules_load() : no module %d exist.\n", whichplug));
		return NULL;
	}
	if (strncmp(inst->name, "gf_", 3))
		return NULL;
	opt = gf_cfg_get_key(pm->cfg, "PluginsCache", inst->name);
	if (opt) {
		if (!strcmp(opt, "invalid")) return NULL;
		if (!strstr(opt, "GFR1:yes")) return NULL;
	}
	if (!gf_modules_load_library(inst)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] Cannot load library %s\n", inst->name));
		gf_cfg_set_key(pm->cfg, "PluginsCache", inst->name, "invalid");
		return NULL;
	}
	freg = NULL;
	if (inst->filterreg_func) {
		freg = inst->filterreg_func(fsess);
	}
	if (!freg) {
		gf_modules_unload_library(inst);
	}
	return freg;
}

GF_EXPORT
GF_BaseInterface *gf_modules_load_by_name(const char *plug_name, u32 InterfaceFamily)
{
	const char *file_name;
	u32 i, count;
	GF_BaseInterface *ifce;
	GF_ModuleManager *pm = gpac_modules_static;
	if (!pm || !plug_name || !pm->plug_list || !pm->cfg) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] gf_modules_load_by_name has bad parameters pm=%p, plug_name=%s.\n", pm, plug_name));
		return NULL;
	}

	gf_modules_check_load();

	count = gf_list_count(pm->plug_list);
	/*look for cache entry*/
	file_name = gf_cfg_get_key(pm->cfg, "PluginsCache", plug_name);

	if (file_name) {

		for (i=0; i<count; i++) {
			ModuleInstance *inst = (ModuleInstance *) gf_list_get(pm->plug_list, i);
			if (!strcmp(inst->name,  file_name)) {
				ifce = gf_modules_load(i, InterfaceFamily);
				if (ifce) return ifce;
			}
		}
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("[Core] Plugin %s of type %d not found in cache, searching for it...\n", plug_name, InterfaceFamily));
	for (i=0; i<count; i++) {
		const char *mod_filename;
		ifce = gf_modules_load(i, InterfaceFamily);
		if (!ifce) continue;
		if (ifce->module_name && !strnicmp(ifce->module_name, plug_name, MIN(strlen(ifce->module_name), strlen(plug_name)) )) {
			/*update cache entry*/
			gf_cfg_set_key(pm->cfg, "PluginsCache", plug_name, ((ModuleInstance*)ifce->HPLUG)->name);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Core] Added plugin cache %s for %s\n", plug_name, ((ModuleInstance*)ifce->HPLUG)->name));
			return ifce;
		}
		/*check direct addressing by dynamic lib name*/
		mod_filename = gf_module_get_file_name(ifce);
		if (mod_filename && strstr(mod_filename, plug_name)) {
			return ifce;
		}
		gf_modules_close_interface(ifce);
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("[Core] Plugin %s not found in %d modules.\n", plug_name, count));
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
const char *gf_modules_get_file_name(u32 i)
{
	ModuleInstance *inst = (ModuleInstance *) gf_list_get(gpac_modules_static->plug_list, i);
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

#include <gpac/modules/video_out.h>
#include <gpac/modules/audio_out.h>
#include <gpac/modules/font.h>
static Bool module_check_ifce(GF_BaseInterface *ifce, u32 ifce_type)
{
	switch (ifce_type) {
	case GF_VIDEO_OUTPUT_INTERFACE:
	{
		GF_VideoOutput *vout = (GF_VideoOutput *) ifce;
		if (!vout || !vout->Flush || !vout->Setup) return GF_FALSE;
		return GF_TRUE;
	}
	case GF_AUDIO_OUTPUT_INTERFACE:
	{
		GF_AudioOutput *aout = (GF_AudioOutput *) ifce;
		if (!aout || !aout->Configure || !aout->Setup) return GF_FALSE;
		//no more support for raw out, deprecated
		if (!stricmp(ifce->module_name, "Raw Audio Output")) return GF_FALSE;
		/*check that's a valid audio mode*/
		if ((aout->SelfThreaded && aout->SetPriority) || aout->WriteAudio)
			return GF_TRUE;
		return GF_FALSE;
	}

	default:
		return GF_TRUE;
	}
}


GF_BaseInterface *gf_module_load(u32 ifce_type, const char *name)
{
	GF_BaseInterface *ifce = NULL;
	if (name) {
		ifce = gf_modules_load_by_name(name, ifce_type);
		if (!module_check_ifce(ifce, ifce_type)) {
			gf_modules_close_interface(ifce);
			ifce = NULL;
		}
	}
	/*get a preferred output*/
	if (!ifce) {
		const char *sOpt;
		switch (ifce_type) {
		case GF_VIDEO_OUTPUT_INTERFACE:
			sOpt = gf_opts_get_key("core", "video-output");
			break;
		case GF_AUDIO_OUTPUT_INTERFACE:
			sOpt = gf_opts_get_key("core", "audio-output");
			break;
		case GF_FONT_READER_INTERFACE:
			sOpt = gf_opts_get_key("core", "font-reader");
			break;
		default:
			sOpt = NULL;
		}
		if (sOpt) {
			ifce = gf_modules_load_by_name(sOpt, ifce_type);
			if (!module_check_ifce(ifce, ifce_type)) {
				gf_modules_close_interface(ifce);
				ifce = NULL;
			}
		}
	}
	if (!ifce) {
		u32 i, count = gf_modules_count();
		for (i=0; i<count; i++) {
			ifce = gf_modules_load(i, ifce_type);
			if (!ifce) continue;

			if (!module_check_ifce(ifce, ifce_type)) {
				gf_modules_close_interface(ifce);
				ifce = NULL;
				continue;
			}
			return ifce;
		}
	}
	return ifce;
}
