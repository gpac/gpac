/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2006-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / Symbian GUI player
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
 */


// INCLUDE FILES
#include <avkon.hrh>
#include <eikmenup.h>
#include <akntitle.h>

#include <stringloader.h>
#include <osmo4.rsg>
#include <f32file.h>
#include <s32file.h>
#include <bacline.h>
#include <eikbtgps.h>
#include <aknquerydialog.h>


#include "osmo4_ui.h"
#include "osmo4_view.h"
#include "playlist.h"

#include <gpac/utf.h>
#include <gpac/options.h>



// ============================ MEMBER FUNCTIONS ===============================


// -----------------------------------------------------------------------------
// Cosmo4AppUi::ConstructL()
// Symbian 2nd phase constructor can leave.
// -----------------------------------------------------------------------------
//
void COsmo4AppUi::ConstructL()
{
	// Initialise app UI with standard value.
	BaseConstructL(CAknAppUi::EAknEnableSkin);

	/*Create display*/
	iAppView = COsmo4AppView::NewL( ClientRect() );
	AddToStackL(iAppView);

	/*create playlist*/
#ifndef GPAC_GUI_ONLY
	iPlaylist = CPlaylist::NewL( ClientRect(), iAppView->GetUser() );

	iPlaylist->MakeVisible(EFalse);
#endif

	iAppView->MakeVisible(ETrue);
	view_mode = 0;

	m_title = NULL;

	//StatusPane ()->SwitchLayoutL ( R_AVKON_STATUS_PANE_LAYOUT_SMALL );

	nb_keys = 0;
	CaptureKeys(1);



	CCommandLineArguments *args = CCommandLineArguments::NewL();
#ifndef GPAC_GUI_ONLY
	if (args->Count() > 1) {
		TPtrC url = args->Arg(1);
#if defined(_UNICODE)
		char szURL[1024];
		u16 szURLUTF16[1024];
		size_t len;
		len = url.Size();
		memcpy(szURLUTF16, url.Ptr(), sizeof(u8)*len);
		szURLUTF16[len/2] = 0;
		const u16 *sptr = szURLUTF16;
		len = gf_utf8_wcstombs(szURL, 512, &sptr);
		if (len != (size_t) -1) {
			szURL[len] = 0;
			iAppView->Connect((const char *)szURL);
		}
#else
		iAppView->Connect((const char *)url.Ptr());
#endif
	}
#endif
	delete args;
}

// -----------------------------------------------------------------------------
// COsmo4AppUi::COsmo4AppUi()
// C++ default constructor can NOT contain any code, that might leave.
// -----------------------------------------------------------------------------
//
COsmo4AppUi::COsmo4AppUi()
{
	// No implementation required
}

// -----------------------------------------------------------------------------
// COsmo4AppUi::~COsmo4AppUi()
// Destructor.
// -----------------------------------------------------------------------------
//
COsmo4AppUi::~COsmo4AppUi()
{
	CaptureKeys(0);

	switch (view_mode) {
	case 0:
		if (iAppView) RemoveFromStack(iAppView);
		break;
	case 1:
		if (iPlaylist) RemoveFromStack(iPlaylist);
		break;
	}
	if (iAppView) delete iAppView;
	if (iPlaylist) delete iPlaylist;
	if (m_title) gf_free(m_title);
	m_title = NULL;
}


