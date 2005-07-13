/*
  V4NodePool.cpp

*/
#include "safe_include.h"
#include <gpac/scenegraph_vrml.h>

#include "V4NodePool.h"


// FindNode -- search for a node with the specified NodeID or pointer
u32 V4NodePool::FindNode(const u32 NodeID, const GF_Node * node) {

  if ( (NodeID == 0) && (node == NULL) ) return (u32) -1;

  for (int i = 0; i < Nodes.size(); i++)
    if ( (NodeID == Nodes.at(i).GetID()) || (node == Nodes.at(i).GetNode()) ) return i;

  return (u32) -1;
}


// AddNode --
V4Node& V4NodePool::AddNode(GF_Node * _node) {
  // checks that the node is not already in the pool
  u32 id = gf_node_get_id(_node);
  u32 i = FindNode(id);
  if (i != (u32) -1) return Nodes.at(i);

  // add a new entry into the pool
  V4Node node(id, _node);
  Nodes.push_back(node);
  count++;
  return Nodes.back();
}


// DeleteNode -- Deletes a node from the pool
void V4NodePool::DeleteNode(const u32 _NodeID, const GF_Node * _node) {
  u32 i = FindNode(_NodeID, _node);
  std::vector<V4Node>::iterator iter = Nodes.begin();

  for (u32 j=0; j<i; j++) iter++;
  Nodes.erase(iter);
}


// GetV4Node -- retrieves a node from the pool
V4Node&  V4NodePool::GetV4Node(const u32 _NodeID, const GF_Node * _node) {
  u32 i = FindNode(_NodeID, _node);
  return Nodes.at(i);
}


// GetNodesCount -- returns the numbre of nodes
u32 V4NodePool::GetNodesCount() const {
  return Nodes.size();
}


// FirstNode -- returns the beginning the list
V4Node& V4NodePool::at(u32 n) {
  return Nodes.at(n);
}



// Clear -- deletes all item
void V4NodePool::Clear() {
  Nodes.clear();
}

// returns count
u32 V4NodePool::GetCount() const {
  return count;
}