/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Compositor sub-project
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



#include "nodes_stacks.h"
#include "visual_manager.h"
/*for anchor*/
#include "mpeg4_grouping.h"

#ifndef GPAC_DISABLE_VRML

/*for event DOM filtering type ...*/
#include <gpac/scenegraph_svg.h>


static void mpeg4_sensor_deleted(GF_Node *node, GF_SensorHandler *hdl)
{
	GF_Compositor *compositor = gf_sc_get_compositor(node);
	if (compositor) {
		GF_VisualManager *visual;
		u32 i=0;
		gf_list_del_item(compositor->sensors, hdl);
		gf_list_del_item(compositor->previous_sensors, hdl);
		if (compositor->interaction_sensors) compositor->interaction_sensors--;
		while ( (visual=gf_list_enum(compositor->visuals, &i)) ) {
			if (visual->offscreen)
				compositor_compositetexture_sensor_delete(visual->offscreen, hdl);
		}

#ifndef GPAC_DISABLE_SVG
		gf_sg_unregister_event_type(gf_node_get_graph(node), GF_DOM_EVENT_MOUSE|GF_DOM_EVENT_KEY);
#endif
	}
}

static void mpeg4_sensor_created(GF_Compositor *compositor, GF_Node *node)
{
	compositor->interaction_sensors--;
#ifndef GPAC_DISABLE_SVG
	gf_sg_register_event_type(gf_node_get_graph(node), GF_DOM_EVENT_MOUSE|GF_DOM_EVENT_KEY);
#endif
}


typedef struct
{
	GROUPING_MPEG4_STACK_2D

	Bool enabled, active, over;
	GF_SensorHandler hdl;
	GF_Compositor *compositor;
} AnchorStack;

static void TraverseAnchor(GF_Node *node, void *rs, Bool is_destroy)
{
	AnchorStack *st = (AnchorStack *) gf_node_get_private(node);
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;

	if (is_destroy) {
		mpeg4_sensor_deleted(node, &st->hdl);
		gf_sc_check_focus_upon_destroy(node);
		if (st->sensors) gf_list_del(st->sensors);
		gf_free(st);
		return;
	}

	if (gf_node_dirty_get(node) & GF_SG_NODE_DIRTY) {
		MFURL *url = NULL;
		switch (gf_node_get_tag(node)) {
		case TAG_MPEG4_Anchor:
			url = & ((M_Anchor *)node)->url;
			break;
#ifndef GPAC_DISABLE_X3D
		case TAG_X3D_Anchor:
			url = & ((X_Anchor *)node)->url;
			break;
#endif
		}
		st->enabled = 0;
		if (url && url->count && url->vals[0].url && strlen(url->vals[0].url) )
			st->enabled = 1;

		gf_node_dirty_clear(node, GF_SG_NODE_DIRTY);
	}

	group_2d_traverse(node, (GroupingNode2D*)st, tr_state);
}

static void anchor_activation(GF_Node *node, AnchorStack *st, GF_Compositor *compositor)
{
	GF_Event evt;
	u32 i;
	MFURL *url = NULL;
	switch (gf_node_get_tag(node)) {
	case TAG_MPEG4_Anchor:
		url = & ((M_Anchor *)node)->url;
		evt.navigate.param_count = ((M_Anchor *)node)->parameter.count;
		evt.navigate.parameters = (const char **) ((M_Anchor *)node)->parameter.vals;
		break;
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Anchor:
		url = & ((X_Anchor *)node)->url;
		evt.navigate.param_count = ((X_Anchor *)node)->parameter.count;
		evt.navigate.parameters = (const char **) ((X_Anchor *)node)->parameter.vals;
		break;
#endif
	}
	evt.type = GF_EVENT_NAVIGATE;
	i=0;
	while (url && i<url->count) {
		evt.navigate.to_url = url->vals[i].url;
		if (!evt.navigate.to_url) break;
		/*current scene navigation*/
		if (evt.navigate.to_url[0] == '#') {
			GF_Node *bindable;
			evt.navigate.to_url++;
			bindable = gf_sg_find_node_by_name(gf_node_get_graph(node), (char *) evt.navigate.to_url);
			if (bindable) {
				Bindable_SetSetBind(bindable, 1);
				break;
			}
		} else {
			if (gf_scene_process_anchor(node, &evt))
				break;
		}
		i++;
	}
}

static Bool OnAnchor(GF_SensorHandler *sh, Bool is_over, Bool is_cancel, GF_Event *ev, GF_Compositor *compositor)
{
	GF_Event evt;
	MFURL *url = NULL;
	AnchorStack *st = (AnchorStack *) gf_node_get_private(sh->sensor);

	if ((ev->type==GF_EVENT_MOUSEDOWN) && (ev->mouse.button==GF_MOUSE_LEFT)) st->active = 1;
	else if ((ev->type==GF_EVENT_KEYDOWN) && (ev->key.key_code==GF_KEY_ENTER)) st->active = 1;
	else if (st->active && (
	             /*mouse*/ ((ev->type==GF_EVENT_MOUSEUP) && (ev->mouse.button==GF_MOUSE_LEFT))
	             || /*mouse*/((ev->type==GF_EVENT_KEYUP) && (ev->key.key_code==GF_KEY_ENTER))
	         ) ) {
		if (!is_cancel) anchor_activation(sh->sensor, st, compositor);
	} else if (is_over && !st->over) {
		st->over = 1;
		evt.type = GF_EVENT_NAVIGATE_INFO;
		switch (gf_node_get_tag(sh->sensor)) {
		case TAG_MPEG4_Anchor:
			evt.navigate.to_url = ((M_Anchor *)sh->sensor)->description.buffer;
			url = & ((M_Anchor *)sh->sensor)->url;
			break;
#ifndef GPAC_DISABLE_X3D
		case TAG_X3D_Anchor:
			evt.navigate.to_url = ((X_Anchor *)sh->sensor)->description.buffer;
			url = & ((X_Anchor *)sh->sensor)->url;
			break;
#endif
		}
		if (url && (!evt.navigate.to_url || !strlen(evt.navigate.to_url))) evt.navigate.to_url = url->vals[0].url;
		gf_sc_send_event(compositor, &evt);

	} else if (!is_over) {
		st->over = 0;
	}
	return 0;
}

static Bool anchor_is_enabled(GF_Node *node)
{
	AnchorStack *st = (AnchorStack *) gf_node_get_private(node);
	return st->enabled;
}

static void on_activate_anchor(GF_Node *node, GF_Route *route)
{
	AnchorStack *st = (AnchorStack *) gf_node_get_private(node);
	if (!((M_Anchor *)node)->on_activate) return;

	anchor_activation(node, st, st->compositor);
}

GF_SensorHandler *gf_sc_anchor_get_handler(GF_Node *n)
{
	AnchorStack *st = (AnchorStack *) gf_node_get_private(n);
	return &st->hdl;
}


void compositor_init_anchor(GF_Compositor *compositor, GF_Node *node)
{
	AnchorStack *stack;
	GF_SAFEALLOC(stack, AnchorStack);
	if (!stack) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate anchor stack\n"));
		return;
	}

	stack->hdl.IsEnabled = anchor_is_enabled;
	stack->hdl.OnUserEvent = OnAnchor;
	stack->hdl.sensor = node;
	if (gf_node_get_tag(node)==TAG_MPEG4_Anchor) {
		((M_Anchor *)node)->on_activate = on_activate_anchor;
	}
	stack->compositor = compositor;
	mpeg4_sensor_created(compositor, node);
	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, TraverseAnchor);
}


