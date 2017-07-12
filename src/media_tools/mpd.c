/**
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre, Cyril COncolato
 *			Copyright (c) Telecom ParisTech 2000-2012
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

#include <gpac/internal/mpd.h>
#include <gpac/download.h>
#include <gpac/internal/m3u8.h>
#include <gpac/network.h>
#include <gpac/maths.h>

#ifdef _WIN32_WCE
#include <winbase.h>
#else
/*for mktime*/
#include <time.h>
#endif

#ifndef GPAC_DISABLE_CORE_TOOLS

static Bool gf_mpd_parse_bool(char *attr)
{
	if (!strcmp(attr, "true")) return 1;
	if (!strcmp(attr, "1")) return 1;
	return 0;
}

static char *gf_mpd_parse_string(char *attr)
{
	return gf_strdup(attr);
}


static Bool gf_mpd_valid_child(GF_MPD *mpd, GF_XMLNode *child)
{
	if (child->type != GF_XML_NODE_TYPE) return 0;
	if (!mpd->xml_namespace && !child->ns) return 1;
	if (mpd->xml_namespace && child->ns && !strcmp(mpd->xml_namespace, child->ns)) return 1;
	return 0;
}

static Bool gf_mpd_is_known_descriptor(GF_XMLNode *child)
{
	if (!strcmp(child->name, "FramePacking")  ||
	!strcmp(child->name, "AudioChannelConfiguration") ||
	!strcmp(child->name, "ContentProtection") ||
	!strcmp(child->name, "EssentialProperty") ||
	!strcmp(child->name, "SupplementalProperty")){
		return GF_TRUE;
	}
	else{
		return GF_FALSE;
	}
}

static void gf_mpd_parse_other_descriptors(GF_XMLNode *child, GF_List *other_desc)
{
	if(!gf_mpd_is_known_descriptor(child)){
		char *descriptors=gf_xml_dom_serialize(child,GF_FALSE);
		GF_MPD_other_descriptors *Desc;
		GF_SAFEALLOC(Desc,GF_MPD_other_descriptors);
		Desc->xml_desc=descriptors;
		gf_list_add(other_desc, Desc);
	}
}

static char *gf_mpd_parse_text_content(GF_XMLNode *child)
{
	u32 child_index = 0;
	while (1) {
		child = gf_list_get(child->content, child_index);
		if (!child) {
			break;
		} else if (child->type == GF_XML_TEXT_TYPE) {
			return gf_mpd_parse_string(child->name);
		}
		child_index++;
	}
	return NULL;
}

static u32 gf_mpd_parse_int(char *attr)
{
	return atoi(attr);
}

static u64 gf_mpd_parse_long_int(char *attr)
{
	u64 longint;
	sscanf(attr, LLU, &longint);
	return longint;
}

static Double gf_mpd_parse_double(char *attr)
{
	return atof(attr);
}

static GF_MPD_Fractional *gf_mpd_parse_frac(char *attr)
{
	GF_MPD_Fractional *res;
	GF_SAFEALLOC(res, GF_MPD_Fractional);
	sscanf(attr, "%d:%d", &res->num, &res->den);
	return res;
}

static u64 gf_mpd_parse_date(char *attr)
{
	return gf_net_parse_date(attr);
}

static u64 gf_mpd_parse_duration(char *duration)
{
	u32 i;
	char *sep1, *sep2;
	u32 h, m;
	double s;
	const char *startT;
	if (!duration) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD] Error parsing duration: no value indicated\n"));
		return 0;
	}
	i = 0;
	while (1) {
		if (duration[i] == ' ') i++;
		else if (duration[i] == 0) return 0;
		else {
			break;
		}
	}
	if (duration[i] != 'P') {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD] Error parsing duration: no value indicated\n"));
		return 0;
	}
	startT = strchr(duration+1, 'T');

	if (duration[i+1] == 0) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD] Error parsing duration: no value indicated\n"));
		return 0;
	}
	if (! startT) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD] Error parsing duration: no Time section found\n"));
		return 0;
	}

	h = m = 0;
	s = 0;
	if (NULL != (sep1 = strchr(startT+1, 'H'))) {
		*sep1 = 0;
		h = atoi(duration+i+2);
		*sep1 = 'H';
		sep1++;
	} else {
		sep1 = (char *) startT+1;
	}
	if (NULL != (sep2 = strchr(sep1, 'M'))) {
		*sep2 = 0;
		m = atoi(sep1);
		*sep2 = 'M';
		sep2++;
	} else {
		sep2 = sep1;
	}
	if (NULL != (sep1 = strchr(sep2, 'S'))) {
		*sep1 = 0;
		s = atof(sep2);
		*sep1 = 'S';
	}
	return (u64)((h*3600+m*60+s)*(u64)1000);
}

static u32 gf_mpd_parse_duration_u32(char *duration)
{
	u64 dur = gf_mpd_parse_duration(duration);
	if (dur <= GF_UINT_MAX) {
		return (u32)dur;
	} else {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD] Parsed duration "LLU" doesn't fit on 32 bits! Setting to the 32 bits max.\n", dur));
		return GF_UINT_MAX;
	}
}

static GF_MPD_ByteRange *gf_mpd_parse_byte_range(char *attr)
{
	GF_MPD_ByteRange *br;
	GF_SAFEALLOC(br, GF_MPD_ByteRange);
	sscanf(attr, LLD"-"LLD, &br->start_range, &br->end_range);
	return br;
}

static GF_Err gf_mpd_parse_location(GF_MPD *mpd, GF_XMLNode *child)
{
	char *str = gf_mpd_parse_text_content(child);
	if (str) return gf_list_add(mpd->locations, str);
	return GF_OK;
}

static GF_Err gf_mpd_parse_metrics(GF_MPD *mpd, GF_XMLNode *child)
{
	GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD] Metrics not implemented yet\n"));
	return GF_OK;
}

GF_Err gf_mpd_parse_base_url(GF_List *container, GF_XMLNode *node)
{
	u32 i;
	GF_Err e;
	GF_XMLAttribute *att;
	GF_MPD_BaseURL *url;
	GF_SAFEALLOC(url, GF_MPD_BaseURL);
	if (! url) return GF_OUT_OF_MEM;
	e = gf_list_add(container, url);
	if (e) return GF_OUT_OF_MEM;

	i = 0;
	while ( (att = gf_list_enum(node->attributes, &i))) {
		if (!strcmp(att->name, "serviceLocation")) url->service_location = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "byteRange")) url->byte_range = gf_mpd_parse_byte_range(att->value);
	}
	url->URL = gf_mpd_parse_text_content(node);
	return GF_OK;
}

static GF_Err gf_mpd_parse_program_info(GF_MPD *mpd, GF_XMLNode *root)
{
	GF_MPD_ProgramInfo *info;
	u32 att_index, child_index;
	GF_XMLAttribute *att;
	GF_XMLNode *child;

	GF_SAFEALLOC(info, GF_MPD_ProgramInfo);
	if (!info) return GF_OUT_OF_MEM;

	att_index = 0;
	while (1) {
		att = gf_list_get(root->attributes, att_index);
		if (!att) {
			break;
		} else if (!strcmp(att->name, "moreInformationURL")) {
			info->more_info_url = gf_mpd_parse_string(att->value);
		} else if (!strcmp(att->name, "lang")) {
			info->lang = gf_mpd_parse_string(att->value);
		}
		att_index++;
	}

	child_index = 0;
	while (1) {
		child = gf_list_get(root->content, child_index);
		if (!child) {
			break;
		} else if (child->type == GF_XML_NODE_TYPE) {
			if (!strcmp(child->name, "Title")) {
				GF_XMLNode *data_node = gf_list_get(child->content, 0);
				if (data_node && data_node->type == GF_XML_TEXT_TYPE) {
					info->title = gf_strdup(data_node->name);
				}
			} else if (!strcmp(child->name, "Source")) {
				GF_XMLNode *data_node = gf_list_get(child->content, 0);
				if (data_node && data_node->type == GF_XML_TEXT_TYPE) {
					info->source = gf_strdup(data_node->name);
				}
			} else if (!strcmp(child->name, "Copyright")) {
				GF_XMLNode *data_node = gf_list_get(child->content, 0);
				if (data_node && data_node->type == GF_XML_TEXT_TYPE) {
					info->copyright = gf_strdup(data_node->name);
				}
			}
		}
		child_index++;
	}
	return gf_list_add(mpd->program_infos, info);
}

static GF_MPD_URL *gf_mpd_parse_url(GF_XMLNode *root)
{
	u32 i;
	GF_MPD_URL *url;
	GF_XMLAttribute *att;

	GF_SAFEALLOC(url, GF_MPD_URL);
	if (!url) return NULL;

	i = 0;
	while ( (att = gf_list_enum(root->attributes, &i)) ) {
		if (!strcmp(att->name, "sourceURL")) url->sourceURL = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "range")) url->byte_range = gf_mpd_parse_byte_range(att->value);
	}
	return url;
}

static void gf_mpd_parse_segment_base_generic(GF_MPD *mpd, GF_MPD_SegmentBase *seg, GF_XMLNode *root)
{
	GF_XMLAttribute *att;
	GF_XMLNode *child;
	u32 i = 0;

	/*0 by default*/
	seg->time_shift_buffer_depth = 0;

	while ( (att = gf_list_enum(root->attributes, &i)) ) {
		if (!strcmp(att->name, "timescale")) seg->timescale = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "presentationTimeOffset")) seg->presentation_time_offset = gf_mpd_parse_long_int(att->value);
		else if (!strcmp(att->name, "indexRange")) seg->index_range = gf_mpd_parse_byte_range(att->value);
		else if (!strcmp(att->name, "indexRangeExact")) seg->index_range_exact = gf_mpd_parse_bool(att->value);
		else if (!strcmp(att->name, "availabilityTimeOffset")) seg->availability_time_offset = gf_mpd_parse_double(att->value);
		else if (!strcmp(att->name, "timeShiftBufferDepth")) seg->time_shift_buffer_depth = gf_mpd_parse_duration_u32(att->value);
	}

	i = 0;
	while ( (child = gf_list_enum(root->content, &i))) {
		if (!gf_mpd_valid_child(mpd, child)) continue;
		if (!strcmp(child->name, "Initialization")) seg->initialization_segment = gf_mpd_parse_url(child);
		else if (!strcmp(child->name, "RepresentationIndex")) seg->representation_index = gf_mpd_parse_url(child);
	}
}

static GF_MPD_SegmentTimeline *gf_mpd_parse_segment_timeline(GF_MPD *mpd, GF_XMLNode *root)
{
	u32 i, j;
	GF_XMLAttribute *att;
	GF_XMLNode *child;
	GF_MPD_SegmentTimeline *seg;
	GF_SAFEALLOC(seg, GF_MPD_SegmentTimeline);
	if (!seg) return NULL;
	seg->entries = gf_list_new();

	i = 0;
	while ( (child = gf_list_enum(root->content, &i))) {
		if (!gf_mpd_valid_child(mpd, child)) continue;
		if (!strcmp(child->name, "S")) {
			GF_MPD_SegmentTimelineEntry *seg_tl_ent;
			GF_SAFEALLOC(seg_tl_ent, GF_MPD_SegmentTimelineEntry);
			if (!seg_tl_ent) continue;
			gf_list_add(seg->entries, seg_tl_ent);

			j = 0;
			while ( (att = gf_list_enum(child->attributes, &j)) ) {
				if (!strcmp(att->name, "t"))
					seg_tl_ent->start_time = gf_mpd_parse_long_int(att->value);
				else if (!strcmp(att->name, "d"))
					seg_tl_ent->duration = gf_mpd_parse_int(att->value);
				else if (!strcmp(att->name, "r")) {
					seg_tl_ent->repeat_count = gf_mpd_parse_int(att->value);
					if (seg_tl_ent->repeat_count == (u32)-1)
						seg_tl_ent->repeat_count--;
				}
			}
		}
	}
	return seg;
}

static GF_MPD_SegmentBase *gf_mpd_parse_segment_base(GF_MPD *mpd, GF_XMLNode *root)
{
	GF_MPD_SegmentBase *seg;
	GF_SAFEALLOC(seg, GF_MPD_SegmentBase);
	if (!seg) return NULL;
	gf_mpd_parse_segment_base_generic(mpd, seg, root);
	return seg;
}

void gf_mpd_parse_multiple_segment_base(GF_MPD *mpd, GF_MPD_MultipleSegmentBase *seg, GF_XMLNode *root)
{
	u32 i;
	GF_XMLAttribute *att;
	GF_XMLNode *child;

	gf_mpd_parse_segment_base_generic(mpd, (GF_MPD_SegmentBase*)seg, root);
	seg->start_number = (u32) -1;

	i = 0;
	while ( (att = gf_list_enum(root->attributes, &i)) ) {
		if (!strcmp(att->name, "duration")) seg->duration = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "startNumber")) seg->start_number = gf_mpd_parse_int(att->value);
	}

	i = 0;
	while ( (child = gf_list_enum(root->content, &i))) {
		if (!gf_mpd_valid_child(mpd, child)) continue;
		if (!strcmp(child->name, "SegmentTimeline")) seg->segment_timeline = gf_mpd_parse_segment_timeline(mpd, child);
		else if (!strcmp(child->name, "BitstreamSwitching")) seg->bitstream_switching_url = gf_mpd_parse_url(child);
	}
}

GF_MPD_SegmentURL *gf_mpd_segmenturl_new(const char*media, u64 start_range, u64 end_range, const char *index, u64 idx_start_range, u64 idx_end_range)
{
	GF_MPD_SegmentURL *seg_url;
	GF_SAFEALLOC(seg_url, GF_MPD_SegmentURL);
	GF_SAFEALLOC(seg_url->media_range, GF_MPD_ByteRange);
	seg_url->media_range->start_range = start_range;
	seg_url->media_range->end_range = end_range;
	if (idx_start_range || idx_end_range) {
		GF_SAFEALLOC(seg_url->index_range, GF_MPD_ByteRange);
		seg_url->index_range->start_range = idx_start_range;
		seg_url->index_range->end_range = idx_end_range;
	}
	seg_url->media=gf_strdup(media);
	return seg_url;
}

void gf_mpd_parse_segment_url(GF_List *container, GF_XMLNode *root)
{
	u32 i;
	GF_MPD_SegmentURL *seg;
	GF_XMLAttribute *att;

	GF_SAFEALLOC(seg, GF_MPD_SegmentURL);
	if (!seg) return;
	gf_list_add(container, seg);

	i = 0;
	while ( (att = gf_list_enum(root->attributes, &i)) ) {
		if (!strcmp(att->name, "media")) seg->media = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "index")) seg->index = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "mediaRange")) seg->media_range = gf_mpd_parse_byte_range(att->value);
		else if (!strcmp(att->name, "indexRange")) seg->index_range = gf_mpd_parse_byte_range(att->value);
		//else if (!strcmp(att->name, "hls:keyMethod")) seg->key_url = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "hls:keyURL")) seg->key_url = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "hls:keyIV")) gf_bin128_parse(att->value, seg->key_iv);

	}
}

static GF_MPD_SegmentList *gf_mpd_parse_segment_list(GF_MPD *mpd, GF_XMLNode *root)
{
	u32 i;
	GF_MPD_SegmentList *seg;
	GF_XMLAttribute *att;
	GF_XMLNode *child;

	GF_SAFEALLOC(seg, GF_MPD_SegmentList);
	if (!seg) return NULL;
	seg->segment_URLs = gf_list_new();

	i = 0;
	while ( (att = gf_list_enum(root->attributes, &i)) ) {
		if (strstr(att->name, "href")) seg->xlink_href = gf_mpd_parse_string(att->value);
		else if (strstr(att->name, "actuate")) seg->xlink_actuate_on_load = !strcmp(att->value, "onLoad") ? 1 : 0;
	}
	gf_mpd_parse_multiple_segment_base(mpd, (GF_MPD_MultipleSegmentBase *)seg, root);

	i = 0;
	while ( (child = gf_list_enum(root->content, &i))) {
		if (!gf_mpd_valid_child(mpd, child)) continue;
		if (!strcmp(child->name, "SegmentURL")) gf_mpd_parse_segment_url(seg->segment_URLs, child);
	}
	if (!gf_list_count(seg->segment_URLs)) {
		gf_list_del(seg->segment_URLs);
		seg->segment_URLs = NULL;
	}
	return seg;
}

static GF_MPD_SegmentTemplate *gf_mpd_parse_segment_template(GF_MPD *mpd, GF_XMLNode *root)
{
	u32 i;
	GF_MPD_SegmentTemplate *seg;
	GF_XMLAttribute *att;

	GF_SAFEALLOC(seg, GF_MPD_SegmentTemplate);
	if (!seg) return NULL;

	i = 0;
	while ( (att = gf_list_enum(root->attributes, &i)) ) {
		if (!strcmp(att->name, "media")) seg->media = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "index")) seg->index = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "initialization") ) seg->initialization = gf_mpd_parse_string(att->value);
		else if (!stricmp(att->name, "initialisation") || !stricmp(att->name, "initialization") ) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD] Wrong spelling: got %s but expected \"initialization\" \n", att->name ));
			seg->initialization = gf_mpd_parse_string(att->value);
		}
		else if (!strcmp(att->name, "bitstreamSwitching")) seg->bitstream_switching = gf_mpd_parse_string(att->value);
	}
	gf_mpd_parse_multiple_segment_base(mpd, (GF_MPD_MultipleSegmentBase *)seg, root);
	return seg;
}

