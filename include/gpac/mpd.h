/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre - Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2010-2024
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

#ifndef GPAC_DISABLE_MPD

/*!
\file <gpac/mpd.h>
\brief Utility tools for dash, smooth and HLS manifest loading.
*/

/*!
\addtogroup mpd_grp MPD/M3U8/Smooth Manifest Parsing
\ingroup media_grp
\brief Utility tools DASH manifests

This section documents the DASH, Smooth and HLS manifest parsing functions of the GPAC framework.
@{
*/

/*! DASH template resolution mode*/
typedef enum
{
	/*! resolve template for segment*/
	GF_DASH_TEMPLATE_SEGMENT = 0,
	/*! resolve template for initialization segment*/
	GF_DASH_TEMPLATE_INITIALIZATION,
	/*! resolve template for segment template*/
	GF_DASH_TEMPLATE_TEMPLATE,
	/*! resolve template for segment template with %d padding*/
	GF_DASH_TEMPLATE_TEMPLATE_WITH_PATH,
	/*! resolve template for initialization segment template*/
	GF_DASH_TEMPLATE_INITIALIZATION_TEMPLATE,
	/*! resolve template for segment index*/
	GF_DASH_TEMPLATE_REPINDEX,
	/*! resolve template for segment index template*/
	GF_DASH_TEMPLATE_REPINDEX_TEMPLATE,
	/*! resolve template for segment index template with %d padding*/
	GF_DASH_TEMPLATE_REPINDEX_TEMPLATE_WITH_PATH,
	/*! same as GF_DASH_TEMPLATE_INITIALIZATION but skip default "init" concatenation */
	GF_DASH_TEMPLATE_INITIALIZATION_SKIPINIT,
	/*! same as GF_DASH_TEMPLATE_INITIALIZATION_TEMPLATE but skip default "init" concatenation*/
	GF_DASH_TEMPLATE_INITIALIZATION_TEMPLATE_SKIPINIT,
} GF_DashTemplateSegmentType;

/*! formats the segment name according to its template
\param seg_type the desired format mode
\param is_bs_switching set to GF_TRUE to indicate the target segment is a bitstream switching segment
\param segment_name target buffer where the segment name is formatted - size must be GF_MAX_PATH
\param rep_id ID of the target representation
\param base_url base URL, may be NULL
\param seg_rad_name base name of the output segmeents (eg, myfile_ZZZ), shall not be NULL, may be empty ("")
\param seg_ext segment extensions
\param start_time start time of the segment in MPD timescale
\param bandwidth bandwidth used for the representation
\param segment_number number of the target segment
\param use_segment_timeline indicates if segmentTimeline is used for segment addressing in the MPD
\param forced if true, do not append extension or missing $Number$ or $Time$  when resolving template
\return error if any
*/
GF_Err gf_media_mpd_format_segment_name(GF_DashTemplateSegmentType seg_type, Bool is_bs_switching, char *segment_name, const char *rep_id, const char *base_url, const char *seg_rad_name, const char *seg_ext, u64 start_time, u32 bandwidth, u32 segment_number, Bool use_segment_timeline, Bool forced);

/*! metrics, not yet supported*/
typedef struct
{
	u32 dummy;
} GF_MPD_Metrics;

/*! Content Component description*/
typedef struct
{
	/*! content component ID*/
	u32 id;
	/*! content component mime type*/
	char *type;
	/*! content component language*/
	char *lang;
} GF_MPD_ContentComponent;


/*! macro for extensible MPD element
Some elments are typically overloaded in XML, we keep the attributes / children nodes here. The attributes list is NULL if no extensions were found, otherwise it is a list of GF_XMLAttribute.
The children list is NULL if no extensions were found, otherwise it is a list of GF_XMLNode
*/
#define MPD_EXTENSIBLE	\
	GF_List *x_attributes;	\
	GF_List *x_children;	\

/*! basic extensible MPD descriptor*/
typedef struct
{
	MPD_EXTENSIBLE
	/*! mandatory schemeid URL*/
	char *scheme_id_uri;
	/*! associated value, may be NULL*/
	char *value;
	/*! associated ID, may be NULL*/
	char *id;
} GF_MPD_Descriptor;

/*! subset, not yet supported*/
typedef struct
{
	u32 dummy;
} GF_MPD_Subset;

/*! SegmentTimeline entry*/
typedef struct
{
	/*! startTime in representation's MPD timescale*/
	u64 start_time;
	/*! duration in representation's MPD timescale - mandatory*/
	u32 duration; /*MANDATORY*/
	/*! may be 0xFFFFFFFF (-1) (\warning this needs further testing)*/
	u32 repeat_count;
} GF_MPD_SegmentTimelineEntry;

/*! Segment Timeline*/
typedef struct
{
	/*! list of entries*/
	GF_List *entries;
} GF_MPD_SegmentTimeline;

/*! Byte range info*/
typedef struct
{
	/*! start range (offset of first byte in associated resource)*/
	u64 start_range;
	/*! start range (offset of last byte in associated resource)*/
	u64 end_range;
} GF_MPD_ByteRange;


/*! base URL*/
typedef struct
{
	/*! URL*/
	char *URL;
	/*! service location if any*/
	char *service_location;
	/*! byte range if any*/
	GF_MPD_ByteRange *byte_range;

	/*!GPAC internal: redirection for that URL */
	char *redirection;
	/*!GPAC internal: original URL relative to HLS  variant playlist  */
	const char *hls_vp_rel_url;
} GF_MPD_BaseURL;

/*! MPD URL*/
typedef struct
{
	/*! URL of source*/
	char *sourceURL;
	/*! byte range if any*/
	GF_MPD_ByteRange *byte_range;

	/*! GPAC internal - indicates the URL has already been solved*/
	Bool is_resolved;
} GF_MPD_URL;

/*! MPD fraction*/
typedef struct
{
	/*! numerator*/
	u32 num;
	/*! denominator*/
	u32 den;
} GF_MPD_Fractional;

