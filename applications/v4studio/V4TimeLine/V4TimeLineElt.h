/*
  V4TimeLineElt.h

    Class representing one line in the timeline

    A line consists in a label, giving the name of the object it represents, and an owner drawn zone where are represented the events
*/

#ifndef _V4TimeLineElt_
#define _V4TimeLineElt_ 

#include "../safe_include.h"

// wxWidgets includes
#include <wx/wx.h>

// Own includes
#include "V4TimeLineLine.h"


// include GPAC
//#include "../safe_include.h" // definition des types de données


class V4TimeLine;


class V4TimeLineElt : public V4TimeLineLine {
  public:
	// Constructor
	V4TimeLineElt(V4TimeLine * parent, unsigned int pos, unsigned long NodeID, wxString eltName, unsigned long frame=0);

	// Destructor
	~V4TimeLineElt();

  // inherited
  virtual unsigned char GetType();

  // returns NodeID
  u32 GetNodeID();

  private:
	  wxTextCtrl * txt;
	  wxStaticText * lbl;
    unsigned long NodeID;
};

#endif