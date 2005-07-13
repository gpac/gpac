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

	m_Selector.AddString("General");
	m_Selector.AddString("MPEG-4 Systems");
	m_Selector.AddString("Media Decoders");
	m_Selector.AddString("Rendering");
	m_Selector.AddString("Renderer 2D");
	m_Selector.AddString("Renderer 3D");
	m_Selector.AddString("Video Output");
	m_Selector.AddString("Audio Output");
	m_Selector.AddString("Text Engine");
	m_Selector.AddString("File Download");
	m_Selector.AddString("Real-Time Streaming");
	m_Selector.AddString("Streaming Cache");
	m_Selector.AddString("File Types");

	HideAll();
	m_Selector.SetCurSel(0);
	m_general.ShowWindow(SW_SHOW);

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
}

void COptions::OnSelchangeSelect() 
{
	HideAll();
	switch (m_Selector.GetCurSel()) {
	case 0: m_general.ShowWindow(SW_SHOW); break;
	case 1: m_systems.ShowWindow(SW_SHOW); break;
	case 2: m_decoder.ShowWindow(SW_SHOW); break;
	case 3: m_render.ShowWindow(SW_SHOW); break;
	case 4: m_render2d.ShowWindow(SW_SHOW); break;
	case 5: m_render3d.ShowWindow(SW_SHOW); break;
	case 6: m_video.ShowWindow(SW_SHOW); break;
	case 7: m_audio.ShowWindow(SW_SHOW); break;
	case 8: m_font.ShowWindow(SW_SHOW); break;
	case 9: m_http.ShowWindow(SW_SHOW); break;
	case 10: m_stream.ShowWindow(SW_SHOW); break;
	case 11: m_cache.ShowWindow(SW_SHOW); break;
	case 12: m_files.ShowWindow(SW_SHOW); break;
	}
}

void COptions::OnSaveopt() 
{
	Bool need_reload;
	m_general.SaveOptions();
	m_systems.SaveOptions();
	m_decoder.SaveOptions();
	need_reload = m_render.SaveOptions();
	m_render2d.SaveOptions();
	m_render3d.SaveOptions();
	m_audio.SaveOptions();
	m_video.SaveOptions();
	m_http.SaveOptions();
	m_font.SaveOptions();
	m_stream.SaveOptions();
	m_cache.SaveOptions();

	WinGPAC *gpac = GetApp();
	if (!need_reload) {
		gf_term_set_option(gpac->m_term, GF_OPT_RELOAD_CONFIG, 1);
	} else {
		gpac->ReloadTerminal();
	}
	m_render2d.SetYUV();
}

void COptions::OnClose() 
{
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

	WinGPAC *gpac = GetApp();
	char *sOpt;
	
	sOpt = gf_cfg_get_key(gpac->m_user.config, "General", "Loop");
	m_Loop.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(gpac->m_user.config, "General", "LookForSubtitles");
	m_LookForSubs.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(gpac->m_user.config, "General", "ConsoleOff");
	m_NoConsole.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(gpac->m_user.config, "General", "ViewXMT");
	m_ViewXMT.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	return TRUE; 
}

