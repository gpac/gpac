/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Management sub-project
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

#include <gpac/scene_manager.h>
#include <gpac/constants.h>
#include <gpac/internal/scenegraph_dev.h>

#ifndef GPAC_DISABLE_SCENE_STATS

struct _statman
{
	GF_SceneStatistics *stats;
	GF_List *def_nodes;
};

static GF_SceneStatistics *NewSceneStats()
{
	GF_SceneStatistics *tmp;
	GF_SAFEALLOC(tmp, GF_SceneStatistics);
	tmp->node_stats = gf_list_new();
	tmp->proto_stats = gf_list_new();

	tmp->max_2d.x = FIX_MIN;
	tmp->max_2d.y = FIX_MIN;
	tmp->max_3d.x = FIX_MIN;
	tmp->max_3d.y = FIX_MIN;
	tmp->max_3d.z = FIX_MIN;
	tmp->min_2d.x = FIX_MAX;
	tmp->min_2d.y = FIX_MAX;
	tmp->min_3d.x = FIX_MAX;
	tmp->min_3d.y = FIX_MAX;
	tmp->min_3d.z = FIX_MAX;
	return tmp;
}

static void ResetStatisitics(GF_SceneStatistics *stat)
{
	while (gf_list_count(stat->node_stats)) {
		GF_NodeStats *ptr = (GF_NodeStats *)gf_list_get(stat->node_stats, 0);
		gf_list_rem(stat->node_stats, 0);
		gf_free(ptr);
	}
	while (gf_list_count(stat->proto_stats)) {
		GF_NodeStats *ptr = (GF_NodeStats *)gf_list_get(stat->proto_stats, 0);
		gf_list_rem(stat->proto_stats, 0);
		gf_free(ptr);
	}
	stat->max_2d.x = FIX_MIN;
	stat->max_2d.y = FIX_MIN;
	stat->max_3d.x = FIX_MIN;
	stat->max_3d.y = FIX_MIN;
	stat->max_3d.z = FIX_MIN;
	stat->min_2d.x = FIX_MAX;
	stat->min_2d.y = FIX_MAX;
	stat->min_3d.x = FIX_MAX;
	stat->min_3d.y = FIX_MAX;
	stat->min_3d.z = FIX_MAX;
	stat->count_2d = stat->rem_2d = stat->count_3d = stat->rem_3d = stat->count_float = 0;
	stat->rem_float = stat->count_color = stat->rem_color = stat->count_2f = stat->count_3f = 0;
}

static void DeleteStatisitics(GF_SceneStatistics *stat)
{
	ResetStatisitics(stat);
	gf_list_del(stat->node_stats);
	gf_list_del(stat->proto_stats);
	gf_free(stat);
}

static void StatNode(GF_SceneStatistics *stat, GF_Node *n, Bool isUsed, Bool isDelete, GF_Node *prev)
{
	u32 i;
	GF_NodeStats *ptr = NULL;
	if (!stat) return;

	if (n->sgprivate->tag == TAG_ProtoNode) {
#ifndef GPAC_DISABLE_VRML
		GF_ProtoInstance *pr = (GF_ProtoInstance *)n;
		i=0;
		while ((ptr = (GF_NodeStats *)gf_list_enum(stat->proto_stats, &i))) {
			if (pr->proto_interface->ID == ptr->tag) break;
			ptr = NULL;
		}
		if (!ptr) {
			GF_SAFEALLOC(ptr, GF_NodeStats);
			ptr->tag = pr->proto_interface->ID;
			ptr->name = gf_sg_proto_get_class_name(pr->proto_interface);
			gf_list_add(stat->proto_stats, ptr);
		}
#endif
	} else {
		i=0;
		while ((ptr = (GF_NodeStats *)gf_list_enum(stat->node_stats, &i))) {
			if (n->sgprivate->tag == ptr->tag) break;
			ptr = NULL;
		}
		if (!ptr) {
			GF_SAFEALLOC(ptr, GF_NodeStats);
			ptr->tag = n->sgprivate->tag;
			ptr->name = gf_node_get_class_name(n);
			gf_list_add(stat->node_stats, ptr);
		}
	}
	if (isDelete) ptr->nb_del += n->sgprivate->num_instances;
	else if (isUsed) ptr->nb_used += 1;
	/*this is because the node passes twice in the stat, once on DumpNode and once in replaceALL*/
	else ptr->nb_created += prev ? (prev->sgprivate->num_instances - 1) : 1;
}

