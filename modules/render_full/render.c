/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / 2D+3D rendering module
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


#include "render.h"
#include "visual_manager.h"
#include "node_stacks.h"

#include "texturing.h"

void render_set_aspect_ratio_scaling(Render *sr, Fixed scaleX, Fixed scaleY)
{
	sr->scale_x = scaleX;
	sr->scale_y = scaleY;
	render_2d_set_user_transform(sr, sr->zoom, sr->trans_x, sr->trans_y, 1);
}

GF_TextureHandler *render_get_texture_handler(GF_Node *n)
{
	if (!n) return NULL;
	switch (gf_node_get_tag(n)) {
	case TAG_MPEG4_CompositeTexture2D: 
	case TAG_MPEG4_CompositeTexture3D: 
		return render_get_composite_texture(n);
	case TAG_MPEG4_LinearGradient: 
	case TAG_MPEG4_RadialGradient: 
		return render_get_gradient_texture(n);

#ifndef GPAC_DISABLE_SVG
	case TAG_SVG_linearGradient: 
	case TAG_SVG_radialGradient: 
		return render_svg_get_gradient_texture(n);
	case TAG_SVG_image:
	case TAG_SVG_video:
		return render_svg_get_image_texture(n);
#endif

	default: return gf_sr_texture_get_handler(n);
	}
}

static Bool render_exec_event(GF_VisualRenderer *vr, GF_Event *evt)
{
	Render *sr = (Render *)vr->user_priv;

	if (evt->type<=GF_EVENT_MOUSEMOVE) {
		if (sr->visual->center_coords) {
			evt->mouse.x = evt->mouse.x - sr->compositor->width/2;
			evt->mouse.y = sr->compositor->height/2 - evt->mouse.y;
		} else {
			evt->mouse.x -= sr->vp_x;
			evt->mouse.y -= sr->vp_y;
		}
	}

	/*process regular events except if navigation is grabbed*/
	if (!sr->navigation_grabbed && (sr->compositor->interaction_level & GF_INTERACT_NORMAL) && render_execute_event(sr, sr->traverse_state, evt, NULL)) 
		return 1;
#ifndef GPAC_DISABLE_3D
	/*remember active layer on mouse click - may be NULL*/
	if ((evt->type==GF_EVENT_MOUSEDOWN) && (evt->mouse.button==GF_MOUSE_LEFT)) sr->active_layer = sr->traverse_state->layer3d;
#endif
	/*process navigation events*/
	if (sr->compositor->interaction_level & GF_INTERACT_NAVIGATION) return render_handle_navigation(sr, evt);
	return 0;
}

