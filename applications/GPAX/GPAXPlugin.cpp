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

#ifndef _WIN32_WCE
#include <mshtml.h>
#endif

/////////////////////////////////////////////////////////////////////////////
// CGPAXPlugin

#if 0
static print_err(char *msg, char *title)
{
	u16 w_msg[1024], w_title[1024];
	CE_CharToWide(msg, w_msg);
	CE_CharToWide(title, w_title);
	::MessageBox(NULL, w_msg, w_title, MB_OK);
}
#endif


void CGPAXPlugin::SetStatusText(char *msg)
{
#ifndef _WIN32_WCE
	if (m_pBrowser) {
		if (msg) {
			u16 w_msg[1024];
			gf_utf8_mbstowcs(w_msg, 1024, (const char **)&msg);
			m_pBrowser->put_StatusText((BSTR) w_msg);
		} else {
			m_pBrowser->put_StatusText(L"");
		}
	}
#endif
}
//GPAC player Event Handler. not yet implemented, just dummies here
Bool CGPAXPlugin::EventProc(GF_Event *evt)
{
	char msg[1024];
	if (!m_term) return 0;

    switch (evt->type) {
	case GF_EVENT_MESSAGE:
		if (evt->message.error) {
			sprintf(msg, "(GPAC) %s (%s)", evt->message.message, gf_error_to_string(evt->message.error));
		} else {
			sprintf(msg, "(GPAC) %s", evt->message.message);
		}
		SetStatusText(msg);
        break;
	case GF_EVENT_PROGRESS:
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
    case GF_EVENT_CONNECT:
        m_bIsConnected = evt->connect.is_connected;
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
	case GF_EVENT_DBLCLICK:
		gf_term_set_option(m_term, GF_OPT_FULLSCREEN, !gf_term_get_option(m_term, GF_OPT_FULLSCREEN));
		break;
	case GF_EVENT_KEYDOWN:
		if ((evt->key.flags  & GF_KEY_MOD_ALT)) {
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
		SetStatusText(msg);
		break;
	case GF_EVENT_NAVIGATE:
		if (gf_term_is_supported_url(m_term, evt->navigate.to_url, 1, 1)) {
			gf_term_navigate_to(m_term, evt->navigate.to_url);
			return 1;
		} 
#ifndef _WIN32_WCE
		else if (m_pBrowser) {
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
					target.bstrVal = (BSTR) w_szTar;
				}
			}
			sz_ptr = & evt->navigate.to_url;
			gf_utf8_mbstowcs(w_szURL, 1024, (const char **)sz_ptr);
			m_pBrowser->Navigate((BSTR) w_szURL, &flags, &target, NULL, NULL);;
			return 1;
		}
#endif
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
                                 WCHAR *name, char *buf, int bufsize)
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
//            USES_CONVERSION;
//            lstrcpyn(buf,OLE2T(v.bstrVal),bufsize);
			const u16 *srcp = (const u16 *) v.bstrVal;
			u32 len = gf_utf8_wcstombs(buf, bufsize, &srcp);
			if (len>=0) {
				buf[len] = 0;
				retval=1;
			}
        }
        VariantClear(&v);
    }
    return retval;
}

