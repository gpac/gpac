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

#include <eiktxlbx.h>
#include <eiktxlbm.h>


#include <gpac/utf.h>

// INCLUDE FILES
#include "osmo4_ui.h"
#include "playlist.h"

#ifdef USE_SKIN
#include <aknlists.h>
#include <akntabgrp.h>
#include <AknsDrawUtils.h>// skin
#include <AknsBasicBackgroundControlContext.h> //skin 
#endif


CPlaylist::CPlaylist()
{
	playlist_mode = 0;
	view_all_files = 0;
}
CPlaylist::~CPlaylist()
{
	delete iListBox;
	delete iBackGround;
}

CPlaylist* CPlaylist::NewL( const TRect& aRect, GF_User *user)
{
	CPlaylist* self = CPlaylist::NewLC( aRect, user);
	CleanupStack::Pop( self );
	return self;
}
CPlaylist* CPlaylist::NewLC( const TRect& aRect, GF_User *user)
{
	CPlaylist* self = new ( ELeave ) CPlaylist;
	CleanupStack::PushL( self );
	self->ConstructL( aRect, user);
	return self;
}

void CPlaylist::ConstructL(const TRect& aRect, GF_User *user)
{
	CreateWindowL();

#ifdef USE_SKIN
	iListBox = new (ELeave) CAknSingleStyleListBox();
#else
	iListBox = new (ELeave) CEikTextListBox();
#endif
	iListBox->ConstructL(this);
	iListBox->SetContainerWindowL(*this);
	iListBox->SetListBoxObserver(this);

	CDesCArray* textArray = new (ELeave) CDesCArrayFlat(16);
	iListBox->Model()->SetItemTextArray( textArray );
	iListBox->Model()->SetOwnershipType( ELbmOwnsItemArray );

	// Creates scrollbar.
	iListBox->CreateScrollBarFrameL( ETrue );
	iListBox->ScrollBarFrame()->SetScrollBarVisibilityL(CEikScrollBarFrame::EAuto, CEikScrollBarFrame::EAuto);
	//iListBox->ActivateL();

	iListBox->SetFocus(ETrue);

	SetRect(aRect);
	ActivateL();
	MakeVisible(EFalse);

	strcpy(szCurrentDir, "");

#ifndef GPAC_GUI_ONLY
	m_user = user;

	strcpy(ext_list, "");
	u32 count = gf_cfg_get_key_count(user->config, "MimeTypes");
	for (u32 i=0; i<count; i++) {
		char szKeyList[1000], *sKey;
		const char *sMime = gf_cfg_get_key_name(user->config, "MimeTypes", i);
		const char *opt = gf_cfg_get_key(user->config, "MimeTypes", sMime);
		strcpy(szKeyList, opt+1);
		sKey = strrchr(szKeyList, '\"');
		if (!sKey) continue;
		sKey[0] = 0;
		strcat(ext_list, szKeyList);
		strcat(ext_list, " ");
	}

	const char *opt = gf_cfg_get_key(m_user->config, "General", "LastWorkingDir");
	if (opt) strcpy(szCurrentDir, opt);
#endif

}

void CPlaylist::SizeChanged()
{
	iListBox->SetRect( Rect() );
}

void CPlaylist::Draw(const TRect& aRect) const
{
#ifdef USE_SKIN
	CWindowGc& gc = SystemGc();
	MAknsSkinInstance* skin = AknsUtils::SkinInstance();
	MAknsControlContext* cc = AknsDrawUtils::ControlContext( this );
	AknsDrawUtils::Background( skin, cc, this, gc, aRect );
#endif

}

#ifdef USE_SKIN
TTypeUid::Ptr CPlaylist::MopSupplyObject(TTypeUid aId)
{
	if(aId.iUid == MAknsControlContext::ETypeId && iBackGround) {
		return MAknsControlContext::SupplyMopObject( aId, iBackGround);
	}
	return CCoeControl::MopSupplyObject( aId );
}
#endif


