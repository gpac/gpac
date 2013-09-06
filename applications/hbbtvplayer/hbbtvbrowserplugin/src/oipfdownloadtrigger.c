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
 *
 *		Authors: Stanislas Selle 		
 *				
 */
#include "oipfdownloadtrigger.h"


#define kODWLDTRG_NUM_PROPERTY_IDENTIFIERS                   0

#define kODWLDTRG_ID_METHOD_REGISTER_DOWNLOAD	             0
#define kODWLDTRG_ID_METHOD_REGISTER_DOWNLOADURL            1
#define kODWLDTRG_NUM_METHOD_IDENTIFIERS                     2

bool            v_bODWLDTRGIdentifiersInitialized = false;

NPIdentifier    v_ODWLDTRGPropertyIdentifiers[kODWLDTRG_NUM_PROPERTY_IDENTIFIERS];
const NPUTF8 *  v_ODWLDTRGPropertyNames[kODWLDTRG_NUM_PROPERTY_IDENTIFIERS] = {
};

NPIdentifier    v_ODWLDTRGMethodIdentifiers[kODWLDTRG_NUM_METHOD_IDENTIFIERS];
const NPUTF8 *  v_ODWLDTRGMethodNames[kODWLDTRG_NUM_METHOD_IDENTIFIERS] = {
    "registerDownload",
    "registerDownloadURL"
};



static  void    ODWLDTRGinitializeIdentifiers(void)
{
    sBrowserFuncs->getstringidentifiers( v_ODWLDTRGPropertyNames, kODWLDTRG_NUM_PROPERTY_IDENTIFIERS, v_ODWLDTRGPropertyIdentifiers );
    sBrowserFuncs->getstringidentifiers( v_ODWLDTRGMethodNames,   kODWLDTRG_NUM_METHOD_IDENTIFIERS,   v_ODWLDTRGMethodIdentifiers );
}

NPClass  stODWLDTRGclass;
NPClass* pODWLDTRGclass = NULL;

NPClass* fillODWLDTRGpclass(void)
{
    TRACEINFO;
    if (pODWLDTRGclass == NULL)
    {
        stODWLDTRGclass.allocate          = ODWLDTRG_Allocate;
        stODWLDTRGclass.deallocate        = ODWLDTRG_Deallocate;
        stODWLDTRGclass.invalidate        = ODWLDTRG_Invalidate;
        stODWLDTRGclass.hasMethod         = ODWLDTRG_HasMethod;
        stODWLDTRGclass.invoke            = ODWLDTRG_Invoke;
        stODWLDTRGclass.invokeDefault     = ODWLDTRG_InvokeDefault;
        stODWLDTRGclass.hasProperty       = ODWLDTRG_HasProperty;
        stODWLDTRGclass.getProperty       = ODWLDTRG_GetProperty;
        stODWLDTRGclass.setProperty       = ODWLDTRG_SetProperty;
        stODWLDTRGclass.removeProperty    = ODWLDTRG_RemoveProperty;
        stODWLDTRGclass.enumerate         = ODWLDTRG_Enumerate;
        pODWLDTRGclass = &stODWLDTRGclass;
    }

    return pODWLDTRGclass;
}


NPObject *          ODWLDTRG_Allocate(NPP npp, NPClass *theClass)
{
    TRACEINFO;

    NPObject* newdwldtrg = NULL;
    if (!v_bODWLDTRGIdentifiersInitialized)
    {
        v_bODWLDTRGIdentifiersInitialized = true;
        ODWLDTRGinitializeIdentifiers();
    }

    newdwldtrg = (NPObject*)MEMALLOC(sizeof(NPObject));

    return newdwldtrg;
}

  void        ODWLDTRG_Deallocate(NPObject* obj)
{
    TRACEINFO;
    MEMFREE(obj);
    return;
}

  void        ODWLDTRG_Invalidate(NPObject* obj)
{
    TRACEINFO;
    return;
}

  bool        ODWLDTRG_HasMethod(NPObject* obj, NPIdentifier name)
{
    TRACEINFO;

    bool result = false;
    int i = 0;
    NPUTF8* utf8methodname = (char*)sBrowserFuncs->utf8fromidentifier(name);
    while ((i < kODWLDTRG_NUM_METHOD_IDENTIFIERS) && (result == false))
    {
        if ( name == v_ODWLDTRGMethodIdentifiers[i] )
        {
            result= true;
        }
        i++;
    }
	
    return result;
}

bool        ODWLDTRG_Invoke(NPObject* obj, NPIdentifier name, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    TRACEINFO;
	bool fctresult = false;
    if (name == v_ODWLDTRGMethodIdentifiers[kODWLDTRG_ID_METHOD_REGISTER_DOWNLOAD])
    {
		ODWLDTRG_Invoke_registerDownload(obj, args, argCount);
		fctresult = true;
    }
    else if (name == v_ODWLDTRGMethodIdentifiers[kODWLDTRG_ID_METHOD_REGISTER_DOWNLOADURL])
    {
		ODWLDTRG_Invoke_registerDownloadURL(obj, args, argCount);
		fctresult = true;
    }
	else
    {        
        fctresult = false;
    }
    return fctresult;
}

 bool        ODWLDTRG_InvokeDefault(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    TRACEINFO;
    return true;
}

  bool        ODWLDTRG_HasProperty(NPObject* obj, NPIdentifier name)
{
    TRACEINFO;
    bool result = false;
    NPUTF8* utf8propertyname = (char*)sBrowserFuncs->utf8fromidentifier(name);

    int i = 0;
    while ((i < kODWLDTRG_NUM_PROPERTY_IDENTIFIERS) && (result == false))
    {
        if ( name == v_ODWLDTRGPropertyIdentifiers[i] )
        {
            result= true;
        }
        i++;
    }
  
    return result;
}

  bool        ODWLDTRG_GetProperty(NPObject* obj, NPIdentifier name, NPVariant* result)
{
    TRACEINFO;
    return true;
}

  bool        ODWLDTRG_SetProperty(NPObject *obj, NPIdentifier name, const NPVariant *value)
{
    TRACEINFO;
    return true;
}

 bool        ODWLDTRG_RemoveProperty(NPObject *npobj, NPIdentifier name)
{
    TRACEINFO;
    return true;
}


 bool        ODWLDTRG_Enumerate(NPObject *npobj, NPIdentifier **value, uint32_t *count)
{
    TRACEINFO;
    return true;
}

void ODWLDTRG_Invoke_registerDownload(NPObject* obj,const NPVariant* args, uint32_t argCount)
{
	TRACEINFO;
	NOTIMPLEMENTED;
}

void ODWLDTRG_Invoke_registerDownloadURL(NPObject* obj,const NPVariant* args, uint32_t argCount)
{
	TRACEINFO;
	NOTIMPLEMENTED;
}