static GF_Err gf_mpd_parse_content_component(GF_List *comps, GF_XMLNode *root)
{
	u32 i;
	GF_XMLAttribute *att;
	GF_MPD_ContentComponent *comp;
	GF_SAFEALLOC(comp, GF_MPD_ContentComponent);
	if (!comp) return GF_OUT_OF_MEM;
	i = 0;
	while ((att = gf_list_enum(root->attributes, &i))) {
		if (!strcmp(att->name, "id")) comp->id = atoi(att->value);
		else if (!strcmp(att->name, "contentType")) comp->type = gf_strdup(att->value);
		else if (!strcmp(att->name, "lang")) comp->lang = gf_strdup(att->value);
	}
	gf_list_add(comps, comp);
	return GF_OK;
}

#define MPD_STORE_EXTENSION_ATTR(_elem)	\
			if (!_elem->attributes) _elem->attributes = gf_list_new();	\
			i--;	\
			gf_list_rem(root->attributes, i);	\
			gf_list_add(_elem->attributes, att);	\
 
#define MPD_STORE_EXTENSION_NODE(_elem)	\
		if (!_elem->children) _elem->children = gf_list_new();	\
		i--;	\
		gf_list_rem(root->content, i);	\
		gf_list_add(_elem->children, child);	\
 
static GF_Err gf_mpd_parse_descriptor(GF_List *container, GF_XMLNode *root)
{
	GF_XMLAttribute *att;
	GF_XMLNode *child;
	GF_MPD_Descriptor *mpd_desc;
	u32 i = 0;

	GF_SAFEALLOC(mpd_desc, GF_MPD_Descriptor);
	if (!mpd_desc) return GF_OUT_OF_MEM;
	
	while ( (att = gf_list_enum(root->attributes, &i)) ) {
		if (!strcmp(att->name, "schemeIdUri")) mpd_desc->scheme_id_uri = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "value")) mpd_desc->value = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "id")) mpd_desc->id = gf_mpd_parse_string(att->value);
		else {
			MPD_STORE_EXTENSION_ATTR(mpd_desc)
		}
	}
	gf_list_add(container, mpd_desc);

	i = 0;
	while ( (child = gf_list_enum(root->content, &i))) {
		if (child->type != GF_XML_NODE_TYPE) continue;

		MPD_STORE_EXTENSION_NODE(mpd_desc)

	}

	return GF_OK;
}

static void gf_mpd_parse_common_representation(GF_MPD *mpd, GF_MPD_CommonAttributes *com, GF_XMLNode *root)
{
	GF_XMLAttribute *att;
	GF_XMLNode *child;
	u32 i = 0;

	com->max_playout_rate = 1.0;

	while ( (att = gf_list_enum(root->attributes, &i)) ) {
		if (!strcmp(att->name, "profiles")) com->profiles = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "width")) com->width = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "height")) com->height = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "sar")) {
			if (com->sar) gf_free(com->sar);
			com->sar = gf_mpd_parse_frac(att->value);
		}
		else if (!strcmp(att->name, "frameRate")) {
			if (com->framerate) gf_free(com->framerate);
			com->framerate = gf_mpd_parse_frac(att->value);
		}
		else if (!strcmp(att->name, "audioSamplingRate")) com->samplerate = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "mimeType")) com->mime_type = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "segmentProfiles")) com->segmentProfiles = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "codecs")) com->codecs = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "maximumSAPPeriod")) com->maximum_sap_period = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "startWithSAP")) {
			if (!strcmp(att->value, "false")) com->starts_with_sap = 0;
			else com->starts_with_sap = gf_mpd_parse_int(att->value);
		}
		else if (!strcmp(att->name, "maxPlayoutRate")) com->max_playout_rate = gf_mpd_parse_double(att->value);
		else if (!strcmp(att->name, "codingDependency")) com->coding_dependency = gf_mpd_parse_bool(att->value);
		else if (!strcmp(att->name, "scanType")) {
			if (!strcmp(att->value, "progressive")) com->scan_type = GF_MPD_SCANTYPE_PROGRESSIVE;
			else if (!strcmp(att->value, "interlaced")) com->scan_type = GF_MPD_SCANTYPE_INTERLACED;
		}
	}

	i = 0;
	while ( (child = gf_list_enum(root->content, &i))) {
		if (!gf_mpd_valid_child(mpd, child)) continue;
		if (!strcmp(child->name, "FramePacking")) {
			gf_mpd_parse_descriptor(com->frame_packing, child);
		}
		else if (!strcmp(child->name, "AudioChannelConfiguration")) {
			gf_mpd_parse_descriptor(com->audio_channels, child);
		}
		else if (!strcmp(child->name, "ContentProtection")) {
			gf_mpd_parse_descriptor(com->content_protection, child);
		}
		else if (!strcmp(child->name, "EssentialProperty")) {
			gf_mpd_parse_descriptor(com->essential_properties, child);
		}
		else if (!strcmp(child->name, "SupplementalProperty")) {
			gf_mpd_parse_descriptor(com->supplemental_properties, child);
		}
	}
}

static void gf_mpd_init_common_attributes(GF_MPD_CommonAttributes *com)
{
	com->audio_channels = gf_list_new();
	com->content_protection = gf_list_new();
	com->essential_properties = gf_list_new();
	com->supplemental_properties = gf_list_new();
	com->frame_packing = gf_list_new();
	com->max_playout_rate = 1.0;
}

GF_MPD_Representation *gf_mpd_representation_new()
{
	GF_MPD_Representation *rep;
	GF_SAFEALLOC(rep, GF_MPD_Representation);
	if (!rep) return NULL;
	gf_mpd_init_common_attributes((GF_MPD_CommonAttributes *)rep);
	rep->base_URLs = gf_list_new();
	rep->sub_representations = gf_list_new();
	rep->other_descriptors = gf_list_new();
	return rep;
}

static GF_Err gf_mpd_parse_representation(GF_MPD *mpd, GF_List *container, GF_XMLNode *root)
{
	u32 i;
	GF_MPD_Representation *rep;
	GF_XMLAttribute *att;
	GF_XMLNode *child;
	GF_Err e;

	rep = gf_mpd_representation_new();
	e = gf_list_add(container, rep);
	if (e) return e;

	i = 0;
	while ( (att = gf_list_enum(root->attributes, &i)) ) {
		if (!strcmp(att->name, "id")) rep->id = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "bandwidth")) rep->bandwidth = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "qualityRanking")) rep->quality_ranking = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "dependencyId")) rep->dependency_id = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "mediaStreamStructureId")) rep->media_stream_structure_id = gf_mpd_parse_string(att->value);
	}
	gf_mpd_parse_common_representation(mpd, (GF_MPD_CommonAttributes*)rep, root);

	i = 0;
	while ( (child = gf_list_enum(root->content, &i))) {
		if (!gf_mpd_valid_child(mpd, child)) continue;
		if (!strcmp(child->name, "BaseURL")) {
			e = gf_mpd_parse_base_url(rep->base_URLs, child);
			if (e) return e;
		}
		else if (!strcmp(child->name, "SegmentBase")) {
			rep->segment_base = gf_mpd_parse_segment_base(mpd, child);
		}
		else if (!strcmp(child->name, "SegmentList")) {
			rep->segment_list = gf_mpd_parse_segment_list(mpd, child);
		}
		else if (!strcmp(child->name, "SegmentTemplate")) {
			rep->segment_template = gf_mpd_parse_segment_template(mpd, child);
		}
		else if (!strcmp(child->name, "SubRepresentation")) {
			/*TODO
						e = gf_mpd_parse_subrepresentation(rep->sub_representations, child);
						if (e) return e;
			*/
		}
		else{
			/*We'll be assuming here that any unrecognized element is a representation level
			  *descriptor*/
			gf_mpd_parse_other_descriptors(child,rep->other_descriptors);

		}
	}
	return GF_OK;
}

GF_MPD_AdaptationSet *gf_mpd_adaptation_set_new() {
	GF_MPD_AdaptationSet *set;
	GF_SAFEALLOC(set, GF_MPD_AdaptationSet);
	if (!set) return NULL;
	gf_mpd_init_common_attributes((GF_MPD_CommonAttributes *)set);
	set->accessibility = gf_list_new();
	set->role = gf_list_new();
	set->rating = gf_list_new();
	set->viewpoint = gf_list_new();
	set->content_component = gf_list_new();
	set->base_URLs = gf_list_new();
	set->representations = gf_list_new();
	set->other_descriptors = gf_list_new();
	GF_SAFEALLOC(set->par, GF_MPD_Fractional);
	/*assign default ID and group*/
	set->group = -1;
	return set;
}

static GF_Err gf_mpd_parse_adaptation_set(GF_MPD *mpd, GF_List *container, GF_XMLNode *root)
{
	u32 i;
	GF_MPD_AdaptationSet *set;
	GF_XMLAttribute *att;
	GF_XMLNode *child;
	GF_Err e;

	set = gf_mpd_adaptation_set_new();
	if (!set) return GF_OUT_OF_MEM;

	e = gf_list_add(container, set);
	if (e) return e;

	i = 0;
	while ( (att = gf_list_enum(root->attributes, &i)) ) {
		if (strstr(att->name, "href")) set->xlink_href = gf_mpd_parse_string(att->value);
		else if (strstr(att->name, "actuate")) set->xlink_actuate_on_load = !strcmp(att->value, "onLoad") ? GF_TRUE : GF_FALSE;
		else if (!strcmp(att->name, "id")) set->id = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "group")) set->group = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "lang")) set->lang = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "contentType")) set->content_type = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "par")) {
			if (set->par) gf_free(set->par);
			set->par = gf_mpd_parse_frac(att->value);
		}
		else if (!strcmp(att->name, "minBandwidth")) set->min_bandwidth = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "maxBandwidth")) set->max_bandwidth = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "minWidth")) set->min_width = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "maxWidth")) set->max_width = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "minHeight")) set->min_height = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "maxHeight")) set->max_height = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "minFrameRate")) set->min_framerate = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "maxFrameRate")) set->max_framerate = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "segmentAlignment")) set->segment_alignment = gf_mpd_parse_bool(att->value);
		else if (!strcmp(att->name, "bitstreamSwitching")) set->bitstream_switching = gf_mpd_parse_bool(att->value);
		else if (!strcmp(att->name, "subsegmentAlignment")) set->subsegment_alignment = gf_mpd_parse_bool(att->value);
		else if (!strcmp(att->name, "subsegmentStartsWithSAP")) {
			if (!strcmp(att->value, "false")) set->subsegment_starts_with_sap  = 0;
			else set->subsegment_starts_with_sap = gf_mpd_parse_int(att->value);
		}
	}
	gf_mpd_parse_common_representation(mpd, (GF_MPD_CommonAttributes*)set, root);

	i = 0;
	while ( (child = gf_list_enum(root->content, &i))) {
		if (!gf_mpd_valid_child(mpd, child)) continue;
		if (!strcmp(child->name, "Accessibility")) {
			e = gf_mpd_parse_descriptor(set->accessibility, child);
			if (e) return e;
		}
		else if (!strcmp(child->name, "Role")) {
			e = gf_mpd_parse_descriptor(set->role, child);
			if (e) return e;
		}
		else if (!strcmp(child->name, "Rating")) {
			e = gf_mpd_parse_descriptor(set->rating, child);
			if (e) return e;
		}
		else if (!strcmp(child->name, "Viewpoint")) {
			e = gf_mpd_parse_descriptor(set->viewpoint, child);
			if (e) return e;
		}
		else if (!strcmp(child->name, "BaseURL")) {
			e = gf_mpd_parse_base_url(set->base_URLs, child);
			if (e) return e;
		}
		else if (!strcmp(child->name, "ContentComponent")) {
			e = gf_mpd_parse_content_component(set->content_component, child);
			if (e) return e;
		}
		else if (!strcmp(child->name, "SegmentBase")) {
			set->segment_base = gf_mpd_parse_segment_base(mpd, child);
		}
		else if (!strcmp(child->name, "SegmentList")) {
			set->segment_list = gf_mpd_parse_segment_list(mpd, child);
		}
		else if (!strcmp(child->name, "SegmentTemplate")) {
			set->segment_template = gf_mpd_parse_segment_template(mpd, child);
		}
		else if (!strcmp(child->name, "Representation")) {
			e = gf_mpd_parse_representation(mpd, set->representations, child);
			if (e) return e;
		}
		else{
			/*We'll be assuming here that any unrecognized element is a adaptation level
			  *descriptor*/
			gf_mpd_parse_other_descriptors(child,set->other_descriptors);
		}
	}
	return GF_OK;
}

GF_MPD_Period *gf_mpd_period_new() {
	GF_MPD_Period *period;
	GF_SAFEALLOC(period, GF_MPD_Period);
	if (!period) return NULL;
	period->adaptation_sets = gf_list_new();
	period->base_URLs = gf_list_new();
	period->subsets = gf_list_new();
	period->other_descriptors = gf_list_new();
	return period;
}

GF_Err gf_mpd_parse_period(GF_MPD *mpd, GF_XMLNode *root)
{
	u32 i;
	GF_MPD_Period *period;
	GF_XMLAttribute *att;
	GF_XMLNode *child;
	GF_Err e;

	period = gf_mpd_period_new();
	if (!period) return GF_OUT_OF_MEM;
	e = gf_list_add(mpd->periods, period);
	if (e) return e;

	i = 0;
	while ( (att = gf_list_enum(root->attributes, &i)) ) {
		if (strstr(att->name, "href")) period->xlink_href = gf_mpd_parse_string(att->value);
		else if (strstr(att->name, "actuate")) period->xlink_actuate_on_load = !strcmp(att->value, "onLoad") ? 1 : 0;
		else if (!strcmp(att->name, "id")) period->ID = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "start")) period->start = gf_mpd_parse_duration(att->value);
		else if (!strcmp(att->name, "duration")) period->duration = gf_mpd_parse_duration(att->value);
		else if (!strcmp(att->name, "bitstreamSwitching")) period->bitstream_switching = gf_mpd_parse_bool(att->value);
	}

	i = 0;
	while ( (child = gf_list_enum(root->content, &i))) {
		if (!gf_mpd_valid_child(mpd, child)) continue;
		if (!strcmp(child->name, "BaseURL")) {
			e = gf_mpd_parse_base_url(period->base_URLs, child);
			if (e) return e;
		}
		else if (!strcmp(child->name, "SegmentBase")) {
			period->segment_base = gf_mpd_parse_segment_base(mpd, child);
		}
		else if (!strcmp(child->name, "SegmentList")) {
			period->segment_list = gf_mpd_parse_segment_list(mpd, child);
		}
		else if (!strcmp(child->name, "SegmentTemplate")) {
			period->segment_template = gf_mpd_parse_segment_template(mpd, child);
		}
		else if (!strcmp(child->name, "AdaptationSet")) {
			e = gf_mpd_parse_adaptation_set(mpd, period->adaptation_sets, child);
			if (e) return e;
		}
		else if (!strcmp(child->name, "SubSet")) {
		}
		else{
			/*We'll be assuming here that any unrecognized element is a period level
			  *descriptor*/
			gf_mpd_parse_other_descriptors(child, period->other_descriptors);
		}

	}
	return GF_OK;
}

GF_EXPORT
GF_MPD *gf_mpd_new()
{
	GF_MPD *mpd;
	GF_SAFEALLOC(mpd, GF_MPD);
	return mpd;
}

static void gf_mpd_del_list(GF_List *list, void (*__destructor)(void *), Bool reset_only)
{
	if (!list) return;
	while (gf_list_count(list)) {
		void *item = gf_list_last(list);
		gf_list_rem_last(list);
		if (item && __destructor) __destructor(item);
	}
	if (!reset_only) gf_list_del(list);
}

void gf_mpd_base_url_free(void *_item)
{
	GF_MPD_BaseURL *base_url = (GF_MPD_BaseURL *)_item;
	if (base_url->service_location) gf_free(base_url->service_location);
	if (base_url->URL) gf_free(base_url->URL);
	gf_free(base_url);
}

void gf_mpd_url_free(void *_item)
{
	GF_MPD_URL *ptr = (GF_MPD_URL*)_item;
	if (ptr->sourceURL) gf_free(ptr->sourceURL);
	if (ptr->byte_range) gf_free(ptr->byte_range);
	gf_free(ptr);
}
void gf_mpd_string_free(void *_item) {
	gf_free(_item);
}

void gf_mpd_prog_info_free(void *_item)
{
	GF_MPD_ProgramInfo *ptr = (GF_MPD_ProgramInfo *)_item;
	if (ptr->lang) gf_free(ptr->lang);
	if (ptr->title) gf_free(ptr->title);
	if (ptr->source) gf_free(ptr->source);
	if (ptr->copyright) gf_free(ptr->copyright);
	if (ptr->more_info_url) gf_free(ptr->more_info_url);
	gf_free(ptr);
}
void gf_mpd_segment_url_free(void *_ptr)
{
	GF_MPD_SegmentURL *ptr = (GF_MPD_SegmentURL *)_ptr;
	if (ptr->index) gf_free(ptr->index);
	if (ptr->index_range) gf_free(ptr->index_range);
	if (ptr->media) gf_free(ptr->media);
	if (ptr->media_range) gf_free(ptr->media_range);
	if (ptr->key_url) gf_free(ptr->key_url);
	gf_free(ptr);
}
void gf_mpd_segment_base_free(void *_item)
{
	GF_MPD_SegmentBase *ptr = (GF_MPD_SegmentBase *)_item;
	if (ptr->initialization_segment) gf_mpd_url_free(ptr->initialization_segment);
	if (ptr->representation_index) gf_mpd_url_free(ptr->representation_index);
	if (ptr->index_range) gf_free(ptr->index_range);
	gf_free(ptr);
}

void gf_mpd_segment_entry_free(void *_item)
{
	gf_free(_item);
}
void gf_mpd_segment_timeline_free(void *_item)
{
	GF_MPD_SegmentTimeline *ptr = (GF_MPD_SegmentTimeline *)_item;
	gf_mpd_del_list(ptr->entries, gf_mpd_segment_entry_free, 0);
	gf_free(ptr);
}

