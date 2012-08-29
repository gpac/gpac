/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato, Jean Le Feuvre
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

#include <gpac/modules/service.h>
#include <gpac/internal/terminal_dev.h>
#include <gpac/thread.h>
#include <gpac/network.h>
#include <gpac/crypt.h>
#include <gpac/internal/mpd.h>
#include <gpac/internal/m3u8.h>
#include <string.h>

/*set to 1 if you want MPD to use SegmentTemplate if possible instead of SegmentList*/
#define M3U8_TO_MPD_USE_TEMPLATE	0

/*!
 * All the possible Mime-types for MPD files
 */
static const char * MPD_MIME_TYPES[] = { "application/dash+xml", "video/vnd.3gpp.mpd", "audio/vnd.3gpp.mpd", NULL };

/*!
 * All the possible Mime-types for M3U8 files
 */
static const char * M3U8_MIME_TYPES[] = { "video/x-mpegurl", "audio/x-mpegurl", "application/x-mpegurl", "application/vnd.apple.mpegurl", NULL};

typedef enum {
	MPD_STATE_STOPPED = 0,
	MPD_STATE_RUNNING,
	MPD_STATE_CONNECTING,
} MPD_STATE;

GF_Err MPD_downloadWithRetry( GF_ClientService * service, GF_DownloadSession ** sess, const char *url, gf_dm_user_io user_io,  void *usr_cbk, u64 start_range, u64 end_range, Bool persistent);

typedef struct
{
    char *cache;
    char *url;
	u64 start_range, end_range;
	/*representation index in adaptation_set->representations*/
	u32 representation_index;
	Bool do_not_delete;
} segment_cache_entry;

/*this structure Group is the implementation of the adaptationSet element of the MPD.*/
typedef struct __mpd_group 
{
	/*pointer to adaptation set*/
	GF_MPD_AdaptationSet *adaptation_set;
	/*pointer to active period*/
	GF_MPD_Period *period;

	/*active representation index in adaptation_set->representations*/
	u32 active_rep_index;

	u32 prev_active_rep_index;

	Bool bitstream_switching, dont_delete_first_segment;

	Bool selected;
	Bool done;
	Bool force_switch_bandwidth, min_bandwidth_selected;
	u32 nb_bw_check;
	u32 active_bitrate, max_bitrate, min_bitrate;

	u32 nb_segments_in_rep;
	Double segment_duration;

	/*local file playback, do not delete them*/
	Bool local_files;
	/*next segment to download for this group*/
	u32 download_segment_index;

	/*next file (cached) to delete at next GF_NET_SERVICE_QUERY_NEXT for this group*/
    char * urlToDeleteNext;
    volatile u32 max_cached_segments, nb_cached_segments;
    segment_cache_entry *cached;

    GF_DownloadSession *segment_dnload;
    const char *segment_local_url;
	/*usually 0-0 (no range) but can be non-zero when playing local MPD/DASH sessions*/
	u64 local_url_start_range, local_url_end_range;

    u32 nb_segments_done;

    Bool segment_must_be_streamed;

	/* Service really managing the segments */
    GF_InputService *input_module;
    char *service_mime;
    Bool service_connected, service_descriptor_fetched;

	struct __mpd_module *mpd_in;

	u32 force_representation_idx_plus_one;

	Bool force_segment_switch;

	/*set when switching segment, indicates the current downloaded segment duration*/
    u64 current_downloaded_segment_duration;
} GF_MPD_Group;

typedef struct __mpd_module {
    /* GPAC Service object (i.e. how this module is seen by the terminal)*/
    GF_ClientService *service;
    /* URL to which this service is connected
       Used to detect when audio service connection request is made on the same URL as video */
    char *url;

	/*interface to mpd parser*/
	GF_FileDownload getter;

    u32 option_max_cached;
    u32 auto_switch_count;
    Bool keep_files, disable_switching;
	/*0: min bandwith 1: max bandwith 2: min quality 3: max quality*/
	u32 first_select_mode;

	/* MPD downloader*/
    GF_DownloadSession *mpd_dnload;
	/* MPD */
    GF_MPD *mpd;
	/* number of time the MPD has been reloaded and last update time*/
    u32 reload_count, last_update_time;
	/*signature of last MPD*/
    u8 lastMPDSignature[20];
	/*mime used by M3U8 server*/
    char *mimeTypeForM3U8Segments;

	/* active period in MPD (only one currently supported) */
    u32 active_period_index;
	u32 request_period_switch;

	u64 start_time_in_active_period;

	/*list of groups in the active period*/
	GF_List *groups;
	/*group 0 if present, NULL otherwise*/
	GF_MPD_Group *group_zero_selected;

    /*Main MPD Thread handling segment downloads and MPD/M3U8 update*/
    GF_Thread *mpd_thread;
	/*mutex for group->cache file name access and MPD update*/
    GF_Mutex *dl_mutex;

    /* 0 not started, 1 download in progress */
    MPD_STATE mpd_is_running;
    Bool mpd_stop_request;
	Bool in_period_setup;

    /* TODO - handle playback status for SPEED/SEEK through SIDX */
    Double playback_speed, playback_start_range, previous_start_range;
	Double start_range_in_segment_at_next_period;
	Bool in_seek;
} GF_MPD_In;

void MPD_ResetGroups(GF_MPD_In *mpdin);
GF_Err MPD_SetupPeriod(GF_MPD_In *mpdin);



static const char *MPD_GetMimeType(GF_MPD_SubRepresentation *subrep, GF_MPD_Representation *rep, GF_MPD_AdaptationSet *set)
{
	if (subrep && subrep->mime_type) return subrep->mime_type;
	if (rep && rep->mime_type) return rep->mime_type;
	if (set && set->mime_type) return set->mime_type;
	return NULL;
}


static Bool MPD_CheckRootType(const char *local_url)
{
    if (local_url) {
        char *rtype = gf_xml_get_root_type(local_url, NULL);
        if (rtype) {
            Bool handled = 0;
            if (!strcmp(rtype, "MPD")) {
                handled = 1;
            }
            gf_free(rtype);
            return handled;
        }
    }
    return 0;
}

/**
 * NET IO for MPD, we don't need this anymore since mime-type can be given by session
 */
void MPD_NetIO_Segment(void *cbk, GF_NETIO_Parameter *param)
{
    GF_Err e;
	u32 download_rate;
    GF_MPD_Group *group= (GF_MPD_Group*) cbk;

    /*handle service message*/
    gf_term_download_update_stats(group->segment_dnload);
	if (group->done) {
		gf_dm_sess_abort(group->segment_dnload);
		return;
	}	
	
    if ((param->msg_type == GF_NETIO_PARSE_HEADER) && !strcmp(param->name, "Content-Type")) {
		if (!group->service_mime) {
			group->service_mime = gf_strdup(param->value);
		} else if (stricmp(group->service_mime, param->value)) {
			GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);
			if (! MPD_GetMimeType(NULL, rep, group->adaptation_set) ) rep->mime_type = gf_strdup(param->value);
			rep->disabled = 1;
			GF_LOG(GF_LOG_WARNING, GF_LOG_MODULE,
				("[MPD_IN] Disabling representation since mime does not match: expected %s, but had %s for %s!\n", group->service_mime, param->value, gf_dm_sess_get_resource_name(group->segment_dnload)));
			group->force_switch_bandwidth = 1;
			gf_dm_sess_abort(group->segment_dnload);
			return;
		}
	}

	e = param->error;
	if (param->msg_type == GF_NETIO_PARSE_REPLY) {
		if (! gf_dm_sess_can_be_cached_on_disk(group->segment_dnload)) {
			GF_LOG(GF_LOG_INFO, GF_LOG_MODULE,
				("[MPD_IN] Segment %s cannot be cached on disk, will use direct streaming\n", gf_dm_sess_get_resource_name(group->segment_dnload)));
			group->segment_must_be_streamed = 1;
			gf_dm_sess_abort(group->segment_dnload);
		} else {
			group->segment_must_be_streamed = 0;
		}
	}
	else if ((param->msg_type == GF_NETIO_DATA_EXCHANGE) || (param->msg_type == GF_NETIO_DATA_TRANSFERED)) {
		if (!group->mpd_in->disable_switching && (gf_dm_sess_get_stats(group->segment_dnload, NULL, NULL, NULL, NULL, &download_rate, NULL) == GF_OK)) {
			if (download_rate) {
				download_rate *= 8;
				if (download_rate<group->min_bitrate) group->min_bitrate = download_rate;
				if (download_rate>group->max_bitrate) group->max_bitrate = download_rate;

				if (download_rate && (download_rate < group->active_bitrate)) {
					u32 set_idx = gf_list_find(group->period->adaptation_sets, group->adaptation_set)+1;
					group->nb_bw_check ++;
					if (group->min_bandwidth_selected) {
						fprintf(stdout, "Downloading from set #%d at rate %d kbps but media bitrate is %d kbps - no lower bitrate available ...\n", set_idx, download_rate/1024, group->active_bitrate/1024);
					} else if (group->nb_bw_check>2) {
						fprintf(stdout, "Downloading from set #%d at rate %d kbps but media bitrate is %d kbps - switching\n", set_idx, download_rate/1024, group->active_bitrate/1024);
						group->force_switch_bandwidth = 1;
						gf_dm_sess_abort(group->segment_dnload);
					} else {
						fprintf(stdout, "Downloading from set #%ds at rate %d kbps but media bitrate is %d kbps\n", set_idx, download_rate/1024, group->active_bitrate/1024);
					}
				} else {
					group->nb_bw_check = 0;
				}
			}
		}
	}
}

/*!
 * Returns true if given mime type is a MPD file
 * \param mime the mime-type to check
 * \return true if mime-type if MPD-OK
 */
static Bool MPD_is_MPD_mime(const char * mime) {
    u32 i;
    if (!mime)
        return 0;
    for (i = 0 ; MPD_MIME_TYPES[i] ; i++){
        if ( !stricmp(mime, MPD_MIME_TYPES[i]))
            return 1;
    }
    return 0;
}

/*!
 * Returns true if mime type is an M3U8 mime-type
 * \param mime The mime-type to check
 * \return true if mime-type is OK for M3U8
 */
static Bool MPD_isM3U8_mime(const char * mime) {
    u32 i;
    if (!mime)
        return 0;
    for (i = 0 ; M3U8_MIME_TYPES[i] ; i++) {
        if ( !stricmp(mime, M3U8_MIME_TYPES[i]))
            return 1;
    }
    return 0;
}

void MPD_NetIO(void *cbk, GF_NETIO_Parameter *param)
{
    GF_Err e;
    GF_MPD_In *mpdin = (GF_MPD_In*) cbk;

    /*handle service message*/
    gf_term_download_update_stats(mpdin->mpd_dnload);
    e = param->error;
}