static void render_reset_visuals(Render *sr)
{
	GF_VisualManager *surf;
	u32 i=0;
	while ((surf = (GF_VisualManager *)gf_list_enum(sr->visuals, &i))) {
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

static void render_reset(GF_VisualRenderer *vr)
{
	u32 flag;
	Render *sr = (Render*) vr->user_priv;
	if (!sr) return;
	
	render_reset_visuals(sr);
	gf_list_reset(sr->sensors);
	gf_list_reset(sr->previous_sensors);

	/*reset traverse state*/
	flag = sr->traverse_state->trav_flags;
	gf_list_del(sr->traverse_state->vrml_sensors);
	memset(sr->traverse_state, 0, sizeof(GF_TraverseState));
	sr->traverse_state->vrml_sensors = gf_list_new();
	gf_mx2d_init(sr->traverse_state->transform);
	gf_cmx_init(&sr->traverse_state->color_mat);
	sr->traverse_state->trav_flags = flag;

	sr->compositor->reset_graphics = 1;
	sr->trans_x = sr->trans_y = 0;
	sr->zoom = FIX_ONE;
	sr->grab_node = NULL;
	sr->grab_use = NULL;
	sr->focus_node = NULL;

	/*force resetup in case we're switching coord system*/
	sr->root_visual_setup = 0;
	visual_2d_reset_raster(sr->visual);
	
	render_set_aspect_ratio_scaling(sr, sr->scale_x, sr->scale_x);
}


void render_visual_register(Render *sr, GF_VisualManager *surf)
{
	if (render_visual_is_registered(sr, surf)) return;
	gf_list_add(sr->visuals, surf);
}

void render_visual_unregister(Render *sr, GF_VisualManager *surf)
{
	gf_list_del_item(sr->visuals, surf);
}

Bool render_visual_is_registered(Render *sr, GF_VisualManager *surf)
{
	GF_VisualManager *tmp;
	u32 i = 0;
	while ((tmp = (GF_VisualManager *)gf_list_enum(sr->visuals, &i))) {
		if (tmp == surf) return 1;
	}
	return 0;
}


static void render_draw_scene(GF_VisualRenderer *vr)
{
	GF_Window rc;
	u32 flags;
	Render *sr = (Render *)vr->user_priv;
	GF_Node *top_node = gf_sg_get_root_node(sr->compositor->scene);

	if (!top_node && !sr->visual->last_had_back && !sr->visual->cur_context) {
		//GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Renderer] Scene has no root node, nothing to draw\n"));
		return;
	}

	if (top_node && !sr->root_visual_setup) {
		u32 node_tag;
#ifndef GPAC_DISABLE_3D
		Bool was_3d = sr->visual->type_3d;
#endif
		sr->root_uses_dom_events = 1;
		sr->root_visual_setup = 1;
		sr->visual->center_coords = 1;

		sr->traverse_state->pixel_metrics = 1;
		sr->traverse_state->min_hsize = INT2FIX(MIN(sr->compositor->scene_width, sr->compositor->scene_height)) / 2;

		node_tag = gf_node_get_tag(top_node);
		switch (node_tag) {
		case TAG_MPEG4_OrderedGroup:
		case TAG_MPEG4_Layer2D:
#ifndef GPAC_DISABLE_3D
			sr->visual->type_3d = 0;
			sr->visual->camera.is_3D = 0;
#endif
			sr->root_uses_dom_events = 0;
			sr->traverse_state->pixel_metrics = gf_sg_use_pixel_metrics(sr->compositor->scene);
			break;
		case TAG_MPEG4_Group:
		case TAG_MPEG4_Layer3D:
#ifndef GPAC_DISABLE_3D
			sr->visual->type_3d = 2;
			sr->visual->camera.is_3D = 1;
#endif
			sr->root_uses_dom_events = 0;
			sr->traverse_state->pixel_metrics = gf_sg_use_pixel_metrics(sr->compositor->scene);
			break;
		case TAG_X3D_Group:
#ifndef GPAC_DISABLE_3D
			sr->visual->type_3d = 3;
#endif
			sr->root_uses_dom_events = 0;
			sr->traverse_state->pixel_metrics = gf_sg_use_pixel_metrics(sr->compositor->scene);
			break;
#ifndef GPAC_DISABLE_SVG
		case TAG_SVG_svg:
#ifndef GPAC_DISABLE_3D
			sr->visual->type_3d = 0;
			sr->visual->camera.is_3D = 0;
#endif
			sr->visual->center_coords = 0;
			sr->root_visual_setup = 2;
			/*by default we set the focus on the content*/
			//sr->focus_node = NULL;
			render_svg_focus_switch_ring(sr, 0);
			break;
#endif
		}

		/*setup camera mode*/
#ifndef GPAC_DISABLE_3D
		if (! (sr->compositor->video_out->hw_caps & GF_VIDEO_HW_OPENGL)) {
			sr->visual->type_3d = 0;
			sr->visual->camera.is_3D = 0;
		}

#ifdef GPAC_USE_TINYGL
		sr->visual->type_3d = 0;
#endif
		sr->visual->camera.is_3D = (sr->visual->type_3d>1) ? 1 : 0;
		camera_invalidate(&sr->visual->camera);
#endif

		sr->traverse_state->vp_size.x = INT2FIX(sr->compositor->scene_width);
		sr->traverse_state->vp_size.y = INT2FIX(sr->compositor->scene_height);


		GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Renderer] Main scene setup - Using DOM events: %d - pixel metrics %d - center coords %d\n", sr->root_uses_dom_events, sr->traverse_state->pixel_metrics, sr->visual->center_coords));

#ifndef GPAC_DISABLE_3D
		/*change in 2D/3D config, force AR recompute/video restup*/
		if (was_3d != sr->visual->type_3d) sr->recompute_ar = was_3d ? 1 : 2;
#endif
	}

	if (sr->recompute_ar) {
#ifndef GPAC_DISABLE_3D
		if (sr->visual->type_3d) render_3d_set_aspect_ratio(sr);
		else
#endif
			render_2d_set_aspect_ratio(sr);
		sr->recompute_ar = 0;

		sr->traverse_state->vp_size.x = INT2FIX(sr->compositor->scene_width);
		sr->traverse_state->vp_size.y = INT2FIX(sr->compositor->scene_height);
	}

#if 0
	{
		GF_SystemRTInfo rti;
		gf_sys_get_rti(0, &rti, GF_RTI_SYSTEM_MEMORY_ONLY);
		fprintf(stdout, "Memory usage before DrawScene: %d\n", rti.gpac_memory);
	}
#endif

	flags = sr->traverse_state->trav_flags;
	visual_render_frame(sr->visual, top_node, sr->traverse_state, 1);

	sr->traverse_state->trav_flags = flags;
	sr->traverse_state->invalidate_all = 0;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Renderer] Rendering done - flushing output\n"));

	/*and flush*/
	rc.x = rc.y = 0; 
	rc.w = sr->compositor->width;	
	rc.h = sr->compositor->height;		
	sr->compositor->video_out->Flush(sr->compositor->video_out, &rc);

#if 0
	{
		GF_SystemRTInfo rti;
		gf_sys_get_rti(0, &rti, GF_RTI_SYSTEM_MEMORY_ONLY);
		fprintf(stdout, "Memory usage after DrawScene: %d\n", rti.gpac_memory);
	}
#endif 
}

