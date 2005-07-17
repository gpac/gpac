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
#include <windows.h>


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

static Bool gf_module_is_loaded_ce(GF_ModuleManager *pm, unsigned short *filename) 
{
	unsigned char szName[1024];
	u32 i;
	ModuleInstance *inst;
	CE_WideToChar(filename, szName);
	for (i=0; i<gf_list_count(pm->plug_list); i++) {
		inst = gf_list_get(pm->plug_list, i);
		if (!strcmp(inst->szName, szName)) return 1;
	}
	return 0;
}


Bool gf_modules_load_library(ModuleInstance *inst)
{
	HMODULE ModuleLib;
	unsigned short w_path[GF_MAX_PATH];
	char path[GF_MAX_PATH];
	if (inst->lib_handle) return 1;
	sprintf(path, "%s%c%s", inst->plugman->dir, GF_PATH_SEPARATOR, inst->szName);
	CE_CharToWide(path, w_path);

	ModuleLib = LoadLibrary(w_path);
	if (!ModuleLib) return 0;
	inst->query_func = (QueryInterface) GetProcAddress(ModuleLib, _T("QueryInterface"));
	inst->load_func = (LoadInterface) GetProcAddress(ModuleLib, _T("LoadInterface"));
	inst->destroy_func = (ShutdownInterface) GetProcAddress(ModuleLib, _T("ShutdownInterface"));
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

	unsigned short w_path[GF_MAX_PATH];
	unsigned short w_file[GF_MAX_PATH];

	unsigned char path[GF_MAX_PATH];
	unsigned char file[GF_MAX_PATH];

	WIN32_FIND_DATA FindData;
	HANDLE SearchH;
	HMODULE ModuleLib;
	
	if (!pm) return 0;

	sprintf(path, "%s%c*", pm->dir, GF_PATH_SEPARATOR);

	CE_CharToWide(path, w_path);
	SearchH= FindFirstFile(w_path, &FindData);

	if (SearchH == INVALID_HANDLE_VALUE) return 0;
	while (SearchH != INVALID_HANDLE_VALUE) {
	
		/*Load the current file*/
		sprintf(file, "%s%c", pm->dir, GF_PATH_SEPARATOR);

		if (!wcscmp(FindData.cFileName, _T("."))) goto next;
		if (!wcscmp(FindData.cFileName, _T("..") )) goto next;
		if (!wcsncmp(FindData.cFileName, _T("."), 1)) goto next;
		if (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) goto next;

		MultiByteToWideChar(CP_ACP, 0, file, -1, w_file, sizeof(w_file)/sizeof(TCHAR));
		wcscat(w_file, FindData.cFileName);
		if (wcsstr(w_file, _T(".")) ) {
			while (w_file[wcslen(w_file)-1] != '.') w_file[wcslen(w_file)-1] = 0;
			w_file[wcslen(w_file)-1] = 0;
		}

		ModuleLib = LoadLibrary(w_file);
		if (!ModuleLib) goto next;

		query_func = (QueryInterface) GetProcAddress(ModuleLib, _T("QueryInterface"));
		load_func = (LoadInterface) GetProcAddress(ModuleLib, _T("LoadInterface"));
		del_func = (ShutdownInterface) GetProcAddress(ModuleLib, _T("ShutdownInterface"));

		if (!load_func || !query_func || !del_func) {
			FreeLibrary(ModuleLib);
			goto next;
		}
		
		if (gf_module_is_loaded_ce(pm, w_file) ) {
			FreeLibrary(ModuleLib);
			goto next;
		}

		wcscpy(w_file, FindData.cFileName);
		if (wcsstr(w_file, _T(".")) ) {
			while (w_file[wcslen(w_file)-1] != '.') w_file[wcslen(w_file)-1] = 0;
			w_file[wcslen(w_file)-1] = 0;
		}
		CE_WideToChar(w_file, file);

		FreeLibrary(ModuleLib);
		GF_SAFEALLOC(inst, sizeof(ModuleInstance));
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

