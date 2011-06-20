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

#include <gpac/internal/mpd.h>
#include <gpac/download.h>
#include <gpac/internal/m3u8.h>
#include <gpac/network.h>

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
            rep->content_protection_type = gf_strdup(att->value);
        } else if (!strcmp(att->name, "schemeIdUri")) {
            rep->content_protection_uri = gf_strdup(att->value);
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

static GF_Err gf_mpd_parse_rep_initseg(GF_XMLNode *root, GF_MPD_Representation *rep, const char * baseURL)
{
    u32 att_index;
    GF_XMLAttribute *att;

    att_index = 0;
    while (1) {
        att = gf_list_get(root->attributes, att_index);
        if (!att) {
            break;
        } else if (!strcmp(att->name, "sourceURL")) {
            GF_URL_Info info;
            GF_Err e;
            gf_dm_url_info_init(&info);
            e = gf_dm_get_url_info(att->value, &info, baseURL);
            assert( e == GF_OK );
            assert( info.canonicalRepresentation);
            rep->init_url = gf_strdup(info.canonicalRepresentation);
            gf_dm_url_info_del(&info);
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
            rep->url_template = gf_strdup(att->value);
        } else if (!strcmp(att->name, "id")) {
            rep->id = gf_strdup(att->value);
        } else if (!strcmp(att->name, "startIndex")) {
            rep->startIndex = atoi(att->value)-1;
        } else if (!strcmp(att->name, "endIndex")) {
            rep->endIndex = atoi(att->value)-1;
        }
        att_index++;
    }
    return GF_OK;
}

static GF_Err gf_mpd_parse_rep_urlelt(GF_XMLNode *root, GF_MPD_SegmentInfo *seg, const char * baseURL)
{
    u32 att_index;
    GF_XMLAttribute *att;

    att_index = 0;
    while (1) {
        att = gf_list_get(root->attributes, att_index);
        if (!att) {
            break;
        } else if (!strcmp(att->name, "sourceURL")) {
            GF_URL_Info info;
            GF_Err e;
            gf_dm_url_info_init(&info);
            e = gf_dm_get_url_info(att->value, &info, baseURL);
            assert( e == GF_OK );
            assert( info.canonicalRepresentation);
            seg->url = gf_strdup(info.canonicalRepresentation);
            gf_dm_url_info_del(&info);
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
            rep->default_base_url = gf_strdup(att->value);
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
                gf_mpd_parse_rep_initseg(child, rep, rep->default_base_url);
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
	assert( !rep->segments );
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
					if (!seg_info) return GF_OUT_OF_MEM;
                    seg_info->byterange_end = 0;
                    seg_info->byterange_start = 0;
                    seg_info->use_byterange = 0;
                    seg_info->url = NULL;
                    gf_mpd_parse_rep_urlelt(child, seg_info, rep->default_base_url);
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
            rep->lang = gf_strdup(att->value);
        } else if (!strcmp(att->name, "mimeType")) {
            rep->mime = gf_strdup(att->value);
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
            period->default_base_url = gf_strdup(att->value);
        } else if (!strcmp(att->name, "sourceUrlTemplatePeriod")) {
            period->url_template = gf_strdup(att->value);
        }
        att_index++;
    }

    return GF_OK;
}

static GF_Err gf_mpd_parse_period(GF_XMLNode *root, GF_MPD_Period *period, const char *default_base_url)
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
				if (!rep)
					return GF_OUT_OF_MEM;
				if (default_base_url) {
					rep->default_base_url = gf_strdup(default_base_url);
				}
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
            mpd->more_info_url = gf_strdup(att->value);
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
                    mpd->title = gf_strdup(data_node->name);
                }
            } else if (!strcmp(child->name, "Source")) {
                GF_XMLNode *data_node = gf_list_get(child->content, 0);
                if (data_node && data_node->type == GF_XML_TEXT_TYPE) {
                    mpd->source = gf_strdup(data_node->name);
                }
            } else if (!strcmp(child->name, "Copyright")) {
                GF_XMLNode *data_node = gf_list_get(child->content, 0);
                if (data_node && data_node->type == GF_XML_TEXT_TYPE) {
                    mpd->copyright = gf_strdup(data_node->name);
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
                gf_free(seg);
            }
            if (rep->content_protection_type) gf_free(rep->content_protection_type);
            rep->content_protection_type = NULL;
            if (rep->content_protection_uri) gf_free(rep->content_protection_uri);
            rep->content_protection_uri = NULL;
            if (rep->default_base_url) gf_free(rep->default_base_url);
            rep->default_base_url = NULL;
            if (rep->id) gf_free(rep->id);
            rep->id = NULL;
            if (rep->init_url) gf_free(rep->init_url);
            rep->init_url = NULL;
            if (rep->lang) gf_free(rep->lang);
            rep->lang = NULL;
            if (rep->mime) gf_free(rep->mime);
            rep->mime = NULL;
            if (rep->url_template) gf_free(rep->url_template);
            rep->url_template = NULL;
            if (rep->segments) gf_list_del(rep->segments);
            rep->segments = NULL;
            gf_free(rep);
        }
        gf_list_del(period->representations);
        period->representations = NULL;
        if (period->default_base_url) gf_free(period->default_base_url);
        if (period->url_template) gf_free(period->url_template);

        gf_free(period);
    }
    gf_list_del(mpd->periods);
    mpd->periods = NULL;
    if (mpd->base_url) gf_free(mpd->base_url);
    mpd->base_url = NULL;
    if (mpd->title) gf_free(mpd->title);
    mpd->title = NULL;
    if (mpd->source) gf_free(mpd->source);
    mpd->source = NULL;
    if (mpd->copyright) gf_free(mpd->copyright);
    mpd->copyright = NULL;
    if (mpd->more_info_url) gf_free(mpd->more_info_url);
    mpd->more_info_url = NULL;
    gf_free(mpd);
}