void CGPAXPlugin::LoadDATAUrl()
{
#ifndef _WIN32_WCE
	HRESULT hr;

	if (m_url[0]) return;
    /*get parent doc*/
	CComPtr<IOleContainer> spContainer;
	if (m_spClientSite->GetContainer(&spContainer) != S_OK)
		return;
    CComPtr<IHTMLDocument2> spDoc = CComQIPtr<IHTMLDocument2>(spContainer);
    CComPtr<IHTMLElementCollection> spColl;
	if (spDoc->get_all(&spColl) != S_OK)
		return;
	/*get HTML <object> in the doc*/
    CComPtr<IDispatch> spDisp;
	CComPtr<IHTMLElementCollection> sphtmlObjects;

    CComPtr<IDispatch> spDispObjects;
	if (spColl->tags(CComVariant("OBJECT"), &spDispObjects) != S_OK)
		return;
    CComPtr<IHTMLElementCollection> spObjs = CComQIPtr<IHTMLElementCollection>(spDispObjects);
	
	/*browse all objects and find us*/
	long lCount = 0;
	spObjs->get_length(&lCount);
	for (long lCnt = 0; lCnt < lCount; lCnt++) {   
		IDispatch *an_obj= NULL;
		CComVariant varEmpty;
		CComVariant varName;
		varName.vt = VT_I4;
		varName.lVal = lCnt;
		hr = spObjs->item(varName, varEmpty, &an_obj);
		varName.Clear();
		varEmpty.Clear();
		if (hr != S_OK) continue;

		/*get the IHTMLObject*/
		IHTMLObjectElement* pObjectElem=NULL;
		an_obj->QueryInterface(IID_IHTMLObjectElement, (void**)&pObjectElem);
		if (!pObjectElem) continue;

		/*get its parent owner - it MUST be us*/
		IDispatch *disp= NULL;
		pObjectElem->get_object(&disp);
		if (disp != this) continue;

		BSTR data = NULL;
		if ((pObjectElem->get_data(&data) == S_OK) && data) {
			const u16 *srcp = (const u16 *) data;
			u32 len = gf_utf8_wcstombs(m_url, MAXLEN_URL, &srcp);
			if (len>=0) m_url[len] = 0;
		}
		SysFreeString(data);
		break;
	}

	if (m_url) {
		UpdateURL();
	}
#endif

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

#if defined(_DEBUG) && !defined(_WIN32_WCE)
	strcpy((char *) config_path, "D:\\cvs\\gpac\\bin\\w32_deb\\");
#else
	//Here we retrieve GPAC config file in the install diractory, which is indicated in the 
	//Registry
    HKEY hKey = NULL;
    DWORD dwSize;
#ifdef _WIN32_WCE
    u16 w_path[1024];
	RegOpenKeyEx(HKEY_CLASSES_ROOT, TEXT("GPAC"), 0, KEY_READ, &hKey);
	DWORD dwType = REG_SZ;
    dwSize = GF_MAX_PATH;
    RegQueryValueEx(hKey, TEXT("InstallDir"), 0, &dwType, (LPBYTE) w_path, &dwSize);
	CE_WideToChar(w_path, (char *)config_path);
    RegCloseKey(hKey);
#else
    RegOpenKeyEx(HKEY_CLASSES_ROOT, "GPAC", 0, KEY_READ, &hKey);
    dwSize = GF_MAX_PATH;
    RegQueryValueEx(hKey, "InstallDir", NULL, NULL, (unsigned char*) config_path, &dwSize);
    RegCloseKey(hKey);
#endif
#endif

	//Create a structure m_user for initialize the terminal. the parameters to set:
	//1)config file path
	//2)Modules file path
	//3)window handler
	//4)EventProc
    memset(&m_user, 0, sizeof(m_user));

    m_user.config = gf_cfg_new((const char*) config_path, gpac_cfg);
    if(!m_user.config) {
		char cfg_file[MAX_PATH];
		/*create a blank config*/
		sprintf(cfg_file, "%s\\%s", config_path, gpac_cfg);
		FILE *test = fopen(cfg_file, "wt");
		if (test) fclose(test);
		m_user.config = gf_cfg_new((const char *) config_path, gpac_cfg);
	    if(!m_user.config) {
#ifdef _WIN32_WCE
			::MessageBox(NULL, _T("GPAC Configuration file not found"), _T("Fatal Error"), MB_OK);
#else
			::MessageBox(NULL, "GPAC Configuration file not found", "Fatal Error", MB_OK);
#endif
			goto err_exit;
		}
		gf_cfg_set_key(m_user.config, "General", "ModulesDirectory", (const char *) config_path);
		gf_cfg_set_key(m_user.config, "Compositor", "Raster2D", "GPAC 2D Raster");
		sprintf((char *) cfg_file, "%s\\cache", config_path);
		gf_cfg_set_key(m_user.config, "General", "CacheDirectory", (const char *) cfg_file);
		gf_cfg_set_key(m_user.config, "Network", "AutoReconfigUDP", "no");
		gf_cfg_set_key(m_user.config, "Network", "BufferLength", "0");
		gf_cfg_set_key(m_user.config, "Audio", "ForceConfig", "yes");
		gf_cfg_set_key(m_user.config, "Audio", "NumBuffers", "2");
		gf_cfg_set_key(m_user.config, "Audio", "TotalDuration", "120");
		gf_cfg_set_key(m_user.config, "Video", "DriverName", "dx_hw");
		gf_cfg_set_key(m_user.config, "Video", "UseHardwareMemory", "yes");

#ifdef _WIN32_WCE
		strcpy((char*)cfg_file, "\\windows");
#else
		::GetWindowsDirectory((char*)cfg_file, MAX_PATH);
#endif
		if (cfg_file[strlen((char*)cfg_file)-1] != '\\') strcat((char*)cfg_file, "\\");
		strcat((char *)cfg_file, "Fonts");
		gf_cfg_set_key(m_user.config, "FontEngine", "FontDirectory", (const char *) cfg_file);
	}

    str = gf_cfg_get_key(m_user.config, "General", "ModulesDirectory");
    m_user.modules = gf_modules_new(str, m_user.config);
    if(!gf_modules_get_count(m_user.modules)) goto err_exit;

    m_user.os_window_handler = m_hWnd;
    m_user.opaque = this;
    m_user.EventProc = GPAX_EventProc;

	//create a terminal
    m_term = gf_term_new(&m_user);

	if (!m_term) goto err_exit;
	
	gf_term_set_option(m_term, GF_OPT_AUDIO_VOLUME, 100);
    
	LoadDATAUrl();

	RECT rc;
    ::GetWindowRect(m_hWnd, &rc);
    m_width = rc.right-rc.left;
    m_height = rc.bottom-rc.top;
	if (m_bAutoStart && strlen(m_url)) Play();
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

void CGPAXPlugin::UnloadTerm()
{
	if (m_term) {
		GF_Terminal *a_term = m_term;
		m_term = NULL;
		gf_term_del(a_term);
	}
	if (m_user.modules) gf_modules_del(m_user.modules);
	if (m_user.config) gf_cfg_del(m_user.config);
	memset(&m_user, 0, sizeof(m_user));
}
CGPAXPlugin::~CGPAXPlugin()
{
	UnloadTerm();
#ifndef _WIN32_WCE
	if (m_pBrowser) m_pBrowser->Release();
	m_pBrowser = NULL;
#endif
}
LRESULT CGPAXPlugin::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	UnloadTerm();
    return 0;
}


