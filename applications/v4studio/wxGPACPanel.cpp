#include "safe_include.h" 
#include <gpac/terminal.h>
#include <gpac/options.h>

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
  #include "wx/wx.h"
#endif

#include "wxGPACPanel.h"
#include "V4StudioFrame.h"
#include "V4StudioApp.h"
#include "V4Service.h"


bool GPACInit(void *application, GF_Terminal **term, GF_User *user, bool quiet);

wxGPACPanel::wxGPACPanel(V4SceneManager *parent, const char *path) 
{
	this->parent = parent;
	m_term = NULL;
	m_pService = NULL;

	// Initializes the MPEG-4 terminal
	GPACInit(this, &m_term, &m_user, true);			
	if (!m_term) return;

	// Setting all the variables needed for picking of objects.
	m_iDragging = 0;
	m_transformMode = 0;
	dragX = dragY = 0;
	picked = NULL;

	if (path) {
		m_pService = new V4Service(path);
		gf_term_attach_service(m_term, m_pService->GetServiceInterface());
	}
}

wxGPACPanel::~wxGPACPanel()
{
	if (m_pService) delete m_pService;
	if (m_term) gf_term_del(m_term);
	if (m_user.modules) gf_modules_del(m_user.modules);
	if (m_user.config) gf_cfg_del(m_user.config);
}


Bool V4S_EventProc(void *par, GF_Event *evt)
{
	wxGPACPanel *panel = (wxGPACPanel *)par;
	if (!panel->GetMPEG4Terminal()) return 0;

	switch (evt->type) {
	case GF_EVENT_REFRESH:
		gf_term_set_option(panel->GetMPEG4Terminal(), GF_OPT_REFRESH, 0);
		break;
	case GF_EVENT_SCENE_SIZE:
	case GF_EVENT_SIZE:
		gf_sc_set_size(panel->GetSceneCompositor(), evt->size.width, evt->size.height);
		panel->Update();
		break;
	case GF_EVENT_MOUSEDOWN:
		if (evt->mouse.button==GF_MOUSE_LEFT) {
			panel->picked = gf_sc_pick_node(panel->GetSceneCompositor(), evt->mouse.x, evt->mouse.y);
			panel->m_iDragging ++;
			if (panel->picked) {
				panel->GetV4SceneManager()->GetV4StudioFrame()->GetTreeView()->SetSelectedItem(panel->picked);	
				panel->dragX = evt->mouse.x;
				panel->dragY = evt->mouse.y;
				panel->m_transformMode = 0;
			}
		}
		else if (evt->mouse.button==GF_MOUSE_MIDDLE) {
			panel->m_iDragging ++;
			panel->picked = gf_sc_pick_node(panel->GetSceneCompositor(), evt->mouse.x, evt->mouse.y);
			if (panel->picked) {
				panel->GetV4SceneManager()->GetV4StudioFrame()->GetTreeView()->SetSelectedItem(panel->picked);	
				panel->dragX = evt->mouse.x;
				panel->dragY = evt->mouse.y;
				panel->m_transformMode = 2;
			}
		}
		else if (evt->mouse.button==GF_MOUSE_RIGHT) {
			panel->m_iDragging ++;
			panel->picked = gf_sc_pick_node(panel->GetSceneCompositor(), evt->mouse.x, evt->mouse.y);
			if (panel->picked) {
				panel->GetV4SceneManager()->GetV4StudioFrame()->GetTreeView()->SetSelectedItem(panel->picked);	
				panel->dragX = evt->mouse.x;
				panel->dragY = evt->mouse.y;
				panel->m_transformMode = 1;
			}
		}
		break;
	case GF_EVENT_MOUSEUP:
		if (evt->mouse.button==GF_MOUSE_LEFT) {
			panel->m_iDragging --;
			if (panel->picked) {
				int dX = evt->mouse.x - panel->dragX;
				int dY = evt->mouse.y - panel->dragY;
				panel->dragX = evt->mouse.x;
				panel->dragY = evt->mouse.y;
				wxString pos;
				pos << dX;
				pos << ',';
				pos << dY;
				panel->GetV4SceneManager()->GetV4StudioFrame()->GetTreeView()->Translate(dX,dY);
				panel->GetV4SceneManager()->GetV4StudioFrame()->GetStatusBar()->SetStatusText(pos);
			}
		}
		else if (evt->mouse.button==GF_MOUSE_MIDDLE) {
			panel->m_iDragging --;
			if (panel->picked) {
				int dX = evt->mouse.x - panel->dragX;
				int dY = evt->mouse.y - panel->dragY;
				panel->dragX = evt->mouse.x;
				panel->dragY = evt->mouse.y;
				wxString pos;
				pos << dX;
				pos << ',';
				pos << dY;
				panel->GetV4SceneManager()->GetV4StudioFrame()->GetTreeView()->Rotate(dX,dY);
				panel->GetV4SceneManager()->GetV4StudioFrame()->GetStatusBar()->SetStatusText(pos);
			}
		}
		else if (evt->mouse.button==GF_MOUSE_RIGHT) {
			panel->m_iDragging --;
			if (panel->picked) {
				int dX = evt->mouse.x - panel->dragX;
				int dY = evt->mouse.y - panel->dragY;
				panel->dragX = evt->mouse.x;
				panel->dragY = evt->mouse.y;
				wxString pos;
				pos << dX;
				pos << ',';
				pos << dY;
				panel->GetV4SceneManager()->GetV4StudioFrame()->GetTreeView()->Scale(dX,dY);
				panel->GetV4SceneManager()->GetV4StudioFrame()->GetStatusBar()->SetStatusText(pos);
			}
		}
		break;
	case GF_EVENT_MOUSEMOVE:
	{
		wxString pos;
		if (panel->picked && panel->m_iDragging) {
			int dX = evt->mouse.x - panel->dragX;
			int dY = evt->mouse.y - panel->dragY;
			panel->dragX = evt->mouse.x;
			panel->dragY = evt->mouse.y;
			pos << dX;
			pos << ',';
			pos << dY;
			switch (panel->m_transformMode) {
			case 0:
				panel->GetV4SceneManager()->GetV4StudioFrame()->GetTreeView()->Translate(dX,dY);
				break;
			case 1:
				panel->GetV4SceneManager()->GetV4StudioFrame()->GetTreeView()->Scale(dX,dY);
				break;
			case 2:			
				panel->GetV4SceneManager()->GetV4StudioFrame()->GetTreeView()->Rotate(dX,dY);
				break;
			}
		} else {
			pos << evt->mouse.x;
			pos << ',';
			pos << evt->mouse.y;
		}
		panel->GetV4SceneManager()->GetV4StudioFrame()->GetStatusBar()->SetStatusText(pos);
	}
		break;
	case GF_EVENT_QUIT:
		panel->GetV4SceneManager()->GetV4StudioFrame()->Close(TRUE);
		break;
	}
	return 0;
}

