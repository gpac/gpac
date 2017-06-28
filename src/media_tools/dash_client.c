/**
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre, Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2010-
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

#include <gpac/thread.h>
#include <gpac/network.h>
#include <gpac/dash.h>
#include <gpac/internal/mpd.h>
#include <gpac/internal/m3u8.h>
#include <gpac/internal/isomedia_dev.h>
#include <gpac/base_coding.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32_WCE
#include <winbase.h>
#else
#include <time.h>
#endif

#include <math.h>


#ifndef GPAC_DISABLE_DASH_CLIENT

/*ISO 639 languages*/
#include <gpac/iso639.h>

/*set to 1 if you want MPD to use SegmentTemplate if possible instead of SegmentList*/
#define M3U8_TO_MPD_USE_TEMPLATE	0

typedef enum {
	GF_DASH_STATE_STOPPED = 0,
	/*period setup and playback chain creation*/
	GF_DASH_STATE_SETUP,
	/*request to start playback chain*/
	GF_DASH_STATE_CONNECTING,
	GF_DASH_STATE_RUNNING,
} GF_DASH_STATE;


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
	Bool keep_files, disable_switching, allow_local_mpd_update, enable_buffering, estimate_utc_drift, ntp_forced;
	Bool is_m3u8, is_smooth;

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

	/* active period in MPD (only one currently supported) */
	u32 active_period_index;
	u32 request_period_switch;

	Bool next_period_checked;

	u64 start_time_in_active_period;

	Bool ignore_mpd_duration;
	u32 initial_time_shift_value;

	/*list of groups in the active period*/
	GF_List *groups;

	/*Main Thread handling segment downloads and MPD/M3U8 update*/
	GF_Thread *dash_thread;
	/*mutex for MPD updates and group access*/
	GF_Mutex *dash_mutex;

	/* one of the above state*/
	GF_DASH_STATE dash_state;
	Bool mpd_stop_request;
	Bool in_period_setup;

	u32 nb_buffering;
	u32 idle_interval;

	s32 utc_drift_estimate;
	s32 utc_shift;

	Double start_range_period;

	Double speed;
	u32 probe_times_before_switch;
	Bool agressive_switching;
	u32 min_wait_ms_before_next_request;

	Bool force_mpd_update;

	u32 user_buffer_ms;

	u32 min_timeout_between_404, segment_lost_after_ms;

	Bool use_threaded_download;

	//in ms
	u32 time_in_tsb, prev_time_in_tsb;
	u32 tsb_exceeded;
	s32 debug_group_index;
	Bool disable_speed_adaptation;

	u32 tile_rate_decrease;
	GF_DASHTileAdaptationMode tile_adapt_mode;

	GF_List *SRDs;

	GF_DASHAdaptationAlgorithm adaptation_algorithm;

	s32 (*rate_adaptation_algo)(GF_DashClient *dash, GF_DASH_Group *group, GF_DASH_Group *base_group,
												  u32 dl_rate, Double speed, Double max_available_speed, Bool force_lower_complexity,
												  GF_MPD_Representation *rep, Bool go_up_bitrate);

	GF_Err (*rate_adaptation_download_monitor)(GF_DashClient *dash, GF_DASH_Group *group);
};

static void gf_dash_seek_group(GF_DashClient *dash, GF_DASH_Group *group, Double seek_to, Bool is_dynamic);


typedef struct
{
	char *cache;
	char *url;
	u64 start_range, end_range;
	/*representation index in adaptation_set->representations*/
	u32 representation_index;
	Bool loop_detected;
	u32 duration;
	char *key_url;
	bin128 key_IV;
	Bool has_dep_following;
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

	GF_DASHGroupSelection selection;

	/*may be mpd@time_shift_buffer_depth or rep@time_shift_buffer_depth*/
	u32 time_shift_buffer_depth;

	Bool bitstream_switching, dont_delete_first_segment;
	GF_DASH_Group *depend_on_group;
	Bool done;
	//if set, will redownload the last segment partially downloaded
	Bool force_switch_bandwidth;
	Bool min_bandwidth_selected;
	u32 download_start_time;
	u32 active_bitrate, max_bitrate, min_bitrate;
	u32 min_representation_bitrate;

	u32 nb_segments_in_rep;

	/* Segment duration as advertised in the MPD
	   for the real duration of the segment being downloaded see current_downloaded_segment_duration */
	Double segment_duration;

	Double start_playback_range;

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

	/*next file (cached) to delete at next GF_NET_SERVICE_QUERY_NEXT for this group*/
	char * urlToDeleteNext;
	volatile u32 max_cached_segments, nb_cached_segments, max_buffer_segments;
	segment_cache_entry *cached;

	GF_DASHFileIOSession segment_download;
	//0: not set, 1: abort because group has been stopped - 2: abort because bandwidth was too low
	u32 download_abort_type;
	/*usually 0-0 (no range) but can be non-zero when playing local MPD/DASH sessions*/
	u64 bs_switching_init_segment_url_start_range, bs_switching_init_segment_url_end_range;
	char *bs_switching_init_segment_url;

	u32 nb_segments_done;
	u32 last_segment_time;
	u32 nb_segments_since_switch;

	//stats of last downloaded segment
	u32 total_size, bytes_per_sec;


	Bool segment_must_be_streamed;
	Bool broken_timing;
	Bool buffering;
	u32 maybe_end_of_stream;
	u32 cache_duration;
	u32 time_at_first_reload_required;
	u32 force_representation_idx_plus_one;

	Bool force_segment_switch;
	Bool is_downloading;
	Bool loop_detected;

	u32 time_at_first_failure;
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
	u32 force_max_rep_index;
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
	u64 hls_next_start_time;

	GF_List *groups_depending_on;
	u32 current_dep_idx;

	u32 target_new_rep;

	u32 srd_x, srd_y, srd_w, srd_h, srd_row_idx, srd_col_idx;
	struct _dash_srd_desc *srd_desc;

	/*mutex for group->cache access (read and write in download)*/
	GF_Mutex *cache_mutex;

	GF_Thread *download_th;
	Bool download_th_done;

	/*current index of the base URL used*/
	u32 current_base_url_idx;

	u32 quality_degradation_hint;

	Bool rate_adaptation_postponed;

	/* current segment index in BBA and BOLA algorithm */
	u32 current_index;
};

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

static void gf_dash_buffer_off(GF_DASH_Group *group)
{
	if (!group->dash->enable_buffering) return;
	if (group->buffering) {
		assert(group->dash->nb_buffering);
		group->dash->nb_buffering--;
		if (!group->dash->nb_buffering) {
			group->dash->dash_io->on_dash_event(group->dash->dash_io, GF_DASH_EVENT_BUFFER_DONE, -1, GF_OK);
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Session buffering done\n"));
		}
		group->buffering = GF_FALSE;
	}
}

static void gf_dash_buffer_on(GF_DASH_Group *group)
{
	if (!group->dash->enable_buffering) return;

	if (!group->buffering) {
		if (!group->dash->nb_buffering) {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Starting session buffering\n"));
		}
		group->dash->nb_buffering++;
		group->buffering = GF_TRUE;
	}
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
	if (!group->base_rep_index_plus_one || (group->base_rep_index_plus_one == group->force_max_rep_index+1))
		return nb_rep_need; // we need to download only one representation
	rep = gf_list_get(group->adaptation_set->representations, group->base_rep_index_plus_one-1);
	next_rep_index_plus_one = rep->enhancement_rep_index_plus_one;
	while ((nb_rep_need < count) && rep->enhancement_rep_index_plus_one) {
		nb_rep_need++;
		if (next_rep_index_plus_one == group->force_max_rep_index+1)
			break;
		rep = gf_list_get(group->adaptation_set->representations, next_rep_index_plus_one-1);
		next_rep_index_plus_one = rep->enhancement_rep_index_plus_one;
	}

	assert(nb_rep_need <= count);

	return nb_rep_need;
}

GF_EXPORT
void gf_dash_get_buffer_info(GF_DashClient *dash, u32 *total_buffer, u32 *media_buffered)
{
	u32 nb_buffering = 0;
	if (dash->nb_buffering) {
		u32 i, j, nb_groups;
		*total_buffer = 0;
		*media_buffered = 0;
		nb_groups = gf_list_count(dash->groups);
		for (i=0; i<nb_groups; i++) {
			GF_DASH_Group *group = gf_list_get(dash->groups, i);
			if (group->buffering) {
				u32 buffer = 0;
				*total_buffer += (u32) (group->segment_duration*group->max_buffer_segments*1000);
				for (j=0; j<group->nb_cached_segments; j++) {
					buffer += group->cached[j].duration;
				}
				*media_buffered += buffer;
				nb_buffering += gf_dash_group_count_rep_needed(group);
			}
		}
		if (*media_buffered > *total_buffer)
			*media_buffered  = *total_buffer;
		if (nb_buffering) {
			*total_buffer /= nb_buffering;
			*media_buffered /= nb_buffering;
		}
	}
}

static void gf_dash_update_buffering(GF_DASH_Group *group, GF_DashClient *dash)
{
	if (dash->nb_buffering) {
		dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_BUFFERING, -1, GF_OK);

		if (group->cached[0].duration && group->nb_cached_segments>=group->max_buffer_segments)
			gf_dash_buffer_off(group);
	}
}


GF_EXPORT
Bool gf_dash_check_mpd_root_type(const char *local_url)
{
	if (local_url) {
		char *rtype = gf_xml_get_root_type(local_url, NULL);
		if (rtype) {
			Bool handled = GF_FALSE;
			if (!strcmp(rtype, "MPD")) {
				handled = GF_TRUE;
			}
			gf_free(rtype);
			return handled;
		}
	}
	return GF_FALSE;
}


static void gf_dash_group_timeline_setup(GF_MPD *mpd, GF_DASH_Group *group, u64 fetch_time)
{
	GF_MPD_SegmentTimeline *timeline = NULL;
	GF_MPD_Representation *rep = NULL;
	u32 shift, timescale;
	u64 current_time, current_time_no_timeshift, availabilityStartTime;
	u32 ast_diff, start_number;
	Double ast_offset = 0;

	if (mpd->type==GF_MPD_TYPE_STATIC)
		return;

	/*M3U8 does not use NTP sync */
	if (group->dash->is_m3u8)
		return;

	if (group->broken_timing )
		return;


	/*if no AST, do not use NTP sync */
	if (! group->dash->mpd->availabilityStartTime) {
		group->broken_timing = GF_TRUE;
		return;
	}

	if (!fetch_time) {
		//when we initialize the timeline without an explicit fetch time, use our local clock - this allows for better precision
		//when trying to locate the live edge
		fetch_time = gf_net_get_utc();
	}


	if (!group->dash->ntp_forced && group->dash->estimate_utc_drift && !group->dash->utc_drift_estimate && group->dash->mpd_dnload && group->dash->dash_io->get_header_value) {
		const char *val = group->dash->dash_io->get_header_value(group->dash->dash_io, group->dash->mpd_dnload, "Server-UTC");
		if (val) {
			u64 utc;
			sscanf(val, LLU, &utc);
			group->dash->utc_drift_estimate = (s32) ((s64) fetch_time - (s64) utc);
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Estimated UTC diff between client and server %d ms (UTC fetch "LLU" - server UTC "LLU" - MPD AST "LLU" - MPD PublishTime "LLU"\n", group->dash->utc_drift_estimate, fetch_time, utc, group->dash->mpd->availabilityStartTime, group->dash->mpd->publishTime));
		} else {
			val = group->dash->dash_io->get_header_value(group->dash->dash_io, group->dash->mpd_dnload, "Date");
			if (val) {
				u64 utc = gf_net_parse_date(val);
				if (utc) {
					group->dash->utc_drift_estimate = (s32) ((s64) fetch_time - (s64) utc);
					GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Estimated UTC diff between client and server %d ms (UTC fetch "LLU" - server UTC "LLU" - MPD AST "LLU" - MPD PublishTime "LLU"\n", group->dash->utc_drift_estimate, fetch_time, utc, group->dash->mpd->availabilityStartTime, group->dash->mpd->publishTime));
				}
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
			t1 = gmtime(&gtime1);
			gtime2 = availabilityStartTime / 1000;
			t2 = gmtime(&gtime2);
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

	if (current_time < group->period->start) current_time = 0;
	else current_time -= group->period->start;

#if 0
	{
		s64 diff = (s64) current_time - (s64) (mpd->media_presentation_duration);
		if (ABS(diff)>10) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Broken UTC timing in client or server - got Media URL is not set in segment list\n"));

		}
		current_time = mpd->media_presentation_duration;
	}
#endif

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
		//commented for now, this increase the delay to the live ...
#if 0
		else if (group->dash->user_buffer_ms) {
			shift = MIN(group->dash->user_buffer_ms, mpd->time_shift_buffer_depth);

			if (current_time < shift) current_time = 0;
			else current_time -= shift;
		}
#endif
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

	if (timeline) {
		u64 start_segtime = 0;
		u64 segtime = 0;
		u64 current_time_rescale;
		u64 timeline_duration = 0;
		u32 count;
		u32 i, seg_idx = 0;

		current_time_rescale = current_time;
		current_time_rescale /= 1000;
		current_time_rescale *= timescale;

		count = gf_list_count(timeline->entries);
		for (i=0; i<count; i++) {
			GF_MPD_SegmentTimelineEntry *ent = gf_list_get(timeline->entries, i);

			if (!i && (current_time_rescale + ent->duration < ent->start_time)) {
				current_time_rescale = current_time_no_timeshift * timescale / 1000;
			}
			timeline_duration += (1+ent->repeat_count)*ent->duration;

			if (i+1 == count) timeline_duration -= ent->duration;
		}

		for (i=0; i<count; i++) {
			u32 repeat;
			GF_MPD_SegmentTimelineEntry *ent = gf_list_get(timeline->entries, i);
			if (!segtime) {
				start_segtime = segtime = ent->start_time;

				//if current time is before the start of the previous segement, consider our timing is broken
				if (current_time_rescale + ent->duration < segtime) {
					GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] current time "LLU" is before start time "LLU" of first segment in timeline (timescale %d) by %g sec - using first segment as starting point\n", current_time_rescale, segtime, timescale, (segtime-current_time_rescale)*1.0/timescale));
					group->download_segment_index = seg_idx;
					group->nb_segments_in_rep = count;
					group->start_playback_range = (segtime)*1.0/timescale;
					group->ast_at_init = availabilityStartTime - (u32) (ast_offset*1000);
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
					group->ast_at_init = availabilityStartTime - (u32) (ast_offset*1000);

					//to remove - this is a hack to speedup starting for some strange MPDs which announce the live point as the first segment but have already produced the complete timeline
					if (group->dash->utc_drift_estimate<0) {
						group->ast_at_init -= (timeline_duration - (segtime-start_segtime)) *1000/timescale;
					}
					return;
				}
				segtime += ent->duration;
				repeat--;
				seg_idx++;
			}
		}
		//check if we're ahead of time but "reasonnably" ahead (max 1 min) - otherwise consider the timing is broken
		if ((current_time_rescale >= segtime) && (current_time_rescale <= segtime + 60*timescale)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] current time "LLU" is greater than last SegmentTimeline end "LLU" - defaulting to last entry in SegmentTimeline\n", current_time_rescale, segtime));
			group->download_segment_index = seg_idx-1;
			group->nb_segments_in_rep = 10;
			group->start_playback_range = (current_time)/1000.0;
			group->ast_at_init = availabilityStartTime - (u32) (ast_offset*1000);
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

		//not time shifting, we are at the live edge, we must stick to start of segment otherwise we won't have enough data to play until next segment is ready

		if (!group->dash->initial_time_shift_value) {
			Double ms_in_seg;
			group->start_playback_range = shift * group->segment_duration;

			ms_in_seg = (Double) current_time/1000.0;
			ms_in_seg -= group->start_playback_range;

			//if low latency, try to adjust
			if (ast_offset) {
				Double ast_diff;
				if (ast_offset>group->segment_duration) ast_offset = group->segment_duration;
				ast_diff = group->segment_duration - ast_offset;

				//we assume that in low latency mode, chunks are made available every (group->segment_duration - ast_offset)
				//we need to seek such that the remaining time R satisfies now + R = NextSegAST
				//hence S(n) + ms_in_seg + R = S(n+1) + Aoffset
				//which gives us R = S(n+1) + Aoffset - S(n) - ms_in_seg = D + Aoffset - ms_in_seg
				//seek = D - R = D - (D + Aoffset - ms_in_seg) = ms_in_seg - Ao
				if (ms_in_seg > ast_diff) {
					group->start_playback_range += ms_in_seg - ast_diff;
				}
			}
		} else {
			group->start_playback_range = (Double) current_time / 1000.0;
		}

		if (!group->start_number_at_last_ast) {
			group->download_segment_index = shift;
			group->start_number_at_last_ast = start_number;

			group->ast_at_init = availabilityStartTime - (u32) (ast_offset*1000);
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] AST at init "LLD"\n", group->ast_at_init));

			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] At current time "LLD" ms: Initializing Timeline: startNumber=%d segmentNumber=%d segmentDuration=%f - %.03f seconds in segment\n", current_time, start_number, shift, group->segment_duration, group->start_playback_range ? group->start_playback_range - shift*group->segment_duration : 0));
		} else {
			group->download_segment_index += start_number;
			if (group->download_segment_index > group->start_number_at_last_ast) {
				group->download_segment_index -= group->start_number_at_last_ast;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] At current time %d ms: Updating Timeline: startNumber=%d segmentNumber=%d downloadSegmentIndex=%d segmentDuration=%g AST_diff=%d\n", current_time, start_number, shift, group->download_segment_index, group->segment_duration, ast_diff));
			} else {
				group->download_segment_index = shift;
				group->ast_at_init = availabilityStartTime -  (u32) (ast_offset*1000);
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





/*!
* Returns true if given mime type is a MPD file
* \param mime the mime-type to check
* \return true if mime-type if MPD-OK
*/
static Bool gf_dash_is_dash_mime(const char *mime) {
	u32 i;
	if (!mime)
		return GF_FALSE;
	for (i = 0 ; GF_DASH_MPD_MIME_TYPES[i] ; i++) {
		if ( !stricmp(mime, GF_DASH_MPD_MIME_TYPES[i]))
			return GF_TRUE;
	}
	return GF_FALSE;
}

/*!
* Returns true if mime type of a given URL is an M3U8 mime-type
* \param url The url to check
* \param mime The mime-type to check
* \return true if mime-type is OK for M3U8
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

static Bool gf_dash_is_smooth_mime(const char *url, const char * mime) 
{
	u32 i;
	if (!url || !mime)
		return GF_FALSE;
	if (strstr(url, ".mpd") || strstr(url, ".MPD"))
		return GF_FALSE;

	for (i = 0 ; GF_DASH_SMOOTH_MIME_TYPES[i] ; i++) {
		if ( !stricmp(mime, GF_DASH_SMOOTH_MIME_TYPES[i]))
			return GF_TRUE;
	}
	return GF_FALSE;
}


GF_EXPORT
GF_Err gf_dash_group_check_bandwidth(GF_DashClient *dash, u32 idx)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return GF_BAD_PARAM;

	if (dash->rate_adaptation_download_monitor)
		return dash->rate_adaptation_download_monitor(dash, group);

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

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Downloading %s starting at UTC "LLU" ms\n", url, gf_net_get_utc() ));

	if (group) {
		group_idx = gf_list_find(group->dash->groups, group);
	}

	if (! *sess) {
		*sess = dash_io->create(dash_io, persistent_mode ? 1 : 0, url, group_idx);
		if (!(*sess)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot try to download %s... OUT of memory ?\n", url));
			return GF_OUT_OF_MEM;
		}
	} else {
		had_sess = GF_TRUE;
		if (persistent_mode!=2) {
			e = dash_io->setup_from_url(dash_io, *sess, url, group_idx);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot resetup session for url %s: %s\n", url, gf_error_to_string(e) ));
				return e;
			}
		}
	}
	if (group) {
		group->is_downloading = GF_TRUE;
		group->download_start_time  = gf_sys_clock();
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
			if (group)
				group->is_downloading = GF_FALSE;

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
			/*we allow servers to give us broken mim types for the representation served ...*/
#if 0
			else if (mime && stricmp(group->service_mime, mime)) {
				GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);
				if (! gf_dash_get_mime_type(NULL, rep, group->adaptation_set) )
					rep->mime_type = gf_strdup(mime);
				rep->disabled = 1;
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Disabling representation since mime does not match: expected %s, but had %s for %s!\n", group->service_mime, mime, url));
				group->force_switch_bandwidth = 1;
				if (group->segment_download) dash_io->abort(dash_io, group->segment_download);
				group->is_downloading = 0;
				return GF_OK;
			}
#endif
		}

		/*file cannot be cached on disk !*/
		if (group) {
			if (dash_io->get_cache_name(dash_io, group->segment_download) == NULL) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Segment %s cannot be cached on disk, will use direct streaming\n", url));
				group->segment_must_be_streamed = GF_TRUE;
				if (group->segment_download) dash_io->abort(dash_io, group->segment_download);
				group->is_downloading = GF_TRUE;
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
	if (group && group->download_abort_type) {
		group->is_downloading = GF_FALSE;
		return GF_IP_CONNECTION_CLOSED;
	}
	switch (e) {
	case GF_IP_CONNECTION_FAILURE:
	case GF_IP_NETWORK_FAILURE:
		if (!dash->in_error || group) {
			dash_io->del(dash_io, *sess);
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] failed to download, retrying once with %s...\n", url));
			*sess = dash_io->create(dash_io, 0, url, group_idx);
			if (! (*sess)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot retry to download %s... OUT of memory ?\n", url));
				if (group)
					group->is_downloading = GF_FALSE;
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
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] FAILED to download %s = %s...\n", url, gf_error_to_string(e)));
		break;
	}
	if (group)
		group->is_downloading = GF_FALSE;

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
		if (!mediaDuration) {
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
				if (start_time == segment_start ) return idx;
				if (start_time > segment_start) {
					GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Warning: segment timeline entry start "LLU" greater than segment start "LLU", using current entry\n", start_time, segment_start));
					return idx;
				}
			} else {
				if (start_time*start_timescale == segment_start * timescale) return idx;
				if (start_time*start_timescale > segment_start * timescale) {
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
		if (start_time == segment_start ) return idx;
	} else {
		if (start_time*start_timescale == segment_start * timescale) return idx;
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
		u32 prev_idx = group->download_segment_index;
		group->nb_segments_in_rep = nb_new_segs;
		group->download_segment_index = gf_dash_get_index_in_timeline(new_timeline, group->current_start_time, group->current_timescale, timescale ? timescale : group->current_timescale);

		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Updated SegmentTimeline: New segment number %d - old %d - start time "LLD"\n", group->download_segment_index , prev_idx, group->current_start_time));
	} else {
		u32 i;
		for (i=0; i<gf_list_count(dash->groups); i++) {
			GF_DASH_Group *a_group = gf_list_get(dash->groups, i);
			u32 prev_idx = a_group->download_segment_index;
			a_group->nb_segments_in_rep = nb_new_segs;
			a_group->download_segment_index = gf_dash_get_index_in_timeline(new_timeline, a_group->current_start_time, a_group->current_timescale, timescale ? timescale : a_group->current_timescale);

			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Updated SegmentTimeline: New segment number %d - old %d - start time "LLD"\n", a_group->download_segment_index , prev_idx, a_group->current_start_time));
		}
	}


#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_DASH, GF_LOG_INFO) ) {
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] New SegmentTimeline: \n"));
		for (idx=0; idx<gf_list_count(new_timeline->entries); idx++) {
			GF_MPD_SegmentTimelineEntry *ent = gf_list_get(new_timeline->entries, idx);
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("\tt="LLU" d=%d r=%d\n", ent->start_time, ent->duration, ent->repeat_count));
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
			if (!ent->start_time) ent->start_time = start_time;
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
			}
		}
		group->nb_segments_purged += nb_removed;
	}
	return nb_removed;
}

