#include "V4SceneManager.h"
#include "V4StudioFrame.h"

#include <gpac/scene_manager.h>
#include <gpac/nodes_mpeg4.h>
#include <gpac/internal/scenegraph_dev.h>
#include <gpac/constants.h>


V4SceneManager::V4SceneManager(V4StudioFrame *parent) : frame(parent)
{
	m_pSm = NULL;
	m_pIs = NULL;
	units = 1000; // used to calculate AU timings
}

V4SceneManager::~V4SceneManager()
{
	// m_pIs should be deleted here or by the terminal

	// m_pSm should be deleted here or by the terminal
	// TODO : pquoi fallait il commenter cette ligne ?
	//if (m_pSm) gf_sm_del(m_pSm);
	delete m_gpac_panel;
	m_gpac_panel = NULL;
}

void v4s_node_init(void *cbkObject, u32 type, GF_Node *node, void *param)
{
	if (type==GF_SG_CALLBACK_INIT) {
		switch (gf_node_get_tag(node)) {
		case TAG_MPEG4_Conditional:
		case TAG_MPEG4_QuantizationParameter:
			break;
		default:
			gf_sc_on_node_init(((V4SceneManager *)cbkObject)->GetSceneCompositor(), node);
			break;
		}
	}
}

void V4SceneManager::LoadNew() 
{
	m_pIs = gf_inline_new(NULL);
	gf_sg_set_private(m_pIs->graph, this);
	gf_sg_set_node_callback(m_pIs->graph, v4s_node_init);
	m_gpac_panel = new wxGPACPanel(this, NULL);
	GF_Terminal *term = m_gpac_panel->GetMPEG4Terminal();
	/* Cannot set the new scene because no root node 
	   gf_sr_set_scene(term->renderer, m_pIs->graph);
	   */

	m_pSm = gf_sm_new(m_pIs->graph);
	/* Create a BIFS stream with one AU with on ReplaceScene */
	GF_StreamContext *sc = gf_sm_stream_new(m_pSm, 1, GF_STREAM_SCENE, 0);
	GF_AUContext *au = gf_sm_stream_au_new(sc, 0, 0, 1);
	GF_Command *command = gf_sg_command_new(m_pIs->graph, GF_SG_SCENE_REPLACE);
	gf_list_add(au->commands, command);

	SetLength(50);
	SetFrameRate(25);

	// reinitializes the node pool
	pools.Clear();
}

void V4SceneManager::LoadFileOld(const char *path) 
{
	m_gpac_panel = new wxGPACPanel(this, path);
	GF_Terminal *term = m_gpac_panel->GetMPEG4Terminal();

	// Get the RootScene created by the terminal.
	m_pIs = term->root_scene;


	if (0) {
		//Create a Scene Manager based on the current scene_graph
		m_pSm = gf_sm_new(m_pIs->graph);

		/* Loading of a file (BT, MP4 ...) and modification of the SceneManager */
		GF_SceneLoader load;
		memset(&load, 0, sizeof(GF_SceneLoader));
		load.fileName = path;
		load.ctx = m_pSm;
		if (strstr(path, ".mp4") || strstr(path, ".MP4") ) load.isom = gf_isom_open(path, GF_ISOM_OPEN_READ, NULL);
		gf_sm_load_init(&load);
		gf_sm_load_run(&load);
		gf_sm_load_done(&load);
		if (load.isom) gf_isom_delete(load.isom);

		/* SceneManager should be initialized and filled correctly */
		gf_sg_set_scene_size_info(m_pIs->graph, m_pSm->scene_width, m_pSm->scene_height, m_pSm->is_pixel_metrics);

		// TODO : replace with GetBifsStream
		GF_StreamContext *sc = (GF_StreamContext *) gf_list_get(m_pSm->streams,0);
		gf_list_count(m_pSm->streams);

		if (sc->streamType == 3) {
			GF_AUContext *au = (GF_AUContext *) gf_list_get(sc->AUs,0);
		gf_list_count(sc->AUs);
			GF_Command *c = (GF_Command *) gf_list_get(au->commands,0);
		gf_list_count(au->commands);
			gf_sg_command_apply(m_pIs->graph, c, 0);
			/* This is a patch to solve the save pb:
				When ApplyCommand is made on a Scene Replace Command
				The command node is set to NULL
				When we save a BIFS stream whose first command is of this kind,
				the file saver thinks the bifs commands should come from an NHNT file 
			   This is a temporary patch */
			if (c->tag == GF_SG_SCENE_REPLACE) { c->node = m_pIs->graph->RootNode; }
		}
		gf_sc_set_scene(term->compositor, m_pIs->graph);

		// retrieves all the node from the tree and adds them to the node pool
		GF_Node * root = gf_sg_get_root_node(m_pIs->graph);
		CreateDictionnary();

	}

}

