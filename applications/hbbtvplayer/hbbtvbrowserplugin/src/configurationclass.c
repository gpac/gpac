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
#include "configurationclass.h"


#define kCONFIGURATION_ID_PROPERTY_PREFERREDAUDIOLANGUAGE		0
#define kCONFIGURATION_ID_PROPERTY_PREFERREDSUBTITLELANGUAGE 	1
#define kCONFIGURATION_ID_PROPERTY_COUNTRYID					2
#define kCONFIGURATION_NUM_PROPERTY_IDENTIFIERS				    3

#define kCONFIGURATION_NUM_METHOD_IDENTIFIERS                	0



bool            v_bCONFIGURATIONIdentifiersInitialized = false;

NPIdentifier    v_CONFIGURATIONPropertyIdentifiers[kCONFIGURATION_NUM_PROPERTY_IDENTIFIERS];
const NPUTF8 *  v_CONFIGURATIONPropertyNames[kCONFIGURATION_NUM_PROPERTY_IDENTIFIERS] = {
	"preferredAudioLanguage",
    "preferredSubtitleLanguage",
    "countryId"
    };

NPIdentifier    v_CONFIGURATIONMethodIdentifiers[kCONFIGURATION_NUM_METHOD_IDENTIFIERS];
const NPUTF8 *  v_CONFIGURATIONMethodNames[kCONFIGURATION_NUM_METHOD_IDENTIFIERS] = {};

static  void    CONFIGURATIONinitializeIdentifiers(void)
{
    sBrowserFuncs->getstringidentifiers( v_CONFIGURATIONPropertyNames, kCONFIGURATION_NUM_PROPERTY_IDENTIFIERS, v_CONFIGURATIONPropertyIdentifiers );
    sBrowserFuncs->getstringidentifiers( v_CONFIGURATIONMethodNames,   kCONFIGURATION_NUM_METHOD_IDENTIFIERS,   v_CONFIGURATIONMethodIdentifiers );
}


NPClass  stCONFIGURATIONclass;
NPClass* pCONFIGURATIONclass = NULL;

NPClass* fillCONFIGURATIONpclass(void)
{
    TRACEINFO;
    if (pCONFIGURATIONclass == NULL)
    {
        stCONFIGURATIONclass.allocate          = CONFIGURATION_Allocate;
        stCONFIGURATIONclass.deallocate        = CONFIGURATION_Deallocate;
        stCONFIGURATIONclass.invalidate        = CONFIGURATION_Invalidate;
        stCONFIGURATIONclass.hasMethod         = CONFIGURATION_HasMethod;
        stCONFIGURATIONclass.invoke            = CONFIGURATION_Invoke;
        stCONFIGURATIONclass.invokeDefault     = CONFIGURATION_InvokeDefault;
        stCONFIGURATIONclass.hasProperty       = CONFIGURATION_HasProperty;
        stCONFIGURATIONclass.getProperty       = CONFIGURATION_GetProperty;
        stCONFIGURATIONclass.setProperty       = CONFIGURATION_SetProperty;
        stCONFIGURATIONclass.removeProperty    = CONFIGURATION_RemoveProperty;
        stCONFIGURATIONclass.enumerate         = CONFIGURATION_Enumerate;
        pCONFIGURATIONclass = &stCONFIGURATIONclass;
    }

    return pCONFIGURATIONclass;
}


NPObject *          CONFIGURATION_Allocate(NPP npp, NPClass *theClass)
{
    TRACEINFO;
    
    if (!v_bCONFIGURATIONIdentifiersInitialized)
    {
        v_bCONFIGURATIONIdentifiersInitialized = true;
        CONFIGURATIONinitializeIdentifiers();
    }
	NPObject* newconfiguration = NULL;
	
    newconfiguration = (NPObject *)MEMALLOC(sizeof(NPObject));

    return newconfiguration;
}


void        CONFIGURATION_Deallocate(NPObject* obj)
{
    TRACEINFO;
     MEMFREE(obj);
    return;
}

void        CONFIGURATION_Invalidate(NPObject* obj)
{
    TRACEINFO;
    return;
}

bool        CONFIGURATION_HasMethod(NPObject* obj, NPIdentifier name)
{
    TRACEINFO;
	bool result = false;
    int i = 0;
    NPUTF8* utf8methodname = (char*)sBrowserFuncs->utf8fromidentifier(name);
    while ((i < kCONFIGURATION_NUM_METHOD_IDENTIFIERS) && (result == false))
    {
        if ( name == v_CONFIGURATIONMethodIdentifiers[i] )
        {
            result= true;
        }
        i++;
    }
    
    return result;
}

bool        CONFIGURATION_Invoke(NPObject* obj, NPIdentifier name, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    TRACEINFO;
    return true;
}

bool        CONFIGURATION_InvokeDefault(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    TRACEINFO;
    return true;
}

bool        CONFIGURATION_HasProperty(NPObject* obj, NPIdentifier name)
{
    TRACEINFO;
    bool result = false;
    NPUTF8* utf8propertyname = (char*)sBrowserFuncs->utf8fromidentifier(name);

    int i = 0;
    while ((i < kCONFIGURATION_NUM_PROPERTY_IDENTIFIERS) && (result == false))
    {
        if ( name == v_CONFIGURATIONPropertyIdentifiers[i] )
        {
            result= true;
        }
        i++;
    }

    
    return result;
}

bool        CONFIGURATION_GetProperty(NPObject* obj, NPIdentifier name, NPVariant* result)
{
    TRACEINFO;
    return true;
}

bool        CONFIGURATION_SetProperty(NPObject *obj, NPIdentifier name, const NPVariant *value)
{
    TRACEINFO;
    return true;
}

bool        CONFIGURATION_RemoveProperty(NPObject *npobj, NPIdentifier name)
{
    TRACEINFO;
    return true;
}


bool        CONFIGURATION_Enumerate(NPObject *npobj, NPIdentifier **value, uint32_t *count)
{
    TRACEINFO;
    return true;
}

