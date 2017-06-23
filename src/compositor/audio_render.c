/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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

GF_Err gf_afc_load(GF_AudioFilterChain *afc, GF_User *user, char *filterstring)
{
	struct _audiofilterentry *prev_filter = NULL;

	while (filterstring) {
		u32 i, count;
		GF_AudioFilter *filter;
		char *sep = strstr(filterstring, ";;");
		if (sep) sep[0] = 0;

		count = gf_modules_get_count(user->modules);
		filter = NULL;
		for (i=0; i<count; i++) {
			filter = (GF_AudioFilter *)gf_modules_load_interface(user->modules, i, GF_AUDIO_FILTER_INTERFACE);
			if (filter) {
				if (filter->SetFilter
				        && filter->Configure
				        && filter->Process
				        && filter->Reset
				        && filter->SetOption
				        && filter->GetOption
				        && filter->SetFilter(filter, filterstring)
				   )
					break;

				gf_modules_close_interface((GF_BaseInterface *)filter);
			}
			filter = NULL;
		}
		if (filter) {
			struct _audiofilterentry *entry;
			GF_SAFEALLOC(entry, struct _audiofilterentry);
			if (!entry) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate audio filter entry\n"));
			} else {
				entry->filter = filter;
				if (prev_filter) prev_filter->next = entry;
				else afc->filters = entry;
				prev_filter = entry;
			}
		}
		if (sep) {
			sep[0] = ';';
			filterstring = sep + 2;
		} else {
			break;
		}
	}
	return GF_OK;
}

GF_Err gf_afc_setup(GF_AudioFilterChain *afc, u32 bps, u32 sr, u32 chan, u32 ch_cfg, u32 *ch_out, u32 *ch_cfg_out)
{
	struct _audiofilterentry *entry;
	u32 block_len;
	u32 och, ocfg, in_ch;
	Bool not_in_place;

	if (afc->tmp_block1) gf_free(afc->tmp_block1);
	afc->tmp_block1 = NULL;
	if (afc->tmp_block2) gf_free(afc->tmp_block2);
	afc->tmp_block2 = NULL;

	*ch_out = *ch_cfg_out = 0;
	in_ch = chan;
	afc->min_block_size = 0;
	afc->max_block_size = 0;
	afc->delay_ms = 0;

	not_in_place = GF_FALSE;

	entry = afc->filters;
	while (entry) {
		if (entry->in_block) {
			gf_free(entry->in_block);
			entry->in_block = NULL;
		}

		if (entry->filter->Configure(entry->filter, sr, bps, chan, ch_cfg, &och, &ocfg, &block_len, &entry->delay_ms, &entry->in_place)==GF_OK) {
			u32 out_block_size;
			entry->in_block_size = chan * bps * block_len / 8;
			if (!afc->min_block_size || (afc->min_block_size > entry->in_block_size))
				afc->min_block_size = entry->in_block_size;

			out_block_size = och * bps * block_len / 8;
			if (afc->max_block_size < out_block_size) afc->max_block_size = out_block_size;

			entry->enable = GF_TRUE;
			chan = och;
			ch_cfg = ocfg;

			if (!entry->in_place) not_in_place = GF_TRUE;

			afc->delay_ms += entry->delay_ms;
		} else {
			entry->enable = GF_FALSE;
		}
		entry = entry->next;
	}

	if (!afc->max_block_size) afc->max_block_size = 1000;
	if (!afc->min_block_size) afc->min_block_size = afc->max_block_size * in_ch / chan;
	afc->tmp_block1 = (char*)gf_malloc(sizeof(char) * afc->max_block_size * 2);
	if (!afc->tmp_block1) return GF_OUT_OF_MEM;
	if (not_in_place) {
		afc->tmp_block2 = (char*)gf_malloc(sizeof(char) * afc->max_block_size * 2);
		if (!afc->tmp_block2) return GF_OUT_OF_MEM;
	}

	/*alloc buffers*/
	entry = afc->filters;
	while (entry) {
		if (entry->enable && entry->in_block_size) {
			entry->in_block = (char*)gf_malloc(sizeof(char) * (entry->in_block_size + afc->max_block_size) );
			if (!entry->in_block) return GF_OUT_OF_MEM;
		}
		entry = entry->next;
	}
	*ch_out = chan;
	*ch_cfg_out = ch_cfg;
	afc->enable_filters = GF_TRUE;
	return GF_OK;
}

