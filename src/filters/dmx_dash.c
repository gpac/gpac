/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017
 *					All rights reserved
 *
 *  This file is part of GPAC / DASH/HLS demux filter
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

#ifndef GPAC_DISABLE_DASH_CLIENT

#include <gpac/dash.h>

enum {
	GF_GPAC_DOWNLOAD_SESSION = GF_4CC('G','H','T','T'),
};

typedef struct
{
	//opts
	s32 shift_utc, debug_as, atsc_shift;
	u32 max_buffer, auto_switch, timeshift, tiles_rate, store, delay40X, exp_threshold, switch_count;
	Bool server_utc, screen_res, aggressive, speed;
	GF_DASHInitialSelectionMode start_with;
	GF_DASHTileAdaptationMode tile_mode;
	GF_DASHAdaptationAlgorithm algo;
	Bool max_res, immediate, abort, use_bmin;

	GF_FilterPid *mpd_pid;
	GF_Filter *filter;

	GF_DashClient *dash;
	//http io for manifest
	GF_DASHFileIO dash_io;
	GF_DownloadManager *dm;
	
	Bool closed;
	Bool reuse_download_session;

	Bool initial_setup_done;
	u32 nb_playing;

	/*max width & height in all active representations*/
	u32 width, height;

	Double seek_request;
	Double media_start_range;

	Bool mpd_open;
} GF_DASHDmxCtx;

typedef struct
{
	GF_DASHDmxCtx *ctx;
	GF_Filter *seg_filter_src;

	u32 idx;
	Bool init_switch_seg_sent;
	Bool segment_sent;

	u32 nb_eos, nb_pids;
	Bool stats_uploaded;
	Bool wait_for_pck;
	Bool eos_detected;

	GF_DownloadSession *sess;
	Bool is_timestamp_based, pto_setup;
	Bool prev_is_init_segment;
	u32 timescale;
	s64 pto;
	s64 max_cts_in_period;
	bin128 key_IV;
} GF_DASHGroup;


void dashdmx_forward_packet(GF_DASHDmxCtx *ctx, GF_FilterPacket *in_pck, GF_FilterPid *in_pid, GF_FilterPid *out_pid, GF_DASHGroup *group)
{
	GF_FilterPacket *dst_pck;
	Bool do_map_time = GF_FALSE;
	Bool seek_flag = 0;
	u64 cts, dts;

	if (gf_dash_is_m3u8(ctx->dash)) {
		gf_filter_pck_forward(in_pck, out_pid);
		gf_filter_pid_drop_packet(in_pid);

		if (!group->pto_setup) {
			cts = gf_filter_pck_get_cts(in_pck);
			gf_filter_pid_set_info_str(out_pid, "time:timestamp", &PROP_LONGUINT(cts) );
			gf_filter_pid_set_info_str(out_pid, "time:media", &PROP_DOUBLE(ctx->media_start_range) );
			group->pto_setup = GF_TRUE;
		}
		return;
	}

	//filter any packet outside the current period
	dts = gf_filter_pck_get_dts(in_pck);
	cts = gf_filter_pck_get_cts(in_pck);
	seek_flag = gf_filter_pck_get_seek_flag(in_pck);

	//if sync is based on timestamps do not adjust the timestamps back
	if (! group->is_timestamp_based) {
		if (!group->pto_setup) {
			Double scale;
			s64 start, dur;
			u64 pto;
			u32 ts = gf_filter_pck_get_timescale(in_pck);
			gf_dash_group_get_presentation_time_offset(ctx->dash, group->idx, &pto, &group->timescale);
			group->pto = (s64) pto;
			group->pto_setup = 1;

			if (group->timescale && (group->timescale != ts)) {
				group->pto *= ts;
				group->pto /= group->timescale;
			}
			scale = ts;
			scale /= 1000;

			dur = (u64) (scale * gf_dash_get_period_duration(ctx->dash));
			if (dur) {
				group->max_cts_in_period = group->pto + dur;
			} else {
				group->max_cts_in_period = 0;
			}

			start = (u64) (scale * gf_dash_get_period_start(ctx->dash));
			group->pto -= start;
		}

		if (group->max_cts_in_period && (s64) cts > group->max_cts_in_period) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Packet timestamp "LLU" larger than max CTS in period "LLU" - skipping\n", cts, group->max_cts_in_period));
//			return;
		}

		//remap timestamps to our timeline
		if (dts != GF_FILTER_NO_TS) {
			if ((s64) dts >= group->pto)
				dts -= group->pto;
			else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Packet DTS "LLU" less than PTO "LLU" - forcing DTS to 0\n", dts, group->pto));
				dts = 0;
				seek_flag = 1;
			}
		}
		if (cts!=GF_FILTER_NO_TS) {
			if ((s64) cts >= group->pto)
				cts -= group->pto;
			else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Packet CTS "LLU" less than PTO "LLU" - forcing CTS to 0\n", cts, group->pto));
				cts = 0;
				seek_flag = 1;
			}
		}
	} else if (!group->pto_setup) {
		do_map_time = 1;
		group->pto_setup = 1;
	}

	dst_pck = gf_filter_pck_new_ref(out_pid, NULL, 0, in_pck);
	//this will copy over clock info for PCR in TS
	gf_filter_pck_merge_properties(in_pck, dst_pck);
	gf_filter_pck_set_dts(dst_pck, dts);
	gf_filter_pck_set_cts(dst_pck, cts);
	gf_filter_pck_set_seek_flag(dst_pck, seek_flag);
	gf_filter_pck_send(dst_pck);
	gf_filter_pid_drop_packet(in_pid);

	if (do_map_time) {
		gf_filter_pid_set_info_str(out_pid, "time:timestamp", &PROP_LONGUINT(cts) );
		gf_filter_pid_set_info_str(out_pid, "time:media", &PROP_DOUBLE(ctx->media_start_range) );
	}
}


static void dashdmx_on_filter_setup_error(GF_Filter *failed_filter, void *udta, GF_Err err)
{
	GF_DASHGroup *group = (GF_DASHGroup *)udta;
	gf_dash_set_group_download_state(group->ctx->dash, group->idx, err);
	if (err) {
		Bool group_done=GF_FALSE;
		gf_filter_post_process_task(group->ctx->filter);
		gf_dash_group_get_num_segments_ready(group->ctx->dash, group->idx, &group_done);
		if (group_done) {
			group->eos_detected = GF_TRUE;
			gf_filter_post_process_task(group->ctx->filter);
		}
	}
}

