/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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

#include "visual_manager.h"

#ifndef GPAC_DISABLE_3D

#include <gpac/options.h>
#include "gl_inc.h"

#if (defined(WIN32) || defined(_WIN32_WCE)) && !defined(__GNUC__)
# if defined(GPAC_USE_TINYGL)
#  pragma comment(lib, "TinyGL")

# elif defined(GPAC_USE_OGL_ES)

#  if 0
#   pragma message("Using OpenGL-ES Common Lite Profile")
#   pragma comment(lib, "libGLES_CL")
#	define GL_ES_CL_PROFILE
#  else
#   pragma message("Using OpenGL-ES Common Profile")
#   pragma comment(lib, "libGLES_CM")
#  endif

# else
#  pragma comment(lib, "opengl32")
#  pragma comment(lib, "glu32")
# endif
#endif

/*!! HORRIBLE HACK, but on my test devices, it seems that glClipPlanex is missing on the device but not in the SDK lib !!*/
#if defined(GL_MAX_CLIP_PLANES) && defined(__SYMBIAN32__)
#undef GL_MAX_CLIP_PLANES
#endif

#define CHECK_GL_EXT(name) ((strstr(ext, name) != NULL) ? 1 : 0)

void gf_sc_load_opengl_extensions(GF_Compositor *compositor)
{
#ifdef GPAC_USE_TINYGL
	/*let TGL handle texturing*/
	compositor->gl_caps.rect_texture = 1;
	compositor->gl_caps.npot_texture = 1;
#else

#if defined (GPAC_USE_OGL_ES)
#define GET_GLFUN(__name) (PFNGLARBMULTITEXTUREPROC) eglGetProcAddress(__name) 
#elif defined (WIN32)
#define GET_GLFUN(__name) (PFNGLARBMULTITEXTUREPROC) wglGetProcAddress(__name) 
#else
#define GET_GLFUN(__name) (PFNGLARBMULTITEXTUREPROC) glXGetProcAddress(__name) 
#endif

	const char *ext;
	if (!compositor->visual->type_3d) return;

	ext = (const char *) glGetString(GL_EXTENSIONS);
	/*store OGL extension to config for app usage*/
	gf_cfg_set_key(compositor->user->config, "Compositor", "OpenGLExtensions", ext ? ext : "None");
	if (!ext) return;
	memset(&compositor->gl_caps, 0, sizeof(GLCaps));

	if (CHECK_GL_EXT("GL_ARB_multisample") || CHECK_GL_EXT("GLX_ARB_multisample") || CHECK_GL_EXT("WGL_ARB_multisample")) 
		compositor->gl_caps.multisample = 1;
	if (CHECK_GL_EXT("GL_ARB_texture_non_power_of_two")) 
		compositor->gl_caps.npot_texture = 1;
	if (CHECK_GL_EXT("GL_EXT_abgr")) 
		compositor->gl_caps.abgr_texture = 1;
	if (CHECK_GL_EXT("GL_EXT_bgra")) 
		compositor->gl_caps.bgra_texture = 1;

	if (CHECK_GL_EXT("GL_EXT_texture_rectangle") || CHECK_GL_EXT("GL_NV_texture_rectangle")) {
		compositor->gl_caps.rect_texture = 1;

		if (CHECK_GL_EXT("GL_MESA_ycbcr_texture")) compositor->gl_caps.yuv_texture = YCBCR_MESA;
		else if (CHECK_GL_EXT("GL_APPLE_ycbcr_422")) compositor->gl_caps.yuv_texture = YCBCR_422_APPLE;
	}

	if (CHECK_GL_EXT("GL_ARB_multitexture")) {
		compositor->gl_caps.glActiveTextureARB = GET_GLFUN("glActiveTextureARB");
		compositor->gl_caps.glClientActiveTextureARB = GET_GLFUN("glClientActiveTextureARB");
	}

#endif
}

static void visual_3d_setup_quality(GF_VisualManager *visual)
{
	if (visual->compositor->high_speed) {
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
		glHint(GL_LINE_SMOOTH_HINT, GL_FASTEST);
#ifdef GL_POLYGON_SMOOTH_HINT
		glHint(GL_POLYGON_SMOOTH_HINT, GL_FASTEST);
#endif
	} else {
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
		glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
#ifdef GL_POLYGON_SMOOTH_HINT
		glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
#endif
	}

	if (visual->compositor->antiAlias == GF_ANTIALIAS_FULL) {
		glEnable(GL_LINE_SMOOTH);
#ifndef GPAC_USE_OGL_ES
		if (visual->compositor->poly_aa)
			glEnable(GL_POLYGON_SMOOTH);
		else
			glDisable(GL_POLYGON_SMOOTH);
#endif
	} else {
		glDisable(GL_LINE_SMOOTH);
#ifndef GPAC_USE_OGL_ES
		glDisable(GL_POLYGON_SMOOTH);
#endif
	}
}

void visual_3d_setup(GF_VisualManager *visual)
{
#ifndef GPAC_USE_TINYGL
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthFunc(GL_LEQUAL);
#endif
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);

#ifdef GPAC_USE_OGL_ES
	glClearDepthx(FIX_ONE);
	glLightModelx(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
	glMaterialx(GL_FRONT_AND_BACK, GL_SHININESS, FLT2FIX(0.2f * 128) );
#else
	glClearDepth(1.0f);
	glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_FALSE);
	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, (float) (0.2 * 128));
#endif

	glShadeModel(GL_SMOOTH);
	glGetIntegerv(GL_MAX_LIGHTS, (GLint*)&visual->max_lights);
#ifdef GL_MAX_CLIP_PLANES
	glGetIntegerv(GL_MAX_CLIP_PLANES, &visual->max_clips);
#endif

	visual_3d_setup_quality(visual);

	glDisable(GL_POINT_SMOOTH);
	glDisable(GL_COLOR_MATERIAL);
	glDisable(GL_LIGHTING);
	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_CULL_FACE);
	glDisable(GL_FOG);
	/*Note: we cannot enable/disable normalization on the fly, because we have no clue when the GL implementation
	will actually compute the related fragments. Since a typical world always use scaling, we always turn normalization on 
	to avoid tracking scale*/
	glEnable(GL_NORMALIZE);

	glClear(GL_DEPTH_BUFFER_BIT);
}

void visual_3d_set_background_state(GF_VisualManager *visual, Bool on)
{
	if (on) {
		glDisable(GL_LIGHTING);
		glDisable(GL_FOG);
		glDisable(GL_LINE_SMOOTH);
		glDisable(GL_BLEND);
#ifndef GPAC_USE_OGL_ES
		glDisable(GL_POLYGON_SMOOTH);
#endif
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
	} else {
		visual_3d_setup_quality(visual);
	}
}



void visual_3d_enable_antialias(GF_VisualManager *visual, Bool bOn)
{
	if (bOn) {
		glEnable(GL_LINE_SMOOTH);
#ifndef GPAC_USE_OGL_ES
		if (visual->compositor->poly_aa)
			glEnable(GL_POLYGON_SMOOTH);
		else
			glDisable(GL_POLYGON_SMOOTH);
#endif
	} else {
		glDisable(GL_LINE_SMOOTH);
#ifndef GPAC_USE_OGL_ES
		glDisable(GL_POLYGON_SMOOTH);
#endif
	}
}

void visual_3d_enable_depth_buffer(GF_VisualManager *visual, Bool on)
{
	if (on) glEnable(GL_DEPTH_TEST);
	else glDisable(GL_DEPTH_TEST);
}

void visual_3d_enable_headlight(GF_VisualManager *visual, Bool bOn, GF_Camera *cam)
{
	SFVec3f dir;
	SFColor col;
	if (!bOn) return;

	col.blue = col.red = col.green = FIX_ONE;
	if (cam->is_3D) {
		dir = camera_get_target_dir(cam);
	} else {
		dir.x = dir.y = 0; dir.z = FIX_ONE;
	}
	visual_3d_add_directional_light(visual, 0, col, FIX_ONE, dir);
}

void visual_3d_set_viewport(GF_VisualManager *visual, GF_Rect vp)
{
	glViewport(FIX2INT(vp.x), FIX2INT(vp.y), FIX2INT(vp.width), FIX2INT(vp.height));
}

void visual_3d_clear_depth(GF_VisualManager *visual)
{
	glClear(GL_DEPTH_BUFFER_BIT);
}