u32 gf_afc_process(GF_AudioFilterChain *afc, u32 nb_bytes)
{
	struct _audiofilterentry *entry = afc->filters;

	while (entry) {
		char *inptr, *outptr;
		if (!nb_bytes || !entry->enable) {
			entry = entry->next;
			continue;
		}
		inptr = afc->tmp_block1;
		outptr = entry->in_place ? afc->tmp_block1 : afc->tmp_block2;

		/*sample-based input, process directly the data*/
		if (!entry->in_block) {
			entry->filter->Process(entry->filter, inptr, nb_bytes, outptr, &nb_bytes);
		} else {
			u32 processed = 0;
			u32 nb_bytes_out = 0;

			assert(nb_bytes + entry->nb_bytes <= entry->in_block_size + afc->max_block_size);
			/*copy bytes in input*/
			memcpy(entry->in_block + entry->nb_bytes, inptr, nb_bytes);
			entry->nb_bytes += nb_bytes;

			/*and process*/
			while (entry->nb_bytes >= entry->in_block_size) {
				u32 done;
				entry->filter->Process(entry->filter, entry->in_block + processed, entry->in_block_size, outptr + nb_bytes_out, &done);
				done = entry->in_block_size;
				nb_bytes_out += done;
				entry->nb_bytes -= entry->in_block_size;
				processed += entry->in_block_size;
			}
			/*move remaining data at the beginning of the buffer*/
			if (processed && entry->nb_bytes)
				memmove(entry->in_block, entry->in_block+processed, entry->nb_bytes);

			nb_bytes = nb_bytes_out;
		}

		/*swap ptr so that input data of next step (filter, output write) is always in tmp_block1*/
		if (inptr != outptr) {
			afc->tmp_block1 = outptr;
			afc->tmp_block2 = inptr;
		}
		entry = entry->next;
	}
	return nb_bytes;
}

void gf_afc_unload(GF_AudioFilterChain *afc)
{
	while (afc->filters) {
		struct _audiofilterentry *tmp = afc->filters;
		afc->filters = tmp->next;
		gf_modules_close_interface((GF_BaseInterface *)tmp->filter);
		if (tmp->in_block) gf_free(tmp->in_block);
		gf_free(tmp);
	}
	if (afc->tmp_block1) gf_free(afc->tmp_block1);
	if (afc->tmp_block2) gf_free(afc->tmp_block2);
	memset(afc, 0, sizeof(GF_AudioFilterChain));
}

void gf_afc_reset(GF_AudioFilterChain *afc)
{
	struct _audiofilterentry *filter = afc->filters;
	while (filter) {
		filter->filter->Reset(filter->filter);
		filter->nb_bytes = 0;

		filter = filter->next;
	}
}


