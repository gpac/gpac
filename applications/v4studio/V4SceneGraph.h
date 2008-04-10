#ifndef _V4SCENEGRAPH_H
#define _V4SCENEGRAPH_H

#include "safe_include.h" 
#include <gpac/scene_manager.h>
#include <gpac/compositor.h>
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

	/* Various */

	void LoadCommand(u32 commandNumber); // ?
	GF_StreamContext * GetBifsStream();   // Get the first stream with id = 3


	/* access to private members */

	// SceneSize
	void SetSceneSize(int w, int h);
	void GetSceneSize(wxSize &size);

	// Renderer (owned by the terminal)
	void SetRenderer(GF_Compositor *sr) { m_pSr = sr; }
	GF_Compositor *GetSceneCompositor() { return m_pSr; }

	// GF_InlineScene
	// TODO : Should be modified ?
	LPINLINESCENE GetInlineScene() { return m_pIs; }

	// SceneManager
	// TODO : Should be modified ?
	GF_SceneManager * GetSceneManager() { return m_pSm; }

	// SceneGraph
	GF_SceneGraph *GetSceneGraph() { return m_pSg; }
	GF_Node *GetRootNode() { return gf_sg_get_root_node(m_pSg); }

protected:

	// self created
	LPINLINESCENE m_pIs;
	GF_SceneGraph * m_pSg;
	GF_SceneManager *m_pSm;
	GF_Node * dictionnary;

	// from other objects
	GF_Compositor *m_pSr;
	GF_Terminal *m_term;
	V4StudioFrame * frame;
	
	char *m_pOriginal_mp4;
	Bool m_bEncodeNames;
	V4Service *m_pService;
};

#endif

