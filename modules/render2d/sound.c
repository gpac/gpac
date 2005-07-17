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


typedef struct
{
	GF_SoundInterface snd_ifce;
	SFVec2f pos;
} Sound2DStack;

static void DestroySound2D(GF_Node *node)
{
	Sound2DStack *st = (Sound2DStack *)gf_node_get_private(node);
	free(st);
}

/*sound2D wraper - spacialization is not supported yet*/
static void RenderSound2D(GF_Node *node, void *rs)
{
	RenderEffect2D *eff = (RenderEffect2D*) rs;
	M_Sound2D *snd = (M_Sound2D *)node;
	Sound2DStack *st = (Sound2DStack *)gf_node_get_private(node);

	if (!snd->source) return;

	/*this implies no DEF/USE for real location...*/
	st->pos = snd->location;
	gf_mx2d_apply_point(&eff->transform, &st->pos);

	eff->sound_holder = &st->snd_ifce;
	gf_node_render((GF_Node *) snd->source, eff);
	eff->sound_holder = NULL;
}
static Bool SND2D_GetChannelVolume(GF_Node *node, Fixed *vol)
{
	vol[0] = vol[1] = vol[2] = vol[3] = vol[4] = vol[5] = ((M_Sound2D *)node)->intensity;
	return (vol[0]==FIX_ONE) ? 0 : 1;
}
static u8 SND2D_GetPriority(GF_Node *node)
{
	return 255;
}

void R2D_InitSound2D(Render2D *sr, GF_Node *node)
{
	Sound2DStack *snd;
	GF_SAFEALLOC(snd, sizeof(Sound2DStack));
	snd->snd_ifce.GetChannelVolume = SND2D_GetChannelVolume;
	snd->snd_ifce.GetPriority = SND2D_GetPriority;
	snd->snd_ifce.owner = node;
	gf_node_set_private(node, snd);
	gf_node_set_render_function(node, RenderSound2D);
	gf_node_set_predestroy_function(node, DestroySound2D);
}
