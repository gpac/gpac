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
#include <string.h>

#ifndef GPAC_DISABLE_DASH_CLIENT

/*set to 1 if you want MPD to use SegmentTemplate if possible instead of SegmentList*/
#define M3U8_TO_MPD_USE_TEMPLATE	0


typedef enum {
	GF_DASH_STATE_STOPPED = 0,
	GF_DASH_STATE_RUNNING,
	GF_DASH_STATE_CONNECTING,
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
	Bool keep_files, disable_switching;
	/*0: min bandwith 1: max bandwith 2: min quality 3: max quality*/
	u32 first_select_mode;

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

	/*list of groups in the active period*/
	GF_List *groups;

	/*Main Thread handling segment downloads and MPD/M3U8 update*/
	GF_Thread *dash_thread;
	/*mutex for group->cache file name access and MPD update*/
	GF_Mutex *dl_mutex;

	/* 0 not started, 1 download in progress */
	GF_DASH_STATE mpd_is_running;
	Bool mpd_stop_request;
	Bool in_period_setup;


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

	GF_DASHGroupSelection selection;


	Bool bitstream_switching, dont_delete_first_segment;

	Bool done;
	Bool force_switch_bandwidth, min_bandwidth_selected;
	u32 nb_bw_check;
	u32 active_bitrate, max_bitrate, min_bitrate;

	u32 nb_segments_in_rep;
	Double segment_duration;

	/*local file playback, do not delete them*/
	Bool local_files;
	/*next segment to download for this group*/
	u32 download_segment_index;
	/*number of segments pruged since the start of the period*/
	u32 nb_segments_purged;

	/*next file (cached) to delete at next GF_NET_SERVICE_QUERY_NEXT for this group*/
	char * urlToDeleteNext;
	volatile u32 max_cached_segments, nb_cached_segments;
	segment_cache_entry *cached;

	GF_DASHFileIOSession segment_download;

	const char *segment_local_url;
	/*usually 0-0 (no range) but can be non-zero when playing local MPD/DASH sessions*/
	u64 local_url_start_range, local_url_end_range;

	u32 nb_segments_done;

	Bool segment_must_be_streamed;

	u32 force_representation_idx_plus_one;

	Bool force_segment_switch;

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

void gf_dash_group_check_switch(GF_DASHFileIO *dash_io, GF_DASH_Group *group, Bool download_active)
{
	u32 download_rate;

	if (group->dash->disable_switching) return;
	download_rate = dash_io->get_bytes_per_sec(dash_io, group->segment_download);
	if (!download_rate) return;

	download_rate *= 8;
	if (download_rate<group->min_bitrate) group->min_bitrate = download_rate;
	if (download_rate>group->max_bitrate) group->max_bitrate = download_rate;

	if (download_rate && (download_rate < group->active_bitrate)) {
		u32 set_idx = gf_list_find(group->period->adaptation_sets, group->adaptation_set)+1;
		group->nb_bw_check ++;
		if (group->min_bandwidth_selected) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Downloading from set #%d at rate %d kbps but media bitrate is %d kbps - no lower bitrate available ...\n", set_idx, download_rate/1024, group->active_bitrate/1024 ));
		} else if (group->nb_bw_check>2) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Downloading from set #%d at rate %d kbps but media bitrate is %d kbps - switching\n", set_idx, download_rate/1024, group->active_bitrate/1024 ));
			group->force_switch_bandwidth = 1;
			if (download_active) 
				if (group->segment_download) dash_io->abort(dash_io, group->segment_download);
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Downloading from set #%ds at rate %d kbps but media bitrate is %d kbps\n", set_idx, download_rate/1024, group->active_bitrate/1024 ));
		}
	} else {
		group->nb_bw_check = 0;
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
static Bool gf_dash_is_m3u8_mime(const char * mime) {
	u32 i;
	if (!mime)
		return 0;
	for (i = 0 ; GF_DASH_M3U8_MIME_TYPES[i] ; i++) {
		if ( !stricmp(mime, GF_DASH_M3U8_MIME_TYPES[i]))
			return 1;
	}
	return 0;
}


/*!
* Download a file with possible retry if GF_IP_CONNECTION_FAILURE|GF_IP_NETWORK_FAILURE
* (I discovered that with my WIFI connection, I had many issues with BFM-TV downloads)
* Similar to gf_term_download_new() and gf_dm_sess_process().
* Parameters are identical to the ones of gf_term_download_new.
* \see gf_term_download_new()
*/
GF_Err gf_dash_download_resource(GF_DASHFileIO *dash_io, GF_DASHFileIOSession *sess, const char *url, u64 start_range, u64 end_range, Bool persistent, GF_DASH_Group *group)
{
	Bool had_sess = 0;
	Bool retry = 1;
	GF_Err e;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Downloading %s...\n", url));

	if (! *sess) {
		*sess = dash_io->create(dash_io, persistent, url);
		if (!(*sess)){
			assert(0);
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot try to download %s... OUT of memory ?\n", url));
			return GF_OUT_OF_MEM;
		}
	} else {
		had_sess = 1;
		e = dash_io->setup_from_url(dash_io, *sess, url);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot resetup session for url %s: %s\n", url, gf_error_to_string(e) ));
			return e;
		}
	}

retry:

	if (end_range) {
		e = dash_io->set_range(dash_io, *sess, start_range, end_range);
		if (e) {
			if (had_sess) {
				dash_io->del(dash_io, *sess);
				*sess = NULL;
				return gf_dash_download_resource(dash_io, sess, url, start_range, end_range, persistent, group);
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
		*sess = dash_io->create(dash_io, 0, url);
		if (! (*sess)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot retry to download %s... OUT of memory ?\n", url));
			return GF_OUT_OF_MEM;
		}

		if (retry) {
			retry = 0;
			goto retry;
		}
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] two consecutive failures, aborting the download %s.\n", url));
		return e;
	}
case GF_OK:
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Download %s complete\n", url));
	break;
default:
	GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] FAILED to download %s = %s...\n", url, gf_error_to_string(e)));
	return e;
	}
	return GF_OK;
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
	u32 timescale;
	u64 duration;
	GF_MPD_SegmentTimeline *timeline = NULL;
	*nb_segments = timescale = 0;
	duration = 0;

	/*single segment*/
	if (rep->segment_base || set->segment_base || period->segment_base) {
		return;
	}
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

	if (period->segment_template) {
		if (period->segment_template->duration) duration = period->segment_template->duration;
		if (period->segment_template->timescale) timescale = period->segment_template->timescale;
		if (period->segment_template->segment_timeline) timeline = period->segment_template->segment_timeline;
	}
	if (set->segment_template) {
		if (set->segment_template->duration) duration = set->segment_template->duration;
		if (set->segment_template->timescale) timescale = set->segment_template->timescale;
		if (set->segment_template->segment_timeline) timeline = set->segment_template->segment_timeline;
	}
	if (rep->segment_template) {
		if (rep->segment_template->duration) duration = rep->segment_template->duration;
		if (rep->segment_template->timescale) timescale = rep->segment_template->timescale;
		if (rep->segment_template->segment_timeline) timeline = rep->segment_template->segment_timeline;
	}
	if (!timescale) timescale=1;

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

