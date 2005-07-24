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


#include "render3d.h"
#include "visual_surface.h"
#include "render3d_nodes.h"
#include <gpac/options.h>

void effect3d_reset(RenderEffect3D *eff)
{
	GF_List *sbck = eff->sensors;
	GF_List *dbck = eff->local_lights;
	memset(eff, 0, sizeof(RenderEffect3D));
	gf_mx_init(eff->model_matrix);
	gf_cmx_init(&eff->color_mat);
	eff->sensors = sbck;
	eff->local_lights = dbck;
	gf_list_reset(eff->sensors);

	while (gf_list_count(eff->local_lights)) {
		DLightContext *dl = gf_list_get(eff->local_lights, 0);
		gf_list_rem(eff->local_lights, 0);
		free(dl);
	}
}

RenderEffect3D *effect3d_new()
{
	RenderEffect3D *eff;
	GF_SAFEALLOC(eff, sizeof(RenderEffect3D));
	eff->sensors = gf_list_new();
	eff->local_lights = gf_list_new();
	return eff;
}

void effect3d_delete(RenderEffect3D *eff)
{
	effect3d_reset(eff);
	gf_list_del(eff->sensors);
	gf_list_del(eff->local_lights);
	free(eff);
}


GF_Rect R3D_UpdateClipper(RenderEffect3D *eff, GF_Rect this_clip, Bool *need_restore, GF_Rect *original, Bool for_layer)
{
	GF_Rect clip, orig;
	if (for_layer) {
		orig = eff->layer_clipper;
		*need_restore = eff->has_layer_clip;
	} else {
		orig = eff->clipper;
		*need_restore = eff->has_clip;
	}
	*original = orig;

	clip = this_clip;
	if (*need_restore) {
		GF_Matrix mx;
		gf_mx_copy(mx, eff->model_matrix);
		gf_mx_inverse(&mx);
		gf_mx_apply_rect(&mx, &orig);
		if (clip.x < orig.x) {
			clip.width -= (orig.x - clip.x);
			clip.x = orig.x;
		}
		if (clip.x + clip.width > orig.x + orig.width) {
			clip.width = orig.x + orig.width - clip.x;
		}
		if (clip.y > orig.y) {
			clip.height -= (clip.y - orig.y);
			clip.y = orig.y;
		}
		if (clip.y - clip.height < orig.y - orig.height) {
			clip.height = clip.y - orig.y + orig.height;
		}
	}
	if (for_layer) {
		eff->layer_clipper = clip;
		eff->has_layer_clip = 1;
	} else {
		eff->clipper = clip;
		/*retranslate to world coords*/
		gf_mx_apply_rect(&eff->model_matrix, &eff->clipper);
		eff->has_clip = 1;
	}
	return clip;
}


Bool R3D_GetSurfaceSizeInfo(RenderEffect3D *eff, Fixed *surf_width, Fixed *surf_height)
{
	u32 w, h;
	w = eff->surface->width;
	h = eff->surface->height;
	/*no size info, use main render output size*/
	if (!w || !h) {
		w = eff->surface->render->out_width;
		h = eff->surface->render->out_height;
	}
	if (eff->is_pixel_metrics) {
		*surf_width = INT2FIX(w);
		*surf_height = INT2FIX(h);
		return 1;
	}
	if (h > w) {
		*surf_width = 2*FIX_ONE;
		*surf_height = gf_divfix(2*INT2FIX(h), INT2FIX(w));
	} else {
		*surf_width = gf_divfix(2*INT2FIX(w), INT2FIX(h));
		*surf_height = 2*FIX_ONE;
	}
	return 0;
}

