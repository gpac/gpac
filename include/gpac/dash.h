/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2012-2023
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
\file <gpac/dash.h>
\brief DASH Client API. The DASH client can be used without GPAC player but requires at least the base utils (threads, lists, NTP timing). The HTTP interface used can be either GPAC's one or any other downloader.
*/
	
/*!
\addtogroup dashc_grp DASH Client
\ingroup media_grp
\brief MPEG-DASH and HLS Client

@{
*/

#include <gpac/tools.h>
#include <gpac/list.h>

#ifndef GPAC_DISABLE_DASHIN

/*!
 * All the possible Mime-types for MPD files
 */
static const char * const GF_DASH_MPD_MIME_TYPES[] = { "application/dash+xml", "video/vnd.3gpp.mpd", "audio/vnd.3gpp.mpd", "video/vnd.mpeg.dash.mpd", "audio/vnd.mpeg.dash.mpd", NULL };

/*!
 * All the possible Mime-types for M3U8 files
 */
static const char * const GF_DASH_M3U8_MIME_TYPES[] = { "video/x-mpegurl", "audio/x-mpegurl", "application/x-mpegURL", "application/vnd.apple.mpegURL", NULL};

/*!
 * All the possible Mime-types for Smooth files
 */
static const char * const GF_DASH_SMOOTH_MIME_TYPES[] = { "application/vnd.ms-sstr+xml", NULL};

/*! DASH Event type. The DASH client communicates with the user through a callback mechanism using events*/
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
	/*! event sent when resetting groups at period switch or at exit - user should unload playback chain(s) at this point*/
	GF_DASH_EVENT_DESTROY_PLAYBACK,
	/*! event sent once a new segment becomes available*/
	GF_DASH_EVENT_SEGMENT_AVAILABLE,
	/*! event sent when quality has been switched for the given group*/
	GF_DASH_EVENT_QUALITY_SWITCH,
	/*! position in timeshift buffer has changed (eg, paused)*/
	GF_DASH_EVENT_TIMESHIFT_UPDATE,
	/*! event sent when timeshift buffer is overflown - the group_idx param contains the max number of dropped segments of all representations dropped by the client, or -1 if play pos is ahead of live */
	GF_DASH_EVENT_TIMESHIFT_OVERFLOW,
	/*! event sent when we need the decoding statistics*/
	GF_DASH_EVENT_CODEC_STAT_QUERY,
	/*! event sent when no threading to trigger segment download abort*/
	GF_DASH_EVENT_ABORT_DOWNLOAD,
	/*! event sent whenever cache is full, to allow client to dispatch any segment*/
	GF_DASH_EVENT_CACHE_FULL,
	/*! event sent when all groups are done in a period - if group_idx is 1, this announces a time discontinuity for next period*/
	GF_DASH_EVENT_END_OF_PERIOD,
} GF_DASHEventType;

/*! DASH network I/O abstraction object*/
typedef struct _gf_dash_io GF_DASHFileIO;
/*! structure used for all IO sessions for DASH*/
typedef void *GF_DASHFileIOSession;

/*! DASH network I/O abstraction object*/
struct _gf_dash_io
{
	/*! user private data*/
	void *udta;

	/*! signals errors or specific actions to perform*/
	GF_Err (*on_dash_event)(GF_DASHFileIO *dashio, GF_DASHEventType evt, s32 group_idx, GF_Err setup_error);

	/*! used to check whether a representation is supported or not. Function returns 1 if supported, 0 otherwise
	if this callback is not set, the representation is assumed to be supported*/
	Bool (*dash_codec_supported)(GF_DASHFileIO *dashio, const char *codec, u32 width, u32 height, Bool is_interlaced, u32 fps_num, u32 fps_denum, u32 sample_rate);

	/*! called whenever a file has to be deleted*/
	void (*delete_cache_file)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session, const char *cache_url);

	/*! create a file download session for the given resource - group_idx may be -1 if this is a global resource , otherwise it indicates the group/adaptationSet in which the download happens*/
	GF_DASHFileIOSession (*create)(GF_DASHFileIO *dashio, Bool persistent, const char *url, s32 group_idx);
	/*! delete a file download session*/
	void (*del)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session);
	/*! aborts downloading in the given file session*/
	void (*abort)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session);
	/*! resetup the file session with a new resource to get - this allows persistent connection usage with HTTP servers*/
	GF_Err (*setup_from_url)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session, const char *url, s32 group_idx);
	/*! set download range for the file session*/
	GF_Err (*set_range)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session, u64 start_range, u64 end_range, Bool discontinue_cache);
	/*! initialize the file session - all the headers shall be fetched before returning*/
	GF_Err (*init)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session);
	/*! download the content - synchronous call: all the file shall be fetched before returning*/
	GF_Err (*run)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session);

	/*! get URL of the file - it may be different from the original one if resource relocation happened*/
	const char *(*get_url)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session);
	/*! get the name of the cache file. If NULL is returned, the file cannot be cached and its associated URL will be used when
	the client request file to play*/
	const char *(*get_cache_name)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session);
	/*! get the MIME type of the file*/
	const char *(*get_mime)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session);
	/*! get the given hedaer value in the last HTTP response. Function is optional*/
	const char *(*get_header_value)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session, const char *header_name);
	/*! gets the UTC time at which reply has been received. Function is optional*/
	u64 (*get_utc_start_time)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session);
	/*! get the average download rate for the session. If no session is specified, gets the max download rate
	for the client (used for bandwidth simulation in local files)*/
	u32 (*get_bytes_per_sec)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session);
	/*! get the total size on bytes for the session*/
	u32 (*get_total_size)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session);
	/*! get the total size on bytes for the session*/
	u32 (*get_bytes_done)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session);
	/*! get the status of the session - GF_OK for done, GF_NOT_READY for in progress, other error code indicate download error*/
	GF_Err (*get_status)(GF_DASHFileIO *dashio, GF_DASHFileIOSession session);

	/*! callback when manifest (DASH, HLS) or sub-playlist (HLS) is updated*/
	void (*manifest_updated)(GF_DASHFileIO *dashio, const char *manifest_name, const char *local_path, s32 group_idx);

};

/*! DASH client object*/
typedef struct __dash_client GF_DashClient;

/*! Quality selection mode of initial segments*/
typedef enum
{
	/*! selects the lowest quality when starting - if one of the representation does not have video (HLS), it may be selected*/
	GF_DASH_SELECT_QUALITY_LOWEST=0,
	/*! selects the highest quality when starting*/
	GF_DASH_SELECT_QUALITY_HIGHEST,
	/*! selects the lowest bandwidth when starting - if one of the representation does not have video (HLS), it will NOT be selected*/
	GF_DASH_SELECT_BANDWIDTH_LOWEST,
	/*! selects the highest bandwidth when starting - for tiles all low priority tiles will have the lower (below max) bandwidth selected*/
	GF_DASH_SELECT_BANDWIDTH_HIGHEST,
	/*! selects the highest bandwidth when starting - for tiles all low priority tiles will have their lowest bandwidth selected*/
	GF_DASH_SELECT_BANDWIDTH_HIGHEST_TILES
} GF_DASHInitialSelectionMode;


