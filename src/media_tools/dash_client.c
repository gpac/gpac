/*
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
#include <string.h>

#ifdef _WIN32_WCE
#include <winbase.h>
#else
#include <time.h>
#endif

#ifndef GPAC_DISABLE_DASH_CLIENT

/*set to 1 if you want MPD to use SegmentTemplate if possible instead of SegmentList*/
#define M3U8_TO_MPD_USE_TEMPLATE	0

/*uncomment to only play the first representation set*/
//#define DEBUG_FIRST_SET_ONLY

typedef enum {
	GF_DASH_STATE_STOPPED = 0,
	/*period setup and playback chain creation*/
	GF_DASH_STATE_SETUP,
	/*request to start playback chain*/
	GF_DASH_STATE_CONNECTING,
	GF_DASH_STATE_RUNNING,
} GF_DASH_STATE;


typedef struct __dash_group GF_DASH_Group;

struct __dash_client
{
	GF_DASHFileIO *dash_io;

	/*interface to mpd parser - get rid of this and use the DASHIO instead ?*/
	GF_FileDownload getter;

	char *base_url;

	u32 max_cache_duration;
	u32 auto_switch_count;
	Bool keep_files, disable_switching, allow_local_mpd_update, enable_buffering;
	Bool is_m3u8;

	u64 mpd_fetch_time;
	GF_DASHInitialSelectionMode first_select_mode;

	/* MPD downloader*/
	GF_DASHFileIOSession mpd_dnload;
	/* MPD */
	GF_MPD *mpd;
	/* number of time the MPD has been reloaded and last update time*/
	u32 reload_count, last_update_time;
	/*signature of last MPD*/
	u8 lastMPDSignature[20];
	/*mime type of media segments (m3u8)*/
	char *mimeTypeForM3U8Segments;

	/* active period in MPD (only one currently supported) */
	u32 active_period_index;
	u32 request_period_switch;

	u64 start_time_in_active_period;

	Bool ignore_mpd_duration;
	u32 initial_time_shift_percent;

	/*list of groups in the active period*/
	GF_List *groups;

	/*Main Thread handling segment downloads and MPD/M3U8 update*/
	GF_Thread *dash_thread;
	/*mutex for group->cache file name access and MPD update*/
	GF_Mutex *dl_mutex;

	/* one of the above state*/
	GF_DASH_STATE dash_state;
	Bool mpd_stop_request;
	Bool in_period_setup;
	
	u32 nb_buffering;

	/* TODO - handle playback status for SPEED/SEEK through SIDX */
	Double playback_start_range;
	Double start_range_in_segment_at_next_period;
};

static void gf_dash_seek_group(GF_DashClient *dash, GF_DASH_Group *group);


typedef struct
{
	char *cache;
	char *url;
	u64 start_range, end_range;
	/*representation index in adaptation_set->representations*/
	u32 representation_index;
	Bool do_not_delete;
	u32 duration;
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

	Bool done;
	//if set, will redownload the last segment partially downloaded
	Bool force_switch_bandwidth;
	Bool min_bandwidth_selected;
	u32 download_start_time;
	u32 active_bitrate, max_bitrate, min_bitrate;
	u32 min_representation_bitrate;

	u32 nb_segments_in_rep;
	Double segment_duration;

	/*for debug purpose of templates only*/
	u32 start_number;

	Bool was_segment_base;
	/*local file playback, do not delete them*/
	Bool local_files;
	/*next segment to download for this group*/
	u32 download_segment_index;
	/*number of segments pruged since the start of the period*/
	u32 nb_segments_purged;

	u32 nb_retry_on_last_segment;
	u32 start_number_at_last_ast;
	u64 ast_at_init;

	/*next file (cached) to delete at next GF_NET_SERVICE_QUERY_NEXT for this group*/
	char * urlToDeleteNext;
	volatile u32 max_cached_segments, nb_cached_segments, max_buffer_segments;
	segment_cache_entry *cached;

	GF_DASHFileIOSession segment_download;

	const char *segment_local_url;
	/*usually 0-0 (no range) but can be non-zero when playing local MPD/DASH sessions*/
	u64 local_url_start_range, local_url_end_range;

	u32 nb_segments_done;
	u32 last_segment_time;

	Bool segment_must_be_streamed;
	Bool broken_timing;
	Bool buffering;
	Bool maybe_end_of_stream;
	u32 cache_duration;
	u32 time_at_first_reload_required;
	
	u32 force_representation_idx_plus_one;

	Bool force_segment_switch;
	Bool is_downloading, download_aborted;

	/*set when switching segment, indicates the current downloaded segment duration*/
	u64 current_downloaded_segment_duration;

	char *service_mime;

	void *udta;
};

static const char *gf_dash_get_mime_type(GF_MPD_SubRepresentation *subrep, GF_MPD_Representation *rep, GF_MPD_AdaptationSet *set)
{
	if (subrep && subrep->mime_type) return subrep->mime_type;
	if (rep && rep->mime_type) return rep->mime_type;
	if (set && set->mime_type) return set->mime_type;
	return NULL;
}

static void gf_dash_buffer_off(GF_DASH_Group *group, GF_DashClient *dash) 
{
	if (!dash->enable_buffering) return;
	if (group->buffering) { 
		assert(dash->nb_buffering); 
		dash->nb_buffering--;	
		if (!dash->nb_buffering) {
			dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_BUFFER_DONE, GF_OK); 
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Session buffering done\n"));
		}
		group->buffering = 0;
	} 
}

static void gf_dash_buffer_on(GF_DASH_Group *group, GF_DashClient *dash) 
{
	if (!dash->enable_buffering) return;
	if ((group->selection==GF_DASH_GROUP_SELECTED) && !group->buffering) {
		if (!dash->nb_buffering) {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Starting session buffering\n"));
		}
		dash->nb_buffering++; 
		group->buffering = 1;
	}
}

GF_EXPORT
void gf_dash_get_buffer_info_buffering(GF_DashClient *dash, u32 *total_buffer, u32 *media_buffered)
{
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
			}
		}
		if (*media_buffered > *total_buffer) *media_buffered  = *total_buffer;
		*total_buffer /= dash->nb_buffering;
		*media_buffered /= dash->nb_buffering;
	}
}

static void gf_dash_update_buffering(GF_DASH_Group *group, GF_DashClient *dash)
{
	if (dash->nb_buffering) {
		dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_BUFFERING, GF_OK);

		if (group->cached[0].duration && group->nb_cached_segments>=group->max_buffer_segments) 
			gf_dash_buffer_off(group, dash);
	}
}


GF_EXPORT
Bool gf_dash_check_mpd_root_type(const char *local_url)
{
	if (local_url) {
		char *rtype = gf_xml_get_root_type(local_url, NULL);
		if (rtype) {
			Bool handled = 0;
			if (!strcmp(rtype, "MPD")) {
				handled = 1;
			}
			gf_free(rtype);
			return handled;
		}
	}
	return 0;
}

static void gf_dash_group_timeline_setup(GF_MPD *mpd, GF_DASH_Group *group, u64 fetch_time)
{
	GF_MPD_SegmentTimeline *timeline = NULL;
	GF_MPD_Representation *rep = NULL;
	u32 shift, timescale;
	u64 current_time;
	u32 ast_diff, start_number;
	
	if (mpd->type==GF_MPD_TYPE_STATIC) 
		return;
	
	/*M3U8 does not use NTP sync */
	if (group->dash->is_m3u8)
		return;
	
	/*if no AST, do not use NTP sync */
	if (! group->dash->mpd->availabilityStartTime) {
		group->broken_timing = 1;
		return;
	}
	
	//temp hack 
	mpd->media_presentation_duration = 0;
	
	ast_diff = (u32) (mpd->availabilityStartTime - group->dash->mpd->availabilityStartTime);
	if (!fetch_time) fetch_time = group->dash->mpd_fetch_time;
	current_time = fetch_time;

	if (current_time < mpd->availabilityStartTime) {
#ifndef _WIN32_WCE
		time_t gtime;
		struct tm t1, t2;
		gtime = current_time;
		t1 = * gmtime(&gtime);
		gtime = mpd->availabilityStartTime / 1000;
		t2 = * gmtime(&gtime);
		t1.tm_year = t2.tm_year;
		
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error in UTC clock: current time %d-%02d-%02dT%02d:%02d:%02dZ is less than AST %d-%02d-%02dT%02d:%02d:%02dZ!\n", 
			   1900+t1.tm_year, t1.tm_mon+1, t1.tm_mday, t1.tm_hour, t1.tm_min, t1.tm_sec,
			   1900+t2.tm_year, t2.tm_mon+1, t2.tm_mday, t2.tm_hour, t2.tm_min, t2.tm_sec
		));		
#endif
		current_time = 0;
		group->broken_timing = 1;
		return;
	}
	else current_time -= mpd->availabilityStartTime;	

	if (current_time < group->period->start) current_time = 0;
	else current_time -= group->period->start;

#if 0
	{
		s32 diff = (s32) current_time - (s32) (mpd->media_presentation_duration);
		if (ABS(diff)>10) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Broken UTC timing in client or server - got Media URL is not set in segment list\n"));
				
		}
		current_time = mpd->media_presentation_duration;
	}
#endif

	if ( ((s32) mpd->time_shift_buffer_depth>=0)) {
		shift = mpd->time_shift_buffer_depth;
		shift *= group->dash->initial_time_shift_percent;
		shift /= 100;

		if (current_time < shift) current_time = 0;
		else current_time -= shift;
	}

	timeline = NULL;
	timescale=1;
	start_number=0;
	rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);

	if (group->period->segment_list) {
		if (group->period->segment_list->segment_timeline) timeline = group->period->segment_list->segment_timeline;
		if (group->period->segment_list->timescale) timescale = group->period->segment_list->timescale;
		if (group->period->segment_list->start_number) start_number = group->period->segment_list->start_number;
	}
	if (group->adaptation_set->segment_list) {
		if (group->adaptation_set->segment_list->segment_timeline) timeline = group->adaptation_set->segment_list->segment_timeline;
		if (group->adaptation_set->segment_list->timescale) timescale = group->adaptation_set->segment_list->timescale;
		if (group->adaptation_set->segment_list->start_number) start_number = group->adaptation_set->segment_list->start_number;
	}
	if (rep->segment_list) {
		if (rep->segment_list->segment_timeline) timeline = rep->segment_list->segment_timeline;
		if (rep->segment_list->timescale) timescale = rep->segment_list->timescale;
		if (rep->segment_list->start_number) start_number = rep->segment_list->start_number;
	}

	if (group->period->segment_template) {
		if (group->period->segment_template->segment_timeline) timeline = group->period->segment_template->segment_timeline;
		if (group->period->segment_template->timescale) timescale = group->period->segment_template->timescale;
		if (group->period->segment_template->start_number) start_number = group->period->segment_template->start_number;
	}
	if (group->adaptation_set->segment_template) {
		if (group->adaptation_set->segment_template->segment_timeline) timeline = group->adaptation_set->segment_template->segment_timeline;
		if (group->adaptation_set->segment_template->timescale) timescale = group->adaptation_set->segment_template->timescale;
		if (group->adaptation_set->segment_template->start_number) start_number = group->adaptation_set->segment_template->start_number;
	}
	if (rep->segment_template) {
		if (rep->segment_template->segment_timeline) timeline = rep->segment_template->segment_timeline;
		if (rep->segment_template->timescale) timescale = rep->segment_template->timescale;
		if (rep->segment_template->start_number) start_number = rep->segment_template->start_number;
	}

	if (timeline) {
		u64 start_segtime = 0;
		u64 segtime = 0;
		u32 i, seg_idx = 0;
		current_time /= 1000;
		current_time *= timescale;
		for (i=0; i<gf_list_count(timeline->entries); i++) {
			u32 repeat;
			GF_MPD_SegmentTimelineEntry *ent = gf_list_get(timeline->entries, i);
			if (!segtime) start_segtime = segtime = ent->start_time;

			repeat = 1+ent->repeat_count;
			while (repeat) {
				if ((current_time >= segtime) && (current_time < segtime + ent->duration)) {
					group->download_segment_index = seg_idx;
					group->nb_segments_in_rep = seg_idx + 10;
					GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Found segment %d for current time "LLU" is in SegmentTimeline ["LLU"-"LLU"] - cannot estimate current startNumber, default to 0 ...\n", current_time, segtime, segtime + ent->duration));
					return;
				}
				segtime += ent->duration;
				repeat--;
				seg_idx++;
			}
		}
		//NOT FOUND !!
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] current time "LLU" is NOT in SegmentTimeline ["LLU"-"LLU"] - cannot estimate current startNumber, default to 0 ...\n", current_time, start_segtime, segtime));
		group->download_segment_index = 0;
		group->nb_segments_in_rep = 10;
		group->broken_timing = 1;
		return;
	}

	if (group->segment_duration) {
		u32 nb_segs_in_update = (u32) (mpd->minimum_update_period / (1000*group->segment_duration) );
		Double nb_seg = (Double) current_time;
		nb_seg /= 1000;
		nb_seg /= group->segment_duration;
		shift = (u32) nb_seg;
		if (!group->start_number_at_last_ast) {
			group->download_segment_index = shift;
			group->start_number_at_last_ast = start_number;
			group->ast_at_init = mpd->availabilityStartTime;

			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] At current time %d ms: Initializing Timeline: startNumber=%d segmentNumber=%d segmentDuration=%g\n", current_time, start_number, shift, group->segment_duration));
		} else {
			group->download_segment_index += start_number;
			if (group->download_segment_index > group->start_number_at_last_ast) {
				group->download_segment_index -= group->start_number_at_last_ast;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] At current time %d ms: Updating Timeline: startNumber=%d segmentNumber=%d downloadSegmentIndex=%d segmentDuration=%g AST_diff=%d\n", current_time, start_number, shift, group->download_segment_index, group->segment_duration, ast_diff));
			} else {
				group->download_segment_index = shift;
				group->ast_at_init = mpd->availabilityStartTime;
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] At current time %d ms: Re-Initializing Timeline: startNumber=%d segmentNumber=%d segmentDuration=%g AST_diff=%d\n", current_time, start_number, shift, group->segment_duration, ast_diff));
			}
			group->start_number_at_last_ast = start_number;


		}

		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] UTC time indicates first segment in period is %d, MPD indicates %d segments are available ...\n", group->download_segment_index , group->nb_segments_in_rep));

		if (group->nb_segments_in_rep && (group->download_segment_index + nb_segs_in_update > group->nb_segments_in_rep)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Not enough segments (%d needed vs %d indicated) to reach period endTime indicated in MPD - ignoring MPD duration\n", nb_segs_in_update, group->nb_segments_in_rep - group->download_segment_index ));
			group->nb_segments_in_rep = shift + nb_segs_in_update;
			group->dash->ignore_mpd_duration = 1;
		}
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Segment duration unknown - cannot estimate current startNumber\n"));
	}
}





