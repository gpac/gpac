/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
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



#include "nodes_stacks.h"
#include "visual_manager.h"

#if !defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_COMPOSITOR)

typedef struct
{
	GF_SoundInterface snd_ifce;
	SFVec3f pos;
} Sound2DStack;

/*sound2D wraper - spacialization is not supported yet*/
static void TraverseSound2D(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_TraverseState *tr_state = (GF_TraverseState*) rs;
	M_Sound2D *snd = (M_Sound2D *)node;
	Sound2DStack *st = (Sound2DStack *)gf_node_get_private(node);

	if (is_destroy) {
		gf_free(st);
		return;
	}
	if (!snd->source) return;

	/*this implies no DEF/USE for real location...*/
	st->pos.x = snd->location.x;
	st->pos.y = snd->location.y;
	st->pos.z = 0;
#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d)
		gf_mx_apply_vec(&tr_state->model_matrix, &st->pos);
	else
#endif
		gf_mx2d_apply_coords(&tr_state->transform, &st->pos.x, &st->pos.y);


	tr_state->sound_holder = &st->snd_ifce;
	gf_node_traverse((GF_Node *) snd->source, tr_state);
	tr_state->sound_holder = NULL;
	/*never cull Sound2d*/
	tr_state->disable_cull = 1;
}
static Bool SND2D_GetChannelVolume(GF_Node *node, Fixed *vol)
{
	u32 i;
	Fixed volume = ((M_Sound2D *)node)->intensity;
	for (i=0; i<GF_AUDIO_MIXER_MAX_CHANNELS; i++) {
		vol[i] = volume;
	}
	return (volume==FIX_ONE) ? 0 : 1;
}


void compositor_init_sound2d(GF_Compositor *compositor, GF_Node *node)
{
	Sound2DStack *snd;
	GF_SAFEALLOC(snd, Sound2DStack);
	if (!snd) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate sound 2d stack\n"));
		return;
	}
	snd->snd_ifce.GetChannelVolume = SND2D_GetChannelVolume;
	snd->snd_ifce.owner = node;
	gf_node_set_private(node, snd);
	gf_node_set_callback_function(node, TraverseSound2D);
}

#ifndef GPAC_DISABLE_3D