HRESULT CGPAXPlugin::OnDraw(ATL_DRAWINFO& di)
{
	if (m_term && m_bInitialDraw) {
		m_bInitialDraw = FALSE;
		if (m_bAutoStart) Play();
	}
    return S_OK;
}

// Load is called before OnCreate, but it may not be called at
// all if there are no parameters.
STDMETHODIMP CGPAXPlugin::Load(LPPROPERTYBAG pPropBag, LPERRORLOG pErrorLog)
{
	char szOpt[1024];
    // examine the <embed>/<param> tag arguments

	m_url[0] = 0;
    ReadParamString(pPropBag,pErrorLog,L"src", m_url, MAXLEN_URL);
	if (!m_url[0])
	    ReadParamString(pPropBag,pErrorLog,L"data", m_url, MAXLEN_URL);

    if (ReadParamString(pPropBag,pErrorLog,L"autostart", szOpt, 1024))
		m_bAutoStart = (!stricmp(szOpt, "false") || !stricmp(szOpt, "no")) ? 0 : 1;

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

	UpdateURL();

#ifndef _WIN32_WCE
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
	if (m_pBrowser) m_pBrowser->put_StatusText(L"GPAC Ready");
#endif

	return IPersistPropertyBagImpl<CGPAXPlugin>::Load(pPropBag, pErrorLog);
}

