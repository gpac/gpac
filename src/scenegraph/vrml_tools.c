/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Graph sub-project
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <gpac/internal/scenegraph_dev.h>

/*MPEG4 & X3D tags (for node tables & script handling)*/
#include <gpac/nodes_mpeg4.h>
#include <gpac/nodes_x3d.h>


#ifndef GPAC_DISABLE_VRML
#include <gpac/internal/bifs_dev.h>


GF_EXPORT
Bool gf_node_in_table_by_tag(u32 tag, u32 NDTType)
{
	if (!tag) return 0;
	if (tag==TAG_ProtoNode) return 1;
	else if (tag<=GF_NODE_RANGE_LAST_MPEG4) {
#ifndef GPAC_DISABLE_BIFS
		u32 i;
		for (i=0; i<GF_BIFS_LAST_VERSION; i++) {
			if (gf_bifs_get_node_type(NDTType, tag, i+1)) return 1;
		}
		return 0;
#else
		/*if BIFS is disabled, we don't have the NDTs - we therefore allow any node in any table otherwise we would reject
		them all*/
		return 1;
#endif

	}
#ifndef GPAC_DISABLE_X3D
	else if (tag<=GF_NODE_RANGE_LAST_X3D) {
		return gf_x3d_get_node_type(NDTType, tag);
	}
#endif
	return 0;
}


static void Node_on_add_children(GF_Node *node, GF_Route *route)
{
	GF_ChildNodeItem *list;
	GF_FieldInfo field;
	GF_VRMLParent *n = (GF_VRMLParent *)node;

	if (n->children) {
		list = n->children;
		while (list->next) list = list->next;
		list->next = n->addChildren;
	} else {
		n->children = n->addChildren;
	}
	n->addChildren = NULL;

	/*signal children field is modified*/
	field.name = "children";
	field.eventType = GF_SG_EVENT_EXPOSED_FIELD;
	field.fieldType = GF_SG_VRML_MFNODE;
	field.NDTtype = -1;
	if ( node->sgprivate->tag == TAG_MPEG4_Transform )
		field.fieldIndex = 3;
	else
		field.fieldIndex = 2;
	field.far_ptr = & n->children;
	gf_node_event_out(node, field.fieldIndex);
	gf_node_changed(node, &field);

	if (node->sgprivate->scenegraph->on_node_modified) {
		field.name = "addChildren";
		field.eventType = GF_SG_EVENT_IN;
		field.fieldType = GF_SG_VRML_MFNODE;
		field.NDTtype = -1;
		field.fieldIndex = 0;
		field.far_ptr = & n->addChildren;
		node->sgprivate->scenegraph->on_node_modified(node->sgprivate->scenegraph, node, &field, NULL);
	}
}

static void Node_on_remove_children(GF_Node *node, GF_Route *route)
{
	GF_ChildNodeItem *list;
	GF_FieldInfo field;
	GF_VRMLParent *n = (GF_VRMLParent *)node;

	if (!n->removeChildren) return;

	list = n->removeChildren;
	while (list) {
		if (gf_node_list_del_child(& n->children, list->node)) {
			gf_node_unregister(list->node, node);
		}
		list = list->next;
	}
	gf_node_unregister_children(node, n->removeChildren);
	n->removeChildren = NULL;

	/*signal children field is modified*/
	field.name = "children";
	field.eventType = GF_SG_EVENT_EXPOSED_FIELD;
	field.fieldType = GF_SG_VRML_MFNODE;
	field.NDTtype = -1;
	if ( node->sgprivate->tag == TAG_MPEG4_Transform )
		field.fieldIndex = 3;
	else
		field.fieldIndex = 2;
	field.far_ptr = & n->children;
	gf_node_event_out(node, field.fieldIndex);
	gf_node_changed(node, &field);


	if (node->sgprivate->scenegraph->on_node_modified) {
		field.name = "removeChildren";
		field.eventType = GF_SG_EVENT_IN;
		field.fieldType = GF_SG_VRML_MFNODE;
		field.NDTtype = -1;
		field.fieldIndex = 1;
		field.far_ptr = & n->removeChildren;
		node->sgprivate->scenegraph->on_node_modified(node->sgprivate->scenegraph, node, &field, NULL);
	}
}

void gf_sg_vrml_parent_setup(GF_Node *pNode)
{
	GF_VRMLParent *par = (GF_VRMLParent *)pNode;
	par->children = NULL;
	par->addChildren = NULL;
	par->on_addChildren = Node_on_add_children;
	par->removeChildren = NULL;
	par->on_removeChildren = Node_on_remove_children;
	pNode->sgprivate->flags |= GF_SG_CHILD_DIRTY;
}

void gf_sg_vrml_parent_destroy(GF_Node *pNode)
{
	GF_VRMLParent *par = (GF_VRMLParent *)pNode;
	gf_node_unregister_children(pNode, par->children);
	gf_node_unregister_children(pNode, par->addChildren);
	gf_node_unregister_children(pNode, par->removeChildren);
}

GF_EXPORT
GF_Err gf_sg_delete_all_protos(GF_SceneGraph *scene)
{
	if (!scene) return GF_BAD_PARAM;
	while (gf_list_count(scene->protos)) {
		GF_Proto *p = (GF_Proto *)gf_list_get(scene->protos, 0);
		gf_sg_proto_del(p);
	}
	return GF_OK;
}

GF_EXPORT
void gf_sg_set_proto_loader(GF_SceneGraph *scene, GF_SceneGraph *(*GetExternProtoLib)(void *SceneCallback, MFURL *lib_url))
{
	if (!scene) return;
	scene->GetExternProtoLib = GetExternProtoLib;
}

GF_EXPORT
u32 gf_sg_get_next_available_route_id(GF_SceneGraph *sg)
{
	u32 i, count;
	u32 ID = 0;

	if (!sg->max_defined_route_id) {
		count = gf_list_count(sg->Routes);
		/*routes are not sorted*/
		for (i=0; i<count; i++) {
			GF_Route *r = (GF_Route *)gf_list_get(sg->Routes, i);
			if (ID<=r->ID) ID = r->ID;
		}
		return ID+1;
	} else {
		sg->max_defined_route_id++;
		return sg->max_defined_route_id;
	}
}

GF_EXPORT
void gf_sg_set_max_defined_route_id(GF_SceneGraph *sg, u32 ID)
{
	sg->max_defined_route_id = MAX(sg->max_defined_route_id, ID);
}

GF_EXPORT
u32 gf_sg_get_next_available_proto_id(GF_SceneGraph *sg)
{
	u32 i, count;
	u32 ID = 0;
	count = gf_list_count(sg->protos);
	/*protos are not sorted*/
	for (i=0; i<count; i++) {
		GF_Proto *p = (GF_Proto *)gf_list_get(sg->protos, i);
		if (ID<=p->ID) ID = p->ID;
	}
	count = gf_list_count(sg->unregistered_protos);
	for (i=0; i<count; i++) {
		GF_Proto *p = (GF_Proto *)gf_list_get(sg->unregistered_protos, i);
		if (ID<=p->ID) ID = p->ID;
	}
	return ID+1;
}

//adds a child in the children list
GF_EXPORT
GF_Err gf_node_insert_child(GF_Node *parent, GF_Node *new_child, s32 Position)
{
	GF_ParentNode *node = (GF_ParentNode *) parent;
	if (Position == -1) {
		gf_node_list_add_child(& node->children, new_child);
	} else {
		gf_node_list_insert_child(& node->children, new_child, Position);
	}
	return GF_OK;
}

/*for V4Studio...*/
GF_EXPORT
GF_Err gf_node_remove_child(GF_Node *parent, GF_Node *toremove_child)
{
	if (!gf_node_list_del_child(& ((GF_ParentNode *) parent)->children, toremove_child)) return GF_BAD_PARAM;
	/*V4Studio doesn't handle DEF/USE properly yet...*/
	/*gf_node_unregister(toremove_child, parent);*/
	return GF_OK;
}

GF_EXPORT
void gf_sg_script_load(GF_Node *n)
{
	if (n && n->sgprivate->scenegraph->script_load) n->sgprivate->scenegraph->script_load(n);
}

GF_EXPORT
GF_Proto *gf_sg_find_proto(GF_SceneGraph *sg, u32 ProtoID, char *name)
{
	GF_Proto *proto;
	u32 i;

	assert(sg);

	/*browse all top-level */
	i=0;
	while ((proto = (GF_Proto *)gf_list_enum(sg->protos, &i))) {
		/*first check on name if given, since parsers use this with ID=0*/
		if (name) {
			if (proto->Name && !stricmp(name, proto->Name)) return proto;
		} else if (proto->ID == ProtoID) return proto;
	}
	/*browse all top-level unregistered in reverse order*/
	for (i=gf_list_count(sg->unregistered_protos); i>0; i--) {
		proto = (GF_Proto *)gf_list_get(sg->unregistered_protos, i-1);
		if (name) {
			if (proto->Name && !stricmp(name, proto->Name)) return proto;
		} else if (proto->ID == ProtoID) return proto;
	}
	return NULL;
}



