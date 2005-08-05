/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / 3D rendering module
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



#include "render3d_nodes.h"
/*for anchor*/
#include "grouping.h"
/*for anchor processing, which needs to be filtered at the inline scene level*/
#include <gpac/internal/terminal_dev.h>

typedef struct
{
	GF_Node *owner;
	GF_Renderer *compositor;
	GROUPINGNODESTACK
	Bool enabled, active;
	SensorHandler hdl;
} AnchorStack;

static void DestroyAnchor(GF_Node *n)
{
	AnchorStack *st = (AnchorStack*)gf_node_get_private(n);
	R3D_SensorDeleted(st->compositor, &st->hdl);
	DeleteGroupingNode((GroupingNode *)st);
	free(st);
}

static void RenderAnchor(GF_Node *node, void *rs)
{
	AnchorStack *st = (AnchorStack *) gf_node_get_private(node);
	RenderEffect3D *eff = rs;

	if (!st->compositor->user->EventProc) {
		st->enabled = 0;
		return;
	}

	/*note we don't clear dirty flag, this is done in traversing*/
	if (gf_node_dirty_get(node) & GF_SG_NODE_DIRTY) {
		MFURL *url;
		if (gf_node_get_tag(node)==TAG_MPEG4_Anchor) {
			url = & ((M_Anchor *)node)->url;
		} else {
			url = & ((X_Anchor *)node)->url;
		}
		st->enabled = 0;
		if (url->count && url->vals[0].url && strlen(url->vals[0].url) )
			st->enabled = 1;
	}

	grouping_traverse((GroupingNode*)st, eff, NULL);
}

static void OnAnchor(SensorHandler *sh, Bool is_over, u32 eventType, RayHitInfo *hit_info)
{
	MFURL *url;
	AnchorStack *st = (AnchorStack *) gf_node_get_private(sh->owner);

	if (eventType==GF_EVT_LEFTDOWN) st->active = 1;
	else if (st->active && (eventType==GF_EVT_LEFTUP) ) {
		u32 i;
		GF_Event evt;
		if (gf_node_get_tag(sh->owner)==TAG_MPEG4_Anchor) {
			url = & ((M_Anchor *)sh->owner)->url;
			evt.navigate.param_count = ((M_Anchor *)sh->owner)->parameter.count;
			evt.navigate.parameters = (const char **) ((M_Anchor *)sh->owner)->parameter.vals;
		} else {
			url = & ((X_Anchor *)sh->owner)->url;
			evt.navigate.param_count = ((X_Anchor *)sh->owner)->parameter.count;
			evt.navigate.parameters = (const char **) ((X_Anchor *)sh->owner)->parameter.vals;
		}
		evt.type = GF_EVT_NAVIGATE;
		i=0;
		while (i<url->count) {
			evt.navigate.to_url = url->vals[i].url;
			if (!evt.navigate.to_url) break;
			/*current scene navigation*/
			if (evt.navigate.to_url[0] == '#') {
				GF_Node *bindable;
				evt.navigate.to_url++;
				bindable = gf_sg_find_node_by_name(gf_node_get_graph(sh->owner), (char *) evt.navigate.to_url);
				if (bindable) {
					Bindable_SetSetBind(bindable, 1);
					break;
				}
			} else if (st->compositor->term) {
				if (gf_is_process_anchor(sh->owner, &evt))
					break;
			} else if (st->compositor->user->EventProc) {
				if (st->compositor->user->EventProc(st->compositor->user->opaque, &evt))
					break;
			}
			i++;
		}
	}
}

static Bool anchor_is_enabled(GF_Node *node)
{
	AnchorStack *st = (AnchorStack *) gf_node_get_private(node);
	return st->enabled;
}

static void on_activate_anchor(GF_Node *node)
{
	UserEvent3D ev;
	SFVec3f pt;
	AnchorStack *st = (AnchorStack *) gf_node_get_private(node);
	if (!((M_Anchor *)node)->on_activate) return;

	ev.event_type = GF_EVT_LEFTUP;
	pt.x = pt.y = pt.z = 0;
	OnAnchor(&st->hdl, 0, 1, NULL);
}

SensorHandler *r3d_anchor_get_handler(GF_Node *n)
{
	AnchorStack *st = (AnchorStack *) gf_node_get_private(n);
	return &st->hdl;
}


