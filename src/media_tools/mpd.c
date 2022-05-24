/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre, Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2000-2022
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

#include <gpac/mpd.h>
#include <gpac/download.h>
#include <gpac/internal/m3u8.h>
#include <gpac/network.h>
#include <gpac/maths.h>

#ifndef GPAC_DISABLE_CORE_TOOLS

#define MPD_STORE_EXTENSION_ATTR(_elem)	\
			if (!_elem->x_attributes) _elem->x_attributes = gf_list_new();	\
			i--;	\
			gf_list_rem(root->attributes, i);	\
			gf_list_add(_elem->x_attributes, att);	\

#define MPD_STORE_EXTENSION_NODE(_elem)	\
		if (!_elem->x_children) _elem->x_children = gf_list_new();	\
		i--;	\
		gf_list_rem(root->content, i);	\
		child->orig_pos = child_idx;\
		gf_list_add(_elem->x_children, child);	\

#define MPD_FREE_EXTENSION_NODE(_elem)	\
	if (_elem->x_attributes) {\
		while (gf_list_count(_elem->x_attributes)) {\
			GF_XMLAttribute *att = gf_list_pop_back(_elem->x_attributes);\
			if (att->name) gf_free(att->name);\
			if (att->value) gf_free(att->value);\
			gf_free(att);\
		}\
		gf_list_del(_elem->x_attributes);\
	}\
	if (_elem->x_children) {\
		while (gf_list_count(_elem->x_children)) {\
			GF_XMLNode *child = gf_list_pop_back(_elem->x_children);\
			gf_xml_dom_node_del(child);\
		}\
		gf_list_del(_elem->x_children);\
	}\


static Bool gf_mpd_parse_bool(const char * const attr)
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
	if (child->ns && !strcmp(child->ns, "gpac")) return 1;
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

static u32 gf_mpd_parse_int(const char * const attr)
{
	return atoi(attr);
}

static u64 gf_mpd_parse_long_int(const char * const attr)
{
	u64 longint;
	sscanf(attr, LLU, &longint);
	return longint;
}

static Double gf_mpd_parse_double(const char * const attr)
{
	return atof(attr);
}

static GF_MPD_Fractional *gf_mpd_parse_frac(const char * const attr, const char sep, GF_MPD_Fractional *res)
{
	char str[6];
	int ok;
	if (res==NULL) {
		GF_SAFEALLOC(res, GF_MPD_Fractional);
		if (!res) return NULL;
		res->den = 1;
	}
	snprintf(str, sizeof(str), "%%d%c%%d", sep);
	ok = sscanf(attr, str, &res->num, &res->den);
	if (ok!=2) {
		res->den = 1;
		res->num = atoi(attr);
	}
	return res;
}

static GF_Fraction gf_mpd_parse_fraction(const char * const attr)
{
	GF_Fraction res;
	int ok = sscanf(attr, "%d/%u", &res.num, &res.den);
	if (ok!=2) {
		res.den = 1;
		res.num = atoi(attr);
	}
	return res;
}

u64 gf_mpd_parse_date(const char * const attr)
{
	return gf_net_parse_date(attr);
}

static u64 gf_mpd_parse_duration(const char * const duration)
{
	u32 i;
	char *sep1, *sep2;
	u32 h, m, s, ms;
	u32 year, month, day;
	Bool has_year, has_month, has_day;
	u64 y_dur;
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

	year = month = day = 0;
	has_year = strchr(duration+1, 'Y') ? GF_TRUE : GF_FALSE;
	has_month = strchr(duration+1, 'M') ? GF_TRUE : GF_FALSE;
	has_day = strchr(duration+1, 'D') ? GF_TRUE : GF_FALSE;
	if (has_year && has_month && has_day) sscanf(duration+1, "%dY%dM%dD", &year, &month, &day);
	else if (has_month && has_day) sscanf(duration+1, "%dM%dD", &month, &day);
	else if (has_year && has_month) sscanf(duration+1, "%dY%dM", &year, &month);
	else if (has_year && has_day) sscanf(duration+1, "%dY%dD", &year, &day);
	else if (has_year) sscanf(duration+1, "%dY", &year);
	else if (has_month) sscanf(duration+1, "%dM", &month);
	else if (has_day) sscanf(duration+1, "%dD", &day);
	y_dur = (year*365 + month*30 + day ) * 24;
	y_dur *= 3600;
	y_dur *= 1000;

	if (! startT) return y_dur;

	h = m = s = ms = 0;
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
		char *sep_dec = strchr(sep2, '.');
		*sep1 = 0;
		if (sep_dec) {
			sep_dec[0] = 0;
			s = atoi(sep2);
			ms = atoi(sep_dec+1);
			sep_dec[0] = '.';
		} else {
			s = atoi(sep2);
		}
		*sep1 = 'S';
	}
	return y_dur + (u64) ( ((h*3600+m*60+s)*(u64)1000) + ms );
}

static u32 gf_mpd_parse_duration_u32(const char * const duration)
{
	u64 dur = gf_mpd_parse_duration(duration);
	if (dur <= GF_UINT_MAX) {
		return (u32)dur;
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MPD] Parsed duration %s ("LLU") doesn't fit on 32 bits! Setting to the 32 bits max.\n", duration, dur));
		return GF_UINT_MAX;
	}
}

static GF_MPD_ByteRange *gf_mpd_parse_byte_range(const char * const attr)
{
	GF_MPD_ByteRange *br;
	GF_SAFEALLOC(br, GF_MPD_ByteRange);
	if (!br) return NULL;
	sscanf(attr, LLD"-"LLD, &br->start_range, &br->end_range);
	return br;
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
	GF_XMLNode *child;
	GF_XMLAttribute *att;
	u32 child_idx, i;

	GF_SAFEALLOC(info, GF_MPD_ProgramInfo);
	if (!info) return GF_OUT_OF_MEM;

	i = 0;
	while ( (att = gf_list_enum(root->attributes, &i)) ) {
		if (!strcmp(att->name, "moreInformationURL")) {
			info->more_info_url = gf_mpd_parse_string(att->value);
		} else if (!strcmp(att->name, "lang")) {
			info->lang = gf_mpd_parse_string(att->value);
		} else {
			MPD_STORE_EXTENSION_ATTR(info)
		}
	}

	child_idx = i = 0;
	while ((child = gf_list_enum(root->content, &i))) {
		if (child->type != GF_XML_NODE_TYPE)
			continue;
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
		} else {
			MPD_STORE_EXTENSION_NODE(info)
		}
		child_idx++;
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
 	seg->time_shift_buffer_depth = (u32) -1; /*infinite by default*/

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
	u64 curr_start_time = 0;
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
			seg_tl_ent->start_time = curr_start_time;
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
			if (seg_tl_ent->start_time)
				curr_start_time = seg_tl_ent->start_time;

			curr_start_time += (u64) (seg_tl_ent->duration * (seg_tl_ent->repeat_count+1));
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

#if 0 //unused
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
	if(media)
		seg_url->media=gf_strdup(media);
	return seg_url;
}
#endif

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
		else if (!strcmp(att->name, "hls:keyIV")) {
			GF_Err e = gf_bin128_parse(att->value, seg->key_iv);
            if (e != GF_OK) {
                GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD] Cannot parse hls:keyIV\n"));
                return;
            }
        }
		else if (!strcmp(att->name, "duration")) seg->duration=gf_mpd_parse_int(att->value);
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


static GF_Err gf_mpd_parse_descriptor_ex(GF_List *container, GF_MPD_Descriptor **out_ptr, GF_XMLNode *root)
{
	GF_XMLAttribute *att;
	GF_XMLNode *child;
	GF_MPD_Descriptor *mpd_desc;
	u32 i = 0;
	u32 child_idx = 0;

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
	if (container)
		gf_list_add(container, mpd_desc);
	else {
		assert(out_ptr);
		if (*out_ptr) gf_mpd_descriptor_free(*out_ptr);
		*out_ptr = mpd_desc;
	}

	i = 0;
	child_idx = 0;
	while ( (child = gf_list_enum(root->content, &i))) {
		if (child->type != GF_XML_NODE_TYPE) continue;

		MPD_STORE_EXTENSION_NODE(mpd_desc)
		child_idx++;
	}
	return GF_OK;
}

static GF_Err gf_mpd_parse_descriptor(GF_List *container, GF_XMLNode *root)
{
	return gf_mpd_parse_descriptor_ex(container, NULL, root);
}

GF_MPD_ProducerReferenceTime *gf_mpd_parse_produce_ref_time(GF_XMLNode *root)
{
	GF_XMLAttribute *att;
	GF_XMLNode *child;
	GF_MPD_ProducerReferenceTime *pref;
	u32 i = 0;

	GF_SAFEALLOC(pref, GF_MPD_ProducerReferenceTime);
	if (!pref) return NULL;

	while ( (att = gf_list_enum(root->attributes, &i)) ) {
		if (!strcmp(att->name, "id")) pref->ID = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "inband")) pref->inband = gf_mpd_parse_bool(att->value);
		else if (!strcmp(att->name, "type")) {
			if (!strcmp(att->value, "application")) pref->type = GF_MPD_PRODUCER_REF_APPLICATION;
			else if (!strcmp(att->value, "captured")) pref->type = GF_MPD_PRODUCER_REF_CAPTURED;
		}
		else if (!strcmp(att->name, "applicationScheme") && !pref->scheme) pref->scheme = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "wallClockTime") && !pref->wallclock) pref->wallclock = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "presentationTime")) pref->presentation_time = gf_mpd_parse_long_int(att->value);
	}

	i = 0;
	while ( (child = gf_list_enum(root->content, &i))) {
		if (child->type != GF_XML_NODE_TYPE) continue;
		if (!strcmp(child->name, "UTCTiming"))
			gf_mpd_parse_descriptor_ex(NULL, &pref->utc_timing, child);
	}
	return pref;
}

static void gf_mpd_parse_common_representation_attr(GF_MPD *mpd, GF_MPD_CommonAttributes *com, GF_XMLNode *root, GF_XMLAttribute *att, u32 *index)
{
	if (!strcmp(att->name, "profiles")) com->profiles = gf_mpd_parse_string(att->value);
	else if (!strcmp(att->name, "width")) com->width = gf_mpd_parse_int(att->value);
	else if (!strcmp(att->name, "height")) com->height = gf_mpd_parse_int(att->value);
	else if (!strcmp(att->name, "sar")) {
		if (com->sar) gf_free(com->sar);
		com->sar = gf_mpd_parse_frac(att->value, ':', NULL);
	}
	else if (!strcmp(att->name, "frameRate")) {
		if (com->framerate) gf_free(com->framerate);
		com->framerate = gf_mpd_parse_frac(att->value, '/', NULL);
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
	else if (!strcmp(att->name, "selectionRriority")) com->selection_priority = gf_mpd_parse_int(att->value);
	else if (!strcmp(att->name, "tag")) com->tag = gf_mpd_parse_string(att->value);

	else {
		u32 i = *index;
		MPD_STORE_EXTENSION_ATTR(com);
		(*index) = i;
	}
}

static void gf_mpd_parse_common_representation_child(GF_MPD *mpd, GF_MPD_CommonAttributes *com, GF_XMLNode *root, GF_XMLNode *child, u32 *index, u32 child_idx)
{
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
	else if (!strcmp(child->name, "ProducerReferenceTime")) {
		GF_MPD_ProducerReferenceTime *pref = gf_mpd_parse_produce_ref_time(child);
		if (pref) {
			if (!com->producer_reference_time) com->producer_reference_time = gf_list_new();
			gf_list_add(com->producer_reference_time, pref);
		}
	} else {
		u32 i = *index;
		MPD_STORE_EXTENSION_NODE(com);
		*index = i;
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
	return rep;
}

static GF_DASH_SegmenterContext *gf_mpd_parse_dasher_context(GF_MPD *mpd, GF_XMLNode *root)
{
	u32 i;
	GF_DASH_SegmenterContext *dasher;
	GF_XMLAttribute *att;
	GF_SAFEALLOC(dasher, GF_DASH_SegmenterContext);
	if (!dasher) return NULL;

	i = 0;
	while ( (att = gf_list_enum(root->attributes, &i)) ) {
		if (!strcmp(att->name, "done")) dasher->done = gf_mpd_parse_bool(att->value);
		else if (!strcmp(att->name, "init")) dasher->init_seg = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "template")) dasher->template_seg = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "index")) dasher->template_idx = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "url")) dasher->src_url = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "periodID")) dasher->period_id = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "segNumber")) dasher->seg_number = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "lastPacketIdx")) dasher->last_pck_idx = gf_mpd_parse_long_int(att->value);
		else if (!strcmp(att->name, "pidID")) dasher->pid_id = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "depID")) dasher->dep_pid_id = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "periodStart")) gf_parse_lfrac(att->value, &dasher->period_start);
		else if (!strcmp(att->name, "periodDuration")) gf_parse_lfrac(att->value, &dasher->period_duration);
		else if (!strcmp(att->name, "ownsSet")) dasher->owns_set = gf_mpd_parse_bool(att->value);
		else if (!strcmp(att->name, "multiPIDInit")) dasher->multi_pids = gf_mpd_parse_bool(att->value);
		else if (!strcmp(att->name, "dashDuration")) dasher->dash_dur = gf_mpd_parse_fraction(att->value);
		else if (!strcmp(att->name, "nextSegmentStart")) dasher->next_seg_start = gf_mpd_parse_long_int(att->value);
		else if (!strcmp(att->name, "firstCTS")) dasher->first_cts = gf_mpd_parse_long_int(att->value);
		else if (!strcmp(att->name, "firstDTS")) dasher->first_cts = gf_mpd_parse_long_int(att->value);
		else if (!strcmp(att->name, "estimatedNextDTS")) dasher->est_next_dts = gf_mpd_parse_long_int(att->value);
		else if (!strcmp(att->name, "nbRepeat")) dasher->nb_repeat = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "tsOffset")) dasher->ts_offset = gf_mpd_parse_long_int(att->value);
		else if (!strcmp(att->name, "mpdTimescale")) dasher->mpd_timescale = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "sourcePID")) dasher->source_pid = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "cumulatedDur")) dasher->cumulated_dur = gf_mpd_parse_double(att->value);
		else if (!strcmp(att->name, "cumulatedSubdur")) dasher->cumulated_subdur = gf_mpd_parse_double(att->value);
		else if (!strcmp(att->name, "muxPIDs")) dasher->mux_pids = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "segsPurged")) dasher->segs_purged = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "durPurged")) dasher->dur_purged = gf_mpd_parse_double(att->value);
		else if (!strcmp(att->name, "moofSN")) dasher->moof_sn = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "moofInc")) dasher->moof_sn_inc = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "lastDynPeriodID")) dasher->last_dyn_period_id = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "subdurForced")) dasher->subdur_forced = gf_mpd_parse_bool(att->value);
	}
	return dasher;
}

static GF_List *gf_mpd_parse_segments_context(GF_MPD *mpd, GF_XMLNode *root)
{
	GF_List *res = NULL;
	u32 i, j;
	GF_XMLAttribute *att;
	GF_XMLNode *child;
	i=0;
	while ((child = gf_list_enum(root->content, &i))) {
		GF_DASH_SegmentContext *sctx;
		if (!gf_mpd_valid_child(mpd, child)) continue;

		if (strcmp(child->name, "segmentInfo")) continue;
		if (!res) res = gf_list_new();

		GF_SAFEALLOC(sctx, GF_DASH_SegmentContext);
		if (!sctx) break;

		gf_list_add(res, sctx);

		j = 0;
		while ( (att = gf_list_enum(child->attributes, &j)) ) {
			if (!strcmp(att->name, "file")) sctx->filename = gf_mpd_parse_string(att->value);
			if (!strcmp(att->name, "path")) sctx->filepath = gf_mpd_parse_string(att->value);
			else if (!strcmp(att->name, "time")) sctx->time = gf_mpd_parse_long_int(att->value);
			else if (!strcmp(att->name, "dur")) sctx->dur = gf_mpd_parse_long_int(att->value);
			else if (!strcmp(att->name, "size")) sctx->file_size = gf_mpd_parse_int(att->value);
			else if (!strcmp(att->name, "offset")) sctx->file_offset = gf_mpd_parse_long_int(att->value);
			else if (!strcmp(att->name, "idx_size")) sctx->index_size = gf_mpd_parse_int(att->value);
			else if (!strcmp(att->name, "idx_offset")) sctx->index_offset = gf_mpd_parse_long_int(att->value);
			else if (!strcmp(att->name, "seg_num")) sctx->seg_num = gf_mpd_parse_int(att->value);

		}
	}
	return res;
}

static GF_Err gf_mpd_parse_representation(GF_MPD *mpd, GF_List *container, GF_XMLNode *root)
{
	u32 i, child_idx;
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
		else gf_mpd_parse_common_representation_attr(mpd, (GF_MPD_CommonAttributes*)rep, root, att, &i);
	}

	i = 0;
	child_idx = 0;
	while ( (child = gf_list_enum(root->content, &i))) {
		if (!gf_mpd_valid_child(mpd, child))
			continue;
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
		/*TODO
		else if (!strcmp(child->name, "SubRepresentation")) {
						e = gf_mpd_parse_subrepresentation(rep->sub_representations, child);
						if (e) return e;
		}
		*/
		else if (!strcmp(child->name, "dasher")) {
			assert(!rep->dasher_ctx);
			rep->dasher_ctx = gf_mpd_parse_dasher_context(mpd, child);
		}
		else if (!strcmp(child->name, "segments")) {
			assert(!rep->state_seg_list);
			rep->state_seg_list = gf_mpd_parse_segments_context(mpd, child);
		} else {
			gf_mpd_parse_common_representation_child(mpd, (GF_MPD_CommonAttributes*)rep, root, child, &i, child_idx);
		}
		child_idx++;
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
	GF_SAFEALLOC(set->par, GF_MPD_Fractional);
	/*assign default ID and group*/
	set->group = -1;
	set->id = -1;
	return set;
}

static GF_Err gf_mpd_parse_adaptation_set(GF_MPD *mpd, GF_List *container, GF_XMLNode *root)
{
	u32 i, child_idx;
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
			set->par = gf_mpd_parse_frac(att->value, ':', NULL);
		}
		else if (!strcmp(att->name, "minBandwidth")) set->min_bandwidth = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "maxBandwidth")) set->max_bandwidth = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "minWidth")) set->min_width = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "maxWidth")) set->max_width = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "minHeight")) set->min_height = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "maxHeight")) set->max_height = gf_mpd_parse_int(att->value);
		else if (!strcmp(att->name, "minFrameRate")) gf_mpd_parse_frac(att->value, '/', &set->min_framerate);
		else if (!strcmp(att->name, "maxFrameRate")) gf_mpd_parse_frac(att->value, '/', &set->max_framerate);
		else if (!strcmp(att->name, "segmentAlignment")) set->segment_alignment = gf_mpd_parse_bool(att->value);
		else if (!strcmp(att->name, "bitstreamSwitching")) set->bitstream_switching = gf_mpd_parse_bool(att->value);
		else if (!strcmp(att->name, "subsegmentAlignment")) set->subsegment_alignment = gf_mpd_parse_bool(att->value);
		else if (!strcmp(att->name, "subsegmentStartsWithSAP")) {
			if (!strcmp(att->value, "false")) set->subsegment_starts_with_sap  = 0;
			else set->subsegment_starts_with_sap = gf_mpd_parse_int(att->value);
		}
		else gf_mpd_parse_common_representation_attr(mpd, (GF_MPD_CommonAttributes*)set, root, att, &i);
	}

	child_idx = 0;
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
		} else {
			gf_mpd_parse_common_representation_child(mpd, (GF_MPD_CommonAttributes*)set, root, child, &i, child_idx);
		}
		child_idx++;
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
	return period;
}

