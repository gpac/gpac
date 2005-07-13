/*
  V4TimeLineLine.h

    Defines generic behaviour for a line in the timeline
    There are two child classes: V4TimeLineElt and V4TimeLineHdr

*/

#ifndef _V4TimeLineLine_
#define _V4TimeLineLine_

#include "../safe_include.h"

#include <wx/wx.h>
#include <vector>
#include "V4TimeLineCase.h"


#define LINE_TYPE_HDR 0
#define LINE_TYPE_ELT 1

class V4TimeLine;


// macro that creates the sizer and the grid for a line
#define CreateLine(CELL_TYPE) \
  SetSizeHints(parent->GetOffset() + 20 * GetLength(), -1); \
  \
  for (u32 i=0; i<GetLength(); i++) { \
    cells.push_back(new V4TimeLineCell(this,i, CELL_TYPE)); \
    sizer->Add(cells.back(), 1, wxALL, 0); \
  } \
  SetFrame(frame);


class V4TimeLineLine : public wxWindow {
  public:
    // Constructor
    V4TimeLineLine(V4TimeLine * parent, unsigned int _pos, unsigned long frame=0);
  
    // retrieves the X positon of the grid
    unsigned int GetOffset() const;

    // returns the Y position of the line
    unsigned int GetPos() const;

    // accesses the length of the grid
    unsigned int GetLength() const;
    void SetLength(const unsigned int length_);

    // Update display to match new time
    void SetFrame(unsigned long _frame);
    unsigned long GetFrame() const;

    virtual unsigned char GetType() PURE;


  protected:
    std::vector<V4TimeLineCell *> cells; // cases of the grid
    wxBoxSizer * sizer;

    unsigned int pos; // Y position of the line in the grid

    unsigned long frame; // current frame

};

#endif