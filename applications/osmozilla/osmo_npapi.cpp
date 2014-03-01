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


////////////////////////////////////////////////////////////
//
// Implementation of Netscape entry points (NPN_*)
//
#include "osmo_npapi.h"
#include "osmozilla.h"

#if defined(XP_UNIX) && !defined(XP_MACOS) 
#include <malloc.h>
#include <string.h>
#endif

NPNetscapeFuncs *sBrowserFunctions = NULL;

NPError Osmozilla_GetURL(NPP instance, const char *url, const char *target)
{
	if (!sBrowserFunctions) return NPERR_INVALID_FUNCTABLE_ERROR;
	return sBrowserFunctions->geturl(instance, url, target);
}

void Osmozilla_SetStatus(NPP instance, const char *message)
{
	if (!sBrowserFunctions) return;
	sBrowserFunctions->status(instance, message);
}

#ifndef GECKO_XPCOM
void Osmozilla_InitScripting(Osmozilla *osmo);
#endif

NPError NPOsmozilla_New(NPMIMEType pluginType, NPP instance, uint16_t mode, int16_t argc, char* argn[], char* argv[], NPSavedData* saved)
{   
	Osmozilla *osmo;
	NPError rv = NPERR_NO_ERROR;
	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	osmo = (Osmozilla *) malloc(sizeof(Osmozilla));
	memset(osmo, 0, sizeof(Osmozilla));

	osmo->np_instance = instance;
	// associate the plugin instance object with NPP instance
	instance->pdata = (void *)osmo;

	osmo->supports_xembed = 0;
	sBrowserFunctions->getvalue(NULL, NPNVSupportsXEmbedBool, (void *)&osmo->supports_xembed);

	Osmozilla_Initialize(osmo, argc, argn, argv);

#ifndef GECKO_XPCOM
	Osmozilla_InitScripting(osmo);
#endif

	return rv;
}