static u64 gf_dash_segment_timeline_start(GF_MPD_SegmentTimeline *timeline, u32 segment_index)
{
	u64 start_time = 0;
	u32 i, idx, k;
	idx = 0;
	for (i=0; i<gf_list_count(timeline->entries); i++) {
		GF_MPD_SegmentTimelineEntry *ent = gf_list_get(timeline->entries, i);
		if (ent->start_time) start_time = ent->start_time;		
		for (k=0; k<ent->repeat_count+1; k++) {
			if (idx==segment_index) 
				return start_time;
			idx ++;
			start_time += ent->duration;
		}
	}
	return start_time;
}

static Double gf_dash_get_segment_start_time(GF_DASH_Group *group)
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
	segment_index = group->download_segment_index - group->nb_cached_segments;

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
			start_time = (Double) gf_dash_segment_timeline_start(timeline, segment_index);
		} else {
			start_time = segment_index * (Double) duration;
		}
		start_time /= timescale;
		return start_time;
	}

	if (period->segment_template) {
		if (period->segment_template->duration) duration = period->segment_template->duration;
		if (period->segment_template->timescale) timescale = period->segment_template->timescale;
		if (period->segment_template->segment_timeline) timeline = period->segment_template->segment_timeline;
	}
	if (set->segment_template) {
		if (set->segment_template->duration) duration = set->segment_template->duration;
		if (set->segment_template->timescale) timescale = set->segment_template->timescale;
		if (set->segment_template->segment_timeline) timeline = set->segment_template->segment_timeline;
	}
	if (rep->segment_template) {
		if (rep->segment_template->duration) duration = rep->segment_template->duration;
		if (rep->segment_template->timescale) timescale = rep->segment_template->timescale;
		if (rep->segment_template->segment_timeline) timeline = rep->segment_template->segment_timeline;
	}
	if (!timescale) timescale=1;

	if (timeline) {
		start_time = (Double) gf_dash_segment_timeline_start(timeline, segment_index);
	} else {
		start_time = segment_index * (Double) duration;
	}
	start_time /= timescale;
	return start_time;
}

