/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre - Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2010-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / 3GPP/MPEG Media Presentation Description input module
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
#ifndef _MPD_H_
#define _MPD_H_

#include <gpac/constants.h>
#include <gpac/xml.h>
#include <gpac/media_tools.h>

#ifndef GPAC_DISABLE_CORE_TOOLS


typedef enum
{
	GF_DASH_TEMPLATE_SEGMENT = 0,
	GF_DASH_TEMPLATE_INITIALIZATION,
	GF_DASH_TEMPLATE_TEMPLATE,
	GF_DASH_TEMPLATE_TEMPLATE_WITH_PATH,
	GF_DASH_TEMPLATE_INITIALIZATION_TEMPLATE,
	GF_DASH_TEMPLATE_REPINDEX,
	GF_DASH_TEMPLATE_REPINDEX_TEMPLATE,
	GF_DASH_TEMPLATE_REPINDEX_TEMPLATE_WITH_PATH,
	//same as GF_DASH_TEMPLATE_INITIALIZATION but skip default "init" concatenation
	GF_DASH_TEMPLATE_INITIALIZATION_SKIPINIT,
	//same as GF_DASH_TEMPLATE_INITIALIZATION_TEMPLATE but skip default "init" concatenation
	GF_DASH_TEMPLATE_INITIALIZATION_TEMPLATE_SKIPINIT,
} GF_DashTemplateSegmentType;

GF_Err gf_media_mpd_format_segment_name(GF_DashTemplateSegmentType seg_type, Bool is_bs_switching, char *segment_name, const char *output_file_name, const char *rep_id, const char *base_url, const char *seg_rad_name, const char *seg_ext, u64 start_time, u32 bandwidth, u32 segment_number, Bool use_segment_timeline);

/*TODO*/
typedef struct
{
	u32 dummy;
} GF_MPD_Metrics;

/*TODO*/
typedef struct
{
	u32 id;
	char *type;
	char *lang;
} GF_MPD_ContentComponent;


//some elments are typically overloaded in XML, we keep the attributes / children nodes here. The attributes list is NULL if no extensions were found, otherwise it is a list of @GF_XMLAttribute.
//The children list is NULL if no extensions were found, otherwise it is a list of @GF_XMLNode
#define MPD_EXTENSIBLE	\
	GF_List *attributes;	\
	GF_List *children;	\

typedef struct
{
	MPD_EXTENSIBLE
} GF_MPD_ExtensibleVirtual;

typedef struct
{
	MPD_EXTENSIBLE

	char *scheme_id_uri; /*MANDATORY*/
	char *value;
	char *id;

} GF_MPD_Descriptor;

/*TODO*/
typedef struct
{
	u32 dummy;
} GF_MPD_Subset;

typedef struct
{
	u64 start_time;
	u32 duration; /*MANDATORY*/
	//may be -1 (FIXME this needs further testing)
	u32 repeat_count;
} GF_MPD_SegmentTimelineEntry;

typedef struct
{
	GF_List *entries;
} GF_MPD_SegmentTimeline;

typedef struct
{
	u64 start_range, end_range;
} GF_MPD_ByteRange;


typedef struct
{
	char *URL;
	char *service_location;
	GF_MPD_ByteRange *byte_range;

	//GPAC internal: redirection for that URL
	char *redirection;
} GF_MPD_BaseURL;

typedef struct
{
	char *sourceURL;
	GF_MPD_ByteRange *byte_range;

	//GPAC internal - indicates the URL has already been solved
	Bool is_resolved;
} GF_MPD_URL;

typedef struct
{
	u32 num, den;
} GF_MPD_Fractional;

typedef struct
{
	u32 trackID; //ID of track/pid/etc
	char *stsd;
	s64 mediaOffset;
} GF_MPD_ISOBMFInfo;


typedef struct
{
	char *xml_desc;
} GF_MPD_other_descriptors;



#define GF_MPD_SEGMENT_BASE	\
	u32 timescale;	\
	u64 presentation_time_offset;	\
	u32 time_shift_buffer_depth; /* expressed in milliseconds */	\
	GF_MPD_ByteRange *index_range;	\
	Bool index_range_exact;	\
	Double availability_time_offset;	\
	GF_MPD_URL *initialization_segment;	\
	GF_MPD_URL *representation_index;	\



typedef struct
{
	GF_MPD_SEGMENT_BASE
} GF_MPD_SegmentBase;