static GF_Err render_recompute_aspect_ratio(GF_VisualRenderer *vr)
{
	Render *sr = (Render *)vr->user_priv;
	sr->recompute_ar = 1;
	return GF_OK;
}


static GF_Node *render_pick_node(GF_VisualRenderer *vr, s32 X, s32 Y)
{
	return NULL;
}

static void render_reset_graphics(GF_VisualRenderer *vr)
{
#ifndef GPAC_DISABLE_3D
	Render *sr = (Render *)vr->user_priv;
	if (sr) {
		render_load_opengl_extensions(sr);
		sr->offscreen_width = sr->offscreen_height = 0;
	}
#endif
}

static void render_reload_config(GF_VisualRenderer *vr)
{
	const char *sOpt;
	Render *sr = (Render *)vr->user_priv;

	gf_sr_lock(sr->compositor, 1);

	sOpt = gf_modules_get_option((GF_BaseInterface *)vr, "Render", "DirectRender");

	if (sOpt && !stricmp(sOpt, "yes") ) {
		sr->traverse_state->trav_flags |= TF_RENDER_DIRECT;
	} else {
		sr->traverse_state->trav_flags &= ~TF_RENDER_DIRECT;
	}

	sOpt = gf_modules_get_option((GF_BaseInterface *)vr, "Render", "ScalableZoom");
	sr->scalable_zoom = (!sOpt || !stricmp(sOpt, "yes") ) ? 1 : 0;
	sOpt = gf_modules_get_option((GF_BaseInterface *)vr, "Render", "DisableYUV");
	sr->enable_yuv_hw = (sOpt && !stricmp(sOpt, "yes") ) ? 0 : 1;
	sOpt = gf_modules_get_option((GF_BaseInterface *)vr, "Render", "DisablePartialHardwareBlit");
	sr->disable_partial_hw_blit = (sOpt && !stricmp(sOpt, "yes") ) ? 1 : 0;


#ifndef GPAC_DISABLE_3D

	/*currently:
	- no tesselator for GL-ES, so use raster outlines.
	- no support for npow2 textures, and no support for DrawPixels
	*/
#ifndef GPAC_USE_OGL_ES
	sOpt = gf_modules_get_option((GF_BaseInterface *)vr, "Render3D", "RasterOutlines");
	sr->raster_outlines = (sOpt && !stricmp(sOpt, "yes") ) ? 1 : 0;
	sOpt = gf_modules_get_option((GF_BaseInterface *)vr, "Render3D", "EmulatePOW2");
	sr->emul_pow2 = (sOpt && !stricmp(sOpt, "yes") ) ? 1 : 0;
	sOpt = gf_modules_get_option((GF_BaseInterface *)vr, "Render3D", "BitmapCopyPixels");
	sr->bitmap_use_pixels = (sOpt && !stricmp(sOpt, "yes") ) ? 1 : 0;
#else
	sr->raster_outlines = 1;
	sr->emul_pow2 = 1;
	sr->bitmap_use_pixels = 0;
#endif

	sOpt = gf_modules_get_option((GF_BaseInterface *)vr, "Render3D", "PolygonAA");
	sr->poly_aa = (sOpt && !stricmp(sOpt, "yes") ) ? 1 : 0;

	sOpt = gf_modules_get_option((GF_BaseInterface *)vr, "Render3D", "BackFaceCulling");
	if (sOpt && !stricmp(sOpt, "Off")) sr->backcull = GF_BACK_CULL_OFF;
	else if (sOpt && !stricmp(sOpt, "Alpha")) sr->backcull = GF_BACK_CULL_ALPHA;
	else sr->backcull = GF_BACK_CULL_ON;

	sOpt = gf_modules_get_option((GF_BaseInterface *)vr, "Render3D", "Wireframe");
	if (sOpt && !stricmp(sOpt, "WireOnly")) sr->wiremode = GF_WIREFRAME_ONLY;
	else if (sOpt && !stricmp(sOpt, "WireOnSolid")) sr->wiremode = GF_WIREFRAME_SOLID;
	else sr->wiremode = GF_WIREFRAME_NONE;

	sOpt = gf_modules_get_option((GF_BaseInterface *)vr, "Render3D", "DrawNormals");
	if (sOpt && !stricmp(sOpt, "PerFace")) sr->draw_normals = GF_NORMALS_FACE;
	else if (sOpt && !stricmp(sOpt, "PerVertex")) sr->draw_normals = GF_NORMALS_VERTEX;
	else sr->draw_normals = GF_NORMALS_NONE;

	sOpt = gf_modules_get_option((GF_BaseInterface *)vr, "Render3D", "DisableRectExt");
	sr->disable_rect_ext = (sOpt && !stricmp(sOpt, "yes") ) ? 1 : 0;
	/*RECT texture support - we must reload HW*/
	gf_sr_reset_graphics(sr->compositor);
#endif


	sr->compositor->msg_type |= GF_SR_CFG_AR;
	sr->compositor->draw_next_frame = 1;
	gf_sr_lock(sr->compositor, 0);
}

