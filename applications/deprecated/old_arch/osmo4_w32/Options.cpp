// Options.cpp : implementation file
//

#include "stdafx.h"
#include "Osmo4.h"
#include "MainFrm.h"

#include <gpac/modules/codec.h>
#include <gpac/modules/raster2d.h>
#include <gpac/modules/font.h>
#include <gpac/modules/audio_out.h>
#include <gpac/modules/video_out.h>

#include <gpac/iso639.h>
#include "Options.h"

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
	DDX_Control(pDX, IDC_SELECT, m_Selector);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptions, CDialog)
	//{{AFX_MSG_MAP(COptions)
	ON_BN_CLICKED(IDC_SAVEOPT, OnSaveopt)
	ON_WM_CLOSE()
	ON_WM_DESTROY()
	ON_CBN_SELCHANGE(IDC_SELECT, OnSelchangeSelect)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL COptions::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_general.Create(IDD_OPT_GEN, this);
	m_systems.Create(IDD_OPT_SYSTEMS, this);
	m_render.Create(IDD_OPT_RENDER, this);
	m_render2d.Create(IDD_OPT_RENDER2D, this);
	m_render3d.Create(IDD_OPT_RENDER3D, this);
	m_decoder.Create(IDD_OPT_DECODER, this);
	m_audio.Create(IDD_OPT_AUDIO, this);
	m_video.Create(IDD_OPT_VIDEO, this);
	m_http.Create(IDD_OPT_HTTP, this);
	m_font.Create(IDD_OPT_FONT, this);
	m_stream.Create(IDD_OPT_STREAM, this);
	m_cache.Create(IDD_OPT_MCACHE, this);
	m_files.Create(IDD_OPT_FILETYPES, this);
	m_logs.Create(IDD_OPT_LOGS, this);

	m_Selector.AddString("General");
	m_Selector.AddString("MPEG-4 Systems");
	m_Selector.AddString("Media Decoders");
	m_Selector.AddString("Compositor");
	m_Selector.AddString("2D Drawing");
	m_Selector.AddString("3D Drawing");
	m_Selector.AddString("Video Output");
	m_Selector.AddString("Audio Output");
	m_Selector.AddString("Text Engine");
	m_Selector.AddString("File Download");
	m_Selector.AddString("Real-Time Streaming");
	m_Selector.AddString("Streaming Cache");
	m_Selector.AddString("File Types");
	m_Selector.AddString("Log System");

	HideAll();

	const char *sOpt = gf_cfg_get_key(GetApp()->m_user.config, "General", "ConfigPanel");
	u32 sel = sOpt ? atoi(sOpt) : 0;
	if (sel>13) sel=13;
	m_Selector.SetCurSel(sel);
	m_general.ShowWindow(SW_SHOW);

	OnSelchangeSelect();

	return TRUE;
}


/////////////////////////////////////////////////////////////////////////////
// COptions message handlers

void COptions::HideAll()
{
	m_general.ShowWindow(SW_HIDE);
	m_systems.ShowWindow(SW_HIDE);
	m_render.ShowWindow(SW_HIDE);
	m_render2d.ShowWindow(SW_HIDE);
	m_render3d.ShowWindow(SW_HIDE);
	m_audio.ShowWindow(SW_HIDE);
	m_video.ShowWindow(SW_HIDE);
	m_http.ShowWindow(SW_HIDE);
	m_font.ShowWindow(SW_HIDE);
	m_stream.ShowWindow(SW_HIDE);
	m_decoder.ShowWindow(SW_HIDE);
	m_cache.ShowWindow(SW_HIDE);
	m_files.ShowWindow(SW_HIDE);
	m_files.ShowWindow(SW_HIDE);
	m_logs.ShowWindow(SW_HIDE);
}

void COptions::OnSelchangeSelect()
{
	HideAll();
	switch (m_Selector.GetCurSel()) {
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
		m_render2d.ShowWindow(SW_SHOW);
		break;
	case 5:
		m_render3d.ShowWindow(SW_SHOW);
		break;
	case 6:
		m_video.ShowWindow(SW_SHOW);
		break;
	case 7:
		m_audio.ShowWindow(SW_SHOW);
		break;
	case 8:
		m_font.ShowWindow(SW_SHOW);
		break;
	case 9:
		m_http.ShowWindow(SW_SHOW);
		break;
	case 10:
		m_stream.ShowWindow(SW_SHOW);
		break;
	case 11:
		m_cache.ShowWindow(SW_SHOW);
		break;
	case 12:
		m_files.ShowWindow(SW_SHOW);
		break;
	case 13:
		m_logs.ShowWindow(SW_SHOW);
		break;
	}
}

void COptions::OnSaveopt()
{
	m_general.SaveOptions();
	m_systems.SaveOptions();
	m_decoder.SaveOptions();
	m_render.SaveOptions();
	m_render2d.SaveOptions();
	m_render3d.SaveOptions();
	m_audio.SaveOptions();
	m_video.SaveOptions();
	m_http.SaveOptions();
	m_font.SaveOptions();
	m_stream.SaveOptions();
	m_cache.SaveOptions();
	m_logs.SaveOptions();

	Osmo4 *gpac = GetApp();
	gf_term_set_option(gpac->m_term, GF_OPT_RELOAD_CONFIG, 1);
	m_render2d.SetYUV();
}

void COptions::OnClose()
{
	char str[20];
	sprintf(str, "%d", m_Selector.GetCurSel());
	gf_cfg_set_key(GetApp()->m_user.config, "General", "ConfigPanel", str);

	DestroyWindow();
}

void COptions::OnDestroy()
{
	CDialog::OnDestroy();
	delete this;
	((CMainFrame *)GetApp()->m_pMainWnd)->m_pOpt = NULL;
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
	DDX_Control(pDX, IDC_LOOKFORSUB, m_LookForSubs);
	DDX_Control(pDX, IDC_DUMP_XMT, m_ViewXMT);
	DDX_Control(pDX, IDC_NO_CONSOLE, m_NoConsole);
	DDX_Control(pDX, IDC_LOOP, m_Loop);
	DDX_Control(pDX, IDC_SINGLE_INSTANCE, m_SingleInstance);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptGen, CDialog)
	//{{AFX_MSG_MAP(COptGen)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptGen message handlers



BOOL COptGen::OnInitDialog()
{
	CDialog::OnInitDialog();

	Osmo4 *gpac = GetApp();
	const char *sOpt;

	sOpt = gf_cfg_get_key(gpac->m_user.config, "General", "Loop");
	m_Loop.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(gpac->m_user.config, "General", "LookForSubtitles");
	m_LookForSubs.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(gpac->m_user.config, "General", "ConsoleOff");
	m_NoConsole.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(gpac->m_user.config, "General", "ViewXMT");
	m_ViewXMT.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(gpac->m_user.config, "General", "SingleInstance");
	m_SingleInstance.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	return TRUE;
}

