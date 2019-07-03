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
	u32 freq, a_fmt, nb_chan, ch_cfg;
	u32 bsize;

	freq = ar->compositor->asr;
	a_fmt = ar->compositor->afmt;
	nb_chan = ar->compositor->ach;
	ch_cfg = ar->compositor->alayout;
	bsize = ar->compositor->asize;
	if (!bsize) bsize = 1024;

	if (!freq || !a_fmt || !nb_chan || !ch_cfg) {
		gf_mixer_get_config(ar->mixer, &freq, &nb_chan, &a_fmt, &ch_cfg);

		/*user disabled multichannel audio*/
		if (!ar->compositor->amc && (nb_chan>2) ) nb_chan = 2;
	} else {
		if (!ar->config_forced) ar->config_forced++;
	}


	gf_mixer_set_config(ar->mixer, freq, nb_chan, a_fmt, ch_cfg);

	if (ar->samplerate) {
		ar->time_at_last_config_sr = ar->current_time_sr * freq / ar->samplerate;
	}
	if (!ar->compositor->abuf) ar->compositor->abuf = 100;
	ar->samplerate = freq;
	ar->bytes_per_samp = nb_chan * gf_audio_fmt_bit_depth(a_fmt) / 8;
	ar->bytes_per_second = freq * ar->bytes_per_samp;
	ar->max_bytes_out = ar->bytes_per_second * ar->compositor->abuf / 1000;
	while (ar->max_bytes_out % (2*ar->bytes_per_samp) ) ar->max_bytes_out++;
	ar->buffer_size = ar->bytes_per_samp * bsize;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_AUDIO, ("[Compositor] Reconfigure audio to %d Hz %d channels %s\n", freq, nb_chan, gf_audio_fmt_name(a_fmt) ));

	if (ar->aout) {
		gf_filter_pid_set_property(ar->aout, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(freq) );
		gf_filter_pid_set_property(ar->aout, GF_PROP_PID_TIMESCALE, &PROP_UINT(freq) );
		gf_filter_pid_set_property(ar->aout, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(nb_chan) );
		gf_filter_pid_set_property(ar->aout, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(a_fmt) );
		gf_filter_pid_set_property(ar->aout, GF_PROP_PID_CHANNEL_LAYOUT, &PROP_UINT(ch_cfg) );
		gf_filter_pid_set_property(ar->aout, GF_PROP_PID_AUDIO_VOLUME, &PROP_UINT(ar->volume) );
		gf_filter_pid_set_property(ar->aout, GF_PROP_PID_AUDIO_PAN, &PROP_UINT(ar->pan) );

		gf_filter_pid_set_max_buffer(ar->aout, 1000*ar->compositor->abuf);
	}

	ar->time_at_last_config = ar->current_time;
	ar->bytes_requested = 0;

	if (ar->aout) {
		GF_FilterPacket *pck;
		//issue a dummy packet to tag the point at which we reconfigured
		pck = gf_filter_pck_new_shared(ar->aout, (u8 *) ar, 0, gf_ar_rcfg_done);
		ar->wait_for_rcfg ++;
		gf_filter_pck_set_readonly(pck);
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


GF_AudioRenderer *gf_sc_ar_load(GF_Compositor *compositor, u32 init_flags)
{
	GF_AudioRenderer *ar;
	ar = (GF_AudioRenderer *) gf_malloc(sizeof(GF_AudioRenderer));
	memset(ar, 0, sizeof(GF_AudioRenderer));

	ar->compositor = compositor;

	ar->mixer = gf_mixer_new(ar);
	ar->non_rt_output = GF_TRUE;
	ar->volume = MIN(100, compositor->avol);
	ar->pan = MIN(100, compositor->apan);
	if (! (init_flags & GF_TERM_NO_AUDIO) ) {
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
	if (Volume>100) Volume=100;
	if (ar->volume==Volume) return;
	if (ar->aout) gf_filter_pid_set_property(ar->aout, GF_PROP_PID_AUDIO_VOLUME, &PROP_UINT(ar->volume) );
}

void gf_sc_ar_mute(GF_AudioRenderer *ar, Bool mute)
{
	ar->mute = mute;
	if (ar->aout) gf_filter_pid_set_property(ar->aout, GF_PROP_PID_AUDIO_VOLUME, &PROP_UINT(mute ? 0 : ar->volume) );
}

void gf_sc_ar_set_pan(GF_AudioRenderer *ar, u32 Balance)
{
	if (Balance>100) Balance = 100;
	if (ar->pan == Balance) return;
	ar->pan = Balance;
	if (ar->aout) gf_filter_pid_set_property(ar->aout, GF_PROP_PID_AUDIO_PAN, &PROP_UINT(ar->pan) );
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


#if 0 //unused
void gf_sc_ar_set_priority(GF_AudioRenderer *ar, u32 priority)
{
	if (ar->aout)
		gf_filter_pid_set_property(ar->aout, GF_PROP_PID_AUDIO_PRIORITY, &PROP_UINT(priority) );
}
#endif

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
			ar->current_time = (u32) ( (gf_sys_clock_high_res() - ar->start_time)/1000);
			return;
		}
	}

	while (max_send) {
		u8 *data;
		u32 dur;
		u32 delay_ms = 0;
		GF_FilterPacket *pck;

		if (gf_filter_pid_would_block(ar->aout))
			break;

		pck = gf_filter_pck_new_alloc_destructor(ar->aout, ar->buffer_size, &data, gf_ar_pck_done);
		if (!pck) break;

		if (ar->compositor->async) {
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

		//this is a safety for non blocking mode, otherwise the pid_would_block is enough
		if (ar->nb_bytes_out > ar->max_bytes_out)
			break;
	}
}


void gf_sc_ar_send_or_reconfig(GF_AudioRenderer *ar)
{
	Bool frozen;
	if (ar->need_reconfig) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_AUDIO, ("[Compositor] Reconfiguring audio mixer\n"));
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
	GF_LOG(GF_LOG_DEBUG, GF_LOG_AUDIO, ("[Compositor] sending audio packets\n"));
	gf_ar_send_packets(ar);
}

#if 0 //unused
u32 gf_sc_ar_get_delay(GF_AudioRenderer *ar)
{
	if (!ar->bytes_per_second) return 0;
	//try to FIXME, this is not as precise as what we have before using ar->audio_out->GetAudioDelay(ar->audio_out)
	// since we don't know how much of the first packet data out there has been consumed
	return 1000 * ar->nb_bytes_out / ar->bytes_per_second;
}
#endif

u32 gf_sc_ar_get_clock(GF_AudioRenderer *ar)
{
	return ar->current_time;
}



