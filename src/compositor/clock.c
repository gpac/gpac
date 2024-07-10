/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
 *					All rights reserved
 *
 *  This file scene part of GPAC / Scene Compositor sub-project
 *
 *  GPAC scene free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  GPAC scene distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <gpac/internal/compositor_dev.h>
#ifndef GPAC_DISABLE_COMPOSITOR

static GF_Clock *gf_clock_new(GF_Compositor *compositor)
{
	GF_Clock *tmp;
	GF_SAFEALLOC(tmp, GF_Clock);
	if (!tmp) return NULL;
	tmp->mx = gf_mx_new("Clock");
	tmp->compositor = compositor;
	tmp->speed = FIX_ONE;
	tmp->timeline_id = 1;
	return tmp;
}

void gf_clock_del(GF_Clock *ck)
{
	gf_mx_del(ck->mx);
	gf_free(ck);
}

GF_Clock *gf_clock_find(GF_List *Clocks, u16 clock_id, u16 ES_ID)
{
	u32 i;
	GF_Clock *tmp;
	i=0;
	while ((tmp = (GF_Clock *)gf_list_enum(Clocks, &i))) {
		//first check the clock ID
		if (tmp->clock_id == clock_id) return tmp;
		//then check the ES ID
		if (ES_ID && (tmp->clock_id == ES_ID)) return tmp;
	}
	//no clocks found...
	return NULL;
}

static GF_Clock *gf_ck_look_for_clock_dep(GF_Scene *scene, u16 clock_id)
{
	u32 i, j;
	GF_ODMExtraPid *xpid;
	GF_ObjectManager *odm;

	/*check in top OD*/
	if (scene->root_od->pid_id == clock_id) return scene->root_od->ck;
	i=0;
	while ((xpid = (GF_ODMExtraPid*)gf_list_enum(scene->root_od->extra_pids, &i))) {
		if (xpid->pid_id == clock_id) return scene->root_od->ck;
	}
	/*check in sub ODs*/
	j=0;
	while ((odm = (GF_ObjectManager*)gf_list_enum(scene->resources, &j))) {
		if (odm->pid_id == clock_id) return odm->ck;
		i=0;
		while ((xpid = (GF_ODMExtraPid*)gf_list_enum(odm->extra_pids, &i))) {
			if (xpid->pid_id == clock_id) return odm->ck;
		}
	}
	return NULL;
}

/*remove clocks created due to out-of-order OCR dependencies*/
static void gf_ck_resolve_clock_dep(GF_List *clocks, GF_Scene *scene, GF_Clock *new_ck, u16 Clock_ESID)
{
	u32 i;
	GF_Clock *clock;
	GF_ObjectManager *odm;

	/*check all objects - if any uses a clock which ID == the clock_ESID then
	this clock shall be removed*/
	if (scene->root_od->ck && (scene->root_od->ck->clock_id == Clock_ESID)) {
		scene->root_od->ck = new_ck;
	}
	i=0;
	while ((odm = (GF_ObjectManager*)gf_list_enum(scene->resources, &i))) {
		if (odm->ck && (odm->ck->clock_id == Clock_ESID)) {
			odm->ck = new_ck;
		}
	}
	/*destroy clock*/
	i=0;
	while ((clock = (GF_Clock*)gf_list_enum(clocks, &i))) {
		if (clock->clock_id == Clock_ESID) {
			gf_list_rem(clocks, i-1);
			gf_clock_del(clock);
			return;
		}
	}
}