static GF_Err gf_dash_solve_representation_xlink(GF_DashClient *dash, GF_MPD_Representation *rep)
{
	u32 count, i;
	GF_Err e;
	Bool is_local=GF_FALSE;
	const char *local_url;
	char *url;
	GF_DOMParser *parser;

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


static GF_Err gf_dash_update_manifest(GF_DashClient *dash)
{
	GF_Err e;
	Bool force_timeline_setup = GF_FALSE;
	u32 group_idx, rep_idx, i, j;
	u64 fetch_time=0;
	GF_DOMParser *mpd_parser;
	u8 signature[GF_SHA1_DIGEST_SIZE];
	GF_MPD_Period *period, *new_period;
	const char *local_url;
	char mime[128];
	char * purl;
	Double timeline_start_time;
	GF_MPD *new_mpd=NULL;
	Bool fetch_only = GF_FALSE;

	if (!dash->mpd_dnload) {
		local_url = purl = NULL;
		if (!gf_list_count(dash->mpd->locations)) {
			FILE *t = gf_fopen(dash->base_url, "rt");
			if (t) {
				local_url = dash->base_url;
				gf_fclose(t);
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
			gf_delete_file(local_url);
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
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update playlist: download problem %s for MPD file\n", gf_error_to_string(e)));
				dash->in_error = GF_TRUE;
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
			e = gf_m3u8_to_mpd(local_url, purl, NULL, dash->reload_count, dash->mimeTypeForM3U8Segments, 0, M3U8_TO_MPD_USE_TEMPLATE, &dash->getter, new_mpd, GF_FALSE, dash->keep_files);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update playlist: error in MPD creation %s\n", gf_error_to_string(e)));
				gf_mpd_del(new_mpd);
				return GF_NON_COMPLIANT_BITSTREAM;
			}
		} else if (!gf_dash_is_dash_mime(mime)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] mime '%s' should be m3u8 or mpd\n", mime));
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
		if (!gf_dash_check_mpd_root_type(local_url)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update playlist: MPD file type is not correct %s\n", local_url));
			return GF_NON_COMPLIANT_BITSTREAM;
		}

		if (gf_sha1_file( local_url, signature)) {
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
		e = gf_mpd_init_from_dom(gf_xml_dom_get_root(mpd_parser), new_mpd, purl);
		gf_xml_dom_del(mpd_parser);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update playlist: error in MPD creation %s\n", gf_error_to_string(e)));
			gf_mpd_del(new_mpd);
			return GF_NON_COMPLIANT_BITSTREAM;
		}
	}

	assert(new_mpd);

	period = gf_list_get(dash->mpd->periods, dash->active_period_index);
	if (fetch_only  && !period) goto exit;

	new_period = NULL;
	for (i=0; i<gf_list_count(new_mpd->periods); i++) {
		new_period = gf_list_get(new_mpd->periods, i);
		if (new_period->start==period->start) break;
		new_period=NULL;
	}

	if (!new_period) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update playlist: missing period\n"));
		gf_mpd_del(new_mpd);
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	dash->active_period_index = gf_list_find(new_mpd->periods, new_period);

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

	for (group_idx=0; group_idx<gf_list_count(dash->groups); group_idx++) {
		Double seg_dur;
		Bool reset_segment_count;
		GF_MPD_AdaptationSet *set, *new_set;
		u32 rep_i;
		GF_DASH_Group *group = gf_list_get(dash->groups, group_idx);

		/*update info even if the group is not selected !*/
		if (group->selection==GF_DASH_GROUP_NOT_SELECTABLE)
			continue;

		set = group->adaptation_set;
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

		/*get all representations in both periods*/
		for (rep_idx = 0; rep_idx <gf_list_count(group->adaptation_set->representations); rep_idx++) {
			GF_List *segments, *new_segments;
			GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, rep_idx);
			GF_MPD_Representation *new_rep = gf_list_get(new_set->representations, rep_idx);

			if (rep->segment_base || group->adaptation_set->segment_base || period->segment_base) {
				if (!new_rep->segment_base && !new_set->segment_base && !new_period->segment_base) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update playlist: representation does not use segment base as previous version\n"));
					gf_mpd_del(new_mpd);
					return GF_NON_COMPLIANT_BITSTREAM;
				}
				/*what else should we check ??*/

				/*OK, this rep is fine*/
			}

			else if (rep->segment_template || group->adaptation_set->segment_template || period->segment_template) {
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
				/*we're using segment list*/
				assert(rep->segment_list || group->adaptation_set->segment_list || period->segment_list);

				//if we have a xlink_href in segment_list, solve it
				while (new_rep->segment_list->xlink_href && (group->active_rep_index==rep_idx)) {
					u32 retry=10;
					GF_Err e;
					Bool is_static = GF_FALSE;
					u64 dur = 0;

					if (new_rep->segment_list->consecutive_xlink_count) {
						GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Resolving a XLINK pointed from another XLINK (%d consecutive XLINK in segment list)\n", new_rep->segment_list->consecutive_xlink_count));
					}

					while (retry) {
						if (dash->is_m3u8) {
							e = gf_m3u8_solve_representation_xlink(new_rep, &group->dash->getter, &is_static, &dur);
						} else {
							e = gf_dash_solve_representation_xlink(group->dash, new_rep);
						}
						if (e==GF_OK) break;
						if (e==GF_NON_COMPLIANT_BITSTREAM) break;
						if (e==GF_OUT_OF_MEM) break;
						if (group->dash->dash_state != GF_DASH_STATE_RUNNING)
							break;

						gf_sleep(100);
						retry --;
					}

					if (e==GF_OK) {
						if (dash->is_m3u8 && is_static) {
							GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[m3u8] MPD type changed from dynamic to static\n"));
							group->dash->mpd->type = GF_MPD_TYPE_STATIC;
							group->dash->mpd->media_presentation_duration = dur;
							group->dash->mpd->minimum_update_period = 0;
							group->period->duration = dur;
						}
					}
				}


				if (!new_rep->segment_list && !new_set->segment_list && !new_period->segment_list) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update playlist: representation does not use segment list as previous version\n"));
					gf_mpd_del(new_mpd);
					return GF_NON_COMPLIANT_BITSTREAM;
				}
				/*get the segment list*/
				segments = new_segments = NULL;
				if (period->segment_list && period->segment_list->segment_URLs) segments = period->segment_list->segment_URLs;
				if (set->segment_list && set->segment_list->segment_URLs) segments = set->segment_list->segment_URLs;
				if (rep->segment_list && rep->segment_list->segment_URLs) segments = rep->segment_list->segment_URLs;

				if (new_period->segment_list && new_period->segment_list->segment_URLs) new_segments = new_period->segment_list->segment_URLs;
				if (new_set->segment_list && new_set->segment_list->segment_URLs) new_segments = new_set->segment_list->segment_URLs;
				if (new_rep->segment_list && new_rep->segment_list->segment_URLs) new_segments = new_rep->segment_list->segment_URLs;


				for (i=0; i<gf_list_count(new_segments); i++) {
					GF_MPD_SegmentURL *new_seg = gf_list_get(new_segments, i);
					Bool found = GF_FALSE;
					for (j=0; j<gf_list_count(segments); j++) {
						GF_MPD_SegmentURL *seg = gf_list_get(segments, j);
						if (seg->media && new_seg->media && !strcmp(seg->media, new_seg->media)) {
							found=1;
							break;
						}
						if (seg->media_range && new_seg->media_range && (seg->media_range->start_range==new_seg->media_range->start_range) && (seg->media_range->end_range==new_seg->media_range->end_range) ) {
							found=1;
							break;
						}
					}
					/*this is a new segment, merge it: we remove from new list and push to old one, before doing a final swap
					this ensures that indexing in the segment_list is still correct after merging*/
					if (!found) {
						gf_list_rem(new_segments, i);
						i--;
						gf_list_add(segments, new_seg);
						GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Representation #%d: Adding new segment %s\n", rep_idx+1, new_seg->media));
					}
				}

				/*what else should we check ?*/

				/*swap segment list content*/
				gf_list_swap(new_segments, segments);

				//HLS live: if a new time is set (active group only), we just switched betwwe qualities
				//locate the segment with the same start time in the manifest, and purge previous ones
				//it may happen that the manifest does still not contain the segment we are looking for, force an MPD update
				if (group->hls_next_start_time && (group->active_rep_index==rep_idx)) {
					u32 k;

					for (k=0; k<gf_list_count(new_segments); k++) {
						s64 diff;
						GF_MPD_SegmentURL *segu = (GF_MPD_SegmentURL *) gf_list_get(new_segments, k);
						diff = (s64) group->hls_next_start_time;
						diff -= (s64) segu->hls_utc_start_time;
						if (abs( (s32) diff)<200) {
							group->download_segment_index = k;
							group->hls_next_start_time=0;
							break;
						}
						//purge old segments
						if (segu->hls_utc_start_time < group->hls_next_start_time) {
							gf_mpd_segment_url_free(segu);
							gf_list_rem(new_segments, k);
							k--;
						}
						if (segu->hls_utc_start_time > group->hls_next_start_time) {
							group->download_segment_index = k;
							GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Waiting for HLS segment start "LLU" but found segment at "LLU" - missing segment ?\n", group->hls_next_start_time, segu->hls_utc_start_time));
							group->hls_next_start_time=0;
							break;
						}
					}
					//not yet available
					if (group->hls_next_start_time) {
						GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Cannot find segment for given HLS start time "LLU" - forcing manifest update\n", group->hls_next_start_time));
						dash->force_mpd_update=GF_TRUE;
						//force sleep of half sec to avoid updating manifest too often - this will need refinement for low latency !!
						gf_sleep(500);
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

			/*what else should we check ??*/


			/*switch all internal GPAC stuff*/
			memcpy(&new_rep->playback, &rep->playback, sizeof(GF_DASH_RepresentationPlayback));
			if (rep->playback.cached_init_segment_url) rep->playback.cached_init_segment_url = NULL;

			if (!new_rep->mime_type) {
				new_rep->mime_type = rep->mime_type;
				rep->mime_type = NULL;
			}
		}

		/*update segmentTimeline at AdaptationSet level before switching the set (old setup needed to compute current timing of each group) */
		e = gf_dash_merge_segment_timeline(group, NULL, set->segment_list, set->segment_template, new_set->segment_list, new_set->segment_template, timeline_start_time);
		if (e) {
			gf_mpd_del(new_mpd);
			return e;
		}

		/*update group/period to new period*/
		j = gf_list_find(group->period->adaptation_sets, group->adaptation_set);
		group->adaptation_set = gf_list_get(new_period->adaptation_sets, j);
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
			group->timeline_setup = 0;
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

	}

exit:
	/*swap representations - we don't need to update download_segment_index as it still points to the right entry in the merged list*/
	if (dash->mpd)
		gf_mpd_del(dash->mpd);
	dash->mpd = new_mpd;
	dash->last_update_time = gf_sys_clock();
	dash->mpd_fetch_time = fetch_time;
	return GF_OK;
}


static void gf_dash_set_group_representation(GF_DASH_Group *group, GF_MPD_Representation *rep)
{
#ifndef GPAC_DISABLE_LOG
	u32 width=0, height=0, samplerate=0;
	GF_MPD_Fractional *framerate=NULL;
#endif
	u32 k;
	s32 timeshift;
	GF_MPD_AdaptationSet *set;
	GF_MPD_Period *period;
	u32 nb_segs;
	u32 i = gf_list_find(group->adaptation_set->representations, rep);
	u32 prev_active_rep_index = group->active_rep_index;
	u32 nb_cached_seg_per_rep = group->max_cached_segments / gf_dash_group_count_rep_needed(group);
	assert((s32) i >= 0);

	/* in case of dependent representations: we set force_max_rep_index than active_rep_index*/
	if (group->base_rep_index_plus_one)
		group->force_max_rep_index = i;
	else
		group->active_rep_index = i;
	group->active_bitrate = rep->bandwidth;
	group->max_cached_segments = nb_cached_seg_per_rep * gf_dash_group_count_rep_needed(group);
	nb_segs = group->nb_segments_in_rep;

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
		u32 retry=10;
		GF_Err e=GF_OK;

		if (rep->segment_list->consecutive_xlink_count) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Resolving a XLINK pointed from another XLINK (%d consecutive XLINK in segment list)\n", rep->segment_list->consecutive_xlink_count));
		}

		while (retry) {
			if (group->dash->is_m3u8) {
				e = gf_m3u8_solve_representation_xlink(rep, &group->dash->getter, &is_static, &dur);
			} else {
				e = gf_dash_solve_representation_xlink(group->dash, rep);
			}
			if (e==GF_OK) break;
			if (e==GF_NON_COMPLIANT_BITSTREAM) break;
			if (group->dash->dash_state != GF_DASH_STATE_RUNNING)
				break;

			retry--;
			gf_sleep(100);
		}

		//after resolving xlink: if this represenstation is marked as disabled, we have nothing to do
		if (rep->playback.disabled)
			return;

		if (e) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Could not reslove XLINK %s in time - using old representation\n", rep->segment_list->xlink_href));
			group->active_rep_index = prev_active_rep_index;
			return;
		}

		//we only know this is a static or dynamic MPD after parsing the first subplaylist
		//if this is static, we need to update infos in mpd and period
		if (group->dash->is_m3u8 && is_static) {
			group->dash->mpd->type = GF_MPD_TYPE_STATIC;
			group->dash->mpd->media_presentation_duration = dur;
			group->dash->mpd->minimum_update_period = 0;
			group->period->duration = dur;
		}
	}

	if (group->dash->is_m3u8) {
		//here we change to another representation: we need to remove all URLs from segment list and adjust the download segment index for this group
		if (group->dash->dash_state == GF_DASH_STATE_RUNNING) {
			u32 next_media_seq = group->m3u8_start_media_seq + group->download_segment_index;
			GF_MPD_Representation *prev_active_rep = (GF_MPD_Representation *)gf_list_get(group->adaptation_set->representations, prev_active_rep_index);

			if (group->dash->mpd->type == GF_MPD_TYPE_DYNAMIC) {
				u64 current_start_time = 0;
				Bool next_found=GF_FALSE;

				//find the start time of the next segment on the old representation
				GF_MPD_SegmentURL *seg_url = gf_list_get(prev_active_rep->segment_list->segment_URLs, group->download_segment_index);
				if (!seg_url) {
					//end of segment list, assume next was last one plus duration
					seg_url = gf_list_last(prev_active_rep->segment_list->segment_URLs);
					if (seg_url) current_start_time = seg_url->hls_utc_start_time + (seg_url->duration ? seg_url->duration : prev_active_rep->segment_list->duration);
				} else {
					current_start_time = seg_url->hls_utc_start_time;
				}
				group->hls_next_start_time = 0;

				//check in new list where the start is
				for (k=0; k<gf_list_count(rep->segment_list->segment_URLs); k++) {
					s64 start_diff;
					seg_url = (GF_MPD_SegmentURL *) gf_list_get(rep->segment_list->segment_URLs, k);
					assert(seg_url->hls_utc_start_time);

					start_diff = (s64) current_start_time;
					start_diff -= (s64) seg_url->hls_utc_start_time;
					//Warning, we may have precision issues in start times, add 200 ms for safety
					if (ABS(start_diff) <= 200) {
						group->download_segment_index = k;
						next_media_seq = rep->m3u8_media_seq_min + group->download_segment_index;
						next_found = GF_TRUE;
						break;
					}
					if (current_start_time < seg_url->hls_utc_start_time) {
						GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Switching to HLS start time "LLU" but found earlier segment with start time "LLU" - probabluy lost one segment\n", current_start_time, seg_url->hls_utc_start_time));
						group->download_segment_index = k;
						next_media_seq = rep->m3u8_media_seq_min + group->download_segment_index;
						next_found = GF_TRUE;
						break;
					}
				}
				//no segment in the new playlist is found for this start time, force an MPD update
				if (!next_found) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] No segment in new rep for current HLS time "LLU", updating manifest\n", current_start_time));
					group->hls_next_start_time = current_start_time;
					//this will force the MPD update below
					next_media_seq = 1+rep->m3u8_media_seq_max;
				}
			}

			if (rep->m3u8_media_seq_min > next_media_seq) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Something wrong here: next media segment %d vs min media segment in segment list %d - some segments missing\n", next_media_seq, rep->m3u8_media_seq_min));
				group->download_segment_index = rep->m3u8_media_seq_min;
			} else if (rep->m3u8_media_seq_max < next_media_seq) {
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Too late: next media segment %d vs max media segment in segment list %d - force updating mpd\n", next_media_seq, rep->m3u8_media_seq_max));
				group->dash->force_mpd_update = GF_TRUE;
			} else {
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] next  media segment %d found in  segment list (min %d - max %d) - adjusting download segment index\n", next_media_seq, rep->m3u8_media_seq_min, rep->m3u8_media_seq_max));
				group->download_segment_index =  next_media_seq - rep->m3u8_media_seq_min;
			}
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] after switching download segment index should be %d\n", group->download_segment_index));
		}
		group->m3u8_start_media_seq = rep->m3u8_media_seq_min;
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

		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] AS#%d changed quality to bitrate %d kbps - Width %d Height %d FPS %d/%d (playback speed %g)\n", 1+gf_list_find(group->period->adaptation_sets, group->adaptation_set), rep->bandwidth/1024, width, height, num, den, group->dash->speed));
	}
	else if (samplerate) {
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] AS#%d changed quality to bitrate %d kbps - sample rate %u (playback speed %g)\n", 1+gf_list_find(group->period->adaptation_sets, group->adaptation_set), rep->bandwidth/1024, samplerate, group->dash->speed));
	}
	else {
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] AS#%d changed quality to bitrate %d kbps (playback speed %g)\n", 1+gf_list_find(group->period->adaptation_sets, group->adaptation_set), rep->bandwidth/1024, group->dash->speed));
	}