void gf_mpd_segment_url_list_free(GF_List *list)
{
	gf_mpd_del_list(list, gf_mpd_segment_url_free, 0);
}

void gf_mpd_segment_list_free(void *_item)
{
	GF_MPD_SegmentList *ptr = (GF_MPD_SegmentList *)_item;
	if (ptr->xlink_href) gf_free(ptr->xlink_href);
	if (ptr->initialization_segment) gf_mpd_url_free(ptr->initialization_segment);
	if (ptr->bitstream_switching_url) gf_mpd_url_free(ptr->bitstream_switching_url);
	if (ptr->representation_index) gf_mpd_url_free(ptr->representation_index);
	if (ptr->segment_timeline) gf_mpd_segment_timeline_free(ptr->segment_timeline);
	gf_mpd_del_list(ptr->segment_URLs, gf_mpd_segment_url_free, 0);
	gf_free(ptr);
}

void gf_mpd_segment_template_free(void *_item)
{
	GF_MPD_SegmentTemplate *ptr = (GF_MPD_SegmentTemplate *)_item;
	if (ptr->initialization_segment) gf_mpd_url_free(ptr->initialization_segment);
	if (ptr->bitstream_switching_url) gf_mpd_url_free(ptr->bitstream_switching_url);
	if (ptr->representation_index) gf_mpd_url_free(ptr->representation_index);
	if (ptr->segment_timeline) gf_mpd_segment_timeline_free(ptr->segment_timeline);
	if (ptr->index) gf_free(ptr->index);
	if (ptr->media) gf_free(ptr->media);
	if (ptr->initialization) gf_free(ptr->initialization);
	if (ptr->bitstream_switching) gf_free(ptr->bitstream_switching);
	gf_free(ptr);
}

void gf_mpd_extensible_free(GF_MPD_ExtensibleVirtual *item)
{
	if (item->attributes) {
		while (gf_list_count(item->attributes)) {
			GF_XMLAttribute *att = gf_list_last(item->attributes);
			gf_list_rem_last(item->attributes);
			if (att->name) gf_free(att->name);
			if (att->value) gf_free(att->value);
			gf_free(att);
		}
		gf_list_del(item->attributes);
	}
	if (item->children) {
		while (gf_list_count(item->children)) {
			GF_XMLNode *child = gf_list_last(item->children);
			gf_list_rem_last(item->children);
			gf_xml_dom_node_del(child);
		}
		gf_list_del(item->children);
	}
}

GF_MPD_Descriptor *gf_mpd_descriptor_new(const char *id, const char *schemeIdUri, const char *value) {
	GF_MPD_Descriptor *mpd_desc;
	GF_SAFEALLOC(mpd_desc, GF_MPD_Descriptor);
	if (!mpd_desc) return NULL;
	if(id!=NULL){
		mpd_desc->id = gf_strdup(id);
	}
	mpd_desc->scheme_id_uri = gf_strdup(schemeIdUri);
	mpd_desc->value = gf_strdup(value);
	return mpd_desc;
}

void gf_mpd_descriptor_free(void *item)
{
	GF_MPD_Descriptor *mpd_desc = (GF_MPD_Descriptor*) item;
	if (mpd_desc->id) gf_free(mpd_desc->id);
	if (mpd_desc->scheme_id_uri) gf_free(mpd_desc->scheme_id_uri);
	if (mpd_desc->value) gf_free(mpd_desc->value);
	gf_mpd_extensible_free((GF_MPD_ExtensibleVirtual *)mpd_desc);

	gf_free(mpd_desc);
}

void gf_mpd_other_descriptor_free(void *_item)
{
	GF_MPD_other_descriptors *ptr = (GF_MPD_other_descriptors *)_item;
	if(ptr->xml_desc)gf_free(ptr->xml_desc);
	gf_free(ptr);
}

void gf_mpd_content_component_free(void *item)
{
	GF_MPD_ContentComponent *component_descriptor=(GF_MPD_ContentComponent*) item;
	if (component_descriptor->type) gf_free(component_descriptor->type);
	if (component_descriptor->lang) gf_free(component_descriptor->lang);
	gf_free(item);
}

void gf_mpd_common_attributes_free(GF_MPD_CommonAttributes *ptr)
{
	if (ptr->profiles) gf_free(ptr->profiles);
	if (ptr->sar) gf_free(ptr->sar);
	if (ptr->framerate) gf_free(ptr->framerate);
	if (ptr->mime_type) gf_free(ptr->mime_type);
	if (ptr->segmentProfiles) gf_free(ptr->segmentProfiles);
	if (ptr->codecs) gf_free(ptr->codecs);
	gf_mpd_del_list(ptr->frame_packing, gf_mpd_descriptor_free, 0);
	gf_mpd_del_list(ptr->audio_channels, gf_mpd_descriptor_free, 0);
	gf_mpd_del_list(ptr->content_protection, gf_mpd_descriptor_free, 0);
	gf_mpd_del_list(ptr->essential_properties, gf_mpd_descriptor_free, 0);
	gf_mpd_del_list(ptr->supplemental_properties, gf_mpd_descriptor_free, 0);
}

void gf_mpd_representation_free(void *_item)
{
	GF_MPD_Representation *ptr = (GF_MPD_Representation *)_item;
	gf_mpd_common_attributes_free((GF_MPD_CommonAttributes *)ptr);
	if (ptr->id) gf_free(ptr->id);
	if (ptr->dependency_id) gf_free(ptr->dependency_id);
	if (ptr->media_stream_structure_id) gf_free(ptr->media_stream_structure_id);

	if (ptr->playback.cached_init_segment_url) {
		if (ptr->playback.owned_gmem && !strnicmp(ptr->playback.cached_init_segment_url, "gmem://", 7)) {
			u32 size;
			char *mem_address;
			if (sscanf(ptr->playback.cached_init_segment_url, "gmem://%d@%p", &size, &mem_address) != 2) {
				assert(0);
			}
			gf_free(mem_address);
		}
		gf_free(ptr->playback.cached_init_segment_url);
	}
	if (ptr->playback.init_segment_data) gf_free(ptr->playback.init_segment_data);
	if (ptr->playback.key_url) gf_free(ptr->playback.key_url);

	gf_mpd_del_list(ptr->base_URLs, gf_mpd_base_url_free, 0);
	gf_mpd_del_list(ptr->sub_representations, NULL/*TODO*/, 0);
	if (ptr->segment_base) gf_mpd_segment_base_free(ptr->segment_base);
	if (ptr->segment_list) gf_mpd_segment_list_free(ptr->segment_list);
	if (ptr->segment_template) gf_mpd_segment_template_free(ptr->segment_template);
	if (ptr->other_descriptors)gf_mpd_del_list(ptr->other_descriptors, gf_mpd_other_descriptor_free, 0);
	gf_free(ptr);
}

void gf_mpd_adaptation_set_free(void *_item)
{
	GF_MPD_AdaptationSet *ptr = (GF_MPD_AdaptationSet *)_item;
	gf_mpd_common_attributes_free((GF_MPD_CommonAttributes *)ptr);
	if (ptr->lang) gf_free(ptr->lang);
	if (ptr->content_type) gf_free(ptr->content_type);
	if (ptr->par) gf_free(ptr->par);
	if (ptr->xlink_href) gf_free(ptr->xlink_href);
	gf_mpd_del_list(ptr->accessibility, gf_mpd_descriptor_free, 0);
	gf_mpd_del_list(ptr->role, gf_mpd_descriptor_free, 0);
	gf_mpd_del_list(ptr->rating, gf_mpd_descriptor_free, 0);
	gf_mpd_del_list(ptr->viewpoint, gf_mpd_descriptor_free, 0);
	gf_mpd_del_list(ptr->content_component, gf_mpd_content_component_free, 0);
	if (ptr->segment_base) gf_mpd_segment_base_free(ptr->segment_base);
	if (ptr->segment_list) gf_mpd_segment_list_free(ptr->segment_list);
	if (ptr->segment_template) gf_mpd_segment_template_free(ptr->segment_template);
	gf_mpd_del_list(ptr->base_URLs, gf_mpd_base_url_free, 0);
	gf_mpd_del_list(ptr->representations, gf_mpd_representation_free, 0);
	gf_mpd_del_list(ptr->other_descriptors, gf_mpd_other_descriptor_free, 0);
	gf_free(ptr);
}

void gf_mpd_period_free(void *_item)
{
	GF_MPD_Period *ptr = (GF_MPD_Period *)_item;
	if (ptr->ID) gf_free(ptr->ID);
	if (ptr->xlink_href) gf_free(ptr->xlink_href);
	if (ptr->segment_base) gf_mpd_segment_base_free(ptr->segment_base);
	if (ptr->segment_list) gf_mpd_segment_list_free(ptr->segment_list);
	if (ptr->segment_template) gf_mpd_segment_template_free(ptr->segment_template);

	gf_mpd_del_list(ptr->base_URLs, gf_mpd_base_url_free, 0);
	gf_mpd_del_list(ptr->adaptation_sets, gf_mpd_adaptation_set_free, 0);
	gf_mpd_del_list(ptr->other_descriptors,gf_mpd_other_descriptor_free,0);
	gf_mpd_del_list(ptr->subsets, NULL/*TODO*/, 0);
	gf_free(ptr);
}

GF_EXPORT
void gf_mpd_del(GF_MPD *mpd)
{
	if (!mpd) return;
	gf_mpd_del_list(mpd->program_infos, gf_mpd_prog_info_free, 0);
	gf_mpd_del_list(mpd->base_URLs, gf_mpd_base_url_free, 0);
	gf_mpd_del_list(mpd->locations, gf_mpd_string_free, 0);
	gf_mpd_del_list(mpd->metrics, NULL/*TODO*/, 0);
	gf_mpd_del_list(mpd->periods, gf_mpd_period_free, 0);
	if (mpd->profiles) gf_free(mpd->profiles);
	if (mpd->ID) gf_free(mpd->ID);
	gf_mpd_extensible_free((GF_MPD_ExtensibleVirtual*) mpd);
	gf_free(mpd);
}

GF_EXPORT
void gf_mpd_reset_periods(GF_MPD *mpd)
{
	if (!mpd) return;
	gf_mpd_del_list(mpd->periods, gf_mpd_period_free, 0);
	mpd->periods = gf_list_new();
}


GF_EXPORT
GF_Err gf_mpd_complete_from_dom(GF_XMLNode *root, GF_MPD *mpd, const char *default_base_url)
{
	GF_Err e;
	u32 i;
	Bool ns_ok = GF_FALSE;
	GF_XMLAttribute *att;
	GF_XMLNode *child;

	if (!root || !mpd) return GF_BAD_PARAM;
	i=0;
	while ((att = gf_list_enum(root->attributes, &i))) {
		if (!strcmp(att->name, "xmlns")) {
			if (!root->ns && (!strcmp(att->value, "urn:mpeg:dash:schema:mpd:2011") || !strcmp(att->value, "urn:mpeg:DASH:schema:MPD:2011")) ) {
				ns_ok = 1;
				break;
			}
		}
		else if (!strncmp(att->name, "xmlns:", 6)) {
			if (root->ns && !strcmp(att->name+6, root->ns) && (!strcmp(att->value, "urn:mpeg:dash:schema:mpd:2011") || !strcmp(att->value, "urn:mpeg:DASH:schema:MPD:2011")) ) {
				ns_ok = 1;
				if (!mpd->xml_namespace) mpd->xml_namespace = root->ns;
				break;
			}
		}
	}

	if (!ns_ok) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD] Wrong namespace found for DASH MPD - cannot parse\n"));
	}

	if (!strcmp(root->name, "Period")) {
		return gf_mpd_parse_period(mpd, root);
	}

	i = 0;
	while ((att = gf_list_enum(root->attributes, &i))) {
		if (!strcmp(att->name, "id")) {
			mpd->ID = gf_mpd_parse_string(att->value);
		} else if (!strcmp(att->name, "profiles")) {
			mpd->profiles = gf_mpd_parse_string(att->value);
		} else if (!strcmp(att->name, "type")) {
			if (!strcmp(att->value, "static")) mpd->type = GF_MPD_TYPE_STATIC;
			else if (!strcmp(att->value, "dynamic")) mpd->type = GF_MPD_TYPE_DYNAMIC;
		} else if (!strcmp(att->name, "availabilityStartTime")) {
			mpd->availabilityStartTime = gf_mpd_parse_date(att->value);
		} else if (!strcmp(att->name, "availabilityEndTime")) {
			mpd->availabilityEndTime = gf_mpd_parse_date(att->value);
		} else if (!strcmp(att->name, "publishTime")) {
			mpd->publishTime = gf_mpd_parse_date(att->value);
		} else if (!strcmp(att->name, "mediaPresentationDuration")) {
			mpd->media_presentation_duration = gf_mpd_parse_duration(att->value);
		} else if (!strcmp(att->name, "minimumUpdatePeriod")) {
			mpd->minimum_update_period = gf_mpd_parse_duration_u32(att->value);
		} else if (!strcmp(att->name, "minBufferTime")) {
			mpd->min_buffer_time = gf_mpd_parse_duration_u32(att->value);
		} else if (!strcmp(att->name, "timeShiftBufferDepth")) {
			mpd->time_shift_buffer_depth = gf_mpd_parse_duration_u32(att->value);
		} else if (!strcmp(att->name, "suggestedPresentationDelay")) {
			mpd->suggested_presentation_delay = gf_mpd_parse_duration_u32(att->value);
		} else if (!strcmp(att->name, "maxSegmentDuration")) {
			mpd->max_segment_duration = gf_mpd_parse_duration_u32(att->value);
		} else if (!strcmp(att->name, "maxSubsegmentDuration")) {
			mpd->max_subsegment_duration = gf_mpd_parse_duration_u32(att->value);
		} else {
			MPD_STORE_EXTENSION_ATTR(mpd)
		}
	}
	if (mpd->type == GF_MPD_TYPE_STATIC)
		mpd->minimum_update_period = mpd->time_shift_buffer_depth = 0;

	i = 0;
	while ( ( child = gf_list_enum(root->content, &i )) ) {
		if (! gf_mpd_valid_child(mpd, child))
			continue;

		if (!strcmp(child->name, "ProgramInformation")) {
			e = gf_mpd_parse_program_info(mpd, child);
			if (e) return e;
		} else if (!strcmp(child->name, "Location")) {
			e = gf_mpd_parse_location(mpd, child);
			if (e) return e;
		} else if (!strcmp(child->name, "Period")) {
			e = gf_mpd_parse_period(mpd, child);
			if (e) return e;
		} else if (!strcmp(child->name, "Metrics")) {
			e = gf_mpd_parse_metrics(mpd, child);
			if (e) return e;
		} else if (!strcmp(child->name, "BaseURL")) {
			e = gf_mpd_parse_base_url(mpd->base_URLs, child);
			if (e) return e;
		} else {
			MPD_STORE_EXTENSION_NODE(mpd)
		}
	}
	return GF_OK;
}


GF_EXPORT
GF_Err gf_mpd_init_from_dom(GF_XMLNode *root, GF_MPD *mpd, const char *default_base_url)
{
	if (!root || !mpd) return GF_BAD_PARAM;

	assert(!mpd->periods);
	mpd->periods = gf_list_new();
	mpd->program_infos = gf_list_new();
	mpd->base_URLs = gf_list_new();
	mpd->locations = gf_list_new();
	mpd->metrics = gf_list_new();

	/*setup some defaults*/
	mpd->type = GF_MPD_TYPE_STATIC;
	mpd->time_shift_buffer_depth = (u32) -1; /*infinite by default*/
	mpd->xml_namespace = NULL;

	return gf_mpd_complete_from_dom(root, mpd, default_base_url);
}

GF_EXPORT
void gf_mpd_getter_del_session(GF_FileDownload *getter) {
	if (!getter || !getter->del_session)
		return;
	getter->del_session(getter);
}

