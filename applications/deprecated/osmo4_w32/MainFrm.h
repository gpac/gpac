// MainFrm.h : interface of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_MAINFRM_H__3666B63B_D886_4F0B_9953_A2AF09E3C15A__INCLUDED_)
#define AFX_MAINFRM_H__3666B63B_D886_4F0B_9953_A2AF09E3C15A__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <gpac/tools.h>
#include <gpac/constants.h>
#include <gpac/modules/service.h>
#include <gpac/options.h>

#include "FileProps.h"
#include "Options.h"
#include "AddressBar.h"
#include "Sliders.h"
#include "Playlist.h"


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

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMainFrame)
public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
	virtual BOOL DestroyWindow();
protected:
	virtual BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CMainFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

public:
	CStatusBar  m_wndStatusBar;
	CToolBar    m_wndToolBar;
	Sliders m_Sliders;
	AddressBar m_Address;
	CFileProps *m_pProps;
	COptions *m_pOpt;
	Playlist *m_pPlayList;
	CChildView *m_pWndView;
	Bool m_bFullScreen;
	u32 m_RestoreFS;
	UINT_PTR m_timer_on;
	CString console_message;
	CString console_service;
	GF_Err console_err;
	u32 m_aspect_ratio;
	RECT backup_wnd_rc;
	Bool m_bFirstStreamQuery;
	/*filter progress events to avoid killing importers with status bar text display...*/
	s32 m_last_prog;
	Bool m_show_rti;
	Bool m_bStartupFile;

public:
	void SetFullscreen();
	void BuildViewList();
	void BuildStreamList(Bool reset_ony);
	void BuildChapterList(Bool reset_ony);
	void SetProgTimer(Bool bOn);
	void AddSubtitle(const char *fileName, Bool auto_play);

private:
	void ForwardMessage();
	HICON m_icoerror, m_icomessage;
	s32 nb_viewpoints;
	Bool m_bInitShow;

	void SetNavigate(u32 mode);
	void LookForSubtitles();

	Double *m_chapters_start;
	u32 m_num_chapters;


// Generated message map functions
protected:
	//{{AFX_MSG(CMainFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSetFocus(CWnd *pOldWnd);
	afx_msg void OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu);
	afx_msg BOOL OnCommand(WPARAM wParam, LPARAM lParam);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnMove(int x, int y);
	afx_msg LRESULT OnSetSize(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnNavigate(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT Open(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT NewInstanceOpened(WPARAM wParam, LPARAM lParam);

	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnChar( UINT nChar, UINT nRepCnt, UINT nFlags );
	afx_msg void OnSysKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags );
	afx_msg void OnSysKeyUp( UINT nChar, UINT nRepCnt, UINT nFlags );
	afx_msg void OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags );
	afx_msg void OnKeyUp( UINT nChar, UINT nRepCnt, UINT nFlags );
	afx_msg void OnDropFiles(HDROP hDropInfo);
	afx_msg LRESULT OnConsoleMessage(WPARAM wParam, LPARAM lParam);
	afx_msg void OnViewOriginal();
	afx_msg void OnViewFullscreen();
	afx_msg void OnArKeep();
	afx_msg void OnArFill();
	afx_msg void OnAr43();
	afx_msg void OnAr169();
	afx_msg void OnUpdateAr169(CCmdUI* pCmdUI);
	afx_msg void OnUpdateAr43(CCmdUI* pCmdUI);
	afx_msg void OnUpdateArFill(CCmdUI* pCmdUI);
	afx_msg void OnUpdateArKeep(CCmdUI* pCmdUI);
	afx_msg void OnNavigateNone();
	afx_msg void OnNavigateWalk();
	afx_msg void OnNavigateFly();
	afx_msg void OnNavigateExam();
	afx_msg void OnNavigateSlide();
	afx_msg void OnNavigatePan();
	afx_msg void OnNavigateOrbit();
	afx_msg void OnNavigateGame();
	afx_msg void OnNavigateVR();
	afx_msg void OnNavigateReset();
	afx_msg void OnShortcuts();
	afx_msg void OnConfigure();
	afx_msg void OnFileProp();
	afx_msg void OnViewPlaylist();
	afx_msg void OnUpdateFileProp(CCmdUI* pCmdUI);
	afx_msg void OnUpdateNavigate(CCmdUI* pCmdUI);
	afx_msg void OnCacheEnable();
	afx_msg void OnUpdateCacheEnable(CCmdUI* pCmdUI);
	afx_msg void OnCacheStop();
	afx_msg void OnCacheAbort();
	afx_msg void OnUpdateCacheStop(CCmdUI* pCmdUI);
	afx_msg void OnCollideDisp();
	afx_msg void OnUpdateCollideDisp(CCmdUI* pCmdUI);
	afx_msg void OnCollideNone();
	afx_msg void OnUpdateCollideNone(CCmdUI* pCmdUI);
	afx_msg void OnCollideReg();
	afx_msg void OnUpdateCollideReg(CCmdUI* pCmdUI);
	afx_msg void OnHeadlight();
	afx_msg void OnUpdateHeadlight(CCmdUI* pCmdUI);
	afx_msg void OnGravity();
	afx_msg void OnUpdateGravity(CCmdUI* pCmdUI);
	afx_msg void OnNavInfo();
	afx_msg void OnNavNext();
	afx_msg void OnNavPrev();
	afx_msg void OnUpdateNavNext(CCmdUI* pCmdUI);
	afx_msg void OnUpdateNavPrev(CCmdUI* pCmdUI);
	afx_msg void OnClearNav();
	afx_msg void OnUpdateViewPlaylist(CCmdUI* pCmdUI);
	afx_msg void OnPlaylistLoop();
	afx_msg void OnUpdatePlaylistLoop(CCmdUI* pCmdUI);
	afx_msg void OnAddSubtitle();
	afx_msg void OnFileExit();
	afx_msg void OnViewCPU();
	afx_msg void OnUpdateViewCPU(CCmdUI* pCmdUI);
	afx_msg void OnFileCopy();
	afx_msg void OnUpdateFileCopy(CCmdUI* pCmdUI);
	afx_msg void OnFilePaste();
	afx_msg void OnUpdateFilePaste(CCmdUI* pCmdUI);

	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MAINFRM_H__3666B63B_D886_4F0B_9953_A2AF09E3C15A__INCLUDED_)