#endif

	gf_dash_get_segment_duration(rep, set, period, group->dash->mpd, &group->nb_segments_in_rep, &group->segment_duration);

	/*if broken indication in duration restore previous seg count*/
	if (group->dash->ignore_mpd_duration)
		group->nb_segments_in_rep = nb_segs;

	timeshift = (s32) (rep->segment_base ? rep->segment_base->time_shift_buffer_depth : (rep->segment_list ? rep->segment_list->time_shift_buffer_depth : (rep->segment_template ? rep->segment_template->time_shift_buffer_depth : -1) ) );
	if (timeshift == -1) timeshift = (s32) (set->segment_base ? set->segment_base->time_shift_buffer_depth : (set->segment_list ? set->segment_list->time_shift_buffer_depth : (set->segment_template ? set->segment_template->time_shift_buffer_depth : -1) ) );
	if (timeshift == -1) timeshift = (s32) (period->segment_base ? period->segment_base->time_shift_buffer_depth : (period->segment_list ? period->segment_list->time_shift_buffer_depth : (period->segment_template ? period->segment_template->time_shift_buffer_depth : -1) ) );

	if (timeshift == -1) timeshift = (s32) group->dash->mpd->time_shift_buffer_depth;
	group->time_shift_buffer_depth = (u32) timeshift;

	group->dash->dash_io->on_dash_event(group->dash->dash_io, GF_DASH_EVENT_QUALITY_SWITCH, gf_list_find(group->dash->groups, group), GF_OK);
}

static void gf_dash_switch_group_representation(GF_DashClient *mpd, GF_DASH_Group *group)
{
	u32 i, bandwidth, min_bandwidth;
	GF_MPD_Representation *rep_sel = NULL;
	GF_MPD_Representation *min_rep_sel = NULL;
	Bool min_bandwidth_selected = GF_FALSE;
	bandwidth = 0;
	min_bandwidth = (u32) -1;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Checking representations between %d and %d kbps\n", group->min_bitrate/1024, group->max_bitrate/1024));

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
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] No representation found with bandwidth below %d kbps - using representation @ %d kbps\n", group->max_bitrate/1024, rep_sel->bandwidth/1024));
		}
		gf_dash_set_group_representation(group, rep_sel);
	}
}


static GF_Err gf_dash_resolve_url(GF_MPD *mpd, GF_MPD_Representation *rep, GF_DASH_Group *group, const char *mpd_url, GF_MPD_URLResolveType resolve_type, u32 item_index, char **out_url, u64 *out_range_start, u64 *out_range_end, u64 *segment_duration, Bool *is_in_base_url, char **out_key_url, bin128 *out_key_iv, Bool *data_url_process)
{
	GF_Err e;
	GF_MPD_AdaptationSet *set = group->adaptation_set;
	GF_MPD_Period *period = group->period;
	u32 timescale;

	if (!group->timeline_setup) {
		gf_dash_group_timeline_setup(mpd, group, 0);
		group->timeline_setup = 1;
		item_index = group->download_segment_index;
	}

	gf_mpd_resolve_segment_duration(rep, set, period, segment_duration, &timescale, NULL, NULL);
	*segment_duration = (resolve_type==GF_MPD_RESOLVE_URL_MEDIA) ? (u32) ((Double) ((*segment_duration) * 1000.0) / timescale) : 0;
	e = gf_mpd_resolve_url(mpd, rep, set, period, mpd_url, group->current_base_url_idx, resolve_type, item_index, group->nb_segments_purged, out_url, out_range_start, out_range_end, segment_duration, is_in_base_url, out_key_url, out_key_iv);


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
			char *decoded_base64_data;
			u32 len;
			sep+=8;
			len = (u32)strlen(sep) + 1;
			decoded_base64_data = (char *)gf_malloc(len);
			len = gf_base64_decode(sep, len, decoded_base64_data, len);
			sprintf(*out_url, "gmem://%d@%p", len, decoded_base64_data);
			*data_url_process = GF_TRUE;
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("data scheme with encoding different from base64 not supported\n"));
		}
	}

	return e;
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
	bytes_per_sec = group->bytes_per_sec;
	max_dl_speed = 8.0*bytes_per_sec / rep->bandwidth;

	//if framerate is not in MPD, suppose that it is 25 fps
	framerate = rep->framerate ? rep->framerate->num : 25;
	if (group->decode_only_rap)
		max_decoding_speed = group->irap_max_dec_time ? 1000000.0 / group->irap_max_dec_time : 0;
	else
		max_decoding_speed = group->avg_dec_time ? 1000000.0 / (group->max_dec_time + group->avg_dec_time*(framerate - 1)) : 0;
	max_available_speed = max_decoding_speed > max_dl_speed ? max_dl_speed : max_decoding_speed;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Representation %s max playout rate: in MPD %f - calculated by stat: %f\n", rep->id, rep->max_playout_rate, max_available_speed));

	return max_available_speed/2; // for testing and debug
}

static void dash_store_stats(GF_DashClient *dash, GF_DASH_Group *group, u32 bytes_per_sec, u32 file_size)
{
	const char *url;
	u32 buffer_ms = 0;
	Double bitrate, time;
	GF_MPD_Representation *rep;

	if (!group->nb_cached_segments)
		return;
	url = strrchr( group->cached[group->nb_cached_segments-1].url, '/');
	if (!url) url = strrchr( group->cached[group->nb_cached_segments-1].url, '\\');
	if (url) url+=1;
	else url = group->cached[group->nb_cached_segments-1].url;

	group->total_size = file_size;
	group->bytes_per_sec = bytes_per_sec;
	group->last_segment_time = gf_sys_clock();
	group->nb_segments_since_switch ++;

#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_DASH, GF_LOG_INFO)) {
		u32 i;
		//force a call go query buffer
		dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_CODEC_STAT_QUERY, gf_list_find(dash->groups, group), GF_OK);
		buffer_ms = group->buffer_occupancy_ms;
		for (i=0; i < group->nb_cached_segments; i++) {
			buffer_ms += group->cached[i].duration;
		}
	}

	bitrate=0;
	time=0;
	rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);
	if (group->current_downloaded_segment_duration) {
		bitrate = 8*group->total_size;
		bitrate /= group->current_downloaded_segment_duration;
	}
	if (group->bytes_per_sec) {
		time = group->total_size;
		time /= group->bytes_per_sec;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] AS#%d got %s stats: %d bytes in %g sec (%d kbps) - duration %g sec - Media Rate: indicated %d - computed %d kbps - buffer %d ms\n", 1+gf_list_find(group->period->adaptation_sets, group->adaptation_set), url, group->total_size, time, 8*group->bytes_per_sec/1000, group->current_downloaded_segment_duration/1000.0, rep->bandwidth/1000, (u32) bitrate, buffer_ms));

#endif
}

static GF_Err dash_do_rate_monitor_default(GF_DashClient *dash, GF_DASH_Group *group)
{
	Bool default_switch_mode = GF_FALSE;
	u32 download_rate, set_idx, time_since_start, done, tot_size, time_until_end;
	if (group->depend_on_group) return GF_BAD_PARAM;
	if (group->dash->disable_switching) return GF_OK;

	if (group->buffering)
		return GF_OK;

	download_rate = group->dash->dash_io->get_bytes_per_sec(group->dash->dash_io, group->segment_download);
	if (!download_rate) return GF_OK;


	done = group->dash->dash_io->get_bytes_done(group->dash->dash_io, group->segment_download);
	tot_size = group->dash->dash_io->get_total_size(group->dash->dash_io, group->segment_download);
	time_until_end = 0;
	if (tot_size) {
		time_until_end = 1000*(tot_size-done) / download_rate;
	}

	download_rate *= 8;
	if (download_rate<group->min_bitrate) group->min_bitrate = download_rate;
	if (download_rate>group->max_bitrate) group->max_bitrate = download_rate;

	if (!download_rate || (download_rate > group->active_bitrate)) {
		return GF_OK;
	}

	set_idx = gf_list_find(group->period->adaptation_sets, group->adaptation_set)+1;
	time_since_start = gf_sys_clock() - group->download_start_time;

	if (group->min_bandwidth_selected) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Downloading from set #%d at rate %d kbps but media bitrate is"
				" %d kbps - no lower bitrate available ...\n", set_idx, download_rate/1024, group->active_bitrate/1024 ));
		return GF_OK;
	}


	//TODO - when do we start checking ?
	if (time_since_start < 200) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Downloading from set #%ds at rate %d kbps but "
				"media bitrate is %d kbps\n", set_idx, download_rate/1024, group->active_bitrate/1024 ));
		return GF_OK;
	}

	if (time_until_end) {
		u32 i, cache_dur=0;
		for (i=1; i<group->nb_cached_segments; i++) {
			cache_dur += group->cached[i].duration;
		}
		//we have enough cache data to go until end of this download, perform rate switching at next segment
		if (time_until_end<cache_dur) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Downloading from set #%ds at rate %d kbps but "
					"media bitrate is %d kbps - %d till end of download and %d in cache - "
					"going on with download\n", set_idx, download_rate/1024, group->active_bitrate/1024,time_until_end, cache_dur ));
			return GF_OK;
		}
	}

	GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Downloading from set #%d at rate %d kbps but "
			"media bitrate is %d kbps - %d/%d in cache - killing connection and "
			"switching\n", set_idx, download_rate/1024, group->active_bitrate/1024, group->nb_cached_segments, group->max_cached_segments ));

	group->download_abort_type = 2;
	group->dash->dash_io->abort(group->dash->dash_io, group->segment_download);

	//in live we just abort current download and go to next. In onDemand, we may want to rebuffer
	default_switch_mode = (group->dash->mpd->type==GF_MPD_TYPE_DYNAMIC) ? GF_FALSE : GF_TRUE;

	//if we have time to download from another rep ?
	if (group->current_downloaded_segment_duration <= time_since_start) {
		//don't force bandwidth switch (it's too late anyway, consider we lost the segment), let the rate adaptation decide
		group->force_switch_bandwidth = default_switch_mode;

		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Download time longer than segment duration - trying to resync on next "
				"segment\n"));
	} else {
		u32 target_rate;
		//compute min bitrate needed to fetch the segement in another rep, with the time remaining
		Double ratio = ((u32)group->current_downloaded_segment_duration - time_since_start);
		ratio /= (u32)group->current_downloaded_segment_duration;

		target_rate = (u32) (download_rate * ratio);

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
	return GF_OK;
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
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] AS#%d no better match for requested bandwidth %d - not switching (AS bitrate %d)!\n", 1 + gf_list_find(group->period->adaptation_sets, group->adaptation_set), dl_rate, rep->bandwidth));
		do_switch = GF_FALSE;
	}

	if (do_switch) {
		//if we're switching to the next upper bitrate (no intermediate bitrates), do not immediately switch
		//but for a given number of segments - this avoids fluctuation in the quality
		if (go_up_bitrate && !nb_inter_rep) {
			new_rep->playback.probe_switch_count++;
			if (new_rep->playback.probe_switch_count > dash->probe_times_before_switch) {
				new_rep->playback.probe_switch_count = 0;
			}
			else {
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

	/* buffer_max_ms is non-null when the adaptation algorithm requires buffer information (e.g. GF_DASH_ALGO_GPAC_LEGACY_BUFFER ) */
	/* if the cache is full (i.e. player did not fetch downloaded data yet)
	   if we are below half of the buffer don't try to go up and limit rate to less than our current rep bandwidth*/
	if (group->buffer_max_ms && (group->nb_cached_segments<group->max_cached_segments)) {
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
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] AS#%d bitrate %d bps buffer max %d current %d refill since last %d - running low, switching down, target rate %d\n", 1 + gf_list_find(group->period->adaptation_sets, group->adaptation_set), rep->bandwidth, group->buffer_max_ms, group->buffer_occupancy_ms, occ, dl_rate));
		}
		//switch up if above max threshold and buffer refill is fast enough
		else if ((occ>0) && (group->buffer_occupancy_ms > buf_high_threshold)) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] AS#%d bitrate %d bps buffer max %d current %d refill since last %d - running high, will try to switch up, target rate %d\n", 1 + gf_list_find(group->period->adaptation_sets, group->adaptation_set), rep->bandwidth, group->buffer_max_ms, group->buffer_occupancy_ms, occ, dl_rate));
			go_up_bitrate = GF_TRUE;
		}
		//don't do anything in the middle range of the buffer or if refill not fast enough
		else {
			do_switch = GF_FALSE;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] AS#%d bitrate %d bps buffer max %d current %d refill since last %d - steady\n", 1 + gf_list_find(group->period->adaptation_sets, group->adaptation_set), rep->bandwidth, group->buffer_max_ms, group->buffer_occupancy_ms, occ));
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
	u32 k;
	u32 max_rate = 0;
	GF_MPD_Representation *rep;

	u32 nb_reps = gf_list_count(representations);
	for (k = nb_reps-1; k >=0 ; k--) {
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
	double f_buf_now;
	
	/* We don't use the segment duration as advertised in the MPD because it may not be there due to segment timeline*/
	u32 segment_duration_ms = (u32)group->current_downloaded_segment_duration;

	rate_min = ((GF_MPD_Representation *)gf_list_get(group->adaptation_set->representations, 0))->bandwidth;
	rate_max = ((GF_MPD_Representation *)gf_list_get(group->adaptation_set->representations, gf_list_count(group->adaptation_set->representations) - 1))->bandwidth;
	/* if the current buffer cannot hold an entire new segment, we indicate that we don't want to download it
	   NOTE: This is not described in the paper
	*/
	if (group->buffer_occupancy_ms + segment_duration_ms > group->buffer_max_ms) {
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
	if (group->buffer_max_ms <= segment_duration_ms) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] BBA-0: cannot initialize BBA-0 given the buffer size (%d) and segment duration (%d)\n", group->buffer_max_ms, group->segment_duration*1000));
		return -1;
	}
	r = (u32)(37.5*group->buffer_max_ms / 100);
	if (r < segment_duration_ms) {
		r = segment_duration_ms;
	}
	cu = (u32)((90-37.5)*group->buffer_max_ms / 100);

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
		new_index = gf_list_count(group->adaptation_set->representations) - 1;;
		get_min_rate_above(group->adaptation_set->representations, f_buf_now, &new_index);
	}
	else {
		// no change
		new_index = group->active_rep_index;
	}

	if (new_index != -1) {
		GF_MPD_Representation *result = gf_list_get(group->adaptation_set->representations, (u32)new_index);
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
		GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, k);
		rep->playback.bola_v = log(((Double)rep->bandwidth) / min_rep->bandwidth);
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
						GF_MPD_Representation *rep_m_prime, *rep_m_prime_plus_one;
						Double Sm_prime, Sm_prime_plus_one, f_m_prime, f_m_prime_1, bola_o_pause;
						assert(m_prime >= 0 && m_prime < nb_reps - 2);
						rep_m_prime = (GF_MPD_Representation *)gf_list_get(group->adaptation_set->representations, m_prime);
						rep_m_prime_plus_one = (GF_MPD_Representation *)gf_list_get(group->adaptation_set->representations, m_prime+1);
						Sm_prime = rep_m_prime->bandwidth*p;
						Sm_prime_plus_one = rep_m_prime_plus_one->bandwidth*p;
						f_m_prime = V_D*(rep_m_prime->playback.bola_v + gamma*p) / Sm_prime;
						f_m_prime_1 = V_D*(rep_m_prime_plus_one->playback.bola_v + gamma*p) / Sm_prime_plus_one;
						bola_o_pause = Q - (f_m_prime - f_m_prime_1) / (1 / Sm_prime - 1 / Sm_prime_plus_one);
						// TODO wait for bola_o_pause before making the download 
					}
				}
				new_index = m_prime;
			}
		}
		// TODO trigger pause for max(p*(Q-Q_Dmax+1), 0)
	}

	if (new_index != -1) {
		GF_MPD_Representation *result = gf_list_get(group->adaptation_set->representations, (u32)new_index);
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
	s32 new_index;
	GF_DASH_Group *base_group;
	GF_MPD_Representation *rep;
	GF_MPD_Representation *new_rep;
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
	dl_rate = (u32)  (8*group->bytes_per_sec / speed);

	/* Get the active representation in the AdaptationSet */
	rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);

	/*check whether we can play with this speed (i.e. achieve target frame rate);
	if not force, let algorithm know that they should switch to a lower resolution*/
	max_available_speed = gf_dash_get_max_available_speed(dash, base_group, rep);
	if (!dash->disable_speed_adaptation && !rep->playback.waiting_codec_reset) {
		if (max_available_speed && (speed > max_available_speed)) {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Forcing a lower complexity to achieve desired playback speed"));
			force_lower_complexity = GF_TRUE;
		}
		else {
			force_lower_complexity = GF_FALSE;
		}
	}
	else {
		force_lower_complexity = GF_FALSE;
	}

	/*query codec and buffer statistics for buffer-based algorithms */
	group->buffer_max_ms = 0;
	group->buffer_occupancy_ms = 0;
	group->codec_reset = 0;
	/* the DASH Client asks the player for its buffer level
	  (uses a function pointer to avoid depenencies on the player code, to reuse the DASH client in different situations)*/
	dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_CODEC_STAT_QUERY, gf_list_find(group->dash->groups, group), GF_OK);

	//adjust buffer with current segments not yet consumed by player
	for (k=0; k<group->nb_cached_segments; k++) {
		group->buffer_occupancy_ms += group->cached[k].duration;
	}


	/* If the playback for the current representation was waiting for a codec reset and it happened,
	   indicate that this representation does not need a reset anymore */
	if (rep->playback.waiting_codec_reset && group->codec_reset) {
		rep->playback.waiting_codec_reset = GF_FALSE;
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
		group->rate_adaptation_postponed = GF_TRUE;
		return;
	}
	group->rate_adaptation_postponed = GF_FALSE;

	if (new_index != group->active_rep_index) {
		new_rep = gf_list_get(group->adaptation_set->representations, (u32)new_index);
		if (!new_rep) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error: Cannot find new representation index: %d\n", new_index));
			return;
		}

		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] AS#%d switching after playing %d segments, from rep %d to "
				"rep %d\n", 1 + gf_list_find(group->period->adaptation_sets, group->adaptation_set),
				group->nb_segments_since_switch, group->active_rep_index, new_index));
		group->nb_segments_since_switch = 0;

		if (force_lower_complexity) {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Requesting codec reset"));
			new_rep->playback.waiting_codec_reset = GF_TRUE;
		}
		/* request downloads for the new representation */
		gf_dash_set_group_representation(group, new_rep);

		/* Reset smoothing of switches
		(note: should really apply only to algorithms using the switch_probe_count (smoothing the aggressiveness of the change)
		for now: only GF_DASH_ALGO_GPAC_LEGACY_RATE and  GF_DASH_ALGO_GPAC_LEGACY_BUFFER */
		for (k = 0; k < gf_list_count(group->adaptation_set->representations); k++) {
			GF_MPD_Representation *arep = gf_list_get(group->adaptation_set->representations, k);
			if (new_rep == arep) continue;
			arep->playback.probe_switch_count = 0;
		}

	} else if (force_lower_complexity) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Speed %f is too fast to play - speed down \n", dash->speed));
		/*FIXME: should do something here*/
	}

	/* Remembering the buffer level for the processing of the next segment */
	group->buffer_occupancy_at_last_seg = group->buffer_occupancy_ms;
}