static GF_Err gf_ar_setup_output_format(GF_AudioRenderer *ar)
{
	GF_Err e;
	u32 freq, nb_bits, nb_chan, ch_cfg;
	u32 in_ch, in_cfg, in_bps, in_freq;

	gf_mixer_get_config(ar->mixer, &freq, &nb_chan, &nb_bits, &ch_cfg);

	/*user disabled multichannel audio*/
	if (ar->disable_multichannel && (nb_chan>2) ) nb_chan = 2;

	in_ch = nb_chan;
	in_cfg = ch_cfg;
	in_bps = nb_bits;
	in_freq = freq;

	if (ar->filter_chain.filters) {
		u32 osr, obps, och, ocfg;

		e = gf_afc_setup(&ar->filter_chain, nb_bits, freq, nb_chan, ch_cfg, &och, &ocfg);
		osr = freq;
		obps = nb_bits;
		nb_chan = och;

		/*try to reconfigure audio output*/
		if (!e)
			e = ar->audio_out->ConfigureOutput(ar->audio_out, &osr, &och, &obps, ocfg);

		/*output module cannot support filter output, disable it ...*/
		if (e || (osr != freq) || (och != nb_chan) || (obps != nb_bits)) {
			nb_bits = in_bps;
			freq = in_freq;
			nb_chan = in_ch;
			ar->filter_chain.enable_filters = GF_FALSE;
			e = ar->audio_out->ConfigureOutput(ar->audio_out, &freq, &nb_chan, &nb_bits, ch_cfg);
		}
	} else {
		e = ar->audio_out->ConfigureOutput(ar->audio_out, &freq, &nb_chan, &nb_bits, ch_cfg);
	}

	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[AudioRender] reconfigure error %d\n", e));
		if (nb_chan>2) {
			nb_chan=2;
			in_ch=2;
			ch_cfg=0;
			e = ar->audio_out->ConfigureOutput(ar->audio_out, &freq, &nb_chan, &nb_bits, ch_cfg);
		}
		if (e) return e;
	}
	gf_mixer_set_config(ar->mixer, freq, nb_chan, nb_bits, in_cfg);
	ar->audio_delay = ar->audio_out->GetAudioDelay(ar->audio_out);

	ar->audio_out->SetVolume(ar->audio_out, ar->volume);
	ar->audio_out->SetPan(ar->audio_out, ar->pan);

	ar->time_at_last_config = ar->current_time;
	ar->bytes_requested = 0;
	ar->bytes_per_second = freq * nb_chan * nb_bits / 8;

	if (ar->audio_listeners) {
		u32 k=0;
		GF_AudioListener *l;
		while ((l = (GF_AudioListener*)gf_list_enum(ar->audio_listeners, &k))) {
			l->on_audio_reconfig(l->udta, in_freq, in_bps, in_ch, in_cfg);
		}
	}
	return GF_OK;
}

static void gf_ar_pause(GF_AudioRenderer *ar, Bool DoFreeze, Bool for_reconfig, Bool reset_hw_buffer)
{
	gf_mixer_lock(ar->mixer, GF_TRUE);
	if (DoFreeze) {
		if (!ar->Frozen) {
			ar->freeze_time = gf_sys_clock_high_res();
			if (!for_reconfig && ar->audio_out && ar->audio_out->Play) ar->audio_out->Play(ar->audio_out, 0);
			ar->Frozen = GF_TRUE;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SYNC, ("[Audio] pausing master clock - time "LLD" (sys time "LLD")\n", ar->freeze_time, gf_sys_clock_high_res()));
		}
	} else {
		if (ar->Frozen) {
			if (!for_reconfig && ar->audio_out && ar->audio_out->Play) ar->audio_out->Play(ar->audio_out, reset_hw_buffer ? 2 : 1);
			ar->Frozen = GF_FALSE;
			ar->start_time += gf_sys_clock_high_res() - ar->freeze_time;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SYNC, ("[Audio] resuming master clock - new time "LLD" (sys time "LLD") \n", ar->start_time, gf_sys_clock_high_res()));
		}
	}
	gf_mixer_lock(ar->mixer, GF_FALSE);
}


