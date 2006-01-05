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

#include "visual_surface.h"
#include <gpac/options.h>

#include "gl_inc.h"

#define CHECK_GL_EXT(name) ((strstr(ext, name) != NULL) ? 1 : 0)
void R3D_LoadExtensions(Render3D *sr)
{
	const char *ext = glGetString(GL_EXTENSIONS);
	if (!ext) return;
	memset(&sr->hw_caps, 0, sizeof(HardwareCaps));

	if (CHECK_GL_EXT("GL_ARB_multisample") || CHECK_GL_EXT("GLX_ARB_multisample") || CHECK_GL_EXT("WGL_ARB_multisample")) 
		sr->hw_caps.multisample = 1;
	if (CHECK_GL_EXT("GL_ARB_texture_non_power_of_two")) 
		sr->hw_caps.npot_texture = 1;
	if (CHECK_GL_EXT("GL_EXT_abgr")) 
		sr->hw_caps.abgr_texture = 1;
	if (CHECK_GL_EXT("GL_EXT_bgra")) 
		sr->hw_caps.bgra_texture = 1;

	if (CHECK_GL_EXT("GL_EXT_texture_rectangle") || CHECK_GL_EXT("GL_NV_texture_rectangle")) 
		sr->hw_caps.rect_texture = 1;
}


void VS3D_Setup(VisualSurface *surf)
{
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
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
	glGetIntegerv(GL_MAX_LIGHTS, &surf->max_lights);
	glGetIntegerv(GL_MAX_CLIP_PLANES, &surf->max_clips);

	if (surf->render->compositor->high_speed) {
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
		glHint(GL_LINE_SMOOTH_HINT, GL_FASTEST);
		glHint(GL_POINT_SMOOTH_HINT, GL_FASTEST);
#ifdef GL_POLYGON_SMOOTH_HINT
		glHint(GL_POLYGON_SMOOTH_HINT, GL_FASTEST);
#endif
	} else {
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
		glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
		glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
#ifdef GL_POLYGON_SMOOTH_HINT
		glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
#endif
	}

	if (surf->render->compositor->antiAlias == GF_ANTIALIAS_FULL) {
		glEnable(GL_LINE_SMOOTH);
		glEnable(GL_POINT_SMOOTH);
#ifndef GPAC_USE_OGL_ES
		if (surf->render->poly_aa)
			glEnable(GL_POLYGON_SMOOTH);
		else
			glDisable(GL_POLYGON_SMOOTH);
#endif
	} else {
		glDisable(GL_LINE_SMOOTH);
		glDisable(GL_POINT_SMOOTH);
#ifndef GPAC_USE_OGL_ES
		glDisable(GL_POLYGON_SMOOTH);
#endif
	}

	glDisable(GL_COLOR_MATERIAL);
	glDisable(GL_LIGHTING);
	glEnable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_CULL_FACE);
	glDisable(GL_FOG);
	/*Note: we cannot enable/disable normalization on the fly, because we have no clue when the GL implementation
	will actually compute the related fragments. Since a typical world always use scaling, we always turn normalization on 
	to avoid tracking scale*/
	glEnable(GL_NORMALIZE);
	glClear(GL_DEPTH_BUFFER_BIT);
}

void VS3D_SetDepthBuffer(VisualSurface *surf, Bool on)
{
	if (on) glEnable(GL_DEPTH_TEST);
	else glDisable(GL_DEPTH_TEST);
}

void VS3D_SetHeadlight(VisualSurface *surf, Bool bOn, GF_Camera *cam)
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
	VS3D_AddDirectionalLight(surf, 0, col, FIX_ONE, dir);
}

void VS3D_SetViewport(VisualSurface *surf, GF_Rect vp)
{
	glViewport(FIX2INT(vp.x), FIX2INT(vp.y), FIX2INT(vp.width), FIX2INT(vp.height));
}