GF_Err gf_mpd_parse_period(GF_MPD *mpd, GF_XMLNode *root)
{
	u32 i, child_idx;
	GF_MPD_Period *period;
	GF_XMLAttribute *att;
	GF_XMLNode *child;
	GF_Err e;

	Bool is_preperiod = !strcmp(root->name, "PrePeriod") ? GF_TRUE : GF_FALSE;

	if (is_preperiod) {
		period = gf_list_get(mpd->periods, 0);
		if (period) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD] At most one PrePeriod allowed, and it shall be the first period\n"));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
	}

	period = gf_mpd_period_new();
	if (!period) return GF_OUT_OF_MEM;
	period->is_preroll = is_preperiod;

	e = gf_list_add(mpd->periods, period);
	if (e) return e;

	i = 0;
	while ( (att = gf_list_enum(root->attributes, &i)) ) {
		if (strstr(att->name, "href")) period->xlink_href = gf_mpd_parse_string(att->value);
		else if (strstr(att->name, "actuate")) period->xlink_actuate_on_load = !strcmp(att->value, "onLoad") ? 1 : 0;
		else if (!strcmp(att->name, "id")) period->ID = gf_mpd_parse_string(att->value);
		else if (!strcmp(att->name, "start")) {
			if (is_preperiod) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD] PrePeriod with @start not allowed, ignoring attribute\n"));
			} else {
				period->start = gf_mpd_parse_duration(att->value);
			}
		}
		else if (!strcmp(att->name, "duration")) period->duration = gf_mpd_parse_duration(att->value);
		else if (!strcmp(att->name, "bitstreamSwitching")) period->bitstream_switching = gf_mpd_parse_bool(att->value);
		else {
			MPD_STORE_EXTENSION_ATTR(period)
		}
	}

	child_idx = 0;
	i = 0;
	while ( (child = gf_list_enum(root->content, &i))) {
		if (!gf_mpd_valid_child(mpd, child))
			continue;
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
		else {
			MPD_STORE_EXTENSION_NODE(period)
		}
		child_idx++;
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

void gf_mpd_del_list(GF_List *list, void (*__destructor)(void *), Bool reset_only)
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
	if (base_url->redirection) gf_free(base_url->redirection);
	gf_free(base_url);
}

void gf_mpd_url_free(void *_item)
{
	GF_MPD_URL *ptr = (GF_MPD_URL*)_item;
	if (ptr->sourceURL) gf_free(ptr->sourceURL);
	if (ptr->byte_range) gf_free(ptr->byte_range);
	gf_free(ptr);
}
void gf_mpd_string_free(void *_item)
{
	if (_item) gf_free(_item);
}

void gf_mpd_prog_info_free(void *_item)
{
	GF_MPD_ProgramInfo *ptr = (GF_MPD_ProgramInfo *)_item;
	if (ptr->lang) gf_free(ptr->lang);
	if (ptr->title) gf_free(ptr->title);
	if (ptr->source) gf_free(ptr->source);
	if (ptr->copyright) gf_free(ptr->copyright);
	if (ptr->more_info_url) gf_free(ptr->more_info_url);
	MPD_FREE_EXTENSION_NODE(ptr);
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

#if 0 //unused
void gf_mpd_segment_url_list_free(GF_List *list)
{
	gf_mpd_del_list(list, gf_mpd_segment_url_free, 0);
}
#endif

void gf_mpd_segment_list_free(void *_item)
{
	GF_MPD_SegmentList *ptr = (GF_MPD_SegmentList *)_item;
	if (ptr->xlink_href) gf_free(ptr->xlink_href);
	if (ptr->initialization_segment) gf_mpd_url_free(ptr->initialization_segment);
	if (ptr->bitstream_switching_url) gf_mpd_url_free(ptr->bitstream_switching_url);
	if (ptr->representation_index) gf_mpd_url_free(ptr->representation_index);
	if (ptr->segment_timeline) gf_mpd_segment_timeline_free(ptr->segment_timeline);
	gf_mpd_del_list(ptr->segment_URLs, gf_mpd_segment_url_free, 0);
	if (ptr->dasher_segment_name) gf_free(ptr->dasher_segment_name);
	if (ptr->previous_xlink_href) gf_free(ptr->previous_xlink_href);
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


GF_MPD_Descriptor *gf_mpd_descriptor_new(const char *id, const char *schemeIdUri, const char *value) {
	GF_MPD_Descriptor *mpd_desc;
	GF_SAFEALLOC(mpd_desc, GF_MPD_Descriptor);
	if (!mpd_desc) return NULL;
	if (id) mpd_desc->id = gf_strdup(id);
	if (schemeIdUri) mpd_desc->scheme_id_uri = gf_strdup(schemeIdUri);
	if (value) mpd_desc->value = gf_strdup(value);
	return mpd_desc;
}

void gf_mpd_descriptor_free(void *item)
{
	GF_MPD_Descriptor *mpd_desc = (GF_MPD_Descriptor*) item;
	if (mpd_desc->id) gf_free(mpd_desc->id);
	if (mpd_desc->scheme_id_uri) gf_free(mpd_desc->scheme_id_uri);
	if (mpd_desc->value) gf_free(mpd_desc->value);
	MPD_FREE_EXTENSION_NODE(mpd_desc);

	gf_free(mpd_desc);
}

void gf_mpd_content_component_free(void *item)
{
	GF_MPD_ContentComponent *component_descriptor=(GF_MPD_ContentComponent*) item;
	if (component_descriptor->type) gf_free(component_descriptor->type);
	if (component_descriptor->lang) gf_free(component_descriptor->lang);
	gf_free(item);
}

void gf_mpd_producer_reftime_free(void *item)
{
	GF_MPD_ProducerReferenceTime *pref=(GF_MPD_ProducerReferenceTime*) item;
	if (pref->scheme) gf_free(pref->scheme);
	if (pref->wallclock) gf_free(pref->wallclock);
	if (pref->utc_timing) gf_mpd_descriptor_free(pref->utc_timing);
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
	gf_mpd_del_list(ptr->producer_reference_time, gf_mpd_producer_reftime_free, 0);
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
	if (ptr->playback.init_segment.data) gf_free(ptr->playback.init_segment.data);
	if (ptr->playback.key_url) gf_free(ptr->playback.key_url);

	gf_mpd_del_list(ptr->base_URLs, gf_mpd_base_url_free, 0);
	gf_mpd_del_list(ptr->sub_representations, NULL/*TODO*/, 0);
	if (ptr->segment_base) gf_mpd_segment_base_free(ptr->segment_base);
	if (ptr->segment_list) gf_mpd_segment_list_free(ptr->segment_list);
	if (ptr->segment_template) gf_mpd_segment_template_free(ptr->segment_template);
	MPD_FREE_EXTENSION_NODE(ptr);

	if (ptr->dasher_ctx) {
		gf_free(ptr->dasher_ctx->init_seg);
		if (ptr->dasher_ctx->period_id)
			gf_free(ptr->dasher_ctx->period_id);
		gf_free(ptr->dasher_ctx->src_url);
		gf_free(ptr->dasher_ctx->template_seg);
		if (ptr->dasher_ctx->template_idx) gf_free(ptr->dasher_ctx->template_idx);
		if (ptr->dasher_ctx->mux_pids) gf_free(ptr->dasher_ctx->mux_pids);
		gf_free(ptr->dasher_ctx);
	}
	if (ptr->state_seg_list) {
		while (gf_list_count(ptr->state_seg_list)) {
			GF_DASH_SegmentContext *s = gf_list_pop_back(ptr->state_seg_list);
			if (s->filename) gf_free(s->filename);
			if (s->filepath) gf_free(s->filepath);
			if (s->frags) gf_free(s->frags);
			if (s->hls_key_uri) gf_free(s->hls_key_uri);
			gf_free(s);
		}
		gf_list_del(ptr->state_seg_list);
	}
	if (ptr->m3u8_var_name) gf_free(ptr->m3u8_var_name);
	if (ptr->m3u8_var_file) gf_fclose(ptr->m3u8_var_file);

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
	MPD_FREE_EXTENSION_NODE(ptr);
	gf_free(ptr);
}

void gf_mpd_period_free(void *_item)
{
	GF_MPD_Period *ptr = (GF_MPD_Period *)_item;
	if (ptr->ID) gf_free(ptr->ID);
	if (ptr->origin_base_url) gf_free(ptr->origin_base_url);
	if (ptr->broken_xlink) gf_free(ptr->broken_xlink);
	if (ptr->xlink_href) gf_free(ptr->xlink_href);
	if (ptr->segment_base) gf_mpd_segment_base_free(ptr->segment_base);
	if (ptr->segment_list) gf_mpd_segment_list_free(ptr->segment_list);
	if (ptr->segment_template) gf_mpd_segment_template_free(ptr->segment_template);

	gf_mpd_del_list(ptr->base_URLs, gf_mpd_base_url_free, 0);
	gf_mpd_del_list(ptr->adaptation_sets, gf_mpd_adaptation_set_free, 0);
	MPD_FREE_EXTENSION_NODE(ptr);
	gf_mpd_del_list(ptr->subsets, NULL/*TODO*/, 0);
	gf_free(ptr);
}

GF_EXPORT
void gf_mpd_del(GF_MPD *mpd)
{
	if (!mpd) return;
#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_cov_mode()) {
		gf_mpd_string_free(NULL);
	}
#endif

	gf_mpd_del_list(mpd->program_infos, gf_mpd_prog_info_free, 0);
	gf_mpd_del_list(mpd->base_URLs, gf_mpd_base_url_free, 0);
	gf_mpd_del_list(mpd->locations, gf_mpd_string_free, 0);
	gf_mpd_del_list(mpd->metrics, NULL/*TODO*/, 0);
	gf_mpd_del_list(mpd->periods, gf_mpd_period_free, 0);
	if (mpd->profiles) gf_free(mpd->profiles);
	if (mpd->ID) gf_free(mpd->ID);
	gf_mpd_del_list(mpd->utc_timings, gf_mpd_descriptor_free, 0);
	gf_mpd_del_list(mpd->essential_properties, gf_mpd_descriptor_free, 0);
	gf_mpd_del_list(mpd->supplemental_properties, gf_mpd_descriptor_free, 0);
	MPD_FREE_EXTENSION_NODE(mpd);
	gf_free(mpd);
}


GF_EXPORT
GF_Err gf_mpd_complete_from_dom(GF_XMLNode *root, GF_MPD *mpd, const char *default_base_url)
{
	GF_Err e;
	u32 i, child_idx;
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

	if (!strcmp(root->name, "PrePeriod") || !strcmp(root->name, "Period")) {
		return gf_mpd_parse_period(mpd, root);
	}

	i = 0;
	while ((att = gf_list_enum(root->attributes, &i))) {
		if (!strcmp(att->name, "id")) {
			if (mpd->ID) gf_free(mpd->ID);
			mpd->ID = gf_mpd_parse_string(att->value);
		} else if (!strcmp(att->name, "profiles")) {
			if (mpd->profiles) gf_free(mpd->profiles);
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
		} else if (!strcmp(att->name, "gpac:init_gen_time")) {
			mpd->gpac_init_ntp_ms = gf_mpd_parse_long_int(att->value);
		} else if (!strcmp(att->name, "gpac:next_gen_time")) {
			mpd->gpac_next_ntp_ms = gf_mpd_parse_long_int(att->value);
		} else if (!strcmp(att->name, "gpac:mpd_time")) {
			mpd->gpac_mpd_time = gf_mpd_parse_long_int(att->value);
		} else {
			MPD_STORE_EXTENSION_ATTR(mpd)
		}
	}
	if (mpd->type == GF_MPD_TYPE_STATIC)
		mpd->minimum_update_period = mpd->time_shift_buffer_depth = 0;

	child_idx = 0;
	i = 0;
	while ( ( child = gf_list_enum(root->content, &i )) ) {
		if (! gf_mpd_valid_child(mpd, child))
			continue;

		if (!strcmp(child->name, "ProgramInformation")) {
			e = gf_mpd_parse_program_info(mpd, child);
			if (e) return e;
		} else if (!strcmp(child->name, "Location")) {
			char *str = gf_mpd_parse_text_content(child);
			if (str) gf_list_add(mpd->locations, str);
		} else if (!strcmp(child->name, "PrePeriod") || !strcmp(child->name, "Period")) {
			e = gf_mpd_parse_period(mpd, child);
			if (e) return e;
		} else if (!strcmp(child->name, "Metrics")) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD] Metrics not implemented yet\n"));
		} else if (!strcmp(child->name, "BaseURL")) {
			e = gf_mpd_parse_base_url(mpd->base_URLs, child);
			if (e) return e;
		} else if (!strcmp(child->name, "UTCTiming")) {
			gf_mpd_parse_descriptor(mpd->utc_timings, child);
		} else if (!strcmp(child->name, "EssentialProperty")) {
			gf_mpd_parse_descriptor(mpd->essential_properties, child);
		}
		else if (!strcmp(child->name, "SupplementalProperty")) {
			gf_mpd_parse_descriptor(mpd->supplemental_properties, child);
		} else {
			MPD_STORE_EXTENSION_NODE(mpd)
		}
		child_idx++;
	}

	return GF_OK;
}

static void gf_mpd_init_struct(GF_MPD *mpd)
{
	assert(!mpd->periods);
	mpd->periods = gf_list_new();
	mpd->program_infos = gf_list_new();
	mpd->base_URLs = gf_list_new();
	mpd->locations = gf_list_new();
	mpd->metrics = gf_list_new();
	mpd->utc_timings = gf_list_new();
	mpd->essential_properties = gf_list_new();
	mpd->supplemental_properties = gf_list_new();
}

GF_EXPORT
GF_Err gf_mpd_init_from_dom(GF_XMLNode *root, GF_MPD *mpd, const char *default_base_url)
{
	if (!root || !mpd) return GF_BAD_PARAM;

	gf_mpd_init_struct(mpd);

	/*setup some defaults*/
	mpd->type = GF_MPD_TYPE_STATIC;
	mpd->time_shift_buffer_depth = (u32) -1; /*infinite by default*/
	mpd->xml_namespace = NULL;

	return gf_mpd_complete_from_dom(root, mpd, default_base_url);
}

