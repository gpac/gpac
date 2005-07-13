/*
  V4CommandPanel.h

    Header file for CommandPanel, the control alloing to create commands from the timeline
*/

#ifndef _V4CommandPanel_h_
#define _V4CommandPanel_h_

#include "safe_include.h"
#include <wx/wx.h>
#include <wx/notebook.h>

// list of all commands
#include <gpac/scenegraph_vrml.h>


#define cmbCommandID        wxID_HIGHEST+1
#define cmbFieldID          wxID_HIGHEST+2
#define CreateID            wxID_HIGHEST+3


class V4StudioFrame;
class V4NodePool;


class V4CommandPanel : public wxPanel {
  public:
    // Constructor Destructor
    V4CommandPanel(V4StudioFrame * parent);
    ~V4CommandPanel();

    // Update
    void Refresh(u32 frame);

  private:

    u32 frame;

    // controls

    wxNotebook tabs;
    wxPanel tabView;
    wxPanel tabAdd;

    // add page
    wxComboBox cmbCommands;
    wxStaticText lblCommands;

    wxComboBox cmbNodes;
    wxStaticText lblNodes;

    wxComboBox cmbFields;
    wxStaticText lblFields;

    wxButton btnCreate;

    wxTextCtrl txtField;


    // view page
    wxComboBox cmbListCommands;
    wxTextCtrl txtDesc;
    wxButton btnDelete;

    //sizers
    // global
    wxBoxSizer * sizerTabs;
    // Add page
    wxBoxSizer * sizerAdd, * sizerU, * sizerM, * sizerD, * szSelectNode, * szSelectField, * szTxtField;
    // View page
    wxBoxSizer * sizerView, * sizerUp, * sizerDown;


    // UI -- show or hides part of the control
    void ShowNodeSizer(bool show);
    void ShowFieldSizer(bool show);

    // Refreshing of various comboboxes
    void RefreshCommands();
    void RefreshNodes();
    void RefreshFields();
    void RefreshListCommands();

    // fills the cmdNodes with nodes from pool, do not add the specified node if found
    void PopulateNodes(V4NodePool& pool, GF_Node * node);

    // events
    DECLARE_EVENT_TABLE()
    void OnPaint(wxPaintEvent& event);
    void OnCommandCombo(wxCommandEvent &event);
    void OnFieldCombo(wxCommandEvent &event);
    void OnCreateCommand(wxCommandEvent &event);

    bool IsCommandValidForNode(u32 command, GF_Node * node); // tells if we can apply this command to that node
    GF_Node * GetCurrentNode(); // returns the node currently selected in the timeline

	// All the component of the V4Studio Main Frame have a pointer to that Main Frame, called parent.
    V4StudioFrame * parent;

};

#endif