static void StatFixed(GF_SceneStatistics *stat, Fixed v, Bool scale)
{
	u32 int_res, frac_res;
	u32 fixv  = FIX2INT((v>0?v:-v) * (1<<16));
	s32 intv  = (fixv & 0xFFFF0000)>>16;
	u32 fracv = fixv & 0x0000FFFF;

	int_res = 0;
	while ( (intv >> int_res) ) int_res++;
	int_res++; /* signedness */

	if (fracv) {
		frac_res = 1;
		while ((fracv << frac_res) & 0x0000FFFF) 
			frac_res++;
	} else {
		frac_res = 0;
	}

	if (scale) {
		if (int_res > stat->scale_int_res_2d) stat->scale_int_res_2d = int_res;
		if (frac_res > stat->scale_frac_res_2d) stat->scale_frac_res_2d = frac_res;
	} else {
		if (int_res > stat->int_res_2d) stat->int_res_2d = int_res;
		if (frac_res > stat->frac_res_2d) stat->frac_res_2d = frac_res;
	}
	if (stat->max_fixed < v) stat->max_fixed = v;
	if (stat->min_fixed > v) stat->min_fixed = v;
}



static void StatSVGPoint(GF_SceneStatistics *stat, SFVec2f *val)
{
	if (!stat) return;
	if (stat->max_2d.x < val->x) stat->max_2d.x = val->x;
	if (stat->max_2d.y < val->y) stat->max_2d.y = val->y;
	if (stat->min_2d.x > val->x) stat->min_2d.x = val->x;
	if (stat->min_2d.y > val->y) stat->min_2d.y = val->y;
	StatFixed(stat, val->x, 0);
	StatFixed(stat, val->y, 0);
}	

static void StatSFVec2f(GF_SceneStatistics *stat, SFVec2f *val)
{
	if (!stat) return;
	if (stat->max_2d.x < val->x) stat->max_2d.x = val->x;
	if (stat->max_2d.y < val->y) stat->max_2d.y = val->y;
	if (stat->min_2d.x > val->x) stat->min_2d.x = val->x;
	if (stat->min_2d.y > val->y) stat->min_2d.y = val->y;
}	

static void StatSFVec3f(GF_SceneStatistics *stat, SFVec3f *val)
{
	if (!stat) return;
	if (stat->max_3d.x < val->x) stat->max_3d.x = val->x;
	if (stat->max_3d.y < val->y) stat->max_3d.y = val->y;
	if (stat->max_3d.z < val->z) stat->max_3d.z = val->y;
	if (stat->min_3d.x > val->x) stat->min_3d.x = val->x;
	if (stat->min_3d.y > val->y) stat->min_3d.y = val->y;
	if (stat->min_3d.z > val->z) stat->min_3d.z = val->z;
}

static void StatField(GF_SceneStatistics *stat, GF_FieldInfo *field)
{
	u32 i;

	switch (field->fieldType) {
	case GF_SG_VRML_SFFLOAT:
		stat->count_float++;
		if (stat->max_fixed < *(SFFloat*)field->far_ptr)
			stat->max_fixed = *(SFFloat*)field->far_ptr;
		if (stat->min_fixed > *(SFFloat*)field->far_ptr)
			stat->min_fixed = *(SFFloat*)field->far_ptr;
		break;
	case GF_SG_VRML_SFCOLOR:
		stat->count_color++;
		break;
	case GF_SG_VRML_SFVEC2F:
		stat->count_2f++;
		StatSFVec2f(stat, field->far_ptr);
		break;
	case GF_SG_VRML_SFVEC3F:
		stat->count_3f++;
		StatSFVec3f(stat, field->far_ptr);
		break;

	case GF_SG_VRML_MFFLOAT:
		stat->count_float+= ((MFFloat *)field->far_ptr)->count;
		break;
	case GF_SG_VRML_MFCOLOR:
		stat->count_color+= ((MFColor *)field->far_ptr)->count;
		break;
	case GF_SG_VRML_MFVEC2F:
	{
		MFVec2f *mf2d = (MFVec2f *)field->far_ptr;
		for (i=0; i<mf2d->count; i++) {
			StatSFVec2f(stat, &mf2d->vals[i]);
			stat->count_2d ++;
		}
	}
		break;
	case GF_SG_VRML_MFVEC3F:
	{
		MFVec3f *mf3d = (MFVec3f *)field->far_ptr;
		for (i=0; i<mf3d->count; i++) {
			StatSFVec3f(stat, &mf3d->vals[i]);
			stat->count_3d ++;
		}
	}
		break;
	}
}


