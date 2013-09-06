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
#ifndef hbbtvbrowserplugin_h
#define hbbtvbrowserplugin_h

#ifdef __cplusplus
extern "C" {
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <npapi.h>
#include <npfunctions.h>
#include <npruntime.h>

#include "hbbtvbrowserpluginapi.h"

#ifndef DEBUG
	#define DEBUG  true
	#define PROJECTNAME "HbbTVBrowserPlugin"
	#define TRACEINFO if (DEBUG) { fprintf(stderr,"\x1b[%i;3%im%s\x1b[0m : call %s\n", 1, 3, PROJECTNAME, __FUNCTION__); }
	#define NOTIMPLEMENTED if (DEBUG) { printf("%s : %s is \x1b[1;31mNOT IMPLEMENTED\x1b[0m : TODO \n", PROJECTNAME, __FUNCTION__); }
#endif

typedef struct
{	
	NPObject* obj;
	char* type;
	NPWindow* window;
	NPP npp;
}HBBTVPluginData;

NPNetscapeFuncs* sBrowserFuncs;

#define ALLOCBROWSERMEMORY  true

#ifdef	ALLOCBROWSERMEMORY
	#define MEMALLOC sBrowserFuncs->memalloc
	#define MEMFREE  sBrowserFuncs->memfree
#else 
	#define MEMALLOC malloc
	#define MEMFREE  free
#endif

char* booltostr(bool data);

NPError NP_Initialize(NPNetscapeFuncs* bFuncs, NPPluginFuncs* pFuncs);
NPError NP_Shutdown();
NPError NP_GetValue(void *instance, NPPVariable variable, void *value);
char*	NP_GetPluginVersion();
char*	NP_GetMIMEDescription();

NPError OIPF_NPP_New(NPMIMEType    pluginType, NPP instance, uint16_t mode, int16_t argc,   char *argn[], char *argv[], NPSavedData *saved);
NPError OIPF_NPP_Destroy(NPP instance, NPSavedData **save);
NPError OIPF_NPP_SetWindow(NPP instance, NPWindow *window);
NPError OIPF_NPP_HandleEvent(NPP instance, void* Event);
NPError OIPF_NPP_GetValue(NPP instance, NPPVariable variable, void *value);
NPError OIPF_NPP_SetValue(NPP instance, NPNVariable variable, void *value);
NPError OIPF_NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16_t* stype);

#ifdef __cplusplus
}
#endif

#endif
