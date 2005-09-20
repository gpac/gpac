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

#include "../module_wrap.h"
#include <sys/stat.h>
#include <dlfcn.h>
#include <dirent.h>


/*delete all interfaces loaded on object*/
void gf_modules_free_module(ModuleInstance *inst)
{
	void *objinterface;
	while (gf_list_count(inst->interfaces)) {
		objinterface = gf_list_get(inst->interfaces, 0);
		gf_list_rem(inst->interfaces, 0);
		inst->destroy_func(objinterface);
	}

	if (inst->lib_handle) dlclose(inst->lib_handle);

	gf_list_del(inst->interfaces);
	free(inst);
}

Bool gf_modules_load_library(ModuleInstance *inst)
{
	void *ModuleLib;
	s32 _flags;
	char path[GF_MAX_PATH];
	if (inst->lib_handle) return 1;
	sprintf(path, "%s%c%s", inst->plugman->dir, GF_PATH_SEPARATOR, inst->szName);

#ifdef RTLD_GLOBAL
	_flags =RTLD_LAZY | RTLD_GLOBAL;
#else
	_flags =RTLD_LAZY;
#endif
	
	ModuleLib = dlopen(path, _flags);
	if (!ModuleLib) return 0;
	inst->query_func = (QueryInterface) dlsym(ModuleLib, "QueryInterface");
	inst->load_func = (LoadInterface) dlsym(ModuleLib, "LoadInterface");
	inst->destroy_func = (ShutdownInterface) dlsym(ModuleLib, "ShutdownInterface");
	inst->lib_handle = ModuleLib;
	return 1;

}

void gf_modules_unload_library(ModuleInstance *inst)
{
	if (!inst->lib_handle || gf_list_count(inst->interfaces)) return;
	dlclose(inst->lib_handle);
	inst->lib_handle = NULL;
	inst->load_func = NULL;
	inst->destroy_func = NULL;
	inst->query_func = NULL;
}

/*refresh modules - note we don't check for deleted modules but since we've open them the OS should forbid delete*/
u32 gf_modules_refresh(GF_ModuleManager *pm)
{
#if CHECK_MODULE
	QueryInterface query_func;
	LoadInterface load_func;
	ShutdownInterface del_func;
	s32 _flags;
	void *ModuleLib;
#endif
	ModuleInstance *inst;
	unsigned char file[GF_MAX_PATH];

	DIR *the_dir;
	struct dirent* the_file;
	struct stat st;
	
	if (!pm) return 0;

	the_dir = opendir(pm->dir);
	if (the_dir == NULL) return 0;

	the_file = readdir(the_dir);
	while (the_file) {

		/*Load the current file*/
		sprintf(file, "%s%c", pm->dir, GF_PATH_SEPARATOR);

		if (!strcmp(the_file->d_name, "..")) goto next;
		if (the_file->d_name[0] == '.') goto next;

		strcat(file, the_file->d_name);
		/*filter directories*/
		if (stat(file, &st ) != 0) goto next;
		if ( (st.st_mode & S_IFMT) == S_IFDIR) goto next;

#if CHECK_MODULE

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

		if (!load_func || !query_func || !del_func) {
			dlclose(ModuleLib);
			goto next;
		}

		if (gf_module_is_loaded(pm, file) ) {
			dlclose(ModuleLib);
			goto next;
		}
		dlclose(ModuleLib);
#else
		if (strncmp(the_file->d_name, "gm_", 3)) goto next;
#endif
		GF_SAFEALLOC(inst, sizeof(ModuleInstance));
		inst->interfaces = gf_list_new();
		inst->plugman = pm;
		strcpy(inst->szName, the_file->d_name);
		gf_list_add(pm->plug_list, inst);

next:
		the_file = readdir(the_dir);
	}

	closedir(the_dir);

	return gf_list_count(pm->plug_list);
}

