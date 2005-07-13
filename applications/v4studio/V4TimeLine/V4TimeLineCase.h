/*
  V4TimeLineCase.h

    // TODO : description
*/

#ifndef _V4TimeLineCell_
#define _V4TimeLineCell_


#include <gpac/scenegraph_vrml.h>

#include <wx/window.h>


class V4TimeLineLine;

// differents types of a cell -- differs with the line type
#define CELL_TYPE_HDR       0
#define CELL_TYPE_NORMAL    1

// different states of a cell
#define CELL_STATE_CLICKED  1
#define CELL_STATE_SELECTED 2
#define CELL_STATE_COMMAND  4

class V4StudioFrame;

class V4TimeLineCell: public wxWindow {

  public:
    // Constructor
    V4TimeLineCell(V4TimeLineLine * parent, unsigned int num=0, unsigned int type=CELL_TYPE_NORMAL);
 
    // Destructor
    virtual ~V4TimeLineCell();

    // Changes cell state
    void SetState(unsigned int _state);
    unsigned int GetState() const;

    // returns a pointer to parent (used to retrieve line number and node ID)
    V4TimeLineLine * GetParent() const;

    // manipulates the commands list
    void AddCommand(GF_Command *);
    void DeleteCommands();
    void DeleteCommand(const u32 n);
    GF_Command * GetCommand(u32 n);

  private:
    unsigned int state; // possible values not yet defined
    unsigned int type;  // type of cell

    unsigned int num;

    DECLARE_EVENT_TABLE()

    V4StudioFrame * v4sf;
    V4TimeLineLine * parent;

    GF_List * commands; // actions to perform for this object (row) when entering the frame (col)

    void OnLeftUp(wxMouseEvent &);
    void OnPaint(wxPaintEvent &);
};

#endif