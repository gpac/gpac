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
#include "downloadclass.h"

#define kDOWNLOAD_ID_PROPERTY_TOTALIZE			    0
#define kDOWNLOAD_ID_PROPERTY_STATE					1
#define kDOWNLOAD_ID_PROPERTY_AMOUNTDOWNLOADED		2
#define kDOWNLOAD_ID_PROPERTY_NAME					3
#define kDOWNLOAD_ID_PROPERTY_ID					4
#define kDOWNLOAD_ID_PROPERTY_CONTENTURL			5
#define kDOWNLOAD_ID_PROPERTY_DESCRIPTION			6
#define kDOWNLOAD_ID_PROPERTY_PARENTALRATINGS		7
#define kDOWNLOAD_ID_PROPERTY_DRMCONTROL			8
#define kDOWNLOAD_ID_PROPERTY_STARTTIME				9
#define kDOWNLOAD_ID_PROPERTY_TIMEELAPSED			10
#define kDOWNLOAD_ID_PROPERTY_TIMEREMAINING			11
#define kDOWNLOAD_ID_PROPERTY_TRANSFERTYPE			12
#define kDOWNLOAD_ID_PROPERTY_ORIGINSITE			13
#define kDOWNLOAD_ID_PROPERTY_ORIGINSITENAME		14
#define kDOWNLOAD_ID_PROPERTY_CONTENTID				15
#define kDOWNLOAD_ID_PROPERTY_ICONURL				16
#define kDOWNLOAD_NUM_PROPERTY_IDENTIFIERS			17


#define kDOWNLOAD_NUM_METHOD_IDENTIFIERS    		0


bool            v_bDOWNLOADIdentifiersInitialized = false;

NPIdentifier    v_DOWNLOADPropertyIdentifiers[kDOWNLOAD_NUM_PROPERTY_IDENTIFIERS];
const NPUTF8 *  v_DOWNLOADPropertyNames[kDOWNLOAD_NUM_PROPERTY_IDENTIFIERS] = {
	"totalize",
	"state",
	"amountDownloaded",
	"name",
	"id",
	"contentURL",
	"description",
	"parentalRatings",
	"drmControl",
	"startTime",
	"timeElapsed",
	"timeRemaining",
	"transferType",
	"originSite",
	"originSiteName",
	"contentID",
	"iconURL"
	};

NPIdentifier    v_DOWNLOADMethodIdentifiers[kDOWNLOAD_NUM_METHOD_IDENTIFIERS];
const NPUTF8 *  v_DOWNLOADMethodNames[kDOWNLOAD_NUM_METHOD_IDENTIFIERS] = {
};


static  void    DOWNLOADinitializeIdentifiers(void)
{
    sBrowserFuncs->getstringidentifiers( v_DOWNLOADPropertyNames, kDOWNLOAD_NUM_PROPERTY_IDENTIFIERS, v_DOWNLOADPropertyIdentifiers );
    sBrowserFuncs->getstringidentifiers( v_DOWNLOADMethodNames,   kDOWNLOAD_NUM_METHOD_IDENTIFIERS,   v_DOWNLOADMethodIdentifiers );
}

NPClass  stDOWNLOADclass;
NPClass* pDOWNLOADclass = NULL;

NPClass* fillDOWNLOADpclass(void)
{
    TRACEINFO;
    if (pDOWNLOADclass == NULL)
    {
        stDOWNLOADclass.allocate          = DOWNLOAD_Allocate;
        stDOWNLOADclass.deallocate        = DOWNLOAD_Deallocate;
        stDOWNLOADclass.invalidate        = DOWNLOAD_Invalidate;
        stDOWNLOADclass.hasMethod         = DOWNLOAD_HasMethod;
        stDOWNLOADclass.invoke            = DOWNLOAD_Invoke;
        stDOWNLOADclass.invokeDefault     = DOWNLOAD_InvokeDefault;
        stDOWNLOADclass.hasProperty       = DOWNLOAD_HasProperty;
        stDOWNLOADclass.getProperty       = DOWNLOAD_GetProperty;
        stDOWNLOADclass.setProperty       = DOWNLOAD_SetProperty;
        stDOWNLOADclass.removeProperty    = DOWNLOAD_RemoveProperty;
        stDOWNLOADclass.enumerate         = DOWNLOAD_Enumerate;
        pDOWNLOADclass = &stDOWNLOADclass;
    }

    return pDOWNLOADclass;
}


NPObject *          DOWNLOAD_Allocate(NPP npp, NPClass *theClass)
{
    TRACEINFO;

    
    if (!v_bDOWNLOADIdentifiersInitialized)
    {
        v_bDOWNLOADIdentifiersInitialized = true;
        DOWNLOADinitializeIdentifiers();
    }

    NPObject* newdownload = (NPObject *)MEMALLOC(sizeof(NPObject));

    return newdownload;
}

  void        DOWNLOAD_Deallocate(NPObject* obj)
{
    TRACEINFO;
    MEMFREE(obj);
    return;
}

  void        DOWNLOAD_Invalidate(NPObject* obj)
{
    TRACEINFO;
    return;
}

  bool        DOWNLOAD_HasMethod(NPObject* obj, NPIdentifier name)
{
    TRACEINFO;

    bool result = false;
    int i = 0;
    NPUTF8* utf8methodname = (char*)sBrowserFuncs->utf8fromidentifier(name);
    while ((i < kDOWNLOAD_NUM_METHOD_IDENTIFIERS) && (result == false))
    {
        if ( name == v_DOWNLOADMethodIdentifiers[i] )
        {
            result= true;
        }
        i++;
    }

    return result;
}

bool        DOWNLOAD_Invoke(NPObject* obj, NPIdentifier name, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    TRACEINFO;
    return true;
}

 bool        DOWNLOAD_InvokeDefault(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    TRACEINFO;
    return true;
}

  bool        DOWNLOAD_HasProperty(NPObject* obj, NPIdentifier name)
{
    TRACEINFO;
    bool result = false;
    NPUTF8* utf8propertyname = (char*)sBrowserFuncs->utf8fromidentifier(name);

    int i = 0;
    while ((i < kDOWNLOAD_NUM_PROPERTY_IDENTIFIERS) && (result == false))
    {
        if ( name == v_DOWNLOADPropertyIdentifiers[i] )
        {
            result= true;
        }
        i++;
    }

    return result;
}

  bool        DOWNLOAD_GetProperty(NPObject* obj, NPIdentifier name, NPVariant* result)
{
    TRACEINFO;
    return true;
}

  bool        DOWNLOAD_SetProperty(NPObject *obj, NPIdentifier name, const NPVariant *value)
{
    TRACEINFO;
    return true;
}

 bool        DOWNLOAD_RemoveProperty(NPObject *npobj, NPIdentifier name)
{
    TRACEINFO;
    return true;
}


 bool        DOWNLOAD_Enumerate(NPObject *npobj, NPIdentifier **value, uint32_t *count)
{
    TRACEINFO;
    return true;
}

