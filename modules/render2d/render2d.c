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


#include "render2d.h"
#include "stacks2d.h"
#include "visualsurface2d.h"
#include <gpac/options.h>

#ifndef GPAC_DISABLE_SVG
#include "svg_stacks.h"
#endif

void R2D_MapCoordsToAR(Render2D *sr, s32 *x, s32 *y)
{
	if (sr->surface->center_coords) {
		/*revert to BIFS like*/
		*x = *x - sr->compositor->width /2;
		*y = sr->compositor->height/2 - *y;
	} else {
		*x -= sr->offset_x;
		*y -= sr->offset_y;
	}

	/*if no size info scaling is never applied*/
	if (!sr->compositor->has_size_info) return;

	if (!sr->scalable_zoom) {
		Fixed _x, _y;
		_x = INT2FIX(*x);
		_y = INT2FIX(*y);
		_x = gf_muldiv(_x, INT2FIX(sr->compositor->scene_width ), INT2FIX(sr->compositor->width));
		_y = gf_muldiv(_y, INT2FIX(sr->compositor->scene_height), INT2FIX(sr->compositor->height));
		*x = FIX2INT(_x);
		*y = FIX2INT(_y);
	}
}

static void R2D_SetUserTransform(Render2D *sr, Fixed zoom, Fixed tx, Fixed ty, Bool is_resize) 
{
	Fixed ratio;
	Fixed old_tx, old_ty, old_z;
	
	gf_sr_lock(sr->compositor, 1);
	old_tx = tx;
	old_ty = ty;
	old_z = sr->zoom;

	if (zoom <= 0) zoom = FIX_ONE/1000;
	sr->trans_x = tx;
	sr->trans_y = ty;

	if (zoom != sr->zoom) {
		ratio = gf_divfix(zoom, sr->zoom);
		sr->trans_x = gf_mulfix(sr->trans_x, ratio);
		sr->trans_y = gf_mulfix(sr->trans_y, ratio);
		sr->zoom = zoom;

		/*recenter surface*/
		if (!sr->surface->center_coords) {
			Fixed c_x, c_y, nc_x, nc_y;
			c_x = INT2FIX(sr->compositor->width/2);
			nc_y = c_y = INT2FIX(sr->compositor->height/2);
			nc_x = gf_mulfix(c_x, ratio);
			nc_y = gf_mulfix(c_y, ratio);
			sr->trans_x -= (nc_x-c_x);
			sr->trans_y -= (nc_y-c_y);
		}
	}
	gf_mx2d_init(sr->top_effect->transform);
	gf_mx2d_add_scale(&sr->top_effect->transform, gf_mulfix(sr->zoom,sr->scale_x), gf_mulfix(sr->zoom,sr->scale_y));
//	gf_mx2d_add_scale(&sr->top_effect->transform, sr->zoom, sr->zoom);
	gf_mx2d_add_translation(&sr->top_effect->transform, sr->trans_x, sr->trans_y);
	if (sr->rotation) gf_mx2d_add_rotation(&sr->top_effect->transform, 0, 0, sr->rotation);

	if (!sr->surface->center_coords) {
		gf_mx2d_add_translation(&sr->top_effect->transform, INT2FIX(sr->offset_x), INT2FIX(sr->offset_y));
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 2D] Changing Zoom (%g) and Pan (%g %g)\n", FIX2FLT(sr->zoom), FIX2FLT(sr->trans_x) , FIX2FLT(sr->trans_y)));


	sr->compositor->draw_next_frame = 1;
	sr->top_effect->invalidate_all = 1;

#ifndef GPAC_DISABLE_SVG
	if (sr->use_dom_events) {
		GF_DOM_Event evt;
		memset(&evt, 0, sizeof(GF_DOM_Event));
		evt.prev_scale = sr->scale_x*old_z;
		evt.new_scale = sr->scale_x*sr->zoom;

		if (is_resize) {
			evt.type = GF_EVENT_RESIZE;
		} else if (evt.prev_scale == evt.new_scale) {
			/*cannot get params for scroll events*/
			evt.type = GF_EVENT_SCROLL;
		} else {
			evt.screen_rect.x = INT2FIX(sr->offset_x);
			evt.screen_rect.y = INT2FIX(sr->offset_y);
			evt.screen_rect.width = INT2FIX(sr->cur_width);
			evt.screen_rect.height = INT2FIX(sr->cur_height);
			evt.prev_translate.x = old_tx;
			evt.prev_translate.y = old_ty;
			evt.new_translate.x = sr->trans_x;
			evt.new_translate.y = sr->trans_y;
			evt.type = GF_EVENT_ZOOM;
		}
		gf_dom_event_fire(gf_sg_get_root_node(sr->compositor->scene), NULL, &evt);
	}
#endif

	gf_sr_lock(sr->compositor, 0);
}

void R2D_SetScaling(Render2D *sr, Fixed scaleX, Fixed scaleY)
{
	sr->scale_x = scaleX;
	sr->scale_y = scaleY;
	R2D_SetUserTransform(sr, sr->zoom, sr->trans_x, sr->trans_y, 1);
}

void R2D_ResetSurfaces(Render2D *sr)
{
	VisualSurface2D *surf;
	u32 i=0;
	while ((surf = (VisualSurface2D *)gf_list_enum(sr->surfaces_2D, &i))) {
		/*reset display list*/
		surf->cur_context = surf->context;
		if (surf->cur_context) surf->cur_context->drawable = NULL;
		while (surf->prev_nodes) {
			struct _drawable_store *cur = surf->prev_nodes;
			surf->prev_nodes = cur->next;
			free(cur);
		}
		surf->last_prev_entry = NULL;
		surf->to_redraw.count = 0;
	}
}

void R2D_SceneReset(GF_VisualRenderer *vr)
{
	u32 flag;
	Render2D *sr = (Render2D*) vr->user_priv;
	if (!sr) return;
	R2D_ResetSurfaces(sr);
	while (gf_list_count(sr->sensors)) {
		gf_list_rem(sr->sensors, 0);
	}

	flag = sr->top_effect->trav_flags;
	effect_reset(sr->top_effect);
	sr->top_effect->trav_flags = flag;
	sr->compositor->reset_graphics = 1;
	sr->trans_x = sr->trans_y = 0;
	sr->zoom = FIX_ONE;
	sr->grab_node = NULL;
	sr->grab_ctx = NULL;
	sr->focus_node = NULL;
	R2D_SetScaling(sr, sr->scale_x, sr->scale_y);
	/*force resetup of main surface in case we're switching coord system*/
	sr->main_surface_setup = 0;
	sr->navigation_disabled = 0;
	VS2D_ResetGraphics(sr->surface);
}

GF_Rect R2D_ClipperToPixelMetrics(RenderEffect2D *eff, SFVec2f size)
{
	GF_Rect res;

	if (eff->surface->composite) {
		res.width = INT2FIX(eff->surface->width);
		res.height = INT2FIX(eff->surface->height);
	} else {
		res.width = INT2FIX(eff->surface->render->compositor->scene_width);
		res.height = INT2FIX(eff->surface->render->compositor->scene_height);
	}
	if (eff->is_pixel_metrics) {
		if (size.x>=0) res.width = size.x;
		if (size.y>=0) res.height = size.y;
	} else {
		if (size.x>=0) res.width = gf_mulfix(res.width, size.x / 2);
		if (size.y>=0) res.height = gf_mulfix(res.height, size.y / 2);
	}
	res = gf_rect_center(res.width, res.height);
	return res;
}


void R2D_RegisterSurface(Render2D *sr, struct _visual_surface_2D  *surf)
{
	if (R2D_IsSurfaceRegistered(sr, surf)) return;
	gf_list_add(sr->surfaces_2D, surf);
}

void R2D_UnregisterSurface(Render2D *sr, struct _visual_surface_2D  *surf)
{
	gf_list_del_item(sr->surfaces_2D, surf);
}

Bool R2D_IsSurfaceRegistered(Render2D *sr, struct _visual_surface_2D *surf)
{
	VisualSurface2D *tmp;
	u32 i = 0;
	while ((tmp = (VisualSurface2D *)gf_list_enum(sr->surfaces_2D, &i))) {
		if (tmp == surf) return 1;
	}
	return 0;
}

