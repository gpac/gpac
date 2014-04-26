// Options.cpp : implementation file
//

#include "stdafx.h"
#include "Osmo4.h"
#include <gpac/options.h>
#include <gpac/modules/audio_out.h>
#include <gpac/modules/codec.h>
#include <gpac/modules/font.h>
#include <gpac/constants.h>
#include <gpac/iso639.h>

#include "Options.h"
#include <gx.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// COptions dialog


COptions::COptions(CWnd* pParent /*=NULL*/)
	: CDialog(COptions::IDD, pParent)
{
	//{{AFX_DATA_INIT(COptions)
	//}}AFX_DATA_INIT

}


void COptions::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptions)
	DDX_Control(pDX, IDC_COMBOSEL, m_Selection);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptions, CDialog)
	//{{AFX_MSG_MAP(COptions)
	ON_BN_CLICKED(IDC_SAVEOPT, OnSaveopt)
	ON_CBN_SELCHANGE(IDC_COMBOSEL, OnSelchangeCombosel)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptions message handlers


void COptions::OnSelchangeCombosel()
{
	HideAll();
	switch (m_Selection.GetCurSel()) {
	case 0:
		m_general.ShowWindow(SW_SHOW);
		break;
	case 1:
		m_systems.ShowWindow(SW_SHOW);
		break;
	case 2:
		m_decoder.ShowWindow(SW_SHOW);
		break;
	case 3:
		m_render.ShowWindow(SW_SHOW);
		break;
	case 4:
		m_render3D.ShowWindow(SW_SHOW);
		break;
	case 5:
		m_audio.ShowWindow(SW_SHOW);
		break;
	case 6:
		m_font.ShowWindow(SW_SHOW);
		break;
	case 7:
		m_http.ShowWindow(SW_SHOW);
		break;
	case 8:
		m_stream.ShowWindow(SW_SHOW);
		break;
	}
}

void COptions::HideAll()
{
	m_general.ShowWindow(SW_HIDE);
	m_systems.ShowWindow(SW_HIDE);
	m_render.ShowWindow(SW_HIDE);
	m_render3D.ShowWindow(SW_HIDE);
	m_audio.ShowWindow(SW_HIDE);
	m_http.ShowWindow(SW_HIDE);
	m_font.ShowWindow(SW_HIDE);
	m_stream.ShowWindow(SW_HIDE);
	m_decoder.ShowWindow(SW_HIDE);
}

BOOL COptions::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_general.Create(IDD_OPT_GEN, this);
	m_systems.Create(IDD_OPT_SYSTEMS, this);
	m_decoder.Create(IDD_OPT_DECODER, this);
	m_render.Create(IDD_OPT_RENDER, this);
	m_render3D.Create(IDD_OPT_RENDER3D, this);
	m_audio.Create(IDD_OPT_AUDIO, this);
	m_http.Create(IDD_OPT_HTTP, this);
	m_font.Create(IDD_OPT_FONT, this);
	m_stream.Create(IDD_OPT_STREAM, this);

	m_Selection.AddString(_T("General"));
	m_Selection.AddString(_T("MPEG-4 Systems"));
	m_Selection.AddString(_T("Decoders"));
	m_Selection.AddString(_T("Compositor"));
	m_Selection.AddString(_T("3D Rendering"));
	m_Selection.AddString(_T("Audio"));
	m_Selection.AddString(_T("Text"));
	m_Selection.AddString(_T("Download"));
	m_Selection.AddString(_T("Streaming"));
	HideAll();

	const char *sOpt = gf_cfg_get_key(GetApp()->m_user.config, "General", "ConfigPanel");
	u32 sel = sOpt ? atoi(sOpt) : 0;
	if (sel>8) sel=8;
	m_Selection.SetCurSel(sel);
	OnSelchangeCombosel();

	SetFocus();
	return TRUE;
}

void COptions::OnSaveopt()
{
	m_general.SaveOptions();
	m_systems.SaveOptions();
	m_render.SaveOptions();
	m_render3D.SaveOptions();
	m_audio.SaveOptions();
	m_http.SaveOptions();
	m_font.SaveOptions();
	m_stream.SaveOptions();
	m_decoder.SaveOptions();

	COsmo4 *gpac = GetApp();
	gf_term_set_option(gpac->m_term, GF_OPT_RELOAD_CONFIG, 1);
}

void COptions::OnOK()
{
	char str[20];
	sprintf(str, "%d", m_Selection.GetCurSel());
	gf_cfg_set_key(GetApp()->m_user.config, "General", "ConfigPanel", str);

	EndDialog(IDCANCEL);
}



COptAudio::COptAudio(CWnd* pParent /*=NULL*/)
	: CDialog(COptAudio::IDD, pParent)
{
	//{{AFX_DATA_INIT(COptAudio)
	//}}AFX_DATA_INIT
}


void COptAudio::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptAudio)
	DDX_Control(pDX, IDC_DRIVER_LIST, m_DriverList);
	DDX_Control(pDX, IDC_AUDIO_RESYNC, m_AudioResync);
	DDX_Control(pDX, IDC_AUDIO_DUR, m_AudioDur);
	DDX_Control(pDX, IDC_SPIN_DUR, m_SpinDur);
	DDX_Control(pDX, IDC_FORCE_AUDIO, m_ForceConfig);
	DDX_Control(pDX, IDC_SPIN_AUDIO, m_AudioSpin);
	DDX_Control(pDX, IDC_EDIT_AUDIO, m_AudioEdit);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptAudio, CDialog)
	//{{AFX_MSG_MAP(COptAudio)
	ON_BN_CLICKED(IDC_FORCE_AUDIO, OnForceAudio)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptAudio message handlers