void COsmo4AppUi::CaptureKey(TInt32 code, TInt32 scancode)
{
	RWindowGroup iWG = CCoeEnv::Static()->RootWin();
	if (nb_keys>=MAX_KEY_CAP) return;
	keys[nb_keys].key_cap = iWG.CaptureKey(code, 0, 0);
	keys[nb_keys].key_cap_ud = iWG.CaptureKeyUpAndDowns(scancode, 0, 0);
	nb_keys++;
}
/*
possible meaning for key codes:
EStdKeyYes          -Call
EStdKeyNo           -End
EStdKeyApplication0 -Apps key
EStdKeyDevice0      -Left softkey
EStdKeyDevice1      -Right softkey
EStdKeyDevice2      -Power
EStdKeyDevice3      -Button press
EStdKeyDevice4      -Flip - Open
EStdKeyDevice5      -Flip - Close
EStdKeyDevice6      -Side key

EStdKeyDeviceD		-Jog Dial forward
EStdKeyDeviceE		-Jog Dial back
*/
void COsmo4AppUi::CaptureKeys(int do_capture)
{
	if (do_capture) {
		CaptureKey(EKeyIncVolume, EStdKeyIncVolume);
		CaptureKey(EKeyDecVolume, EStdKeyDecVolume);
	} else {
		RWindowGroup iWG = CCoeEnv::Static()->RootWin();
		for (int i=0; i<nb_keys; i++) {
			iWG.CancelCaptureKey(keys[i].key_cap);
			iWG.CancelCaptureKeyUpAndDowns(keys[i].key_cap_ud);
		}
		nb_keys = 0;
	}
}


void COsmo4AppUi::HandleForegroundEventL(TBool aForeground)
{
	CaptureKeys(aForeground ? 1 : 0);
	CAknAppUi::HandleForegroundEventL(aForeground);
}