void COptGen::SaveOptions()
{
	Osmo4 *gpac = GetApp();

	gpac->m_Loop = (Bool) m_Loop.GetCheck();
	gf_cfg_set_key(gpac->m_user.config, "General", "Loop", gpac->m_Loop ? "yes" : "no");
	gpac->m_LookForSubtitles = (Bool) m_LookForSubs.GetCheck();
	gf_cfg_set_key(gpac->m_user.config, "General", "LookForSubtitles",  gpac->m_LookForSubtitles ? "yes" : "no");
	gpac->m_NoConsole = (Bool) m_NoConsole.GetCheck();
	gf_cfg_set_key(gpac->m_user.config, "General", "ConsoleOff", gpac->m_NoConsole ? "yes" : "no");
	gpac->m_ViewXMTA = (Bool) m_ViewXMT.GetCheck();
	gf_cfg_set_key(gpac->m_user.config, "General", "ViewXMT", gpac->m_ViewXMTA ? "yes" : "no");
	gpac->m_SingleInstance = (Bool) m_SingleInstance.GetCheck();
	gf_cfg_set_key(gpac->m_user.config, "General", "SingleInstance", gpac->m_SingleInstance ? "yes" : "no");
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
	DDX_Control(pDX, IDC_BIFSDROP, m_LateFramesAlwaysDrawn);
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

	Osmo4 *gpac = GetApp();
	const char *sOpt;

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Systems", "Language3CC");
	if (!sOpt) sOpt = "eng";
	s32 select = 0;
	while (m_Lang.GetCount()) m_Lang.DeleteString(0);
	u32 i, count = gf_lang_get_count();
	for (i=0; i<count; i++) {
		const char *n2c = gf_lang_get_2cc(i);
		const char *n3c = gf_lang_get_3cc(i);

		m_Lang.AddString(gf_lang_get_name(i) );
		if (sOpt && n3c && !stricmp(sOpt, n3c))
			select = m_Lang.GetCount() - 1;
	}
	m_Lang.SetCurSel(select);


	/*system config*/
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Systems", "ThreadingPolicy");
	select = 0;
	while (m_Threading.GetCount()) m_Threading.DeleteString(0);
	m_Threading.AddString("Single Thread");
	m_Threading.AddString("Mutli Thread");
	if (sOpt && !stricmp(sOpt, "Multi")) select = 1;
	m_Threading.AddString("Free");
	if (sOpt && !stricmp(sOpt, "Free")) select = 2;
	m_Threading.SetCurSel(select);


	sOpt = gf_cfg_get_key(gpac->m_user.config, "Systems", "ForceSingleClock");
	if (sOpt && !stricmp(sOpt, "yes")) {
		m_ForceDuration.SetCheck(1);
	} else {
		m_ForceDuration.SetCheck(0);
	}
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Systems", "DrawLateFrames");
	if (sOpt && !stricmp(sOpt, "yes")) {
		m_LateFramesAlwaysDrawn.SetCheck(1);
	} else {
		m_LateFramesAlwaysDrawn.SetCheck(0);
	}


	return TRUE;
}


void COptSystems::SaveOptions()
{
	Osmo4 *gpac = GetApp();

	s32 sel = m_Lang.GetCurSel();
	u32 i=0;

	gf_cfg_set_key(gpac->m_user.config, "Systems", "LanguageName", gf_lang_get_name(i) );
	gf_cfg_set_key(gpac->m_user.config, "Systems", "Language3CC", gf_lang_get_3cc(i) );
	gf_cfg_set_key(gpac->m_user.config, "Systems", "Language2CC", gf_lang_get_2cc(i) );

	sel = m_Threading.GetCurSel();
	gf_cfg_set_key(gpac->m_user.config, "Systems", "ThreadingPolicy", (sel==0) ? "Single" : ( (sel==1) ? "Multi" : "Free"));
	gf_cfg_set_key(gpac->m_user.config, "Systems", "ForceSingleClock", m_ForceDuration.GetCheck() ? "yes" : "no");
	gf_cfg_set_key(gpac->m_user.config, "Systems", "DrawLateFrames", m_LateFramesAlwaysDrawn.GetCheck() ? "yes" : "no");
}


OptDecoder::OptDecoder(CWnd* pParent /*=NULL*/)
	: CDialog(OptDecoder::IDD, pParent)
{
	//{{AFX_DATA_INIT(OptDecoder)
	// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void OptDecoder::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(OptDecoder)
	DDX_Control(pDX, IDC_VIDEC_LIST, m_Video);
	DDX_Control(pDX, IDC_AUDEC_LIST, m_Audio);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(OptDecoder, CDialog)
	//{{AFX_MSG_MAP(OptDecoder)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// OptDecoder message handlers



BOOL OptDecoder::OnInitDialog()
{
	u32 i;
	const char *sOpt;
	CDialog::OnInitDialog();

	Osmo4 *gpac = GetApp();

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
			if (sOpt && !stricmp(ifce->module_name, sOpt)) select = to_sel;
			m_Audio.AddString(ifce->module_name);
			to_sel++;
		}
		gf_modules_close_interface((GF_BaseInterface *)ifce);
	}
	m_Audio.SetCurSel(select);

	/*video dec enum*/
	while (m_Video.GetCount()) m_Video.DeleteString(0);
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Systems", "DefVideoDec");
	count = gf_modules_get_count(gpac->m_user.modules);
	select = 0;
	to_sel = 0;
	for (i=0; i<count; i++) {
		ifce = (GF_BaseDecoder *) gf_modules_load_interface(gpac->m_user.modules, i, GF_MEDIA_DECODER_INTERFACE);
		if (!ifce) continue;

		if (ifce->CanHandleStream(ifce, GF_STREAM_VISUAL, NULL, 0)) {
			if (sOpt && !stricmp(ifce->module_name, sOpt)) select = to_sel;
			m_Video.AddString(ifce->module_name);
			to_sel++;
		}
		gf_modules_close_interface((GF_BaseInterface *)ifce);
	}
	m_Video.SetCurSel(select);
	return TRUE;
}