SensorHandler *r3d_get_sensor_handler(GF_Node *n)
{
	SensorHandler *hs;
	if (!n) return NULL;

	switch (gf_node_get_tag(n)) {
	case TAG_MPEG4_DiscSensor: hs = r3d_ds_get_handler(n); break;
	case TAG_MPEG4_PlaneSensor2D: hs = r3d_ps2D_get_handler(n); break;
	case TAG_MPEG4_ProximitySensor2D: hs = r3d_prox2D_get_handler(n); break;

	case TAG_MPEG4_Anchor: case TAG_X3D_Anchor: hs = r3d_anchor_get_handler(n); break;
	case TAG_MPEG4_TouchSensor: case TAG_X3D_TouchSensor: hs = r3d_touch_sensor_get_handler(n); break;
	case TAG_MPEG4_PlaneSensor: case TAG_X3D_PlaneSensor: hs = r3d_ps_get_handler(n); break;
	case TAG_MPEG4_CylinderSensor: case TAG_X3D_CylinderSensor: hs = r3d_cs_get_handler(n); break;
	case TAG_MPEG4_SphereSensor: case TAG_X3D_SphereSensor: hs = r3d_sphere_get_handler(n); break;
	default: return NULL;
	}
	if (hs && hs->IsEnabled(n)) return hs;
	return NULL;
}


void R3D_SensorDeleted(GF_Renderer *rend, SensorHandler *hdl)
{
	Render3D *sr = (Render3D *)rend->visual_renderer->user_priv;
	gf_list_del_item(sr->prev_sensors, hdl);
	if (rend->interaction_sensors) rend->interaction_sensors--;
}

void R3D_SetGrabbed(GF_Renderer *rend, Bool bOn) 
{
	((Render3D *)rend->visual_renderer->user_priv)->is_grabbed = bOn; 
}

GF_Node *R3D_PickNode(GF_VisualRenderer *vr, s32 X, s32 Y)
{
	return NULL;
}

static void DestroyLineProps(GF_Node *n)
{
	u32 i;
	LinePropStack *st = gf_node_get_private(n);
	Render3D *sr = (Render3D *)st->sr;
	
	for (i=0; i<gf_list_count(sr->strike_bank); i++) {
		StrikeInfo *si = gf_list_get(sr->strike_bank, i);
		if (si->lineProps == n) {
			/*remove from node*/
			if (si->node2D) {
				stack2D *st = (stack2D *) gf_node_get_private(si->node2D);
				gf_list_del_item(st->strike_list, si);
			}
			gf_list_rem(sr->strike_bank, i);
			delete_strikeinfo(si);
			i--;
		}
	}
	free(st);
}

void R3D_InitLineProps(Render3D *sr, GF_Node *node)
{
	LinePropStack *st = malloc(sizeof(LinePropStack));
	st->sr = sr;
	st->last_mod_time = 1;
	gf_node_set_private(node, st);
	gf_node_set_predestroy_function(node, DestroyLineProps);
}

u32 R3D_LP_GetLastUpdateTime(GF_Node *node)
{
	LinePropStack *st = gf_node_get_private(node);
	if (!st) return 0;
	if (gf_node_dirty_get(node) & GF_SG_NODE_DIRTY) {
		st->last_mod_time ++;
		gf_node_dirty_clear(node, 0);
	}
	return st->last_mod_time;
}


