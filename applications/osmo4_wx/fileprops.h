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

#ifndef _FILEPROPS_H
#define _FILEPROPS_H

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
  #include "wx/wx.h"
#endif

#include <wx/treectrl.h>

#include <gpac/terminal.h>

/*abstract class for all items in the tree*/
class ODTreeData : public wxTreeItemData
{
public:
	ODTreeData(GF_ObjectManager *odm) : wxTreeItemData(), m_pODMan(odm) {}
	GF_ObjectManager *m_pODMan;
};


class wxOsmo4Frame;
class wxFileProps : public wxDialog 
{
public:
    wxFileProps(wxWindow *parent);
    virtual ~wxFileProps();

private:
	DECLARE_EVENT_TABLE()

	wxOsmo4Frame *m_pApp;

	wxTreeCtrl *m_pTreeView;
	wxTextCtrl *m_pViewInfo;
	wxComboBox *m_pViewSel;
	wxButton *m_pViewWI, *m_pViewSG;
	wxTimer *m_pTimer;

	GF_ObjectManager *m_current_odm;

	void RewriteODTree();
	void SetGeneralInfo();
	void SetStreamsInfo();
	void SetDecoderInfo();
	void SetNetworkInfo();
	void WriteInlineTree(ODTreeData *pRoot);
	void OnSetSelection(wxTreeEvent &event);
	void OnSelectInfo(wxCommandEvent &event);
	void OnTimer(wxTimerEvent &event);
	void OnViewWorld(wxCommandEvent &event);
	void OnViewSG(wxCommandEvent &event);
	void SetInfo(GF_ObjectManager *odm);
};

#endif

