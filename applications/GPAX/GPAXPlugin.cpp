/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Y.XI, X. ZHAO, J. Le Feuvre
 *			Copyright (c) ENST 2006-200x
 *					All rights reserved
 *
 *  This file is part of GPAC / ActiveX control
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

#include "stdafx.h"
#include "GPAX.h"
#include "GPAXPlugin.h"
#include <gpac/network.h>
#include <gpac/utf.h>


/////////////////////////////////////////////////////////////////////////////
// CGPAXPlugin

void CGPAXPlugin::SetStatusText(char *msg)
{
	if (m_pBrowser) {
		if (msg) {
			u16 w_msg[1024];
			gf_utf8_mbstowcs(w_msg, 1024, (const char **)&msg);
			m_pBrowser->put_StatusText(w_msg);
		} else {
			m_pBrowser->put_StatusText(L"");
		}
	}
}
//GPAC player Event Handler. not yet implemented, just dummies here
Bool CGPAXPlugin::EventProc(GF_Event *evt)
{
	char msg[1024];
	if (!m_term) return 0;

    switch (evt->type) {
	case GF_EVT_MESSAGE:
		if (evt->message.error) {
			sprintf(msg, "(GPAC) %s (%s)", evt->message.message, gf_error_to_string(evt->message.error));
		} else {
			sprintf(msg, "(GPAC) %s", evt->message.message);
		}
		SetStatusText(msg);
        break;
	case GF_EVT_PROGRESS:
		if (evt->progress.done == evt->progress.total) {
			SetStatusText(NULL);
		} else {
			char *szTitle = "";
			if (evt->progress.progress_type==0) szTitle = "Buffer ";
			else if (evt->progress.progress_type==1) szTitle = "Download ";
			else if (evt->progress.progress_type==2) szTitle = "Import ";
			sprintf(msg, "(GPAC) %s: %02.2f", szTitle, (100.0*evt->progress.done) / evt->progress.total);
			SetStatusText(msg);
		}
		break;
    case GF_EVT_CONNECT:
        m_bIsConnected = evt->connect.is_connected;
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
		SetStatusText(msg);
		break;
	case GF_EVT_NAVIGATE:
		if (gf_term_is_supported_url(m_term, evt->navigate.to_url, 1, 1)) {
			gf_term_navigate_to(m_term, evt->navigate.to_url);
			return 1;
		} else if (m_pBrowser) {
			u32 i;
			const char **sz_ptr;
			u16 w_szTar[1024], w_szURL[1024];
			VARIANT target, flags;
			flags.intVal = 0;
			target.bstrVal = L"_SELF";

			for (i=0; i<evt->navigate.param_count; i++) {
				if (!strcmp(evt->navigate.parameters[i], "_parent")) target.bstrVal = L"_PARENT";
				else if (!strcmp(evt->navigate.parameters[i], "_blank")) target.bstrVal = L"_BLANK";
				else if (!strcmp(evt->navigate.parameters[i], "_top")) target.bstrVal = L"_TOP";
				else if (!strcmp(evt->navigate.parameters[i], "_new")) flags.intVal |= navOpenInNewWindow;
				else if (!strnicmp(evt->navigate.parameters[i], "_target=", 8)) {
					sz_ptr = & evt->navigate.parameters[i]+8;
					gf_utf8_mbstowcs(w_szTar, 1024, (const char **)sz_ptr);
					target.bstrVal = w_szTar;
				}
			}
			sz_ptr = & evt->navigate.to_url;
			gf_utf8_mbstowcs(w_szURL, 1024, (const char **)sz_ptr);
			m_pBrowser->Navigate(w_szURL, &flags, &target, NULL, NULL);;
			return 1;
		}
		break;
    }
    return 0;
}

Bool GPAX_EventProc(void *ptr, GF_Event *evt)
{
    CGPAXPlugin *_this = (CGPAXPlugin *)ptr;
    return _this->EventProc(evt);
}