static GF_Err gf_dash_download_init_segment(GF_DashClient *dash, GF_DASH_Group *group)
{
	GF_Err e;
	char *base_init_url;
	char *init_segment_local_url=NULL;
	GF_MPD_Representation *rep;
	u64 start_range, end_range;
	char mime[128];
	const char *mime_type;
	Bool data_url_processed = GF_FALSE;
	/* This variable is 0 if there is a initURL, the index of first segment downloaded otherwise */
	u32 nb_segment_read = 0;
	u32 file_size=0, Bps= 0;
	char *key_url=NULL;
	bin128 key_iv;

	if (!dash || !group)
		return GF_BAD_PARAM;

	assert(group->adaptation_set && group->adaptation_set->representations);
	rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);
	if (!rep) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Unable to find any representation, aborting.\n"));
		return GF_IO_ERR;
	}
	start_range = end_range = 0;

	e = gf_dash_resolve_url(dash->mpd, rep, group, dash->base_url, GF_MPD_RESOLVE_URL_INIT, 0, &base_init_url, &start_range, &end_range, &group->current_downloaded_segment_duration, NULL, &key_url, &key_iv, &data_url_processed);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Unable to resolve initialization URL: %s\n", gf_error_to_string(e) ));
		return e;
	}

	if (!base_init_url && rep->dependency_id) {
		return GF_OK;
	}

	/*no error and no init segment, go for media segment - this is needed for TS so that the set of media streams can be
	declared to the player */
	if (!base_init_url) {
		e = gf_dash_resolve_url(dash->mpd, rep, group, dash->base_url, GF_MPD_RESOLVE_URL_MEDIA, group->download_segment_index, &base_init_url, &start_range, &end_range, &group->current_downloaded_segment_duration, NULL, &key_url, &key_iv, NULL);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Unable to resolve media URL: %s\n", gf_error_to_string(e) ));
			return e;
		}
		nb_segment_read = 1;
	} else if (!group->bitstream_switching) {
		group->dont_delete_first_segment = 1;
	}

	if (!strstr(base_init_url, "://") || !strnicmp(base_init_url, "file://", 7) || !strnicmp(base_init_url, "gmem://", 7) || !strnicmp(base_init_url, "views://", 8) || !strnicmp(base_init_url, "isobmff://", 10)) {
		//if file-based, check if file exists, if not switch base URL
		if ( strnicmp(base_init_url, "gmem://", 7)) {
			FILE *ftest = gf_fopen(base_init_url, "rb");
			if (!ftest) {
				if (group->current_base_url_idx + 1 < gf_mpd_get_base_url_count(dash->mpd, group->period, group->adaptation_set, rep) ){
					group->current_base_url_idx++;
					gf_free(base_init_url);
					return gf_dash_download_init_segment(dash, group);
				}
			} else {
				gf_fseek(ftest, 0, SEEK_END);
				file_size = (u32) gf_ftell(ftest);
				gf_fclose(ftest);
			}
		}
		//we don't reset the baseURL index until we are done fetching all init segments

		assert(!group->nb_cached_segments);
		group->cached[0].cache = gf_strdup(base_init_url);
		group->cached[0].url = gf_strdup(base_init_url);
		group->cached[0].representation_index = group->active_rep_index;
		group->prev_active_rep_index = group->active_rep_index;
		if (key_url) {
			group->cached[0].key_url = key_url;
			memcpy(group->cached[0].key_IV, key_iv, sizeof(bin128));
		}

		group->nb_cached_segments = 1;
		/*do not erase local files*/
		group->local_files = group->was_segment_base ? 0 : 1;
		if (group->local_files) {
			gf_dash_buffer_off(group);
		}

		group->download_segment_index += nb_segment_read;
		init_segment_local_url = group->cached[0].cache;
		if (group->bitstream_switching) {
			group->bs_switching_init_segment_url = gf_strdup(init_segment_local_url);
			group->bs_switching_init_segment_url_start_range = start_range;
			group->bs_switching_init_segment_url_end_range = end_range;
			if (data_url_processed) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("URL with data scheme not handled for Bistream Switching Segments, probable memory leak"));
			}
		} else {
			rep->playback.cached_init_segment_url = gf_strdup(init_segment_local_url);
			rep->playback.owned_gmem = data_url_processed;
			rep->playback.init_start_range = start_range;
			rep->playback.init_end_range = end_range;
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

				e = gf_dash_resolve_url(dash->mpd, a_rep, group, dash->base_url, GF_MPD_RESOLVE_URL_INIT, 0, &a_base_init_url, &a_start, &a_end, &a_dur, NULL, &a_rep->playback.key_url, &a_rep->playback.key_IV, &a_rep->playback.owned_gmem);
				if (!e && a_base_init_url) {
					a_rep->playback.cached_init_segment_url = a_base_init_url;
					a_rep->playback.init_start_range = a_start;
					a_rep->playback.init_end_range =a_end ;
				} else if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot solve initialization segment for representation: %s - discarding representation\n", gf_error_to_string(e) ));
					a_rep->playback.disabled = 1;
				}
			}
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] First segment is %s \n", init_segment_local_url));
		gf_free(base_init_url);
		group->current_base_url_idx=0;
		dash_store_stats(dash, group, 0, file_size);
		return GF_OK;
	}

	group->max_bitrate = 0;
	group->min_bitrate = (u32)-1;
	/*use persistent connection for segment downloads*/
	e = gf_dash_download_resource(dash, &(group->segment_download), base_init_url, start_range, end_range, 1, group);

	if ((e==GF_OK) && group->force_switch_bandwidth && !dash->auto_switch_count) {
		gf_free(base_init_url);
		if (key_url) gf_free(key_url);
		gf_dash_switch_group_representation(dash, group);
		return gf_dash_download_init_segment(dash, group);
	}

	if ((e==GF_URL_ERROR) && base_init_url) {
		if (group->current_base_url_idx + 1 < gf_mpd_get_base_url_count(dash->mpd, group->period, group->adaptation_set, rep) ){
			group->current_base_url_idx++;
			gf_free(base_init_url);
			if (key_url) gf_free(key_url);
			return gf_dash_download_init_segment(dash, group);
		}
	}


	if ((e==GF_URL_ERROR) && base_init_url && !group->download_abort_type) { /* We have a 404 and started with segments */
		/* It is possible that the first segment has been deleted while we made the first request...
		 * so we try with the next segment on some M3U8 servers */
		gf_free(base_init_url);
		if (key_url) gf_free(key_url);
		e = gf_dash_resolve_url(dash->mpd, rep, group, dash->base_url, GF_MPD_RESOLVE_URL_MEDIA, group->download_segment_index + 1, &base_init_url, &start_range, &end_range, &group->current_downloaded_segment_duration, NULL, &key_url, &key_iv, NULL);
		if (e != GF_OK) {
			return e;
		}
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("Download of first segment failed... retrying with second one : %s\n", base_init_url));
		nb_segment_read = 2;
		/*use persistent connection for segment downloads*/
		e = gf_dash_download_resource(dash, &(group->segment_download), base_init_url, 0, 0, 1, group);
	} /* end of 404 */


	if ((e==GF_IP_CONNECTION_CLOSED) && group->download_abort_type) {
		group->download_abort_type = 0;
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Aborted while downloading init segment (seek ?)%s \n", base_init_url));
		gf_free(base_init_url);
		if (key_url) gf_free(key_url);
		return GF_OK;
	}

	if (e!= GF_OK && !group->segment_must_be_streamed) {
		dash->mpd_stop_request = 1;
		gf_free(base_init_url);
		if (key_url) gf_free(key_url);
		return e;
	}


	if (!group->nb_segments_in_rep) {
		if (dash->mpd->type==GF_MPD_TYPE_STATIC) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] 0 segments in static representation, aborting\n"));
			gf_free(base_init_url);
			if (key_url) gf_free(key_url);
			return GF_BAD_PARAM;
		}
	} else if (!group->groups_depending_on &&  (group->nb_segments_in_rep < group->max_cached_segments)) {
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Resizing to %u max_cached_segments elements instead of %u.\n", group->nb_segments_in_rep, group->max_cached_segments));
		/* OK, we have a problem, it may ends download */
		group->max_cached_segments = group->nb_segments_in_rep;
	}

	/* Mime-Type check */
	mime_type = dash->dash_io->get_mime(dash->dash_io, group->segment_download) ;
	strcpy(mime, mime_type ? mime_type : "");
	strlwr(mime);

	if (dash->mimeTypeForM3U8Segments)
		gf_free(dash->mimeTypeForM3U8Segments);
	dash->mimeTypeForM3U8Segments = gf_strdup( mime );
	mime_type = gf_dash_get_mime_type(NULL, rep, group->adaptation_set);
	if (!rep->mime_type) {
		rep->mime_type = gf_strdup( mime_type ? mime_type : mime );

	//disable mime type check
#if 0
		mime_type = gf_dash_get_mime_type(NULL, rep, group->adaptation_set);
	}
	if (stricmp(mime, mime_type)) {
		Bool valid = GF_FALSE;
		char *stype1, *stype2;
		stype1 = strchr(mime_type, '/');
		stype2 = strchr(mime, '/');
		if (stype1 && stype2 && !strcmp(stype1, stype2)) valid = 1;

		if (!valid) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Mime '%s' is not correct for '%s', it should be '%s'\n", mime, base_init_url, mime_type));
			dash->mpd_stop_request = 0;
			gf_free(base_init_url);
			if (key_url) gf_free(key_url);
			return GF_BAD_PARAM;
		}
	}
	if (!rep->mime_type) {
		rep->mime_type = gf_strdup( mime_type ? mime_type : mime );
#endif
	}

	if (group->segment_must_be_streamed ) {
		init_segment_local_url = (char *) dash->dash_io->get_url(dash->dash_io, group->segment_download);
		e = GF_OK;
	} else {
		init_segment_local_url = (char *) dash->dash_io->get_cache_name(dash->dash_io, group->segment_download);
	}

	if ((e!=GF_OK) || !init_segment_local_url) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error with initialization segment: download result:%s, cache file: %s\n", gf_error_to_string(e), init_segment_local_url ? init_segment_local_url : "UNKNOWN"));
		dash->mpd_stop_request = 1;
		gf_free(base_init_url);
		if (key_url) gf_free(key_url);
		return GF_BAD_PARAM;
	}

	file_size = dash->dash_io->get_total_size(dash->dash_io, group->segment_download) ;
	Bps = dash->dash_io->get_bytes_per_sec(dash->dash_io, group->segment_download) ;

	assert(!group->nb_cached_segments);
	group->cached[0].cache = gf_strdup(init_segment_local_url);
	group->cached[0].url = gf_strdup( dash->dash_io->get_url(dash->dash_io, group->segment_download) );
	group->cached[0].representation_index = group->active_rep_index;
	group->cached[0].duration = (u32) group->current_downloaded_segment_duration;

	if (group->bitstream_switching) {
		group->bs_switching_init_segment_url = gf_strdup(init_segment_local_url);
		group->bs_switching_init_segment_url_start_range = 0;
		group->bs_switching_init_segment_url_end_range = 0;
		if (data_url_processed) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("URL with data scheme not handled for Bistream Switching Segments, probable memory leak"));
		}
	} else {
		rep->playback.cached_init_segment_url = gf_strdup(init_segment_local_url);
		rep->playback.init_start_range = 0;
		rep->playback.init_end_range = 0;
		rep->playback.owned_gmem = data_url_processed;
	}

	group->nb_cached_segments = 1;
	group->download_segment_index += nb_segment_read;
	gf_dash_update_buffering(group, dash);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Adding initialization segment %s to cache: %s\n", init_segment_local_url, group->cached[0].url ));

	gf_free(base_init_url);

	/*download all init segments if any*/
	if (!group->bitstream_switching) {
		u32 k;
		for (k=0; k<gf_list_count(group->adaptation_set->representations); k++) {
			char *a_base_init_url = NULL;
			u64 a_start, a_end, a_dur;
			GF_MPD_Representation *a_rep = gf_list_get(group->adaptation_set->representations, k);
			if (a_rep==rep) continue;
			if (a_rep->playback.disabled) continue;

			e = gf_dash_resolve_url(dash->mpd, a_rep, group, dash->base_url, GF_MPD_RESOLVE_URL_INIT, 0, &a_base_init_url, &a_start, &a_end, &a_dur, NULL, &a_rep->playback.key_url, &a_rep->playback.key_IV, &a_rep->playback.owned_gmem);
			if (!e && a_base_init_url) {
				e = gf_dash_download_resource(dash, &(group->segment_download), a_base_init_url, a_start, a_end, 1, group);

				if ((e==GF_IP_CONNECTION_CLOSED) && group->download_abort_type) {
					group->download_abort_type = 0;
					GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Aborted while downloading init segment (seek ?)%s \n", a_base_init_url));

					gf_free(a_base_init_url);
					return GF_OK;
				}

				if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot retrieve initialization segment %s for representation: %s - discarding representation\n", a_base_init_url, gf_error_to_string(e) ));
					a_rep->playback.disabled = 1;
				} else {
					a_rep->playback.cached_init_segment_url = gf_strdup( dash->dash_io->get_cache_name(dash->dash_io, group->segment_download) );
					a_rep->playback.init_start_range = 0;
					a_rep->playback.init_end_range = 0;
				}
				gf_free(a_base_init_url);
			} else if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot solve initialization segment for representation: %s - discarding representation\n", gf_error_to_string(e) ));
				a_rep->playback.disabled = 1;
			}

		}
	}
	//reset baseURL idx to use first base URL
	group->current_base_url_idx = 0;
	/*if this was not an init segment, perform rate adaptation*/
	if (nb_segment_read) {
		dash_store_stats(dash, group, Bps, file_size);
		dash_do_rate_adaptation(dash, group);
	}

	return GF_OK;
}

static void gf_dash_skip_disabled_representation(GF_DASH_Group *group, GF_MPD_Representation *rep, Bool for_autoswitch)
{
	s32 rep_idx, orig_idx;
	u32 bandwidth = 0xFFFFFFFF;
	if (for_autoswitch && group->segment_download) {
		bandwidth = 8*group->dash->dash_io->get_bytes_per_sec(group->dash->dash_io, group->segment_download);
	}

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
	gf_dash_set_group_representation(group, rep);
}


static void gf_dash_group_reset_cache_entry(segment_cache_entry *cached)
{
	gf_free(cached->cache);
	gf_free(cached->url);
	if (cached->key_url) gf_free(cached->key_url);
	memset(cached, 0, sizeof(segment_cache_entry));
}

static void gf_dash_group_reset(GF_DashClient *dash, GF_DASH_Group *group)
{
	if (group->buffering) {
		gf_dash_buffer_off(group);
	}
	if (group->urlToDeleteNext) {
		if (!dash->keep_files && !group->local_files)
			dash->dash_io->delete_cache_file(dash->dash_io, group->segment_download, group->urlToDeleteNext);

		gf_free(group->urlToDeleteNext);
		group->urlToDeleteNext = NULL;
	}
	if (group->segment_download) {
		dash->dash_io->del(dash->dash_io, group->segment_download);
		group->segment_download = NULL;
	}
	while (group->nb_cached_segments) {
		group->nb_cached_segments --;
		if (!dash->keep_files && !group->local_files)
			gf_delete_file(group->cached[group->nb_cached_segments].cache);

		gf_dash_group_reset_cache_entry(&group->cached[group->nb_cached_segments]);
	}

	group->timeline_setup = 0;
}

static void gf_dash_reset_groups(GF_DashClient *dash)
{
	/*send playback destroy event*/
	dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_DESTROY_PLAYBACK, -1, GF_OK);

	while (gf_list_count(dash->groups)) {
		GF_DASH_Group *group = gf_list_last(dash->groups);
		gf_list_rem_last(dash->groups);

		gf_dash_group_reset(dash, group);

		gf_list_del(group->groups_depending_on);
		gf_free(group->cached);
		if (group->service_mime)
			gf_free(group->service_mime);

		if (group->download_th)
			gf_th_del(group->download_th);

		if (group->cache_mutex)
			gf_mx_del(group->cache_mutex);
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
	return gf_list_find(dash->groups, group);
}

GF_EXPORT
s32 gf_dash_group_has_dependent_group(GF_DashClient *dash, u32 idx)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return GF_FALSE;
	return group->depend_on_group ? gf_list_find(dash->groups, group->depend_on_group) : -1;
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
	return gf_list_find(dash->groups, group_depending_on);
}

