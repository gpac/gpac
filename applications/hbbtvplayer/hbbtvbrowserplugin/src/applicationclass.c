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
#include "applicationprivatedataclass.h"
#include "applicationclass.h"

#define kAPPLICATION_ID_PROPERTY_VISIBLE    		            0
#define kAPPLICATION_ID_PROPERTY_PRIVATEDATA	                1
#define kAPPLICATION_NUM_PROPERTY_IDENTIFIERS                   2

#define kAPPLICATION_ID_METHOD_CREATEAPPLICATION               0
#define kAPPLICATION_ID_METHOD_DESTROYAPPLICATION              1
#define kAPPLICATION_ID_METHOD_SHOW                             2
#define kAPPLICATION_ID_METHOD_HIDE                             3
#define kAPPLICATION_NUM_METHOD_IDENTIFIERS              		4


static  bool            v_bAPPLICATIONIdentifiersInitialized = false;

static  NPIdentifier    v_APPLICATIONPropertyIdentifiers[kAPPLICATION_NUM_PROPERTY_IDENTIFIERS];
static  const NPUTF8 *  v_APPLICATIONPropertyNames[kAPPLICATION_NUM_PROPERTY_IDENTIFIERS] = {
	"visible",
	"privateData"
};

static  NPIdentifier    v_APPLICATIONMethodIdentifiers[kAPPLICATION_NUM_METHOD_IDENTIFIERS];
static  const NPUTF8 *  v_APPLICATIONMethodNames[kAPPLICATION_NUM_METHOD_IDENTIFIERS] = {
    "createApplication",
    "destroyApplication",
    "show",
    "hide"
};


static  void    APPLICATIONinitializeIdentifiers(void)
{
    sBrowserFuncs->getstringidentifiers( v_APPLICATIONPropertyNames, kAPPLICATION_NUM_PROPERTY_IDENTIFIERS, v_APPLICATIONPropertyIdentifiers );
    sBrowserFuncs->getstringidentifiers( v_APPLICATIONMethodNames,   kAPPLICATION_NUM_METHOD_IDENTIFIERS,   v_APPLICATIONMethodIdentifiers );
}

NPClass  stAPPLICATIONclass;
NPClass* pAPPLICATIONclass = NULL;

NPClass* fillAPPLICATIONpclass(void)
{
    TRACEINFO;
    if (pAPPLICATIONclass == NULL)
    {
        stAPPLICATIONclass.allocate          = APPLICATION_Allocate;
        stAPPLICATIONclass.deallocate        = APPLICATION_Deallocate;
        stAPPLICATIONclass.invalidate        = APPLICATION_Invalidate;
        stAPPLICATIONclass.hasMethod         = APPLICATION_HasMethod;
        stAPPLICATIONclass.invoke            = APPLICATION_Invoke;
        stAPPLICATIONclass.invokeDefault     = APPLICATION_InvokeDefault;
        stAPPLICATIONclass.hasProperty       = APPLICATION_HasProperty;
        stAPPLICATIONclass.getProperty       = APPLICATION_GetProperty;
        stAPPLICATIONclass.setProperty       = APPLICATION_SetProperty;
        stAPPLICATIONclass.removeProperty    = APPLICATION_RemoveProperty;
        stAPPLICATIONclass.enumerate         = APPLICATION_Enumerate;
        pAPPLICATIONclass = &stAPPLICATIONclass;
    }

    return pAPPLICATIONclass;
}


NPObject *          APPLICATION_Allocate(NPP npp, NPClass *theClass)
{
    TRACEINFO;
    if (!v_bAPPLICATIONIdentifiersInitialized)
    {
        v_bAPPLICATIONIdentifiersInitialized = true;
        APPLICATIONinitializeIdentifiers();
    }
    
    NPObj_Application* newapplication = (NPObj_Application*)MEMALLOC(sizeof(NPObj_Application));
    fprintf(stderr, "\t%s : Allocation at \x1b[%i;3%im%p\n\x1b[0m",__FUNCTION__, 1, 1, newapplication );
	newapplication->npp = npp;	
	newapplication->visible = true;
    newapplication->privateData = sBrowserFuncs->createobject(npp, fillAPPPRIVDATApclass());
    fprintf(stderr, "\t%s : Create privateData property at \x1b[%i;3%im%p\n\x1b[0m ",__FUNCTION__,  1, 1, newapplication->privateData );
	newapplication->visible = true;
    return (NPObject*)newapplication;
}

  void        APPLICATION_Deallocate(NPObject* obj)
{
    TRACEINFO;
    NPObj_Application* applicationobj = (NPObj_Application*)obj;
    sBrowserFuncs->releaseobject(applicationobj->privateData);
    MEMFREE(applicationobj);
    return;
}

  void        APPLICATION_Invalidate(NPObject* obj)
{
    TRACEINFO;
    return;
}

  bool        APPLICATION_HasMethod(NPObject* obj, NPIdentifier name)
{
    TRACEINFO;

    bool result = false;
    int i = 0;
    NPUTF8* utf8methodname = (char*)sBrowserFuncs->utf8fromidentifier(name);
    while ((i < kAPPLICATION_NUM_METHOD_IDENTIFIERS) && (result == false))
    {
        if ( name == v_APPLICATIONMethodIdentifiers[i] )
        {
            result= true;
        }
        i++;
    }
	
    return result;
}