void effect_add_sensor(RenderEffect2D *eff, SensorHandler *ptr, GF_Matrix2D *mat)
{
	SensorContext *ctx;
	if (!ptr) return;
	ctx = (SensorContext *)malloc(sizeof(SensorContext));
	ctx->h_node = ptr;
	
	if (mat) {
		gf_mx2d_copy(ctx->matrix, *mat);
	} else {
		gf_mx2d_init(ctx->matrix);
	}
	gf_list_add(eff->sensors, ctx);
}
void effect_pop_sensor(RenderEffect2D *eff)
{
	SensorContext *ctx;
	u32 last = gf_list_count(eff->sensors);
	if (!last) return;
	ctx = (SensorContext *)gf_list_get(eff->sensors, last-1);
	gf_list_rem(eff->sensors, last-1);
	free(ctx);
}

void effect_reset_sensors(RenderEffect2D *eff)
{
	SensorContext *ctx;
	while (gf_list_count(eff->sensors)) {
		ctx = (SensorContext *)gf_list_get(eff->sensors, 0);
		gf_list_rem(eff->sensors, 0);
		free(ctx);
	}
}

void effect_reset(RenderEffect2D *eff)
{
	GF_List *bck = eff->sensors;
	memset(eff, 0, sizeof(RenderEffect2D));
	eff->sensors = bck;
	if (bck) effect_reset_sensors(eff);
	gf_mx2d_init(eff->transform);
	gf_cmx_init(&eff->color_mat);
}

void effect_delete(RenderEffect2D *eff)
{
	if (eff->sensors) {
		effect_reset_sensors(eff);
		gf_list_del(eff->sensors);
	}
	free(eff);
}

Bool is_sensor_node(GF_Node *node)
{
	switch (gf_node_get_tag(node)) {
	case TAG_MPEG4_TouchSensor:
	case TAG_MPEG4_PlaneSensor2D:
	case TAG_MPEG4_DiscSensor:
	case TAG_MPEG4_ProximitySensor2D: 
		return 1;

		/*anchor is not considered as a child sensor node when picking sensors*/
	/*case TAG_MPEG4_Anchor:*/
#ifndef GPAC_DISABLE_SVG
	/*case TAG_SVG_a: */
#endif
	default:
		return 0;
	}
}

SensorHandler *get_sensor_handler(GF_Node *n)
{
	SensorHandler *hs;

	switch (gf_node_get_tag(n)) {
	case TAG_MPEG4_Anchor: hs = r2d_anchor_get_handler(n); break;
	case TAG_MPEG4_DiscSensor: hs = r2d_ds_get_handler(n); break;
	case TAG_MPEG4_TouchSensor: hs = r2d_touch_sensor_get_handler(n); break;
	case TAG_MPEG4_PlaneSensor2D: hs = r2d_ps2D_get_handler(n); break;
	case TAG_MPEG4_ProximitySensor2D: hs = r2d_prox2D_get_handler(n); break;
	default: return NULL;
	}
	if (hs && hs->IsEnabled(hs)) return hs;
	return NULL;
}

void R2D_RegisterSensor(GF_Renderer *compositor, SensorHandler *sh)
{
	SensorHandler *tmp;
	u32 i=0;
	Render2D *sr = (Render2D *)compositor->visual_renderer->user_priv;
	while ((tmp = (SensorHandler *)gf_list_enum(sr->sensors, &i))) {
		if (tmp == sh) return;
	}
	gf_list_add(sr->sensors, sh);
}

void R2D_UnregisterSensor(GF_Renderer *compositor, SensorHandler *sh)
{	Render2D *sr = (Render2D *)compositor->visual_renderer->user_priv;
	gf_list_del_item(sr->sensors, sh);
}


#define R2DSETCURSOR(t) { GF_Event evt; evt.type = GF_EVENT_SET_CURSOR; evt.cursor.cursor_type = (t); sr->compositor->video_out->ProcessEvent(sr->compositor->video_out, &evt); }

#ifndef GPAC_DISABLE_SVG

Bool R2D_ExecuteDOMEvent(GF_VisualRenderer *vr, GF_Event *event, Fixed X, Fixed Y)
{
	GF_DOM_Event evt;
	u32 cursor_type;
	Bool ret = 0;
	Render2D *sr = (Render2D *)vr->user_priv;

	cursor_type = GF_CURSOR_NORMAL;
	/*all mouse events*/
	if (event->type<=GF_EVENT_MOUSEMOVE) {
		DrawableContext *ctx = VS2D_PickContext(sr->surface, X, Y);
		if (ctx) {
			cursor_type = sr->last_sensor;
			memset(&evt, 0, sizeof(GF_DOM_Event));
			evt.clientX = evt.screenX = FIX2INT(X);
			evt.clientY = evt.screenY = FIX2INT(Y);
			evt.bubbles = 1;
			evt.cancelable = 1;
			evt.key_flags = sr->compositor->key_states;

			switch (event->type) {
			case GF_EVENT_MOUSEMOVE:
				evt.cancelable = 0;
				if (sr->grab_node != ctx->drawable) {
					/*mouse out*/
					if (sr->grab_node) {
						evt.relatedTarget = ctx->drawable->node;
						evt.type = GF_EVENT_MOUSEOUT;
						ret += gf_dom_event_fire(sr->grab_node->node, NULL, &evt);
						/*prepare mouseOver*/
						evt.relatedTarget = sr->grab_node->node;
					}
					/*mouse over*/
					evt.type = GF_EVENT_MOUSEOVER;
					ret += gf_dom_event_fire(ctx->drawable->node, ctx->appear, &evt);

					sr->grab_ctx = ctx;
					sr->grab_node = ctx->drawable;
				} else {
					evt.type = GF_EVENT_MOUSEMOVE;
					ret += gf_dom_event_fire(ctx->drawable->node, ctx->appear, &evt);
				}
				break;
			case GF_EVENT_MOUSEDOWN:
				if ((sr->last_click_x!=evt.screenX) || (sr->last_click_y!=evt.screenY)) sr->num_clicks = 0;
				evt.type = GF_EVENT_MOUSEDOWN;
				evt.detail = event->mouse.button;
				ret += gf_dom_event_fire(ctx->drawable->node, ctx->appear, &evt);
				sr->last_click_x = evt.screenX;
				sr->last_click_y = evt.screenY;
				break;
			case GF_EVENT_MOUSEUP:
				evt.type = GF_EVENT_MOUSEUP;
				evt.detail = event->mouse.button;
				ret += gf_dom_event_fire(ctx->drawable->node, ctx->appear, &evt);
				if ((sr->last_click_x==evt.screenX) || (sr->last_click_y==evt.screenY)) {
					sr->num_clicks ++;
					evt.type = GF_EVENT_CLICK;
					evt.detail = sr->num_clicks;
					ret += gf_dom_event_fire(ctx->drawable->node, ctx->appear, &evt);
				}
				break;
			}
			cursor_type = evt.has_ui_events ? GF_CURSOR_TOUCH : GF_CURSOR_NORMAL;
		} else {
			/*mouse out*/
			if (sr->grab_node) {
				memset(&evt, 0, sizeof(GF_DOM_Event));
				evt.clientX = evt.screenX = FIX2INT(X);
				evt.clientY = evt.screenY = FIX2INT(Y);
				evt.bubbles = 1;
				evt.cancelable = 1;
				evt.key_flags = sr->compositor->key_states;
				evt.type = GF_EVENT_MOUSEOUT;
				ret += gf_dom_event_fire(sr->grab_node->node, NULL, &evt);
			}
			sr->grab_node = NULL;
			sr->grab_ctx = NULL;

			/*dispatch event to root SVG*/
			memset(&evt, 0, sizeof(GF_DOM_Event));
			evt.clientX = evt.screenX = FIX2INT(X);
			evt.clientY = evt.screenY = FIX2INT(Y);
			evt.bubbles = 1;
			evt.cancelable = 1;
			evt.key_flags = sr->compositor->key_states;
			evt.type = event->type;
			ret += gf_dom_event_fire(gf_sg_get_root_node(sr->compositor->scene), NULL, &evt);
		}
		if (sr->last_sensor != cursor_type) {
			R2DSETCURSOR(cursor_type);
			sr->last_sensor = cursor_type;
		}
	}
	else if (event->type==GF_EVENT_TEXTINPUT) {
	} 
	else if ((event->type>=GF_EVENT_KEYUP) && (event->type<=GF_EVENT_LONGKEYPRESS)) {
		memset(&evt, 0, sizeof(GF_DOM_Event));
		evt.key_flags = event->key.flags;
		evt.bubbles = 1;
		evt.cancelable = 1;
		evt.type = event->type;
		evt.detail = event->key.key_code;
		evt.key_hw_code = event->key.hw_code;
		ret += gf_dom_event_fire(sr->focus_node, NULL, &evt);

		if (event->type==GF_EVENT_KEYDOWN) {
			switch (event->key.key_code) {
			case GF_KEY_ENTER:
				evt.type = GF_EVENT_ACTIVATE;
				evt.detail = 0;
				ret += gf_dom_event_fire(sr->focus_node, NULL, &evt);
				break;
			case GF_KEY_TAB:
				ret += svg_focus_switch_ring(sr, (event->key.flags & GF_KEY_MOD_SHIFT) ? 1 : 0);
				break;
			case GF_KEY_UP:
			case GF_KEY_DOWN:
			case GF_KEY_LEFT:
			case GF_KEY_RIGHT:
				ret += svg_focus_navigate(sr, event->key.key_code);
				break;
			}
		} 

/*		else if (event->event_type==GF_EVENT_CHAR) {
		} else {
			evt.type = ((event->event_type==GF_EVENT_VKEYDOWN) || (event->event_type==GF_EVENT_KEYDOWN)) ? GF_EVENT_KEYDOWN : GF_EVENT_KEYUP;
			evt.detail = event->key.virtual_code;
		}
*/
	}
	return ret;
}

