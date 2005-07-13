/*
	V4TimeLine.cpp

	Implémentation de la classe V4TimeLine
*/

#include "V4TimeLine.h"
#include "V4TimeLineCase.h"
#include "..\V4StudioFrame.h"


// Constructeur
V4TimeLine::V4TimeLine(wxWindow * parent_, unsigned int timeLineLength) : wxScrolledWindow(parent_), sizer(NULL) {

  // position en X de la grille
  Offset = 100;
  Length = timeLineLength;
  clicked = NULL;

  parent = (V4StudioFrame *) parent_;

  sizer = new wxBoxSizer(wxVERTICAL);

  // indispensable pour avoir des barres de défilement
  SetScrollRate( 5, 5 );

  hdr = new V4TimeLineHdr(this);
  sizer->Add(hdr);

  SetSizer(sizer);
  FitInside();
}


// Destructeur
V4TimeLine::~V4TimeLine() {
}


// retourne la position en X du debut de la grille
int V4TimeLine::GetOffset() {
  return Offset;
}


// returns Length attribute
unsigned int V4TimeLine::GetLength() {
  return Length;
}


// Adds a line to the timeline
void V4TimeLine::AddLine(u32 NodeID, wxString eltName) {
  // verifies that we have no line with the same NodeID

  for (int i = 0; i < lines.size(); i++) {
    if (lines.at(i)->GetNodeID() == NodeID)
      return;
  }

  // adds the line at the end of the list, in the next position
  // note that we already have the hearder line in position 0
  lines.push_back(
    new V4TimeLineElt(this, lines.size()+1, NodeID, eltName, hdr->GetFrame())
   );

  // redraws the component
  sizer->Add(lines.back());
  Layout();
}


// called when time changed, update display (background colors)
void V4TimeLine::SetFrame(u32 _frame, V4TimeLineCell * newClicked) {

  // updates lines display
  if (_frame != hdr->GetFrame()) {
    hdr->SetFrame(_frame);

    // TODO : apply all commands from previous frames.
    // maybe by adding a NextFrame() method

    for (int i = 0; i < lines.size(); i++)
      lines.at(i)->SetFrame(_frame);
  }


  // updates clicked state on clicked cells

  // removes state from old
  if (clicked != NULL) {
    u32 state = clicked->GetState();
    state &= ~CELL_STATE_CLICKED;
    clicked->SetState(state);
  }

  clicked = newClicked;

  // adds state to new
  if (clicked != NULL) {
    u32 state = clicked->GetState();
    state |= CELL_STATE_CLICKED;
    clicked->SetState(state);
  }


  // updates command panel
  ((V4StudioFrame *) GetParent())->GetCmdPanel()->Refresh(_frame);

}


// GetSelectedID -- returns the ID of the node whose timeline is selected
u32 V4TimeLine::GetSelectedID() const {

  // if nothing is selected then returns null nodeID
  if (!clicked) return 0;

  // gets the line from the cell
  u32 pos = clicked->GetParent()->GetPos();

  // gets the node id (!! the header line is first)
  if (pos) return lines.at(pos-1)->GetNodeID();
  else return 0; // if we clicked on a header cell
}


// AddCommand -- 
void V4TimeLine::AddCommand(GF_Command * c) {
  clicked->AddCommand(c);
}


// DeleteCommand -- 
void V4TimeLine::DeleteCommand(u32 n) {
  clicked->DeleteCommand(n);
}


// GetCommand -- 
GF_Command * V4TimeLine::GetCommand(u32 n) {
  return clicked->GetCommand(n);
}


// SetLength -- 
void V4TimeLine::SetLength(const unsigned int length_) {
  hdr->SetLength(length_);

  // if the selected frames will be deleted, we select the last cell
  if (hdr->GetFrame() > length_) SetFrame(length_);

  for (int i = 0; i < lines.size(); i++)
    lines.at(i)->SetLength(length_);

  Length = length_;

  FitInside();
}