void VS3D_DrawAABBNode(GF_TraverseState *tr_state, GF_Mesh *mesh, u32 prim_type, GF_Plane *fplanes, u32 *p_indices, AABBNode *n)
{
	u32 i;
	
	/*if not leaf do cull*/
	if (n->pos) {
		u32 p_idx, cull;
		SFVec3f vertices[8];
		/*get box vertices*/
		gf_bbox_get_vertices(n->min, n->max, vertices);
		cull = CULL_INSIDE;
		for (i=0; i<6; i++) {
			p_idx = p_indices[i];
			/*check p-vertex: if not in plane, we're out (since p-vertex is the closest point to the plane)*/
			if (gf_plane_get_distance(&fplanes[i], &vertices[p_idx])<0) { cull = CULL_OUTSIDE; break; }
			/*check n-vertex: if not in plane, we're intersecting*/
			if (gf_plane_get_distance(&fplanes[i], &vertices[7-p_idx])<0) { cull = CULL_INTERSECTS; break;}
		}

		if (cull==CULL_OUTSIDE) return;

		if (cull==CULL_INTERSECTS) {
			VS3D_DrawAABBNode(tr_state, mesh, prim_type, fplanes, p_indices, n->pos);
			VS3D_DrawAABBNode(tr_state, mesh, prim_type, fplanes, p_indices, n->neg);
			return;
		}
	}

	/*the good thing about the structure of the aabb tree is that the primitive idx is valid for both
	leaf and non-leaf nodes, so we can use it as soon as we have a CULL_INSIDE.
	However we must push triangles one by one since primitive order may have been swapped when
	building the AABB tree*/
	for (i=0; i<n->nb_idx; i++) {
#ifdef GPAC_USE_OGL_ES
		glDrawElements(prim_type, 3, GL_UNSIGNED_SHORT, &mesh->indices[3*n->indices[i]]);
#else
		glDrawElements(prim_type, 3, GL_UNSIGNED_INT, &mesh->indices[3*n->indices[i]]);
#endif
	}
}

void VS3D_DrawMeshIntern(GF_TraverseState *tr_state, GF_Mesh *mesh)
{
	Bool has_col, has_tx, has_norm;
	u32 prim_type;
	GF_Compositor *compositor = tr_state->visual->compositor;
#if defined(GPAC_FIXED_POINT) && !defined(GPAC_USE_OGL_ES)
	Float *color_array = NULL;
	Float fix_scale = 1.0f;
	fix_scale /= FIX_ONE;
#endif

	has_col = has_tx = has_norm = 0;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[V3D] Drawing mesh 0x%08x\n", mesh));

	glEnableClientState(GL_VERTEX_ARRAY);
#if defined(GPAC_USE_OGL_ES)
	glVertexPointer(3, GL_FIXED, sizeof(GF_Vertex),  &mesh->vertices[0].pos);
#elif defined(GPAC_FIXED_POINT)
	/*scale modelview matrix*/
	glPushMatrix();
	glScalef(fix_scale, fix_scale, fix_scale);
	glVertexPointer(3, GL_INT, sizeof(GF_Vertex),  &mesh->vertices[0].pos);
#else
	glVertexPointer(3, GL_FLOAT, sizeof(GF_Vertex),  &mesh->vertices[0].pos);
#endif

	if (!tr_state->mesh_num_textures && (mesh->flags & MESH_HAS_COLOR)) {
		glEnable(GL_COLOR_MATERIAL);
#if !defined (GPAC_USE_OGL_ES)
		glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
#endif
		glEnableClientState(GL_COLOR_ARRAY);
		has_col = 1;

#if defined (GPAC_USE_OGL_ES)

		if (mesh->flags & MESH_HAS_ALPHA) {
			glEnable(GL_BLEND);
			tr_state->mesh_is_transparent = 1;
		}
#ifdef MESH_USE_SFCOLOR
		/*glES only accepts full RGBA colors*/
		glColorPointer(4, GL_FIXED, sizeof(GF_Vertex), &mesh->vertices[0].color);
#else
		glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(GF_Vertex), &mesh->vertices[0].color);
#endif	/*MESH_USE_SFCOLOR*/

#elif defined (GPAC_FIXED_POINT)


#ifdef MESH_USE_SFCOLOR
		/*this is a real pain: we cannot "scale" colors through openGL, and our components are 16.16 (32 bytes) ranging
		from [0 to 65536] mapping to [0, 1.0], but openGL assumes for s32 a range from [-2^31 2^31] mapping to [0, 1.0]
		we must thus rebuild a dedicated array...*/
		if (mesh->flags & MESH_HAS_ALPHA) {
			u32 i;
			color_array = malloc(sizeof(Float)*4*mesh->v_count);
			for (i=0; i<mesh->v_count; i++) {
				color_array[4*i] = FIX2FLT(mesh->vertices[i].color.red);
				color_array[4*i+1] = FIX2FLT(mesh->vertices[i].color.green);
				color_array[4*i+2] = FIX2FLT(mesh->vertices[i].color.blue);
				color_array[4*i+3] = FIX2FLT(mesh->vertices[i].color.alpha);
			}
			glEnable(GL_BLEND);
			glColorPointer(4, GL_FLOAT, 4*sizeof(Float), color_array);
			tr_state->mesh_is_transparent = 1;
		} else {
			color_array = malloc(sizeof(Float)*3*mesh->v_count);
			for (i=0; i<mesh->v_count; i++) {
				color_array[3*i] = FIX2FLT(mesh->vertices[i].color.red);
				color_array[3*i+1] = FIX2FLT(mesh->vertices[i].color.green);
				color_array[3*i+2] = FIX2FLT(mesh->vertices[i].color.blue);
			}
			glColorPointer(3, GL_FLOAT, 3*sizeof(Float), color_array);
		}
#else
		glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(GF_Vertex), &mesh->vertices[0].color);
#endif /*MESH_USE_SFCOLOR*/

#else	

#ifdef MESH_USE_SFCOLOR
		if (mesh->flags & MESH_HAS_ALPHA) {
			glEnable(GL_BLEND);
			glColorPointer(4, GL_FLOAT, sizeof(GF_Vertex), &mesh->vertices[0].color);
			tr_state->mesh_is_transparent = 1;
		} else {
			glColorPointer(3, GL_FLOAT, sizeof(GF_Vertex), &mesh->vertices[0].color);
		}
#else
		if (mesh->flags & MESH_HAS_ALPHA) {
			glEnable(GL_BLEND);
			tr_state->mesh_is_transparent = 1;
			glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(GF_Vertex), &mesh->vertices[0].color);
		} else {
			glColorPointer(3, GL_UNSIGNED_BYTE, sizeof(GF_Vertex), &mesh->vertices[0].color);
		}
#endif /*MESH_USE_SFCOLOR*/

#endif
	}

	if (tr_state->mesh_num_textures && !mesh->mesh_type && !(mesh->flags & MESH_NO_TEXTURE)) {
		has_tx = 1;
#if defined(GPAC_USE_OGL_ES)
		glTexCoordPointer(2, GL_FIXED, sizeof(GF_Vertex), &mesh->vertices[0].texcoords);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY );
#elif defined(GPAC_FIXED_POINT)
		glMatrixMode(GL_TEXTURE);
		glPushMatrix();
		glScalef(fix_scale, fix_scale, fix_scale);
		glMatrixMode(GL_MODELVIEW);
		glTexCoordPointer(2, GL_INT, sizeof(GF_Vertex), &mesh->vertices[0].texcoords);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY );
#else
		if (tr_state->mesh_num_textures>1) {
			u32 i;
			for (i=0; i<tr_state->mesh_num_textures; i++) {
				compositor->gl_caps.glClientActiveTextureARB(GL_TEXTURE0_ARB + i);
				glTexCoordPointer(2, GL_FLOAT, sizeof(GF_Vertex), &mesh->vertices[0].texcoords);
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			}
		} else {
			glTexCoordPointer(2, GL_FLOAT, sizeof(GF_Vertex), &mesh->vertices[0].texcoords);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY );
		}
#endif
	}

	if (mesh->mesh_type) {
#ifdef GPAC_USE_OGL_ES
		glNormal3x(0, 0, FIX_ONE);
#else
		glNormal3f(0, 0, 1.0f);
#endif
		glDisable(GL_CULL_FACE);
		glDisable(GL_LIGHTING);
		if (mesh->mesh_type==2) glDisable(GL_LINE_SMOOTH);
		else glDisable(GL_POINT_SMOOTH);

#if !defined(GPAC_USE_TINYGL) && !defined(GL_ES_CL_PROFILE)
		glLineWidth(1.0f);
#endif
	
	} else {
		u32 normal_type = GL_FLOAT;
		has_norm = 1;
		glEnableClientState(GL_NORMAL_ARRAY );
#ifdef MESH_USE_FIXED_NORMAL

#if defined(GPAC_USE_OGL_ES)
		normal_type = GL_FIXED;
#elif defined(GPAC_FIXED_POINT)
		normal_type = GL_INT;
#else
		normal_type = GL_FLOAT;
#endif

#else /*MESH_USE_FIXED_NORMAL*/
		/*normals are stored on signed bytes*/
		normal_type = GL_BYTE;
#endif
		glNormalPointer(normal_type, sizeof(GF_Vertex), &mesh->vertices[0].normal);

		if (!mesh->mesh_type) {
			if (compositor->backcull 
				&& (!tr_state->mesh_is_transparent || (compositor->backcull ==GF_BACK_CULL_ALPHA) )
				&& (mesh->flags & MESH_IS_SOLID)) {
				glEnable(GL_CULL_FACE);
				glFrontFace((mesh->flags & MESH_IS_CW) ? GL_CW : GL_CCW);
			} else {
				glDisable(GL_CULL_FACE);
			}
		}
	}

	switch (mesh->mesh_type) {
	case MESH_LINESET: prim_type = GL_LINES; break;
	case MESH_POINTSET: prim_type = GL_POINTS; break;
	default: prim_type = GL_TRIANGLES; break;
	}

