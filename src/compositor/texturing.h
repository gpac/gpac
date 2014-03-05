/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2000-2012
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

#ifndef _TEXTURING_H_
#define _TEXTURING_H_

#include <gpac/internal/compositor_dev.h>
#ifdef __cplusplus
extern "C" {
#endif
/*allocates the HW specific texture handle(s) (potentially both raster and OpenGL handlers)*/
GF_Err gf_sc_texture_allocate(GF_TextureHandler *txh);
/*releases the hardware handle(s) and all associated system resources*/
void gf_sc_texture_release(GF_TextureHandler *txh);
/*signals new data is available in the texture interface*/
GF_Err gf_sc_texture_set_data(GF_TextureHandler *hdl);
/*reset the hardware handle(s) only, but keep associated resources for later hardware bind*/
void gf_sc_texture_reset(GF_TextureHandler *hdl);

/*push data to hardware if needed, creating the hardware handle(s)*/
Bool gf_sc_texture_push_image(GF_TextureHandler *txh, Bool generate_mipmaps, Bool for2d);

/*refreshes hardware data for given rect (eg glTexSubImage)*/
void gf_sc_texture_refresh_area(GF_TextureHandler *, GF_IRect *rc, void *mem);
/*gets texture transform matrix - returns 1 if not identity
@tx_transform: texture transform node from appearance*/
Bool gf_sc_texture_get_transform(GF_TextureHandler *txh, GF_Node *tx_transform, GF_Matrix *mx, Bool for_picking);

/*gets the associated raster2D stencil handler*/
GF_STENCIL gf_sc_texture_get_stencil(GF_TextureHandler *hdl);
/*sets the associated raster2D stencil handler (used by gradients)*/
void gf_sc_texture_set_stencil(GF_TextureHandler *hdl, GF_STENCIL stencil);

Bool gf_sc_texture_is_transparent(GF_TextureHandler *txh);

void gf_sc_texture_check_pause_on_first_load(GF_TextureHandler *txh);

/*ALL THE FOLLOWING ARE ONLY AVAILABLE IN 3D AND DEAL WITH OPENGL TEXTURE MANAGEMENT*/
#ifndef GPAC_DISABLE_3D

/*enable the texture and pushes the given texture transform on the graphics card*/
u32 gf_sc_texture_enable(GF_TextureHandler *txh, GF_Node *tx_transform);

/*same as gf_sc_texture_enable, but provides object bounds for correct mapping of svg gradients in UserSpaceMode*/
u32 gf_sc_texture_enable_ex(GF_TextureHandler *txh, GF_Node *tx_transform, GF_Rect *bounds);

/*disables the texture (unbinds it)*/
void gf_sc_texture_disable(GF_TextureHandler *txh);
/*retrieves the internal (potentially converted for YUV) data buffer*/
char *gf_sc_texture_get_data(GF_TextureHandler *txh, u32 *pix_format);
/*checks if the data buffer shall be pushed to graphics card*/
Bool gf_sc_texture_needs_reload(GF_TextureHandler *hdl);

#ifndef GPAC_USE_TINYGL
/*copy current GL window to the texture - the viewport used is the texture one (0,0,W,H) */
void gf_sc_copy_to_texture(GF_TextureHandler *txh);
#endif

Bool gf_sc_texture_convert(GF_TextureHandler *txh);

/*copy current GL window to the associated data buffer - the viewport used is the texture one (0,0,W,H) */
void gf_sc_copy_to_stencil(GF_TextureHandler *txh);

/*set blending mode*/
enum
{
	TX_DECAL = 0,
	TX_MODULATE,
	TX_REPLACE,
	TX_BLEND,
};
/*set texturing blend mode*/
void gf_sc_texture_set_blend_mode(GF_TextureHandler *txh, u32 mode);

#endif	/*GPAC_DISABLE_3D*/
#ifdef __cplusplus
}
#endif
#endif	/*_TEXTURING_H_*/

