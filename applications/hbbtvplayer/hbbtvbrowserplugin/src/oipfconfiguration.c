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
#include "oipfconfiguration.h"

#define kOCFG_ID_PROPERTY_CONFIGURATION				       	0
#define kOCFG_NUM_PROPERTY_IDENTIFIERS                   	1

#define kOCFG_NUM_METHOD_IDENTIFIERS                	    0

bool            v_bOCFGIdentifiersInitialized = false;

NPIdentifier    v_OCFGPropertyIdentifiers[kOCFG_NUM_PROPERTY_IDENTIFIERS];
const NPUTF8 *  v_OCFGPropertyNames[kOCFG_NUM_PROPERTY_IDENTIFIERS] = {
	"configuration"
    };

NPIdentifier    v_OCFGMethodIdentifiers[kOCFG_NUM_METHOD_IDENTIFIERS];
const NPUTF8 *  v_OCFGMethodNames[kOCFG_NUM_METHOD_IDENTIFIERS] = {};

static  void    OCFGinitializeIdentifiers(void)
{
    sBrowserFuncs->getstringidentifiers( v_OCFGPropertyNames, kOCFG_NUM_PROPERTY_IDENTIFIERS, v_OCFGPropertyIdentifiers );
    sBrowserFuncs->getstringidentifiers( v_OCFGMethodNames,   kOCFG_NUM_METHOD_IDENTIFIERS,   v_OCFGMethodIdentifiers );
}


NPClass  stOCFGclass;
NPClass* pOCFGclass = NULL;

NPClass* fillOCFGpclass(void)
{
    TRACEINFO;
    if (pOCFGclass == NULL)
    {
        stOCFGclass.allocate          = OCFG_Allocate;
        stOCFGclass.deallocate        = OCFG_Deallocate;
        stOCFGclass.invalidate        = OCFG_Invalidate;
        stOCFGclass.hasMethod         = OCFG_HasMethod;
        stOCFGclass.invoke            = OCFG_Invoke;
        stOCFGclass.invokeDefault     = OCFG_InvokeDefault;
        stOCFGclass.hasProperty       = OCFG_HasProperty;
        stOCFGclass.getProperty       = OCFG_GetProperty;
        stOCFGclass.setProperty       = OCFG_SetProperty;
        stOCFGclass.removeProperty    = OCFG_RemoveProperty;
        stOCFGclass.enumerate         = OCFG_Enumerate;
        pOCFGclass = &stOCFGclass;
    }

    return pOCFGclass;
}


NPObject *          OCFG_Allocate(NPP npp, NPClass *theClass)
{
    TRACEINFO;

    NPObject* result;

    
    if (!v_bOCFGIdentifiersInitialized)
    {
        v_bOCFGIdentifiersInitialized = true;
        OCFGinitializeIdentifiers();
    }

    NPObject* newocfg = (NPObject*)MEMALLOC(sizeof(NPObject));
    result = newocfg;
    
    return result;
}


void        OCFG_Deallocate(NPObject* obj)
{
    TRACEINFO;
    MEMFREE(obj);
    return;
}

void        OCFG_Invalidate(NPObject* obj)
{
    TRACEINFO;
    return;
}

bool        OCFG_HasMethod(NPObject* obj, NPIdentifier name)
{
    TRACEINFO;
	bool result = false;
    int i = 0;
    NPUTF8* utf8methodname = (char*)sBrowserFuncs->utf8fromidentifier(name);
    while ((i < kOCFG_NUM_METHOD_IDENTIFIERS) && (result == false))
    {
        if ( name == v_OCFGMethodIdentifiers[i] )
        {
            result= true;
        }
        i++;
    }
    
    return result;
}

bool        OCFG_Invoke(NPObject* obj, NPIdentifier name, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    TRACEINFO;

    bool fctresult;
   
    fctresult = false;

    return fctresult;

}

bool        OCFG_InvokeDefault(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    TRACEINFO;
    return true;
}

bool        OCFG_HasProperty(NPObject* obj, NPIdentifier name)
{
    TRACEINFO;
    bool result = false;
    NPUTF8* utf8propertyname = (char*)sBrowserFuncs->utf8fromidentifier(name);

    int i = 0;
    while ((i < kOCFG_NUM_PROPERTY_IDENTIFIERS) && (result == false))
    {
        if ( name == v_OCFGPropertyIdentifiers[i] )
        {
            result= true;
        }
        i++;
    }
   
    return result;
}

bool        OCFG_GetProperty(NPObject* obj, NPIdentifier name, NPVariant* result)
{
    TRACEINFO;
    return true;
}

bool        OCFG_SetProperty(NPObject *obj, NPIdentifier name, const NPVariant *value)
{
    TRACEINFO;
    return true;
}

bool        OCFG_RemoveProperty(NPObject *npobj, NPIdentifier name)
{
    TRACEINFO;
    return true;
}


bool        OCFG_Enumerate(NPObject *npobj, NPIdentifier **value, uint32_t *count)
{
    TRACEINFO;
    return true;
}