typedef struct
{
	GF_SensorHandler hdl;
	GF_Compositor *compositor;
	Fixed start_angle;
	GF_Matrix initial_matrix;
} DiscSensorStack;

static void DestroyDiscSensor(GF_Node *node, void *rs, Bool is_destroy)
{
	if (is_destroy) {
		DiscSensorStack *st = (DiscSensorStack *) gf_node_get_private(node);
		mpeg4_sensor_deleted(node, &st->hdl);
		gf_free(st);
	}
}

static Bool ds_is_enabled(GF_Node *n)
{
	M_DiscSensor *ds = (M_DiscSensor *)n;
	return (ds->enabled || ds->isActive);
}


static Bool OnDiscSensor(GF_SensorHandler *sh, Bool is_over, Bool is_cancel, GF_Event *ev, GF_Compositor *compositor)
{
	Bool is_mouse = (ev->type <= GF_EVENT_LAST_MOUSE) ? 1 : 0;
	M_DiscSensor *ds = (M_DiscSensor *)sh->sensor;
	DiscSensorStack *stack = (DiscSensorStack *) gf_node_get_private(sh->sensor);

	if (ds->isActive &&
	        (!ds->enabled
	         || /*mouse*/((ev->type==GF_EVENT_MOUSEUP) && (ev->mouse.button==GF_MOUSE_LEFT))
	         || /*keyboar*/(!is_mouse && (!is_over|| ((ev->type==GF_EVENT_KEYDOWN) && (ev->key.key_code==GF_KEY_ENTER))) )
	        ) ) {
		if (ds->autoOffset) {
			ds->offset = ds->rotation_changed;
			/*that's an exposedField*/
			if (!is_cancel) gf_node_event_out(sh->sensor, 4/*"offset"*/);
		}
		ds->isActive = 0;
		if (!is_cancel) gf_node_event_out(sh->sensor, 5/*"isActive"*/);
		sh->grabbed = 0;
		return is_cancel ? 0 : 1;
	} else if (is_mouse) {
		if (!ds->isActive && (ev->type==GF_EVENT_MOUSEDOWN) && (ev->mouse.button==GF_MOUSE_LEFT)) {
			/*store inverse matrix*/
			gf_mx_copy(stack->initial_matrix, compositor->hit_local_to_world);
			stack->start_angle = gf_atan2(compositor->hit_local_point.y, compositor->hit_local_point.x);
			ds->isActive = 1;
			gf_node_event_out(sh->sensor, 5/*"isActive"*/);
			sh->grabbed = 1;
			return 1;
		}
		else if (ds->isActive) {
			GF_Ray loc_ray;
			Fixed rot;
			SFVec3f res;
			loc_ray = compositor->hit_world_ray;
			gf_mx_apply_ray(&stack->initial_matrix, &loc_ray);
			compositor_get_2d_plane_intersection(&loc_ray, &res);

			rot = gf_atan2(res.y, res.x) - stack->start_angle + ds->offset;
			if (ds->minAngle < ds->maxAngle) {
				/*FIXME this doesn't work properly*/
				if (rot < ds->minAngle) rot = ds->minAngle;
				if (rot > ds->maxAngle) rot = ds->maxAngle;
			}
			ds->rotation_changed = rot;
			gf_node_event_out(sh->sensor, 6/*"rotation_changed"*/);
			ds->trackPoint_changed.x = res.x;
			ds->trackPoint_changed.y = res.y;
			gf_node_event_out(sh->sensor, 7/*"trackPoint_changed"*/);
			return 1;
		}
	} else {
		if (!ds->isActive && is_over && (ev->type==GF_EVENT_KEYDOWN) && (ev->key.key_code==GF_KEY_ENTER)) {
			ds->isActive = 1;
			stack->start_angle = ds->offset;
			gf_node_event_out(sh->sensor, 5/*"isActive"*/);
			return 1;
		}
		else if (ds->isActive && (ev->type==GF_EVENT_KEYDOWN)) {
			Fixed res;
			Fixed diff = (ev->key.flags & GF_KEY_MOD_SHIFT) ? GF_PI/8 : GF_PI/64;
			res = stack->start_angle;
			switch (ev->key.key_code) {
			case GF_KEY_LEFT:
			case GF_KEY_UP:
				res += -diff;
				break;
			case GF_KEY_RIGHT:
			case GF_KEY_DOWN:
				res += diff;
				break;
			case GF_KEY_HOME:
				res = ds->offset;
				break;
			default:
				return 0;
			}
			if (ds->minAngle < ds->maxAngle) {
				/*FIXME this doesn't work properly*/
				if (res < ds->minAngle) res = ds->minAngle;
				if (res > ds->maxAngle) res = ds->maxAngle;
			}
			stack->start_angle = res;
			ds->rotation_changed = res;
			gf_node_event_out(sh->sensor, 6/*"rotation_changed"*/);
			return 1;
		}
	}
	return 0;
}

static GF_SensorHandler *disc_sensor_get_handler(GF_Node *n)
{
	DiscSensorStack *st = (DiscSensorStack *)gf_node_get_private(n);
	return &st->hdl;
}

void compositor_init_disc_sensor(GF_Compositor *compositor, GF_Node *node)
{
	DiscSensorStack *st;
	GF_SAFEALLOC(st, DiscSensorStack);
	if (!st) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate disc sensor stack\n"));
		return;
	}

	st->hdl.IsEnabled = ds_is_enabled;
	st->hdl.OnUserEvent = OnDiscSensor;
	st->hdl.sensor = node;
	st->compositor = compositor;
	mpeg4_sensor_created(compositor, node);
	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, DestroyDiscSensor);
}


typedef struct
{
	SFVec2f start_drag;
	GF_Matrix initial_matrix;
	GF_Compositor *compositor;
	GF_SensorHandler hdl;
} PS2DStack;

static void DestroyPlaneSensor2D(GF_Node *node, void *rs, Bool is_destroy)
{
	if (is_destroy) {
		PS2DStack *st = (PS2DStack *) gf_node_get_private(node);
		mpeg4_sensor_deleted(node, &st->hdl);
		gf_free(st);
	}
}

static Bool ps2D_is_enabled(GF_Node *n)
{
	M_PlaneSensor2D *ps2d = (M_PlaneSensor2D *)n;
	return (ps2d->enabled || ps2d->isActive);
}

