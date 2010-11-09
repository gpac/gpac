/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2010-
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

#include "mpd.h"

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
            if (sep1 = strchr(duration+i+2, 'H')) {
                *sep1 = 0;
                h = atoi(duration+i+2);
                *sep1 = 'H';
                sep1++;
            } else {
                sep1 = duration+i+2;
            }
            if (sep2 = strchr(sep1, 'M')) {
                *sep2 = 0;
                m = atoi(sep1);
                *sep2 = 'M';
                sep2++;
            } else {
                sep2 = sep1;
            }
            if (sep1 = strchr(sep2, 'S')) {
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

static GF_Err gf_mpd_parse_rep_cp(GF_XMLNode *root, GF_MPD_Representation *rep)
{
    u32 att_index;
    GF_XMLAttribute *att;
    att_index = 0;
    while (1) {
        att = gf_list_get(root->attributes, att_index);
        if (!att) {
            break;
        } else if (!strcmp(att->name, "SchemeInformation")) {
            rep->content_protection_type = strdup(att->value);
        } else if (!strcmp(att->name, "schemeIdUri")) {
            rep->content_protection_uri = strdup(att->value);
        }
        att_index++;
    }

    return GF_OK;
}

static GF_Err gf_mpd_parse_rep_trickmode(GF_XMLNode *root, GF_MPD_Representation *rep)
{
    u32 att_index;
    GF_XMLAttribute *att;
    att_index = 0;
    while (1) {
        att = gf_list_get(root->attributes, att_index);
        if (!att) {
            break;
        } else if (!strcmp(att->name, "alternatePlayoutRate")) {
            rep->alternatePlayoutRate = atof(att->value);
        }
        att_index++;
    }

    return GF_OK;
}

static GF_Err gf_mpd_parse_rep_initseg(GF_XMLNode *root, GF_MPD_Representation *rep)
{
    u32 att_index;
    GF_XMLAttribute *att;
    
    att_index = 0;
    while (1) {
        att = gf_list_get(root->attributes, att_index);
        if (!att) {
            break;
        } else if (!strcmp(att->name, "sourceURL")) {
            rep->init_url = strdup(att->value);
        } else if (!strcmp(att->name, "range")) {
            rep->init_use_range = 1;
            sscanf(att->value, "%d-%d", &rep->init_byterange_start, &rep->init_byterange_end);
        }
        att_index++;
    }
    return GF_OK;
}

static GF_Err gf_mpd_parse_rep_urltemplate(GF_XMLNode *root, GF_MPD_Representation *rep)
{
    u32 att_index;
    GF_XMLAttribute *att;
    
    att_index = 0;
    while (1) {
        att = gf_list_get(root->attributes, att_index);
        if (!att) {
            break;
        } else if (!strcmp(att->name, "sourceURL")) {
            rep->url_template = strdup(att->value);
        } else if (!strcmp(att->name, "id")) {
            rep->id = strdup(att->value);
        } else if (!strcmp(att->name, "startIndex")) {
            rep->startIndex = atoi(att->value)-1;
        } else if (!strcmp(att->name, "endIndex")) {
            rep->endIndex = atoi(att->value)-1;
        }
        att_index++;
    }
    return GF_OK;
}

static GF_Err gf_mpd_parse_rep_urlelt(GF_XMLNode *root, GF_MPD_SegmentInfo *seg)
{
    u32 att_index;
    GF_XMLAttribute *att;
    
    att_index = 0;
    while (1) {
        att = gf_list_get(root->attributes, att_index);
        if (!att) {
            break;
        } else if (!strcmp(att->name, "sourceURL")) {
            seg->url = strdup(att->value);
        } else if (!strcmp(att->name, "range")) {
			seg->use_byterange = 1;
            sscanf(att->value, "%d-%d", &seg->byterange_start, &seg->byterange_end);
        }
        att_index++;
    }
    return GF_OK;
}

static GF_Err gf_mpd_parse_rep_segmentinfo(GF_XMLNode *root, GF_MPD_Representation *rep)
{
    u32 att_index, child_index;
    u32 nb_urlelements;
    GF_XMLAttribute *att;
    GF_XMLNode *child;
    
    att_index = 0;
    while (1) {
        att = gf_list_get(root->attributes, att_index);
        if (!att) {
            break;
        } else if (!strcmp(att->name, "duration")) {
            rep->default_segment_duration = gf_mpd_parse_duration(att->value);
        } else if (!strcmp(att->name, "baseURL")) {
            rep->default_base_url = strdup(att->value);
        }
        att_index++;
    }

    child_index = 0;
    nb_urlelements = 0;
    while (1) {
        child = gf_list_get(root->content, child_index);
        if (!child) {
            break;
        } else if (child->type == GF_XML_NODE_TYPE) {
            if (!strcmp(child->name, "InitialisationSegmentURL")) {
                gf_mpd_parse_rep_initseg(child, rep);
            } else if (!strcmp(child->name, "UrlTemplate")) {
                gf_mpd_parse_rep_urltemplate(child, rep);
            } else if (!strcmp(child->name, "Url")) {
                nb_urlelements++;
            }
        }
        child_index++;
    }

    /* We assume that URL Template has precedence over URL elements */
    if (rep->url_template) {
        /* TODO: expand the template to create segment urls*/
    } else if (nb_urlelements) {
        u32 urlelt_index = 0;

		rep->segments = gf_list_new();

		child_index = 0;
        while (1) {
            child = gf_list_get(root->content, child_index);
            if (!child) {
                break;
            } else if (child->type == GF_XML_NODE_TYPE) {
                if (!strcmp(child->name, "Url")) {
					GF_MPD_SegmentInfo *seg_info;
					GF_SAFEALLOC(seg_info, GF_MPD_SegmentInfo);
					gf_mpd_parse_rep_urlelt(child, seg_info);
					gf_list_add(rep->segments, seg_info);
                    urlelt_index++;
                }
            }
            child_index++;
        }
    }
    return GF_OK;
}

static GF_Err gf_mpd_parse_representation(GF_XMLNode *root, GF_MPD_Representation *rep)
{
    u32 att_index, child_index;
    GF_XMLAttribute *att;
    GF_XMLNode *child;
    
    att_index = 0;
    while (1) {
        att = gf_list_get(root->attributes, att_index);
        if (!att) {
            break;
        } else if (!strcmp(att->name, "bandwidth")) {
            rep->bandwidth = atoi(att->value);
        } else if (!strcmp(att->name, "width")) {
            rep->width = atoi(att->value);
        } else if (!strcmp(att->name, "height")) {
            rep->height = atoi(att->value);
        } else if (!strcmp(att->name, "lang")) {
            rep->lang = strdup(att->value);
        } else if (!strcmp(att->name, "mimeType")) {
            rep->mime = strdup(att->value);
        } else if (!strcmp(att->name, "group")) {
            rep->groupID = atoi(att->value);
        } else if (!strcmp(att->name, "startWithRAP")) {
            if (!strcmp(att->value, "true")) rep->startWithRap = 1;
        } else if (!strcmp(att->name, "qualityRanking")) {
            rep->qualityRanking = atoi(att->value);
        }
        att_index++;
    }

    child_index = 0;
    while (1) {
        child = gf_list_get(root->content, child_index);
        if (!child) {
            break;
        } else if (child->type == GF_XML_NODE_TYPE) {
            if (!strcmp(child->name, "ContentProtection")) {
                gf_mpd_parse_rep_cp(child, rep);
            } else if (!strcmp(child->name, "TrickMode")) {
                gf_mpd_parse_rep_trickmode(child, rep);
            } else if (!strcmp(child->name, "SegmentInfo")) {
                gf_mpd_parse_rep_segmentinfo(child, rep);
            }
        }
        child_index++;
    }
    return GF_OK;
}

static GF_Err gf_mpd_parse_segment_info_default(GF_XMLNode *root, GF_MPD_Period *period)
{
    u32 att_index;
    GF_XMLAttribute *att;
    
    att_index = 0;
    while (1) {
        att = gf_list_get(root->attributes, att_index);
        if (!att) {
            break;
        } else if (!strcmp(att->name, "duration")) {
            period->default_segment_duration = gf_mpd_parse_duration(att->value);
        } else if (!strcmp(att->name, "baseURL")) {
            period->default_base_url = strdup(att->value);
        } else if (!strcmp(att->name, "sourceUrlTemplatePeriod")) {
            period->url_template = strdup(att->value);
        }
        att_index++;
    }

    return GF_OK;
}

static GF_Err gf_mpd_parse_period(GF_XMLNode *root, GF_MPD_Period *period)
{
    u32 att_index, child_index;
    GF_XMLAttribute *att;
    GF_XMLNode *child;
    
    att_index = 0;
    while (1) {
        att = gf_list_get(root->attributes, att_index);
        if (!att) {
            break;
        } else if (!strcmp(att->name, "start")) {
        } else if (!strcmp(att->name, "segmentAlignmentFlag")) {
            if (!strcmp(att->value, "true")) {
                period->segment_alignment_flag = 1;
            } 
        } else if (!strcmp(att->name, "bitstreamSwitchingFlag")) {
            if (!strcmp(att->value, "true")) {
                period->segment_alignment_flag = 1;
            } 
        }
        att_index++;
    }

    child_index = 0;
    while (1) {
        child = gf_list_get(root->content, child_index);
        if (!child) {
            break;
        } else if (child->type == GF_XML_NODE_TYPE) {
            if (!strcmp(child->name, "SegmentInfoDefault")) {
                gf_mpd_parse_segment_info_default(child, period);
            } else if (!strcmp(child->name, "Representation")) {
				GF_MPD_Representation *rep;
				GF_SAFEALLOC(rep, GF_MPD_Representation);
				gf_mpd_parse_representation(child, rep);
				gf_list_add(period->representations, rep);
            }
        }
        child_index++;
    }

    return GF_OK;
}

static GF_Err gf_mpd_parse_program_info(GF_XMLNode *root, GF_MPD *mpd)
{
    u32 att_index, child_index;
    GF_XMLAttribute *att;
    GF_XMLNode *child;

    att_index = 0;
    while (1) {
        att = gf_list_get(root->attributes, att_index);
        if (!att) {
            break;
        } else if (!strcmp(att->name, "moreInformationURL")) {
            mpd->more_info_url = strdup(att->value);
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
                    mpd->title = strdup(data_node->name);
                }
            } else if (!strcmp(child->name, "Source")) {
                GF_XMLNode *data_node = gf_list_get(child->content, 0);
                if (data_node && data_node->type == GF_XML_TEXT_TYPE) {
                    mpd->source = strdup(data_node->name);
                }
            } else if (!strcmp(child->name, "Copyright")) {
                GF_XMLNode *data_node = gf_list_get(child->content, 0);
                if (data_node && data_node->type == GF_XML_TEXT_TYPE) {
                    mpd->copyright = strdup(data_node->name);
                }
            }
        }
        child_index++;
    }
    return GF_OK;
}