/* create groups (implementation of adaptations set) */
GF_Err gf_dash_setup_groups(GF_DashClient *dash)
{
	GF_Err e;
	u32 i, j, count, nb_dependant_rep;
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
			GF_DASH_Group *group = gf_list_get(dash->groups, j);
			if (group->adaptation_set==set) {
				found = 1;
				break;
			}
		}

		if (found) continue;

		if (! gf_list_count(set->representations)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Empty adaptation set found (ID %s) - ignoring\n", set->id));
			continue;
		}


		GF_SAFEALLOC(group, GF_DASH_Group);
		if (!group) return GF_OUT_OF_MEM;
		group->dash = dash;
		group->adaptation_set = set;
		group->period = period;
		if (dash->use_threaded_download)
			group->download_th = gf_th_new("DashGroupDownload");

		group->cache_mutex = gf_mx_new("DashGroupMutex");

		group->bitstream_switching = (set->bitstream_switching || period->bitstream_switching) ? GF_TRUE : GF_FALSE;

		seg_dur = 0;
		nb_dependant_rep = 0;
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
				if (1) {
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
			rep->enhancement_rep_index_plus_one = 0;
			for (k = 0; k < gf_list_count(set->representations); k++) {
				GF_MPD_Representation *a_rep = gf_list_get(set->representations, k);
				if (a_rep->dependency_id) {
					char * tmp = strrchr(a_rep->dependency_id, ' ');
					if (tmp)
						tmp = tmp + 1;
					else
						tmp = a_rep->dependency_id;
					if (!strcmp(tmp, rep->id))
						rep->enhancement_rep_index_plus_one = k + 1;
				}
			}
			if (!rep->enhancement_rep_index_plus_one)
				group->force_max_rep_index = j;
			if (!rep->playback.disabled && rep->dependency_id)
				nb_dependant_rep++;
		}

		if (!seg_dur && !dash->is_m3u8) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Cannot compute default segment duration\n"));
		}

		group->cache_duration = dash->max_cache_duration;
		if (group->cache_duration < dash->mpd->min_buffer_time)
			group->cache_duration = dash->mpd->min_buffer_time;

		//we want at least 2 segments available in the cache, in order to perform rate adaptation with one cache ahead
		group->max_cached_segments = 2;
		if (seg_dur) {
			while (group->max_cached_segments * seg_dur * 1000 < group->cache_duration)
				group->max_cached_segments ++;

			group->max_buffer_segments = group->max_cached_segments;

#if 0
			/*unless we are in low latency modes*/
			if (dash->max_cache_duration>1000) {
				/*we need one more entry in cache for segment being currently played*/
				if (group->max_cached_segments<3)
					group->max_cached_segments ++;
			}
#endif
			group->max_cached_segments *= (nb_dependant_rep+1);
			group->max_buffer_segments *= (nb_dependant_rep+1);
		}

		if (!has_dependent_representations)
			group->base_rep_index_plus_one = 0; // all representations in this group are independent

		group->cached = gf_malloc(sizeof(segment_cache_entry)*group->max_cached_segments);
		memset(group->cached, 0, sizeof(segment_cache_entry)*group->max_cached_segments);
		if (!group->cached) {
			gf_free(group);
			return GF_OUT_OF_MEM;
		}
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
		u32 j = gf_dash_group_get_dependency_group(dash, i);
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
			group->max_buffer_segments *= (1+nb_dep_groups);
			group->cached = gf_realloc(group->cached, sizeof(segment_cache_entry)*group->max_cached_segments);
			memset(group->cached, 0, sizeof(segment_cache_entry)*group->max_cached_segments);

			for (j=0; j<nb_dep_groups; j++) {
				GF_DASH_Group *dep_group = gf_list_get(group->groups_depending_on, j);
				gf_free(dep_group->cached);
				dep_group->cached = NULL;
				dep_group->max_cached_segments = 0;
			}
		}
	}

	return GF_OK;
}

