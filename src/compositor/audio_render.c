/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2018
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

void gf_ar_rcfg_done(GF_Filter *filter, GF_FilterPid *pid, GF_FilterPacket *pck)
{
	u32 size;
	GF_AudioRenderer *ar = (GF_AudioRenderer *) gf_filter_pck_get_data(pck, &size);
	assert(!size);
	assert(ar->wait_for_rcfg);
	ar->wait_for_rcfg --;
	if (ar->wait_for_rcfg) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_AUDIO, ("[Compositor] Reconfigure negociation %d still pending\n", ar->wait_for_rcfg));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_AUDIO, ("[Compositor] Reconfigure negociation done\n"));
	}
}

static GF_Err gf_ar_setup_output_format(GF_AudioRenderer *ar)
{
	const char *opt;
	u32 freq, a_fmt, nb_chan, ch_cfg;
	u32 bsize;

	freq = a_fmt = nb_chan = ch_cfg = 0;
	opt = gf_cfg_get_key(ar->user->config, "Audio", "ForceFrequency");
	if (!opt) gf_cfg_set_key(ar->user->config, "Audio", "ForceFrequency", "0");
	else freq = atoi(opt);
	opt = gf_cfg_get_key(ar->user->config, "Audio", "ForceChannels");
	if (!opt) gf_cfg_set_key(ar->user->config, "Audio", "ForceChannels", "0");
	else nb_chan = atoi(opt);
	opt = gf_cfg_get_key(ar->user->config, "Audio", "ForceLayout");
	if (!opt) gf_cfg_set_key(ar->user->config, "Audio", "ForceLayout", "0");
	else {
		if (!strnicmp(opt, "0x", 2)) sscanf(opt+2, "%x", &ch_cfg);
		else sscanf(opt, "%x", &ch_cfg);
    }
	opt = gf_cfg_get_key(ar->user->config, "Audio", "ForceFMT");
	if (!opt) gf_cfg_set_key(ar->user->config, "Audio", "ForceFMT", "0");
	else {
		a_fmt = gf_audio_fmt_parse(opt);
		if (!a_fmt) a_fmt = GF_AUDIO_FMT_S16;
	}

	opt = gf_cfg_get_key(ar->user->config, "Audio", "BufferSize");
	bsize = opt ? atoi(opt) : 1024;

	if (!freq || !a_fmt || !nb_chan || !ch_cfg) {
		gf_mixer_get_config(ar->mixer, &freq, &nb_chan, &a_fmt, &ch_cfg);

		/*user disabled multichannel audio*/
		if (ar->disable_multichannel && (nb_chan>2) ) nb_chan = 2;
	} else {
		if (!ar->config_forced) ar->config_forced++;
	}


	gf_mixer_set_config(ar->mixer, freq, nb_chan, a_fmt, ch_cfg);

	if (ar->samplerate) {
		ar->time_at_last_config_sr = ar->current_time_sr * freq / ar->samplerate;
	}
	ar->samplerate = freq;
	ar->bytes_per_samp = nb_chan * gf_audio_fmt_bit_depth(a_fmt) / 8;
	ar->bytes_per_second = freq * ar->bytes_per_samp;
	ar->max_bytes_out = ar->bytes_per_second * ar->total_duration / 1000;
	while (ar->max_bytes_out % (2*ar->bytes_per_samp) ) ar->max_bytes_out++;
	ar->buffer_size = ar->bytes_per_samp * bsize;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_AUDIO, ("[Compositor] Reconfigure audio to %d Hz %d channels %s\n", freq, nb_chan, gf_audio_fmt_name(a_fmt) ));

	if (ar->aout) {
		gf_filter_pid_set_property(ar->aout, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(freq) );
		gf_filter_pid_set_property(ar->aout, GF_PROP_PID_TIMESCALE, &PROP_UINT(freq) );
		gf_filter_pid_set_property(ar->aout, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(nb_chan) );
		gf_filter_pid_set_property(ar->aout, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(a_fmt) );
		gf_filter_pid_set_property(ar->aout, GF_PROP_PID_CHANNEL_LAYOUT, &PROP_UINT(ch_cfg) );
		gf_filter_pid_set_info(ar->aout, GF_PROP_PID_AUDIO_VOLUME, &PROP_UINT(ar->volume) );
		gf_filter_pid_set_info(ar->aout, GF_PROP_PID_AUDIO_PAN, &PROP_UINT(ar->pan) );

		gf_filter_pid_set_max_buffer(ar->aout, 1000*ar->total_duration);
	}

	ar->time_at_last_config = ar->current_time;
	ar->bytes_requested = 0;
	if (ar->aout) {
		GF_FilterPacket *pck;
		//issue a dummy packet to tag the point at which we reconfigured
		pck = gf_filter_pck_new_shared(ar->aout, (u8 *) ar, 0, gf_ar_rcfg_done);
		ar->wait_for_rcfg ++;
		gf_filter_pck_send(pck);
	}
	return GF_OK;
}

