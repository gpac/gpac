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

/*!
 *	\file <gpac/dash.h>
 *	\brief DASH Client API. The DASH client can be used without GPAC player but requires at least the base utils (threads, lists, NTP timing). The HTTP interface used can be either GPAC's one or any other downloader.
 */
	
/*!
 *	\addtogroup dashc_grp DASH
 *	\ingroup media_grp
 *	\brief MPEG-DASH and HLS Client
 *
 *	@{
 */

#include <gpac/tools.h>

#ifndef GPAC_DISABLE_DASH_CLIENT

/*!
 * All the possible Mime-types for MPD files
 */
static const char * const GF_DASH_MPD_MIME_TYPES[] = { "application/dash+xml", "video/vnd.3gpp.mpd", "audio/vnd.3gpp.mpd", "video/vnd.mpeg.dash.mpd", "audio/vnd.mpeg.dash.mpd", NULL };

/*!
 * All the possible Mime-types for M3U8 files
 */
static const char * const GF_DASH_M3U8_MIME_TYPES[] = { "video/x-mpegurl", "audio/x-mpegurl", "application/x-mpegurl", "application/vnd.apple.mpegurl", NULL};

/*! DASH Event type. The DASH client communitcaes with the user through a callback mechanism using events*/
typedef enum
{
	/*! event sent if an error occurs when setting up manifest*/
	GF_DASH_EVENT_MANIFEST_INIT_ERROR,
	/*! event sent before groups first segment is fetched - user shall decide which group to select at this point*/
	GF_DASH_EVENT_SELECT_GROUPS,
	/*! event sent if an error occurs during period setup - the download thread will exit at this point*/
	GF_DASH_EVENT_PERIOD_SETUP_ERROR,
	/*! event sent once the first segment of each selected group is fetched - user should load playback chain(s) at this point*/
	GF_DASH_EVENT_CREATE_PLAYBACK,
	/*! event sent when reseting groups at period switch or at exit - user should unload playback chain(s) at this point*/
	GF_DASH_EVENT_DESTROY_PLAYBACK,
	/*! event sent after each segment begins/ends download when segment bufferig is enabled*/
	GF_DASH_EVENT_BUFFERING,
	/*! event sent once buffering is done*/
	GF_DASH_EVENT_BUFFER_DONE,
	/*! event sent once a new segment becomes available*/
	GF_DASH_EVENT_SEGMENT_AVAILABLE,
	/*! event sent when quality has been switched for the given group*/
	GF_DASH_EVENT_QUALITY_SWITCH,
	/*! position in timeshift buffer has changed (eg, paused)*/
	GF_DASH_EVENT_TIMESHIFT_UPDATE,
	/*! event sent when timeshift buffer is overflown - the group_idx param contains the max number of dropped segments of all representations droped by the client, or -1 if play pos is ahead of live */
	GF_DASH_EVENT_TIMESHIFT_OVERFLOW,
	/*! event send when we need the decoding statistics*/
	GF_DASH_EVENT_CODEC_STAT_QUERY,
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

	/*used to check whether a representation is supported or not. Function returns 1 if supported, 0 otherwise
	if this callback is not set, the representation is assumed to be supported*/
	Bool (*dash_codec_supported)(GF_DASHFileIO *dashio, const char *codec, u32 width, u32 height, Bool is_interlaced, u32 fps_num, u32 fps_denum, u32 sample_rate);

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
	/*get the average download rate for the session. If no session is specified, gets the max download rate
	for the client (used for bandwidth simulation in local files)*/
	u32 (*get_bytes_per_sec)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session);
	/*get the total size on bytes for the session*/
	u32 (*get_total_size)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session);
	/*get the total size on bytes for the session*/
	u32 (*get_bytes_done)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session);
};

typedef struct __dash_client GF_DashClient;