/*locates input service (demuxer) based on mime type or segment name*/
static GF_Err dashdmx_load_source(GF_DASHDmxCtx *ctx, u32 group_index, const char *mime, const char *init_segment_name, u64 start_range, u64 end_range)
{
	GF_DASHGroup *group;
	GF_Err e;
	char *sURL = NULL;
	if (!init_segment_name) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASHDmx] Error locating plugin for segment - mime type %s - name %s\n", mime, init_segment_name));
		return GF_FILTER_NOT_FOUND;
	}

	GF_SAFEALLOC(group, GF_DASHGroup);
	if (!group) return GF_OUT_OF_MEM;
	group->ctx = ctx;
	group->idx = group_index;
	gf_dash_set_group_udta(ctx->dash, group_index, group);

	sURL = gf_malloc(sizeof(char) * (strlen(init_segment_name) + 200) );
	strcpy(sURL, init_segment_name);
	if (!ctx->store) strcat(sURL, ":cache=mem");
	else if (ctx->store==2) strcat(sURL, ":cache=keep");

	if (start_range || end_range) {
		char szRange[500];
		snprintf(szRange, 500, ":range="LLU"-"LLU, start_range, end_range);
		strcat(sURL, szRange);
	}

	group->seg_filter_src = gf_filter_connect_source(ctx->filter, sURL, NULL, &e);
	if (!group->seg_filter_src) {
		gf_free(sURL);
		gf_free(group);
		return e;
	}
	gf_filter_set_setup_failure_callback(ctx->filter, group->seg_filter_src, dashdmx_on_filter_setup_error, group);
	gf_dash_group_discard_segment(ctx->dash, group->idx);
	group->prev_is_init_segment = GF_TRUE;
	gf_free(sURL);
	return GF_OK;
}

void dashdmx_io_delete_cache_file(GF_DASHFileIO *dashio, GF_DASHFileIOSession session, const char *cache_url)
{
	if (!session) {
		return;
	}
	gf_dm_delete_cached_file_entry_session((GF_DownloadSession *)session, cache_url);
}

GF_DASHFileIOSession dashdmx_io_create(GF_DASHFileIO *dashio, Bool persistent, const char *url, s32 group_idx)
{
	GF_DownloadSession *sess;
	const GF_PropertyValue *p;
	GF_Err e;
	u32 flags = GF_NETIO_SESSION_NOT_THREADED;
	GF_DASHDmxCtx *ctx = (GF_DASHDmxCtx *)dashio->udta;

	//we work in non-threaded mode, only MPD fetcher is allowed
	if (group_idx>=0)
		return NULL;

	//crude hack when using gpac downloader to initialize the MPD pid: get the pointer to the download session
	//this should be safe unless the mpd_pid is destroyed, which should only happen upon destruction of the DASH session
	p = gf_filter_pid_get_property(ctx->mpd_pid, GF_GPAC_DOWNLOAD_SESSION);
	if (p) {
		GF_DownloadSession *sess = (GF_DownloadSession *) p->value.ptr;
		if (!ctx->store) {
			gf_dm_sess_force_memory_mode(sess);
		}
		ctx->reuse_download_session = GF_TRUE;
		return (GF_DASHFileIOSession) sess;
	}

	if (!ctx->store) flags |= GF_NETIO_SESSION_MEMORY_CACHE;
	if (persistent) flags |= GF_NETIO_SESSION_PERSISTENT;
	sess = gf_dm_sess_new(ctx->dm, url, flags, NULL, NULL, &e);
	return (GF_DASHFileIOSession) sess;
}
void dashdmx_io_del(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	GF_DASHDmxCtx *ctx = (GF_DASHDmxCtx *)dashio->udta;
	if (!ctx->reuse_download_session)
		gf_dm_sess_del((GF_DownloadSession *)session);
}
void dashdmx_io_abort(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	gf_dm_sess_abort((GF_DownloadSession *)session);
}
GF_Err dashdmx_io_setup_from_url(GF_DASHFileIO *dashio, GF_DASHFileIOSession session, const char *url, s32 group_idx)
{
	return gf_dm_sess_setup_from_url((GF_DownloadSession *)session, url, GF_FALSE);
}
GF_Err dashdmx_io_set_range(GF_DASHFileIO *dashio, GF_DASHFileIOSession session, u64 start_range, u64 end_range, Bool discontinue_cache)
{
	return gf_dm_sess_set_range((GF_DownloadSession *)session, start_range, end_range, discontinue_cache);
}
GF_Err dashdmx_io_init(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	return gf_dm_sess_process_headers((GF_DownloadSession *)session);
}
GF_Err dashdmx_io_run(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	return gf_dm_sess_process((GF_DownloadSession *)session);
}
const char *dashdmx_io_get_url(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	return gf_dm_sess_get_resource_name((GF_DownloadSession *)session);
}
const char *dashdmx_io_get_cache_name(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	return gf_dm_sess_get_cache_name((GF_DownloadSession *)session);
}
const char *dashdmx_io_get_mime(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	return gf_dm_sess_mime_type((GF_DownloadSession *)session);
}
const char *dashdmx_io_get_header_value(GF_DASHFileIO *dashio, GF_DASHFileIOSession session, const char *header_name)
{
	return gf_dm_sess_get_header((GF_DownloadSession *)session, header_name);
}
u64 dashdmx_io_get_utc_start_time(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	return gf_dm_sess_get_utc_start((GF_DownloadSession *)session);
}



u32 dashdmx_io_get_bytes_per_sec(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	u32 bps=0;
//	GF_DownloadSession *sess = (GF_DownloadSession *)session;
	if (session) {
		gf_dm_sess_get_stats((GF_DownloadSession *)session, NULL, NULL, NULL, NULL, &bps, NULL);
	} else {
		GF_DASHDmxCtx *ctx = (GF_DASHDmxCtx *)dashio->udta;
		bps = gf_dm_get_data_rate(ctx->dm);
		bps/=8;
	}
	//GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("DEBUG. 2. max max max  %d \n", bps*8));
	return bps;
}
u32 dashdmx_io_get_total_size(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	u32 size=0;
//	GF_DownloadSession *sess = (GF_DownloadSession *)session;
	gf_dm_sess_get_stats((GF_DownloadSession *)session, NULL, NULL, &size, NULL, NULL, NULL);
	return size;
}
u32 dashdmx_io_get_bytes_done(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	u32 size=0;
//	GF_DownloadSession *sess = (GF_DownloadSession *)session;
	gf_dm_sess_get_stats((GF_DownloadSession *)session, NULL, NULL, NULL, &size, NULL, NULL);
	return size;
}