void R3D_DrawScene(GF_VisualRenderer *vr)
{
	u32 i, tag;
	RenderEffect3D static_eff;
	Render3D *sr = (Render3D *)vr->user_priv;
	GF_Node *top_node = NULL;
	
	if (sr->compositor->scene) top_node = gf_sg_get_root_node(sr->compositor->scene);

	/*setup GL*/
	VS3D_Setup(sr->surface);

	memcpy(&static_eff, sr->top_effect, sizeof(RenderEffect3D));

	if (top_node) {
		if (!sr->main_surface_setup) {
			tag = gf_node_get_tag(top_node);
			sr->surface->width = sr->compositor->scene_width;
			sr->surface->height = sr->compositor->scene_height;
			if ((tag>=GF_NODE_RANGE_FIRST_X3D) && (tag<=GF_NODE_RANGE_LAST_X3D)) {
				sr->surface->camera.is_3D = sr->root_is_3D = 1;
			} else {
				sr->surface->camera.is_3D = sr->root_is_3D = ((tag==TAG_MPEG4_Group) || (tag==TAG_MPEG4_Layer3D) ) ? 1 : 0;
			}
			camera_invalidate(&sr->surface->camera);
			sr->main_surface_setup = 1;
		}
		sr->top_effect->is_pixel_metrics = gf_sg_use_pixel_metrics(sr->compositor->scene);
		/*setup our effects*/
		VS_SetupEffects(sr->surface, sr->top_effect);
		VS_NodeRender(sr->top_effect, top_node);

		sr->top_effect->surface = NULL;
	}

	for (i=0; i<gf_list_count(sr->compositor->extra_scenes); i++) {
		GF_SceneGraph *sg = gf_list_get(sr->compositor->extra_scenes, i);
		GF_Node *n = gf_sg_get_root_node(sg);
		if (!n) continue;
		
		tag = gf_node_get_tag(n);
		if (!sr->main_surface_setup) {
			sr->surface->width = sr->compositor->scene_width;
			sr->surface->height = sr->compositor->scene_height;
		}
/*		if ((tag>=GF_NODE_RANGE_FIRST_X3D) && (tag<=GF_NODE_RANGE_LAST_X3D)) {
			sr->surface->camera.is_3D = 1;
		} else {
			sr->surface->camera.is_3D = ((tag==TAG_MPEG4_Group) || (tag==TAG_MPEG4_Layer3D) ) ? 1 : 0;
		}
		sr->surface->camera.flags |= CAM_IS_DIRTY;
*/		sr->top_effect->is_pixel_metrics = gf_sg_use_pixel_metrics(sg);
		VS_SetupEffects(sr->surface, sr->top_effect);
		//VS_NodeRender(sr->top_effect, n);
		sr->top_effect->traversing_mode = TRAVERSE_SORT;
		gf_node_render(n, sr->top_effect);
	}
	//if (i) sr->main_surface_setup = 0;
	memcpy(sr->top_effect, &static_eff, sizeof(RenderEffect3D));
	
	if (!i && !top_node) {
		/*default color is VRML default background color*/
		SFColor c;
		c.red = c.green = c.blue = 0;
		VS3D_ClearSurface(sr->surface, c, FIX_ONE);
	}

	sr->compositor->video_out->FlushVideo(sr->compositor->video_out, NULL);
}

static Bool R3D_ExecuteEvent(GF_VisualRenderer *vr, GF_UserEvent *event)
{
	GF_UserEvent evt;
	Render3D *sr = (Render3D *)vr->user_priv;
	/*revert to BIFS like*/
	evt = *event;
	if (evt.event_type<=GF_EVT_LEFTUP) {
		evt.mouse.x = event->mouse.x - sr->compositor->width/2;
		evt.mouse.y = sr->compositor->height/2 - event->mouse.y;
	}
	sr->top_effect->is_pixel_metrics = gf_sg_use_pixel_metrics(sr->compositor->scene);
	/*process regular events*/
	if ((sr->compositor->interaction_level & GF_INTERACT_NORMAL) && VS_ExecuteEvent(sr->surface, sr->top_effect, &evt, NULL)) 
		return 1;
	/*remember active layer on mouse click - may be NULL*/
	if (event->event_type==GF_EVT_LEFTDOWN) sr->active_layer = sr->top_effect->collect_layer;
	/*process navigation events*/
	if (sr->compositor->interaction_level & GF_INTERACT_NAVIGATION) return R3D_HandleUserEvent(sr, &evt);
	return 0;
}

static void R3D_SetScaling(Render3D *sr, Fixed sx, Fixed sy)
{
	sr->scale_x = sx;
	sr->scale_y = sy;
}