TInt CPlaylist::CountComponentControls() const
{
	return 1;
}
CCoeControl* CPlaylist::ComponentControl(TInt aIndex) const
{
	switch (aIndex) {
	case 0:
		return iListBox;
	default:
		return NULL;
	}
}

TKeyResponse CPlaylist::OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType)
{
	if (aType != EEventKey) return iListBox->OfferKeyEventL(aKeyEvent, aType);

	switch (aKeyEvent.iScanCode) {
	case EStdKeyEnter:
		HandleSelection();
		return EKeyWasConsumed;
	default:
		return iListBox->OfferKeyEventL(aKeyEvent, aType);
	}
}
void CPlaylist::HandleListBoxEventL(CEikListBox* aListBox, TListBoxEvent aEventType )
{
	if (aEventType == MEikListBoxObserver::EEventItemClicked ||
	        aEventType == MEikListBoxObserver::EEventEnterKeyPressed)

		HandleSelection();
}

void CPlaylist::ShowHide(Bool show)
{
	if (show) {
		RefreshPlaylist();
		MakeVisible(ETrue);
		DrawNow();
	} else {
		/*cleanup*/
		ResetView();
		MakeVisible(EFalse);
		((COsmo4AppUi *) CEikonEnv::Static()->AppUi())->SetTitle(NULL, 0);
	}
}


void CPlaylist::FlushItemList()
{
	iListBox->HandleItemAdditionL();
	iListBox->SetCurrentItemIndexAndDraw(0);
}

void CPlaylist::ResetView()
{
	CDesCArray* array = static_cast<CDesCArray*>(iListBox->Model()->ItemTextArray());
	array->Reset();
	iListBox->Reset();
}

void CPlaylist::AddItem(const char *name, int is_directory)
{
	TBuf<100> tmp;
	char szName[100];
	CDesCArray* array = static_cast<CDesCArray*>(iListBox->Model()->ItemTextArray());

	if (is_directory) {
#ifdef USE_SKIN
		sprintf(szName, "\t+ %s\t\t", name);
#else
		sprintf(szName, "+ %s", name);
#endif
	} else {
#ifdef USE_SKIN
		sprintf(szName, "\t%s\t\t", name);
#else
		strcpy(szName, name);
#endif
	}
	tmp.SetLength(strlen(szName)+1);
	tmp.Copy( TPtrC8(( TText8* ) szName) );
	tmp.ZeroTerminate();
	array->AppendL(tmp);
}

static Bool enum_dirs(void *cbk, char *name, char *path, GF_FileEnumInfo *file_info)
{
	CPlaylist *of = (CPlaylist *)cbk;
	of->AddItem(name, 1);
	return 0;
}

static Bool enum_files(void *cbk, char *name, char *path, GF_FileEnumInfo *file_info)
{
	CPlaylist *of = (CPlaylist *)cbk;
	of->AddItem(name, 0);
	return 0;
}


void CPlaylist::ScanDirectory(const char *dir)
{
	ResetView();

	if (!dir || !strlen(dir)) {
		RFs iFs;
		TDriveList aList;
		iFs.Connect();
		iFs.DriveList(aList);
		for (TInt i=0; i<KMaxDrives; i++) {
			if (aList[i]) {
				TChar aDrive;
				iFs.DriveToChar(i, aDrive);
				sprintf(szCurrentDir, "%c:", (TUint)aDrive);
				AddItem(szCurrentDir, 0);
			}
		}
		iFs.Close();
		FlushItemList();
		strcpy(szCurrentDir, "");
		return;
	} else {
		strcpy(szCurrentDir, dir);
		AddItem("..", 1);
	}

#ifndef GPAC_GUI_ONLY
	gf_enum_directory((const char *) szCurrentDir, 1, enum_dirs, this, NULL);
	gf_enum_directory((char *) szCurrentDir, 0, enum_files, this, view_all_files ? NULL : ext_list);
#endif
	FlushItemList();

	((COsmo4AppUi *) CEikonEnv::Static()->AppUi())->SetTitle(szCurrentDir, 0);

}