static void gf_ar_pause(GF_AudioRenderer *ar, Bool DoFreeze, Bool for_reconfig, Bool reset_hw_buffer)
{
	GF_FilterEvent evt;
	gf_mixer_lock(ar->mixer, GF_TRUE);
	if (DoFreeze) {
		if (!ar->Frozen) {
			ar->freeze_time = gf_sys_clock_high_res();
			if (!for_reconfig && ar->aout) {
				GF_FEVT_INIT(evt, GF_FEVT_STOP, ar->aout);
				gf_filter_pid_send_event(ar->aout, &evt);
			}
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SYNC, ("[Audio] pausing master clock - time "LLD" (sys time "LLD")\n", ar->freeze_time, gf_sys_clock_high_res()));
			ar->Frozen = GF_TRUE;
		}
	} else {
		if (ar->Frozen) {
			if (!for_reconfig && ar->aout) {
				GF_FEVT_INIT(evt, GF_FEVT_PLAY, ar->aout);
				evt.play.hw_buffer_reset = reset_hw_buffer ? 1 : 0;
				gf_filter_pid_send_event(ar->aout, &evt);
			}

			ar->start_time += gf_sys_clock_high_res() - ar->freeze_time;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SYNC, ("[Audio] resuming master clock - new time "LLD" (sys time "LLD") \n", ar->start_time, gf_sys_clock_high_res()));
			ar->Frozen = GF_FALSE;
		}
	}
	gf_mixer_lock(ar->mixer, GF_FALSE);
}


GF_AudioRenderer *gf_sc_ar_load(GF_User *user)
{
	const char *sOpt;
	u32 total_duration;
	GF_AudioRenderer *ar;
	ar = (GF_AudioRenderer *) gf_malloc(sizeof(GF_AudioRenderer));
	memset(ar, 0, sizeof(GF_AudioRenderer));

	total_duration = 200;
	sOpt = gf_cfg_get_key(user->config, "Audio", "ForceConfig");
	if (sOpt && !stricmp(sOpt, "yes")) {
		sOpt = gf_cfg_get_key(user->config, "Audio", "TotalDuration");
		total_duration = sOpt ? atoi(sOpt) : 400;
	}

	sOpt = gf_cfg_get_key(user->config, "Audio", "NoResync");
	ar->disable_resync = (sOpt && !stricmp(sOpt, "yes")) ? GF_TRUE : GF_FALSE;
	sOpt = gf_cfg_get_key(user->config, "Audio", "DisableMultiChannel");
	ar->disable_multichannel = (sOpt && !stricmp(sOpt, "yes")) ? GF_TRUE : GF_FALSE;

	ar->mixer = gf_mixer_new(ar);
	ar->user = user;
	ar->non_rt_output = GF_TRUE;

	ar->volume = 100;
	sOpt = gf_cfg_get_key(user->config, "Audio", "Volume");
	if (!sOpt) gf_cfg_set_key(user->config, "Audio", "Volume", "100");
	else ar->volume = atoi(sOpt);
	if (ar->volume >= 98) ar->volume = 100;

	sOpt = gf_cfg_get_key(user->config, "Audio", "Pan");
	ar->pan = sOpt ? atoi(sOpt) : 50;


	if (! (user->init_flags & GF_TERM_NO_AUDIO) ) {
		ar->total_duration = total_duration;
		gf_ar_setup_output_format(ar);
	}

	ar->current_time = 0;
	return ar;
}

