/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / audio output filter
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


#include <gpac/filters.h>
#include <gpac/constants.h>
#include <gpac/modules/audio_out.h>
#include <gpac/thread.h>

typedef struct
{
	//options
	char *drv;
	u32 bnum, bdur, threaded, priority;
	Bool clock;
	GF_Fraction64 dur;
	Double speed, start;
	u32 vol, pan, buffer, mbuffer, rbuffer;
	GF_Fraction adelay;
	
	GF_FilterPid *pid;
	u32 sr, afmt, nb_ch, timescale;
	u64 ch_cfg;
	GF_AudioOutput *audio_out;
	GF_Thread *th;
	u32 audio_th_state;
	Bool needs_recfg, wait_recfg;
	u32 bytes_per_sample;

	u32 pck_offset;
	u64 first_cts;
	u64 last_cts;
	Bool aborted;
	u32 speed_set;
	GF_Filter *filter;
	Bool is_eos, in_error;
	Bool first_write_done;

	s64 pid_delay;

	Bool buffer_done, no_buffering;
	u64 hwdelay_us, totaldelay_us;

	u64 rebuffer;
	Bool do_seek;
	u64 last_clock;
} GF_AudioOutCtx;


void aout_reconfig(GF_AudioOutCtx *ctx)
{
	u32 sr, afmt, old_afmt, nb_ch;
	u64 ch_cfg;
	GF_Err e = GF_OK;
	sr = ctx->sr;

	nb_ch = ctx->nb_ch;
	afmt = old_afmt = ctx->afmt;
	ch_cfg = ctx->ch_cfg;

	//config not ready, wait
	if (!nb_ch || !sr || !afmt) {
		//force a get_packet to trigger reconfigure
		gf_filter_pid_get_packet(ctx->pid);
		return;
	}
	//we only support packed audio at output
	if (afmt>GF_AUDIO_FMT_LAST_PACKED)
		afmt -= GF_AUDIO_FMT_LAST_PACKED;

	e = ctx->audio_out->Configure(ctx->audio_out, &sr, &nb_ch, &afmt, ch_cfg);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[AudioOut] Failed to configure audio output: %s\n", gf_error_to_string(e) ));
		afmt = GF_AUDIO_FMT_S16;
		sr = 44100;
		nb_ch = 2;
	}
	if (ctx->speed == FIX_ONE) ctx->speed_set = 1;

	if (ctx->vol<=100) {
		if (ctx->audio_out->SetVolume)
			ctx->audio_out->SetVolume(ctx->audio_out, ctx->vol);
		ctx->vol = 101;
	}
	if (ctx->pan<=100) {
		if (ctx->audio_out->SetPan)
			ctx->audio_out->SetPan(ctx->audio_out, ctx->pan);
		ctx->pan = 101;
	}


	if (ctx->sr * ctx->nb_ch * old_afmt == 0) {
		ctx->needs_recfg = GF_FALSE;
		ctx->wait_recfg = GF_FALSE;
		return;
	}

	if ((sr != ctx->sr) || (nb_ch!=ctx->nb_ch) || (afmt!=old_afmt) || !ctx->speed_set) {
		gf_filter_pid_negociate_property(ctx->pid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(sr));
		gf_filter_pid_negociate_property(ctx->pid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(afmt));
		gf_filter_pid_negociate_property(ctx->pid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(nb_ch));
		gf_filter_pid_negociate_property(ctx->pid, GF_PROP_PID_AUDIO_SPEED, &PROP_DOUBLE(ctx->speed));
		ctx->speed_set = (ctx->speed==1.0) ? 1 : 2;
		ctx->needs_recfg = GF_FALSE;
		//drop all packets until next reconfig
		ctx->wait_recfg = GF_TRUE;
		ctx->sr = sr;
		ctx->nb_ch = nb_ch;
		ctx->afmt = afmt;
		ctx->ch_cfg = ch_cfg;
	} else if (e==GF_OK) {
		ctx->needs_recfg = GF_FALSE;
		ctx->wait_recfg = GF_FALSE;
	} else {
		if (!ctx->in_error) {
			ctx->in_error = GF_TRUE;
			gf_filter_abort(ctx->filter);
		}
		return;
	}
	ctx->bytes_per_sample = gf_audio_fmt_bit_depth(afmt) * nb_ch / 8;
	ctx->hwdelay_us = 0;
	if (ctx->audio_out->GetAudioDelay) {
		ctx->hwdelay_us = ctx->audio_out->GetAudioDelay(ctx->audio_out);
		ctx->hwdelay_us *= 1000;
		GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("[AudioOut] Hardware delay is "LLU" us\n", ctx->hwdelay_us ));
	}
	ctx->totaldelay_us = 0;
	if (ctx->audio_out->GetTotalBufferTime) {
		ctx->totaldelay_us = ctx->audio_out->GetTotalBufferTime(ctx->audio_out);
		ctx->totaldelay_us *= 1000;
		GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("[AudioOut] Total audio delay is "LLU" ms\n", ctx->totaldelay_us ));
	}
}

