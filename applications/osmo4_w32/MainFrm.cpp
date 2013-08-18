// MainFrm.cpp : implementation of the CMainFrame class
//

#include "stdafx.h"
#include "Osmo4.h"

#include "MainFrm.h"
#include "resource.h"

#include <gpac/network.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CChildView

CChildView::CChildView()
{
}

CChildView::~CChildView()
{
	/*since the wndproc is overwritten by the terminal, we detach the handle otherwise we get a nice assertion
	failure from windows*/
	HWND hWnd = Detach();
	::PostMessage(hWnd, WM_QUIT, 0, 0);
}


BEGIN_MESSAGE_MAP(CChildView,CWnd )
	//{{AFX_MSG_MAP(CChildView)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CChildView message handlers

BOOL CChildView::PreCreateWindow(CREATESTRUCT& cs) 
{
	cs.dwExStyle = 0;
	cs.style &= ~WS_BORDER;

	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, 
		::LoadCursor(NULL, IDC_ARROW), HBRUSH(COLOR_WINDOW+1), NULL);

	return TRUE;
}



/////////////////////////////////////////////////////////////////////////////
// CMainFrame

IMPLEMENT_DYNAMIC(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
	//{{AFX_MSG_MAP(CMainFrame)
	ON_WM_CREATE()
	ON_WM_SETFOCUS()
	ON_WM_INITMENUPOPUP()
	ON_WM_SIZE()
	ON_WM_MOVE()
	ON_MESSAGE(WM_SETSIZE,OnSetSize)
	ON_MESSAGE(WM_NAVIGATE,OnNavigate)
	ON_MESSAGE(WM_OPENURL, Open)
	ON_MESSAGE(WM_NEWINSTANCE, NewInstanceOpened)
	
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_LBUTTONUP()
	ON_WM_CHAR()
	ON_WM_SYSKEYDOWN()
	ON_WM_SYSKEYUP()
	ON_WM_KEYDOWN()
	ON_WM_KEYUP()
	ON_WM_DROPFILES()
	ON_MESSAGE(WM_CONSOLEMSG, OnConsoleMessage)
	ON_COMMAND(ID_VIEW_ORIGINAL, OnViewOriginal)
	ON_COMMAND(ID_VIEW_FULLSCREEN, OnViewFullscreen)
	ON_COMMAND(ID_AR_KEEP, OnArKeep)
	ON_COMMAND(ID_AR_FILL, OnArFill)
	ON_COMMAND(ID_AR_43, OnAr43)
	ON_COMMAND(ID_AR_169, OnAr169)
	ON_UPDATE_COMMAND_UI(ID_AR_169, OnUpdateAr169)
	ON_UPDATE_COMMAND_UI(ID_AR_43, OnUpdateAr43)
	ON_UPDATE_COMMAND_UI(ID_AR_FILL, OnUpdateArFill)
	ON_UPDATE_COMMAND_UI(ID_AR_KEEP, OnUpdateArKeep)
	ON_COMMAND(ID_NAVIGATE_NONE, OnNavigateNone)
	ON_COMMAND(ID_NAVIGATE_WALK, OnNavigateWalk)
	ON_COMMAND(ID_NAVIGATE_FLY, OnNavigateFly)
	ON_COMMAND(ID_NAVIGATE_EXAM, OnNavigateExam)
	ON_COMMAND(ID_NAVIGATE_SLIDE, OnNavigateSlide)
	ON_COMMAND(ID_NAVIGATE_PAN, OnNavigatePan)
	ON_COMMAND(ID_NAVIGATE_ORBIT, OnNavigateOrbit)
	ON_COMMAND(ID_NAVIGATE_GAME, OnNavigateGame)
	ON_COMMAND(ID_NAVIGATE_VR, OnNavigateVR)
	ON_COMMAND(ID_NAV_RESET, OnNavigateReset)
	ON_COMMAND(ID_SHORTCUTS, OnShortcuts)
	ON_COMMAND(IDD_CONFIGURE, OnConfigure)
	ON_COMMAND(ID_FILE_PROP, OnFileProp)
	ON_COMMAND(ID_VIEW_PL, OnViewPlaylist)
	ON_UPDATE_COMMAND_UI(ID_FILE_PROP, OnUpdateFileProp)
	ON_UPDATE_COMMAND_UI(ID_NAVIGATE_NONE, OnUpdateNavigate)
	ON_COMMAND(ID_REC_ENABLE, OnCacheEnable)
	ON_UPDATE_COMMAND_UI(ID_REC_ENABLE, OnUpdateCacheEnable)
	ON_COMMAND(ID_REC_STOP, OnCacheStop)
	ON_COMMAND(ID_REC_ABORT, OnCacheAbort)
	ON_UPDATE_COMMAND_UI(ID_REC_STOP, OnUpdateCacheStop)
	ON_COMMAND(ID_COLLIDE_DISP, OnCollideDisp)
	ON_UPDATE_COMMAND_UI(ID_COLLIDE_DISP, OnUpdateCollideDisp)
	ON_COMMAND(ID_COLLIDE_NONE, OnCollideNone)
	ON_UPDATE_COMMAND_UI(ID_COLLIDE_NONE, OnUpdateCollideNone)
	ON_COMMAND(ID_COLLIDE_REG, OnCollideReg)
	ON_UPDATE_COMMAND_UI(ID_COLLIDE_REG, OnUpdateCollideReg)
	ON_COMMAND(ID_HEADLIGHT, OnHeadlight)
	ON_UPDATE_COMMAND_UI(ID_HEADLIGHT, OnUpdateHeadlight)
	ON_COMMAND(ID_GRAVITY, OnGravity)
	ON_UPDATE_COMMAND_UI(ID_GRAVITY, OnUpdateGravity)
	ON_COMMAND(ID_NAV_INFO, OnNavInfo)
	ON_COMMAND(ID_NAV_NEXT, OnNavNext)
	ON_COMMAND(ID_NAV_PREV, OnNavPrev)
	ON_UPDATE_COMMAND_UI(ID_NAV_NEXT, OnUpdateNavNext)
	ON_UPDATE_COMMAND_UI(ID_NAV_PREV, OnUpdateNavPrev)
	ON_COMMAND(ID_CLEAR_NAV, OnClearNav)
	ON_UPDATE_COMMAND_UI(ID_VIEW_PL, OnUpdateViewPlaylist)
	ON_COMMAND(ID_PLAYLIST_LOOP, OnPlaylistLoop)
	ON_UPDATE_COMMAND_UI(ID_PLAYLIST_LOOP, OnUpdatePlaylistLoop)
	ON_COMMAND(ID_ADD_SUBTITLE, OnAddSubtitle)
	ON_UPDATE_COMMAND_UI(ID_REC_ABORT, OnUpdateCacheStop)
	ON_UPDATE_COMMAND_UI(ID_NAVIGATE_WALK, OnUpdateNavigate)
	ON_UPDATE_COMMAND_UI(ID_NAVIGATE_FLY, OnUpdateNavigate)
	ON_UPDATE_COMMAND_UI(ID_NAVIGATE_EXAM, OnUpdateNavigate)
	ON_UPDATE_COMMAND_UI(ID_NAVIGATE_PAN, OnUpdateNavigate)
	ON_UPDATE_COMMAND_UI(ID_NAVIGATE_SLIDE, OnUpdateNavigate)
	ON_UPDATE_COMMAND_UI(ID_NAVIGATE_ORBIT, OnUpdateNavigate)
	ON_UPDATE_COMMAND_UI(ID_NAVIGATE_VR, OnUpdateNavigate)
	ON_UPDATE_COMMAND_UI(ID_NAVIGATE_GAME, OnUpdateNavigate)
	ON_COMMAND(ID_FILE_EXIT, OnFileExit)
	ON_COMMAND(ID_VIEW_CPU, OnViewCPU)
	ON_UPDATE_COMMAND_UI(ID_VIEW_CPU, OnUpdateViewCPU)

	ON_COMMAND(ID_FILE_COPY, OnFileCopy)
	ON_UPDATE_COMMAND_UI(ID_FILE_COPY, OnUpdateFileCopy)
	ON_COMMAND(ID_FILE_PASTE, OnFilePaste)
	ON_UPDATE_COMMAND_UI(ID_FILE_PASTE, OnUpdateFilePaste)
	
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CMainFrame construction/destruction

CMainFrame::CMainFrame()
{
	m_icoerror = AfxGetApp()->LoadIcon(IDI_ERR);
	m_icomessage = AfxGetApp()->LoadIcon(IDI_MESSAGE);
	m_bFullScreen = GF_FALSE;	
	m_RestoreFS = 0;
	m_aspect_ratio = GF_ASPECT_RATIO_KEEP;
	m_pProps = NULL;
	m_pOpt = NULL;
	m_pPlayList = NULL;
	m_pWndView = new CChildView();
	m_bInitShow = GF_TRUE;
	m_bStartupFile = GF_TRUE;
	m_num_chapters = 0;
	m_chapters_start = NULL;
	m_last_prog = -1;
	m_timer_on = 0;
	m_show_rti = GF_FALSE;
	nb_viewpoints = 0;
}

CMainFrame::~CMainFrame()
{
	if (m_chapters_start) gf_free(m_chapters_start);
	if (m_pProps != NULL) m_pProps->DestroyWindow();
	if (m_pOpt != NULL) m_pOpt->DestroyWindow();
	if (m_pPlayList != NULL) delete m_pPlayList;
	delete m_pWndView;
}

#define RTI_TIMER	22
#define RTI_REFRESH_MS		250

void CALLBACK EXPORT RTInfoTimer(HWND , UINT , UINT_PTR nID , DWORD )
{
	char szMsg[100];
	GF_SystemRTInfo rti;
	if (nID != RTI_TIMER) return;
	Osmo4 *app = GetApp();
	CMainFrame *pFrame = (CMainFrame *) app->m_pMainWnd;
	/*shutdown*/
	if (!pFrame) return;

	if (pFrame->m_show_rti && !pFrame->m_timer_on) {
		if (!gf_sys_get_rti(RTI_REFRESH_MS, &rti, 0)) return;
		if (!rti.gpac_memory) rti.gpac_memory = rti.process_memory ? rti.process_memory : rti.physical_memory;

		if (pFrame->m_show_rti && !pFrame->m_timer_on) {
			sprintf(szMsg, "FPS %02.2f - CPU %02d (%02d) - Mem %d kB", 
						gf_term_get_framerate(app->m_term, GF_FALSE), rti.total_cpu_usage, rti.process_cpu_usage, rti.gpac_memory/1024);
			pFrame->m_wndStatusBar.SetPaneText(1, szMsg);
		}
	}

	if (! gf_term_get_option(app->m_term, GF_OPT_IS_FINISHED)) {
		u32 ms = gf_term_get_time_in_ms(app->m_term);
		u32 h = ms / 1000 / 3600;
		u32 m = ms / 1000 / 60 - h*60;
		u32 s = ms / 1000 - h*3600 - m*60;
		
		sprintf(szMsg, "%02d:%02d.%02d", h, m, s);
		pFrame->m_wndStatusBar.SetPaneText(0, szMsg);
	}
}

static UINT status_indics[] =
{
	ID_TIMER,
	ID_SEPARATOR,           // status line indicator
};



int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	UINT buttonArray[50];
	TBBUTTONINFO bi;
	u32 *ba;
	if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	// create a view to occupy the client area of the frame
	if (!m_pWndView->CreateEx(0, NULL, NULL, WS_CHILD, 0, 0, 300, 200, m_hWnd, NULL, NULL))
	{
		TRACE0("Failed to create view window\n");
		return -1;
	}
	m_pPlayList = new Playlist();
	m_pPlayList->Create();
	m_pPlayList->ShowWindow(SW_HIDE);


	if (!m_wndToolBar.CreateEx(this, WS_CHILD | CBRS_TOP | CBRS_FLYBY) ||
		!m_wndToolBar.LoadBitmap(IDR_MAINTOOLS))
	{
		TRACE0("Failed to create toolbar\n");
		return -1;      // fail to create
	}

	ba = &buttonArray[0];
	*ba = ID_OPEN_FILE; ba++;
	*ba = ID_SEPARATOR; ba++;
	*ba = ID_NAV_PREV; ba++;
	*ba = ID_NAV_NEXT; ba++;
	*ba = ID_SEPARATOR; ba++;
	*ba = ID_FILE_PLAY; ba++;
	*ba = ID_FILE_STEP; ba++;
	*ba = ID_FILE_STOP; ba++;
	*ba = ID_SEPARATOR; ba++;
	*ba = ID_FILE_PROP; ba++;
	*ba = ID_SEPARATOR; ba++;
	*ba = ID_FILE_PROP; ba++;
	*ba = ID_SWITCH_RENDER;
	m_wndToolBar.SetButtons(buttonArray, 13);
	m_wndToolBar.SetButtonInfo(0, ID_OPEN_FILE, TBBS_BUTTON, 0);
	m_wndToolBar.SetButtonInfo(1, ID_SEPARATOR, TBBS_SEPARATOR, 0);
	m_wndToolBar.SetButtonInfo(2, ID_NAV_PREV, TBBS_DROPDOWN, 1);
	m_wndToolBar.SetButtonInfo(3, ID_NAV_NEXT, TBBS_DROPDOWN, 2);
	m_wndToolBar.SetButtonInfo(4, ID_SEPARATOR, TBBS_SEPARATOR, 0);
	m_wndToolBar.SetButtonInfo(5, ID_FILE_PLAY, TBBS_BUTTON, 3);
	m_wndToolBar.SetButtonInfo(6, ID_FILE_STEP, TBBS_BUTTON, 5);
	m_wndToolBar.SetButtonInfo(7, ID_FILE_STOP, TBBS_BUTTON, 6);
	m_wndToolBar.SetButtonInfo(8, ID_SEPARATOR, TBBS_SEPARATOR, 0);
	m_wndToolBar.SetButtonInfo(9, ID_FILE_PROP, TBBS_BUTTON, 7);
	m_wndToolBar.SetButtonInfo(10, ID_SEPARATOR, TBBS_SEPARATOR, 0);
	m_wndToolBar.SetButtonInfo(11, IDD_CONFIGURE, TBBS_BUTTON, 8);
	m_wndToolBar.SetButtonInfo(12, ID_SWITCH_RENDER, TBBS_BUTTON, 9);

	CToolBarCtrl &ctrl = m_wndToolBar.GetToolBarCtrl();
	ctrl.SetStyle(TBSTYLE_FLAT | TBSTYLE_DROPDOWN);
	ctrl.SetExtendedStyle(TBSTYLE_EX_DRAWDDARROWS);

	memset(&bi, 0, sizeof(bi));
	bi.cbSize = sizeof(bi);
	ctrl.GetButtonInfo(2, &bi);
	bi.fsStyle |= TBSTYLE_DROPDOWN;
	ctrl.SetButtonInfo(ID_NAV_PREV, &bi);

	memset(&bi, 0, sizeof(bi));
	bi.cbSize = sizeof(bi);
	ctrl.GetButtonInfo(3, &bi);
	bi.fsStyle |= TBSTYLE_DROPDOWN;
	ctrl.SetButtonInfo(ID_NAV_NEXT, &bi);

	if (!m_wndStatusBar.Create(this) ||
		!m_wndStatusBar.SetIndicators(status_indics,
		  sizeof(status_indics)/sizeof(UINT)))
	{
		TRACE0("Failed to create status bar\n");
		return -1;      // fail to create
	}

	if (!m_Address.Create(this, IDD_NAVBAR, WS_CHILD | CBRS_TOP | CBRS_FLYBY | CBRS_SIZE_DYNAMIC, IDD_NAVBAR) ) {
		return -1;      // fail to create
	}

	if (!m_Sliders.Create(IDD_SLIDERS, this) ) {
		return -1;      // fail to create
	}

	m_wndStatusBar.SetPaneInfo(0, ID_TIMER, SBPS_NORMAL, 60);
	m_wndStatusBar.SetPaneInfo(1, ID_SEPARATOR, SBPS_STRETCH, 0);
	SetIcon(AfxGetApp()->LoadIcon(IDR_MAINFRAME), TRUE);
	SetIcon(AfxGetApp()->LoadIcon(IDR_MAINFRAME), FALSE);

	SetTimer(RTI_TIMER, RTI_REFRESH_MS, RTInfoTimer);
	return 0;
}


BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if( !CFrameWnd::PreCreateWindow(cs) )
		return FALSE;
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
	cs.lpszClass = AfxRegisterWndClass(0);
	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CMainFrame diagnostics

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
	CFrameWnd::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
	CFrameWnd::Dump(dc);
}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CMainFrame message handlers
void CMainFrame::OnSetFocus(CWnd* pOldWnd)
{
	m_pWndView->SetFocus();
	if (m_RestoreFS==1) {
		m_RestoreFS=2;
	}
	else if (m_RestoreFS==2) {
		m_RestoreFS = 0;
		SetFullscreen();
	}
}

BOOL CMainFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
	// let the view have first crack at the command
	if (m_pWndView->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
		return TRUE;

	// otherwise, do default handling
	return CFrameWnd::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}


void CMainFrame::OnSize(UINT nType, int cx, int cy) 
{
	RECT rc2;
	u32 tool_h, slide_h, add_h, stat_h;

	if (m_bInitShow) {
		CFrameWnd::OnSize(nType, cx, cy);
		return;
	}
	m_wndToolBar.GetClientRect(&rc2);
	tool_h = rc2.bottom - rc2.top;
	m_Address.GetClientRect(&rc2);
	add_h = rc2.bottom - rc2.top;
	m_Sliders.GetClientRect(&rc2);
	slide_h = rc2.bottom - rc2.top;
	m_wndStatusBar.GetClientRect(&rc2);
	stat_h = rc2.bottom - rc2.top;
	if ((u32) cy <= tool_h+add_h+slide_h+stat_h) {
		OnSetSize(cx, 1);
		return;
	}

	CFrameWnd::OnSize(nType, cx, cy);
	cy -= tool_h + add_h + slide_h + stat_h;

	m_Address.SetWindowPos(this, 0, 0, cx, add_h, SWP_SHOWWINDOW | SWP_NOMOVE);

	m_pWndView->ShowWindow(SW_SHOW);
	m_pWndView->SetWindowPos(this, 0, add_h + tool_h, cx, cy, SWP_NOZORDER);

	m_Sliders.SetWindowPos(this, 0, add_h + tool_h + cy, cx, slide_h, SWP_NOZORDER|SWP_SHOWWINDOW);
	/*and resize term*/
	gf_term_set_size(GetApp()->m_term, cx, cy);
}