#if 1
	/*if inside or no aabb for the mesh draw vertex array*/
	if (compositor->disable_gl_cull || (tr_state->cull_flag==CULL_INSIDE) || !mesh->aabb_root || !mesh->aabb_root->pos)	{
#ifdef GPAC_USE_OGL_ES
		glDrawElements(prim_type, mesh->i_count, GL_UNSIGNED_SHORT, mesh->indices);
#else
		glDrawElements(prim_type, mesh->i_count, GL_UNSIGNED_INT, mesh->indices);
#endif
	} else {
		/*otherwise cull aabb against frustum - after some testing it appears (as usual) that there must 
		be a compromise: we're slowing down the compositor here, however the gain is really appreciable for 
		large meshes, especially terrains/elevation grids*/

		/*first get transformed frustum in local space*/
		GF_Matrix mx;
		u32 i, p_idx[6];
		GF_Plane fplanes[6];
		gf_mx_copy(mx, tr_state->model_matrix);
		gf_mx_inverse(&mx);
		for (i=0; i<6; i++) {
			fplanes[i] = tr_state->camera->planes[i];
			gf_mx_apply_plane(&mx, &fplanes[i]);
			p_idx[i] = gf_plane_get_p_vertex_idx(&fplanes[i]);
		}
		/*then recursively cull & draw AABB tree*/
		VS3D_DrawAABBNode(tr_state, mesh, prim_type, fplanes, p_idx, mesh->aabb_root->pos);
		VS3D_DrawAABBNode(tr_state, mesh, prim_type, fplanes, p_idx, mesh->aabb_root->neg);
	}

#endif

	glDisableClientState(GL_VERTEX_ARRAY);
	if (has_col) glDisableClientState(GL_COLOR_ARRAY);
	glDisable(GL_COLOR_MATERIAL);

	if (has_tx) glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	if (has_norm) glDisableClientState(GL_NORMAL_ARRAY);

#if defined(GPAC_FIXED_POINT) && !defined(GPAC_USE_OGL_ES)
	if (color_array) free(color_array);
	if (!mesh->mesh_type && !(mesh->flags & MESH_NO_TEXTURE)) {
		glMatrixMode(GL_TEXTURE);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
	}
	glPopMatrix();
#endif
	
	if (tr_state->mesh_is_transparent) glDisable(GL_BLEND);
	tr_state->mesh_is_transparent = 0;
}

#ifdef GPAC_USE_OGL_ES
u32 ogles_push_enable(u32 mask)
{
	u32 attrib = 0;
#if !defined(__SYMBIAN32__) && !defined(GL_ES_CL_PROFILE)
	if ((mask & GL_LIGHTING) && glIsEnabled(GL_LIGHTING) ) attrib |= GL_LIGHTING;
	if ((mask & GL_BLEND) && glIsEnabled(GL_BLEND) ) attrib |= GL_BLEND;
	if ((mask & GL_COLOR_MATERIAL) && glIsEnabled(GL_COLOR_MATERIAL) ) attrib |= GL_COLOR_MATERIAL;
	if ((mask & GL_TEXTURE_2D) && glIsEnabled(GL_TEXTURE_2D) ) attrib |= GL_TEXTURE_2D;
#endif
	return attrib;
}
void ogles_pop_enable(u32 mask)
{
#if !defined(__SYMBIAN32__) && !defined(GL_ES_CL_PROFILE)
	if (mask & GL_LIGHTING) glEnable(GL_LIGHTING);
	if (mask & GL_BLEND) glEnable(GL_BLEND);
	if (mask & GL_COLOR_MATERIAL) glEnable(GL_COLOR_MATERIAL);
	if (mask & GL_TEXTURE_2D) glEnable(GL_TEXTURE_2D);
#endif
}
#endif

/*note we don't perform any culling for normal drawing...*/
void VS3D_DrawNormals(GF_TraverseState *tr_state, GF_Mesh *mesh)
{
#ifndef GPAC_USE_TINYGL

	GF_Vec pt, end;
	u32 i, j;
	Fixed scale = mesh->bounds.radius / 4;

#ifdef GPAC_USE_OGL_ES
	GF_Vec va[2];
	u16 indices[2];
	u32 attrib = ogles_push_enable(GL_LIGHTING | GL_BLEND | GL_COLOR_MATERIAL | GL_TEXTURE_2D);
	glEnableClientState(GL_VERTEX_ARRAY);
#else
	glPushAttrib(GL_ENABLE_BIT);
#endif

	glDisable(GL_LIGHTING | GL_BLEND | GL_COLOR_MATERIAL | GL_TEXTURE_2D);
#ifdef GPAC_USE_OGL_ES
	glColor4x(0, 0, 0, 1);
#else
	glColor3f(1, 1, 1);
#endif

	if (tr_state->visual->compositor->draw_normals==GF_NORMALS_VERTEX) {
		IDX_TYPE *idx = mesh->indices;
		for (i=0; i<mesh->i_count; i+=3) {
			for (j=0; j<3; j++) {
				pt = mesh->vertices[idx[j]].pos;
				MESH_GET_NORMAL(end, mesh->vertices[idx[j]]);
				end = gf_vec_scale(end, scale);
				gf_vec_add(end, pt, end);
#ifdef GPAC_USE_OGL_ES
				va[0] = pt;
				va[1] = end;
				indices[0] = 0;
				indices[1] = 1;
				glVertexPointer(3, GL_FIXED, 0, va);
				glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT, indices);
#else
				glBegin(GL_LINES);
				glVertex3f(FIX2FLT(pt.x), FIX2FLT(pt.y), FIX2FLT(pt.z));
				glVertex3f(FIX2FLT(end.x), FIX2FLT(end.y), FIX2FLT(end.z));
				glEnd();
#endif
			}
			idx+=3;
		}
	} else {
		IDX_TYPE *idx = mesh->indices;
		for (i=0; i<mesh->i_count; i+=3) {
			gf_vec_add(pt, mesh->vertices[idx[0]].pos, mesh->vertices[idx[1]].pos);
			gf_vec_add(pt, pt, mesh->vertices[idx[2]].pos);
			pt = gf_vec_scale(pt, FIX_ONE/3);
			MESH_GET_NORMAL(end, mesh->vertices[idx[0]]);
			end = gf_vec_scale(end, scale);
			gf_vec_add(end, pt, end);
#ifdef GPAC_USE_OGL_ES
				va[0] = pt;
				va[1] = end;
				indices[0] = 0;
				indices[1] = 1;
				glVertexPointer(3, GL_FIXED, 0, va);
				glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT, indices);
#else
				glBegin(GL_LINES);
				glVertex3f(FIX2FLT(pt.x), FIX2FLT(pt.y), FIX2FLT(pt.z));
				glVertex3f(FIX2FLT(end.x), FIX2FLT(end.y), FIX2FLT(end.z));
				glEnd();
#endif
			idx += 3;
		}
	}
#ifdef GPAC_USE_OGL_ES
	ogles_pop_enable(attrib);
	glDisableClientState(GL_VERTEX_ARRAY);
#else
	glPopAttrib();
#endif

#endif	/*GPAC_USE_TINYGL*/
}


void VS3D_DrawAABBNodeBounds(GF_TraverseState *tr_state, AABBNode *node)
{
	if (node->pos) {
		VS3D_DrawAABBNodeBounds(tr_state, node->pos);
		VS3D_DrawAABBNodeBounds(tr_state, node->neg);
	} else {
		SFVec3f c, s;
		gf_vec_diff(s, node->max, node->min);
		c = gf_vec_scale(s, FIX_ONE/2);
		gf_vec_add(c, node->min, c);

		glPushMatrix();
#ifdef GPAC_USE_OGL_ES
		glTranslatex(c.x, c.y, c.z);
		glScalex(s.x, s.y, s.z);
#else
		glTranslatef(FIX2FLT(c.x), FIX2FLT(c.y), FIX2FLT(c.z));
		glScalef(FIX2FLT(s.x), FIX2FLT(s.y), FIX2FLT(s.z));
#endif
		VS3D_DrawMeshIntern(tr_state, tr_state->visual->compositor->unit_bbox);
		glPopMatrix();
	}
}

void visual_3d_draw_bbox(GF_TraverseState *tr_state, GF_BBox *box)
{
	SFVec3f c, s;
#ifdef GPAC_USE_TINYGL

#elif defined(GPAC_USE_OGL_ES)
	u32 atts = ogles_push_enable(GL_LIGHTING);
#else
	glPushAttrib(GL_ENABLE_BIT);
#endif
	gf_vec_diff(s, box->max_edge, box->min_edge);
	c.x = box->min_edge.x + s.x/2;
	c.y = box->min_edge.y + s.y/2;
	c.z = box->min_edge.z + s.z/2;

	visual_3d_set_material_2d_argb(tr_state->visual, tr_state->visual->compositor->highlight_stroke);
	glPushMatrix();
	
#ifdef GPAC_USE_OGL_ES
	glTranslatex(c.x, c.y, c.z);
	glScalex(s.x, s.y, s.z);
#else
	glTranslatef(FIX2FLT(c.x), FIX2FLT(c.y), FIX2FLT(c.z));
	glScalef(FIX2FLT(s.x), FIX2FLT(s.y), FIX2FLT(s.z));
//	glScalef(1.1f, 1.1f, 1.1f);
#endif
	VS3D_DrawMeshIntern(tr_state, tr_state->visual->compositor->unit_bbox);
	glPopMatrix();

#ifdef GPAC_USE_TINYGL

#elif defined(GPAC_USE_OGL_ES)
	ogles_pop_enable(atts);
#else
	glPopAttrib();
#endif

}
//#endif


