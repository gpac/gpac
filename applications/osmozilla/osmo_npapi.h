/*
*			GPAC - Multimedia Framework C SDK
*
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2000-2012
*					All rights reserved
*
*  This file is part of GPAC / Osmozilla NPAPI plugin
*
*  GPAC is free software; you can redistribute it and/or modify
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
*/

#ifndef _OSMO_NPAPI_H_
#define _OSMO_NPAPI_H_


#ifdef WIN32
#include <windows.h>
#ifndef __cplusplus
typedef BOOL bool;
#endif //__cplusplus
#endif

#include "npapi.h"

/*check this with gecko 1.9.2*/
#if (NP_VERSION_MINOR < 20)
#define GECKO_XPCOM
#endif

#ifdef GECKO_XPCOM
#include "npupp.h"

#ifndef uint16_t
typedef uint16 uint16_t;
#endif

#ifndef int16_t
typedef int16 int16_t;
#endif

#define NPINT32	int32

#else

#include "npfunctions.h"

#define NPINT32	int32_t

#endif

#ifdef XP_UNIX
#include <stdio.h>
#endif //XP_UNIX


#ifndef HIBYTE
#define HIBYTE(i) (i >> 8)
#endif

#ifndef LOBYTE
#define LOBYTE(i) (i & 0xff)
#endif

/*functions callbacks to browser*/
NPError Osmozilla_GetURL(NPP instance, const char *url, const char *target);
void Osmozilla_SetStatus(NPP instance, const char *message);


/*
Plugins functions exposed to browser
*/
NPError NPOsmozilla_New(NPMIMEType pluginType, NPP instance, uint16_t mode, int16_t argc, char* argn[], char* argv[], NPSavedData* saved);
NPError NPOsmozilla_Destroy(NPP instance, NPSavedData** save);
NPError NPOsmozilla_SetWindow(NPP instance, NPWindow* window);
NPError NPOsmozilla_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16_t* stype);
NPError NPOsmozilla_DestroyStream(NPP instance, NPStream* stream, NPError reason);
NPINT32 NPOsmozilla_WriteReady(NPP instance, NPStream* stream);
NPINT32 NPOsmozilla_Write(NPP instance, NPStream* stream, NPINT32 offset, NPINT32 len, void* buffer);
void    NPOsmozilla_StreamAsFile(NPP instance, NPStream* stream, const char* fname);
void    NPOsmozilla_Print(NPP instance, NPPrint* platformPrint);
int16_t NPOsmozilla_HandleEvent(NPP instance, void* event);
void    NPOsmozilla_URLNotify(NPP instance, const char* url, NPReason reason, void* notifyData);
NPError NPOsmozilla_GetValue(NPP instance, NPPVariable variable, void *result);
NPError NPOsmozilla_SetValue(NPP instance, NPNVariable variable, void *value);


/*
Functions called by browser
*/

typedef struct __tag_osmozilla Osmozilla;

/*base functions*/
int Osmozilla_Initialize(Osmozilla *osmo, signed short argc, char* argn[], char* argv[]);
void Osmozilla_Shutdown(Osmozilla *osmo);
int Osmozilla_SetWindow(Osmozilla *osmozilla, void *os_wnd_handle, void *os_wnd_display, unsigned int width, unsigned int height);
void Osmozilla_ConnectTo(Osmozilla *osmozilla, const char *url);
void Osmozilla_Print(Osmozilla *osmozilla, unsigned int is_embed, void *os_print_dc, unsigned int target_x, unsigned int target_y, unsigned int target_width, unsigned int target_height);
char *Osmozilla_GetVersion();


/*scripting functions*/
void Osmozilla_Play(Osmozilla *osmo);
void Osmozilla_Pause(Osmozilla *osmo);
void Osmozilla_Stop(Osmozilla *osmo);
void Osmozilla_Update(Osmozilla *osmo, const char *type, const char *commands);
void Osmozilla_QualitySwitch(Osmozilla *osmo, int switch_up);
void Osmozilla_SetURL(Osmozilla *osmo, const char *url);
int Osmozilla_GetDownloadProgress(Osmozilla *osmo);


#ifdef GECKO_XPCOM

void NPOsmozilla_GetServiceManager();
void NPOsmozilla_ReleaseServiceManager();
void NPOsmozilla_ShutdownScript(Osmozilla *osmo);
NPError	NPOsmozilla_GetPeer(Osmozilla *osmo, void *value);
NPError	NPOsmozilla_GetPeerIID(Osmozilla *osmo, void *value);

#endif //GECKO_XPCOM

#endif //_NPPLAT_H_