void R3D_InitAnchor(Render3D *sr, GF_Node *node)
{
	GF_List *children;
	AnchorStack *stack = malloc(sizeof(AnchorStack));
	memset(stack, 0, sizeof(AnchorStack));
	stack->hdl.IsEnabled = anchor_is_enabled;
	stack->hdl.OnUserEvent = OnAnchor;
	stack->hdl.owner = node;
	if (gf_node_get_tag(node)==TAG_MPEG4_Anchor) {
		((M_Anchor *)node)->on_activate = on_activate_anchor;
		children = ((M_Anchor *)node)->children;
	} else {
		children = ((X_Anchor *)node)->children;
	}
	SetupGroupingNode((GroupingNode*)stack, sr->compositor, node, children);
	sr->compositor->interaction_sensors++;
	gf_node_set_private(node, stack);
	gf_node_set_predestroy_function(node, DestroyAnchor);
	gf_node_set_render_function(node, RenderAnchor);
}


typedef struct 
{
	SensorHandler hdl;
	GF_Renderer *compositor;
	Fixed start_angle;
	GF_Matrix initial_matrix;
} DiscSensorStack;

static void DestroyDiscSensor(GF_Node *node)
{
	DiscSensorStack *st = (DiscSensorStack *) gf_node_get_private(node);
	R3D_SensorDeleted(st->compositor, &st->hdl);
	free(st);
}

static Bool ds_is_enabled(GF_Node *n)
{
	M_DiscSensor *ds = (M_DiscSensor *)n;
	return (ds->enabled || ds->isActive);
}


static void OnDiscSensor(SensorHandler *sh, Bool is_over, u32 eventType, RayHitInfo *hit_info)
{
	M_DiscSensor *ds = (M_DiscSensor *)sh->owner;
	DiscSensorStack *stack = (DiscSensorStack *) gf_node_get_private(sh->owner);
	
	if (ds->isActive && (!ds->enabled || (eventType==GF_EVT_LEFTUP)) ) {
		if (ds->autoOffset) {
			ds->offset = ds->rotation_changed;
			/*that's an exposedField*/
			gf_node_event_out_str(sh->owner, "offset");
		}
		ds->isActive = 0;
		gf_node_event_out_str(sh->owner, "isActive");
		R3D_SetGrabbed(stack->compositor, 0);
	}
	else if ((eventType==GF_EVT_LEFTDOWN) && !ds->isActive) {
		/*store inverse matrix*/
		gf_mx_copy(stack->initial_matrix, hit_info->local_to_world);
		stack->start_angle = gf_atan2(hit_info->local_point.y, hit_info->local_point.x);
		ds->isActive = 1;
		gf_node_event_out_str(sh->owner, "isActive");
		R3D_SetGrabbed(stack->compositor, 1);
	}
	else if (ds->isActive) {
		GF_Ray loc_ray;
		Fixed rot;
		SFVec3f res;
		loc_ray = hit_info->world_ray;
		gf_mx_apply_ray(&stack->initial_matrix, &loc_ray);
		R3D_Get2DPlaneIntersection(&loc_ray, &res);
	
		rot = gf_atan2(res.y, res.x) - stack->start_angle + ds->offset;
		if (ds->minAngle < ds->maxAngle) {
			/*FIXME this doesn't work properly*/
			if (rot < ds->minAngle) rot = ds->minAngle;
			if (rot > ds->maxAngle) rot = ds->maxAngle;
		}
		ds->rotation_changed = rot;
		gf_node_event_out_str(sh->owner, "rotation_changed");
	   	ds->trackPoint_changed.x = res.x;
	   	ds->trackPoint_changed.y = res.y;
		gf_node_event_out_str(sh->owner, "trackPoint_changed");
	}
}

SensorHandler *r3d_ds_get_handler(GF_Node *n)
{
	DiscSensorStack *st = (DiscSensorStack *)gf_node_get_private(n);
	return &st->hdl;
}

void R3D_InitDiscSensor(Render3D *sr, GF_Node *node)
{
	DiscSensorStack *st;
	st = malloc(sizeof(DiscSensorStack));
	memset(st, 0, sizeof(DiscSensorStack));
	st->hdl.IsEnabled = ds_is_enabled;
	st->hdl.OnUserEvent = OnDiscSensor;
	st->hdl.owner = node;
	st->compositor = sr->compositor;
	sr->compositor->interaction_sensors++;
	gf_node_set_private(node, st);
	gf_node_set_predestroy_function(node, DestroyDiscSensor);
}


typedef struct 
{
	SFVec2f start_drag;
	GF_Matrix initial_matrix;
	GF_Renderer *compositor;
	SensorHandler hdl;
} PS2DStack;

static void DestroyPlaneSensor2D(GF_Node *node)
{
	PS2DStack *st = (PS2DStack *) gf_node_get_private(node);
	R3D_SensorDeleted(st->compositor, &st->hdl);
	free(st);
}

static Bool ps2D_is_enabled(GF_Node *n)
{
	M_PlaneSensor2D *ps2d = (M_PlaneSensor2D *)n;
	return (ps2d->enabled || ps2d->isActive);
}