BOOL COptAudio::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_AudioSpin.SetBuddy(& m_AudioEdit);
	m_SpinDur.SetBuddy(& m_AudioDur);
	m_SpinDur.SetRange(0, 1000);

	COsmo4 *gpac = GetApp();
	const char *sOpt;
	TCHAR wTmp[500];

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Audio", "ForceConfig");
	m_ForceConfig.SetCheck( (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Audio", "NumBuffers");
	if (sOpt) {
		CE_CharToWide((char *)sOpt, (u16 *)wTmp);
		m_AudioEdit.SetWindowText(wTmp);
	} else {
		m_AudioEdit.SetWindowText(_T("2"));
	}
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Audio", "TotalDuration");
	if (sOpt) {
		CE_CharToWide((char *)sOpt, (u16 *)wTmp);
		m_AudioDur.SetWindowText(wTmp);
	} else {
		m_AudioDur.SetWindowText(_T("200"));
	}

	OnForceAudio();

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Audio", "NoResync");
	if (sOpt && !stricmp(sOpt, "yes")) {
		m_AudioResync.SetCheck(1);
	} else {
		m_AudioResync.SetCheck(0);
	}

	/*driver enum*/
	while (m_DriverList.GetCount()) m_DriverList.DeleteString(0);
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Audio", "DriverName");
	u32 count = gf_modules_get_count(gpac->m_user.modules);
	GF_BaseInterface *ifce;
	s32 select = 0;
	s32 to_sel = 0;
	for (u32 i=0; i<count; i++) {
		ifce = gf_modules_load_interface(gpac->m_user.modules, i, GF_AUDIO_OUTPUT_INTERFACE);
		if (!ifce) continue;
		if (sOpt && !stricmp(((GF_BaseInterface *)ifce)->module_name, sOpt)) select = to_sel;
		CE_CharToWide((char *) ((GF_BaseInterface *)ifce)->module_name, (u16 *)wTmp);
		m_DriverList.AddString(wTmp);
		gf_modules_close_interface(ifce);
		to_sel++;
	}
	m_DriverList.SetCurSel(select);


	return TRUE;
}


void COptAudio::SaveOptions()
{
	COsmo4 *gpac = GetApp();
	TCHAR wstr[50];
	char str[50];

	gf_cfg_set_key(gpac->m_user.config, "Audio", "ForceConfig", m_ForceConfig.GetCheck() ? "yes" : "no");
	gf_cfg_set_key(gpac->m_user.config, "Audio", "NoResync", m_AudioResync.GetCheck() ? "yes" : "no");

	m_AudioEdit.GetWindowText(wstr, 20);
	CE_WideToChar((u16 *)wstr, str);
	gf_cfg_set_key(gpac->m_user.config, "Audio", "NumBuffers", str);
	m_AudioDur.GetWindowText(wstr, 20);
	CE_WideToChar((u16 *)wstr, str);
	gf_cfg_set_key(gpac->m_user.config, "Audio", "TotalDuration", str);

	m_DriverList.GetWindowText(wstr, 50);
	CE_WideToChar((u16 *)wstr, str);
	gf_cfg_set_key(gpac->m_user.config, "Audio", "DriverName", str);

}

void COptAudio::OnForceAudio()
{
	BOOL en = m_ForceConfig.GetCheck();

	m_AudioSpin.EnableWindow(en);
	m_AudioEdit.EnableWindow(en);
	m_SpinDur.EnableWindow(en);
	m_AudioDur.EnableWindow(en);
}


COptDecoder::COptDecoder(CWnd* pParent /*=NULL*/)
	: CDialog(COptDecoder::IDD, pParent)
{
	//{{AFX_DATA_INIT(COptDecoder)
	// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void COptDecoder::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptDecoder)
	DDX_Control(pDX, IDC_VIDEC_LIST, m_Video);
	DDX_Control(pDX, IDC_AUDEC_LIST, m_Audio);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptDecoder, CDialog)
	//{{AFX_MSG_MAP(COptDecoder)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptDecoder message handlers

BOOL COptDecoder::OnInitDialog()
{
	u32 i;
	CDialog::OnInitDialog();

	COsmo4 *gpac = GetApp();
	const char *sOpt;

	/*audio dec enum*/
	while (m_Audio.GetCount()) m_Audio.DeleteString(0);
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Systems", "DefAudioDec");
	u32 count = gf_modules_get_count(gpac->m_user.modules);
	GF_BaseDecoder *ifce;
	s32 select = 0;
	s32 to_sel = 0;
	for (i=0; i<count; i++) {
		ifce = (GF_BaseDecoder *) gf_modules_load_interface(gpac->m_user.modules, i, GF_MEDIA_DECODER_INTERFACE);
		if (!ifce) continue;
		if (ifce->CanHandleStream(ifce, GF_STREAM_AUDIO, NULL, 0)) {
			if (sOpt && !stricmp(((GF_BaseInterface *)ifce)->module_name, sOpt)) select = to_sel;
			TCHAR wzTmp[500];
			CE_CharToWide((char *) ifce->module_name, (u16 *)wzTmp);
			m_Audio.AddString(wzTmp);
			to_sel++;
		}
		gf_modules_close_interface((GF_BaseInterface *) ifce);
	}
	m_Audio.SetCurSel(select);

	/*audio dec enum*/
	while (m_Video.GetCount()) m_Video.DeleteString(0);
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Systems", "DefVideoDec");
	count = gf_modules_get_count(gpac->m_user.modules);
	select = 0;
	to_sel = 0;
	for (i=0; i<count; i++) {
		ifce  = (GF_BaseDecoder *) gf_modules_load_interface(gpac->m_user.modules, i, GF_MEDIA_DECODER_INTERFACE);
		if (!ifce) continue;
		if (ifce->CanHandleStream(ifce, GF_STREAM_VISUAL, NULL, 0)) {
			if (sOpt && !stricmp(((GF_BaseInterface *)ifce)->module_name, sOpt)) select = to_sel;
			TCHAR wzTmp[500];
			CE_CharToWide((char *) ifce->module_name, (u16 *)wzTmp);
			m_Video.AddString(wzTmp);
			to_sel++;
		}
		gf_modules_close_interface((GF_BaseInterface *) ifce);
	}
	m_Video.SetCurSel(select);

	return TRUE;
}

void COptDecoder::SaveOptions()
{
	COsmo4 *gpac = GetApp();
	TCHAR wstr[100];
	char str[100];

	m_Audio.GetWindowText(wstr, 50);
	CE_WideToChar((u16 *)wstr, str);
	gf_cfg_set_key(gpac->m_user.config, "Systems", "DefAudioDec", str);
	m_Video.GetWindowText(wstr, 50);
	CE_WideToChar((u16 *)wstr, str);
	gf_cfg_set_key(gpac->m_user.config, "Systems", "DefVideoDec", str);
}



COptFont::COptFont(CWnd* pParent /*=NULL*/)
	: CDialog(COptFont::IDD, pParent)
{
	//{{AFX_DATA_INIT(COptFont)
	//}}AFX_DATA_INIT
}


void COptFont::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptFont)
	DDX_Control(pDX, IDC_USE_TEXTURE, m_UseTexture);
	DDX_Control(pDX, IDC_FONT_LIST, m_Fonts);
	DDX_Control(pDX, IDC_BROWSE_FONT, m_BrowseFont);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptFont, CDialog)
	//{{AFX_MSG_MAP(COptFont)
	ON_BN_CLICKED(IDC_BROWSE_FONT, OnBrowseFont)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptFont message handlers

