// Osmo4.h : main header file for the OSMO4 application
//

#if !defined(AFX_OSMO4_H__7E4A02D1_F77D_4E97_9E10_032054B29E33__INCLUDED_)
#define AFX_OSMO4_H__7E4A02D1_F77D_4E97_9E10_032054B29E33__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#ifndef __AFXWIN_H__
#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// COsmo4:
// See Osmo4.cpp for the implementation of this class
//

/*MPEG4 term*/
#include <gpac/terminal.h>

enum {
	WM_SCENE_DONE = WM_USER + 1,
	WM_NAVIGATE,
	WM_SETSIZE,
	WM_OPENURL,
	WM_CONSOLEMSG,
};


#define IPAQ_TRANSLATE_KEY(vk)	(LOWORD(vk) != 0x5b ? LOWORD(vk) : vk )
/*navigation pad keys*/
#define VK_IPAQ_LEFT		0x25
#define VK_IPAQ_UP			0x26
#define VK_IPAQ_RIGHT		0x27
#define VK_IPAQ_DOWN		0x28
/*"enter" key*/
#define VK_IPAQ_START		0x86
/*ipaq keys from left to right*/
#define VK_IPAQ_A			0xC1
#define VK_IPAQ_B			0xC2
#define VK_IPAQ_C			0xC3
#define VK_IPAQ_D			0xC4
/*record button*/
#define VK_IPAQ_E			0xC5

class COsmo4 : public CWinApp
{
public:
	COsmo4();

	GF_Terminal *m_term;
	GF_User m_user;
	CString m_filename;

	u32 m_duration;
	CString m_navigate_url;
	Bool m_Loop, m_fit_screen, m_can_seek, m_open, m_disable_backlight, m_stopped, m_no_mime_fetch;
	void Pause();

	u32 m_scene_width, m_scene_height, m_reconnect_time;
	u32 m_screen_width, m_screen_height, m_menu_height;
	/*task bar on/off*/
	void ShowTaskBar(Bool showIt, Bool pause_only = 0);

	CString GetFileFilter();

	void SetBacklightState(Bool disable);
	void EnableLogs(Bool turn_on);

private:
	Bool m_DoResume;

	void SetOptions();

	/*power management*/
	u32 m_prev_batt_bl, m_prev_ac_bl;

	FILE *m_logs;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(COsmo4)
public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(COsmo4)
	afx_msg void OnAppAbout();
	afx_msg void OnConfigure();
	afx_msg void OnOpenFile();
	afx_msg void OnOpenUrl();
	afx_msg void OnShortcuts();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

inline COsmo4 *GetApp() {
	return (COsmo4 *)AfxGetApp();
}

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft eMbedded Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_OSMO4_H__7E4A02D1_F77D_4E97_9E10_032054B29E33__INCLUDED_)