static GF_Err gf_m3u8_fill_mpd_struct(MasterPlaylist *pl, const char *m3u8_file, const char *src_base_url, const char *mpd_file, char *title, Double update_interval, char *mimeTypeForM3U8Segments, Bool do_import, Bool use_mpd_templates, Bool use_segment_timeline, Bool is_end, u32 max_dur, GF_MPD *mpd, Bool parse_sub_playlist)
{
	char *sep, *template_base=NULL, *template_ext;
	u32 nb_streams, i, j, k, template_width, template_idx_start;
	Stream *stream;
	PlaylistElement *pe, *elt;
	GF_MPD_ProgramInfo *info;
	GF_MPD_Period *period;
	GF_Err e;
	if (!parse_sub_playlist) use_mpd_templates = GF_FALSE;
	Bool all_template_used = use_mpd_templates;
	char str[1024];

	if (!mpd) return GF_BAD_PARAM;
	gf_mpd_init_struct(mpd);

	mpd->time_shift_buffer_depth = (u32) -1; /*infinite by default*/
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
	sprintf(str, "Generated from URL %s", gf_file_basename(src_base_url));
	info->source = gf_strdup(str);
	if (!gf_sys_is_test_mode())
		sprintf(str, "Generated by GPAC %s", gf_gpac_version());
	else
		sprintf(str, "Generated by GPAC");
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
		set->id = -1;
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
							GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[MPD] Cannot remap M3U8 to segment template MPD, using segment list\n"));
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
			if (pe)
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
			char *variant_base_url=NULL;
			u32 count_elements;
			char szName[20];
#ifndef GPAC_DISABLE_MEDIA_IMPORT
			Bool import_file = do_import;
#endif
			char *byte_range_media_file = NULL;
			GF_MPD_Representation *rep;
			pe = gf_list_get(stream->variants, j);

			if (pe->element_type == TYPE_MEDIA) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[M3U8] NOT SUPPORTED: M3U8 Media\n"));
			} else if (pe->load_error) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[M3U8] Error loading playlist element %s\n", pe->url));
			} else if (pe->element_type != TYPE_PLAYLIST) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[M3U8] NOT SUPPORTED: M3U8 unknown type for %s\n", pe->url));
			}

			count_elements = gf_list_count(pe->element.playlist.elements);
			if (parse_sub_playlist && !count_elements)
				continue;

			if (pe->codecs && (pe->codecs[0] == '\"')) {
				u32 len = (u32) strlen(pe->codecs);
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

			//get base URL of child  manifest, only needed when injecting base URL
			variant_base_url = gf_strdup(pe->url);

			sep = strrchr(variant_base_url, '/');
			if (!sep) sep = strrchr(variant_base_url, '\\');
			/*keep final '/' */
			if (sep) {
				sep[1] = 0;
				/* base_url is same as manifest dir, no add of MPD BaseURL */
				if (!strcmp(variant_base_url, "./") || !strcmp(variant_base_url, ".\\")) {
					gf_free(variant_base_url);
					variant_base_url = NULL;
				}
			}
			/* base_url is just a filename so is same as manifest dir, no add of MPD BaseURL */
			else {
				gf_free(variant_base_url);
				variant_base_url = NULL;
			}

			width = pe->width;
			height = pe->height;
			samplerate = num_channels = 0;

retry_import:

#ifndef GPAC_DISABLE_MEDIA_IMPORT
			if (elt && import_file) {
				GF_MediaImporter import;
				char *elt_url = elt->init_segment_url ? elt->init_segment_url : elt->url;
				u64 br_start, br_end;
				char *tmp_file = NULL;

				br_start = elt->init_segment_url ? elt->init_byte_range_start : elt->byte_range_start;
				br_end = elt->init_segment_url ? elt->init_byte_range_end : elt->byte_range_end;

				char *par_url = gf_url_concatenate(src_base_url, pe->url);
				elt_url = gf_url_concatenate(par_url, elt_url);
				gf_free(par_url);

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

				if (!strstr(elt_url, "://") && !gf_file_exists(elt_url)) {
					import_file = GF_FALSE;
					if (elt_url) gf_free(elt_url);
					goto retry_import;
				}
				e = gf_media_import(&import);

				if (e != GF_OK) {
					k++;
					if (elt_url) gf_free(elt_url);
					goto try_next_segment;
				}

				if (import.in_name && !pe->bandwidth && !elt->init_segment_url && pe->duration_info) {
					u64 pos = 0;

					Double bw;
					FILE *t = gf_fopen(import.in_name, "rb");
					if (t) {
						pos = gf_fsize(t);
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
					gf_file_delete(tmp_file);

				if (!pe->codecs) {
					char *codecs = NULL;
					for (k=0; k<import.nb_tracks; k++) {
						if (strlen(import.tk_info[k].szCodecProfile)) {
							gf_dynstrcat(&codecs, import.tk_info[k].szCodecProfile, ",");
						}
					}
					pe->codecs = codecs;
				}
				for (k=0; k<import.nb_tracks; k++) {
					switch (import.tk_info[k].stream_type) {
					case GF_STREAM_VISUAL:
						width = import.tk_info[k].video_info.width;
						height = import.tk_info[k].video_info.height;
						break;
					case GF_STREAM_AUDIO:
						samplerate = import.tk_info[k].audio_info.sample_rate;
						num_channels = import.tk_info[k].audio_info.nb_channels;
						break;
					}
				}
				if (elt_url) gf_free(elt_url);
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
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[M3U8] Unknown mime-type when converting from M3U8 HLS playlist, setting %s\n", mimeTypeForM3U8Segments));
			}
			if (elt && elt->init_segment_url && (strstr(elt->init_segment_url, ".mp4") || strstr(elt->init_segment_url, ".MP4")) ) {
				rep->mime_type = gf_strdup(samplerate ? "audio/mp4" : "video/mp4");
			} else {
				rep->mime_type = gf_strdup(mimeTypeForM3U8Segments);
			}
			if (pe->codecs) {
				rep->codecs = gf_strdup(pe->codecs);
			}
			if (pe->language && !set->lang) {
				set->lang = pe->language;
				pe->language = NULL;
			}
			if (width && height) {
				rep->width = width;
				rep->height = height;
			}
			if (elt) {
				if (elt->drm_method==DRM_AES_128)
					rep->crypto_type = 1;
				else if (elt->drm_method==DRM_CENC)
					rep->crypto_type = 2;
			}
			if (samplerate) {
				rep->samplerate = samplerate;
			}
			if (num_channels) {
				GF_MPD_Descriptor *desc;
				GF_SAFEALLOC(desc, GF_MPD_Descriptor);
				if (desc) {
					char szChan[10];
					desc->scheme_id_uri = gf_strdup("urn:mpeg:dash:23003:3:audio_channel_configuration:2011");
					sprintf(szChan, "%d", num_channels);
					desc->value = gf_strdup(szChan);
					if (!rep->audio_channels) rep->audio_channels = gf_list_new();
					gf_list_add(rep->audio_channels, desc);
				}
			}

			//if parsing subplaylist with a MPD, we translate HLS to MPD directly
			//compute relative url from source HLS to target MPD and inject it in base URL (SegmentTemplate or SegmentList)
			//otherwise, we use the element url as is, and wait for xlink resolution, which will be done
			//against the HLS base URL
			char *rel_url = NULL;
			if (parse_sub_playlist && mpd_file) {
				u32 base_len;
				//check we do not generate the MPD at the same location
				char *f1 = gf_file_basename(mpd_file);
				char *f2 = gf_file_basename(src_base_url);
				base_len = (u32) (f1 - mpd_file);
				if ((base_len == (u32) (f2 - src_base_url)) && !strncmp(src_base_url, mpd_file, base_len)) {
					rel_url = NULL;
				} else {
					rel_url = gf_url_concatenate_parent(mpd_file, src_base_url);
				}
			}


			if (use_template) {
				//inject baseURL pointing to source playlist
				if (rel_url || variant_base_url) {
					GF_MPD_BaseURL *url;
					GF_SAFEALLOC(url, GF_MPD_BaseURL);
					if (! url) return GF_OUT_OF_MEM;
					e = gf_list_add(rep->base_URLs, url);
					if (e) return GF_OUT_OF_MEM;

					if (rel_url) {
						if (variant_base_url)
							url->URL = gf_url_concatenate(rel_url, variant_base_url);
						else
							url->URL = gf_strdup(rel_url);
					} else {
						url->URL = gf_strdup(variant_base_url);
					}
				}

				if (elt->init_segment_url) {
					GF_SAFEALLOC(rep->segment_template, GF_MPD_SegmentTemplate);
					if (!rep->segment_template)  return GF_OUT_OF_MEM;
					rep->segment_template->start_number = (u32) -1;
					//no need to conctenate with rel_url since it is now in our MPD base url
					rep->segment_template->initialization = gf_strdup(elt->init_segment_url);
				}

				if (rel_url) gf_free(rel_url);
				if (variant_base_url) gf_free(variant_base_url);
				continue;
			}

			byte_range_media_file = NULL;
			elt = gf_list_get(pe->element.playlist.elements, 0);

			if (parse_sub_playlist && elt && (elt->byte_range_end || elt->byte_range_start)) {
				byte_range_media_file = elt->url;
			}

			//only if we parse subplaylist now (otherwise baseURL added when resolving xlink)
			if (parse_sub_playlist && elt && (
				//if byte-range media file, add base URL
				byte_range_media_file
				//otherwise if we have different location for HLS vs MPD url or if child playlist is not in same path as master HLS
				|| (rel_url || strchr(elt->url, '/') || strchr(elt->url, '\\'))
			)) {
				GF_MPD_BaseURL *url;
				GF_SAFEALLOC(url, GF_MPD_BaseURL);
				if (! url) {
					if (rel_url) gf_free(rel_url);
					if (variant_base_url) gf_free(variant_base_url);
					return GF_OUT_OF_MEM;
				}
				e = gf_list_add(rep->base_URLs, url);
				if (e) {
					if (rel_url) gf_free(rel_url);
					if (variant_base_url) gf_free(variant_base_url);
					return GF_OUT_OF_MEM;
				}
				if (rel_url) {
					if (variant_base_url) {
						char *rel_base = gf_url_concatenate(rel_url, variant_base_url);
						url->URL = gf_url_concatenate(rel_base, elt->url);
						gf_free(rel_base);
					} else {
						url->URL = gf_strdup(rel_url);
					}
				}
				else
					url->URL = gf_strdup(elt->url);
			}
			if (rel_url) gf_free(rel_url);
			//no longer needed
			if (variant_base_url) gf_free(variant_base_url);

			GF_SAFEALLOC(rep->segment_list, GF_MPD_SegmentList);
			if (!rep->segment_list) return GF_OUT_OF_MEM;

			//Not parsing sub-playlists, save URL to these sub-playlist in xlink:href
			//resolution will be done against the HLS base URL
			//child playlist will be loaded if active and not modified since last load
			if (!parse_sub_playlist) {
				rep->segment_list->xlink_href = pe->url;
				pe->url=NULL;
				if (template_base) {
					gf_free(template_base);
					template_base = NULL;
				}
				continue;
			}
			rep->segment_list->segment_URLs = gf_list_new();
			rep->segment_list->duration = (u64) (pe->duration_info * 1000);
			rep->segment_list->timescale = 1000;
			if (elt && elt->init_segment_url) {
				GF_SAFEALLOC(rep->segment_list->initialization_segment, GF_MPD_URL);
				if (!rep->segment_list->initialization_segment) return GF_OUT_OF_MEM;

				rep->segment_list->initialization_segment->sourceURL = gf_strdup(elt->init_segment_url);

				if (elt->init_byte_range_end) {
					GF_SAFEALLOC(rep->segment_list->initialization_segment->byte_range, GF_MPD_ByteRange);
					if (!rep->segment_list->initialization_segment->byte_range) return GF_OUT_OF_MEM;

					rep->segment_list->initialization_segment->byte_range->start_range = elt->init_byte_range_start;
					rep->segment_list->initialization_segment->byte_range->end_range = elt->init_byte_range_end;
				}
			}


			Double avg_dur = 0;
			Double cur_start=0;
			Bool do_seg_timeline = use_segment_timeline;

			for (k=0; k<count_elements; k++) {
				Double diff, seg_start;
				GF_MPD_SegmentURL *segment_url;
				elt = gf_list_get(pe->element.playlist.elements, k);

				if (!avg_dur) avg_dur = elt->duration_info;
				else if (elt->duration_info) {
					diff = elt->duration_info - avg_dur;
					if (diff<0) diff = -diff;
					if (diff > avg_dur/2) {
						do_seg_timeline = GF_TRUE;
					}
				}
				seg_start = ((Double)k*rep->segment_list->duration) / rep->segment_list->timescale;
				if (k) {
					diff = cur_start - seg_start;
					if (diff<0) diff = -diff;
					if (diff > avg_dur/2) {
						do_seg_timeline = GF_TRUE;
					}
				}

				cur_start += elt->duration_info;

				GF_SAFEALLOC(segment_url, GF_MPD_SegmentURL);
				if (!segment_url) return GF_OUT_OF_MEM;
				gf_list_add(rep->segment_list->segment_URLs, segment_url);
				if (byte_range_media_file) {
					GF_SAFEALLOC(segment_url->media_range, GF_MPD_ByteRange);
					if (!segment_url->media_range) return GF_OUT_OF_MEM;
					segment_url->media_range->start_range = elt->byte_range_start;
					segment_url->media_range->end_range = elt->byte_range_end;
					if (strcmp(elt->url, byte_range_media_file)) {
						segment_url->media = elt->url;
						elt->url=NULL;
					}
				} else {
					segment_url->media = elt->url;
					elt->url=NULL;
				}
				//only signal duration if different from default one
				segment_url->duration = (u64) (rep->segment_list->timescale * elt->duration_info);
				if (segment_url->duration == rep->segment_list->duration)
					segment_url->duration = 0;
					
				if (elt->drm_method != DRM_NONE) {
					//segment_url->key_url = "aes-128";
					if (elt->key_uri) {
						segment_url->key_url = elt->key_uri;
						elt->key_uri=NULL;
						memcpy(segment_url->key_iv, elt->key_iv, sizeof(bin128));
					}
				}
			}
			if (do_seg_timeline) {
				u64 start_time = 0;
				GF_SAFEALLOC(rep->segment_list->segment_timeline, GF_MPD_SegmentTimeline);
				if (!rep->segment_list->segment_timeline) return GF_OUT_OF_MEM;

				rep->segment_list->segment_timeline->entries = gf_list_new();
				for (k=0; k<count_elements; k++) {
					u64 dur;
					GF_MPD_SegmentTimelineEntry *se;
					elt = gf_list_get(pe->element.playlist.elements, k);
					GF_SAFEALLOC(se, GF_MPD_SegmentTimelineEntry);
					if (!se) return GF_OUT_OF_MEM;

					dur = (u64) ( elt->duration_info * rep->segment_list->timescale);
					se->duration = (u32) dur;
					se->start_time = start_time;
					start_time += dur;
					gf_list_add(rep->segment_list->segment_timeline->entries, se);
				}
			}
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
                      u32 reload_count, char *mimeTypeForM3U8Segments, Bool do_import, Bool use_mpd_templates, Bool use_segment_timeline, GF_FileDownload *getter,
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
		if (!keep_files) gf_file_delete(m3u8_file);
		mpd_file = m3u8_file;
	}

	mpd->xml_namespace = NULL;
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

			if (!parse_sub_playlist)
				continue;

			if (strcmp(base_url, pe->url))
				suburl = gf_url_concatenate(base_url, pe->url);

			if (!suburl || !strcmp(base_url, suburl)) {
				if (suburl)
					gf_free(suburl);
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[M3U8] Not downloading, programs are identical for %s...\n", pe->url));
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
					pe->load_error = gf_m3u8_parse_sub_playlist(getter->get_cache_name(getter), &pl, suburl, stream, pe, GF_FALSE);
				}
				//getter->del_session(getter);
			} else { /* for use in MP4Box */
				if (strstr(suburl, "://")) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[M3U8] Downloading %s...\n", suburl));
					e = gf_dm_wget(suburl, "tmp.m3u8", 0, 0, NULL);
					if (e == GF_OK) {
						e = gf_m3u8_parse_sub_playlist("tmp.m3u8", &pl, suburl, stream, pe, GF_FALSE);
					} else {
						GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[M3U8] Download failed for %s\n", suburl));
						e = GF_OK;
					}
					gf_file_delete("tmp.m3u8");
				} else {
					e = gf_m3u8_parse_sub_playlist(suburl, &pl, suburl, stream, pe, GF_FALSE);
				}
				if (e) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[M3U8] Failed to parse subplaylist %s\n", suburl));
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
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[M3U8] The M3U8 playlist is not correct.\n"));
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
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[M3U8] No need to refresh playlist!\n"));
		mpd->type = GF_MPD_TYPE_STATIC;
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[M3U8] Playlist will be refreshed every %g seconds, len=%d\n", update_interval, the_pe->duration_info));
		mpd->type = GF_MPD_TYPE_DYNAMIC;
	}

	title = the_pe->title;
	if (!title || strlen(title) < 2)
		title = the_pe->url;

	assert(mpd_file);
	assert(mpd);

	e = gf_m3u8_fill_mpd_struct(pl, m3u8_file, base_url, mpd_file, title, update_interval, mimeTypeForM3U8Segments, do_import, use_mpd_templates, use_segment_timeline, is_end,  max_dur, mpd, parse_sub_playlist);

	gf_m3u8_master_playlist_del(&pl);

	return e;
}

GF_EXPORT
GF_Err gf_m3u8_solve_representation_xlink(GF_MPD_Representation *rep, const char *base_url, GF_FileDownload *getter, Bool *is_static, u64 *duration, u8 last_sig[GF_SHA1_DIGEST_SIZE])
{
	GF_Err e;
	MasterPlaylist *pl = NULL;
	Stream *stream;
	PlaylistElement *pe;
	u32 k, count_elements;
	u32 seq_num;
	Bool has_full_seg_following = GF_FALSE;
	Bool can_merge_parts = GF_FALSE;
	Bool first_ll_part = GF_TRUE;
	u8 signature[GF_SHA1_DIGEST_SIZE];
	const char *loc_file = rep->segment_list->xlink_href;
	char *full_url;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[M3U8] Solving m3u8 variant playlist %s\n", rep->segment_list->xlink_href));

	if (!getter || !getter->new_session || !getter->del_session || !getter->get_cache_name) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[M3U8] FileDownloader not found\n"));
		return GF_BAD_PARAM;
	}

	//get absolute resource path
	full_url = gf_url_concatenate(base_url, rep->segment_list->xlink_href);

	if (!strncmp(loc_file, "gmem://", 7)) {
		u8 *m3u8_payload;
		u32 m3u8_size;
		e = gf_blob_get(loc_file, &m3u8_payload,  &m3u8_size, NULL);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH,("[M3U8] Cannot load m3u8 source blob %s\n", loc_file));
			gf_free(full_url);
			return e;
		}
		gf_blob_release(loc_file);

		gf_sha1_csum(m3u8_payload, m3u8_size, signature);
	} else  if (gf_url_is_local(full_url)) {
		loc_file = full_url;
		gf_sha1_file(loc_file, signature);
	} else {
		e = getter->new_session(getter, full_url);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[M3U8] Download failed for %s: %s\n", rep->segment_list->xlink_href, gf_error_to_string(e) ));
			gf_free(full_url);
			return e;
		}
		loc_file = getter->get_cache_name(getter);
		gf_sha1_file(loc_file, signature);
	}
	if (! memcmp(signature, last_sig, GF_SHA1_DIGEST_SIZE)) {
		gf_free(full_url);
		return GF_EOS;
	}

	e = gf_m3u8_parse_sub_playlist(loc_file, &pl, rep->segment_list->xlink_href, NULL, NULL, GF_FALSE);
	//no longer needed
	gf_free(full_url);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[M3U8] Failed to parse playlist %s\n", rep->segment_list->xlink_href));
		gf_m3u8_master_playlist_del(&pl);
		return e;
	}

	assert(pl);
	assert(pl->streams);

	if (!gf_list_count(pl->streams)) {
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[M3U8] Playlist %s still empty\n", rep->segment_list->xlink_href));
		gf_m3u8_master_playlist_del(&pl);
		return GF_IP_NETWORK_EMPTY;
	}

	assert(gf_list_count(pl->streams) == 1);


	memcpy(last_sig, signature, GF_SHA1_DIGEST_SIZE);

	if (is_static) {
		*is_static = pl->playlist_needs_refresh ? GF_FALSE : GF_TRUE;
	}

	stream = (Stream *)gf_list_get(pl->streams, 0);
	assert(gf_list_count(stream->variants) == 1);
	pe = (PlaylistElement *)gf_list_get(stream->variants, 0);

	if (duration) {
		*duration = (u32) (stream->computed_duration * 1000);
	}

	//create a base URL if not found, otherwise modify existing one, pointing to our unresolved base (xlink)
	//if xlink is relative, it wil still be resolved against manifest base url when solving segment templates
	//this allows using parent path (ie "../") in the master playlist
	GF_MPD_BaseURL *burl = gf_list_get(rep->base_URLs, 0);
	if (!burl) {
		GF_SAFEALLOC(burl, GF_MPD_BaseURL);
		gf_list_add(rep->base_URLs, burl);
	} else {
		gf_free(burl->URL);
	}
	burl->URL = gf_strdup(rep->segment_list->xlink_href);

	if (pe->init_segment_url) {
		if (!rep->segment_list->initialization_segment) {
			GF_SAFEALLOC(rep->segment_list->initialization_segment, GF_MPD_URL);
			if (!rep->segment_list->initialization_segment) return GF_OUT_OF_MEM;

			if (strstr(pe->init_segment_url, "mp4") || strstr(pe->init_segment_url, "MP4")) {
				if (rep->mime_type) gf_free(rep->mime_type);
				rep->mime_type = gf_strdup("video/mp4");
			}
			rep->segment_list->initialization_segment->sourceURL = pe->init_segment_url;
			pe->init_segment_url=NULL;

			if (pe->init_byte_range_end) {
				GF_SAFEALLOC(rep->segment_list->initialization_segment->byte_range, GF_MPD_ByteRange);
				if (!rep->segment_list->initialization_segment->byte_range) return GF_OUT_OF_MEM;

				 rep->segment_list->initialization_segment->byte_range->start_range = pe->init_byte_range_start;
				 rep->segment_list->initialization_segment->byte_range->end_range = pe->init_byte_range_end;
			}
		}
	}
	rep->starts_with_sap = pl->independent_segments ? 1: 3;
	if (pl->low_latency)
		rep->m3u8_low_latency = GF_TRUE;

	rep->segment_list->duration = (u64) (pe->duration_info * 1000);
	rep->segment_list->timescale = 1000;
	rep->m3u8_media_seq_min = pe->element.playlist.media_seq_min;
	rep->m3u8_media_seq_max = pe->element.playlist.media_seq_max;
	if (!rep->segment_list->segment_URLs)
		rep->segment_list->segment_URLs = gf_list_new();
	count_elements = gf_list_count(pe->element.playlist.elements);

	seq_num = pe->element.playlist.media_seq_min;
	seq_num += pe->element.playlist.discontinuity;

	u64 seg_utc = 0;
	for (k=0; k<count_elements; k++) {
		GF_MPD_SegmentURL *segment_url;
		PlaylistElement *elt = gf_list_get(pe->element.playlist.elements, k);
		if (!elt)
			continue;

		//NOTE: for GPAC now, we disable stream AAC to avoid the problem when switching quality. It should be improved later !
		if (strstr(elt->url, ".aac")) {
			rep->playback.disabled = GF_TRUE;
			return GF_OK;
		}
		if (elt->drm_method==DRM_AES_128)
			rep->crypto_type = 1;
		else if (elt->drm_method==DRM_CENC)
			rep->crypto_type = 2;

		if (elt->low_lat_chunk && !has_full_seg_following) {
			u32 j;
			u64 last_end = 0;
			if (elt->byte_range_end && first_ll_part) {
				last_end = elt->byte_range_end;
				can_merge_parts = GF_TRUE;
			}
			for (j=k+1; j<count_elements; j++) {
				PlaylistElement *next_elt = gf_list_get(pe->element.playlist.elements, j);
				if (next_elt->low_lat_chunk) {
					Bool match = GF_TRUE;
					if (!first_ll_part) continue;

					if (strcmp(elt->url, next_elt->url))
						match = GF_FALSE;
					else if (!elt->byte_range_end && next_elt->byte_range_end)
						match = GF_FALSE;
					else if (elt->byte_range_end && !next_elt->byte_range_end)
						match = GF_FALSE;
					else if (last_end + 1 != next_elt->byte_range_start)
						match = GF_FALSE;
					else
						last_end = next_elt->byte_range_end;

					if (!match)
						can_merge_parts = GF_FALSE;

					continue;
				}
				has_full_seg_following = GF_TRUE;
				if (strcmp(elt->url, next_elt->url))
					can_merge_parts = GF_FALSE;
				break;
			}
		}

		GF_SAFEALLOC(segment_url, GF_MPD_SegmentURL);
		if (!segment_url) {
			return GF_OUT_OF_MEM;
		}
		gf_list_add(rep->segment_list->segment_URLs, segment_url);


		segment_url->media = elt->url;
		elt->url = NULL;

		segment_url->duration = (u64) (rep->segment_list->timescale * elt->duration_info);

		if (elt->utc_start_time) {
			seg_utc = elt->utc_start_time;
		}

		segment_url->hls_utc_time = seg_utc;
		if (seg_utc)
			seg_utc += (u32) (elt->duration_info*1000);

		//we keep the same seq num for each part
		segment_url->hls_seq_num = seq_num;

		if (elt->low_lat_chunk) {
			segment_url->hls_ll_chunk_type = (elt->independent_chunk || first_ll_part) ? 2 : 1;
			segment_url->can_merge = can_merge_parts ? 1 : 0;
			segment_url->is_first_part = first_ll_part;
			rep->m3u8_low_latency = GF_TRUE;
			first_ll_part = GF_FALSE;
			if (segment_url->hls_ll_chunk_type==2)
				rep->m3u8_media_seq_indep_last = gf_list_count(rep->segment_list->segment_URLs) - 1;
		} else {
			first_ll_part = GF_TRUE;
			has_full_seg_following = GF_FALSE;
			can_merge_parts = GF_FALSE;
			rep->m3u8_media_seq_indep_last = gf_list_count(rep->segment_list->segment_URLs) - 1;
			seq_num++;
		}

		if (elt->drm_method != DRM_NONE) {
			if (elt->key_uri) {
				segment_url->key_url = elt->key_uri;
				elt->key_uri=NULL;
				memcpy(segment_url->key_iv, elt->key_iv, sizeof(bin128));
			}
		}
		if (elt->byte_range_end) {
			GF_SAFEALLOC(segment_url->media_range, GF_MPD_ByteRange);
			if (!segment_url->media_range) return GF_OUT_OF_MEM;
			segment_url->media_range->start_range = elt->byte_range_start;
			segment_url->media_range->end_range = elt->byte_range_end;
		}
	}

	if (!gf_list_count(rep->segment_list->segment_URLs)) {
		gf_list_del(rep->segment_list->segment_URLs);
		rep->segment_list->segment_URLs = NULL;
	}

	if (rep->segment_list->previous_xlink_href) gf_free(rep->segment_list->previous_xlink_href);
	rep->segment_list->previous_xlink_href = rep->segment_list->xlink_href;
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


static GFINLINE void gf_mpd_lf(FILE *out, s32 indent)
{
	if (indent>=0) gf_fprintf(out, "\n");
}
static GFINLINE void gf_mpd_nl(FILE *out, s32 indent)
{
	if (indent>=0) {
		u32 i=(u32)indent;
		while (i) {
			gf_fprintf(out, " ");
			i--;
		}
	}
}

/*time is given in ms*/
void gf_mpd_print_date(FILE *out, char *name, u64 time)
{
	time_t gtime;
	struct tm *t;
	u32 sec;
	u32 ms;
	gtime = time / 1000;
	sec = (u32)(time / 1000);
	ms = (u32)(time - ((u64)sec) * 1000);

	if (name) {
		gf_fprintf(out, " %s=\"", name);
	}
	t = gf_gmtime(&gtime);
	sec = t->tm_sec;
	//see issue #859, no clue how this happened...
	if (sec > 60)
		sec = 60;
	gf_fprintf(out, "%d-%02d-%02dT%02d:%02d:%02d.%03dZ", 1900 + t->tm_year, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, sec, ms);

	if (name) {
		gf_fprintf(out, "\"");
	}
}

