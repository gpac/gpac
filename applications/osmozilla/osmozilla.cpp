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
	
	m_argc=aCreateDataStruct->argc;
	m_argv=aCreateDataStruct->argv;
	m_argn=aCreateDataStruct->argn;
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

Bool nsOsmozillaInstance::EventProc(GF_Event *evt)
{
	char msg[1024];

	if (!m_term) return 0;

	switch (evt->type) {
	case GF_EVT_MESSAGE:
		if (!evt->message.message) return 0;
		if (evt->message.error)
			sprintf((char *)msg, "GPAC: %s (%s)", evt->message.message, gf_error_to_string(evt->message.error));
		else
			sprintf((char *)msg, "GPAC: %s", evt->message.message);

		NPN_Status(mInstance, msg);
		break;
	case GF_EVT_PROGRESS:
		if (evt->progress.done == evt->progress.total) {
			NPN_Status(mInstance, "");
		} else {
			char *szTitle = "";
			if (evt->progress.progress_type==0) szTitle = "Buffer ";
			else if (evt->progress.progress_type==1) szTitle = "Download ";
			else if (evt->progress.progress_type==2) szTitle = "Import ";
			sprintf(msg, "(GPAC) %s: %02.2f", szTitle, (100.0*evt->progress.done) / evt->progress.total);
			NPN_Status(mInstance, msg);
		}
		break;

	/*IGNORE any scene size, just work with the size allocated in the parent doc*/
	case GF_EVT_SCENE_SIZE:	
		gf_term_set_size(m_term, m_width, m_height);
		break;
	/*window has been resized (full-screen plugin), resize*/
	case GF_EVT_SIZE:	
		m_width = evt->size.width;
		m_height = evt->size.height;
		gf_term_set_size(m_term, m_width, m_height);
		break;
	case GF_EVT_CONNECT:	
		m_bIsConnected = evt->connect.is_connected;
		break;
	case GF_EVT_DURATION:	
		m_bCanSeek = evt->duration.can_seek;
		m_Duration = evt->duration.duration;
		break;
	case GF_EVT_LDOUBLECLICK:
		gf_term_set_option(m_term, GF_OPT_FULLSCREEN, !gf_term_get_option(m_term, GF_OPT_FULLSCREEN));
		break;
	case GF_EVT_VKEYDOWN:
		if ((evt->key.key_states & GF_KM_ALT)) {
	    } else {
			switch (evt->key.vk_code) {
			case GF_VK_HOME:
				gf_term_set_option(m_term, GF_OPT_NAVIGATION_TYPE, 1);
				break;
			case GF_VK_ESCAPE:
				gf_term_set_option(m_term, GF_OPT_FULLSCREEN, !gf_term_get_option(m_term, GF_OPT_FULLSCREEN));
				break;
			}
		}
	    break;
	case GF_EVT_NAVIGATE_INFO:
		strcpy(msg, evt->navigate.to_url);
		NPN_Status(mInstance, msg);
		break;
	case GF_EVT_NAVIGATE:
		if (gf_term_is_supported_url(m_term, evt->navigate.to_url, 1, 1)) {
			gf_term_navigate_to(m_term, evt->navigate.to_url);
			return 1;
		} else {
			u32 i;
			char *target = "_self";

			for (i=0; i<evt->navigate.param_count; i++) {
				if (!strcmp(evt->navigate.parameters[i], "_parent")) target = "_parent";
				else if (!strcmp(evt->navigate.parameters[i], "_blank")) target = "_blank";
				else if (!strcmp(evt->navigate.parameters[i], "_top")) target = "_top";
				else if (!strcmp(evt->navigate.parameters[i], "_new")) target = "_new";
				else if (!strnicmp(evt->navigate.parameters[i], "_target=", 8)) target = (char *) evt->navigate.parameters[i]+8;
			}
			NPN_GetURL(mInstance, evt->navigate.to_url, target);
			return 1;
		}
		break;
	}
	return 0;
}

Bool Osmozilla_EventProc(void *priv, GF_Event *evt)
{
	nsOsmozillaInstance *gpac = (nsOsmozillaInstance *) priv;
	return gpac->EventProc(evt);
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
	/*HACK - although we don't use the display in the X11 plugin, this is used to signal that 
	the user is mozilla and prevent some X11 calls crashing the browser in file playing mode 
	(eg, "firefox myfile.mp4" )*/
	m_user.os_display =((NPSetWindowCallbackStruct *)aWindow->ws_info)->display;
	XSynchronize((Display *) m_user.os_display, True);
	m_user.os_window_handler = aWindow->window;
#endif

 	m_prev_time = 0;
	m_url_changed = 0;
	SetOptions();
 
    /*setup 3D mode if requested*/
	if (m_szURL && (strstr(m_szURL, ".wrl") || strstr(m_szURL, ".x3d") || strstr(m_szURL, ".x3dv"))) {
		gf_cfg_set_key(m_user.config, "Rendering", "RendererName", "GPAC 3D Renderer");
	} else {
		gf_cfg_set_key(m_user.config, "Rendering", "RendererName", m_bUse3D ? "GPAC 3D Renderer" : "GPAC 2D Renderer");
	}
	m_term = gf_term_new(&m_user);
	if (! m_term) return NPERR_GENERIC_ERROR;

	mInitialized = TRUE;

	SetFocus((HWND)m_user.os_window_handler);
	
	/*stream not ready*/
	if (!m_szURL) return TRUE;

	/*connect from 0 and pause if not autoplay*/
	gf_term_connect_from_time(m_term, m_szURL, 0, m_bAutoStart ? 0 : 1);
	return TRUE;
}

