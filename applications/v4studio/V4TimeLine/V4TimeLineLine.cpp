/*
  V4TimeLineLine.cpp

    Implements V4TimeLineLine class

*/

#include "V4TimeLineLine.h"
#include "V4TimeLine.h"

// Constructor
V4TimeLineLine::V4TimeLineLine(V4TimeLine * parent, unsigned int _pos, unsigned long frame) : wxWindow((wxWindow *) parent, -1), pos(_pos) {
}


// retrieves X position of the grid
unsigned int V4TimeLineLine::GetOffset() const {
  return ( (V4TimeLine *)GetParent() )->GetOffset();
}


// returns the number of frame in the scene
unsigned int V4TimeLineLine::GetLength() const {
  return ( (V4TimeLine *)GetParent() )->GetLength();
}


// returns the position of the line in the timeLine
unsigned int V4TimeLineLine::GetPos() const {
  return pos;
}


// Update diplay to match new time
void V4TimeLineLine::SetFrame(unsigned long _frame) {
  std::vector<V4TimeLineCell *>::iterator iter;

  int i = 0;

  // uptades the needed cells

  for (iter = cells.begin(); iter != cells.end(); iter++) {
    // newly selected cell, adds selected state
    if (i == _frame) {
      unsigned int state = (*iter)->GetState();
      state |= CELL_STATE_SELECTED;
      (*iter)->SetState(state);
    }

    // old selected cell, deletes selected state
    if ( (i == frame) && (_frame != frame) ) {
      unsigned int state = (*iter)->GetState();
      state &= ~CELL_STATE_SELECTED;
      (*iter)->SetState(state);
    }

    i++;
  }

  frame = _frame;
}


// Retrieves frame
unsigned long V4TimeLineLine::GetFrame() const {
  return frame;
}


//
void V4TimeLineLine::SetLength(const unsigned int length_) {
  int L = GetLength();

  V4TimeLine * line = (V4TimeLine *) GetParent();
  SetSizeHints(line->GetOffset() + 20 * length_, -1);
  line->Layout();

  if (L <= length_) {

    for (int i = L; i < length_; i++) {
      cells.push_back(new V4TimeLineCell(this,i, GetType()));
      sizer->Add(cells.back(), 1, wxALL, 0);
    }
  } else {
    for (int i = L-1; i >= length_; i--) {
      if (cells.back()->GetCommand(0)) cells.back()->DeleteCommands();
      delete cells.back();
      cells.pop_back();
    } 
  }

}