void VS3D_DrawMeshBoundingVolume(GF_TraverseState *tr_state, GF_Mesh *mesh)
{
#ifndef GPAC_USE_TINYGL
	if (mesh->aabb_root && (tr_state->visual->compositor->draw_bvol==GF_BOUNDS_AABB)) {
#ifdef GPAC_USE_OGL_ES
		u32 atts = ogles_push_enable(GL_LIGHTING);
#else
		glPushAttrib(GL_ENABLE_BIT);
#endif
		glDisable(GL_LIGHTING);
		VS3D_DrawAABBNodeBounds(tr_state, mesh->aabb_root);
#ifdef GPAC_USE_OGL_ES
		ogles_pop_enable(atts);
#else
		glPopAttrib();
#endif
	} else {
		visual_3d_draw_bbox(tr_state, &mesh->bounds);
	}


#endif /*GPAC_USE_TINYGL*/
}

void visual_3d_mesh_paint(GF_TraverseState *tr_state, GF_Mesh *mesh)
{
	Bool mesh_drawn = 0;
	if (tr_state->visual->compositor->wiremode != GF_WIREFRAME_ONLY) {
		VS3D_DrawMeshIntern(tr_state, mesh);
		mesh_drawn = 1;
	}

	if (tr_state->visual->compositor->draw_normals) VS3D_DrawNormals(tr_state, mesh);
	if (!mesh->mesh_type && (tr_state->visual->compositor->wiremode != GF_WIREFRAME_NONE)) {
		glDisable(GL_LIGHTING);
#ifdef GPAC_USE_OGL_ES
		if (mesh_drawn) glColor4x(0, 0, 0, FIX_ONE);
#else
		if (mesh_drawn) glColor4f(0, 0, 0, 1.0f);
#endif
		glEnableClientState(GL_VERTEX_ARRAY);
#ifdef GPAC_USE_OGL_ES
		glVertexPointer(3, GL_FIXED, sizeof(GF_Vertex),  &mesh->vertices[0].pos);
		glDrawElements(GL_LINES, mesh->i_count, GL_UNSIGNED_SHORT, mesh->indices);
#else
		glVertexPointer(3, GL_FLOAT, sizeof(GF_Vertex),  &mesh->vertices[0].pos);
		glDrawElements(GL_LINES, mesh->i_count, GL_UNSIGNED_INT, mesh->indices);
#endif
		glDisableClientState(GL_VERTEX_ARRAY);
	}
	if (tr_state->visual->compositor->draw_bvol) VS3D_DrawMeshBoundingVolume(tr_state, mesh);
}

#if !defined(GPAC_USE_OGL_ES) && !defined(GPAC_USE_TINYGL)


static GLubyte hatch_horiz[] = {
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff
};

static GLubyte hatch_vert[] = {
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa
};

static GLubyte hatch_up[] = {
	0xc0, 0xc0, 0xc0, 0xc0, 0x30, 0x30, 0x30, 0x30,
	0x0c, 0x0c, 0x0c, 0x0c, 0x03, 0x03, 0x03, 0x03,
	0xc0, 0xc0, 0xc0, 0xc0, 0x30, 0x30, 0x30, 0x30,
	0x0c, 0x0c, 0x0c, 0x0c, 0x03, 0x03, 0x03, 0x03,
	0xc0, 0xc0, 0xc0, 0xc0, 0x30, 0x30, 0x30, 0x30,
	0x0c, 0x0c, 0x0c, 0x0c, 0x03, 0x03, 0x03, 0x03,
	0xc0, 0xc0, 0xc0, 0xc0, 0x30, 0x30, 0x30, 0x30,
	0x0c, 0x0c, 0x0c, 0x0c, 0x03, 0x03, 0x03, 0x03,
	0xc0, 0xc0, 0xc0, 0xc0, 0x30, 0x30, 0x30, 0x30,
	0x0c, 0x0c, 0x0c, 0x0c, 0x03, 0x03, 0x03, 0x03,
	0xc0, 0xc0, 0xc0, 0xc0, 0x30, 0x30, 0x30, 0x30,
	0x0c, 0x0c, 0x0c, 0x0c, 0x03, 0x03, 0x03, 0x03,
	0xc0, 0xc0, 0xc0, 0xc0, 0x30, 0x30, 0x30, 0x30,
	0x0c, 0x0c, 0x0c, 0x0c, 0x03, 0x03, 0x03, 0x03,
	0xc0, 0xc0, 0xc0, 0xc0, 0x30, 0x30, 0x30, 0x30,
	0x0c, 0x0c, 0x0c, 0x0c, 0x03, 0x03, 0x03, 0x03
};

static GLubyte hatch_down[] = {
	0x03, 0x03, 0x03, 0x03, 0x0c, 0x0c, 0x0c, 0x0c,
	0x30, 0x30, 0x30, 0x30, 0xc0, 0xc0, 0xc0, 0xc0,
	0x03, 0x03, 0x03, 0x03, 0x0c, 0x0c, 0x0c, 0x0c,
	0x30, 0x30, 0x30, 0x30, 0xc0, 0xc0, 0xc0, 0xc0,
	0x03, 0x03, 0x03, 0x03, 0x0c, 0x0c, 0x0c, 0x0c,
	0x30, 0x30, 0x30, 0x30, 0xc0, 0xc0, 0xc0, 0xc0,
	0x03, 0x03, 0x03, 0x03, 0x0c, 0x0c, 0x0c, 0x0c,
	0x30, 0x30, 0x30, 0x30, 0xc0, 0xc0, 0xc0, 0xc0,
	0x03, 0x03, 0x03, 0x03, 0x0c, 0x0c, 0x0c, 0x0c,
	0x30, 0x30, 0x30, 0x30, 0xc0, 0xc0, 0xc0, 0xc0,
	0x03, 0x03, 0x03, 0x03, 0x0c, 0x0c, 0x0c, 0x0c,
	0x30, 0x30, 0x30, 0x30, 0xc0, 0xc0, 0xc0, 0xc0,
	0x03, 0x03, 0x03, 0x03, 0x0c, 0x0c, 0x0c, 0x0c,
	0x30, 0x30, 0x30, 0x30, 0xc0, 0xc0, 0xc0, 0xc0,
	0x03, 0x03, 0x03, 0x03, 0x0c, 0x0c, 0x0c, 0x0c,
	0x30, 0x30, 0x30, 0x30, 0xc0, 0xc0, 0xc0, 0xc0
};

static GLubyte hatch_cross[] = {
	0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
	0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
	0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
	0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c
};

void visual_3d_mesh_hatch(GF_TraverseState *tr_state, GF_Mesh *mesh, u32 hatchStyle, SFColor hatchColor)
{
	if (mesh->mesh_type) return;

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, sizeof(GF_Vertex),  &mesh->vertices[0].pos);
	if (mesh->mesh_type || (mesh->flags & MESH_IS_2D)) {
		glDisableClientState(GL_NORMAL_ARRAY);
		if (mesh->mesh_type) glDisable(GL_LIGHTING);
		glNormal3f(0, 0, 1.0f);
		glDisable(GL_CULL_FACE);
	} else {
		glEnableClientState(GL_NORMAL_ARRAY );
		glNormalPointer(GL_FLOAT, sizeof(GF_Vertex), &mesh->vertices[0].normal);

		if (!mesh->mesh_type) {
			/*if mesh is transparent DON'T CULL*/
			if (!tr_state->mesh_is_transparent && (mesh->flags & MESH_IS_SOLID)) {
				glEnable(GL_CULL_FACE);
				glFrontFace((mesh->flags & MESH_IS_CW) ? GL_CW : GL_CCW);
			} else {
				glDisable(GL_CULL_FACE);
			}
		}
	}

	glEnable(GL_POLYGON_STIPPLE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	/*can't access ISO International Register of Graphical Items www site :)*/
	switch (hatchStyle) {
	case 5: glPolygonStipple(hatch_cross); break;
	case 4: glPolygonStipple(hatch_up); break;
	case 3: glPolygonStipple(hatch_down); break;
	case 2: glPolygonStipple(hatch_vert); break;
	case 1: glPolygonStipple(hatch_horiz); break;
	default: glDisable(GL_POLYGON_STIPPLE); break;
	}
	glColor3f(FIX2FLT(hatchColor.red), FIX2FLT(hatchColor.green), FIX2FLT(hatchColor.blue));
	glDrawElements(GL_TRIANGLES, mesh->i_count, GL_UNSIGNED_INT, mesh->indices);

	glDisable(GL_POLYGON_STIPPLE);
}
#endif