BOOL COptFont::OnInitDialog()
{
	u32 i;
	GF_BaseInterface *ifce;

	CDialog::OnInitDialog();

	COsmo4 *gpac = GetApp();
	TCHAR wTmp[500];
	const char *sOpt;

	/*video drivers enum*/
	while (m_Fonts.GetCount()) m_Fonts.DeleteString(0);
	sOpt = gf_cfg_get_key(gpac->m_user.config, "FontEngine", "DriverName");
	s32 to_sel = 0;
	s32 select = 0;
	u32 count = gf_modules_get_count(gpac->m_user.modules);
	for (i=0; i<count; i++) {
		ifce = gf_modules_load_interface(gpac->m_user.modules, i, GF_FONT_READER_INTERFACE);
		if (!ifce) continue;
		if (sOpt && !stricmp(((GF_BaseInterface *)ifce)->module_name, sOpt)) select = to_sel;
		CE_CharToWide((char *) ifce->module_name, (u16 *)wTmp);
		m_Fonts.AddString(wTmp);
		gf_modules_close_interface(ifce);
		to_sel++;
	}
	m_Fonts.SetCurSel(select);


	sOpt = gf_cfg_get_key(gpac->m_user.config, "FontEngine", "FontDirectory");
	CE_CharToWide((char *)sOpt, (u16 *)wTmp);
	if (sOpt) m_BrowseFont.SetWindowText(wTmp);

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Compositor", "TextureTextMode");
	m_UseTexture.SetCheck( (!sOpt || stricmp(sOpt, "Never")) ? 1 : 0);

	return TRUE;
}

void COptFont::OnBrowseFont()
{

}


void COptFont::SaveOptions()
{
	COsmo4 *gpac = GetApp();
	char str[MAX_PATH];
	TCHAR wstr[MAX_PATH];

	m_Fonts.GetWindowText(wstr, 50);
	CE_WideToChar((u16 *)wstr, str);
	gf_cfg_set_key(gpac->m_user.config, "FontEngine", "FontReader", str);
	m_BrowseFont.GetWindowText(wstr, 50);
	CE_WideToChar((u16 *)wstr, str);
	gf_cfg_set_key(gpac->m_user.config, "FontEngine", "FontDirectory", str);
	gf_cfg_set_key(gpac->m_user.config, "Compositor", "TextureTextMode", m_UseTexture.GetCheck() ? "Default" : "Never");
}



COptGen::COptGen(CWnd* pParent /*=NULL*/)
	: CDialog(COptGen::IDD, pParent)
{
	//{{AFX_DATA_INIT(COptGen)
	//}}AFX_DATA_INIT
}


void COptGen::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptGen)
	DDX_Control(pDX, IDC_NO_BACKLIGHT, m_NoBacklight);
	DDX_Control(pDX, IDC_FILL_SCREEN, m_Fill);
	DDX_Control(pDX, IDC_LOOP, m_Loop);
	DDX_Control(pDX, IDC_ENABLE_LOGS, m_Logs);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptGen, CDialog)
	//{{AFX_MSG_MAP(COptGen)
	ON_BN_CLICKED(IDC_FILEASSOC, OnFileassoc)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptGen message handlers