void V4SceneManager::LoadFile(const char *path) 
{	
	m_gpac_panel = new wxGPACPanel(this, NULL);
	GF_Terminal *term = m_gpac_panel->GetMPEG4Terminal();

	// initializes a new scene
	// We need an GF_InlineScene, a SceneManager and an GF_ObjectManager
	m_pIs = gf_inline_new(NULL);
	m_pSm = gf_sm_new(m_pIs->graph);

	m_pIs->root_od = gf_odm_new();
	m_pIs->root_od->parentscene = NULL;
	m_pIs->root_od->subscene = m_pIs;
	m_pIs->root_od->term = term;

	term->root_scene = m_pIs;
	
	/* Loading of a file (BT, MP4 ...) and modification of the SceneManager */
	GF_SceneLoader load;
	memset(&load, 0, sizeof(GF_SceneLoader));
	load.fileName = path;
	load.ctx = m_pSm;
	if (strstr(path, ".mp4") || strstr(path, ".MP4") ) load.isom = gf_isom_open(path, GF_ISOM_OPEN_READ, NULL);
	gf_sm_load_init(&load);
	gf_sm_load_run(&load);
	gf_sm_load_done(&load);
	if (load.isom) gf_isom_delete(load.isom);

	/* SceneManager should be initialized  and filled correctly */

	gf_sg_set_scene_size_info(m_pIs->graph, m_pSm->scene_width, m_pSm->scene_height, m_pSm->is_pixel_metrics);

	// TODO : replace with GetBifsStream
	GF_StreamContext *sc = (GF_StreamContext *) gf_list_get(m_pSm->streams,0);

	if (sc->streamType == 3) {
		GF_AUContext *au = (GF_AUContext *) gf_list_get(sc->AUs,0);
		GF_Command *c = (GF_Command *) gf_list_get(au->commands,0);
		gf_sg_command_apply(m_pIs->graph, c, 0);
		/* This is a patch to solve the save pb:
		    When ApplyCommand is made on a Scene Replace Command
		    The command node is set to NULL
		    When we save a BIFS stream whose first command is of this kind,
		    the file saver thinks the bifs commands should come from an NHNT file 
		   This is a temporary patch */
		if (c->tag == GF_SG_SCENE_REPLACE) { c->node = m_pIs->graph->RootNode; }
	}
	gf_sc_set_scene(term->compositor, m_pIs->graph);

	// TODO : read actual values from file
	SetLength(50);
	SetFrameRate(25);

	// retrieves all the node from the tree and adds them to the node pool
	GF_Node * root = gf_sg_get_root_node(m_pIs->graph);
	CreateDictionnary();
}

void V4SceneManager::SaveFile(const char *path) 
{
	GF_SMEncodeOptions opts;
	char rad_name[5000];
	strcpy(rad_name, "dump");
	gf_sm_dump(m_pSm, rad_name, 0);
	GF_ISOFile *mp4 = gf_isom_open(path, GF_ISOM_WRITE_EDIT, NULL);
	m_pSm->max_node_id = gf_sg_get_max_node_id(m_pSm->scene_graph);
	memset(&opts, 0, sizeof(opts));
	opts.flags = GF_SM_LOAD_MPEG4_STRICT;
	gf_sm_encode_to_file(m_pSm, mp4, &opts);
	gf_isom_close(mp4);
}

void V4SceneManager::SetSceneSize(int w, int h)
{
	gf_sg_set_scene_size_info(m_pIs->graph, w,h, 1);
	/*reassign scene graph to update scene size in renderer*/
	gf_sc_set_scene(m_gpac_panel->GetMPEG4Terminal()->compositor, m_pIs->graph);
}

void V4SceneManager::GetSceneSize(wxSize &size) 
{
	gf_sg_get_scene_size_info(m_pIs->graph, (u32 *)&(size.x), (u32 *)&(size.y));	
}

