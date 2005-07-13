/*
  V4NodePools

    List all the Node pools for each possible node tag

*/

#include "safe_include.h"
#include <gpac/scenegraph.h>
#include <gpac/nodes_mpeg4.h>

#include <vector>

#include "V4NodePool.h"


using namespace std;


enum {
  DICT_GEN = 0,
  DICT_GEOMETRY,
  DICT_APPEARANCE,
  DICT_TEXTURE,
  DICT_MATERIAL,

  DICT_LAST // last element, gives the actual number of pool
};


class V4NodePools {

  public:
    // Constructor
    V4NodePools();

    // gets the correct pool from the given tag
    V4NodePool& pool(const u32 tag);
    V4NodePool& poolFromFieldName(const char * name);
    u32 poolN(const u32 tag);

    // clears all the pools
    void Clear();

    // Adds a node to the correct pool
    void Add(GF_Node * node);

    // tells if nodes as field of the given type
    bool NodeHasField(GF_Node * node, u32 fieldType);
    
    // returns the total number of nodes created for that type of node
    u32 GetCount(const u32 tag);


  private:
    vector<V4NodePool> pools;
    unsigned int unPooled;

};