static Bool OnPlaneSensor2D(GF_SensorHandler *sh, Bool is_over, Bool is_cancel, GF_Event *ev, GF_Compositor *compositor)
{
	Bool is_mouse = (ev->type <= GF_EVENT_LAST_MOUSE) ? 1 : 0;
	M_PlaneSensor2D *ps = (M_PlaneSensor2D *)sh->sensor;
	PS2DStack *stack = (PS2DStack *) gf_node_get_private(sh->sensor);


	if (ps->isActive &&
	        (!ps->enabled
	         || /*mouse*/((ev->type==GF_EVENT_MOUSEUP) && (ev->mouse.button==GF_MOUSE_LEFT))
	         || /*keyboar*/(!is_mouse && (!is_over|| ((ev->type==GF_EVENT_KEYDOWN) && (ev->key.key_code==GF_KEY_ENTER))) )
	        ) ) {
		if (ps->autoOffset) {
			ps->offset = ps->translation_changed;
			if (!is_cancel) gf_node_event_out(sh->sensor, 4/*"offset"*/);
		}

		ps->isActive = 0;
		if (!is_cancel) gf_node_event_out(sh->sensor, 5/*"isActive"*/);
		sh->grabbed = 0;
		return is_cancel ? 0 : 1;
	} else if (is_mouse) {
		if (!ps->isActive && (ev->type==GF_EVENT_MOUSEDOWN) && (ev->mouse.button==GF_MOUSE_LEFT)) {
			gf_mx_copy(stack->initial_matrix, compositor->hit_local_to_world);
			stack->start_drag.x = compositor->hit_local_point.x - ps->offset.x;
			stack->start_drag.y = compositor->hit_local_point.y - ps->offset.y;
			ps->isActive = 1;
			gf_node_event_out(sh->sensor, 5/*"isActive"*/);
			sh->grabbed = 1;
			/*fallthrough to fire mouse coords*/
			//return 1;
		}
		if (ps->isActive) {
			SFVec3f res;
			GF_Ray loc_ray;
			loc_ray = compositor->hit_world_ray;
			gf_mx_apply_ray(&stack->initial_matrix, &loc_ray);

			compositor_get_2d_plane_intersection(&loc_ray, &res);

			ps->trackPoint_changed.x = res.x;
			ps->trackPoint_changed.y = res.y;
			gf_node_event_out(sh->sensor, 6/*"trackPoint_changed"*/);

			res.x -= stack->start_drag.x;
			res.y -= stack->start_drag.y;
			/*clip*/
			if (ps->minPosition.x <= ps->maxPosition.x) {
				if (res.x < ps->minPosition.x) res.x = ps->minPosition.x;
				if (res.x > ps->maxPosition.x) res.x = ps->maxPosition.x;
			}
			if (ps->minPosition.y <= ps->maxPosition.y) {
				if (res.y < ps->minPosition.y)
					res.y = ps->minPosition.y;
				if (res.y > ps->maxPosition.y)
					res.y = ps->maxPosition.y;
			}
			ps->translation_changed.x = res.x;
			ps->translation_changed.y = res.y;
			gf_node_event_out(sh->sensor, 7/*"translation_changed"*/);
			return 1;
		}
	} else {
		if (!ps->isActive && is_over && (ev->type==GF_EVENT_KEYDOWN) && (ev->key.key_code==GF_KEY_ENTER)) {
			ps->isActive = 1;
			stack->start_drag = ps->offset;
			gf_node_event_out(sh->sensor, 5/*"isActive"*/);
			return 1;
		}
		else if (ps->isActive && (ev->type==GF_EVENT_KEYDOWN)) {
			SFVec2f res;
			Fixed diff = (ev->key.flags & GF_KEY_MOD_SHIFT) ? 5*FIX_ONE : FIX_ONE;
			if (!gf_sg_use_pixel_metrics(gf_node_get_graph(sh->sensor)))
				diff = gf_divfix(diff, INT2FIX(compositor->vp_width/2));
			res = stack->start_drag;
			switch (ev->key.key_code) {
			case GF_KEY_LEFT:
				res.x += -diff;
				break;
			case GF_KEY_RIGHT:
				res.x += diff;
				break;
			case GF_KEY_UP:
				res.y += diff;
				break;
			case GF_KEY_DOWN:
				res.y += -diff;
				break;
			case GF_KEY_HOME:
				res = ps->offset;
				break;
			default:
				return 0;
			}
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
			gf_node_event_out(sh->sensor, 7/*"translation_changed"*/);
			ps->trackPoint_changed.x = res.x + stack->start_drag.x;
			ps->trackPoint_changed.y = res.y + stack->start_drag.y;
			gf_node_event_out(sh->sensor, 6/*"trackPoint_changed"*/);
			stack->start_drag = res;
			return 1;
		}
	}
	return 0;
}

static GF_SensorHandler *plane_sensor2d_get_handler(GF_Node *n)
{
	PS2DStack *st = (PS2DStack *)gf_node_get_private(n);
	return &st->hdl;
}

void compositor_init_plane_sensor2d(GF_Compositor *compositor, GF_Node *node)
{
	PS2DStack *st;
	GF_SAFEALLOC(st, PS2DStack);
	if (!st) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate plane sensor 2d stack\n"));
		return;
	}

	st->hdl.IsEnabled = ps2D_is_enabled;
	st->hdl.OnUserEvent = OnPlaneSensor2D;
	st->hdl.sensor = node;
	st->compositor = compositor;
	mpeg4_sensor_created(compositor, node);
	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, DestroyPlaneSensor2D);
}


typedef struct
{
	Double last_time;
	GF_Compositor *compositor;
	GF_SensorHandler hdl;
} Prox2DStack;

static void DestroyProximitySensor2D(GF_Node *node, void *rs, Bool is_destroy)
{
	if (is_destroy) {
		Prox2DStack *st = (Prox2DStack *) gf_node_get_private(node);
		mpeg4_sensor_deleted(node, &st->hdl);
		gf_free(st);
	}
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

static Bool OnProximitySensor2D(GF_SensorHandler *sh, Bool is_over, Bool is_cancel, GF_Event *ev, GF_Compositor *compositor)
{
	M_ProximitySensor2D *ps = (M_ProximitySensor2D *)sh->sensor;
	Prox2DStack *stack = (Prox2DStack *) gf_node_get_private(sh->sensor);

	assert(ps->enabled);

	if (is_over) {
		stack->last_time = gf_node_get_scene_time(sh->sensor);
		if (is_cancel) return 0;
		if (prox2D_is_in_sensor(stack, ps, compositor->hit_local_point.x, compositor->hit_local_point.y)) {
			ps->position_changed.x = compositor->hit_local_point.x;
			ps->position_changed.y = compositor->hit_local_point.y;
			gf_node_event_out(sh->sensor, 4/*"position_changed"*/);

			if (!ps->isActive) {
				ps->isActive = 1;
				gf_node_event_out(sh->sensor, 3/*"isActive"*/);
				ps->enterTime = stack->last_time;
				gf_node_event_out(sh->sensor, 6/*"enterTime"*/);
			}
			return 1;
		}
	}
	/*either we're not over the shape or we're not in sensor*/
	if (ps->isActive) {
		ps->exitTime = stack->last_time;
		gf_node_event_out(sh->sensor, 7/*"exitTime"*/);
		ps->isActive = 0;
		gf_node_event_out(sh->sensor, 3/*"isActive"*/);
		return 1;
	}
	return 0;
}

static GF_SensorHandler *proximity_sensor2d_get_handler(GF_Node *n)
{
	Prox2DStack *st = (Prox2DStack *)gf_node_get_private(n);
	return &st->hdl;
}


void compositor_init_proximity_sensor2d(GF_Compositor *compositor, GF_Node *node)
{
	Prox2DStack *st;
	GF_SAFEALLOC(st, Prox2DStack);
	if (!st) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate proximity sensor 2d stack\n"));
		return;
	}

	st->hdl.IsEnabled = prox2D_is_enabled;
	st->hdl.OnUserEvent = OnProximitySensor2D;
	st->hdl.sensor = node;
	st->compositor = compositor;
	mpeg4_sensor_created(compositor, node);
	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, DestroyProximitySensor2D);
}

typedef struct
{
	GF_SensorHandler hdl;
	Bool mouse_down;
	GF_Compositor *compositor;
} TouchSensorStack;

static void DestroyTouchSensor(GF_Node *node, void *rs, Bool is_destroy)
{
	if (is_destroy) {
		TouchSensorStack *st = (TouchSensorStack *) gf_node_get_private(node);
		mpeg4_sensor_deleted(node, &st->hdl);
		gf_free(st);
	}
}

static Bool ts_is_enabled(GF_Node *n)
{
	return ((M_TouchSensor *) n)->enabled;
}

