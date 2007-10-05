/*
  V4CommandPanel.cpp

    Panel allowing to choose an action for a given frame in the time line

*/

#include "safe_include.h"

#include "V4CommandPanel.h"
#include "V4StudioFrame.h"

#include "V4Node.h"



// !! order matters !!, index will become the command id
// when command has no string, it is not available
// see v4_scenegraph_vrml.h
wxString cmdNames[] = { wxT(""),                    // GF_SG_SCENE_REPLACE
                        wxT("Replace Node"),        // GF_SG_NODE_REPLACE
                        wxT("Replace Field"),       // GF_SG_FIELD_REPLACE
                        wxT(""),                    // GF_SG_INDEXED_REPLACE
                        wxT(""),                    // GF_SG_ROUTE_REPLACE
                        wxT("Delete Node"),         // GF_SG_NODE_DELETE
                        wxT(""),                    // GF_SG_INDEXED_DELETE
                        wxT(""),                    // GF_SG_ROUTE_DELETE
                        wxT("Insert Node"),         // GF_SG_NODE_INSERT
                        wxT("")                     // GF_SG_INDEXED_INSERT
                        // TODO : to complete
                      };


// Events table
BEGIN_EVENT_TABLE(V4CommandPanel, wxPanel)
  EVT_PAINT(V4CommandPanel::OnPaint)
  EVT_COMBOBOX(cmbCommandID, V4CommandPanel::OnCommandCombo)
  EVT_COMBOBOX(cmbFieldID, V4CommandPanel::OnFieldCombo)
  EVT_BUTTON(CreateID, V4CommandPanel::OnCreateCommand)
END_EVENT_TABLE()



// Constructor
V4CommandPanel::V4CommandPanel(V4StudioFrame * parent_) 
	: wxPanel(parent_, -1), 
	// tabs
	tabs(this, -1), 
	tabView(&tabs, -1), 
	tabAdd(&tabs, -1),

	// add Page
	cmbCommands(&tabAdd, cmbCommandID, ""),
	lblCommands(&tabAdd, -1, "Action :"),
	cmbNodes(&tabAdd, -1, ""),
	lblNodes(&tabAdd, -1, "Node :"), 
	cmbFields(&tabAdd ,cmbFieldID, ""),
	lblFields(&tabAdd, -1, "Field :"),
	txtField(&tabAdd, -1, ""),
	btnCreate(&tabAdd, CreateID, "Create"),

	// view Page
	cmbListCommands(&tabView, -1),
	txtDesc(&tabView, -1),
	btnDelete(&tabView, -1, "Delete")
{
	parent = parent_;

	// disposition

	// global
	sizerTabs = new wxBoxSizer(wxHORIZONTAL);
	sizerTabs->Add(&tabs, 1, wxEXPAND);

	tabs.AddPage(&tabView, "View");
	tabs.AddPage(&tabAdd, "Add");


	// Add Page
	sizerAdd = new wxBoxSizer(wxVERTICAL); // up = combobox, middle = differs with the command, bottom = buttons

	sizerU = new wxBoxSizer(wxHORIZONTAL); // up
	sizerM = new wxBoxSizer(wxVERTICAL);   // middle
	sizerD = new wxBoxSizer(wxHORIZONTAL); // bottom

	szSelectNode = new wxBoxSizer(wxHORIZONTAL);
	szSelectField = new wxBoxSizer(wxHORIZONTAL);
	szTxtField = new wxBoxSizer(wxHORIZONTAL);

	sizerAdd->Add(sizerU, 1, wxEXPAND);
	sizerAdd->Add(sizerM, 3, wxEXPAND);
	sizerAdd->Add(sizerD, 1, wxEXPAND);

	sizerU->Add(&lblCommands, 1, wxEXPAND | wxALL, 5);
	sizerU->Add(&cmbCommands, 3, wxEXPAND | wxALL, 2);

	szSelectField->Add(&lblFields, 1, wxEXPAND | wxALL, 5);
	szSelectField->Add(&cmbFields, 2, wxEXPAND | wxALL, 2);

	szSelectNode->Add(&lblNodes, 1, wxEXPAND | wxALL, 5);
	szSelectNode->Add(&cmbNodes, 2, wxEXPAND | wxALL, 2);

	szTxtField->Add(&txtField, 1, wxEXPAND | wxALL, 5);

	sizerD->Add(&btnCreate, 1, wxEXPAND | wxALL, 3);

	tabAdd.SetSizer(sizerAdd);



	// View Page
	sizerView = new wxBoxSizer(wxVERTICAL);
	sizerUp   = new wxBoxSizer(wxHORIZONTAL);
	sizerDown = new wxBoxSizer(wxHORIZONTAL);

	sizerView->Add(sizerUp, 0, wxEXPAND);
	sizerView->Add(sizerDown, 4, wxEXPAND);

	sizerUp->Add(&cmbListCommands, 2, wxEXPAND | wxALL, 2);
	sizerUp->Add(&btnDelete, 1, wxEXPAND | wxALL, 2);

	sizerDown->Add(&txtDesc, 1, wxEXPAND);

	tabView.SetSizer(sizerView);



	// global
	SetSizer(sizerTabs);

	//Layout();

	ShowFieldSizer(false);
	ShowNodeSizer(false);
	txtField.Show(false);
}