static GF_Err render_set_option(GF_VisualRenderer *vr, u32 option, u32 value)
{
	Render *sr = (Render *)vr->user_priv;
	switch (option) {
	case GF_OPT_DIRECT_RENDER:
		gf_sr_lock(sr->compositor, 1);
		if (value) {
			sr->traverse_state->trav_flags |= TF_RENDER_DIRECT;
		} else {
			sr->traverse_state->trav_flags &= ~TF_RENDER_DIRECT;
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
	case GF_OPT_RELOAD_CONFIG: render_reload_config(vr); return GF_OK;
	case GF_OPT_ORIGINAL_VIEW: 
		render_2d_set_user_transform(sr, FIX_ONE, 0, 0, 1);
		return GF_OK;
	case GF_OPT_NAVIGATION_TYPE: 
		render_2d_set_user_transform(sr, FIX_ONE, 0, 0, 1);
#ifndef GPAC_DISABLE_3D
		render_3d_reset_camera(sr);
#endif
		return GF_OK;
	case GF_OPT_NAVIGATION:
		if (sr->navigation_disabled) return GF_NOT_SUPPORTED;
#ifndef GPAC_DISABLE_3D
		if (sr->visual->type_3d || sr->active_layer) {
			GF_Camera *cam = render_3d_get_camera(sr);
			if (cam->navigation_flags & NAV_ANY) {
				/*if not specifying mode, try to (un)bind top*/
				if (!value) {
					if (sr->active_layer) {
						render_layer3d_bind_camera(sr->active_layer, 0, value);
					} else {
						GF_Node *n = (GF_Node*)gf_list_get(sr->visual->navigation_stack, 0);
						if (n) Bindable_SetSetBind(n, 0);
						else cam->navigate_mode = value;
					}
				} else {
					cam->navigate_mode = value;
				}
				return GF_OK;
			}
		} else 
#endif
		{
			if ((value!=GF_NAVIGATE_NONE) && (value!=GF_NAVIGATE_SLIDE) && (value!=GF_NAVIGATE_EXAMINE)) return GF_NOT_SUPPORTED;
			sr->navigate_mode = value;
			return GF_OK;
		}
		return GF_NOT_SUPPORTED;

	case GF_OPT_HEADLIGHT: 
#ifndef GPAC_DISABLE_3D
		if (sr->visual->type_3d || sr->active_layer) {
			GF_Camera *cam = render_3d_get_camera(sr);
			if (cam->navigation_flags & NAV_ANY) {
				if (value) 
					cam->navigation_flags |= NAV_HEADLIGHT;
				else
					cam->navigation_flags &= ~NAV_HEADLIGHT;
				return GF_OK;
			}
		}
#endif
		return GF_NOT_SUPPORTED;

	
#ifndef GPAC_DISABLE_3D

	case GF_OPT_EMULATE_POW2: sr->emul_pow2 = value; return GF_OK;
	case GF_OPT_POLYGON_ANTIALIAS: sr->poly_aa = value; return GF_OK;
	case GF_OPT_BACK_CULL: sr->backcull = value; return GF_OK;
	case GF_OPT_WIREFRAME: sr->wiremode = value; return GF_OK;
	case GF_OPT_NORMALS: sr->draw_normals = value; return GF_OK;
#ifndef GPAC_USE_OGL_ES
	case GF_OPT_RASTER_OUTLINES: sr->raster_outlines = value; return GF_OK;
#endif

	case GF_OPT_NO_RECT_TEXTURE:
		if (value != sr->disable_rect_ext) {
			sr->disable_rect_ext = value;
			/*RECT texture support - we must reload HW*/
			gf_sr_reset_graphics(sr->compositor);
		}
		return GF_OK;
	case GF_OPT_BITMAP_COPY: sr->bitmap_use_pixels = value; return GF_OK;


	case GF_OPT_COLLISION: 
		sr->collide_mode = value;
		return GF_OK;
	case GF_OPT_GRAVITY: 
	{	
		GF_Camera *cam = render_3d_get_camera(sr);
		sr->gravity_on = value;
		/*force collision pass*/
		cam->last_pos.z -= 1;
		gf_sr_invalidate(sr->compositor, NULL);
	}
		return GF_OK;	
#endif

	default:
		return GF_NOT_SUPPORTED;
	}
}

static u32 render_get_option(GF_VisualRenderer *vr, u32 option)
{
	Render *sr = (Render *)vr->user_priv;
	switch (option) {
	case GF_OPT_DIRECT_RENDER: return (sr->traverse_state->trav_flags & TF_RENDER_DIRECT) ? 1 : 0;
	case GF_OPT_SCALABLE_ZOOM: return sr->scalable_zoom;
	case GF_OPT_YUV_HARDWARE: return sr->enable_yuv_hw;
	case GF_OPT_YUV_FORMAT: return sr->enable_yuv_hw ? sr->compositor->video_out->yuv_pixel_format : 0;
	case GF_OPT_NAVIGATION_TYPE: 
#ifndef GPAC_DISABLE_3D
		if (sr->visual->type_3d || sr->active_layer) {
			GF_Camera *cam = render_3d_get_camera(sr);
			if (!(cam->navigation_flags & NAV_ANY)) return GF_NAVIGATE_TYPE_NONE;
			return ((cam->is_3D || sr->active_layer) ? GF_NAVIGATE_TYPE_3D : GF_NAVIGATE_TYPE_2D);
		} else 
#endif
		{
			return sr->navigation_disabled ? GF_NAVIGATE_TYPE_NONE : GF_NAVIGATE_TYPE_2D;
		}
	case GF_OPT_NAVIGATION: 
#ifndef GPAC_DISABLE_3D
		if (sr->visual->type_3d || sr->active_layer) {
			GF_Camera *cam = render_3d_get_camera(sr);
			return cam->navigate_mode;
		} 
#endif
		return sr->navigate_mode;

	case GF_OPT_HEADLIGHT: return 0;
	case GF_OPT_COLLISION: return GF_COLLISION_NONE;
	case GF_OPT_GRAVITY: return 0;
	default: return 0;
	}
}

/*render inline scene*/
static void render_inline_mpeg4(GF_VisualRenderer *vr, GF_Node *inline_parent, GF_Node *inline_root, void *rs)
{
	Bool use_pm, flip_coords;
	u32 h, w;
	GF_SceneGraph *in_scene;
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;

	flip_coords = 0;
	in_scene = gf_node_get_graph(inline_root);
	switch (gf_node_get_tag(inline_root)) {
	case TAG_MPEG4_Layer2D:
	case TAG_MPEG4_OrderedGroup:
	case TAG_MPEG4_Layer3D:
	case TAG_MPEG4_Group:
	case TAG_X3D_Group:
		use_pm = gf_sg_use_pixel_metrics(in_scene);
		break;
	default:
		flip_coords = 1;
		use_pm = 1;
		break;
	}
	if (use_pm == tr_state->pixel_metrics) {
		if (flip_coords) {
			GF_Matrix2D mx_bck, mx;
			gf_mx2d_copy(mx_bck, tr_state->transform);
			gf_mx2d_init(mx);
			gf_mx2d_add_scale(&mx, 1, -1);
			gf_mx2d_add_translation(&mx, -tr_state->vp_size.x/2, tr_state->vp_size.y/2);
			gf_mx2d_pre_multiply(&tr_state->transform, &mx);
			gf_node_render(inline_root, rs);
			gf_mx2d_copy(tr_state->transform, mx_bck);
		} else {
			gf_node_render(inline_root, rs);
		}
		return;
	}
	/*override aspect ratio if any size info is given in the scene*/
	if (gf_sg_get_scene_size_info(in_scene, &w, &h)) {
		Fixed scale = INT2FIX( MIN(w, h) / 2);
		if (scale) tr_state->min_hsize = scale;
	}

#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) {
		GF_Matrix gf_mx_bck, mx;
		gf_mx_copy(gf_mx_bck, tr_state->model_matrix);
		gf_mx_init(mx);
		/*apply meterMetrics<->pixelMetrics scale*/
		if (!use_pm) {
			gf_mx_add_scale(&mx, tr_state->min_hsize, tr_state->min_hsize, tr_state->min_hsize);
		} else {
			Fixed inv_scale = gf_invfix(tr_state->min_hsize);
			gf_mx_add_scale(&mx, inv_scale, inv_scale, inv_scale);
		}
		tr_state->pixel_metrics = use_pm;
		gf_mx_add_matrix(&tr_state->model_matrix, &mx);
		if (tr_state->traversing_mode==TRAVERSE_RENDER) {
			visual_3d_matrix_push(tr_state->visual);
			visual_3d_matrix_add(tr_state->visual, mx.m);
			gf_node_render(inline_root, rs);
			visual_3d_matrix_pop(tr_state->visual);
		} else {
			gf_node_render(inline_root, rs);
		}
		gf_mx_copy(tr_state->model_matrix, gf_mx_bck);
	} else 
#endif
	{
		GF_Matrix2D mx_bck, mx;
		gf_mx2d_copy(mx_bck, tr_state->transform);
		gf_mx2d_init(mx);
		if (!use_pm) {
			gf_mx2d_add_scale(&mx, tr_state->min_hsize, tr_state->min_hsize);
		} else {
			Fixed inv_scale = gf_invfix(tr_state->min_hsize);
			gf_mx2d_add_scale(&mx, inv_scale, inv_scale);
		}
		tr_state->pixel_metrics = use_pm;
		gf_mx2d_add_matrix(&tr_state->transform, &mx);
		gf_node_render(inline_root, rs);
		gf_mx2d_copy(tr_state->transform, mx_bck);
	}
	tr_state->pixel_metrics = !use_pm;
}