static SFBool *NewSFBool()
{
	SFBool *tmp = (SFBool *)gf_malloc(sizeof(SFBool));
	memset(tmp, 0, sizeof(SFBool));
	return tmp;
}
static SFFloat *NewSFFloat()
{
	SFFloat *tmp = (SFFloat *)gf_malloc(sizeof(SFFloat));
	memset(tmp, 0, sizeof(SFFloat));
	return tmp;
}
static SFDouble *NewSFDouble()
{
	SFDouble *tmp = (SFDouble *)gf_malloc(sizeof(SFDouble));
	memset(tmp, 0, sizeof(SFDouble));
	return tmp;
}
static SFTime *NewSFTime()
{
	SFTime *tmp = (SFTime *)gf_malloc(sizeof(SFTime));
	memset(tmp, 0, sizeof(SFTime));
	return tmp;
}
static SFInt32 *NewSFInt32()
{
	SFInt32 *tmp = (SFInt32 *)gf_malloc(sizeof(SFInt32));
	memset(tmp, 0, sizeof(SFInt32));
	return tmp;
}
static SFString *NewSFString()
{
	SFString *tmp = (SFString *)gf_malloc(sizeof(SFString));
	memset(tmp, 0, sizeof(SFString));
	return tmp;
}
static SFVec3f *NewSFVec3f()
{
	SFVec3f *tmp = (SFVec3f *)gf_malloc(sizeof(SFVec3f));
	memset(tmp, 0, sizeof(SFVec3f));
	return tmp;
}
static SFVec3d *NewSFVec3d()
{
	SFVec3d *tmp = (SFVec3d *)gf_malloc(sizeof(SFVec3d));
	memset(tmp, 0, sizeof(SFVec3d));
	return tmp;
}
static SFVec2f *NewSFVec2f()
{
	SFVec2f *tmp = (SFVec2f *)gf_malloc(sizeof(SFVec2f));
	memset(tmp, 0, sizeof(SFVec2f));
	return tmp;
}
static SFVec2d *NewSFVec2d()
{
	SFVec2d *tmp = (SFVec2d *)gf_malloc(sizeof(SFVec2d));
	memset(tmp, 0, sizeof(SFVec2d));
	return tmp;
}
static SFColor *NewSFColor()
{
	SFColor *tmp = (SFColor *)gf_malloc(sizeof(SFColor));
	memset(tmp, 0, sizeof(SFColor));
	return tmp;
}
static SFColorRGBA *NewSFColorRGBA()
{
	SFColorRGBA *tmp = (SFColorRGBA *)gf_malloc(sizeof(SFColorRGBA));
	memset(tmp, 0, sizeof(SFColorRGBA));
	return tmp;
}
static SFRotation *NewSFRotation()
{
	SFRotation *tmp = (SFRotation *)gf_malloc(sizeof(SFRotation));
	memset(tmp, 0, sizeof(SFRotation));
	return tmp;
}
static SFImage *NewSFImage()
{
	SFImage *tmp = (SFImage *)gf_malloc(sizeof(SFImage));
	memset(tmp, 0, sizeof(SFImage));
	return tmp;
}
static SFURL *NewSFURL()
{
	SFURL *tmp = (SFURL *)gf_malloc(sizeof(SFURL));
	memset(tmp, 0, sizeof(SFURL));
	return tmp;
}
static SFCommandBuffer *NewSFCommandBuffer()
{
	SFCommandBuffer *tmp = (SFCommandBuffer *)gf_malloc(sizeof(SFCommandBuffer));
	memset(tmp, 0, sizeof(SFCommandBuffer));
	tmp->commandList = gf_list_new();
	return tmp;
}
static SFScript *NewSFScript()
{
	SFScript *tmp = (SFScript *)gf_malloc(sizeof(SFScript));
	memset(tmp, 0, sizeof(SFScript));
	return tmp;
}
static SFAttrRef *NewSFAttrRef()
{
	SFAttrRef *tmp;
	GF_SAFEALLOC(tmp, SFAttrRef);
	return tmp;
}
static MFBool *NewMFBool()
{
	MFBool *tmp = (MFBool *)gf_malloc(sizeof(MFBool));
	memset(tmp, 0, sizeof(MFBool));
	return tmp;
}
static MFFloat *NewMFFloat()
{
	MFFloat *tmp = (MFFloat *)gf_malloc(sizeof(MFFloat));
	memset(tmp, 0, sizeof(MFFloat));
	return tmp;
}
static MFTime *NewMFTime()
{
	MFTime *tmp = (MFTime *)gf_malloc(sizeof(MFTime));
	memset(tmp, 0, sizeof(MFTime));
	return tmp;
}
static MFInt32 *NewMFInt32()
{
	MFInt32 *tmp = (MFInt32 *)gf_malloc(sizeof(MFInt32));
	memset(tmp, 0, sizeof(MFInt32));
	return tmp;
}
static MFString *NewMFString()
{
	MFString *tmp = (MFString *)gf_malloc(sizeof(MFString));
	memset(tmp, 0, sizeof(MFString));
	return tmp;
}
static MFVec3f *NewMFVec3f()
{
	MFVec3f *tmp = (MFVec3f *)gf_malloc(sizeof(MFVec3f));
	memset(tmp, 0, sizeof(MFVec3f));
	return tmp;
}
static MFVec3d *NewMFVec3d()
{
	MFVec3d *tmp = (MFVec3d *)gf_malloc(sizeof(MFVec3d));
	memset(tmp, 0, sizeof(MFVec3d));
	return tmp;
}
static MFVec2f *NewMFVec2f()
{
	MFVec2f *tmp = (MFVec2f *)gf_malloc(sizeof(MFVec2f));
	memset(tmp, 0, sizeof(MFVec2f));
	return tmp;
}
static MFVec2d *NewMFVec2d()
{
	MFVec2d *tmp = (MFVec2d *)gf_malloc(sizeof(MFVec2d));
	memset(tmp, 0, sizeof(MFVec2d));
	return tmp;
}
static MFColor *NewMFColor()
{
	MFColor *tmp = (MFColor *)gf_malloc(sizeof(MFColor));
	memset(tmp, 0, sizeof(MFColor));
	return tmp;
}
static MFColorRGBA *NewMFColorRGBA()
{
	MFColorRGBA *tmp = (MFColorRGBA *)gf_malloc(sizeof(MFColorRGBA));
	memset(tmp, 0, sizeof(MFColorRGBA));
	return tmp;
}
static MFRotation *NewMFRotation()
{
	MFRotation *tmp = (MFRotation *)gf_malloc(sizeof(MFRotation));
	memset(tmp, 0, sizeof(MFRotation));
	return tmp;
}
static MFURL *NewMFURL()
{
	MFURL *tmp = (MFURL *)gf_malloc(sizeof(MFURL));
	memset(tmp, 0, sizeof(MFURL));
	return tmp;
}
static MFScript *NewMFScript()
{
	MFScript *tmp = (MFScript *)gf_malloc(sizeof(MFScript));
	memset(tmp, 0, sizeof(MFScript));
	return tmp;
}
static MFAttrRef *NewMFAttrRef()
{
	MFAttrRef *tmp;
	GF_SAFEALLOC(tmp, MFAttrRef);
	return tmp;
}

GF_EXPORT
void *gf_sg_vrml_field_pointer_new(u32 FieldType)
{
	switch (FieldType) {
	case GF_SG_VRML_SFBOOL:
		return NewSFBool();
	case GF_SG_VRML_SFFLOAT:
		return NewSFFloat();
	case GF_SG_VRML_SFDOUBLE:
		return NewSFDouble();
	case GF_SG_VRML_SFTIME:
		return NewSFTime();
	case GF_SG_VRML_SFINT32:
		return NewSFInt32();
	case GF_SG_VRML_SFSTRING:
		return NewSFString();
	case GF_SG_VRML_SFVEC3F:
		return NewSFVec3f();
	case GF_SG_VRML_SFVEC2F:
		return NewSFVec2f();
	case GF_SG_VRML_SFVEC3D:
		return NewSFVec3d();
	case GF_SG_VRML_SFVEC2D:
		return NewSFVec2d();
	case GF_SG_VRML_SFCOLOR:
		return NewSFColor();
	case GF_SG_VRML_SFCOLORRGBA:
		return NewSFColorRGBA();
	case GF_SG_VRML_SFROTATION:
		return NewSFRotation();
	case GF_SG_VRML_SFIMAGE:
		return NewSFImage();
	case GF_SG_VRML_SFATTRREF:
		return NewSFAttrRef();
	case GF_SG_VRML_MFBOOL:
		return NewMFBool();
	case GF_SG_VRML_MFFLOAT:
		return NewMFFloat();
	case GF_SG_VRML_MFTIME:
		return NewMFTime();
	case GF_SG_VRML_MFINT32:
		return NewMFInt32();
	case GF_SG_VRML_MFSTRING:
		return NewMFString();
	case GF_SG_VRML_MFVEC3F:
		return NewMFVec3f();
	case GF_SG_VRML_MFVEC2F:
		return NewMFVec2f();
	case GF_SG_VRML_MFVEC3D:
		return NewMFVec3d();
	case GF_SG_VRML_MFVEC2D:
		return NewMFVec2d();
	case GF_SG_VRML_MFCOLOR:
		return NewMFColor();
	case GF_SG_VRML_MFCOLORRGBA:
		return NewMFColorRGBA();
	case GF_SG_VRML_MFROTATION:
	case GF_SG_VRML_MFVEC4F:
		return NewMFRotation();
	case GF_SG_VRML_MFATTRREF:
		return NewMFAttrRef();

	//used in commands
	case GF_SG_VRML_SFCOMMANDBUFFER:
		return NewSFCommandBuffer();

	case GF_SG_VRML_SFURL:
		return NewSFURL();
	case GF_SG_VRML_MFURL:
		return NewMFURL();

	case GF_SG_VRML_SFSCRIPT:
		return NewSFScript();
	case GF_SG_VRML_MFSCRIPT:
		return NewMFScript();
	}
	return NULL;
}

