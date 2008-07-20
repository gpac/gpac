/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / Osmo4 wxWidgets GUI
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *   
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 *		
 */

#ifndef _WXOSMO4_H
#define _WXOSMO4_H

/*
we need to force X to work in sync mode when we use embedded view...
include first to avoid Bool type redef between X11 and gpac
*/
#ifdef __WXGTK__
#include <X11/Xlib.h>
#endif

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include <wx/image.h>
#include <wx/listctrl.h>
#include <wx/event.h>
#include "menubtn.h"

/*include gpac AFTER wx in case we override malloc/realloc/free for mem tracking*/
#include <gpac/terminal.h>
#include <gpac/term_info.h>

class wxOsmo4App : public wxApp
{
public:
	virtual bool OnInit();
};

DECLARE_APP(wxOsmo4App)

class wxOsmo4Frame;
class wxPlaylist;

class GPACLogs : public wxLogWindow {
public:
    GPACLogs(wxFrame *parent) : wxLogWindow(parent, wxT("GPAC Logs"), FALSE, FALSE) {
		m_pMain = (wxOsmo4Frame *) parent;
	}
	virtual bool OnFrameClose(wxFrame *frame);

private:
	wxOsmo4Frame *m_pMain;
};

#define MAX_VIEWPOINTS	50

enum {
	// Menu commands
	FILE_OPEN = wxID_HIGHEST+1,
	FILE_OPEN_URL,
	FILE_RELOAD,
	FILE_RELOAD_CONFIG,
	FILE_PLAY,
	FILE_STEP,
	FILE_STOP,
	FILE_PREV,
	FILE_NEXT,
	FILE_PROPERTIES,
	FILE_COPY,
	FILE_PASTE,
	TERM_RELOAD,
	FILE_QUIT,
	VIEW_FULLSCREEN,
	VIEW_ORIGINAL,
	VIEW_AR_KEEP,
	VIEW_AR_FILL,
	VIEW_AR_43,
	VIEW_AR_169,
	VIEW_OPTIONS,
	VIEW_LOGS,
	VIEW_RTI,
	VIEW_PLAYLIST,
	SWITCH_RENDER,
	APP_SHORTCUTS,
	APP_NAV_KEYS,
	APP_ABOUT,
	ID_ADDRESS,
	ID_URL_GO,
	ID_ABOUT_CLOSE,
	ID_CLEAR_NAV,
	ID_STREAM_MENU,
	ID_CHAPTER_MENU,
	ID_ADD_SUB,

	ID_MCACHE_ENABLE,
	ID_MCACHE_STOP,
	ID_MCACHE_ABORT,

	ID_CTRL_TIMER,
	ID_SLIDER,

	ID_TREE_VIEW,
	ID_OD_TIMER,
	ID_VIEW_SG,
	ID_VIEW_WI,
	ID_VIEW_SEL,


	ID_HEADLIGHT,
	ID_NAVIGATE_NONE,
	ID_NAVIGATE_WALK,
	ID_NAVIGATE_FLY,
	ID_NAVIGATE_EXAMINE,
	ID_NAVIGATE_SLIDE,
	ID_NAVIGATE_PAN,
	ID_NAVIGATE_ORBIT,
	ID_NAVIGATE_GAME,
	ID_NAVIGATE_RESET,

	ID_COLLIDE_NONE,
	ID_COLLIDE_REG,
	ID_COLLIDE_DISP,
	ID_GRAVITY,

	ID_PL_OPEN,
	ID_PL_SAVE,
	ID_PL_ADD_FILE,
	ID_PL_ADD_URL,
	ID_PL_ADD_DIR,
	ID_PL_ADD_DIR_REC,
	ID_PL_REM_FILE,
	ID_PL_REM_ALL,
	ID_PL_REM_DEAD,
	ID_PL_UP,
	ID_PL_DOWN,
	ID_PL_RANDOMIZE,
	ID_PL_REVERSE,
	ID_PL_SEL_REV,
	ID_PL_SORT_TITLE,
	ID_PL_SORT_FILE,
	ID_PL_SORT_DUR,
	ID_PL_PLAY,


	/*reserve IDs for viewpoint menu*/
	ID_VIEWPOINT_FIRST,
	ID_VIEWPOINT_LAST = ID_VIEWPOINT_FIRST + MAX_VIEWPOINTS,

