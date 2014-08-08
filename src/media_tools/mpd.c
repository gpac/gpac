/*
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

#ifdef _WIN32_WCE
#include <winbase.h>
#else
/*for mktime*/
#include <time.h>
#endif

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
	if (mpd->xml_namespace && child->ns && !strcmp(mpd->xml_namespace, child->ns) ) return 1;
	return 0;
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

static u32 gf_mpd_parse_duration(char *duration) {
	u32 i;
	if (!duration) return 0;
	i = 0;
	while (1) {
		if (duration[i] == ' ') i++;
		else if (duration[i] == 0) return 0;
		else {
			break;
		}
	}
	if (duration[i] == 'P') {
		if (duration[i+1] == 0) return 0;
		else if (duration[i+1] != 'T') return 0;
		else {
			char *sep1, *sep2;
			u32 h, m;
			double s;
			h = m = 0;
			s = 0;
			if (NULL != (sep1 = strchr(duration+i+2, 'H'))) {
				*sep1 = 0;
				h = atoi(duration+i+2);
				*sep1 = 'H';
				sep1++;
			} else {
				sep1 = duration+i+2;
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
			return (u32)((h*3600+m*60+s)*1000);
		}
	} else {
		return 0;
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

	/*infinite by default*/
	seg->time_shift_buffer_depth = (u32) -1;

	while ( (att = gf_list_enum(root->attributes, &i)) ) {
		if (!strcmp(att->name, "timescale")) seg->timescale = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "presentationTimeOffset")) seg->presentation_time_offset = gf_mpd_parse_long_int(att->value);
		else if (!strcmp(att->name, "indexRange")) seg->index_range = gf_mpd_parse_byte_range(att->value);
		else if (!strcmp(att->name, "indexRangeExact")) seg->index_range_exact = gf_mpd_parse_bool(att->value);
		else if (!strcmp(att->name, "availabilityTimeOffset")) seg->availability_time_offset = gf_mpd_parse_double(att->value);
		else if (!strcmp(att->name, "timeShiftBufferDepth")) seg->time_shift_buffer_depth = gf_mpd_parse_duration(att->value);
	}

	if (mpd->type == GF_MPD_TYPE_STATIC)
		seg->time_shift_buffer_depth = 0;

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
			gf_list_add(seg->entries, seg_tl_ent);

			j = 0;
			while ( (att = gf_list_enum(child->attributes, &j)) ) {
				if (!strcmp(att->name, "t"))
					seg_tl_ent->start_time = gf_mpd_parse_long_int(att->value);
				else if (!strcmp(att->name, "d"))
					seg_tl_ent->duration = gf_mpd_parse_int(att->value);
				else if (!strcmp(att->name, "r"))
					seg_tl_ent->repeat_count = (u32) gf_mpd_parse_int(att->value);
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

static void gf_mpd_parse_segment_url(GF_List *container, GF_XMLNode *root)
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

static GF_Err gf_mpd_parse_content_component(GF_List *container, GF_XMLNode *root)
{
	return GF_OK;
}

static GF_Err gf_mpd_parse_descriptor(GF_List *container, GF_XMLNode *root)
{
	GF_XMLAttribute *att;
	GF_MPD_Descriptor *mpd_desc;
	u32 i = 0;

	GF_SAFEALLOC(mpd_desc, GF_MPD_Descriptor);
	while ( (att = gf_list_enum(root->attributes, &i)) ) {
		if (!strcmp(att->name, "schemeIdUri")) mpd_desc->scheme_id_uri = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "value")) mpd_desc->value = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "id")) mpd_desc->id = gf_mpd_parse_string(att->value);
	}
	gf_list_add(container, mpd_desc);
	return GF_OK;
}

