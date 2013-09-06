/*
 *		Copyright (c) 2010-2011 Telecom-Paristech
 *                 All Rights Reserved
 *	GPAC is free software; you can redistribute it and/or modify
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
 *		Authors:    Stanislas Selle		
 *				
 */
#ifndef    __VIDEOBROADCAST_H__
#define    __VIDEOBROADCAST_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <npapi.h>
#include <nptypes.h>
#include <npfunctions.h>
#include <npruntime.h>

#include "hbbtvbrowserplugin.h"

#define FULLSIZE_WIDTH 1280
#define FULLSIZE_HEIGHT 720

typedef struct
{
	NPObject header;	
	NPP npp; 
	NPBool      fullscreen;
	int32_t     width;              
    int32_t     height; 
    
    
    //~ char*           pcArg_onChannelChangeSucceeded;
    //~ NPObject*       onChannelChangeSucceeded;
    //NPObject*   channelConfig;
    
} NPObj_VidBrc;

NPClass* fillVIDBRCpclass(void);

NPObject *  VIDBRC_Allocate(NPP npp, NPClass *aClass);
void        VIDBRC_Deallocate(NPObject *obj);
void        VIDBRC_Invalidate(NPObject *obj);
bool        VIDBRC_HasMethod(NPObject *obj, NPIdentifier name);
bool        VIDBRC_Invoke(NPObject *obj, NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result);
bool        VIDBRC_InvokeDefault(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result);
bool        VIDBRC_HasProperty(NPObject *obj, NPIdentifier name);
bool        VIDBRC_GetProperty(NPObject *obj, NPIdentifier name, NPVariant *result);
bool        VIDBRC_SetProperty(NPObject *obj, NPIdentifier name, const NPVariant *value);
bool        VIDBRC_RemoveProperty(NPObject *npobj, NPIdentifier name);
bool        VIDBRC_Enumerate(NPObject *npobj, NPIdentifier **value, uint32_t *count);

void		VIDBRC_Invoke_setFullScreen(NPObj_VidBrc* obj,const NPVariant* args, uint32_t argCount);
void		VIDBRC_Invoke_bindToCurrentChannel(NPObject* obj,const NPVariant* args, uint32_t argCount);
void		VIDBRC_Invoke_getChannelConfig(NPObj_VidBrc* obj,const NPVariant* args, uint32_t argCount, NPVariant* result);

void		VIDBRC_setsize(NPObj_VidBrc* obj,int32_t witdh, int32_t height);
#endif
