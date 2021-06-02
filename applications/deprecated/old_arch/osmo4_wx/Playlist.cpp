/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / Osmo4 wxWidgets GUI
 *
 *  GPAC is gf_free software; you can redistribute it and/or modify
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

#include "wxOsmo4.h"
#include "Playlist.h"


#include "playlist.xpm"

PLEntry::PLEntry(wxString url)
{
	m_url = gf_strdup(url.mb_str(wxConvUTF8));
	Bool is_remote = 0;
	wxCharBuffer the_url = (const char *) url.mb_str(wxConvUTF8);
	const char *_url = strstr(the_url, "://");
	if (_url) {
		_url += 3;
		is_remote = 1;
	}
	else _url = (const char *) the_url;

	char *str = (char*)strrchr(_url, '\\');
	if (!str) str = (char*)strrchr(_url, '/');
	if (str && strlen(str+1)) {
		m_disp_name = gf_strdup(str+1);
		str = strrchr(m_disp_name, '.');
		if (str) str[0] = 0;
	} else {
		m_disp_name = gf_strdup(_url);
		if (!is_remote) {
			str = strrchr(m_disp_name, '.');
			if (str) str[0] = 0;
		}
	}
	m_duration = 0;
	m_bIsDead = 0;
	m_bIsPlaying = 0;
	m_bIsSelected = 0;
}

PLEntry::~PLEntry()
{
	if (m_url) gf_free(m_url);
	if (m_disp_name) gf_free(m_disp_name);

}

wxPlaylist::wxPlaylist(wxWindow *parent)
	: wxFrame(parent, -1, wxString(_T("Osmo4 Playlist")), wxDefaultPosition, wxDefaultSize,
	          wxCLOSE_BOX | wxSYSTEM_MENU | wxCAPTION | wxRESIZE_BORDER)
{

	m_pApp = (wxOsmo4Frame *)parent;

	m_pOpen = new wxBitmap(pl_open);
	m_pSave = new wxBitmap(pl_save);
	m_pAdd = new wxBitmap(pl_add);
	m_pRem = new wxBitmap(pl_rem);
	m_pUp = new wxBitmap(pl_up);
	m_pDown = new wxBitmap(pl_down);
	m_pSort = new wxBitmap(pl_sort);

	m_pToolBar = CreateToolBar(wxTB_HORIZONTAL);
	m_pAddBut = new wxMenuButton(m_pToolBar, ID_PL_ADD_FILE, *m_pAdd);
	wxMenu *menu = new wxMenu();
	menu->Append(ID_PL_ADD_URL, wxT("&Url"));
	menu->Append(ID_PL_ADD_DIR, wxT("&Directory"));
	menu->Append(ID_PL_ADD_DIR_REC, wxT("&Directory and subfolders"));
	m_pAddBut->AssignMenu(menu);
	m_pAddBut->SetToolTip(wxString(wxT("Add Files")));

	m_pRemBut = new wxMenuButton(m_pToolBar, ID_PL_REM_FILE, *m_pRem);
	menu = new wxMenu();
	menu->Append(ID_PL_REM_ALL, wxT("&Clear"));
	menu->Append(ID_PL_REM_DEAD, wxT("&Remove dead entries"));
	m_pRemBut->AssignMenu(menu);
	m_pRemBut->SetToolTip(wxString(wxT("Remove Selected Files")));

	m_pSortBut = new wxMenuButton(m_pToolBar, ID_PL_SORT_FILE, *m_pSort);
	menu = new wxMenu();
	menu->Append(ID_PL_SORT_TITLE, wxT("&Sort by Title"));
	menu->Append(ID_PL_SORT_FILE, wxT("&Sort by file name"));
	menu->Append(ID_PL_SORT_DUR, wxT("&Sort by Duration"));
	menu->AppendSeparator();
	menu->Append(ID_PL_REVERSE, wxT("&Reverse"));
	menu->Append(ID_PL_RANDOMIZE, wxT("&Randomize"));
	m_pSortBut->AssignMenu(menu);
	m_pSortBut->SetToolTip(wxString(wxT("Sort Playlist by filename")));

	m_pToolBar->AddTool(ID_PL_OPEN, wxT(""), *m_pOpen, wxT("Open Playlist"));
	m_pToolBar->AddTool(ID_PL_SAVE, wxT(""), *m_pSave, wxT("Save Playlist"));
	m_pToolBar->AddSeparator();
	m_pToolBar->AddControl(m_pAddBut);
	m_pToolBar->AddControl(m_pRemBut);
	m_pToolBar->AddSeparator();
	m_pToolBar->AddTool(ID_PL_UP, wxT(""), *m_pUp, wxT("Moves Selected Files Up"));
	m_pToolBar->AddTool(ID_PL_DOWN, wxT(""), *m_pDown, wxT("Moves Selected Files Down"));
	m_pToolBar->AddSeparator();
	m_pToolBar->AddControl(m_pSortBut);
	m_pToolBar->Realize();

	m_FileList = new wxListCtrl(this, ID_FILE_LIST, wxDefaultPosition, wxDefaultSize, wxLC_REPORT);

	m_FileList->InsertColumn(0, wxT(""), wxLIST_FORMAT_LEFT, 1);
	m_FileList->InsertColumn(1, wxT("Title"), wxLIST_FORMAT_LEFT, 1);
	m_FileList->InsertColumn(2, wxT("Duration"), wxLIST_FORMAT_LEFT, 1);

	m_entries = gf_list_new();
	m_cur_entry = -1;
	m_all_dead_entries = -1;

	SetSize(220, 380);
	Centre();
}

