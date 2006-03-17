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

#include <nsIServiceManager.h>
#include <nsIMemory.h>
#include <nsISupportsUtils.h>
#include <nsISupports.h>
#include <nsMemory.h>

#include "osmozilla.h"

#include <gpac/options.h>

nsIServiceManager *gServiceManager = NULL;


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


char* NPP_GetMIMEDescription(void)
{
	return GPAC_PLUGIN_MIMETYPES;
}

/////////////////////////////////////
// general initialization and shutdown
//
NPError NS_PluginInitialize()
{
    // this is probably a good place to get the service manager
    // note that Mozilla will add reference, so do not forget to release
    nsISupports *sm = NULL;

    NPN_GetValue(NULL, NPNVserviceManager, &sm);

    // Mozilla returns nsIServiceManager so we can use it directly; doing QI on
    // nsISupports here can still be more appropriate in case something is changed
    // in the future so we don't need to do casting of any sort.
    if (sm) {
		sm->QueryInterface(NS_GET_IID(nsIServiceManager), (void **) &gServiceManager);
		NS_RELEASE(sm);
    }
    return NPERR_NO_ERROR;
}

void NS_PluginShutdown()
{
    // we should release the service manager
    NS_IF_RELEASE(gServiceManager);
    gServiceManager = NULL;
}

// get values per plugin
NPError NS_PluginGetValue(NPPVariable aVariable, void *aValue)
{
	NPError err = NPERR_NO_ERROR;
	switch (aVariable) {
	case NPPVpluginNameString:
		*((char **)aValue) = "Osmozilla";
		break;
	case NPPVpluginDescriptionString:
		*((char **)aValue) = "GPAC Plugin for Mozilla. For more information go to <a href=\"http://gpac.sourceforge.net\">GPAC website</a>";
		break;
	default:
		err = NPERR_INVALID_PARAM;
		break;
	}
	return err;
}

/////////////////////////////////////////////////////////////
//
// construction and destruction of our plugin instance object
//
nsPluginInstanceBase * NS_NewPluginInstance(nsPluginCreateData * aCreateDataStruct)
{
	if(!aCreateDataStruct) return NULL;
	nsOsmozillaInstance * plugin = new nsOsmozillaInstance(aCreateDataStruct);
	return plugin;
}

void NS_DestroyPluginInstance(nsPluginInstanceBase * aPlugin)
{
	if(aPlugin) delete (nsOsmozillaInstance *)aPlugin;
}

////////////////////////////////////////
//
// nsOsmozillaInstance class implementation
//
nsOsmozillaInstance::nsOsmozillaInstance(nsPluginCreateData * aCreateDataStruct) : nsPluginInstanceBase(),
  mInstance(aCreateDataStruct->instance)
{
#ifdef XP_UNIX
	mWindow = 0L;
	mFontInfo = NULL;
	mXtwidget = NULL;
#endif

#ifdef XP_WIN
	m_hWnd = NULL;
#endif
	
	mScriptablePeer = NULL;
	mInitialized = 0;

	
	m_szURL = NULL;
	m_term = NULL;
	m_bIsConnected = 0;
	
	
	SetArg(aCreateDataStruct);
}

nsOsmozillaInstance::~nsOsmozillaInstance()
{
    if (mInstance) {
		mInstance->pdata = NULL;
		mInstance = NULL;
	}
    mInitialized = FALSE;
    if (mScriptablePeer != NULL) {
		mScriptablePeer->SetInstance(NULL);
		NS_IF_RELEASE(mScriptablePeer);
    }
}

