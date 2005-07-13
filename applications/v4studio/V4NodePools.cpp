/*
  V4NodePools.cpp

    Implements V4NodePools class

*/

#include "V4NodePools.h"
#include "V4NodePool.h"

V4NodePool dummy;

// Constructor
V4NodePools::V4NodePools() {
  pools.resize(DICT_LAST);
  unPooled = 0;
}


// pool -- returns the pool of object for the given tag 
V4NodePool& V4NodePools::pool(const u32 tag) 
{
	u32 n = poolN(tag);
  if ((u32) -1 == n) return dummy;
	return pools.at(n);
}


// pool -- returns the NUMBER of the pool for the given tag
u32 V4NodePools::poolN(const u32 tag) {

  switch (tag) {

    // objects that can be children of a switch
    case TAG_MPEG4_OrderedGroup:
    case TAG_MPEG4_Shape: {
      return DICT_GEN;
      break;
    }

    // objects that can be a geometry
    case TAG_MPEG4_Rectangle:
    case TAG_MPEG4_IndexedFaceSet2D:
    case TAG_MPEG4_Circle: {
      return DICT_GEOMETRY;
      break;
    }

    // objects that can be an appearance
    case TAG_MPEG4_Appearance: {
      return DICT_APPEARANCE;
      break;
    }

    // objects that can be a texture
    case TAG_MPEG4_MovieTexture:
    case TAG_MPEG4_ImageTexture: {
      return DICT_TEXTURE;
      break;
    }

    // objects that can be a material
    case TAG_MPEG4_Material2D: {
      return DICT_MATERIAL;
      break;
    }

  }

  return (u32) -1;
}


// poolNFromFieldName -- Get the correct pool from a field name
V4NodePool& V4NodePools::poolFromFieldName(const char * name) {

  if (!strcmp("appearance", name))    return pools.at(DICT_APPEARANCE);
  if (!strcmp("geometry", name))      return pools.at(DICT_GEOMETRY);
  if (!strcmp("material", name))      return pools.at(DICT_MATERIAL);
  if (!strcmp("texture", name))       return pools.at(DICT_TEXTURE);

  // throws an exception if not found
  return dummy;
}


// Clear -- clears all the pool
void V4NodePools::Clear() {
  for (u32 i = 0; i < pools.size(); i++)
    pools.at(i).Clear();
}


// Add -- Adds a node to the correct pool
void V4NodePools::Add(GF_Node * node) {
  u32 tag = gf_node_get_tag(node);
  if (poolN(tag) == (u32) -1) return;
  pool(tag).AddNode(node);
}


// NodeHasField -- tells if nodes as field of the given type
bool V4NodePools::NodeHasField(GF_Node * node, u32 fieldType) {
  u32 count = gf_node_get_field_count(node);
  GF_FieldInfo field;

  // cycles through all fields and checks field type
  for (u32 i = 0; i < count; i++) {
    gf_node_get_field(node, i, &field);
    if (field.fieldType == fieldType) return true;
  }

  return false;
}


// returns the total number of nodes created for that type of node
u32 V4NodePools::GetCount(const u32 tag) {
  if (poolN(tag) == (u32) -1) return unPooled++;
  return pool(tag).GetCount();
}