	/*reserve IDs for navigation menus*/
	ID_NAV_PREV_0,
	ID_NAV_PREV_9 = ID_NAV_PREV_0 + 10,
	ID_NAV_NEXT_0,
	ID_NAV_NEXT_9 = ID_NAV_NEXT_0 + 10,
	/*reserve IDs for stream selection menus*/
	ID_SELSTREAM_0,
	ID_SELSTREAM_9 = ID_SELSTREAM_0 + 10,

	/*reserve IDs for chapter selection menus*/
	ID_SETCHAP_FIRST,
	ID_SETCHAP_LAST = ID_SELSTREAM_0 + 200,
};

wxString get_pref_browser(GF_Config *cfg);

class wxGPACEvent : public wxEvent
{
public:
	wxGPACEvent( wxWindow* win = (wxWindow*) NULL );
	void CopyObject( wxObject& obj ) const;
	virtual wxEvent *Clone() const;

	wxString to_url;
	GF_Event gpac_evt;

	DECLARE_DYNAMIC_CLASS(wxGPACEvent)
};
typedef void (wxEvtHandler::*GPACEventFunction)(wxGPACEvent&);
DEFINE_EVENT_TYPE(GPAC_EVENT)

#define EVT_GPACEVENT(func) DECLARE_EVENT_TABLE_ENTRY(GPAC_EVENT, -1, -1, (wxObjectEventFunction) (wxEventFunction) (GPACEventFunction) & func, (wxObject*) NULL),

class OpenURLDlg : public wxDialog {
public:
    OpenURLDlg(wxWindow *parent, GF_Config *cfg);
	wxString m_urlVal;
private:
    wxButton *m_go;
    wxComboBox *m_url;
	GF_Config *m_cfg;
	void OnGo(wxCommandEvent& event);
    DECLARE_EVENT_TABLE()
};

class wxMyComboBox : public wxComboBox
{
public:
	wxMyComboBox(wxWindow* parent, wxWindowID id, const wxString& value = wxT(""), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize)
		: wxComboBox(parent, id, value, pos, size, 0, NULL, wxCB_DROPDOWN)
	{}

private:
	DECLARE_EVENT_TABLE()

	void OnKeyUp(wxKeyEvent &event);
};

class wxOsmo4Frame : public wxFrame {
public:
    wxOsmo4Frame();
    virtual ~wxOsmo4Frame();

	char szAppPath[GF_MAX_PATH];

	u32 m_duration;
	wxString the_next_url;
	GF_Terminal *m_term;
	GF_User m_user;
	Bool m_connected, m_can_seek, m_console_off, m_loop, m_lookforsubs;
	
	void DoConnect();

	void ConnectAcknowledged(Bool bOk); 
	void SetStatus(wxString str);

	void OnFilePlay(wxCommandEvent &event);
	void OnFileStep(wxCommandEvent &event);
	void OnFileStop(wxCommandEvent &event);
	wxString GetFileFilter();

	void BuildViewList();
	void BuildStreamList(Bool reset_only);
	void BuildChapterList(Bool reset_only);

	void AddSubtitle(const char *fileName, Bool auto_play);

	wxWindow *m_pView;

#ifdef __WXGTK__
	u32 m_last_grab_time, m_last_grab_pos;
	wxWindow *m_pVisual;
#endif
	wxSlider *m_pProg;	
	wxPlaylist *m_pPlayList;

	void DoLayout(u32 v_width = 0, u32 v_height = 0);
	s32 m_last_prog;

	FILE *m_logs;
	u32 m_log_level, m_log_tools;
	u32 m_LastStatusTime;

protected:

private:
	DECLARE_EVENT_TABLE()

	void OnCloseApp(wxCloseEvent &event);
	void OnSize(wxSizeEvent &event);