/*!
* Returns true if given mime type is a MPD file
* \param mime the mime-type to check
* \return true if mime-type if MPD-OK
*/
static Bool gf_dash_is_dash_mime(const char * mime) {
	u32 i;
	if (!mime)
		return 0;
	for (i = 0 ; GF_DASH_MPD_MIME_TYPES[i] ; i++){
		if ( !stricmp(mime, GF_DASH_MPD_MIME_TYPES[i]))
			return 1;
	}
	return 0;
}

/*!
* Returns true if mime type is an M3U8 mime-type
* \param mime The mime-type to check
* \return true if mime-type is OK for M3U8
*/
static Bool gf_dash_is_m3u8_mime(const char *url, const char * mime) {
	u32 i;
	if (!url || !mime)
		return 0;
	if (strstr(url, ".mpd") || strstr(url, ".MPD"))
		return 0;

	for (i = 0 ; GF_DASH_M3U8_MIME_TYPES[i] ; i++) {
		if ( !stricmp(mime, GF_DASH_M3U8_MIME_TYPES[i]))
			return 1;
	}
	return 0;
}


GF_EXPORT
GF_Err gf_dash_group_check_bandwidth(GF_DashClient *dash, u32 idx)
{
	Bool default_switch_mode = 0;
	u32 download_rate, set_idx, time_since_start, done, tot_size, time_until_end;
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return GF_BAD_PARAM;
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
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Downloading from set #%d at rate %d kbps but media bitrate is %d kbps - no lower bitrate available ...\n", set_idx, download_rate/1024, group->active_bitrate/1024 ));
		return GF_OK;
	}
	

	//TODO - when do we start checking ?
	if (time_since_start < 200) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Downloading from set #%ds at rate %d kbps but media bitrate is %d kbps\n", set_idx, download_rate/1024, group->active_bitrate/1024 ));
		return GF_OK;
	}

	if (time_until_end) {
		u32 i, cache_dur=0;
		for (i=1; i<group->nb_cached_segments; i++) {
			cache_dur += group->cached[i].duration;
		}
		//we have enough cache data to go until end of this download, perform rate switching at next segment
		if (time_until_end<cache_dur) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Downloading from set #%ds at rate %d kbps but media bitrate is %d kbps - %d till end of download and %d in cache - going on with download\n", set_idx, download_rate/1024, group->active_bitrate/1024,time_until_end, cache_dur ));
			return GF_OK;
		}
	}

	GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Downloading from set #%d at rate %d kbps but media bitrate is %d kbps - %d/%d in cache - killing connection and switching\n", set_idx, download_rate/1024, group->active_bitrate/1024, group->nb_cached_segments, group->max_cached_segments ));

	group->dash->dash_io->abort(group->dash->dash_io, group->segment_download);
	group->download_aborted = GF_TRUE;

	//in live we just abort current download and go to next. In onDemand, we may want to rebuffer
	default_switch_mode = (group->dash->mpd->type==GF_MPD_TYPE_DYNAMIC) ? 0 : 1;

	//if we have time to download from another rep ?
	if (group->current_downloaded_segment_duration <= time_since_start) {
		//don't force bandwidth switch (it's too late anyway, consider we lost the segment), let the rate adaptation decide
		group->force_switch_bandwidth = default_switch_mode;

		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Download time longer than segment duration - trying to resync on next segment\n"));
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
			group->force_switch_bandwidth = 1;
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Attempting to re-download at target rate %d\n", target_rate));
		}
		//cap max bitrate for next rate adaptation pass
		group->max_bitrate = target_rate;
	}
	return GF_OK;
}

/*!
* Download a file with possible retry if GF_IP_CONNECTION_FAILURE|GF_IP_NETWORK_FAILURE
* (I discovered that with my WIFI connection, I had many issues with BFM-TV downloads)
* Similar to gf_term_download_new() and gf_dm_sess_process().
* Parameters are identical to the ones of gf_term_download_new.
* \see gf_term_download_new()
*/
GF_Err gf_dash_download_resource(GF_DASHFileIO *dash_io, GF_DASHFileIOSession *sess, const char *url, u64 start_range, u64 end_range, u32 persistent_mode, GF_DASH_Group *group)
{
	s32 group_idx = -1;
	Bool had_sess = 0;
	Bool retry = 1;
	GF_Err e;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Downloading %s starting at UTC "LLU" ms\n", url, gf_net_get_utc() ));

	if (group) {
		group_idx = gf_list_find(group->dash->groups, group);
	}

	if (! *sess) {
		*sess = dash_io->create(dash_io, persistent_mode ? 1 : 0, url, group_idx);
		if (!(*sess)){
			assert(0);
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot try to download %s... OUT of memory ?\n", url));
			return GF_OUT_OF_MEM;
		}
	} else {
		had_sess = 1;
		if (persistent_mode!=2) {
			e = dash_io->setup_from_url(dash_io, *sess, url, group_idx);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot resetup session for url %s: %s\n", url, gf_error_to_string(e) ));
				return e;
			}
		}
	}
	if (group) {
		group->is_downloading = 1;
		group->download_start_time  = gf_sys_clock();
	}

retry:

	if (end_range) {
		e = dash_io->set_range(dash_io, *sess, start_range, end_range, (persistent_mode==2) ? 0 : 1);
		if (e) {
			if (had_sess) {
				dash_io->del(dash_io, *sess);
				*sess = NULL;
				return gf_dash_download_resource(dash_io, sess, url, start_range, end_range, persistent_mode ? 1 : 0, group);
			}
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot setup byte-range download for %s: %s\n", url, gf_error_to_string(e) ));
			if (group)
				group->is_downloading = 0;
			return e;
		}
	}
		assert(*sess);

	/*issue HTTP GET for headers only*/
	e = dash_io->init(dash_io, *sess);

	if (e>=GF_OK) {
		/*check mime type of the adaptation set if not provided*/
		if (group) {
			const char *mime = dash_io->get_mime(dash_io, *sess);
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
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Segment %s cannot be cached on disk, will use direct streaming\n", url));
				group->segment_must_be_streamed = 1;
				if (group->segment_download) dash_io->abort(dash_io, group->segment_download);
				group->is_downloading = 1;
				return GF_OK;
			} 
			group->segment_must_be_streamed = 0;
		} 

		/*we can download the file*/
		e = dash_io->run(dash_io, *sess);
	}
	switch (e) {
	case GF_IP_CONNECTION_FAILURE:
	case GF_IP_NETWORK_FAILURE:
	{
		dash_io->del(dash_io, *sess);
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] failed to download, retrying once with %s...\n", url));
		*sess = dash_io->create(dash_io, 0, url, group_idx);
		if (! (*sess)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot retry to download %s... OUT of memory ?\n", url));
			if (group)
				group->is_downloading = 0;
			return GF_OUT_OF_MEM;
		}

		if (retry) {
			retry = 0;
			goto retry;
		}
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] two consecutive failures, aborting the download %s.\n", url));
		break;
	}
	case GF_OK:
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Download %s complete at UTC "LLU" ms\n", url, gf_net_get_utc() ));
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] FAILED to download %s = %s...\n", url, gf_error_to_string(e)));
		break;
	}
	if (group)
		group->is_downloading = 0;
	return e;
}

static void gf_dash_get_timeline_duration(GF_MPD_SegmentTimeline *timeline, u32 *nb_segments, Double *max_seg_duration)
{
	u32 i, count;

	*nb_segments = 0;
	if (max_seg_duration) *max_seg_duration = 0;
	count = gf_list_count(timeline->entries);
	for (i=0; i<count; i++) {
		GF_MPD_SegmentTimelineEntry *ent = gf_list_get(timeline->entries, i);
		*nb_segments += 1 + ent->repeat_count;
		if (max_seg_duration && (*max_seg_duration < ent->duration)) *max_seg_duration = ent->duration;
	}
}