GF_MPD *gf_mpd_new()
{
	GF_MPD *mpd;
	GF_SAFEALLOC(mpd, GF_MPD);
	mpd->periods = gf_list_new();
	return mpd;
}

void gf_mpd_del(GF_MPD *mpd)
{
	while (gf_list_count(mpd->periods)) {
		GF_MPD_Period *period = gf_list_get(mpd->periods, 0);
		gf_list_rem(mpd->periods, 0);
		while (gf_list_count(period->representations)) {
			GF_MPD_Representation *rep = gf_list_get(period->representations, 0);
			gf_list_rem(period->representations, 0);

			while (gf_list_count(rep->segments)) {
				GF_MPD_SegmentInfo *seg = gf_list_get(rep->segments, 0);
				gf_list_rem(rep->segments, 0);
				if (seg->url) gf_free(seg->url);
				free(seg);
			}
			if (rep->content_protection_type) gf_free(rep->content_protection_type);
			if (rep->content_protection_uri) gf_free(rep->content_protection_uri);
			if (rep->default_base_url) gf_free(rep->default_base_url);
			if (rep->id) gf_free(rep->id);
			if (rep->init_url) gf_free(rep->init_url);
			if (rep->lang) gf_free(rep->lang);
			if (rep->mime) gf_free(rep->mime);
			if (rep->url_template) gf_free(rep->url_template);
			gf_free(rep);
		}
		gf_free(period->representations);
		if (period->default_base_url) gf_free(period->default_base_url);
		if (period->url_template) gf_free(period->url_template);

		gf_free(period);
	}
	gf_list_del(mpd->periods);
	if (mpd->base_url) gf_free(mpd->base_url);
	if (mpd->title) gf_free(mpd->title);
	if (mpd->source) gf_free(mpd->source);
	if (mpd->copyright) gf_free(mpd->copyright);
	if (mpd->more_info_url) gf_free(mpd->more_info_url);
}

