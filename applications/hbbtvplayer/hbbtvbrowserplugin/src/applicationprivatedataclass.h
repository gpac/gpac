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
#ifndef __APPLICATIONPRIVATEDATACLASS_H__
#define __APPLICATIONPRIVATEDATACLASS_H__

#include "keysetclass.h"
#include "hbbtvbrowserplugin.h"

typedef struct
{
	NPObject header;	
	NPP npp; 

    NPObject*   keyset;        
    NPObject*   currentChannel;	
} NPObj_AppPrivData;

NPClass* fillAPPPRIVDATApclass(void);

NPObject *  APPPRIVDATA_Allocate(NPP npp, NPClass *aClass);
void        APPPRIVDATA_Deallocate(NPObject *obj);
void        APPPRIVDATA_Invalidate(NPObject *obj);
bool        APPPRIVDATA_HasMethod(NPObject *obj, NPIdentifier name);
bool        APPPRIVDATA_Invoke(NPObject *obj, NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result);
bool        APPPRIVDATA_InvokeDefault(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result);
bool        APPPRIVDATA_HasProperty(NPObject *obj, NPIdentifier name);
bool        APPPRIVDATA_GetProperty(NPObject *obj, NPIdentifier name, NPVariant *result);
bool        APPPRIVDATA_SetProperty(NPObject *obj, NPIdentifier name, const NPVariant *value);

bool        APPPRIVDATA_RemoveProperty(NPObject *npobj, NPIdentifier name);

bool        APPPRIVDATA_Enumerate(NPObject *npobj, NPIdentifier **value, uint32_t *count);


void APPPRIVDATA_Invoke_GetFreeMen(NPObject* obj,const NPVariant* args, uint32_t argCount);

#endif