static Bool OnTouchSensor(GF_SensorHandler *sh, Bool is_over, Bool is_cancel, GF_Event *ev, GF_Compositor *compositor)
{
	Bool is_mouse = (ev->type <= GF_EVENT_LAST_MOUSE);
	M_TouchSensor *ts = (M_TouchSensor *)sh->sensor;

	/*this is not specified in VRML, however we consider that a de-enabled sensor will not sent deactivation events*/
	if (!ts->enabled) {
		if (ts->isActive) {
			sh->grabbed = 0;
		}
		return 0;
	}

	/*isActive becomes false, send touch time*/
	if (ts->isActive) {
		if (
		    /*mouse*/ ((ev->type==GF_EVENT_MOUSEUP) && (ev->mouse.button==GF_MOUSE_LEFT) )
		    || /*keyboard*/ ((ev->type==GF_EVENT_KEYUP) && (ev->key.key_code==GF_KEY_ENTER) )
		) {
			ts->touchTime = gf_node_get_scene_time(sh->sensor);
			if (!is_cancel) gf_node_event_out(sh->sensor, 6/*"touchTime"*/);
			ts->isActive = 0;
			if (!is_cancel) gf_node_event_out(sh->sensor, 4/*"isActive"*/);
			sh->grabbed = 0;
			return is_cancel ? 0 : 1;
		}
	}
	if (is_over != ts->isOver) {
		ts->isOver = is_over;
		if (!is_cancel) gf_node_event_out(sh->sensor, 5/*"isOver"*/);
		return is_cancel ? 0 : 1;
	}
	if (!ts->isActive && is_over) {
		if (/*mouse*/ ((ev->type==GF_EVENT_MOUSEDOWN) && (ev->mouse.button==GF_MOUSE_LEFT))
		              || /*keyboard*/ ((ev->type==GF_EVENT_KEYDOWN) && (ev->key.key_code==GF_KEY_ENTER))
		   ) {
			ts->isActive = 1;
			gf_node_event_out(sh->sensor, 4/*"isActive"*/);
			sh->grabbed = 1;
			return 1;
		}
		if (ev->type==GF_EVENT_MOUSEUP) return 0;
	}
	if (is_over && is_mouse && (ev->type==GF_EVENT_MOUSEMOVE) ) {
		/*THIS IS NOT CONFORMANT, the hitpoint should be in the touchsensor coordinate system, eg we
		should store the matrix from TS -> shape and apply that ...*/
		ts->hitPoint_changed = compositor->hit_local_point;
		gf_node_event_out(sh->sensor, 1/*"hitPoint_changed"*/);
		ts->hitNormal_changed = compositor->hit_normal;
		gf_node_event_out(sh->sensor, 2/*"hitNormal_changed"*/);
		ts->hitTexCoord_changed = compositor->hit_texcoords;
		gf_node_event_out(sh->sensor, 3/*"hitTexCoord_changed"*/);
		return 1;
	}
	return 0;
}

static GF_SensorHandler *touch_sensor_get_handler(GF_Node *n)
{
	TouchSensorStack *ts = (TouchSensorStack *)gf_node_get_private(n);
	return &ts->hdl;
}


void compositor_init_touch_sensor(GF_Compositor *compositor, GF_Node *node)
{
	TouchSensorStack *st;
	GF_SAFEALLOC(st, TouchSensorStack);
	if (!st) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate touch sensor stack\n"));
		return;
	}

	st->hdl.IsEnabled = ts_is_enabled;
	st->hdl.OnUserEvent = OnTouchSensor;
	st->hdl.sensor = node;
	st->compositor = compositor;
	mpeg4_sensor_created(compositor, node);
	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, DestroyTouchSensor);
}

#ifndef GPAC_DISABLE_3D

void TraverseProximitySensor(GF_Node *node, void *rs, Bool is_destroy)
{
	SFVec3f user_pos, dist, up;
	SFRotation ori;
	GF_Matrix mx;
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	M_ProximitySensor *ps = (M_ProximitySensor *)node;
	if (is_destroy) return;

	if (tr_state->traversing_mode==TRAVERSE_GET_BOUNDS) {
		/*work with twice bigger bbox to get sure we're notify when culled out*/
		gf_vec_add(tr_state->bbox.max_edge, ps->center, ps->size);
		gf_vec_diff(tr_state->bbox.min_edge, ps->center, ps->size);
		gf_bbox_refresh(&tr_state->bbox);
		return;
	} else if (!ps->enabled || (tr_state->traversing_mode != TRAVERSE_SORT) ) return;

	/*TODO FIXME - find a way to cache inverted matrix*/
	gf_mx_copy(mx, tr_state->model_matrix);
	gf_mx_inverse(&mx);
	/*get use pos in local coord system*/
	user_pos = tr_state->camera->position;
	gf_mx_apply_vec(&mx, &user_pos);
	gf_vec_diff(dist, user_pos, ps->center);

	if (dist.x<0) dist.x *= -1;
	if (dist.y<0) dist.y *= -1;
	if (dist.z<0) dist.z *= -1;

	if ((2*dist.x <= ps->size.x)
	        && (2*dist.y <= ps->size.y)
	        && (2*dist.z <= ps->size.z) ) {

		if (!ps->isActive) {
			ps->isActive = 1;
			gf_node_event_out(node, 3/*"isActive"*/);
			ps->enterTime = gf_node_get_scene_time(node);
			gf_node_event_out(node, 6/*"enterTime"*/);
		}
		if ((ps->position_changed.x != user_pos.x)
		        || (ps->position_changed.y != user_pos.y)
		        || (ps->position_changed.z != user_pos.z) )
		{
			ps->position_changed = user_pos;
			gf_node_event_out(node, 4/*"position_changed"*/);
		}
		dist = tr_state->camera->target;
		gf_mx_apply_vec(&mx, &dist);
		up = tr_state->camera->up;
		gf_mx_apply_vec(&mx, &up);
		ori = camera_get_orientation(user_pos, dist, tr_state->camera->up);
		if ((ori.q != ps->orientation_changed.q)
		        || (ori.x != ps->orientation_changed.x)
		        || (ori.y != ps->orientation_changed.y)
		        || (ori.z != ps->orientation_changed.z) ) {
			ps->orientation_changed = ori;
			gf_node_event_out(node, 5/*"orientation_changed"*/);
		}
	} else if (ps->isActive) {
		ps->isActive = 0;
		gf_node_event_out(node, 3/*"isActive"*/);
		ps->exitTime = gf_node_get_scene_time(node);
		gf_node_event_out(node, 7/*"exitTime"*/);
	}
}

void compositor_init_proximity_sensor(GF_Compositor *compositor, GF_Node *node)
{
	gf_node_set_callback_function(node, TraverseProximitySensor);
}


typedef struct
{
	SFVec3f start_drag;
	GF_Plane tracker;
	GF_Matrix initial_matrix;
	GF_Compositor *compositor;
	GF_SensorHandler hdl;
} PSStack;

static void DestroyPlaneSensor(GF_Node *node, void *rs, Bool is_destroy)
{
	if (is_destroy) {
		PSStack *st = (PSStack *) gf_node_get_private(node);
		mpeg4_sensor_deleted(node, &st->hdl);
		gf_free(st);
	}
}

static Bool ps_is_enabled(GF_Node *n)
{
	M_PlaneSensor *ps = (M_PlaneSensor *)n;
	return (ps->enabled || ps->isActive);
}

