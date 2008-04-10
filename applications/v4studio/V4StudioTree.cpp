#include "safe_include.h" 

// For compilers that supports precompilation , includes "wx/wx.h"
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
	#include "wx/wx.h"
#endif

#include "V4StudioTree.h"
#include "V4StudioFrame.h"

#include "V4SceneManager.h" // DICTNAME constant

#include "wx/image.h"
#include "wx/imaglist.h"
#include "wx/treectrl.h"

#include <math.h>

#include "treeIcon1.xpm"
#include "treeIcon2.xpm"
#include "treeIcon3.xpm"
#include "treeIcon4.xpm"
#include "treeIcon5.xpm"

#include <gpac/nodes_mpeg4.h>


BEGIN_EVENT_TABLE(V4StudioTree, wxTreeCtrl)
    EVT_TREE_SEL_CHANGED(-1, V4StudioTree::OnSelChanged)
	EVT_TREE_ITEM_RIGHT_CLICK(-1, V4StudioTree::OnItemRightClick)
    EVT_TREE_BEGIN_DRAG(-1, V4StudioTree::OnBeginDrag)
    EVT_TREE_END_DRAG(-1, V4StudioTree::OnEndDrag)
END_EVENT_TABLE()

GF_Err V4StudioTreeItemData::GetField(GF_FieldInfo *f)
{
	if (!f || fieldIndex < 0) return GF_BAD_PARAM;
	return gf_node_get_field(parent, fieldIndex, f);	
}

V4StudioTree::V4StudioTree(wxWindow *parent_, wxSize size,V4FieldList *fieldView) : 
	wxTreeCtrl(parent_, -1, wxDefaultPosition, size, wxTR_DEFAULT_STYLE)
{
	CreateImageList();
	AddRoot("Scene", V4StudioTree::TreeCtrlIcon_Folder, V4StudioTree::TreeCtrlIcon_FolderOpened);
	m_transformNode = NULL;
	m_selectedNode = NULL;

	parent = (V4StudioFrame *) parent_;
}


// Refresh -- redraws the tree starting at the specified node
void V4StudioTree::Refresh(GF_Node * node) 
{

	// check if we have to display the dictionnary or not
	if (parent->GetEditDict())
	  node = parent->GetV4SceneManager()->GetDictionnary();


	wxTreeItemId rootId = GetRootItem();
	CollapseAndReset(rootId);
	if (node) {
		AddNodesToItem(rootId, node, -1, -1);
		Expand(rootId);
	}
}

void V4StudioTree::AddNodesToItem(wxTreeItemId parentItemId, GF_Node * node, s32 fieldIndex, s32 position) 
{
	GF_FieldInfo field;
	u32 count, i, j;
	char *name;

	if (!node) return;

	// displays the name and the defname it exists
  u32 s = strlen(gf_node_get_class_name(node));

  if (gf_node_get_name(node)) {
    s += strlen(gf_node_get_name(node));
    name = new char[s+4];
    strcpy(name, gf_node_get_class_name(node));
    strcat(name, " - ");
    strcat(name, gf_node_get_name(node));
  } else {
    name = new char[s+1];
    strcpy(name, gf_node_get_class_name(node));
  }

	GF_Node * parent = NULL;
	
	V4StudioTreeItemData *parentItemData = (V4StudioTreeItemData *)GetItemData(parentItemId);
	if (parentItemData != NULL) parent = parentItemData->GetNode();

	V4StudioTreeItemData * currentItemData = new V4StudioTreeItemData(node, parent, fieldIndex, position);
	wxTreeItemId nodeItemId;
	if (position == -1) nodeItemId = AppendItem(parentItemId, wxString(name), -1, -1, currentItemData);
	else nodeItemId = InsertItem(parentItemId, position, wxString(name), -1, -1, currentItemData);

  delete [] name;
  name = NULL;

	count = gf_node_get_field_count(node);
	for (i=0;i<count; i++) {		
		gf_node_get_field(node, i, &field);
		if (field.eventType == GF_SG_EVENT_IN || field.eventType == GF_SG_EVENT_OUT) continue;
		switch (field.fieldType) {
		case GF_SG_VRML_SFNODE:
			if (* (GF_Node **) field.far_ptr) {
				AddNodesToItem(nodeItemId, * (GF_Node **) field.far_ptr, i, -1);
			}
			break;
		case GF_SG_VRML_MFNODE:
			{
				GF_ChildNodeItem *nodes = (* (GF_ChildNodeItem **) field.far_ptr); // list of children
				u32 nbNodes = gf_node_list_get_count(nodes);
				u8 skipped = 0; // counts nodes not added

				for (j=0; j< nbNodes; j++) {          
					// gets a pointer to the current child
					GF_Node *child = (GF_Node *)gf_node_list_get_child(nodes, j);
					// prevents the dictionnary from being added to the graph
					const char * c = gf_node_get_name(child);
					if ( (c != NULL) && !strcmp(c,DICTNAME) ) {
						skipped++;
						continue;
					}
					// recursively adds children
					AddNodesToItem(nodeItemId, child, i, j-skipped);
				}
			}
			break;
		default:
			break;
		}
	}
	Expand(nodeItemId);
}