// Destructor
V4CommandPanel::~V4CommandPanel() {
	delete szSelectNode;
	delete szSelectField;
	delete szTxtField;
};

/************************/
/*        Events        */
/************************/

// OnPaint event
void V4CommandPanel::OnPaint(wxPaintEvent& event) {
	wxPaintDC dc(this);

	dc.BeginDrawing();

	// draws a seperator line on the left border
	int w,h;
	this->GetSize(&w, &h);
	dc.DrawLine(0, 0, 0, h);

	dc.EndDrawing();
}


// OnCommandCombo -- command combo box selection has changed
void V4CommandPanel::OnCommandCombo(wxCommandEvent &event) {
  // retrieves the command id
  u32 command = (u32) cmbCommands.GetClientData(cmbCommands.GetSelection());

  // retrieves the current node
  GF_Node * node = GetCurrentNode();
  
  if (! IsCommandValidForNode(command, node)) {
    cmbCommands.SetSelection(-1);
    command = (u32) -1;
  }

  // shows or hides components
  switch (command) {
    case GF_SG_FIELD_REPLACE: {
      ShowNodeSizer(false);
      ShowFieldSizer(true);
      break;
    }

    case GF_SG_NODE_REPLACE:
    case GF_SG_NODE_INSERT: {
      ShowFieldSizer(false);
      ShowNodeSizer(true);
      break;
    }

    case GF_SG_NODE_DELETE:
    default : {
      ShowFieldSizer(false);
      ShowNodeSizer(false);
    }
  }
}


// OnFieldCombo -- field combo box selection has changed
void V4CommandPanel::OnFieldCombo(wxCommandEvent &event) {

  // if no selection exits
  u32 sel = cmbFields.GetSelection();
  if (sel == wxNOT_FOUND) return;


  // gets selected node in the timeline
  GF_Node * node = GetCurrentNode();
  if (!node) return;

  GF_FieldInfo field;
  gf_node_get_field(node, sel, &field);

  // show or hides elements depending on the type of the field we will modify
  if ( (field.fieldType == GF_SG_VRML_SFNODE) || (field.fieldType == GF_SG_VRML_MFNODE) ) {
    sizerM->Detach(szTxtField);
    txtField.Show(false);
    ShowNodeSizer(true);
  } else {
    sizerM->Detach(szTxtField);
    sizerM->Add(szTxtField, 2, wxEXPAND);
    txtField.Show(true);
  }

}