void gf_sg_mfint32_del(MFInt32 par) {
	gf_free(par.vals);
}
void gf_sg_mffloat_del(MFFloat par) {
	gf_free(par.vals);
}
void gf_sg_mfdouble_del(MFDouble par) {
	gf_free(par.vals);
}
void gf_sg_mfbool_del(MFBool par) {
	gf_free(par.vals);
}
void gf_sg_mfcolor_del(MFColor par) {
	gf_free(par.vals);
}
void gf_sg_mfcolorrgba_del(MFColorRGBA par) {
	gf_free(par.vals);
}
void gf_sg_mfrotation_del(MFRotation par) {
	gf_free(par.vals);
}
void gf_sg_mftime_del(MFTime par) {
	gf_free(par.vals);
}
void gf_sg_mfvec2f_del(MFVec2f par) {
	gf_free(par.vals);
}
void gf_sg_mfvec2d_del(MFVec2d par) {
	gf_free(par.vals);
}
void gf_sg_mfvec3f_del(MFVec3f par) {
	gf_free(par.vals);
}
void gf_sg_mfvec3d_del(MFVec3d par) {
	gf_free(par.vals);
}
void gf_sg_mfvec4f_del(MFVec4f par) {
	gf_free(par.vals);
}
void gf_sg_mfattrref_del(MFAttrRef par) {
	gf_free(par.vals);
}
void gf_sg_sfimage_del(SFImage im) {
	gf_free(im.pixels);
}
void gf_sg_sfstring_del(SFString par) {
	if (par.buffer) gf_free(par.buffer);
}
void gf_sg_sfscript_del(SFScript par) {
	if (par.script_text) gf_free(par.script_text);
}


void gf_sg_sfcommand_del(SFCommandBuffer cb)
{
	u32 i;
	for (i=gf_list_count(cb.commandList); i>0; i--) {
		GF_Command *com = (GF_Command *)gf_list_get(cb.commandList, i-1);
		gf_sg_command_del(com);
	}
	gf_list_del(cb.commandList);
	if (cb.buffer) gf_free(cb.buffer);
}

GF_EXPORT
void gf_sg_vrml_field_pointer_del(void *field, u32 FieldType)
{
	GF_Node *node;

	switch (FieldType) {
	case GF_SG_VRML_SFBOOL:
	case GF_SG_VRML_SFFLOAT:
	case GF_SG_VRML_SFDOUBLE:
	case GF_SG_VRML_SFTIME:
	case GF_SG_VRML_SFINT32:
	case GF_SG_VRML_SFVEC3F:
	case GF_SG_VRML_SFVEC3D:
	case GF_SG_VRML_SFVEC2F:
	case GF_SG_VRML_SFVEC2D:
	case GF_SG_VRML_SFCOLOR:
	case GF_SG_VRML_SFCOLORRGBA:
	case GF_SG_VRML_SFROTATION:
	case GF_SG_VRML_SFATTRREF:
		break;
	case GF_SG_VRML_SFSTRING:
		if ( ((SFString *)field)->buffer) gf_free(((SFString *)field)->buffer);
		break;
	case GF_SG_VRML_SFIMAGE:
		gf_sg_sfimage_del(* ((SFImage *)field));
		break;

	case GF_SG_VRML_SFNODE:
		node = *(GF_Node **) field;
		if (node) gf_node_del(node);
		return;
	case GF_SG_VRML_SFCOMMANDBUFFER:
		gf_sg_sfcommand_del(*(SFCommandBuffer *)field);
		break;

	case GF_SG_VRML_MFBOOL:
		gf_sg_mfbool_del( * ((MFBool *) field));
		break;
	case GF_SG_VRML_MFFLOAT:
		gf_sg_mffloat_del( * ((MFFloat *) field));
		break;
	case GF_SG_VRML_MFDOUBLE:
		gf_sg_mfdouble_del( * ((MFDouble *) field));
		break;
	case GF_SG_VRML_MFTIME:
		gf_sg_mftime_del( * ((MFTime *)field));
		break;
	case GF_SG_VRML_MFINT32:
		gf_sg_mfint32_del( * ((MFInt32 *)field));
		break;
	case GF_SG_VRML_MFSTRING:
		gf_sg_mfstring_del( *((MFString *)field));
		break;
	case GF_SG_VRML_MFVEC3F:
		gf_sg_mfvec3f_del( * ((MFVec3f *)field));
		break;
	case GF_SG_VRML_MFVEC2F:
		gf_sg_mfvec2f_del( * ((MFVec2f *)field));
		break;
	case GF_SG_VRML_MFVEC3D:
		gf_sg_mfvec3d_del( * ((MFVec3d *)field));
		break;
	case GF_SG_VRML_MFVEC2D:
		gf_sg_mfvec2d_del( * ((MFVec2d *)field));
		break;
	case GF_SG_VRML_MFCOLOR:
		gf_sg_mfcolor_del( * ((MFColor *)field));
		break;
	case GF_SG_VRML_MFCOLORRGBA:
		gf_sg_mfcolorrgba_del( * ((MFColorRGBA *)field));
		break;
	case GF_SG_VRML_MFROTATION:
	case GF_SG_VRML_MFVEC4F:
		gf_sg_mfrotation_del( * ((MFRotation *)field));
		break;
	case GF_SG_VRML_SFURL:
		gf_sg_sfurl_del( * ((SFURL *) field));
		break;
	case GF_SG_VRML_MFURL:
		gf_sg_mfurl_del( * ((MFURL *) field));
		break;
	case GF_SG_VRML_MFATTRREF:
		gf_sg_mfattrref_del( * ((MFAttrRef *) field));
		break;
	//used only in proto since this field is created by default for regular nodes
	case GF_SG_VRML_MFNODE:
		assert(0);
		return;
	case GF_SG_VRML_MFSCRIPT:
		gf_sg_mfscript_del( * ((MFScript *) field));
		break;

	default:
		assert(0);
		return;
	}
	//free pointer
	gf_free(field);
}


/*********************************************************************
		MF Fields manipulation (alloc, gf_realloc, GetAt)
*********************************************************************/
GF_EXPORT
const char *gf_sg_vrml_get_event_type_name(u32 EventType, Bool forX3D)
{
	switch (EventType) {
	case GF_SG_EVENT_IN:
		return forX3D ? "inputOnly" : "eventIn";
	case GF_SG_EVENT_FIELD:
		return forX3D ? "initializeOnly" : "field";
	case GF_SG_EVENT_EXPOSED_FIELD:
		return forX3D ? "inputOutput" : "exposedField";
	case GF_SG_EVENT_OUT:
		return forX3D ? "outputOnly" : "eventOut";
	default:
		return "unknownEvent";
	}
}

GF_EXPORT
const char *gf_sg_vrml_get_field_type_by_name(u32 FieldType)
{

	switch (FieldType) {
	case GF_SG_VRML_SFBOOL:
		return "SFBool";
	case GF_SG_VRML_SFFLOAT:
		return "SFFloat";
	case GF_SG_VRML_SFDOUBLE:
		return "SFDouble";
	case GF_SG_VRML_SFTIME:
		return "SFTime";
	case GF_SG_VRML_SFINT32:
		return "SFInt32";
	case GF_SG_VRML_SFSTRING:
		return "SFString";
	case GF_SG_VRML_SFVEC3F:
		return "SFVec3f";
	case GF_SG_VRML_SFVEC2F:
		return "SFVec2f";
	case GF_SG_VRML_SFVEC3D:
		return "SFVec3d";
	case GF_SG_VRML_SFVEC2D:
		return "SFVec2d";
	case GF_SG_VRML_SFCOLOR:
		return "SFColor";
	case GF_SG_VRML_SFCOLORRGBA:
		return "SFColorRGBA";
	case GF_SG_VRML_SFROTATION:
		return "SFRotation";
	case GF_SG_VRML_SFIMAGE:
		return "SFImage";
	case GF_SG_VRML_SFNODE:
		return "SFNode";
	case GF_SG_VRML_SFVEC4F:
		return "SFVec4f";
	case GF_SG_VRML_SFATTRREF:
		return "SFAttrRef";
	case GF_SG_VRML_MFBOOL:
		return "MFBool";
	case GF_SG_VRML_MFFLOAT:
		return "MFFloat";
	case GF_SG_VRML_MFDOUBLE:
		return "MFDouble";
	case GF_SG_VRML_MFTIME:
		return "MFTime";
	case GF_SG_VRML_MFINT32:
		return "MFInt32";
	case GF_SG_VRML_MFSTRING:
		return "MFString";
	case GF_SG_VRML_MFVEC3F:
		return "MFVec3f";
	case GF_SG_VRML_MFVEC2F:
		return "MFVec2f";
	case GF_SG_VRML_MFVEC3D:
		return "MFVec3d";
	case GF_SG_VRML_MFVEC2D:
		return "MFVec2d";
	case GF_SG_VRML_MFCOLOR:
		return "MFColor";
	case GF_SG_VRML_MFCOLORRGBA:
		return "MFColorRGBA";
	case GF_SG_VRML_MFROTATION:
		return "MFRotation";
	case GF_SG_VRML_MFIMAGE:
		return "MFImage";
	case GF_SG_VRML_MFNODE:
		return "MFNode";
	case GF_SG_VRML_MFVEC4F:
		return "MFVec4f";
	case GF_SG_VRML_SFURL:
		return "SFURL";
	case GF_SG_VRML_MFURL:
		return "MFURL";
	case GF_SG_VRML_MFATTRREF:
		return "MFAttrRef";
	case GF_SG_VRML_SFCOMMANDBUFFER:
		return "SFCommandBuffer";
	case GF_SG_VRML_SFSCRIPT:
		return "SFScript";
	case GF_SG_VRML_MFSCRIPT:
		return "MFScript";
	default:
		return "UnknownType";
	}
}