void gf_mpd_print_duration(FILE *out, char *name, u64 duration_in_ms, Bool UseHoursAndMinutes)
{
	u32 h, m, s, ms;

	h = (u32) (duration_in_ms / 3600000);
	m = (u32) (duration_in_ms/ 60000) - h*60;
	s = (u32) (duration_in_ms/1000) - h*3600 - m*60;
	ms = (u32) (duration_in_ms) - h*3600*1000 - m*60*1000 - s*1000;

	gf_fprintf(out, " %s=\"PT", name);
	if(UseHoursAndMinutes)
		gf_fprintf(out, "%dH%dM", h, m);
	gf_fprintf(out, "%d", s);
	gf_fprintf(out, ".");
	gf_fprintf(out, "%03dS\"", ms);
}

static void gf_mpd_print_base_url(FILE *out, GF_MPD_BaseURL *base_URL, s32 indent)
{
	gf_mpd_nl(out, indent);
	gf_fprintf(out, "<BaseURL");
	if (base_URL->service_location)
		gf_xml_dump_string(out, " serviceLocation=\"", base_URL->service_location, "\"");
	if (base_URL->byte_range)
		gf_fprintf(out, " byteRange=\""LLD"-"LLD"\"", base_URL->byte_range->start_range, base_URL->byte_range->end_range);

	gf_xml_dump_string(out, ">", base_URL->URL, "</BaseURL>");
	gf_mpd_lf(out, indent);
}

static void gf_mpd_print_base_urls(FILE *out, GF_List *base_URLs, s32 indent)
{
	GF_MPD_BaseURL *url;
	u32 i;
	i=0;

	while ((url = (GF_MPD_BaseURL *)gf_list_enum(base_URLs, &i))) {
		gf_mpd_print_base_url(out, url, indent);
	}
}

static void gf_mpd_print_url(FILE *out, GF_MPD_URL *url, char *name, s32 indent)
{
	gf_mpd_nl(out, indent);
	gf_fprintf(out, "<%s", name);
	if (url->byte_range) gf_fprintf(out, " range=\""LLD"-"LLD"\"", url->byte_range->start_range, url->byte_range->end_range);
	if (url->sourceURL) gf_fprintf(out, " sourceURL=\"%s\"", url->sourceURL);
	gf_fprintf(out, "/>");
	gf_mpd_lf(out, indent);
}

static void gf_mpd_print_segment_base_attr(FILE *out, GF_MPD_SegmentBase *s)
{
	if (s->timescale) gf_fprintf(out, " timescale=\"%d\"", s->timescale);
	if (s->presentation_time_offset) gf_fprintf(out, " presentationTimeOffset=\""LLU"\"", s->presentation_time_offset);
	if (s->index_range && s->index_range->end_range) {
		if (s->index_range_exact) gf_fprintf(out, " indexRangeExact=\"true\"");
		if (s->index_range) gf_fprintf(out, " indexRange=\""LLD"-"LLD"\"", s->index_range->start_range, s->index_range->end_range);
	}
	if (s->availability_time_offset) gf_fprintf(out, " availabilityTimeOffset=\"%g\"", s->availability_time_offset);
	if ((s32) s->time_shift_buffer_depth > 0)
		gf_mpd_print_duration(out, "timeShiftBufferDepth", s->time_shift_buffer_depth, GF_TRUE);
}

static void gf_mpd_print_segment_base(FILE *out, GF_MPD_SegmentBase *s, s32 indent)
{
	gf_mpd_nl(out, indent);
	gf_fprintf(out, "<SegmentBase");
	gf_mpd_print_segment_base_attr(out, s);
	gf_fprintf(out, ">");
	gf_mpd_lf(out, indent);

	if (s->initialization_segment) gf_mpd_print_url(out, s->initialization_segment, "Initialization", indent+1);
	if (s->representation_index) gf_mpd_print_url(out, s->representation_index, "RepresentationIndex", indent+1);

	gf_mpd_nl(out, indent);
	gf_fprintf(out, "</SegmentBase>");
	gf_mpd_lf(out, indent);
}

static void gf_mpd_print_segment_timeline(FILE *out, GF_MPD_SegmentTimeline *tl, s32 indent)
{
	u32 i;
	u64 start_time=0;
	GF_MPD_SegmentTimelineEntry *se;

	gf_mpd_nl(out, indent);
	gf_fprintf(out, "<SegmentTimeline>");
	gf_mpd_lf(out, indent);

	i = 0;
	while ( (se = gf_list_enum(tl->entries, &i))) {
		gf_mpd_nl(out, indent+1);
		gf_fprintf(out, "<S");
		if (!start_time || (se->start_time != start_time)) {
			gf_fprintf(out, " t=\""LLD"\"", se->start_time);
			start_time = se->start_time;
		}
		start_time += (se->repeat_count+1) * se->duration;

		if (se->duration) gf_fprintf(out, " d=\"%d\"", se->duration);
		if (se->repeat_count) gf_fprintf(out, " r=\"%d\"", se->repeat_count);
		gf_fprintf(out, "/>");
		gf_mpd_lf(out, indent);
	}
	gf_mpd_nl(out, indent);
	gf_fprintf(out, "</SegmentTimeline>");
	gf_mpd_lf(out, indent);
}

GF_MPD_SegmentTimeline *gf_mpd_segmentimeline_new(void)
{
	GF_MPD_SegmentTimeline *seg_tl;
	GF_SAFEALLOC(seg_tl, GF_MPD_SegmentTimeline);
	if (seg_tl && !seg_tl->entries) seg_tl->entries=gf_list_new();
	return seg_tl;
}

static u32 gf_mpd_print_multiple_segment_base(FILE *out, GF_MPD_MultipleSegmentBase *ms, s32 indent, Bool close_if_no_child)
{
	gf_mpd_print_segment_base_attr(out, (GF_MPD_SegmentBase *)ms);

	if (ms->start_number != (u32) -1) gf_fprintf(out, " startNumber=\"%d\"", ms->start_number);
	if (ms->duration) gf_fprintf(out, " duration=\""LLD"\"", ms->duration);


	if (!ms->bitstream_switching_url && !ms->segment_timeline && !ms->initialization_segment && !ms->representation_index) {
		if (close_if_no_child) gf_fprintf(out, "/");
		gf_fprintf(out, ">");
		gf_mpd_lf(out, indent);
		return 1;
	}
	gf_fprintf(out, ">");
	gf_mpd_lf(out, indent);

	if (ms->initialization_segment) gf_mpd_print_url(out, ms->initialization_segment, "Initialization", indent+1);
	if (ms->representation_index) gf_mpd_print_url(out, ms->representation_index, "RepresentationIndex", indent+1);

	if (ms->segment_timeline) gf_mpd_print_segment_timeline(out, ms->segment_timeline, indent+1);
	if (ms->bitstream_switching_url) gf_mpd_print_url(out, ms->bitstream_switching_url, "BitstreamSwitching", indent+1);
	return 0;
}

static void gf_mpd_print_segment_list(FILE *out, GF_MPD_SegmentList *s, s32 indent)
{
	gf_mpd_nl(out, indent);
	gf_fprintf(out, "<SegmentList");
	if (s->xlink_href) {
		gf_fprintf(out, " xlink:href=\"%s\"", s->xlink_href);
		if (s->xlink_actuate_on_load)
			gf_fprintf(out, " actuate=\"onLoad\"");
	}	
	gf_mpd_print_multiple_segment_base(out, (GF_MPD_MultipleSegmentBase *)s, indent, GF_FALSE);
	
	if (s->segment_URLs) {
		u32 i;
		GF_MPD_SegmentURL *url;
		i = 0;
		while ( (url = gf_list_enum(s->segment_URLs, &i))) {
			gf_mpd_nl(out, indent+1);
			gf_fprintf(out, "<SegmentURL");
                       if (url->media) gf_fprintf(out, " media=\"%s\"", url->media);
                       if (url->duration)gf_fprintf(out, " duration=\""LLU"\"", url->duration);
			if (url->index) gf_fprintf(out, " index=\"%s\"", url->index);
			if (url->media_range && url->media_range->end_range!=0) gf_fprintf(out, " mediaRange=\""LLD"-"LLD"\"", url->media_range->start_range, url->media_range->end_range);
			if (url->index_range && url->index_range->end_range!=0) gf_fprintf(out, " indexRange=\""LLD"-"LLD"\"", url->index_range->start_range, url->index_range->end_range);
			if (url->key_url) {
				u32 idx;
				gf_fprintf(out, " hls:keyMethod=\"aes-128\" hls:KeyURL=%s hls:KeyIV=\"", url->key_url);
				for (idx=0; idx<16; idx++) {
					gf_fprintf(out, "%02x", url->key_iv[idx]);
				}
				gf_fprintf(out, "\"");
			}
			gf_fprintf(out, "/>");
			gf_mpd_lf(out, indent);
		}
	}
	gf_mpd_nl(out, indent);
	gf_fprintf(out, "</SegmentList>");
	gf_mpd_lf(out, indent);
}

static void gf_mpd_print_segment_template(FILE *out, GF_MPD_SegmentTemplate *s, s32 indent)
{
	gf_mpd_nl(out, indent);
	gf_fprintf(out, "<SegmentTemplate");

	if (s->media) gf_fprintf(out, " media=\"%s\"", s->media);
	if (s->index) gf_fprintf(out, " index=\"%s\"", s->index);
	if (s->initialization) gf_fprintf(out, " initialization=\"%s\"", s->initialization);
	if (s->bitstream_switching) gf_fprintf(out, " bitstreamSwitching=\"%s\"", s->bitstream_switching);

	if (gf_mpd_print_multiple_segment_base(out, (GF_MPD_MultipleSegmentBase *)s, indent, GF_TRUE))
		return;

	gf_mpd_nl(out, indent);
	gf_fprintf(out, "</SegmentTemplate>");
	gf_mpd_lf(out, indent);
}

static void gf_mpd_extensible_print_attr(FILE *out, GF_List *attributes)
{
	if (!attributes) return;
	u32 j=0;
	GF_XMLAttribute *att;
	while ((att = (GF_XMLAttribute *)gf_list_enum(attributes, &j))) {
		if (!strcmp(att->name, "xmlns")) continue;
		else if (!strcmp(att->name, "xmlns:gpac")) continue;
		gf_fprintf(out, " %s=\"", att->name);
		gf_xml_dump_string(out, NULL, att->value, "\"");
	}
}

static void gf_mpd_extensible_print_nodes(FILE *out, GF_List *children, s32 indent, u32 *child_idx, Bool is_final)
{
	u32 idx=0, i, count;
	if (!children) return;
	idx = *child_idx;
	count = gf_list_count(children);
	for (i=0; i<count; i++) {
		char *txt;
		GF_XMLNode *child = (GF_XMLNode *) gf_list_get(children, i);

		if (child->orig_pos < idx)
			continue;
		if ((child->orig_pos > idx) && !is_final) {
			*child_idx = idx+1;
			return;
		}

		txt = gf_xml_dom_serialize(child, GF_FALSE, GF_TRUE);
		gf_mpd_nl(out, indent+1);
		gf_fprintf(out, "%s", txt);
		gf_free(txt);
		gf_mpd_lf(out, indent);
		idx++;
	}
	if (!is_final) {
		*child_idx = idx+1;
	}
}

static void gf_mpd_print_desc(FILE *out, GF_MPD_Descriptor *desc, char *desc_name, s32 indent)
{
	gf_mpd_nl(out, indent);
	gf_fprintf(out, "<%s", desc_name);
	if (desc->id) gf_fprintf(out, " id=\"%s\"", desc->id);
	if (desc->scheme_id_uri) gf_fprintf(out, " schemeIdUri=\"%s\"", desc->scheme_id_uri);
	if (desc->value) gf_fprintf(out, " value=\"%s\"", desc->value);

	gf_mpd_extensible_print_attr(out, desc->x_attributes);

	if (desc->x_children) {
		u32 idx=0;
		gf_fprintf(out, ">");
		gf_mpd_lf(out, indent);
		gf_mpd_extensible_print_nodes(out, desc->x_children, indent, &idx, GF_TRUE);
		gf_mpd_nl(out, indent);
		gf_fprintf(out, "</%s>", desc_name);
		gf_mpd_lf(out, indent);
	} else {
		gf_fprintf(out, "/>");
		gf_mpd_lf(out, indent);
	}
}
static void gf_mpd_print_descriptors(FILE *out, GF_List *desc_list, char *desc_name, s32 indent, GF_List *x_children, u32 *child_idx)
{
	u32 i=0;
	GF_MPD_Descriptor *desc;
	while ((desc = (GF_MPD_Descriptor *)gf_list_enum(desc_list, &i))) {
		gf_mpd_extensible_print_nodes(out, x_children, indent, child_idx, GF_FALSE);
		gf_mpd_print_desc(out, desc, desc_name, indent);
	}
}

static void gf_mpd_print_content_component(FILE *out, GF_List *content_component , s32 indent)
{
	u32 i=0;
	GF_MPD_ContentComponent *cc;
	while ((cc = gf_list_enum(content_component, &i))) {
		gf_mpd_nl(out, indent);
		gf_fprintf(out, "<ContentComponent id=\"%d\" contentType=\"%s\"", cc->id, cc->type);
		if (cc->lang)
			gf_fprintf(out, " lang=\"%s\"", cc->lang);
		gf_fprintf(out, "/>");
		gf_mpd_lf(out, indent);
	}
}

static void gf_mpd_print_common_attributes(FILE *out, GF_MPD_CommonAttributes *ca)
{
	if (ca->profiles) {
		gf_xml_dump_string(out, " profiles=\"", ca->profiles, "\"");
	}
	if (ca->mime_type) gf_fprintf(out, " mimeType=\"%s\"", ca->mime_type);
	if (ca->codecs) gf_fprintf(out, " codecs=\"%s\"", ca->codecs);
	if (ca->width) gf_fprintf(out, " width=\"%d\"", ca->width);
	if (ca->height) gf_fprintf(out, " height=\"%d\"", ca->height);
	if (ca->framerate){
		gf_fprintf(out, " frameRate=\"%d",ca->framerate->num);
		if(ca->framerate->den>1)gf_fprintf(out, "/%d",ca->framerate->den);
		gf_fprintf(out, "\"");
	}
	if (ca->sar) gf_fprintf(out, " sar=\"%d:%d\"", ca->sar->num, ca->sar->den);
	if (ca->samplerate) gf_fprintf(out, " audioSamplingRate=\"%d\"", ca->samplerate);
	if (ca->segmentProfiles) {
		gf_xml_dump_string(out, " segmentProfiles=\"", ca->segmentProfiles, "\"");
	}
	if (ca->maximum_sap_period) gf_fprintf(out, " maximumSAPPeriod=\"%d\"", ca->maximum_sap_period);
	if (ca->starts_with_sap) gf_fprintf(out, " startWithSAP=\"%d\"", ca->starts_with_sap);
	if ((ca->max_playout_rate!=1.0)) gf_fprintf(out, " maxPlayoutRate=\"%g\"", ca->max_playout_rate);
	if (ca->coding_dependency) gf_fprintf(out, " codingDependency=\"true\"");
	if (ca->scan_type != GF_MPD_SCANTYPE_UNKNOWN) gf_fprintf(out, " scanType=\"%s\"", ca->scan_type == GF_MPD_SCANTYPE_PROGRESSIVE ? "progressive" : "interlaced");

	if (ca->selection_priority) gf_fprintf(out, " selectionPriority=\"%d\"", ca->selection_priority);
	if (ca->tag) gf_fprintf(out, " selectionPriority=\"%s\"", ca->tag);

}

static u32 gf_mpd_print_common_children(FILE *out, GF_MPD_CommonAttributes *ca, s32 indent, u32 *child_idx)
{
	gf_mpd_print_descriptors(out, ca->frame_packing, "Framepacking", indent, ca->x_children, child_idx);
	gf_mpd_print_descriptors(out, ca->audio_channels, "AudioChannelConfiguration", indent, ca->x_children, child_idx);
	gf_mpd_print_descriptors(out, ca->content_protection, "ContentProtection", indent, ca->x_children, child_idx);
	gf_mpd_print_descriptors(out, ca->essential_properties, "EssentialProperty", indent, ca->x_children, child_idx);
	gf_mpd_print_descriptors(out, ca->supplemental_properties, "SupplementalProperty", indent, ca->x_children, child_idx);

	if (ca->producer_reference_time) {
		u32 i, count = gf_list_count(ca->producer_reference_time);
		for (i=0; i<count; i++) {
			GF_MPD_ProducerReferenceTime *pref = gf_list_get(ca->producer_reference_time, i);
			gf_mpd_nl(out, indent);
			gf_fprintf(out, "<ProducerReferenceTime id=\"%d\" presentationTime=\"%d\"", pref->ID, pref->presentation_time);
			if (pref->inband) gf_fprintf(out, " inband=\"true\"");
			if (pref->wallclock) gf_fprintf(out, " wallClockTime=\"%s\"", pref->wallclock);
			switch (pref->type) {
			case GF_MPD_PRODUCER_REF_ENCODER:
				gf_fprintf(out, " type=\"encoder\"");
				break;
			case GF_MPD_PRODUCER_REF_CAPTURED:
				gf_fprintf(out, " type=\"captured\"");
				break;
			case GF_MPD_PRODUCER_REF_APPLICATION:
				gf_fprintf(out, " type=\"application\"");
				if (pref->scheme) gf_fprintf(out, " applicationScheme=\"%s\"", pref->scheme);
				break;
			}
			if (pref->utc_timing) {
				gf_fprintf(out, ">");
				gf_mpd_lf(out, indent);
				gf_mpd_print_desc(out, pref->utc_timing, "UTCTiming", indent+1);
				gf_fprintf(out, "</ProducerReferenceTime>");
				gf_mpd_lf(out, indent);
			} else {
				gf_fprintf(out, "/>");
				gf_mpd_lf(out, indent);
			}
		}
	}
	if (ca->isobmf_tracks) {
		u32 k=0;
		GF_MPD_ISOBMFInfo *info;
		gf_mpd_nl(out, indent);
		gf_fprintf(out, "<ISOBMFInfo>");
		gf_mpd_lf(out, indent);
		while ((info = (GF_MPD_ISOBMFInfo *) gf_list_enum(ca->isobmf_tracks, &k))) {
			gf_mpd_nl(out, indent+1);
			gf_fprintf(out, "<ISOBMFTrack");
			if (info->trackID) gf_fprintf(out, " ID=\"%d\"", info->trackID);
			if (info->stsd) gf_fprintf(out, " stsd=\"%s\"", info->stsd);
			if (info->mediaOffset) gf_fprintf(out, " offset=\""LLD"\"", info->mediaOffset);
			gf_fprintf(out, "/>");
			gf_mpd_lf(out, indent);
		}
		gf_mpd_nl(out, indent);
		gf_fprintf(out, "</ISOBMFInfo>");
		gf_mpd_lf(out, indent);
	}
	return 0;
}

