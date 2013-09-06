
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
 */#include "keysetclass.h"

#define kKEYSET_ID_PROPERTY_VALUE					       	0
#define kKEYSET_ID_PROPERTY_MAXIMUMVALUE   					1
#define kKEYSET_ID_PROPERTY_RED   							2
#define kKEYSET_ID_PROPERTY_GREEN   						3
#define kKEYSET_ID_PROPERTY_YELLOW   						4
#define kKEYSET_ID_PROPERTY_BLUE  							5
#define kKEYSET_ID_PROPERTY_NAVIGATION   					6
#define kKEYSET_ID_PROPERTY_VCR   							7
#define kKEYSET_ID_PROPERTY_SCROLL   						8
#define kKEYSET_ID_PROPERTY_INFO   							9
#define kKEYSET_ID_PROPERTY_NUMERIC   						10
#define kKEYSET_ID_PROPERTY_ALPHA  							11
#define kKEYSET_ID_PROPERTY_OTHER   						12
#define kKEYSET_NUM_PROPERTY_IDENTIFIERS                    13

#define kKEYSET_ID_METHOD_SETVALUE               	        0
#define kKEYSET_NUM_METHOD_IDENTIFIERS                	    1



bool            v_bKEYSETIdentifiersInitialized = false;

NPIdentifier    v_KEYSETPropertyIdentifiers[kKEYSET_NUM_PROPERTY_IDENTIFIERS];
const NPUTF8 *  v_KEYSETPropertyNames[kKEYSET_NUM_PROPERTY_IDENTIFIERS] = {
	"value",
    "maximumValue",
    "RED",
    "GREEN",
    "YELLOW",
    "BLUE",
    "NAVIGATION",
    "VCR",
    "SCROLL",
    "INFO",
    "NUMERIC",
    "ALPHA",
    "OTHER"
    };

NPIdentifier    v_KEYSETMethodIdentifiers[kKEYSET_NUM_METHOD_IDENTIFIERS];
const NPUTF8 *  v_KEYSETMethodNames[kKEYSET_NUM_METHOD_IDENTIFIERS] = {
	"setValue"
	};

static  void    KEYSETinitializeIdentifiers(void)
{
    sBrowserFuncs->getstringidentifiers( v_KEYSETPropertyNames, kKEYSET_NUM_PROPERTY_IDENTIFIERS, v_KEYSETPropertyIdentifiers );
    sBrowserFuncs->getstringidentifiers( v_KEYSETMethodNames,   kKEYSET_NUM_METHOD_IDENTIFIERS,   v_KEYSETMethodIdentifiers );
}


NPClass  stKEYSETclass;
NPClass* pKEYSETclass = NULL;

NPClass* fillKEYSETpclass(void)
{
    TRACEINFO;
    if (pKEYSETclass == NULL)
    {
        stKEYSETclass.allocate          = KEYSET_Allocate;
        stKEYSETclass.deallocate        = KEYSET_Deallocate;
        stKEYSETclass.invalidate        = KEYSET_Invalidate;
        stKEYSETclass.hasMethod         = KEYSET_HasMethod;
        stKEYSETclass.invoke            = KEYSET_Invoke;
        stKEYSETclass.invokeDefault     = KEYSET_InvokeDefault;
        stKEYSETclass.hasProperty       = KEYSET_HasProperty;
        stKEYSETclass.getProperty       = KEYSET_GetProperty;
        stKEYSETclass.setProperty       = KEYSET_SetProperty;
        stKEYSETclass.removeProperty    = KEYSET_RemoveProperty;
        stKEYSETclass.enumerate         = KEYSET_Enumerate;
        pKEYSETclass = &stKEYSETclass;
    }

    return pKEYSETclass;
}


