// GPAC.h : main header file for the GPAC application
//

#if !defined(AFX_GPAC_H__8B06A368_E142_47E3_ABE7_0B459FC0E853__INCLUDED_)
#define AFX_GPAC_H__8B06A368_E142_47E3_ABE7_0B459FC0E853__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// Osmo4:
// See GPAC.cpp for the implementation of this class
//


/*GPAC terminal*/
#include <gpac/terminal.h>
/*GPAC terminal info (OD browsing)*/
#include <gpac/term_info.h>

enum {
	WM_SCENE_DONE = WM_USER + 1, 
	WM_NAVIGATE,
	WM_SETSIZE,
	WM_OPENURL,
	WM_RESTARTURL,
	WM_CONSOLEMSG,
	WM_NEWINSTANCE,
};

const char *static_gpac_get_url();

class Osmo4 : public CWinApp
{
public:
	Osmo4();

	GF_Terminal *m_term;
	GF_User m_user;

	Bool m_isopen, m_reset;
	u32 max_duration;
	Bool can_seek;
	u32 orig_width,orig_height, m_reconnect_time; 

	CString m_navigate_url;
	void Pause();
	void PlayFromTime(u32 time);

	void SetOptions();
	void UpdateRenderSwitch();
	void UpdatePlayButton(Bool force_play = 0);

	/*general options*/
	Bool m_Loop, m_LookForSubtitles, m_NoConsole, m_ViewXMTA, m_SingleInstance, m_NoMimeFetch;
	u32 start_mode;

	CString GetFileFilter();
	
	char szAppPath[GF_MAX_PATH];

	FILE *m_logs;
	u32 m_log_level, m_log_tools;

	HANDLE m_hMutex;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(Osmo4)
	public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();
	//}}AFX_VIRTUAL

// Implementation

public:
	//{{AFX_MSG(Osmo4)
	afx_msg void OnOpenFile();
	afx_msg void OnMainPause();
	afx_msg void OnFileStep();
	afx_msg void OnOpenUrl();
	afx_msg void OnFileReload();
	afx_msg void OnFileMigrate();
	afx_msg void OnConfigReload();
	afx_msg void OnFilePlay();
	afx_msg void OnUpdateFilePlay(CCmdUI* pCmdUI);
	afx_msg void OnUpdateFileStep(CCmdUI* pCmdUI);
	afx_msg void OnFileStop();
	afx_msg void OnUpdateFileStop(CCmdUI* pCmdUI);
	afx_msg void OnSwitchRender();
	afx_msg void OnAbout();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

inline Osmo4 *GetApp() { return (Osmo4 *)AfxGetApp(); }



/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GPAC_H__8B06A368_E142_47E3_ABE7_0B459FC0E853__INCLUDED_)
	