static GF_Err MPD_UpdatePlaylist(GF_MPD_In *mpdin)
{
    GF_Err e;
    u32 group_idx, rep_idx, i, j;
    Bool seg_found = 0;
    GF_DOMParser *mpd_parser;
    GF_MPD_Period *period, *new_period;
    const char *local_url;
    char mime[128];
    char * purl;
    Bool is_m3u8 = 0;
	u32 oldUpdateTime = mpdin->mpd->minimum_update_period;
    /*reset update time - if any error occurs, we will no longer attempt to update the MPD*/
    mpdin->mpd->minimum_update_period = 0;

    if (!mpdin->mpd_dnload) {
		if (!gf_list_count(mpdin->mpd->locations)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot update playlist: no HTTP source for MPD could be found\n"));
			return GF_BAD_PARAM;
		}
		purl = gf_strdup(gf_list_get(mpdin->mpd->locations, 0));
	} else {
		local_url = gf_dm_sess_get_cache_name(mpdin->mpd_dnload);
		if (!local_url) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot update playlist: wrong cache file %s\n", local_url));
			return GF_IO_ERR;
		}
		gf_delete_file(local_url);
		purl = gf_strdup(gf_dm_sess_get_resource_name(mpdin->mpd_dnload));
	}

	/*if update location is specified, update - spec does not say whether location is a relative or absoute URL*/
	if (gf_list_count(mpdin->mpd->locations)) {
		char *update_loc = gf_list_get(mpdin->mpd->locations, 0);
		char *update_url = gf_url_concatenate(purl, update_loc);
		if (update_url) {
			gf_free(purl);
			purl = update_url;
		}
	}

    GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Updating Playlist %s...\n", purl));
	/*use non-persistent connection for MPD*/
    e = MPD_downloadWithRetry(mpdin->service, &(mpdin->mpd_dnload), purl, MPD_NetIO, mpdin, 0, 0, 0);
    if (e!=GF_OK) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot update playlist: download problem %s for MPD file\n", gf_error_to_string(e)));
        gf_free(purl);
        return gf_dm_sess_last_error(mpdin->mpd_dnload);
    } else {
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Playlist %s updated with success\n", purl));
    }
    strncpy(mime, gf_dm_sess_mime_type(mpdin->mpd_dnload), sizeof(mime));
    strlwr(mime);

    /*in case the session has been restarted, local_url may have been destroyed - get it back*/
    local_url = gf_dm_sess_get_cache_name(mpdin->mpd_dnload);

    /* Some servers, for instance http://tv.freebox.fr, serve m3u8 as text/plain */
    if (MPD_isM3U8_mime(mime) || strstr(purl, ".m3u8")) {
        gf_m3u8_to_mpd(local_url, purl, NULL, mpdin->reload_count, mpdin->mimeTypeForM3U8Segments, 0, M3U8_TO_MPD_USE_TEMPLATE, &mpdin->getter);
    } else if (!MPD_is_MPD_mime(mime)) {
        GF_LOG(GF_LOG_WARNING, GF_LOG_MODULE, ("[MPD_IN] mime '%s' should be m3u8 or mpd\n", mime));
    }

    gf_free(purl);
    purl = NULL;

    if (!MPD_CheckRootType(local_url)) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot update playlist: MPD file type is not correct %s\n", local_url));
        return GF_NON_COMPLIANT_BITSTREAM;
    }
    {
        u8 signature[sizeof(mpdin->lastMPDSignature)];
        if (gf_sha1_file( local_url, signature)) {
            GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] : cannot SHA1 file %s\n", local_url));
        } else {
            if (! memcmp( signature, mpdin->lastMPDSignature, sizeof(mpdin->lastMPDSignature))) {
                GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] MPD file did not change\n"));
                mpdin->reload_count++;
                mpdin->mpd->minimum_update_period = oldUpdateTime;
            } else {
                GF_MPD *new_mpd;
                mpdin->reload_count = 0;
                memccpy(mpdin->lastMPDSignature, signature, sizeof(char), sizeof(mpdin->lastMPDSignature));

                /* It means we have to reparse the file ... */
                /* parse the MPD */
                mpd_parser = gf_xml_dom_new();
                e = gf_xml_dom_parse(mpd_parser, local_url, NULL, NULL);
                if (e != GF_OK) {
                    gf_xml_dom_del(mpd_parser);
                    GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot update playlist: error in XML parsing %s\n", gf_error_to_string(e)));
                    return GF_NON_COMPLIANT_BITSTREAM;
                }
                new_mpd = gf_mpd_new();
                e = gf_mpd_init_from_dom(gf_xml_dom_get_root(mpd_parser), new_mpd, purl);
                gf_xml_dom_del(mpd_parser);
                if (e) {
                    GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot update playlist: error in MPD creation %s\n", gf_error_to_string(e)));
                    gf_mpd_del(new_mpd);
                    return GF_NON_COMPLIANT_BITSTREAM;
                }

                /*TODO - check periods are the same*/
                period = gf_list_get(mpdin->mpd->periods, mpdin->active_period_index);
                new_period = gf_list_get(new_mpd->periods, mpdin->active_period_index);
                if (!new_period) {
                    GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot update playlist: missing period\n"));
                    gf_mpd_del(new_mpd);
                    return GF_NON_COMPLIANT_BITSTREAM;
                }

				if (gf_list_count(period->adaptation_sets) != gf_list_count(new_period->adaptation_sets)) {
                    GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot update playlist: missing AdaptationSet\n"));
                    gf_mpd_del(new_mpd);
                    return GF_NON_COMPLIANT_BITSTREAM;
                }

				for (group_idx=0; group_idx<gf_list_count(mpdin->groups); group_idx++) {
					GF_MPD_AdaptationSet *set, *new_set;
					GF_MPD_Group *group = gf_list_get(mpdin->groups, group_idx);
					if (!group->selected) continue;
					set = group->adaptation_set;
					new_set = gf_list_get(new_period->adaptation_sets, group_idx);

					if (gf_list_count(new_set->representations) != gf_list_count(group->adaptation_set->representations)) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot update playlist: missing representation in adaptation set\n"));
						gf_mpd_del(new_mpd);
						return GF_NON_COMPLIANT_BITSTREAM;
					}
					
					/*get all representations in both periods*/
					for (rep_idx = 0; rep_idx <gf_list_count(group->adaptation_set->representations); rep_idx++) {
						GF_List *segments, *new_segments;
						GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, rep_idx);
						GF_MPD_Representation *new_rep = gf_list_get(new_set->representations, rep_idx);

						if (rep->segment_base || group->adaptation_set->segment_base || period->segment_base) {
							if (!new_rep->segment_base && !new_set->segment_base && !new_period->segment_base) {
								GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot update playlist: representation does not use segment base as previous version\n"));
								gf_mpd_del(new_mpd);
								return GF_NON_COMPLIANT_BITSTREAM;
							}
							/*what else should we check ??*/

							/*OK, this rep is fine*/
						}

						else if (rep->segment_template || group->adaptation_set->segment_template || period->segment_template) {
							if (!new_rep->segment_template && !new_set->segment_template && !new_period->segment_template) {
								GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot update playlist: representation does not use segment template as previous version\n"));
								gf_mpd_del(new_mpd);
								return GF_NON_COMPLIANT_BITSTREAM;
							}
							/*what else should we check ??*/

							/*OK, this rep is fine*/
						}

						else {
							/*we're using segment list*/
							assert(rep->segment_list || group->adaptation_set->segment_list || period->segment_list);

							if (!new_rep->segment_list && !new_set->segment_list && !new_period->segment_list) {
								GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot update playlist: representation does not use segment list as previous version\n"));
								gf_mpd_del(new_mpd);
								return GF_NON_COMPLIANT_BITSTREAM;
							}
							/*what else should we check ??*/

							/*get the segment list*/
							segments = new_segments = NULL;
							if (period->segment_list && period->segment_list->segment_URLs) segments = period->segment_list->segment_URLs;
							if (set->segment_list && set->segment_list->segment_URLs) segments = set->segment_list->segment_URLs;
							if (rep->segment_list && rep->segment_list->segment_URLs) segments = rep->segment_list->segment_URLs;

							if (new_period->segment_list && new_period->segment_list->segment_URLs) new_segments = new_period->segment_list->segment_URLs;
							if (new_set->segment_list && new_set->segment_list->segment_URLs) new_segments = new_set->segment_list->segment_URLs;
							if (new_rep->segment_list && new_rep->segment_list->segment_URLs) new_segments = new_rep->segment_list->segment_URLs;


							for (i=0; i<gf_list_count(new_segments); i++) {
								GF_MPD_SegmentURL *new_seg = gf_list_get(new_segments, i);
								Bool found = 0;
								for (j=0; j<gf_list_count(segments); j++) {
									GF_MPD_SegmentURL *seg = gf_list_get(segments, j);
									if (seg->media && new_seg->media && !strcmp(seg->media, new_seg->media)) {
										found=1;
										break;
									}
									if (seg->media_range && new_seg->media_range && (seg->media_range->start_range==new_seg->media_range->start_range) && (seg->media_range->end_range==new_seg->media_range->end_range) ) {
										found=1;
										break;
									}
								}
								/*this is a new segment, merge it: we remove from new list and push to old one, before doing a final swap
								this ensures that indexing in the segment_list is still correct after merging*/
								if (!found) {
									gf_list_rem(new_segments, i);
									i--;
									gf_list_add(segments, new_seg);
									GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Representation #%d: Adding new segment %s\n", rep_idx+1, new_seg->media));
								}
							}

							/*what else should we check ?*/

							/*swap segment list content*/
							gf_list_swap(new_segments, segments);

							/*current representation is the active one in the group - update the number of segments*/
							if (group->active_rep_index==rep_idx) {
								group->nb_segments_in_rep = gf_list_count(new_segments);
							}
						}

						/*copy over a few things from former rep*/
						new_rep->disabled = rep->disabled;
						if (!new_rep->mime_type) {
							new_rep->mime_type = rep->mime_type;
							rep->mime_type = NULL;
						}
					}
					/*update group/period to new period*/
					j = gf_list_find(group->period->adaptation_sets, group->adaptation_set);
					group->adaptation_set = gf_list_get(new_period->adaptation_sets, j);
					group->period = new_period;
				}
                /*swap representations - we don't need to update download_segment_index as it still points to the right entry in the merged list*/
                if (mpdin->mpd)
                    gf_mpd_del(mpdin->mpd);
                mpdin->mpd = new_mpd;
                mpdin->last_update_time = gf_sys_clock();
            }
        }
    }

    return GF_OK;
}

static GF_Err MPD_ClientQuery(GF_InputService *ifce, GF_NetworkCommand *param)
{
	u32 i;
	GF_MPD_Group *group = NULL;
	GF_MPD_In *mpdin = (GF_MPD_In *) ifce->proxy_udta;
    if (!param || !ifce || !ifce->proxy_udta) return GF_BAD_PARAM;

	if (param->command_type==GF_NET_SERVICE_QUERY_INIT_RANGE) {
		param->url_query.next_url = NULL;
		param->url_query.start_range = 0;
		param->url_query.end_range = 0;
		
		mpdin->in_seek = 0;

		for (i=0; i<gf_list_count(mpdin->groups); i++) {
			group = gf_list_get(mpdin->groups, i);
			if (group->selected && (group->input_module == ifce)) break;
			group = NULL;
		}
		
		if (!group) return GF_SERVICE_ERROR;
		param->url_query.start_range = group->local_url_start_range;
		param->url_query.end_range = group->local_url_end_range;
		
		return GF_OK;
	}
	if (param->command_type==GF_NET_SERVICE_QUERY_NEXT) {
		Bool discard_first_cache_entry = 1;
        u32 timer = gf_sys_clock();
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Service Query Next request from terminal\n"));
        gf_mx_p(mpdin->dl_mutex);


		param->url_query.discontinuity_type = 0;
		if (mpdin->in_seek) {
			mpdin->in_seek = 0;
			param->url_query.discontinuity_type = 2;
			discard_first_cache_entry = 0;
		}

		for (i=0; i<gf_list_count(mpdin->groups); i++) {
			group = gf_list_get(mpdin->groups, i);
			if (group->selected && (group->input_module == ifce)) break;
			group = NULL;
		}
		
		if (!group) {
	        gf_mx_v(mpdin->dl_mutex);
			return GF_SERVICE_ERROR;
		}
		group->force_segment_switch = 0;

        /* Wait until no file is scheduled to be downloaded */
        while (mpdin->mpd_is_running && group->nb_cached_segments<2) {
            gf_mx_v(mpdin->dl_mutex);
			if (group->done) {
				if (!mpdin->request_period_switch && (mpdin->active_period_index+1 < gf_list_count(mpdin->mpd->periods))) {
					GF_NetworkCommand com;
					memset(&com, 0, sizeof(GF_NetworkCommand));
					com.command_type = GF_NET_BUFFER_QUERY;
					while (mpdin->request_period_switch != 1) {
						gf_term_on_command(mpdin->service, &com, GF_OK);
						if (!com.buffer.occupancy) {
							mpdin->request_period_switch = 1;
							break;
						}
						gf_sleep(20);
					}
				} 
				return GF_EOS;
			}
            gf_sleep(16);
            gf_mx_p(mpdin->dl_mutex);
        }
        if (group->nb_cached_segments<2) {
            GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[MPD_IN] No more file in cache, EOS\n"));
            gf_mx_v(mpdin->dl_mutex);
            return GF_EOS;
        } else {
            GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Had to wait for %u ms for the only cache file to be downloaded\n", (gf_sys_clock() - timer)));
        }

		if (discard_first_cache_entry) {
			if (group->cached[0].cache) {
				if (group->urlToDeleteNext) {
					if (!group->local_files && !mpdin->keep_files)
						gf_dm_delete_cached_file_entry_session(group->segment_dnload, group->urlToDeleteNext);

					gf_free( group->urlToDeleteNext);
					group->urlToDeleteNext = NULL;
				}
				assert( group->cached[0].url );

				if (group->dont_delete_first_segment) {
					group->dont_delete_first_segment = 0;
				} else {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] deleting cache file %s : %s\n", group->cached[0].url, group->cached[0].cache));
					group->urlToDeleteNext = gf_strdup( group->cached[0].url );
				}

				gf_free(group->cached[0].cache);
				gf_free(group->cached[0].url);
				group->cached[0].url = NULL;
				group->cached[0].cache = NULL;
				group->cached[0].representation_index = 0;
			}
			memmove(&group->cached[0], &group->cached[1], sizeof(segment_cache_entry)*(group->nb_cached_segments-1));
			memset(&(group->cached[group->nb_cached_segments-1]), 0, sizeof(segment_cache_entry));
			group->nb_cached_segments--;
		}

        param->url_query.next_url = group->cached[0].cache;
		param->url_query.start_range = group->cached[0].start_range;
		param->url_query.end_range = group->cached[0].end_range;

		if (group->cached[0].representation_index != group->prev_active_rep_index) {
			GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, group->cached[0].representation_index);
			param->url_query.next_url_init_or_switch_segment = rep->cached_init_segment_url;
			param->url_query.switch_start_range = rep->init_start_range;
			param->url_query.switch_end_range = rep->init_end_range;
		} else {
			param->url_query.next_url_init_or_switch_segment = NULL;
			param->url_query.switch_start_range = param->url_query.switch_end_range = 0;
		}
		group->prev_active_rep_index = group->cached[0].representation_index;

        gf_mx_v(mpdin->dl_mutex);
        {
            u32 timer2 = gf_sys_clock() - timer ;
            if (timer2 > 1000) {
                GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] We were stuck waiting for download to end during too much time : %u ms !\n", timer2));
            }
			if (group->cached[0].end_range) {
				GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[MPD_IN] Switching segment playback to %s (Media Range: "LLD"-"LLD")\n", group->cached[0].url, group->cached[0].start_range, group->cached[0].end_range));
			} else {
	            GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[MPD_IN] Switching segment playback to %s\n", group->cached[0].url));
			}
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Waited %d ms - Elements in cache: %u/%u\n\tCache URL: %s\n", timer2, group->nb_cached_segments, group->max_cached_segments, group->cached[0].cache));
        }
    } else {
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Client Query request (%d) from terminal\n", param->command_type));
    }
    return GF_OK;
}

static GF_Err MPD_LoadMediaService(GF_MPD_In *mpdin, GF_MPD_Group *group, const char *mime, const char *init_segment_name)
{
	u32 i;
    const char *sPlug;
	if (mime) {
		/* Check MIME type to start the right InputService */
		sPlug = gf_cfg_get_key(mpdin->service->term->user->config, "MimeTypes", mime);
		if (sPlug) sPlug = strrchr(sPlug, '"');
		if (sPlug) {
			sPlug += 2;
			group->input_module = (GF_InputService *) gf_modules_load_interface_by_name(mpdin->service->term->user->modules, sPlug, GF_NET_CLIENT_INTERFACE);
			if (group->input_module) {
				group->input_module->proxy_udta = mpdin;
				group->input_module->query_proxy = MPD_ClientQuery;
				return GF_OK;
			}
		}
	}
	if (init_segment_name) {
		for (i=0; i< gf_modules_get_count(mpdin->service->term->user->modules); i++) {
			GF_InputService *ifce = (GF_InputService *) gf_modules_load_interface(mpdin->service->term->user->modules, i, GF_NET_CLIENT_INTERFACE);
			if (!ifce) continue;
			
			if (ifce->CanHandleURL && ifce->CanHandleURL(ifce, init_segment_name)) {
				group->input_module = ifce;
				group->input_module->proxy_udta = mpdin;
				group->input_module->query_proxy = MPD_ClientQuery;
				return GF_OK;
			}
			gf_modules_close_interface((GF_BaseInterface *) ifce);
		}
	}

	GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error locating plugin for segment - mime type %s - name %s\n", mime, init_segment_name));
    return GF_CODEC_NOT_FOUND;
}