/*! create a new DASH client
\param dash_io DASH callbacks to the user
\param max_cache_duration maximum duration in milliseconds for the cached media. If less than \code mpd@minBufferTime \endcode , \code mpd@minBufferTime \endcode  is used
\param auto_switch_count forces representation switching (quality up if positive, down if negative) every auto_switch_count segments, set to 0 to disable
\param keep_files do not delete files from the cache
\param disable_switching turn off bandwidth switching algorithm
\param first_select_mode indicates which representation to select upon startup
\param initial_time_shift_value sets initial buffering: if between 0 and 100, this is a percentage of the time shift window of the session. If greater than 100, this is a time shift in milliseconds.
\return a new DASH client
*/
GF_DashClient *gf_dash_new(GF_DASHFileIO *dash_io,
                           u32 max_cache_duration,
                           s32 auto_switch_count,
                           Bool keep_files,
                           Bool disable_switching,
                           GF_DASHInitialSelectionMode first_select_mode,
                           u32 initial_time_shift_value);

/*! destroys a DASH client
\param dash the target dash clien
*/
void gf_dash_del(GF_DashClient *dash);

/*! opens the DASH client for the specific manifest file
\param dash the target dash client
\param manifest_url the URL of the manifest to open
\return error if any
*/
GF_Err gf_dash_open(GF_DashClient *dash, const char *manifest_url);

/*! closes the dash client
\param dash the target dash client
*/
void gf_dash_close(GF_DashClient *dash);

/*! for unthreaded session, executes DASH logic
\param dash the target dash client
\return error if any, GF_EOS on end of session*/
GF_Err gf_dash_process(GF_DashClient *dash);

/*! returns URL of the DASH manifest file
\param dash the target dash client
\return the currently loaded manifest
*/
const char *gf_dash_get_url(GF_DashClient *dash);

/*! tells whether we are playing some Apple HLS M3U8
\param dash the target dash client
\return GF_TRUE if the manifest is M3U8
*/
Bool gf_dash_is_m3u8(GF_DashClient *dash);

/*! tells whether we are playing some MS SmoothStreaming
\param dash the target dash client
\return GF_TRUE if the manifest is SmoothStreaming
*/
Bool gf_dash_is_smooth_streaming(GF_DashClient *dash);

/*! gets title and source for this MPD
\param dash the target dash client
\param title set to the title of the manifest (may be NULL)
\param source set to the source of the manifest (may be NULL)
*/
void gf_dash_get_info(GF_DashClient *dash, const char **title, const char **source);

/*! switches quality up or down
\param dash the target dash client
\param switch_up indicates if the quality should be increased (GF_TRUE) or decreased (GF_FALSE)
*/
void gf_dash_switch_quality(GF_DashClient *dash, Bool switch_up);

/*! indicates whether the DASH client is running or not
\param dash the target dash client
\return GF_TRUE if the session is still active, GF_FALSE otherwise
*/
Bool gf_dash_is_running(GF_DashClient *dash);

/*! indicates whether the DASH client is in setup stage (sloving periods, destroying or creating playback chain) or not
\param dash the target dash client
\return GF_TRUE if the session is in setup, GF_FALSE otherwise
*/
Bool gf_dash_is_in_setup(GF_DashClient *dash);

/*! gets duration of the presentation
\param dash the target dash client
\return the duration in second of the session
*/
Double gf_dash_get_duration(GF_DashClient *dash);

/*! sets timeshift for the presentation - this function does not trigger a seek, this has to be done by the caller
\param dash the target dash client
\param ms_in_timeshift if between 0 and 100, this is a percentage of the time shift window of the session. If greater than 100, this is a time shift in milliseconds
\return error if any
*/
GF_Err gf_dash_set_timeshift(GF_DashClient *dash, u32 ms_in_timeshift);

/*! returns the number of groups. A group is a set of media resources that are alternate of each other in terms of bandwidth/quality
\param dash the target dash client
\return the number of groups in the active session
*/
u32 gf_dash_get_group_count(GF_DashClient *dash);

/*! associates user data (or NULL) with a given group
\param dash the target dash client
\param group_index the 0-based index of the target group
\param udta the opaque data to associate to the group
\return error if any
*/
GF_Err gf_dash_set_group_udta(GF_DashClient *dash, u32 group_index, void *udta);

/*! returns the user data associated with this group
\param dash the target dash client
\param group_index the 0-based index of the target group
\return the opaque data to associate to the group
*/
void *gf_dash_get_group_udta(GF_DashClient *dash, u32 group_index);

/*! indicates whether a group is selected for playback or not. Currently groups cannot be selected during playback
\param dash the target dash client
\param group_index the 0-based index of the target group
\return GF_TRUE if the group is selected for playback/download
*/
Bool gf_dash_is_group_selected(GF_DashClient *dash, u32 group_index);

/*! indicates whether this group is dependent on another group (because representations are). If this is the case, all representations of this group will be made available through the base group (and has_next_segment flag) if the group is selected.
returns -1 if not dependent on another group, otherwise return dependent group index
\param dash the target dash client
\param group_idx the 0-based index of the target group
\return -1 if the group does not depend on another group, otherwise returns the dependent group index
*/
s32 gf_dash_group_has_dependent_group(GF_DashClient *dash, u32 group_idx);

/*! gives the number of groups depending on this one for decoding
\param dash the target dash client
\param group_idx the 0-based index of the target group
\return the number of other groups depending on this group
*/
u32 gf_dash_group_get_num_groups_depending_on(GF_DashClient *dash, u32 group_idx);

/*! gets the index of the depending_on group with the specified group_depending_on_dep_idx (between 0 and \ref gf_dash_group_get_num_groups_depending_on -1)
\param dash the target dash client
\param group_idx the 0-based index of the target group
\param group_depending_on_dep_idx the 0-based index of the queried dependent group
\return the index of the depending_on group
*/
s32 gf_dash_get_dependent_group_index(GF_DashClient *dash, u32 group_idx, u32 group_depending_on_dep_idx);

/*! indicates whether a group can be selected for playback or not. Some groups may have been disabled because of non supported features
\param dash the target dash client
\param group_idx the 0-based index of the target group
\return GF_TRUE if the group can be selected for playback, GF_FALSE otherwise
*/
Bool gf_dash_is_group_selectable(GF_DashClient *dash, u32 group_idx);