GF_Err dashdmx_io_on_dash_event(GF_DASHFileIO *dashio, GF_DASHEventType dash_evt, s32 group_idx, GF_Err error_code)
{
	GF_Err e;
	u32 i;
	GF_DASHDmxCtx *ctx = (GF_DASHDmxCtx *)dashio->udta;

	if (dash_evt==GF_DASH_EVENT_PERIOD_SETUP_ERROR) {
		if (!ctx->initial_setup_done) {
			gf_filter_setup_failure(ctx->filter, error_code);
			ctx->initial_setup_done= GF_TRUE;
		}
		return GF_OK;
	}

	if (dash_evt==GF_DASH_EVENT_SELECT_GROUPS) {
		//let the player decide which group to play: we declare everything
		return GF_OK;
	}

	/*for all selected groups, create input service and connect to init/first segment*/
	if (dash_evt==GF_DASH_EVENT_CREATE_PLAYBACK) {
		/*select input services if possible*/
		for (i=0; i<gf_dash_get_group_count(ctx->dash); i++) {
			const char *mime, *init_segment;
			u64 start_range, end_range;
			u32 j;
			Bool playable = GF_TRUE;
			//let the player decide which group to play
			if (!gf_dash_is_group_selectable(ctx->dash, i))
				continue;

			j=0;
			while (1) {
				const char *desc_id, *desc_scheme, *desc_value;
				if (! gf_dash_group_enum_descriptor(ctx->dash, i, GF_MPD_DESC_ESSENTIAL_PROPERTIES, j, &desc_id, &desc_scheme, &desc_value))
					break;
				j++;
				if (!strcmp(desc_scheme, "urn:mpeg:dash:srd:2014")) {
				} else {
					playable = GF_FALSE;
					break;
				}
			}
			if (!playable) {
				gf_dash_group_select(ctx->dash, i, GF_FALSE);
				continue;
			}

			if (gf_dash_group_has_dependent_group(ctx->dash, i) >=0 ) {
				gf_dash_group_select(ctx->dash, i, GF_TRUE);
				continue;
			}

			mime = gf_dash_group_get_segment_mime(ctx->dash, i);
			init_segment = gf_dash_group_get_segment_init_url(ctx->dash, i, &start_range, &end_range);

			e = dashdmx_load_source(ctx, i, mime, init_segment, start_range, end_range);
			if (e != GF_OK) {
				gf_dash_group_select(ctx->dash, i, GF_FALSE);
			} else {
				u32 w, h;
				/*connect our media service*/
				gf_dash_group_get_video_info(ctx->dash, i, &w, &h);
				if (w && h && w>ctx->width && h>ctx->height) {
					ctx->width = w;
					ctx->height = h;
				}
				if (gf_dash_group_get_srd_max_size_info(ctx->dash, i, &w, &h)) {
					ctx->width = w;
					ctx->height = h;
				}
				if (ctx->closed) return GF_OK;
			}
		}

		if (!ctx->initial_setup_done) {
			ctx->initial_setup_done = GF_TRUE;
		}

		//we had a seek outside of the period we were setting up, during period setup !
		//request the seek again from the player
		if (ctx->seek_request>=0) {
#ifdef FILTER_FIXME
			GF_NetworkCommand com;
			memset(&com, 0, sizeof(GF_NetworkCommand));
			com.command_type = GF_NET_SERVICE_SEEK;
			com.play.start_range = ctx->seek_request;
			ctx->seek_request = 0;
			gf_service_command(ctx->service, &com, GF_OK);
#endif

		}
		return GF_OK;
	}

	/*for all running services, stop service*/
	if (dash_evt==GF_DASH_EVENT_DESTROY_PLAYBACK) {
		for (i=0; i<gf_dash_get_group_count(ctx->dash); i++) {
			GF_DASHGroup *group = gf_dash_get_group_udta(ctx->dash, i);
			if (!group) continue;
			if (group->seg_filter_src) {
				gf_filter_remove(group->seg_filter_src, ctx->filter);
				group->seg_filter_src = NULL;
			}
			gf_free(group);
			gf_dash_set_group_udta(ctx->dash, i, NULL);
		}
		return GF_OK;
	}
	if (dash_evt==GF_DASH_EVENT_ABORT_DOWNLOAD) {
		GF_DASHGroup *group = gf_dash_get_group_udta(ctx->dash, group_idx);
		if (group) {
			GF_FilterEvent evt;
			GF_FEVT_INIT(evt, GF_FEVT_SOURCE_SWITCH,  NULL);
			evt.seek.start_offset = -1;
			evt.seek.source_switch = NULL;
			gf_filter_send_event(group->seg_filter_src, &evt);
		}
		return GF_OK;
	}

	if (dash_evt==GF_DASH_EVENT_BUFFERING) {
		u32 tot, done;
		gf_dash_get_buffer_info(ctx->dash, &tot, &done);
		return GF_OK;
	}
	if (dash_evt==GF_DASH_EVENT_QUALITY_SWITCH) {
		if (group_idx>=0) {
			GF_DASHGroup *group = gf_dash_get_group_udta(ctx->dash, group_idx);
			if (!group) {
				group_idx = gf_dash_group_has_dependent_group(ctx->dash, group_idx);
				group = gf_dash_get_group_udta(ctx->dash, group_idx);
			}
			if (group) {
#ifdef FILTER_FIXME
				GF_NetworkCommand com;
				memset(&com, 0, sizeof(GF_NetworkCommand));

				com.command_type = GF_NET_SERVICE_EVENT;
				com.send_event.evt.type = GF_EVENT_QUALITY_SWITCHED;
				gf_service_command(ctx->service, &com, GF_OK);
#endif

			}
		}
		return GF_OK;
	}
#ifdef FILTER_FIXME
	if (dash_evt==GF_DASH_EVENT_TIMESHIFT_OVERFLOW) {
		GF_NetworkCommand com;
		com.command_type = GF_NET_SERVICE_EVENT;
		com.send_event.evt.type = (group_idx>=0) ? GF_EVENT_TIMESHIFT_OVERFLOW : GF_EVENT_TIMESHIFT_UNDERRUN;
		gf_service_command(ctx->service, &com, GF_OK);
	}
	if (dash_evt==GF_DASH_EVENT_TIMESHIFT_UPDATE) {
		GF_NetworkCommand com;
		com.command_type = GF_NET_SERVICE_EVENT;
		com.send_event.evt.type = GF_EVENT_TIMESHIFT_UPDATE;
		gf_service_command(ctx->service, &com, GF_OK);
	}

	if (dash_evt==GF_DASH_EVENT_CODEC_STAT_QUERY) {
		GF_NetworkCommand com;
		memset(&com, 0, sizeof(GF_NetworkCommand));
		com.command_type = GF_NET_SERVICE_CODEC_STAT_QUERY;
		gf_service_command(ctx->service, &com, GF_OK);
		gf_dash_group_set_codec_stat(ctx->dash, group_idx, com.codec_stat.avg_dec_time, com.codec_stat.max_dec_time, com.codec_stat.irap_avg_dec_time, com.codec_stat.irap_max_dec_time, com.codec_stat.codec_reset, com.codec_stat.decode_only_rap);

		memset(&com, 0, sizeof(GF_NetworkCommand));
		com.command_type = GF_NET_BUFFER_QUERY;
		gf_service_command(ctx->service, &com, GF_OK);
		gf_dash_group_set_buffer_levels(ctx->dash, group_idx, com.buffer.min, com.buffer.max, com.buffer.occupancy);
	}
#endif
	return GF_OK;
}

static void dashdmx_setup_buffer(GF_DASHDmxCtx *ctx, GF_DASHGroup *group)
{
	u32 buffer_ms, play_buf_ms;
	gf_filter_get_buffer_max(ctx->filter, &buffer_ms, &play_buf_ms);
	buffer_ms /= 1000;
	play_buf_ms /= 1000;

	//use min buffer from MPD
	if (ctx->use_bmin) {
		u64 mpd_buffer_ms = gf_dash_get_min_buffer_time(ctx->dash);
		if (mpd_buffer_ms > buffer_ms)
			buffer_ms = mpd_buffer_ms;
	}
	if (buffer_ms) {
		gf_dash_set_user_buffer(ctx->dash, buffer_ms);
	}
	gf_dash_group_set_max_buffer_playout(ctx->dash, group->idx, play_buf_ms);
}