/*!
 * Download a file with possible retry if GF_IP_CONNECTION_FAILURE|GF_IP_NETWORK_FAILURE
 * (I discovered that with my WIFI connection, I had many issues with BFM-TV downloads)
 * Similar to gf_term_download_new() and gf_dm_sess_process().
 * Parameters are identical to the ones of gf_term_download_new.
 * \see gf_term_download_new()
 */
GF_Err MPD_downloadWithRetry( GF_ClientService * service, GF_DownloadSession **sess, const char *url, gf_dm_user_io user_io,  void *usr_cbk, u64 start_range, u64 end_range, Bool persistent)
{
	Bool had_sess = 0;
    GF_Err e;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Downloading %s...\n", url));

	if (! *sess) {
		u32 flags = GF_NETIO_SESSION_NOT_THREADED;
		if (persistent) flags |= GF_NETIO_SESSION_PERSISTENT; 
		*sess = gf_term_download_new(service, url, flags, user_io, usr_cbk);
		if (!(*sess)){
			assert(0);
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Cannot try to download %s... OUT of memory ?\n", url));
			return GF_OUT_OF_MEM;
		}
	} else {
		had_sess = 1;
		e = gf_dm_sess_setup_from_url(*sess, url);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Cannot resetup session for url %s: %s\n", url, gf_error_to_string(e) ));
			return e;
		}

	}
	if (end_range) {
		e = gf_dm_sess_set_range(*sess, start_range, end_range);
		if (e) {
			if (had_sess) {
				gf_term_download_del(*sess);
				*sess = NULL;
				return MPD_downloadWithRetry(service, sess, url, user_io, usr_cbk, start_range, end_range, persistent);
			}


			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Cannot setup byte-range download for %s: %s\n", url, gf_error_to_string(e) ));
			return e;
		}
	}
    e = gf_dm_sess_process(*sess);
    switch (e) {
    case GF_IP_CONNECTION_FAILURE:
    case GF_IP_NETWORK_FAILURE:
    {
        gf_term_download_del(*sess);
        GF_LOG(GF_LOG_WARNING, GF_LOG_MODULE,
               ("[MPD_IN] failed to download, retrying once with %s...\n", url));
        *sess = gf_term_download_new(service, url, GF_NETIO_SESSION_NOT_THREADED, user_io, usr_cbk);
        if (!(*sess)){
	    GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Cannot retry to download %s... OUT of memory ?\n", url));
            return GF_OUT_OF_MEM;
        }
        e = gf_dm_sess_process(*sess);
        if (e != GF_OK) {
            GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE,
                   ("[MPD_IN] two consecutive failures, aborting the download %s.\n", url));
        }
        return e;
    }
    case GF_OK:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] OK, Download %s complete\n", url));
        return e;
    default:
        GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] FAILED to download %s = %s...\n", url, gf_error_to_string(e)));
        return e;
    }
}

static void MPD_GetTimelineDuration(GF_MPD_SegmentTimeline *timeline, u32 *nb_segments, Double *seg_duration)
{
	u32 i, count;

	*nb_segments = 0;
	*seg_duration = 0;
	count = gf_list_count(timeline->entries);
	for (i=0; i<count; i++) {
		GF_MPD_SegmentTimelineEntry *ent = gf_list_get(timeline->entries, i);
		*nb_segments += 1 + ent->repeat_count;
		if (*seg_duration < ent->duration) *seg_duration = ent->duration;
	}
}

static void MPD_GetSegmentDuration(GF_MPD_Representation *rep, GF_MPD_AdaptationSet *set, GF_MPD_Period *period, GF_MPD *mpd, u32 *nb_segments, Double *seg_duration)
{
	Double mediaDuration;
	u32 timescale;
	u64 duration;
	GF_MPD_SegmentTimeline *timeline = NULL;
	*nb_segments = timescale = 0;
	duration = 0;
	
	/*single segment*/
	if (rep->segment_base || set->segment_base || period->segment_base) {
		return;
	}
	if (rep->segment_list || set->segment_list || period->segment_list) {
		GF_List *segments = NULL;
		if (period->segment_list) {
			if (period->segment_list->duration) duration = period->segment_list->duration;
			if (period->segment_list->timescale) timescale = period->segment_list->timescale;
			if (period->segment_list->segment_URLs) segments = period->segment_list->segment_URLs;
			if (period->segment_list->segment_timeline) timeline = period->segment_list->segment_timeline;
		}
		if (set->segment_list) {
			if (set->segment_list->duration) duration = set->segment_list->duration;
			if (set->segment_list->timescale) timescale = set->segment_list->timescale;
			if (set->segment_list->segment_URLs) segments = set->segment_list->segment_URLs;
			if (set->segment_list->segment_timeline) timeline = set->segment_list->segment_timeline;
		}
		if (rep->segment_list) {
			if (rep->segment_list->duration) duration = rep->segment_list->duration;
			if (rep->segment_list->timescale) timescale = rep->segment_list->timescale;
			if (rep->segment_list->segment_URLs) segments = rep->segment_list->segment_URLs;
			if (rep->segment_list->segment_timeline) timeline = rep->segment_list->segment_timeline;
		}
		if (! timescale) timescale=1;

		if (timeline) {
			MPD_GetTimelineDuration(timeline, nb_segments, seg_duration);
			*seg_duration /= timescale;
		} else {
			if (segments) 
				*nb_segments = gf_list_count(segments);
			*seg_duration = (Double) duration;
			*seg_duration /= timescale;
		}
		return;
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
	if (!timescale) timescale=1;

	if (timeline) {
		MPD_GetTimelineDuration(timeline, nb_segments, seg_duration);
		*seg_duration /= timescale;
	} else {
		*seg_duration = (Double) duration;
		*seg_duration /= timescale;
		mediaDuration = period->duration;
		if (!mediaDuration) mediaDuration = mpd->media_presentation_duration;
		if (mediaDuration && duration) {
			Double nb_seg = (Double) mediaDuration;
			/*duration is given in ms*/
			nb_seg /= 1000;
			nb_seg *= timescale;
			nb_seg /= duration;
			*nb_segments = (u32) ceil(nb_seg);
		}
	}
}


static void MPD_SetGroupRepresentation(GF_MPD_Group *group, GF_MPD_Representation *rep)
{
	u64 duration = 0;
	u64 mediaDuration = 0;
#ifndef GPAC_DISABLE_LOG
	u32 width=0, height=0, samplerate=0;
	GF_MPD_Fractional *framerate=NULL;
#endif
	u32 k, timescale = 1;
	GF_MPD_AdaptationSet *set;
	GF_MPD_Period *period;
	u32 i = gf_list_find(group->adaptation_set->representations, rep);
	assert((s32) i >= 0);

	group->active_rep_index = i;
	group->active_bitrate = rep->bandwidth;
	group->nb_segments_in_rep = 1;

	group->min_bandwidth_selected = 1;
	for (k=0; k<gf_list_count(group->adaptation_set->representations); k++) {
		GF_MPD_Representation *arep = gf_list_get(group->adaptation_set->representations, k);
		if (group->active_bitrate > arep->bandwidth) {
			group->min_bandwidth_selected = 0;
			break;
		}
	}

	set = group->adaptation_set;
	period = group->period;

#ifndef GPAC_DISABLE_LOG

#define GET_REP_ATTR(_a)	_a = rep->_a; if (!_a) _a = set->_a;

	GET_REP_ATTR(width);
	GET_REP_ATTR(height);
	GET_REP_ATTR(samplerate);
	GET_REP_ATTR(framerate);

	GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[MPDIn] Switched to representation bandwidth %d kbps\n", rep->bandwidth/1024));
	if (group->max_bitrate) GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("\tmax download bandwidth: %d kbps\n", group->max_bitrate/1024));
	if (width&&height) {
		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("\tWidth %d Height %d", width, height));
		if (framerate) GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("framerate %d/%d", framerate->num, framerate->den));
		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("\n"));
	} else if (samplerate) {
		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("\tsamplerate %d\n", samplerate));
	}
#endif


	MPD_GetSegmentDuration(rep, set, period, group->mpd_in->mpd, &group->nb_segments_in_rep, &group->segment_duration);
}

static void MPD_SwitchGroupRepresentation(GF_MPD_In *mpd, GF_MPD_Group *group)
{
	u32 i, bandwidth, min_bandwidth;
	GF_MPD_Representation *rep_sel = NULL;
	GF_MPD_Representation *min_rep_sel = NULL;
	Bool min_bandwidth_selected = 0;
	bandwidth = 0;
	min_bandwidth = (u32) -1;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPDIn] Checking representations between %d and %d kbps\n", group->min_bitrate/1024, group->max_bitrate/1024));

	if (group->force_representation_idx_plus_one) {
		rep_sel = gf_list_get(group->adaptation_set->representations, group->force_representation_idx_plus_one - 1);
		group->force_representation_idx_plus_one = 0;
	} 

	if (!rep_sel) {
		for (i=0; i<gf_list_count(group->adaptation_set->representations); i++) {
			GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, i);
			if (rep->disabled) continue;
			if ((rep->bandwidth > bandwidth) && (rep->bandwidth < group->max_bitrate )) {
				rep_sel = rep;
				bandwidth = rep->bandwidth;
			}
			if (rep->bandwidth < min_bandwidth) {
				min_rep_sel = rep;
				min_bandwidth = rep->bandwidth;
			}
		}
	}

	if (!rep_sel) {
		rep_sel = min_rep_sel;
		min_bandwidth_selected = 1;
	}
	assert(rep_sel);
	i = gf_list_find(group->adaptation_set->representations, rep_sel);

	assert((s32) i >= 0);

	group->force_switch_bandwidth = 0;
	group->max_bitrate = 0;
	group->min_bitrate = (u32) -1;

	if (i != group->active_rep_index) {
		if (min_bandwidth_selected) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MODULE, ("[MPDIn] No representation found with bandwidth below %d kbps - using representation @ %d kbps\n", group->max_bitrate/1024, rep_sel->bandwidth/1024));
		}
		MPD_SetGroupRepresentation(group, rep_sel);
	}
}


static void MPD_ResolveDuration(GF_MPD_Representation *rep, GF_MPD_AdaptationSet *set, GF_MPD_Period *period, u64 *out_duration, u32 *out_timescale)
{
	u32 timescale = 0;
	GF_MPD_SegmentTimeline *segment_timeline;
	GF_MPD_MultipleSegmentBase *mbase_rep, *mbase_set, *mbase_period;
	/*single media segment - duration is not known unless indicated in period*/
	if (rep->segment_base || set->segment_base || period->segment_base) {
		*out_duration = period ? period->duration : 0;
		*out_timescale = 1000;
		return;
	}
	/*we have a segment template list or template*/
	mbase_rep = rep->segment_list ? (GF_MPD_MultipleSegmentBase *) rep->segment_list : (GF_MPD_MultipleSegmentBase *) rep->segment_template;
	mbase_set = set->segment_list ? (GF_MPD_MultipleSegmentBase *)set->segment_list : (GF_MPD_MultipleSegmentBase *)set->segment_template;
	mbase_period = period->segment_list ? (GF_MPD_MultipleSegmentBase *)period->segment_list : (GF_MPD_MultipleSegmentBase *)period->segment_template;

	segment_timeline = NULL;
	if (mbase_period) segment_timeline =  mbase_period->segment_timeline;
	if (mbase_set) segment_timeline =  mbase_set->segment_timeline;
	if (mbase_rep) segment_timeline =  mbase_rep->segment_timeline;

	timescale = mbase_rep ? mbase_rep->timescale : 0;
	if (!timescale && mbase_set && mbase_set->timescale) timescale = mbase_set->timescale;
	if (!timescale && mbase_period && mbase_period->timescale) timescale  = mbase_period->timescale;
	if (!timescale) timescale = 1;
	*out_timescale = timescale;

	if (mbase_rep && mbase_rep->duration) *out_duration = mbase_rep->duration;
	else if (mbase_set && mbase_set->duration) *out_duration = mbase_set->duration;
	else if (mbase_period && mbase_period->duration) *out_duration = mbase_period->duration;

}

typedef enum
{
	GF_MPD_RESOLVE_URL_MEDIA,
	GF_MPD_RESOLVE_URL_INIT,
	GF_MPD_RESOLVE_URL_INDEX,
} GF_MPDURLResolveType;


