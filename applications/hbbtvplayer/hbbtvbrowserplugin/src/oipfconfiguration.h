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
#ifndef    __OIPFCONFIGURATION_H__
#define    __OIPFCONFIGURATION_H__

#include "hbbtvbrowserplugin.h"

NPClass* fillOCFGpclass(void);

NPObject *  OCFG_Allocate(NPP npp, NPClass *aClass);
void        OCFG_Deallocate(NPObject *obj);
void        OCFG_Invalidate(NPObject *obj);
bool        OCFG_HasMethod(NPObject *obj, NPIdentifier name);
bool        OCFG_Invoke(NPObject *obj, NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result);
bool        OCFG_InvokeDefault(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result);
bool        OCFG_HasProperty(NPObject *obj, NPIdentifier name);
bool        OCFG_GetProperty(NPObject *obj, NPIdentifier name, NPVariant *result);
bool        OCFG_SetProperty(NPObject *obj, NPIdentifier name, const NPVariant *value);

bool        OCFG_RemoveProperty(NPObject *npobj, NPIdentifier name);

bool        OCFG_Enumerate(NPObject *npobj, NPIdentifier **value, uint32_t *count);


#endif