void VS3D_ClearDepth(VisualSurface *surf)
{
	glClear(GL_DEPTH_BUFFER_BIT);
}

void VS3D_DrawAABBNode(RenderEffect3D *eff, GF_Mesh *mesh, u32 prim_type, GF_Plane *fplanes, u32 *p_indices, AABBNode *n)
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
			VS3D_DrawAABBNode(eff, mesh, prim_type, fplanes, p_indices, n->pos);
			VS3D_DrawAABBNode(eff, mesh, prim_type, fplanes, p_indices, n->neg);
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

void VS3D_DrawMeshIntern(RenderEffect3D *eff, GF_Mesh *mesh)
{
	Bool has_col, has_tx, has_norm;
	u32 prim_type;
#if defined(GPAC_FIXED_POINT) && !defined(GPAC_USE_OGL_ES)
	u32 i;
	Float *color_array = NULL;
	Float fix_scale = 1.0f;
	fix_scale /= FIX_ONE;
#endif

	has_col = has_tx = has_norm = 0;

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

	if ((eff->mesh_has_texture != 1) && (mesh->flags & MESH_HAS_COLOR)) {
		glEnable(GL_COLOR_MATERIAL);
#if !defined (GPAC_USE_OGL_ES)
		glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
#endif
		glEnableClientState(GL_COLOR_ARRAY);
		has_col = 1;

#if defined (GPAC_USE_OGL_ES)
		if (mesh->flags & MESH_HAS_ALPHA) {
			glEnable(GL_BLEND);
			eff->mesh_is_transparent = 1;
		}
		/*glES only accepts full RGBA colors*/
		glColorPointer(4, GL_FIXED, sizeof(GF_Vertex), &mesh->vertices[0].color);
#elif defined (GPAC_FIXED_POINT)
		/*this is a real pain: we cannot "scale" colors through openGL, and our components are 16.16 (32 bytes) ranging
		from [0 to 65536] mapping to [0, 1.0], but openGL assumes for s32 a range from [-2^31 2^31] mapping to [0, 1.0]
		we must thus rebuild a dedicated array...*/
		if (mesh->flags & MESH_HAS_ALPHA) {
			color_array = malloc(sizeof(Float)*4*mesh->v_count);
			for (i=0; i<mesh->v_count; i++) {
				color_array[4*i] = FIX2FLT(mesh->vertices[i].color.red);
				color_array[4*i+1] = FIX2FLT(mesh->vertices[i].color.green);
				color_array[4*i+2] = FIX2FLT(mesh->vertices[i].color.blue);
				color_array[4*i+3] = FIX2FLT(mesh->vertices[i].color.alpha);
			}
			glEnable(GL_BLEND);
			glColorPointer(4, GL_FLOAT, 4*sizeof(Float), color_array);
			eff->mesh_is_transparent = 1;
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
		if (mesh->flags & MESH_HAS_ALPHA) {
			glEnable(GL_BLEND);
			glColorPointer(4, GL_FLOAT, sizeof(GF_Vertex), &mesh->vertices[0].color);
			eff->mesh_is_transparent = 1;
		} else {
			glColorPointer(3, GL_FLOAT, sizeof(GF_Vertex), &mesh->vertices[0].color);
		}
#endif
	}

	if (eff->mesh_has_texture && !mesh->mesh_type && !(mesh->flags & MESH_NO_TEXTURE)) {
		glEnableClientState(GL_TEXTURE_COORD_ARRAY );
		has_tx = 1;
#if defined(GPAC_USE_OGL_ES)
		glTexCoordPointer(2, GL_FIXED, sizeof(GF_Vertex), &mesh->vertices[0].texcoords);
#elif defined(GPAC_FIXED_POINT)
		glMatrixMode(GL_TEXTURE);
		glPushMatrix();
		glScalef(fix_scale, fix_scale, fix_scale);
		glMatrixMode(GL_MODELVIEW);
		glTexCoordPointer(2, GL_INT, sizeof(GF_Vertex), &mesh->vertices[0].texcoords);
#else
		glTexCoordPointer(2, GL_FLOAT, sizeof(GF_Vertex), &mesh->vertices[0].texcoords);
#endif
	}
	
	if (mesh->mesh_type) {
		glNormal3f(0, 0, 1.0f);
		glDisable(GL_CULL_FACE);
		glDisable(GL_LIGHTING);
		if (mesh->mesh_type==2) glDisable(GL_LINE_SMOOTH);
		else glDisable(GL_POINT_SMOOTH);
		glLineWidth(1.0f);
	} else {
		has_norm = 1;
		glEnableClientState(GL_NORMAL_ARRAY );
#if defined(GPAC_USE_OGL_ES)
		glNormalPointer(GL_FIXED, sizeof(GF_Vertex), &mesh->vertices[0].normal);
#elif defined(GPAC_FIXED_POINT)
		glNormalPointer(GL_INT, sizeof(GF_Vertex), &mesh->vertices[0].normal);
#else
		glNormalPointer(GL_FLOAT, sizeof(GF_Vertex), &mesh->vertices[0].normal);
#endif

		if (!mesh->mesh_type) {
			if (eff->surface->render->backcull 
				&& (!eff->mesh_is_transparent || (eff->surface->render->backcull ==GF_BACK_CULL_ALPHA) )
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

	/*if inside or no aabb for the mesh draw vertex array*/
	if ((eff->cull_flag==CULL_INSIDE) || !mesh->aabb_root || !mesh->aabb_root->pos) {
#ifdef GPAC_USE_OGL_ES
		glDrawElements(prim_type, mesh->i_count, GL_UNSIGNED_SHORT, mesh->indices);
#else
		glDrawElements(prim_type, mesh->i_count, GL_UNSIGNED_INT, mesh->indices);
#endif
	} else {
		/*otherwise cull aabb against frustum - after some testing it appears (as usual) that there must 
		be a compromise: we're slowing down the renderer here, however the gain is really appreciable for 
		large meshes, especially terrains/elevation grids*/

		/*first get transformed frustum in local space*/
		GF_Matrix mx;
		u32 i, p_idx[6];
		GF_Plane fplanes[6];
		gf_mx_copy(mx, eff->model_matrix);
		gf_mx_inverse(&mx);
		for (i=0; i<6; i++) {
			fplanes[i] = eff->camera->planes[i];
			gf_mx_apply_plane(&mx, &fplanes[i]);
			p_idx[i] = gf_plane_get_p_vertex_idx(&fplanes[i]);
		}
		/*then recursively cull & render AABB tree*/
		VS3D_DrawAABBNode(eff, mesh, prim_type, fplanes, p_idx, mesh->aabb_root->pos);
		VS3D_DrawAABBNode(eff, mesh, prim_type, fplanes, p_idx, mesh->aabb_root->neg);
	}

	glDisableClientState(GL_VERTEX_ARRAY);
	if (has_col) {
		glDisableClientState(GL_COLOR_ARRAY);
		glDisable(GL_COLOR_MATERIAL);
	}
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
	
	if (eff->mesh_is_transparent) glDisable(GL_BLEND);
	eff->mesh_is_transparent = 0;
}

#ifdef GPAC_USE_OGL_ES
u32 ogles_push_enable(u32 mask)
{
	u32 attrib = 0;
	if ((mask & GL_LIGHTING) && glIsEnabled(GL_LIGHTING) ) attrib |= GL_LIGHTING;
	if ((mask & GL_BLEND) && glIsEnabled(GL_BLEND) ) attrib |= GL_BLEND;
	if ((mask & GL_COLOR_MATERIAL) && glIsEnabled(GL_COLOR_MATERIAL) ) attrib |= GL_COLOR_MATERIAL;
	if ((mask & GL_TEXTURE_2D) && glIsEnabled(GL_TEXTURE_2D) ) attrib |= GL_TEXTURE_2D;
	return attrib;
}
void ogles_pop_enable(u32 mask)
{
	u32 atts = 0;
	if (mask & GL_LIGHTING) glEnable(GL_LIGHTING);
	if (mask & GL_BLEND) glEnable(GL_BLEND);
	if (mask & GL_COLOR_MATERIAL) glEnable(GL_COLOR_MATERIAL);
	if (mask & GL_TEXTURE_2D) glEnable(GL_TEXTURE_2D);
}
#endif

/*note we don't perform any culling for normal drawing...*/
void VS3D_DrawNormals(RenderEffect3D *eff, GF_Mesh *mesh)
{
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
	glColor3f(0, 0, 0);
#endif

	if (eff->surface->render->draw_normals==GF_NORMALS_VERTEX) {
		IDX_TYPE *idx = mesh->indices;
		for (i=0; i<mesh->i_count; i+=3) {
			for (j=0; j<3; j++) {
				pt = mesh->vertices[idx[j]].pos;
				end = gf_vec_scale(mesh->vertices[idx[j]].normal, scale);
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
			end = gf_vec_scale(mesh->vertices[idx[0]].normal, scale);
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
}


void VS3D_DrawAABBNodeBounds(RenderEffect3D *eff, AABBNode *node)
{
	if (node->pos) {
		VS3D_DrawAABBNodeBounds(eff, node->pos);
		VS3D_DrawAABBNodeBounds(eff, node->neg);
	} else {
		SFVec3f c, s;
		gf_vec_diff(s, node->max, node->min);
		c = gf_vec_scale(s, FIX_ONE/2);
		gf_vec_add(c, node->min, c);

		glPushMatrix();
		glTranslatef(FIX2FLT(c.x), FIX2FLT(c.y), FIX2FLT(c.z));
		glScalef(FIX2FLT(s.x), FIX2FLT(s.y), FIX2FLT(s.z));
		VS3D_DrawMeshIntern(eff, eff->surface->render->unit_bbox);
		glPopMatrix();
	}
}

void VS3D_DrawMeshBoundingVolume(RenderEffect3D *eff, GF_Mesh *mesh)
{
	SFVec3f c, s;
#ifdef GPAC_USE_OGL_ES
	u32 atts = ogles_push_enable(GL_LIGHTING);
#else
	glPushAttrib(GL_ENABLE_BIT);
#endif

	if (mesh->aabb_root && (eff->surface->render->compositor->draw_bvol==GF_BOUNDS_AABB)) {
		glDisable(GL_LIGHTING);
		VS3D_DrawAABBNodeBounds(eff, mesh->aabb_root);
	} else {
		gf_vec_diff(s, mesh->bounds.max_edge, mesh->bounds.min_edge);
		c.x = mesh->bounds.min_edge.x + s.x/2;
		c.y = mesh->bounds.min_edge.y + s.y/2;
		c.z = mesh->bounds.min_edge.z + s.z/2;

		glPushMatrix();
		
#ifdef GPAC_USE_OGL_ES
		glTranslatex(c.x, c.y, c.z);
		glScalex(s.x, s.y, s.z);
#else
		glTranslatef(FIX2FLT(c.x), FIX2FLT(c.y), FIX2FLT(c.z));
		glScalef(FIX2FLT(s.x), FIX2FLT(s.y), FIX2FLT(s.z));
#endif
		VS3D_DrawMeshIntern(eff, eff->surface->render->unit_bbox);
		glPopMatrix();
	}

#ifdef GPAC_USE_OGL_ES
	ogles_pop_enable(atts);
#else
	glPopAttrib();
#endif
}

void VS3D_DrawMesh(RenderEffect3D *eff, GF_Mesh *mesh)
{
	Bool mesh_drawn = 0;
	if (eff->surface->render->wiremode != GF_WIREFRAME_ONLY) {
		VS3D_DrawMeshIntern(eff, mesh);
		mesh_drawn = 1;
	}

	if (eff->surface->render->draw_normals) VS3D_DrawNormals(eff, mesh);
	if (!mesh->mesh_type && (eff->surface->render->wiremode != GF_WIREFRAME_NONE)) {
		glDisable(GL_LIGHTING);
		if (mesh_drawn) glColor4f(0, 0, 0, 1.0f);
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
	if (eff->surface->render->compositor->draw_bvol) VS3D_DrawMeshBoundingVolume(eff, mesh);
}

#ifndef GPAC_USE_OGL_ES

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

void VS3D_HatchMesh(RenderEffect3D *eff, GF_Mesh *mesh, u32 hatchStyle, SFColor hatchColor)
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
			if (!eff->mesh_is_transparent && (mesh->flags & MESH_IS_SOLID)) {
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
void VS3D_StrikeMesh(RenderEffect3D *eff, GF_Mesh *mesh, Fixed width, u32 dash_style)
{
#ifndef GPAC_USE_OGL_ES
	u16 style;
#endif
	
	if (mesh->mesh_type != MESH_LINESET) return;

	width/=2;
	glLineWidth( FIX2FLT(width));
#ifndef GPAC_USE_OGL_ES
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
		VS3D_DrawMesh(eff, mesh);
		glDisable (GL_LINE_STIPPLE);
	} else 
#endif
		VS3D_DrawMesh(eff, mesh);
}

void VS3D_SetMaterial2D(VisualSurface *surf, SFColor col, Fixed alpha)
{
	glDisable(GL_LIGHTING);
	if (alpha != FIX_ONE) {
		glEnable(GL_BLEND);
		VS3D_SetAntiAlias(surf, 0);
	} else {
		glDisable(GL_BLEND);
		VS3D_SetAntiAlias(surf, surf->render->compositor->antiAlias ? 1 : 0);
	}
#ifdef GPAC_USE_OGL_ES
	glColor4x(col.red, col.green, col.blue, alpha);
#else
	glColor4f(FIX2FLT(col.red), FIX2FLT(col.green), FIX2FLT(col.blue), FIX2FLT(alpha));
#endif
}

void VS3D_SetAntiAlias(VisualSurface *surf, Bool bOn)
{
	if (bOn) {
		glEnable(GL_LINE_SMOOTH);
		glEnable(GL_POINT_SMOOTH);
#ifndef GPAC_USE_OGL_ES
		if (surf->render->poly_aa)
			glEnable(GL_POLYGON_SMOOTH);
		else
			glDisable(GL_POLYGON_SMOOTH);
#endif
	} else {
		glDisable(GL_LINE_SMOOTH);
		glDisable(GL_POINT_SMOOTH);
#ifndef GPAC_USE_OGL_ES
		glDisable(GL_POLYGON_SMOOTH);
#endif
	}
}

void VS3D_ClearSurface(VisualSurface *surf, SFColor color, Fixed alpha)
{
	glClearColor(FIX2FLT(color.red), FIX2FLT(color.green), FIX2FLT(color.blue), FIX2FLT(alpha));
	glClear(GL_COLOR_BUFFER_BIT);
}

#ifndef GPAC_USE_OGL_ES
void VS3D_DrawImage(VisualSurface *surf, Fixed pos_x, Fixed pos_y, u32 width, u32 height, u32 pixelformat, char *data, Fixed scale_x, Fixed scale_y)
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
		if (!surf->render->hw_caps.bgra_texture) return;
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


void VS3D_GetMatrix(VisualSurface *surf, u32 mat_type, Fixed *mat)
{
#ifdef GPAC_FIXED_POINT
	u32 i = 0;
#endif
	Float _mat[16];
	switch (mat_type) {
	case MAT_MODELVIEW:
		glGetFloatv(GL_MODELVIEW_MATRIX, _mat);
		break;
	case MAT_PROJECTION:
		glGetFloatv(GL_PROJECTION_MATRIX, _mat);
		break;
	case MAT_TEXTURE:
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

void VS3D_SetMatrixMode(VisualSurface *surf, u32 mat_type)
{
	switch (mat_type) {
	case MAT_MODELVIEW:
		glMatrixMode(GL_MODELVIEW);
		break;
	case MAT_PROJECTION:
		glMatrixMode(GL_PROJECTION);
		break;
	case MAT_TEXTURE:
		glMatrixMode(GL_TEXTURE);
		break;
	}
}

void VS3D_ResetMatrix(VisualSurface *surf)
{
	glLoadIdentity();
}
void VS3D_PushMatrix(VisualSurface *surf)
{
	glPushMatrix();
}
void VS3D_MultMatrix(VisualSurface *surf, Fixed *mat)
{
#ifdef GPAC_FIXED_POINT
	u32 i;
	Float _mat[16];
	for (i=0; i<16; i++) _mat[i] = FIX2FLT(mat[i]);
	glMultMatrixf(_mat);
#else
	glMultMatrixf(mat);
#endif
}

void VS3D_PopMatrix(VisualSurface *surf)
{
	glPopMatrix();
}

void VS3D_LoadMatrix(VisualSurface *surf, Fixed *mat)
{
#ifdef GPAC_FIXED_POINT
	Float _mat[16];
	u32 i;
	for (i=0; i<16; i++) _mat[i] = FIX2FLT(mat[i]);
	glLoadMatrixf(_mat);
#else
	glLoadMatrixf(mat);
#endif
}


void VS3D_SetClipper2D(VisualSurface *surf, GF_Rect clip)
{
#ifdef GPAC_USE_OGL_ES

	Fixed g[4];
	u32 cp;
	VS3D_ResetClipper2D(surf);
	if (surf->num_clips + 4 > surf->max_clips)  return;
	cp = surf->num_clips;
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
	surf->num_clips += 4;
#else
	Double g[4];
	u32 cp;
	VS3D_ResetClipper2D(surf);
	if (surf->num_clips + 4 > surf->max_clips) return;
	cp = surf->num_clips;
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
	surf->num_clips += 4;
#endif
}

void VS3D_ResetClipper2D(VisualSurface *surf)
{
	u32 cp;
	if (surf->num_clips < 4) return;
	cp = surf->num_clips - 4;
	glDisable(GL_CLIP_PLANE0 + cp + 3);
	glDisable(GL_CLIP_PLANE0 + cp + 2);
	glDisable(GL_CLIP_PLANE0 + cp + 1);
	glDisable(GL_CLIP_PLANE0 + cp);
	surf->num_clips -= 4;
}

void VS3D_SetClipPlane(VisualSurface *surf, GF_Plane p)
{
#ifdef GPAC_USE_OGL_ES
	Fixed g[4];
	if (surf->num_clips + 1 > surf->max_clips) return;
	gf_vec_norm(&p.normal);
	g[0] = p.normal.x;
	g[1] = p.normal.y;
	g[2] = p.normal.z;
	g[3] = p.d;
	glClipPlanex(GL_CLIP_PLANE0 + surf->num_clips, g); 
#else
	Double g[4];
	if (surf->num_clips + 1 > surf->max_clips) return;
	gf_vec_norm(&p.normal);
	g[0] = FIX2FLT(p.normal.x);
	g[1] = FIX2FLT(p.normal.y);
	g[2] = FIX2FLT(p.normal.z);
	g[3] = FIX2FLT(p.d);
	glClipPlane(GL_CLIP_PLANE0 + surf->num_clips, g); 
#endif
	glEnable(GL_CLIP_PLANE0 + surf->num_clips);
	surf->num_clips++;
}

void VS3D_ResetClipPlane(VisualSurface *surf)
{
	if (!surf->num_clips) return;
	glDisable(GL_CLIP_PLANE0 + surf->num_clips-1);
	surf->num_clips -= 1;
}

void VS3D_SetMaterial(VisualSurface *surf, u32 material_type, Fixed *rgba)
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
	case MATERIAL_AMBIENT: mode = GL_AMBIENT; break;
	case MATERIAL_DIFFUSE: mode = GL_DIFFUSE; break;
	case MATERIAL_SPECULAR: mode = GL_SPECULAR; break;
	case MATERIAL_EMISSIVE: mode = GL_EMISSION; break;

	case MATERIAL_NONE:
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

void VS3D_SetShininess(VisualSurface *surf, Fixed shininess)
{
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, FIX2FLT(shininess) * 128);
}

void VS3D_SetState(VisualSurface *surf, u32 flag_mask, Bool setOn)
{
	if (setOn) {
		if (flag_mask & F3D_LIGHT) glEnable(GL_LIGHTING);
		if (flag_mask & F3D_BLEND) glEnable(GL_BLEND);
		if (flag_mask & F3D_COLOR) glEnable(GL_COLOR_MATERIAL);
	} else {
		if (flag_mask & F3D_LIGHT) 
			glDisable(GL_LIGHTING);
		if (flag_mask & F3D_BLEND) glDisable(GL_BLEND);
#ifdef GPAC_USE_OGL_ES
		if (flag_mask & F3D_COLOR) glDisable(GL_COLOR_MATERIAL);
#else
		if (flag_mask & F3D_COLOR) glDisable(GL_COLOR_MATERIAL | GL_COLOR_MATERIAL_FACE);
#endif
	}
}

Bool VS3D_AddSpotLight(VisualSurface *surf, Fixed _ambientIntensity, SFVec3f attenuation, Fixed _beamWidth, 
					   SFColor color, Fixed _cutOffAngle, SFVec3f direction, Fixed _intensity, SFVec3f location)
{
	Float vals[4], intensity, cutOffAngle, beamWidth, ambientIntensity, exp;
	GLint iLight;

	if (!surf->num_lights) glEnable(GL_LIGHTING);
	if (surf->num_lights==surf->max_lights) return 0;
	iLight = GL_LIGHT0 + surf->num_lights;
	surf->num_lights++;
	glEnable(iLight);

	ambientIntensity = FIX2FLT(_ambientIntensity);
	intensity = FIX2FLT(_intensity);
	cutOffAngle = FIX2FLT(_cutOffAngle);
	beamWidth = FIX2FLT(_beamWidth);
	
	/*in case...*/
	gf_vec_norm(&direction);
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
	return 1;
}

/*insert pointlight - returns 0 if too many lights*/
Bool VS3D_AddPointLight(VisualSurface *surf, Fixed _ambientIntensity, SFVec3f attenuation, SFColor color, Fixed _intensity, SFVec3f location)
{
	Float vals[4], ambientIntensity, intensity;
	u32 iLight;

	if (!surf->num_lights) glEnable(GL_LIGHTING);
	if (surf->num_lights==surf->max_lights) return 0;
	iLight = GL_LIGHT0 + surf->num_lights;
	surf->num_lights++;
	glEnable(iLight);

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
	return 1;
}

Bool VS3D_AddDirectionalLight(VisualSurface *surf, Fixed _ambientIntensity, SFColor color, Fixed _intensity, SFVec3f direction)
{
	Float vals[4], ambientIntensity, intensity;
	u32 iLight;
	if (!surf->num_lights) glEnable(GL_LIGHTING);
	if (surf->num_lights==surf->max_lights) return 0;
	iLight = GL_LIGHT0 + surf->num_lights;
	surf->num_lights++;
	glEnable(iLight);

	ambientIntensity = FIX2FLT(_ambientIntensity);
	intensity = FIX2FLT(_intensity);

	/*in case...*/
	gf_vec_norm(&direction);
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
	return 1;
}

void VS3D_RemoveLastLight(VisualSurface *surf)
{
	if (surf->num_lights) {
		glDisable(GL_LIGHT0+surf->num_lights-1);
		surf->num_lights--;
	}
}

void VS3D_ClearAllLights(VisualSurface *surf)
{
	u32 i;
	for (i=surf->num_lights; i>0; i--) {
		glDisable(GL_LIGHT0+i-1);
	}
	surf->num_lights = 0;
	//glDisable(GL_LIGHTING);
}

void VS3D_SetFog(VisualSurface *surf, const char *type, SFColor color, Fixed density, Fixed visibility)
{
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
	glHint(GL_FOG_HINT, surf->render->compositor->high_speed ? GL_FASTEST : GL_NICEST);
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
	glHint(GL_FOG_HINT, surf->render->compositor->high_speed ? GL_FASTEST : GL_NICEST);
#endif

}

void VS3D_FillRect(VisualSurface *surf, GF_Rect rc, SFColorRGBA color)
{
	glDisable(GL_BLEND | GL_LIGHTING | GL_TEXTURE_2D);
	glNormal3f(0, 0, 1);

#ifdef GPAC_USE_OGL_ES
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
	if (color.alpha!=FIX_ONE) {
		glEnable(GL_BLEND);
		glColor4f(FIX2FLT(color.red), FIX2FLT(color.green), FIX2FLT(color.blue), FIX2FLT(color.alpha));
	} else {
		glColor3f(FIX2FLT(color.red), FIX2FLT(color.green), FIX2FLT(color.blue));
	}
	glBegin(GL_QUADS);
	glVertex3f(FIX2FLT(rc.x), FIX2FLT(rc.y), 0);
	glVertex3f(FIX2FLT(rc.x+rc.width), FIX2FLT(rc.y), 0);
	glVertex3f(FIX2FLT(rc.x+rc.width), FIX2FLT(rc.y-rc.height), 0);
	glVertex3f(FIX2FLT(rc.x), FIX2FLT(rc.y-rc.height), 0);
	glEnd();
#endif

	glDisable(GL_BLEND);
}

GF_Err R3D_GetScreenBuffer(GF_VisualRenderer *vr, GF_VideoSurface *fb)
{
	u32 i, hy;
	char *tmp;
	Render3D *sr = (Render3D *)vr->user_priv;

	fb->video_buffer = malloc(sizeof(char)*3*sr->out_width * sr->out_height);
	fb->width = sr->out_width;
	fb->pitch = 3*sr->out_width;
	fb->height = sr->out_height;
	fb->pixel_format = GF_PIXEL_RGB_24;

	/*don't understand why I get BGR when using GL_RGB*/
	glReadPixels(sr->out_x, sr->out_y, sr->out_width, sr->out_height, GL_BGR_EXT, GL_UNSIGNED_BYTE, fb->video_buffer);

	/*flip image (openGL always handle image data bottom to top) */
	tmp = malloc(sizeof(char)*fb->pitch);
	hy = fb->height/2;
	for (i=0; i<hy; i++) {
		memcpy(tmp, fb->video_buffer+ i*fb->pitch, fb->pitch);
		memcpy(fb->video_buffer + i*fb->pitch, fb->video_buffer + (fb->height - 1 - i) * fb->pitch, fb->pitch);
		memcpy(fb->video_buffer + (fb->height - 1 - i) * fb->pitch, tmp, fb->pitch);
	}
	free(tmp);
	return GF_OK;
}

GF_Err R3D_ReleaseScreenBuffer(GF_VisualRenderer *vr, GF_VideoSurface *framebuffer)
{
	free(framebuffer->video_buffer);
	framebuffer->video_buffer = 0;
	return GF_OK;
}