static GF_Err gf_m3u8_fill_mpd_struct(MasterPlaylist *pl, const char *m3u8_file, const char *src_base_url, const char *mpd_file, char *title, Double update_interval,
                                      char *mimeTypeForM3U8Segments, Bool do_import, Bool use_mpd_templates, Bool is_end, u32 max_dur, GF_MPD *mpd, Bool parse_sub_playlist)
{
	char *sep, *template_base=NULL, *template_ext;
	u32 nb_streams, i, j, k, template_width, template_idx_start;
	Stream *stream;
	PlaylistElement *pe, *elt;
	GF_MPD_ProgramInfo *info;
	GF_MPD_Period *period;
	GF_Err e;
	Bool all_template_used = use_mpd_templates;
	char str[1024];

	if (!mpd) return GF_BAD_PARAM;
	assert(!mpd->periods);
	mpd->periods = gf_list_new();
	mpd->program_infos = gf_list_new();
	mpd->base_URLs = gf_list_new();
	mpd->locations = gf_list_new();
	mpd->metrics = gf_list_new();
	mpd->time_shift_buffer_depth = (u32) -1; /*infinite by default*/
	mpd->xml_namespace = "urn:mpeg:dash:schema:mpd:2011";
	mpd->type = is_end ? GF_MPD_TYPE_STATIC : GF_MPD_TYPE_DYNAMIC;


	sep = strrchr(m3u8_file, '/');
	if (!sep)
		sep = strrchr(m3u8_file, '\\');
	if (sep)
		sep = sep + 1;
	else
		sep = (char *)m3u8_file;
	mpd->ID = gf_strdup(sep);

	if (update_interval) {
		mpd->minimum_update_period = (u32) (update_interval*1000);
	}
	if (is_end) {
		mpd->media_presentation_duration = (u64) (max_dur*1000);
	}
	if (mpd->type == GF_MPD_TYPE_STATIC)
		mpd->minimum_update_period = mpd->time_shift_buffer_depth = 0;
	mpd->min_buffer_time = 1500;

	GF_SAFEALLOC(info, GF_MPD_ProgramInfo);
	if (!info) return GF_OUT_OF_MEM;
	info->more_info_url = gf_strdup("http://gpac.io");
	info->title = gf_strdup(title);
	sprintf(str, "Generated from URL %s", src_base_url);
	info->source = gf_strdup(str);
	sprintf(str, "Generated by GPAC %s", GPAC_FULL_VERSION);
	info->copyright = gf_strdup(str);
	gf_list_add(mpd->program_infos, info);

	GF_SAFEALLOC(period, GF_MPD_Period);
	if (!period) return GF_OUT_OF_MEM;
	period->adaptation_sets = gf_list_new();
	period->base_URLs = gf_list_new();
	period->subsets = gf_list_new();
	e = gf_list_add(mpd->periods, period);
	if (e) return e;
	if (is_end) {
		period->duration = max_dur*1000;
	}

	nb_streams = gf_list_count(pl->streams);
	/*check if we use templates*/
	template_base = NULL;
	template_ext = NULL;
	template_width = 0;
	template_idx_start = 0;
	elt = NULL;
	pe = NULL;


	nb_streams = gf_list_count(pl->streams);
	for (i=0; i<nb_streams; i++) {
		u32 count_variants;
		u32 width, height, samplerate, num_channels;
		GF_MPD_AdaptationSet *set;
		Bool use_template = use_mpd_templates;
		GF_SAFEALLOC(set, GF_MPD_AdaptationSet);
		if (!set) return GF_OUT_OF_MEM;
		gf_mpd_init_common_attributes((GF_MPD_CommonAttributes *)set);
		set->accessibility = gf_list_new();
		set->role = gf_list_new();
		set->rating = gf_list_new();
		set->viewpoint = gf_list_new();
		set->content_component = gf_list_new();
		set->base_URLs = gf_list_new();
		set->representations = gf_list_new();
		/*assign default ID and group*/
		set->group = -1;
		set->segment_alignment = GF_TRUE;
		e = gf_list_add(period->adaptation_sets, set);
		if (e) return e;
		
		/*check if we use templates*/
		stream = gf_list_get(pl->streams, i);
		count_variants = gf_list_count(stream->variants);
		
		if (use_template) {
			for (j=0; j<count_variants; j++) {
				u32 count_elements;
				pe = gf_list_get(stream->variants, j);
				if (pe->element_type != TYPE_PLAYLIST)
					continue;

				count_elements = gf_list_count(pe->element.playlist.elements);
				if (!count_elements)
					continue;

				if (!template_base && use_template) {
					char *sub_url;
					elt = gf_list_get(pe->element.playlist.elements, 0);
					sub_url = strrchr(elt->url, '/');
					if (!sub_url) {
						sub_url = elt->url;
					} else {
						sub_url ++;
					}
					template_base = gf_strdup(sub_url);
					template_ext = strrchr(template_base, '.');
					k=0;
					while (1) {
						if (strchr("0123456789", template_base[k])) {
							if (template_ext) {
								template_ext[0] = 0;
								template_width = (u32) strlen(template_base + k);
								template_idx_start = atoi(template_base + k);
								template_ext[0] = '.';
							}
							template_base[k] = 0;
							break;
						}
						k++;
						if (!template_base[k]) {
							use_template = GF_FALSE;
							break;
						}
					}
				}
				if (!template_ext) template_ext="";

				if (use_template) {
					for (k=0; k<count_elements; k++) {
						char szURL[GF_MAX_PATH], *sub_url;
						elt = gf_list_get(pe->element.playlist.elements, k);

						if (template_width == 2)
							sprintf(szURL, "%s%02d%s", template_base, template_idx_start + k, template_ext);
						else if (template_width == 3)
							sprintf(szURL, "%s%03d%s", template_base, template_idx_start + k, template_ext);
						else if (template_width == 4)
							sprintf(szURL, "%s%04d%s", template_base, template_idx_start + k, template_ext);
						else if (template_width == 5)
							sprintf(szURL, "%s%05d%s", template_base, template_idx_start + k, template_ext);
						else if (template_width == 6)
							sprintf(szURL, "%s%06d%s", template_base, template_idx_start + k, template_ext);
						else
							sprintf(szURL, "%s%d%s", template_base, template_idx_start + k, template_ext);

						sub_url = strrchr(elt->url, '/');
						if (!sub_url) sub_url = elt->url;
						else sub_url ++;
						if (strcmp(szURL, sub_url)) {
							use_template = GF_FALSE;
							break;
						}
					}
				}
			}
		}

		/*if we use templates, put the SegmentTemplate element at the adaptationSet level*/
		if (use_template) {
			GF_SAFEALLOC(set->segment_template, GF_MPD_SegmentTemplate);
			if (!set->segment_template)  return GF_OUT_OF_MEM;
			set->segment_template->duration = (u32)pe->duration_info;
			if (template_width > 1) {
				sprintf(str, "%s$%%0%ddNumber$%s", template_base, template_width, template_ext);
			} else {
				sprintf(str, "%s$Number$%s", template_base, template_ext);
			}
			set->segment_template->media = gf_strdup(str);
			set->segment_template->start_number = template_idx_start;
		} else {
			all_template_used = GF_FALSE;
		}
		for (j=0; j<count_variants; j++) {
			char *base_url=NULL;
			u32 count_elements;
			char szName[20];
#ifndef GPAC_DISABLE_MEDIA_IMPORT
			Bool import_file = do_import;
#endif
			char *byte_range_media_file = NULL;
			GF_MPD_Representation *rep;
			pe = gf_list_get(stream->variants, j);

			if (pe->element_type == TYPE_MEDIA) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD] NOT SUPPORTED: M3U8 Media\n"));
			} else if (pe->load_error) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD] Error loading playlist element %s\n", pe->url));
			} else if (pe->element_type != TYPE_PLAYLIST) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD] NOT SUPPORTED: M3U8 unknown type for %s\n", pe->url));
			}

			count_elements = gf_list_count(pe->element.playlist.elements);
			if (parse_sub_playlist && !count_elements)
				continue;

			if (pe->codecs && (pe->codecs[0] == '\"')) {
				u32 len = (u32) strlen(pe->codecs);
//				strncpy(pe->codecs, pe->codecs+1, len-1);
				memmove(pe->codecs, pe->codecs+1, len-1);
				pe->codecs[len-2] = 0;
			}
#ifndef GPAC_DISABLE_MEDIA_IMPORT
			if (pe->bandwidth && pe->codecs && pe->width && pe->height) {
				import_file = GF_FALSE;
			}
#endif
			if (pe->media_type==MEDIA_TYPE_SUBTITLES) {
#ifndef GPAC_DISABLE_MEDIA_IMPORT
				import_file = GF_FALSE;
#endif
				if (!pe->codecs) pe->codecs = gf_strdup("wvtt");
			}
			if (pe->media_type==MEDIA_TYPE_CLOSED_CAPTIONS) {
#ifndef GPAC_DISABLE_MEDIA_IMPORT
				import_file = GF_FALSE;
#endif
				if (!pe->codecs) pe->codecs = gf_strdup("wvtt");
			}

			k = 0;
#ifndef GPAC_DISABLE_MEDIA_IMPORT
try_next_segment:
#endif
			elt = gf_list_get(pe->element.playlist.elements, k);
			if (parse_sub_playlist && !elt)
				break;

			if (elt) {
				base_url = gf_url_concatenate(pe->url, elt->url);
			} else {
				base_url = gf_strdup(pe->url);
			}
			sep = strrchr(base_url, '/');
			if (!sep)
				sep = strrchr(base_url, '\\');
			/*keep final '/' */
			if (sep)
				sep[1] = 0;


			width = pe->width;
			height = pe->height;
			samplerate = num_channels = 0;
#ifndef GPAC_DISABLE_MEDIA_IMPORT
			if (elt && import_file) {
				GF_Err e;
				GF_MediaImporter import;
				char *elt_url = elt->init_segment_url ? elt->init_segment_url : elt->url;
				u64 br_start, br_end;
				char *tmp_file = NULL;
				
				br_start = elt->init_segment_url ? elt->init_byte_range_start : elt->byte_range_start;
				br_end = elt->init_segment_url ? elt->init_byte_range_end : elt->byte_range_end;
				elt_url = gf_url_concatenate(pe->url, elt_url);

				memset(&import, 0, sizeof(GF_MediaImporter));
				import.trackID = 0;
				import.flags = GF_IMPORT_PROBE_ONLY;

				if (strstr(elt_url, "://") && !strstr(elt_url, "file://")) {
					tmp_file = strrchr(elt_url, '/');
					if (!tmp_file)
						tmp_file = strrchr(elt_url, '\\');
					if (tmp_file) {
						tmp_file++;
						e = gf_dm_wget(elt_url, tmp_file, br_start, br_end, NULL);
						if (e == GF_OK) {
							import.in_name = tmp_file;
						}
					}
				} else {
					import.in_name = elt_url;
				}
				e = gf_media_import(&import);
				gf_free(elt_url);
				
				if (e != GF_OK) {
//					GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD] M3U8 missing Media Element %s< (Playlist %s) %s \n", import.in_name, base_url));
					k++;
					goto try_next_segment;
				}

				if (import.in_name && !pe->bandwidth && !elt->init_segment_url && pe->duration_info) {
					u64 pos = 0;

					Double bw;
					FILE *t = gf_fopen(import.in_name, "rb");
					if (t) {
						gf_fseek(t, 0, SEEK_END);
						pos = gf_ftell(t);
						gf_fclose(t);
					}
					bw = (Double) pos;
					bw *= 8;
					bw /= pe->duration_info;
					pe->bandwidth = (u32) bw;
				} else if (!pe->bandwidth) {
					//unknown bandwidth, default to 128k ...
					pe->bandwidth = 128000;
				}

				if (tmp_file)
					gf_delete_file(tmp_file);

				if (!pe->codecs) {
					char szCodecs[1024];
					szCodecs[0] = 0;
					for (k=0; k<import.nb_tracks; k++) {
						if (strlen(import.tk_info[k].szCodecProfile)) {
							if (strlen(szCodecs)) strcat(szCodecs, ",");
							strcat(szCodecs, import.tk_info[k].szCodecProfile);
						}
					}
					pe->codecs = gf_strdup(szCodecs);
				}
				for (k=0; k<import.nb_tracks; k++) {
					switch (import.tk_info[k].type) {
					case GF_ISOM_MEDIA_VISUAL:
						width = import.tk_info[k].video_info.width;
						height = import.tk_info[k].video_info.height;
						break;
					case GF_ISOM_MEDIA_AUDIO:
						samplerate = import.tk_info[k].audio_info.sample_rate;
						num_channels = import.tk_info[k].audio_info.nb_channels;
						break;
					}
				}
			}
#endif
			GF_SAFEALLOC(rep, GF_MPD_Representation);
			if (!rep) return GF_OUT_OF_MEM;
			gf_mpd_init_common_attributes((GF_MPD_CommonAttributes *)rep);
			rep->base_URLs = gf_list_new();
			rep->sub_representations = gf_list_new();

			/*get rid of level 0 aac*/
			if (elt && strstr(elt->url, ".aac"))
				rep->playback.disabled = GF_TRUE;

			e = gf_list_add(set->representations, rep);
			if (e) return e;
			sprintf(szName, "R%d_%d", i+1, j+1);
			rep->id = gf_strdup(szName);
			rep->bandwidth = pe->bandwidth;
			/* TODO : if mime-type is still unknown, don't try to add codec information since it would be wrong */
			if (!strcmp(M3U8_UNKNOWN_MIME_TYPE, mimeTypeForM3U8Segments)) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD] Unknown mime-type when converting from M3U8 HLS playlist, setting %s\n", mimeTypeForM3U8Segments));
			}
			if (elt && elt->init_segment_url && (strstr(elt->init_segment_url, ".mp4") || strstr(elt->init_segment_url, ".MP4")) ) {
				rep->mime_type = gf_strdup(samplerate ? "audio/mp4" : "video/mp4");
			} else {
				rep->mime_type = gf_strdup(mimeTypeForM3U8Segments);
			}
			if (pe->codecs) {
				rep->codecs = gf_strdup(pe->codecs);
			}
			if (pe->language) {
				//???
			}
			if (width && height) {
				rep->width = width;
				rep->height = height;
			}
			if (samplerate) {
				rep->samplerate = samplerate;
			}
			if (num_channels) {
				char szChan[10];
				GF_MPD_Descriptor *desc;
				GF_SAFEALLOC(desc, GF_MPD_Descriptor);
				desc->scheme_id_uri = gf_strdup("urn:mpeg:dash:23003:3:audio_channel_configuration:2011");
				sprintf(szChan, "%d", num_channels);
				desc->value = gf_strdup(szChan);
				if (!rep->audio_channels) rep->audio_channels = gf_list_new();
				gf_list_add(rep->audio_channels, desc);
			}


			if (use_template) {
				GF_MPD_BaseURL *url;
				GF_SAFEALLOC(url, GF_MPD_BaseURL);
				if (! url) return GF_OUT_OF_MEM;
				e = gf_list_add(rep->base_URLs, url);
				if (e) return GF_OUT_OF_MEM;
				url->URL = gf_strdup(base_url);
				
				
				if (elt->init_segment_url) {
					u32 len = (u32) strlen(base_url);
					GF_SAFEALLOC(rep->segment_template, GF_MPD_SegmentTemplate);
					if (!rep->segment_template)  return GF_OUT_OF_MEM;
					rep->segment_template->start_number = (u32) -1;
					if (!strncmp(base_url, elt->init_segment_url, len)) {
						rep->segment_template->initialization = gf_strdup(elt->init_segment_url + len);
					} else {
						rep->segment_template->initialization = gf_strdup(elt->init_segment_url);
					}
				}
				
				continue;
			}

			byte_range_media_file = NULL;
			elt = gf_list_get(pe->element.playlist.elements, 0);
			if (elt && (elt->byte_range_end || elt->byte_range_start)) {
				GF_MPD_BaseURL *url;
				GF_SAFEALLOC(url, GF_MPD_BaseURL);
				if (! url) return GF_OUT_OF_MEM;
				e = gf_list_add(rep->base_URLs, url);
				if (e) return GF_OUT_OF_MEM;
				byte_range_media_file = elt->url;
				url->URL = gf_strdup(byte_range_media_file);
			} else {
				GF_MPD_BaseURL *url;
				GF_SAFEALLOC(url, GF_MPD_BaseURL);
				if (! url) return GF_OUT_OF_MEM;
				e = gf_list_add(rep->base_URLs, url);
				if (e) return GF_OUT_OF_MEM;
				url->URL = gf_strdup(base_url);
			}

			GF_SAFEALLOC(rep->segment_list, GF_MPD_SegmentList);
			if (!rep->segment_list) return GF_OUT_OF_MEM;
			// doesn't parse sub-playlists, we need to save URL to these sub-playlist in xlink:href so that we can get the segment URL when we need
			// note: for MPD type static, always parse all sub-playlist because we just do it once in a period
			if ((mpd->type == GF_MPD_TYPE_DYNAMIC) && !parse_sub_playlist) {
				rep->segment_list->xlink_href = pe->url;
				pe->url=NULL;
				gf_free(base_url);
				base_url = NULL;
				if (template_base) {
					gf_free(template_base);
					template_base = NULL;
				}
				continue;
			}
			rep->segment_list->segment_URLs = gf_list_new();
			rep->segment_list->duration = (u64) (pe->duration_info * 1000);
			if (elt && elt->init_segment_url) {
				u32 len = (u32) strlen(base_url);
				GF_SAFEALLOC(rep->segment_list->initialization_segment, GF_MPD_URL);
				
				if (!strncmp(base_url, elt->init_segment_url, len)) {
					rep->segment_list->initialization_segment->sourceURL = gf_strdup(elt->init_segment_url + len);
				} else {
					rep->segment_list->initialization_segment->sourceURL = gf_strdup(elt->init_segment_url);
				}
				if (elt->init_byte_range_end) {
					GF_SAFEALLOC(rep->segment_list->initialization_segment->byte_range, GF_MPD_ByteRange);
					rep->segment_list->initialization_segment->byte_range->start_range = elt->init_byte_range_start;
					rep->segment_list->initialization_segment->byte_range->end_range = elt->init_byte_range_end;
				}
			}
			
//			update_interval = (count_elements - 1) * pe->duration_info * 1000;
			for (k=0; k<count_elements; k++) {
				GF_MPD_SegmentURL *segment_url;
				elt = gf_list_get(pe->element.playlist.elements, k);

				GF_SAFEALLOC(segment_url, GF_MPD_SegmentURL);
				if (!segment_url) return GF_OUT_OF_MEM;
				gf_list_add(rep->segment_list->segment_URLs, segment_url);
				if (byte_range_media_file) {
					GF_SAFEALLOC(segment_url->media_range, GF_MPD_ByteRange);
					segment_url->media_range->start_range = elt->byte_range_start;
					segment_url->media_range->end_range = elt->byte_range_end;
					if (strcmp(elt->url, byte_range_media_file)) {
						segment_url->media = elt->url;
						elt->url=NULL;
					}
				} else {
					u32 len = (u32) strlen(base_url);
					if (!strncmp(base_url, elt->url, len)) {
						segment_url->media = gf_strdup(elt->url+len);
					} else {
						segment_url->media = elt->url;
						elt->url=NULL;
					}
				}
				if (elt->drm_method != DRM_NONE) {
					//segment_url->key_url = "aes-128";
					if (elt->key_uri) {
						segment_url->key_url = elt->key_uri;
						elt->key_uri=NULL;
						memcpy(segment_url->key_iv, elt->key_iv, sizeof(bin128));
					}
				}
			}
			gf_free(base_url);
		}

		if (template_base) {
			gf_free(template_base);
			template_base = NULL;
		}

	}
	if (all_template_used) {
		mpd->profiles = gf_strdup("urn:mpeg:dash:profile:isoff-live:2011");
	} else {
		mpd->profiles = gf_strdup("urn:mpeg:dash:profile:isoff-main:2011");
	}

	return GF_OK;
}

