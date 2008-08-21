/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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

#include <gpac/internal/compositor_dev.h>


static GF_Err gf_ar_setup_output_format(GF_AudioRenderer *ar)
{
	GF_Err e;
	u32 freq, nb_bits, nb_chan, ch_cfg;
	gf_mixer_get_config(ar->mixer, &freq, &nb_chan, &nb_bits, &ch_cfg);

	/*user disabled multichannel audio*/
	if (ar->disable_multichannel && (nb_chan>2) ) nb_chan = 2;

	e = ar->audio_out->ConfigureOutput(ar->audio_out, &freq, &nb_chan, &nb_bits, ch_cfg);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[AudioRender] reconfigure error %e\n", e));
		if (nb_chan>2) {
			nb_chan=2;
			e = ar->audio_out->ConfigureOutput(ar->audio_out, &freq, &nb_chan, &nb_bits, ch_cfg);
		}
		if (e) return e;
	}
	gf_mixer_set_config(ar->mixer, freq, nb_chan, nb_bits, ch_cfg);
	ar->audio_delay = ar->audio_out->GetAudioDelay(ar->audio_out);

	ar->audio_out->SetVolume(ar->audio_out, ar->volume);
	ar->audio_out->SetPan(ar->audio_out, ar->pan);
	
	return GF_OK;
}

static u32 gf_ar_fill_output(void *ptr, char *buffer, u32 buffer_size)
{
	GF_AudioRenderer *ar = (GF_AudioRenderer *) ptr;
	if (!ar->need_reconfig) {
		return gf_mixer_get_output(ar->mixer, buffer, buffer_size);
	}
	return 0;
}

