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

#define kAPPPRIVDATA_ID_PROPERTY_KEYSET					       	0
#define kAPPPRIVDATA_ID_PROPERTY_CURRENTCHANNEL   				1
#define kAPPPRIVDATA_NUM_PROPERTY_IDENTIFIERS                   2

#define kAPPPRIVDATA_ID_METHOD_GETMEMFREE               	    0
#define kAPPPRIVDATA_NUM_METHOD_IDENTIFIERS                	    1



bool            v_bAPPPRIVDATAIdentifiersInitialized = false;

NPIdentifier    v_APPPRIVDATAPropertyIdentifiers[kAPPPRIVDATA_NUM_PROPERTY_IDENTIFIERS];
const NPUTF8 *  v_APPPRIVDATAPropertyNames[kAPPPRIVDATA_NUM_PROPERTY_IDENTIFIERS] = {
	"keyset",
    "currentChannel"
    };

NPIdentifier    v_APPPRIVDATAMethodIdentifiers[kAPPPRIVDATA_NUM_METHOD_IDENTIFIERS];
const NPUTF8 *  v_APPPRIVDATAMethodNames[kAPPPRIVDATA_NUM_METHOD_IDENTIFIERS] = {
	"getMEMFREE"
	};

static  void    APPPRIVDATAinitializeIdentifiers(void)
{
    sBrowserFuncs->getstringidentifiers( v_APPPRIVDATAPropertyNames, kAPPPRIVDATA_NUM_PROPERTY_IDENTIFIERS, v_APPPRIVDATAPropertyIdentifiers );
    sBrowserFuncs->getstringidentifiers( v_APPPRIVDATAMethodNames,   kAPPPRIVDATA_NUM_METHOD_IDENTIFIERS,   v_APPPRIVDATAMethodIdentifiers );
}


NPClass  stAPPPRIVDATAclass;
NPClass* pAPPPRIVDATAclass = NULL;

NPClass* fillAPPPRIVDATApclass(void)
{
    TRACEINFO;
    if (pAPPPRIVDATAclass == NULL)
    {
        stAPPPRIVDATAclass.allocate          = APPPRIVDATA_Allocate;
        stAPPPRIVDATAclass.deallocate        = APPPRIVDATA_Deallocate;
        stAPPPRIVDATAclass.invalidate        = APPPRIVDATA_Invalidate;
        stAPPPRIVDATAclass.hasMethod         = APPPRIVDATA_HasMethod;
        stAPPPRIVDATAclass.invoke            = APPPRIVDATA_Invoke;
        stAPPPRIVDATAclass.invokeDefault     = APPPRIVDATA_InvokeDefault;
        stAPPPRIVDATAclass.hasProperty       = APPPRIVDATA_HasProperty;
        stAPPPRIVDATAclass.getProperty       = APPPRIVDATA_GetProperty;
        stAPPPRIVDATAclass.setProperty       = APPPRIVDATA_SetProperty;
        stAPPPRIVDATAclass.removeProperty    = APPPRIVDATA_RemoveProperty;
        stAPPPRIVDATAclass.enumerate         = APPPRIVDATA_Enumerate;
        pAPPPRIVDATAclass = &stAPPPRIVDATAclass;
    }

    return pAPPPRIVDATAclass;
}


NPObject *          APPPRIVDATA_Allocate(NPP npp, NPClass *theClass)
{
    TRACEINFO;

    NPObject* result;

    
    if (!v_bAPPPRIVDATAIdentifiersInitialized)
    {
        v_bAPPPRIVDATAIdentifiersInitialized = true;
        APPPRIVDATAinitializeIdentifiers();
    }
		
	NPObj_AppPrivData* newappprivdata = (NPObj_AppPrivData*)MEMALLOC(sizeof(NPObj_AppPrivData));    
	fprintf(stderr, "\t%s : Allocation at \x1b[%i;3%im%p\n\x1b[0m",__FUNCTION__, 1, 1, newappprivdata);    
    newappprivdata->npp = npp;
    newappprivdata->keyset = sBrowserFuncs->createobject(npp, fillKEYSETpclass());    
    fprintf(stderr, "\t%s : Create keyset property at \x1b[%i;3%im%p\n\x1b[0m",__FUNCTION__, 1, 1, newappprivdata->keyset );
    result = (NPObject*)newappprivdata;
    return result;
}


