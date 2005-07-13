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

#ifndef _GF_CONFIG_H_
#define _GF_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/tools.h>


/*********************************************************************
			a simple config file formatted as .ini of Win32 for now
**********************************************************************/

typedef struct __tag_config GF_Config;

/* load a ini file. If the file is not found the object is still created - filePath may be NULL*/
GF_Config *gf_cfg_new(const char *filePath, const char *fileName);
/* delete and save the ini file if needed */
void gf_cfg_del(GF_Config *iniFile);
/* return the curent key - do NOT delete it */
char *gf_cfg_get_key(GF_Config *iniFile, const char *secName, const char *keyName);
/* set a key value - create section and/or key if needed*/
GF_Err gf_cfg_set_key(GF_Config *iniFile, const char *secName, const char *keyName, const char *keyValue);
/*get number of keys in a section*/
u32 gf_cfg_get_section_count(GF_Config *iniFile);
/*get section - secIndex is 0-based*/
const char *gf_cfg_get_section_name(GF_Config *iniFile, u32 secIndex);
/*get number of keys in a section*/
u32 gf_cfg_get_key_count(GF_Config *iniFile, const char *secName);
/*get key in a section - keyIndex is 0-based*/
const char *gf_cfg_get_key_name(GF_Config *iniFile, const char *secName, u32 keyIndex);



#ifdef __cplusplus
}
#endif


#endif		/*_GF_CONFIG_H_*/