wxPlaylist::~wxPlaylist()
{
	Clear();
	gf_list_del(m_entries);
	delete m_pAddBut;
	delete m_pRemBut;
	delete m_pSortBut;
	delete m_pOpen;
	delete m_pSave;
	delete m_pAdd;
	delete m_pRem;
	delete m_pUp;
	delete m_pDown;
	delete m_pSort;
}


BEGIN_EVENT_TABLE(wxPlaylist, wxWindow)
	EVT_CLOSE(wxPlaylist::OnClose)
	EVT_SIZE(wxPlaylist::OnSize)
	EVT_TOOL(ID_PL_ADD_FILE, wxPlaylist::OnAddFile)
	EVT_TOOL(ID_PL_ADD_URL, wxPlaylist::OnAddURL)
	EVT_TOOL(ID_PL_ADD_DIR, wxPlaylist::OnAddDir)
	EVT_TOOL(ID_PL_ADD_DIR_REC, wxPlaylist::OnAddDirRec)
	EVT_TOOL(ID_PL_REM_FILE, wxPlaylist::OnRemFile)
	EVT_TOOL(ID_PL_REM_ALL, wxPlaylist::OnRemAll)
	EVT_TOOL(ID_PL_REM_DEAD, wxPlaylist::OnRemDead)
	EVT_TOOL(ID_PL_UP, wxPlaylist::OnSelUp)
	EVT_TOOL(ID_PL_DOWN, wxPlaylist::OnSelDown)
	EVT_TOOL(ID_PL_SAVE, wxPlaylist::OnSave)
	EVT_TOOL(ID_PL_OPEN, wxPlaylist::OnOpen)
	EVT_MENU(ID_PL_PLAY, wxPlaylist::OnPlay)
	EVT_MENU(ID_PL_RANDOMIZE, wxPlaylist::OnRandomize)
	EVT_MENU(ID_PL_REVERSE, wxPlaylist::OnReverseList)
	EVT_MENU(ID_PL_SEL_REV, wxPlaylist::OnReverseSelection)
	EVT_MENU(ID_PL_SORT_TITLE, wxPlaylist::OnSortTitle)
	EVT_MENU(ID_PL_SORT_FILE, wxPlaylist::OnSortFile)
	EVT_MENU(ID_PL_SORT_DUR, wxPlaylist::OnSortDuration)
	EVT_LIST_ITEM_ACTIVATED(ID_FILE_LIST, wxPlaylist::OnItemActivate)
	EVT_LIST_ITEM_RIGHT_CLICK(ID_FILE_LIST, wxPlaylist::OnRightClick)
