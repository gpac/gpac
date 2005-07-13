/*
  V4Node.h

    Represents a node with additionnal attributes for V4Studio, such as lifespan
*/

#ifndef _V4Node_
#define _V4Node_

#include "safe_include.h"
#include <gpac/scenegraph.h> // GF_Node structure

#include <wx/wx.h>

#include <list>


struct segment {
  u32 from, to;
};


class V4Node {
  public:
    // constructor destructor
    V4Node(const u32 NodeID, GF_Node * node=NULL); // node may be modified, don't use const GF_Node *

    // accesses to members
    u32 GetID() const;
    GF_Node * GetNode() const;


    // computes whether the node is alive at the given frame
    bool IsAliveAt(const u32 frame);

    // Add an apparition, inclusive
    void Appear(const u32 startFrame, const u32 stopFrame);

    // Remove frames from lifeSpan, inclusive
    void Disappear(const u32 startFrame, const u32 stopFrame);


    // (to save workspace) tells whether the node should appear on the timeline
    bool isOnTimeLine;

    // retrieves the node _DEF_ name (node has a Node ID)
    wxString GetName();

    

  private:
    // various accesses to the node
    u32 NodeID;
    GF_Node * node;

    // timeLine
    std::list<segment> lifeSpan; // each element of the list is one use of this node, first value is start frame, second is end frame, inclusive

};

#endif