#endif


Bool R2D_ExecuteEvent(GF_VisualRenderer *vr, GF_Event *event)
{
	u32 i, type, count;
	Bool act;
	s32 key_inv;
	Fixed key_trans;
	GF_Event evt;
	SensorContext *sc;
	DrawableContext *ctx, *prev_ctx;
	Render2D *sr = (Render2D *)vr->user_priv;

	evt = *event;

	if (event->type<=GF_EVENT_MOUSEWHEEL) {
		R2D_MapCoordsToAR(sr, &evt.mouse.x, &evt.mouse.y);
	}

#ifndef GPAC_DISABLE_SVG
	/*DOM-style events*/
	if (sr->use_dom_events) {
		if (R2D_ExecuteDOMEvent(vr, event, INT2FIX(evt.mouse.x), INT2FIX(evt.mouse.y) )) {
			sr->grabbed = 0;
			return 1;
		}
		goto browser_event;
	}
#endif
	/*only process mouse events for MPEG-4/VRML*/
	if (event->type>=GF_EVENT_MOUSEWHEEL) goto browser_event;
	
	if (sr->is_tracking) {
		/*in case a node is inserted at the depth level of a node previously tracked (rrrhhhaaaa...) */
		if (sr->grab_ctx && sr->grab_ctx->drawable != sr->grab_node) {
			sr->is_tracking = 0;
			sr->grab_ctx = NULL;
		}
	}
	
	prev_ctx = sr->grab_ctx;
	if (!sr->is_tracking) {
		ctx = VS2D_PickSensitiveNode(sr->surface, INT2FIX(evt.mouse.x), INT2FIX(evt.mouse.y) );
		sr->grab_ctx = ctx;
		if (ctx) sr->grab_node = ctx->drawable;
	} else {
		ctx = sr->grab_ctx;
	}

	//3- mark all sensors of the context to skip deactivation
	if (ctx) {	
		SensorContext *sc = ctx->sensor;
		type = GF_CURSOR_NORMAL;
		
		while (sc) {
			sc->h_node->skip_second_pass = 1;
			if (!sc->next) {
				switch (gf_node_get_tag(sc->h_node->owner)) {
				case TAG_MPEG4_Anchor: type = GF_CURSOR_ANCHOR; break;
				case TAG_MPEG4_PlaneSensor2D: type = GF_CURSOR_PLANE; break;
				case TAG_MPEG4_DiscSensor: type = GF_CURSOR_ROTATE; break;
				case TAG_MPEG4_ProximitySensor2D: type = GF_CURSOR_PROXIMITY; break;
				case TAG_MPEG4_TouchSensor: type = GF_CURSOR_TOUCH; break;
#ifndef GPAC_DISABLE_SVG
#ifdef GPAC_ENABLE_SVG_SA
				case TAG_SVG_SA_a: 
#endif
				case TAG_SVG_a: 
					type = GF_CURSOR_ANCHOR; 
					break;
#endif
				default: type = GF_CURSOR_TOUCH; break;
				}
			}
			sc = sc->next;
		}
		//also notify the app we're above a sensor
		if (type != GF_CURSOR_NORMAL) {
			if (sr->last_sensor != type) {
				GF_Event evt;
				evt.type = GF_EVENT_SET_CURSOR;
				evt.cursor.cursor_type = type;
				sr->compositor->video_out->ProcessEvent(sr->compositor->video_out, &evt);
				sr->last_sensor = type;
			}
		}
	}

	if (!ctx && (sr->last_sensor != GF_CURSOR_NORMAL)) {
		R2DSETCURSOR(GF_CURSOR_NORMAL);
		sr->last_sensor = GF_CURSOR_NORMAL;
	}

	/*deactivate all other registered sensors*/
	count = gf_list_count(sr->sensors);
	for (i=0; i< count; i++) {
		SensorHandler *sh = (SensorHandler *)gf_list_get(sr->sensors, i);
		act = ! sh->skip_second_pass;
		sh->skip_second_pass = 0;
		if (act)
			sh->OnUserEvent(sh, &evt, NULL, NULL);
		if (count != gf_list_count(sr->sensors)) {
			count--;
			i-= 1;
		}
	}
	/*some sensors may not register with us, we still call them*/
	if (prev_ctx && (prev_ctx != ctx)) {
		sc = prev_ctx->sensor;
		while (sc) {
			sc->h_node->OnUserEvent(sc->h_node, &evt, NULL, NULL);
			sc = sc->next;
		}
	}

	/*activate current one if any*/
	if (ctx) {
		sc = ctx->sensor;
		while (sc) {
			sc->h_node->skip_second_pass = 0;
			if (sc->h_node->OnUserEvent(sc->h_node, &evt, ctx, &sc->matrix)) 
				break;
			sc = sc->next;
		}
		return 1;
	}

browser_event:
	/*no object, perform zoom & pan*/
	if (!(sr->compositor->interaction_level & GF_INTERACT_NAVIGATION) || !sr->navigate_mode || sr->navigation_disabled) return 0;

	key_inv = 1;
	key_trans = 2*FIX_ONE;
	if (sr->compositor->key_states & GF_KEY_MOD_SHIFT) key_trans *= 4;

	switch (event->type) {
	case GF_EVENT_MOUSEDOWN:
		if (event->mouse.button==GF_MOUSE_LEFT) {
			sr->grab_x = evt.mouse.x;
			sr->grab_y = evt.mouse.y;
			sr->grabbed = 1;
		}
		break;
	case GF_EVENT_MOUSEUP:
		if (event->mouse.button==GF_MOUSE_LEFT) sr->grabbed = 0;
		break;
	case GF_EVENT_MOUSEWHEEL:
		if (sr->navigate_mode == GF_NAVIGATE_SLIDE) {
			Fixed new_zoom = sr->zoom;
			if (event->mouse.wheel_pos > 0) new_zoom += FIX_ONE/20;
			else new_zoom -= FIX_ONE/20;
			R2D_SetUserTransform(sr, new_zoom, sr->trans_x, sr->trans_y, 0);
		}
		break;
	case GF_EVENT_MOUSEMOVE:
		if (sr->grabbed && (sr->navigate_mode == GF_NAVIGATE_SLIDE)) {
			Fixed dx, dy;
			dx = INT2FIX(evt.mouse.x - sr->grab_x);
			dy = INT2FIX(evt.mouse.y - sr->grab_y);
			if (! sr->top_effect->is_pixel_metrics) {
				dx /= sr->cur_width;
				dy /= sr->cur_height;
			}
			if (sr->compositor->key_states & GF_KEY_MOD_SHIFT) {
				dx *= 2;
				dy *= 2;
			}
			/*set zoom*/
			if (sr->compositor->key_states & GF_KEY_MOD_CTRL) {
				Fixed new_zoom = sr->zoom;
				if (new_zoom > FIX_ONE/2) new_zoom += dy/10;
				else new_zoom += dy*new_zoom/10;
				R2D_SetUserTransform(sr, new_zoom, sr->trans_x, sr->trans_y, 0);
			}
			/*set pan*/
			else {
				R2D_SetUserTransform(sr, sr->zoom, sr->trans_x+dx, sr->trans_y+dy, 0);
			}
			sr->grab_x = evt.mouse.x;
			sr->grab_y = evt.mouse.y;
		}
		break;
	case GF_EVENT_KEYDOWN:
		switch (event->key.key_code) {
		case GF_KEY_HOME:
			if (!sr->grabbed) R2D_SetUserTransform(sr, FIX_ONE, 0, 0, 0);
			break;
		case GF_KEY_LEFT: key_inv = -1;
		case GF_KEY_RIGHT:
			sr->trans_x += key_inv*key_trans;
			R2D_SetUserTransform(sr, sr->zoom, sr->trans_x + key_inv*key_trans, sr->trans_y, 0);
			break;
		case GF_KEY_DOWN: key_inv = -1;
		case GF_KEY_UP:
			if (sr->compositor->key_states & GF_KEY_MOD_CTRL) {
				Fixed new_zoom = sr->zoom;
				if (new_zoom > FIX_ONE) new_zoom += key_inv*FIX_ONE/10;
				else new_zoom += key_inv*FIX_ONE/20;
				R2D_SetUserTransform(sr, new_zoom, sr->trans_x, sr->trans_y, 0);
			} else {
				R2D_SetUserTransform(sr, sr->zoom, sr->trans_x, sr->trans_y + key_inv*key_trans, 0);
			}
			break;
		case GF_KEY_VOLUMEDOWN: key_inv = -1;
		case GF_KEY_VOLUMEUP:
		{
			Fixed new_zoom = sr->zoom;
			if (new_zoom > FIX_ONE) new_zoom += key_inv*FIX_ONE/10;
			else new_zoom += key_inv*FIX_ONE/20;
			R2D_SetUserTransform(sr, new_zoom, sr->trans_x, sr->trans_y, 0);
		}
			break;
		}
		break;
	}
	return 0;
}