/*check in all groups if the service can support reverse playback (speed<0); return GF_OK only if service is supported in all groups*/
static Bool dashdmx_dash_can_reverse_playback(GF_DASHDmxCtx *ctx)
{
	u32 i, count = gf_filter_get_ipid_count(ctx->filter);

	for (i=0; i<count; i++) {
		const GF_PropertyValue *p;
		GF_FilterPid *pid = gf_filter_get_ipid(ctx->filter, i);
		if (ctx->mpd_pid == pid) continue;

		p = gf_filter_pid_get_info(pid, GF_PROP_PID_REVERSE_PLAYBACK);
		if (!p || !p->value.boolean) return GF_FALSE;
	}
	return GF_TRUE;
}


static s32 dashdmx_group_idx_from_pid(GF_DASHDmxCtx *ctx, GF_FilterPid *src_pid)
{
	s32 i;

	for (i=0; (u32) i < gf_dash_get_group_count(ctx->dash); i++) {
		GF_DASHGroup *group = gf_dash_get_group_udta(ctx->dash, i);
		if (!group) continue;
		if (gf_filter_pid_is_filter_in_parents(src_pid, group->seg_filter_src))
			return i;
	}
	return -1;
}

static GF_Err dashdmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	s32 group_idx;
	GF_FilterPid *opid;
	GF_Err e=GF_OK;
	const GF_PropertyValue *p;
	GF_DASHDmxCtx *ctx = (GF_DASHDmxCtx*) gf_filter_get_udta(filter);

	if (is_remove) {
		//TODO
		if (pid==ctx->mpd_pid) {
			ctx->mpd_pid = NULL;
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	//configure MPD pid
	if (!ctx->mpd_pid) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_URL);
		if (!p || !p->value.string) {
			return GF_NOT_SUPPORTED;
		}

		gf_filter_pid_set_framing_mode(pid, GF_TRUE);
		ctx->mpd_pid = pid;
		ctx->seek_request = -1;
		ctx->nb_playing = 0;

		e = gf_dash_open(ctx->dash, p->value.string);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASHDmx] Error - cannot initialize DASH Client for %s: %s\n", p->value.string, gf_error_to_string(e)));
			gf_filter_setup_failure(filter, e);
			return e;
		}
		return GF_OK;
	} else if (ctx->mpd_pid == pid) {
		return GF_OK;
	}

	//figure out group for this pid
	group_idx = dashdmx_group_idx_from_pid(ctx, pid);
	if (group_idx<0) return GF_NOT_SUPPORTED;

	//initial configure
	opid = gf_filter_pid_get_udta(pid);
	if (opid == NULL) {
		u32 dur;
		GF_DASHGroup *group = gf_dash_get_group_udta(ctx->dash, group_idx);
		assert(group);
		//for now we declare every component from the input source
		opid = gf_filter_pid_new(filter);
		gf_filter_pid_set_udta(opid, group);
		gf_filter_pid_set_udta(pid, opid);
		group->nb_pids ++;

		if (dashdmx_dash_can_reverse_playback(ctx) ) {
			gf_filter_pid_set_info(opid, GF_PROP_PID_REVERSE_PLAYBACK, &PROP_BOOL(GF_TRUE));
		}
		if (ctx->max_res && gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH)) {
			gf_filter_pid_set_info(opid, GF_PROP_SERVICE_WIDTH, &PROP_UINT(ctx->width));
			gf_filter_pid_set_info(opid, GF_PROP_SERVICE_HEIGHT, &PROP_UINT(ctx->height));
		}

		dur = (1000*gf_dash_get_duration(ctx->dash) );
		if (dur>0)
			gf_filter_pid_set_info(opid, GF_PROP_PID_DURATION, &PROP_FRAC_INT(dur, 1000) );

		dur = (1000*gf_dash_group_get_time_shift_buffer_depth(ctx->dash, group->idx) );
		if (dur>0)
			gf_filter_pid_set_info(opid, GF_PROP_PID_TIMESHIFT, &PROP_FRAC_INT(dur, 1000) );


		if (ctx->use_bmin) {
			u32 max = gf_dash_get_min_buffer_time(ctx->dash);
			gf_filter_pid_set_property_str(opid, "BufferLength", &PROP_UINT(max));
		}
		if (ctx->max_buffer)
			gf_filter_pid_set_property_str(opid, "BufferMaxOccupancy", &PROP_UINT(ctx->max_buffer) );

		gf_filter_pid_set_clock_mode(opid, GF_TRUE);
	}

	gf_filter_pid_copy_properties(opid, pid);

	return GF_OK;
}

static GF_Err dashdmx_initialize(GF_Filter *filter)
{
	GF_DASHDmxCtx *ctx = (GF_DASHDmxCtx*) gf_filter_get_udta(filter);
	ctx->filter = filter;
	ctx->dm = gf_filter_get_download_manager(filter);
	if (!ctx->dm) return GF_SERVICE_ERROR;

	ctx->dash_io.udta = ctx;
	ctx->dash_io.delete_cache_file = dashdmx_io_delete_cache_file;
	ctx->dash_io.create = dashdmx_io_create;
	ctx->dash_io.del = dashdmx_io_del;
	ctx->dash_io.abort = dashdmx_io_abort;
	ctx->dash_io.setup_from_url = dashdmx_io_setup_from_url;
	ctx->dash_io.set_range = dashdmx_io_set_range;
	ctx->dash_io.init = dashdmx_io_init;
	ctx->dash_io.run = dashdmx_io_run;
	ctx->dash_io.get_url = dashdmx_io_get_url;
	ctx->dash_io.get_cache_name = dashdmx_io_get_cache_name;
	ctx->dash_io.get_mime = dashdmx_io_get_mime;
	ctx->dash_io.get_header_value = dashdmx_io_get_header_value;
	ctx->dash_io.get_utc_start_time = dashdmx_io_get_utc_start_time;
	ctx->dash_io.get_bytes_per_sec = dashdmx_io_get_bytes_per_sec;
	ctx->dash_io.get_total_size = dashdmx_io_get_total_size;
	ctx->dash_io.get_bytes_done = dashdmx_io_get_bytes_done;
	ctx->dash_io.on_dash_event = dashdmx_io_on_dash_event;

	ctx->dash = gf_dash_new(&ctx->dash_io, GF_DASH_THREAD_NONE, ctx->max_buffer, ctx->auto_switch, (ctx->store==2) ? GF_TRUE : GF_FALSE, (ctx->algo==GF_DASH_ALGO_NONE) ? GF_TRUE : GF_FALSE, ctx->start_with, GF_FALSE, ctx->timeshift);

	if (!ctx->dash) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASHDmx] Error - cannot create DASH Client\n"));
		return GF_IO_ERR;
	}

	if (ctx->screen_res) {
		GF_FilterSessionCaps caps;
		gf_filter_get_session_caps(ctx->filter, &caps);
		gf_dash_set_max_resolution(ctx->dash, caps.max_screen_width, caps.max_screen_height, caps.max_screen_bpp);
	}

	gf_dash_set_algo(ctx->dash, ctx->algo);
	gf_dash_set_utc_shift(ctx->dash, ctx->shift_utc);
	gf_dash_set_atsc_ast_shift(ctx->dash, ctx->atsc_shift);
	gf_dash_enable_utc_drift_compensation(ctx->dash, ctx->server_utc);
	gf_dash_set_tile_adaptation_mode(ctx->dash, ctx->tile_mode, ctx->tiles_rate);

	gf_dash_set_min_timeout_between_404(ctx->dash, ctx->delay40X);
	gf_dash_set_segment_expiration_threshold(ctx->dash, ctx->exp_threshold);
	gf_dash_set_switching_probe_count(ctx->dash, ctx->switch_count);
	gf_dash_set_agressive_adaptation(ctx->dash, ctx->aggressive);
	gf_dash_debug_group(ctx->dash, ctx->debug_as);
	gf_dash_disable_speed_adaptation(ctx->dash, ctx->speed);

	return GF_OK;
}