typedef enum
{
	//selects the lowest quality when starting - if one of the representation does not have video (HLS), it may be selected
	GF_DASH_SELECT_QUALITY_LOWEST,
	//selects the highest quality when starting
	GF_DASH_SELECT_QUALITY_HIGHEST,
	//selects the lowest bandwidth when starting - if one of the representation does not have video (HLS), it will NOT be selected
	GF_DASH_SELECT_BANDWIDTH_LOWEST,
	//selects the highest bandwidth when starting - for tiles all low priority tiles will have the lower (below max) bandwidth selected
	GF_DASH_SELECT_BANDWIDTH_HIGHEST,
	//selects the highest bandwidth when starting - for tiles all low priority tiles will have their lowest bandwidth selected
	GF_DASH_SELECT_BANDWIDTH_HIGHEST_TILES
} GF_DASHInitialSelectionMode;

/*create a new DASH client:
	@dash_io: DASH callbacks to the user
	@max_cache_duration: maximum duration in milliseconds for the cached media. If less than mpd@minBufferTime, mpd@minBufferTime is used
	@auto_switch_count: forces representation switching every auto_switch_count segments, set to 0 to disable
	@keep_files: do not delete files from the cache
	@disable_switching: turn off bandwidth switching algorithm
	@first_select_mode: indicates which representation to select upon startup
	@enable_buffering: forces buffering of segments for the duration indicated in the MPD before calling back the user
	@initial_time_shift_value: sets initial buffering: if between 0 and 100, this is a percentage of the time shift window of the session. If greater than 100, this is a time shift in milliseconds.
*/
GF_DashClient *gf_dash_new(GF_DASHFileIO *dash_io,
                           u32 max_cache_duration,
                           u32 auto_switch_count,
                           Bool keep_files,
                           Bool disable_switching,
                           GF_DASHInitialSelectionMode first_select_mode,
                           Bool enable_buffering, u32 initial_time_shift_value);

/*delete the DASH client*/
void gf_dash_del(GF_DashClient *dash);

/*open the DASH client for the specific manifest file*/
GF_Err gf_dash_open(GF_DashClient *dash, const char *manifest_url);
/*closes the dash client*/
void gf_dash_close(GF_DashClient *dash);

/*returns URL of the DASH manifest file*/
const char *gf_dash_get_url(GF_DashClient *dash);

/*tells whether we are playing some Apple HLS M3U8*/
Bool gf_dash_is_m3u8(GF_DashClient *dash);

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

/*sets timeshift for the presentation - this function does not trigger a seek, this has to be done by the caller
	@ms_in_timeshift: if between 0 and 100, this is a percentage of the time shift window of the session. If greater than 100, this is a time shift in milliseconds.
*/
GF_Err gf_dash_set_timeshift(GF_DashClient *dash, u32 ms_in_timeshift);

/*returns the number of groups. A group is a set of media resources that are alternate of each other in terms of bandwidth/quality.*/
u32 gf_dash_get_group_count(GF_DashClient *dash);
/*associate user data (or NULL) with this group*/
GF_Err gf_dash_set_group_udta(GF_DashClient *dash, u32 group_index, void *udta);
/*returns the user data associated with this group*/
void *gf_dash_get_group_udta(GF_DashClient *dash, u32 group_index);
/*indicates whether a group is selected for playback or not. Currently groups cannot be selected during playback*/
Bool gf_dash_is_group_selected(GF_DashClient *dash, u32 group_index);

/*indicates whether this group is dependent on another group (because representations are). If this is the case, all representations of this group will be made available through the base group (and @has_next_segment flag) if the group is selected.
returns -1 if not dependent on another group, otherwise return dependent group index*/
s32 gf_dash_group_has_dependent_group(GF_DashClient *dash, u32 idx);

/*gives the number of groups depending on this one for decoding.*/
u32 gf_dash_group_get_num_groups_depending_on(GF_DashClient *dash, u32 idx);

/*gets the index of the depending_on group with the specified group_depending_on_dep_idx (between 0 and gf_dash_group_get_num_groups_depending_on()-1)*/
s32 gf_dash_get_dependent_group_index(GF_DashClient *dash, u32 idx, u32 group_depending_on_dep_idx);

/*indicates whether a group can be selected for playback or not. Some groups may have been disabled because of non supported features*/
Bool gf_dash_is_group_selectable(GF_DashClient *dash, u32 idx);

/*selects a group for playback. If other groups are alternate to this group (through the @group attribute), they are automatically deselected. */
void gf_dash_group_select(GF_DashClient *dash, u32 idx, Bool select);