// -----------------------------------------------------------------------------
// COsmo4AppUi::HandleCommandL()
// Takes care of command handling.
// -----------------------------------------------------------------------------
//
void COsmo4AppUi::HandleCommandL( TInt aCommand )
{
	GF_Err e;
#ifndef GPAC_GUI_ONLY
	switch( aCommand ) {
	case EAknSoftkeyBack:
		if (view_mode==1) TogglePlaylist();
		break;
	case EEikCmdExit:
	case EAknSoftkeyExit:
		iAppView->Shutdown();
		Exit();
		break;
	/*PLAYLIST commands*/
	case EOsmo4PlayListAdd:
		iPlaylist->PlaylistAct(Osmo4PLAdd);
		break;
	case EOsmo4PlayListRem:
		iPlaylist->PlaylistAct(Osmo4PLRem);
		break;
	case EOsmo4PlayListMoveUp:
		iPlaylist->PlaylistAct(Osmo4PLMoveUp);
		break;
	case EOsmo4PlayListMoveDown:
		iPlaylist->PlaylistAct(Osmo4PLMoveDown);
		break;
	case EOsmo4PlayListClear:
		iPlaylist->PlaylistAct(Osmo4PLClear);
		break;
	case EOsmo4PlayListMode:
		iPlaylist->PlaylistAct(Osmo4PLToggleMode);
		break;
	case EOsmo4PlayListAllFiles:
		iPlaylist->PlaylistAct(Osmo4PLToggleAllFiles);
		break;

	/*FILE menu command*/
	case EOsmo4PlayListView:
		TogglePlaylist();
		break;
	case EOsmo4OpenURL:
		break;
	case EOsmo4Fullscreen:
		break;
	case EOsmo4ViewMaxSize:
	{
		CEikStatusPane* statusPane = StatusPane();
		if (statusPane->IsVisible()) statusPane->MakeVisible(EFalse);
		else statusPane->MakeVisible(ETrue);
	}
	break;
	case EOsmo4AROriginal:
		gf_term_set_option(iAppView->m_term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_KEEP);
		break;
	case EOsmo4ARFillScreen:
		gf_term_set_option(iAppView->m_term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_FILL_SCREEN);
		break;
	case EOsmo4AR4_3:
		gf_term_set_option(iAppView->m_term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_4_3);
		break;
	case EOsmo4AR16_9:
		gf_term_set_option(iAppView->m_term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_16_9);
		break;

	case EOsmo4NavReset:
		gf_term_set_option(iAppView->m_term, GF_OPT_NAVIGATION_TYPE, 0);
		break;
	case EOsmo4NavNone:
		gf_term_set_option(iAppView->m_term, GF_OPT_NAVIGATION, GF_NAVIGATE_NONE);
		break;
	case EOsmo4NavSlide:
		e = gf_term_set_option(iAppView->m_term, GF_OPT_NAVIGATION, GF_NAVIGATE_SLIDE);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Cannot set navigation: %s", gf_error_to_string(e) ));
		}
		break;
	case EOsmo4NavWalk:
		gf_term_set_option(iAppView->m_term, GF_OPT_NAVIGATION, GF_NAVIGATE_WALK);
		break;
	case EOsmo4NavFly:
		gf_term_set_option(iAppView->m_term, GF_OPT_NAVIGATION, GF_NAVIGATE_FLY);
		break;
	case EOsmo4NavExamine:
		gf_term_set_option(iAppView->m_term, GF_OPT_NAVIGATION, GF_NAVIGATE_EXAMINE);
		break;
	case EOsmo4NavHeadlight:
		gf_term_set_option(iAppView->m_term, GF_OPT_HEADLIGHT, !gf_term_get_option(iAppView->m_term, GF_OPT_HEADLIGHT) );
		break;
	case EOsmo4CollideNone:
		gf_term_set_option(iAppView->m_term, GF_OPT_COLLISION, GF_COLLISION_NONE);
		break;
	case EOsmo4CollideSimple:
		gf_term_set_option(iAppView->m_term, GF_OPT_COLLISION, GF_COLLISION_NORMAL);
		break;
	case EOsmo4CollideDisp:
		gf_term_set_option(iAppView->m_term, GF_OPT_COLLISION, GF_COLLISION_DISPLACEMENT);
		break;
	case EOsmo4NavGravity:
		gf_term_set_option(iAppView->m_term, GF_OPT_GRAVITY, !gf_term_get_option(iAppView->m_term, GF_OPT_GRAVITY));
		break;
	case EOsmo4ViewRTI:
		iAppView->show_rti = !iAppView->show_rti;
		break;

	case EOsmo4OptEnableLogs:
	{
		const char *opt = gf_cfg_get_key(iAppView->m_user.config, "General", "Logs");
		if (opt && !stricmp(opt, "@debug")) {
			gf_cfg_set_key(iAppView->m_user.config, "General", "Logs", "all@error");
		} else {
			gf_cfg_set_key(iAppView->m_user.config, "General", "Logs", "all@debug");
		}
		iAppView->SetupLogs();
	}
	break;
	case EOsmo4OptOpenGL:
	{
		const char *opt = gf_cfg_get_key(iAppView->m_user.config, "Compositor", "ForceOpenGL");
		Bool use_gl = (opt && !strcmp(opt, "yes")) ? 1 : 0;
		gf_cfg_set_key(iAppView->m_user.config, "Compositor", "ForceOpenGL", use_gl ? "no" : "yes");
		gf_term_set_option(iAppView->m_term, GF_OPT_USE_OPENGL, !use_gl);
	}
	break;
	case EOsmo4OptDirectDraw:
	{
		const char *opt = gf_cfg_get_key(iAppView->m_user.config, "Compositor", "DirectDraw");
		Bool use_dd = (opt && !strcmp(opt, "yes")) ? 1 : 0;
		gf_cfg_set_key(iAppView->m_user.config, "Compositor", "DirectDraw", use_dd ? "no" : "yes");
		gf_term_set_option(iAppView->m_term, GF_OPT_DIRECT_DRAW, !use_dd);
	}
	break;
	case EOsmo4OptXMLProgressive:
	{
		const char *opt = gf_cfg_get_key(iAppView->m_user.config, "SAXLoader", "Progressive");
		Bool use_prog = (opt && !strcmp(opt, "yes")) ? 1 : 0;
		gf_cfg_set_key(iAppView->m_user.config, "SAXLoader", "Progressive", use_prog ? "no" : "yes");
		gf_cfg_set_key(iAppView->m_user.config, "SAXLoader", "MaxDuration", "100");
	}
	break;

	default:
		if ((aCommand>=EOsmo4OpenRecentFirst) && (aCommand<=EOsmo4OpenRecentLast)) {
			const char *sOpt = gf_cfg_get_key_name(iAppView->m_user.config, "RecentFiles", aCommand - EOsmo4OpenRecentFirst);
			if (sOpt) iAppView->Connect(sOpt);
		} else {
			iAppView->MessageBox("Unandled command - panic", "Osmo4");
			Panic( EOsmo4Ui );
		}
		break;
	}