static void gf_dash_get_segment_duration(GF_MPD_Representation *rep, GF_MPD_AdaptationSet *set, GF_MPD_Period *period, GF_MPD *mpd, u32 *nb_segments, Double *max_seg_duration)
{
	Double mediaDuration;
	Bool single_segment = 0;
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
		if (! timescale) timescale=1;

		if (timeline) {
			gf_dash_get_timeline_duration(timeline, nb_segments, max_seg_duration);
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
		*max_seg_duration = mpd->media_presentation_duration;
		*max_seg_duration /= 1000;
		*nb_segments = 1;
		return;
	}

	single_segment = 1;
	if (period->segment_template) {
		single_segment = 0;
		if (period->segment_template->duration) duration = period->segment_template->duration;
		if (period->segment_template->timescale) timescale = period->segment_template->timescale;
		if (period->segment_template->segment_timeline) timeline = period->segment_template->segment_timeline;
	}
	if (set->segment_template) {
		single_segment = 0;
		if (set->segment_template->duration) duration = set->segment_template->duration;
		if (set->segment_template->timescale) timescale = set->segment_template->timescale;
		if (set->segment_template->segment_timeline) timeline = set->segment_template->segment_timeline;
	}
	if (rep->segment_template) {
		single_segment = 0;
		if (rep->segment_template->duration) duration = rep->segment_template->duration;
		if (rep->segment_template->timescale) timescale = rep->segment_template->timescale;
		if (rep->segment_template->segment_timeline) timeline = rep->segment_template->segment_timeline;
	}
	if (!timescale) timescale=1;

	/*if no SegmentXXX is found, this is a single segment representation (onDemand profile)*/
	if (single_segment) {
		*max_seg_duration = mpd->media_presentation_duration;
		*max_seg_duration /= 1000;
		*nb_segments = 1;
		return;
	}

	if (timeline) {
		gf_dash_get_timeline_duration(timeline, nb_segments, max_seg_duration);
		if (max_seg_duration) *max_seg_duration /= timescale;
	} else {
		if (max_seg_duration) {
			*max_seg_duration = (Double) duration;
			*max_seg_duration /= timescale;
		}
		mediaDuration = period->duration;
		if (!mediaDuration) mediaDuration = mpd->media_presentation_duration;
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

static u64 gf_dash_segment_timeline_start(GF_MPD_SegmentTimeline *timeline, u32 segment_index, u64 *segment_duration)
{
	u64 start_time = 0;
	u32 i, idx, k;
	idx = 0;
	for (i=0; i<gf_list_count(timeline->entries); i++) {
		GF_MPD_SegmentTimelineEntry *ent = gf_list_get(timeline->entries, i);
		if (ent->start_time) start_time = ent->start_time;		
		for (k=0; k<ent->repeat_count+1; k++) {
			if (idx==segment_index) {
				if (segment_duration) 
					*segment_duration = ent->duration;
				return start_time;
			}
			idx ++;
			start_time += ent->duration;
		}
	}
	return start_time;
}

static Double gf_dash_get_segment_start_time(GF_DASH_Group *group, Double *segment_duration)
{
	GF_MPD_Representation *rep;
	GF_MPD_AdaptationSet *set;
	GF_MPD_Period *period;
	Double start_time;
	u32 timescale, segment_index;
	u64 duration;
	GF_MPD_SegmentTimeline *timeline = NULL;
	timescale = 0;
	duration = 0;
	start_time = 0;

	rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);
	set = group->adaptation_set;
	period = group->period;
	segment_index = 0;
	if (group->download_segment_index >= group->nb_cached_segments) {
//		segment_index = group->download_segment_index - group->nb_cached_segments;
		segment_index = group->download_segment_index;
	}

	/*single segment: return nothing*/
	if (rep->segment_base || set->segment_base || period->segment_base) {
		return start_time;
	}
	if (rep->segment_list || set->segment_list || period->segment_list) {
		if (period->segment_list) {
			if (period->segment_list->duration) duration = period->segment_list->duration;
			if (period->segment_list->timescale) timescale = period->segment_list->timescale;
			if (period->segment_list->segment_timeline) timeline = period->segment_list->segment_timeline;
		}
		if (set->segment_list) {
			if (set->segment_list->duration) duration = set->segment_list->duration;
			if (set->segment_list->timescale) timescale = set->segment_list->timescale;
			if (set->segment_list->segment_timeline) timeline = set->segment_list->segment_timeline;
		}
		if (rep->segment_list) {
			if (rep->segment_list->duration) duration = rep->segment_list->duration;
			if (rep->segment_list->timescale) timescale = rep->segment_list->timescale;
			if (rep->segment_list->segment_timeline) timeline = rep->segment_list->segment_timeline;
		}
		if (! timescale) timescale=1;

		if (timeline) {
			start_time = (Double) gf_dash_segment_timeline_start(timeline, segment_index, &duration);
		} else {
			start_time = segment_index * (Double) duration;
		}
		start_time /= timescale;
		if (segment_duration) {
			*segment_duration = (Double) duration; 
			*segment_duration /= timescale; 
		}
		return start_time;
	}

	if (period->segment_template) {
		if (period->segment_template->duration) duration = period->segment_template->duration;
		if (period->segment_template->timescale) timescale = period->segment_template->timescale;
		if (period->segment_template->segment_timeline) timeline = period->segment_template->segment_timeline;
		if (period->segment_template->start_number) group->start_number = period->segment_template->start_number;
	}
	if (set->segment_template) {
		if (set->segment_template->duration) duration = set->segment_template->duration;
		if (set->segment_template->timescale) timescale = set->segment_template->timescale;
		if (set->segment_template->segment_timeline) timeline = set->segment_template->segment_timeline;
		if (set->segment_template->start_number) group->start_number = set->segment_template->start_number;
	}
	if (rep->segment_template) {
		if (rep->segment_template->duration) duration = rep->segment_template->duration;
		if (rep->segment_template->timescale) timescale = rep->segment_template->timescale;
		if (rep->segment_template->segment_timeline) timeline = rep->segment_template->segment_timeline;
		if (rep->segment_template->start_number) group->start_number = rep->segment_template->start_number;
	}
	if (!timescale) timescale=1;

	if (timeline) {
		start_time = (Double) gf_dash_segment_timeline_start(timeline, segment_index, &duration);
	} else {
		start_time = segment_index * (Double) duration;
	}
	start_time /= timescale;
	if (segment_duration) {
		*segment_duration = (Double) duration; 
		*segment_duration /= timescale; 
	}

	return start_time;
}

u64 gf_dash_get_segment_availability_start_time(GF_MPD *mpd, GF_DASH_Group *group, u32 segment_index, u32 *seg_dur_ms)
{
	Double seg_ast, seg_dur;
	seg_ast = gf_dash_get_segment_start_time(group, &seg_dur);
	if (seg_dur_ms) *seg_dur_ms = (u32) (seg_dur * 1000);

	seg_ast += seg_dur;
	seg_ast *= 1000;
	seg_ast += group->period->start + group->ast_at_init;
	return (u64) seg_ast;
}

static void gf_dash_resolve_duration(GF_MPD_Representation *rep, GF_MPD_AdaptationSet *set, GF_MPD_Period *period, u64 *out_duration, u32 *out_timescale, u64 *out_pts_offset, GF_MPD_SegmentTimeline **out_segment_timeline)
{
	u32 timescale = 0;
	u64 pts_offset = 0;
	GF_MPD_SegmentTimeline *segment_timeline;
	GF_MPD_MultipleSegmentBase *mbase_rep, *mbase_set, *mbase_period;

	if (out_segment_timeline) *out_segment_timeline = NULL;
	if (out_pts_offset) *out_pts_offset = 0;

	/*single media segment - duration is not known unless indicated in period*/
	if (rep->segment_base || set->segment_base || period->segment_base) {
		*out_duration = period ? period->duration : 0;
		timescale = 0;
		if (rep->segment_base && rep->segment_base->presentation_time_offset) pts_offset = rep->segment_base->presentation_time_offset;
		if (rep->segment_base && rep->segment_base->timescale) timescale = rep->segment_base->timescale;
		if (!pts_offset && set->segment_base && set->segment_base->presentation_time_offset) pts_offset = set->segment_base->presentation_time_offset;
		if (!timescale && set->segment_base && set->segment_base->timescale) timescale = set->segment_base->timescale;
		if (!pts_offset && period->segment_base && period->segment_base->presentation_time_offset) pts_offset = period->segment_base->presentation_time_offset;
		if (!timescale && period->segment_base && period->segment_base->timescale) timescale = period->segment_base->timescale;

		if (out_pts_offset) *out_pts_offset = pts_offset;
		*out_timescale = timescale ? timescale : 1;
		return;
	}
	/*we have a segment template list or template*/
	mbase_rep = rep->segment_list ? (GF_MPD_MultipleSegmentBase *) rep->segment_list : (GF_MPD_MultipleSegmentBase *) rep->segment_template;
	mbase_set = set->segment_list ? (GF_MPD_MultipleSegmentBase *)set->segment_list : (GF_MPD_MultipleSegmentBase *)set->segment_template;
	mbase_period = period->segment_list ? (GF_MPD_MultipleSegmentBase *)period->segment_list : (GF_MPD_MultipleSegmentBase *)period->segment_template;

	segment_timeline = NULL;
	if (mbase_period) segment_timeline =  mbase_period->segment_timeline;
	if (mbase_set && mbase_set->segment_timeline) segment_timeline =  mbase_set->segment_timeline;
	if (mbase_rep && mbase_rep->segment_timeline) segment_timeline =  mbase_rep->segment_timeline;

	timescale = mbase_rep ? mbase_rep->timescale : 0;
	if (!timescale && mbase_set && mbase_set->timescale) timescale = mbase_set->timescale;
	if (!timescale && mbase_period && mbase_period->timescale) timescale  = mbase_period->timescale;
	if (!timescale) timescale = 1;
	*out_timescale = timescale;

	if (out_pts_offset) {
		pts_offset = mbase_rep ? mbase_rep->presentation_time_offset : 0;
		if (!pts_offset && mbase_set && mbase_set->presentation_time_offset) pts_offset = mbase_set->presentation_time_offset;
		if (!pts_offset && mbase_period && mbase_period->presentation_time_offset) pts_offset = mbase_period->presentation_time_offset;
		*out_pts_offset = pts_offset;
	}

	if (mbase_rep && mbase_rep->duration) *out_duration = mbase_rep->duration;
	else if (mbase_set && mbase_set->duration) *out_duration = mbase_set->duration;
	else if (mbase_period && mbase_period->duration) *out_duration = mbase_period->duration;

	if (out_segment_timeline) *out_segment_timeline = segment_timeline;

	/*for SegmentTimeline, just pick the first one as an indication (exact timeline solving is not done here)*/
	if (segment_timeline) {
		GF_MPD_SegmentTimelineEntry *ent = gf_list_get(segment_timeline->entries, 0);
		if (ent) *out_duration = ent->duration;
	}
	else if (rep->segment_list) {
		GF_MPD_SegmentURL *url = gf_list_get(rep->segment_list->segment_URLs, 0);
		if (url && url->duration) *out_duration = url->duration;
	}
}

static GF_Err gf_dash_merge_segment_timeline(GF_MPD_SegmentList *old_list, GF_MPD_SegmentTemplate *old_template, GF_MPD_SegmentList *new_list, GF_MPD_SegmentTemplate *new_template, Double min_start_time)
{
	GF_MPD_SegmentTimeline *old_timeline, *new_timeline;
	u32 idx;
	u64 start_time;
	GF_MPD_SegmentTimelineEntry *first_new_entry;

	old_timeline = new_timeline = NULL;
	if (old_list && old_list->segment_timeline) {
		if (!new_list || !new_list->segment_timeline) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update playlist: segment timeline not present in new MPD segmentList\n"));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		old_timeline = old_list->segment_timeline;
		new_timeline = new_list->segment_timeline;
	} else if (old_template && old_template->segment_timeline) {
		if (!new_template || !new_template->segment_timeline) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update playlist: segment timeline not present in new MPD segmentTemplate\n"));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		old_timeline = old_template->segment_timeline;
		new_timeline = new_template->segment_timeline;
	}
	if (!old_timeline && !new_timeline) return GF_OK;

	first_new_entry  = gf_list_get(new_timeline->entries, 0);
	idx = 0;
	start_time=0;
	while (1) {
		GF_MPD_SegmentTimelineEntry *ent = gf_list_get(old_timeline->entries, 0);
		if (!ent) break;
		if (ent->start_time) 
			start_time = ent->start_time;
		/*if starttime is equal or greater than first entry in new timeline, we're done*/
		if (start_time >= first_new_entry->start_time)
			break;

		/*if entry overlaps first entry in new timeline, remove segments*/
		while (start_time + ent->duration * (1 + ent->repeat_count) > first_new_entry->start_time) {
			ent->repeat_count--;
		}
		/*we will insert the first entry in the new timeline, make sure we have a start*/
		if (!idx && !ent->start_time) 
			ent->start_time = start_time;

		start_time += ent->duration * (1 + ent->repeat_count);

		gf_list_insert(new_timeline->entries, ent, idx);
		idx ++;
		gf_list_rem(old_timeline->entries, 0);

	}
	return GF_OK;
}

static u32 gf_dash_purge_segment_timeline(GF_DASH_Group *group, Double min_start_time)
{
	u32 nb_removed, time_scale;
	u64 start_time, min_start, duration;
	GF_MPD_SegmentTimeline *timeline=NULL;
	GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);

	if (!min_start_time) return 0;
	gf_dash_resolve_duration(rep, group->adaptation_set, group->period, &duration, &time_scale, NULL, &timeline);
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

static GF_Err gf_dash_update_manifest(GF_DashClient *dash)
{
	GF_Err e;
	u32 group_idx, rep_idx, i, j;
	u64 fetch_time=0;
	GF_DOMParser *mpd_parser;
	u8 signature[sizeof(dash->lastMPDSignature)];
	GF_MPD_Period *period, *new_period;
	const char *local_url;
	char mime[128];
	char * purl;
	Double timeline_start_time;
	GF_MPD *new_mpd;

	if (!dash->mpd_dnload) {
		local_url = purl = NULL;
		if (!gf_list_count(dash->mpd->locations)) {
			FILE *t = fopen(dash->base_url, "rt");
			if (t) {
				local_url = dash->base_url;
				fclose(t);
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
		}
	} else {
		local_url = dash->dash_io->get_cache_name(dash->dash_io, dash->mpd_dnload);
		if (!local_url) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update playlist: wrong cache file %s\n", local_url));
			return GF_IO_ERR;
		}
		gf_delete_file(local_url);
		purl = gf_strdup( dash->dash_io->get_url(dash->dash_io, dash->mpd_dnload) );
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
		e = gf_dash_download_resource(dash->dash_io, &(dash->mpd_dnload), purl, 0, 0, 0, NULL);
		if (e!=GF_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update playlist: download problem %s for MPD file\n", gf_error_to_string(e)));
			gf_free(purl);
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
			gf_m3u8_to_mpd(local_url, purl, NULL, dash->reload_count, dash->mimeTypeForM3U8Segments, 0, M3U8_TO_MPD_USE_TEMPLATE, &dash->getter);
		} else if (!gf_dash_is_dash_mime(mime)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] mime '%s' should be m3u8 or mpd\n", mime));
		}

		gf_free(purl);
		purl = NULL;
	}
	fetch_time = gf_net_get_utc();

	if (!gf_dash_check_mpd_root_type(local_url)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update playlist: MPD file type is not correct %s\n", local_url));
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	if (gf_sha1_file( local_url, signature)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] : cannot SHA1 file %s\n", local_url));
		return GF_IO_ERR;
	} 

	if (! memcmp( signature, dash->lastMPDSignature, sizeof(dash->lastMPDSignature))) {

		dash->reload_count++;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] MPD file did not change for %d consecutive reloads\n", dash->reload_count));
		/*if the MPD did not change, we should refresh "soon" but cannot wait a full refresh cycle in case of 
		low latencies as we could miss a segment*/

		if (dash->is_m3u8) {
			dash->last_update_time += dash->mpd->minimum_update_period/2;
		} else {
			dash->last_update_time = gf_sys_clock();
		}
		return GF_OK;
	} 

	dash->reload_count = 0;
	memccpy(dash->lastMPDSignature, signature, sizeof(char), sizeof(dash->lastMPDSignature));

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

	/*TODO - check periods are the same !!*/
	period = gf_list_get(dash->mpd->periods, dash->active_period_index);
	new_period = gf_list_get(new_mpd->periods, dash->active_period_index);
	if (!new_period) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update playlist: missing period\n"));
		gf_mpd_del(new_mpd);
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	if (gf_list_count(period->adaptation_sets) != gf_list_count(new_period->adaptation_sets)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update playlist: missing AdaptationSet\n"));
		gf_mpd_del(new_mpd);
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	timeline_start_time = 0;
	/*if not infinity for timeShift, compute min media time before merge and adjust it*/
	if (dash->mpd->time_shift_buffer_depth != (u32) -1) {
		Double timeshift = dash->mpd->time_shift_buffer_depth;
		timeshift /= 1000;

		for (group_idx=0; group_idx<gf_list_count(dash->groups); group_idx++) {
			GF_DASH_Group *group = gf_list_get(dash->groups, group_idx);
			Double group_start = gf_dash_get_segment_start_time(group, NULL);
			if (!group_idx || (timeline_start_time > group_start) ) timeline_start_time = group_start;
		}
		/*we can rewind our segments from timeshift*/
		if (timeline_start_time > timeshift) timeline_start_time -= timeshift;
		/*we can rewind all segments*/
		else timeline_start_time = 0;
	}

	/*update segmentTimeline at Period level*/
	e = gf_dash_merge_segment_timeline(period->segment_list, period->segment_template, new_period->segment_list, new_period->segment_template, timeline_start_time);
	if (e) {
		gf_mpd_del(new_mpd);
		return e;
	}

	for (group_idx=0; group_idx<gf_list_count(dash->groups); group_idx++) {
		Double seg_dur;
		Bool reset_segment_count;
		GF_MPD_AdaptationSet *set, *new_set;
		GF_DASH_Group *group = gf_list_get(dash->groups, group_idx);

		/*update info even if the group is not selected !*/


		set = group->adaptation_set;
		new_set = gf_list_get(new_period->adaptation_sets, group_idx);

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
				u32 start;
				if (!new_rep->segment_template && !new_set->segment_template && !new_period->segment_template) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update playlist: representation does not use segment template as previous version\n"));
					gf_mpd_del(new_mpd);
					return GF_NON_COMPLIANT_BITSTREAM;
				}
				start = 0;
				/*override new startNum with old one - otherwise we would need to get rid of the entries before the start number in the MPD ...*/
				if (period->segment_template && (period->segment_template->start_number != (u32) -1) ) start = period->segment_template->start_number;
				if (set->segment_template && (set->segment_template->start_number != (u32) -1) ) start = set->segment_template->start_number;
				if (rep->segment_template && (rep->segment_template->start_number != (u32) -1) ) start = rep->segment_template->start_number;

				if (new_period->segment_template && (new_period->segment_template->start_number != (u32) -1) ) new_period->segment_template->start_number = start;
				if (new_set->segment_template && (new_set->segment_template->start_number != (u32) -1) ) new_set->segment_template->start_number = start;
				if (new_rep->segment_template && (new_rep->segment_template->start_number != (u32) -1) ) new_rep->segment_template->start_number = start;

				/*OK, this rep is fine*/
			}
			else {
				/*we're using segment list*/
				assert(rep->segment_list || group->adaptation_set->segment_list || period->segment_list);

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
					Bool found = 0;
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

				/*current representation is the active one in the group - update the number of segments*/
				if (group->active_rep_index==rep_idx) {
					group->nb_segments_in_rep = gf_list_count(new_segments);
				}
			}

			e = gf_dash_merge_segment_timeline(rep->segment_list, rep->segment_template, new_rep->segment_list, new_rep->segment_template, timeline_start_time);
			if (e) {
				gf_mpd_del(new_mpd);
				return e;
			}

			/*what else should we check ??*/


			/*switch all intertnal GPAC stuff*/
			memcpy(&new_rep->playback, &rep->playback, sizeof(GF_DASH_RepresentationPlayback));
			if (rep->playback.cached_init_segment_url) rep->playback.cached_init_segment_url = NULL;

			if (!new_rep->mime_type) {
				new_rep->mime_type = rep->mime_type;
				rep->mime_type = NULL;
			}
		}
		/*update group/period to new period*/
		j = gf_list_find(group->period->adaptation_sets, group->adaptation_set);
		group->adaptation_set = gf_list_get(new_period->adaptation_sets, j);
		group->period = new_period;

		j = gf_list_count(group->adaptation_set->representations);
		assert(j);

		/*update segmentTimeline at AdaptationSet level*/
		e = gf_dash_merge_segment_timeline(set->segment_list, set->segment_template, new_set->segment_list, new_set->segment_template, timeline_start_time);
		if (e) {
			gf_mpd_del(new_mpd);
			return e;
		}

		/*now that all possible SegmentXXX have been updated, purge them if needed: all segments ending before timeline_start_time
		will be removed from MPD*/
		if (timeline_start_time) {
			u32 nb_segments_removed = gf_dash_purge_segment_timeline(group, timeline_start_time);
			if (nb_segments_removed) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] AdaptationSet %d - removed %d segments from timeline (%d since start of the period)\n", group_idx+1, nb_segments_removed, group->nb_segments_purged));
			}
		}

		if (new_mpd->availabilityStartTime != dash->mpd->availabilityStartTime) {
			gf_dash_group_timeline_setup(new_mpd, group, fetch_time);
		}

		group->maybe_end_of_stream = 0;
		reset_segment_count = 0;
		/*compute fetchTime + minUpdatePeriod and check period end time*/
		if (new_mpd->minimum_update_period && new_mpd->media_presentation_duration) {
			u32 endTime = (u32) (fetch_time - new_mpd->availabilityStartTime - period->start);
			if (endTime > new_mpd->media_presentation_duration) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Period EndTime is signaled to %d, less than fetch time %d ! Ignoring mediaPresentationDuration\n", new_mpd->media_presentation_duration, endTime));
				new_mpd->media_presentation_duration = 0;
				reset_segment_count = 1;
			} else {
				endTime += new_mpd->minimum_update_period;
				if (endTime > new_mpd->media_presentation_duration) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Period EndTime is signaled to %d, less than fetch time + next update - maybe end of stream ?\n", new_mpd->availabilityStartTime, endTime));
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
	assert((s32) i >= 0);

	group->active_rep_index = i;
	group->active_bitrate = rep->bandwidth;
	nb_segs = group->nb_segments_in_rep;

	group->min_bandwidth_selected = 1;
	for (k=0; k<gf_list_count(group->adaptation_set->representations); k++) {
		GF_MPD_Representation *arep = gf_list_get(group->adaptation_set->representations, k);
		if (group->active_bitrate > arep->bandwidth) {
			group->min_bandwidth_selected = 0;
			break;
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

	GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Switched to representation bandwidth %d kbps\n", rep->bandwidth/1024));
	if (group->max_bitrate) GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("\tmax download bandwidth: %d kbps\n", group->max_bitrate/1024));
	if (width&&height) {
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("\tWidth %d Height %d", width, height));
		if (framerate) GF_LOG(GF_LOG_INFO, GF_LOG_DASH, (" framerate %d/%d", framerate->num, framerate->den));
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("\n"));
	} else if (samplerate) {
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("\tsamplerate %d\n", samplerate));
	}
