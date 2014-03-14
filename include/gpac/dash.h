/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2012
 *					All rights reserved
 *
 *  This file is part of GPAC / Adaptive HTTP Streaming sub-project
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


#ifndef _GF_DASH_H_
#define _GF_DASH_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/tools.h>

#ifndef GPAC_DISABLE_DASH_CLIENT

/*!
 * All the possible Mime-types for MPD files
 */
static const char * GF_DASH_MPD_MIME_TYPES[] = { "application/dash+xml", "video/vnd.3gpp.mpd", "audio/vnd.3gpp.mpd", NULL };

/*!
 * All the possible Mime-types for M3U8 files
 */
static const char * GF_DASH_M3U8_MIME_TYPES[] = { "video/x-mpegurl", "audio/x-mpegurl", "application/x-mpegurl", "application/vnd.apple.mpegurl", NULL};

typedef enum 
{
	/*event sent if an error occurs when setting up manifest*/
	GF_DASH_EVENT_MANIFEST_INIT_ERROR,
	/*event sent before groups first segment is fetched - user shall decide which group to select at this point*/
	GF_DASH_EVENT_SELECT_GROUPS,
	/*event sent if an error occurs during period setup - the download thread will exit at this point*/
	GF_DASH_EVENT_PERIOD_SETUP_ERROR,
	/*event sent once the first segment of each selected group is fetched - user should load playback chain(s) at this point*/
	GF_DASH_EVENT_CREATE_PLAYBACK,
	/*event sent when reseting groups at period switch or at exit - user should unload playback chain(s) at this point*/
	GF_DASH_EVENT_DESTROY_PLAYBACK,

	GF_DASH_EVENT_BUFFERING,
	GF_DASH_EVENT_BUFFER_DONE,

	GF_DASH_EVENT_SEGMENT_AVAILABLE,
} GF_DASHEventType;

/*structure used for all IO operations for DASH*/
typedef struct _gf_dash_io GF_DASHFileIO;
typedef void *GF_DASHFileIOSession;

struct _gf_dash_io
{
	/*user private data*/
	void *udta;
	
	/*signals errors or specific actions to perform*/
	GF_Err (*on_dash_event)(GF_DASHFileIO *dashio, GF_DASHEventType evt, s32 group_idx, GF_Err setup_error);

	/*called whenever a file has to be deleted*/
	void (*delete_cache_file)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session, const char *cache_url);

	/*create a file download session for the given resource - group_idx may be -1 if this is a global resource , otherwise it indicates the group/adaptationSet in which the download happens*/
	GF_DASHFileIOSession (*create)(GF_DASHFileIO *dashio, Bool persistent, const char *url, s32 group_idx);
	/*delete a file download session*/
	void (*del)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session);
	/*aborts downloading in the given file session*/
	void (*abort)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session);
	/*resetup the file session with a new resource to get - this allows persistent connection usage with HTTP servers*/
	GF_Err (*setup_from_url)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session, const char *url, s32 group_idx);
	/*set download range for the file session*/
	GF_Err (*set_range)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session, u64 start_range, u64 end_range, Bool discontinue_cache);
	/*initialize the file session - all the headers shall be fetched before returning*/
	GF_Err (*init)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session);
	/*download the content - synchronous call: all the file shall be fetched before returning*/
	GF_Err (*run)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session);

	/*get URL of the file - i tmay be different from the original one if resource relocation happened*/
	const char *(*get_url)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session);
	/*get the name of the cache file. If NULL is returned, the file cannot be cached and its associated UTL will be used when 
	the client request file to play*/
	const char *(*get_cache_name)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session);
	/*get the MIME type of the file*/
	const char *(*get_mime)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session);
	/*get the given hedaer value in the last HTTP response. Function is optional*/
	const char *(*get_header_value)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session, const char *header_name);
	/*gets the UTC time at which reply has been received. Function is optional*/
	u64 (*get_utc_start_time)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session);
	/*get the average download rate for the session*/
	u32 (*get_bytes_per_sec)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session);
	/*get the total size on bytes for the session*/
	u32 (*get_total_size)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session);
	/*get the total size on bytes for the session*/
	u32 (*get_bytes_done)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session);
};

typedef struct __dash_client GF_DashClient;

typedef enum
{
	GF_DASH_SELECT_QUALITY_LOWEST,
	GF_DASH_SELECT_QUALITY_HIGHEST,
	GF_DASH_SELECT_BANDWIDTH_LOWEST,
	GF_DASH_SELECT_BANDWIDTH_HIGHEST
} GF_DASHInitialSelectionMode;

