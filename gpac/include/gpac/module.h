/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2019
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

/*!
\file <gpac/module.h>
\brief plugable dynamic module.
 */

/*!
\ingroup mods_grp
\brief Plugable Dynamic Modules

This section documents the plugable module functions of the GPAC framework.
A module is a dynamic/shared library providing one or several interfaces to the GPAC framework.
A module cannot provide several interfaces of the same type. Each module must export the following functions:
 \code
 	u32 *QueryInterfaces(u32 interface_type);
 \endcode
 	This function is used to query supported interfaces. It returns a zero-terminated array of supported interface types.\n
 \code
 	GF_BaseInterface *LoadInterface(u32 interface_type);
 \endcode
 	This function is used to load an interface. It returns the interface object, NULL if error.\n
 \code
 	void ShutdownInterface(GF_BaseInterface *interface);
 \endcode
 This function is used to destroy an interface.\n\n
 Each interface must begin with the interface macro in order to be type-casted to the base interface structure.
 \code
	struct {
		GF_DECL_MODULE_INTERFACE
		extensions;
	};
 \endcode

@{
 */

#include <gpac/config_file.h>
#include <gpac/main.h>

/*!
\brief common module interface
\hideinitializer

This is the module interface declaration macro. It must be placed first in an interface structure declaration.
*/
#define GF_DECL_MODULE_INTERFACE	\
	u32 InterfaceType;				\
	const char *module_name;		\
	const char *author_name;		\
	void *HPLUG;					\
	GF_GPACArg *args; 				\
	char *description;				\

/*!
\brief Base Interface
 *
 *This structure represent a base interface, e.g. the minimal interface declaration without functionalities. Each interface is
 *type-casted to this structure and shall always be checked against its interface type. API Versioning is taken care of in the
 *interface type itsel, changing at each modification of the interface API
 */
typedef struct
{
	GF_DECL_MODULE_INTERFACE
} GF_BaseInterface;

/*!
\brief module interface registration
\hideinitializer

This is the module interface registration macro. A module must call this macro whenever creating a new interface.
 - \e _ifce: interface being registered
 - \e _ifce_type: the four character code defining the interface type.
 - \e _ifce_name: a printable string giving the interface name (const char *).
 - \e _ifce_author: a printable string giving the author name (const char *).
 \n
 This is a sample GPAC codec interface declaration:
 \code
GF_BaseInterface *MyDecoderInterfaceLoad() {
	GF_MediaDecoder *ifce;
	GF_SAFEALLOC(ifce, GF_MediaDecoder);
	GF_REGISTER_MODULE_INTERFACE(ifce, GF_MEDIA_DECODER_INTERFACE, "Sample Decoder", "The Author")
	//follows any initialization private to the decoder
	return (GF_BaseInterface *)ifce;
}
 \endcode
*/
#define GF_REGISTER_MODULE_INTERFACE(_ifce, _ifce_type, _ifce_name, _ifce_author) \
	_ifce->InterfaceType = _ifce_type;	\
	_ifce->module_name = _ifce_name ? _ifce_name : "unknown";	\
	_ifce->author_name = _ifce_author ? _ifce_author : "gpac distribution";	\

/*!
\brief static module declaration
 \hideinitializer
*/
#ifdef GPAC_STATIC_MODULES
#define GPAC_MODULE_EXPORT	static
#else
#define GPAC_MODULE_EXPORT	GF_EXPORT
#endif

/*!
\brief Interface Registry

This structure represent a base interface loader, when not using dynamic / shared libraries
 */
typedef struct
{
	/*! name of interface*/
	const char *name;
	/*! query interface callback*/
	const u32 *(*QueryInterfaces) ();
	/*! load interface callback*/
	GF_BaseInterface * (*LoadInterface) (u32 InterfaceType);
	/*! shutdown interface callback*/
	void (*ShutdownInterface) (GF_BaseInterface *interface_obj);
} GF_InterfaceRegister;

/*!
\brief module interface
\hideinitializer

Module interface function export. Modules that can be compiled in libgpac rather than in sharde libraries shall use this macro to declare the 3 exported functions
*/
#ifdef GPAC_STATIC_MODULES

#define GPAC_MODULE_STATIC_DECLARATION(__name)	\
	GF_InterfaceRegister *gf_register_module_##__name()	{	\
		GF_InterfaceRegister *reg;	\
		GF_SAFEALLOC(reg, GF_InterfaceRegister);	\
		if (!reg) return NULL;\
		reg->name = "gsm_" #__name;	\
		reg->QueryInterfaces = QueryInterfaces;	\
		reg->LoadInterface = LoadInterface;	\
		reg->ShutdownInterface = ShutdownInterface;	\
		return reg;\
	}	\