GF_Err R3D_RecomputeAR(GF_VisualRenderer *vr)
{
	Double ratio;
	Fixed scaleX, scaleY;
	Render3D *sr = (Render3D *)vr->user_priv;

	sr->surface->camera.flags |= CAM_IS_DIRTY;

	if (!sr->compositor->height || !sr->compositor->width) return GF_OK;

	sr->out_width = sr->compositor->width;
	sr->out_height = sr->compositor->height;
	sr->out_x = 0;
	sr->out_y = 0;

	if (!sr->compositor->has_size_info) {
		R3D_SetScaling(sr, FIX_ONE, FIX_ONE);
		sr->out_width = sr->surface->width = sr->compositor->width;
		sr->out_height = sr->surface->height = sr->compositor->height;
		return GF_OK;
	}

	switch (sr->compositor->aspect_ratio) {
	case GF_ASPECT_RATIO_FILL_SCREEN:
		break;
	case GF_ASPECT_RATIO_16_9:
		sr->out_width = sr->compositor->width;
		sr->out_height = 9 * sr->compositor->width / 16;
		break;
	case GF_ASPECT_RATIO_4_3:
		sr->out_width = sr->compositor->width;
		sr->out_height = 3 * sr->compositor->width / 4;
		break;
	default:
		ratio = sr->compositor->scene_height;
		ratio /= sr->compositor->scene_width;
		if (sr->out_width * ratio > sr->out_height) {
			sr->out_width = sr->out_height * sr->compositor->scene_width;
			sr->out_width /= sr->compositor->scene_height;
		}
		else {
			sr->out_height = sr->out_width * sr->compositor->scene_height;
			sr->out_height /= sr->compositor->scene_width;
		}
		break;
	}
	sr->out_x = (sr->compositor->width - sr->out_width) / 2;
	sr->out_y = (sr->compositor->height - sr->out_height) / 2;
	/*update size info for main surface*/
	if (sr->surface) {
		sr->surface->width = sr->compositor->scene_width;
		sr->surface->height = sr->compositor->scene_height;
	}
	/*scaling is still needed for bitmap*/
	scaleX = gf_divfix(INT2FIX(sr->out_width), INT2FIX(sr->compositor->scene_width));
	scaleY = gf_divfix(INT2FIX(sr->out_height), INT2FIX(sr->compositor->scene_height));
	R3D_SetScaling(sr, scaleX, scaleY);
	return GF_OK;
}


void R3D_SceneReset(GF_VisualRenderer *vr)
{
	Render3D *sr = (Render3D*)vr->user_priv;
	sr->main_surface_setup = 0;
}

void R3D_ReloadConfig(GF_VisualRenderer *vr)
{
	const char *sOpt;
	Render3D *sr = (Render3D *)vr->user_priv;

	gf_sr_lock(sr->compositor, 1);

	sOpt = gf_modules_get_option((GF_BaseInterface *)vr, "Render3D", "RasterOutlines");
	sr->raster_outlines = (sOpt && !stricmp(sOpt, "yes") ) ? 1 : 0;
	sOpt = gf_modules_get_option((GF_BaseInterface *)vr, "Render3D", "EmulatePOW2");
	sr->emul_pow2 = (sOpt && !stricmp(sOpt, "yes") ) ? 1 : 0;
	sOpt = gf_modules_get_option((GF_BaseInterface *)vr, "Render3D", "PolygonAA");
	sr->poly_aa = (sOpt && !stricmp(sOpt, "yes") ) ? 1 : 0;
	sOpt = gf_modules_get_option((GF_BaseInterface *)vr, "Render3D", "DisableBackFaceCulling");
	sr->no_backcull = (sOpt && !stricmp(sOpt, "yes") ) ? 1 : 0;
	sOpt = gf_modules_get_option((GF_BaseInterface *)vr, "Render3D", "Wireframe");
	if (sOpt && !stricmp(sOpt, "WireOnly")) sr->wiremode = GF_WIREFRAME_ONLY;
	else if (sOpt && !stricmp(sOpt, "WireOnSolid")) sr->wiremode = GF_WIREFRAME_SOLID;
	else sr->wiremode = GF_WIREFRAME_NONE;

	sOpt = gf_modules_get_option((GF_BaseInterface *)vr, "Render3D", "BitmapCopyPixels");
	sr->bitmap_use_pixels = (sOpt && !stricmp(sOpt, "yes") ) ? 1 : 0;

	sOpt = gf_modules_get_option((GF_BaseInterface *)vr, "Render3D", "DisableRectExt");
	sr->disable_rect_ext = (sOpt && !stricmp(sOpt, "yes") ) ? 1 : 0;
	/*RECT texture support - we must reload HW*/
	gf_sr_reset_graphics(sr->compositor);

	gf_sr_lock(sr->compositor, 0);
}

