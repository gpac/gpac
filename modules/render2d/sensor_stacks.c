/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / 2D rendering module
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



#include "stacks2d.h"

static void unregister_sensor(GF_Renderer *comp, SensorHandler *hdl)
{
	R2D_UnregisterSensor(comp, hdl);
	if (comp->interaction_sensors) comp->interaction_sensors--;
}


typedef struct 
{
	SensorHandler hdl;
	Bool mouse_down;
	Fixed start_angle;
	Bool last_not_over;
	GF_Matrix2D inv_init_matrix;
	GF_Renderer *compositor;
} DiscSensorStack;

static void DestroyDiscSensor(GF_Node *node)
{
	DiscSensorStack *st = (DiscSensorStack *) gf_node_get_private(node);
	unregister_sensor(st->compositor, &st->hdl);
	free(st);
}

static Bool ds_is_enabled(SensorHandler *sh)
{
	return ((M_DiscSensor *) sh->owner)->enabled;
}

static Fixed ds_point_to_angle(DiscSensorStack *st, M_DiscSensor *ds, Fixed X, Fixed Y)
{
	Fixed result = gf_atan2(X, Y) - st->start_angle + ds->offset;
	if (ds->minAngle < ds->maxAngle) {
		/*FIXME this doesn't work properly*/
		if (result < ds->minAngle) result = ds->minAngle;
		if (result > ds->maxAngle) result = ds->maxAngle;
	}
	return result;
}

static void OnDiscSensor(SensorHandler *sh, UserEvent2D *ev, GF_Matrix2D *sensor_matrix)
{
	Fixed X, Y;
	M_DiscSensor *ds = (M_DiscSensor *)sh->owner;
	DiscSensorStack *stack = (DiscSensorStack *) gf_node_get_private(sh->owner);
	Render2D *sr = (Render2D *)stack->compositor->visual_renderer->user_priv;
	
	if (! ds->enabled) return;
	
	if (ev->context == NULL) {
		if (stack->last_not_over) {
			if (ev->event_type == GF_EVT_LEFTUP) {
				R2D_UnregisterSensor(stack->compositor, &stack->hdl);
				sr->is_tracking = 0;
				stack->mouse_down = 0;
				if (ds->isActive) {
					ds->isActive = 0;
					gf_node_event_out_str(sh->owner, "isActive");
				}
			}
			if (!stack->mouse_down)
				R2D_UnregisterSensor(stack->compositor, &stack->hdl);
			return;
		}
		stack->last_not_over = 1;
		return;
	}

	stack->last_not_over = 0;
	X = ev->x;
	Y = ev->y;

	if ((ev->event_type == GF_EVT_LEFTDOWN) && !stack->mouse_down) {
		stack->mouse_down = 1;
		/*store inverse of initial matrix*/
		gf_mx2d_copy(stack->inv_init_matrix, *sensor_matrix);
		gf_mx2d_inverse(&stack->inv_init_matrix);
		gf_mx2d_apply_coords(&stack->inv_init_matrix, &X, &Y);
		stack->start_angle = gf_atan2(X, Y);
		ds->isActive = 1;
		gf_node_event_out_str(sh->owner, "isActive");
		R2D_RegisterSensor(stack->compositor, &stack->hdl);	
	}
	else if ((ev->event_type == GF_EVT_LEFTUP) && stack->mouse_down) {
		R2D_UnregisterSensor(stack->compositor, &stack->hdl);
		sr->is_tracking = 0;
		stack->mouse_down = 0;
		
		gf_mx2d_apply_coords(&stack->inv_init_matrix, &X, &Y);
		if (ds->autoOffset) {
			ds->offset = ds->rotation_changed;
			/*that's an exposedField*/
			gf_node_event_out_str(sh->owner, "offset");
		}

		ds->isActive = 0;
		gf_node_event_out_str(sh->owner, "isActive");
		gf_mx2d_init(stack->inv_init_matrix);
	}
	else if ((ev->event_type == GF_EVT_MOUSEMOVE) && stack->mouse_down) {
		sr->is_tracking = 1;
		gf_mx2d_apply_coords(&stack->inv_init_matrix, &X, &Y);
		ds->rotation_changed = ds_point_to_angle(stack, ds, X, Y);
		gf_node_event_out_str(sh->owner, "rotation_changed");
	   	ds->trackPoint_changed.x = X;
	   	ds->trackPoint_changed.y = Y;
		gf_node_event_out_str(sh->owner, "trackPoint_changed");
	}
}