u32 gf_sg_field_type_by_name(char *fieldType)
{
	if (!stricmp(fieldType, "SFBool")) return GF_SG_VRML_SFBOOL;
	else if (!stricmp(fieldType, "SFFloat")) return GF_SG_VRML_SFFLOAT;
	else if (!stricmp(fieldType, "SFDouble")) return GF_SG_VRML_SFDOUBLE;
	else if (!stricmp(fieldType, "SFTime")) return GF_SG_VRML_SFTIME;
	else if (!stricmp(fieldType, "SFInt32")) return GF_SG_VRML_SFINT32;
	else if (!stricmp(fieldType, "SFString")) return GF_SG_VRML_SFSTRING;
	else if (!stricmp(fieldType, "SFVec2f")) return GF_SG_VRML_SFVEC2F;
	else if (!stricmp(fieldType, "SFVec3f")) return GF_SG_VRML_SFVEC3F;
	else if (!stricmp(fieldType, "SFVec2d")) return GF_SG_VRML_SFVEC2D;
	else if (!stricmp(fieldType, "SFVec3d")) return GF_SG_VRML_SFVEC3D;
	else if (!stricmp(fieldType, "SFColor")) return GF_SG_VRML_SFCOLOR;
	else if (!stricmp(fieldType, "SFColorRGBA")) return GF_SG_VRML_SFCOLORRGBA;
	else if (!stricmp(fieldType, "SFRotation")) return GF_SG_VRML_SFROTATION;
	else if (!stricmp(fieldType, "SFImage")) return GF_SG_VRML_SFIMAGE;
	else if (!stricmp(fieldType, "SFAttrRef")) return GF_SG_VRML_SFATTRREF;
	else if (!stricmp(fieldType, "SFNode")) return GF_SG_VRML_SFNODE;

	else if (!stricmp(fieldType, "MFBool")) return GF_SG_VRML_MFBOOL;
	else if (!stricmp(fieldType, "MFFloat")) return GF_SG_VRML_MFFLOAT;
	else if (!stricmp(fieldType, "MFDouble")) return GF_SG_VRML_MFDOUBLE;
	else if (!stricmp(fieldType, "MFTime")) return GF_SG_VRML_MFTIME;
	else if (!stricmp(fieldType, "MFInt32")) return GF_SG_VRML_MFINT32;
	else if (!stricmp(fieldType, "MFString")) return GF_SG_VRML_MFSTRING;
	else if (!stricmp(fieldType, "MFVec2f")) return GF_SG_VRML_MFVEC2F;
	else if (!stricmp(fieldType, "MFVec3f")) return GF_SG_VRML_MFVEC3F;
	else if (!stricmp(fieldType, "MFVec2d")) return GF_SG_VRML_MFVEC2D;
	else if (!stricmp(fieldType, "MFVec3d")) return GF_SG_VRML_MFVEC3D;
	else if (!stricmp(fieldType, "MFColor")) return GF_SG_VRML_MFCOLOR;
	else if (!stricmp(fieldType, "MFColorRGBA")) return GF_SG_VRML_MFCOLORRGBA;
	else if (!stricmp(fieldType, "MFRotation")) return GF_SG_VRML_MFROTATION;
	else if (!stricmp(fieldType, "MFImage")) return GF_SG_VRML_MFIMAGE;
	else if (!stricmp(fieldType, "MFAttrRef")) return GF_SG_VRML_MFATTRREF;
	else if (!stricmp(fieldType, "MFNode")) return GF_SG_VRML_MFNODE;

	return GF_SG_VRML_UNKNOWN;
}

#endif

void gf_sg_sfurl_del(SFURL url) {
	if (url.url) gf_free(url.url);
}

GF_EXPORT
Bool gf_sg_vrml_is_sf_field(u32 FieldType)
{
	return (FieldType<GF_SG_VRML_FIRST_MF);
}

void gf_sg_mfstring_del(MFString par)
{
	u32 i;
	for (i=0; i<par.count; i++) {
		if (par.vals[i]) gf_free(par.vals[i]);
	}
	gf_free(par.vals);
}


GF_EXPORT
void gf_sg_mfurl_del(MFURL url)
{
	u32 i;
	for (i=0; i<url.count; i++) {
		gf_sg_sfurl_del(url.vals[i]);
	}
	gf_free(url.vals);
}
void gf_sg_mfscript_del(MFScript sc)
{
	u32 i;
	for (i=0; i<sc.count; i++) {
		if (sc.vals[i].script_text) gf_free(sc.vals[i].script_text);
	}
	gf_free(sc.vals);
}


void gf_sg_vrml_copy_mfurl(MFURL *dst, MFURL *src)
{
	u32 i;
	gf_sg_vrml_mf_reset(dst, GF_SG_VRML_MFURL);
	dst->count = src->count;
	dst->vals = gf_malloc(sizeof(SFURL)*src->count);
	for (i=0; i<src->count; i++) {
		dst->vals[i].OD_ID = src->vals[i].OD_ID;
		dst->vals[i].url = src->vals[i].url ? gf_strdup(src->vals[i].url) : NULL;
	}
}



//return the size of fixed fields (eg no buffer in the field)
u32 gf_sg_vrml_get_sf_size(u32 FieldType)
{
	switch (FieldType) {
	case GF_SG_VRML_SFBOOL:
	case GF_SG_VRML_MFBOOL:
		return sizeof(SFBool);
	case GF_SG_VRML_SFFLOAT:
	case GF_SG_VRML_MFFLOAT:
		return sizeof(SFFloat);
	case GF_SG_VRML_SFTIME:
	case GF_SG_VRML_MFTIME:
		return sizeof(SFTime);
	case GF_SG_VRML_SFDOUBLE:
	case GF_SG_VRML_MFDOUBLE:
		return sizeof(SFDouble);
	case GF_SG_VRML_SFINT32:
	case GF_SG_VRML_MFINT32:
		return sizeof(SFInt32);
	case GF_SG_VRML_SFVEC3F:
	case GF_SG_VRML_MFVEC3F:
		return 3*sizeof(SFFloat);
	case GF_SG_VRML_SFVEC2F:
	case GF_SG_VRML_MFVEC2F:
		return 2*sizeof(SFFloat);
	case GF_SG_VRML_SFVEC3D:
	case GF_SG_VRML_MFVEC3D:
		return 3*sizeof(SFDouble);
	case GF_SG_VRML_SFCOLOR:
	case GF_SG_VRML_MFCOLOR:
		return 3*sizeof(SFFloat);
	case GF_SG_VRML_SFCOLORRGBA:
	case GF_SG_VRML_MFCOLORRGBA:
		return 4*sizeof(SFFloat);
	case GF_SG_VRML_SFROTATION:
	case GF_SG_VRML_MFROTATION:
	case GF_SG_VRML_MFVEC4F:
		return 4*sizeof(SFFloat);

	case GF_SG_VRML_SFATTRREF:
	case GF_SG_VRML_MFATTRREF:
		return sizeof(SFAttrRef);
	//check if that works!!
	case GF_SG_VRML_SFSTRING:
	case GF_SG_VRML_MFSTRING:
		//ptr to char
		return sizeof(SFString);
	case GF_SG_VRML_SFSCRIPT:
	case GF_SG_VRML_MFSCRIPT:
		return sizeof(SFScript);
	case GF_SG_VRML_SFURL:
	case GF_SG_VRML_MFURL:
		return sizeof(SFURL);
	default:
		return 0;
	}
}

