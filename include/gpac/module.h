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

#ifndef _GF_MODULE_H_
#define _GF_MODULE_H_

#ifdef __cplusplus
extern "C" {
#endif

/*!
 *	\file <gpac/module.h>
 *	\brief plugable dynamic module.
 */

/*!
 *	\ingroup mods_grp
 *	\brief Plugable Dynamic Modules
 *
 *This section documents the plugable module functions of the GPAC framework.
 *A module is a dynamic/shared library providing one or several interfaces to the GPAC framework.
 *A module cannot provide several interfaces of the same type. Each module must export the following functions:
 \code
 *	u32 *QueryInterfaces(u32 interface_type);
 \endcode
 *	This function is used to query supported interfaces. It returns a zero-terminated array of supported interface types.\n
 \code
 	GF_BaseInterface *LoadInterface(u32 interface_type);
 \endcode
 *	This function is used to load an interface. It returns the interface object, NULL if error.\n
 \code
 	void ShutdownInterface(GF_BaseInterface *interface);
 \endcode
 *This function is used to destroy an interface.\n\n
 *Each interface must begin with the interface macro in order to be type-casted to the base interface structure.
 \code
	struct {
		GF_DECL_MODULE_INTERFACE
		extensions;
	};
 \endcode
 *	@{
 */

#include <gpac/config_file.h>

typedef struct __tag_mod_man GF_ModuleManager;

/*!
 *\brief common module interface
 *\hideinitializer
 *
 *This is the module interface declaration macro. It must be placed first in an interface structure declaration.
*/
#define GF_DECL_MODULE_INTERFACE	\
	u32 InterfaceType;				\
	const char *module_name;		\
	const char *author_name;		\
	void *HPLUG;					\
 
/*!
 *\brief Base Interface
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
 *\brief module interface registration
 *\hideinitializer
 *
 *This is the module interface registration macro. A module must call this macro whenever creating a new interface.
 *- \e _ifce: interface being registered
 *- \e _ifce_type: the four character code defining the interface type.
 *- \e _ifce_name: a printable string giving the interface name (const char *).
 *- \e _ifce_author: a printable string giving the author name (const char *).
 *\n
 *This is a sample GPAC codec interface declaration:
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
 *\brief module interface function export. Modules that can be compiled in libgpac rather than in sharde libraries shall use this macro to declare the 3 exported functions
 *\hideinitializer
 *
*/
#ifdef GPAC_STATIC_MODULES
#define GPAC_MODULE_EXPORT	static
#else
#define GPAC_MODULE_EXPORT	GF_EXPORT
#endif

/*!
 *\brief Interface Registry
 *
 *This structure represent a base interface loader, when not using dynamic / shared libraries
 */
typedef struct
{
	const char *name;
	const u32 *(*QueryInterfaces) ();
	GF_BaseInterface * (*LoadInterface) (u32 InterfaceType);
	void (*ShutdownInterface) (GF_BaseInterface *interface_obj);
} GF_InterfaceRegister;

/*!
 *\brief module interface function export. Modules that can be compiled in libgpac rather than in sharde libraries shall use this macro to declare the 3 exported functions
 *\hideinitializer
 *
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
 *\brief module manager construtcor
 *
 *Constructs a module manager object.
 *\param directory absolute path to the directory where the manager shall look for modules
 *\param cfgFile GPAC configuration file handle. If this is NULL, the modules won't be able to share the configuration
 *file with the rest of the GPAC framework.
 *\return the module manager object
*/
GF_ModuleManager *gf_modules_new(const char *directory, GF_Config *cfgFile);

/*!
 *\brief module manager destructor
 *
 *Destroys the module manager
 *\param pm the module manager
 */
void gf_modules_del(GF_ModuleManager *pm);

/*!
 *\brief load a static module given its interface function
 *
 *\param pm the module manager
 *\param register_module the register interface function
 */
GF_Err gf_module_load_static(GF_ModuleManager *pm, GF_InterfaceRegister *(*register_module)());

/*!
 *\brief declare a module for loading
 *
 * When using GPAC as a static library, if GPAC_MODULE_CUSTOM_LOAD is
 * defined, this macro can be used with GF_MODULE_STATIC_DECLARE() and
 * gf_module_refresh() to load individual modules.
 *
 * It's first needed to call GF_MODULE_STATIC_DECLARE() with the name
 * of the module you need to load outside of any function. This macro
 * will declare the prototype of the module registration function so
 * it should only be used outside functions.
 *
 * Then in your GPAC initialization code, you need to call
 * GF_MODULE_LOAD_STATIC() with your GPAC module manager and the
 * module name.
 *
 * Finally, you'll need to call gf_modules_refresh() with your module
 * manager.
 *
 * \code
 * GF_MODULE_STATIC_DECLARE(aac_in);
 * GF_MODULE_STATIC_DECLARE(audio_filter);
 * GF_MODULE_STATIC_DECLARE(ffmpeg);
 * ...
 *
 * void prepare() {
 *     GF_User user;
 *     ...
 *     user.modules = gf_modules_new("/data/gpac/modules", user.config);
 *
 *     GF_MODULE_LOAD_STATIC(user.modules, aac_in);
 *     GF_MODULE_LOAD_STATIC(user.modules, audio_filter);
 *     GF_MODULE_LOAD_STATIC(user.modules, ffmpeg);
 *     ...
 *     gf_modules_refresh(user.modules);
 *    ...
 * }
 * \endcode
 * \see GF_MODULE_LOAD_STATIC() gf_modules_refresh()
 */
#ifdef __cplusplus
#define GF_MODULE_STATIC_DECLARE(_name)				\
	extern "C" GF_InterfaceRegister *gf_register_module_##_name()
#else
#define GF_MODULE_STATIC_DECLARE(_name)				\
	GF_InterfaceRegister *gf_register_module_##_name()
#endif
/*!
 *\brief load a static module given its name
 *
 * Use this function to load a statically compiled
 * module. GF_MODULE_STATIC_DECLARE() should be called before and
 * gf_modules_refresh() after loading all the needed modules.
 *
 *\param _pm the module manager
 *\param _name the module name
 *\see GF_MODULE_STATIC_DECLARE() gf_modules_refresh()
 */
#define GF_MODULE_LOAD_STATIC(_pm, _name)			\
	gf_module_load_static(_pm,gf_register_module_##_name)


/*!
 *\brief refreshes modules
 *
 *Refreshes all modules in the manager directory and load unloaded ones
 *\param pm the module manager
 *\return the number of loaded modules
 */
u32 gf_modules_refresh(GF_ModuleManager *pm);

/*!
 *\brief get module count
 *
 *Gets the number of modules found in the manager directory
 *\param pm the module manager
 *\return the number of loaded modules
 */
u32 gf_modules_get_count(GF_ModuleManager *pm);

/*!
 *\brief get all modules directories
 *
 * Update module manager with all modules directories
 *\param pm the module manager
 *\param num_dirs the number of module directories
 *\return The list of modules directories
 */
const char **gf_modules_get_module_directories(GF_ModuleManager *pm, u32* num_dirs);

/*!
 *\brief get module file name
 *
 *Gets a module shared library file name based on its index
 *\param pm the module manager
 *\param index the 0-based index of the module to query
 *\return the name of the shared library module
 */
const char *gf_modules_get_file_name(GF_ModuleManager *pm, u32 index);

/*!
 *\brief get module file name
 *
 *Gets a module shared library file name based on its index
 *\param ifce the module instance to query
 *\return the name of the shared library module
 */
const char *gf_module_get_file_name(GF_BaseInterface *ifce);

/*!
 *\brief loads an interface
 *
 *Loads an interface in the desired module.
 *\param pm the module manager
 *\param index the 0-based index of the module to load the interface from
 *\param InterfaceFamily type of the interface to load
 *\return the interface object if found and loaded, NULL otherwise.
 */
GF_BaseInterface *gf_modules_load_interface(GF_ModuleManager *pm, u32 index, u32 InterfaceFamily);

/*!
 *\brief loads an interface by module name
 *
 *Loads an interface in the desired module
 *\param pm the module manager
 *\param mod_name the name of the module (shared library file) or of the interface as declared when registered.
 *\param InterfaceFamily type of the interface to load
 *\return the interface object if found and loaded, NULL otherwise.
 */
GF_BaseInterface *gf_modules_load_interface_by_name(GF_ModuleManager *pm, const char *mod_name, u32 InterfaceFamily);

/*!
 *\brief interface shutdown
 *
 *Closes an interface

 *\param interface_obj the interface to close
 */
GF_Err gf_modules_close_interface(GF_BaseInterface *interface_obj);

/*!
 *\brief interface option query
 *
 *Gets an option from the config file associated with the module manager
 *\param interface_obj the interface object used
 *\param secName the desired key parent section name
 *\param keyName the desired key name
 *\return the desired key value if found, NULL otherwise.
 */
const char *gf_modules_get_option(GF_BaseInterface *interface_obj, const char *secName, const char *keyName);
/*!
 *\brief interface option update
 *
 *Sets an option in the config file associated with the module manager
 *\param interface_obj the interface object used
 *\param secName the desired key parent section name
 *\param keyName the desired key name
 *\param keyValue the desired key value
 *\note this will also create both section and key if they are not found in the configuration file
 */
GF_Err gf_modules_set_option(GF_BaseInterface *interface_obj, const char *secName, const char *keyName, const char *keyValue);

/*!
 *\brief get config file
 *
 *Gets the configuration file for the module instance
 *\param ifce the interface object used
 *\return handle to the config file
 */
GF_Config *gf_modules_get_config(GF_BaseInterface *ifce);

/*! @} */

#ifdef __cplusplus
}
#endif


#endif		/*_GF_MODULE_H_*/