// OnCreateCommand -- creates a command with the option from the UI
void V4CommandPanel::OnCreateCommand(wxCommandEvent &event) {

  // verifies that there is a valid command value on cmbCommands
  u32 sel = cmbCommands.GetSelection();
  if (sel == wxNOT_FOUND) return;

  // gets data from the UI
  u32 tag = (u32) cmbCommands.GetClientData(sel);
  GF_Node * node = GetCurrentNode();

  // Creates the new command - REQUIRES registering the node
  GF_Command * c = gf_sg_command_new(parent->GetV4SceneManager()->GetSceneGraph(), tag);
  gf_node_register(node, NULL);
  c->node = node; // !! the node have to be registered if command is validated

  bool succeed = false;;

  // performs various initialization depending  on the command tag
  switch (tag) {
    case GF_SG_NODE_REPLACE: {
      // TODO : not implemented yet because il would even replace the nodes in the dictionnary
      break;
    }

    case GF_SG_FIELD_REPLACE: {
      // verifies that a valid field is selected
      u32 selF = cmbFields.GetSelection();
      if (selF == wxNOT_FOUND) break;

      // gets the field
      GF_FieldInfo field;
      gf_node_get_field(node, (u32)cmbFields.GetClientData(selF), &field);

      GF_CommandField * cmdField = gf_sg_command_field_new(c); // if failure, will be freed with freeing the command
      cmdField->fieldIndex = field.fieldIndex;
      cmdField->fieldType  = field.fieldType;
      
      // fills the GF_CommandField structures with data depending on the field type
      switch (field.fieldType) {
        case GF_SG_VRML_SFNODE: {
          // get the node we will use as a replacement
          u32 selN = cmbNodes.GetSelection();
          if (selN == wxNOT_FOUND) break;

          cmdField->new_node = (GF_Node *) cmbNodes.GetClientData(selN);
          // TODO : why is the 2nd line necessary ?
          cmdField->field_ptr = &cmdField->new_node;

          succeed = true;
          break;
        }

        case GF_SG_VRML_MFNODE: {
          break;
        }

        // field is not a node field
        default: {

          wxString s = txtField.GetValue();
          if (s.IsEmpty()) break;

          GF_FieldInfo dummy;
          dummy.far_ptr = gf_sg_vrml_field_pointer_new(field.fieldType);
          dummy.fieldType = field.fieldType;
          parent->GetFieldView()->SetFieldValue(dummy, &s, 0); // TODO : check the zero

          cmdField->field_ptr = dummy.far_ptr;

          succeed = true;

          break;
        }

      }
      
      break;
    }

    case GF_SG_NODE_INSERT: {
      break;
    }

    case GF_SG_NODE_DELETE: {
      break;
    }
  }

  if (!succeed) {
    gf_sg_command_del(c);
    return;
  }


  V4SceneManager * sm = parent->GetV4SceneManager();

  // The command is created, now searching for an AU to put it in
  GF_StreamContext * ctx = sm->GetBifsStream();
  u32 count = gf_list_count (ctx->AUs);
  GF_AUContext * au = NULL;

  for (u32 i = 0; i < count; i++) {
     au = (GF_AUContext *) gf_list_get(ctx->AUs, i);
     // V4Studio only uses timing
     // TODO : FPS is defined in V4SceneGraph.h, should change and become a property somewhere
     // TODO : units is a constant
     if ( au->timing == sm->GetUnits() / sm->GetFrameRate() * frame ) break;
     au = NULL;
  }

  // creates new AU at the right time if none found
  if (!au) au = gf_sm_stream_au_new(ctx, sm->GetUnits() / sm->GetFrameRate() * frame, 0, false);

  // adds command to AU and to the cell
  gf_list_add(au->commands, (void *) c);
  parent->GetTimeLine()->AddCommand(c);

  //gf_sg_command_apply(parent->GetV4Scene()->GetSceneGraph(), c, 0);


/*
  // checks whether there is already a command for this object
  GF_List * chain = parent->timeLine->GetCommands();

  // if we can fuse the two commands together, we do it
  if (gf_list_count(chain) > 0) {
    GF_Command * old;
    old = (GF_Command *) gf_list_get(chain, gf_list_count(chain) - 1);
    if ( (old->tag = 
  }
*/


}


/************************/
/*       Refresh        */
/************************/

// refresh -- refresh the node and the command combo boxes
void V4CommandPanel::Refresh(u32 frame_) {

  frame = frame_;

  // update the GUI
  // cmbCommands updates in turns cmbNodes and/or cmbFields
  RefreshCommands();

  // update the list of the existing command for the cell currently selected
  RefreshListCommands();
}


//  RefreshListCommands --
void V4CommandPanel::RefreshListCommands() {
  // retrieves pointer to the node
  GF_Node * node = GetCurrentNode();
  if (!node) return;

  cmbListCommands.Clear();

  GF_Command * c;
  int i=0;

  while ( c = parent->GetTimeLine()->GetCommand(i) ) {
    cmbListCommands.Append(wxString(cmdNames[c->tag]), (void *) i);
    i++;
  }
}


// RefreshCommands -- lists the available commands for the selected node
void V4CommandPanel::RefreshCommands() {

  // retrieves pointer to the node
  GF_Node * node = GetCurrentNode();
  if (!node) return;

  u32 oldSel = cmbCommands.GetSelection();

  // deletes all items from the combo box
  cmbCommands.Clear();

  // Prints the different possible commands, associates them with their id
  for (int i=0; i<sizeof(cmdNames)/sizeof(wxString); i++) {
    if ( cmdNames[i].CompareTo(wxT("")) ) 
      if (IsCommandValidForNode(i, node)) cmbCommands.Append(cmdNames[i], (void *)i);
  }

  // updates other controls by signaling a new selection on the command combo box
  cmbCommands.SetSelection(oldSel);
  OnCommandCombo(wxCommandEvent());
}


