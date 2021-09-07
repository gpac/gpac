/**
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre, Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2010-2021
 *					All rights reserved
 *
 *  This file is part of GPAC / Adaptive HTTP Streaming
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

#include <gpac/network.h>
#include <gpac/dash.h>
#include <gpac/mpd.h>
#include <gpac/internal/m3u8.h>
#include <gpac/internal/isomedia_dev.h>
#include <gpac/base_coding.h>
#include <string.h>
#include <sys/stat.h>

#include <math.h>


#ifndef GPAC_DISABLE_DASH_CLIENT

/*ISO 639 languages*/
#include <gpac/iso639.h>

/*set to 1 if you want MPD to use SegmentTemplate if possible instead of SegmentList*/
#define M3U8_TO_MPD_USE_TEMPLATE	0
/*set to 1 if you want MPD to use SegmentTimeline*/
#define M3U8_TO_MPD_USE_SEGTIMELINE	0


typedef enum {
	GF_DASH_STATE_STOPPED = 0,
	/*period setup and playback chain creation*/
	GF_DASH_STATE_SETUP,
	/*request to start playback chain*/
	GF_DASH_STATE_CONNECTING,
	GF_DASH_STATE_RUNNING,

	GF_DASH_STATE_CHAIN_NEXT,
	GF_DASH_STATE_CHAIN_FALLBACK,
} GF_DASH_STATE;


enum
{
	GF_DASH_CHAIN_REG=0,
	GF_DASH_CHAIN_PUSH,
	GF_DASH_CHAIN_POP
};


//shifts AST in the past(>0) or future (<0)  so that client starts request in the future or in the past
//#define FORCE_DESYNC	4000

/* WARNING: GF_DASH_Group does not represent a Group in DASH
   It corresponds to an AdaptationSet with additional live information not present in the MPD
   (e.g. current active representation)
*/
typedef struct __dash_group GF_DASH_Group;

struct __dash_client
{
	GF_DASHFileIO *dash_io;

	/*interface to mpd parser - get rid of this and use the DASHIO instead ?*/
	GF_FileDownload getter;

	char *base_url;

	u32 max_cache_duration, max_width, max_height;
	u8 max_bit_per_pixel;
	u32 auto_switch_count;
	Bool keep_files, disable_switching, allow_local_mpd_update, estimate_utc_drift, ntp_forced;
	Bool is_m3u8, is_smooth;
	Bool split_adaptation_set;
	GF_DASHLowLatencyMode low_latency_mode;
	//set when MPD downloading fails. Will resetup DASH live once MPD is sync again
	Bool in_error;

	u64 mpd_fetch_time;
	GF_DASHInitialSelectionMode first_select_mode;

	/* MPD downloader*/
	GF_DASHFileIOSession mpd_dnload;
	/* MPD */
	GF_MPD *mpd;
	/* number of time the MPD has been reloaded and last update time*/
	u32 reload_count, last_update_time;
	/*signature of last MPD*/
	u8 lastMPDSignature[GF_SHA1_DIGEST_SIZE];
	/*mime type of media segments (m3u8)*/
	char *mimeTypeForM3U8Segments;

	/* active period in MPD */
	u32 active_period_index;
	u32 reinit_period_index;
	u32 request_period_switch;

	Bool next_period_checked;

	u64 start_time_in_active_period;

	Bool ignore_mpd_duration;
	u32 initial_time_shift_value;

	const char *query_string;

	/*list of groups in the active period*/
	GF_List *groups;

	/* one of the above state*/
	GF_DASH_STATE dash_state;

	Bool in_period_setup;
	Bool all_groups_done_notified;

	s64 utc_drift_estimate;
	s32 utc_shift;

	Double start_range_period;

	Double speed;
	Bool is_rt_speed;
	u32 probe_times_before_switch;
	Bool agressive_switching;
	u32 min_wait_ms_before_next_request;
	u32 min_wait_sys_clock;

	Bool force_mpd_update;
	u32 force_period_reload;

	u32 user_buffer_ms;

	u32 min_timeout_between_404, segment_lost_after_ms;

	Bool ignore_xlink;

	//0: not ROUTE - 1: ROUTE but clock not init - 2: ROUTE clock init
	u32 route_clock_state;
	//ROUTE AST shift in ms
	u32 route_ast_shift;
	u32 route_skip_segments_ms;
    Bool route_low_latency;

	Bool initial_period_tunein;
	u32 preroll_state;

	Bool llhls_single_range;
	Bool m3u8_reload_master;
	u32 hls_reload_time;


	//in ms
	u32 time_in_tsb, prev_time_in_tsb;
	u32 tsb_exceeded;
	const u32 *dbg_grps_index;
	u32 nb_dbg_grps;
	Bool disable_speed_adaptation;

	Bool period_groups_setup;
	u32 tile_rate_decrease;
	GF_DASHTileAdaptationMode tile_adapt_mode;
	Bool disable_low_quality_tiles;
	u32 chaining_mode, chain_stack_state;

	GF_List *SRDs;

	GF_DASHAdaptationAlgorithm adaptation_algorithm;

	s32 (*rate_adaptation_algo)(GF_DashClient *dash, GF_DASH_Group *group, GF_DASH_Group *base_group,
												  u32 dl_rate, Double speed, Double max_available_speed, Bool force_lower_complexity,
												  GF_MPD_Representation *rep, Bool go_up_bitrate);

	s32 (*rate_adaptation_download_monitor)(GF_DashClient *dash, GF_DASH_Group *group, u32 bits_per_sec, u64 total_bytes, u64 bytes_done, u64 us_since_start, u32 buffer_dur_ms, u32 current_seg_dur);

	//for custom algo, total rate of all active groups being downloaded
	u32 total_rate;

	gf_dash_rate_adaptation rate_adaptation_algo_custom;
	gf_dash_download_monitor rate_adaptation_download_monitor_custom;
	void *udta_custom_algo;

	/*if set, enables group selection at dash client level, otherwise leave the decision to the user app*/
	Bool enable_group_selection;


	char *chain_next, *chain_fallback;
	GF_List *chain_stack;
};

static void gf_dash_seek_group(GF_DashClient *dash, GF_DASH_Group *group, Double seek_to, Bool is_dynamic);


enum
{
	SEG_FLAG_LOOP_DETECTED = 1,
	SEG_FLAG_DEP_FOLLOWING = 1<<1,
	SEG_FLAG_DISABLED = 1<<2,
};

typedef struct
{
	char *url;
	u64 start_range, end_range;
	/*representation index in adaptation_set->representations*/
	u32 representation_index;
	u32 duration;
	u32 dep_group_idx;
	char *key_url;
	bin128 key_IV;
	u32 seg_number;
	const char *seg_name_start;
	GF_Fraction64 time;

	u32 flags;
} segment_cache_entry;

typedef enum
{
	/*set if group cannot be selected (wrong MPD)*/
	GF_DASH_GROUP_NOT_SELECTABLE = 0,
	GF_DASH_GROUP_NOT_SELECTED,
	GF_DASH_GROUP_SELECTED,
} GF_DASHGroupSelection;

/*this structure Group is the implementation of the adaptationSet element of the MPD.*/
struct __dash_group
{
	GF_DashClient *dash;

	/*pointer to adaptation set*/
	GF_MPD_AdaptationSet *adaptation_set;
	/*pointer to active period*/
	GF_MPD_Period *period;

	/*active representation index in adaptation_set->representations*/
	u32 active_rep_index;

	u32 prev_active_rep_index;

	Bool timeline_setup;
	Bool force_timeline_reeval;
	Bool first_hls_chunk;

	GF_DASHGroupSelection selection;

	/*may be mpd@time_shift_buffer_depth or rep@time_shift_buffer_depth*/
	u32 time_shift_buffer_depth;

	Bool bitstream_switching;
	GF_DASH_Group *depend_on_group;
	Bool done;
	//if set, will redownload the last segment partially downloaded
	Bool force_switch_bandwidth;
	Bool min_bandwidth_selected;

	u32 active_bitrate, max_bitrate, min_bitrate;
	u32 min_representation_bitrate;

	u32 nb_segments_in_rep;
	//cached index of this group in dash->groups to avoid using gf_list_find
	u32 groups_idx;

	/* Segment duration as advertised in the MPD
	   for the real duration of the segment being downloaded see current_downloaded_segment_duration */
	Double segment_duration;

	Double start_playback_range;

	Bool group_setup;

	Bool was_segment_base;
	/*local file playback, do not delete them*/
	Bool local_files;
	/*next segment to download for this group - negative number to take into account SN wrapping*/
	s32 download_segment_index;
	/*number of segments pruged since the start of the period*/
	u32 nb_segments_purged;

	u32 nb_retry_on_last_segment;
	s32 start_number_at_last_ast;
	u64 ast_at_init;
	u32 ast_offset;

	Bool init_segment_is_media;

	u32 max_cached_segments, nb_cached_segments;
	segment_cache_entry *cached;

	/*usually 0-0 (no range) but can be non-zero when playing local MPD/DASH sessions*/
	u64 bs_switching_init_segment_url_start_range, bs_switching_init_segment_url_end_range;
	char *bs_switching_init_segment_url;
	const char *bs_switching_init_segment_url_name_start;

	u32 nb_segments_done;
	u32 last_segment_time;
	u32 nb_segments_since_switch;

	//stats of last downloaded segment
	u32 total_size, bytes_per_sec, bytes_done, backup_Bps;


	Bool segment_must_be_streamed;
	Bool broken_timing;

	u32 maybe_end_of_stream;
	u32 cache_duration;
	u32 time_at_first_reload_required;
	u32 force_representation_idx_plus_one;

	Bool force_segment_switch;
	Bool loop_detected;

	u32 time_at_first_failure, time_at_last_request;
	Bool prev_segment_ok, segment_in_valid_range;
	//this is the number of 404
	u32 nb_consecutive_segments_lost;
	u64 retry_after_utc;
	/*set when switching segment, indicates the current downloaded segment duration*/
	u64 current_downloaded_segment_duration;

	char *service_mime;

	/* base representation index of this group plus one, or 0 if all representations in this group are independent*/
	u32 base_rep_index_plus_one;

	/* maximum representation index we want to download*/
	u32 max_complementary_rep_index;
	//start time and timescales of currently downloaded segment
	u64 current_start_time;
	u32 current_timescale;

	void *udta;

	Bool has_pending_enhancement;

	/*Codec statistics*/
	u32 avg_dec_time, max_dec_time, irap_avg_dec_time, irap_max_dec_time;
	Bool codec_reset;
	Bool decode_only_rap;
	/*display statistics*/
	u32 display_width, display_height;
	/*sets by user, indicates when the client will decide to play/resume after a buffering period (this is a static value for the entire session)*/
	u32 max_buffer_playout_ms;
	/*buffer status*/
	u32 buffer_min_ms, buffer_max_ms, buffer_occupancy_ms;
	u32 buffer_occupancy_at_last_seg;

	u32 m3u8_start_media_seq;
	u32 hls_next_seq_num;

	GF_List *groups_depending_on;
	u32 current_dep_idx;

	u32 target_new_rep;

	u32 srd_x, srd_y, srd_w, srd_h, srd_row_idx, srd_col_idx;
	struct _dash_srd_desc *srd_desc;

	/*current index of the base URL used*/
	u32 current_base_url_idx;

	u32 quality_degradation_hint;

	Bool rate_adaptation_postponed;
	Bool update_tile_qualities;

	//for dash custom, allows temporary disabling a group
	Bool disabled;

	/* current segment index in BBA and BOLA algorithm */
	u32 current_index;

	//in non-threaded mode, indicates that the demux for this group has nothing to do...
	Bool force_early_fetch;
	Bool is_low_latency;

	u32 hint_visible_width, hint_visible_height;

	//last chunk scheduled for download
	GF_MPD_SegmentURL *llhls_edge_chunk;
	Bool llhls_last_was_merged;
	s32 llhls_switch_request;
	u32 last_mpd_change_time;
};

//wait time before requesting again a M3U8 child playlist update when something goes wrong during the update: either same file or the expected next segment is not there
#define HLS_MIN_RELOAD_TIME(_dash) _dash->hls_reload_time = 50 + gf_sys_clock();


static void gf_dash_solve_period_xlink(GF_DashClient *dash, GF_List *period_list, u32 period_idx);

struct _dash_srd_desc
{
	u32 srd_nb_rows, srd_nb_cols;
	u32 id, width, height, srd_fw, srd_fh;
};

void drm_decrypt(unsigned char * data, unsigned long dataSize, const char * decryptMethod, const char * keyfileURL, const unsigned char * keyIV);



static const char *gf_dash_get_mime_type(GF_MPD_SubRepresentation *subrep, GF_MPD_Representation *rep, GF_MPD_AdaptationSet *set)
{
	if (subrep && subrep->mime_type) return subrep->mime_type;
	if (rep && rep->mime_type) return rep->mime_type;
	if (set && set->mime_type) return set->mime_type;
	return NULL;
}


static u64 dash_get_fetch_time(GF_DashClient *dash)
{
	u64 utc = 0;

	if (dash->mpd_dnload && dash->dash_io->get_utc_start_time)
		utc = dash->dash_io->get_utc_start_time(dash->dash_io, dash->mpd_dnload);
	if (!utc)
		utc = gf_net_get_utc();
	return utc;
}


static u32 gf_dash_group_count_rep_needed(GF_DASH_Group *group)
{
	u32 count, nb_rep_need, next_rep_index_plus_one;
	GF_MPD_Representation *rep;
	count  = gf_list_count(group->adaptation_set->representations);
	nb_rep_need = 1;
	if (!group->base_rep_index_plus_one || (group->base_rep_index_plus_one == group->max_complementary_rep_index+1))
		return nb_rep_need; // we need to download only one representation
	rep = gf_list_get(group->adaptation_set->representations, group->base_rep_index_plus_one-1);
	next_rep_index_plus_one = rep->playback.enhancement_rep_index_plus_one;
	while ((nb_rep_need < count) && rep->playback.enhancement_rep_index_plus_one) {
		nb_rep_need++;
		if (next_rep_index_plus_one == group->max_complementary_rep_index+1)
			break;
		rep = gf_list_get(group->adaptation_set->representations, next_rep_index_plus_one-1);
		next_rep_index_plus_one = rep->playback.enhancement_rep_index_plus_one;
	}

	assert(nb_rep_need <= count);

	return nb_rep_need;
}

static
u32 gf_dash_check_mpd_root_type(const char *local_url)
{
	if (local_url) {
		char *rtype = gf_xml_get_root_type(local_url, NULL);
		if (rtype) {
			u32 handled = 0;
			if (!strcmp(rtype, "MPD")) {
				handled = 1;
			}
			else if (!strcmp(rtype, "SmoothStreamingMedia")) {
				handled = 2;
			}
			gf_free(rtype);
			return handled;
		}
	}
	return GF_FALSE;
}

static Bool gf_dash_get_date(GF_DashClient *dash, char *scheme_id, char *url, u64 *utc)
{
	GF_DASHFileIOSession session;
	GF_Err e;
	u8 *data;
	u32 len;
	Bool res = GF_TRUE;
	const char *cache_name;
	*utc = 0;

	//unsupported schemes
	if (!strcmp(scheme_id, "urn:mpeg:dash:utc:ntp:2014")) return GF_FALSE;
	if (!strcmp(scheme_id, "urn:mpeg:dash:utc:sntp:2014")) return GF_FALSE;

	if (!dash->dash_io) return GF_FALSE;

	session = dash->dash_io->create(dash->dash_io, GF_FALSE, url, -2);
	if (!session) return GF_FALSE;
	e = dash->dash_io->run(dash->dash_io, session);
	if (e) {
		dash->dash_io->del(dash->dash_io, session);
		return GF_FALSE;
	}
	cache_name = dash->dash_io->get_cache_name(dash->dash_io, session);
	gf_blob_get(cache_name, &data, &len, NULL);

	if (!strcmp(scheme_id, "urn:mpeg:dash:utc:http-head:2014")) {
		const char *hdr = dash->dash_io->get_header_value(dash->dash_io, session, "Date");
		if (hdr)
			*utc = gf_net_parse_date(hdr);
		else
			res = GF_FALSE;
	}
	else if (!data) {
		res = GF_FALSE;
	} else {
		if (!strcmp(scheme_id, "urn:mpeg:dash:utc:http-xsdate:2014")) {
			*utc = gf_mpd_parse_date(data);
		}
		else if (!strcmp(scheme_id, "urn:mpeg:dash:utc:http-iso:2014")) {
			*utc = gf_net_parse_date(data);
		}
		else if (!strcmp(scheme_id, "urn:mpeg:dash:utc:http-ntp:2014")) {
			u64 ntp_ts;
			if (sscanf((char *) data, LLU, &ntp_ts) == 1) {
				//ntp value not counted since 1900, assume format is seconds till 1 jan 1970
				if (ntp_ts<=GF_NTP_SEC_1900_TO_1970) {
					*utc = ntp_ts*1000;
				} else {
					*utc = gf_net_ntp_to_utc(ntp_ts);
				}
			} else {
				res = GF_FALSE;
			}
		}
	}
    gf_blob_release(cache_name);
    
	dash->dash_io->del(dash->dash_io, session);
	return res;
}

GF_Err gf_dash_download_resource(GF_DashClient *dash, GF_DASHFileIOSession *sess, const char *url, u64 start_range, u64 end_range, u32 persistent_mode, GF_DASH_Group *group);

static void gf_dash_group_timeline_setup_single(GF_MPD *mpd, GF_DASH_Group *group, u64 fetch_time)
{
	GF_MPD_SegmentTimeline *timeline = NULL;
	GF_MPD_Representation *rep = NULL;
	GF_MPD_Descriptor *utc_timing = NULL;
	const char *val;
	u32 shift, timescale;
	u64 current_time, current_time_no_timeshift, availabilityStartTime;
	u32 ast_diff, start_number;
	Double ast_offset = 0;

	if (mpd->type==GF_MPD_TYPE_STATIC) {
		if (group->dash->route_clock_state)
			goto setup_route;
		return;
	}

	//always init clock even if active period is a remote one
#if 0
	if (group->period->origin_base_url && (group->period->type != GF_MPD_TYPE_DYNAMIC))
		return;
#endif

	/*M3U8 does not use NTP sync, we solve edge while loading subplaylist */
	if (group->dash->is_m3u8) {
		return;
	}

	if (group->dash->is_smooth) {
		u32 seg_idx = 0;
		u64 timeshift = 0;
		if (group->dash->initial_time_shift_value && ((s32) mpd->time_shift_buffer_depth>=0)) {
			if (group->dash->initial_time_shift_value<=100) {
				timeshift = mpd->time_shift_buffer_depth;
				timeshift *= group->dash->initial_time_shift_value;
				timeshift /= 100;
			} else {
				timeshift = (u32) group->dash->initial_time_shift_value;
				if (timeshift > mpd->time_shift_buffer_depth) timeshift = mpd->time_shift_buffer_depth;
			}
			timeshift = mpd->time_shift_buffer_depth - timeshift;
		}

		if (!timeshift && group->adaptation_set->smooth_max_chunks) {
			seg_idx = group->adaptation_set->smooth_max_chunks;
		} else {
			u32 i, count;
			u64 start = 0;
			u64 cumulated_dur = 0;
			GF_MPD_SegmentTimeline *stl;
			if (!group->adaptation_set->segment_template || !group->adaptation_set->segment_template->segment_timeline)
				return;

			timeshift *= group->adaptation_set->segment_template->timescale;
			timeshift /= 1000;
			stl = group->adaptation_set->segment_template->segment_timeline;
			count = gf_list_count(stl->entries);
			for (i=0; i<count; i++) {
				u64 dur;
				GF_MPD_SegmentTimelineEntry *e = gf_list_get(stl->entries, i);
				if (!e->duration)
					continue;
				if (e->start_time)
					start = e->start_time;

				dur = e->duration * (e->repeat_count+1);
				if (cumulated_dur + dur >= timeshift) {
					u32 nb_segs = (u32) ( (timeshift - cumulated_dur) / e->duration );
					seg_idx += nb_segs;
					break;
				}
				cumulated_dur += dur;
				start += dur;
				seg_idx += e->repeat_count+1;
				if (group->adaptation_set->smooth_max_chunks && (seg_idx>=group->adaptation_set->smooth_max_chunks)) {
					seg_idx = group->adaptation_set->smooth_max_chunks;
					break;
				}
			}
		}
		group->download_segment_index = (seg_idx>1) ? seg_idx - 1 : 0;
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Smooth group tuned in at segment %d\n", group->download_segment_index));
		return;
	}

	if (group->broken_timing )
		return;


	/*if no AST, do not use NTP sync */
	if (! group->dash->mpd->availabilityStartTime) {
		group->broken_timing = GF_TRUE;
		return;
	}

	if ((group->dash->preroll_state<2) && group->dash->initial_period_tunein && gf_list_count(group->dash->mpd->periods)) {
		GF_MPD_Period *ap = gf_list_get(group->dash->mpd->periods, 0);
		if (ap->is_preroll) {
			group->dash->preroll_state = 1;
		}
	}
	if (group->dash->preroll_state == 1) {
		group->broken_timing = GF_TRUE;
		return;
	}

	//if ROUTE and clock not setup, do it
setup_route:
	val = group->dash->dash_io->get_header_value(group->dash->dash_io, group->dash->mpd_dnload, "x-route");
	if (val && !group->dash->utc_drift_estimate) {
		u32 i;
		GF_MPD_Period *dyn_period=NULL;
		u32 found = 0;
		u64 timeline_offset_ms=0;
		if (!group->dash->route_clock_state) {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Detected ROUTE DASH service ID %s\n", val));
			group->dash->route_clock_state = 1;
		}

		for (i=0; i<gf_list_count(group->dash->mpd->periods); i++) {
			dyn_period = gf_list_get(group->dash->mpd->periods, i);
			if (!dyn_period->xlink_href && !dyn_period->origin_base_url) break;
			if (dyn_period->xlink_href && !dyn_period->origin_base_url && gf_list_count(dyn_period->adaptation_sets) ) break;
			dyn_period = NULL;
		}
		if (!dyn_period) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] ROUTE with no dynamic period, cannot init clock yet\n"));
			return;
		}

		//for m3u8 we force refreshing the root manifest, because the download session might be tuned on a child playlist
		//which will not have the x-route-first-seg set
		if (group->dash->is_m3u8) {
			gf_dash_download_resource(group->dash, &(group->dash->mpd_dnload), group->dash->base_url, 0, 0, 1, NULL);
		}
		val = group->dash->dash_io->get_header_value(group->dash->dash_io, group->dash->mpd_dnload, "x-route-first-seg");
		if (!val) {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Waiting for ROUTE clock ...\n"));
			return;
		}

		for (i=0; i<gf_list_count(dyn_period->adaptation_sets); i++) {
			u64 sr, seg_dur;
			u32 j, len, nb_space=0;
			GF_MPD_AdaptationSet *set;
			char *sep, *start, *end, *seg_url = NULL;

			set = gf_list_get(dyn_period->adaptation_sets, i);
			for (j=0; j<gf_list_count(set->representations); j++) {
				u64 dur = dyn_period->duration;
				rep = gf_list_get(set->representations, j);

				dyn_period->duration = 0;

				if (group->dash->is_m3u8) {
					u32 k, count;
					if (found) break;
					if (!rep->segment_list)
						continue;
					count = gf_list_count(rep->segment_list->segment_URLs);
					for (k=0; k<count; k++) {
						GF_MPD_SegmentURL *surl = gf_list_get(rep->segment_list->segment_URLs, k);
						if (surl->media && strstr(surl->media, val)) {
							found = k+1;
							break;
						}
					}
					continue;
				}

				gf_mpd_resolve_url(group->dash->mpd, rep, set, dyn_period, "./", 0, GF_MPD_RESOLVE_URL_MEDIA_NOSTART, 9876, 0, &seg_url, &sr, &sr, &seg_dur, NULL, NULL, NULL, NULL);

				dyn_period->duration = dur;

				sep = seg_url ? strstr(seg_url, "987") : NULL;
				if (!sep) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Failed to resolve template for segment #9876 on rep #%d\n", j+1));
					if (seg_url) gf_free(seg_url);
					continue;
				}
				start = sep;
				end = sep+4;
				while (start>seg_url && (*(start-1)=='0')) { start--; nb_space++;}
				start[0]=0;
				len = (u32) strlen(seg_url)-2;
				if (!strncmp(val, seg_url+2, len)) {
					u32 number=0;
					char szTemplate[100];

					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Resolve ROUTE clock on bootstrap segment URL %s template %s\n", val, seg_url+2));

					strcpy(szTemplate, seg_url+2);
					strcat(szTemplate, "%");
					if (nb_space) {
						char szFmt[20];
						sprintf(szFmt, "0%d", nb_space+4);
						strcat(szTemplate, szFmt);
					}
					strcat(szTemplate, "d");
					strcat(szTemplate, end);
					if (sscanf(val, szTemplate, &number) == 1) {
						u32 startNum = 1;
						if (dyn_period->segment_template) startNum = dyn_period->segment_template->start_number;
						if (set->segment_template) startNum = set->segment_template->start_number;
						if (rep->segment_template) startNum = rep->segment_template->start_number;
						if (number>=startNum) {
                            //clock is init which means the segment is available, so the timeline offset must match the AST of the segment (includes seg dur)

                            const char *ll_val = group->dash->dash_io->get_header_value(group->dash->dash_io, group->dash->mpd_dnload, "x-route-ll");
                            if (ll_val && !strcmp(ll_val, "yes")) {
                                //low latency case, we are currently receiving the segment
                                group->dash->route_low_latency = GF_TRUE;
                                number--;
                            }
                            
                            timeline_offset_ms = seg_dur * ( 1 + number - startNum);
						}
						found = 1;
					}
				} else {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] ROUTE bootstrap segment URL %s does not match template %s for rep #%d\n", val, seg_url+2, j+1));
				}
				gf_free(seg_url);
				if (found) break;
			}
			if (found) break;
		}
		if (found) {
			if (group->dash->is_m3u8) {
				//purge segments (we assume we roughly are in the same state on all child playlists, we could keep one for safety)
				for (i=0; i<gf_list_count(dyn_period->adaptation_sets); i++) {
					u32 j;
					GF_MPD_AdaptationSet *set = gf_list_get(dyn_period->adaptation_sets, i);
					for (j=0; j<gf_list_count(set->representations); j++) {
						u32 to_rem;
						rep = gf_list_get(set->representations, j);
						if (!rep->segment_list) continue;
						to_rem = found-1;
						while (to_rem) {
							GF_MPD_SegmentURL *surl = gf_list_pop_front(rep->segment_list->segment_URLs);
							gf_mpd_segment_url_free(surl);
							to_rem--;
						}
					}
				}
			} else {
				//adjust so that nb_seg = current_time/segdur = (fetch-ast)/seg_dur;
				// = (fetch- ( mpd->availabilityStartTime + group->dash->utc_shift + group->dash->utc_drift_estimate) / segdur;
				//hence nb_seg*seg_dur = fetch - mpd->availabilityStartTime - group->dash->utc_shift - group->dash->utc_drift_estimate
				//so group->dash->utc_drift_estimate = fetch - (mpd->availabilityStartTime + nb_seg*seg_dur)


				u64 utc = mpd->availabilityStartTime + dyn_period->start + timeline_offset_ms;
				group->dash->utc_drift_estimate = ((s64) fetch_time - (s64) utc);
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Estimated UTC diff of ROUTE broadcast "LLD" ms (UTC fetch "LLU" - server UTC "LLU" - MPD AST "LLU" - MPD PublishTime "LLU" - bootstraping on segment %s\n", group->dash->utc_drift_estimate, fetch_time, utc, group->dash->mpd->availabilityStartTime, group->dash->mpd->publishTime, val));
			}
			group->dash->route_clock_state = 2;
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Failed to setup ROUTE clock from segment template with bootstrap URL %s, using NTP\n", val));
			group->dash->route_clock_state = 3;
		}
		if (mpd->type==GF_MPD_TYPE_STATIC) {
			if (found)
				group->dash->route_skip_segments_ms = (u32) timeline_offset_ms;
			group->timeline_setup = GF_TRUE;
			return;
		}
	}
	else if (val) {
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] ROUTE clock already setup - UTC diff of ROUTE broadcast "LLD" ms\n", group->dash->utc_drift_estimate));
	} else {
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] No ROUTE entity on HTTP request\n"));
	}

	if (!group->dash->route_clock_state || (group->dash->route_clock_state>2)) {
		GF_MPD_ProducerReferenceTime *pref = gf_list_get(group->adaptation_set->producer_reference_time, 0);
		if (pref)
			utc_timing = pref->utc_timing;
		if (!utc_timing)
			utc_timing = gf_list_get(group->dash->mpd->utc_timings, 0);
	}

	if (utc_timing && utc_timing->scheme_id_uri) {
		Bool res = GF_FALSE;
		u64 utc=0;
		s64 drift_estimate;

		if (!strcmp(utc_timing->scheme_id_uri, "urn:mpeg:dash:utc:direct:2014")) {
			utc = gf_net_parse_date(utc_timing->value);
			res = GF_TRUE;
		} else {
			char *time_refs = utc_timing->value;
			utc = 0;
			while (time_refs) {
				char *sep = strchr(time_refs, ' ');
				if (sep) sep[0] = 0;

				res = gf_dash_get_date(group->dash, utc_timing->scheme_id_uri, time_refs, &utc);

				time_refs = NULL;
				if (sep) {
					sep[0] = ' ';
					time_refs = sep+1;
				}
				if (res) break;
			}
		}
		if (res) {
			drift_estimate = ((s64) fetch_time - (s64) utc);
			group->dash->utc_drift_estimate = drift_estimate;
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Estimated UTC diff between client and server (%s): "LLD" ms (UTC fetch "LLU" - server UTC "LLU" - MPD AST "LLU" - MPD PublishTime "LLU"\n", utc_timing->value, group->dash->utc_drift_estimate, fetch_time, utc,
				group->dash->mpd->availabilityStartTime, group->dash->mpd->publishTime));
		} else {
			utc_timing = NULL;
		}
	}

	if ((!group->dash->route_clock_state || (group->dash->route_clock_state>2))
		&& !group->dash->ntp_forced
		&& group->dash->estimate_utc_drift
		&& !group->dash->utc_drift_estimate
		&& group->dash->mpd_dnload
		&& group->dash->dash_io->get_header_value
		&& !utc_timing
	) {
		val = group->dash->dash_io->get_header_value(group->dash->dash_io, group->dash->mpd_dnload, "Server-UTC");
		if (val) {
			u64 utc;
			sscanf(val, LLU, &utc);
			group->dash->utc_drift_estimate = ((s64) fetch_time - (s64) utc);
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Estimated UTC diff between client and server "LLD" ms (UTC fetch "LLU" - server UTC "LLU" - MPD AST "LLU" - MPD PublishTime "LLU"\n", group->dash->utc_drift_estimate, fetch_time, utc, group->dash->mpd->availabilityStartTime, group->dash->mpd->publishTime));
		} else {
			s64 drift_estimate = 0;
			u64 utc = 0;
			val = group->dash->dash_io->get_header_value(group->dash->dash_io, group->dash->mpd_dnload, "Date");
			if (val)
				utc = gf_net_parse_date(val);
			if (utc)
				drift_estimate = ((s64) fetch_time - (s64) utc);

			//HTTP date is in second - if the clock diff is less than 1 sec, we cannot infer anything
			if (ABS(drift_estimate) > 1000) {
				group->dash->utc_drift_estimate = 1 + drift_estimate;
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Estimated UTC diff between client and server "LLD" ms (UTC fetch "LLU" - server UTC "LLU" - MPD AST "LLU" - MPD PublishTime "LLU"\n", group->dash->utc_drift_estimate, fetch_time, utc, group->dash->mpd->availabilityStartTime, group->dash->mpd->publishTime));
			} else {
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] No UTC diff between client and server (UTC fetch "LLU" - server UTC "LLU" - MPD AST "LLU" - MPD PublishTime "LLU"\n", fetch_time, utc, group->dash->mpd->availabilityStartTime, group->dash->mpd->publishTime));
				group->dash->utc_drift_estimate = 1;
			}
		}
	}

	availabilityStartTime = 0;
	if ((s64) mpd->availabilityStartTime + group->dash->utc_shift > (s64) - group->dash->utc_drift_estimate) {
		availabilityStartTime = mpd->availabilityStartTime + group->dash->utc_shift + group->dash->utc_drift_estimate;
	}


#ifdef FORCE_DESYNC
	availabilityStartTime -= FORCE_DESYNC;
#endif

	ast_diff = (u32) (availabilityStartTime - group->dash->mpd->availabilityStartTime);
	current_time = fetch_time;

	if (current_time < availabilityStartTime) {
		//if more than 1 sec consider we have a pb
		if (availabilityStartTime - current_time >= 1000) {
			Bool broken_timing = GF_TRUE;
#ifndef _WIN32_WCE
			time_t gtime1, gtime2;
			struct tm *t1, *t2;
			gtime1 = current_time / 1000;
			t1 = gf_gmtime(&gtime1);
			gtime2 = availabilityStartTime / 1000;
			t2 = gf_gmtime(&gtime2);
			if (t1 == t2) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Slight drift in UTC clock at time %d-%02d-%02dT%02d:%02d:%02dZ: diff AST - now %d ms\n", 1900+t1->tm_year, t1->tm_mon+1, t1->tm_mday, t1->tm_hour, t1->tm_min, t1->tm_sec, (s32) (availabilityStartTime - current_time) ));
				current_time = 0;
				broken_timing = GF_FALSE;
			}
			else if (t1 && t2) {
				t1->tm_year = t2->tm_year;
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error in UTC clock: current time %d-%02d-%02dT%02d:%02d:%02dZ is less than AST %d-%02d-%02dT%02d:%02d:%02dZ - diff AST-now %d ms\n",
				                                   1900+t1->tm_year, t1->tm_mon+1, t1->tm_mday, t1->tm_hour, t1->tm_min, t1->tm_sec,
				                                   1900+t2->tm_year, t2->tm_mon+1, t2->tm_mday, t2->tm_hour, t2->tm_min, t2->tm_sec,
				                                   (u32) (availabilityStartTime - current_time)
				                                  ));
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error in UTC clock: could not retrieve time!\n"));
			}

#endif
			if (broken_timing) {
				if (group->dash->utc_shift + group->dash->utc_drift_estimate > 0) {
					availabilityStartTime = current_time;
				} else {
					group->broken_timing = GF_TRUE;
					return;
				}
			}
		} else {
			availabilityStartTime = current_time;
			current_time = 0;
		}
	}
	else current_time -= availabilityStartTime;

	if (gf_list_count(group->dash->mpd->periods)) {
		u64 seg_start_ms = current_time;
		u64 seg_end_ms = (u64) (seg_start_ms + group->segment_duration*1000);
		u32 i;
		u64 start = 0;
		for (i=0; i<gf_list_count(group->dash->mpd->periods); i++) {
			GF_MPD_Period *ap = gf_list_get(group->dash->mpd->periods, i);
			if (ap->is_preroll) continue;

			if (ap->start) start = ap->start;

			if (group->dash->initial_period_tunein
				&& (seg_start_ms>=ap->start)
				&& (!ap->duration || (seg_end_ms<=start + ap->duration))
			) {
				if (i != group->dash->active_period_index) {
					group->dash->reinit_period_index = 1+i;
					group->dash->start_range_period = (Double) seg_start_ms;
					group->dash->start_range_period -= ap->start;
					group->dash->start_range_period /= 1000;
					return;
				}
			}

			if (!ap->duration) break;
			start += ap->duration;
		}
	}

	//compute current time in period
	if (current_time < group->period->start) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Period will start in %d ms\n", group->period->start - current_time));
		current_time = 0;
	} else {
		if (group->dash->initial_period_tunein || group->force_timeline_reeval) {
			current_time -= group->period->start;
		} else {
			//initial period was setup, consider we are moving to a new period, so time in this period is 0
			current_time = 0;
			if (group->start_playback_range) current_time = (u64) (group->start_playback_range*1000);
		}
	}

	current_time_no_timeshift = current_time;
	if ( ((s32) mpd->time_shift_buffer_depth>=0)) {

		if (group->dash->initial_time_shift_value) {
			if (group->dash->initial_time_shift_value<=100) {
				shift = mpd->time_shift_buffer_depth;
				shift *= group->dash->initial_time_shift_value;
				shift /= 100;
			} else {
				shift = (u32) group->dash->initial_time_shift_value;
				if (shift > mpd->time_shift_buffer_depth) shift = mpd->time_shift_buffer_depth;
			}

			if (current_time < shift) current_time = 0;
			else current_time -= shift;
		}
	}
	group->dash->time_in_tsb = group->dash->prev_time_in_tsb = 0;

	timeline = NULL;
	timescale=1;
	start_number=0;
	rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);

	if (group->period->segment_list) {
		if (group->period->segment_list->segment_timeline) timeline = group->period->segment_list->segment_timeline;
		if (group->period->segment_list->timescale) timescale = group->period->segment_list->timescale;
		if (group->period->segment_list->start_number) start_number = group->period->segment_list->start_number;
		if (group->period->segment_list->availability_time_offset) ast_offset = group->period->segment_list->availability_time_offset;
	}
	if (group->adaptation_set->segment_list) {
		if (group->adaptation_set->segment_list->segment_timeline) timeline = group->adaptation_set->segment_list->segment_timeline;
		if (group->adaptation_set->segment_list->timescale) timescale = group->adaptation_set->segment_list->timescale;
		if (group->adaptation_set->segment_list->start_number) start_number = group->adaptation_set->segment_list->start_number;
		if (group->adaptation_set->segment_list->availability_time_offset) ast_offset = group->adaptation_set->segment_list->availability_time_offset;
	}
	if (rep->segment_list) {
		if (rep->segment_list->segment_timeline) timeline = rep->segment_list->segment_timeline;
		if (rep->segment_list->timescale) timescale = rep->segment_list->timescale;
		if (rep->segment_list->start_number) start_number = rep->segment_list->start_number;
		if (rep->segment_list->availability_time_offset) ast_offset = rep->segment_list->availability_time_offset;
	}

	if (group->period->segment_template) {
		if (group->period->segment_template->segment_timeline) timeline = group->period->segment_template->segment_timeline;
		if (group->period->segment_template->timescale) timescale = group->period->segment_template->timescale;
		if (group->period->segment_template->start_number) start_number = group->period->segment_template->start_number;
		if (group->period->segment_template->availability_time_offset) ast_offset = group->period->segment_template->availability_time_offset;
	}
	if (group->adaptation_set->segment_template) {
		if (group->adaptation_set->segment_template->segment_timeline) timeline = group->adaptation_set->segment_template->segment_timeline;
		if (group->adaptation_set->segment_template->timescale) timescale = group->adaptation_set->segment_template->timescale;
		if (group->adaptation_set->segment_template->start_number) start_number = group->adaptation_set->segment_template->start_number;
		if (group->adaptation_set->segment_template->availability_time_offset) ast_offset = group->adaptation_set->segment_template->availability_time_offset;
	}
	if (rep->segment_template) {
		if (rep->segment_template->segment_timeline) timeline = rep->segment_template->segment_timeline;
		if (rep->segment_template->timescale) timescale = rep->segment_template->timescale;
		if (rep->segment_template->start_number) start_number = rep->segment_template->start_number;
		if (rep->segment_template->availability_time_offset) ast_offset = rep->segment_template->availability_time_offset;
	}

	group->is_low_latency = GF_FALSE;
	if (group->dash->low_latency_mode==GF_DASH_LL_DISABLE) {
		ast_offset = 0;
	} else if (ast_offset>0) {
		group->is_low_latency = GF_TRUE;
	}
	if (timeline) {
		u64 start_segtime = 0;
		u64 segtime = 0;
		u64 current_time_rescale;
		u64 timeline_duration = 0;
		u32 count;
		u64 last_s_dur=0;
		u32 i, seg_idx = 0;

		current_time_rescale = current_time;
		current_time_rescale *= timescale;
		current_time_rescale /= 1000;

		count = gf_list_count(timeline->entries);
		for (i=0; i<count; i++) {
			GF_MPD_SegmentTimelineEntry *ent = gf_list_get(timeline->entries, i);

			if (!i && (current_time_rescale + ent->duration < ent->start_time)) {
				current_time_rescale = current_time_no_timeshift * timescale / 1000;
			}
			timeline_duration += (1+ent->repeat_count)*ent->duration;

			if (i+1 == count) timeline_duration -= ent->duration;
			last_s_dur=ent->duration;
		}


		if (!group->dash->mpd->minimum_update_period) {
			last_s_dur *= 1000;
			last_s_dur /= timescale;
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] dynamic MPD but no update period specified and SegmentTimeline used - will use segment duration %d ms as default update rate\n", last_s_dur));

			group->dash->mpd->minimum_update_period = (u32) last_s_dur;
		}

		for (i=0; i<count; i++) {
			u32 repeat;
			GF_MPD_SegmentTimelineEntry *ent = gf_list_get(timeline->entries, i);
			if (!segtime) {
				start_segtime = segtime = ent->start_time;

				//if current time is before the start of the previous segment, consider our timing is broken
				if (current_time_rescale + ent->duration < segtime) {
					GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] current time "LLU" is before start time "LLU" of first segment in timeline (timescale %d) by %g sec - using first segment as starting point\n", current_time_rescale, segtime, timescale, (segtime-current_time_rescale)*1.0/timescale));
					group->download_segment_index = seg_idx;
					group->nb_segments_in_rep = count;
					group->start_playback_range = (segtime)*1.0/timescale;
					group->ast_at_init = availabilityStartTime;
					group->ast_offset = (u32) (ast_offset*1000);
					group->broken_timing = GF_TRUE;
					return;
				}
			}

			repeat = 1+ent->repeat_count;
			while (repeat) {
				if ((current_time_rescale >= segtime) && (current_time_rescale < segtime + ent->duration)) {
					GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Found segment %d for current time "LLU" is in SegmentTimeline ["LLU"-"LLU"] (timecale %d - current index %d - startNumber %d)\n", seg_idx, current_time_rescale, start_segtime, segtime + ent->duration, timescale, group->download_segment_index, start_number));

					group->download_segment_index = seg_idx;
					group->nb_segments_in_rep = seg_idx + count - i;
					group->start_playback_range = (current_time)/1000.0;
					group->ast_at_init = availabilityStartTime;
					group->ast_offset = (u32) (ast_offset*1000);

					//to remove - this is a hack to speedup starting for some strange MPDs which announce the live point as the first segment but have already produced the complete timeline
					if (group->dash->utc_drift_estimate<0) {
						group->ast_at_init -= (timeline_duration - (segtime-start_segtime)) *1000/timescale;
					}
					return;
				}
				segtime += ent->duration;
				repeat--;
				seg_idx++;
				last_s_dur=ent->duration;
			}
		}
		//check if we're ahead of time but "reasonnably" ahead (max 1 min) - otherwise consider the timing is broken
		if ((current_time_rescale + last_s_dur >= segtime) && (current_time_rescale <= segtime + 60*timescale)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] current time "LLU" is greater than last SegmentTimeline end "LLU" - defaulting to last entry in SegmentTimeline\n", current_time_rescale, segtime));
			group->download_segment_index = seg_idx-1;
			group->nb_segments_in_rep = seg_idx;
			//we can't trust our UTC check, play from last segment with start_range=0 (eg from start of first segment)
			group->start_playback_range = 0;

			group->ast_at_init = availabilityStartTime;
			group->ast_offset = (u32) (ast_offset*1000);
			//force an update in half the target period
			group->dash->last_update_time = gf_sys_clock() + group->dash->mpd->minimum_update_period/2;
		} else {
			//NOT FOUND !!
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] current time "LLU" is NOT in SegmentTimeline ["LLU"-"LLU"] - cannot estimate current startNumber, default to 0 ...\n", current_time_rescale, start_segtime, segtime));
			group->download_segment_index = 0;
			group->nb_segments_in_rep = 10;
			group->broken_timing = GF_TRUE;
		}

		return;
	}

	if (group->segment_duration) {
		u32 nb_segs_in_update = (u32) (mpd->minimum_update_period / (1000*group->segment_duration) );
		Double nb_seg = (Double) current_time;
		nb_seg /= 1000;
		nb_seg /= group->segment_duration;
		shift = (u32) nb_seg;

		if ((group->dash->route_clock_state == 2) && shift) {
			//shift currently points to the next segment after the one used for clock bootstrap
            if (!group->dash->route_low_latency)
                shift--;
            //avoid querying too early the cache since segments do not usually arrive exactly on time ...
			availabilityStartTime += group->dash->route_ast_shift;
		}

		if (group->dash->initial_period_tunein || group->force_timeline_reeval) {
			u64 seg_start_ms, seg_end_ms;
			if (group->force_timeline_reeval) {
				group->start_number_at_last_ast = 0;
				group->force_timeline_reeval = GF_FALSE;
			}
			seg_start_ms = (u64) (group->segment_duration * (shift+start_number) * 1000);
			seg_end_ms = (u64) (seg_start_ms + group->segment_duration*1000);
			//we are in the right period
			if (seg_start_ms>=group->period->start && (!group->period->duration || (seg_end_ms<=group->period->start+group->period->duration)) ) {
			} else {
				u32 i;
				u64 start = 0;
				for (i=0; i<gf_list_count(group->dash->mpd->periods); i++) {
					GF_MPD_Period *ap = gf_list_get(group->dash->mpd->periods, i);
					if (ap->start) start = ap->start;

					if ((seg_start_ms>=ap->start) && (!ap->duration || (seg_end_ms<=start + ap->duration))) {
						group->dash->reinit_period_index = 1+i;
						group->dash->start_range_period = (Double) seg_start_ms;
						group->dash->start_range_period -= ap->start;
						group->dash->start_range_period /= 1000;
						return;
					}

					if (!ap->duration) break;
					start += ap->duration;
				}
			}
		}

		//not time shifting, we are at the live edge, we must stick to start of segment otherwise we won't have enough data to play until next segment is ready

		if (!group->dash->initial_time_shift_value) {
			Double time_in_seg;
			//by default playback starts at beginning of segment
			group->start_playback_range = shift * group->segment_duration;

			time_in_seg = (Double) current_time/1000.0;
			time_in_seg -= group->start_playback_range;

			//if low latency, try to adjust
			if (ast_offset) {
				Double ast_diff_d;
				if (ast_offset>group->segment_duration) ast_offset = group->segment_duration;
				ast_diff_d = group->segment_duration - ast_offset;

				//we assume that in low latency mode, chunks are made available every (group->segment_duration - ast_offset)
				//we need to seek such that the remaining time R satisfies now + R = NextSegAST
				//hence S(n) + ms_in_seg + R = S(n+1) + Aoffset
				//which gives us R = S(n+1) + Aoffset - S(n) - ms_in_seg = D + Aoffset - ms_in_seg
				//seek = D - R = D - (D + Aoffset - ms_in_seg) = ms_in_seg - Ao
				if (time_in_seg > ast_diff_d) {
					group->start_playback_range += time_in_seg - ast_diff_d;
				}
			}
		} else {
			group->start_playback_range = (Double) current_time / 1000.0;
		}

		if (!group->start_number_at_last_ast) {
			group->download_segment_index = shift;
			group->start_number_at_last_ast = start_number;

			group->ast_at_init = availabilityStartTime;
			group->ast_offset = (u32) (ast_offset*1000);
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] AST at init "LLD"\n", group->ast_at_init));

			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] At current time "LLD" ms: Initializing Timeline: startNumber=%d segmentNumber=%d segmentDuration=%f - %.03f seconds in segment (start range %g)\n", current_time, start_number, shift, group->segment_duration, group->start_playback_range ? group->start_playback_range - shift*group->segment_duration : 0, group->start_playback_range));
		} else {
			group->download_segment_index += start_number;
			if (group->download_segment_index > group->start_number_at_last_ast) {
				group->download_segment_index -= group->start_number_at_last_ast;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] At current time %d ms: Updating Timeline: startNumber=%d segmentNumber=%d downloadSegmentIndex=%d segmentDuration=%g AST_diff=%d\n", current_time, start_number, shift, group->download_segment_index, group->segment_duration, ast_diff));
			} else {
				group->download_segment_index = shift;
				group->ast_at_init = availabilityStartTime;
				group->ast_offset = (u32) (ast_offset*1000);
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] At current time "LLU" ms: Re-Initializing Timeline: startNumber=%d segmentNumber=%d segmentDuration=%g AST_diff=%d\n", current_time, start_number, shift, group->segment_duration, ast_diff));
			}
			group->start_number_at_last_ast = start_number;
		}
		if (group->nb_segments_in_rep) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] UTC time indicates first segment in period is %d, MPD indicates %d segments are available\n", group->download_segment_index , group->nb_segments_in_rep));
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] UTC time indicates first segment in period is %d\n", group->download_segment_index));
		}

		if (group->nb_segments_in_rep && (group->download_segment_index + nb_segs_in_update > group->nb_segments_in_rep)) {
			if (group->download_segment_index < (s32)group->nb_segments_in_rep) {

			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Not enough segments (%d needed vs %d indicated) to reach period endTime indicated in MPD - ignoring MPD duration\n", nb_segs_in_update, group->nb_segments_in_rep - group->download_segment_index ));
				group->nb_segments_in_rep = shift + nb_segs_in_update;
				group->dash->ignore_mpd_duration = GF_TRUE;
			}
		}
		group->prev_segment_ok = GF_TRUE;
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Segment duration unknown - cannot estimate current startNumber\n"));
	}
}

static void gf_dash_group_timeline_setup(GF_MPD *mpd, GF_DASH_Group *group, u64 fetch_time)
{
	u32 i;

	if (!fetch_time) {
		//when we initialize the timeline without an explicit fetch time, use our local clock - this allows for better precision
		//when trying to locate the live edge
		fetch_time = gf_net_get_utc();
	}
	gf_dash_group_timeline_setup_single(mpd, group, fetch_time);

	//also init all dependend groups for the same time
	for (i=0; i<gf_list_count(group->groups_depending_on); i++) {
		GF_DASH_Group *d_grp = gf_list_get(group->groups_depending_on, i);
		gf_dash_group_timeline_setup_single(mpd, d_grp, fetch_time);
		d_grp->timeline_setup = GF_TRUE;
	}
}
/*!
* Returns true if mime type of a given URL is an M3U8 mime-type
\param url The url to check
\param mime The mime-type to check
\return true if mime-type is OK for M3U8
*/
static Bool gf_dash_is_m3u8_mime(const char *url, const char * mime) {
	u32 i;
	if (!url || !mime)
		return GF_FALSE;
	if (strstr(url, ".mpd") || strstr(url, ".MPD"))
		return GF_FALSE;

	for (i = 0 ; GF_DASH_M3U8_MIME_TYPES[i] ; i++) {
		if ( !stricmp(mime, GF_DASH_M3U8_MIME_TYPES[i]))
			return GF_TRUE;
	}
	return GF_FALSE;
}

GF_EXPORT
GF_Err gf_dash_group_check_bandwidth(GF_DashClient *dash, u32 group_idx, u32 bits_per_sec, u64 total_bytes, u64 bytes_done, u64 us_since_start)
{
	s32 res;
	GF_DASH_Group *group = gf_list_get(dash->groups, group_idx);
	if (!group) return GF_BAD_PARAM;

	if (! dash->rate_adaptation_download_monitor) return GF_OK;
	//do not abort if other groups depend on this one
	if (group->groups_depending_on) return GF_OK;
	if (group->dash->disable_switching) return GF_OK;
	if (!total_bytes || !bytes_done || !bits_per_sec) return GF_OK;
	if (total_bytes == bytes_done) return GF_OK;

	//force a call go query buffer
	dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_CODEC_STAT_QUERY, group_idx, GF_OK);

	res = dash->rate_adaptation_download_monitor(dash, group, bits_per_sec, total_bytes, bytes_done, us_since_start, group->buffer_occupancy_ms, (u32) group->current_downloaded_segment_duration);

	if (res==-1) return GF_OK;

	dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_ABORT_DOWNLOAD, group->groups_idx, GF_OK);

	//internal return value, switching has already been setup
	if (res<0) return GF_OK;

	group->force_segment_switch = GF_TRUE;
	group->force_representation_idx_plus_one = (u32) res + 1;
	return GF_OK;
}

/*!
* Download a file with possible retry if GF_IP_CONNECTION_FAILURE|GF_IP_NETWORK_FAILURE
* (I discovered that with my WIFI connection, I had many issues with BFM-TV downloads)
* Similar to gf_service_download_new() and gf_dm_sess_process().
* Parameters are identical to the ones of gf_service_download_new.
* \see gf_service_download_new()
*/
GF_Err gf_dash_download_resource(GF_DashClient *dash, GF_DASHFileIOSession *sess, const char *url, u64 start_range, u64 end_range, u32 persistent_mode, GF_DASH_Group *group)
{
	s32 group_idx = -1;
	Bool had_sess = GF_FALSE;
	Bool retry = GF_TRUE;
	GF_Err e;
	GF_DASHFileIO *dash_io = dash->dash_io;

	if (!dash_io) return GF_BAD_PARAM;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Downloading %s starting at UTC "LLU" ms\n", url, gf_net_get_utc() ));

	if (group) {
		group_idx = group->groups_idx;
	}

	if (! *sess) {
		*sess = dash_io->create(dash_io, persistent_mode ? 1 : 0, url, group_idx);
		if (!(*sess)) {
			if (dash->route_clock_state)
				return GF_IP_NETWORK_EMPTY;
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot try to download %s... out of memory ?\n", url));
			return GF_OUT_OF_MEM;
		}
	} else {
		had_sess = GF_TRUE;
		if (persistent_mode!=2) {
			e = dash_io->setup_from_url(dash_io, *sess, url, group_idx);
			if (e) {
				//with ROUTE we may have 404 right away if nothing in cache yet, not an error
				GF_LOG(dash->route_clock_state ? GF_LOG_DEBUG : GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot resetup downloader for url %s: %s\n", url, gf_error_to_string(e) ));
				return e;
			}
		}
	}

retry:

	if (end_range) {
		e = dash_io->set_range(dash_io, *sess, start_range, end_range, (persistent_mode==2) ? GF_FALSE : GF_TRUE);
		if (e) {
			if (had_sess) {
				dash_io->del(dash_io, *sess);
				*sess = NULL;
				return gf_dash_download_resource(dash, sess, url, start_range, end_range, persistent_mode ? 1 : 0, group);
			}
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot setup byte-range download for %s: %s\n", url, gf_error_to_string(e) ));
			return e;
		}
	}
	assert(*sess);

	/*issue HTTP GET for headers only*/
	e = dash_io->init(dash_io, *sess);

	if (e>=GF_OK) {
		/*check mime type of the adaptation set if not provided*/
		if (group) {
			const char *mime = *sess ? dash_io->get_mime(dash_io, *sess) : NULL;
			if (mime && !group->service_mime) {
				group->service_mime = gf_strdup(mime);
			}


			/*file cannot be cached on disk !*/
			if (dash_io->get_cache_name(dash_io, *sess ) == NULL) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Segment %s cannot be cached on disk, will use direct streaming\n", url));
				group->segment_must_be_streamed = GF_TRUE;
				return GF_OK;
			}
			group->segment_must_be_streamed = GF_FALSE;
		}

		//release dl_mutex while downloading segment
		/*we can download the file*/
		e = dash_io->run(dash_io, *sess);
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] At "LLU" error %s - released dl_mutex\n", gf_net_get_utc(), gf_error_to_string(e)));
	}

	switch (e) {
	case GF_IP_CONNECTION_FAILURE:
	case GF_IP_NETWORK_FAILURE:
		if (!dash->in_error || group) {
			dash_io->del(dash_io, *sess);
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] failed to download, retrying once with %s...\n", url));
			*sess = dash_io->create(dash_io, 0, url, group_idx);
			if (! (*sess)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot retry to download %s... out of memory ?\n", url));
				return GF_OUT_OF_MEM;
			}

			if (retry) {
				retry = GF_FALSE;
				goto retry;
			}
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] two consecutive failures, aborting the download %s.\n", url));
		} else if (dash->in_error) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Download still in error for %s.\n", url));
		}
		break;
	case GF_OK:
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Download %s complete at UTC "LLU" ms\n", url, gf_net_get_utc() ));
		break;
	default:
		//log as warning, maybe the dash client can recover from this error
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Failed to download %s: %s\n", url, gf_error_to_string(e)));
		break;
	}
	return e;
}

static void gf_dash_get_timeline_duration(GF_MPD *mpd, GF_MPD_Period *period, GF_MPD_SegmentTimeline *timeline, u32 timescale, u32 *nb_segments, Double *max_seg_duration)
{
	u32 i, count;
	u64 period_duration, start_time, dur;
	if (period->duration) {
		period_duration = period->duration;
	} else {
		period_duration = mpd->media_presentation_duration - period->start;
	}
	period_duration *= timescale;
	period_duration /= 1000;

	*nb_segments = 0;
	if (max_seg_duration) *max_seg_duration = 0;
	start_time = 0;
	dur = 0;
	count = gf_list_count(timeline->entries);
	for (i=0; i<count; i++) {
		GF_MPD_SegmentTimelineEntry *ent = gf_list_get(timeline->entries, i);
		if ((s32)ent->repeat_count >=0) {
			*nb_segments += 1 + ent->repeat_count;
			if (ent->start_time) {
				start_time = ent->start_time;
				dur = (1 + ent->repeat_count);
			} else {
				dur += (1 + ent->repeat_count);
			}
			dur	*= ent->duration;
		} else {
			u32 nb_seg = 0;
			if (i+1<count) {
				GF_MPD_SegmentTimelineEntry *ent2 = gf_list_get(timeline->entries, i+1);
				if (ent2->start_time>0) {
					nb_seg = (u32) ( (ent2->start_time - start_time - dur) / ent->duration);
					dur += ((u64)nb_seg) * ent->duration;
				}
			}
			if (!nb_seg) {
				nb_seg = (u32) ( (period_duration - start_time) / ent->duration );
				dur += ((u64)nb_seg) * ent->duration;
			}
			*nb_segments += nb_seg;
		}
		if (max_seg_duration && (*max_seg_duration < ent->duration)) *max_seg_duration = ent->duration;
	}
}

static void gf_dash_get_segment_duration(GF_MPD_Representation *rep, GF_MPD_AdaptationSet *set, GF_MPD_Period *period, GF_MPD *mpd, u32 *nb_segments, Double *max_seg_duration)
{
	Double mediaDuration;
	Bool single_segment = GF_FALSE;
	u32 timescale;
	u64 duration;
	GF_MPD_SegmentTimeline *timeline = NULL;
	*nb_segments = timescale = 0;
	duration = 0;

	if (rep->segment_list || set->segment_list || period->segment_list) {
		GF_List *segments = NULL;
		if (period->segment_list) {
			if (period->segment_list->duration) duration = period->segment_list->duration;
			if (period->segment_list->timescale) timescale = period->segment_list->timescale;
			if (period->segment_list->segment_URLs) segments = period->segment_list->segment_URLs;
			if (period->segment_list->segment_timeline) timeline = period->segment_list->segment_timeline;
		}
		if (set->segment_list) {
			if (set->segment_list->duration) duration = set->segment_list->duration;
			if (set->segment_list->timescale) timescale = set->segment_list->timescale;
			if (set->segment_list->segment_URLs) segments = set->segment_list->segment_URLs;
			if (set->segment_list->segment_timeline) timeline = set->segment_list->segment_timeline;
		}
		if (rep->segment_list) {
			if (rep->segment_list->duration) duration = rep->segment_list->duration;
			if (rep->segment_list->timescale) timescale = rep->segment_list->timescale;
			if (rep->segment_list->segment_URLs) segments = rep->segment_list->segment_URLs;
			if (rep->segment_list->segment_timeline) timeline = rep->segment_list->segment_timeline;
		}
		if (!timescale) timescale=1;

		if (timeline) {
			gf_dash_get_timeline_duration(mpd, period, timeline, timescale, nb_segments, max_seg_duration);
			if (max_seg_duration) *max_seg_duration /= timescale;
		} else {
			if (segments)
				*nb_segments = gf_list_count(segments);
			if (max_seg_duration) {
				*max_seg_duration = (Double) duration;
				*max_seg_duration /= timescale;
			}
		}
		return;
	}

	/*single segment*/
	if (rep->segment_base || set->segment_base || period->segment_base) {
		*max_seg_duration = (Double)mpd->media_presentation_duration;
		*max_seg_duration /= 1000;
		*nb_segments = 1;
		return;
	}

	single_segment = GF_TRUE;
	if (period->segment_template) {
		single_segment = GF_FALSE;
		if (period->segment_template->duration) duration = period->segment_template->duration;
		if (period->segment_template->timescale) timescale = period->segment_template->timescale;
		if (period->segment_template->segment_timeline) timeline = period->segment_template->segment_timeline;
	}
	if (set->segment_template) {
		single_segment = GF_FALSE;
		if (set->segment_template->duration) duration = set->segment_template->duration;
		if (set->segment_template->timescale) timescale = set->segment_template->timescale;
		if (set->segment_template->segment_timeline) timeline = set->segment_template->segment_timeline;
	}
	if (rep->segment_template) {
		single_segment = GF_FALSE;
		if (rep->segment_template->duration) duration = rep->segment_template->duration;
		if (rep->segment_template->timescale) timescale = rep->segment_template->timescale;
		if (rep->segment_template->segment_timeline) timeline = rep->segment_template->segment_timeline;
	}
	if (!timescale) timescale=1;

	/*if no SegmentXXX is found, this is a single segment representation (onDemand profile)*/
	if (single_segment) {
		*max_seg_duration = (Double)mpd->media_presentation_duration;
		*max_seg_duration /= 1000;
		*nb_segments = 1;
		return;
	}

	if (timeline) {
		gf_dash_get_timeline_duration(mpd, period, timeline, timescale, nb_segments, max_seg_duration);
		if (max_seg_duration) *max_seg_duration /= timescale;
	} else {
		if (max_seg_duration) {
			*max_seg_duration = (Double) duration;
			*max_seg_duration /= timescale;
		}
		mediaDuration = (Double)period->duration;
		if (!mediaDuration && mpd->media_presentation_duration) {
			u32 i, count = gf_list_count(mpd->periods);
			Double start = 0;
			for (i=0; i<count; i++) {
				GF_MPD_Period *ap = gf_list_get(mpd->periods, i);
				if (ap==period) break;
				if (ap->start) start = (Double)ap->start;
				start += ap->duration;
			}
			mediaDuration = mpd->media_presentation_duration - start;
		}
		if (mediaDuration && duration) {
			Double nb_seg = (Double) mediaDuration;
			/*duration is given in ms*/
			nb_seg /= 1000;
			nb_seg *= timescale;
			nb_seg /= duration;
			*nb_segments = (u32) nb_seg;
			if (*nb_segments < nb_seg) (*nb_segments)++;
		}
	}
}


static u64 gf_dash_get_segment_start_time_with_timescale(GF_DASH_Group *group, u64 *segment_duration, u32 *scale)
{
	GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);
	GF_MPD_AdaptationSet *set = group->adaptation_set;
	GF_MPD_Period *period = group->period;
	s32 segment_index = group->download_segment_index;
	u64 start_time = 0;

	gf_mpd_get_segment_start_time_with_timescale(segment_index,
		period, set, rep,
		&start_time, segment_duration, scale);

	return start_time;
}

static Double gf_dash_get_segment_start_time(GF_DASH_Group *group, Double *segment_duration)
{
	u64 start = 0;
	u64 dur = 0;
	u32 scale = 1000;

	start = gf_dash_get_segment_start_time_with_timescale(group, &dur, &scale);
	if (segment_duration) {
		*segment_duration = (Double) dur;
		*segment_duration /= scale;
	}
	return ((Double)start)/scale;
}

static u64 gf_dash_get_segment_availability_start_time(GF_MPD *mpd, GF_DASH_Group *group, u32 segment_index, u32 *seg_dur_ms)
{
	Double seg_ast, seg_dur=0.0;
	seg_ast = gf_dash_get_segment_start_time(group, &seg_dur);
	if (seg_dur_ms) *seg_dur_ms = (u32) (seg_dur * 1000);

	seg_ast += seg_dur;
	seg_ast *= 1000;
	seg_ast += group->period->start + group->ast_at_init;
	seg_ast -= group->ast_offset;
 	return (u64) seg_ast;
}

static u32 gf_dash_get_index_in_timeline(GF_MPD_SegmentTimeline *timeline, u64 segment_start, u64 start_timescale, u64 timescale)
{
	u64 start_time = 0;
	u32 idx = 0;
	u32 i, count, repeat;
	count = gf_list_count(timeline->entries);
	for (i=0; i<count; i++) {
		GF_MPD_SegmentTimelineEntry *ent = gf_list_get(timeline->entries, i);

		if (!i || ent->start_time) start_time = ent->start_time;

		repeat = ent->repeat_count+1;
		while (repeat) {
			if (start_timescale==timescale) {
				if (start_time == segment_start )
					return idx;
				if (start_time > segment_start) {
					GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Warning: segment timeline entry start "LLU" greater than segment start "LLU", using current entry\n", start_time, segment_start));
					return idx;
				}
			} else {
				if (gf_timestamp_equal(start_time, timescale, segment_start, start_timescale)) return idx;
				if (gf_timestamp_greater(start_time, timescale, segment_start, start_timescale)) {
					GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Warning: segment timeline entry start "LLU" greater than segment start "LLU", using current entry\n", start_time, segment_start));
					return idx;
				}
			}
			start_time+=ent->duration;
			repeat--;
			idx++;
		}
	}
	//end of list in regular case: segment was the last one of the previous list and no changes happend
	if (start_timescale==timescale) {
		if (start_time == segment_start )
			return idx;
	} else {
		if (start_time*start_timescale == segment_start * timescale)
			return idx;
	}

	GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error: could not find previous segment start in current timeline ! seeking to end of timeline\n"));
	return idx;
}


static GF_Err gf_dash_merge_segment_timeline(GF_DASH_Group *group, GF_DashClient *dash, GF_MPD_SegmentList *old_list, GF_MPD_SegmentTemplate *old_template, GF_MPD_SegmentList *new_list, GF_MPD_SegmentTemplate *new_template, Double min_start_time)
{
	GF_MPD_SegmentTimeline *old_timeline, *new_timeline;
	u32 i, idx, timescale, nb_new_segs;
	GF_MPD_SegmentTimelineEntry *ent;

	old_timeline = new_timeline = NULL;
	if (old_list && old_list->segment_timeline) {
		if (!new_list || !new_list->segment_timeline) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update playlist: segment timeline not present in new MPD segmentList\n"));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		old_timeline = old_list->segment_timeline;
		new_timeline = new_list->segment_timeline;
		timescale = new_list->timescale;
	} else if (old_template && old_template->segment_timeline) {
		if (!new_template || !new_template->segment_timeline) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update playlist: segment timeline not present in new MPD segmentTemplate\n"));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		old_timeline = old_template->segment_timeline;
		new_timeline = new_template->segment_timeline;
		timescale = new_template->timescale;
	}
	if (!old_timeline && !new_timeline) return GF_OK;

	if (group) {
		group->current_start_time = gf_dash_get_segment_start_time_with_timescale(group, NULL, &group->current_timescale);
	} else {
		for (i=0; i<gf_list_count(dash->groups); i++) {
			GF_DASH_Group *a_group = gf_list_get(dash->groups, i);
			a_group->current_start_time = gf_dash_get_segment_start_time_with_timescale(a_group, NULL, &a_group->current_timescale);
		}
	}

	nb_new_segs = 0;
	idx=0;
	while ((ent = gf_list_enum(new_timeline->entries, &idx))) {
		nb_new_segs += 1 + ent->repeat_count;
	}

	if (group) {
#ifndef GPAC_DISABLE_LOG
		u32 prev_idx = group->download_segment_index;
#endif
		group->nb_segments_in_rep = nb_new_segs;
		group->download_segment_index = gf_dash_get_index_in_timeline(new_timeline, group->current_start_time, group->current_timescale, timescale ? timescale : group->current_timescale);

		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Updated SegmentTimeline: New segment number %d - old %d - start time "LLD"\n", group->download_segment_index , prev_idx, group->current_start_time));
	} else {
		for (i=0; i<gf_list_count(dash->groups); i++) {
			GF_DASH_Group *a_group = gf_list_get(dash->groups, i);
#ifndef GPAC_DISABLE_LOG
			u32 prev_idx = a_group->download_segment_index;
#endif
			a_group->nb_segments_in_rep = nb_new_segs;
			a_group->download_segment_index = gf_dash_get_index_in_timeline(new_timeline, a_group->current_start_time, a_group->current_timescale, timescale ? timescale : a_group->current_timescale);

			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Updated SegmentTimeline: New segment number %d - old %d - start time "LLD"\n", a_group->download_segment_index , prev_idx, a_group->current_start_time));
		}
	}


#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_DASH, GF_LOG_INFO) ) {
		u64 start = 0;
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Old SegmentTimeline: \n"));
		for (idx=0; idx<gf_list_count(old_timeline->entries); idx++) {
			ent = gf_list_get(old_timeline->entries, idx);
			if (ent->start_time) start = ent->start_time;
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("\tt="LLU" d=%d r=%d (current start "LLU")\n", ent->start_time, ent->duration, ent->repeat_count, start));
			start += ent->duration * (1+ent->repeat_count);
		}


		start = 0;
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] New SegmentTimeline: \n"));
		for (idx=0; idx<gf_list_count(new_timeline->entries); idx++) {
			ent = gf_list_get(new_timeline->entries, idx);
			if (ent->start_time) start = ent->start_time;
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("\tt="LLU" d=%d r=%d (current start "LLU")\n", ent->start_time, ent->duration, ent->repeat_count, start));
			start += ent->duration * (1+ent->repeat_count);
		}
	}
#endif

	return GF_OK;
}

static u32 gf_dash_purge_segment_timeline(GF_DASH_Group *group, Double min_start_time)
{
	u32 nb_removed, time_scale;
	u64 start_time, min_start, duration;
	GF_MPD_SegmentTimeline *timeline=NULL;
	GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);

	if (!min_start_time) return 0;
	gf_mpd_resolve_segment_duration(rep, group->adaptation_set, group->period, &duration, &time_scale, NULL, &timeline);
	if (!timeline) return 0;

	min_start = (u64) (min_start_time*time_scale);
	start_time = 0;
	nb_removed=0;
	while (1) {
		GF_MPD_SegmentTimelineEntry *ent = gf_list_get(timeline->entries, 0);
		if (!ent) break;
		if (ent->start_time) start_time = ent->start_time;

		while (start_time + ent->duration < min_start) {
			if (! ent->repeat_count) break;
			ent->repeat_count--;
			nb_removed++;
			start_time += ent->duration;
		}
		/*this entry is in our range, keep it and make sure it has a start time*/
		if (start_time + ent->duration >= min_start) {
			if (!ent->start_time)
				ent->start_time = start_time;
			break;
		}
		start_time += ent->duration;
		gf_list_rem(timeline->entries, 0);
		gf_free(ent);
		nb_removed++;
	}
	if (nb_removed) {
		GF_MPD_SegmentList *segment_list;
		/*update next download index*/
		group->download_segment_index -= nb_removed;
		assert(group->nb_segments_in_rep >= nb_removed);
		group->nb_segments_in_rep -= nb_removed;
		/*clean segmentList*/
		segment_list = NULL;
		if (group->period && group->period->segment_list) segment_list = group->period->segment_list;
		if (group->adaptation_set && group->adaptation_set->segment_list) segment_list = group->adaptation_set->segment_list;
		if (rep && rep->segment_list) segment_list = rep->segment_list;

		if (segment_list) {
			u32 i = nb_removed;
			while (i) {
				GF_MPD_SegmentURL *seg_url = gf_list_get(segment_list->segment_URLs, 0);
				gf_list_rem(segment_list->segment_URLs, 0);
				gf_mpd_segment_url_free(seg_url);
				i--;
			}
		}
		group->nb_segments_purged += nb_removed;
	}
	return nb_removed;
}

static GF_Err gf_dash_solve_representation_xlink(GF_DashClient *dash, GF_MPD_Representation *rep, u8 last_sig[GF_SHA1_DIGEST_SIZE])
{
	u32 count, i;
	GF_Err e;
	Bool is_local=GF_FALSE;
	const char *local_url;
	char *url;
	GF_DOMParser *parser;
	u8 signature[GF_SHA1_DIGEST_SIZE];
	if (!rep->segment_list->xlink_href) return GF_BAD_PARAM;

	GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Resolving Representation SegmentList XLINK %s\n", rep->segment_list->xlink_href));

	//SPEC: If this value is present, the element containing the xlink:href attribute and all @xlink attributes included in the element containing @xlink:href shall be removed at the time when the resolution is due.
	if (!strcmp(rep->segment_list->xlink_href, "urn:mpeg:dash:resolve-to-zero:2013")) {
		gf_mpd_delete_segment_list(rep->segment_list);
		rep->segment_list = NULL;
		return GF_OK;
	}

	//xlink relative to our MPD base URL
	url = gf_url_concatenate(dash->base_url, rep->segment_list->xlink_href);

	if (!strstr(url, "://") || !strnicmp(url, "file://", 7) ) {
		local_url = url;
		is_local=GF_TRUE;
		e = GF_OK;
	} else {
		/*use non-persistent connection for MPD*/
		e = gf_dash_download_resource(dash, &(dash->mpd_dnload), url ? url : rep->segment_list->xlink_href, 0, 0, 0, NULL);
		gf_free(url);
	}

	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot download Representation SegmentList XLINK %s: error %s\n", rep->segment_list->xlink_href, gf_error_to_string(e)));
		gf_free(rep->segment_list->xlink_href);
		rep->segment_list->xlink_href = NULL;
		return e;
	}

	if (!is_local) {
		/*in case the session has been restarted, local_url may have been destroyed - get it back*/
		local_url = dash->dash_io->get_cache_name(dash->dash_io, dash->mpd_dnload);
	}

	gf_sha1_file(local_url, signature);
	if (! memcmp(signature, last_sig, GF_SHA1_DIGEST_SIZE)) {
		if (is_local) gf_free(url);
		return GF_EOS;
	}
	memcpy(last_sig, signature, GF_SHA1_DIGEST_SIZE);

	parser = gf_xml_dom_new();
	e = gf_xml_dom_parse(parser, local_url, NULL, NULL);
	if (is_local) gf_free(url);

	if (e != GF_OK) {
		gf_xml_dom_del(parser);
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot parse Representation SegmentList XLINK: error in XML parsing %s\n", gf_error_to_string(e)));
		gf_free(rep->segment_list->xlink_href);
		rep->segment_list->xlink_href = NULL;
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	count = gf_xml_dom_get_root_nodes_count(parser);
	if (count > 1) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] XLINK %s has more than one segment list - ignoring it\n", rep->segment_list->xlink_href));
		gf_mpd_delete_segment_list(rep->segment_list);
		rep->segment_list = NULL;
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	for (i=0; i<count; i++) {
		GF_XMLNode *root = gf_xml_dom_get_root_idx(parser, i);
		if (!strcmp(root->name, "SegmentList")) {
			GF_MPD_SegmentList *new_seg_list = gf_mpd_solve_segment_list_xlink(dash->mpd, root);
			//forbiden
			if (new_seg_list && new_seg_list->xlink_href) {
				if (new_seg_list->xlink_actuate_on_load) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] XLINK %s references to remote element entities that contain another @xlink:href attribute with xlink:actuate set to onLoad - forbiden\n", rep->segment_list->xlink_href));
					gf_mpd_delete_segment_list(new_seg_list);
					new_seg_list = NULL;
				} else {
					new_seg_list->consecutive_xlink_count = rep->segment_list->consecutive_xlink_count + 1;
				}
			}
			//replace current segment list by the one from remote element entity (located by xlink:href)
			gf_mpd_delete_segment_list(rep->segment_list);
			rep->segment_list = new_seg_list;
		}
		else
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] XML node %s is not a representation segmentlist - ignoring it\n", root->name));
	}
	return GF_OK;
}

static void dash_purge_xlink(GF_MPD *new_mpd)
{
	u32 i, count = gf_list_count(new_mpd->periods);
	for (i=0; i<count; i++) {
		GF_MPD_Period *period = gf_list_get(new_mpd->periods, i);
		if (!gf_list_count(period->adaptation_sets)) continue;
		if (!period->xlink_href) continue;
		gf_free(period->xlink_href);
		period->xlink_href = NULL;
	}
}

static void gf_dash_mark_group_done(GF_DASH_Group *group)
{
	GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] AS#%d group is done\n", 1+group->groups_idx ));
	group->done = GF_TRUE;
}


static const char *dash_strip_base_url(const char *url, const char *base_url)
{
	const char *url_start = gf_url_get_path(url);
	const char *base_url_start = gf_url_get_path(base_url);
	const char *base_url_end = base_url_start ? strrchr(base_url_start, '/') : NULL;

	if (base_url_start && url_start) {
		u32 diff = (u32) (base_url_end - base_url_start);
		if (!strncmp(url_start, base_url_start, diff))
			return url_start + diff + 1;
	}
	return url;
}

static GF_Err gf_dash_solve_m3u8_representation_xlink(GF_DASH_Group *group, GF_MPD_Representation *rep, Bool *is_static, u64 *duration, u8 signature[GF_SHA1_DIGEST_SIZE])
{
	GF_Err e;
	char *xlink_copy;
	const char *name, *local_url;
	GF_DashClient *dash = group->dash;
	if (!dash->dash_io->manifest_updated) {
		return gf_m3u8_solve_representation_xlink(rep, &dash->getter, is_static, duration, signature);
	}

	xlink_copy = gf_strdup(rep->segment_list->xlink_href);
	e = gf_m3u8_solve_representation_xlink(rep, &dash->getter, is_static, duration, signature);
	//do not notify m3u8 update if same as last one
	if (e==GF_EOS) {
		gf_free(xlink_copy);
		return e;
	}
	
	if (gf_url_is_local(xlink_copy)) {
		local_url = xlink_copy;
	} else {
		local_url = dash->getter.get_cache_name(&dash->getter);
	}
	name = dash_strip_base_url(xlink_copy, dash->base_url);
	dash->dash_io->manifest_updated(dash->dash_io, name, local_url, group->groups_idx);
	gf_free(xlink_copy);
	return e;
}

static u32 ls_hls_purge_segments(s32 live_edge_idx, GF_List *l)
{
	u32 i=0, count = gf_list_count(l);
	u32 nb_removed_before_live = 0;

	//first remove all ll chunks until live edge if set
	if (live_edge_idx>=0) {
		count = live_edge_idx;

		for (i=0; i<count; i++) {
			GF_MPD_SegmentURL *seg = gf_list_get(l, i);
			if (!seg->hls_ll_chunk_type)
				continue;

			gf_list_rem(l, i);
			gf_mpd_segment_url_free(seg);
			i--;
			count--;
			live_edge_idx--;
			nb_removed_before_live++;
		}
		if (live_edge_idx<0) return nb_removed_before_live;
		count = gf_list_count(l);
		//skip live edge
		i++;

		//skip parts following live edge
		while (i<count) {
			GF_MPD_SegmentURL *seg = gf_list_get(l, i);
			if (!seg->hls_ll_chunk_type)
				break;
			i++;
		}
	}

	//locate last full seg
	while (i<count) {
		GF_MPD_SegmentURL *seg = gf_list_get(l, count-1);
		if (!seg->hls_ll_chunk_type)
			break;
		count--;
		if (!count) break;
	}
	//purge the rest
	for (; i<count; i++) {
		GF_MPD_SegmentURL *seg = gf_list_get(l, i);
		if (!seg->hls_ll_chunk_type)
			continue;

		gf_list_rem(l, i);
		gf_mpd_segment_url_free(seg);
		i--;
		count--;
	}
	return nb_removed_before_live;
}

static void dash_check_chaining(GF_DashClient *dash, char *scheme_id, char **chain_ptr, u32 stack_state)
{
	char *sep;
	GF_MPD_Descriptor *chaining = gf_mpd_get_descriptor(dash->mpd->essential_properties, scheme_id);
	if (!chaining)
		chaining = gf_mpd_get_descriptor(dash->mpd->supplemental_properties, scheme_id);

	if (chain_ptr) {
		if (*chain_ptr) gf_free(*chain_ptr);
		*chain_ptr = NULL;
	}

	if (!chaining) return;

	dash->chain_stack_state = stack_state;
	if ((stack_state==GF_DASH_CHAIN_POP) && !gf_list_count(dash->chain_stack))
		dash->chain_stack_state = GF_DASH_CHAIN_REG;

	if (!chain_ptr) return;

	if (*chain_ptr) gf_free(*chain_ptr);
	sep = strchr(chaining->value, ' ');
	if (sep) sep[0] = 0;
	*chain_ptr = gf_url_concatenate(dash->base_url, chaining->value);
	if (!*chain_ptr)
		*chain_ptr = gf_strdup(chaining->value);
	if (sep) sep[0] = ' ';

	if (*chain_ptr && (chain_ptr == &dash->chain_fallback)) {
		if (!strcmp(dash->chain_fallback, dash->base_url)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Chain fallback URL is same as manifest, disabling fallback\n"));
			gf_free(dash->chain_fallback);
			dash->chain_fallback = NULL;
		}
	}
}


static GF_Err gf_dash_update_manifest(GF_DashClient *dash)
{
	GF_Err e;
	Bool force_timeline_setup = GF_FALSE;
	u32 group_idx, rep_idx, i, j;
	u64 fetch_time=0;
	GF_DOMParser *mpd_parser;
	u8 signature[GF_SHA1_DIGEST_SIZE];
	GF_MPD_Period *period=NULL, *new_period=NULL;
	const char *local_url;
	char mime[128];
	char * purl;
	Double timeline_start_time=0;
	GF_MPD *new_mpd=NULL;
	Bool fetch_only = GF_FALSE;
	u32 nb_group_unchanged = 0;
	Bool has_reps_unchanged = GF_FALSE;

	//HLS: do not reload the playlist, directly update the reps
	if (dash->is_m3u8 && !dash->m3u8_reload_master) {
		new_mpd = NULL;
		new_period = NULL;
		fetch_time = dash_get_fetch_time(dash);
		period = gf_list_get(dash->mpd->periods, dash->active_period_index);
		goto process_m3u8_manifest;
	}

	if (!dash->mpd_dnload) {
		local_url = purl = NULL;
		if (!gf_list_count(dash->mpd->locations)) {
			if (gf_file_exists(dash->base_url)) {
				local_url = dash->base_url;
			}
			if (!local_url) {
				/*we will no longer attempt to update the MPD ...*/
				dash->mpd->minimum_update_period = 0;
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update playlist: no HTTP source for MPD could be found\n"));
				return GF_BAD_PARAM;
			}
		}
		if (!local_url) {
			purl = gf_strdup(gf_list_get(dash->mpd->locations, 0));

			/*if no absolute URL, use <Location> to get MPD in case baseURL is relative...*/
			if (!strstr(dash->base_url, "://")) {
				gf_free(dash->base_url);
				dash->base_url = gf_strdup(purl);
			}
			fetch_only = 1;
		}
	} else {
		local_url = dash->dash_io->get_cache_name(dash->dash_io, dash->mpd_dnload);
		if (local_url) {
			gf_file_delete(local_url);
		}
		//use the redirected url stored in base URL - DO NOT USE the redirected URL of the session since
		//the session may have been reused for period XLINK dowload.
		purl = gf_strdup( dash->base_url );
	}

	/*if update location is specified, update - spec does not say whether location is a relative or absoute URL*/
	if (gf_list_count(dash->mpd->locations)) {
		char *update_loc = gf_list_get(dash->mpd->locations, 0);
		char *update_url = gf_url_concatenate(purl, update_loc);
		if (update_url) {
			gf_free(purl);
			purl = update_url;
		}
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Updating Playlist %s...\n", purl ? purl : local_url));
	if (purl) {
		const char *mime_type;
		/*use non-persistent connection for MPD*/
		e = gf_dash_download_resource(dash, &(dash->mpd_dnload), purl, 0, 0, 0, NULL);
		if (e!=GF_OK) {
			if (!dash->in_error) {
				if (e==GF_URL_ERROR) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update manifest: %s, aborting\n", gf_error_to_string(e)));
					dash->in_error = GF_TRUE;
				} else {
					GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Error while fetching new manifest (%s) - will retry later\n", gf_error_to_string(e)));
				}
			}
			gf_free(purl);
			//try to refetch MPD every second
			dash->last_update_time+=1000;
			return e;
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Playlist %s updated with success\n", purl));
		}

		mime_type = dash->dash_io->get_mime(dash->dash_io, dash->mpd_dnload) ;
		strcpy(mime, mime_type ? mime_type : "");
		strlwr(mime);

		/*in case the session has been restarted, local_url may have been destroyed - get it back*/
		local_url = dash->dash_io->get_cache_name(dash->dash_io, dash->mpd_dnload);

		/* Some servers, for instance http://tv.freebox.fr, serve m3u8 as text/plain */
		if (gf_dash_is_m3u8_mime(purl, mime) || strstr(purl, ".m3u8")) {
			new_mpd = gf_mpd_new();
			e = gf_m3u8_to_mpd(local_url, purl, NULL, dash->reload_count, dash->mimeTypeForM3U8Segments, 0, M3U8_TO_MPD_USE_TEMPLATE, M3U8_TO_MPD_USE_SEGTIMELINE, &dash->getter, new_mpd, GF_FALSE, dash->keep_files);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update playlist: error in MPD creation %s\n", gf_error_to_string(e)));
				gf_mpd_del(new_mpd);
				return GF_NON_COMPLIANT_BITSTREAM;
			}
		}

		gf_free(purl);

		purl = (char *) dash->dash_io->get_url(dash->dash_io, dash->mpd_dnload) ;

		/*if relocated, reassign MPD base URL*/
		if (strcmp(purl, dash->base_url)) {
			gf_free(dash->base_url);
			dash->base_url = gf_strdup(purl);

		}
		purl = NULL;
	}

	fetch_time = dash_get_fetch_time(dash);

	// parse the mpd file for filling the GF_MPD structure. Note: for m3u8, MPD has been fetched above
	if (!new_mpd) {
		u32 res = gf_dash_check_mpd_root_type(local_url);
		if ((res==1) && dash->is_smooth) res = 0;
		else if ((res==2) && !dash->is_smooth) res = 0;

		if (!res) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update playlist: MPD file type is not correct %s\n", local_url));
			return GF_NON_COMPLIANT_BITSTREAM;
		}

		if (gf_sha1_file( local_url, signature) != GF_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] : cannot SHA1 file %s\n", local_url));
			return GF_IO_ERR;
		}

		if (!dash->in_error && ! memcmp( signature, dash->lastMPDSignature, GF_SHA1_DIGEST_SIZE)) {

			dash->reload_count++;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] MPD file did not change for %d consecutive reloads\n", dash->reload_count));
			/*if the MPD did not change, we should refresh "soon" but cannot wait a full refresh cycle in case of
			low latencies as we could miss a segment*/

			if (dash->is_m3u8) {
				dash->last_update_time += dash->mpd->minimum_update_period/2;
			} else {
				dash->last_update_time = gf_sys_clock();
			}

			dash->mpd_fetch_time = fetch_time;
			return GF_OK;
		}

		force_timeline_setup = dash->in_error;
		dash->in_error = GF_FALSE;
		dash->reload_count = 0;
		memcpy(dash->lastMPDSignature, signature, GF_SHA1_DIGEST_SIZE);

		if (dash->dash_io->manifest_updated) {
			char *szName = gf_file_basename(dash->base_url);
			dash->dash_io->manifest_updated(dash->dash_io, szName, local_url, -1);
		}

		/* It means we have to reparse the file ... */
		/* parse the MPD */
		mpd_parser = gf_xml_dom_new();
		e = gf_xml_dom_parse(mpd_parser, local_url, NULL, NULL);
		if (e != GF_OK) {
			gf_xml_dom_del(mpd_parser);
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update playlist: error in XML parsing %s\n", gf_error_to_string(e)));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		new_mpd = gf_mpd_new();
		if (dash->is_smooth)
			e = gf_mpd_init_smooth_from_dom(gf_xml_dom_get_root(mpd_parser), new_mpd, purl);
		else
			e = gf_mpd_init_from_dom(gf_xml_dom_get_root(mpd_parser), new_mpd, purl);
		gf_xml_dom_del(mpd_parser);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update playlist: error in MPD creation %s\n", gf_error_to_string(e)));
			gf_mpd_del(new_mpd);
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		if (dash->ignore_xlink)
			dash_purge_xlink(new_mpd);

		if (!e && dash->split_adaptation_set)
			gf_mpd_split_adaptation_sets(new_mpd);

	}

	assert(new_mpd);

	period = gf_list_get(dash->mpd->periods, dash->active_period_index);
	if (fetch_only  && !period) goto exit;

#ifndef GPAC_DISABLE_LOG
	GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Updated manifest:\n"));
	for (i=0; i<gf_list_count(new_mpd->periods); i++) {
		GF_MPD_Period *ap = gf_list_get(new_mpd->periods, i);
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("\tP#%d: start "LLU" - duration " LLU" - xlink %s\n", i+1, ap->start, ap->duration, ap->xlink_href ? ap->xlink_href : "none"));
	}
#endif

	//if current period was a remote period, do a pass on the new manifest periods, check for xlink
	if (period->origin_base_url || period->broken_xlink) {
restart_period_check:
		for (i=0; i<gf_list_count(new_mpd->periods); i++) {
			new_period = gf_list_get(new_mpd->periods, i);
			if (!new_period->xlink_href) continue;

			if ((new_period->start<=period->start) &&
				( (new_period->start+new_period->duration >= period->start + period->duration) || (!new_period->duration))
			) {
				const char *base_url;
				u32 insert_idx;
				if (period->type == GF_MPD_TYPE_DYNAMIC) {
					gf_dash_solve_period_xlink(dash, new_mpd->periods, i);
					goto restart_period_check;
				}
				//this is a static period xlink, no need to further update the mpd, jsut swap the old periods in the new MPD

				base_url = period->origin_base_url ? period->origin_base_url : period->broken_xlink;
				if (!base_url) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - found new Xlink period overlapping a non-xlink period in original manifest\n"));
					gf_mpd_del(new_mpd);
					return GF_NON_COMPLIANT_BITSTREAM;
				}
				insert_idx = i;
				gf_list_rem(new_mpd->periods, i);
				for (j=0; j<gf_list_count(dash->mpd->periods); j++) {
					GF_MPD_Period *ap = gf_list_get(dash->mpd->periods, j);
					if (!ap->origin_base_url && !ap->broken_xlink) continue;
					if (ap->origin_base_url && strcmp(ap->origin_base_url, base_url)) continue;
					if (ap->broken_xlink && strcmp(ap->broken_xlink, base_url)) continue;
					gf_list_rem(dash->mpd->periods, j);
					j--;
					gf_list_insert(new_mpd->periods, ap, insert_idx);
					insert_idx++;
				}
				//update active period index in new list
				dash->active_period_index = gf_list_find(new_mpd->periods, period);
				assert((s32)dash->active_period_index >= 0);
				//this will do the garbage collection
				gf_list_add(dash->mpd->periods, new_period);
				goto exit;
			}
			new_period=NULL;
		}
	}

	new_period = NULL;
	for (i=0; i<gf_list_count(new_mpd->periods); i++) {
		new_period = gf_list_get(new_mpd->periods, i);
		if (new_period->start==period->start) break;
		new_period=NULL;
	}

	if (!new_period) {
		if (dash->mpd->type == GF_MPD_TYPE_DYNAMIC) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] current active period not found in MPD update - assuming end of active period and switching to first period in new MPD\n"));

			//assume the old period is no longer valid
			dash->active_period_index = 0;
			dash->request_period_switch = 1;
			for (i=0; i<gf_list_count(dash->groups); i++) {
				GF_DASH_Group *group = gf_list_get(dash->groups, i);
				gf_dash_mark_group_done(group);
				group->adaptation_set = NULL;
			}
			goto exit;
		}
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update playlist: missing period\n"));
		gf_mpd_del(new_mpd);
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	dash->active_period_index = gf_list_find(new_mpd->periods, new_period);
	assert((s32)dash->active_period_index >= 0);

	if (gf_list_count(period->adaptation_sets) != gf_list_count(new_period->adaptation_sets)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update playlist: missing AdaptationSet\n"));
		gf_mpd_del(new_mpd);
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Updating playlist at UTC time "LLU" - availabilityStartTime "LLU"\n", fetch_time, new_mpd->availabilityStartTime));

	timeline_start_time = 0;
	/*if not infinity for timeShift, compute min media time before merge and adjust it*/
	if (dash->mpd->time_shift_buffer_depth != (u32) -1) {
		Double timeshift = dash->mpd->time_shift_buffer_depth;
		timeshift /= 1000;

		for (group_idx=0; group_idx<gf_list_count(dash->groups); group_idx++) {
			GF_DASH_Group *group = gf_list_get(dash->groups, group_idx);
			if (group->selection!=GF_DASH_GROUP_NOT_SELECTABLE) {
				Double group_start = gf_dash_get_segment_start_time(group, NULL);
				if (!group_idx || (timeline_start_time > group_start) ) timeline_start_time = group_start;
			}
		}
		/*we can rewind our segments from timeshift*/
		if (timeline_start_time > timeshift) timeline_start_time -= timeshift;
		/*we can rewind all segments*/
		else timeline_start_time = 0;
	}

	/*update segmentTimeline at Period level*/
	e = gf_dash_merge_segment_timeline(NULL, dash, period->segment_list, period->segment_template, new_period->segment_list, new_period->segment_template, timeline_start_time);
	if (e) {
		gf_mpd_del(new_mpd);
		return e;
	}

process_m3u8_manifest:

	for (group_idx=0; group_idx<gf_list_count(dash->groups); group_idx++) {
		GF_MPD_AdaptationSet *set, *new_set=NULL;
		u32 rep_i;
		u32 nb_rep_unchanged = 0;
		GF_DASH_Group *group = gf_list_get(dash->groups, group_idx);

		/*update info even if the group is not selected !*/
		if (group->selection==GF_DASH_GROUP_NOT_SELECTABLE)
			continue;

		set = group->adaptation_set;

		if (new_period) {
			new_set = gf_list_get(new_period->adaptation_sets, group_idx);

			//sort by bandwidth and quality
			for (rep_i = 1; rep_i < gf_list_count(new_set->representations); rep_i++) {
				Bool swap=GF_FALSE;
				GF_MPD_Representation *r2 = gf_list_get(new_set->representations, rep_i);
				GF_MPD_Representation *r1 = gf_list_get(new_set->representations, rep_i-1);
				if (r1->bandwidth > r2->bandwidth) {
					swap=GF_TRUE;
				} else if ((r1->bandwidth == r2->bandwidth) && (r1->quality_ranking<r2->quality_ranking)) {
					swap=GF_TRUE;
				}
				if (swap) {
					gf_list_rem(new_set->representations, rep_i);
					gf_list_insert(new_set->representations, r2, rep_i-1);
					rep_i=0;
				}
			}

			if (gf_list_count(new_set->representations) != gf_list_count(group->adaptation_set->representations)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update playlist: missing representation in adaptation set\n"));
				gf_mpd_del(new_mpd);
				return GF_NON_COMPLIANT_BITSTREAM;
			}
		}

		/*get all representations in both periods*/
		for (rep_idx = 0; rep_idx <gf_list_count(group->adaptation_set->representations); rep_idx++) {
			GF_List *segments, *new_segments;
			GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, rep_idx);
			GF_MPD_Representation *new_rep = new_set ? gf_list_get(new_set->representations, rep_idx) : NULL;
			GF_MPD_Representation *hls_temp_rep = NULL;

			rep->playback.not_modified = GF_FALSE;

			if (rep->segment_base || group->adaptation_set->segment_base || period->segment_base) {
				assert(new_rep);
				if (!new_rep->segment_base && !new_set->segment_base && !new_period->segment_base) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update playlist: representation does not use segment base as previous version\n"));
					gf_mpd_del(new_mpd);
					return GF_NON_COMPLIANT_BITSTREAM;
				}
				/*what else should we check ??*/

				/*OK, this rep is fine*/
			}

			else if (rep->segment_template || group->adaptation_set->segment_template || period->segment_template) {
				assert(new_rep);
				if (!new_rep->segment_template && !new_set->segment_template && !new_period->segment_template) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update playlist: representation does not use segment template as previous version\n"));
					gf_mpd_del(new_mpd);
					return GF_NON_COMPLIANT_BITSTREAM;
				}
				//if no segment timeline, adjust current idx of the group if start number changes (not needed if SegmentTimeline, otherwise, we will look for the index with the same starttime in the timeline)
				if ((period->segment_template && period->segment_template->segment_timeline)
				        || (set->segment_template && set->segment_template->segment_timeline)
				        || (rep->segment_template && rep->segment_template->segment_timeline)
				   ) {
				} else {
					s32 sn_diff = 0;

					if (period->segment_template && (period->segment_template->start_number != (u32) -1) ) sn_diff = period->segment_template->start_number;
					else if (set->segment_template && (set->segment_template->start_number != (u32) -1) ) sn_diff = set->segment_template->start_number;
					else if (rep->segment_template && (rep->segment_template->start_number != (u32) -1) ) sn_diff = rep->segment_template->start_number;

					if (new_period->segment_template && (new_period->segment_template->start_number != (u32) -1) ) sn_diff -= (s32) new_period->segment_template->start_number;
					else if (new_set->segment_template && (new_set->segment_template->start_number != (u32) -1) ) sn_diff -= (s32) new_set->segment_template->start_number;
					else if (new_rep->segment_template && (new_rep->segment_template->start_number != (u32) -1) ) sn_diff -= (s32) new_rep->segment_template->start_number;

					if (sn_diff != 0) {
						GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] startNumber change for SegmentTemplate without SegmentTimeline - adjusting current segment index by %d\n", sn_diff));
						group->download_segment_index += sn_diff;
					}
				}
				/*OK, this rep is fine*/
			}
			else {
				Bool same_rep = GF_FALSE;
				/*we're using segment list*/
				assert(rep->segment_list || group->adaptation_set->segment_list || period->segment_list);

				//HLS case
				if (!new_rep) {
					assert(rep->segment_list);
					assert(rep->segment_list->previous_xlink_href || rep->segment_list->xlink_href);
					hls_temp_rep = gf_mpd_representation_new();
					GF_SAFEALLOC(hls_temp_rep->segment_list, GF_MPD_SegmentList);
					hls_temp_rep->segment_list->segment_URLs = gf_list_new();
					if (rep->segment_list->xlink_href)
						hls_temp_rep->segment_list->xlink_href = gf_strdup(rep->segment_list->xlink_href);
					else
						hls_temp_rep->segment_list->xlink_href = gf_strdup(rep->segment_list->previous_xlink_href);

					new_rep = hls_temp_rep;
				}
				if (group->active_rep_index!=rep_idx)
					same_rep = GF_TRUE;

				//if we have a xlink_href in segment_list, solve it for the active quality only
				while (new_rep->segment_list->xlink_href && !same_rep) {
					u32 retry=10;
					Bool is_static = GF_FALSE;
					u64 dur = 0;

					if (new_rep->segment_list->consecutive_xlink_count) {
						GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Resolving a XLINK pointed from another XLINK (%d consecutive XLINK in segment list)\n", new_rep->segment_list->consecutive_xlink_count));
					}

					while (retry) {
						if (dash->is_m3u8) {
							if (!rep_idx)
								rep_idx = 0;
							e = gf_dash_solve_m3u8_representation_xlink(group, new_rep, &is_static, &dur, rep->playback.xlink_digest);
						} else {
							e = gf_dash_solve_representation_xlink(group->dash, new_rep, rep->playback.xlink_digest);
						}
						if (e==GF_OK) break;
						if (e==GF_EOS) {
							same_rep = GF_TRUE;
							e = GF_OK;
							break;
						}
						if (e==GF_NON_COMPLIANT_BITSTREAM) break;
						if (e==GF_OUT_OF_MEM) break;
						if (group->dash->dash_state != GF_DASH_STATE_RUNNING)
							break;

						gf_sleep(100);
						retry --;
					}

					if (same_rep) break;

					if (e==GF_OK) {
						if (dash->is_m3u8 && is_static) {
							GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[m3u8] MPD type changed from dynamic to static\n"));
							group->dash->mpd->type = GF_MPD_TYPE_STATIC;
							group->dash->mpd->minimum_update_period = 0;
							if (group->dash->mpd->media_presentation_duration < dur)
								group->dash->mpd->media_presentation_duration = dur;
							if (group->period->duration < dur)
								group->period->duration = dur;
						}
					}
				}

				if (same_rep==GF_TRUE) {
					rep->playback.not_modified = GF_TRUE;
					nb_rep_unchanged ++;
					has_reps_unchanged = GF_TRUE;
					if (hls_temp_rep)
						gf_mpd_representation_free(hls_temp_rep);
					if (group->hls_next_seq_num) {
						HLS_MIN_RELOAD_TIME(dash)
					}
					continue;
				}

				if (!new_rep->segment_list && (!new_set || !new_set->segment_list) && (!new_period || !new_period->segment_list)) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update playlist: representation does not use segment list as previous version\n"));
					gf_mpd_del(new_mpd);
					return GF_NON_COMPLIANT_BITSTREAM;
				}
				/*get the segment list*/
				segments = new_segments = NULL;
				if (period->segment_list && period->segment_list->segment_URLs) segments = period->segment_list->segment_URLs;
				if (set->segment_list && set->segment_list->segment_URLs) segments = set->segment_list->segment_URLs;
				if (rep->segment_list && rep->segment_list->segment_URLs) segments = rep->segment_list->segment_URLs;

				if (new_period && new_period->segment_list && new_period->segment_list->segment_URLs) new_segments = new_period->segment_list->segment_URLs;
				if (new_set && new_set->segment_list && new_set->segment_list->segment_URLs) new_segments = new_set->segment_list->segment_URLs;
				if (new_rep->segment_list && new_rep->segment_list->segment_URLs) new_segments = new_rep->segment_list->segment_URLs;

				Bool skip_next_seg_url = GF_FALSE;
				Bool has_ll_hls = GF_FALSE;
				Bool live_edge_passed = GF_FALSE;
				u32 dld_index_offset = 0;
				GF_MPD_SegmentURL *first_added_chunk = NULL;
				GF_MPD_SegmentURL *hls_last_chunk = group->llhls_edge_chunk;

				if (dash->is_m3u8 && group->is_low_latency) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Representation #%d: merging segments, current live chunk %s\n", rep_idx+1, hls_last_chunk ? hls_last_chunk->media : "none"));

					//active rep, figure out the diff between the scheduled last chunk and the download index
					//typically if update_mpd is called after a segment is downloaded but before next segment is evaluated for download
					//there is a diff of 1 seg
					//we reapply this diff when updating download_segment_index below
					if ((group->active_rep_index==rep_idx) && hls_last_chunk) {
						s32 pos = gf_list_find(segments, hls_last_chunk);
						if ((pos>=0) && (pos < group->download_segment_index))
							dld_index_offset = (s32) group->download_segment_index - pos;
					}
				}

//#define DUMP_LIST

				if (dash->is_m3u8) {
					//preprocess new segment list, starting from end and locate the most recent full segment already present
					// in old list before the live edge
					//we start browsing from old list since it is likely the last segments in new list are not present at all in the old
					u32 nb_new_segs = gf_list_count(new_segments);
					u32 nb_old_segs = gf_list_count(segments);
					s32 purge_segs_until = -1;

#ifdef DUMP_LIST
					fprintf(stderr, "New list received:\n");
					for (i=0; i<nb_new_segs; i++) {
						GF_MPD_SegmentURL *surl = gf_list_get(new_segments, i);
						fprintf(stderr, "\tsegment %s - chunk type %d\n", surl->media, surl->hls_ll_chunk_type);
					}
#endif


					for (j=nb_old_segs; j>0; j--) {
						GF_MPD_SegmentURL *old_seg = gf_list_get(segments, j-1);
						//ignore chunks
						if (old_seg->hls_ll_chunk_type) continue;
						//if live edge dont't trash anything after it, append and parts merge is done below
						if (hls_last_chunk && (hls_last_chunk->hls_seq_num <= old_seg->hls_seq_num)) continue;
						for (i=nb_new_segs; i>0; i--) {
							GF_MPD_SegmentURL *new_seg = gf_list_get(new_segments, i-1);
							//ignore chunks
							if (new_seg->hls_ll_chunk_type) continue;

							//match, we will trash everything in new list until this point
							if (old_seg->hls_seq_num == new_seg->hls_seq_num) {
								purge_segs_until = (s32) i-1;
								break;
							}
						}
						if (purge_segs_until>=0) break;
					}
					while (purge_segs_until>=0) {
						GF_MPD_SegmentURL *new_seg = gf_list_pop_front(new_segments);
						gf_mpd_segment_url_free(new_seg);
						purge_segs_until--;
					}
				}

				//browse new list - this is for both DASH or HLS
				for (i=0; i<gf_list_count(new_segments); i++) {
					GF_MPD_SegmentURL *new_seg = gf_list_get(new_segments, i);
					u32 nb_segs = gf_list_count(segments);
					Bool found = GF_FALSE;

					//part of a segment
					if (new_seg->hls_ll_chunk_type) {
						has_ll_hls = GF_TRUE;

						//remaining chunk of our live edge, add it (skip loop below)
						if (skip_next_seg_url)
							nb_segs = 0;
					}

					//find this segment in the old list
					for (j = 0; j<nb_segs; j++) {
						GF_MPD_SegmentURL *seg = gf_list_get(segments, j);

						//compare only segs or parts
						if (new_seg->hls_ll_chunk_type && !seg->hls_ll_chunk_type)
							continue;
						if (!new_seg->hls_ll_chunk_type && seg->hls_ll_chunk_type)
							continue;

						//full segment
						if (!seg->media || !new_seg->media || strcmp(seg->media, new_seg->media))
							continue;

						if (seg->media_range && new_seg->media_range) {
							if ((seg->media_range->start_range != new_seg->media_range->start_range) ||
								(seg->media_range->end_range != new_seg->media_range->end_range)
							) {
								continue;
							}
						}
						//we already had the segment, do not add
						found = 1;

						//this is the live edge !
						if (seg==hls_last_chunk) {
							skip_next_seg_url = GF_TRUE;
						}
						break;
					}

					//remove all part before live edge - parts after live edge with a full seg following will be purged once merged
					if (!found && !skip_next_seg_url && new_seg->hls_ll_chunk_type && !live_edge_passed) {
						found = GF_TRUE;
					}
					//first full seg after our live edge, insert before the first fragment of this segment still in our list
					if (!new_seg->hls_ll_chunk_type && skip_next_seg_url && !found) {
						//starting from our current live edge, rewind and insert after the first full segment found
						s32 pos = group->download_segment_index;
						while (pos>0) {
							GF_MPD_SegmentURL *prev = gf_list_get(segments, pos);
							if (!prev) break;
							if (!prev->hls_ll_chunk_type) {
								gf_list_insert(segments, new_seg, pos+1);
								pos = pos+1;
								break;
							}
							pos--;
						}
						assert(pos>=0);
						if (pos==0) {
							gf_list_insert(segments, new_seg, 0);
						}
						//remove from new segments
						gf_list_rem(new_segments, i);
						i--;

						GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Representation #%d: Injecting segment %s before LL chunk\n", rep_idx+1, new_seg->media));
						found = GF_TRUE;
						//no longer at live edge point
						skip_next_seg_url = GF_FALSE;
						//edge is now passed, do not discard following parts
						live_edge_passed = GF_TRUE;
						first_added_chunk = NULL;
					}

					/*this is a new segment, merge it: we remove from new list and push to old one, before doing a final swap
					this ensures that indexing in the segment_list is still correct after merging*/
					if (!found) {
						gf_list_rem(new_segments, i);
						i--;

						if (new_seg->hls_ll_chunk_type) {
							if (!first_added_chunk) {
								first_added_chunk = new_seg;
							}
						} else {
							//we just flushed a new full seg, remove all parts
							if (first_added_chunk) {
								s32 pos = gf_list_find(segments, first_added_chunk);
								assert(pos>=0);
								while (pos < (s32) gf_list_count(segments)) {
									GF_MPD_SegmentURL *surl = gf_list_pop_back(segments);
									assert(surl->hls_ll_chunk_type);
									gf_mpd_segment_url_free(surl);
								}
								first_added_chunk = NULL;
							}
						}

						gf_list_add(segments, new_seg);
						GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Representation #%d: Adding new segment %s\n", rep_idx+1, new_seg->media));
					}
				}
				/*what else should we check ?*/

#ifdef DUMP_LIST
				fprintf(stderr, "updated segment list before purge\n");
				for (i=0; i<gf_list_count(segments); i++) {
					GF_MPD_SegmentURL *surl = gf_list_get(segments, i);
					fprintf(stderr, "\tsegment %s - chunk type %d\n", surl->media, surl->hls_ll_chunk_type);
				}
#endif

				if (dash->is_m3u8) {
					//also remove all segments older than min seq num in new rep
					while (1) {
						GF_MPD_SegmentURL *old_seg = gf_list_get(segments, 0);
						if (!old_seg) break;
						if (old_seg->hls_seq_num >= new_rep->m3u8_media_seq_min)
							break;

						gf_list_rem(segments, 0);
						gf_mpd_segment_url_free(old_seg);
						//rewind the current download index for non low-latency cases
						//it is recomputed below for low-latency cases
						if (group->download_segment_index) {
							group->download_segment_index--;
						}
					}
				} else {
					//todo for MPD with rep update on xlink (no existing profile use this)
					GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Representation using segment list in live: purging not implemented, patch welcome\n"));
				}

				/*swap segment list content*/
				gf_list_swap(new_segments, segments);

				/*LL-HLS, cleanup everything except part around "our" live edge and part at manifest live edge*/
				if (has_ll_hls) {
					s32 live_edge_idx = -1;
					//active rep, find live edge index before purge
					if (group->llhls_edge_chunk && (group->active_rep_index==rep_idx)) {
						live_edge_idx = gf_list_find(new_segments, group->llhls_edge_chunk);
					}
					ls_hls_purge_segments(live_edge_idx, new_segments);

					//active rep, update download_segment_index after purge
					if (group->llhls_edge_chunk && (group->active_rep_index==rep_idx)) {
						live_edge_idx = gf_list_find(new_segments, group->llhls_edge_chunk);
						if (live_edge_idx>=0)
							group->download_segment_index = (u32) live_edge_idx + dld_index_offset;
					}
				}

#ifdef DUMP_LIST
				fprintf(stderr, "%d updated segment list - min/max seq num in new list %d / %d\n", gf_sys_clock(), new_rep->m3u8_media_seq_min, new_rep->m3u8_media_seq_max);

				for (i=0; i<gf_list_count(new_segments); i++) {
					GF_MPD_SegmentURL *surl = gf_list_get(new_segments, i);
					fprintf(stderr, "\tsegment %s - chunk type %d\n", surl->media, surl->hls_ll_chunk_type);
				}
#endif

				//HLS live: if a new time is set (active group only), we just switched between qualities
				//locate the segment with the same start time in the manifest, and purge previous ones
				//it may happen that the manifest does still not contain the segment we are looking for, force an MPD update
				if (group->hls_next_seq_num && (group->active_rep_index==rep_idx)) {
					Bool found = GF_FALSE;
					u32 k, count = gf_list_count(new_segments);

					//browse new segment list in reverse order, looking for our desired seq num
					for (k=count; k>0; k--) {
						GF_MPD_SegmentURL *segu = (GF_MPD_SegmentURL *) gf_list_get(new_segments, k-1);
						if (segu->hls_ll_chunk_type && !segu->is_first_part)
							continue;

						if (group->hls_next_seq_num == segu->hls_seq_num) {
							group->download_segment_index = k-1;
							found = GF_TRUE;
							GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] HLS switching qualities on %s%s\n", segu->media, segu->hls_ll_chunk_type ? " - live edge" : ""));
							break;
						}
						/*
						"In order to play the presentation normally, the next Media Segment to load is the one with the
						lowest Media Sequence Number that is greater than the Media Sequence Number of the last Media Segment loaded."

						so we store this one, but continue until we find an exact match, if any
						*/
						else if (segu->hls_seq_num > group->hls_next_seq_num) {
							group->download_segment_index = k-1;
							found = GF_TRUE;
						}
						//seg num is lower than our requested next, abort browsing
						else
							break;
					}
					//not yet available
					if (!found) {
						//use group last modification time
						u32 timer = gf_sys_clock() - group->last_mpd_change_time;
						if (!group->segment_duration || (timer < group->segment_duration * 2000) ) {
							GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Cannot find segment for given HLS SN %d - forcing manifest update\n", group->hls_next_seq_num));
							HLS_MIN_RELOAD_TIME(dash)
						} else {
							GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Segment list has not been updated for more than %d ms - assuming end of period\n", timer));
							gf_dash_mark_group_done(group);
							group->hls_next_seq_num = 0;
						}
					} else {
						group->hls_next_seq_num = 0;
					}
				}

				/*current representation is the active one in the group - update the number of segments*/
				if (group->active_rep_index==rep_idx) {
					group->nb_segments_in_rep = gf_list_count(new_segments);
				}
			}

			e = gf_dash_merge_segment_timeline(group, NULL, rep->segment_list, rep->segment_template, new_rep->segment_list, new_rep->segment_template, timeline_start_time);
			if (e) {
				gf_mpd_del(new_mpd);
				return e;
			}

			//move redirections in representations base URLs (we could do that on AS as well )
			if (rep->base_URLs && new_rep->base_URLs) {
				u32 k;
				for (i=0; i<gf_list_count(new_rep->base_URLs); i++) {
					GF_MPD_BaseURL *n_url = gf_list_get(new_rep->base_URLs, i);
					if (!n_url->URL) continue;

					for (k=0; k<gf_list_count(rep->base_URLs); k++) {
						GF_MPD_BaseURL *o_url = gf_list_get(rep->base_URLs, k);
						if (o_url->URL && !strcmp(o_url->URL, n_url->URL)) {
							n_url->redirection = o_url->redirection;
							o_url->redirection = NULL;
						}
					}
				}
			}

			/*what else should we check ??*/


			if (hls_temp_rep) {
				//reswap segment lists: new segments contain the actual list
				rep->segment_list->segment_URLs = new_segments;
				new_rep->segment_list->segment_URLs = segments;
				//gf_list_swap(new_segments, segments);

				//destroy temporary rep
				gf_mpd_representation_free(hls_temp_rep);
				group->last_mpd_change_time = gf_sys_clock();
			} else {
				/*switch all internal GPAC stuff*/
				memcpy(&new_rep->playback, &rep->playback, sizeof(GF_DASH_RepresentationPlayback));
				if (rep->playback.cached_init_segment_url) rep->playback.cached_init_segment_url = NULL;

				if (!new_rep->mime_type) {
					new_rep->mime_type = rep->mime_type;
					rep->mime_type = NULL;
				}
			}
		}

		/*update segmentTimeline at AdaptationSet level before switching the set (old setup needed to compute current timing of each group) */
		if (new_set) {
			e = gf_dash_merge_segment_timeline(group, NULL, set->segment_list, set->segment_template, new_set->segment_list, new_set->segment_template, timeline_start_time);
			if (e) {
				gf_mpd_del(new_mpd);
				return e;
			}

			if (nb_rep_unchanged == gf_list_count(new_set->representations))
				nb_group_unchanged++;
			else
				group->last_mpd_change_time = gf_sys_clock();
		}
	}
	//good to go, switch pointers
	for (group_idx=0; group_idx<gf_list_count(dash->groups) && new_period; group_idx++) {
		Double seg_dur;
		Bool reset_segment_count;
		GF_MPD_AdaptationSet *new_as;
		GF_DASH_Group *group = gf_list_get(dash->groups, group_idx);

		/*update group/period to new period*/
		j = gf_list_find(group->period->adaptation_sets, group->adaptation_set);
		new_as = gf_list_get(new_period->adaptation_sets, j);
		assert(new_as);

		if (has_reps_unchanged) {
			//swap all unchanged reps from old MPD to new MPD
			for (j=0; j<gf_list_count(group->adaptation_set->representations); j++) {
				GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, j);
				GF_MPD_Representation *new_rep = gf_list_get(new_as->representations, j);
				if (!rep->playback.not_modified) continue;
				gf_list_rem(group->adaptation_set->representations, j);
				gf_list_rem(new_as->representations, j);
				gf_list_insert(group->adaptation_set->representations, new_rep, j);
				gf_list_insert(new_as->representations, rep, j);
				rep->playback.not_modified = GF_FALSE;
			}
		}

		/*swap group period/AS to new period/AS*/
		group->adaptation_set = new_as;
		group->period = new_period;

		j = gf_list_count(group->adaptation_set->representations);
		assert(j);

		/*now that all possible SegmentXXX have been updated, purge them if needed: all segments ending before timeline_start_time
		will be removed from MPD*/
		if (timeline_start_time) {
			u32 nb_segments_removed = gf_dash_purge_segment_timeline(group, timeline_start_time);
			if (nb_segments_removed) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] AdaptationSet %d - removed %d segments from timeline (%d since start of the period)\n", group_idx+1, nb_segments_removed, group->nb_segments_purged));
			}
		}

		if (force_timeline_setup) {
			group->timeline_setup = GF_FALSE;
			group->start_number_at_last_ast = 0;
			gf_dash_group_timeline_setup(new_mpd, group, fetch_time);
		}
		else if (new_mpd->availabilityStartTime != dash->mpd->availabilityStartTime) {
			s64 diff = new_mpd->availabilityStartTime;
			diff -= dash->mpd->availabilityStartTime;
			if (diff < 0) diff = -diff;
			if (diff>3000)
				gf_dash_group_timeline_setup(new_mpd, group, fetch_time);
		}

		group->maybe_end_of_stream = 0;
		reset_segment_count = GF_FALSE;
		/*compute fetchTime + minUpdatePeriod and check period end time*/
		if (new_mpd->minimum_update_period && new_mpd->media_presentation_duration) {
			u64 endTime = fetch_time - new_mpd->availabilityStartTime - period->start;
			if (endTime > new_mpd->media_presentation_duration) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Period EndTime is signaled to "LLU", less than fetch time "LLU" ! Ignoring mediaPresentationDuration\n", new_mpd->media_presentation_duration, endTime));
				new_mpd->media_presentation_duration = 0;
				reset_segment_count = GF_TRUE;
			} else {
				endTime += new_mpd->minimum_update_period;
				if (endTime > new_mpd->media_presentation_duration) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Period EndTime is signaled to "LLU", less than fetch time + next update "LLU" - maybe end of stream ?\n", new_mpd->availabilityStartTime, endTime));
					group->maybe_end_of_stream = 1;
				}
			}
		}

		/*update number of segments in active rep*/
		gf_dash_get_segment_duration(gf_list_get(group->adaptation_set->representations, group->active_rep_index), group->adaptation_set, group->period, new_mpd, &group->nb_segments_in_rep, &seg_dur);

		if (reset_segment_count) {
			u32 nb_segs_in_mpd_period = (u32) (dash->mpd->minimum_update_period / (1000*seg_dur) );
			group->nb_segments_in_rep = group->download_segment_index + nb_segs_in_mpd_period;
		}
		/*check if number of segments are coherent ...*/
		else if (!group->maybe_end_of_stream && new_mpd->minimum_update_period && new_mpd->media_presentation_duration) {
			u32 nb_segs_in_mpd_period = (u32) (dash->mpd->minimum_update_period / (1000*seg_dur) );

			if (group->download_segment_index + nb_segs_in_mpd_period >= group->nb_segments_in_rep) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Period has %d segments but %d are needed until next refresh. Maybe end of stream is near ?\n", group->nb_segments_in_rep, group->download_segment_index + nb_segs_in_mpd_period));
				group->maybe_end_of_stream = 1;
			}
		}

		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Updated AdaptationSet %d - %d segments\n", group_idx+1, group->nb_segments_in_rep));

		if (!period->duration && new_period->duration) {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("End of period upcoming, current segment index for group #%d: %d\n", group_idx+1, group->download_segment_index));
			if (group->download_segment_index > (s32) group->nb_segments_in_rep)
				gf_dash_mark_group_done(group);
		}

	}

exit:
	/*swap MPDs*/
	if (new_mpd) {
		if (dash->mpd) {
			if (!new_mpd->minimum_update_period && (new_mpd->type==GF_MPD_TYPE_DYNAMIC))
				new_mpd->minimum_update_period = dash->mpd->minimum_update_period;
			gf_mpd_del(dash->mpd);
		}
		dash->mpd = new_mpd;

		if (dash->chaining_mode) {
			dash_check_chaining(dash, "urn:mpeg:dash:mpd-chaining:2016", &dash->chain_next, 0);
			dash_check_chaining(dash, "urn:mpeg:dash:fallback:2016", &dash->chain_fallback, 0);
			dash_check_chaining(dash, "urn:mpeg:dash:origin-set:2019", NULL, GF_DASH_CHAIN_PUSH);
			dash_check_chaining(dash, "urn:mpeg:dash:origin-back:2019", NULL, GF_DASH_CHAIN_POP);
		}
	}
	dash->last_update_time = gf_sys_clock();
	dash->mpd_fetch_time = fetch_time;

#ifndef GPAC_DISABLE_LOG
	if (new_period) {
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Manifest after update:\n"));
		for (i=0; i<gf_list_count(new_mpd->periods); i++) {
			GF_MPD_Period *ap = gf_list_get(new_mpd->periods, i);
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("\tP#%d: start "LLU" - duration " LLU" - xlink %s\n", i+1, ap->start, ap->duration, ap->xlink_href ? ap->xlink_href : ap->origin_base_url ? ap->origin_base_url : "none"));
		}
	}
#endif

	return GF_OK;
}

static void m3u8_setup_timeline(GF_DASH_Group *group, GF_MPD_Representation *rep)
{
	u64 timeshift = 0;
	u64 tsb_depth = 0;
	u32 i, count;

	if (!group->dash->initial_time_shift_value || !rep->segment_list) return;

	count = gf_list_count(rep->segment_list->segment_URLs);
	for (i=0; i<count; i++) {
		GF_MPD_SegmentURL *s = gf_list_get(rep->segment_list->segment_URLs, i);
		tsb_depth += s->duration;
	}

	if (group->dash->initial_time_shift_value<=100) {
		timeshift = tsb_depth;
		timeshift *= group->dash->initial_time_shift_value;
		timeshift /= 100;
	} else {
		timeshift = (u32) group->dash->initial_time_shift_value;
		timeshift *= rep->segment_list->timescale;
		timeshift /= 1000;
		if (timeshift > tsb_depth) timeshift = tsb_depth;
	}
	tsb_depth = 0;
	for (i=count; i>0; i--) {
		GF_MPD_SegmentURL *s = gf_list_get(rep->segment_list->segment_URLs, i-1);
		tsb_depth += s->duration;
		//only check on independent parts
		if (s->hls_ll_chunk_type == 1)
			continue;
		if (tsb_depth > timeshift) {
			group->download_segment_index = i-1;
			break;
		}
	}
}


static GF_Err gf_dash_resolve_url(GF_MPD *mpd, GF_MPD_Representation *rep, GF_DASH_Group *group, const char *mpd_url, GF_MPD_URLResolveType resolve_type, u32 item_index, char **out_url, u64 *out_range_start, u64 *out_range_end, u64 *segment_duration, Bool *is_in_base_url, char **out_key_url, bin128 *out_key_iv, Bool *data_url_process, u32 *out_start_number)
{
	GF_Err e;
	GF_MPD_AdaptationSet *set = group->adaptation_set;
	GF_MPD_Period *period = group->period;
	u32 timescale;

	if (!mpd_url) return GF_BAD_PARAM;

	if (!strncmp(mpd_url, "gfio://", 7))
		mpd_url = gf_file_basename(gf_fileio_translate_url(mpd_url));

	if (!group->timeline_setup) {
		gf_dash_group_timeline_setup(mpd, group, 0);
		//we must wait for ROUTE clock to initialize, even if first period is static remote (we need to know when to tune)
		if (group->dash->route_clock_state==1)
			return GF_IP_NETWORK_EMPTY;

		if (group->dash->reinit_period_index)
			return GF_IP_NETWORK_EMPTY;

		group->timeline_setup = GF_TRUE;
		item_index = group->download_segment_index;
	}

	gf_mpd_resolve_segment_duration(rep, set, period, segment_duration, &timescale, NULL, NULL);
	*segment_duration = (resolve_type==GF_MPD_RESOLVE_URL_MEDIA) ? (u32) ((Double) ((*segment_duration) * 1000.0) / timescale) : 0;
	e = gf_mpd_resolve_url(mpd, rep, set, period, mpd_url, group->current_base_url_idx, resolve_type, item_index, group->nb_segments_purged, out_url, out_range_start, out_range_end, segment_duration, is_in_base_url, out_key_url, out_key_iv, out_start_number);


	if (e == GF_NON_COMPLIANT_BITSTREAM) {
//		group->selection = GF_DASH_GROUP_NOT_SELECTABLE;
	}
	if (!*out_url) {
		return e;
	}

	if (*out_url && data_url_process && !strncmp(*out_url, "data:", 5)) {
		char *sep;
		sep = strstr(*out_url, ";base64,");
		if (sep) {
			GF_Blob *blob;
			u32 len;
			sep+=8;
			len = (u32)strlen(sep) + 1;
			GF_SAFEALLOC(blob, GF_Blob);
			if (!blob) return GF_OUT_OF_MEM;

			blob->data = (char *)gf_malloc(len);
			blob->size = gf_base64_decode(sep, len, blob->data, len);
			sprintf(*out_url, "gmem://%p", blob);
			*data_url_process = GF_TRUE;
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("data scheme with encoding different from base64 not supported\n"));
		}
	}

	return e;
}


static void gf_dash_set_group_representation(GF_DASH_Group *group, GF_MPD_Representation *rep, Bool is_next_schedule)
{
#ifndef GPAC_DISABLE_LOG
	u32 width=0, height=0, samplerate=0;
	GF_MPD_Fractional *framerate=NULL;
#endif
	u32 k;
	s32 timeshift;
	GF_MPD_AdaptationSet *set;
	GF_MPD_Period *period;
	u32 ol_nb_segs_in_rep;
	u32 i = gf_list_find(group->adaptation_set->representations, rep);
	u32 prev_active_rep_index = group->active_rep_index;
	u32 nb_cached_seg_per_rep = group->max_cached_segments / gf_dash_group_count_rep_needed(group);
	assert((s32) i >= 0);

	if (group->llhls_edge_chunk && group->llhls_edge_chunk->hls_ll_chunk_type) {
		group->llhls_switch_request = i;
		return;
	}
	group->llhls_switch_request = -1;

	//we do not support switching in the middle of a segment
	if (group->llhls_edge_chunk && group->llhls_edge_chunk->hls_ll_chunk_type) {
		return;
	}

	/* in case of dependent representations: we set max_complementary_rep_index than active_rep_index*/
	if (group->base_rep_index_plus_one)
		group->max_complementary_rep_index = i;
	else {
		group->active_rep_index = i;
//			if (group->timeline_setup)
//				group->llhls_edge_chunk = NULL;
	}
	group->active_bitrate = rep->bandwidth;
	group->max_cached_segments = nb_cached_seg_per_rep * gf_dash_group_count_rep_needed(group);
	ol_nb_segs_in_rep = group->nb_segments_in_rep;

	group->min_bandwidth_selected = GF_TRUE;
	for (k=0; k<gf_list_count(group->adaptation_set->representations); k++) {
		GF_MPD_Representation *arep = gf_list_get(group->adaptation_set->representations, k);
		if (group->active_bitrate > arep->bandwidth) {
			group->min_bandwidth_selected = GF_FALSE;
			break;
		}
	}

	while (rep->segment_list && rep->segment_list->xlink_href) {
		Bool is_static = GF_FALSE;
		u64 dur = 0;
		GF_Err e=GF_OK;

		if (rep->segment_list->consecutive_xlink_count) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Resolving a XLINK pointed from another XLINK (%d consecutive XLINK in segment list)\n", rep->segment_list->consecutive_xlink_count));
		}

		if (group->dash->is_m3u8) {
			e = gf_dash_solve_m3u8_representation_xlink(group, rep, &is_static, &dur, rep->playback.xlink_digest);
		} else {
			e = gf_dash_solve_representation_xlink(group->dash, rep, rep->playback.xlink_digest);
		}
		if (is_static)
			group->dash->mpd->type = GF_MPD_TYPE_STATIC;

		//after resolving xlink: if this represenstation is marked as disabled, we have nothing to do
		if (rep->playback.disabled)
			return;

		if (e) {
			if (group->dash->dash_state != GF_DASH_STATE_RUNNING) {
				group->dash->force_period_reload = 1;
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Could not reslove XLINK %s of initial rep, will retry\n", (rep->segment_list && rep->segment_list->xlink_href) ? rep->segment_list->xlink_href : "", gf_error_to_string(e) ));
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Could not reslove XLINK %s in time: %s - using old representation\n", (rep->segment_list && rep->segment_list->xlink_href) ? rep->segment_list->xlink_href : "", gf_error_to_string(e) ));
				group->active_rep_index = prev_active_rep_index;
			}
			return;
		}

		//we only know this is a static or dynamic MPD after parsing the first subplaylist
		//if this is static, we need to update infos in mpd and period
		if (group->dash->is_m3u8 && is_static) {
			group->dash->mpd->type = GF_MPD_TYPE_STATIC;
			group->dash->mpd->minimum_update_period = 0;
			if (group->dash->mpd->media_presentation_duration < dur)
				group->dash->mpd->media_presentation_duration = dur;
			if (group->period->duration < dur)
				group->period->duration = dur;
		}
	}

	if (group->dash->is_m3u8) {
		//here we change to another representation: we need to remove all URLs from segment list and adjust the download segment index for this group
		if (group->dash->dash_state == GF_DASH_STATE_RUNNING) {
			u32 group_dld_index = group->download_segment_index;
			u32 next_media_seq;
			Bool found = GF_FALSE;
			Bool is_dynamic = (group->dash->mpd->type==GF_MPD_TYPE_DYNAMIC) ? GF_TRUE : GF_FALSE;
			u32 nb_segs;
			GF_MPD_Representation *prev_active_rep = (GF_MPD_Representation *)gf_list_get(group->adaptation_set->representations, prev_active_rep_index);
			GF_MPD_SegmentURL *last_seg_url;
			assert(rep->segment_list);
			assert(prev_active_rep->segment_list);

			last_seg_url = gf_list_get(prev_active_rep->segment_list->segment_URLs, group_dld_index);
			if (last_seg_url)
				next_media_seq = last_seg_url->hls_seq_num;
			else {
				last_seg_url = gf_list_get(prev_active_rep->segment_list->segment_URLs, group_dld_index-1);
				if (last_seg_url)
					next_media_seq = last_seg_url->hls_seq_num+1;
				else
					next_media_seq = rep->m3u8_media_seq_max;
			}

			nb_segs = gf_list_count(rep->segment_list->segment_URLs);
			for (k=nb_segs; k>0; k--) {
				GF_MPD_SegmentURL *seg_url = (GF_MPD_SegmentURL *) gf_list_get(rep->segment_list->segment_URLs, k-1);
				if (seg_url->hls_ll_chunk_type) continue;

				if (next_media_seq == seg_url->hls_seq_num) {
					group->download_segment_index = k-1;
					found = GF_TRUE;
					break;
				}
				/*
				"In order to play the presentation normally, the next Media Segment to load is the one with the
				lowest Media Sequence Number that is greater than the Media Sequence Number of the last Media Segment loaded."

				so we store this one, but continue until we find an exact match, if any
				*/
				else if (next_media_seq < seg_url->hls_seq_num) {
					group->download_segment_index = k-1;
					found = GF_TRUE;
				}
				//segment before our current target, abort
				else {
					break;
				}
			}

			if (!found) {
				if (is_dynamic) {
					//we switch quality but next seg is not known, force an update NOW
					group->dash->force_mpd_update = GF_TRUE;
					group->hls_next_seq_num = next_media_seq;
				} else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] next media segment %d not found in new variant stream (min %d - max %d), aborting\n", next_media_seq, rep->m3u8_media_seq_min, rep->m3u8_media_seq_max));
					group->done = GF_TRUE;
				}
			} else {
				if (is_dynamic) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] HLS next seg %d found in new rep, no manifest update\n", next_media_seq));
				}
			}
		}

		//switching to a rep for which we didn't solve the init segment yet
		if (!rep->playback.cached_init_segment_url) {
			GF_Err e;
			Bool timeline_setup = group->timeline_setup;
			char *r_base_init_url = NULL;
			u64 r_start = 0, r_end = 0, r_dur = 0;

			e = gf_dash_resolve_url(group->dash->mpd, rep, group, group->dash->base_url, GF_MPD_RESOLVE_URL_INIT, 0, &r_base_init_url, &r_start, &r_end, &r_dur, NULL, &rep->playback.key_url, &rep->playback.key_IV, &rep->playback.owned_gmem, NULL);

			group->timeline_setup = timeline_setup;
			if (!e && r_base_init_url) {
				rep->playback.cached_init_segment_url = r_base_init_url;
				rep->playback.init_start_range = r_start;
				rep->playback.init_end_range = r_end;
				rep->playback.init_seg_name_start = dash_strip_base_url(r_base_init_url, group->dash->base_url);
			} else if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot solve initialization segment for representation: %s - discarding representation\n", gf_error_to_string(e) ));
				rep->playback.disabled = 1;
			}
		}

		group->m3u8_start_media_seq = rep->m3u8_media_seq_min;
		if (rep->m3u8_low_latency)
			group->is_low_latency = GF_TRUE;
		if (group->dash->mpd->type==GF_MPD_TYPE_STATIC)
			group->timeline_setup = GF_TRUE;

		if (!group->current_downloaded_segment_duration && rep->segment_list && rep->segment_list->timescale)
			group->current_downloaded_segment_duration = rep->segment_list->duration * 1000 / rep->segment_list->timescale;

		//setup tune point
		if (!group->timeline_setup) {
			//tune to last entry (live edge)
			group->download_segment_index = rep->m3u8_media_seq_indep_last;
			if (rep->m3u8_low_latency && rep->segment_list) {
				u32 nb_removed = ls_hls_purge_segments(group->download_segment_index, rep->segment_list->segment_URLs);
				group->download_segment_index -= nb_removed;
			}
			//if TSB set, roll back
			m3u8_setup_timeline(group, rep);
			group->timeline_setup = GF_TRUE;
			group->first_hls_chunk = GF_TRUE;
		} else {
			if (rep->m3u8_low_latency && rep->segment_list) {
				ls_hls_purge_segments(-1, rep->segment_list->segment_URLs);
			}
		}
	}

	set = group->adaptation_set;
	period = group->period;

#ifndef GPAC_DISABLE_LOG

#define GET_REP_ATTR(_a)	_a = rep->_a; if (!_a) _a = set->_a;

	GET_REP_ATTR(width);
	GET_REP_ATTR(height);
	GET_REP_ATTR(samplerate);
	GET_REP_ATTR(framerate);

	if (width || height) {
		u32 num=25, den=1;
		if (framerate) {
			num = framerate->num;
			if (framerate->den) den = framerate->den;
		}

		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] AS#%d changed quality to bitrate %d kbps - Width %d Height %d FPS %d/%d (playback speed %g)\n", 1+group->groups_idx, rep->bandwidth/1000, width, height, num, den, group->dash->speed));
	}
	else if (samplerate) {
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] AS#%d changed quality to bitrate %d kbps - sample rate %u (playback speed %g)\n", 1+group->groups_idx, rep->bandwidth/1000, samplerate, group->dash->speed));
	}
	else {
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] AS#%d changed quality to bitrate %d kbps (playback speed %g)\n", 1+group->groups_idx, rep->bandwidth/1000, group->dash->speed));
	}
#endif

	gf_dash_get_segment_duration(rep, set, period, group->dash->mpd, &group->nb_segments_in_rep, &group->segment_duration);

	/*if broken indication in duration restore previous seg count*/
	if (group->dash->ignore_mpd_duration)
		group->nb_segments_in_rep = ol_nb_segs_in_rep;

	timeshift = (s32) (rep->segment_base ? rep->segment_base->time_shift_buffer_depth : (rep->segment_list ? rep->segment_list->time_shift_buffer_depth : (rep->segment_template ? rep->segment_template->time_shift_buffer_depth : -1) ) );
	if (timeshift == -1) timeshift = (s32) (set->segment_base ? set->segment_base->time_shift_buffer_depth : (set->segment_list ? set->segment_list->time_shift_buffer_depth : (set->segment_template ? set->segment_template->time_shift_buffer_depth : -1) ) );
	if (timeshift == -1) timeshift = (s32) (period->segment_base ? period->segment_base->time_shift_buffer_depth : (period->segment_list ? period->segment_list->time_shift_buffer_depth : (period->segment_template ? period->segment_template->time_shift_buffer_depth : -1) ) );

	if (timeshift == -1) timeshift = (s32) group->dash->mpd->time_shift_buffer_depth;
	group->time_shift_buffer_depth = (u32) timeshift;

	group->dash->dash_io->on_dash_event(group->dash->dash_io, GF_DASH_EVENT_QUALITY_SWITCH, group->groups_idx, GF_OK);
}

static void gf_dash_switch_group_representation(GF_DashClient *mpd, GF_DASH_Group *group)
{
	u32 i, bandwidth, min_bandwidth;
	GF_MPD_Representation *rep_sel = NULL;
	GF_MPD_Representation *min_rep_sel = NULL;
	Bool min_bandwidth_selected = GF_FALSE;
	bandwidth = 0;
	min_bandwidth = (u32) -1;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Checking representations between %d and %d kbps\n", group->min_bitrate/1000, group->max_bitrate/1000));

	if (group->force_representation_idx_plus_one) {
		rep_sel = gf_list_get(group->adaptation_set->representations, group->force_representation_idx_plus_one - 1);
		group->force_representation_idx_plus_one = 0;
	}

	if (!rep_sel) {
		for (i=0; i<gf_list_count(group->adaptation_set->representations); i++) {
			GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, i);
			if (rep->playback.disabled) continue;
			if ((rep->bandwidth > bandwidth) && (rep->bandwidth < group->max_bitrate )) {
				rep_sel = rep;
				bandwidth = rep->bandwidth;
			}
			if (rep->bandwidth < min_bandwidth) {
				min_rep_sel = rep;
				min_bandwidth = rep->bandwidth;
			}
		}
	}

	if (!rep_sel) {
		if (!min_rep_sel) {
			min_rep_sel = gf_list_get(group->adaptation_set->representations, 0);
		}
		rep_sel = min_rep_sel;
		min_bandwidth_selected = 1;
	}
	assert(rep_sel);
	i = gf_list_find(group->adaptation_set->representations, rep_sel);

	assert((s32) i >= 0);

	group->force_switch_bandwidth = 0;
	group->max_bitrate = 0;
	group->min_bitrate = (u32) -1;

	if (i != group->active_rep_index) {
		if (min_bandwidth_selected) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] No representation found with bandwidth below %d kbps - using representation @ %d kbps\n", group->max_bitrate/1000, rep_sel->bandwidth/1000));
		}
		gf_dash_set_group_representation(group, rep_sel, GF_FALSE);
	}
}

/* Estimate the maximum speed that we can play, using our statistic. If it is below the max_playout_rate in MPD, return max_playout_rate*/
static Double gf_dash_get_max_available_speed(GF_DashClient *dash, GF_DASH_Group *group, GF_MPD_Representation *rep)
{
	Double max_available_speed = 0;
	Double max_dl_speed, max_decoding_speed;
	u32 framerate;
	u32 bytes_per_sec;

	if (!group->irap_max_dec_time && !group->avg_dec_time) {
		return 0;
	}
	bytes_per_sec = group->backup_Bps;
	max_dl_speed = 8.0*bytes_per_sec / rep->bandwidth;

	//if framerate is not in MPD, suppose that it is 25 fps
	framerate = 25;
	if (rep->framerate) {
		framerate = rep->framerate->num;
		if (rep->framerate->den) {
			framerate /= rep->framerate->den;
		}
	}

	if (group->decode_only_rap)
		max_decoding_speed = group->irap_max_dec_time ? 1000000.0 / group->irap_max_dec_time : 0;
	else
		max_decoding_speed = group->avg_dec_time ? 1000000.0 / (group->max_dec_time + group->avg_dec_time*(framerate - 1)) : 0;
	max_available_speed = max_decoding_speed > max_dl_speed ? max_dl_speed : max_decoding_speed;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Representation %s max playout rate: in MPD %f - calculated by stat: %f\n", rep->id, rep->max_playout_rate, max_available_speed));

	return max_available_speed;
}

static void dash_store_stats(GF_DashClient *dash, GF_DASH_Group *group, u32 bytes_per_sec, u32 file_size, Bool is_broadcast, u32 cur_dep_idx, u64 us_since_start)
{
#ifndef GPAC_DISABLE_LOG
	const char *url=NULL, *full_url=NULL;
#endif
	GF_MPD_Representation *rep;

	if (!group->nb_cached_segments)
		return;

#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_DASH, GF_LOG_INFO)) {
		GF_DASH_Group *bgroup = group->depend_on_group ? group->depend_on_group : group;
		if (cur_dep_idx) {
			u32 i=0;
			while (i<bgroup->nb_cached_segments) {
				full_url = bgroup->cached[i].url;
				if (group->depend_on_group) {
					if (bgroup->cached[i].dep_group_idx==cur_dep_idx)
						break;
				} else {
					if (bgroup->cached[i].representation_index==cur_dep_idx)
						break;
				}
				i++;
			}
		} else {
			full_url = group->cached[0].url;
		}
		url = strrchr(full_url, '/');
		if (!url) url = strrchr(full_url, '\\');
		if (url) url+=1;
		else url = full_url;
	}
#endif

	
	if (!bytes_per_sec && group->local_files) {
		bytes_per_sec = (u32) -1;
		bytes_per_sec /= 8;
	}

	group->total_size = file_size;
	//in broadcast mode, just store the rate
	if (is_broadcast) group->bytes_per_sec = bytes_per_sec;
	//otherwise store the min rate we got (to deal with complementary representations)
	else if (!group->bytes_per_sec || group->bytes_per_sec > bytes_per_sec)
		group->bytes_per_sec = bytes_per_sec;

	group->last_segment_time = gf_sys_clock();
	group->nb_segments_since_switch ++;

	group->prev_segment_ok = GF_TRUE;
	if (group->time_at_first_failure) {
#ifndef GPAC_DISABLE_LOG
		if (gf_log_tool_level_on(GF_LOG_DASH, GF_LOG_INFO)) {
			if (group->current_base_url_idx) {
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Recovered segment %s after 404 by switching baseURL\n", url));
			} else {
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Recovered segment %s after 404 - was our download schedule %d too early ?\n", url, group->time_at_last_request - group->time_at_first_failure));
			}
		}
#endif
		group->time_at_first_failure = 0;
	}
	group->nb_consecutive_segments_lost = 0;
	group->current_base_url_idx = 0;


	rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);
	rep->playback.broadcast_flag = is_broadcast;

	/*
	we "merged" the following segments with the current one in a single open byte-range: the downloaded segment duration indicated
	is the one of the PART, NOT of the segment
	We need to compute (for ABR logic) the segment duration but we likely don't have the information since we issued this merge
	on the live edge and have not performed a forced manifest update since then, so last PARTs are not known

	We therefore assume a default duration of the average indicated, and check (in case we are lucky) if we have the full segment in the
	list (i.e. before live edge)
	if not, we SHOULD force an update just to fetch this duration before calling the ABR, but if the ABR decides to change right now
	we would fetch this manifest for nothing, and we are already fetching a LOT of manifests in HLS...
	So for the time being, we assume the target duration is a good approximation
	*/
	if (dash->llhls_single_range && group->llhls_last_was_merged) {
		u64 duration;
		assert(rep->segment_list);
		assert(rep->segment_list->timescale);
		duration = rep->segment_list->duration;
		if (group->llhls_edge_chunk) {
			s32 pos = group->download_segment_index-1;
			while (pos>=0) {
				GF_MPD_SegmentURL *surl = gf_list_get(rep->segment_list->segment_URLs, pos);
				if (surl->hls_seq_num < group->llhls_edge_chunk->hls_seq_num) break;
				if (!surl->hls_ll_chunk_type) {
					duration = surl->duration;
					break;
				}
				pos--;
			}
			group->current_downloaded_segment_duration = duration * 1000 / rep->segment_list->timescale;
		}
	}

#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_DASH, GF_LOG_INFO)) {
		u32 i, buffer_ms = 0;
		Double bitrate, time_sec;
		//force a call go query buffer
		dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_CODEC_STAT_QUERY, group->groups_idx, GF_OK);
		buffer_ms = group->buffer_occupancy_ms;
		for (i=0; i < group->nb_cached_segments; i++) {
			buffer_ms += group->cached[i].duration;
		}

		bitrate=0;
		time_sec=0;
		if (group->current_downloaded_segment_duration) {
			bitrate = 8*group->total_size;
			bitrate /= group->current_downloaded_segment_duration;
		}

		if (!us_since_start) {
			if (bytes_per_sec) {
				time_sec = group->total_size;
				time_sec /= bytes_per_sec;
			}
		} else {
			time_sec = (Double) us_since_start;
			time_sec /= 1000000;
		}

		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] AS#%d got %s stats: %d bytes in %.03g sec at %d kbps - dur %g sec - bitrate: %d (avg %d) kbps - buffer %d ms\n", 1+group->groups_idx, url, group->total_size, time_sec, 8*bytes_per_sec/1000, group->current_downloaded_segment_duration/1000.0, (u32) bitrate, rep->bandwidth/1000, buffer_ms));
	}
#endif
}

static s32 dash_do_rate_monitor_default(GF_DashClient *dash, GF_DASH_Group *group, u32 bits_per_sec, u64 total_bytes, u64 bytes_done, u64 us_since_start, u32 buffer_dur_ms, u32 current_seg_dur)
{
	Bool default_switch_mode;
	u32 time_until_end;

	//do not abort if we are downloading faster than current rate
	if (bits_per_sec > group->active_bitrate) {
		return -1;
	}

	if (group->min_bandwidth_selected) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Downloading from set #%d at rate %d kbps but media bitrate is"
				" %d kbps - no lower bitrate available ...\n", 1+group->groups_idx, bits_per_sec/1000, group->active_bitrate/1000 ));
		return -1;
	}

	//we start checking after 100ms
	if (us_since_start < 100000) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Downloading from set #%d at rate %d kbps (media bitrate %d kbps) but 100ms only ellapsed, waiting\n", 1+group->groups_idx, bits_per_sec/1000, group->active_bitrate/1000 ));
		return -1;
	}

	time_until_end = (u32) (8000*(total_bytes-bytes_done) / bits_per_sec);

	if (bits_per_sec<group->min_bitrate)
		group->min_bitrate = bits_per_sec;
	if (bits_per_sec>group->max_bitrate)
		group->max_bitrate = bits_per_sec;

	//we have enough cache data to go until end of this download, perform rate switching at next segment
	if (time_until_end < buffer_dur_ms) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Downloading from set #%ds at rate %d kbps (media bitrate %d kbps) - %d ms until end of download and %d ms in buffer, not aborting\n", 1+group->groups_idx, bits_per_sec/1000, group->active_bitrate/1000, time_until_end, buffer_dur_ms));
		return -1;
	}

	GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Downloading from set #%d at rate %d kbps but media bitrate is %d kbps - %d ms until end of download but %d ms in buffer - aborting segment download\n", 1+group->groups_idx, bits_per_sec/1000, group->active_bitrate/1000, buffer_dur_ms));

	//in live we just abort current download and go to next. In onDemand, we may want to rebuffer
	default_switch_mode = (dash->mpd->type==GF_MPD_TYPE_DYNAMIC) ? GF_FALSE : GF_TRUE;

	us_since_start /= 1000;
	//if we have time to download from another rep ?
	if (current_seg_dur <= us_since_start) {
		//don't force bandwidth switch (it's too late anyway, consider we lost the segment), let the rate adaptation decide
		group->force_switch_bandwidth = default_switch_mode;

		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Download time longer than segment duration - trying to resync on next segment\n"));
	} else {
		u32 target_rate;
		//compute min bitrate needed to fetch the segment in another rep, with the time remaining
		Double ratio = (Double) ((u32)current_seg_dur - us_since_start);
		ratio /= (u32) current_seg_dur;

		target_rate = (u32) (bits_per_sec * ratio);

		if (target_rate < group->min_representation_bitrate) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Download rate lower than min available rate ...\n"));
			target_rate = group->min_representation_bitrate;
			//don't force bandwidth switch, we won't have time to redownload the segment.
			group->force_switch_bandwidth = default_switch_mode;
		} else {
			group->force_switch_bandwidth = GF_TRUE;
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Attempting to re-download at target rate %d\n", target_rate));
		}
		//cap max bitrate for next rate adaptation pass
		group->max_bitrate = target_rate;
	}
	return -2;
}

static s32 dash_do_rate_adaptation_legacy_rate(GF_DashClient *dash, GF_DASH_Group *group, GF_DASH_Group *base_group,
												u32 dl_rate, Double speed, Double max_available_speed, Bool force_lower_complexity,
												GF_MPD_Representation *rep, Bool go_up_bitrate)
{
	u32 k;
	Bool do_switch;
	GF_MPD_Representation *new_rep;
	s32 new_index = group->active_rep_index;

	/* records the number of representations between the current one and the next chosen one */
	u32 nb_inter_rep = 0;

	/* We assume that there will be a change in quality */
	do_switch = GF_TRUE;

	/*find best bandwidth that fits our bitrate and playing speed*/
	new_rep = NULL;

	/* for each playable representation, if we still need to switch, evaluate it */
	for (k = 0; k<gf_list_count(group->adaptation_set->representations) && do_switch; k++) {
		GF_MPD_Representation *arep = gf_list_get(group->adaptation_set->representations, k);
		if (!arep->playback.prev_max_available_speed) {
			arep->playback.prev_max_available_speed = 1.0;
		}
		if (arep->playback.disabled) {
			continue;
		}
		if (arep->playback.prev_max_available_speed && (speed > arep->playback.prev_max_available_speed)) {
			continue;
		}
		/* Only try to switch to a representation, if download rate is greater than its bitrate */
		if (dl_rate >= arep->bandwidth) {

			/* First check if we are adapting to CPU */
			if (!dash->disable_speed_adaptation && force_lower_complexity) {

				/*try to switch to highest quality below the current one*/
				if ((arep->quality_ranking < rep->quality_ranking) ||
					(arep->width < rep->width) || (arep->height < rep->height)) {

					/* If we hadn't found a new representation, use it
					   otherwise use it only if current representation is better*/
					if (!new_rep) {
						new_rep = arep;
						new_index = k;
					}
					else if ((arep->quality_ranking > new_rep->quality_ranking) ||
						(arep->width > new_rep->width) || (arep->height > new_rep->height)) {
						new_rep = arep;
						new_index = k;
					}
				}
				rep->playback.prev_max_available_speed = max_available_speed;
				go_up_bitrate = GF_FALSE;
			}
			else {
				/* if speed adaptation is not used or used, but the new representation can be played at the right speed */

				if (!new_rep) {
					new_rep = arep;
					new_index = k;
				}
				else if (go_up_bitrate) {

					/* agressive switching is configured in the GPAC configuration */
					if (dash->agressive_switching) {
						/*be agressive, try to switch to highest bitrate below available download rate*/
						if (arep->bandwidth > new_rep->bandwidth) {
							if (new_rep->bandwidth > rep->bandwidth) {
								nb_inter_rep++;
							}
							new_rep = arep;
							new_index = k;
						}
						else if (arep->bandwidth > rep->bandwidth) {
							nb_inter_rep++;
						}
					}
					else {
						/*don't be agressive, try to switch to lowest bitrate above our current rep*/
						if (new_rep->bandwidth <= rep->bandwidth) {
							new_rep = arep;
							new_index = k;
						}
						else if ((arep->bandwidth < new_rep->bandwidth) && (arep->bandwidth > rep->bandwidth)) {
							new_rep = arep;
							new_index = k;
						}
					}
				}
				else {
					/* go_up_bitrate is GF_FALSE */
					/*try to switch to highest bitrate below available download rate*/
					if (arep->bandwidth > new_rep->bandwidth) {
						new_rep = arep;
						new_index = k;
					}
				}
			}
		}
	}

	if (!new_rep || (new_rep == rep)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] AS#%d no better match for requested bandwidth %d - not switching (AS bitrate %d)!\n", 1 + group->groups_idx, dl_rate, rep->bandwidth));
		do_switch = GF_FALSE;
	}

	if (do_switch) {
		//if we're switching to the next upper bitrate (no intermediate bitrates), do not immediately switch
		//but for a given number of segments - this avoids fluctuation in the quality
		if (go_up_bitrate && !nb_inter_rep) {
			new_rep->playback.probe_switch_count++;
			if (new_rep->playback.probe_switch_count > dash->probe_times_before_switch) {
				new_rep->playback.probe_switch_count = 0;
			} else {
				new_index = group->active_rep_index;
			}
		}
	}
	return new_index;
}

static s32 dash_do_rate_adaptation_legacy_buffer(GF_DashClient *dash, GF_DASH_Group *group, GF_DASH_Group *base_group,
												  u32 dl_rate, Double speed, Double max_available_speed, Bool force_lower_complexity,
												  GF_MPD_Representation *rep, Bool go_up_bitrate)
{
	Bool do_switch;
	s32 new_index = group->active_rep_index;

	/* We assume that there will be a change in quality
	   and then set it to no change or increase if this is not the case */
	do_switch = GF_TRUE;

	/* if the current representation bitrate is smaller than the measured bandwidth,
	   tentatively start to increase bitrate */
	if (rep->bandwidth < dl_rate) {
		go_up_bitrate = GF_TRUE;
	}

	/* clamp download bitrate to the lowest representation rate, to allow choosing it */
	if (dl_rate < group->min_representation_bitrate) {
		dl_rate = group->min_representation_bitrate;
	}

	//we have buffered output
	if (group->buffer_max_ms) {
		u32 buf_high_threshold, buf_low_threshold;
		s32 occ;

		if (group->current_downloaded_segment_duration && (group->buffer_max_ms > group->current_downloaded_segment_duration)) {
			buf_high_threshold = group->buffer_max_ms - (u32)group->current_downloaded_segment_duration;
		}
		else {
			buf_high_threshold = 2 * group->buffer_max_ms / 3;
		}
		buf_low_threshold = (group->current_downloaded_segment_duration && (group->buffer_min_ms>10)) ? group->buffer_min_ms : (u32)group->current_downloaded_segment_duration;
		if (buf_low_threshold > group->buffer_max_ms) buf_low_threshold = 1 * group->buffer_max_ms / 3;

		//compute how much we managed to refill (current state minus previous state)
		occ = (s32)group->buffer_occupancy_ms;
		occ -= (s32)group->buffer_occupancy_at_last_seg;
		//if above max buffer force occ>0 since a segment may still be pending and not dispatched (buffer regulation)
		if (group->buffer_occupancy_ms>group->buffer_max_ms) occ = 1;

		//switch down if current buffer falls below min threshold
		if ((s32)group->buffer_occupancy_ms < (s32)buf_low_threshold) {
			if (!group->buffer_occupancy_ms) {
				dl_rate = group->min_representation_bitrate;
			}
			else {
				dl_rate = (rep->bandwidth > 10) ? rep->bandwidth - 10 : 1;
			}
			go_up_bitrate = GF_FALSE;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] AS#%d bitrate %d bps buffer max %d current %d refill since last %d - running low, switching down, target rate %d\n", 1 + group->groups_idx, rep->bandwidth, group->buffer_max_ms, group->buffer_occupancy_ms, occ, dl_rate));
		}
		//switch up if above max threshold and buffer refill is fast enough
		else if ((occ>0) && (group->buffer_occupancy_ms > buf_high_threshold)) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] AS#%d bitrate %d bps buffer max %d current %d refill since last %d - running high, will try to switch up, target rate %d\n", 1 + group->groups_idx, rep->bandwidth, group->buffer_max_ms, group->buffer_occupancy_ms, occ, dl_rate));
			go_up_bitrate = GF_TRUE;
		}
		//don't do anything in the middle range of the buffer or if refill not fast enough
		else {
			do_switch = GF_FALSE;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] AS#%d bitrate %d bps buffer max %d current %d refill since last %d - steady\n", 1 + group->groups_idx, rep->bandwidth, group->buffer_max_ms, group->buffer_occupancy_ms, occ));
		}
	}

	/* Unless the switching has been turned off (e.g. middle buffer range),
	   we apply rate-based adaptation */
	if (do_switch) {
		new_index = dash_do_rate_adaptation_legacy_rate(dash, group, base_group, dl_rate, speed, max_available_speed, force_lower_complexity, rep, go_up_bitrate);
	}

	return new_index;
}

// returns the bitrate and index of the representation having the minimum bitrate above the given rate
static u32 get_min_rate_above(GF_List *representations, double rate, s32 *index) {
	u32 k;
	u32 min_rate = GF_INT_MAX;
	GF_MPD_Representation *rep;

	u32 nb_reps = gf_list_count(representations);
	for (k = 0; k < nb_reps; k++) {
		rep = gf_list_get(representations, k);
		if ((rep->bandwidth < min_rate) && (rep->bandwidth > rate)) {
			min_rate = rep->bandwidth;
			if (index) {
				*index = k;
			}
			return min_rate; // representations are sorted by bandwidth
		}
	}
	return min_rate;
}

// returns the bitrate and index of the representation having the maximum bitrate below the given rate
static u32 get_max_rate_below(GF_List *representations, double rate, s32 *index) {
	s32 k;
	u32 max_rate = 0;
	GF_MPD_Representation *rep;

	u32 nb_reps = gf_list_count(representations);
	for (k = (s32) nb_reps-1; k >=0 ; k--) {
		rep = gf_list_get(representations, k);
		if ((rep->bandwidth > max_rate) && (rep->bandwidth < rate)) {
			max_rate = rep->bandwidth;
			if (index) {
				*index = k;
			}
			return max_rate; // representations are sorted by bandwidth
		}
	}
	return max_rate;
}

/**
Adaptation Algorithm as described in
	T.-Y. Huang et al. 2014. A buffer-based approach to rate adaptation: evidence from a large video streaming service.
	In Proceedings of the 2014 ACM conference on SIGCOMM (SIGCOMM '14).
*/
static s32 dash_do_rate_adaptation_bba0(GF_DashClient *dash, GF_DASH_Group *group, GF_DASH_Group *base_group,
												  u32 dl_rate, Double speed, Double max_available_speed, Bool force_lower_complexity,
												  GF_MPD_Representation *rep, Bool go_up_bitrate)
{
	u32 rate_plus;
	u32 rate_minus;
	u32 rate_prev = group->active_bitrate;
	u32 rate_max;
	u32 rate_min;
	s32 new_index;
	u32 r; // reservoir
	u32 cu; // cushion
	u32 buf_now = group->buffer_occupancy_ms;
	u32 buf_max = group->buffer_max_ms;
	double f_buf_now;

	/* We don't use the segment duration as advertised in the MPD because it may not be there due to segment timeline*/
	u32 segment_duration_ms = (u32)group->current_downloaded_segment_duration;

	rate_min = ((GF_MPD_Representation *)gf_list_get(group->adaptation_set->representations, 0))->bandwidth;
	rate_max = ((GF_MPD_Representation *)gf_list_get(group->adaptation_set->representations, gf_list_count(group->adaptation_set->representations) - 1))->bandwidth;

	if (!buf_max) buf_max = 3*segment_duration_ms;

	/* buffer level higher than max buffer, keep high quality*/
	if (group->buffer_occupancy_ms > buf_max) {
		return gf_list_count(group->adaptation_set->representations) - 1;
	}
	/* we cannot run bba if segments are longer than the max buffer*/
	if (buf_max < segment_duration_ms) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] BBA-0: max buffer %d shorter than segment duration %d, cannot adapt - will use current quality\n", buf_max, group->buffer_occupancy_ms));
		return group->active_rep_index;
	}
	/* if the current buffer cannot hold an entire new segment, we indicate that we don't want to download it now
	   NOTE: This is not described in the paper
	*/
	if (group->buffer_occupancy_ms + segment_duration_ms > buf_max) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] BBA-0: not enough space to download new segment: %d\n", group->buffer_occupancy_ms));
		return -1;
	}

    if (rate_prev == rate_max) {
    	rate_plus = rate_max;
    } else {
		rate_plus = get_min_rate_above(group->adaptation_set->representations, rate_prev, NULL);
    }

    if (rate_prev == rate_min) {
    	rate_minus = rate_min;
    } else {
		rate_minus = get_max_rate_below(group->adaptation_set->representations, rate_prev, NULL);
    }

    /*
     * the size of the reservoir is 37.5% of the buffer size, but at least = 1 chunk duration)
	 * the size of the upper reservoir is 10% of the buffer size
     * the size of cushion is between 37.5% and 90% of the buffer size
	 * the rate map is piece-wise
     */
	if (buf_max <= segment_duration_ms) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] BBA-0: cannot initialize BBA-0 given the buffer size (%d) and segment duration (%d)\n", group->buffer_max_ms, group->segment_duration*1000));
		return -1;
	}
	r = (u32)(37.5*buf_max / 100);
	if (r < segment_duration_ms) {
		r = segment_duration_ms;
	}
	cu = (u32)((90-37.5)*buf_max / 100);

	if (buf_now <= r) {
		f_buf_now = rate_min;
	}
	else if (buf_now >= (cu + r)) {
		f_buf_now = rate_max;
	}
	else {
		f_buf_now = rate_min + (rate_max - rate_min)*((buf_now - r) * 1.0 / cu);
	}

	if (f_buf_now == rate_max) {
		// rate_next = rate_max;
		new_index = gf_list_count(group->adaptation_set->representations) - 1;
	}
	else if (f_buf_now == rate_min) {
		// rate_next = rate_min;
		new_index = 0;
	}
	else if (f_buf_now >= rate_plus) {
		// rate_next = max of Ri st. Ri < f_buf_now
		new_index = 0;
		get_max_rate_below(group->adaptation_set->representations, f_buf_now, &new_index);
	}
	else if (f_buf_now <= rate_minus) {
		// rate_next = min of Ri st. Ri > f_buf_now
		new_index = gf_list_count(group->adaptation_set->representations) - 1;
		get_min_rate_above(group->adaptation_set->representations, f_buf_now, &new_index);
	}
	else {
		// no change
		new_index = group->active_rep_index;
	}

	if (new_index != -1) {
#ifndef GPAC_DISABLE_LOG
		GF_MPD_Representation *result = gf_list_get(group->adaptation_set->representations, (u32)new_index);
#endif
		// increment the segment number for debug purposes
		group->current_index++;
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] BBA-0: buffer %d ms, segment number %d, new quality %d with rate %d\n", group->buffer_occupancy_ms, group->current_index, new_index, result->bandwidth));
	}
	return new_index;
}

/* returns the index of the representation which maximises BOLA utility function
   based on the relative log-based utility of each representation compared to the one with the lowest bitrate
   NOTE: V can represent V (in BOLA BASIC) or V_D (in other modes of BOLA) */
static s32 bola_find_max_utility_index(GF_List *representations, Double V, Double gamma, Double p, Double Q) {
	u32 k;
	Double max_utility = GF_MIN_DOUBLE;
	u32 nb_reps = gf_list_count(representations);
	s32 new_index = -1;

	for (k = 0; k < nb_reps; k++) {
		GF_MPD_Representation *rep = gf_list_get(representations, k);
		Double utility = (V * rep->playback.bola_v + V*gamma*p - Q) / (rep->bandwidth*p);
		if (utility >= max_utility) {
			max_utility = utility;
			new_index = k;
		}
	}
	return new_index;
}

/**
Adaptation Algorithm as described in
K. Spiteri et al. 2016. BOLA: Near-Optimal Bitrate Adaptation for Online Videos
Arxiv.org
*/
static s32 dash_do_rate_adaptation_bola(GF_DashClient *dash, GF_DASH_Group *group, GF_DASH_Group *base_group,
		  	  	  	  	  	  	  	  	  u32 dl_rate, Double speed, Double max_available_speed, Bool force_lower_complexity,
										  	  GF_MPD_Representation *rep, Bool go_up_bitrate)
{
	s32 new_index = -1;
	u32 k;
	Double p = group->current_downloaded_segment_duration / 1000.0;	// segment duration
	Double gamma = (double)5/(double)p;
	Double Qmax = group->buffer_max_ms / 1000.0 / p;		// max nb of segments in the buffer
	Double Q = group->buffer_occupancy_ms / 1000.0 / p;		// current buffer occupancy in number of segments

	GF_MPD_Representation *min_rep;
	GF_MPD_Representation *max_rep;
	u32 nb_reps;

	if (dash->mpd->type != GF_MPD_TYPE_STATIC) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] BOLA: Cannot be used for live MPD\n"));
		return -1;
	}

	nb_reps = gf_list_count(group->adaptation_set->representations);
	min_rep = gf_list_get(group->adaptation_set->representations, 0);
	max_rep = gf_list_get(group->adaptation_set->representations, nb_reps - 1);

	// Computing the log-based utility of each segment (recomputing each time for period changes)
	for (k = 0; k < nb_reps; k++) {
		GF_MPD_Representation *a_rep = gf_list_get(group->adaptation_set->representations, k);
		a_rep->playback.bola_v = log(((Double)a_rep->bandwidth) / min_rep->bandwidth);
	}

	if (dash->adaptation_algorithm == GF_DASH_ALGO_BOLA_BASIC) {
		/* BOLA Basic is the variant of BOLA that assumes infinite duration streams (no wind-up/down, no rate use, no oscillation control)
		   It simply consists in finding the maximum utility */
		// NOTE in BOLA, representation indices decrease when the quality increases [1 = best quality]
		Double V = (Qmax - 1) / (gamma * p + max_rep->playback.bola_v);
		new_index = bola_find_max_utility_index(group->adaptation_set->representations, V, gamma, p, Q);
	}
	else if (dash->adaptation_algorithm == GF_DASH_ALGO_BOLA_FINITE ||
		dash->adaptation_algorithm == GF_DASH_ALGO_BOLA_O ||
		dash->adaptation_algorithm == GF_DASH_ALGO_BOLA_U) {
		/* BOLA FINITE is the same as BOLA Basic with the wind-up and down phases */
		/* BOLA O and U add extra steps to BOLA FINITE */
		Double t_bgn; //play time from begin
		Double t_end; //play time to the end
		Double t;
		Double t_prime;
		Double Q_Dmax;
		Double V_D;
		Double N = dash->mpd->media_presentation_duration / p;

		t_bgn = p*group->current_index;
		t_end = (N - group->current_index)*p;
		t = MIN(t_bgn, t_end);
		t_prime = MAX(t / 2, 3 * p);
		Q_Dmax = MIN(Qmax, t_prime / p);
		V_D = (Q_Dmax - 1) / (gamma * p + max_rep->playback.bola_v);

		new_index = bola_find_max_utility_index(group->adaptation_set->representations, V_D, gamma, p, Q);

		if (dash->adaptation_algorithm == GF_DASH_ALGO_BOLA_U || dash->adaptation_algorithm == GF_DASH_ALGO_BOLA_O) {
			//Bola U algorithm
			if ((new_index != -1) && ((u32)new_index > group->active_rep_index)) {
				u32 r = group->bytes_per_sec*8;

				// index_prime the min m such that (Sm[m]/p)<= max(bandwidth_previous,S_M/p))
				// NOTE in BOLA, representation indices decrease when the quality increases [1 = best quality]
				u32 m_prime = 0;
				get_max_rate_below(group->adaptation_set->representations, MAX(r, min_rep->bandwidth), &m_prime);
				if (m_prime >= (u32)new_index) {
					m_prime = new_index;
				}
				else if (m_prime < group->active_rep_index) {
					m_prime = group->active_rep_index;
				}
				else {
					if (dash->adaptation_algorithm == GF_DASH_ALGO_BOLA_U) {
						m_prime++;
					}
					else { //GF_DASH_ALGO_BOLA_O
#if 0
						GF_MPD_Representation *rep_m_prime, *rep_m_prime_plus_one;
						Double Sm_prime, Sm_prime_plus_one, f_m_prime, f_m_prime_1, bola_o_pause;

						assert(m_prime >= 0 && m_prime < nb_reps - 2);
						rep_m_prime = (GF_MPD_Representation *)gf_list_get(group->adaptation_set->representations, m_prime);
						rep_m_prime_plus_one = (GF_MPD_Representation *)gf_list_get(group->adaptation_set->representations, m_prime+1);
						Sm_prime = rep_m_prime->bandwidth*p;
						Sm_prime_plus_one = rep_m_prime_plus_one->bandwidth*p;
						f_m_prime = V_D*(rep_m_prime->playback.bola_v + gamma*p) / Sm_prime;
						f_m_prime_1 = V_D*(rep_m_prime_plus_one->playback.bola_v + gamma*p) / Sm_prime_plus_one;
						// TODO wait for bola_o_pause before making the download
						bola_o_pause = Q - (f_m_prime - f_m_prime_1) / (1 / Sm_prime - 1 / Sm_prime_plus_one);
#endif
					}
				}
				new_index = m_prime;
			}
		}
		// TODO trigger pause for max(p*(Q-Q_Dmax+1), 0)
	}

	if (new_index != -1) {
#ifndef GPAC_DISABLE_LOG
		GF_MPD_Representation *result = gf_list_get(group->adaptation_set->representations, (u32)new_index);
#endif
		// increment the segment number for debug purposes
		group->current_index++;
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] BOLA: buffer %d ms, segment number %d, new quality %d with rate %d\n", group->buffer_occupancy_ms, group->current_index, new_index, result->bandwidth));
	}
	return new_index;
}

/* This function is called each time a new segment has been downloaded */
static void dash_do_rate_adaptation(GF_DashClient *dash, GF_DASH_Group *group)
{
	Double speed;
	Double max_available_speed;
	u32 dl_rate;
	u32 k;
	s32 new_index, old_index;
	GF_DASH_Group *base_group;
	GF_MPD_Representation *rep;
	Bool force_lower_complexity;

	/* Don't do adaptation if configured switching to happen systematically (debug) */
	if (dash->auto_switch_count) {
		return;
	}
	/* Don't do adaptation if GPAC config disabled switching */
	if (group->dash->disable_switching) {
		return;
	}

	/* The bytes_per_sec field is set each time a segment is downloaded,
	   (this may need to be adjusted in the future to accomodate algorithms
	   that smooth download rate over several segments)
	   if set to 0, this means that no segment was downloaded since the last call
	   because this AdaptationSet is not selected
	   So no rate adaptation should be done*/
	if (!group->bytes_per_sec) {
		return;
	}

	/* Find the AdaptationSet on which this AdaptationSet depends, if any
	   (e.g. used for specific coding schemes: scalable streams, tiled streams, ...)*/
	base_group = group;
	while (base_group->depend_on_group) {
		base_group = base_group->depend_on_group;
	}

	/* adjust the download rate according to the playback speed
	   All adaptation algorithms should use this value */
	speed = dash->speed;
	if (speed<0) speed = -speed;
	dl_rate = (u32) (8 * (u64) group->bytes_per_sec / speed);
	if ((s32) dl_rate < 0)
		dl_rate = GF_INT_MAX;

	/* Get the active representation in the AdaptationSet */
	rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);

	/*check whether we can play with this speed (i.e. achieve target frame rate);
	if not force, let algorithm know that they should switch to a lower resolution*/
	max_available_speed = gf_dash_get_max_available_speed(dash, base_group, rep);
	if (!dash->disable_speed_adaptation && !rep->playback.waiting_codec_reset) {
		if (max_available_speed && (0.9 * speed > max_available_speed)) {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Forcing a lower complexity to achieve desired playback speed\n"));
			force_lower_complexity = GF_TRUE;
		} else {
			force_lower_complexity = GF_FALSE;
		}
	} else {
		force_lower_complexity = GF_FALSE;
	}

	/*query codec and buffer statistics for buffer-based algorithms */
	group->buffer_max_ms = 0;
	group->buffer_occupancy_ms = 0;
	group->codec_reset = 0;
	/* the DASH Client asks the player for its buffer level
	  (uses a function pointer to avoid depenencies on the player code, to reuse the DASH client in different situations)*/
	dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_CODEC_STAT_QUERY, group->groups_idx, GF_OK);

	/* If the playback for the current representation was waiting for a codec reset and it happened,
	   indicate that this representation does not need a reset anymore */
	if (rep->playback.waiting_codec_reset && group->codec_reset) {
		rep->playback.waiting_codec_reset = GF_FALSE;
	}

	old_index = group->active_rep_index;
	//scalable case, force the rate algo to consider the active rep is the max rep
	if (group->base_rep_index_plus_one) {
		group->active_rep_index = group->max_complementary_rep_index;
	}
	if (group->dash->route_clock_state) {
		rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);
		if (rep->playback.broadcast_flag && (dl_rate < rep->bandwidth)) {
			dl_rate = rep->bandwidth+1;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] AS#%d representation %d segment sent over broadcast, forcing bandwidth to %d\n", 1 + group->groups_idx, group->active_rep_index, dl_rate));
		}
	}

	/* Call a specific adaptation algorithm (see GPAC configuration)
	Each algorithm should:
	- return the new_index value to the desired quality

	It can use:
	- the information about each available representation (group->adaptation_set->representations, e.g. bandwidth required for that representation)
	- the information of the current representation (rep)
	- the download_rate dl_rate (computed on the previously downloaded segment, and adjusted to the playback speed),
	- the buffer levels:
	    - current: group->buffer_occupancy_ms,
		- previous: group->buffer_occupancy_at_last_seg
		- max: group->buffer_max_ms,
	- the playback speed,
	- the maximum achievable speed at the current resolution,
	- the indicator that the current representation is too demanding CPU-wise (force_lower_complexity)

	Private algorithm information should be stored in the dash object if global to all AdaptationSets,
	or in the group if local to an AdaptationSet.

	TODO: document how to access other possible parameters (e.g. segment sizes if available, ...)
	*/
	new_index = group->active_rep_index;
	if (dash->rate_adaptation_algo) {
		new_index = dash->rate_adaptation_algo(dash, group, base_group,
														  dl_rate, speed, max_available_speed, force_lower_complexity,
														  rep, GF_FALSE);
	}

	if (new_index==-1) {
		group->active_rep_index = old_index;
		group->rate_adaptation_postponed = GF_TRUE;
		return;
	}
	group->rate_adaptation_postponed = GF_FALSE;
	if (new_index < 0) {
		if (new_index == -2) {
			group->disabled = GF_TRUE;
		}
		group->active_rep_index = old_index;
		return;
	}
	if (new_index != group->active_rep_index) {
		GF_MPD_Representation *new_rep = gf_list_get(group->adaptation_set->representations, (u32)new_index);
		group->disabled = GF_FALSE;
		if (!new_rep) {
			group->active_rep_index = old_index;
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Cannot find new representation index %d, using previous one\n", new_index));
			return;
		}

		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] AS#%d switching after playing %d segments, from rep %d to rep %d\n", 1 + group->groups_idx,
				group->nb_segments_since_switch, group->active_rep_index, new_index));
		group->nb_segments_since_switch = 0;

		if (force_lower_complexity) {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Requesting codec reset\n"));
			new_rep->playback.waiting_codec_reset = GF_TRUE;
		}
		/* request downloads for the new representation */
		gf_dash_set_group_representation(group, new_rep, GF_FALSE);

		/* Reset smoothing of switches
		(note: should really apply only to algorithms using the switch_probe_count (smoothing the aggressiveness of the change)
		for now: only GF_DASH_ALGO_GPAC_LEGACY_RATE and  GF_DASH_ALGO_GPAC_LEGACY_BUFFER */
		for (k = 0; k < gf_list_count(group->adaptation_set->representations); k++) {
			GF_MPD_Representation *arep = gf_list_get(group->adaptation_set->representations, k);
			if (new_rep == arep) continue;
			arep->playback.probe_switch_count = 0;
		}

	} else {
		group->active_rep_index = old_index;
		if (force_lower_complexity) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Forced to lower quality/rate because of playback speed %f higher than max speed possible %f, but no other quality available: cannot switch down\n", speed, max_available_speed));
			/*FIXME: should do something here*/
		}
	}

	/* Remembering the buffer level for the processing of the next segment */
	group->buffer_occupancy_at_last_seg = group->buffer_occupancy_ms;
}

static char *gf_dash_get_fileio_url(const char *base_url, char *res_url)
{
	const char *new_res;
	GF_FileIO *gfio;
	if (!base_url)
		return NULL;
	if (strncmp(base_url, "gfio://", 7))
		return res_url;

	gfio = gf_fileio_from_url(base_url);

	new_res = gf_fileio_factory(gfio, res_url);
	if (!new_res) return res_url;
	gf_free(res_url);
	return gf_strdup(new_res);
}

static GF_Err gf_dash_download_init_segment(GF_DashClient *dash, GF_DASH_Group *group)
{
	GF_Err e;
	char *base_init_url;
	GF_MPD_Representation *rep;
	u64 start_range, end_range;
	Bool data_url_processed = GF_FALSE;
	/* This variable is 0 if there is a initURL, the index of first segment downloaded otherwise */
	u32 nb_segment_read = 0;
	char *base_url=NULL;
	char *base_url_orig=NULL;
	char *key_url=NULL;
	bin128 key_iv;
	u32 start_number = 0;

	if (!dash || !group)
		return GF_BAD_PARAM;

	assert(group->adaptation_set && group->adaptation_set->representations);
	rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);
	if (!rep) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Unable to find any representation, aborting.\n"));
		return GF_IO_ERR;
	}
	start_range = end_range = 0;
	base_url = dash->base_url;
	if (group->period->origin_base_url) base_url = group->period->origin_base_url;

	base_url_orig = base_url;
	if (base_url && !strncmp(base_url, "gfio://", 7)) {
		GF_FileIO *gfio = gf_fileio_from_url(base_url);
		base_url = (char *) gf_file_basename(gf_fileio_resource_url(gfio));
	}

	e = gf_dash_resolve_url(dash->mpd, rep, group, base_url, GF_MPD_RESOLVE_URL_INIT, 0, &base_init_url, &start_range, &end_range, &group->current_downloaded_segment_duration, NULL, &key_url, &key_iv, &data_url_processed, NULL);
	if (e) {
		if (e != GF_IP_NETWORK_EMPTY) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Unable to resolve initialization URL: %s\n", gf_error_to_string(e) ));
		}
		return e;
	}

	if (!base_init_url && rep->dependency_id) {
		return GF_OK;
	}

	/*no error and no init segment, go for media segment - this is needed for TS so that the set of media streams can be
	declared to the player */
	if (!base_init_url) {
		e = gf_dash_resolve_url(dash->mpd, rep, group, dash->base_url, GF_MPD_RESOLVE_URL_MEDIA, group->download_segment_index, &base_init_url, &start_range, &end_range, &group->current_downloaded_segment_duration, NULL, &key_url, &key_iv, NULL, &start_number);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Unable to resolve media URL: %s\n", gf_error_to_string(e) ));
			return e;
		}
		nb_segment_read = 1;
	} else if (dash->is_m3u8) {
		char *tmp_url=NULL;
		u64 dur, sr, er;
		u32 startnum;
		e = gf_dash_resolve_url(dash->mpd, rep, group, dash->base_url, GF_MPD_RESOLVE_URL_MEDIA, group->download_segment_index, &tmp_url, &sr, &er, &dur, NULL, &key_url, &key_iv, NULL, &startnum);
		if (tmp_url) gf_free(tmp_url);
	}

	base_url = base_url_orig;
	base_init_url = gf_dash_get_fileio_url(base_url, base_init_url);

	if (nb_segment_read) group->init_segment_is_media = GF_TRUE;

	if (!strstr(base_init_url, "://") || !strnicmp(base_init_url, "file://", 7) || !strnicmp(base_init_url, "gmem://", 7)
		|| !strnicmp(base_init_url, "views://", 8) || !strnicmp(base_init_url, "mosaic://", 9)
		|| !strnicmp(base_init_url, "isobmff://", 10) || !strnicmp(base_init_url, "gfio://", 7)
	) {
		//if file-based, check if file exists, if not switch base URL
		if ( strnicmp(base_init_url, "gmem://", 7) && strnicmp(base_init_url, "gfio://", 7)) {
			if (! gf_file_exists(base_init_url) ) {
				if (group->current_base_url_idx + 1 < gf_mpd_get_base_url_count(dash->mpd, group->period, group->adaptation_set, rep) ){
					group->current_base_url_idx++;
					gf_free(base_init_url);
					return gf_dash_download_init_segment(dash, group);
				}
			}
		}
		//we don't reset the baseURL index until we are done fetching all init segments

		assert(!group->nb_cached_segments);
		//transfer mem
		group->cached[0].url = base_init_url;
		group->cached[0].representation_index = group->active_rep_index;
		group->prev_active_rep_index = group->active_rep_index;
		if (key_url) {
			group->cached[0].key_url = key_url;
			memcpy(group->cached[0].key_IV, key_iv, sizeof(bin128));
		}
		group->cached[0].seg_number = start_number + group->download_segment_index;

		group->nb_cached_segments = 1;
		/*do not erase local files*/
		group->local_files = group->was_segment_base ? 0 : 1;

		group->download_segment_index += nb_segment_read;
		if (group->bitstream_switching) {
			group->bs_switching_init_segment_url = gf_strdup(base_init_url);
			group->bs_switching_init_segment_url_start_range = start_range;
			group->bs_switching_init_segment_url_end_range = end_range;
			group->bs_switching_init_segment_url_name_start = dash_strip_base_url(group->bs_switching_init_segment_url, base_url);
			if (data_url_processed) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("URL with data scheme not handled for Bistream Switching Segments, probable memory leak"));
			}
		} else {
			if (rep->playback.cached_init_segment_url) gf_free(rep->playback.cached_init_segment_url);
			rep->playback.cached_init_segment_url = gf_strdup(base_init_url);
			rep->playback.owned_gmem = data_url_processed;
			rep->playback.init_start_range = start_range;
			rep->playback.init_end_range = end_range;
			rep->playback.init_seg_name_start = dash_strip_base_url(rep->playback.cached_init_segment_url, base_url);
			if (key_url) {
				rep->playback.key_url = gf_strdup(key_url);
				memcpy(rep->playback.key_IV, key_iv, sizeof(bin128) );
			}
		}


		/*finally download all init segments if any*/
		if (!group->bitstream_switching) {
			u32 k;
			for (k=0; k<gf_list_count(group->adaptation_set->representations); k++) {
				char *a_base_init_url = NULL;
				u64 a_start = 0, a_end = 0, a_dur = 0;
				GF_MPD_Representation *a_rep = gf_list_get(group->adaptation_set->representations, k);
				if (a_rep==rep) continue;
				if (a_rep->playback.disabled) continue;

				e = gf_dash_resolve_url(dash->mpd, a_rep, group, dash->base_url, GF_MPD_RESOLVE_URL_INIT, 0, &a_base_init_url, &a_start, &a_end, &a_dur, NULL, &a_rep->playback.key_url, &a_rep->playback.key_IV, &a_rep->playback.owned_gmem, NULL);
				if (!e && a_base_init_url) {
					if (a_rep->playback.cached_init_segment_url) gf_free(a_rep->playback.cached_init_segment_url);
					a_rep->playback.cached_init_segment_url = a_base_init_url;
					a_rep->playback.init_start_range = a_start;
					a_rep->playback.init_end_range = a_end;
					a_rep->playback.init_seg_name_start = dash_strip_base_url(a_base_init_url, base_url);
				} else if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot solve initialization segment for representation: %s - discarding representation\n", gf_error_to_string(e) ));
					a_rep->playback.disabled = 1;
				}
			}
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] First segment is %s \n", base_init_url));
		//do NOT free base_init_url, it is now in group->cached[0].url
		group->current_base_url_idx=0;
		return GF_OK;
	}

	group->max_bitrate = 0;
	group->min_bitrate = (u32)-1;


	if (dash->route_clock_state && !group->period->origin_base_url) {
		GF_DASHFileIOSession sess = NULL;
		/*check the init segment has been received*/
		e = gf_dash_download_resource(dash, &sess, base_init_url, start_range, end_range, 1, NULL);
		dash->dash_io->del(dash->dash_io, sess);

		if (e!=GF_OK) {
			gf_free(base_init_url);
			return e;
		}
	}

	assert(!group->nb_cached_segments);
	group->cached[0].url = base_init_url;
	group->cached[0].representation_index = group->active_rep_index;
	group->cached[0].duration = (u32) group->current_downloaded_segment_duration;

	if (group->bitstream_switching) {
		group->bs_switching_init_segment_url = gf_strdup(base_init_url);
		group->bs_switching_init_segment_url_name_start = dash_strip_base_url(group->bs_switching_init_segment_url, base_url);

		group->bs_switching_init_segment_url_start_range = start_range;
		group->bs_switching_init_segment_url_end_range = end_range;
		if (data_url_processed) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("URL with data scheme not handled for Bistream Switching Segments, probable memory leak"));
		}
	} else {
		if (rep->playback.cached_init_segment_url) gf_free(rep->playback.cached_init_segment_url);
		rep->playback.cached_init_segment_url = gf_strdup(base_init_url);
		rep->playback.init_start_range = start_range;
		rep->playback.init_end_range = end_range;
		rep->playback.owned_gmem = data_url_processed;
		rep->playback.init_seg_name_start = dash_strip_base_url(rep->playback.cached_init_segment_url, base_url);
		if (key_url) {
			rep->playback.key_url = key_url;
			memcpy(rep->playback.key_IV, key_iv, sizeof(bin128) );
			key_url = NULL;
		}
	}
	if (key_url) gf_free(key_url);
	
	group->nb_cached_segments = 1;
	group->download_segment_index += nb_segment_read;

	/*download all init segments if any*/
	if (!group->bitstream_switching) {
		u32 k;
		for (k=0; k<gf_list_count(group->adaptation_set->representations); k++) {
			char *a_base_init_url = NULL;
			u64 a_start, a_end, a_dur;
			GF_MPD_Representation *a_rep = gf_list_get(group->adaptation_set->representations, k);
			if (a_rep==rep) continue;
			if (a_rep->playback.disabled) continue;

			e = gf_dash_resolve_url(dash->mpd, a_rep, group, dash->base_url, GF_MPD_RESOLVE_URL_INIT, 0, &a_base_init_url, &a_start, &a_end, &a_dur, NULL, &a_rep->playback.key_url, &a_rep->playback.key_IV, &a_rep->playback.owned_gmem, NULL);
			if (!e && a_base_init_url) {
				if (a_rep->playback.cached_init_segment_url) gf_free(a_rep->playback.cached_init_segment_url);
				a_rep->playback.cached_init_segment_url = a_base_init_url;
				a_rep->playback.init_start_range = a_start;
				a_rep->playback.init_end_range = a_end;
				a_rep->playback.init_seg_name_start = dash_strip_base_url(a_rep->playback.cached_init_segment_url, base_url);
			} else if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot solve initialization segment for representation: %s - disabling representation\n", gf_error_to_string(e) ));
				a_rep->playback.disabled = 1;
			}
		}
	}
	return GF_OK;
}

static void gf_dash_skip_disabled_representation(GF_DASH_Group *group, GF_MPD_Representation *rep, Bool for_autoswitch)
{
	s32 rep_idx, orig_idx;
	u32 bandwidth = 0xFFFFFFFF;

	rep_idx = orig_idx = gf_list_find(group->adaptation_set->representations, rep);
	while (1) {
		rep_idx++;
		if (rep_idx==gf_list_count(group->adaptation_set->representations)) rep_idx = 0;
		//none other than current one
		if (orig_idx==rep_idx) return;

		rep = gf_list_get(group->adaptation_set->representations, rep_idx);
		if (rep->playback.disabled) continue;

		if (rep->bandwidth<=bandwidth) break;
		assert(for_autoswitch);
		//go to next rep
	}
	assert(rep && !rep->playback.disabled);
	gf_dash_set_group_representation(group, rep, GF_FALSE);
}


static void gf_dash_group_reset_cache_entry(segment_cache_entry *cached)
{
	if (cached->url) gf_free(cached->url);
	if (cached->key_url) gf_free(cached->key_url);
	memset(cached, 0, sizeof(segment_cache_entry));
}

static void gf_dash_group_reset(GF_DashClient *dash, GF_DASH_Group *group)
{
	while (group->nb_cached_segments) {
		group->nb_cached_segments--;
		gf_dash_group_reset_cache_entry(&group->cached[group->nb_cached_segments]);
	}
	group->llhls_edge_chunk = NULL;

	group->timeline_setup = GF_FALSE;
}

static void gf_dash_reset_groups(GF_DashClient *dash)
{
	/*send playback destroy event*/
	if (dash->dash_io) dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_DESTROY_PLAYBACK, -1, GF_OK);

	while (gf_list_count(dash->groups)) {
		GF_DASH_Group *group = gf_list_last(dash->groups);
		gf_list_rem_last(dash->groups);

		gf_dash_group_reset(dash, group);

		gf_list_del(group->groups_depending_on);
		gf_free(group->cached);
		if (group->service_mime)
			gf_free(group->service_mime);

		if (group->bs_switching_init_segment_url)
			gf_free(group->bs_switching_init_segment_url);

		gf_free(group);
	}
	gf_list_del(dash->groups);
	dash->groups = NULL;

	while (gf_list_count(dash->SRDs)) {
		struct _dash_srd_desc *srd = gf_list_last(dash->SRDs);
		gf_list_rem_last(dash->SRDs);
		gf_free(srd);
	}
	gf_list_del(dash->SRDs);
	dash->SRDs = NULL;
}

#ifndef GPAC_DISABLE_LOG
static u32 gf_dash_get_start_number(GF_DASH_Group *group, GF_MPD_Representation *rep)
{
	if (rep->segment_list && rep->segment_list->start_number) return rep->segment_list->start_number;
	if (group->adaptation_set->segment_list && group->adaptation_set->segment_list->start_number) return group->adaptation_set->segment_list->start_number;
	if (group->period->segment_list && group->period->segment_list->start_number) return group->period->segment_list->start_number;

	if (rep->segment_template && rep->segment_template->start_number) return rep->segment_template->start_number;
	if (group->adaptation_set->segment_template && group->adaptation_set->segment_template->start_number) return group->adaptation_set->segment_template->start_number;
	if (group->period->segment_template && group->period->segment_template->start_number) return group->period->segment_template->start_number;

	return 0;
}
#endif

static GF_MPD_Representation *gf_dash_find_rep(GF_DashClient *dash, const char *dependency_id, GF_DASH_Group **rep_group)
{
	u32 i, j, nb_groups, nb_reps;
	GF_MPD_Representation *rep;

	if (rep_group) *rep_group = NULL;

	if (!dependency_id) return NULL;

	nb_groups = gf_list_count(dash->groups);
	for (i=0; i<nb_groups; i++) {
		GF_DASH_Group *group = gf_list_get(dash->groups, i);
		nb_reps = gf_list_count(group->adaptation_set->representations);
		for (j=0; j<nb_reps; j++) {
			rep = gf_list_get(group->adaptation_set->representations, j);
			if (rep->id && !strcmp(rep->id, dependency_id)) {
				if (rep_group) *rep_group = group;
				return rep;
			}
		}
	}
	return NULL;
}

static
s32 gf_dash_group_get_dependency_group(GF_DashClient *dash, u32 idx)
{
	GF_MPD_Representation *rep;
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return idx;

	rep = gf_list_get(group->adaptation_set->representations, 0);

	while (rep && rep->dependency_id) {
		char *sep = strchr(rep->dependency_id, ' ');
		if (sep) sep[0] = 0;
		rep = gf_dash_find_rep(dash, rep->dependency_id, &group);
		if (sep) sep[0] = ' ';
	}
	return group->groups_idx;
}

GF_EXPORT
s32 gf_dash_group_has_dependent_group(GF_DashClient *dash, u32 idx)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return GF_FALSE;
	return group->depend_on_group ? group->depend_on_group->groups_idx : -1;
}

GF_EXPORT
u32 gf_dash_group_get_num_groups_depending_on(GF_DashClient *dash, u32 idx)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return 0;
	return group->groups_depending_on ? gf_list_count(group->groups_depending_on) : 0;
}

GF_EXPORT
s32 gf_dash_get_dependent_group_index(GF_DashClient *dash, u32 idx, u32 group_depending_on_dep_idx)
{
	GF_DASH_Group *group_depending_on;
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group || !group->groups_depending_on) return -1;
	group_depending_on = gf_list_get(group->groups_depending_on, group_depending_on_dep_idx);
	if (!group_depending_on) return -1;
	return group_depending_on->groups_idx;
}

/* create groups (implementation of adaptations set) */
GF_Err gf_dash_setup_groups(GF_DashClient *dash)
{
	GF_Err e;
	u32 i, j, count, nb_dependent_rep;
	GF_MPD_Period *period;

	if (!dash->groups) {
		dash->groups = gf_list_new();
		if (!dash->groups) return GF_OUT_OF_MEM;
	}

	period = gf_list_get(dash->mpd->periods, dash->active_period_index);
	if (!period) return GF_BAD_PARAM;

	count = gf_list_count(period->adaptation_sets);
	for (i=0; i<count; i++) {
		Double seg_dur;
		GF_DASH_Group *group;
		Bool found = GF_FALSE;
		Bool has_dependent_representations = GF_FALSE;
		GF_MPD_AdaptationSet *set = gf_list_get(period->adaptation_sets, i);
		for (j=0; j<gf_list_count(dash->groups); j++) {
			group = gf_list_get(dash->groups, j);
			if (group->adaptation_set==set) {
				found = 1;
				break;
			}
		}

		if (found) continue;

		if (! gf_list_count(set->representations)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Empty adaptation set found (ID %d) - ignoring\n", set->id));
			continue;
		}


		GF_SAFEALLOC(group, GF_DASH_Group);
		if (!group) return GF_OUT_OF_MEM;
		group->dash = dash;
		group->adaptation_set = set;
		group->period = period;
		group->bitstream_switching = (set->bitstream_switching || period->bitstream_switching) ? GF_TRUE : GF_FALSE;

		seg_dur = 0;
		nb_dependent_rep = 0;
		for (j=0; j<gf_list_count(set->representations); j++) {
			Double dur;
			u32 nb_seg, k;
			Bool cp_supported;
			GF_MPD_Representation *rep = gf_list_get(set->representations, j);
			gf_dash_get_segment_duration(rep, set, period, dash->mpd, &nb_seg, &dur);
			if (dur>seg_dur) seg_dur = dur;

			if (group->bitstream_switching && (set->segment_base || period->segment_base || rep->segment_base) ) {
				group->bitstream_switching = GF_FALSE;
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] bitstreamSwitching set for onDemand content - ignoring flag\n"));
			}

			if (dash->dash_io->dash_codec_supported) {
				Bool res = dash->dash_io->dash_codec_supported(dash->dash_io, rep->codecs, rep->width, rep->height, (rep->scan_type==GF_MPD_SCANTYPE_INTERLACED) ? 1 : 0, rep->framerate ? rep->framerate->num : 0, rep->framerate ? rep->framerate->den : 0, rep->samplerate);
				if (!res) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Representation not supported by playback engine - ignoring\n"));
					rep->playback.disabled = 1;
					continue;
				}
			}
			//filter out everything above HD
			if ((dash->max_width>2000) && (dash->max_height>2000)) {
				if ((rep->width>dash->max_width) || (rep->height>dash->max_height)) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Representation size %dx%d exceeds max display size allowed %dx%d - ignoring\n", rep->width, rep->height, dash->max_width, dash->max_height));
					rep->playback.disabled = 1;
					continue;
				}
			}
			if (rep->codecs && (dash->max_bit_per_pixel > 8) ) {
				char *vid_type = strstr(rep->codecs, "hvc");
				if (!vid_type) vid_type = strstr(rep->codecs, "hev");
				if (!vid_type) vid_type = strstr(rep->codecs, "avc");
				if (!vid_type) vid_type = strstr(rep->codecs, "svc");
				if (!vid_type) vid_type = strstr(rep->codecs, "mvc");
				//HEVC
				if (vid_type && (!strnicmp(rep->codecs, "hvc", 3) || !strnicmp(rep->codecs, "hev", 3))) {
					char *pidc = rep->codecs+5;
					if ((pidc[0]=='A') || (pidc[0]=='B') || (pidc[0]=='C')) pidc++;
					//Main 10 !!
					if (!strncmp(pidc, "2.", 2)) {
						rep->playback.disabled = 1;
						GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Representation bit depth higher than max display bit depth - ignoring\n"));
						continue;
					}
				}
				//AVC
				if (vid_type && (!strnicmp(rep->codecs, "avc", 3) || !strnicmp(rep->codecs, "svc", 3) || !strnicmp(rep->codecs, "mvc", 3))) {
					char prof_string[3];
					u8 prof;
					strncpy(prof_string, vid_type+5, 2);
					prof_string[2]=0;
					prof = atoi(prof_string);
					//Main 10
					if (prof==0x6E) {
						rep->playback.disabled = 1;
						GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Representation bit depth higher than max display bit depth - ignoring\n"));
						continue;
					}
				}
			}

			for (k=0; k<gf_list_count(rep->essential_properties); k++) {
				GF_MPD_Descriptor *mpd_desc = gf_list_get(rep->essential_properties, k);

				//we don't know any defined scheme for now
				if (! strstr(mpd_desc->scheme_id_uri, "gpac") ) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Representation with unrecognized EssentialProperty %s - ignoring because not supported\n", mpd_desc->scheme_id_uri));
					rep->playback.disabled = 1;
					break;
				}
			}
			if (rep->playback.disabled) {
				continue;
			}

			cp_supported = 1;
			for (k=0; k<gf_list_count(rep->content_protection); k++) {
				GF_MPD_Descriptor *mpd_desc = gf_list_get(rep->content_protection, k);
				//we don't know any defined scheme for now
				if (strcmp(mpd_desc->scheme_id_uri, "urn:mpeg:dash:mp4protection:2011")) {
					GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Representation with unrecognized ContentProtection %s\n", mpd_desc->scheme_id_uri));
					cp_supported = 0;
				} else {
					cp_supported = 1;
					break;
				}
			}
			if (!cp_supported) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Representation with no supported ContentProtection\n"));
				rep->playback.disabled = 1;
				continue;
			}

			rep->playback.disabled = 0;
			if (rep->width>set->max_width) {
				set->max_width = rep->width;
				set->max_height = rep->height;
			}
			if (rep->dependency_id && strlen(rep->dependency_id))
				has_dependent_representations = GF_TRUE;
			else
				group->base_rep_index_plus_one = j+1;
			rep->playback.enhancement_rep_index_plus_one = 0;
			for (k = 0; k < gf_list_count(set->representations); k++) {
				GF_MPD_Representation *a_rep = gf_list_get(set->representations, k);
				if (a_rep->dependency_id) {
					char * tmp = strrchr(a_rep->dependency_id, ' ');
					if (tmp)
						tmp = tmp + 1;
					else
						tmp = a_rep->dependency_id;
					if (!strcmp(tmp, rep->id))
						rep->playback.enhancement_rep_index_plus_one = k + 1;
				}
			}
			if (!rep->playback.enhancement_rep_index_plus_one)
				group->max_complementary_rep_index = j;
			if (!rep->playback.disabled && rep->dependency_id)
				nb_dependent_rep++;
		}

		if (!seg_dur && !dash->is_m3u8) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Cannot compute default segment duration\n"));
		}

		group->cache_duration = dash->max_cache_duration;
		if (group->cache_duration < dash->mpd->min_buffer_time)
			group->cache_duration = dash->mpd->min_buffer_time;

		group->max_cached_segments = (nb_dependent_rep+1);

		if (!has_dependent_representations)
			group->base_rep_index_plus_one = 0; // all representations in this group are independent

		if (group->max_cached_segments>50) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Too many cached segments (%d), segment duration %g - using 50 max\n", group->max_cached_segments, seg_dur));
			group->max_cached_segments = 50;
		}
		group->cached = gf_malloc(sizeof(segment_cache_entry)*group->max_cached_segments);
		memset(group->cached, 0, sizeof(segment_cache_entry)*group->max_cached_segments);
		if (!group->cached) {
			gf_free(group);
			return GF_OUT_OF_MEM;
		}
		group->groups_idx = gf_list_count(dash->groups);
		e = gf_list_add(dash->groups, group);
		if (e) {
			gf_free(group->cached);
			gf_free(group);
			return e;
		}
	}


	count = gf_list_count(dash->groups);
	for (i=0; i<count; i++) {
		GF_DASH_Group *group = gf_list_get(dash->groups, i);
		j = gf_dash_group_get_dependency_group(dash, i);
		if (i != j) {
			GF_DASH_Group *base_group = gf_list_get(dash->groups, j);
			assert(base_group);
			group->depend_on_group = base_group;
			if (!base_group->groups_depending_on) {
				base_group->groups_depending_on = gf_list_new();
			}
			gf_list_add(base_group->groups_depending_on, group);
		}
	}

	for (i=0; i<count; i++) {
		GF_DASH_Group *group = gf_list_get(dash->groups, i);
		if (group->groups_depending_on) {
			u32 nb_dep_groups = gf_list_count(group->groups_depending_on);
			//all dependent groups will be stored in the base group
			group->max_cached_segments *= (1+nb_dep_groups);
			group->cached = gf_realloc(group->cached, sizeof(segment_cache_entry)*group->max_cached_segments);
			memset(group->cached, 0, sizeof(segment_cache_entry)*group->max_cached_segments);

			for (j=0; j<nb_dep_groups; j++) {
				GF_DASH_Group *dep_group = gf_list_get(group->groups_depending_on, j);

				dep_group->max_cached_segments = 0;

				/* the rest of the code assumes that at least group->cached[0] is allocated */
				dep_group->cached = gf_realloc(dep_group->cached, sizeof(segment_cache_entry));
				memset(dep_group->cached, 0, sizeof(segment_cache_entry));

			}
		}
	}

	return GF_OK;
}

static GF_Err gf_dash_load_sidx(GF_BitStream *bs, GF_MPD_Representation *rep, Bool separate_index, u64 sidx_offset)
{
#ifdef GPAC_DISABLE_ISOM
	return GF_NOT_SUPPORTED;
#else
	u64 anchor_position, prev_pos;
	GF_SegmentIndexBox *sidx = NULL;
	u32 i, size, type;
	GF_Err e;
	u64 offset;

	prev_pos = gf_bs_get_position(bs);
	gf_bs_seek(bs, sidx_offset);
	size = gf_bs_read_u32(bs);
	type = gf_bs_read_u32(bs);
	if (type != GF_ISOM_BOX_TYPE_SIDX) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error parsing SIDX: type is %s (box start offset "LLD")\n", gf_4cc_to_str(type), gf_bs_get_position(bs)-8 ));
		return GF_ISOM_INVALID_FILE;
	}

	gf_bs_seek(bs, sidx_offset);

	anchor_position = sidx_offset + size;
	if (separate_index)
		anchor_position = 0;

	e = gf_isom_box_parse((GF_Box **) &sidx, bs);
	if (e) return e;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Loading SIDX - %d entries - Earliest Presentation Time "LLD"\n", sidx->nb_refs, sidx->earliest_presentation_time));

	offset = sidx->first_offset + anchor_position;
	rep->segment_list->timescale = sidx->timescale;
	for (i=0; i<sidx->nb_refs; i++) {
		if (sidx->refs[i].reference_type) {
			e = gf_dash_load_sidx(bs, rep, separate_index, offset);
			if (e) {
				break;
			}
		} else {
			GF_MPD_SegmentURL *seg;
			GF_SAFEALLOC(seg, GF_MPD_SegmentURL);
			if (!seg) return GF_OUT_OF_MEM;
			GF_SAFEALLOC(seg->media_range, GF_MPD_ByteRange);
			if (!seg->media_range) return GF_OUT_OF_MEM;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Found media segment size %d - duration %d - start with SAP: %d - SAP type %d - SAP Deltat Time %d\n",
			                                   sidx->refs[i].reference_size, sidx->refs[i].subsegment_duration, sidx->refs[i].starts_with_SAP, sidx->refs[i].SAP_type, sidx->refs[i].SAP_delta_time));

			seg->media_range->start_range = offset;
			offset += sidx->refs[i].reference_size;
			seg->media_range->end_range = offset - 1;
			seg->duration = sidx->refs[i].subsegment_duration;
			gf_list_add(rep->segment_list->segment_URLs, seg);
		}
	}
	gf_isom_box_del((GF_Box*)sidx);
	gf_bs_seek(bs, prev_pos);
	return e;
#endif
}

static GF_Err gf_dash_load_representation_sidx(GF_DASH_Group *group, GF_MPD_Representation *rep, const char *cache_name, Bool separate_index, Bool needs_mov_range)
{
	GF_Err e;
	GF_BitStream *bs;
	FILE *f=NULL;
	if (!cache_name) return GF_BAD_PARAM;

	if (!strncmp(cache_name, "gmem://", 7)) {
		u32 size;
		u8 *mem_address;
		e = gf_blob_get(cache_name, &mem_address, &size, NULL);
		if (e) return e;

		bs = gf_bs_new(mem_address, size, GF_BITSTREAM_READ);
        gf_blob_release(cache_name);
	} else {
		f = gf_fopen(cache_name, "rb");
		if (!f) return GF_IO_ERR;
		bs = gf_bs_from_file(f, GF_BITSTREAM_READ);
	}
	e = GF_OK;
	while (gf_bs_available(bs)) {
		u32 size = gf_bs_read_u32(bs);
		u32 type = gf_bs_read_u32(bs);
		if (type != GF_ISOM_BOX_TYPE_SIDX) {
			gf_bs_skip_bytes(bs, size-8);

			if (needs_mov_range && (type==GF_ISOM_BOX_TYPE_MOOV )) {
				GF_SAFEALLOC(rep->segment_list->initialization_segment->byte_range, GF_MPD_ByteRange);
				if (rep->segment_list->initialization_segment->byte_range)
					rep->segment_list->initialization_segment->byte_range->end_range = gf_bs_get_position(bs);
			}
			continue;
		}
		gf_bs_seek(bs, gf_bs_get_position(bs)-8);
		e = gf_dash_load_sidx(bs, rep, separate_index, gf_bs_get_position(bs) );

		/*we could also parse the sub sidx*/
		break;
	}
	gf_bs_del(bs);
	if (f) gf_fclose(f);
	return e;
}

static GF_Err dash_load_box_type(const char *cache_name, u32 offset, u32 *box_type, u32 *box_size)
{
	*box_type = *box_size = 0;
	if (!strncmp(cache_name, "gmem://", 7)) {
		GF_Err e;
		u32 size;
		u8 *mem_address;
		e = gf_blob_get(cache_name, &mem_address, &size, NULL);
		if (e) return e;
        gf_blob_release(cache_name);
		if (offset+8 > size)
			return GF_IO_ERR;
		mem_address += offset;
		*box_size = GF_4CC(mem_address[0], mem_address[1], mem_address[2], mem_address[3]);
		*box_type = GF_4CC(mem_address[4], mem_address[5], mem_address[6], mem_address[7]);
	} else {
		unsigned char data[4];
		FILE *f = gf_fopen(cache_name, "rb");
		if (!f) return GF_IO_ERR;
		if (gf_fseek(f, offset, SEEK_SET))
			return GF_IO_ERR;
		if (gf_fread(data, 4, f) == 4) {
			*box_size = GF_4CC(data[0], data[1], data[2], data[3]);
			if (gf_fread(data, 4, f) == 4) {
				*box_type = GF_4CC(data[0], data[1], data[2], data[3]);
			}
		}
		gf_fclose(f);
	}
	return GF_OK;
}

extern void gf_mpd_segment_template_free(void *_item);
static GF_Err gf_dash_setup_single_index_mode(GF_DASH_Group *group)
{
	u32 i;
	GF_Err e = GF_OK;
	char *init_url = NULL;
	char *index_url = NULL;
	GF_DASHFileIOSession *download_sess;
	GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, 0);

	download_sess = &group->dash->mpd_dnload;

	if (!rep->segment_base && !group->adaptation_set->segment_base && !group->period->segment_base) {
		if (rep->segment_template || group->adaptation_set->segment_template || group->period->segment_template) return GF_OK;
		if (rep->segment_list || group->adaptation_set->segment_list || group->period->segment_list) return GF_OK;
	} else {
		char *profile = rep->profiles;
		if (!profile) profile = group->adaptation_set->profiles;
		if (!profile) profile = group->dash->mpd->profiles;

		//if on-demand cleanup all segment templates and segment list if we have base URLs
		if (profile && strstr(profile, "on-demand")) {
			u32 nb_rem=0;
			if (rep->segment_template) {
				nb_rem++;
				gf_mpd_segment_template_free(rep->segment_template);
				rep->segment_template = NULL;
			}
			if (group->adaptation_set->segment_template) {
				nb_rem++;
				gf_mpd_segment_template_free(group->adaptation_set->segment_template);
				group->adaptation_set->segment_template = NULL;
			}

			if (group->period->segment_template) {
				nb_rem++;
				gf_mpd_segment_template_free(group->period->segment_template);
				group->period->segment_template = NULL;
			}
			if (nb_rem) {
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] SegmentTemplate present for on-demand with SegmentBase present - skipping SegmentTemplate\n"));
			}
			nb_rem=0;
			if (rep->segment_list) {
				nb_rem++;
				gf_mpd_segment_template_free(rep->segment_list);
				rep->segment_list = NULL;
			}
			if (group->adaptation_set->segment_list) {
				nb_rem++;
				gf_mpd_segment_template_free(group->adaptation_set->segment_list);
				group->adaptation_set->segment_list = NULL;
			}
			if (group->period->segment_list) {
				nb_rem++;
				gf_mpd_segment_template_free(group->period->segment_list);
				group->period->segment_list = NULL;
			}
			if (nb_rem) {
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] SegmentList present for on-demand with SegmentBase present - skipping SegmentList\n"));
			}
		}
	}

	/*OK we are in single-file mode, download all required indexes & co*/
	for (i=0; i<gf_list_count(group->adaptation_set->representations); i++) {
		char *sidx_file = NULL;
		u64 duration, index_start_range = 0, index_end_range = 0, init_start_range, init_end_range;
		Bool index_in_base, init_in_base;
		Bool init_needs_byte_range = GF_FALSE;
		Bool has_seen_sidx = GF_FALSE;
		Bool is_isom = GF_TRUE;
		rep = gf_list_get(group->adaptation_set->representations, i);

		index_in_base = init_in_base = GF_FALSE;
		e = gf_dash_resolve_url(group->dash->mpd, rep, group, group->dash->base_url, GF_MPD_RESOLVE_URL_INIT, 0, &init_url, &init_start_range, &init_end_range, &duration, &init_in_base, NULL, NULL, NULL, NULL);
		if (e) goto exit;

		e = gf_dash_resolve_url(group->dash->mpd, rep, group, group->dash->base_url, GF_MPD_RESOLVE_URL_INDEX, 0, &index_url, &index_start_range, &index_end_range, &duration, &index_in_base, NULL, NULL, NULL, NULL);
		if (e) goto exit;


		if (is_isom && (init_in_base || index_in_base)) {
			if (!strstr(init_url, "://") || (!strnicmp(init_url, "file://", 7) ) ) {
				GF_SAFEALLOC(rep->segment_list, GF_MPD_SegmentList);
				if (!rep->segment_list) {
					e = GF_OUT_OF_MEM;
					goto exit;
				}
				rep->segment_list->segment_URLs = gf_list_new();

				if (rep->segment_base) rep->segment_list->presentation_time_offset = rep->segment_base->presentation_time_offset;
				else if (group->adaptation_set->segment_base) rep->segment_list->presentation_time_offset = group->adaptation_set->segment_base->presentation_time_offset;
				else if (group->period->segment_base) rep->segment_list->presentation_time_offset = group->period->segment_base->presentation_time_offset;

				if (init_in_base) {
					GF_SAFEALLOC(rep->segment_list->initialization_segment, GF_MPD_URL);
					if (!rep->segment_list->initialization_segment) {
						e = GF_OUT_OF_MEM;
						goto exit;
					}
					rep->segment_list->initialization_segment->sourceURL = gf_strdup(init_url);
					rep->segment_list->initialization_segment->is_resolved = GF_TRUE;
					/*we don't want to load the entire movie */
					init_needs_byte_range = 1;
				}
				if (index_in_base) {
					sidx_file = (char *)init_url;
				}
			}
			/*we need to download the init segment, at least partially*/
			else {
				u32 offset = 0;
				u32 box_type = 0;
				u32 box_size = 0;
				u32 sidx_start = 0;
				const char *cache_name;

				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Downloading init segment and SIDX for representation %s\n", init_url));

				/*download first 8 bytes and check if we do have a box starting there*/
				e = gf_dash_download_resource(group->dash, download_sess, init_url, offset, 7, 1, group);
				if (e) goto exit;
				cache_name = group->dash->dash_io->get_cache_name(group->dash->dash_io, *download_sess);
				e = dash_load_box_type(cache_name, offset, &box_type, &box_size);
				offset = 8;
				while (box_type) {
					/*we got the moov, stop here */
					if (!index_in_base && (box_type==GF_ISOM_BOX_TYPE_MOOV)) {
						e = gf_dash_download_resource(group->dash, download_sess, init_url, offset, offset+box_size-9, 2, group);
						break;
					} else {
						const u32 offset_ori = offset;
						e = gf_dash_download_resource(group->dash, download_sess, init_url, offset, offset+box_size-1, 2, group);
						if (e < 0) goto exit;
						offset += box_size;
						/*we need to refresh the cache name because of our memory astorage thing ...*/
						cache_name = group->dash->dash_io->get_cache_name(group->dash->dash_io, *download_sess);
						e = dash_load_box_type(cache_name, offset-8, &box_type, &box_size);
						if (e == GF_IO_ERR) {
							/*if the socket was closed then gf_dash_download_resource() with gmem:// was reset - retry*/
							e = dash_load_box_type(cache_name, offset-offset_ori-8, &box_type, &box_size);
							if (box_type == GF_ISOM_BOX_TYPE_SIDX) {
								offset -= 8;
								/*FIXME sidx found, reload the full resource*/
								GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] have to re-downloading init and SIDX for rep %s\n", init_url));
								e = gf_dash_download_resource(group->dash, download_sess, init_url, 0, offset+box_size-1, 2, group);
								break;
							}
						}

						if (box_type == GF_ISOM_BOX_TYPE_SIDX) {
							if (!sidx_start) sidx_start = offset;
							has_seen_sidx = 1;
						} else if (has_seen_sidx)
							break;
					}
				}
				if (e < 0) goto exit;

				if (box_type == 0) {
					e = GF_ISOM_INVALID_FILE;
					goto exit;
				}
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Done downloading init segment and SIDX\n"));

				GF_SAFEALLOC(rep->segment_list, GF_MPD_SegmentList);
				if (!rep->segment_list) {
					e = GF_OUT_OF_MEM;
					goto exit;
				}
				rep->segment_list->segment_URLs = gf_list_new();

				cache_name = group->dash->dash_io->get_cache_name(group->dash->dash_io, *download_sess);
				if (init_in_base) {
					char szName[100];
					GF_SAFEALLOC(rep->segment_list->initialization_segment, GF_MPD_URL);
					if (!rep->segment_list->initialization_segment) {
						e = GF_OUT_OF_MEM;
						goto exit;
					}

					rep->segment_list->initialization_segment->sourceURL = gf_strdup(init_url);
					GF_SAFEALLOC(rep->segment_list->initialization_segment->byte_range, GF_MPD_ByteRange);
					if (rep->segment_list->initialization_segment->byte_range) {
						rep->segment_list->initialization_segment->byte_range->start_range = init_start_range;
						rep->segment_list->initialization_segment->byte_range->end_range = init_end_range ? init_end_range : (sidx_start-1);
					}

					//we need to store the init segment since it has the same name as the rest of the segments and will be destroyed when cleaning up the cache ..
					else if (!strnicmp(cache_name, "gmem://", 7)) {
						u8 *mem_address;
						e = gf_blob_get(cache_name, &mem_address, &rep->playback.init_segment.size, NULL);
						if (e) {
							goto exit;
						}
						rep->playback.init_segment.data = gf_malloc(sizeof(char) * rep->playback.init_segment.size);
						memcpy(rep->playback.init_segment.data, mem_address, sizeof(char) * rep->playback.init_segment.size);

						sprintf(szName, "gmem://%p", &rep->playback.init_segment);
						rep->segment_list->initialization_segment->sourceURL = gf_strdup(szName);
						rep->segment_list->initialization_segment->is_resolved = GF_TRUE;
                        gf_blob_release(cache_name);
					} else {
						FILE *t = gf_fopen(cache_name, "rb");
						if (t) {
							s32 res;
							rep->playback.init_segment.size = (u32) gf_fsize(t);

							rep->playback.init_segment.data = gf_malloc(sizeof(char) * rep->playback.init_segment.size);
							res = (s32) gf_fread(rep->playback.init_segment.data, rep->playback.init_segment.size, t);
							if (res != rep->playback.init_segment.size) {
								GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Failed to store init segment\n"));
							} else if (rep->segment_list && rep->segment_list->initialization_segment) {
								sprintf(szName, "gmem://%p", &rep->playback.init_segment);
								rep->segment_list->initialization_segment->sourceURL = gf_strdup(szName);
								rep->segment_list->initialization_segment->is_resolved = GF_TRUE;
							}
						}
					}
				}
				if (index_in_base) {
					sidx_file = (char *)cache_name;
				}
			}
		}
		/*we have index url, download it*/
		if (! index_in_base) {
			e = gf_dash_download_resource(group->dash, download_sess, index_url, index_start_range, index_end_range, 1, group);
			if (e) goto exit;
			sidx_file = (char *)group->dash->dash_io->get_cache_name(group->dash->dash_io, *download_sess);
		}

		/*load sidx*/
		e = gf_dash_load_representation_sidx(group, rep, sidx_file, !index_in_base, init_needs_byte_range);
		if (e) {
			rep->playback.disabled = 1;
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Failed to load segment index for this representation - disabling\n"));
		}

		//cleanup cache right away
		group->dash->dash_io->delete_cache_file(group->dash->dash_io, *download_sess, init_url);

		/*reset all seg based stuff*/
		if (rep->segment_base) {
			gf_mpd_segment_base_free(rep->segment_base);
			rep->segment_base = NULL;
		}

		gf_free(index_url);
		index_url = NULL;
		gf_free(init_url);
		init_url = NULL;
	}
	if (group->adaptation_set->segment_base) {
		gf_mpd_segment_base_free(group->adaptation_set->segment_base);
		group->adaptation_set->segment_base = NULL;
	}
	group->was_segment_base = 1;

exit:
	if (init_url) gf_free(init_url);
	if (index_url) gf_free(index_url);
	return e;
}

static void gf_dash_solve_period_xlink(GF_DashClient *dash, GF_List *period_list, u32 period_idx)
{
	u32 count, i;
	GF_Err e;
	u64 start = 0;
	u64 src_duration = 0;
	Bool is_local=GF_FALSE;
	const char *local_url;
	char *url, *period_xlink;
	GF_DOMParser *parser;
	GF_MPD *new_mpd;
	GF_MPD_Period *period;
	u32 nb_inserted = 0;
	GF_DASHFileIOSession xlink_sess=NULL;

	period = gf_list_get(period_list, period_idx);
	if (!period->xlink_href || (dash->route_clock_state==1)) {
		return;
	}
	start = period->start;
	GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Resolving period XLINK %s\n", period->xlink_href));

	if (!strcmp(period->xlink_href, "urn:mpeg:dash:resolve-to-zero:2013")) {
		//spec is not very clear here, I suppose it means "remove the element"
		gf_list_rem(period_list, period_idx);
		gf_mpd_period_free(period);
		return;
	}

	//ATSC puts a tag in front of the url ("tag:atsc.org,2016:xlink") - in case others decide to follow this crazy example, whe search for http:// or https://
	period_xlink = strstr(period->xlink_href, "http://");
	if (!period_xlink) period_xlink = strstr(period->xlink_href, "HTTP://");
	if (!period_xlink) period_xlink = strstr(period->xlink_href, "https://");
	if (!period_xlink) period_xlink = strstr(period->xlink_href, "HTTPS://");
	if (!period_xlink) period_xlink = period->xlink_href;

	//xlink relative to our MPD base URL
	url = gf_url_concatenate(dash->base_url, period_xlink);

	if (!strstr(url, "://") || !strnicmp(url, "file://", 7) ) {
		local_url = url;
		is_local=GF_TRUE;
		e = GF_OK;
	} else {
		if (dash->query_string) {
			char *full_url;
			char *purl, *sep;
			u32 len;
			purl = url ? url : period_xlink;
			len = (u32) (2 + strlen(purl) + strlen(dash->query_string) + (period->ID ? strlen(period->ID) : 0 ) );
			full_url = gf_malloc(sizeof(char)*len);

			strcpy(full_url, purl);
			if (strchr(purl, '?')) strcat(full_url, "&");
			else strcat(full_url, "?");

			//append the query string
			strcat(full_url, dash->query_string);
			//if =PID is given, replace by period ID
			sep = strstr(dash->query_string, "=PID");
			if (sep && period->ID) {
				char *sep2 = strstr(full_url, "=PID");
				assert(sep2);
				sep2[1] = 0;
				strcat(full_url, period->ID);
				strcat(full_url, sep+4);
			}

			/*use non-persistent connection for MPD*/
			e = gf_dash_download_resource(dash, &xlink_sess, full_url, 0, 0, 0, NULL);
			gf_free(full_url);

		} else {
			/*use non-persistent connection for MPD*/
			e = gf_dash_download_resource(dash, &xlink_sess, url ? url : period_xlink, 0, 0, 0, NULL);
		}
	}

	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot download xlink from periods %s: error %s\n", period->xlink_href, gf_error_to_string(e)));
		gf_free(period->xlink_href);
		period->xlink_href = NULL;
		if (xlink_sess) dash->dash_io->del(dash->dash_io, xlink_sess);
		if (url) gf_free(url);
		return;
	}

	if (!is_local) {
		/*in case the session has been restarted, local_url may have been destroyed - get it back*/
		local_url = dash->dash_io->get_cache_name(dash->dash_io, xlink_sess);
	}

	/* parse the MPD */
	parser = gf_xml_dom_new();
	e = gf_xml_dom_parse(parser, local_url, NULL, NULL);
	if (url) gf_free(url);
	url = NULL;

	if (xlink_sess) {
		//get redirected URL
		url = (char *) dash->dash_io->get_url(dash->dash_io, xlink_sess);
		if (url) url = gf_strdup(url);
		dash->dash_io->del(dash->dash_io, xlink_sess);
	}
	if (e != GF_OK) {
		gf_xml_dom_del(parser);
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot parse xlink periods: error in XML parsing %s\n", gf_error_to_string(e)));
		gf_free(period->xlink_href);
		period->xlink_href = NULL;
		if (url) gf_free(url);
		return;
	}
	new_mpd = gf_mpd_new();

	count = gf_xml_dom_get_root_nodes_count(parser);
	for (i=0; i<count; i++) {
		GF_XMLNode *root = gf_xml_dom_get_root_idx(parser, i);
		if (i) {
			e = gf_mpd_complete_from_dom(root, new_mpd, period->xlink_href);
		} else {
			e = gf_mpd_init_from_dom(root, new_mpd, period->xlink_href);
		}
		if (e) break;
	}
	gf_xml_dom_del(parser);
	if (e) {
		gf_free(period->xlink_href);
		period->xlink_href = NULL;
		gf_mpd_del(new_mpd);
		if (url) gf_free(url);
		return;
	}

	gf_list_rem(period_list, period_idx);

	if (dash->split_adaptation_set)
		gf_mpd_split_adaptation_sets(new_mpd);

	if (!period->duration) {
		GF_MPD_Period *next_period = gf_list_get(period_list, period_idx);
		if (next_period && next_period->start)
			period->duration = next_period->start - period->start;
	}
	src_duration = period->duration;
	//insert all periods
	while (gf_list_count(new_mpd->periods)) {
		GF_MPD_Period *inserted_period = gf_list_get(new_mpd->periods, 0);
		gf_list_rem(new_mpd->periods, 0);
		//forbiden
		if (inserted_period->xlink_href && inserted_period->xlink_actuate_on_load) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Invalid remote period with xlink:actuate=\"onLoad\" and xlink:href set, removing parent period.\n"));
			gf_mpd_period_free(inserted_period);
			continue;
		}
		inserted_period->origin_base_url = url ? gf_strdup(url) : NULL;
		inserted_period->start = start;
		inserted_period->type = GF_MPD_TYPE_STATIC;

		gf_list_insert(period_list, inserted_period, period_idx);
		period_idx++;
		nb_inserted++;

		if (period->duration) {
			//truncate duration
			if (inserted_period->duration > src_duration) {
				inserted_period->duration = src_duration;
				break;
			} else {
				src_duration -= inserted_period->duration;
			}
		}
		start += inserted_period->duration;
	}
	if (url) gf_free(url);

	if (!nb_inserted && gf_list_count(period->adaptation_sets)) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] No periods inserted during ad insertion, but origin period not empty - ignoring ad insertion.\n"));
		gf_list_insert(period_list, period, period_idx);
		period->broken_xlink = period->xlink_href;
		period->xlink_href = NULL;
	} else {
		//this will do the garbage collection
		gf_list_add(new_mpd->periods, period);
	}
	gf_mpd_del(new_mpd);
}

static u32 gf_dash_get_tiles_quality_rank(GF_DashClient *dash, GF_DASH_Group *tile_group)
{
	s32 res, res2;
	struct _dash_srd_desc *srd = tile_group->srd_desc;

	//no SRD is max quality for now
	if (!srd) return 0;
	if (!tile_group->srd_w || !tile_group->srd_h) return 0;

	if (tile_group->quality_degradation_hint) {
		u32 v = tile_group->quality_degradation_hint * MAX(srd->srd_nb_rows, srd->srd_nb_cols);
		v/=100;
		if (dash->disable_low_quality_tiles)
			tile_group->disabled = GF_TRUE;
		return v;
	}
	tile_group->disabled = GF_FALSE;

	//TODO - use visibility rect as well

	switch (dash->tile_adapt_mode) {
	case GF_DASH_ADAPT_TILE_NONE:
		return 0;
	case GF_DASH_ADAPT_TILE_ROWS:
		return tile_group->srd_row_idx;
	case GF_DASH_ADAPT_TILE_ROWS_REVERSE:
		return srd->srd_nb_rows - 1 - tile_group->srd_row_idx;
	case GF_DASH_ADAPT_TILE_ROWS_MIDDLE:
		res = srd->srd_nb_rows/2;
		res -= tile_group->srd_row_idx;
		return ABS(res);
	case GF_DASH_ADAPT_TILE_COLUMNS:
		return tile_group->srd_col_idx;
	case GF_DASH_ADAPT_TILE_COLUMNS_REVERSE:
		return srd->srd_nb_cols - 1 - tile_group->srd_col_idx;
	case GF_DASH_ADAPT_TILE_COLUMNS_MIDDLE:
		res = srd->srd_nb_cols/2;
		res -= tile_group->srd_col_idx;
		return ABS(res);
	case GF_DASH_ADAPT_TILE_CENTER:
		res = srd->srd_nb_rows/2 - tile_group->srd_row_idx;
		res2 = srd->srd_nb_cols/2 - tile_group->srd_col_idx;
		return MAX( ABS(res), ABS(res2) );
	case GF_DASH_ADAPT_TILE_EDGES:
		res = srd->srd_nb_rows/2 - tile_group->srd_row_idx;
		res = srd->srd_nb_rows/2 - ABS(res);
		res2 = srd->srd_nb_cols/2 - tile_group->srd_col_idx;
		res2 = srd->srd_nb_cols/2 - ABS(res2);
		return MIN( res, res2 );
	}
	return 0;
}

//used upon startup of the session only
static void gf_dash_set_tiles_quality(GF_DashClient *dash, struct _dash_srd_desc *srd, Bool force_all)
{
	u32 i, count;
	Bool tiles_use_lowest = (dash->first_select_mode==GF_DASH_SELECT_BANDWIDTH_HIGHEST_TILES) ? GF_TRUE : GF_FALSE;

	count = gf_list_count(dash->groups);
	for (i=0; i<count; i++) {
		GF_DASH_Group *group = gf_list_get(dash->groups, i);
		u32 lower_quality;
		if (group->srd_desc != srd) continue;

		//dynamic changes of qualities, only update if changed
		if (!force_all) {
			if (!group->update_tile_qualities) continue;
			group->update_tile_qualities = GF_FALSE;
		}

		lower_quality = gf_dash_get_tiles_quality_rank(dash, group);
		if (!lower_quality) continue;

		if (tiles_use_lowest && (group->active_rep_index >= lower_quality)) {
			lower_quality = group->active_rep_index - lower_quality;
		} else {
			lower_quality = 0;
		}
		gf_dash_set_group_representation(group, gf_list_get(group->adaptation_set->representations, lower_quality), GF_FALSE);
	}
}

static struct _dash_srd_desc *gf_dash_get_srd_desc(GF_DashClient *dash, u32 srd_id, Bool do_create)
{
	u32 i, count;
	struct _dash_srd_desc *srd;
	count = dash->SRDs ? gf_list_count(dash->SRDs) : 0;
	for (i=0; i<count; i++) {
		srd = gf_list_get(dash->SRDs, i);
		if (srd->id==srd_id) return srd;
	}
	if (!do_create) return NULL;
	GF_SAFEALLOC(srd, struct _dash_srd_desc);
	if (!srd) return NULL;
	srd->id = srd_id;
	if (!dash->SRDs) dash->SRDs = gf_list_new();
	gf_list_add(dash->SRDs, srd);
	return srd;
}

static GF_Err gf_dash_setup_period(GF_DashClient *dash)
{
	GF_MPD_Period *period;
	u32 rep_i, as_i, group_i, j, nb_groups_ok;
	u32 retry = 10;

	//solve xlink - if
	while (retry) {
		period = gf_list_get(dash->mpd->periods, dash->active_period_index);
		if (!period) return GF_EOS;
		if (!period->xlink_href) break;
		gf_dash_solve_period_xlink(dash, dash->mpd->periods, dash->active_period_index);
		retry --;
	}
	period = gf_list_get(dash->mpd->periods, dash->active_period_index);
	if (period->xlink_href && (dash->route_clock_state!=1) ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Too many xlink indirections on the same period - not supported\n"));
		return GF_NOT_SUPPORTED;
	}

	if (!period->duration) {
		GF_MPD_Period *next_period = gf_list_get(dash->mpd->periods, dash->active_period_index+1);
		if (next_period && next_period->start)
			period->duration = next_period->start - period->start;
	}

	/*we are not able to process webm dash (youtube)*/
	j = gf_list_count(period->adaptation_sets);
	for (as_i=0; as_i<j; as_i++) {
		GF_MPD_AdaptationSet *set = (GF_MPD_AdaptationSet*)gf_list_get(period->adaptation_sets, as_i);
		if (set->mime_type && strstr(set->mime_type, "webm")) {
			u32 k;
			for (k=0; k<gf_list_count(set->representations); ++k) {
				GF_MPD_Representation *rep = (GF_MPD_Representation*)gf_list_get(set->representations, k);
				rep->playback.disabled = GF_TRUE;
			}
		}
	}

	GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Setting up period start "LLU" duration "LLU" xlink %s ID %s\n", period->start, period->duration, period->origin_base_url ? period->origin_base_url : "none", period->ID ? period->ID : "none"));

	/*setup all groups*/
	gf_dash_setup_groups(dash);

	nb_groups_ok = 0;
	for (group_i=0; group_i<gf_list_count(dash->groups); group_i++) {
		GF_MPD_Representation *rep_sel;
		u32 active_rep, nb_rep;
		const char *mime_type;
		u32 nb_rep_ok = 0;
		Bool group_has_video = GF_FALSE;
		Bool disabled = GF_FALSE;
		Bool cp_supported = GF_FALSE;
		GF_DASH_Group *group = gf_list_get(dash->groups, group_i);
		Bool active_rep_found;

		active_rep = 0;

		if (dash->dbg_grps_index) {
			Bool disable = GF_TRUE;
			u32 gidx;
			for (gidx=0; gidx<dash->nb_dbg_grps; gidx++) {
				if (group_i == dash->dbg_grps_index[gidx]) {
					disable = GF_FALSE;
					break;
				}
			}
			if (disable) {
				group->selection = GF_DASH_GROUP_NOT_SELECTABLE;
				continue;
			}
		}

		nb_rep = gf_list_count(group->adaptation_set->representations);

		//on HLS get rid of audio only adaptation set if not in fMP4 mode
		if (dash->is_m3u8
			&& !group->adaptation_set->max_width
			&& !group->adaptation_set->max_height
			&& (gf_list_count(dash->groups)>1)
		) {
			GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, 0);
			if ((!rep->segment_template || !rep->segment_template->initialization)
				&& (!rep->segment_list || (!rep->segment_list->initialization_segment && !rep->segment_list->xlink_href))
			) {
				group->selection = GF_DASH_GROUP_NOT_SELECTABLE;
				continue;
			}
		}

		if ((nb_rep>1) && !group->adaptation_set->segment_alignment && !group->adaptation_set->subsegment_alignment) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] AdaptationSet without segmentAlignment flag set - may result in broken adaptation\n"));
		}
		if (group->adaptation_set->xlink_href) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] AdaptationSet with xlink:href to %s - ignoring because not supported\n", group->adaptation_set->xlink_href));
			continue;
		}

		for (j=0; j<gf_list_count(group->adaptation_set->essential_properties); j++) {
			GF_MPD_Descriptor *mpd_desc = gf_list_get(group->adaptation_set->essential_properties, j);
			if (!strcmp(mpd_desc->scheme_id_uri, "urn:mpeg:dash:srd:2014")) {
				u32 id, w, h, res;
				w = h = 0;
				res = sscanf(mpd_desc->value, "%d,%d,%d,%d,%d,%d,%d", &id, &group->srd_x, &group->srd_y, &group->srd_w, &group->srd_h, &w, &h);
				if (res != 7) {
					res = sscanf(mpd_desc->value, "%d,%d,%d,%d,%d", &id, &group->srd_x, &group->srd_y, &group->srd_w, &group->srd_h);
					if (res!=5) res=0;
				}
				if (res) {
					group->srd_desc = gf_dash_get_srd_desc(dash, id, GF_TRUE);
					if (!w) w = group->srd_x + group->srd_w;
					if (!h) h = group->srd_y + group->srd_h;

					if (w>group->srd_desc->srd_fw)
						group->srd_desc->srd_fw = w;
					if (h>group->srd_desc->srd_fh)
						group->srd_desc->srd_fh = h;
				}

			} else {
				//we don't know any defined scheme for now
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] AdaptationSet with unrecognized EssentialProperty %s - ignoring because not supported\n", mpd_desc->scheme_id_uri));
				disabled = 1;
				break;
			}
		}
		if (disabled) {
			continue;
		}

		cp_supported = 1;
		for (j=0; j<gf_list_count(group->adaptation_set->content_protection); j++) {
			GF_MPD_Descriptor *mpd_desc = gf_list_get(group->adaptation_set->content_protection, j);
			//we don't know any defined scheme for now
			if (strcmp(mpd_desc->scheme_id_uri, "urn:mpeg:dash:mp4protection:2011")) {
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] AdaptationSet with unrecognized ContentProtection %s\n", mpd_desc->scheme_id_uri));
				cp_supported = 0;
			} else {
				cp_supported = 1;
				break;
			}
		}
		if (!cp_supported) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] AdaptationSet with no supported ContentProtection - ignoring\n"));
			continue;
		}

		for (j=0; j<gf_list_count(group->adaptation_set->supplemental_properties); j++) {
			GF_MPD_Descriptor *mpd_desc = gf_list_get(group->adaptation_set->supplemental_properties, j);
			if (!strcmp(mpd_desc->scheme_id_uri, "urn:mpeg:dash:srd:2014")) {
				u32 id, w, h, res;
				w = h = 0;
				res = sscanf(mpd_desc->value, "%d,%d,%d,%d,%d,%d,%d", &id, &group->srd_x, &group->srd_y, &group->srd_w, &group->srd_h, &w, &h);
				if (res != 7) {
					res = sscanf(mpd_desc->value, "%d,%d,%d,%d,%d", &id, &group->srd_x, &group->srd_y, &group->srd_w, &group->srd_h);
					if (res != 5) res=0;
				}
				if (res) {
					group->srd_desc = gf_dash_get_srd_desc(dash, id, GF_TRUE);
					if (!w) w = group->srd_x + group->srd_w;
					if (!h) h = group->srd_y + group->srd_h;

					if (w>group->srd_desc->srd_fw)
						group->srd_desc->srd_fw = w;
					if (h>group->srd_desc->srd_fh)
						group->srd_desc->srd_fh = h;
				}
			}
		}

		/*translate from single-indexed file to SegmentList*/
		gf_dash_setup_single_index_mode(group);

		/* Select the appropriate representation in the given period */
		for (rep_i = 0; rep_i < nb_rep; rep_i++) {
			GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, rep_i);
			if (rep->width && rep->height) group_has_video = GF_TRUE;
		}

		//sort by ascending bandwidth and quality
		for (rep_i = 1; rep_i < nb_rep; rep_i++) {
			Bool swap=GF_FALSE;
			GF_MPD_Representation *r2 = gf_list_get(group->adaptation_set->representations, rep_i);
			GF_MPD_Representation *r1 = gf_list_get(group->adaptation_set->representations, rep_i-1);
			if (r1->bandwidth > r2->bandwidth) {
				swap=GF_TRUE;
			} else if ((r1->bandwidth == r2->bandwidth) && (r1->quality_ranking<r2->quality_ranking)) {
				swap=GF_TRUE;
			}
			if (swap) {
				gf_list_rem(group->adaptation_set->representations, rep_i);
				gf_list_insert(group->adaptation_set->representations, r2, rep_i-1);
				rep_i=0;
			}
		}

select_active_rep:
		group->min_representation_bitrate = (u32) -1;
		active_rep_found = GF_FALSE;
		for (rep_i = 0; rep_i < nb_rep; rep_i++) {
			u32 first_select_mode = dash->first_select_mode;
			GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, rep_i);
			if (rep->playback.disabled)
				continue;
			if (!active_rep_found) {
				active_rep = rep_i;
				active_rep_found = GF_TRUE;
			}

			rep_sel = gf_list_get(group->adaptation_set->representations, active_rep);

			if (group_has_video && !rep->width && !rep->height) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Adaptation %s: non-video in a video group - disabling it\n", rep->id));
				rep->playback.disabled = 1;
				continue;
			}

			if (group_has_video && !rep_sel->width && !rep_sel->height && rep->width && rep->height) {
				rep_sel = rep;
			}

			if (rep->bandwidth < group->min_representation_bitrate) {
				group->min_representation_bitrate = rep->bandwidth;
			}

			if (rep_i) {
				Bool ok;
				if (rep->codecs && rep_sel->codecs) {
					char *sep = strchr(rep_sel->codecs, '.');
					if (sep) sep[0] = 0;
					ok = !strnicmp(rep->codecs, rep_sel->codecs, strlen(rep_sel->codecs) );
					//check for scalable coding
					if (!ok && rep->dependency_id) {
						if (!strncmp(rep_sel->codecs, "avc", 3)) {
							//we accept LHVC with different configs as enhancement for AVC
							if (!strncmp(rep->codecs, "lhv", 3) || !strncmp(rep->codecs, "lhe", 3) ) ok = 1;
							//we accept SVC and MVC as enhancement for AVC
							else if (!strncmp(rep->codecs, "svc", 3) || !strncmp(rep->codecs, "mvc", 3) ) ok = 1;
						}
						else if (!strncmp(rep_sel->codecs, "hvc", 3) || !strncmp(rep_sel->codecs, "hev", 3)) {
							//we accept HEVC and HEVC+LHVC with different configs
							if (!strncmp(rep->codecs, "hvc", 3) || !strncmp(rep->codecs, "hev", 3) ) ok = 1;
							//we accept LHVC with different configs
							else if (!strncmp(rep->codecs, "lhv", 3) || !strncmp(rep->codecs, "lhe", 3) ) ok = 1;
						}
					}

					if (sep) sep[0] = '.';
					if (!ok) {
						GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Different codec types (%s vs %s) in same AdaptationSet - disabling rep %s\n", rep_sel->codecs, rep->codecs, rep->codecs));
						//we don(t support mixes
						rep->playback.disabled = 1;
						continue;
					}
				}
			}
			//move to highest rate if ROUTE session and rep is not a remote one (baseURL set)
			if (dash->route_clock_state && (first_select_mode==GF_DASH_SELECT_BANDWIDTH_LOWEST) && !gf_list_count(rep->base_URLs))
				first_select_mode = GF_DASH_SELECT_BANDWIDTH_HIGHEST;

			switch (first_select_mode) {
			case GF_DASH_SELECT_QUALITY_LOWEST:
				if (rep->quality_ranking && (rep->quality_ranking < rep_sel->quality_ranking)) {
					active_rep = rep_i;
					break;
				}/*fallthrough if quality is not indicated*/
			case GF_DASH_SELECT_BANDWIDTH_LOWEST:
				if ((rep->width&&rep->height) || !group_has_video) {
					if (rep->bandwidth < rep_sel->bandwidth) {
						active_rep = rep_i;
					}
				}
				break;
			case GF_DASH_SELECT_QUALITY_HIGHEST:
				if (rep->quality_ranking > rep_sel->quality_ranking) {
					active_rep = rep_i;
					break;
				}
				/*fallthrough if quality is not indicated*/
			case GF_DASH_SELECT_BANDWIDTH_HIGHEST:
				if (rep->bandwidth > rep_sel->bandwidth) {
					active_rep = rep_i;
				}
				break;
			default:
				break;
			}
		}
		for (rep_i = 0; rep_i < nb_rep; rep_i++) {
			GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, rep_i);
			if (!rep->playback.disabled)
				nb_rep_ok++;
		}

		if (! nb_rep_ok) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] No valid representation in this group - disabling\n"));
			group->selection = GF_DASH_GROUP_NOT_SELECTABLE;
			continue;
		}

		rep_sel = gf_list_get(group->adaptation_set->representations, active_rep);

		gf_dash_set_group_representation(group, rep_sel, GF_FALSE);
		if (group->dash->force_period_reload) {
			gf_dash_reset_groups(dash);
			dash->period_groups_setup = GF_FALSE;
			dash->dash_state = GF_DASH_STATE_SETUP;
			return GF_OK;
		}
		
		// active representation is marked as disabled, we need to redo the selection
		if (rep_sel->playback.disabled)
			goto select_active_rep;

		//adjust seek
		if (dash->start_range_period) {
			gf_dash_seek_group(dash, group, dash->start_range_period, 0);
		}

		mime_type = gf_dash_get_mime_type(NULL, rep_sel, group->adaptation_set);

		if (!mime_type) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Missing MIME type for AdaptationSet - skipping\n"));
			continue;
		}

		/* TODO: Generate segment names if urltemplates are used */
		if (!rep_sel->segment_base && !rep_sel->segment_list && !rep_sel->segment_template
		        && !group->adaptation_set->segment_base && !group->adaptation_set->segment_list && !group->adaptation_set->segment_template
		        && !group->period->segment_base && !group->period->segment_list && !group->period->segment_template
		        && !gf_list_count(rep_sel->base_URLs)
		   ) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Segment URLs are not present for AdaptationSet - skipping\n"));
			continue;
		}

		group->selection = GF_DASH_GROUP_NOT_SELECTED;
		nb_groups_ok++;
	}
	dash->start_range_period = 0;

	//setup SRDs
	for (as_i = 0; as_i<gf_list_count(dash->SRDs); as_i++) {
		u32 cols[10], rows[10];
		struct _dash_srd_desc *srd = gf_list_get(dash->SRDs, as_i);

		srd->srd_nb_rows = srd->srd_nb_cols = 0;

		//sort SRDs
		for (j=1; j < gf_list_count(dash->groups); j++) {
			GF_DASH_Group *dg2 = gf_list_get(dash->groups, j);
			GF_DASH_Group *dg1 = gf_list_get(dash->groups, j-1);
			u32 dg1_weight = dg1->srd_y << 16 | dg1->srd_x;
			u32 dg2_weight = dg2->srd_y << 16 | dg2->srd_x;

			if (dg1->srd_desc != srd) continue;
			if (dg2->srd_desc != srd) continue;

			if (dg1_weight > dg2_weight) {
				gf_list_rem(dash->groups, j);
				gf_list_insert(dash->groups, dg2, j-1);
				dg2->groups_idx = j-1;
				j=0;
			}
		}

		//groups are now sorted for this srd, locate col/row positions
		for (group_i=0; group_i<gf_list_count(dash->groups); group_i++) {
			u32 k;
			Bool found = GF_FALSE;
			GF_DASH_Group *group = gf_list_get(dash->groups, group_i);
			if (group->srd_desc != srd) continue;

			if (!group->srd_w || !group->srd_h) continue;

			for (k=0; k<srd->srd_nb_cols; k++) {
				if (cols[k]==group->srd_x) {
					found=GF_TRUE;
					break;
				}
			}
			if (!found) {
				cols[srd->srd_nb_cols] = group->srd_x;
				group->srd_col_idx = srd->srd_nb_cols;
				srd->srd_nb_cols++;

				srd->width += group->adaptation_set->max_width;

			} else {
				group->srd_col_idx = k;
			}

			found = GF_FALSE;
			for (k=0; k<srd->srd_nb_rows; k++) {
				if (rows[k]==group->srd_y) {
					found=GF_TRUE;
					break;
				}
			}
			if (!found) {
				rows[srd->srd_nb_rows] = group->srd_y;
				group->srd_row_idx = srd->srd_nb_rows;
				srd->srd_nb_rows++;
				srd->height += group->adaptation_set->max_height;
			} else {
				group->srd_row_idx = k;
			}

		}
		gf_dash_set_tiles_quality(dash, srd, GF_TRUE);
	}

	period = gf_list_get(dash->mpd->periods, dash->active_period_index);

	if (period->segment_base) {
		gf_mpd_segment_base_free(period->segment_base);
		period->segment_base = NULL;
	}

	if (!nb_groups_ok) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] No AdaptationSet could be selected in the MPD - Cannot play\n"));
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	/*and seek if needed*/
	return GF_OK;
}


static void gf_dash_group_check_time(GF_DASH_Group *group)
{
	s64 check_time;
	u32 nb_dropped;

	if (group->dash->is_m3u8) return;
	if (! group->timeline_setup) return;
	if (group->broken_timing) return;

	check_time = (s64) gf_net_get_utc();
	nb_dropped = 0;

	while (1) {
		u32 seg_dur_ms;
		u64 seg_ast = gf_dash_get_segment_availability_start_time(group->dash->mpd, group, group->download_segment_index, &seg_dur_ms);

		s64 now = check_time + (s64) seg_dur_ms;
		if (now <= (s64) seg_ast) {
			group->dash->tsb_exceeded = (u32) -1;
			return;
		}

		now -= (s64) seg_ast;
		if (now <= (s64) seg_dur_ms) {
			group->dash->tsb_exceeded = (u32) -1;
			return;
		}
		if (((s32) group->time_shift_buffer_depth > 0) && (now > group->time_shift_buffer_depth)) {
			group->download_segment_index++;
			nb_dropped ++;
			group->dash->time_in_tsb = 0;
			continue;
		}

		if (nb_dropped > group->dash->tsb_exceeded) {
			group->dash->tsb_exceeded = nb_dropped;
		}

		now -= group->dash->user_buffer_ms;
		if (now<0) return;

		if (now>group->dash->time_in_tsb)
			group->dash->time_in_tsb = (u32) now;
		return;
	}
}

typedef enum
{
	GF_DASH_DownloadCancel,
	GF_DASH_DownloadRestart,
	GF_DASH_DownloadSuccess,
} DownloadGroupStatus;


static DownloadGroupStatus dash_download_group_download(GF_DashClient *dash, GF_DASH_Group *group, GF_DASH_Group *base_group, Bool has_dep_following);

static GFINLINE void dash_set_min_wait(GF_DashClient *dash, u32 min_wait)
{
	if (!dash->min_wait_ms_before_next_request || (min_wait < dash->min_wait_ms_before_next_request)) {
		dash->min_wait_ms_before_next_request = min_wait;
		dash->min_wait_sys_clock = gf_sys_clock();
	}
}

/*TODO decide what is the best, fetch from another representation or ignore ...*/
static DownloadGroupStatus on_group_download_error(GF_DashClient *dash, GF_DASH_Group *group, GF_DASH_Group *base_group, GF_Err e, GF_MPD_Representation *rep, char *new_base_seg_url, char *key_url, Bool has_dep_following)
{
	u32 clock_time;
	Bool will_retry = GF_FALSE;
	Bool is_live = GF_FALSE;
    u32 min_wait;
	if (!dash || !group)
		return GF_DASH_DownloadCancel;

	clock_time = gf_sys_clock();

    min_wait = dash->min_timeout_between_404;
    if (dash->route_clock_state) {
        if (!group->period->origin_base_url)
            min_wait = 50; //50 ms between retries if route and not a remote period
    }

    dash_set_min_wait(dash, min_wait);

	group->retry_after_utc = min_wait + gf_net_get_utc();
	if (!group->period->origin_base_url && (dash->mpd->type==GF_MPD_TYPE_DYNAMIC))
		is_live = GF_TRUE;

	if (e==GF_REMOTE_SERVICE_ERROR) {
		gf_dash_mark_group_done(group);
	}
	//failure on last segment in non dynamic mode: likely due to rounding in dash segment duration, assume no error
	//in dynamic mode, we need to check if this is a download schedule issue
	else if (!is_live && group->period->duration && (group->download_segment_index + 1 >= (s32) group->nb_segments_in_rep) ) {
		gf_dash_mark_group_done(group);
	}
	else if (group->maybe_end_of_stream) {
		if (group->maybe_end_of_stream==2) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Couldn't get segment %s (error %s) and end of period was guessed during last update - stopping playback\n", new_base_seg_url, gf_error_to_string(e)));
			group->maybe_end_of_stream = 0;
			gf_dash_mark_group_done(group);
		}
		group->maybe_end_of_stream++;
	} else if (group->segment_in_valid_range) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error in downloading new segment: %s %s - segment was lost at server/proxy side\n", new_base_seg_url, gf_error_to_string(e)));
		if (dash->speed >= 0) {
			group->download_segment_index++;
		} else if (group->download_segment_index) {
			group->download_segment_index--;
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Playing in backward - start of playlist reached - assuming end of stream\n"));
			gf_dash_mark_group_done(group);
		}
		group->current_dep_idx=0;
		group->segment_in_valid_range=0;
    }
    //ROUTE case, the file was removed from cache by the route demuxer
    else if (e==GF_URL_REMOVED) {
        if (dash->speed >= 0) {
            group->download_segment_index++;
        } else if (group->download_segment_index) {
            group->download_segment_index--;
        }
		group->current_dep_idx=0;
	} else if (group->prev_segment_ok && !group->time_at_first_failure && !group->loop_detected) {
        group->time_at_first_failure = clock_time;
        GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Error in downloading new segment: %s %s - starting countdown for %d ms (delay between retry %d ms)\n", new_base_seg_url, gf_error_to_string(e), group->current_downloaded_segment_duration, min_wait));

        will_retry = GF_TRUE;
	}
	//if multiple baseURL, try switching the base
	else if ((e==GF_URL_ERROR) && (group->current_base_url_idx + 1 < gf_mpd_get_base_url_count(dash->mpd, group->period, group->adaptation_set, rep) )) {
		group->current_base_url_idx++;
		if (new_base_seg_url) gf_free(new_base_seg_url);
		if (key_url) gf_free(key_url);

		if (dash->speed>=0) {
			group->download_segment_index--;
		} else {
			group->download_segment_index++;
		}
		return dash_download_group_download(dash, group, base_group, has_dep_following);
	}
	//if previous segment download was OK, we are likely asking too early - retry for the complete duration in case one segment was lost - we add some default safety safety
	else if (group->prev_segment_ok && (clock_time - group->time_at_first_failure <= group->current_downloaded_segment_duration + dash->segment_lost_after_ms )) {
		will_retry = GF_TRUE;
	} else {
		if ((group->dash->route_clock_state==2) && (e==GF_URL_ERROR)) {
			const char *val = group->dash->dash_io->get_header_value(group->dash->dash_io, group->dash->mpd_dnload, "x-route-loop");
			Bool is_loop = (val && !strcmp(val, "yes")) ? GF_TRUE : GF_FALSE;
			//if explicit loop or more than 5 consecutive seg lost restart synchro
			if ((group->nb_consecutive_segments_lost >= 5) || is_loop) {
				u32 i=0;
				if (is_loop) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] ROUTE loop detected, reseting timeline\n"));
				} else {
					GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] ROUTE lost %d consecutive segments, resetup tune-in\n", group->nb_consecutive_segments_lost));
				}
				dash->utc_drift_estimate = 0;
				dash->initial_period_tunein = GF_TRUE;
				dash->route_clock_state = 1;
				while ((group = gf_list_enum(dash->groups, &i))) {
					group->start_number_at_last_ast = 0;
					if (!group->depend_on_group)
						gf_dash_group_timeline_setup(dash->mpd, group, 0);

					group->current_dep_idx=0;
					group->loop_detected = is_loop;
					group->time_at_first_failure = 0;
					group->prev_segment_ok = GF_TRUE;
				}
				if (new_base_seg_url) gf_free(new_base_seg_url);
				if (key_url) gf_free(key_url);
				return GF_DASH_DownloadCancel;
			}
		}
		if (group->prev_segment_ok) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error in downloading new segment %s: %s - waited %d ms but segment still not available, checking next one ...\n", new_base_seg_url, gf_error_to_string(e), clock_time - group->time_at_first_failure));
			group->time_at_first_failure = 0;
			//for route we still consider the previous segment valid and don't attempt to resync the timeline
			if (!group->dash->route_clock_state) {
				group->prev_segment_ok = GF_FALSE;

				if ((dash->mpd->type==GF_MPD_TYPE_STATIC) && (dash->chaining_mode==2) && dash->chain_fallback) {
					dash->dash_state = GF_DASH_STATE_CHAIN_FALLBACK;
				}
			}
		}
		group->nb_consecutive_segments_lost ++;

		//we are lost ....
		if (group->nb_consecutive_segments_lost == 10) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Too many consecutive segments not found, sync or signal has been lost - entering end of stream detection mode\n"));
			dash_set_min_wait(dash, 1000);
			group->maybe_end_of_stream = 1;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error in downloading new segment %s: %s\n", new_base_seg_url, gf_error_to_string(e)));
			if (dash->speed >= 0) {
				group->download_segment_index++;
			} else if (group->download_segment_index) {
				group->download_segment_index--;
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Playing in backward - start of playlist reached - assuming end of stream\n"));
				gf_dash_mark_group_done(group);
			}
			group->current_dep_idx=0;
		}
	}
	//if retry, do not reset dependency status
	if (!will_retry) {
		if (rep->dependency_id) {
			segment_cache_entry *cache_entry = &base_group->cached[base_group->nb_cached_segments];
			cache_entry->flags &= ~SEG_FLAG_DEP_FOLLOWING;
		}

		if (group->base_rep_index_plus_one) {
			group->active_rep_index = group->base_rep_index_plus_one - 1;
			group->has_pending_enhancement = GF_FALSE;
		}
	} else {
		if (dash->speed>=0) {
			group->download_segment_index--;
		} else {
			group->download_segment_index++;
		}
	}

	if (new_base_seg_url) gf_free(new_base_seg_url);
	if (key_url) gf_free(key_url);
	return GF_DASH_DownloadCancel;
}

static DownloadGroupStatus dash_download_group_download(GF_DashClient *dash, GF_DASH_Group *group, GF_DASH_Group *base_group, Bool has_dep_following)
{
	//commented out as we end up doing too many requets
	GF_Err e;
	GF_MPD_Representation *rep;
	char *new_base_seg_url=NULL;
	char *key_url=NULL;
	bin128 key_iv;
	u64 start_range, end_range;
	Bool use_byterange;
	u32 llhls_live_edge_type=0;
	u32 representation_index;
	u32 clock_time;
	Bool remote_file = GF_FALSE;
	const char *base_url = NULL;
	u32 start_number=0;
	u64 seg_dur;
	u32 seg_scale;
	segment_cache_entry *cache_entry;

	GF_MPD_Type dyn_type = dash->mpd->type;
	if (group->period->origin_base_url)
		dyn_type = group->period->type;

	if (group->done)
		return GF_DASH_DownloadSuccess;
	if (!base_group)
		return GF_DASH_DownloadSuccess;

	//we are waiting for the playlist to be updated to find the next segment to play
	if (group->hls_next_seq_num) {
		return GF_DASH_DownloadCancel;
	}

	if (group->selection != GF_DASH_GROUP_SELECTED) return GF_DASH_DownloadSuccess;

	if (base_group->nb_cached_segments>=base_group->max_cached_segments) {
		return GF_DASH_DownloadCancel;
	}
	if (!group->timeline_setup) {
		gf_dash_group_timeline_setup(dash->mpd, group, 0);
		group->timeline_setup = GF_TRUE;
	}

	/*remember the active rep index, since group->active_rep_index may change because of bandwidth control algorithm*/
	representation_index = group->active_rep_index;
	rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);
	rep->playback.broadcast_flag = GF_FALSE;

llhls_rety:
	//special case for LL-HLS: if we have a switch request pending, check if next fragment is the first of a new seg
	//or a complete seg (we do not switch in the middle of a segment)
	if (group->llhls_switch_request>=0) {
		GF_MPD_SegmentURL *hlsseg = gf_list_get(rep->segment_list->segment_URLs, group->download_segment_index);
		if (hlsseg && (! hlsseg->hls_ll_chunk_type || hlsseg->is_first_part)) {
			rep = gf_list_get(group->adaptation_set->representations, group->llhls_switch_request);
			group->llhls_edge_chunk = NULL;
			gf_dash_set_group_representation(group, rep, GF_TRUE);
			assert(group->llhls_switch_request<0);
			//we are waiting for playlist update, return
			if (group->hls_next_seq_num) {
				return GF_DASH_DownloadCancel;
			}
			//otherwise new rep is set
			representation_index = group->active_rep_index;
			rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);
			rep->playback.broadcast_flag = GF_FALSE;
		}
	}

	/* if the index of the segment to be downloaded is greater or equal to the last segment (as seen in the playlist),
	 we need to check if a new playlist is ready */
	if (group->nb_segments_in_rep && (group->download_segment_index >= (s32) group->nb_segments_in_rep)) {
		u32 timer = gf_sys_clock() - dash->last_update_time;
		Bool update_playlist = 0;

		/* this period is done*/
		if ((dyn_type==GF_MPD_TYPE_DYNAMIC) && group->period->duration) {
		}
		/* update of the playlist, only if indicated */
		else if (dash->mpd->minimum_update_period && (timer > dash->mpd->minimum_update_period)) {
			update_playlist = 1;
		}
		/* if media_presentation_duration is 0 and we are in live, force a refresh (not in the spec but safety check*/
		else if ((dyn_type==GF_MPD_TYPE_DYNAMIC) && !dash->mpd->media_presentation_duration) {
			if (group->segment_duration && (timer > group->segment_duration*1000))
				update_playlist = 1;
		}
		if (update_playlist) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Playlist should be updated, postponing group download until playlist is updated\n"));
			dash->force_mpd_update = GF_TRUE;
			return GF_DASH_DownloadCancel;
		}
		/* Now that the playlist is up to date, we can check again */
		if (group->download_segment_index  >= (s32) group->nb_segments_in_rep) {
			/* if there is a specified update period, we redo the whole process */
			if (dash->mpd->minimum_update_period || dyn_type==GF_MPD_TYPE_DYNAMIC) {

				if (dyn_type==GF_MPD_TYPE_STATIC) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Last segment in static period (dynamic MPD) - group is done\n"));
					gf_dash_mark_group_done(group);
					return GF_DASH_DownloadCancel;
				}
				else if ((dyn_type==GF_MPD_TYPE_DYNAMIC) && group->period->duration) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Last segment in period (dynamic mode) - group is done\n"));
					gf_dash_mark_group_done(group);
					return GF_DASH_DownloadCancel;
				}
				else if (! group->maybe_end_of_stream) {
					u32 now = gf_sys_clock();
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] End of segment list reached (%d segments but idx is %d), waiting for next MPD update\n", group->nb_segments_in_rep, group->download_segment_index));

					if (group->nb_cached_segments) {
						return GF_DASH_DownloadCancel;
					}

					if (dash->is_m3u8 && (dyn_type==GF_MPD_TYPE_DYNAMIC)) {
						if (!group->time_at_first_reload_required)
							group->time_at_first_reload_required = now;

						//use group last modification time
						timer = now - group->last_mpd_change_time;
						if (timer < group->segment_duration * 2000) {
							//no more segment, force a manifest update now
							dash->force_mpd_update = GF_TRUE;
						} else {
							GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] HLS Segment list has not been updated for more than %d ms - assuming end of session\n", now - group->time_at_first_reload_required));
							gf_dash_mark_group_done(group);
						}
						return GF_DASH_DownloadCancel;
					}

					if (!group->time_at_first_reload_required) {
						group->time_at_first_reload_required = now;
						return GF_DASH_DownloadCancel;
					}
					if (now - group->time_at_first_reload_required < group->cache_duration)
						return GF_DASH_DownloadCancel;
					if (dash->mpd->minimum_update_period) {
						if (now - group->time_at_first_reload_required < dash->mpd->minimum_update_period)
							return GF_DASH_DownloadCancel;
					} else if (dyn_type==GF_MPD_TYPE_DYNAMIC) {
						//use group last modification time
						timer = now - group->last_mpd_change_time;
						if (timer < 2 * group->segment_duration * 2000)
							return GF_DASH_DownloadCancel;
					}

					GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Segment list has not been updated for more than %d ms - assuming end of period\n", now - group->time_at_first_reload_required));
					gf_dash_mark_group_done(group);
					return GF_DASH_DownloadCancel;
				}
			} else {
				/* if not, we are really at the end of the playlist, we can quit */
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] End of period reached for group\n"));
				gf_dash_mark_group_done(group);
				if (!dash->request_period_switch && !group->has_pending_enhancement && !has_dep_following) {
					dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_SEGMENT_AVAILABLE, base_group->groups_idx, GF_OK);
				}
				return GF_DASH_DownloadCancel;
			}
		}
	}
	group->time_at_first_reload_required = 0;

	if (group->force_switch_bandwidth && !dash->auto_switch_count) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Forcing representation switch, retesting group"));
		gf_dash_switch_group_representation(dash, group);
		/*restart*/
		return GF_DASH_DownloadRestart;
	}

	/*check availablity start time of segment in Live !!*/
	if (!group->broken_timing && (dyn_type==GF_MPD_TYPE_DYNAMIC) && !dash->is_m3u8 && !dash->is_smooth) {
		s32 to_wait = 0;
		u32 seg_dur_ms=0;
		s64 segment_ast = (s64) gf_dash_get_segment_availability_start_time(dash->mpd, group, group->download_segment_index, &seg_dur_ms);
		s64 now = (s64) gf_net_get_utc();
#ifndef GPAC_DISABLE_LOG
		start_number = gf_dash_get_start_number(group, rep);
#endif


		if (group->retry_after_utc > (u64) now) {
			to_wait = (u32) (group->retry_after_utc - (u64) now);
			dash_set_min_wait(dash, (u32) to_wait);

			return GF_DASH_DownloadCancel;
		}

		clock_time = gf_sys_clock();
		to_wait = (s32) (segment_ast - now);

		if (group->force_early_fetch) {
			if (to_wait>1) {
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Set #%d demux empty but wait time for segment %d is still %d ms, forcing scheduling\n", 1+group->groups_idx, group->download_segment_index + start_number, to_wait));
				to_wait = 0;
			} else {
				//we officially reached segment AST
				group->force_early_fetch = GF_FALSE;
			}
		}

		/*if segment AST is greater than now, it is not yet available - we would need an estimate on how long the request takes to be sent to the server in order to be more reactive ...*/
		if (to_wait > 1) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Set #%d At %d Next segment %d (AST "LLD" - sec in period %g) is not yet available on server - requesting later in %d ms\n", 1+group->groups_idx, gf_sys_clock(), group->download_segment_index + start_number, segment_ast, (segment_ast - group->period->start - group->ast_at_init + group->ast_offset)/1000.0, to_wait));
			if (group->last_segment_time) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] %d ms elapsed since previous segment download\n", clock_time - group->last_segment_time));
			}

			dash_set_min_wait(dash, (u32) to_wait);

			return GF_DASH_DownloadCancel;
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Set #%d At %d Next segment %d (AST "LLD" - sec in period %g) should now be available on server since %d ms - requesting it\n", 1+group->groups_idx, gf_sys_clock(), group->download_segment_index + start_number, segment_ast, (segment_ast - group->period->start - group->ast_at_init + group->ast_offset)/1000.0, -to_wait));

			if (group->last_segment_time) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] %d ms elapsed since previous segment download\n", clock_time - group->last_segment_time));
			}
		}
        group->time_at_last_request = gf_sys_clock();
    }

	base_url = dash->base_url;
	if (group->period->origin_base_url) base_url = group->period->origin_base_url;
	/* At this stage, there are some segments left to be downloaded */
	e = gf_dash_resolve_url(dash->mpd, rep, group, base_url, GF_MPD_RESOLVE_URL_MEDIA, group->download_segment_index, &new_base_seg_url, &start_range, &end_range, &group->current_downloaded_segment_duration, NULL, &key_url, &key_iv, NULL, &start_number);


	if ((e==GF_EOS)	&& group->llhls_edge_chunk && group->llhls_edge_chunk->hls_ll_chunk_type) {
		//no more segments, force update now
		dash->force_mpd_update = GF_TRUE;
		return GF_DASH_DownloadCancel;
	}

	if (e || !new_base_seg_url) {
		if (e==GF_EOS) {
			gf_dash_mark_group_done(group);
		} else {
			/*do something!!*/
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error resolving URL of next segment: %s\n", gf_error_to_string(e) ));
		}
		if (new_base_seg_url) gf_free(new_base_seg_url);
		if (key_url) gf_free(key_url);
		group->llhls_edge_chunk = NULL;
		return GF_DASH_DownloadCancel;
	}

	if (dash->is_m3u8 && group->is_low_latency) {
		assert(rep->segment_list);
		GF_MPD_SegmentURL *hlsseg = gf_list_get(rep->segment_list->segment_URLs, group->download_segment_index);
		assert(hlsseg);

		if (dash->llhls_single_range && hlsseg->media_range && (hlsseg->can_merge || group->llhls_last_was_merged) ) {
			//if not very first request (tune in) and not first part of seg, if mergeable issue a single byterange
			if (!group->first_hls_chunk && hlsseg->media_range->start_range) {
				if (!hlsseg->can_merge) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] LL-HLS part cannot be merged with previously open byte-range request, disabling merging !\n"));
					dash->llhls_single_range = GF_FALSE;
				}
				group->download_segment_index++;
				if (new_base_seg_url) gf_free(new_base_seg_url);
				new_base_seg_url = NULL;
				if (key_url) gf_free(key_url);
				key_url = NULL;
				goto llhls_rety;
			}
			group->first_hls_chunk = GF_FALSE;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Changing LL-HLS request %s @ "LLU"->"LLU" to open end range\n", new_base_seg_url, start_range, end_range));
			end_range = (u64) -1;
			llhls_live_edge_type = 2;
			group->llhls_last_was_merged = GF_TRUE;
		} else {
			group->llhls_last_was_merged = GF_FALSE;
			if (hlsseg->hls_ll_chunk_type)
				llhls_live_edge_type = 1;
		}
		group->llhls_edge_chunk = hlsseg;
	}
	use_byterange = (start_range || end_range) ? 1 : 0;

#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_DASH, GF_LOG_INFO)) {
		if (llhls_live_edge_type==2) {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Queing next segment: %s (live edge merged range: "LLU" -> END)\n", gf_file_basename(new_base_seg_url), start_range));
		} else if (use_byterange) {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Queing next %s: %s (range: "LLU" -> "LLU")\n", (llhls_live_edge_type==1) ? "LL-HLS part" : "segment",  gf_file_basename(new_base_seg_url), start_range, end_range));
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Queing next %s: %s\n", (llhls_live_edge_type==1) ? "LL-HLS part" : "segment", gf_file_basename(new_base_seg_url)));
		}
	}
#endif

	/*local file*/
	if (strnicmp(base_url, "gfio://", 7)
		&& (!strstr(new_base_seg_url, "://") || (!strnicmp(new_base_seg_url, "file://", 7) || !strnicmp(new_base_seg_url, "gmem://", 7) ) )
	) {
		e = GF_OK;
		/*do not erase local files*/
		group->local_files = 1;
		if (group->force_switch_bandwidth && !dash->auto_switch_count) {
			if (new_base_seg_url) gf_free(new_base_seg_url);
			gf_dash_switch_group_representation(dash, group);
			/*restart*/
			return GF_DASH_DownloadRestart;
		}
		if (! gf_file_exists(new_base_seg_url)) {
			if (group->current_base_url_idx + 1 < gf_mpd_get_base_url_count(dash->mpd, group->period, group->adaptation_set, rep) ){
				group->current_base_url_idx++;
				if (new_base_seg_url) gf_free(new_base_seg_url);
				if (key_url) gf_free(key_url);

				return dash_download_group_download(dash, group, base_group, has_dep_following);
			} else if (group->period->duration && (group->download_segment_index + 1 == group->nb_segments_in_rep) ) {
				if (new_base_seg_url) gf_free(new_base_seg_url);
				gf_dash_mark_group_done(group);
				return GF_DASH_DownloadCancel;
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] File %s not found on disk\n", new_base_seg_url));
				group->current_base_url_idx = 0;
				//increment seg index before calling download_error
				if (dash->speed >= 0) {
					group->download_segment_index++;
				} else if (group->download_segment_index) {
					group->download_segment_index--;
				}
				return on_group_download_error(dash, group, base_group, GF_NOT_FOUND, rep, new_base_seg_url, key_url, has_dep_following);
			}
		}
		group->current_base_url_idx = 0;
	} else {
		remote_file = GF_TRUE;
	}


	assert(base_group->nb_cached_segments<base_group->max_cached_segments);
	cache_entry = &base_group->cached[base_group->nb_cached_segments];

	//assign url
	cache_entry->url = new_base_seg_url;
	if (use_byterange && remote_file) {
		cache_entry->start_range = start_range;
		cache_entry->end_range = end_range;
	} else {
		cache_entry->start_range = 0;
		cache_entry->end_range = 0;
	}
	cache_entry->representation_index = representation_index;
	cache_entry->duration = (u32) group->current_downloaded_segment_duration;
	cache_entry->flags = group->loop_detected ? SEG_FLAG_LOOP_DETECTED : 0;
	cache_entry->dep_group_idx = group->depend_on_group ? group->depend_on_group->current_dep_idx : group->current_dep_idx;
	if (has_dep_following) cache_entry->flags |= SEG_FLAG_DEP_FOLLOWING;
	if (group->disabled)
		cache_entry->flags |= SEG_FLAG_DISABLED;
	if (key_url) {
		cache_entry->key_url = key_url;
		memcpy(cache_entry->key_IV, key_iv, sizeof(bin128));
		//set to NULL since we stored it, so that it won't be freed when exiting this function
		key_url = NULL;
	}

	cache_entry->time.num = gf_dash_get_segment_start_time_with_timescale(group, &seg_dur, &seg_scale);
	cache_entry->time.den = seg_scale;

	cache_entry->seg_number = group->download_segment_index + start_number;
	cache_entry->seg_name_start = dash_strip_base_url(cache_entry->url, base_url);
	group->loop_detected = GF_FALSE;

	if (group->local_files && use_byterange) {
		cache_entry->start_range = start_range;
		cache_entry->end_range = end_range;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Added file to cache (%u/%u in cache): %s\n", base_group->nb_cached_segments+1, base_group->max_cached_segments, cache_entry->url));

	base_group->nb_cached_segments++;

	/* download enhancement representation of this segment*/
	if ((representation_index != group->max_complementary_rep_index) && rep->playback.enhancement_rep_index_plus_one) {
		group->active_rep_index = rep->playback.enhancement_rep_index_plus_one - 1;
		group->has_pending_enhancement = GF_TRUE;
	}
	/* if we have downloaded all enhancement representations of this segment, restart from base representation and increase downloaded segment index by 1*/
	else {
		if (group->base_rep_index_plus_one) group->active_rep_index = group->base_rep_index_plus_one - 1;
		if (dash->speed >= 0) {
			group->download_segment_index++;
		} else if (group->download_segment_index) {
			group->download_segment_index--;
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Playing in backward - start of playlist reached - assuming end of stream\n"));
			gf_dash_mark_group_done(group);
		}
		group->has_pending_enhancement = GF_FALSE;
	}
	if (dash->auto_switch_count) {
		if (group->llhls_edge_chunk && group->llhls_edge_chunk->hls_ll_chunk_type) {
			if (group->llhls_edge_chunk->is_first_part)
				group->nb_segments_done++;
		} else {
			group->nb_segments_done++;
		}
		if (group->nb_segments_done==dash->auto_switch_count) {
			group->nb_segments_done=0;
			gf_dash_skip_disabled_representation(group, rep, GF_TRUE);
		}
	}

	//do not notify segments if there is a pending period switch - since these are decided by the user, we don't
	//want to notify old segments
	if (!dash->request_period_switch && !group->has_pending_enhancement && !has_dep_following) {
		dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_SEGMENT_AVAILABLE, base_group->groups_idx, GF_OK);
	}

	//do NOT free new_base_seg_url, it is now in cache_entry->url
	if (key_url) gf_free(key_url);
	if (e) return GF_DASH_DownloadCancel;
	return GF_DASH_DownloadSuccess;
}


static DownloadGroupStatus dash_download_group(GF_DashClient *dash, GF_DASH_Group *group, GF_DASH_Group *base_group, Bool has_dep_following)
{
	DownloadGroupStatus res;

	if (!group->current_dep_idx) {
		res = dash_download_group_download(dash, group, base_group, has_dep_following);
		if (res==GF_DASH_DownloadRestart) return res;
		if (res==GF_DASH_DownloadCancel) return res;
		group->current_dep_idx = 1;
	}

	if (group->groups_depending_on) {
		u32 i, count = gf_list_count(group->groups_depending_on);
		i = group->current_dep_idx - 1;
		for (; i<count; i++) {

			GF_DASH_Group *dep_group = gf_list_get(group->groups_depending_on, i);
			if ((i+1==count) && !dep_group->groups_depending_on)
				has_dep_following = GF_FALSE;

			group->current_dep_idx = i + 1;
			res = dash_download_group(dash, dep_group, base_group, has_dep_following);
			if (res==GF_DASH_DownloadRestart) {
				i--;
				continue;
			}
			if (res==GF_DASH_DownloadCancel)
				return GF_DASH_DownloadCancel;
		}
	}
	group->current_dep_idx = 0;
	return GF_DASH_DownloadSuccess;
}

//tile based adaptation
static void dash_global_rate_adaptation(GF_DashClient *dash, Bool for_postponed_only)
{
	u32 quality_rank;
	u32 min_bandwidth = 0;
	Bool force_rep_idx = GF_FALSE;
	Bool local_file_mode = GF_FALSE;
	Bool use_custom_algo = GF_FALSE;
	GF_MPD_Representation *rep, *rep_new;
	u32 total_rate, bandwidths[20], groups_per_quality[20], max_level;
	u32 q_idx, nb_qualities = 0;
	u32 i, count = gf_list_count(dash->groups), local_files = 0;
	u64 cumulated_rate=0, cumulated_size=0;

	//initialize min/max bandwidth
	min_bandwidth = 0;
	max_level = 0;
	total_rate = (u32) -1;
	nb_qualities = 1;

	//get max qualities due to SRD descriptions
	//for now, consider all non-SRDs group to run in max quality
	for (i=0; i<gf_list_count(dash->SRDs); i++) {
		struct _dash_srd_desc *srd = gf_list_get(dash->SRDs, i);
		u32 nb_q = MAX(srd->srd_nb_cols, srd->srd_nb_rows);
		if (nb_q > nb_qualities) nb_qualities = nb_q;
	}

	//estimate bitrate
	for (i=0; i<count; i++) {
		GF_DASH_Group *group = gf_list_get(dash->groups, i);
		if (group->selection != GF_DASH_GROUP_SELECTED) continue;
		if (group->local_files) local_files ++;
		if (!group->bytes_per_sec) {
			if (!for_postponed_only && !group->disabled)
				return;
			continue;
		}
		if (group->done) continue;

		//change of tile qualities
		if (group->update_tile_qualities) {
			group->update_tile_qualities = GF_FALSE;
			if (!dash->rate_adaptation_algo_custom) {
				if (group->srd_desc)
					gf_dash_set_tiles_quality(dash, group->srd_desc, GF_FALSE);
			}
		}


		group->backup_Bps = group->bytes_per_sec;
		//only count broadband ones
		if (dash->route_clock_state && !gf_list_count(group->period->base_URLs) && !gf_list_count(group->adaptation_set->base_URLs) && !group->period->origin_base_url) {
			u32 j;
			//get all active rep, count bandwidth for broadband ones
			for (j=0; j<=group->max_complementary_rep_index; j++) {
				rep = gf_list_get(group->adaptation_set->representations, j);
				//this rep is not in broadcast, add bandwidth
				if (gf_list_count(rep->base_URLs)) {
					cumulated_rate += ((u64) group->bytes_per_sec) * group->total_size;
					cumulated_size += group->total_size;
				}
			}
		} else {
			//weighted everage of download rates vs file size - using the min will not work for very small files
			//for which the rate estimation may be low due to round-trip request time
			//TODO we should do the weighted average per base URL for finer results
			cumulated_rate += ((u64) group->bytes_per_sec) * group->total_size;
			cumulated_size += group->total_size;
		}
	}
	if (cumulated_size) {
		total_rate = (u32) (cumulated_rate / cumulated_size);
	}

	if (total_rate == (u32) -1) {
		total_rate = 0;
	}
	if (local_files==count) {
		total_rate = dash->dash_io->get_bytes_per_sec ? dash->dash_io->get_bytes_per_sec(dash->dash_io, NULL) : 0;
		if (!total_rate) local_file_mode = GF_TRUE;
	} else if (!total_rate) {
		return;
	}

	if (dash->rate_adaptation_algo_custom) {
		use_custom_algo = GF_TRUE;
		dash->total_rate = total_rate;
		goto custom_algo;
	}

	for (q_idx=0; q_idx<nb_qualities; q_idx++) {
		bandwidths[q_idx] = 0;
		groups_per_quality[q_idx] = 0;

		for (i=0; i<count; i++) {
			GF_DASH_Group *group = gf_list_get(dash->groups, i);
			if (group->selection != GF_DASH_GROUP_SELECTED) continue;
			if (group->done) continue;

			quality_rank = gf_dash_get_tiles_quality_rank(dash, group);
			if (quality_rank >= nb_qualities) quality_rank = nb_qualities-1;
			if (quality_rank != q_idx) continue;

			group->target_new_rep = 0;
			rep = gf_list_get(group->adaptation_set->representations, group->target_new_rep);
			bandwidths[q_idx] += rep->bandwidth;
			groups_per_quality[q_idx] ++;
			if (max_level < 1 + quality_rank) max_level = 1+quality_rank;

			//quick trick here: if no download cap in local playback, compute the total rate based on
			//quality rank
			if (local_file_mode) {
				u32 nb_reps = gf_list_count(group->adaptation_set->representations);
				//get rep matching the given quality rank - quality 0 is the highest and our
				//reps are sorted from lowest !
				u32 rep_target;
				if (!quality_rank)
					rep_target = nb_reps-1;
				else
					rep_target = (nb_qualities - quality_rank) * nb_reps / nb_qualities;

				rep = gf_list_get(group->adaptation_set->representations, rep_target);
				//in bits per sec!
				total_rate += rep->bandwidth;
			}
		}
		min_bandwidth += bandwidths[q_idx];
	}
	if (local_file_mode) {
		//total rate is in bytes per second, and add a safety of 10 bytes/s to ensure selection
		total_rate = 10 + total_rate / 8;
	}

	/*no per-quality adaptation, we may have oscillations*/
	if (!dash->tile_rate_decrease) {
	}
	/*automatic rate alloc*/
	else if (dash->tile_rate_decrease==100) {
		//for each quality level (starting from highest priority), increase the bitrate if possible
		for (q_idx=0; q_idx < max_level; q_idx++) {
			Bool test_pass = GF_TRUE;
			while (1) {
				u32 nb_rep_increased = 0;
				u32 nb_rep_in_qidx = 0;
				u32 cumulated_bw_in_pass = 0;

				for (i=0; i<count; i++) {
					u32 diff;
					GF_DASH_Group *group = gf_list_get(dash->groups, i);
					if (group->selection != GF_DASH_GROUP_SELECTED) continue;
					if (group->done) continue;

					quality_rank = gf_dash_get_tiles_quality_rank(dash, group);
					if (quality_rank >= nb_qualities) quality_rank = nb_qualities-1;
					if (quality_rank != q_idx) continue;

					if (group->target_new_rep + 1 == gf_list_count(group->adaptation_set->representations))
						continue;

					nb_rep_in_qidx++;

					rep = gf_list_get(group->adaptation_set->representations, group->target_new_rep);
					diff = rep->bandwidth;
					rep_new = gf_list_get(group->adaptation_set->representations, group->target_new_rep+1);
					diff = rep_new->bandwidth - diff;

					if (dash->route_clock_state) {
						//if baseURL in period or adaptation set, we assume we are in broadband mode, otherwise we re in broadcast, don't count bitrate
						if (!gf_list_count(group->period->base_URLs) && !gf_list_count(group->adaptation_set->base_URLs)) {
							//new rep is in broadcast, force diff to 0 to select the rep
							if (!gf_list_count(rep_new->base_URLs)) {
								diff = 0;
							}
							//new rep is in broadband, prev rep is in broadcast, diff is the new rep bandwidth
							else if (!gf_list_count(rep->base_URLs)) {
								diff = rep_new->bandwidth;
							}
						}
					}

					if (test_pass) {
						cumulated_bw_in_pass+= diff;
						nb_rep_increased ++;
					} else if (min_bandwidth + diff < 8*total_rate) {
						min_bandwidth += diff;
						nb_rep_increased ++;
						bandwidths[q_idx] += diff;
						group->target_new_rep++;
					}
				}
				if (test_pass) {
					//all reps cannot be switched up in this quality level, do it
					if ( min_bandwidth + cumulated_bw_in_pass > 8*total_rate) {
						break;
					}
				}
				//no more adjustement possible for this quality level
				if (! nb_rep_increased)
					break;

				test_pass = !test_pass;
			}
		}

		if (! for_postponed_only) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Rate Adaptation - download rate %d kbps - %d quality levels (cumulated representations rate %d kbps)\n", 8*total_rate/1000, max_level, min_bandwidth/1000));

			for (q_idx=0; q_idx<max_level; q_idx++) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH]\tLevel #%d - %d Adaptation Sets for a total %d kbps allocated\n", q_idx+1, groups_per_quality[q_idx], bandwidths[q_idx]/1000 ));
			}
		}

		force_rep_idx = GF_TRUE;
	}
	/*for each quality level get tile_rate_decrease% of the available bandwidth*/
	else {
		u32 rate = bandwidths[0] = total_rate * dash->tile_rate_decrease / 100;
		for (i=1; i<max_level; i++) {
			u32 remain = total_rate - rate;
			if (i+1==max_level) {
				bandwidths[i] = remain;
			} else {
				bandwidths[i] = remain * dash->tile_rate_decrease/100;
				rate += bandwidths[i];
			}
		}

		if (! for_postponed_only) {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Rate Adaptation - download rate %d kbps - %d quality levels (cumulated rate %d kbps)\n", 8*total_rate/1000, max_level, 8*min_bandwidth/1000));
			for (q_idx=0; q_idx<max_level; q_idx++) {
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH]\tLevel #%d - %d Adaptation Sets for a total %d kbps allocated\n", q_idx+1, groups_per_quality[q_idx], 8*bandwidths[q_idx]/1000 ));
			}
		}
	}

custom_algo:

	//GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("DEBUG. 2. dowload at %d \n", 8*bandwidths[q_idx]/1000));
	//bandwitdh sharing done, perform rate adaptation with theses new numbers
	for (i=0; i<count; i++) {
		GF_DASH_Group *group = gf_list_get(dash->groups, i);
		if (group->selection != GF_DASH_GROUP_SELECTED) continue;
		if (group->done) continue;

		//in custom algo case, we don't change the bitrate of the group
		if (!use_custom_algo) {
			if (force_rep_idx) {
				rep = gf_list_get(group->adaptation_set->representations, group->target_new_rep);
				//add 100 bytes/sec to make sure we select the target one
				group->bytes_per_sec = 100 + rep->bandwidth / 8;
			}
			//decrease by quality level
			else if (dash->tile_rate_decrease) {
				quality_rank = gf_dash_get_tiles_quality_rank(dash, group);
				if (quality_rank >= nb_qualities) quality_rank = nb_qualities-1;
				assert(groups_per_quality[quality_rank]);
				group->bytes_per_sec = bandwidths[quality_rank] / groups_per_quality[quality_rank];
			}
		}

		if (for_postponed_only) {
			if (group->rate_adaptation_postponed)
				dash_do_rate_adaptation(dash, group);
			group->bytes_per_sec = group->backup_Bps;
		} else {
			dash_do_rate_adaptation(dash, group);
			//reset/restore bytes_per_sec once all groups have been called
		}
	}

	if (!for_postponed_only) {
		for (i=0; i<count; i++) {
			GF_DASH_Group *group = gf_list_get(dash->groups, i);
			if (group->selection != GF_DASH_GROUP_SELECTED) continue;
			if (group->done) continue;

			if (!group->rate_adaptation_postponed)
				group->bytes_per_sec = 0;
			else
				group->bytes_per_sec = group->backup_Bps;
		}
	}
}




static GF_Err dash_setup_period_and_groups(GF_DashClient *dash)
{
	u32 i, group_count;
	GF_Err e;

	//don't resetup the entire period, only the broken group(s) ...
	if (!dash->period_groups_setup) {
		/*setup period*/
		e = gf_dash_setup_period(dash);
		if (e) {
			//move to stop state before sending the error event otherwise we might deadlock when disconnecting the dash client
			dash->dash_state = GF_DASH_STATE_STOPPED;
			dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_PERIOD_SETUP_ERROR, -1, e);
			return e;
		}
		if (dash->force_period_reload) return GF_OK;

		dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_SELECT_GROUPS, -1, GF_OK);

		dash->period_groups_setup = GF_TRUE;
		dash->all_groups_done_notified = GF_FALSE;
	}

	e = GF_OK;
	group_count = gf_list_count(dash->groups);
	for (i=0; i<group_count; i++) {
		GF_DASH_Group *group = gf_list_get(dash->groups, i);
		if (group->selection==GF_DASH_GROUP_NOT_SELECTABLE)
			continue;

		if (group->group_setup) continue;

		e = gf_dash_download_init_segment(dash, group);

		//might happen with broadcast DASH (eg ATSC3)
		if (e == GF_IP_NETWORK_EMPTY) {
			if (dash->reinit_period_index) {
				gf_dash_reset_groups(dash);
				dash->active_period_index = dash->reinit_period_index-1;
				dash->reinit_period_index = 0;
				dash->period_groups_setup = GF_FALSE;
				return dash_setup_period_and_groups(dash);
			}

			return e;
		}
		group->group_setup = GF_TRUE;
		if (dash->initial_period_tunein && !dash->route_clock_state) {
			group->timeline_setup = GF_FALSE;
			group->force_timeline_reeval = GF_TRUE;
		}
		if (e) break;
	}
	dash->initial_period_tunein = GF_FALSE;

	/*if error signal to the user*/
	if (e != GF_OK) {
		//move to stop state before sending the error event otherwise we might deadlock when disconnecting the dash client
		dash->dash_state = GF_DASH_STATE_STOPPED;
		dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_PERIOD_SETUP_ERROR, -1, e);
		return e;
	}

	return GF_OK;
}

static void dash_do_groups(GF_DashClient *dash)
{
	u32 i, group_count = gf_list_count(dash->groups);

	dash->min_wait_ms_before_next_request = 0;

	/*for each selected groups*/
	for (i=0; i<group_count; i++) {
		GF_DASH_Group *group = gf_list_get(dash->groups, i);
		if (group->selection != GF_DASH_GROUP_SELECTED) {
			if (group->nb_cached_segments) {
				gf_dash_group_reset(dash, group);
			}
			continue;
		}

		if (group->depend_on_group) continue;
		//not yet scheduled for download
		if (group->rate_adaptation_postponed) {
			continue;
		}

		DownloadGroupStatus res;
		res = dash_download_group(dash, group, group, group->groups_depending_on ? GF_TRUE : GF_FALSE);
		if (res==GF_DASH_DownloadRestart) {
			i--;
			continue;
		}
	}
}

static GF_Err dash_check_mpd_update_and_cache(GF_DashClient *dash, Bool *cache_is_full)
{
	GF_Err e = GF_OK;
	u32 i, group_count;
	u32 now = gf_sys_clock();
	u32 timer = now - dash->last_update_time;
	Bool has_postponed_rate_adaptation;

	(*cache_is_full) = GF_TRUE;
	has_postponed_rate_adaptation = GF_FALSE;

	group_count = gf_list_count(dash->groups);

	/*refresh MPD*/
	if (dash->force_mpd_update
		//regular MPD update
		|| (dash->mpd->minimum_update_period && (timer > dash->mpd->minimum_update_period))
		//pending HLS playlist refresh
		|| (dash->hls_reload_time && (now > dash->hls_reload_time))
	) {
		if (dash->force_mpd_update) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Forcing playlist refresh (last segment reached)\n"));
		} else if (dash->mpd->minimum_update_period) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Update the playlist (%u ms elapsed since last refresh / min reload rate %u ms)\n", gf_sys_clock() , timer, dash->mpd->minimum_update_period));
		}
		dash->force_mpd_update = GF_FALSE;
		dash->hls_reload_time = 0;
		e = gf_dash_update_manifest(dash);
		if (e) {
			if (!dash->in_error) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error updating MPD %s\n", gf_error_to_string(e)));
			}
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Updated MPD in %d ms\n", gf_sys_clock() - now));
		}
		(*cache_is_full) = GF_FALSE;
	} else {
		Bool all_groups_done = GF_TRUE;
		Bool cache_full = GF_TRUE;

		has_postponed_rate_adaptation = GF_FALSE;

		/*wait if nothing is ready to be downloaded*/
		if (dash->min_wait_ms_before_next_request > 1) {
			if (gf_sys_clock() < dash->min_wait_sys_clock + dash->min_wait_ms_before_next_request) {
				return GF_EOS;
			}
			dash->min_wait_ms_before_next_request = 0;
		}

		/*check if cache is not full*/
		dash->tsb_exceeded = 0;
		dash->time_in_tsb = 0;
		for (i=0; i<group_count; i++) {
			GF_MPD_Type type = dash->mpd->type;
			GF_DASH_Group *group = gf_list_get(dash->groups, i);

			if ((group->selection != GF_DASH_GROUP_SELECTED)
				|| group->depend_on_group
				|| !group->adaptation_set
				|| (group->done && !group->nb_cached_segments)
			) {
				continue;
			}
			if (group->period->origin_base_url)
				type = group->period->type;

			all_groups_done = 0;
			if (type==GF_MPD_TYPE_DYNAMIC) {
				gf_dash_group_check_time(group);
			}
			//cache is full, notify client some segments are pending
			if ((group->nb_cached_segments == group->max_cached_segments)
				&& !dash->request_period_switch
				&& !group->has_pending_enhancement
			) {
				dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_SEGMENT_AVAILABLE, i, GF_OK);
			}
			if (group->done && group->nb_cached_segments) {
				dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_SEGMENT_AVAILABLE, i, GF_OK);
			}

			if (group->nb_cached_segments<group->max_cached_segments) {
				cache_full = 0;
			}
			if (group->rate_adaptation_postponed)
				has_postponed_rate_adaptation = GF_TRUE;

			if (!cache_full)
				break;
		}

		if (dash->tsb_exceeded) {
			dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_TIMESHIFT_OVERFLOW, (s32) dash->tsb_exceeded, GF_OK);
			dash->tsb_exceeded = 0;
		} else if (dash->time_in_tsb != dash->prev_time_in_tsb) {
			dash->prev_time_in_tsb = dash->time_in_tsb;
			dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_TIMESHIFT_UPDATE, 0, GF_OK);
		}


		if (!cache_full) {
			(*cache_is_full) = GF_FALSE;
		} else {
			//seek request
			if (dash->request_period_switch==2) all_groups_done = 1;

			if (all_groups_done && dash->next_period_checked) {
				dash->next_period_checked = 1;
			}
			if (all_groups_done && dash->request_period_switch) {
				Bool is_chain = GF_FALSE;
				gf_dash_reset_groups(dash);
				if (dash->request_period_switch == 1) {
					if (dash->speed<0) {
						if (dash->active_period_index) {
							dash->active_period_index--;
						}
					} else {
						dash->active_period_index++;
					}
				}

				(*cache_is_full) = GF_FALSE;
				dash->request_period_switch = 0;
				dash->period_groups_setup = GF_FALSE;
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Switching to period #%d\n", dash->active_period_index+1));
				dash->dash_state = GF_DASH_STATE_SETUP;

				if (dash->active_period_index >= gf_list_count(dash->mpd->periods)) {
					if (dash->chain_next) {
						if (dash->chain_stack_state==GF_DASH_CHAIN_PUSH) {
							if (!dash->chain_stack) dash->chain_stack = gf_list_new();
							gf_list_add(dash->chain_stack, gf_strdup(dash->base_url));
						}
					} else if (dash->chain_stack_state==GF_DASH_CHAIN_POP) {
						dash->chain_next = gf_list_pop_back(dash->chain_stack);
					}
					assert(dash->chain_next);
					dash->chain_stack_state = 0;
					dash->dash_state = GF_DASH_STATE_CHAIN_NEXT;
					is_chain = GF_TRUE;
				} else if (dash->preroll_state==1) {
					is_chain = GF_TRUE;
					dash->preroll_state = 2;
					dash->initial_period_tunein = GF_TRUE;
				}

				if (!dash->all_groups_done_notified) {
					dash->all_groups_done_notified = GF_TRUE;
					dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_END_OF_PERIOD, is_chain, GF_OK);
				}
			} else {
				(*cache_is_full) = GF_TRUE;
				return GF_OK;
			}
		}
	}

	if (has_postponed_rate_adaptation) {
		dash_global_rate_adaptation(dash, GF_TRUE);
	}
	return GF_OK;
}

static GF_Err gf_dash_process_internal(GF_DashClient *dash)
{
	GF_Err e;
	Bool cache_is_full;

retry:
	if (dash->in_error) return GF_SERVICE_ERROR;

	if (dash->force_period_reload) {
		if (gf_sys_clock() - dash->force_period_reload < 500) return GF_OK;
		dash->force_period_reload = 0;
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Retrying period reload after previous failure\n"));
	}

	switch (dash->dash_state) {
	case GF_DASH_STATE_CHAIN_FALLBACK:
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error in playback, loading fallback MPD\n"));
		gf_dash_reset_groups(dash);
		dash->request_period_switch = 0;
		dash->active_period_index = gf_list_count(dash->mpd->periods);
		dash->period_groups_setup = GF_FALSE;
		if (dash->chain_next) gf_free(dash->chain_next);
		dash->chain_next = dash->chain_fallback;
		dash->chain_fallback = NULL;

		dash->dash_state = GF_DASH_STATE_CHAIN_NEXT;
		dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_END_OF_PERIOD, GF_TRUE, GF_OK);
		//fallthrough

	case GF_DASH_STATE_CHAIN_NEXT:
	{
		char *next_mpd = dash->chain_next;
		dash->chain_next = NULL;
		char *fallback_mpd = dash->chain_fallback;
		dash->chain_fallback = NULL;
		e = gf_dash_open(dash, next_mpd);

		if (e) {
			if (fallback_mpd) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Failed to open DASH MPD %s in chain: %s - using fallback %s\n", next_mpd, gf_error_to_string(e), fallback_mpd));
				e = gf_dash_open(dash, fallback_mpd);
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Failed to open DASH MPD %s in chain: %s\n", next_mpd, gf_error_to_string(e) ));
			}
		}
		gf_free(next_mpd);
		if (fallback_mpd) gf_free(fallback_mpd);
		return e;
	}
	case GF_DASH_STATE_SETUP:
		dash->in_period_setup = 1;
		e = dash_setup_period_and_groups(dash);
		if (e) return e;

		if (dash->force_period_reload) {
			dash->force_period_reload = gf_sys_clock();
			return GF_OK;
		}

		dash->last_update_time = gf_sys_clock();
		dash->dash_state = GF_DASH_STATE_CONNECTING;

		//fallthrough
		
	case GF_DASH_STATE_CONNECTING:
		/*ask the user to connect to desired groups*/
		e = dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_CREATE_PLAYBACK, -1, GF_OK);
		if (e) return e;
		dash->in_period_setup = 0;
		dash->dash_state = GF_DASH_STATE_RUNNING;
		dash->min_wait_ms_before_next_request = 0;
		return GF_OK;
	case GF_DASH_STATE_RUNNING:
		e = dash_check_mpd_update_and_cache(dash, &cache_is_full);
		if (e || cache_is_full) {
			return GF_OK;
		}

		if (dash->dash_state == GF_DASH_STATE_SETUP)
			return GF_OK;
		//we rerun here, otherwise the user might think we are done...
		if ((dash->dash_state==GF_DASH_STATE_CHAIN_NEXT) || (dash->dash_state==GF_DASH_STATE_CHAIN_FALLBACK))
			goto retry;

		dash->initial_period_tunein = GF_FALSE;
		dash_do_groups(dash);
		return GF_OK;
	case GF_DASH_STATE_STOPPED:
		return GF_EOS;
	}
	return GF_OK;
}

GF_Err gf_dash_process(GF_DashClient *dash)
{
	return gf_dash_process_internal(dash);
}


static u32 gf_dash_period_index_from_time(GF_DashClient *dash, u64 time)
{
	u32 i, count, active_period_plus_one;
	u64 cumul_start = 0;
	Bool is_dyn = GF_FALSE;
	GF_MPD_Period *period;

	if (dash->mpd->type==GF_MPD_TYPE_DYNAMIC) {
		u64 now, availabilityStartTime;
		availabilityStartTime = dash->mpd->availabilityStartTime + dash->utc_shift;
		availabilityStartTime += dash->utc_drift_estimate;

		now = dash->mpd_fetch_time + (gf_sys_clock() - dash->last_update_time) - availabilityStartTime;
		if (dash->initial_time_shift_value<=100) {
			now -= dash->mpd->time_shift_buffer_depth * dash->initial_time_shift_value / 100;
		} else {
			now -= dash->initial_time_shift_value;
		}
		if (!time) is_dyn = GF_TRUE;
		time += now;
	}


restart:
	count = gf_list_count(dash->mpd->periods);
	cumul_start = 0;
	active_period_plus_one = 0;
	for (i = 0; i<count; i++) {
		period = gf_list_get(dash->mpd->periods, i);

		if (period->xlink_href && !period->duration) {
			if (!active_period_plus_one || period->xlink_actuate_on_load) {
				gf_dash_solve_period_xlink(dash, dash->mpd->periods, i);
				goto restart;
			}
		}

		if ((period->start > time) || (cumul_start > time)) {
		} else {
			cumul_start += period->duration;

			if (!active_period_plus_one && (time < cumul_start)) {
				active_period_plus_one = i+1;
			}
		}
	}
	if (is_dyn) {
		for (i = 0; i<count; i++) {
			period = gf_list_get(dash->mpd->periods, i);
			if (!period->xlink_href && !period->origin_base_url) return i;
		}
	}
	return active_period_plus_one ? active_period_plus_one-1 : 0;
}

static Bool gf_dash_seek_periods(GF_DashClient *dash, Double seek_time)
{
	Double start_time;
	/*we have an arch issue here: we may get a seek request in normal multiperiod playback
	 with a seek time close but before to the active period, typically if the stream is shorter
	 than the advertised period duration. We will use a safety check of 0.5 seconds, any seek request
	 at Start(Pn) - 0.5 will resolve in seek at Start(Pn)
	 * */
	Bool at_period_boundary=GF_FALSE;
	u32 i, period_idx;
	u32 nb_retry = 10;

	dash->start_range_period = 0;
	start_time = 0;
	period_idx = 0;
	for (i=0; i<gf_list_count(dash->mpd->periods); i++) {
		GF_MPD_Period *period = gf_list_get(dash->mpd->periods, i);
		Double dur;

		if (period->xlink_href) {
			gf_dash_solve_period_xlink(dash, dash->mpd->periods, i);
			if (nb_retry) {
				nb_retry --;
				i--;
				continue;
			}
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Period still resolves to XLINK %s for more than 10 consecutive retry, ignoring it ...\n", period->xlink_href));
			gf_free(period->xlink_href);
			period->xlink_href = NULL;
		} else {
			nb_retry = 10;
		}
		dur = (Double)period->duration;
		dur /= 1000;
		if (seek_time + 0.5 >= start_time) {
			if ((i+1==gf_list_count(dash->mpd->periods)) || (seek_time + 0.5 < start_time + dur) ) {
				if (seek_time > start_time + dur) {
					at_period_boundary = GF_TRUE;
				}
				period_idx = i;
				break;
			}
		}
		start_time += dur;
	}
	if (period_idx != dash->active_period_index) {
		seek_time -= start_time;
		dash->active_period_index = period_idx;
		dash->request_period_switch = 2;

		dash->start_range_period = seek_time;
	} else if (seek_time < start_time) {
		at_period_boundary = GF_TRUE;
	}

	if (at_period_boundary) return GF_TRUE;
	return dash->request_period_switch ? 1 : 0;
}

static void gf_dash_seek_group(GF_DashClient *dash, GF_DASH_Group *group, Double seek_to, Bool is_dynamic)
{
	GF_Err e;
	u32 first_downloaded, last_downloaded, segment_idx, orig_idx;

	if (group->selection==GF_DASH_GROUP_NOT_SELECTABLE) return;
	
	group->force_segment_switch = 0;
	if (!is_dynamic) {
		/*figure out where to seek*/
		orig_idx = group->download_segment_index;
		e = gf_mpd_seek_in_period(seek_to, MPD_SEEK_PREV, group->period, group->adaptation_set, gf_list_get(group->adaptation_set->representations, group->active_rep_index), &segment_idx, NULL);
		if (e<0)
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] An error occured while seeking to time %lf: %s\n", seek_to, gf_error_to_string(e)));

		group->download_segment_index = orig_idx;

		/*remember to seek to given duration*/
		group->start_playback_range = seek_to;

		first_downloaded = last_downloaded = group->download_segment_index;
		if (group->download_segment_index + 1 >= (s32) group->nb_cached_segments) {
			first_downloaded = group->download_segment_index + 1 - group->nb_cached_segments;
		}
		/*we are seeking in our download range, just go on*/
		if ((segment_idx>=first_downloaded) && (segment_idx<=last_downloaded)) {
			return;
		}

		group->force_segment_switch = 1;
		group->download_segment_index = segment_idx;
	} else {
		group->start_number_at_last_ast = 0;
		/*remember to adjust time in timeline setup*/
		group->start_playback_range = seek_to;
		group->timeline_setup = GF_FALSE;
	}

	while (group->nb_cached_segments) {
		group->nb_cached_segments--;
		gf_dash_group_reset_cache_entry(&group->cached[group->nb_cached_segments]);
	}
	group->done = 0;
}

GF_EXPORT
void gf_dash_group_seek(GF_DashClient *dash, u32 group_idx, Double seek_to)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, group_idx);
	if (!group) return;
	gf_dash_seek_group(dash, group, seek_to, (dash->mpd->type==GF_MPD_TYPE_DYNAMIC) ? GF_TRUE : GF_FALSE);
}

static void gf_dash_seek_groups(GF_DashClient *dash, Double seek_time, Bool is_dynamic)
{
	u32 i;

	if (dash->active_period_index) {
		Double dur = 0;
		for (i=0; i<dash->active_period_index; i++) {
			GF_MPD_Period *period = gf_list_get(dash->mpd->periods, dash->active_period_index-1);
			dur += period->duration/1000.0;
		}
		seek_time -= dur;
	}
	for (i=0; i<gf_list_count(dash->groups); i++) {
		GF_DASH_Group *group = gf_list_get(dash->groups, i);
		gf_dash_seek_group(dash, group, seek_time, is_dynamic);
	}
}


static GF_Err http_ifce_get(GF_FileDownload *getter, char *url)
{
	GF_Err e;
	Bool owns_sess = GF_FALSE;
	GF_DASHFileIOSession *sess;
	GF_DashClient *dash = (GF_DashClient*) getter->udta;
	if (!getter->session) {
		if (!dash->mpd_dnload) {
			sess = dash->dash_io->create(dash->dash_io, 1, url, -1);
			if (!sess) return GF_IO_ERR;
			getter->session = sess;
			e = GF_OK;
			owns_sess = GF_TRUE;
		} else {
			sess = getter->session = dash->mpd_dnload;
			e = dash->dash_io->setup_from_url(dash->dash_io, getter->session, url, -1);
		}
	}
	else {
		u32 group_idx = -1, i;
		for (i = 0; i < gf_list_count(dash->groups); i++) {
			GF_DASH_Group *group = gf_list_get(dash->groups, i);
			if (group->selection != GF_DASH_GROUP_SELECTED) continue;
			group_idx = i;
			break;
		}
		e = dash->dash_io->setup_from_url(dash->dash_io, getter->session, url, group_idx);
		if (e) {
			//with ROUTE we may have 404 right away if nothing in cache yet, not an error
			GF_LOG(dash->route_clock_state ? GF_LOG_DEBUG : GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot resetup downloader for url %s: %s\n", url, gf_error_to_string(e) ));
			return e;
		}
		sess = (GF_DASHFileIOSession *)getter->session;
	}
	if (!e)
		e = dash->dash_io->init(dash->dash_io, sess);

	if (e) {
		if (owns_sess) {
			dash->dash_io->del(dash->dash_io, sess);
			if (getter->session == sess)
				getter->session = NULL;
		}
		return e;
	}
	return dash->dash_io->run(dash->dash_io, sess);
}

static void http_ifce_clean(GF_FileDownload *getter)
{
	GF_DashClient *dash = (GF_DashClient*) getter->udta;
	if (getter->session) dash->dash_io->del(dash->dash_io, getter->session);
	getter->session = NULL;
}

static const char *http_ifce_cache_name(GF_FileDownload *getter)
{
	GF_DashClient *dash = (GF_DashClient*) getter->udta;
	if (getter->session) return dash->dash_io->get_cache_name(dash->dash_io, getter->session);
	return NULL;
}

GF_EXPORT
GF_Err gf_dash_open(GF_DashClient *dash, const char *manifest_url)
{
	char local_path[GF_MAX_PATH];
	const char *local_url;
	char *sep_cgi = NULL;
	char *sep_frag = NULL;
	GF_Err e;
	GF_MPD_Period *period;
	GF_DOMParser *mpd_parser=NULL;
	Bool is_local = GF_FALSE;

	if (!dash || !manifest_url) return GF_BAD_PARAM;

	memset( dash->lastMPDSignature, 0, GF_SHA1_DIGEST_SIZE);
	dash->reload_count = 0;

	dash->initial_period_tunein = GF_TRUE;
	dash->route_clock_state = dash->reload_count = dash->last_update_time = 0;
	memset(dash->lastMPDSignature, 0, sizeof(char)*GF_SHA1_DIGEST_SIZE);
	dash->utc_drift_estimate = 0;
	dash->time_in_tsb = dash->prev_time_in_tsb = 0;
	dash->reinit_period_index = 0;

	if (dash->base_url) gf_free(dash->base_url);
	sep_cgi = strrchr(manifest_url, '?');
	if (sep_cgi) sep_cgi[0] = 0;
	dash->base_url = gf_strdup(manifest_url);
	if (sep_cgi) sep_cgi[0] = '?';

	dash->getter.udta = dash;
	dash->getter.new_session = http_ifce_get;
	dash->getter.del_session = http_ifce_clean;
	dash->getter.get_cache_name = http_ifce_cache_name;
	dash->getter.session = NULL;



	if (dash->mpd_dnload) dash->dash_io->del(dash->dash_io, dash->mpd_dnload);
	dash->mpd_dnload = NULL;
	local_url = NULL;

	if (!strnicmp(manifest_url, "file://", 7)) {
		local_url = manifest_url + 7;
		is_local = 1;
		if (strstr(manifest_url, ".m3u8")) {
			dash->is_m3u8 = GF_TRUE;
		}
	} else if (strstr(manifest_url, "://") && strncmp(manifest_url, "gfio://", 7)) {
		const char *reloc_url, *mtype;
		char mime[128];
		e = gf_dash_download_resource(dash, &(dash->mpd_dnload), manifest_url, 0, 0, 1, NULL);
		if (e!=GF_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot connect service: MPD downloading problem %s for %s\n", gf_error_to_string(e), manifest_url));
			dash->dash_io->del(dash->dash_io, dash->mpd_dnload);
			dash->mpd_dnload = NULL;
			return e;
		}

		mtype = dash->dash_io->get_mime(dash->dash_io, dash->mpd_dnload);
		strcpy(mime, mtype ? mtype : "");
		strlwr(mime);

		reloc_url = dash->dash_io->get_url(dash->dash_io, dash->mpd_dnload);

		/*if relocated use new URL as base URL for all requests*/
		if (strcmp(reloc_url, manifest_url)) {
			gf_free(dash->base_url);
			dash->base_url = gf_strdup(reloc_url);
		}

		local_url = dash->dash_io->get_cache_name(dash->dash_io, dash->mpd_dnload);
		if (!local_url) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot connect service: cache problem %s\n", local_url));
			dash->dash_io->del(dash->dash_io, dash->mpd_dnload);
			dash->mpd_dnload = NULL;
			return GF_IO_ERR;
		}
	} else {
		local_url = manifest_url;
		is_local = 1;
	}

	if (is_local && strncmp(manifest_url, "gfio://", 7)) {
		FILE *f = gf_fopen(local_url, "rt");
		if (!f) {
			sep_cgi = strrchr(local_url, '?');
			if (sep_cgi) sep_cgi[0] = 0;
			sep_frag = strrchr(local_url, '#');
			if (sep_frag) sep_frag[0] = 0;

			f = gf_fopen(local_url, "rt");
			if (!f) {
				if (sep_cgi) sep_cgi[0] = '?';
				if (sep_frag) sep_frag[0] = '#';
				return GF_URL_ERROR;
			}
		}
		gf_fclose(f);
	}
	dash->mpd_fetch_time = dash_get_fetch_time(dash);

	gf_dash_reset_groups(dash);

	if (dash->mpd)
		gf_mpd_del(dash->mpd);

	dash->mpd = gf_mpd_new();
	if (!dash->mpd) {
		e = GF_OUT_OF_MEM;
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot connect service: MPD creation problem %s\n", gf_error_to_string(e)));
		goto exit;
	}
	if (dash->dash_io->manifest_updated) {
		const char *szName = gf_file_basename(manifest_url);
		dash->dash_io->manifest_updated(dash->dash_io, szName, local_url, -1);
	}

	//peek payload, check if m3u8 - MPD and SmoothStreaming are checked after
	char szLine[100];
	FILE *f = gf_fopen(local_url, "r");
	if (f) {
		gf_fread(szLine, 100, f);
		gf_fclose(f);
	}
	szLine[99] = 0;
	if (strstr(szLine, "#EXTM3U"))
		dash->is_m3u8 = 1;

	if (dash->is_m3u8) {
		if (is_local) {
			char *sep;
			strcpy(local_path, local_url);
			sep = gf_file_ext_start(local_path);
			if (sep) sep[0]=0;
			strcat(local_path, ".mpd");

			e = gf_m3u8_to_mpd(local_url, manifest_url, local_path, dash->reload_count, dash->mimeTypeForM3U8Segments, 0, M3U8_TO_MPD_USE_TEMPLATE, M3U8_TO_MPD_USE_SEGTIMELINE, &dash->getter, dash->mpd, GF_FALSE, dash->keep_files);
		} else {
			const char *redirected_url = dash->dash_io->get_url(dash->dash_io, dash->mpd_dnload);
			if (!redirected_url) redirected_url=manifest_url;

			e = gf_m3u8_to_mpd(local_url, redirected_url, NULL, dash->reload_count, dash->mimeTypeForM3U8Segments, 0, M3U8_TO_MPD_USE_TEMPLATE, M3U8_TO_MPD_USE_SEGTIMELINE, &dash->getter, dash->mpd, GF_FALSE, dash->keep_files);
		}

		if (!e && dash->split_adaptation_set)
			gf_mpd_split_adaptation_sets(dash->mpd);

	} else {
		u32 res = gf_dash_check_mpd_root_type(local_url);
		if (res==2) {
			dash->is_smooth = 1;
		}
		if (!dash->is_smooth && !res) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot connect service: wrong file type %s\n", local_url));
			dash->dash_io->del(dash->dash_io, dash->mpd_dnload);
			dash->mpd_dnload = NULL;
			return GF_URL_ERROR;
		}

		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] parsing %s manifest %s\n", dash->is_smooth ? "SmoothStreaming" : "DASH-MPD", local_url));

		/* parse the MPD */
		mpd_parser = gf_xml_dom_new();
		e = gf_xml_dom_parse(mpd_parser, local_url, NULL, NULL);

		if (sep_cgi) sep_cgi[0] = '?';
		if (sep_frag) sep_frag[0] = '#';

		if (e != GF_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot connect service: MPD parsing problem %s\n", gf_xml_dom_get_error(mpd_parser) ));
			gf_xml_dom_del(mpd_parser);
			dash->dash_io->del(dash->dash_io, dash->mpd_dnload);
			dash->mpd_dnload = NULL;
			return GF_URL_ERROR;
		}

		if (dash->is_smooth) {
			e = gf_mpd_init_smooth_from_dom(gf_xml_dom_get_root(mpd_parser), dash->mpd, manifest_url);
		} else {
			e = gf_mpd_init_from_dom(gf_xml_dom_get_root(mpd_parser), dash->mpd, manifest_url);
		}
		gf_xml_dom_del(mpd_parser);

		if (!e && dash->split_adaptation_set)
			gf_mpd_split_adaptation_sets(dash->mpd);

		if (dash->ignore_xlink)
			dash_purge_xlink(dash->mpd);

	}
	//for both DASH and HLS, we support ROUTE
	if (!is_local) {
		const char *hdr = dash->dash_io->get_header_value(dash->dash_io, dash->mpd_dnload, "x-route");
		if (hdr) {
			if (!dash->route_clock_state) {
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Detected ROUTE DASH service ID %s\n", hdr));
				dash->route_clock_state = 1;
			}
		}
	}

	if (e != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot connect service: MPD creation problem %s\n", gf_error_to_string(e)));
		goto exit;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] DASH client initialized from MPD at UTC time "LLU" - availabilityStartTime "LLU"\n", dash->mpd_fetch_time , dash->mpd->availabilityStartTime));

	if (is_local && dash->mpd->minimum_update_period) {
		e = gf_dash_update_manifest(dash);
		if (e != GF_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update MPD: %s\n", gf_error_to_string(e)));
			goto exit;
		}
	}

	/* Get the right period from the given time */
	dash->active_period_index = gf_dash_period_index_from_time(dash, 0);
	period = gf_list_get(dash->mpd->periods, dash->active_period_index);
	if (period->xlink_href) {
		gf_dash_solve_period_xlink(dash, dash->mpd->periods, dash->active_period_index);
		period = gf_list_get(dash->mpd->periods, dash->active_period_index);
	}
	if (!period || !gf_list_count(period->adaptation_sets) ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot start: not enough periods or representations in MPD\n"));
		e = GF_URL_ERROR;
		goto exit;
	}

	if (dash->chaining_mode) {
		dash_check_chaining(dash, "urn:mpeg:dash:mpd-chaining:2016", &dash->chain_next, 0);
		dash_check_chaining(dash, "urn:mpeg:dash:fallback:2016", &dash->chain_fallback, 0);
		dash_check_chaining(dash, "urn:mpeg:dash:origin-set:2019", NULL, GF_DASH_CHAIN_PUSH);
		dash_check_chaining(dash, "urn:mpeg:dash:origin-back:2019", NULL, GF_DASH_CHAIN_POP);
	}

	dash->dash_state = GF_DASH_STATE_SETUP;
	return GF_OK;

exit:
	if (dash->dash_io) {
		dash->dash_io->del(dash->dash_io, dash->mpd_dnload);
		dash->mpd_dnload = NULL;
	}

	if (dash->mpd)
		gf_mpd_del(dash->mpd);
	dash->mpd = NULL;
	return e;
}

GF_EXPORT
void gf_dash_close(GF_DashClient *dash)
{
	assert(dash);

	if (dash->dash_io) {
		if (dash->mpd_dnload) {
			if (dash->mpd_dnload == dash->getter.session)
				dash->getter.session = NULL;
			dash->dash_io->del(dash->dash_io, dash->mpd_dnload);
			dash->mpd_dnload = NULL;
		}

		if (dash->getter.del_session && dash->getter.session)
			dash->getter.del_session(&dash->getter);
	}
	if (dash->mpd) {
		gf_mpd_del(dash->mpd);
		dash->mpd = NULL;
	}
	if (dash->chain_next) {
		gf_free(dash->chain_next);
		dash->chain_next = NULL;
	}
	if (dash->chain_fallback) {
		gf_free(dash->chain_fallback);
		dash->chain_fallback = NULL;
	}
	dash->chain_stack_state = 0;

	if (dash->dash_state != GF_DASH_STATE_CONNECTING)
		gf_dash_reset_groups(dash);
}

GF_EXPORT
void gf_dash_set_algo(GF_DashClient *dash, GF_DASHAdaptationAlgorithm algo)
{
	dash->adaptation_algorithm = algo;
	switch (dash->adaptation_algorithm) {
	case GF_DASH_ALGO_GPAC_LEGACY_BUFFER:
		dash->rate_adaptation_algo = dash_do_rate_adaptation_legacy_buffer;
		dash->rate_adaptation_download_monitor = dash_do_rate_monitor_default;
		break;
	case GF_DASH_ALGO_GPAC_LEGACY_RATE:
		dash->rate_adaptation_algo = dash_do_rate_adaptation_legacy_rate;
		dash->rate_adaptation_download_monitor = dash_do_rate_monitor_default;
		break;
	case GF_DASH_ALGO_BBA0:
		dash->rate_adaptation_algo = dash_do_rate_adaptation_bba0;
		dash->rate_adaptation_download_monitor = dash_do_rate_monitor_default;
		break;
	case GF_DASH_ALGO_BOLA_FINITE:
	case GF_DASH_ALGO_BOLA_BASIC:
	case GF_DASH_ALGO_BOLA_U:
	case GF_DASH_ALGO_BOLA_O:
		dash->rate_adaptation_algo = dash_do_rate_adaptation_bola;
		dash->rate_adaptation_download_monitor = dash_do_rate_monitor_default;
		break;
	case GF_DASH_ALGO_NONE:
	default:
		dash->rate_adaptation_algo = NULL;
		break;
	}
}

static s32 dash_do_rate_adaptation_custom(GF_DashClient *dash, GF_DASH_Group *group, GF_DASH_Group *base_group,
		  	  	  	  	  	  	  	  	  u32 dl_rate, Double speed, Double max_available_speed, Bool force_lower_complexity,
										  	  GF_MPD_Representation *rep, Bool go_up_bitrate)
{
	GF_DASHCustomAlgoInfo stats;
	u32 g_idx = group->groups_idx;
	u32 b_idx = base_group->groups_idx;

	stats.download_rate = dl_rate;
	stats.file_size = group->total_size;
	stats.speed = speed;
	stats.max_available_speed = max_available_speed;
	stats.disp_width = group->hint_visible_width;
	stats.disp_height = group->hint_visible_height;
	stats.active_quality_idx = group->active_rep_index;
	stats.buffer_min_ms = group->buffer_min_ms;
	stats.buffer_max_ms = group->buffer_max_ms;
	stats.buffer_occupancy_ms = group->buffer_occupancy_ms;
	stats.quality_degradation_hint = group->quality_degradation_hint;
	stats.total_rate = dash->total_rate;

	return dash->rate_adaptation_algo_custom(dash->udta_custom_algo, g_idx, b_idx, force_lower_complexity, &stats);

}

static s32 dash_do_rate_monitor_custom(GF_DashClient *dash, GF_DASH_Group *group, u32 bits_per_sec, u64 total_bytes, u64 bytes_done, u64 us_since_start, u32 buffer_dur_ms, u32 current_seg_dur)
{
	u32 g_idx = group->groups_idx;
	return dash->rate_adaptation_download_monitor_custom(dash->udta_custom_algo, g_idx, bits_per_sec, total_bytes, bytes_done, us_since_start, buffer_dur_ms, current_seg_dur);
}

GF_EXPORT
void gf_dash_set_algo_custom(GF_DashClient *dash, void *udta,
		gf_dash_rate_adaptation algo_custom,
		gf_dash_download_monitor download_monitor_custom)
{
	dash->adaptation_algorithm = GF_DASH_ALGO_CUSTOM;
	dash->rate_adaptation_algo = dash_do_rate_adaptation_custom;
	dash->rate_adaptation_download_monitor = dash_do_rate_monitor_custom;

	dash->udta_custom_algo = udta;
	dash->rate_adaptation_algo_custom = algo_custom;
	dash->rate_adaptation_download_monitor_custom = download_monitor_custom;

}

GF_EXPORT
GF_DashClient *gf_dash_new(GF_DASHFileIO *dash_io, u32 max_cache_duration, u32 auto_switch_count, Bool keep_files, Bool disable_switching, GF_DASHInitialSelectionMode first_select_mode, u32 initial_time_shift_percent)
{
	GF_DashClient *dash;
	if (!dash_io) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot create client withou sync IO for HTTP\n"));
		return NULL;
	}

	GF_SAFEALLOC(dash, GF_DashClient);
	if (!dash) return NULL;
	dash->dash_io = dash_io;
	dash->speed = 1.0;
	dash->is_rt_speed = GF_TRUE;
	dash->low_latency_mode = GF_DASH_LL_STRICT;

	//wait one segment to validate we have enough bandwidth
	dash->probe_times_before_switch = 1;
	//FIXME: mime type for segments MUST be mp2t, webvtt or a Packed Audio file (like AAC)
	dash->mimeTypeForM3U8Segments = gf_strdup( "video/mp2t" );

	dash->max_cache_duration = max_cache_duration;
	dash->initial_time_shift_value = initial_time_shift_percent;

	dash->auto_switch_count = auto_switch_count;
	dash->keep_files = keep_files;
	dash->disable_switching = disable_switching;
	dash->first_select_mode = first_select_mode;
	dash->min_timeout_between_404 = 500;
	dash->segment_lost_after_ms = 100;
	dash->dbg_grps_index = NULL;
	dash->tile_rate_decrease = 100;
	dash->route_ast_shift = 1000;
	dash->initial_period_tunein = GF_TRUE;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Client created\n"));

#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_cov_mode()) {
		on_group_download_error(NULL, NULL, NULL, GF_OK, NULL, NULL, NULL, GF_FALSE);
		gf_dash_is_running(dash);
	}
#endif

	return dash;
}

GF_EXPORT
void gf_dash_del(GF_DashClient *dash)
{
	//force group cleanup
	dash->dash_state = GF_DASH_STATE_STOPPED;
	gf_dash_close(dash);

	while (gf_list_count(dash->chain_stack)) {
		char *url = gf_list_pop_back(dash->chain_stack);
		gf_free(url);
	}
	gf_list_del(dash->chain_stack);

	if (dash->mimeTypeForM3U8Segments) gf_free(dash->mimeTypeForM3U8Segments);
	if (dash->base_url) gf_free(dash->base_url);

	gf_free(dash);
}

GF_EXPORT
void gf_dash_enable_utc_drift_compensation(GF_DashClient *dash, Bool estimate_utc_drift)
{
	dash->estimate_utc_drift = estimate_utc_drift;
}

GF_EXPORT
void gf_dash_set_switching_probe_count(GF_DashClient *dash, u32 switch_probe_count)
{
	dash->probe_times_before_switch = switch_probe_count;
}

GF_EXPORT
void gf_dash_enable_single_range_llhls(GF_DashClient *dash, Bool enable)
{
	dash->llhls_single_range = enable;
}

GF_EXPORT
void gf_dash_enable_group_selection(GF_DashClient *dash, Bool enable)
{
	dash->enable_group_selection = enable;
}

GF_EXPORT
void gf_dash_set_agressive_adaptation(GF_DashClient *dash, Bool agressive_switch)
{
	dash->agressive_switching = agressive_switch;
}


GF_EXPORT
u32 gf_dash_get_group_count(GF_DashClient *dash)
{
	return gf_list_count(dash->groups);
}


GF_EXPORT
void *gf_dash_get_group_udta(GF_DashClient *dash, u32 idx)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return NULL;
	return group->udta;
}

GF_EXPORT
GF_Err gf_dash_set_group_udta(GF_DashClient *dash, u32 idx, void *udta)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return GF_BAD_PARAM;
	group->udta = udta;
	return GF_OK;
}

GF_EXPORT
Bool gf_dash_is_group_selected(GF_DashClient *dash, u32 idx)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return 0;
	return (group->selection == GF_DASH_GROUP_SELECTED) ? 1 : 0;
}

GF_EXPORT
Bool gf_dash_is_group_selectable(GF_DashClient *dash, u32 idx)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return 0;
	return (group->selection == GF_DASH_GROUP_NOT_SELECTABLE) ? 0 : 1;
}

GF_EXPORT
void gf_dash_get_info(GF_DashClient *dash, const char **title, const char **source)
{
	GF_MPD_ProgramInfo *info = dash ? gf_list_get(dash->mpd->program_infos, 0) : NULL;
	if (title) *title = info ? info->title : NULL;
	if (source) *source = info ? info->source : NULL;
}


GF_EXPORT
void gf_dash_switch_quality(GF_DashClient *dash, Bool switch_up)
{
	u32 i;
	for (i=0; i<gf_list_count(dash->groups); i++) {
		u32 switch_to_rep_idx = 0;
		u32 bandwidth, quality, k;
		GF_MPD_Representation *rep, *active_rep;
		GF_DASH_Group *group = gf_list_get(dash->groups, i);
		u32 current_idx = group->active_rep_index;
		if (group->selection != GF_DASH_GROUP_SELECTED) continue;

		if (group->base_rep_index_plus_one) current_idx = group->max_complementary_rep_index;
		if (group->force_representation_idx_plus_one) current_idx = group->force_representation_idx_plus_one - 1;

		active_rep = gf_list_get(group->adaptation_set->representations, current_idx);
		if (!active_rep) continue;
		bandwidth = switch_up ? (u32) -1 : 0;
		quality = switch_up ? (u32) -1 : 0;

		for (k=0; k<gf_list_count(group->adaptation_set->representations); k++) {
			rep = gf_list_get(group->adaptation_set->representations, k);
			if (switch_up) {
				if ((rep->quality_ranking>active_rep->quality_ranking) || (rep->bandwidth>active_rep->bandwidth)) {
					if ((rep->quality_ranking < quality) || (rep->bandwidth < bandwidth)) {
						bandwidth = rep->bandwidth;
						quality = rep->quality_ranking;
						switch_to_rep_idx = k+1;
					}
				}
			} else {
				if ((rep->quality_ranking < active_rep->quality_ranking) || (rep->bandwidth < active_rep->bandwidth)) {
					if ((rep->quality_ranking > quality) || (rep->bandwidth > bandwidth)) {
						bandwidth = rep->bandwidth;
						quality = rep->quality_ranking;
						switch_to_rep_idx = k+1;
					}
				}
			}
		}
		if (switch_to_rep_idx && (switch_to_rep_idx-1 != current_idx) ) {
			u32 nb_cached_seg_per_rep = group->max_cached_segments / gf_dash_group_count_rep_needed(group);

			group->force_switch_bandwidth = 1;
			if (!group->base_rep_index_plus_one)
				group->force_representation_idx_plus_one = switch_to_rep_idx;
			else
				group->max_complementary_rep_index = switch_to_rep_idx-1;


			//deprecated code from old arch, dasher no longer downloads segments and cannot do immediate switch
			//cf https://github.com/gpac/gpac/issues/1852
#if 0
			if (group->local_files || immediate_switch) {
				u32 keep_seg_index = 0;
				//keep all scalable enhancements of the first segment
				rep = gf_list_get(group->adaptation_set->representations, group->cached[0].representation_index);
				if (rep->playback.enhancement_rep_index_plus_one) {
					u32 rep_idx = rep->playback.enhancement_rep_index_plus_one;
					while (keep_seg_index + 1 < group->nb_cached_segments) {
						rep = gf_list_get(group->adaptation_set->representations, group->cached[keep_seg_index+1].representation_index);
						if (rep_idx == group->cached[keep_seg_index+1].representation_index+1) {
							keep_seg_index ++;
							rep_idx = rep->playback.enhancement_rep_index_plus_one;
						}
						else
							break;
					}
				}

				if (!group->base_rep_index_plus_one) {
					/*in local playback just switch at the end of the current segment
					for remote, we should let the user decide*/
					while (group->nb_cached_segments > keep_seg_index + 1) {
						group->nb_cached_segments--;
						GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Group %d switching quality - delete cached segment: %s\n", i, group->cached[group->nb_cached_segments].url));

						gf_dash_group_reset_cache_entry(&group->cached[group->nb_cached_segments]);

						group->cached[group->nb_cached_segments].duration = (u32) group->current_downloaded_segment_duration;
						if (group->download_segment_index>1)
							group->download_segment_index--;
					}
				} else {
					if (switch_up) {
						//first, we keep the second segment and remove all segments from the third one
						keep_seg_index++;
						rep = gf_list_get(group->adaptation_set->representations, group->cached[keep_seg_index].representation_index);
						if (rep->playback.enhancement_rep_index_plus_one) {
							u32 rep_idx = rep->playback.enhancement_rep_index_plus_one;
							while (keep_seg_index + 1 < group->nb_cached_segments) {
								rep = gf_list_get(group->adaptation_set->representations, group->cached[keep_seg_index+1].representation_index);
								if (rep_idx == group->cached[keep_seg_index+1].representation_index+1) {
									keep_seg_index ++;
									rep_idx = rep->playback.enhancement_rep_index_plus_one;
								}
								else
									break;
							}
						}
						while (group->nb_cached_segments > keep_seg_index + 1) {
							Bool decrease_download_segment_index = (group->cached[group->nb_cached_segments-1].representation_index == current_idx) ? GF_TRUE : GF_FALSE;
							group->nb_cached_segments--;
							GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Group %d switching quality - delete cached segment: %s\n", i, group->cached[group->nb_cached_segments].url));

							gf_dash_group_reset_cache_entry(&group->cached[group->nb_cached_segments]);

							group->cached[group->nb_cached_segments].duration = (u32) group->current_downloaded_segment_duration;

							if (decrease_download_segment_index && group->download_segment_index>1)
								group->download_segment_index--;
						}
						/*force to download scalable enhancement of the second segment*/
						group->force_representation_idx_plus_one = switch_to_rep_idx;
						group->active_rep_index = switch_to_rep_idx - 1;
						group->download_segment_index--;
					}
					else if (group->nb_cached_segments) {
						/* we remove highest scalable enhancements of the dowloaded segments, and keep another segments*/
						for (k = group->nb_cached_segments - 1; k > keep_seg_index; k--) {
							if (group->cached[k].representation_index != current_idx)
								continue;
							group->nb_cached_segments--;
							GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Group %d switching quality - delete cached segment: %s\n", i, group->cached[k].url));
							if (k != group->nb_cached_segments) {
								memmove(&group->cached[k], &group->cached[k+1], (group->nb_cached_segments-k)*sizeof(segment_cache_entry));
							}
							memset(&group->cached[group->nb_cached_segments], 0, sizeof(segment_cache_entry));
						}
					}
				}
			}
#endif

			/*resize max cached segment*/
			group->max_cached_segments = nb_cached_seg_per_rep * gf_dash_group_count_rep_needed(group);

			if (group->srd_desc)
				gf_dash_set_tiles_quality(dash, group->srd_desc, GF_TRUE);
		}
	}
}

GF_EXPORT
Double gf_dash_get_duration(GF_DashClient *dash)
{
	return gf_mpd_get_duration(dash->mpd);
}

GF_EXPORT
u32 gf_dash_group_get_time_shift_buffer_depth(GF_DashClient *dash, u32 idx)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return 0;
	return group->time_shift_buffer_depth;
}

GF_EXPORT
const char *gf_dash_get_url(GF_DashClient *dash)
{
	return dash->base_url;
}

GF_EXPORT
Bool gf_dash_is_m3u8(GF_DashClient *dash) {
	return dash->is_m3u8;
}
GF_EXPORT
Bool gf_dash_is_smooth_streaming(GF_DashClient *dash) {
	return dash->is_smooth;
}

GF_EXPORT
const char *gf_dash_group_get_segment_mime(GF_DashClient *dash, u32 idx)
{
	GF_MPD_Representation *rep;
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return NULL;

	rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);
	return gf_dash_get_mime_type(NULL, rep, group->adaptation_set);
}



GF_EXPORT
const char *gf_dash_group_get_segment_init_url(GF_DashClient *dash, u32 idx, u64 *start_range, u64 *end_range)
{
	GF_MPD_Representation *rep;
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return NULL;

	/*solve dependencies if any - we first test highest: if this is a complementary rep, keep the highest for init
	otherwise use the selected one
	this is need for scalable because we only init once the demuxer*/

	rep = gf_list_last(group->adaptation_set->representations);
	if (!rep->dependency_id)
		rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);

	if (group->bs_switching_init_segment_url) {
		if (start_range) *start_range = group->bs_switching_init_segment_url_start_range;
		if (end_range) *end_range = group->bs_switching_init_segment_url_end_range;
		return group->bs_switching_init_segment_url;
	}

	//no rep found or no init for the rep, check all reps
	//this may happen when the adaptation rate algo changed the rep between the init (TS) and the next segment
	if (!rep || !rep->playback.cached_init_segment_url) {
		u32 i, count;
		count = gf_list_count(group->adaptation_set->representations);
		for (i=0; i<count; i++) {
			rep = gf_list_get(group->adaptation_set->representations, i);
			if (rep->playback.cached_init_segment_url) break;
			rep = NULL;
		}
	}
	if (!rep) return NULL;
	if (start_range) *start_range = rep->playback.init_start_range;
	if (end_range) *end_range = rep->playback.init_end_range;
	return rep->playback.cached_init_segment_url;
}

GF_EXPORT
const char *gf_dash_group_get_segment_init_keys(GF_DashClient *dash, u32 idx, u32 *crypt_type, bin128 *key_IV)
{
	GF_MPD_Representation *rep;
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return NULL;

	rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);
	if (!rep) return NULL;

	if (crypt_type) {
		*crypt_type = rep->crypto_type;
	}
	if (key_IV) memcpy(*key_IV, rep->playback.key_IV, sizeof(bin128));
	return rep->playback.key_url;
}

Bool gf_dash_group_init_segment_is_media(GF_DashClient *dash, u32 idx)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return GF_FALSE;
	return group->init_segment_is_media;
}

GF_EXPORT
s32 gf_dash_group_get_id(GF_DashClient *dash, u32 idx)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return -1;
	return group->adaptation_set->group;
}

GF_EXPORT
void gf_dash_group_select(GF_DashClient *dash, u32 idx, Bool select)
{
	Bool needs_resetup = GF_FALSE;
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return;
	if (group->selection == GF_DASH_GROUP_NOT_SELECTABLE)
		return;

	if ((group->selection==GF_DASH_GROUP_NOT_SELECTED) && select) needs_resetup = 1;

	group->selection = select ? GF_DASH_GROUP_SELECTED : GF_DASH_GROUP_NOT_SELECTED;

	/*this set is part of a group, make sure no all other sets from the indicated group are unselected*/
	if (dash->enable_group_selection && select && (group->adaptation_set->group>=0)) {
		u32 i;
		for (i=0; i<gf_dash_get_group_count(dash); i++) {
			GF_DASH_Group *agroup = gf_list_get(dash->groups, i);
			if (agroup==group) continue;

			/*either one Representation from group 0, if present,*/
			if ((group->adaptation_set->group==0)
			        /*or the combination of at most one Representation from each non-zero group*/
			        || (group->adaptation_set->group==agroup->adaptation_set->group)
			   ) {
				agroup->selection = GF_DASH_GROUP_NOT_SELECTED;
			}
		}
	}

	//TODO: recompute group download index based on current playback ...
	if (needs_resetup) {

	}
}

GF_EXPORT
void gf_dash_groups_set_language(GF_DashClient *dash, const char *lang_code_rfc_5646)
{
	u32 i, len;
	s32 lang_idx;
	GF_List *groups_selected;
	if (!lang_code_rfc_5646) lang_code_rfc_5646 = "und";

	groups_selected = gf_list_new();

	//first pass, check exact match
	for (i=0; i<gf_list_count(dash->groups); i++) {
		GF_DASH_Group *group = gf_list_get(dash->groups, i);
		if (group->selection==GF_DASH_GROUP_NOT_SELECTABLE) continue;

		//select groups with no language info or undetermined or matching our code
		if (!group->adaptation_set->lang
			|| !stricmp(group->adaptation_set->lang, lang_code_rfc_5646)
			|| !strnicmp(group->adaptation_set->lang, "und", 3)
			|| !strnicmp(group->adaptation_set->lang, "unkn", 4)
		) {
			gf_dash_group_select(dash, i, 1);
			gf_list_add(groups_selected, group);
		}
	}

	lang_idx = gf_lang_find(lang_code_rfc_5646);
	if (lang_idx>=0) {
		const char *n2cc = gf_lang_get_2cc(lang_idx);
		const char *n3cc = gf_lang_get_3cc(lang_idx);

		for (i=0; i<gf_list_count(dash->groups); i++) {
			char *sep;
			GF_DASH_Group *group = gf_list_get(dash->groups, i);
			if (group->selection==GF_DASH_GROUP_NOT_SELECTABLE) continue;
			if (!group->adaptation_set->lang) continue;
			if (gf_list_find(groups_selected, group) >= 0) continue;

			//check we didn't select one AS in this group in the previous pass or in this pass
			if (dash->enable_group_selection && group->adaptation_set->group>=0) {
				u32 k;
				Bool found = GF_FALSE;
				for (k=0; k<gf_list_count(groups_selected); k++) {
					GF_DASH_Group *ag = gf_list_get(groups_selected, k);

					if (ag->adaptation_set->group == group->adaptation_set->group) {
						found = 1;
						break;
					}
				}
				if (found) continue;
			}

			//get the 2 or 3 land code
			sep = strchr(group->adaptation_set->lang, '-');
			if (sep) {
				sep[0] = 0;
			}
			len = (u32) strlen(group->adaptation_set->lang);
			//compare with what we found
			if ( ((len==3) && !stricmp(group->adaptation_set->lang, n3cc))
			        || ((len==2) && !stricmp(group->adaptation_set->lang, n2cc))
			   ) {
				gf_dash_group_select(dash, i, 1);
				gf_list_add(groups_selected, group);
			} else {
				gf_dash_group_select(dash, i, 0);
			}

			if (sep) sep[0] = '-';
		}
	}

	gf_list_del(groups_selected);
}

GF_EXPORT
Bool gf_dash_is_running(GF_DashClient *dash)
{
	return (dash->dash_state==GF_DASH_STATE_STOPPED) ? GF_FALSE : GF_TRUE;
}

GF_EXPORT
Bool gf_dash_is_in_setup(GF_DashClient *dash)
{
	if (dash->dash_state==GF_DASH_STATE_SETUP) return GF_TRUE;
	if (dash->dash_state==GF_DASH_STATE_CONNECTING) return GF_TRUE;
	if (dash->request_period_switch) return GF_TRUE;
	return GF_FALSE;
}

GF_EXPORT
u32 gf_dash_get_period_switch_status(GF_DashClient *dash)
{
	return dash->request_period_switch;
}
GF_EXPORT
void gf_dash_request_period_switch(GF_DashClient *dash)
{
	GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Period switch has been requested\n"));
	dash->request_period_switch = 1;
}
GF_EXPORT
Bool gf_dash_in_last_period(GF_DashClient *dash, Bool check_eos)
{
	switch (dash->dash_state) {
	case GF_DASH_STATE_SETUP:
	case GF_DASH_STATE_CONNECTING:
		return GF_FALSE;
	default:
		break;
	}
	
	if (dash->active_period_index+1 < gf_list_count(dash->mpd->periods))
		return GF_FALSE;

	//check chaining
	if (dash->chain_next) return GF_FALSE;
	if ((dash->chain_stack_state==GF_DASH_CHAIN_POP) && gf_list_count(dash->chain_stack))
		return GF_FALSE;

	return GF_TRUE;
}
GF_EXPORT
Bool gf_dash_in_period_setup(GF_DashClient *dash)
{
	return dash->in_period_setup;
}

GF_EXPORT
void gf_dash_set_speed(GF_DashClient *dash, Double speed)
{
	u32 i;
	if (!dash) return;
	if (!speed) speed = 1.0;
	if (dash->speed == speed) return;

	for (i=0; i<gf_list_count(dash->groups); i++) {
		GF_DASH_Group *group = gf_list_get(dash->groups, i);
		GF_MPD_Representation *active_rep;
		Double max_available_speed;
		if (!group || (group->selection != GF_DASH_GROUP_SELECTED)) continue;
		active_rep = (GF_MPD_Representation *)gf_list_get(group->adaptation_set->representations, group->active_rep_index);
		if (speed < 0)
			group->decode_only_rap = GF_TRUE;

		max_available_speed = gf_dash_get_max_available_speed(dash, group, active_rep);

		/*verify if this representation support this speed*/
		if (!max_available_speed || (ABS(speed) <= max_available_speed)) {
			//nothing to do
		} else {
			/*if the representation does not support this speed, search for another which support it*/
			u32 switch_to_rep_idx = 0;
			u32 bandwidth = 0, quality = 0, k;
			GF_MPD_Representation *rep;
			for (k=0; k<gf_list_count(group->adaptation_set->representations); k++) {
				rep = gf_list_get(group->adaptation_set->representations, k);
				if ((ABS(speed) <= rep->max_playout_rate) && ((rep->quality_ranking > quality) || (rep->bandwidth > bandwidth))) {
					bandwidth = rep->bandwidth;
					quality = rep->quality_ranking;
					switch_to_rep_idx = k+1;
				}
			}
			if (switch_to_rep_idx) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Switching representation for adapting playing speed\n"));
				group->force_switch_bandwidth = 1;
				group->force_representation_idx_plus_one = switch_to_rep_idx;
			}
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Playing at %f speed \n", speed));
		dash->speed = speed;
		dash->is_rt_speed = (ABS(speed - 1.0)<0.1) ? GF_TRUE : GF_FALSE;

	}
}

GF_EXPORT
GF_Err gf_dash_group_get_segment_duration(GF_DashClient *dash, u32 idx, u32 *dur, u32 *timescale)
{
	GF_MPD_Representation *rep;
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return GF_BAD_PARAM;
	if (!group->adaptation_set) return GF_BAD_PARAM;
	rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);
	if (!rep) return GF_BAD_PARAM;

	*dur = 0;
	*timescale = 0;
	if (rep->segment_template) { *dur = (u32) rep->segment_template->duration; (*timescale) = rep->segment_template->timescale; }
	else if (rep->segment_list) { *dur = (u32) rep->segment_list->duration; *timescale = rep->segment_list->timescale; }

	if (group->adaptation_set->segment_template && ! *dur) *dur = (u32) group->adaptation_set->segment_template->duration;
	else if (group->adaptation_set->segment_list && ! *dur) *dur = (u32) group->adaptation_set->segment_list->duration;

	if (group->adaptation_set->segment_template && ! *timescale) *timescale = group->adaptation_set->segment_template->timescale;
	else if (group->adaptation_set->segment_list && ! *timescale) *timescale = group->adaptation_set->segment_list->timescale;

	if (group->period->segment_template && ! *dur) *dur = (u32) group->period->segment_template->timescale;
	else if (group->period->segment_list && ! *dur) *dur = (u32) group->period->segment_list->timescale;

	if (group->period->segment_template && ! *timescale) *timescale = group->period->segment_template->timescale;
	else if (group->period->segment_list && ! *timescale) *timescale = group->period->segment_list->timescale;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dash_group_next_seg_info(GF_DashClient *dash, u32 group_idx, u32 dependent_representation_index, const char **seg_name, u32 *seg_number, GF_Fraction64 *seg_time, u32 *seg_dur_ms, const char **init_segment)
{
	GF_Err res = GF_OK;
	GF_DASH_Group *group = gf_list_get(dash->groups, group_idx);
	if (!group) return GF_BAD_PARAM;

	if (init_segment) {
		if (group->bs_switching_init_segment_url) {
			*init_segment = group->bs_switching_init_segment_url_name_start;
		} else {
			GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);
			*init_segment = rep ? rep->playback.init_seg_name_start : NULL;
		}
	} else {
		u32 rep_idx = dependent_representation_index;
		if (group->nb_cached_segments <= rep_idx) {
			res = GF_BUFFER_TOO_SMALL;
		} else {
			if (seg_name) *seg_name = group->cached[rep_idx].seg_name_start;
			if (seg_number) *seg_number = group->cached[rep_idx].seg_number;
			if (seg_time) *seg_time = group->cached[rep_idx].time;
			if (seg_dur_ms) *seg_dur_ms = group->cached[rep_idx].duration;
		}
	}
	return res;
}

GF_EXPORT
const char *gf_dash_group_get_representation_id(GF_DashClient *dash, u32 idx)
{
	GF_MPD_Representation *rep;
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return NULL;
	rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);
	return rep->id;
}


GF_EXPORT
void gf_dash_group_discard_segment(GF_DashClient *dash, u32 idx)
{
	GF_DASH_Group *group;
	Bool delete_next;

	group = gf_list_get(dash->groups, idx);

discard_segment:
	if (!group->nb_cached_segments) {
		return;
	}
	delete_next = (group->cached[0].flags & SEG_FLAG_DEP_FOLLOWING) ? GF_TRUE : GF_FALSE;
	assert(group->cached[0].url);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] removing segment %s from list\n", group->cached[0].url));

	//remember the representation index of the last segment
	group->prev_active_rep_index = group->cached[0].representation_index;

	gf_dash_group_reset_cache_entry(&group->cached[0]);

	memmove(&group->cached[0], &group->cached[1], sizeof(segment_cache_entry)*(group->nb_cached_segments-1));
	memset(&(group->cached[group->nb_cached_segments-1]), 0, sizeof(segment_cache_entry));
	group->nb_cached_segments--;

	if (delete_next) {
		goto discard_segment;
	}

	/*if we have dependency representations, we need also discard them*/
	if (group->base_rep_index_plus_one) {
		if (group->cached[0].url && (group->cached[0].representation_index != group->base_rep_index_plus_one-1))
			goto discard_segment;
	}
}

GF_EXPORT
void gf_dash_set_group_done(GF_DashClient *dash, u32 idx, Bool done)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (group) group->done = done;
}

GF_EXPORT
Bool gf_dash_get_group_done(GF_DashClient *dash, u32 idx)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (group) return group->done;
	return GF_FALSE;
}

GF_EXPORT
GF_Err gf_dash_group_get_presentation_time_offset(GF_DashClient *dash, u32 idx, u64 *presentation_time_offset, u32 *timescale)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (group) {
		u64 duration;
		GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);
		gf_mpd_resolve_segment_duration(rep, group->adaptation_set, group->period, &duration, timescale, presentation_time_offset, NULL);
		return GF_OK;
	}
	return GF_BAD_PARAM;
}

GF_EXPORT
GF_Err gf_dash_group_get_next_segment_location(GF_DashClient *dash, u32 idx, u32 dependent_representation_index, const char **url, u64 *start_range, u64 *end_range, s32 *switching_index, const char **switching_url, u64 *switching_start_range, u64 *switching_end_range, const char **original_url, Bool *has_next_segment, const char **key_url, bin128 *key_IV)
{
	GF_DASH_Group *group;
	u32 index;
	Bool has_dep_following;
	*url = NULL;
	if (switching_url) *switching_url = NULL;
	if (start_range) *start_range = 0;
	if (end_range) *end_range = 0;
	if (switching_start_range) *switching_start_range = 0;
	if (switching_end_range) *switching_end_range = 0;
	if (original_url) *original_url = NULL;
	if (switching_index) *switching_index = -1;
	if (has_next_segment) *has_next_segment = GF_FALSE;

	group = gf_list_get(dash->groups, idx);

	if (!group) {
		return GF_BAD_PARAM;
	}

	if (!group->nb_cached_segments) {
		if (group->done) return GF_EOS;
		if ((dash->low_latency_mode==GF_DASH_LL_EARLY_FETCH)
			&& group->is_low_latency
			&& dash->is_rt_speed
			&& !dash->time_in_tsb
		) {
			group->force_early_fetch = GF_TRUE;
			dash->min_wait_ms_before_next_request = 1;
		}
		return GF_BUFFER_TOO_SMALL;
	}

	/*check the dependent rep is in the cache and does not target next segment (next in time)*/
	has_dep_following = (group->cached[0].flags & SEG_FLAG_DEP_FOLLOWING) ? GF_TRUE : GF_FALSE;
	index = 0;
	while (dependent_representation_index) {
		GF_Err err = GF_OK;

		if (has_dep_following) {
			if (index+1 >= group->nb_cached_segments)
				err = GF_BUFFER_TOO_SMALL;
			else if (! (group->cached[index].flags & SEG_FLAG_DEP_FOLLOWING) )
				err = GF_BAD_PARAM;
		} else {
			GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, group->cached[index].representation_index);

			if (index+1 >= group->nb_cached_segments) err = GF_BUFFER_TOO_SMALL;
			else if (!rep->playback.enhancement_rep_index_plus_one) err = GF_BAD_PARAM;
			else if (rep->playback.enhancement_rep_index_plus_one != group->cached[index+1].representation_index + 1) err = GF_BAD_PARAM;
		}

		if (err) {
			return err;
		}
		index ++;
		dependent_representation_index--;
	}

	*url = group->cached[index].url;
	if (start_range)
		*start_range = group->cached[index].start_range;
	if (end_range)
		*end_range = group->cached[index].end_range;
	if (original_url) *original_url = group->cached[index].url;
	if (key_url) *key_url = group->cached[index].key_url;
	if (key_IV) memcpy((*key_IV), group->cached[index].key_IV, sizeof(bin128));

	if (!group->base_rep_index_plus_one && (group->cached[index].representation_index != group->prev_active_rep_index)) {
		GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, group->cached[0].representation_index);
		if (switching_index)
			*switching_index = group->cached[0].representation_index;
		if (switching_start_range)
			*switching_start_range = rep->playback.init_start_range;
		if (switching_end_range)
			*switching_end_range = rep->playback.init_end_range;
		if (switching_url)
			*switching_url = rep->playback.cached_init_segment_url;
	}
	group->force_segment_switch = 0;

	if (has_next_segment) {
		if (group->cached[index].flags & SEG_FLAG_DEP_FOLLOWING) {
			*has_next_segment = GF_TRUE;
		} else if ((index+1<group->max_cached_segments) && group->cached[index+1].url  && group->adaptation_set) {
			GF_MPD_Representation *rep;

			rep = gf_list_get(group->adaptation_set->representations, group->cached[index].representation_index);
			if (rep && (rep->playback.enhancement_rep_index_plus_one == group->cached[index+1].representation_index+1) ) {
				*has_next_segment = GF_TRUE;
			}
		} else if (group->has_pending_enhancement) {
			*has_next_segment = GF_TRUE;
		}
	}
	if (group->cached[index].flags & SEG_FLAG_DISABLED)
		return GF_URL_REMOVED;
	return GF_OK;
}


GF_EXPORT
void gf_dash_seek(GF_DashClient *dash, Double start_range)
{
	Bool is_dynamic = GF_FALSE;

	GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Seek request - playing from %g\n", start_range));

	//are we live ? if so adjust start range
	if (dash->preroll_state == 1) {
		dash->initial_period_tunein = GF_TRUE;
		start_range = 0;
	}
	else if (dash->mpd->type==GF_MPD_TYPE_DYNAMIC) {
		u64 now, availabilityStartTime;
		availabilityStartTime = dash->mpd->availabilityStartTime + dash->utc_shift;
		availabilityStartTime += dash->utc_drift_estimate;

		now = dash->mpd_fetch_time + (gf_sys_clock() - dash->last_update_time) - availabilityStartTime;

		if (dash->initial_time_shift_value<=100) {
			now -= dash->mpd->time_shift_buffer_depth * dash->initial_time_shift_value / 100;
		} else {
			now -= dash->initial_time_shift_value;
		}
//		now += (u64) (start_range*1000);
		start_range = (Double) now;
		start_range /= 1000;

		is_dynamic = GF_TRUE;
		dash->initial_period_tunein = GF_TRUE;
	}

	/*first check if we seek to another period*/
	if (! gf_dash_seek_periods(dash, start_range)) {
		/*if no, seek in group*/
		gf_dash_seek_groups(dash, start_range, is_dynamic);
	}
}

GF_EXPORT
Bool gf_dash_group_segment_switch_forced(GF_DashClient *dash, u32 idx)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	return group->force_segment_switch;
}

GF_EXPORT
void gf_dash_set_utc_shift(GF_DashClient *dash, s32 shift_utc_sec)
{
	if (dash) dash->utc_shift = shift_utc_sec;
}

GF_EXPORT
GF_Err gf_dash_group_get_video_info(GF_DashClient *dash, u32 idx, u32 *max_width, u32 *max_height)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group || !max_width || !max_height) return GF_BAD_PARAM;

	*max_width = group->adaptation_set->max_width;
	*max_height = group->adaptation_set->max_height;
	return GF_OK;
}

GF_EXPORT
Bool gf_dash_group_get_srd_max_size_info(GF_DashClient *dash, u32 idx, u32 *max_width, u32 *max_height)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group || !group->srd_desc || !max_width || !max_height) return GF_FALSE;

	*max_width = group->srd_desc->width;
	*max_height = group->srd_desc->height;
	return GF_TRUE;
}

GF_EXPORT
GF_Err gf_dash_set_min_timeout_between_404(GF_DashClient *dash, u32 min_timeout_between_404)
{
	if (!dash) return GF_BAD_PARAM;
	dash->min_timeout_between_404 = min_timeout_between_404;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dash_set_segment_expiration_threshold(GF_DashClient *dash, u32 expire_after_ms)
{
	if (!dash) return GF_BAD_PARAM;
	dash->segment_lost_after_ms = expire_after_ms;
	return GF_OK;
}

GF_EXPORT
Bool gf_dash_group_loop_detected(GF_DashClient *dash, u32 idx)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group || !group->nb_cached_segments)
		return GF_FALSE;
	return (group->cached[0].flags & SEG_FLAG_LOOP_DETECTED) ? GF_TRUE : GF_FALSE;
}

GF_EXPORT
Double gf_dash_group_get_start_range(GF_DashClient *dash, u32 idx)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return 0.0;
	return group->start_playback_range;
}


GF_EXPORT
Bool gf_dash_is_dynamic_mpd(GF_DashClient *dash)
{
	return (dash && dash->mpd->type==GF_MPD_TYPE_DYNAMIC) ? 1 : 0;
}

GF_EXPORT
u32 gf_dash_get_min_buffer_time(GF_DashClient *dash)
{
	return dash ? dash->mpd->min_buffer_time : 0;
}

#if 0 //unused
GF_Err gf_dash_resync_to_segment(GF_DashClient *dash, const char *latest_segment_name, const char *earliest_segment_name)
{
	Bool found = GF_FALSE;
	u32 i, j, latest_segment_number, earliest_segment_number, start_number;
	/*Double latest_segment_time, earliest_segment_time;*/ //FIX : set but not used
	u64 start_range, end_range, current_dur;
	char *seg_url, *seg_name, *seg_sep;
	GF_MPD_Representation *rep;
	GF_DASH_Group *group = NULL;
	if (!latest_segment_name) return GF_BAD_PARAM;

	seg_url = NULL;
	for (i=0; i<gf_list_count(dash->groups); i++) {
		group = gf_list_get(dash->groups, i);
		for (j=0; j<gf_list_count(group->adaptation_set->representations); j++) {
			GF_Err e;
			rep = gf_list_get(group->adaptation_set->representations, j);
			e = gf_dash_resolve_url(dash->mpd, rep, group, dash->base_url, GF_MPD_RESOLVE_URL_MEDIA_TEMPLATE, i, &seg_url, &start_range, &end_range, &current_dur, NULL, NULL, NULL, NULL, NULL);
			if (e)
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Unable to resolve media template URL: %s\n", gf_error_to_string(e)));

			if (!seg_url) continue;

			seg_sep = NULL;
			seg_name = strstr(seg_url, "://");
			if (seg_name) seg_name = strchr(seg_name+4, '/');
			if (!seg_name) {
				gf_free(seg_url);
				continue;
			}
			seg_sep = strchr(seg_name+1, '$');
			if (seg_sep) seg_sep[0] = 0;

			if (!strncmp(seg_name, latest_segment_name, strlen(seg_name)))
				found = GF_TRUE;

			if (found) break;

			if (seg_sep) seg_sep[0] = '$';
			gf_free(seg_url);
			continue;
		}
		if (found) break;
	}

	if (!found) {
		if (seg_url) gf_free(seg_url);
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] No representation found matching the resync segment name %s\n", latest_segment_name));
		return GF_BAD_PARAM;
	}

	start_number = gf_dash_get_start_number(group, rep);

	if (seg_sep) {
		char *sep_template, *sep_name, c;
		char *latest_template = (char *) (latest_segment_name + strlen(seg_name));
		char *earliest_template = earliest_segment_name ? (char *) (earliest_segment_name + strlen(seg_name)) : NULL;

		latest_segment_number = earliest_segment_number = 0;
		/*latest_segment_time = earliest_segment_time = 0;*/

		seg_sep[0] = '$';
		while (seg_sep) {
			sep_template = strchr(seg_sep+1, '$');
			if (!sep_template) break;
			c = sep_template[1];
			sep_template[1] = 0;
			//solve template for latest
			sep_name = strchr(latest_template, c);
			if (!sep_name) break;

			sep_name[0] = 0;
			if (!strcmp(seg_sep, "$Number$")) {
				latest_segment_number = atoi(latest_template);
			}
			/*else if (!strcmp(seg_sep, "$Time$")) {
				latest_segment_time = atof(latest_template);
			}*/
			sep_name[0] = c;
			latest_template = sep_name;

			//solve template for earliest
			if (earliest_template) {
				sep_name = strchr(earliest_template, c);
				if (!sep_name) break;

				sep_name[0] = 0;
				if (!strcmp(seg_sep, "$Number$")) {
					earliest_segment_number = atoi(earliest_template);
				}
				/*else if (!strcmp(seg_sep, "$Time$")) {
					earliest_segment_time = atof(earliest_template);
				}*/
				sep_name[0] = c;
				earliest_template = sep_name;
			}

			sep_template[1] = c;
			seg_sep = sep_template+1;
			//find next $ - if any, move the segment name of the same amount of chars that what found in the template
			sep_template = strchr(seg_sep, '$');
			if (!sep_template) break;
			sep_template[0]=0;
			latest_template += strlen(sep_template);
			sep_template[0]='$';
		}
		if (earliest_segment_number && !latest_segment_number) {
			latest_segment_number = earliest_segment_number;
		}

		gf_free(seg_url);

		//todo - recompute an AST offset so that the AST of the new segment equals UTC(now) + offset
		if (latest_segment_number) {
			Bool loop_detected = GF_FALSE;
			s32 nb_seg_diff = 0;
			s32 range_in = 0;
			//how many segment ahead are we ?
			nb_seg_diff = start_number + group->download_segment_index;
			nb_seg_diff -= latest_segment_number;

			//we are just too early for this request, do request later
			if (nb_seg_diff == 1 ) {
				//set to false, eg don't increment seg index
				group->segment_in_valid_range = GF_FALSE;
				return GF_OK;
			}

			//if earliest is not given, allow 5 segments
			if (!earliest_segment_number) range_in = 4;
			else range_in = latest_segment_number - earliest_segment_number;

			//loop
			if (latest_segment_number <= start_number ) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Loop in segment start numbers detected - old start %d new seg %d\n", start_number , latest_segment_number));
				loop_detected = GF_TRUE;
			}
			//we are behind live
			else if (nb_seg_diff<0) {
				//we fall in the buffer of the sender, we liklely have a loss
				if (nb_seg_diff + range_in >= 0) {
					group->segment_in_valid_range = GF_TRUE;
					return GF_OK;
				}
				//we are late (something wrong happen locally maybe) - If not too late (5 segs) jump to newest
				else if (earliest_segment_number && (start_number + group->download_segment_index + 5 >= earliest_segment_number)) {
					group->download_segment_index = latest_segment_number - start_number;
					group->segment_in_valid_range = GF_FALSE;
					return GF_OK;
				}
				//we are too late resync...
			}

			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Sync to live was lost - reloading MPD (loop detected %d)\n", loop_detected));
			for (i=0; i< gf_list_count(dash->groups); i++) {
				group = gf_list_get(dash->groups, i);
				//force reinit of timeline for this group
				group->start_number_at_last_ast = 0;
				if (loop_detected)
					group->loop_detected = GF_TRUE;
			}
			dash->force_mpd_update = GF_TRUE;
		}
		return GF_OK;
	}

	//TODO segment list addressing:
	return GF_OK;
}
#endif


GF_EXPORT
GF_Err gf_dash_set_max_resolution(GF_DashClient *dash, u32 width, u32 height, u8 max_display_bpp)
{
	if (dash) {
		dash->max_width = width;
		dash->max_height = height;
		dash->max_bit_per_pixel = max_display_bpp;
		return GF_OK;
	}
	return GF_BAD_PARAM;
}

GF_EXPORT
void gf_dash_debug_groups(GF_DashClient *dash, const u32 *groups_idx, u32 nb_groups)
{
	dash->dbg_grps_index = groups_idx;
	dash->nb_dbg_grps = nb_groups;
}

GF_EXPORT
void gf_dash_split_adaptation_sets(GF_DashClient *dash)
{
	dash->split_adaptation_set = GF_TRUE;
}

GF_EXPORT
void gf_dash_set_low_latency_mode(GF_DashClient *dash, GF_DASHLowLatencyMode low_lat_mode)
{
	dash->low_latency_mode = low_lat_mode;
}


GF_EXPORT
void gf_dash_set_user_buffer(GF_DashClient *dash, u32 buffer_time_ms)
{
	if (dash) dash->user_buffer_ms = buffer_time_ms;
}

/*returns active period start in ms*/
GF_EXPORT
u64 gf_dash_get_period_start(GF_DashClient *dash)
{
	u64 start;
	u32 i;
	GF_MPD_Period *period;
	if (!dash || !dash->mpd) return 0;

	if (dash->preroll_state==1) return 0;

	start = 0;
	for (i=0; i<=dash->active_period_index; i++) {
		period = gf_list_get(dash->mpd->periods, i);
		if (period->is_preroll) continue;
		if (period->start) start = period->start;

		if (i<dash->active_period_index) start += period->duration;
	}
	return start;
}


/*returns active period duration in ms*/
GF_EXPORT
u64 gf_dash_get_period_duration(GF_DashClient *dash)
{
	u64 start;
	u32 i;
	GF_MPD_Period *period = NULL;
	if (!dash || !dash->mpd) return 0;

	start = 0;
	for (i=0; i<=dash->active_period_index; i++) {
		period = gf_list_get(dash->mpd->periods, i);
		if (period->start) start = period->start;
		if (i<dash->active_period_index) start += period->duration;
	}
	if (!period) return 0;
	if (period->duration) return period->duration;
	period = gf_list_get(dash->mpd->periods, dash->active_period_index+1);

	if (!period) {
		//infered from MPD duration
		if (dash->mpd->media_presentation_duration) return dash->mpd->media_presentation_duration - start;
		//duration is not known (live)
		if (dash->mpd->type==GF_MPD_TYPE_STATIC) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Period duration is not computable: last period without duration and no MPD duration !\n"));
		}
		return 0;
	}
	if (!period->start) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Period duration is not computable, paeriod has no duration and next period has no start !\n"));
		return 0;
	}
	return period->start - start;
}

GF_EXPORT
const char *gf_dash_group_get_language(GF_DashClient *dash, u32 idx)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return NULL;
	return group->adaptation_set->lang;
}

GF_EXPORT
u32 gf_dash_group_get_audio_channels(GF_DashClient *dash, u32 idx)
{
	GF_MPD_Descriptor *mpd_desc;
	u32 i=0;
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return 0;

	while ((mpd_desc=gf_list_enum(group->adaptation_set->audio_channels, &i))) {
		if (!strcmp(mpd_desc->scheme_id_uri, "urn:mpeg:dash:23003:3:audio_channel_configuration:2011")) {
			return atoi(mpd_desc->value);
		}
	}
	return 0;
}

GF_EXPORT
u32 gf_dash_group_get_num_qualities(GF_DashClient *dash, u32 idx)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return 0;
	return gf_list_count(group->adaptation_set->representations);
}

GF_EXPORT
u32 gf_dash_group_get_num_components(GF_DashClient *dash, u32 idx)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return 0;
	return gf_list_count(group->adaptation_set->content_component);
}

GF_EXPORT
char *gf_dash_group_get_template(GF_DashClient *dash, u32 idx)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	GF_MPD_Representation *rep;
	const char *tpl;
	char *solved_template;
	if (!group) return NULL;
	rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);
	if (!rep)
		rep = gf_list_get(group->adaptation_set->representations, 0);

	tpl = NULL;
	if (rep && rep->segment_template) tpl = rep->segment_template->media;
	if (!tpl && group->adaptation_set && group->adaptation_set->segment_template) tpl = group->adaptation_set->segment_template->media;
	if (!tpl && group->period && group->period->segment_template) tpl = group->period->segment_template->media;

	if (tpl) {
		u64 range_start, range_end, segment_duration_in_ms;
		gf_mpd_resolve_url(dash->mpd, rep, group->adaptation_set, group->period, "", 0, GF_MPD_RESOLVE_URL_MEDIA_TEMPLATE_NO_BASE, 0, 0, &solved_template, &range_start, &range_end, &segment_duration_in_ms, NULL, NULL, NULL, NULL);
		return solved_template;
	}
	if (dash->is_m3u8 && rep) {
		char *ext;
		u32 i, len, last_num, last_non_num;
		GF_MPD_SegmentURL *first_seg = gf_list_get(rep->segment_list->segment_URLs, 0);
		//GF_MPD_SegmentURL *last_seg = gf_list_last(rep->segment_list->segment_URLs);
		if (!first_seg) return NULL;
		if (!first_seg->media) return NULL;
		if (first_seg->media_range) return NULL;

		solved_template = NULL;
		ext = strrchr(first_seg->media, '.');
		if (ext) ext[0] = 0;
		gf_dynstrcat(&solved_template, first_seg->media, NULL);
		if (ext) ext[0] = '.';
		len = (u32) strlen(solved_template);
		last_num = last_non_num = 0;
		for (i=len; i>0; i--) {
			if (isdigit(solved_template[i-1])) {
				if (!last_num) last_num = i-1;
			} else {
				if (last_num) {
					last_non_num = i-1;
					break;
				}
			}
		}
		if (!last_non_num || (last_num>=last_non_num+1)) {
			u32 num;
			char szVal[100];
			solved_template[last_num] = 0;
			num = atoi(solved_template+last_non_num+1);
			snprintf(szVal, 100, "%u", num);
			len = (u32) strlen(szVal);
			if (len < last_num - last_non_num) {
				u32 pad = last_num - last_non_num - len;
				snprintf(szVal, 100, "$Number%%0%dd$", pad);
				gf_dynstrcat(&solved_template, szVal, NULL);
			} else {
				gf_dynstrcat(&solved_template, "$Number$", NULL);
			}
			gf_dynstrcat(&solved_template, first_seg->media + last_num + 1, NULL);
			return solved_template;
		}

	}

	return NULL;
}


GF_EXPORT
GF_Err gf_dash_group_get_quality_info(GF_DashClient *dash, u32 idx, u32 quality_idx, GF_DASHQualityInfo *quality)
{
	GF_MPD_Fractional *sar;
	u32 timescale = 0;
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	GF_MPD_Representation *rep;
	if (!group || !quality) return GF_BAD_PARAM;
	rep = gf_list_get(group->adaptation_set->representations, quality_idx);
	if (!rep) return GF_BAD_PARAM;

	memset(quality, 0, sizeof(GF_DASHQualityInfo));
	quality->mime = rep->mime_type ? rep->mime_type : group->adaptation_set->mime_type;
	quality->codec = rep->codecs ? rep->codecs : group->adaptation_set->codecs;
	quality->disabled = rep->playback.disabled;
	sar = rep->framerate ? rep->framerate : group->adaptation_set->framerate;
	if (sar) {
		quality->fps_den = sar->den;
		quality->fps_num = sar->num;
	}
	quality->height = rep->height ? rep->height : group->adaptation_set->height;
	quality->width = rep->width ? rep->width : group->adaptation_set->width;
	quality->nb_channels = gf_dash_group_get_audio_channels(dash, idx);
	sar = rep->sar ? rep->sar : group->adaptation_set->sar;
	if (sar) {
		quality->par_num = sar->num;
		quality->par_den = sar->den;
	}
	quality->sample_rate = rep->samplerate ? rep->samplerate : group->adaptation_set->samplerate;
	quality->bandwidth = rep->bandwidth;
	quality->ID = rep->id;
	quality->interlaced = (rep->scan_type == GF_MPD_SCANTYPE_INTERLACED) ? 1 : ( (group->adaptation_set->scan_type == GF_MPD_SCANTYPE_INTERLACED) ? 1 : 0);

	if (group->was_segment_base && rep->segment_list)
		quality->seg_urls = rep->segment_list->segment_URLs;

	//scalable rep, selected quality is max_complementary_rep_index
	if (group->base_rep_index_plus_one) {
		quality->is_selected = (quality_idx==group->max_complementary_rep_index) ? 1 : 0;
	} else {
		quality->is_selected = (quality_idx==group->active_rep_index) ? 1 : 0;
	}

	if (rep->segment_template) {
		if (!quality->ast_offset) quality->ast_offset = rep->segment_template->availability_time_offset;
		if (!timescale) timescale = rep->segment_template->timescale;
		if (!quality->average_duration) quality->average_duration = (Double) rep->segment_template->duration;
	}
	if (group->adaptation_set->segment_template) {
		if (!quality->ast_offset) quality->ast_offset = group->adaptation_set->segment_template->availability_time_offset;
		if (!timescale) timescale = group->adaptation_set->segment_template->timescale;
		if (!quality->average_duration) quality->average_duration =  (Double) group->adaptation_set->segment_template->duration;
	}
	if (group->period->segment_template) {
		if (!quality->ast_offset) quality->ast_offset = group->period->segment_template->availability_time_offset;
		if (!timescale) timescale = group->period->segment_template->timescale;
		if (!quality->average_duration) quality->average_duration =  (Double) group->period->segment_template->duration;
	}

	if (timescale) {
		quality->average_duration /= timescale;
	} else {
		quality->average_duration = 0;
	}
	return GF_OK;
}


static Bool gf_dash_group_enum_descriptor_list(GF_DashClient *dash, u32 idx, GF_List *descs, const char **desc_id, const char **desc_scheme, const char **desc_value)
{
	GF_MPD_Descriptor *mpd_desc;
	if (idx>=gf_list_count(descs)) return 0;
	mpd_desc = gf_list_get(descs, idx);
	if (desc_value) *desc_value = mpd_desc->value;
	if (desc_scheme) *desc_scheme = mpd_desc->scheme_id_uri;
	if (desc_id) *desc_id = mpd_desc->id;
	return 1;
}

GF_EXPORT
Bool gf_dash_group_enum_descriptor(GF_DashClient *dash, u32 group_idx, GF_DashDescriptorType  desc_type, u32 desc_idx, const char **desc_id, const char **desc_scheme, const char **desc_value)
{
	GF_List *descs = NULL;
	GF_DASH_Group *group = gf_list_get(dash->groups, group_idx);
	if (!group) return 0;
	switch (desc_type) {
	case GF_MPD_DESC_ACCESSIBILITY:
		descs = group->adaptation_set->accessibility;
		break;
	case GF_MPD_DESC_AUDIOCONFIG:
		descs = group->adaptation_set->audio_channels;
		break;
	case GF_MPD_DESC_CONTENT_PROTECTION:
		descs = group->adaptation_set->content_protection;
		break;
	case GF_MPD_DESC_ESSENTIAL_PROPERTIES:
		descs = group->adaptation_set->essential_properties;
		break;
	case GF_MPD_DESC_SUPPLEMENTAL_PROPERTIES:
		descs = group->adaptation_set->supplemental_properties;
		break;
	case GF_MPD_DESC_FRAME_PACKING:
		descs = group->adaptation_set->frame_packing;
		break;
	case GF_MPD_DESC_ROLE:
		descs = group->adaptation_set->role;
		break;
	case GF_MPD_DESC_RATING:
		descs = group->adaptation_set->rating;
		break;
	case GF_MPD_DESC_VIEWPOINT:
		descs = group->adaptation_set->viewpoint;
		break;
	default:
		return 0;
	}
	return gf_dash_group_enum_descriptor_list(dash, desc_idx, descs, desc_id, desc_scheme, desc_value);
}

GF_EXPORT
Bool gf_dash_get_automatic_switching(GF_DashClient *dash)
{
	return (dash && dash->disable_switching) ? GF_FALSE : GF_TRUE;
}

GF_EXPORT
GF_Err gf_dash_set_automatic_switching(GF_DashClient *dash, Bool enable_switching)
{
	if (!dash) return GF_BAD_PARAM;
	dash->disable_switching = !enable_switching;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dash_group_select_quality(GF_DashClient *dash, u32 idx, const char *ID, u32 q_idx)
{
	u32 i, count;
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return GF_BAD_PARAM;

	if (!ID) {
		GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, q_idx);
		if (!rep) return GF_BAD_PARAM;
		group->force_representation_idx_plus_one = q_idx+1;
		group->force_switch_bandwidth = 1;
		return GF_OK;
	}

	count = gf_list_count(group->adaptation_set->representations);
	for (i=0; i<count; i++) {
		GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, i);
		if (rep->id && !strcmp(rep->id, ID)) {
			group->force_representation_idx_plus_one = i+1;
			group->force_switch_bandwidth = 1;
			return GF_OK;
		}
	}
	return GF_BAD_PARAM;
}

GF_EXPORT
s32 gf_dash_group_get_active_quality(GF_DashClient *dash, u32 idx)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return -1;
	return group->active_rep_index;
}

GF_EXPORT
GF_Err gf_dash_set_timeshift(GF_DashClient *dash, u32 ms_in_timeshift)
{
	if (!dash) return GF_BAD_PARAM;
	dash->initial_time_shift_value = ms_in_timeshift;
	return GF_OK;
}

GF_EXPORT
Double gf_dash_get_timeshift_buffer_pos(GF_DashClient *dash)
{
	return dash ? dash->prev_time_in_tsb / 1000.0 : 0.0;
}

GF_EXPORT
void gf_dash_group_set_codec_stat(GF_DashClient *dash, u32 idx, u32 avg_dec_time, u32 max_dec_time, u32 irap_avg_dec_time, u32 irap_max_dec_time, Bool codec_reset, Bool decode_only_rap)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return;
	group->avg_dec_time = avg_dec_time;
	group->max_dec_time = max_dec_time;
	group->irap_avg_dec_time = irap_avg_dec_time;
	group->irap_max_dec_time = irap_max_dec_time;
	group->codec_reset = codec_reset;
	group->decode_only_rap = decode_only_rap;
}

GF_EXPORT
void gf_dash_group_set_buffer_levels(GF_DashClient *dash, u32 idx, u32 buffer_min_ms, u32 buffer_max_ms, u32 buffer_occupancy_ms)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return;
	group->buffer_min_ms = buffer_min_ms;
	group->buffer_max_ms = buffer_max_ms;
	if (group->max_buffer_playout_ms > buffer_max_ms) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Max buffer %d less than max playout buffer %d, overwriting max playout buffer\n", buffer_max_ms, group->max_buffer_playout_ms));
		group->max_buffer_playout_ms = buffer_max_ms;
	}
	group->buffer_occupancy_ms = buffer_occupancy_ms;
}


GF_EXPORT
void gf_dash_disable_speed_adaptation(GF_DashClient *dash, Bool disable)
{
	dash->disable_speed_adaptation = disable;
}

GF_EXPORT
void gf_dash_override_ntp(GF_DashClient *dash, u64 server_ntp)
{
	if (server_ntp) {
		dash->utc_drift_estimate = gf_net_get_ntp_diff_ms(server_ntp);
		dash->ntp_forced = 1;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Overwriting local NTP "LLU" to given one "LLU"\n", gf_net_get_ntp_ts(), server_ntp));
	} else {
		dash->utc_drift_estimate = 0;
		dash->ntp_forced = 0;
	}
}

GF_EXPORT
s32 gf_dash_get_utc_drift_estimate(GF_DashClient *dash) {
	return (s32) dash->utc_drift_estimate;
}

GF_EXPORT
GF_DASHTileAdaptationMode gf_dash_get_tile_adaptation_mode(GF_DashClient *dash)
{
	return dash->tile_adapt_mode;
}

GF_EXPORT
void gf_dash_set_tile_adaptation_mode(GF_DashClient *dash, GF_DASHTileAdaptationMode mode, u32 tile_rate_decrease)
{
	u32 i;
	dash->tile_adapt_mode = mode;
	dash->tile_rate_decrease = (tile_rate_decrease<100) ? tile_rate_decrease : 100;
	for (i=0; i<gf_list_count(dash->groups); i++) {
		GF_DASH_Group *group = gf_list_get(dash->groups, i);
		if (group->srd_desc) gf_dash_set_tiles_quality(dash, group->srd_desc, GF_TRUE);
	}
}

GF_EXPORT
void gf_dash_disable_low_quality_tiles(GF_DashClient *dash, Bool disable_tiles)
{
	dash->disable_low_quality_tiles = disable_tiles;
}

GF_EXPORT
void gf_dash_set_chaining_mode(GF_DashClient *dash, u32 chaining_mode)
{
	dash->chaining_mode = chaining_mode;
}


GF_EXPORT
Bool gf_dash_group_get_srd_info(GF_DashClient *dash, u32 idx, u32 *srd_id, u32 *srd_x, u32 *srd_y, u32 *srd_w, u32 *srd_h, u32 *srd_width, u32 *srd_height)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group || !group->srd_desc) return GF_FALSE;

	if (group->srd_desc) {
		if (srd_id) (*srd_id) = group->srd_desc->id;
		if (srd_width) (*srd_width) = group->srd_desc->srd_fw;
		if (srd_height) (*srd_height) = group->srd_desc->srd_fh;
	}

	if (srd_x) (*srd_x) = group->srd_x;
	if (srd_y) (*srd_y) = group->srd_y;
	if (srd_w) (*srd_w) = group->srd_w;
	if (srd_h) (*srd_h) = group->srd_h;


	return GF_TRUE;
}

GF_EXPORT
void gf_dash_ignore_xlink(GF_DashClient *dash, Bool ignore_xlink)
{
	dash->ignore_xlink = ignore_xlink;
}

GF_EXPORT
void gf_dash_set_route_ast_shift(GF_DashClient *dash, u32 ast_shift)
{
	dash->route_ast_shift = ast_shift;
}

GF_EXPORT
GF_Err gf_dash_group_set_max_buffer_playout(GF_DashClient *dash, u32 idx, u32 max_buffer_playout_ms)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return GF_BAD_PARAM;
	group->max_buffer_playout_ms = max_buffer_playout_ms;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dash_group_set_quality_degradation_hint(GF_DashClient *dash, u32 idx, u32 quality_degradation_hint)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return GF_BAD_PARAM;

	group->quality_degradation_hint = quality_degradation_hint;
	if (group->quality_degradation_hint > 100) group->quality_degradation_hint=100;
	return GF_OK;
}


GF_EXPORT
GF_Err gf_dash_group_set_visible_rect(GF_DashClient *dash, u32 idx, u32 min_x, u32 max_x, u32 min_y, u32 max_y, Bool is_gaze)
{
	u32 i, count;
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return GF_BAD_PARAM;

	if (!min_x && !max_x && !min_y && !max_y) {
		group->quality_degradation_hint = 0;
	}

	//for both regular or tiled, store visible width/height
	group->hint_visible_width = max_x - min_x;
	group->hint_visible_height = max_y - min_y;

	if (!group->groups_depending_on) return GF_OK;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Group Visible rect %d,%d,%d,%d \n", min_x, max_x, min_y, max_y));
	count = gf_list_count(group->groups_depending_on);
	for (i=0; i<count; i++) {
		Bool is_visible = GF_TRUE;
		GF_DASH_Group *a_group = gf_list_get(group->groups_depending_on, i);
		u32 old_hint;
		if (!a_group->srd_w || !a_group->srd_h) continue;

		old_hint = a_group->quality_degradation_hint;
		if (is_gaze) {

			if (min_x < a_group->srd_x)
				is_visible = GF_FALSE;
			else if (min_x > a_group->srd_x + a_group->srd_w)
				is_visible = GF_FALSE;
			else if (min_y < a_group->srd_y)
				is_visible = GF_FALSE;
			else if (min_y > a_group->srd_y + a_group->srd_h)
				is_visible = GF_FALSE;

		} else {

			//single rectangle case
			if (min_x<max_x) {
				if (a_group->srd_x+a_group->srd_w <min_x) is_visible = GF_FALSE;
				else if (a_group->srd_x>max_x) is_visible = GF_FALSE;
			} else {
				if ( (a_group->srd_x>max_x) && (a_group->srd_x+a_group->srd_w<min_x)) is_visible = GF_FALSE;
			}

			if (a_group->srd_y>max_y) is_visible = GF_FALSE;
			else if (a_group->srd_y+a_group->srd_h < min_y) is_visible = GF_FALSE;

		}
		a_group->quality_degradation_hint = is_visible ? 0 : 100;
		if (old_hint != a_group->quality_degradation_hint) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Group SRD %d,%d,%d,%d is %s\n", a_group->srd_x, a_group->srd_w, a_group->srd_y, a_group->srd_h, is_visible ? "visible" : "hidden"));

			//remember to update tile quality for non-custom algo
			a_group->update_tile_qualities = GF_TRUE;
			group->update_tile_qualities = GF_TRUE;
		}
	}
	return GF_OK;
}

void gf_dash_set_group_download_state(GF_DashClient *dash, u32 idx, u32 cur_dep_idx, GF_Err err)
{
	GF_MPD_Representation *rep;
	Bool has_dep_following;
	u32 resume_from_dep_group, i;
	char *key_url, *url;
	GF_DASH_Group *base_group;
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return;

	//we forced early fetch because demux was empty, consider all errors as 404
	if (group->force_early_fetch && err) {
		err = GF_URL_ERROR;
	}

	if (!err) {
		group->force_early_fetch = GF_FALSE;
		return;
	}
	if (!group->nb_cached_segments) return;
	rep = gf_list_get(group->adaptation_set->representations, group->cached[0].representation_index);

	while (group->cached[0].representation_index < cur_dep_idx) {
		gf_free(group->cached[0].key_url);
		gf_free(group->cached[0].url);
		group->nb_cached_segments--;
		memmove(&group->cached[0], &group->cached[1], sizeof(segment_cache_entry) * group->nb_cached_segments);
		if (!group->nb_cached_segments)
			break;
	}
	has_dep_following = (group->cached[0].flags & SEG_FLAG_DEP_FOLLOWING) ? GF_TRUE : GF_FALSE;

	//detach URL and key URL, they will be freed in on_group_download_error below
	key_url = group->cached[0].key_url;
	url = group->cached[0].url;
	group->cached[0].url = NULL;
	group->cached[0].key_url = NULL;
	//remember for which dependent group we failed
	resume_from_dep_group = group->cached[0].dep_group_idx;

	base_group = group;
	while (base_group->depend_on_group) {
		base_group = base_group->depend_on_group;
	}

	//rewind segment index for all following dep groups
	for (i=1; i < group->nb_cached_segments; i++) {
		if (!group->cached[i].dep_group_idx) break;
		GF_DASH_Group *dep_grp = gf_list_get(base_group->groups_depending_on, group->cached[i].dep_group_idx - 1);
		if (!dep_grp) break;

		if (dash->speed>=0) {
			dep_grp->download_segment_index--;
		} else {
			dep_grp->download_segment_index++;
		}
	}

	//reset cache but do not reset timeline nor LLHLS live chunk
	while (group->nb_cached_segments) {
		group->nb_cached_segments--;
		gf_dash_group_reset_cache_entry(&group->cached[group->nb_cached_segments]);
	}

	//restore current dependent group - if the error triggers moving to next segment, this will be reseted
	if (group->groups_depending_on) {
		group->current_dep_idx = resume_from_dep_group;
	}

	on_group_download_error(dash, group, base_group, err, rep, url, key_url, has_dep_following);

	//error (segment skipped), we assume time alignment of dependent groups
	if (group->groups_depending_on && !group->current_dep_idx) {
		for (i=0; i<gf_list_count(group->groups_depending_on); i++) {
			GF_DASH_Group *dep_grp = gf_list_get(base_group->groups_depending_on, i);
			dep_grp->download_segment_index = group->download_segment_index;
		}
	}
}

void gf_dash_group_store_stats(GF_DashClient *dash, u32 idx, u32 dep_rep_idx, u32 bytes_per_sec, u64 file_size, Bool is_broadcast, u64 us_since_start)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return;
	if (!group->nb_cached_segments) return;

	if (group->groups_depending_on) {
		Bool is_last = (dep_rep_idx == gf_list_count(group->groups_depending_on)) ? GF_TRUE : GF_FALSE;
		if (dep_rep_idx) {
			group = gf_list_get(group->groups_depending_on, dep_rep_idx-1);
			if (!group)
				return;
		}
		dash_store_stats(dash, group, bytes_per_sec, (u32) file_size, is_broadcast, dep_rep_idx, us_since_start);

		if (is_last)
			dash_global_rate_adaptation(dash, GF_FALSE);
	} else {
		dash_store_stats(dash, group, bytes_per_sec, (u32) file_size, is_broadcast, dep_rep_idx, us_since_start);

		dash_global_rate_adaptation(dash, GF_FALSE);
	}
}

u32 gf_dash_get_min_wait_ms(GF_DashClient *dash)
{
	if (dash && dash->min_wait_ms_before_next_request) {
		u32 ellapsed = gf_sys_clock() - dash->min_wait_sys_clock;
		if (ellapsed < dash->min_wait_ms_before_next_request) dash->min_wait_ms_before_next_request -= ellapsed;
		else dash->min_wait_ms_before_next_request = 0;
		return dash->min_wait_ms_before_next_request;
	}
	return 0;
}

GF_EXPORT
Bool gf_dash_all_groups_done(GF_DashClient *dash)
{
	u32 i, count = gf_list_count(dash->groups);
	for (i=0; i<count; i++) {
		GF_DASH_Group *group = gf_list_get(dash->groups, i);
		if (group->selection != GF_DASH_GROUP_SELECTED) continue;
		if (!group->done) return GF_FALSE;
		if (group->nb_cached_segments) return GF_FALSE;
	}
	return GF_TRUE;
}

GF_EXPORT
void gf_dash_set_period_xlink_query_string(GF_DashClient *dash, const char *query_string)
{
	if (dash) dash->query_string = query_string;
}

GF_EXPORT
s32 gf_dash_group_get_as_id(GF_DashClient *dash, u32 group_idx)
{
	GF_DASH_Group *group;
	if (!dash) return 0;
	group = gf_list_get(dash->groups, group_idx);
	if (!group) return 0;
	return group->adaptation_set->id;
}


GF_Err gf_dash_group_push_tfrf(GF_DashClient *dash, u32 group_idx, void *_tfrf, u32 timescale)
{
	GF_MSSTimeRefBox *tfrf = (GF_MSSTimeRefBox *)_tfrf;
	u32 i;
	GF_MPD_SegmentTemplate *stpl;
	GF_DASH_Group *group;
	if (!dash) return GF_BAD_PARAM;
	if (!dash->is_smooth) return GF_OK;
	if (dash->mpd->type != GF_MPD_TYPE_DYNAMIC) return GF_OK;

	group = gf_list_get(dash->groups, group_idx);
	if (!group) return GF_BAD_PARAM;
	if (!group->adaptation_set || !group->adaptation_set->segment_template || !group->adaptation_set->segment_template->segment_timeline)
		return GF_BAD_PARAM;

	stpl = group->adaptation_set->segment_template;

	for (i=0; i<tfrf->frags_count; i++) {
		u32 k, nb_segs;
		Bool exists = GF_FALSE;
		GF_MPD_SegmentTimelineEntry *se = NULL;
		u64 start = 0;
		u64 frag_time = tfrf->frags[i].absolute_time_in_track_timescale;
		u64 frag_dur = tfrf->frags[i].fragment_duration_in_track_timescale;
		if (timescale != stpl->timescale) {
			frag_time *= stpl->timescale;
			frag_time /= timescale;
			frag_dur *= stpl->timescale;
			frag_dur /= timescale;
		}
		nb_segs = gf_list_count(stpl->segment_timeline->entries);
		for (k=0; k<nb_segs; k++) {
			u64 se_dur;
			se = gf_list_get(stpl->segment_timeline->entries, k);
			if (se->start_time)
				start = se->start_time;
			se_dur = se->duration * (1+se->repeat_count);
			if (start + se_dur <= frag_time) {
				start += se_dur;
				continue;
			}
			exists = GF_TRUE;
			break;
		}
		if (!exists) {
			if (se && se->duration==frag_dur) {
				se->repeat_count++;
			} else {
				GF_SAFEALLOC(se, GF_MPD_SegmentTimelineEntry);
				if (frag_time != start)
					se->start_time = frag_time;
				se->duration = (u32) frag_dur;
				gf_list_add(stpl->segment_timeline->entries, se);
			}
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Smooth push new fragment start "LLU" dur "LLU" (inserted at start_time "LLU")\n", frag_time, frag_dur, start));
			group->nb_segments_in_rep++;
		}
	}
	return GF_OK;
}

#endif //GPAC_DISABLE_DASH_CLIENT