GF_EXPORT
GF_Err gf_m3u8_to_mpd(const char *m3u8_file, const char *base_url,
                      const char *mpd_file,
                      u32 reload_count, char *mimeTypeForM3U8Segments, Bool do_import, Bool use_mpd_templates, GF_FileDownload *getter,
                      GF_MPD *mpd, Bool parse_sub_playlist, Bool keep_files)
{
	GF_Err e;
	char *title;
	u32 i, j, k;
	Double update_interval;
	MasterPlaylist *pl = NULL;
	Stream *stream;
	PlaylistElement *pe, *the_pe;
	Bool is_end;
	u32 max_dur = 0;

	// first, we always need to parse the master playlist
	e = gf_m3u8_parse_master_playlist(m3u8_file, &pl, base_url);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[M3U8] Failed to parse root playlist '%s', error = %s\n", m3u8_file, gf_error_to_string(e)));
		gf_m3u8_master_playlist_del(&pl);
		return e;
	}
	if (mpd_file == NULL) {
		if (!keep_files) gf_delete_file(m3u8_file);
		mpd_file = m3u8_file;
	}

	the_pe = NULL;
	pe = NULL;
	i = 0;
	assert(pl);
	assert(pl->streams);
	while ((stream = gf_list_enum(pl->streams, &i))) {
		j = 0;
		while (NULL != (pe = gf_list_enum(stream->variants, &j))) {
			Bool found = GF_FALSE;
			char *suburl;
			if (!pe->url)
				continue;

			/* filter out duplicated entries (seen on M6 m3u8) */
			for (k=0; k<j-1; ++k) {
				PlaylistElement *a_pe = gf_list_get(stream->variants, k);
				if (a_pe->url && pe->url && !strcmp(a_pe->url, pe->url)) {
					found = GF_TRUE;
					break;
				}
			}
			if (found)
				continue;

			the_pe = pe;
			suburl = NULL;
			if (!strstr(pe->url, ".m3u8"))
				continue; /*not HLS*/

			if (!parse_sub_playlist)
				continue;

			if (strcmp(base_url, pe->url))
				suburl = gf_url_concatenate(base_url, pe->url);

			if (!suburl || !strcmp(base_url, suburl)) {
				if (suburl)
					gf_free(suburl);
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MPD Generator] Not downloading, programs are identical for %s...\n", pe->url));
				continue;
			}

			if (getter && getter->new_session && getter->del_session && getter->get_cache_name) {
				e = getter->new_session(getter, suburl);
				if (e) {
					gf_free(suburl);
					pe->load_error = e;
					continue;
				}
				if (e == GF_OK) {
					pe->load_error = gf_m3u8_parse_sub_playlist(getter->get_cache_name(getter), &pl, suburl, stream, pe);
				}
				//getter->del_session(getter);
			} else { /* for use in MP4Box */
				if (strstr(suburl, "://")) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MPD Generator] Downloading %s...\n", suburl));
					e = gf_dm_wget(suburl, "tmp.m3u8", 0, 0, NULL);
					if (e == GF_OK) {
						e = gf_m3u8_parse_sub_playlist("tmp.m3u8", &pl, suburl, stream, pe);
					} else {
						GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD Generator] Download failed for %s\n", suburl));
						e = GF_OK;
					}
					gf_delete_file("tmp.m3u8");
				} else {
					e = gf_m3u8_parse_sub_playlist(suburl, &pl, suburl, stream, pe);
				}
				if (e) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[M3U8toMPD] Failed to parse subplaylist %s\n", suburl));
				}
				
			}
			gf_free(suburl);
		}
		if (max_dur < (u32) stream->computed_duration) {
			max_dur = (u32) stream->computed_duration;
		}
	}

	is_end = !pl->playlist_needs_refresh;
	if (!the_pe) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD Generator] The M3U8 playlist is not correct.\n"));
		return GF_BAD_PARAM;
	}

	/*update interval is set to the duration of the last media file with rules defined in http live streaming RFC section 6.3.4*/
	switch (reload_count) {
	case 0:
		update_interval = the_pe->duration_info;
		break;
	case 1:
		update_interval = (Double)the_pe->duration_info / 2;
		break;
	case 2:
		update_interval = 3 * ((Double)the_pe->duration_info / 2);
		break;
	default:
		update_interval = 3 * the_pe->duration_info;
		break;
	}
	if (is_end || ((the_pe->element_type == TYPE_PLAYLIST) && the_pe->element.playlist.is_ended)) {
		update_interval = 0;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MPD Generator] No need to refresh playlist!\n"));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MPD Generator] Playlist will be refreshed every %g seconds, len=%d\n", update_interval, the_pe->duration_info));
	}

	title = the_pe->title;
	if (!title || strlen(title) < 2)
		title = the_pe->url;

	assert(mpd_file);
	assert(mpd);

	e = gf_m3u8_fill_mpd_struct(pl, m3u8_file, base_url, mpd_file, title, update_interval, mimeTypeForM3U8Segments, do_import, use_mpd_templates, is_end,  max_dur, mpd, parse_sub_playlist);

	gf_m3u8_master_playlist_del(&pl);

	//if local file force static
	if (strstr(base_url, "://")==NULL)
		mpd->type = GF_MPD_TYPE_STATIC;

	return e;
}

GF_EXPORT
GF_Err gf_m3u8_solve_representation_xlink(GF_MPD_Representation *rep, GF_FileDownload *getter, Bool *is_static, u64 *duration)
{
	GF_Err e;
	MasterPlaylist *pl = NULL;
	Stream *stream;
	PlaylistElement *pe;
	u32 k, count_elements;
	u64 start_time=0;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASH] Solving m3u8 variant playlist %s\n", rep->segment_list->xlink_href));

	if (!getter || !getter->new_session || !getter->del_session || !getter->get_cache_name) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] FileDownloader not found\n"));
		return GF_BAD_PARAM;
	}

	if (gf_url_is_local(rep->segment_list->xlink_href)) {
		e = gf_m3u8_parse_master_playlist(rep->segment_list->xlink_href, &pl, rep->segment_list->xlink_href);
	} else {
		e = getter->new_session(getter, rep->segment_list->xlink_href);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Download failed for %s\n", rep->segment_list->xlink_href));
			return e;
		}
		e = gf_m3u8_parse_master_playlist(getter->get_cache_name(getter), &pl, rep->segment_list->xlink_href);
	}
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[M3U8] Failed to parse playlist %s\n", rep->segment_list->xlink_href));
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	assert(pl);
	assert(pl->streams);
	assert(gf_list_count(pl->streams) == 1);

	if (is_static) {
		*is_static = pl->playlist_needs_refresh ? GF_FALSE : GF_TRUE;
	}

	stream = (Stream *)gf_list_get(pl->streams, 0);
	assert(gf_list_count(stream->variants) == 1);
	pe = (PlaylistElement *)gf_list_get(stream->variants, 0);

	if (duration) {
		*duration = (u32) (stream->computed_duration * 1000);
	}
	
	if (pe->init_segment_url) {
		if (!rep->segment_list->initialization_segment) {
			GF_SAFEALLOC(rep->segment_list->initialization_segment, GF_MPD_URL);

			if (strstr(pe->init_segment_url, "mp4") || strstr(pe->init_segment_url, "MP4")) {
				if (rep->mime_type) gf_free(rep->mime_type);
				rep->mime_type = gf_strdup("video/mp4");
			}
			rep->segment_list->initialization_segment->sourceURL = pe->init_segment_url;
			pe->init_segment_url=NULL;

			if (pe->init_byte_range_end) {
				GF_SAFEALLOC(rep->segment_list->initialization_segment->byte_range, GF_MPD_ByteRange);
				 rep->segment_list->initialization_segment->byte_range->start_range = pe->init_byte_range_start;
				 rep->segment_list->initialization_segment->byte_range->end_range = pe->init_byte_range_end;
			}
		}
	}
	rep->starts_with_sap = pl->independent_segments ? 1: 3;
	
	rep->segment_list->duration = (u64) (pe->duration_info * 1000);
	rep->segment_list->timescale = 1000;
	rep->m3u8_media_seq_min = pe->element.playlist.media_seq_min;
	rep->m3u8_media_seq_max = pe->element.playlist.media_seq_max;
	if (!rep->segment_list->segment_URLs)
		rep->segment_list->segment_URLs = gf_list_new();
	count_elements = gf_list_count(pe->element.playlist.elements);
	for (k=0; k<count_elements; k++) {
		GF_MPD_SegmentURL *segment_url;
		PlaylistElement *elt = gf_list_get(pe->element.playlist.elements, k);
		if (!elt)
			continue;

		//NOTE: for GPAC now, we disable stream AAC to avoid the problem when switching quality. It should be improved later !
		if (elt && strstr(elt->url, ".aac")) {
			rep->playback.disabled = GF_TRUE;
			return GF_OK;
		}

		GF_SAFEALLOC(segment_url, GF_MPD_SegmentURL);
		if (!segment_url) {
			return GF_OUT_OF_MEM;
		}
		gf_list_add(rep->segment_list->segment_URLs, segment_url);
		segment_url->media = gf_url_concatenate(pe->url, elt->url);

		if (! elt->utc_start_time) elt->utc_start_time = start_time;
		segment_url->hls_utc_start_time = elt->utc_start_time;
		start_time = elt->utc_start_time + (u64) (1000*elt->duration_info);

		if (elt->drm_method != DRM_NONE) {
			if (elt->key_uri) {
				segment_url->key_url = elt->key_uri;
				elt->key_uri=NULL;
				memcpy(segment_url->key_iv, elt->key_iv, sizeof(bin128));
			}
		}
		if (elt->byte_range_end) {
			GF_SAFEALLOC(segment_url->media_range, GF_MPD_ByteRange);
			segment_url->media_range->start_range = elt->byte_range_start;
			segment_url->media_range->end_range = elt->byte_range_end;
		}
	}

	if (!gf_list_count(rep->segment_list->segment_URLs)) {
		gf_list_del(rep->segment_list->segment_URLs);
		rep->segment_list->segment_URLs = NULL;
	}

	gf_free(rep->segment_list->xlink_href);
	rep->segment_list->xlink_href = NULL;

	gf_m3u8_master_playlist_del(&pl);

	return GF_OK;
}

GF_EXPORT
GF_MPD_SegmentList *gf_mpd_solve_segment_list_xlink(GF_MPD *mpd, GF_XMLNode *root)
{
	return gf_mpd_parse_segment_list(mpd, root);
}

GF_EXPORT
void gf_mpd_delete_segment_list(GF_MPD_SegmentList *segment_list)
{
	gf_mpd_segment_list_free(segment_list);
}

/*time is given in ms*/
void gf_mpd_print_date(FILE *out, char *name, u64 time)
{
	time_t gtime;
	struct tm *t;
	u32 sec;
	u32 ms;
	gtime = time / 1000;
	gtime -= GF_NTP_SEC_1900_TO_1970;
	sec = (u32)(time / 1000);
	ms = (u32)(time - ((u64)sec) * 1000);

#ifdef _WIN32_WCE
	*(LONGLONG *) &filet = (sec - GF_NTP_SEC_1900_TO_1970) * 10000000 + TIMESPEC_TO_FILETIME_OFFSET;
	FileTimeToSystemTime(&filet, &syst);
	fprintf(out, " %s=\"%d-%02d-%02dT%02d:%02d:%02d.%03dZ\"", syst.wYear, syst.wMonth, syst.wDay, syst.wHour, syst.wMinute, syst.wSecond, (u32) ms);
#else
	t = gmtime(&gtime);
	fprintf(out, " %s=\"%d-%02d-%02dT%02d:%02d:%02d.%03dZ\"", name, 1900 + t->tm_year, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, ms);
#endif

}

void gf_mpd_print_duration(FILE *out, char *name, u64 duration_in_ms, Bool UseHoursAndMinutes)
{
	u32 h, m;
	Double s;
	h = (u32) (duration_in_ms / 3600000);
	m = (u32) (duration_in_ms/ 60000) - h*60;
	s = ((Double) duration_in_ms/1000.0) - h*3600 - m*60;

	fprintf(out, " %s=\"PT", name);
	if(UseHoursAndMinutes)
		fprintf(out, "%dH%dM", h, m);
	fprintf(out, "%.3fS\"", s);
}

void gf_mpd_print_base_url(FILE *out, GF_MPD_BaseURL *base_URL, char *indent)
{
	fprintf(out, "   %s<BaseURL", indent);
	if (base_URL->service_location)
		fprintf(out, " serviceLocation=\"%s\"", base_URL->service_location);
	if (base_URL->byte_range)
		fprintf(out, " byteRange=\""LLD"-"LLD"\"", base_URL->byte_range->start_range, base_URL->byte_range->end_range);
	fprintf(out, ">%s</BaseURL>\n", base_URL->URL);
}

static void gf_mpd_print_base_urls(FILE *out, GF_List *base_URLs, char *indent)
{
	GF_MPD_BaseURL *url;
	u32 i;
	i=0;

	while ((url = (GF_MPD_BaseURL *)gf_list_enum(base_URLs, &i))) {
		gf_mpd_print_base_url(out, url, indent);
	}
}

static void gf_mpd_print_url(FILE *out, GF_MPD_URL *url, char *name, char *indent)
{
	fprintf(out, "%s<%s", indent, name);
	if (url->byte_range) fprintf(out, " range=\""LLD"-"LLD"\"", url->byte_range->start_range, url->byte_range->end_range);
	if (url->sourceURL) fprintf(out, " sourceURL=\"%s\"", url->sourceURL);
	fprintf(out, "/>\n");
}

static void gf_mpd_print_segment_base_attr(FILE *out, GF_MPD_SegmentBase *s)
{
	if (s->timescale) fprintf(out, " timescale=\"%d\"", s->timescale);
	if (s->presentation_time_offset) fprintf(out, " presentationTimeOffset=\""LLU"\"", s->presentation_time_offset);
	if (s->index_range_exact) fprintf(out, " indexRangeExact=\"true\"");
	if (s->index_range) fprintf(out, " indexRange=\""LLD"-"LLD"\"", s->index_range->start_range, s->index_range->end_range);	
	if (s->availability_time_offset) fprintf(out, " availabilityTimeOffset=\"%g\"", s->availability_time_offset);
	if (s->time_shift_buffer_depth)
		gf_mpd_print_duration(out, "timeShiftBufferDepth", s->time_shift_buffer_depth, GF_TRUE);
}

void gf_mpd_print_segment_base(FILE *out, GF_MPD_SegmentBase *s, char *indent)
{
	fprintf(out, "%s<SegmentBase", indent);
	gf_mpd_print_segment_base_attr(out, s);
	fprintf(out, ">\n");

	char tmp_indent[256];
	sprintf(tmp_indent, "%s ",indent);
	
	if (s->initialization_segment) gf_mpd_print_url(out, s->initialization_segment, "Initialization", tmp_indent);
	if (s->representation_index) gf_mpd_print_url(out, s->representation_index, "RepresentationIndex", indent);

	fprintf(out, "%s</SegmentBase>\n", indent);
}

static void gf_mpd_print_segment_timeline(FILE *out, GF_MPD_SegmentTimeline *tl, char *indent)
{
	u32 i;
	GF_MPD_SegmentTimelineEntry *se;
	fprintf(out, "%s <SegmentTimeline>\n", indent);

	i = 0;
	while ( (se = gf_list_enum(tl->entries, &i))) {
		fprintf(out, "%s <S", indent);
		if (se->start_time) fprintf(out, " t=\""LLD"\"", se->start_time);
		if (se->duration) fprintf(out, " d=\"%d\"", se->duration);
		if (se->repeat_count) fprintf(out, " r=\"%d\"", se->repeat_count);
		fprintf(out, "/>\n");
	}
	fprintf(out, "%s </SegmentTimeline>\n", indent);
}

GF_MPD_SegmentTimeline *gf_mpd_segmentimeline_new(void)
{
	GF_MPD_SegmentTimeline *seg_tl;
	GF_SAFEALLOC(seg_tl, GF_MPD_SegmentTimeline);
	if(!seg_tl->entries)seg_tl->entries=gf_list_new();
	return seg_tl;
}

static u32 gf_mpd_print_multiple_segment_base(FILE *out, GF_MPD_MultipleSegmentBase *ms, char *indent, Bool close_if_no_child)
{
	gf_mpd_print_segment_base_attr(out, (GF_MPD_SegmentBase *)ms);

	if (ms->duration) fprintf(out, " duration=\""LLD"\"", ms->duration);
	if (ms->start_number != (u32) -1) fprintf(out, " startNumber=\"%d\"", ms->start_number);


	if (!ms->bitstream_switching_url && !ms->segment_timeline && !ms->initialization_segment && !ms->representation_index) {
		if (close_if_no_child) fprintf(out, "/");
		fprintf(out, ">\n");
		return 1;
	}
	fprintf(out, ">\n");

	if (ms->initialization_segment) gf_mpd_print_url(out, ms->initialization_segment, "Initialization", indent);
	if (ms->representation_index) gf_mpd_print_url(out, ms->representation_index, "RepresentationIndex", indent);

	if (ms->segment_timeline) gf_mpd_print_segment_timeline(out, ms->segment_timeline, indent);
	if (ms->bitstream_switching_url) gf_mpd_print_url(out, ms->bitstream_switching_url, "BitstreamSwitching", indent);
	return 0;
}

