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
#ifndef    __OIPFAPPLICATIONMANAGER_H__
#define    __OIPFAPPLICATIONMANAGER_H__

#include "hbbtvbrowserplugin.h"

typedef struct {
    NPObject    header;
    NPP         npp;
    
    NPObject*   ownerApplication; 
}  NPObj_OAM;

NPClass* fillOAMpclass(void);

NPObject *  OAM_Allocate(NPP npp, NPClass *aClass);
void        OAM_Deallocate(NPObject *obj);
void        OAM_Invalidate(NPObject *obj);
bool        OAM_HasMethod(NPObject *obj, NPIdentifier name);
bool        OAM_Invoke(NPObject *obj, NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result);
bool        OAM_InvokeDefault(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result);
bool        OAM_HasProperty(NPObject *obj, NPIdentifier name);
bool        OAM_GetProperty(NPObject *obj, NPIdentifier name, NPVariant *result);
bool        OAM_SetProperty(NPObject *obj, NPIdentifier name, const NPVariant *value);
bool        OAM_RemoveProperty(NPObject *npobj, NPIdentifier name);
bool        OAM_Enumerate(NPObject *npobj, NPIdentifier **value, uint32_t *count);

void 		OAM_ObjectMain_Invoke_GetOwnerApplication(NPObj_OAM* obj,const NPVariant* args, uint32_t argCount, NPVariant* result);

/*
typedef struct{
	NPObject	headear;
	NPP			npp;
	NPObject*	onLowMemory;
	NPObject* 	ownerApplication;
	NPObject* 	newApplication;
} OAM_ObjectMain;
*/


#endif
