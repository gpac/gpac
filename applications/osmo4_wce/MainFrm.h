// MainFrm.h : interface of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_MAINFRM_H__1DEE4BC7_6B56_48A8_BDD7_5DC14EF6AD3E__INCLUDED_)
#define AFX_MAINFRM_H__1DEE4BC7_6B56_48A8_BDD7_5DC14EF6AD3E__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "ProgressBar.h"


class CChildView : public CWnd
{
// Construction
public:
	CChildView();

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CChildView)
	protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CChildView();
	// Generated message map functions
protected:
	//{{AFX_MSG(CChildView)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

class CMainFrame : public CFrameWnd
{
public:
	CMainFrame();



protected: 
	DECLARE_DYNAMIC(CMainFrame)

// Attributes
public:

	ProgressBar    m_progBar;
	Bool m_full_screen, m_restore_fs, m_view_timing;
	u32 m_timer_on;
	CString console_message;
	GF_Err console_err;
	u32 m_aspect_ratio;

// Operations
public:
	void SetPauseButton(Bool force_play_button = 0);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMainFrame)
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	afx_msg void OnSetFocus(CWnd *pOldWnd);
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CMainFrame();

/*
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif
*/

protected:  // control bar embedded members

#if (_MSC_VER >= 1300)
	CCommandBar	m_wndCommandBar;
#else
	CCeCommandBar	m_wndCommandBar;
#endif

	void CloseURL();
	void ForwardMessage();

private:
	RECT m_view_rc;

public:
	/*m_dumbWnd is used to clean the screen...*/
	CChildView m_wndView, m_dumbWnd;
	void UpdateTime();

// Generated message map functions
protected:
	//{{AFX_MSG(CMainFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnAppExit();
	afx_msg LONG Open(WPARAM wParam, LPARAM lParam);
	afx_msg LONG OnSIPChange(WPARAM wParam, LPARAM lParam);
	afx_msg LONG OnSetSize(WPARAM wParam, LPARAM lParam);
	afx_msg LONG OnNavigate(WPARAM wParam, LPARAM lParam);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnFileStep();
	afx_msg void OnUpdateFileStep(CCmdUI* pCmdUI);
	afx_msg void OnFilePause();
	afx_msg void OnUpdateFilePause(CCmdUI* pCmdUI);
	afx_msg void OnFileStop();
	afx_msg void OnUpdateFileStop(CCmdUI* pCmdUI);
	afx_msg void OnViewFullscreen();
	afx_msg void OnUpdateViewFullscreen(CCmdUI* pCmdUI);
	afx_msg void OnClose();
	afx_msg void OnViewFit();
	afx_msg void OnUpdateViewFit(CCmdUI* pCmdUI);
	afx_msg void OnViewArOrig();
	afx_msg void OnViewArFill();
	afx_msg void OnViewAr43();
	afx_msg void OnViewAr169();
	afx_msg void OnNavNone();
	afx_msg void OnNavSlide();
	afx_msg void OnNaveReset();
	afx_msg void OnSetNavigation(UINT nID);

	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnViewTiming();
	afx_msg void OnUpdateViewTiming(CCmdUI* pCmdUI);
	afx_msg void OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft eMbedded Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MAINFRM_H__1DEE4BC7_6B56_48A8_BDD7_5DC14EF6AD3E__INCLUDED_)