void R2D_DrawScene(GF_VisualRenderer *vr)
{
	GF_Window rc;
	u32 i;
	GF_Err e;
	GF_SceneGraph *sg;
	RenderEffect2D static_eff;
	Render2D *sr = (Render2D *)vr->user_priv;
	GF_Node *top_node = gf_sg_get_root_node(sr->compositor->scene);

	if (!top_node && !sr->surface->last_had_back && !sr->surface->cur_context) {
		//GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 2D] Scene has no root node, nothing to draw\n"));
		return;
	}

	if (top_node && !sr->main_surface_setup) {
		sr->use_dom_events = 0;
		sr->main_surface_setup = 1;
		sr->surface->center_coords = 1;

		sr->focus_node = NULL;

		sr->top_effect->is_pixel_metrics = gf_sg_use_pixel_metrics(sr->compositor->scene);
		sr->top_effect->min_hsize = INT2FIX(MIN(sr->compositor->scene_width, sr->compositor->scene_height)) / 2;

#ifndef GPAC_DISABLE_SVG
		{
			u32 node_tag = gf_node_get_tag(top_node);
			if ( ((node_tag>=GF_NODE_RANGE_FIRST_SVG) && (node_tag<=GF_NODE_RANGE_LAST_SVG)) 
#ifdef GPAC_ENABLE_SVG_SA
				|| ((node_tag>=GF_NODE_RANGE_FIRST_SVG_SA) && (node_tag<=GF_NODE_RANGE_LAST_SVG_SA))
#endif
#ifdef GPAC_ENABLE_SVG_SANI
				|| ((node_tag>=GF_NODE_RANGE_FIRST_SVG_SANI) && (node_tag<=GF_NODE_RANGE_LAST_SVG_SANI))
#endif
				) {
				sr->surface->center_coords = 0;
				sr->main_surface_setup = 2;
				sr->use_dom_events = 1;
				sr->top_effect->is_pixel_metrics = 1;
			}
		}
#endif

		GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 2D] Main scene setup - Using DOM events: %d - pixel metrics %d - center coords %d\n", sr->use_dom_events, sr->top_effect->is_pixel_metrics, sr->surface->center_coords));
	}

	memcpy(&static_eff, sr->top_effect, sizeof(RenderEffect2D));

	sr->surface->width = sr->cur_width;
	sr->surface->height = sr->cur_height;

#if 0
	{
		GF_SystemRTInfo rti;
		gf_sys_get_rti(0, &rti, GF_RTI_SYSTEM_MEMORY_ONLY);
		fprintf(stdout, "Memory usage before DrawScene: %d\n", rti.gpac_memory);
	}
#endif

	e = VS2D_InitDraw(sr->surface, sr->top_effect);
	if (e) return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 2D] Traversing scene tree (top node %s)\n", top_node ? gf_node_get_class_name(top_node) : "none"));

#ifndef GPAC_DISABLE_SVG
	i = gf_sys_clock();
#endif
	gf_node_render(top_node, sr->top_effect);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 2D] Traversing scene done in %d ms\n", gf_sys_clock() - i));

	i=0;
	while ((sg = (GF_SceneGraph*)gf_list_enum(sr->compositor->extra_scenes, &i))) {
		GF_Node *n = gf_sg_get_root_node(sg);
		if (n) gf_node_render(n, sr->top_effect);
	}

	VS2D_TerminateDraw(sr->surface, sr->top_effect);
	memcpy(sr->top_effect, &static_eff, sizeof(RenderEffect2D));
	sr->top_effect->invalidate_all = 0;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 2D] Rendering done - flushing output\n"));

	/*and flush*/
	rc.x = rc.y = 0; 
	rc.w = sr->compositor->width;	
	rc.h = sr->compositor->height;		
	sr->compositor->video_out->Flush(sr->compositor->video_out, &rc);
	sr->frame_num++;
#if 0
	{
		GF_SystemRTInfo rti;
		gf_sys_get_rti(0, &rti, GF_RTI_SYSTEM_MEMORY_ONLY);
		fprintf(stdout, "Memory usage after DrawScene: %d\n", rti.gpac_memory);
	}
#endif 
}

Bool R2D_IsPixelMetrics(GF_Node *n)
{
	GF_SceneGraph *sg = gf_node_get_graph(n);
	return gf_sg_use_pixel_metrics(sg);
}


static GF_Err R2D_RecomputeAR(GF_VisualRenderer *vr)
{
	Double ratio;
	GF_Event evt;
	u32 out_width, out_height;
	Fixed scaleX, scaleY;
	Render2D *sr = (Render2D *)vr->user_priv;
	if (!sr->compositor->scene_height || !sr->compositor->scene_width) return GF_OK;
	if (!sr->compositor->height || !sr->compositor->width) return GF_OK;

	sr->cur_width = sr->compositor->scene_width;
	sr->cur_height = sr->compositor->scene_height;
	sr->offset_x = sr->offset_y = 0;

	/*force complete clean*/
	sr->top_effect->invalidate_all = 1;

	if (!sr->compositor->has_size_info && !(sr->compositor->override_size_flags & 2) ) {
		sr->cur_width = sr->compositor->width;
		sr->cur_height = sr->compositor->height;
		R2D_SetScaling(sr, FIX_ONE, FIX_ONE);
		/*and resize hardware surface*/
		evt.type = GF_EVENT_VIDEO_SETUP;
		evt.size.width = sr->cur_width;
		evt.size.height = sr->cur_height;
		return sr->compositor->video_out->ProcessEvent(sr->compositor->video_out, &evt);
	}
	out_width = sr->compositor->width;
	out_height = sr->compositor->height;

	switch (sr->compositor->aspect_ratio) {
	case GF_ASPECT_RATIO_FILL_SCREEN:
		break;
	case GF_ASPECT_RATIO_16_9:
		out_width = sr->compositor->width;
		out_height = 9 * sr->compositor->width / 16;
		if (out_height>sr->compositor->height) {
			out_height = sr->compositor->height;
			out_width = 16 * sr->compositor->height / 9;
		}
		break;
	case GF_ASPECT_RATIO_4_3:
		out_width = sr->compositor->width;
		out_height = 3 * sr->compositor->width / 4;
		if (out_height>sr->compositor->height) {
			out_height = sr->compositor->height;
			out_width = 4 * sr->compositor->height / 3;
		}
		break;
	default:
		ratio = sr->compositor->scene_height;
		ratio /= sr->compositor->scene_width;
		if (out_width * ratio > out_height) {
			out_width = out_height * sr->compositor->scene_width;
			out_width /= sr->compositor->scene_height;
		}
		else {
			out_height = out_width * sr->compositor->scene_height;
			out_height /= sr->compositor->scene_width;
		}
		break;
	}
	sr->offset_x = (sr->compositor->width - out_width) / 2;
	sr->offset_y = (sr->compositor->height - out_height) / 2;

	scaleX = gf_divfix(INT2FIX(out_width), INT2FIX(sr->compositor->scene_width));
	scaleY = gf_divfix(INT2FIX(out_height), INT2FIX(sr->compositor->scene_height));

	if (!sr->scalable_zoom) {
		sr->cur_width = sr->compositor->scene_width;
		sr->cur_height = sr->compositor->scene_height;
		out_width = FIX2INT(gf_divfix(INT2FIX(sr->compositor->width), scaleX));
		out_height = FIX2INT(gf_divfix(INT2FIX(sr->compositor->height), scaleY));

		sr->offset_x = (out_width - sr->cur_width) / 2;
		sr->offset_y = (out_height - sr->cur_height) / 2;

		scaleX = scaleY = FIX_ONE;
	} else {
		sr->cur_width = out_width;
		sr->cur_height = out_height;
		out_width = sr->compositor->width;
		out_height = sr->compositor->height;
	}
	/*set scale factor*/
	R2D_SetScaling(sr, scaleX, scaleY);
	gf_sr_invalidate(sr->compositor, NULL);
	/*and resize hardware surface*/
	evt.type = GF_EVENT_VIDEO_SETUP;
	evt.size.width = out_width;
	evt.size.height = out_height;
	return sr->compositor->video_out->ProcessEvent(sr->compositor->video_out, &evt);
}