/*WARNING: duration is expressed in GF_MPD_SEGMENT_BASE@timescale unit
           startnumber=(u32)-1 if unused, 1 bydefault.
*/
#define GF_MPD_MULTIPLE_SEGMENT_BASE	\
	GF_MPD_SEGMENT_BASE	\
	u64 duration;	\
	u32 start_number;	\
	GF_MPD_SegmentTimeline *segment_timeline;	\
	GF_MPD_URL *bitstream_switching_url;	\

typedef struct
{
	GF_MPD_MULTIPLE_SEGMENT_BASE
} GF_MPD_MultipleSegmentBase;

typedef struct
{
	char *media;
	GF_MPD_ByteRange *media_range;
	char *index;
	GF_MPD_ByteRange *index_range;
	u64 duration;
	/*HLS only*/
	char *key_url;
	bin128 key_iv;
	u64 hls_utc_start_time;
	GF_List *Segments_duration_list;	\
} GF_MPD_SegmentURL;

typedef struct
{
	GF_MPD_MULTIPLE_SEGMENT_BASE
	/*list of segments - can be NULL if no segment*/
	GF_List *segment_URLs;

	char *xlink_href;
	Bool xlink_actuate_on_load;

	//playback related
	u32 consecutive_xlink_count;
	//generation related, we store the segment template here
	char *dasher_segment_name;
} GF_MPD_SegmentList;

typedef struct
{
	GF_MPD_MULTIPLE_SEGMENT_BASE
	char *media;
	char *index;
	char *initialization;
	char *bitstream_switching;
} GF_MPD_SegmentTemplate;

typedef enum
{
	GF_MPD_SCANTYPE_UNKNWON,
	GF_MPD_SCANTYPE_PROGRESSIVE,
	GF_MPD_SCANTYPE_INTERLACED
} GF_MPD_ScanType;

/*MANDATORY:
	mime_type
	codecs
*/
#define GF_MPD_COMMON_ATTRIBUTES_ELEMENTS	\
	char *profiles;	\
	u32 width;	\
	u32 height;	\
	GF_MPD_Fractional *sar;	\
	GF_MPD_Fractional *framerate;	\
	u32 samplerate;	\
	char *mime_type;	\
	char *segmentProfiles;	\
	char *codecs;	\
	u32 maximum_sap_period;	\
	u32 starts_with_sap;	\
	Double max_playout_rate;	\
	Bool coding_dependency;	\
	GF_MPD_ScanType scan_type;	\
	GF_List *frame_packing;	\
	GF_List *audio_channels;	\
	GF_List *content_protection;	\
	GF_List *essential_properties;	\
	GF_List *supplemental_properties;	\
	GF_List *isobmf_tracks;	\

typedef struct {
	GF_MPD_COMMON_ATTRIBUTES_ELEMENTS
} GF_MPD_CommonAttributes;

typedef struct {
	GF_MPD_COMMON_ATTRIBUTES_ELEMENTS

	u32 level;
	char *dependecy_level;
	u32 bandwidth; /*MANDATORY if level set*/
	char *content_components;
} GF_MPD_SubRepresentation;

typedef struct
{
	Bool disabled;
	char *cached_init_segment_url;
	Bool owned_gmem;
	u64 init_start_range, init_end_range;
	u32 probe_switch_count;
	GF_Blob init_segment;

	char *key_url;
	bin128 key_IV;
	/*previous maximum speed that this representation can be played, or 0 if it has never been played*/
	Double prev_max_available_speed;
	/*after switch we may have some buffered segments of the previous representation; so codec stats at this moment is unreliable. we should wait after the codec reset*/
	Bool waiting_codec_reset;
	// BOLA Utility
	Double bola_v;

	/*index of the next enhancement representation plus 1, 0 is reserved in case of the highest representation*/
	u32 enhancement_rep_index_plus_one;

	Bool broadcast_flag;

	void *udta;
} GF_DASH_RepresentationPlayback;


typedef struct
{
	char *period_id;
	Double period_start;
	Double period_duration;
	Bool done;
	u64 last_pck_idx;
	u32 seg_number;
	char *src_url;
	char *init_seg;
	char *template_seg;
	char *template_idx;
	u32 pid_id, source_pid, dep_pid_id;
	Bool owns_set;
	Bool multi_pids;
	Bool removed;
	Double dash_dur;
	u64 next_seg_start;
	u64 first_cts, first_dts;
	u32 nb_repeat;
	u64 ts_offset;
	u32 mpd_timescale;
	u64 est_next_dts;
	Double cumulated_subdur, cumulated_dur;
	char *mux_pids;
	u32 segs_purged;
	Double dur_purged;
	u32 moof_sn, moof_sn_inc;
	u32 last_dyn_period_id;
} GF_DASH_SegmenterContext;