GF_Err gf_mpd_init_from_dom(GF_XMLNode *root, GF_MPD *mpd)
{
    u32 att_index, child_index;
    GF_XMLAttribute *att;
    GF_XMLNode *child;
    
    if (!root || !mpd) return GF_BAD_PARAM;

	memset(mpd, 0, sizeof(GF_MPD));
    mpd->periods = gf_list_new();

    att_index = 0;
    while (1) {
        att = gf_list_get(root->attributes, att_index);
        if (!att) {
            break;
        } else if (!strcmp(att->name, "type")) {
            if (!strcmp(att->value, "OnDemand")) mpd->type = GF_MPD_TYPE_ON_DEMAND;
            else if (!strcmp(att->value, "Live")) mpd->type = GF_MPD_TYPE_LIVE;
        } else if (!strcmp(att->name, "availabilityStartTime")) {
        } else if (!strcmp(att->name, "availabilityEndTime")) {
        } else if (!strcmp(att->name, "mediaPresentationDuration")) {
            mpd->duration = gf_mpd_parse_duration(att->value);
        } else if (!strcmp(att->name, "minimumUpdatePeriodMPD")) {
            mpd->min_update_time = gf_mpd_parse_duration(att->value);
        } else if (!strcmp(att->name, "minBufferTime")) {
            mpd->min_buffer_time = gf_mpd_parse_duration(att->value);
        } else if (!strcmp(att->name, "timeShiftBufferDepth")) {
            mpd->time_shift_buffer_depth = gf_mpd_parse_duration(att->value);
        } else if (!strcmp(att->name, "baseURL")) {
        }
        att_index++;
    }

    child_index = 0;
    while (1) {
        child = gf_list_get(root->content, child_index);
        if (!child) {
            break;
        } else if (child->type == GF_XML_NODE_TYPE) {
            if (!strcmp(child->name, "ProgramInformation")) {
                gf_mpd_parse_program_info(child, mpd);
            } else if (!strcmp(child->name, "Period")) {
                GF_MPD_Period *period;
                GF_SAFEALLOC(period, GF_MPD_Period);
				period->representations = gf_list_new();
                gf_mpd_parse_period(child, period);
                gf_list_add(mpd->periods, period);
            }
        }
        child_index++;
    }
    return GF_OK;
}