//Read Parameters from pPropBag given by MSIE
Bool CGPAXPlugin::ReadParamString(LPPROPERTYBAG pPropBag, LPERRORLOG pErrorLog,
                                 WCHAR *name, TCHAR *buf, int bufsize)
{
    VARIANT v;
    HRESULT hr;
    Bool retval=0;

    v.vt = VT_EMPTY;
    v.bstrVal = NULL;
    hr = pPropBag->Read(name, &v, pErrorLog);
    if(SUCCEEDED(hr))
    {
        if(v.vt==VT_BSTR && v.bstrVal)
        {
            USES_CONVERSION;
            lstrcpyn(buf,OLE2T(v.bstrVal),bufsize);
            retval=1;
        }
        VariantClear(&v);
    }
    return retval;
}

//Create window message fuction. when the window is created, also initialize a instance of
//GPAC player instance.
LRESULT CGPAXPlugin::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if (m_term) return 0;
	
		
    unsigned char config_path[GF_MAX_PATH];
    char *gpac_cfg;
    const char *str;

    if (m_hWnd==NULL) return 0;
    gpac_cfg = "GPAC.cfg";

#ifdef _DEBUG
	strcpy((char *) config_path, "D:\\cvs\\gpac\\bin\\w32_deb\\");
#else
	//Here we retrieve GPAC config file in the install diractory, which is indicated in the 
	//Registry
    HKEY hKey = NULL;
    DWORD dwSize;
    RegOpenKeyEx(HKEY_CLASSES_ROOT, "GPAC", 0, KEY_READ, &hKey);
    dwSize = GF_MAX_PATH;
    RegQueryValueEx(hKey, "InstallDir", NULL, NULL, (unsigned char*) config_path, &dwSize);
    RegCloseKey(hKey);
#endif

	//Create a structure m_user for initialize the terminal. the parameters to set:
	//1)config file path
	//2)Modules file path
	//3)window handler
	//4)EventProc
    memset(&m_user, 0, sizeof(m_user));

    m_user.config = gf_cfg_new((const char*) config_path, gpac_cfg);
    if(!m_user.config) goto err_exit;

    str = gf_cfg_get_key(m_user.config, "General", "ModulesDirectory");
    m_user.modules = gf_modules_new((const unsigned char *) str, m_user.config);
    if(!gf_modules_get_count(m_user.modules)) goto err_exit;

    m_user.os_window_handler = m_hWnd;
    m_user.opaque = this;
    m_user.EventProc = GPAX_EventProc;

	gf_cfg_set_key(m_user.config, "Rendering", "RendererName", m_bUse3D ? "GPAC 3D Renderer" : "GPAC 2D Renderer");

	//create a terminal
    m_term = gf_term_new(&m_user);

	if (!m_term) goto err_exit;
	
	gf_term_set_option(m_term, GF_OPT_AUDIO_VOLUME, 100);
    
	RECT rc;
    ::GetWindowRect(m_hWnd, &rc);
    m_width = rc.right-rc.left;
    m_height = rc.bottom-rc.top;

	if (m_bAutoPlay && strlen(m_url)) Play();
    return 0;

	//Error Processing
err_exit:
    if(m_user.modules)
        gf_modules_del(m_user.modules);
    m_user.modules = NULL;
    if(m_user.config)
        gf_cfg_del(m_user.config);
    m_user.config = NULL;
	return 1;
}

CGPAXPlugin::~CGPAXPlugin()
{
	if (m_term) {
		GF_Terminal *a_term = m_term;
		m_term = NULL;
		gf_term_del(a_term);
	}
	if (m_user.modules) gf_modules_del(m_user.modules);
	if (m_user.config) gf_cfg_del(m_user.config);
	memset(&m_user, 0, sizeof(m_user));

	if (m_pBrowser) m_pBrowser->Release();
	m_pBrowser = NULL;
}


HRESULT CGPAXPlugin::OnDraw(ATL_DRAWINFO& di)
{
	if (m_term && m_bInitialDraw) {
		m_bInitialDraw = FALSE;
		if (m_bAutoPlay) Play();
	}
    return S_OK;
}