static Bool OnPlaneSensor(GF_SensorHandler *sh, Bool is_over, Bool is_cancel, GF_Event *ev, GF_Compositor *compositor)
{
	Bool is_mouse = (ev->type <= GF_EVENT_LAST_MOUSE) ? 1 : 0;
	M_PlaneSensor *ps = (M_PlaneSensor *)sh->sensor;
	PSStack *stack = (PSStack *) gf_node_get_private(sh->sensor);


	if (ps->isActive &&
	        ( /*mouse*/((ev->type==GF_EVENT_MOUSEUP) && (ev->mouse.button==GF_MOUSE_LEFT))
	                   || /*keyboar*/(!is_mouse && (!is_over|| ((ev->type==GF_EVENT_KEYDOWN) && (ev->key.key_code==GF_KEY_ENTER))) )
	        ) ) {
		if (ps->autoOffset) {
			ps->offset = ps->translation_changed;
			if (!is_cancel) gf_node_event_out(sh->sensor, 4/*"offset"*/);
		}
		ps->isActive = 0;
		if (!is_cancel) gf_node_event_out(sh->sensor, 5/*"isActive"*/);
		sh->grabbed = 0;
		return is_cancel ? 0 : 1;
	}
	/*mouse*/
	else if (is_mouse) {
		if (!ps->isActive && (ev->type==GF_EVENT_MOUSEDOWN) && (ev->mouse.button==GF_MOUSE_LEFT) ) {
			gf_mx_copy(stack->initial_matrix, compositor->hit_local_to_world);
			gf_vec_diff(stack->start_drag, compositor->hit_local_point, ps->offset);
			stack->tracker.normal.x = stack->tracker.normal.y = 0;
			stack->tracker.normal.z = FIX_ONE;
			stack->tracker.d = - gf_vec_dot(stack->start_drag, stack->tracker.normal);
			ps->isActive = 1;
			gf_node_event_out(sh->sensor, 5/*"isActive"*/);
			sh->grabbed = 1;
			return 1;
		}
		else if (ps->isActive) {
			GF_Ray loc_ray;
			SFVec3f res;
			loc_ray = compositor->hit_world_ray;
			gf_mx_apply_ray(&stack->initial_matrix, &loc_ray);
			gf_plane_intersect_line(&stack->tracker, &loc_ray.orig, &loc_ray.dir, &res);
			ps->trackPoint_changed = res;
			gf_node_event_out(sh->sensor, 6/*"trackPoint_changed"*/);

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
			gf_node_event_out(sh->sensor, 7/*"translation_changed"*/);
			return 1;
		}
	} else {
		if (!ps->isActive && is_over && (ev->type==GF_EVENT_KEYDOWN) && (ev->key.key_code==GF_KEY_ENTER)) {
			ps->isActive = 1;
			stack->start_drag = ps->offset;
			gf_node_event_out(sh->sensor, 5/*"isActive"*/);
			return 1;
		}
		else if (ps->isActive && (ev->type==GF_EVENT_KEYDOWN)) {
			SFVec3f res;
			Fixed diff = (ev->key.flags & GF_KEY_MOD_SHIFT) ? 5*FIX_ONE : FIX_ONE;
			if (!gf_sg_use_pixel_metrics(gf_node_get_graph(sh->sensor)))
				diff = gf_divfix(diff, INT2FIX(compositor->vp_width/2));

			res = stack->start_drag;
			switch (ev->key.key_code) {
			case GF_KEY_LEFT:
				res.x -= diff;
				break;
			case GF_KEY_RIGHT:
				res.x += diff;
				break;
			case GF_KEY_UP:
				res.y += diff;
				break;
			case GF_KEY_DOWN:
				res.y -= diff;
				break;
			case GF_KEY_HOME:
				res = ps->offset;
				break;
			default:
				return 0;
			}
			/*clip*/
			if (ps->minPosition.x <= ps->maxPosition.x) {
				if (res.x < ps->minPosition.x) res.x = ps->minPosition.x;
				if (res.x > ps->maxPosition.x) res.x = ps->maxPosition.x;
			}
			if (ps->minPosition.y <= ps->maxPosition.y) {
				if (res.y < ps->minPosition.y) res.y = ps->minPosition.y;
				if (res.y > ps->maxPosition.y) res.y = ps->maxPosition.y;
			}
			stack->start_drag = res;
			ps->translation_changed = res;
			gf_node_event_out(sh->sensor, 7/*"translation_changed"*/);
			return 1;
		}
	}
	return 0;
}

static GF_SensorHandler *plane_sensor_get_handler(GF_Node *n)
{
	PSStack *st = (PSStack *)gf_node_get_private(n);
	return &st->hdl;
}

void compositor_init_plane_sensor(GF_Compositor *compositor, GF_Node *node)
{
	PSStack *st;
	GF_SAFEALLOC(st, PSStack);
	if (!st) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate plane sensor stack\n"));
		return;
	}

	st->hdl.IsEnabled = ps_is_enabled;
	st->hdl.OnUserEvent = OnPlaneSensor;
	st->hdl.sensor = node;
	st->compositor = compositor;
	mpeg4_sensor_created(compositor, node);
	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, DestroyPlaneSensor);
}

typedef struct
{
	GF_SensorHandler hdl;
	GF_Compositor *compositor;
	GF_Matrix init_matrix;
	Bool disk_mode;
	SFVec3f grab_start;
	GF_Plane yplane, zplane, xplane;
} CylinderSensorStack;

static void DestroyCylinderSensor(GF_Node *node, void *rs, Bool is_destroy)
{
	if (is_destroy) {
		CylinderSensorStack *st = (CylinderSensorStack *) gf_node_get_private(node);
		mpeg4_sensor_deleted(node, &st->hdl);
		gf_free(st);
	}
}

static Bool cs_is_enabled(GF_Node *n)
{
	M_CylinderSensor *cs = (M_CylinderSensor *)n;
	return (cs->enabled || cs->isActive);
}