void gf_sc_ar_del(GF_AudioRenderer *ar)
{
	if (!ar) return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_AUDIO, ("[Compositor] Destroying compositor\n"));
	/*resume if paused (might cause deadlock otherwise)*/
	if (ar->Frozen) gf_sc_ar_control(ar, GF_SC_AR_RESUME);

	gf_mixer_del(ar->mixer);
	gf_free(ar);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_AUDIO, ("[Compositor] Renderer destroyed\n"));
}


void gf_sc_ar_reset(GF_AudioRenderer *ar)
{
	gf_mixer_remove_all(ar->mixer);
	if (ar->scene_ready) {
		ar->scene_ready = 0;
		ar->current_time = 0;
		ar->current_time_sr = 0;
		ar->bytes_requested = 0;
		ar->nb_audio_objects = 0;
	}
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
	if (ar->aout) gf_filter_pid_set_info(ar->aout, GF_PROP_PID_AUDIO_VOLUME, &PROP_UINT(ar->volume) );

	sprintf(sOpt, "%d", ar->volume);
	gf_cfg_set_key(ar->user->config, "Audio", "Volume", sOpt);

	gf_mixer_lock(ar->mixer, GF_FALSE);
}

void gf_sc_ar_mute(GF_AudioRenderer *ar, Bool mute)
{
	gf_mixer_lock(ar->mixer, GF_TRUE);
	ar->mute = mute;
	if (ar->aout) gf_filter_pid_set_info(ar->aout, GF_PROP_PID_AUDIO_VOLUME, &PROP_UINT(mute ? 0 : ar->volume) );
	gf_mixer_lock(ar->mixer, GF_FALSE);
}

void gf_sc_ar_set_pan(GF_AudioRenderer *ar, u32 Balance)
{
	gf_mixer_lock(ar->mixer, GF_TRUE);
	ar->pan = MIN(Balance, 100);
	if (ar->aout) gf_filter_pid_set_info(ar->aout, GF_PROP_PID_AUDIO_PAN, &PROP_UINT(ar->pan) );
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

	if (!gf_mixer_empty(ar->mixer) && ar->aout) {
		GF_FilterEvent evt;
		GF_FEVT_INIT(evt, GF_FEVT_PLAY, ar->aout);
		gf_filter_pid_send_event(ar->aout, &evt);
	}

	/*unlock mixer*/
	gf_mixer_lock(ar->mixer, GF_FALSE);
}

void gf_sc_ar_remove_src(GF_AudioRenderer *ar, GF_AudioInterface *source)
{
	if (ar) {
		gf_mixer_remove_input(ar->mixer, source);
		if (gf_mixer_empty(ar->mixer) && ar->aout) {
			GF_FilterEvent evt;
			GF_FEVT_INIT(evt, GF_FEVT_STOP, ar->aout);
			gf_filter_pid_send_event(ar->aout, &evt);
		}
	}
}


void gf_sc_ar_set_priority(GF_AudioRenderer *ar, u32 priority)
{
	if (ar->aout)
		gf_filter_pid_set_info(ar->aout, GF_PROP_PID_AUDIO_PRIORITY, &PROP_UINT(priority) );
}

void gf_sc_ar_update_video_clock(GF_AudioRenderer *ar, u32 video_ts)
{
	ar->video_ts = video_ts;
}

static void gf_ar_pck_done(GF_Filter *filter, GF_FilterPid *pid, GF_FilterPacket *pck)
{
	u32 size;
	GF_Compositor *compositor = gf_filter_pid_get_udta(pid);
	/*data = */gf_filter_pck_get_data(pck, &size);
	if (!size) return;
	
	assert(size <= compositor->audio_renderer->nb_bytes_out);
	compositor->audio_renderer->nb_bytes_out -= size;
}

