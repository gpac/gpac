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
#include "oipfapplicationmanager.h"
#include "applicationclass.h"

#define kOAM_ID_PROPERTY_ON_LOW_MEMORY                  0
#define kOAM_NUM_PROPERTY_IDENTIFIERS                   1

#define kOAM_ID_METHOD_GET_OWNER_APPLICATION            0
#define kOAM_NUM_METHOD_IDENTIFIERS                     1

bool            v_bOAMIdentifiersInitialized = false;

NPIdentifier    v_OAMPropertyIdentifiers[kOAM_NUM_PROPERTY_IDENTIFIERS];
const NPUTF8 *  v_OAMPropertyNames[kOAM_NUM_PROPERTY_IDENTIFIERS] = {
    "onLowMemory"
	};

NPIdentifier    v_OAMMethodIdentifiers[kOAM_NUM_METHOD_IDENTIFIERS];
const NPUTF8 *  v_OAMMethodNames[kOAM_NUM_METHOD_IDENTIFIERS] = {
    "getOwnerApplication"
};




static  void    OAMinitializeIdentifiers(void)
{
    sBrowserFuncs->getstringidentifiers( v_OAMPropertyNames, kOAM_NUM_PROPERTY_IDENTIFIERS, v_OAMPropertyIdentifiers );
    sBrowserFuncs->getstringidentifiers( v_OAMMethodNames,   kOAM_NUM_METHOD_IDENTIFIERS,   v_OAMMethodIdentifiers );
}

NPClass  stOAMclass;
NPClass* pOAMclass = NULL;

NPClass* fillOAMpclass(void)
{
    TRACEINFO;
    if (pOAMclass == NULL)
    {
        stOAMclass.allocate          = OAM_Allocate;
        stOAMclass.deallocate        = OAM_Deallocate;
        stOAMclass.invalidate        = OAM_Invalidate;
        stOAMclass.hasMethod         = OAM_HasMethod;
        stOAMclass.invoke            = OAM_Invoke;
        stOAMclass.invokeDefault     = OAM_InvokeDefault;
        stOAMclass.hasProperty       = OAM_HasProperty;
        stOAMclass.getProperty       = OAM_GetProperty;
        stOAMclass.setProperty       = OAM_SetProperty;
        stOAMclass.removeProperty    = OAM_RemoveProperty;
        stOAMclass.enumerate         = OAM_Enumerate;
        pOAMclass = &stOAMclass;
    }

    return pOAMclass;
}


NPObject *          OAM_Allocate(NPP npp, NPClass *theClass)
{
    TRACEINFO;

	
    if (!v_bOAMIdentifiersInitialized)
    {
        v_bOAMIdentifiersInitialized = true;
        OAMinitializeIdentifiers();
    }
    
    NPObj_OAM* newapplicationmanager = (NPObj_OAM*)MEMALLOC(sizeof(NPObj_OAM));
	fprintf(stderr, "\t%s : Allocation at \x1b[%i;3%im%p\n\x1b[0m ",__FUNCTION__, 1, 1, newapplicationmanager );
    newapplicationmanager->npp = npp;
    newapplicationmanager->ownerApplication = sBrowserFuncs->createobject(npp, fillAPPLICATIONpclass());
    fprintf(stderr, "\t%s : Create ownerApplication member at \x1b[%i;3%im%p\n\x1b[0m ",__FUNCTION__, 1, 1, newapplicationmanager->ownerApplication );
    return (NPObject *)newapplicationmanager;
}

  void        OAM_Deallocate(NPObject* obj)
{
    TRACEINFO;
    NPObj_OAM* oamobj = (NPObj_OAM*)obj;
    sBrowserFuncs->releaseobject(oamobj->ownerApplication);    
    MEMFREE(oamobj);
    return;
}

  void        OAM_Invalidate(NPObject* obj)
{
    TRACEINFO;
    return;
}

  bool        OAM_HasMethod(NPObject* obj, NPIdentifier name)
{
    TRACEINFO;

    bool result = false;
    int i = 0;
    NPUTF8* utf8methodname = (char*)sBrowserFuncs->utf8fromidentifier(name);
    while ((i < kOAM_NUM_METHOD_IDENTIFIERS) && (result == false))
    {
        if ( name == v_OAMMethodIdentifiers[i] )
        {
            result= true;
        }
        i++;
    }
	
    return result;
}

bool        OAM_Invoke(NPObject* obj, NPIdentifier name, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    TRACEINFO;
	//OAM_ObjectMain * pMainObj = reinterpret_cast<tWOTVe_OAM_ObjectMain*>(obj);
	bool fctresult = false;
    if (name == v_OAMMethodIdentifiers[kOAM_ID_METHOD_GET_OWNER_APPLICATION])
    {
		OAM_ObjectMain_Invoke_GetOwnerApplication((NPObj_OAM*)obj, args, argCount, result);
		fctresult = true;
    }
    else
    {
        
        fctresult = false;
    }
    return fctresult;
}

 bool        OAM_InvokeDefault(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    TRACEINFO;
    return true;
}

  bool        OAM_HasProperty(NPObject* obj, NPIdentifier name)
{
    TRACEINFO;
    bool result = false;
    NPUTF8* utf8propertyname = (char*)sBrowserFuncs->utf8fromidentifier(name);

    int i = 0;
    while ((i < kOAM_NUM_PROPERTY_IDENTIFIERS) && (result == false))
    {
        if ( name == v_OAMPropertyIdentifiers[i] )
        {
            result= true;
        }
        i++;
    }
   
    return result;
}

bool          OAM_GetProperty(NPObject* obj, NPIdentifier name, NPVariant* result)
{
    TRACEINFO;
    return true;
}

  bool        OAM_SetProperty(NPObject *obj, NPIdentifier name, const NPVariant *value)
{
    TRACEINFO;
    return true;
}

 bool        OAM_RemoveProperty(NPObject *npobj, NPIdentifier name)
{
    TRACEINFO;
    return true;
}


 bool        OAM_Enumerate(NPObject *npobj, NPIdentifier **value, uint32_t *count)
{
    TRACEINFO;
    return true;
}

/** implementation **/
void 		OAM_ObjectMain_Invoke_GetOwnerApplication(NPObj_OAM* obj,const NPVariant* args, uint32_t argCount, NPVariant* result)
{
	TRACEINFO;
	sBrowserFuncs->retainobject(obj->ownerApplication);
	OBJECT_TO_NPVARIANT((NPObject*)(obj->ownerApplication), *result);	
}