static void OnPlaneSensor2D(SensorHandler *sh, Bool is_over, u32 eventType, RayHitInfo *hit_info)
{
	M_PlaneSensor2D *ps = (M_PlaneSensor2D *)sh->owner;
	PS2DStack *stack = (PS2DStack *) gf_node_get_private(sh->owner);

	if (ps->isActive && (!ps->enabled || (eventType==GF_EVT_LEFTUP))) {
		if (ps->autoOffset) {
			ps->offset = ps->translation_changed;
			gf_node_event_out_str(sh->owner, "offset");
		}
		ps->isActive = 0;
		gf_node_event_out_str(sh->owner, "isActive");
		R3D_SetGrabbed(stack->compositor, 0);
	} else if ((eventType==GF_EVT_LEFTDOWN) && !ps->isActive) {
		gf_mx_copy(stack->initial_matrix, hit_info->local_to_world);
		stack->start_drag.x = hit_info->local_point.x - ps->offset.x;
		stack->start_drag.y = hit_info->local_point.y - ps->offset.y;
		ps->isActive = 1;
		gf_node_event_out_str(sh->owner, "isActive");
		R3D_SetGrabbed(stack->compositor, 1);
	} else if (ps->isActive) {
		GF_Ray loc_ray;
		SFVec3f res;
		loc_ray = hit_info->world_ray;
		gf_mx_apply_ray(&stack->initial_matrix, &loc_ray);

		R3D_Get2DPlaneIntersection(&loc_ray, &res);
		ps->trackPoint_changed.x = res.x;
		ps->trackPoint_changed.y = res.y;
		gf_node_event_out_str(sh->owner, "trackPoint_changed");

		res.x -= stack->start_drag.x;
		res.y -= stack->start_drag.y;
		/*clip*/
		if (ps->minPosition.x <= ps->maxPosition.x) {
			if (res.x < ps->minPosition.x) res.x = ps->minPosition.x;
			if (res.x > ps->maxPosition.x) res.x = ps->maxPosition.x;
		}
		if (ps->minPosition.y <= ps->maxPosition.y) {
			if (res.y < ps->minPosition.y) res.y = ps->minPosition.y;
			if (res.y > ps->maxPosition.y) res.y = ps->maxPosition.y;
		}
		ps->translation_changed.x = res.x;
		ps->translation_changed.y = res.y;
		gf_node_event_out_str(sh->owner, "translation_changed");
	}
}

SensorHandler *r3d_ps2D_get_handler(GF_Node *n)
{
	PS2DStack *st = (PS2DStack *)gf_node_get_private(n);
	return &st->hdl;
}

void R3D_InitPlaneSensor2D(Render3D *sr, GF_Node *node)
{
	PS2DStack *st = malloc(sizeof(PS2DStack));
	memset(st, 0, sizeof(PS2DStack));
	st->hdl.IsEnabled = ps2D_is_enabled;
	st->hdl.OnUserEvent = OnPlaneSensor2D;
	st->hdl.owner = node;
	st->compositor = sr->compositor;
	st->compositor->interaction_sensors++;
	gf_node_set_private(node, st);
	gf_node_set_predestroy_function(node, DestroyPlaneSensor2D);
}


typedef struct 
{
	Double last_time;
	GF_Renderer *compositor;
	SensorHandler hdl;
} Prox2DStack;

static void DestroyProximitySensor2D(GF_Node *node)
{
	Prox2DStack *st = (Prox2DStack *) gf_node_get_private(node);
	R3D_SensorDeleted(st->compositor, &st->hdl);
	free(st);
}

static Bool prox2D_is_enabled(GF_Node *n)
{
	return ((M_ProximitySensor2D *) n)->enabled;
}

static Bool prox2D_is_in_sensor(Prox2DStack *st, M_ProximitySensor2D *ps, Fixed X, Fixed Y)
{
	if (X < ps->center.x - ps->size.x/2) return 0;
	if (X > ps->center.x + ps->size.x/2) return 0;
	if (Y < ps->center.y - ps->size.y/2) return 0;
	if (Y > ps->center.y + ps->size.y/2) return 0;
	return 1;
}