GF_Err gf_mpd_init_from_dom(GF_XMLNode *root, GF_MPD *mpd, const char *default_base_url)
{
    u32 att_index, child_index;
    GF_XMLAttribute *att;
    GF_XMLNode *child;

    if (!root || !mpd) return GF_BAD_PARAM;

    assert( !mpd->periods );
    mpd->periods = gf_list_new();

    att_index = 0;
    child_index = gf_list_count(root->attributes);
    for (att_index = 0 ; att_index < child_index; att_index++) {
        att = gf_list_get(root->attributes, att_index);
        if (!att) {
            continue;
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
		if (!period)
			return GF_OUT_OF_MEM;
                period->representations = gf_list_new();
                gf_mpd_parse_period(child, period, default_base_url);
                gf_list_add(mpd->periods, period);
            }
        }
        child_index++;
    }
    return GF_OK;
}

GF_Err gf_m3u8_to_mpd(GF_ClientService *service, const char *m3u8_file, const char *base_url,
					  const char *mpd_file,
					  u32 reload_count, char *mimeTypeForM3U8Segments)
{
    GF_Err e;
    u32 i, count;
    Double update_interval;
    VariantPlaylist * pl = NULL;
    Program *prog;
    PlaylistElement *pe;
    FILE *fmpd;
    Bool is_end;

    e = parse_root_playlist(m3u8_file, &pl, base_url);
    if (e) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[M3U8] Failed to parse root playlist '%s', error = %s\n", m3u8_file, gf_error_to_string(e)));
        if (pl) variant_playlist_del(pl);
        pl = NULL;
        return e;
    }
    if (mpd_file == NULL){
	gf_delete_file(m3u8_file);
	mpd_file = m3u8_file;
    }
    is_end = !pl->playlistNeedsRefresh;
    i=0;
    assert( pl );
    assert( pl->programs );
    while ((prog = gf_list_enum(pl->programs, &i))) {
        u32 j=0;
		while (NULL != (pe = gf_list_enum(prog->bitrates, &j))) {
			Bool found = 0;
			u32 k;
			char *suburl;
			
			if (!pe->url || !strstr(pe->url, ".m3u8")) 
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

            suburl = gf_url_concatenate(base_url, pe->url);
            if (!strcmp(base_url, suburl)) {
                gf_free(suburl);
                GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD Generator] Not downloading, programs are identical for %s...\n", pe->url));
                continue;
            }
			if (service) {
                GF_DownloadSession *sess = gf_term_download_new(service, suburl, GF_NETIO_SESSION_NOT_THREADED, NULL, NULL);
                if (!sess) {
                    gf_free(suburl);
                    break;
                }
                e = gf_dm_sess_process(sess);
                if (e==GF_OK) {
                    e = parse_sub_playlist(gf_dm_sess_get_cache_name(sess), &pl, suburl, prog, pe);
                }
                gf_term_download_del(sess);
                gf_free(suburl);
			} else { /* for use in MP4Box */
				extern GF_Err gf_dm_wget(const char *url, const char *filename);
				e = gf_dm_wget(suburl, "tmp.m3u8");
                if (e==GF_OK) {
                    e = parse_sub_playlist("tmp.m3u8", &pl, suburl, prog, pe);
                }
				gf_delete_file("tmp.m3u8");
			}
        }
    }

    update_interval = 0;
    prog = gf_list_get(pl->programs, 0);
    assert( prog );
    pe = gf_list_last(prog->bitrates);
    assert( pe );
    /*update interval is set to the duration of the last media file with rules defined in http live streaming RFC section 6.3.4*/
    switch (reload_count) {
    case 0:
        update_interval = pe->durationInfo;
        break;
    case 1:
        update_interval = pe->durationInfo/2;
        break;
    case 2:
        update_interval = 3*(pe->durationInfo/2);
        break;
    default:
        update_interval = 3*(pe->durationInfo);
        break;
    }
    if (is_end || ((pe->elementType == TYPE_PLAYLIST) && pe->element.playlist.is_ended)) {
        update_interval = 0;
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD Generator] NO NEED to refresh playlist !\n"));
    } else {
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD Generator] Playlist will be refreshed every %g seconds, len=%d\n", update_interval, pe->durationInfo));
    }

    assert( mpd_file );
    fmpd = gf_f64_open(mpd_file, "wt");
    if (!fmpd){
	GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD Generator] Cannot write to temp file %s!\n", mpd_file));
	variant_playlist_del(pl);
	return GF_IO_ERR;
    }
    fprintf(fmpd, "<MPD type=\"Live\" xmlns=\"urn:mpeg:mpegB:schema:DASH:MPD:DIS2011\" profiles=\"urn:mpeg:mpegB:profile:dash:full:2011\"");
    if (update_interval) fprintf(fmpd, " minimumUpdatePeriodMPD=\"PT%02.2gS\"", update_interval);
    fprintf(fmpd, ">\n");

    fprintf(fmpd, " <ProgramInformation moreInformationURL=\"http://gpac.sourceforge.net\">\n");
    {
        char * title = pe->title;
        if (!title || strlen(title) < 2)
            title = pe->url;
        fprintf(fmpd, "  <Title>%s</Title>\n", title );
    }
    fprintf(fmpd, "  <Copyright>Generated from URL %s</Copyright>\n", base_url );
    fprintf(fmpd, "  <Source>Generated by GPAC %s from %s</Source>\n", GPAC_FULL_VERSION, base_url);

    fprintf(fmpd, " </ProgramInformation>\n");
    fprintf(fmpd, " <Period start=\"PT0S\">\n");

    count = gf_list_count(pl->programs);
    for (i=0; i<count; i++) {
        u32 j, count2;
        Program *prog = gf_list_get(pl->programs, i);
        count2 = gf_list_count(prog->bitrates);
        for (j = 0; j<count2; j++) {
            PlaylistElement *pe = gf_list_get(prog->bitrates, j);
            if (pe->elementType == TYPE_PLAYLIST) {
                u32 k, count3;
				char *base_url = gf_strdup(pe->url);
				char *sep = strrchr(base_url, '/');
				if (sep) *(sep+1) = 0;
				if (pe->codecs && (pe->codecs[0] = '\"')) {
					u32 len = strlen(pe->codecs);
					strncpy(pe->codecs, pe->codecs+1, len-1);
					pe->codecs[len-2] = 0;
				}
				/* SOUCHAY : if mime-type is still unknown, do not try to add codec information since it would be wrong */
				if (!strcmp(M3U8_UNKOWN_MIME_TYPE, mimeTypeForM3U8Segments)){
					fprintf(fmpd, "  <Representation mimeType=\"%s\" bandwidth=\"%d\">\n", mimeTypeForM3U8Segments, pe->bandwidth);
				} else {
					fprintf(fmpd, "  <Representation mimeType=\"%s%s%s\" bandwidth=\"%d\">\n", mimeTypeForM3U8Segments, (pe->codecs ? ";codecs=":""), (pe->codecs ? pe->codecs:""), pe->bandwidth);
				}
                fprintf(fmpd, "\n   <SegmentInfo duration=\"PT%dS\" baseURL=\"%s\">\n", pe->durationInfo, base_url);
                count3 = gf_list_count(pe->element.playlist.elements);
                update_interval = (count3 - 1) * pe->durationInfo * 1000;
                for (k=0; k<count3; k++) {
					u32 cmp = 0;
					char *src_url, *seg_url;
                    PlaylistElement *elt = gf_list_get(pe->element.playlist.elements, k);

					/*remove protocol scheme and try to find the common part in baseURL and segment URL - this avoids copying the entire url*/
					src_url = strstr(base_url, "://");
					if (src_url) src_url += 3;
					else src_url = base_url;

					seg_url = strstr(elt->url, "://");
					if (seg_url) seg_url += 3;
					else seg_url = elt->url;

					while (src_url[cmp] == seg_url[cmp]) cmp++;
					fprintf(fmpd, "    <Url sourceURL=\"%s\"/>\n", cmp ? (seg_url + cmp) : elt->url);
                }
                fprintf(fmpd, "   </SegmentInfo>\n");
                fprintf(fmpd, "  </Representation>\n");
				gf_free(base_url);
            } else if (pe->elementType == TYPE_STREAM) {
                fprintf(stdout, "NOT SUPPORTED: M3U8 Stream\n");
            }
        }
    }
    fprintf(fmpd, " </Period>\n");
    fprintf(fmpd, "</MPD>");
    fclose(fmpd);
    variant_playlist_del(pl);
    return GF_OK;
}