static void render_render_inline(GF_VisualRenderer *vr, GF_Node *inline_parent, GF_Node *inline_root, void *rs)
{
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	if (!inline_root) return;

#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d ) {
		Bool needs_3d_switch = 0;
		switch (gf_node_get_tag(inline_root)) {
		case TAG_MPEG4_Layer3D:
//		case TAG_MPEG4_Group:
//		case TAG_X3D_Group:
			needs_3d_switch = 1;
			break;
		}
		if (needs_3d_switch) {
		}
	}
#endif
	switch (gf_node_get_tag(inline_parent)) {
	case TAG_SVG_animation:
		render_svg_render_animation(inline_parent, inline_root, rs);
		break;
	case TAG_SVG_use:
		render_svg_render_use(inline_parent, inline_root, rs);
		break;
	default:
		render_inline_mpeg4(vr, inline_parent, inline_root, rs);
		break;
	}
}

static GF_Err render_get_screenbuffer(GF_VisualRenderer *vr, GF_VideoSurface *framebuffer)
{
	Render *sr = (Render *)vr->user_priv;
#ifndef GPAC_DISABLE_3D
	if (sr->visual->type_3d) return render_3d_get_screen_buffer(sr, framebuffer);
#endif
	return sr->compositor->video_out->LockBackBuffer(sr->compositor->video_out, framebuffer, 1);
}