NPObject *          KEYSET_Allocate(NPP npp, NPClass *theClass)
{
    TRACEINFO;

    NPObject* result;

    
    if (!v_bKEYSETIdentifiersInitialized)
    {
        v_bKEYSETIdentifiersInitialized = true;
        KEYSETinitializeIdentifiers();
    }
	
	NPObj_KeySet* newkeyset = (NPObj_KeySet *)MEMALLOC(sizeof(NPObj_KeySet));
	newkeyset->npp = npp;
	newkeyset->value = (double*)MEMALLOC(sizeof(double));
    newkeyset->maximumValue = (double*)MEMALLOC(sizeof(double));
	result = (NPObject*)newkeyset;
	OnKEYSET_Allocate();
    return  result;
}


void        KEYSET_Deallocate(NPObject* obj)
{
    TRACEINFO;
    NPObj_KeySet* keysetobj = (NPObj_KeySet*)obj;
    MEMFREE(keysetobj->value);
    MEMFREE(keysetobj->maximumValue);
    MEMFREE(keysetobj);
    return;
}

void        KEYSET_Invalidate(NPObject* obj)
{
    TRACEINFO;
    return;
}

bool        KEYSET_HasMethod(NPObject* obj, NPIdentifier name)
{
    TRACEINFO;
	bool result = false;
    int i = 0;
    NPUTF8* utf8methodname = (char*)sBrowserFuncs->utf8fromidentifier(name);
    while ((i < kKEYSET_NUM_METHOD_IDENTIFIERS) && (result == false))
    {
        if ( name == v_KEYSETMethodIdentifiers[i] )
        {
            result= true;
        }
        i++;
    }
    
    return result;
}

bool        KEYSET_Invoke(NPObject* obj, NPIdentifier name, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    TRACEINFO;
    bool fctresult = false;
    if (name == v_KEYSETMethodIdentifiers[kKEYSET_ID_METHOD_SETVALUE])
    {
		KEYSET_Invoke_SetValue(obj, args, argCount, result);
		fctresult = true;
    }
    else
    {    	
    	fctresult = false;
    }
    return fctresult;
}

bool        KEYSET_InvokeDefault(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    TRACEINFO;
    return true;
}

bool        KEYSET_HasProperty(NPObject* obj, NPIdentifier name)
{
    TRACEINFO;
    bool result = false;
    NPUTF8* utf8propertyname = (char*)sBrowserFuncs->utf8fromidentifier(name);

    int i = 0;
    while ((i < kKEYSET_NUM_PROPERTY_IDENTIFIERS) && (result == false))
    {
        if ( name == v_KEYSETPropertyIdentifiers[i] )
        {
            result= true;
        }
        i++;
    }

    
    return result;
}