BOOL COptGen::OnInitDialog()
{
	CDialog::OnInitDialog();
	COsmo4 *gpac = GetApp();
	const char *sOpt;

	sOpt = gf_cfg_get_key(gpac->m_user.config, "General", "Loop");
	if (sOpt && !stricmp(sOpt, "yes")) {
		m_Loop.SetCheck(1);
	} else {
		m_Loop.SetCheck(0);
	}
	sOpt = gf_cfg_get_key(gpac->m_user.config, "General", "FillScreen");
	m_Fill.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(gpac->m_user.config, "General", "DisableBackLight");
	m_NoBacklight.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);

	sOpt = gf_cfg_get_key(gpac->m_user.config, "General", "Logs");
	m_Logs.SetCheck((sOpt && !strstr(sOpt, "none")) ? 1 : 0);
	return TRUE;
}

void COptGen::SaveOptions()
{
	COsmo4 *gpac = GetApp();

	gpac->m_Loop = m_Loop.GetCheck();
	gf_cfg_set_key(gpac->m_user.config, "General", "Loop", gpac->m_Loop ? "yes" : "no");
	gpac->m_fit_screen = m_Fill.GetCheck();
	gf_cfg_set_key(gpac->m_user.config, "General", "FillScreen", gpac->m_fit_screen ? "yes" : "no");
	gpac->m_disable_backlight = m_NoBacklight.GetCheck();
	gf_cfg_set_key(gpac->m_user.config, "General", "DisableBackLight", gpac->m_disable_backlight ? "yes" : "no");

	gpac->EnableLogs(m_Logs.GetCheck() ? 1 : 0);
}

void COptGen::OnFileassoc()
{
	HKEY hSection;
	TCHAR szDir[MAX_PATH];
	char szTemp[MAX_PATH];
	TCHAR cmd[MAX_PATH];
	DWORD ioSize = MAX_PATH;
	DWORD dwDisp;

	RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\Osmo4"), 0, KEY_READ, &hSection);

	GetModuleFileName(NULL, szDir, MAX_PATH);

	while (szDir[strlen((char *) szDir)-1] != (TCHAR) '\\') szDir[strlen((char *) szDir)-1] = 0;
	if (!hSection)
		RegCreateKeyEx(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\Osmo4"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hSection, &dwDisp);

	CE_WideToChar((u16 *)szDir, szTemp);
	/*overwrite install dir with current path*/
	RegSetValueEx(hSection, _T("Install_Dir"), 0, REG_SZ, (const unsigned char *) szTemp, strlen(szTemp)+1);
	RegCloseKey(hSection);


	/*overwrite .mp4 file associations */
	RegCreateKeyEx(HKEY_CLASSES_ROOT, _T("mp4file\\DefaultIcon"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hSection, &dwDisp);
	wcscpy(cmd, szDir);
	wcscat(cmd, _T("Osmo4.ico") );
	CE_WideToChar((u16 *)cmd, szTemp);

	RegSetValueEx(hSection, _T(""), 0, REG_SZ, (const unsigned char *) szTemp, strlen((const char *) szTemp)+1);
	RegCloseKey(hSection);

	RegCreateKeyEx(HKEY_CLASSES_ROOT, _T("mp4file\\Shell\\open\\command"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hSection, &dwDisp);
	wcscpy(cmd, szDir);
	wcscat(cmd, _T("Osmo4.exe \"%L\"") );
	CE_WideToChar((u16 *)cmd, szTemp);
	RegSetValueEx(hSection, _T(""), 0, REG_SZ, (const unsigned char *) szTemp, strlen(szTemp)+1);
	RegCloseKey(hSection);

	RegCreateKeyEx(HKEY_CLASSES_ROOT, _T(".mp4"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hSection, &dwDisp);
	RegSetValueEx(hSection, _T(""), 0, REG_SZ, (const unsigned char *) "mp4file", strlen("mp4file")+1);
	RegCloseKey(hSection);
}



COptHTTP::COptHTTP(CWnd* pParent /*=NULL*/)
	: CDialog(COptHTTP::IDD, pParent)
{
	//{{AFX_DATA_INIT(COptHTTP)
	//}}AFX_DATA_INIT
}


void COptHTTP::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptHTTP)
	DDX_Control(pDX, IDC_RESTART_CACHE, m_RestartFile);
	DDX_Control(pDX, IDC_CLEAN_CACHE, m_CleanCache);
	DDX_Control(pDX, IDC_BROWSE_CACHE, m_CacheDir);
	DDX_Control(pDX, IDC_SAX_PROGRESSIVE, m_Progressive);
	DDX_Control(pDX, IDC_SAX_DURATION, m_SaxDuration);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptHTTP, CDialog)
	//{{AFX_MSG_MAP(COptHTTP)
	ON_BN_CLICKED(IDC_BROWSE_CACHE, OnBrowseCache)
	ON_BN_CLICKED(IDC_SAX_PROGRESSIVE, OnProgressive)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptHTTP message handlers


void COptHTTP::OnBrowseCache()
{

}

BOOL COptHTTP::OnInitDialog()
{
	CDialog::OnInitDialog();

	COsmo4 *gpac = GetApp();
	TCHAR wTmp[500];
	const char *sOpt;

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Downloader", "CleanCache");
	m_CleanCache.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Downloader", "RestartFiles");
	m_RestartFile.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(gpac->m_user.config, "SAXLoader", "Progressive");
	m_Progressive.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);

	sOpt = gf_cfg_get_key(gpac->m_user.config, "SAXLoader", "MaxDuration");
	if (sOpt) {
		CE_CharToWide((char *) sOpt, (u16 *)wTmp);
		m_SaxDuration.SetWindowText(wTmp);
	} else {
		m_SaxDuration.SetWindowText( _T("30") );
	}

	sOpt = gf_cfg_get_key(gpac->m_user.config, "General", "CacheDirectory");
	CE_CharToWide((char *) sOpt, (u16 *)wTmp);
	if (sOpt) m_CacheDir.SetWindowText(wTmp);

	OnProgressive();
	return TRUE;
}

void COptHTTP::OnProgressive()
{
	m_SaxDuration.EnableWindow( m_Progressive.GetCheck() ? TRUE : FALSE );
}

void COptHTTP::SaveOptions()
{
	TCHAR wTmp[500];
	char szCacheDir[500];
	COsmo4 *gpac = GetApp();

	gf_cfg_set_key(gpac->m_user.config, "Downloader", "CleanCache", m_CleanCache.GetCheck() ? "yes" : "no");
	gf_cfg_set_key(gpac->m_user.config, "Downloader", "RestartFiles", m_RestartFile.GetCheck() ? "yes" : "no");
	gf_cfg_set_key(gpac->m_user.config, "SAXLoader", "Progressive", m_Progressive.GetCheck() ? "yes" : "no");

	m_SaxDuration.GetWindowText(wTmp, MAX_PATH);
	CE_WideToChar((u16 *)wTmp, szCacheDir);
	gf_cfg_set_key(gpac->m_user.config, "SAXLoader", "MaxDuration", szCacheDir);

	m_CacheDir.GetWindowText(wTmp, MAX_PATH);
	CE_WideToChar((u16 *)wTmp, szCacheDir);
	gf_cfg_set_key(gpac->m_user.config, "General", "CacheDirectory", szCacheDir);
}



COptRender::COptRender(CWnd* pParent /*=NULL*/)
	: CDialog(COptRender::IDD, pParent)
{
	//{{AFX_DATA_INIT(COptRender)
	//}}AFX_DATA_INIT
}


void COptRender::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptRender)
	DDX_Control(pDX, IDC_AA_LIST, m_Antialias);
	DDX_Control(pDX, IDC_FORCE_SIZE, m_ForceSize);
	DDX_Control(pDX, IDC_FAST_RENDER, m_HighSpeed);
	DDX_Control(pDX, IDC_ZOOM_SCALABLE, m_Scalable);
	DDX_Control(pDX, IDC_DIRECTRENDER, m_DirectRender);
	DDX_Control(pDX, IDC_BIFS_RATE, m_BIFSRate);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptRender, CDialog)
	//{{AFX_MSG_MAP(COptRender)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptRender message handlers



