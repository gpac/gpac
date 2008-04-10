#include "V4SceneGraph.h"
#include "V4StudioFrame.h"

#include <gpac/scene_manager.h>
#include <gpac/nodes_mpeg4.h>
#include <gpac/internal/scenegraph_dev.h>

#include "V4Service.h"

V4SceneGraph::V4SceneGraph(V4StudioFrame *parent) : frame(parent)
{
	m_pSm = NULL;
	m_pSg = NULL;
	m_pSr = NULL;
	m_pIs = NULL;
	m_pOriginal_mp4 = NULL;
	m_bEncodeNames = 0;
	m_term = parent->GetGPACPanel()->m_term;
}

V4SceneGraph::~V4SceneGraph()
{
  // TODO : pquoi fallait il commenter cette ligne ?
	//if (m_pSm) gf_sm_del(m_pSm);
	if (m_pOriginal_mp4) free(m_pOriginal_mp4);
	if (m_pService) delete m_pService;
}

void V4SceneGraph::SetSceneSize(int w, int h)
{
	gf_sg_set_scene_size_info(m_pSg, w,h, 1);
	/*reassign scene graph to update scene size in renderer*/
	gf_sr_set_scene(m_pSr, m_pSg);
}

void V4SceneGraph::GetSceneSize(wxSize &size) 
{
	gf_sg_get_scene_size_info(m_pSg, (u32 *)&(size.x), (u32 *)&(size.y));	
}

GF_Node *V4SceneGraph::SetTopNode(u32 tag) 
{
	GF_Node * root = NewNode(tag);
	gf_sg_set_root_node(m_pSg, root);
	return (GF_Node *)root;
}


GF_Node *V4SceneGraph::NewNode(u32 tag)
{
	GF_Node *n = gf_node_new(m_pSg, tag);
	if (!n) return NULL;
	gf_node_init(n);
	return n;
}

void V4SceneGraph::SaveFile(const char *path) 
{
	char rad_name[5000];
	strcpy(rad_name, "dump");
	gf_sm_dump(m_pSm, rad_name, 0);
	GF_ISOFile *mp4 = gf_isom_open(path, GF_ISOM_WRITE_EDIT, NULL);
	m_pSm->max_node_id = gf_sg_get_max_node_id(m_pSm->scene_graph);
	gf_sm_encode_to_file(m_pSm, mp4, "c:\\log.txt", NULL, GF_SM_LOAD_MPEG4_STRICT, 0);
	gf_isom_close(mp4);
}

void V4SceneGraph::LoadNew() 
{
	if (m_pSm) {
		gf_sr_set_scene(m_pSr, NULL);
		gf_sm_del(m_pSm);
		m_pSm = NULL;
		gf_sg_del(m_pSg);
		m_pSg = NULL;
	}
	if (m_pOriginal_mp4) free(m_pOriginal_mp4);
	m_pOriginal_mp4 = NULL;

	m_pIs = gf_is_new(NULL);
	m_pSg = m_pIs->graph;
	gf_sg_set_init_callback(m_pSg, v4s_node_init, this);
	gf_sr_set_scene(m_pSr, m_pSg);

	m_pSm = gf_sm_new(m_pSg);
	/* Create a BIFS stream with one AU with on ReplaceScene */
	GF_StreamContext *sc = gf_sm_stream_new(m_pSm, 1, GF_STREAM_SCENE, 0);
	GF_AUContext *au = gf_sm_stream_au_new(sc, 0, 0, 1);
	GF_Command *command = gf_sg_command_new(m_pSg, GF_SG_SCENE_REPLACE);
	gf_list_add(au->commands, command);

	// reinitializes the node pool
	frame->pools.Clear();
}

void V4SceneGraph::LoadFileOld(const char *path) 
{
	m_pService = new V4Service(path);
	gf_term_attach_service(m_term, m_pService->m_pNetClient);
	m_pIs = m_term->root_scene;
	m_pSg = m_pIs->graph;
	gf_term_play_from_time(m_term, 0);
	while(!m_pIs->graph_attached) {}
	// CreateDictionnary crashes because the root node in m_pSg is not set.
	CreateDictionnary();
}

