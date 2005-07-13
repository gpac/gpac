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

#ifndef _GF_MODULE_H_
#define _GF_MODULE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/config.h>


/*********************************************************************
					a generic plugable module manager
**********************************************************************/

/*****************************************************
Each module must export the following functions:

	-- returns non zero if module handle interface , 0 otherwise-- 
	Bool QueryInterface(u32 interface_type);
	-- returns interface object -- 
	void *LoadInterface(u32 interface_type);
	-- destroy interface object -- 
	void ShutdownInterface(void *interface);

Each interface must begin with the interface macro def and shall assign its interface type. Ex:
struct {
	GF_DECL_MODULE_INTERFACE

	extensions;
};

  so that each interface can be type-casted to the GF_BaseInterface structure
*****************************************************/

typedef struct __tag_mod_man GF_ModuleManager;

/*common module interface - 
- the HPLUG handle is private to the app and shall not be touched
*/
#define GF_DECL_MODULE_INTERFACE	\
	u32 InterfaceType;				\
	char *module_name;		\
	char *author_name;		\
	u32 version;				\
	void *HPLUG;					\

typedef struct
{
	GF_DECL_MODULE_INTERFACE
} GF_BaseInterface;

#define GF_REGISTER_MODULE(__plug, type, dr_name, author, vers) \
	__plug->InterfaceType = type;	\
	__plug->module_name = dr_name ? dr_name : "unknown";	\
	__plug->author_name = author ? author : "gpac distribution";	\
	__plug->version = vers ? vers : GPAC_VERSION_INT;	\
	
/*
	Module Manager Constructor in specified directory (ABSOLUTE PATH)
	cfg must be specified to allow modules to share the main cfg file
*/
GF_ModuleManager *gf_modules_new(const unsigned char *directory, GF_Config *cfg);
void gf_modules_del(GF_ModuleManager *pm);

/*refresh all modules and load unloaded ones*/
u32 gf_modules_refresh(GF_ModuleManager *pm);

/* return the number of available modules*/
u32 gf_modules_get_count(GF_ModuleManager *pm);

/* return the file name of the given module*/
const char *gf_modules_get_file_name(GF_ModuleManager *pm, u32 i);

/* get first available interface - return 0 if no interface present. otherwise interface is set to newly created interface*/
Bool gf_modules_load_interface(GF_ModuleManager *pm, u32 whichplug, u32 InterfaceFamily, void **interface_obj);

/* get interface on specified module file - return 0 if no interface present. otherwise interface is set to newly created interface*/
Bool gf_modules_load_interface_by_name(GF_ModuleManager *pm, const char *plug_name, u32 InterfaceFamily, void **interface_obj);

/* shutdown interface */
GF_Err gf_modules_close_interface(void *interface_obj);

/*get/set/enum option for module - modules should use these functions in order to use only one config file*/
char *gf_modules_get_option(void *interface_obj, const char *secName, const char *keyName);
GF_Err gf_modules_set_option(void *interface_obj, const char *secName, const char *keyName, const char *keyValue);

#ifdef __cplusplus
}
#endif


#endif		/*_GF_MODULE_H_*/