static void gf_mpd_print_dasher_context(FILE *out, GF_DASH_SegmenterContext *dasher, s32 indent)
{
	gf_mpd_nl(out, indent);
	gf_fprintf(out, "<gpac:dasher ");
	gf_fprintf(out, "done=\"%s\" ", dasher->done ? "true" : "false");
	gf_fprintf(out, "init=\"%s\" ", dasher->init_seg);
	gf_fprintf(out, "template=\"%s\" ", dasher->template_seg);
	if (dasher->template_idx)
		gf_fprintf(out, "index=\"%s\" ", dasher->template_idx);
	gf_fprintf(out, "segNumber=\"%d\" ", dasher->seg_number);
	gf_fprintf(out, "url=\"%s\" ", dasher->src_url);
	gf_fprintf(out, "lastPacketIdx=\""LLU"\" ", dasher->last_pck_idx);
	gf_fprintf(out, "pidID=\"%d\" ", dasher->pid_id);

	if (dasher->dep_pid_id)
		gf_fprintf(out, "depID=\"%d\" ", dasher->dep_pid_id);

	if (dasher->period_id)
		gf_fprintf(out, "periodID=\"%s\" ", dasher->period_id);

	if (dasher->period_duration.num && dasher->period_duration.den)
		gf_fprintf(out, "periodDuration=\""LLD"/"LLU"\" ", dasher->period_duration.num, dasher->period_duration.den);
	if (dasher->period_start.num && dasher->period_start.den)
		gf_fprintf(out, "periodStart=\""LLD"/"LLU"\" ", dasher->period_start.num, dasher->period_start.den);

	gf_fprintf(out, "multiPIDInit=\"%s\" ", dasher->multi_pids ? "true" : "false");
	gf_fprintf(out, "dashDuration=\"%d/%d\" ", dasher->dash_dur.num, dasher->dash_dur.den);
	gf_fprintf(out, "nextSegmentStart=\""LLU"\" ", dasher->next_seg_start);
	gf_fprintf(out, "firstCTS=\""LLU"\" ", dasher->first_cts);
	gf_fprintf(out, "firstDTS=\""LLU"\" ", dasher->first_dts);
	gf_fprintf(out, "mpdTimescale=\"%d\" ", dasher->mpd_timescale);
	gf_fprintf(out, "sourcePID=\"%d\" ", dasher->source_pid);
	gf_fprintf(out, "estimatedNextDTS=\""LLU"\" ", dasher->est_next_dts);
	gf_fprintf(out, "cumulatedDur=\"%g\" ", dasher->cumulated_dur);
	gf_fprintf(out, "cumulatedSubdur=\"%g\" ", dasher->cumulated_subdur);

	gf_fprintf(out, "moofSN=\"%d\" ", dasher->moof_sn);
	gf_fprintf(out, "moofInc=\"%d\" ", dasher->moof_sn_inc);

	if (dasher->segs_purged)
		gf_fprintf(out, "segsPurged=\"%d\" ", dasher->segs_purged);
	if (dasher->dur_purged)
		gf_fprintf(out, "durPurged=\"%g\" ", dasher->dur_purged);

	if (dasher->nb_repeat)
		gf_fprintf(out, "nbRepeat=\"%d\" ", dasher->nb_repeat);
	if (dasher->ts_offset)
		gf_fprintf(out, "tsOffset=\""LLU"\" ", dasher->ts_offset);
	if (dasher->mux_pids)
		gf_fprintf(out, "muxPIDs=\"%s\" ", dasher->mux_pids);

	if (dasher->last_dyn_period_id) {
		gf_fprintf(out, "lastDynPeriodID=\"%d\" ", dasher->last_dyn_period_id);
	}
	if (dasher->subdur_forced) {
		gf_fprintf(out, "subdurForced=\"true\" ");
	}

	gf_fprintf(out, "ownsSet=\"%s\"/>", dasher->owns_set ? "true" : "false");
	gf_mpd_lf(out, indent);
}

static void gf_mpd_print_dasher_segments(FILE *out, GF_List *segments, s32 indent)
{
	u32 i, count = gf_list_count(segments);
	if (!count) return;

	gf_mpd_nl(out, indent);
	gf_fprintf(out, "<gpac:segments>\n");
	for (i=0; i<count; i++) {
		GF_DASH_SegmentContext *sctx = gf_list_get(segments, i);
		gf_mpd_nl(out, indent+1);
		gf_fprintf(out, "<segmentInfo ");
		gf_fprintf(out, "time=\""LLU"\" ", sctx->time);
		gf_fprintf(out, "dur=\""LLU"\" ", sctx->dur);
		gf_fprintf(out, "seg_num=\"%d\" ", sctx->seg_num);
		if (sctx->filename) gf_fprintf(out, "file=\"%s\" ", sctx->filename);
		if (sctx->filepath) gf_fprintf(out, "path=\"%s\" ", sctx->filepath);
		if (sctx->file_size) {
			gf_fprintf(out, "size=\"%d\" ", sctx->file_size);
			if (sctx->file_offset) gf_fprintf(out, "offset=\""LLU"\" ", sctx->file_offset);
		}
		if (sctx->index_size) {
			gf_fprintf(out, "idx_size=\"%d\" ", sctx->index_size);
			if (sctx->index_offset) gf_fprintf(out, "idx_offset=\""LLU"\" ", sctx->index_offset);
		}
		//we don't store frag context because we can only serialize the dash state at segment boundaries, and we don't keep frag states for already published
		//segments
		gf_fprintf(out, "/>");
		gf_mpd_lf(out, indent);
	}

	gf_mpd_nl(out, indent);
	gf_fprintf(out, "</gpac:segments>");
	gf_mpd_lf(out, indent);
}

static void gf_mpd_print_representation(GF_MPD_Representation *rep, FILE *out, Bool write_context, s32 indent, u32 alt_mha_profile)
{
	u32 child_idx = 0;
	char *bck_codecs = NULL;
	gf_mpd_nl(out, indent);
	gf_fprintf(out, "<Representation");
	if (rep->id) gf_fprintf(out, " id=\"%s\"", rep->id);

/*	if (!gf_list_count(rep->base_URLs) && !rep->segment_base && !rep->segment_template && !rep->segment_list && !gf_list_count(rep->sub_representations)) {
		can_close = 1;
	}
*/
	if (alt_mha_profile) {
		char szTmp[15], *sep;
		bck_codecs = rep->codecs;
		rep->codecs = gf_strdup(bck_codecs);
		snprintf(szTmp, 14, "0x%02X", alt_mha_profile-1);
		szTmp[14] = 0;
		sep = strstr(rep->codecs, ".0x");
		if (sep) strcpy(sep+1, szTmp);
	}
	gf_mpd_print_common_attributes(out, (GF_MPD_CommonAttributes*)rep);

	if (rep->bandwidth) gf_fprintf(out, " bandwidth=\"%d\"", rep->bandwidth);
	if (rep->quality_ranking) gf_fprintf(out, " qualityRanking=\"%d\"", rep->quality_ranking);
	if (rep->dependency_id) gf_fprintf(out, " dependencyId=\"%s\"", rep->dependency_id);
	if (rep->media_stream_structure_id) gf_fprintf(out, " mediaStreamStructureId=\"%s\"", rep->media_stream_structure_id);

	if (bck_codecs) {
		gf_free(rep->codecs);
		rep->codecs = bck_codecs;
	}


	gf_fprintf(out, ">");
	gf_mpd_lf(out, indent);

	if (write_context) {
		if (rep->dasher_ctx) {
			gf_mpd_print_dasher_context(out, rep->dasher_ctx, indent+1);
		}
		if (rep->state_seg_list) {
			gf_mpd_print_dasher_segments(out, rep->state_seg_list, indent+1);
		}
	}

	gf_mpd_print_common_children(out, (GF_MPD_CommonAttributes*)rep, indent+1, &child_idx);

	gf_mpd_print_base_urls(out, rep->base_URLs, indent+1);
	if (rep->segment_base) {
		gf_mpd_extensible_print_nodes(out, rep->x_children, indent, &child_idx, GF_FALSE);
		gf_mpd_print_segment_base(out, rep->segment_base, indent+1);
	}
	if (rep->segment_list) {
		gf_mpd_extensible_print_nodes(out, rep->x_children, indent, &child_idx, GF_FALSE);
		gf_mpd_print_segment_list(out, rep->segment_list, indent+1);
	}
	if (rep->segment_template) {
		gf_mpd_extensible_print_nodes(out, rep->x_children, indent, &child_idx, GF_FALSE);
		gf_mpd_print_segment_template(out, rep->segment_template, indent+1);
	}
	/*TODO
				e = gf_mpd_parse_subrepresentation(rep->sub_representations, child);
				if (e) return e;
	*/

	gf_mpd_extensible_print_nodes(out, rep->x_children, indent, &child_idx, GF_TRUE);

	gf_mpd_nl(out, indent);
	gf_fprintf(out, "</Representation>");
	gf_mpd_lf(out, indent);
}

static void gf_mpd_print_adaptation_set(GF_MPD_AdaptationSet *as, FILE *out, Bool write_context, s32 indent, u32 alt_mha_profile)
{
	u32 i, child_idx=0;
	GF_MPD_Representation *rep;

	if (!alt_mha_profile && as->nb_alt_mha_profiles && as->alt_mha_profiles_only) {
		for (i=0; i<as->nb_alt_mha_profiles; i++) {
			gf_mpd_print_adaptation_set(as, out, write_context, indent, as->alt_mha_profiles[i] + 1);
		}

		return;
	}

	gf_mpd_nl(out, indent);
	gf_fprintf(out, "<AdaptationSet");

	if (as->id>=0) gf_fprintf(out, " id=\"%d\"", as->id);
	if (as->xlink_href) {
		gf_fprintf(out, " xlink:href=\"%s\"", as->xlink_href);
		if (as->xlink_actuate_on_load)
			gf_fprintf(out, " actuate=\"onLoad\"");
	}
	if (as->segment_alignment) gf_fprintf(out, " segmentAlignment=\"true\"");
	if (as->group !=  (u32) -1) gf_fprintf(out, " group=\"%d\"", as->group);
	if (as->min_bandwidth) gf_fprintf(out, " minBandwidth=\"%d\"", as->min_bandwidth);
	if (as->max_bandwidth) gf_fprintf(out, " maxBandwidth=\"%d\"", as->max_bandwidth);
	if (as->min_width) gf_fprintf(out, " minWidth=\"%d\"", as->min_width);
	if (as->max_width) gf_fprintf(out, " maxWidth=\"%d\"", as->max_width);
	if (as->min_height) gf_fprintf(out, " minHeight=\"%d\"", as->min_height);
	if (as->max_height) gf_fprintf(out, " maxHeight=\"%d\"", as->max_height);
	if ((as->min_framerate.num != 0) && (as->min_framerate.den != 0)) {
		if (as->min_framerate.den==1)
			gf_fprintf(out, " minFrameRate=\"%d\"", as->min_framerate.num);
		else
			gf_fprintf(out, " minFrameRate=\"%d/%d\"", as->min_framerate.num, as->min_framerate.den);
	}
	if ((as->max_framerate.num != 0) && (as->max_framerate.den != 0)) {
		if (as->max_framerate.den==1)
			gf_fprintf(out, " maxFrameRate=\"%d\"", as->max_framerate.num);
		else
			gf_fprintf(out, " maxFrameRate=\"%d/%d\"", as->max_framerate.num, as->max_framerate.den);
	}
	if (as->par && (as->par->num != 0) && (as->par->den != 0))
		gf_fprintf(out, " par=\"%d:%d\"", as->par->num, as->par->den);
	if (as->lang) gf_fprintf(out, " lang=\"%s\"", as->lang);
	if (as->bitstream_switching) gf_fprintf(out, " bitstreamSwitching=\"true\"");

	gf_mpd_print_common_attributes(out, (GF_MPD_CommonAttributes*)as);
	//backward compatibilty with old arch
	if (as->subsegment_alignment) gf_fprintf(out, " subsegmentAlignment=\"true\"");
	if (as->subsegment_starts_with_sap) gf_fprintf(out, " subsegmentStartsWithSAP=\"%d\"", as->subsegment_starts_with_sap);

	gf_fprintf(out, ">");
	gf_mpd_lf(out, indent);

	gf_mpd_print_common_children(out, (GF_MPD_CommonAttributes*)as, indent+1, &child_idx);

	gf_mpd_print_base_urls(out, as->base_URLs, indent+1);

	gf_mpd_print_descriptors(out, as->accessibility, "Accessibility", indent+1, as->x_children, &child_idx);
	gf_mpd_print_descriptors(out, as->role, "Role", indent+1, as->x_children, &child_idx);
	gf_mpd_print_descriptors(out, as->rating, "Rating", indent+1, as->x_children, &child_idx);
	gf_mpd_print_descriptors(out, as->viewpoint, "Viewpoint", indent+1, as->x_children, &child_idx);
	gf_mpd_print_content_component(out, as->content_component, indent+1);

	if (as->segment_base) {
		gf_mpd_extensible_print_nodes(out, as->x_children, indent, &child_idx, GF_FALSE);
		gf_mpd_print_segment_base(out, as->segment_base, indent+1);
	}
	if (as->segment_list) {
		gf_mpd_extensible_print_nodes(out, as->x_children, indent, &child_idx, GF_FALSE);
		gf_mpd_print_segment_list(out, as->segment_list, indent+1);
	}
	if (as->segment_template) {
		gf_mpd_extensible_print_nodes(out, as->x_children, indent, &child_idx, GF_FALSE);
		gf_mpd_print_segment_template(out, as->segment_template, indent+1);
	}

	i=0;
	while ((rep = (GF_MPD_Representation *)gf_list_enum(as->representations, &i))) {
		gf_mpd_extensible_print_nodes(out, as->x_children, indent, &child_idx, GF_FALSE);
		gf_mpd_print_representation(rep, out, write_context, indent+1, alt_mha_profile);
	}
	gf_mpd_extensible_print_nodes(out, as->x_children, indent, &child_idx, GF_TRUE);
	gf_mpd_nl(out, indent);
	gf_fprintf(out, "</AdaptationSet>");
	gf_mpd_lf(out, indent);

	if (!alt_mha_profile) {
		for (i=0; i<as->nb_alt_mha_profiles; i++) {
			gf_mpd_print_adaptation_set(as, out, write_context, indent, as->alt_mha_profiles[i] + 1);
		}
	}
}

static void gf_mpd_print_period(GF_MPD_Period const * const period, Bool is_dynamic, FILE *out, Bool write_context, s32 indent)
{
	GF_MPD_AdaptationSet *as;
	u32 i, child_idx=0;
	gf_mpd_nl(out, indent);
	gf_fprintf(out, "<Period");
	if (period->xlink_href) {
		gf_fprintf(out, " xlink:href=\"%s\"", period->xlink_href);
		if (period->xlink_actuate_on_load)
			gf_fprintf(out, " actuate=\"onLoad\"");
	}
	if (period->ID)
		gf_fprintf(out, " id=\"%s\"", period->ID);
	if (is_dynamic || period->start)
		gf_mpd_print_duration(out, "start", period->start, GF_TRUE);
	if (period->duration)
		gf_mpd_print_duration(out, "duration", period->duration, GF_TRUE);
	if (period->bitstream_switching)
		gf_fprintf(out, " bitstreamSwitching=\"true\"");

	gf_fprintf(out, ">");
	gf_mpd_lf(out, indent);

	gf_mpd_print_base_urls(out, period->base_URLs, indent+1);

	if (period->segment_base) {
		gf_mpd_extensible_print_nodes(out, period->x_children, indent, &child_idx, GF_FALSE);
		gf_mpd_print_segment_base(out, period->segment_base, indent+1);
	}
	if (period->segment_list) {
		gf_mpd_extensible_print_nodes(out, period->x_children, indent, &child_idx, GF_FALSE);
		gf_mpd_print_segment_list(out, period->segment_list, indent+1);
	}
	if (period->segment_template) {
		gf_mpd_extensible_print_nodes(out, period->x_children, indent, &child_idx, GF_FALSE);
		gf_mpd_print_segment_template(out, period->segment_template, indent+1);
	}

	i=0;
	while ( (as = (GF_MPD_AdaptationSet *) gf_list_enum(period->adaptation_sets, &i))) {
		gf_mpd_extensible_print_nodes(out, period->x_children, indent, &child_idx, GF_FALSE);
		gf_mpd_print_adaptation_set(as, out, write_context, indent+1, 0);
	}
	gf_mpd_extensible_print_nodes(out, period->x_children, indent, &child_idx, GF_TRUE);
	gf_mpd_nl(out, indent);
	gf_fprintf(out, "</Period>");
	gf_mpd_lf(out, indent);
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

	gtime = sec;
	t = gf_gmtime(&gtime);
	if (! gf_sys_is_test_mode() ){
		gf_fprintf(out, "<!-- MPD file Generated with GPAC version %s at %d-%02d-%02dT%02d:%02d:%02d.%03dZ -->\n", gf_gpac_version(), 1900 + t->tm_year, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, (u32)time_ms);
	}
	if (!mpd->write_context) {
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[MPD] Generating MPD at time %d-%02d-%02dT%02d:%02d:%02d.%03dZ\n", 1900 + t->tm_year, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, (u32)time_ms));
	}
	return GF_OK;
}

static void gf_mpd_write_m3u8_playlist_tags_entry(FILE *out, const GF_MPD_Representation *rep, char *m3u8_name, const char *codec_ext, const char *g_type, const char *g_id_pref, u32 g_as_idx, const char *g2_type, const char *g2_id_pref, u32 g2_as_idx, GF_List *groups_done, const GF_MPD_AdaptationSet *set)
{
	u32 i;
	if (groups_done) {
		u32 count=gf_list_count(groups_done);
		Bool g1_done = GF_FALSE;
		Bool g2_done = GF_FALSE;
		for (i=0; i<count; i++) {
			const char *group_name = gf_list_get(groups_done, i);
			if (g_id_pref && !strcmp(group_name, g_id_pref)) g1_done=GF_TRUE;
			if (g2_id_pref && !strcmp(group_name, g2_id_pref)) g1_done=GF_TRUE;
		}
		if (g_id_pref) {
			if (g1_done) return;
			if (!g_as_idx)
				gf_list_add(groups_done, (void *) g_id_pref);
		}
		if (g2_id_pref) {
			if (g2_done) return;
			if (!g2_as_idx)
				gf_list_add(groups_done, (void *) g2_id_pref);
		}
	}

	if (set && set->intra_only)
		gf_fprintf(out, "#EXT-X-I-FRAME-STREAM-INF:");
	else
		gf_fprintf(out, "#EXT-X-STREAM-INF:");

	gf_fprintf(out, "BANDWIDTH=%d,CODECS=\"%s", rep->bandwidth, rep->codecs);
	if (codec_ext)
		gf_fprintf(out, ",%s", codec_ext);
	gf_fprintf(out, "\"");

	if (rep->width && rep->height)
		gf_fprintf(out, ",RESOLUTION=%dx%d", rep->width, rep->height);

	if (set && set->intra_only) {
		gf_fprintf(out, ",URI=\"%s\"\n", m3u8_name);
		return;
	}
	if (rep->fps)
		gf_fprintf(out,",FRAME-RATE=%.03g", rep->fps);

	if (g_type && g_id_pref) {
		gf_fprintf(out, ",%s=\"%s", g_type, g_id_pref);
		if (g_as_idx)
			gf_fprintf(out, "%d", g_as_idx);
		gf_fprintf(out, "\"");
	}
	if (g2_type && g2_id_pref) {
		gf_fprintf(out,",%s=\"%s", g2_type, g2_id_pref);
		if (g2_as_idx)
			gf_fprintf(out,"%d", g2_as_idx);
		gf_fprintf(out,"\"");
	}
	for (i=0; i<rep->nb_hls_master_tags; i++) {
		gf_fprintf(out,",%s", rep->hls_master_tags[i]);
	}
	gf_fprintf(out,"\n");

	gf_fprintf(out, "%s\n",m3u8_name);

}