NPBool nsOsmozillaInstance::init(NPWindow* aWindow)
{	
	unsigned char config_path[GF_MAX_PATH];
	char *gpac_cfg;
	const char *str;
	
	if(aWindow == NULL) return FALSE;

#ifdef XP_WIN
	gpac_cfg = "GPAC.cfg";
#ifdef _DEBUG
//#if 0
	strcpy((char *) config_path, "D:\\CVS\\gpac\\bin\\w32_deb");
#else
	HKEY hKey = NULL;
	DWORD dwSize;
	RegOpenKeyEx(HKEY_CLASSES_ROOT, "GPAC", 0, KEY_READ, &hKey);
	dwSize = GF_MAX_PATH;
	RegQueryValueEx(hKey, "InstallDir", NULL, NULL,(unsigned char*) config_path, &dwSize);
	RegCloseKey(hKey);
#endif

#endif	/*XP_WIN*/

#ifdef XP_UNIX
	gpac_cfg = ".gpacrc";
	strcpy((char *) config_path, getenv("HOME"));
#endif
	
	memset(&m_user, 0, sizeof(m_user));
	m_user.config = gf_cfg_new((const char *) config_path, gpac_cfg);
	/*need to have a valid cfg file for now*/
	if (!m_user.config) goto err_exit;

	str = gf_cfg_get_key(m_user.config, "General", "ModulesDirectory");
	m_user.modules = gf_modules_new((const unsigned char *) str, m_user.config);
	if (!gf_modules_get_count(m_user.modules)) goto err_exit;

	m_user.opaque = this;
	
	if (SetWindow(aWindow)) mInitialized = TRUE;
	return mInitialized;

err_exit:

#ifdef WIN32
	MessageBox(NULL, "GPAC CONFIGURATION FILE NOT FOUND OR INVALID - PLEASE LAUNCH OSMO4 FIRST", "OSMOZILLA FATAL ERROR", MB_OK);
#else
	fprintf(stdout, "OSMOZILLA FATAL ERROR\nGPAC CONFIGURATION FILE NOT FOUND OR INVALID\nPLEASE LAUNCH OSMO4 or MP4Client FIRST\n");
#endif
	if (m_user.modules) gf_modules_del(m_user.modules);
	m_user.modules = NULL;
	if (m_user.config) gf_cfg_del(m_user.config);
	m_user.config = NULL;
	return FALSE;
}

void nsOsmozillaInstance::shut()
{
	if (m_szURL) free(m_szURL);
	m_szURL = NULL;
	if (m_term) {
		GF_Terminal *a_term = m_term;
		m_term = NULL;
		gf_term_del(a_term);
	}
	if (m_user.modules) gf_modules_del(m_user.modules);
	if (m_user.config) gf_cfg_del(m_user.config);
	memset(&m_user, 0, sizeof(m_user));
}

const char * nsOsmozillaInstance::getVersion()
{
  return NPN_UserAgent(mInstance);
}

NPError nsOsmozillaInstance::GetValue(NPPVariable aVariable, void *aValue)
{
  NPError rv = NPERR_NO_ERROR;

    switch (aVariable) {
    case NPPVpluginScriptableInstance:
	{
		nsIOsmozilla *scriptablePeer = getScriptablePeer();
	    if (scriptablePeer) {
			*(nsISupports **) aValue = scriptablePeer;
		} else
			rv = NPERR_OUT_OF_MEMORY_ERROR;
	}
		break;	


    case NPPVpluginScriptableIID:
	{
	    static nsIID scriptableIID = NS_IOSMOZILLA_IID;
	    nsIID *ptr = (nsIID *) NPN_MemAlloc(sizeof(nsIID));
	    if (ptr) {
			*ptr = scriptableIID;
			*(nsIID **) aValue = ptr;
		} else
			rv = NPERR_OUT_OF_MEMORY_ERROR;
	}
		break;

    default:
		break;
    }
    return rv;
}

Bool Osmozilla_EventProc(void *priv, GF_Event *evt)
{
	nsOsmozillaInstance *gpac = (nsOsmozillaInstance *) priv;
	if (!gpac->m_term) return 0;
	
	switch (evt->type) {
	case GF_EVT_MESSAGE:
	/*FIXME !!!*/
#if 0
	{	
		const char *servName = "main service";
		if (!evt->message.message) return 0;
		if (evt->message.error) 
			fprintf(stdout, "plugin :%s (%s): %s\n", evt->message.message, servName, gf_error_to_string(evt->message.error));
		/*file download*/
		else if (strstr(evt->message.message, "Download") || strstr(evt->message.message, "Buffering") || strstr(evt->message.message, "Importing")) 
			fprintf(stdout, "plugin :%s (%s)\r", evt->message.message, servName);
		else
			fprintf(stdout, "plugin :%s (%s)\n", evt->message.message, servName);
	}
#endif
		break;

	case GF_EVT_SIZE:	
    gf_term_set_size(gpac->m_term, gpac->m_width, gpac->m_height);
		break;
	case GF_EVT_CONNECT:	
		gpac->m_bIsConnected = 1;
		/*pause upon connection*/
		if (!gpac->m_bAutoStart) {
			gpac->m_bAutoStart = 1;
			gf_term_set_option(gpac->m_term, GF_OPT_PLAY_STATE, GF_STATE_PAUSED);
		}
		break;
	case GF_EVT_DURATION:		
		break;
	case GF_EVT_LDOUBLECLICK:
		gf_term_set_option(gpac->m_term, GF_OPT_FULLSCREEN, !gf_term_get_option(gpac->m_term, GF_OPT_FULLSCREEN));
		break;
	case GF_EVT_VKEYDOWN:
		if ((evt->key.key_states & GF_KM_ALT)) {
	    } else {
			switch (evt->key.vk_code) {
			case GF_VK_HOME:
				gf_term_set_option(gpac->m_term, GF_OPT_NAVIGATION_TYPE, 1);
				break;
			case GF_VK_ESCAPE:
				gf_term_set_option(gpac->m_term, GF_OPT_FULLSCREEN, !gf_term_get_option(gpac->m_term, GF_OPT_FULLSCREEN));
				break;
			}
    }
    break;
	}
	return 0;
}


