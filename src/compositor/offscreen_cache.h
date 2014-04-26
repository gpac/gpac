/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2006-2012
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


#ifndef OFFSCREEN_CACHE_H
#define OFFSCREEN_CACHE_H

#include "drawable.h"

typedef struct _group_cache
{
	/*gpac texture object*/
	GF_TextureHandler txh;
	/*drawable representing the cached group*/
	Drawable *drawable;

	Fixed opacity;
	Bool force_recompute;
	/*user scale (zoom and AR) of the group*/
	Fixed scale;
	SFVec2f orig_vp;

} GroupCache;

GroupCache *group_cache_new(GF_Compositor *compositor, GF_Node *node);
void group_cache_del(GroupCache *cache);

/*returns 1 if cache is being recomputed due to dirty subtree*/
Bool group_cache_traverse(GF_Node *node, GroupCache *cache, GF_TraverseState *tr_state, Bool force_recompute, Bool is_mpeg4, Bool auto_fit_vp);

void group_cache_draw(GroupCache *cache, GF_TraverseState *tr_state);
Fixed group_cache_check_coverage_increase(GF_Rect *ctx, GF_Rect *grp_bounds, DrawableContext *curr, DrawableContext* first_child);



#endif	/*OFFSCREEN_CACHE_H*/