static void gf_mpd_write_m3u8_playlist_tags(const GF_MPD_AdaptationSet *as, u32 as_idx, const GF_MPD_Representation *rep, FILE *out, char *m3u8_name, GF_MPD_Period *period, u32 nb_audio, u32 nb_subs, u32 nb_cc)
{
	u32 i, j;
	GF_MPD_AdaptationSet *r_as;
	GF_MPD_Representation *r_rep;
	GF_List *groups_done;

	if (!rep->mime_type) return;

	//no period, this is a component description
	if (!period) {
		const char *g_type = NULL;
		const char *g_id = NULL;
		char szLANG[100];

		if (rep->streamtype==GF_STREAM_AUDIO) {
			g_type = "AUDIO";
			g_id = "audio";
		}
		else if (rep->streamtype==GF_STREAM_TEXT) {
			g_type = "SUBTITLES";
			g_id = "subs";
		}
		if (!g_type || !g_id)
			return;

		//don't inject LANGUAGE if not known
		if (!strcmp(as->lang, "und")) {
			szLANG[0] = 0;
		} else {
			sprintf(szLANG, ",LANGUAGE=\"%s\"", as->lang);
		}
		if (rep->groupID)
			gf_fprintf(out, "#EXT-X-MEDIA:TYPE=%s,GROUP-ID=\"%s\",NAME=\"%s\"%s,AUTOSELECT=YES,URI=\"%s\"", g_type, rep->groupID, rep->id, szLANG, m3u8_name);
		else
			gf_fprintf(out, "#EXT-X-MEDIA:TYPE=%s,GROUP-ID=\"%s%d\",NAME=\"%s\"%s,AUTOSELECT=YES,URI=\"%s\"", g_type, g_id, as_idx, rep->id, szLANG, m3u8_name);
		if (rep->nb_chan)
			gf_fprintf(out,",CHANNELS=\"%d\"", rep->nb_chan);

		for (i=0; i<rep->nb_hls_master_tags; i++) {
			gf_fprintf(out,",%s", rep->hls_master_tags[i]);
		}
		return;
	}

	//no other streams, directly write the entry
	if (!nb_audio && !nb_subs && !nb_cc) {
		gf_mpd_write_m3u8_playlist_tags_entry(out, rep, m3u8_name, NULL, NULL, NULL, 0, NULL, NULL, 0, NULL, as);
		return;
	}
	groups_done = gf_list_new();

	//otherwise browse all adaptation sets
	i=0;
	while ( (r_as = (GF_MPD_AdaptationSet *) gf_list_enum(period->adaptation_sets, &i))) {
		u32 g_as_idx;
		Bool is_audio = GF_FALSE;
		GF_MPD_AdaptationSet *r2_as;
		GF_MPD_Representation *r2_rep;
		const char *g_type = NULL;
		const char *g_id = NULL;
		const char *g_codec = NULL;
		if (as==r_as) continue;

		g_as_idx = (r_as->group>0) ? r_as->group : i;
		r_rep = (GF_MPD_Representation *) gf_list_get(r_as->representations, 0);

		//if audio streams are present, the first pass gather audio and we will need a second loop to get subs
		if (nb_audio) {
			if (r_rep->streamtype==GF_STREAM_AUDIO) {
				g_type = "AUDIO";
				g_id = "audio";
				g_codec = r_rep->codecs;
				if (r_rep->groupID) {
					g_id = r_rep->groupID;
					g_as_idx = 0;
				}
			}
			is_audio = GF_TRUE;
		} else {
			if (r_rep->streamtype==GF_STREAM_TEXT) {
				g_type = "SUBTITLES";
				g_id = "subs";
			}
		}
		//not our type
		if (!g_type) continue;
		//no audio, or no subs
		if (!is_audio || !nb_subs) {
			gf_mpd_write_m3u8_playlist_tags_entry(out, rep, m3u8_name, g_codec, g_type, g_id, g_as_idx, NULL, NULL, 0, groups_done, as);
			continue;
		}
		//audio and subs, we need a second loop on audio to get all subs
		j=0;
		while ( (r2_as = (GF_MPD_AdaptationSet *) gf_list_enum(period->adaptation_sets, &j))) {
			u32 g2_as_idx;
			const char *g2_type = NULL;
			const char *g2_id = NULL;
			if (r_as==r2_as) continue;
			g2_as_idx = (r2_as->group>0) ? r2_as->group : j;

			r2_rep = (GF_MPD_Representation *) gf_list_get(r2_as->representations, 0);
			if (r2_rep->streamtype==GF_STREAM_TEXT) {
				g2_type = "SUBTITLES";
				g2_id = "subs";
				if (r_rep->groupID) {
					g2_id = r_rep->groupID;
					g2_as_idx = 0;
				}
			}
			if (!g2_type) continue;

			gf_mpd_write_m3u8_playlist_tags_entry(out, rep, m3u8_name, g_codec, g_type, g_id, g_as_idx, g2_type, g2_id, g2_as_idx, NULL, as);
		}
	}
	gf_list_del(groups_done);
}

static const char *gf_mpd_m3u8_get_init_seg(const GF_MPD_Period *period, const GF_MPD_AdaptationSet *as, const GF_MPD_Representation *rep)
{
	const char *url = NULL;
	if (rep->segment_list && rep->segment_list->initialization_segment) url = rep->segment_list->initialization_segment->sourceURL;
	else if (rep->segment_template && rep->segment_template->initialization) url = rep->segment_template->initialization;
	else if (rep->segment_template && rep->segment_template->initialization_segment) url = rep->segment_template->initialization_segment->sourceURL;

	if (as->segment_list && as->segment_list->initialization_segment) url = as->segment_list->initialization_segment->sourceURL;
	else if (as->segment_template && as->segment_template->initialization) url = as->segment_template->initialization;
	else if (as->segment_template && as->segment_template->initialization_segment) url = as->segment_template->initialization_segment->sourceURL;

	if (period->segment_list && period->segment_list->initialization_segment) url = period->segment_list->initialization_segment->sourceURL;
	else if (period->segment_template && period->segment_template->initialization) url = period->segment_template->initialization;
	else if (period->segment_template && period->segment_template->initialization_segment) url = period->segment_template->initialization_segment->sourceURL;
	return url;
}

static GF_Err gf_mpd_write_m3u8_playlist(const GF_MPD *mpd, const GF_MPD_Period *period, const GF_MPD_AdaptationSet *as, GF_MPD_Representation *rep, char *m3u8_name, u32 hls_version, Double max_part_dur_session, const char *force_base_url)
{
	u32 i, count;
	GF_DASH_SegmentContext *sctx;
	FILE *out;
	char *force_url=NULL;
	const char *last_kms = NULL;
	Bool close_file = GF_FALSE;

	if (!strcmp(m3u8_name, "std")) out = stdout;
	else if (mpd->create_m3u8_files) {
		out = gf_fopen(m3u8_name, "wb");
		if (!out) return GF_IO_ERR;
		close_file = GF_TRUE;
	} else {
		out = gf_file_temp(NULL);
		if (rep->m3u8_var_file) gf_fclose(rep->m3u8_var_file);
		rep->m3u8_var_file = out;
	}

	count = gf_list_count(rep->state_seg_list);
	sctx = gf_list_get(rep->state_seg_list, 0);

	gf_fprintf(out,"#EXTM3U\n");
	gf_fprintf(out,"#EXT-X-TARGETDURATION:%d\n", (u32) ((Double) rep->dash_dur.num) / rep->dash_dur.den);
	gf_fprintf(out,"#EXT-X-VERSION:%d\n", hls_version);
	gf_fprintf(out,"#EXT-X-MEDIA-SEQUENCE:%d\n", sctx->seg_num);
	if (as->use_hls_ll) {
		//PART-HOLD-BACK is REQUIRED if the Playlist contains the EXT-X-PART-INF tag
		//we use the recommended (should) PART-TARGET x 3
		if (mpd->llhls_part_holdback>0) {
			gf_fprintf(out,"#EXT-X-SERVER-CONTROL:PART-HOLD-BACK=%g\n", mpd->llhls_part_holdback);
		} else {
			gf_fprintf(out,"#EXT-X-SERVER-CONTROL:PART-HOLD-BACK=%g\n", 3 * max_part_dur_session);
		}
		gf_fprintf(out,"#EXT-X-PART-INF:PART-TARGET=%g\n", rep->hls_ll_part_dur);
	}
	for (i=0; i<rep->nb_hls_variant_tags; i++) {
		gf_fprintf(out,"%s\n", rep->hls_variant_tags[i]);
	}

	if (as->starts_with_sap<SAP_TYPE_3)
		gf_fprintf(out,"#EXT-X-INDEPENDENT-SEGMENTS\n");

	if (mpd->m3u8_time && rep->timescale_mpd && (mpd->type == GF_MPD_TYPE_DYNAMIC)) {
		u64 seg_ast = mpd->availabilityStartTime;
		seg_ast += (sctx->time * 1000) / rep->timescale_mpd;
		gf_fprintf(out, "#EXT-X-PROGRAM-DATE-TIME:");
		gf_mpd_print_date(out, NULL, seg_ast);
		gf_fprintf(out, "\n");
	}


	if (sctx->filename) {
		if (as->intra_only) {
			gf_fprintf(out,"#EXT-X-I-FRAMES-ONLY\n");
		}
		if (rep->hls_single_file_name) {
			if (force_base_url)
				force_url = gf_url_concatenate(force_base_url, rep->hls_single_file_name);

			gf_fprintf(out,"#EXT-X-MAP:URI=\"%s\"\n", force_url ? force_url : rep->hls_single_file_name);

			if (force_url) {
				gf_free(force_url);
				force_url = NULL;
			}
		}

		for (i=0; i<count; i++) {
			Double dur;
			sctx = gf_list_get(rep->state_seg_list, i);
			assert(sctx->filename);

			if (rep->crypto_type) {
				const char *kms;
				if (!sctx->encrypted) kms = "NONE";
				else if (sctx->hls_key_uri) kms = sctx->hls_key_uri;
				else {
					kms = "URI=\"gpac:hls:key:locator:null\"";
					if (!rep->def_kms_used) {
						rep->def_kms_used = 1;
						GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[HLS] Missing key URI in one or more keys - will use dummy one %s\n", kms));
					}
				}

				if (!last_kms || strcmp(kms, last_kms)) {
					if (!strcmp(kms, "NONE")) {
						gf_fprintf(out,"#EXT-X-KEY:METHOD=NONE\n");
					} else {
						char *subkms = (char *) kms;
						while (1) {
							char *next = strstr(subkms, ",URI");
							if (next) next[0] = 0;
							if (rep->crypto_type==1) {
								u32 k;
								gf_fprintf(out,"#EXT-X-KEY:METHOD=AES-128,%s,IV=0x", subkms);
								for (k=0; k<16; k++)
									gf_fprintf(out, "%02X", sctx->hls_iv[k]);
								gf_fprintf(out, "\n");
							} else {
								gf_fprintf(out,"#EXT-X-KEY:METHOD=SAMPLE-AES,%s\n", subkms);
							}
							if (!next) break;
							next[0] = ',';
							subkms = next+1;
						}
					}
					last_kms = (rep->crypto_type==2) ? kms : NULL;
				}
			}

			u64 next_br_start_plus_one=0;
			u32 next_seg_idx=0;
			if ((mpd->type == GF_MPD_TYPE_DYNAMIC) && sctx->llhls_mode) {
				u32 k;
				for (k=0; k<sctx->nb_frags; k++) {
					Bool write_br = GF_FALSE;
					dur = sctx->frags[k].duration;
					dur /= rep->timescale;

					if (force_base_url)
						force_url = gf_url_concatenate(force_base_url, sctx->filename);

					gf_fprintf(out, "#EXT-X-PART:DURATION=%g,URI=\"%s", dur, force_url ? force_url : sctx->filename);
					if (force_url ) {
						gf_free(force_url);
						force_url = NULL;
					}

					if (mpd->force_llhls_mode==1) write_br = GF_TRUE;
					else if (mpd->force_llhls_mode==2) write_br = GF_FALSE;
					else if (sctx->llhls_mode==1) write_br = GF_TRUE;

					if (write_br) {
						gf_fprintf(out, "\",BYTERANGE=\""LLU"@"LLU"\"", sctx->frags[k].size, sctx->frags[k].offset );
						next_br_start_plus_one = 1 + sctx->frags[k].offset + sctx->frags[k].size;
					} else {
						next_seg_idx = k+2;
						gf_fprintf(out, ".%d\"", k+1);
					}

					if (sctx->frags[k].independent)
						gf_fprintf(out, ",INDEPENDENT=YES");
					gf_fprintf(out, "\n");
				}
				//live edge not done yet
				if (!sctx->llhls_mode) {
					if (close_file)
						gf_fclose(out);
					return GF_OK;
				}
			}

			//live edge seg not done yet, do not write EXTINF and stop writing
			if (sctx->llhls_mode && (i+1==count) && !sctx->llhls_done) {
				//write preload hint
				if (mpd->llhls_preload) {
					//last seg has no parts yet, we just started it (live edge), advertise first part
					if (!sctx->nb_frags) {
						assert(sctx->filename);
						if (sctx->llhls_mode==2) next_seg_idx = 1;
						else next_br_start_plus_one = 1;
					}

					if (force_base_url)
						force_url = gf_url_concatenate(force_base_url, sctx->filename);

					if (next_seg_idx)
						gf_fprintf(out, "#EXT-X-PRELOAD-HINT:TYPE=PART,URI=\"%s.%d\"\n", force_url ? force_url : sctx->filename, next_seg_idx);
					else if (next_br_start_plus_one)
						gf_fprintf(out, "#EXT-X-PRELOAD-HINT:TYPE=PART,URI=\"%s\",BYTERANGE-START="LLU"\n", force_url ? force_url : sctx->filename, next_br_start_plus_one-1);

					if (force_url) {
						gf_free(force_url);
						force_url = NULL;
					}
				}
				//generate rendition report
				if (mpd->llhls_rendition_reports) {
					char *par_dir= strchr(m3u8_name, '/');
					u32 i_as=0;
					const GF_MPD_AdaptationSet *o_as;
					while ( (o_as = gf_list_enum(period->adaptation_sets, &i_as))) {
						u32 i_rep=0;
						GF_MPD_Representation *o_rep;
						while ( (o_rep = gf_list_enum(o_as->representations, &i_rep))) {
							GF_DASH_SegmentContext *o_sctx;
							if (o_rep == rep) continue;
							o_sctx = gf_list_last(o_rep->state_seg_list);
							if (!o_sctx || !o_sctx->llhls_mode) continue;
							//not clear in the spec, we assume what is listed must be the last PART completely produced
							//if no frag and a segment exists before, use last part of that segment
							if (!o_sctx->nb_frags) {
								u32 idx = gf_list_count(o_rep->state_seg_list);
								if (idx<2) continue;
								o_sctx = gf_list_get(o_rep->state_seg_list, idx-2);
								if (!o_sctx || !o_sctx->nb_frags) continue;
							}

							char *o_name = (char *) o_rep->m3u8_name;
							if (!o_name) {
								o_name = gf_file_basename(o_rep->m3u8_var_name);
							}
							char *par_url = NULL;
							//we always produce from the same root, so just use ../
							//if no parent dir we are producing at the root, and o_name is relative to the root
							if (par_dir) {
								gf_dynstrcat(&par_url, "../", NULL);
								gf_dynstrcat(&par_url, o_name, NULL);
							}
							fprintf(out, "#EXT-X-RENDITION-REPORT:URI=\"%s\",LAST-MSN=%d,LAST-PART=%d\n", par_url ? par_url : o_name, o_sctx->seg_num, o_sctx->nb_frags);
							if (par_url) gf_free(par_url);
						}
					}
				}
				
				if (close_file)
					gf_fclose(out);
				return GF_OK;
			}
			dur = (Double) sctx->dur;
			dur /= rep->timescale;
			gf_fprintf(out,"#EXTINF:%g,\n", dur);

			if (force_base_url)
				force_url = gf_url_concatenate(force_base_url, sctx->filename);

			gf_fprintf(out,"%s\n", force_url ? force_url : sctx->filename);

			if (force_url) {
				gf_free(force_url);
				force_url = NULL;
			}
		}
	} else {
		GF_MPD_BaseURL *base_url=NULL;
		GF_MPD_URL *init=NULL;
		
		if (rep->segment_base && rep->segment_base->initialization_segment) init = rep->segment_base->initialization_segment;
		if (as->segment_base && as->segment_base->initialization_segment) init = as->segment_base->initialization_segment;
		if (period->segment_base && period->segment_base->initialization_segment) init = period->segment_base->initialization_segment;

		if (rep->segment_list && rep->segment_list->initialization_segment) init = rep->segment_list->initialization_segment;
		if (as->segment_list && as->segment_list->initialization_segment) init = as->segment_list->initialization_segment;
		if (period->segment_list && period->segment_list->initialization_segment) init = period->segment_list->initialization_segment;

		base_url = gf_list_get(rep->base_URLs, 0);
		assert(base_url);

		if (init) {

			if (force_base_url)
				force_url = gf_url_concatenate(force_base_url, base_url->URL);

			if (init->byte_range) {
				gf_fprintf(out,"#EXT-X-MAP:URI=\"%s\",BYTERANGE=\"%d@"LLU"\"\n", force_url ? force_url : base_url->URL, (u32) (1+init->byte_range->end_range - init->byte_range->start_range), init->byte_range->start_range);
			} else {
				gf_fprintf(out,"#EXT-X-MAP:URI=\"%s\"\n", force_url ? force_url : base_url->URL);
			}

			if (force_url) {
				gf_free(force_url);
				force_url = NULL;
			}
		}

		for (i=0; i<count; i++) {
			Double dur;
			sctx = gf_list_get(rep->state_seg_list, i);
			assert(!sctx->filename);
			assert(sctx->file_size);

			dur = (Double) sctx->dur;
			dur /= rep->timescale;
			gf_fprintf(out,"#EXTINF:%g\n", dur);
			gf_fprintf(out,"#EXT-X-BYTERANGE:%d@"LLU"\n", sctx->file_size, sctx->file_offset);
			if (force_base_url)
				force_url = gf_url_concatenate(force_base_url, base_url->URL);

			gf_fprintf(out,"%s\n", force_url ? force_url : base_url->URL);

			if (force_url) {
				gf_free(force_url);
				force_url = NULL;
			}
		}
	}

	if (mpd->type != GF_MPD_TYPE_DYNAMIC)
		gf_fprintf(out,"\n#EXT-X-ENDLIST\n");

	if (close_file)
		gf_fclose(out);
	
	return GF_OK;
}