u32 aout_th_proc(void *p)
{
	GF_AudioOutCtx *ctx = (GF_AudioOutCtx *) p;

	ctx->audio_th_state = 1;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[AudioOut] Entering audio thread ID %d\n", gf_th_id() ));

	while (ctx->audio_th_state == 1) {
		if (ctx->needs_recfg) {
			aout_reconfig(ctx);
		} else if (ctx->pid) {
			ctx->audio_out->WriteAudio(ctx->audio_out);
		} else {
			gf_sleep(10);
		}
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[AudioOut] Exiting audio thread\n"));
	ctx->audio_out->Shutdown(ctx->audio_out);
	ctx->audio_th_state = 3;
	return 0;
}


static u32 aout_fill_output(void *ptr, u8 *buffer, u32 buffer_size)
{
	u32 done = 0;
	GF_AudioOutCtx *ctx = ptr;
	Bool is_first_pck = GF_TRUE;

	memset(buffer, 0, buffer_size);
	if (!ctx->pid || ctx->aborted) return 0;
	if (!ctx->speed) return 0;

	if (ctx->do_seek) {
		GF_FilterEvent evt;

		GF_FEVT_INIT(evt, GF_FEVT_STOP, ctx->pid);
		gf_filter_pid_send_event(ctx->pid, &evt);

		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[AudioOut] Seek request to %f speed %f\n", ctx->start, ctx->speed));
		gf_filter_pid_init_play_event(ctx->pid, &evt, ctx->start, ctx->speed, "VideoOut");
		gf_filter_pid_send_event(ctx->pid, &evt);

		//reinit clock
		ctx->first_write_done = GF_FALSE;
		ctx->buffer_done = GF_FALSE;
		ctx->do_seek = GF_FALSE;
		ctx->pck_offset = 0;
		ctx->last_cts = 0;
		return 0;
	}


	if (!ctx->buffer_done) {
		u32 size;
		GF_FilterPacket *pck;

		//query full buffer duration in us
		u64 dur = gf_filter_pid_query_buffer_duration(ctx->pid, GF_FALSE);

		GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[AudioOut] buffer %d / %d ms\r", dur/1000, ctx->buffer));

		/*the compositor sends empty packets after its reconfiguration to check when the config is active
		we therefore probe the first packet before probing the buffer fullness*/
		pck = gf_filter_pid_get_packet(ctx->pid);
		if (!pck) {
			if (gf_filter_pid_is_eos(ctx->pid))
				ctx->is_eos = GF_TRUE;
			return 0;
		}

		if (! gf_filter_pck_is_blocking_ref(pck)) {
			if ((dur < ctx->buffer * 1000) && !gf_filter_pid_is_eos(ctx->pid))
				return 0;
			gf_filter_pck_get_data(pck, &size);
			if (!size) {
				gf_filter_pid_drop_packet(ctx->pid);
				return 0;
			}
			//check the decoder output is full (avoids initial underrun)
			if (gf_filter_pid_query_buffer_duration(ctx->pid, GF_TRUE)==0)
				return 0;
		}
		ctx->buffer_done = GF_TRUE;
		if (ctx->rebuffer) {
			GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[AudioOut] rebuffer done in "LLU" ms\n", (u32) ( (gf_sys_clock_high_res() - ctx->rebuffer) /1000) ));
			ctx->rebuffer = 0;
		}
	} else if (ctx->rbuffer && !ctx->rebuffer) {
		//query full buffer duration in us
		u64 dur = gf_filter_pid_query_buffer_duration(ctx->pid, GF_FALSE);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[AudioOut] buffer %d / %d ms\r", dur/1000, ctx->buffer));
		if ((dur < ctx->rbuffer * 1000) && !gf_filter_pid_has_seen_eos(ctx->pid)) {
			GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[AudioOut] buffer %u less than min threshold %u, rebuffering\n", (u32) (dur/1000), ctx->rbuffer));
			ctx->rebuffer = gf_sys_clock_high_res();
			ctx->buffer_done = GF_FALSE;
			return GF_OK;
		}
#ifndef GPAC_DISABLE_LOG
	} else if (gf_log_tool_level_on(GF_LOG_MMIO, GF_LOG_DEBUG)) {
		u64 dur = gf_filter_pid_query_buffer_duration(ctx->pid, GF_FALSE);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[AudioOut] buffer %d / %d ms\r", dur/1000, ctx->buffer));
#endif
	}

	//do not throw underflow log util first packet is fetched
	if (ctx->first_write_done)
		is_first_pck = GF_FALSE;

	while (done < buffer_size) {
		const char *data;
		u32 size;
		u64 cts;
		s64 delay;
		GF_FilterPacket *pck = gf_filter_pid_get_packet(ctx->pid);
		if (!pck) {
			if (gf_filter_pid_is_eos(ctx->pid)) {
				ctx->is_eos = GF_TRUE;
			} else if (!is_first_pck) {
				GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[AudioOut] buffer underflow\n"));
			}
			return done;
		}
		ctx->is_eos = GF_FALSE;
		if (ctx->needs_recfg) {
			return done;
		}

		delay = ctx->pid_delay;
		if (ctx->adelay.den)
			delay += gf_timestamp_rescale(ctx->adelay.num, ctx->adelay.den, ctx->timescale);

		cts = gf_filter_pck_get_cts(pck);
		if (delay >= 0) {
			cts += delay;
		} else if (cts < (u64) -delay) {
			gf_filter_pid_drop_packet(ctx->pid);
			continue;
		} else {
			cts -= (u64) -delay;
		}

		if (ctx->last_cts && (cts != GF_FILTER_NO_TS) && (cts>ctx->last_cts)) {
			u64 now = gf_sys_clock_high_res();
			u64 diff = cts - ctx->last_cts;
			//diff too high and no discontinuity, wait
			if ((diff > ctx->timescale/5) && (gf_filter_pid_get_clock_info(ctx->pid, NULL, NULL) != GF_FILTER_CLOCK_PCR_DISC) ) {
				diff = gf_timestamp_rescale(diff, ctx->timescale, 1000000);
				if (now < ctx->last_clock + diff) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[AudioOut] Frame too early by "LLU" us\n", ctx->last_clock + diff - now));
					return 0;
				}
			}
		}


		if (ctx->dur.num>0) {
			if (!ctx->first_cts) ctx->first_cts = cts+1;

			if (gf_timestamp_greater(cts - ctx->first_cts + 1, ctx->timescale, ctx->dur.num, ctx->dur.den)) {
				gf_filter_pid_drop_packet(ctx->pid);
				if (!ctx->aborted) {
					GF_FilterEvent evt;
					GF_FEVT_INIT(evt, GF_FEVT_STOP, ctx->pid);
					gf_filter_pid_send_event(ctx->pid, &evt);

					ctx->aborted = GF_TRUE;
				}
				return done;
			}
		}

		data = gf_filter_pck_get_data(pck, &size);

		if (!done && ctx->clock && data && size) {
			GF_Fraction64 timestamp;
			timestamp.num = cts;
			if (ctx->pck_offset) {
				u32 nb_samp = ctx->pck_offset/ctx->bytes_per_sample;
				if (ctx->timescale != ctx->sr) {
					nb_samp = (u32) gf_timestamp_rescale(nb_samp, ctx->sr, ctx->timescale);
				}
				timestamp.num += nb_samp;
			}

			timestamp.num -= gf_timestamp_rescale(ctx->hwdelay_us, 1000000, ctx->timescale);
			if (timestamp.num<0) timestamp.num = 0;
			timestamp.den = ctx->timescale;
			gf_filter_hint_single_clock(ctx->filter, gf_sys_clock_high_res(), timestamp);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[AudioOut] At %d ms audio frame CTS "LLU" (compensated time %g s, HW delay "LLU" us)\n", gf_sys_clock(), cts, ((Double)timestamp.num)/timestamp.den, ctx->hwdelay_us ));
		}
		
		if (data && !ctx->wait_recfg && (size >= ctx->pck_offset)) {
			u32 nb_copy;
			
			nb_copy = (size - ctx->pck_offset);
			if (nb_copy + done > buffer_size) nb_copy = buffer_size - done;
			memcpy(buffer+done, data+ctx->pck_offset, nb_copy);

			if (!done && gf_filter_reporting_enabled(ctx->filter)) {
				char szStatus[1024];
				u64 bdur = gf_filter_pid_query_buffer_duration(ctx->pid, GF_FALSE);
				sprintf(szStatus, "%d Hz %d ch %s buffer %d / %d ms", ctx->sr, ctx->nb_ch, gf_audio_fmt_name(ctx->afmt), (u32) (bdur/1000), ctx->buffer);
				gf_filter_update_status(ctx->filter, -1, szStatus);
			}

			ctx->last_cts = cts;
			ctx->last_clock = gf_sys_clock_high_res();

			done += nb_copy;
			ctx->first_write_done = GF_TRUE;
			is_first_pck = GF_FALSE;
			if (nb_copy + ctx->pck_offset < size) {
				ctx->pck_offset += nb_copy;
				return done;
			}
			ctx->last_cts += (size / ctx->bytes_per_sample) * ctx->sr;
			ctx->pck_offset = 0;
		}
		gf_filter_pid_drop_packet(ctx->pid);
	}
	return done;
}

