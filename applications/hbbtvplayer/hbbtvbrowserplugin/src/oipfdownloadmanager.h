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
#ifndef __OIPFDOWNLOADMANAGER_H__
#define __OIPFDOWNLOADMANAGER_H__

#include "hbbtvbrowserplugin.h"


NPClass* fillODWLDMANpclass(void);

NPObject *  ODWLDMAN_Allocate(NPP npp, NPClass *aClass);
void        ODWLDMAN_Deallocate(NPObject *obj);
void        ODWLDMAN_Invalidate(NPObject *obj);
bool        ODWLDMAN_HasMethod(NPObject *obj, NPIdentifier name);
bool        ODWLDMAN_Invoke(NPObject *obj, NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result);
bool        ODWLDMAN_InvokeDefault(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result);
bool        ODWLDMAN_HasProperty(NPObject *obj, NPIdentifier name);
bool        ODWLDMAN_GetProperty(NPObject *obj, NPIdentifier name, NPVariant *result);
bool        ODWLDMAN_SetProperty(NPObject *obj, NPIdentifier name, const NPVariant *value);

bool        ODWLDMAN_RemoveProperty(NPObject *npobj, NPIdentifier name);

bool        ODWLDMAN_Enumerate(NPObject *npobj, NPIdentifier **value, uint32_t *count);


void ODWLDMAN_Invoke_GetDownloads(NPObject* obj,const NPVariant* args, uint32_t argCount);
void ODWLDMAN_Invoke_Pause(NPObject* obj,const NPVariant* args, uint32_t argCount);
void ODWLDMAN_Invoke_Remove(NPObject* obj,const NPVariant* args, uint32_t argCount);
void ODWLDMAN_Invoke_Resume(NPObject* obj,const NPVariant* args, uint32_t argCount);

#endif