void V4StudioTree::CreateImageList(int size)
{
    if ( size == -1 ) {
        SetImageList(NULL);
        return;
    }
    // Make an image list containing small icons
    wxImageList *images = new wxImageList(size, size, TRUE);

    // should correspond to TreeCtrlIcon_xxx enum
    wxBusyCursor wait;
    wxIcon icons[5];
    icons[0] = wxIcon(icon1_xpm);
    icons[1] = wxIcon(icon2_xpm);
    icons[2] = wxIcon(icon3_xpm);
    icons[3] = wxIcon(icon4_xpm);
    icons[4] = wxIcon(icon5_xpm);
    int sizeOrig = icons[0].GetWidth();
    for ( size_t i = 0; i < WXSIZEOF(icons); i++ ) {
		if ( size == sizeOrig ) images->Add(icons[i]);
		else images->Add(wxBitmap(wxBitmap(icons[i]).ConvertToImage().Rescale(size, size)));
    }
    AssignImageList(images);
}

void V4StudioTree::OnSelChanged(wxTreeEvent& event) 
{
	wxKeyEvent kevt = event.GetKeyEvent();

	wxTreeItemId itemId = event.GetItem();
	m_selectedItem = itemId;
	if (itemId.IsOk()) {
		V4StudioTreeItemData *itemData = (V4StudioTreeItemData *)GetItemData(itemId);
		if (!itemData) {
			event.Skip();
			return;
		}
		GF_Node *itemNode = itemData->GetNode();
		GF_Node *itemParentNode = itemData->GetNodeParent();
		if (!itemNode) {
			event.Skip();
			return;
		}
		V4StudioFrame *mainFrame = (V4StudioFrame *)GetParent();
		mainFrame->UpdateSelection(itemNode, itemParentNode);
	}
}

void V4StudioTree::SetSelectedItem(GF_Node *node)
{
	if (m_selectedNode == node) return;
	wxTreeItemId rootId = GetRootItem();
	wxTreeItemId itemId = FindNodeItem(rootId, node);
	m_transformNode = FindTransformNode(itemId);
	itemId = GetItemParent(itemId);
	if (itemId.IsOk()) {
		V4StudioTreeItemData *data = (V4StudioTreeItemData *)GetItemData(itemId);
		if (data != NULL) {
			V4StudioFrame *mainFrame = (V4StudioFrame *)GetParent();
			GF_Node *itemNode = data->GetNode();
			m_selectedNode = itemNode;
			GF_Node *itemParentNode = data->GetNodeParent();
			mainFrame->UpdateSelection(itemNode, itemParentNode);
		}
		SelectItem(itemId);
	}
}

wxTreeItemId V4StudioTree::FindNodeItem(wxTreeItemId itemId, GF_Node *node) 
{
	void* cookie;
	V4StudioTreeItemData *data = (V4StudioTreeItemData *)GetItemData(itemId);
	if (data != NULL) {
		if (data->GetNode() == node) return itemId;
	} 
	wxTreeItemId ret = (wxTreeItemId)0l;
	wxTreeItemId child = GetFirstChild(itemId, cookie);
	while (child.IsOk()) {
		wxTreeItemId ret = FindNodeItem(child, node);
		if (ret.IsOk()) return ret;
		child = GetNextChild(itemId, cookie);
	}
	return ret;
}

void V4StudioTree::OnItemRightClick(wxTreeEvent &event) 
{
	wxTreeItemId itemId = event.GetItem();
}

void V4StudioTree::OnBeginDrag(wxTreeEvent& event)
{
    // need to explicitly allow drag
    if ( event.GetItem() != GetRootItem() ){
        m_draggedItem = event.GetItem();
		V4StudioTreeItemData *draggedData = (V4StudioTreeItemData *)GetItemData(m_draggedItem);		
        event.Allow();
    } else {
        wxLogMessage(wxT("OnBeginDrag: this item can't be dragged."));
    }
}