typedef struct
{
	u64 time; //in mpd timescale
	u64 dur; //in timescale
	char *filename;//name as printed in segment lists / m3u8
	char *filepath; //full path
	u32 file_size;
	u64 file_offset;
	u32 index_size;
	u64 index_offset;
	u32 seg_num;
} GF_DASH_SegmentContext;

typedef struct {
	GF_MPD_COMMON_ATTRIBUTES_ELEMENTS

	char *id; /*MANDATORY*/
	u32 bandwidth; /*MANDATORY*/
	u32 quality_ranking;
	char *dependency_id;
	char *media_stream_structure_id;

	GF_List *base_URLs;
	GF_MPD_SegmentBase *segment_base;
	GF_MPD_SegmentList *segment_list;
	GF_MPD_SegmentTemplate *segment_template;

	GF_List *sub_representations;

	/*GPAC playback implementation*/
	GF_DASH_RepresentationPlayback playback;
	u32 m3u8_media_seq_min, m3u8_media_seq_max;
	GF_List *other_descriptors;

	GF_DASH_SegmenterContext *dasher_ctx;
	GF_List *state_seg_list;
	u64 gpac_index_start_range;
	u64 gpac_index_endrange;
	//used for HLS
	u32 timescale;
	u32 timescale_mpd;
	Double dash_dur;
	const char *init_seg;
	u32 nb_chan;
	Double fps;
	const char *m3u8_name; //assigned by user
	char *m3u8_var_name; //gen one
	FILE *m3u8_var_file; //tmp file
} GF_MPD_Representation;


typedef struct
{
	GF_MPD_COMMON_ATTRIBUTES_ELEMENTS

	u32 id;
	/*default value is -1: not set in MPD*/
	s32 group;

	char *lang;
	char *content_type;
	GF_MPD_Fractional *par;
	u32 min_bandwidth;
	u32 max_bandwidth;
	u32 min_width;
	u32 max_width;
	u32 min_height;
	u32 max_height;
	u32 min_framerate;
	u32 max_framerate;
	Bool segment_alignment;
	Bool bitstream_switching;
	Bool subsegment_alignment;
	Bool subsegment_starts_with_sap;

	GF_List *accessibility;
	GF_List *role;
	GF_List *rating;
	GF_List *viewpoint;
	GF_List *content_component;

	GF_List *base_URLs;
	GF_MPD_SegmentBase *segment_base;
	GF_MPD_SegmentList *segment_list;
	GF_MPD_SegmentTemplate *segment_template;

	GF_List *representations;

	char *xlink_href;
	Bool xlink_actuate_on_load;
	GF_List *other_descriptors;

	//used by dasher
	void *udta;
} GF_MPD_AdaptationSet;

typedef enum {
	GF_MPD_TYPE_STATIC,
	GF_MPD_TYPE_DYNAMIC,
	GF_MPD_TYPE_DYNAMIC_LAST,
} GF_MPD_Type;

typedef struct
{
	char *ID;
	u64 start; /* expressed in ms, relative to the start of the MPD */
	u64 duration; /* expressed in ms*/
	Bool bitstream_switching;

	GF_List *base_URLs;
	GF_MPD_SegmentBase *segment_base;
	GF_MPD_SegmentList *segment_list;
	GF_MPD_SegmentTemplate *segment_template;

	GF_List *adaptation_sets;
	GF_List *subsets;
	char *xlink_href;
	Bool xlink_actuate_on_load;

	GF_List *other_descriptors;
	char *origin_base_url;
	GF_MPD_Type type;
} GF_MPD_Period;

typedef struct
{
	char *lang;
	char *title;
	char *source;
	char *copyright;
	char *more_info_url;
} GF_MPD_ProgramInfo;


