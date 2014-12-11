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
	if (mpd->xml_namespace && child->ns && !strcmp(mpd->xml_namespace, child->ns)) return 1;
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

void gf_mpd_descriptor_free(void *item)
{
	GF_MPD_Descriptor *mpd_desc = (GF_MPD_Descriptor*) item;
	if (mpd_desc->id) gf_free(mpd_desc->id);
	if (mpd_desc->scheme_id_uri) gf_free(mpd_desc->scheme_id_uri);
	if (mpd_desc->value) gf_free(mpd_desc->value);
	gf_mpd_extensible_free((GF_MPD_ExtensibleVirtual *)mpd_desc);

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
	if (ptr->playback.init_segment_data) gf_free(ptr->playback.init_segment_data);

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
	gf_mpd_extensible_free((GF_MPD_ExtensibleVirtual*) mpd);
	gf_free(mpd);
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
GF_Err gf_m3u8_to_mpd(const char *m3u8_file, const char *base_url,
                      const char *mpd_file,
                      u32 reload_count, char *mimeTypeForM3U8Segments, Bool do_import, Bool use_mpd_templates, GF_FileDownload *getter)
{
	GF_Err e;
	char *sep, *template_base, *template_ext;
	u32 i, nb_streams, j, k, template_width, template_idx_start;
	Double update_interval;
	MasterPlaylist *pl = NULL;
	Bool use_template;
	Stream *stream;
	PlaylistElement *pe, *the_pe, *elt;
	FILE *fmpd;
	Bool is_end;
	u32 max_dur = 0;

	e = gf_m3u8_parse_master_playlist(m3u8_file, &pl, base_url);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[M3U8] Failed to parse root playlist '%s', error = %s\n", m3u8_file, gf_error_to_string(e)));
		if (pl)
			gf_m3u8_master_playlist_del(pl);
		pl = NULL;
		return e;
	}
	if (mpd_file == NULL) {
		gf_delete_file(m3u8_file);
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
					break;
				}
				if (e == GF_OK) {
					e = gf_m3u8_parse_sub_playlist(getter->get_cache_name(getter), &pl, suburl, stream, pe);
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
					e = gf_m3u8_parse_sub_playlist(gf_dm_sess_get_cache_name(sess), &pl, suburl, prog, pe);
				}
				gf_service_download_del(sess);
#endif
			} else { /* for use in MP4Box */
				if (strstr(suburl, "://")) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MPD Generator] Downloading %s...\n", suburl));
					e = gf_dm_wget(suburl, "tmp.m3u8", 0, 0);
					if (e == GF_OK) {
						e = gf_m3u8_parse_sub_playlist("tmp.m3u8", &pl, suburl, stream, pe);
					} else {
						GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD Generator] Download faile for %s\n", suburl));
					}
					gf_delete_file("tmp.m3u8");
				} else {
					e = gf_m3u8_parse_sub_playlist(suburl, &pl, suburl, stream, pe);
				}
			}
			gf_free(suburl);
		}
		if (max_dur < (u32) stream->computed_duration) {
			max_dur = (u32) stream->computed_duration;
		}
	}

	is_end = !pl->playlist_needs_refresh;
	assert(the_pe);

	update_interval = 0;
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

	assert(mpd_file);
	fmpd = gf_f64_open(mpd_file, "wt");
	if (!fmpd) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[MPD Generator] Cannot write to temp file %s!\n", mpd_file));
		gf_m3u8_master_playlist_del(pl);
		return GF_IO_ERR;
	}

	fprintf(fmpd, "<MPD type=\"%s\" xmlns=\"urn:mpeg:DASH:schema:MPD:2011\" profiles=\"urn:mpeg:dash:profile:full:2011\"", is_end ? "static" : "dynamic" );
	sep = strrchr(m3u8_file, '/');
	if (!sep)
		sep = strrchr(m3u8_file, '\\');
	if (sep)
		sep = sep + 1;
	else
		sep = (char *)m3u8_file;
	fprintf(fmpd, " id=\"%s\"", sep);

	if (update_interval)
		fprintf(fmpd, " minimumUpdatePeriod=\"PT%02.2gS\"", update_interval);
	if (is_end)
		fprintf(fmpd, " mediaPresentationDuration=\"PT%dS\"", max_dur);
	fprintf(fmpd, " minBufferTime=\"PT1.5S\"");
	fprintf(fmpd, ">\n");

	fprintf(fmpd, " <ProgramInformation moreInformationURL=\"http://gpac.sourceforge.net\">\n");
	{
		char *title = the_pe->title;
		if (!title || strlen(title) < 2)
			title = the_pe->url;
		fprintf(fmpd, "  <Title>%s</Title>\n", title);
	}
	fprintf(fmpd, "  <Source>Generated from URL %s</Source>\n", base_url );
	fprintf(fmpd, "  <Copyright>Generated by GPAC %s from %s</Copyright>\n", GPAC_FULL_VERSION, base_url);

	fprintf(fmpd, " </ProgramInformation>\n");
	fprintf(fmpd, " <Period start=\"PT0S\"");
	if (is_end) fprintf(fmpd, " duration=\"PT%dS\"", max_dur);
	fprintf(fmpd, " >\n");
	
	nb_streams = gf_list_count(pl->streams);
	/*check if we use templates*/
	template_base = NULL;
	template_ext = NULL;
	use_template = use_mpd_templates;
	template_width = 0;
	template_idx_start = 0;
	elt = NULL;
	for (i=0; i<nb_streams; i++) {
		u32 count_variants;
		stream = gf_list_get(pl->streams, i);
		count_variants = gf_list_count(stream->variants);
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
	
	nb_streams = gf_list_count(pl->streams);
	for (i=0; i<nb_streams; i++) {
		u32 count_variants;
		u32 width, height, samplerate, num_channels;
		fprintf(fmpd, "  <AdaptationSet segmentAlignment=\"true\">\n");

		/*if we use templates, put the SegmentTemplate element at the adaptationSet level*/
		if (use_template) {
			fprintf(fmpd, "   <SegmentTemplate");

			fprintf(fmpd, " duration=\"%d\"", pe->duration_info);
			if (template_width > 1) {
				fprintf(fmpd, " media=\"%s$%%0%ddNumber$%s\"", template_base, template_width, template_ext);
			} else {
				fprintf(fmpd, " media=\"%s$Number$%s\"", template_base, template_ext);
			}
			fprintf(fmpd, " startNumber=\"%d\"", template_idx_start);
			fprintf(fmpd, "/>\n");
		}

		if (elt && do_import) {
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

			if (import.nb_tracks > 1) {
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
		stream = gf_list_get(pl->streams, i);
		count_variants = gf_list_count(stream->variants);
		for (j=0; j<count_variants; j++) {
			char *base_url;
			u32 count_elements;
#ifndef GPAC_DISABLE_MEDIA_IMPORT
			Bool import_file = do_import;
#endif
			Bool is_aac = GF_FALSE;
			char *byte_range_media_file = NULL;
			pe = gf_list_get(stream->variants, j);

			if (pe->element_type == TYPE_MEDIA) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD] NOT SUPPORTED: M3U8 Media\n"));
			} else if (pe->element_type != TYPE_PLAYLIST) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD] NOT SUPPORTED: M3U8 unknown type for %s\n", pe->url));
			}

			count_elements = gf_list_count(pe->element.playlist.elements);
			if (!count_elements)
				continue;

			base_url = gf_strdup(pe->url);
			sep = strrchr(base_url, '/');

			if (pe->codecs && (pe->codecs[0] == '\"')) {
				u32 len = (u32) strlen(pe->codecs);
				strncpy(pe->codecs, pe->codecs+1, len-1);
				pe->codecs[len-2] = 0;
			}
#ifndef GPAC_DISABLE_MEDIA_IMPORT
			if (pe->bandwidth && pe->codecs && pe->width && pe->height) {
				import_file = GF_FALSE;
			}
#endif

			k = 0;
try_next_segment:
			k++;
			elt = gf_list_get(pe->element.playlist.elements, k);
			if (!elt)
				break;
			/*get rid of level 0 aac*/
			if (elt && strstr(elt->url, ".aac"))
				is_aac = GF_TRUE;

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
				memset(&import, 0, sizeof(GF_MediaImporter));
				import.trackID = 0;
				import.flags = GF_IMPORT_PROBE_ONLY;

				if (strstr(elt->url, "://") && !strstr(elt->url, "file://")) {
					tmp_file = strrchr(elt->url, '/');
					if (!tmp_file)
						tmp_file = strrchr(elt->url, '\\');
					if (tmp_file) {
						tmp_file++;
						e = gf_dm_wget(elt->url, tmp_file, elt->byte_range_start, elt->byte_range_end);
						if (e == GF_OK) {
							import.in_name = tmp_file;
						}
					}
				} else {
					import.in_name = elt->url;
				}
				e = gf_media_import(&import);
				if (!e) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD] M3U8 missing Media Element %s (Playlist %s) %s \n", import.in_name, base_url));
					goto try_next_segment;
				}

				if (import.in_name && !pe->bandwidth && pe->duration_info) {
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
					bw /= pe->duration_info;
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
			/* TODO : if mime-type is still unknown, don't try to add codec information since it would be wrong */
			if (!strcmp(M3U8_UNKNOWN_MIME_TYPE, mimeTypeForM3U8Segments)) {
				fprintf(fmpd, " mimeType=\"%s\"", mimeTypeForM3U8Segments);
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD] Unknown mime-type when converting from M3U8 HLS playlist, setting %s\n", mimeTypeForM3U8Segments));
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
			if (elt && (elt->byte_range_end || elt->byte_range_start)) {
				byte_range_media_file = elt->url;
				fprintf(fmpd, "    <BaseURL>%s</BaseURL>\n", byte_range_media_file);
			} else if (sep) {
				sep[1] = 0;
				fprintf(fmpd, "    <BaseURL>%s</BaseURL>\n", base_url);
			}

			fprintf(fmpd, "    <SegmentList duration=\"%lf\">\n", pe->duration_info);
			update_interval = (count_elements - 1) * pe->duration_info * 1000;
			for (k=0; k<count_elements; k++) {
				u32 cmp = 0;
				char *src_url, *seg_url;
				elt = gf_list_get(pe->element.playlist.elements, k);

				/* remove protocol scheme and try to find the common part in baseURL and segment URL - this avoids copying the entire url */
				src_url = strstr(base_url, "://");
				if (src_url) src_url += 3;
				else src_url = base_url;

				seg_url = strstr(elt->url, "://");
				if (seg_url) seg_url += 3;
				else seg_url = elt->url;

				while (src_url[cmp] == seg_url[cmp]) cmp++;

				if (byte_range_media_file) {
					fprintf(fmpd, "     <SegmentURL mediaRange=\""LLU"-"LLU"\"", elt->byte_range_start, elt->byte_range_end);
					if (strcmp(elt->url, byte_range_media_file))
						fprintf(fmpd, " media=\"%s\"", elt->url);
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

		if (template_base) {
			gf_free(template_base);
			template_base = NULL;
		}

		fprintf(fmpd, "  </AdaptationSet>\n");
	}
	fprintf(fmpd, " </Period>\n");
	fprintf(fmpd, "</MPD>");

	fclose(fmpd);
	gf_m3u8_master_playlist_del(pl);

	return GF_OK;
}

void gf_mpd_print_date(FILE *out, char *name, u64 time)
{
	time_t gtime;
	struct tm *t;
	gtime = time / 1000;
	t = gmtime(&gtime);
	fprintf(out, " %s=\"%d-%02d-%02dT%02d:%02d:%02dZ\"", name, 1900+t->tm_year, t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
}

void gf_mpd_print_duration(FILE *out, char *name, u32 duration)
{
	u32 h, m;
	Double s;
	h = (u32) (duration / 3600000);
	m = (u32) (duration/ 60000) - h*60;
	s = ((Double) duration/1000.0) - h*3600 - m*60;

	fprintf(out, " %s=\"PT%02dH%02dM%02.2fS\"", name, h, m, s);
}

static void gf_mpd_print_base_url(FILE *out, GF_List *base_URLs, char *indent) 
{
	GF_MPD_BaseURL *url;
	u32 i;
	i=0; 
	while ((url = (GF_MPD_BaseURL *)gf_list_enum(base_URLs, &i))) {
		fprintf(out, "%s<BaseURL ", indent);
		if (url->service_location)
			fprintf(out, " serviceLocation=\"%s\"", url->service_location);
		if (url->byte_range)
			fprintf(out, " byteRange=\""LLD"-"LLD"\"", url->byte_range->start_range, url->byte_range->end_range);
		fprintf(out, ">%s</BaseURL>\n", url->URL);
	}
}

static void gf_mpd_print_url(FILE *out, GF_MPD_URL *url, char *name, char *indent)
{
	fprintf(out, "%s<%s", indent, name);
	if (url->byte_range) fprintf(out, " range=\""LLD"-"LLD"\"", url->byte_range->start_range, url->byte_range->end_range);
	if (url->sourceURL) fprintf(out, " range=\"%s\"", url->sourceURL);
	fprintf(out, "/>\n");
}

static void gf_mpd_print_segment_base_attr(FILE *out, GF_MPD_SegmentBase *s)
{
	if (s->timescale) fprintf(out, " timescale=\"%d\"", s->timescale);
	if (s->presentation_time_offset) fprintf(out, " timescale=\""LLU"\"", s->presentation_time_offset);
	if (s->index_range) fprintf(out, " indexRange=\""LLD"-"LLD"\"", s->index_range->start_range, s->index_range->end_range);
	if (s->index_range_exact) fprintf(out, " indexRangeExact=\"true\"");
	if (s->availability_time_offset) fprintf(out, " availabilityTimeOffset=\"%g\"", s->availability_time_offset);
	if (s->time_shift_buffer_depth) 
		gf_mpd_print_duration(out, "timeShiftBufferDepth", s->time_shift_buffer_depth);
}

static void gf_mpd_print_segment_base(FILE *out, GF_MPD_SegmentBase *s, char *indent)
{
	fprintf(out, "%s<SegmentBase", indent);
	gf_mpd_print_segment_base_attr(out, s);
	fprintf(out, ">\n");

	if (s->initialization_segment) gf_mpd_print_url(out, s->initialization_segment, "Initialization", indent);
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
		fprintf(out, "%s <S ", indent);
		if (se->start_time) fprintf(out, " t=\""LLD"\"", se->start_time);
		if (se->duration) fprintf(out, " d=\"%d\"", se->duration);
		if (se->repeat_count) fprintf(out, " r=\"%d\"", se->repeat_count);
		fprintf(out, ">\n");
	}
	fprintf(out, "%s </SegmentTimeline>\n", indent);
}

static u32 gf_mpd_print_multiple_segment_base(FILE *out, GF_MPD_MultipleSegmentBase *ms, char *indent)
{
	gf_mpd_print_segment_base_attr(out, (GF_MPD_SegmentBase *)ms);

	if (ms->duration) fprintf(out, " duration=\""LLD"\"", ms->duration);
	if (ms->start_number != (u32) -1) fprintf(out, " startNumber=\"%d\"", ms->start_number);


	if (!ms->bitstream_switching_url && !ms->segment_timeline && !ms->initialization_segment && !ms->representation_index) {
		fprintf(out, "/>\n");
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
	gf_mpd_print_multiple_segment_base(out, (GF_MPD_MultipleSegmentBase *)s, indent);

	if (s->segment_URLs) {
		u32 i;
		GF_MPD_SegmentURL *url;
		i = 0;
		while ( (url = gf_list_enum(s->segment_URLs, &i))) {
			fprintf(out, "%s<SegmentURL", indent);
			if (url->media) fprintf(out, " media=\"%s\"", url->media);
			if (url->index) fprintf(out, " index=\"%s\"", url->index);
			if (url->media_range) fprintf(out, " mediaRange=\""LLD"-"LLD"\"", url->media_range->start_range, url->media_range->end_range);
			if (url->index_range) fprintf(out, " indexRange=\""LLD"-"LLD"\"", url->index_range->start_range, url->index_range->end_range);
			fprintf(out, ">\n");
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

	if (gf_mpd_print_multiple_segment_base(out, (GF_MPD_MultipleSegmentBase *)s, indent))
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

static u32 gf_mpd_print_common_representation(FILE *out, GF_MPD_CommonAttributes *ca, char *indent, Bool can_close)
{
	if (ca->profiles) fprintf(out, " profiles=\"%s\"", ca->profiles);
	if (ca->width) fprintf(out, " width=\"%d\"", ca->width);
	if (ca->height) fprintf(out, " height=\"%d\"", ca->height);
	if (ca->sar) fprintf(out, " sar=\"%d:%d\"", ca->sar->num, ca->sar->den);
	if (ca->framerate) fprintf(out, " frameRate=\"%d:%d\"", ca->framerate->num, ca->framerate->den);
	if (ca->samplerate) fprintf(out, " audioSamplingRate=\"%d\"", ca->samplerate);
	if (ca->mime_type) fprintf(out, " mimeType=\"%s\"", ca->mime_type);
	if (ca->segmentProfiles) fprintf(out, " segmentProfiles=\"%s\"", ca->segmentProfiles);
	if (ca->codecs) fprintf(out, " codecs=\"%s\"", ca->codecs);
	if (ca->maximum_sap_period) fprintf(out, " maximumSAPPeriod=\"%d\"", ca->maximum_sap_period);
	if (ca->starts_with_sap) fprintf(out, " startWithSAP=\"%d\"", ca->starts_with_sap);
	if (ca->max_playout_rate) fprintf(out, " maxPlayoutRate=\"%g\"", ca->max_playout_rate);
	if (ca->coding_dependency) fprintf(out, " codingDependency=\"true\"");
	if (ca->scan_type!=GF_MPD_SCANTYPE_UNKNWON) fprintf(out, " scanType=\"%s\"", ca->scan_type==GF_MPD_SCANTYPE_PROGRESSIVE ? "progressive" : "interlaced");

	if (can_close && !gf_list_count(ca->frame_packing) && !gf_list_count(ca->audio_channels) && !gf_list_count(ca->content_protection) && !gf_list_count(ca->essential_properties) && !gf_list_count(ca->supplemental_properties) && !ca->isobmf_tracks) {
		fprintf(out, "/>\n");
		return 1;
	}

	fprintf(out, ">\n");

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

	gf_mpd_print_descriptors(out, ca->frame_packing, "Framepacking", indent);
	gf_mpd_print_descriptors(out, ca->audio_channels, "AudioChannelConfiguration", indent);
	gf_mpd_print_descriptors(out, ca->content_protection, "ContentProtection", indent);
	gf_mpd_print_descriptors(out, ca->essential_properties, "EssentialProperty", indent);
	gf_mpd_print_descriptors(out, ca->supplemental_properties, "SupplementalProperty", indent);
	return 0;
}

static void gf_mpd_print_representation(FILE *out, GF_MPD_Representation *rep)
{
	Bool can_close = GF_FALSE;
	fprintf(out, "   <Representation");
	if (rep->id) fprintf(out, " id=\"%s\"", rep->id);
	if (rep->bandwidth) fprintf(out, " bandwidth=\"%d\"", rep->bandwidth);
	if (rep->quality_ranking) fprintf(out, " qualityRanking=\"%d\"", rep->quality_ranking);
	if (rep->dependency_id) fprintf(out, " dependencyId=\"%s\"", rep->dependency_id);
	if (rep->media_stream_structure_id) fprintf(out, " mediaStreamStructureId=\"%s\"", rep->media_stream_structure_id);


	if (!gf_list_count(rep->base_URLs) && !rep->segment_base && !rep->segment_template && !rep->segment_list && !gf_list_count(rep->sub_representations)) {
		can_close = 1;
	}

	if (gf_mpd_print_common_representation(out, (GF_MPD_CommonAttributes*)rep, "    ", can_close)) 
		return;

	gf_mpd_print_base_url(out, rep->base_URLs, " ");
	if (rep->segment_base) {
		gf_mpd_print_segment_base(out, rep->segment_base, "   ");
	}
	if (rep->segment_list) {
		gf_mpd_print_segment_list(out, rep->segment_list, "   ");
	}
	if (rep->segment_template) {
		gf_mpd_print_segment_template(out, rep->segment_template, "   ");
	}
		/*TODO
					e = gf_mpd_parse_subrepresentation(rep->sub_representations, child);
					if (e) return e;
		*/

	fprintf(out, "   </Representation>\n");
}

static void gf_mpd_print_adaptation_set(FILE *out, GF_MPD_AdaptationSet *as)
{
	u32 i;
	GF_MPD_Representation *rep;
	fprintf(out, "  <AdaptationSet");

	if (as->xlink_href) {
		fprintf(out, " xlink:href=\"%s\"", as->xlink_href);
		if (as->xlink_actuate_on_load) 
			fprintf(out, " actuate=\"onLoad\"");
	}
	if (as->id) fprintf(out, " id=\"%d\"", as->id);
	if (as->group !=  (u32) -1) fprintf(out, " group=\"%d\"", as->group);
	if (as->lang) fprintf(out, " lang=\"%s\"", as->lang);
	if (as->par) fprintf(out, " par=\"%d:%d\"", as->par->num, as->par->den);
	if (as->min_bandwidth) fprintf(out, " minBandwidth=\"%d\"", as->min_bandwidth);
	if (as->max_bandwidth) fprintf(out, " maxBandwidth=\"%d\"", as->max_bandwidth);
	if (as->min_width) fprintf(out, " minWidth=\"%d\"", as->min_width);
	if (as->max_width) fprintf(out, " maxWidth=\"%d\"", as->max_width);
	if (as->min_height) fprintf(out, " minHeight=\"%d\"", as->min_height);
	if (as->max_height) fprintf(out, " maxHeight=\"%d\"", as->max_height);
	if (as->min_framerate) fprintf(out, " minFrameRate=\"%d\"", as->min_framerate);
	if (as->max_framerate) fprintf(out, " maxFrameRate=\"%d\"", as->max_framerate);
	if (as->segment_alignment) fprintf(out, " segmentAlignment=\"true\"");
	if (as->bitstream_switching) fprintf(out, " bitstreamSwitching=\"true\"");
	if (as->subsegment_alignment) fprintf(out, " subsegmentAlignment=\"true\"");
	if (as->subsegment_starts_with_sap) fprintf(out, " subsegmentStartsWithSAP=\"%d\"", as->subsegment_starts_with_sap);


	gf_mpd_print_common_representation(out, (GF_MPD_CommonAttributes*)as, "   ", 0);
	
	gf_mpd_print_base_url(out, as->base_URLs, " ");

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
		gf_mpd_print_representation(out, rep);
	}
	fprintf(out, "  </AdaptationSet>\n");


}

static void gf_mpd_print_period(GF_MPD_Period *period, FILE *out)
{
	GF_MPD_AdaptationSet *as;
	u32 i;
	fprintf(out, " <Period");
	if (period->xlink_href) {
		fprintf(out, " xlink:href=\"%s\"", period->xlink_href);
		if (period->xlink_actuate_on_load) 
			fprintf(out, " actuate=\"onLoad\"");
	}
	if (period->ID) 
		fprintf(out, " id=\"%s\"", period->ID);
	if (period->start) 
		gf_mpd_print_duration(out, "start", period->start);
	if (period->duration) 
		gf_mpd_print_duration(out, "duration", period->start);
	if (period->bitstream_switching) 
		fprintf(out, " bitstreamSwitching=\"true\"");

	fprintf(out, ">\n");
	
	gf_mpd_print_base_url(out, period->base_URLs, " ");

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
		gf_mpd_print_adaptation_set(out, as);
	}
	fprintf(out, " </Period>\n");

}

static GF_Err gf_mpd_write(GF_MPD *mpd, FILE *out)
{
	u32 i;
	GF_MPD_ProgramInfo *info;
	char *text;
	GF_MPD_Period *period;

	fprintf(out, "<?xml version=\"1.0\"?>\n<MPD xmlns=\"%s\" type=\"%s\"", mpd->xml_namespace, (mpd->type == GF_MPD_TYPE_STATIC) ? "static" : "dynamic");

	if (mpd->ID)
		fprintf(out, " ID=\"%s\"", mpd->ID);
	
	if (mpd->profiles)
		fprintf(out, " profiles=\"%s\"", mpd->profiles);
	if (mpd->availabilityStartTime)
		gf_mpd_print_date(out, "availabilityStartTime", mpd->availabilityStartTime);
	if (mpd->availabilityEndTime)
		gf_mpd_print_date(out, "availabilityStartTime", mpd->availabilityEndTime);
	if (mpd->publishTime)
		gf_mpd_print_date(out, "availabilityStartTime", mpd->publishTime);
	if (mpd->media_presentation_duration) 
		gf_mpd_print_duration(out, "mediaPresentationDuration", mpd->media_presentation_duration);
	if (mpd->minimum_update_period) 
		gf_mpd_print_duration(out, "minimumUpdatePeriod", mpd->minimum_update_period);
	if (mpd->min_buffer_time) 
		gf_mpd_print_duration(out, "minBufferTime", mpd->min_buffer_time);
	if (mpd->time_shift_buffer_depth) 
		gf_mpd_print_duration(out, "timeShiftBufferDepth", mpd->time_shift_buffer_depth);
	if (mpd->suggested_presentaton_delay) 
		gf_mpd_print_duration(out, "suggestedPresentationDelay", mpd->suggested_presentaton_delay);
	if (mpd->max_segment_duration) 
		gf_mpd_print_duration(out, "maxSegmentDuration", mpd->max_segment_duration);
	if (mpd->max_subsegment_duration) 
		gf_mpd_print_duration(out, "maxSubsegmentDuration", mpd->max_subsegment_duration);

	if (mpd->attributes) gf_mpd_extensible_print_attr(out, (GF_MPD_ExtensibleVirtual*)mpd);

	fprintf(out, ">\n");

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
		fprintf(out, " </ProgramInformation>\n");
	}

	gf_mpd_print_base_url(out, mpd->base_URLs, " ");

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
		gf_mpd_print_period(period, out);
	}


	fprintf(out, "</MPD>\n");

	return GF_OK;
}

GF_EXPORT
GF_Err gf_mpd_write_file(GF_MPD *mpd, char *file_name)
{
	GF_Err e;
	FILE *out;
	if (!strcmp(file_name, "std")) out = stdout;
	else {
		out = fopen(file_name, "wb");
		if (!out) return GF_IO_ERR;
	}

	e = gf_mpd_write(mpd, out);
	fclose(out);
	return e;
}


GF_EXPORT
GF_Err gf_mpd_resolve_url(GF_MPD *mpd, GF_MPD_Representation *rep, GF_MPD_AdaptationSet *set, GF_MPD_Period *period, const char *mpd_url, GF_MPD_URLResolveType resolve_type, u32 item_index, u32 nb_segments_removed, char **out_url, u64 *out_range_start, u64 *out_range_end, u64 *segment_duration, Bool *is_in_base_url)
{
	GF_MPD_BaseURL *url_child;
	GF_MPD_SegmentTimeline *timeline = NULL;
	u32 start_number = 1;
	u32 timescale=0;
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

	/*single URL*/
	if (!rep->segment_list && !set->segment_list && !period->segment_list && !rep->segment_template && !set->segment_template && !period->segment_template) {
		GF_MPD_URL *res_url;
		GF_MPD_SegmentBase *base_seg = NULL;
		if (item_index > 0)
			return GF_EOS;
		switch (resolve_type) {
		case GF_MPD_RESOLVE_URL_MEDIA:
		case GF_MPD_RESOLVE_URL_MEDIA_TEMPLATE:
			if (!url) return GF_NON_COMPLIANT_BITSTREAM;
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
				*out_url = gf_url_concatenate(url, res_url->sourceURL);
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
			if (period->segment_list->representation_index) index_url = period->segment_list->representation_index;
			if (period->segment_list->segment_URLs) segments = period->segment_list->segment_URLs;
			if (period->segment_list->start_number != (u32) -1) start_number = period->segment_list->start_number;
			if (period->segment_list->segment_timeline) timeline = period->segment_list->segment_timeline;
			if (!timescale && period->segment_list->timescale) timescale = period->segment_list->timescale;
		}
		if (set->segment_list) {
			if (set->segment_list->initialization_segment) init_url = set->segment_list->initialization_segment;
			if (set->segment_list->representation_index) index_url = set->segment_list->representation_index;
			if (set->segment_list->segment_URLs) segments = set->segment_list->segment_URLs;
			if (set->segment_list->start_number != (u32) -1) start_number = set->segment_list->start_number;
			if (set->segment_list->segment_timeline) timeline = set->segment_list->segment_timeline;
			if (!timescale && set->segment_list->timescale) timescale = set->segment_list->timescale;
		}
		if (rep->segment_list) {
			if (rep->segment_list->initialization_segment) init_url = rep->segment_list->initialization_segment;
			if (rep->segment_list->representation_index) index_url = rep->segment_list->representation_index;
			if (rep->segment_list->segment_URLs) segments = rep->segment_list->segment_URLs;
			if (rep->segment_list->start_number != (u32) -1) start_number = rep->segment_list->start_number;
			if (rep->segment_list->segment_timeline) timeline = rep->segment_list->segment_timeline;
			if (!timescale && rep->segment_list->timescale) timescale = rep->segment_list->timescale;
		}


		segment_count = gf_list_count(segments);

		switch (resolve_type) {
		case GF_MPD_RESOLVE_URL_INIT:
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
				*segment_duration = (u32) ((Double) (segment->duration) * 1000.0 / timescale);
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
	}
	if (set->segment_template) {
		if (set->segment_template->initialization) init_template = set->segment_template->initialization;
		if (set->segment_template->index) index_template = set->segment_template->index;
		if (set->segment_template->media) media_url = set->segment_template->media;
		if (set->segment_template->start_number != (u32) -1) start_number = set->segment_template->start_number;
		if (set->segment_template->segment_timeline) timeline = set->segment_template->segment_timeline;
		if (!timescale && set->segment_template->timescale) timescale = set->segment_template->timescale;
	}
	if (rep->segment_template) {
		if (rep->segment_template->initialization) init_template = rep->segment_template->initialization;
		if (rep->segment_template->index) index_template = rep->segment_template->index;
		if (rep->segment_template->media) media_url = rep->segment_template->media;
		if (rep->segment_template->start_number != (u32) -1) start_number = rep->segment_template->start_number;
		if (rep->segment_template->segment_timeline) timeline = rep->segment_template->segment_timeline;
		if (!timescale && rep->segment_template->timescale) timescale = rep->segment_template->timescale;
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
			strcat(solved_template, rep->id);
		}
		else if (!strcmp(first_sep+1, "Number")) {
			if (resolve_type==GF_MPD_RESOLVE_URL_MEDIA_TEMPLATE) {
				strcat(solved_template, "$Number$");
			} else {
				sprintf(szFormat, szPrintFormat, start_number + item_index);
				strcat(solved_template, szFormat);
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