void aout_set_priority(GF_AudioOutCtx *ctx, u32 prio)
{
	if (prio==ctx->priority) return;
	ctx->priority = prio;
	if (ctx->th) gf_th_set_priority(ctx->th, (s32) ctx->priority);
	else if (ctx->audio_out->SelfThreaded && ctx->audio_out->SetPriority)
		ctx->audio_out->SetPriority(ctx->audio_out, ctx->priority);
}

static GF_Err aout_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	GF_PropertyEntry *pe=NULL;
	u32 sr, nb_ch, afmt, timescale;
	u64 ch_cfg;
	GF_AudioOutCtx *ctx = (GF_AudioOutCtx *) gf_filter_get_udta(filter);

	if (is_remove) {
		assert(ctx->pid==pid);
		ctx->pid=NULL;
		return GF_OK;
	}
	assert(!ctx->pid || (ctx->pid==pid));

	if (!gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	sr = afmt = nb_ch = timescale = 0;
	ch_cfg = 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	if (p) timescale = p->value.uint;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLE_RATE);
	if (p) sr = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_AUDIO_FORMAT);
	if (p) afmt = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_NUM_CHANNELS);
	if (p) nb_ch = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CHANNEL_LAYOUT);
	if (p) ch_cfg = p->value.longuint;

	if (ctx->audio_out->SetVolume) {
		p = gf_filter_pid_get_info(pid, GF_PROP_PID_AUDIO_VOLUME, &pe);
		if (p) ctx->audio_out->SetVolume(ctx->audio_out, p->value.uint);
	}
	if (ctx->audio_out->SetPan) {
		p = gf_filter_pid_get_info(pid, GF_PROP_PID_AUDIO_PAN, &pe);
		if (p) ctx->audio_out->SetPan(ctx->audio_out, p->value.uint);
	}
	gf_filter_release_property(pe);
	
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_AUDIO_PRIORITY);
	if (p) aout_set_priority(ctx, p->value.uint);

	if (ctx->first_cts && (ctx->timescale != timescale)) {
		ctx->first_cts-=1;
		gf_timestamp_rescale(ctx->first_cts, ctx->timescale, timescale);
		ctx->first_cts+=1;

		gf_timestamp_rescale(ctx->last_cts, ctx->timescale, timescale);
	}
	ctx->timescale = timescale;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_PLAY_BUFFER);
	ctx->no_buffering = (p && !p->value.uint) ? GF_TRUE : GF_FALSE;
	if (ctx->no_buffering) {
		ctx->buffer_done = GF_TRUE;
		ctx->clock = GF_FALSE;
	}
	Bool buffer_req_changed = GF_FALSE;
	if (p && p->value.uint) {
		if (ctx->buffer < p->value.uint) {
			ctx->buffer = p->value.uint;
			buffer_req_changed = GF_TRUE;
		}
	}
	ctx->pid = pid;

	if ((ctx->sr!=sr) || (ctx->afmt != afmt) || (ctx->nb_ch != nb_ch)) {
		buffer_req_changed = GF_TRUE;
	}
	if (buffer_req_changed) {
		GF_FilterEvent evt;
		//set buffer reqs to bdur or 100 ms - we don't "buffer" in the filter, but this will allow dispatching
		//several input frames in the buffer (default being 1 pck / 1000 us max in buffers). Not doing so could cause
		//the session to end because input is blocked (no tasks posted) and output still holds a packet
		GF_FEVT_INIT(evt, GF_FEVT_BUFFER_REQ, pid);
		evt.buffer_req.max_buffer_us = ctx->buffer * 1000;
		if (ctx->bdur) {
			u64 b = ctx->bdur;
			b *= 1000;
			if (evt.buffer_req.max_buffer_us < b)
				evt.buffer_req.max_buffer_us = (u32) b;
		}
		//we have a max buffer, move our computed max to playout and setup max buffer
		if (ctx->mbuffer > evt.buffer_req.max_buffer_us / 1000 ) {
			evt.buffer_req.max_playout_us = evt.buffer_req.max_buffer_us;
			evt.buffer_req.max_buffer_us = ctx->mbuffer * 1000;
		}
		//more than 200ms, use regular buffer
		else if (ctx->buffer>200) {
			evt.buffer_req.max_playout_us = evt.buffer_req.max_buffer_us;
		}
		//we don't have a max buffer, set buffer requirements to PID only
		else {
			evt.buffer_req.pid_only = GF_TRUE;
		}
		gf_filter_pid_send_event(pid, &evt);
	}

	if ((ctx->sr==sr) && (ctx->afmt == afmt) && (ctx->nb_ch == nb_ch) && (ctx->ch_cfg == ch_cfg)) {
		ctx->needs_recfg = GF_FALSE;
		ctx->wait_recfg = GF_FALSE;
		return GF_OK;
	}

	//whenever change of sample rate / format / channel, force speed setup
	if ((ctx->sr!=sr) || (ctx->afmt != afmt) || (ctx->nb_ch != nb_ch)) {
		GF_FilterEvent evt;
		ctx->speed_set = 0;

		gf_filter_pid_init_play_event(pid, &evt, ctx->start, ctx->speed, "AudioOut");
		gf_filter_pid_send_event(pid, &evt);
		ctx->speed = evt.play.speed;
		ctx->start = evt.play.start_range;
	}
	ctx->sr = sr;
	ctx->afmt = afmt;
	ctx->nb_ch = nb_ch;
	ctx->ch_cfg = ch_cfg;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_AUDIO_PRIORITY);
	if (p) aout_set_priority(ctx, p->value.uint);

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DELAY);
	ctx->pid_delay = p ? p->value.longsint : 0;

	ctx->needs_recfg = GF_TRUE;
	
	//not threaded, request a task to restart audio (cannot do it during the audio callback)
	if (!ctx->th) gf_filter_post_process_task(filter);
	return GF_OK;
}