u32 gf_ar_proc(void *p)
{
	GF_AudioRenderer *ar = (GF_AudioRenderer *) p;

	ar->audio_th_state = 1;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[AudioRender] Entering audio thread ID %d\n", gf_th_id() ));

	gf_mixer_lock(ar->mixer, 1);
	ar->need_reconfig = 1;
	gf_sc_ar_reconfig(ar);
	gf_mixer_lock(ar->mixer, 0);

	while (ar->audio_th_state == 1) {
		//GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[AudioRender] Audio simulation step\n"));
		
		/*THIS IS NEEDED FOR SYMBIAN - if no yield here, the audio module always grabs the 
		main mixer mutex and it takes forever before it can be grabed by another thread, 
		for instance when reconfiguring scene*/
		gf_sleep(0);

		gf_mixer_lock(ar->mixer, 1);
		if (ar->Frozen || gf_mixer_empty(ar->mixer) ) {
			gf_mixer_lock(ar->mixer, 0);
			gf_sleep(33);
		} else {
			if (ar->need_reconfig) gf_sc_ar_reconfig(ar);
			ar->audio_out->WriteAudio(ar->audio_out);
			gf_mixer_lock(ar->mixer, 0);
		}
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[AudioRender] Exiting audio thread\n"));
	ar->audio_out->Shutdown(ar->audio_out);
	ar->audio_th_state = 3;
	return 0;
}


GF_AudioRenderer *gf_sc_ar_load(GF_User *user)
{
	const char *sOpt;
	u32 i, count;
	u32 num_buffers, total_duration;
	GF_Err e;
	GF_AudioRenderer *ar;
	ar = (GF_AudioRenderer *) malloc(sizeof(GF_AudioRenderer));
	memset(ar, 0, sizeof(GF_AudioRenderer));

	num_buffers = total_duration = 0;
	sOpt = gf_cfg_get_key(user->config, "Audio", "ForceConfig");
	if (sOpt && !stricmp(sOpt, "yes")) {
		sOpt = gf_cfg_get_key(user->config, "Audio", "NumBuffers");
		num_buffers = sOpt ? atoi(sOpt) : 6;
		sOpt = gf_cfg_get_key(user->config, "Audio", "TotalDuration");
		total_duration = sOpt ? atoi(sOpt) : 400;
	}

	sOpt = gf_cfg_get_key(user->config, "Audio", "NoResync");
	ar->disable_resync = (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0;
	sOpt = gf_cfg_get_key(user->config, "Audio", "DisableMultiChannel");
	ar->disable_multichannel = (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0;
	
	ar->mixer = gf_mixer_new(ar);
	ar->user = user;

	sOpt = gf_cfg_get_key(user->config, "Audio", "Volume");
	ar->volume = sOpt ? atoi(sOpt) : 75;
	sOpt = gf_cfg_get_key(user->config, "Audio", "Pan");
	ar->pan = sOpt ? atoi(sOpt) : 50;

	if (! (user->init_flags & GF_TERM_NO_AUDIO) ) {

		/*get a prefered compositor*/
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
				GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[AudioRender] Audio output module %s loaded\n", ar->audio_out->module_name));
				/*check that's a valid audio compositor*/
				if (ar->audio_out->SelfThreaded) {
					if (ar->audio_out->SetPriority) break;
				} else {
					if (ar->audio_out->WriteAudio) break;
				}
				gf_modules_close_interface((GF_BaseInterface *)ar->audio_out);
				ar->audio_out = NULL;
			}
		}

		/*if not init we run with a NULL audio compositor*/
		if (ar->audio_out) {
			ar->audio_out->FillBuffer = gf_ar_fill_output;
			ar->audio_out->audio_renderer = ar;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[AudioRender] Setting up audio module %s\n", ar->audio_out->module_name));
			e = ar->audio_out->Setup(ar->audio_out, ar->user->os_window_handler, num_buffers, total_duration);
			
			/*if audio module is not threaded, reconfigure it from our own thread*/
//			if (e==GF_OK) e = gf_ar_setup_output_format(ar);

			if (e != GF_OK) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("Could not setup audio out %s\n", ar->audio_out->module_name));
				gf_modules_close_interface((GF_BaseInterface *)ar->audio_out);
				ar->audio_out = NULL;
			} else {
				/*remember the module we use*/
				gf_cfg_set_key(user->config, "Audio", "DriverName", ar->audio_out->module_name);
				if (!ar->audio_out->SelfThreaded) {
					ar->th = gf_th_new("AudioRenderer");
					gf_th_run(ar->th, gf_ar_proc, ar);
				} else {
					gf_ar_setup_output_format(ar);
					if (ar->audio_out->SetPriority) ar->audio_out->SetPriority(ar->audio_out, GF_THREAD_PRIORITY_REALTIME);
				}
			}
		}
		if (!ar->audio_out) gf_cfg_set_key(user->config, "Audio", "DriverName", "No Audio Output Available");
	}

	/*init compositor timer*/
	ar->startTime = gf_sys_clock();
	return ar;
}

void gf_sc_ar_del(GF_AudioRenderer *ar)
{
	if (!ar) return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[AudioRender] Destroying compositor\n"));
	/*resume if paused (might cause deadlock otherwise)*/
	if (ar->Frozen) gf_sc_ar_control(ar, 1);
	/*stop and shutdown*/
	if (ar->audio_out) {
		/*kill audio thread*/
		if (!ar->audio_out->SelfThreaded) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[AudioRender] stoping audio thread\n"));
			ar->audio_th_state = 2;
			while (ar->audio_th_state != 3) {
				gf_sleep(33);
			}
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[AudioRender] audio thread stopped\n"));
			gf_th_del(ar->th);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[AudioRender] audio thread destroyed\n"));
		}
		/*lock access before shutdown and emulate a reconfig (avoids mixer lock from self-threaded modules)*/
		ar->need_reconfig = 1;
		gf_mixer_lock(ar->mixer, 1);
		if (ar->audio_out->SelfThreaded) ar->audio_out->Shutdown(ar->audio_out);
		gf_modules_close_interface((GF_BaseInterface *)ar->audio_out);
		gf_mixer_lock(ar->mixer, 0);
	}
	gf_mixer_del(ar->mixer);
	free(ar);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[AudioRender] Renderer destroyed\n"));
}