#define NUM_RATES 11
static char *BIFSRates[11] =
{
	"5.0",
	"7.5",
	"10.0",
	"12.5",
	"15.0",
	"24.0",
	"25.0",
	"30.0",
	"50.0",
	"60.0",
	"100.0",
};



BOOL COptRender::OnInitDialog()
{
	CDialog::OnInitDialog();

	COsmo4 *gpac = GetApp();
	const char *sOpt;

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Compositor", "DirectDraw");
	if (sOpt && !stricmp(sOpt, "yes")) {
		m_DirectRender.SetCheck(1);
	} else {
		m_DirectRender.SetCheck(0);
	}
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Compositor", "ScalableZoom");
	if (sOpt && !stricmp(sOpt, "no")) {
		m_Scalable.SetCheck(0);
	} else {
		m_Scalable.SetCheck(1);
	}
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Compositor", "ForceSceneSize");
	if (sOpt && !stricmp(sOpt, "yes")) {
		m_ForceSize.SetCheck(1);
	} else {
		m_ForceSize.SetCheck(0);
	}

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Compositor", "FrameRate");
	if (!sOpt) sOpt = "30.0";
	s32 select = 0;
	while (m_BIFSRate.GetCount()) m_BIFSRate.DeleteString(0);
	for (s32 i = 0; i<NUM_RATES; i++) {
		TCHAR szText[100];
		CE_CharToWide(BIFSRates[i], (u16 *)szText);
		m_BIFSRate.AddString(szText);
		if (sOpt && !stricmp(sOpt, BIFSRates[i]) ) select = i;
	}
	m_BIFSRate.SetCurSel(select);

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Compositor", "HighSpeed");
	m_HighSpeed.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Compositor", "AntiAlias");
	while (m_Antialias.GetCount()) m_Antialias.DeleteString(0);

	m_Antialias.AddString(_T("None"));
	m_Antialias.AddString(_T("Text only"));
	m_Antialias.AddString(_T("Complete"));
	select = 2;
	if (sOpt && !stricmp(sOpt, "Text")) select = 1;
	else if (sOpt && !stricmp(sOpt, "None")) select = 0;
	m_Antialias.SetCurSel(select);

	return TRUE;
}