/*! GPAC internal, ISOBMFF info for base64 and other embedding, see N. Bouzakaria thesis*/
typedef struct
{
	/*! ID of track/pid*/
	u32 trackID;
	/*! base64 STSD entry*/
	char *stsd;
	/*! media offset*/
	s64 mediaOffset;
} GF_MPD_ISOBMFInfo;

/*! macro for MPD segment base*/
#define GF_MPD_SEGMENT_BASE	\
	u32 timescale;	\
	u64 presentation_time_offset;	\
	u32 time_shift_buffer_depth; /* expressed in milliseconds */	\
	GF_MPD_ByteRange *index_range;	\
	Bool index_range_exact;	\
	Double availability_time_offset;	\
	GF_MPD_URL *initialization_segment;	\
	GF_MPD_URL *representation_index;	\



/*! MPD segment base*/
typedef struct
{
	/*! inherits segment base*/
	GF_MPD_SEGMENT_BASE
} GF_MPD_SegmentBase;

/*! macro for multiple segment base
WARNING: duration is expressed in GF_MPD_SEGMENT_BASE timescale unit
           startnumber=(u32)-1 if unused, 1 bydefault.
*/
#define GF_MPD_MULTIPLE_SEGMENT_BASE	\
	GF_MPD_SEGMENT_BASE	\
	u64 duration;	\
	u32 start_number;	\
	GF_MPD_SegmentTimeline *segment_timeline;	\
	u32 tsb_first_entry;	\
	GF_MPD_URL *bitstream_switching_url;	\

/*! Multiple segment base*/
typedef struct
{
	/*! inherits multiple segment base*/
	GF_MPD_MULTIPLE_SEGMENT_BASE
} GF_MPD_MultipleSegmentBase;

/*! segment URL*/
typedef struct
{
	/*! media URL if any*/
	char *media;
	/*! media range if any*/
	GF_MPD_ByteRange *media_range;
	/*! index url if any*/
	char *index;
	/*! index range if any*/
	GF_MPD_ByteRange *index_range;
	/*! duration of segment*/
	u64 duration;
	/*! key URL of segment, HLS only*/
	char *key_url;
	/*! key IV of segment, HLS only*/
	bin128 key_iv;
	/*! sequence number of segment, HLS only*/
	u32 hls_seq_num;
	/*! informative UTC start time of segment, HLS only*/
	u64 hls_utc_time;
	/*! 0: full segment, 1: LL-HLS part, 2: independent LL-HLS part */
	u8 hls_ll_chunk_type;
	/*! merge flag for byte-range subsegs 0: cannot merge, 1: can merge */
	u8 can_merge;
	/*! merge flag for byte-range subsegs 0: cannot merge, 1: can merge */
	u8 is_first_part;


	u64 first_tfdt, first_pck_seq, frag_start_offset, frag_tfdt;
	u32 split_first_dur, split_last_dur;
} GF_MPD_SegmentURL;

/*! SegmentList*/
typedef struct
{
	/*! inherits multiple segment base*/
	GF_MPD_MULTIPLE_SEGMENT_BASE
	/*! list of segments - can be NULL if no segment*/
	GF_List *segment_URLs;
	/*! xlink URL for external list*/
	char *xlink_href;
	/*! xlink evaluation on load if set, otherwise on use*/
	Bool xlink_actuate_on_load;

	/*! GPAC internal, number of consecutive xlink while solving*/
	u32 consecutive_xlink_count;
	/*! GPAC internal, we store the segment template here*/
	char *dasher_segment_name;
	/*! GPAC internal, we store the previous xlink before resolution*/
	char *previous_xlink_href;
	/*! GPAC internal for index mode*/
	Bool index_mode;
	Bool use_split_dur;
	u32 sample_duration, src_timescale, pid_delay;
	s32 first_cts_offset;
} GF_MPD_SegmentList;

/*! SegmentTemplate*/
typedef struct
{
	/*! inherits multiple segment base*/
	GF_MPD_MULTIPLE_SEGMENT_BASE
	/*! media segment template*/
	char *media;
	/*! index segment template*/
	char *index;
	/*! init segment template*/
	char *initialization;
	/*! bitstream switching segment template*/
	char *bitstream_switching;

	/*! internal, for HLS generation*/
	const char *hls_init_name;
} GF_MPD_SegmentTemplate;

/*! MPD scan types*/
typedef enum
{
	/*! unknown*/
	GF_MPD_SCANTYPE_UNKNOWN,
	/*! progressive*/
	GF_MPD_SCANTYPE_PROGRESSIVE,
	/*! interlaced*/
	GF_MPD_SCANTYPE_INTERLACED
} GF_MPD_ScanType;

/*! Macro for common attributes and elements (representation, AdaptationSet, Preselection, ...)

not yet implemented;
	GF_List *inband_event_stream;	\
	GF_List *switching;	\
	GF_List *random_access;	\
	GF_List *group_labels;	\
	GF_List *labels;	\
	GF_List *content_popularity;	\

MANDATORY:
	mime_type
	codecs
*/
#define GF_MPD_COMMON_ATTRIBUTES_ELEMENTS	\
	GF_List *x_attributes;	\
	GF_List *x_children;	\
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
	u32 selection_priority;	\
	char *tag;	\
	GF_List *frame_packing;	\
	GF_List *audio_channels;	\
	GF_List *content_protection;	\
	GF_List *essential_properties;	\
	GF_List *supplemental_properties;	\
	GF_List *producer_reference_time;	\
	GF_List *isobmf_tracks;	\

/*! common attributes*/
typedef struct {
	/*! inherits common attributes*/
	GF_MPD_COMMON_ATTRIBUTES_ELEMENTS
} GF_MPD_CommonAttributes;

/*! type of reference clock */
typedef enum
{
	GF_MPD_PRODUCER_REF_ENCODER = 0,
	GF_MPD_PRODUCER_REF_CAPTURED,
	GF_MPD_PRODUCER_REF_APPLICATION,
} GF_MPD_ProducerRefType;