static void gf_mpd_print_segment_list(FILE *out, GF_MPD_SegmentList *s, char *indent)
{
	fprintf(out, "%s<SegmentList", indent);
	if (s->xlink_href) {
		fprintf(out, " xlink:href=\"%s\"", s->xlink_href);
		if (s->xlink_actuate_on_load)
			fprintf(out, " actuate=\"onLoad\"");
	}
	char tmp_indent[256];
	sprintf(tmp_indent, "%s ",indent);
	gf_mpd_print_multiple_segment_base(out, (GF_MPD_MultipleSegmentBase *)s, tmp_indent, GF_FALSE);
	
	sprintf(tmp_indent, "%s  ",indent);

	if (s->segment_URLs) {
		u32 i;
		GF_MPD_SegmentURL *url;
		i = 0;
		while ( (url = gf_list_enum(s->segment_URLs, &i))) {
			fprintf(out, "%s<SegmentURL", tmp_indent);
			if (url->media) fprintf(out, " media=\"%s\"", url->media);
			if (url->index) fprintf(out, " index=\"%s\"", url->index);
			if (url->media_range && url->media_range->end_range!=0) fprintf(out, " mediaRange=\""LLD"-"LLD"\"", url->media_range->start_range, url->media_range->end_range);
			if (url->index_range && url->index_range->end_range!=0) fprintf(out, " indexRange=\""LLD"-"LLD"\"", url->index_range->start_range, url->index_range->end_range);
			if (url->key_url) {
				u32 idx;
				fprintf(out, " hls:keyMethod=\"aes-128\" hls:KeyURL=%s hls:KeyIV=\"", url->key_url);
				for (idx=0; idx<16; idx++) {
					fprintf(out, "%02x", url->key_iv[idx]);
				}
				fprintf(out, "\"");
			}
			fprintf(out, "/>\n");
		}
	}
	fprintf(out, "%s</SegmentList>\n", indent);
}

static void gf_mpd_print_segment_template(FILE *out, GF_MPD_SegmentTemplate *s, char *indent)
{
	fprintf(out, "%s<SegmentTemplate", indent);

	if (s->media) fprintf(out, " media=\"%s\"", s->media);
	if (s->index) fprintf(out, " index=\"%s\"", s->index);
	if (s->initialization) fprintf(out, " initialization=\"%s\"", s->initialization);
	if (s->bitstream_switching) fprintf(out, " bitstreamSwitching=\"%s\"", s->bitstream_switching);

	if (gf_mpd_print_multiple_segment_base(out, (GF_MPD_MultipleSegmentBase *)s, indent, GF_TRUE))
		return;

	fprintf(out, "%s</SegmentTemplate>\n", indent);
}

static void gf_mpd_extensible_print_attr(FILE *out, GF_MPD_ExtensibleVirtual *item)
{
	if (item->attributes) {
		u32 j=0;
		GF_XMLAttribute *att;
		while ((att = (GF_XMLAttribute *)gf_list_enum(item->attributes, &j))) {
			fprintf(out, " %s=\"%s\"", att->name, att->value);
		}
	}
}

static void gf_mpd_extensible_print_nodes(FILE *out, GF_MPD_ExtensibleVirtual *item)
{
	if (item->children) {
		u32 j=0;
		GF_XMLNode *child;
		fprintf(out, ">\n");
		while ((child = (GF_XMLNode *)gf_list_enum(item->children, &j))) {
			char *txt = gf_xml_dom_serialize(child, 0);
			fprintf(out, "%s", txt);
			gf_free(txt);
		}
	}
}

static void gf_mpd_print_descriptors(FILE *out, GF_List *desc_list, char *desc_name, char *indent)
{
	u32 i=0;
	GF_MPD_Descriptor *desc;
	while ((desc = (GF_MPD_Descriptor *)gf_list_enum(desc_list, &i))) {
		fprintf(out, "%s<%s", indent, desc_name);
		if (desc->id) fprintf(out, " id=\"%s\"", desc->id);
		if (desc->scheme_id_uri) fprintf(out, " schemeIdUri=\"%s\"", desc->scheme_id_uri);
		if (desc->value) fprintf(out, " value=\"%s\"", desc->value);

		if (desc->attributes) gf_mpd_extensible_print_attr(out, (GF_MPD_ExtensibleVirtual*)desc);

		if (desc->children) {
			gf_mpd_extensible_print_nodes(out, (GF_MPD_ExtensibleVirtual*)desc);
			fprintf(out, "%s</%s>\n", indent, desc_name);
		} else {
			fprintf(out, "/>\n");
		}
	}
}

static u32 gf_mpd_print_common_attributes(FILE *out, GF_MPD_CommonAttributes *ca, char *indent, Bool can_close)
{
	if (ca->profiles) fprintf(out, " profiles=\"%s\"", ca->profiles);
	if (ca->mime_type) fprintf(out, " mimeType=\"%s\"", ca->mime_type);
	if (ca->codecs) fprintf(out, " codecs=\"%s\"", ca->codecs);
	if (ca->width) fprintf(out, " width=\"%d\"", ca->width);
	if (ca->height) fprintf(out, " height=\"%d\"", ca->height);
	if (ca->framerate){
		fprintf(out, " frameRate=\"%d",ca->framerate->num);
		if(ca->framerate->den>1)fprintf(out, "/%d",ca->framerate->den);
		fprintf(out, "\"");
	}
	if (ca->sar) fprintf(out, " sar=\"%d:%d\"", ca->sar->num, ca->sar->den);
	if (ca->samplerate) fprintf(out, " audioSamplingRate=\"%d\"", ca->samplerate);
	if (ca->segmentProfiles) fprintf(out, " segmentProfiles=\"%s\"", ca->segmentProfiles);
	if (ca->maximum_sap_period) fprintf(out, " maximumSAPPeriod=\"%d\"", ca->maximum_sap_period);
	if (ca->starts_with_sap) fprintf(out, " startWithSAP=\"%d\"", ca->starts_with_sap);
	if ((ca->max_playout_rate!=1.0)) fprintf(out, " maxPlayoutRate=\"%g\"", ca->max_playout_rate);
	if (ca->coding_dependency) fprintf(out, " codingDependency=\"true\"");
	if (ca->scan_type!=GF_MPD_SCANTYPE_UNKNWON) fprintf(out, " scanType=\"%s\"", ca->scan_type==GF_MPD_SCANTYPE_PROGRESSIVE ? "progressive" : "interlaced");

	if (can_close && !gf_list_count(ca->frame_packing) && !gf_list_count(ca->audio_channels) && !gf_list_count(ca->content_protection) && !gf_list_count(ca->essential_properties) && !gf_list_count(ca->supplemental_properties) && !ca->isobmf_tracks) {
		return 1;
	}

	if (ca->isobmf_tracks) {
		u32 k=0;
		GF_MPD_ISOBMFInfo *info;
		fprintf(out, "%s<ISOBMFInfo>\n", indent);
		while ((info = (GF_MPD_ISOBMFInfo *) gf_list_enum(ca->isobmf_tracks, &k))) {
			fprintf(out, "%s  <ISOBMFTrack", indent);
			if (info->trackID) fprintf(out, " ID=\"%d\"", info->trackID);
			if (info->stsd) fprintf(out, " stsd=\"%s\"", info->stsd);
			if (info->mediaOffset) fprintf(out, " offset=\""LLD"\"", info->mediaOffset);
			fprintf(out, "/>\n");
		}
		fprintf(out, "%s</ISOBMFInfo>\n", indent);
	}

	return 0;
}

static u32 gf_mpd_print_common_children(FILE *out, GF_MPD_CommonAttributes *ca, char *indent)
{
	gf_mpd_print_descriptors(out, ca->frame_packing, "Framepacking", indent);
	gf_mpd_print_descriptors(out, ca->audio_channels, "AudioChannelConfiguration", indent);
	gf_mpd_print_descriptors(out, ca->content_protection, "ContentProtection", indent);
	gf_mpd_print_descriptors(out, ca->essential_properties, "EssentialProperty", indent);
	gf_mpd_print_descriptors(out, ca->supplemental_properties, "SupplementalProperty", indent);

	return 0;
}

static void gf_mpd_print_representation(GF_MPD_Representation const * const rep, FILE *out)
{
	Bool can_close = GF_FALSE;
	u32 i;
	GF_MPD_other_descriptors *rsld;

	fprintf(out, "   <Representation");
	if (rep->id) fprintf(out, " id=\"%s\"", rep->id);

	if (!gf_list_count(rep->base_URLs) && !rep->segment_base && !rep->segment_template && !rep->segment_list && !gf_list_count(rep->sub_representations)) {
		can_close = 1;
	}

	gf_mpd_print_common_attributes(out, (GF_MPD_CommonAttributes*)rep, "    ", can_close);

	if (rep->bandwidth) fprintf(out, " bandwidth=\"%d\"", rep->bandwidth);
	if (rep->quality_ranking) fprintf(out, " qualityRanking=\"%d\"", rep->quality_ranking);
	if (rep->dependency_id) fprintf(out, " dependencyId=\"%s\"", rep->dependency_id);
	if (rep->media_stream_structure_id) fprintf(out, " mediaStreamStructureId=\"%s\"", rep->media_stream_structure_id);

	if(can_close){
		fprintf(out, "/>\n");
		return;
	}
	fprintf(out, ">\n");
	
	if (rep->other_descriptors){
		i=0;
		while ( (rsld = (GF_MPD_other_descriptors*) gf_list_enum(rep->other_descriptors, &i))) {
			fprintf(out, "    %s\n",rsld->xml_desc);
		}
	}

	gf_mpd_print_common_children(out, (GF_MPD_CommonAttributes*)rep, "    ");

	gf_mpd_print_base_urls(out, rep->base_URLs, " ");
	if (rep->segment_base) {
		gf_mpd_print_segment_base(out, rep->segment_base, "    ");
	}
	if (rep->segment_list) {
		gf_mpd_print_segment_list(out, rep->segment_list, "    ");
	}
	if (rep->segment_template) {
		gf_mpd_print_segment_template(out, rep->segment_template, "    ");
	}
	/*TODO
				e = gf_mpd_parse_subrepresentation(rep->sub_representations, child);
				if (e) return e;
	*/


	fprintf(out, "   </Representation>\n");
}

static void gf_mpd_print_adaptation_set(GF_MPD_AdaptationSet const * const as, FILE *out)
{
	u32 i;
	GF_MPD_Representation *rep;
	GF_MPD_other_descriptors *Asld;
	GF_MPD_Descriptor *desc;

	fprintf(out, "  <AdaptationSet");

	if (as->xlink_href) {
		fprintf(out, " xlink:href=\"%s\"", as->xlink_href);
		if (as->xlink_actuate_on_load)
			fprintf(out, " actuate=\"onLoad\"");
	}
	if (as->segment_alignment) fprintf(out, " segmentAlignment=\"true\"");
	if (as->id) fprintf(out, " id=\"%d\"", as->id);
	if (as->group !=  (u32) -1) fprintf(out, " group=\"%d\"", as->group);
	if (as->min_bandwidth) fprintf(out, " minBandwidth=\"%d\"", as->min_bandwidth);
	if (as->max_bandwidth) fprintf(out, " maxBandwidth=\"%d\"", as->max_bandwidth);
	if (as->min_width) fprintf(out, " minWidth=\"%d\"", as->min_width);
	if (as->max_width) fprintf(out, " maxWidth=\"%d\"", as->max_width);
	if (as->min_height) fprintf(out, " minHeight=\"%d\"", as->min_height);
	if (as->max_height) fprintf(out, " maxHeight=\"%d\"", as->max_height);
	if (as->min_framerate) fprintf(out, " minFrameRate=\"%d\"", as->min_framerate);
	if (as->max_framerate) fprintf(out, " maxFrameRate=\"%d\"", as->max_framerate);	
	if (as->par && (as->par->num !=0 ) && (as->par->num != 0))
		fprintf(out, " par=\"%d:%d\"", as->par->num, as->par->den);
	if (as->lang) fprintf(out, " lang=\"%s\"", as->lang);	
	if (as->bitstream_switching) fprintf(out, " bitstreamSwitching=\"true\"");
	if (as->subsegment_alignment) fprintf(out, " subsegmentAlignment=\"true\"");
	if (as->subsegment_starts_with_sap) fprintf(out, " subsegmentStartsWithSAP=\"%d\"", as->subsegment_starts_with_sap);


	gf_mpd_print_common_attributes(out, (GF_MPD_CommonAttributes*)as, "   ", 0);

	fprintf(out, ">\n");
	
	i=0;
	while ( (Asld = (GF_MPD_other_descriptors*) gf_list_enum(as->other_descriptors, &i))) {
		fprintf(out, "  %s\n",Asld->xml_desc);
	}

	gf_mpd_print_common_children(out, (GF_MPD_CommonAttributes*)as, "   ");

	gf_mpd_print_base_urls(out, as->base_URLs, " ");

	gf_mpd_print_descriptors(out, as->accessibility, "Accessibility", "   ");
	gf_mpd_print_descriptors(out, as->role, "Role", "   ");
	gf_mpd_print_descriptors(out, as->rating, "Rating", "   ");
	gf_mpd_print_descriptors(out, as->viewpoint, "Viewpoint", "   ");

//			e = gf_mpd_parse_content_component(set->content_component, child);

	if (as->segment_base) {
		gf_mpd_print_segment_base(out, as->segment_base, "   ");
	}
	if (as->segment_list) {
		gf_mpd_print_segment_list(out, as->segment_list, "   ");
	}
	if (as->segment_template) {
		gf_mpd_print_segment_template(out, as->segment_template, "   ");
	}


	i=0;
	while ((rep = (GF_MPD_Representation *)gf_list_enum(as->representations, &i))) {
		gf_mpd_print_representation(rep, out);
	}
	fprintf(out, "  </AdaptationSet>\n");


}

void gf_mpd_print_period(GF_MPD_Period const * const period, Bool is_dynamic, FILE *out)
{
	GF_MPD_AdaptationSet *as;
	GF_MPD_other_descriptors *pld;
	u32 i;
	fprintf(out, " <Period");
	if (period->xlink_href) {
		fprintf(out, " xlink:href=\"%s\"", period->xlink_href);
		if (period->xlink_actuate_on_load)
			fprintf(out, " actuate=\"onLoad\"");
	}
	if (period->ID)
		fprintf(out, " id=\"%s\"", period->ID);
	if (is_dynamic || period->start)
		gf_mpd_print_duration(out, "start", period->start, GF_TRUE);
	if (period->duration)
		gf_mpd_print_duration(out, "duration", period->duration, GF_TRUE);
	if (period->bitstream_switching)
		fprintf(out, " bitstreamSwitching=\"true\"");

	fprintf(out, ">\n");
	
	i=0;
	while ( (pld = (GF_MPD_other_descriptors*) gf_list_enum(period->other_descriptors, &i))) {
		fprintf(out, "  %s\n",pld->xml_desc);
	}
	

	gf_mpd_print_base_urls(out, period->base_URLs, " ");

	if (period->segment_base) {
		gf_mpd_print_segment_base(out, period->segment_base, " ");
	}
	if (period->segment_list) {
		gf_mpd_print_segment_list(out, period->segment_list, " ");
	}
	if (period->segment_template) {
		gf_mpd_print_segment_template(out, period->segment_template, " ");
	}

	i=0;
	while ( (as = (GF_MPD_AdaptationSet *) gf_list_enum(period->adaptation_sets, &i))) {
		gf_mpd_print_adaptation_set(as, out);
	}
	fprintf(out, " </Period>\n");

}

static GF_Err mpd_write_generation_comment(GF_MPD const * const mpd, FILE *out)
{
	u64 time_ms;
	time_t gtime;
	struct tm *t;
	u32 sec;

	time_ms = mpd->publishTime;
	sec = (u32)(time_ms / 1000);
	time_ms -= ((u64)sec) * 1000;
	assert(time_ms<1000);

	gtime = sec - GF_NTP_SEC_1900_TO_1970;
	t = gmtime(&gtime);
	if(!mpd->force_test_mode){
		fprintf(out, "<!-- MPD file Generated with GPAC version "GPAC_FULL_VERSION" ");
		fprintf(out, " at %d-%02d-%02dT%02d:%02d:%02d.%03dZ", 1900 + t->tm_year, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, (u32)time_ms);
		fprintf(out, "-->\n");
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASH] Generating MPD at time %d-%02d-%02dT%02d:%02d:%02d.%03dZ\n", 1900 + t->tm_year, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, (u32)time_ms));	
	return GF_OK;
}

