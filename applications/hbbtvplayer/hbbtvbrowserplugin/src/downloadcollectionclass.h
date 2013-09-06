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
#ifndef __DWLDCOLLECTIONCOLLECTIONCLASS_H__
#define __DWLDCOLLECTIONCOLLECTIONCLASS_H__

#include "hbbtvbrowserplugin.h"


NPClass* fillDWLDCOLLECTIONpclass(void);

NPObject *  DWLDCOLLECTION_Allocate(NPP npp, NPClass *aClass);
void        DWLDCOLLECTION_Deallocate(NPObject *obj);
void        DWLDCOLLECTION_Invalidate(NPObject *obj);
bool        DWLDCOLLECTION_HasMethod(NPObject *obj, NPIdentifier name);
bool        DWLDCOLLECTION_Invoke(NPObject *obj, NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result);
bool        DWLDCOLLECTION_InvokeDefault(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result);
bool        DWLDCOLLECTION_HasProperty(NPObject *obj, NPIdentifier name);
bool        DWLDCOLLECTION_GetProperty(NPObject *obj, NPIdentifier name, NPVariant *result);
bool        DWLDCOLLECTION_SetProperty(NPObject *obj, NPIdentifier name, const NPVariant *value);

bool        DWLDCOLLECTION_RemoveProperty(NPObject *npobj, NPIdentifier name);

bool        DWLDCOLLECTION_Enumerate(NPObject *npobj, NPIdentifier **value, uint32_t *count);

void DWLDCOLLECTION_Invoke_Item(NPObject* obj,const NPVariant* args, uint32_t argCount);
#endif
