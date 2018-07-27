// OpenDlg.cpp : implementation file
//
#include "stdafx.h"
#include "resource.h"
#include "OpenDlg.h"
#include "Osmo4.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// OpenDlg dialog


OpenDlg::OpenDlg(CWnd* pParent /*=NULL*/)
	: CDialog(OpenDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(OpenDlg)
	// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void OpenDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(OpenDlg)
	DDX_Control(pDX, IDC_FILELIST, m_URLs);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(OpenDlg, CDialog)
	//{{AFX_MSG_MAP(OpenDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void OpenDlg::OnOK()
{
	CString URL;
	char szUrl[5000];

	int sel = m_URLs.GetCurSel();
	if (sel == CB_ERR) {
		m_URLs.GetWindowText(URL);
	} else {
		m_URLs.GetLBText(sel, URL);
	}
	if (!URL.GetLength()) {
		EndDialog(IDCANCEL);
		return;
	}
	COsmo4 *app = GetApp();
	u32 nb_entries;

	app->m_filename = URL;

	CE_WideToChar((unsigned short *) (LPCTSTR) URL, szUrl);

	gf_cfg_set_key(app->m_user.config, "RecentFiles", szUrl, NULL);
	gf_cfg_insert_key(app->m_user.config, "RecentFiles", szUrl, "", 0);
	/*remove last entry if needed*/
	nb_entries = gf_cfg_get_key_count(app->m_user.config, "RecentFiles");
	if (nb_entries>20) {
		gf_cfg_set_key(app->m_user.config, "RecentFiles", gf_cfg_get_key_name(app->m_user.config, "RecentFiles", nb_entries-1), NULL);
	}
	EndDialog(IDOK);
}

BOOL OpenDlg::OnInitDialog()
{
	TCHAR w_str[5000];
	CDialog::OnInitDialog();
	COsmo4 *app = GetApp();
	const char *sOpt;
	u32 i=0;

	while (m_URLs.GetCount()) m_URLs.DeleteString(0);
	while (1) {
		sOpt = gf_cfg_get_key_name(app->m_user.config, "RecentFiles", i);
		if (!sOpt) break;
		CE_CharToWide((char *) sOpt, (u16 *)w_str);
		m_URLs.AddString(w_str);
		i++;
	}

	SetFocus();
	return TRUE;
}