/*! producer reference time*/
typedef struct {
	/*! ID of producer */
	u32 ID;
	/*! is timing inband (prft in segment) */
	Bool inband;
	/*! clock type*/
	GF_MPD_ProducerRefType type;
	/*! scheme for application ref type*/
	char *scheme;
	/*! wallclock time as UTC timestamp*/
	char *wallclock;
	/*! presentation time in timescale of the Representation*/
	u64 presentation_time;
	/*! UTC timing desc if any */
	GF_MPD_Descriptor *utc_timing;
} GF_MPD_ProducerReferenceTime;


/*! SubRepresentation*/
typedef struct {
	/*! inherits common attributes*/
	GF_MPD_COMMON_ATTRIBUTES_ELEMENTS
	/*! level of subrepresentation*/
	u32 level;
	/*! dependency level of subrepresentation*/
	char *dependecy_level;
	/*! bandwidth of subrepresentation, MANDATORY if level set*/
	u32 bandwidth;
	/*! content comonents string*/
	char *content_components;
} GF_MPD_SubRepresentation;

/*! State for representation playback, GPAC internal*/
typedef struct
{
	/*! disabled*/
	Bool disabled;
	/*! name of cahed init segment URL (usually local cache or gmem:// url)*/
	char *cached_init_segment_url;
	/*! if set indicates the associated gmem memory is owned by this representation*/
	Bool owned_gmem;
	/*! start range of the init segment*/
	u64 init_start_range;
	/*! end range of the init segment*/
	u64 init_end_range;
	/*! number of switching probes*/
	u32 probe_switch_count;
	/*! init segment blob*/
	GF_Blob init_segment;
	/*! associated key URL if any, for HLS*/
	char *key_url;
	/*! associated key IV if any, for HLS*/
	bin128 key_IV;
	/*! previous maximum speed that this representation can be played, or 0 if it has never been played*/
	Double prev_max_available_speed;
	/*! after switch we may have some buffered segments of the previous representation; so codec stats at this moment is unreliable. we should wait after the codec reset*/
	Bool waiting_codec_reset;
	/*! BOLA Utility*/
	Double bola_v;

	/*! index of the next enhancement representation plus 1, 0 is reserved in case of the highest representation*/
	u32 enhancement_rep_index_plus_one;
	/*! set to true if the representation comes from a broadcast link (ATSC3, eMBMS)*/
	Bool broadcast_flag;

	/*! if set indicates the associated representations use vvc rpr switching*/
	Bool vvc_rpr_switch;

	/*! start of segment name in full url*/
	const char *init_seg_name_start;
	/*! opaque data*/
	void *udta;
	/*! SHA1 digest for xlinks / m3u8*/
	u8 xlink_digest[GF_SHA1_DIGEST_SIZE];
	/*! set to TRUE if not modified in the update of an xlink*/
	Bool not_modified;
} GF_DASH_RepresentationPlayback;

/*! segment context used by the dasher, GPAC internal*/
typedef struct
{
	/*! ID of active period*/
	char *period_id;
	/*! start of active period*/
	GF_Fraction64 period_start;
	/*! duration of active period*/
	GF_Fraction64 period_duration;
	/*! if GF_TRUE, representation is over*/
	Bool done;
	/*! niumber of last packet processed (to resume dashing)*/
	u64 last_pck_idx;
	/*! number of last produced segment*/
	u32 seg_number;
	/*! source URL*/
	char *src_url;
	/*! name of init segment*/
	char *init_seg;
	/*! segment template (half-resolved, no more %s in it)*/
	char *template_seg;
	/*! index template (half-resolved, no more %s in it)*/
	char *template_idx;
	/*! ID of output PID*/
	u32 pid_id;
	/*! ID of source PID*/
	u32 source_pid;
	/*! ID of source dependent PID*/
	u32 dep_pid_id;
	/*! indicates if this representation drives the AS segmentation*/
	Bool owns_set;
	/*! indicates if uses multi PID (eg, multiple sample descriptions in init segment)*/
	Bool multi_pids;
	/*! target segment duration for this stream*/
	GF_Fraction dash_dur;
	/*! estimated next segment start time in MPD timescale*/
	u64 next_seg_start;
	/*! first CTS of stream in stream timescale*/
	u64 first_cts;
	/*! first DCTS of stream in stream timescale*/
	u64 first_dts;
	/*! number of past repetitions of the stream */
	u32 nb_repeat;
	/*! timestamp offset (in stream timescale) due to repetitions*/
	u64 ts_offset;
	/*! mpd timescale of the stream*/
	u32 mpd_timescale;
	/*! estimated next DTS of the stream in media timescale*/
	u64 est_next_dts;
	/*! cumulated sub duration of the stream (to handle partial file dashing)*/
	Double cumulated_subdur;
	/*! cumulated duration of the stream (to handle loops)*/
	Double cumulated_dur;
	/*! space-separated list of PID IDs of streams muxed with this stream in a multiplex representation*/
	char *mux_pids;
	/*! number of segments purged from the timeline and from disk*/
	u32 segs_purged;
	/*! cumulated duration of segments purged*/
	Double dur_purged;
	/*! next moof sequence number*/
	u32 moof_sn;
	/*! next moof sequence number increment*/
	u32 moof_sn_inc;
	/*! ID of last dynamic period in manifest*/
	u32 last_dyn_period_id;
	/*! one subdur was forced on this rep due to looping*/
	Bool subdur_forced;
} GF_DASH_SegmenterContext;

/*! fragment context info for LL-HLS*/
typedef struct
{
	/*! frag offset in bytes*/
	u64 offset;
	/*! frag size in bytes*/
	u64 size;
	/*! frag duration in representation timescale*/
	u32 duration;
	/*! fragment contains an IDR*/
	Bool independent;
} GF_DASH_FragmentContext;