GF_Err MPD_ResolveURL(GF_MPD *mpd, GF_MPD_Representation *rep, GF_MPD_AdaptationSet *set, GF_MPD_Period *period, char *mpd_url, GF_MPDURLResolveType resolve_type, u32 item_index, char **out_url, u64 *out_range_start, u64 *out_range_end, u64 *segment_duration)
{
	GF_MPD_BaseURL *url_child;
	GF_MPD_SegmentTimeline *timeline = NULL;
	u32 start_number = 1;
	u32 timescale;
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

	MPD_ResolveDuration(rep, set, period, segment_duration, &timescale);
	*segment_duration = (u32) ((Double) (*segment_duration) * 1000.0 / timescale);

	/*single URL*/
	if (rep->segment_base || set->segment_base || period->segment_base) {
		GF_MPD_URL *res_url;
		if (item_index>0) return GF_EOS;
		switch (resolve_type) {
		case GF_MPD_RESOLVE_URL_MEDIA:
			if (!url) return GF_NON_COMPLIANT_BITSTREAM;
			*out_url = url;
			return GF_OK;
		case GF_MPD_RESOLVE_URL_INIT:
		case GF_MPD_RESOLVE_URL_INDEX:
			res_url = NULL;
			if (resolve_type == GF_MPD_RESOLVE_URL_INDEX) {
				if (period->segment_base) res_url = period->segment_base->representation_index;
				if (set->segment_base) res_url = set->segment_base->representation_index;
				if (rep->segment_base) res_url = rep->segment_base->representation_index;
			} else {
				if (period->segment_base) res_url = period->segment_base->initialization_segment;
				if (set->segment_base) res_url = set->segment_base->initialization_segment;
				if (rep->segment_base) res_url = rep->segment_base->initialization_segment;
			}
			/*no initialization segment / index*/
			if (!res_url) {
				gf_free(url);
				return GF_OK;
			}
			if (res_url->sourceURL) {
				*out_url = gf_url_concatenate(url, res_url->sourceURL);
				gf_free(url);
			} else {
				*out_url = url;
			}
			if (res_url->byte_range) {
				*out_range_start = res_url->byte_range->start_range;
				*out_range_end = res_url->byte_range->end_range;
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
		}
		if (set->segment_list) {
			if (set->segment_list->initialization_segment) init_url = set->segment_list->initialization_segment;
			if (set->segment_list->representation_index) index_url = set->segment_list->representation_index;
			if (set->segment_list->segment_URLs) segments = set->segment_list->segment_URLs;
			if (set->segment_list->start_number != (u32) -1) start_number = set->segment_list->start_number;
			if (set->segment_list->segment_timeline) timeline = set->segment_list->segment_timeline;
		}
		if (rep->segment_list) {
			if (rep->segment_list->initialization_segment) init_url = rep->segment_list->initialization_segment;
			if (rep->segment_list->representation_index) index_url = rep->segment_list->representation_index;
			if (rep->segment_list->segment_URLs) segments = rep->segment_list->segment_URLs;
			if (rep->segment_list->start_number != (u32) -1) start_number = rep->segment_list->start_number;
			if (rep->segment_list->segment_timeline) timeline = rep->segment_list->segment_timeline;
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
			if (!url) {
		        GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Media URL is not set in segment list\n"));
				return GF_SERVICE_ERROR;
			}
			if (item_index >= segment_count) {
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
	}
	if (set->segment_template) {
		if (set->segment_template->initialization) init_template = set->segment_template->initialization;
		if (set->segment_template->index) index_template = set->segment_template->index;
		if (set->segment_template->media) media_url = set->segment_template->media;
		if (set->segment_template->start_number != (u32) -1) start_number = set->segment_template->start_number;
		if (set->segment_template->segment_timeline) timeline = set->segment_template->segment_timeline;
	}
	if (rep->segment_template) {
		if (rep->segment_template->initialization) init_template = rep->segment_template->initialization;
		if (rep->segment_template->index) index_template = rep->segment_template->index;
		if (rep->segment_template->media) media_url = rep->segment_template->media;
		if (rep->segment_template->start_number != (u32) -1) start_number = rep->segment_template->start_number;
		if (rep->segment_template->segment_timeline) timeline = rep->segment_template->segment_timeline;
	}
	if (!media_url) {
		GF_MPD_BaseURL *base = gf_list_get(rep->base_URLs, 0);
		media_url = base->URL;
	}
	url_to_solve = NULL;
	switch (resolve_type) {
	case GF_MPD_RESOLVE_URL_INIT:
		url_to_solve = init_template;
		break;
	case GF_MPD_RESOLVE_URL_MEDIA:
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
	solved_template = gf_malloc(sizeof(char)*strlen(url_to_solve)*2);
	solved_template[0] = 0;
	strcpy(solved_template, url_to_solve);
	first_sep = strchr(solved_template, '$');
	if (first_sep) first_sep[0] = 0;

	first_sep = strchr(url_to_solve, '$');
	while (first_sep) {
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
		if (format_tag) format_tag[0] = 0;
		/* identifier is $$ -> replace by $*/
		if (!strlen(first_sep+1)) {
			strcat(solved_template, "$");
		}
		else if (!strcmp(first_sep+1, "RepresentationID")) {
			strcat(solved_template, rep->id);
		}
		else if (!strcmp(first_sep+1, "Number")) {
			if (format_tag) {
				char szPrintFormat[20];
				strcpy(szPrintFormat, "%");
				strcat(szPrintFormat, format_tag+1);
				strcat(szPrintFormat, "d");
				sprintf(szFormat, szPrintFormat, start_number + item_index);
			} else {
				sprintf(szFormat, "%d", start_number + item_index);
			}
			strcat(solved_template, szFormat);
		}
		else if (!strcmp(first_sep+1, "Bandwidth")) {
			if (format_tag) {
				char szPrintFormat[20];
				strcpy(szPrintFormat, "%");
				strcat(szPrintFormat, format_tag+1);
				strcat(szPrintFormat, "d");
				sprintf(szFormat, format_tag+1, rep->bandwidth);
			} else {
				sprintf(szFormat, "%d", rep->bandwidth);
			}
			strcat(solved_template, szFormat);
		}
		else if (!strcmp(first_sep+1, "Time")) {
			if (timeline) {
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
						start_time += ent->duration * (1 + ent->repeat_count);
						continue;
					}
					*segment_duration = ent->duration;
					*segment_duration = (u32) ((Double) (*segment_duration) * 1000.0 / timescale);
					nb_repeat = item_index - cur_idx;
					time = ent->start_time ? ent->start_time : start_time;
					time += nb_repeat * ent->duration;
					sprintf(szFormat, ""LLD"", time);
					strcat(solved_template, szFormat);
					break;
				}
			}
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
	*out_url = gf_url_base_concatenate(url, solved_template);
	gf_free(url);
	gf_free(solved_template);
	return GF_OK;
}

static GF_Err MPD_DownloadInitSegment(GF_MPD_In *mpdin, GF_MPD_Group *group)
{
    GF_Err e;
    char *base_init_url;
    GF_MPD_Representation *rep;
	u64 start_range, end_range;
    /* This variable is 0 if there is a initURL, the index of first segment downloaded otherwise */
    u32 nb_segment_read = 0;
    if (!mpdin || !group)
        return GF_BAD_PARAM;
    gf_mx_p(mpdin->dl_mutex);
    
	assert( group->adaptation_set && group->adaptation_set->representations );
    rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);
    if (!rep) {
        gf_mx_v(mpdin->dl_mutex);
        GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Unable to find any representation, aborting.\n"));
        return GF_IO_ERR;
    }
	start_range = end_range = 0;
	
	e = MPD_ResolveURL(mpdin->mpd, rep, group->adaptation_set, group->period, mpdin->url, GF_MPD_RESOLVE_URL_INIT, 0, &base_init_url, &start_range, &end_range, &group->current_downloaded_segment_duration);
	if (e) {
        gf_mx_v(mpdin->dl_mutex);
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Unable to resolve initialization URL: %s\n", gf_error_to_string(e) ));
        return e;
	}

	/*no error and no init segment, go for media segment*/
	if (!base_init_url) {
		e = MPD_ResolveURL(mpdin->mpd, rep, group->adaptation_set, group->period, mpdin->url, GF_MPD_RESOLVE_URL_MEDIA, group->download_segment_index, &base_init_url, &start_range, &end_range, &group->current_downloaded_segment_duration);
		if (e) {
			gf_mx_v(mpdin->dl_mutex);
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Unable to resolve media URL: %s\n", gf_error_to_string(e) ));
			return e;
		}
		nb_segment_read = 1;
	} else if (!group->bitstream_switching) {
		group->dont_delete_first_segment = 1;
	}

	if (!strstr(base_init_url, "://") || !strnicmp(base_init_url, "file://", 7)) {
        assert(!group->nb_cached_segments);
        group->cached[0].cache = gf_strdup(base_init_url);
        group->cached[0].url = gf_strdup(base_init_url);
        group->cached[0].representation_index = group->active_rep_index;
		group->prev_active_rep_index = group->active_rep_index;
		
        group->nb_cached_segments = 1;
		/*do not erase local files*/
		group->local_files = 1;
		group->download_segment_index += nb_segment_read;
        group->segment_local_url = group->cached[0].cache;
		group->local_url_start_range = start_range;
		group->local_url_end_range = end_range;
        rep->cached_init_segment_url = gf_strdup(group->segment_local_url);
		rep->init_start_range = start_range;
		rep->init_end_range = end_range;

		GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Setup initialization segment %s \n", group->segment_local_url));
		if (!group->input_module) {
			const char *mime_type = MPD_GetMimeType(NULL, rep, group->adaptation_set);
			e = MPD_LoadMediaService(mpdin, group, mime_type, group->segment_local_url);
		}
        gf_mx_v(mpdin->dl_mutex);
        gf_free(base_init_url);
		return GF_OK;
	}

	group->max_bitrate = 0;
	group->min_bitrate = (u32)-1;
	/*use persistent connection for segment downloads*/
    e = MPD_downloadWithRetry(mpdin->service, &(group->segment_dnload), base_init_url, MPD_NetIO_Segment, group, start_range, end_range, 1);

	if ((e==GF_OK) && group->force_switch_bandwidth && !mpdin->auto_switch_count) {
		MPD_SwitchGroupRepresentation(mpdin, group);
        gf_mx_v(mpdin->dl_mutex);
		return MPD_DownloadInitSegment(mpdin, group);
	}


    if (e == GF_URL_ERROR && !base_init_url) { /* We have a 404 and started with segments */
        /* It is possible that the first segment has been deleted while we made the first request...
         * so we try with the next segment on some M3U8 servers */

		gf_free(base_init_url);

		e = MPD_ResolveURL(mpdin->mpd, rep, group->adaptation_set, group->period, mpdin->url, GF_MPD_RESOLVE_URL_MEDIA, group->download_segment_index + 1, &base_init_url, &start_range, &end_range, &group->current_downloaded_segment_duration);
		if (!e) {
            gf_mx_v(mpdin->dl_mutex);
            return e;
        }
        GF_LOG(GF_LOG_WARNING, GF_LOG_MODULE, ("Download of first segment failed... retrying with second one : %s\n", base_init_url));
        nb_segment_read = 2;
		/*use persistent connection for segment downloads*/
        e = MPD_downloadWithRetry(mpdin->service, &(group->segment_dnload), base_init_url, MPD_NetIO_Segment, group, 0, 0, 1);
    } /* end of 404 */

    if (e!= GF_OK && !group->segment_must_be_streamed) {
        mpdin->mpd_stop_request = 1;
        gf_mx_v(mpdin->dl_mutex);
        gf_free(base_init_url);
        return e;
    } else {
        char mime[128];
		const char *mime_type;
        u32 count = group->nb_segments_in_rep + 1;
        if (count < group->max_cached_segments) {
            if (count < 1) {
                GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] 0 representations, aborting\n"));
                gf_free(base_init_url);
		        gf_mx_v(mpdin->dl_mutex);
                return GF_BAD_PARAM;
            }
            GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[MPD_IN] Resizing to %u max_cached_segments elements instead of %u.\n", count, group->max_cached_segments));
            /* OK, we have a problem, it may ends download */
            group->max_cached_segments = count;
        }
        e = gf_dm_sess_process(group->segment_dnload);
        /* Mime-Type check */
		strncpy(mime, gf_dm_sess_mime_type(group->segment_dnload), sizeof(mime));
        strlwr(mime);
        if (mime && group->input_module == NULL) {
            GF_Err e;
            GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Searching an input plugin for mime type : %s...\n", mime));
            gf_free( mpdin->mimeTypeForM3U8Segments);
            mpdin->mimeTypeForM3U8Segments = gf_strdup( mime );
			if (rep->mime_type) gf_free( rep->mime_type);
            rep->mime_type = gf_strdup( mime );
            e = MPD_LoadMediaService(mpdin, group, mime, base_init_url);
			if (e != GF_OK) {
		        gf_mx_v(mpdin->dl_mutex);
                return e;
			}
        }
		mime_type = MPD_GetMimeType(NULL, rep, group->adaptation_set);
        if (!mime || (stricmp(mime, mime_type))) {
			Bool valid = 0;
			char *stype1, *stype2;
			stype1 = strchr(mime_type, '/');
			stype2 = mime ? strchr(mime, '/') : NULL;
			if (stype1 && stype2 && !strcmp(stype1, stype2)) valid = 1;

			if (!valid && 0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Mime '%s' is not correct for '%s', it should be '%s'\n", mime, base_init_url, mime_type));
				mpdin->mpd_stop_request = 0;
				gf_mx_v(mpdin->dl_mutex);
				gf_free(base_init_url);
				base_init_url = NULL;
				return GF_BAD_PARAM;
			}
        }

        if (group->segment_must_be_streamed ) {
            group->segment_local_url = gf_dm_sess_get_resource_name(group->segment_dnload);
            e = GF_OK;
        } else {
            group->segment_local_url = gf_dm_sess_get_cache_name(group->segment_dnload);
        }

        if ((e!=GF_OK) || !group->segment_local_url) {
            GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error with initialization segment: download result:%s, cache file:%s\n", gf_error_to_string(e), group->segment_local_url));
            mpdin->mpd_stop_request = 1;
            gf_mx_v(mpdin->dl_mutex);
            gf_free(base_init_url);
            return GF_BAD_PARAM;
        } 

        assert(!group->nb_cached_segments);
        group->cached[0].cache = gf_strdup(group->segment_local_url);
        group->cached[0].url = gf_strdup(gf_dm_sess_get_resource_name(group->segment_dnload));
		group->cached[0].representation_index = group->active_rep_index;
        rep->cached_init_segment_url = gf_strdup(group->segment_local_url);
		rep->init_start_range = 0;
		rep->init_end_range = 0;

        group->nb_cached_segments = 1;
        group->download_segment_index += nb_segment_read;
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Adding initialization segment %s to cache: %s\n", group->segment_local_url, group->cached[0].url ));
        gf_mx_v(mpdin->dl_mutex);
        gf_free(base_init_url);

		/*finally download all init segments if any*/
		if (!group->bitstream_switching) {
			u32 k;
			for (k=0; k<gf_list_count(group->adaptation_set->representations); k++) {
				char *a_base_init_url = NULL;
				u64 a_start, a_end, a_dur;
				GF_MPD_Representation *a_rep = gf_list_get(group->adaptation_set->representations, k);
				if (a_rep==rep) continue;
				if (a_rep->disabled) continue;

				e = MPD_ResolveURL(mpdin->mpd, a_rep, group->adaptation_set, group->period, mpdin->url, GF_MPD_RESOLVE_URL_INIT, 0, &a_base_init_url, &a_start, &a_end, &a_dur);
				if (!e && a_base_init_url) {
					e = MPD_downloadWithRetry(mpdin->service, &(group->segment_dnload), a_base_init_url, MPD_NetIO_Segment, group, a_start, a_end, 1);
					if (e) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Cannot retrieve initialization segment %s for representation: %s - discarding representation\n", a_base_init_url, gf_error_to_string(e) ));
						a_rep->disabled = 1;
					} else {
			            a_rep->cached_init_segment_url = gf_strdup(gf_dm_sess_get_cache_name(group->segment_dnload));
						rep->init_start_range = 0;
						rep->init_end_range = 0;
					}
					gf_free(a_base_init_url);
				} else if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Cannot solve initialization segment for representation: %s - discarding representation\n", gf_error_to_string(e) ));
					a_rep->disabled = 1;
				}

			}
		}

		return GF_OK;
    }
}