static void gf_dash_resolve_duration(GF_MPD_Representation *rep, GF_MPD_AdaptationSet *set, GF_MPD_Period *period, u64 *out_duration, u32 *out_timescale, GF_MPD_SegmentTimeline **out_segment_timeline)
{
	u32 timescale = 0;
	GF_MPD_SegmentTimeline *segment_timeline;
	GF_MPD_MultipleSegmentBase *mbase_rep, *mbase_set, *mbase_period;

	if (out_segment_timeline) *out_segment_timeline = NULL;

	/*single media segment - duration is not known unless indicated in period*/
	if (rep->segment_base || set->segment_base || period->segment_base) {
		*out_duration = period ? period->duration : 0;
		*out_timescale = 1000;
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

	if (mbase_rep && mbase_rep->duration) *out_duration = mbase_rep->duration;
	else if (mbase_set && mbase_set->duration) *out_duration = mbase_set->duration;
	else if (mbase_period && mbase_period->duration) *out_duration = mbase_period->duration;

	if (out_segment_timeline) *out_segment_timeline = segment_timeline;

	/*for SegmentTimeline, just pick the first one as an indication (exact timeline solving is not done here)*/
	if (segment_timeline) {
		GF_MPD_SegmentTimelineEntry *ent = gf_list_get(segment_timeline->entries, 0);
		if (ent) *out_duration = ent->duration;
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
	gf_dash_resolve_duration(rep, group->adaptation_set, group->period, &duration, &time_scale, &timeline);
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
	GF_DOMParser *mpd_parser;
	GF_MPD_Period *period, *new_period;
	const char *local_url;
	char mime[128];
	char * purl;

	if (!dash->mpd_dnload) {
		if (!gf_list_count(dash->mpd->locations)) {
			/*we will no longer attempt to update the MPD ...*/
			dash->mpd->minimum_update_period = 0;
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update playlist: no HTTP source for MPD could be found\n"));
			return GF_BAD_PARAM;
		}
		purl = gf_strdup(gf_list_get(dash->mpd->locations, 0));

		/*if no absolute URL, use <Location> to get MPD in case baseURL is relative...*/
		if (!strstr(dash->base_url, "://")) {
			gf_free(dash->base_url);
			dash->base_url = gf_strdup(purl);
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

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Updating Playlist %s...\n", purl));
	/*use non-persistent connection for MPD*/
	e = gf_dash_download_resource(dash->dash_io, &(dash->mpd_dnload), purl, 0, 0, 0, NULL);
	if (e!=GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update playlist: download problem %s for MPD file\n", gf_error_to_string(e)));
		gf_free(purl);
		return e;
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Playlist %s updated with success\n", purl));
	}
	strcpy(mime, dash->dash_io->get_mime(dash->dash_io, dash->mpd_dnload) );
	strlwr(mime);

	/*in case the session has been restarted, local_url may have been destroyed - get it back*/
	local_url = dash->dash_io->get_cache_name(dash->dash_io, dash->mpd_dnload);

	/* Some servers, for instance http://tv.freebox.fr, serve m3u8 as text/plain */
	if (gf_dash_is_m3u8_mime(mime) || strstr(purl, ".m3u8")) {
		gf_m3u8_to_mpd(local_url, purl, NULL, dash->reload_count, dash->mimeTypeForM3U8Segments, 0, M3U8_TO_MPD_USE_TEMPLATE, &dash->getter);
	} else if (!gf_dash_is_dash_mime(mime)) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] mime '%s' should be m3u8 or mpd\n", mime));
	}

	gf_free(purl);
	purl = NULL;

	if (!gf_dash_check_mpd_root_type(local_url)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error - cannot update playlist: MPD file type is not correct %s\n", local_url));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	{
		u8 signature[sizeof(dash->lastMPDSignature)];
		if (gf_sha1_file( local_url, signature)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] : cannot SHA1 file %s\n", local_url));
		} else {
			if (! memcmp( signature, dash->lastMPDSignature, sizeof(dash->lastMPDSignature))) {
				dash->reload_count++;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] MPD file did not change for %d consecutive reloads\n", dash->reload_count));
				/*if the MPD did not change, we should refresh "soon" but cannot wait a full refresh cycle in case of 
				low latencies as we could miss a segment*/
				dash->last_update_time += dash->mpd->minimum_update_period/2;
			} else {
				Double timeline_start_time;
				GF_MPD *new_mpd;
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
						Double group_start = gf_dash_get_segment_start_time(group);
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
					GF_MPD_AdaptationSet *set, *new_set;
					GF_DASH_Group *group = gf_list_get(dash->groups, group_idx);
					if (group->selection != GF_DASH_GROUP_SELECTED) continue;
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

					/*update number of segments in active rep*/
					gf_dash_get_segment_duration(gf_list_get(group->adaptation_set->representations, group->active_rep_index), group->adaptation_set, group->period, new_mpd, &group->nb_segments_in_rep, NULL);

					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Updated AdaptationSet %d - %d segments\n", group_idx+1, group->nb_segments_in_rep));

				}
				/*swap representations - we don't need to update download_segment_index as it still points to the right entry in the merged list*/
				if (dash->mpd)
					gf_mpd_del(dash->mpd);
				dash->mpd = new_mpd;
				dash->last_update_time = gf_sys_clock();
			}
		}
	}
	return GF_OK;
}


static void gf_dash_set_group_representation(GF_DASH_Group *group, GF_MPD_Representation *rep)
{
#ifndef GPAC_DISABLE_LOG
	u32 width=0, height=0, samplerate=0;
	GF_MPD_Fractional *framerate=NULL;
#endif
	u32 k;
	GF_MPD_AdaptationSet *set;
	GF_MPD_Period *period;
	u32 i = gf_list_find(group->adaptation_set->representations, rep);
	assert((s32) i >= 0);

	group->active_rep_index = i;
	group->active_bitrate = rep->bandwidth;
	group->nb_segments_in_rep = 1;

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
		if (framerate) GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("framerate %d/%d", framerate->num, framerate->den));
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("\n"));
	} else if (samplerate) {
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("\tsamplerate %d\n", samplerate));
	}
#endif


	gf_dash_get_segment_duration(rep, set, period, group->dash->mpd, &group->nb_segments_in_rep, &group->segment_duration);
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


