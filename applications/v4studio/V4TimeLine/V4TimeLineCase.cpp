/*
  V4TimeLineCase.cpp

*/

#include "V4TimeLineCase.h"
#include "V4TimeLineLine.h"
#include "V4TimeLine.h"

#include "..\V4StudioFrame.h"
#include "..\V4SceneManager.h"


BEGIN_EVENT_TABLE(V4TimeLineCell, wxWindow)
  EVT_PAINT(V4TimeLineCell::OnPaint)
  EVT_LEFT_UP(V4TimeLineCell::OnLeftUp)
END_EVENT_TABLE()


// Constructor
V4TimeLineCell::V4TimeLineCell(V4TimeLineLine * parent_, unsigned int _num, unsigned int _type) : wxWindow(parent_, -1), num(_num), state(0), type(_type) {

  parent = parent_;
  commands = gf_list_new();

  SetSize(parent_->GetOffset() + _num * 20, 0, -1, -1);

  // line, timeline, frame
  v4sf = (V4StudioFrame *) parent->GetParent()->GetParent();

}


// destructor
V4TimeLineCell::~V4TimeLineCell() {
  gf_list_del(commands);
}


// OnPaint
void V4TimeLineCell::OnPaint(wxPaintEvent & event) {
  wxPaintDC dc(this);

  dc.BeginDrawing();

    int w, h;
    GetSize(&w, &h);

    // draws the background
    dc.SetPen(*wxTRANSPARENT_PEN);

    // finds a color according to the parity
    unsigned char r,g,b;
    if ( ((V4TimeLineLine *)GetParent())->GetPos() % 2 ) { r = 220; g = 220; b = 255; }
    else { r = 200; g = 200; b = 255; };

    // if we are selected (ie current frame) adds green
    if (state & CELL_STATE_SELECTED)
      g += 35;

    wxColour c(r,g,b);

    wxBrush br(c);
    dc.SetBrush( br );

    dc.DrawRectangle(0,0,w,h);

    dc.SetBrush(*wxWHITE_BRUSH);

    // draws the vertical borders
    dc.SetPen(*wxLIGHT_GREY_PEN);
    dc.DrawLine(0,0,0,h);

    dc.SetPen(*wxBLACK_PEN);

    // draws state specific information
    if (state & CELL_STATE_CLICKED) dc.DrawCircle(w/2,h/2,10);
    if (state & CELL_STATE_COMMAND) dc.DrawRectangle(2, 2, w-4, h-4);

    // draws frame number
    wxString s;
    if ( (type == CELL_TYPE_HDR) && (! (num % 10)) ) {
      s = s.Format(wxT("%d"), num);
      // centers text
      int wt, ht;
      dc.GetTextExtent(s, &wt, &ht);
      dc.DrawText(s, (w-wt)/2, (h-ht)/2);
    }

  dc.EndDrawing();
}


// OnLeftUp -- tells the application that the user switched frame
void V4TimeLineCell::OnLeftUp(wxMouseEvent & event) {

  ((V4TimeLine *) GetParent()->GetParent())->SetFrame(num, this);

  V4SceneManager * sg = v4sf->GetV4SceneManager();

  // don't trigger any more reaction if this is a cell in the first row
  //if (type == CELL_TYPE_HDR) return;

  Refresh();
  v4sf->Update();
}


// SetState -- Changes cell state
void V4TimeLineCell::SetState(unsigned int _state) {
  state = _state;

  // if cell is clicked but not selected, can't be clicked anymore
  if ( (state & CELL_STATE_CLICKED) && !(state & CELL_STATE_SELECTED) )
    state &= ~CELL_STATE_CLICKED;

  Refresh();
}


// GetState -- Retrieves cells state
unsigned int V4TimeLineCell::GetState() const {
  return state;
}


// AddCommand -- 
void V4TimeLineCell::AddCommand(GF_Command *c) {
  gf_list_add(commands, c);
  SetState( GetState() | CELL_STATE_COMMAND );
}

// DeleteCommand -- Deletes ONE command
void V4TimeLineCell::DeleteCommand(const u32 n) {
  // TODO :
}


// DeleteCommand -- destroys all the commands from this cell (for instance when shortening the scene)
// ONLY works for NORMAL cells
void V4TimeLineCell::DeleteCommands() {

  // deletes the commands from the au

  // useful objects
  V4SceneManager *sm = v4sf->GetV4SceneManager();
  GF_StreamContext *ctx = sm->GetBifsStream();
  GF_AUContext * au = NULL;
  GF_Node * myNode = gf_sg_find_node( sm->GetSceneGraph(), ((V4TimeLineElt *) parent)->GetNodeID() );

  u32 myTiming = sm->GetUnits() / sm->GetFrameRate() * num;

  // first, locates the AU
  for (int i = 0; i < gf_list_count(ctx->AUs); i++) {
    au = (GF_AUContext *) gf_list_get(ctx->AUs, i);
    if (au->timing == myTiming) {
      // then the commands for this node and deletes it
      for (int j = 0; j < gf_list_count(au->commands); j++) {
        GF_Command * c = (GF_Command *)gf_list_get(au->commands, j);
        if ( c->node == myNode ) {
          gf_sg_command_del(c);
          gf_list_rem(au->commands, j);
          break;
        }
      }
    }
  }

  // deletes the command in our chain
  gf_list_reset(commands);

  SetState( GetState() ^ CELL_STATE_COMMAND );
}


// GetCommand -- allow listing of the command for this cell
GF_Command * V4TimeLineCell::GetCommand(u32 n) {
  if (gf_list_count(commands) < n) return NULL;
  else return (GF_Command *) gf_list_get(commands, n);
}


// GetParent -- returns a pointer to parent (used to retrieve line number and node ID)
V4TimeLineLine * V4TimeLineCell::GetParent() const {
  return parent;
}