void wxGPACPanel::Update() 
{
	if (m_term) {
		//gf_term_set_option(m_term, GF_OPT_PLAY_STATE, GF_STATE_STEP_PAUSE);
		gf_sc_invalidate(m_term->compositor, NULL);
		gf_sc_draw_frame(m_term->compositor);
	}
}

bool GPACInit(void *application, GF_Terminal **term, GF_User *user, bool quiet) 
{
	memset(user, 0, sizeof(GF_User));

	/*locate exec dir for cfg file*/
    wxPathList pathList;
	wxString currentDir(wxGetCwd());
    wxString abs_gpac_path = "";
	const char *gpac_cfg;

	if (!quiet) ::wxLogMessage("Looking for GPAC configuration file");

#if defined(__WXMAC__) && !defined(__DARWIN__)
    // On Mac, the current directory is the relevant one when the application starts.
    abs_gpac_path = wxGetCwd();
	gpac_cfg = "GPAC.cfg";
#else

#ifdef WIN32
	V4StudioApp &app = wxGetApp();
	gpac_cfg = "GPAC.cfg";
	/*locate exe*/
    if (wxIsAbsolutePath(app.argv[0])) {
        abs_gpac_path = wxPathOnly(app.argv[0]);
	} else {
		if (currentDir.Last() != wxFILE_SEP_PATH) currentDir += wxFILE_SEP_PATH;
		abs_gpac_path = currentDir + app.argv[0];
		if (wxFileExists(abs_gpac_path)) {
			abs_gpac_path = wxPathOnly(abs_gpac_path);
		} else {
			abs_gpac_path = "";
			pathList.AddEnvList(wxT("PATH"));
			abs_gpac_path = pathList.FindAbsoluteValidPath(app.argv[0]);
			if (!abs_gpac_path.IsEmpty()) {
				abs_gpac_path = wxPathOnly(abs_gpac_path);
			} else {
				/*ask user*/
				wxDirDialog dlg(NULL, "Locate GPAC config file directory");
				if ( dlg.ShowModal() != wxID_OK ) return 0;
				abs_gpac_path = dlg.GetPath();
			}
		}
	}
#else
	gpac_cfg = ".gpacrc";
	char *cfg_dir = getenv("HOME");
	if (cfg_dir) {
		abs_gpac_path = cfg_dir;
	} else {
		/*ask user*/
		wxDirDialog dlg(NULL, "Locate GPAC config file directory");
		if ( dlg.ShowModal() != wxID_OK ) return 0;
		abs_gpac_path = dlg.GetPath();
	}

#endif

#endif

	/*load config*/
	user->config = gf_cfg_new(abs_gpac_path.c_str(), gpac_cfg);

	if (!user->config) {
		unsigned char config_file[GF_MAX_PATH];
		strcpy((char *) config_file, (const char *) abs_gpac_path.c_str());
		if (config_file[strlen((char *) config_file)-1] != GF_PATH_SEPARATOR) {
		  char szSep[2];
		  szSep[0] = GF_PATH_SEPARATOR;
		  szSep[1] = 0;
		  strcat((char *) config_file, (const char *)szSep);
		}
		strcat((char *) config_file, gpac_cfg);
		FILE *ft = fopen((const char *) config_file, "wt");
		if (!ft) {
			wxMessageDialog(NULL, "Cannot create blank config file", "Init error", wxOK).ShowModal();
			return 0;
		}
		fclose(ft);
		user->config = gf_cfg_new(abs_gpac_path.c_str(), gpac_cfg);
		if (!user->config) {
			wxMessageDialog(NULL, "Cannot open GPAC configuration file", "Init error", wxOK);
			return 0;
		}
	}
	if (!quiet) ::wxLogMessage("GPAC configuration file opened - looking for modules");
	const char *str = gf_cfg_get_key(user->config, "General", "ModulesDirectory");
	Bool first_launch = 0;
	if (!str) {
	  first_launch = 1;
#ifdef GPAC_MODULES_PATH
		str = GPAC_MODULES_PATH;
#else
		str = abs_gpac_path.c_str();
#endif
	}
	user->modules = gf_modules_new(str, user->config);

	/*initial launch*/
	if (first_launch || !gf_modules_get_count(user->modules)) {
		const char *sOpt;
		wxDirDialog dlg(NULL, "Locate GPAC modules directory");
		if  (!gf_modules_get_count(user->modules)) {
		  gf_modules_del(user->modules);
		  user->modules = NULL;
			if ( dlg.ShowModal() != wxID_OK ) return false;
			str = dlg.GetPath().c_str();;
	
			user->modules = gf_modules_new(str, user->config);
			if (!user->modules || !gf_modules_get_count(user->modules) ) {
				wxMessageDialog(NULL, "Cannot find any modules for GPAC", "Init error", wxOK);
				gf_cfg_del(user->config);
				return 0;
			}		
		}

		gf_cfg_set_key(user->config, "General", "ModulesDirectory", (const char *) str);

		/*check audio config on windows, force config*/
		sOpt = gf_cfg_get_key(user->config, "Audio", "ForceConfig");
		if (!sOpt) {
			gf_cfg_set_key(user->config, "Audio", "ForceConfig", "yes");
			gf_cfg_set_key(user->config, "Audio", "NumBuffers", "2");
			gf_cfg_set_key(user->config, "Audio", "TotalDuration", "120");
		}

#ifdef WIN32
		sOpt = gf_cfg_get_key(user->config, "Compositor", "Raster2D");
		if (!sOpt) gf_cfg_set_key(user->config, "Compositor", "Raster2D", "gdip_rend");
		sOpt = gf_cfg_get_key(user->config, "General", "CacheDirectory");
		if (!sOpt) {
			unsigned char str_path[MAX_PATH];
			sprintf((char *) str_path, "%scache", abs_gpac_path.c_str());
			gf_cfg_set_key(user->config, "General", "CacheDirectory", (const char *) str_path);
		}
		/*by default use GDIplus, much faster than freetype on font loading*/
		gf_cfg_set_key(user->config, "FontEngine", "FontReader", "gdip_rend");
		gf_cfg_set_key(user->config, "Video", "DriverName", "DirectX Video Output");
#else
		wxDirDialog dlg3(NULL, "Please specify a cache directory for GPAC");
		dlg3.SetPath("/tmp");
		if ( dlg3.ShowModal() == wxID_OK ) 
			gf_cfg_set_key(user->config, "General", "CacheDirectory", (const char *) dlg3.GetPath().c_str() );

		wxDirDialog dlg2(NULL, "Please locate a TrueType font repository on your system for text support");
		dlg2.SetPath("/usr/share/fonts/truetype");
		if ( dlg2.ShowModal() == wxID_OK ) 
			gf_cfg_set_key(user->config, "FontEngine", "FontDirectory", (const char *) dlg2.GetPath().c_str() );

		gf_cfg_set_key(user->config, "Video", "DriverName", "SDL Video Output");
#endif
	}	
	if (! gf_modules_get_count(user->modules) ) {
		wxMessageDialog(NULL, "No modules available - system cannot work", "Fatal Error", wxOK).ShowModal();
		gf_modules_del(user->modules);
		gf_cfg_del(user->config);
		return 0;
	}

	if (!quiet) ::wxLogMessage("%d modules found:", gf_modules_get_count(user->modules));
	for (u32 i=0; i<gf_modules_get_count(user->modules); i++) {
		if (!quiet) ::wxLogMessage("\t%s", gf_modules_get_file_name(user->modules, i));
	}

	if (!quiet) ::wxLogMessage("Starting GPAC Terminal");
	/*now load terminal*/
	user->opaque = application;
	user->EventProc = V4S_EventProc;
//	user->os_window_handler = ((wxGPACPanel *)application)->GetV4SceneManager()->GetV4StudioFrame()->GetHandle();

	// Forces the renderer to not use a thread and do the rendering by itfe
	gf_cfg_set_key(user->config, "Systems", "NoVisualThread", "No");

	*term = gf_term_new(user);
	if (!*term) {
		wxMessageDialog(NULL, "Fatal Error", "Cannot load GPAC Terminal", wxOK).ShowModal();
		return 0;
	} else {
		if (!quiet) ::wxLogMessage("GPAC Terminal started");
	}

	return 1;
}