void COptGen::SaveOptions()
{
	WinGPAC *gpac = GetApp();

	gpac->m_Loop = m_Loop.GetCheck();
	gf_cfg_set_key(gpac->m_user.config, "General", "Loop", gpac->m_Loop ? "yes" : "no");
	gpac->m_LookForSubtitles = m_LookForSubs.GetCheck();
	gf_cfg_set_key(gpac->m_user.config, "General", "LookForSubtitles",  gpac->m_LookForSubtitles ? "yes" : "no");
	gpac->m_NoConsole = m_NoConsole.GetCheck();
	gf_cfg_set_key(gpac->m_user.config, "General", "ConsoleOff", gpac->m_NoConsole ? "yes" : "no");
	gpac->m_ViewXMTA = m_ViewXMT.GetCheck();
	gf_cfg_set_key(gpac->m_user.config, "General", "ViewXMT", gpac->m_ViewXMTA ? "yes" : "no");

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




/*ISO 639-2 code names (complete set in /doc directory)*/
#define NUM_LANGUAGE	59
static char *Languages[118] = 
{
"Albanian","alb",
"Arabic","ara",
"Armenian","arm",
"Azerbaijani","aze",
"Basque","baq",
"Belarusian","bel",
"Bosnian","bos",
"Breton","bre",
"Bulgarian","bul",
"Catalan","cat",
"Celtic (Other)","cel",
"Chinese","chi",
"Croatian","scr",
"Czech","cze",
"Danish","dan",
"Dutch","dut",
"English","eng",
"Esperanto","epo",
"Estonian","est",
"Fijian","fij",
"Finnish","fin",
"French","fre",
"Georgian","geo",
"German","ger",
"Greek, Modern (1453-)","gre",
"Haitian","hat",
"Hawaiian","haw",
"Hebrew","heb",
"Indonesian","ind",
"Iranian (Other)","ira",
"Irish","gle",
"Italian","ita",
"Japanese","jpn",
"Korean","kor",
"Kurdish","kur",
"Latin","lat",
"Lithuanian","lit",
"Luxembourgish","ltz",
"Macedonian","mac",
"Mongolian","mon",
"Norwegian","nor",
"Occitan (post 1500)","oci",
"Persian","per",
"Philippine (Other)","phi" ,
"Polish","pol",
"Portuguese","por",
"Russian","rus",
"Serbian","srp",
"Slovak","slo",
"Slovenian","slv",
"Somali","som",
"Spanish","spa",
"Swedish","swe",
"Tahitian","tah",
"Thai","tha",
"Tibetan","tib",
"Turkish","tur",
"Undetermined","und",
"Vietnamese","vie",
};


BOOL COptSystems::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	WinGPAC *gpac = GetApp();
	char *sOpt;

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Systems", "Language");
	if (!sOpt) sOpt = "eng";
	s32 select = 0;
	while (m_Lang.GetCount()) m_Lang.DeleteString(0);
	for (s32 i = 0; i<NUM_LANGUAGE; i++) {
		m_Lang.AddString(Languages[2*i]);
		if (sOpt && !stricmp(sOpt, Languages[2*i + 1])) select = i;
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
	WinGPAC *gpac = GetApp();

	s32 sel = m_Lang.GetCurSel();
	gf_cfg_set_key(gpac->m_user.config, "Systems", "Language", Languages[2*sel + 1]);
	sel = m_Threading.GetCurSel();
	gf_cfg_set_key(gpac->m_user.config, "Systems", "ThreadingPolicy", (sel==0) ? "Single" : ( (sel==1) ? "Multi" : "Free"));
	gf_cfg_set_key(gpac->m_user.config, "Systems", "ForceSingleClock", m_ForceDuration.GetCheck() ? "yes" : "no");
	gf_cfg_set_key(gpac->m_user.config, "Systems", "AlwaysDrawBIFS", m_BifsAlwaysDrawn.GetCheck() ? "yes" : "no");
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
	const char *sOpt;
	CDialog::OnInitDialog();
	
	WinGPAC *gpac = GetApp();
	
	/*audio dec enum*/
	while (m_Audio.GetCount()) m_Audio.DeleteString(0);
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Systems", "DefAudioDec");
	u32 count = gf_modules_get_count(gpac->m_user.modules);
	GF_BaseDecoder *ifce;
	s32 select = 0;
	s32 to_sel = 0;
	for (u32 i=0; i<count; i++) {
		if (!gf_modules_load_interface(gpac->m_user.modules, i, GF_MEDIA_DECODER_INTERFACE, (void **) &ifce)) continue;

		if (ifce->CanHandleStream(ifce, GF_STREAM_AUDIO, 0, NULL, 0, 0)) {
			if (sOpt && !stricmp(ifce->module_name, sOpt)) select = to_sel;
			m_Audio.AddString(ifce->module_name);
			to_sel++;
		}
		gf_modules_close_interface(ifce);
	}
	m_Audio.SetCurSel(select);

	/*video dec enum*/
	while (m_Video.GetCount()) m_Video.DeleteString(0);
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Systems", "DefVideoDec");
	count = gf_modules_get_count(gpac->m_user.modules);
	select = 0;
	to_sel = 0;
	for (i=0; i<count; i++) {
		if (!gf_modules_load_interface(gpac->m_user.modules, i, GF_MEDIA_DECODER_INTERFACE, (void **) &ifce)) continue;

		if (ifce->CanHandleStream(ifce, GF_STREAM_VISUAL, 0, NULL, 0, 0)) {
			if (sOpt && !stricmp(ifce->module_name, sOpt)) select = to_sel;
			m_Video.AddString(ifce->module_name);
			to_sel++;
		}
		gf_modules_close_interface(ifce);
	}
	m_Video.SetCurSel(select);
	return TRUE;  
}

void OptDecoder::SaveOptions()
{
	WinGPAC *gpac = GetApp();
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
	DDX_Control(pDX, IDC_FAST_RENDER, m_FastRender);
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
	
	WinGPAC *gpac = GetApp();
	char *sOpt;
	
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Rendering", "RendererName");
	m_Use3DRender.SetCheck( (sOpt && strstr(sOpt, "3D")) ? 1 : 0);

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Rendering", "ForceSceneSize");
	m_ForceSize.SetCheck( (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Rendering", "FrameRate");
	if (!sOpt) sOpt = "30.0";
	s32 select = 0;
	while (m_BIFSRate.GetCount()) m_BIFSRate.DeleteString(0);
	for (s32 i = 0; i<NUM_RATES; i++) {
		m_BIFSRate.AddString(BIFSRates[i]);
		if (sOpt && !stricmp(sOpt, BIFSRates[i]) ) select = i;
	}
	m_BIFSRate.SetCurSel(select);
	
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Rendering", "FastRender");
	m_FastRender.SetCheck( (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Rendering", "AntiAlias");
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
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Rendering", "Raster2D");
	s32 count = gf_modules_get_count(gpac->m_user.modules);
	void *ifce;
	select = 0;
	u32 to_sel = 0;
	for (i=0; i<count; i++) {
		if (!gf_modules_load_interface(gpac->m_user.modules, i, GF_RASTER_2D_INTERFACE, &ifce)) continue;
		if (sOpt && !stricmp(((GF_BaseInterface *)ifce)->module_name, sOpt)) select = to_sel;
		m_Graphics.AddString(((GF_BaseInterface *)ifce)->module_name);
		gf_modules_close_interface(ifce);
		to_sel++;
	}
	m_Graphics.SetCurSel(select);


	m_DrawBounds.AddString("None");
	m_DrawBounds.AddString("Box/Rect");
	m_DrawBounds.AddString("AABB Tree");
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Rendering", "BoundingVolume");
	if (sOpt && !stricmp(sOpt, "Box")) m_DrawBounds.SetCurSel(1);
	else if (sOpt && !stricmp(sOpt, "AABB")) m_DrawBounds.SetCurSel(2);
	else m_DrawBounds.SetCurSel(0);

	return TRUE;  
}


Bool COptRender::SaveOptions()
{
	char str[50];
	WinGPAC *gpac = GetApp();

	gf_cfg_set_key(gpac->m_user.config, "Rendering", "FastRender", m_FastRender.GetCheck() ? "yes" : "no");
	gf_cfg_set_key(gpac->m_user.config, "Rendering", "ForceSceneSize", m_ForceSize.GetCheck() ? "yes" : "no");

	s32 sel = m_BIFSRate.GetCurSel();
	gf_cfg_set_key(gpac->m_user.config, "Rendering", "FrameRate", BIFSRates[sel]);

	sel = m_AntiAlias.GetCurSel();
	gf_cfg_set_key(gpac->m_user.config, "Rendering", "AntiAlias", (sel==0) ? "None" : ( (sel==1) ? "Text" : "All"));

	sel = m_DrawBounds.GetCurSel();
	gf_cfg_set_key(gpac->m_user.config, "Rendering", "BoundingVolume", (sel==2) ? "AABB" : (sel==1) ? "Box" : "None");

	m_Graphics.GetWindowText(str, 50);
	gf_cfg_set_key(gpac->m_user.config, "Rendering", "Raster2D", str);

	char *opt;
	opt = gf_cfg_get_key(gpac->m_user.config, "Rendering", "RendererName");
	if (!opt || strstr(opt, "2D")) {
		if (!m_Use3DRender.GetCheck()) return 0;
		gf_cfg_set_key(gpac->m_user.config, "Rendering", "RendererName", "GPAC 3D Renderer");
		return 1;
	}
	if (m_Use3DRender.GetCheck()) return 0;
	gf_cfg_set_key(gpac->m_user.config, "Rendering", "RendererName", "GPAC 2D Renderer");
	return 1;
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
	
	WinGPAC *gpac = GetApp();
	char *sOpt;

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Render2D", "DirectRender");
	if (sOpt && !stricmp(sOpt, "yes")) {
		m_DirectRender.SetCheck(1);
	} else {
		m_DirectRender.SetCheck(0);
	}
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Render2D", "ScalableZoom");
	if (sOpt && !stricmp(sOpt, "no")) {
		m_Scalable.SetCheck(0);
	} else {
		m_Scalable.SetCheck(1);
	}
	
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Render2D", "DisableYUV");
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
	WinGPAC *gpac = GetApp();
	u32 yuv_format = gf_term_get_option(gpac->m_term, GF_OPT_YUV_FORMAT);
	if (!yuv_format) {
		m_YUVFormat.SetWindowText("(No YUV used)");
	} else {
		char fmt[5], str[100];
		sprintf(str, "(%s used)", gf_4cc_to_str(yuv_format, fmt));
		m_YUVFormat.SetWindowText(str);
	}
}

void COptRender2D::SaveOptions()
{
	WinGPAC *gpac = GetApp();
	gf_cfg_set_key(gpac->m_user.config, "Render2D", "DirectRender", m_DirectRender.GetCheck() ? "yes" : "no");
	gf_cfg_set_key(gpac->m_user.config, "Render2D", "ScalableZoom", m_Scalable.GetCheck() ? "yes" : "no");

	gf_cfg_set_key(gpac->m_user.config, "Render2D", "DisableYUV", m_NoYUV.GetCheck() ? "yes" : "no");
}


COptRender3D::COptRender3D(CWnd* pParent /*=NULL*/)
	: CDialog(COptRender3D::IDD, pParent)
{
	//{{AFX_DATA_INIT(COptRender3D)
	m_Wireframe = -1;
	//}}AFX_DATA_INIT
}


void COptRender3D::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptRender3D)
	DDX_Control(pDX, IDC_NO_BACKCULL, m_NoBackCull);
	DDX_Control(pDX, IDC_BITMAP_USE_PIXEL, m_BitmapPixels);
	DDX_Control(pDX, IDC_DISABLE_TX_RECT, m_DisableTXRect);
	DDX_Control(pDX, IDC_RASTER_OUTLINE, m_RasterOutlines);
	DDX_Control(pDX, IDC_EMUL_POW2, m_EmulPow2);
	DDX_Control(pDX, IDC_DISABLE_POLY_AA, m_PolyAA);
	DDX_Radio(pDX, IDC_WIRE_NONE, m_Wireframe);
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
	

	WinGPAC *gpac = GetApp();
	char *sOpt;

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Render3D", "RasterOutlines");
	m_RasterOutlines.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Render3D", "EmulatePOW2");
	m_EmulPow2.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Render3D", "PolygonAA");
	m_PolyAA.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Render3D", "DisableBackFaceCulling");
	m_NoBackCull.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Render3D", "Wireframe");
	if (sOpt && !stricmp(sOpt, "WireOnly")) m_Wireframe = 1;
	else if (sOpt && !stricmp(sOpt, "WireOnSolid")) m_Wireframe = 2;
	else m_Wireframe = 0;
	UpdateData(FALSE);

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Render3D", "BitmapCopyPixels");
	m_BitmapPixels.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Render3D", "DisableRectExt");
	m_DisableTXRect.SetCheck((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void COptRender3D::SaveOptions()
{
	WinGPAC *gpac = GetApp();

	gf_cfg_set_key(gpac->m_user.config, "Render3D", "RasterOutlines", m_RasterOutlines.GetCheck() ? "yes" : "no");
	gf_cfg_set_key(gpac->m_user.config, "Render3D", "EmulatePOW2", m_EmulPow2.GetCheck() ? "yes" : "no");
	gf_cfg_set_key(gpac->m_user.config, "Render3D", "PolygonAA", m_PolyAA.GetCheck() ? "yes" : "no");
	gf_cfg_set_key(gpac->m_user.config, "Render3D", "DisableBackFaceCulling", m_NoBackCull.GetCheck() ? "yes" : "no");

	UpdateData();
	switch (m_Wireframe) {
	case 2:
		gf_cfg_set_key(gpac->m_user.config, "Render3D", "Wireframe", "WireOnSolid");
		break;
	case 1:
		gf_cfg_set_key(gpac->m_user.config, "Render3D", "Wireframe", "WireOnly");
		break;
	case 0:
		gf_cfg_set_key(gpac->m_user.config, "Render3D", "Wireframe", "WireNone");
		break;
	}
	gf_cfg_set_key(gpac->m_user.config, "Render3D", "DisableRectExt", m_DisableTXRect.GetCheck() ? "yes" : "no");
	gf_cfg_set_key(gpac->m_user.config, "Render3D", "BitmapCopyPixels", m_BitmapPixels.GetCheck() ? "yes" : "no");
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

	WinGPAC *gpac = GetApp();
	char *sOpt;
	
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Video", "SwitchResolution");
	if (sOpt && !stricmp(sOpt, "yes")) {
		m_SwitchRes.SetCheck(1);
	} else {
		m_SwitchRes.SetCheck(0);
	}

	
	u32 count = gf_modules_get_count(gpac->m_user.modules);
	void *ifce;
	s32 to_sel = 0;
	s32 select = 0;
	/*video drivers enum*/
	while (m_Videos.GetCount()) m_Videos.DeleteString(0);
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Video", "DriverName");

	for (u32 i=0; i<count; i++) {
		if (!gf_modules_load_interface(gpac->m_user.modules, i, GF_VIDEO_OUTPUT_INTERFACE, &ifce)) continue;
		if (sOpt && !stricmp(((GF_BaseInterface *)ifce)->module_name, sOpt)) select = to_sel;
		m_Videos.AddString(((GF_BaseInterface *)ifce)->module_name);
		gf_modules_close_interface(ifce);
		to_sel++;
	}
	m_Videos.SetCurSel(select);

	return TRUE;
	            
}

void COptVideo::SaveOptions()
{
	WinGPAC *gpac = GetApp();
	char str[50];

	gf_cfg_set_key(gpac->m_user.config, "Video", "SwitchResolution", m_SwitchRes.GetCheck() ? "yes" : "no");
	m_Videos.GetWindowText(str, 50);
	gf_cfg_set_key(gpac->m_user.config, "Video", "DriverName", str);
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
	DDX_Control(pDX, IDC_AUDIO_FPS, m_AudioFPS);
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
	m_SpinFPS.SetBuddy(& m_AudioFPS);

	WinGPAC *gpac = GetApp();
	char *sOpt;

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
		m_AudioEdit.SetWindowText("6");
	}
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Audio", "BuffersPerSecond");
	if (sOpt) {
		m_AudioFPS.SetWindowText(sOpt);
	} else {
		m_AudioFPS.SetWindowText("15");
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
		if (!gf_modules_load_interface(gpac->m_user.modules, i, GF_AUDIO_OUTPUT_INTERFACE, (void **) &ifce)) continue;
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
	WinGPAC *gpac = GetApp();
	char str[50];

	gf_cfg_set_key(gpac->m_user.config, "Audio", "ForceConfig", m_ForceConfig.GetCheck() ? "yes" : "no");
	gf_cfg_set_key(gpac->m_user.config, "Audio", "NoResync", m_AudioResync.GetCheck() ? "yes" : "no");

	m_AudioEdit.GetWindowText(str, 20);
	gf_cfg_set_key(gpac->m_user.config, "Audio", "NumBuffers", str);
	m_AudioFPS.GetWindowText(str, 20);
	gf_cfg_set_key(gpac->m_user.config, "Audio", "BuffersPerSecond", str);

	m_DriverList.GetWindowText(str, 50);
	gf_cfg_set_key(gpac->m_user.config, "Audio", "DriverName", str);

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
	m_AudioFPS.EnableWindow(en);
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
	DDX_Control(pDX, IDC_TEXTURE_TEXT, m_UseTexture);
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
	void *ifce;
	
	CDialog::OnInitDialog();
	
	WinGPAC *gpac = GetApp();
	char *sOpt;

	/*video drivers enum*/
	while (m_Fonts.GetCount()) m_Fonts.DeleteString(0);
	sOpt = gf_cfg_get_key(gpac->m_user.config, "FontEngine", "DriverName");
	s32 to_sel = 0;
	s32 select = 0;
	u32 count = gf_modules_get_count(gpac->m_user.modules);
	for (i=0; i<count; i++) {
		if (!gf_modules_load_interface(gpac->m_user.modules, i, GF_FONT_RASTER_INTERFACE, &ifce)) continue;
		if (sOpt && !stricmp(((GF_BaseInterface *)ifce)->module_name, sOpt)) select = to_sel;
		m_Fonts.AddString(((GF_BaseInterface *)ifce)->module_name);
		gf_modules_close_interface(ifce);
		to_sel++;
	}
	m_Fonts.SetCurSel(select);
	

	sOpt = gf_cfg_get_key(gpac->m_user.config, "FontEngine", "FontDirectory");
	if (sOpt) m_BrowseFont.SetWindowText(sOpt);


	sOpt = gf_cfg_get_key(gpac->m_user.config, "FontEngine", "UseTextureText");
	m_UseTexture.SetCheck( (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);

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
	WinGPAC *gpac = GetApp();
	char str[MAX_PATH];
		
	m_Fonts.GetWindowText(str, 50);
	gf_cfg_set_key(gpac->m_user.config, "FontEngine", "DriverName", str);
	m_BrowseFont.GetWindowText(str, 50);
	gf_cfg_set_key(gpac->m_user.config, "FontEngine", "FontDirectory", str);
	gf_cfg_set_key(gpac->m_user.config, "FontEngine", "UseTextureText", m_UseTexture.GetCheck() ? "yes" : "no");
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
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptHTTP, CDialog)
	//{{AFX_MSG_MAP(COptHTTP)
	ON_BN_CLICKED(IDC_BROWSE_CACHE, OnBrowseCache)
	//}}AFX_MSG_MAP
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
	CDialog::OnInitDialog();
	
	WinGPAC *gpac = GetApp();
	char *sOpt;

	sOpt = gf_cfg_get_key(gpac->m_user.config, "Downloader", "CleanCache");
	if (sOpt && !stricmp(sOpt, "yes")) {
		m_CleanCache.SetCheck(1);
	} else {
		m_CleanCache.SetCheck(0);
	}
	sOpt = gf_cfg_get_key(gpac->m_user.config, "Downloader", "RestartFiles");
	if (sOpt && !stricmp(sOpt, "yes")) {
		m_RestartFile.SetCheck(1);
	} else {
		m_RestartFile.SetCheck(0);
	}

	sOpt = gf_cfg_get_key(gpac->m_user.config, "General", "CacheDirectory");
	if (sOpt) m_CacheDir.SetWindowText(sOpt);
	
	return TRUE; 
}


void COptHTTP::SaveOptions()
{
	WinGPAC *gpac = GetApp();

	gf_cfg_set_key(gpac->m_user.config, "Downloader", "CleanCache", m_CleanCache.GetCheck() ? "yes" : "no");
	gf_cfg_set_key(gpac->m_user.config, "Downloader", "RestartFiles", m_RestartFile.GetCheck() ? "yes" : "no");

	m_CacheDir.GetWindowText(szCacheDir, MAX_PATH);
	gf_cfg_set_key(gpac->m_user.config, "General", "CacheDirectory", szCacheDir);
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

	WinGPAC *gpac = GetApp();
	char *sOpt;

	while (m_Port.GetCount()) m_Port.DeleteString(0);
	m_Port.AddString("554 (RTSP standard)");
	m_Port.AddString("7070 (RTSP ext)");
	m_Port.AddString("80 (RTSP / HTTP tunnel)");
	m_Port.AddString("8080 (RTSP / HTTP tunnel)");

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
	WinGPAC *gpac = GetApp();
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
	
	WinGPAC *gpac = GetApp();
	char *sOpt;
	
	sOpt = gf_cfg_get_key(gpac->m_user.config, "StreamingCache", "RecordDirectory");
	if (!sOpt) sOpt = gf_cfg_get_key(gpac->m_user.config, "General", "CacheDirectory");
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
	WinGPAC *gpac = GetApp();

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
	
	WinGPAC *gpac = GetApp();
	u32 count, i;

	while (m_FileDescs.GetCount()) m_FileDescs.DeleteString(0);
	count = gf_cfg_get_key_count(gpac->m_user.config, "MimeTypes");
	for (i=0; i<count; i++) {
		char *sMime, *sKey, sDesc[200];
		sMime = (char *) gf_cfg_get_key_name(gpac->m_user.config, "MimeTypes", i);
		if (!sMime) continue;
		sKey = gf_cfg_get_key(gpac->m_user.config, "MimeTypes", sMime);
		if (!sKey) continue;
		sKey = strstr(sKey, "\" \"");
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
	WinGPAC *gpac = GetApp();
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

	Bool has_asso, need_asso, go = 1;
	sKey = cur_ext;
	need_asso = has_asso = 0;

	HKEY hKey;
	DWORD dwSize;
	while (go) {
		Bool ok;
		char szExt[50], szReg[60], c;
		char *tmp = strchr(sKey, ' ');
		if (!tmp) { go = 0;}
		else { c = tmp[0]; tmp[0] = 0; }
		sprintf(szExt, ".%s", sKey);
		sprintf(szReg, "GPAC\\%s", sKey);
		if (tmp) { tmp[0] = c; tmp += 1; }

		if (RegOpenKeyEx(HKEY_CLASSES_ROOT, szExt, 0, 0, &hKey ) == ERROR_SUCCESS) {
			dwSize = 200;
			ok = 1;
			if (RegQueryValueEx(hKey, "", NULL, NULL,(unsigned char*) sDesc, &dwSize) != ERROR_SUCCESS) ok = 0;
			RegCloseKey(hKey);
			if (ok && !stricmp((char *)sDesc, szReg)) has_asso = 1;
			else need_asso = 1;
		} else need_asso = 1;
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

	strcpy((char *) szApp, GetApp()->szAppPath);
	strcpy((char *) szIco, (const char *) szApp);
	strcat((char *) szIco, "Osmo4.ico");
	strcat((char *) szApp, "Osmo4.exe \"%L\"");

	if (m_DoAssociate.GetCheck()) {
		Bool go = 1;
		sKey = cur_ext;

		HKEY hKey;
		DWORD dwSize;
		while (go) {
			Bool ok;
			char szExt[50], szReg[60], szOld[80], szPath[1024], c;
			char *tmp = strchr(sKey, ' ');
			if (!tmp) { go = 0;}
			else { c = tmp[0]; tmp[0] = 0; }
			sprintf(szExt, ".%s", sKey);
			sprintf(szReg, "GPAC\\%s", sKey);
			if (tmp) { tmp[0] = c; tmp += 1; }

			RegOpenKeyEx(HKEY_CLASSES_ROOT, szExt, 0, 0, &hKey );
			dwSize = 200;
			ok = 1;
			if (RegQueryValueEx(hKey, "", NULL, NULL,(unsigned char*) sDesc, &dwSize) != ERROR_SUCCESS) ok = 0;
			RegCloseKey(hKey);
			strcpy(szOld, "");
			if (ok && stricmp((char *)sDesc, szReg)) strcpy(szOld, sDesc);

			strcpy(szPath, szReg); strcat(szPath, "\\DefaultIcon");
			RegCreateKeyEx(HKEY_CLASSES_ROOT, szPath, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &dwSize);
			RegSetValueEx(hKey, "", 0, REG_SZ, szIco, strlen((const char *) szIco)+1);
			RegCloseKey(hKey);

			strcpy(szPath, szReg); strcat(szPath, "\\Shell\\open\\command");
			RegCreateKeyEx(HKEY_CLASSES_ROOT, szPath, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &dwSize);
			RegSetValueEx(hKey, "", 0, REG_SZ, szApp, strlen((const char *) szApp)+1);
			RegCloseKey(hKey);

			if (strlen(szOld)) {
				strcpy(szPath, szReg); strcat(szPath, "\\Backup");
				RegCreateKeyEx(HKEY_CLASSES_ROOT, szPath, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &dwSize);
				RegSetValueEx(hKey, "", 0, REG_SZ, (unsigned char *) szOld, strlen((const char *) szIco)+1);
				RegCloseKey(hKey);
			}

			RegCreateKeyEx(HKEY_CLASSES_ROOT, szExt, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &dwSize);
			RegSetValueEx(hKey, "", 0, REG_SZ, (const unsigned char *) szReg, strlen(szReg)+1);
			RegCloseKey(hKey);

			sKey = tmp;
		}
	} else {
		Bool go = 1;
		sKey = cur_ext;

		HKEY hKey;
		DWORD dwSize;
		while (go) {
			Bool ok;
			char szExt[50], szReg[60], szPath[1024], c;
			char *tmp = strchr(sKey, ' ');
			if (!tmp) { go = 0;}
			else { c = tmp[0]; tmp[0] = 0; }
			sprintf(szExt, ".%s", sKey);
			sprintf(szReg, "GPAC\\%s", sKey);
			if (tmp) { tmp[0] = c; tmp += 1; }

			strcpy(szPath, szReg); strcat(szPath, "\\Backup");
			RegOpenKeyEx(HKEY_CLASSES_ROOT, szPath, 0, 0, &hKey );
			dwSize = 200;
			ok = 1;
			if (RegQueryValueEx(hKey, "", NULL, NULL,(unsigned char*) sDesc, &dwSize) != ERROR_SUCCESS) ok = 0;
			RegCloseKey(hKey);
			if (ok && strlen((char *)sDesc)) {
				RegCreateKeyEx(HKEY_CLASSES_ROOT, szExt, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &dwSize);
				RegSetValueEx(hKey, "", 0, REG_SZ, (unsigned char*) sDesc, strlen((const char *) sDesc)+1);
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