/*only used for ILS/ILS2D or IFS2D outline*/
void visual_3d_mesh_strike(GF_TraverseState *tr_state, GF_Mesh *mesh, Fixed width, Fixed line_scale, u32 dash_style)
{
#if !defined(GPAC_USE_OGL_ES) && !defined(GPAC_USE_TINYGL)
	u16 style;
#endif
	
	if (mesh->mesh_type != MESH_LINESET) return;
	if (line_scale) width = gf_mulfix(width, line_scale);
	width/=2;
#if !defined(GPAC_USE_TINYGL) && !defined(GL_ES_CL_PROFILE)
	glLineWidth( FIX2FLT(width));
#endif

#if !defined(GPAC_USE_OGL_ES) && !defined(GPAC_USE_TINYGL)

	switch (dash_style) {
	case GF_DASH_STYLE_DASH: style = 0x1F1F; break;
	case GF_DASH_STYLE_DOT: style = 0x3333; break;
	case GF_DASH_STYLE_DASH_DOT: style = 0x6767; break;
	case GF_DASH_STYLE_DASH_DASH_DOT: style = 0x33CF; break;
	case GF_DASH_STYLE_DASH_DOT_DOT: style = 0x330F; break;
	default:
		style = 0;
		break;
	}
	if (style) {
		u32 factor = FIX2INT(width);
		if (!factor) factor = 1;
		glEnable(GL_LINE_STIPPLE);
		glLineStipple(factor, style); 
		visual_3d_mesh_paint(tr_state, mesh);
		glDisable (GL_LINE_STIPPLE);
	} else 
#endif
		visual_3d_mesh_paint(tr_state, mesh);
}

void visual_3d_set_material_2d(GF_VisualManager *visual, SFColor col, Fixed alpha)
{
	glDisable(GL_LIGHTING);
	if (alpha != FIX_ONE) {
		glEnable(GL_BLEND);
		visual_3d_enable_antialias(visual, 0);
	} else {
		glDisable(GL_BLEND);
		visual_3d_enable_antialias(visual, visual->compositor->antiAlias ? 1 : 0);
	}
#ifdef GPAC_USE_OGL_ES
	glColor4x(col.red, col.green, col.blue, alpha);
#else
	glColor4f(FIX2FLT(col.red), FIX2FLT(col.green), FIX2FLT(col.blue), FIX2FLT(alpha));
#endif
}

void visual_3d_set_material_2d_argb(GF_VisualManager *visual, u32 col)
{
	u32 a;
	a = GF_COL_A(col);

	glDisable(GL_LIGHTING);
	if (a != 0xFF) {
		glEnable(GL_BLEND);
		visual_3d_enable_antialias(visual, 0);
	} else {
		glDisable(GL_BLEND);
		visual_3d_enable_antialias(visual, visual->compositor->antiAlias ? 1 : 0);
	}
#ifdef GPAC_USE_OGL_ES
	glColor4x(GF_COL_R(col)<<8, GF_COL_G(col)<<8, GF_COL_B(col)<<8, a<<8);
#else
	glColor4f(GF_COL_R(col)/255.0f, GF_COL_G(col)/255.0f, GF_COL_B(col)/255.0f, a/255.0f);
#endif
}

void visual_3d_clear(GF_VisualManager *visual, SFColor color, Fixed alpha)
{
#ifdef GPAC_USE_OGL_ES
	glClearColorx(color.red, color.green, color.blue, alpha);
#else
	glClearColor(FIX2FLT(color.red), FIX2FLT(color.green), FIX2FLT(color.blue), FIX2FLT(alpha));
#endif
	glClear(GL_COLOR_BUFFER_BIT);
}

#if !defined(GPAC_USE_OGL_ES) && !defined(GPAC_USE_TINYGL)

void visual_3d_draw_image(GF_VisualManager *visual, Fixed pos_x, Fixed pos_y, u32 width, u32 height, u32 pixelformat, char *data, Fixed scale_x, Fixed scale_y)
{
	u32 gl_format;
	glPixelZoom(FIX2FLT(scale_x), FIX2FLT(scale_y));

	gl_format = 0;
	switch (pixelformat) {
	case GF_PIXEL_RGB_24:
		gl_format = GL_RGB;
		break;
	case GF_PIXEL_RGB_32:
	case GF_PIXEL_RGBA:
		gl_format = GL_RGBA;
		break;
	case GF_PIXEL_ARGB:
		if (!visual->compositor->gl_caps.bgra_texture) return;
		gl_format = GL_BGRA_EXT;
		break;
	default:
		return;
	}

	/*glRasterPos2f doesn't accept point outside the view volume (it invalidates all draw pixel, draw bitmap)
	so we move to the center of the local coord system, draw a NULL bitmap with raster pos displacement*/
	glRasterPos2f(0, 0);
	glBitmap(0, 0, 0, 0, FIX2FLT(pos_x), -FIX2FLT(pos_y), NULL);
	glDrawPixels(width, height, gl_format, GL_UNSIGNED_BYTE, data);
	glBitmap(0, 0, 0, 0, -FIX2FLT(pos_x), FIX2FLT(pos_y), NULL);

}


void visual_3d_matrix_get(GF_VisualManager *visual, u32 mat_type, Fixed *mat)
{
#ifdef GPAC_FIXED_POINT
	u32 i = 0;
#endif
	Float _mat[16];
	switch (mat_type) {
	case V3D_MATRIX_MODELVIEW:
		glGetFloatv(GL_MODELVIEW_MATRIX, _mat);
		break;
	case V3D_MATRIX_PROJECTION:
		glGetFloatv(GL_PROJECTION_MATRIX, _mat);
		break;
	case V3D_MATRIX_TEXTURE:
		glGetFloatv(GL_TEXTURE_MATRIX, _mat);
		break;
	}
#ifdef GPAC_FIXED_POINT
	for (i=0; i<16; i++) mat[i] = FLT2FIX(_mat[i]);
#else
	memcpy(mat, _mat, sizeof(Fixed)*16);
#endif
}

#endif

void visual_3d_set_matrix_mode(GF_VisualManager *visual, u32 mat_type)
{
	switch (mat_type) {
	case V3D_MATRIX_MODELVIEW:
		glMatrixMode(GL_MODELVIEW);
		break;
	case V3D_MATRIX_PROJECTION:
		glMatrixMode(GL_PROJECTION);
		break;
	case V3D_MATRIX_TEXTURE:
		glMatrixMode(GL_TEXTURE);
		break;
	}
}

void visual_3d_matrix_reset(GF_VisualManager *visual)
{
	glLoadIdentity();
}
void visual_3d_matrix_push(GF_VisualManager *visual)
{
	glPushMatrix();
}
void visual_3d_matrix_add(GF_VisualManager *visual, Fixed *mat)
{
#ifdef GPAC_USE_OGL_ES
	glMultMatrixx(mat);
#elif defined(GPAC_FIXED_POINT)
	u32 i;
	Float _mat[16];
	for (i=0; i<16; i++) _mat[i] = FIX2FLT(mat[i]);
	glMultMatrixf(_mat);
#else
	glMultMatrixf(mat);
#endif
}

void visual_3d_matrix_pop(GF_VisualManager *visual)
{
	glPopMatrix();
}

void visual_3d_matrix_load(GF_VisualManager *visual, Fixed *mat)
{
#ifdef GPAC_USE_OGL_ES
	glLoadMatrixx(mat);
#elif defined(GPAC_FIXED_POINT)
	Float _mat[16];
	u32 i;
	for (i=0; i<16; i++) _mat[i] = FIX2FLT(mat[i]);
	glLoadMatrixf(_mat);
#else
	glLoadMatrixf(mat);
#endif
}


void visual_3d_set_clipper_2d(GF_VisualManager *visual, GF_Rect clip)
{
#ifdef GL_MAX_CLIP_PLANES

#ifdef GPAC_USE_OGL_ES

	Fixed g[4];
	u32 cp;
	visual_3d_reset_clipper_2d(visual);
	if (visual->num_clips + 4 > visual->max_clips)  return;
	cp = visual->num_clips;
	g[2] = 0; g[1] = 0; 
	g[3] = clip.x + clip.width; g[0] = -FIX_ONE; 
	glClipPlanex(GL_CLIP_PLANE0 + cp, g); glEnable(GL_CLIP_PLANE0 + cp);
	g[3] = -clip.x; g[0] = FIX_ONE;
	glClipPlanex(GL_CLIP_PLANE0 + cp + 1, g); glEnable(GL_CLIP_PLANE0 + cp + 1);
	g[0] = 0;
	g[3] = clip.y; g[1] = -FIX_ONE; 
	glClipPlanex(GL_CLIP_PLANE0 + cp + 2, g); glEnable(GL_CLIP_PLANE0 + cp + 2);
	g[3] = clip.height - clip.y; g[1] = FIX_ONE; 
	glClipPlanex(GL_CLIP_PLANE0 + cp + 3, g); glEnable(GL_CLIP_PLANE0 + cp + 3);
	visual->num_clips += 4;
#else
	Double g[4];
	u32 cp;
	visual_3d_reset_clipper_2d(visual);
	if (visual->num_clips + 4 > visual->max_clips) return;
	cp = visual->num_clips;
	g[2] = 0; 
	g[1] = 0; 
	g[3] = FIX2FLT(clip.x) + FIX2FLT(clip.width); g[0] = -1; 
	glClipPlane(GL_CLIP_PLANE0 + cp, g); glEnable(GL_CLIP_PLANE0 + cp);
	g[3] = -FIX2FLT(clip.x); g[0] = 1;
	glClipPlane(GL_CLIP_PLANE0 + cp + 1, g); glEnable(GL_CLIP_PLANE0 + cp + 1);
	g[0] = 0;
	g[3] = FIX2FLT(clip.y); g[1] = -1; 
	glClipPlane(GL_CLIP_PLANE0 + cp + 2, g); glEnable(GL_CLIP_PLANE0 + cp + 2);
	g[3] = FIX2FLT(clip.height-clip.y); g[1] = 1; 
	glClipPlane(GL_CLIP_PLANE0 + cp + 3, g); glEnable(GL_CLIP_PLANE0 + cp + 3);
	visual->num_clips += 4;
#endif

#endif
}

