#ifndef V4STUDIO_FRAME_H
#define V4STUDIO_FRAME_H

#include "safe_include.h"  // include m4_tools.h

#include <wx/wx.h>
#include <wx/image.h>
#include <wx/listctrl.h>

// UI
#include "V4StudioTree.h"
#include "V4FieldList.h"
#include "V4TimeLine\V4TimeLine.h"
#include "V4CommandPanel.h"

// Scene data
#include "V4SceneManager.h" 

enum {
	// File Menu commands
	MENU_FILE_NEW,
	MENU_FILE_OPEN,
	MENU_FILE_SAVE,
	MENU_FILE_CLOSE,
	MENU_FILE_QUIT,

	// Tools Menu commands
	MENU_TOOL_SHOW_LOWLEVEL,
	MENU_TOOL_SHOW_HIGHLEVEL,

	// Generic Toolbar commands
	TOOL_FILE_NEW,
	TOOL_FILE_OPEN,
	TOOL_FILE_SAVE,
	TOOL_FILE_CLOSE,
	TOOL_FILE_PREVIEW,
	TOOL_EDIT_CUT,
	TOOL_EDIT_COPY,
	TOOL_EDIT_PASTE,
	TOOL_EDIT_PASTE_USE,
	TOOL_EDIT_DELETE,
	TOOL_EDIT_UNDO,
	TOOL_EDIT_REDO,
	TOOL_FRAME_COMBO,
	TOOL_ADD_TO_TL,
	TOOL_NEXT_FRAME,
	TOOL_VIEW_DICT,

	CHANGE_SIZE_DIALOG,
	CHANGE_FRAMERATE,
	CHANGE_LENGTH,

	/* High level node creation */
	TOOL_NEW_2DSCENE,

	/* Low level node creations */
	// 2D node Toolbar commands
	TOOL_NEW_ORDEREDGROUP,
	TOOL_NEW_LAYER2D,
	TOOL_NEW_TRANSFORM2D,
	TOOL_NEW_TRANSFORMMATRIX2D,
	TOOL_NEW_COLOR_TRANSFORM,
	TOOL_NEW_SHAPE,

	// Appearance and sub-appearance Toolbar commands
	TOOL_NEW_APPEARANCE,
	TOOL_NEW_MATERIAL2D,
	TOOL_NEW_LINEPROPS,
	TOOL_NEW_XLINEPROPS,
	TOOL_NEW_FONTSTYLE,
	TOOL_NEW_LINEAR_GRADIENT,
	TOOL_NEW_RADIAL_GRADIENT,

	// Geometry Toolbar commands
	TOOL_NEW_RECT,
	TOOL_NEW_CIRCLE,
	TOOL_NEW_IFS2D,
	TOOL_NEW_ILS2D,
	TOOL_NEW_TEXT,

	// Media Toolbar commands
	TOOL_NEW_BACKGROUND2D,
	TOOL_NEW_MOVIE,
	TOOL_NEW_IMAGE,
	TOOL_NEW_SOUND
};

class V4StudioFrame: public wxFrame {
public:

	// Constructor / Desctructor
	V4StudioFrame();
	~V4StudioFrame();

	/* generic UI functions */

	/* File Menu functions */
	void OnNew(wxCommandEvent &event);
	void OnFileOpen(wxCommandEvent &event);
	void OnSave(wxCommandEvent &event);
	void OnClose(wxCommandEvent &event);
	void OnQuit(wxCommandEvent &event);
	void OnChangeSize(wxCommandEvent &event);
	void OnChangeFrameRate(wxCommandEvent &event);
	void OnChangeLength(wxCommandEvent &event);

	/* Tools Menu functions */
	void OnLowLevelTools(wxCommandEvent &event);
	void OnHighLevelTools(wxCommandEvent &event);

	/* edition toolbar functions */
	void OnEditCut(wxCommandEvent &event);
	void OnEditCopy(wxCommandEvent &event);
	void OnEditPaste(wxCommandEvent &event);
	void OnEditPasteUse(wxCommandEvent &event);
	void OnEditDelete(wxCommandEvent &event);
	void OnAddToTimeLine(wxCommandEvent &event);
	void OnCombo(wxCommandEvent& event);
	void NextFrame(wxCommandEvent &event);
	void SwitchView(wxCommandEvent &event);

	/* new components toolbar functions */
	void OnNewLayer2D(wxCommandEvent &event);
	void OnNewOrderedGroup(wxCommandEvent &event);
	void OnNewTransform2D(wxCommandEvent &event);
	void OnNewTransformMatrix2D(wxCommandEvent &event);
	void OnNewColorTransform(wxCommandEvent &event);
	void OnNewShape(wxCommandEvent &event);
	void OnNewAppearance(wxCommandEvent &event);
	void OnNewMaterial2D(wxCommandEvent &event);
	void OnNewLinearGradient(wxCommandEvent &event);
	void OnNewRadialGradient(wxCommandEvent &event);
	void OnNewLineProps(wxCommandEvent &event);
	void OnNewXLineProps(wxCommandEvent &event);
	void OnNewText(wxCommandEvent &event);
	void OnNewFontStyle(wxCommandEvent &event);
	void OnNewRect(wxCommandEvent &event);
	void OnNewCircle(wxCommandEvent &event);
	void OnNewSound(wxCommandEvent &event);
	void OnNewBackground2D(wxCommandEvent &event);
	void OnNewImageTexture(wxCommandEvent &event);
	void OnNewMovieTexture(wxCommandEvent &event);


	/* display update functions */
	void Update();
	void UpdateSelection(GF_Node *node, GF_Node *parent);
	void SetSelection(GF_Node *node) { m_selection = node; }
	void SetParentSelection(GF_Node *node) { m_parentSelection = node; }
	void UpdateToolBar();
	void SceneGraphChanged();


	/* access to private members */

	// UI objects
	V4FieldList    * GetFieldView()			{ return fieldView; }
	V4StudioTree   * GetTreeView()			{ return treeView; }
	V4CommandPanel * GetCmdPanel()			{ return cmdPanel; }
	V4TimeLine     * GetTimeLine()			{ return timeLine; }
	V4SceneManager * GetV4SceneManager()    { return m_pV4sm; }

	// variables
	void SetFrame(unsigned long _frame);  // changes time to the frame specified, implies playing the scene up to that point
	void SetEditDict(bool editDict);      // edit the dictionnary or not
	bool GetEditDict() const;


private:
    void set_properties();
    void do_layout();

protected:

	DECLARE_EVENT_TABLE()

	// select, cut and paste members
	GF_Node			*m_selection;
	GF_Node			*m_parentSelection;
	GF_Node			*m_clipboardNode;
	GF_Node			*m_clipboardParentNode;

	// scene data
	V4SceneManager	*m_pV4sm;

	// UI objects
	V4StudioTree	*treeView;
	V4FieldList		*fieldView;
	V4TimeLine		*timeLine;
	V4CommandPanel	*cmdPanel;

	wxMenu			*m_pFileMenu;
	wxMenu			*m_pToolsMenu;
	wxStatusBar		*m_pStatusbar;
	// The Main bar that contains the new, load, save, cut, copy ...
	wxToolBar		*m_pMainToolbar;

	// The tool bar to create all the MPEG-4 nodes
	wxToolBar		*m_pLowLevelNodeToolbar;
	wxToolBar		*m_pHighLevelNodeToolbar;
	u32				m_uSelectedNodeToolBar;

	// editing states machine
	bool editDict;              // true if we are editing the dictionnary
	unsigned long m_frame;      // frame currently selected

};


#endif 