#endif

	gf_dash_get_segment_duration(rep, set, period, group->dash->mpd, &group->nb_segments_in_rep, &group->segment_duration);

	/*if broken indication in duration restore previous seg count*/
	if (group->dash->ignore_mpd_duration)
		group->nb_segments_in_rep = nb_segs;


	timeshift = -1;
	timeshift = (s32) (rep->segment_base ? rep->segment_base->time_shift_buffer_depth : (rep->segment_list ? rep->segment_list->time_shift_buffer_depth : (rep->segment_template ? rep->segment_template->time_shift_buffer_depth : -1) ) );
	if (timeshift == -1) timeshift = (s32) (set->segment_base ? set->segment_base->time_shift_buffer_depth : (set->segment_list ? set->segment_list->time_shift_buffer_depth : (set->segment_template ? set->segment_template->time_shift_buffer_depth : -1) ) );
	if (timeshift == -1) timeshift = (s32) (period->segment_base ? period->segment_base->time_shift_buffer_depth : (period->segment_list ? period->segment_list->time_shift_buffer_depth : (period->segment_template ? period->segment_template->time_shift_buffer_depth : -1) ) );

	if (timeshift == -1) timeshift = (s32) group->dash->mpd->time_shift_buffer_depth;
	group->time_shift_buffer_depth = (u32) timeshift;
}

static void gf_dash_switch_group_representation(GF_DashClient *mpd, GF_DASH_Group *group)
{
	u32 i, bandwidth, min_bandwidth;
	GF_MPD_Representation *rep_sel = NULL;
	GF_MPD_Representation *min_rep_sel = NULL;
	Bool min_bandwidth_selected = 0;
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



typedef enum
{
	GF_DASH_RESOLVE_URL_MEDIA,
	GF_DASH_RESOLVE_URL_INIT,
	GF_DASH_RESOLVE_URL_INDEX,
} GF_DASHURLResolveType;



GF_Err gf_dash_resolve_url(GF_MPD *mpd, GF_MPD_Representation *rep, GF_DASH_Group *group, const char *mpd_url, GF_DASHURLResolveType resolve_type, u32 item_index, char **out_url, u64 *out_range_start, u64 *out_range_end, u64 *segment_duration, Bool *is_in_base_url)
{
	GF_MPD_BaseURL *url_child;
	GF_MPD_SegmentTimeline *timeline = NULL;
	u32 start_number = 1;
	GF_MPD_AdaptationSet *set = group->adaptation_set;
	GF_MPD_Period *period = group->period;
	u32 timescale;
	char *url;
	char *url_to_solve, *solved_template, *first_sep, *media_url;
	char *init_template, *index_template;

	*out_range_start = *out_range_end = 0;
	*out_url = NULL;


	if (!group->timeline_setup) {
		gf_dash_group_timeline_setup(mpd, group, 0);
		group->timeline_setup = 1;
	}




	/*resolve base URLs from document base (download location) to representation (media)*/
	url = gf_strdup(mpd_url);

	url_child = gf_list_get(mpd->base_URLs, 0);
	if (url_child) {
		char *t_url = gf_url_concatenate(url, url_child->URL);
		gf_free(url);
		url = t_url;
	}

	url_child = gf_list_get(period->base_URLs, 0);
	if (url_child) {
		char *t_url = gf_url_concatenate(url, url_child->URL);
		gf_free(url);
		url = t_url;
	}

	url_child = gf_list_get(set->base_URLs, 0);
	if (url_child) {
		char *t_url = gf_url_concatenate(url, url_child->URL);
		gf_free(url);
		url = t_url;
	}

	url_child = gf_list_get(rep->base_URLs, 0);
	if (url_child) {
		char *t_url = gf_url_concatenate(url, url_child->URL);
		gf_free(url);
		url = t_url;
	}

	gf_dash_resolve_duration(rep, set, period, segment_duration, &timescale, NULL, NULL);
	*segment_duration = (resolve_type==GF_DASH_RESOLVE_URL_MEDIA) ? (u32) ((Double) (*segment_duration) * 1000.0 / timescale) : 0;

	/*single URL*/
//	if (rep->segment_base || set->segment_base || period->segment_base) {
	if (!rep->segment_list && !set->segment_list && !period->segment_list && !rep->segment_template && !set->segment_template && !period->segment_template) {	
		GF_MPD_URL *res_url;
		GF_MPD_SegmentBase *base_seg = NULL;
		if (item_index>0) return GF_EOS;
		switch (resolve_type) {
		case GF_DASH_RESOLVE_URL_MEDIA:
			if (!url) return GF_NON_COMPLIANT_BITSTREAM;
			*out_url = url;
			return GF_OK;
		case GF_DASH_RESOLVE_URL_INIT:
		case GF_DASH_RESOLVE_URL_INDEX:
			res_url = NULL;
			base_seg = rep->segment_base;
			if (!base_seg) base_seg = set->segment_base;
			if (!base_seg) base_seg = period->segment_base;

			if (base_seg) {
				if (resolve_type == GF_DASH_RESOLVE_URL_INDEX) {
					res_url = base_seg->representation_index;
				} else {
					res_url = base_seg->initialization_segment;
				}
			}
			if (is_in_base_url) *is_in_base_url = 0;
			/*no initialization segment / index, use base URL*/
			if (res_url && res_url->sourceURL) {
				*out_url = gf_url_concatenate(url, res_url->sourceURL);
				gf_free(url);
			} else {
				*out_url = url;
				if (is_in_base_url) *is_in_base_url = 1;
			}
			if (res_url && res_url->byte_range) {
				*out_range_start = res_url->byte_range->start_range;
				*out_range_end = res_url->byte_range->end_range;
			} else if (base_seg && base_seg->index_range && (resolve_type == GF_DASH_RESOLVE_URL_INDEX)) {
				*out_range_start = base_seg->index_range->start_range;
				*out_range_end = base_seg->index_range->end_range;
			}
			return GF_OK;
		default:
			break;
		}
		gf_free(url);
		return GF_BAD_PARAM;
	}

	/*segmentList*/
	if (rep->segment_list || set->segment_list || period->segment_list) {	
		GF_MPD_URL *init_url, *index_url;
		GF_MPD_SegmentURL *segment;
		GF_List *segments = NULL;
		u32 segment_count;

		init_url = index_url = NULL;

		/*apply inheritance of attributes, lowest level having preceedence*/
		if (period->segment_list) {
			if (period->segment_list->initialization_segment) init_url = period->segment_list->initialization_segment;
			if (period->segment_list->representation_index) index_url = period->segment_list->representation_index;
			if (period->segment_list->segment_URLs) segments = period->segment_list->segment_URLs;
			if (period->segment_list->start_number != (u32) -1) start_number = period->segment_list->start_number;
			if (period->segment_list->segment_timeline) timeline = period->segment_list->segment_timeline;
		}
		if (set->segment_list) {
			if (set->segment_list->initialization_segment) init_url = set->segment_list->initialization_segment;
			if (set->segment_list->representation_index) index_url = set->segment_list->representation_index;
			if (set->segment_list->segment_URLs) segments = set->segment_list->segment_URLs;
			if (set->segment_list->start_number != (u32) -1) start_number = set->segment_list->start_number;
			if (set->segment_list->segment_timeline) timeline = set->segment_list->segment_timeline;
		}
		if (rep->segment_list) {
			if (rep->segment_list->initialization_segment) init_url = rep->segment_list->initialization_segment;
			if (rep->segment_list->representation_index) index_url = rep->segment_list->representation_index;
			if (rep->segment_list->segment_URLs) segments = rep->segment_list->segment_URLs;
			if (rep->segment_list->start_number != (u32) -1) start_number = rep->segment_list->start_number;
			if (rep->segment_list->segment_timeline) timeline = rep->segment_list->segment_timeline;
		}


		segment_count = gf_list_count(segments);

		switch (resolve_type) {
		case GF_DASH_RESOLVE_URL_INIT:
			if (init_url) {
				if (init_url->sourceURL) {
					*out_url = gf_url_concatenate(url, init_url->sourceURL);
					gf_free(url);
				} else {
					*out_url = url;
				}
				if (init_url->byte_range) {
					*out_range_start = init_url->byte_range->start_range;
					*out_range_end = init_url->byte_range->end_range;
				}
			} else {
				gf_free(url);
			}
			return GF_OK;
		case GF_DASH_RESOLVE_URL_MEDIA:
			if (!url) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Media URL is not set in segment list\n"));
				return GF_SERVICE_ERROR;
			}
			if (item_index >= segment_count) {
				gf_free(url);
				return GF_EOS;
			}
			*out_url = url;
			segment = gf_list_get(segments, item_index);
			if (segment->media) {
				*out_url = gf_url_concatenate(url, segment->media);
				gf_free(url);
			}
			if (segment->media_range) {
				*out_range_start = segment->media_range->start_range;
				*out_range_end = segment->media_range->end_range;
			}
			if (segment->duration) {
				*segment_duration = (u32) ((Double) (segment->duration) * 1000.0 / timescale);
			}
			return GF_OK;
		case GF_DASH_RESOLVE_URL_INDEX:
			if (item_index >= segment_count) {
				gf_free(url);
				return GF_EOS;
			}
			*out_url = url;
			segment = gf_list_get(segments, item_index);
			if (segment->index) {
				*out_url = gf_url_concatenate(url, segment->index);
				gf_free(url);
			}
			if (segment->index_range) {
				*out_range_start = segment->index_range->start_range;
				*out_range_end = segment->index_range->end_range;
			}
			return GF_OK;
		default:
			break;
		}
		gf_free(url);
		return GF_BAD_PARAM;
	}

	/*segmentTemplate*/
	media_url = init_template = index_template = NULL;

	/*apply inheritance of attributes, lowest level having preceedence*/
	if (period->segment_template) {
		if (period->segment_template->initialization) init_template = period->segment_template->initialization;
		if (period->segment_template->index) index_template = period->segment_template->index;
		if (period->segment_template->media) media_url = period->segment_template->media;
		if (period->segment_template->start_number != (u32) -1) start_number = period->segment_template->start_number;
		if (period->segment_template->segment_timeline) timeline = period->segment_template->segment_timeline;
	}
	if (set->segment_template) {
		if (set->segment_template->initialization) init_template = set->segment_template->initialization;
		if (set->segment_template->index) index_template = set->segment_template->index;
		if (set->segment_template->media) media_url = set->segment_template->media;
		if (set->segment_template->start_number != (u32) -1) start_number = set->segment_template->start_number;
		if (set->segment_template->segment_timeline) timeline = set->segment_template->segment_timeline;
	}
	if (rep->segment_template) {
		if (rep->segment_template->initialization) init_template = rep->segment_template->initialization;
		if (rep->segment_template->index) index_template = rep->segment_template->index;
		if (rep->segment_template->media) media_url = rep->segment_template->media;
		if (rep->segment_template->start_number != (u32) -1) start_number = rep->segment_template->start_number;
		if (rep->segment_template->segment_timeline) timeline = rep->segment_template->segment_timeline;
	}

	/*offset the start_number with the number of discarded segments (no longer in our lists)*/
	start_number += group->nb_segments_purged;

	if (!media_url) {
		GF_MPD_BaseURL *base = gf_list_get(rep->base_URLs, 0);
		if (!base) return GF_BAD_PARAM;
		media_url = base->URL;
	}
	url_to_solve = NULL;
	switch (resolve_type) {
	case GF_DASH_RESOLVE_URL_INIT:
		url_to_solve = init_template;
		break;
	case GF_DASH_RESOLVE_URL_MEDIA:
		url_to_solve = media_url;
		break;
	case GF_DASH_RESOLVE_URL_INDEX:
		url_to_solve = index_template;
		break;
	default:
		gf_free(url);
		return GF_BAD_PARAM;
	}
	if (!url_to_solve) {
		gf_free(url);
		return GF_OK;
	}
	/*let's solve the template*/
	solved_template = gf_malloc(sizeof(char)*strlen(url_to_solve)*2);
	solved_template[0] = 0;
	strcpy(solved_template, url_to_solve);
	first_sep = strchr(solved_template, '$');
	if (first_sep) first_sep[0] = 0;

	first_sep = strchr(url_to_solve, '$');
	while (first_sep) {
		char szPrintFormat[50];
		char szFormat[100];
		char *format_tag;
		char *second_sep = strchr(first_sep+1, '$');
		if (!second_sep) {
			gf_free(url);
			gf_free(solved_template);
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		second_sep[0] = 0;
		format_tag = strchr(first_sep+1, '%');
		
		if (format_tag) {
			strcpy(szPrintFormat, format_tag);
			format_tag[0] = 0;
		} else {
			strcpy(szPrintFormat, "%d");
		}
		/* identifier is $$ -> replace by $*/
		if (!strlen(first_sep+1)) {
			strcat(solved_template, "$");
		}
		else if (!strcmp(first_sep+1, "RepresentationID")) {
			strcat(solved_template, rep->id);
		}
		else if (!strcmp(first_sep+1, "Number")) {
			sprintf(szFormat, szPrintFormat, start_number + item_index);
			strcat(solved_template, szFormat);
		}
		else if (!strcmp(first_sep+1, "Index")) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Wrong template identifier Index detected - using Number instead\n\n"));
			sprintf(szFormat, szPrintFormat, start_number + item_index);
			strcat(solved_template, szFormat);
		}
		else if (!strcmp(first_sep+1, "Bandwidth")) {
			sprintf(szFormat, szPrintFormat, rep->bandwidth);
			strcat(solved_template, szFormat);
		}
		else if (!strcmp(first_sep+1, "Time")) {
			if (timeline) {
				/*uses segment timeline*/
				u32 k, nb_seg, cur_idx, nb_repeat;
				u64 time, start_time;
				nb_seg = gf_list_count(timeline->entries);
				cur_idx = 0;
				start_time=0;
				for (k=0; k<nb_seg; k++) {
					GF_MPD_SegmentTimelineEntry *ent = gf_list_get(timeline->entries, k);
					if (item_index>cur_idx+ent->repeat_count) {
						cur_idx += 1 + ent->repeat_count;
						if (ent->start_time) start_time = ent->start_time;

						start_time += ent->duration * (1 + ent->repeat_count);
						continue;
					}
					*segment_duration = ent->duration;
					*segment_duration = (u32) ((Double) (*segment_duration) * 1000.0 / timescale);
					nb_repeat = item_index - cur_idx;
					time = ent->start_time ? ent->start_time : start_time;
					time += nb_repeat * ent->duration;

					/*replace final 'd' with LLD (%lld or I64d)*/
					szPrintFormat[strlen(szPrintFormat)-1] = 0;
					strcat(szPrintFormat, &LLD[1]);
					sprintf(szFormat, szPrintFormat, time);
					strcat(solved_template, szFormat);
					break;
				}
			}
		}
		else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Unknown template identifier %s - disabling rep\n\n", first_sep+1));
			*out_url = NULL;
			gf_free(url);
			gf_free(solved_template);
			group->selection = GF_DASH_GROUP_NOT_SELECTABLE;
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		if (format_tag) format_tag[0] = '%';
		second_sep[0] = '$';
		/*look for next keyword - copy over remaining text if any*/
		first_sep = strchr(second_sep+1, '$');
		if (first_sep) first_sep[0] = 0;
		if (strlen(second_sep+1)) 
			strcat(solved_template, second_sep+1);
		if (first_sep) first_sep[0] = '$';
	}
	*out_url = gf_url_concatenate(url, solved_template);
	gf_free(url);
	gf_free(solved_template);
	return GF_OK;
}