void CPlaylist::GetSelectionName(char *szName)
{
	CDesCArray* array = static_cast<CDesCArray*>(iListBox->Model()->ItemTextArray());
	TInt idx = iListBox->CurrentItemIndex();

#ifndef GPAC_GUI_ONLY

#if defined(_UNICODE)
	size_t len;
	/*handle terminating zero !!*/
	u16 szNameUTF16[100];
	len = (*array)[idx].Size();
	memcpy(szNameUTF16, (*array)[idx].Ptr(), sizeof(u8)*len);
	szNameUTF16[len/2] = 0;
	const u16 *sptr = szNameUTF16;

	/*skip initial '\t'*/
#ifdef USE_SKIN
	sptr += 1;
#endif

	len = gf_utf8_wcstombs(szName, 512, &sptr);
	szName[len] = 0;


#else

	char *src = (*array)[idx]).Ptr();
	/*skip initial '\t'*/
#ifdef USE_SKIN
	src += 1;
#endif
	strcpy(szName, (const char *) src) ;
#endif

	/*remove trailing "\t\t"*/
#ifdef USE_SKIN
	len = strlen(szName);
	szName[len-2] = 0;
#endif

#else
	szName[0] = 0;
#endif

}

void CPlaylist::HandleSelection()
{
	char szName[100];
	GetSelectionName(szName);

	/*sub-directory*/
	if ((szName[0] == '+') && (szName[1] == ' ')) {
		/*browse up*/
		if ((szName[2] == '.') && (szName[3] == '.')) {
			char *prev = strrchr(szCurrentDir, '\\');
			if (prev) {
				prev[0] = 0;
				ScanDirectory(szCurrentDir);
			} else {
				ScanDirectory(NULL);
			}
		} else {
			strcat(szCurrentDir, "\\");
			strcat(szCurrentDir, szName+2);
			ScanDirectory(szCurrentDir);
		}
	} else if (szName[1] == ':') {
		ScanDirectory(szName);
	} else {
		char szURL[1024];
		COsmo4AppUi *app = (COsmo4AppUi *) CEikonEnv::Static()->AppUi();
		if (playlist_mode) {
			TInt idx = iListBox->CurrentItemIndex();
#ifndef GPAC_GUI_ONLY
			const char *url = gf_cfg_get_key_name(m_user->config, "Playlist", idx);
			if (url) app->PlayURL(url);
#endif
		} else {
			gf_cfg_set_key(m_user->config, "General", "LastWorkingDir", (const char *) szCurrentDir);
			sprintf(szURL, "%s\\%s", szCurrentDir, szName);
			app->PlayURL(szURL);
		}
	}
}

Bool CPlaylist::SelectionIsFile()
{
	char szName[100];
	GetSelectionName(szName);
	if ((szName[0] == '+') && (szName[1] == ' ')) return 0;
	else if (szName[1] == ':') return 0;
	return 1;
}

Bool CPlaylist::IsInPlaylist()
{
	char szURL[1024];
	char szName[100];
	GetSelectionName(szName);
	if ((szName[0] == '+') && (szName[1] == ' ')) return 0;
	else if (szName[1] == ':') return 0;

	/*remove from playlist*/
	sprintf(szURL, "%s\\%s", szCurrentDir, szName);
#ifndef GPAC_GUI_ONLY
	const char *opt = gf_cfg_get_key(m_user->config, "Playlist", szURL);
	if (opt) return 1;
#endif
	return 0;
}

static Bool dir_add_files(void *cbk, char *name, char *path, GF_FileEnumInfo *file_info)
{
	CPlaylist *pl = (CPlaylist *)cbk;

#if 0
	if (!bViewUnknownTypes && extension_list) {
		char *ext = strrchr(name, '.');
		if (!ext || !strstr(extension_list, ext+1)) return 0;
	}
#endif

#ifndef GPAC_GUI_ONLY
	gf_cfg_set_key(pl->m_user->config, "Playlist", path, "");
#endif

	return 0;
}