GF_Err gf_mpd_write_m3u8_master_playlist(GF_MPD const * const mpd, FILE *out, const char* m3u8_name, GF_MPD_Period *period)
{
	u32 i, j, hls_version;
	u32 var_idx;
	GF_Err e;
	GF_MPD_AdaptationSet *as;
	GF_MPD_Representation *rep;
	Bool use_range = GF_FALSE;
	Bool use_intra_only = GF_FALSE;
	Bool use_init = GF_FALSE;
	Bool use_ind_segments = GF_TRUE;
	Bool is_fmp4 = GF_FALSE;
	char *szVariantName;
	char *m3u8_name_rad, *sep, *force_base_url=NULL;
	u32 nb_audio=0;
	u32 nb_cc=0;
	u32 nb_subs=0;
	Bool has_muxed_comp = GF_FALSE;
	Bool has_video = GF_FALSE;
	Bool has_audio = GF_FALSE;

	if (!m3u8_name || !period) return GF_BAD_PARAM;

	i=0;
	while ( (as = (GF_MPD_AdaptationSet *) gf_list_enum(period->adaptation_sets, &i))) {
		if (gf_list_count(as->content_protection)) { /*use_crypt = GF_TRUE; */ }
		if (as->starts_with_sap>2) use_ind_segments = GF_FALSE;
		if (as->intra_only) use_intra_only = GF_TRUE;

		j=0;
		while ( (rep = (GF_MPD_Representation *) gf_list_enum(as->representations, &j))) {
			GF_DASH_SegmentContext *sctx;
			const char *init_seg;
			if (!rep->state_seg_list || !gf_list_count(rep->state_seg_list) ) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[M3U8] No segment state in representation, MPD cannot be translated to M3U8\n"));
				return GF_BAD_PARAM;
			}

			sctx = gf_list_get(rep->state_seg_list, 0);
			if (sctx && !sctx->filename) use_range = GF_TRUE;
			if (gf_list_count(rep->content_protection))  { /*use_crypt = GF_TRUE; */ }

			init_seg = gf_mpd_m3u8_get_init_seg(period, as, rep);
			if (init_seg) use_init = GF_TRUE;
			if (rep->mime_type) {
				if (strstr(rep->mime_type, "mp4")) is_fmp4 = GF_TRUE;
			}

			if (rep->streamtype==GF_STREAM_AUDIO) nb_audio++;
			else if (rep->streamtype==GF_STREAM_TEXT) nb_subs++;
		}
	}
	//we by default use floating point durations
	hls_version = 3;
	if (use_range) hls_version = 4;
	if (use_intra_only) hls_version = 5;
	if (is_fmp4 || use_init) hls_version = 6;

	gf_fprintf(out, "#EXTM3U\n");
	gf_fprintf(out, "#EXT-X-VERSION:%d\n", hls_version);
	if (use_ind_segments)
		gf_fprintf(out, "#EXT-X-INDEPENDENT-SEGMENTS\n");

	for (i=0; i<mpd->nb_hls_ext_master; i++)
		gf_fprintf(out, "%s\n", mpd->hls_ext_master[i]);
	gf_fprintf(out, "\n");

	if (!strncmp(m3u8_name, "gfio://", 7)) {
		const char *fpath = gf_fileio_translate_url(m3u8_name);
		if (!fpath) return GF_BAD_PARAM;
		//use basename since this file will be created throught GF_FileIO factory (relative to the original gfio://)
		m3u8_name_rad = gf_strdup(gf_file_basename(fpath));
	} else {
		m3u8_name_rad = gf_strdup(m3u8_name);
	}
	sep = strrchr(m3u8_name_rad, '.');
	if (sep) sep[0] = 0;
	szVariantName = gf_malloc(sizeof(char) * (100 + strlen(m3u8_name_rad)) );


	if (mpd->hls_abs_url) {
		GF_MPD_BaseURL *burl = gf_list_get(mpd->base_URLs, 0);
		if (burl)
			force_base_url = burl->URL;
	}

	//if live low lat, update parts dur
	Double max_part_dur_session=0;
	i=0;
	while ( (as = (GF_MPD_AdaptationSet *) gf_list_enum(period->adaptation_sets, &i))) {
		u32 j=0;
		GF_MPD_Representation *rep;
		if (!as->use_hls_ll) continue;

		while ( (rep = (GF_MPD_Representation *) gf_list_enum(as->representations, &j))) {
			u32 k=0;
			//figure out max part duration for this version of the playlist
			Double max_part_dur=0;
			GF_DASH_SegmentContext *sctx;
			while ( (sctx = (GF_DASH_SegmentContext *) gf_list_enum(rep->state_seg_list, &k))) {
				u32 nseg;

				//check max part dur for active ll segments, but use previously computed value
				//this avoids changing the HOLD-BACK too often which throws error in apple tools/safari
				Double dur = rep->hls_ll_part_dur;
				if (!sctx->llhls_mode || sctx->llhls_done) continue;
				for (nseg=0; nseg<sctx->nb_frags; nseg++) {
					dur = sctx->frags[nseg].duration;
					dur /= rep->timescale;

					if (dur>max_part_dur)
						max_part_dur = dur;
				}
			}
			if (!max_part_dur) max_part_dur = as->hls_ll_target_frag_dur;
			if (max_part_dur<rep->hls_ll_part_dur)
				max_part_dur = rep->hls_ll_part_dur;

			//default cdur in gpac
			if (!max_part_dur) {
				max_part_dur=1.0;
			}
			max_part_dur = ceil(max_part_dur*1000) / 1000.0;
			rep->hls_ll_part_dur = max_part_dur;
			if (max_part_dur_session < max_part_dur)
				max_part_dur_session = max_part_dur;
		}
	}

	//first pass, check if we have muxed components, or video or audio, generate playlists names
	var_idx = 1;
	i=0;
	while ( (as = (GF_MPD_AdaptationSet *) gf_list_enum(period->adaptation_sets, &i))) {
		if (as->content_component && gf_list_count(as->content_component)>1) {
			has_muxed_comp = GF_TRUE;
		}
		if (as->max_width && as->max_height) {
			has_video = GF_TRUE;
		}
		if (as->samplerate)
			has_audio = GF_TRUE;

		j=0;
		while ( (rep = (GF_MPD_Representation *) gf_list_enum(as->representations, &j))) {
			if (rep->mime_type) {
				if (!strncmp(rep->mime_type, "video/", 6)) has_video = GF_TRUE;
				else if (!strncmp(rep->mime_type, "audio/", 6)) has_audio = GF_TRUE;
			}

			if (!rep->m3u8_name) {
				sprintf(szVariantName, "%s_%d.m3u8",m3u8_name_rad, var_idx);
				if (rep->m3u8_var_name) gf_free(rep->m3u8_var_name);
				rep->m3u8_var_name = gf_strdup(szVariantName);
			}
			var_idx++;
		}
	}

	//second pass, generate all subplaylists
	i=0;
	while ( (as = (GF_MPD_AdaptationSet *) gf_list_enum(period->adaptation_sets, &i))) {
		j=0;
		while ( (rep = (GF_MPD_Representation *) gf_list_enum(as->representations, &j))) {
			if (!rep->state_seg_list || !gf_list_count(rep->state_seg_list) ) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[M3U8] No segment state in representation, MPD cannot be translated to M3U8, ignoring representation\n"));
				continue;
			}

			char *name = (char *) rep->m3u8_name;
			if (!name) {
				name = gf_file_basename(rep->m3u8_var_name);
			}

			e = gf_mpd_write_m3u8_playlist(mpd, period, as, rep, name, hls_version, max_part_dur_session,
				((mpd->hls_abs_url==1) || (mpd->hls_abs_url==3)) ? force_base_url : NULL);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[M3U8] IO error while opening m3u8 files\n"));
				return GF_IO_ERR;
			}
		}
	}

	//no muxed comp, no video, the audio is the main media we will list, force nb_audio=0 for gf_mpd_write_m3u8_playlist_tags
	if (!has_video && !has_muxed_comp)
		nb_audio = 0;

	//third pass, generate master playlists with the right groups
	i=0;
	while ( (as = (GF_MPD_AdaptationSet *) gf_list_enum(period->adaptation_sets, &i))) {
		Bool is_video = GF_FALSE;
		Bool is_audio = GF_FALSE;
		Bool is_muxed_comp = GF_FALSE;
		Bool is_primary = GF_TRUE;
		if (as->content_component && gf_list_count(as->content_component)>1) {
			is_muxed_comp = GF_TRUE;
		}
		if (as->max_width && as->max_height) {
			is_video = GF_TRUE;
		}
		if (as->samplerate) {
			is_audio = GF_TRUE;
		}

		//check if we have audio or video
		j=0;
		while ( (rep = (GF_MPD_Representation *) gf_list_enum(as->representations, &j))) {
			if (rep->mime_type) {
				if (!strncmp(rep->mime_type, "video/", 6)) is_video = GF_TRUE;
				else if (!strncmp(rep->mime_type, "audio/", 6)) is_audio = GF_TRUE;
				break;
			}
		}
		//figure out if this is the primary media or an associated media with the content
		if (has_muxed_comp) {
			if (!is_muxed_comp) is_primary = GF_FALSE;
		} else if (has_video) {
			if (!is_video) is_primary = GF_FALSE;
		} else if (has_audio) {
			if (!is_audio) is_primary = GF_FALSE;
		}

		j=0;
		while ( (rep = (GF_MPD_Representation *) gf_list_enum(as->representations, &j))) {
			char szSuffixName[GF_MAX_PATH+1];
			char *name = (char *) rep->m3u8_name;
			if (!rep->state_seg_list || !gf_list_count(rep->state_seg_list) ) {
				continue;
			}
			if (rep->m3u8_var_name) {
				name = gf_file_basename(rep->m3u8_var_name);
			}

			if (mpd->force_llhls_mode==2) {
				strcpy(szSuffixName, name);
				sep = gf_file_ext_start(szSuffixName);
				if (sep) sep[0] = 0;
				strcat(szSuffixName, "_IF");
				sep = gf_file_ext_start(name);
				if (sep)
					strcat(szSuffixName, sep);
				name = szSuffixName;
			}


			char *force_url = NULL;
			if (force_base_url && ((mpd->hls_abs_url==2) || (mpd->hls_abs_url==3)))
				force_url = gf_url_concatenate(force_base_url, name);

			gf_mpd_write_m3u8_playlist_tags(as, i, rep, out, force_url ? force_url : name, is_primary ? period : NULL, nb_audio, nb_subs, nb_cc);
			gf_fprintf(out, "\n");
			if (force_url) gf_free(force_url);
		}
	}
	gf_free(m3u8_name_rad);
	gf_free(szVariantName);
	return GF_OK;
}


#if 0 //unused
GF_Err gf_mpd_write_m3u8_file(GF_MPD *mpd, const char *file_name, GF_MPD_Period *period)
{
	GF_Err e;
	FILE *out;
	if (!strcmp(file_name, "std")) out = stdout;
	else {
		out = gf_fopen(file_name, "wb");
		if (!out) return GF_IO_ERR;

		mpd->create_m3u8_files = GF_TRUE;
	}

	e = gf_mpd_write_m3u8_master_playlist(mpd, out, file_name, period);
	gf_fclose(out);
	mpd->create_m3u8_files = GF_FALSE;
	return e;
}
#endif



GF_Err gf_mpd_write(GF_MPD const * const mpd, FILE *out, Bool compact)
{
	u32 i, count, child_idx;
	s32 indent = compact ? GF_INT_MIN : 0;
	GF_MPD_ProgramInfo *info;
	char *text;

	if (!mpd->xml_namespace) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD] No namespace found while writing. Setting to default.\n"));
	}

	gf_fprintf(out, "<?xml version=\"1.0\"?>");
	gf_mpd_lf(out, indent);
	mpd_write_generation_comment(mpd, out);
	gf_fprintf(out, "<MPD xmlns=\"%s\"", (mpd->xml_namespace ? mpd->xml_namespace : "urn:mpeg:dash:schema:mpd:2011"));

	if (mpd->write_context) {
	 	gf_fprintf(out, " xmlns:gpac=\"urn:gpac:filters:dasher:2018\"" );
	}

	if (mpd->ID)
		gf_fprintf(out, " id=\"%s\"", mpd->ID);

	if (mpd->min_buffer_time)
		gf_mpd_print_duration(out, "minBufferTime", mpd->min_buffer_time, GF_FALSE);

	gf_fprintf(out," type=\"%s\"",(mpd->type == GF_MPD_TYPE_STATIC ? "static" : "dynamic"));


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
	if ((s32) mpd->time_shift_buffer_depth > 0)
		gf_mpd_print_duration(out, "timeShiftBufferDepth", mpd->time_shift_buffer_depth, GF_TRUE);
	if (mpd->suggested_presentation_delay)
		gf_mpd_print_duration(out, "suggestedPresentationDelay", mpd->suggested_presentation_delay, GF_TRUE);
	if (mpd->max_segment_duration) 
		gf_mpd_print_duration(out, "maxSegmentDuration", mpd->max_segment_duration, GF_TRUE);
	if (mpd->max_subsegment_duration)
		gf_mpd_print_duration(out, "maxSubsegmentDuration", mpd->max_subsegment_duration, GF_TRUE);

	if (mpd->profiles) {
		gf_xml_dump_string(out, " profiles=\"", mpd->profiles, "\"");
	}

	gf_mpd_extensible_print_attr(out, mpd->x_attributes);

	if (mpd->write_context) {
		if (mpd->gpac_init_ntp_ms)
			gf_fprintf(out," gpac:init_gen_time=\""LLU"\"", mpd->gpac_init_ntp_ms);
		if (mpd->gpac_next_ntp_ms)
			gf_fprintf(out," gpac:next_gen_time=\""LLU"\"", mpd->gpac_next_ntp_ms);
		if (mpd->gpac_mpd_time)
			gf_fprintf(out," gpac:mpd_time=\""LLU"\"", mpd->gpac_mpd_time);
	}

	gf_fprintf(out, ">");
	gf_mpd_lf(out, indent);

	child_idx = 0;
	i=0;
	while ((info = (GF_MPD_ProgramInfo *)gf_list_enum(mpd->program_infos, &i))) {
		u32 sub_child_idx=0;
		gf_mpd_extensible_print_nodes(out, mpd->x_children, indent, &child_idx, GF_FALSE);

		gf_mpd_nl(out, indent+1);
		gf_fprintf(out, "<ProgramInformation");
		if (info->lang) {
			gf_fprintf(out, " lang=\"%s\"", info->lang);
		}
		if (info->more_info_url) {
			gf_xml_dump_string(out, " moreInformationURL=\"", info->more_info_url, "\"");
		}
		gf_mpd_extensible_print_attr(out, info->x_attributes);
		gf_fprintf(out, ">");

		gf_mpd_lf(out, indent);
		if (info->title) {
			gf_mpd_extensible_print_nodes(out, info->x_children, indent+1, &sub_child_idx, GF_FALSE);
			gf_mpd_nl(out, indent+2);
			gf_xml_dump_string(out, "<Title>", info->title, "</Title>");
			gf_mpd_lf(out, indent);
		}
		if (info->source) {
			gf_mpd_extensible_print_nodes(out, info->x_children, indent+1, &sub_child_idx, GF_FALSE);
			gf_mpd_nl(out, indent+2);
			gf_xml_dump_string(out, "<Source>", info->source, "</Source>");
			gf_mpd_lf(out, indent);
		}
		if (info->copyright) {
			gf_mpd_extensible_print_nodes(out, info->x_children, indent+1, &sub_child_idx, GF_FALSE);
			gf_mpd_nl(out, indent+2);
			gf_xml_dump_string(out, "<Copyright>", info->copyright, "</Copyright>");
			gf_mpd_lf(out, indent);
		}
		gf_mpd_extensible_print_nodes(out, info->x_children, indent+1, &sub_child_idx, GF_TRUE);
		gf_mpd_nl(out, indent+1);
		gf_fprintf(out, "</ProgramInformation>");
		gf_mpd_lf(out, indent);
	}

	gf_mpd_print_base_urls(out, mpd->base_URLs, indent+1);

	gf_mpd_lf(out, indent);

	i=0;
	while ((text = (char *)gf_list_enum(mpd->locations, &i))) {
		gf_mpd_extensible_print_nodes(out, mpd->x_children, indent, &child_idx, GF_FALSE);
		gf_mpd_nl(out, indent+1);
		gf_xml_dump_string(out, "<Location>", text, "</Location>");
		gf_mpd_lf(out, indent);
	}

	if (mpd->inject_service_desc) {
		gf_mpd_extensible_print_nodes(out, mpd->x_children, indent, &child_idx, GF_FALSE);
		gf_mpd_nl(out, indent+1);
		gf_fprintf(out, "<ServiceDescription id=\"0\">");
		gf_mpd_lf(out, indent);
		gf_mpd_nl(out, indent+2);
		gf_fprintf(out, "<Latency max=\"6000\" min=\"2000\" referenceId=\"0\" target=\"4000\"/>");
		gf_mpd_lf(out, indent);
		gf_mpd_nl(out, indent+2);
		gf_fprintf(out, "<PlaybackRate max=\"1.04\" min=\"0.96\"/>");
		gf_mpd_lf(out, indent);
		gf_mpd_nl(out, indent+1);
		gf_fprintf(out, "</ServiceDescription>");
		gf_mpd_lf(out, indent);
	}

	/*
		i=0;
		while ((text = (char *)gf_list_enum(mpd->metrics, &i))) {

		}
	*/

	count = gf_list_count(mpd->periods);
	for (i=0; i<count; i++) {
		Bool is_dynamic;
		GF_MPD_Period *period = (GF_MPD_Period *)gf_list_get(mpd->periods, i);
		is_dynamic = (mpd->type==GF_MPD_TYPE_DYNAMIC) ? GF_TRUE : GF_FALSE;
		//hack for backward compat with old arch, forces print period@start if 0
		if (!i && count>1 && mpd->was_dynamic) is_dynamic = GF_TRUE;

		gf_mpd_extensible_print_nodes(out, mpd->x_children, indent, &child_idx, GF_FALSE);
		gf_mpd_print_period(period, is_dynamic, out, mpd->write_context, indent+1);
	}

	if (gf_list_count(mpd->utc_timings)) {
		gf_mpd_extensible_print_nodes(out, mpd->x_children, indent, &child_idx, GF_FALSE);
		gf_mpd_print_descriptors(out, mpd->utc_timings, "UTCTiming", indent+1, mpd->x_children, &child_idx);
	}
	if (gf_list_count(mpd->essential_properties)) {
		gf_mpd_extensible_print_nodes(out, mpd->x_children, indent, &child_idx, GF_FALSE);
		gf_mpd_print_descriptors(out, mpd->essential_properties, "EssentialProperty", indent+1, mpd->x_children, &child_idx);
	}
	if (gf_list_count(mpd->supplemental_properties)) {
		gf_mpd_extensible_print_nodes(out, mpd->x_children, indent, &child_idx, GF_FALSE);
		gf_mpd_print_descriptors(out, mpd->supplemental_properties, "SupplementalProperty", indent+1, mpd->x_children, &child_idx);
	}
	gf_mpd_extensible_print_nodes(out, mpd->x_children, indent, &child_idx, GF_TRUE);

	gf_fprintf(out, "</MPD>");

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

	e = gf_mpd_write(mpd, out, GF_FALSE);
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

static char *gf_mpd_get_base_url(GF_List *baseURLs, char *parent_url, u32 *base_url_index)
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
		char *t_url = gf_url_concatenate(parent_url, url_child->redirection ? url_child->redirection : url_child->URL);
		gf_free(parent_url);
		parent_url = t_url;
	}
	return parent_url;
}