LRESULT CMainFrame::OnSetSize(WPARAM wParam, LPARAM lParam)
{
	UINT width, height;
	width = (UINT) wParam;
	height = (UINT) lParam;
	if (m_bInitShow) {
		m_wndToolBar.UpdateWindow();
		m_wndToolBar.ShowWindow(SW_SHOW);
		m_Address.UpdateWindow();
		m_Address.ShowWindow(SW_SHOW);
		m_Sliders.UpdateWindow();
		m_Sliders.ShowWindow(SW_SHOW);
		m_Sliders.m_PosSlider.EnableWindow(FALSE);
		m_pWndView->ShowWindow(SW_SHOW);
		ShowWindow(SW_SHOW);
		m_bInitShow = GF_FALSE;
	}

	RECT winRect;
	winRect.left = 0;
	winRect.right = width;
	winRect.top = 0;
	winRect.bottom = height;
	AdjustWindowRectEx(&winRect, GetStyle(), TRUE, GetExStyle());
	winRect.bottom -= winRect.top;
	winRect.right -= winRect.left;
	winRect.left = winRect.top = 0;

	RECT rc2;
	m_Address.GetClientRect(&rc2);
	winRect.bottom += rc2.bottom;
	m_wndToolBar.GetClientRect(&rc2);
	winRect.bottom += rc2.bottom;
	m_Sliders.GetClientRect(&rc2);
	winRect.bottom += rc2.bottom;
	m_wndStatusBar.GetClientRect(&rc2);
	winRect.bottom += rc2.bottom;

	GetWindowRect(&rc2);
	rc2.bottom -= rc2.top;
	rc2.right -= rc2.left;
	if ((rc2.right != winRect.right) || (rc2.bottom != winRect.bottom)) {
		SetWindowPos(NULL, 0, 0, winRect.right, winRect.bottom, SWP_NOZORDER | SWP_NOMOVE | SWP_SHOWWINDOW);
	} else {
		/*just resize term*/
		//gf_term_set_size(GetApp()->m_term, width, height);
		SetWindowPos(NULL, 0, 0, winRect.right, winRect.bottom, SWP_NOZORDER | SWP_NOMOVE | SWP_SHOWWINDOW);
	}
	return 0;
}

