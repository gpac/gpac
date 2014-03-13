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

/*TODO*/
typedef struct 
{
	u32 dummy;
} GF_MPD_Metrics;

/*TODO*/
typedef struct 
{
	u32 dummy;
} GF_MPD_ContentComponent;

/*TODO*/
typedef struct 
{
	char *scheme_id_uri; /*MANDATORY*/
	char *value;
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
} GF_MPD_BaseURL;

typedef struct 
{
    char *sourceURL;
    GF_MPD_ByteRange *byte_range;
} GF_MPD_URL;

typedef struct 
{
    u32 num, den;
} GF_MPD_Fractional;


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

/*WARNING: duration is expressed in GF_MPD_SEGMENT_BASE@timescale unit*/
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
} GF_MPD_SegmentURL;

typedef struct 
{
	GF_MPD_MULTIPLE_SEGMENT_BASE	
	/*list of segments - can be NULL if no segment*/
	GF_List *segment_URLs;

	char *xlink_href;
	Bool xlink_actuate_on_load;
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
	Bool starts_with_sap;	\
	Double max_playout_rate;	\
	Bool coding_dependency;	\
	GF_MPD_ScanType scan_type;	\
	GF_List *frame_packing;	\
	GF_List *audio_channels;	\
	GF_List *content_protection;	\


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
	u64 init_start_range, init_end_range;
} GF_DASH_RepresentationPlayback;

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

	/*index of the next enhancement representation plus 1, 0 is reserved in case of the highest representation*/
	u32 enhancement_rep_index_plus_one;

	/*GPAC playback implementation*/
	GF_DASH_RepresentationPlayback playback;
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
} GF_MPD_AdaptationSet;


typedef struct 
{
	char *ID;
    u32 start; /* expressed in ms, relative to the start of the MPD */
	u32 duration; /* expressed in ms*/
	Bool bitstream_switching;

	GF_List *base_URLs;
	GF_MPD_SegmentBase *segment_base;
	GF_MPD_SegmentList *segment_list;
	GF_MPD_SegmentTemplate *segment_template;

	GF_List *adaptation_sets;
	GF_List *subsets;
	char *xlink_href;
	Bool xlink_actuate_on_load;
} GF_MPD_Period;

typedef struct 
{
    char *lang;
    char *title;
    char *source;
    char *copyright;
    char *more_info_url;
} GF_MPD_ProgramInfo;


typedef enum {
    GF_MPD_TYPE_STATIC,
    GF_MPD_TYPE_DYNAMIC,
} GF_MPD_Type;

typedef struct {
    char *ID;
    char *profiles;	/*MANDATORY*/
    GF_MPD_Type type;
	u64 availabilityStartTime; /* expressed in milliseconds */	/*MANDATORY if type=dynamic*/
	u64 availabilityEndTime;/* expressed in milliseconds */	
	u64 publishTime;/* expressed in milliseconds */	
    u32 media_presentation_duration; /* expressed in milliseconds */	/*MANDATORY if type=static*/
    u32 minimum_update_period; /* expressed in milliseconds */
    u32 min_buffer_time; /* expressed in milliseconds */	/*MANDATORY*/

	u32 time_shift_buffer_depth; /* expressed in milliseconds */
	u32 suggested_presentaton_delay; /* expressed in milliseconds */

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
	const char *xml_namespace;
} GF_MPD;

GF_Err gf_mpd_init_from_dom(GF_XMLNode *root, GF_MPD *mpd, const char *base_url);

GF_MPD *gf_mpd_new();
void gf_mpd_del(GF_MPD *mpd);
/*frees a GF_MPD_SegmentURL structure (type-casted to void *)*/
void gf_mpd_segment_url_free(void *ptr);
void gf_mpd_segment_base_free(void *ptr);

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
GF_Err gf_m3u8_to_mpd(const char *m3u8_file, const char *base_url, const char *mpd_file, u32 reload_count, char *mimeTypeForM3U8Segments, Bool do_import, Bool use_mpd_templates, GF_FileDownload *getter);

#endif // _MPD_H_