static GF_Err aout_initialize(GF_Filter *filter)
{
	const char *sOpt;
	void *os_wnd_handler;
	GF_Err e;
	GF_AudioOutCtx *ctx = (GF_AudioOutCtx *) gf_filter_get_udta(filter);

	ctx->filter = filter;


	ctx->audio_out = (GF_AudioOutput *) gf_module_load(GF_AUDIO_OUTPUT_INTERFACE, ctx->drv);
	/*if not init we run with a NULL audio compositor*/
	if (!ctx->audio_out) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[AudioOut] No audio output modules found, cannot load audio output\n"));
		return GF_IO_ERR;
	}
	if (!gf_opts_get_key("core", "audio-output")) {
		gf_opts_set_key("core", "audio-output", ctx->audio_out->module_name);
	}

	ctx->audio_out->FillBuffer = aout_fill_output;
	ctx->audio_out->audio_renderer = ctx;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[AudioOut] Setting up audio module %s\n", ctx->audio_out->module_name));

	if (!ctx->bnum || !ctx->bdur) ctx->bnum = ctx->bdur = 0;

	os_wnd_handler = NULL;
	sOpt = gf_opts_get_key("temp", "window-handle");
	if (sOpt) sscanf(sOpt, "%p", &os_wnd_handler);
	e = ctx->audio_out->Setup(ctx->audio_out, os_wnd_handler, ctx->bnum, ctx->bdur);

	if (e != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[AudioOut] Could not setup module %s\n", ctx->audio_out->module_name));
		gf_modules_close_interface((GF_BaseInterface *)ctx->audio_out);
		ctx->audio_out = NULL;
		return e;
	}
	/*only used for coverage for now*/
	if (ctx->audio_out->QueryOutputSampleRate) {
		u32 sr = 48000;
		u32 ch = 2;
		u32 bps = 16;
		ctx->audio_out->QueryOutputSampleRate(ctx->audio_out, &sr, &ch, &bps);
	}
	if (ctx->audio_out->SelfThreaded) {
	} else if (ctx->threaded) {
		ctx->th = gf_th_new("gf_aout");
		gf_th_run(ctx->th, aout_th_proc, ctx);
	}

	aout_set_priority(ctx, GF_THREAD_PRIORITY_REALTIME);

	return GF_OK;
}

