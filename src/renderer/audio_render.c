/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Rendering sub-project
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

#include <gpac/internal/renderer_dev.h>

void AR_FillBuffer(void *ar, char *buffer, u32 buffer_size);
u32 AR_MainLoop(void *ar);

GF_Err AR_SetupAudioFormat(GF_AudioRenderer *ar, GF_AudioOutput *dr)
{
	GF_Err e;
	u32 freq, nb_bits, nb_chan, BPS, ch_cfg;
	gf_mixer_get_config(ar->mixer, &freq, &nb_chan, &nb_bits, &ch_cfg);

	/*we try to set the FPS so that SR*2/FPS is an integer, and FPS the smallest int >= 30 fps */
	BPS = freq * nb_chan * nb_bits / 8;

	e = dr->ConfigureOutput(dr, &freq, &nb_chan, &nb_bits, ch_cfg);
	if (e) {
		if (nb_chan>2) {
			nb_chan=2;
			e = dr->ConfigureOutput(dr, &freq, &nb_chan, &nb_bits, ch_cfg);
		}
		if (e) return e;
	}

	gf_mixer_set_config(ar->mixer, freq, nb_chan, nb_bits, ch_cfg);
	ar->audio_delay = ar->audio_out->GetAudioDelay(ar->audio_out);
	return GF_OK;
}

GF_AudioRenderer *gf_sr_ar_load(GF_User *user)
{
	const char *sOpt;
	u32 i, count;
	GF_Err e;
	GF_AudioRenderer *ar;
	ar = (GF_AudioRenderer *) malloc(sizeof(GF_AudioRenderer));
	memset(ar, 0, sizeof(GF_AudioRenderer));

	ar->force_cfg = 0;
	ar->num_buffers = ar->num_buffer_per_sec = 0;
	sOpt = gf_cfg_get_key(user->config, "Audio", "ForceConfig");
	if (sOpt && !stricmp(sOpt, "yes")) {
		ar->force_cfg = 1;
		sOpt = gf_cfg_get_key(user->config, "Audio", "NumBuffers");
		ar->num_buffers = sOpt ? atoi(sOpt) : 6;
		sOpt = gf_cfg_get_key(user->config, "Audio", "BuffersPerSecond");
		ar->num_buffer_per_sec = sOpt ? atoi(sOpt) : 15;
	}

	ar->resync_clocks = 1;
	sOpt = gf_cfg_get_key(user->config, "Audio", "NoResync");
	if (sOpt && !stricmp(sOpt, "yes")) ar->resync_clocks = 0;
	
	ar->mixer = gf_mixer_new(ar);
	ar->user = user;

	/*get a prefered renderer*/
	sOpt = gf_cfg_get_key(user->config, "Audio", "DriverName");
	if (sOpt) {
		ar->audio_out = (GF_AudioOutput *) gf_modules_load_interface_by_name(user->modules, sOpt, GF_AUDIO_OUTPUT_INTERFACE);
		if (!ar->audio_out) {
			ar->audio_out = NULL;
			sOpt = NULL;
		}
	}
	if (!ar->audio_out) {
		count = gf_modules_get_count(ar->user->modules);
		for (i=0; i<count; i++) {
			ar->audio_out = (GF_AudioOutput *) gf_modules_load_interface(ar->user->modules, i, GF_AUDIO_OUTPUT_INTERFACE);
			if (!ar->audio_out) continue;
			/*check that's a valid audio renderer*/
			if (ar->audio_out->SelfThreaded) {
				if (ar->audio_out->SetPriority) break;
			} else {
				if (ar->audio_out->WriteAudio) break;
			}
			gf_modules_close_interface((GF_BaseInterface *)ar->audio_out);
			ar->audio_out = NULL;
		}
	}

	/*if not init we run with a NULL audio renderer*/
	if (ar->audio_out) {
		ar->audio_out->FillBuffer = AR_FillBuffer;
		ar->audio_out->audio_renderer = ar;
		e = ar->audio_out->SetupHardware(ar->audio_out, ar->user->os_window_handler, ar->num_buffers, ar->num_buffer_per_sec);
		if (e==GF_OK) e = AR_SetupAudioFormat(ar, ar->audio_out);
		if (e != GF_OK) {
			gf_modules_close_interface((GF_BaseInterface *)ar->audio_out);
			ar->audio_out = NULL;
		} else {
			/*remember the module we use*/
			gf_cfg_set_key(user->config, "Audio", "DriverName", ar->audio_out->module_name);
			if (!ar->audio_out->SelfThreaded) {
				ar->th = gf_th_new();
				ar->audio_th_state = 1;
				gf_th_run(ar->th, AR_MainLoop, ar);
			}
			if (ar->audio_out->SelfThreaded && ar->audio_out->SetPriority) ar->audio_out->SetPriority(ar->audio_out, GF_THREAD_PRIORITY_REALTIME);
		}
	}
	if (!ar->audio_out) gf_cfg_set_key(user->config, "Audio", "DriverName", "No Audio Output Available");

	sOpt = gf_cfg_get_key(user->config, "Audio", "Volume");
	ar->volume = sOpt ? atoi(sOpt) : 75;
	sOpt = gf_cfg_get_key(user->config, "Audio", "Pan");
	ar->pan = sOpt ? atoi(sOpt) : 50;
	if (ar->audio_out) {
		ar->audio_out->SetVolume(ar->audio_out, ar->volume);
		ar->audio_out->SetPan(ar->audio_out, ar->pan);
	}

	gf_sys_clock_start();
	/*init renderer timer*/
	ar->startTime = gf_sys_clock();
	return ar;
}