static void gf_mpd_parse_common_representation(GF_MPD *mpd, GF_MPD_CommonAttributes *com, GF_XMLNode *root)
{
	GF_XMLAttribute *att;
	GF_XMLNode *child;
	u32 i = 0;

	/*setup some default*/
	com->max_playout_rate = 1.0;

	while ( (att = gf_list_enum(root->attributes, &i)) ) {
		if (!strcmp(att->name, "profiles")) com->profiles = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "width")) com->width = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "height")) com->height = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "sar")) com->sar = gf_mpd_parse_frac(att->value);
		else if (!strcmp(att->name, "frameRate")) com->framerate = gf_mpd_parse_frac(att->value);
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
}
static GF_Err gf_mpd_parse_representation(GF_MPD *mpd, GF_List *container, GF_XMLNode *root)
{
	u32 i;
	GF_MPD_Representation *rep;
	GF_XMLAttribute *att;
	GF_XMLNode *child;
	GF_Err e;

	GF_SAFEALLOC(rep, GF_MPD_Representation);
	if (!rep) return GF_OUT_OF_MEM;
	gf_mpd_init_common_attributes((GF_MPD_CommonAttributes *)rep);
	rep->base_URLs = gf_list_new();
	rep->sub_representations = gf_list_new();
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
	}
	return GF_OK;
}

static GF_Err gf_mpd_parse_adaptation_set(GF_MPD *mpd, GF_List *container, GF_XMLNode *root)
{
	u32 i;
	GF_MPD_AdaptationSet *set;
	GF_XMLAttribute *att;
	GF_XMLNode *child;
	GF_Err e;

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

	e = gf_list_add(container, set);
	if (e) return e;

	i = 0;
	while ( (att = gf_list_enum(root->attributes, &i)) ) {
		if (strstr(att->name, "href")) set->xlink_href = gf_mpd_parse_string(att->value);
		else if (strstr(att->name, "actuate")) set->xlink_actuate_on_load = !strcmp(att->value, "onLoad") ? 1 : 0;
		else if (!strcmp(att->name, "id")) set->id = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "group")) set->group = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "lang")) set->lang = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "contentType")) set->content_type = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "par")) set->par = gf_mpd_parse_frac(att->value);
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
	}
	return GF_OK;
}

static GF_Err gf_mpd_parse_period(GF_MPD *mpd, GF_XMLNode *root)
{
	u32 i;
	GF_MPD_Period *period;
	GF_XMLAttribute *att;
	GF_XMLNode *child;
	GF_Err e;

	GF_SAFEALLOC(period, GF_MPD_Period);
	if (!period) return GF_OUT_OF_MEM;
	period->adaptation_sets = gf_list_new();
	period->base_URLs = gf_list_new();
	period->subsets = gf_list_new();
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
	gf_free(ptr);
}
void gf_mpd_string_free(void *_item)
{
	gf_free( (char *) _item );
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
void gf_mpd_descriptor_free(void *item)
{
	GF_MPD_Descriptor *mpd_desc = (GF_MPD_Descriptor*) item;
	if (mpd_desc->id) gf_free(mpd_desc->id);
	if (mpd_desc->scheme_id_uri) gf_free(mpd_desc->scheme_id_uri);
	if (mpd_desc->value) gf_free(mpd_desc->value);
	gf_free(mpd_desc);
}

void gf_mpd_content_component_free(void *item)
{
	GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD] content component not implemented\n"));
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

	if (ptr->playback.cached_init_segment_url) gf_free(ptr->playback.cached_init_segment_url);

	gf_mpd_del_list(ptr->base_URLs, gf_mpd_base_url_free, 0);
	gf_mpd_del_list(ptr->sub_representations, NULL/*TODO*/, 0);
	if (ptr->segment_base) gf_mpd_segment_base_free(ptr->segment_base);
	if (ptr->segment_list) gf_mpd_segment_list_free(ptr->segment_list);
	if (ptr->segment_template) gf_mpd_segment_template_free(ptr->segment_template);
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
	gf_mpd_del_list(ptr->subsets, NULL/*TODO*/, 0);
	gf_free(ptr);
}

GF_EXPORT
void gf_mpd_del(GF_MPD *mpd)
{
	gf_mpd_del_list(mpd->program_infos, gf_mpd_prog_info_free, 0);
	gf_mpd_del_list(mpd->base_URLs, gf_mpd_base_url_free, 0);
	gf_mpd_del_list(mpd->locations, gf_mpd_string_free, 0);
	gf_mpd_del_list(mpd->metrics, NULL/*TODO*/, 0);
	gf_mpd_del_list(mpd->periods, gf_mpd_period_free, 0);
	if (mpd->profiles) gf_free(mpd->profiles);
	if (mpd->ID) gf_free(mpd->ID);
	gf_free(mpd);
}