static Bool OnCylinderSensor(GF_SensorHandler *sh, Bool is_over, Bool is_cancel, GF_Event *ev, GF_Compositor *compositor)
{
	Bool is_mouse = (ev->type <= GF_EVENT_LAST_MOUSE) ? 1 : 0;
	M_CylinderSensor *cs = (M_CylinderSensor *)sh->sensor;
	CylinderSensorStack *st = (CylinderSensorStack *) gf_node_get_private(sh->sensor);

	if (cs->isActive && (!cs->enabled
	                     || /*mouse*/((ev->type==GF_EVENT_MOUSEUP) && (ev->mouse.button==GF_MOUSE_LEFT))
	                     || /*keyboar*/(!is_mouse && (!is_over|| ((ev->type==GF_EVENT_KEYDOWN) && (ev->key.key_code==GF_KEY_ENTER))))
	                    ) ) {
		if (cs->autoOffset) {
			cs->offset = cs->rotation_changed.q;
			if (!is_cancel) gf_node_event_out(sh->sensor, 5/*"offset"*/);
		}
		cs->isActive = 0;
		if (!is_cancel) gf_node_event_out(sh->sensor, 6/*"isActive"*/);
		sh->grabbed = 0;
		return is_cancel ? 0 : 1;
	}
	else if (is_mouse) {
		if (!cs->isActive && (ev->type==GF_EVENT_MOUSEDOWN) && (ev->mouse.button==GF_MOUSE_LEFT)) {
			GF_Ray r;
			SFVec3f yaxis;
			Fixed acute, reva;
			SFVec3f bearing;

			gf_mx_copy(st->init_matrix, compositor->hit_world_to_local);
			/*get initial angle & check disk mode*/
			r = compositor->hit_world_ray;
			gf_vec_add(r.dir, r.orig, r.dir);
			gf_mx_apply_vec(&compositor->hit_world_to_local, &r.orig);
			gf_mx_apply_vec(&compositor->hit_world_to_local, &r.dir);
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

			st->grab_start = compositor->hit_local_point;
			/*cos we're lazy*/
			st->yplane.d = 0;
			st->yplane.normal.x = st->yplane.normal.z = st->yplane.normal.y = 0;
			st->zplane = st->xplane = st->yplane;
			st->xplane.normal.x = FIX_ONE;
			st->yplane.normal.y = FIX_ONE;
			st->zplane.normal.z = FIX_ONE;

			cs->rotation_changed.x = 0;
			cs->rotation_changed.y = FIX_ONE;
			cs->rotation_changed.z = 0;

			cs->isActive = 1;
			gf_node_event_out(sh->sensor, 6/*"isActive"*/);
			sh->grabbed = 1;
			return 1;
		}
		else if (cs->isActive) {
			GF_Ray r;
			Fixed radius, rot;
			SFVec3f dir1, dir2, cx;

			if (is_over) {
				cs->trackPoint_changed = compositor->hit_local_point;
				gf_node_event_out(sh->sensor, 8/*"trackPoint_changed"*/);
			} else {
				GF_Plane project_to;
				r = compositor->hit_world_ray;
				gf_mx_apply_ray(&st->init_matrix, &r);

				/*no intersection, use intersection with "main" fronting plane*/
				if ( ABS(r.dir.z) > ABS(r.dir.y)) {
					if (ABS(r.dir.z) > ABS(r.dir.x)) project_to = st->xplane;
					else project_to = st->yplane;
				} else {
					if (ABS(r.dir.z) > ABS(r.dir.x)) project_to = st->xplane;
					else project_to = st->zplane;
				}
				if (!gf_plane_intersect_line(&project_to, &r.orig, &r.dir, &compositor->hit_local_point)) return 0;
			}

			dir1.x = compositor->hit_local_point.x;
			dir1.y = 0;
			dir1.z = compositor->hit_local_point.z;
			if (st->disk_mode) {
				radius = FIX_ONE;
			} else {
				radius = gf_vec_len(dir1);
			}
			gf_vec_norm(&dir1);
			dir2.x = st->grab_start.x;
			dir2.y = 0;
			dir2.z = st->grab_start.z;
			gf_vec_norm(&dir2);
			cx = gf_vec_cross(dir2, dir1);
			gf_vec_norm(&cx);
			if (gf_vec_len(cx)<FIX_EPSILON) return 0;
			rot = gf_mulfix(radius, gf_acos(gf_vec_dot(dir2, dir1)) );
			if (fabs(cx.y + FIX_ONE) < FIX_EPSILON) rot = -rot;
			if (cs->autoOffset) rot += cs->offset;

			if (cs->minAngle < cs->maxAngle) {
				if (rot < cs->minAngle) rot = cs->minAngle;
				else if (rot > cs->maxAngle) rot = cs->maxAngle;
			}
			cs->rotation_changed.q = rot;
			gf_node_event_out(sh->sensor, 7/*"rotation_changed"*/);
			return 1;
		}
	} else {
		if (!cs->isActive && is_over && (ev->type==GF_EVENT_KEYDOWN) && (ev->key.key_code==GF_KEY_ENTER)) {
			cs->isActive = 1;
			cs->rotation_changed.q = cs->offset;
			cs->rotation_changed.x = cs->rotation_changed.z = 0;
			cs->rotation_changed.y = FIX_ONE;
			gf_node_event_out(sh->sensor, 6/*"isActive"*/);
			return 1;
		}
		else if (cs->isActive && (ev->type==GF_EVENT_KEYDOWN)) {
			SFFloat res;
			Fixed diff = (ev->key.flags & GF_KEY_MOD_SHIFT) ? GF_PI/8 : GF_PI/64;

			res = cs->rotation_changed.q;
			switch (ev->key.key_code) {
			case GF_KEY_LEFT:
				res -= diff;
				break;
			case GF_KEY_RIGHT:
				res += diff;
				break;
			case GF_KEY_HOME:
				res = cs->offset;
				break;
			default:
				return 0;
			}
			/*clip*/
			if (cs->minAngle <= cs->maxAngle) {
				if (res < cs->minAngle) res = cs->minAngle;
				if (res > cs->maxAngle) res = cs->maxAngle;
			}
			cs->rotation_changed.q = res;
			gf_node_event_out(sh->sensor, 7/*"rotation_changed"*/);
			return 1;
		}
	}
	return 0;
}

static GF_SensorHandler *cylinder_sensor_get_handler(GF_Node *n)
{
	CylinderSensorStack *st = (CylinderSensorStack  *)gf_node_get_private(n);
	return &st->hdl;
}

void compositor_init_cylinder_sensor(GF_Compositor *compositor, GF_Node *node)
{
	CylinderSensorStack *st;
	GF_SAFEALLOC(st, CylinderSensorStack);
	if (!st) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate cylinder sensor 2d stack\n"));
		return;
	}

	st->hdl.IsEnabled = cs_is_enabled;
	st->hdl.OnUserEvent = OnCylinderSensor;
	st->hdl.sensor = node;
	st->compositor = compositor;
	mpeg4_sensor_created(compositor, node);
	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, DestroyCylinderSensor);
}


typedef struct
{
	GF_SensorHandler hdl;
	GF_Compositor *compositor;
	Fixed radius;
	/*center in world coords */
	SFVec3f grab_vec, center;
} SphereSensorStack;

static void DestroySphereSensor(GF_Node *node, void *rs, Bool is_destroy)
{
	if (is_destroy) {
		SphereSensorStack *st = (SphereSensorStack *) gf_node_get_private(node);
		mpeg4_sensor_deleted(node, &st->hdl);
		gf_free(st);
	}
}

static Bool sphere_is_enabled(GF_Node *n)
{
	M_SphereSensor *ss = (M_SphereSensor *)n;
	return (ss->enabled || ss->isActive);
}