static void OnProximitySensor2D(SensorHandler *sh, Bool is_over, u32 eventType, RayHitInfo *hit_info)
{
	M_ProximitySensor2D *ps = (M_ProximitySensor2D *)sh->owner;
	Prox2DStack *stack = (Prox2DStack *) gf_node_get_private(sh->owner);
	
	assert(ps->enabled);
	
	if (is_over) {
		stack->last_time = gf_node_get_scene_time(sh->owner);
		if (prox2D_is_in_sensor(stack, ps, hit_info->local_point.x, hit_info->local_point.y)) {
			ps->position_changed.x = hit_info->local_point.x;
			ps->position_changed.y = hit_info->local_point.y;
			gf_node_event_out_str(sh->owner, "position_changed");

			if (!ps->isActive) {
				ps->isActive = 1;
				gf_node_event_out_str(sh->owner, "isActive");
				ps->enterTime = stack->last_time;
				gf_node_event_out_str(sh->owner, "enterTime");
			}
			return;
		}
	} 
	/*either we're not over the shape or we're not in sensor*/
	if (ps->isActive) {
		ps->exitTime = stack->last_time;
		gf_node_event_out_str(sh->owner, "exitTime");
		ps->isActive = 0;
		gf_node_event_out_str(sh->owner, "isActive");
	}
}

SensorHandler *r3d_prox2D_get_handler(GF_Node *n)
{
	Prox2DStack *st = (Prox2DStack *)gf_node_get_private(n);
	return &st->hdl;
}


void R3D_InitProximitySensor2D(Render3D *sr, GF_Node *node)
{
	Prox2DStack *st = malloc(sizeof(Prox2DStack));
	memset(st, 0, sizeof(Prox2DStack));
	st->hdl.IsEnabled = prox2D_is_enabled;
	st->hdl.OnUserEvent = OnProximitySensor2D;
	st->hdl.owner = node;
	st->compositor = sr->compositor;
	st->compositor->interaction_sensors++;
	gf_node_set_private(node, st);
	gf_node_set_predestroy_function(node, DestroyProximitySensor2D);
}

typedef struct 
{
	SensorHandler hdl;
	Bool mouse_down;
	GF_Renderer *compositor;
} TouchSensorStack;

static void DestroyTouchSensor(GF_Node *node)
{
	TouchSensorStack *st = (TouchSensorStack *) gf_node_get_private(node);
	R3D_SensorDeleted(st->compositor, &st->hdl);
	free(st);
}

static Bool ts_is_enabled(GF_Node *n)
{
	return ((M_TouchSensor *) n)->enabled;
}

static void OnTouchSensor(SensorHandler *sh, Bool is_over, u32 eventType, RayHitInfo *hit_info)
{
	M_TouchSensor *ts = (M_TouchSensor *)sh->owner;
	TouchSensorStack *st = (TouchSensorStack *) gf_node_get_private(sh->owner);
	//assert(ts->enabled);

	/*isActive becomes false, send touch time*/
	if ((eventType==GF_EVT_LEFTUP) && ts->isActive) {
		ts->touchTime = gf_node_get_scene_time(sh->owner);
		gf_node_event_out_str(sh->owner, "touchTime");
		ts->isActive = 0;
		gf_node_event_out_str(sh->owner, "isActive");
		R3D_SetGrabbed(st->compositor, 0);
	}
	if (is_over != ts->isOver) {
		ts->isOver = is_over;
		gf_node_event_out_str(sh->owner, "isOver");
	}
	if ((eventType==GF_EVT_LEFTDOWN) && !ts->isActive) {
		ts->isActive = 1;
		gf_node_event_out_str(sh->owner, "isActive");
		R3D_SetGrabbed(st->compositor, 1);
	}
	if (is_over) {
		ts->hitPoint_changed = hit_info->local_point;
		gf_node_event_out_str(sh->owner, "hitPoint_changed");
		ts->hitNormal_changed = hit_info->hit_normal;
		gf_node_event_out_str(sh->owner, "hitNormal_changed");
		ts->hitTexCoord_changed = hit_info->hit_texcoords;
		gf_node_event_out_str(sh->owner, "hitTexCoord_changed");
	}
}

SensorHandler *r3d_touch_sensor_get_handler(GF_Node *n)
{
	TouchSensorStack *ts = (TouchSensorStack *)gf_node_get_private(n);
	return &ts->hdl;
}


void R3D_InitTouchSensor(Render3D *sr, GF_Node *node)
{
	TouchSensorStack *st = malloc(sizeof(TouchSensorStack));
	memset(st, 0, sizeof(TouchSensorStack));
	st->hdl.IsEnabled = ts_is_enabled;
	st->hdl.OnUserEvent = OnTouchSensor;
	st->hdl.owner = node;
	st->compositor = sr->compositor;
	st->compositor->interaction_sensors++;
	gf_node_set_private(node, st);
	gf_node_set_predestroy_function(node, DestroyTouchSensor);
}