static void MPDIn_skip_disabled_rep(GF_MPD_Group *group, GF_MPD_Representation *rep)
{
	s32 rep_idx = gf_list_find(group->adaptation_set->representations, rep);
	while (1) {
		rep_idx++;
		if (rep_idx==gf_list_count(group->adaptation_set->representations)) rep_idx = 0;
		rep = gf_list_get(group->adaptation_set->representations, rep_idx);
		if (!rep->disabled) break;
	}
	assert(rep && !rep->disabled);
	MPD_SetGroupRepresentation(group, rep);
	GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[MPD_IN] Switching to representation %d - BW %d\n", group->active_rep_index, group->active_bitrate ));
}


static u32 download_segments(void *par)
{
    GF_Err e;
    GF_MPD_In *mpdin = (GF_MPD_In*) par;
    GF_MPD_Period *period;
    GF_MPD_Representation *rep;
    u32 i, group_count, ret = 0;
	Bool go_on = 1;
    char *new_base_seg_url;
    assert(mpdin);
    if (!mpdin->mpd){
		GF_LOG(GF_LOG_WARNING, GF_LOG_MODULE, ("[MPD_IN] Incorrect state, no mpdin->mpd for URL=%s, already stopped ?\n", mpdin->url));
      return 1;
    }

    /* Setting the download status in exclusive code */
    gf_mx_p(mpdin->dl_mutex);
    mpdin->mpd_is_running = MPD_STATE_CONNECTING;
    gf_mx_v(mpdin->dl_mutex);

restart_period:
	mpdin->in_period_setup = 1;
	e = GF_OK;
    period = gf_list_get(mpdin->mpd->periods, mpdin->active_period_index);
	group_count = gf_list_count(mpdin->groups);
	for (i=0; i<group_count; i++) {
	    GF_MPD_Group *group = gf_list_get(mpdin->groups, i);
		if (!group->selected) continue;
	    e = MPD_DownloadInitSegment(mpdin, group);
		if (e) break;
	}
    mpdin->mpd_stop_request=0;

	if (e != GF_OK) {
        gf_term_on_connect(mpdin->service, NULL, e);
        ret = 1;
		goto exit;
    }

    mpdin->last_update_time = gf_sys_clock();

    gf_mx_p(mpdin->dl_mutex);
    mpdin->mpd_is_running = MPD_STATE_CONNECTING;
    gf_mx_v(mpdin->dl_mutex);

	for (i=0; i<group_count; i++) {
		GF_MPD_Group *group = gf_list_get(mpdin->groups, i);
		if (!group->selected) continue;
		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[MPD_IN] Connecting initial service... %s\n", group->segment_local_url));
		if (! group->input_module) {
			e = GF_SERVICE_ERROR;
	        gf_term_on_connect(mpdin->service, NULL, e);
			ret = 1;
			goto exit;
		}
		e = group->input_module->ConnectService(group->input_module, mpdin->service, group->segment_local_url);
		if (e) {
			ret = 1;
			goto exit;
		}
		group->service_connected = 1;
		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[MPD_IN] Connecting initial service DONE\n", group->segment_local_url));
	}

    gf_mx_p(mpdin->dl_mutex);
	mpdin->in_period_setup = 0;
    mpdin->mpd_is_running = MPD_STATE_RUNNING;
    gf_mx_v(mpdin->dl_mutex);

	while (go_on) {
		const char *local_file_name = NULL;
		const char *resource_name = NULL;
        /*wait until next segment is needed*/
        while (!mpdin->mpd_stop_request) {
            u32 timer = gf_sys_clock() - mpdin->last_update_time;
            Bool shouldParsePlaylist = mpdin->mpd->minimum_update_period && (timer > mpdin->mpd->minimum_update_period);

			if (shouldParsePlaylist) {
                GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Next segment in cache, but it is time to update the playlist (%u ms/%u)\n", timer, mpdin->mpd->minimum_update_period));
                e = MPD_UpdatePlaylist(mpdin);
				group_count = gf_list_count(mpdin->groups);
                if (e) {
                    GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error updating MPD %s\n", gf_error_to_string(e)));
                }
            } else {
				Bool all_groups_done = 1;
				Bool cache_full = 1;
			    gf_mx_p(mpdin->dl_mutex);
				for (i=0; i<group_count; i++) {
					GF_MPD_Group *group = gf_list_get(mpdin->groups, i);
					if (!group->selected || group->done) continue;
					all_groups_done = 0;
					if (group->nb_cached_segments<group->max_cached_segments) {
						cache_full = 0;
						break;
					}
				}
	            gf_mx_v(mpdin->dl_mutex);
				if (!cache_full) break;

				if (mpdin->request_period_switch==2) all_groups_done = 1;

				if (all_groups_done && mpdin->request_period_switch) {
					MPD_ResetGroups(mpdin);
					if (mpdin->request_period_switch == 1) 
						mpdin->active_period_index++;
					
					MPD_SetupPeriod(mpdin);
					mpdin->request_period_switch = 0;

					goto restart_period;
				}

				gf_sleep(16);
            }
        }

        /* stop the thread if requested */
        if (mpdin->mpd_stop_request) {
            go_on = 0;
            break;
        }

        /* Continue the processing (no stop request) */
        period = gf_list_get(mpdin->mpd->periods, mpdin->active_period_index);

		/*for each selected groups*/
		for (i=0; i<group_count; i++) {		
			u64 start_range, end_range;
			Bool use_byterange;
			GF_MPD_Group *group = gf_list_get(mpdin->groups, i);
			if (! group->selected) continue;
			if (group->done) continue;


			if (group->nb_cached_segments>=group->max_cached_segments) {
				continue;
			}

			rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);

			/* if the index of the segment to be downloaded is greater or equal to the last segment (as seen in the playlist),
			   we need to check if a new playlist is ready */
			if (group->download_segment_index>=group->nb_segments_in_rep) {
				u32 timer = gf_sys_clock() - mpdin->last_update_time;
				/* update of the playlist, only if indicated */
				if (mpdin->mpd->minimum_update_period && timer > mpdin->mpd->minimum_update_period) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Last segment in current playlist downloaded, checking updates after %u ms\n", timer));
					e = MPD_UpdatePlaylist(mpdin);
					if (e) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error updating MPD %s\n", gf_error_to_string(e)));
					}
					group_count = gf_list_count(mpdin->groups);
					period = gf_list_get(mpdin->mpd->periods, mpdin->active_period_index);
					rep = gf_list_get(group->adaptation_set->representations, group->active_rep_index);
				} else {
					gf_sleep(16);
				}
				/* Now that the playlist is up to date, we can check again */
				if (group->download_segment_index >= group->nb_segments_in_rep) {
					if (mpdin->mpd->minimum_update_period) {
						/* if there is a specified update period, we redo the whole process */
						continue;
					} else {
						/* if not, we are really at the end of the playlist, we can quit */
						GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[MPD_IN] End of playlist reached... downloading remaining elements..."));
						group->done = 1;
						break;
					}
				}
			}
			gf_mx_p(mpdin->dl_mutex);

			/* At this stage, there are some segments left to be downloaded */
			e = MPD_ResolveURL(mpdin->mpd, rep, group->adaptation_set, group->period, mpdin->url, GF_MPD_RESOLVE_URL_MEDIA, group->download_segment_index, &new_base_seg_url, &start_range, &end_range, &group->current_downloaded_segment_duration);
			gf_mx_v(mpdin->dl_mutex);
			if (e) {
				/*do something!!*/
				break;
			}
			use_byterange = (start_range || end_range) ? 1 : 0;

			if (use_byterange) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Downloading new segment: %s (range: "LLD"-"LLD")\n", new_base_seg_url, start_range, end_range));
			}

			/*local file*/
			if (!strstr(new_base_seg_url, "://") || !strnicmp(new_base_seg_url, "file://", 7)) {
				resource_name = local_file_name = (char *) new_base_seg_url; 
				e = GF_OK;
				/*do not erase local files*/
				group->local_files = 1;
				if (group->force_switch_bandwidth && !mpdin->auto_switch_count) {
					MPD_SwitchGroupRepresentation(mpdin, group);
					/*restart*/
					i--;
					continue;
				}

			} else {
				u32 total_size, bytes_per_sec;

				group->max_bitrate = 0;
				group->min_bitrate = (u32)-1;
				/*use persistent connection for segment downloads*/
				if (use_byterange) {
					e = MPD_downloadWithRetry(mpdin->service, &(group->segment_dnload), new_base_seg_url, MPD_NetIO_Segment, group, start_range, end_range, 1);
				} else {
					e = MPD_downloadWithRetry(mpdin->service, &(group->segment_dnload), new_base_seg_url, MPD_NetIO_Segment, group, 0, 0, 1);
				}

				if ((e==GF_OK) && group->force_switch_bandwidth) {
					if (!mpdin->auto_switch_count) {
						MPD_SwitchGroupRepresentation(mpdin, group);
						/*restart*/
						i--;
						continue;
					}
					if (rep->disabled) {
						MPDIn_skip_disabled_rep(group, rep);
						/*restart*/
						i--;
						continue;
					}
				}

				if (group->segment_must_be_streamed) local_file_name = gf_dm_sess_get_resource_name(group->segment_dnload);
				else local_file_name = gf_dm_sess_get_cache_name(group->segment_dnload);

				resource_name = gf_dm_sess_get_resource_name(group->segment_dnload);

				gf_dm_sess_get_stats(group->segment_dnload, NULL, NULL, &total_size, NULL, &bytes_per_sec, NULL);
				if (total_size && bytes_per_sec && group->current_downloaded_segment_duration) {
					Double bitrate, time;
					bitrate = 8*total_size;
					bitrate *= 1000;
					bitrate /= group->current_downloaded_segment_duration;
					bitrate /= 1024;
					time = total_size;
					time /= bytes_per_sec;
	
					GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("Downloaded segment %d bytes in %g seconds - duration %g sec - Bandwidth (kbps): indicated %d - computed %d - download %d\n", total_size, time, group->current_downloaded_segment_duration/1000.0, rep->bandwidth/1024, (u32) bitrate, 8*bytes_per_sec/1024));

					if (rep->bandwidth < 8*bytes_per_sec) {
						u32 k;
						/*find highest bandwidth that fits our bitrate*/
						GF_MPD_Representation *new_rep = NULL;
						for (k=0; k<gf_list_count(group->adaptation_set->representations); k++) {
							GF_MPD_Representation *arep = gf_list_get(group->adaptation_set->representations, k);
							if (8*bytes_per_sec > arep->bandwidth) {
								if (!new_rep) new_rep = arep;
								else if (arep->bandwidth > new_rep->bandwidth) {
									new_rep = arep;
								}
							}
						}
						if (!mpdin->disable_switching && new_rep) {
							GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("Switching to new representation bitrate %d kbps\n", new_rep->bandwidth/1024));
							MPD_SetGroupRepresentation(group, new_rep);
						}
					}
				}
			}

			if (local_file_name && (e == GF_OK || group->segment_must_be_streamed )) {
				gf_mx_p(mpdin->dl_mutex);
				assert(group->nb_cached_segments<group->max_cached_segments);
				assert( local_file_name );
				group->cached[group->nb_cached_segments].cache = gf_strdup(local_file_name);
				group->cached[group->nb_cached_segments].url = gf_strdup( resource_name );
				group->cached[group->nb_cached_segments].start_range = 0;
				group->cached[group->nb_cached_segments].end_range = 0;
				group->cached[group->nb_cached_segments].representation_index = group->active_rep_index;
				if (group->local_files && use_byterange) {
					group->cached[group->nb_cached_segments].start_range = start_range;
					group->cached[group->nb_cached_segments].end_range = end_range;
				}
				if (!group->local_files) {
					GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[MPD_IN] Added file to cache (%u/%u in cache): %s\n", group->nb_cached_segments+1, group->max_cached_segments, group->cached[group->nb_cached_segments].url));
				}
				group->nb_cached_segments++;
				group->download_segment_index++;
				if (mpdin->auto_switch_count) {
					group->nb_segments_done++;
					if (group->nb_segments_done==mpdin->auto_switch_count) {
						group->nb_segments_done=0;
						MPDIn_skip_disabled_rep(group, rep);
					}
				}
				gf_mx_v(mpdin->dl_mutex);
			}
			gf_free(new_base_seg_url);
			new_base_seg_url = NULL;
			if (e != GF_OK) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error in downloading new segment: %s %s\n", new_base_seg_url, gf_error_to_string(e)));
				go_on=0;
				break;
			}
		}
    }

exit:
    /* Signal that the download thread has ended */
    gf_mx_p(mpdin->dl_mutex);
    mpdin->mpd_is_running = MPD_STATE_STOPPED;
    gf_mx_v(mpdin->dl_mutex);
    return ret;
}

const char * MPD_MPD_DESC = "HTTP MPD Streaming";

const char * MPD_MPD_EXT = "3gm mpd";

const char * MPD_M3U8_DESC = "HTTP M3U8 Playlist Streaming";

const char * MPD_M3U8_EXT = "m3u8 m3u";

static u32 MPD_RegisterMimeTypes(const GF_InputService *plug)
{
    u32 i, c;
    for (i = 0 ; MPD_MIME_TYPES[i]; i++)
        gf_term_register_mime_type (plug, MPD_MIME_TYPES[i], MPD_MPD_EXT, MPD_MPD_DESC);
    c = i;
    for (i = 0 ; M3U8_MIME_TYPES[i]; i++)
        gf_term_register_mime_type(plug, M3U8_MIME_TYPES[i], MPD_M3U8_EXT, MPD_M3U8_DESC);
    return c+i;
}

Bool MPD_CanHandleURL(GF_InputService *plug, const char *url)
{
    u32 i;
	char *sExt;
    if (!plug || !url)
      return 0;
    sExt = strrchr(url, '.');
    GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Can Handle URL request from terminal for %s\n", url));
    for (i = 0 ; MPD_MIME_TYPES[i]; i++) {
        if (gf_term_check_extension(plug, MPD_MIME_TYPES[i], MPD_MPD_EXT, MPD_MPD_DESC, sExt))
            return 1;
    }
    for (i = 0 ; M3U8_MIME_TYPES[i]; i++) {
        if (gf_term_check_extension(plug, M3U8_MIME_TYPES[i], MPD_M3U8_EXT, MPD_M3U8_DESC, sExt))
            return 1;
    }
    return MPD_CheckRootType(url);
}


