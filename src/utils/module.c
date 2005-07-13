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
#include <gpac/config.h>

GF_ModuleManager *gf_modules_new(const unsigned char *directory, GF_Config *config)
{
	GF_ModuleManager *tmp;
	if (!directory || !strlen(directory) || (strlen(directory) > GF_MAX_PATH)) return NULL;

	tmp = malloc(sizeof(GF_ModuleManager));
	if (!tmp) return NULL;
	memset(tmp, sizeof(GF_ModuleManager), 0);
	strcpy(tmp->dir, directory);

	/*remove the final delimiter*/
	if (tmp->dir[strlen(tmp->dir)-1] == GF_PATH_SEPARATOR) tmp->dir[strlen(tmp->dir)-1] = 0;

	tmp->plug_list = gf_list_new();
	if (!tmp->plug_list) {
		free(tmp);
		return NULL;
	}
	tmp->cfg = config;
	gf_modules_refresh(tmp);
	return tmp;
}

void gf_modules_del(GF_ModuleManager *pm)
{
	ModuleInstance *inst;

	/*unload all modules*/
	while (gf_list_count(pm->plug_list)) {
		inst = gf_list_get(pm->plug_list, 0);
		gf_modules_free_module(inst);
		gf_list_rem(pm->plug_list, 0);
	}
	gf_list_del(pm->plug_list);
	free(pm);
}

Bool gf_module_is_loaded(GF_ModuleManager *pm, unsigned char *filename) 
{
	u32 i;
	ModuleInstance *inst;
	for (i=0; i<gf_list_count(pm->plug_list); i++) {
		inst = gf_list_get(pm->plug_list, i);
		if (!strcmp(inst->szName, filename)) return 1;
	}
	return 0;
}

u32 gf_modules_get_count(GF_ModuleManager *pm)
{
	if (!pm) return 0;
	return gf_list_count(pm->plug_list);
}


Bool gf_modules_load_interface(GF_ModuleManager *pm, u32 whichplug, u32 InterfaceFamily, void **interface_obj)
{
	ModuleInstance *inst;
	GF_BaseInterface *ifce;

	if (!pm) return 0;
	inst = gf_list_get(pm->plug_list, whichplug);
	if (!inst) return 0;
	if (!gf_modules_load_library(inst)) return 0;

	if (! inst->query_func(InterfaceFamily) ) return 0;

	ifce = (GF_BaseInterface *) inst->load_func(InterfaceFamily);
	if (!ifce) return 0;

	/*sanity check*/
	if (!ifce->module_name || (ifce->InterfaceType != InterfaceFamily)) {
		inst->destroy_func(ifce);
		return 0;
	}
	gf_list_add(inst->interfaces, ifce);
	/*keep track of parent*/
	ifce->HPLUG = inst;
	*interface_obj = ifce;
	return 1;
}


Bool gf_modules_load_interface_by_name(GF_ModuleManager *pm, const char *plug_name, u32 InterfaceFamily, void **interface_obj)
{
	u32 i;
	GF_BaseInterface *ifce;
	for (i=0; i<gf_list_count(pm->plug_list); i++) {
		if (gf_modules_load_interface(pm, i, InterfaceFamily, (void **) &ifce)) {
			/*check by driver name*/
			if (ifce->module_name && !stricmp(ifce->module_name, plug_name)) {
				*interface_obj = ifce;
				return 1;
			}
			/*check by file name*/
			if (!stricmp(((ModuleInstance *)ifce->HPLUG)->szName, plug_name)) {
				*interface_obj = ifce;
				return 1;
			}
			gf_modules_close_interface(ifce);
		}
	}
	return 0;
}

GF_Err gf_modules_close_interface(void *interface_obj)
{
	GF_BaseInterface *ifce;
	ModuleInstance *par;
	u32 i;
	if (!interface_obj) return GF_BAD_PARAM;
	ifce = (GF_BaseInterface *)interface_obj;
	par = ifce->HPLUG;

	if (!par || !ifce->InterfaceType) return GF_BAD_PARAM;

	i = gf_list_find(par->plugman->plug_list, par);
	if (i<0) return GF_BAD_PARAM;

	i = gf_list_find(par->interfaces, interface_obj);
	if (i<0) return GF_BAD_PARAM;
	gf_list_rem(par->interfaces, (u32) i);
	par->destroy_func(interface_obj);

	gf_modules_unload_library(par);
	return GF_OK;
}

char *gf_modules_get_option(void *interface_obj, const char *secName, const char *keyName)
{
	GF_BaseInterface *ifce = (GF_BaseInterface *) interface_obj;
	if (!ifce || !ifce->HPLUG) return NULL;
	return gf_cfg_get_key(((ModuleInstance *)ifce->HPLUG)->plugman->cfg, secName, keyName);
}

GF_Err gf_modules_set_option(void *interface_obj, const char *secName, const char *keyName, const char *keyValue)
{
	GF_BaseInterface *ifce = (GF_BaseInterface *) interface_obj;
	if (!ifce || !ifce->HPLUG) return GF_BAD_PARAM;
	return gf_cfg_set_key(((ModuleInstance *)ifce->HPLUG)->plugman->cfg, secName, keyName, keyValue);
}

const char *gf_modules_get_file_name(GF_ModuleManager *pm, u32 i)
{
	ModuleInstance *inst = gf_list_get(pm->plug_list, i);
	if (!inst) return NULL;
	return inst->szName;
}