END_EVENT_TABLE()

void wxPlaylist::OnClose(wxCloseEvent &event)
{
	if (event.CanVeto()) {
		event.Veto();
		Hide();
	}
}

void wxPlaylist::OnSize(wxSizeEvent &event)
{
	wxSize s = event.GetSize();
	m_FileList->SetSize(0, 0, s.GetWidth()-2, s.GetHeight());
	m_FileList->SetColumnWidth(0, 30);
	m_FileList->SetColumnWidth(2, 60);
	m_FileList->SetColumnWidth(1, s.GetWidth()-96);
}

void wxPlaylist::Clear()
{
	m_FileList->DeleteAllItems();
	while (gf_list_count(m_entries)) {
		PLEntry *ple = (PLEntry *) gf_list_get(m_entries, 0);
		gf_list_rem(m_entries, 0);
		delete ple;
	}
	m_cur_entry = -1;
}

void wxPlaylist::ClearButPlaying()
{
	PLEntry *p = NULL;
	if (m_cur_entry >= 0) {
		p = (PLEntry *) gf_list_get(m_entries, m_cur_entry);
		gf_list_rem(m_entries, m_cur_entry);
	}
	Clear();
	if (p) {
		gf_list_add(m_entries, p);
		m_cur_entry = 0;
	}
	RefreshList();
}

void wxPlaylist::UpdateEntry(u32 idx)
{
	char szText[20];

	PLEntry *ple = (PLEntry *) gf_list_get(m_entries, idx);
	if (idx+1<10) sprintf(szText, "00%d", idx+1);
	else if (idx+1<100) sprintf(szText, "0%d", idx+1);
	else sprintf(szText, "%d", idx+1);
	m_FileList->SetItem(idx, 0, wxString(szText, wxConvUTF8));

	wxString str;
	if (ple->m_bIsDead) str = wxT("!! ") + wxString(ple->m_disp_name, wxConvUTF8) + wxT(" (DEAD)!!)");
	else if (ple->m_bIsPlaying) str = wxT(">> ") + wxString(ple->m_disp_name, wxConvUTF8) + wxT(" >>");
	else str = wxString(ple->m_disp_name, wxConvUTF8);
	m_FileList->SetItem(idx, 1, str);

	if (ple->m_duration) {
		u32 h = (u32) (ple->m_duration / 3600);
		u32 m = (u32) (ple->m_duration / 60) - h*60;
		u32 s = (u32) (ple->m_duration) - h*3600 - m*60;
		m_FileList->SetItem(idx, 2, wxString::Format(wxT("%02d:%02d:%02d"), h, m, s) );
	} else {
		m_FileList->SetItem(idx, 2, wxT("Unknown"));
	}
}

void wxPlaylist::RefreshList()
{
	u32 i, top_idx;
	char szPath[GF_MAX_PATH];
	Bool first_sel;

	top_idx = m_FileList->GetTopItem();
	m_FileList->DeleteAllItems();

	first_sel = 0;
	for (i=0; i<gf_list_count(m_entries); i++) {
		PLEntry *ple = (PLEntry *) gf_list_get(m_entries, i);
		m_FileList->InsertItem(i, wxT(""));
		/*cast for 64-bit compilation*/
		m_FileList->SetItemData(i, (unsigned long) ple);
		UpdateEntry(i);

		if (ple->m_bIsPlaying) m_cur_entry = i;

		if (ple->m_bIsSelected) {
			m_FileList->SetItemState(i, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED | wxLIST_MASK_STATE);
			ple->m_bIsSelected = 0;
			/*ensure first item of selection is visible*/
			if (!first_sel) {
				first_sel = 1;
				top_idx = i;
			}
		}
	}

	if (m_cur_entry >= (s32) gf_list_count(m_entries)-1) m_cur_entry = gf_list_count(m_entries)-1;
	else {
		s32 last_idx = top_idx + m_FileList->GetCountPerPage();
		m_FileList->EnsureVisible(top_idx);
		if (gf_list_count(m_entries)<1+ (u32) last_idx) last_idx = gf_list_count(m_entries)-1;
		m_FileList->EnsureVisible(last_idx);
	}

	strcpy((char *) szPath, m_pApp->szAppPath);
#ifdef WIN32
	strcat(szPath, "gpac_pl.m3u");
#else
	strcat(szPath, ".gpac_pl.m3u");
#endif
	Save(szPath, 1);
}