	void OnFileOpen(wxCommandEvent &event);
	void OnFileOpenURL(wxCommandEvent &event);
	void OnFileReload(wxCommandEvent &event);
	void OnFileReloadConfig(wxCommandEvent & event);
	void OnFileProperties(wxCommandEvent &event);
	void OnFileQuit(wxCommandEvent &event);
	void OnFullScreen(wxCommandEvent &event);
	void OnOptions(wxCommandEvent &event);
	void OnViewARKeep(wxCommandEvent &event);
	void OnViewARFill(wxCommandEvent &event);
	void OnViewAR169(wxCommandEvent &event);
	void OnViewAR43(wxCommandEvent &event);
	void OnViewOriginal(wxCommandEvent &event);
	void OnPlaylist(wxCommandEvent &event);
	void OnShortcuts(wxCommandEvent &event);
	void OnNavInfo(wxCommandEvent &event);
	void OnAddSub(wxCommandEvent &event);
	void OnAbout(wxCommandEvent &event);
	Bool LoadTerminal();
	void OnGPACEvent(wxGPACEvent &event);
	void OnTimer(wxTimerEvent& event);
	void OnSlide(wxScrollEvent &event);
	void OnRelease(wxScrollEvent &event);
	void OnLogs(wxCommandEvent & event);
	void OnRTI(wxCommandEvent & event);
	void OnUpdatePlay(wxUpdateUIEvent &event);
	void OnUpdateNeedsConnect(wxUpdateUIEvent &event);
	void OnUpdateFullScreen(wxUpdateUIEvent &event);
	void OnUpdateAR(wxUpdateUIEvent &event);
	void OnViewport(wxCommandEvent & event);
	void OnUpdateViewport(wxUpdateUIEvent & event);
	void OnNavigate(wxCommandEvent & event);
	void OnNavigateReset(wxCommandEvent & event);
	void OnUpdateNavigation(wxUpdateUIEvent & event);
	void OnRenderSwitch(wxCommandEvent &event);
	void OnCollide(wxCommandEvent & event);
	void OnUpdateCollide(wxUpdateUIEvent & event);
	void OnHeadlight(wxCommandEvent & event);
	void OnUpdateHeadlight(wxUpdateUIEvent & event);
	void OnGravity(wxCommandEvent & event);
	void OnUpdateGravity(wxUpdateUIEvent & event);
	void OnURLSelect(wxCommandEvent &event);
	void OnUpdatePlayList(wxUpdateUIEvent & event);
	void OnFilePrevOpen(wxNotifyEvent & event);
	void OnFileNextOpen(wxNotifyEvent & event);
	void OnNavPrev(wxCommandEvent &event);
	void OnUpdateNavPrev(wxUpdateUIEvent & event);
	void OnNavPrevMenu(wxCommandEvent &event);
	void OnNavNext(wxCommandEvent &event);
	void OnUpdateNavNext(wxUpdateUIEvent & event);
	void OnNavNextMenu(wxCommandEvent &event);
	void OnClearNav(wxCommandEvent &event);
	void OnStreamSel(wxCommandEvent &event);
	void OnUpdateStreamSel(wxUpdateUIEvent & event);
	void OnUpdateStreamMenu(wxUpdateUIEvent & event);
	void OnChapterSel(wxCommandEvent &event);
	void OnUpdateChapterSel(wxUpdateUIEvent & event);
	void OnUpdateChapterMenu(wxUpdateUIEvent & event);

	void SelectionReady();
	void ReloadURLs();
	void LookForSubtitles();

	void OnCacheEnable(wxCommandEvent &event);
	void OnCacheStop(wxCommandEvent &event);
	void OnCacheAbort(wxCommandEvent &event);
	void OnUpdateCacheEnable(wxUpdateUIEvent & event);
	void OnUpdateCacheAbort(wxUpdateUIEvent & event);

	void OnFileCopy(wxCommandEvent &event);
	void OnUpdateFileCopy(wxUpdateUIEvent &event);
	void OnFilePaste(wxCommandEvent &event);
	void OnUpdateFilePaste(wxUpdateUIEvent &event);


	void CheckVideoOut();

    wxMenuBar* m_pMenubar;
    wxStatusBar* m_pStatusbar;
	wxTimer *m_pTimer;
	GPACLogs *m_pLogs;
	wxBoxSizer *m_pAddBar;

	Bool m_bGrabbed, m_bToReset, m_bFirstStreamListBuild;
	wxBitmap *m_pOpenFile, *m_pPrev, *m_pNext, *m_pPlay, *m_pPause, *m_pStep, *m_pStop, *m_pInfo, *m_pConfig, *m_pSW2D, *m_pSW3D;
	wxMenuButton *m_pPrevBut, *m_pNextBut;
	wxToolBar *m_pToolBar;
	wxMyComboBox *m_Address;
	
	wxMenu *vp_list;
	wxMenu *sel_menu;
	wxMenu *chap_menu;
	void Stop();

	s32 nb_viewpoints;

	void UpdateRenderSwitch();
	void UpdatePlay();

	u32 m_orig_width, m_orig_height;

	u32 m_num_chapters;
	Double *m_chapters_start;
	Bool m_bExternalView, m_bViewRTI, m_bStartupFile;

	void ShowViewWindow(Bool do_show);
};


#endif 