void V4StudioTree::OnEndDrag(wxTreeEvent& event)
{
    wxTreeItemId itemSrc = m_draggedItem, itemDst = event.GetItem(), dstParentItem = GetItemParent(itemDst);
    m_draggedItem = (wxTreeItemId)0l;

	V4StudioTreeItemData *srcData = (V4StudioTreeItemData *)GetItemData(itemSrc);
	GF_FieldInfo srcField;
	srcData->GetField(&srcField);
	// Removal of the src item from its parent field
	switch (srcField.fieldType) {
	case GF_SG_VRML_SFNODE:
		if (* (GF_Node **) srcField.far_ptr) {}
		break;
	case GF_SG_VRML_MFNODE:
		{
			GF_ChildNodeItem *nodes = (* (GF_ChildNodeItem **) srcField.far_ptr);
			gf_node_list_del_child_idx(&nodes, srcData->GetPosition());
		}
		break;
	default:
		break;
	}

	GF_Node *srcNode = srcData->GetNode();
	GF_FieldInfo dstField;
	V4StudioTreeItemData *dstData = (V4StudioTreeItemData *)GetItemData(itemDst);
	dstData->GetField(&dstField);
	// Addition of the src item prior to the dest item
	switch (dstField.fieldType) {
	case GF_SG_VRML_SFNODE:
		if (* (GF_Node **) dstField.far_ptr) {}
		break;
	case GF_SG_VRML_MFNODE:
		{
			GF_ChildNodeItem *nodes = (* (GF_ChildNodeItem **) dstField.far_ptr);
			gf_node_list_insert_child(&nodes, srcNode, dstData->GetPosition());
			gf_node_dirty_set(dstData->GetNode(), 0, 1);
		}
		break;
	default:
		break;
	}

	GF_Node *dstParentNode = dstData->GetNodeParent();
	
	Delete(itemSrc);
	AddNodesToItem(dstParentItem, srcNode, dstData->GetFieldIndex(), dstData->GetPosition());
	V4StudioFrame *mainFrame = (V4StudioFrame *)GetParent();
	mainFrame->UpdateSelection(srcNode,dstData->GetNodeParent());
	mainFrame->Update();
}

GF_Node *V4StudioTree::FindTransformNode(wxTreeItemId itemId) 
{
	GF_Node *transformNode = NULL;
	while (true) {
		if (itemId.IsOk()) {
			V4StudioTreeItemData *data = (V4StudioTreeItemData *)GetItemData(itemId);
			if (data != NULL) {
				GF_Node *itemNode = data->GetNode();
				u32 tag = gf_node_get_tag(itemNode);
				if (tag == TAG_MPEG4_Transform2D || tag == TAG_MPEG4_TransformMatrix2D) {
					transformNode = itemNode;
					break;
				}
			} else {
				break;
			}
		} else {
			break;
		}
		itemId = GetItemParent(itemId);
	}
	return transformNode;
}

void V4StudioTree::Translate(int dX, int dY) 
{
	if (m_transformNode) {
		GF_FieldInfo field;
		u32 tag = gf_node_get_tag(m_transformNode);
		if (tag == TAG_MPEG4_Transform2D){
			gf_node_get_field(m_transformNode, 7, &field);
			((SFVec2f *)field.far_ptr)->x += dX;
			((SFVec2f *)field.far_ptr)->y += dY;
		} else {
			gf_node_get_field(m_transformNode, 5, &field);
			*((SFFloat *)field.far_ptr) += dX;
			gf_node_get_field(m_transformNode, 8, &field);
			*((SFFloat *)field.far_ptr) += dY;
		}
		V4StudioFrame *mainFrame = (V4StudioFrame *)GetParent();
		mainFrame->GetFieldView()->SetNode(m_transformNode);
		mainFrame->GetFieldView()->Create();
	}
}

void V4StudioTree::Scale(int dX, int dY) 
{
	if (m_transformNode && (dX || dY)) {
		GF_FieldInfo field;
		u32 tag = gf_node_get_tag(m_transformNode);
		if (tag == TAG_MPEG4_Transform2D){
			gf_node_get_field(m_transformNode, 5, &field);
			if (dX) ((SFVec2f *)field.far_ptr)->x *= (dX>0?1.01:0.99);
			if (dY) ((SFVec2f *)field.far_ptr)->y *= (dY>0?1.01:0.99);
		} else {
			gf_node_get_field(m_transformNode, 3, &field);
			if (dX) *((SFFloat *)field.far_ptr) *= (dX>0?1.01:0.99);
			gf_node_get_field(m_transformNode, 7, &field);
			if (dY) *((SFFloat *)field.far_ptr) *= (dY>0?1.01:0.99);
		}
		V4StudioFrame *mainFrame = (V4StudioFrame *)GetParent();
		mainFrame->GetFieldView()->SetNode(m_transformNode);
		mainFrame->GetFieldView()->Create();
	}
}


void V4StudioTree::Rotate(int dX, int dY) 
{
	/*quick and dirty of course...*/
	dY *= -1;
	if (m_transformNode && (dX || dY)) {
		u32 tag = gf_node_get_tag(m_transformNode);
		if (tag == TAG_MPEG4_Transform2D){
			Double ang;
			ang = atan2(dY, dX) / 100;
			((M_Transform2D *) m_transformNode)->rotationAngle += ang;
		} else {
			((M_TransformMatrix2D *) m_transformNode)->mxy += dX;
			((M_TransformMatrix2D *) m_transformNode)->myx += dY;
		}
		V4StudioFrame *mainFrame = (V4StudioFrame *)GetParent();
		mainFrame->GetFieldView()->SetNode(m_transformNode);
		mainFrame->GetFieldView()->Create();
	}
}