bool        APPLICATION_Invoke(NPObject* obj, NPIdentifier name, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    TRACEINFO;
	bool fctresult = false;
    if (name == v_APPLICATIONMethodIdentifiers[kAPPLICATION_ID_METHOD_CREATEAPPLICATION])
    {
		APPLICATION_Invoke_CreateApplication(obj, args, argCount);
		fctresult = true;
    }
    else if (name == v_APPLICATIONMethodIdentifiers[kAPPLICATION_ID_METHOD_DESTROYAPPLICATION])
    {
		APPLICATION_Invoke_DestroyApplication(obj, args, argCount);
		fctresult = true;
    }
    else if (name == v_APPLICATIONMethodIdentifiers[kAPPLICATION_ID_METHOD_HIDE])
    {
		APPLICATION_Invoke_Hide(obj, args, argCount);
		fctresult = true;
    }
	else if (name == v_APPLICATIONMethodIdentifiers[kAPPLICATION_ID_METHOD_SHOW])
    {
		APPLICATION_Invoke_Show(obj, args, argCount);
		fctresult = true;
    }
    else
    {       
        fctresult = false;
    }
    return fctresult;
}

 bool        APPLICATION_InvokeDefault(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    TRACEINFO;
    return true;
}

  bool        APPLICATION_HasProperty(NPObject* obj, NPIdentifier name)
{
    TRACEINFO;
    bool result = false;
    NPUTF8* utf8propertyname = (char*)sBrowserFuncs->utf8fromidentifier(name);

    int i = 0;
    while ((i < kAPPLICATION_NUM_PROPERTY_IDENTIFIERS) && (result == false))
    {
        if ( name == v_APPLICATIONPropertyIdentifiers[i] )
        {
            result= true;
        }
        i++;
    }
  
    return result;
}

bool        APPLICATION_GetProperty(NPObject* obj, NPIdentifier name, NPVariant* result)
{
    TRACEINFO;
   
    bool fctresult = false;
    NPObj_Application* AppliObj = (NPObj_Application*)obj;
    
    if (name == v_APPLICATIONPropertyIdentifiers[kAPPLICATION_ID_PROPERTY_PRIVATEDATA])
    {    			
		sBrowserFuncs->retainobject(AppliObj->privateData);
		OBJECT_TO_NPVARIANT(AppliObj->privateData, *result);	
    	fctresult = true;
    }
    else  if (name == v_APPLICATIONPropertyIdentifiers[kAPPLICATION_ID_PROPERTY_VISIBLE])
    {    			
		BOOLEAN_TO_NPVARIANT(AppliObj->visible, *result);
		fctresult = true;
    }
    else
		
    return fctresult;
}

  bool        APPLICATION_SetProperty(NPObject *obj, NPIdentifier name, const NPVariant *value)
{
    TRACEINFO;
    return true;
}

 bool        APPLICATION_RemoveProperty(NPObject *npobj, NPIdentifier name)
{
    TRACEINFO;
    return true;
}


 bool        APPLICATION_Enumerate(NPObject *npobj, NPIdentifier **value, uint32_t *count)
{
    TRACEINFO;
    return true;
}

/** implementation **/

void APPLICATION_Invoke_CreateApplication(NPObject* obj,const NPVariant* args, uint32_t argCount)
{
	TRACEINFO;
	NOTIMPLEMENTED;
}

void APPLICATION_Invoke_DestroyApplication(NPObject* obj,const NPVariant* args, uint32_t argCount)
{
	TRACEINFO;
	NOTIMPLEMENTED;
}

void APPLICATION_Invoke_Show(NPObject* obj,const NPVariant* args, uint32_t argCount)
{
	TRACEINFO;	
	NPObj_Application* AppliObj = (NPObj_Application*)obj;
	OnAPPLICATION_Show();
	AppliObj->visible = true;
	NOTIMPLEMENTED;
}

void APPLICATION_Invoke_Hide(NPObject* obj,const NPVariant* args, uint32_t argCount)
{
	TRACEINFO;
	
	NPObj_Application* AppliObj = (NPObj_Application*)obj;
	OnAPPLICATION_Hide();
	AppliObj->visible = false;
	NOTIMPLEMENTED; 
}