void gf_sr_ar_del(GF_AudioRenderer *ar)
{
	if (!ar) return;

	/*resume if paused (might cause deadlock otherwise)*/
	if (ar->Frozen) gf_sr_ar_control(ar, 1);
	/*stop and shutdown*/
	if (ar->audio_out) {
		/*kill audio thread*/
		if (!ar->audio_out->SelfThreaded) {
			ar->audio_th_state = 2;
			while (ar->audio_th_state != 3) gf_sleep(10);
			gf_th_del(ar->th);
		}
		/*lock access before shutdown and emulate a reconfig (avoids mixer lock from self-threaded modules)*/
		ar->need_reconfig = 1;
		gf_mixer_lock(ar->mixer, 1);
		ar->audio_out->Shutdown(ar->audio_out);
		gf_modules_close_interface((GF_BaseInterface *)ar->audio_out);
		gf_mixer_lock(ar->mixer, 0);
	}


	gf_mixer_del(ar->mixer);
	free(ar);

	/*shutdown OS timer system*/
	gf_sys_clock_stop();
}


void gf_sr_ar_reset(GF_AudioRenderer *ar)
{
	gf_mixer_remove_all(ar->mixer);
}

void AR_FreezeIntern(GF_AudioRenderer *ar, Bool DoFreeze, Bool for_reconfig, Bool reset_hw_buffer)
{
	gf_mixer_lock(ar->mixer, 1);
	if (DoFreeze) {
		if (!ar->Frozen) {
			ar->FreezeTime = gf_sys_clock();
			if (!for_reconfig && ar->audio_out && ar->audio_out->Play) ar->audio_out->Play(ar->audio_out, 0);
			ar->Frozen = 1;
		}
	} else {
		if (ar->Frozen) {
			if (!for_reconfig && ar->audio_out && ar->audio_out->Play) ar->audio_out->Play(ar->audio_out, reset_hw_buffer ? 2 : 1);
			ar->Frozen = 0;
			ar->startTime += gf_sys_clock() - ar->FreezeTime;
		}
	}
	gf_mixer_lock(ar->mixer, 0);
}

