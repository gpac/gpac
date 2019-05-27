// AddressBar.cpp : implementation file
//

#include "stdafx.h"
#include "osmo4.h"
#include "MainFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// AddressBar dialog

IMPLEMENT_DYNAMIC(CInitDialogBar, CDialogBar)

BEGIN_MESSAGE_MAP(CInitDialogBar, CDialogBar)
END_MESSAGE_MAP()


CInitDialogBar::CInitDialogBar()
{
}

CInitDialogBar::~CInitDialogBar()
{
}

BOOL CInitDialogBar::Create(CWnd * pParentWnd, LPCTSTR lpszTemplateName, UINT nStyle, UINT nID)
{
	if(!CDialogBar::Create(pParentWnd, lpszTemplateName, nStyle, nID))
		return FALSE;

	if (!OnInitDialog()) return FALSE;

	return TRUE;
}

BOOL CInitDialogBar::Create(CWnd * pParentWnd, UINT nIDTemplate, UINT nStyle, UINT nID)
{
	if(!Create(pParentWnd, MAKEINTRESOURCE(nIDTemplate), nStyle, nID)) return FALSE;

	if(!OnInitDialog()) return FALSE;
	return TRUE;
}


BOOL CInitDialogBar::OnInitDialog()
{
	UpdateData(FALSE);
	return TRUE;
}

void CInitDialogBar::DoDataExchange(CDataExchange* pDX)
{
	CDialogBar::DoDataExchange(pDX);
}



IMPLEMENT_DYNAMIC(AddressBar, CInitDialogBar)


BEGIN_MESSAGE_MAP(AddressBar, CInitDialogBar)
	//{{AFX_MSG_MAP(AddressBar)
	ON_WM_SIZE()
	ON_WM_CLOSE()
	ON_CBN_SELENDOK(IDC_ADDRESS, OnSelendOK)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


AddressBar::AddressBar () : CInitDialogBar()
{
}

BOOL AddressBar::OnInitDialog()
{
	CInitDialogBar::OnInitDialog();

	return TRUE;
}


void AddressBar::DoDataExchange(CDataExchange* pDX)
{
	CInitDialogBar::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(AddressBar)
	DDX_Control(pDX, IDC_DUMTXT, m_Title);
	DDX_Control(pDX, IDC_ADDRESS, m_Address);
	//}}AFX_DATA_MAP
}


/////////////////////////////////////////////////////////////////////////////
// AddressBar message handlers

void AddressBar::OnSize(UINT nType, int cx, int cy)
{
	u32 w;
	POINT pt;
	//CDialog::OnSize(nType, cx, cy);

	if (!m_Address.m_hWnd) return;
	RECT rc;
	m_Title.GetClientRect(&rc);
	w = rc.right - rc.left;
	m_Address.GetWindowRect(&rc);
	pt.x = rc.left;
	pt.y = rc.top;
	ScreenToClient(&pt);
	rc.right = cx - pt.x;
	m_Address.SetWindowPos(this, 0, 0, rc.right, rc.bottom, SWP_NOZORDER | SWP_NOMOVE);

}

void AddressBar::OnClose()
{
}

void AddressBar::ReloadURLs()
{
	Osmo4 *gpac = GetApp();
	u32 i=0;

	while (m_Address.GetCount()) m_Address.DeleteString(0);
	while (1) {
		const char *sOpt = gf_cfg_get_key_name(gpac->m_user.config, "RecentFiles", i);
		if (!sOpt) return;
		m_Address.AddString(sOpt);
		i++;
	}
}

void AddressBar::SelectionReady()
{
	void UpdateLastFiles(GF_Config *cfg, const char *URL);

	CString URL;
	int sel = m_Address.GetCurSel();
	if (sel == CB_ERR) {
		m_Address.GetWindowText(URL);
	} else {
		m_Address.GetLBText(sel, URL);
	}
	if (!URL.GetLength()) return;
	Osmo4 *gpac = GetApp();
	Playlist *pl = ((CMainFrame*)gpac->m_pMainWnd)->m_pPlayList;
	/*don't store local files*/
	if (URL.Find("://", 0)>0) {
		UpdateLastFiles(gpac->m_user.config, URL);
		ReloadURLs();
	}
	pl->Truncate();
	pl->QueueURL(URL);
	pl->RefreshList();
	pl->PlayNext();
}

void AddressBar::OnSelendOK()
{
	SelectionReady();
}

BOOL AddressBar::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN) {
		switch (pMsg->wParam) {
		case VK_RETURN:
			::TranslateMessage(pMsg);
			::DispatchMessage(pMsg);
			SelectionReady();
			return TRUE;
		default:
			break;
		}
	}
	return CInitDialogBar::PreTranslateMessage(pMsg);
}