static GF_Err gf_dash_download_init_segment(GF_DashClient *dash, GF_DASH_Group *group)
{
	GF_Err e;
	char *base_init_url;
	GF_MPD_Representation *rep;
	u64 start_range, end_range;
	/* This variable is 0 if there is a initURL, the index of first segment downloaded otherwise */
	u32 nb_segment_read = 0;
	if (!dash || !group)
		return GF_BAD_PARAM;
	gf_mx_p(dash->dl_mutex);

	assert( group->adaptation_set && group->adaptation_set->representations );
	rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);
	if (!rep) {
		gf_mx_v(dash->dl_mutex);
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Unable to find any representation, aborting.\n"));
		return GF_IO_ERR;
	}
	start_range = end_range = 0;

	e = gf_dash_resolve_url(dash->mpd, rep, group, dash->base_url, GF_DASH_RESOLVE_URL_INIT, 0, &base_init_url, &start_range, &end_range, &group->current_downloaded_segment_duration, NULL);
	if (e) {
		gf_mx_v(dash->dl_mutex);
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Unable to resolve initialization URL: %s\n", gf_error_to_string(e) ));
		return e;
	}

	/*no error and no init segment, go for media segment*/
	if (!base_init_url) {
		//if no init segment don't download first segment
#if 1
		gf_mx_v(dash->dl_mutex);
		return GF_OK;
#else
		e = gf_dash_resolve_url(dash->mpd, rep, group, dash->base_url, GF_DASH_RESOLVE_URL_MEDIA, group->download_segment_index, &base_init_url, &start_range, &end_range, &group->current_downloaded_segment_duration, NULL);
		if (e) {
			gf_mx_v(dash->dl_mutex);
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Unable to resolve media URL: %s\n", gf_error_to_string(e) ));
			return e;
		}
		nb_segment_read = 1;
#endif
	} else if (!group->bitstream_switching) {
		group->dont_delete_first_segment = 1;
	}

	if (!strstr(base_init_url, "://") || !strnicmp(base_init_url, "file://", 7) || !strnicmp(base_init_url, "gmem://", 7) || !strnicmp(base_init_url, "views://", 8)) {
		assert(!group->nb_cached_segments);
		group->cached[0].cache = gf_strdup(base_init_url);
		group->cached[0].url = gf_strdup(base_init_url);
		group->cached[0].representation_index = group->active_rep_index;
		group->prev_active_rep_index = group->active_rep_index;

		group->nb_cached_segments = 1;
		/*do not erase local files*/
		group->local_files = group->was_segment_base ? 0 : 1;
		if (group->local_files) {
			gf_dash_buffer_off(group, dash);
		}

		group->download_segment_index += nb_segment_read;
		group->segment_local_url = group->cached[0].cache;
		group->local_url_start_range = start_range;
		group->local_url_end_range = end_range;
		rep->playback.cached_init_segment_url = gf_strdup(group->segment_local_url);
		rep->playback.init_start_range = start_range;
		rep->playback.init_end_range = end_range;


		/*finally download all init segments if any*/
		if (!group->bitstream_switching) {
			u32 k;
			for (k=0; k<gf_list_count(group->adaptation_set->representations); k++) {
				char *a_base_init_url = NULL;
				u64 a_start, a_end, a_dur;
				GF_MPD_Representation *a_rep = gf_list_get(group->adaptation_set->representations, k);
				if (a_rep==rep) continue;
				if (a_rep->playback.disabled) continue;

				e = gf_dash_resolve_url(dash->mpd, a_rep, group, dash->base_url, GF_DASH_RESOLVE_URL_INIT, 0, &a_base_init_url, &a_start, &a_end, &a_dur, NULL);
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
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] First segment is %s \n", group->segment_local_url));
		gf_mx_v(dash->dl_mutex);
		gf_free(base_init_url);
		return GF_OK;
	}

	group->max_bitrate = 0;
	group->min_bitrate = (u32)-1;
	/*use persistent connection for segment downloads*/
	e = gf_dash_download_resource(dash->dash_io, &(group->segment_download), base_init_url, start_range, end_range, 1, group);

	if ((e==GF_OK) && group->force_switch_bandwidth && !dash->auto_switch_count) {
		gf_dash_switch_group_representation(dash, group);
		gf_mx_v(dash->dl_mutex);
		return gf_dash_download_init_segment(dash, group);
	}


	if (e == GF_URL_ERROR && !base_init_url) { /* We have a 404 and started with segments */
		/* It is possible that the first segment has been deleted while we made the first request...
		* so we try with the next segment on some M3U8 servers */

		gf_free(base_init_url);

		e = gf_dash_resolve_url(dash->mpd, rep, group, dash->base_url, GF_DASH_RESOLVE_URL_MEDIA, group->download_segment_index + 1, &base_init_url, &start_range, &end_range, &group->current_downloaded_segment_duration, NULL);
		if (!e) {
			gf_mx_v(dash->dl_mutex);
			return e;
		}
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("Download of first segment failed... retrying with second one : %s\n", base_init_url));
		nb_segment_read = 2;
		/*use persistent connection for segment downloads*/
		e = gf_dash_download_resource(dash->dash_io, &(group->segment_download), base_init_url, 0, 0, 1, group);
	} /* end of 404 */

	if (e!= GF_OK && !group->segment_must_be_streamed) {
		dash->mpd_stop_request = 1;
		gf_mx_v(dash->dl_mutex);
		gf_free(base_init_url);
		return e;
	} else {
		char mime[128];
		const char *mime_type;
		if (!group->nb_segments_in_rep) {
			if (dash->mpd->type==GF_MPD_TYPE_STATIC) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] 0 segments in static representation, aborting\n"));
				gf_free(base_init_url);
				gf_mx_v(dash->dl_mutex);
				return GF_BAD_PARAM;
			}
		} else if (group->nb_segments_in_rep < group->max_cached_segments) {
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
			mime_type = gf_dash_get_mime_type(NULL, rep, group->adaptation_set);
		}

		if (stricmp(mime, mime_type)) {
			Bool valid = 0;
			char *stype1, *stype2;
			stype1 = strchr(mime_type, '/');
			stype2 = strchr(mime, '/');
			if (stype1 && stype2 && !strcmp(stype1, stype2)) valid = 1;

			if (!valid && 0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Mime '%s' is not correct for '%s', it should be '%s'\n", mime, base_init_url, mime_type));
				dash->mpd_stop_request = 0;
				gf_mx_v(dash->dl_mutex);
				gf_free(base_init_url);
				base_init_url = NULL;
				return GF_BAD_PARAM;
			}
		}

		if (group->segment_must_be_streamed ) {
			group->segment_local_url = dash->dash_io->get_url(dash->dash_io, group->segment_download);
			e = GF_OK;
		} else {
			group->segment_local_url = dash->dash_io->get_cache_name(dash->dash_io, group->segment_download);
		}

		if ((e!=GF_OK) || !group->segment_local_url) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error with initialization segment: download result:%s, cache file:%s\n", gf_error_to_string(e), group->segment_local_url));
			dash->mpd_stop_request = 1;
			gf_mx_v(dash->dl_mutex);
			gf_free(base_init_url);
			return GF_BAD_PARAM;
		} 

		assert(!group->nb_cached_segments);
		group->cached[0].cache = gf_strdup(group->segment_local_url);
		group->cached[0].url = gf_strdup( dash->dash_io->get_url(dash->dash_io, group->segment_download) );
		group->cached[0].representation_index = group->active_rep_index;
		group->cached[0].duration = (u32) group->current_downloaded_segment_duration;
		rep->playback.cached_init_segment_url = gf_strdup(group->segment_local_url);
		rep->playback.init_start_range = 0;
		rep->playback.init_end_range = 0;

		group->nb_cached_segments = 1;
		group->download_segment_index += nb_segment_read;
		gf_dash_update_buffering(group, dash);

		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Adding initialization segment %s to cache: %s\n", group->segment_local_url, group->cached[0].url ));
		gf_mx_v(dash->dl_mutex);
		gf_free(base_init_url);

		/*finally download all init segments if any*/
		if (!group->bitstream_switching) {
			u32 k;
			for (k=0; k<gf_list_count(group->adaptation_set->representations); k++) {
				char *a_base_init_url = NULL;
				u64 a_start, a_end, a_dur;
				GF_MPD_Representation *a_rep = gf_list_get(group->adaptation_set->representations, k);
				if (a_rep==rep) continue;
				if (a_rep->playback.disabled) continue;

				e = gf_dash_resolve_url(dash->mpd, a_rep, group, dash->base_url, GF_DASH_RESOLVE_URL_INIT, 0, &a_base_init_url, &a_start, &a_end, &a_dur, NULL);
				if (!e && a_base_init_url) {
					e = gf_dash_download_resource(dash->dash_io, &(group->segment_download), a_base_init_url, a_start, a_end, 1, group);
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

		return GF_OK;
	}
}

static void gf_dash_skip_disabled_representation(GF_DASH_Group *group, GF_MPD_Representation *rep)
{
	s32 rep_idx = gf_list_find(group->adaptation_set->representations, rep);
	while (1) {
		rep_idx++;
		if (rep_idx==gf_list_count(group->adaptation_set->representations)) rep_idx = 0;
		rep = gf_list_get(group->adaptation_set->representations, rep_idx);
		if (!rep->playback.disabled) break;
	}
	assert(rep && !rep->playback.disabled);
	gf_dash_set_group_representation(group, rep);
}

static void gf_dash_reset_groups(GF_DashClient *dash)
{
	/*send playback destroy event*/
	dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_DESTROY_PLAYBACK, GF_OK);

	while (gf_list_count(dash->groups)) {
		GF_DASH_Group *group = gf_list_last(dash->groups);
		gf_list_rem_last(dash->groups);

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

			gf_free(group->cached[group->nb_cached_segments].cache);
			gf_free(group->cached[group->nb_cached_segments].url);
		}
		gf_free(group->cached);

		if (group->service_mime) 
			gf_free(group->service_mime);
		gf_free(group);
	}
	gf_list_del(dash->groups);
	dash->groups = NULL;
}


/* create groups (implementation of adaptations set) */
GF_Err gf_dash_setup_groups(GF_DashClient *dash)
{
	GF_Err e;
	u32 i, j, count;
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
		Bool found = 0;
		GF_MPD_AdaptationSet *set = gf_list_get(period->adaptation_sets, i);
		for (j=0; j<gf_list_count(dash->groups); j++) {
			GF_DASH_Group *group = gf_list_get(dash->groups, j);
			if (group->adaptation_set==set) {
				found = 1;
				break;
			}
		}

		if (found) continue;


		GF_SAFEALLOC(group, GF_DASH_Group);
		if (!group) return GF_OUT_OF_MEM;
		group->dash = dash;
		group->adaptation_set = set;
		group->period = period;

		group->bitstream_switching = (set->bitstream_switching || period->bitstream_switching) ? 1 : 0;

		seg_dur = 0;
		for (j=0; j<gf_list_count(set->representations); j++) {
			Double dur;
			u32 nb_seg;
			GF_MPD_Representation *rep = gf_list_get(set->representations, j);
			gf_dash_get_segment_duration(rep, set, period, dash->mpd, &nb_seg, &dur);
			if (dur>seg_dur) seg_dur = dur;

			if (rep->width>set->max_width) {
				set->max_width = rep->width;
				set->max_height = rep->height;
			}
		}

		if (!seg_dur) {
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

			/*we need one more entry in cache for segment being currently played*/
			if (group->max_cached_segments<3)
				group->max_cached_segments ++;
		}

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

	e = gf_isom_parse_box((GF_Box **) &sidx, bs);
	if (e) return e;

	GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Loading SIDX - %d entries - Earliest Presentation Time "LLD"\n", sidx->nb_refs, sidx->earliest_presentation_time));

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
			GF_SAFEALLOC(seg->media_range, GF_MPD_ByteRange);
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Found media segment size %d - duration %d - start with SAP: %d - SAP type %d - SAP Deltat Time %d\n",
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
		FILE *f = gf_f64_open(cache_name, "rb");
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
	if (f) fclose(f);
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
		mem_address+=offset;
		*box_size = GF_4CC(mem_address[0], mem_address[1], mem_address[2], mem_address[3]);
		*box_type = GF_4CC(mem_address[4], mem_address[5], mem_address[6], mem_address[7]);
	} else {
		unsigned char data[4];
		FILE *f = gf_f64_open(cache_name, "rb");
		if (!f) return GF_IO_ERR;
		if (gf_f64_seek(f, offset, SEEK_SET))
			return GF_IO_ERR;
		if (fread(data, 1, 4, f) == 4) {
			*box_size = GF_4CC(data[0], data[1], data[2], data[3]);
			if (fread(data, 1, 4, f) == 4) {
				*box_type = GF_4CC(data[0], data[1], data[2], data[3]);
			}
		}
		fclose(f);
	}
	return GF_OK;
}