void wxPlaylist::OnAddFile(wxCommandEvent &WXUNUSED(event))
{
	wxFileDialog dlg(this, wxT("Select file(s)"), wxT(""), wxT(""), m_pApp->GetFileFilter(), wxOPEN | wxCHANGE_DIR | /*wxHIDE_READONLY |*/ wxMULTIPLE);

	if (dlg.ShowModal() == wxID_OK) {
		wxArrayString stra;
		dlg.GetPaths(stra);
		for (u32 i=0; i<stra.GetCount(); i++) {
			PLEntry *ple = new PLEntry(stra[i]);
			gf_list_add(m_entries, ple);
		}
		m_all_dead_entries = -1;
		RefreshList();
	}
}


static Bool pl_enum_dir_item(void *cbck, char *item_name, char *item_path, GF_FileEnumInfo *file_info)
{
	wxPlaylist *_this = (wxPlaylist *)cbck;

	if (gf_term_is_supported_url(_this->m_pApp->m_term, item_name, 0, 1)) {
		PLEntry *ple = new PLEntry(wxString(item_path, wxConvUTF8) );
		gf_list_add(_this->m_entries, ple);
	}
	return 0;
}

static Bool pl_enum_dir_dirs(void *cbck, char *item_name, char *item_path, GF_FileEnumInfo *file_info)
{
	gf_enum_directory(item_path, 0, pl_enum_dir_item, cbck, NULL);
	gf_enum_directory(item_path, 1, pl_enum_dir_dirs, cbck, NULL);
	return 0;
}


void wxPlaylist::AddDir(Bool do_recurse)
{
	wxDirDialog dlg(this);
	dlg.SetPath(wxString(szCacheDir, wxConvUTF8) );
	if (dlg.ShowModal() != wxID_OK) return;

	strcpy(szCacheDir, dlg.GetPath().mb_str(wxConvUTF8));
	gf_enum_directory(szCacheDir, 0, pl_enum_dir_item, this, NULL);
	if (do_recurse) gf_enum_directory(szCacheDir, 1, pl_enum_dir_dirs, this, NULL);
	m_all_dead_entries = -1;
	RefreshList();
}
void wxPlaylist::OnAddDir(wxCommandEvent &WXUNUSED(event))
{
	AddDir(0);
}
void wxPlaylist::OnAddDirRec(wxCommandEvent &WXUNUSED(event))
{
	AddDir(1);
}

void wxPlaylist::OnAddURL(wxCommandEvent &WXUNUSED(event))
{
	OpenURLDlg dlg(this, m_pApp->m_user.config);
	if (dlg.ShowModal() != wxID_OK) return;
	PLEntry *ple = new PLEntry(dlg.m_urlVal);
	gf_list_add(m_entries, ple);
	m_all_dead_entries = -1;
	RefreshList();
}

void wxPlaylist::OnRemFile(wxCommandEvent &WXUNUSED(event))
{
	if (!m_FileList->GetSelectedItemCount()) return;

	long item = -1;
	for (;;) {
		item = m_FileList->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (item == -1) break;
		PLEntry *ple = (PLEntry *) m_FileList->GetItemData(item);
		gf_list_del_item(m_entries, ple);
		delete ple;
	}
	RefreshList();
}

