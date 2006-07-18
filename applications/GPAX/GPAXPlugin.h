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

#ifndef __GPAXPLUGIN_H_
#define __GPAXPLUGIN_H_

#define MAXLEN_URL     300

#include "resource.h"       // main symbols
#include <atlctl.h>
#include <SHLGUID.h>



#include <gpac/terminal.h>
#include <gpac/user.h>
#include <gpac/config.h>
#include <gpac/module.h>
#include <gpac/options.h>

Bool GPAX_EventProc(void *ptr, GF_Event *evt);

/////////////////////////////////////////////////////////////////////////////
// CGPAXPlugin
class ATL_NO_VTABLE CGPAXPlugin :
            public CComObjectRootEx<CComSingleThreadModel>,
            public IDispatchImpl<IGPAX, &IID_IGPAX, &LIBID_GPAXLib>,
            public CComControl<CGPAXPlugin>,
            public IPersistStreamInitImpl<CGPAXPlugin>,
            public IOleControlImpl<CGPAXPlugin>,
            public IOleObjectImpl<CGPAXPlugin>,
            public IOleInPlaceActiveObjectImpl<CGPAXPlugin>,
            public IViewObjectExImpl<CGPAXPlugin>,
            public IOleInPlaceObjectWindowlessImpl<CGPAXPlugin>,
            public ISupportErrorInfo,
            public IConnectionPointContainerImpl<CGPAXPlugin>,
            public IPersistStorageImpl<CGPAXPlugin>,
            public ISpecifyPropertyPagesImpl<CGPAXPlugin>,
            public IQuickActivateImpl<CGPAXPlugin>,
            public IDataObjectImpl<CGPAXPlugin>,
            public IProvideClassInfo2Impl<&CLSID_GPAX, &DIID_IGPAXEvents, &LIBID_GPAXLib>,
            public IPropertyNotifySinkCP<CGPAXPlugin>,
            public CComCoClass<CGPAXPlugin, &CLSID_GPAX>,
			public IPersistPropertyBagImpl<CGPAXPlugin>,
			public IObjectSafetyImpl<CGPAXPlugin, INTERFACESAFE_FOR_UNTRUSTED_CALLER>

