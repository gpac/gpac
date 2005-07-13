/*
  V4TimeLineHdr.h

  Defines the class for printing hearder information on the timeline (frame numbers...)
*/

#ifndef _V4TimeLineHdr_
#define _V4TimeLineHdr_

#include "../safe_include.h"

#include <wx/wx.h>
#include "V4TimeLineLine.h"  // parent class

class V4TimeLine;


class V4TimeLineHdr : public V4TimeLineLine {
  public:
    // Constructor 
    V4TimeLineHdr(V4TimeLine * parent, unsigned int pos=0);

    // Destructor
    ~V4TimeLineHdr();

    // inherited
    virtual unsigned char GetType();

  private:

    wxStaticText * lbl;  // holds the title string

    DECLARE_EVENT_TABLE();
};

#endif
