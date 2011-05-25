/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

//////////////////////////////////////////////////////////////
//
// Main plugin entry point implementation -- exports from the 
// plugin library
//
#include "npplat.h"

#include "osmozilla.h"

NPNetscapeFuncs *sBrowserFunctions = NULL;

NPError OSCALL NP_Shutdown()
{
  NS_PluginShutdown();
  return NPERR_NO_ERROR;
}

static NPError fillPluginFunctionTable(NPPluginFuncs* aNPPFuncs)
{
  if(aNPPFuncs == NULL)
    return NPERR_INVALID_FUNCTABLE_ERROR;

  // Set up the plugin function table that Netscape will use to
  // call us. Netscape needs to know about our version and size   
  // and have a UniversalProcPointer for every function we implement.

  aNPPFuncs->version       = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
#ifdef XP_MAC
  aNPPFuncs->newp          = NewNPP_NewProc(Private_New);
  aNPPFuncs->destroy       = NewNPP_DestroyProc(Private_Destroy);
  aNPPFuncs->setwindow     = NewNPP_SetWindowProc(Private_SetWindow);
  aNPPFuncs->newstream     = NewNPP_NewStreamProc(Private_NewStream);
  aNPPFuncs->destroystream = NewNPP_DestroyStreamProc(Private_DestroyStream);
  aNPPFuncs->asfile        = NewNPP_StreamAsFileProc(Private_StreamAsFile);
  aNPPFuncs->writeready    = NewNPP_WriteReadyProc(Private_WriteReady);
  aNPPFuncs->write         = NewNPP_WriteProc(Private_Write);
  aNPPFuncs->print         = NewNPP_PrintProc(Private_Print);
  aNPPFuncs->event         = NewNPP_HandleEventProc(Private_HandleEvent);	
  aNPPFuncs->urlnotify     = NewNPP_URLNotifyProc(Private_URLNotify);			
  aNPPFuncs->getvalue      = NewNPP_GetValueProc(Private_GetValue);
  aNPPFuncs->setvalue      = NewNPP_SetValueProc(Private_SetValue);
#else
  aNPPFuncs->newp          = NPOsomzilla_New;
  aNPPFuncs->destroy       = NPOsomzilla_Destroy;
  aNPPFuncs->setwindow     = NPOsomzilla_SetWindow;
  aNPPFuncs->newstream     = NPOsomzilla_NewStream;
  aNPPFuncs->destroystream = NPOsomzilla_DestroyStream;
  aNPPFuncs->asfile        = NPOsomzilla_StreamAsFile;
  aNPPFuncs->writeready    = NPOsomzilla_WriteReady;
  aNPPFuncs->write         = NPOsomzilla_Write;
  aNPPFuncs->print         = NPOsomzilla_Print;
  aNPPFuncs->event         = NPOsomzilla_HandleEvent;
  aNPPFuncs->urlnotify     = NPOsomzilla_URLNotify;
  aNPPFuncs->getvalue      = NPOsomzilla_GetValue;
  aNPPFuncs->setvalue      = NPOsomzilla_SetValue;
#endif
#ifdef OJI
  aNPPFuncs->javaClass     = NULL;
#endif

  return NPERR_NO_ERROR;
}


#ifdef XP_WIN

NPError OSCALL NP_Initialize(NPNetscapeFuncs* aNPNFuncs)
{
	sBrowserFunctions = aNPNFuncs;
	return NS_PluginInitialize();
}

NPError OSCALL NP_GetEntryPoints(NPPluginFuncs* aNPPFuncs)
{
  return fillPluginFunctionTable(aNPPFuncs);
}

#endif


#ifdef XP_UNIX

NPError OSCALL NP_Initialize(NPNetscapeFuncs* aNPNFuncs, NPPluginFuncs* aNPPFuncs)
{
	sBrowserFunctions = aNPNFuncs;
	rv = fillPluginFunctionTable(aNPPFuncs);
	if(rv != NPERR_NO_ERROR)
		return rv;

	return NS_PluginInitialize();
}

char * NP_GetMIMEDescription(void)
{
	return NPP_GetMIMEDescription();
}