GF_EXPORT
GF_Err gf_mpd_complete_from_dom(GF_XMLNode *root, GF_MPD *mpd, const char *default_base_url)
{
	GF_Err e;
	Bool ns_ok = 0;
	u32 att_index, child_index;
	GF_XMLAttribute *att;
	GF_XMLNode *child;

	if (!root || !mpd) return GF_BAD_PARAM;

	att_index = 0;
	child_index = gf_list_count(root->attributes);
	for (att_index = 0 ; att_index < child_index; att_index++) {
		att = gf_list_get(root->attributes, att_index);
		if (!att) continue;
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

	att_index = 0;
	for (att_index = 0 ; att_index < child_index; att_index++) {
		att = gf_list_get(root->attributes, att_index);
		if (!att) {
			continue;
		}

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
			mpd->minimum_update_period = gf_mpd_parse_duration(att->value);
		} else if (!strcmp(att->name, "minBufferTime")) {
			mpd->min_buffer_time = gf_mpd_parse_duration(att->value);
		} else if (!strcmp(att->name, "timeShiftBufferDepth")) {
			mpd->time_shift_buffer_depth = gf_mpd_parse_duration(att->value);
		} else if (!strcmp(att->name, "suggestedPresentationDelay")) {
			mpd->suggested_presentaton_delay = gf_mpd_parse_duration(att->value);
		} else if (!strcmp(att->name, "maxSegmentDuration")) {
			mpd->max_segment_duration = gf_mpd_parse_duration(att->value);
		} else if (!strcmp(att->name, "maxSubsegmentDuration")) {
			mpd->max_subsegment_duration = gf_mpd_parse_duration(att->value);
		}
	}
	if (mpd->type == GF_MPD_TYPE_STATIC)
		mpd->minimum_update_period = mpd->time_shift_buffer_depth = 0;

	child_index = 0;
	while (1) {
		child = gf_list_get(root->content, child_index);
		if (!child) {
			break;
		} else if (gf_mpd_valid_child(mpd, child) ) {
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
			}
		}
		child_index++;
	}
	return GF_OK;
}


GF_EXPORT
GF_Err gf_mpd_init_from_dom(GF_XMLNode *root, GF_MPD *mpd, const char *default_base_url)
{
	if (!root || !mpd) return GF_BAD_PARAM;

	assert( !mpd->periods );
	mpd->periods = gf_list_new();
	mpd->program_infos = gf_list_new();
	mpd->base_URLs = gf_list_new();
	mpd->locations = gf_list_new();
	mpd->metrics = gf_list_new();
	/*setup some defaults*/
	mpd->type = GF_MPD_TYPE_STATIC;
	/*infinite by default*/
	mpd->time_shift_buffer_depth = (u32) -1;

	mpd->xml_namespace = NULL;

	return gf_mpd_complete_from_dom(root, mpd, default_base_url);
}