GF_Err gf_dash_resolve_url(GF_MPD *mpd, GF_MPD_Representation *rep, GF_DASH_Group *group, char *mpd_url, GF_DASHURLResolveType resolve_type, u32 item_index, char **out_url, u64 *out_range_start, u64 *out_range_end, u64 *segment_duration)
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

	gf_dash_resolve_duration(rep, set, period, segment_duration, &timescale, NULL);
	*segment_duration = (u32) ((Double) (*segment_duration) * 1000.0 / timescale);

	/*single URL*/
	if (rep->segment_base || set->segment_base || period->segment_base) {
		GF_MPD_URL *res_url;
		if (item_index>0) return GF_EOS;
		switch (resolve_type) {
		case GF_DASH_RESOLVE_URL_MEDIA:
			if (!url) return GF_NON_COMPLIANT_BITSTREAM;
			*out_url = url;
			return GF_OK;
		case GF_DASH_RESOLVE_URL_INIT:
		case GF_DASH_RESOLVE_URL_INDEX:
			res_url = NULL;
			if (resolve_type == GF_DASH_RESOLVE_URL_INDEX) {
				if (period->segment_base) res_url = period->segment_base->representation_index;
				if (set->segment_base) res_url = set->segment_base->representation_index;
				if (rep->segment_base) res_url = rep->segment_base->representation_index;
			} else {
				if (period->segment_base) res_url = period->segment_base->initialization_segment;
				if (set->segment_base) res_url = set->segment_base->initialization_segment;
				if (rep->segment_base) res_url = rep->segment_base->initialization_segment;
			}
			/*no initialization segment / index*/
			if (!res_url) {
				gf_free(url);
				return GF_OK;
			}
			if (res_url->sourceURL) {
				*out_url = gf_url_concatenate(url, res_url->sourceURL);
				gf_free(url);
			} else {
				*out_url = url;
			}
			if (res_url->byte_range) {
				*out_range_start = res_url->byte_range->start_range;
				*out_range_end = res_url->byte_range->end_range;
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
		if (format_tag) format_tag[0] = 0;
		/* identifier is $$ -> replace by $*/
		if (!strlen(first_sep+1)) {
			strcat(solved_template, "$");
		}
		else if (!strcmp(first_sep+1, "RepresentationID")) {
			strcat(solved_template, rep->id);
		}
		else if (!strcmp(first_sep+1, "Number")) {
			if (format_tag) {
				char szPrintFormat[20];
				strcpy(szPrintFormat, "%");
				strcat(szPrintFormat, format_tag+1);
				strcat(szPrintFormat, "d");
				sprintf(szFormat, szPrintFormat, start_number + item_index);
			} else {
				sprintf(szFormat, "%d", start_number + item_index);
			}
			strcat(solved_template, szFormat);
		}
		else if (!strcmp(first_sep+1, "Bandwidth")) {
			if (format_tag) {
				char szPrintFormat[20];
				strcpy(szPrintFormat, "%");
				strcat(szPrintFormat, format_tag+1);
				strcat(szPrintFormat, "d");
				sprintf(szFormat, format_tag+1, rep->bandwidth);
			} else {
				sprintf(szFormat, "%d", rep->bandwidth);
			}
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
					sprintf(szFormat, ""LLD"", time);
					strcat(solved_template, szFormat);
					break;
				}
			}
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
	/*To check with the group, the commented version should just be a hack for bad sources*/
	//	*out_url = gf_url_base_concatenate(url, solved_template);
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

	e = gf_dash_resolve_url(dash->mpd, rep, group, dash->base_url, GF_DASH_RESOLVE_URL_INIT, 0, &base_init_url, &start_range, &end_range, &group->current_downloaded_segment_duration);
	if (e) {
		gf_mx_v(dash->dl_mutex);
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Unable to resolve initialization URL: %s\n", gf_error_to_string(e) ));
		return e;
	}

	/*no error and no init segment, go for media segment*/
	if (!base_init_url) {
		e = gf_dash_resolve_url(dash->mpd, rep, group, dash->base_url, GF_DASH_RESOLVE_URL_MEDIA, group->download_segment_index, &base_init_url, &start_range, &end_range, &group->current_downloaded_segment_duration);
		if (e) {
			gf_mx_v(dash->dl_mutex);
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Unable to resolve media URL: %s\n", gf_error_to_string(e) ));
			return e;
		}
		nb_segment_read = 1;
	} else if (!group->bitstream_switching) {
		group->dont_delete_first_segment = 1;
	}

	if (!strstr(base_init_url, "://") || !strnicmp(base_init_url, "file://", 7)) {
		assert(!group->nb_cached_segments);
		group->cached[0].cache = gf_strdup(base_init_url);
		group->cached[0].url = gf_strdup(base_init_url);
		group->cached[0].representation_index = group->active_rep_index;
		group->prev_active_rep_index = group->active_rep_index;

		group->nb_cached_segments = 1;
		/*do not erase local files*/
		group->local_files = 1;
		group->download_segment_index += nb_segment_read;
		group->segment_local_url = group->cached[0].cache;
		group->local_url_start_range = start_range;
		group->local_url_end_range = end_range;
		rep->playback.cached_init_segment_url = gf_strdup(group->segment_local_url);
		rep->playback.init_start_range = start_range;
		rep->playback.init_end_range = end_range;

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

		e = gf_dash_resolve_url(dash->mpd, rep, group, dash->base_url, GF_DASH_RESOLVE_URL_MEDIA, group->download_segment_index + 1, &base_init_url, &start_range, &end_range, &group->current_downloaded_segment_duration);
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
		u32 count = group->nb_segments_in_rep + 1;
		if (count < group->max_cached_segments) {
			if (count < 1) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] 0 representations, aborting\n"));
				gf_free(base_init_url);
				gf_mx_v(dash->dl_mutex);
				return GF_BAD_PARAM;
			}
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Resizing to %u max_cached_segments elements instead of %u.\n", count, group->max_cached_segments));
			/* OK, we have a problem, it may ends download */
			group->max_cached_segments = count;
		}

		/* Mime-Type check */
		mime_type = dash->dash_io->get_mime(dash->dash_io, group->segment_download) ;
		strcpy(mime, mime_type ? mime_type : "");
		strlwr(mime);

		if (dash->mimeTypeForM3U8Segments) 
			gf_free(dash->mimeTypeForM3U8Segments);
		dash->mimeTypeForM3U8Segments = gf_strdup( mime );
		if (!rep->mime_type) {
			rep->mime_type = gf_strdup( mime );
		}
		mime_type = gf_dash_get_mime_type(NULL, rep, group->adaptation_set);
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
		rep->playback.cached_init_segment_url = gf_strdup(group->segment_local_url);
		rep->playback.init_start_range = 0;
		rep->playback.init_end_range = 0;

		group->nb_cached_segments = 1;
		group->download_segment_index += nb_segment_read;
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

				e = gf_dash_resolve_url(dash->mpd, a_rep, group, dash->base_url, GF_DASH_RESOLVE_URL_INIT, 0, &a_base_init_url, &a_start, &a_end, &a_dur);
				if (!e && a_base_init_url) {
					e = gf_dash_download_resource(dash->dash_io, &(group->segment_download), a_base_init_url, a_start, a_end, 1, group);
					if (e) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Cannot retrieve initialization segment %s for representation: %s - discarding representation\n", a_base_init_url, gf_error_to_string(e) ));
						a_rep->playback.disabled = 1;
					} else {
						a_rep->playback.cached_init_segment_url = gf_strdup( dash->dash_io->get_cache_name(dash->dash_io, group->segment_download) );
						rep->playback.init_start_range = 0;
						rep->playback.init_end_range = 0;
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


/* create groups (implemntation of adaptations set) */
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
		Bool found = 0;
		GF_MPD_AdaptationSet *set = gf_list_get(period->adaptation_sets, i);
		for (j=0; j<gf_list_count(dash->groups); j++) {
			GF_DASH_Group *group = gf_list_get(dash->groups, j);
			if (group->adaptation_set==set) {
				found = 1;
				break;
			}
		}
		if (!found) {
			u32 cache_duration;
			Double seg_dur;
			GF_DASH_Group *group;
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
				gf_dash_get_segment_duration(gf_list_get(set->representations, j), set, period, dash->mpd, &nb_seg, &dur);
				if (dur>seg_dur) seg_dur = dur;
			}

			cache_duration = dash->max_cache_duration * 1000;
			if (dash->mpd->min_buffer_time)
				cache_duration = dash->mpd->min_buffer_time;
			group->max_cached_segments = 1;
			if (seg_dur) {
				while (group->max_cached_segments * seg_dur * 1000 < cache_duration)
					group->max_cached_segments ++;

				/*we need one more entry in cache for segment being currently played*/
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
	}
	return GF_OK;
}

static GF_Err gf_dash_setup_period(GF_DashClient *dash)
{
	u32 rep_i, group_i, nb_groups_ok;

	/*setup all groups*/
	gf_dash_setup_groups(dash);

	nb_groups_ok = 0;
	for (group_i=0; group_i<gf_list_count(dash->groups); group_i++) {
		GF_MPD_Representation *rep_sel;
		u32 active_rep, nb_rep;
		const char *mime_type;
		GF_DASH_Group *group = gf_list_get(dash->groups, group_i);

		if (!group->adaptation_set->segment_alignment) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] AdaptationSet without segmentAlignment flag set - ignoring because not supported\n"));
			continue;
		}
		if (group->adaptation_set->xlink_href) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] AdaptationSet with xlink:href to %s - ignoring because not supported\n", group->adaptation_set->xlink_href));
			continue;
		}

		nb_rep = gf_list_count(group->adaptation_set->representations);
		/* Select the appropriate representation in the given period */
		active_rep = 0;
		for (rep_i = 0; rep_i < nb_rep; rep_i++) {
			GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, rep_i);
			rep_sel = gf_list_get(group->adaptation_set->representations, active_rep);

			if (rep_i) {
				Bool ok;
				char *sep;
				if ( !rep->codecs || !rep_sel->codecs ) continue;
				sep = strchr(rep_sel->codecs, '.');
				if (sep) sep[0] = 0;
				ok = !strnicmp(rep->codecs, rep_sel->codecs, strlen(rep_sel->codecs) );
				if (sep) sep[0] = '.';
				if (!ok) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Different codec types (%s vs %s) in same AdaptationSet - disabling %s\n", rep_sel->codecs, rep->codecs, rep->codecs));
					rep->playback.disabled = 1;
					continue;
				}
				if (rep->segment_list && rep->segment_list->xlink_href) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Representation SegmentList uses xlink:href to %s - disabling because not supported\n", rep->segment_list->xlink_href));
					rep->playback.disabled = 1;
					continue;
				}
			}
			if ((dash->first_select_mode==0) && (rep->bandwidth < rep_sel->bandwidth)) {
				active_rep = rep_i;
			} else if ((dash->first_select_mode==1) && (rep->bandwidth > rep_sel->bandwidth)) {
				active_rep = rep_i;
			} else if ((dash->first_select_mode==2) && (rep->quality_ranking < rep_sel->quality_ranking)) {
				active_rep = rep_i;
			} else if ((dash->first_select_mode==3) && (rep->quality_ranking > rep_sel->quality_ranking)) {
				active_rep = rep_i;
			}
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
	}
	if (!nb_groups_ok) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] No AdaptationSet could be selected in the MPD - Cannot play\n"));
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	/*and seek if needed*/
	return GF_OK;
}


