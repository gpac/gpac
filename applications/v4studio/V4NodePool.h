/*
  V4NodePool

    Holds every node usable

*/

#ifndef _V4NodePool_
#define _V4NodePool_

#include "safe_include.h"
#include <vector>
#include "V4Node.h"

using namespace std;

class V4NodePool {

  public:

    // Constructor
    V4NodePool() { count = 0; }
    
    // Adds a node to the pool
    V4Node& AddNode(GF_Node * node=NULL); // node may be modified, don't use const GF_Node *

    // Deletes a node from the pool
    void DeleteNode(const u32 NodeID, const GF_Node * node=NULL);

    // retrieves a node from the pool
    V4Node& GetV4Node(const u32 NodeID, const GF_Node * node=NULL);

    // returns the numbre of nodes
    u32 GetNodesCount() const;

    // returns the nth element in the vector
    V4Node& at(u32 n);

    // clear the pool
    void Clear();

    // returns count
    u32 GetCount() const;


  private:
    vector<V4Node> Nodes;

    // find the V4Node with the specified attributes
    u32 FindNode(const u32 NodeID, const GF_Node * node=NULL);

    // counts the nodes added to that pool (used in autonaming nodes)
    u32 count;

};

#endif