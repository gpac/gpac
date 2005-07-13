/*
  V4TimeLineElt.cpp

	Implements V4TimeLineElt class
*/

#include "V4TimeLineElt.h"
#include "V4TimeLine.h"
#include "V4TimeLineCase.h"

#include "..\V4StudioFrame.h"
#include "..\V4SceneManager.h"  // FPS constant


// Constructor
V4TimeLineElt::V4TimeLineElt(V4TimeLine * parent, unsigned int pos, unsigned long _NodeID, wxString eltName, unsigned long frame) :
V4TimeLineLine(parent, pos, frame), txt(NULL), lbl(NULL) {
  // creates sub-controls

  NodeID = _NodeID;

  sizer = new wxBoxSizer(wxHORIZONTAL);
  SetSizer(sizer);

  lbl = new wxStaticText(this, -1, eltName);
  lbl->SetSize(0,0,parent->GetOffset(), 20);


  sizer->Add(lbl, 0, wxALL, 0);

  CreateLine(CELL_TYPE_NORMAL);


  // locates the existing commands for a node
  GF_StreamContext * stream = parent->parent->GetV4SceneManager()->GetBifsStream();

  // tries all access units
  for (i=0; i < gf_list_count(stream->AUs); i++) {
    GF_AUContext * au = (GF_AUContext *) gf_list_get(stream->AUs, i);

    // and all commands in each au
    for (u32 j=0; j < gf_list_count(au->commands); j++) {
      GF_Command * c = (GF_Command *) gf_list_get(au->commands, j);

      // and checks whether they are appled to this node
      if (gf_node_get_id(c->node) == NodeID) {
        // TODO : the timing unit might not be 1 ms
        u32 cellNum = au->timing / 1000 * parent->parent->GetV4SceneManager()->GetFrameRate();
        cells.at(cellNum)->SetState(CELL_STATE_COMMAND);
        cells.at(cellNum)->AddCommand(c);
      }
    }

  }

}


// Destructor
V4TimeLineElt::~V4TimeLineElt() {

}


// returns NodeID
u32 V4TimeLineElt::GetNodeID() {
  return NodeID;
}


//
unsigned char V4TimeLineElt::GetType() {
  return CELL_TYPE_NORMAL;
}