void COptRender::SaveOptions()
{
	COsmo4 *gpac = GetApp();

	gf_cfg_set_key(gpac->m_user.config, "Compositor", "DirectDraw", m_DirectRender.GetCheck() ? "yes" : "no");
	gf_cfg_set_key(gpac->m_user.config, "Compositor", "ScalableZoom", m_Scalable.GetCheck() ? "yes" : "no");
	gf_cfg_set_key(gpac->m_user.config, "Compositor", "HighSpeed", m_HighSpeed.GetCheck() ? "yes" : "no");
	gf_cfg_set_key(gpac->m_user.config, "Compositor", "ForceSceneSize", m_ForceSize.GetCheck() ? "yes" : "no");

	s32 sel = m_BIFSRate.GetCurSel();
	gf_cfg_set_key(gpac->m_user.config, "Compositor", "FrameRate", BIFSRates[sel]);

	sel = m_Antialias.GetCurSel();
	gf_cfg_set_key(gpac->m_user.config, "Compositor", "AntiAlias", (sel==0) ? "None" : ( (sel==1) ? "Text" : "All"));
}




COptRender3D::COptRender3D(CWnd* pParent /*=NULL*/)
	: CDialog(COptRender3D::IDD, pParent)
{
	//{{AFX_DATA_INIT(COptRender)
	//}}AFX_DATA_INIT
}


void COptRender3D::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptRender)
	DDX_Control(pDX, IDC_WIRE_MODE, m_WireMode);
	DDX_Control(pDX, IDC_DRAW_NORMALS, m_DrawNormals);
	DDX_Control(pDX, IDC_USE_3D_REN, m_Use3DRender);
	DDX_Control(pDX, IDC_NO_BACKCULL, m_NoBackFace);
	DDX_Control(pDX, IDC_EMULATE_POW2, m_EmulatePOW2);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptRender3D, CDialog)
	//{{AFX_MSG_MAP(COptRender3D)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL COptRender3D::OnInitDialog()
{
	CDialog::OnInitDialog();

	COsmo4 *gpac = GetApp();
	const char *sOpt;

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Compositor", "ForceOpenGL");
	m_Use3DRender.SetCheck( (sOpt && !strcmp(sOpt, "yes")) ? 1 : 0);

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Compositor", "BackFaceCulling");
	m_NoBackFace.SetCheck( (sOpt && !stricmp(sOpt, "Off")) ? 1 : 0);

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Compositor", "EmulatePOW2");
	m_EmulatePOW2.SetCheck( (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);

	m_WireMode.ResetContent();
	m_WireMode.AddString(_T("Solid Draw"));
	m_WireMode.AddString(_T("Wireframe"));
	m_WireMode.AddString(_T("Both"));
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Compositor", "Wireframe");
	if (sOpt && !stricmp(sOpt, "WireOnly")) m_WireMode.SetCurSel(1);
	else if (sOpt && !stricmp(sOpt, "WireOnSolid")) m_WireMode.SetCurSel(2);
	else m_WireMode.SetCurSel(0);


	m_DrawNormals.ResetContent();
	m_DrawNormals.AddString(_T("Never"));
	m_DrawNormals.AddString(_T("Per Face"));
	m_DrawNormals.AddString(_T("Per Vertex"));
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Compositor", "DrawNormals");
	if (sOpt && !stricmp(sOpt, "PerFace")) m_DrawNormals.SetCurSel(1);
	else if (sOpt && !stricmp(sOpt, "PerVertex")) m_DrawNormals.SetCurSel(2);
	else m_DrawNormals.SetCurSel(0);

	return TRUE;
}


void COptRender3D::SaveOptions()
{
	COsmo4 *gpac = GetApp();

	u32 sel = m_DrawNormals.GetCurSel();
	gf_cfg_set_key(gpac->m_user.config, "Compositor", "DrawNormals", (sel==2) ? "PerVertex" : (sel==1) ? "PerFace" : "Never");

	sel = m_WireMode.GetCurSel();
	gf_cfg_set_key(gpac->m_user.config, "Compositor", "Wireframe", (sel==2) ? "WireOnSolid" : (sel==1) ? "WireOnly" : "WireNone");

	gf_cfg_set_key(gpac->m_user.config, "Compositor", "BackFaceCulling", m_NoBackFace.GetCheck() ? "Off" : "On");
	gf_cfg_set_key(gpac->m_user.config, "Compositor", "EmulatePOW2", m_EmulatePOW2.GetCheck() ? "yes" : "no");
}


COptStream::COptStream(CWnd* pParent /*=NULL*/)
	: CDialog(COptStream::IDD, pParent)
{
	//{{AFX_DATA_INIT(COptStream)
	//}}AFX_DATA_INIT
}


void COptStream::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptStream)
	DDX_Control(pDX, IDC_REBUFFER_LEN, m_RebufferLen);
	DDX_Control(pDX, IDC_REBUFFER, m_Rebuffer);
	DDX_Control(pDX, IDC_BUFFER, m_Buffer);
	DDX_Control(pDX, IDC_TIMEOUT, m_Timeout);
	DDX_Control(pDX, IDC_REORDER, m_Reorder);
	DDX_Control(pDX, IDC_RTSP, m_UseRTSP);
	DDX_Control(pDX, IDC_PORT, m_Port);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptStream, CDialog)
	//{{AFX_MSG_MAP(COptStream)
	ON_CBN_SELCHANGE(IDC_PORT, OnSelchangePort)
	ON_BN_CLICKED(IDC_RTSP, OnRtsp)
	ON_BN_CLICKED(IDC_REBUFFER, OnRebuffer)
	ON_EN_UPDATE(IDC_REBUFFER_LEN, OnUpdateRebufferLen)
	ON_EN_UPDATE(IDC_BUFFER, OnUpdateBuffer)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptStream message handlers

