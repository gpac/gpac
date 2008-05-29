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

#ifndef _GF_CONFIG_FILE_H_
#define _GF_CONFIG_FILE_H_

#ifdef __cplusplus
extern "C" {
#endif

/*!
 *	\file <gpac/config_file.h>
 *	\brief configuration file functions.
 */

 /*!
 *	\addtogroup cfg_grp configuration
 *	\ingroup utils_grp
 *	\brief Configuration File object
 *
 *	This section documents the configuration file object of the GPAC framework.
 *	This file is formatted as the INI file mode of WIN32 in sections and keys.\n
 *\note For more information on the GPAC configuration file itself, please refer to the GPAC configuration help provided with GPAC.
 *	@{
 */

#include <gpac/tools.h>


typedef struct __tag_config GF_Config;

/*!
 *	\brief configuration file constructor
 *
 *Constructs a configuration file
 *\param filePath directory the file is located in
 *\param fileName name of the configuration file
 *\return the configuration file object
 */
GF_Config *gf_cfg_new(const char *filePath, const char *fileName);
/*!
 *	\brief configuration file destructor
 *
 *Destroys the configuration file and saves it if needed.
 *\param cfgFile the target configuration file
 */
void gf_cfg_del(GF_Config *cfgFile);
/*!
 *	\brief configuration saving
 *
 *Saves the configuration file if modified.
 *\param cfgFile the target configuration file
 */
GF_Err gf_cfg_save(GF_Config *iniFile);
/*!
 *	\brief key value query
 *
 *Gets a key value from its section and name.
 *\param cfgFile the target configuration file
 *\param secName the desired key parent section name
 *\param keyName the desired key name
 *\return the desired key value if found, NULL otherwise.
 */
const char *gf_cfg_get_key(GF_Config *cfgFile, const char *secName, const char *keyName);
/*!
 *	\brief key value update
 *
 *Sets a key value from its section and name.
 *\param cfgFile the target configuration file
 *\param secName the desired key parent section name
 *\param keyName the desired key name
 *\param keyValue the desired key value
 *\note this will also create both section and key if they are not found in the configuration file
 */
GF_Err gf_cfg_set_key(GF_Config *cfgFile, const char *secName, const char *keyName, const char *keyValue);
/*!
 *	\brief section count query
 *
 *Gets the number of sections in the configuration file
 *\param cfgFile the target configuration file
 *\return the number of sections
 */
u32 gf_cfg_get_section_count(GF_Config *cfgFile);
/*!
 *	\brief section name query
 *
 *Gets a section name based on its index
 *\param cfgFile the target configuration file
 *\param secIndex 0-based index of the section to query
 *\return the section name if found, NULL otherwise
 */
const char *gf_cfg_get_section_name(GF_Config *cfgFile, u32 secIndex);
/*!
 *	\brief key count query
 *
 *Gets the number of keys in a section of the configuration file
 *\param cfgFile the target configuration file
 *\param secName the target section
 *\return the number of keys in the section
 */
u32 gf_cfg_get_key_count(GF_Config *cfgFile, const char *secName);
/*!
 *	\brief key count query
 *
 *Gets the number of keys in a section of the configuration file
 *\param cfgFile the target configuration file
 *\param secName the target section
 *\param keyIndex 0-based index of the key in the section
 *\return the key name if found, NULL otherwise
 */
const char *gf_cfg_get_key_name(GF_Config *cfgFile, const char *secName, u32 keyIndex);

/*!
 *	\brief key insertion
 *
 *Inserts a new key in a given section. Returns an error if a key with the given name 
 *already exists in the section
 *\param cfgFile the target configuration file
 *\param secName the target section
 *\param keyName the name of the target key
 *\param keyValue the value for the new key
 *\param index the 0-based index position of the new key
 */
GF_Err gf_cfg_insert_key(GF_Config *cfgFile, const char *secName, const char *keyName, const char *keyValue, u32 index);

/*!
 *	\brief section destrouction
 *
 *Removes all entries in the given section
 *\param cfgFile the target configuration file
 *\param secName the target section
 */
void gf_cfg_del_section(GF_Config *cfgFile, const char *secName);

/*! @} */


#ifdef __cplusplus
}
#endif


#endif		/*_GF_CONFIG_FILE_H_*/