GF_InputService *MPD_GetInputServiceForChannel(GF_MPD_In *mpdin, LPNETCHANNEL channel)
{
	GF_Channel *ch;
	if (mpdin->group_zero_selected) return mpdin->group_zero_selected->input_module;
	ch = (GF_Channel *) channel;
	assert(ch && ch->odm && ch->odm->OD);

	return (GF_InputService *) ch->odm->OD->service_ifce;
}

GF_MPD_Group *MPD_GetGroupForInputService(GF_MPD_In *mpdin, GF_InputService *ifce)
{
	u32 i;
	for (i=0; i<gf_list_count(mpdin->groups); i++) {
		GF_MPD_Group *group = gf_list_get(mpdin->groups, i);
		if (group->input_module==ifce) return group;
	}
	return NULL;
}

GF_Err MPD_ConnectChannel(GF_InputService *plug, LPNETCHANNEL channel, const char *url, Bool upstream)
{
    GF_MPD_In *mpdin = (GF_MPD_In*) plug->priv;
	GF_InputService *segment_ifce = MPD_GetInputServiceForChannel(mpdin, channel);
    if (!plug || !plug->priv || !segment_ifce) return GF_SERVICE_ERROR;
    GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Channel Connection (%p) request from terminal for %s\n", channel, url));
	return segment_ifce->ConnectChannel(segment_ifce, channel, url, upstream);
}

GF_Err MPD_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
    GF_MPD_In *mpdin = (GF_MPD_In*) plug->priv;
	GF_InputService *segment_ifce = MPD_GetInputServiceForChannel(mpdin, channel);
    if (!plug || !plug->priv || !segment_ifce) return GF_SERVICE_ERROR;
    GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Disconnect channel (%p) request from terminal \n", channel));

	return segment_ifce->DisconnectChannel(segment_ifce, channel);
}

static u32 MPD_GetPeriodIndexFromTime(GF_MPD_In *mpdin, u32 time)
{
    u32 i, count;
    GF_MPD_Period *period;
    count = gf_list_count(mpdin->mpd->periods);
    for (i = 0; i<count; i++) {
        period = gf_list_get(mpdin->mpd->periods, i);
        if (period->start > time) {
            break;
        }
    }
    return (i-1 >= 0 ? (i-1) : 0);
}

static void MPD_DownloadStop(GF_MPD_In *mpdin)
{
	u32 i;
    assert( mpdin );
    if (mpdin->groups) {
		for (i=0; i<gf_list_count(mpdin->groups); i++) {
			GF_MPD_Group *group = gf_list_get(mpdin->groups, i);
			assert( group );
			if (group->selected && group->segment_dnload) {
				gf_dm_sess_abort(group->segment_dnload);
				group->done = 1;
			}
		}
    }
    /* stop the download thread */
    gf_mx_p(mpdin->dl_mutex);
    if (mpdin->mpd_is_running != MPD_STATE_STOPPED) {
        mpdin->mpd_stop_request = 1;
        gf_mx_v(mpdin->dl_mutex);
        while (1) {
            /* waiting for the download thread to stop */
            gf_sleep(16);
            gf_mx_p(mpdin->dl_mutex);
            if (mpdin->mpd_is_running != MPD_STATE_RUNNING) {
                /* it's stopped we can continue */
                gf_mx_v(mpdin->dl_mutex);
                break;
            }
            gf_mx_v(mpdin->dl_mutex);
        }
    } else {
        gf_mx_v(mpdin->dl_mutex);
    }
}

Bool MPD_SeekPeriods(GF_MPD_In *mpdin)
{
	Double start_time;
	u32 i, period_idx;

	gf_mx_p(mpdin->dl_mutex);
	
	mpdin->start_range_in_segment_at_next_period = 0;
	start_time = 0;
	period_idx = 0;
	for (i=0; i<=gf_list_count(mpdin->mpd->periods); i++) {
		GF_MPD_Period *period = gf_list_get(mpdin->mpd->periods, i);
		Double dur = period->duration;
		dur /= 1000;
		if (mpdin->playback_start_range >= start_time) {
			if ((i+1==gf_list_count(mpdin->mpd->periods)) || (mpdin->playback_start_range < start_time + dur) ) {
				period_idx = i;
				break;
			}
		}
		start_time += dur;
	}
	if (period_idx != mpdin->active_period_index) {
		mpdin->playback_start_range -= start_time;
		mpdin->active_period_index = period_idx;
		mpdin->request_period_switch = 2;

		/*figure out default segment duration and substract from our start range request*/
		if (mpdin->playback_start_range) {
			Double duration;
			u32 nb_segs;
			GF_MPD_Period *period = gf_list_get(mpdin->mpd->periods, period_idx);
			GF_MPD_AdaptationSet *set = gf_list_get(period->adaptation_sets, 0);
			GF_MPD_Representation *rep = gf_list_get(set->representations, 0);

			MPD_GetSegmentDuration(rep, set, period, mpdin->mpd, &nb_segs, &duration);

			if (duration) {
				while (mpdin->playback_start_range - mpdin->start_range_in_segment_at_next_period >= duration)
					mpdin->start_range_in_segment_at_next_period += duration;
			}

		}
	}
	gf_mx_v(mpdin->dl_mutex);
	
	return mpdin->request_period_switch ? 1 : 0;
}

void MPD_SeekGroup(GF_MPD_In *mpdin, GF_MPD_Group *group)
{
	Double seg_start;
	u32 first_downloaded, last_downloaded, segment_idx;

	group->force_segment_switch = 0;
	if (!group->segment_duration) return;

	/*figure out where to seek*/
	segment_idx = 0;
	seg_start = 0.0;
	while (1) {
		if ((mpdin->playback_start_range >= seg_start) && (mpdin->playback_start_range < seg_start + group->segment_duration)) 
			break;
		seg_start += group->segment_duration;
		segment_idx++;
	}
	/*todo - seek to given duration*/
	mpdin->playback_start_range -= seg_start;

	first_downloaded = last_downloaded = group->download_segment_index;
	if (group->download_segment_index +1 >= group->nb_cached_segments) {
		first_downloaded = group->download_segment_index + 1 - group->nb_cached_segments;
	}
	/*we are seeking in our download range, just go on*/
	if ((segment_idx >= first_downloaded) && (segment_idx<=last_downloaded)) return;

	group->force_segment_switch = 1;
	group->download_segment_index = segment_idx;

	if (group->segment_dnload) 
		gf_dm_sess_abort(group->segment_dnload);

	if (group->urlToDeleteNext) {
		if (!mpdin->keep_files && !group->local_files)
			gf_dm_delete_cached_file_entry_session(group->segment_dnload, group->urlToDeleteNext);
    
		gf_free(group->urlToDeleteNext);
		group->urlToDeleteNext = NULL;
	}
	if (group->segment_dnload) {
		gf_term_download_del(group->segment_dnload);
		group->segment_dnload = NULL;
	}
	while (group->nb_cached_segments) {
		group->nb_cached_segments --;
		if (!mpdin->keep_files && !group->local_files)
			gf_delete_file(group->cached[group->nb_cached_segments].cache);

		gf_free(group->cached[group->nb_cached_segments].cache);
		gf_free(group->cached[group->nb_cached_segments].url);
		memset(&group->cached[group->nb_cached_segments], 0, sizeof(segment_cache_entry));
	}
}

void MPD_SeekGroupsDownloads(GF_MPD_In *mpdin)
{
	u32 i;

	gf_mx_p(mpdin->dl_mutex);

	if (mpdin->active_period_index) {
		Double dur = 0;
		u32 i;
		for (i=0; i<mpdin->active_period_index; i++) {
			GF_MPD_Period *period = gf_list_get(mpdin->mpd->periods, mpdin->active_period_index-1);
			dur += period->duration/1000.0;
		}
		mpdin->playback_start_range -= dur;
	}
	for (i=0; i<gf_list_count(mpdin->groups); i++) {
		GF_MPD_Group *group = gf_list_get(mpdin->groups, i);
		MPD_SeekGroup(mpdin, group);
	}
	gf_mx_v(mpdin->dl_mutex);
}


void MPD_ResetGroups(GF_MPD_In *mpdin)
{
	mpdin->service->subservice_disconnect = 1;
	gf_term_on_disconnect(mpdin->service, NULL, GF_OK);

	mpdin->service->subservice_disconnect = 2;
	while (gf_list_count(mpdin->groups)) {
		GF_MPD_Group *group = gf_list_last(mpdin->groups);
		gf_list_rem_last(mpdin->groups);

		if (group->urlToDeleteNext) {
			if (!mpdin->keep_files && !group->local_files)
				gf_dm_delete_cached_file_entry_session(group->segment_dnload, group->urlToDeleteNext);
	    
			gf_free(group->urlToDeleteNext);
			group->urlToDeleteNext = NULL;
		}
		if (group->segment_dnload) {
			gf_term_download_del(group->segment_dnload);
			group->segment_dnload = NULL;
		}
		while (group->nb_cached_segments) {
			group->nb_cached_segments --;
			if (!mpdin->keep_files && !group->local_files)
				gf_delete_file(group->cached[group->nb_cached_segments].cache);

			gf_free(group->cached[group->nb_cached_segments].cache);
			gf_free(group->cached[group->nb_cached_segments].url);
		}
		gf_free(group->cached);

		if (group->input_module) {
			if (group->service_connected) {
				group->input_module->CloseService(group->input_module);
				group->service_connected = 0;
			}
			gf_modules_close_interface((GF_BaseInterface *) group->input_module);
			group->input_module = NULL;
		}

		gf_free(group);
	}
	gf_list_del(mpdin->groups);
	mpdin->groups = NULL;
	mpdin->service->subservice_disconnect = 0;
}

/* create groups (implemntation of adaptations set) */
GF_Err MPD_SetupGroups(GF_MPD_In *mpdin)
{
	GF_Err e;
	u32 i, j, count;
    GF_MPD_Period *period;
	if (!mpdin->groups) {
		mpdin->groups = gf_list_new();
		if (!mpdin->groups) return GF_OUT_OF_MEM;
	}

	period = gf_list_get(mpdin->mpd->periods, mpdin->active_period_index);
	if (!period) return GF_BAD_PARAM;

	count = gf_list_count(period->adaptation_sets);
	for (i=0; i<count; i++) {
        Bool found = 0;
		GF_MPD_AdaptationSet *set = gf_list_get(period->adaptation_sets, i);
		for (j=0; j<gf_list_count(mpdin->groups); j++) {
			GF_MPD_Group *group = gf_list_get(mpdin->groups, j);
			if (group->adaptation_set==set) {
		        found = 1;
				break;
			}
		}
		if (!found) {
			GF_MPD_Group *group;
			GF_SAFEALLOC(group, GF_MPD_Group);
			if (!group) return GF_OUT_OF_MEM;
			group->mpd_in = mpdin;
			group->adaptation_set = set;
			group->period = period;
		
			group->bitstream_switching = (set->bitstream_switching || period->bitstream_switching) ? 1 : 0;

			group->max_cached_segments = mpdin->option_max_cached;
			group->cached = gf_malloc(sizeof(segment_cache_entry)*group->max_cached_segments);
			memset(group->cached, 0, sizeof(segment_cache_entry)*group->max_cached_segments);
			if (!group->cached) {
				gf_free(group);
				return GF_OUT_OF_MEM;
			}
			e = gf_list_add(mpdin->groups, group);
			if (e) {
				gf_free(group->cached);
				gf_free(group);
				return e;
			}
		}
	}
	return GF_OK;
}

GF_Err MPD_SetupPeriod(GF_MPD_In *mpdin)
{
	GF_Err e;
    u32 rep_i, group_i;

	/*setup all groups*/
	MPD_SetupGroups(mpdin);
	mpdin->group_zero_selected = NULL;

	for (group_i=0; group_i<gf_list_count(mpdin->groups); group_i++) {
		GF_MPD_Representation *rep_sel;
		u32 active_rep, nb_rep;
		const char *mime_type;
		GF_MPD_Group *group = gf_list_get(mpdin->groups, group_i);

		if (group->adaptation_set->group==0) {
			mpdin->group_zero_selected = group;
		} else if (mpdin->group_zero_selected) {
			/* if this group is not the group 0 and we have found the group 0, 
			we can safely ignore this group. */
			break;
		}

		nb_rep = gf_list_count(group->adaptation_set->representations);
		/* Select the appropriate representation in the given period */
		active_rep = 0;
		for (rep_i = 0; rep_i < nb_rep; rep_i++) {
			GF_MPD_Representation *rep = gf_list_get(group->adaptation_set->representations, rep_i);
			rep_sel = gf_list_get(group->adaptation_set->representations, active_rep);

			if (rep_i) {
				Bool ok;
				char *sep;
				if ( !rep->codecs || !rep_sel->codecs ) continue;
				sep = strchr(rep_sel->codecs, '.');
				if (sep) sep[0] = 0;
				ok = !strnicmp(rep->codecs, rep_sel->codecs, strlen(rep_sel->codecs) );
				if (sep) sep[0] = '.';
				if (!ok) continue;
			}
			if ((mpdin->first_select_mode==0) && (rep->bandwidth < rep_sel->bandwidth)) {
				active_rep = rep_i;
			} else if ((mpdin->first_select_mode==1) && (rep->bandwidth > rep_sel->bandwidth)) {
				active_rep = rep_i;
			} else if ((mpdin->first_select_mode==2) && (rep->quality_ranking < rep_sel->quality_ranking)) {
				active_rep = rep_i;
			} else if ((mpdin->first_select_mode==3) && (rep->quality_ranking > rep_sel->quality_ranking)) {
				active_rep = rep_i;
			}
		}

		rep_sel = gf_list_get(group->adaptation_set->representations, active_rep);
		MPD_SetGroupRepresentation(group, rep_sel);

		if (mpdin->playback_start_range>=0) 
			MPD_SeekGroup(mpdin, group);

		mime_type = MPD_GetMimeType(NULL, rep_sel, group->adaptation_set);

		if (!mime_type) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot start: missing mime\n"));
			return GF_NON_COMPLIANT_BITSTREAM;
		}

		/* TODO: Generate segment names if urltemplates are used */
		if (!rep_sel->segment_base && !rep_sel->segment_list && !rep_sel->segment_template
			&& !group->adaptation_set->segment_base && !group->adaptation_set->segment_list && !group->adaptation_set->segment_template
			&& !group->period->segment_base && !group->period->segment_list && !group->period->segment_template
			&& !gf_list_count(rep_sel->base_URLs)
		
		) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot start: missing segments\n"));
			return GF_NON_COMPLIANT_BITSTREAM;
		}

		group->input_module = NULL;
		if (strcmp(M3U8_UNKOWN_MIME_TYPE, mime_type)) {
			e = MPD_LoadMediaService(mpdin, group, mime_type, NULL);
			if (e != GF_OK) return e;
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Ignoring mime type %s, wait for first file...\n", mime_type));
		}
		group->selected = 1;
	}

	/*and seek if needed*/
	return GF_OK;
}