static void dashdmx_finalize(GF_Filter *filter)
{
	GF_DASHDmxCtx *ctx = (GF_DASHDmxCtx*) gf_filter_get_udta(filter);
	assert(ctx);

	if (ctx->dash)
		gf_dash_del(ctx->dash);
}

static Bool dashdmx_process_event(GF_Filter *filter, const GF_FilterEvent *fevt)
{
	u32 i, count;
	GF_FilterEvent src_evt;
	GF_FilterPid *ipid;
	u64 pto;
	u32 timescale;
	Double offset;
	GF_DASHDmxCtx *ctx = (GF_DASHDmxCtx*) gf_filter_get_udta(filter);
	GF_DASHGroup *group;


	switch (fevt->base.type) {
	case GF_FEVT_QUALITY_SWITCH:
		if (fevt->quality_switch.set_tile_mode_plus_one) {
			GF_DASHTileAdaptationMode tile_mode = fevt->quality_switch.set_tile_mode_plus_one - 1;
			gf_dash_set_tile_adaptation_mode(ctx->dash, tile_mode, 100);
		} else if (fevt->quality_switch.set_auto) {
			gf_dash_set_automatic_switching(ctx->dash, 1);
		} else if (fevt->base.on_pid) {
			s32 idx;
			group = gf_filter_pid_get_udta(fevt->base.on_pid);
			if (!group) return GF_TRUE;
			idx = group->idx;

			gf_dash_group_set_quality_degradation_hint(ctx->dash, group->idx, fevt->quality_switch.quality_degradation);
			if (! fevt->quality_switch.ID) return GF_TRUE;

			if (fevt->quality_switch.dependent_group_index) {
				if (fevt->quality_switch.dependent_group_index > gf_dash_group_get_num_groups_depending_on(ctx->dash, group->idx))
					return GF_BAD_PARAM;

				idx = gf_dash_get_dependent_group_index(ctx->dash, group->idx, fevt->quality_switch.dependent_group_index-1);
				if (idx==-1) return GF_BAD_PARAM;
			}

			gf_dash_set_automatic_switching(ctx->dash, 0);
			gf_dash_group_select_quality(ctx->dash, idx, fevt->quality_switch.ID);
		} else {
			gf_dash_switch_quality(ctx->dash, fevt->quality_switch.up, ctx->immediate);
		}
		return GF_TRUE;

#if 0
	case GF_NET_SERVICE_INFO:
	{
		s32 idx;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASHDmx] Received Info command from terminal on Service (%p)\n", ctx->service));

		e = GF_OK;
		if (seg_filter_src) {
			/* defer to the real input service */
			e = seg_filter_src->ServiceCommand(seg_filter_src, com);
		}

		if (e!= GF_OK || !com->info.name || 2 > strlen(com->info.name)) {
			gf_dash_get_info(ctx->dash, &com->info.name, &com->info.comment);
		}
		idx = MPD_GetGroupIndexForChannel(ctx, com->play.on_channel);
		if (idx>=0) {
			//gather role and co
			if (!com->info.role) {
				gf_dash_group_enum_descriptor(ctx->dash, idx, GF_MPD_DESC_ROLE, 0, NULL, NULL, &com->info.role);
			}
			if (!com->info.accessibility) {
				gf_dash_group_enum_descriptor(ctx->dash, idx, GF_MPD_DESC_ACCESSIBILITY, 0, NULL, NULL, &com->info.accessibility);
			}
			if (!com->info.rating) {
				gf_dash_group_enum_descriptor(ctx->dash, idx, GF_MPD_DESC_RATING, 0, NULL, NULL, &com->info.rating);
			}
		}
		return GF_OK;
	}
	case GF_NET_GET_TIMESHIFT:
		com->timeshift.time = gf_dash_get_timeshift_buffer_pos(ctx->dash);
		return GF_OK;

	case GF_NET_ASSOCIATED_CONTENT_TIMING:
		gf_dash_override_ntp(ctx->dash, com->addon_time.ntp);
		return GF_OK;
#endif
	default:
		break;
	}

	/*not supported*/
	if (!fevt->base.on_pid) return GF_NOT_SUPPORTED;
	group = gf_filter_pid_get_udta(fevt->base.on_pid);
	if (!group) return GF_NOT_SUPPORTED;
	count = gf_filter_get_ipid_count(filter);
	ipid = NULL;
	for (i=0; i<count; i++) {
		ipid = gf_filter_get_ipid(filter, i);
		if (gf_filter_pid_get_udta(ipid) == fevt->base.on_pid) break;
		ipid = NULL;
	}

	switch (fevt->base.type) {


#if 0
	case GF_NET_CHAN_GET_SRD:
	{
		Bool res;
		idx = MPD_GetGroupIndexForChannel(ctx, com->base.on_channel);
		if (idx < 0) return GF_BAD_PARAM;
		if (com->srd.dependent_group_index) {
			if (com->srd.dependent_group_index > gf_dash_group_get_num_groups_depending_on(ctx->dash, idx))
				return GF_BAD_PARAM;
			
			idx = gf_dash_get_dependent_group_index(ctx->dash, idx, com->srd.dependent_group_index-1);
		}		
		res = gf_dash_group_get_srd_info(ctx->dash, idx, NULL, &com->srd.x, &com->srd.y, &com->srd.w, &com->srd.h, &com->srd.width, &com->srd.height);
		return res ? GF_OK : GF_NOT_SUPPORTED;
	}

	case GF_NET_SERVICE_QUALITY_QUERY:
	{
		GF_DASHQualityInfo qinfo;
		GF_Err e;
		u32 count, g_idx;
		idx = MPD_GetGroupIndexForChannel(ctx, com->quality_query.on_channel);
		if (idx < 0) return GF_BAD_PARAM;
		count = gf_dash_group_get_num_qualities(ctx->dash, idx);
		if (!com->quality_query.index && !com->quality_query.dependent_group_index) {
			com->quality_query.index = count;
			com->quality_query.dependent_group_index = gf_dash_group_get_num_groups_depending_on(ctx->dash, idx);
			return GF_OK;
		}
		if (com->quality_query.dependent_group_index) {
			if (com->quality_query.dependent_group_index > gf_dash_group_get_num_groups_depending_on(ctx->dash, idx))
				return GF_BAD_PARAM;
			
			g_idx = gf_dash_get_dependent_group_index(ctx->dash, idx, com->quality_query.dependent_group_index-1);
			if (g_idx==(u32)-1) return GF_BAD_PARAM;
			count = gf_dash_group_get_num_qualities(ctx->dash, g_idx);
			if (com->quality_query.index>count) return GF_BAD_PARAM;
		} else {
			if (com->quality_query.index>count) return GF_BAD_PARAM;
			g_idx = idx;
		}

		e = gf_dash_group_get_quality_info(ctx->dash, g_idx, com->quality_query.index-1, &qinfo);
		if (e) return e;
		//group->bandwidth = qinfo.bandwidth;
		com->quality_query.bandwidth = qinfo.bandwidth;
		com->quality_query.ID = qinfo.ID;
		com->quality_query.mime = qinfo.mime;
		com->quality_query.codec = qinfo.codec;
		com->quality_query.width = qinfo.width;
		com->quality_query.height = qinfo.height;
		com->quality_query.interlaced = qinfo.interlaced;
		if (qinfo.fps_den) {
			com->quality_query.fps = qinfo.fps_num;
			com->quality_query.fps /= qinfo.fps_den;
		} else {
			com->quality_query.fps = qinfo.fps_num;
		}
		com->quality_query.par_num = qinfo.par_num;
		com->quality_query.par_den = qinfo.par_den;
		com->quality_query.sample_rate = qinfo.sample_rate;
		com->quality_query.nb_channels = qinfo.nb_channels;
		com->quality_query.disabled = qinfo.disabled;
		com->quality_query.is_selected = qinfo.is_selected;
		com->quality_query.automatic = gf_dash_get_automatic_switching(ctx->dash);
		com->quality_query.tile_adaptation_mode = (u32) gf_dash_get_tile_adaptation_mode(ctx->dash);
		return GF_OK;
	}
	case GF_NET_CHAN_VISIBILITY_HINT:
		idx = MPD_GetGroupIndexForChannel(ctx, com->base.on_channel);
		if (idx < 0) return GF_BAD_PARAM;

		return gf_dash_group_set_visible_rect(ctx->dash, idx, com->visibility_hint.min_x, com->visibility_hint.max_x, com->visibility_hint.min_y, com->visibility_hint.max_y);
#endif

	case GF_FEVT_PLAY:
		src_evt = *fevt;

		//adjust play range from media timestamps to MPD time
		if (fevt->play.timestamp_based) {

			if (fevt->play.timestamp_based==1) {
				gf_dash_group_get_presentation_time_offset(ctx->dash, group->idx, &pto, &timescale);
				offset = (Double) pto;
				offset /= timescale;
				src_evt.play.start_range -= offset;
				if (src_evt.play.start_range < 0) src_evt.play.start_range = 0;
			}

			group->is_timestamp_based = 1;
			group->pto_setup = 0;
			ctx->media_start_range = fevt->play.start_range;
		} else {
			group->is_timestamp_based = 0;
			group->pto_setup = 0;
			if (fevt->play.start_range<0) src_evt.play.start_range = 0;
			//in m3u8, we need also media start time for mapping time
			if (gf_dash_is_m3u8(ctx->dash))
				ctx->media_start_range = fevt->play.start_range;
		}

		//we cannot handle seek request outside of a period being setup, this messes up our internal service setup
		//we postpone the seek and will request it later on ...
		if (gf_dash_in_period_setup(ctx->dash)) {
			u64 p_end = gf_dash_get_period_duration(ctx->dash);
			if (p_end) {
				p_end += gf_dash_get_period_start(ctx->dash);
				if (p_end<1000*fevt->play.start_range) {
					ctx->seek_request = fevt->play.start_range;
					return GF_TRUE;
				}
			}
		}

		gf_dash_set_speed(ctx->dash, fevt->play.speed);

		/*don't seek if this command is the first PLAY request of objects declared by the subservice, unless start range is not default one (0) */
		if (!ctx->nb_playing && (!fevt->play.initial_broadcast_play || (fevt->play.start_range>1.0) )) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASHDmx] Received Play command on group %d\n", group->idx));

			if (fevt->play.end_range<=0) {
				u32 ms = (u32) ( 1000 * (-fevt->play.end_range) );
				if (ms<1000) ms = 0;
				gf_dash_set_timeshift(ctx->dash, ms);
			}
			gf_dash_seek(ctx->dash, fevt->play.start_range);

			//to remove once we manage to keep the service alive
			/*don't forward commands if a switch of period is to be scheduled, we are killing the service anyway ...*/
			if (gf_dash_get_period_switch_status(ctx->dash)) return GF_TRUE;
		} else if (!fevt->play.initial_broadcast_play) {
			/*don't forward commands if a switch of period is to be scheduled, we are killing the service anyway ...*/
			if (gf_dash_get_period_switch_status(ctx->dash)) return GF_TRUE;

			//seek on a single group
			gf_dash_group_seek(ctx->dash, group->idx, fevt->play.start_range);
		}

		//check if current segment playback should be aborted
		src_evt.play.forced_dash_segment_switch = gf_dash_group_segment_switch_forced(ctx->dash, group->idx);

		gf_dash_group_select(ctx->dash, group->idx, GF_TRUE);
		gf_dash_set_group_done(ctx->dash, (u32) group->idx, 0);

		//adjust start range from MPD time to media time
		src_evt.play.start_range = gf_dash_group_get_start_range(ctx->dash, group->idx);
		gf_dash_group_get_presentation_time_offset(ctx->dash, group->idx, &pto, &timescale);
		src_evt.play.start_range += ((Double)pto) / timescale;

		dashdmx_setup_buffer(ctx, group);

		ctx->nb_playing++;
		//forward new event
		src_evt.base.on_pid = ipid;
		gf_filter_pid_send_event(ipid, &src_evt);
		//cancel the event
		return GF_TRUE;

	case GF_FEVT_STOP:
		gf_dash_set_group_done(ctx->dash, (u32) group->idx, 1);

		if (ctx->nb_playing)
			ctx->nb_playing--;
		//don't cancel the event
		return GF_FALSE;

	case GF_FEVT_CAPS_CHANGE:
		if (ctx->screen_res) {
			GF_FilterSessionCaps caps;
			gf_filter_get_session_caps(ctx->filter, &caps);
			gf_dash_set_max_resolution(ctx->dash, caps.max_screen_width, caps.max_screen_height, caps.max_screen_bpp);
		}
		return GF_FALSE;
	//by default don't cancel
	default:
		return GF_FALSE;
	}

	return GF_FALSE;
}