GF_Node *R2D_PickNode(GF_VisualRenderer *vr, s32 X, s32 Y)
{
	GF_Node *res = NULL;
	Render2D *sr = (Render2D *)vr->user_priv;

	if (!sr) return NULL;
	/*lock to prevent any change while picking*/
	gf_sr_lock(sr->compositor, 1);
	if (sr->compositor->scene) {
		R2D_MapCoordsToAR(sr, &X, &Y);
		res = VS2D_PickNode(sr->surface, INT2FIX(X), INT2FIX(Y) );
	}
	gf_sr_lock(sr->compositor, 0);
	return res;
}

GF_Err R2D_GetSurfaceAccess(VisualSurface2D *surf)
{
	GF_Err e;
	Render2D *sr = surf->render;

	if (!surf->the_surface) return GF_BAD_PARAM;
	sr->locked = 0;
	e = GF_IO_ERR;
	
	/*try from device*/
	if (sr->compositor->r2d->surface_attach_to_device && sr->compositor->video_out->LockOSContext) {
		sr->hardware_context = sr->compositor->video_out->LockOSContext(sr->compositor->video_out, 1);
		if (sr->hardware_context) {
			e = sr->compositor->r2d->surface_attach_to_device(surf->the_surface, sr->hardware_context, sr->cur_width, sr->cur_height);
			if (!e) {
				surf->is_attached = 1;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render2D] Video surface handle attached to raster\n"));
				return GF_OK;
			}
			sr->compositor->video_out->LockOSContext(sr->compositor->video_out, 0);
			GF_LOG(GF_LOG_ERROR, GF_LOG_RENDER, ("[Render2D] Cannot attach video surface handle to raster: %s\n", gf_error_to_string(e) ));
		}
	}
	
	/*TODO - collect hw accelerated blit routines if any*/

	if (sr->compositor->video_out->LockBackBuffer(sr->compositor->video_out, &sr->hw_surface, 1)==GF_OK) {
		sr->locked = 1;
		e = sr->compositor->r2d->surface_attach_to_buffer(surf->the_surface, sr->hw_surface.video_buffer, 
							sr->hw_surface.width, 
							sr->hw_surface.height,
							sr->hw_surface.pitch,
							(GF_PixelFormat) sr->hw_surface.pixel_format);
		if (!e) {
			surf->is_attached = 1;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render2D] Video surface memory attached to raster\n"));
			return GF_OK;
		}
		GF_LOG(GF_LOG_ERROR, GF_LOG_RENDER, ("[Render2D] Cannot attach video surface memory to raster: %s\n", gf_error_to_string(e) ));
		sr->compositor->video_out->LockBackBuffer(sr->compositor->video_out, &sr->hw_surface, 0);
	}
	sr->locked = 0;
	surf->is_attached = 0;
	return e;		
}

void R2D_ReleaseSurfaceAccess(VisualSurface2D *surf)
{
	Render2D *sr = surf->render;
	if (surf->is_attached) {
		sr->compositor->r2d->surface_detach(surf->the_surface);
		surf->is_attached = 0;
	}
	if (sr->hardware_context) {
		sr->compositor->video_out->LockOSContext(sr->compositor->video_out, 0);
		sr->hardware_context = NULL;
	} else if (sr->locked) {
		sr->compositor->video_out->LockBackBuffer(sr->compositor->video_out, &sr->hw_surface, 0);
		sr->locked = 0;
	}
}

Bool R2D_SupportsFormat(VisualSurface2D *surf, u32 pixel_format)
{
	switch (pixel_format) {
	case GF_PIXEL_RGB_24:
	case GF_PIXEL_BGR_24:
	case GF_PIXEL_RGB_555:
	case GF_PIXEL_RGB_565:
	case GF_PIXEL_ARGB:
	case GF_PIXEL_RGBA:
	case GF_PIXEL_YV12:
	case GF_PIXEL_IYUV:
	case GF_PIXEL_I420:
		return 1;
	/*the rest has to be displayed through brush for now, we only use YUV and RGB pool*/
	default:
		return 0;
	}
}