static Bool OnSphereSensor(GF_SensorHandler *sh, Bool is_over, Bool is_cancel, GF_Event *ev, GF_Compositor *compositor)
{
	Bool is_mouse = (ev->type <= GF_EVENT_LAST_MOUSE) ? 1 : 0;
	M_SphereSensor *sphere = (M_SphereSensor *)sh->sensor;
	SphereSensorStack *st = (SphereSensorStack *) gf_node_get_private(sh->sensor);


	if (sphere->isActive && (!sphere->enabled
	                         || /*mouse*/((ev->type==GF_EVENT_MOUSEUP) && (ev->mouse.button==GF_MOUSE_LEFT))
	                         || /*keyboar*/(!is_mouse && (!is_over|| ((ev->type==GF_EVENT_KEYDOWN) && (ev->key.key_code==GF_KEY_ENTER))))
	                        ) ) {
		if (sphere->autoOffset) {
			sphere->offset = sphere->rotation_changed;
			if (!is_cancel) gf_node_event_out(sh->sensor, 2/*"offset"*/);
		}
		sphere->isActive = 0;
		if (!is_cancel) gf_node_event_out(sh->sensor, 3/*"isActive"*/);
		sh->grabbed = 0;
		return is_cancel ? 0 : 1;
	}
	else if (is_mouse) {
		if (!sphere->isActive && (ev->type==GF_EVENT_MOUSEDOWN) && (ev->mouse.button==GF_MOUSE_LEFT)) {
			st->center.x = st->center.y = st->center.z = 0;
			gf_mx_apply_vec(&compositor->hit_local_to_world, &st->center);
			st->radius = gf_vec_len(compositor->hit_local_point);
			if (!st->radius) st->radius = FIX_ONE;
			st->grab_vec = gf_vec_scale(compositor->hit_local_point, gf_invfix(st->radius));

			sphere->isActive = 1;
			gf_node_event_out(sh->sensor, 3/*"isActive"*/);
			sh->grabbed = 1;
			return 1;
		}
		else if (sphere->isActive) {
			SFVec3f vec, axis;
			SFVec4f q1, q2;
			SFRotation r;
			Fixed cl;
			if (is_over) {
				sphere->trackPoint_changed = compositor->hit_local_point;
				gf_node_event_out(sh->sensor, 5/*"trackPoint_changed"*/);
			} else {
				GF_Ray ray = compositor->hit_world_ray;
				gf_mx_apply_ray(&compositor->hit_world_to_local, &ray);
				if (!gf_ray_hit_sphere(&ray, NULL, st->radius, &compositor->hit_local_point)) {
					vec.x = vec.y = vec.z = 0;
					/*doesn't work properly...*/
					compositor->hit_local_point = gf_closest_point_to_line(ray.orig, ray.dir, vec);
				}
			}

			vec = gf_vec_scale(compositor->hit_local_point, gf_invfix(st->radius));
			axis = gf_vec_cross(st->grab_vec, vec);
			cl = gf_vec_len(axis);

			if (cl < -FIX_ONE) cl = -FIX_ONE;
			else if (cl > FIX_ONE) cl = FIX_ONE;
			r.q = gf_asin(cl);
			if (gf_vec_dot(st->grab_vec, vec) < 0) r.q += GF_PI / 2;

			gf_vec_norm(&axis);
			r.x = axis.x;
			r.y = axis.y;
			r.z = axis.z;
			q1 = gf_quat_from_rotation(r);
			if (sphere->autoOffset) {
				q2 = gf_quat_from_rotation(sphere->offset);
				q1 = gf_quat_multiply(&q1, &q2);
			}
			sphere->rotation_changed = gf_quat_to_rotation(&q1);
			gf_node_event_out(sh->sensor, 4/*"rotation_changed"*/);
			return 1;
		}
	} else {
		if (!sphere->isActive && is_over && (ev->type==GF_EVENT_KEYDOWN) && (ev->key.key_code==GF_KEY_ENTER)) {
			sphere->isActive = 1;
			sphere->rotation_changed = sphere->offset;
			gf_node_event_out(sh->sensor, 3/*"isActive"*/);
			return 1;
		}
		else if (sphere->isActive && (ev->type==GF_EVENT_KEYDOWN)) {
			SFVec4f res, rot;
			Fixed diff = GF_PI/64;

			res = sphere->rotation_changed;
			switch (ev->key.key_code) {
			case GF_KEY_LEFT:
				diff = -diff;
			case GF_KEY_RIGHT:
				rot.x = 0;
				rot.y = FIX_ONE;
				rot.z = 0;
				rot.q = diff;
				res = gf_quat_from_rotation(res);
				rot = gf_quat_from_rotation(rot);
				rot = gf_quat_multiply(&rot, &res);
				res = gf_quat_to_rotation(&rot);
				break;
			case GF_KEY_DOWN:
				diff = -diff;
			case GF_KEY_UP:
				if (ev->key.flags & GF_KEY_MOD_SHIFT) {
					rot.x = 0;
					rot.z = FIX_ONE;
				} else {
					rot.x = FIX_ONE;
					rot.z = 0;
				}
				rot.y = 0;
				rot.q = diff;
				res = gf_quat_from_rotation(res);
				rot = gf_quat_from_rotation(rot);
				rot = gf_quat_multiply(&rot, &res);
				res = gf_quat_to_rotation(&rot);
				break;
			case GF_KEY_HOME:
				res = sphere->offset;
				break;
			default:
				return 0;
			}
			sphere->rotation_changed = res;
			gf_node_event_out(sh->sensor, 4/*"rotation_changed"*/);
			return 1;
		}
	}
	return 0;
}

static GF_SensorHandler *sphere_get_handler(GF_Node *n)
{
	SphereSensorStack *st = (SphereSensorStack *)gf_node_get_private(n);
	return &st->hdl;
}

void compositor_init_sphere_sensor(GF_Compositor *compositor, GF_Node *node)
{
	SphereSensorStack *st;
	GF_SAFEALLOC(st, SphereSensorStack);
	if (!st) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate sphere sensor 2d stack\n"));
		return;
	}

	st->hdl.IsEnabled = sphere_is_enabled;
	st->hdl.OnUserEvent = OnSphereSensor;
	st->hdl.sensor = node;
	st->compositor = compositor;
	mpeg4_sensor_created(compositor, node);
	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, DestroySphereSensor);
}

void TraverseVisibilitySensor(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	M_VisibilitySensor *vs = (M_VisibilitySensor *)node;

	if (is_destroy || !vs->enabled) return;

	if (tr_state->traversing_mode==TRAVERSE_GET_BOUNDS) {
		/*work with twice bigger bbox to get sure we're notify when culled out*/
		gf_vec_add(tr_state->bbox.max_edge, vs->center, vs->size);
		gf_vec_diff(tr_state->bbox.min_edge, vs->center, vs->size);
		gf_bbox_refresh(&tr_state->bbox);

	} else if (tr_state->traversing_mode==TRAVERSE_SORT) {
		Bool visible;
		u32 cull_flag;
		GF_BBox bbox;
		SFVec3f s;
		s = gf_vec_scale(vs->size, FIX_ONE/2);
		/*cull with normal bbox*/
		gf_vec_add(bbox.max_edge, vs->center, s);
		gf_vec_diff(bbox.min_edge, vs->center, s);
		gf_bbox_refresh(&bbox);
		cull_flag = tr_state->cull_flag;
		tr_state->cull_flag = CULL_INTERSECTS;
		visible = visual_3d_node_cull(tr_state, &bbox, 0);
		tr_state->cull_flag = cull_flag;

		if (visible && !vs->isActive) {
			vs->isActive = 1;
			gf_node_event_out(node, 5/*"isActive"*/);
			vs->enterTime = gf_node_get_scene_time(node);
			gf_node_event_out(node, 3/*"enterTime"*/);
		}
		else if (!visible && vs->isActive) {
			vs->isActive = 0;
			gf_node_event_out(node, 5/*"isActive"*/);
			vs->exitTime = gf_node_get_scene_time(node);
			gf_node_event_out(node, 4/*"exitTime"*/);
		}
	}
}

void compositor_init_visibility_sensor(GF_Compositor *compositor, GF_Node *node)
{
	gf_node_set_callback_function(node, TraverseVisibilitySensor);
}

#endif

GF_SensorHandler *compositor_mpeg4_get_sensor_handler(GF_Node *n)
{
	return compositor_mpeg4_get_sensor_handler_ex(n, GF_FALSE);
}
GF_SensorHandler *compositor_mpeg4_get_sensor_handler_ex(GF_Node *n, Bool skip_anchors)
{
	GF_SensorHandler *hs;

	switch (gf_node_get_tag(n)) {
	/*anchor is not considered as a child sensor node when picking sensors*/
	case TAG_MPEG4_Anchor:
		if (skip_anchors) return NULL;
		hs = gf_sc_anchor_get_handler(n);
		break;
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Anchor:
		if (skip_anchors) return NULL;
		hs = gf_sc_anchor_get_handler(n);
		break;
#endif
	case TAG_MPEG4_DiscSensor:
		hs = disc_sensor_get_handler(n);
		break;
	case TAG_MPEG4_PlaneSensor2D:
		hs = plane_sensor2d_get_handler(n);
		break;
	case TAG_MPEG4_ProximitySensor2D:
		hs = proximity_sensor2d_get_handler(n);
		break;
	case TAG_MPEG4_TouchSensor:
		hs = touch_sensor_get_handler(n);
		break;
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_TouchSensor:
		hs = touch_sensor_get_handler(n);
		break;
#endif
#ifndef GPAC_DISABLE_3D
	case TAG_MPEG4_CylinderSensor:
		hs = cylinder_sensor_get_handler(n);
		break;
	case TAG_MPEG4_PlaneSensor:
		hs = plane_sensor_get_handler(n);
		break;
	case TAG_MPEG4_SphereSensor:
		hs = sphere_get_handler(n);
		break;
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_CylinderSensor:
		hs = cylinder_sensor_get_handler(n);
		break;
	case TAG_X3D_PlaneSensor:
		hs = plane_sensor_get_handler(n);
		break;
	case TAG_X3D_SphereSensor:
		hs = sphere_get_handler(n);
		break;
#endif
#endif /*GPAC_DISABLE_3D*/
	default:
		return NULL;
	}
	if (hs && hs->IsEnabled(n)) return hs;
	return NULL;
}

