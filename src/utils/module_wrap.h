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

#include <gpac/module.h>
#include <gpac/list.h>

#ifndef _GF_MODULE_WRAP_H_
#define _GF_MODULE_WRAP_H_

#define MAX_MODULE_DIRS 1024

/* interface api*/
typedef const u32 *(*QueryInterfaces) ();
typedef void * (*LoadInterface) (u32 InterfaceType);
typedef void (*ShutdownInterface) (void *interface_obj);


typedef struct
{
	struct __tag_mod_man *plugman;
	char *name;
	GF_List *interfaces;
	
	/*for static modules*/
	GF_InterfaceRegister *ifce_reg;

	/*library is loaded only when an interface is attached*/
	void *lib_handle;
	QueryInterfaces query_func;
	LoadInterface load_func;
	ShutdownInterface destroy_func;
	char* dir;
} ModuleInstance;


struct __tag_mod_man
{
	/*location of the modules*/
	const char* dirs[MAX_MODULE_DIRS];
	u32 num_dirs;
	GF_List *plug_list;
	GF_Config *cfg;
	Bool no_unload;
	/*the one and only ssl instance used throughout the client engine*/
	void *ssl_inst;

	/*all static modules store their InterfaceRegistry here*/
	GF_List *plugin_registry;
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