{
public:
    CGPAXPlugin() {
		m_term = NULL;
		m_bAutoPlay = TRUE;
		m_bInitialDraw = TRUE;
		m_bWindowOnly = TRUE;  //to declare that the control is a window control in order
							   //to inherit the member variable m_hWnd which contains the window handler
		m_bIsConnected = 0;
		m_bUse3D = 0;
		m_AR = GF_ASPECT_RATIO_KEEP;
		m_url[0] = 0;
		m_pBrowser = NULL;
    }

	~CGPAXPlugin();

	Bool EventProc(GF_Event *evt);


    DECLARE_REGISTRY_RESOURCEID(IDR_GPAXPLUGIN)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(CGPAXPlugin)
    COM_INTERFACE_ENTRY(IGPAX)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IViewObjectEx)
    COM_INTERFACE_ENTRY(IViewObject2)
    COM_INTERFACE_ENTRY(IViewObject)
    COM_INTERFACE_ENTRY(IOleInPlaceObjectWindowless)
    COM_INTERFACE_ENTRY(IOleInPlaceObject)
    COM_INTERFACE_ENTRY2(IOleWindow, IOleInPlaceObjectWindowless)
    COM_INTERFACE_ENTRY(IOleInPlaceActiveObject)
    COM_INTERFACE_ENTRY(IOleControl)
    COM_INTERFACE_ENTRY(IOleObject)
    COM_INTERFACE_ENTRY(IPersistStreamInit)
    COM_INTERFACE_ENTRY2(IPersist, IPersistStreamInit)
    COM_INTERFACE_ENTRY(ISupportErrorInfo)
    COM_INTERFACE_ENTRY(IConnectionPointContainer)
    COM_INTERFACE_ENTRY(ISpecifyPropertyPages)
    COM_INTERFACE_ENTRY(IQuickActivate)
    COM_INTERFACE_ENTRY(IPersistStorage)
    COM_INTERFACE_ENTRY(IDataObject)
    COM_INTERFACE_ENTRY(IProvideClassInfo)
    COM_INTERFACE_ENTRY(IProvideClassInfo2)
	COM_INTERFACE_ENTRY(IObjectSafety)
	COM_INTERFACE_ENTRY(IPersistPropertyBag)
    END_COM_MAP()

    BEGIN_PROP_MAP(CGPAXPlugin)
    PROP_DATA_ENTRY("_cx", m_sizeExtent.cx, VT_UI4)
    PROP_DATA_ENTRY("_cy", m_sizeExtent.cy, VT_UI4)
    // Example entries
    // PROP_ENTRY("Property Description", dispid, clsid)
    // PROP_PAGE(CLSID_StockColorPage)
    END_PROP_MAP()

    BEGIN_CONNECTION_POINT_MAP(CGPAXPlugin)
    CONNECTION_POINT_ENTRY(IID_IPropertyNotifySink)
    END_CONNECTION_POINT_MAP()

    BEGIN_MSG_MAP(CGPAXPlugin)
    CHAIN_MSG_MAP(CComControl<CGPAXPlugin>)
    DEFAULT_REFLECTION_HANDLER()
	MESSAGE_HANDLER(WM_CREATE, OnCreate)

    END_MSG_MAP()
    // Handler prototypes:
    //  LRESULT MessageHandler(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    //  LRESULT CommandHandler(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    //  LRESULT NotifyHandler(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);



    // ISupportsErrorInfo
    STDMETHOD(InterfaceSupportsErrorInfo)(REFIID riid)
    {
        static const IID* arr[] =
            {
                &IID_IGPAX,
            };
        for (int i=0; i<sizeof(arr)/sizeof(arr[0]); i++)
        {
            if (InlineIsEqualGUID(*arr[i], riid))
                return S_OK;
        }
        return S_FALSE;
    }

    // IViewObjectEx
    DECLARE_VIEW_STATUS(VIEWSTATUS_SOLIDBKGND | VIEWSTATUS_OPAQUE)

    // IGPAX
public:
	//Interface methods
	STDMETHOD(Stop)();
	STDMETHOD(Pause)();
	STDMETHOD(Play)();
    STDMETHOD(Update)(BSTR mtype,BSTR updates);

	//Interface properties
    STDMETHODIMP get_src(BSTR *url);
    STDMETHODIMP put_src(BSTR url);
    STDMETHODIMP get_AutoStart(VARIANT_BOOL *as);
    STDMETHODIMP put_AutoStart(VARIANT_BOOL as);

	//Customed Window Message functions: OnCreate and OnDestroy are called when a window
	//is created or destroyed. OnDraw is to establish inital connection.
    HRESULT OnDraw(ATL_DRAWINFO& di);
    LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

	//IPersistPropertyBag method: to handle persist property packed and transfered by MSIE
	//in html doc, the ActiveX control is added by tags
	//   <object CLSID=...>
	//		<PARAM name="..." value="...">
	//   </object>
	//the interface IPersistPropertyBag enable MSIE and ActiveX Control to communicate these
	//properties included in tags <PARAM> 
    STDMETHODIMP Load(LPPROPERTYBAG pPropBag, LPERRORLOG pErrorLog);
    STDMETHODIMP Save(LPPROPERTYBAG, BOOL, BOOL);

private:
	Bool ReadParamString(LPPROPERTYBAG pPropBag, LPERRORLOG pErrorLog, WCHAR *name, TCHAR *buf, int bufsize);
	void SetStatusText(char *msg);

	GF_Terminal *m_term;
    GF_User m_user;
	TCHAR m_url[MAXLEN_URL];
	/*pointer to the parent browser if any*/
	IWebBrowser2 *m_pBrowser;

	u32 m_width, m_height, m_AR;
	Bool m_bIsConnected, m_bInitialDraw, m_bAutoPlay, m_bUse3D, m_bLoop;

};



#endif //__GPAXPLUGIN_H_