void wxPlaylist::OnRemAll(wxCommandEvent &WXUNUSED(event))
{
	Clear();
	RefreshList();
	m_cur_entry = -1;
	m_all_dead_entries = 1;
}

void wxPlaylist::OnRemDead(wxCommandEvent &WXUNUSED(event))
{
	for (u32 i=0; i<gf_list_count(m_entries); i++) {
		PLEntry *ple = (PLEntry *) gf_list_get(m_entries, i);
		if (!ple->m_bIsDead) continue;
		gf_list_rem(m_entries, i);
		i--;
		delete ple;
	}
	m_all_dead_entries = gf_list_count(m_entries) ? 0 : 1;
	RefreshList();
}


void wxPlaylist::OnSelUp(wxCommandEvent &WXUNUSED(event))
{
	s32 i;
	if (!m_FileList->GetSelectedItemCount()) return;
	long item = -1;
	item = m_FileList->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (item <= 0) return;

	item = -1;
	for (;;) {
		item = m_FileList->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (item == -1) break;
		PLEntry *ple = (PLEntry *) m_FileList->GetItemData(item);
		i = gf_list_del_item(m_entries, ple);
		assert(i>=1);
		gf_list_insert(m_entries, ple, i-1);
		ple->m_bIsSelected = 1;
	}
	RefreshList();
}

void wxPlaylist::OnSelDown(wxCommandEvent &WXUNUSED(event))
{
	s32 i;

	if (!m_FileList->GetSelectedItemCount()) return;
	long item = -1;
	for (;;) {
		item = m_FileList->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (item == -1) break;
	}
	if ((u32) item + 1 == gf_list_count(m_entries)) return;

	item = -1;
	for (;;) {
		item = m_FileList->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (item == -1) break;
		PLEntry *ple = (PLEntry *) m_FileList->GetItemData(item);
		i = gf_list_del_item(m_entries, ple);
		assert(i>=1);
		gf_list_insert(m_entries, ple, i+1);
		ple->m_bIsSelected = 1;
	}
	RefreshList();
}



void wxPlaylist::OnSave(wxCommandEvent & WXUNUSED(event))
{
	Bool save_m3u;
	char szPath[GF_MAX_PATH];
	if (!gf_list_count(m_entries)) return;

	wxFileDialog dlg(this, wxT("Select file(s)"), wxT(""), wxT(""), wxT("M3U Playlists|*.m3u|ShoutCast Playlists|*.pls|"), wxSAVE | wxCHANGE_DIR | wxOVERWRITE_PROMPT);
	if (dlg.ShowModal() != wxID_OK) return;

	strcpy(szPath, dlg.GetPath().mb_str(wxConvUTF8));
	strlwr(szPath);
	save_m3u = (dlg.GetFilterIndex()==0) ? 1 : 0;
	if (save_m3u) {
		if (!strstr(szPath, ".m3u")) {
			strcpy(szPath, dlg.GetPath().mb_str(wxConvUTF8));
			strcat(szPath, ".m3u");
		} else {
			strcpy(szPath, dlg.GetPath().mb_str(wxConvUTF8));
		}
	} else {
		if (!strstr(szPath, ".pls")) {
			strcpy(szPath, dlg.GetPath().mb_str(wxConvUTF8));
			strcat(szPath, ".pls");
		} else {
			strcpy(szPath, dlg.GetPath().mb_str(wxConvUTF8));
		}
	}
	Save(szPath, save_m3u);
}

void wxPlaylist::Save(char *szPath, Bool save_m3u)
{
	FILE *out = gf_fopen(szPath, "wt");
	if (!save_m3u)
		fprintf(out, "[playlist]\nNumberOfEntries=%d\n", gf_list_count(m_entries));

	for (u32 i=0; i<gf_list_count(m_entries); i++) {
		PLEntry *ple = (PLEntry *) gf_list_get(m_entries, i);
		if (save_m3u) {
			fprintf(out, "%s\n", ple->m_url);
		} else {
			fprintf(out, "File%d=%s\n", i+1, ple->m_url);
			fprintf(out, "Title%d=%s\n", i+1, ple->m_disp_name);
			if (ple->m_duration) fprintf(out, "Length%d=%d\n", i+1, ple->m_duration);
			else fprintf(out, "Length%d=-1\n", i+1);
		}
	}
	if (!save_m3u) fprintf(out, "Version=2\n");

	fprintf(out, "\n");
	gf_fclose(out);
}