GF_EXPORT
u32 gf_sg_vrml_get_sf_type(u32 FieldType)
{
	switch (FieldType) {
	case GF_SG_VRML_SFBOOL:
	case GF_SG_VRML_MFBOOL:
		return GF_SG_VRML_SFBOOL;
	case GF_SG_VRML_SFFLOAT:
	case GF_SG_VRML_MFFLOAT:
		return GF_SG_VRML_SFFLOAT;
	case GF_SG_VRML_SFDOUBLE:
	case GF_SG_VRML_MFDOUBLE:
		return GF_SG_VRML_SFDOUBLE;
	case GF_SG_VRML_SFTIME:
	case GF_SG_VRML_MFTIME:
		return GF_SG_VRML_SFTIME;
	case GF_SG_VRML_SFINT32:
	case GF_SG_VRML_MFINT32:
		return GF_SG_VRML_SFINT32;
	case GF_SG_VRML_SFVEC3F:
	case GF_SG_VRML_MFVEC3F:
		return GF_SG_VRML_SFVEC3F;
	case GF_SG_VRML_SFVEC2F:
	case GF_SG_VRML_MFVEC2F:
		return GF_SG_VRML_SFVEC2F;
	case GF_SG_VRML_SFVEC3D:
	case GF_SG_VRML_MFVEC3D:
		return GF_SG_VRML_SFVEC3D;
	case GF_SG_VRML_SFVEC2D:
	case GF_SG_VRML_MFVEC2D:
		return GF_SG_VRML_SFVEC2D;
	case GF_SG_VRML_SFCOLOR:
	case GF_SG_VRML_MFCOLOR:
		return GF_SG_VRML_SFCOLOR;
	case GF_SG_VRML_SFCOLORRGBA:
	case GF_SG_VRML_MFCOLORRGBA:
		return GF_SG_VRML_SFCOLORRGBA;
	case GF_SG_VRML_SFROTATION:
	case GF_SG_VRML_MFROTATION:
		return GF_SG_VRML_SFROTATION;
	case GF_SG_VRML_SFATTRREF:
	case GF_SG_VRML_MFATTRREF:
		return GF_SG_VRML_SFATTRREF;

	//check if that works!!
	case GF_SG_VRML_SFSTRING:
	case GF_SG_VRML_MFSTRING:
		//ptr to char
		return GF_SG_VRML_SFSTRING;
	case GF_SG_VRML_SFSCRIPT:
	case GF_SG_VRML_MFSCRIPT:
		return GF_SG_VRML_SFSCRIPT;
	case GF_SG_VRML_SFURL:
	case GF_SG_VRML_MFURL:
		return GF_SG_VRML_SFURL;
	case GF_SG_VRML_SFNODE:
	case GF_SG_VRML_MFNODE:
		return GF_SG_VRML_SFNODE;
	default:
		return GF_SG_VRML_UNKNOWN;
	}
}

//
//	Insert (+alloc) an MFField with a specified position for insertion and sets the ptr to the
//	newly created slot
//	!! Doesnt work for MFNodes
//	InsertAt is the 0-based index for the new slot
GF_EXPORT
GF_Err gf_sg_vrml_mf_insert(void *mf, u32 FieldType, void **new_ptr, u32 InsertAt)
{
	char *buffer;
	u32 FieldSize, i, k;
	GenMFField *mffield = (GenMFField *)mf;

	if (gf_sg_vrml_is_sf_field(FieldType)) return GF_BAD_PARAM;
	if (FieldType == GF_SG_VRML_MFNODE) return GF_BAD_PARAM;

	FieldSize = gf_sg_vrml_get_sf_size(FieldType);

	//field we can't copy
	if (!FieldSize) return GF_BAD_PARAM;

	//first item ever
	if (!mffield->count || !mffield->array) {
		if (mffield->array) gf_free(mffield->array);
		mffield->array = (char*)gf_malloc(sizeof(char)*FieldSize);
		memset(mffield->array, 0, sizeof(char)*FieldSize);
		mffield->count = 1;
		if (new_ptr) *new_ptr = mffield->array;
		return GF_OK;
	}

	//append at the end
	if (InsertAt >= mffield->count) {
		mffield->array = (char*)gf_realloc(mffield->array, sizeof(char)*(1+mffield->count)*FieldSize);
		memset(mffield->array + mffield->count * FieldSize, 0, FieldSize);
		if (new_ptr) *new_ptr = mffield->array + mffield->count * FieldSize;
		mffield->count += 1;
		return GF_OK;
	}
	//alloc 1+itemCount
	buffer = (char*)gf_malloc(sizeof(char)*(1+mffield->count)*FieldSize);

	//insert in the array
	k=0;
	for (i=0; i < mffield->count; i++) {
		if (InsertAt == i) {
			if (new_ptr) {
				*new_ptr = buffer + i*FieldSize;
				memset(*new_ptr, 0, sizeof(char)*FieldSize);
			}
			k = 1;
		}
		memcpy(buffer + (k+i) * FieldSize , mffield->array + i*FieldSize, FieldSize);
	}
	gf_free(mffield->array);
	mffield->array = buffer;
	mffield->count += 1;
	return GF_OK;
}


GF_EXPORT
GF_Err gf_sg_vrml_mf_reset(void *mf, u32 FieldType)
{
	GenMFField *mffield = (GenMFField *)mf;
	if (!mffield->array) return GF_OK;

	//field we can't copy
	if (gf_sg_vrml_is_sf_field(FieldType)) return GF_BAD_PARAM;
	if (!gf_sg_vrml_get_sf_size(FieldType)) return GF_BAD_PARAM;

	switch (FieldType) {
	case GF_SG_VRML_MFSTRING:
		gf_sg_mfstring_del( * ((MFString *) mf));
		break;
	case GF_SG_VRML_MFURL:
		gf_sg_mfurl_del( * ((MFURL *) mf));
		break;
	case GF_SG_VRML_MFSCRIPT:
		gf_sg_mfscript_del( * ((MFScript *) mf));
		break;
	default:
		if (mffield->array) gf_free(mffield->array);
		break;
	}

	mffield->array = NULL;
	mffield->count = 0;
	return GF_OK;
}

#ifndef GPAC_DISABLE_VRML