static u32 gf_ar_fill_output(void *ptr, char *buffer, u32 buffer_size)
{
	u32 written;
	GF_AudioRenderer *ar = (GF_AudioRenderer *) ptr;
	if (!ar->need_reconfig) {
		u32 delay_ms = ar->disable_resync ?	0 : ar->audio_delay;

		if (ar->Frozen) {
			memset(buffer, 0, buffer_size);
			return buffer_size;
		}

		gf_mixer_lock(ar->mixer, GF_TRUE);

		if (ar->filter_chain.enable_filters) {
			char *ptr = buffer;
			written = 0;
			delay_ms += ar->filter_chain.delay_ms;

			while (buffer_size) {
				u32 to_copy;
				if (!ar->nb_used) {
					u32 nb_bytes;

					/*fill input block*/
					nb_bytes = gf_mixer_get_output(ar->mixer, ar->filter_chain.tmp_block1, ar->filter_chain.min_block_size, delay_ms);
					if (!nb_bytes)
						return written;

					/*delay used to check for late frames - we only use it on the first call to gf_mixer_get_output()*/
					delay_ms = 0;

					ar->nb_filled = gf_afc_process(&ar->filter_chain, nb_bytes);
					if (!ar->nb_filled) continue;
				}
				to_copy = ar->nb_filled - ar->nb_used;
				if (to_copy>buffer_size) to_copy = buffer_size;
				memcpy(ptr, ar->filter_chain.tmp_block1 + ar->nb_used, to_copy);
				ptr += to_copy;
				buffer_size -= to_copy;
				written += to_copy;
				ar->nb_used += to_copy;
				if (ar->nb_used==ar->nb_filled) ar->nb_used = 0;
			}
		} else {
			/*written = */gf_mixer_get_output(ar->mixer, buffer, buffer_size, delay_ms);
		}
		gf_mixer_lock(ar->mixer, GF_FALSE);

		//done with one sim step, go back in pause
		if (ar->step_mode) {
			ar->step_mode = GF_FALSE;
			gf_ar_pause(ar, GF_TRUE, GF_FALSE, GF_FALSE);
		}

		if (!ar->need_reconfig) {
			if (ar->audio_listeners) {
				u32 k=0;
				GF_AudioListener *l;
				while ((l = (GF_AudioListener*)gf_list_enum(ar->audio_listeners, &k))) {
					l->on_audio_frame(l->udta, buffer, buffer_size, gf_sc_ar_get_clock(ar), delay_ms);
				}
			}

			ar->bytes_requested += buffer_size;
			ar->current_time = ar->time_at_last_config + (u32) (ar->bytes_requested * 1000 / ar->bytes_per_second);
		}
		//always return buffer size (eg requested input size to be filled), since the clock is always increased by buffer_size (cf above line)
		return buffer_size;
	}
	return 0;
}

void gf_sc_flush_next_audio(GF_Compositor *compositor)
{
	if (!compositor->audio_renderer->audio_out)
		return;

	gf_mixer_lock(compositor->audio_renderer->mixer, GF_TRUE);
	compositor->audio_renderer->step_mode = GF_TRUE;
	//resume for one frame
	if (compositor->audio_renderer->Frozen) {
		gf_ar_pause(compositor->audio_renderer, GF_FALSE, GF_FALSE, GF_FALSE);
	}
	gf_mixer_lock(compositor->audio_renderer->mixer, GF_FALSE);
}

Bool gf_sc_check_audio_pending(GF_Compositor *compositor)
{
	Bool res = GF_FALSE;
	gf_mixer_lock(compositor->audio_renderer->mixer, GF_TRUE);
	if (compositor->audio_renderer->step_mode)
		res = GF_FALSE;
	gf_mixer_lock(compositor->audio_renderer->mixer, GF_FALSE);

	return res;
}