// Load is called before OnCreate, but it may not be called at
// all if there are no parameters.
STDMETHODIMP CGPAXPlugin::Load(LPPROPERTYBAG pPropBag, LPERRORLOG pErrorLog)
{
	char szOpt[1024];
    // examine the <embed>/<param> tag arguments

    ReadParamString(pPropBag,pErrorLog,L"src", m_url, MAXLEN_URL);

    if (ReadParamString(pPropBag,pErrorLog,L"autostart", szOpt, 1024))
		m_bAutoPlay = (!stricmp(szOpt, "false") || !stricmp(szOpt, "no")) ? 0 : 1;

    if (ReadParamString(pPropBag,pErrorLog,L"use3d", szOpt, 1024))
		m_bUse3D = (!stricmp(szOpt, "true") || !stricmp(szOpt, "yes")) ? 1 : 0;

    if (ReadParamString(pPropBag,pErrorLog,L"aspectratio", szOpt, 1024)) {
		if (!stricmp(szOpt, "keep")) m_AR = GF_ASPECT_RATIO_KEEP;
		else if (!stricmp(szOpt, "16:9")) m_AR = GF_ASPECT_RATIO_16_9;
		else if (!stricmp(szOpt, "4:3")) m_AR = GF_ASPECT_RATIO_4_3;
		else if (!stricmp(szOpt, "fill")) m_AR = GF_ASPECT_RATIO_FILL_SCREEN;
	}

    if (ReadParamString(pPropBag,pErrorLog,L"loop", szOpt, 1024))
		m_bLoop = !stricmp(szOpt, "true") ? 0 : 1;

	/*get absolute URL*/
	if (strlen(m_url)) {
		IMoniker* pMoniker	= NULL;
		LPOLESTR sDisplayName;

		if (SUCCEEDED(m_spClientSite->GetMoniker(OLEGETMONIKER_TEMPFORUSER,
										   OLEWHICHMK_CONTAINER,
										   &pMoniker) ) ) {
			char parent_url[1024];
			pMoniker->GetDisplayName(NULL, NULL, &sDisplayName);
			wcstombs(parent_url, sDisplayName, 300);
			pMoniker->Release();


			char *abs_url = gf_url_concatenate(parent_url, m_url);
			if (abs_url) {
				strcpy(m_url, abs_url);
				free(abs_url);
			}
		}
	}
	/*get the top-level container*/
	if (!m_pBrowser) {
		IServiceProvider *isp, *isp2 = NULL;
		if ( SUCCEEDED(m_spClientSite->QueryInterface(IID_IServiceProvider, reinterpret_cast<void **>(&isp)) ) ) {

			if (SUCCEEDED(isp->QueryService(SID_STopLevelBrowser, IID_IServiceProvider, reinterpret_cast<void **>(&isp2)) ) ) {
				isp2->QueryService(SID_SWebBrowserApp, IID_IWebBrowser2, reinterpret_cast<void **>(&m_pBrowser));
				isp2->Release();
			}
			isp->Release();
		}
	}

	if (m_pBrowser) {
		m_pBrowser->put_StatusText(L"GPAC Ready");
	}

	return IPersistPropertyBagImpl<CGPAXPlugin>::Load(pPropBag, pErrorLog);
}

STDMETHODIMP CGPAXPlugin::Play()
{
	if (m_term) {
		if (!m_bIsConnected) {
			if (strlen(m_url)) {
				gf_term_connect(m_term, m_url);
				gf_term_set_option(m_term, GF_OPT_ASPECT_RATIO, m_AR);
			}
		} else
			gf_term_set_option(m_term, GF_OPT_PLAY_STATE, GF_STATE_PLAYING);   //if target is connected, set it playing
	}
    return S_OK;
}

STDMETHODIMP CGPAXPlugin::Pause()
{
    if(m_term) {
		if (gf_term_get_option(m_term, GF_OPT_PLAY_STATE) == GF_STATE_PAUSED) {
	        gf_term_set_option(m_term, GF_OPT_PLAY_STATE, GF_STATE_PLAYING);
		} else {
	        gf_term_set_option(m_term, GF_OPT_PLAY_STATE, GF_STATE_PAUSED);
		}
	}
    return S_OK;
}

STDMETHODIMP CGPAXPlugin::Stop()
{
    if(m_term) gf_term_disconnect(m_term);     //set it stop
    return S_OK;
}

STDMETHODIMP CGPAXPlugin::Reload()
{
    if (m_term && strlen(m_url)) {
        gf_term_disconnect(m_term);
        gf_term_connect(m_term, m_url);
		gf_term_set_option(m_term, GF_OPT_ASPECT_RATIO, m_AR);
    }
    return S_OK;
}