void CPlaylist::RefreshPlaylist()
{
	if (playlist_mode) {
#ifndef GPAC_GUI_ONLY
		u32 count = gf_cfg_get_key_count(m_user->config, "Playlist");
		ResetView();
		for (u32 i=0; i<count; i++) {
			const char *opt = gf_cfg_get_key_name(m_user->config, "Playlist", i);
			const char *sep = strrchr(opt, '\\');
			if (!sep) sep = strrchr(opt, '/');
			AddItem(sep ? (sep+1) : opt, 0);
		}
		if (!count) AddItem("[empty]", 0);
#endif
		FlushItemList();

		((COsmo4AppUi *) CEikonEnv::Static()->AppUi())->SetTitle("Playlist", 0);
	} else {
		ScanDirectory(szCurrentDir);
	}
}


void CPlaylist::PlaylistAct(Osmo4_PLActions act)
{
	char szURL[1024];
	char szName[100];
	CDesCArray*array;
	TInt idx;
	TInt count;

	if (act==Osmo4PLClear) {
		while (1) {
#ifndef GPAC_GUI_ONLY
			const char *opt = gf_cfg_get_key_name(m_user->config, "Playlist", 0);
			if (!opt) break;
			gf_cfg_set_key(m_user->config, "Playlist", opt, NULL);
#endif
		}
		RefreshPlaylist();
		return;
	} else if (act == Osmo4PLToggleMode) {
		playlist_mode = !playlist_mode;
		RefreshPlaylist();
		return;
	} else if (act == Osmo4PLToggleAllFiles) {
		view_all_files = !view_all_files;
		RefreshPlaylist();
		return;
	} else if (act == Osmo4PLAdd) {
#ifndef GPAC_GUI_ONLY
		GetSelectionName(szName);
		if ((szName[0] == '+') && (szName[1] == ' ')) {
			if ((szName[2] != '.') && (szName[3] != '.')) {
				sprintf(szURL, "%s\\%s", szCurrentDir, szName+2);
				gf_enum_directory(szURL, 0, dir_add_files, this, view_all_files ? NULL : ext_list);
			}
		} else if (szName[1] == ':') {
			gf_enum_directory(szName, 0, dir_add_files, this, view_all_files ? NULL : ext_list);
		} else {
			sprintf(szURL, "%s\\%s", szCurrentDir, szName);
			gf_cfg_set_key(m_user->config, "Playlist", szURL, "");
		}
#endif
		return;
	}

	GetSelectionName(szName);
	if ((szName[0] == '+') && (szName[1] == ' ')) return;
	else if (szName[1] == ':') return;

	switch (act) {
	/*remove from playlist*/
	case Osmo4PLRem:
#ifndef GPAC_GUI_ONLY
		sprintf(szURL, "%s\\%s", szCurrentDir, szName);
		gf_cfg_set_key(m_user->config, "Playlist", szURL, NULL);
#endif
		RefreshPlaylist();
		break;
	/*move up*/
	case Osmo4PLMoveUp:
		array = static_cast<CDesCArray*>(iListBox->Model()->ItemTextArray());
		count = array->Count();
		idx = iListBox->CurrentItemIndex();
		sprintf(szURL, "%s\\%s", szCurrentDir, szName);
#ifndef GPAC_GUI_ONLY
		gf_cfg_set_key(m_user->config, "Playlist", szURL, NULL);
		gf_cfg_insert_key(m_user->config, "Playlist", szURL, "", idx-1);
#endif
		RefreshPlaylist();
		if (idx>1) iListBox->SetCurrentItemIndexAndDraw(idx-1);
		break;
	/*move down*/
	case Osmo4PLMoveDown:
		array = static_cast<CDesCArray*>(iListBox->Model()->ItemTextArray());
		count = array->Count();
		idx = iListBox->CurrentItemIndex();
		sprintf(szURL, "%s\\%s", szCurrentDir, szName);
#ifndef GPAC_GUI_ONLY
		gf_cfg_set_key(m_user->config, "Playlist", szURL, NULL);
		gf_cfg_insert_key(m_user->config, "Playlist", szURL, "", idx+1);
#endif
		RefreshPlaylist();
		if (idx<count-1) iListBox->SetCurrentItemIndexAndDraw(idx+1);
		break;
	default:
		break;
	}
}

