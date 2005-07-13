#ifndef _V4SCENEGRAPH_H
#define _V4SCENEGRAPH_H

#include "safe_include.h" 
#include <gpac/scene_manager.h>
#include <gpac/renderer.h>
#include <gpac/internal/terminal_dev.h> // MPEG4CLIENT


#include <wx/gdicmn.h>


#define DICTNAME "__V4dictionnary"


#define FPS 25


class V4StudioFrame;
class V4Service;

extern "C" {
  GF_Node *CloneNodeForEditing(GF_SceneGraph *inScene, GF_Node *orig);
  int GetNextNodeID(GF_SceneGraph *sg);
}

class V4SceneGraph {
public:
	// Constructor / Desctructor
	V4SceneGraph(V4StudioFrame *parent);
	~V4SceneGraph();


	/* file management functions */

	void SaveFile(const char * path);
	void LoadNew(); 
	void LoadFile(const char *path); 
	void LoadFileOld(const char *path); 


	/* Node management functions */

	GF_Node *NewNode(u32 tag);
	GF_Node *CopyNode(GF_Node *node, GF_Node *parent, bool copy);
	GF_Node *SetTopNode(u32 tag);  // TODO : will destroy dictionnary


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

	// Renderer (owned by the terminal)
	void SetRenderer(GF_Renderer *sr) { m_pSr = sr; }
	GF_Renderer *GetSceneRenderer() { return m_pSr; }

	// GF_InlineScene
	// TODO : Should be modified ?
	LPINLINESCENE GetInlineScene() { return m_pIs; }

	// SceneManager
	// TODO : Should be modified ?
	GF_SceneManager * GetSceneManager() { return m_pSm; }

	// SceneGraph
	GF_SceneGraph *GetSceneGraph() { return m_pSg; }
	GF_Node *GetRootNode() { return gf_sg_get_root_node(m_pSg); }

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
	GF_Node * V4SceneGraph::HasDefParent(GF_Node * node);

	// adds a node to the dictionnary
	void AddGen(GF_Node * node);
	void AddGeometry(GF_Node * node);
	void AddAppearance(GF_Node * node);
	void AddTexture(GF_Node * node);
	void AddMaterial(GF_Node * node);


protected:

	// self created
	LPINLINESCENE m_pIs;
	GF_SceneGraph * m_pSg;
	GF_SceneManager *m_pSm;
	GF_Node * dictionnary;

	// from other objects
	GF_Renderer *m_pSr;
	GF_Terminal *m_term;
	V4StudioFrame * frame;
	
	char *m_pOriginal_mp4;
	Bool m_bEncodeNames;
	V4Service *m_pService;
};

#endif