/*! selects a group for playback. If group selection is enabled,  other groups are alternate to this group (through the group attribute), they are automatically deselected
\param dash the target dash client
\param group_idx the 0-based index of the target group
\param select if GF_TRUE, will select this group and disable any alternate group. If GF_FALSE, only deselects the group
*/
void gf_dash_group_select(GF_DashClient *dash, u32 group_idx, Bool select);

/*! gets group ID (through the group attribute), -1 if undefined
\param dash the target dash client
\param group_idx the 0-based index of the target group
\return ID of the group
*/
s32 gf_dash_group_get_id(GF_DashClient *dash, u32 group_idx);

/*! gets cuirrent period ID
\param dash the target dash client
\return ID of the current period or NULL
*/
const char*gf_dash_get_period_id(GF_DashClient *dash);

/*! enables group selection  through the group attribute
\param dash the target dash client
\param enable if GF_TRUE, group selection will be done whenever selecting a new group
*/
void gf_dash_enable_group_selection(GF_DashClient *dash, Bool enable);

/*! checks if first segment (used to initialize) was an init segment or the first in a sequence (aka M2TS)
\param dash the target dash client
\param group_idx the 0-based index of the target group
\return GF_TRUE if first segment was a media segment
*/
Bool gf_dash_group_init_segment_is_media(GF_DashClient *dash, u32 group_idx);

/*! performs selection of representations based on language code
\param dash the target dash client
\param lang_code_rfc_5646 the language code used by the default group selection
*/
void gf_dash_groups_set_language(GF_DashClient *dash, const char *lang_code_rfc_5646);

/*! returns the mime type of the media resources in this group
\param dash the target dash client
\param group_idx the 0-based index of the target group
\return the mime type of the segments in this group
*/
const char *gf_dash_group_get_segment_mime(GF_DashClient *dash, u32 group_idx);

/*! returns the URL of the first media resource to play (init segment or first media segment depending on format). start_range and end_range are optional
\param dash the target dash client
\param group_idx the 0-based index of the target group
\param start_range set to the byte start offset in the init segment
\param end_range set to the byte end offset in the init segment
\return URL of the init segment (can be a relative path if manifest is a local file)
*/
const char *gf_dash_group_get_segment_init_url(GF_DashClient *dash, u32 group_idx, u64 *start_range, u64 *end_range);

/*! returns the URL and IV associated with the first media segment if any (init segment or first media segment depending on format).
This is used for full segment encryption modes of MPEG-2 TS segments. key_IV is optional
\param dash the target dash client
\param group_idx the 0-based index of the target group
\param crypto_type set to 0 if no encryption in segments, 1 if full segment encryption, 2 if CENC/per-sample encryption is used -  may be NULL
\param key_IV set to the IV used for the first media segment (can be NULL)
\return the key URL of the first media segment, either a URN or a resolved URL
*/
const char *gf_dash_group_get_segment_init_keys(GF_DashClient *dash, u32 group_idx, u32 *crypto_type, bin128 *key_IV);

/*! returns the language of the group
\param dash the target dash client
\param group_idx the 0-based index of the target group
\return the language of the group or NULL if no language associated
*/
const char *gf_dash_group_get_language(GF_DashClient *dash, u32 group_idx);

/*! returns the number of audio channelsof the group
\param dash the target dash client
\param group_idx the 0-based index of the target group
\return the number of audio channels, or 0 if not audio or unspecified
*/
u32 gf_dash_group_get_audio_channels(GF_DashClient *dash, u32 group_idx);

/*! gets time shift buffer depth of the group
\param dash the target dash client
\param group_idx the 0-based index of the target group
\return time shift buffer depth in ms of the group, -1 means infinity
*/
u32 gf_dash_group_get_time_shift_buffer_depth(GF_DashClient *dash, u32 group_idx);

/*! gets current time in time shift buffer
This functions retrieves the maximum value (further in the past) of all selected/active groups
\param dash the target dash client
\return current time in timeshift buffer in seconds - 0 means 'live point'
*/
Double gf_dash_get_timeshift_buffer_pos(GF_DashClient *dash);

/*! sets codec statistics of a group for playback rate adjustment
\param dash the target dash client
\param group_idx the 0-based index of the target group
\param avg_dec_time average decode time in microseconds of a frame, 0 if unknown
\param max_dec_time maximum decode time in microseconds of a frame, 0 if unknown
\param irap_avg_dec_time average decode time of SAP 1/2/3 pictures in microseconds of a frame, 0 if unknown
\param irap_max_dec_time maximum decode time of SAP 1/2/3 pictures in microseconds of a frame, 0 if unknown
\param codec_reset indicates a codec reset is pending (pipeline flush is needed before reinit)
\param decode_only_rap indicates only SAP1/2/3 are currently being decoded. This may trigger a switch to a trick mode representation
*/
void gf_dash_group_set_codec_stat(GF_DashClient *dash, u32 group_idx, u32 avg_dec_time, u32 max_dec_time, u32 irap_avg_dec_time, u32 irap_max_dec_time, Bool codec_reset, Bool decode_only_rap);

/*! sets buffer levels of a group
\param dash the target dash client
\param group_idx the 0-based index of the target group
\param buffer_min_ms minimum buffer in milliseconds (pipeline would rebuffer below this level)
\param buffer_max_ms maximum buffer in milliseconds (pipeline would block above this level)
\param buffer_occupancy_ms current buffer occupancy in milliseconds
*/
void gf_dash_group_set_buffer_levels(GF_DashClient *dash, u32 group_idx, u32 buffer_min_ms, u32 buffer_max_ms, u32 buffer_occupancy_ms);

/*! indicates the buffer time in ms after which the player resumes playback. This value is less or equal to the buffer_max_ms
indicated in \ref gf_dash_group_set_buffer_levels
\param dash the target dash client
\param group_idx the 0-based index of the target group
\param max_target_buffer_ms buffer in milliseconds above which pipeline resumes after a rebuffering event
\return error if any
*/
GF_Err gf_dash_group_set_max_buffer_playout(GF_DashClient *dash, u32 group_idx, u32 max_target_buffer_ms);

/*! DASH descriptor types*/
typedef enum
{
	/*! Accessibility descriptor*/
	GF_MPD_DESC_ACCESSIBILITY,
	/*! Audio config descriptor*/
	GF_MPD_DESC_AUDIOCONFIG,
	/*! Content Protection descriptor*/
	GF_MPD_DESC_CONTENT_PROTECTION,
	/*! Essential Property descriptor*/
	GF_MPD_DESC_ESSENTIAL_PROPERTIES,
	/*! Supplemental Property descriptor*/
	GF_MPD_DESC_SUPPLEMENTAL_PROPERTIES,
	/*! Frame packing descriptor*/
	GF_MPD_DESC_FRAME_PACKING,
	/*! Role descriptor*/
	GF_MPD_DESC_ROLE,
	/*! Rating descriptor*/
	GF_MPD_DESC_RATING,
	/*! Viewpoint descriptor*/
	GF_MPD_DESC_VIEWPOINT
} GF_DashDescriptorType;

