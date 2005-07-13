#include "safe_include.h" 

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
  #include "wx/wx.h"
#endif

#include "V4StudioApp.h"
#include "V4StudioFrame.h"
#include <wx/image.h>
//#include <wx/xrc/xmlres.h>

IMPLEMENT_APP(V4StudioApp)

bool V4StudioApp::OnInit()
{
	wxFrame *frame = new V4StudioFrame();
	frame->Show(TRUE);
	SetTopWindow(frame);
	return true;
}