static Fixed snd_compute_gain(Fixed min_b, Fixed min_f, Fixed max_b, Fixed max_f, SFVec3f pos)
{
	Fixed sqpos_x, sqpos_z;
	Fixed y_pos, x_pos, dist_ellip, viewp_dist, dist_from_foci_min, dist_from_foci_max, d_min, d_max, sqb_min, sqb_max;
	Fixed a_in = (min_f+min_b)/2;
	Fixed b_in = gf_sqrt(gf_mulfix(min_b, min_f));
	Fixed alpha_min = (min_f-min_b)/2;
	Fixed dist_foci_min = (min_f-min_b);
	Fixed a_out = (max_f+max_b)/2;	//first ellipse axis
	Fixed b_out = gf_sqrt(gf_mulfix(max_b, max_f));
	Fixed alpha_max = (max_f-max_b)/2; //origo from focus
	Fixed dist_foci_max = (max_f-max_b);
	Fixed x_min = 0;
	Fixed x_max = 0;
	Fixed y_min = 0;
	Fixed y_max = 0;
	Fixed k = (ABS(pos.z) >= FIX_EPSILON) ? gf_divfix(pos.x, pos.z) : 0;

	sqpos_x = gf_mulfix(pos.x, pos.x);
	sqpos_z = gf_mulfix(pos.z, pos.z);

	dist_from_foci_min = gf_sqrt(sqpos_z + sqpos_x) + gf_sqrt( gf_mulfix(pos.z - dist_foci_min, pos.z - dist_foci_min) + sqpos_x);
	dist_from_foci_max = gf_sqrt(sqpos_z + sqpos_x) + gf_sqrt( gf_mulfix(pos.z - dist_foci_max, pos.z - dist_foci_max) + sqpos_x);
	d_min = min_f+min_b;
	d_max = max_f+max_b;
	if(dist_from_foci_max > d_max) return 0;
	else if (dist_from_foci_min <= d_min) return FIX_ONE;

	sqb_min = gf_mulfix(b_in, b_in);
	sqb_max = gf_mulfix(b_out, b_out);

	if (ABS(pos.z) > FIX_ONE/10000) {
		s32 sign = (pos.z>0) ? 1 : -1;
		Fixed a_in_k_sq, a_out_k_sq;
		a_in_k_sq = gf_mulfix(a_in, k);
		a_in_k_sq = gf_mulfix(a_in_k_sq, a_in_k_sq);

		x_min = gf_mulfix(alpha_min, sqb_min) + sign*gf_mulfix( gf_mulfix(a_in, b_in), gf_sqrt(a_in_k_sq + sqb_min - gf_mulfix( gf_mulfix(alpha_min, k), gf_mulfix(alpha_min, k))));
		x_min = gf_divfix(x_min, sqb_min + a_in_k_sq);
		y_min = gf_mulfix(k, x_min);

		a_out_k_sq = gf_mulfix(a_out, k);
		a_out_k_sq = gf_mulfix(a_out_k_sq, a_out_k_sq);

		x_max = gf_mulfix(alpha_max, sqb_max) + sign*gf_mulfix( gf_mulfix(a_out, b_out), gf_sqrt( a_out_k_sq + sqb_max - gf_mulfix( gf_mulfix(alpha_max, k), gf_mulfix(alpha_max, k))));
		x_max = gf_divfix(x_max, sqb_max + a_out_k_sq);
		y_max = gf_mulfix(k, x_max);
	} else {
		x_min = x_max = 0;
		y_min = gf_mulfix(b_in, gf_sqrt(FIX_ONE - gf_mulfix( gf_divfix(alpha_min,a_in), gf_divfix(alpha_min,a_in)) ) );
		y_max = gf_mulfix(b_out, gf_sqrt(FIX_ONE - gf_mulfix( gf_divfix(alpha_max,a_out), gf_divfix(alpha_max,a_out)) ) );
	}

	y_pos = gf_sqrt(sqpos_x) - y_min;
	x_pos = pos.z - x_min;
	x_max -= x_min;
	y_max -= y_min;
	dist_ellip = gf_sqrt( gf_mulfix(y_max, y_max) + gf_mulfix(x_max, x_max));
	viewp_dist = gf_sqrt( gf_mulfix(y_pos, y_pos) + gf_mulfix(x_pos, x_pos));
	viewp_dist = gf_divfix(viewp_dist, dist_ellip);

	return FLT2FIX ( (Float) pow(10.0,- FIX2FLT(viewp_dist)));
}


typedef struct
{
	GF_SoundInterface snd_ifce;
	GF_Matrix mx;
	SFVec3f last_pos;
	Bool identity;
	/*local system*/
	Fixed intensity;
	Fixed lgain, rgain;
} SoundStack;