#else
#define GPAC_MODULE_STATIC_DECLARATION(__name)
#endif

/*!
\brief load a static module given its interface function

\param register_module the register interface function
\return error if any
 */
GF_Err gf_module_load_static(GF_InterfaceRegister *(*register_module)());

/*!
\brief declare a module for loading

When using GPAC as a static library, if GPAC_MODULE_CUSTOM_LOAD is
defined, this macro can be used with GF_MODULE_STATIC_DECLARE() and
gf_module_refresh() to load individual modules.
\warning function load_all_modules You will need to patch src/utils/module.c
 */
#ifdef __cplusplus
#define GF_MODULE_STATIC_DECLARE(_name)				\
	extern "C" GF_InterfaceRegister *gf_register_module_##_name()
#else
#define GF_MODULE_STATIC_DECLARE(_name)				\
	GF_InterfaceRegister *gf_register_module_##_name()
#endif
/*!
\brief load a static module given its name

Use this function to load a statically compiled module. GF_MODULE_STATIC_DECLARE() should be called before and gf_modules_refresh() after loading all the needed modules.

\param _name the module name
\see GF_MODULE_STATIC_DECLARE() gf_modules_refresh()
 */
#define GF_MODULE_LOAD_STATIC(_name)			\
	gf_module_load_static(gf_register_module_##_name)

/*!
\brief get module count

Gets the number of modules found in the manager directory
\return the number of loaded modules
 */
u32 gf_modules_count();

/*!
\brief get module file name

Gets a module shared library file name based on its index
\param index the 0-based index of the module to query
\return the name of the shared library module
 */
const char *gf_modules_get_file_name(u32 index);

/*!
\brief get module file name

Gets a module shared library file name based on its index
\param ifce the module instance to query
\return the name of the shared library module
 */
const char *gf_module_get_file_name(GF_BaseInterface *ifce);

/*!
\brief loads an interface

Loads an interface in the desired module.
\param index the 0-based index of the module to load the interface from
\param InterfaceFamily type of the interface to load
\return the interface object if found and loaded, NULL otherwise.
 */
GF_BaseInterface *gf_modules_load(u32 index, u32 InterfaceFamily);

/*!
\brief loads an interface by module name

Loads an interface in the desired module
\param mod_name the name of the module (shared library file) or of the interface as declared when registered.
\param InterfaceFamily type of the interface to load
\param rebrowse_all if GF_TRUE, locate an interface for this type if not found by name
\return the interface object if found and loaded, NULL otherwise.
 */
GF_BaseInterface *gf_modules_load_by_name(const char *mod_name, u32 InterfaceFamily, Bool rebrowse_all);

/*!
\brief interface shutdown

Closes an interface
\param interface_obj the interface to close
\return error if any
 */
GF_Err gf_modules_close_interface(GF_BaseInterface *interface_obj);


/*!
\brief module load

Loads a module based on a preferred name.
If not found, check for predefined names for the given interface type in fthe global config and loads by predefined name.
If still not found, enumerate modules.
\param ifce_type type of module interface to load
\param name name of preferred module
\return the loaded module, or NULL if not found
*/
GF_BaseInterface *gf_module_load(u32 ifce_type, const char *name);


/*!
\brief filter module load

Loads a filter module (not using GF_BaseInterface) from its index. Returns NULL if no such filter.
\param index index of the filter module to load
\param fsess opaque handle passed to the filter loader
\return the loaded filter register, or NULL if not found
*/
void *gf_modules_load_filter(u32 index, void *fsess);

/*!
\brief query module option

Loads a key from config in the module options.
\param ifce the module instance to query
\param key the key to query
\return the key value or null
*/
const char *gf_module_get_key(GF_BaseInterface *ifce, char *key);

/*!
\brief query module option as boolean

Loads a boolean key from config in the module options.
\param ifce the module instance to query
\param key_name the key to query
\return GF_TRUE if key is found and equals yes, true or 1, GF_FALSE otherwise
*/
Bool gf_module_get_bool(GF_BaseInterface *ifce, char *key_name);

/*!
\brief query module option as int

Loads an integer key from config in the module options.
\param ifce the module instance to query
\param key_name the key to query
\return integer value of key, 0 if not found
*/
Bool gf_module_get_int(GF_BaseInterface *ifce, char *key_name);

/*! @} */

#ifdef __cplusplus
}
#endif


#endif		/*_GF_MODULE_H_*/
