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
		*((char **)aValue) = "GPAC Plugin " GPAC_FULL_VERSION " for Mozilla. For more information go to <a href=\"http://gpac.sourceforge.net\">GPAC website</a>";
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

	SetOptions();
}

void nsOsmozillaInstance::SetOptions()
{
	m_bLoop = 0;
	m_bAutoStart = 1;
	m_bUse3D = 0;		

	/*options sent from plugin*/
	for(int i=0;i<m_argc;i++) {   
		if (!m_argn[i] || !m_argv[i]) continue;
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
			aspect_ratio = GF_ASPECT_RATIO_KEEP;
			if (!stricmp(m_argv[i], "keep")) aspect_ratio = GF_ASPECT_RATIO_KEEP;
			else if (!stricmp(m_argv[i], "16:9")) aspect_ratio = GF_ASPECT_RATIO_16_9;
			else if (!stricmp(m_argv[i], "4:3")) aspect_ratio = GF_ASPECT_RATIO_4_3;
			else if (!stricmp(m_argv[i], "fill")) aspect_ratio = GF_ASPECT_RATIO_FILL_SCREEN;
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

		if (!absolute_url) {
			char *url = m_szURL;
			m_szURL = NULL;
			NPN_GetURL(mInstance, url, NULL);
			free(url);
		}
	}

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

static void osmozilla_do_log(void *cbk, u32 level, u32 tool, const char *fmt, va_list list)
{
	FILE *logs = (FILE *) cbk;
	vfprintf(logs, fmt, list);
	fflush(logs);
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
	strcpy((char *) config_path, "C:\\CVS\\gpac\\bin\\w32_deb");
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
	m_user.modules = gf_modules_new(str, m_user.config);
	if (!gf_modules_get_count(m_user.modules)) goto err_exit;

	m_user.opaque = this;
	
	m_disable_mime = 1;
	str = gf_cfg_get_key(m_user.config, "General", "NoMIMETypeFetch");
	if (str && !strcmp(str, "no")) m_disable_mime = 0;

	if (SetWindow(aWindow)) mInitialized = TRUE;

	/*check log file*/
	str = gf_cfg_get_key(m_user.config, "General", "LogFile");
	if (str) {
		m_logs = fopen(str, "wt");
		gf_log_set_callback(m_logs, osmozilla_do_log);
	}
	else m_logs = NULL;

	/*set log level*/
	m_log_level = 0;
	str = gf_cfg_get_key(m_user.config, "General", "LogLevel");
	if (str) {
		if (!stricmp(str, "debug")) m_log_level = GF_LOG_DEBUG;
		else if (!stricmp(str, "info")) m_log_level = GF_LOG_INFO;
		else if (!stricmp(str, "warning")) m_log_level = GF_LOG_WARNING;
		else if (!stricmp(str, "error")) m_log_level = GF_LOG_ERROR;
		gf_log_set_level(m_log_level);
	}
	if (m_log_level && !m_logs) m_logs = stdout;

	/*set log tools*/
	m_log_tools = 0;
	str = gf_cfg_get_key(m_user.config, "General", "LogTools");
	if (str) {
		char *sep;
		char *val = (char *) str;
		while (val) {
			sep = strchr(val, ':');
			if (sep) sep[0] = 0;
			if (!stricmp(val, "core")) m_log_tools |= GF_LOG_CODING;
			else if (!stricmp(val, "coding")) m_log_tools |= GF_LOG_CODING;
			else if (!stricmp(val, "container")) m_log_tools |= GF_LOG_CONTAINER;
			else if (!stricmp(val, "network")) m_log_tools |= GF_LOG_NETWORK;
			else if (!stricmp(val, "rtp")) m_log_tools |= GF_LOG_RTP;
			else if (!stricmp(val, "author")) m_log_tools |= GF_LOG_AUTHOR;
			else if (!stricmp(val, "sync")) m_log_tools |= GF_LOG_SYNC;
			else if (!stricmp(val, "codec")) m_log_tools |= GF_LOG_CODEC;
			else if (!stricmp(val, "parser")) m_log_tools |= GF_LOG_PARSER;
			else if (!stricmp(val, "media")) m_log_tools |= GF_LOG_MEDIA;
			else if (!stricmp(val, "scene")) m_log_tools |= GF_LOG_SCENE;
			else if (!stricmp(val, "script")) m_log_tools |= GF_LOG_SCRIPT;
			else if (!stricmp(val, "interact")) m_log_tools |= GF_LOG_INTERACT;
			else if (!stricmp(val, "compose")) m_log_tools |= GF_LOG_COMPOSE;
			else if (!stricmp(val, "mmio")) m_log_tools |= GF_LOG_MMIO;
			else if (!stricmp(val, "none")) m_log_tools = 0;
			else if (!stricmp(val, "all")) m_log_tools = 0xFFFFFFFF;
			if (!sep) break;
			sep[0] = ':';
			val = sep+1;
		}
		gf_log_set_tools(m_log_tools);
	}
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
	case GF_EVENT_MESSAGE:
		if (!evt->message.message) return 0;
		if (evt->message.error)
			sprintf((char *)msg, "GPAC: %s (%s)", evt->message.message, gf_error_to_string(evt->message.error));
		else
			sprintf((char *)msg, "GPAC: %s", evt->message.message);

		NPN_Status(mInstance, msg);
		break;
	case GF_EVENT_PROGRESS:
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
	case GF_EVENT_SCENE_SIZE:	
		gf_term_set_size(m_term, m_width, m_height);
		break;
	/*window has been resized (full-screen plugin), resize*/
	case GF_EVENT_SIZE:	
		m_width = evt->size.width;
		m_height = evt->size.height;
		gf_term_set_size(m_term, m_width, m_height);
		break;
	case GF_EVENT_CONNECT:	
		m_bIsConnected = evt->connect.is_connected;
		break;
	case GF_EVENT_DURATION:	
		m_bCanSeek = evt->duration.can_seek;
		m_Duration = evt->duration.duration;
		break;
	case GF_EVENT_DBLCLICK:
		gf_term_set_option(m_term, GF_OPT_FULLSCREEN, !gf_term_get_option(m_term, GF_OPT_FULLSCREEN));
		break;
	case GF_EVENT_KEYDOWN:
		if ((evt->key.flags & GF_KEY_MOD_ALT)) {
	    } else {
			switch (evt->key.key_code) {
			case GF_KEY_HOME:
				gf_term_set_option(m_term, GF_OPT_NAVIGATION_TYPE, 1);
				break;
			case GF_KEY_ESCAPE:
				gf_term_set_option(m_term, GF_OPT_FULLSCREEN, !gf_term_get_option(m_term, GF_OPT_FULLSCREEN));
				break;
			}
		}
	    break;
	case GF_EVENT_NAVIGATE_INFO:
		strcpy(msg, evt->navigate.to_url);
		NPN_Status(mInstance, msg);
		break;
	case GF_EVENT_NAVIGATE:
		if (gf_term_is_supported_url(m_term, evt->navigate.to_url, 1, m_disable_mime)) {
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
	if (!aWindow->width || !aWindow->height) return FALSE;

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
 
	m_term = gf_term_new(&m_user);
	if (! m_term) return FALSE;

	gf_term_set_option(m_term, GF_OPT_ASPECT_RATIO, aspect_ratio);
	mInitialized = TRUE;
	
	/*stream not ready*/
	if (!m_szURL || !m_bAutoStart) return TRUE;

	/*connect from 0 and pause if not autoplay*/
	gf_term_connect(m_term, m_szURL);
	return TRUE;
}



NPError nsOsmozillaInstance::NewStream(NPMIMEType type, NPStream * stream,
				    NPBool seekable, uint16 * stype)
{
	if (m_szURL) free(m_szURL);
	m_szURL = strdup((const char *)stream->url);

	/*connect from 0 and pause if not autoplay*/
	if (m_bAutoStart)
		gf_term_connect(m_term, m_szURL);

	/*we handle data fetching ourselves*/
    *stype = NP_SEEK;
    return NPERR_NO_ERROR;
}

NPError nsOsmozillaInstance::DestroyStream(NPStream * stream, NPError reason)
{
	if (0 && m_szURL) {
		gf_term_disconnect(m_term);
		free(m_szURL);
		m_szURL = NULL;
	}
	return NPERR_NO_ERROR;
}

uint16 nsOsmozillaInstance::HandleEvent(void* event)
{
	fprintf(stdout, "event !\n");
  return false;
}
 
void nsOsmozillaInstance::Pause()
{
fprintf(stdout, "pause\n");
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

#ifdef XP_WIN
PBITMAPINFO CreateBitmapInfoStruct(GF_VideoSurface *pfb)
{ 
    PBITMAPINFO pbmi; 
    WORD    cClrBits; 

	cClrBits = 32;

    pbmi = (PBITMAPINFO) LocalAlloc(LPTR, 
                    sizeof(BITMAPINFOHEADER)); 

    pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER); 
	pbmi->bmiHeader.biWidth = pfb->width;
	pbmi->bmiHeader.biHeight = 1;
    pbmi->bmiHeader.biPlanes = 1; 
    pbmi->bmiHeader.biBitCount = cClrBits; 

    pbmi->bmiHeader.biCompression = BI_RGB; 
    pbmi->bmiHeader.biSizeImage = ((pbmi->bmiHeader.biWidth * cClrBits +31) & ~31) /8
                                  * pbmi->bmiHeader.biHeight; 
     pbmi->bmiHeader.biClrImportant = 0; 
     return pbmi; 
}
#endif

void nsOsmozillaInstance::Print(NPPrint* printInfo)
{
	if (printInfo->mode == NP_EMBED)
	{
		NPEmbedPrint *ep = (NPEmbedPrint *)printInfo;
#ifdef XP_MACOS
		/*
		ep->platformPrint contains a THPrint reference on MacOS
		*/
		}
#endif  // XP_MACOS
#ifdef XP_UNIX
		/*
		ep->platformPrint  contains a NPPrintCallbackStruct on Unix and
		the plug-in location and size in the NPWindow are in page coordinates (720/ inch), but the printer requires point coordinates (72/inch)
		*/
#endif  // XP_UNIX
#ifdef XP_WIN
		/*
		The coordinates for the window rectangle are in TWIPS format. 
		This means that you need to convert the x-y coordinates using the Windows API call DPtoLP when you output text
		*/
		HDC pDC = (HDC)printInfo->print.embedPrint.platformPrint;
		GF_VideoSurface fb;
		u32 xsrc, ysrc;
		u16 src_16;
		char *src;
		/*lock the source buffer */
		gf_term_get_screen_buffer(m_term, &fb);
		BITMAPINFO	*infoSrc = CreateBitmapInfoStruct(&fb);
		float deltay = (float)printInfo->print.embedPrint.window.height/(float)fb.height;
		int	ysuiv = 0;
		char *ligne = (char *) LocalAlloc(GMEM_FIXED, fb.width*4);
		for (ysrc=0; ysrc<fb.height; ysrc++)
		{
			char *dst = (char*)ligne;
			src = fb.video_buffer + ysrc * fb.pitch;
			for (xsrc=0; xsrc<fb.width; xsrc++)
			{
				switch (fb.pixel_format) {
				case GF_PIXEL_RGB_32:
				case GF_PIXEL_ARGB:
					dst[0] = src[0];
					dst[1] = src[1];
					dst[2] = src[2];
					src+=4;
					break;
				case GF_PIXEL_BGR_32:
				case GF_PIXEL_RGBA:
					dst[0] = src[3];
					dst[1] = src[2];
					dst[2] = src[1];
					src+=4;
					break;
				case GF_PIXEL_RGB_24:
					dst[0] = src[2];
					dst[1] = src[1];
					dst[2] = src[0];
					src+=3;
					break;
				case GF_PIXEL_BGR_24:
					dst[0] = src[2];
					dst[1] = src[1];
					dst[2] = src[0];
					src+=3;
					break;
				case GF_PIXEL_RGB_565:
					src_16 = * ( (u16 *)src );
					dst[2] = (src_16 >> 8) & 0xf8;
					dst[2] += dst[2]>>5;
					dst[1] = (src_16 >> 3) & 0xfc;
					dst[1] += dst[1]>>6;
					dst[0] = (src_16 << 3) & 0xf8;
					dst[0] += dst[0]>>5;
					src+=2;
					break;
				case GF_PIXEL_RGB_555:
					src_16 = * (u16 *)src;
					dst[2] = (src_16 >> 7) & 0xf8;
					dst[2] += dst[2]>>5;
					dst[1] = (src_16 >> 2) & 0xf8;
					dst[1] += dst[1]>>5;
					dst[0] = (src_16 << 3) & 0xf8;
					dst[0] += dst[0]>>5;
					src+=2;
					break;
				}
				dst += 4;
			}
			int ycrt = ysuiv;
			ysuiv = (u32) ( ((float)ysrc+1.0)*deltay);
			int delta = ysuiv-ycrt;
			StretchDIBits(
				pDC, printInfo->print.embedPrint.window.x, ycrt+printInfo->print.embedPrint.window.y, printInfo->print.embedPrint.window.width, 
				delta, 
				0, 0, fb.width, 1,
				ligne, infoSrc, DIB_RGB_COLORS, SRCCOPY);
		}

		/*unlock GPAC frame buffer */
		gf_term_release_screen_buffer(m_term, &fb);
		/* free temporary  objects */
		GlobalFree(ligne);
		LocalFree(infoSrc);
#endif   // XP_WIN
	} else if (printInfo->mode == NP_FULL)
	{
		NPFullPrint *ep = (NPFullPrint *)printInfo;
		// TODO present the print dialog and manage the print
	}
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
fprintf(stdout, "get peer\n");
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

