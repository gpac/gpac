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
	const char *sOpt;
	char filename[1024];
	u32 i=0;

	app->m_filename = URL;

	CE_WideToChar((unsigned short *) (LPCTSTR) URL, szUrl);

	while (1) {
		sprintf(filename, "last_file_%d", i);
		sOpt = gf_cfg_get_key(app->m_user.config, "General", filename);
		if (!sOpt) break;
		if (!strcmp(sOpt, szUrl)) {
			EndDialog(IDOK);
			return;
		}
		i++;
	}
	if (i<10) {
		gf_cfg_set_key(app->m_user.config, "General", filename, szUrl);
	} else {
		gf_cfg_set_key(app->m_user.config, "General", "last_file_10", szUrl);
	}
	CDialog::OnOK();
}

BOOL OpenDlg::OnInitDialog() 
{
	TCHAR w_str[5000];
	CDialog::OnInitDialog();
	
	COsmo4 *app = GetApp();
	const char *sOpt;
	char filename[1024];
	u32 i=0;

	while (m_URLs.GetCount()) m_URLs.DeleteString(0);
	while (1) {
		sprintf(filename, "last_file_%d", i);
		sOpt = gf_cfg_get_key(app->m_user.config, "General", filename);
		if (!sOpt) 
			break;

		CE_CharToWide((char *) sOpt, w_str);
		m_URLs.AddString(w_str);
		i++;
	}

	SetFocus();
	return TRUE;  
}