/*! enumerates descriptors of the given type
\param dash the target dash client
\param group_idx the 0-based index of the target group
\param desc_type type of descriptor being checked
\param role_idx index of the descriptor being checked for this type
\param desc_id set to the ID of the descriptor if found (optional, may be NULL)
\param desc_scheme set to the scheme of the descriptor if found (optional, may be NULL)
\param desc_value set to the desc_value of the descriptor if found (optional, may be NULL)
\return GF_TRUE if descriptor is found, GF_FALSE otherwise
*/
Bool gf_dash_group_enum_descriptor(GF_DashClient *dash, u32 group_idx, GF_DashDescriptorType desc_type, u32 role_idx, const char **desc_id, const char **desc_scheme, const char **desc_value);

/*! returns the URL and byte range of the next media resource to play in this group.
\param dash the target dash client
\param group_idx the 0-based index of the target group
\param dependent_representation_index index of the dependent representation to query, 0-based
\param url set to the URL of the next segment
\param start_range set to the start byte offset in the segment (optional, may be NULL)
\param end_range set to the end byte offset in the segment (optional, may be NULL)
\param switching_index set to the quality index of the segment (optional, may be NULL)
\param switching_url set to the URL of the switching segment if needed (optional, may be NULL)
\param switching_start_range set to start byte offset of the switching segment if needed (optional, may be NULL)
\param switching_end_range set to end byte offset of the switching segment if needed (optional, may be NULL)
\param original_url set to original URL value of the segment (optional, may be NULL)
\param has_next_segment set to GF_TRUE if next segment location is known (unthreaded mode) or next segment is downloaded (threaded mode) (optional, may be NULL)
\param key_url set to the key URL of the next segment for MPEG-2 TS full segment encryption (optional, may be NULL). The URL is either a URN or a resolved URL
\param key_IV set to the key initialization vector of the next segment for MPEG-2 TS full segment encryption (optional, may be NULL)
\param utc set to UTC mapping for first sample of segment, 0 if none defined
\return GF_BUFFER_TOO_SMALL if no segment found, GF_EOS if end of session, GF_URL_REMOVED if segment is disabled (but all output info is OK, this can be ignored and considered as GF_OK by the user) or error if any
*/
GF_Err gf_dash_group_get_next_segment_location(GF_DashClient *dash, u32 group_idx, u32 dependent_representation_index, const char **url, u64 *start_range, u64 *end_range,
        s32 *switching_index, const char **switching_url, u64 *switching_start_range, u64 *switching_end_range,
        const char **original_url, Bool *has_next_segment, const char **key_url, bin128 *key_IV, u64 *utc);

/*! gets some info on the segment
\param dash the target dash client
\param group_idx the 0-based index of the target group
\param dependent_representation_index index of the dependent representation to query, 0-based
\param seg_name  set to the segment name, without base url - optional, may be NULL
\param seg_number  set to the segment number for $Number$ addressing - optional, may be NULL
\param seg_time  set to the segment start time  - optional, may be NULL
\param seg_dur_ms  set to the segment estimated duration in ms  - optional, may be NULL
\param init_segment set to the init segment name, without base url  - optional, may be NULL
\return error if any, GF_BUFFER_TOO_SMALL if no segments queued for download
*/
GF_Err gf_dash_group_next_seg_info(GF_DashClient *dash, u32 group_idx, u32 dependent_representation_index, const char **seg_name, u32 *seg_number, GF_Fraction64 *seg_time, u32 *seg_dur_ms, const char **init_segment);

/*! checks if loop was detected in playback. This is mostly used for broadcast (eMBMS, ROUTE) based on pcap replay.
\param dash the target dash client
\param group_idx the 0-based index of the target group
\return  GF_TRUE if segment numbers loop was detected
*/
Bool gf_dash_group_loop_detected(GF_DashClient *dash, u32 group_idx);

/*! checks if group is using low latency delivery.
\param dash the target dash client
\param group_idx the 0-based index of the target group
\return GF_TRUE if low latency is used, GF_FALSE otherwise
*/
Bool gf_dash_is_low_latency(GF_DashClient *dash, u32 group_idx);

/*! gets average duration of segments for the current rep.
\param dash the target dash client
\param group_idx the 0-based index of the target group
\param duration set to average segment duration
\param timescale set to timescale used to exprss duration
\return error if any
*/
GF_Err gf_dash_group_get_segment_duration(GF_DashClient *dash, u32 group_idx, u32 *duration, u32 *timescale);

/*! gets ID of active representaion.
\param dash the target dash client
\param group_idx the 0-based index of the target group
\return ID of representation, NULL if error
*/
const char *gf_dash_group_get_representation_id(GF_DashClient *dash, u32 group_idx);

/*! returns number of seconds at which playback shall start for the group in the current period.
The first segment available for the period will be so that gf_dash_group_get_start_range is in this range after the caller
adjusts it with PTO (eg the returned time is in period timeline, not media timeline)
\param dash the target dash client
\param group_idx the 0-based index of the target group
\return media playback start range in seconds*/
Double gf_dash_group_get_start_range(GF_DashClient *dash, u32 group_idx);

/*! discards the first media resource in the queue of this group
\param dash the target dash client
\param group_idx the 0-based index of the target group
*/
void gf_dash_group_discard_segment(GF_DashClient *dash, u32 group_idx);

/*! indicates to the DASH engine that the group playback has been stopped by the user
\param dash the target dash client
\param group_idx the 0-based index of the target group
\param done mark group as done if GF_TRUE, or not done if GF_FALSE
*/
void gf_dash_set_group_done(GF_DashClient *dash, u32 group_idx, Bool done);

/*! gets presentationTimeOffset and timescale for the active representation
\param dash the target dash client
\param group_idx the 0-based index of the target group
\param presentation_time_offset set to the presentation time offset for the group
\param timescale set to the timescale used to represent the presentation time offset
\return error if any
*/
GF_Err gf_dash_group_get_presentation_time_offset(GF_DashClient *dash, u32 group_idx, u64 *presentation_time_offset, u32 *timescale);

/*! checks if the session is in the last period
\param dash the target dash client
\param check_eos if GF_TRUE, return GF_TRUE only if the last period is known to be the last one (not an open period in live)
\return GF_TRUE if the playback position is in the last period of the presentation
*/
Bool gf_dash_in_last_period(GF_DashClient *dash, Bool check_eos);

/*! checks if the group is playing
\param dash the target dash client
\param group_idx the 0-based index of the target group
\return GF_TRUE if group is done playing
*/
Bool gf_dash_get_group_done(GF_DashClient *dash, u32 group_idx);