static void StatSingleField(GF_SceneStatistics *stat, GF_FieldInfo *field)
{
	switch (field->fieldType) {
	case GF_SG_VRML_SFVEC2F:
		StatSFVec2f(stat, (SFVec2f *)field->far_ptr);
		break;
	case GF_SG_VRML_MFVEC3F:
		StatSFVec3f(stat, (SFVec3f *)field->far_ptr);
		break;
	}
}

static void StatSVGAttribute(GF_SceneStatistics *stat, GF_FieldInfo *field)
{
	u32 i = 0;

	stat->nb_svg_attributes++;

	switch (field->fieldType) {
	case SVG_PathData_datatype:
		{
#if USE_GF_PATH
			SVG_PathData *d = (SVG_PathData *)field->far_ptr;
			for (i=0; i<d->n_points; i++) {
				StatSVGPoint(stat, &(d->points[i]));
				stat->count_2d ++;
			}		
#else
			SVG_PathData *d = (SVG_PathData *)field->far_ptr;
			for (i=0; i<gf_list_count(d->points); i++) {
				SVG_Point *p = (SVG_Point *)gf_list_get(d->points, i);
				StatSVGPoint(stat, (SFVec2f *)p);
				stat->count_2d ++;
			}
#endif
		}
		break;
	case SVG_ViewBox_datatype:
		{
			SVG_ViewBox *vB = (SVG_ViewBox *)field->far_ptr;
			StatFixed(stat, vB->x, 0);
			StatFixed(stat, vB->y, 0);
			StatFixed(stat, vB->width, 0);
			StatFixed(stat, vB->height, 0);
		}
		break;
	case SVG_Points_datatype:
	case SVG_Coordinates_datatype:
		{
			GF_List *points = *((GF_List **)field->far_ptr);
			for (i=0; i<gf_list_count(points); i++) {
				SVG_Point *p = (SVG_Point *)gf_list_get(points, i);
				StatSVGPoint(stat, (SFVec2f *)p);
				stat->count_2d ++;
			}
		}
		break;
	case SVG_Transform_datatype:
		{
			GF_Matrix2D *mx = &((SVG_Transform *)field->far_ptr)->mat;
			if (!gf_mx2d_is_identity(*mx) && !(!mx->m[0] && !mx->m[1] && !mx->m[3] && !mx->m[4])) {
				StatFixed(stat, mx->m[0], 1);
				StatFixed(stat, mx->m[1], 1);
				StatFixed(stat, mx->m[3], 1);
				StatFixed(stat, mx->m[4], 1);				
				StatFixed(stat, mx->m[2], 0);
				StatFixed(stat, mx->m[5], 0);
			} 
		}
		break;
	case SVG_Motion_datatype:
		{
			GF_Matrix2D *mx = (GF_Matrix2D *)field->far_ptr;
			if (!gf_mx2d_is_identity(*mx) && !(!mx->m[0] && !mx->m[1] && !mx->m[3] && !mx->m[4])) {
				StatFixed(stat, mx->m[0], 1);
				StatFixed(stat, mx->m[1], 1);
				StatFixed(stat, mx->m[3], 1);
				StatFixed(stat, mx->m[4], 1);				
				StatFixed(stat, mx->m[2], 0);
				StatFixed(stat, mx->m[5], 0);
			} 
		}
		break;
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype: 
		StatFixed(stat, ((SVG_Number *)field->far_ptr)->value, 0);
		break;
	}
}

static void StatRemField(GF_SceneStatistics *stat, u32 fieldType, GF_FieldInfo *field)
{
	u32 count = 1;
	if (field) count = ((GenMFField*)field->far_ptr)->count;
	switch (fieldType) {
	case GF_SG_VRML_MFFLOAT:
		stat->rem_float += count;
		break;
	case GF_SG_VRML_SFCOLOR:
		stat->rem_color += count;
		break;
	case GF_SG_VRML_MFVEC2F:
		stat->rem_2d += count;
		break;
	case GF_SG_VRML_MFVEC3F:
		stat->rem_3d += count;
		break;
	}
}


Bool StatIsUSE(GF_StatManager *st, GF_Node *n) 
{
	u32 i;
	GF_Node *ptr;
	if (!n || !gf_node_get_id(n) ) return 0;
	i=0;
	while ((ptr = (GF_Node*)gf_list_enum(st->def_nodes, &i))) {
		if (ptr == n) return 1;
	}
	gf_list_add(st->def_nodes, n);
	return 0;
}