GF_Node *V4SceneManager::SetTopNode(u32 tag) 
{
	M_OrderedGroup * root = (M_OrderedGroup *)NewNode(TAG_MPEG4_OrderedGroup);
	gf_sg_set_root_node(m_pIs->graph, (GF_Node *)root);
	return (GF_Node *)root;
}


GF_Node *V4SceneManager::NewNode(u32 tag)
{
	GF_Node *n = gf_node_new(m_pIs->graph, tag);
	if (!n) return NULL;
	gf_node_init(n);
	return n;
}

GF_Node *V4SceneManager::CopyNode(GF_Node *node, GF_Node *parent, bool copy) 
{
	if (copy) return CloneNodeForEditing(m_pIs->graph, node);
	u32 nodeID = gf_node_get_id(node);
	if (!nodeID) {
		nodeID = gf_sg_get_next_available_node_id(m_pIs->graph);
		gf_node_set_id(node, nodeID, NULL);
		gf_node_register(node, parent);
	}
	return node;
}

extern "C" {

GF_Node *CloneNodeForEditing(GF_SceneGraph *inScene, GF_Node *orig) //, GF_Node *cloned_parent)
{
	u32 i, j, count;
	GF_Node *node, *child, *tmp;
	GF_ChildNodeItem *list, *list2;
	GF_FieldInfo field_orig, field;

	/*this is not a mistake*/
	if (!orig) return NULL;

	/*check for DEF/USE
	if (orig->sgprivate->NodeID) {
		node = gf_sg_find_node(inScene, orig->sgprivate->NodeID);
		//node already created, USE
		if (node) {
			gf_node_register(node, cloned_parent);
			return node;
		}
	}
	*/
	/*create a node*/
/*
	if (orig->sgprivate->tag == TAG_MPEG4_ProtoNode) {
		proto_node = ((GF_ProtoInstance *)orig)->proto_interface;
		//create the instance but don't load the code -c we MUST wait for ISed routes to be cloned before
		node = gf_sg_proto_create_node(inScene, proto_node, (GF_ProtoInstance *) orig);
	} else {
*/
	node = gf_node_new(inScene, gf_node_get_tag(orig));
//	}

	count = gf_node_get_field_count(orig);

	/*copy each field*/
	for (i=0; i<count; i++) {
		gf_node_get_field(orig, i, &field_orig);

		/*get target ptr*/
		gf_node_get_field(node, i, &field);

		assert(field.eventType==field_orig.eventType);
		assert(field.fieldType==field_orig.fieldType);

		/*duplicate it*/
		switch (field.fieldType) {
		case GF_SG_VRML_SFNODE:
			child = CloneNodeForEditing(inScene, (GF_Node *) (* ((GF_Node **) field_orig.far_ptr)));//, node);
			*((GF_Node **) field.far_ptr) = child;
			break;
		case GF_SG_VRML_MFNODE:
			list = *( (GF_ChildNodeItem **) field_orig.far_ptr);
			list2 = *( (GF_ChildNodeItem **) field.far_ptr);

			for (j=0; j<gf_node_list_get_count(list); j++) {
				tmp = (GF_Node *)gf_node_list_get_child(list, j);
				child = CloneNodeForEditing(inScene, tmp);//, node);
				gf_node_list_add_child(&list2, child);
			}
			break;
		default:
			gf_sg_vrml_field_copy(field.far_ptr, field_orig.far_ptr, field.fieldType);
			break;
		}
	}
	/*register node
	if (orig->sgprivate->NodeID) {
		Node_SetID(node, orig->sgprivate->NodeID);
		gf_node_register(node, cloned_parent);
	}*/

	/*init node before creating ISed routes so the eventIn handler are in place*/
	if (gf_node_get_tag(node) != TAG_ProtoNode) gf_node_init(node);

	return node;
}

} // extern "C"


// GetBifsStream -- gets the first stream with id = 3
GF_StreamContext * V4SceneManager::GetBifsStream() {
  GF_StreamContext * ctx = NULL;
  u32 count = gf_list_count(m_pSm->streams);

  // cycle trough each stream
  while (count--) {
    ctx = (GF_StreamContext *) gf_list_get(m_pSm->streams, count);
    if (ctx->streamType == 3) break;
  }

  if (ctx == NULL) return NULL;

  // if we went through the loop without finding any BIFS stream return null
  if (ctx->streamType != 3) return NULL;

  return ctx;
}


