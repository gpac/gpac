#ifndef _V4SceneManager_H
#define _V4SceneManager_H

#include "safe_include.h" 
#include <gpac/scene_manager.h>
#include <gpac/compositor.h>
#include <gpac/internal/terminal_dev.h> // MPEG4CLIENT
#include "wxGPACPanel.h"
#include "V4NodePools.h"


#include <wx/gdicmn.h>


#define DICTNAME "__V4dictionnary"


class V4StudioFrame;

extern "C" {
  GF_Node *CloneNodeForEditing(GF_SceneGraph *inScene, GF_Node *orig);
  int GetNextNodeID(GF_SceneGraph *sg);
}

class V4SceneManager {
public:
	// Constructor / Desctructor
	V4SceneManager(V4StudioFrame *parent);
	~V4SceneManager();

	// reusable nodes, sorted by tag
	V4NodePools pools;

	/* file management functions */

	void SaveFile(const char * path);
	
	void LoadCommon(); 
	void LoadNew(); 
	void LoadFile(const char *path); 

	/* Node management functions */

	GF_Node *NewNode(u32 tag);
	GF_Node *CopyNode(GF_Node *node, GF_Node *parent, bool copy);
	GF_Node *SetTopNode(u32 tag);  // TODO : will destroy dictionnary

	void CreateIDandAddToPool(GF_Node *node);

	/* Dictionnary functions */

	// adds a node to the dictionnary, increases its reference count, the node must already have an ID
	void AddToDictionnary(GF_Node * node);

	// removes a node from the dictionnary = DELETES it's ID
	void RemoveFromDictionnary(GF_Node * node);

	GF_Node * GetDictionnary() const;


	/* Various */

	void LoadCommand(u32 commandNumber); // ?
	GF_StreamContext * GetBifsStream();   // Get the first stream with id = 3


	/* access to private members */

	// SceneSize
	void SetSceneSize(int w, int h);
	void GetSceneSize(wxSize &size);

  // FrameRate
  void SetFrameRate(const u32 framerate_) { frameRate = framerate_; }
  u32 GetFrameRate() const { return frameRate; }

  // accesses Length
  void SetLength(const u32 length_);
  u32 GetLength() const { return length; }

  // accesses units
  void SetUnits(u32 units_) { units = units_; }
  u32 GetUnits() const { return units; }



	GF_Compositor *GetSceneCompositor() { return m_gpac_panel->GetSceneCompositor(); }

	// GF_InlineScene
	GF_InlineScene	*GetInlineScene() { return m_pIs; }

	// SceneGraph
	GF_SceneGraph	*GetSceneGraph() { return m_pIs->graph; }
	GF_Node			*GetRootNode() { return gf_sg_get_root_node(m_pIs->graph); }

	wxGPACPanel		*GetGPACPanel() { return m_gpac_panel; }
	V4StudioFrame	*GetV4StudioFrame() { return frame; }

	// TODO : terminal
	private:

	/* dictionnary function */

	// initializes the dictionnary node
	void CreateDictionnary();

	// Finds all node with an ID from the given node and adds them to the node pool and the dictionnary
	void AddRecursive(GF_Node * node, bool parentAdded = false);

	// calls a proper function to add the node according to its type
	void AddEffective(GF_Node * node);

	// Create a dummy node of type tag and it to the specified field (or reimplace if the field is single valued)
	GF_Node * CreateDummyNode(const u32 tag, GF_Node * parent, const u32 fieldIndex);

	// Sets the first node as a child of the second using the given field
	void MakeChild(GF_Node * child, GF_Node * parent, const u32 fieldIndex);

	// Checks is specified node as a parent with an ID
	GF_Node * V4SceneManager::HasDefParent(GF_Node * node);

	// adds a node to the dictionnary
	void AddGen(GF_Node * node);
	void AddGeometry(GF_Node * node);
	void AddAppearance(GF_Node * node);
	void AddTexture(GF_Node * node);
	void AddMaterial(GF_Node * node);


protected:

	// self created
	LPINLINESCENE	m_pIs;
	GF_SceneManager	*m_pSm;
	GF_Node			*dictionnary;
	wxGPACPanel		*m_gpac_panel;

	// from other objects
	V4StudioFrame	*frame;

  u8  frameRate;
  u32 length;            // in frames!
  u32 units;             // = 1000
	
};

#endif

