/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _NPPLAT_H_
#define _NPPLAT_H_

/**************************************************/
/*                                                */
/*                   Windows                      */
/*                                                */
/**************************************************/

#ifdef WIN32
#include <windows.h>
#endif

#include "npapi.h"

/*check this with gecko 1.9.2*/
#if (NP_VERSION_MINOR < 20)
#define GECKO_OLD_API
#endif


#ifdef GECKO_OLD_API
#include "npupp.h"

#ifndef uint16_t
typedef uint16 uint16_t;
#endif

#ifndef int16_t
typedef int16 int16_t;
#endif

#else
#include "npfunctions.h"
#endif

/**************************************************/
/*                                                */
/*                    Unix                        */
/*                                                */
/**************************************************/
#ifdef XP_UNIX
#include <stdio.h>
#endif //XP_UNIX

/**************************************************/
/*                                                */
/*                     Mac                        */
/*                                                */
/**************************************************/
#ifdef XP_MAC

#include <Processes.h>
#include <Gestalt.h>
#include <CodeFragments.h>
#include <Timer.h>
#include <Resources.h>
#include <ToolUtils.h>

#include "jri.h"

// The Mixed Mode procInfos defined in npupp.h assume Think C-
// style calling conventions.  These conventions are used by
// Metrowerks with the exception of pointer return types, which
// in Metrowerks 68K are returned in A0, instead of the standard
// D0. Thus, since NPN_MemAlloc and NPN_UserAgent return pointers,
// Mixed Mode will return the values to a 68K plugin in D0, but 
// a 68K plugin compiled by Metrowerks will expect the result in
// A0.  The following pragma forces Metrowerks to use D0 instead.
//
#ifdef __MWERKS__
#ifndef powerc
#pragma pointers_in_D0
#endif
#endif

#ifdef __MWERKS__
#ifndef powerc
#pragma pointers_in_A0
#endif
#endif

// The following fix for static initializers (which fixes a preious
// incompatibility with some parts of PowerPlant, was submitted by 
// Jan Ulbrich.
#ifdef __MWERKS__
	#ifdef __cplusplus
	extern "C" {
	#endif
		#ifndef powerc
			extern void	__InitCode__(void);
		#else
			extern void __sinit(void);
			#define __InitCode__ __sinit
		#endif
		extern void	__destroy_global_chain(void);
	#ifdef __cplusplus
	}
	#endif // __cplusplus
#endif // __MWERKS__

// Wrapper functions for all calls from Netscape to the plugin.
// These functions let the plugin developer just create the APIs
// as documented and defined in npapi.h, without needing to 
// install those functions in the function table or worry about
// setting up globals for 68K plugins.
NPError Private_Initialize(void);
void    Private_Shutdown(void);
NPError Private_New(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc, char* argn[], char* argv[], NPSavedData* saved);
NPError Private_Destroy(NPP instance, NPSavedData** save);
NPError Private_SetWindow(NPP instance, NPWindow* window);
NPError Private_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16* stype);
NPError Private_DestroyStream(NPP instance, NPStream* stream, NPError reason);
int32   Private_WriteReady(NPP instance, NPStream* stream);
int32   Private_Write(NPP instance, NPStream* stream, int32 offset, int32 len, void* buffer);
void    Private_StreamAsFile(NPP instance, NPStream* stream, const char* fname);
void    Private_Print(NPP instance, NPPrint* platformPrint);
int16   Private_HandleEvent(NPP instance, void* event);
void    Private_URLNotify(NPP instance, const char* url, NPReason reason, void* notifyData);
jref    Private_GetJavaClass(void);
NPError Private_GetValue(NPP instance, NPPVariable variable, void *result);
NPError Private_SetValue(NPP instance, NPNVariable variable, void *value);

#endif //XP_MAC


NPError NPOsomzilla_New(NPMIMEType pluginType, NPP instance, uint16_t mode, int16_t argc, char* argn[], char* argv[], NPSavedData* saved);
NPError NPOsomzilla_Destroy(NPP instance, NPSavedData** save);
NPError NPOsomzilla_SetWindow(NPP instance, NPWindow* window);
NPError NPOsomzilla_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16_t* stype);
NPError NPOsomzilla_DestroyStream(NPP instance, NPStream* stream, NPError reason);

#ifdef GECKO_OLD_API
int32 NPOsomzilla_WriteReady(NPP instance, NPStream* stream);
int32 NPOsomzilla_Write (NPP instance, NPStream *stream, int32 offset, int32 len, void *buffer);
#else
int32_t   NPOsomzilla_WriteReady(NPP instance, NPStream* stream);
int32_t   NPOsomzilla_Write(NPP instance, NPStream* stream, int32_t offset, int32_t len, void* buffer);
#endif

void    NPOsomzilla_StreamAsFile(NPP instance, NPStream* stream, const char* fname);
void    NPOsomzilla_Print(NPP instance, NPPrint* platformPrint);
int16_t   NPOsomzilla_HandleEvent(NPP instance, void* event);
void    NPOsomzilla_URLNotify(NPP instance, const char* url, NPReason reason, void* notifyData);
NPError NPOsomzilla_GetValue(NPP instance, NPPVariable variable, void *result);
NPError NPOsomzilla_SetValue(NPP instance, NPNVariable variable, void *value);


#ifndef HIBYTE
#define HIBYTE(i) (i >> 8)
#endif

#ifndef LOBYTE
#define LOBYTE(i) (i & 0xff)
#endif


#ifdef GECKO_OLD_API
void* NPN_MemAlloc(unsigned int size);
#endif


#endif //_NPPLAT_H_