// RefreshNodes -- lists the available nodes for the given action
void V4CommandPanel::RefreshNodes() {

  // retrieves pointer to the node
  GF_Node * node = GetCurrentNode();
  if (!node) return;

  // refresh the node combo box with the nodePool
  cmbNodes.Clear();


  // Adds nodes selectable for this frame and action (selectable nodes have a NodeID != 0)
  // associates in the comboBox each element with its pointer

  u32 command = (u32) cmbCommands.GetClientData(cmbCommands.GetSelection());


  // according to command type selets a node pool
  switch (command) {

    case GF_SG_NODE_REPLACE: {
      // we replace this node, so we need the node from the pool for its type
      V4NodePool &pool = parent->GetV4SceneManager()->pools.pool(gf_node_get_tag(node));

      // fills the combo box
      PopulateNodes(pool, node);

      break;
    }

    case GF_SG_FIELD_REPLACE: {
      // checks if the field combo box has a valid selection
      u32 index = cmbFields.GetSelection();
      if ( index == wxNOT_FOUND ) break;

      // get the field selected
      GF_FieldInfo field;
      gf_node_get_field(node, index, &field);

      // if field is not a node field then exits
      if ( (field.fieldType != GF_SG_VRML_SFNODE) && (field.fieldType != GF_SG_VRML_MFNODE) ) break;

      // get the pool corresponding to that field
      V4NodePool &pool = parent->GetV4SceneManager()->pools.poolFromFieldName(field.name);

      // fills the combo box
      PopulateNodes(pool, node);      

      break;
    }

    case GF_SG_NODE_INSERT: {
      // we add a child, that is to say we can add any generic node, we use "shape" to find that pool
      V4NodePool &pool = parent->GetV4SceneManager()->pools.pool(TAG_MPEG4_Shape);

      // fills the combo box
      PopulateNodes(pool, node);

      break;
    }

    default: {
      return;
    }
  }

}


// RefreshFields -- lists the fields this nodes has
void V4CommandPanel::RefreshFields() {

  // retrieves pointer to the node
  GF_Node * node = GetCurrentNode();
  if (!node) return;

  // clears the combo box
  cmbFields.Clear();

  int count = gf_node_get_field_count(node);
  GF_FieldInfo field;

  for (u32 i = 0; i < count; i++) {
    gf_node_get_field(node, i, &field);
    cmbFields.Append(field.name, (void *) i);
  }

  // TODO : are there some nodes with no fields ?
  cmbFields.SetSelection(0);

  OnFieldCombo(wxCommandEvent());

}


// ShowNodeSizer -- shows or hides controls in the szSelectNode sizer
void V4CommandPanel::ShowNodeSizer(bool show) {
  cmbNodes.Show(show);
  lblNodes.Show(show);

  // always detaches to avoid accumulation of objects
  sizerM->Detach(szSelectNode);

  if (show) {
    sizerM->Add(szSelectNode, 1, wxEXPAND);
    RefreshNodes();
  }

  tabAdd.Layout();
}



// ShowFieldSizer -- shows or hides controls in the szSelectField sizer
void V4CommandPanel::ShowFieldSizer(bool show) {
  cmbFields.Show(show);
  lblFields.Show(show);
  txtField.Show(false); // always hide, RefreshField may bring it back

  // always detaches to avoid accumulation of objects
  sizerM->Detach(szSelectField);
  sizerM->Detach(szTxtField);

  if (show) {
    // here order matters, RefreshFields may add szSelectNode, so we have to had ourselves before that
    sizerM->Add(szSelectField, 1, wxEXPAND);
    RefreshFields();
  }

  tabAdd.Layout();
}


/************************/
/*        Utils         */
/************************/

// IsCommandValidForNode -- tells if we can apply this command to that node
bool V4CommandPanel::IsCommandValidForNode(u32 command, GF_Node * node) {
  switch (command) {
    case GF_SG_FIELD_REPLACE:
    case GF_SG_NODE_DELETE:
    case GF_SG_NODE_REPLACE: {
      return true;
      break;
    }

    case GF_SG_NODE_INSERT: {
      // to insert a child, node must have a GF_SG_VRML_MFNODE field, other children are not "inserted"
      return parent->GetV4SceneManager()->pools.NodeHasField(node, GF_SG_VRML_MFNODE);
      break;
    }

  }

  return false;
}


// GetCurrentNode -- returns the node currently selected in the timeline
GF_Node * V4CommandPanel::GetCurrentNode() {
  u32 id = parent->GetTimeLine()->GetSelectedID();
  if (!id) return NULL;

  return gf_sg_find_node(parent->GetV4SceneManager()->GetSceneGraph(), id);
}



// PopulateNodes -- fills the cmdNodes with nodes from pool, do not add the specified node if found
void V4CommandPanel::PopulateNodes(V4NodePool& pool, GF_Node * node) {
  for (u32 i = 0; i != pool.GetNodesCount(); i++)
    if (pool.at(i).GetNode() != node)
      cmbNodes.Append(pool.at(i).GetName(), (void *) pool.at(i).GetNode());
}