void V4SceneGraph::LoadFile(const char *path) 
{
	GF_SceneLoader load;
	if (m_pSm) {
		gf_sr_set_scene(m_pSr, NULL);
		gf_sm_del(m_pSm);
		gf_sg_del(m_pSg);
	}
	
	// initializes a new scene
	// We need an GF_InlineScene, a SceneManager and an GF_ObjectManager
	m_pIs = gf_is_new(NULL);
	m_pSg = m_pIs->graph;
	m_pSm = gf_sm_new(m_pSg);

	m_pIs->root_od = gf_odm_new();

	m_pIs->root_od->parentscene = NULL;
	m_pIs->root_od->subscene = m_pIs;
	m_pIs->root_od->term = m_term;

	m_term->root_scene = m_pIs;
	
	// TODO : what's the use of this ?
	if (m_pOriginal_mp4) free(m_pOriginal_mp4);
	m_pOriginal_mp4 = NULL;

	/* Loading of a file (BT, MP4 ...) and modification of the SceneManager */
	memset(&load, 0, sizeof(GF_SceneLoader));
	load.fileName = path;
	load.ctx = m_pSm;
	load.cbk = this;
	if (strstr(path, ".mp4") || strstr(path, ".MP4") ) load.isom = gf_isom_open(path, GF_ISOM_OPEN_READ, NULL);
	gf_sm_load_init(&load);
	gf_sm_load_run(&load);
	gf_sm_load_done(&load);
	if (load.isom) gf_isom_delete(load.isom);

	/* SceneManager should be initialized  and filled correctly */

	gf_sg_set_scene_size_info(m_pSg, m_pSm->scene_width, m_pSm->scene_height, m_pSm->is_pixel_metrics);

	// TODO : replace with GetBifsStream
	GF_StreamContext *sc = (GF_StreamContext *) gf_list_get(m_pSm->streams,0);

	if (sc->streamType == 3) {
		GF_AUContext *au = (GF_AUContext *) gf_list_get(sc->AUs,0);
		GF_Command *c = (GF_Command *) gf_list_get(au->commands,0);
		gf_sg_command_apply(m_pSg, c, 0);
		/* This is a patch to solve the save pb:
		    When ApplyCommand is made on a Scene Replace Command
		    The command node is set to NULL
		    When we save a BIFS stream whose first command is of this kind,
		    the file saver thinks the bifs commands should come from an NHNT file 
		   This is a temporary patch */
		if (c->tag == GF_SG_SCENE_REPLACE) { c->node = m_pSg->RootNode; }
	}
	gf_sr_set_scene(m_pSr, m_pSg);

	// retrieves all the node from the tree and adds them to the node pool
	GF_Node * root = gf_sg_get_root_node(m_pSg);
	CreateDictionnary();
}

GF_Node *V4SceneGraph::CopyNode(GF_Node *node, GF_Node *parent, bool copy) 
{
	if (copy) return CloneNodeForEditing(m_pSg, node);
	u32 nodeID = gf_node_get_id(node);
	if (!nodeID) {
		nodeID = gf_sg_get_next_available_node_id(m_pSg);
		gf_node_set_id(node, nodeID, NULL);
		gf_node_register(node, parent);
	}
	return node;
}

void V4SceneGraph::LoadCommand(u32 commandNumber)
{
	GF_StreamContext *sc = NULL;
	for (u32 i=0; i<gf_list_count(m_pSm->streams); i++) {
		sc = (GF_StreamContext *) gf_list_get(m_pSm->streams, i);
		if (sc->streamType == GF_STREAM_SCENE) break;
		sc = NULL;
	}
	assert(sc);
	GF_AUContext *au = (GF_AUContext *) gf_list_get(sc->AUs,0);
	if (commandNumber < gf_list_count(au->commands)) {
		for (u32 j=0; j<=commandNumber; j++) {
			GF_Command *c = (GF_Command *) gf_list_get(au->commands, j);
			gf_sg_command_apply(m_pSg, c, 0);
		}
	}
}



extern "C" {

GF_Node *CloneNodeForEditing(GF_SceneGraph *inScene, GF_Node *orig) //, GF_Node *cloned_parent)
{
	u32 i, j, count;
	GF_Node *node, *child, *tmp;
	GF_List *list, *list2;
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
			list = *( (GF_List **) field_orig.far_ptr);
			list2 = *( (GF_List **) field.far_ptr);

			for (j=0; j<gf_list_count(list); j++) {
				tmp = (GF_Node *)gf_list_get(list, j);
				child = CloneNodeForEditing(inScene, tmp);//, node);
				gf_list_add(list2, child);
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

}


// GetBifsStream -- gets the first stream with id = 3
GF_StreamContext * V4SceneGraph::GetBifsStream() {
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