#endif
}


// -----------------------------------------------------------------------------
//  Called by the framework when the application status pane
//  size is changed.  Passes the new client rectangle to the
//  AppView
// -----------------------------------------------------------------------------
//
void COsmo4AppUi::HandleStatusPaneSizeChange()
{
	iAppView->SetRect( ClientRect() );
#ifndef GPAC_GUI_ONLY
	iPlaylist->SetRect( ClientRect() );
#endif
}

void COsmo4AppUi::TogglePlaylist()
{
	CEikButtonGroupContainer* cba= CEikButtonGroupContainer::Current();

#ifndef GPAC_GUI_ONLY
	switch (view_mode) {
	case 0:
		RemoveFromStack(iAppView);
		iAppView->ShowHide(0);
		AddToStackL(iPlaylist);
		if (cba) {
			cba->SetCommandSetL(R_AVKON_SOFTKEYS_OPTIONS_BACK);
			cba->DrawDeferred();
		}
		view_was_max = StatusPane()->IsVisible() ? 0 : 1;
		if (view_was_max) StatusPane()->MakeVisible(ETrue);
		iPlaylist->ShowHide(1);
		view_mode = 1;
		break;
	case 1:
		RemoveFromStack(iPlaylist);
		iPlaylist->ShowHide(0);
		AddToStackL(iAppView);
		if (cba) {
			cba->SetCommandSetL(R_AVKON_SOFTKEYS_OPTIONS_EXIT);
			cba->DrawDeferred();
		}
		iAppView->ShowHide(1);
		if (view_was_max) StatusPane()->MakeVisible(EFalse);
		view_was_max = 0;
		view_mode = 0;
		break;
	}
#endif
}

void COsmo4AppUi::PlayURL(const char *url)
{
	if (view_mode) {
		TogglePlaylist();
	}
	if (url) {
		char *sep;
		iAppView->Connect(url);
		sep = strrchr(url, '\\');
		SetTitle(sep ? sep+1 : url);
	}
}

void COsmo4AppUi::SetTitleInfo(const char *title)
{
#if 1
	CEikStatusPane* statusPane = StatusPane();
	CAknTitlePane *iTitlePane = (CAknTitlePane*) statusPane->ControlL(TUid::Uid(EEikStatusPaneUidTitle));

	if (!title) title = "Osmo4";

	HBufC *htitle = HBufC::NewL( strlen(title)+1);
	htitle->Des().Copy( TPtrC8(( TText8* ) title) );
	iTitlePane->SetText(htitle);
#endif
}

void COsmo4AppUi::SetTitle(const char *title, int store_it)
{
	if (store_it) {
		if (m_title) gf_free(m_title);
		m_title = NULL;
		if (title) m_title = gf_strdup(title);
	}
	SetTitleInfo(title ? title : m_title);
}

void COsmo4AppUi::SetInfo(const char *info)
{
	if (view_mode) return;
	if (info) {
		char szTitle[200];
		sprintf(szTitle, "%s\n%s", info, m_title ? m_title : "Osmo4");
		SetTitleInfo(szTitle);
	} else {
		SetTitleInfo(m_title);
	}
}


#define DECLARE_MENU_ITEM(__text, __com, __check, __res, has_sep)	\
		item.iText = __text;	\
		item.iCommandId = __com;	\
		item.iFlags = has_sep ? EEikMenuItemSeparatorAfter : 0;	\
		if (__check) item.iFlags |= EEikMenuItemCheckBox | EEikMenuItemSymbolOn; \
		item.iCascadeId = __res;	\
		aMenuPane->AddMenuItemL(item);


