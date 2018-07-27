#if !defined(AFX_OPTIONS_H__5C839953_58C0_4D9D_89CE_2820C7686C1B__INCLUDED_)
#define AFX_OPTIONS_H__5C839953_58C0_4D9D_89CE_2820C7686C1B__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// Options.h : header file
//


class COptAudio : public CDialog
{
// Construction
public:
	COptAudio(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(COptAudio)
	enum { IDD = IDD_OPT_AUDIO };
	CButton	m_Notifs;
	CComboBox	m_DriverList;
	CButton	m_AudioResync;
	CButton	m_AudioMultiCH;
	CEdit	m_AudioDur;
	CSpinButtonCtrl	m_SpinFPS;
	CButton	m_ForceConfig;
	CSpinButtonCtrl	m_AudioSpin;
	CEdit	m_AudioEdit;
	//}}AFX_DATA

	void SaveOptions();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(COptAudio)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(COptAudio)
	virtual BOOL OnInitDialog();
	afx_msg void OnForceAudio();
	afx_msg void OnSelchangeDriverList();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// OptDecoder dialog

class OptDecoder : public CDialog
{
// Construction
public:
	OptDecoder(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(OptDecoder)
	enum { IDD = IDD_OPT_DECODER };
	CComboBox	m_Video;
	CComboBox	m_Audio;
	//}}AFX_DATA

	void SaveOptions();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(OptDecoder)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(OptDecoder)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

class OptFiles : public CDialog
{
// Construction
public:
	OptFiles(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(OptFiles)
	enum { IDD = IDD_OPT_FILETYPES };
	CButton	m_DoAssociate;
	CStatic	m_PlugName;
	CStatic	m_mimes;
	CStatic	m_extensions;
	CComboBox	m_FileDescs;
	//}}AFX_DATA

	void SetSelection(u32 sel);
	char cur_ext[200], cur_mime[200];

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(OptFiles)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(OptFiles)
	virtual BOOL OnInitDialog();
	afx_msg void OnSelchangeFilelist();
	afx_msg void OnAssociate();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// COptFont dialog

class COptFont : public CDialog
{
// Construction
public:
	COptFont(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(COptFont)
	enum { IDD = IDD_OPT_FONT };
	CComboBox	m_TextureModes;
	CComboBox	m_Fonts;
	CButton	m_BrowseFont;
	//}}AFX_DATA


	void SaveOptions();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(COptFont)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(COptFont)
	virtual BOOL OnInitDialog();
	afx_msg void OnBrowseFont();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// COptGen dialog

class COptGen : public CDialog
{
// Construction
public:
	COptGen(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(COptGen)
	enum { IDD = IDD_OPT_GEN };
	CButton	m_LookForSubs;
	CButton	m_ViewXMT;
	CButton	m_NoConsole;
	CButton	m_Loop;
	CButton	m_SingleInstance;
	//}}AFX_DATA

	void SaveOptions();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(COptGen)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(COptGen)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////
// COptHTTP dialog

class COptHTTP : public CDialog
{
// Construction
public:
	COptHTTP(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(COptHTTP)
	enum { IDD = IDD_OPT_HTTP };
	CEdit	m_ProxyName;
	CButton	m_useProxy;
	CEdit	m_SAXDuration;
	CButton	m_Progressive;
	CButton	m_DisableCache;
	CButton	m_CleanCache;
	CButton	m_CacheDir;
	//}}AFX_DATA


	void SaveOptions();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(COptHTTP)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(COptHTTP)
	afx_msg void OnBrowseCache();
	virtual BOOL OnInitDialog();
	afx_msg void OnSaxProgressive();
	afx_msg void OnUseProxy();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedRestartCache();
};


class COptMCache : public CDialog
{
// Construction
public:
	COptMCache(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(COptMCache)
	enum { IDD = IDD_OPT_MCACHE };
	CEdit	m_BaseName;
	CButton	m_UseBase;
	CButton	m_Overwrite;
	CButton	m_RecDir;
	//}}AFX_DATA


	void SaveOptions();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(COptMCache)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(COptMCache)
	virtual BOOL OnInitDialog();
	afx_msg void OnBrowseMcache();
	afx_msg void OnMcacheUsename();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

class COptRender : public CDialog
{
// Construction
public:
	COptRender(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(COptRender)
	enum { IDD = IDD_OPT_RENDER };
	CComboBox	m_DrawBounds;
	CComboBox	m_Graphics;
	CButton	m_Use3DRender;
	CComboBox	m_AntiAlias;
	CButton	m_ForceSize;
	CButton	m_HighSpeed;
	CComboBox	m_BIFSRate;
	//}}AFX_DATA


	Bool SaveOptions();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(COptRender)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(COptRender)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

class COptRender2D : public CDialog
{
// Construction
public:
	COptRender2D(CWnd* pParent = NULL);   // standard constructor

	void SaveOptions();
	void SetYUV();

// Dialog Data
	//{{AFX_DATA(COptRender2D)
	enum { IDD = IDD_OPT_RENDER2D };
	CStatic	m_YUVFormat;
	CButton	m_NoYUV;
	CButton	m_Scalable;
	CButton	m_DirectRender;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(COptRender2D)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(COptRender2D)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

class COptRender3D : public CDialog
{
// Construction
public:
	COptRender3D(CWnd* pParent = NULL);   // standard constructor

	void SaveOptions();

// Dialog Data
	//{{AFX_DATA(COptRender3D)
	enum { IDD = IDD_OPT_RENDER3D };
	CButton	m_BitmapPixels;
	CButton	m_DisableTXRect;
	CButton	m_RasterOutlines;
	CButton	m_EmulPow2;
	CButton	m_PolyAA;
	CComboBox	m_BackCull;
	CComboBox	m_DrawNormals;
	CComboBox	m_Wireframe;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(COptRender3D)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(COptRender3D)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

class COptStream : public CDialog
{
// Construction
public:
	COptStream(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(COptStream)
	enum { IDD = IDD_OPT_STREAM };
	CEdit	m_RebufferLen;
	CButton	m_Rebuffer;
	CEdit	m_Buffer;
	CEdit	m_Timeout;
	CButton	m_Reorder;
	CButton	m_UseRTSP;
	CComboBox	m_Port;
	//}}AFX_DATA


	void SaveOptions();

	void CheckRebuffer();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(COptStream)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(COptStream)
	virtual BOOL OnInitDialog();
	afx_msg void OnSelchangePort();
	afx_msg void OnRtsp();
	afx_msg void OnRebuffer();
	afx_msg void OnUpdateRebufferLen();
	afx_msg void OnUpdateBuffer();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

class COptSystems : public CDialog
{
// Construction
public:
	COptSystems(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(COptSystems)
	enum { IDD = IDD_OPT_SYSTEMS };
	CButton	m_ForceDuration;
	CComboBox	m_Threading;
	CButton	m_LateFramesAlwaysDrawn;
	CComboBox	m_Lang;
	//}}AFX_DATA

	void SaveOptions();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(COptSystems)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(COptSystems)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

class COptVideo : public CDialog
{
// Construction
public:
	COptVideo(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(COptVideo)
	enum { IDD = IDD_OPT_VIDEO };
	CButton	m_SwitchRes;
	CButton	m_UseHWMemory;
	CComboBox	m_Videos;
	//}}AFX_DATA


	void SaveOptions();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(COptVideo)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(COptVideo)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// COptLogs dialog

class COptLogs : public CDialog
{
// Construction
public:
	COptLogs(CWnd* pParent = NULL);   // standard constructor
	void SaveOptions();

// Dialog Data
	//{{AFX_DATA(COptLogs)
	enum { IDD = IDD_OPT_LOGS };
	CButton	m_sync;
	CButton	m_script;
	CButton	m_scene;
	CButton	m_rtp;
	CButton	m_render;
	CButton	m_parser;
	CButton	m_net;
	CButton	m_mmio;
	CButton	m_media;
	CButton	m_core;
	CButton	m_container;
	CButton	m_compose;
	CButton	m_coding;
	CButton	m_codec;
	CButton	m_author;
	CComboBox	m_Level;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(COptLogs)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(COptLogs)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};



/////////////////////////////////////////////////////////////////////////////
// COptions dialog

class COptions : public CDialog
{
// Construction
public:
	COptions(CWnd* pParent = NULL);   // standard constructor
	BOOL Create(CWnd * pParent)
	{
		return CDialog::Create( COptions::IDD, pParent);
	}

// Dialog Data
	//{{AFX_DATA(COptions)
	enum { IDD = IDD_OPTIONS };
	CComboBox	m_Selector;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(COptions)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	COptGen m_general;
	COptSystems m_systems;
	COptRender m_render;
	COptRender2D m_render2d;
	COptRender3D m_render3d;
	COptAudio m_audio;
	OptDecoder m_decoder;
	COptVideo m_video;
	COptHTTP m_http;
	COptFont m_font;
	COptStream m_stream;
	COptMCache m_cache;
	OptFiles m_files;
	COptLogs m_logs;

	void HideAll();

	// Generated message map functions
	//{{AFX_MSG(COptions)
	virtual BOOL OnInitDialog();
	afx_msg void OnSaveopt();
	afx_msg void OnClose();
	afx_msg void OnDestroy();
	afx_msg void OnSelchangeSelect();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_OPTIONS_H__5C839953_58C0_4D9D_89CE_2820C7686C1B__INCLUDED_)