void visual_3d_reset_clipper_2d(GF_VisualManager *visual)
{
#ifdef GL_MAX_CLIP_PLANES
	u32 cp;
	if (visual->num_clips < 4) return;
	cp = visual->num_clips - 4;
	glDisable(GL_CLIP_PLANE0 + cp + 3);
	glDisable(GL_CLIP_PLANE0 + cp + 2);
	glDisable(GL_CLIP_PLANE0 + cp + 1);
	glDisable(GL_CLIP_PLANE0 + cp);
	visual->num_clips -= 4;
#endif
}

void visual_3d_set_clip_plane(GF_VisualManager *visual, GF_Plane p)
{
#ifdef GL_MAX_CLIP_PLANES

#ifdef GPAC_USE_OGL_ES
	Fixed g[4];
	if (visual->num_clips + 1 > visual->max_clips) return;
	gf_vec_norm(&p.normal);
	g[0] = p.normal.x;
	g[1] = p.normal.y;
	g[2] = p.normal.z;
	g[3] = p.d;
	glClipPlanex(GL_CLIP_PLANE0 + visual->num_clips, g); 
#else
	Double g[4];
	if (visual->num_clips + 1 > visual->max_clips) return;
	gf_vec_norm(&p.normal);
	g[0] = FIX2FLT(p.normal.x);
	g[1] = FIX2FLT(p.normal.y);
	g[2] = FIX2FLT(p.normal.z);
	g[3] = FIX2FLT(p.d);
	glClipPlane(GL_CLIP_PLANE0 + visual->num_clips, g); 
#endif
	glEnable(GL_CLIP_PLANE0 + visual->num_clips);
	visual->num_clips++;
#endif

}

void visual_3d_reset_clip_plane(GF_VisualManager *visual)
{
#ifdef GL_MAX_CLIP_PLANES
	if (!visual->num_clips) return;
	glDisable(GL_CLIP_PLANE0 + visual->num_clips-1);
	visual->num_clips -= 1;
#endif
}

void visual_3d_set_material(GF_VisualManager *visual, u32 material_type, Fixed *rgba)
{
	GLenum mode;
#if defined(GPAC_USE_OGL_ES)
	Fixed *_rgba = rgba;
#elif defined(GPAC_FIXED_POINT)
	Float _rgba[4];
	_rgba[0] = FIX2FLT(rgba[0]); _rgba[1] = FIX2FLT(rgba[1]); _rgba[2] = FIX2FLT(rgba[2]); _rgba[3] = FIX2FLT(rgba[3]);
#else
	Float *_rgba = rgba;
#endif

	switch (material_type) {
	case V3D_MATERIAL_AMBIENT: mode = GL_AMBIENT; break;
	case V3D_MATERIAL_DIFFUSE: mode = GL_DIFFUSE; break;
	case V3D_MATERIAL_SPECULAR: mode = GL_SPECULAR; break;
	case V3D_MATERIAL_EMISSIVE: mode = GL_EMISSION; break;

	case V3D_MATERIAL_NONE:
#ifdef GPAC_USE_OGL_ES
		glColor4x(_rgba[0], _rgba[1], _rgba[2], _rgba[3]);
#else
		glColor4fv(_rgba);
#endif
		/*fall-through*/
	default:
		return;
	}
#ifdef GPAC_USE_OGL_ES
	glMaterialxv(GL_FRONT_AND_BACK, mode, _rgba);
#else
	glMaterialfv(GL_FRONT_AND_BACK, mode, _rgba);
#endif
}

void visual_3d_set_shininess(GF_VisualManager *visual, Fixed shininess)
{
#ifdef GPAC_USE_OGL_ES
	glMaterialx(GL_FRONT_AND_BACK, GL_SHININESS, shininess * 128);
#else
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, FIX2FLT(shininess) * 128);
#endif
}

void visual_3d_set_state(GF_VisualManager *visual, u32 flag_mask, Bool setOn)
{
	if (setOn) {
		if (flag_mask & V3D_STATE_LIGHT) glEnable(GL_LIGHTING);
		if (flag_mask & V3D_STATE_BLEND) glEnable(GL_BLEND);
		if (flag_mask & V3D_STATE_COLOR) glEnable(GL_COLOR_MATERIAL);
	} else {
		if (flag_mask & V3D_STATE_LIGHT) 
			glDisable(GL_LIGHTING);
		if (flag_mask & V3D_STATE_BLEND) glDisable(GL_BLEND);
#ifdef GPAC_USE_OGL_ES
		if (flag_mask & V3D_STATE_COLOR) glDisable(GL_COLOR_MATERIAL);
#else
		if (flag_mask & V3D_STATE_COLOR) glDisable(GL_COLOR_MATERIAL | GL_COLOR_MATERIAL_FACE);
#endif
	}
}

Bool visual_3d_add_spot_light(GF_VisualManager *visual, Fixed _ambientIntensity, SFVec3f attenuation, Fixed _beamWidth, 
					   SFColor color, Fixed _cutOffAngle, SFVec3f direction, Fixed _intensity, SFVec3f location)
{
#ifdef GPAC_USE_OGL_ES
	Fixed vals[4], exp;
#else
	Float vals[4], intensity, cutOffAngle, beamWidth, ambientIntensity, exp;
#endif
	GLint iLight;

	if (!visual->num_lights) glEnable(GL_LIGHTING);
	if (visual->num_lights==visual->max_lights) return 0;
	iLight = GL_LIGHT0 + visual->num_lights;
	visual->num_lights++;
	glEnable(iLight);

#ifndef GPAC_USE_OGL_ES
	ambientIntensity = FIX2FLT(_ambientIntensity);
	intensity = FIX2FLT(_intensity);
	cutOffAngle = FIX2FLT(_cutOffAngle);
	beamWidth = FIX2FLT(_beamWidth);
#endif

	/*in case...*/
	gf_vec_norm(&direction);

#ifdef GPAC_USE_OGL_ES
	vals[0] = direction.x; vals[1] = direction.y; vals[2] = direction.z; vals[3] = FIX_ONE;
	glLightxv(iLight, GL_SPOT_DIRECTION, vals);
	vals[0] = location.x; vals[1] = location.y; vals[2] = location.z; vals[3] = FIX_ONE;
	glLightxv(iLight, GL_POSITION, vals);
	glLightx(iLight, GL_CONSTANT_ATTENUATION, attenuation.x ? attenuation.x : FIX_ONE);
	glLightx(iLight, GL_LINEAR_ATTENUATION, attenuation.y);
	glLightx(iLight, GL_QUADRATIC_ATTENUATION, attenuation.z);
	vals[0] = gf_mulfix(color.red, _intensity); vals[1] = gf_mulfix(color.green, _intensity); vals[2] = gf_mulfix(color.blue, _intensity); vals[3] = FIX_ONE;
	glLightxv(iLight, GL_DIFFUSE, vals);
	glLightxv(iLight, GL_SPECULAR, vals);
	vals[0] = gf_mulfix(color.red, _ambientIntensity); vals[1] = gf_mulfix(color.green, _ambientIntensity); vals[2] = gf_mulfix(color.blue, _ambientIntensity); vals[3] = FIX_ONE;
	glLightxv(iLight, GL_AMBIENT, vals);

	if (!_beamWidth) exp = FIX_ONE;
	else if (_beamWidth>_cutOffAngle) exp = 0;
	else {
		exp = FIX_ONE - gf_cos(_beamWidth);
		if (exp>FIX_ONE) exp = FIX_ONE;
	}
	glLightx(iLight, GL_SPOT_EXPONENT,  exp*128);
	glLightx(iLight, GL_SPOT_CUTOFF, gf_divfix(180*_cutOffAngle, GF_PI) );
#else
	vals[0] = FIX2FLT(direction.x); vals[1] = FIX2FLT(direction.y); vals[2] = FIX2FLT(direction.z); vals[3] = 1;
	glLightfv(iLight, GL_SPOT_DIRECTION, vals);
	vals[0] = FIX2FLT(location.x); vals[1] = FIX2FLT(location.y); vals[2] = FIX2FLT(location.z); vals[3] = 1;
	glLightfv(iLight, GL_POSITION, vals);
	glLightf(iLight, GL_CONSTANT_ATTENUATION, attenuation.x ? FIX2FLT(attenuation.x) : 1.0f);
	glLightf(iLight, GL_LINEAR_ATTENUATION, FIX2FLT(attenuation.y));
	glLightf(iLight, GL_QUADRATIC_ATTENUATION, FIX2FLT(attenuation.z));
	vals[0] = FIX2FLT(color.red)*intensity; vals[1] = FIX2FLT(color.green)*intensity; vals[2] = FIX2FLT(color.blue)*intensity; vals[3] = 1;
	glLightfv(iLight, GL_DIFFUSE, vals);
	glLightfv(iLight, GL_SPECULAR, vals);
	vals[0] = FIX2FLT(color.red)*ambientIntensity; vals[1] = FIX2FLT(color.green)*ambientIntensity; vals[2] = FIX2FLT(color.blue)*ambientIntensity; vals[3] = 1;
	glLightfv(iLight, GL_AMBIENT, vals);

	//glLightf(iLight, GL_SPOT_EXPONENT, 0.5f * (beamWidth+0.001f) /*(Float) (0.5 * log(0.5) / log(cos(beamWidth)) ) */);
	if (!beamWidth) exp = 1;
	else if (beamWidth>cutOffAngle) exp = 0;
	else {
		exp = 1.0f - (Float) cos(beamWidth);
		if (exp>1) exp = 1;
	}
	glLightf(iLight, GL_SPOT_EXPONENT,  exp*128);
	glLightf(iLight, GL_SPOT_CUTOFF, 180*cutOffAngle/FIX2FLT(GF_PI));
#endif
	
	return 1;
}