Bool compositor_mpeg4_is_sensor_node(GF_Node *node)
{
	GF_SensorHandler *sh = compositor_mpeg4_get_sensor_handler(node);
	if (sh && sh->IsEnabled(node)) return 1;
	return 0;
}

static void traverse_envtest(GF_Node *node, void *rs, Bool is_destroy)
{
	if (is_destroy) {
		GF_Compositor *compositor = gf_node_get_private(node);
		gf_list_del_item(compositor->env_tests, node);
	}
}

void envtest_evaluate(GF_Node *node, GF_Route *_route)
{
	Bool smaller, larger, equal;
	Float ar, arft;
	u32 par;
	char par_value[50];
	const char *opt;
	M_EnvironmentTest *envtest = (M_EnvironmentTest *)node;
	GF_Compositor *compositor = (GF_Compositor *)gf_node_get_private(node);

	if (envtest->parameterValue.buffer) gf_free(envtest->parameterValue.buffer);
	envtest->parameterValue.buffer=NULL;

	smaller = larger = equal = 0;
	switch (envtest->parameter) {
	/*screen aspect ratio*/
	case 0:
		if (compositor->display_width>compositor->display_height) {
			ar = (Float) compositor->display_width;
			ar /= compositor->display_height;
		} else {
			ar = (Float) compositor->display_height;
			ar /= compositor->display_width;
		}
		if (envtest->compareValue.buffer && (sscanf(envtest->compareValue.buffer, "%f", &arft)==1)) {
			if (ar==arft) equal=1;
			else if (ar>arft) smaller=1;
			else larger=1;
		}
		sprintf(par_value, "%f", ar);
		break;
	/*screen is portrait */
	case 1:
		equal = (compositor->display_width < compositor->display_height) ? 1 : 2;
		strcpy(par_value, (equal==1) ? "TRUE" : "FALSE");
		break;
	/*screen width */
	case 2:
		if (envtest->compareValue.buffer && (sscanf(envtest->compareValue.buffer, "%u", &par)==1)) {
			if (compositor->display_width==par) equal=1;
			else if (compositor->display_width>par) smaller=1;
			else larger=1;
		}
		sprintf(par_value, "%u", compositor->display_width);
		break;
	/*screen width */
	case 3:
		if (envtest->compareValue.buffer && (sscanf(envtest->compareValue.buffer, "%u", &par)==1)) {
			if (compositor->display_height==par) equal=1;
			else if (compositor->display_height>par) smaller=1;
			else larger=1;
		}
		sprintf(par_value, "%u", compositor->display_height);
		break;
	/*screen dpi horizontal */
	case 4:
		if (envtest->compareValue.buffer && (sscanf(envtest->compareValue.buffer, "%u", &par)==1)) {
			if (compositor->video_out->dpi_x==par) equal=1;
			else if (compositor->video_out->dpi_x>par) smaller=1;
			else larger=1;
		}
		sprintf(par_value, "%u", compositor->video_out->dpi_x);
		break;
	/*screen dpi vertical*/
	case 5:
		if (envtest->compareValue.buffer && (sscanf(envtest->compareValue.buffer, "%u", &par)==1)) {
			if (compositor->video_out->dpi_y==par) equal=1;
			else if (compositor->video_out->dpi_y>par) smaller=1;
			else larger=1;
		}
		sprintf(par_value, "%u", compositor->video_out->dpi_y);
		break;
	/*automotive situation - fixme we should use a profile doc ?*/
	case 6:
		opt = gf_opts_get_key("Profile", "Automotive");
		equal = (opt && !strcmp(opt, "yes")) ? 1 : 2;
		strcpy(par_value, (equal==1) ? "TRUE" : "FALSE");
		break;
	/*visually challenged - fixme we should use a profile doc ?*/
	case 7:
		opt = gf_opts_get_key("Profile", "VisuallyChallenged");
		equal = (opt && !strcmp(opt, "yes")) ? 1 : 2;
		strcpy(par_value, (equal==1) ? "TRUE" : "FALSE");
		break;
	/*has touch - fixme we should find out by ourselves*/
	case 8:
		opt = gf_opts_get_key("Profile", "HasTouchScreen");
		equal = (!opt || !strcmp(opt, "yes")) ? 1 : 2;
		strcpy(par_value, (equal==1) ? "TRUE" : "FALSE");
		break;
	/*has key - fixme we should find out by ourselves*/
	case 9:
		opt = gf_opts_get_key("Profile", "HasKeyPad");
		equal = (!opt || !strcmp(opt, "yes")) ? 1 : 2;
		strcpy(par_value, (equal==1) ? "TRUE" : "FALSE");
		break;
	}

	if (equal) {
		envtest->valueEqual=(equal==1) ? 1 : 0;
		gf_node_event_out(node, 6/*"valueEqual"*/);
	}
	else if (smaller) {
		envtest->valueSmaller=1;
		gf_node_event_out(node, 7/*"valueSmaller"*/);
	}
	else if (larger) {
		envtest->valueLarger=1;
		gf_node_event_out(node, 5/*"valueLarger"*/);
	}
	envtest->parameterValue.buffer = gf_strdup(par_value);
	gf_node_event_out(node, 8/*"parameterValue"*/);
}

void compositor_evaluate_envtests(GF_Compositor *compositor, u32 param_type)
{
	u32 i, count;
	count = gf_list_count(compositor->env_tests);
	for (i=0; i<count; i++) {
		GF_Node *envtest = gf_list_get(compositor->env_tests, i);
		if (!((M_EnvironmentTest *)envtest)->evaluateOnChange) continue;

		switch (((M_EnvironmentTest *)envtest)->parameter) {
		/*screen-size related*/
		case 0:
		case 1:
		case 2:
		case 3:
			if (param_type==0) envtest_evaluate(envtest, NULL);
			break;
		/*DPI related*/
		case 4:
		case 5:
			if (param_type==1) envtest_evaluate(envtest, NULL);
			break;
		/*automotive situation*/
		case 6:
			if (param_type==2) envtest_evaluate(envtest, NULL);
			break;
			/*the rest are static events*/
		}
	}
}

void compositor_envtest_modified(GF_Node *node)
{
	envtest_evaluate(node, NULL);
}

void compositor_init_envtest(GF_Compositor *compositor, GF_Node *node)
{
	M_EnvironmentTest *envtest = (M_EnvironmentTest *)node;
	gf_list_add(compositor->env_tests, node);
	gf_node_set_private(node, compositor);
	gf_node_set_callback_function(node, traverse_envtest);

	envtest->on_evaluate = envtest_evaluate;

#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_cov_mode()) {
		compositor_envtest_modified(node);
	}
#endif
}



#endif /*GPAC_DISABLE_VRML*/