/*! Segment context - GPAC internal, used to produce HLS manifests and segment lists/timeline*/
typedef struct
{
	/*! time in mpd timescale*/
	u64 time;
	/*! duration in mpd timescale*/
	u64 dur;
	/*! name as printed in segment lists / m3u8*/
	char *filename;
	/*! full path of file*/
	char *filepath;
	/*! file size in bytes*/
	u32 file_size;
	/*! file offset in bytes*/
	u64 file_offset;
	/*! index size in bytes*/
	u32 index_size;
	/*! index offset in bytes*/
	u64 index_offset;
	/*! segment number */
	u32 seg_num;
	/*! number of fragment infos */
	u32 nb_frags;
	/*! number of fragment infos */
	GF_DASH_FragmentContext *frags;
	/*! HLS LL signaling - 0: disabled, 1: byte range, 2: files */
	u32 llhls_mode;
	/*! HLS LL segment done */
	Bool llhls_done;
	/*! HLS set to TRUE if encrypted */
	Bool encrypted;
	/*! HLS key params (URI and co)*/
	char *hls_key_uri;
	/*! HLS IV*/
	bin128 hls_iv;
} GF_DASH_SegmentContext;

/*! Representation*/
typedef struct {
	/*! inherits common attributes*/
	GF_MPD_COMMON_ATTRIBUTES_ELEMENTS

	/*! ID of representation, mandatory*/
	char *id;
	/*! bandwidth in bits per secon, mandatory*/
	u32 bandwidth;
	/*! quality ranking*/
	u32 quality_ranking;
	/*! dependency IDs of dependent representations*/
	char *dependency_id;
	/*! stream structure ID, not used by GPAC*/
	char *media_stream_structure_id;
	/*! list of baseURLs if any*/
	GF_List *base_URLs;

	/*! segment base of representation, or NULL if list or template is used*/
	GF_MPD_SegmentBase *segment_base;
	/*! segment list of representation, or NULL if base or template is used*/
	GF_MPD_SegmentList *segment_list;
	/*! segment template of representation, or NULL if base or list is used*/
	GF_MPD_SegmentTemplate *segment_template;
	/*! number of subrepresentation*/
	GF_List *sub_representations;

	/*! all the below members are GPAC internal*/

	/*! GPAC playback implementation*/
	GF_DASH_RepresentationPlayback playback;
	/*! internal, HLS: min sequence number of segments in playlist*/
	u32 m3u8_media_seq_min;
	/*! internal, HLS: max sequence number of segments in playlist*/
	u32 m3u8_media_seq_max;
	/*! internal, HLS: indicate this is a low latency rep*/
	u32 m3u8_low_latency;
	/*! internal, HLS:  sequence number of last indeendent  segment or PART in playlist*/
	u32 m3u8_media_seq_indep_last;

	/*! GPAC dasher context*/
	GF_DASH_SegmenterContext *dasher_ctx;
	/*! list of segment states*/
	GF_List *state_seg_list;
	s32 tsb_first_entry;
	/*! segment timescale (for HLS)*/
	u32 timescale;
	/*! stream type (for HLS)*/
	u32 streamtype;
	/*! segment manifest timescale (for HLS)*/
	u32 timescale_mpd;
	/*! dash duration*/
	GF_Fraction dash_dur;
	/*! init segment name for HLS single file*/
	const char *hls_single_file_name;
	/*! number of audio channels - HLS only*/
	u32 nb_chan;
	/*! video FPS - HLS only*/
	Double fps;
	/*! groupID (for HLS)*/
	const char *groupID;

	/*! user assigned m3u8 name for this representation*/
	const char *m3u8_name;
	/*! generated m3u8 name if no user-assigned one*/
	char *m3u8_var_name;
	/*! temp file for m3u8 generation*/
	FILE *m3u8_var_file;

	/*! for m3u8: 0: not encrypted, 1: full segment, 2: CENC*/
	u8 crypto_type;
	u8 def_kms_used;

	u32 nb_hls_master_tags;
	const char **hls_master_tags;

	u32 nb_hls_variant_tags;
	const char **hls_variant_tags;

	/*! target part (cmaf chunk) duration for HLS LL*/
	Double hls_ll_part_dur;

	/*! tfdt of first segment*/
	u64 first_tfdt_plus_one;
	u32 first_tfdt_timescale;

	GF_Fraction hls_max_seg_dur;

	//download in progress for m3u8
	Bool in_progress;
	char *res_url;
	u32 trackID;

	Bool sub_forced;
	const char *hls_forced;
} GF_MPD_Representation;

/*! AdaptationSet*/
typedef struct
{
	/*! inherits common attributes*/
	GF_MPD_COMMON_ATTRIBUTES_ELEMENTS
	/*! ID of this set, -1 if not set*/
	s32 id;
	/*! group ID for this set, default value is -1: not set in MPD*/
	s32 group;
	/*! language*/
	char *lang;
	/*! mime type*/
	char *content_type;
	/*! picture aspect ratio*/
	GF_MPD_Fractional *par;
	/*! min bandwidth in bps*/
	u32 min_bandwidth;
	/*! max bandwidth in bps*/
	u32 max_bandwidth;
	/*! min width in pixels*/
	u32 min_width;
	/*! max width in pixels*/
	u32 max_width;
	/*! min height in pixels*/
	u32 min_height;
	/*! max height in pixels*/
	u32 max_height;
	/*! min framerate*/
	GF_MPD_Fractional min_framerate;
	/*! max framerate*/
	GF_MPD_Fractional max_framerate;
	/*! set if segment boundaries are time-aligned across qualities*/
	Bool segment_alignment;
	/*! set if a single init segment is needed (no reinit at quality switch)*/
	Bool bitstream_switching;
	/*! set if subsegment boundaries are time-aligned across qualities*/
	Bool subsegment_alignment;
	/*! set if subsegment all start with given SAP type, 0 otherwise*/
	u32 subsegment_starts_with_sap;
	/*! accessibility descriptor list if any*/
	GF_List *accessibility;
	/*! role descriptor list if any*/
	GF_List *role;
	/*! rating descriptor list if any*/
	GF_List *rating;
	/*! viewpoint descriptor list if any*/
	GF_List *viewpoint;
	/*! content component descriptor list if any*/
	GF_List *content_component;

	/*! base URL (alternate location) list if any*/
	GF_List *base_URLs;
	/*! segment base of representation, or NULL if list or template is used*/
	GF_MPD_SegmentBase *segment_base;
	/*! segment list of representation, or NULL if base or template is used*/
	GF_MPD_SegmentList *segment_list;
	/*! segment template of representation, or NULL if base or list is used*/
	GF_MPD_SegmentTemplate *segment_template;
	/*! list of representations*/
	GF_List *representations;
	/*! xlink URL for the adaptation set*/
	char *xlink_href;
	/*! xlink evaluation on load if set, otherwise on use*/
	Bool xlink_actuate_on_load;

	/*! user private, eg used by dasher*/
	void *udta;

	/*! mpegh compatible profile hack*/
	u32 nb_alt_mha_profiles, *alt_mha_profiles;
	Bool alt_mha_profiles_only;

	/*! max number of valid chunks in smooth manifest*/
	u32 smooth_max_chunks;

	/*! INTRA-ONLY trick mode*/
	Bool intra_only;
	/*! adaptation set uses HLS LL*/
	Bool use_hls_ll;
	/*target fragment duration*/
	Double hls_ll_target_frag_dur;
} GF_MPD_AdaptationSet;