void R2D_DrawBitmap(VisualSurface2D *surf, struct _gf_sr_texture_handler *txh, GF_IRect *clip, GF_Rect *unclip, u8 alpha, u32 *col_key, GF_ColorMatrix *cmat)
{
	GF_VideoSurface video_src;
	Fixed w_scale, h_scale, tmp;
	GF_Err e;
	Bool use_soft_stretch;
	GF_Window src_wnd, dst_wnd;
	u32 start_x, start_y, cur_width, cur_height;
	GF_IRect clipped_final = *clip;
	GF_Rect final = *unclip;

	if (!txh->data) return;

	if (!surf->render->compositor->has_size_info && !(surf->render->compositor->msg_type & GF_SR_CFG_OVERRIDE_SIZE) 
		&& (surf->render->compositor->override_size_flags & 1) 
		&& !(surf->render->compositor->override_size_flags & 2) 
		) {
		if ( (surf->render->compositor->scene_width < txh->width) 
			|| (surf->render->compositor->scene_height < txh->height)) {
			surf->render->compositor->scene_width = txh->width;
			surf->render->compositor->scene_height = txh->height;
			surf->render->compositor->msg_type |= GF_SR_CFG_OVERRIDE_SIZE;
			return;
		}
	}
	
	/*this should never happen but we check for float rounding safety*/
	if (final.width<=0 || final.height <=0) return;

	w_scale = final.width / txh->width;
	h_scale = final.height / txh->height;

	/*take care of pixel rounding for odd width/height and make sure we strictly draw in the clipped bounds*/
	cur_width = surf->render->cur_width;
	cur_height = surf->render->cur_height;

	if (surf->center_coords) {
		if (cur_width % 2) {
			clipped_final.x += (cur_width-1) / 2;
			final.x += INT2FIX( (cur_width-1) / 2 );
		} else {
			clipped_final.x += cur_width / 2;
			final.x += INT2FIX( cur_width / 2 );
		}
		if (cur_height % 2) {
			clipped_final.y = (cur_height-1) / 2 - clipped_final.y;
			final.y = INT2FIX( (cur_height - 1) / 2) - final.y;
		} else {
			clipped_final.y = cur_height/ 2 - clipped_final.y;
			final.y = INT2FIX( cur_height / 2) - final.y;
		}
	} else {
		final.x -= surf->render->offset_x;
		clipped_final.x -= surf->render->offset_x;
		final.y -= surf->render->offset_y + final.height;
		clipped_final.y -= surf->render->offset_y + clipped_final.height;
	}

	/*make sure we lie in the final rect (this is needed for directRender mode)*/
	if (clipped_final.x<0) {
		clipped_final.width += clipped_final.x;
		clipped_final.x = 0;
		if (clipped_final.width <= 0) return;
	}
	if (clipped_final.y<0) {
		clipped_final.height += clipped_final.y;
		clipped_final.y = 0;
		if (clipped_final.height <= 0) return;
	}
	if (clipped_final.x + clipped_final.width > (s32) cur_width) {
		clipped_final.width = cur_width - clipped_final.x;
		clipped_final.x = cur_width - clipped_final.width;
	}
	if (clipped_final.y + clipped_final.height > (s32) cur_height) {
		clipped_final.height = cur_height - clipped_final.y;
		clipped_final.y = cur_height - clipped_final.height;
	}
	/*needed in direct rendering since clipping is not performed*/
	if (clipped_final.width<=0 || clipped_final.height <=0) 
		return;

	/*compute X offset in src bitmap*/
	start_x = 0;
	tmp = INT2FIX(clipped_final.x);
	if (tmp >= final.x)
		start_x = FIX2INT( gf_divfix(tmp - final.x, w_scale) );


	/*compute Y offset in src bitmap*/
	start_y = 0;
	tmp = INT2FIX(clipped_final.y);
	if (tmp >= final.y)
		start_y = FIX2INT( gf_divfix(tmp - final.y, h_scale) );
	
	/*add AR offset */
	clipped_final.x += surf->render->offset_x;
	clipped_final.y += surf->render->offset_y;

	dst_wnd.x = (u32) clipped_final.x;
	dst_wnd.y = (u32) clipped_final.y;
	dst_wnd.w = (u32) clipped_final.width;
	dst_wnd.h = (u32) clipped_final.height;

	src_wnd.w = FIX2INT( gf_divfix(INT2FIX(dst_wnd.w), w_scale) );
	src_wnd.h = FIX2INT( gf_divfix(INT2FIX(dst_wnd.h), h_scale) );
	if (src_wnd.w>txh->width) src_wnd.w=txh->width;
	if (src_wnd.h>txh->height) src_wnd.h=txh->height;
	
	src_wnd.x = start_x;
	src_wnd.y = start_y;


	if (!src_wnd.w || !src_wnd.h) return;
	/*make sure we lie in src bounds*/
	if (src_wnd.x + src_wnd.w>txh->width) src_wnd.w = txh->width - src_wnd.x;
	if (src_wnd.y + src_wnd.h>txh->height) src_wnd.h = txh->height - src_wnd.y;

	use_soft_stretch = 1;
	if (!cmat && (alpha==0xFF) && surf->render->compositor->video_out->Blit) {
		u32 hw_caps = surf->render->compositor->video_out->hw_caps;
		/*get the right surface and copy the part of the image on it*/
		switch (txh->pixelformat) {
		case GF_PIXEL_RGB_24:
		case GF_PIXEL_BGR_24:
			use_soft_stretch = 0;
			break;
		case GF_PIXEL_YV12:
		case GF_PIXEL_IYUV:
		case GF_PIXEL_I420:
			if (surf->render->enable_yuv_hw && (hw_caps & GF_VIDEO_HW_HAS_YUV))
				use_soft_stretch = 0;
			break;
		default:
			break;
		}
		if (col_key && (GF_COL_A(*col_key) || !(hw_caps & GF_VIDEO_HW_HAS_COLOR_KEY))) use_soft_stretch = 1;
	}

	/*most graphic cards can't perform bliting on locked surface - force unlock by releasing the hardware*/
	VS2D_TerminateSurface(surf);

	video_src.height = txh->height;
	video_src.width = txh->width;
	video_src.pitch = txh->stride;
	video_src.pixel_format = txh->pixelformat;
	video_src.video_buffer = txh->data;
	if (!use_soft_stretch) {
		e = surf->render->compositor->video_out->Blit(surf->render->compositor->video_out, &video_src, &src_wnd, &dst_wnd, col_key);
		/*HW pb, try soft*/
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RENDER, ("[Render 2D] Error during hardware blit - trying with soft one\n"));
			use_soft_stretch = 1;
		}
	}
	if (use_soft_stretch) {
		GF_VideoSurface backbuffer;
		e = surf->render->compositor->video_out->LockBackBuffer(surf->render->compositor->video_out, &backbuffer, 1);
		gf_stretch_bits(&backbuffer, &video_src, &dst_wnd, &src_wnd, 0, alpha, 0, col_key, cmat);
		e = surf->render->compositor->video_out->LockBackBuffer(surf->render->compositor->video_out, &backbuffer, 0);
	}
	VS2D_InitSurface(surf);
}

GF_Err R2D_LoadRenderer(GF_VisualRenderer *vr, GF_Renderer *compositor)
{
	Render2D *sr;
	const char *sOpt;
	if (vr->user_priv) return GF_BAD_PARAM;

	GF_SAFEALLOC(sr, Render2D);
	if (!sr) return GF_OUT_OF_MEM;

	sr->compositor = compositor;

	sr->strike_bank = gf_list_new();
	sr->surfaces_2D = gf_list_new();

	GF_SAFEALLOC(sr->top_effect, RenderEffect2D);
	sr->top_effect->sensors = gf_list_new();
	sr->sensors = gf_list_new();
	
	/*and create main surface*/
	sr->surface = NewVisualSurface2D();
	sr->surface->GetSurfaceAccess = R2D_GetSurfaceAccess;
	sr->surface->ReleaseSurfaceAccess = R2D_ReleaseSurfaceAccess;

	sr->surface->DrawBitmap = R2D_DrawBitmap;
	sr->surface->SupportsFormat = R2D_SupportsFormat;
	sr->surface->render = sr;
	gf_list_add(sr->surfaces_2D, sr->surface);

	sr->zoom = sr->scale_x = sr->scale_y = FIX_ONE;
	vr->user_priv = sr;

	sOpt = gf_cfg_get_key(compositor->user->config, "Render2D", "FocusHighlightFill");
	if (sOpt) sscanf(sOpt, "%x", &sr->highlight_fill);
	sOpt = gf_cfg_get_key(compositor->user->config, "Render2D", "FocusHighlightStroke");
	if (sOpt) sscanf(sOpt, "%x", &sr->highlight_stroke);
	else sr->highlight_stroke = 0xFF000000;

	/*create a drawable for focus highlight*/
	sr->focus_highlight = drawable_new();
	/*associate a dummy node for rendering*/
	sr->focus_highlight->node = gf_node_new(NULL, TAG_UndefinedNode);
	gf_node_register(sr->focus_highlight->node, NULL);
	gf_node_set_callback_function(sr->focus_highlight->node, drawable_render_focus);

	/*load options*/
	sOpt = gf_cfg_get_key(compositor->user->config, "Render2D", "DirectRender");
	if (sOpt && ! stricmp(sOpt, "yes")) 
		sr->top_effect->trav_flags |= TF_RENDER_DIRECT;
	else
		sr->top_effect->trav_flags &= ~TF_RENDER_DIRECT;
	
	sOpt = gf_cfg_get_key(compositor->user->config, "Render2D", "ScalableZoom");
	sr->scalable_zoom = (!sOpt || !stricmp(sOpt, "yes") ) ? 1 : 0;
	sOpt = gf_cfg_get_key(compositor->user->config, "Render2D", "DisableYUV");
	sr->enable_yuv_hw = (sOpt && !stricmp(sOpt, "yes") ) ? 0 : 1;
	return GF_OK;
}



void R2D_UnloadRenderer(GF_VisualRenderer *vr)
{
	Render2D *sr = (Render2D *)vr->user_priv;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render2D] Destroying 2D renderer\n"));
	
	gf_node_unregister(sr->focus_highlight->node, NULL);
	drawable_del_ex(sr->focus_highlight, sr);

	DeleteVisualSurface2D(sr->surface);
	gf_list_del(sr->sensors);
	gf_list_del(sr->surfaces_2D);
	gf_list_del(sr->strike_bank);
	effect_delete(sr->top_effect);

	free(sr);
	vr->user_priv = NULL;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render2D] 2D renderer destroyed\n"));
}


GF_Err R2D_AllocTexture(GF_TextureHandler *hdl)
{
	if (hdl->hwtx) return GF_BAD_PARAM;
	hdl->hwtx = hdl->compositor->r2d->stencil_new(hdl->compositor->r2d, GF_STENCIL_TEXTURE);
	return GF_OK;
}

void R2D_ReleaseTexture(GF_TextureHandler *hdl)
{
	if (hdl->hwtx) hdl->compositor->r2d->stencil_delete(hdl->hwtx);
	hdl->hwtx = NULL;
}