void wxPlaylist::OnOpen(wxCommandEvent & WXUNUSED(event))
{
	wxFileDialog dlg(this, wxT("Select file(s)"), wxT(""), wxT(""), wxT("M3U & PLS Playlists|*.m3u;*.pls|M3U Playlists|*.m3u|ShoutCast Playlists|*.pls|"), wxOPEN | wxCHANGE_DIR/* | wxHIDE_READONLY*/);
	if (dlg.ShowModal() != wxID_OK) return;

	Clear();
	OpenPlaylist(dlg.GetPath());
	m_cur_entry = 0;
	Play();
}

void wxPlaylist::OpenPlaylist(wxString filename)
{
	FILE *pl;
	PLEntry *ple;
	Bool load_m3u, go;
	char szLine[GF_MAX_PATH];
	pl = gf_fopen(filename.mb_str(wxConvUTF8) , "rt");
	if (!pl) return;

	ple = NULL;
	load_m3u = 1;
	while (!feof(pl)) {
		fgets(szLine, GF_MAX_PATH, pl);
		go = 1;
		while (go) {
			switch (szLine[strlen(szLine)-1]) {
			case '\n':
			case '\r':
			case ' ':
				szLine[strlen(szLine)-1] = 0;
				break;
			default:
				go = 0;
				break;
			}
		}
		if (!strlen(szLine)) continue;
		if (!stricmp(szLine, "[playlist]")) {
			load_m3u = 0;
		} else if (load_m3u) {
			ple = new PLEntry(wxString(szLine, wxConvUTF8) );
			gf_list_add(m_entries, ple);
		} else if (!strnicmp(szLine, "file", 4)) {
			char *st = strchr(szLine, '=');
			if (!st) ple = NULL;
			else {
				ple = new PLEntry(wxString(st + 1, wxConvUTF8) );
				gf_list_add(m_entries, ple);
			}
		} else if (ple && !strnicmp(szLine, "Length", 6)) {
			char *st = strchr(szLine, '=');
			s32 d = atoi(st + 1);
			if (d>0) ple->m_duration = d;
		} else if (ple && !strnicmp(szLine, "Title", 5)) {
			char *st = strchr(szLine, '=');
			gf_free(ple->m_disp_name);
			ple->m_disp_name = gf_strdup(st + 6);
		}
	}
	gf_fclose(pl);
	m_all_dead_entries = -1;
	m_cur_entry = -1;
	RefreshList();
}

void wxPlaylist::OnRightClick(wxListEvent & event)
{
	if (!m_FileList->GetItemCount()) return;

	wxMenu *popup = new wxMenu();

	if (m_FileList->GetSelectedItemCount()==1) {
		popup->Append(ID_PL_PLAY, wxT("Play"));
		popup->AppendSeparator();
	}
	popup->Append(ID_PL_SEL_REV, wxT("Inverse Selection"));
	if (m_FileList->GetSelectedItemCount()) popup->Append(ID_PL_REM_FILE, wxT("Remove File(s)"));
	if (m_FileList->GetItemCount()>1) {
		popup->AppendSeparator();
		popup->Append(ID_PL_SORT_TITLE, wxT("Sort By Title"));
		popup->Append(ID_PL_SORT_FILE, wxT("Sort By File Name"));
		popup->Append(ID_PL_SORT_DUR, wxT("Sort By Duration"));
		popup->AppendSeparator();
		popup->Append(ID_PL_REVERSE, wxT("Reverse List"));
		popup->Append(ID_PL_RANDOMIZE, wxT("Randomize"));
	}

	PopupMenu(popup, event.GetPoint());
	delete popup;
}

