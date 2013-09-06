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
#include "downloadcollectionclass.h"


#define kDWLDCOLLECTION_ID_PROPERTY_LENGH					       	0
#define kDWLDCOLLECTION_NUM_PROPERTY_IDENTIFIERS                   1

#define kDWLDCOLLECTION_ID_METHOD_ITEM			               	    0
#define kDWLDCOLLECTION_NUM_METHOD_IDENTIFIERS                	    1

bool            v_bDWLDCOLLECTIONIdentifiersInitialized = false;

NPIdentifier    v_DWLDCOLLECTIONPropertyIdentifiers[kDWLDCOLLECTION_NUM_PROPERTY_IDENTIFIERS];
const NPUTF8 *  v_DWLDCOLLECTIONPropertyNames[kDWLDCOLLECTION_NUM_PROPERTY_IDENTIFIERS] = {
	"lengh"
    };

NPIdentifier    v_DWLDCOLLECTIONMethodIdentifiers[kDWLDCOLLECTION_NUM_METHOD_IDENTIFIERS];
const NPUTF8 *  v_DWLDCOLLECTIONMethodNames[kDWLDCOLLECTION_NUM_METHOD_IDENTIFIERS] = {
	"item"
	};

static  void    DWLDCOLLECTIONinitializeIdentifiers(void)
{
    sBrowserFuncs->getstringidentifiers( v_DWLDCOLLECTIONPropertyNames, kDWLDCOLLECTION_NUM_PROPERTY_IDENTIFIERS, v_DWLDCOLLECTIONPropertyIdentifiers );
    sBrowserFuncs->getstringidentifiers( v_DWLDCOLLECTIONMethodNames,   kDWLDCOLLECTION_NUM_METHOD_IDENTIFIERS,   v_DWLDCOLLECTIONMethodIdentifiers );
}


NPClass  stDWLDCOLLECTIONclass;
NPClass* pDWLDCOLLECTIONclass = NULL;

NPClass* fillDWLDCOLLECTIONpclass(void)
{
    TRACEINFO;
    if (pDWLDCOLLECTIONclass == NULL)
    {
        stDWLDCOLLECTIONclass.allocate          = DWLDCOLLECTION_Allocate;
        stDWLDCOLLECTIONclass.deallocate        = DWLDCOLLECTION_Deallocate;
        stDWLDCOLLECTIONclass.invalidate        = DWLDCOLLECTION_Invalidate;
        stDWLDCOLLECTIONclass.hasMethod         = DWLDCOLLECTION_HasMethod;
        stDWLDCOLLECTIONclass.invoke            = DWLDCOLLECTION_Invoke;
        stDWLDCOLLECTIONclass.invokeDefault     = DWLDCOLLECTION_InvokeDefault;
        stDWLDCOLLECTIONclass.hasProperty       = DWLDCOLLECTION_HasProperty;
        stDWLDCOLLECTIONclass.getProperty       = DWLDCOLLECTION_GetProperty;
        stDWLDCOLLECTIONclass.setProperty       = DWLDCOLLECTION_SetProperty;
        stDWLDCOLLECTIONclass.removeProperty    = DWLDCOLLECTION_RemoveProperty;
        stDWLDCOLLECTIONclass.enumerate         = DWLDCOLLECTION_Enumerate;
        pDWLDCOLLECTIONclass = &stDWLDCOLLECTIONclass;
    }

    return pDWLDCOLLECTIONclass;
}


NPObject *          DWLDCOLLECTION_Allocate(NPP npp, NPClass *theClass)
{
    TRACEINFO;

    if (!v_bDWLDCOLLECTIONIdentifiersInitialized)
    {
        v_bDWLDCOLLECTIONIdentifiersInitialized = true;
        DWLDCOLLECTIONinitializeIdentifiers();
    }

    NPObject* newdwldcollection = (NPObject *)MEMALLOC(sizeof(NPObject));

    return newdwldcollection;
}


void        DWLDCOLLECTION_Deallocate(NPObject* obj)
{
    TRACEINFO;
    MEMFREE(obj);
    return;
}

void        DWLDCOLLECTION_Invalidate(NPObject* obj)
{
    TRACEINFO;
    return;
}

bool        DWLDCOLLECTION_HasMethod(NPObject* obj, NPIdentifier name)
{
    TRACEINFO;
	bool result = false;
    int i = 0;
    NPUTF8* utf8methodname = (char*)sBrowserFuncs->utf8fromidentifier(name);
    while ((i < kDWLDCOLLECTION_NUM_METHOD_IDENTIFIERS) && (result == false))
    {
        if ( name == v_DWLDCOLLECTIONMethodIdentifiers[i] )
        {
            result= true;
        }
        i++;
    }
    
    return result;
}

bool        DWLDCOLLECTION_Invoke(NPObject* obj, NPIdentifier name, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    TRACEINFO;
    bool fctresult = false;
    if (name == v_DWLDCOLLECTIONMethodIdentifiers[kDWLDCOLLECTION_ID_METHOD_ITEM ])
    {
		DWLDCOLLECTION_Invoke_Item(obj, args, argCount);
		fctresult = true;
    }
    else
    {    	
    	fctresult = false;
    }
    return fctresult;

}

bool        DWLDCOLLECTION_InvokeDefault(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    TRACEINFO;
    return true;
}

bool        DWLDCOLLECTION_HasProperty(NPObject* obj, NPIdentifier name)
{
    TRACEINFO;
    bool result = false;
    NPUTF8* utf8propertyname = (char*)sBrowserFuncs->utf8fromidentifier(name);

    int i = 0;
    while ((i < kDWLDCOLLECTION_NUM_PROPERTY_IDENTIFIERS) && (result == false))
    {
        if ( name == v_DWLDCOLLECTIONPropertyIdentifiers[i] )
        {
            result= true;
        }
        i++;
    }

    
    return result;
}

bool        DWLDCOLLECTION_GetProperty(NPObject* obj, NPIdentifier name, NPVariant* result)
{
    TRACEINFO;
    return true;
}

bool        DWLDCOLLECTION_SetProperty(NPObject *obj, NPIdentifier name, const NPVariant *value)
{
    TRACEINFO;
    return true;
}

bool        DWLDCOLLECTION_RemoveProperty(NPObject *npobj, NPIdentifier name)
{
    TRACEINFO;
    return true;
}


bool        DWLDCOLLECTION_Enumerate(NPObject *npobj, NPIdentifier **value, uint32_t *count)
{
    TRACEINFO;
    return true;
}

/** implementation **/
void DWLDCOLLECTION_Invoke_Item(NPObject* obj,const NPVariant* args, uint32_t argCount)
{
	TRACEINFO;
	NOTIMPLEMENTED;
}