GF_Err R3D_LoadRenderer(GF_VisualRenderer *vr, GF_Renderer *compositor)
{
	Render3D *sr;
	if (vr->user_priv) return GF_BAD_PARAM;

	sr = malloc(sizeof(Render3D));
	if (!sr) return GF_OUT_OF_MEM;
	memset(sr, 0, sizeof(Render3D));

	sr->compositor = compositor;
	sr->strike_bank = gf_list_new();
	/*create default unit sphere and box for bounds*/
	sr->unit_bbox = new_mesh();
	mesh_new_unit_bbox(sr->unit_bbox);

	sr->top_effect = effect3d_new();
	sr->sensors = gf_list_new();
	sr->prev_sensors = gf_list_new();

	/*note we create main surface only when attached to scene*/
	sr->surface = VS_New();
	sr->surface->render = sr;
	sr->main_surface_setup = 0;
	/*default collision mode*/
	sr->collide_mode = GF_COLLISION_DISPLACEMENT;
//	sr->collide_mode = GF_COLLISION_NORMAL;
	sr->gravity_on = 1;
	vr->user_priv = sr;

	R3D_ReloadConfig(vr);
	return GF_OK;
}

void R3D_UnloadRenderer(GF_VisualRenderer *vr)
{
	Render3D *sr = (Render3D *)vr->user_priv;
	if (!sr) return;

	assert(!gf_list_count(sr->strike_bank));
	gf_list_del(sr->strike_bank);

	if (sr->unit_bbox) mesh_free(sr->unit_bbox);
	effect3d_delete(sr->top_effect);
	gf_list_del(sr->sensors);
	gf_list_del(sr->prev_sensors);

	VS_Delete(sr->surface);

	free(sr);
	vr->user_priv = NULL;
}

void R3D_GraphicsReset(GF_VisualRenderer *vr)
{
	Render3D *sr = (Render3D *)vr->user_priv;
	if (!sr) return;
	R3D_LoadExtensions(sr);
}

GF_Camera *R3D_GetCamera(Render3D *sr) 
{
	if (sr->active_layer) {
		return l3d_get_camera(sr->active_layer);
	} else {
		return &sr->surface->camera;
	}
}