GF_Err R2D_SetTextureData(GF_TextureHandler *hdl)
{
	Render2D *sr = (Render2D *) hdl->compositor->visual_renderer->user_priv;
	return hdl->compositor->r2d->stencil_set_texture(hdl->hwtx, hdl->data, hdl->width, hdl->height, hdl->stride, (GF_PixelFormat) hdl->pixelformat, (GF_PixelFormat) sr->compositor->video_out->pixel_format, 0);
}

/*no module used HW for texturing for now*/
void R2D_TextureHWReset(GF_TextureHandler *hdl)
{
	return;
}

void R2D_GraphicsReset(GF_VisualRenderer *vr)
{
}

void R2D_ReloadConfig(GF_VisualRenderer *vr)
{
	const char *sOpt;
	Render2D *sr = (Render2D *)vr->user_priv;

	gf_sr_lock(sr->compositor, 1);

	sOpt = gf_modules_get_option((GF_BaseInterface *)vr, "Render2D", "DirectRender");

	if (sOpt && !stricmp(sOpt, "yes") ) {
		sr->top_effect->trav_flags |= TF_RENDER_DIRECT;
	} else {
		sr->top_effect->trav_flags &= ~TF_RENDER_DIRECT;
	}

	sOpt = gf_modules_get_option((GF_BaseInterface *)vr, "Render2D", "ScalableZoom");
	sr->scalable_zoom = (!sOpt || !stricmp(sOpt, "yes") ) ? 1 : 0;
	sOpt = gf_modules_get_option((GF_BaseInterface *)vr, "Render2D", "DisableYUV");
	sr->enable_yuv_hw = (sOpt && !stricmp(sOpt, "yes") ) ? 0 : 1;

	sr->compositor->msg_type |= GF_SR_CFG_AR;
	sr->compositor->draw_next_frame = 1;
	gf_sr_lock(sr->compositor, 0);
}

GF_Err R2D_SetOption(GF_VisualRenderer *vr, u32 option, u32 value)
{
	Render2D *sr = (Render2D *)vr->user_priv;
	switch (option) {
	case GF_OPT_DIRECT_RENDER:
		gf_sr_lock(sr->compositor, 1);
		if (value) {
			sr->top_effect->trav_flags |= TF_RENDER_DIRECT;
		} else {
			sr->top_effect->trav_flags &= ~TF_RENDER_DIRECT;
		}
		/*force redraw*/
		gf_sr_invalidate(sr->compositor, NULL);
		gf_sr_lock(sr->compositor, 0);
		return GF_OK;
	case GF_OPT_SCALABLE_ZOOM:
		sr->scalable_zoom = value;
		/*emulate size message to force AR recompute*/
		sr->compositor->msg_type |= GF_SR_CFG_AR;
		return GF_OK;
	case GF_OPT_YUV_HARDWARE:
		sr->enable_yuv_hw = value;
		return GF_OK;
	case GF_OPT_RELOAD_CONFIG: R2D_ReloadConfig(vr); return GF_OK;
	case GF_OPT_ORIGINAL_VIEW: 
		R2D_SetUserTransform(sr, FIX_ONE, 0, 0, 1);
		return GF_OK;
	case GF_OPT_NAVIGATION_TYPE: 
		R2D_SetUserTransform(sr, FIX_ONE, 0, 0, 1);
		return GF_OK;
	case GF_OPT_NAVIGATION:
		if (sr->navigation_disabled) return GF_NOT_SUPPORTED;
		if ((value!=GF_NAVIGATE_NONE) && (value!=GF_NAVIGATE_SLIDE)) return GF_NOT_SUPPORTED;
		sr->navigate_mode = value;
		return GF_OK;
	case GF_OPT_HEADLIGHT: return GF_NOT_SUPPORTED;
	case GF_OPT_COLLISION: return GF_NOT_SUPPORTED;
	case GF_OPT_GRAVITY: return GF_NOT_SUPPORTED;
	default: return GF_BAD_PARAM;
	}
}

u32 R2D_GetOption(GF_VisualRenderer *vr, u32 option)
{
	Render2D *sr = (Render2D *)vr->user_priv;
	switch (option) {
	case GF_OPT_DIRECT_RENDER: return (sr->top_effect->trav_flags & TF_RENDER_DIRECT) ? 1 : 0;
	case GF_OPT_SCALABLE_ZOOM: return sr->scalable_zoom;
	case GF_OPT_YUV_HARDWARE: return sr->enable_yuv_hw;
	case GF_OPT_YUV_FORMAT: return sr->enable_yuv_hw ? sr->compositor->video_out->yuv_pixel_format : 0;
	case GF_OPT_NAVIGATION_TYPE: return sr->navigation_disabled ? GF_NAVIGATE_TYPE_NONE : GF_NAVIGATE_TYPE_2D;
	case GF_OPT_NAVIGATION: return sr->navigate_mode;
	case GF_OPT_HEADLIGHT: return 0;
	case GF_OPT_COLLISION: return GF_COLLISION_NONE;
	case GF_OPT_GRAVITY: return 0;
	default: return 0;
	}
}


/*render inline scene*/
static void R2D_RenderInlineMPEG4(GF_VisualRenderer *vr, GF_Node *inline_parent, GF_Node *inline_root, void *rs)
{
	Bool use_pm;
	u32 h, w;
	GF_Matrix2D mx_bck, mx;
	GF_SceneGraph *in_scene;
	RenderEffect2D *eff = (RenderEffect2D *)rs;

	in_scene = gf_node_get_graph(inline_root);
	use_pm = gf_sg_use_pixel_metrics(in_scene);
	if (use_pm == eff->is_pixel_metrics) {
		gf_node_render(inline_root, rs);
		return;
	}
	gf_mx2d_copy(mx_bck, eff->transform);
	/*override aspect ratio if any size info is given in the scene*/
	if (gf_sg_get_scene_size_info(in_scene, &w, &h)) {
		Fixed scale = INT2FIX( MIN(w, h) / 2);
		if (scale) eff->min_hsize = scale;
	}
	gf_mx2d_init(mx);
	/*apply meterMetrics<->pixelMetrics scale*/
	if (!use_pm) {
		gf_mx2d_add_scale(&mx, eff->min_hsize, eff->min_hsize);
	} else {
		Fixed inv_scale = gf_invfix(eff->min_hsize);
		gf_mx2d_add_scale(&mx, inv_scale, inv_scale);
	}
	eff->is_pixel_metrics = use_pm;
	gf_mx2d_add_matrix(&eff->transform, &mx);
	gf_node_render(inline_root, rs);
	eff->is_pixel_metrics = !use_pm;
	gf_mx2d_copy(eff->transform, mx_bck);
}

void R2D_RenderInline(GF_VisualRenderer *vr, GF_Node *inline_parent, GF_Node *inline_root, void *rs)
{
	switch (gf_node_get_tag(inline_parent)) {
#ifndef GPAC_DISABLE_SVG

#ifdef GPAC_ENABLE_SVG_SA
	case TAG_SVG_SA_animation:
	{
		u32 tag = gf_node_get_tag(inline_root);
		if ((tag>=GF_NODE_RANGE_FIRST_SVG_SA) && (tag<=GF_NODE_RANGE_LAST_SVG_SA)) {
			R2D_render_svg_sa_animation(inline_parent, inline_root, rs);
		} else {
			RenderEffect2D *eff = (RenderEffect2D *)rs;
			GF_Matrix2D mx, bck;
			gf_mx2d_copy(bck, eff->transform);
			gf_mx2d_init(mx);
			/*match both coordinate systems:
			1- we decide SVG center is aligned with BIFS center, so no translation
			2- since parent is SVG (top-left origin), flip the entire subscene
			*/
			gf_mx2d_add_scale(&mx, 1, -1);
			gf_mx2d_pre_multiply(&eff->transform, &mx);
			R2D_RenderInlineMPEG4(vr, inline_parent, inline_root, rs);
			gf_mx2d_copy(eff->transform, bck);
		}
	}
		break;
	case TAG_SVG_SA_use:
		R2D_render_svg_sa_use(inline_parent, inline_root, rs);
		break;
#endif

#ifdef GPAC_ENABLE_SVG_SANI
	case TAG_SVG_SANI_use:
		r2d_render_svg_sani_use(inline_parent, inline_root, rs);
		break;
#endif
	case TAG_SVG_use:
		r2d_render_svg_use(inline_parent, inline_root, rs);
		break;
#endif
	default:
		if (!inline_root) return;
		R2D_RenderInlineMPEG4(vr, inline_parent, inline_root, rs);
		break;
	}
}