BOOL COptStream::OnInitDialog()
{
	CDialog::OnInitDialog();

	COsmo4 *gpac = GetApp();
	TCHAR wTmp[500];
	const char *sOpt;

	while (m_Port.GetCount()) m_Port.DeleteString(0);
	m_Port.AddString(_T("554 (RTSP standard)"));
	m_Port.AddString(_T("7070 (RTSP ext)"));
	m_Port.AddString(_T("80 (RTSP / HTTP tunnel)"));
	m_Port.AddString(_T("8080 (RTSP / HTTP tunnel)"));

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Streaming", "DefaultPort");
	u32 port = 554;
	Bool force_rtsp = 0;;
	if (sOpt) port = atoi(sOpt);
	switch (port) {
	case 8080:
		m_Port.SetCurSel(3);
		force_rtsp = 1;
		break;
	case 80:
		m_Port.SetCurSel(2);
		force_rtsp = 1;
		break;
	case 7070:
		m_Port.SetCurSel(1);
		break;
	default:
		m_Port.SetCurSel(0);
		break;
	}

	Bool use_rtsp = 0;
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Streaming", "RTPoverRTSP");
	if (sOpt && !stricmp(sOpt, "yes")) use_rtsp = 1;

	if (force_rtsp) {
		m_UseRTSP.SetCheck(1);
		m_UseRTSP.EnableWindow(0);
		m_Reorder.SetCheck(0);
		m_Reorder.EnableWindow(0);
	} else {
		m_UseRTSP.SetCheck(use_rtsp);
		m_UseRTSP.EnableWindow(1);
		m_Reorder.EnableWindow(1);
		sOpt = gf_cfg_get_key(gpac->m_user.config, "Streaming", "ReorderSize");
		if (sOpt && !stricmp(sOpt, "0")) {
			m_Reorder.SetCheck(0);
		} else {
			m_Reorder.SetCheck(1);
		}
	}

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Streaming", "RTSPTimeout");
	if (sOpt) {
		CE_CharToWide((char *) sOpt, (u16 *)wTmp);
		m_Timeout.SetWindowText(wTmp);
	} else {
		m_Timeout.SetWindowText(_T("30000"));
	}

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Network", "BufferLength");
	if (sOpt) {
		CE_CharToWide((char *) sOpt, (u16 *)wTmp);
		m_Buffer.SetWindowText(wTmp);
	} else {
		m_Buffer.SetWindowText(_T("3000"));
	}

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Network", "RebufferLength");
	u32 buf_len = 0;
	if (sOpt) buf_len = atoi(sOpt);
	if (buf_len) {
		CE_CharToWide((char *) sOpt, (u16 *)wTmp);
		m_RebufferLen.SetWindowText(wTmp);
		m_Rebuffer.SetCheck(1);
		m_RebufferLen.EnableWindow(1);
	} else {
		m_RebufferLen.SetWindowText(_T("0"));
		m_Rebuffer.SetCheck(0);
		m_RebufferLen.EnableWindow(0);
	}

	return TRUE;
}


void COptStream::OnSelchangePort()
{
	s32 sel = m_Port.GetCurSel();
	switch (sel) {
	case 3:
	case 2:
		m_UseRTSP.SetCheck(1);
		m_UseRTSP.EnableWindow(0);
		m_Reorder.SetCheck(0);
		m_Reorder.EnableWindow(0);
		break;
	case 1:
	default:
		m_UseRTSP.SetCheck(0);
		m_UseRTSP.EnableWindow(1);
		m_Reorder.SetCheck(1);
		m_Reorder.EnableWindow(1);
		break;
	}
}

void COptStream::OnRtsp()
{
	if (m_UseRTSP.GetCheck()) {
		m_Reorder.SetCheck(0);
		m_Reorder.EnableWindow(0);
	} else {
		m_Reorder.SetCheck(1);
		m_Reorder.EnableWindow(1);
	}

}

void COptStream::CheckRebuffer()
{
	TCHAR wstr[50];
	char str[50];
	s32 buf, rebuf;
	m_Buffer.GetWindowText(wstr, 50);
	CE_WideToChar((u16 *)wstr, str);
	buf = atoi(str);
	m_RebufferLen.GetWindowText(wstr, 50);
	CE_WideToChar((u16 *)wstr, str);
	rebuf = atoi(str);
	if (rebuf*2 > buf) {
		rebuf = buf/2;
		wsprintf(wstr, _T("%d"), rebuf);
		m_RebufferLen.SetWindowText(wstr);
	}
}

void COptStream::OnRebuffer()
{
	if (!m_Rebuffer.GetCheck()) {
		m_RebufferLen.EnableWindow(0);
	} else {
		m_RebufferLen.EnableWindow(1);
		CheckRebuffer();
	}
}

void COptStream::OnUpdateRebufferLen()
{
	CheckRebuffer();
}

void COptStream::OnUpdateBuffer()
{
	CheckRebuffer();
}