static GF_Err gf_dash_load_sidx(GF_BitStream *bs, GF_MPD_Representation *rep, Bool seperate_index, u64 sidx_offset)
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
	if (type != GF_4CC('s','i','d','x')) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error parsing SIDX: type is %s (box start offset "LLD")\n", gf_4cc_to_str(type), gf_bs_get_position(bs)-8 ));
		return GF_ISOM_INVALID_FILE;
	}

	gf_bs_seek(bs, sidx_offset);

	anchor_position = sidx_offset + size;
	if (seperate_index)
		anchor_position = 0;

	e = gf_isom_box_parse((GF_Box **) &sidx, bs);
	if (e) return e;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Loading SIDX - %d entries - Earliest Presentation Time "LLD"\n", sidx->nb_refs, sidx->earliest_presentation_time));

	offset = sidx->first_offset + anchor_position;
	rep->segment_list->timescale = sidx->timescale;
	for (i=0; i<sidx->nb_refs; i++) {
		GF_MPD_SegmentURL *seg;
		if (sidx->refs[i].reference_type) {
			e = gf_dash_load_sidx(bs, rep, seperate_index, offset);
			if (e) {
				break;
			}
		} else {
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

static GF_Err gf_dash_load_representation_sidx(GF_DASH_Group *group, GF_MPD_Representation *rep, const char *cache_name, Bool seperate_index, Bool needs_mov_range)
{
	GF_Err e;
	GF_BitStream *bs;
	FILE *f=NULL;
	if (!strncmp(cache_name, "gmem://", 7)) {
		u32 size;
		char *mem_address;
		if (sscanf(cache_name, "gmem://%d@%p", &size, &mem_address) != 2) {
			return GF_IO_ERR;
		}
		bs = gf_bs_new(mem_address, size, GF_BITSTREAM_READ);
	} else {
		FILE *f = gf_fopen(cache_name, "rb");
		if (!f) return GF_IO_ERR;
		bs = gf_bs_from_file(f, GF_BITSTREAM_READ);
	}
	e = GF_OK;
	while (gf_bs_available(bs)) {
		u32 size = gf_bs_read_u32(bs);
		u32 type = gf_bs_read_u32(bs);
		if (type != GF_4CC('s','i','d','x')) {
			gf_bs_skip_bytes(bs, size-8);

			if (needs_mov_range && (type==GF_4CC('m','o','o','v') )) {
				GF_SAFEALLOC(rep->segment_list->initialization_segment->byte_range, GF_MPD_ByteRange);
				rep->segment_list->initialization_segment->byte_range->end_range = gf_bs_get_position(bs);
			}
			continue;
		}
		gf_bs_seek(bs, gf_bs_get_position(bs)-8);
		e = gf_dash_load_sidx(bs, rep, seperate_index, gf_bs_get_position(bs) );

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
		u32 size;
		u8 *mem_address;
		if (sscanf(cache_name, "gmem://%d@%p", &size, &mem_address) != 2) {
			return GF_IO_ERR;
		}
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
		if (fread(data, 1, 4, f) == 4) {
			*box_size = GF_4CC(data[0], data[1], data[2], data[3]);
			if (fread(data, 1, 4, f) == 4) {
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
	GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, 0);

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
		e = gf_dash_resolve_url(group->dash->mpd, rep, group, group->dash->base_url, GF_MPD_RESOLVE_URL_INIT, 0, &init_url, &init_start_range, &init_end_range, &duration, &init_in_base, NULL, NULL, NULL);
		if (e) goto exit;

		e = gf_dash_resolve_url(group->dash->mpd, rep, group, group->dash->base_url, GF_MPD_RESOLVE_URL_INDEX, 0, &index_url, &index_start_range, &index_end_range, &duration, &index_in_base, NULL, NULL, NULL);
		if (e) goto exit;


		if (is_isom && (init_in_base || index_in_base)) {
			if (!strstr(init_url, "://") || (!strnicmp(init_url, "file://", 7) || !strnicmp(init_url, "views://", 7)) ) {
				GF_SAFEALLOC(rep->segment_list, GF_MPD_SegmentList);
				if (!rep->segment_list) {
					e = GF_OUT_OF_MEM;
					goto exit;
				}
				rep->segment_list->segment_URLs = gf_list_new();

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
			/*we need to download the init segement, at least partially*/
			else {
				u32 offset = 0;
				u32 box_type = 0;
				u32 box_size = 0;
				const char *cache_name;

				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Downloading init segment and SIDX for representation %s\n", init_url));

				/*download first 8 bytes and check if we do have a box starting there*/
				e = gf_dash_download_resource(group->dash, &(group->segment_download), init_url, offset, 7, 1, group);
				if (e) goto exit;
				cache_name = group->dash->dash_io->get_cache_name(group->dash->dash_io, group->segment_download);
				e = dash_load_box_type(cache_name, offset, &box_type, &box_size);
				offset = 8;
				while (box_type) {
					/*we got the moov, stop here */
					if (!index_in_base && (box_type==GF_4CC('m','o','o','v'))) {
						e = gf_dash_download_resource(group->dash, &(group->segment_download), init_url, offset, offset+box_size-9, 2, group);
						break;
					} else {
						const u32 offset_ori = offset;
						e = gf_dash_download_resource(group->dash, &(group->segment_download), init_url, offset, offset+box_size-1, 2, group);
						if (e < 0) goto exit;
						offset += box_size;
						/*we need to refresh the cache name because of our memory astorage thing ...*/
						cache_name = group->dash->dash_io->get_cache_name(group->dash->dash_io, group->segment_download);
						e = dash_load_box_type(cache_name, offset-8, &box_type, &box_size);
						if (e == GF_IO_ERR) {
							/*if the socket was closed then gf_dash_download_resource() with gmem:// was reset - retry*/
							e = dash_load_box_type(cache_name, offset-offset_ori-8, &box_type, &box_size);
							if (box_type == GF_4CC('s','i','d','x')) {
								offset -= 8;
								/*FIXME sidx found, reload the full resource*/
								GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] have to re-downloading init and SIDX for rep %s\n", init_url));
								e = gf_dash_download_resource(group->dash, &(group->segment_download), init_url, 0, offset+box_size-1, 2, group);
								break;
							}
						}

						if (box_type == GF_4CC('s','i','d','x'))
							has_seen_sidx = 1;
						else if (has_seen_sidx)
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

				cache_name = group->dash->dash_io->get_cache_name(group->dash->dash_io, group->segment_download);
				if (init_in_base) {
					char szName[100];
					GF_SAFEALLOC(rep->segment_list->initialization_segment, GF_MPD_URL);
					if (!rep->segment_list->initialization_segment) {
						e = GF_OUT_OF_MEM;
						goto exit;
					}
					//we need to store the init segment since it has the same name as the rest of the segments and will be destroyed when cleaning up the cache ..
					if (!strnicmp(cache_name, "gmem://", 7)) {
						char *mem_address;
						if (sscanf(cache_name, "gmem://%d@%p", &rep->playback.init_segment_size, &mem_address) != 2) {
							e = GF_IO_ERR;
							goto exit;
						}
						rep->playback.init_segment_data = gf_malloc(sizeof(char) * rep->playback.init_segment_size);
						memcpy(rep->playback.init_segment_data, mem_address, sizeof(char) * rep->playback.init_segment_size);

						sprintf(szName, "gmem://%d@%p", rep->playback.init_segment_size, rep->playback.init_segment_data);
						rep->segment_list->initialization_segment->sourceURL = gf_strdup(szName);
						rep->segment_list->initialization_segment->is_resolved = GF_TRUE;
					} else {
						FILE *t = gf_fopen(cache_name, "rb");
						if (t) {
							s32 res;
							fseek(t, 0, SEEK_END);
							rep->playback.init_segment_size = (u32) gf_ftell(t);
							fseek(t, 0, SEEK_SET);

							rep->playback.init_segment_data = gf_malloc(sizeof(char) * rep->playback.init_segment_size);
							res = (s32) fread(rep->playback.init_segment_data, sizeof(char), rep->playback.init_segment_size, t);
							if (res != rep->playback.init_segment_size) {
								GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Failed to store init segment\n"));
							} else if (rep->segment_list && rep->segment_list->initialization_segment) {
								sprintf(szName, "gmem://%d@%p", rep->playback.init_segment_size, rep->playback.init_segment_data);
								rep->segment_list->initialization_segment->sourceURL = gf_strdup(szName);
								rep->segment_list->initialization_segment->is_resolved = GF_TRUE;
							}
						}
					}

					cache_name = rep->segment_list->initialization_segment->sourceURL;
					//cleanup cache right away
					group->dash->dash_io->delete_cache_file(group->dash->dash_io, group->segment_download, init_url);

				}
				if (index_in_base) {
					sidx_file = (char *)cache_name;
				}
			}
		}
		/*we have index url, download it*/
		if (! index_in_base) {
			e = gf_dash_download_resource(group->dash, &(group->segment_download), index_url, index_start_range, index_end_range, 1, group);
			if (e) goto exit;
			sidx_file = (char *)group->dash->dash_io->get_cache_name(group->dash->dash_io, group->segment_download);
		}

		/*load sidx*/
		e = gf_dash_load_representation_sidx(group, rep, sidx_file, !index_in_base, init_needs_byte_range);
		if (e) {
			rep->playback.disabled = 1;
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Failed to load segment index for this representation - disabling\n"));
		}

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

static void gf_dash_solve_period_xlink(GF_DashClient *dash, u32 period_idx)
{
	u32 count, i;
	GF_Err e;
	Bool is_local=GF_FALSE;
	const char *local_url;
	char *url;
	GF_DOMParser *parser;
	GF_MPD *new_mpd;
	GF_MPD_Period *period;

	gf_mx_p(dash->dash_mutex);

	period = gf_list_get(dash->mpd->periods, period_idx);
	if (!period->xlink_href) {
		gf_mx_v(dash->dash_mutex);
		return;
	}

	GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Resolving period XLINK %s\n", period->xlink_href));

	if (!strcmp(period->xlink_href, "urn:mpeg:dash:resolve-to-zero:2013")) {
		//spec is not very clear here, I suppose it means "remove the element"
		gf_list_rem(dash->mpd->periods, period_idx);
		gf_mpd_period_free(period);
		gf_mx_v(dash->dash_mutex);
		return;
	}

	//xlink relative to our MPD base URL
	url = gf_url_concatenate(dash->base_url, period->xlink_href);

	if (!strstr(url, "://") || !strnicmp(url, "file://", 7) ) {
		local_url = url;
		is_local=GF_TRUE;
		e = GF_OK;
	} else {
		/*use non-persistent connection for MPD*/
		e = gf_dash_download_resource(dash, &(dash->mpd_dnload), url ? url : period->xlink_href, 0, 0, 0, NULL);

		gf_free(url);
	}

	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot download xlink from periods %s: error %s\n", period->xlink_href, gf_error_to_string(e)));
		gf_free(period->xlink_href);
		period->xlink_href = NULL;
		gf_mx_v(dash->dash_mutex);
		return;
	}

	if (!is_local) {
		/*in case the session has been restarted, local_url may have been destroyed - get it back*/
		local_url = dash->dash_io->get_cache_name(dash->dash_io, dash->mpd_dnload);
	}

	/* parse the MPD */
	parser = gf_xml_dom_new();
	e = gf_xml_dom_parse(parser, local_url, NULL, NULL);
	if (is_local) gf_free(url);

	if (e != GF_OK) {
		gf_xml_dom_del(parser);
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot parse xlink periods: error in XML parsing %s\n", gf_error_to_string(e)));
		gf_free(period->xlink_href);
		period->xlink_href = NULL;
		gf_mx_v(dash->dash_mutex);
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
		gf_mx_v(dash->dash_mutex);
		return;
	}

	gf_list_rem(dash->mpd->periods, period_idx);
	//insert all periods
	while (gf_list_count(new_mpd->periods)) {
		GF_MPD_Period *inserted_period = gf_list_get(new_mpd->periods, 0);
		gf_list_rem(new_mpd->periods, 0);
		//forbiden
		if (inserted_period->xlink_href && inserted_period->xlink_actuate_on_load) {
			gf_mpd_period_free(period);
			continue;
		}
		gf_list_insert(dash->mpd->periods, inserted_period, period_idx);
		period_idx++;
	}
	//this will do the garbage collection
	gf_list_add(new_mpd->periods, period);

	gf_mpd_del(new_mpd);

	gf_mx_v(dash->dash_mutex);
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
		return v;
	}


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
static void gf_dash_set_tiles_quality(GF_DashClient *dash, struct _dash_srd_desc *srd)
{
	u32 i, count;
	Bool tiles_use_lowest = (dash->first_select_mode==GF_DASH_SELECT_BANDWIDTH_HIGHEST_TILES) ? GF_TRUE : GF_FALSE;

	count = gf_list_count(dash->groups);
	for (i=0; i<count; i++) {
		GF_DASH_Group *group = gf_list_get(dash->groups, i);
		u32 lower_quality;
		if (group->srd_desc != srd) continue;

		lower_quality = gf_dash_get_tiles_quality_rank(dash, group);
		if (!lower_quality) continue;

		if (tiles_use_lowest && (group->active_rep_index >= lower_quality)) {
			lower_quality = group->active_rep_index - lower_quality;
		} else {
			lower_quality = 0;
		}
		gf_dash_set_group_representation(group,
		                                 gf_list_get(group->adaptation_set->representations, lower_quality) );
	}
}

static struct _dash_srd_desc *gf_dash_get_srd_desc(GF_DashClient *dash, u32 srd_id, Bool do_create)
{
	u32 i, count;
	struct _dash_srd_desc *srd;
	count = dash->SRDs ? gf_list_count(dash->SRDs) : 0;
	for (i=0; i<count; i++) {
		struct _dash_srd_desc *srd = gf_list_get(dash->SRDs, i);
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
		if (!period->xlink_href) break;
		gf_dash_solve_period_xlink(dash, dash->active_period_index);
		retry --;
	}
	period = gf_list_get(dash->mpd->periods, dash->active_period_index);
	if (period->xlink_href) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Too many xlink indirections on the same period - not supported\n"));
		return GF_NOT_SUPPORTED;
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

	/*setup all groups*/
	gf_dash_setup_groups(dash);

	if (dash->debug_group_index>=0) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Debuging adaptation set #%d in period, ignoring other ones!\n\n", dash->debug_group_index + 1));
	}

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

		if ((dash->debug_group_index>=0) && (group_i != (u32) dash->debug_group_index)) {
			group->selection = GF_DASH_GROUP_NOT_SELECTABLE;
			continue;
		}

		nb_rep = gf_list_count(group->adaptation_set->representations);

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
					group->srd_desc->srd_fw = w;
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
					group->srd_desc->srd_fw = w;
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

		//sort by  bandwidth and quality
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
				char *sep;
				if (rep->codecs && rep_sel->codecs) {

					sep = strchr(rep_sel->codecs, '.');
					if (sep) sep[0] = 0;
					ok = !strnicmp(rep->codecs, rep_sel->codecs, strlen(rep_sel->codecs) );
					if (sep) sep[0] = '.';
					if (!ok) {
						GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Different codec types (%s vs %s) in same AdaptationSet\n", rep_sel->codecs, rep->codecs));
						//rep->playback.disabled = 1;
						//continue;
					}
				}
			}

			switch (dash->first_select_mode) {
			case GF_DASH_SELECT_QUALITY_LOWEST:
				if (rep->quality_ranking && (rep->quality_ranking < rep_sel->quality_ranking)) {
					active_rep = rep_i;
					break;
				}/*fallthrough if quality is not indicated*/
				break;
			case GF_DASH_SELECT_BANDWIDTH_LOWEST:
				if ((rep->width&&rep->height) || !group_has_video) {
					if (rep->bandwidth < rep_sel->bandwidth) {
						active_rep = rep_i;
					}
				}
				break;
			case GF_DASH_SELECT_QUALITY_HIGHEST:
				/*fallthrough if quality is not indicated*/
				if (rep->quality_ranking > rep_sel->quality_ranking) {
					active_rep = rep_i;
					break;
				}
				break;
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

		gf_dash_set_group_representation(group, rep_sel);
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
		gf_dash_set_tiles_quality(dash, srd);
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

static Bool gf_dash_is_seamless_period_switch(GF_DashClient *dash)
{
	return 0;
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
			group->download_segment_index ++;
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


/*TODO decide what is the best, fetch from another representation or ignore ...*/
static DownloadGroupStatus on_group_download_error(GF_DashClient *dash, GF_DASH_Group *group, GF_DASH_Group *base_group, GF_Err e, GF_MPD_Representation *rep, char *new_base_seg_url, char *key_url, Bool has_dep_following)
{
	u32 clock_time = gf_sys_clock();

	if (!dash->min_wait_ms_before_next_request || (dash->min_timeout_between_404 < dash->min_wait_ms_before_next_request))
		dash->min_wait_ms_before_next_request = dash->min_timeout_between_404;

	group->retry_after_utc = dash->min_timeout_between_404 + gf_net_get_utc();

	if (group->maybe_end_of_stream) {
		if (group->maybe_end_of_stream==2) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Couldn't get segment %s (error %s) and end of period was guessed during last update - stopping playback\n", new_base_seg_url, gf_error_to_string(e)));
			group->maybe_end_of_stream = 0;
			group->done = 1;
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
			group->done = 1;
		}
		group->segment_in_valid_range=0;
	} else if (group->prev_segment_ok && !group->time_at_first_failure) {
		if (!group->loop_detected) {
			group->time_at_first_failure = clock_time;
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Error in downloading new segment: %s %s - starting countdown for %d ms\n", new_base_seg_url, gf_error_to_string(e), group->current_downloaded_segment_duration));
		}
	}
	//if multiple baseURL, try switching the base
	else if ((e==GF_URL_ERROR) && (group->current_base_url_idx + 1 < gf_mpd_get_base_url_count(dash->mpd, group->period, group->adaptation_set, rep) )) {
		group->current_base_url_idx++;
		if (new_base_seg_url) gf_free(new_base_seg_url);
		if (key_url) gf_free(key_url);
		return dash_download_group_download(dash, group, base_group, has_dep_following);
	}
	//if previous segment download was OK, we are likely asking too early - retry for the complete duration in case one segment was lost - we add 100ms safety
	else if (group->prev_segment_ok && (clock_time - group->time_at_first_failure <= group->current_downloaded_segment_duration + dash->segment_lost_after_ms )) {
	} else {
		if (group->prev_segment_ok) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error in downloading new segment: %s %s - waited %d ms but segment still not available, checking next one ...\n", new_base_seg_url, gf_error_to_string(e), clock_time - group->time_at_first_failure));
			group->time_at_first_failure = 0;
			group->prev_segment_ok = GF_FALSE;
		}
		group->nb_consecutive_segments_lost ++;
		//we are lost ....
		if (group->nb_consecutive_segments_lost == 20) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Too many consecutive segments not found, sync or signal has been lost - entering end of stream detection mode\n"));
			if (dash->min_wait_ms_before_next_request || (dash->min_wait_ms_before_next_request > 1000))
				dash->min_wait_ms_before_next_request = 1000;
			group->maybe_end_of_stream = 1;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error in downloading new segment: %s %s\n", new_base_seg_url, gf_error_to_string(e)));
			if (dash->speed >= 0) {
				group->download_segment_index++;
			} else if (group->download_segment_index) {
				group->download_segment_index--;
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Playing in backward - start of playlist reached - assuming end of stream\n"));
				group->done = 1;
			}
		}
	}
	if (rep->dependency_id) {
		segment_cache_entry *cache_entry = &base_group->cached[base_group->nb_cached_segments];
		cache_entry->has_dep_following = 0;
	}

	if (group->base_rep_index_plus_one) {
		group->active_rep_index = group->base_rep_index_plus_one - 1;
		group->has_pending_enhancement = GF_FALSE;
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
	u32 representation_index;
	u32 clock_time, file_size=0, Bps=0;
	Bool empty_file = GF_FALSE;
	const char *local_file_name = NULL;
	const char *resource_name = NULL;

	if (group->done) return GF_DASH_DownloadSuccess;

	if (group->selection != GF_DASH_GROUP_SELECTED) return GF_DASH_DownloadSuccess;

	if (base_group->nb_cached_segments>=base_group->max_cached_segments) {
		return GF_DASH_DownloadCancel;
	}

	/*remember the active rep index, since group->active_rep_index may change because of bandwidth control algorithm*/
	representation_index = group->active_rep_index;
	rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);

	/* if the index of the segment to be downloaded is greater or equal to the last segment (as seen in the playlist),
	 we need to check if a new playlist is ready */
	if (group->nb_segments_in_rep && (group->download_segment_index >= (s32) group->nb_segments_in_rep)) {
		u32 timer = gf_sys_clock() - dash->last_update_time;
		Bool update_playlist = 0;

		/* this period is done*/
		if ((dash->mpd->type==GF_MPD_TYPE_DYNAMIC) && group->period->duration) {
		}
		/* update of the playlist, only if indicated */
		else if (dash->mpd->minimum_update_period && (timer > dash->mpd->minimum_update_period)) {
			update_playlist = 1;
		}
		/* if media_presentation_duration is 0 and we are in live, force a refresh (not in the spec but safety check*/
		else if ((dash->mpd->type==GF_MPD_TYPE_DYNAMIC) && !dash->mpd->media_presentation_duration) {
			if (group->segment_duration && (timer > group->segment_duration*1000))
				update_playlist = 1;
		}
		if (update_playlist) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Playlist should be updated, postponing group download until playlist is updated\n"));
			dash->force_mpd_update = 1;
			return GF_DASH_DownloadCancel;
		}
		/* Now that the playlist is up to date, we can check again */
		if (group->download_segment_index  >= (s32) group->nb_segments_in_rep) {
			/* if there is a specified update period, we redo the whole process */
			if (dash->mpd->minimum_update_period || dash->mpd->type==GF_MPD_TYPE_DYNAMIC) {

				if ((dash->mpd->type==GF_MPD_TYPE_DYNAMIC) && group->period->duration) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Last segment in period (dynamic mode) - group is done\n"));
					group->done = 1;
					return GF_DASH_DownloadCancel;
				}
				else if (! group->maybe_end_of_stream) {
					u32 now = gf_sys_clock();
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] End of segment list reached (%d segments but idx is %d), waiting for next MPD update\n", group->nb_segments_in_rep, group->download_segment_index));
					if (group->nb_cached_segments) {
						if (dash->is_m3u8 && (group->nb_cached_segments <= 1)) {
							GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[M3U8] There is only %d segment in cache, force MPD update\n", group->nb_cached_segments));
							dash->force_mpd_update = GF_TRUE;
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
					} else if (dash->mpd->type==GF_MPD_TYPE_DYNAMIC) {
						if (timer < group->nb_segments_in_rep * group->segment_duration * 1000)
							return GF_DASH_DownloadCancel;
					}

					GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Segment list has not been updated for more than %d ms - assuming end of period\n", now - group->time_at_first_reload_required));
					group->done = 1;
					return GF_DASH_DownloadCancel;
				}
			} else {
				/* if not, we are really at the end of the playlist, we can quit */
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] End of period reached for group\n"));
				group->done = 1;
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
	if (!group->broken_timing && (dash->mpd->type==GF_MPD_TYPE_DYNAMIC) && !dash->is_m3u8) {
		s32 to_wait = 0;
		u32 seg_dur_ms=0;
#ifndef GPAC_DISABLE_LOG
		u32 start_number = gf_dash_get_start_number(group, rep);
#endif
		s64 segment_ast = (s64) gf_dash_get_segment_availability_start_time(dash->mpd, group, group->download_segment_index, &seg_dur_ms);
		s64 now = (s64) gf_net_get_utc();


		if (group->retry_after_utc > (u64) now) {
			to_wait = (u32) (group->retry_after_utc - (u64) now);
			if (!dash->min_wait_ms_before_next_request || ((u32) to_wait < dash->min_wait_ms_before_next_request))
				dash->min_wait_ms_before_next_request = to_wait;

			return GF_DASH_DownloadCancel;
		}

		clock_time = gf_sys_clock();
		to_wait = (s32) (segment_ast - now);

		/*if segment AST is greater than now, it is not yet available - we would need an estimate on how long the request takes to be sent to the server in order to be more reactive ...*/
		if (to_wait > 1) {

			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Set #%d At %d Next segment %d (AST "LLD" - sec in period %g) is not yet available on server - requesting later in %d ms\n", 1+gf_list_find(dash->groups, group), gf_sys_clock(), group->download_segment_index + start_number, segment_ast, (segment_ast - group->period->start - group->ast_at_init)/1000.0, to_wait));
			if (group->last_segment_time) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] %d ms elapsed since previous segment download\n", clock_time - group->last_segment_time));
			}
			if (!dash->min_wait_ms_before_next_request || ((u32) to_wait < dash->min_wait_ms_before_next_request))
				dash->min_wait_ms_before_next_request = to_wait;

			return GF_DASH_DownloadCancel;
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Set #%d At %d Next segment %d (AST "LLD" - sec in period %g) should now be available on server since %d ms - requesting it\n", 1+gf_list_find(dash->groups, group), gf_sys_clock(), group->download_segment_index + start_number, segment_ast, (segment_ast - group->period->start - group->ast_at_init)/1000.0, -to_wait));

			if (group->last_segment_time) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] %d ms elapsed since previous segment download\n", clock_time - group->last_segment_time));
			}
#if 0
			/*check if we are in the segment availability end time*/
			if (now < segment_ast + seg_dur_ms + group->time_shift_buffer_depth )
				in_segment_avail_time = 1;
#endif
		}
	}

	/* At this stage, there are some segments left to be downloaded */
	e = gf_dash_resolve_url(dash->mpd, rep, group, dash->base_url, GF_MPD_RESOLVE_URL_MEDIA, group->download_segment_index, &new_base_seg_url, &start_range, &end_range, &group->current_downloaded_segment_duration, NULL, &key_url, &key_iv, NULL);

	if (e || !new_base_seg_url) {
		if (e==GF_EOS) {
			group->done = GF_TRUE;
		} else {
			/*do something!!*/
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error resolving URL of next segment: %s\n", gf_error_to_string(e) ));
		}
		return GF_DASH_DownloadCancel;
	}
	use_byterange = (start_range || end_range) ? 1 : 0;

	if (use_byterange) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Downloading new segment: %s (range: "LLD"-"LLD")\n", new_base_seg_url, start_range, end_range));
	}

	/*local file*/
	if (!strstr(new_base_seg_url, "://") || (!strnicmp(new_base_seg_url, "file://", 7) || !strnicmp(new_base_seg_url, "gmem://", 7)) ) {
		FILE *ftest;
		resource_name = local_file_name = (char *) new_base_seg_url;
		e = GF_OK;
		/*do not erase local files*/
		group->local_files = 1;
		gf_dash_buffer_off(group);
		if (group->force_switch_bandwidth && !dash->auto_switch_count) {
			gf_dash_switch_group_representation(dash, group);
			/*restart*/
			return GF_DASH_DownloadRestart;
		}
		ftest = gf_fopen(local_file_name, "rb");
		if (!ftest) {
			if (group->current_base_url_idx + 1 < gf_mpd_get_base_url_count(dash->mpd, group->period, group->adaptation_set, rep) ){
				group->current_base_url_idx++;
				if (new_base_seg_url) gf_free(new_base_seg_url);
				if (key_url) gf_free(key_url);
				return dash_download_group_download(dash, group, base_group, has_dep_following);
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] File %s not found on disk\n", local_file_name));
				group->current_base_url_idx = 0;
				return on_group_download_error(dash, group, base_group, GF_NOT_FOUND, rep, new_base_seg_url, key_url, has_dep_following);
			}
		} else {
			gf_fseek(ftest, 0, SEEK_END);
			file_size = (u32) gf_ftell(ftest);
			gf_fclose(ftest);
		}
		group->current_base_url_idx = 0;
	} else {
		base_group->max_bitrate = 0;
		base_group->min_bitrate = (u32)-1;

		/*use persistent connection for segment downloads*/
		if (use_byterange) {
			e = gf_dash_download_resource(dash, &(base_group->segment_download), new_base_seg_url, start_range, end_range, 1, base_group);
		} else {
			e = gf_dash_download_resource(dash, &(base_group->segment_download), new_base_seg_url, 0, 0, 1, base_group);
		}

		if ((e==GF_IP_CONNECTION_CLOSED) && group->download_abort_type) {
			base_group->download_abort_type = 0;
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Aborted while downloading segment (seek ?)%s \n", new_base_seg_url));
			if (new_base_seg_url) gf_free(new_base_seg_url);
			if (key_url) gf_free(key_url);
			return GF_DASH_DownloadSuccess;
		}

		if (e != GF_OK) {
			return on_group_download_error(dash, group, base_group, e, rep, new_base_seg_url, key_url, has_dep_following);
		}

		group->prev_segment_ok = GF_TRUE;
		if (group->time_at_first_failure) {
			if (group->current_base_url_idx) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Recovered segment %s after 404 by switching baseURL\n", new_base_seg_url));
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Recovered segment %s after 404 - was our download schedule too early ?\n", new_base_seg_url));
			}
			group->time_at_first_failure = 0;
		}
		group->nb_consecutive_segments_lost = 0;
		group->current_base_url_idx = 0;

		if ((e==GF_OK) && group->force_switch_bandwidth) {
			if (!dash->auto_switch_count) {
				gf_dash_switch_group_representation(dash, group);
				if (new_base_seg_url) gf_free(new_base_seg_url);
				if (key_url) gf_free(key_url);
				/*restart*/
				return GF_DASH_DownloadRestart;
			}
			if (rep->playback.disabled) {
				gf_dash_skip_disabled_representation(group, rep, GF_FALSE);
				if (new_base_seg_url) gf_free(new_base_seg_url);
				if (key_url) gf_free(key_url);
				/*restart*/
				return GF_DASH_DownloadRestart;
			}
		}
		group->segment_must_be_streamed = base_group->segment_must_be_streamed;

		if (group->segment_must_be_streamed)
			local_file_name = dash->dash_io->get_url(dash->dash_io, base_group->segment_download);
		else
			local_file_name = dash->dash_io->get_cache_name(dash->dash_io, base_group->segment_download);

		file_size = dash->dash_io->get_total_size(dash->dash_io, base_group->segment_download);
		if (file_size==0) {
			empty_file = GF_TRUE;
		}
		resource_name = dash->dash_io->get_url(dash->dash_io, base_group->segment_download);

		Bps = dash->dash_io->get_bytes_per_sec(dash->dash_io, base_group->segment_download);
	}

	if (local_file_name && (e == GF_OK || group->segment_must_be_streamed )) {
		gf_mx_p(group->cache_mutex);

		assert(base_group->nb_cached_segments<base_group->max_cached_segments);
		assert(local_file_name);

		if (!empty_file) {
			segment_cache_entry *cache_entry = &base_group->cached[base_group->nb_cached_segments];

			cache_entry->cache = gf_strdup(local_file_name);
			cache_entry->url = gf_strdup( resource_name );
			cache_entry->start_range = 0;
			cache_entry->end_range = 0;
			cache_entry->representation_index = representation_index;
			cache_entry->duration = (u32) group->current_downloaded_segment_duration;
			cache_entry->loop_detected = group->loop_detected;
			cache_entry->has_dep_following = has_dep_following;
			if (key_url) {
				cache_entry->key_url = key_url;
				memcpy(cache_entry->key_IV, key_iv, sizeof(bin128));
				key_url = NULL;
			}

			group->loop_detected = GF_FALSE;

			if (group->local_files && use_byterange) {
				cache_entry->start_range = start_range;
				cache_entry->end_range = end_range;
			}
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Added file to cache (%u/%u in cache): %s\n", base_group->nb_cached_segments+1, base_group->max_cached_segments, cache_entry->url));

			base_group->nb_cached_segments++;
			gf_dash_update_buffering(group, dash);
		}
		dash_store_stats(dash, group, Bps, file_size);

		/* download enhancement representation of this segment*/
		if ((representation_index != group->force_max_rep_index) && rep->enhancement_rep_index_plus_one) {
			group->active_rep_index = rep->enhancement_rep_index_plus_one - 1;
			group->has_pending_enhancement = GF_TRUE;
		}
		/* if we have downloaded all enhancement representations of this segment, restart from base representation and increase dowloaded segment index by 1*/
		else {
			if (group->base_rep_index_plus_one) group->active_rep_index = group->base_rep_index_plus_one - 1;
			if (dash->speed >= 0) {
				group->download_segment_index++;
			} else if (group->download_segment_index) {
				group->download_segment_index--;
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Playing in backward - start of playlist reached - assuming end of stream\n"));
				group->done = 1;
			}
			group->has_pending_enhancement = GF_FALSE;
		}
		if (dash->auto_switch_count) {
			group->nb_segments_done++;
			if (group->nb_segments_done==dash->auto_switch_count) {
				group->nb_segments_done=0;
				gf_dash_skip_disabled_representation(group, rep, GF_TRUE);
			}
		}

		gf_mx_v(group->cache_mutex);

		//do not notify segments if there is a pending period switch - since these are decided by the user, we don't
		//want to notify old segments
		if (!dash->request_period_switch && !group->has_pending_enhancement && !has_dep_following)
			dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_SEGMENT_AVAILABLE, gf_list_find(dash->groups, base_group), GF_OK);

	}
	if (new_base_seg_url) gf_free(new_base_seg_url);
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

			res = dash_download_group(dash, dep_group, base_group, has_dep_following);
			if (res==GF_DASH_DownloadRestart) {
				i--;
				continue;
			}
			group->current_dep_idx = i + 1;
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
	GF_MPD_Representation *rep;
	u32 total_rate, bandwidths[20], groups_per_quality[20], max_level;
	u32 q_idx, nb_qualities = 0;
	u32 i, count = gf_list_count(dash->groups), local_files = 0;

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
		if (!group->bytes_per_sec) continue;

		//keep min rate to perform rate adaptation
		if (total_rate > group->bytes_per_sec)
			total_rate = group->bytes_per_sec;
	}
	if (total_rate == (u32) -1) {
		total_rate = 0;
	}
	if (local_files==count) {
		total_rate = dash->dash_io->get_bytes_per_sec(dash->dash_io, NULL);
	}
	if (!total_rate) return;


	for (q_idx=0; q_idx<nb_qualities; q_idx++) {
		bandwidths[q_idx] = 0;
		groups_per_quality[q_idx] = 0;

		for (i=0; i<count; i++) {
			GF_DASH_Group *group = gf_list_get(dash->groups, i);
			if (group->selection != GF_DASH_GROUP_SELECTED) continue;

			quality_rank = gf_dash_get_tiles_quality_rank(dash, group);
			if (quality_rank >= nb_qualities) quality_rank = nb_qualities-1;
			if (quality_rank != q_idx) continue;

			group->target_new_rep = 0;
			rep = gf_list_get(group->adaptation_set->representations, group->target_new_rep);
			bandwidths[q_idx] += rep->bandwidth;
			groups_per_quality[q_idx] ++;
			if (max_level < 1 + quality_rank) max_level = 1+quality_rank;
		}
		min_bandwidth += bandwidths[q_idx];
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

					quality_rank = gf_dash_get_tiles_quality_rank(dash, group);
					if (quality_rank >= nb_qualities) quality_rank = nb_qualities-1;
					if (quality_rank != q_idx) continue;

					if (group->target_new_rep + 1 == gf_list_count(group->adaptation_set->representations))
						continue;

					nb_rep_in_qidx++;

					rep = gf_list_get(group->adaptation_set->representations, group->target_new_rep);
					diff = rep->bandwidth;
					rep = gf_list_get(group->adaptation_set->representations, group->target_new_rep+1);
					diff = rep->bandwidth - diff;

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

	//GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("DEBUG. 2. dowload at %d \n", 8*bandwidths[q_idx]/1000));
	//bandwitdh sharing done, perform rate adaptation with theses new numbers
	for (i=0; i<count; i++) {
		GF_DASH_Group *group = gf_list_get(dash->groups, i);
		if (group->selection != GF_DASH_GROUP_SELECTED) continue;

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

		if (for_postponed_only) {
			if (group->rate_adaptation_postponed)
				dash_do_rate_adaptation(dash, group);
		} else {
			dash_do_rate_adaptation(dash, group);

			if (!group->rate_adaptation_postponed)
				group->bytes_per_sec = 0;
		}
	}
}


static u32 dash_download_threaded(void *par)
{
	GF_DASH_Group *group = (GF_DASH_Group *) par;

	group->download_th_done = GF_FALSE;

	while (1) {
		DownloadGroupStatus res = dash_download_group(group->dash, group, group, group->groups_depending_on ? GF_TRUE : GF_FALSE);
		if (res==GF_DASH_DownloadRestart) {
			continue;
		}
		break;
	}
	group->download_th_done = GF_TRUE;
	return 0;
}

static u32 dash_main_thread_proc(void *par)
{
	GF_Err e;
	GF_DashClient *dash = (GF_DashClient*) par;
	u32 i, group_count, ret = 0;
	Bool go_on = GF_TRUE;
	Bool first_period_in_mpd = GF_TRUE;

	assert(dash);
	if (!dash->mpd) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Incorrect state, no dash->mpd for URL=%s, already stopped ?\n", dash->base_url));
		return 1;
	}

