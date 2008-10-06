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

#ifndef __PLUGIN_H__
#define __PLUGIN_H__

#include <nsIClassInfo.h>

#include "npplat.h"
#include "nsIOsmozilla.h"

#include <gpac/terminal.h>

#ifndef WIN32
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/cursorfont.h>
#endif

class nsOsmozillaPeer;

struct nsPluginCreateData
{
  NPP instance;
  NPMIMEType type; 
  uint16 mode; 
  int16 argc; 
  char** argn; 
  char** argv; 
  NPSavedData* saved;
};

class nsPluginInstanceBase
{
public:
  // these three methods must be implemented in the derived
  // class platform specific way
  virtual NPBool init(NPWindow* aWindow) = 0;
  virtual void shut() = 0;
  virtual NPBool isInitialized() = 0;

  // implement all or part of those methods in the derived 
  // class as needed
  virtual NPError SetWindow(NPWindow* pNPWindow)                    { return NPERR_NO_ERROR; }
  virtual NPError NewStream(NPMIMEType type, NPStream* stream, 
                            NPBool seekable, uint16* stype)         { return NPERR_NO_ERROR; }
  virtual NPError DestroyStream(NPStream *stream, NPError reason)   { return NPERR_NO_ERROR; }
  virtual void    StreamAsFile(NPStream* stream, const char* fname) { return; }
  virtual int32   WriteReady(NPStream *stream)                      { return 0x0fffffff; }
  virtual int32   Write(NPStream *stream, int32 offset, 
                        int32 len, void *buffer)                    { return len; }
  virtual void    Print(NPPrint* printInfo)                         { return; }
    virtual uint16  HandleEvent(void* event) { return 0; }
  virtual void    URLNotify(const char* url, NPReason reason, 
                            void* notifyData)                       { return; }
  virtual NPError GetValue(NPPVariable variable, void *value)       { return NPERR_NO_ERROR; }
  virtual NPError SetValue(NPNVariable variable, void *value)       { return NPERR_NO_ERROR; }
};

// functions that should be implemented for each specific plugin

// creation and destruction of the object of the derived class
nsPluginInstanceBase * NS_NewPluginInstance(nsPluginCreateData * aCreateDataStruct);
void NS_DestroyPluginInstance(nsPluginInstanceBase * aPlugin);

// global plugin initialization and shutdown
NPError NS_PluginInitialize();
void NS_PluginShutdown();

// global to get plugins name & description 
NPError NS_PluginGetValue(NPPVariable aVariable, void *aValue);
char* NPP_GetMIMEDescription(void);


//#define NO_GPAC

class nsOsmozillaInstance : public nsPluginInstanceBase
{
public:
	
	nsOsmozillaInstance(nsPluginCreateData * aCreateDataStruct);
	virtual ~nsOsmozillaInstance();

	NPBool init(NPWindow* aWindow);
	void shut();
	NPBool isInitialized() {return mInitialized;}


	NPError SetWindow(NPWindow* aWindow);
	NPError NewStream(NPMIMEType type, NPStream * stream, NPBool seekable,uint16 * stype);
	NPError DestroyStream(NPStream * stream, NPError reason);
	NPError GetValue(NPPVariable aVariable, void *aValue);
    virtual uint16 HandleEvent(void* event);
    

	nsOsmozillaPeer * getScriptablePeer();

	// locals
	const char * getVersion();

	int m_argc;
	char **m_argv;
	char **m_argn;
	nsOsmozillaPeer *mScriptablePeer;

	void Pause();
	void Play();
	void Stop();
	void Update(const char *type, const char *commands);
	void Print(NPPrint* printInfo);
	Bool EventProc(GF_Event *evt);


private:
	NPP mInstance;
	NPBool mInitialized;

#ifdef XP_WIN
	HWND m_hWnd;
#endif
#ifdef XP_UNIX
	Window mWindow;
	Display *mDisplay;
	XFontStruct *mFontInfo;
	Widget mXtwidget;
#endif

#ifdef XP_MACOSX
	NPWindow *window;
#endif

	/*general options*/
	Bool m_bLoop, m_bAutoStart, m_bIsConnected;

	GF_Terminal *m_term;
	GF_User m_user;

	char *m_szURL;

	Bool m_isopen, m_paused, m_url_changed, m_bUse3D, m_disable_mime;
	u32 max_duration, m_log_level, m_log_tools, aspect_ratio;
	FILE *m_logs;
	Bool m_bCanSeek;
	Double m_Duration;
	uint32 m_height, m_width;
	unsigned char *m_navigate_url;
	u32 current_time_ms, m_prev_time;
	Float current_FPS;

	void SetOptions();
};

// We must implement nsIClassInfo because it signals the
	// Mozilla Security Manager to allow calls from JavaScript.
	// helper class to implement all necessary nsIClassInfo method stubs
	// and to set flags used by the security system
	
class nsClassInfoMixin : public nsIClassInfo
	{
	// These flags are used by the DOM and security systems to signal that
	// JavaScript callers are allowed to call this object's scritable methods.
	NS_IMETHOD GetFlags(PRUint32 *aFlags)
	{*aFlags = nsIClassInfo::PLUGIN_OBJECT | nsIClassInfo::DOM_OBJECT;
	return NS_OK;}
	
	NS_IMETHOD GetImplementationLanguage(PRUint32 *aImplementationLanguage)
	{*aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
	return NS_OK;}
	
	// The rest of the methods can safely return error codes...
	NS_IMETHOD GetInterfaces(PRUint32 *count, nsIID * **array)
	{return NS_ERROR_NOT_IMPLEMENTED;}
	NS_IMETHOD GetHelperForLanguage(PRUint32 language, nsISupports **_retval)
	{return NS_ERROR_NOT_IMPLEMENTED;}
	NS_IMETHOD GetContractID(char * *aContractID)
	{return NS_ERROR_NOT_IMPLEMENTED;}
	NS_IMETHOD GetClassDescription(char * *aClassDescription)
	{return NS_ERROR_NOT_IMPLEMENTED;}
	NS_IMETHOD GetClassID(nsCID * *aClassID)
	{return NS_ERROR_NOT_IMPLEMENTED;}
	NS_IMETHOD GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
	{return NS_ERROR_NOT_IMPLEMENTED;}
	};


class nsOsmozillaPeer : public nsIOsmozilla , public nsClassInfoMixin
{
public:
	nsOsmozillaPeer(nsOsmozillaInstance * aPlugin);
	virtual ~nsOsmozillaPeer();
	
    // methods from nsISupports
    NS_IMETHOD QueryInterface(const nsIID & aIID, void **aInstancePtr);
    NS_IMETHOD_(nsrefcnt) AddRef();
    NS_IMETHOD_(nsrefcnt) Release();
	
public:
	NS_DECL_NSIOSMOZILLA
	void SetInstance(nsOsmozillaInstance * plugin);
	
protected:
  	nsrefcnt mRefCnt; 
    nsOsmozillaInstance * mPlugin;
};


#endif // __PLUGIN_H__