GF_EXPORT
GF_Err gf_m3u8_to_mpd(const char *m3u8_file, const char *base_url,
                      const char *mpd_file,
                      u32 reload_count, char *mimeTypeForM3U8Segments, Bool do_import, Bool use_mpd_templates, GF_FileDownload *getter)
{
	GF_Err e;
	char *sep, *template_base, *template_ext;
	u32 i, count, j, count2, k, count3, template_width, template_idx_start;
	Double update_interval;
	VariantPlaylist * pl = NULL;
	Bool use_template;
	Program *prog;
	PlaylistElement *pe, *the_pe, *elt;
	FILE *fmpd;
	Bool is_end;
	u32 max_dur = 0;

	e = parse_root_playlist(m3u8_file, &pl, base_url);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[M3U8] Failed to parse root playlist '%s', error = %s\n", m3u8_file, gf_error_to_string(e)));
		if (pl) variant_playlist_del(pl);
		pl = NULL;
		return e;
	}
	if (mpd_file == NULL) {
		gf_delete_file(m3u8_file);
		mpd_file = m3u8_file;
	}
	the_pe = NULL;
	pe = NULL;
	i=0;
	assert( pl );
	assert( pl->programs );
	while ((prog = gf_list_enum(pl->programs, &i))) {
		u32 j=0;

		while (NULL != (pe = gf_list_enum(prog->bitrates, &j))) {
			Bool found = 0;
			u32 k;
			char *suburl;

			if (!pe->url )
				continue;

			/*filter out duplicated entries (seen on M6 m3u8)*/
			for (k=0; k<j-1; k++) {
				PlaylistElement *a_pe = gf_list_get(prog->bitrates, k);
				if (a_pe->url && pe->url && !strcmp(a_pe->url, pe->url)) {
					found = 1;
					break;
				}
			}
			if (found) continue;

			the_pe = pe;
			suburl = NULL;

			/*not HLS*/
			if ( !strstr(pe->url, ".m3u8"))
				continue;

			if (strcmp(base_url, pe->url))
				suburl = gf_url_concatenate(base_url, pe->url);

			if (!suburl || !strcmp(base_url, suburl)) {
				if (suburl) gf_free(suburl);
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MPD Generator] Not downloading, programs are identical for %s...\n", pe->url));
				continue;
			}
			if (getter && getter->new_session && getter->del_session && getter->get_cache_name) {
				e = getter->new_session(getter, suburl);
				if (e) {
					gf_free(suburl);
					break;
				}
				if (e==GF_OK) {
					e = parse_sub_playlist(getter->get_cache_name(getter), &pl, suburl, prog, pe);
				}
				getter->del_session(getter);

#if 0
				GF_DownloadSession *sess = gf_service_download_new(service, suburl, GF_NETIO_SESSION_NOT_THREADED, NULL, NULL);
				if (!sess) {
					gf_free(suburl);
					break;
				}
				e = gf_dm_sess_process(sess);
				if (e==GF_OK) {
					e = parse_sub_playlist(gf_dm_sess_get_cache_name(sess), &pl, suburl, prog, pe);
				}
				gf_service_download_del(sess);
#endif
				gf_free(suburl);
			} else { /* for use in MP4Box */
				if (strstr(suburl, "://") ) {
					e = gf_dm_wget(suburl, "tmp.m3u8", 0, 0);
					if (e==GF_OK) {
						e = parse_sub_playlist("tmp.m3u8", &pl, suburl, prog, pe);
					}
					gf_delete_file("tmp.m3u8");
				} else {
					e = parse_sub_playlist(suburl, &pl, suburl, prog, pe);
				}
			}
		}
		if (max_dur < (u32) prog->computed_duration)
			max_dur = (u32) prog->computed_duration;
	}

	is_end = !pl->playlistNeedsRefresh;
	assert(the_pe);

	update_interval = 0;
	/*update interval is set to the duration of the last media file with rules defined in http live streaming RFC section 6.3.4*/
	switch (reload_count) {
	case 0:
		update_interval = the_pe->durationInfo;
		break;
	case 1:
		update_interval = (Double)the_pe->durationInfo/2;
		break;
	case 2:
		update_interval = 3*((Double)the_pe->durationInfo/2);
		break;
	default:
		update_interval = 3*(the_pe->durationInfo);
		break;
	}
	if (is_end || ((the_pe->elementType == TYPE_PLAYLIST) && the_pe->element.playlist.is_ended)) {
		update_interval = 0;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MPD Generator] NO NEED to refresh playlist !\n"));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MPD Generator] Playlist will be refreshed every %g seconds, len=%d\n", update_interval, the_pe->durationInfo));
	}

	assert( mpd_file );
	fmpd = gf_f64_open(mpd_file, "wt");
	if (!fmpd) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[MPD Generator] Cannot write to temp file %s!\n", mpd_file));
		variant_playlist_del(pl);
		return GF_IO_ERR;
	}
	fprintf(fmpd, "<MPD type=\"%s\" xmlns=\"urn:mpeg:DASH:schema:MPD:2011\" profiles=\"urn:mpeg:dash:profile:full:2011\"", is_end ? "static" : "dynamic" );
	sep = strrchr(m3u8_file, '/');
	if (!sep) sep = strrchr(m3u8_file, '\\');
	if (sep) sep = sep + 1;
	else sep = (char *)m3u8_file;
	fprintf(fmpd, " id=\"%s\"", sep);

	if (update_interval) fprintf(fmpd, " minimumUpdatePeriod=\"PT%02.2gS\"", update_interval);
	if (is_end) fprintf(fmpd, " mediaPresentationDuration=\"PT%dS\"", max_dur);
	fprintf(fmpd, " minBufferTime=\"PT1.5S\"");
	fprintf(fmpd, ">\n");

	fprintf(fmpd, " <ProgramInformation moreInformationURL=\"http://gpac.sourceforge.net\">\n");
	{
		char * title = the_pe->title;
		if (!title || strlen(title) < 2)
			title = the_pe->url;
		fprintf(fmpd, "  <Title>%s</Title>\n", title );
	}
	fprintf(fmpd, "  <Source>Generated from URL %s</Source>\n", base_url );
	fprintf(fmpd, "  <Copyright>Generated by GPAC %s from %s</Copyright>\n", GPAC_FULL_VERSION, base_url);

	fprintf(fmpd, " </ProgramInformation>\n");
	fprintf(fmpd, " <Period start=\"PT0S\"");
	if (is_end) fprintf(fmpd, " duration=\"PT%dS\"", max_dur);
	fprintf(fmpd, " >\n");


	count = gf_list_count(pl->programs);
	/*check if we use templates*/
	template_base = NULL;
	template_ext = NULL;
	use_template = use_mpd_templates;
	template_width = 0;
	template_idx_start = 0;
	for (i=0; i<count; i++) {
		prog = gf_list_get(pl->programs, i);
		count2 = gf_list_count(prog->bitrates);
		for (j = 0; j<count2; j++) {
			pe = gf_list_get(prog->bitrates, j);
			if (pe->elementType != TYPE_PLAYLIST)
				continue;

			count3 = gf_list_count(pe->element.playlist.elements);
			if (!count3) continue;

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
						use_template = 0;
						break;
					}
				}
			}
			if (!template_ext) template_ext="";

			if (use_template) {
				for (k=0; k<count3; k++) {
					char szURL[GF_MAX_PATH], *sub_url;
					elt = gf_list_get(pe->element.playlist.elements, k);

					if (template_width==2) sprintf(szURL, "%s%02d%s", template_base, template_idx_start + k, template_ext);
					else if (template_width==3) sprintf(szURL, "%s%03d%s", template_base, template_idx_start + k, template_ext);
					else if (template_width==4) sprintf(szURL, "%s%04d%s", template_base, template_idx_start + k, template_ext);
					else if (template_width==5) sprintf(szURL, "%s%05d%s", template_base, template_idx_start + k, template_ext);
					else if (template_width==6) sprintf(szURL, "%s%06d%s", template_base, template_idx_start + k, template_ext);
					else sprintf(szURL, "%s%d%s", template_base, template_idx_start + k, template_ext);

					sub_url = strrchr(elt->url, '/');
					if (!sub_url) sub_url = elt->url;
					else sub_url ++;
					if (strcmp(szURL, sub_url)) {
						use_template = 0;
						break;
					}
				}
			}

		}
	}

	fprintf(fmpd, "  <AdaptationSet segmentAlignment=\"true\">\n");

	/*if we use templates, put the SegmentTemplate element at the adaptationSet level*/
	if (use_template) {
		fprintf(fmpd, "   <SegmentTemplate");

		fprintf(fmpd, " duration=\"%d\"", pe->durationInfo);
		if (template_width>1) {
			fprintf(fmpd, " media=\"%s$%%0%ddNumber$%s\"", template_base, template_width, template_ext);
		} else {
			fprintf(fmpd, " media=\"%s$Number$%s\"", template_base, template_ext);
		}
		fprintf(fmpd, " startNumber=\"%d\"", template_idx_start);
		fprintf(fmpd, "/>\n");
	}

	if (do_import) {
#ifndef GPAC_DISABLE_MEDIA_IMPORT
		GF_Err e;
		GF_MediaImporter import;
		char *tmp_file = NULL;
		elt = gf_list_get(pe->element.playlist.elements, 0);
		memset(&import, 0, sizeof(GF_MediaImporter));
		import.trackID = 0;
		import.flags = GF_IMPORT_PROBE_ONLY;

		if (strstr(elt->url, "://") && !strstr(elt->url, "file://")) {
			tmp_file = strrchr(elt->url, '/');
			if (!tmp_file) tmp_file = strrchr(elt->url, '\\');
			if (tmp_file) {
				e = gf_dm_wget(elt->url, tmp_file, 0, 0);
				if (e==GF_OK) {
					import.in_name = tmp_file;
					e = gf_media_import(&import);
				}
			}
		} else {
			import.in_name = elt->url;
			e = gf_media_import(&import);
		}

		if (import.nb_tracks>1) {
			for (k=0; k<import.nb_tracks; k++) {
				fprintf(fmpd, "   <ContentComponent id=\"%d\"", import.tk_info[k].track_num);
				if (import.tk_info[k].lang)
					fprintf(fmpd, " lang=\"%s\"", gf_4cc_to_str(import.tk_info[k].lang));
				switch (import.tk_info[k].type) {
				case GF_ISOM_MEDIA_VISUAL:
					fprintf(fmpd, " contentType=\"video\"");
					break;
				case GF_ISOM_MEDIA_AUDIO:
					fprintf(fmpd, " contentType=\"audio\"");
					break;
				default:
					fprintf(fmpd, " contentType=\"application\"");
					break;
				}
				fprintf(fmpd, "/>\n");
			}
		}
		if (tmp_file)
			gf_delete_file(tmp_file);
#endif
	}

	/*check if we use templates*/
	count = gf_list_count(pl->programs);
	for (i=0; i<count; i++) {
		u32 width, height, samplerate, num_channels;
		prog = gf_list_get(pl->programs, i);
		count2 = gf_list_count(prog->bitrates);
		for (j = 0; j<count2; j++) {
			char *base_url;
#ifndef GPAC_DISABLE_MEDIA_IMPORT
			Bool import_file = do_import;
#endif
			Bool is_aac = 0;
			char *byte_range_media_file = NULL;
			pe = gf_list_get(prog->bitrates, j);

			if (pe->elementType == TYPE_STREAM) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD] NOT SUPPORTED: M3U8 Stream\n"));
			} else if (pe->elementType != TYPE_PLAYLIST) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD] NOT SUPPORTED: M3U8 unknown type\n"));
			}

			count3 = gf_list_count(pe->element.playlist.elements);
			if (!count3) continue;

			base_url = gf_strdup(pe->url);
			sep = strrchr(base_url, '/');

			if (pe->codecs && (pe->codecs[0] == '\"')) {
				u32 len = (u32) strlen(pe->codecs);
				strncpy(pe->codecs, pe->codecs+1, len-1);
				pe->codecs[len-2] = 0;
			}