/*! gets current period switching status for the session.
\param dash the target dash client
\return possible values:
	1 if the period switching has been requested (due to seeking),
	2 if the switching is in progress (all groups will soon be destroyed and plyback will be stopped and restarted)
	0 if no switching is requested
*/
u32 gf_dash_get_period_switch_status(GF_DashClient *dash);

/*! request period switch - this is typically called when the media engine signals that no more data is available for playback
\param dash the target dash client
*/
void gf_dash_request_period_switch(GF_DashClient *dash);

/*! checks if the client is in a period setup state
\param dash the target dash client
\return GF_TRUE if the DASH engine is currently setting up a period (creating groups and fetching initial segments)
*/
Bool gf_dash_in_period_setup(GF_DashClient *dash);

/*! seeks playback to the given time. If period changes, all playback is stopped and restarted
If the session is dynamic (live), the start_range is ignored and recomputed from current UTC clock to be at the live point. If timeshifting is desired, use \ref gf_dash_set_timeshift before seeking
\param dash the target dash client
\param start_range the desired seek time in seconds
*/
void gf_dash_seek(GF_DashClient *dash, Double start_range);

/*! checks if seeking time is in the previously playing segment
\param dash the target dash client
\param group_idx the 0-based index of the target group
\return GF_TRUE if the seek request was in a different segment than the previously playing one
*/
Bool gf_dash_group_segment_switch_forced(GF_DashClient *dash, u32 group_idx);
/*! gets video info for this group if video
\param dash the target dash client
\param group_idx the 0-based index of the target group
\param max_width set to the maximum width in the group
\param max_height set to the maximum height in the group
\return error if any
*/
GF_Err gf_dash_group_get_video_info(GF_DashClient *dash, u32 group_idx, u32 *max_width, u32 *max_height);

/*! seeks only a given group - results are undefined if the seek operation triggers a period switch
\param dash the target dash client
\param group_idx the 0-based index of the target group
\param seek_to the desired seek time in seconds
*/
void gf_dash_group_seek(GF_DashClient *dash, u32 group_idx, Double seek_to);

/*! sets playback speed of the session. Speed is used in adaptation logic
\param dash the target dash client
\param speed current playback speed
*/
void gf_dash_set_speed(GF_DashClient *dash, Double speed);

/*! updates media bandwidth for the given group. Only allowed for groups without dependencies to other groups
\param dash the target dash client
\param group_idx the 0-based index of the target group
\param bits_per_sec current download rate in bits per seconds
\param total_bytes total size of segment being downloaded
\param bytes_done number of bytes already downloaded in current segment
\param us_since_start time elapsed in microseconds since  segment has been scheduled for download
\return error if any
*/
GF_Err gf_dash_group_check_bandwidth(GF_DashClient *dash, u32 group_idx, u32 bits_per_sec, u64 total_bytes, u64 bytes_done, u64 us_since_start);

/*! enables UTC drift computation using HTTP header "Server-UTC: UTC", where UTC is in ms
\param dash the target dash client
\param estimate_utc_drift if GF_TRUE, enables UTC drift compensation, otherwise disables it
*/
void gf_dash_enable_utc_drift_compensation(GF_DashClient *dash, Bool estimate_utc_drift);

/*! checks if session is dynamic offering (live)
\param dash the target dash client
\return GF_TRUE if MPD is dynamic, GF_FALSE otherwise
*/
Bool gf_dash_is_dynamic_mpd(GF_DashClient *dash);

/*! gets minimum buffer time of session indicated in MPD
\param dash the target dash client
\return minimum buffer time in ms
*/
u32 gf_dash_get_min_buffer_time(GF_DashClient *dash);

/*! gets the maximum segment duration in session
\param dash the target dash client
\return the maximum segment duration in ms
*/
u32 gf_dash_get_max_segment_duration(GF_DashClient *dash);

/*! gets the difference between the local UTC clock and the one reported by the server
\param dash the target dash client
\return difference in milliseconds
*/
s32 gf_dash_get_utc_drift_estimate(GF_DashClient *dash);

/*! shifts UTC clock of server by shift_utc_ms so that new UTC in MPD is old + shift_utc_ms
\param dash the target dash client
\param shift_utc_ms UTC clock shift in milliseconds. A positive value will move the clock in the future, a negative value in the past
*/
void gf_dash_set_utc_shift(GF_DashClient *dash, s32 shift_utc_ms);

/*! sets max video display capabilities
\param dash the target dash client
\param width the maximum width of the display
\param height the maximum height of the display
\param max_display_bpp the maximum bits per pixel of the display
\return error if any
*/
GF_Err gf_dash_set_max_resolution(GF_DashClient *dash, u32 width, u32 height, u8 max_display_bpp);

/*! sets min time in ms between a 404 and the next request on the same group. The default value is 500 ms.
\param dash the target dash client
\param min_timeout_between_404 minimum delay in milliseconds between retries
\return error if any
*/
GF_Err gf_dash_set_min_timeout_between_404(GF_DashClient *dash, u32 min_timeout_between_404);

/*! sets time in ms after which 404 request for a segment will indicate segment lost. The client always retries for segment availability time + segment duration. This allows extending slightly the probe time (used when segment durations varies, or for VBR broadcast). The default value is 100 ms.
\param dash the target dash client
\param expire_after_ms delay in milliseconds
\return error if any
*/
GF_Err gf_dash_set_segment_expiration_threshold(GF_DashClient *dash, u32 expire_after_ms);

/*! only enables the given groups - this shall be set before calling \ref gf_dash_open. If NULL, no groups will be disabled
\param dash the target dash client
\param groups_idx list of  0-based index of the target groups to enable,
\param nb_groups number of group indexes in list
*/
void gf_dash_debug_groups(GF_DashClient *dash, const u32 *groups_idx, u32 nb_groups);

/*! split all adatation sets so that they contain only one representation (quality)
\param dash the target dash client
*/
void gf_dash_split_adaptation_sets(GF_DashClient *dash);

/*! low latency mode of dash client*/
typedef enum
{
	/*! disable low latency*/
	GF_DASH_LL_DISABLE = 0,
	/*! strict respect of segment availability start time*/
	GF_DASH_LL_STRICT,
	/*! allow fetching segments earlier than their availability start time in case of empty demux*/
	GF_DASH_LL_EARLY_FETCH,
} GF_DASHLowLatencyMode;

/*! allow early segment fetch in low latency mode
\param dash the target dash client
\param low_lat_mode low latency mode
*/
void gf_dash_set_low_latency_mode(GF_DashClient *dash, GF_DASHLowLatencyMode low_lat_mode);

/*! indicates typical buffering used by the user app before playback starts.
 This allows fetching data earlier in live mode, if the timeshiftbuffer allows for it
\param dash the target dash client
\param buffer_time_ms typical playout buffer in milliseconds
*/
void gf_dash_set_user_buffer(GF_DashClient *dash, u32 buffer_time_ms);

