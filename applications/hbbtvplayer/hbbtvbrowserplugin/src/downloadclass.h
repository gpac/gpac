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
#ifndef __DOWNLOADCLASS_H__
#define __DOWNLOADCLASS_H__

#include "hbbtvbrowserplugin.h"

NPClass* fillDOWNLOADpclass(void);

NPObject *  DOWNLOAD_Allocate(NPP npp, NPClass *aClass);
void        DOWNLOAD_Deallocate(NPObject *obj);
void        DOWNLOAD_Invalidate(NPObject *obj);
bool        DOWNLOAD_HasMethod(NPObject *obj, NPIdentifier name);
bool        DOWNLOAD_Invoke(NPObject *obj, NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result);
bool        DOWNLOAD_InvokeDefault(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result);
bool        DOWNLOAD_HasProperty(NPObject *obj, NPIdentifier name);
bool        DOWNLOAD_GetProperty(NPObject *obj, NPIdentifier name, NPVariant *result);
bool        DOWNLOAD_SetProperty(NPObject *obj, NPIdentifier name, const NPVariant *value);

bool        DOWNLOAD_RemoveProperty(NPObject *npobj, NPIdentifier name);

bool        DOWNLOAD_Enumerate(NPObject *npobj, NPIdentifier **value, uint32_t *count);


#endif