void gf_ar_send_packets(GF_AudioRenderer *ar)
{
	u32 written, max_send=100;
	if (!ar->aout) return;
	if (!ar->scene_ready) return;
	if (ar->need_reconfig) return;
	if (ar->Frozen) return;

	//reconfiguration is pending, wait for the packet issued at reconfig to be consummed before issuing any new frame
	if (ar->wait_for_rcfg) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_AUDIO, ("[Compositor] Waiting for audio output reconfiguration\n"));
		return;
	}
	if (ar->scene_ready) {
		if (!ar->start_time) {
			ar->start_time = gf_sys_clock_high_res();
		}
		if (!ar->nb_audio_objects && !ar->non_rt_output) {
			ar->current_time = (gf_sys_clock_high_res() - ar->start_time)/1000;
			return;
		}
	}

	while (max_send && (ar->nb_bytes_out < ar->max_bytes_out)) {
		char *data;
		u32 dur;
		u32 delay_ms = 0;
		GF_FilterPacket *pck;

		//FIXME - find a better regulation algo if needed, this breaks audio-only playback
#if 0
		//if audio output is too ahead of video time, don't push packets
		if (ar->current_time > 5*ar->total_duration + ar->video_ts)
			break;
#endif

		pck = gf_filter_pck_new_alloc_destructor(ar->aout, ar->buffer_size, &data, gf_ar_pck_done);
		if (!pck) break;

		if (!ar->disable_resync) {
			delay_ms = (1000*ar->nb_bytes_out) / ar->bytes_per_second;
		}

		gf_mixer_lock(ar->mixer, GF_TRUE);
		written = gf_mixer_get_output(ar->mixer, data, ar->buffer_size, delay_ms);
		gf_mixer_lock(ar->mixer, GF_FALSE);

		if (!written) {
			if (!ar->non_rt_output) written = ar->buffer_size;
			else if (ar->scene_ready && ar->nb_audio_objects && !gf_mixer_buffering(ar->mixer) ) written = ar->buffer_size;
			else {
				gf_filter_pck_truncate(pck, 0);
				gf_filter_pck_discard(pck);
				break;
			}
		}

		if (written<ar->buffer_size) {
			gf_filter_pck_truncate(pck, written);
		}
		gf_filter_pck_set_sap(pck, GF_FILTER_SAP_1);
		gf_filter_pck_set_cts(pck, ar->current_time_sr);
		dur = written / ar->bytes_per_samp;
		gf_filter_pck_set_duration(pck, dur);
		GF_LOG(GF_LOG_INFO, GF_LOG_AUDIO, ("[Compositor] Send audio frame TS "LLU" nb samples %d - AR clock %u\n", ar->current_time_sr, dur, ar->current_time));

		ar->nb_bytes_out += written;
		gf_filter_pck_send(pck);

		ar->bytes_requested += written;
		ar->current_time_sr = ar->time_at_last_config_sr + (u32) (ar->bytes_requested / ar->bytes_per_samp);
		ar->current_time = ar->time_at_last_config + (u32) (ar->bytes_requested * 1000 / ar->bytes_per_second);

		max_send--;
	}
}


void gf_sc_ar_reconfig(GF_AudioRenderer *ar)
{
	Bool frozen;
	if (ar->need_reconfig) {
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
	gf_ar_send_packets(ar);
}

u32 gf_sc_ar_get_delay(GF_AudioRenderer *ar)
{
	if (!ar->bytes_per_second) return 0;
	//try to FIXME, this is not as precise as what we have before using ar->audio_out->GetAudioDelay(ar->audio_out)
	// since we don't know how much of the first packet data out there has been consumed
	return 1000 * ar->nb_bytes_out / ar->bytes_per_second;
}


u32 gf_sc_ar_get_clock(GF_AudioRenderer *ar)
{
	return ar->current_time;
}