static void TraverseSound(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_TraverseState *tr_state = (GF_TraverseState*) rs;
	M_Sound *snd = (M_Sound *)node;
	SoundStack *st = (SoundStack *)gf_node_get_private(node);

	if (is_destroy) {
		gf_free(st);
		return;
	}
	if (!snd->source) return;

	tr_state->sound_holder = &st->snd_ifce;

	/*forward in case we're switched off*/
	if (tr_state->switched_off) {
		gf_node_traverse((GF_Node *) snd->source, tr_state);
	}
	else if (tr_state->traversing_mode==TRAVERSE_GET_BOUNDS) {
		/*we can't cull sound since*/
		tr_state->disable_cull = 1;
	} else if (tr_state->traversing_mode==TRAVERSE_SORT) {
		GF_Matrix mx;
		SFVec3f usr, snd_dir, pos;
		Fixed mag, ang;
		/*this implies no DEF/USE for real location...*/
		gf_mx_copy(st->mx, tr_state->model_matrix);
		gf_mx_copy(mx, tr_state->model_matrix);
		gf_mx_inverse(&mx);

		snd_dir = snd->direction;
		gf_vec_norm(&snd_dir);

		/*get user location*/
		usr = tr_state->camera->position;
		gf_mx_apply_vec(&mx, &usr);

		/*recenter to ellipse focal*/
		gf_vec_diff(usr, usr, snd->location);
		mag = gf_vec_len(usr);
		if (!mag) mag = FIX_ONE/10;
		ang = gf_divfix(gf_vec_dot(snd_dir, usr), mag);

		usr.z = gf_mulfix(ang, mag);
		usr.x = gf_sqrt(gf_mulfix(mag, mag) - gf_mulfix(usr.z, usr.z));
		usr.y = 0;
		if (!gf_vec_equal(usr, st->last_pos)) {
			st->intensity = snd_compute_gain(snd->minBack, snd->minFront, snd->maxBack, snd->maxFront, usr);
			st->intensity = gf_mulfix(st->intensity, snd->intensity);
			st->last_pos = usr;
		}
		st->identity = (st->intensity==FIX_ONE) ? 1 : 0;

		if (snd->spatialize) {
			Fixed sign;
			SFVec3f cross;
			pos = snd->location;
			gf_mx_apply_vec(&tr_state->model_matrix, &pos);
			gf_vec_diff(pos, pos, tr_state->camera->position);
			gf_vec_diff(usr, tr_state->camera->target, tr_state->camera->position);
			gf_vec_norm(&pos);
			gf_vec_norm(&usr);

			ang = gf_acos(gf_vec_dot(usr, pos));
			/*get orientation*/
			cross = gf_vec_cross(usr, pos);
			sign = gf_vec_dot(cross, tr_state->camera->up);
			if (sign>0) ang *= -1;
			ang = (FIX_ONE + gf_sin(ang)) / 2;
			st->lgain = (FIX_ONE - gf_mulfix(ang, ang));
			st->rgain = FIX_ONE - gf_mulfix(FIX_ONE - ang, FIX_ONE - ang);
			/*renorm between 0 and 1*/
			st->lgain = gf_mulfix(st->lgain, 4*st->intensity/3);
			st->rgain = gf_mulfix(st->rgain, 4*st->intensity/3);

			if (st->identity && ((st->lgain!=FIX_ONE) || (st->rgain!=FIX_ONE))) st->identity = 0;
		} else {
			st->lgain = st->rgain = FIX_ONE;
		}
		gf_node_traverse((GF_Node *) snd->source, tr_state);
	}

	tr_state->sound_holder = NULL;
}
static Bool SND_GetChannelVolume(GF_Node *node, Fixed *vol)
{
	u32 i;
	M_Sound *snd = (M_Sound *)node;
	SoundStack *st = (SoundStack *)gf_node_get_private(node);
	for (i=2; i<GF_AUDIO_MIXER_MAX_CHANNELS; i++)
		vol[i] = st->intensity;

	if (snd->spatialize) {
		vol[0] = st->lgain;
		vol[1] = st->rgain;
	} else {
		vol[0] = vol[1] = st->intensity;
	}
	return !st->identity;
}

void compositor_init_sound(GF_Compositor *compositor, GF_Node *node)
{
	SoundStack *snd;
	GF_SAFEALLOC(snd, SoundStack);
	if (!snd) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate sound stack\n"));
		return;
	}
	snd->snd_ifce.GetChannelVolume = SND_GetChannelVolume;
	snd->snd_ifce.owner = node;
	gf_node_set_private(node, snd);
	gf_node_set_callback_function(node, TraverseSound);
}

#endif /*GPAC_DISABLE_3D*/

#endif //!defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_COMPOSITOR)