GF_Clock *gf_clock_attach(GF_List *clocks, GF_Scene *scene, u16 clock_id, u16 ES_ID, s32 hasOCR)
{
	Bool check_dep;
	GF_Clock *tmp = gf_clock_find(clocks, clock_id, ES_ID);
	/*ck dep can only be solved if in the main service*/
	check_dep = (scene->root_od->scene_ns && scene->root_od->scene_ns->clocks==clocks) ? GF_TRUE : GF_FALSE;

	/*this partly solves a->b->c*/
	if (!tmp && check_dep) tmp = gf_ck_look_for_clock_dep(scene, clock_id);
	if (!tmp) {
		tmp = gf_clock_new(scene->compositor);
		tmp->clock_id = clock_id;
		gf_list_add(clocks, tmp);
	} else {
		if (tmp->clock_id == ES_ID) tmp->clock_id = clock_id;
		/*this finally solves a->b->c*/
		if (check_dep && (tmp->clock_id != ES_ID)) gf_ck_resolve_clock_dep(clocks, scene, tmp, ES_ID);
	}
	return tmp;
}

void gf_clock_reset(GF_Clock *ck)
{
	ck->clock_init = 0;
	ck->audio_delay = 0;
	ck->speed_set_time = 0;
	//do NOT reset buffering flag, because RESET scene called only
	//for the stream owning the clock, and other streams may
	//have signaled buffering on this clock
	ck->init_timestamp = 0;
	ck->start_time = 0;
	ck->has_seen_eos = 0;
	ck->has_media_time_shift = GF_FALSE;
	//do NOT reset media timestamp mapping once the clock is init
	//if discontinuities are found the media time mapping will be adjusted then
	ck->timeline_id++;
}

void gf_clock_set_time(GF_Clock *ck, u64 ref_TS, u32 timescale)
{
	if (!ck->clock_init) {
		u64 real_ts_ms = gf_timestamp_rescale(ref_TS, timescale, 1000);
		ck->init_ts_loops = (u32) (real_ts_ms / 0xFFFFFFFFUL);
		ck->init_timestamp = (u32) (real_ts_ms % 0xFFFFFFFFUL);
		ck->clock_init = 1;
		ck->audio_delay = 0;
		/*update starttime and pausetime even in pause mode*/
		ck->pause_time = ck->start_time = gf_sc_get_clock(ck->compositor);
	}
}



void gf_clock_pause(GF_Clock *ck)
{
	gf_mx_p(ck->mx);
	if (!ck->nb_paused)
		ck->pause_time = gf_sc_get_clock(ck->compositor);
	ck->nb_paused += 1;
	gf_mx_v(ck->mx);
}

void gf_clock_resume(GF_Clock *ck)
{
	gf_mx_p(ck->mx);
	if (ck->nb_paused)
		ck->nb_paused -= 1;
	//in player mode, increment the start time to reflect how long we have been buffering
	//in non-player mode, since we don't care about real-time, don't update the clock start time
	//this avoids cases where the first composed frame is dispatched while the object(s) are buffering
	//updating the clock would rewind the timebase in the past and won't trigger next frame fetch on these objects
	if (!ck->nb_paused && ck->compositor->player)
		ck->start_time += gf_sc_get_clock(ck->compositor) - ck->pause_time;
	gf_mx_v(ck->mx);
}


u32 gf_clock_real_time(GF_Clock *ck)
{
	u32 time;
	if (!ck) return 0;
	if (!ck->clock_init) return ck->start_time;
	time = ck->nb_paused > 0 ? ck->pause_time : gf_sc_get_clock(ck->compositor);

#ifdef GPAC_FIXED_POINT

	if ((ck->speed < 0) && ((s32) ck->init_timestamp < FIX2INT( (-ck->speed * 100) * (time - ck->start_time)) / 100 ) ) {
		time = 0;
	} else {
		time = ck->speed_set_time + ck->init_timestamp + (time - ck->start_time) * FIX2INT(100*ck->speed) / 100;
	}

#else

	if ((ck->speed < 0) && ((s32) ck->init_timestamp < (-ck->speed) * (time - ck->start_time))) {
		time = 0;
	} else {
		//DO NOT CHANGE the position of cast float->u32, otherwise we have precision issues when ck->init_timestamp
		//is >= 0x40000000. We know for sure that ck->speed * (time - ck->start_time) is positive
		time = ck->speed_set_time + ck->init_timestamp + (u32) (ck->speed * (time - ck->start_time) );
	}

#endif

	return time;
}