void gf_sr_ar_control(GF_AudioRenderer *ar, u32 PauseType)
{
	AR_FreezeIntern(ar, !PauseType, 0, (PauseType==2) ? 1 : 0);
}

void gf_sr_ar_set_volume(GF_AudioRenderer *ar, u32 Volume)
{	
	char sOpt[10];
	gf_mixer_lock(ar->mixer, 1);
	ar->volume = MIN(Volume, 100);
	if (ar->audio_out) ar->audio_out->SetVolume(ar->audio_out, ar->volume);
	sprintf(sOpt, "%d", ar->volume);
	gf_cfg_set_key(ar->user->config, "Audio", "Volume", sOpt);

	gf_mixer_lock(ar->mixer, 0);
}
void gf_sr_ar_set_pan(GF_AudioRenderer *ar, u32 Balance)
{
	gf_mixer_lock(ar->mixer, 1);
	ar->pan = MIN(Balance, 100);
	if (ar->audio_out) ar->audio_out->SetPan(ar->audio_out, ar->pan);
	gf_mixer_lock(ar->mixer, 0);
}


void gf_sr_ar_add_src(GF_AudioRenderer *ar, GF_AudioInterface *source)
{
	Bool recfg;
	if (!ar) return;
	/*lock mixer*/
	gf_mixer_lock(ar->mixer, 1);
	gf_mixer_add_input(ar->mixer, source);
	/*if changed reconfig*/
	recfg = gf_mixer_reconfig(ar->mixer);
	if (!ar->need_reconfig) ar->need_reconfig = recfg;

	/*unlock mixer*/
	gf_mixer_lock(ar->mixer, 0);
}

void gf_sr_ar_remove_src(GF_AudioRenderer *ar, GF_AudioInterface *source)
{
	if (ar) gf_mixer_remove_input(ar->mixer, source);
}


void gf_sr_ar_set_priority(GF_AudioRenderer *ar, u32 priority)
{
	ar->priority = priority;
	if (ar->audio_out && ar->audio_out->SelfThreaded) {
		ar->audio_out->SetPriority(ar->audio_out, priority);
	} else {
		gf_th_set_priority(ar->th, priority);
	}
}

u32 AR_MainLoop(void *p)
{
	GF_AudioRenderer *ar = (GF_AudioRenderer *) p;

	ar->audio_th_state = 1;
	while (ar->audio_th_state == 1) {
		gf_mixer_lock(ar->mixer, 1);
		if (ar->Frozen) {
			gf_mixer_lock(ar->mixer, 0);
			gf_sleep(10);
		} else {
			ar->audio_out->WriteAudio(ar->audio_out);
			gf_mixer_lock(ar->mixer, 0);
		}
	}
	ar->audio_th_state = 3;
	return 0;
}

void AR_FillBuffer(void *ptr, char *buffer, u32 buffer_size)
{
	GF_AudioRenderer *ar = (GF_AudioRenderer *) ptr;
	if (!ar->need_reconfig) gf_mixer_get_output(ar->mixer, buffer, buffer_size);
}

void gf_sr_ar_reconfig(GF_AudioRenderer *ar)
{
	if (!ar->need_reconfig || !ar->audio_out) return;
	/*lock mixer*/
	gf_mixer_lock(ar->mixer, 1);

	AR_FreezeIntern(ar, 1, 1, 0);

	AR_SetupAudioFormat(ar, ar->audio_out);

	AR_FreezeIntern(ar, 0, 1, 0);
	
	ar->need_reconfig = 0;
	/*unlock mixer*/
	gf_mixer_lock(ar->mixer, 0);
}

u32 gf_sr_ar_get_delay(GF_AudioRenderer *ar)
{
	return ar->audio_out->GetAudioDelay(ar->audio_out);
}


u32 gf_sr_ar_get_clock(GF_AudioRenderer *ar)
{
	if (ar->Frozen) return ar->FreezeTime - ar->startTime;
	return gf_sys_clock() - ar->startTime;
}