void CMainFrame::OnMove(int x, int y) 
{
	CFrameWnd::OnMove(x, y);
	RECT rc;
	
	m_wndToolBar.GetClientRect(&rc);
	m_wndToolBar.SetWindowPos(this, x, y, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
	y += rc.bottom - rc.top;
	m_Address.SetWindowPos(this, x, y, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
	m_Address.GetClientRect(&rc);
	y += rc.bottom - rc.top;
	m_pWndView->SetWindowPos(this, x, y, 0, 0, SWP_NOSIZE);
	m_pWndView->GetClientRect(&rc);
	y += rc.bottom;
	m_Sliders.SetWindowPos(this, x, y, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
}


#define PROGRESS_TIMER	20
#define PROGRESS_REFRESH_MS		100

void CALLBACK EXPORT ProgressTimer(HWND , UINT , UINT_PTR nID , DWORD )
{
	u32 now;
	if (nID != PROGRESS_TIMER) return;
	Osmo4 *app = GetApp();
	CMainFrame *pFrame = (CMainFrame *) app->m_pMainWnd;
	/*shutdown*/
	if (!pFrame) return;

	now = gf_term_get_time_in_ms(app->m_term);
	if (!now) return;

	if (app->can_seek && !pFrame->m_Sliders.m_grabbed) {
		if (now >= app->max_duration + 100) {
			if (gf_term_get_option(app->m_term, GF_OPT_IS_FINISHED)) {
				pFrame->m_pPlayList->PlayNext();
		}
			/*if no IsOver go on forever*/
		} else {
			if (!app->m_reset)
				pFrame->m_Sliders.m_PosSlider.SetPos(now);
		}
	}
}

void CMainFrame::SetProgTimer(Bool bOn) 
{
	if (bOn) 
		SetTimer(PROGRESS_TIMER, PROGRESS_REFRESH_MS, ProgressTimer); 
	else
		KillTimer(PROGRESS_TIMER);
}


LRESULT CMainFrame::Open(WPARAM wParam, LPARAM lParam)
{
	Bool do_pause;
	Osmo4 *app = GetApp();
	CString txt, url;
	m_bStartupFile = GF_FALSE;
	txt = "Osmo4 - ";
	txt += m_pPlayList->GetDisplayName();

	url = m_pPlayList->GetURL();
	m_Address.m_Address.SetWindowText(url);
	SetWindowText(txt);
	if (app->start_mode==1) do_pause = GF_TRUE;
	else if (app->start_mode==2) do_pause = GF_FALSE;
	else do_pause = /*!app->m_AutoPlay*/GF_FALSE;
	gf_term_connect_from_time(app->m_term, (LPCSTR) url, app->m_reconnect_time, do_pause);
	app->m_reconnect_time = 0;
	app->start_mode = 0;
	app->UpdatePlayButton();
	nb_viewpoints = 0;
	return 1;	
}

LRESULT CMainFrame::NewInstanceOpened(WPARAM wParam, LPARAM lParam)
{
	Bool queue_only = GF_FALSE;
	char *url = (char *) static_gpac_get_url();
	if (!strnicmp(url, "-queue ", 7)) {
		queue_only = GF_TRUE;
		url += 7;
	}
	m_pPlayList->QueueURL(url);
	m_pPlayList->RefreshList();
	if (!queue_only) m_pPlayList->PlayNext();
	return 1;
}


void CMainFrame::ForwardMessage()
{
	const MSG *msg = GetCurrentMessage();
	m_pWndView->SendMessage(msg->message, msg->wParam, msg->lParam);
}
void CMainFrame::OnSysKeyUp(UINT , UINT , UINT ) { ForwardMessage(); }
void CMainFrame::OnSysKeyDown(UINT , UINT , UINT ) { ForwardMessage(); }
void CMainFrame::OnChar(UINT , UINT , UINT ) { ForwardMessage(); }
void CMainFrame::OnKeyDown(UINT , UINT , UINT ) { ForwardMessage(); }
void CMainFrame::OnKeyUp(UINT , UINT , UINT ) { ForwardMessage(); }
void CMainFrame::OnLButtonDown(UINT , CPoint ) { ForwardMessage(); }
void CMainFrame::OnLButtonDblClk(UINT , CPoint ) { ForwardMessage(); }
void CMainFrame::OnLButtonUp(UINT , CPoint ) { ForwardMessage(); }

void CMainFrame::OnDropFiles(HDROP hDropInfo) 
{
	u32 i, count;
	Osmo4 *app = GetApp();
	char fileName[MAX_PATH];

	count = ::DragQueryFile(hDropInfo, 0xFFFFFFFF, NULL, 0);
	if (!count) return;

	/*if playing and sub d&d, open sub in current presentation*/
	if (app->m_isopen && (count==1)) {
		::DragQueryFile(hDropInfo, 0, fileName, MAX_PATH);
		char *ext = strrchr(fileName, '.');
		if (ext && ( !stricmp(ext, ".srt") || !stricmp(ext, ".sub") || !stricmp(ext, ".ttxt") || !stricmp(ext, ".xml") ) ) {
			AddSubtitle(fileName, GF_TRUE);
			return;
		}
	}
	
/*	if (count==1) 
		m_pPlayList->Truncate();
	else
*/		m_pPlayList->Clear();

	for (i=0; i<count; i++) {
		::DragQueryFile (hDropInfo, i, fileName, MAX_PATH);
		m_pPlayList->QueueURL(fileName);
	}
	m_pPlayList->RefreshList();
	m_pPlayList->PlayNext();
}

void CALLBACK EXPORT ConsoleTimer(HWND , UINT , UINT_PTR , DWORD )
{
	CMainFrame *pFrame = (CMainFrame *) GetApp()->m_pMainWnd;
	
	pFrame->m_wndStatusBar.GetStatusBarCtrl().SetIcon(2, NULL);
	pFrame->KillTimer(pFrame->m_timer_on);
	pFrame->m_timer_on = 0;
	pFrame->m_wndStatusBar.SetPaneText(1, "Ready");
}

#define CONSOLE_DISPLAY_TIME	1000

LRESULT CMainFrame::OnConsoleMessage(WPARAM wParam, LPARAM lParam)
{
	if (m_timer_on) KillTimer(m_timer_on);
	
	if (console_err>=0) {
		m_wndStatusBar.GetStatusBarCtrl().SetIcon(2, m_icomessage);
		m_wndStatusBar.SetPaneText(1, console_message);
	} else {
		char msg[5000];
		m_wndStatusBar.GetStatusBarCtrl().SetIcon(2, m_icoerror);
		sprintf(msg, "%s (%s)", console_message, console_service);
		m_wndStatusBar.SetPaneText(1, msg);
	}
	m_timer_on = SetTimer(10, wParam ? (UINT) wParam : CONSOLE_DISPLAY_TIME, ConsoleTimer);
	return 0;
}

BOOL CMainFrame::DestroyWindow() 
{
	if (GetApp()->m_isopen) KillTimer(PROGRESS_TIMER);
	/*signal close to prevent callbacks but don't close, this is done in ExitInstance (otherwise there's a
	deadlock happening not sure why yet)*/
//	GetApp()->m_open = 0;
	return CFrameWnd::DestroyWindow();
}


void CMainFrame::OnViewOriginal() 
{
	Osmo4 *gpac = GetApp();
	gf_term_set_option(gpac->m_term, GF_OPT_ORIGINAL_VIEW, 1);	
	OnSetSize(gpac->orig_width, gpac->orig_height);
}

void CMainFrame::SetFullscreen() 
{
	Osmo4 *gpac = GetApp();
	if (!m_bFullScreen) {
		GetWindowRect(&backup_wnd_rc);
		if (gf_term_set_option(gpac->m_term, GF_OPT_FULLSCREEN, 1) == GF_OK) 
			m_bFullScreen = GF_TRUE;
	} else {
		if (gf_term_set_option(gpac->m_term, GF_OPT_FULLSCREEN, 0) == GF_OK) 
			m_bFullScreen = GF_FALSE;
		SetWindowPos(NULL, backup_wnd_rc.left, backup_wnd_rc.top, backup_wnd_rc.right-backup_wnd_rc.left, backup_wnd_rc.bottom-backup_wnd_rc.top, SWP_NOZORDER);
	}
}

void CMainFrame::OnViewFullscreen() 
{
	SetFullscreen();
}

void CMainFrame::OnArKeep() 
{
	gf_term_set_option(GetApp()->m_term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_KEEP);	
	m_aspect_ratio = GF_ASPECT_RATIO_KEEP;
}

void CMainFrame::OnArFill() 
{
	gf_term_set_option(GetApp()->m_term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_FILL_SCREEN);	
	m_aspect_ratio = GF_ASPECT_RATIO_FILL_SCREEN;
}

void CMainFrame::OnAr43() 
{
	gf_term_set_option(GetApp()->m_term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_4_3);	
	m_aspect_ratio = GF_ASPECT_RATIO_4_3;
}

void CMainFrame::OnAr169() 
{
	gf_term_set_option(GetApp()->m_term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_16_9);	
	m_aspect_ratio = GF_ASPECT_RATIO_16_9;
}

void CMainFrame::OnUpdateAr169(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_aspect_ratio == GF_ASPECT_RATIO_16_9);
}

void CMainFrame::OnUpdateAr43(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_aspect_ratio == GF_ASPECT_RATIO_4_3);
}

void CMainFrame::OnUpdateArFill(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_aspect_ratio == GF_ASPECT_RATIO_FILL_SCREEN);
}

void CMainFrame::OnUpdateArKeep(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_aspect_ratio == GF_ASPECT_RATIO_KEEP);
}

void CMainFrame::OnUpdateNavigate(CCmdUI* pCmdUI)
{
	BOOL enable;
	Osmo4 *app = GetApp();
	pCmdUI->Enable(FALSE);
	if (!app->m_isopen) return;
	
	u32 type = gf_term_get_option(app->m_term, GF_OPT_NAVIGATION_TYPE);
	enable = type ? TRUE : FALSE;

	if (pCmdUI->m_nID==ID_NAV_RESET) {
		pCmdUI->Enable(TRUE);
		return;
	}

	u32 mode = gf_term_get_option(app->m_term, GF_OPT_NAVIGATION);
	/*common 2D/3D modes*/
	if (pCmdUI->m_nID==ID_NAVIGATE_NONE) { pCmdUI->Enable(enable); pCmdUI->SetCheck(mode ? 0 : 1); }
	else if (pCmdUI->m_nID==ID_NAVIGATE_EXAM) { pCmdUI->Enable(enable); pCmdUI->SetCheck((mode==GF_NAVIGATE_EXAMINE) ? 1 : 0); }
	else if (pCmdUI->m_nID==ID_NAVIGATE_SLIDE) { pCmdUI->Enable(enable); pCmdUI->SetCheck((mode==GF_NAVIGATE_SLIDE) ? 1 : 0); }

	if (type==GF_NAVIGATE_TYPE_2D) return;
	pCmdUI->Enable(enable); 	
	if (pCmdUI->m_nID==ID_NAVIGATE_WALK) pCmdUI->SetCheck((mode==GF_NAVIGATE_WALK) ? 1 : 0);
	else if (pCmdUI->m_nID==ID_NAVIGATE_FLY) pCmdUI->SetCheck((mode==GF_NAVIGATE_FLY) ? 1 : 0);
	else if (pCmdUI->m_nID==ID_NAVIGATE_PAN) pCmdUI->SetCheck((mode==GF_NAVIGATE_PAN) ? 1 : 0);
	else if (pCmdUI->m_nID==ID_NAVIGATE_VR) pCmdUI->SetCheck((mode==GF_NAVIGATE_VR) ? 1 : 0);
	else if (pCmdUI->m_nID==ID_NAVIGATE_GAME) pCmdUI->SetCheck((mode==GF_NAVIGATE_GAME) ? 1 : 0);
}


