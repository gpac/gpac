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


#ifndef OFFSCREEN_CACHE_H
#define OFFSCREEN_CACHE_H

#include "render.h"

#ifdef GPAC_RENDER_USE_CACHE

#include "drawable.h"


typedef struct _drawable_cache
{
	/*gpac texture object*/
	GF_TextureHandler txh;
	/*bitmap path for the cache*/
	GF_Path *path;
	/*the priority to determine the overlapping*/
	u8 priority;
} DrawableCache;

void drawable_cache_del(DrawableCache *bitmap);

DrawableCache *drawable_cache_initialize(DrawableContext *ctx, GF_Renderer *compositor, GF_Rect *local_bounds);

void drawable_cache_update(DrawableContext *ctx, GF_VisualManager *surface, GF_Rect *local_bounds) ;

void drawable_cache_draw(GF_TraverseState *eff, GF_Path *cache_path, GF_TextureHandler *cache_txh);


typedef struct _group_cache
{
	/*gpac texture object*/
	GF_TextureHandler txh;

	/*the priority to determine the overlapping*/
	u8 priority;
	/*drawable representing the cached group*/
	Drawable *drawable;

	Fixed opacity;
} GroupCache;


GroupCache *group_cache_new(GF_Node *node);
void group_cache_del(GroupCache *cache);

#endif	/*GPAC_RENDER_USE_CACHE*/

#endif	/*OFFSCREEN_CACHE_H*/