// here is the place to clean up and destroy the nsPluginInstance object
NPError NPOsmozilla_Destroy (NPP instance, NPSavedData** save)
{
	NPError rv = NPERR_NO_ERROR;
	Osmozilla *osmozilla;
	if(instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	osmozilla = (Osmozilla*)instance->pdata;
	if (osmozilla != NULL) {
		Osmozilla_Shutdown(osmozilla);
#ifdef GECKO_XPCOM
		NPOsmozilla_ShutdownScript(osmozilla);
#else
		if (osmozilla->script_obj) sBrowserFunctions->releaseobject(osmozilla->script_obj);
		osmozilla->script_obj = NULL;
#endif

		free(osmozilla);
	}
	instance->pdata = NULL;
	return rv;
}

// during this call we know when the plugin window is ready or
// is about to be destroyed so we can do some gui specific
// initialization and shutdown
NPError NPOsmozilla_SetWindow (NPP instance, NPWindow* pNPWindow)
{    
	Osmozilla *osmozilla;
	void *os_wnd_handle, *os_wnd_display;
	NPError rv = NPERR_NO_ERROR;

	if (!instance || !instance->pdata) return NPERR_INVALID_INSTANCE_ERROR;
	if (pNPWindow == NULL) return NPERR_GENERIC_ERROR;
	osmozilla = (Osmozilla *)instance->pdata;

	// window just created
	if (!osmozilla->window_set) {
		if (pNPWindow->window == NULL) return NPERR_GENERIC_ERROR;

#ifdef XP_WIN
		os_wnd_handle = pNPWindow->window;
		os_wnd_display = NULL;
#elif defined(XP_MAXOS)
		os_wnd_handle = pNPWindow->window;
		os_wnd_display = NULL;
#elif defined(XP_UNIX)
		os_wnd_handle = pNPWindow->window;
		/*HACK - although we don't use the display in the X11 plugin, this is used to signal that 
		the user is mozilla and prevent some X11 calls crashing the browser in file playing mode 
		(eg, "firefox myfile.mp4" )*/
		os_wnd_display =((NPSetWindowCallbackStruct *)pNPWindow->ws_info)->display;
#endif

		if (! Osmozilla_SetWindow(osmozilla, os_wnd_handle, os_wnd_display, pNPWindow->width, pNPWindow->height) ) {
			return NPERR_MODULE_LOAD_FAILED_ERROR;
		}

	}

#if 0
	// window goes away
	if((pNPWindow->window == NULL) && plugin->isInitialized())
		return plugin->SetWindow(pNPWindow);

	// window resized?
	if(plugin->isInitialized() && (pNPWindow->window != NULL))
		return plugin->SetWindow(pNPWindow);

	// this should not happen, nothing to do
	if((pNPWindow->window == NULL) && !plugin->isInitialized())
		return plugin->SetWindow(pNPWindow);
#endif

	return rv;
}

NPError NPOsmozilla_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16_t *stype)
{
	Osmozilla *osmozilla;
	if(instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	osmozilla = (Osmozilla *)instance->pdata;
	if(osmozilla== NULL) 
		return NPERR_GENERIC_ERROR;

	Osmozilla_ConnectTo(osmozilla, stream->url);
	*stype = NP_SEEK;
	return NPERR_NO_ERROR;
}

NPINT32 NPOsmozilla_WriteReady (NPP instance, NPStream *stream)
{
	return 0x0fffffff;
}

NPINT32 NPOsmozilla_Write (NPP instance, NPStream *stream, NPINT32 offset, NPINT32 len, void *buffer)
{   
	return len;
}

NPError NPOsmozilla_DestroyStream (NPP instance, NPStream *stream, NPError reason)
{
	return NPERR_NO_ERROR;
}

void NPOsmozilla_StreamAsFile (NPP instance, NPStream* stream, const char* fname)
{
}

void NPOsmozilla_Print (NPP instance, NPPrint* printInfo)
{
	Osmozilla *osmozilla;
	if(instance == NULL)
		return;

	osmozilla = (Osmozilla *)instance->pdata;
	if(osmozilla== NULL) 
		return;

	Osmozilla_Print(osmozilla, (printInfo->mode == NP_EMBED) ? 1 : 0, printInfo->print.embedPrint.platformPrint, 
		printInfo->print.embedPrint.window.x, printInfo->print.embedPrint.window.y,
		printInfo->print.embedPrint.window.width, printInfo->print.embedPrint.window.height);
}

void NPOsmozilla_URLNotify(NPP instance, const char* url, NPReason reason, void* notifyData)
{
	return;
}

NPError	NPOsmozilla_GetValue(NPP instance, NPPVariable variable, void *value)
{
	NPError rv = NPERR_NO_ERROR;
	Osmozilla *osmozilla;
	if(instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	osmozilla = (Osmozilla *)instance->pdata;
	if(osmozilla== NULL) return NPERR_GENERIC_ERROR;

	switch (variable) {
#ifdef GECKO_XPCOM
	case NPPVpluginScriptableInstance:
		rv = NPOsmozilla_GetPeer(osmozilla, value);
		break;	

	case NPPVpluginScriptableIID:
		rv = NPOsmozilla_GetPeerIID(osmozilla, value);
		break;
#else

	case NPPVpluginScriptableNPObject:
		sBrowserFunctions->retainobject(osmozilla->script_obj);
		* (void **)value = osmozilla->script_obj;
		break;

#endif
	case NPPVpluginNeedsXEmbed:
		*((int *)value) = 1;
		break;
	case NPPVpluginNameString :
		*(const char**)value = "Osmozilla/GPAC plugin for NPAPI";
		break;
	default:
		break;
	}

	return rv;
}



NPError NPOsmozilla_SetValue(NPP instance, NPNVariable variable, void *value)
{
	return NPERR_NO_ERROR;
}

int16_t	NPOsmozilla_HandleEvent(NPP instance, void* event)
{
	/*we hacked the proc*/
	return 0;
}


NPError OSCALL NP_Shutdown()
{
#ifdef GECKO_XPCOM
	NPOsmozilla_ReleaseServiceManager();
#endif
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
	aNPPFuncs->newp          = NPOsmozilla_New;
	aNPPFuncs->destroy       = NPOsmozilla_Destroy;
	aNPPFuncs->setwindow     = NPOsmozilla_SetWindow;
	aNPPFuncs->newstream     = NPOsmozilla_NewStream;
	aNPPFuncs->destroystream = NPOsmozilla_DestroyStream;
	aNPPFuncs->asfile        = NPOsmozilla_StreamAsFile;
	aNPPFuncs->writeready    = NPOsmozilla_WriteReady;
	aNPPFuncs->write         = NPOsmozilla_Write;
	aNPPFuncs->print         = NPOsmozilla_Print;
	aNPPFuncs->event         = NPOsmozilla_HandleEvent;
	aNPPFuncs->urlnotify     = NPOsmozilla_URLNotify;
	aNPPFuncs->getvalue      = NPOsmozilla_GetValue;
	aNPPFuncs->setvalue      = NPOsmozilla_SetValue;
	return NPERR_NO_ERROR;
}


static NPError NS_PluginInitialize()
{
#ifdef GECKO_XPCOM
	NPOsmozilla_GetServiceManager();
#endif
	return NPERR_NO_ERROR;
}

// get values per plugin
NPError NS_PluginGetValue(NPPVariable aVariable, void *aValue)
{
	NPError err = NPERR_NO_ERROR;
	switch (aVariable) {
	case NPPVpluginNameString:
		*((char **)aValue) = (char *) "Osmozilla";
		break;
	case NPPVpluginDescriptionString:
		*((char **)aValue) = Osmozilla_GetVersion();
		break;
	default:
		err = NPERR_INVALID_PARAM;
		break;
	}
	return err;
}




#define GPAC_PLUGIN_MIMETYPES \
	"audio/mpeg:mp2,mp3,mpga,mpega:MP3 Music;" \
	"audio/x-mpeg:mp2,mp3,mpga,mpega:MP3 Music;" \
	"audio/amr:amr,awb:AMR Audio;" \
	"audio/mp4:mp4,mpg4,mpeg4,m4a:MPEG-4 Audio;" \
	"audio/aac:aac:MPEG-4 AAC Music;" \
	"audio/aacp:aac:MPEG-4 AACPlus Music;" \
	"audio/basic:snd,au:Basic Audio;"	\
	"audio/x-wav:wav:WAV Audio;"	\
	"audio/3gpp:3gp,3gpp:3GPP/MMS Music;"	\
	"audio/3gpp2:3g2,3gp2:3GPP2/MMS Music;"	\
	"video/mpeg:mpg,mpeg,mpe,mpv2:MPEG Video;" \
	"video/x-mpeg:mpg,mpeg,mpe,mpv2:MPEG Video;" \
	"video/mpeg-system:mpg,mpeg,mpe,vob,mpv2:MPEG Video;" \
	"video/x-mpeg-system:mpg,mpeg,mpe,vob,mpv2:MPEG Video;" \
	"video/avi:avi:AVI Video;" \
	"video/quicktime:mov,qt:QuickTime Movies;" \
	"video/x-ms-asf:asf,asx:Windows Media Video;" \
	"video/x-ms-wmv:wmv:Windows Media;" \
	"video/mp4:mp4,mpg4:MPEG-4 Video;" \
	"video/3gpp:3gp,3gpp:3GPP/MMS Video;" \
	"video/3gpp2:3g2,3gp2:3GPP2/MMS Video;" \
	"image/jpeg:jpeg,jpg:JPEG Images;"	\
	"image/png:png:PNG Images;"	\
	"image/bmp:bmp:MS Bitmap Images;"	\
	"image/svg+xml:svg,svg.gz,svgz:SVG Document;"	\
	"image/x-svgm:svgm:SVGM Document;"	\
	"x-subtitle/srt:srt:SRT SubTitles;"	\
	"x-subtitle/sub:sub:SUB SubTitles;"	\
	"x-subtitle/ttxt:ttxt:GPAC 3GPP TimedText;"	\
	"model/vrml:wrl,wrl.gz:VRML World;"	\
	"model/x3d+vrml:x3dv,x3dv.gz,x3dvz:X3D/VRML World;"	\
	"model/x3d+xml:x3d,x3d.gz,x3dz:X3D/XML World;" \
	"application/ogg:ogg:Ogg Media;" \
	"application/x-ogg:ogg:Ogg Media;" \
	"application/x-bt:bt,bt.gz,btz:MPEG-4 Text (BT);"	\
	"application/x-xmt:xmt,xmt.gz,xmtz:MPEG-4 Text (XMT);"	\
	"application/mp4:mp4,mpg4:MPEG-4 Movies;" \
	"application/sdp:sdp:Streaming Media Session;" \
	/* explicit plugin call */ \
	"application/x-gpac::GPAC plugin;" \

char * NP_GetMIMEDescription(void)
{
	return (char *) GPAC_PLUGIN_MIMETYPES;
}


NPError NP_GetValue(void *future, NPPVariable aVariable, void *aValue)
{
	return NS_PluginGetValue(aVariable, aValue);
}


#if defined(XP_WIN) || defined(XP_MACOS) 

NPError OSCALL NP_Initialize(NPNetscapeFuncs* aNPNFuncs)
{
	sBrowserFunctions = aNPNFuncs;

	return NS_PluginInitialize();
}

NPError OSCALL NP_GetEntryPoints(NPPluginFuncs* aNPPFuncs)
{
	return fillPluginFunctionTable(aNPPFuncs);
}


#elif defined(XP_UNIX)

NPError OSCALL NP_Initialize(NPNetscapeFuncs* aNPNFuncs, NPPluginFuncs* aNPPFuncs)
{
	NPError rv;
	sBrowserFunctions = aNPNFuncs;
	rv = fillPluginFunctionTable(aNPPFuncs);
	if(rv != NPERR_NO_ERROR)
		return rv;

	return NS_PluginInitialize();
}
#endif


#ifdef GECKO_XPCOM


#include <nsIServiceManager.h>
#include <nsIMemory.h>
#include <nsISupportsUtils.h>
#include <nsISupports.h>
#include <nsMemory.h>

#include "nsIOsmozilla.h"

#include "osmozilla.h"


nsIServiceManager *gServiceManager = NULL;


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
	nsOsmozillaPeer(Osmozilla *osmo);
	virtual ~nsOsmozillaPeer();

	// methods from nsISupports
	NS_IMETHOD QueryInterface(const nsIID & aIID, void **aInstancePtr);
	NS_IMETHOD_(nsrefcnt) AddRef();
	NS_IMETHOD_(nsrefcnt) Release();

public:
	NS_DECL_NSIOSMOZILLA
		void SetInstance(Osmozilla *osmo);

protected:
	nsrefcnt mRefCnt; 
	Osmozilla *mPlugin;
};


static NS_DEFINE_IID(kIZillaPluginIID, NS_IOSMOZILLA_IID);
static NS_DEFINE_IID(kIClassInfoIID, NS_ICLASSINFO_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsOsmozillaPeer::nsOsmozillaPeer(Osmozilla *osmo)
{
	mPlugin=osmo;
	mRefCnt = 0;
}

nsOsmozillaPeer::~nsOsmozillaPeer()
{
}
// Notice that we expose our claim to implement nsIClassInfo.
//NS_IMPL_ISUPPORTS2(nsOsmozillaPeer, nsITestPlugin, nsIClassInfo)

// the following method will be callable from JavaScript
NS_IMETHODIMP nsOsmozillaPeer::Pause() { 
	Osmozilla_Pause(mPlugin); 
	return NS_OK; 
}
NS_IMETHODIMP nsOsmozillaPeer::Play() { 
	Osmozilla_Play(mPlugin); 
	return NS_OK; 
}
NS_IMETHODIMP nsOsmozillaPeer::Stop() { 
	Osmozilla_Stop(mPlugin); 
	return NS_OK; 
}

NS_IMETHODIMP nsOsmozillaPeer::Update(const char *type, const char *commands) 
{
	Osmozilla_Update(mPlugin, type, commands); 
	return NS_OK; 
}

NS_IMETHODIMP nsOsmozillaPeer::QualitySwitch(int switch_up) 
{
	Osmozilla_QualitySwitch(mPlugin, switch_up); 
	return NS_OK; 
}

NS_IMETHODIMP nsOsmozillaPeer::SetURL(const char *url) 
{
	Osmozilla_SetURL(mPlugin, url); 
	return NS_OK; 
}

void nsOsmozillaPeer::SetInstance(Osmozilla *osmo)
{
	mPlugin = osmo;
}

NS_IMETHODIMP_(nsrefcnt) nsOsmozillaPeer::AddRef()
{
	++mRefCnt;
	return mRefCnt;
}

NS_IMETHODIMP_(nsrefcnt) nsOsmozillaPeer::Release()
{
	--mRefCnt;
	if (mRefCnt == 0) {
		delete this;
		return 0;
	}
	return mRefCnt;
}

// here nsOsmozillaPeer should return three interfaces it can be asked for by their iid's
// static casts are necessary to ensure that correct pointer is returned
NS_IMETHODIMP nsOsmozillaPeer::QueryInterface(const nsIID & aIID,
											  void **aInstancePtr)
{
	if (!aInstancePtr)
		return NS_ERROR_NULL_POINTER;

	if (aIID.Equals(kIZillaPluginIID)) {
		*aInstancePtr = NS_STATIC_CAST(nsIOsmozilla *, this);
		AddRef();
		return NS_OK;
	}

	if (aIID.Equals(kIClassInfoIID)) {
		*aInstancePtr = NS_STATIC_CAST(nsIClassInfo *, this);
		AddRef();
		return NS_OK;
	}

	if (aIID.Equals(kISupportsIID)) {
		*aInstancePtr = NS_STATIC_CAST(nsISupports *, (NS_STATIC_CAST (nsIOsmozilla *, this)));
		AddRef();
		return NS_OK;
	}
	return NS_NOINTERFACE;
}

extern NPNetscapeFuncs *sBrowserFunctions;

void NPOsmozilla_GetServiceManager()
{
	// this is probably a good place to get the service manager
	// note that Mozilla will add reference, so do not forget to release
	nsISupports *sm = NULL;

	if (!sBrowserFunctions) return;
	sBrowserFunctions->getvalue(NULL, NPNVserviceManager, &sm);

	// Mozilla returns nsIServiceManager so we can use it directly; doing QI on
	// nsISupports here can still be more appropriate in case something is changed
	// in the future so we don't need to do casting of any sort.
	if (sm) {
		sm->QueryInterface(NS_GET_IID(nsIServiceManager), (void **) &gServiceManager);
		NS_RELEASE(sm);
	}
}

void NPOsmozilla_ReleaseServiceManager()
{
#ifdef GECKO_XPCOM
	// we should release the service manager
	NS_IF_RELEASE(gServiceManager);
	gServiceManager = NULL;
#endif
}

void NPOsmozilla_ShutdownScript(Osmozilla *osmo)
{
	nsOsmozillaPeer *peer = (nsOsmozillaPeer *) osmo->scriptable_peer;
	if (peer != NULL) {
		peer->SetInstance(NULL);
		NS_IF_RELEASE(peer);
	}
}


NPError	NPOsmozilla_GetPeer(Osmozilla *osmo, void *value)
{
	if (!osmo->scriptable_peer) {
		osmo->scriptable_peer = new nsOsmozillaPeer(osmo);
		if (!osmo->scriptable_peer) return NPERR_OUT_OF_MEMORY_ERROR;
		NS_ADDREF( (nsOsmozillaPeer *) osmo->scriptable_peer);
	}

	NS_ADDREF( (nsOsmozillaPeer *)osmo->scriptable_peer);
	*(nsISupports **) value = (nsISupports *) osmo->scriptable_peer;
	return NPERR_NO_ERROR;
}

NPError	NPOsmozilla_GetPeerIID(Osmozilla *osmo, void *value)
{
	static nsIID scriptableIID = NS_IOSMOZILLA_IID;
	if (!sBrowserFunctions) return NPERR_OUT_OF_MEMORY_ERROR;

	nsIID *ptr = (nsIID *) sBrowserFunctions->memalloc( sizeof(nsIID) );
	if (! ptr) return NPERR_OUT_OF_MEMORY_ERROR;

	*ptr = scriptableIID;
	*(nsIID **) value = ptr;
	return NPERR_NO_ERROR;
}

#else

enum
{
	kOSMOZILLA_ID_METHOD_PLAY = 0,
	kOSMOZILLA_ID_METHOD_PAUSE,
	kOSMOZILLA_ID_METHOD_STOP,
	kOSMOZILLA_ID_METHOD_UPDATE,
	kOSMOZILLA_ID_METHOD_QUALITY_SWITCH,
	kOSMOZILLA_ID_METHOD_SET_URL,
	
	kOSMOZILLA_NUM_METHODS
};

NPIdentifier    v_OSMOZILLA_MethodIdentifiers[kOSMOZILLA_NUM_METHODS];
const NPUTF8 *  v_OSMOZILLA_MethodNames[kOSMOZILLA_NUM_METHODS] = {
	"Play",
	"Pause",
	"Stop",
	"Update",
	"QualitySwitch",
	"SetURL",
};

NPClass osmozilla_script_class;

typedef struct {
	NPClass *_class;
	uint32_t referenceCount;
	Osmozilla *osmo;
} OsmozillaObject;

NPObject *OSMOZILLA_Allocate(NPP npp, NPClass *theClass)
{
	OsmozillaObject *obj = NULL;

	sBrowserFunctions->getstringidentifiers(v_OSMOZILLA_MethodNames, kOSMOZILLA_NUM_METHODS, v_OSMOZILLA_MethodIdentifiers);
	obj = (OsmozillaObject *)malloc(sizeof(OsmozillaObject));
	obj->osmo = (Osmozilla *) npp->pdata;
	return (NPObject *)obj;
}

void OSMOZILLA_Deallocate(NPObject* obj)
{
	free(obj);
	return;
}

void OSMOZILLA_Invalidate(NPObject* obj)
{
	return;
}

bool OSMOZILLA_HasMethod(NPObject* obj, NPIdentifier name)
{
	int i = 0;
	while (i < kOSMOZILLA_NUM_METHODS) {
		if ( name == v_OSMOZILLA_MethodIdentifiers[i] ) {
			return 1;
		}
		i++;
	}
	return 0;
}

bool OSMOZILLA_Invoke(NPObject* obj, NPIdentifier name, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
	OsmozillaObject *npo = (OsmozillaObject *)obj;
	if (!npo->osmo) return 0;
	if (name == v_OSMOZILLA_MethodIdentifiers[kOSMOZILLA_ID_METHOD_PLAY]) {
		Osmozilla_Play(npo->osmo);
		return 1;
	}
	if (name == v_OSMOZILLA_MethodIdentifiers[kOSMOZILLA_ID_METHOD_PAUSE]) {
		Osmozilla_Pause(npo->osmo);
		return 1;
	}
	if (name == v_OSMOZILLA_MethodIdentifiers[kOSMOZILLA_ID_METHOD_STOP]) {
		Osmozilla_Stop(npo->osmo);
		return 1;
	}
	if (name == v_OSMOZILLA_MethodIdentifiers[kOSMOZILLA_ID_METHOD_UPDATE]) {
		const char *mime = NULL;
		const char *update = NULL;
		if (argCount==2) {
			mime = (args[0].type==NPVariantType_String) ? args[0].value.stringValue.UTF8Characters : NULL; 
			update = (args[1].type==NPVariantType_String) ? args[1].value.stringValue.UTF8Characters : NULL; 
		}
		if (!update) return 0;
		Osmozilla_Update(npo->osmo, mime, update);
		return 1;
	}
	if (name == v_OSMOZILLA_MethodIdentifiers[kOSMOZILLA_ID_METHOD_QUALITY_SWITCH]) {
		int up = 1;
		if (argCount==1) {
			if (args[0].type==NPVariantType_Bool) up = args[0].value.boolValue ? 1 : 0;
			else if (args[0].type==NPVariantType_Int32) up = args[0].value.intValue ? 1 : 0;
		}
		Osmozilla_QualitySwitch(npo->osmo, up);
		return 1;
	}
	if (name == v_OSMOZILLA_MethodIdentifiers[kOSMOZILLA_ID_METHOD_SET_URL]) {
		const char *url = "";
		if (argCount>=1) {
			if (args[0].type==NPVariantType_String) 
				url = args[0].value.stringValue.UTF8Characters;
		}
		Osmozilla_SetURL(npo->osmo, url);
		return 1;
	}
	return 0;
}

bool OSMOZILLA_InvokeDefault(NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
	return 1;
}

bool OSMOZILLA_HasProperty(NPObject* obj, NPIdentifier name)
{
	bool result = 0;
	if ( sBrowserFunctions->identifierisstring(name) )
	{
		NPUTF8 *val = sBrowserFunctions->utf8fromidentifier(name);

		if ( !strcmp(val, "DownloadProgress") )
		{
			result = 1;
		}

		sBrowserFunctions->memfree(val);
	}
	/*nothing exposed yet*/
	return result;
}

bool OSMOZILLA_GetProperty(NPObject* obj, NPIdentifier name, NPVariant* result)
{
	OsmozillaObject *npo = (OsmozillaObject *)obj;
	if (!npo->osmo) return 0;
	if ( sBrowserFunctions->identifierisstring(name) )
	{
		NPUTF8 *val = sBrowserFunctions->utf8fromidentifier(name);

		if ( !strcmp(val, "DownloadProgress") )
		{
			int val = Osmozilla_GetDownloadProgress(npo->osmo);
			INT32_TO_NPVARIANT(val, *result);
		}

		sBrowserFunctions->memfree(val);
	}
	return 1;
}

bool OSMOZILLA_SetProperty(NPObject *obj, NPIdentifier name, const NPVariant *value)
{
	return 1;
}

bool OSMOZILLA_RemoveProperty(NPObject *npobj, NPIdentifier name)
{
	return 1;
}

bool OSMOZILLA_Enumerate(NPObject *npobj, NPIdentifier **value, uint32_t *count)
{
	return 1;
}

void Osmozilla_InitScripting(Osmozilla *osmo)
{
	osmozilla_script_class.allocate          = OSMOZILLA_Allocate;
	osmozilla_script_class.deallocate        = OSMOZILLA_Deallocate;
	osmozilla_script_class.invalidate        = OSMOZILLA_Invalidate;
	osmozilla_script_class.hasMethod         = OSMOZILLA_HasMethod;
	osmozilla_script_class.invoke            = OSMOZILLA_Invoke;
	osmozilla_script_class.invokeDefault     = OSMOZILLA_InvokeDefault;
	osmozilla_script_class.hasProperty       = OSMOZILLA_HasProperty;
	osmozilla_script_class.getProperty       = OSMOZILLA_GetProperty;
	osmozilla_script_class.setProperty       = OSMOZILLA_SetProperty;
	osmozilla_script_class.removeProperty    = OSMOZILLA_RemoveProperty;
	osmozilla_script_class.enumerate         = OSMOZILLA_Enumerate;

	/*create script object*/
	osmo->script_obj = sBrowserFunctions->createobject(osmo->np_instance, &osmozilla_script_class);

}

#endif //GECKO_XPCOM

