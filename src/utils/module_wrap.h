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

#include <gpac/module.h>
#include <gpac/list.h>

#ifndef _GF_MODULE_WRAP_H_
#define _GF_MODULE_WRAP_H_

/* interface api*/
typedef Bool (*QueryInterface) (u32 InterfaceType);
typedef void * (*LoadInterface) (u32 InterfaceType);
typedef void (*ShutdownInterface) (void *interface_obj);


typedef struct
{
	struct __tag_mod_man *plugman;
	char szName[GF_MAX_PATH];
	GF_List *interfaces;
	
	/*library is loaded only when an interface is attached*/
	void *lib_handle;
	QueryInterface query_func;
	LoadInterface load_func;
	ShutdownInterface destroy_func;
} ModuleInstance;


struct __tag_mod_man
{
	/*location of the modules*/
	char dir[GF_MAX_PATH];
	GF_List *plug_list;
	GF_Config *cfg;
	/*the one and only ssl instance used throughout the client engine*/
	void *ssl_inst;
};

#ifdef __cplusplus
extern "C" {
#endif
	
/*returns 1 if a module with the same filename is already loaded*/
Bool gf_module_is_loaded(GF_ModuleManager *pm, char *filename);

/*these are OS specific*/
void gf_modules_free_module(ModuleInstance *inst);
Bool gf_modules_load_library(ModuleInstance *inst);
void gf_modules_unload_library(ModuleInstance *inst);
u32 gf_modules_refresh(GF_ModuleManager *pm);

#ifdef __cplusplus
}
#endif

#endif	/*_GF_MODULE_WRAP_H_*/