/*! indicates the number of segments to wait before switching up bandwidth. The default value is 1 (ie stay in current bandwidth or one more segment before switching up, event if download rate is enough).
Setting this to 0 means the switch will happen instantly, but this is more prone to quality changes due to network variations
\param dash the target dash client
\param switch_probe_count the number of probes before switching
*/
void gf_dash_set_switching_probe_count(GF_DashClient *dash, u32 switch_probe_count);

/*! enables agressive switching mode. If agressive switching is enabled, switching targets to the closest bandwidth fitting the available download rate. Otherwise, switching targets the lowest bitrate representation that is above the currently played (eg does not try to switch to max bandwidth). Default value is no.
\param dash the target dash client
\param enable_agressive_switch if GF_TRUE, enables agressive mode, otherwise disables it
*/
void gf_dash_set_agressive_adaptation(GF_DashClient *dash, Bool enable_agressive_switch);

/*! enables single-range requests for LL-HLS byterange, rather than issuing a request per PART. This assumes that:
 -  each URI in the different parts is the SAME
 -  byte ranges are contiguous in the URL

Errors will be thrown if these are not met on future parts and merging will be disabled, however the scheduled buggy segment will NOT be disarded

\param dash the target dash client
\param enable_single_range if GF_TRUE, enables single range, otherwise disables it
*/
void gf_dash_enable_single_range_llhls(GF_DashClient *dash, Bool enable_single_range);

/*! create a new DASH client
\param dash the target dash cleint
\param auto_switch_count forces representation switching (quality up if positive, down if negative) every auto_switch_count segments, set to 0 to disable
\param auto_switch_loop if false (default when creating dasher), restart at lowest quality when higher quality is reached and vice-versa. If true, quality switches decreases then increase in loop
*/
void gf_dash_set_auto_switch(GF_DashClient *dash, s32 auto_switch_count, Bool auto_switch_loop);

/*! returns active period start
\param dash the target dash client
\return period start in milliseconds
*/
u64 gf_dash_get_period_start(GF_DashClient *dash);

/*! gets active period duration
\param dash the target dash client
\return active period duration in milliseconds
*/
u64 gf_dash_get_period_duration(GF_DashClient *dash);

/*! gets number of quality available for a group
\param dash the target dash client
\param group_idx the 0-based index of the target group
\return number of quality available
*/
u32 gf_dash_group_get_num_qualities(GF_DashClient *dash, u32 group_idx);

/*! gets the number of components in a group
\param dash the target dash client
\param group_idx the 0-based index of the target group
\return number of components
*/
u32 gf_dash_group_get_num_components(GF_DashClient *dash, u32 group_idx);

/*! disable speed adaptation
\param dash the target dash client
\param disable if GF_TRUE? speed adaptation is disabled, otherwise it is enabled
*/
void gf_dash_disable_speed_adaptation(GF_DashClient *dash, Bool disable);

/*! DASH/HLS quality information structure*/
//UPDATE DASHQualityInfoNat in libgpac.py whenever modifying this structure !!
typedef struct
{
	/*! bandwidth in bits per second*/
	u32 bandwidth;
	/*! ID*/
	const char *ID;
	/*! mime type*/
	const char *mime;
	/*! codec parameter of mime type*/
	const char *codec;
	/*! video width*/
	u32 width;
	/*! video width*/
	u32 height;
	/*! video interlaced flag*/
	Bool interlaced;
	/*! video framerate numerator*/
	u32 fps_num;
	/*! video framerate denominator*/
	u32 fps_den;
	/*! video sample aspect ratio numerator*/
	u32 par_num;
	/*! video sample aspect ratio denominator*/
	u32 par_den;
	/*! audio sample rate*/
	u32 sample_rate;
	/*! audio channel count*/
	u32 nb_channels;
	/*! disabled flag (not supported by DASH client)*/
	Bool disabled;
	/*! selected flag*/
	Bool is_selected;
	/*! AST offset in seconds, 0 if not low latency*/
	Double ast_offset;
	/*! average segment duration, 0 if unknown*/
	Double average_duration;
	/*! list of segmentURLs if known, NULL otherwise. Used for onDemand profile to get segment sizes*/
	const GF_List *seg_urls;
} GF_DASHQualityInfo;

/*! gets information on  a given quality
\param dash the target dash client
\param group_idx the 0-based index of the target group
\param quality_idx the 0-based index of the quality
\param quality filled with information for the desired quality
\return error if any
*/
GF_Err gf_dash_group_get_quality_info(GF_DashClient *dash, u32 group_idx, u32 quality_idx, GF_DASHQualityInfo *quality);

/*! gets segment template info used by group
\param dash the target dash client
\param group_idx the 0-based index of the target group
\param segment_timeline_timescale set to segment timeline timescale, or to 0 if no segment timeline
\param init_url set to initialization URL (template, timeline or base URL for VoD) as indicated in manifest (no resolution to base URL)
\param hls_variant set toHLS variant name or NULL
\return segment template, NULL if no templates used. Memory must be freed by caller
*/
char *gf_dash_group_get_template(GF_DashClient *dash, u32 group_idx, u32 *segment_timeline_timescale, const char **init_url, const char **hls_variant);

/*! checks automatic switching mode
\param dash the target dash client
\return GF_TRUE if automatic quality switching is enabled
*/
Bool gf_dash_get_automatic_switching(GF_DashClient *dash);

/*! sets automatic quality switching mode. If automatic switching is off, switching can only happen based on caller inputs
\param dash the target dash client
\param enable_switching if GF_TRUE, automatic switching is enabled; otherwise it is disabled
\return error if any
*/
GF_Err gf_dash_set_automatic_switching(GF_DashClient *dash, Bool enable_switching);

/*! selects quality of a group, either by ID or by index if ID is null
\param dash the target dash client
\param group_idx the 0-based index of the target group
\param ID the ID of the desired quality
\param q_idx the 0-based index of the desired quality
\return error if any
*/
GF_Err gf_dash_group_select_quality(GF_DashClient *dash, u32 group_idx, const char *ID, u32 q_idx);

/*! gets currently active quality of a group
\param dash the target dash client
\param group_idx the 0-based index of the target group
\return the current quality index for the given group*/
s32 gf_dash_group_get_active_quality(GF_DashClient *dash, u32 group_idx);

/*! forces NTP of the DASH client to be the given NTP
\param dash the target dash client
\param server_ntp NTP timestamp to set as server clock
*/
void gf_dash_override_ntp(GF_DashClient *dash, u64 server_ntp);