/*performs selection of representations based on language code*/
void gf_dash_groups_set_language(GF_DashClient *dash, const char *lang_code_rfc_5646);

/*returns the mime type of the media resources in this group*/
const char *gf_dash_group_get_segment_mime(GF_DashClient *dash, u32 idx);
/*returns the URL of the first media resource to play (init segment or first media segment depending on format). start_range and end_range are optional*/
const char *gf_dash_group_get_segment_init_url(GF_DashClient *dash, u32 idx, u64 *start_range, u64 *end_range);

/*returns the URL and IV associated with the first media segment if any (init segment or first media segment depending on format). key_IV is optional*/
const char *gf_dash_group_get_segment_init_keys(GF_DashClient *dash, u32 idx, bin128 *key_IV);

/*returns the language of the group, or NULL if none associated*/
const char *gf_dash_group_get_language(GF_DashClient *dash, u32 idx);

/*returns the language of the group, or NULL if none associated*/
u32 gf_dash_group_get_audio_channels(GF_DashClient *dash, u32 idx);

/*get time shift buffer depth of the group - (u32) -1 means infinity*/
u32 gf_dash_group_get_time_shift_buffer_depth(GF_DashClient *dash, u32 idx);

/*get current time in time shift buffer in seconds - 0 means 'live point'
this gets the maximum value (further in the past) of all representations playing*/
Double gf_dash_get_timeshift_buffer_pos(GF_DashClient *dash);

/*sets codec statistics for playback rate adjustment*/
void gf_dash_set_codec_stat(GF_DashClient *dash, u32 idx, u32 avg_dec_time, u32 max_dec_time, u32 irap_avg_dec_time, u32 irap_max_dec_time, Bool codec_reset, Bool decode_only_rap);

/*sets buffer levels*/
void gf_dash_set_buffer_levels(GF_DashClient *dash, u32 idx, u32 buffer_min_ms, u32 buffer_max_ms, u32 buffer_occupancy_ms);

typedef enum
{
	GF_MPD_DESC_ACCESSIBILITY,
	GF_MPD_DESC_AUDIOCONFIG,
	GF_MPD_DESC_CONTENT_PROTECTION,
	GF_MPD_DESC_ESSENTIAL_PROPERTIES,
	GF_MPD_DESC_SUPPLEMENTAL_PROPERTIES,
	GF_MPD_DESC_FRAME_PACKING,
	GF_MPD_DESC_ROLE,
	GF_MPD_DESC_RATING,
	GF_MPD_DESC_VIEWPOINT
} GF_DashDescriptorType;

//enumerate descriptors of the given type:
//group_idx: index of the group for which descriptors are enumerated
//desc_type: type of descriptor being checked, one of the above
//role_idx: index of the descriptor being checked for this type
Bool gf_dash_group_enum_descriptor(GF_DashClient *dash, u32 group_idx, GF_DashDescriptorType desc_type, u32 role_idx, const char **desc_id, const char **desc_scheme, const char **desc_value);

/*returns the URL and byte range of the next media resource to play in this group.
If switching occured, sets switching_index to the new representation index.
If no bitstream switching is possible, also set the url and byte range of the media file required to intialize
the playback of the next segment
original_url is optional and may be used to het the URI of the segment
*/
GF_Err gf_dash_group_get_next_segment_location(GF_DashClient *dash, u32 idx, u32 dependent_representation_index, const char **url, u64 *start_range, u64 *end_range,
        s32 *switching_index, const char **switching_url, u64 *switching_start_range, u64 *switching_end_range,
        const char **original_url, Bool *has_next_segment, const char **key_url, bin128 *key_IV);

/*same as gf_dash_group_get_next_segment_location but query the current downloaded segment*/
GF_EXPORT
GF_Err gf_dash_group_probe_current_download_segment_location(GF_DashClient *dash, u32 idx, const char **url, s32 *switching_index, const char **switching_url, const char **original_url, Bool *switched);

/*returns 1 if segment numbers loops at this level (not allowed but happens when looping captures ...*/
Bool gf_dash_group_loop_detected(GF_DashClient *dash, u32 idx);

