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
#include "oipfdownloadmanager.h"

#define kODWLDMAN_ID_PROPERTY_ONDOWNLOADSTATECHANGE 0
#define kODWLDMAN_NUM_PROPERTY_IDENTIFIERS			1

#define kODWLDMAN_ID_METHOD_PAUSE					0
#define kODWLDMAN_ID_METHOD_RESUME					1
#define kODWLDMAN_ID_METHOD_REMOVE					2
#define kODWLDMAN_ID_METHOD_GETDOWNLOADS			3
#define kODWLDMAN_NUM_METHOD_IDENTIFIERS    		4

bool            v_bODWLDMANIdentifiersInitialized = false;

NPIdentifier    v_ODWLDMANPropertyIdentifiers[kODWLDMAN_NUM_PROPERTY_IDENTIFIERS];
const NPUTF8 *  v_ODWLDMANPropertyNames[kODWLDMAN_NUM_PROPERTY_IDENTIFIERS] = {
	"onDownloadStateChange"
	};

NPIdentifier    v_ODWLDMANMethodIdentifiers[kODWLDMAN_NUM_METHOD_IDENTIFIERS];
const NPUTF8 *  v_ODWLDMANMethodNames[kODWLDMAN_NUM_METHOD_IDENTIFIERS] = {
    "pause",
    "resume",
    "remove",
    "getDownloads"
};


static  void    ODWLDMANinitializeIdentifiers(void)
{
    sBrowserFuncs->getstringidentifiers( v_ODWLDMANPropertyNames, kODWLDMAN_NUM_PROPERTY_IDENTIFIERS, v_ODWLDMANPropertyIdentifiers );
    sBrowserFuncs->getstringidentifiers( v_ODWLDMANMethodNames,   kODWLDMAN_NUM_METHOD_IDENTIFIERS,   v_ODWLDMANMethodIdentifiers );
}

NPClass  stODWLDMANclass;
NPClass* pODWLDMANclass = NULL;

NPClass* fillODWLDMANpclass(void)
{
    TRACEINFO;
    if (pODWLDMANclass == NULL)
    {
        stODWLDMANclass.allocate          = ODWLDMAN_Allocate;
        stODWLDMANclass.deallocate        = ODWLDMAN_Deallocate;
        stODWLDMANclass.invalidate        = ODWLDMAN_Invalidate;
        stODWLDMANclass.hasMethod         = ODWLDMAN_HasMethod;
        stODWLDMANclass.invoke            = ODWLDMAN_Invoke;
        stODWLDMANclass.invokeDefault     = ODWLDMAN_InvokeDefault;
        stODWLDMANclass.hasProperty       = ODWLDMAN_HasProperty;
        stODWLDMANclass.getProperty       = ODWLDMAN_GetProperty;
        stODWLDMANclass.setProperty       = ODWLDMAN_SetProperty;
        stODWLDMANclass.removeProperty    = ODWLDMAN_RemoveProperty;
        stODWLDMANclass.enumerate         = ODWLDMAN_Enumerate;
        pODWLDMANclass = &stODWLDMANclass;
    }

    return pODWLDMANclass;
}


NPObject *          ODWLDMAN_Allocate(NPP npp, NPClass *theClass)
{
    TRACEINFO;

    NPObject* newdwldman = NULL;
    if (!v_bODWLDMANIdentifiersInitialized)
    {
        v_bODWLDMANIdentifiersInitialized = true;
        ODWLDMANinitializeIdentifiers();
    }

    newdwldman = (NPObject*)MEMALLOC(sizeof(NPObject));

    return newdwldman;
}

  void        ODWLDMAN_Deallocate(NPObject* obj)
{
    TRACEINFO;
    MEMFREE(obj);
    return;
}

  void        ODWLDMAN_Invalidate(NPObject* obj)
{
    TRACEINFO;
    return;
}

  bool        ODWLDMAN_HasMethod(NPObject* obj, NPIdentifier name)
{
    TRACEINFO;

    bool result = false;
    int i = 0;
    NPUTF8* utf8methodname = (char*)sBrowserFuncs->utf8fromidentifier(name);
    while ((i < kODWLDMAN_NUM_METHOD_IDENTIFIERS) && (result == false))
    {
        if ( name == v_ODWLDMANMethodIdentifiers[i] )
        {
            result= true;
        }
        i++;
    }	

    return result;
}

bool        ODWLDMAN_Invoke(NPObject* obj, NPIdentifier name, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    TRACEINFO;
    bool fctresult = false;
    if (name == v_ODWLDMANMethodIdentifiers[kODWLDMAN_ID_METHOD_GETDOWNLOADS])
    {
		ODWLDMAN_Invoke_GetDownloads(obj, args, argCount);
		fctresult = true;
    }
    else if (name == v_ODWLDMANMethodIdentifiers[kODWLDMAN_ID_METHOD_PAUSE])
    {
		ODWLDMAN_Invoke_Pause(obj, args, argCount);
		fctresult = true;
    }
    else if (name == v_ODWLDMANMethodIdentifiers[kODWLDMAN_ID_METHOD_REMOVE])
    {
		ODWLDMAN_Invoke_Remove(obj, args, argCount);
		fctresult = true;
    }
	else if (name == v_ODWLDMANMethodIdentifiers[kODWLDMAN_ID_METHOD_RESUME])
    {
		ODWLDMAN_Invoke_Resume(obj, args, argCount);
		fctresult = true;
    }
    else
    {        
        fctresult = false;
    }
    return fctresult;
}

 bool        ODWLDMAN_InvokeDefault(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    TRACEINFO;
    return true;
}

  bool        ODWLDMAN_HasProperty(NPObject* obj, NPIdentifier name)
{
    TRACEINFO;
    bool result = false;
    NPUTF8* utf8propertyname = (char*)sBrowserFuncs->utf8fromidentifier(name);

    int i = 0;
    while ((i < kODWLDMAN_NUM_PROPERTY_IDENTIFIERS) && (result == false))
    {
        if ( name == v_ODWLDMANPropertyIdentifiers[i] )
        {
            result= true;
        }
        i++;
    }
    
    return result;
}

  bool        ODWLDMAN_GetProperty(NPObject* obj, NPIdentifier name, NPVariant* result)
{
    TRACEINFO;
    return true;
}

  bool        ODWLDMAN_SetProperty(NPObject *obj, NPIdentifier name, const NPVariant *value)
{
    TRACEINFO;
    return true;
}

 bool        ODWLDMAN_RemoveProperty(NPObject *npobj, NPIdentifier name)
{
    TRACEINFO;
    return true;
}


 bool        ODWLDMAN_Enumerate(NPObject *npobj, NPIdentifier **value, uint32_t *count)
{
    TRACEINFO;
    return true;
}

/** implementation */


void ODWLDMAN_Invoke_GetDownloads(NPObject* obj,const NPVariant* args, uint32_t argCount)
{
	TRACEINFO;
	NOTIMPLEMENTED;
}

void ODWLDMAN_Invoke_Pause(NPObject* obj,const NPVariant* args, uint32_t argCount)
{
	TRACEINFO;
	NOTIMPLEMENTED;
}

void ODWLDMAN_Invoke_Remove(NPObject* obj,const NPVariant* args, uint32_t argCount)
{
	TRACEINFO;
	NOTIMPLEMENTED;
}

void ODWLDMAN_Invoke_Resume(NPObject* obj,const NPVariant* args, uint32_t argCount)
{
	TRACEINFO;
	NOTIMPLEMENTED;
}