void RenderProximitySensor(GF_Node *node, void *rs)
{
	SFVec3f user_pos, dist;
	SFRotation ori;
	RenderEffect3D *eff = (RenderEffect3D *)rs;
	M_ProximitySensor *ps = (M_ProximitySensor *)node;

	if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) {
		/*work with twice bigger bbox to get sure we're notify when culled out*/
		gf_vec_add(eff->bbox.max_edge, ps->center, ps->size);
		gf_vec_diff(eff->bbox.min_edge, ps->center, ps->size);
		gf_bbox_refresh(&eff->bbox);
		return;
	} else if (!ps->enabled || (eff->traversing_mode != TRAVERSE_SORT) ) return;

	user_pos = eff->camera->position;
	gf_mx_apply_vec(&eff->model_matrix, &user_pos);
	gf_vec_diff(dist, user_pos, ps->center);

	if (dist.x<0) dist.x *= -1;
	if (dist.y<0) dist.y *= -1;
	if (dist.z<0) dist.z *= -1;

	if ((2*dist.x <= ps->size.x) 
		&& (2*dist.y <= ps->size.y)
		&& (2*dist.z <= ps->size.z) ) {

		if (!ps->isActive) {
			ps->isActive = 1;
			gf_node_event_out_str(node, "isActive");
			ps->enterTime = gf_node_get_scene_time(node);
			gf_node_event_out_str(node, "enterTime");
		}
		if ((ps->position_changed.x != user_pos.x)
			|| (ps->position_changed.y != user_pos.y)
			|| (ps->position_changed.z != user_pos.z) )
		{
			ps->position_changed = user_pos;
			gf_node_event_out_str(node, "position_changed");
		}
		dist = eff->camera->target;
		gf_mx_apply_vec(&eff->model_matrix, &dist);
		ori = camera_get_orientation(user_pos, dist, eff->camera->up);
		if ((ori.q != ps->orientation_changed.q)
			|| (ori.x != ps->orientation_changed.x)
			|| (ori.y != ps->orientation_changed.y)
			|| (ori.z != ps->orientation_changed.z) ) {
			ps->orientation_changed = ori;
			gf_node_event_out_str(node, "orientation_changed");
		}
	} else if (ps->isActive) {
		ps->isActive = 0;
		gf_node_event_out_str(node, "isActive");
		ps->exitTime = gf_node_get_scene_time(node);
		gf_node_event_out_str(node, "exitTime");
	}
}

void R3D_InitProximitySensor(Render3D *sr, GF_Node *node)
{
	gf_node_set_render_function(node, RenderProximitySensor);
}


typedef struct 
{
	SFVec3f start_drag;
	GF_Plane tracker;
	GF_Matrix initial_matrix;
	GF_Renderer *compositor;
	SensorHandler hdl;
} PSStack;

static void DestroyPlaneSensor(GF_Node *node)
{
	PSStack *st = (PSStack *) gf_node_get_private(node);
	R3D_SensorDeleted(st->compositor, &st->hdl);
	free(st);
}

static Bool ps_is_enabled(GF_Node *n)
{
	M_PlaneSensor *ps = (M_PlaneSensor *)n;
	return (ps->enabled || ps->isActive);
}

static void OnPlaneSensor(SensorHandler *sh, Bool is_over, u32 eventType, RayHitInfo *hit_info)
{
	M_PlaneSensor *ps = (M_PlaneSensor *)sh->owner;
	PSStack *stack = (PSStack *) gf_node_get_private(sh->owner);
	
	
	if (ps->isActive && (!ps->enabled || (eventType==GF_EVT_LEFTUP))) {
		if (ps->autoOffset) {
			ps->offset = ps->translation_changed;
			gf_node_event_out_str(sh->owner, "offset");
		}
		ps->isActive = 0;
		gf_node_event_out_str(sh->owner, "isActive");
		R3D_SetGrabbed(stack->compositor, 0);
	}
	else if ((eventType==GF_EVT_LEFTDOWN) && !ps->isActive) {
		gf_mx_copy(stack->initial_matrix, hit_info->local_to_world);
		gf_vec_diff(stack->start_drag, hit_info->local_point, ps->offset);
		stack->tracker.normal.x = stack->tracker.normal.y = 0; stack->tracker.normal.z = FIX_ONE;
		stack->tracker.d = - gf_vec_dot(stack->start_drag, stack->tracker.normal);
		ps->isActive = 1;
		gf_node_event_out_str(sh->owner, "isActive");
		R3D_SetGrabbed(stack->compositor, 1);
	}
	else if (ps->isActive) {
		GF_Ray loc_ray;
		SFVec3f res;
		loc_ray = hit_info->world_ray;
		gf_mx_apply_ray(&stack->initial_matrix, &loc_ray);
		gf_plane_intersect_line(&stack->tracker, &loc_ray.orig, &loc_ray.dir, &res);
		ps->trackPoint_changed = res;
		gf_node_event_out_str(sh->owner, "trackPoint_changed");

		gf_vec_diff(res, res, stack->start_drag);
		/*clip*/
		if (ps->minPosition.x <= ps->maxPosition.x) {
			if (res.x < ps->minPosition.x) res.x = ps->minPosition.x;
			if (res.x > ps->maxPosition.x) res.x = ps->maxPosition.x;
		}
		if (ps->minPosition.y <= ps->maxPosition.y) {
			if (res.y < ps->minPosition.y) res.y = ps->minPosition.y;
			if (res.y > ps->maxPosition.y) res.y = ps->maxPosition.y;
		}
		ps->translation_changed = res;
		gf_node_event_out_str(sh->owner, "translation_changed");
	}
}