SensorHandler *r2d_ds_get_handler(GF_Node *n)
{
	DiscSensorStack *st = (DiscSensorStack *)gf_node_get_private(n);
	return &st->hdl;
}

void R2D_InitDiscSensor(Render2D *sr, GF_Node *node)
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
	SensorHandler hdl;
	Bool mouse_down;
	SFVec2f start_drag;
	Bool last_not_over;
	GF_Matrix2D init_matrix;
	GF_Renderer *compositor;
} PS2DStack;

static void DestroyPlaneSensor2D(GF_Node *node)
{
	PS2DStack *st = (PS2DStack *) gf_node_get_private(node);
	unregister_sensor(st->compositor, &st->hdl);
	free(st);
}

static Bool ps2D_is_enabled(SensorHandler *sh)
{
	return ((M_PlaneSensor2D *) sh->owner)->enabled;
}

static SFVec2f get_translation(PS2DStack *stack, M_PlaneSensor2D *ps, Fixed X, Fixed Y, GF_Matrix2D *inversed_sensor_matrix)
{
	SFVec2f res;
	Fixed a, b;

	a = stack->start_drag.x;
	b = stack->start_drag.y;
	gf_mx2d_apply_coords(&stack->init_matrix, &a, &b);
	gf_mx2d_apply_coords(inversed_sensor_matrix, &a, &b);

	res.x = X - a + ps->offset.x;
	res.y = Y - b + ps->offset.y;
	/*clip*/
	if (ps->minPosition.x <= ps->maxPosition.x) {
		if (res.x < ps->minPosition.x) res.x = ps->minPosition.x;
		if (res.x > ps->maxPosition.x) res.x = ps->maxPosition.x;
	}
	if (ps->minPosition.y <= ps->maxPosition.y) {
		if (res.y < ps->minPosition.y) res.y = ps->minPosition.y;
		if (res.y > ps->maxPosition.y) res.y = ps->maxPosition.y;
	}
	return res;
}

static void OnPlaneSensor2D(SensorHandler *sh, UserEvent2D *ev, GF_Matrix2D *sensor_matrix)
{
	Fixed X, Y;
	GF_Matrix2D inv;
	M_PlaneSensor2D *ps = (M_PlaneSensor2D *)sh->owner;
	PS2DStack *stack = (PS2DStack *) gf_node_get_private(sh->owner);
	Render2D *sr = (Render2D *)stack->compositor->visual_renderer->user_priv;
	
	if (!ps->enabled) return;

	// this function is called when the mouse is not on a object
	if (ev->context == NULL) {
		if (stack->last_not_over) {
			if (ev->event_type == GF_EVT_LEFTUP) {
				sr->is_tracking = 0;
				stack->mouse_down = 0;
				if (ps->isActive) {
					ps->isActive = 0;
					gf_node_event_out_str(sh->owner, "isActive");
				}

			}
			if (!stack->mouse_down) {
				R2D_UnregisterSensor(stack->compositor, &stack->hdl);
			}

			return;
		}
		stack->last_not_over = 1;
		return;
	}


	stack->last_not_over = 0;
	gf_mx2d_copy(inv, *sensor_matrix);
	gf_mx2d_inverse(&inv);
	X = ev->x;
	Y = ev->y;
	gf_mx2d_apply_coords(&inv, &X, &Y);

	if ((ev->event_type == GF_EVT_LEFTDOWN) && !stack->mouse_down) {
		stack->mouse_down = 1;
		gf_mx2d_copy(stack->init_matrix, *sensor_matrix);
		stack->start_drag.x = X;
		stack->start_drag.y = Y;
		R2D_RegisterSensor(stack->compositor, &stack->hdl);
		ps->isActive = 1;
		gf_node_event_out_str(sh->owner, "isActive");
	}

	if (ev->event_type == GF_EVT_LEFTUP && stack->mouse_down) {
		R2D_UnregisterSensor(stack->compositor, &stack->hdl);
		sr->is_tracking = 0;
		stack->mouse_down = 0;
		if (ps->isActive) {
			ps->isActive = 0;
			gf_node_event_out_str(sh->owner, "isActive");
		}
		
		if (ps->autoOffset) {
			ps->offset = get_translation(stack, ps, X, Y, &inv);
			gf_node_event_out_str(sh->owner, "offset");
		}

		return;
	}

	if ( (ev->event_type == GF_EVT_MOUSEMOVE) && stack->mouse_down) {
		sr->is_tracking = 1;
		ps->trackPoint_changed.x = X;
		ps->trackPoint_changed.y = Y;
		gf_node_event_out_str(sh->owner, "trackPoint_changed");
		
		ps->translation_changed = get_translation(stack, ps, X, Y, &inv);
		gf_node_event_out_str(sh->owner, "translation_changed");
	}
	return;
}

