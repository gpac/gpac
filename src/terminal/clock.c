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

GF_Clock *NewClock(GF_Terminal *term)
{
	GF_Clock *tmp;
	GF_SAFEALLOC(tmp, GF_Clock);
	if (!tmp) return NULL;
	tmp->mx = gf_mx_new("Clock");
	tmp->term = term;
	tmp->speed = FIX_ONE;
	if (term->play_state) tmp->Paused = 1;
	tmp->data_timeout = term->net_data_timeout;
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

GF_Clock *CK_LookForClockDep(GF_Scene *scene, u16 clockID)
{
	u32 i, j;
	GF_Channel *ch;
	GF_ObjectManager *odm;

	/*check in top OD*/
	i=0;
	while ((ch = (GF_Channel*)gf_list_enum(scene->root_od->channels, &i))) {
		if (ch->esd->ESID == clockID) return ch->clock;
	}
	/*check in sub ODs*/
	j=0;
	while ((odm = (GF_ObjectManager*)gf_list_enum(scene->resources, &j))) {
		i=0;
		while ((ch = (GF_Channel*)gf_list_enum(odm->channels, &i))) {
			if (ch->esd->ESID == clockID) return ch->clock;
		}
	}
	return NULL;
}

/*remove clocks created due to out-of-order OCR dependencies*/
void CK_ResolveClockDep(GF_List *clocks, GF_Scene *scene, GF_Clock *ck, u16 Clock_ESID)
{
	u32 i, j;
	GF_Clock *clock;
	GF_Channel *ch;
	GF_ObjectManager *odm;

	/*check all channels - if any scene using a clock which ID == the clock_ESID then
	this clock shall be removed*/
	i=0;
	while ((ch = (GF_Channel*)gf_list_enum(scene->root_od->channels, &i))) {
		if (ch->clock->clockID == Clock_ESID) {
			if (scene->scene_codec && scene->scene_codec->ck == ch->clock) scene->scene_codec->ck = ck;
			if (scene->od_codec && scene->od_codec->ck == ch->clock) scene->od_codec->ck = ck;
#ifndef GPAC_MINIMAL_ODF
			if (scene->root_od->oci_codec && scene->root_od->oci_codec->ck == ch->clock) scene->root_od->oci_codec->ck = ck;
#endif
			ch->clock = ck;
			if (ch->esd) ch->esd->OCRESID = ck->clockID;
		}
	}
	j=0;
	while ((odm = (GF_ObjectManager*)gf_list_enum(scene->resources, &j))) {
		i=0;
		while ((ch = (GF_Channel*)gf_list_enum(odm->channels, &i))) {
			if (ch->clock->clockID == Clock_ESID) {
				if (odm->codec && (odm->codec->ck==ch->clock)) odm->codec->ck = ck;
#ifndef GPAC_MINIMAL_ODF
				if (odm->oci_codec && (odm->oci_codec->ck==ch->clock)) odm->oci_codec->ck = ck;
#endif
				ch->clock = ck;
				if (ch->esd) ch->esd->OCRESID = ck->clockID;
			}
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
	check_dep = (scene->root_od->net_service && scene->root_od->net_service->Clocks==clocks) ? GF_TRUE : GF_FALSE;
	/*this partly solves a->b->c*/
	if (!tmp && check_dep) tmp = CK_LookForClockDep(scene, clockID);
	if (!tmp) {
		tmp = NewClock(scene->root_od->term);
		tmp->clockID = clockID;
		gf_list_add(clocks, tmp);
	} else {
		if (tmp->clockID == ES_ID) tmp->clockID = clockID;
		/*this finally solves a->b->c*/
		if (check_dep && (tmp->clockID != ES_ID)) CK_ResolveClockDep(clocks, scene, tmp, ES_ID);
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
}

void gf_clock_stop(GF_Clock *ck)
{
	ck->clock_init = 0;
	ck->StartTime = 0;
}

void gf_clock_set_time(GF_Clock *ck, u32 TS)
{
	if (!ck->clock_init) {
		ck->init_time = TS;
		ck->clock_init = 1;
		ck->drift = 0;
		/*update starttime and pausetime even in pause mode*/
		ck->PauseTime = ck->StartTime = gf_term_get_time(ck->term);
		if (ck->term->play_state) ck->Paused ++;
	}
#if 0
	/*TODO: test with pure OCR streams*/
	else if (ck->use_ocr) {
		/*just update the drift - we could also apply a drift algo*/
		u32 now = gf_clock_real_time(ck);
		s32 drift = (s32) TS - (s32) now;
		ck->drift += drift;
	}
#endif
}



void gf_clock_pause(GF_Clock *ck)
{
	gf_mx_p(ck->mx);
	if (!ck->Paused)
		ck->PauseTime = gf_term_get_time(ck->term);
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
		ck->StartTime += gf_term_get_time(ck->term) - ck->PauseTime;
	gf_mx_v(ck->mx);
}


u32 gf_clock_real_time(GF_Clock *ck)
{
	u32 time;
	assert(ck);
	if (!ck->clock_init) return ck->StartTime;
	time = ck->Paused > 0 ? ck->PauseTime : gf_term_get_time(ck->term);
#ifdef GPAC_FIXED_POINT
	time = ck->discontinuity_time + ck->init_time + (time - ck->StartTime) * FIX2INT(100*ck->speed) / 100;
#else
	time = ck->discontinuity_time + ck->init_time + (u32) (ck->speed * (time - ck->StartTime) );
#endif
	return time;
}

u32 gf_clock_time(GF_Clock *ck)
{
	u32 time = gf_clock_real_time(ck);
	if ((ck->drift>0) && (time < (u32) ck->drift)) return 0;
	return time - ck->drift;
}

u32 gf_clock_elapse_time(GF_Clock *ck)
{
	if (ck->no_time_ctrl) return gf_clock_time(ck) - ck->init_time;
	return gf_clock_time(ck);
}


Bool gf_clock_is_started(GF_Clock *ck)
{
	if (!ck || ck->Buffering || ck->Paused) return 0;
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
	time = gf_term_get_time(ck->term);
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
	u32 i, j;
	GF_Channel *ch;
	GF_ObjectManager *odm;

	/*check all channels and reset timing */
	i=0;
	while ((ch = (GF_Channel*)gf_list_enum(scene->root_od->channels, &i))) {
		if (ch->clock == ck) {
			gf_es_reset_timing(ch);
		}
	}
	j=0;
	while ((odm = (GF_ObjectManager*)gf_list_enum(scene->resources, &j))) {
		if (odm->state==GF_ODM_STATE_STOP)
			continue;

		i=0;
		while ((ch = (GF_Channel*)gf_list_enum(odm->channels, &i))) {
			if (ch->clock == ck) {
				//on clock looping we force emptying all buffer
				//on clock disc we should only reset timing, but this needs further debugging
				if (!is_pcr_discontinuity && odm->codec && gf_term_lock_codec(odm->codec, GF_TRUE, GF_FALSE)) {
					gf_es_reset_buffers(ch);
					ch->IsClockInit = 0;
					gf_term_lock_codec(odm->codec, GF_FALSE, GF_FALSE);
					GF_LOG(GF_LOG_WARNING, GF_LOG_SYNC, ("[SyncLayer] Reinitializing buffers for ES%d\n", ch->esd->ESID));
				} else {
//					gf_es_reset_timing(ch);
//					ch->IsClockInit = 0;

					gf_es_reset_buffers(ch);
					ch->IsClockInit = 0;
					gf_term_lock_codec(odm->codec, GF_FALSE, GF_FALSE);

					GF_LOG(GF_LOG_WARNING, GF_LOG_SYNC, ("[SyncLayer] Reinitializing timing for ES%d\n", ch->esd->ESID));
				}

				if (ch->odm->codec && ch->odm->codec->CB)
					gf_cm_reset_timing(ch->odm->codec->CB);

			}
		}
	}
	gf_clock_reset(ck);
}
