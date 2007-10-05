#ifndef _GPACPANEL_H
#define _GPACPANEL_H


#include "safe_include.h" 
#include <gpac/scenegraph.h>
#include <gpac/compositor.h>
/*for service connection...*/
#include <gpac/internal/terminal_dev.h>

class V4SceneManager;
class V4Service;

class wxGPACPanel 
{

public :
	//  Constructor / Desctructor
	wxGPACPanel(V4SceneManager *parent, const char *path);
	~wxGPACPanel();

	 // Access to private members
	GF_Terminal *GetMPEG4Terminal() { return m_term; }	
	GF_Compositor *GetSceneCompositor() { return m_term->compositor; }	
	V4SceneManager *GetV4SceneManager() { return parent; }
	// 
	void Update();

	// Variables used for drag, drop, move, ... picking ...
	s32 m_iDragging;
	u32 m_transformMode; // 0 Move, 1 Scale, 2 Rotate
	GF_Node *picked;
	u32 dragX, dragY;

private:
	void TranslateCoordinates(long x, long y, int &sceneX, int &sceneY);

	V4SceneManager *parent;

	GF_Terminal *m_term;
	GF_User m_user;

	V4Service *m_pService;
};

#endif
