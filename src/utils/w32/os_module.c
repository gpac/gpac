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


#include <windows.h>
#include "../module_wrap.h"

/*delete all interfaces loaded on object*/
void gf_modules_free_module(ModuleInstance *inst)
{
	void *objinterface;
	while (gf_list_count(inst->interfaces)) {
		objinterface = gf_list_get(inst->interfaces, 0);
		gf_list_rem(inst->interfaces, 0);
		inst->destroy_func(objinterface);
	}

	if (inst->lib_handle) FreeLibrary(inst->lib_handle);
	gf_list_del(inst->interfaces);
	free(inst);
}

Bool gf_modules_load_library(ModuleInstance *inst)
{
	HMODULE ModuleLib;
	char path[GF_MAX_PATH];
	if (inst->lib_handle) return 1;
	sprintf(path, "%s%c%s", inst->plugman->dir, GF_PATH_SEPARATOR, inst->szName);
	ModuleLib = LoadLibrary(path);
	if (!ModuleLib) return 0;
	inst->query_func = (QueryInterface) GetProcAddress(ModuleLib, "QueryInterface");
	inst->load_func = (LoadInterface) GetProcAddress(ModuleLib, "LoadInterface");
	inst->destroy_func = (ShutdownInterface) GetProcAddress(ModuleLib, "ShutdownInterface");
	inst->lib_handle = ModuleLib;
	return 1;
}

void gf_modules_unload_library(ModuleInstance *inst)
{
	if (!inst->lib_handle || gf_list_count(inst->interfaces)) return;
	FreeLibrary(inst->lib_handle);
	inst->lib_handle = NULL;
	inst->load_func = NULL;
	inst->destroy_func = NULL;
	inst->query_func = NULL;
}

/*refresh modules - note we don't check for deleted modules but since we've open them the OS should forbid delete*/
u32 gf_modules_refresh(GF_ModuleManager *pm)
{
	QueryInterface query_func;
	LoadInterface load_func;
	ShutdownInterface del_func;
	ModuleInstance *inst;
	unsigned char path[GF_MAX_PATH];
	unsigned char file[GF_MAX_PATH];

	WIN32_FIND_DATA FindData;
	HANDLE SearchH;
	HMODULE ModuleLib;
	
	if (!pm) return 0;

	sprintf(path, "%s%c*", pm->dir, GF_PATH_SEPARATOR);
	SearchH= FindFirstFile(path, &FindData);
	if (SearchH == INVALID_HANDLE_VALUE) return 0;
	while (SearchH != INVALID_HANDLE_VALUE) {
	
		if (!strstr(FindData.cFileName, ".dll")) goto next;
		if (!strcmp(FindData.cFileName, "nposmozilla.dll")) goto next;

		/*Load the current file*/
		sprintf(file, "%s%c%s", pm->dir, GF_PATH_SEPARATOR, FindData.cFileName);

		ModuleLib = LoadLibrary(file);
		if (!ModuleLib) goto next;

		strcpy(file, FindData.cFileName);
		while (file[strlen(file)-1] != '.') file[strlen(file)-1] = 0;
		file[strlen(file)-1] = 0;

		query_func = (QueryInterface) GetProcAddress(ModuleLib, "QueryInterface");
		load_func = (LoadInterface) GetProcAddress(ModuleLib, "LoadInterface");
		del_func = (ShutdownInterface) GetProcAddress(ModuleLib, "ShutdownInterface");

		if (!load_func || !query_func || !del_func) {
			FreeLibrary(ModuleLib);
			goto next;
		}
		
		if (gf_module_is_loaded(pm, file) ) {
			FreeLibrary(ModuleLib);
			goto next;
		}

		FreeLibrary(ModuleLib);

		SAFEALLOC(inst, sizeof(ModuleInstance));
		inst->interfaces = gf_list_new();
		inst->plugman = pm;
		strcpy(inst->szName, file);
		gf_list_add(pm->plug_list, inst);

next:
		if (!FindNextFile(SearchH, &FindData)) {
			FindClose(SearchH);
			SearchH = INVALID_HANDLE_VALUE;
		}
	}

	return gf_list_count(pm->plug_list);
}