void CMainFrame::SetNavigate(u32 mode)
{
	Osmo4 *app = GetApp();
	gf_term_set_option(app->m_term, GF_OPT_NAVIGATION, mode);
}
void CMainFrame::OnNavigateNone() { SetNavigate(GF_NAVIGATE_NONE); }
void CMainFrame::OnNavigateWalk() { SetNavigate(GF_NAVIGATE_WALK); }
void CMainFrame::OnNavigateFly() { SetNavigate(GF_NAVIGATE_FLY); }
void CMainFrame::OnNavigateExam() { SetNavigate(GF_NAVIGATE_EXAMINE); }
void CMainFrame::OnNavigateSlide() { SetNavigate(GF_NAVIGATE_SLIDE); }
void CMainFrame::OnNavigatePan() { SetNavigate(GF_NAVIGATE_PAN); }
void CMainFrame::OnNavigateOrbit() { SetNavigate(GF_NAVIGATE_ORBIT); }
void CMainFrame::OnNavigateVR() { SetNavigate(GF_NAVIGATE_VR); }
void CMainFrame::OnNavigateGame() { SetNavigate(GF_NAVIGATE_GAME); }

void CMainFrame::OnNavigateReset()
{
	Osmo4 *app = GetApp();
	gf_term_set_option(app->m_term, GF_OPT_NAVIGATION_TYPE, 0);
}


LRESULT CMainFrame::OnNavigate(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	Osmo4 *gpac = GetApp();

	/*this is a migrate instruction, just disconnect the player*/
	if (gpac->m_navigate_url.IsEmpty() ) {
		gf_term_disconnect(gpac->m_term);
		return 0;
	}

	if (gf_term_is_supported_url(gpac->m_term, gpac->m_navigate_url, GF_TRUE, gpac->m_NoMimeFetch)) {
		char *str = gf_url_concatenate(m_pPlayList->GetURL(), gpac->m_navigate_url);
		if (str) {
			m_pPlayList->Truncate();
			m_pPlayList->QueueURL(str);
			gf_free(str);
			m_pPlayList->RefreshList();
			m_pPlayList->PlayNext();
			return 0;
		}
	}
	
	if (m_bFullScreen) {
		SetFullscreen();
		m_RestoreFS = 1;
	}

	console_message = gpac->m_navigate_url;
	console_err = GF_OK;
	PostMessage(WM_CONSOLEMSG);
	ShellExecute(NULL, "open", (LPCSTR) gpac->m_navigate_url, NULL, NULL, SW_SHOWNORMAL);

	return 0;
}

void CMainFrame::OnFileProp() 
{
	if (!m_pProps) {
		m_pProps = new CFileProps(this);
		m_pProps->Create(this);
	}
	m_pProps->ShowWindow(SW_SHOW);
}

void CMainFrame::OnUpdateFileProp(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetApp()->m_isopen);	
}

void CMainFrame::OnConfigure() 
{
	if (!m_pOpt) {
		m_pOpt = new COptions(this);
		m_pOpt->Create(this);
	}
	m_pOpt->ShowWindow(SW_SHOW);
}

void CMainFrame::OnShortcuts() 
{
	MessageBox(
		"Open File: Ctrl + O\n"
		"Open URL: Ctrl + U\n"
		"Reload File: F5\n"
		"Pause/Resume File: Ctrl + P\n"
		"Step by Step: Ctrl + S\n"
		"Seek +5%: Alt + left arrow\n"
		"Seek -5%: Alt + right arrow\n"
		"Switch quality up: Ctrl + H\n"
		"Switch quality down: Ctrl + L\n"
		"Fullscreen On/Off: Double-click or Escape\n"
		"\n"
		"Show Properties: Ctrl + I\n"
		"Show Playlist: F3\n"
		"Next Playlist Item: Ctrl + right arrow\n"
		"Previous Playlist Item: Ctrl + left arrow\n"
		"\n"
		"Aspect Ratio Normal: Ctrl + 1\n"
		"Aspect Ratio Fill: Ctrl + 2\n"
		"Aspect Ratio 4/3: Ctrl + 3\n"
		"Aspect Ratio 16/9: Ctrl + 4\n"
		
		
		, "Shortcuts Available on Osmo4", MB_OK);
}

void CMainFrame::OnNavInfo() 
{
	MessageBox(
		"* Walk & Fly modes:\n"
		"\tH move: H pan - V move: Z-translate - V move+CTRL or Wheel: V pan - Right Click (Walk only): Jump\n"
		"\tleft/right: H pan - left/right+CTRL: H translate - up/down: Z-translate - up/down+CTRL: V pan\n"
		"* Pan mode:\n"
		"\tH move: H pan - V move: V pan - V move+CTRL or Wheel: Z-translate\n"
		"\tleft/right: H pan - left/right+CTRL: H translate - up/down: V pan - up/down+CTRL: Z-translate\n"
		"* Slide mode:\n"
		"\tH move: H translate - V move: V translate - V move+CTRL or Wheel: Z-translate\n"
		"\tleft/right: H translate - left/right+CTRL: H pan - up/down: V translate - up/down+CTRL: Z-translate\n"
		"* Examine & Orbit mode:\n"
		"\tH move: Y-Axis rotate - H move+CTRL: Z-Axis rotate - V move: X-Axis rotate - V move+CTRL or Wheel: Z-translate\n"
		"\tleft/right: Y-Axis rotate - left/right+CTRL: H translate - up/down: X-Axis rotate - up/down+CTRL: Y-translate\n"
		"* VR mode:\n"
		"\tH move: H pan - V move: V pan - V move+CTRL or Wheel: Camera Zoom\n"
		"\tleft/right: H pan - up/down: V pan - up/down+CTRL: Camera Zoom\n"
		"* Game mode (press END to escape):\n"
		"\tH move: H pan - V move: V pan\n"
		"\tleft/right: H translate - up/down: Z-translate\n"
		"\n"
		"* All 3D modes: CTRL+PGUP/PGDOWN will zoom in/out camera (field of view) \n"

		"\n"
		"*Slide Mode in 2D:\n"
		"\tH move: H translate - V move: V translate - V move+CTRL: zoom\n"
		"\tleft/right: H translate - up/down: V translate - up/down+CTRL: zoom\n"
		"*Examine Mode in 2D (3D renderer only):\n"
		"\tH move: Y-Axis rotate - V move: X-Axis rotate\n"
		"\tleft/right: Y-Axis rotate - up/down: X-Axis rotate\n"

		"\n"
		"HOME: reset navigation to last viewpoint (2D or 3D navigation)\n"
		"SHIFT key in all modes: fast movement\n"

		, "3D navigation keys (\'H\'orizontal and \'V\'ertical) used in GPAC", MB_OK);
}



void CMainFrame::BuildViewList()
{
	Osmo4 *app = GetApp();
	if (!app->m_isopen) return;

	/*THIS IS HARCODED FROM THE MENU LAYOUT */
	CMenu *pMenu = GetMenu()->GetSubMenu(1)->GetSubMenu(0);
	while (pMenu->GetMenuItemCount()) pMenu->DeleteMenu(0, MF_BYPOSITION);

	s32 id = ID_VP_0;
	nb_viewpoints = 0;
	while (1) {
		const char *szName = NULL;
		Bool bound;
		GF_Err e = gf_term_get_viewpoint(app->m_term, nb_viewpoints+1, &szName, &bound);
		if (e) break;
		if (szName) {
			pMenu->AppendMenu(MF_ENABLED, id+nb_viewpoints, szName);
		} else {
			char szLabel[1024];
			sprintf(szLabel, "Viewpoint #%d", nb_viewpoints+1);
			pMenu->AppendMenu(MF_ENABLED, id+nb_viewpoints, szLabel);
		}
		nb_viewpoints++;
		if (nb_viewpoints==ID_VP_19-ID_VP_0) break;
	}
}


