#if !defined(AFX_OPTIONS_H__5C839953_58C0_4D9D_89CE_2820C7686C1B__INCLUDED_)
#define AFX_OPTIONS_H__5C839953_58C0_4D9D_89CE_2820C7686C1B__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// Options.h : header file
//


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
	CButton	m_BifsAlwaysDrawn;
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
class COptRender : public CDialog
{
// Construction
public:
	COptRender(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(COptRender)
	enum { IDD = IDD_OPT_RENDER };
	CComboBox	m_Antialias;
	CButton	m_ForceSize;
	CButton	m_HighSpeed;
	CButton	m_Scalable;
	CButton	m_DirectRender;
	CComboBox	m_BIFSRate;
	//}}AFX_DATA


	void SaveOptions();

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
class COptHTTP : public CDialog
{
// Construction
public:
	COptHTTP(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(COptHTTP)
	enum { IDD = IDD_OPT_HTTP };
	CButton	m_RestartFile;
	CButton	m_Progressive;
	CButton	m_CleanCache;
	CButton	m_CacheDir;
	CEdit	m_SaxDuration;
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
	afx_msg void OnProgressive();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
class COptGen : public CDialog
{
// Construction
public:
	COptGen(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(COptGen)
	enum { IDD = IDD_OPT_GEN };
	CButton	m_NoBacklight;
	CButton	m_Fill;
	CButton	m_Loop;
	CButton	m_Logs;
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
	afx_msg void OnFileassoc();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
class COptFont : public CDialog
{
// Construction
public:
	COptFont(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(COptFont)
	enum { IDD = IDD_OPT_FONT };
	CButton	m_UseTexture;
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
class COptDecoder : public CDialog
{
// Construction
public:
	COptDecoder(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(COptDecoder)
	enum { IDD = IDD_OPT_DECODER };
	CComboBox	m_Video;
	CComboBox	m_Audio;
	//}}AFX_DATA

	void SaveOptions();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(COptDecoder)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(COptDecoder)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

class COptAudio : public CDialog
{
// Construction
public:
	COptAudio(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(COptAudio)
	enum { IDD = IDD_OPT_AUDIO };
	CComboBox	m_DriverList;
	CButton	m_AudioResync;
	CEdit	m_AudioDur;
	CSpinButtonCtrl	m_SpinDur;
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
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


class COptRender3D : public CDialog
{
// Construction
public:
	COptRender3D(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(COptRender)
	enum { IDD = IDD_OPT_RENDER3D };
	CComboBox	m_WireMode;
	CComboBox	m_DrawNormals;
	CButton	m_Use3DRender;
	CButton	m_NoBackFace;
	CButton	m_EmulatePOW2;
	//}}AFX_DATA

	void SaveOptions();

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

/////////////////////////////////////////////////////////////////////////////
// COptions dialog

class COptions : public CDialog
{
// Construction
public:
	COptions(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(COptions)
	enum { IDD = IDD_OPTIONS };
	CComboBox	m_Selection;
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
	COptRender3D m_render3D;
	COptAudio m_audio;
	COptHTTP m_http;
	COptFont m_font;
	COptStream m_stream;
	COptDecoder m_decoder;


	void HideAll();

	// Generated message map functions
	//{{AFX_MSG(COptions)
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnSaveopt();
	afx_msg void OnSelchangeCombosel();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_OPTIONS_H__5C839953_58C0_4D9D_89CE_2820C7686C1B__INCLUDED_)