GF_EXPORT
GF_Err gf_mpd_resolve_url(GF_MPD *mpd, GF_MPD_Representation *rep, GF_MPD_AdaptationSet *set, GF_MPD_Period *period, const char *mpd_url, u32 base_url_index, GF_MPD_URLResolveType resolve_type, u32 item_index, u32 nb_segments_removed, char **out_url, u64 *out_range_start, u64 *out_range_end, u64 *segment_duration_in_ms, Bool *is_in_base_url, char **out_key_url, bin128 *out_key_iv, u32 *out_start_number)
{
	GF_MPD_SegmentTimeline *timeline = NULL;
	u32 start_number = 1;
	u32 timescale=0;
	u64 duration=0;
	u64 pto=0;
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

	if (resolve_type == GF_MPD_RESOLVE_URL_MEDIA_TEMPLATE_NO_BASE) {
		resolve_type = GF_MPD_RESOLVE_URL_MEDIA_TEMPLATE;
		url[0] = 0;
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
		case GF_MPD_RESOLVE_URL_MEDIA_NOSTART:
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
		case GF_MPD_RESOLVE_URL_MEDIA_NOSTART:
			if (!url) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[MPD] Media URL is not set in segment list\n"));
				return GF_SERVICE_ERROR;
			}
			if ((item_index >= segment_count) || ((s32) item_index < 0)) {
				gf_free(url);
				return GF_EOS;
			}
			*out_url = url;
			segment = gf_list_get(segments, item_index);

			if (segment->key_url && out_key_url) {
				//if ':', key URL is absolute or urn:
				if (strchr(segment->key_url+2, ':'))
					*out_key_url = gf_strdup(segment->key_url);
				//otherwise key url is relative to url (base url)
				else
					*out_key_url = gf_url_concatenate(url, segment->key_url);
				if (out_key_iv)
					memcpy((*out_key_iv), segment->key_iv, sizeof(bin128) );
			}

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
		pto = period->segment_template->presentation_time_offset;
	}
	if (set->segment_template) {
		if (set->segment_template->initialization) init_template = set->segment_template->initialization;
		if (set->segment_template->index) index_template = set->segment_template->index;
		if (set->segment_template->media) media_url = set->segment_template->media;
		if (set->segment_template->start_number != (u32) -1) start_number = set->segment_template->start_number;
		if (set->segment_template->segment_timeline) timeline = set->segment_template->segment_timeline;
		if (!timescale && set->segment_template->timescale) timescale = set->segment_template->timescale;
		if (!duration && set->segment_template->duration) duration = set->segment_template->duration;
		if (set->segment_template->presentation_time_offset) pto = set->segment_template->presentation_time_offset;
	}
	if (rep->segment_template) {
		if (rep->segment_template->initialization) init_template = rep->segment_template->initialization;
		if (rep->segment_template->index) index_template = rep->segment_template->index;
		if (rep->segment_template->media) media_url = rep->segment_template->media;
		if (rep->segment_template->start_number != (u32) -1) start_number = rep->segment_template->start_number;
		if (rep->segment_template->segment_timeline) timeline = rep->segment_template->segment_timeline;
		if (!timescale && rep->segment_template->timescale) timescale = rep->segment_template->timescale;
		if (!duration && rep->segment_template->duration) duration = rep->segment_template->duration;
		if (rep->segment_template->presentation_time_offset) pto = rep->segment_template->presentation_time_offset;
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
	case GF_MPD_RESOLVE_URL_MEDIA_NOSTART:
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
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[MPD] Missing ID on representation - cannot solve template\n\n"));
				gf_free(url);
				gf_free(solved_template);
				second_sep[0] = '$';
				return GF_NON_COMPLIANT_BITSTREAM;
			}
		}
		else if (!strcmp(first_sep+1, "Number")) {
			if (resolve_type==GF_MPD_RESOLVE_URL_MEDIA_TEMPLATE) {
				strcat(solved_template, "$Number");
				if (format_tag)
					strcat(solved_template, szPrintFormat);
				strcat(solved_template, "$");
			} else if (resolve_type==GF_MPD_RESOLVE_URL_MEDIA_NOSTART) {
				if (out_start_number) *out_start_number = 0;
				sprintf(szFormat, szPrintFormat, item_index);
				strcat(solved_template, szFormat);
			} else {
				if (out_start_number) *out_start_number = start_number;
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
			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD] Wrong template identifier Index detected - using Number instead\n\n"));
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
						if (ent->start_time)
							start_time = ent->start_time;
						if (k<nb_seg-1) {
							start_time +=  (u64) (ent->duration * (1 + ent->repeat_count));
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
					strcat(szPrintFormat, &LLU[1]);
					sprintf(szFormat, szPrintFormat, time);
					strcat(solved_template, szFormat);
					break;
				}
			} else if (duration) {
				u64 time = item_index * duration + pto;
				szPrintFormat[strlen(szPrintFormat)-1] = 0;
				strcat(szPrintFormat, &LLD[1]);
				sprintf(szFormat, szPrintFormat, time);
				strcat(solved_template, szFormat);
			}
		}
		else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[MPD] Unknown template identifier %s - disabling rep\n\n", first_sep+1));
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

	if (!period) return;

	if (out_segment_timeline) *out_segment_timeline = NULL;
	if (out_pts_offset) *out_pts_offset = 0;

	/*single media segment - duration is not known unless indicated in period*/
	if (rep->segment_base || set->segment_base || period->segment_base) {
		if (period->duration) {
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

		if (!pts_offset && period->segment_base && period->segment_base->presentation_time_offset) pts_offset = period->segment_base->presentation_time_offset;
		if (!timescale && period->segment_base && period->segment_base->timescale) timescale = period->segment_base->timescale;

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

	if (!rep->segment_template && !set->segment_template && !period->segment_template) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD] Representation without any SegmentBase, SegmentList or SegmentTemplate (non compliant). Assuming default SegmentBase\n"));
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

#if 0 //unused
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
#endif


GF_EXPORT
GF_Err gf_mpd_seek_in_period(Double seek_time, MPDSeekMode seek_mode,
	GF_MPD_Period const * const in_period, GF_MPD_AdaptationSet const * const in_set, GF_MPD_Representation const * const in_rep,
	u32 *out_segment_index, Double *out_opt_seek_time)
{
	Double seg_start = 0.0;
	u32 segment_idx = 0;

	if (!out_segment_index) {
		return GF_BAD_PARAM;
	}

	while (1) {
		Double segment_duration;
		u32 timescale=1000;
		u64 segment_duration_in_scale=0, seg_start_in_scale;

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

#if 0 //unused
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
#endif

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
        if (!strcmp(att->name, "r")) {
			chunk->repeat_count = atoi(att->value);
			//repeat count is one-based in smooth (number of repeats), 0-based in dash (number of additional repeats)
			if (chunk->repeat_count)
				chunk->repeat_count -= 1;
		}
        else if (!strcmp(att->name, "d"))
			chunk->duration = atoi(att->value);
        else if (!strcmp(att->name, "t")) {
			sscanf(att->value, LLU, &chunk->start_time);
		}
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
    char *szISOBMFFInit = NULL;

    GF_SAFEALLOC(rep, GF_MPD_Representation);
    if (!rep) return GF_OUT_OF_MEM;
    gf_mpd_init_common_attributes((GF_MPD_CommonAttributes *)rep);
    rep->base_URLs = gf_list_new();
    rep->sub_representations = gf_list_new();
    e = gf_list_add(container, rep);
    if (e) return e;

	gf_dynstrcat(&szISOBMFFInit, "isobmff://", NULL);


#define ISBMFFI_ADD_KEYWORD(_name, _value) \
	if (_value != NULL) {\
		gf_dynstrcat(&szISOBMFFInit, _name, NULL);\
		gf_dynstrcat(&szISOBMFFInit, "=", NULL);\
		gf_dynstrcat(&szISOBMFFInit, _value, NULL);\
		gf_dynstrcat(&szISOBMFFInit, " ", NULL);\
	}

#define ISBMFFI_ADD_KEYWORD_CONST(_name, _value) \
	gf_dynstrcat(&szISOBMFFInit, _name, NULL);\
	gf_dynstrcat(&szISOBMFFInit, "=", NULL);\
	gf_dynstrcat(&szISOBMFFInit, _value, NULL);\
	gf_dynstrcat(&szISOBMFFInit, " ", NULL);\


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
    //reserve space for max u64 as hex, without 0x prefix
    ISBMFFI_ADD_KEYWORD_CONST("tfdt", "0000000000000000")
    //create a url for the IS to be reconstructed
    rep->mime_type = gf_strdup(is_audio ? "audio/mp4" : "video/mp4");
    GF_SAFEALLOC(rep->segment_template, GF_MPD_SegmentTemplate);
    if (!rep->segment_template) return GF_OUT_OF_MEM;
    rep->segment_template->initialization = szISOBMFFInit;
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
    set->id = -1;
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
        else if (!strcmp(att->name, "Chunks")) {
			set->smooth_max_chunks = atoi(att->value);
		}
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
            if (!set->segment_template->segment_timeline) return GF_OUT_OF_MEM;
            
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
    u64 tsb = 0;

    if (!root || !mpd) return GF_BAD_PARAM;

	gf_mpd_init_struct(mpd);

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
        else if (!strcmp(att->name, "IsLive") && !stricmp(att->value, "true") )
			mpd->type = GF_MPD_TYPE_DYNAMIC;
        else if (!strcmp(att->name, "DVRWindowLength"))
			sscanf(att->value, LLU, &tsb);
//        else if (!strcmp(att->name, "LookaheadCount")) { }
    }
    mpd->media_presentation_duration = mpd->media_presentation_duration * 1000 / timescale;
    tsb *= 1000;
    tsb /= timescale;
    mpd->time_shift_buffer_depth = (u32) tsb;

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

GF_EXPORT
GF_Err gf_mpd_smooth_to_mpd(char * smooth_file, GF_MPD *mpd, const char *default_base_url)
{
	GF_DOMParser *dom = gf_xml_dom_new();
	GF_Err e = gf_xml_dom_parse(dom, smooth_file, NULL, 0);
	if (!e) {
		e = gf_mpd_init_smooth_from_dom(gf_xml_dom_get_root(dom), mpd, default_base_url);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[MPD] Failed to convert smooth manifest to MPD\n"));
		}
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[MPD] Failed to load smooth manifest\n"));
	}
	gf_xml_dom_del(dom);
	return e;
}
#define EXTRACT_FORMAT(_nb_chars)	\
			strcpy(szFmt, "%d");	\
			char_template+=_nb_chars;	\
			if (seg_rad_name[char_template]=='%') {	\
				char *sep = strchr(seg_rad_name+char_template, '$');	\
				if (sep) {	\
					sep[0] = 0;	\
					strcpy(szFmt, seg_rad_name+char_template);	\
					char_template += (u32) strlen(seg_rad_name+char_template);	\
					sep[0] = '$';	\
				}	\
			}	\
			char_template+=1;	\

GF_EXPORT
GF_Err gf_media_mpd_format_segment_name(GF_DashTemplateSegmentType seg_type, Bool is_bs_switching, char *segment_name, const char *rep_id, const char *base_url, const char *seg_rad_name, const char *seg_ext, u64 start_time, u32 bandwidth, u32 segment_number, Bool use_segment_timeline)
{
	Bool has_number= GF_FALSE;
	Bool force_path = GF_FALSE;
	if (seg_type==GF_DASH_TEMPLATE_TEMPLATE_WITH_PATH) {
		seg_type = GF_DASH_TEMPLATE_TEMPLATE;
		force_path = GF_TRUE;
	}
	if (seg_type==GF_DASH_TEMPLATE_REPINDEX_TEMPLATE_WITH_PATH) {
		seg_type = GF_DASH_TEMPLATE_REPINDEX_TEMPLATE;
		force_path = GF_TRUE;
	}

	Bool is_index = (seg_type==GF_DASH_TEMPLATE_REPINDEX) ? GF_TRUE : GF_FALSE;
	Bool is_init = (seg_type==GF_DASH_TEMPLATE_INITIALIZATION) ? GF_TRUE : GF_FALSE;
	Bool is_template = (seg_type==GF_DASH_TEMPLATE_TEMPLATE) ? GF_TRUE : GF_FALSE;
	Bool is_init_template = (seg_type==GF_DASH_TEMPLATE_INITIALIZATION_TEMPLATE) ? GF_TRUE : GF_FALSE;
	Bool is_index_template = (seg_type==GF_DASH_TEMPLATE_REPINDEX_TEMPLATE) ? GF_TRUE : GF_FALSE;
	Bool needs_init=((is_init || is_init_template) && !is_bs_switching) ? GF_TRUE : GF_FALSE;
	u32 has_init_keyword = 0;
	Bool needs_index = GF_FALSE;
	u32 char_template = 0;
	size_t seg_rad_name_len;

	char tmp[100];
	strcpy(segment_name, "");

	if (is_index_template) is_template = GF_TRUE;
	
	if (seg_type==GF_DASH_TEMPLATE_INITIALIZATION_SKIPINIT) {
		seg_type = GF_DASH_TEMPLATE_INITIALIZATION;
		needs_init = GF_FALSE;
		is_init = GF_TRUE;
	}
	if (seg_type==GF_DASH_TEMPLATE_INITIALIZATION_TEMPLATE_SKIPINIT) {
		seg_type = GF_DASH_TEMPLATE_INITIALIZATION_TEMPLATE;
		is_init_template = GF_TRUE;
		needs_init = GF_FALSE;
	}

	if (!seg_rad_name) return GF_BAD_PARAM;

	seg_rad_name_len = strlen(seg_rad_name);

	if (seg_type!=GF_DASH_TEMPLATE_REPINDEX) {
		if ( (is_index || is_index_template) && !strstr(seg_rad_name, "$Index")) {
			needs_index = GF_TRUE;
		}
	}
	if (strstr(seg_rad_name, "$RepresentationID$") || strstr(seg_rad_name, "$Bandwidth$"))
		needs_init = GF_FALSE;

	if (strstr(seg_rad_name, "$Init="))
		has_init_keyword = 1;
	if (strstr(seg_rad_name, "$XInit="))
		has_init_keyword = 2;

	while (char_template <= seg_rad_name_len) {
		char szFmt[20];
		char char_val = seg_rad_name[char_template];

		if (!is_template && !is_init_template && !strnicmp(& seg_rad_name[char_template], "$RepresentationID$", 18) ) {
			char_template += 18;
			strcat(segment_name, rep_id);
			needs_init = GF_FALSE;
		}
		else if (!is_template && !is_init_template && !strnicmp(& seg_rad_name[char_template], "$Bandwidth", 10)) {
			EXTRACT_FORMAT(10);

			sprintf(tmp, szFmt, bandwidth);
			strcat(segment_name, tmp);
			needs_init = GF_FALSE;
		}
		else if (!is_template && !strnicmp(& seg_rad_name[char_template], "$Time", 5)) {
			EXTRACT_FORMAT(5);
			if (is_init || is_init_template) {
				if (!has_init_keyword && needs_init) {
					strcat(segment_name, "init");
					needs_init = GF_FALSE;
				}
				continue;
			}
			/*replace %d to LLD*/
			szFmt[strlen(szFmt)-1]=0;
			strcat(szFmt, &LLD[1]);
			sprintf(tmp, szFmt, start_time);
			strcat(segment_name, tmp);
			has_number = GF_TRUE;
		}
		else if (!is_template && !strnicmp(& seg_rad_name[char_template], "$Number", 7)) {
			EXTRACT_FORMAT(7);

			if (is_init || is_init_template) {
				if (!has_init_keyword && needs_init) {
					strcat(segment_name, "init");
					needs_init = GF_FALSE;
				}
				continue;
			}
			sprintf(tmp, szFmt, segment_number);
			strcat(segment_name, tmp);
			has_number = GF_TRUE;
		}
		else if (!strnicmp(& seg_rad_name[char_template], "$Init=", 6)) {
			char *sep = strchr(seg_rad_name + char_template+6, '$');
			if (sep) sep[0] = 0;
			if (is_init || is_init_template) {
				strcat(segment_name, seg_rad_name + char_template+6);
				needs_init = GF_FALSE;
			}
			char_template += (u32) strlen(seg_rad_name + char_template)+1;
			if (sep) sep[0] = '$';
		}
		else if (!strnicmp(& seg_rad_name[char_template], "$XInit=", 7)) {
			char *sep = strchr(seg_rad_name + char_template+6, '$');
			if (sep) sep[0] = 0;
			if (is_init || is_init_template) {
				strcpy(segment_name, seg_rad_name + char_template+7);
				needs_init = GF_FALSE;
				if (sep) sep[0] = '$';
				break;
			}
			char_template += (u32) strlen(seg_rad_name + char_template)+1;
			if (sep) sep[0] = '$';
		}
		else if (!strnicmp(& seg_rad_name[char_template], "$Index=", 7)) {
			char *sep = strchr(seg_rad_name + char_template+7, '$');
			if (sep) sep[0] = 0;
			if (is_index) {
				strcat(segment_name, seg_rad_name + char_template+6);
				needs_index = GF_FALSE;
			}
			char_template += (u32) strlen(seg_rad_name + char_template)+1;
			if (sep) sep[0] = '$';
		}
		else if (!strnicmp(& seg_rad_name[char_template], "$Path=", 6)) {
			char *sep = strchr(seg_rad_name + char_template+6, '$');
			if (sep) sep[0] = 0;
			if (force_path || (!is_template && !is_init_template)) {
				strcat(segment_name, seg_rad_name + char_template+6);
			}
			char_template += (u32) strlen(seg_rad_name + char_template)+1;
			if (sep) sep[0] = '$';
		}
		else if (!strnicmp(& seg_rad_name[char_template], "$Segment=", 9)) {
			char *sep = strchr(seg_rad_name + char_template+9, '$');
			if (sep) sep[0] = 0;
			if (!is_init && !is_init_template) {
				strcat(segment_name, seg_rad_name + char_template+9);
			}
			char_template += (u32) strlen(seg_rad_name + char_template)+1;
			if (sep) sep[0] = '$';
		}

		else {
			char_template+=1;
			if (char_val=='\\') char_val = '/';

			sprintf(tmp, "%c", char_val);
			strcat(segment_name, tmp);
		}
	}

	if (is_template && !strstr(seg_rad_name, "$Number") && !strstr(seg_rad_name, "$Time")) {
		if (use_segment_timeline) {
			strcat(segment_name, "$Time$");
		} else {
			strcat(segment_name, "$Number$");
		}
	}

	if (needs_init)
		strcat(segment_name, "init");
	if (needs_index)
		strcat(segment_name, "idx");

	if (!is_init && !is_template && !is_init_template && !is_index && !has_number) {
		if (use_segment_timeline) {
			sprintf(tmp, LLU, start_time);
			strcat(segment_name, tmp);
		}
		else {
			sprintf(tmp, "%d", segment_number);
			strcat(segment_name, tmp);
		}
	}
	if (seg_ext) {
		strcat(segment_name, ".");
		strcat(segment_name, seg_ext);
	}

	if ((seg_type != GF_DASH_TEMPLATE_TEMPLATE)
		&& (seg_type != GF_DASH_TEMPLATE_SEGMENT)
		&& (seg_type != GF_DASH_TEMPLATE_INITIALIZATION)
		&& (seg_type != GF_DASH_TEMPLATE_INITIALIZATION_TEMPLATE)
	) {
		char *sep = strrchr(segment_name, '/');
		if (sep) {
			char cv = sep[0];
			sep[0] = 0;
			if (!gf_dir_exists(segment_name)) {
				gf_mkdir(segment_name);
			}
			sep[0] = cv;
		}
	}

	return GF_OK;
}


GF_Err gf_mpd_load_cues(const char *cues_file, u32 stream_id, u32 *cues_timescale, Bool *use_edit_list, s32 *ts_offset, GF_DASHCueInfo **out_cues, u32 *nb_cues)
{
	GF_XMLNode *root, *stream, *cue;
	GF_XMLAttribute *att;
	u32 i, j, k;
	GF_DOMParser *parser = gf_xml_dom_new();
	GF_Err e = gf_xml_dom_parse(parser, cues_file, NULL, NULL);
	if (e != GF_OK) {
		gf_xml_dom_del(parser);
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error loading cue file %s: %s\n", cues_file, gf_error_to_string(e)));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	root = gf_xml_dom_get_root(parser);
	if (e != GF_OK) {
		gf_xml_dom_del(parser);
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Error loading cue file, no root element found\n"));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	if (strcmp(root->name, "DASHCues")) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Wrong cue file, expecting DASHCues got %s\n", root->name));
		gf_xml_dom_del(parser);
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	i=0;
	while ((stream = gf_list_enum(root->content, &i))) {
		u32 id=0;
		u32 cur_cue;
		GF_DASHCueInfo *cues;
		u32 timescale=1000;
		if (stream->type != GF_XML_NODE_TYPE) continue;
		if (strcmp(stream->name, "Stream")) continue;

		*use_edit_list = GF_FALSE;
		*ts_offset = 0;
		j=0;
		while ((att = gf_list_enum(stream->attributes, &j))) {
			if (!strcmp(att->name, "id")) id = atoi(att->value);
			else if (!strcmp(att->name, "timescale")) timescale = atoi(att->value);
			else if (!strcmp(att->name, "mode") && !strcmp(att->value, "edit") ) *use_edit_list = GF_TRUE;
			else if (!strcmp(att->name, "ts_offset")) *ts_offset = atoi(att->value);
		}
		if (id != stream_id) continue;

		*cues_timescale = timescale;
		*nb_cues = 0;

		j=0;
		while ((cue = gf_list_enum(stream->content, &j))) {
			if (cue->type != GF_XML_NODE_TYPE) continue;
			if (strcmp(cue->name, "Cue")) continue;
			(*nb_cues)++;
		}
		cues = gf_malloc(sizeof(GF_DASHCueInfo)* (*nb_cues) );
		if (!cues) {
			gf_xml_dom_del(parser);
			GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] Failed to allocate %d cues\n", (*nb_cues) ));
			return GF_OUT_OF_MEM;
		}
		memset(cues, 0, sizeof(GF_DASHCueInfo)* (*nb_cues) );
		*out_cues = cues;

		j=0;
		cur_cue = 0;
		while ((cue = gf_list_enum(stream->content, &j))) {
			if (cue->type != GF_XML_NODE_TYPE) continue;
			if (strcmp(cue->name, "Cue")) continue;

			k=0;
			while ((att = gf_list_enum(cue->attributes, &k))) {
				if (!strcmp(att->name, "sample")) cues[cur_cue].sample_num = atoi(att->value);
				else if (!strcmp(att->name, "dts")) sscanf(att->value, LLD, &cues[cur_cue].dts);
				else if (!strcmp(att->name, "cts")) sscanf(att->value, LLD, &cues[cur_cue].cts);
			}
			cur_cue++;
		}


		break;
	}
	gf_xml_dom_del(parser);

	if (!stream) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASH] No cues found for requested stream %d\n", stream_id));
		return GF_BAD_PARAM;
	}
	return GF_OK;
}

GF_Err gf_mpd_split_adaptation_sets(GF_MPD *mpd)
{
	u32 i, nb_periods, next_as_id=0;
	if (!mpd) return GF_BAD_PARAM;

	nb_periods = gf_list_count(mpd->periods);
	for (i=0; i<nb_periods; i++) {
		u32 j, nb_as;
		GF_MPD_Period *period = gf_list_get(mpd->periods, i);
		nb_as = gf_list_count(period->adaptation_sets);
		for (j=0; j<nb_as; j++) {
			GF_MPD_AdaptationSet *set = gf_list_get(period->adaptation_sets, j);
			if (set->id > (s32) next_as_id)
				next_as_id = (u32) set->id;
		}
	}
	next_as_id++;

	for (i=0; i<nb_periods; i++) {
		u32 j, nb_as;
		GF_MPD_Period *period = gf_list_get(mpd->periods, i);
		GF_List *new_as = gf_list_new();

		nb_as = gf_list_count(period->adaptation_sets);
		for (j=0; j<nb_as; j++) {
			GF_MPD_AdaptationSet *set = gf_list_get(period->adaptation_sets, j);
			GF_List *reps = set->representations;
			u32 nb_reps = gf_list_count(set->representations);
			set->group = -1;

			gf_list_add(new_as, set);
			if (nb_reps<=1) {
				continue;
			}
			while (gf_list_count(set->representations)>1) {
				FILE *f = gf_file_temp(NULL);
				u32 size;
				char *data, szAdd[100];
				GF_Blob blob;
				GF_DOMParser *dom;
				GF_XMLNode *root;
				GF_MPD_Representation *rep = gf_list_get(reps, 1);
				gf_list_rem(reps, 1);
				set->representations = gf_list_new();
				gf_list_add(set->representations, rep);
				if (rep->dependency_id) {
					gf_free(rep->dependency_id);
					rep->dependency_id = NULL;
				}

				if (set->id) {
					set->id = next_as_id;
					next_as_id++;
				}

				//serialize
				gf_mpd_print_adaptation_set(set, f, GF_FALSE, 0, 0);
				size = (u32) gf_ftell(f);
				data = gf_malloc(size+1);
				gf_fseek(f, 0, SEEK_SET);
				size = (u32) gf_fread(data, size, f);
				data[size]=0;
				memset(&blob, 0, sizeof(GF_Blob));
				blob.data = data;
				blob.size = size;
				sprintf(szAdd, "gmem://%p", &blob);

				//parse
				dom = gf_xml_dom_new();
				gf_xml_dom_parse(dom, szAdd, NULL, NULL);
				root = gf_xml_dom_get_root(dom);
				gf_mpd_parse_adaptation_set(mpd, new_as, root);
				gf_xml_dom_del(dom);
				gf_free(data);
				gf_fclose(f);

				
				gf_mpd_representation_free(rep);
				gf_list_del(set->representations);
				set->representations = reps;
			}
		}
		gf_list_del(period->adaptation_sets);
		period->adaptation_sets = new_as;
	}
	return GF_OK;
}

GF_MPD_Descriptor *gf_mpd_get_descriptor(GF_List *desclist, char *scheme_id)
{
	u32 i, count = gf_list_count(desclist);
	for (i=0; i<count; i++) {
		GF_MPD_Descriptor *desc = gf_list_get(desclist, i);
		if (desc->scheme_id_uri && !strcmp(desc->scheme_id_uri, scheme_id)) return desc;
	}
	return NULL;
}


#endif /*GPAC_DISABLE_CORE_TOOLS*/