void COptStream::SaveOptions()
{
	COsmo4 *gpac = GetApp();
	Bool force_rtsp = 0;
	s32 sel = m_Port.GetCurSel();
	switch (sel) {
	case 3:
		gf_cfg_set_key(gpac->m_user.config, "Streaming", "DefaultPort", "8080");
		force_rtsp = 1;
		break;
	case 2:
		gf_cfg_set_key(gpac->m_user.config, "Streaming", "DefaultPort", "80");
		force_rtsp = 1;
		break;
	case 1:
		gf_cfg_set_key(gpac->m_user.config, "Streaming", "DefaultPort", "7070");
		break;
	default:
		gf_cfg_set_key(gpac->m_user.config, "Streaming", "DefaultPort", "554");
		break;
	}

	if (force_rtsp) {
		gf_cfg_set_key(gpac->m_user.config, "Streaming", "RTPoverRTSP", "yes");
	} else {
		gf_cfg_set_key(gpac->m_user.config, "Streaming", "RTPoverRTSP", m_UseRTSP.GetCheck() ? "yes" : "no");
		if (!m_UseRTSP.GetCheck()) gf_cfg_set_key(gpac->m_user.config, "Streaming", "ReorderSize", m_Reorder.GetCheck() ? "30" : "0");
	}

	TCHAR wstr[50];
	char str[50];

	m_Timeout.GetWindowText(wstr, 50);
	CE_WideToChar((u16 *)wstr, str);
	gf_cfg_set_key(gpac->m_user.config, "Streaming", "RTSPTimeout", str);

	m_Buffer.GetWindowText(wstr, 50);
	CE_WideToChar((u16 *)wstr, str);
	gf_cfg_set_key(gpac->m_user.config, "Network", "BufferLength", str);
	if (m_Rebuffer.GetCheck()) {
		m_RebufferLen.GetWindowText(wstr, 50);
		CE_WideToChar((u16 *)wstr, str);
		gf_cfg_set_key(gpac->m_user.config, "Network", "RebufferLength", str);
	} else {
		gf_cfg_set_key(gpac->m_user.config, "Network", "RebufferLength", "0");
	}
}



COptSystems::COptSystems(CWnd* pParent /*=NULL*/)
	: CDialog(COptSystems::IDD, pParent)
{
	//{{AFX_DATA_INIT(COptSystems)
	// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void COptSystems::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptSystems)
	DDX_Control(pDX, IDC_FORCE_DURATION, m_ForceDuration);
	DDX_Control(pDX, IDC_DEC_THREAD, m_Threading);
	DDX_Control(pDX, IDC_BIFSDROP, m_BifsAlwaysDrawn);
	DDX_Control(pDX, IDC_LANG, m_Lang);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptSystems, CDialog)
	//{{AFX_MSG_MAP(COptSystems)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptSystems message handlers



BOOL COptSystems::OnInitDialog()
{
	CDialog::OnInitDialog();

	COsmo4 *gpac = GetApp();
	const char *sOpt;

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Systems", "Language3CC");
	if (!sOpt) sOpt = "eng";
	s32 select = 0;
	while (m_Lang.GetCount()) m_Lang.DeleteString(0);
	u32 i=0;
	while (GF_ISO639_Lang[i]) {
		TCHAR szTmp[100];
		/*only use common languages (having both 2- and 3-char code names)*/
		if (GF_ISO639_Lang[i+2][0]) {
			CE_CharToWide( (char *)GF_ISO639_Lang[i], (u16 *)szTmp);
			m_Lang.AddString(szTmp);
			if (sOpt && !stricmp(sOpt, GF_ISO639_Lang[i+1])) select = m_Lang.GetCount() - 1;
		}
		i+=3;
	}
	m_Lang.SetCurSel(select);


	/*system config*/
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Systems", "ThreadingPolicy");
	select = 0;
	while (m_Threading.GetCount()) m_Threading.DeleteString(0);
	m_Threading.AddString(_T("Single Thread"));
	m_Threading.AddString(_T("Mutli Thread"));
	if (sOpt && !stricmp(sOpt, "Multi")) select = 1;
	m_Threading.AddString(_T("Free"));
	if (sOpt && !stricmp(sOpt, "Free")) select = 2;
	m_Threading.SetCurSel(select);


	sOpt = gf_cfg_get_key(gpac->m_user.config, "Systems", "ForceSingleClock");
	if (sOpt && !stricmp(sOpt, "yes")) {
		m_ForceDuration.SetCheck(1);
	} else {
		m_ForceDuration.SetCheck(0);
	}
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Systems", "AlwaysDrawBIFS");
	if (sOpt && !stricmp(sOpt, "yes")) {
		m_BifsAlwaysDrawn.SetCheck(1);
	} else {
		m_BifsAlwaysDrawn.SetCheck(0);
	}


	return TRUE;
}


void COptSystems::SaveOptions()
{
	COsmo4 *gpac = GetApp();

	s32 sel = m_Lang.GetCurSel();
	u32 i=0;
	while (GF_ISO639_Lang[i]) {
		/*only use common languages (having both 2- and 3-char code names)*/
		if (GF_ISO639_Lang[i+2][0]) {
			if (!sel) break;
			sel--;
		}
		i+=3;
	}
	gf_cfg_set_key(gpac->m_user.config, "Systems", "LanguageName", GF_ISO639_Lang[i]);
	gf_cfg_set_key(gpac->m_user.config, "Systems", "Language3CC", GF_ISO639_Lang[i+1]);
	gf_cfg_set_key(gpac->m_user.config, "Systems", "Language2CC", GF_ISO639_Lang[i+2]);

	sel = m_Threading.GetCurSel();
	gf_cfg_set_key(gpac->m_user.config, "Systems", "ThreadingPolicy", (sel==0) ? "Single" : ( (sel==1) ? "Multi" : "Free"));

	/*reset duration flag*/
	gpac->m_duration = (u32) -1;
	gf_cfg_set_key(gpac->m_user.config, "Systems", "ForceSingleClock", m_ForceDuration.GetCheck() ? "yes" : "no");
	gf_cfg_set_key(gpac->m_user.config, "Systems", "AlwaysDrawBIFS", m_BifsAlwaysDrawn.GetCheck() ? "yes" : "no");

}