// CreateDictionnary -- Create the root node of the dictionnary and populates it
void V4SceneManager::CreateDictionnary() {
  GF_Node * root = gf_sg_get_root_node(m_pIs->graph);
  dictionnary = NewNode(TAG_MPEG4_Switch);

  // Insert the dictionnary in the scene and gives it a name
  gf_node_insert_child(root, dictionnary, 0);
  gf_node_register(dictionnary, root);
  gf_node_set_id(dictionnary, gf_sg_get_next_available_node_id(m_pIs->graph), DICTNAME);

  // makes the dictionnary invisible
  // the number 1 field is the "whichChoice", setting it to -1 makes it display nothing
  GF_FieldInfo field;
  gf_node_get_field(dictionnary, 1, &field);
  frame->GetFieldView()->SetFieldValue(field, &wxString("-1"), -1);  // TODO : maybe put the method SetFieldValue somewhere more accessible

  // populates dictionnary with the root node of the scene
  AddToDictionnary(root);
}


// AddToDictionnary -- adds a node to the dictionnary, increases its reference count, the node must already have an ID
// WARNING : does not add the node to the pool if it has a parent in the dictionnary
void V4SceneManager::AddToDictionnary(GF_Node * node) {

  // checks if nodes has a parent with an id
  // if true then there is already a reference to this node 
  if (HasDefParent(node)) return;

  AddRecursive(node);
}


// AddRecursive -- recursively adds items with an ID to the dictionnary
void V4SceneManager::AddRecursive(GF_Node * node, bool parentAdded) {

  // skips empty nodes
  if (!node) return;

  // skips the dictionnary
  const char * c = gf_node_get_name(node);
  if ( (c != NULL) && (!strcmp(c, DICTNAME)) ) return;

  // if node as an id adds it to the dictionnary and the node pool
  u32 id = gf_node_get_id(node);
  if (id) {
    pools.Add(node);
    // children of added node are not added to the dictionnary
    if (!parentAdded) AddEffective(node);
    parentAdded = true;
  }
  
  GF_FieldInfo field;
  GF_ChildNodeItem * list;
  int count = gf_node_get_field_count(node);

  // tests all fields, if a field is a node then adds it and process its children recursively
  for (int i=0; i<count; i++) {

    gf_node_get_field(node, i, &field);

    // single value node field
    if (field.fieldType == GF_SG_VRML_SFNODE)
      AddRecursive( * ((GF_Node **) field.far_ptr), parentAdded );

    // multiple value node field
    if (field.fieldType == GF_SG_VRML_MFNODE) {
      list = *( (GF_ChildNodeItem **) field.far_ptr);
			for (u32 j=0; j<gf_node_list_get_count(list); j++)
				AddRecursive( (GF_Node *) gf_node_list_get_child(list, j), parentAdded );
    }
  }
}


// AddEffective --
void V4SceneManager::AddEffective(GF_Node * node) {

  // gets the node type (equals pool number)
  u32 poolN = pools.poolN(gf_node_get_tag(node));

  // adds the node using a type specific function
  switch (poolN) {
    case DICT_GEN: {
      AddGen(node);
      break;
    }

    case DICT_GEOMETRY: {
      AddGeometry(node);
      break;
    }

    case DICT_APPEARANCE: {
      AddAppearance(node);
      break;
    }

    case DICT_TEXTURE: {
      AddTexture(node);
      break;
    }

    case DICT_MATERIAL: {
      AddMaterial(node);
      break;
    }

    // TODO : raise an exception when not found
  }

}


// RemoveFromDictionnary -- Removes _ONE_ item from the dictionnary, if some of the children are also in the dictionnary preserves them
void V4SceneManager::RemoveFromDictionnary(GF_Node * node) {

}


/******* Add element to dictionnary functions *******/

// TODO : can add genericity around here
// ATTENTION, all these functions thinks the node as already been assigned an ID

// Add gen -- adds any element which can be a children of a switch node
void V4SceneManager::AddGen(GF_Node * node) {

  // retrieves the chain of children (field number 0 named "choice")
  GF_FieldInfo field;
  GF_List * list;

  gf_node_get_field(dictionnary, 0, &field);
  list = *( (GF_List **) field.far_ptr);

  // adds a reference to that node in the dictionnary
  gf_list_add(list, node);
  gf_node_register(node, dictionnary);

}


// TODO : check if there is a dummy node with free space in it