static GF_Err gf_mpd_write(GF_MPD const * const mpd, FILE *out)
{
	u32 i;
	GF_MPD_ProgramInfo *info;
	char *text;
	GF_MPD_Period *period;

	if (!mpd->xml_namespace) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD] No namespace found while writing. Setting to default.\n"));
	}

	fprintf(out, "<?xml version=\"1.0\"?>\n");
	mpd_write_generation_comment(mpd, out);
	fprintf(out, "<MPD xmlns=\"%s\"", (mpd->xml_namespace ? mpd->xml_namespace : "urn:mpeg:dash:schema:mpd:2011"));

	if (mpd->ID)
		fprintf(out, " ID=\"%s\"", mpd->ID);

	if (mpd->profiles)
		fprintf(out, " profiles=\"%s\"", mpd->profiles);
	if (mpd->min_buffer_time)
		gf_mpd_print_duration(out, "minBufferTime", mpd->min_buffer_time, GF_FALSE);

	fprintf(out," type=\"%s\"",(mpd->type == GF_MPD_TYPE_STATIC ? "static" : "dynamic"));

	if (mpd->type == GF_MPD_TYPE_DYNAMIC)
		gf_mpd_print_date(out, "availabilityStartTime", mpd->availabilityStartTime);
	if (mpd->availabilityEndTime)
		gf_mpd_print_date(out, "availabilityEndTime", mpd->availabilityEndTime);
	if (mpd->publishTime && mpd->type != GF_MPD_TYPE_STATIC)
		gf_mpd_print_date(out, "publishTime", mpd->publishTime);
	if (mpd->media_presentation_duration)
		gf_mpd_print_duration(out, "mediaPresentationDuration", mpd->media_presentation_duration, GF_TRUE);
	if (mpd->minimum_update_period)
		gf_mpd_print_duration(out, "minimumUpdatePeriod", mpd->minimum_update_period, GF_TRUE);
	if (mpd->time_shift_buffer_depth)
		gf_mpd_print_duration(out, "timeShiftBufferDepth", mpd->time_shift_buffer_depth, GF_TRUE);
	if (mpd->suggested_presentation_delay)
		gf_mpd_print_duration(out, "suggestedPresentationDelay", mpd->suggested_presentation_delay, GF_TRUE);
	if (mpd->max_segment_duration)
		gf_mpd_print_duration(out, "maxSegmentDuration", mpd->max_segment_duration*1000, GF_TRUE);
	if (mpd->max_subsegment_duration)
		gf_mpd_print_duration(out, "maxSubsegmentDuration", mpd->max_subsegment_duration, GF_TRUE);

	if (mpd->attributes) gf_mpd_extensible_print_attr(out, (GF_MPD_ExtensibleVirtual*)mpd);

	//fprintf(out, ">\n");

	if (mpd->children) {
		gf_mpd_extensible_print_nodes(out, (GF_MPD_ExtensibleVirtual*)mpd);
	}

	i=0;
	while ((info = (GF_MPD_ProgramInfo *)gf_list_enum(mpd->program_infos, &i))) {
		fprintf(out, " <ProgramInformation");
		if (info->lang) {
			fprintf(out, " lang=\"%s\"", info->lang);
		}
		if (info->more_info_url) {
			fprintf(out, " moreInformationURL=\"%s\"", info->more_info_url);
		}
		fprintf(out, ">\n");
		if (info->title) {
			fprintf(out, "  <Title>%s</Title>\n", info->title);
		}
		if (info->source) {
			fprintf(out, "  <Source>%s</Source>\n", info->source);
		}
		if (info->copyright) {
			fprintf(out, "  <Copyright>%s</Copyright>\n", info->copyright);
		}
		fprintf(out, " </ProgramInformation>\n\n");
	}

	gf_mpd_print_base_urls(out, mpd->base_URLs, " ");

	i=0;
	while ((text = (char *)gf_list_enum(mpd->locations, &i))) {
		fprintf(out, " <Location>%s</Location>\n", text);
	}

	/*
		i=0;
		while ((text = (char *)gf_list_enum(mpd->metrics, &i))) {

		}
	*/

	i=0;
	while ((period = (GF_MPD_Period *)gf_list_enum(mpd->periods, &i))) {
		gf_mpd_print_period(period, mpd->type==GF_MPD_TYPE_DYNAMIC, out);
	}

	fprintf(out, "</MPD>");

	return GF_OK;
}

GF_EXPORT
GF_Err gf_mpd_write_file(GF_MPD const * const mpd, const char *file_name)
{
	GF_Err e;
	FILE *out;
	if (!strcmp(file_name, "std")) out = stdout;
	else {
		out = gf_fopen(file_name, "wb");
		if (!out) return GF_IO_ERR;
	}

	e = gf_mpd_write(mpd, out);
	gf_fclose(out);
	return e;
}

GF_EXPORT
u32 gf_mpd_get_base_url_count(GF_MPD *mpd, GF_MPD_Period *period, GF_MPD_AdaptationSet *set, GF_MPD_Representation *rep)
{
	u32 base_url_count, i;
	base_url_count = 1;
	i = gf_list_count(mpd->base_URLs);
	if (i>1) base_url_count *= i;
	i = gf_list_count(period->base_URLs);
	if (i>1) base_url_count *= i;
	i = gf_list_count(set->base_URLs);
	if (i>1) base_url_count *= i;
	i = gf_list_count(rep->base_URLs);
	if (i>1) base_url_count *= i;

	return base_url_count;
}

static char *gf_mpd_get_base_url(GF_List *baseURLs, char *url, u32 *base_url_index)
{
	GF_MPD_BaseURL *url_child;
	u32 idx = 0;
	u32 nb_base = gf_list_count(baseURLs);
	if (nb_base>1) {
		u32 nb_bits = gf_get_bit_size(nb_base-1);
		u32 mask=0;
		u32 i=0;
		while (1) {
			mask |= 1;
			i++;
			if (i>=nb_bits) break;
			mask <<= 1;
		}
		idx = (*base_url_index) & mask;
		(*base_url_index) = (*base_url_index) >> nb_bits;
	} else {
		idx = 0;
	}
	
	url_child = gf_list_get(baseURLs, idx);
	if (url_child) {
		char *t_url = gf_url_concatenate(url, url_child->URL);
		gf_free(url);
		url = t_url;
	}
	return url;
}