/*create a new DASH client:
	@dash_io: DASH callbacks to the user
	@max_cache_duration: maximum duration in milliseconds for the cached media. If less than mpd@minBufferTime, mpd@minBufferTime is used
	@auto_switch_count: forces representation switching every auto_switch_count segments
	@keep_files: do not delete files from the cache
	@disable_switching: turn off bandwidth switching algorithm
	@first_select_mode: indicates which representation to select upon startup
*/
GF_DashClient *gf_dash_new(GF_DASHFileIO *dash_io, 
						   u32 max_cache_duration, 
						   u32 auto_switch_count, 
						   Bool keep_files, 
						   Bool disable_switching, 
						   GF_DASHInitialSelectionMode first_select_mode,
						   Bool enable_buffering, u32 initial_time_shift_percent);

/*delete the DASH client*/
void gf_dash_del(GF_DashClient *dash);

/*open the DASH client for the specific manifest file*/
GF_Err gf_dash_open(GF_DashClient *dash, const char *manifest_url);
/*closes the dash client*/
void gf_dash_close(GF_DashClient *dash);

/*returns URL of the DASH manifest file*/
const char *gf_dash_get_url(GF_DashClient *dash);

/*get title and source for this MPD*/
void gf_dash_get_info(GF_DashClient *dash, const char **title, const char **source);

/*switches quality up or down*/
void gf_dash_switch_quality(GF_DashClient *dash, Bool switch_up, Bool force_immediate_switch);

/*indicates whether the DASH client is running or not. For the moment, the DASH client is always run by an internal thread*/
Bool gf_dash_is_running(GF_DashClient *dash);

/*get duration of the presentation*/
Double gf_dash_get_duration(GF_DashClient *dash);
/*check that the given file has the right XML root element*/
Bool gf_dash_check_mpd_root_type(const char *local_url);


/*returns the number of groups. A group is a set of media resources that are alternate of each other in terms of bandwidth/quality.*/
u32 gf_dash_get_group_count(GF_DashClient *dash);
/*associate user data (or NULL) with this group*/
GF_Err gf_dash_set_group_udta(GF_DashClient *dash, u32 group_index, void *udta);
/*returns the user data associated with this group*/
void *gf_dash_get_group_udta(GF_DashClient *dash, u32 group_index);
/*indicates whether a group is selected for playback or not. Currently groups cannot be selected during playback*/
Bool gf_dash_is_group_selected(GF_DashClient *dash, u32 group_index);

/*indicates whether a group can be selected for playback or not. Some groups may have been disabled because of non supported features*/
Bool gf_dash_is_group_selectable(GF_DashClient *dash, u32 idx);

/*selects a group for playback. If other groups are alternate to this group (through the @group attribute), they are automatically deselected. */
void gf_dash_group_select(GF_DashClient *dash, u32 idx, Bool select);

/*performs selection of representations based on language code*/
void gf_dash_groups_set_language(GF_DashClient *dash, const char *lang_3cc);

/*returns the mime type of the media resources in this group*/
const char *gf_dash_group_get_segment_mime(GF_DashClient *dash, u32 idx);
/*returns the URL of tyhe first media resource to play (init segment or first media segment depending on format). start_range and end_range are optional*/
const char *gf_dash_group_get_segment_init_url(GF_DashClient *dash, u32 idx, u64 *start_range, u64 *end_range);

/*returns the URL and byte range of the next media resource to play in this group. 
If switching occured, sets switching_index to the new representation index.
If no bitstream switching is possible, also set the url and byte range of the media file required to intialize 
the playback of the next segment
original_url is optional and may be used to het the URI of the segment
*/
GF_Err gf_dash_group_get_next_segment_location(GF_DashClient *dash, u32 idx, u32 dependent_representation_index, const char **url, u64 *start_range, u64 *end_range, 
											s32 *switching_index, const char **switching_url, u64 *switching_start_range, u64 *switching_end_range, 
											const char **original_url, Bool *has_next_segment);

/*same as gf_dash_group_get_next_segment_location but query the current downloaded segment*/
GF_EXPORT
GF_Err gf_dash_group_probe_current_download_segment_location(GF_DashClient *dash, u32 idx, const char **url, s32 *switching_index, const char **switching_url, const char **original_url, Bool *switched);

/*returns 1 if segment numbers loops at this level (not allowed but happens when looping captures ...*/
Bool gf_dash_group_loop_detected(GF_DashClient *dash, u32 idx);

/*returns number of seconds at which playback shall start */
Double gf_dash_group_get_start_range(GF_DashClient *dash, u32 idx);

/*discards the first media resource in the queue of this group*/
void gf_dash_group_discard_segment(GF_DashClient *dash, u32 idx);
/*get the number of media resources available in the cache for this group*/
u32 gf_dash_group_get_num_segments_ready(GF_DashClient *dash, u32 idx, Bool *group_is_done);
/*get the maximum number of media resources  that can be put in the cache for this group*/
u32 gf_dash_group_get_max_segments_in_cache(GF_DashClient *dash, u32 idx);
/*indicates to the DASH engine that the group playback has been stopped by the user*/
void gf_dash_set_group_done(GF_DashClient *dash, u32 idx, Bool done);
/*gets presentationTimeOffset and timescale for the active representation*/
GF_Err gf_dash_group_get_presentation_time_offset(GF_DashClient *dash, u32 idx, u64 *presentation_time_offset, u32 *timescale);