u32 gf_ar_proc(void *p)
{
	GF_AudioRenderer *ar = (GF_AudioRenderer *) p;

	ar->audio_th_state = 1;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[AudioRender] Entering audio thread ID %d\n", gf_th_id() ));

	gf_mixer_lock(ar->mixer, GF_TRUE);
	ar->need_reconfig = GF_TRUE;
	gf_sc_ar_reconfig(ar);
	gf_mixer_lock(ar->mixer, GF_FALSE);

	while (ar->audio_th_state == 1) {
		//do mix even if mixer is empty, otherwise we will push the same buffer over and over to the sound card
		/*
				if (ar->Frozen ) {
					gf_sleep(0);
				} else
		*/		{
			if (ar->need_reconfig) gf_sc_ar_reconfig(ar);
			ar->audio_out->WriteAudio(ar->audio_out);
		}
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_AUDIO, ("[AudioRender] Exiting audio thread\n"));
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
	ar = (GF_AudioRenderer *) gf_malloc(sizeof(GF_AudioRenderer));
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
	ar->disable_resync = (sOpt && !stricmp(sOpt, "yes")) ? GF_TRUE : GF_FALSE;
	sOpt = gf_cfg_get_key(user->config, "Audio", "DisableMultiChannel");
	ar->disable_multichannel = (sOpt && !stricmp(sOpt, "yes")) ? GF_TRUE : GF_FALSE;

	ar->mixer = gf_mixer_new(ar);
	ar->user = user;

	ar->volume = 100;
	sOpt = gf_cfg_get_key(user->config, "Audio", "Volume");
	if (!sOpt) gf_cfg_set_key(user->config, "Audio", "Volume", "100");
	else ar->volume = atoi(sOpt);
	
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
			GF_AudioOutput *raw_out = NULL;
			count = gf_modules_get_count(ar->user->modules);
			for (i=0; i<count; i++) {
				ar->audio_out = (GF_AudioOutput *) gf_modules_load_interface(ar->user->modules, i, GF_AUDIO_OUTPUT_INTERFACE);
				if (!ar->audio_out) continue;

				//in enum mode, only use raw out if everything else failed ...
				if (!stricmp(ar->audio_out->module_name, "Raw Audio Output")) {
					raw_out = ar->audio_out;
					ar->audio_out = NULL;
					continue;
				}
				GF_LOG(GF_LOG_DEBUG, GF_LOG_AUDIO, ("[AudioRender] Audio output module %s loaded\n", ar->audio_out->module_name));
				/*check that's a valid audio compositor*/
				if ((ar->audio_out->SelfThreaded && ar->audio_out->SetPriority) || ar->audio_out->WriteAudio) {
					/*remember the module we use*/
					gf_cfg_set_key(user->config, "Audio", "DriverName", ar->audio_out->module_name);
					break;
				}
				gf_modules_close_interface((GF_BaseInterface *)ar->audio_out);
				ar->audio_out = NULL;
			}
			if (raw_out) {
				if (ar->audio_out) gf_modules_close_interface((GF_BaseInterface *)raw_out);
				else ar->audio_out = raw_out;
			}
		}

		/*if not init we run with a NULL audio compositor*/
		if (ar->audio_out) {
			ar->audio_out->FillBuffer = gf_ar_fill_output;
			ar->audio_out->audio_renderer = ar;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_AUDIO, ("[AudioRender] Setting up audio module %s\n", ar->audio_out->module_name));
			e = ar->audio_out->Setup(ar->audio_out, ar->user->os_window_handler, num_buffers, total_duration);


			/*load main audio filter*/
			gf_afc_load(&ar->filter_chain, user, (char*)gf_cfg_get_key(user->config, "Audio", "Filter"));


			if (e != GF_OK) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("Could not setup audio out %s\n", ar->audio_out->module_name));
				gf_modules_close_interface((GF_BaseInterface *)ar->audio_out);
				ar->audio_out = NULL;
			} else {
				if (!ar->audio_out->SelfThreaded) {
					ar->th = gf_th_new("AudioRenderer");
					gf_th_run(ar->th, gf_ar_proc, ar);
				} else {
					gf_ar_setup_output_format(ar);
					if (ar->audio_out->SetPriority) ar->audio_out->SetPriority(ar->audio_out, GF_THREAD_PRIORITY_REALTIME);
				}
			}
		}
		if (!ar->audio_out) {
			gf_cfg_set_key(user->config, "Audio", "DriverName", "No Audio Output Available");
		} else {
			if (user->init_flags & GF_TERM_USE_AUDIO_HW_CLOCK)
				ar->clock_use_audio_out = GF_TRUE;
		}
	}
	/*init compositor timer*/
	ar->start_time = gf_sys_clock_high_res();
	ar->current_time = 0;
	return ar;
}