static void dashdmx_switch_segment(GF_DASHDmxCtx *ctx, GF_DASHGroup *group)
{
	u32 dependent_representation_index = 0;
	GF_Err e;
	Bool has_next;
	GF_FilterEvent evt;
	const char *next_url, *next_url_init_or_switch_segment, *src_url, *key_url;
	u64 start_range, end_range, switch_start_range, switch_end_range;
	u32 nb_segments_cached;
	bin128 key_IV;
	Bool group_done;

	group->wait_for_pck = GF_TRUE;

	if (group->segment_sent) {
		gf_dash_group_discard_segment(ctx->dash, group->idx);
		group->segment_sent = GF_FALSE;
		//no thread mode, we work with at most one entry in cache, call process right away to get the group next URL ready
		gf_dash_process(ctx->dash);
	}

	group->stats_uploaded = GF_FALSE;
	group_done = GF_FALSE;
	nb_segments_cached = gf_dash_group_get_num_segments_ready(ctx->dash, group->idx, &group_done);

	if (group_done) {
		if (!gf_dash_get_period_switch_status(ctx->dash) && gf_dash_in_last_period(ctx->dash, GF_TRUE)) {
			return;
		}
	}
	e = gf_dash_group_get_next_segment_location(ctx->dash, group->idx, dependent_representation_index, &next_url, &start_range, &end_range,
		        NULL, &next_url_init_or_switch_segment, &switch_start_range , &switch_end_range,
		        &src_url, &has_next, &key_url, &key_IV);

	if (e == GF_EOS) {
		group->eos_detected = GF_TRUE;
		return;
	}
	if (e != GF_OK) {
		return;
	}
	assert(next_url);

	if (next_url_init_or_switch_segment && !group->init_switch_seg_sent) {
		GF_FEVT_INIT(evt, GF_FEVT_SOURCE_SWITCH,  NULL);
		evt.seek.start_offset = switch_start_range;
		evt.seek.end_offset = switch_end_range;
		evt.seek.source_switch = next_url_init_or_switch_segment;
		evt.seek.previous_is_init_segment = group->prev_is_init_segment;
		evt.seek.skip_cache_expiration = GF_TRUE;

		group->prev_is_init_segment = GF_TRUE;

		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Queuing next init/switching segment %s\n", next_url_init_or_switch_segment));


		group->init_switch_seg_sent = GF_TRUE;
		gf_filter_send_event(group->seg_filter_src, &evt);
		return;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Queuing next media segment %s\n", next_url));

	GF_FEVT_INIT(evt, GF_FEVT_SOURCE_SWITCH, NULL);
	evt.seek.source_switch = next_url;
	evt.seek.start_offset = start_range;
	evt.seek.end_offset = end_range;
	evt.seek.previous_is_init_segment = group->prev_is_init_segment;
	group->segment_sent = GF_TRUE;
	group->prev_is_init_segment = GF_FALSE;
	group->init_switch_seg_sent = GF_FALSE;
	gf_filter_send_event(group->seg_filter_src, &evt);
}

static void dashdmx_update_group_stats(GF_DASHDmxCtx *ctx, GF_DASHGroup *group)
{
	u32 bytes_per_sec=0, file_size=0, bytes_done=0;
	const GF_PropertyValue *p;
	if (group->stats_uploaded) return;

	p = gf_filter_get_info(group->seg_filter_src, GF_PROP_PID_DOWN_RATE);
	if (p) bytes_per_sec = p->value.uint / 8;

	p = gf_filter_get_info(group->seg_filter_src, GF_PROP_PID_DOWN_SIZE);
	if (p) file_size = p->value.uint;

	p = gf_filter_get_info(group->seg_filter_src, GF_PROP_PID_DOWN_BYTES);
	if (p) bytes_done = p->value.uint;

	gf_dash_group_store_stats(ctx->dash, group->idx, bytes_per_sec, file_size, bytes_done);

	//we allow file abort, check the download
	if (ctx->abort)
		gf_dash_group_check_bandwidth(ctx->dash, group->idx);

	p = gf_filter_get_info(group->seg_filter_src, GF_PROP_PID_FILE_CACHED);
	if (p && p->value.boolean)
		group->stats_uploaded = GF_TRUE;
}

GF_Err dashdmx_process(GF_Filter *filter)
{
	u32 i, count;
	GF_FilterPacket *pck;
	GF_Err e;
	Bool check_eos = GF_FALSE;
	Bool has_pck = GF_FALSE;
	GF_DASHDmxCtx *ctx = (GF_DASHDmxCtx*) gf_filter_get_udta(filter);

	//reset group states and update stats
	count = gf_dash_get_group_count(ctx->dash);
	for (i=0; i<count; i++) {
		GF_DASHGroup *group = gf_dash_get_group_udta(ctx->dash, i);
		if (!group) continue;
		group->nb_eos = 0;
		if (group->eos_detected) check_eos = GF_TRUE;
		dashdmx_update_group_stats(ctx, group);
	}
	if (ctx->mpd_pid) {
		//check MPD pid
		pck = gf_filter_pid_get_packet(ctx->mpd_pid);
		if (pck) {
			gf_filter_pid_drop_packet(ctx->mpd_pid);
		}
		e = gf_dash_process(ctx->dash);
		if (e)
			return e;
	}

	//flush all media input
	count = gf_filter_get_ipid_count(filter);
	for (i=0; i<count; i++) {
		GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
		GF_FilterPid *opid;
		GF_DASHGroup *group;
		if (ipid == ctx->mpd_pid) continue;
		opid = gf_filter_pid_get_udta(ipid);
		group = gf_filter_pid_get_udta(opid);

		while (1) {
			pck = gf_filter_pid_get_packet(ipid);
			if (!pck) {
				if (gf_filter_pid_is_eos(ipid)) {
					group->nb_eos++;

					if (group->nb_eos==group->nb_pids) {
						gf_filter_pid_clear_eos(ipid);
						dashdmx_update_group_stats(ctx, group);
						dashdmx_switch_segment(ctx, group);
						if (group->eos_detected && !has_pck) check_eos = GF_TRUE;
					}
				}
				break;
			}
			has_pck = GF_TRUE;
			check_eos = GF_FALSE;
			dashdmx_forward_packet(ctx, pck, ipid, opid, group);
			group->wait_for_pck = GF_FALSE;
		}
	}

	if (check_eos) {
		for (i=0; i<count; i++) {
			GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
			GF_FilterPid *opid;
			GF_DASHGroup *group;
			if (ipid == ctx->mpd_pid) continue;
			opid = gf_filter_pid_get_udta(ipid);
			group = gf_filter_pid_get_udta(opid);
			if (group->eos_detected) gf_filter_pid_set_eos(opid);
		}
	}
	return GF_OK;
}

static const GF_FilterCapability DASHDmxInputs[] =
{
	CAP_INC_STRING(GF_PROP_PID_MIME, "application/dash+xml|video/vnd.3gpp.mpd|audio/vnd.3gpp.mpd|video/vnd.mpeg.dash.mpd|audio/vnd.mpeg.dash.mpd"),
	{},
	CAP_INC_STRING(GF_PROP_PID_FILE_EXT, "mpd|m3u8|3gm|ism"),
	{},
	//accept any stream but files, framed
	{ .code=GF_PROP_PID_STREAM_TYPE, .val=PROP_UINT(GF_STREAM_FILE), .exclude=1, .in_bundle=1, .explicit_only=1 },
	{ .code=GF_PROP_PID_UNFRAMED, .val=PROP_BOOL(GF_TRUE), .exclude=1, .in_bundle=1, .explicit_only=1 },
};


#define OFFS(_n)	#_n, offsetof(GF_DASHDmxCtx, _n)
static const GF_FilterArgs DASHDmxArgs[] =
{
	{ OFFS(max_buffer), "max content buffer in ms. If 0 uses default player settings", GF_PROP_UINT, "10000", NULL, GF_FALSE},
	{ OFFS(auto_switch), "switch quality every N segments, disabled if 0", GF_PROP_UINT, "0", NULL, GF_FALSE},
	{ OFFS(store), "enables file caching", GF_PROP_UINT, "mem", "mem|file|cache", GF_FALSE},
	{ OFFS(algo), "adaptation algorithm to use. The following algorithms are defined:\n\
					\tnone: no adaptation logic\n\
					\tgrate: GAPC legacy algo based on available rate\n\
					\tgbuf: GAPC legacy algo based on buffer occupancy\n\
					\tbba0: BBA-0\n\
					\tbolaf: BOLA Finite\n\
					\tbolab: BOLA Basic\n\
					\tbolau: BOLA-U\n\
					\tbolao: BOLA-O\n\
					", GF_PROP_UINT, "gbuf", "none|grate|gbuf|bba0|bolaf|bolab|bolau|bolao", GF_FALSE},
	{ OFFS(start_with), "specifies the initial representation criteria:\n\
						min_q: start with lowest quality\n\
						max_q: start with highest quality\n\
						min_bw: start with lowest bitrate\n\
						max_bw: start with highest bitrate; for tiles are used, all low priority tiles will have the lower (below max) bandwidth selected\n\
						max_bw_tiles: start with highest bitrate; for tiles all low priority tiles will have their lowest bandwidth selected\n\
						", GF_PROP_UINT, "min_bw", "min_q|max_q|min_bw|max_bw|max_bw_tiles", GF_FALSE},

	{ OFFS(max_res), "use max media resolution to configure display", GF_PROP_BOOL, "true", NULL, GF_FALSE},
	{ OFFS(immediate), "when interactive switching is requested and immediate is set, the buffer segments are trashed", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(abort), "allow abort during a segment download", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(use_bmin), "Uses the indicated min buffer time of the MPD if true, otherwise uses default player settings", GF_PROP_BOOL, "false", NULL, GF_FALSE},

	{ OFFS(shift_utc), "shifts DASH UTC clock", GF_PROP_SINT, "0", NULL, GF_FALSE},
	{ OFFS(atsc_shift), "shifts ATSC requests time by given ms", GF_PROP_SINT, "0", NULL, GF_FALSE},
	{ OFFS(server_utc), "Uses ServerUTC: or Date: http headers instead of local UTC", GF_PROP_BOOL, "yes", NULL, GF_FALSE},
	{ OFFS(screen_res), "Uses screen resolution in selection phase", GF_PROP_BOOL, "yes", NULL, GF_FALSE},
	{ OFFS(timeshift), "Sets initial timshift in ms (if >0) or in %% of timeshift buffer (if <0)", GF_PROP_UINT, "0", NULL, GF_FALSE},
	{ OFFS(tile_mode), "Sets tile adaptiaion mode:\
						none: bitrate is shared equaly accross all tiles\n\
						rows: bitrate decreases for each row of tiles starting from the top, same rate for each tile on the row\n\
						rrows: bitrate decreases for each row of tiles starting from the bottom, same rate for each tile on the row\n\
						mrows: bitrate decreased for top and bottom rows only, same rate for each tile on the row\n\
						cols: bitrate decreases for each columns of tiles starting from the left, same rate for each tile on the columns\n\
						rcols: bitrate decreases for each columns of tiles starting from the right, same rate for each tile on the columns\n\
						mcols: bitrate decreased for left and right columns only, same rate for each tile on the columns\n\
						center: bitrate decreased for all tiles on the edge of the picture\n\
						edges: bitrate decreased for all tiles on the center of the picture\n\
						", GF_PROP_UINT, "none", "none|rows|rrows|mrows|cols|rcols|mcols|center|edges", GF_FALSE},
	{ OFFS(tiles_rate), "Indicates the amount of bandwidth to use at each quality level. The rate is recursively applied at each level, e.g. if 50%, Level1 gets 50%, level2 gets 25%, ... If 100, automatic rate allocation will be done by maximizing the quality in order of priority. If 0, bitstream will not be smoothed across tiles/qualities, and concurrency may happen between different media.", GF_PROP_UINT, "100", NULL, GF_FALSE},
	{ OFFS(delay40X), "Delay in millisconds to wait between two 40X on the same segment", GF_PROP_UINT, "500", NULL, GF_FALSE},
	{ OFFS(exp_threshold), "Delay in millisconds to wait after the segment AvailabilityEndDate before considering the segment lost", GF_PROP_UINT, "100", NULL, GF_FALSE},
	{ OFFS(switch_count), "Indicates how many segments the client shall wait before switching up bandwidth. If 0, switch will happen as soon as the bandwidth is enough, but this is more prone to network variations", GF_PROP_UINT, "1", NULL, GF_FALSE},
	{ OFFS(aggressive), "If enabled, switching algo targets the closest bandwidth fitting the available download rate. If no, switching algo targets the lowest bitrate representation that is above the currently played (eg does not try to switch to max bandwidth)", GF_PROP_BOOL, "no", NULL, GF_FALSE},
	{ OFFS(debug_as), "Plays only the adaptation set indicated by its index in the MPD; if negative, all sets are used", GF_PROP_UINT, "-1", NULL, GF_FALSE},
	{ OFFS(speed), "Enables adaptation based on playback speed", GF_PROP_BOOL, "no", NULL, GF_FALSE},
	{}
};

static const GF_FilterCapability DASHDmxOutputs[] =
{
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_EXC_UINT(GF_PROP_PID_CODECID, GF_CODECID_RAW),
	{},
#if 0
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_SCENE),
	CAP_INC_UINT(GF_PROP_PID_CODECID, GF_CODECID_BIFS),
	CAP_INC_UINT(GF_PROP_PID_CODECID, GF_CODECID_BIFS_V2),
	{},
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_OD),
	CAP_INC_UINT(GF_PROP_PID_CODECID, GF_CODECID_OD_V1),
	CAP_INC_UINT(GF_PROP_PID_CODECID, GF_CODECID_OD_V2),
	{},
#endif
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_PRIVATE_SCENE),
	CAP_INC_UINT(GF_PROP_PID_CODECID, GF_CODECID_DVB_EIT),
};


GF_FilterRegister DASHDmxRegister = {
	.name = "dash",
	.description = "MPEG-DASH & HLS Demux",
	.private_size = sizeof(GF_DASHDmxCtx),
	.initialize = dashdmx_initialize,
	.finalize = dashdmx_finalize,
	.args = DASHDmxArgs,
	INCAPS(DASHDmxInputs),
	OUTCAPS(DASHDmxOutputs),
	.configure_pid = dashdmx_configure_pid,
	.process = dashdmx_process,
	.process_event = dashdmx_process_event,
	//we accept as many input pids as loaded by the session
	.max_extra_pids = (u32) -1,
};


#endif //GPAC_DISABLE_DASH_CLIENT

const GF_FilterRegister *dashdmx_register(GF_FilterSession *session)
{
#ifndef GPAC_DISABLE_DASH_CLIENT
	return &DASHDmxRegister;
#else
	return NULL;
#endif
}
