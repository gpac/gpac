/*
  V4TimeLineHdr.cpp

    Implements V4TimeLineHdr class.
*/

#include "V4TimeLineHdr.h"
#include "V4TimeLine.h"
#include <wx/dcclient.h>

BEGIN_EVENT_TABLE(V4TimeLineHdr, wxWindow)
END_EVENT_TABLE()


// Constructor
V4TimeLineHdr::V4TimeLineHdr(V4TimeLine * parent, unsigned int pos) : V4TimeLineLine(parent, pos) {

  // Creates default line
  sizer = new wxBoxSizer(wxHORIZONTAL);
  SetSizer(sizer);
  lbl = new wxStaticText(this, -1, wxT("Object"));
  lbl->SetSize(0,0,parent->GetOffset(), 20);
  sizer->Add(lbl, 0, wxALL, 0);

  CreateLine(CELL_TYPE_HDR);

}


// Desctructor
V4TimeLineHdr::~V4TimeLineHdr() {};


//
unsigned char V4TimeLineHdr::GetType() {
  return CELL_TYPE_HDR;
}