static GF_Err gf_dash_setup_single_index_mode(GF_DASH_Group *group)
{
	u32 i;
	GF_Err e;
	char *init_url = NULL;
	char *index_url = NULL;
	GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, 0);
	
	if (rep->segment_template || group->adaptation_set->segment_template || group->period->segment_template) return GF_OK;
	if (rep->segment_list || group->adaptation_set->segment_list || group->period->segment_list) return GF_OK;

	/*OK we are in single-file mode, download all required indexes & co*/
	for (i=0; i<gf_list_count(group->adaptation_set->representations); i++) {
		char *sidx_file = NULL;
		u64 duration, index_start_range, index_end_range, init_start_range, init_end_range;
		Bool index_in_base, init_in_base;
		Bool init_needs_byte_range = 0;
		Bool has_seen_sidx = 0;
		Bool is_isom = 1;
		rep = gf_list_get(group->adaptation_set->representations, i);

		index_in_base = init_in_base = 0;
		e = gf_dash_resolve_url(group->dash->mpd, rep, group, group->dash->base_url, GF_DASH_RESOLVE_URL_INIT, 0, &init_url, &init_start_range, &init_end_range, &duration, &init_in_base);
		if (e) goto exit;

		e = gf_dash_resolve_url(group->dash->mpd, rep, group, group->dash->base_url, GF_DASH_RESOLVE_URL_INDEX, 0, &index_url, &index_start_range, &index_end_range, &duration, &index_in_base);
		if (e) goto exit;


		if (is_isom && (init_in_base || index_in_base)) {	
			if (!strstr(init_url, "://") || (!strnicmp(init_url, "file://", 7) || !strnicmp(init_url, "views://", 7)) ) {
				GF_SAFEALLOC(rep->segment_list, GF_MPD_SegmentList);
				rep->segment_list->segment_URLs  =gf_list_new();

				if (init_in_base) {
					GF_SAFEALLOC(rep->segment_list->initialization_segment, GF_MPD_URL);
					rep->segment_list->initialization_segment->sourceURL = gf_strdup(init_url);
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
				u32 box_type=0;
				u32 box_size=0;
				const char *cache_name;

				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Downloading init segment and SIDX for representation %s\n", init_url));

				/*download first 8 bytes and check if we do have a box starting there*/
				e = gf_dash_download_resource(group->dash->dash_io, &(group->segment_download), init_url, offset, 7, 1, group);
				if (e) goto exit;
				cache_name = group->dash->dash_io->get_cache_name(group->dash->dash_io, group->segment_download);
				e = dash_load_box_type(cache_name, offset, &box_type, &box_size);
				offset=8;
				while (box_type) {
					/*we got the moov , stop here */
					if (!index_in_base && (box_type==GF_4CC('m','o','o','v'))) {
						e = gf_dash_download_resource(group->dash->dash_io, &(group->segment_download), init_url, offset, offset+box_size-8, 2, group);
						break;
					} else {
						e = gf_dash_download_resource(group->dash->dash_io, &(group->segment_download), init_url, offset, offset+box_size-1, 2, group);
						offset += box_size;
						/*we need to refresh the cache name because of our memory astorage thing ...*/
						cache_name = group->dash->dash_io->get_cache_name(group->dash->dash_io, group->segment_download);
						e = dash_load_box_type(cache_name, offset-8, &box_type, &box_size);

						if (box_type==GF_4CC('s','i','d','x'))
							has_seen_sidx = 1;
						else if (has_seen_sidx)
							break;
	
	
					}
				}
				if (e<0) goto exit;

				if (box_type==0) {
					e = GF_ISOM_INVALID_FILE;
					goto exit;
				}
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Done downloading init segment and SIDX\n"));

				GF_SAFEALLOC(rep->segment_list, GF_MPD_SegmentList);
				rep->segment_list->segment_URLs  =gf_list_new();

				cache_name = group->dash->dash_io->get_cache_name(group->dash->dash_io, group->segment_download);
				if (init_in_base) {
					GF_SAFEALLOC(rep->segment_list->initialization_segment, GF_MPD_URL);
					rep->segment_list->initialization_segment->sourceURL = gf_strdup(cache_name);
				}
				if (index_in_base) {
					sidx_file = (char *)cache_name;
				}
			}
		}
		/*we have index url, download it*/
		if (! index_in_base) {
			e = gf_dash_download_resource(group->dash->dash_io, &(group->segment_download), index_url, index_start_range, index_end_range, 1, group);
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

static GF_Err gf_dash_setup_period(GF_DashClient *dash)
{
	GF_MPD_Period *period;
	u32 rep_i, group_i, nb_groups_ok;
	/*setup all groups*/
	gf_dash_setup_groups(dash);

	nb_groups_ok = 0;
	for (group_i=0; group_i<gf_list_count(dash->groups); group_i++) {
		GF_MPD_Representation *rep_sel;
		u32 active_rep, nb_rep;
		const char *mime_type;
		u32 nb_rep_ok = 0;
		GF_DASH_Group *group = gf_list_get(dash->groups, group_i);

		nb_rep = gf_list_count(group->adaptation_set->representations);

		if ((nb_rep>1) && !group->adaptation_set->segment_alignment && !group->adaptation_set->subsegment_alignment) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] AdaptationSet without segmentAlignment flag set - ignoring because not supported\n"));
			continue;
		}
		if (group->adaptation_set->xlink_href) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] AdaptationSet with xlink:href to %s - ignoring because not supported\n", group->adaptation_set->xlink_href));
			continue;
		}

		/*translate from single-indexed file to SegmentList*/
		gf_dash_setup_single_index_mode(group);

		/* Select the appropriate representation in the given period */
		group->min_representation_bitrate = (u32) -1;
		active_rep = 0;
		for (rep_i = 0; rep_i < nb_rep; rep_i++) {
			GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, rep_i);
			rep_sel = gf_list_get(group->adaptation_set->representations, active_rep);

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
						GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Different codec types (%s vs %s) in same AdaptationSet - disabling %s\n", rep_sel->codecs, rep->codecs, rep->codecs));
						rep->playback.disabled = 1;
						continue;
					}
					if (rep->dependency_id) {
						GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Representation dependent on representation %d - not supported\n", rep->dependency_id));
						rep->playback.disabled = 1;
						continue;
					}
					if (rep->segment_list && rep->segment_list->xlink_href) {
						GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Representation SegmentList uses xlink:href to %s - disabling because not supported\n", rep->segment_list->xlink_href));
						rep->playback.disabled = 1;
						continue;
					}
				}
			}

			switch (dash->first_select_mode) {
			case GF_DASH_SELECT_QUALITY_LOWEST:
				if (rep->quality_ranking && (rep->quality_ranking < rep_sel->quality_ranking)) {
					active_rep = rep_i;
					break;
				}/*fallthrough if quality is not indicated*/
			case GF_DASH_SELECT_BANDWIDTH_LOWEST:
				if (rep->bandwidth < rep_sel->bandwidth) {
					active_rep = rep_i;
				}
				break;
			case GF_DASH_SELECT_QUALITY_HIGHEST:
				/*fallthrough if quality is not indicated*/
				if (rep->quality_ranking > rep_sel->quality_ranking) {
					active_rep = rep_i;
					break;
				}
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

		if (dash->playback_start_range>=0) 
			gf_dash_seek_group(dash, group);

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

#ifdef DEBUG_FIRST_SET_ONLY
		break;
#endif
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

static void dash_do_rate_adaptation(GF_DashClient *dash, GF_DASH_Group *group, GF_MPD_Representation *rep)
{
	GF_MPD_Representation *new_rep;
	u32 total_size, bytes_per_sec;
	
	if (group->dash->disable_switching) return;

	total_size = dash->dash_io->get_total_size(dash->dash_io, group->segment_download);
	bytes_per_sec = dash->dash_io->get_bytes_per_sec(dash->dash_io, group->segment_download);
	group->last_segment_time = gf_sys_clock();

	if (/*(!group->buffering || !group->min_bandwidth_selected) && */ total_size && bytes_per_sec && group->current_downloaded_segment_duration) {
		Double bitrate, time;
		u32 k, dl_rate;
		Bool go_up = 0;
		bitrate = 8*total_size;
		bitrate /= group->current_downloaded_segment_duration;
		time = total_size;
		time /= bytes_per_sec;

		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Downloaded segment %d bytes in %g seconds - duration %g sec - Bandwidth (kbps): indicated %d - computed %d - download %d\n", total_size, time, group->current_downloaded_segment_duration/1000.0, rep->bandwidth/1000, (u32) bitrate, 8*bytes_per_sec/1000));

		dl_rate = 8*bytes_per_sec;
		if (rep->bandwidth < dl_rate) {
			go_up = 1;
		}
		if (dl_rate < group->min_representation_bitrate)
			dl_rate = group->min_representation_bitrate;

		/*find best bandwidth that fits our bitrate*/
		new_rep = NULL;
		for (k=0; k<gf_list_count(group->adaptation_set->representations); k++) {
			GF_MPD_Representation *arep = gf_list_get(group->adaptation_set->representations, k);
			if (dl_rate >= arep->bandwidth) {
				if (!new_rep) new_rep = arep;
				else if (go_up) {
					/*try to switch to highest bitrate below availble download rate*/
					if (arep->bandwidth > new_rep->bandwidth) {
						new_rep = arep;
					}
				} else {
					/*try to switch to highest bitrate below availble download rate*/
					if (arep->bandwidth > new_rep->bandwidth) {
						new_rep = arep;
					}
				}
			}
		}

		if (!dash->disable_switching && new_rep && (new_rep!=rep)) {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("\n\n[DASH] Download rate %d - switching representation %s from bandwidth %d bps to %d bps at UTC "LLU" ms\n\n", dl_rate, go_up ? "up" : "down", rep->bandwidth, new_rep->bandwidth, gf_net_get_utc() ));
			gf_dash_set_group_representation(group, new_rep);
		}
	}
}

static u32 dash_main_thread_proc(void *par)
{
	GF_Err e;
	GF_DashClient *dash = (GF_DashClient*) par;
	GF_MPD_Representation *rep;
	u32 i, group_count, ret = 0;
	Bool go_on = 1;
	u32 min_wait = 0;
	Bool first_period_in_mpd = 1;
	char *new_base_seg_url;
	assert(dash);
	if (!dash->mpd) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Incorrect state, no dash->mpd for URL=%s, already stopped ?\n", dash->base_url));
		return 1;
	}

