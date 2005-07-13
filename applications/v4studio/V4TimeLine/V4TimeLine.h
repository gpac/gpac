/*
  V4TimeLine.h

	V4Studio component allowing to edit timings
	The component draw sub components whose type is V4TimeLineElt
*/

#ifndef _V4TimeLine_
#define _V4TimeLine_

#include "../safe_include.h"

// wxWidgets includes
#include <wx/scrolwin.h>
#include <wx/sizer.h>

// Own includes
#include "V4TimeLineElt.h"
#include "V4TimeLineHdr.h"

// STL includes
#include <vector>

class V4TimeLineCell;
class V4StudioFrame;



class V4TimeLine : public wxScrolledWindow {
  public:
	  // Constructeur -- timeLineLength est compté en images
	  V4TimeLine(wxWindow * parent, unsigned int timeLineLength = 50);

	  // Desctructeur
	  ~V4TimeLine();

    void AddLine(u32 NodeID, wxString eltName); // Adds a line to the timeline
                                    // TODO : add an additionnal parameter to specify a pointer to the element represented by the line
                                    // TODO : use node pointer instead of id ?

    void Clear(); // clears all the line in the timeline
                  // TODO : check if everything is destroyed correctly

    // retourne la position en X du debut de la grille
    int GetOffset();

    // access to Length
    unsigned int GetLength();
    void SetLength(const unsigned int length_);

    // called when time changed, update display (background colors)
    void SetFrame(u32 _frame, V4TimeLineCell * newClicked = NULL);

    // returns the ID of the node whose timeline is selected
    u32 GetSelectedID() const;

    // Deletes a command from the current cell
    void DeleteCommand(u32 n);

    // Adds a command to the current cell
    void AddCommand(GF_Command *);

    // Retrieves the list of commands for the current cell
    GF_Command * GetCommand(u32 n);

    // pointer to the parent
    V4StudioFrame * parent;


  private:
    // composants graphiques
    std::vector<V4TimeLineElt *> lines;
    V4TimeLineHdr * hdr;
	  wxBoxSizer * sizer;

    // attributs divers
    int Offset;                   // X position of the grid
    u32 Length;                   // length (in frames) of the scene
    V4TimeLineCell * clicked;     // cell currently "clicked"


};

#endif