void COsmo4AppUi::DynInitMenuPaneL(TInt aResourceId, CEikMenuPane* aMenuPane)
{
	CEikMenuPaneItem::SData item;

	if (aResourceId==R_OSMO4_MENU) {

		aMenuPane->Reset();

		if (view_mode==1) {
#ifndef GPAC_GUI_ONLY
			Bool is_file = iPlaylist->SelectionIsFile();
			Bool in_pl = (is_file && iPlaylist->IsInPlaylist()) ? 1 : 0;

			if (iPlaylist->PlaylistMode()) {
				DECLARE_MENU_ITEM(_L("Up"), EOsmo4PlayListMoveUp, 0, 0, 0);
				DECLARE_MENU_ITEM(_L("Down"), EOsmo4PlayListMoveDown, 0, 0, 0);
				DECLARE_MENU_ITEM(_L("Remove"), EOsmo4PlayListRem, 0, 0, 0);
				DECLARE_MENU_ITEM(_L("Clear"), EOsmo4PlayListClear, 0, 0, 1);
			} else if (!in_pl) {
				DECLARE_MENU_ITEM(_L("Add to PlayList"), EOsmo4PlayListAdd, 0, 0, 1);
			} else if (is_file) {
				DECLARE_MENU_ITEM(_L("Remove from Playlist"), EOsmo4PlayListRem, 0, 0, 1);
			}

			if (! iPlaylist->PlaylistMode()) {
				DECLARE_MENU_ITEM(iPlaylist->ViewAllFiles() ? _L("View known files") : _L("View all files"), EOsmo4PlayListAllFiles, 0, 0, 1);
			} else {
				DECLARE_MENU_ITEM(_L("Sort"), 0, 0, R_OSMO4_SM1, 1);
			}
			DECLARE_MENU_ITEM(iPlaylist->PlaylistMode() ? _L("Browse") : _L("Playlist"), EOsmo4PlayListMode, 0, 0, 0);
#endif
		} else {
			/*open*/
			DECLARE_MENU_ITEM(_L("File"), 0, 0, R_OSMO4_SM1, 0);
			DECLARE_MENU_ITEM(_L("View"), 0, 0, R_OSMO4_SM2, 0);
			DECLARE_MENU_ITEM(_L("Options"), 0, 0, R_OSMO4_SM3, 0);
			//DECLARE_MENU_ITEM(_L("Exit"), EEikCmdExit, 0, 0, 0);
		}
		smenu_id = 0;
		return;
	}
	else if (aResourceId==R_OSMO4_SM1) {
		aMenuPane->Reset();
		/*sort menu*/
		if (view_mode==1) {
		}
		/*file menu*/
		else {
			DECLARE_MENU_ITEM(_L("Open local"), EOsmo4PlayListView, 0, 0, 0);
			DECLARE_MENU_ITEM(_L("Open URL"), EOsmo4OpenURL, 0, 0, 1);
#ifndef GPAC_GUI_ONLY
			if (gf_cfg_get_key_name(iAppView->m_user.config, "RecentFiles", 0) != NULL) {
				DECLARE_MENU_ITEM(_L("Recent"), 0, 0, R_OSMO4_SSM1, 0);
			}
#endif
		}
		smenu_id = 1;
		return;
	}
	/*not used*/
	if (view_mode==1) return;

	/*View menu*/
	if (aResourceId==R_OSMO4_SM2) {
		aMenuPane->Reset();
#ifndef GPAC_GUI_ONLY
		/*content view menu*/
		if (gf_term_get_option(iAppView->m_term, GF_OPT_NAVIGATION_TYPE) != GF_NAVIGATE_TYPE_NONE) {
			DECLARE_MENU_ITEM(_L("Navigate"), 0, 0, R_OSMO4_SSM1, 1);
		}
#endif
		DECLARE_MENU_ITEM(_L("Fullscreen"), EOsmo4Fullscreen, 0, 0, 0);
		/*don't allow content AR modification by user*/
		//DECLARE_MENU_ITEM(_L("Aspect Ratio"), 0, 0, R_OSMO4_SSM2, 1);
		DECLARE_MENU_ITEM(_L("Maximize size"), EOsmo4ViewMaxSize, (StatusPane()->IsVisible() ? 0 : 1), 0, 1);
		DECLARE_MENU_ITEM(_L("CPU Usage"), EOsmo4ViewRTI, iAppView->show_rti, 0, 0);
		smenu_id = 2;
		return;
	}
	/*Option menu*/
	if (aResourceId==R_OSMO4_SM3) {
#ifndef GPAC_GUI_ONLY
		const char *opt = gf_cfg_get_key(iAppView->m_user.config, "Compositor", "ForceOpenGL");
		DECLARE_MENU_ITEM(_L("Use 2D OpenGL"), EOsmo4OptOpenGL, (opt && !strcmp(opt, "yes")) ? 1 : 0, 0, 0);
		opt = gf_cfg_get_key(iAppView->m_user.config, "Compositor", "DirectDraw");
		DECLARE_MENU_ITEM(_L("Direct Draw"), EOsmo4OptDirectDraw, (opt && !strcmp(opt, "yes")) ? 1 : 0, 0, 0);
		opt = gf_cfg_get_key(iAppView->m_user.config, "SAXLoader", "Progressive");
		DECLARE_MENU_ITEM(_L("Progressive XML"), EOsmo4OptXMLProgressive, (opt && !strcmp(opt, "yes")) ? 1 : 0, 0, 0);

#endif

		DECLARE_MENU_ITEM(_L("Enable Logs"), EOsmo4OptEnableLogs, iAppView->do_log, 0, 0);
		return;
	}

	if (aResourceId==R_OSMO4_SSM1) {
		aMenuPane->Reset();
		if (smenu_id == 1) {
			u32 i = 0;
#ifndef GPAC_GUI_ONLY
			while (1) {
				const char *opt = gf_cfg_get_key_name(iAppView->m_user.config, "RecentFiles", i);
				if (!opt) break;
				const char *sep = strrchr(opt, '\\');
				if (!sep) sep = strrchr(opt, '/');
				if (!sep) sep = opt;
				else sep += 1;
				item.iText.Copy( TPtrC8(( TText8* ) sep)  );
				item.iCommandId = EOsmo4OpenRecentFirst + i;
				item.iFlags = 0;
				item.iCascadeId = 0;
				aMenuPane->AddMenuItemL(item);
				i++;
				if (i>=10) break;
			}
			if (!i) {
				DECLARE_MENU_ITEM(_L("_"), 0, 0, 0, 0);
			}
#endif
		} else if (smenu_id == 2) {
			DECLARE_MENU_ITEM(_L("Reset"), EOsmo4NavReset, 0, 0, 1);
			DECLARE_MENU_ITEM(_L("None"), EOsmo4NavNone, 0, 0, 0);
			DECLARE_MENU_ITEM(_L("Slide"), EOsmo4NavSlide, 0, 0, 0);

#ifndef GPAC_GUI_ONLY
			if (gf_term_get_option(iAppView->m_term, GF_OPT_NAVIGATION_TYPE) == GF_NAVIGATE_TYPE_3D) {
				DECLARE_MENU_ITEM(_L("Walk"), EOsmo4NavWalk, 0, 0, 0);
				DECLARE_MENU_ITEM(_L("Fly"), EOsmo4NavFly, 0, 0, 0);
				DECLARE_MENU_ITEM(_L("Examine"), EOsmo4NavExamine, 0, 0, 1);
				DECLARE_MENU_ITEM(_L("Headlight"), EOsmo4NavHeadlight, 0, 0, 0);
				DECLARE_MENU_ITEM(_L("Gravity"), EOsmo4NavGravity, 0, 0, 0);
			}
#endif
		}
		return;
	}

	if (aResourceId==R_OSMO4_SSM2) {
		aMenuPane->Reset();
		DECLARE_MENU_ITEM(_L("Keep Original"), EOsmo4AROriginal, 0, 0, 0);
		DECLARE_MENU_ITEM(_L("Fill Screen"), EOsmo4ARFillScreen, 0, 0, 0);
		DECLARE_MENU_ITEM(_L("Ratio 4-3"), EOsmo4AR4_3, 0, 0, 0);
		DECLARE_MENU_ITEM(_L("Ratio 16-9"), EOsmo4AR16_9, 0, 0, 0);
		return;
	}
}

