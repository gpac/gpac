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

#ifndef __osmo4playlist_H__
#define __osmo4playlist_H__

#include <e32base.h>
#include <coecntrl.h>
#include <eiklbo.h>

#define USE_SKIN

#ifdef USE_SKIN
class MAknsControlContext; // for skins support
#endif


#include <gpac/user.h>

class CEikTextListBox; //For list box

enum Osmo4_PLActions
{
	Osmo4PLAdd = 0,
	Osmo4PLRem,
	Osmo4PLClear,
	Osmo4PLMoveUp,
	Osmo4PLMoveDown,
	Osmo4PLToggleMode,
	Osmo4PLToggleAllFiles,
};

class CPlaylist : public CCoeControl,MEikListBoxObserver
{
public:
	static CPlaylist* NewL( const TRect& aRect, GF_User *user);
	static CPlaylist* NewLC( const TRect& aRect, GF_User *user);
	virtual ~CPlaylist();
	void SizeChanged();

	virtual TKeyResponse OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType);

	void HandleListBoxEventL(CEikListBox* aListBox, TListBoxEvent aEventType );

	void AddItem(const char *name, int is_directory);
	TInt CountComponentControls() const;
	CCoeControl* ComponentControl(TInt aIndex) const;
	void Draw(const TRect& aRect) const;

	void ShowHide(Bool show);
	Bool SelectionIsFile();
	Bool IsInPlaylist();
	Bool PlaylistMode() {
		return playlist_mode;
	}
	Bool ViewAllFiles() {
		return view_all_files;
	}

	void PlaylistAct(Osmo4_PLActions act);

#ifndef GPAC_GUI_ONLY
	GF_User *m_user;
#endif

private:
	void ConstructL(const TRect& aRect, GF_User *user);
	CPlaylist();


#ifdef USE_SKIN
	TTypeUid::Ptr MopSupplyObject(TTypeUid aId);
	MAknsControlContext* iBackGround;
#endif


	void ResetView();
	void FlushItemList();
	void ScanDirectory(const char *dir);
	void HandleSelection();
	void GetSelectionName(char *name);
	void RefreshPlaylist();

	char szCurrentDir[1024];
	CEikTextListBox* iListBox;
	Bool playlist_mode;
	Bool view_all_files;
	char ext_list[4096];
};

#endif //__osmo4playlist_H__