// AddGeometry -- adds any element which can be a children of a shape as a geometry node
void V4SceneManager::AddGeometry(GF_Node * node) {

  // creates a dummy node to hold this one
  GF_Node * parent = CreateDummyNode(TAG_MPEG4_Shape, dictionnary, 0);

  // sets the parentship relation
  MakeChild(node, parent, 1); // geometry is field one in a shape

}


// AddAppearance -- adds any element which can be a children of a shape as an appearance node
void V4SceneManager::AddAppearance(GF_Node * node) {

  // creates a dummy node to hold this one
  GF_Node * parent = CreateDummyNode(TAG_MPEG4_Shape, dictionnary, 0);

  // sets the parentship relation
  MakeChild(node, parent, 0); // appearance is field zero in a shape
}


// AddTexture -- adds any element which can be a children of an appearance as a texture node
void V4SceneManager::AddTexture(GF_Node * node) {

  // creates dummy nodes to hold this one
  GF_Node * parent1 = CreateDummyNode(TAG_MPEG4_Shape, dictionnary, 0);
  GF_Node * parent2 = CreateDummyNode(TAG_MPEG4_Appearance, parent1, 3);

  // sets the parentship relation
  MakeChild(node, parent2, 1); // texture is field one in a appearance

}


// AddMaterial -- adds any element which can be a children of an appearance as a material node
void V4SceneManager::AddMaterial(GF_Node * node) {

    // creates dummy nodes to hold this one
  GF_Node * parent1 = CreateDummyNode(TAG_MPEG4_Shape, dictionnary, 0);
  GF_Node * parent2 = CreateDummyNode(TAG_MPEG4_Appearance, parent1, 3);

  // sets the parentship relation
  MakeChild(node, parent2, 0); // material is field zero in appearance
}


// CreateDummyNode -- creates a node to hold another one, adds this node to given field of the given parent
// (designed for use with nodes in the dictionnary)
GF_Node * V4SceneManager::CreateDummyNode(const u32 tag, GF_Node * parent, const u32 fieldIndex) {

  // creates a new node for this scene
  GF_Node * newNode = NewNode(tag);

  // sets the parentship relation !! AND registers the node !!
  MakeChild(newNode, parent, fieldIndex);

  return newNode;
}


// MakeChild -- Sets the first node as a child of the second using the given field
void V4SceneManager::MakeChild(GF_Node * child, GF_Node * parent, const u32 fieldIndex) {
  // get the field where we have to add the new child
  GF_FieldInfo field;
  gf_node_get_field(parent, fieldIndex, &field);

  // keeps the count of uses up to date
  gf_node_register(child, parent);

  // if the field is single valued
  if (field.fieldType == GF_SG_VRML_SFNODE) {
    *((GF_Node **) field.far_ptr) = child;
  }

  // if the field is multi valued
  if (field.fieldType == GF_SG_VRML_MFNODE)
    gf_node_list_add_child((GF_ChildNodeItem **)field.far_ptr, child );

}


// GetDictionnary -- access to the dictionnary
GF_Node * V4SceneManager::GetDictionnary() const {
  return dictionnary;  
}


// HasDefParent -- checks if an ancestor of this node has an ID
GF_Node * V4SceneManager::HasDefParent(GF_Node * node) {
  
  assert(node);

  // also checks the node itself

  int count = gf_node_get_parent_count(node);
  GF_Node * parent;

  // loops through all the parents and recurses
  for (int i=0; i<count; i++) {
    parent = gf_node_get_parent(node, i);
    const char * name = gf_node_get_name(parent);
    if (name) return parent;

    parent = HasDefParent(parent);
    if (parent) return parent;
  }

  return NULL;
}

void V4SceneManager::CreateIDandAddToPool(GF_Node *node)
{
	// gets a node ID
	u32 id = gf_sg_get_next_available_node_id(GetSceneGraph());

	char d[10]; // supposes no more than 1 000 000 000 objects

	// queries the pool to know which number to use for the name
	itoa(pools.GetCount(gf_node_get_tag(node)), d, 10); 

	char c[50];
	strcpy(c, gf_node_get_class_name(node));
	strncat(c, "_", 1);
	strncat(c, d, 10);

	gf_node_set_id(node, id, c); // assigns NodeID and name
	pools.Add(node); // adds to the node pool

}


// SetLength -- updates the number of cells in the timeline
void V4SceneManager::SetLength(const u32 length_) {
  length = length_;
  frame->GetTimeLine()->SetLength(length_);
}