static GF_Err StatNodeGraph(GF_StatManager *st, GF_Node *n)
{
	GF_Node *clone;
	GF_FieldInfo field;

	if (!n) return GF_OK;
	StatNode(st->stats, n, StatIsUSE(st, n), 0, NULL);

	if (n->sgprivate->tag != TAG_ProtoNode) {
		clone = gf_node_new(n->sgprivate->scenegraph, n->sgprivate->tag);
	} else {
#ifndef GPAC_DISABLE_VRML
		clone = gf_sg_proto_create_node(n->sgprivate->scenegraph, ((GF_ProtoInstance *)n)->proto_interface, NULL);
#else
		clone = NULL;
#endif
	}
	if (!clone) return GF_OK;
	gf_node_register(clone, NULL);

#ifndef GPAC_DISABLE_SVG
	if ((n->sgprivate->tag>= GF_NODE_RANGE_FIRST_SVG) && (n->sgprivate->tag<= GF_NODE_RANGE_LAST_SVG)) {
		GF_ChildNodeItem *list = ((SVG_Element *)n)->children;
		GF_DOMAttribute *atts = ((GF_DOMNode*)n)->attributes;
		while (atts) {
			field.far_ptr = atts->data;
			field.fieldType = atts->data_type;
			field.fieldIndex = atts->tag;
			field.name = NULL; 
			StatSVGAttribute(st->stats, &field);

			atts = atts->next;
		}
		while (list) {
			StatNodeGraph(st, list->node);
			list = list->next;
		}
	} else 
#endif
	if (n->sgprivate->tag == TAG_DOMText) {
	} else if (n->sgprivate->tag == TAG_DOMFullNode) {
	} 
#ifndef GPAC_DISABLE_VRML
	else if (n->sgprivate->tag<= GF_NODE_RANGE_LAST_X3D) {
		GF_Node *child;
		GF_ChildNodeItem *list;
		u32 i, count;
		GF_FieldInfo clone_field;

		count = gf_node_get_field_count(n);
	
		for (i=0; i<count; i++) {
			gf_node_get_field(n, i, &field);
			if (field.eventType==GF_SG_EVENT_IN) continue;
			if (field.eventType==GF_SG_EVENT_OUT) continue;

			switch (field.fieldType) {
			case GF_SG_VRML_SFNODE:
				child = *((GF_Node **)field.far_ptr);
				StatNodeGraph(st, child);
				break;
			case GF_SG_VRML_MFNODE:
				list = *((GF_ChildNodeItem **)field.far_ptr);
				while (list) {
					StatNodeGraph(st, list->node);
					list = list->next;
				}
				break;
			default:
				gf_node_get_field(clone, i, &clone_field);
				if (!gf_sg_vrml_field_equal(clone_field.far_ptr, field.far_ptr, field.fieldType)) {
					StatField(st->stats, &field);
				}
				break;
			}
		}
	}
#endif

	gf_node_unregister(clone, NULL);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_sm_stats_for_command(GF_StatManager *stat, GF_Command *com)
{
#ifdef GPAC_DISABLE_VRML
	return GF_NOT_SUPPORTED;
#else
	GF_FieldInfo field;
	GF_Err e;
	GF_ChildNodeItem *list;
	GF_CommandField *inf = NULL;
	if (gf_list_count(com->command_fields)) 
		inf = (GF_CommandField*)gf_list_get(com->command_fields, 0);

	if (!com || !stat) return GF_BAD_PARAM;
	switch (com->tag) {
	case GF_SG_SCENE_REPLACE:
		if (com->node) StatNodeGraph(stat, com->node);
		break;
	case GF_SG_NODE_REPLACE:
		if (inf && inf->new_node) StatNodeGraph(stat, inf->new_node);
		break;
	case GF_SG_FIELD_REPLACE:
		if (!inf) return GF_OK;
		e = gf_node_get_field(com->node, inf->fieldIndex, &field);
		if (e) return e;

		switch (field.fieldType) {
		case GF_SG_VRML_SFNODE:
			if (inf->new_node) StatNodeGraph(stat, inf->new_node);
			break;
		case GF_SG_VRML_MFNODE:
			list = * ((GF_ChildNodeItem**) inf->field_ptr);
			while (list) {
				StatNodeGraph(stat, list->node);
				list = list->next;
			}
			break;
		default:
			field.far_ptr = inf->field_ptr;
			StatField(stat->stats, &field);
			break;
		}
		break;
	case GF_SG_INDEXED_REPLACE:
		if (!inf) return GF_OK;
		e = gf_node_get_field(com->node, inf->fieldIndex, &field);
		if (e) return e;

		if (field.fieldType == GF_SG_VRML_MFNODE) {
			StatNodeGraph(stat, inf->new_node);
		} else {
			field.fieldType = gf_sg_vrml_get_sf_type(field.fieldType);
			field.far_ptr = inf->field_ptr;
			StatSingleField(stat->stats, &field);
		}
		break;
	case GF_SG_NODE_DELETE:
		if (com->node) StatNode(stat->stats, com->node, 0, 1, NULL);
		break;
	case GF_SG_INDEXED_DELETE:
		if (!inf) return GF_OK;
		e = gf_node_get_field(com->node, inf->fieldIndex, &field);
		if (e) return e;

		/*then we need special handling in case of a node*/
		if (gf_sg_vrml_get_sf_type(field.fieldType) == GF_SG_VRML_SFNODE) {
			GF_Node *n = gf_node_list_get_child( * (GF_ChildNodeItem **) field.far_ptr, inf->pos);
			if (n) StatNode(stat->stats, n, 0, 1, NULL);
		} else {
			StatRemField(stat->stats, inf->fieldType, NULL);
		}
		break;
	case GF_SG_NODE_INSERT:
		if (inf && inf->new_node) StatNodeGraph(stat, inf->new_node);
		break;
	case GF_SG_INDEXED_INSERT:
		if (!inf) return GF_OK;
		e = gf_node_get_field(com->node, inf->fieldIndex, &field);
		if (e) return e;

		/*rescale the MFField and parse the SFField*/
		if (field.fieldType != GF_SG_VRML_MFNODE) {
			field.fieldType = gf_sg_vrml_get_sf_type(field.fieldType);
			field.far_ptr = inf->field_ptr;
			StatSingleField(stat->stats, &field);
		} else {
			if (inf->new_node) StatNodeGraph(stat, inf->new_node);
		}
		break;
	case GF_SG_ROUTE_REPLACE:
	case GF_SG_ROUTE_DELETE:
	case GF_SG_ROUTE_INSERT:
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
	return GF_OK;
#endif
}

static GF_Err gf_sm_stat_au(GF_List *commandList, GF_StatManager *st)
{
	u32 i, count;
	count = gf_list_count(commandList);
	for (i=0; i<count; i++) {
		GF_Command *com = (GF_Command *)gf_list_get(commandList, i);
		gf_sm_stats_for_command(st, com);
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_sm_stats_for_scene(GF_StatManager *stat, GF_SceneManager *sm)
{
	u32 i, j;
	GF_StreamContext *sc;
	GF_Err e;

	if (gf_list_count(sm->streams)) {
		i=0;
		while ((sc = (GF_StreamContext*)gf_list_enum(sm->streams, &i))) {
			GF_AUContext *au;
			if (sc->streamType != GF_STREAM_SCENE) continue;
			
			if (!stat->stats->base_layer)
				stat->stats->base_layer = sc;

			j=0;
			while ((au = (GF_AUContext*)gf_list_enum(sc->AUs, &j))) {
				e = gf_sm_stat_au(au->commands, stat);
				if (e) return e;
			}
		}
	} else { /* No scene stream: e.g. SVG */
		if (sm->scene_graph) gf_sm_stats_for_graph(stat, sm->scene_graph);
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_sm_stats_for_graph(GF_StatManager *stat, GF_SceneGraph *sg)
{
	if (!stat || !sg) return GF_BAD_PARAM;
	return StatNodeGraph(stat, sg->RootNode);
}

/*creates new stat handler*/
GF_EXPORT
GF_StatManager *gf_sm_stats_new()
{
	GF_StatManager *sm = (GF_StatManager *)gf_malloc(sizeof(GF_StatManager));
	sm->def_nodes = gf_list_new();
	sm->stats = NewSceneStats();
	return sm;

}
/*deletes stat object returned by one of the above functions*/
GF_EXPORT
void gf_sm_stats_del(GF_StatManager *stat)
{
	gf_list_del(stat->def_nodes);
	DeleteStatisitics(stat->stats);
	gf_free(stat);
}

GF_EXPORT
GF_SceneStatistics *gf_sm_stats_get(GF_StatManager *stat)
{
	return stat->stats;
}

GF_EXPORT
void gf_sm_stats_reset(GF_StatManager *stat)
{
	if (!stat) return;
	ResetStatisitics(stat->stats);
}

#endif /*GPAC_DISABLE_SCENE_STATS*/