#define MAX_MFFIELD_ALLOC		5000000
GF_EXPORT
GF_Err gf_sg_vrml_mf_alloc(void *mf, u32 FieldType, u32 NbItems)
{
	u32 FieldSize;
	GenMFField *mffield = (GenMFField *)mf;

	if (gf_sg_vrml_is_sf_field(FieldType)) return GF_BAD_PARAM;
	if (FieldType == GF_SG_VRML_MFNODE) return GF_BAD_PARAM;

	FieldSize = gf_sg_vrml_get_sf_size(FieldType);

	//field we can't copy
	if (!FieldSize) return GF_BAD_PARAM;
	if (NbItems>MAX_MFFIELD_ALLOC) return GF_IO_ERR;

	if (mffield->count==NbItems) return GF_OK;
	gf_sg_vrml_mf_reset(mf, FieldType);
	if (NbItems) {
		mffield->array = (char*)gf_malloc(sizeof(char)*FieldSize*NbItems);
		memset(mffield->array, 0, sizeof(char)*FieldSize*NbItems);
	}
	mffield->count = NbItems;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_sg_vrml_mf_get_item(void *mf, u32 FieldType, void **new_ptr, u32 ItemPos)
{
	u32 FieldSize;
	GenMFField *mffield = (GenMFField *)mf;

	*new_ptr = NULL;
	if (gf_sg_vrml_is_sf_field(FieldType)) return GF_BAD_PARAM;
	if (FieldType == GF_SG_VRML_MFNODE) return GF_BAD_PARAM;

	FieldSize = gf_sg_vrml_get_sf_size(FieldType);

	//field we can't copy
	if (!FieldSize) return GF_BAD_PARAM;
	if (ItemPos >= mffield->count) return GF_BAD_PARAM;
	*new_ptr = mffield->array + ItemPos * FieldSize;
	return GF_OK;
}


GF_EXPORT
GF_Err gf_sg_vrml_mf_append(void *mf, u32 FieldType, void **new_ptr)
{
	GenMFField *mffield = (GenMFField *)mf;
	return gf_sg_vrml_mf_insert(mf, FieldType, new_ptr, mffield->count+2);
}


//remove the specified item (0-based index)
GF_EXPORT
GF_Err gf_sg_vrml_mf_remove(void *mf, u32 FieldType, u32 RemoveFrom)
{
	char *buffer;
	u32 FieldSize, i, k;
	GenMFField *mffield = (GenMFField *)mf;

	FieldSize = gf_sg_vrml_get_sf_size(FieldType);

	//field we can't copy
	if (!FieldSize) return GF_BAD_PARAM;

	if (!mffield->count || RemoveFrom >= mffield->count) return GF_BAD_PARAM;

	if (mffield->count == 1) {
		gf_free(mffield->array);
		mffield->array = NULL;
		mffield->count = 0;
		return GF_OK;
	}
	k=0;
	buffer = (char*)gf_malloc(sizeof(char)*(mffield->count-1)*FieldSize);
	for (i=0; i<mffield->count; i++) {
		if (RemoveFrom == i) {
			k = 1;
		} else {
			memcpy(buffer + (i-k)*FieldSize, mffield->array + i*FieldSize, FieldSize);
		}
	}
	gf_free(mffield->array);
	mffield->array = buffer;
	mffield->count -= 1;
	return GF_OK;
}

/*special cloning with type-casting from SF/MF strings to URL conversion since proto URL doesn't exist
as a field type (it's just a stupid encoding trick) */
void VRML_FieldCopyCast(void *dest, u32 dst_field_type, void *orig, u32 ori_field_type)
{
	SFURL *url;
	char tmp[50];
	u32 size, i, sf_type_ori, sf_type_dst;
	void *dst_field, *orig_field;
	if (!dest || !orig) return;

	switch (dst_field_type) {
	case GF_SG_VRML_SFSTRING:
		if (ori_field_type == GF_SG_VRML_SFURL) {
			url = ((SFURL *)orig);
			if (url->OD_ID>0) {
				sprintf(tmp, "%d", url->OD_ID);
				if ( ((SFString*)dest)->buffer) gf_free(((SFString*)dest)->buffer);
				((SFString*)dest)->buffer = gf_strdup(tmp);
			} else {
				if ( ((SFString*)dest)->buffer) gf_free(((SFString*)dest)->buffer);
				((SFString*)dest)->buffer = url->url ? gf_strdup(url->url) : NULL;
			}
		}
		/*for SFString to MFString cast*/
		else if (ori_field_type == GF_SG_VRML_SFSTRING) {
			if ( ((SFString*)dest)->buffer) gf_free(((SFString*)dest)->buffer);
			((SFString*)dest)->buffer = ((SFString*)orig)->buffer ? gf_strdup(((SFString*)orig)->buffer) : NULL;
		}
		return;
	case GF_SG_VRML_SFURL:
		if (ori_field_type != GF_SG_VRML_SFSTRING) return;
		url = ((SFURL *)dest);
		url->OD_ID = 0;
		if (url->url) gf_free(url->url);
		if ( ((SFString*)orig)->buffer)
			url->url = gf_strdup(((SFString*)orig)->buffer);
		else
			url->url = NULL;
		return;
	case GF_SG_VRML_MFSTRING:
	case GF_SG_VRML_MFURL:
		break;
	default:
		return;
	}

	sf_type_dst = gf_sg_vrml_get_sf_type(dst_field_type);

	if (gf_sg_vrml_is_sf_field(ori_field_type)) {
		size = 1;
		gf_sg_vrml_mf_alloc(dest, dst_field_type, size);
		gf_sg_vrml_mf_get_item(dest, dst_field_type, &dst_field, 0);
		VRML_FieldCopyCast(dst_field, sf_type_dst, orig, ori_field_type);
		return;
	}

	size = ((GenMFField *)orig)->count;
	if (size != ((GenMFField *)dest)->count) gf_sg_vrml_mf_alloc(dest, dst_field_type, size);

	sf_type_ori = gf_sg_vrml_get_sf_type(ori_field_type);
	//duplicate all items
	for (i=0; i<size; i++) {
		gf_sg_vrml_mf_get_item(dest, dst_field_type, &dst_field, i);
		gf_sg_vrml_mf_get_item(orig, ori_field_type, &orig_field, i);
		VRML_FieldCopyCast(dst_field, sf_type_dst, orig_field, sf_type_ori);
	}
	return;
}

GF_EXPORT
void gf_sg_vrml_field_clone(void *dest, void *orig, u32 field_type, GF_SceneGraph *inScene)
{
	u32 size, i, sf_type;
	void *dst_field, *orig_field;

	if (!dest || !orig) return;

	switch (field_type) {
	case GF_SG_VRML_SFBOOL:
		memcpy(dest, orig, sizeof(SFBool));
		break;
	case GF_SG_VRML_SFCOLOR:
		memcpy(dest, orig, sizeof(SFColor));
		break;
	case GF_SG_VRML_SFFLOAT:
		memcpy(dest, orig, sizeof(SFFloat));
		break;
	case GF_SG_VRML_SFINT32:
		memcpy(dest, orig, sizeof(SFInt32));
		break;
	case GF_SG_VRML_SFROTATION:
		memcpy(dest, orig, sizeof(SFRotation));
		break;
	case GF_SG_VRML_SFTIME:
		memcpy(dest, orig, sizeof(SFTime));
		break;
	case GF_SG_VRML_SFVEC2F:
		memcpy(dest, orig, sizeof(SFVec2f));
		break;
	case GF_SG_VRML_SFVEC3F:
		memcpy(dest, orig, sizeof(SFVec3f));
		break;
	case GF_SG_VRML_SFATTRREF:
		memcpy(dest, orig, sizeof(SFAttrRef));
		break;
	case GF_SG_VRML_SFSTRING:
		if ( ((SFString*)dest)->buffer) gf_free(((SFString*)dest)->buffer);
		if ( ((SFString*)orig)->buffer )
			((SFString*)dest)->buffer = gf_strdup(((SFString*)orig)->buffer);
		else
			((SFString*)dest)->buffer = NULL;
		break;
	case GF_SG_VRML_SFURL:
		if ( ((SFURL *)dest)->url ) gf_free( ((SFURL *)dest)->url );
		((SFURL *)dest)->OD_ID = ((SFURL *)orig)->OD_ID;
		if (((SFURL *)orig)->url)
			((SFURL *)dest)->url = gf_strdup(((SFURL *)orig)->url);
		else
			((SFURL *)dest)->url = NULL;
		break;
	case GF_SG_VRML_SFIMAGE:
		if (((SFImage *)dest)->pixels) gf_free(((SFImage *)dest)->pixels);
		((SFImage *)dest)->width = ((SFImage *)orig)->width;
		((SFImage *)dest)->height = ((SFImage *)orig)->height;
		((SFImage *)dest)->numComponents  = ((SFImage *)orig)->numComponents;
		size = ((SFImage *)dest)->width * ((SFImage *)dest)->height * ((SFImage *)dest)->numComponents;
		((SFImage *)dest)->pixels = (u8*)gf_malloc(sizeof(char)*size);
		memcpy(((SFImage *)dest)->pixels, ((SFImage *)orig)->pixels, sizeof(char)*size);
		break;
	case GF_SG_VRML_SFCOMMANDBUFFER:
	{
		SFCommandBuffer *cb_dst = (SFCommandBuffer *)dest;
		SFCommandBuffer *cb_src = (SFCommandBuffer *)orig;

		cb_dst->bufferSize = cb_src->bufferSize;
		if (cb_dst->bufferSize && !gf_list_count(cb_src->commandList) ) {
			cb_dst->buffer = (u8*)gf_realloc(cb_dst->buffer, sizeof(char)*cb_dst->bufferSize);
			memcpy(cb_dst->buffer, cb_src->buffer, sizeof(char)*cb_src->bufferSize);
		} else {
			u32 j, c2;
			if (cb_dst->buffer) gf_free(cb_dst->buffer);
			cb_dst->buffer = NULL;
			/*clone command list*/
			c2 = gf_list_count(cb_src->commandList);
			for (j=0; j<c2; j++) {
				GF_Command *sub_com = (GF_Command *)gf_list_get(cb_src->commandList, j);
				GF_Command *new_com = gf_sg_vrml_command_clone(sub_com, inScene, 0);
				gf_list_add(cb_dst->commandList, new_com);
			}
		}
	}
	break;

	/*simply copy text string*/
	case GF_SG_VRML_SFSCRIPT:
		if (((SFScript*)dest)->script_text) gf_free(((SFScript*)dest)->script_text);
		((SFScript*)dest)->script_text = NULL;
		if ( ((SFScript*)orig)->script_text)
			((SFScript *)dest)->script_text = (char *)gf_strdup( (char*) ((SFScript*)orig)->script_text );
		break;


	//simple MFFields, do a memcpy
	case GF_SG_VRML_MFBOOL:
	case GF_SG_VRML_MFFLOAT:
	case GF_SG_VRML_MFTIME:
	case GF_SG_VRML_MFINT32:
	case GF_SG_VRML_MFVEC3F:
	case GF_SG_VRML_MFVEC2F:
	case GF_SG_VRML_MFCOLOR:
	case GF_SG_VRML_MFROTATION:
	case GF_SG_VRML_MFATTRREF:
		size = gf_sg_vrml_get_sf_size(field_type) * ((GenMFField *)orig)->count;
		if (((GenMFField *)orig)->count != ((GenMFField *)dest)->count) {
			((GenMFField *)dest)->array = gf_realloc(((GenMFField *)dest)->array, size);
			((GenMFField *)dest)->count = ((GenMFField *)orig)->count;
		}
		memcpy(((GenMFField *)dest)->array, ((GenMFField *)orig)->array, size);
		break;
	//complex MFFields
	case GF_SG_VRML_MFSTRING:
	case GF_SG_VRML_MFIMAGE:
	case GF_SG_VRML_MFURL:
	case GF_SG_VRML_MFSCRIPT:
		size = ((GenMFField *)orig)->count;
		gf_sg_vrml_mf_reset(dest, field_type);
		gf_sg_vrml_mf_alloc(dest, field_type, size);
		sf_type = gf_sg_vrml_get_sf_type(field_type);
		//duplicate all items
		for (i=0; i<size; i++) {
			gf_sg_vrml_mf_get_item(dest, field_type, &dst_field, i);
			gf_sg_vrml_mf_get_item(orig, field_type, &orig_field, i);
			gf_sg_vrml_field_copy(dst_field, orig_field, sf_type);
		}
		break;
	}
}

GF_EXPORT
void gf_sg_vrml_field_copy(void *dest, void *orig, u32 field_type)
{
	gf_sg_vrml_field_clone(dest, orig, field_type, NULL);
}

GF_EXPORT
Bool gf_sg_vrml_field_equal(void *dest, void *orig, u32 field_type)
{
	u32 size, i, sf_type;
	void *dst_field, *orig_field;
	Bool changed = 0;

	if (!dest || !orig) return 0;

	switch (field_type) {
	case GF_SG_VRML_SFBOOL:
		changed = memcmp(dest, orig, sizeof(SFBool));
		break;
	case GF_SG_VRML_SFCOLOR:
		if (((SFColor *)dest)->red != ((SFColor *)orig)->red) changed = 1;
		else if (((SFColor *)dest)->green != ((SFColor *)orig)->green) changed = 1;
		else if (((SFColor *)dest)->blue != ((SFColor *)orig)->blue) changed = 1;
		break;
	case GF_SG_VRML_SFFLOAT:
		if ( (*(SFFloat *)dest) != (*(SFFloat *)orig) ) changed = 1;
		break;
	case GF_SG_VRML_SFINT32:
		changed = memcmp(dest, orig, sizeof(SFInt32));
		break;
	case GF_SG_VRML_SFROTATION:
		if (((SFRotation *)dest)->x != ((SFRotation *)orig)->x) changed = 1;
		else if (((SFRotation *)dest)->y != ((SFRotation *)orig)->y) changed = 1;
		else if (((SFRotation *)dest)->z != ((SFRotation *)orig)->z) changed = 1;
		else if (((SFRotation *)dest)->q != ((SFRotation *)orig)->q) changed = 1;
		break;
	case GF_SG_VRML_SFTIME:
		if ( (*(SFTime *)dest) != (*(SFTime*)orig) ) changed = 1;
		break;
	case GF_SG_VRML_SFVEC2F:
		if (((SFVec2f *)dest)->x != ((SFVec2f *)orig)->x) changed = 1;
		else if (((SFVec2f *)dest)->y != ((SFVec2f *)orig)->y) changed = 1;
		break;
	case GF_SG_VRML_SFVEC3F:
		if (((SFVec3f *)dest)->x != ((SFVec3f *)orig)->x) changed = 1;
		else if (((SFVec3f *)dest)->y != ((SFVec3f *)orig)->y) changed = 1;
		else if (((SFVec3f *)dest)->z != ((SFVec3f *)orig)->z) changed = 1;
		break;
	case GF_SG_VRML_SFSTRING:
		if ( ((SFString*)dest)->buffer && ((SFString*)orig)->buffer) {
			changed = strcmp(((SFString*)dest)->buffer, ((SFString*)orig)->buffer);
		} else {
			changed = ( !((SFString*)dest)->buffer && !((SFString*)orig)->buffer) ? 0 : 1;
		}
		break;
	case GF_SG_VRML_SFURL:
		if (((SFURL *)dest)->OD_ID > 0 || ((SFURL *)orig)->OD_ID > 0) {
			if ( ((SFURL *)orig)->OD_ID != ((SFURL *)dest)->OD_ID) changed = 1;
		} else {
			if ( ((SFURL *)orig)->url && ! ((SFURL *)dest)->url) changed = 1;
			else if ( ! ((SFURL *)orig)->url && ((SFURL *)dest)->url) changed = 1;
			else if ( strcmp( ((SFURL *)orig)->url , ((SFURL *)dest)->url) ) changed = 1;
		}
		break;
	case GF_SG_VRML_SFIMAGE:
	case GF_SG_VRML_SFATTRREF:
	case GF_SG_VRML_SFSCRIPT:
	case GF_SG_VRML_SFCOMMANDBUFFER:
		changed = 1;
		break;

	//MFFields
	case GF_SG_VRML_MFATTRREF:
		changed = 1;
		break;
	case GF_SG_VRML_MFBOOL:
	case GF_SG_VRML_MFFLOAT:
	case GF_SG_VRML_MFTIME:
	case GF_SG_VRML_MFINT32:
	case GF_SG_VRML_MFSTRING:
	case GF_SG_VRML_MFVEC3F:
	case GF_SG_VRML_MFVEC2F:
	case GF_SG_VRML_MFCOLOR:
	case GF_SG_VRML_MFROTATION:
	case GF_SG_VRML_MFIMAGE:
	case GF_SG_VRML_MFURL:
	case GF_SG_VRML_MFSCRIPT:
		if ( ((GenMFField *)orig)->count != ((GenMFField *)dest)->count) changed = 1;
		else {
			size = ((GenMFField *)orig)->count;
			sf_type = gf_sg_vrml_get_sf_type(field_type);
			for (i=0; i<size; i++) {
				gf_sg_vrml_mf_get_item(dest, field_type, &dst_field, i);
				gf_sg_vrml_mf_get_item(orig, field_type, &orig_field, i);
				if (! gf_sg_vrml_field_equal(dst_field, orig_field, sf_type) ) {
					changed = 1;
					break;
				}
			}
		}
		break;
	}
	return changed ? 0 : 1;
}



GF_EXPORT
SFColorRGBA gf_sg_sfcolor_to_rgba(SFColor val)
{
	SFColorRGBA res;
	res.alpha = FIX_ONE;
	res.red = val.red;
	res.green = val.green;
	res.blue = val.blue;
	return res;
}


GF_EXPORT
u32 gf_node_get_num_fields_in_mode(GF_Node *Node, u8 IndexMode)
{
	assert(Node);
	if (Node->sgprivate->tag == TAG_ProtoNode) return gf_sg_proto_get_num_fields(Node, IndexMode);
	else if (Node->sgprivate->tag == TAG_MPEG4_Script)
		return gf_sg_script_get_num_fields(Node, IndexMode);
#ifndef GPAC_DISABLE_X3D
	else if (Node->sgprivate->tag == TAG_X3D_Script)
		return gf_sg_script_get_num_fields(Node, IndexMode);
#endif
	else if (Node->sgprivate->tag <= GF_NODE_RANGE_LAST_MPEG4) return gf_sg_mpeg4_node_get_field_count(Node, IndexMode);
#ifndef GPAC_DISABLE_X3D
	else if (Node->sgprivate->tag <= GF_NODE_RANGE_LAST_X3D) return gf_sg_x3d_node_get_field_count(Node);
#endif
	else return 0;
}



/*all our internally handled nodes*/
Bool InitColorInterpolator(M_ColorInterpolator *node);
Bool InitCoordinateInterpolator2D(M_CoordinateInterpolator2D *node);
Bool InitCoordinateInterpolator(M_CoordinateInterpolator *n);
Bool InitNormalInterpolator(M_NormalInterpolator *n);
Bool InitPositionInterpolator2D(M_PositionInterpolator2D *node);
Bool InitPositionInterpolator(M_PositionInterpolator *node);
Bool InitScalarInterpolator(M_ScalarInterpolator *node);
Bool InitOrientationInterpolator(M_OrientationInterpolator *node);
Bool InitValuator(M_Valuator *node);
Bool InitCoordinateInterpolator4D(M_CoordinateInterpolator4D *node);
Bool InitPositionInterpolator4D(M_PositionInterpolator4D *node);

void PA_Init(GF_Node *n);
void PA_Modified(GF_Node *n, GF_FieldInfo *field);
void PA2D_Init(GF_Node *n);
void PA2D_Modified(GF_Node *n, GF_FieldInfo *field);
void SA_Init(GF_Node *n);
void SA_Modified(GF_Node *n, GF_FieldInfo *field);
/*X3D tools*/
void InitBooleanFilter(GF_Node *n);
void InitBooleanSequencer(GF_Node *n);
void InitBooleanToggle(GF_Node *n);
void InitBooleanTrigger(GF_Node *n);
void InitIntegerSequencer(GF_Node *n);
void InitIntegerTrigger(GF_Node *n);
void InitTimeTrigger(GF_Node *n);

Bool gf_sg_vrml_node_init(GF_Node *node)
{
	switch (node->sgprivate->tag) {
	case TAG_MPEG4_ColorInterpolator:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_ColorInterpolator:
#endif
		return InitColorInterpolator((M_ColorInterpolator *)node);
	case TAG_MPEG4_CoordinateInterpolator:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_CoordinateInterpolator:
#endif
		return InitCoordinateInterpolator((M_CoordinateInterpolator *)node);
	case TAG_MPEG4_CoordinateInterpolator2D:
		return InitCoordinateInterpolator2D((M_CoordinateInterpolator2D *)node);
	case TAG_MPEG4_NormalInterpolator:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_NormalInterpolator:
#endif
		return InitNormalInterpolator((M_NormalInterpolator*)node);
	case TAG_MPEG4_OrientationInterpolator:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_OrientationInterpolator:
#endif
		return InitOrientationInterpolator((M_OrientationInterpolator*)node);
	case TAG_MPEG4_PositionInterpolator:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_PositionInterpolator:
#endif
		return InitPositionInterpolator((M_PositionInterpolator *)node);
	case TAG_MPEG4_PositionInterpolator2D:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_PositionInterpolator2D:
#endif
		return InitPositionInterpolator2D((M_PositionInterpolator2D *)node);
	case TAG_MPEG4_ScalarInterpolator:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_ScalarInterpolator:
#endif
		return InitScalarInterpolator((M_ScalarInterpolator *)node);
	case TAG_MPEG4_Valuator:
		return InitValuator((M_Valuator *)node);
	case TAG_MPEG4_PositionAnimator:
		PA_Init(node);
		return 1;
	case TAG_MPEG4_PositionAnimator2D:
		PA2D_Init(node);
		return 1;
	case TAG_MPEG4_ScalarAnimator:
		SA_Init(node);
		return 1;
	case TAG_MPEG4_PositionInterpolator4D:
		return InitPositionInterpolator4D((M_PositionInterpolator4D *)node);
	case TAG_MPEG4_CoordinateInterpolator4D:
		return InitCoordinateInterpolator4D((M_CoordinateInterpolator4D *)node);
	case TAG_MPEG4_Script:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Script:
#endif
		return 1;

#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_BooleanFilter:
		InitBooleanFilter(node);
		return 1;
	case TAG_X3D_BooleanSequencer:
		InitBooleanSequencer(node);
		return 1;
	case TAG_X3D_BooleanToggle:
		InitBooleanToggle(node);
		return 1;
	case TAG_X3D_BooleanTrigger:
		InitBooleanTrigger(node);
		return 1;
	case TAG_X3D_IntegerSequencer:
		InitIntegerSequencer(node);
		return 1;
	case TAG_X3D_IntegerTrigger:
		InitIntegerTrigger(node);
		return 1;
	case TAG_X3D_TimeTrigger:
		InitTimeTrigger(node);
		return 1;
#endif
	}
	return 0;
}

Bool gf_sg_vrml_node_changed(GF_Node *node, GF_FieldInfo *field)
{
	switch (node->sgprivate->tag) {
	case TAG_ProtoNode:
		/*hardcoded protos need modification notifs*/
		if (node->sgprivate->UserCallback) return 0;
	case TAG_MPEG4_ColorInterpolator:
	case TAG_MPEG4_CoordinateInterpolator:
	case TAG_MPEG4_CoordinateInterpolator2D:
	case TAG_MPEG4_NormalInterpolator:
	case TAG_MPEG4_OrientationInterpolator:
	case TAG_MPEG4_PositionInterpolator:
	case TAG_MPEG4_PositionInterpolator2D:
	case TAG_MPEG4_ScalarInterpolator:
	case TAG_MPEG4_Valuator:
	case TAG_MPEG4_PositionInterpolator4D:
	case TAG_MPEG4_CoordinateInterpolator4D:
	case TAG_MPEG4_Script:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_ColorInterpolator:
	case TAG_X3D_CoordinateInterpolator:
	case TAG_X3D_NormalInterpolator:
	case TAG_X3D_OrientationInterpolator:
	case TAG_X3D_PositionInterpolator:
	case TAG_X3D_ScalarInterpolator:
	case TAG_X3D_Script:
	case TAG_X3D_BooleanFilter:
	case TAG_X3D_BooleanSequencer:
	case TAG_X3D_BooleanToggle:
	case TAG_X3D_BooleanTrigger:
	case TAG_X3D_IntegerSequencer:
	case TAG_X3D_IntegerTrigger:
	case TAG_X3D_TimeTrigger:
#endif
		return 1;
	case TAG_MPEG4_PositionAnimator:
		PA_Modified(node, field);
		return 1;
	case TAG_MPEG4_PositionAnimator2D:
		PA2D_Modified(node, field);
		return 1;
	case TAG_MPEG4_ScalarAnimator:
		SA_Modified(node, field);
		return 1;
	}
	return 0;
}


char *gf_node_vrml_dump_attribute(GF_Node *n, GF_FieldInfo *info)
{
	char szVal[1024];

	switch (info->fieldType) {
	case GF_SG_VRML_SFBOOL:
		strcpy(szVal, *((SFBool*)info->far_ptr) ? "TRUE" : "FALSE");
		return gf_strdup(szVal);
	case GF_SG_VRML_SFINT32:
		sprintf(szVal, "%d", *((SFInt32*)info->far_ptr) );
		return gf_strdup(szVal);
	case GF_SG_VRML_SFFLOAT:
		sprintf(szVal, "%g", FIX2FLT( *((SFFloat*)info->far_ptr) ) );
		return gf_strdup(szVal);
	case GF_SG_VRML_SFDOUBLE:
		sprintf(szVal, "%g", *((SFDouble *)info->far_ptr) );
		return gf_strdup(szVal);
	case GF_SG_VRML_SFTIME:
		sprintf(szVal, "%g", *((SFTime *)info->far_ptr) );
		return gf_strdup(szVal);
	case GF_SG_VRML_SFVEC2F:
		sprintf(szVal, "%g %g", FIX2FLT(((SFVec2f *)info->far_ptr)->x), FIX2FLT( ((SFVec2f *)info->far_ptr)->y) );
		return gf_strdup(szVal);
	case GF_SG_VRML_SFVEC2D:
		sprintf(szVal, "%g %g", ((SFVec2d *)info->far_ptr)->x, ((SFVec2d *)info->far_ptr)->y);
		return gf_strdup(szVal);
	case GF_SG_VRML_SFVEC3F:
		sprintf(szVal, "%g %g %g", FIX2FLT(((SFVec3f *)info->far_ptr)->x), FIX2FLT( ((SFVec3f *)info->far_ptr)->y) , FIX2FLT( ((SFVec3f *)info->far_ptr)->z) );
		return gf_strdup(szVal);
	case GF_SG_VRML_SFVEC3D:
		sprintf(szVal, "%g %g %g", ((SFVec3d *)info->far_ptr)->x, ((SFVec3d *)info->far_ptr)->y, ((SFVec3d *)info->far_ptr)->z);
		return gf_strdup(szVal);
	case GF_SG_VRML_SFCOLOR:
		sprintf(szVal, "%g %g %g", FIX2FLT(((SFColor *)info->far_ptr)->red), FIX2FLT( ((SFColor *)info->far_ptr)->green) , FIX2FLT( ((SFColor *)info->far_ptr)->blue) );
		return gf_strdup(szVal);
	case GF_SG_VRML_SFCOLORRGBA:
		sprintf(szVal, "%g %g %g %g", FIX2FLT(((SFColorRGBA *)info->far_ptr)->red), FIX2FLT( ((SFColorRGBA*)info->far_ptr)->green) , FIX2FLT( ((SFColorRGBA*)info->far_ptr)->blue) , FIX2FLT( ((SFColorRGBA*)info->far_ptr)->alpha) );
		return gf_strdup(szVal);
	case GF_SG_VRML_SFROTATION:
		sprintf(szVal, "%g %g %g %g", FIX2FLT(((SFRotation *)info->far_ptr)->x), FIX2FLT( ((SFRotation *)info->far_ptr)->y) , FIX2FLT( ((SFRotation *)info->far_ptr)->z), FIX2FLT( ((SFRotation *)info->far_ptr)->q) );
		return gf_strdup(szVal);
	case GF_SG_VRML_SFSTRING:
		if (!((SFString*)info->far_ptr)->buffer ) return gf_strdup("");
		return gf_strdup( ((SFString*)info->far_ptr)->buffer );

	case GF_SG_VRML_SFURL:
		if (((SFURL *)info->far_ptr)->url) {
			return gf_strdup( ((SFURL *)info->far_ptr)->url );
		} else {
			sprintf(szVal, "od://%d", ((SFURL *)info->far_ptr)->OD_ID);
			return gf_strdup(szVal);
		}

	case GF_SG_VRML_SFIMAGE:
	{
		u32 i, count;
		char *buf;
		SFImage *img = (SFImage *)info->far_ptr;

		count = img->width * img->height * img->numComponents;
		i = (3/*' 0x'*/ + 2/*%02X*/*img->numComponents)*count + 20;
		buf = gf_malloc(sizeof(char) * i);

		sprintf(buf , "%d %d %d", img->width, img->height, img->numComponents);

		for (i=0; i<count; ) {
			switch (img->numComponents) {
			case 1:
				sprintf(szVal, " 0x%02X", img->pixels[i]);
				i++;
				break;
			case 2:
				sprintf(szVal, " 0x%02X%02X", img->pixels[i], img->pixels[i+1]);
				i+=2;
				break;
			case 3:
				sprintf(szVal, " 0x%02X%02X%02X", img->pixels[i], img->pixels[i+1], img->pixels[i+2]);
				i+=3;
				break;
			case 4:
				sprintf(szVal, " 0x%02X%02X%02X%02X", img->pixels[i], img->pixels[i+1], img->pixels[i+2], img->pixels[i+3]);
				i+=4;
				break;
			}
			strcat(buf, szVal);
		}
		return buf;
	}
	default:
		break;
	}
	/*todo - dump MFFields*/
	return NULL;
}

#endif /*GPAC_DISABLE_VRML*/

Bool gf_node_in_table(GF_Node *node, u32 NDTType)
{
#ifndef GPAC_DISABLE_VRML
	u32 tag = node ? node->sgprivate->tag : 0;
	if (tag==TAG_ProtoNode) {
		tag = gf_sg_proto_get_root_tag(((GF_ProtoInstance *)node)->proto_interface);
		if (tag==TAG_UndefinedNode) return 1;
	}
	return gf_node_in_table_by_tag(tag, NDTType);
#else
	return 1;
#endif
}