#ifndef GPAC_DISABLE_MEDIA_IMPORT
			if (pe->bandwidth && pe->codecs && pe->width && pe->height) {
				import_file = 0;
			}
#endif

			/*get rid of level 0 aac*/
			elt = gf_list_get(pe->element.playlist.elements, 0);
			if (elt && strstr(elt->url, ".aac"))
				is_aac = 1;

			if (is_aac)
				fprintf(fmpd, "<!-- \n");

			width = pe->width;
			height = pe->height;
			samplerate = num_channels = 0;
#ifndef GPAC_DISABLE_MEDIA_IMPORT
			if (import_file) {
				GF_Err e;
				GF_MediaImporter import;
				char *tmp_file = NULL;
				elt = gf_list_get(pe->element.playlist.elements, 0);
				memset(&import, 0, sizeof(GF_MediaImporter));
				import.trackID = 0;
				import.flags = GF_IMPORT_PROBE_ONLY;

				if (strstr(elt->url, "://") && !strstr(elt->url, "file://")) {
					tmp_file = strrchr(elt->url, '/');
					if (!tmp_file) tmp_file = strrchr(elt->url, '\\');
					if (tmp_file) {
						tmp_file++;
						e = gf_dm_wget(elt->url, tmp_file, elt->byteRangeStart, elt->byteRangeEnd);
						if (e==GF_OK) {
							import.in_name = tmp_file;
						}
					}
				} else {
					import.in_name = elt->url;
				}
				e = gf_media_import(&import);

				if (!pe->bandwidth && pe->durationInfo) {
					u64 pos = 0;
					Double bw;
					FILE *t = gf_f64_open(import.in_name, "rb");
					if (t) {
						gf_f64_seek(t, 0, SEEK_END);
						pos = gf_f64_tell(t);
						fclose(t);
					}
					bw = (Double) pos;
					bw *= 8;
					bw /= pe->durationInfo;
					pe->bandwidth = (u32) bw;
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

			fprintf(fmpd, "   <Representation id=\"%s\" bandwidth=\"%d\"", sep ? sep+1 : base_url, pe->bandwidth);
			/* SOUCHAY : if mime-type is still unknown, do not try to add codec information since it would be wrong */
			if (!strcmp(M3U8_UNKOWN_MIME_TYPE, mimeTypeForM3U8Segments)) {
				fprintf(fmpd, " mimeType=\"%s\"", mimeTypeForM3U8Segments);
			} else {
				fprintf(fmpd, " mimeType=\"%s\"", mimeTypeForM3U8Segments);
			}
			if (pe->codecs)
				fprintf(fmpd, " codecs=\"%s\"", pe->codecs);
			if (width && height) {
				fprintf(fmpd, " width=\"%d\" height=\"%d\"", width, height);
			}
			if (samplerate)
				fprintf(fmpd, " audioSamplingRate=\"%d\"", samplerate);


			if (use_template) {
				if (sep) {
					/*keep final '/' */
					sep[1] = 0;
					fprintf(fmpd, ">\n    <BaseURL>%s</BaseURL>\n   </Representation>\n", base_url);
				} else
					fprintf(fmpd, "/>\n");

				if (is_aac)
					fprintf(fmpd, " -->");
				continue;
			}

			fprintf(fmpd, ">\n");

			byte_range_media_file = NULL;
			elt = gf_list_get(pe->element.playlist.elements, 0);
			if (elt && (elt->byteRangeEnd || elt->byteRangeStart)) {
				byte_range_media_file = elt->url;
				fprintf(fmpd, "    <BaseURL>%s</BaseURL>\n", byte_range_media_file);
			} else if (sep) {
				sep[1] = 0;
				fprintf(fmpd, "    <BaseURL>%s</BaseURL>\n", base_url);
			}

			fprintf(fmpd, "    <SegmentList duration=\"%d\">\n", pe->durationInfo);
			update_interval = (count3 - 1) * pe->durationInfo * 1000;
			for (k=0; k<count3; k++) {
				u32 cmp = 0;
				char *src_url, *seg_url;
				elt = gf_list_get(pe->element.playlist.elements, k);

				/*remove protocol scheme and try to find the common part in baseURL and segment URL - this avoids copying the entire url*/
				src_url = strstr(base_url, "://");
				if (src_url) src_url += 3;
				else src_url = base_url;

				seg_url = strstr(elt->url, "://");
				if (seg_url) seg_url += 3;
				else seg_url = elt->url;

				while (src_url[cmp] == seg_url[cmp]) cmp++;

				if (byte_range_media_file) {
					fprintf(fmpd, "     <SegmentURL mediaRange=\""LLU"-"LLU"\"", elt->byteRangeStart, elt->byteRangeEnd);
					if (strcmp(elt->url, byte_range_media_file) )fprintf(fmpd, " media=\"%s\"", elt->url);
					fprintf(fmpd, "/>\n");
				} else {
					fprintf(fmpd, "     <SegmentURL media=\"%s\"/>\n", cmp ? (seg_url + cmp) : elt->url);
				}
			}
			fprintf(fmpd, "    </SegmentList>\n");
			fprintf(fmpd, "   </Representation>\n");
			gf_free(base_url);

			if (is_aac)
				fprintf(fmpd, " -->\n");
		}
	}

	if (template_base) {
		gf_free(template_base);
		template_base = NULL;
	}

	fprintf(fmpd, "  </AdaptationSet>\n");
	fprintf(fmpd, " </Period>\n");
	fprintf(fmpd, "</MPD>");
	fclose(fmpd);
	variant_playlist_del(pl);
	return GF_OK;
}