void wxPlaylist::OnReverseSelection(wxCommandEvent &WXUNUSED(event) )
{
	u32 i;
	long item = -1;
	for (;;) {
		item = m_FileList->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (item == -1) break;
		PLEntry *ple = (PLEntry *) m_FileList->GetItemData(item);
		ple->m_bIsSelected = 1;
	}
	for (i=0; i<gf_list_count(m_entries); i++) {
		PLEntry *ple = (PLEntry *) gf_list_get(m_entries, i);
		ple->m_bIsSelected = !ple->m_bIsSelected;
	}
	RefreshList();
}

void wxPlaylist::OnReverseList(wxCommandEvent &WXUNUSED(event) )
{
	u32 count = gf_list_count(m_entries);
	u32 hcount = count / 2;
	count--;
	for (u32 i=0; i<hcount; i++) {
		PLEntry *ple1 = (PLEntry *) gf_list_get(m_entries, i);
		PLEntry *ple2 = (PLEntry *) gf_list_get(m_entries, count-i);
		gf_list_rem(m_entries, i);
		gf_list_insert(m_entries, ple2, i);
		gf_list_rem(m_entries, count-i);
		gf_list_insert(m_entries, ple1, count-i);
	}
	RefreshList();
}

void wxPlaylist::OnRandomize(wxCommandEvent &WXUNUSED(event) )
{
	GF_List *new_entries = gf_list_new();

	gf_rand_init(0);

	while (gf_list_count(m_entries)>1) {
		u32 pos = gf_rand() % (gf_list_count(m_entries)-1);
		PLEntry *ple = (PLEntry *)gf_list_get(m_entries, pos);
		gf_list_rem(m_entries, pos);
		gf_list_add(new_entries, ple);
	}
	PLEntry *ple = (PLEntry *)gf_list_get(m_entries, 0);
	gf_list_rem(m_entries, 0);
	gf_list_add(new_entries, ple);

	gf_list_del(m_entries);
	m_entries = new_entries;
	m_cur_entry = -1;
	RefreshList();
}

void wxPlaylist::Sort(u32 type)
{
	u32 i, j, smallest;

	for (i=0; i<gf_list_count(m_entries)-1; i++) {
		smallest = i;
		for (j=i+1; j<gf_list_count(m_entries); j++) {
			PLEntry *ple2 = (PLEntry *) gf_list_get(m_entries, smallest);
			PLEntry *ple1 = (PLEntry *) gf_list_get(m_entries, j);
			s32 test = 0;
			switch (type) {
			case 0:
				test = stricmp(ple1->m_url, ple2->m_url);
				break;
			case 1:
				test = stricmp(ple1->m_disp_name, ple2->m_disp_name);
				break;
			case 2:
				test = ple1->m_duration - ple2->m_duration;
				break;
			}
			if (test<0) smallest = j;
		}
		PLEntry *ple = (PLEntry *)gf_list_get(m_entries, smallest);
		gf_list_rem(m_entries, smallest);
		gf_list_insert(m_entries, ple, i);
	}
	m_cur_entry = -1;
	RefreshList();
}

void wxPlaylist::OnSortFile(wxCommandEvent &WXUNUSED(event) ) {
	Sort(0);
}
void wxPlaylist::OnSortTitle(wxCommandEvent &WXUNUSED(event) ) {
	Sort(1);
}
void wxPlaylist::OnSortDuration(wxCommandEvent &WXUNUSED(event) ) {
	Sort(2);
}

void wxPlaylist::RefreshCurrent()
{
	PLEntry *ple;
	if (m_cur_entry<0) return;
	ple = (PLEntry *) gf_list_get(m_entries, m_cur_entry);
	if (ple && ple->m_bIsPlaying) {
		ple->m_bIsPlaying = 0;
		UpdateEntry(m_cur_entry);
	}
}