SensorHandler *r3d_ps_get_handler(GF_Node *n)
{
	PSStack *st = (PSStack *)gf_node_get_private(n);
	return &st->hdl;
}

void R3D_InitPlaneSensor(Render3D *sr, GF_Node *node)
{
	PSStack *st = malloc(sizeof(PSStack));
	memset(st, 0, sizeof(PSStack));
	st->hdl.IsEnabled = ps_is_enabled;
	st->hdl.OnUserEvent = OnPlaneSensor;
	st->hdl.owner = node;
	st->compositor = sr->compositor;
	st->compositor->interaction_sensors++;
	gf_node_set_private(node, st);
	gf_node_set_predestroy_function(node, DestroyPlaneSensor);
}

typedef struct 
{
	SensorHandler hdl;
	GF_Renderer *compositor;
	GF_Matrix init_matrix;
	Bool disk_mode;
	SFVec3f grab_start;
	GF_Plane yplane, zplane, xplane;
} CylinderSensorStack;

static void DestroyCylinderSensor(GF_Node *node)
{
	CylinderSensorStack *st = (CylinderSensorStack *) gf_node_get_private(node);
	R3D_SensorDeleted(st->compositor, &st->hdl);
	free(st);
}

static Bool cs_is_enabled(GF_Node *n)
{
	M_CylinderSensor *cs = (M_CylinderSensor *)n;
	return (cs->enabled || cs->isActive);
}

static void OnCylinderSensor(SensorHandler *sh, Bool is_over, u32 eventType, RayHitInfo *hit_info)
{
	M_CylinderSensor *cs = (M_CylinderSensor *)sh->owner;
	CylinderSensorStack *st = (CylinderSensorStack *) gf_node_get_private(sh->owner);

	if (cs->isActive && (!cs->enabled || (eventType==GF_EVT_LEFTUP)) ) {
		if (cs->autoOffset) {
			cs->offset = cs->rotation_changed.q;
			gf_node_event_out_str(sh->owner, "offset");
		}
		cs->isActive = 0;
		gf_node_event_out_str(sh->owner, "isActive");
		R3D_SetGrabbed(st->compositor, 0);
	}
	else if ((eventType==GF_EVT_LEFTDOWN) && !cs->isActive) {
		GF_Ray r;
		SFVec3f yaxis;
		Fixed acute, reva;
		SFVec3f bearing;

		gf_mx_copy(st->init_matrix, hit_info->world_to_local);
		/*get initial angle & check disk mode*/
		r = hit_info->world_ray;
		gf_vec_add(r.dir, r.orig, r.dir);
		gf_mx_apply_vec(&hit_info->world_to_local, &r.orig);
		gf_mx_apply_vec(&hit_info->world_to_local, &r.dir);
		gf_vec_diff(bearing, r.orig, r.dir);
		gf_vec_norm(&bearing);
		yaxis.x = yaxis.z = 0;
		yaxis.y = FIX_ONE;
		acute = gf_vec_dot(bearing, yaxis);
		if (acute < -FIX_ONE) acute = -FIX_ONE; 
		else if (acute > FIX_ONE) acute = FIX_ONE;
		acute = gf_acos(acute);
		reva = ABS(GF_PI - acute);
		if (reva<acute) acute = reva;
		st->disk_mode = (acute < cs->diskAngle) ? 1 : 0;

		st->grab_start = hit_info->local_point;
		/*cos we're lazy*/
		st->yplane.d = 0; st->yplane.normal.x = st->yplane.normal.z = st->yplane.normal.y = 0;
		st->zplane = st->xplane = st->yplane;
		st->xplane.normal.x = FIX_ONE;
		st->yplane.normal.y = FIX_ONE;
		st->zplane.normal.z = FIX_ONE;

		cs->rotation_changed.x = 0;
		cs->rotation_changed.y = FIX_ONE;
		cs->rotation_changed.z = 0;

		cs->isActive = 1;
		gf_node_event_out_str(sh->owner, "isActive");
		R3D_SetGrabbed(st->compositor, 1);
	}
	else if (cs->isActive) {
		GF_Ray r;
		Fixed radius, rot;
		SFVec3f dir1, dir2, cx;

		if (is_over) {
			cs->trackPoint_changed = hit_info->local_point;
			gf_node_event_out_str(sh->owner, "trackPoint_changed");
		} else {
			GF_Plane project_to;
			r = hit_info->world_ray;
			gf_mx_apply_ray(&st->init_matrix, &r);

			/*no intersection, use intersection with "main" fronting plane*/
			if ( ABS(r.dir.z) > ABS(r.dir.y)) {
				if (ABS(r.dir.x) > ABS(r.dir.x)) project_to = st->xplane;
				else project_to = st->zplane;
			} else project_to = st->yplane;
			if (!gf_plane_intersect_line(&project_to, &r.orig, &r.dir, &hit_info->local_point)) return;
		}

  		dir1.x = hit_info->local_point.x; dir1.y = 0; dir1.z = hit_info->local_point.z;
		if (st->disk_mode) {
			radius = FIX_ONE;
		} else {
			radius = gf_vec_len(dir1);
		}
		gf_vec_norm(&dir1);
       	dir2.x = st->grab_start.x; dir2.y = 0; dir2.z = st->grab_start.z;
		gf_vec_norm(&dir2);
		cx = gf_vec_cross(dir2, dir1);
		gf_vec_norm(&cx);
		if (gf_vec_len(cx)<GF_EPSILON_FLOAT) return;
		rot = gf_mulfix(radius, gf_acos(gf_vec_dot(dir2, dir1)) );
		if (fabs(cx.y + FIX_ONE) < FIX_EPSILON) rot = -rot;
		if (cs->autoOffset) rot += cs->offset;

		if (cs->minAngle < cs->maxAngle) {
			if (rot < cs->minAngle) rot = cs->minAngle;
			else if (rot > cs->maxAngle) rot = cs->maxAngle;
		}
		cs->rotation_changed.q = rot;
		gf_node_event_out_str(sh->owner, "rotation_changed");
	}
}