SensorHandler *r2d_ps2D_get_handler(GF_Node *n)
{
	PS2DStack *st = (PS2DStack *)gf_node_get_private(n);
	return &st->hdl;
}

void R2D_InitPlaneSensor2D(Render2D *sr, GF_Node *node)
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
	SensorHandler hdl;
	Double last_time;
	Bool in_area, just_entered, has_left;
	GF_Matrix2D init_matrix;
	GF_Renderer *compositor;
} Prox2DStack;

static void DestroyProximitySensor2D(GF_Node *node)
{
	Prox2DStack *st = (Prox2DStack *) gf_node_get_private(node);
	unregister_sensor(st->compositor, &st->hdl);
	free(st);
}

static Bool prox2D_is_enabled(SensorHandler *sh)
{
	return ((M_ProximitySensor2D *) sh->owner)->enabled;
}

static Bool prox2D_is_in_sensor(Prox2DStack *st, M_ProximitySensor2D *ps, Fixed X, Fixed Y)
{
	if (X < ps->center.x - ps->size.x/2) return 0;
	if (X > ps->center.x + ps->size.x/2) return 0;
	if (Y < ps->center.y - ps->size.y/2) return 0;
	if (Y > ps->center.y + ps->size.y/2) return 0;
	return 1;
}

static void OnProximitySensor2D(SensorHandler *sh, UserEvent2D *ev, GF_Matrix2D *sensor_matrix)
{
	Fixed X, Y;
	GF_Matrix2D mat;
	M_ProximitySensor2D *ps = (M_ProximitySensor2D *)sh->owner;
	Prox2DStack *stack = (Prox2DStack *) gf_node_get_private(sh->owner);
	
	if (! ps->enabled) return;
	
	if (ev->context) {
		X = ev->x;
		Y = ev->y;
		gf_mx2d_copy(mat, *sensor_matrix);
		gf_mx2d_inverse(&mat);
		gf_mx2d_apply_coords(&mat, &X, &Y);

		stack->last_time = gf_node_get_scene_time(sh->owner);

		//don't use transform since we just reverted our coords
		if (prox2D_is_in_sensor(stack, ps, X, Y)) {
			stack->has_left = 0;
			ps->position_changed.x = X;
			ps->position_changed.y = Y;
			gf_node_event_out_str(sh->owner, "position_changed");

			if(! stack->just_entered) {
				stack->just_entered = 1;
				stack->in_area = 1;
				ps->isActive = 1;
				gf_node_event_out_str(sh->owner, "isActive");
				ps->enterTime = stack->last_time;
				gf_node_event_out_str(sh->owner, "enterTime");
				R2D_RegisterSensor(stack->compositor, &stack->hdl);
			}
			return;
		}
	}
	if (stack->in_area) {
		if (stack->has_left) {
			stack->in_area = 0;
			stack->just_entered = 0;
			ps->exitTime = stack->last_time;
			gf_node_event_out_str(sh->owner, "exitTime");
			ps->isActive = 0;
			gf_node_event_out_str(sh->owner, "isActive");
			R2D_UnregisterSensor(stack->compositor, &stack->hdl);
			return;
		}
		stack->has_left = 1;
	}
}