typedef struct {
	MPD_EXTENSIBLE

	char *ID;
	char *profiles;	/*MANDATORY*/
	GF_MPD_Type type;
	u64 availabilityStartTime; /*UTC,  expressed in milliseconds */	/*MANDATORY if type=dynamic*/
	u64 availabilityEndTime;/* UTC, expressed in milliseconds */
	u64 publishTime;/* UTC, expressed in milliseconds */
	u64 media_presentation_duration; /* expressed in milliseconds */	/*MANDATORY if type=static*/
	u32 minimum_update_period; /* expressed in milliseconds */
	u32 min_buffer_time; /* expressed in milliseconds */	/*MANDATORY*/

	u32 time_shift_buffer_depth; /* expressed in milliseconds */
	u32 suggested_presentation_delay; /* expressed in milliseconds */

	u32 max_segment_duration; /* expressed in milliseconds */
	u32 max_subsegment_duration; /* expressed in milliseconds */

	/*list of GF_MPD_ProgramInfo */
	GF_List *program_infos;
	/*list of GF_MPD_BaseURL */
	GF_List *base_URLs;
	/*list of strings */
	GF_List *locations;
	/*list of Metrics */
	GF_List *metrics;
	/*list of GF_MPD_Period */
	GF_List *periods;

	/*set during parsing*/
	const char *xml_namespace; /*won't be freed by GPAC*/

	/*serialized for state mode only*/
	u64 gpac_init_ntp_ms;
	u64 gpac_next_ntp_ms;
	u64 gpac_mpd_time; //in ms

	//internal options
	//indicates the GPAC state info should be written
	Bool write_context;
	//indicates this is tha last static serialization of a previously dynamic MPD
	Bool was_dynamic;
	//indicates the HLS variant files shall be created, otherwise temp files are used
	Bool create_m3u8_files;
	Bool m3u8_time;
} GF_MPD;

GF_Err gf_mpd_init_from_dom(GF_XMLNode *root, GF_MPD *mpd, const char *base_url);
GF_Err gf_mpd_complete_from_dom(GF_XMLNode *root, GF_MPD *mpd, const char *base_url);

GF_MPD *gf_mpd_new();
void gf_mpd_del(GF_MPD *mpd);
/*frees a GF_MPD_SegmentURL structure (type-casted to void *)*/
void gf_mpd_segment_url_free(void *ptr);
void gf_mpd_segment_base_free(void *ptr);
void gf_mpd_parse_segment_url(GF_List *container, GF_XMLNode *root);
void gf_mpd_url_free(void *_item);

GF_MPD_Period *gf_mpd_period_new();
void gf_mpd_period_free(void *_item);

GF_Err gf_mpd_write(GF_MPD const * const mpd, FILE *out, Bool compact);
GF_Err gf_mpd_write_file(GF_MPD const * const mpd, const char *file_name);

GF_Err gf_mpd_write_m3u8_master_playlist(GF_MPD const * const mpd, FILE *out, const char* m3u8_name, GF_MPD_Period *period);

GF_Err gf_mpd_parse_period(GF_MPD *mpd, GF_XMLNode *root);

GF_MPD_Descriptor *gf_mpd_descriptor_new(const char *id, const char *uri, const char *value);
GF_MPD_AdaptationSet *gf_mpd_adaptation_set_new();

typedef struct _gf_file_get GF_FileDownload;
struct _gf_file_get
{
	GF_Err (*new_session)(GF_FileDownload *getter, char *url);
	void (*del_session)(GF_FileDownload *getter);
	const char *(*get_cache_name)(GF_FileDownload *getter);

	void *udta;
	/*created by udta after new_session*/
	void *session;
};

/*converts M3U8 to MPD - getter is optional (download will still be processed if NULL)*/
GF_Err gf_m3u8_to_mpd(const char *m3u8_file, const char *base_url, const char *mpd_file, u32 reload_count, char *mimeTypeForM3U8Segments, Bool do_import, Bool use_mpd_templates, Bool use_segment_timeline, GF_FileDownload *getter, GF_MPD *mpd, Bool parse_sub_playlist, Bool keep_files);

GF_Err gf_m3u8_solve_representation_xlink(GF_MPD_Representation *rep, GF_FileDownload *getter, Bool *is_static, u64 *duration);

GF_MPD_SegmentList *gf_mpd_solve_segment_list_xlink(GF_MPD *mpd, GF_XMLNode *root);

GF_Err gf_mpd_init_smooth_from_dom(GF_XMLNode *root, GF_MPD *mpd, const char *default_base_url);

void gf_mpd_delete_segment_list(GF_MPD_SegmentList *segment_list);

void gf_mpd_getter_del_session(GF_FileDownload *getter);

GF_Err gf_mpd_init_smooth_from_dom(GF_XMLNode *root, GF_MPD *mpd, const char *default_base_url);

void gf_mpd_del_list(GF_List *list, void (*__destructor)(void *), Bool reset_only);
void gf_mpd_descriptor_free(void *item);