GF_Err R2D_GetScreenBuffer(GF_VisualRenderer *vr, GF_VideoSurface *framebuffer)
{
	Render2D *sr = (Render2D *)vr->user_priv;
	return sr->compositor->video_out->LockBackBuffer(sr->compositor->video_out, framebuffer, 1);
}

GF_Err R2D_ReleaseScreenBuffer(GF_VisualRenderer *vr, GF_VideoSurface *framebuffer)
{
	Render2D *sr = (Render2D *)vr->user_priv;
	return sr->compositor->video_out->LockBackBuffer(sr->compositor->video_out, framebuffer, 0);
}

static Bool R2D_ScriptAction(GF_VisualRenderer *vr, u32 type, GF_Node *n, GF_JSAPIParam *param)
{
	Render2D *sr = (Render2D *)vr->user_priv;
	
	switch (type) {
	case GF_JSAPI_OP_GET_SCALE: param->val = sr->zoom; return 1;
	case GF_JSAPI_OP_SET_SCALE: R2D_SetUserTransform(sr, param->val, sr->trans_x, sr->trans_y, 0); return 1;
	case GF_JSAPI_OP_GET_ROTATION: param->val = gf_divfix(180*sr->rotation, GF_PI); return 1;
	case GF_JSAPI_OP_SET_ROTATION: sr->rotation = gf_mulfix(GF_PI, param->val/180); R2D_SetUserTransform(sr, sr->zoom, sr->trans_x, sr->trans_y, 0); return 1;
	case GF_JSAPI_OP_GET_TRANSLATE: param->pt.x = sr->trans_x; param->pt.y = sr->trans_y; return 1;
	case GF_JSAPI_OP_SET_TRANSLATE: R2D_SetUserTransform(sr, sr->zoom, param->pt.x, param->pt.y, 0); return 1;
	/*FIXME - better SMIL timelines support*/
	case GF_JSAPI_OP_GET_TIME: param->time = gf_node_get_scene_time(n); return 1;
	case GF_JSAPI_OP_SET_TIME: /*seek_to(param->time);*/ return 0;
	/*FIXME - this will not work for inline docs, we will have to store the clipper at the <svg> level*/
	case GF_JSAPI_OP_GET_VIEWPORT: 
		param->rc = gf_rect_ft(&sr->surface->top_clipper); 
		if (!sr->surface->center_coords) param->rc.y = param->rc.height - param->rc.y;
		return 1;
	case GF_JSAPI_OP_SET_FOCUS: sr->focus_node = param->node; return 1;
	case GF_JSAPI_OP_GET_FOCUS: param->node = sr->focus_node; return 1;

	/*same routine: traverse tree from root to target, collecting both bounds and transform matrix*/
	case GF_JSAPI_OP_GET_LOCAL_BBOX:
	case GF_JSAPI_OP_GET_SCREEN_BBOX:
	case GF_JSAPI_OP_GET_TRANSFORM:
	{
		RenderEffect2D eff;
		memset(&eff, 0, sizeof(eff));
		eff.traversing_mode = TRAVERSE_GET_BOUNDS;
		eff.surface = sr->surface;
		eff.for_node = n;
		gf_mx2d_init(eff.transform);
		gf_node_render(gf_sg_get_root_node(sr->compositor->scene), &eff);
		if (type==GF_JSAPI_OP_GET_LOCAL_BBOX) gf_bbox_from_rect(&param->bbox, &eff.bounds);
		else if (type==GF_JSAPI_OP_GET_TRANSFORM) gf_mx_from_mx2d(&param->mx, &eff.transform);
		else {
			gf_mx2d_apply_rect(&eff.transform, &eff.bounds);
			gf_bbox_from_rect(&param->bbox, &eff.bounds);
		}
	}
		return 1;
	case GF_JSAPI_OP_LOAD_URL:
	{
		GF_Node *target;
		char *sub_url = strrchr(param->uri.url, '#');
		if (!sub_url) return 0;
		target = gf_sg_find_node_by_name(gf_node_get_graph(n), sub_url+1);
		if (target && (gf_node_get_tag(target)==TAG_MPEG4_Viewport) ) {
			((M_Viewport *)target)->set_bind = 1;
			((M_Viewport *)target)->on_set_bind(n);
			return 1;
		}
		return 0;
	}
	}
	return 0;
}

GF_Err R2D_GetViewport(GF_VisualRenderer *vr, u32 viewpoint_idx, const char **outName, Bool *is_bound);
GF_Err R2D_SetViewport(GF_VisualRenderer *vr, u32 viewpoint_idx, const char *viewpoint_name);

GF_VisualRenderer *NewVisualRenderer()
{
	GF_VisualRenderer *sr;	
	GF_SAFEALLOC(sr, GF_VisualRenderer);
	if (!sr) return NULL;

	sr->LoadRenderer = R2D_LoadRenderer;
	sr->UnloadRenderer = R2D_UnloadRenderer;
	sr->GraphicsReset = R2D_GraphicsReset;
	sr->NodeChanged = R2D_NodeChanged;
	sr->NodeInit = R2D_NodeInit;
	sr->DrawScene = R2D_DrawScene;
	sr->ExecuteEvent = R2D_ExecuteEvent;
	sr->RecomputeAR = R2D_RecomputeAR;
	sr->SceneReset = R2D_SceneReset;
	sr->RenderInline = R2D_RenderInline;
	sr->AllocTexture = R2D_AllocTexture;
	sr->ReleaseTexture = R2D_ReleaseTexture;
	sr->SetTextureData = R2D_SetTextureData;
	sr->TextureHWReset = R2D_TextureHWReset;
	sr->SetOption = R2D_SetOption;
	sr->GetOption = R2D_GetOption;
	sr->GetScreenBuffer = R2D_GetScreenBuffer;
	sr->ReleaseScreenBuffer = R2D_ReleaseScreenBuffer;
	sr->GetViewpoint = R2D_GetViewport;
	sr->SetViewpoint = R2D_SetViewport;
	sr->ScriptAction = R2D_ScriptAction;

	sr->user_priv = NULL;
	return sr;
}

#ifndef GPAC_STANDALONE_RENDER_2D

/*interface create*/
GF_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	GF_VisualRenderer *sr;
	if (InterfaceType != GF_RENDERER_INTERFACE) return NULL;
	
	GF_SAFEALLOC(sr, GF_VisualRenderer);
	if (!sr) return NULL;

	GF_REGISTER_MODULE_INTERFACE(sr, GF_RENDERER_INTERFACE, "GPAC 2D Renderer", "gpac distribution");

	sr->LoadRenderer = R2D_LoadRenderer;
	sr->UnloadRenderer = R2D_UnloadRenderer;
	sr->GraphicsReset = R2D_GraphicsReset;
	sr->NodeChanged = R2D_NodeChanged;
	sr->NodeInit = R2D_NodeInit;
	sr->DrawScene = R2D_DrawScene;
	sr->ExecuteEvent = R2D_ExecuteEvent;
	sr->RecomputeAR = R2D_RecomputeAR;
	sr->SceneReset = R2D_SceneReset;
	sr->RenderInline = R2D_RenderInline;
	sr->AllocTexture = R2D_AllocTexture;
	sr->ReleaseTexture = R2D_ReleaseTexture;
	sr->SetTextureData = R2D_SetTextureData;
	sr->TextureHWReset = R2D_TextureHWReset;
	sr->SetOption = R2D_SetOption;
	sr->GetOption = R2D_GetOption;
	sr->GetScreenBuffer = R2D_GetScreenBuffer;
	sr->ReleaseScreenBuffer = R2D_ReleaseScreenBuffer;
	sr->GetViewpoint = R2D_GetViewport;
	sr->SetViewpoint = R2D_SetViewport;
	sr->ScriptAction = R2D_ScriptAction;

	sr->user_priv = NULL;
	return (GF_BaseInterface *)sr;
}


/*interface destroy*/
GF_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	GF_VisualRenderer *rend = (GF_VisualRenderer *)ifce;
	if (rend->InterfaceType != GF_RENDERER_INTERFACE) return;
	if (rend->user_priv) R2D_UnloadRenderer(rend);
	free(rend);
}

/*interface query*/
GF_EXPORT
Bool QueryInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_RENDERER_INTERFACE) return 1;
	return 0;
}

#endif