/*insert pointlight - returns 0 if too many lights*/
Bool visual_3d_add_point_light(GF_VisualManager *visual, Fixed _ambientIntensity, SFVec3f attenuation, SFColor color, Fixed _intensity, SFVec3f location)
{
#ifdef GPAC_USE_OGL_ES
	Fixed vals[4];
#else
	Float vals[4], ambientIntensity, intensity;
#endif
	u32 iLight;

	if (!visual->num_lights) glEnable(GL_LIGHTING);
	if (visual->num_lights==visual->max_lights) return 0;
	iLight = GL_LIGHT0 + visual->num_lights;
	visual->num_lights++;
	glEnable(iLight);

#ifdef GPAC_USE_OGL_ES
	vals[0] = location.x; vals[1] = location.y; vals[2] = location.z; vals[3] = FIX_ONE;
	glLightxv(iLight, GL_POSITION, vals);
	glLightx(iLight, GL_CONSTANT_ATTENUATION, attenuation.x ? attenuation.x : FIX_ONE);
	glLightx(iLight, GL_LINEAR_ATTENUATION, attenuation.y);
	glLightx(iLight, GL_QUADRATIC_ATTENUATION, attenuation.z);
	vals[0] = gf_mulfix(color.red, _intensity); vals[1] = gf_mulfix(color.green, _intensity); vals[2] = gf_mulfix(color.blue, _intensity); vals[3] = FIX_ONE;
	glLightxv(iLight, GL_DIFFUSE, vals);
	glLightxv(iLight, GL_SPECULAR, vals);
	vals[0] = gf_mulfix(color.red, _ambientIntensity); vals[1] = gf_mulfix(color.green, _ambientIntensity); vals[2] = gf_mulfix(color.blue, _ambientIntensity); vals[3] = FIX_ONE;
	glLightxv(iLight, GL_AMBIENT, vals);

	glLightx(iLight, GL_SPOT_EXPONENT, 0);
	glLightx(iLight, GL_SPOT_CUTOFF, INT2FIX(180) );
#else
	ambientIntensity = FIX2FLT(_ambientIntensity);
	intensity = FIX2FLT(_intensity);

	vals[0] = FIX2FLT(location.x); vals[1] = FIX2FLT(location.y); vals[2] = FIX2FLT(location.z); vals[3] = 1;
	glLightfv(iLight, GL_POSITION, vals);

	glLightf(iLight, GL_CONSTANT_ATTENUATION, attenuation.x ? FIX2FLT(attenuation.x) : 1.0f);
	glLightf(iLight, GL_LINEAR_ATTENUATION, FIX2FLT(attenuation.y));
	glLightf(iLight, GL_QUADRATIC_ATTENUATION, FIX2FLT(attenuation.z));
	vals[0] = FIX2FLT(color.red)*intensity; vals[1] = FIX2FLT(color.green)*intensity; vals[2] = FIX2FLT(color.blue)*intensity; vals[3] = 1;
	glLightfv(iLight, GL_DIFFUSE, vals);
	glLightfv(iLight, GL_SPECULAR, vals);
	vals[0] = FIX2FLT(color.red)*ambientIntensity; vals[1] = FIX2FLT(color.green)*ambientIntensity; vals[2] = FIX2FLT(color.blue)*ambientIntensity; vals[3] = 1;
	glLightfv(iLight, GL_AMBIENT, vals);

	glLightf(iLight, GL_SPOT_EXPONENT, 0);
	glLightf(iLight, GL_SPOT_CUTOFF, 180);
#endif
	return 1;
}

Bool visual_3d_add_directional_light(GF_VisualManager *visual, Fixed _ambientIntensity, SFColor color, Fixed _intensity, SFVec3f direction)
{
#ifdef GPAC_USE_OGL_ES
	Fixed vals[4];
#else
	Float vals[4], ambientIntensity, intensity;
#endif
	u32 iLight;
	if (!visual->num_lights) glEnable(GL_LIGHTING);
	if (visual->num_lights==visual->max_lights) return 0;
	iLight = GL_LIGHT0 + visual->num_lights;
	visual->num_lights++;
	glEnable(iLight);

	/*in case...*/
	gf_vec_norm(&direction);
#ifdef GPAC_USE_OGL_ES
	vals[0] = -direction.x; vals[1] = -direction.y; vals[2] = -direction.z; vals[3] = 0;
	glLightxv(iLight, GL_POSITION, vals);
	vals[0] = gf_mulfix(color.red, _intensity); vals[1] = gf_mulfix(color.green, _intensity); vals[2] = gf_mulfix(color.blue, _intensity); vals[3] = FIX_ONE;
	glLightxv(iLight, GL_DIFFUSE, vals);
	glLightxv(iLight, GL_SPECULAR, vals);
	vals[0] = gf_mulfix(color.red, _ambientIntensity); vals[1] = gf_mulfix(color.green, _ambientIntensity); vals[2] = gf_mulfix(color.blue, _ambientIntensity); vals[3] = FIX_ONE;
	glLightxv(iLight, GL_AMBIENT, vals);

	glLightx(iLight, GL_CONSTANT_ATTENUATION, FIX_ONE);
	glLightx(iLight, GL_LINEAR_ATTENUATION, 0);
	glLightx(iLight, GL_QUADRATIC_ATTENUATION, 0);
	glLightx(iLight, GL_SPOT_CUTOFF, INT2FIX(180) );
#else
	ambientIntensity = FIX2FLT(_ambientIntensity);
	intensity = FIX2FLT(_intensity);

	vals[0] = -FIX2FLT(direction.x); vals[1] = -FIX2FLT(direction.y); vals[2] = -FIX2FLT(direction.z); vals[3] = 0;
	glLightfv(iLight, GL_POSITION, vals);
	vals[0] = FIX2FLT(color.red)*intensity; vals[1] = FIX2FLT(color.green)*intensity; vals[2] = FIX2FLT(color.blue)*intensity; vals[3] = 1;
	glLightfv(iLight, GL_DIFFUSE, vals);
	glLightfv(iLight, GL_SPECULAR, vals);
	vals[0] = FIX2FLT(color.red)*ambientIntensity; vals[1] = FIX2FLT(color.green)*ambientIntensity; vals[2] = FIX2FLT(color.blue)*ambientIntensity; vals[3] = 1;
	glLightfv(iLight, GL_AMBIENT, vals);

	glLightf(iLight, GL_CONSTANT_ATTENUATION, 1.0f);
	glLightf(iLight, GL_LINEAR_ATTENUATION, 0);
	glLightf(iLight, GL_QUADRATIC_ATTENUATION, 0);
	glLightf(iLight, GL_SPOT_CUTOFF, 180);
#endif
	return 1;
}

void visual_3d_remove_last_light(GF_VisualManager *visual)
{
	if (visual->num_lights) {
		glDisable(GL_LIGHT0+visual->num_lights-1);
		visual->num_lights--;
	}
}

void visual_3d_clear_all_lights(GF_VisualManager *visual)
{
	u32 i;
	for (i=visual->num_lights; i>0; i--) {
		glDisable(GL_LIGHT0+i-1);
	}
	visual->num_lights = 0;
	//glDisable(GL_LIGHTING);
}