GF_Err R3D_SetOption(GF_VisualRenderer *vr, u32 option, u32 value)
{
	GF_Node *n;
	GF_Camera *cam;
	Render3D *sr = (Render3D *)vr->user_priv;
	switch (option) {
	case GF_OPT_RASTER_OUTLINES: sr->raster_outlines = value; return GF_OK;
	case GF_OPT_EMULATE_POW2: sr->emul_pow2 = value; return GF_OK;
	case GF_OPT_POLYGON_ANTIALIAS: sr->poly_aa = value; return GF_OK;
	case GF_OPT_NO_BACK_CULL: sr->no_backcull = value; return GF_OK;
	case GF_OPT_WIREFRAME: sr->wiremode = value; return GF_OK;
	case GF_OPT_RELOAD_CONFIG:
		R3D_ReloadConfig(vr);
		return GF_OK;
	case GF_OPT_NO_RECT_TEXTURE:
		if (value != sr->disable_rect_ext) {
			sr->disable_rect_ext = value;
			/*RECT texture support - we must reload HW*/
			gf_sr_reset_graphics(sr->compositor);
		}
		return GF_OK;
	case GF_OPT_BITMAP_COPY: sr->bitmap_use_pixels = value; return GF_OK;
	case GF_OPT_ORIGINAL_VIEW: 
		R3D_ResetCamera(sr);
		return GF_OK;

	case GF_OPT_NAVIGATION_TYPE:
		if (!sr->surface) return GF_BAD_PARAM;
		R3D_ResetCamera(sr);
		return GF_OK;
	case GF_OPT_NAVIGATION: 
		cam = R3D_GetCamera(sr);
		if (cam->navigation_flags & NAV_ANY) {
			/*if not specifying mode, try to (un)bind top*/
			if (!value) {
				if (sr->active_layer) {
					l3d_bind_camera(sr->active_layer, 0, value);
				} else {
					n = gf_list_get(sr->surface->navigation_stack, 0);
					if (n) Bindable_SetSetBind(n, 0);
					else cam->navigate_mode = value;
				}
			} else {
				cam->navigate_mode = value;
			}
			return GF_OK;
		}
		return GF_NOT_SUPPORTED;

	case GF_OPT_HEADLIGHT:
		cam = R3D_GetCamera(sr);
		if (cam->navigation_flags & NAV_ANY) {
			if (value) 
				cam->navigation_flags |= NAV_HEADLIGHT;
			else
				cam->navigation_flags &= ~NAV_HEADLIGHT;
			return GF_OK;
		}
		return GF_NOT_SUPPORTED;
	case GF_OPT_COLLISION: 
		sr->collide_mode = value;
		return GF_OK;
	case GF_OPT_GRAVITY: 
		cam = R3D_GetCamera(sr);
		sr->gravity_on = value;
		/*force collision pass*/
		cam->last_pos.z -= 1;
		gf_sr_invalidate(sr->compositor, NULL);
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

u32 R3D_GetOption(GF_VisualRenderer *vr, u32 option)
{
	GF_Camera *cam;
	Render3D *sr = (Render3D *)vr->user_priv;
	switch (option) {
	case GF_OPT_RASTER_OUTLINES: return sr->raster_outlines;
	case GF_OPT_EMULATE_POW2: return sr->emul_pow2;
	case GF_OPT_POLYGON_ANTIALIAS: return sr->poly_aa;
	case GF_OPT_NO_BACK_CULL: return sr->no_backcull;
	case GF_OPT_WIREFRAME: return sr->wiremode;
	case GF_OPT_NO_RECT_TEXTURE: return sr->disable_rect_ext;
	case GF_OPT_BITMAP_COPY: return sr->bitmap_use_pixels;
	case GF_OPT_NAVIGATION_TYPE: 
		cam = R3D_GetCamera(sr);
		if (!(cam->navigation_flags & NAV_ANY)) return GF_NAVIGATE_TYPE_NONE;
		return ((sr->root_is_3D || sr->active_layer) ? GF_NAVIGATE_TYPE_3D : GF_NAVIGATE_TYPE_2D);
	case GF_OPT_NAVIGATION: 
		cam = R3D_GetCamera(sr);
		return cam->navigate_mode;
	case GF_OPT_HEADLIGHT: 
		cam = R3D_GetCamera(sr);
		return (cam->navigation_flags & NAV_HEADLIGHT) ? 1 : 0;
	case GF_OPT_COLLISION: return sr->collide_mode;
	case GF_OPT_GRAVITY: return sr->gravity_on;
	default: return 0;
	}
}


GF_Err R3D_GetScreenBuffer(GF_VisualRenderer *vr, GF_VideoSurface *framebuffer);
GF_Err R3D_ReleaseScreenBuffer(GF_VisualRenderer *vr, GF_VideoSurface *framebuffer);
GF_Err R3D_GetViewpoint(GF_VisualRenderer *vr, u32 viewpoint_idx, const char **outName, Bool *is_bound);
GF_Err R3D_SetViewpoint(GF_VisualRenderer *vr, u32 viewpoint_idx, const char *viewpoint_name);


/*render inline scene*/
void R3D_RenderInline(GF_VisualRenderer *vr, GF_Node *inline_root, void *rs)
{
	Bool had_scale, use_pm;
	u32 h, w;
	Fixed prev_scale;
	GF_Matrix gf_mx_bck, mx;
	GF_SceneGraph *in_scene;
	RenderEffect3D *eff = (RenderEffect3D *)rs;

	in_scene = gf_node_get_graph(inline_root);
	use_pm = gf_sg_use_pixel_metrics(in_scene);
	if (use_pm == eff->is_pixel_metrics) {
		gf_node_render(inline_root, rs);
		return;
	}
	gf_mx_copy(gf_mx_bck, eff->model_matrix);
	prev_scale = eff->min_hsize;
	had_scale = eff->has_scale;
	/*override aspect ratio if any size info is given in the scene*/
	if (gf_sg_get_scene_size_info(in_scene, &w, &h)) {
		Fixed scale = INT2FIX(MIN(w, h)) / 2;
		if (scale) eff->min_hsize = scale;
	}
	gf_mx_init(mx);
	/*apply meterMetrics<->pixelMetrics scale*/
	if (!use_pm) {
		gf_mx_add_scale(&mx, eff->min_hsize, eff->min_hsize, eff->min_hsize);
	} else {
		Fixed inv_scale = gf_divfix(FIX_ONE, eff->min_hsize);
		gf_mx_add_scale(&mx, inv_scale, inv_scale, inv_scale);
	}
	eff->has_scale = 1;
	eff->is_pixel_metrics = use_pm;
	gf_mx_add_matrix(&eff->model_matrix, &mx);
	if (eff->traversing_mode==TRAVERSE_SORT) {
		VS3D_PushMatrix(eff->surface);
		VS3D_MultMatrix(eff->surface, mx.m);
		gf_node_render(inline_root, rs);
		VS3D_PopMatrix(eff->surface);
	} else {
		gf_node_render(inline_root, rs);
	}
	eff->is_pixel_metrics = !use_pm;
	gf_mx_copy(eff->model_matrix, gf_mx_bck);
	eff->has_scale = had_scale;
}

/*interface create*/
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	GF_VisualRenderer *sr;
	if (InterfaceType != GF_RENDERER_INTERFACE) return NULL;
	
	sr = malloc(sizeof(GF_VisualRenderer));
	if (!sr) return NULL;
	memset(sr, 0, sizeof(GF_VisualRenderer));
	GF_REGISTER_MODULE_INTERFACE(sr, GF_RENDERER_INTERFACE, "GPAC 3D Renderer", "gpac distribution");

	sr->LoadRenderer = R3D_LoadRenderer;
	sr->UnloadRenderer = R3D_UnloadRenderer;
	sr->GraphicsReset = R3D_GraphicsReset;
	sr->NodeChanged = R3D_NodeChanged;
	sr->NodeInit = R3D_NodeInit;
	sr->DrawScene = R3D_DrawScene;
	sr->RenderInline = R3D_RenderInline;
	sr->ExecuteEvent = R3D_ExecuteEvent;
	sr->RecomputeAR = R3D_RecomputeAR;
	sr->SceneReset = R3D_SceneReset;
	sr->AllocTexture = tx_allocate;
	sr->ReleaseTexture = tx_delete;
	sr->SetTextureData = R3D_SetTextureData;
	sr->TextureHWReset = R3D_TextureHWReset;

	sr->SetViewpoint = R3D_SetViewpoint;
	sr->GetViewpoint = R3D_GetViewpoint;

	sr->SetOption = R3D_SetOption;
	sr->GetOption = R3D_GetOption;
	sr->GetScreenBuffer = R3D_GetScreenBuffer;
	sr->ReleaseScreenBuffer = R3D_ReleaseScreenBuffer;

	/*signal we need openGL*/
	sr->bNeedsGL = 1;
	sr->user_priv = NULL;
	return (GF_BaseInterface *)sr;
}


/*interface destroy*/
void ShutdownInterface(GF_BaseInterface *ifce)
{
	GF_VisualRenderer *rend = (GF_VisualRenderer *)ifce;
	if (rend->InterfaceType != GF_RENDERER_INTERFACE) return;
	assert(rend->user_priv==NULL);
	free(rend);
}

/*interface query*/
Bool QueryInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_RENDERER_INTERFACE) return 1;
	return 0;
}