SensorHandler *r3d_cs_get_handler(GF_Node *n)
{
	CylinderSensorStack *st = (CylinderSensorStack  *)gf_node_get_private(n);
	return &st->hdl;
}

void R3D_InitCylinderSensor(Render3D *sr, GF_Node *node)
{
	CylinderSensorStack *st = malloc(sizeof(CylinderSensorStack));
	memset(st, 0, sizeof(CylinderSensorStack));
	st->hdl.IsEnabled = cs_is_enabled;
	st->hdl.OnUserEvent = OnCylinderSensor;
	st->hdl.owner = node;
	st->compositor = sr->compositor;
	st->compositor->interaction_sensors++;
	gf_node_set_private(node, st);
	gf_node_set_predestroy_function(node, DestroyCylinderSensor);
}


typedef struct 
{
	SensorHandler hdl;
	GF_Renderer *compositor;
	Fixed radius;
	/*center in world coords */
	SFVec3f grab_vec, center;
} SphereSensorStack;

static void DestroySphereSensor(GF_Node *node)
{
	SphereSensorStack *st = (SphereSensorStack *) gf_node_get_private(node);
	R3D_SensorDeleted(st->compositor, &st->hdl);
	free(st);
}

static Bool sphere_is_enabled(GF_Node *n)
{
	M_SphereSensor *ss = (M_SphereSensor *)n;
	return (ss->enabled || ss->isActive);
}