/*! Tile adaptation mode
This mode specifies how bitrate is allocated across tiles of the same video
*/
typedef enum
{
	/*! each tile receives the same amount of bitrate (default strategy)*/
	GF_DASH_ADAPT_TILE_NONE=0,
	/*! bitrate decreases for each row of tiles starting from the top, same rate for each tile on the row*/
	GF_DASH_ADAPT_TILE_ROWS,
	/*! bitrate decreases for each row of tiles starting from the bottom, same rate for each tile on the row*/
	GF_DASH_ADAPT_TILE_ROWS_REVERSE,
	/*! bitrate decreased for top and bottom rows only, same rate for each tile on the row*/
	GF_DASH_ADAPT_TILE_ROWS_MIDDLE,
	/*! bitrate decreases for each column of tiles starting from the left, same rate for each tile on the column*/
	GF_DASH_ADAPT_TILE_COLUMNS,
	/*! bitrate decreases for each column of tiles starting from the right, same rate for each tile on the column*/
	GF_DASH_ADAPT_TILE_COLUMNS_REVERSE,
	/*! bitrate decreased for left and right columns only, same rate for each tile on the column*/
	GF_DASH_ADAPT_TILE_COLUMNS_MIDDLE,
	/*! bitrate decreased for all tiles on the edge of the picture*/
	GF_DASH_ADAPT_TILE_CENTER,
	/*! bitrate decreased for all tiles on the center of the picture*/
	GF_DASH_ADAPT_TILE_EDGES,
} GF_DASHTileAdaptationMode;

/*! sets tile adaptation mode
\param dash the target dash client
\param mode the desired tile adaptation mode
\param tile_rate_decrease percentage (0->100) of global bandwidth to use at each level (recursive rate decrease for all level). If 0% or 100%, automatic rate allocation among tiles is performed (default mode)
*/
void gf_dash_set_tile_adaptation_mode(GF_DashClient *dash, GF_DASHTileAdaptationMode mode, u32 tile_rate_decrease);


/*! consider tile with highest quality degradation hints (not visible ones or not gazed at) as lost, triggering a  GF_URL_REMOVE  upon \ref gf_dash_group_get_next_segment_location calls. Mostly used to debug tiling adaptation
\param dash the target dash client
\param disable_tiles if GF_TRUE, tiles with highest quality degradation hints will not be played.
*/
void gf_dash_disable_low_quality_tiles(GF_DashClient *dash, Bool disable_tiles);

/*! gets current tile adaptation mode
\param dash the target dash client
\return tile adaptation mode*/
GF_DASHTileAdaptationMode gf_dash_get_tile_adaptation_mode(GF_DashClient *dash);

/*! gets max width and height in pixels of the SRD (Spatial Relationship Descriptor) a given group belongs to, if any
\param dash the target dash client
\param group_idx the 0-based index of the target group
\param max_width set to the maximum width of the SRD of this group
\param max_height set to the maximum height of the SRD of this group
\return GF_TRUE if the group has an SRD descriptor associated, GF_FALSE otherwise
*/
Bool gf_dash_group_get_srd_max_size_info(GF_DashClient *dash, u32 group_idx, u32 *max_width, u32 *max_height);

/*! gets SRD info, in SRD coordinate, of the SRD this group belongs to, if any
\param dash the target dash client
\param group_idx the 0-based index of the target group
\param srd_id set to the id of the SRD of this group
\param srd_x set to the horizontal coordinate of the SRD of this group
\param srd_y set to the vertical coordinate of the SRD of this group
\param srd_w set to the width of the SRD of this group
\param srd_h set to the height of the SRD of this group
\param srd_width set to the reference width (usually max width) of the SRD of this group
\param srd_height set to the reference height (usually max height) of the SRD of this group
\return GF_TRUE if the group has an SRD descriptor associated, GF_FALSE otherwise
*/
Bool gf_dash_group_get_srd_info(GF_DashClient *dash, u32 group_idx, u32 *srd_id, u32 *srd_x, u32 *srd_y, u32 *srd_w, u32 *srd_h, u32 *srd_width, u32 *srd_height);

/*! sets quality hint for the given group
\param dash the target dash client
\param group_idx the 0-based index of the target group
\param quality_degradation_hint quality degradation from 0 (no degradation) to 100 (worse quality possible)
\return error if any
*/
GF_Err gf_dash_group_set_quality_degradation_hint(GF_DashClient *dash, u32 group_idx, u32 quality_degradation_hint);

/*! sets visible rectangle of a video object, may be used for adaptation. If min_x==max_x==min_y=max_y==0, disable adaptation
\param dash the target dash client
\param group_idx the 0-based index of the target group
\param min_x horizontal coordinate of first visible column
\param max_x horizontal coordinate of last visible column
\param min_y horizontal coordinate of first visible row
\param max_y horizontal coordinate of last visible row
\param is_gaze if set, {min_x, min_y} indicate the position of the gaze (and {max_x, max_y} are ignored)
\return error if any
*/
GF_Err gf_dash_group_set_visible_rect(GF_DashClient *dash, u32 group_idx, u32 min_x, u32 max_x, u32 min_y, u32 max_y, Bool is_gaze);

/*! Ignores xlink on periods if some adaptation sets are specified in the period with xlink
\param dash the target dash client
\param ignore_xlink if GF_TRUE? xlinks will be ignored on periods containing both xlinks and adaptation sets
*/
void gf_dash_ignore_xlink(GF_DashClient *dash, Bool ignore_xlink);

/*! checks if all groups are done
\param dash the target dash client
\return GF_TRUE if all active groups in period are done
*/
Bool gf_dash_all_groups_done(GF_DashClient *dash);

/*! sets a query string to append to xlink on periods
\param dash the target dash client
\param query_string the query string to append to xlinks on periods
*/
void gf_dash_set_period_xlink_query_string(GF_DashClient *dash, const char *query_string);

/*! sets MPD chaining mode
\param dash the target dash client
\param chaining_mode if 0, no chaining. If 1, chain at end. If 2 chain on error or at end
*/
void gf_dash_set_chaining_mode(GF_DashClient *dash, u32 chaining_mode);


/*! DASH client adaptation algorithm*/
typedef enum {
	/*! no adaptation*/
	GF_DASH_ALGO_NONE = 0,
	/*! GPAC rate-based adaptation*/
	GF_DASH_ALGO_GPAC_LEGACY_RATE,
	/*! GPAC buffer-based adaptation*/
	GF_DASH_ALGO_GPAC_LEGACY_BUFFER,
	/*! BBA-0*/
	GF_DASH_ALGO_BBA0,
	/*! BOLA finite*/
	GF_DASH_ALGO_BOLA_FINITE,
	/*! BOLA basic*/
	GF_DASH_ALGO_BOLA_BASIC,
	/*! BOLA-U*/
	GF_DASH_ALGO_BOLA_U,
	/*! BOLA-O*/
	GF_DASH_ALGO_BOLA_O,
	/*! Custom*/
	GF_DASH_ALGO_CUSTOM
} GF_DASHAdaptationAlgorithm;