void CMainFrame::BuildStreamList(Bool reset_only)
{
	u32 nb_subs;
	CMenu *pSelect;
	Osmo4 *app = GetApp();

	pSelect = GetMenu()->GetSubMenu(2)->GetSubMenu(0);
	/*THIS IS HARCODED FROM THE MENU LAYOUT */
	CMenu *pMenu = pSelect->GetSubMenu(0);
	while (pMenu->GetMenuItemCount()) pMenu->DeleteMenu(0, MF_BYPOSITION);
	pMenu = pSelect->GetSubMenu(1);
	while (pMenu->GetMenuItemCount()) pMenu->DeleteMenu(0, MF_BYPOSITION);
	pMenu = pSelect->GetSubMenu(2);
	while (pMenu->GetMenuItemCount()) pMenu->DeleteMenu(0, MF_BYPOSITION);

	if (reset_only) {
		m_bFirstStreamQuery = GF_TRUE;
		return;
	}
	if (!app->m_isopen || !gf_term_get_option(app->m_term, GF_OPT_CAN_SELECT_STREAMS)) return;

	GF_ObjectManager *root_od = gf_term_get_root_object(app->m_term);
	if (!root_od) return;
	u32 count = gf_term_get_object_count(app->m_term, root_od);
	nb_subs = 0;

	for (u32 i=0; i<count; i++) {
		char szLabel[1024];
		GF_MediaInfo info;
		GF_ObjectManager *odm = gf_term_get_object(app->m_term, root_od, i);
		if (!odm) return;

		if (gf_term_get_object_info(app->m_term, odm, &info) != GF_OK) break;
		if (info.owns_service) {
			char *szName = (char *)strrchr(info.service_url, '\\');
			if (!szName) szName = (char *)strrchr(info.service_url, '/');
			if (!szName) szName = (char *) info.service_url;
			else szName += 1;
			strcpy(szLabel, szName);
			szName = strrchr(szLabel, '.');
			if (szName) szName[0] = 0;
		}

		switch (info.od_type) {
		case GF_STREAM_AUDIO:
			pMenu = pSelect->GetSubMenu(0);
			if (!info.owns_service) {
				if (info.lang) {
					sprintf(szLabel, "Language %s (ID %d)", gf_4cc_to_str(info.lang), info.od->objectDescriptorID);
				} else {
					sprintf(szLabel, "ID %d", info.od->objectDescriptorID);
				}
			}
			pMenu->AppendMenu(MF_ENABLED, ID_SELOBJ_0 + i, szLabel);
			break;
		case GF_STREAM_VISUAL:
			pMenu = pSelect->GetSubMenu(1);
			if (!info.owns_service) sprintf(szLabel, "ID %d", info.od->objectDescriptorID);
			pMenu->AppendMenu(MF_ENABLED, ID_SELOBJ_0 + i, szLabel);
			break;
		case GF_STREAM_TEXT:
			nb_subs ++;
			pMenu = pSelect->GetSubMenu(2);
			if (!info.owns_service) {
				if (info.lang) {
					sprintf(szLabel, "Language %s (ID %d)", gf_4cc_to_str(info.lang), info.od->objectDescriptorID);
				} else {
					sprintf(szLabel, "ID %d", info.od->objectDescriptorID);
				}
			}
			pMenu->AppendMenu(MF_ENABLED, ID_SELOBJ_0 + i, szLabel);
			break;
		}
	}
	if (m_bFirstStreamQuery) {
		m_bFirstStreamQuery = GF_FALSE;
		if (!nb_subs && app->m_LookForSubtitles) LookForSubtitles();
	}

}

BOOL CMainFrame::OnCommand(WPARAM wParam, LPARAM lParam)
{
	int ID = LOWORD(wParam);
	Osmo4 *app = GetApp();

	if ( (ID>=ID_VP_0) && (ID<=ID_VP_0+nb_viewpoints)) {
		ID -= ID_VP_0;
		gf_term_set_viewpoint(app->m_term, ID+1, NULL);
		return TRUE;
	}
	if ( (ID>=ID_NAV_PREV_0) && (ID<=ID_NAV_PREV_9)) {
		ID -= ID_NAV_PREV_0;
		s32 prev = m_pPlayList->m_cur_entry - ID;
		if (prev>=0) {
			m_pPlayList->m_cur_entry = prev;
			m_pPlayList->PlayPrev();
		}
		return TRUE;
	}
	if ( (ID>=ID_NAV_NEXT_0) && (ID<=ID_NAV_NEXT_9)) {
		ID -= ID_NAV_NEXT_0;
		u32 next = m_pPlayList->m_cur_entry + ID;
		if (next < gf_list_count(m_pPlayList->m_entries) ) {
			m_pPlayList->m_cur_entry = next;
			m_pPlayList->PlayNext();
		}
		return TRUE;
	}
	if ( (ID>=ID_SELOBJ_0) && (ID<=ID_SELOBJ_29)) {
		ID -= ID_SELOBJ_0;
		GF_ObjectManager *root_od = gf_term_get_root_object(app->m_term);
		if (!root_od) return TRUE;
		GF_ObjectManager *odm = gf_term_get_object(app->m_term, root_od, ID);
		gf_term_select_object(app->m_term, odm);
		return TRUE;
	}
	if ( (ID>=ID_SETCHAP_FIRST) && (ID<=ID_SETCHAP_LAST)) {
		ID -= ID_SETCHAP_FIRST;
		gf_term_play_from_time(app->m_term, (u32) (1000*m_chapters_start[ID]), 0);
		return TRUE;
	}
	return CFrameWnd::OnCommand(wParam, lParam);
}

void CMainFrame::OnInitMenuPopup(CMenu* pPopupMenu, UINT ID, BOOL bSys)
{
	Osmo4 *app = GetApp();
	/*viewport list*/
	if (pPopupMenu->GetMenuItemID(0)==ID_VP_0) {
		for (int i=0; i<nb_viewpoints; i++) {
			const char *szName;
			Bool bound;
			GF_Err e = gf_term_get_viewpoint(app->m_term, i+1, &szName, &bound);
			pPopupMenu->EnableMenuItem(i, MF_BYPOSITION);
			if (bound) pPopupMenu->CheckMenuItem(i, MF_BYPOSITION | MF_CHECKED);
		}
		return;
	}
	/*navigation*/
	if ((pPopupMenu->GetMenuItemID(0)==ID_NAV_PREV_0) || (pPopupMenu->GetMenuItemID(0)==ID_NAV_NEXT_0)) {
		int count = pPopupMenu->GetMenuItemCount();
		for (int i=0; i<count; i++) {
			pPopupMenu->EnableMenuItem(i, MF_BYPOSITION);		
		}
		return;
	}
	/*stream selection*/
	if (pPopupMenu->m_hMenu == GetMenu()->GetSubMenu(2)->m_hMenu) {
		if (!app->m_isopen || !gf_term_get_option(app->m_term, GF_OPT_CAN_SELECT_STREAMS)) {
			pPopupMenu->EnableMenuItem(0, MF_BYPOSITION | MF_DISABLED | MF_GRAYED);
		} else {
			pPopupMenu->EnableMenuItem(0, MF_BYPOSITION | MF_ENABLED);
		}
	}
	if ((pPopupMenu->GetMenuItemID(0)>=ID_SELOBJ_0) && (pPopupMenu->GetMenuItemID(0)<=ID_SELOBJ_29)) {
		GF_ObjectManager *root_od = gf_term_get_root_object(app->m_term);
		if (!root_od) return;

		int count = pPopupMenu->GetMenuItemCount();
		for (int i=0; i<count; i++) {
			u32 id = pPopupMenu->GetMenuItemID(i) - ID_SELOBJ_0;
			GF_ObjectManager *odm = gf_term_get_object(app->m_term, root_od, id);
			if (!odm) {
				pPopupMenu->EnableMenuItem(i, MF_DISABLED | MF_BYPOSITION);
			} else {
				GF_MediaInfo info;

				gf_term_get_object_info(app->m_term, odm, &info);
				pPopupMenu->EnableMenuItem(i, MF_BYPOSITION);
				pPopupMenu->CheckMenuItem(i, MF_BYPOSITION | (info.status ? MF_CHECKED : MF_UNCHECKED) );
			}
		}
		return;
	}
	/*chapters*/
	if (pPopupMenu->m_hMenu == GetMenu()->GetSubMenu(2)->m_hMenu) {
		if (!app->m_isopen || !m_num_chapters) {
			pPopupMenu->EnableMenuItem(1, MF_BYPOSITION | MF_DISABLED | MF_GRAYED);
		} else {
			pPopupMenu->EnableMenuItem(1, MF_BYPOSITION | MF_ENABLED);
		}
	}
	if ((pPopupMenu->GetMenuItemID(0)>=ID_SETCHAP_FIRST) && (pPopupMenu->GetMenuItemID(0)<=ID_SETCHAP_LAST)) {
		Double now = gf_term_get_time_in_ms(app->m_term);
		now /= 1000;

		int count = pPopupMenu->GetMenuItemCount();
		for (int i=0; i<count; i++) {
			u32 id = pPopupMenu->GetMenuItemID(i) - ID_SETCHAP_FIRST;
			pPopupMenu->EnableMenuItem(i, MF_BYPOSITION);

			Bool is_current = GF_FALSE;
			if (m_chapters_start[id]<=now) {
				if (id+1<m_num_chapters) {
					if (m_chapters_start[id+1]>now) is_current = GF_TRUE;
				} else {
					is_current = GF_TRUE;
				}
			}
			pPopupMenu->CheckMenuItem(i, MF_BYPOSITION | (is_current ? MF_CHECKED : MF_UNCHECKED));
		}
		return;
	}
	/*default*/
	CFrameWnd::OnInitMenuPopup(pPopupMenu, ID, bSys);
}

