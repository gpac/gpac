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




#define MAX_LAST_FILES		20
void UpdateLastFiles(GF_Config *cfg, const char *URL)
{
	u32 nb_entries;
	gf_cfg_set_key(cfg, "RecentFiles", URL, NULL);
	gf_cfg_insert_key(cfg, "RecentFiles", URL, "", 0);
	/*remove last entry if needed*/
	nb_entries = gf_cfg_get_key_count(cfg, "RecentFiles");
	if (nb_entries>MAX_LAST_FILES) {
		gf_cfg_set_key(cfg, "RecentFiles", gf_cfg_get_key_name(cfg, "RecentFiles", nb_entries-1), NULL);
	}
}


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

	Osmo4 *gpac = GetApp();

	m_url = URL;
	UpdateLastFiles(gpac->m_user.config, (const char *) URL);
	EndDialog(IDOK);
}

BOOL COpenUrl::OnInitDialog()
{
	CDialog::OnInitDialog();

	Osmo4 *gpac = GetApp();
	u32 i=0;

	while (m_URLs.GetCount()) m_URLs.DeleteString(0);
	while (1) {
		const char *sOpt = gf_cfg_get_key_name(gpac->m_user.config, "RecentFiles", i);
		if (!sOpt) break;
		m_URLs.AddString(sOpt);
		i++;
	}
	return TRUE;
}
