#ifndef _V4STUDIO_TREE_H
#define _V4STUDIO_TREE_H

#include "safe_include.h" 
#include <gpac/scenegraph.h>
#include "V4FieldList.h"

#include <wx/treectrl.h>


class V4StudioFrame;


class V4StudioTreeItemData : public wxTreeItemData {
public:
	V4StudioTreeItemData(GF_Node *n, GF_Node *p, s32 fi, s32 pos = -1) : node(n), parent(p), fieldIndex(fi), position(pos) {}

public:
	GF_Node *GetNode() { return node; }
	void SetNode(GF_Node *n) { node = n; }
	GF_Err GetField(GF_FieldInfo *f);

	GF_Node *GetNodeParent() { return parent; }
	void SetNodeParent(GF_Node *n) { parent = n; }
	s32 GetPosition() { return position; }
	void SetPosition(s32 pos) { position = pos; }
	void SetFieldIndex(s32 i) { fieldIndex = i; }
	s32 GetFieldIndex() { return fieldIndex; }

private:
	GF_Node *node;
	GF_Node *parent;
	s32 fieldIndex;
	s32 position;

};

class V4StudioTree: public wxTreeCtrl {

public:
    enum
    {
        TreeCtrlIcon_File,
        TreeCtrlIcon_FileSelected,
        TreeCtrlIcon_Folder,
        TreeCtrlIcon_FolderSelected,
        TreeCtrlIcon_FolderOpened
    };

	V4StudioTree(wxWindow *parent, wxSize size, V4FieldList *fieldView);

	void CreateImageList(int size = 16);
	
	void Refresh(GF_Node * node); 

	void OnSelChanged(wxTreeEvent& event);
	void OnItemRightClick(wxTreeEvent &event);
	void ShowMenu(wxTreeItemId id, const wxPoint& pt);
	void OnBeginDrag(wxTreeEvent& event);
	void OnEndDrag(wxTreeEvent& event);

	void SetSelectedItem(GF_Node *node);
	void Translate(int dX, int dY);
	void Scale(int dX, int dY);
	void Rotate(int dX, int dY);
	wxTreeItemId FindNodeItem(wxTreeItemId itemId, GF_Node *node);
	GF_Node *FindTransformNode(wxTreeItemId itemId);

protected:
    DECLARE_EVENT_TABLE()

private:
	void AddNodesToItem(wxTreeItemId parentItemId, GF_Node * node, s32 fieldIndex, s32 position);

    wxTreeItemId m_selectedItem;             // item being dragged right now
    wxTreeItemId m_draggedItem;             // item being dragged right now

	GF_Node * m_selectedNode;
	GF_Node * m_transformNode;

	// All the component of the V4Studio Main Frame have a pointer to that Main Frame, called parent.
	V4StudioFrame * parent;

};

#endif