/*! MPD offering type*/
typedef enum {
	/*! content is statically available*/
	GF_MPD_TYPE_STATIC,
	/*! content is dynamically available*/
	GF_MPD_TYPE_DYNAMIC,
	/*! content is the last if a dynamical offering, converts MPD to static (GPAC internal)*/
	GF_MPD_TYPE_DYNAMIC_LAST,
} GF_MPD_Type;

/*! Period*/
typedef struct
{
	/*! inherits from extensible*/
	MPD_EXTENSIBLE

	/*! ID of period*/
	char *ID;
	/*! start time in milliseconds, relative to the start of the MPD */
	u64 start;
	/*! duration in milliseconds*/
	u64 duration;
	/*! set to GF_TRUE if adaptation sets in the period don't need reinit when switching quality*/
	Bool bitstream_switching;

	/*! base URL (alternate location) list if any*/
	GF_List *base_URLs;
	/*! segment base of representation, or NULL if list or template is used*/
	GF_MPD_SegmentBase *segment_base;
	/*! segment list of representation, or NULL if base or template is used*/
	GF_MPD_SegmentList *segment_list;
	/*! segment template of representation, or NULL if base or list is used*/
	GF_MPD_SegmentTemplate *segment_template;
	/*! list of adaptation sets*/
	GF_List *adaptation_sets;
	/*! list of subsets (not yet implemented)*/
	GF_List *subsets;
	/*! xlink URL for the period*/
	char *xlink_href;
	/*! xlink evaluation on load if set, otherwise on use*/
	Bool xlink_actuate_on_load;

	/*! original xlink URL before resolution - GPAC internal. Used to
		- identify already resolved xlinks in MPD updates
		- resolve URLs in remote period if no baseURL is explictly listed
	*/
	char *origin_base_url;
	/*! broken/ignored xlink, used to identify ignored xlinks in MPD updates  - GPAC internal*/
	char *broken_xlink;
	/*! type of the period - GPAC internal*/
	GF_MPD_Type type;

	/*! period is preroll - test only, GPAC internal*/
	Bool is_preroll;
} GF_MPD_Period;

/*! Program info*/
typedef struct
{
	/*! inherits from extensible*/
	MPD_EXTENSIBLE

	/*! languae*/
	char *lang;
	/*! title*/
	char *title;
	/*! source*/
	char *source;
	/*! copyright*/
	char *copyright;
	/*! URL to get more info*/
	char *more_info_url;
} GF_MPD_ProgramInfo;

/*! MPD*/
typedef struct {
	/*! inherits from extensible*/
	MPD_EXTENSIBLE
	/*! ID of the MPD*/
	char *ID;
	/*! profile, mandatory*/
	char *profiles;
	/*! offering type*/
	GF_MPD_Type type;
	/*! UTC of availability start anchor,  expressed in milliseconds, MANDATORY if type=dynamic*/
	u64 availabilityStartTime;
	/*! UTC of availability end anchor,  expressed in milliseconds*/
	u64 availabilityEndTime;
	/*! UTC of last publishing of the manifest*/
	u64 publishTime;
	/*! presentation duration in milliseconds, MANDATORY if type=static*/
	u64 media_presentation_duration;
	/*! refresh rate of MPD for dynamic offering, in milliseconds */
	u32 minimum_update_period;
	/*! minimum buffer time in milliseconds, MANDATORY*/
	u32 min_buffer_time;

	/*! time shift depth in milliseconds */
	u32 time_shift_buffer_depth;
	/*! presentation delay in milliseconds */
	u32 suggested_presentation_delay;
	/*! maximum segment duration in milliseconds */
	u32 max_segment_duration;
	/*! maximum subsegment duration in milliseconds */
	u32 max_subsegment_duration;

	/*! list of GF_MPD_ProgramInfo */
	GF_List *program_infos;
	/*! list of GF_MPD_BaseURL */
	GF_List *base_URLs;
	/*! list of strings */
	GF_List *locations;
	/*! list of Metrics */
	GF_List *metrics;
	/*! list of GF_MPD_Period */
	GF_List *periods;

	/*! set during parsing, to set during authoring, won't be freed by GPAC*/
	const char *xml_namespace;

	/*! UTC timing desc if any */
	GF_List *utc_timings;
	/*! Essential properties */
	GF_List *essential_properties;
	/*! Supplemental properties */
	GF_List *supplemental_properties;

	/* internal variables for dasher*/

	/*! inject DASHIF-LL profile service desc*/
	Bool inject_service_desc;

	/*Generate index mode instead of MPD*/
	Bool index_mode;

	/*! dasher init NTP clock in ms - GPAC internal*/
	u64 gpac_init_ntp_ms;
	/*! dasher next generation time NTP clock in ms - GPAC internal*/
	u64 gpac_next_ntp_ms;
	/*! dasher current MPD time in milliseconds - GPAC internal*/
	u64 gpac_mpd_time;

	/*! indicates the GPAC state info should be written*/
	Bool write_context;
	Bool use_gpac_ext;
	/*! indicates this is the last static serialization of a previously dynamic MPD*/
	Bool was_dynamic;
	/*! indicates the HLS variant files shall be created, otherwise temp files are used*/
	Bool create_m3u8_files;
	/*! indicates to insert clock reference in variant playlists*/
	Bool m3u8_time;
	/*! indicates  LL-HLS forced generation. 0: regular write, 1: write as byterange, 2: write as independent files*/
	u32 force_llhls_mode;
	/*! HLS extensions to append in the master playlist*/
	u32 nb_hls_ext_master;
	const char **hls_ext_master;
	/*! if true inject EXT-X-PRELOAD-HINT*/
	Bool llhls_preload;
	/*! if true inject EXT-X-RENDITION-REPORT*/
	Bool llhls_rendition_reports;
	/*! user-defined  PART-HOLD-BACK, auto computed if <=0*/
	Double llhls_part_holdback;
	//als absolute url flag
	u32 hls_abs_url;
	Bool m3u8_use_repid;
	Bool hls_audio_primary;

	/*! requested segment duration for index mode */
	u32 segment_duration;
	char *segment_template;
	Bool allow_empty_reps;
} GF_MPD;