/*returns number of seconds at which playback shall start for the group in the current period
the first segment available for the period will be so that gf_dash_group_get_start_range is in this range after the caller
adjusts it with PTO (eg the returned time is in period timeline, not media timeline */
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
	2 if the switching is in progress (all groups will soon be destroyed and plyback will be stopped and restarted)
	0 if no switching is requested
*/
u32 gf_dash_get_period_switch_status(GF_DashClient *dash);
/*request period switch - this is typically called when the media engine signals that no more data is available for playback*/
void gf_dash_request_period_switch(GF_DashClient *dash);
/*returns 1 if the DASH engine is currently setting up a period (creating groups and fetching initial segments)*/
Bool gf_dash_in_period_setup(GF_DashClient *dash);
/*seeks playback to the given time. If period changes, all playback is stopped and restarted
If the session is dynamic (live), the start_range is ignored and recomputed from current UTC clock to be at the live point. If timeshifting is desired, use @gf_dash_set_timeshift before seeking.*/
void gf_dash_seek(GF_DashClient *dash, Double start_range);
/*when seeking, this flag is set when the seek is outside of the previously playing segment.*/
Bool gf_dash_group_segment_switch_forced(GF_DashClient *dash, u32 idx);
/*get video info for this group if video*/
GF_Err gf_dash_group_get_video_info(GF_DashClient *dash, u32 idx, u32 *max_width, u32 *max_height);

/*sets playback speed of the session. Speed is used in adaptation logic*/
void gf_dash_set_speed(GF_DashClient *dash, Double speed);

/*returns the start_time of the first segment in the queue (usually the one being played)*/
Double gf_dash_group_current_segment_start_time(GF_DashClient *dash, u32 idx);

/*allow reloading of MPD on the local file system - useful for testing live generators*/
void gf_dash_allow_local_mpd_update(GF_DashClient *dash, Bool allow_local_mpd_update);

/*gets media info for representation*/
GF_Err gf_dash_group_get_representation_info(GF_DashClient *dash, u32 idx, u32 representation_idx, u32 *width, u32 *height, u32 *audio_samplerate, u32 *bandwidth, const char **codecs);

/*gets media buffering info for all active representations*/
void gf_dash_get_buffer_info(GF_DashClient *dash, u32 *total_buffer, u32 *media_buffered);

/*updates media bandwidth for the given group. Only allowed for groups without dependencies to other groups*/
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

// gets the difference between the local UTC clock and the one reported by the server 
s32 gf_dash_get_utc_drift_estimate(GF_DashClient *dash);

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

//indicates the number of segments to wait before switching up bandwidth. The default value is 1 (ie stay in current
//bandwidth or one more segment before switching up, event if download rate is enough)
//seting to 0 means the switch will happen instantly, but this is more prone to quality changes due to network variations
void gf_dash_set_switching_probe_count(GF_DashClient *dash, u32 switch_probe_count);
/*If agressive switching is enabled, switching targets to the closest bandwidth fitting the available download rate. Otherwise, switching targets the lowest bitrate representation that is above the currently played (eg does not try to switch to max bandwidth). Default value is no.
*/
void gf_dash_set_agressive_adaptation(GF_DashClient *dash, Bool eanble_agressive_switch);

/*returns active period start in ms*/
u64 gf_dash_get_period_start(GF_DashClient *dash);
/*returns active period duration in ms*/
u64 gf_dash_get_period_duration(GF_DashClient *dash);

//returns number of quality available for the given group
u32 gf_dash_group_get_num_qualities(GF_DashClient *dash, u32 idx);

void gf_dash_disable_speed_adaptation(GF_DashClient *dash, Bool disable);


typedef struct
{
	u32 bandwidth;
	const char *ID;
	const char *mime;
	const char *codec;
	u32 width;
	u32 height;
	Bool interlaced;
	u32 fps_den, fps_num;
	u32 par_num;
	u32 par_den;
	u32 sample_rate;
	u32 nb_channels;
	Bool disabled;
	Bool is_selected;
} GF_DASHQualityInfo;

//returns number of quality available for the given group
GF_Err gf_dash_group_get_quality_info(GF_DashClient *dash, u32 idx, u32 quality_idx, GF_DASHQualityInfo *quality);