/*returns 1 if the playback position is in the last period of the presentation*/
Bool gf_dash_in_last_period(GF_DashClient *dash);
/*return value: 
	1 if the period switching has been requested (due to seeking), 
	2 if the switching is in progress (all groups will soon be destroyed and plyback will be stoped and restarted)
	0 if no switching is requested
*/
u32 gf_dash_get_period_switch_status(GF_DashClient *dash);
/*request period switch - this is typically called when the media engine signals that no more data is available for playback*/
void gf_dash_request_period_switch(GF_DashClient *dash);
/*returns 1 if the DASH engine is currently setting up a period (creating groups and fetching initial segments)*/
Bool gf_dash_in_period_setup(GF_DashClient *dash);
/*seeks playback to the given time. If period changes, all playback is stopped and restarted*/
void gf_dash_seek(GF_DashClient *dash, Double start_range);
/*gets playback start range for the first segment to play after the seek has been done. This is the amount of data to skip from the first segment to be played*/
Double gf_dash_get_playback_start_range(GF_DashClient *dash);
/*when seeking, this flag is set when the seek is outside of the previously playing segment.*/
Bool gf_dash_group_segment_switch_forced(GF_DashClient *dash, u32 idx);
/*get video info for this group if video*/
GF_Err gf_dash_group_get_video_info(GF_DashClient *dash, u32 idx, u32 *max_width, u32 *max_height);

/*returns the start_time of the first segment in the queue (usually the one being played)*/
Double gf_dash_group_current_segment_start_time(GF_DashClient *dash, u32 idx);

/*allow reloading of MPD on the local file system - usefull for testing live generators*/
void gf_dash_allow_local_mpd_update(GF_DashClient *dash, Bool allow_local_mpd_update);

/*gets media info for representation*/
GF_Err gf_dash_group_get_representation_info(GF_DashClient *dash, u32 idx, u32 representation_idx, u32 *width, u32 *height, u32 *audio_samplerate, u32 *bandwidth, const char **codecs);

/*gets media buffering info for all active representations*/
void gf_dash_get_buffer_info_buffering(GF_DashClient *dash, u32 *total_buffer, u32 *media_buffered);

/*updates media bandwidth for the given group*/
GF_Err gf_dash_group_check_bandwidth(GF_DashClient *dash, u32 idx);

/*resync the downloader so that the next downloaded segment falls into the indicated range - used for error recovery*/
GF_Err gf_dash_resync_to_segment(GF_DashClient *dash, const char *latest_segment_name, const char *earliest_segment_name);

/*sets dash idle interval. The default is 1 sec. The dash client thread will never go to sleep for more than this interval*/
void gf_dash_set_idle_interval(GF_DashClient *dash, u32 idle_time_ms);

/*enables UTC drift computation using HTTP header "Server-UTC: UTC", where UTC is in ms*/
void gf_dash_enable_utc_drift_compensation(GF_DashClient *dash, Bool estimate_utc_drift);

/*returns GF-TRUE if MPD is dynamic, false otherwise*/
Bool gf_dash_is_dynamic_mpd(GF_DashClient *dash);

/*returns minimum buffer time indicated in mpd in ms*/
u32 gf_dash_get_min_buffer_time(GF_DashClient *dash);

//shifts UTC clock of server by shift_utc_ms so that new UTC in MPD is old + shift_utc_ms
void gf_dash_set_utc_shift(GF_DashClient *dash, s32 shift_utc_ms);

//sets max resolution@bpp for all video 
GF_Err gf_dash_set_max_resolution(GF_DashClient *dash, u32 width, u32 height, u8 max_display_bpp);

//sets min time in ms between a 404 and the next request on the same group. The default value is 500 ms. 
GF_Err gf_dash_set_min_timeout_between_404(GF_DashClient *dash, u32 min_timeout_between_404);

//sets time in ms after which 404 request for a segment will indicate segment lost. The default value is 100 ms. 
GF_Err gf_dash_set_segment_expiration_threshold(GF_DashClient *dash, u32 expire_after_ms);


//only enables the given group - this shall be set before calling @gf_dash_open. If group_index is <0 (default) no groups will be disabled. 
void gf_dash_debug_group(GF_DashClient *dash, s32 group_index);

//indicates typical buffering used by the user app . This allows fetching data earlier in live mode, if the timeshiftbuffer allows for it
void gf_dash_set_user_buffer(GF_DashClient *dash, u32 buffer_time_ms);

#endif //GPAC_DISABLE_DASH_CLIENT


#ifdef __cplusplus
}
#endif

#endif	/*_GF_DASH_H_*/