/*! parses an MPD Element (and subtree) from DOM
\param root root of DOM parsing result
\param mpd MPD structure to fill
\param base_url base URL of the DOM document
\return error if any
*/
GF_Err gf_mpd_init_from_dom(GF_XMLNode *root, GF_MPD *mpd, const char *base_url);
/*! parses an MPD Period element (and subtree) from DOM
\param root root of DOM parsing result
\param mpd MPD structure to fill
\param base_url base URL of the DOM document
\return error if any
*/
GF_Err gf_mpd_complete_from_dom(GF_XMLNode *root, GF_MPD *mpd, const char *base_url);
/*! MPD constructor
\return a new MPD*/
GF_MPD *gf_mpd_new();
/*! MPD destructor
\param mpd the target MPD*/
void gf_mpd_del(GF_MPD *mpd);
/*! frees a GF_MPD_SegmentURL structure (type-casted to void *)
\param ptr the target GF_MPD_SegmentURL
*/
void gf_mpd_segment_url_free(void *ptr);
/*! frees a GF_MPD_SegmentBase structure (type-casted to void *)
\param ptr the target GF_MPD_SegmentBase
*/
void gf_mpd_segment_base_free(void *ptr);
/*! parses a new GF_MPD_SegmentURL from its DOM description
\param container the container list where to insert the segment URL
\param root the DOM description of the segment URL
*/
void gf_mpd_parse_segment_url(GF_List *container, GF_XMLNode *root);

/*! parses a xsDateTime
\param attr the date time value
\return value as UTC timestamp
*/
u64 gf_mpd_parse_date(const char * const attr);

/*! frees a GF_MPD_URL structure (type-casted to void *)
\param _item the target GF_MPD_URL
*/
void gf_mpd_url_free(void *_item);

/*! MPD Period constructor
\return a new MPD Period*/
GF_MPD_Period *gf_mpd_period_new();
/*! MPD Period destructor
\param _item the MPD Period to free*/
void gf_mpd_period_free(void *_item);
/*! writes an MPD to a file stream
\param mpd the target MPD to write
\param out the target file object
\param compact if set, removes all new line and indentation in the output
\return error if any
*/
GF_Err gf_mpd_write(GF_MPD const * const mpd, FILE *out, Bool compact);
/*! writes an MPD to a local file
\param mpd the target MPD to write
\param file_name the target file name
\return error if any
*/
GF_Err gf_mpd_write_file(GF_MPD const * const mpd, const char *file_name);

/*! write mode for M3U8 */
typedef enum
{
	/*! write both master and child playlists */
	GF_M3U8_WRITE_ALL=0,
	/*! write master  playlist only */
	GF_M3U8_WRITE_MASTER,
	/*! write child playlist only */
	GF_M3U8_WRITE_CHILD,
} GF_M3U8WriteMode;

/*! writes an MPD to a m3u8 playlist
\param mpd the target MPD to write
\param out the target file object
\param m3u8_name the base m3u8 name to use (needed when generating variant playlist file names)
\param period the MPD period for that m3u8
\param mode the write operation desired
\return error if any
*/
GF_Err gf_mpd_write_m3u8_master_playlist(GF_MPD const * const mpd, FILE *out, const char* m3u8_name, GF_MPD_Period *period, GF_M3U8WriteMode mode);


/*! parses an MPD Period and appends it to the MPD period list
\param mpd the target MPD to write
\param root the DOM element describing the period
\return error if any
*/
GF_Err gf_mpd_parse_period(GF_MPD *mpd, GF_XMLNode *root);

/*! creates a new MPD descriptor
\param id the descriptor ID, may be NULL
\param uri the descriptor schemeid URI, mandatory
\param value the descriptor value, may be NULL
\return the new descriptor or NULL if error
*/
GF_MPD_Descriptor *gf_mpd_descriptor_new(const char *id, const char *uri, const char *value);

/*! creates a new MPD AdaptationSet
\return the new GF_MPD_AdaptationSet or NULL if error
*/
GF_MPD_AdaptationSet *gf_mpd_adaptation_set_new();

/*! file download abstraction object*/
typedef struct _gf_file_get GF_FileDownload;
/*! file download abstraction object*/
struct _gf_file_get
{
	/*! callback function for session creation, fetches the given URL*/
	GF_Err (*new_session)(GF_FileDownload *getter, char *url);
	/*! callback function to destroy the session*/
	void (*del_session)(GF_FileDownload *getter);
	/*! callback function to get the local file name*/
	const char *(*get_cache_name)(GF_FileDownload *getter);
	/*! callback function to get download status - returns:
		- GF_OK: session is done
		- GF_NOT_READY: session is in progress
		- Any other error: session done with error*/
	GF_Err (*get_status)(GF_FileDownload *getter);
	/*! user private*/
	void *udta;
	/*! created by user after new_session*/
	void *session;
};