//returns 1 if automatic quality switching is enabled
Bool gf_dash_get_automatic_switching(GF_DashClient *dash);

//sets automatic quality switching enabled or disabled
GF_Err gf_dash_set_automatic_switching(GF_DashClient *dash, Bool enable_switching);

//selects quality of given ID
GF_Err gf_dash_group_select_quality(GF_DashClient *dash, u32 idx, const char *ID);

//gets download rate in bytes per second for the given group
u32 gf_dash_group_get_download_rate(GF_DashClient *dash, u32 idx);

//forces NTP of the DASH client to be the given NTP
void gf_dash_override_ntp(GF_DashClient *dash, u64 server_ntp);

//specifies how bitrate is allocated accross tiles of the same video
typedef enum
{
	//each tile receives the same amount of bitrate (default strategy)
	GF_DASH_ADAPT_TILE_NONE=0,
	//bitrate decreases for each row of tiles starting from the top, same rate for each tile on the row
	GF_DASH_ADAPT_TILE_ROWS,
	//bitrate decreases for each row of tiles starting from the bottom, same rate for each tile on the row
	GF_DASH_ADAPT_TILE_ROWS_REVERSE,
	//bitrate decreased for top and bottom rows only, same rate for each tile on the row
	GF_DASH_ADAPT_TILE_ROWS_MIDDLE,
	//bitrate decreases for each column of tiles starting from the left, same rate for each tile on the column
	GF_DASH_ADAPT_TILE_COLUMNS,
	//bitrate decreases for each column of tiles starting from the right, same rate for each tile on the column
	GF_DASH_ADAPT_TILE_COLUMNS_REVERSE,
	//bitrate decreased for left and right columns only, same rate for each tile on the column
	GF_DASH_ADAPT_TILE_COLUMNS_MIDDLE,
	//bitrate decreased for all tiles on the edge of the picture
	GF_DASH_ADAPT_TILE_CENTER,
	//bitrate decreased for all tiles on the center of the picture
	GF_DASH_ADAPT_TILE_EDGES,
} GF_DASHTileAdaptationMode;

//sets type of tile adaptation.
// @tile_rate_decrease: percentage (0->100) of global bandwidth to use at each level (recursive rate decrease for all level). If 0% or 100%, automatic rate allocation among tiles is performed (default mode)
void gf_dash_set_tile_adaptation_mode(GF_DashClient *dash, GF_DASHTileAdaptationMode mode, u32 tile_rate_decrease);

/*returns tile adaptation mode currently used*/
GF_DASHTileAdaptationMode gf_dash_get_tile_adaptation_mode(GF_DashClient *dash);

//gets max width and height in pixels of the SRD this group belongs to, if any
Bool gf_dash_group_get_srd_max_size_info(GF_DashClient *dash, u32 idx, u32 *max_width, u32 *max_height);

//gets SRD info, in SRD coordinate, of the SRD this group belongs to, if any
Bool gf_dash_group_get_srd_info(GF_DashClient *dash, u32 idx, u32 *srd_id, u32 *srd_x, u32 *srd_y, u32 *srd_w, u32 *srd_h, u32 *srd_width, u32 *srd_height);

//sets quality hint for the given group. Quality degradation ranges from 0 (no degradation) to 100 (worse quality possible)
GF_Err gf_dash_group_set_quality_degradation_hint(GF_DashClient *dash, u32 idx, u32 quality_degradation_hint);

//sets visible rectangle of a video object, may be used for adaptation. If min_x==max_x==min_y=max_y==0, disable adaptation
GF_Err gf_dash_group_set_visible_rect(GF_DashClient *dash, u32 idx, u32 min_x, u32 max_x, u32 min_y, u32 max_y);

/*Enables or disables threaded downloads of media files for the dash client
 @use_threads: if true, threads are used to download files*/
void gf_dash_set_threaded_download(GF_DashClient *dash, Bool use_threads);

#endif //GPAC_DISABLE_DASH_CLIENT

/*!	@} */

#ifdef __cplusplus
}
#endif

#endif	/*_GF_DASH_H_*/