bool        KEYSET_GetProperty(NPObject* obj, NPIdentifier name, NPVariant* result)
{
    TRACEINFO;
    bool fctresult = false;
    NPObj_KeySet* keysetobj = (NPObj_KeySet*)obj;
    
    double 	KEYMASK_RED 		= 0x1;
	double 	KEYMASK_GREEN 		= 0x2;
	double 	KEYMASK_YELLOW 		= 0x4;
	double 	KEYMASK_BLUE 		= 0x8;
	double 	KEYMASK_NAVIGATION 	= 0x10;
	double 	KEYMASK_VCR 		= 0x20;
	double 	KEYMASK_SCROLL 		= 0x40;
	double 	KEYMASK_INFO 		= 0x80;
	double 	KEYMASK_NUMERIC 	= 0x100;
	double 	KEYMASK_ALPHA 		= 0x200;
	double 	KEYMASK_OTHER 		= 0x400;
	
	if (name == v_KEYSETPropertyIdentifiers[kKEYSET_ID_PROPERTY_RED])
	{    			
		DOUBLE_TO_NPVARIANT(KEYMASK_RED, *result);
    	fctresult = true;
	}
	else
	if (name == v_KEYSETPropertyIdentifiers[kKEYSET_ID_PROPERTY_GREEN])
	{    			
		DOUBLE_TO_NPVARIANT(KEYMASK_GREEN, *result);
    	fctresult = true;
	}
	if (name == v_KEYSETPropertyIdentifiers[kKEYSET_ID_PROPERTY_YELLOW])
	{    			
		DOUBLE_TO_NPVARIANT(KEYMASK_YELLOW, *result);
    	fctresult = true;
	}
	if (name == v_KEYSETPropertyIdentifiers[kKEYSET_ID_PROPERTY_BLUE])
	{    			
		DOUBLE_TO_NPVARIANT(KEYMASK_BLUE, *result);
    	fctresult = true;
	}
	if (name == v_KEYSETPropertyIdentifiers[kKEYSET_ID_PROPERTY_NAVIGATION])
	{    			
		DOUBLE_TO_NPVARIANT(KEYMASK_NAVIGATION, *result);
    	fctresult = true;
	}
	if (name == v_KEYSETPropertyIdentifiers[kKEYSET_ID_PROPERTY_VCR])
	{    			
		DOUBLE_TO_NPVARIANT(KEYMASK_VCR, *result);
    	fctresult = true;
	}
	if (name == v_KEYSETPropertyIdentifiers[kKEYSET_ID_PROPERTY_SCROLL])
	{    			
		DOUBLE_TO_NPVARIANT(KEYMASK_SCROLL, *result);
    	fctresult = true;
	}
	if (name == v_KEYSETPropertyIdentifiers[kKEYSET_ID_PROPERTY_INFO])
	{    			
		DOUBLE_TO_NPVARIANT(KEYMASK_INFO, *result);
    	fctresult = true;
	}
	if (name == v_KEYSETPropertyIdentifiers[kKEYSET_ID_PROPERTY_NUMERIC])
	{    			
		DOUBLE_TO_NPVARIANT(KEYMASK_NUMERIC, *result);
    	fctresult = true;
	}
	if (name == v_KEYSETPropertyIdentifiers[kKEYSET_ID_PROPERTY_ALPHA])
	{    			
		DOUBLE_TO_NPVARIANT(KEYMASK_ALPHA, *result);
    	fctresult = true;
	}
	if (name == v_KEYSETPropertyIdentifiers[kKEYSET_ID_PROPERTY_OTHER])
	{    			
		DOUBLE_TO_NPVARIANT(KEYMASK_OTHER, *result);
    	fctresult = true;
	}
	if (name == v_KEYSETPropertyIdentifiers[kKEYSET_ID_PROPERTY_VALUE])
	{    			
		double* value = keysetobj->value;
		DOUBLE_TO_NPVARIANT(*value, *result);
    	fctresult = true;
	}
	
	if (fctresult)
	{
							
	}
    
		
    
    return fctresult;    
}

bool        KEYSET_SetProperty(NPObject *obj, NPIdentifier name, const NPVariant *value)
{
    TRACEINFO;
    bool fctresult = false;
    
    return fctresult;
}

bool        KEYSET_RemoveProperty(NPObject *npobj, NPIdentifier name)
{
    TRACEINFO;
    return true;
}


bool        KEYSET_Enumerate(NPObject *npobj, NPIdentifier **value, uint32_t *count)
{
    TRACEINFO;
    return true;
}


/** implementation **/
void KEYSET_Invoke_SetValue(NPObject* obj,const NPVariant* args, uint32_t argCount, NPVariant *result)
{
	TRACEINFO;
	//~ NOTIMPLEMENTED; // in progress
	double param;
	NPObj_KeySet* keysetobj = (NPObj_KeySet*)obj;
	double* value = keysetobj->value;
	fprintf(stderr,"nb args : %i, %f", argCount,NPVARIANT_TO_DOUBLE(args[0]) );
	if (!NPVARIANT_IS_DOUBLE(args[0])) {
		fprintf(stderr,"\t%s : error\n",__FUNCTION__);
		return;
    }
    else {
		param = NPVARIANT_TO_DOUBLE( args[0] );
		fprintf(stderr, "\t%s : param to transmit %f \n", __FUNCTION__, param);
		*value = param;
		OnKEYSET_SetValue(param);
	}
}