restart_period:

	/* Setting the download status in exclusive code */
	gf_mx_p(dash->dash_mutex);
	dash->dash_state = GF_DASH_STATE_SETUP;
	gf_mx_v(dash->dash_mutex);
	dash->in_period_setup = 1;

	/*setup period*/
	e = gf_dash_setup_period(dash);
	if (e) {
		//move to stop state before sending the error event otherwise we might deadlock when disconnecting the dash client
		dash->dash_state = GF_DASH_STATE_STOPPED;
		dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_PERIOD_SETUP_ERROR, -1, e);
		ret = 1;
		goto exit;
	}
	dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_SELECT_GROUPS, -1, GF_OK);

	e = GF_OK;
	group_count = gf_list_count(dash->groups);
	for (i=0; i<group_count; i++) {
		GF_DASH_Group *group = gf_list_get(dash->groups, i);
		if (group->selection==GF_DASH_GROUP_NOT_SELECTABLE)
			continue;

		//by default all groups are started (init seg download and buffering). They will be (de)selected by the user
		if (first_period_in_mpd) {
			gf_dash_buffer_on(group);
		}
		gf_mx_p(group->cache_mutex);
		e = gf_dash_download_init_segment(dash, group);
		gf_mx_v(group->cache_mutex);
		if (e) break;
	}
	first_period_in_mpd = 0;

	/*if error signal to the user*/
	if (e != GF_OK) {
		//move to stop state before sending the error event otherwise we might deadlock when disconnecting the dash client
		dash->dash_state = GF_DASH_STATE_STOPPED;
		dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_PERIOD_SETUP_ERROR, -1, e);
		ret = 1;
		goto exit;
	}


	dash->last_update_time = gf_sys_clock();

	gf_mx_p(dash->dash_mutex);
	dash->dash_state = GF_DASH_STATE_CONNECTING;
	gf_mx_v(dash->dash_mutex);


	/*ask the user to connect to desired groups*/
	e = dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_CREATE_PLAYBACK, -1, GF_OK);
	if (e) {
		ret = 1;
		goto exit;
	}
	if (dash->mpd_stop_request) {
		ret = 1;
		goto exit;
	}

	gf_mx_p(dash->dash_mutex);
	dash->in_period_setup = 0;
	dash->dash_state = GF_DASH_STATE_RUNNING;
	gf_mx_v(dash->dash_mutex);

	dash->min_wait_ms_before_next_request = 0;
	while (go_on) {
		Bool has_postponed_rate_adaptation = GF_FALSE;

		/*wait until next segment is needed*/
		while (!dash->mpd_stop_request) {
			u32 timer = gf_sys_clock() - dash->last_update_time;

			/*refresh MPD*/
			if (dash->force_mpd_update || (dash->mpd->minimum_update_period && (timer > dash->mpd->minimum_update_period))) {
				u32 diff = gf_sys_clock();
				if (dash->force_mpd_update || dash->mpd->minimum_update_period) {
					GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] At %d Time to update the playlist (%u ms elapsed since last refresh and min reload rate is %u)\n", gf_sys_clock() , timer, dash->mpd->minimum_update_period));
				}
				dash->force_mpd_update = 0;

				gf_mx_p(dash->dash_mutex);
				e = gf_dash_update_manifest(dash);
				gf_mx_v(dash->dash_mutex);

				group_count = gf_list_count(dash->groups);
				diff = gf_sys_clock() - diff;
				if (e) {
					if (!dash->in_error) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error updating MPD %s\n", gf_error_to_string(e)));
					}
				} else {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Updated MPD in %d ms\n", diff));
				}
			} else {
				Bool all_groups_done = GF_TRUE;
				Bool cache_full = GF_TRUE;

				has_postponed_rate_adaptation = GF_FALSE;

				/*wait if nothing is ready to be downloaded*/
				if (dash->min_wait_ms_before_next_request > 1) {
					u32 sleep_for = MIN(dash->min_wait_ms_before_next_request/2, 1000);
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] No segments available on the server until %d ms - going to sleep for %d ms\n", dash->min_wait_ms_before_next_request, sleep_for));
					gf_sleep(sleep_for);
				}

				/*check if cache is not full*/
				dash->tsb_exceeded = 0;
				dash->time_in_tsb = 0;
				for (i=0; i<group_count; i++) {
					GF_DASH_Group *group = gf_list_get(dash->groups, i);

					gf_mx_p(group->cache_mutex);

					if ((group->selection != GF_DASH_GROUP_SELECTED) || group->done || group->depend_on_group) {
						gf_mx_v(group->cache_mutex);
						continue;
					}
					all_groups_done = 0;
					if (dash->mpd->type==GF_MPD_TYPE_DYNAMIC) {
						gf_dash_group_check_time(group);
					}
					if (group->nb_cached_segments<group->max_cached_segments) {
						cache_full = 0;
					}
					if (group->rate_adaptation_postponed)
						has_postponed_rate_adaptation = GF_TRUE;

					gf_mx_v(group->cache_mutex);
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


				if (!cache_full) break;

				//seek request
				if (dash->request_period_switch==2) all_groups_done = 1;

				if (all_groups_done && dash->next_period_checked) {
					dash->next_period_checked = 1;
					//check if we can continue next period with the same groups
					if (gf_dash_is_seamless_period_switch(dash)) {
						all_groups_done = 0;
					}
				}
				if (all_groups_done && dash->request_period_switch) {
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

					dash->request_period_switch = 0;
					GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Switching to period #%d\n", dash->active_period_index+1));
					goto restart_period;
				}

				gf_sleep(30);
			}
		}

		/* stop the thread if requested */
		if (dash->mpd_stop_request) {
			break;
		}

		dash->min_wait_ms_before_next_request = 0;

		if (has_postponed_rate_adaptation) {
			dash_global_rate_adaptation(dash, GF_TRUE);
		}


		/*for each selected groups*/
		for (i=0; i<group_count; i++) {
			GF_DASH_Group *group = gf_list_get(dash->groups, i);
			if (group->selection != GF_DASH_GROUP_SELECTED) {
				if (group->nb_cached_segments && !group->dont_delete_first_segment) {
					gf_dash_group_reset(dash, group);
				}
				continue;
			}

			if (group->depend_on_group) continue;
			//not yet scheduled for download
			if (group->rate_adaptation_postponed) continue;

			if (dash->use_threaded_download) {
				group->download_th_done = GF_FALSE;
				e = gf_th_run(group->download_th, dash_download_threaded, group);
				if (e!=GF_OK) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot launch download thread for AdaptationSet #%d - error %s\n", i+1, gf_error_to_string(e)));
					group->download_th_done = GF_TRUE;
				}
			} else {
				DownloadGroupStatus res;
				group->download_th_done = GF_FALSE;
				res = dash_download_group(dash, group, group, group->groups_depending_on ? GF_TRUE : GF_FALSE);
				if (res==GF_DASH_DownloadRestart) {
					i--;
					continue;
				}
				group->download_th_done = GF_TRUE;
			}
		}

		while (dash->use_threaded_download) {
			Bool all_done = GF_TRUE;
			for (i=0; i<group_count; i++) {
				GF_DASH_Group *group = gf_list_get(dash->groups, i);
				if (group->selection != GF_DASH_GROUP_SELECTED) {
					continue;
				}
				if (group->depend_on_group) continue;
				//not yet scheduled for download
				if (group->rate_adaptation_postponed) continue;

				if (!group->download_th_done) {
					all_done = GF_FALSE;
					break;
				}
			}
			if (all_done)
				break;
			gf_sleep(1);
		}

		dash_global_rate_adaptation(dash, GF_FALSE);
	}

exit:
	/* Signal that the download thread has ended */
	gf_mx_p(dash->dash_mutex);

	/*an error occured during playback chain creation and we couldn't release our plyayback chain in time, do it now*/
	if (dash->dash_state == GF_DASH_STATE_CONNECTING)
		gf_dash_reset_groups(dash);

	dash->dash_state = GF_DASH_STATE_STOPPED;
	gf_mx_v(dash->dash_mutex);
	return ret;
}

static u32 gf_dash_period_index_from_time(GF_DashClient *dash, u64 time)
{
	u32 i, count;
	u64 cumul_start = 0;
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
		time += now;
	}


restart:
	count = gf_list_count(dash->mpd->periods);
	cumul_start = 0;
	for (i = 0; i<count; i++) {
		period = gf_list_get(dash->mpd->periods, i);

		if (period->xlink_href) {
			gf_dash_solve_period_xlink(dash, i);
			goto restart;
		}

		if ((period->start > time) || (cumul_start > time)) {
			break;
		}
		cumul_start += period->duration;

		if (time < cumul_start) {
			break;
		}
	}
	return (i>=1 ? (i-1) : 0);
}

static void gf_dash_download_stop(GF_DashClient *dash)
{
	u32 i;
	assert(dash);
	gf_mx_p(dash->dash_mutex);
	if (dash->groups) {
		for (i=0; i<gf_list_count(dash->groups); i++) {
			GF_DASH_Group *group = gf_list_get(dash->groups, i);
			assert(group);
			if ((group->selection == GF_DASH_GROUP_SELECTED) && group->segment_download) {
				if (group->segment_download)
					dash->dash_io->abort(dash->dash_io, group->segment_download);
				group->done = 1;
			}
		}
	}
	/* stop the download thread */
	dash->mpd_stop_request = GF_TRUE;
	if (dash->dash_state != GF_DASH_STATE_STOPPED) {
		dash->mpd_stop_request = 1;
		gf_mx_v(dash->dash_mutex);
		while (1) {
			/* waiting for the download thread to stop */
			gf_mx_p(dash->dash_mutex);
			if (dash->dash_state == GF_DASH_STATE_STOPPED) {
				/* it's stopped we can continue */
				gf_mx_v(dash->dash_mutex);
				break;
			}
			gf_mx_v(dash->dash_mutex);
		}
	} else {
		gf_mx_v(dash->dash_mutex);
	}
	dash->mpd_stop_request = GF_TRUE;
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
	gf_mx_p(dash->dash_mutex);

	dash->start_range_period = 0;
	start_time = 0;
	period_idx = 0;
	for (i=0; i<gf_list_count(dash->mpd->periods); i++) {
		GF_MPD_Period *period = gf_list_get(dash->mpd->periods, i);
		Double dur;

		if (period->xlink_href) {
			gf_dash_solve_period_xlink(dash, i);
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

	gf_mx_v(dash->dash_mutex);
	if (at_period_boundary) return GF_TRUE;
	return dash->request_period_switch ? 1 : 0;
}

static void gf_dash_seek_group(GF_DashClient *dash, GF_DASH_Group *group, Double seek_to, Bool is_dynamic)
{
	GF_Err e = GF_OK;
	u32 first_downloaded, last_downloaded, segment_idx, orig_idx;

	gf_mx_p(group->cache_mutex);

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
			gf_mx_v(group->cache_mutex);
			return;
		}

		group->force_segment_switch = 1;
		group->download_segment_index = segment_idx;
	} else {
		group->start_number_at_last_ast = 0;
		/*remember to adjust time in timeline steup*/
		group->start_playback_range = seek_to;
		group->timeline_setup = 0;
	}


	if (group->segment_download)
		dash->dash_io->abort(dash->dash_io, group->segment_download);

	if (group->urlToDeleteNext) {
		if (!dash->keep_files && !group->local_files)
			dash->dash_io->delete_cache_file(dash->dash_io, group->segment_download, group->urlToDeleteNext);

		gf_free(group->urlToDeleteNext);
		group->urlToDeleteNext = NULL;
	}

	if (group->segment_download) {
		dash->dash_io->abort(dash->dash_io, group->segment_download);
		dash->dash_io->del(dash->dash_io, group->segment_download);
		group->segment_download = NULL;
	}
	while (group->nb_cached_segments) {
		group->nb_cached_segments --;
		if (!dash->keep_files && !group->local_files && !group->segment_must_be_streamed)
			gf_delete_file(group->cached[group->nb_cached_segments].cache);

		gf_dash_group_reset_cache_entry(&group->cached[group->nb_cached_segments]);
	}
	group->done = 0;

	gf_mx_v(group->cache_mutex);
}

GF_EXPORT
void gf_dash_group_seek(GF_DashClient *dash, u32 group_idx, Double seek_to)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, group_idx);
	if (!group) return;
	gf_mx_p(dash->dash_mutex);
	gf_dash_seek_group(dash, group, seek_to, (dash->mpd->type==GF_MPD_TYPE_DYNAMIC) ? GF_TRUE : GF_FALSE);
	gf_mx_v(dash->dash_mutex);
}

static void gf_dash_seek_groups(GF_DashClient *dash, Double seek_time, Bool is_dynamic)
{
	u32 i;

	gf_mx_p(dash->dash_mutex);

	if (dash->active_period_index) {
		Double dur = 0;
		u32 i;
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

	gf_mx_v(dash->dash_mutex);
}


static GF_Err http_ifce_get(GF_FileDownload *getter, char *url)
{
	GF_Err e;
	GF_DASHFileIOSession *sess;
	GF_DashClient *dash = (GF_DashClient*) getter->udta;
	if (!getter->session) {
		sess = dash->dash_io->create(dash->dash_io, 1, url, -1);
		if (!sess) return GF_IO_ERR;
		getter->session = sess;
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
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot resetup session for url %s: %s\n", url, gf_error_to_string(e) ));
			return e;
		}
		sess = (GF_DASHFileIOSession *)getter->session;
	}
	e = dash->dash_io->init(dash->dash_io, sess);
	if (e) {
		dash->dash_io->del(dash->dash_io, sess);
		if (getter->session == sess)
			getter->session = NULL;
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
	} else if (strstr(manifest_url, "://")) {
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
		/* Some servers, for instance http://tv.freebox.fr, serve m3u8 as text/plain */
		if (gf_dash_is_m3u8_mime(reloc_url, mime) || strstr(reloc_url, ".m3u8") || strstr(reloc_url, ".M3U8")) {
			dash->is_m3u8 = 1;
		} else if (gf_dash_is_smooth_mime(reloc_url, mime)) {
			dash->is_smooth = 1;
		} else if (!gf_dash_is_dash_mime(mime) && !strstr(reloc_url, ".mpd") && !strstr(reloc_url, ".MPD")) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] mime '%s' for '%s' should be m3u8 or mpd\n", mime, reloc_url));
			dash->dash_io->del(dash->dash_io, dash->mpd_dnload);
			dash->mpd_dnload = NULL;
			return GF_REMOTE_SERVICE_ERROR;
		}

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
		if (strstr(manifest_url, ".m3u8"))
			dash->is_m3u8 = 1;
	}

	if (is_local) {
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

	if (dash->mpd)
		gf_mpd_del(dash->mpd);

	dash->mpd = gf_mpd_new();
	if (!dash->mpd) {
		e = GF_OUT_OF_MEM;
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot connect service: MPD creation problem %s\n", gf_error_to_string(e)));
		goto exit;
	}

	if (dash->is_m3u8) {
		if (is_local) {
			char *sep;
			strcpy(local_path, local_url);
			sep = strrchr(local_path, '.');
			if (sep) sep[0]=0;
			strcat(local_path, ".mpd");

			e = gf_m3u8_to_mpd(local_url, manifest_url, local_path, dash->reload_count, dash->mimeTypeForM3U8Segments, 0, M3U8_TO_MPD_USE_TEMPLATE, &dash->getter, dash->mpd, GF_FALSE, dash->keep_files);
		} else {
			const char *redirected_url = dash->dash_io->get_url(dash->dash_io, dash->mpd_dnload);
			if (!redirected_url) redirected_url=manifest_url;

			e = gf_m3u8_to_mpd(local_url, redirected_url, NULL, dash->reload_count, dash->mimeTypeForM3U8Segments, 0, M3U8_TO_MPD_USE_TEMPLATE, &dash->getter, dash->mpd, GF_FALSE, dash->keep_files);
		}
	} else {
		if (!dash->is_smooth && !gf_dash_check_mpd_root_type(local_url)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot connect service: wrong file type %s\n", local_url));
			dash->dash_io->del(dash->dash_io, dash->mpd_dnload);
			dash->mpd_dnload = NULL;
			return GF_URL_ERROR;
		}

		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] parsing MPD %s\n", local_url));

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
	if (!period || !gf_list_count(period->adaptation_sets) ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot start: not enough periods or representations in MPD\n"));
		e = GF_URL_ERROR;
		goto exit;
	}

	dash->mpd_stop_request = 0;
	e = gf_th_run(dash->dash_thread, dash_main_thread_proc, dash);

	return e;
exit:
	dash->dash_io->del(dash->dash_io, dash->mpd_dnload);
	dash->mpd_dnload = NULL;

	if (dash->mpd)
		gf_mpd_del(dash->mpd);
	dash->mpd = NULL;
	return e;
}