void visual_3d_set_fog(GF_VisualManager *visual, const char *type, SFColor color, Fixed density, Fixed visibility)
{

#ifndef GPAC_USE_TINYGL

#ifdef GPAC_USE_OGL_ES
	Fixed vals[4];
	glEnable(GL_FOG);
	if (!type || !stricmp(type, "LINEAR")) glFogx(GL_FOG_MODE, GL_LINEAR);
	else if (!stricmp(type, "EXPONENTIAL")) glFogx(GL_FOG_MODE, GL_EXP);
	else if (!stricmp(type, "EXPONENTIAL2")) glFogx(GL_FOG_MODE, GL_EXP2);
	glFogx(GL_FOG_DENSITY, density);
	glFogx(GL_FOG_START, 0);
	glFogx(GL_FOG_END, visibility);
	vals[0] = color.red; vals[1] = color.green; vals[2] = color.blue; vals[3] = FIX_ONE;
	glFogxv(GL_FOG_COLOR, vals);
	glHint(GL_FOG_HINT, visual->compositor->high_speed ? GL_FASTEST : GL_NICEST);
#else
	Float vals[4];
	glEnable(GL_FOG);
	if (!type || !stricmp(type, "LINEAR")) glFogi(GL_FOG_MODE, GL_LINEAR);
	else if (!stricmp(type, "EXPONENTIAL")) glFogi(GL_FOG_MODE, GL_EXP);
	else if (!stricmp(type, "EXPONENTIAL2")) glFogi(GL_FOG_MODE, GL_EXP2);
	glFogf(GL_FOG_DENSITY, FIX2FLT(density));
	glFogf(GL_FOG_START, 0);
	glFogf(GL_FOG_END, FIX2FLT(visibility));
	vals[0] = FIX2FLT(color.red); vals[1] = FIX2FLT(color.green); vals[2] = FIX2FLT(color.blue); vals[3] = 1;
	glFogfv(GL_FOG_COLOR, vals);
	glHint(GL_FOG_HINT, visual->compositor->high_speed ? GL_FASTEST : GL_NICEST);
#endif

#endif

}

void visual_3d_fill_rect(GF_VisualManager *visual, GF_Rect rc, SFColorRGBA color)
{
	glDisable(GL_BLEND | GL_LIGHTING | GL_TEXTURE_2D);

#ifdef GPAC_USE_OGL_ES
	glNormal3x(0, 0, FIX_ONE);
	if (color.alpha!=FIX_ONE) glEnable(GL_BLEND);
	glColor4x(color.red, color.green, color.blue, color.alpha);
	{
		Fixed v[8];
		u16 indices[3];
		indices[0] = 0;
		indices[1] = 1;
		indices[2] = 2;

		v[0] = rc.x; v[1] = rc.y;
		v[2] = rc.x+rc.width; v[3] = rc.y-rc.height;
		v[4] = rc.x+rc.width; v[5] = rc.y;
		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(2, GL_FIXED, 0, v);
		glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, indices);

		v[4] = rc.x; v[5] = rc.y-rc.height;
		glVertexPointer(2, GL_FIXED, 0, v);
		glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, indices);

		glDisableClientState(GL_VERTEX_ARRAY);
	}
#else
	glNormal3f(0, 0, 1);
	if (color.alpha!=FIX_ONE) {
		glEnable(GL_BLEND);
		glColor4f(FIX2FLT(color.red), FIX2FLT(color.green), FIX2FLT(color.blue), FIX2FLT(color.alpha));
	} else {
		glColor3f(FIX2FLT(color.red), FIX2FLT(color.green), FIX2FLT(color.blue));
	}
	glBegin(GL_QUADS);
	glVertex3f(FIX2FLT(rc.x), FIX2FLT(rc.y), 0);
	glVertex3f(FIX2FLT(rc.x), FIX2FLT(rc.y-rc.height), 0);
	glVertex3f(FIX2FLT(rc.x+rc.width), FIX2FLT(rc.y-rc.height), 0);
	glVertex3f(FIX2FLT(rc.x+rc.width), FIX2FLT(rc.y), 0);
	glEnd();
	
	glDisable(GL_COLOR_MATERIAL | GL_COLOR_MATERIAL_FACE);
#endif

	glDisable(GL_BLEND);
}

GF_Err compositor_3d_get_screen_buffer(GF_Compositor *compositor, GF_VideoSurface *fb, u32 depth_dump_mode)
{
	/*FIXME*/
	u32 i, hy;
	char *tmp;

	fb->width = compositor->vp_width;
	fb->height = compositor->vp_height;

	/*depthmap-only dump*/
	if (depth_dump_mode==1) {

#ifdef GPAC_USE_OGL_ES
		return GF_NOT_SUPPORTED;
#else

		fb->pitch = compositor->vp_width; /* multiply by 4 if float depthbuffer */

#ifndef GPAC_USE_TINYGL
		fb->video_buffer = (char*)malloc(sizeof(char)* fb->pitch * fb->height);
#else
		fb->video_buffer = (char*)malloc(sizeof(char)* 2 * fb->pitch * fb->height);
#endif

		fb->pixel_format = GF_PIXEL_GREYSCALE;
#ifndef GPAC_USE_TINYGL
		glPixelTransferf(GL_DEPTH_SCALE, compositor->OGLDepthGain); 
		glPixelTransferf(GL_DEPTH_BIAS, compositor->OGLDepthOffset); 
#endif
		/*the following is the inverse z-transform of OpenGL*/		
#if 0		
		/* GL_FLOAT for float depthbuffer */
		glReadPixels(compositor->vp_x, compositor->vp_y, fb->width, fb->height, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, fb->video_buffer); 
#else
		{
		float *buff = (float *) malloc(sizeof(float)* fb->width * fb->height);
		Fixed n = compositor->traverse_state->camera->z_near;
		Fixed f = compositor->traverse_state->camera->z_far;
		
		glReadPixels(compositor->vp_x, compositor->vp_y, fb->width, fb->height, GL_DEPTH_COMPONENT, GL_FLOAT, buff); 

		for (i=0; i<fb->height*fb->width; i++) 
			fb->video_buffer[i] = (char) (255 * (1.0 - buff[i]) / (1 - buff[i]*(1-(n/f))));

		free(buff);
		
		}
		
#endif

#endif	/*GPAC_USE_OGL_ES*/
	}	

	/* RGBDS or RGBD dump*/
	else if (depth_dump_mode==2 || depth_dump_mode==3){
#ifdef GPAC_USE_OGL_ES
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor]: RGB+Depth format not implemented in OpenGL ES\n"));
		return GF_NOT_SUPPORTED;
#else
		char *depth_data=NULL;
		fb->pitch = compositor->vp_width*4; /* 4 bytes for each rgbds pixel */

#ifndef GPAC_USE_TINYGL
		fb->video_buffer = (char*)malloc(sizeof(char)* fb->pitch * fb->height);
#else
		fb->video_buffer = (char*)malloc(sizeof(char)* 2 * fb->pitch * fb->height);
#endif


		
#ifndef GPAC_USE_TINYGL	
		
		glReadPixels(0, 0, fb->width, fb->height, GL_RGBA, GL_UNSIGNED_BYTE, fb->video_buffer);

		glPixelTransferf(GL_DEPTH_SCALE, compositor->OGLDepthGain); 
		glPixelTransferf(GL_DEPTH_BIAS, compositor->OGLDepthOffset); 
		
		depth_data = (char*) malloc(sizeof(char)*fb->width*fb->height);
		glReadPixels(0, 0, fb->width, fb->height, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, depth_data);

		if (depth_dump_mode==2) {
			u32 i;
			fb->pixel_format = GF_PIXEL_RGBDS;
			
			/*this corresponds to the RGBDS ordering*/
			for (i=0; i<fb->height*fb->width; i++) {
				u8 ds;
				/* erase lowest-weighted depth bit */
				u8 depth = depth_data[i] & 0xfe; 
				/*get alpha*/
				ds = (fb->video_buffer[i*4 + 3]);
				/* if heaviest-weighted alpha bit is set (>128) , turn on shape bit*/
				if (ds & 0x80) depth |= 0x01;
				fb->video_buffer[i*4+3] = depth; /*insert depth onto alpha*/ 
			}
		/*this corresponds to RGBD ordering*/	
		} else if (depth_dump_mode==3) {
			u32 i;
			fb->pixel_format = GF_PIXEL_RGBD;
			for (i=0; i<fb->height*fb->width; i++) 
				fb->video_buffer[i*4+3] = depth_data[i];
		}
#else
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor]: RGB+Depth format not implemented in TinyGL\n"));
		return GF_NOT_SUPPORTED;
#endif
		
#endif /*GPAC_USE_OGL_ES*/
		
	} else {
		fb->pitch = 3*compositor->vp_width;
		fb->video_buffer = (char*)malloc(sizeof(char) * fb->pitch * fb->height);
		fb->pixel_format = GF_PIXEL_RGB_24;

		glReadPixels(compositor->vp_x, compositor->vp_y, fb->width, fb->height, GL_RGB, GL_UNSIGNED_BYTE, fb->video_buffer);
	}

	/*flip image (openGL always handle image data bottom to top) */
	tmp = (char*)malloc(sizeof(char)*fb->pitch);
	hy = fb->height/2;
	for (i=0; i<hy; i++) {
		memcpy(tmp, fb->video_buffer+ i*fb->pitch, fb->pitch);
		memcpy(fb->video_buffer + i*fb->pitch, fb->video_buffer + (fb->height - 1 - i) * fb->pitch, fb->pitch);
		memcpy(fb->video_buffer + (fb->height - 1 - i) * fb->pitch, tmp, fb->pitch);
	}
	free(tmp);
	return GF_OK;
}

GF_Err compositor_3d_release_screen_buffer(GF_Compositor *compositor, GF_VideoSurface *framebuffer)
{
	free(framebuffer->video_buffer);
	framebuffer->video_buffer = 0;
	return GF_OK;
}

#endif	/*GPAC_DISABLE_3D*/