static void aout_finalize(GF_Filter *filter)
{
	GF_AudioOutCtx *ctx = (GF_AudioOutCtx *) gf_filter_get_udta(filter);

	/*stop and shutdown*/
	if (ctx->audio_out) {
		/*kill audio thread*/
		if (ctx->th) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[AudioOut] stopping audio thread\n"));
			ctx->audio_th_state = 2;
			while (ctx->audio_th_state != 3) {
				gf_sleep(33);
			}
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[AudioOut] audio thread stopped\n"));
			gf_th_del(ctx->th);
		} else {
			ctx->aborted = GF_TRUE;
			ctx->audio_out->Shutdown(ctx->audio_out);
		}
		gf_modules_close_interface((GF_BaseInterface *)ctx->audio_out);
		ctx->audio_out = NULL;
	}
}

static GF_Err aout_process(GF_Filter *filter)
{
	GF_AudioOutCtx *ctx = (GF_AudioOutCtx *) gf_filter_get_udta(filter);

	if (ctx->in_error)
		return GF_IO_ERR;

	if (!ctx->th && ctx->needs_recfg) {
		aout_reconfig(ctx);
	}

	if (ctx->th || ctx->audio_out->SelfThreaded) {
		//not configured, force fetching first packet
		if (!ctx->sr && ctx->pid)
			gf_filter_pid_get_packet(ctx->pid);
		if (ctx->is_eos) return GF_EOS;
		gf_filter_ask_rt_reschedule(filter, 100000);
		return GF_OK;
	}

	ctx->audio_out->WriteAudio(ctx->audio_out);
	return GF_OK;
}

