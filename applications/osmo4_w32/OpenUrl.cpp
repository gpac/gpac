// OpenUrl.cpp : implementation file
//

#include "stdafx.h"
#include "Osmo4.h"
#include "OpenUrl.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// COpenUrl dialog


COpenUrl::COpenUrl(CWnd* pParent /*=NULL*/)
	: CDialog(COpenUrl::IDD, pParent)
{
	//{{AFX_DATA_INIT(COpenUrl)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void COpenUrl::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COpenUrl)
	DDX_Control(pDX, IDC_COMBOURL, m_URLs);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COpenUrl, CDialog)
	//{{AFX_MSG_MAP(COpenUrl)
	ON_BN_CLICKED(IDC_BUTGO, OnButgo)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COpenUrl message handlers


void COpenUrl::OnButgo() 
{
	CString URL;
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

	WinGPAC *gpac = GetApp();
	const char *sOpt;
	char filename[1024];
	u32 i=0;

	m_url = URL;

	while (1) {
		sprintf(filename, "last_file_%d", i);
		sOpt = gf_cfg_get_key(gpac->m_user.config, "General", filename);
		if (!sOpt) break;
		if (!strcmp(sOpt, URL)) {
			EndDialog(IDOK);
			return;
		}
		i++;
	}
	/*add it*/
	if (i<10) {
		gf_cfg_set_key(gpac->m_user.config, "General", filename, URL);
	} else {
		gf_cfg_set_key(gpac->m_user.config, "General", "last_file_10", URL);
	}
	EndDialog(IDOK);
}

BOOL COpenUrl::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	WinGPAC *gpac = GetApp();
	const char *sOpt;
	char filename[1024];
	u32 i=0;

	while (m_URLs.GetCount()) m_URLs.DeleteString(0);
	while (1) {
		sprintf(filename, "last_file_%d", i);
		sOpt = gf_cfg_get_key(gpac->m_user.config, "General", filename);
		if (!sOpt) break;
		m_URLs.AddString(sOpt);
		i++;
	}
	return TRUE;
}