void gf_sc_ar_del(GF_AudioRenderer *ar)
{
	if (!ar) return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_AUDIO, ("[AudioRender] Destroying compositor\n"));
	/*resume if paused (might cause deadlock otherwise)*/
	if (ar->Frozen) gf_sc_ar_control(ar, GF_SC_AR_RESUME);
	/*stop and shutdown*/
	if (ar->audio_out) {
		/*kill audio thread*/
		if (!ar->audio_out->SelfThreaded) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_AUDIO, ("[AudioRender] stopping audio thread\n"));
			ar->audio_th_state = 2;
			while (ar->audio_th_state != 3) {
				gf_sleep(33);
			}
			GF_LOG(GF_LOG_DEBUG, GF_LOG_AUDIO, ("[AudioRender] audio thread stopped\n"));
			gf_th_del(ar->th);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_AUDIO, ("[AudioRender] audio thread destroyed\n"));
		}
		/*lock access before shutdown and emulate a reconfig (avoids mixer lock from self-threaded modules)*/
		ar->need_reconfig = GF_TRUE;
		gf_mixer_lock(ar->mixer, GF_TRUE);
		if (ar->audio_out->SelfThreaded) ar->audio_out->Shutdown(ar->audio_out);
		gf_modules_close_interface((GF_BaseInterface *)ar->audio_out);
		ar->audio_out = NULL;
		gf_mixer_lock(ar->mixer, GF_FALSE);
	}
	gf_mixer_del(ar->mixer);

	if (ar->audio_listeners) gf_list_del(ar->audio_listeners);
	gf_afc_unload(&ar->filter_chain);
	gf_free(ar);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_AUDIO, ("[AudioRender] Renderer destroyed\n"));
}


void gf_sc_ar_reset(GF_AudioRenderer *ar)
{
	gf_mixer_remove_all(ar->mixer);
}

void gf_sc_ar_control(GF_AudioRenderer *ar, u32 PauseType)
{
	gf_ar_pause(ar, (PauseType==GF_SC_AR_PAUSE) ? GF_TRUE : GF_FALSE, GF_FALSE, (PauseType==GF_SC_AR_RESET_HW_AND_PLAY) ? GF_TRUE : GF_FALSE);
}

void gf_sc_ar_set_volume(GF_AudioRenderer *ar, u32 Volume)
{
	char sOpt[10];
	gf_mixer_lock(ar->mixer, GF_TRUE);
	ar->volume = MIN(Volume, 100);
	if (ar->audio_out) ar->audio_out->SetVolume(ar->audio_out, ar->volume);
	sprintf(sOpt, "%d", ar->volume);
	gf_cfg_set_key(ar->user->config, "Audio", "Volume", sOpt);

	gf_mixer_lock(ar->mixer, GF_FALSE);
}

void gf_sc_ar_mute(GF_AudioRenderer *ar, Bool mute)
{
	gf_mixer_lock(ar->mixer, GF_TRUE);
	ar->mute = mute;
	if (ar->audio_out) ar->audio_out->SetVolume(ar->audio_out, mute ? 0 : ar->volume);
	gf_mixer_lock(ar->mixer, GF_FALSE);
}

void gf_sc_ar_set_pan(GF_AudioRenderer *ar, u32 Balance)
{
	gf_mixer_lock(ar->mixer, GF_TRUE);
	ar->pan = MIN(Balance, 100);
	if (ar->audio_out) ar->audio_out->SetPan(ar->audio_out, ar->pan);
	gf_mixer_lock(ar->mixer, GF_FALSE);
}