static GF_Err MPD_SegmentsProcessStart(GF_MPD_In *mpdin, u32 time)
{
    GF_Err e = GF_BAD_PARAM;
    GF_MPD_Period *period;

	MPD_ResetGroups(mpdin);

	/* Get the right period from the given time */
    mpdin->active_period_index = MPD_GetPeriodIndexFromTime(mpdin, time);
    period = gf_list_get(mpdin->mpd->periods, mpdin->active_period_index);
	if (!period || !gf_list_count(period->adaptation_sets) ) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot start: not enough periods or representations in MPD\n"));
        goto exit;
    }

	e = MPD_SetupPeriod(mpdin);
	if (e) goto exit;

	gf_th_run(mpdin->mpd_thread, download_segments, mpdin);
    return GF_OK;

exit:
    gf_term_on_connect(mpdin->service, NULL, e);
    return e;
}


static GF_Err http_ifce_get(GF_FileDownload *getter, char *url)
{
    GF_MPD_In *mpdin = (GF_MPD_In*) getter->udta;
	GF_DownloadSession *sess = gf_term_download_new(mpdin->service, url, GF_NETIO_SESSION_NOT_THREADED, NULL, NULL);
	if (!sess) return GF_IO_ERR;
	getter->session = sess;
	return gf_dm_sess_process(sess);
}

static void http_ifce_clean(GF_FileDownload *getter)
{
    GF_MPD_In *mpdin = (GF_MPD_In*) getter->udta;
	if (getter->session) gf_term_download_del(getter->session);
}

static const char *http_ifce_cache_name(GF_FileDownload *getter)
{
    GF_MPD_In *mpdin = (GF_MPD_In*) getter->udta;
	if (getter->session) return gf_dm_sess_get_cache_name(getter->session);
	return NULL;
}

GF_Err MPD_ConnectService(GF_InputService *plug, GF_ClientService *serv, const char *url)
{
    GF_MPD_In *mpdin = (GF_MPD_In*) plug->priv;
	char local_path[GF_MAX_PATH];
    const char *local_url, *opt;
    GF_Err e;
    GF_DOMParser *mpd_parser;
    Bool is_m3u8 = 0;
    Bool is_local = 0;

    GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Service Connection request (%p) from terminal for %s\n", serv, url));

    if (!mpdin|| !serv || !url) return GF_BAD_PARAM;

    mpdin->service = serv;
    memset( mpdin->lastMPDSignature, 0, sizeof(mpdin->last_update_time));
    mpdin->reload_count = 0;
    if (mpdin->url)
      gf_free(mpdin->url);
    mpdin->url = gf_strdup(url);
    mpdin->option_max_cached = 0;

	mpdin->getter.udta = mpdin;
	mpdin->getter.new_session = http_ifce_get;
	mpdin->getter.del_session = http_ifce_clean;
	mpdin->getter.get_cache_name = http_ifce_cache_name;
	mpdin->getter.session = NULL;


	opt = gf_modules_get_option((GF_BaseInterface *)plug, "DASH", "MaxCachedSegments");
    if (!opt) gf_modules_set_option((GF_BaseInterface *)plug, "DASH", "MaxCachedSegments", "3");
    if (opt) mpdin->option_max_cached = atoi(opt);
    if (!mpdin->option_max_cached) mpdin->option_max_cached = 1;
	/*we need one more entry for the current segment being played*/
	mpdin->option_max_cached++;

    mpdin->auto_switch_count = 0;
    opt = gf_modules_get_option((GF_BaseInterface *)plug, "DASH", "AutoSwitchCount");
    if (!opt) gf_modules_set_option((GF_BaseInterface *)plug, "DASH", "AutoSwitchCount", "0");
    if (opt) mpdin->auto_switch_count = atoi(opt);

    opt = gf_modules_get_option((GF_BaseInterface *)plug, "DASH", "KeepFiles");
    if (!opt) gf_modules_set_option((GF_BaseInterface *)plug, "DASH", "KeepFiles", "no");
    if (opt && !strcmp(opt, "yes")) mpdin->keep_files = 1;

	opt = gf_modules_get_option((GF_BaseInterface *)plug, "DASH", "DisableSwitching");
    if (opt && !strcmp(opt, "yes")) mpdin->disable_switching = 1;

	opt = gf_modules_get_option((GF_BaseInterface *)plug, "DASH", "StartRepresentation");
	if (!opt) {
		gf_modules_set_option((GF_BaseInterface *)plug, "DASH", "StartRepresentation", "minBandwidth");
		opt = "minBandwidth";
	}
	if (opt && !strcmp(opt, "maxBandwidth")) mpdin->first_select_mode = 1;
    else if (opt && !strcmp(opt, "minQuality")) mpdin->first_select_mode = 2;
    else if (opt && !strcmp(opt, "maxQuality")) mpdin->first_select_mode = 3;
    else mpdin->first_select_mode = 0;
	
	
	if (mpdin->mpd_dnload) gf_term_download_del(mpdin->mpd_dnload);
    mpdin->mpd_dnload = NULL;
	mpdin->in_seek = 0;
	mpdin->previous_start_range = -1;


    if (!strnicmp(url, "file://", 7)) {
        local_url = url + 7;
		is_local = 1;
        if (strstr(url, ".m3u8")) {
            is_m3u8 = 1;
        }
    } else if (strstr(url, "://")) {
		/*use non-persistent connection for MPD downloads*/
        e = MPD_downloadWithRetry(mpdin->service, &(mpdin->mpd_dnload), url, MPD_NetIO, mpdin, 0, 0, 1);
        if (e!=GF_OK) {
            GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot connect service: MPD downloading problem %s for %s\n", gf_error_to_string(e), url));
            gf_term_on_connect(mpdin->service, NULL, GF_IO_ERR);
            gf_term_download_del(mpdin->mpd_dnload);
            mpdin->mpd_dnload = NULL;
            return e;
		}
		{
			const char *url;
			char mime[128];
			strncpy(&(mime[0]), gf_dm_sess_mime_type(mpdin->mpd_dnload), sizeof(mime));
			strlwr(mime);
			url = gf_dm_sess_get_resource_name(mpdin->mpd_dnload);
			/* Some servers, for instance http://tv.freebox.fr, serve m3u8 as text/plain */
			if (MPD_isM3U8_mime(mime) || strstr(url, ".m3u8")) {
				is_m3u8 = 1;
			} else if (!MPD_is_MPD_mime(mime) && !strstr(url, ".mpd")) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] mime '%s' for '%s' should be m3u8 or mpd\n", mime, url));
				gf_term_on_connect(mpdin->service, NULL, GF_BAD_PARAM);
				gf_term_download_del(mpdin->mpd_dnload);
				mpdin->mpd_dnload = NULL;
				return GF_CODEC_NOT_FOUND;
			}
		}
        local_url = gf_dm_sess_get_cache_name(mpdin->mpd_dnload);
        if (!local_url) {
            GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot connect service: cache problem %s\n", local_url));
            gf_term_on_connect(mpdin->service, NULL, GF_IO_ERR);
            gf_term_download_del(mpdin->mpd_dnload);
            mpdin->mpd_dnload = NULL;
            return GF_OK;
        }
    } else {
        local_url = url;
		is_local = 1;
        if (strstr(url, ".m3u8"))
            is_m3u8 = 1;
    }
	
	if (is_local) {
		FILE *f = fopen(local_url, "rt");
		if (!f) {
	        gf_term_on_connect(mpdin->service, NULL, GF_URL_ERROR);
			return GF_OK;
		}
		fclose(f);
	}

    if (is_m3u8) {
		if (is_local) {
			char *sep;
			strcpy(local_path, local_url);
			sep = strrchr(local_path, '.');
			if (sep) sep[0]=0;
			strcat(local_path, ".mpd");

	        gf_m3u8_to_mpd(local_url, url, local_path, mpdin->reload_count, mpdin->mimeTypeForM3U8Segments, 0, M3U8_TO_MPD_USE_TEMPLATE, &mpdin->getter);
			local_url = local_path;
		} else {
	        gf_m3u8_to_mpd(local_url, url, NULL, mpdin->reload_count, mpdin->mimeTypeForM3U8Segments, 0, M3U8_TO_MPD_USE_TEMPLATE, &mpdin->getter);
		}
    }

    if (!MPD_CheckRootType(local_url)) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot connect service: wrong file type %s\n", local_url));
        gf_term_on_connect(mpdin->service, NULL, GF_BAD_PARAM);
        gf_term_download_del(mpdin->mpd_dnload);
        mpdin->mpd_dnload = NULL;
        return GF_OK;
    }

    /* parse the MPD */
    mpd_parser = gf_xml_dom_new();
    e = gf_xml_dom_parse(mpd_parser, local_url, NULL, NULL);
    if (e != GF_OK) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot connect service: MPD parsing problem %s\n", gf_xml_dom_get_error(mpd_parser) ));
        gf_xml_dom_del(mpd_parser);
        gf_term_on_connect(mpdin->service, NULL, e);
        gf_term_download_del(mpdin->mpd_dnload);
        mpdin->mpd_dnload = NULL;
        return GF_OK;
    }
    if (mpdin->mpd)
        gf_mpd_del(mpdin->mpd);
    mpdin->mpd = gf_mpd_new();
    if (!mpdin->mpd) {
        e = GF_OUT_OF_MEM;
    } else {
        e = gf_mpd_init_from_dom(gf_xml_dom_get_root(mpd_parser), mpdin->mpd, url);
    }
    gf_xml_dom_del(mpd_parser);
    if (e != GF_OK) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot connect service: MPD creation problem %s\n", gf_error_to_string(e)));
        gf_term_on_connect(mpdin->service, NULL, e);
        gf_term_download_del(mpdin->mpd_dnload);
        mpdin->mpd_dnload = NULL;
        if (mpdin->mpd)
            gf_mpd_del(mpdin->mpd);
        mpdin->mpd = NULL;
        return e;
    }

    e = MPD_SegmentsProcessStart(mpdin, 0);
    return e;
}

static GF_Descriptor *MPD_GetServiceDesc(GF_InputService *plug, u32 expect_type, const char *sub_url)
{
    GF_MPD_In *mpdin = (GF_MPD_In*) plug->priv;
    GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Service Description request from terminal for %s\n", sub_url));
	if (mpdin->group_zero_selected) {
		if (mpdin->group_zero_selected->input_module) {
	        return mpdin->group_zero_selected->input_module->GetServiceDescriptor(mpdin->group_zero_selected->input_module, expect_type, sub_url);
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot provide service description: no segment service hanlder created\n"));
			return NULL;
		}
	} else {
		u32 i;
		for (i=0; i<gf_list_count(mpdin->groups); i++) {
			GF_MPD_Group *group = gf_list_get(mpdin->groups, i);
			if (!group->selected) continue;
			if (group->service_descriptor_fetched) continue;
			group->service_descriptor_fetched = 1;
	        return group->input_module->GetServiceDescriptor(group->input_module, expect_type, sub_url);
		}
		return NULL;
	}
}

void MPD_Stop(GF_MPD_In *mpdin)
{
    assert( mpdin );
    GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Stopping service %p\n", mpdin->service));
    MPD_DownloadStop(mpdin);
    MPD_ResetGroups(mpdin);

    if (mpdin->mpd_dnload) {
        gf_term_download_del(mpdin->mpd_dnload);
        mpdin->mpd_dnload = NULL;
    }
    if (mpdin->mpd)
        gf_mpd_del(mpdin->mpd);
    mpdin->mpd = NULL;
}

GF_Err MPD_CloseService(GF_InputService *plug)
{
    GF_MPD_In *mpdin = (GF_MPD_In*) plug->priv;
    assert( mpdin );
    GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Close Service (%p) request from terminal\n", mpdin->service));
   
    MPD_Stop(mpdin);

	gf_term_on_disconnect(mpdin->service, NULL, GF_OK);

	return GF_OK;
}