restart_period:

	/* Setting the download status in exclusive code */
	gf_mx_p(dash->dl_mutex);
	dash->dash_state = GF_DASH_STATE_SETUP;
	gf_mx_v(dash->dl_mutex);

	dash->in_period_setup = 1;

	/*setup period*/
	e = gf_dash_setup_period(dash);
	if (e) {
		dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_PERIOD_SETUP_ERROR, e);
		ret = 1;
		goto exit;
	}
	dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_SELECT_GROUPS, GF_OK);

	e = GF_OK;
	group_count = gf_list_count(dash->groups);
	for (i=0; i<group_count; i++) {
		GF_DASH_Group *group = gf_list_get(dash->groups, i);
		if (group->selection != GF_DASH_GROUP_SELECTED) continue;
		if (first_period_in_mpd) {
			gf_dash_buffer_on(group, dash);
		}
		e = gf_dash_download_init_segment(dash, group);
		if (e) break;
	}
	dash->mpd_stop_request=0;
	first_period_in_mpd = 0;

	/*if error signal to the user*/
	if (e != GF_OK) {
		dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_PERIOD_SETUP_ERROR, e);
		ret = 1;
		goto exit;
	}


	dash->last_update_time = gf_sys_clock();

	gf_mx_p(dash->dl_mutex);
	dash->dash_state = GF_DASH_STATE_CONNECTING;
	gf_mx_v(dash->dl_mutex);


	/*ask the user to connect to desired groups*/
	e = dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_CREATE_PLAYBACK, GF_OK);
	if (e) {
		ret = 1;
		goto exit;
	}
	if (dash->mpd_stop_request) {
		ret = 1;
		goto exit;
	}

	gf_mx_p(dash->dl_mutex);
	dash->in_period_setup = 0;
	dash->dash_state = GF_DASH_STATE_RUNNING;
	gf_mx_v(dash->dl_mutex);

	min_wait = 0;
	while (go_on) {
		const char *local_file_name = NULL;
		const char *resource_name = NULL;

		/*wait until next segment is needed*/
		while (!dash->mpd_stop_request) {
			u32 timer = gf_sys_clock() - dash->last_update_time;

			/*refresh MPD*/
			if (dash->mpd->minimum_update_period && (timer > dash->mpd->minimum_update_period)) {
				u32 diff = gf_sys_clock();
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] At %d Time to update the playlist (%u ms elapsed since last refresh and min reoad rate is %u)\n", gf_sys_clock() , timer, dash->mpd->minimum_update_period));
				e = gf_dash_update_manifest(dash);
				group_count = gf_list_count(dash->groups);
				diff = gf_sys_clock() - diff;
				if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error updating MPD %s\n", gf_error_to_string(e)));
				} else {
					GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Updated MPD in %d ms\n", diff));
				}
			} else {
				Bool all_groups_done = 1;
				Bool cache_full = 1;

				/*wait if nothing is ready to be downloaded*/
				if (min_wait>2) {
					u32 sleep_for = MIN(min_wait, 100) /2;
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] No segments available on the server until %d ms - going to sleep\n", sleep_for));
					gf_sleep(sleep_for);
				}

				/*check if cache is not full*/
				gf_mx_p(dash->dl_mutex);
				for (i=0; i<group_count; i++) {
					GF_DASH_Group *group = gf_list_get(dash->groups, i);
					if ((group->selection != GF_DASH_GROUP_SELECTED) || group->done) continue;
					all_groups_done = 0;
					if (group->nb_cached_segments<group->max_cached_segments) {
						cache_full = 0;
						break;
					}
				}
				gf_mx_v(dash->dl_mutex);

				if (!cache_full) break;

				if (dash->request_period_switch==2) all_groups_done = 1;

				if (all_groups_done && dash->request_period_switch) {
					gf_dash_reset_groups(dash);
					if (dash->request_period_switch == 1) 
						dash->active_period_index++;

					dash->request_period_switch = 0;

					goto restart_period;
				}
				gf_sleep(30);
			}
		}

		/* stop the thread if requested */
		if (dash->mpd_stop_request) {
			go_on = 0;
			break;
		}

		min_wait = 0;

		/*for each selected groups*/
		for (i=0; i<group_count; i++) {		
			Bool in_segment_avail_time;
			u64 start_range, end_range;
			Bool use_byterange;
			u32 representation_index;
			GF_DASH_Group *group = gf_list_get(dash->groups, i);
			if (group->selection != GF_DASH_GROUP_SELECTED) continue;
			if (group->done) continue;


			if (group->nb_cached_segments>=group->max_cached_segments) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] At %d Cache is full for this group - skipping\n", gf_sys_clock() ));
				continue;
			}

			/*remember the active rep index, since group->active_rep_index may change because of bandwidth control algorithm*/
			representation_index = group->active_rep_index;
			rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);

			/* if the index of the segment to be downloaded is greater or equal to the last segment (as seen in the playlist),
			we need to check if a new playlist is ready */
			if (group->nb_segments_in_rep && (group->download_segment_index>=group->nb_segments_in_rep)) {
				u32 timer = gf_sys_clock() - dash->last_update_time;
				Bool update_playlist = 0;
				/* update of the playlist, only if indicated */
				if (dash->mpd->minimum_update_period && (timer > dash->mpd->minimum_update_period)) {
					update_playlist = 1;
				}
				/* if media_presentation_duration is 0 and we are in live, force a refresh (not in the spec but safety check*/
				else if ((dash->mpd->type==GF_MPD_TYPE_DYNAMIC) && !dash->mpd->media_presentation_duration) {
					if (group->segment_duration && (timer > group->segment_duration*1000))
						update_playlist = 1;
				}
				if (update_playlist) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Last segment in current playlist downloaded, checking updates after %u ms\n", timer));
					e = gf_dash_update_manifest(dash);
					if (e) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error updating MPD %s\n", gf_error_to_string(e)));
					}
					group_count = gf_list_count(dash->groups);
					rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);
				} else {
					gf_sleep(16);
				}
				/* Now that the playlist is up to date, we can check again */
				if (group->download_segment_index >= group->nb_segments_in_rep) {
					/* if there is a specified update period, we redo the whole process */
					if (dash->mpd->minimum_update_period || dash->mpd->type==GF_MPD_TYPE_DYNAMIC) {
						if (! group->maybe_end_of_stream) {
							u32 now = gf_sys_clock();
							GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] End of segment list reached (%d segments but idx is %d), waiting for next MPD update\n", group->nb_segments_in_rep, group->download_segment_index));
							if (group->nb_cached_segments)
								continue;
							
							if (!group->time_at_first_reload_required) {
								group->time_at_first_reload_required = now;
								continue;
							}
							if (now - group->time_at_first_reload_required < group->cache_duration)
								continue;
						
							GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Segment list has not been updated for more than %d ms - assuming end of stream\n", now - group->time_at_first_reload_required));
							group->done = 1;
							break;
						}
					} else {
						/* if not, we are really at the end of the playlist, we can quit */
						GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] End of playlist reached... downloading remaining elements..."));
						group->done = 1;
						break;
					}
				}
			}
			group->time_at_first_reload_required = 0;
			gf_mx_p(dash->dl_mutex);

			if (group->force_switch_bandwidth && !dash->auto_switch_count) {
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Forcing representation switch, retesting group"));
				gf_dash_switch_group_representation(dash, group);
				/*restart*/
				i--;
				gf_mx_v(dash->dl_mutex);
				continue;
			}

			in_segment_avail_time = 0;

			/*check availablity start time of segment in Live !!*/
			if (!group->broken_timing && (dash->mpd->type==GF_MPD_TYPE_DYNAMIC) && !dash->is_m3u8) {
				u32 clock_time = gf_sys_clock();
				s32 to_wait = 0;
				u32 seg_dur_ms=0;
				s64 segment_ast = (s64) gf_dash_get_segment_availability_start_time(dash->mpd, group, group->download_segment_index, &seg_dur_ms);
				s64 now = (s64) gf_net_get_utc();
				
				to_wait = (s32) (segment_ast - now);

				/*if segment AST is greater than now, it is not yet available ...*/
				if (to_wait > 4) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Set #%d At %d Next segment %d (AST "LLD") is not yet available on server - requesting later in %d ms\n", i+1, gf_sys_clock(), group->download_segment_index + group->start_number, segment_ast, to_wait));
					if (group->last_segment_time) {
						GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] %d ms elapsed since previous segment download\n", clock_time - group->last_segment_time));
					}
					gf_mx_v(dash->dl_mutex);
					if (!min_wait || ((u32) to_wait < min_wait))
						min_wait = to_wait;
					continue;
				} else {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Set #%d At %d Next segment %d (AST "LLD") should now be available on server since %d ms - requesting it\n", i+1, gf_sys_clock(), group->download_segment_index + group->start_number, segment_ast, -to_wait));

					if (group->last_segment_time) {
						GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] %d ms elapsed since previous segment download\n", clock_time - group->last_segment_time));
					}
					/*check if we are in the segment availability end time*/
					if (now < segment_ast + seg_dur_ms + group->time_shift_buffer_depth )
						in_segment_avail_time = 1;
				}
			}
			min_wait = 0;

			/* At this stage, there are some segments left to be downloaded */
			e = gf_dash_resolve_url(dash->mpd, rep, group, dash->base_url, GF_DASH_RESOLVE_URL_MEDIA, group->download_segment_index, &new_base_seg_url, &start_range, &end_range, &group->current_downloaded_segment_duration, NULL);
			gf_mx_v(dash->dl_mutex);
			if (e) {
				/*do something!!*/
				break;
			}
			use_byterange = (start_range || end_range) ? 1 : 0;

			if (use_byterange) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Downloading new segment: %s (range: "LLD"-"LLD")\n", new_base_seg_url, start_range, end_range));
			}

			/*local file*/
			if (!strstr(new_base_seg_url, "://") || (!strnicmp(new_base_seg_url, "file://", 7) || !strnicmp(new_base_seg_url, "gmem://", 7)) ) {
				resource_name = local_file_name = (char *) new_base_seg_url; 
				e = GF_OK;
				/*do not erase local files*/
				group->local_files = 1;
				gf_dash_buffer_off(group, dash);
				if (group->force_switch_bandwidth && !dash->auto_switch_count) {
					gf_dash_switch_group_representation(dash, group);
					/*restart*/
					i--;
					continue;
				}

			} else {
				group->max_bitrate = 0;
				group->min_bitrate = (u32)-1;
				/*use persistent connection for segment downloads*/
				if (use_byterange) {
					e = gf_dash_download_resource(dash->dash_io, &(group->segment_download), new_base_seg_url, start_range, end_range, 1, group);
				} else {
					e = gf_dash_download_resource(dash->dash_io, &(group->segment_download), new_base_seg_url, 0, 0, 1, group);
				}

				/*TODO decide what is the best, fetch from another representation or ignore ...*/
				if (e != GF_OK) {
					if (group->maybe_end_of_stream) {
						if (group->maybe_end_of_stream==2) {
							GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Couldn't get segment %s (error %s) and end of period was guessed during last update - stoping playback\n", new_base_seg_url, gf_error_to_string(e)));
							group->maybe_end_of_stream = 0;
							group->done = 1;
						}
						group->maybe_end_of_stream++;
					} else if (in_segment_avail_time) {
						GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Couldn't get segment %s during its availability period (%s) - retrying\n", new_base_seg_url, gf_error_to_string(e)));
						min_wait = 30;
					} else {
						GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error in downloading new segment: %s %s\n", new_base_seg_url, gf_error_to_string(e)));
						group->download_segment_index++;
					}
					continue;
				}

				if ((e==GF_OK) && group->force_switch_bandwidth) {
					if (!dash->auto_switch_count) {
						gf_dash_switch_group_representation(dash, group);
						/*restart*/
						i--;
						continue;
					}
					if (rep->playback.disabled) {
						gf_dash_skip_disabled_representation(group, rep);
						/*restart*/
						i--;
						continue;
					}
				}

				if (group->segment_must_be_streamed) local_file_name = dash->dash_io->get_url(dash->dash_io, group->segment_download);
				else local_file_name = dash->dash_io->get_cache_name(dash->dash_io, group->segment_download);

				resource_name = dash->dash_io->get_url(dash->dash_io, group->segment_download);

				dash_do_rate_adaptation(dash, group, rep);
			}

			if (local_file_name && (e == GF_OK || group->segment_must_be_streamed )) {
				gf_mx_p(dash->dl_mutex);
				assert(group->nb_cached_segments<group->max_cached_segments);
				assert( local_file_name );
				group->cached[group->nb_cached_segments].cache = gf_strdup(local_file_name);
				group->cached[group->nb_cached_segments].url = gf_strdup( resource_name );
				group->cached[group->nb_cached_segments].start_range = 0;
				group->cached[group->nb_cached_segments].end_range = 0;
				group->cached[group->nb_cached_segments].representation_index = representation_index;
				group->cached[group->nb_cached_segments].duration = (u32) group->current_downloaded_segment_duration;

				if (group->local_files && use_byterange) {
					group->cached[group->nb_cached_segments].start_range = start_range;
					group->cached[group->nb_cached_segments].end_range = end_range;
				}
				if (!group->local_files) {
					GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Added file to cache (%u/%u in cache): %s\n", group->nb_cached_segments+1, group->max_cached_segments, group->cached[group->nb_cached_segments].url));
				}
				group->nb_cached_segments++;
				gf_dash_update_buffering(group, dash);
				group->download_segment_index++;
				if (dash->auto_switch_count) {
					group->nb_segments_done++;
					if (group->nb_segments_done==dash->auto_switch_count) {
						group->nb_segments_done=0;
						gf_dash_skip_disabled_representation(group, rep);
					}
				}
				gf_mx_v(dash->dl_mutex);
			}
			gf_free(new_base_seg_url);
			new_base_seg_url = NULL;
		}
	}

exit:
	/* Signal that the download thread has ended */
	gf_mx_p(dash->dl_mutex);

	/*an error occured during playback chain creation and we couldn't release our plyayback chain in time, do it now*/
	if (dash->dash_state == GF_DASH_STATE_CONNECTING)
		gf_dash_reset_groups(dash);

	dash->dash_state = GF_DASH_STATE_STOPPED;
	gf_mx_v(dash->dl_mutex);
	return ret;
}

static u32 gf_dash_period_index_from_time(GF_DashClient *dash, u32 time)
{
	u32 i, count, cumul_start=0;
	GF_MPD_Period *period;
	count = gf_list_count(dash->mpd->periods);
	for (i = 0; i<count; i++) {
		period = gf_list_get(dash->mpd->periods, i);
		if ((period->start > time) || (cumul_start > time)) {
			break;
		}
		cumul_start+=period->duration;
	}
	return (i-1 >= 0 ? (i-1) : 0);
}

static void gf_dash_download_stop(GF_DashClient *dash)
{
	u32 i;
	assert( dash );
	if (dash->groups) {
		for (i=0; i<gf_list_count(dash->groups); i++) {
			GF_DASH_Group *group = gf_list_get(dash->groups, i);
			assert( group );
			if ((group->selection == GF_DASH_GROUP_SELECTED) && group->segment_download) {
				if (group->segment_download)
					dash->dash_io->abort(dash->dash_io, group->segment_download);
				group->done = 1;
			}
		}
	}
	/* stop the download thread */
	gf_mx_p(dash->dl_mutex);
	if (dash->dash_state != GF_DASH_STATE_STOPPED) {
		dash->mpd_stop_request = 1;
		gf_mx_v(dash->dl_mutex);
		while (1) {
			/* waiting for the download thread to stop */
			gf_sleep(16);
			gf_mx_p(dash->dl_mutex);
			if (dash->dash_state != GF_DASH_STATE_RUNNING) {
				/* it's stopped we can continue */
				gf_mx_v(dash->dl_mutex);
				break;
			}
			gf_mx_v(dash->dl_mutex);
		}
	} else {
		gf_mx_v(dash->dl_mutex);
	}
}



Bool gf_dash_seek_periods(GF_DashClient *dash)
{
	Double start_time;
	u32 i, period_idx;

	gf_mx_p(dash->dl_mutex);

	dash->start_range_in_segment_at_next_period = 0;
	start_time = 0;
	period_idx = 0;
	for (i=0; i<=gf_list_count(dash->mpd->periods); i++) {
		GF_MPD_Period *period = gf_list_get(dash->mpd->periods, i);
		Double dur = period->duration;
		dur /= 1000;
		if (dash->playback_start_range >= start_time) {
			if ((i+1==gf_list_count(dash->mpd->periods)) || (dash->playback_start_range < start_time + dur) ) {
				period_idx = i;
				break;
			}
		}
		start_time += dur;
	}
	if (period_idx != dash->active_period_index) {
		dash->playback_start_range -= start_time;
		dash->active_period_index = period_idx;
		dash->request_period_switch = 2;

		/*figure out default segment duration and substract from our start range request*/
		if (dash->playback_start_range) {
			Double duration;
			u32 nb_segs;
			GF_MPD_Period *period = gf_list_get(dash->mpd->periods, period_idx);
			GF_MPD_AdaptationSet *set = gf_list_get(period->adaptation_sets, 0);
			GF_MPD_Representation *rep = gf_list_get(set->representations, 0);

			gf_dash_get_segment_duration(rep, set, period, dash->mpd, &nb_segs, &duration);

			if (duration) {
				while (dash->playback_start_range - dash->start_range_in_segment_at_next_period >= duration)
					dash->start_range_in_segment_at_next_period += duration;
			}

		}
	}
	gf_mx_v(dash->dl_mutex);

	return dash->request_period_switch ? 1 : 0;
}

static void gf_dash_seek_group(GF_DashClient *dash, GF_DASH_Group *group)
{
	Double seg_start;
	u32 first_downloaded, last_downloaded, segment_idx;

	group->force_segment_switch = 0;
	if (!group->segment_duration) return;

	/*figure out where to seek*/
	segment_idx = 0;
	seg_start = 0.0;
	while (1) {
		if ((dash->playback_start_range >= seg_start) && (dash->playback_start_range < seg_start + group->segment_duration)) 
			break;
		seg_start += group->segment_duration;
		segment_idx++;
	}
	/*todo - seek to given duration*/
	dash->playback_start_range -= seg_start;

	first_downloaded = last_downloaded = group->download_segment_index;
	if (group->download_segment_index +1 >= group->nb_cached_segments) {
		first_downloaded = group->download_segment_index + 1 - group->nb_cached_segments;
	}
	/*we are seeking in our download range, just go on*/
	if ((segment_idx >= first_downloaded) && (segment_idx<=last_downloaded)) return;

	group->force_segment_switch = 1;
	group->download_segment_index = segment_idx;
	
	if (group->segment_download) 
		dash->dash_io->abort(dash->dash_io, group->segment_download);

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
		if (!dash->keep_files && !group->local_files && !group->segment_must_be_streamed)
			gf_delete_file(group->cached[group->nb_cached_segments].cache);

		gf_free(group->cached[group->nb_cached_segments].cache);
		gf_free(group->cached[group->nb_cached_segments].url);
		memset(&group->cached[group->nb_cached_segments], 0, sizeof(segment_cache_entry));
	}
}

void gf_dash_seek_groups(GF_DashClient *dash)
{
	u32 i;

	gf_mx_p(dash->dl_mutex);

	if (dash->active_period_index) {
		Double dur = 0;
		u32 i;
		for (i=0; i<dash->active_period_index; i++) {
			GF_MPD_Period *period = gf_list_get(dash->mpd->periods, dash->active_period_index-1);
			dur += period->duration/1000.0;
		}
		dash->playback_start_range -= dur;
	}
	for (i=0; i<gf_list_count(dash->groups); i++) {
		GF_DASH_Group *group = gf_list_get(dash->groups, i);
		gf_dash_seek_group(dash, group);
	}
	gf_mx_v(dash->dl_mutex);
}