/*! sets dash adaptation algorithm. Cannot be called on an active session
\param dash the target dash client
\param algo the algorithm to use
*/
void gf_dash_set_algo(GF_DashClient *dash, GF_DASHAdaptationAlgorithm algo);

/*! sets group download status of the last downloaded segment for non threaded modes
\param dash the target dash client
\param group_idx the 0-based index of the target group
\param dep_rep_idx the 0-based index of the current dependent rep
\param err error status of the download, GF_OK if no error
*/
void gf_dash_set_group_download_state(GF_DashClient *dash, u32 group_idx, u32 dep_rep_idx, GF_Err err);

/*! sets group download statistics of the last downloaded segment for non threaded modes
\param dash the target dash client
\param group_idx the 0-based index of the target group
\param dep_rep_idx the 0-based index of the dependent rep
\param bytes_per_sec transfer rates in bytes per seconds
\param file_size segment size in bytes
\param is_broadcast set to GF_TRUE if the file is received over a multicast/broadcast link such as eMBMS or ROUTE (i.e. file was pushed to cache)
\param us_since_start time in microseconds since start of the download
*/
void gf_dash_group_store_stats(GF_DashClient *dash, u32 group_idx, u32 dep_rep_idx, u32 bytes_per_sec, u64 file_size, Bool is_broadcast, u64 us_since_start);

/*! Shifts the computed availabilityStartTime by offsetting (or resetting) the SuggestedPresentationDelay value parsed in the DASH manifest
\param dash the target dash client
\param spd availabilityStartTime shift in milliseconds. Positive values shift the clock in the future, negative ones in the past, "-I" will force the SuggestedPresentationDelay value to zero
*/
void gf_dash_set_suggested_presentation_delay(GF_DashClient *dash, s32 spd);

/*! sets availabilityStartTime shift for ROUTE. By default the ROUTE tune-in is done by matching the last received segment name
to the segment template and deriving the ROUTE UTC reference from that. The function allows shifting the computed value by a given amount.
\param dash the target dash client
\param ast_shift clock shift in milliseconds of the ROUTE receiver tune-in. Positive values shift the clock in the future, negative ones in the past
*/
void gf_dash_set_route_ast_shift(GF_DashClient *dash, s32 ast_shift);

/*! gets the minimum wait time before calling \ref gf_dash_process again for unthreaded mode
\param dash the target dash client
\return minimum wait time in ms
*/
u32 gf_dash_get_min_wait_ms(GF_DashClient *dash);

/*! gets the adaptation set ID of a given group
\param dash the target dash client
\param group_idx the 0-based index of the target group
\return the adaptation set ID, -1 if not set
*/
s32 gf_dash_group_get_as_id(GF_DashClient *dash, u32 group_idx);

/*! check if the group has an init segment associated
\param dash the target dash client
\param group_idx the 0-based index of the target group
\return GF_TRUE if init segment is present, GF_FALSE otherwise
*/
Bool gf_dash_group_has_init_segment(GF_DashClient *dash, u32 group_idx);

/*! get sample aspect ratio for video group
\param dash the target dash client
\param group_idx the 0-based index of the target group
\param sar filled with representation SAR if any, set to 0 otherwise - may be NULL
*/
void gf_dash_group_get_sar(GF_DashClient *dash, u32 group_idx, GF_Fraction *sar);

//any change to the structure below MUST be reflected in libgpac.py !!

/*! Information passed to DASH custom algorithm*/
typedef struct
{
	/*! last segment download rate in bits per second */
	u32 download_rate;
	/*! size of last downloaded segment*/
	u32 file_size;
	/*! current playback speed*/
	Double speed;
	/*! max supported playback speed according to associated decoder stats*/
	Double max_available_speed;
	/*! display width of the video in pixels, 0 if audio stream*/
	u32 display_width;
	/*! display height of the video in pixels, 0 if audio stream*/
	u32 display_height;
	/*! index of currently selected quality*/
	u32 active_quality_idx;
	/*! minimum buffer level in milliseconds below witch rebuffer will happen*/
	u32 buffer_min_ms;
	/*! maximum buffer level allowed in milliseconds. Packets won't get dropped if overflow, but the algorithm should try not to overflow this buffer*/
	u32 buffer_max_ms;
	/*! current buffer level in milliseconds*/
	u32 buffer_occupancy_ms;
	/*! degradation hint, 0 means no degradation, 100 means tile completely hidden*/
	u32 quality_degradation_hint;
	/*! cumulated download rate of all active groups in bytes per seconds - 0 means all files are local*/
	u32 total_rate;
} GF_DASHCustomAlgoInfo;

/*! Callback function for custom rate adaptation
\param udta user data
\param group_idx index of group to adapt
\param base_group_idx index of associated base group if group is a dependent group
\param force_lower_complexity set to true if the dash client would like a lower complexity
\param stats statistics for last downloaded segment
\return value can be:
- the index of the new quality to select (as listed in group.reps[])
- `-1` to not take decision now and postpone it until dependent groups are done
- `-2` to disable quality
- any other negative value means error
 */
typedef s32 (*gf_dash_rate_adaptation)(void *udta, u32 group_idx, u32 base_group_idx, Bool force_lower_complexity, GF_DASHCustomAlgoInfo *stats);

/*! Callback function for custom rate monitor, not final yet
\param udta user data
\param group_idx index of group to adapt
\param bits_per_sec estimated download rate (not premultiplied by playback speed)
\param total_bytes size of segment being downloaded, 0 if unknown
\param bytes_done bytes received for segment
\param us_since_start microseconds ellapse since segment was sheduled for download
\param buffer_dur_ms current buffer duration in milliseconds
\param current_seg_dur duration of segment being downloaded, 0 if unknown
\return quality index (>=0) to switch to after abort, -1 to do nothing (no abort), -2 for internal algorithms having already setup the desired quality and requesting only abort
 */
typedef s32 (*gf_dash_download_monitor)(void *udta, u32 group_idx, u32 bits_per_sec, u64 total_bytes, u64 bytes_done, u64 us_since_start, u32 buffer_dur_ms, u32 current_seg_dur);


/*! sets custom rate adaptation logic
\param dash the target dash client
\param udta user data to pass back to callback functions
\param algo_custom rate adaptation custom logic
\param download_monitor_custom download monitor custom logic
 */
void gf_dash_set_algo_custom(GF_DashClient *dash, void *udta,
		gf_dash_rate_adaptation algo_custom,
		gf_dash_download_monitor download_monitor_custom);


#endif //GPAC_DISABLE_DASHIN

/*!	@} */

#ifdef __cplusplus
}
#endif

#endif	/*_GF_DASH_H_*/