void OptDecoder::SaveOptions()
{
	Osmo4 *gpac = GetApp();
	char str[100];
	m_Audio.GetWindowText(str, 100);
	gf_cfg_set_key(gpac->m_user.config, "Systems", "DefAudioDec", str);
	m_Video.GetWindowText(str, 100);
	gf_cfg_set_key(gpac->m_user.config, "Systems", "DefVideoDec", str);

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
	DDX_Control(pDX, IDC_DRAW_BOUNDS, m_DrawBounds);
	DDX_Control(pDX, IDC_GD_LIST, m_Graphics);
	DDX_Control(pDX, IDC_USE_RENDER3D, m_Use3DRender);
	DDX_Control(pDX, IDC_AA_LIST, m_AntiAlias);
	DDX_Control(pDX, IDC_FORCE_SIZE, m_ForceSize);
	DDX_Control(pDX, IDC_FAST_RENDER, m_HighSpeed);
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
	s32 i;
	CDialog::OnInitDialog();

	Osmo4 *gpac = GetApp();
	const char *sOpt;

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Compositor", "OpenGLMode");
	m_Use3DRender.SetCheck( (sOpt && !strcmp(sOpt, "always")) ? 1 : 0);

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Compositor", "ForceSceneSize");
	m_ForceSize.SetCheck( (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Compositor", "FrameRate");
	if (!sOpt) sOpt = "30.0";
	s32 select = 0;
	while (m_BIFSRate.GetCount()) m_BIFSRate.DeleteString(0);
	for (i = 0; i<NUM_RATES; i++) {
		m_BIFSRate.AddString(BIFSRates[i]);
		if (sOpt && !stricmp(sOpt, BIFSRates[i]) ) select = i;
	}
	m_BIFSRate.SetCurSel(select);

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Compositor", "HighSpeed");
	m_HighSpeed.SetCheck( (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Compositor", "AntiAlias");
	while (m_AntiAlias.GetCount()) m_AntiAlias.DeleteString(0);

	m_AntiAlias.AddString("None");
	m_AntiAlias.AddString("Text only");
	m_AntiAlias.AddString("Complete");
	select = 2;
	if (sOpt && !stricmp(sOpt, "Text")) select = 1;
	else if (sOpt && !stricmp(sOpt, "None")) select = 0;
	m_AntiAlias.SetCurSel(select);

	/*graphics driver enum*/
	while (m_Graphics.GetCount()) m_Graphics.DeleteString(0);
	sOpt = gf_cfg_get_key(gpac->m_user.config, "core", "raster2d");
	s32 count = gf_modules_get_count(gpac->m_user.modules);
	GF_BaseInterface *ifce;
	select = 0;
	u32 to_sel = 0;
	for (i=0; i<count; i++) {
		ifce = gf_modules_load_interface(gpac->m_user.modules, i, GF_RASTER_2D_INTERFACE);
		if (!ifce) continue;
		if (sOpt && !stricmp(ifce->module_name, sOpt)) select = to_sel;
		m_Graphics.AddString(ifce->module_name);
		gf_modules_close_interface(ifce);
		to_sel++;
	}
	m_Graphics.SetCurSel(select);


	m_DrawBounds.AddString("None");
	m_DrawBounds.AddString("Box/Rect");
	m_DrawBounds.AddString("AABB Tree");
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Compositor", "BoundingVolume");
	if (sOpt && !stricmp(sOpt, "Box")) m_DrawBounds.SetCurSel(1);
	else if (sOpt && !stricmp(sOpt, "AABB")) m_DrawBounds.SetCurSel(2);
	else m_DrawBounds.SetCurSel(0);

	return TRUE;
}


Bool COptRender::SaveOptions()
{
	char str[50];
	Osmo4 *gpac = GetApp();

	gf_cfg_set_key(gpac->m_user.config, "Compositor", "HighSpeed", m_HighSpeed.GetCheck() ? "yes" : "no");
	gf_cfg_set_key(gpac->m_user.config, "Compositor", "ForceSceneSize", m_ForceSize.GetCheck() ? "yes" : "no");

	s32 sel = m_BIFSRate.GetCurSel();
	gf_cfg_set_key(gpac->m_user.config, "Compositor", "FrameRate", BIFSRates[sel]);

	sel = m_AntiAlias.GetCurSel();
	gf_cfg_set_key(gpac->m_user.config, "Compositor", "AntiAlias", (sel==0) ? "None" : ( (sel==1) ? "Text" : "All"));

	sel = m_DrawBounds.GetCurSel();
	gf_cfg_set_key(gpac->m_user.config, "Compositor", "BoundingVolume", (sel==2) ? "AABB" : (sel==1) ? "Box" : "None");

	m_Graphics.GetWindowText(str, 50);
	gf_cfg_set_key(gpac->m_user.config, "core", "raster2d", str);

	gf_cfg_set_key(gpac->m_user.config, "Compositor", "OpenGLMode", m_Use3DRender.GetCheck() ? "always" : "disable");
	return GF_FALSE;
}


COptRender2D::COptRender2D(CWnd* pParent /*=NULL*/)
	: CDialog(COptRender2D::IDD, pParent)
{
	//{{AFX_DATA_INIT(COptRender2D)
	// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void COptRender2D::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptRender2D)
	DDX_Control(pDX, IDC_FORMAT_YUV, m_YUVFormat);
	DDX_Control(pDX, IDC_YUV, m_NoYUV);
	DDX_Control(pDX, IDC_ZOOM_SCALABLE, m_Scalable);
	DDX_Control(pDX, IDC_DIRECTRENDER, m_DirectRender);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptRender2D, CDialog)
	//{{AFX_MSG_MAP(COptRender2D)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptRender2D message handlers

BOOL COptRender2D::OnInitDialog()
{
	CDialog::OnInitDialog();

	Osmo4 *gpac = GetApp();
	const char *sOpt;

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Compositor", "DrawMode");
	if (sOpt && !stricmp(sOpt, "immediate")) {
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

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Compositor", "DisableYUV");
	if (sOpt && !stricmp(sOpt, "yes")) {
		m_NoYUV.SetCheck(1);
	} else {
		m_NoYUV.SetCheck(0);
	}

	SetYUV();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void COptRender2D::SetYUV()
{
	Osmo4 *gpac = GetApp();
	u32 yuv_format = gf_term_get_option(gpac->m_term, GF_OPT_YUV_FORMAT);
	if (!yuv_format) {
		m_YUVFormat.SetWindowText("(No YUV used)");
	} else {
		char str[100];
		sprintf(str, "(%s used)", gf_4cc_to_str(yuv_format));
		m_YUVFormat.SetWindowText(str);
	}
}

void COptRender2D::SaveOptions()
{
	Osmo4 *gpac = GetApp();
	gf_cfg_set_key(gpac->m_user.config, "Compositor", "DrawMode", m_DirectRender.GetCheck() ? "immediate" : "defer");
	gf_cfg_set_key(gpac->m_user.config, "Compositor", "ScalableZoom", m_Scalable.GetCheck() ? "yes" : "no");

	gf_cfg_set_key(gpac->m_user.config, "Compositor", "DisableYUV", m_NoYUV.GetCheck() ? "yes" : "no");
}


COptRender3D::COptRender3D(CWnd* pParent /*=NULL*/)
	: CDialog(COptRender3D::IDD, pParent)
{
	//{{AFX_DATA_INIT(COptRender3D)
	//}}AFX_DATA_INIT
}


void COptRender3D::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptRender3D)
	DDX_Control(pDX, IDC_BITMAP_USE_PIXEL, m_BitmapPixels);
	DDX_Control(pDX, IDC_DISABLE_TX_RECT, m_DisableTXRect);
	DDX_Control(pDX, IDC_RASTER_OUTLINE, m_RasterOutlines);
	DDX_Control(pDX, IDC_EMUL_POW2, m_EmulPow2);
	DDX_Control(pDX, IDC_DISABLE_POLY_AA, m_PolyAA);
	DDX_Control(pDX, IDC_DRAW_NORMALS, m_DrawNormals);
	DDX_Control(pDX, IDC_BACK_CULL, m_BackCull);
	DDX_Control(pDX, IDC_DRAW_MODE, m_Wireframe);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptRender3D, CDialog)
	//{{AFX_MSG_MAP(COptRender3D)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptRender3D message handlers


BOOL COptRender3D::OnInitDialog()
{
	CDialog::OnInitDialog();


	Osmo4 *gpac = GetApp();
	const char *sOpt;

	m_DrawNormals.AddString("Never");
	m_DrawNormals.AddString("Per Face");
	m_DrawNormals.AddString("Per Vertex");
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Compositor", "DrawNormals");
	if (sOpt && !stricmp(sOpt, "PerFace")) m_DrawNormals.SetCurSel(1);
	else if (sOpt && !stricmp(sOpt, "PerVertex")) m_DrawNormals.SetCurSel(2);
	else m_DrawNormals.SetCurSel(0);

	m_BackCull.AddString("Off");
	m_BackCull.AddString("On");
	m_BackCull.AddString("Alpha");
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Compositor", "BackFaceCulling");
	if (sOpt && !stricmp(sOpt, "Off")) m_BackCull.SetCurSel(0);
	else if (sOpt && !stricmp(sOpt, "Alpha")) m_BackCull.SetCurSel(2);
	else m_BackCull.SetCurSel(1);

	m_Wireframe.AddString("Solid");
	m_Wireframe.AddString("Wireframe");
	m_Wireframe.AddString("Both");
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Compositor", "Wireframe");
	if (sOpt && !stricmp(sOpt, "WireOnly")) m_Wireframe.SetCurSel(1);
	else if (sOpt && !stricmp(sOpt, "WireOnSolid")) m_Wireframe.SetCurSel(2);
	else m_Wireframe.SetCurSel(0);


	sOpt = gf_cfg_get_key(gpac->m_user.config, "Compositor", "RasterOutlines");
	m_RasterOutlines.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Compositor", "EmulatePOW2");
	m_EmulPow2.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Compositor", "PolygonAA");
	m_PolyAA.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Compositor", "BitmapCopyPixels");
	m_BitmapPixels.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Compositor", "DisableRectExt");
	m_DisableTXRect.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void COptRender3D::SaveOptions()
{
	Osmo4 *gpac = GetApp();

	u32 sel = m_DrawNormals.GetCurSel();
	gf_cfg_set_key(gpac->m_user.config, "Compositor", "DrawNormals", (sel==2) ? "PerVertex" : (sel==1) ? "PerFace" : "Never");
	sel = m_BackCull.GetCurSel();
	gf_cfg_set_key(gpac->m_user.config, "Compositor", "BackFaceCulling", (sel==2) ? "Alpha" : (sel==1) ? "On" : "Off");
	sel = m_Wireframe.GetCurSel();
	gf_cfg_set_key(gpac->m_user.config, "Compositor", "Wireframe", (sel==2) ? "WireOnSolid" : (sel==1) ? "WireOnly" : "WireNone");

	gf_cfg_set_key(gpac->m_user.config, "Compositor", "RasterOutlines", m_RasterOutlines.GetCheck() ? "yes" : "no");
	gf_cfg_set_key(gpac->m_user.config, "Compositor", "EmulatePOW2", m_EmulPow2.GetCheck() ? "yes" : "no");
	gf_cfg_set_key(gpac->m_user.config, "Compositor", "PolygonAA", m_PolyAA.GetCheck() ? "yes" : "no");

	gf_cfg_set_key(gpac->m_user.config, "Compositor", "DisableRectExt", m_DisableTXRect.GetCheck() ? "yes" : "no");
	gf_cfg_set_key(gpac->m_user.config, "Compositor", "BitmapCopyPixels", m_BitmapPixels.GetCheck() ? "yes" : "no");
}

COptVideo::COptVideo(CWnd* pParent /*=NULL*/)
	: CDialog(COptVideo::IDD, pParent)
{
	//{{AFX_DATA_INIT(COptVideo)
	//}}AFX_DATA_INIT
}


void COptVideo::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptVideo)
	DDX_Control(pDX, IDC_SWITCH_RES, m_SwitchRes);
	DDX_Control(pDX, IDC_VIDEO_LIST, m_Videos);
	DDX_Control(pDX, IDC_HWMEMORY, m_UseHWMemory);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptVideo, CDialog)
	//{{AFX_MSG_MAP(COptVideo)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptVideo message handlers

BOOL COptVideo::OnInitDialog()
{
	CDialog::OnInitDialog();

	Osmo4 *gpac = GetApp();
	const char *sOpt;

	m_SwitchRes.SetCheck(gf_cfg_get_bool(gpac->m_user.config, "core", "switch-vres") );
	sOpt = gf_cfg_get_bool(gpac->m_user.config, "core", "hwvmem");
	m_UseHWMemory.SetCheck(sOpt && !stricmp(sOpt, "Always") ? 1 : 0);


	u32 count = gf_modules_get_count(gpac->m_user.modules);
	GF_BaseInterface *ifce;
	s32 to_sel = 0;
	s32 select = 0;
	/*video drivers enum*/
	while (m_Videos.GetCount()) m_Videos.DeleteString(0);
	sOpt = gf_cfg_get_key(gpac->m_user.config, "core", "video-output");

	for (u32 i=0; i<count; i++) {
		ifce = gf_modules_load_interface(gpac->m_user.modules, i, GF_VIDEO_OUTPUT_INTERFACE);
		if (!ifce) continue;
		if (sOpt && !stricmp(ifce->module_name, sOpt)) select = to_sel;
		m_Videos.AddString(ifce->module_name);
		gf_modules_close_interface(ifce);
		to_sel++;
	}
	m_Videos.SetCurSel(select);

	return TRUE;

}

void COptVideo::SaveOptions()
{
	Osmo4 *gpac = GetApp();
	char str[50];

	gf_cfg_set_key(gpac->m_user.config, "core", "switch-vres", m_SwitchRes.GetCheck() ? "yes" : "no");
	gf_cfg_set_key(gpac->m_user.config, "core", "hwvmem", m_UseHWMemory.GetCheck() ? "Always" : "Auto");
	m_Videos.GetWindowText(str, 50);
	gf_cfg_set_key(gpac->m_user.config, "core", "video-output", str);
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
	DDX_Control(pDX, IDC_AUDIO_NOTIFS, m_Notifs);
	DDX_Control(pDX, IDC_DRIVER_LIST, m_DriverList);
	DDX_Control(pDX, IDC_AUDIO_RESYNC, m_AudioResync);
	DDX_Control(pDX, IDC_AUDIO_MULTICH, m_AudioMultiCH);
	DDX_Control(pDX, IDC_AUDIO_FPS, m_AudioDur);
	DDX_Control(pDX, IDC_SPIN_FPS, m_SpinFPS);
	DDX_Control(pDX, IDC_FORCE_AUDIO, m_ForceConfig);
	DDX_Control(pDX, IDC_SPIN_AUDIO, m_AudioSpin);
	DDX_Control(pDX, IDC_EDIT_AUDIO, m_AudioEdit);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptAudio, CDialog)
	//{{AFX_MSG_MAP(COptAudio)
	ON_BN_CLICKED(IDC_FORCE_AUDIO, OnForceAudio)
	ON_CBN_SELCHANGE(IDC_DRIVER_LIST, OnSelchangeDriverList)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptAudio message handlers

BOOL COptAudio::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_AudioSpin.SetBuddy(& m_AudioEdit);
	m_SpinFPS.SetBuddy(& m_AudioDur);
	m_SpinFPS.SetRange(0, 2000);

	Osmo4 *gpac = GetApp();
	const char *sOpt;

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Audio", "ForceConfig");
	if (sOpt && !stricmp(sOpt, "yes")) {
		m_ForceConfig.SetCheck(1);
	} else {
		m_ForceConfig.SetCheck(0);
	}
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Audio", "NumBuffers");
	if (sOpt) {
		m_AudioEdit.SetWindowText(sOpt);
	} else {
		m_AudioEdit.SetWindowText("2");
	}
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Audio", "TotalDuration");
	if (sOpt) {
		m_AudioDur.SetWindowText(sOpt);
	} else {
		m_AudioDur.SetWindowText("120");
	}

	OnForceAudio();

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Audio", "NoResync");
	m_AudioResync.SetCheck( (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Audio", "DisableMultiChannel");
	m_AudioMultiCH.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);

	/*driver enum*/
	while (m_DriverList.GetCount()) m_DriverList.DeleteString(0);
	sOpt = gf_cfg_get_key(gpac->m_user.config, "core", "audio-output");
	u32 count = gf_modules_get_count(gpac->m_user.modules);
	GF_BaseInterface *ifce;
	s32 select = 0;
	s32 to_sel = 0;
	for (u32 i=0; i<count; i++) {
		ifce = gf_modules_load_interface(gpac->m_user.modules, i, GF_AUDIO_OUTPUT_INTERFACE);
		if (!ifce) continue;
		if (sOpt && !stricmp(ifce->module_name, sOpt)) select = to_sel;
		m_DriverList.AddString(ifce->module_name);
		gf_modules_close_interface(ifce);
		to_sel++;
	}
	m_DriverList.SetCurSel(select);

	m_Notifs.ShowWindow(SW_HIDE);
	if (sOpt && strstr(sOpt, "DirectSound")) m_Notifs.ShowWindow(SW_SHOW);

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Audio", "DisableNotification");
	if (sOpt && !stricmp(sOpt, "yes"))
		m_Notifs.SetCheck(1);
	else
		m_Notifs.SetCheck(0);

	return TRUE;
}


void COptAudio::SaveOptions()
{
	Osmo4 *gpac = GetApp();
	char str[50];

	gf_cfg_set_key(gpac->m_user.config, "Audio", "ForceConfig", m_ForceConfig.GetCheck() ? "yes" : "no");
	gf_cfg_set_key(gpac->m_user.config, "Audio", "NoResync", m_AudioResync.GetCheck() ? "yes" : "no");
	gf_cfg_set_key(gpac->m_user.config, "Audio", "DisableMultiChannel", m_AudioMultiCH.GetCheck() ? "yes" : "no");

	m_AudioEdit.GetWindowText(str, 20);
	gf_cfg_set_key(gpac->m_user.config, "Audio", "NumBuffers", str);
	m_AudioDur.GetWindowText(str, 20);
	gf_cfg_set_key(gpac->m_user.config, "Audio", "TotalDuration", str);

	m_DriverList.GetWindowText(str, 50);
	gf_cfg_set_key(gpac->m_user.config, "core", "audio-output", str);

	if (strstr(str, "DirectSound")) {
		gf_cfg_set_key(gpac->m_user.config, "Audio", "DisableNotification", m_Notifs.GetCheck() ? "yes" : "no");
	}

}

void COptAudio::OnForceAudio()
{
	BOOL en = m_ForceConfig.GetCheck();

	m_AudioSpin.EnableWindow(en);
	m_AudioEdit.EnableWindow(en);
	m_SpinFPS.EnableWindow(en);
	m_AudioDur.EnableWindow(en);
}

void COptAudio::OnSelchangeDriverList()
{
	char str[50];
	m_DriverList.GetWindowText(str, 50);
	if (strstr(str, "DirectSound")) {
		m_Notifs.ShowWindow(SW_SHOW);
	} else {
		m_Notifs.ShowWindow(SW_HIDE);
	}
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
	DDX_Control(pDX, IDC_TEXTURE_MODE, m_TextureModes);
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

	Osmo4 *gpac = GetApp();
	const char *sOpt;

	/*video drivers enum*/
	while (m_Fonts.GetCount()) m_Fonts.DeleteString(0);
	sOpt = gf_cfg_get_key(gpac->m_user.config, "FontCache", "FontReader");
	s32 to_sel = 0;
	s32 select = 0;
	u32 count = gf_modules_get_count(gpac->m_user.modules);
	for (i=0; i<count; i++) {
		ifce = gf_modules_load_interface(gpac->m_user.modules, i, GF_FONT_READER_INTERFACE);
		if (!ifce) continue;
		if (sOpt && !stricmp(ifce->module_name, sOpt)) select = to_sel;
		m_Fonts.AddString(ifce->module_name);
		gf_modules_close_interface(ifce);
		to_sel++;
	}
	m_Fonts.SetCurSel(select);


	sOpt = gf_cfg_get_key(gpac->m_user.config, "FontCache", "FontDirectory");
	if (sOpt) m_BrowseFont.SetWindowText(sOpt);

	/*text texturing modes*/
	while (m_TextureModes.GetCount()) m_TextureModes.DeleteString(0);
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Compositor", "TextureTextMode");
	m_TextureModes.AddString("Default");
	m_TextureModes.AddString("Never");
	m_TextureModes.AddString("Always");
	if (sOpt && !stricmp(sOpt, "3D")) m_TextureModes.SetCurSel(1);
	else if (sOpt && !stricmp(sOpt, "Always")) m_TextureModes.SetCurSel(2);
	else m_TextureModes.SetCurSel(0);

	return TRUE;
}



static char szCacheDir[MAX_PATH];

static int CALLBACK LocCbck(HWND hwnd, UINT uMsg, LPARAM lp, LPARAM pData)
{
	char dir[MAX_PATH];
	if (uMsg == BFFM_INITIALIZED) {
		strcpy(dir, szCacheDir);
		SendMessage(hwnd, BFFM_SETSELECTION, TRUE,(LPARAM) dir);
	}
	return 0;
}

void COptFont::OnBrowseFont()
{
	BROWSEINFO brw;
	LPMALLOC pMalloc;
	LPITEMIDLIST ret;
	char dir[MAX_PATH];

	if (NOERROR == ::SHGetMalloc(&pMalloc) ) {

		m_BrowseFont.GetWindowText(szCacheDir, MAX_PATH);

		memset(&brw, 0, sizeof(BROWSEINFO));
		brw.hwndOwner = this->GetSafeHwnd();
		brw.pszDisplayName = dir;
		brw.lpszTitle = "Select Font Directory...";
		brw.ulFlags = 0L;
		brw.lpfn = LocCbck;

		ret = SHBrowseForFolder(&brw);
		if (ret != NULL) {
			if (::SHGetPathFromIDList(ret, dir)) {
				m_BrowseFont.SetWindowText(dir);
			}
			pMalloc->Free(ret);
		}
		pMalloc->Release();
	}
}


void COptFont::SaveOptions()
{
	Osmo4 *gpac = GetApp();
	char str[MAX_PATH];

	m_Fonts.GetWindowText(str, 50);
	gf_cfg_set_key(gpac->m_user.config, "FontCache", "FontReader", str);
	m_BrowseFont.GetWindowText(str, 50);
	gf_cfg_set_key(gpac->m_user.config, "FontCache", "FontDirectory", str);
	switch (m_TextureModes.GetCurSel()) {
	case 2:
		gf_cfg_set_key(gpac->m_user.config, "Compositor", "TextureTextMode", "Always");
		break;
	case 1:
		gf_cfg_set_key(gpac->m_user.config, "Compositor", "TextureTextMode", "Never");
		break;
	default:
		gf_cfg_set_key(gpac->m_user.config, "Compositor", "TextureTextMode", "Default");
		break;
	}
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
	DDX_Control(pDX, IDC_HTTP_PROXY, m_ProxyName);
	DDX_Control(pDX, IDC_HTTP_USE_PROXY, m_useProxy);
	DDX_Control(pDX, IDC_SAX_DELAY, m_SAXDuration);
	DDX_Control(pDX, IDC_SAX_PROGRESSIVE, m_Progressive);
	DDX_Control(pDX, IDC_RESTART_CACHE, m_DisableCache);
	DDX_Control(pDX, IDC_CLEAN_CACHE, m_CleanCache);
	DDX_Control(pDX, IDC_BROWSE_CACHE, m_CacheDir);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptHTTP, CDialog)
	//{{AFX_MSG_MAP(COptHTTP)
	ON_BN_CLICKED(IDC_BROWSE_CACHE, OnBrowseCache)
	ON_BN_CLICKED(IDC_SAX_PROGRESSIVE, OnSaxProgressive)
	ON_BN_CLICKED(IDC_HTTP_USE_PROXY, OnUseProxy)
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_RESTART_CACHE, &COptHTTP::OnBnClickedRestartCache)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptHTTP message handlers


void COptHTTP::OnBrowseCache()
{
	BROWSEINFO brw;
	LPMALLOC pMalloc;
	LPITEMIDLIST ret;
	char dir[MAX_PATH];

	if (NOERROR == ::SHGetMalloc(&pMalloc) ) {

		m_CacheDir.GetWindowText(szCacheDir, MAX_PATH);

		memset(&brw, 0, sizeof(BROWSEINFO));
		brw.hwndOwner = this->GetSafeHwnd();
		brw.pszDisplayName = dir;
		brw.lpszTitle = "Select HTTP Cache Directory...";
		brw.ulFlags = 0L;
		brw.lpfn = LocCbck;

		ret = SHBrowseForFolder(&brw);
		if (ret != NULL) {
			if (::SHGetPathFromIDList(ret, dir)) {
				m_CacheDir.SetWindowText(dir);
			}
			pMalloc->Free(ret);
		}
		pMalloc->Release();
	}
}

BOOL COptHTTP::OnInitDialog()
{
	char proxy[GF_MAX_PATH];
	CDialog::OnInitDialog();

	Osmo4 *gpac = GetApp();
	const char *sOpt;

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Core", "CacheDirectory");
	if (sOpt) m_CacheDir.SetWindowText(sOpt);

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Core", "CleanCache");
	m_CleanCache.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Core", "DisableCache");
	m_DisableCache.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);

	sOpt = gf_cfg_get_key(gpac->m_user.config, "SAXLoader", "Progressive");
	m_Progressive.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	OnSaxProgressive();

	sOpt = gf_cfg_get_key(gpac->m_user.config, "SAXLoader", "MaxDuration");
	if (sOpt) {
		m_SAXDuration.SetWindowText(sOpt);
	} else {
		m_SAXDuration.SetWindowText("0");
	}
	//if (m_Progressive.GetCheck()) m_SAXDuration.EnableWindow(1);

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Core", "HTTPProxyEnabled");
	m_useProxy.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	OnUseProxy();
	strcpy(proxy, "");
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Core", "HTTPProxyName");
	if (sOpt) {
		strcpy(proxy, sOpt);
		sOpt = gf_cfg_get_key(gpac->m_user.config, "Core", "HTTPProxyPort");
		if (sOpt) {
			strcat(proxy, ":");
			strcat(proxy, sOpt);
		}
	}
	m_ProxyName.SetWindowText(proxy);
	return TRUE;
}

void COptHTTP::OnSaxProgressive()
{
	if (m_Progressive.GetCheck()) {
		m_SAXDuration.EnableWindow(1);
	} else {
		m_SAXDuration.EnableWindow(0);
	}
}


void COptHTTP::OnUseProxy()
{
	if (m_useProxy.GetCheck()) {
		m_ProxyName.EnableWindow(1);
	} else {
		m_ProxyName.EnableWindow(0);
	}
}

void COptHTTP::SaveOptions()
{
	Osmo4 *gpac = GetApp();

	gf_cfg_set_key(gpac->m_user.config, "Core", "CleanCache", m_CleanCache.GetCheck() ? "yes" : "no");
	gf_cfg_set_key(gpac->m_user.config, "Core", "DisableCache", m_DisableCache.GetCheck() ? "yes" : "no");
	gf_cfg_set_key(gpac->m_user.config, "SAXLoader", "Progressive", m_Progressive.GetCheck() ? "yes" : "no");

	m_SAXDuration.GetWindowText(szCacheDir, MAX_PATH);
	gf_cfg_set_key(gpac->m_user.config, "SAXLoader", "MaxDuration", szCacheDir);

	gf_cfg_set_key(gpac->m_user.config, "Core", "HTTPProxyEnabled", m_useProxy.GetCheck() ? "yes" : "no");
	m_ProxyName.GetWindowText(szCacheDir, MAX_PATH);
	char *sep = strrchr(szCacheDir, ':');
	if (sep) {
		sep[0] = 0;
		gf_cfg_set_key(gpac->m_user.config, "Core", "HTTPProxyName", szCacheDir);
		sep[0] = ':';
		gf_cfg_set_key(gpac->m_user.config, "Core", "HTTPProxyPort", sep+1);
	} else {
		gf_cfg_set_key(gpac->m_user.config, "Core", "HTTPProxyName", szCacheDir);
		gf_cfg_set_key(gpac->m_user.config, "Core", "HTTPProxyPort", NULL);
	}
	m_CacheDir.GetWindowText(szCacheDir, MAX_PATH);
	gf_cfg_set_key(gpac->m_user.config, "Core", "CacheDirectory", szCacheDir);
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

	Osmo4 *gpac = GetApp();
	const char *sOpt;

	while (m_Port.GetCount()) m_Port.DeleteString(0);
	m_Port.AddString("554 (RTSP standard)");
	m_Port.AddString("7070 (RTSP ext)");
	m_Port.AddString("80 (RTSP / HTTP tunnel)");
	m_Port.AddString("8080 (RTSP / HTTP tunnel)");

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Streaming", "DefaultPort");
	u32 port = 554;
	Bool force_rtsp = GF_FALSE;
	if (sOpt) port = atoi(sOpt);
	switch (port) {
	case 8080:
		m_Port.SetCurSel(3);
		force_rtsp = GF_TRUE;
		break;
	case 80:
		m_Port.SetCurSel(2);
		force_rtsp = GF_TRUE;
		break;
	case 7070:
		m_Port.SetCurSel(1);
		break;
	default:
		m_Port.SetCurSel(0);
		break;
	}

	Bool use_rtsp = GF_FALSE;
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Streaming", "RTPoverRTSP");
	if (sOpt && !stricmp(sOpt, "yes")) use_rtsp = GF_TRUE;

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
		m_Timeout.SetWindowText(sOpt);
	} else {
		m_Timeout.SetWindowText("30000");
	}

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Network", "BufferLength");
	if (sOpt) {
		m_Buffer.SetWindowText(sOpt);
	} else {
		m_Buffer.SetWindowText("3000");
	}

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Network", "RebufferLength");
	u32 buf_len = 0;
	if (sOpt) buf_len = atoi(sOpt);
	if (buf_len) {
		m_RebufferLen.SetWindowText(sOpt);
		m_Rebuffer.SetCheck(1);
		m_RebufferLen.EnableWindow(1);
	} else {
		m_RebufferLen.SetWindowText("0");
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
	char str[50];
	s32 buf, rebuf;
	m_Buffer.GetWindowText(str, 50);
	buf = atoi(str);
	m_RebufferLen.GetWindowText(str, 50);
	rebuf = atoi(str);
	if (rebuf*2 > buf) {
		rebuf = buf/2;
		sprintf(str, "%d", rebuf);
		m_RebufferLen.SetWindowText(str);
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
	Osmo4 *gpac = GetApp();
	Bool force_rtsp = GF_FALSE;
	s32 sel = m_Port.GetCurSel();
	switch (sel) {
	case 3:
		gf_cfg_set_key(gpac->m_user.config, "Streaming", "DefaultPort", "8080");
		force_rtsp = GF_TRUE;
		break;
	case 2:
		gf_cfg_set_key(gpac->m_user.config, "Streaming", "DefaultPort", "80");
		force_rtsp = GF_TRUE;
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

	char str[50];

	m_Timeout.GetWindowText(str, 50);
	gf_cfg_set_key(gpac->m_user.config, "Streaming", "RTSPTimeout", str);

	m_Buffer.GetWindowText(str, 50);
	gf_cfg_set_key(gpac->m_user.config, "Network", "BufferLength", str);
	if (m_Rebuffer.GetCheck()) {
		m_RebufferLen.GetWindowText(str, 50);
		gf_cfg_set_key(gpac->m_user.config, "Network", "RebufferLength", str);
	} else {
		gf_cfg_set_key(gpac->m_user.config, "Network", "RebufferLength", "0");
	}
}




COptMCache::COptMCache(CWnd* pParent /*=NULL*/)
	: CDialog(COptMCache::IDD, pParent)
{
	//{{AFX_DATA_INIT(COptMCache)
	// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void COptMCache::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptMCache)
	DDX_Control(pDX, IDC_BASEPRES, m_BaseName);
	DDX_Control(pDX, IDC_MCACHE_USENAME, m_UseBase);
	DDX_Control(pDX, IDC_MCACHE_OVERWRITE, m_Overwrite);
	DDX_Control(pDX, IDC_BROWSE_MCACHE, m_RecDir);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptMCache, CDialog)
	//{{AFX_MSG_MAP(COptMCache)
	ON_BN_CLICKED(IDC_BROWSE_MCACHE, OnBrowseMcache)
	ON_BN_CLICKED(IDC_MCACHE_USENAME, OnMcacheUsename)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptMCache message handlers

void COptMCache::OnBrowseMcache()
{
	BROWSEINFO brw;
	LPMALLOC pMalloc;
	LPITEMIDLIST ret;
	char dir[MAX_PATH];

	if (NOERROR == ::SHGetMalloc(&pMalloc) ) {

		m_RecDir.GetWindowText(szCacheDir, MAX_PATH);

		memset(&brw, 0, sizeof(BROWSEINFO));
		brw.hwndOwner = this->GetSafeHwnd();
		brw.pszDisplayName = dir;
		brw.lpszTitle = "Select HTTP Cache Directory...";
		brw.ulFlags = 0L;
		brw.lpfn = LocCbck;

		ret = SHBrowseForFolder(&brw);
		if (ret != NULL) {
			if (::SHGetPathFromIDList(ret, dir)) {
				m_RecDir.SetWindowText(dir);
			}
			pMalloc->Free(ret);
		}
		pMalloc->Release();
	}
}

BOOL COptMCache::OnInitDialog()
{
	CDialog::OnInitDialog();

	Osmo4 *gpac = GetApp();
	const char *sOpt;

	sOpt = gf_cfg_get_key(gpac->m_user.config, "StreamingCache", "RecordDirectory");
	if (!sOpt) sOpt = gf_cfg_get_key(gpac->m_user.config, "Core", "CacheDirectory");
	if (sOpt) m_RecDir.SetWindowText(sOpt);

	sOpt = gf_cfg_get_key(gpac->m_user.config, "StreamingCache", "KeepExistingFiles");
	m_Overwrite.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 0 : 1);

	sOpt = gf_cfg_get_key(gpac->m_user.config, "StreamingCache", "BaseFileName");
	if (sOpt) {
		m_UseBase.SetCheck(1);
		m_BaseName.EnableWindow(TRUE);
		m_BaseName.SetWindowText(sOpt);
	} else {
		m_UseBase.SetCheck(0);
		m_BaseName.EnableWindow(FALSE);
		m_BaseName.SetWindowText("uses service URL");
	}
	return TRUE;
}

void COptMCache::OnMcacheUsename()
{
	if (m_UseBase.GetCheck()) {
		m_BaseName.EnableWindow(TRUE);
		m_BaseName.SetWindowText("record");
	} else {
		m_BaseName.EnableWindow(FALSE);
		m_BaseName.SetWindowText("uses service URL");
	}
}

void COptMCache::SaveOptions()
{
	Osmo4 *gpac = GetApp();

	gf_cfg_set_key(gpac->m_user.config, "StreamingCache", "KeepExistingFiles", m_Overwrite.GetCheck() ? "no" : "yes");
	if (m_UseBase.GetCheck()) {
		m_BaseName.GetWindowText(szCacheDir, MAX_PATH);
		gf_cfg_set_key(gpac->m_user.config, "StreamingCache", "BaseFileName", szCacheDir);
	} else {
		gf_cfg_set_key(gpac->m_user.config, "StreamingCache", "BaseFileName", NULL);
	}
	m_RecDir.GetWindowText(szCacheDir, MAX_PATH);
	gf_cfg_set_key(gpac->m_user.config, "StreamingCache", "RecordDirectory", szCacheDir);
}


OptFiles::OptFiles(CWnd* pParent /*=NULL*/)
	: CDialog(OptFiles::IDD, pParent)
{
	//{{AFX_DATA_INIT(OptFiles)
	// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void OptFiles::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(OptFiles)
	DDX_Control(pDX, IDC_ASSOCIATE, m_DoAssociate);
	DDX_Control(pDX, IDC_FILES_PLUG, m_PlugName);
	DDX_Control(pDX, IDC_FILES_MIMES, m_mimes);
	DDX_Control(pDX, IDC_FILES_EXT, m_extensions);
	DDX_Control(pDX, IDC_FILELIST, m_FileDescs);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(OptFiles, CDialog)
	//{{AFX_MSG_MAP(OptFiles)
	ON_CBN_SELCHANGE(IDC_FILELIST, OnSelchangeFilelist)
	ON_BN_CLICKED(IDC_ASSOCIATE, OnAssociate)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// OptFiles message handlers

BOOL OptFiles::OnInitDialog()
{
	CDialog::OnInitDialog();

	Osmo4 *gpac = GetApp();
	u32 count, i;

	while (m_FileDescs.GetCount()) m_FileDescs.DeleteString(0);
	count = gf_cfg_get_key_count(gpac->m_user.config, "MimeTypes");
	for (i=0; i<count; i++) {
		char *sMime, *sKey, sDesc[200];
		const char *sOpt;
		sMime = (char *) gf_cfg_get_key_name(gpac->m_user.config, "MimeTypes", i);
		if (!sMime) continue;
		sOpt = gf_cfg_get_key(gpac->m_user.config, "MimeTypes", sMime);
		if (!sOpt) continue;
		sKey = (char *) strstr(sOpt, "\" \"");
		if (!sKey) continue;
		strcpy(sDesc, sKey+3);
		sKey = strchr(sDesc, '\"');
		if (!sKey) continue;
		sKey[0] = 0;
		m_FileDescs.AddString(sDesc);
	}
	m_FileDescs.SetCurSel(0);
	SetSelection(0);
	return TRUE;
}

void OptFiles::OnSelchangeFilelist()
{
	SetSelection(m_FileDescs.GetCurSel());
}

void OptFiles::SetSelection(u32 sel)
{
	Osmo4 *gpac = GetApp();
	char *sMime, *sKey, sDesc[200], sText[200];
	sMime = (char *) gf_cfg_get_key_name(gpac->m_user.config, "MimeTypes", sel);
	sprintf(sText, "Mime Type: %s", sMime);
	m_mimes.SetWindowText(sText);
	strcpy(cur_mime, sMime);
	sMime = (char *) gf_cfg_get_key(gpac->m_user.config, "MimeTypes", sMime);
	strcpy(sDesc, sMime+1);
	sKey = strchr(sDesc, '\"');
	sKey[0] = 0;
	sprintf(sText, "Extensions: %s", sDesc);
	strcpy(cur_ext, sDesc);
	m_extensions.SetWindowText(sText);
	sKey = strrchr(sMime, '\"');
	sprintf(sText, "Module: %s", sKey+2);
	m_PlugName.SetWindowText(sText);

	Bool has_asso, need_asso, go = GF_TRUE;
	sKey = cur_ext;
	need_asso = has_asso = GF_FALSE;

	HKEY hKey;
	DWORD dwSize;
	while (go) {
		Bool ok;
		char szExt[50], szReg[60], c;
		char *tmp = strchr(sKey, ' ');
		if (!tmp) {
			go = GF_FALSE;
		}
		else {
			c = tmp[0];
			tmp[0] = 0;
		}
		sprintf(szExt, ".%s", sKey);
		sprintf(szReg, "GPAC\\%s", sKey);
		if (tmp) {
			tmp[0] = c;
			tmp += 1;
		}

		if (RegOpenKeyEx(HKEY_CLASSES_ROOT, szExt, 0, KEY_READ, &hKey ) == ERROR_SUCCESS) {
			dwSize = 200;
			ok = GF_TRUE;
			if (RegQueryValueEx(hKey, "", NULL, NULL,(unsigned char*) sDesc, &dwSize) != ERROR_SUCCESS) ok = GF_FALSE;
			RegCloseKey(hKey);
			if (ok && !stricmp((char *)sDesc, szReg)) has_asso = GF_TRUE;
			else need_asso = GF_TRUE;
		} else need_asso = GF_TRUE;
		sKey = tmp;

	}
	m_DoAssociate.SetCheck(has_asso);
	if (need_asso && has_asso)
		OnAssociate();
}


void OptFiles::OnAssociate()
{
	char *sKey, sDesc[200];
	unsigned char szApp[MAX_PATH];
	unsigned char szIco[MAX_PATH];

	strcpy((char *) szApp, GetApp()->szApplicationPath);
	strcpy((char *) szIco, (const char *) szApp);
	strcat((char *) szIco, "Osmo4.ico");
	strcat((char *) szApp, "Osmo4.exe \"%L\"");

	if (m_DoAssociate.GetCheck()) {
		Bool go = GF_TRUE;
		sKey = cur_ext;

		HKEY hKey;
		DWORD dwSize;
		while (go) {
			Bool ok;
			char szExt[50], szReg[60], szOld[80], szPath[1024], c;
			char *tmp = strchr(sKey, ' ');
			if (!tmp) {
				go = GF_FALSE;
			}
			else {
				c = tmp[0];
				tmp[0] = 0;
			}
			sprintf(szExt, ".%s", sKey);
			sprintf(szReg, "GPAC\\%s", sKey);
			if (tmp) {
				tmp[0] = c;
				tmp += 1;
			}

			RegOpenKeyEx(HKEY_CLASSES_ROOT, szExt, 0, 0, &hKey );
			dwSize = 200;
			ok = GF_TRUE;
			if (RegQueryValueEx(hKey, "", NULL, NULL,(unsigned char*) sDesc, &dwSize) != ERROR_SUCCESS) ok = GF_FALSE;
			RegCloseKey(hKey);
			strcpy(szOld, "");
			if (ok && stricmp((char *)sDesc, szReg)) strcpy(szOld, sDesc);

			strcpy(szPath, szReg);
			strcat(szPath, "\\DefaultIcon");
			RegCreateKeyEx(HKEY_CLASSES_ROOT, szPath, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &dwSize);
			RegSetValueEx(hKey, "", 0, REG_SZ, szIco, (DWORD) strlen((const char *) szIco)+1);
			RegCloseKey(hKey);

			strcpy(szPath, szReg);
			strcat(szPath, "\\Shell\\open\\command");
			RegCreateKeyEx(HKEY_CLASSES_ROOT, szPath, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &dwSize);
			RegSetValueEx(hKey, "", 0, REG_SZ, szApp, (DWORD) strlen((const char *) szApp)+1);
			RegCloseKey(hKey);

			if (strlen(szOld)) {
				strcpy(szPath, szReg);
				strcat(szPath, "\\Backup");
				RegCreateKeyEx(HKEY_CLASSES_ROOT, szPath, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &dwSize);
				RegSetValueEx(hKey, "", 0, REG_SZ, (unsigned char *) szOld, (DWORD) strlen((const char *) szIco)+1);
				RegCloseKey(hKey);
			}

			RegCreateKeyEx(HKEY_CLASSES_ROOT, szExt, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &dwSize);
			RegSetValueEx(hKey, "", 0, REG_SZ, (const unsigned char *) szReg, (DWORD) strlen(szReg)+1);
			RegCloseKey(hKey);

			sKey = tmp;
		}
	} else {
		Bool go = GF_TRUE;
		sKey = cur_ext;

		HKEY hKey;
		DWORD dwSize;
		while (go) {
			Bool ok;
			char szExt[50], szReg[60], szPath[1024], c;
			char *tmp = strchr(sKey, ' ');
			if (!tmp) {
				go = GF_FALSE;
			}
			else {
				c = tmp[0];
				tmp[0] = 0;
			}
			sprintf(szExt, ".%s", sKey);
			sprintf(szReg, "GPAC\\%s", sKey);
			if (tmp) {
				tmp[0] = c;
				tmp += 1;
			}

			strcpy(szPath, szReg);
			strcat(szPath, "\\Backup");
			RegOpenKeyEx(HKEY_CLASSES_ROOT, szPath, 0, 0, &hKey );
			dwSize = 200;
			ok = GF_TRUE;
			if (RegQueryValueEx(hKey, "", NULL, NULL,(unsigned char*) sDesc, &dwSize) != ERROR_SUCCESS) ok = GF_FALSE;
			RegCloseKey(hKey);
			if (ok && strlen((char *)sDesc)) {
				RegCreateKeyEx(HKEY_CLASSES_ROOT, szExt, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &dwSize);
				RegSetValueEx(hKey, "", 0, REG_SZ, (unsigned char*) sDesc, (DWORD) strlen((const char *) sDesc)+1);
				RegCloseKey(hKey);
			}

			RegOpenKeyEx(HKEY_CLASSES_ROOT, szReg, 0, 0, &hKey );
			RegDeleteKey(hKey, "Backup");
			RegDeleteKey(hKey, "DefaultIcon");
			RegDeleteKey(hKey, "Shell\\open\\command");
			RegDeleteKey(hKey, "Shell\\open");
			RegDeleteKey(hKey, "Shell");
			RegCloseKey(hKey);
			RegDeleteKey(HKEY_CLASSES_ROOT, szReg);

			sKey = tmp;
		}
	}
}

COptLogs::COptLogs(CWnd* pParent /*=NULL*/)
	: CDialog(COptLogs::IDD, pParent)
{
	//{{AFX_DATA_INIT(COptLogs)
	// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void COptLogs::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptLogs)
	DDX_Control(pDX, IDC_TOOL_SYNC, m_sync);
	DDX_Control(pDX, IDC_TOOL_SCRIPT, m_script);
	DDX_Control(pDX, IDC_TOOL_SCENE, m_scene);
	DDX_Control(pDX, IDC_TOOL_RTP, m_rtp);
	DDX_Control(pDX, IDC_TOOL_RENDER, m_render);
	DDX_Control(pDX, IDC_TOOL_PARSER, m_parser);
	DDX_Control(pDX, IDC_TOOL_NET, m_net);
	DDX_Control(pDX, IDC_TOOL_MMIO, m_mmio);
	DDX_Control(pDX, IDC_TOOL_MEDIA, m_media);
	DDX_Control(pDX, IDC_TOOL_CORE, m_core);
	DDX_Control(pDX, IDC_TOOL_CONTAINER, m_container);
	DDX_Control(pDX, IDC_TOOL_COMPOSE, m_compose);
	DDX_Control(pDX, IDC_TOOL_CODING, m_coding);
	DDX_Control(pDX, IDC_TOOL_CODEC, m_codec);
	DDX_Control(pDX, IDC_TOOL_AUTHOR, m_author);
	DDX_Control(pDX, IDC_LOG_LEVEL, m_Level);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptLogs, CDialog)
	//{{AFX_MSG_MAP(COptLogs)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptLogs message handlers

BOOL COptLogs::OnInitDialog()
{
	CDialog::OnInitDialog();

#if 0
	Osmo4 *gpac = GetApp();
	switch (gpac->m_log_level) {
	case GF_LOG_ERROR:
		m_Level.SetCurSel(1);
		break;
	case GF_LOG_WARNING:
		m_Level.SetCurSel(2);
		break;
	case GF_LOG_INFO:
		m_Level.SetCurSel(3);
		break;
	case GF_LOG_DEBUG:
		m_Level.SetCurSel(4);
		break;
	default:
		m_Level.SetCurSel(0);
		break;
	}

	m_sync.SetCheck(gpac->m_log_tools & GF_LOG_COMPTIME);
	m_script.SetCheck(gpac->m_log_tools & GF_LOG_SCRIPT);
	m_scene.SetCheck(gpac->m_log_tools & GF_LOG_SCENE);
	m_rtp.SetCheck(gpac->m_log_tools & GF_LOG_RTP);
	m_render.SetCheck(gpac->m_log_tools & GF_LOG_COMPOSE);
	m_parser.SetCheck(gpac->m_log_tools & GF_LOG_PARSER);
	m_net.SetCheck(gpac->m_log_tools & GF_LOG_NETWORK);
	m_mmio.SetCheck(gpac->m_log_tools & GF_LOG_MMIO);
	m_media.SetCheck(gpac->m_log_tools & GF_LOG_MEDIA);
	m_core.SetCheck(gpac->m_log_tools & GF_LOG_CORE);
	m_container.SetCheck(gpac->m_log_tools & GF_LOG_CONTAINER);
	m_compose.SetCheck(gpac->m_log_tools & GF_LOG_INTERACT);
	m_coding.SetCheck(gpac->m_log_tools & GF_LOG_CODING);
	m_codec.SetCheck(gpac->m_log_tools & GF_LOG_CODEC);
	m_author.SetCheck(gpac->m_log_tools & GF_LOG_FILTER);
#endif

	return TRUE;
}

void COptLogs::SaveOptions()
{
	Osmo4 *gpac = GetApp();
	CString str = "";
	const char *level = "error";
	u32 flags = 0;

	switch (m_Level.GetCurSel()) {
	case 1:
		level = "error";
		break;
	case 2:
		level = "warning";
		break;
	case 3:
		level = "info";
		break;
	case 4:
		level = "debug";
		break;
	default:
		level = "none";
		break;
	}

	if (m_sync.GetCheck()) {
		str +="sync:";
	}
	if (m_script.GetCheck()) {
		str +="script:";
	}
	if (m_scene.GetCheck()) {
		str +="scene:";
	}
	if (m_rtp.GetCheck()) {
		str +="rtp:";
	}
	if (m_render.GetCheck()) {
		str +="compose:";
	}
	if (m_parser.GetCheck()) {
		str +="parser:";
	}
	if (m_net.GetCheck()) {
		str +="network:";
	}
	if (m_mmio.GetCheck()) {
		str +="mmio:";
	}
	if (m_media.GetCheck()) {
		str +="media:";
	}
	if (m_core.GetCheck()) {
		str +="core:";
	}
	if (m_container.GetCheck()) {
		str +="container:";
	}
	if (m_compose.GetCheck()) {
		str +="interact:";
	}
	if (m_coding.GetCheck()) {
		str +="coding:";
	}
	if (m_codec.GetCheck()) {
		str +="codec:";
	}
	if (m_author.GetCheck()) {
		str +="author:";
	}

	gf_cfg_set_key(gpac->m_user.config, "General", "Logs", str);
	str += "@";
	str += level;
	gf_log_set_tools_levels(str);
}

void COptHTTP::OnBnClickedRestartCache()
{
	// TODO : ajoutez ici le code de votre gestionnaire de notification de contrôle
}