GF_EXPORT
u32 gf_clock_time(GF_Clock *ck)
{
	u32 time = gf_clock_real_time(ck);
	if ((ck->audio_delay>0) && (time < (u32) ck->audio_delay)) return 0;
	return time - ck->audio_delay;
}

u64 gf_clock_to_media_time(GF_Clock *ck, u32 clock_val)
{
	u64 t = ck->init_ts_loops;
	t *= 0xFFFFFFFFUL;
	t += clock_val;
	if (ck->has_media_time_shift) {
		if (t > ck->media_ts_orig) t -= ck->media_ts_orig;
		else t=0;
		t += ck->media_time_orig;
	}
	return t;
}

u64 gf_clock_media_time(GF_Clock *ck)
{
	u32 t;
	if (!ck) return 0;
	if (!ck->has_seen_eos && ck->last_ts_rendered) t = ck->last_ts_rendered;
	else t = gf_clock_time(ck);
	return gf_clock_to_media_time(ck, t);
}

u32 gf_clock_elapsed_time(GF_Clock *ck)
{
	if (!ck || ck->nb_buffering || ck->nb_paused) return 0;
	return gf_sys_clock() - ck->start_time;
}

Bool gf_clock_is_started(GF_Clock *ck)
{
	if (!ck || !ck->clock_init || ck->nb_buffering || ck->nb_paused) return 0;
	return 1;
}

/*buffering scene protected by a mutex because it may be triggered by composition memory (audio or visual threads)*/
void gf_clock_buffer_on(GF_Clock *ck)
{
	gf_mx_p(ck->mx);
	if (!ck->nb_buffering) gf_clock_pause(ck);
	ck->nb_buffering += 1;
	gf_mx_v(ck->mx);
}

void gf_clock_buffer_off(GF_Clock *ck)
{
	gf_mx_p(ck->mx);

	if (ck->nb_buffering) {
		ck->nb_buffering -= 1;
		if (!ck->nb_buffering)
			gf_clock_resume(ck);
	}
	gf_mx_v(ck->mx);
}


void gf_clock_set_speed(GF_Clock *ck, Fixed speed)
{
	u32 time;
	if (speed==ck->speed) return;
	time = gf_sc_get_clock(ck->compositor);
	/*adjust start time*/
	ck->speed_set_time = gf_clock_time(ck) - ck->init_timestamp;
	ck->pause_time = ck->start_time = time;
	ck->speed = speed;
}

void gf_clock_set_audio_delay(GF_Clock *ck, s32 ms_delay)
{
	if (ck) ck->audio_delay = ms_delay;
}

u32 gf_timestamp_to_clocktime(u64 ts, u32 timescale)
{
	if (ts==GF_FILTER_NO_TS) return 0;
	//this happens for direct file loaders calling this (btplay & co)
	if (!timescale) return 0;

	ts = gf_timestamp_rescale(ts, timescale, 1000);
	ts = ts % 0xFFFFFFFFUL;
	return (u32) ts;
}

u64 gf_clock_time_absolute(GF_Clock *ck)
{
	if (!ck->clock_init) return 0;
	u64 time = ck->init_ts_loops;
	time *= 0xFFFFFFFFUL;
	time += gf_clock_time(ck);
	return time;
}


s32 gf_clock_diff(GF_Clock *ck, u32 ck_time, u32 ts)
{
	s64 ts_diff;

	ts_diff = ts;
	ts_diff -= ck_time;
	//clock has wrapped around
	if (ck_time & 0x80000000) {
		//cts has not yet wrapped around
		if (! (ts & 0x80000000)) {
			ts_diff += 0xFFFFFFFFUL;
		}
	} else {
		if (ts & 0x80000000) {
			ts_diff -= 0xFFFFFFFFUL;
		}
	}
	if (ck->speed<0)
		ts_diff = -ts_diff;

	return (s32) ts_diff;
}
#endif //GPAC_DISABLE_COMPOSITOR