void gf_sc_ar_add_src(GF_AudioRenderer *ar, GF_AudioInterface *source)
{
	Bool recfg;
	if (!ar) return;
	/*lock mixer*/
	gf_mixer_lock(ar->mixer, GF_TRUE);
	gf_mixer_add_input(ar->mixer, source);
	/*if changed reconfig*/
	recfg = gf_mixer_reconfig(ar->mixer);
	if (!ar->need_reconfig) ar->need_reconfig = recfg;

	if (!gf_mixer_empty(ar->mixer) && ar->audio_out && ar->audio_out->Play)
		ar->audio_out->Play(ar->audio_out, 1);
	/*unlock mixer*/
	gf_mixer_lock(ar->mixer, GF_FALSE);
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
	Bool frozen;
	if (!ar->need_reconfig || !ar->audio_out) return;
	/*lock mixer*/
	gf_mixer_lock(ar->mixer, GF_TRUE);

	frozen = ar->Frozen;
	if (!frozen ) 
		gf_ar_pause(ar, GF_TRUE, GF_TRUE, GF_FALSE);
	
	ar->need_reconfig = GF_FALSE;
	gf_ar_setup_output_format(ar);
	
	if (!frozen)
		gf_ar_pause(ar, GF_FALSE, GF_TRUE, GF_FALSE);

	/*unlock mixer*/
	gf_mixer_lock(ar->mixer, GF_FALSE);
}

u32 gf_sc_ar_get_delay(GF_AudioRenderer *ar)
{
	return ar->audio_out->GetAudioDelay(ar->audio_out);
}


u32 gf_sc_ar_get_clock(GF_AudioRenderer *ar)
{
	if (ar->clock_use_audio_out) return ar->current_time;

	if (ar->Frozen) {
		return (u32) ((ar->freeze_time - ar->start_time) / 1000);
	}

	return (u32) ((gf_sys_clock_high_res() - ar->start_time) / 1000);
}

GF_EXPORT
void gf_sc_reload_audio_filters(GF_Compositor *compositor)
{
	GF_AudioRenderer *ar = compositor->audio_renderer;
	if (!ar) return;

	gf_mixer_lock(ar->mixer, GF_TRUE);

	gf_afc_unload(&ar->filter_chain);
	gf_afc_load(&ar->filter_chain, ar->user, (char*)gf_cfg_get_key(ar->user->config, "Audio", "Filter"));

	gf_ar_pause(ar, GF_TRUE, GF_TRUE, GF_FALSE);
	ar->need_reconfig = GF_FALSE;
	gf_ar_setup_output_format(ar);
	gf_ar_pause(ar, GF_FALSE, GF_TRUE, GF_FALSE);

	gf_mixer_lock(ar->mixer, GF_FALSE);
}

GF_EXPORT
GF_Err gf_sc_add_audio_listener(GF_Compositor *compositor, GF_AudioListener *al)
{
	GF_AudioMixer *mixer;
	u32 sr, ch, bps, ch_cfg;
	if (!compositor || !al || !al->on_audio_frame || !al->on_audio_reconfig) return GF_BAD_PARAM;
	if (!compositor->audio_renderer) return GF_NOT_SUPPORTED;

	mixer = compositor->audio_renderer->mixer;
	gf_mixer_lock(mixer, GF_TRUE);

	if (!compositor->audio_renderer->audio_listeners) compositor->audio_renderer->audio_listeners = gf_list_new();
	gf_list_add(compositor->audio_renderer->audio_listeners, al);

	gf_mixer_get_config(mixer, &sr, &ch, &bps, &ch_cfg);

	al->on_audio_reconfig(al->udta, sr, bps, ch, ch_cfg);

	gf_mixer_lock(mixer, GF_FALSE);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_sc_remove_audio_listener(GF_Compositor *compositor, GF_AudioListener *al)
{
	if (!compositor || !al) return GF_BAD_PARAM;
	if (!compositor->audio_renderer) return GF_NOT_SUPPORTED;

	gf_mixer_lock(compositor->audio_renderer->mixer, GF_TRUE);
	gf_list_del_item(compositor->audio_renderer->audio_listeners, al);
	if (!gf_list_count(compositor->audio_renderer->audio_listeners)) {
		gf_list_del(compositor->audio_renderer->audio_listeners);
		compositor->audio_renderer->audio_listeners = NULL;
	}
	gf_mixer_lock(compositor->audio_renderer->mixer, GF_FALSE);
	return GF_OK;
}