static Bool aout_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_AudioOutCtx *ctx = (GF_AudioOutCtx *) gf_filter_get_udta(filter);
	if (!ctx->audio_out) return GF_TRUE;

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if (ctx->audio_out->Play) ctx->audio_out->Play(ctx->audio_out, evt->play.hw_buffer_reset ? 2 : 1);
		break;
	case GF_FEVT_STOP:
		if (ctx->audio_out->Play) ctx->audio_out->Play(ctx->audio_out, 0);
		break;
	default:
		break;
	}
	//cancel
	return GF_TRUE;
}

GF_Err aout_update_arg(GF_Filter *filter, const char *arg_name, const GF_PropertyValue *new_val)
{
	GF_AudioOutCtx *ctx = (GF_AudioOutCtx *) gf_filter_get_udta(filter);

	if (!strcmp(arg_name, "start")) {
		if (!ctx->pid) return GF_OK;
		ctx->do_seek = GF_TRUE;
	}
	else if (!strcmp(arg_name, "speed")) {
		if (ctx->speed != new_val->value.number) {
			if (new_val->value.number==0) return GF_OK;
			if ((new_val->value.number==1) && ((ctx->speed==0) || (ctx->speed==1)))
				return GF_OK;
			if (ctx->speed_set != 2) {
				ctx->needs_recfg = GF_TRUE;
				ctx->speed_set = 0;
			} else {
				GF_FilterEvent evt;
				GF_FEVT_INIT(evt, GF_FEVT_SET_SPEED, ctx->pid)
				evt.play.speed = new_val->value.number;
				gf_filter_pid_send_event(ctx->pid, &evt);
			}
		}
	}
	return GF_OK;
}