void CGPAXPlugin::UpdateURL()
{
	/*get absolute URL*/
	if (!strlen(m_url)) return;
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

STDMETHODIMP CGPAXPlugin::Save(LPPROPERTYBAG pPropBag, BOOL fClearDirty, BOOL fSaveAllProperties)
{
	u16 wurl[MAXLEN_URL];
	const char *sptr;
	u16 len;

    VARIANT value;
    if( pPropBag == NULL) return E_INVALIDARG;

    VariantInit(&value);

    V_VT(&value) = VT_BOOL;
    V_BOOL(&value) = m_bAutoStart ? VARIANT_TRUE : VARIANT_FALSE;
    pPropBag->Write(OLESTR("AutoStart"), &value);
    VariantClear(&value);

    V_VT(&value) = VT_BSTR;

	sptr = (const char *)m_url;
	len = gf_utf8_mbstowcs(wurl, MAXLEN_URL, &sptr);
    V_BSTR(&value) = SysAllocStringLen(NULL, len+1);
	memcpy(V_BSTR(&value) , wurl, len*sizeof(u16));
	V_BSTR(&value) [len] = 0;
	
    pPropBag->Write(OLESTR("src"), &value);
    VariantClear(&value);
    return S_OK;
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

STDMETHODIMP CGPAXPlugin::Update(BSTR _mtype, BSTR _updates)
{
    if (m_term) {
		u16 *srcp;
		u32 len;
		char mtype[1024], *updates;

		srcp = (u16 *) _mtype;
		len = gf_utf8_wcstombs(mtype, 1024, (const u16 **)&srcp);
		mtype[len] = 0;

		srcp = (u16 *)_updates;
		len = gf_utf8_wcstombs(NULL, 0, (const u16 **)&srcp);
		if (len) {
			updates = (char *) malloc(sizeof(char) * (len+1));
			srcp = (u16 *)_updates;
			len = gf_utf8_wcstombs(updates, len, (const u16 **)&srcp);
			updates[len] = 0;
			gf_term_scene_update(m_term, mtype, updates);
			free(updates);
		}
	}
    return S_OK;
}

STDMETHODIMP CGPAXPlugin::get_src(BSTR *url)
{
	u16 wurl[MAXLEN_URL];
	const char *sptr;
	u16 len;
    if (url==NULL) return E_POINTER;

	sptr = (const char *)m_url;
	len = gf_utf8_mbstowcs(wurl, MAXLEN_URL, &sptr);
    *url = SysAllocStringLen(NULL, len+1);
	memcpy(*url, wurl, len*sizeof(u16));
	*url[len] = 0;
    return S_OK;
}
STDMETHODIMP CGPAXPlugin::put_src(BSTR url)
{
	const u16 *srcp = (const u16 *)url;
	u32 len = gf_utf8_wcstombs(m_url, MAXLEN_URL, &srcp);
	m_url[len] = 0;
	UpdateURL();
    return S_OK;
}

STDMETHODIMP CGPAXPlugin::get_AutoStart(VARIANT_BOOL *as)
{
    if (as==NULL) return E_POINTER;
    *as = m_bAutoStart ? VARIANT_TRUE: VARIANT_FALSE;
    return S_OK;
}
STDMETHODIMP CGPAXPlugin::put_AutoStart(VARIANT_BOOL as)
{
    m_bAutoStart = (as !=VARIANT_FALSE) ? TRUE: FALSE;
    return S_OK;
}

STDMETHODIMP CGPAXPlugin::GetInterfaceSafetyOptions(      
    REFIID riid,
    DWORD *pdwSupportedOptions,
    DWORD *pdwEnabledOptions
)
{
    if( (NULL == pdwSupportedOptions) || (NULL == pdwEnabledOptions) )
        return E_POINTER;

    *pdwSupportedOptions = INTERFACESAFE_FOR_UNTRUSTED_DATA|INTERFACESAFE_FOR_UNTRUSTED_CALLER;

    if ((IID_IDispatch == riid) || (IID_IGPAX == riid)) {
        *pdwEnabledOptions = INTERFACESAFE_FOR_UNTRUSTED_CALLER;
        return NOERROR;
    }
    else if (IID_IPersistPropertyBag == riid)  {
        *pdwEnabledOptions = INTERFACESAFE_FOR_UNTRUSTED_DATA;
        return NOERROR;
    }
    *pdwEnabledOptions = 0;
    return E_NOINTERFACE;
};

STDMETHODIMP CGPAXPlugin::SetInterfaceSafetyOptions(      
    REFIID riid,
    DWORD dwOptionSetMask,
    DWORD dwEnabledOptions
)
{
    if ((IID_IDispatch == riid) || (IID_IGPAX == riid) ) {
        if( (INTERFACESAFE_FOR_UNTRUSTED_CALLER == dwOptionSetMask)
         && (INTERFACESAFE_FOR_UNTRUSTED_CALLER == dwEnabledOptions) ) {
            return NOERROR;
        }
        return E_FAIL;
    }
    else if (IID_IPersistPropertyBag == riid) {
        if( (INTERFACESAFE_FOR_UNTRUSTED_DATA == dwOptionSetMask)
         && (INTERFACESAFE_FOR_UNTRUSTED_DATA == dwEnabledOptions) ) {
            return NOERROR;
        }
        return E_FAIL;
    }
    return E_FAIL;
};