SensorHandler *r2d_prox2D_get_handler(GF_Node *n)
{
	Prox2DStack *st = (Prox2DStack *)gf_node_get_private(n);
	return &st->hdl;
}

void R2D_InitProximitySensor2D(Render2D *sr, GF_Node *node)
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
	unregister_sensor(st->compositor, &st->hdl);
	free(st);
}

static Bool ts_is_enabled(SensorHandler *sh)
{
	return ((M_TouchSensor *) sh->owner)->enabled;
}

static void OnTouchSensor(SensorHandler *sh, UserEvent2D *ev, GF_Matrix2D *sensor_matrix)
{
	Fixed X, Y;
	GF_Matrix2D inv;
	M_TouchSensor *ts = (M_TouchSensor *)sh->owner;
	TouchSensorStack *stack = (TouchSensorStack *) gf_node_get_private(sh->owner);

	if (! ts->enabled) return;

	if (ev->context == NULL) {
		if (ts->isOver) {
			ts->isOver = 0;
			gf_node_event_out_str(sh->owner, "isOver");
		}

		if (ts->isActive) {
			if (ev->event_type == GF_EVT_LEFTUP) {
				ts->isOver = 0;
				gf_node_event_out_str(sh->owner, "isOver");
				ts->isActive = 0;
				gf_node_event_out_str(sh->owner, "isActive");
				
				/*no touchTime - VRML quote :"the pointing device shall be over the geometry (isOver is TRUE)"*/
				stack->mouse_down = 0;
				R2D_UnregisterSensor(stack->compositor, &stack->hdl);
			}
		} else {
			stack->mouse_down = 0;
			R2D_UnregisterSensor(stack->compositor, &stack->hdl);
		}
		return;
	}
	if ((ev->event_type == GF_EVT_MOUSEMOVE) && !ts->isOver) {
		ts->isOver = 1;
		gf_node_event_out_str(sh->owner, "isOver");
		R2D_RegisterSensor(stack->compositor, &stack->hdl);
	}

	if ((ev->event_type == GF_EVT_LEFTDOWN) && !stack->mouse_down) {
		ts->isActive = 1;
		gf_node_event_out_str(sh->owner, "isActive");
		stack->mouse_down = 1;
	}

	// If the button is first released, then generate an isActive = FALSE event, and ungrab all movement events.
	if ((ev->event_type == GF_EVT_LEFTUP) && stack->mouse_down) {
		ts->isActive = 0;
		gf_node_event_out_str(sh->owner, "isActive");
		stack->mouse_down = 0;
		ts->touchTime = gf_node_get_scene_time(sh->owner);
		gf_node_event_out_str(sh->owner, "touchTime");
	}
	X = ev->x;
	Y = ev->y;
	gf_mx2d_copy(inv, *sensor_matrix);
	gf_mx2d_inverse(&inv);
	gf_mx2d_apply_coords(&inv, &X, &Y);
	ts->hitPoint_changed.x = X;
	ts->hitPoint_changed.y = Y;
	ts->hitPoint_changed.z = 0;
	gf_node_event_out_str(sh->owner, "hitPoint_changed");
}

SensorHandler *r2d_touch_sensor_get_handler(GF_Node *n)
{
	TouchSensorStack *ts = (TouchSensorStack *)gf_node_get_private(n);
	return &ts->hdl;
}

void R2D_InitTouchSensor(Render2D *sr, GF_Node *node)
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
