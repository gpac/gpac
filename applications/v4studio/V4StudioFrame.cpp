#include "safe_include.h" 

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
  #include "wx/wx.h"
#endif

#include "V4StudioFrame.h"
#include "wx/xrc/xmlres.h"


#include <gpac/internal/scenegraph_dev.h>
#include <gpac/internal/terminal_dev.h> // GF_Terminal
#include <gpac/nodes_mpeg4.h>

V4StudioFrame::V4StudioFrame():
    wxFrame((wxFrame *) NULL, -1, "V4Studio", wxPoint(50, 50), wxSize(800, 700))
{

	m_pV4sm = NULL;

	m_selection = NULL;
	m_parentSelection = NULL;
	m_clipboardNode = NULL;

	m_frame = 0; // we start at frame=0
	editDict = false;

	// Creates components and places them on the form
	fieldView = new V4FieldList(this, wxSize(100,250));
	treeView = new V4StudioTree(this, wxSize(100,250), fieldView);
	timeLine = new V4TimeLine(this);
	timeLine->SetSize(500, 100);
	cmdPanel = new V4CommandPanel(this);
	cmdPanel->SetSize(100, 100);

	/*new m_pFileMenu bar*/
	wxMenuBar *b = new wxMenuBar();
	/*file*/
	m_pFileMenu = new wxMenu();
	m_pFileMenu->Append(MENU_FILE_NEW, "&New\tCtrl+N", "Create a new document");
	m_pFileMenu->Append(MENU_FILE_OPEN, "&Open...\tCtrl+O", "Open an existing document");
	m_pFileMenu->Append(MENU_FILE_SAVE, "&Save\tCtrl+S", "Save the active document");
	m_pFileMenu->Append(MENU_FILE_CLOSE, "&Close\tCtrl+X", "Close the active document");
	m_pFileMenu->AppendSeparator();
	m_pFileMenu->Append(CHANGE_SIZE_DIALOG, "&Size\tCtrl+Z", "Change scene size");
	m_pFileMenu->Append(CHANGE_FRAMERATE, "&FrameRate\tCtrl+F", "Change FrameRate");
	m_pFileMenu->Append(CHANGE_LENGTH, "&Length\tCtrl+L", "Change Length");
	m_pFileMenu->AppendSeparator();
	m_pFileMenu->Append(MENU_FILE_QUIT, "E&xit", "Quit the application; prompts to save documents");
	b->Append(m_pFileMenu, "&File");
	SetMenuBar(b);

	/*file*/
	m_pToolsMenu = new wxMenu();
	m_pToolsMenu->Append(MENU_TOOL_SHOW_LOWLEVEL, "&Low-Level\tCtrl+L", "Shows the low-level toolbar");
	m_pToolsMenu->Append(MENU_TOOL_SHOW_HIGHLEVEL, "&High-Level\tCtrl+H", "Shows the high-level toolbar");
	b->Append(m_pToolsMenu, "&Tools");
	SetMenuBar(b);

	m_pStatusbar = CreateStatusBar();

	// Main Toolbar
	m_pMainToolbar = new wxToolBar(this, -1, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL|wxTB_FLAT);
	m_pMainToolbar->AddTool(TOOL_FILE_NEW, _("New"), wxBitmap (wxT("rc\\new.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Open a new file"), wxT(""));
	m_pMainToolbar->AddTool(TOOL_FILE_OPEN, _("Open"), wxBitmap (wxT("rc\\open.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Open an existing file (BT/XMT/MP4)"), wxT(""));
	m_pMainToolbar->AddTool(TOOL_FILE_SAVE, _("Save"), wxBitmap (wxT("rc\\save.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Save to a file"), wxT(""));
	m_pMainToolbar->AddTool(TOOL_FILE_CLOSE, _("Close"), wxBitmap (wxT("rc\\close.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Close the file"), wxT(""));
	m_pMainToolbar->AddSeparator();
	m_pMainToolbar->AddTool(TOOL_FILE_PREVIEW, _("Preview"), wxBitmap (wxT("rc\\preview.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Preview in player"), wxT(""));
	m_pMainToolbar->AddSeparator();
	m_pMainToolbar->AddTool(TOOL_EDIT_CUT, _("Cut"), wxBitmap (wxT("rc\\cut.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Cut"), wxT(""));
	m_pMainToolbar->AddTool(TOOL_EDIT_COPY, _("Copy"), wxBitmap (wxT("rc\\copy.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Copy"), wxT(""));
	m_pMainToolbar->AddTool(TOOL_EDIT_PASTE, _("Paste"), wxBitmap (wxT("rc\\paste.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Paste"), wxT(""));
	m_pMainToolbar->AddTool(TOOL_EDIT_PASTE_USE, _("PastePlus"), wxBitmap (wxT("rc\\paste_use.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Paste a USE"), wxT(""));
	m_pMainToolbar->AddTool(TOOL_EDIT_DELETE, _("Delete"), wxBitmap (wxT("rc\\delete.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Delete"), wxT(""));
	m_pMainToolbar->AddSeparator();
	m_pMainToolbar->AddTool(TOOL_EDIT_UNDO, _("Undo"), wxBitmap (wxT("rc\\undo.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Undo"), wxT(""));
	m_pMainToolbar->AddTool(TOOL_EDIT_REDO, _("Redo"), wxBitmap (wxT("rc\\redo.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Redo"), wxT(""));
	m_pMainToolbar->AddSeparator();

	m_pMainToolbar->AddSeparator();

	m_pMainToolbar->AddTool(TOOL_ADD_TO_TL, _("AddToTL"), wxBitmap( wxT("rc\\paste.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Add to timeLine"), wxT(""));
	m_pMainToolbar->AddTool(TOOL_NEXT_FRAME, _("NextFrame"), wxBitmap( wxT("rc\\redo.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Next frame"), wxT(""));
	m_pMainToolbar->AddTool(TOOL_VIEW_DICT, _("ViewDict"), wxBitmap( wxT("rc\\open.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("View Dictionnary"), wxT(""));

	/* Not Implemented yet */
	m_pMainToolbar->EnableTool(TOOL_FILE_PREVIEW, false);
	m_pMainToolbar->EnableTool(TOOL_EDIT_UNDO, false);
	m_pMainToolbar->EnableTool(TOOL_EDIT_REDO, false);
	m_pMainToolbar->EnableTool(TOOL_NEXT_FRAME, false);

	/* Not available when starting V4Studio */
	/* Nothing to save */
	m_pFileMenu->Enable(MENU_FILE_SAVE, false);
	m_pFileMenu->Enable(MENU_FILE_CLOSE, false);
	m_pFileMenu->Enable(CHANGE_SIZE_DIALOG, false);
	m_pFileMenu->Enable(CHANGE_LENGTH, false);
	m_pFileMenu->Enable(CHANGE_FRAMERATE, false);
	m_pMainToolbar->EnableTool(TOOL_FILE_SAVE, false);
	m_pMainToolbar->EnableTool(TOOL_FILE_CLOSE, false);

	/* Nothing to edit */
	m_pMainToolbar->EnableTool(TOOL_EDIT_CUT, false);
	m_pMainToolbar->EnableTool(TOOL_EDIT_COPY, false);
	m_pMainToolbar->EnableTool(TOOL_EDIT_PASTE, false);
	m_pMainToolbar->EnableTool(TOOL_EDIT_PASTE_USE, false);
	m_pMainToolbar->EnableTool(TOOL_EDIT_DELETE, false);

	m_pMainToolbar->EnableTool(TOOL_ADD_TO_TL, false);
	m_pMainToolbar->EnableTool(TOOL_VIEW_DICT, false);
	m_pMainToolbar->Realize();
	SetToolBar(m_pMainToolbar);

	m_uSelectedNodeToolBar = 0;
	// Node Creation Toolbar
	m_pLowLevelNodeToolbar = new wxToolBar(this, -1, wxDefaultPosition, wxDefaultSize, wxTB_VERTICAL|wxTB_FLAT);
	m_pLowLevelNodeToolbar->AddTool(TOOL_NEW_ORDEREDGROUP, _("OrderedGroup"), wxBitmap (wxT("rc\\orderedgroup.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Create an OrderedGroup"), wxT(""));
	m_pLowLevelNodeToolbar->AddTool(TOOL_NEW_LAYER2D, _("Layer2D"), wxBitmap (wxT("rc\\layer2d.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Create a Layer2D"), wxT(""));
	m_pLowLevelNodeToolbar->AddTool(TOOL_NEW_TRANSFORM2D, _("Transform2D"), wxBitmap (wxT("rc\\t2d.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Create a Tranform2D"), wxT(""));
	m_pLowLevelNodeToolbar->AddTool(TOOL_NEW_TRANSFORMMATRIX2D, _("TransformMatrix2D"), wxBitmap (wxT("rc\\tm2d.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Create a TranformMatrix2D"), wxT(""));
	m_pLowLevelNodeToolbar->AddTool(TOOL_NEW_COLOR_TRANSFORM, _("ColorTransform"), wxBitmap (wxT("rc\\colortransform.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Create a ColorTransform"), wxT(""));
	m_pLowLevelNodeToolbar->AddTool(TOOL_NEW_SHAPE, _("Shape"), wxBitmap (wxT("rc\\shape.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Create a Shape"), wxT(""));
	m_pLowLevelNodeToolbar->AddSeparator();
	m_pLowLevelNodeToolbar->AddTool(TOOL_NEW_APPEARANCE, _("Appearance"), wxBitmap (wxT("rc\\appearance.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Create an Appearance"), wxT(""));
	m_pLowLevelNodeToolbar->AddTool(TOOL_NEW_MATERIAL2D, _("Material2D"), wxBitmap (wxT("rc\\material2d.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Create a Material2D"), wxT(""));
	m_pLowLevelNodeToolbar->AddTool(TOOL_NEW_LINEPROPS, _("LineProps"), wxBitmap (wxT("rc\\lineproperties.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Create a LineProperties"), wxT(""));
	m_pLowLevelNodeToolbar->AddTool(TOOL_NEW_LINEAR_GRADIENT, _("LinearGradient"), wxBitmap (wxT("rc\\lg.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Create a Linear Gradient"), wxT(""));
	m_pLowLevelNodeToolbar->AddTool(TOOL_NEW_RADIAL_GRADIENT, _("RadialGradient"), wxBitmap (wxT("rc\\rg.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Create a Radial Gradient"), wxT(""));
	m_pLowLevelNodeToolbar->AddSeparator();
	m_pLowLevelNodeToolbar->AddTool(TOOL_NEW_RECT, _("Rectangle"), wxBitmap (wxT("rc\\rect.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Create a Rectangle"), wxT(""));
	m_pLowLevelNodeToolbar->AddTool(TOOL_NEW_CIRCLE, _("Circle"), wxBitmap (wxT("rc\\circle.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Create a Rectangle"), wxT(""));
	m_pLowLevelNodeToolbar->AddTool(TOOL_NEW_IFS2D, _("IndexedFaceSet2D"), wxBitmap (wxT("rc\\ifs2d.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Create an IndexedFaceSet2D"), wxT(""));
	m_pLowLevelNodeToolbar->AddTool(TOOL_NEW_ILS2D, _("IndexedLineSet2D"), wxBitmap (wxT("rc\\ils2d.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Create an IndexedLineSet2D"), wxT(""));
	m_pLowLevelNodeToolbar->AddTool(TOOL_NEW_XLINEPROPS, _("XLineProps"), wxBitmap (wxT("rc\\xlineproperties.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Create an XLineProperties"), wxT(""));
	m_pLowLevelNodeToolbar->AddSeparator();
	m_pLowLevelNodeToolbar->AddTool(TOOL_NEW_TEXT, _("Text"), wxBitmap (wxT("rc\\text.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Create a Text"), wxT(""));
	m_pLowLevelNodeToolbar->AddTool(TOOL_NEW_FONTSTYLE, _("FontStyle"), wxBitmap (wxT("rc\\fs.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Create a FontStyle"), wxT(""));
	m_pLowLevelNodeToolbar->AddSeparator();
	m_pLowLevelNodeToolbar->AddTool(TOOL_NEW_BACKGROUND2D, _("Background2D"), wxBitmap (wxT("rc\\image.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Add a Background2D"), wxT(""));
	m_pLowLevelNodeToolbar->AddTool(TOOL_NEW_MOVIE, _("Movie"), wxBitmap (wxT("rc\\movie.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Add a Movie"), wxT(""));
	m_pLowLevelNodeToolbar->AddTool(TOOL_NEW_IMAGE, _("Image"), wxBitmap (wxT("rc\\image.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Add an Image"), wxT(""));
	m_pLowLevelNodeToolbar->AddTool(TOOL_NEW_SOUND, _("Sound"), wxBitmap (wxT("rc\\sound.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Add a Sound"), wxT(""));
	m_pLowLevelNodeToolbar->Realize();

/*
	m_pHighLevelNodeToolbar = new wxToolBar(this, -1, wxDefaultPosition, wxDefaultSize, wxTB_VERTICAL|wxTB_FLAT);
	m_pHighLevelNodeToolbar->AddTool(TOOL_NEW_2DSCENE, _("OrderedGroup"), wxBitmap (wxT("rc\\orderedgroup.bmp"), wxBITMAP_TYPE_ANY), wxNullBitmap, wxITEM_NORMAL, _("Creates a 2D scene"), wxT(""));
	m_pHighLevelNodeToolbar->Realize();
*/
	set_properties();
	do_layout();
	UpdateToolBar();
}

V4StudioFrame::~V4StudioFrame()
{
}


/* Functions that changes scene parameters */

void V4StudioFrame::OnChangeFrameRate(wxCommandEvent &event) {
	if (!m_pV4sm) return;

	u32 l = m_pV4sm->GetFrameRate();
	wxString sFR;
	sFR.Printf("%d", l);

	wxTextEntryDialog dialog(this, _T("Enter scene framerate"), _T("one positive integer number"), sFR, wxOK | wxCANCEL);

	if (dialog.ShowModal() != wxID_OK) {
	dialog.Destroy();
	return;
	}

	sscanf(dialog.GetValue(), "%d", &l);
	m_pV4sm->SetFrameRate(l);

	dialog.Destroy();
	SceneGraphChanged();
}


void V4StudioFrame::OnChangeLength(wxCommandEvent &event) {
 	if (!m_pV4sm) return;
	u32 l = m_pV4sm->GetLength();
	wxString sLength;
	sLength.Printf("%d", l);

	wxTextEntryDialog dialog(this, _T("Enter scene length"), _T("one positive integer number"), sLength, wxOK | wxCANCEL);

	if (dialog.ShowModal() != wxID_OK) {
	dialog.Destroy();
	return;
	}

	sscanf(dialog.GetValue(), "%d", &l);
	m_pV4sm->SetLength(l);

	dialog.Destroy();
	SceneGraphChanged();
}

void V4StudioFrame::OnChangeSize(wxCommandEvent &event) 
{
	if (!m_pV4sm) return;
	u32 w, h;
	wxSize sceneSize;
	m_pV4sm->GetSceneSize(sceneSize);	
	w = sceneSize.GetX();
	h = sceneSize.GetY();
	wxString sSize;
	sSize.Printf("%d %d", w, h);
	wxTextEntryDialog dialog(this,
						   _T("Enter the scene width and height"),
						   _T("Please enter two integer numbers"),
						   sSize,
						   wxOK | wxCANCEL);

	while (dialog.ShowModal() != wxID_OK && w == 0 && h == 0) {}
	sscanf(dialog.GetValue(), "%d %d", &w, &h);
	m_pV4sm->SetSceneSize(w,h);
	dialog.Destroy();
	SceneGraphChanged();
}

// Dispatchs the new scene graph to all the sub components who need it.
void V4StudioFrame::SceneGraphChanged() 
{
	m_parentSelection = NULL;
	m_selection = (!m_pV4sm ? NULL : m_pV4sm->GetRootNode());
	Layout();
	Update();
}

void V4StudioFrame::set_properties()
{
    SetTitle(_("V4Studio"));
    wxIcon _icon;
    _icon.CopyFromBitmap(wxBitmap(_("rc\\v4.bmp"), wxBITMAP_TYPE_ANY));
    SetIcon(_icon);
    SetSize(wxSize(500, 500));
}


void V4StudioFrame::do_layout()
{
	wxBoxSizer * sizer_6 = new wxBoxSizer(wxVERTICAL);   // top = treeview + fieldview; bottom = timeline + command panel
	wxBoxSizer * sizer_7 = new wxBoxSizer(wxHORIZONTAL); // right = timeline, left = command panel
	wxBoxSizer * sizer_8 = new wxBoxSizer(wxHORIZONTAL); // right = treeview; left = fieldview

	sizer_6->Add(sizer_8,  2, wxEXPAND, 0); // height of treeview twice bigger thant height of timeline
	sizer_6->Add(sizer_7,  1, wxEXPAND, 0);

	sizer_7->Add(timeLine, 3, wxEXPAND, 0);
	sizer_7->Add(cmdPanel, 1, wxEXPAND, 0);

	sizer_8->Add(treeView,  1, wxEXPAND, 0);
	sizer_8->Add(fieldView, 1, wxEXPAND, 0);

	wxBoxSizer* sizer_9 = new wxBoxSizer(wxHORIZONTAL);
	
	sizer_9->Add(m_pLowLevelNodeToolbar, 0, wxEXPAND, 0);
/*
	if (m_uSelectedNodeToolBar == 0) sizer_9->Add(m_pLowLevelNodeToolbar, 0, wxEXPAND, 0);
	else sizer_9->Add(m_pHighLevelNodeToolbar, 0, wxEXPAND, 0);
*/

	sizer_9->Add(sizer_6, 1, wxEXPAND, 0);

	SetSizer(sizer_9);
	Layout();

	cmdPanel->Layout();
}


BEGIN_EVENT_TABLE(V4StudioFrame, wxFrame)
  // m_pFileMenu events
  EVT_MENU(MENU_FILE_NEW, V4StudioFrame::OnNew)
  EVT_MENU(MENU_FILE_OPEN, V4StudioFrame::OnFileOpen)
  EVT_MENU(MENU_FILE_SAVE, V4StudioFrame::OnSave)
  EVT_MENU(MENU_FILE_CLOSE, V4StudioFrame::OnClose)
  EVT_MENU(MENU_FILE_QUIT, V4StudioFrame::OnQuit)
  EVT_MENU(CHANGE_SIZE_DIALOG, V4StudioFrame::OnChangeSize)
  EVT_MENU(CHANGE_FRAMERATE, V4StudioFrame::OnChangeFrameRate)
  EVT_MENU(CHANGE_LENGTH, V4StudioFrame::OnChangeLength)

  // m_pToolsMenu events
  EVT_MENU(MENU_TOOL_SHOW_LOWLEVEL, V4StudioFrame::OnLowLevelTools)
  EVT_MENU(MENU_TOOL_SHOW_HIGHLEVEL, V4StudioFrame::OnHighLevelTools)

  // edit toolbar events
  EVT_TOOL(TOOL_FILE_NEW, V4StudioFrame::OnNew)
  EVT_TOOL(TOOL_FILE_OPEN, V4StudioFrame::OnFileOpen)
  EVT_TOOL(TOOL_FILE_SAVE, V4StudioFrame::OnSave)
  EVT_TOOL(TOOL_FILE_CLOSE, V4StudioFrame::OnClose)
  EVT_TOOL(TOOL_EDIT_CUT, V4StudioFrame::OnEditCut)
  EVT_TOOL(TOOL_EDIT_COPY, V4StudioFrame::OnEditCopy)
  EVT_TOOL(TOOL_EDIT_PASTE, V4StudioFrame::OnEditPaste)
  EVT_TOOL(TOOL_EDIT_PASTE_USE, V4StudioFrame::OnEditPasteUse)
  EVT_TOOL(TOOL_EDIT_DELETE, V4StudioFrame::OnEditDelete)
  EVT_TOOL(TOOL_ADD_TO_TL, V4StudioFrame::OnAddToTimeLine)
  EVT_TOOL(TOOL_NEXT_FRAME, V4StudioFrame::NextFrame)
  EVT_TOOL(TOOL_VIEW_DICT, V4StudioFrame::SwitchView)

  // new toolbar events
  EVT_TOOL(TOOL_NEW_ORDEREDGROUP, V4StudioFrame::OnNewOrderedGroup)
  EVT_TOOL(TOOL_NEW_LAYER2D, V4StudioFrame::OnNewLayer2D)
  EVT_TOOL(TOOL_NEW_TRANSFORM2D, V4StudioFrame::OnNewTransform2D)
  EVT_TOOL(TOOL_NEW_TRANSFORMMATRIX2D, V4StudioFrame::OnNewTransformMatrix2D)
  EVT_TOOL(TOOL_NEW_COLOR_TRANSFORM, V4StudioFrame::OnNewColorTransform)
  EVT_TOOL(TOOL_NEW_SHAPE, V4StudioFrame::OnNewShape)
  EVT_TOOL(TOOL_NEW_APPEARANCE, V4StudioFrame::OnNewAppearance)
  EVT_TOOL(TOOL_NEW_LINEAR_GRADIENT, V4StudioFrame::OnNewLinearGradient)
  EVT_TOOL(TOOL_NEW_RADIAL_GRADIENT, V4StudioFrame::OnNewRadialGradient)
  EVT_TOOL(TOOL_NEW_MATERIAL2D, V4StudioFrame::OnNewMaterial2D)
  EVT_TOOL(TOOL_NEW_LINEPROPS, V4StudioFrame::OnNewLineProps)
  EVT_TOOL(TOOL_NEW_XLINEPROPS, V4StudioFrame::OnNewXLineProps)
  EVT_TOOL(TOOL_NEW_RECT, V4StudioFrame::OnNewRect)
  EVT_TOOL(TOOL_NEW_CIRCLE, V4StudioFrame::OnNewCircle)
  EVT_TOOL(TOOL_NEW_TEXT, V4StudioFrame::OnNewText)
  EVT_TOOL(TOOL_NEW_FONTSTYLE, V4StudioFrame::OnNewFontStyle)
  EVT_TOOL(TOOL_NEW_SOUND, V4StudioFrame::OnNewSound)
  EVT_TOOL(TOOL_NEW_BACKGROUND2D, V4StudioFrame::OnNewBackground2D)
  EVT_TOOL(TOOL_NEW_IMAGE, V4StudioFrame::OnNewImageTexture)
  EVT_TOOL(TOOL_NEW_MOVIE, V4StudioFrame::OnNewMovieTexture)
END_EVENT_TABLE()

// Creates New Scene
void V4StudioFrame::OnNew(wxCommandEvent &event)
{
	m_pV4sm = new V4SceneManager(this);
	m_pV4sm->LoadNew();

	m_pMainToolbar->EnableTool(TOOL_ADD_TO_TL, true);
	m_pMainToolbar->EnableTool(TOOL_VIEW_DICT, true);
	m_pMainToolbar->EnableTool(TOOL_FILE_SAVE, true);
	m_pFileMenu->Enable(MENU_FILE_SAVE, true);
	m_pMainToolbar->EnableTool(TOOL_FILE_CLOSE, true);
	m_pFileMenu->Enable(MENU_FILE_CLOSE, true);
	m_pMainToolbar->EnableTool(TOOL_FILE_OPEN, false);
	m_pFileMenu->Enable(MENU_FILE_OPEN, false);
	m_pMainToolbar->EnableTool(TOOL_FILE_NEW, false);
	m_pFileMenu->Enable(MENU_FILE_NEW, false);
	m_pFileMenu->Enable(CHANGE_SIZE_DIALOG, true);
	m_pFileMenu->Enable(CHANGE_LENGTH, true);
	m_pFileMenu->Enable(CHANGE_FRAMERATE, true);

	Layout();
	SetStatusText("New scene", 0);
	SceneGraphChanged();

	OnChangeSize(event);
}

// Opens an existing file
void V4StudioFrame::OnFileOpen(wxCommandEvent &event)
{
	wxFileDialog *dlg = new wxFileDialog(this, "Open a bt scene",
									   "", "", "BT Files (*.bt)|*.bt|MP4 Files (*.mp4)|*.mp4|XMT Files (*.xmt)|*.xmt|SWF Files (*.swf)|*.swf|All files (*.*)|*.*",
									   wxOPEN, wxDefaultPosition);
	if ( dlg->ShowModal() == wxID_OK ) {
		if (m_pV4sm) delete m_pV4sm;
		m_pV4sm = new V4SceneManager(this);
		m_pV4sm->LoadFile(dlg->GetPath().c_str());
		
		m_pMainToolbar->EnableTool(TOOL_ADD_TO_TL, true);
		m_pMainToolbar->EnableTool(TOOL_VIEW_DICT, true);
		m_pMainToolbar->EnableTool(TOOL_FILE_SAVE, true);
		m_pFileMenu->Enable(MENU_FILE_SAVE, true);
		m_pMainToolbar->EnableTool(TOOL_FILE_CLOSE, true);
		m_pFileMenu->Enable(MENU_FILE_CLOSE, true);
		m_pMainToolbar->EnableTool(TOOL_FILE_OPEN, false);
		m_pFileMenu->Enable(MENU_FILE_OPEN, false);
		m_pMainToolbar->EnableTool(TOOL_FILE_NEW, false);
		m_pFileMenu->Enable(MENU_FILE_NEW, true);
		m_pFileMenu->Enable(CHANGE_SIZE_DIALOG, true);
		m_pFileMenu->Enable(CHANGE_LENGTH, true);
		m_pFileMenu->Enable(CHANGE_FRAMERATE, true);

		Layout();
		SetStatusText(dlg->GetFilename(), 0);
		SceneGraphChanged();
	}
	dlg->Destroy();
}

void V4StudioFrame::OnSave(wxCommandEvent &event)
{
	if (!m_pV4sm) return;
	wxFileDialog *dlg = new wxFileDialog(this, "Save the scene to an mp4 file",
									   "", "", "MPEG-4 Files (*.mp4)|*.mp4|All files (*.*)|*.*",
									   wxSAVE, wxDefaultPosition);
	if ( dlg->ShowModal() == wxID_OK )
	{
	  m_pV4sm->SaveFile(dlg->GetPath().c_str());
	  SetStatusText(dlg->GetFilename(), 0);
	}
	dlg->Destroy();
}

void V4StudioFrame::OnClose(wxCommandEvent &event)
{
	if (m_pV4sm) delete m_pV4sm;
	m_pV4sm = NULL;

	m_pMainToolbar->EnableTool(TOOL_ADD_TO_TL, false);
	m_pMainToolbar->EnableTool(TOOL_VIEW_DICT, false);
	m_pMainToolbar->EnableTool(TOOL_FILE_SAVE, false);
	m_pFileMenu->Enable(MENU_FILE_SAVE, false);
	m_pMainToolbar->EnableTool(TOOL_FILE_CLOSE, false);
	m_pFileMenu->Enable(MENU_FILE_CLOSE, false);
	m_pMainToolbar->EnableTool(TOOL_FILE_OPEN, true);
	m_pFileMenu->Enable(MENU_FILE_OPEN, true);
	m_pMainToolbar->EnableTool(TOOL_FILE_NEW, true);
	m_pFileMenu->Enable(MENU_FILE_NEW, true);
	m_pFileMenu->Enable(CHANGE_SIZE_DIALOG, false);
	m_pFileMenu->Enable(CHANGE_LENGTH, false);
	m_pFileMenu->Enable(CHANGE_FRAMERATE, false);

	m_clipboardNode = NULL;
	m_clipboardParentNode = NULL;
	m_pMainToolbar->EnableTool(TOOL_EDIT_CUT, false);
	m_pMainToolbar->EnableTool(TOOL_EDIT_COPY, false);
	m_pMainToolbar->EnableTool(TOOL_EDIT_PASTE, false);
	m_pMainToolbar->EnableTool(TOOL_EDIT_PASTE_USE, false);
	m_pMainToolbar->EnableTool(TOOL_EDIT_DELETE, false);
	Layout();
	SceneGraphChanged();
}

// Quits application
// TODO : memory leaks !
void V4StudioFrame::OnQuit(wxCommandEvent &event)
{
	Close(FALSE);
}

/**********************************************/
/*  Tools Menu methods						  */
/**********************************************/
void V4StudioFrame::OnLowLevelTools(wxCommandEvent &event)
{
	m_uSelectedNodeToolBar = 0;
	do_layout();
}

void V4StudioFrame::OnHighLevelTools(wxCommandEvent &event)
{
	m_uSelectedNodeToolBar = 1;
	do_layout();
}

/**********************************************/
/*  Functions to add components to the scene  */
/**********************************************/
void V4StudioFrame::OnNewOrderedGroup(wxCommandEvent &event) 
{
	if (!m_pV4sm) return;
	GF_Node *top = m_pV4sm->GetRootNode();
	if (!top) {
		top = m_pV4sm->SetTopNode(TAG_MPEG4_OrderedGroup);
		m_selection = top;
		m_parentSelection = NULL;
	} else {
		GF_Node *og = m_pV4sm->NewNode(TAG_MPEG4_OrderedGroup);
		gf_node_insert_child(m_selection, og, -1);
		gf_node_register(og, m_selection);
		m_parentSelection = m_selection;
		m_selection = og;
	}
	Update();
}

void V4StudioFrame::OnNewLayer2D(wxCommandEvent &event) 
{
	if (!m_pV4sm) return;
	GF_Node *top = m_pV4sm->GetRootNode();
	if (!top) {
		top = m_pV4sm->SetTopNode(TAG_MPEG4_Layer2D);
		m_selection = top;
		m_parentSelection = NULL;
	} else {
		GF_Node *l2d = m_pV4sm->NewNode(TAG_MPEG4_Layer2D);
		gf_node_insert_child(m_selection, l2d, -1);
    gf_node_register(l2d, m_selection);
		m_parentSelection = m_selection;
		m_selection = l2d;
	}
	Update();
}

void V4StudioFrame::OnNewText(wxCommandEvent &event) 
{
	if (!m_pV4sm) return;
	GF_Node *text = m_pV4sm->NewNode(TAG_MPEG4_Text);
	M_Shape *shape = (M_Shape *)m_selection;
	shape->geometry = text;
	gf_node_register(text, m_selection);
	m_parentSelection = m_selection;
	m_selection = text;
	Update();
}

void V4StudioFrame::OnNewFontStyle(wxCommandEvent &event) 
{
	if (!m_pV4sm) return;
	GF_Node *fs = m_pV4sm->NewNode(TAG_MPEG4_FontStyle);
	M_Text *text = (M_Text *)m_selection;
	text->fontStyle = fs;
	gf_node_register(fs, m_selection);
	m_selection = fs;
	m_parentSelection = (GF_Node *)text;
	Update();
}

void V4StudioFrame::OnNewRect(wxCommandEvent &event) 
{
	if (!m_pV4sm) return;
	GF_Node *rect = m_pV4sm->NewNode(TAG_MPEG4_Rectangle);
	M_Shape *shape = (M_Shape *)m_selection;
	shape->geometry = rect;
	gf_node_register(rect, m_selection);
	m_parentSelection = m_selection;
	m_selection = rect;
	Update();
}

void V4StudioFrame::OnNewCircle(wxCommandEvent &event) 
{
	if (!m_pV4sm) return;
	GF_Node *circle = m_pV4sm->NewNode(TAG_MPEG4_Circle);
	M_Shape *shape = (M_Shape *)m_selection;
	shape->geometry = circle;
	gf_node_register(circle, m_selection);
	((M_Circle *) circle)->radius = 75;
	m_parentSelection = m_selection;
	m_selection = circle;
	Update();
}

void V4StudioFrame::OnNewTransform2D(wxCommandEvent &event)
{
	if (!m_pV4sm) return;
	GF_Node *t2d = m_pV4sm->NewNode(TAG_MPEG4_Transform2D);
	if (!m_selection) m_selection = m_pV4sm->GetRootNode();
	gf_node_insert_child(m_selection, t2d, -1);
	gf_node_register(t2d, m_selection);
	m_parentSelection = m_selection;
	m_selection = t2d;
	Update();
}

void V4StudioFrame::OnNewColorTransform(wxCommandEvent &event)
{
	if (!m_pV4sm) return;
	GF_Node *ct = m_pV4sm->NewNode(TAG_MPEG4_ColorTransform);
	if (!m_selection) m_selection = m_pV4sm->GetRootNode();
	gf_node_insert_child(m_selection, ct, -1);
	gf_node_register(ct, m_selection);
	m_parentSelection = m_selection;
	m_selection = ct;
	Update();
}

void V4StudioFrame::OnNewTransformMatrix2D(wxCommandEvent &event)
{
	if (!m_pV4sm) return;
	GF_Node *tm2d = m_pV4sm->NewNode(TAG_MPEG4_TransformMatrix2D);
	if (!m_selection) m_selection = m_pV4sm->GetRootNode();
	gf_node_insert_child(m_selection, tm2d, -1);
	gf_node_register(tm2d, m_selection);
	m_parentSelection = m_selection;
	m_selection = (GF_Node *)tm2d;
	Update();
}

void V4StudioFrame::OnNewShape(wxCommandEvent &event)
{
	if (!m_pV4sm) return;
	GF_Node *shape = m_pV4sm->NewNode(TAG_MPEG4_Shape);
	gf_node_insert_child(m_selection, shape, -1);
	gf_node_register( shape, m_selection);
	m_parentSelection = m_selection;
	m_selection = (GF_Node *)shape;
	Update();
}

void V4StudioFrame::OnNewAppearance(wxCommandEvent &event) 
{
	if (!m_pV4sm) return;
	GF_Node *app = m_pV4sm->NewNode(TAG_MPEG4_Appearance);
	M_Shape *shape = (M_Shape *)m_selection;
	shape->appearance = app;
	gf_node_register(app, m_selection);
	m_parentSelection = m_selection;
	m_selection = app;
	Update();
}

void V4StudioFrame::OnNewLinearGradient(wxCommandEvent &event)
{
	if (!m_pV4sm) return;
	GF_Node *lg = m_pV4sm->NewNode(TAG_MPEG4_LinearGradient);
	M_Appearance *app = (M_Appearance *)m_selection;
	app->texture = lg;
	gf_node_register(lg, m_selection);
	m_parentSelection = m_selection;
	m_selection = (GF_Node *)lg;
	Update();
}

void V4StudioFrame::OnNewRadialGradient(wxCommandEvent &event)
{
	if (!m_pV4sm) return;
	GF_Node *rg = m_pV4sm->NewNode(TAG_MPEG4_RadialGradient);
	M_Appearance *app = (M_Appearance *)m_selection;
	app->texture = rg;
	gf_node_register(rg, m_selection);
	m_parentSelection = m_selection;
	m_selection = rg;
	Update();
}


void V4StudioFrame::OnNewMaterial2D(wxCommandEvent &event)
{
	if (!m_pV4sm) return;
	GF_Node *mat2d = m_pV4sm->NewNode(TAG_MPEG4_Material2D);
	M_Appearance *app = (M_Appearance *)m_selection;
	app->material = mat2d;
	gf_node_register(mat2d, m_selection);
	m_parentSelection = m_selection;

	m_selection = mat2d;
	Update();
}

void V4StudioFrame::OnNewLineProps(wxCommandEvent &event)
{
	if (!m_pV4sm) return;
	GF_Node *lp = m_pV4sm->NewNode(TAG_MPEG4_LineProperties);
	M_Material2D *mat2d= (M_Material2D *)m_selection;
	mat2d->lineProps = lp;
	gf_node_register(lp, m_selection);
	m_parentSelection = m_selection;
	m_selection = lp;
	Update();
}

void V4StudioFrame::OnNewXLineProps(wxCommandEvent &event)
{
	if (!m_pV4sm) return;
	GF_Node *lp = m_pV4sm->NewNode(TAG_MPEG4_XLineProperties);
	M_Material2D *mat2d= (M_Material2D *)m_selection;
	mat2d->lineProps = lp;
	gf_node_register(lp, m_selection);
	m_parentSelection = m_selection;
	m_selection = lp;
	Update();
}

void V4StudioFrame::OnNewSound(wxCommandEvent &event)
{
}

void V4StudioFrame::OnNewBackground2D(wxCommandEvent &event)
{
	if (!m_pV4sm) return;
	GF_Node *b2d = m_pV4sm->NewNode(TAG_MPEG4_Background2D);
	gf_node_insert_child(m_selection, b2d, -1);
	gf_node_register(b2d, m_selection);
	m_parentSelection = m_selection;
	m_selection = b2d;
	Update();
}

void V4StudioFrame::OnNewImageTexture(wxCommandEvent &event)
{
	if (!m_pV4sm) return;
	GF_Node *it = m_pV4sm->NewNode(TAG_MPEG4_ImageTexture);
	M_Appearance *app = (M_Appearance *) m_selection;
	app->texture = it;
	gf_node_register(it, m_selection);
	m_parentSelection = m_selection;
	m_selection = it;
	Update();
}

void V4StudioFrame::OnNewMovieTexture(wxCommandEvent &event)
{
	if (!m_pV4sm) return;
	GF_Node *mt = m_pV4sm->NewNode(TAG_MPEG4_MovieTexture);
	M_Appearance *app = (M_Appearance *)m_selection;
	app->texture = mt;
	gf_node_register(mt, m_selection);
	m_parentSelection = m_selection;
	m_selection = mt;
	Update();
}

void V4StudioFrame::UpdateSelection(GF_Node *node, GF_Node *parent) 
{
	SetSelection(node);
	SetParentSelection(parent);
	m_pMainToolbar->EnableTool(TOOL_EDIT_CUT, true);
	m_pMainToolbar->EnableTool(TOOL_EDIT_COPY, true);
	m_pMainToolbar->EnableTool(TOOL_EDIT_DELETE, true);
	UpdateToolBar();
	fieldView->SetNode(node);
	fieldView->Create();
}


// Update display
void V4StudioFrame::Update() 
{
	if (m_pV4sm){
		if (m_pV4sm->GetGPACPanel()) m_pV4sm->GetGPACPanel()->Update();
		treeView->Refresh(m_pV4sm->GetRootNode());
	} else {
		treeView->Refresh(NULL);
	}
	UpdateToolBar();
}

void V4StudioFrame::UpdateToolBar()
{
	bool enableGeometry   = false, 
		 enableAppearance = false, 
		 enableMaterial   = false, 
		 enableGroupNode  = false,
		 enableLineProps  = false, 
		 enableFontStyle  = false, 
		 enable2DNode     = false, 
		 enableTexture    = false, 
		 enableTopNode    = false,
		 enableBackground = false;

	// a scene must have been created to enable the controls
	if (m_pV4sm && m_pV4sm->GetInlineScene()) {
		if (m_selection != NULL) {
			u32 tag = gf_node_get_tag(m_selection);
			GF_Node * node = m_selection;
			enableBackground = true;
			switch (tag) {
				case TAG_MPEG4_Switch:
				case TAG_MPEG4_OrderedGroup:
				case TAG_MPEG4_Layer2D:
				case TAG_MPEG4_Transform2D:
				case TAG_MPEG4_TransformMatrix2D:
				case TAG_MPEG4_ColorTransform:
					enableGroupNode = true;
					enable2DNode = true;
					break;
				case TAG_MPEG4_Shape:
					if (!((M_Shape *)node)->geometry) enableGeometry = true;
					if (!((M_Shape *)node)->appearance) enableAppearance = true;
					break;
				case TAG_MPEG4_Appearance:
					if (!((M_Appearance *)node)->material) enableMaterial = true;
					if (!((M_Appearance *)node)->texture) enableTexture = true;
					break;
				case TAG_MPEG4_Material2D:
					if (!((M_Material2D *)node)->lineProps) enableLineProps = true;
					break;
				case TAG_MPEG4_Text:
					if (!((M_Text *)node)->fontStyle) enableFontStyle = true;
					break;
				case TAG_MPEG4_Rectangle:
				case TAG_MPEG4_Circle:
				case TAG_MPEG4_ImageTexture:
				case TAG_MPEG4_MovieTexture:
				case TAG_MPEG4_Background2D:
					break;
			}
		} else {
			enableTopNode = true;
		}
	} 

	m_pLowLevelNodeToolbar->EnableTool(TOOL_NEW_ORDEREDGROUP, enableGroupNode || enable2DNode || enableTopNode);
	m_pLowLevelNodeToolbar->EnableTool(TOOL_NEW_LAYER2D, enableGroupNode || enable2DNode || enableTopNode);
	m_pLowLevelNodeToolbar->EnableTool(TOOL_NEW_TRANSFORM2D, enableGroupNode || enable2DNode);
	m_pLowLevelNodeToolbar->EnableTool(TOOL_NEW_COLOR_TRANSFORM, enableGroupNode || enable2DNode);
	m_pLowLevelNodeToolbar->EnableTool(TOOL_NEW_TRANSFORMMATRIX2D, enableGroupNode || enable2DNode);
	m_pLowLevelNodeToolbar->EnableTool(TOOL_NEW_SHAPE, enable2DNode);
	m_pLowLevelNodeToolbar->EnableTool(TOOL_NEW_RECT, enableGeometry);
	m_pLowLevelNodeToolbar->EnableTool(TOOL_NEW_CIRCLE, enableGeometry);
	m_pLowLevelNodeToolbar->EnableTool(TOOL_NEW_IFS2D, enableGeometry);
	m_pLowLevelNodeToolbar->EnableTool(TOOL_NEW_ILS2D, enableGeometry);
	m_pLowLevelNodeToolbar->EnableTool(TOOL_NEW_TEXT, enableGeometry);
	m_pLowLevelNodeToolbar->EnableTool(TOOL_NEW_MOVIE, enableTexture);
	m_pLowLevelNodeToolbar->EnableTool(TOOL_NEW_IMAGE, enableTexture);
	m_pLowLevelNodeToolbar->EnableTool(TOOL_NEW_SOUND, enable2DNode);
	m_pLowLevelNodeToolbar->EnableTool(TOOL_NEW_APPEARANCE, enableAppearance);
	m_pLowLevelNodeToolbar->EnableTool(TOOL_NEW_MATERIAL2D, enableMaterial);
	m_pLowLevelNodeToolbar->EnableTool(TOOL_NEW_LINEAR_GRADIENT, enableTexture);
	m_pLowLevelNodeToolbar->EnableTool(TOOL_NEW_RADIAL_GRADIENT, enableTexture);
	m_pLowLevelNodeToolbar->EnableTool(TOOL_NEW_LINEPROPS, enableLineProps);
	m_pLowLevelNodeToolbar->EnableTool(TOOL_NEW_XLINEPROPS, enableLineProps);
	m_pLowLevelNodeToolbar->EnableTool(TOOL_NEW_FONTSTYLE, enableFontStyle);
	m_pLowLevelNodeToolbar->EnableTool(TOOL_NEW_BACKGROUND2D, enableBackground);
}

// Cut and paste functions
void V4StudioFrame::OnEditCut(wxCommandEvent &WXUNUSED(event))
{
	if (!m_selection) return;

	m_clipboardNode = m_selection;
	m_clipboardParentNode = m_parentSelection;
	gf_node_remove_child(m_parentSelection, m_selection);
	m_pMainToolbar->EnableTool(TOOL_EDIT_CUT, true);
	m_pMainToolbar->EnableTool(TOOL_EDIT_COPY, true);
	m_pMainToolbar->EnableTool(TOOL_EDIT_PASTE, true);
	m_pMainToolbar->EnableTool(TOOL_EDIT_PASTE_USE, false);
	m_pMainToolbar->EnableTool(TOOL_EDIT_DELETE, false);

	Update();
}

void V4StudioFrame::OnEditDelete(wxCommandEvent &WXUNUSED(event))
{
	if (!m_selection) return;

	gf_node_remove_child(m_parentSelection, m_selection);
	m_clipboardNode = NULL;
	m_clipboardParentNode = NULL;
	m_pMainToolbar->EnableTool(TOOL_EDIT_CUT, false);
	m_pMainToolbar->EnableTool(TOOL_EDIT_COPY, false);
	m_pMainToolbar->EnableTool(TOOL_EDIT_PASTE, false);
	m_pMainToolbar->EnableTool(TOOL_EDIT_PASTE_USE, false);
	m_pMainToolbar->EnableTool(TOOL_EDIT_DELETE, false);

	Update();
}

void V4StudioFrame::OnEditCopy(wxCommandEvent &WXUNUSED(event))
{
	if (!m_selection) return;

	m_clipboardNode = m_selection;
	m_clipboardParentNode = m_parentSelection;
	m_pMainToolbar->EnableTool(TOOL_EDIT_CUT, true);
	m_pMainToolbar->EnableTool(TOOL_EDIT_COPY, true);
	m_pMainToolbar->EnableTool(TOOL_EDIT_PASTE, true);
	m_pMainToolbar->EnableTool(TOOL_EDIT_PASTE_USE, true);
	m_pMainToolbar->EnableTool(TOOL_EDIT_DELETE, false);

	Update();
}

void V4StudioFrame::OnEditPaste(wxCommandEvent &WXUNUSED(event))
{
	if (!m_pV4sm) return;
	if (m_clipboardNode == NULL) return;

	GF_Node *copy = m_pV4sm->CopyNode(m_clipboardNode, m_parentSelection, true);
	gf_node_insert_child(m_selection, copy, -1);
	m_pMainToolbar->EnableTool(TOOL_EDIT_CUT, true);
	m_pMainToolbar->EnableTool(TOOL_EDIT_COPY, true);
	m_pMainToolbar->EnableTool(TOOL_EDIT_PASTE, true);
	m_pMainToolbar->EnableTool(TOOL_EDIT_PASTE_USE, false);
	m_pMainToolbar->EnableTool(TOOL_EDIT_DELETE, false);

	Update();
}

void V4StudioFrame::OnEditPasteUse(wxCommandEvent &WXUNUSED(event))
{
	if (!m_pV4sm) return;
	if (m_clipboardNode == NULL) return;

	GF_Node *copy = m_pV4sm->CopyNode(m_clipboardNode, m_parentSelection, false);
	gf_node_register(copy, m_selection);
	gf_node_insert_child(m_selection, copy, -1);
	m_pMainToolbar->EnableTool(TOOL_EDIT_CUT, true);
	m_pMainToolbar->EnableTool(TOOL_EDIT_COPY, true);
	m_pMainToolbar->EnableTool(TOOL_EDIT_PASTE, true);
	m_pMainToolbar->EnableTool(TOOL_EDIT_PASTE_USE, false);
	m_pMainToolbar->EnableTool(TOOL_EDIT_DELETE, false);

	Update();
}

// OnAddToTimeLine -- adds a node in the timeline (meaning creating a node ID and adding it to the dictionnary and the pool)
void V4StudioFrame::OnAddToTimeLine(wxCommandEvent &event) {
	if (!m_pV4sm) return;
	if (!m_selection) return; // don't add nothing

	// if object has no node id then create one
	if (! gf_node_get_id(m_selection) )
		m_pV4sm->CreateIDandAddToPool(m_selection);
	
	// Adds the node to the dictionnary
//	m_pV4sm->AddToDictionnary(m_selection);

	// calls timeline function to add the line
	char c[50];
	strcpy(c, gf_node_get_name(m_selection));
	timeLine->AddLine( gf_node_get_id(m_selection), wxString(c) );
}


// NextFrame -- goto next frame
void V4StudioFrame::NextFrame(wxCommandEvent &event) {
	//gf_term_play_from_time(gpacPanel->GetMPEG4Terminal(), 1000);  
	m_selection = NULL;
	Update();
}


// SwitchView -- switches between scene editing and dictionnary editing
void V4StudioFrame::SwitchView(wxCommandEvent &event) {
	SetEditDict(!editDict);
}



/****** Access Functions ******/

// changes time to the frame specified, implies playing the scene up to that point
void V4StudioFrame::SetFrame(unsigned long _frame) {
	// TODO : 
	m_frame = _frame;
}


// SetEditDict -- edit the dictionnary or not
void V4StudioFrame::SetEditDict(bool _editDict) {

   // does nothing if no scene
	if (!m_pV4sm) return;
   if ( ! m_pV4sm->GetSceneGraph() ) return;

   editDict = _editDict;

   if (editDict) treeView->Refresh(m_pV4sm->GetDictionnary());
   else treeView->Refresh(m_pV4sm->GetRootNode());
}


// GetEditDict -- Access to edit dict
bool V4StudioFrame::GetEditDict() const {
	return editDict;
}