static GF_Err render_release_screenbuffer(GF_VisualRenderer *vr, GF_VideoSurface *framebuffer)
{
	Render *sr = (Render *)vr->user_priv;
#ifndef GPAC_DISABLE_3D
	if (sr->visual->type_3d) render_3d_release_screen_buffer(sr, framebuffer);
#endif
	return sr->compositor->video_out->LockBackBuffer(sr->compositor->video_out, framebuffer, 0);
}

static Bool render_script_action(GF_VisualRenderer *vr, u32 type, GF_Node *n, GF_JSAPIParam *param)
{
	Render *sr = (Render *)vr->user_priv;
	
	switch (type) {
	case GF_JSAPI_OP_GET_SCALE: param->val = sr->zoom; return 1;
	case GF_JSAPI_OP_SET_SCALE: render_2d_set_user_transform(sr, param->val, sr->trans_x, sr->trans_y, 0); return 1;
	case GF_JSAPI_OP_GET_ROTATION: param->val = gf_divfix(180*sr->rotation, GF_PI); return 1;
	case GF_JSAPI_OP_SET_ROTATION: sr->rotation = gf_mulfix(GF_PI, param->val/180); render_2d_set_user_transform(sr, sr->zoom, sr->trans_x, sr->trans_y, 0); return 1;
	case GF_JSAPI_OP_GET_TRANSLATE: param->pt.x = sr->trans_x; param->pt.y = sr->trans_y; return 1;
	case GF_JSAPI_OP_SET_TRANSLATE: render_2d_set_user_transform(sr, sr->zoom, param->pt.x, param->pt.y, 0); return 1;
	/*FIXME - better SMIL timelines support*/
	case GF_JSAPI_OP_GET_TIME: param->time = gf_node_get_scene_time(n); return 1;
	case GF_JSAPI_OP_SET_TIME: /*seek_to(param->time);*/ return 0;
	/*FIXME - this will not work for inline docs, we will have to store the clipper at the <svg> level*/
	case GF_JSAPI_OP_GET_VIEWPORT: 
		param->rc = gf_rect_ft(&sr->visual->top_clipper); 
		if (!sr->visual->center_coords) param->rc.y = param->rc.height - param->rc.y;
		return 1;
	case GF_JSAPI_OP_SET_FOCUS: sr->focus_node = param->node; return 1;
	case GF_JSAPI_OP_GET_FOCUS: param->node = sr->focus_node; return 1;

	/*same routine: traverse tree from root to target, collecting both bounds and transform matrix*/
	case GF_JSAPI_OP_GET_LOCAL_BBOX:
	case GF_JSAPI_OP_GET_SCREEN_BBOX:
	case GF_JSAPI_OP_GET_TRANSFORM:
	{
		GF_TraverseState tr_state;
		memset(&tr_state, 0, sizeof(tr_state));
		tr_state.traversing_mode = TRAVERSE_GET_BOUNDS;
		tr_state.visual = sr->visual;
		tr_state.for_node = n;
		gf_mx2d_init(tr_state.transform);
		gf_node_render(gf_sg_get_root_node(sr->compositor->scene), &tr_state);
		if (type==GF_JSAPI_OP_GET_LOCAL_BBOX) gf_bbox_from_rect(&param->bbox, &tr_state.bounds);
		else if (type==GF_JSAPI_OP_GET_TRANSFORM) gf_mx_from_mx2d(&param->mx, &tr_state.transform);
		else {
			gf_mx2d_apply_rect(&tr_state.transform, &tr_state.bounds);
			gf_bbox_from_rect(&param->bbox, &tr_state.bounds);
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

static GF_Err render_get_viewport(GF_VisualRenderer *vr, u32 viewpoint_idx, const char **outName, Bool *is_bound)
{
	u32 count;
	GF_Node *n;
	Render *sr = (Render *) vr->user_priv;
	if (!sr->visual) return GF_BAD_PARAM;
	count = gf_list_count(sr->visual->view_stack);
	if (!viewpoint_idx) return GF_BAD_PARAM;
	if (viewpoint_idx>count) return GF_EOS;

	n = (GF_Node*)gf_list_get(sr->visual->view_stack, viewpoint_idx-1);
	switch (gf_node_get_tag(n)) {
	case TAG_MPEG4_Viewport: *outName = ((M_Viewport*)n)->description.buffer; *is_bound = ((M_Viewport*)n)->isBound;return GF_OK;
	case TAG_MPEG4_Viewpoint: case TAG_X3D_Viewpoint: *outName = ((M_Viewpoint*)n)->description.buffer; *is_bound = ((M_Viewpoint*)n)->isBound; return GF_OK;
	default: *outName = NULL; return GF_OK;
	}
	return GF_OK;
}

static GF_Err render_set_viewport(GF_VisualRenderer *vr, u32 viewpoint_idx, const char *viewpoint_name)
{
	u32 count, i;
	GF_Node *n;
	Render *sr = (Render *) vr->user_priv;
	if (!sr->visual) return GF_BAD_PARAM;
	count = gf_list_count(sr->visual->view_stack);
	if (viewpoint_idx>count) return GF_BAD_PARAM;
	if (!viewpoint_idx && !viewpoint_name) return GF_BAD_PARAM;

	if (viewpoint_idx) {
		Bool bind;
		n = (GF_Node*)gf_list_get(sr->visual->view_stack, viewpoint_idx-1);
		bind = Bindable_GetIsBound(n);
		Bindable_SetSetBind(n, !bind);
		return GF_OK;
	}
	for (i=0; i<count;i++) {
		char *name = NULL;
		n = (GF_Node*)gf_list_get(sr->visual->view_stack, viewpoint_idx-1);
		switch (gf_node_get_tag(n)) {
		case TAG_MPEG4_Viewport: name = ((M_Viewport*)n)->description.buffer; break;
		case TAG_MPEG4_Viewpoint: case TAG_X3D_Viewpoint: name = ((M_Viewpoint*)n)->description.buffer; break;
		default: break;
		}
		if (name && !stricmp(name, viewpoint_name)) {
			Bool bind = Bindable_GetIsBound(n);
			Bindable_SetSetBind(n, !bind);
			return GF_OK;
		}
	}
	return GF_BAD_PARAM;
}



static GF_Err render_load(GF_VisualRenderer *vr, GF_Renderer *compositor)
{
	Render *sr;
	const char *sOpt;
	if (vr->user_priv) return GF_BAD_PARAM;

	GF_SAFEALLOC(sr, Render);
	if (!sr) return GF_OUT_OF_MEM;

	sr->compositor = compositor;

	sr->strike_bank = gf_list_new();
	sr->visuals = gf_list_new();

	GF_SAFEALLOC(sr->traverse_state, GF_TraverseState);
	sr->traverse_state->vrml_sensors = gf_list_new();
	sr->sensors = gf_list_new();
	sr->previous_sensors = gf_list_new();
	
	/*setup main visual*/
	sr->visual = visual_new(sr);
	sr->visual->GetSurfaceAccess = render_2d_get_video_access;
	sr->visual->ReleaseSurfaceAccess = render_2d_release_video_access;
	sr->visual->DrawBitmap = render_2d_draw_bitmap;
	sr->visual->SupportsFormat = render_2d_pixel_format_supported;

	gf_list_add(sr->visuals, sr->visual);

	sr->zoom = sr->scale_x = sr->scale_y = FIX_ONE;
	vr->user_priv = sr;

	sOpt = gf_cfg_get_key(compositor->user->config, "Render", "FocusHighlightFill");
	if (sOpt) sscanf(sOpt, "%x", &sr->highlight_fill);
	sOpt = gf_cfg_get_key(compositor->user->config, "Render", "FocusHighlightStroke");
	if (sOpt) sscanf(sOpt, "%x", &sr->highlight_stroke);
	else sr->highlight_stroke = 0xFF000000;

	/*create a drawable for focus highlight*/
	sr->focus_highlight = drawable_new();
	/*associate a dummy node for rendering*/
	sr->focus_highlight->node = gf_node_new(NULL, TAG_UndefinedNode);
	gf_node_register(sr->focus_highlight->node, NULL);
	gf_node_set_callback_function(sr->focus_highlight->node, drawable_render_focus);

	/*load options*/
	sOpt = gf_cfg_get_key(compositor->user->config, "Render", "DirectRender");
	if (sOpt && ! stricmp(sOpt, "yes")) 
		sr->traverse_state->trav_flags |= TF_RENDER_DIRECT;
	else
		sr->traverse_state->trav_flags &= ~TF_RENDER_DIRECT;
	
	sOpt = gf_cfg_get_key(compositor->user->config, "Render", "ScalableZoom");
	sr->scalable_zoom = (!sOpt || !stricmp(sOpt, "yes") ) ? 1 : 0;
	sOpt = gf_cfg_get_key(compositor->user->config, "Render", "DisableYUV");
	sr->enable_yuv_hw = (sOpt && !stricmp(sOpt, "yes") ) ? 0 : 1;

	
#ifndef GPAC_DISABLE_3D
	/*default collision mode*/
	sr->collide_mode = GF_COLLISION_DISPLACEMENT;
//	sr->collide_mode = GF_COLLISION_NORMAL;
	sr->gravity_on = 1;
#endif

	return GF_OK;
}



void render_unload(GF_VisualRenderer *vr)
{
	Render *sr = (Render *)vr->user_priv;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Renderer] Destroying renderer\n"));
	
	gf_node_unregister(sr->focus_highlight->node, NULL);
	drawable_del_ex(sr->focus_highlight, sr);

	visual_del(sr->visual);
	gf_list_del(sr->sensors);
	gf_list_del(sr->previous_sensors);
	gf_list_del(sr->visuals);
	gf_list_del(sr->strike_bank);

	gf_list_del(sr->traverse_state->vrml_sensors);
	free(sr->traverse_state);

#ifndef GPAC_DISABLE_3D
	if (sr->unit_bbox) mesh_free(sr->unit_bbox);
#endif

	free(sr);
	vr->user_priv = NULL;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Renderer] renderer destroyed\n"));
}


/*interface create*/
GF_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	GF_VisualRenderer *sr;
	if (InterfaceType != GF_RENDERER_INTERFACE) return NULL;
	
	GF_SAFEALLOC(sr, GF_VisualRenderer);
	if (!sr) return NULL;

	GF_REGISTER_MODULE_INTERFACE(sr, GF_RENDERER_INTERFACE, "GPAC Full Renderer", "gpac distribution");

	sr->LoadRenderer = render_load;
	sr->UnloadRenderer = render_unload;
	sr->GraphicsReset = render_reset_graphics;
	sr->NodeChanged = render_node_changed;
	sr->NodeInit = render_node_init;
	sr->DrawScene = render_draw_scene;
	sr->ExecuteEvent = render_exec_event;
	sr->RecomputeAR = render_recompute_aspect_ratio;
	sr->SceneReset = render_reset;
	sr->RenderInline = render_render_inline;
	sr->AllocTexture = render_texture_allocate;
	sr->ReleaseTexture = render_texture_release;
	sr->SetTextureData = render_texture_set_data;
	sr->TextureHWReset = render_texture_reset;
	sr->SetOption = render_set_option;
	sr->GetOption = render_get_option;
	sr->GetScreenBuffer = render_get_screenbuffer;
	sr->ReleaseScreenBuffer = render_release_screenbuffer;
	sr->GetViewpoint = render_get_viewport;
	sr->SetViewpoint = render_set_viewport;
	sr->ScriptAction = render_script_action;

	sr->user_priv = NULL;
	return (GF_BaseInterface *)sr;
}


/*interface destroy*/
GF_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	GF_VisualRenderer *rend = (GF_VisualRenderer *)ifce;
	if (rend->InterfaceType != GF_RENDERER_INTERFACE) return;
	if (rend->user_priv) render_unload(rend);
	free(rend);
}

/*interface query*/
GF_EXPORT
Bool QueryInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_RENDERER_INTERFACE) return 1;
	return 0;
}