NPError NP_GetValue(void *future, NPPVariable aVariable, void *aValue)
{
	return NS_PluginGetValue(aVariable, aValue);
}

#endif


#ifdef XP_MAC


#if !TARGET_API_MAC_CARBON
QDGlobals* gQDPtr; // Pointer to Netscape's QuickDraw globals
#endif

short gResFile; // Refnum of the plugin's resource file

NPError Private_Initialize(void)
{
  NPError rv = NS_PluginInitialize();
  return rv;
}

void Private_Shutdown(void)
{
  NS_PluginShutdown();
  __destroy_global_chain();
}

void SetUpQD(void);

void SetUpQD(void)
{
  ProcessSerialNumber PSN;
  FSSpec              myFSSpec;
  Str63               name;
  ProcessInfoRec      infoRec;
  OSErr               result = noErr;
  CFragConnectionID   connID;
  Str255              errName;

  // Memorize the plugin�s resource file refnum for later use.
  gResFile = CurResFile();

#if !TARGET_API_MAC_CARBON
  // Ask the system if CFM is available.
  long response;
  OSErr err = Gestalt(gestaltCFMAttr, &response);
  Boolean hasCFM = BitTst(&response, 31-gestaltCFMPresent);

  if (hasCFM) {
    // GetProcessInformation takes a process serial number and 
    // will give us back the name and FSSpec of the application.
    // See the Process Manager in IM.
    infoRec.processInfoLength = sizeof(ProcessInfoRec);
    infoRec.processName = name;
    infoRec.processAppSpec = &myFSSpec;

    PSN.highLongOfPSN = 0;
    PSN.lowLongOfPSN = kCurrentProcess;

    result = GetProcessInformation(&PSN, &infoRec);
  }
	else
    // If no CFM installed, assume it must be a 68K app.
    result = -1;		

  if (result == noErr) {
    // Now that we know the app name and FSSpec, we can call GetDiskFragment
    // to get a connID to use in a subsequent call to FindSymbol (it will also
    // return the address of �main� in app, which we ignore).  If GetDiskFragment 
    // returns an error, we assume the app must be 68K.
    Ptr mainAddr; 	
    result =  GetDiskFragment(infoRec.processAppSpec, 0L, 0L, infoRec.processName,
                              kReferenceCFrag, &connID, (Ptr*)&mainAddr, errName);
  }

  if (result == noErr) {
    // The app is a PPC code fragment, so call FindSymbol
    // to get the exported �qd� symbol so we can access its
    // QuickDraw globals.
    CFragSymbolClass symClass;
    result = FindSymbol(connID, "\pqd", (Ptr*)&gQDPtr, &symClass);
  }
  else {
    // The app is 68K, so use its A5 to compute the address
    // of its QuickDraw globals.
    gQDPtr = (QDGlobals*)(*((long*)SetCurrentA5()) - (sizeof(QDGlobals) - sizeof(GrafPtr)));
  }
#endif /* !TARGET_API_MAC_CARBON */
}

NPError main(NPNetscapeFuncs* nsTable, NPPluginFuncs* pluginFuncs, NPP_ShutdownUPP* unloadUpp);

#if !TARGET_API_MAC_CARBON
#pragma export on
#if GENERATINGCFM
RoutineDescriptor mainRD = BUILD_ROUTINE_DESCRIPTOR(uppNPP_MainEntryProcInfo, main);
#endif
#pragma export off
#endif /* !TARGET_API_MAC_CARBON */


NPError main(NPNetscapeFuncs* aNPNFuncs, NPPluginFuncs* aNPPFuncs, NPP_ShutdownUPP* aUnloadUpp)
{
	NPError rv = NPERR_NO_ERROR;

	if (aUnloadUpp == NULL)
		rv = NPERR_INVALID_FUNCTABLE_ERROR;

	sBrowserFunctions = aNPNFuncs;

	if (rv == NPERR_NO_ERROR) {
		// defer static constructors until the global functions are initialized.
		__InitCode__();
		rv = fillPluginFunctionTable(aNPPFuncs);
	}

	*aUnloadUpp = NewNPP_ShutdownProc(Private_Shutdown);
	SetUpQD();
	rv = Private_Initialize();

	return rv;
}
#endif //XP_MAC