void gf_sc_ar_reset(GF_AudioRenderer *ar)
{
	gf_mixer_remove_all(ar->mixer);
}

static void gf_ar_freeze_intern(GF_AudioRenderer *ar, Bool DoFreeze, Bool for_reconfig, Bool reset_hw_buffer)
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

void gf_sc_ar_control(GF_AudioRenderer *ar, u32 PauseType)
{
	gf_ar_freeze_intern(ar, !PauseType, 0, (PauseType==2) ? 1 : 0);
}

void gf_sc_ar_set_volume(GF_AudioRenderer *ar, u32 Volume)
{	
	char sOpt[10];
	gf_mixer_lock(ar->mixer, 1);
	ar->volume = MIN(Volume, 100);
	if (ar->audio_out) ar->audio_out->SetVolume(ar->audio_out, ar->volume);
	sprintf(sOpt, "%d", ar->volume);
	gf_cfg_set_key(ar->user->config, "Audio", "Volume", sOpt);

	gf_mixer_lock(ar->mixer, 0);
}
void gf_sc_ar_set_pan(GF_AudioRenderer *ar, u32 Balance)
{
	gf_mixer_lock(ar->mixer, 1);
	ar->pan = MIN(Balance, 100);
	if (ar->audio_out) ar->audio_out->SetPan(ar->audio_out, ar->pan);
	gf_mixer_lock(ar->mixer, 0);
}


void gf_sc_ar_add_src(GF_AudioRenderer *ar, GF_AudioInterface *source)
{
	Bool recfg;
	if (!ar) return;
	/*lock mixer*/
	gf_mixer_lock(ar->mixer, 1);
	gf_mixer_add_input(ar->mixer, source);
	/*if changed reconfig*/
	recfg = gf_mixer_reconfig(ar->mixer);
	if (!ar->need_reconfig) ar->need_reconfig = recfg;

	if (!gf_mixer_empty(ar->mixer) && ar->audio_out && ar->audio_out->Play) 
		ar->audio_out->Play(ar->audio_out, 1);
	/*unlock mixer*/
	gf_mixer_lock(ar->mixer, 0);
}

void gf_sc_ar_remove_src(GF_AudioRenderer *ar, GF_AudioInterface *source)
{
	if (ar) {
		gf_mixer_remove_input(ar->mixer, source);
		if (gf_mixer_empty(ar->mixer) && ar->audio_out && ar->audio_out->Play) 
			ar->audio_out->Play(ar->audio_out, 0);
	}
}


void gf_sc_ar_set_priority(GF_AudioRenderer *ar, u32 priority)
{
	if (ar->audio_out && ar->audio_out->SelfThreaded) {
		ar->audio_out->SetPriority(ar->audio_out, priority);
	} else {
		gf_th_set_priority(ar->th, priority);
	}
}

void gf_sc_ar_reconfig(GF_AudioRenderer *ar)
{
	if (!ar->need_reconfig || !ar->audio_out) return;
	/*lock mixer*/
	gf_mixer_lock(ar->mixer, 1);

	gf_ar_freeze_intern(ar, 1, 1, 0);

	ar->need_reconfig = 0;
	gf_ar_setup_output_format(ar);

	gf_ar_freeze_intern(ar, 0, 1, 0);
	
	/*unlock mixer*/
	gf_mixer_lock(ar->mixer, 0);
}

u32 gf_sc_ar_get_delay(GF_AudioRenderer *ar)
{
	return ar->audio_out->GetAudioDelay(ar->audio_out);
}


u32 gf_sc_ar_get_clock(GF_AudioRenderer *ar)
{
	if (ar->Frozen) return ar->FreezeTime - ar->startTime;
	return gf_sys_clock() - ar->startTime;
}