#define OFFS(_n)	#_n, offsetof(GF_AudioOutCtx, _n)

static const GF_FilterArgs AudioOutArgs[] =
{
	{ OFFS(drv), "audio driver name", GF_PROP_NAME, NULL, NULL, 0},
	{ OFFS(bnum), "number of audio buffers (0 for auto)", GF_PROP_UINT, "2", NULL, 0},
	{ OFFS(bdur), "total duration of all buffers in ms (0 for auto)", GF_PROP_UINT, "100", NULL, 0},
	{ OFFS(threaded), "force dedicated thread creation if sound card driver is not threaded", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(dur), "only play the specified duration", GF_PROP_FRACTION, "0", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(clock), "hint audio clock for this stream", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(speed), "set playback speed. If speed is negative and start is 0, start is set to -1", GF_PROP_DOUBLE, "1.0", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(start), "set playback start offset. A negative value means percent of media duration with -1 equal to duration", GF_PROP_DOUBLE, "0.0", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(vol), "set default audio volume, as a percentage between 0 and 100", GF_PROP_UINT, "100", "0-100", GF_FS_ARG_UPDATE},
	{ OFFS(pan), "set stereo pan, as a percentage between 0 and 100, 50 being centered", GF_PROP_UINT, "50", "0-100", GF_FS_ARG_UPDATE},
	{ OFFS(buffer), "set playout buffer in ms", GF_PROP_UINT, "200", NULL, 0},
	{ OFFS(mbuffer), "set max buffer occupancy in ms. If less than buffer, use buffer", GF_PROP_UINT, "0", NULL, 0},
	{ OFFS(rbuffer), "rebuffer trigger in ms. If 0 or more than buffer, disable rebuffering", GF_PROP_UINT, "0", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(adelay), "set audio delay in sec", GF_PROP_FRACTION, "0", NULL, GF_FS_ARG_HINT_ADVANCED|GF_FS_ARG_UPDATE},
	{ OFFS(buffer_done), "buffer done indication (readonly, for user app)", GF_PROP_BOOL, NULL, NULL, GF_ARG_HINT_EXPERT},
	{ OFFS(rebuffer), "system time in us at which last rebuffer started, 0 if not rebuffering (readonly, for user app)", GF_PROP_LUINT, NULL, NULL, GF_ARG_HINT_EXPERT},
	{0}
};

static const GF_FilterCapability AudioOutCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_RAW),
	//we accept all audio formats, but will ask for input reconfiguration if sound card does not support
};


GF_FilterRegister AudioOutRegister = {
	.name = "aout",
	GF_FS_SET_DESCRIPTION("Audio output")
	GF_FS_SET_HELP("This filter writes a single uncompressed audio input PID to a sound card or other audio output device.\n"
	"\n"
	"The longer the audio buffering [-bdur]() is, the longer the audio latency will be (pause/resume). The quality of fast forward audio playback will also be degraded when using large audio buffers.\n"
	"\n"
	"If [-clock]() is set, the filter will report system time (in us) and corresponding packet CTS for other filters to use for AV sync.\n")
	.private_size = sizeof(GF_AudioOutCtx),
	.args = AudioOutArgs,
#ifdef GPAC_CONFIG_ANDROID
	//on android pin on man thread so that all events/commands come from main thread
	//we use a dedicated thread for filling the audio
	.flags = GF_FS_REG_MAIN_THREAD,
#endif
	SETCAPS(AudioOutCaps),
	.initialize = aout_initialize,
	.finalize = aout_finalize,
	.configure_pid = aout_configure_pid,
	.process = aout_process,
	.process_event = aout_process_event,
	.update_arg = aout_update_arg
};

const GF_FilterRegister *aout_register(GF_FilterSession *session)
{
	return &AudioOutRegister;
}