/*! converts M3U8 to MPD - getter is optional (download will still be processed if NULL)
\param m3u8_file the path to the local m3u8 master playlist file
\param base_url the original URL of the file if any
\param mpd_file the destination MPD file, or NULL when filling an MPD structure
\param reload_count number of times the manifest was reloaded
\param mimeTypeForM3U8Segments default mime type for the segments in case not found in the m3u8
\param do_import if GF_TRUE, will try to load the media segments to extract more info
\param use_mpd_templates if GF_TRUE, will use MPD SegmentTemplate instead of SegmentList (only if parse_sub_playlist is GF_TRUE)
\param use_segment_timeline if GF_TRUE, uses SegmentTimeline to describe the varying duration of segments
\param getter HTTP interface object
\param mpd MPD structure to fill, or NULL if converting to file
\param parse_sub_playlist if GF_TRUE, parses sub playlists, otherwise only the master playlist is parsed and xlink are added on each representation to the target m3u8 sub playlist
\param keep_files if GF_TRUE, will not delete any files downloaded in the conversion process
\return error if any
*/
GF_Err gf_m3u8_to_mpd(const char *m3u8_file, const char *base_url, const char *mpd_file, u32 reload_count, char *mimeTypeForM3U8Segments, Bool do_import, Bool use_mpd_templates, Bool use_segment_timeline, GF_FileDownload *getter, GF_MPD *mpd, Bool parse_sub_playlist, Bool keep_files);

/*! solves an m3u8 xlink on a representation, and fills the SegmentList accordingly
\param rep the target representation
\param base_url base URL of master manifest (representation xlink is likely relative to this URL)
\param getter HTTP interface object
\param is_static set to GF_TRUE if the variant subplaylist is on demand
\param duration set to the duration of the parsed subplaylist
\param signature SHA1 digest of last solved version, updated if changed
\return error if any, GF_EOS if no changes
*/
GF_Err gf_m3u8_solve_representation_xlink(GF_MPD_Representation *rep, const char *base_url, GF_FileDownload *getter, Bool *is_static, u64 *duration, u8 signature[GF_SHA1_DIGEST_SIZE]);

/*! creates a segment list from a remote segment list DOM root
\param mpd the target MPD to write
\param root the DOM element describing the segment list
\return a new segment list, or NULL if error
*/
GF_MPD_SegmentList *gf_mpd_solve_segment_list_xlink(GF_MPD *mpd, GF_XMLNode *root);

/*! inits an MPD from a smooth manifest root node
\param root the root node of a smooth manifest
\param mpd the MPD to fill
\param default_base_url the default URL of the smooth manifest
\return error if any
*/
GF_Err gf_mpd_init_smooth_from_dom(GF_XMLNode *root, GF_MPD *mpd, const char *default_base_url);

/*! deletes a segment list
\param segment_list the segment list to delete*/
void gf_mpd_delete_segment_list(GF_MPD_SegmentList *segment_list);

/*! deletes a list content and optionally destructs the list
\param list the target list
\param __destructor the destructor function to use to destroy list items
\param reset_only  if GF_TRUE, does not destroy the target list
*/
void gf_mpd_del_list(GF_List *list, void (*__destructor)(void *), Bool reset_only);
/*! deletes a GF_MPD_Descriptor object
\param item the descriptor to delete
*/
void gf_mpd_descriptor_free(void *item);

/*! splits all adaptation sets of a source MPD, creating one adaptation set per quality of each orgingal adaptation sets
\param mpd the target MPD
\return error if any
*/
GF_Err gf_mpd_split_adaptation_sets(GF_MPD *mpd);

/*! converts a smooth manifest (local file) to an MPD
\param smooth_file local path to the smooth manifest
\param mpd MPD structure to fill
\param default_base_url the default URL of the smooth manifest
\return error if any
*/
GF_Err gf_mpd_smooth_to_mpd(char * smooth_file, GF_MPD *mpd, const char *default_base_url);

/*! get the number of base URLs for the given representation. This cumuluates all base URLs at MPD, period, AdaptationSet and Representation levels
\param mpd the target MPD
\param period the target period
\param set the target adaptation set
\param rep the target representation
\return the number of base URLs
*/
u32 gf_mpd_get_base_url_count(GF_MPD *mpd, GF_MPD_Period *period, GF_MPD_AdaptationSet *set, GF_MPD_Representation *rep);

/*! MPD URL resolutio mode*/
typedef enum
{
	/*resolves template for a media segment*/
	GF_MPD_RESOLVE_URL_MEDIA,
	/*resolves template for an init segment*/
	GF_MPD_RESOLVE_URL_INIT,
	/*resolves template for an index segment*/
	GF_MPD_RESOLVE_URL_INDEX,
	/*! same as GF_MPD_RESOLVE_URL_MEDIA but does not replace $Time$ and $Number$*/
	GF_MPD_RESOLVE_URL_MEDIA_TEMPLATE,
	/*! same as GF_MPD_RESOLVE_URL_MEDIA but does not use startNumber*/
	GF_MPD_RESOLVE_URL_MEDIA_NOSTART,
	/*! same as GF_MPD_RESOLVE_URL_MEDIA_TEMPLATE but ignores base URL*/
	GF_MPD_RESOLVE_URL_MEDIA_TEMPLATE_NO_BASE,
} GF_MPD_URLResolveType;

