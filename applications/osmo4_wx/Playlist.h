/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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

#ifndef _PLAYLIST_H
#define _PLAYLIST_H

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include "menubtn.h"

enum
{
	ID_FILE_LIST = 1000,
};

class wxOsmo4Frame;

class PLEntry
{
public:
	PLEntry(wxString url);
	~PLEntry();

	char *m_url;
	char *m_disp_name;
	u32 m_duration;

	Bool m_bIsSelected;
	Bool m_bIsDead;
	Bool m_bIsPlaying;
};


class wxPlaylist : public wxFrame
{
public:
	wxPlaylist(wxWindow *parent);
	virtual ~wxPlaylist();

	void Clear();
	void ClearButPlaying();
	void RefreshList();

	void Truncate();
	void QueueURL(wxString filename);
	void Play();
	void PlayNext();
	void PlayPrev();
	void SetDead();
	void SetDuration(u32 duration);
	Bool HasValidEntries();
	void OpenPlaylist(wxString fileName);

	/*for current entry played*/
	wxString GetDisplayName();
	wxString GetURL();

	s32 m_cur_entry;
	GF_List *m_entries;

	wxOsmo4Frame *m_pApp;

private:
	DECLARE_EVENT_TABLE()

	void OnClose(wxCloseEvent &event);
	void OnSize(wxSizeEvent &event);
	void OnAddFile(wxCommandEvent &event);
	void OnAddURL(wxCommandEvent &event);
	void OnAddDir(wxCommandEvent &event);
	void OnAddDirRec(wxCommandEvent &event);
	void OnRemFile(wxCommandEvent &event);
	void OnRemAll(wxCommandEvent &event);
	void OnRemDead(wxCommandEvent &event);
	void OnSelUp(wxCommandEvent &event);
	void OnSelDown(wxCommandEvent &event);
	void OnSave(wxCommandEvent &event);
	void OnOpen(wxCommandEvent &event);
	void OnRightClick(wxListEvent & event);
	void OnReverseSelection(wxCommandEvent &event);
	void OnReverseList(wxCommandEvent &event);
	void OnRandomize(wxCommandEvent &event);
	void OnSortFile(wxCommandEvent &event);
	void OnSortTitle(wxCommandEvent &event);
	void OnSortDuration(wxCommandEvent &event);
	void OnItemActivate(wxListEvent &event);
	void OnPlay(wxCommandEvent &event);


	void Sort(u32 type);
	void UpdateEntry(u32 idx);
	void RefreshCurrent();
	void Save(char *szPath, Bool save_m3u);

	wxBitmap *m_pOpen, *m_pSave, *m_pAdd, *m_pRem, *m_pUp, *m_pDown, *m_pSort;
	wxMenuButton *m_pAddBut, *m_pRemBut, *m_pSortBut;
	wxToolBar *m_pToolBar;
	wxListCtrl *m_FileList;
	char szCacheDir[GF_MAX_PATH];
	s32 m_all_dead_entries;

	void AddDir(Bool do_recurse);
};



#endif