static GF_Err http_ifce_get(GF_FileDownload *getter, char *url)
{
	GF_Err e;
	GF_DashClient *dash = (GF_DashClient*) getter->udta;
	GF_DASHFileIOSession *sess = dash->dash_io->create(dash->dash_io, 0, url, -1);
	if (!sess) return GF_IO_ERR;
	getter->session = sess;
	e = dash->dash_io->init(dash->dash_io, sess);
	if (e) return e;
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
	GF_Err e;
	GF_MPD_Period *period;
	GF_DOMParser *mpd_parser;
	Bool is_local = 0;

	if (!dash || !manifest_url) return GF_BAD_PARAM;

	memset( dash->lastMPDSignature, 0, sizeof(dash->last_update_time));
	dash->reload_count = 0;

	if (dash->base_url) gf_free(dash->base_url);
	dash->base_url = gf_strdup(manifest_url);

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
			dash->is_m3u8 = 1;
		}
	} else if (strstr(manifest_url, "://")) {
		const char *reloc_url, *mtype;
		char mime[128];
		e = gf_dash_download_resource(dash->dash_io, &(dash->mpd_dnload), manifest_url, 0, 0, 1, NULL);
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
		FILE *f = fopen(local_url, "rt");
		if (!f) return GF_URL_ERROR;
		fclose(f);
	}
	dash->mpd_fetch_time = gf_net_get_utc();

	if (dash->is_m3u8) {
		if (is_local) {
			char *sep;
			strcpy(local_path, local_url);
			sep = strrchr(local_path, '.');
			if (sep) sep[0]=0;
			strcat(local_path, ".mpd");

			gf_m3u8_to_mpd(local_url, manifest_url, local_path, dash->reload_count, dash->mimeTypeForM3U8Segments, 0, M3U8_TO_MPD_USE_TEMPLATE, &dash->getter);
			local_url = local_path;
		} else {
			gf_m3u8_to_mpd(local_url, manifest_url, NULL, dash->reload_count, dash->mimeTypeForM3U8Segments, 0, M3U8_TO_MPD_USE_TEMPLATE, &dash->getter);
		}
	}

	if (!gf_dash_check_mpd_root_type(local_url)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot connect service: wrong file type %s\n", local_url));
		dash->dash_io->del(dash->dash_io, dash->mpd_dnload);
		dash->mpd_dnload = NULL;
		return GF_URL_ERROR;
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] parsing MPD %s\n", local_url));

	/* parse the MPD */
	mpd_parser = gf_xml_dom_new();
	e = gf_xml_dom_parse(mpd_parser, local_url, NULL, NULL);
	if (e != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot connect service: MPD parsing problem %s\n", gf_xml_dom_get_error(mpd_parser) ));
		gf_xml_dom_del(mpd_parser);
		dash->dash_io->del(dash->dash_io, dash->mpd_dnload);
		dash->mpd_dnload = NULL;
		return GF_URL_ERROR;
	}
	if (dash->mpd)
		gf_mpd_del(dash->mpd);

	dash->mpd = gf_mpd_new();
	if (!dash->mpd) {
		e = GF_OUT_OF_MEM;
	} else {
		e = gf_mpd_init_from_dom(gf_xml_dom_get_root(mpd_parser), dash->mpd, manifest_url);
	}
	gf_xml_dom_del(mpd_parser);

	if (e != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot connect service: MPD creation problem %s\n", gf_error_to_string(e)));
		goto exit;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] DASH client initialized from MPD\n"));
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
	assert( dash );

	gf_dash_download_stop(dash);

	gf_mx_p(dash->dl_mutex);
	if (dash->mpd_dnload) {
		dash->dash_io->del(dash->dash_io, dash->mpd_dnload);
		dash->mpd_dnload = NULL;
	}
	if (dash->mpd)
		gf_mpd_del(dash->mpd);
	dash->mpd = NULL;

	gf_mx_v(dash->dl_mutex);

	if (dash->dash_state != GF_DASH_STATE_CONNECTING)
		gf_dash_reset_groups(dash);
}

GF_EXPORT
GF_DashClient *gf_dash_new(GF_DASHFileIO *dash_io, u32 max_cache_duration, u32 auto_switch_count, Bool keep_files, Bool disable_switching, GF_DASHInitialSelectionMode first_select_mode, Bool enable_buffering, u32 initial_time_shift_percent)
{
	GF_DashClient *dash;
	GF_SAFEALLOC(dash, GF_DashClient);
	dash->dash_io = dash_io;

	dash->dash_thread = gf_th_new("MPD Segment Downloader Thread");
	dash->dl_mutex = gf_mx_new("MPD Segment Downloader Mutex");
	dash->mimeTypeForM3U8Segments = gf_strdup( "video/mp2t" );

	dash->max_cache_duration = max_cache_duration;
	dash->enable_buffering = enable_buffering;
	dash->initial_time_shift_percent = initial_time_shift_percent;
	if (initial_time_shift_percent>100) dash->initial_time_shift_percent = 100;

	dash->auto_switch_count = auto_switch_count;
	dash->keep_files = keep_files;
	dash->disable_switching = disable_switching;
	dash->first_select_mode = first_select_mode;	
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Client created\n"));
	return dash;
}

GF_EXPORT
void gf_dash_del(GF_DashClient *dash)
{
	gf_dash_close(dash);

	gf_th_del(dash->dash_thread);
	gf_mx_del(dash->dl_mutex);	

	if (dash->mimeTypeForM3U8Segments) gf_free(dash->mimeTypeForM3U8Segments);
	if (dash->base_url) gf_free(dash->base_url);

	gf_free(dash);
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
			gf_mx_p(dash->dl_mutex);
			group->force_switch_bandwidth = 1;
			group->force_representation_idx_plus_one = switch_to_rep_idx;

			if (group->local_files || immediate_switch) {
				/*in local playback just switch at the end of the current segment
				for remote, we should let the user decide*/
				while (group->nb_cached_segments>1) {
					group->nb_cached_segments--;
					gf_dash_update_buffering(group, dash);

					gf_free(group->cached[group->nb_cached_segments].url);
					group->cached[group->nb_cached_segments].url = NULL;
					if (!group->local_files && group->cached[group->nb_cached_segments].cache) {
						gf_delete_file( group->cached[group->nb_cached_segments].cache );
						gf_free(group->cached[group->nb_cached_segments].cache);
						group->cached[group->nb_cached_segments].cache = NULL;
					}
					group->cached[group->nb_cached_segments].representation_index = 0;
					group->cached[group->nb_cached_segments].start_range = 0;
					group->cached[group->nb_cached_segments].end_range = 0;
					group->cached[group->nb_cached_segments].duration = (u32) group->current_downloaded_segment_duration;
					if (group->download_segment_index>1)
						group->download_segment_index--;
				}
			}
			gf_mx_v(dash->dl_mutex);
		}
	}
}

GF_EXPORT
Double gf_dash_get_duration(GF_DashClient *dash)
{
	Double duration;
	duration = (Double)dash->mpd->media_presentation_duration;
	if (!duration) {
		u32 i;
		for (i=0; i<gf_list_count(dash->mpd->periods); i++) {
			GF_MPD_Period *period = gf_list_get(dash->mpd->periods, i);
			duration += (Double)period->duration;
		}
	}
	duration /= 1000;
	return duration;
}

GF_EXPORT
const char *gf_dash_get_url(GF_DashClient *dash)
{
	return dash->base_url;
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
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return NULL;

	if (start_range) *start_range = group->local_url_start_range;
	if (end_range) *end_range = group->local_url_end_range;		
	return group->segment_local_url;
}

GF_EXPORT
void gf_dash_group_select(GF_DashClient *dash, u32 idx, Bool select)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group) return;
	if (group->selection == GF_DASH_GROUP_NOT_SELECTABLE)
		return;

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
}

GF_EXPORT
void gf_dash_groups_set_language(GF_DashClient *dash, const char *lang_3cc)
{
	u32 i;
	if (!lang_3cc) return;
	for (i=0; i<gf_list_count(dash->groups); i++) {
		GF_DASH_Group *group = gf_list_get(dash->groups, i);
		if (group->selection==GF_DASH_GROUP_NOT_SELECTABLE) continue;
		if (group->adaptation_set->lang && !stricmp(group->adaptation_set->lang, lang_3cc)) {
			gf_dash_group_select(dash, i, 1);
		}
	}
}

GF_EXPORT
Bool gf_dash_is_running(GF_DashClient *dash) 
{
	return dash->dash_state;
}

GF_EXPORT
u32 gf_dash_get_period_switch_status(GF_DashClient *dash) 
{
	return dash->request_period_switch;
}
GF_EXPORT
void gf_dash_request_period_switch(GF_DashClient *dash)
{
	dash->request_period_switch = 1;
}
GF_EXPORT
Bool gf_dash_in_last_period(GF_DashClient *dash) 
{
	return (dash->active_period_index+1 < gf_list_count(dash->mpd->periods)) ? 0 : 1;
}
GF_EXPORT
Bool gf_dash_in_period_setup(GF_DashClient *dash) 
{
	return dash->in_period_setup;
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

	gf_mx_p(dash->dl_mutex);
	group = gf_list_get(dash->groups, idx);

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
	gf_mx_v(dash->dl_mutex);
	return res;
}

GF_EXPORT
void gf_dash_group_discard_segment(GF_DashClient *dash, u32 idx)
{
	GF_DASH_Group *group;

	gf_mx_p(dash->dl_mutex);
	group = gf_list_get(dash->groups, idx);

	if (!group->nb_cached_segments) {
		gf_mx_v(dash->dl_mutex);	
		return;
	}
	if (group->cached[0].cache) {
		if (group->urlToDeleteNext) {
			if (!group->local_files && !dash->keep_files)
				dash->dash_io->delete_cache_file(dash->dash_io, group->segment_download, group->urlToDeleteNext);

			gf_free( group->urlToDeleteNext);
			group->urlToDeleteNext = NULL;
		}
		assert( group->cached[0].url );

		if (group->dont_delete_first_segment) {
			group->dont_delete_first_segment = 0;
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MPD_IN] deleting cache file %s : %s\n", group->cached[0].url, group->cached[0].cache));
			group->urlToDeleteNext = gf_strdup( group->cached[0].url );
		}

		gf_free(group->cached[0].cache);
		gf_free(group->cached[0].url);
		group->cached[0].url = NULL;
		group->cached[0].cache = NULL;
		group->cached[0].representation_index = 0;
		group->cached[0].duration = 0;
	}
	memmove(&group->cached[0], &group->cached[1], sizeof(segment_cache_entry)*(group->nb_cached_segments-1));
	memset(&(group->cached[group->nb_cached_segments-1]), 0, sizeof(segment_cache_entry));
	group->nb_cached_segments--;

	gf_mx_v(dash->dl_mutex);
}

GF_EXPORT
void gf_dash_set_group_done(GF_DashClient *dash, u32 idx, Bool done)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (group) {
		group->done = done;
		if (done && group->segment_download) dash->dash_io->abort(dash->dash_io, group->segment_download);
	}
}

GF_EXPORT
GF_Err gf_dash_group_get_presentation_time_offset(GF_DashClient *dash, u32 idx, u64 *presentation_time_offset, u32 *timescale)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (group) {
		u64 duration;
		GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);
		gf_dash_resolve_duration(rep, group->adaptation_set, group->period, &duration, timescale, presentation_time_offset, NULL);
		return GF_OK;
	}
	return GF_BAD_PARAM;
}

GF_EXPORT
GF_Err gf_dash_group_get_next_segment_location(GF_DashClient *dash, u32 idx, const char **url, u64 *start_range, u64 *end_range, s32 *switching_index, const char **switching_url, u64 *switching_start_range, u64 *switching_end_range, const char **original_url)
{
	GF_DASH_Group *group;

	*url = NULL;
	if (switching_url) *switching_url = NULL;
	if (start_range) *start_range = 0;
	if (end_range) *end_range = 0;
	if (switching_start_range) *switching_start_range = 0;
	if (switching_end_range) *switching_end_range = 0;
	if (original_url) *original_url = NULL;
	if (switching_index) *switching_index = -1;

	gf_mx_p(dash->dl_mutex);	
	group = gf_list_get(dash->groups, idx);
	if (!group->nb_cached_segments) {
		gf_mx_v(dash->dl_mutex);	
		return GF_BAD_PARAM;
	}
	*url = group->cached[0].cache;
	if (start_range) 
		*start_range = group->cached[0].start_range;
	if (end_range)
		*end_range = group->cached[0].end_range;
	if (original_url) *original_url = group->cached[0].url;

	if (group->cached[0].representation_index != group->prev_active_rep_index) {
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
	group->prev_active_rep_index = group->cached[0].representation_index;
	gf_mx_v(dash->dl_mutex);	
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

	gf_mx_p(dash->dl_mutex);	
	group = gf_list_get(dash->groups, idx);
	if (!group) {
		gf_mx_v(dash->dl_mutex);	
		return GF_BAD_PARAM;
	}

	if (!group->is_downloading) {
		gf_mx_v(dash->dl_mutex);
		return GF_OK;
	}

	*switched = GF_FALSE;
	if (group->download_aborted) {
		group->download_aborted = 0;
		*switched = GF_TRUE;
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
	gf_mx_v(dash->dl_mutex);	
	return GF_OK;
}

GF_EXPORT
void gf_dash_seek(GF_DashClient *dash, Double start_range)
{
	gf_mx_p(dash->dl_mutex);	
	
	dash->playback_start_range = start_range;
	/*first check if we seek to another period*/
	if (! gf_dash_seek_periods(dash)) {
		/*if no, seek in group*/
		gf_dash_seek_groups(dash);
	}
	gf_mx_v(dash->dl_mutex);	
}

GF_EXPORT
Double gf_dash_get_playback_start_range(GF_DashClient *dash)
{
	return dash->playback_start_range;
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
GF_Err gf_dash_group_get_video_info(GF_DashClient *dash, u32 idx, u32 *max_width, u32 *max_height)
{
	GF_DASH_Group *group = gf_list_get(dash->groups, idx);
	if (!group || !max_width || !max_height) return GF_BAD_PARAM;

	*max_width = group->adaptation_set->max_width;
	*max_height = group->adaptation_set->max_height;
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
	if (height) *width = rep->height ? rep->height : group->adaptation_set->height;
	if (codecs) *codecs = rep->codecs ? rep->codecs : group->adaptation_set->codecs;
	if (bandwidth) *bandwidth = rep->bandwidth;
	if (audio_samplerate) *audio_samplerate = rep->samplerate ? rep->samplerate : group->adaptation_set->samplerate;

	return GF_OK;
}


#endif //GPAC_DISABLE_DASH_CLIENT