NPError nsOsmozillaInstance::SetWindow(NPWindow* aWindow)
{
	if (mInitialized) {
    m_width = aWindow->width;
    m_height = aWindow->height;
    if (m_bIsConnected) gf_term_set_size(m_term, m_width, m_height);
    return TRUE;
  }
	if(aWindow == NULL) return FALSE;

	m_width = aWindow->width;
	m_height = aWindow->height;

	m_user.EventProc = Osmozilla_EventProc;
	
#ifdef XP_WIN
	m_user.os_window_handler = aWindow->window;
#endif

#ifdef XP_UNIX
  m_user.os_window_handler = aWindow->window;
  /*HACK - although we don't use the display in the X11 plugin, this is 
used to signal that the user is mozilla and prevent some X11 calls crashing the browser in file playing mode (eg, "firefox myfile.mp4" )*/
  m_user.os_display =((NPSetWindowCallbackStruct *)aWindow->ws_info)->display;
  XSynchronize((Display *) m_user.os_display, True);
  m_user.os_window_handler = aWindow->window;
#endif

	gf_cfg_set_key(m_user.config, "Rendering", "RendererName", m_bForce3D ? "GPAC 3D Renderer" : "GPAC 2D Renderer");
	m_bForce3D = 0;
	m_term = gf_term_new(&m_user);
	if (! m_term) return NPERR_GENERIC_ERROR;

	m_prev_time = 0;
	SetOptions();
	m_url_changed = 0;
	mInitialized = TRUE;

	if (!m_szURL) return TRUE;

	/*all protocols not handled by mozilla are just ignored, so we get them from the option list
	and forget about mozilla...*/
	if (!strnicmp(m_szURL, "rtsp://", 7) 
		|| !strnicmp(m_szURL, "rtp://", 6) 
		) {

		if (m_bAutoStart) 
			gf_term_connect(m_term, (const char *) m_szURL);
	} else {
		free(m_szURL);
		m_szURL = NULL;
	}
	return TRUE;
}