static void OnSphereSensor(SensorHandler *sh, Bool is_over, u32 eventType, RayHitInfo *hit_info)
{
	M_SphereSensor *sphere = (M_SphereSensor *)sh->owner;
	SphereSensorStack *st = (SphereSensorStack *) gf_node_get_private(sh->owner);

	if (sphere->isActive && (!sphere->enabled || (eventType==GF_EVT_LEFTUP)) ) {
		if (sphere->autoOffset) {
			sphere->offset = sphere->rotation_changed;
			gf_node_event_out_str(sh->owner, "offset");
		}
		sphere->isActive = 0;
		gf_node_event_out_str(sh->owner, "isActive");
		R3D_SetGrabbed(st->compositor, 0);
	}
	else if ((eventType==GF_EVT_LEFTDOWN) && !sphere->isActive) {
		st->center.x = st->center.y = st->center.z = 0;
		gf_mx_apply_vec(&hit_info->local_to_world, &st->center);
		st->radius = gf_vec_len(hit_info->local_point);
		if (!st->radius) st->radius = FIX_ONE;
		st->grab_vec = gf_vec_scale(hit_info->local_point, gf_divfix(FIX_ONE, st->radius));

		sphere->isActive = 1;
		gf_node_event_out_str(sh->owner, "isActive");
		R3D_SetGrabbed(st->compositor, 1);
	}
	else if (sphere->isActive) {
		SFVec3f vec, axis;
		SFVec4f q1, q2;
		SFRotation r;
		Fixed cl;
		if (is_over) {
			sphere->trackPoint_changed = hit_info->local_point;
			gf_node_event_out_str(sh->owner, "trackPoint_changed");
		} else {
			GF_Ray r;
			r = hit_info->world_ray;
			gf_mx_apply_ray(&hit_info->world_to_local, &r);
			if (!gf_ray_hit_sphere(&r, NULL, st->radius, &hit_info->local_point)) {
				vec.x = vec.y = vec.z = 0;
				/*doesn't work properly...*/
				hit_info->local_point = gf_closest_point_to_line(r.orig, r.dir, vec);
			}
		}

		vec = gf_vec_scale(hit_info->local_point, gf_divfix(FIX_ONE, st->radius));
		axis = gf_vec_cross(st->grab_vec, vec);
		cl = gf_vec_len(axis);

		if (cl < -FIX_ONE) cl = -FIX_ONE;
		else if (cl > FIX_ONE) cl = FIX_ONE;
		r.q = gf_asin(cl);
		if (gf_vec_dot(st->grab_vec, vec) < 0) r.q += GF_PI / 2;

		gf_vec_norm(&axis);
		r.x = axis.x; r.y = axis.y; r.z = axis.z;
		q1 = gf_quat_from_rotation(r);
		if (sphere->autoOffset) {
			q2 = gf_quat_from_rotation(sphere->offset);
			q1 = gf_quat_multiply(&q1, &q2);
		}
		sphere->rotation_changed = gf_quat_to_rotation(&q1);
		gf_node_event_out_str(sh->owner, "rotation_changed");
	}
}

SensorHandler *r3d_sphere_get_handler(GF_Node *n)
{
	SphereSensorStack *st = (SphereSensorStack *)gf_node_get_private(n);
	return &st->hdl;
}

void R3D_InitSphereSensor(Render3D *sr, GF_Node *node)
{
	SphereSensorStack *st = malloc(sizeof(SphereSensorStack));
	memset(st, 0, sizeof(SphereSensorStack));
	st->hdl.IsEnabled = sphere_is_enabled;
	st->hdl.OnUserEvent = OnSphereSensor;
	st->hdl.owner = node;
	st->compositor = sr->compositor;
	st->compositor->interaction_sensors++;
	gf_node_set_private(node, st);
	gf_node_set_predestroy_function(node, DestroySphereSensor);
}

void RenderVisibilitySensor(GF_Node *node, void *rs)
{
	RenderEffect3D *eff = (RenderEffect3D *)rs;
	M_VisibilitySensor *vs = (M_VisibilitySensor *)node;

	if (!vs->enabled) return;

	if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) {
		/*work with twice bigger bbox to get sure we're notify when culled out*/
		gf_vec_add(eff->bbox.max_edge, vs->center, vs->size);
		gf_vec_diff(eff->bbox.min_edge, vs->center, vs->size);
		gf_bbox_refresh(&eff->bbox);

	} else if (eff->traversing_mode==TRAVERSE_SORT) {
		Bool visible;
		u32 cull_flag;
		GF_BBox bbox;
		SFVec3f s;
		s = gf_vec_scale(vs->size, FIX_ONE/2);
		/*cull with normal bbox*/
		gf_vec_add(bbox.max_edge, vs->center, s);
		gf_vec_diff(bbox.min_edge, vs->center, s);
		gf_bbox_refresh(&bbox);
		cull_flag = eff->cull_flag;
		eff->cull_flag = CULL_INTERSECTS;
		visible = node_cull(eff, &bbox, 0);
		eff->cull_flag = cull_flag;

		if (visible && !vs->isActive) {
			vs->isActive = 1;
			gf_node_event_out_str(node, "isActive");
			vs->enterTime = gf_node_get_scene_time(node);
			gf_node_event_out_str(node, "enterTime");
		}
		else if (!visible && vs->isActive) {
			vs->isActive = 0;
			gf_node_event_out_str(node, "isActive");
			vs->exitTime = gf_node_get_scene_time(node);
			gf_node_event_out_str(node, "exitTime");
		}
	}
}

void R3D_InitVisibilitySensor(Render3D *sr, GF_Node *node)
{
	gf_node_set_render_function(node, RenderVisibilitySensor);
}