GF_Err gf_mpd_split_adaptation_sets(GF_MPD *mpd);


GF_Err gf_mpd_smooth_to_mpd(char * smooth_file, GF_MPD *mpd, const char *default_base_url);

/*get the number of base URLs for the given representation. This cumuluates all base URLs at MPD, period, AdaptationSet and Representation levels*/
u32 gf_mpd_get_base_url_count(GF_MPD *mpd, GF_MPD_Period *period, GF_MPD_AdaptationSet *set, GF_MPD_Representation *rep);

typedef enum
{
	GF_MPD_RESOLVE_URL_MEDIA,
	GF_MPD_RESOLVE_URL_INIT,
	GF_MPD_RESOLVE_URL_INDEX,
	//same as GF_MPD_RESOLVE_URL_MEDIA but does not replace $Time$ and $Number$
	GF_MPD_RESOLVE_URL_MEDIA_TEMPLATE,
	//same as GF_MPD_RESOLVE_URL_MEDIA but does not use startNumber
	GF_MPD_RESOLVE_URL_MEDIA_NOSTART,
} GF_MPD_URLResolveType;

/*resolves a URL based for a given segment, based on the MPD url, the type of resolution
	base_url_index: 0-based index of the baseURL to use
	item_index: current downloading index of the segment
	nb_segments_removed: number of segments removed when purging the MPD after updates (can be 0). The start number will be offset by this value
*/
GF_Err gf_mpd_resolve_url(GF_MPD *mpd, GF_MPD_Representation *rep, GF_MPD_AdaptationSet *set, GF_MPD_Period *period, const char *mpd_url, u32 base_url_index, GF_MPD_URLResolveType resolve_type, u32 item_index, u32 nb_segments_removed,
                          char **out_url, u64 *out_range_start, u64 *out_range_end, u64 *segment_duration, Bool *is_in_base_url, char **out_key_url, bin128 *key_iv);

/*get duration of the presentation*/
Double gf_mpd_get_duration(GF_MPD *mpd);

/*get the duration of media segments*/
void gf_mpd_resolve_segment_duration(GF_MPD_Representation *rep, GF_MPD_AdaptationSet *set, GF_MPD_Period *period, u64 *out_duration, u32 *out_timescale, u64 *out_pts_offset, GF_MPD_SegmentTimeline **out_segment_timeline);

/*get the start_time from the segment index of a period/set/rep*/
GF_Err gf_mpd_get_segment_start_time_with_timescale(s32 in_segment_index,
	GF_MPD_Period const * const in_period, GF_MPD_AdaptationSet const * const in_set, GF_MPD_Representation const * const in_rep,
	u64 *out_segment_start_time, u64 *out_opt_segment_duration, u32 *out_opt_scale);

typedef enum {
	MPD_SEEK_PREV,    /*will return the segment containing the requested time*/
	MPD_SEEK_NEAREST, /*the nearest segment start time, may be the previous or the next one*/
} MPDSeekMode;


/*returns the segment index in the given period for the given time*/
GF_Err gf_mpd_seek_in_period(Double seek_time, MPDSeekMode seek_mode,
	GF_MPD_Period const * const in_period, GF_MPD_AdaptationSet const * const in_set, GF_MPD_Representation const * const in_rep,
	u32 *out_segment_index, Double *out_opt_seek_time);

void gf_mpd_base_url_free(void *_item);

GF_MPD_Representation *gf_mpd_representation_new();
void gf_mpd_representation_free(void *_item);

GF_MPD_SegmentTimeline *gf_mpd_segmentimeline_new();

/*get duration of the presentation*/
Double gf_mpd_get_duration(GF_MPD *mpd);

/*get the duration of media segments in seconds*/
void gf_mpd_resolve_segment_duration(GF_MPD_Representation *rep, GF_MPD_AdaptationSet *set, GF_MPD_Period *period, u64 *out_duration, u32 *out_timescale, u64 *out_pts_offset, GF_MPD_SegmentTimeline **out_segment_timeline);


typedef struct
{
	u32 sample_num;
	u64 dts;
	s64 cts;
	Bool is_processed;
} GF_DASHCueInfo;

GF_Err gf_mpd_load_cues(const char *cues_file, u32 stream_id, u32 *cues_timescale, Bool *use_edit_list, GF_DASHCueInfo **out_cues, u32 *nb_cues);


#endif /*GPAC_DISABLE_CORE_TOOLS*/

#endif // _MPD_H_