void        APPPRIVDATA_Deallocate(NPObject* obj)
{
    TRACEINFO;
    NPObj_AppPrivData* appprivdataobj = (NPObj_AppPrivData*)obj;
    sBrowserFuncs->releaseobject(appprivdataobj->keyset);
    MEMFREE(appprivdataobj);
    return;
}

void        APPPRIVDATA_Invalidate(NPObject* obj)
{
    TRACEINFO;
    return;
}

bool        APPPRIVDATA_HasMethod(NPObject* obj, NPIdentifier name)
{
    TRACEINFO;
	bool result = false;
    int i = 0;
    NPUTF8* utf8methodname = (char*)sBrowserFuncs->utf8fromidentifier(name);
    while ((i < kAPPPRIVDATA_NUM_METHOD_IDENTIFIERS) && (result == false))
    {
        if ( name == v_APPPRIVDATAMethodIdentifiers[i] )
        {
            result= true;
        }
        i++;
    }
    
    return result;
}

bool        APPPRIVDATA_Invoke(NPObject* obj, NPIdentifier name, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    TRACEINFO;
    bool fctresult = false;
    if (name == v_APPPRIVDATAMethodIdentifiers[kAPPPRIVDATA_ID_METHOD_GETMEMFREE])
    {
		APPPRIVDATA_Invoke_GetFreeMen(obj, args, argCount);
		fctresult = true;
    }
    else
    {    	
    	fctresult = false;
    }
    return fctresult;
}

bool        APPPRIVDATA_InvokeDefault(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    TRACEINFO;
    return true;
}

bool        APPPRIVDATA_HasProperty(NPObject* obj, NPIdentifier name)
{
    TRACEINFO;
    bool result = false;
    NPUTF8* utf8propertyname = (char*)sBrowserFuncs->utf8fromidentifier(name);

    int i = 0;
    while ((i < kAPPPRIVDATA_NUM_PROPERTY_IDENTIFIERS) && (result == false))
    {
        if ( name == v_APPPRIVDATAPropertyIdentifiers[i] )
        {
            result= true;
        }
        i++;
    }

   
    return result;
}

bool        APPPRIVDATA_GetProperty(NPObject* obj, NPIdentifier name, NPVariant* result)
{
    TRACEINFO;
   
    bool fctresult = false;
	NPObj_AppPrivData* AppPrivDataObj = (NPObj_AppPrivData*)obj;
	if (!strcmp(sBrowserFuncs->utf8fromidentifier(name), v_APPPRIVDATAPropertyNames[kAPPPRIVDATA_ID_PROPERTY_KEYSET]))    
    {    	
		sBrowserFuncs->retainobject(AppPrivDataObj->keyset);
		OBJECT_TO_NPVARIANT(AppPrivDataObj->keyset, *result);
		
    	fctresult = true;
    }
    else
		
    return fctresult;
}

bool        APPPRIVDATA_SetProperty(NPObject *obj, NPIdentifier name, const NPVariant *value)
{
    TRACEINFO;
    bool fctresult = false;
    return fctresult;
}

bool        APPPRIVDATA_RemoveProperty(NPObject *npobj, NPIdentifier name)
{
    TRACEINFO;
    return true;
}


bool        APPPRIVDATA_Enumerate(NPObject *npobj, NPIdentifier **value, uint32_t *count)
{
    TRACEINFO;
    return true;
}


/** implementation **/
void APPPRIVDATA_Invoke_GetFreeMen(NPObject* obj,const NPVariant* args, uint32_t argCount)
{
	TRACEINFO;
	NOTIMPLEMENTED;
}
