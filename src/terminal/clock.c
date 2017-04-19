/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file scene part of GPAC / Media terminal sub-project
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

#include <gpac/internal/terminal_dev.h>

GF_Clock *NewClock(GF_Compositor *compositor)
{
	GF_Clock *tmp;
	GF_SAFEALLOC(tmp, GF_Clock);
	if (!tmp) return NULL;
	tmp->mx = gf_mx_new("Clock");
	tmp->compositor = compositor;
	tmp->speed = FIX_ONE;
	tmp->data_timeout = compositor->net_data_timeout;
	return tmp;
}

void gf_clock_del(GF_Clock *ck)
{
	gf_mx_del(ck->mx);
	gf_free(ck);
}

GF_Clock *gf_clock_find(GF_List *Clocks, u16 clockID, u16 ES_ID)
{
	u32 i;
	GF_Clock *tmp;
	i=0;
	while ((tmp = (GF_Clock *)gf_list_enum(Clocks, &i))) {
		//first check the clock ID
		if (tmp->clockID == clockID) return tmp;
		//then check the ES ID
		if (ES_ID && (tmp->clockID == ES_ID)) return tmp;
	}
	//no clocks found...
	return NULL;
}

static GF_Clock *gf_ck_look_for_clock_dep(GF_Scene *scene, u16 clockID)
{
	u32 i, j;
	GF_ODMExtraPid *xpid;
	GF_ObjectManager *odm;

	/*check in top OD*/
	if (scene->root_od->pid_id == clockID) return scene->root_od->ck;
	i=0;
	while ((xpid = (GF_ODMExtraPid*)gf_list_enum(scene->root_od->extra_pids, &i))) {
		if (xpid->pid_id == clockID) return scene->root_od->ck;
	}
	/*check in sub ODs*/
	j=0;
	while ((odm = (GF_ObjectManager*)gf_list_enum(scene->resources, &j))) {
		if (odm->pid_id == clockID) return odm->ck;
		i=0;
		while ((xpid = (GF_ODMExtraPid*)gf_list_enum(odm->extra_pids, &i))) {
			if (xpid->pid_id == clockID) return odm->ck;
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
	if (scene->root_od->ck && (scene->root_od->ck->clockID == Clock_ESID)) {
		scene->root_od->ck = new_ck;
	}
	i=0;
	while ((odm = (GF_ObjectManager*)gf_list_enum(scene->resources, &i))) {
		if (odm->ck && (odm->ck->clockID == Clock_ESID)) {
			odm->ck = new_ck;
		}
	}
	/*destroy clock*/
	i=0;
	while ((clock = (GF_Clock*)gf_list_enum(clocks, &i))) {
		if (clock->clockID == Clock_ESID) {
			gf_list_rem(clocks, i-1);
			gf_clock_del(clock);
			return;
		}
	}
}

GF_Clock *gf_clock_attach(GF_List *clocks, GF_Scene *scene, u16 clockID, u16 ES_ID, s32 hasOCR)
{
	Bool check_dep;
	GF_Clock *tmp = gf_clock_find(clocks, clockID, ES_ID);
	/*ck dep can only be solved if in the main service*/
	check_dep = (scene->root_od->scene_ns && scene->root_od->scene_ns->Clocks==clocks) ? GF_TRUE : GF_FALSE;

	/*this partly solves a->b->c*/
	if (!tmp && check_dep) tmp = gf_ck_look_for_clock_dep(scene, clockID);
	if (!tmp) {
		tmp = NewClock(scene->compositor);
		tmp->clockID = clockID;
		gf_list_add(clocks, tmp);
	} else {
		if (tmp->clockID == ES_ID) tmp->clockID = clockID;
		/*this finally solves a->b->c*/
		if (check_dep && (tmp->clockID != ES_ID)) gf_ck_resolve_clock_dep(clocks, scene, tmp, ES_ID);
	}
	if (hasOCR >= 0) tmp->use_ocr = hasOCR;
	return tmp;
}

void gf_clock_reset(GF_Clock *ck)
{
	ck->clock_init = 0;
	ck->drift = 0;
	ck->discontinuity_time = 0;
	//do NOT reset buffering flag, because RESET scene called only
	//for the stream owning the clock, and other streams may
	//have signaled buffering on this clock
	ck->init_time = 0;
	ck->StartTime = 0;
	ck->has_seen_eos = 0;
	ck->media_time_at_init = 0;
	ck->has_media_time_shift = 0;
}

void gf_clock_set_time(GF_Clock *ck, u32 TS)
{
	if (!ck->clock_init) {
		ck->init_time = TS;
		ck->clock_init = 1;
		ck->broken_pcr = 0;
		ck->drift = 0;
		/*update starttime and pausetime even in pause mode*/
		ck->PauseTime = ck->StartTime = gf_sc_get_clock(ck->compositor);
	}
}



void gf_clock_pause(GF_Clock *ck)
{
	gf_mx_p(ck->mx);
	if (!ck->Paused)
		ck->PauseTime = gf_sc_get_clock(ck->compositor);
	ck->Paused += 1;
	gf_mx_v(ck->mx);
}

void gf_clock_resume(GF_Clock *ck)
{
	gf_mx_p(ck->mx);
	assert(ck->Paused);
	if (!ck->Paused) {
		assert(!ck->Buffering);
	}
	ck->Paused -= 1;
	if (!ck->Paused)
		ck->StartTime += gf_sc_get_clock(ck->compositor) - ck->PauseTime;
	gf_mx_v(ck->mx);
}


u32 gf_clock_real_time(GF_Clock *ck)
{
	u32 time;
	assert(ck);
	if (!ck->clock_init) return ck->StartTime;
	time = ck->Paused > 0 ? ck->PauseTime : gf_sc_get_clock(ck->compositor);

#ifdef GPAC_FIXED_POINT

	if ((ck->speed < 0) && ((s32) ck->init_time < FIX2INT( (-ck->speed * 100) * (time - ck->StartTime)) / 100 ) ) {
		time = 0;
	} else {
		time = ck->discontinuity_time + ck->init_time + (time - ck->StartTime) * FIX2INT(100*ck->speed) / 100;
	}

#else

	if ((ck->speed < 0) && ((s32) ck->init_time < (-ck->speed) * (time - ck->StartTime))) {
		time = 0;
	} else {
		//DO NOT CHANGE the position of cast float->u32, otherwise we have precision issues when ck->init_time
		//is >= 0x40000000. We know for sure that ck->speed * (time - ck->StartTime) is positive
		time = ck->discontinuity_time + ck->init_time + (u32) (ck->speed * (time - ck->StartTime) );
	}

#endif

	return time;
}

GF_EXPORT
u32 gf_clock_time(GF_Clock *ck)
{
	u32 time = gf_clock_real_time(ck);
	if ((ck->drift>0) && (time < (u32) ck->drift)) return 0;
	return time - ck->drift;
}

u32 gf_clock_media_time(GF_Clock *ck)
{
	u32 t;
	if (!ck) return 0;
	if (!ck->has_seen_eos && ck->last_TS_rendered) t = ck->last_TS_rendered;
	else t = gf_clock_time(ck);
	//if media time is not mapped, we consider that the timestamps are aligned with the media time
	if (ck->has_media_time_shift) {
		if (t>ck->init_time) t -= ck->init_time;
		else t=0;

		t += ck->media_time_at_init;
	}
	return t;
}

u32 gf_clock_elapsed_time(GF_Clock *ck)
{
	if (!ck || ck->Buffering || ck->Paused) return 0;
	return gf_sys_clock() - ck->StartTime;
}

Bool gf_clock_is_started(GF_Clock *ck)
{
	if (!ck || !ck->clock_init || ck->Buffering || ck->Paused) return 0;
	return 1;
}

/*buffering scene protected by a mutex because it may be triggered by composition memory (audio or visual threads)*/
void gf_clock_buffer_on(GF_Clock *ck)
{
	gf_mx_p(ck->mx);
	if (!ck->Buffering) gf_clock_pause(ck);
	ck->Buffering += 1;
	gf_mx_v(ck->mx);
}

void gf_clock_buffer_off(GF_Clock *ck)
{
	gf_mx_p(ck->mx);
	//assert(ck->Buffering);
	if (ck->Buffering) {
		ck->Buffering -= 1;
		if (!ck->Buffering) gf_clock_resume(ck);
	}
	gf_mx_v(ck->mx);
}


void gf_clock_set_speed(GF_Clock *ck, Fixed speed)
{
	u32 time;
	if (speed==ck->speed) return;
	time = gf_sc_get_clock(ck->compositor);
	/*adjust start time*/
	ck->discontinuity_time = gf_clock_time(ck) - ck->init_time;
	ck->PauseTime = ck->StartTime = time;
	ck->speed = speed;
}

void gf_clock_adjust_drift(GF_Clock *ck, s32 ms_drift)
{
	if (ck) ck->drift = ms_drift;
}


/*handle clock discontinuity - for now we only reset timing of all received data and reinit the clock*/
void gf_clock_discontinuity(GF_Clock *ck, GF_Scene *scene, Bool is_pcr_discontinuity)
{
#ifdef FILTER_FIXME
	u32 i, j;
	GF_Channel *ch;
	GF_ObjectManager *odm;

	/*check all channels and reset timing */
	i=0;
	while ((ch = (GF_Channel*)gf_list_enum(scene->root_od->channels, &i))) {
		if (ch->clock == ck) {
			gf_es_reset_timing(ch, is_pcr_discontinuity);
		}
	}
	j=0;
	while ((odm = (GF_ObjectManager*)gf_list_enum(scene->resources, &j))) {
		if (odm->state==GF_ODM_STATE_STOP)
			continue;

		i=0;
		while ((ch = (GF_Channel*)gf_list_enum(odm->channels, &i))) {
			if (ch->clock == ck) {
				gf_es_reset_timing(ch, is_pcr_discontinuity);

				ch->CTS = ch->DTS = 0;
				GF_LOG(GF_LOG_WARNING, GF_LOG_SYNC, ("[SyncLayer] Reinitializing timing for ES%d\n", ch->esd->ESID));
			}
		}
	}

	gf_scene_reset_addons(scene);

	if (ck->has_media_time_shift) {
		u32 new_media_time = ck->media_time_at_init + gf_clock_time(ck) - ck->init_time;

		gf_clock_reset(ck);

		ck->has_media_time_shift = 1;
		ck->media_time_at_init = new_media_time;
	} else {
		gf_clock_reset(ck);
	}
#endif

}
