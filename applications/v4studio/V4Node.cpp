/*
  V4Node.cpp
 
    Implements V4Node class
*/

#include "V4Node.h"
#include <crtdbg.h>


// Constructor
V4Node::V4Node(const u32 _NodeID, GF_Node * _node) : NodeID(_NodeID), node(_node) {
  _ASSERT(_node); // no NULL node
}


// GetID --
u32 V4Node::GetID() const {
  return NodeID;  
}


// GetNode --
GF_Node * V4Node::GetNode() const {
  return node;
}


// IsAliveAt -- check all aparitions
bool V4Node::IsAliveAt(const u32 frame) {
  std::list<segment>::iterator i; 

  for (i = lifeSpan.begin(); i != lifeSpan.end(); i++)
    if ( ((*i).from >= frame) && ((*i).to <= frame) ) return true;

  return false;
}


// Appear
void V4Node::Appear(const u32 startFrame, const u32 stopFrame) {
  // TODO :
  return;
}


// Disappear
void V4Node::Disappear(const u32 startFrame, const u32 stopFrame) {
  // TODO :
  return;
}


// GetName -- 
wxString V4Node::GetName() {
  return wxString(gf_node_get_name(node));
}