void nsOsmozillaInstance::SetOptions()
{
	const char *sOpt = gf_cfg_get_key(m_user.config, "General", "Loop");
	m_Loop = (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0;
	m_bAutoStart = 1;
	m_bForce3D = 0;		

	/*options sent from plugin*/
	for(int i=0;i<m_argc;i++) {   
		if (!stricmp(m_argn[i],"Autostart") && !stricmp(m_argv[i], "False")) 
			m_bAutoStart = 0;

		else if (!stricmp(m_argn[i],"src") ) {
			m_szURL = strdup(m_argv[i]);
		}
		else if (!stricmp(m_argn[i],"use3d") && !stricmp(m_argv[i], "true") ) {
			m_bForce3D = 1;
		}
		else if (!stricmp(m_argn[i],"AspectRatio")) {
			u32 ar = GF_ASPECT_RATIO_KEEP;
			if (!stricmp(m_argv[i], "Keep")) ar = GF_ASPECT_RATIO_KEEP;
			else if (!stricmp(m_argv[i], "16:9")) ar = GF_ASPECT_RATIO_16_9;
			else if (!stricmp(m_argv[i], "4:3")) ar = GF_ASPECT_RATIO_4_3;
			else if (!stricmp(m_argv[i], "Fill")) ar = GF_ASPECT_RATIO_FILL_SCREEN;
			gf_term_set_option(m_term, GF_OPT_ASPECT_RATIO, ar);
		}
	}
}

NPError nsOsmozillaInstance::NewStream(NPMIMEType type, NPStream * stream,
				    NPBool seekable, uint16 * stype)
{
	m_szURL = strdup((const char *)stream->url);

	Bool reload = 0;
	const char *rend_name = gf_cfg_get_key(m_user.config, "Rendering", "RendererName");
	if (m_bForce3D) {
		if (strstr(rend_name, "2D")) {
			gf_cfg_set_key(m_user.config, "Rendering", "RendererName", "GPAC 3D Renderer");
			reload = 1;
		}
		m_bForce3D = 0;
	} else {
		if (strstr(rend_name, "3D")) {
			gf_cfg_set_key(m_user.config, "Rendering", "RendererName", "GPAC 2D Renderer");
			reload = 1;
		}
	}
	if (reload) {
		gf_term_del(m_term);
		m_term = gf_term_new(&m_user);
	}
	gf_term_connect(m_term, (const char *) m_szURL);
    *stype = NP_SEEK;
    return NPERR_NO_ERROR;
}

NPError nsOsmozillaInstance::DestroyStream(NPStream * stream, NPError reason)
{
	if (m_szURL) {
		gf_term_disconnect(m_term);
		free(m_szURL);
		m_szURL = NULL;
	}
	return NPERR_NO_ERROR;
}

void nsOsmozillaInstance::StreamAsFile(NPStream* stream, const char* fname)
{
	m_szURL = strdup((const char *)stream->url);
	gf_term_connect(m_term, (const char *) m_szURL);
}

void nsOsmozillaInstance::URLNotify(const char *url, NPReason reason, void *notifyData)
{
}

uint16 nsOsmozillaInstance::HandleEvent(void* event)
{
  return true;
}
 
void nsOsmozillaInstance::SetArg(nsPluginCreateData * aCreateDataStruct)
{
	m_argc=aCreateDataStruct->argc;
	m_argv=aCreateDataStruct->argv;
	m_argn=aCreateDataStruct->argn;
}

void nsOsmozillaInstance::Pause()
{
	gf_term_set_option(m_term, GF_OPT_PLAY_STATE, GF_STATE_PAUSED);
}

void nsOsmozillaInstance::Play()
{
	/*for RTSP & co*/
	if (!m_bIsConnected && !m_bAutoStart) {
		m_bAutoStart = 1;
		gf_term_connect(m_term, (const char *) m_szURL);
	}
	else
		gf_term_set_option(m_term, GF_OPT_PLAY_STATE, GF_STATE_PLAYING);
}

void nsOsmozillaInstance::Reload()
{
	gf_term_reload(m_term);
}


// Scriptability related code

nsOsmozillaPeer *nsOsmozillaInstance::getScriptablePeer()
{
    if (!mScriptablePeer) {
		mScriptablePeer = new nsOsmozillaPeer(this);
		if (!mScriptablePeer) return NULL;
		NS_ADDREF(mScriptablePeer);
    }
    NS_ADDREF(mScriptablePeer);
    return mScriptablePeer;
}

static NS_DEFINE_IID(kIZillaPluginIID, NS_IOSMOZILLA_IID);
static NS_DEFINE_IID(kIClassInfoIID, NS_ICLASSINFO_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsOsmozillaPeer::nsOsmozillaPeer(nsOsmozillaInstance * aPlugin)
{
	mPlugin=aPlugin;
	mRefCnt = 0;
}

nsOsmozillaPeer::~nsOsmozillaPeer()
{
}
	// Notice that we expose our claim to implement nsIClassInfo.
//NS_IMPL_ISUPPORTS2(nsOsmozillaPeer, nsITestPlugin, nsIClassInfo)

	// the following method will be callable from JavaScript
NS_IMETHODIMP nsOsmozillaPeer::Pause()
{
	mPlugin->Pause();
	return NS_OK;
}

NS_IMETHODIMP nsOsmozillaPeer::Play()
{
	mPlugin->Play();
	return NS_OK;
}

NS_IMETHODIMP nsOsmozillaPeer::Reload()
{
	mPlugin->Reload();
	return NS_OK;
}

void nsOsmozillaPeer::SetInstance(nsOsmozillaInstance * plugin)
{
    mPlugin = plugin;
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