GF_EXPORT
void gf_dash_close(GF_DashClient *dash)
{
	assert(dash);
	gf_dash_download_stop(dash);

	gf_mx_p(dash->dash_mutex);
	if (dash->mpd_dnload) {
		dash->dash_io->del(dash->dash_io, dash->mpd_dnload);
		dash->mpd_dnload = NULL;
	}
	gf_mpd_getter_del_session(&dash->getter);
	if (dash->mpd)
		gf_mpd_del(dash->mpd);
	dash->mpd = NULL;

	gf_mx_v(dash->dash_mutex);

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

GF_EXPORT
GF_DashClient *gf_dash_new(GF_DASHFileIO *dash_io, u32 max_cache_duration, u32 auto_switch_count, Bool keep_files, Bool disable_switching, GF_DASHInitialSelectionMode first_select_mode, Bool enable_buffering, u32 initial_time_shift_percent)
{
	GF_DashClient *dash;
	GF_SAFEALLOC(dash, GF_DashClient);
	if (!dash) return NULL;
	dash->dash_io = dash_io;
	dash->speed = 1.0;

	//wait one segment to validate we have enough bandwidth
	dash->probe_times_before_switch = 1;
	dash->dash_thread = gf_th_new("DashClientMainThread");
	dash->dash_mutex = gf_mx_new("DashClientMainMutex");
	//FIXME: mime type for segments MUST be mp2t, webvtt or a Packed Audio file (like AAC)
	dash->mimeTypeForM3U8Segments = gf_strdup( "video/mp2t" );

	dash->max_cache_duration = max_cache_duration;
	dash->enable_buffering = enable_buffering;
	dash->initial_time_shift_value = initial_time_shift_percent;

	dash->auto_switch_count = auto_switch_count;
	dash->keep_files = keep_files;
	dash->disable_switching = disable_switching;
	dash->first_select_mode = first_select_mode;
	dash->idle_interval = 1000;
	dash->min_timeout_between_404 = 500;
	dash->segment_lost_after_ms = 100;
	dash->debug_group_index = -1;
	dash->tile_rate_decrease = 100;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Client created\n"));
	return dash;
}

GF_EXPORT
void gf_dash_del(GF_DashClient *dash)
{
	gf_dash_close(dash);

	gf_th_del(dash->dash_thread);
	gf_mx_del(dash->dash_mutex);

	if (dash->mimeTypeForM3U8Segments) gf_free(dash->mimeTypeForM3U8Segments);
	if (dash->base_url) gf_free(dash->base_url);

	gf_free(dash);
}

GF_EXPORT
void gf_dash_set_idle_interval(GF_DashClient *dash, u32 idle_time_ms)
{
	dash->idle_interval = idle_time_ms;
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
	GF_MPD_ProgramInfo *info = gf_list_get(dash->mpd->program_infos, 0);
	if (info) {
		*title = info->title;
		*source = info->source;
	}
}


GF_EXPORT
void gf_dash_switch_quality(GF_DashClient *dash, Bool switch_up, Bool immediate_switch)
{
	u32 i;
	for (i=0; i<gf_list_count(dash->groups); i++) {
		u32 switch_to_rep_idx = 0;
		u32 bandwidth, quality, k;
		GF_MPD_Representation *rep, *active_rep;
		GF_DASH_Group *group = gf_list_get(dash->groups, i);
		u32 current_idx = group->active_rep_index;
		if (group->selection != GF_DASH_GROUP_SELECTED) continue;

		if (group->base_rep_index_plus_one) current_idx = group->force_max_rep_index;
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

			gf_mx_p(group->cache_mutex);

			group->force_switch_bandwidth = 1;
			if (!group->base_rep_index_plus_one)
				group->force_representation_idx_plus_one = switch_to_rep_idx;
			else
				group->force_max_rep_index = switch_to_rep_idx-1;


			if (group->local_files || immediate_switch) {
				u32 keep_seg_index = 0;
				//keep all scalable enhancements of the first segment
				rep = gf_list_get(group->adaptation_set->representations, group->cached[0].representation_index);
				if (rep->enhancement_rep_index_plus_one) {
					u32 rep_idx = rep->enhancement_rep_index_plus_one;
					while (keep_seg_index + 1 < group->nb_cached_segments) {
						rep = gf_list_get(group->adaptation_set->representations, group->cached[keep_seg_index+1].representation_index);
						if (rep_idx == group->cached[keep_seg_index+1].representation_index+1) {
							keep_seg_index ++;
							rep_idx = rep->enhancement_rep_index_plus_one;
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
						gf_dash_update_buffering(group, dash);
						GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Switching quality - delete cached segment: %s\n", group->cached[group->nb_cached_segments].url));

						if (!group->local_files && group->cached[group->nb_cached_segments].cache) {
							gf_delete_file( group->cached[group->nb_cached_segments].cache );
						}
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
						if (rep->enhancement_rep_index_plus_one) {
							u32 rep_idx = rep->enhancement_rep_index_plus_one;
							while (keep_seg_index + 1 < group->nb_cached_segments) {
								rep = gf_list_get(group->adaptation_set->representations, group->cached[keep_seg_index+1].representation_index);
								if (rep_idx == group->cached[keep_seg_index+1].representation_index+1) {
									keep_seg_index ++;
									rep_idx = rep->enhancement_rep_index_plus_one;
								}
								else
									break;
							}
						}
						while (group->nb_cached_segments > keep_seg_index + 1) {
							Bool decrease_download_segment_index = (group->cached[group->nb_cached_segments-1].representation_index == current_idx) ? GF_TRUE : GF_FALSE;
							group->nb_cached_segments--;
							gf_dash_update_buffering(group, dash);
							GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Switching quality - delete cached segment: %s\n", group->cached[group->nb_cached_segments].url));

							if (!group->local_files && group->cached[group->nb_cached_segments].cache) {
								gf_delete_file( group->cached[group->nb_cached_segments].cache );
							}

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
							gf_dash_update_buffering(group, dash);
							GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Switching quality - delete cached segment: %s\n", group->cached[k].url));
							if (k != group->nb_cached_segments) {
								memmove(&group->cached[k], &group->cached[k+1], (group->nb_cached_segments-k)*sizeof(segment_cache_entry));
							}
							memset(&group->cached[group->nb_cached_segments], 0, sizeof(segment_cache_entry));
						}
					}
				}
			}
			/*resize max cached segment*/
			group->max_cached_segments = nb_cached_seg_per_rep * gf_dash_group_count_rep_needed(group);

			if (group->srd_desc)
				gf_dash_set_tiles_quality(dash, group->srd_desc);

			gf_mx_v(group->cache_mutex);
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

	/*solve dependencies if any*/
	rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);

	while (rep && rep->dependency_id) {
		char *sep = strchr(rep->dependency_id, ' ');
		if (sep) sep[0] = 0;
		rep = gf_dash_find_rep(dash, rep->dependency_id, &group);
		if (sep) sep[0] = ' ';
	}

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
const char *gf_dash_group_get_segment_init_keys(GF_DashClient *dash, u32 idx, bin128 *key_IV)
{
	GF_MPD_Representation *rep;
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return NULL;

	rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);
	if (!rep) return NULL;

	if (key_IV) memcpy(*key_IV, rep->playback.key_IV, sizeof(bin128));
	return rep->playback.key_url;
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
	if (select && (group->adaptation_set->group>=0)) {
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
	//TODO: recompute grop download index based on current playback ...
	if (needs_resetup) {

	}
}

GF_EXPORT
void gf_dash_groups_set_language(GF_DashClient *dash, const char *lang_code_rfc_5646)
{
	u32 i, len;
	s32 lang_idx;
	char *sep;
	GF_List *groups_selected;
	if (!lang_code_rfc_5646) return;

	groups_selected = gf_list_new();

	gf_mx_p(dash->dash_mutex);

	//first pass, check exact match
	for (i=0; i<gf_list_count(dash->groups); i++) {
		GF_DASH_Group *group = gf_list_get(dash->groups, i);
		if (group->selection==GF_DASH_GROUP_NOT_SELECTABLE) continue;
		if (!group->adaptation_set->lang) continue;

		if (!stricmp(group->adaptation_set->lang, lang_code_rfc_5646)) {
			gf_dash_group_select(dash, i, 1);
			gf_list_add(groups_selected, group);
		}
	}

	lang_idx = gf_lang_find(lang_code_rfc_5646);
	if (lang_idx>=0) {
		const char *n2cc = gf_lang_get_2cc(lang_idx);
		const char *n3cc = gf_lang_get_3cc(lang_idx);

		for (i=0; i<gf_list_count(dash->groups); i++) {
			GF_DASH_Group *group = gf_list_get(dash->groups, i);
			if (group->selection==GF_DASH_GROUP_NOT_SELECTABLE) continue;
			if (!group->adaptation_set->lang) continue;
			if (gf_list_find(groups_selected, group) >= 0) continue;

			//check we didn't select one AS in this group in the previous pass or in this pass
			if (group->adaptation_set->group>=0) {
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
			}

			if (sep) sep[0] = '-';
		}
	}

	gf_mx_v(dash->dash_mutex);

	gf_list_del(groups_selected);
}

GF_EXPORT
Bool gf_dash_is_running(GF_DashClient *dash)
{
	return (dash->dash_state==GF_DASH_STATE_STOPPED) ? 0 : 1;
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
	Bool res = (dash->active_period_index+1 < gf_list_count(dash->mpd->periods)) ? 0 : 1;
	if (res && dash->mpd->type==GF_MPD_TYPE_DYNAMIC) {
		GF_MPD_Period*period = gf_list_last(dash->mpd->periods);
		//consider we are
		if (!period->duration || dash->mpd->media_presentation_duration) res = GF_FALSE;
	}
	return res;
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
	for (i=0; i<gf_list_count(dash->groups); i++) {
		GF_DASH_Group *group = (GF_DASH_Group *)gf_list_get(dash->groups, i);
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
	}
}


GF_EXPORT
u32 gf_dash_group_get_max_segments_in_cache(GF_DashClient *dash, u32 idx)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	return group->max_cached_segments;
}


GF_EXPORT
u32 gf_dash_group_get_num_segments_ready(GF_DashClient *dash, u32 idx, Bool *group_is_done)
{
	u32 res = 0;
	GF_DASH_Group *group;

	gf_mx_p(dash->dash_mutex);
	group = gf_list_get(dash->groups, idx);
	gf_mx_p(group->cache_mutex);

	*group_is_done = 0;
	if (!group) {
		*group_is_done = 1;
	} else {
		*group_is_done = group->done;
		res = group->nb_cached_segments;

		if (group->buffering) {
			res = 0;
		}
	}
	gf_mx_v(group->cache_mutex);
	gf_mx_v(dash->dash_mutex);
	return res;
}

GF_EXPORT
void gf_dash_group_discard_segment(GF_DashClient *dash, u32 idx)
{
	GF_DASH_Group *group;
	Bool delete_next;

	gf_mx_p(dash->dash_mutex);
	group = gf_list_get(dash->groups, idx);
	gf_mx_p(group->cache_mutex);

discard_segment:
	if (!group->nb_cached_segments) {
		gf_mx_v(group->cache_mutex);
		gf_mx_v(dash->dash_mutex);
		return;
	}
	delete_next = group->cached[0].has_dep_following ? GF_TRUE : GF_FALSE;

	if (group->cached[0].cache) {
		if (group->urlToDeleteNext) {
			if (!group->local_files && !dash->keep_files && strncmp(group->urlToDeleteNext, "gmem://", 7) )
				dash->dash_io->delete_cache_file(dash->dash_io, group->segment_download, group->urlToDeleteNext);

			gf_free(group->urlToDeleteNext);
			group->urlToDeleteNext = NULL;
		}
		assert(group->cached[0].url);

		if (group->dont_delete_first_segment) {
			group->dont_delete_first_segment = 0;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] deleting cache file %s : %s (kept in HTTP cache)\n", group->cached[0].url, group->cached[0].cache));
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] deleting cache file %s : %s\n", group->cached[0].url, group->cached[0].cache));
			group->urlToDeleteNext = gf_strdup( group->cached[0].url );
		}

		//remember the representation index of the last segment
		group->prev_active_rep_index = group->cached[0].representation_index;

		gf_dash_group_reset_cache_entry(&group->cached[0]);
	}

	memmove(&group->cached[0], &group->cached[1], sizeof(segment_cache_entry)*(group->nb_cached_segments-1));
	memset(&(group->cached[group->nb_cached_segments-1]), 0, sizeof(segment_cache_entry));
	group->nb_cached_segments--;

	if (delete_next) {
		goto discard_segment;
	}

	/*if we have dependency representations, we need also discard them*/
	if (group->base_rep_index_plus_one) {
		if (group->cached[0].cache && (group->cached[0].representation_index != group->base_rep_index_plus_one-1))
			goto discard_segment;
	}

	gf_mx_v(group->cache_mutex);
	gf_mx_v(dash->dash_mutex);
}

GF_EXPORT
void gf_dash_set_group_done(GF_DashClient *dash, u32 idx, Bool done)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (group) {
		gf_mx_p(dash->dash_mutex);
		gf_mx_p(group->cache_mutex);
		group->done = done;
		if (done && group->segment_download) {
			group->download_abort_type = 1;
			dash->dash_io->abort(dash->dash_io, group->segment_download);
		}
		gf_mx_v(group->cache_mutex);
		gf_mx_v(dash->dash_mutex);
	}
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

	gf_mx_p(dash->dash_mutex);
	group = gf_list_get(dash->groups, idx);

	if (!group) {
		gf_mx_v(dash->dash_mutex);
		return GF_BAD_PARAM;
	}

	gf_mx_p(group->cache_mutex);

	if (!group->nb_cached_segments) {
		gf_mx_v(group->cache_mutex);
		gf_mx_v(dash->dash_mutex);
		return GF_BUFFER_TOO_SMALL;
	}

	/*check the dependent rep is in the cache and does not target next segment (next in time)*/
	has_dep_following = group->cached[0].has_dep_following;
	index = 0;
	while (dependent_representation_index) {
		GF_Err err = GF_OK;

		if (has_dep_following) {
			if (index+1 >= group->nb_cached_segments)
				err = GF_BUFFER_TOO_SMALL;
			else if (! group->cached[index].has_dep_following)
				err = GF_BAD_PARAM;
		} else {
			GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, group->cached[index].representation_index);

			if (index+1 >= group->nb_cached_segments) err = GF_BUFFER_TOO_SMALL;
			else if (!rep->enhancement_rep_index_plus_one) err = GF_BAD_PARAM;
			else if (rep->enhancement_rep_index_plus_one != group->cached[index+1].representation_index + 1) err = GF_BAD_PARAM;
		}

		if (err) {
			gf_mx_v(dash->dash_mutex);
			return err;
		}
		index ++;
		dependent_representation_index--;
	}
	assert(dependent_representation_index==0);

	*url = group->cached[index].cache;
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

	if (group->cached[index].has_dep_following) {
		if (has_next_segment) *has_next_segment = GF_TRUE;
	} else if (group->cached[index+1].cache) {
		GF_MPD_Representation *rep;

		rep = gf_list_get(group->adaptation_set->representations, group->cached[index].representation_index);
		if (rep && (rep->enhancement_rep_index_plus_one == group->cached[index+1].representation_index+1) ) {
			if (has_next_segment)
				*has_next_segment = GF_TRUE;
		}
	}
	gf_mx_v(group->cache_mutex);
	gf_mx_v(dash->dash_mutex);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_dash_group_probe_current_download_segment_location(GF_DashClient *dash, u32 idx, const char **url, s32 *switching_index, const char **switching_url, const char **original_url, Bool *switched)
{
	GF_DASH_Group *group;

	*url = NULL;
	if (switching_url) *switching_url = NULL;
	if (original_url) *original_url = NULL;
	if (switching_index) *switching_index = -1;

	gf_mx_p(dash->dash_mutex);
	group = gf_list_get(dash->groups, idx);
	if (!group) {
		gf_mx_v(dash->dash_mutex);
		return GF_BAD_PARAM;
	}

	if (!group->is_downloading) {
		gf_mx_v(dash->dash_mutex);
		return GF_OK;
	}

	*switched = GF_FALSE;
	if (group->download_abort_type==2) {
		group->download_abort_type = 0;
		*switched = GF_TRUE;
	}

	//no download yet
	if ( ! dash->dash_io->get_bytes_done(dash->dash_io, group->segment_download)) {
		gf_mx_v(dash->dash_mutex);
		return GF_OK;
	}

	*url = dash->dash_io->get_cache_name(dash->dash_io, group->segment_download);
	if (original_url) *original_url = dash->dash_io->get_url(dash->dash_io, group->segment_download);

	if (group->active_rep_index != group->prev_active_rep_index) {
		GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);
		if (switching_index)
			*switching_index = group->active_rep_index;
		if (switching_url)
			*switching_url = rep->playback.cached_init_segment_url;
	}
	gf_mx_v(dash->dash_mutex);
	return GF_OK;
}

GF_EXPORT
void gf_dash_seek(GF_DashClient *dash, Double start_range)
{
	Bool is_dynamic = GF_FALSE;
	gf_mx_p(dash->dash_mutex);

	GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Seek request - playing from %g\n", start_range));

	//are we live ? if so adjust start range
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
//		now += (u64) (start_range*1000);
		start_range = (Double) now;
		start_range /= 1000;

		is_dynamic = 1;
	}

	/*first check if we seek to another period*/
	if (! gf_dash_seek_periods(dash, start_range)) {
		/*if no, seek in group*/
		gf_dash_seek_groups(dash, start_range, is_dynamic);
	}
	gf_mx_v(dash->dash_mutex);
}

GF_EXPORT
Bool gf_dash_group_segment_switch_forced(GF_DashClient *dash, u32 idx)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	return group->force_segment_switch;
}

GF_EXPORT
Double gf_dash_group_current_segment_start_time(GF_DashClient *dash, u32 idx)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	return gf_dash_get_segment_start_time(group, NULL);
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
GF_Err gf_dash_group_get_representation_info(GF_DashClient *dash, u32 idx, u32 representation_idx, u32 *width, u32 *height, u32 *audio_samplerate, u32 *bandwidth, const char **codecs)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	GF_MPD_Representation *rep;
	if (!group) return GF_BAD_PARAM;
	rep = gf_list_get(group->adaptation_set->representations, representation_idx);
	if (!rep) return GF_BAD_PARAM;

	if (width) *width = rep->width ? rep->width : group->adaptation_set->width;
	if (height) *height = rep->height ? rep->height : group->adaptation_set->height;
	if (codecs) *codecs = rep->codecs ? rep->codecs : group->adaptation_set->codecs;
	if (bandwidth) *bandwidth = rep->bandwidth;
	if (audio_samplerate) *audio_samplerate = rep->samplerate ? rep->samplerate : group->adaptation_set->samplerate;

	return GF_OK;
}

GF_EXPORT
Bool gf_dash_group_loop_detected(GF_DashClient *dash, u32 idx)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	return (group && group->nb_cached_segments) ? group->cached[0].loop_detected : GF_FALSE;
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

GF_EXPORT
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
			e = gf_dash_resolve_url(dash->mpd, rep, group, dash->base_url, GF_MPD_RESOLVE_URL_MEDIA_TEMPLATE, i, &seg_url, &start_range, &end_range, &current_dur, NULL, NULL, NULL, NULL);
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
void gf_dash_debug_group(GF_DashClient *dash, s32 group_index)
{
	dash->debug_group_index = group_index;
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

	start = 0;
	for (i=0; i<=dash->active_period_index; i++) {
		period = gf_list_get(dash->mpd->periods, i);
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
GF_Err gf_dash_group_get_quality_info(GF_DashClient *dash, u32 idx, u32 quality_idx, GF_DASHQualityInfo *quality)
{
	GF_MPD_Fractional *sar;
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
	quality->is_selected = (quality_idx==group->active_rep_index) ? 1 : 0;

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
GF_Err gf_dash_group_select_quality(GF_DashClient *dash, u32 idx, const char *ID)
{
	u32 i, count;
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group || !ID) return GF_BAD_PARAM;

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
u32 gf_dash_group_get_download_rate(GF_DashClient *dash, u32 idx)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group || !group->segment_download) return 0;

	return dash->dash_io->get_bytes_per_sec(dash->dash_io, group->segment_download);
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
	GF_DASH_Group *group = (GF_DASH_Group *)gf_list_get(dash->groups, idx);
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
	GF_DASH_Group *group = (GF_DASH_Group *)gf_list_get(dash->groups, idx);
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
	return dash->utc_drift_estimate;
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
		GF_DASH_Group *group = (GF_DASH_Group *)gf_list_get(dash->groups, i);
		if (group->srd_desc) gf_dash_set_tiles_quality(dash, group->srd_desc);
	}
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
void gf_dash_set_threaded_download(GF_DashClient *dash, Bool use_threads)
{
	dash->use_threaded_download = use_threads;
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
GF_Err gf_dash_group_set_visible_rect(GF_DashClient *dash, u32 idx, u32 min_x, u32 max_x, u32 min_y, u32 max_y)
{
	u32 i, count;
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return GF_BAD_PARAM;

	if (!min_x && !max_x && !min_y && !max_y) {
		group->quality_degradation_hint = 0;
	}


	//TODO - single video, we may want to switch down quality if not a lot of the video is visible
	//we will need the zoom factor as well
	if (!group->groups_depending_on) return GF_OK;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Group Visible rect %d,%d,%d,%d \n", min_x, max_x, min_y, max_y));
	count = gf_list_count(group->groups_depending_on);
	for (i=0; i<count; i++) {
		Bool is_visible = GF_TRUE;
		GF_DASH_Group *a_group = gf_list_get(group->groups_depending_on, i);
		if (!a_group->srd_w || !a_group->srd_h) continue;

		//single rectangle case
		if (min_x<max_x) {
			if (a_group->srd_x+a_group->srd_h <min_x) is_visible = GF_FALSE;
			else if (a_group->srd_x>max_x) is_visible = GF_FALSE;
		} else {
			if ( (a_group->srd_x>max_x) && (a_group->srd_x+a_group->srd_w<min_x)) is_visible = GF_FALSE;
		}

		if (a_group->srd_y>max_y) is_visible = GF_FALSE;
		else if (a_group->srd_y+a_group->srd_h < min_y) is_visible = GF_FALSE;

		a_group->quality_degradation_hint = is_visible ? 0 : 100;

		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Group SRD %d,%d,%d,%d is %s\n", a_group->srd_x, a_group->srd_w, a_group->srd_y, a_group->srd_h, is_visible ? "visible" : "hidden"));
	}
	return GF_OK;
}



#endif //GPAC_DISABLE_DASH_CLIENT