/*! resolves a URL based for a given segment, based on the MPD url, the type of resolution
\param mpd the target MPD
\param rep the target Representation
\param set the target AdaptationSet
\param period the target Period
\param mpd_url the original URL of the MPD
\param base_url_index 0-based index of the baseURL to use
\param resolve_type the type of URL resolution desired
\param item_index the index of the target segment (startNumber based)
\param nb_segments_removed number of segments removed when purging the MPD after updates (can be 0). The start number will be offset by this value
\param out_url set to the resolved URL, to be freed by caller
\param out_range_start set to the resolved start range, 0 if no range
\param out_range_end set to the resolved end range, 0 if no range
\param segment_duration set to the resolved segment duartion, 0 if unknown
\param is_in_base_url set to GF_TRUE if the resuloved URL is a sub-part of the baseURL (optional, may be NULL)
\param out_key_url set to the key URL for the segment for HLS (optional, may be NULL)
\param key_iv set to the key IV for the segment for HLS (optional, may be NULL)
\param out_start_number set to the start_number used (optional, may be NULL)

\return error if any
*/
GF_Err gf_mpd_resolve_url(GF_MPD *mpd, GF_MPD_Representation *rep, GF_MPD_AdaptationSet *set, GF_MPD_Period *period, const char *mpd_url, u32 base_url_index, GF_MPD_URLResolveType resolve_type, u32 item_index, u32 nb_segments_removed,
                          char **out_url, u64 *out_range_start, u64 *out_range_end, u64 *segment_duration, Bool *is_in_base_url, char **out_key_url, bin128 *key_iv, u32 *out_start_number);

/*! get duration of the presentation
\param mpd the target MPD
\return the duration in seconds
*/
Double gf_mpd_get_duration(GF_MPD *mpd);

/*! gets the duration of media segments
\param rep the target Representation
\param set the target AdaptationSet
\param period the target Period
\param out_duration set to the average media segment duration
\param out_timescale set to the MPD timescale used by this representation
\param out_pts_offset set to the presentation time offset if any (optional, may be NULL)
\param out_segment_timeline set to the segment timeline description if any (optional, may be NULL)
*/
void gf_mpd_resolve_segment_duration(GF_MPD_Representation *rep, GF_MPD_AdaptationSet *set, GF_MPD_Period *period, u64 *out_duration, u32 *out_timescale, u64 *out_pts_offset, GF_MPD_SegmentTimeline **out_segment_timeline);

/*! gets the start_time from the segment index of a period/set/rep
\param in_segment_index the index of the target segment (startNumber based)
\param in_period the target Period
\param in_set the target AdaptationSet
\param in_rep the target Representation
\param out_segment_start_time set to the MPD start time of the segment
\param out_opt_segment_duration set to the segment duration (optional, may be NULL)
\param out_opt_scale set to the MPD timescale for this segment (optional, may be NULL)
\return error if any
*/
GF_Err gf_mpd_get_segment_start_time_with_timescale(s32 in_segment_index,
	GF_MPD_Period const * const in_period, GF_MPD_AdaptationSet const * const in_set, GF_MPD_Representation const * const in_rep,
	u64 *out_segment_start_time, u64 *out_opt_segment_duration, u32 *out_opt_scale);

/*! MPD seek mode*/
typedef enum {
	/*! will return the segment containing the requested time*/
	MPD_SEEK_PREV,
	/*! will return the nearest segment start time, may be the previous or the next one*/
	MPD_SEEK_NEAREST,
} MPDSeekMode;

/*! returns the segment index in the given period for the given time
\param seek_time the desired time in seconds
\param seek_mode the desired seek mode
\param in_period the target Period
\param in_set the target AdaptationSet
\param in_rep the target Representation
\param out_segment_index the corresponding segment index
\param out_opt_seek_time the corresponding seek time (start time of segment in seconds) (optional, may be NULL)
\return error if any
*/
GF_Err gf_mpd_seek_in_period(Double seek_time, MPDSeekMode seek_mode,
	GF_MPD_Period const * const in_period, GF_MPD_AdaptationSet const * const in_set, GF_MPD_Representation const * const in_rep,
	u32 *out_segment_index, Double *out_opt_seek_time);

/*! deletes a GF_MPD_BaseURL structure (type-casted to void *)
\param _item the GF_MPD_BaseURL to free
*/
void gf_mpd_base_url_free(void *_item);
/*! creates a new GF_MPD_Representation
\return the new representation*/
GF_MPD_Representation *gf_mpd_representation_new();
/*! deletes a GF_MPD_Representation structure (type-casted to void *)
\param _item the GF_MPD_Representation to free
*/
void gf_mpd_representation_free(void *_item);

/*! creates a new GF_MPD_SegmentTimeline
\return the new segment timeline*/
GF_MPD_SegmentTimeline *gf_mpd_segmentimeline_new();

/*! DASHer cues information*/
typedef struct
{
	/*! target splice point sample number (1-based), or 0 if not set*/
	u32 sample_num;
	/*! target splice point dts in cues timescale */
	u64 dts;
	/*! target splice point cts in cues timescale */
	s64 cts;
	/*! internal flag indicating if the cues is processed or not */
	Bool is_processed;
} GF_DASHCueInfo;

/*! loads a cue file and allocates cues as needed
\param cues_file the XML cue file to load
\param stream_id the ID of the stream for which we load cues (typically, TrackID or GF_PROP_PID_ID)
\param cues_timescale set to the timescale used in the cues document
\param use_edit_list set to GF_TRUE if the cts values of cues have edit list applied (i.e. are ISOBMFF presentation times)
\param ts_offset set to the timestamp offset to subtract from DTS/CTS values
\param out_cues set to a newly allocated list of cues, to free by the caller
\param nb_cues set to the number of cues parsed
\return error if any
*/
GF_Err gf_mpd_load_cues(const char *cues_file, u32 stream_id, u32 *cues_timescale, Bool *use_edit_list, s32 *ts_offset, GF_DASHCueInfo **out_cues, u32 *nb_cues);

/*! gets first MPD descriptor from descriptor list for a given scheme_id
\param desclist list of MPD Descriptors
\param scheme_id scheme ID to look for
\return descriptor if found, NUL otherwise
*/
GF_MPD_Descriptor *gf_mpd_get_descriptor(GF_List *desclist, char *scheme_id);

/*! @} */
#endif /*GPAC_DISABLE_MPD*/

#endif // _MPD_H_