void CMainFrame::OnCollideDisp() 
{
	gf_term_set_option(GetApp()->m_term, GF_OPT_COLLISION, GF_COLLISION_DISPLACEMENT);
}

void CMainFrame::OnUpdateCollideDisp(CCmdUI* pCmdUI) 
{
	Osmo4 *gpac = GetApp(); 
	pCmdUI->Enable(gpac->m_isopen);	
	pCmdUI->SetCheck( (gf_term_get_option(gpac->m_term, GF_OPT_COLLISION) == GF_COLLISION_DISPLACEMENT) ? 1 : 0);
}

void CMainFrame::OnCollideNone() 
{
	gf_term_set_option(GetApp()->m_term, GF_OPT_COLLISION, GF_COLLISION_NONE);
}

void CMainFrame::OnUpdateCollideNone(CCmdUI* pCmdUI) 
{
	Osmo4 *gpac = GetApp(); 
	pCmdUI->Enable(gpac->m_isopen);	
	pCmdUI->SetCheck( (gf_term_get_option(gpac->m_term, GF_OPT_COLLISION) == GF_COLLISION_NONE) ? 1 : 0);
}

void CMainFrame::OnCollideReg() 
{
	gf_term_set_option(GetApp()->m_term, GF_OPT_COLLISION, GF_COLLISION_NORMAL);
}

void CMainFrame::OnUpdateCollideReg(CCmdUI* pCmdUI) 
{
	Osmo4 *gpac = GetApp(); 
	pCmdUI->Enable(gpac->m_isopen);	
	pCmdUI->SetCheck( (gf_term_get_option(gpac->m_term, GF_OPT_COLLISION) == GF_COLLISION_NORMAL) ? 1 : 0);
}

void CMainFrame::OnHeadlight() 
{
	Osmo4 *app = GetApp();
	Bool val = gf_term_get_option(app->m_term, GF_OPT_HEADLIGHT) ? GF_FALSE : GF_TRUE;
	gf_term_set_option(app->m_term, GF_OPT_HEADLIGHT, val);
}

void CMainFrame::OnUpdateHeadlight(CCmdUI* pCmdUI) 
{
	Osmo4 *app = GetApp();
	pCmdUI->Enable(FALSE);
	if (!app->m_isopen) return;
	u32 type = gf_term_get_option(app->m_term, GF_OPT_NAVIGATION_TYPE);
	if (type!=GF_NAVIGATE_TYPE_3D) return;

	pCmdUI->Enable(TRUE);
	pCmdUI->SetCheck(gf_term_get_option(app->m_term, GF_OPT_HEADLIGHT) ? 1 : 0);
}

void CMainFrame::OnGravity() 
{
	Osmo4 *app = GetApp();
	Bool val = gf_term_get_option(app->m_term, GF_OPT_GRAVITY) ? GF_FALSE : GF_TRUE;
	gf_term_set_option(app->m_term, GF_OPT_GRAVITY, val);
}

void CMainFrame::OnUpdateGravity(CCmdUI* pCmdUI) 
{
	Osmo4 *app = GetApp();
	pCmdUI->Enable(FALSE);
	if (!app->m_isopen) return;
	u32 type = gf_term_get_option(app->m_term, GF_OPT_NAVIGATION_TYPE);
	if (type!=GF_NAVIGATE_TYPE_3D) return;
	type = gf_term_get_option(app->m_term, GF_OPT_NAVIGATION);
	if (type != GF_NAVIGATE_WALK) return;
	pCmdUI->Enable(TRUE);
	pCmdUI->SetCheck(gf_term_get_option(app->m_term, GF_OPT_GRAVITY) ? 1 : 0);
}


BOOL CMainFrame::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult) 
{

	if (((LPNMHDR)lParam)->code == TBN_DROPDOWN) {
		RECT rc;
		s32 i, count, start;
		POINT pt;
		CMenu *pPopup = new CMenu();
		pPopup->CreatePopupMenu();

		m_wndToolBar.GetWindowRect(&rc);
		pt.y = rc.bottom;
		pt.x = rc.left;
		m_wndToolBar.GetToolBarCtrl().GetItemRect(0, &rc);
		pt.x += (rc.right - rc.left);
		m_wndToolBar.GetToolBarCtrl().GetItemRect(1, &rc);
		pt.x += (rc.right - rc.left);

		count = gf_list_count(m_pPlayList->m_entries);
		if ( ((LPNMTOOLBAR)lParam)->iItem == ID_NAV_PREV) {
			start = m_pPlayList->m_cur_entry - 1;
			for (i=0; i<10; i++) {
				if (start - i < 0) break;
				if (start - i >= count) break;
				PLEntry *ple = (PLEntry *) gf_list_get(m_pPlayList->m_entries, start - i);
				pPopup->AppendMenu(MF_STRING | MF_ENABLED, ID_NAV_PREV_0 + i, ple->m_disp_name);
			}
		} else {
			start = m_pPlayList->m_cur_entry + 1;
			for (i=0; i<10; i++) {
				if (start + i >= count) break;
				PLEntry *ple = (PLEntry *) gf_list_get(m_pPlayList->m_entries, start + i);
				pPopup->AppendMenu(MF_STRING | MF_ENABLED, ID_NAV_NEXT_0 + i, ple->m_disp_name);
			}
			m_wndToolBar.GetToolBarCtrl().GetItemRect(2, &rc);
			pt.x += (rc.right - rc.left);
		}
		pPopup->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, this);
		delete pPopup;

		return FALSE;
	}
	return CFrameWnd::OnNotify(wParam, lParam, pResult);
}

void CMainFrame::OnNavNext() 
{
	Osmo4 *app = GetApp();
	/*don't play if last could trigger playlist loop*/
	if ((m_pPlayList->m_cur_entry<0) || (gf_list_count(m_pPlayList->m_entries) == 1 + (u32) m_pPlayList->m_cur_entry)) return;
	m_pPlayList->PlayNext();
}

void CMainFrame::OnUpdateNavNext(CCmdUI* pCmdUI) 
{
	if (m_pPlayList->m_cur_entry<0) pCmdUI->Enable(FALSE);
	else if ((u32) m_pPlayList->m_cur_entry + 1 == gf_list_count(m_pPlayList->m_entries) ) pCmdUI->Enable(FALSE);
	else pCmdUI->Enable(TRUE);
}

void CMainFrame::OnNavPrev() 
{
	Osmo4 *app = GetApp();
	if (m_pPlayList->m_cur_entry<=0) return;
	m_pPlayList->PlayPrev();
}

void CMainFrame::OnUpdateNavPrev(CCmdUI* pCmdUI) 
{
	if (m_pPlayList->m_cur_entry<=0) pCmdUI->Enable(FALSE);
	else pCmdUI->Enable(TRUE);
}


void CMainFrame::OnClearNav() 
{
	m_pPlayList->ClearButPlaying();
}

void CMainFrame::OnViewPlaylist() 
{
	m_pPlayList->ShowWindow(m_pPlayList->IsWindowVisible() ? SW_HIDE : SW_SHOW);
}

void CMainFrame::OnUpdateViewPlaylist(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_pPlayList->IsWindowVisible() ? 1 : 0);
}
void CMainFrame::OnPlaylistLoop() 
{
	GetApp()->m_Loop = GetApp()->m_Loop ? GF_FALSE : GF_TRUE;
	gf_cfg_set_key(GetApp()->m_user.config, "General", "PlaylistLoop", GetApp()->m_Loop ? "yes" : "no");
}

void CMainFrame::OnUpdatePlaylistLoop(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(GetApp()->m_Loop ? GF_TRUE : GF_FALSE);
}