void nsOsmozillaInstance::SetOptions()
{
	m_bLoop = 0;
	m_bAutoStart = 1;
	m_bUse3D = 0;		

	/*options sent from plugin*/
	for(int i=0;i<m_argc;i++) {   
		if (!stricmp(m_argn[i],"autostart") && (!stricmp(m_argv[i], "false") || !stricmp(m_argv[i], "no")) ) 
			m_bAutoStart = 0;

		else if (!stricmp(m_argn[i],"src") ) {
			if (m_szURL) free(m_szURL);
			m_szURL = strdup(m_argv[i]);
		}
		else if (!stricmp(m_argn[i],"use3d") && (!stricmp(m_argv[i], "true") || !stricmp(m_argv[i], "yes") ) ) {
			m_bUse3D = 1;
		}
		else if (!stricmp(m_argn[i],"loop") && (!stricmp(m_argv[i], "true") || !stricmp(m_argv[i], "yes") ) ) {
			m_bLoop = 1;
		}
		else if (!stricmp(m_argn[i],"aspectratio")) {
			u32 ar = GF_ASPECT_RATIO_KEEP;
			if (!stricmp(m_argv[i], "keep")) ar = GF_ASPECT_RATIO_KEEP;
			else if (!stricmp(m_argv[i], "16:9")) ar = GF_ASPECT_RATIO_16_9;
			else if (!stricmp(m_argv[i], "4:3")) ar = GF_ASPECT_RATIO_4_3;
			else if (!stricmp(m_argv[i], "fill")) ar = GF_ASPECT_RATIO_FILL_SCREEN;
			gf_term_set_option(m_term, GF_OPT_ASPECT_RATIO, ar);
		}
	}

	/*URL is not absolute, request new stream to mozilla - we don't pass absolute URLs since some may not be 
	handled by gecko */
	if (m_szURL) {
		Bool absolute_url = 0;
		if (strstr(m_szURL, "://")) absolute_url = 1;
		else if (m_szURL[0] == '/') {
			FILE *test = fopen(m_szURL, "rb");
			if (test) {	
				absolute_url = 1;
				fclose(test);
			}
		}
		else if ((m_szURL[1] == ':') && (m_szURL[2] == '\\')) absolute_url = 1;

		if (!absolute_url) NPN_GetURL(mInstance, m_szURL, NULL);
		free(m_szURL);
		m_szURL = NULL;
	}

}

void nsOsmozillaInstance::ReloadTerminal()
{
	GF_Terminal *a_term;
	const char *rend;
	Bool needs_3d;
	if (!m_szURL || m_bUse3D) return;

	if (m_szURL && (strstr(m_szURL, ".wrl") || strstr(m_szURL, ".x3d") || strstr(m_szURL, ".x3dv"))) {
		needs_3d = 1;
	} else {
		needs_3d = 0;
	}
	rend = gf_cfg_get_key(m_user.config, "Rendering", "RendererName");
	if (strstr(rend, "3D") && needs_3d) return;
	if (strstr(rend, "2D") && !needs_3d) return;

	a_term = m_term;
	m_term = NULL;
	gf_term_del(a_term);
	gf_cfg_set_key(m_user.config, "Rendering", "RendererName", needs_3d ? "GPAC 3D Renderer" : "GPAC 2D Renderer");
	m_term = gf_term_new(&m_user);
}

NPError nsOsmozillaInstance::NewStream(NPMIMEType type, NPStream * stream,
				    NPBool seekable, uint16 * stype)
{
	if (m_szURL) free(m_szURL);
	m_szURL = strdup((const char *)stream->url);

	ReloadTerminal();
	/*connect from 0 and pause if not autoplay*/
	gf_term_connect_from_time(m_term, m_szURL, 0, m_bAutoStart ? 0 : 1);
	SetFocus((HWND)m_user.os_window_handler);

	/*we handle data fetching ourselves*/
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

uint16 nsOsmozillaInstance::HandleEvent(void* event)
{
  return true;
}
 
void nsOsmozillaInstance::Pause()
{
	if (m_term) {
		if (gf_term_get_option(m_term, GF_OPT_PLAY_STATE) == GF_STATE_PAUSED) {
	        gf_term_set_option(m_term, GF_OPT_PLAY_STATE, GF_STATE_PLAYING);
		} else {
	        gf_term_set_option(m_term, GF_OPT_PLAY_STATE, GF_STATE_PAUSED);
		}
	}
}

void nsOsmozillaInstance::Play()
{
	if (!m_bIsConnected) {
		if (m_szURL) gf_term_connect(m_term, (const char *) m_szURL);
	} else {
		gf_term_set_option(m_term, GF_OPT_PLAY_STATE, GF_STATE_PLAYING);
	}
}

void nsOsmozillaInstance::Stop()
{
	gf_term_disconnect(m_term);
}

#include <gpac/term_info.h>
void nsOsmozillaInstance::Update(const char *type, const char *commands)
{
	if (m_term) {
		GF_Err e = gf_term_scene_update(m_term, (char *) type, (char *) commands);
		if (e) {
			char szMsg[1024];
			sprintf((char *)szMsg, "GPAC: Error applying update (%s)", gf_error_to_string(e) );
			NPN_Status(mInstance, szMsg);
		}
	}
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
NS_IMETHODIMP nsOsmozillaPeer::Pause() { mPlugin->Pause(); return NS_OK; }
NS_IMETHODIMP nsOsmozillaPeer::Play() { mPlugin->Play(); return NS_OK; }
NS_IMETHODIMP nsOsmozillaPeer::Stop() { mPlugin->Stop(); return NS_OK; }

NS_IMETHODIMP nsOsmozillaPeer::Update(const char *type, const char *commands) 
{
	mPlugin->Update(type, commands); 
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

