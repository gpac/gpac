/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
 *					All rights reserved
 *
 *  This file is part of GPAC / Media terminal sub-project
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

#include <gpac/internal/terminal_dev.h>
#include <gpac/renderer.h>

GF_Clock *NewClock(GF_Terminal *term)
{
	GF_Clock *tmp = malloc(sizeof(GF_Clock));
	if (!tmp) return NULL;
	memset(tmp, 0, sizeof(GF_Clock));
	tmp->mx = gf_mx_new();
	tmp->term = term;
	tmp->speed = FIX_ONE;
	return tmp;
}

void gf_clock_del(GF_Clock *ck)
{
	gf_mx_del(ck->mx);
	free(ck);
}

GF_Clock *gf_clock_find(GF_List *Clocks, u16 clockID, u16 ES_ID)
{
	u32 i;
	GF_Clock *tmp;
	for (i = 0; i< gf_list_count(Clocks); i++) {
		tmp = gf_list_get(Clocks, i);
		//first check the clock ID
		if (tmp->clockID == clockID) return tmp;
		//then check the ES ID
		if (ES_ID && (tmp->clockID == ES_ID)) return tmp;
	}
	//no clocks found...
	return NULL;
}

GF_Clock *CK_LookForClockDep(struct _inline_scene *is, u16 clockID)
{
	u32 i, j;
	/*check in top OD*/
	for (i=0; i<gf_list_count(is->root_od->channels); i++) {
		GF_Channel *ch = gf_list_get(is->root_od->channels, i);
		if (ch->esd->ESID == clockID) return ch->clock;
	}
	/*check in sub ODs*/
	for (j=0; j<gf_list_count(is->ODlist); j++) {
		GF_ObjectManager *odm = gf_list_get(is->ODlist, j);
		for (i=0; i<gf_list_count(odm->channels); i++) {
			GF_Channel *ch = gf_list_get(odm->channels, i);
			if (ch->esd->ESID == clockID) return ch->clock;
		}
	}
	return NULL;
}

/*remove clocks created due to out-of-order OCR dependencies*/
void CK_ResolveClockDep(GF_List *clocks, struct _inline_scene *is, GF_Clock *ck, u16 Clock_ESID)
{
	u32 i, j;

	/*check all channels - if any is using a clock which ID is the clock_ESID then
	this clock shall be removed*/
	for (i=0; i<gf_list_count(is->root_od->channels); i++) {
		GF_Channel *ch = gf_list_get(is->root_od->channels, i);
		if (ch->clock->clockID == Clock_ESID) {
			if (is->scene_codec && is->scene_codec->ck == ch->clock) is->scene_codec->ck = ck;
			if (is->od_codec && is->od_codec->ck == ch->clock) is->od_codec->ck = ck;
			if (is->root_od->oci_codec && is->root_od->oci_codec->ck == ch->clock) is->root_od->oci_codec->ck = ck;
			ch->clock = ck;
			if (ch->esd) ch->esd->OCRESID = ck->clockID;
		}
	}
	for (j=0; j<gf_list_count(is->ODlist); j++) {
		GF_ObjectManager *odm = gf_list_get(is->ODlist, j);
		for (i=0; i<gf_list_count(odm->channels); i++) {
			GF_Channel *ch = gf_list_get(odm->channels, i);
			if (ch->clock->clockID == Clock_ESID) {
				if (odm->codec && (odm->codec->ck==ch->clock)) odm->codec->ck = ck;
				if (odm->oci_codec && (odm->oci_codec->ck==ch->clock)) odm->oci_codec->ck = ck;
				ch->clock = ck;
				if (ch->esd) ch->esd->OCRESID = ck->clockID;
			}
		}
	}
	/*destroy clock*/
	for (i=0; i<gf_list_count(clocks); i++) {
		GF_Clock *clock = gf_list_get(clocks, i);
		if (clock->clockID == Clock_ESID) {
			gf_list_rem(clocks, i);
			gf_clock_del(clock);
			return;
		}
	}
}

GF_Clock *gf_clock_attach(GF_List *clocks, struct _inline_scene *is, u16 clockID, u16 ES_ID, s32 hasOCR)
{
	Bool check_dep;
	GF_Clock *tmp = gf_clock_find(clocks, clockID, ES_ID);
	/*ck dep can only be solved if in the main service*/
	check_dep = (is->root_od->net_service && is->root_od->net_service->Clocks==clocks) ? 1 : 0;
	/*this partly solves a->b->c*/
	if (!tmp && check_dep) tmp = CK_LookForClockDep(is, clockID);
	if (!tmp) {
		tmp = NewClock(is->root_od->term);
		tmp->clockID = clockID;
		gf_list_add(clocks, tmp);
	} else {
		if (tmp->clockID == ES_ID) tmp->clockID = clockID;
		/*this finally solves a->b->c*/
		if (check_dep && (tmp->clockID != ES_ID)) CK_ResolveClockDep(clocks, is, tmp, ES_ID);
	}
	if (hasOCR >= 0) tmp->use_ocr = hasOCR;
	return tmp;
}

void gf_clock_reset(GF_Clock *ck)
{
	ck->clock_init = 0;
	ck->drift = 0;
	ck->discontinuity_time = 0;
	//do NOT reset buffering flag, because RESET is called only 
	//for the stream owning the clock, and other streams may 
	//have signaled buffering on this clock
	ck->init_time = 0;
	ck->StartTime = 0;
	ck->has_seen_eos = 0;
}

void gf_clock_set_time(GF_Clock *ck, u32 TS)
{
	if (!ck->clock_init) {
		ck->init_time = TS;
		ck->clock_init = 1;
		/*update starttime and pausetime even in pause mode*/
		ck->PauseTime = ck->StartTime = gf_term_get_time(ck->term);
		ck->drift = gf_sr_get_audio_delay(ck->term->renderer);
	}
	/*TODO: test with pure OCR streams*/
	if (ck->use_ocr) {
		/*just update the drift - we could also apply a drift algo*/
		u32 now = gf_clock_time(ck);
		s32 drift = now - TS;
		ck->drift += drift;
	}
}



void gf_clock_pause(GF_Clock *ck)
{
	gf_mx_p(ck->mx);
	if (!ck->Paused) ck->PauseTime = gf_term_get_time(ck->term);
	ck->Paused += 1;
	gf_mx_v(ck->mx);
}

void gf_clock_resume(GF_Clock *ck)
{
	gf_mx_p(ck->mx);
	assert(ck->Paused);
	ck->Paused -= 1;
	if (!ck->Paused) 
		ck->StartTime += gf_term_get_time(ck->term) - ck->PauseTime;
	gf_mx_v(ck->mx);
}


u32 gf_clock_real_time(GF_Clock *ck)
{
	u32 time;
	if (!ck || !ck->StartTime) return 0;
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
	if ((s32) time < ck->drift) return 0;
	return time - ck->drift;
}


Bool gf_clock_is_started(GF_Clock *ck)
{
	if (!ck->StartTime || ck->Buffering || ck->Paused) return 0;
	return 1;
}

/*buffering is protected by a mutex because it may be triggered by composition memory (audio or visual threads)*/
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
	assert(ck->Buffering);
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
	ck->discontinuity_time = gf_clock_time(ck);
	ck->PauseTime = ck->StartTime = time;
	ck->speed = speed;
}

void gf_clock_adjust_drift(GF_Clock *ck, s32 ms_drift)
{
	if (ck) ck->drift = ms_drift;
}