Bool wxPlaylist::HasValidEntries()
{
	u32 nb_dead = 0;
	if (m_all_dead_entries==-1) {
		for (u32 i=0; i<gf_list_count(m_entries); i++) {
			PLEntry *ple = (PLEntry *) gf_list_get(m_entries, i);
			ple->m_bIsPlaying = 0;
			if (ple->m_bIsDead) nb_dead ++;
		}
		m_all_dead_entries = (nb_dead==gf_list_count(m_entries)) ? 1 : 0;
	}
	return !m_all_dead_entries;
}

void wxPlaylist::Play()
{
	PLEntry *ple;

	if (!HasValidEntries()) return;

	RefreshCurrent();

	if (m_cur_entry >= (s32)gf_list_count(m_entries)) {
		if (!m_pApp->m_loop) return;
		m_cur_entry = 0;
	}

	ple = (PLEntry *) gf_list_get(m_entries, m_cur_entry);
	if (!ple || ple->m_bIsDead) {
		m_cur_entry++;
		Play();
	} else {
		char szPLE[20];
		ple->m_bIsPlaying = 1;
		UpdateEntry(m_cur_entry);
		sprintf(szPLE, "%d", m_cur_entry);
		gf_cfg_set_key(m_pApp->m_user.config, "General", "PLEntry", szPLE);
		m_pApp->DoConnect();
	}
}

void wxPlaylist::OnItemActivate(wxListEvent &WXUNUSED(event) )
{
	long item = m_FileList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (item==-1) return;
	RefreshCurrent();
	m_cur_entry = item;
	Play();
}


void wxPlaylist::OnPlay(wxCommandEvent &WXUNUSED(event))
{
	long item = m_FileList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (item==-1) return;

	RefreshCurrent();
	m_cur_entry = item;
	Play();
}

void wxPlaylist::Truncate()
{
	if (m_cur_entry<0) return;
	while ((u32) m_cur_entry+1 < gf_list_count(m_entries)) {
		PLEntry *ple = (PLEntry *) gf_list_get(m_entries, m_cur_entry+1);
		gf_list_rem(m_entries, m_cur_entry+1);
		delete ple;
	}
	RefreshList();
}

void wxPlaylist::QueueURL(wxString filename)
{
	char *ext = (char*)strrchr(filename.mb_str(wxConvUTF8), '.');
	if (ext && (!stricmp(ext, ".m3u") || !stricmp(ext, ".pls")) ) {
		OpenPlaylist(filename);
	} else {
		PLEntry *ple = new PLEntry(filename);
		gf_list_add(m_entries, ple);
	}
}

void wxPlaylist::PlayNext()
{
	RefreshCurrent();
	if (1+m_cur_entry < (s32)gf_list_count(m_entries)) {
		m_cur_entry++;
		Play();
	}
}

void wxPlaylist::PlayPrev()
{
	RefreshCurrent();
	if (m_cur_entry>0) {
		m_cur_entry--;
		Play();
	}
}

void wxPlaylist::SetDead()
{
	PLEntry *ple = (PLEntry *) gf_list_get(m_entries, m_cur_entry);
	if (ple) {
		ple->m_bIsDead = 1;
		UpdateEntry(m_cur_entry);
		if (ple->m_bIsPlaying) PlayNext();
		m_all_dead_entries = -1;
	}
}
void wxPlaylist::SetDuration(u32 duration)
{
	PLEntry *ple = (PLEntry *) gf_list_get(m_entries, m_cur_entry);
	if (ple) {
		ple->m_duration = duration;
		UpdateEntry(m_cur_entry);
	}
}

wxString wxPlaylist::GetDisplayName()
{
	if (m_cur_entry>=0) {
		PLEntry *ple = (PLEntry *) gf_list_get(m_entries, m_cur_entry);
		if (ple) return wxString(wxString(ple->m_disp_name, wxConvUTF8) );
	}
	return wxT("");
}

wxString wxPlaylist::GetURL()
{
	PLEntry *ple = (PLEntry *) gf_list_get(m_entries, m_cur_entry);
	if (ple) return wxString(ple->m_url, wxConvUTF8);
	return wxT("");
}

