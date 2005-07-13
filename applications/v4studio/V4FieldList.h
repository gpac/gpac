#ifndef _V4_FIELD_LIST_H
#define _V4_FIELD_LIST_H

#include "safe_include.h" 
#include <gpac/scenegraph.h>

#include <wx/grid.h>


class V4StudioFrame;


class V4FieldList: public wxGrid {

public:
	V4FieldList(wxWindow *parent, wxSize size);
	~V4FieldList(); 

	void SetNode(GF_Node *node) { m_pNode = node; }
	
	void Create(); 
	void GetFieldValue(GF_FieldInfo f, wxString *s, int pos);
	void SetFieldValue(GF_FieldInfo f, wxString *s, int pos);

	void OnCellChanged(wxGridEvent &evt);
	void OnCellLeftClick(wxGridEvent &evt);
	void OnCellRightClick(wxGridEvent &evt);
	void OnCellLeftDClick(wxGridEvent &evt);
protected:
    DECLARE_EVENT_TABLE()

private:
	void SetLog(wxString s);

	void InsertOneFieldRow(u32 position, GF_FieldInfo field);

	void SetCellFieldValue(GF_FieldInfo field, u32 pos);

	GF_Node * m_pNode;
	GF_List *positions;

	// All the component of the V4Studio Main Frame have a pointer to that Main Frame, called parent.
	V4StudioFrame * parent;

};

#endif