static u32 dash_main_thread_proc(void *par)
{
	GF_Err e;
	GF_DashClient *dash = (GF_DashClient*) par;
	GF_MPD_Representation *rep;
	u32 i, group_count, ret = 0;
	Bool go_on = 1;
	char *new_base_seg_url;
	assert(dash);
	if (!dash->mpd) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Incorrect state, no dash->mpd for URL=%s, already stopped ?\n", dash->base_url));
		return 1;
	}

	/* Setting the download status in exclusive code */
	gf_mx_p(dash->dl_mutex);
	dash->mpd_is_running = GF_DASH_STATE_CONNECTING;
	gf_mx_v(dash->dl_mutex);

restart_period:
	dash->in_period_setup = 1;

	dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_SELECT_GROUPS, GF_OK);

	e = GF_OK;
	group_count = gf_list_count(dash->groups);
	for (i=0; i<group_count; i++) {
		GF_DASH_Group *group = gf_list_get(dash->groups, i);
		if (group->selection != GF_DASH_GROUP_SELECTED) continue;
		e = gf_dash_download_init_segment(dash, group);
		if (e) break;
	}
	dash->mpd_stop_request=0;

	/*if error signal to the user*/
	if (e != GF_OK) {
		dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_PERIOD_SETUP_ERROR, e);
		ret = 1;
		goto exit;
	}


	dash->last_update_time = gf_sys_clock();

	gf_mx_p(dash->dl_mutex);
	dash->mpd_is_running = GF_DASH_STATE_CONNECTING;
	gf_mx_v(dash->dl_mutex);


	/*ask the user to connect to desired groups*/
	e = dash->dash_io->on_dash_event(dash->dash_io, GF_DASH_EVENT_CREATE_PLAYBACK, GF_OK);
	if (e) {
		ret = 1;
		goto exit;
	}

	gf_mx_p(dash->dl_mutex);
	dash->in_period_setup = 0;
	dash->mpd_is_running = GF_DASH_STATE_RUNNING;
	gf_mx_v(dash->dl_mutex);

	while (go_on) {
		const char *local_file_name = NULL;
		const char *resource_name = NULL;
		/*wait until next segment is needed*/
		while (!dash->mpd_stop_request) {
			u32 timer = gf_sys_clock() - dash->last_update_time;
			Bool shouldParsePlaylist = dash->mpd->minimum_update_period && (timer > dash->mpd->minimum_update_period); 

			if (shouldParsePlaylist) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Time to update the playlist (%u ms ellapsed since last refresh and min reoad rate is %u)\n", timer, dash->mpd->minimum_update_period));
				e = gf_dash_update_manifest(dash);
				group_count = gf_list_count(dash->groups);
				if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error updating MPD %s\n", gf_error_to_string(e)));
				}
			} else {
				Bool all_groups_done = 1;
				Bool cache_full = 1;
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

					gf_dash_setup_period(dash);
					dash->request_period_switch = 0;

					goto restart_period;
				}

				gf_sleep(16);
			}
		}

		/* stop the thread if requested */
		if (dash->mpd_stop_request) {
			go_on = 0;
			break;
		}

		/*for each selected groups*/
		for (i=0; i<group_count; i++) {		
			u64 start_range, end_range;
			Bool use_byterange;
			u32 representation_index;
			GF_DASH_Group *group = gf_list_get(dash->groups, i);
			if (group->selection != GF_DASH_GROUP_SELECTED) continue;
			if (group->done) continue;


			if (group->nb_cached_segments>=group->max_cached_segments) {
				continue;
			}

			/*remember the active rep index, since group->active_rep_index may change because of bandwidth control algorithm*/
			representation_index = group->active_rep_index;
			rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);

			/* if the index of the segment to be downloaded is greater or equal to the last segment (as seen in the playlist),
			we need to check if a new playlist is ready */
			if (group->download_segment_index>=group->nb_segments_in_rep) {
				u32 timer = gf_sys_clock() - dash->last_update_time;
				/* update of the playlist, only if indicated */
				if (dash->mpd->minimum_update_period && timer > dash->mpd->minimum_update_period) {
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
					if (dash->mpd->minimum_update_period) {
						/* if there is a specified update period, we redo the whole process */
						continue;
					} else {
						/* if not, we are really at the end of the playlist, we can quit */
						GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] End of playlist reached... downloading remaining elements..."));
						group->done = 1;
						break;
					}
				}
			}
			gf_mx_p(dash->dl_mutex);

			/* At this stage, there are some segments left to be downloaded */
			e = gf_dash_resolve_url(dash->mpd, rep, group, dash->base_url, GF_DASH_RESOLVE_URL_MEDIA, group->download_segment_index, &new_base_seg_url, &start_range, &end_range, &group->current_downloaded_segment_duration);
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
			if (!strstr(new_base_seg_url, "://") || !strnicmp(new_base_seg_url, "file://", 7)) {
				resource_name = local_file_name = (char *) new_base_seg_url; 
				e = GF_OK;
				/*do not erase local files*/
				group->local_files = 1;
				if (group->force_switch_bandwidth && !dash->auto_switch_count) {
					gf_dash_switch_group_representation(dash, group);
					/*restart*/
					i--;
					continue;
				}

			} else {
				u32 total_size, bytes_per_sec;

				group->max_bitrate = 0;
				group->min_bitrate = (u32)-1;
				/*use persistent connection for segment downloads*/
				if (use_byterange) {
					e = gf_dash_download_resource(dash->dash_io, &(group->segment_download), new_base_seg_url, start_range, end_range, 1, group);
				} else {
					e = gf_dash_download_resource(dash->dash_io, &(group->segment_download), new_base_seg_url, 0, 0, 1, group);
				}

				/*TODO decide what is te best, fetch from another representation or ignore ...*/
				if (e != GF_OK) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error in downloading new segment: %s %s\n", new_base_seg_url, gf_error_to_string(e)));
					group->download_segment_index++;
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

				total_size = dash->dash_io->get_total_size(dash->dash_io, group->segment_download);
				bytes_per_sec = dash->dash_io->get_bytes_per_sec(dash->dash_io, group->segment_download);

				if (total_size && bytes_per_sec && group->current_downloaded_segment_duration) {
					Double bitrate, time;
					bitrate = 8*total_size;
					bitrate *= 1000;
					bitrate /= group->current_downloaded_segment_duration;
					bitrate /= 1024;
					time = total_size;
					time /= bytes_per_sec;

					GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Downloaded segment %d bytes in %g seconds - duration %g sec - Bandwidth (kbps): indicated %d - computed %d - download %d\n", total_size, time, group->current_downloaded_segment_duration/1000.0, rep->bandwidth/1024, (u32) bitrate, 8*bytes_per_sec/1024));

					if (rep->bandwidth < 8*bytes_per_sec) {
						u32 k;
						/*find highest bandwidth that fits our bitrate*/
						GF_MPD_Representation *new_rep = NULL;
						for (k=0; k<gf_list_count(group->adaptation_set->representations); k++) {
							GF_MPD_Representation *arep = gf_list_get(group->adaptation_set->representations, k);
							if (8*bytes_per_sec > arep->bandwidth) {
								if (!new_rep) new_rep = arep;
								else if (arep->bandwidth > new_rep->bandwidth) {
									new_rep = arep;
								}
							}
						}
						if (!dash->disable_switching && new_rep && (new_rep!=rep)) {
							gf_dash_set_group_representation(group, new_rep);
						}
					}
				}
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
				if (group->local_files && use_byterange) {
					group->cached[group->nb_cached_segments].start_range = start_range;
					group->cached[group->nb_cached_segments].end_range = end_range;
				}
				if (!group->local_files) {
					GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Added file to cache (%u/%u in cache): %s\n", group->nb_cached_segments+1, group->max_cached_segments, group->cached[group->nb_cached_segments].url));
				}
				group->nb_cached_segments++;
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
	dash->mpd_is_running = GF_DASH_STATE_STOPPED;
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
	if (dash->mpd_is_running != GF_DASH_STATE_STOPPED) {
		dash->mpd_stop_request = 1;
		gf_mx_v(dash->dl_mutex);
		while (1) {
			/* waiting for the download thread to stop */
			gf_sleep(16);
			gf_mx_p(dash->dl_mutex);
			if (dash->mpd_is_running != GF_DASH_STATE_RUNNING) {
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
	GF_DASHFileIOSession *sess = dash->dash_io->create(dash->dash_io, 0, url);
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
	Bool is_m3u8 = 0;
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
			is_m3u8 = 1;
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
		if (gf_dash_is_m3u8_mime(mime) || strstr(reloc_url, ".m3u8") || strstr(reloc_url, ".M3U8")) {
			is_m3u8 = 1;
		} else if (!gf_dash_is_dash_mime(mime) && !strstr(reloc_url, ".mpd") && !strstr(reloc_url, ".MPD")) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] mime '%s' for '%s' should be m3u8 or mpd\n", mime, reloc_url));
			dash->dash_io->del(dash->dash_io, dash->mpd_dnload);
			dash->mpd_dnload = NULL;
			return GF_REMOTE_SERVICE_ERROR;
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
			is_m3u8 = 1;
	}

	if (is_local) {
		FILE *f = fopen(local_url, "rt");
		if (!f) return GF_URL_ERROR;
		fclose(f);
	}

	if (is_m3u8) {
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

	e = gf_dash_setup_period(dash);
	if (e) goto exit;

	return gf_th_run(dash->dash_thread, dash_main_thread_proc, dash);

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
	gf_dash_reset_groups(dash);

	if (dash->mpd_dnload) {
		dash->dash_io->del(dash->dash_io, dash->mpd_dnload);
		dash->mpd_dnload = NULL;
	}
	if (dash->mpd)
		gf_mpd_del(dash->mpd);
	dash->mpd = NULL;
}

GF_EXPORT
GF_DashClient *gf_dash_new(GF_DASHFileIO *dash_io, u32 max_cache_duration_sec, u32 auto_switch_count, Bool keep_files, Bool disable_switching, u32 first_select_mode)
{
	GF_DashClient *dash;
	GF_SAFEALLOC(dash, GF_DashClient);
	dash->dash_io = dash_io;

	dash->dash_thread = gf_th_new("MPD Segment Downloader Thread");
	dash->dl_mutex = gf_mx_new("MPD Segment Downloader Mutex");
	dash->mimeTypeForM3U8Segments = gf_strdup( M3U8_UNKOWN_MIME_TYPE );

	dash->max_cache_duration = max_cache_duration_sec;

	dash->auto_switch_count = auto_switch_count;
	dash->keep_files = keep_files;
	dash->disable_switching = disable_switching;
	dash->first_select_mode = first_select_mode;	
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

static GF_DASH_Group *gf_dash_group_get(GF_DashClient *dash, u32 idx)
{
	return gf_list_get(dash->groups, idx);
}

GF_EXPORT
void *gf_dash_get_group_udta(GF_DashClient *dash, u32 idx)
{
	GF_DASH_Group *group = gf_dash_group_get(dash, idx);
	if (!group) return NULL;
	return group->udta;
}

GF_EXPORT
GF_Err gf_dash_set_group_udta(GF_DashClient *dash, u32 idx, void *udta)
{
	GF_DASH_Group *group = gf_dash_group_get(dash, idx);
	if (!group) return GF_BAD_PARAM;
	group->udta = udta;
	return GF_OK;
}

GF_EXPORT
Bool gf_dash_is_group_selected(GF_DashClient *dash, u32 idx)
{
	GF_DASH_Group *group = gf_dash_group_get(dash, idx);
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
void gf_dash_switch_quality(GF_DashClient *dash, Bool switch_up)
{
	u32 i;
	for (i=0; i<gf_list_count(dash->groups); i++) {
		Bool do_switch = 0;
		GF_DASH_Group *group = gf_list_get(dash->groups, i);
		u32 current_idx = group->active_rep_index;
		if (group->selection != GF_DASH_GROUP_SELECTED) continue;

		if (group->force_representation_idx_plus_one) current_idx = group->force_representation_idx_plus_one - 1;
		if (switch_up) {
			if (current_idx + 1 < gf_list_count(group->adaptation_set->representations)) {
				group->force_representation_idx_plus_one = 1 + current_idx+1;
				do_switch = 1;
			}
		} else {
			if (current_idx) {
				group->force_representation_idx_plus_one = 1 + current_idx - 1;
				do_switch = 1;
			}
		}
		if (do_switch) {
			gf_mx_p(dash->dl_mutex);
			group->force_switch_bandwidth = 1;
			/*in local playback just switch at the end of the current segment
			for remote, we should let the user decide*/
			while (group->nb_cached_segments>1) {
				group->nb_cached_segments--;
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
				assert(group->download_segment_index>1);
				group->download_segment_index--;
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
	GF_DASH_Group *group = gf_dash_group_get(dash, idx);
	if (!group) return NULL;

	rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);
	return gf_dash_get_mime_type(NULL, rep, group->adaptation_set);
}

GF_EXPORT
const char *gf_dash_group_get_segment_init_url(GF_DashClient *dash, u32 idx, u64 *start_range, u64 *end_range)
{
	GF_DASH_Group *group = gf_dash_group_get(dash, idx);
	if (!group) return NULL;

	if (start_range) *start_range = group->local_url_start_range;
	if (end_range) *end_range = group->local_url_end_range;		
	return group->segment_local_url;
}

GF_EXPORT
void gf_dash_group_select(GF_DashClient *dash, u32 idx, Bool select)
{
	GF_DASH_Group *group = gf_dash_group_get(dash, idx);
	if (!group) return;
	if (group->selection == GF_DASH_GROUP_NOT_SELECTABLE)
		return;

	group->selection = select ? GF_DASH_GROUP_SELECTED : GF_DASH_GROUP_NOT_SELECTED;
	/*this set is part of a group, make sure no all other sets from the indicated group are unselected*/
	if (select && (group->adaptation_set->group>=0)) {
		u32 i;
		for (i=0; i<gf_dash_get_group_count(dash); i++) {
			GF_DASH_Group *agroup = gf_dash_group_get(dash, i);
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
Bool gf_dash_is_running(GF_DashClient *dash) 
{
	return dash->mpd_is_running;
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
	GF_DASH_Group *group = gf_dash_group_get(dash, idx);
	return group->max_cached_segments;
}

GF_EXPORT
u32 gf_dash_group_get_num_segments_ready(GF_DashClient *dash, u32 idx, Bool *group_is_done)
{
	u32 res = 0;
	GF_DASH_Group *group;

	gf_mx_p(dash->dl_mutex);
	group = gf_dash_group_get(dash, idx);

	*group_is_done = 0;
	if (!group) {
		*group_is_done = 1;
	} else {
		*group_is_done = group->done;
		res = group->nb_cached_segments;
	}
	gf_mx_v(dash->dl_mutex);
	return res;
}

GF_EXPORT
void gf_dash_group_discard_segment(GF_DashClient *dash, u32 idx)
{
	GF_DASH_Group *group;

	gf_mx_p(dash->dl_mutex);
	group = gf_dash_group_get(dash, idx);

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
	}
	memmove(&group->cached[0], &group->cached[1], sizeof(segment_cache_entry)*(group->nb_cached_segments-1));
	memset(&(group->cached[group->nb_cached_segments-1]), 0, sizeof(segment_cache_entry));
	group->nb_cached_segments--;

	gf_mx_v(dash->dl_mutex);
}

GF_EXPORT
void gf_dash_set_group_done(GF_DashClient *dash, u32 idx, Bool done)
{
	GF_DASH_Group *group = gf_dash_group_get(dash, idx);
	if (group) {
		group->done = done;
		if (done && group->segment_download) dash->dash_io->abort(dash->dash_io, group->segment_download);
	}

}

GF_EXPORT
GF_Err gf_dash_group_get_next_segment_location(GF_DashClient *dash, u32 idx, const char **url, u64 *start_range, u64 *end_range, const char **switching_url, u64 *switching_start_range, u64 *switching_end_range, const char **original_url)
{
	GF_DASH_Group *group;

	*url = *switching_url = NULL;
	*start_range = *end_range = *switching_start_range = *switching_end_range = 0;
	if (original_url) *original_url = NULL;

	gf_mx_p(dash->dl_mutex);	
	group = gf_dash_group_get(dash, idx);
	if (!group->nb_cached_segments) {
		gf_mx_v(dash->dl_mutex);	
		return GF_BAD_PARAM;
	}
	*url = group->cached[0].cache;
	*start_range = group->cached[0].start_range;
	*end_range = group->cached[0].end_range;
	if (original_url) *original_url = group->cached[0].url;

	if (group->cached[0].representation_index != group->prev_active_rep_index) {
		GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, group->cached[0].representation_index);
		*switching_start_range = rep->playback.init_start_range;
		*switching_end_range = rep->playback.init_end_range;
		*switching_url = rep->playback.cached_init_segment_url;
	}
	group->force_segment_switch = 0;
	group->prev_active_rep_index = group->cached[0].representation_index;
	gf_mx_v(dash->dl_mutex);	
	return GF_OK;
}

GF_EXPORT
void gf_dash_seek(GF_DashClient *dash, Double start_range)
{
	gf_mx_p(dash->dl_mutex);	
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
	GF_DASH_Group *group = gf_dash_group_get(dash, idx);
	return group->force_segment_switch;
}

GF_EXPORT
Double gf_dash_group_current_segment_start_time(GF_DashClient *dash, u32 idx)
{
	GF_DASH_Group *group = gf_dash_group_get(dash, idx);
	return gf_dash_get_segment_start_time(group);
}

#endif //GPAC_DISABLE_DASH_CLIENT