GF_EXPORT
GF_Err gf_mpd_resolve_url(GF_MPD *mpd, GF_MPD_Representation *rep, GF_MPD_AdaptationSet *set, GF_MPD_Period *period, const char *mpd_url, u32 base_url_index, GF_MPD_URLResolveType resolve_type, u32 item_index, u32 nb_segments_removed, char **out_url, u64 *out_range_start, u64 *out_range_end, u64 *segment_duration_in_ms, Bool *is_in_base_url, char **out_key_url, bin128 *out_key_iv)
{
	GF_MPD_SegmentTimeline *timeline = NULL;
	u32 start_number = 1;
	u32 timescale=0;
	u64 duration=0;
	char *url;
	char *url_to_solve, *solved_template, *first_sep, *media_url;
	char *init_template, *index_template;

	if (!out_range_start || !out_range_end || !out_url || !mpd_url || !segment_duration_in_ms)
		return GF_BAD_PARAM;
	*out_range_start = *out_range_end = 0;
	*out_url = NULL;
	if (out_key_url) *out_key_url = NULL;
	/*resolve base URLs from document base (download location) to representation (media)*/
	url = gf_strdup(mpd_url);

	url = gf_mpd_get_base_url(mpd->base_URLs, url, &base_url_index);
	url = gf_mpd_get_base_url(period->base_URLs, url, &base_url_index);
	url = gf_mpd_get_base_url(set->base_URLs, url, &base_url_index);
	url = gf_mpd_get_base_url(rep->base_URLs, url, &base_url_index);
	assert(url);

	/*single URL*/
	if (!rep->segment_list && !set->segment_list && !period->segment_list && !rep->segment_template && !set->segment_template && !period->segment_template) {
		GF_MPD_URL *res_url;
		GF_MPD_SegmentBase *base_seg = NULL;
		if (item_index > 0)
			return GF_EOS;
		switch (resolve_type) {
		case GF_MPD_RESOLVE_URL_MEDIA:
		case GF_MPD_RESOLVE_URL_MEDIA_TEMPLATE:
			if (!url)
				return GF_NON_COMPLIANT_BITSTREAM;
			*out_url = url;
			return GF_OK;
		case GF_MPD_RESOLVE_URL_INIT:
		case GF_MPD_RESOLVE_URL_INDEX:
			res_url = NULL;
			base_seg = rep->segment_base;
			if (!base_seg) base_seg = set->segment_base;
			if (!base_seg) base_seg = period->segment_base;

			if (base_seg) {
				if (resolve_type == GF_MPD_RESOLVE_URL_INDEX) {
					res_url = base_seg->representation_index;
				} else {
					res_url = base_seg->initialization_segment;
				}
			}
			if (is_in_base_url) *is_in_base_url = 0;
			/*no initialization segment / index, use base URL*/
			if (res_url && res_url->sourceURL) {
				if (res_url->is_resolved) {
					*out_url = gf_strdup(res_url->sourceURL);
				} else {
					*out_url = gf_url_concatenate(url, res_url->sourceURL);
				}
				gf_free(url);
			} else {
				*out_url = url;
				if (is_in_base_url) *is_in_base_url = 1;
			}
			if (res_url && res_url->byte_range) {
				*out_range_start = res_url->byte_range->start_range;
				*out_range_end = res_url->byte_range->end_range;
			} else if (base_seg && base_seg->index_range && (resolve_type == GF_MPD_RESOLVE_URL_INDEX)) {
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
			if (period->segment_list->segment_URLs) segments = period->segment_list->segment_URLs;
			if (!timescale && period->segment_list->timescale) timescale = period->segment_list->timescale;
		}
		if (set->segment_list) {
			if (set->segment_list->initialization_segment) init_url = set->segment_list->initialization_segment;
			if (set->segment_list->segment_URLs) segments = set->segment_list->segment_URLs;
			if (!timescale && set->segment_list->timescale) timescale = set->segment_list->timescale;
		}
		if (rep->segment_list) {
			if (rep->segment_list->initialization_segment) init_url = rep->segment_list->initialization_segment;
			if (rep->segment_list->segment_URLs) segments = rep->segment_list->segment_URLs;
			if (!timescale && rep->segment_list->timescale) timescale = rep->segment_list->timescale;
		}


		segment_count = gf_list_count(segments);

		switch (resolve_type) {
		case GF_MPD_RESOLVE_URL_INIT:
			if (init_url) {
				if (init_url->sourceURL) {
					if (init_url->is_resolved) {
						*out_url = gf_strdup(init_url->sourceURL);
					} else {
						*out_url = gf_url_concatenate(url, init_url->sourceURL);
					}
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
		case GF_MPD_RESOLVE_URL_MEDIA:
		case GF_MPD_RESOLVE_URL_MEDIA_TEMPLATE:
			if (!url) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Media URL is not set in segment list\n"));
				return GF_SERVICE_ERROR;
			}
			if ((item_index >= segment_count) || ((s32) item_index < 0)) {
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
				*segment_duration_in_ms = (u32) ((Double) (segment->duration) * 1000.0 / timescale);
			}
			if (segment->key_url && out_key_url) {
				*out_key_url = gf_strdup((const char *) segment->key_url);
				if (out_key_iv)
					memcpy((*out_key_iv), segment->key_iv, sizeof(bin128) );
			}
			return GF_OK;
		case GF_MPD_RESOLVE_URL_INDEX:
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
		if (!timescale && period->segment_template->timescale) timescale = period->segment_template->timescale;
		if (!duration && period->segment_template->duration) duration = period->segment_template->duration;
	}
	if (set->segment_template) {
		if (set->segment_template->initialization) init_template = set->segment_template->initialization;
		if (set->segment_template->index) index_template = set->segment_template->index;
		if (set->segment_template->media) media_url = set->segment_template->media;
		if (set->segment_template->start_number != (u32) -1) start_number = set->segment_template->start_number;
		if (set->segment_template->segment_timeline) timeline = set->segment_template->segment_timeline;
		if (!timescale && set->segment_template->timescale) timescale = set->segment_template->timescale;
		if (!duration && set->segment_template->duration) duration = set->segment_template->duration;
	}
	if (rep->segment_template) {
		if (rep->segment_template->initialization) init_template = rep->segment_template->initialization;
		if (rep->segment_template->index) index_template = rep->segment_template->index;
		if (rep->segment_template->media) media_url = rep->segment_template->media;
		if (rep->segment_template->start_number != (u32) -1) start_number = rep->segment_template->start_number;
		if (rep->segment_template->segment_timeline) timeline = rep->segment_template->segment_timeline;
		if (!timescale && rep->segment_template->timescale) timescale = rep->segment_template->timescale;
		if (!duration&& rep->segment_template->duration) duration = rep->segment_template->duration;
	}

	/*return segment duration in all cases*/
	{
		u64 out_duration;
		u32 out_timescale;
		gf_mpd_resolve_segment_duration(rep, set, period, &out_duration, &out_timescale, NULL, NULL);
		*segment_duration_in_ms = (u64)((out_duration * 1000.0) / out_timescale);
	}

	/*offset the start_number with the number of discarded segments (no longer in our lists)*/
	start_number += nb_segments_removed;

	if (!media_url) {
		GF_MPD_BaseURL *base = gf_list_get(rep->base_URLs, 0);
		if (!base) return GF_BAD_PARAM;
		media_url = base->URL;
	}
	url_to_solve = NULL;
	switch (resolve_type) {
	case GF_MPD_RESOLVE_URL_INIT:
		url_to_solve = init_template;
		break;
	case GF_MPD_RESOLVE_URL_MEDIA:
	case GF_MPD_RESOLVE_URL_MEDIA_TEMPLATE:
		url_to_solve = media_url;
		break;
	case GF_MPD_RESOLVE_URL_INDEX:
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
	solved_template = gf_malloc(sizeof(char)*(strlen(url_to_solve) + (rep->id ? strlen(rep->id) : 0)) * 2);
	if (!solved_template) return GF_OUT_OF_MEM;
	
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
			if (!strchr(szPrintFormat, 'd') && !strchr(szPrintFormat, 'i')  && !strchr(szPrintFormat, 'u'))
				strcat(szPrintFormat, "d");
		} else {
			strcpy(szPrintFormat, "%d");
		}
		/* identifier is $$ -> replace by $*/
		if (!strlen(first_sep+1)) {
			strcat(solved_template, "$");
		}
		else if (!strcmp(first_sep+1, "RepresentationID")) {
			if (rep->id) {
				strcat(solved_template, rep->id);
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Missing ID on representation - cannot solve template\n\n"));
				gf_free(url);
				gf_free(solved_template);
				second_sep[0] = '$';
				return GF_NON_COMPLIANT_BITSTREAM;
			}
		}
		else if (!strcmp(first_sep+1, "Number")) {
			if (resolve_type==GF_MPD_RESOLVE_URL_MEDIA_TEMPLATE) {
				strcat(solved_template, "$Number$");
			} else {
				sprintf(szFormat, szPrintFormat, start_number + item_index);
				strcat(solved_template, szFormat);
			}

			/*check start time is in period (start time is ~seg_duration * item_index, since startNumber seg has start time = 0 in the period*/
			if (period->duration
				&& (item_index * (*segment_duration_in_ms) > period->duration)) {
				gf_free(url);
				gf_free(solved_template);
				second_sep[0] = '$';
				return GF_EOS;
			}
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
			if (resolve_type==GF_MPD_RESOLVE_URL_MEDIA_TEMPLATE) {
				strcat(solved_template, "$Time$");
			} else if (timeline) {
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
						if (k<nb_seg-1) {
							start_time += ent->duration * (1 + ent->repeat_count);
							continue;
						} else {
							gf_free(url);
							gf_free(solved_template);
							second_sep[0] = '$';
							return GF_EOS;
						}
					}
					*segment_duration_in_ms = ent->duration;
					*segment_duration_in_ms = (u32) ((Double) (*segment_duration_in_ms) * 1000.0 / timescale);
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
			} else if (duration) {
				u64 time = item_index * duration;
				szPrintFormat[strlen(szPrintFormat)-1] = 0;
				strcat(szPrintFormat, &LLD[1]);
				sprintf(szFormat, szPrintFormat, time);
				strcat(solved_template, szFormat);
			}
		}
		else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Unknown template identifier %s - disabling rep\n\n", first_sep+1));
			*out_url = NULL;
			gf_free(url);
			gf_free(solved_template);
			second_sep[0] = '$';
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

GF_EXPORT
Double gf_mpd_get_duration(GF_MPD *mpd)
{
	Double duration;
	duration = (Double)mpd->media_presentation_duration;
	if (!duration) {
		u32 i;
		for (i = 0; i<gf_list_count(mpd->periods); i++) {
			GF_MPD_Period *period = gf_list_get(mpd->periods, i);
			duration += (Double)period->duration;
		}
	}
	return duration / 1000.0;
}

GF_EXPORT
void gf_mpd_resolve_segment_duration(GF_MPD_Representation *rep, GF_MPD_AdaptationSet *set, GF_MPD_Period *period, u64 *out_duration, u32 *out_timescale, u64 *out_pts_offset, GF_MPD_SegmentTimeline **out_segment_timeline)
{
	u32 timescale = 0;
	u64 pts_offset = 0;
	GF_MPD_SegmentTimeline *segment_timeline;
	GF_MPD_MultipleSegmentBase *mbase_rep, *mbase_set, *mbase_period;

	if (out_segment_timeline) *out_segment_timeline = NULL;
	if (out_pts_offset) *out_pts_offset = 0;

	/*single media segment - duration is not known unless indicated in period*/
	if (rep->segment_base || set->segment_base || period->segment_base) {
		if (period && period->duration) {
			*out_duration = period->duration;
			timescale = 1000;
		} else {
			*out_duration = 0;
			timescale = 0;
		}
		if (rep->segment_base && rep->segment_base->presentation_time_offset) pts_offset = rep->segment_base->presentation_time_offset;
		if (rep->segment_base && rep->segment_base->timescale) timescale = rep->segment_base->timescale;
		if (!pts_offset && set->segment_base && set->segment_base->presentation_time_offset) pts_offset = set->segment_base->presentation_time_offset;
		if (!timescale && set->segment_base && set->segment_base->timescale) timescale = set->segment_base->timescale;
		if (period) {
			if (!pts_offset && period->segment_base && period->segment_base->presentation_time_offset) pts_offset = period->segment_base->presentation_time_offset;
			if (!timescale && period->segment_base && period->segment_base->timescale) timescale = period->segment_base->timescale;
		}
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

static u64 gf_mpd_segment_timeline_start(GF_MPD_SegmentTimeline *timeline, u32 segment_index, u64 *segment_duration)
{
	u64 start_time = 0;
	u32 i, idx, k;
	idx = 0;
	for (i = 0; i<gf_list_count(timeline->entries); i++) {
		GF_MPD_SegmentTimelineEntry *ent = gf_list_get(timeline->entries, i);
		if (ent->start_time) start_time = ent->start_time;
		for (k = 0; k<ent->repeat_count + 1; k++) {
			if (idx == segment_index) {
				if (segment_duration)
					*segment_duration = ent->duration;
				return start_time;
			}
			idx++;
			start_time += ent->duration;
		}
	}
	return start_time;
}

GF_EXPORT
GF_Err gf_mpd_get_segment_start_time_with_timescale(s32 in_segment_index,
	GF_MPD_Period const * const period, GF_MPD_AdaptationSet const * const set, GF_MPD_Representation const * const rep,
	u64 *out_segment_start_time, u64 *out_opt_segment_duration, u32 *out_opt_scale)
{
	u64 duration = 0, start_time = 0;
	u32 timescale = 0;
	GF_List *seglist = NULL;
	GF_MPD_SegmentTimeline *timeline = NULL;

	if (!out_segment_start_time || !period || !set || !rep) {
		return GF_BAD_PARAM;
	}

	/*single segment: return nothing*/
	if (rep->segment_base || set->segment_base || period->segment_base) {
		*out_segment_start_time = start_time;
		return GF_OK;
	}
	if (rep->segment_list || set->segment_list || period->segment_list) {
		if (period->segment_list) {
			if (period->segment_list->duration) duration = period->segment_list->duration;
			if (period->segment_list->timescale) timescale = period->segment_list->timescale;
			if (period->segment_list->segment_timeline) timeline = period->segment_list->segment_timeline;
			if (gf_list_count(period->segment_list->segment_URLs)) seglist = period->segment_list->segment_URLs;
		}
		if (set->segment_list) {
			if (set->segment_list->duration) duration = set->segment_list->duration;
			if (set->segment_list->timescale) timescale = set->segment_list->timescale;
			if (set->segment_list->segment_timeline) timeline = set->segment_list->segment_timeline;
			if (gf_list_count(set->segment_list->segment_URLs)) seglist = set->segment_list->segment_URLs;
		}
		if (rep->segment_list) {
			if (rep->segment_list->duration) duration = rep->segment_list->duration;
			if (rep->segment_list->timescale) timescale = rep->segment_list->timescale;
			if (gf_list_count(rep->segment_list->segment_URLs)) seglist = rep->segment_list->segment_URLs;
		}
		if (!timescale) timescale = 1;

		if (timeline) {
			start_time = gf_mpd_segment_timeline_start(timeline, in_segment_index, &duration);
		}
		else if (duration) {
			start_time = in_segment_index * duration;
		}
		else if (seglist && (in_segment_index >= 0)) {
			u32 i;
			start_time = 0;
			for (i = 0; i <= (u32)in_segment_index; i++) {
				GF_MPD_SegmentURL *url = gf_list_get(seglist, i);
				if (!url) break;
				duration = url->duration;
				if (i < (u32)in_segment_index)
					start_time += url->duration;
			}
		}
		if (out_opt_segment_duration) *out_opt_segment_duration = duration;
		if (out_opt_scale) *out_opt_scale = timescale;

		*out_segment_start_time = start_time;
		return GF_OK;
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
	if (!timescale) timescale = 1;

	if (timeline) {
		start_time = gf_mpd_segment_timeline_start(timeline, in_segment_index, &duration);
	}
	else {
		start_time = in_segment_index * duration;
	}

	if (out_opt_segment_duration) *out_opt_segment_duration = duration;
	if (out_opt_scale) *out_opt_scale = timescale;
	*out_segment_start_time = start_time;

	return GF_OK;
}

static GF_Err mpd_seek_periods(Double seek_time, GF_MPD const * const in_mpd, GF_MPD_Period **out_period)
{
	Double start_time;
	u32 i;

	start_time = 0;
	for (i=0; i<gf_list_count(in_mpd->periods); i++) {
		GF_MPD_Period *period = gf_list_get(in_mpd->periods, i);
		Double dur;

		if (period->xlink_href) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD] Period contains XLINKs. Not supported.\n", period->xlink_href));
			return GF_NOT_SUPPORTED;
		}

		dur = (Double)period->duration;
		dur /= 1000;
		if (seek_time >= start_time) {
			if ((seek_time < start_time + dur)
				|| (i+1 == gf_list_count(in_mpd->periods) && dur == 0.0)) {
				*out_period = period;
				break;
			} else {
				return GF_EOS;
			}
		}
		start_time += dur;
	}

	return GF_OK;
}

GF_EXPORT
GF_Err gf_mpd_seek_in_period(Double seek_time, MPDSeekMode seek_mode,
	GF_MPD_Period const * const in_period, GF_MPD_AdaptationSet const * const in_set, GF_MPD_Representation const * const in_rep,
	u32 *out_segment_index, Double *out_opt_seek_time)
{
	Double seg_start = 0.0, segment_duration = 0.0;
	u64 segment_duration_in_scale = 0, seg_start_in_scale = 0;
	u32 timescale = 0, segment_idx = 0;

	if (!out_segment_index) {
		return GF_BAD_PARAM;
	}

	while (1) {
		//TODO this could be further optimized by directly querying the index for this start time ...
		GF_Err e = gf_mpd_get_segment_start_time_with_timescale(segment_idx, in_period, in_set, in_rep, &seg_start_in_scale, &segment_duration_in_scale, &timescale);
		if (e<0)
			return e;
		segment_duration = segment_duration_in_scale / (Double)timescale;
		
		if (seek_mode == MPD_SEEK_PREV) {
			if ((seek_time >= seg_start) && (seek_time < seg_start + segment_duration)) {
				if (out_opt_seek_time) *out_opt_seek_time = seg_start;
				break;
			}
		} else if (seek_mode == MPD_SEEK_NEAREST) {
			if ((seek_time >= seg_start) && (seek_time < seg_start + segment_duration)) {
				Double dist_to_prev = seek_time - seg_start;
				Double dist_to_next = seg_start + segment_duration - seek_time;
				if (dist_to_next < dist_to_prev) {
					if (out_opt_seek_time) *out_opt_seek_time = seg_start + segment_duration;
					segment_idx++;
				} else {
					if (out_opt_seek_time) *out_opt_seek_time = seg_start;
				}
				break;
			}
		} else {
			assert(0);
			return GF_NOT_SUPPORTED;
		}

		seg_start += segment_duration;
		segment_idx++;
	}

	*out_segment_index = segment_idx;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_mpd_seek_to_time(Double seek_time, MPDSeekMode seek_mode,
	GF_MPD const * const in_mpd, GF_MPD_AdaptationSet const * const in_set, GF_MPD_Representation const * const in_rep,
	GF_MPD_Period **out_period, u32 *out_segment_index, Double *out_opt_seek_time)
{
	GF_Err e = GF_OK;

	if (!out_period || !out_segment_index) {
		return GF_BAD_PARAM;
	}

	e = mpd_seek_periods(seek_time, in_mpd, out_period);
	if (e)
		return e;

	e = gf_mpd_seek_in_period(seek_time, seek_mode, *out_period, in_set, in_rep, out_segment_index, out_opt_seek_time);
	if (e)
		return e;

	return GF_OK;
}


/*
	smooth streaming 2.1 support
 
	this is still very basic - we miss support for:
	* live streams
	* sparse stream
	* Custom Attributes in Url pattern
 
	The smooth maifest is transformed into an MPD with
 StreamIndex <=> AdaptationSet
 Url Template <=> SegmentTemplate.media at adaptation set level
 QualityLevel <=> Representation
 Codecs info at quality level <=> SegmentTemplate.initialisation at representation level using internal "isobmff:// ..." scheme
 chunks <=> Segment Timeline at adaptation set level
 */

static GF_Err smooth_parse_chunk(GF_MPD *mpd, GF_List *container, GF_XMLNode *root)
{
    u32 i;
    GF_MPD_SegmentTimelineEntry *chunk;
    GF_XMLAttribute *att;
    
    GF_SAFEALLOC(chunk, GF_MPD_SegmentTimelineEntry);
    if (!chunk) return GF_OUT_OF_MEM;
    gf_list_add(container, chunk);
    i = 0;
    while ( (att = gf_list_enum(root->attributes, &i)) ) {
        if (!strcmp(att->name, "n")) {}
        else if (!strcmp(att->name, "d")) chunk->duration = atoi(att->value);
        else if (!strcmp(att->name, "t")) chunk->start_time = atoi(att->value);
    }
    return GF_OK;
}

static GF_Err smooth_replace_string(char *src_str, char *str_match, char *str_replace, char **output)
{
    u32 len;
    char c, *res, *sep = strstr(src_str, str_match);
    if (!sep) {
        res = gf_strdup(src_str);
        if (*output) gf_free(*output);
        *output = res;
        return GF_OK;
    }
    
    c = sep[0];
    sep[0] = 0;
    len = (u32) ( strlen(src_str) + strlen(str_replace) + strlen(sep+strlen(str_match)) + 1 );
    res = gf_malloc(sizeof(char) * len);
    strcpy(res, src_str);
    strcat(res, str_replace);
    strcat(res, sep+strlen(str_match));
    sep[0] = c;
    
    if (*output) gf_free(*output);
    *output = res;
    return GF_OK;
}

static GF_Err smooth_parse_quality_level(GF_MPD *mpd, GF_List *container, GF_XMLNode *root, u32 timescale)
{
    u32 i;
    Bool is_audio = GF_FALSE;
    GF_MPD_Representation *rep;
    GF_XMLAttribute *att;
    GF_Err e;
    char szISOBMFFInit[2048];
    
    GF_SAFEALLOC(rep, GF_MPD_Representation);
    if (!rep) return GF_OUT_OF_MEM;
    gf_mpd_init_common_attributes((GF_MPD_CommonAttributes *)rep);
    rep->base_URLs = gf_list_new();
    rep->sub_representations = gf_list_new();
    e = gf_list_add(container, rep);
    if (e) return e;
    
    strcpy(szISOBMFFInit, "isobmff://");
    
    
#define ISBMFFI_ADD_KEYWORD(_name, _value) \
if (_value) {\
strcat(szISOBMFFInit, _name);\
strcat(szISOBMFFInit, "=");\
strcat(szISOBMFFInit, _value);\
strcat(szISOBMFFInit, " ");\
}
    
    
    i = 0;
    while ( (att = gf_list_enum(root->attributes, &i)) ) {
        if (!strcmp(att->name, "Index")) rep->id = gf_strdup(att->value);
        else if (!strcmp(att->name, "Bitrate")) rep->bandwidth = atoi(att->value);
        else if (!strcmp(att->name, "MaxWidth")) {
            rep->width = atoi(att->value);
            ISBMFFI_ADD_KEYWORD("w", att->value)
        }
        else if (!strcmp(att->name, "MaxHeight")) {
            rep->height = atoi(att->value);
            ISBMFFI_ADD_KEYWORD("h", att->value)
        }
        else if (!strcmp(att->name, "FourCC")) {
            ISBMFFI_ADD_KEYWORD("4cc", att->value)
        }
        else if (!strcmp(att->name, "CodecPrivateData")) {
            ISBMFFI_ADD_KEYWORD("init", att->value)
        }
        else if (!strcmp(att->name, "NALUnitLengthField")) {
            ISBMFFI_ADD_KEYWORD("nal", att->value)
        }
        else if (!strcmp(att->name, "BitsPerSample")) {
            ISBMFFI_ADD_KEYWORD("bps", att->value)
            is_audio = GF_TRUE;
        }
        else if (!strcmp(att->name, "AudioTag")) {
            ISBMFFI_ADD_KEYWORD("atag", att->value)
            is_audio = GF_TRUE;
        }
        else if (!strcmp(att->name, "Channels")) {
            ISBMFFI_ADD_KEYWORD("ch", att->value)
            is_audio = GF_TRUE;
        }
        else if (!strcmp(att->name, "SamplingRate")) {
            ISBMFFI_ADD_KEYWORD("srate", att->value)
            is_audio = GF_TRUE;
        }
    }
    if (timescale != 10000000) {
        char szTS[20], *v;
        sprintf(szTS, "%d", timescale);
	//prevent gcc warning
	v = (char *)szTS;
        ISBMFFI_ADD_KEYWORD("scale", v)
    }
    ISBMFFI_ADD_KEYWORD("tfdt", "0000000000000000000")
    //create a url for the IS to be reconstructed
    rep->mime_type = gf_strdup(is_audio ? "audio/mp4" : "video/mp4");
    GF_SAFEALLOC(rep->segment_template, GF_MPD_SegmentTemplate);
    rep->segment_template->initialization = gf_strdup(szISOBMFFInit);
    return GF_OK;
}

static GF_Err smooth_parse_stream_index(GF_MPD *mpd, GF_List *container, GF_XMLNode *root, u32 timescale)
{
    u32 i;
    GF_MPD_AdaptationSet *set;
    GF_XMLAttribute *att;
    GF_XMLNode *child;
    
    GF_SAFEALLOC(set, GF_MPD_AdaptationSet);
    if (!set) return GF_OUT_OF_MEM;
    gf_mpd_init_common_attributes((GF_MPD_CommonAttributes *)set);
    
    gf_list_add(container, set);
    
    set->accessibility = gf_list_new();
    set->role = gf_list_new();
    set->rating = gf_list_new();
    set->viewpoint = gf_list_new();
    set->content_component = gf_list_new();
    set->base_URLs = gf_list_new();
    set->representations = gf_list_new();
    set->segment_alignment = GF_TRUE;
    /*assign default ID and group*/
    set->group = -1;
    
    i=0;
    while ((att = gf_list_enum(root->attributes, &i))) {
        if (!strcmp(att->name, "Type")) {}
        else if (!strcmp(att->name, "Name")) {}
        else if (!strcmp(att->name, "Chunks")) {}
        else if (!strcmp(att->name, "MaxWidth")) set->max_width = atoi(att->value);
        else if (!strcmp(att->name, "MaxHeight")) set->max_height = atoi(att->value);
        else if (!strcmp(att->name, "Url")) {
            char *template_url=NULL;
            smooth_replace_string(att->value, "{bitrate}", "$Bandwidth$", &template_url);
            smooth_replace_string(template_url, "{Bitrate}", "$Bandwidth$", &template_url);
            smooth_replace_string(template_url, "{start time}", "$Time$", &template_url);
            smooth_replace_string(template_url, "{start_time}", "$Time$", &template_url);
            //TODO handle track substitution and custom attrib
            
            GF_SAFEALLOC(set->segment_template, GF_MPD_SegmentTemplate);
            if (!set->segment_template) return GF_OUT_OF_MEM;
            set->segment_template->media = template_url;
            set->segment_template->timescale = timescale;
            GF_SAFEALLOC(set->segment_template->segment_timeline, GF_MPD_SegmentTimeline);
            set->segment_template->segment_timeline->entries = gf_list_new();
        }
    }
    
    i = 0;
    while ( ( child = gf_list_enum(root->content, &i )) ) {
        if (!strcmp(child->name, "QualityLevel")) {
            smooth_parse_quality_level(mpd, set->representations, child, timescale);
        }
        if (!strcmp(child->name, "c")) {
            smooth_parse_chunk(mpd, set->segment_template->segment_timeline->entries, child);
        }
    }
    
    return GF_OK;
}

GF_EXPORT
GF_Err gf_mpd_init_smooth_from_dom(GF_XMLNode *root, GF_MPD *mpd, const char *default_base_url)
{
    GF_Err e;
    u32 i, timescale;
    GF_XMLAttribute *att;
    GF_XMLNode *child;
    GF_MPD_Period *period;
    
    if (!root || !mpd) return GF_BAD_PARAM;
    
    assert(!mpd->periods);
    mpd->periods = gf_list_new();
    mpd->program_infos = gf_list_new();
    mpd->base_URLs = gf_list_new();
    mpd->locations = gf_list_new();
    mpd->metrics = gf_list_new();
    
    /*setup some defaults*/
    mpd->type = GF_MPD_TYPE_STATIC;
    mpd->time_shift_buffer_depth = (u32) -1; /*infinite by default*/
    mpd->xml_namespace = NULL;
    
    
    timescale = 10000000;
    i=0;
    while ((att = gf_list_enum(root->attributes, &i))) {
        if (!strcmp(att->name, "TimeScale"))
        timescale = atoi(att->value);
        else if (!strcmp(att->name, "Duration"))
        mpd->media_presentation_duration = atoi(att->value);
        else if (!strcmp(att->name, "IsLive") && stricmp(att->value, "true") )
        mpd->type = GF_MPD_TYPE_DYNAMIC;
        else if (!strcmp(att->name, "LookaheadCount"))
        {}
        else if (!strcmp(att->name, "DVRWindowLength"))
        mpd->time_shift_buffer_depth = atoi(att->value);
    }
    mpd->media_presentation_duration = mpd->media_presentation_duration * 1000 / timescale;
    mpd->time_shift_buffer_depth = mpd->time_shift_buffer_depth * 1000 / timescale;
    
    
    GF_SAFEALLOC(period, GF_MPD_Period);
    if (!period) return GF_OUT_OF_MEM;
    gf_list_add(mpd->periods, period);
    period->adaptation_sets = gf_list_new();
    if (!period->adaptation_sets) return GF_OUT_OF_MEM;
    
    i = 0;
    while ( ( child = gf_list_enum(root->content, &i )) ) {
        if (!strcmp(child->name, "StreamIndex")) {
            e = smooth_parse_stream_index(mpd, period->adaptation_sets, child, timescale);
            if (e) return e;
        }
    }
    
    return GF_OK;
}


#endif /*GPAC_DISABLE_CORE_TOOLS*/