void CMainFrame::OnAddSubtitle() 
{
	CFileDialog fd(TRUE,NULL,NULL, OFN_HIDEREADONLY | OFN_FILEMUSTEXIST, "All Subtitles|*.srt;*.sub;*.ttxt;*.xml|SRT Subtitles|*.srt|SUB Subtitles|*.sub|3GPP TimedText|*.ttxt|QuckTime TeXML|*.xml|");
	if (fd.DoModal() != IDOK) return;

	AddSubtitle(fd.GetPathName(), GF_TRUE);
}

void CMainFrame::AddSubtitle(const char *fileName, Bool auto_play)
{
	gf_term_add_object(GetApp()->m_term, fileName, auto_play);
}

static Bool subs_enum_dir_item(void *cbck, char *item_name, char *item_path)
{
	CMainFrame *_this = (CMainFrame *)cbck;
	_this->AddSubtitle(item_path, GF_FALSE);
	return GF_FALSE;
}

void CMainFrame::LookForSubtitles()
{
	char dir[GF_MAX_PATH];
	CString url = m_pPlayList->GetURL();
	strcpy(dir, url);
	char *sep = strrchr(dir, '\\');
	if (!sep) ::GetCurrentDirectory(GF_MAX_PATH, dir);
	else sep[0] = 0;

	gf_enum_directory(dir, GF_FALSE, subs_enum_dir_item, this, "ttxt;srt");
}

void CMainFrame::OnCacheEnable()
{
	Osmo4 *app = GetApp();
	u32 state = gf_term_get_option(app->m_term, GF_OPT_MEDIA_CACHE);
	if (state==GF_MEDIA_CACHE_DISABLED) {
		gf_term_set_option(app->m_term, GF_OPT_MEDIA_CACHE, GF_MEDIA_CACHE_ENABLED);
	} else if (state==GF_MEDIA_CACHE_DISABLED) {
		gf_term_set_option(app->m_term, GF_OPT_MEDIA_CACHE, GF_MEDIA_CACHE_DISABLED);
	}
}

void CMainFrame::OnUpdateCacheEnable(CCmdUI* pCmdUI) 
{
	Osmo4 *app = GetApp();
	u32 state = gf_term_get_option(app->m_term, GF_OPT_MEDIA_CACHE);
	switch (state) {
	case GF_MEDIA_CACHE_ENABLED:
		pCmdUI->SetText("Enabled"); 
		pCmdUI->Enable(TRUE); 
		break;
	case GF_MEDIA_CACHE_RUNNING: 
		pCmdUI->SetText("Running");
		pCmdUI->Enable(FALSE); 
		break;
	case GF_MEDIA_CACHE_DISABLED: 
		pCmdUI->SetText("Disabled");
		break;
	}
}

void CMainFrame::OnUpdateCacheStop(CCmdUI* pCmdUI) 
{
	Osmo4 *app = GetApp();
	u32 state = gf_term_get_option(app->m_term, GF_OPT_MEDIA_CACHE);
	pCmdUI->Enable( (state==GF_MEDIA_CACHE_RUNNING) ? TRUE : FALSE);
}

void CMainFrame::OnCacheStop()
{
	Osmo4 *app = GetApp();
	gf_term_set_option(app->m_term, GF_OPT_MEDIA_CACHE, GF_MEDIA_CACHE_DISABLED);
}
void CMainFrame::OnCacheAbort()
{
	Osmo4 *app = GetApp();
	gf_term_set_option(app->m_term, GF_OPT_MEDIA_CACHE, GF_MEDIA_CACHE_DISCARD);
}

void CMainFrame::OnFileExit() 
{
	DestroyWindow();
}


void CMainFrame::BuildChapterList(Bool reset_only)
{
	CMenu *pChaps;
	GF_MediaInfo odi;
	NetInfoCommand com;
	Osmo4 *app = GetApp();

	/*THIS IS HARCODED FROM THE MENU LAYOUT */
	pChaps = GetMenu()->GetSubMenu(2)->GetSubMenu(1);
	while (pChaps->GetMenuItemCount()) pChaps->DeleteMenu(0, MF_BYPOSITION);

	if (m_chapters_start) gf_free(m_chapters_start);
	m_chapters_start = NULL;
	m_num_chapters = 0;
	if (reset_only) return;

	GF_ObjectManager *root_od = gf_term_get_root_object(app->m_term);
	if (!root_od) return;
	if (gf_term_get_object_info(app->m_term, root_od, &odi) != GF_OK) return;

	u32 count = gf_list_count(odi.od->OCIDescriptors);
	m_num_chapters = 0;
	for (u32 i=0; i<count; i++) {
		char szLabel[1024];
		GF_Segment *seg = (GF_Segment *) gf_list_get(odi.od->OCIDescriptors, i);
		if (seg->tag != GF_ODF_SEGMENT_TAG) continue;

		if (seg->SegmentName && strlen((const char *)seg->SegmentName)) {
			strcpy(szLabel, (const char *) seg->SegmentName);
		} else {
			sprintf(szLabel, "Chapter #%02d", m_num_chapters+1);
		}
		pChaps->AppendMenu(MF_ENABLED, ID_SETCHAP_FIRST + m_num_chapters, szLabel);

		m_chapters_start = (Double *) gf_realloc(m_chapters_start, sizeof(Double)*(m_num_chapters+1));
		m_chapters_start[m_num_chapters] = seg->startTime;
		m_num_chapters++;
	}

	/*get any service info*/
	if (!m_bStartupFile && gf_term_get_service_info(app->m_term, root_od, &com) == GF_OK) {
		CString title("");
		if (com.track_info) { title.Format("%02d ", (u32) (com.track_info>>16) ); }
		if (com.artist) { title += com.artist; title += " "; }
		if (com.name) { title += com.name; title += " "; }
		if (com.album) { title += "("; title += com.album; title += ")"; }
		
		if (title.GetLength()) SetWindowText(title);
	}
}

void CMainFrame::OnViewCPU()
{
	m_show_rti = m_show_rti ? GF_FALSE : GF_TRUE;
}

void CMainFrame::OnUpdateViewCPU(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(TRUE);
	pCmdUI->SetCheck(m_show_rti);
}


void CMainFrame::OnFileCopy()
{
	size_t len;
	const char *text = gf_term_get_text_selection(GetApp()->m_term, GF_FALSE);
	if (!text) return;

	if (!IsClipboardFormatAvailable(CF_TEXT)) return;
	if (!OpenClipboard()) return;
	EmptyClipboard();
	
	len = strlen(text);
	if (!len) return;

	HGLOBAL hglbCopy = GlobalAlloc(GMEM_MOVEABLE, (len + 1) * sizeof(char)); 
	LPTSTR lptstrCopy = (char *) GlobalLock(hglbCopy);
	memcpy(lptstrCopy, text, len * sizeof(char)); 
	lptstrCopy[len] = 0;
	GlobalUnlock(hglbCopy); 
	SetClipboardData(CF_TEXT, hglbCopy);
	CloseClipboard(); 
}

void CMainFrame::OnUpdateFileCopy(CCmdUI* pCmdUI)
{
	Osmo4 *app = GetApp();
	if (IsClipboardFormatAvailable(CF_TEXT)
		&& app->m_term 
		&& (gf_term_get_text_selection(app->m_term, GF_TRUE)!=NULL)
	) {
		pCmdUI->Enable(TRUE);
	} else {
		pCmdUI->Enable(FALSE);
	}
}


void CMainFrame::OnFilePaste()
{
	if (!IsClipboardFormatAvailable(CF_TEXT)) return;
	if (!OpenClipboard()) return;

	HGLOBAL hglbCopy = GetClipboardData(CF_TEXT);
	if (hglbCopy) {
		LPTSTR lptstrCopy = (char *) GlobalLock(hglbCopy);
		gf_term_paste_text(GetApp()->m_term, lptstrCopy, GF_FALSE);
		GlobalUnlock(hglbCopy); 
	}
	CloseClipboard(); 
}

void CMainFrame::OnUpdateFilePaste(CCmdUI* pCmdUI)
{
	Osmo4 *app = GetApp();
	if (IsClipboardFormatAvailable(CF_TEXT)
		&& app->m_term 
		&& (gf_term_paste_text(app->m_term, NULL, GF_TRUE)==GF_OK)
	) {
		pCmdUI->Enable(TRUE);
	} else {
		pCmdUI->Enable(FALSE);
	}
}


