/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / software 2D rasterizer module
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
 *		
 */

#include "rast_soft.h"

/*we don't need any private context*/
GF_Raster2D *EVG_LoadRenderer()
{
	GF_Raster2D *dr;
	GF_SAFEALLOC(dr, GF_Raster2D);
	GF_REGISTER_MODULE_INTERFACE(dr, GF_RASTER_2D_INTERFACE, "GPAC 2D Raster", "gpac distribution")


	dr->stencil_new = evg_stencil_new;
	dr->stencil_delete = evg_stencil_delete;
	dr->stencil_set_matrix = evg_stencil_set_matrix;
	dr->stencil_set_brush_color = evg_stencil_set_brush_color;
	dr->stencil_set_gradient_mode = evg_stencil_set_gradient_mode;
	dr->stencil_set_linear_gradient = evg_stencil_set_linear_gradient;
	dr->stencil_set_radial_gradient = evg_stencil_set_radial_gradient;
	dr->stencil_set_gradient_interpolation = evg_stencil_set_gradient_interpolation;
	dr->stencil_set_alpha = evg_stencil_set_alpha;
	dr->stencil_set_texture = evg_stencil_set_texture;
	dr->stencil_set_tiling = evg_stencil_set_tiling;
	dr->stencil_set_filter = evg_stencil_set_filter;
	dr->stencil_set_color_matrix = evg_stencil_set_color_matrix;
	dr->stencil_create_texture = evg_stencil_create_texture;
	dr->stencil_texture_modified = NULL;

	dr->surface_new = evg_surface_new;
	dr->surface_delete = evg_surface_delete;
	dr->surface_attach_to_device = NULL;
	dr->surface_attach_to_texture = evg_surface_attach_to_texture;
	dr->surface_attach_to_buffer = evg_surface_attach_to_buffer;
	dr->surface_detach = evg_surface_detach;
	dr->surface_set_raster_level = evg_surface_set_raster_level;
	dr->surface_set_matrix = evg_surface_set_matrix;
	dr->surface_set_clipper = evg_surface_set_clipper;
	dr->surface_set_path = evg_surface_set_path;
	dr->surface_fill = evg_surface_fill;
	dr->surface_flush = NULL;
	dr->surface_clear = evg_surface_clear;
	return dr;
}

void EVG_ShutdownRenderer(GF_Raster2D *dr)
{
	free(dr);
}

#ifndef GPAC_STANDALONE_RENDER_2D

GF_EXPORT
Bool QueryInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_RASTER_2D_INTERFACE) return 1;
	return 0;
}

GF_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType==GF_RASTER_2D_INTERFACE) {
		return (GF_BaseInterface *)EVG_LoadRenderer();
	}
	return NULL;
}

GF_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	if (ifce->InterfaceType == GF_RASTER_2D_INTERFACE) {
		EVG_ShutdownRenderer((GF_Raster2D *)ifce);
	}
}

#endif