GF_Err MPD_ServiceCommand(GF_InputService *plug, GF_NetworkCommand *com)
{
    GF_Err e;
    GF_MPD_In *mpdin = (GF_MPD_In*) plug->priv;
	GF_InputService *segment_ifce = NULL;
	GF_MPD_Group *group = NULL;
    if (!plug || !plug->priv || !com ) return GF_SERVICE_ERROR;

	if (mpdin->group_zero_selected) segment_ifce = mpdin->group_zero_selected->input_module;

	switch (com->command_type) {
    case GF_NET_SERVICE_INFO:
    {
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Info command from terminal on Service (%p)\n", mpdin->service));

		e = GF_OK;
		if (segment_ifce) {
			/* defer to the real input service */
			e = segment_ifce->ServiceCommand(segment_ifce, com);
		}

		if (e!= GF_OK || !com->info.name || 2 > strlen(com->info.name)) {
			GF_MPD_ProgramInfo *info = gf_list_get(mpdin->mpd->program_infos, 0);
			if (info) {
				com->info.name = info->title;
				com->info.comment = info->source;
			}
			if (!com->info.name && mpdin->group_zero_selected)
				com->info.name = mpdin->group_zero_selected->cached[0].url;
        }
        return GF_OK;
    }
    case GF_NET_SERVICE_HAS_AUDIO:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Has Audio command from terminal on Service (%p)\n", mpdin->service));
		if (segment_ifce) {
	        /* defer to the real input service */
		    return segment_ifce->ServiceCommand(segment_ifce, com);
		}
        return GF_NOT_SUPPORTED;

	case GF_NET_SERVICE_QUALITY_SWITCH:
		{
			u32 i;

			for (i=0; i<gf_list_count(mpdin->groups); i++) {
				Bool do_switch = 0;
				GF_MPD_Group *group = gf_list_get(mpdin->groups, i);
				u32 current_idx = group->active_rep_index;
				if (! group->selected) continue;

				if (group->force_representation_idx_plus_one) current_idx = group->force_representation_idx_plus_one - 1;
				if (com->switch_quality.up) {
					if (current_idx + 1 < gf_list_count(group->adaptation_set->representations)) {
						group->force_representation_idx_plus_one = 1 + current_idx+1;
						do_switch = 1;
					}
				} else {
					if (current_idx) {
						group->force_representation_idx_plus_one = 1 + current_idx - 1;
						do_switch = 1;
					}
				}
				if (do_switch) {
					gf_mx_p(mpdin->dl_mutex);
					group->force_switch_bandwidth = 1;
					/*in local playback just switch at the end of the current segment*/
					while (group->local_files && (group->nb_cached_segments>1)) {
						group->nb_cached_segments--;
						gf_free(group->cached[group->nb_cached_segments].url);
						group->cached[group->nb_cached_segments].url = NULL;
						group->cached[group->nb_cached_segments].representation_index = 0;
						group->cached[group->nb_cached_segments].start_range = 0;
						group->cached[group->nb_cached_segments].end_range = 0;
						assert(group->download_segment_index>1);
						group->download_segment_index--;
					}
					gf_mx_v(mpdin->dl_mutex);
				}
			}
		}
        return GF_OK;
	}
	/*not supported*/
	if (!com->base.on_channel) return GF_NOT_SUPPORTED;
	
	segment_ifce = MPD_GetInputServiceForChannel(mpdin, com->base.on_channel);
	if (!segment_ifce) return GF_NOT_SUPPORTED;
	group = MPD_GetGroupForInputService(mpdin, segment_ifce);


	switch (com->command_type) {
    case GF_NET_CHAN_SET_PADDING:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Set Padding command from terminal on channel %p on Service (%p)\n", com->base.on_channel, mpdin->service));
        /* for padding settings, the MPD level should not change anything */
        return segment_ifce->ServiceCommand(segment_ifce, com);
        break;
    case GF_NET_CHAN_SET_PULL:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Set Pull command from terminal on channel %p on Service (%p)\n", com->base.on_channel, mpdin->service));
        /* defer to the real input service */
        return segment_ifce->ServiceCommand(segment_ifce, com);
        break;
    case GF_NET_CHAN_INTERACTIVE:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Interactive command from terminal on channel %p on Service (%p)\n", com->base.on_channel, mpdin->service));
        /* we are interactive (that's the whole point of MPD) */
        return GF_OK;

	case GF_NET_CHAN_BUFFER:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Buffer query command from terminal on channel %p on Service (%p)\n", com->base.on_channel, mpdin->service));
        return segment_ifce->ServiceCommand(segment_ifce, com);
        break;
    case GF_NET_CHAN_BUFFER_QUERY:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received buffer query from terminal on channel %p on Service (%p)\n", com->base.on_channel, mpdin->service));
        return segment_ifce->ServiceCommand(segment_ifce, com);
        break;
    case GF_NET_CHAN_DURATION:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Duration query from terminal on channel %p on Service (%p)\n", com->base.on_channel, mpdin->service));
        /* Ignore the duration given by the input service and use the one given in the MPD
           Note: the duration of the initial segment will be 0 anyway (in MP4).*/
        {
            Double duration;
			duration = (Double)mpdin->mpd->media_presentation_duration;
			if (!duration) {
				u32 i;
				for (i=0; i<gf_list_count(mpdin->mpd->periods); i++) {
					GF_MPD_Period *period = gf_list_get(mpdin->mpd->periods, i);
					duration += (Double)period->duration;
				}
			}
            duration /= 1000;
            com->duration.duration = duration;
            return GF_OK;
        }
        break;
    case GF_NET_CHAN_PLAY:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Play command from terminal on channel %p on Service (%p)\n", com->base.on_channel, mpdin->service));

		if (! mpdin->in_period_setup && !com->play.dash_segment_switch && ! mpdin->in_seek) {
			Bool skip_seek;
			
			mpdin->in_seek = 1;
			
			/*if start range request is the same as previous one, don't process it
			- this happens at period switch when new objects are declared*/
			skip_seek = (mpdin->previous_start_range==com->play.start_range) ? 1 : 0;
			mpdin->previous_start_range = com->play.start_range;

	        mpdin->playback_speed = com->play.speed;
		    mpdin->playback_start_range = com->play.start_range;

			/*first check if we seek to another period*/
			if (! MPD_SeekPeriods(mpdin)) {
				/*if no, seek in group*/
				MPD_SeekGroupsDownloads(mpdin);
			}
		} 
		/*For MPEG-2 TS or formats not using Init Seg: since objects are declared and started once the first
		segment is playing, we will stay in playback_start_range!=-1 until next segment (because we won't have a query_next),
		which will prevent seeking until then ... we force a reset of playback_start_range to allow seeking asap*/
		else if (mpdin->in_seek && (com->play.start_range==0)) {
//			mpdin->in_seek = 0;
		}
		else if (mpdin->in_period_setup && (com->play.start_range==0)) {
			com->play.start_range = mpdin->playback_start_range;
		}

		group->done = 0;
		com->play.dash_segment_switch = group->force_segment_switch;
		/*don't forward commands, we are killing the service anyway ...*/
		if (mpdin->request_period_switch) return GF_OK;

		return segment_ifce->ServiceCommand(segment_ifce, com);

	case GF_NET_CHAN_STOP:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Stop command from terminal on channel %p on Service (%p)\n", com->base.on_channel, mpdin->service));
		group->done = 1;
        return segment_ifce->ServiceCommand(segment_ifce, com);
    case GF_NET_CHAN_PAUSE:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Pause command from terminal on channel %p on Service (%p)\n", com->base.on_channel, mpdin->service));
        return segment_ifce->ServiceCommand(segment_ifce, com);
    case GF_NET_CHAN_RESUME:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Resume command from terminal on channel %p on Service (%p)\n", com->base.on_channel, mpdin->service));
        return segment_ifce->ServiceCommand(segment_ifce, com);
	case GF_NET_CHAN_SET_SPEED:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Set Speed command from terminal on channel %p on Service (%p)\n", com->base.on_channel, mpdin->service));
        /* recording the speed, to change the segment download speed */
        mpdin->playback_speed = com->play.speed;
        return segment_ifce->ServiceCommand(segment_ifce, com);

	case GF_NET_CHAN_CONFIG:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Set Config command from terminal on channel %p on Service (%p)\n", com->base.on_channel, mpdin->service));
        /* defer to the real input service */
        return segment_ifce->ServiceCommand(segment_ifce, com);
        break;
    case GF_NET_CHAN_GET_PIXEL_AR:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Pixel Aspect Ratio query from terminal on channel %p on Service (%p)\n", com->base.on_channel, mpdin->service));
        /* defer to the real input service */
        return segment_ifce->ServiceCommand(segment_ifce, com);
        break;
    case GF_NET_CHAN_GET_DSI:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Decoder Specific Info query from terminal on channel %p on Service (%p)\n", com->base.on_channel, mpdin->service));
        return segment_ifce->ServiceCommand(segment_ifce, com);
        break;
    case GF_NET_CHAN_MAP_TIME:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Map Time query from terminal on channel %p on Service (%p)\n", com->base.on_channel, mpdin->service));
        return segment_ifce->ServiceCommand(segment_ifce, com);
        break;
    case GF_NET_CHAN_RECONFIG:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Reconfig command from terminal on channel %p on Service (%p)\n", com->base.on_channel, mpdin->service));
        return segment_ifce->ServiceCommand(segment_ifce, com);
        break;
    case GF_NET_CHAN_DRM_CFG:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received DRM query from terminal on channel %p on Service (%p)\n", com->base.on_channel, mpdin->service));
        return segment_ifce->ServiceCommand(segment_ifce, com);
        break;
    case GF_NET_CHAN_GET_ESD:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Elementary Stream Descriptor query from terminal on channel %p on Service (%p)\n", com->base.on_channel, mpdin->service));
        return segment_ifce->ServiceCommand(segment_ifce, com);
        break;
    case GF_NET_BUFFER_QUERY:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Global Buffer query from terminal on Service (%p)\n", mpdin->service));
        return segment_ifce->ServiceCommand(segment_ifce, com);
        break;
    case GF_NET_GET_STATS:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Statistics query from terminal on Service (%p)\n", mpdin->service));
        return segment_ifce->ServiceCommand(segment_ifce, com);
        break;
    case GF_NET_IS_CACHABLE:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Cachable query from terminal on Service (%p)\n", mpdin->service));
        return segment_ifce->ServiceCommand(segment_ifce, com);
        break;
    case GF_NET_SERVICE_MIGRATION_INFO:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Migration info query from terminal on Service (%p)\n", mpdin->service));
        return segment_ifce->ServiceCommand(segment_ifce, com);
        break;
    default:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received unknown command (%d) on Service (%p)\n",com->command_type, mpdin->service));
        return GF_SERVICE_ERROR;
    }
}

GF_Err MPD_ChannelGetSLP(GF_InputService *plug, LPNETCHANNEL channel, char **out_data_ptr, u32 *out_data_size, GF_SLHeader *out_sl_hdr, Bool *sl_compressed, GF_Err *out_reception_status, Bool *is_new_data)
{
    GF_Err e;
    GF_MPD_In *mpdin = (GF_MPD_In*) plug->priv;
	GF_InputService *segment_ifce = MPD_GetInputServiceForChannel(mpdin, channel);
    if (!plug || !plug->priv || !segment_ifce) return GF_SERVICE_ERROR;

//    gf_mx_p(mpdin->dl_mutex);
    e = segment_ifce->ChannelGetSLP(segment_ifce, channel, out_data_ptr, out_data_size, out_sl_hdr, sl_compressed, out_reception_status, is_new_data);
//    gf_mx_v(mpdin->dl_mutex);
    return e;
}

GF_Err MPD_ChannelReleaseSLP(GF_InputService *plug, LPNETCHANNEL channel)
{
    GF_Err e;
    GF_MPD_In *mpdin = (GF_MPD_In*) plug->priv;
	GF_InputService *segment_ifce = MPD_GetInputServiceForChannel(mpdin, channel);
    if (!plug || !plug->priv || !segment_ifce) return GF_SERVICE_ERROR;

//    gf_mx_p(mpdin->dl_mutex);
    e = segment_ifce->ChannelReleaseSLP(segment_ifce, channel);
//    gf_mx_v(mpdin->dl_mutex);
    return e;
}

Bool MPD_CanHandleURLInService(GF_InputService *plug, const char *url)
{
	/**
	 * May arrive when using pid:// URLs into a TS stream
	 */
	GF_MPD_In *mpdin = (GF_MPD_In*) plug->priv;
    GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Can Handle URL In Service (%p) request from terminal for %s\n", mpdin->service, url));
    if (!plug || !plug->priv) return GF_SERVICE_ERROR;
    if (mpdin->url && !strcmp(mpdin->url, url)) {
        return 1;
    } else {
	GF_InputService *segment_ifce = NULL;
        GF_MPD_Group *group = NULL;
        if (mpdin->group_zero_selected)
		segment_ifce = mpdin->group_zero_selected->input_module;
	if (segment_ifce && segment_ifce->CanHandleURLInService){
		return segment_ifce->CanHandleURLInService(plug, url);
	}
        return 0;
    }
}

GF_EXPORT
const u32 *QueryInterfaces()
{
    static u32 si [] = {
        GF_NET_CLIENT_INTERFACE,
        0
    };
    return si;
}

GF_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
    GF_MPD_In *mpdin;
    GF_InputService *plug;
    if (InterfaceType != GF_NET_CLIENT_INTERFACE) return NULL;

    GF_SAFEALLOC(plug, GF_InputService);
    GF_REGISTER_MODULE_INTERFACE(plug, GF_NET_CLIENT_INTERFACE, "GPAC MPD Loader", "gpac distribution")
    plug->RegisterMimeTypes = MPD_RegisterMimeTypes;
    plug->CanHandleURL = MPD_CanHandleURL;
    plug->ConnectService = MPD_ConnectService;
    plug->CloseService = MPD_CloseService;
    plug->GetServiceDescriptor = MPD_GetServiceDesc;
    plug->ConnectChannel = MPD_ConnectChannel;
    plug->DisconnectChannel = MPD_DisconnectChannel;
    plug->ServiceCommand = MPD_ServiceCommand;
    plug->CanHandleURLInService = MPD_CanHandleURLInService;
    plug->ChannelGetSLP = MPD_ChannelGetSLP;
    plug->ChannelReleaseSLP = MPD_ChannelReleaseSLP;
    GF_SAFEALLOC(mpdin, GF_MPD_In);
    plug->priv = mpdin;
    mpdin->mpd_thread = gf_th_new("MPD Segment Downloader Thread");
    mpdin->dl_mutex = gf_mx_new("MPD Segment Downloader Mutex");
    mpdin->mimeTypeForM3U8Segments = gf_strdup( M3U8_UNKOWN_MIME_TYPE );
    return (GF_BaseInterface *)plug;
}

GF_EXPORT
void ShutdownInterface(GF_BaseInterface *bi)
{
    GF_InputService *ifcn = (GF_InputService*)bi;
    if (ifcn->InterfaceType==GF_NET_CLIENT_INTERFACE) {
        GF_MPD_In *mpdin = (GF_MPD_In*)ifcn->priv;
	assert( mpdin );
        if (mpdin->mpd)
            gf_mpd_del(mpdin->mpd);
        mpdin->mpd = NULL;
	if (mpdin->url)
	  gf_free(mpdin->url);
	mpdin->url = NULL;
	if (mpdin->mpd_thread)
          gf_th_del(mpdin->mpd_thread);
        mpdin->mpd_thread = NULL;
	if (mpdin->dl_mutex)
          gf_mx_del(mpdin->dl_mutex);
        mpdin->dl_mutex = NULL;
        if (mpdin->mimeTypeForM3U8Segments)
		gf_free(mpdin->mimeTypeForM3U8Segments);
	mpdin->mimeTypeForM3U8Segments = NULL;
        gf_free(mpdin);
	ifcn->priv = NULL;
        gf_free(bi);
    }
}
