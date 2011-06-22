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


/*!
 * All the possible Mime-types for MPD files
 */
static const char * MPD_MIME_TYPES[] = { "video/vnd.3gpp.mpd", "audio/vnd.3gpp.mpd", NULL };

/*!
 * All the possible Mime-types for M3U8 files
 */
static const char * M3U8_MIME_TYPES[] = { "video/x-mpegurl", "audio/x-mpegurl", "application/x-mpegurl", "application/vnd.apple.mpegurl", NULL};

GF_Err MPD_downloadWithRetry( GF_ClientService * service, GF_DownloadSession ** sess, const char *url, gf_dm_user_io user_io,  void *usr_cbk, u64 start_range, u64 end_range, Bool persistent);

typedef struct
{
    char *cache;
    char *url;
} segment_cache_entry;

typedef struct __mpd_group 
{
	GF_List *representations;
	u32 group_id;
	Bool selected;
	Bool done;
	Bool force_switch_bandwidth;
	u32 nb_bw_check;
	/*pointer toactive period*/
	GF_MPD_Period *period;
	/*active representation index in period->representations*/
	u32 active_rep_index;
	u32 active_bitrate, max_bitrate, min_bitrate;
	/*local file playback, do not delete them*/
	Bool local_files;
	/*next segment to download for this group*/
	u32 download_segment_index;

	/*next file (cached) to delete at next GF_NET_SERVICE_QUERY_NEXT for this group*/
    char * urlToDeleteNext;
    volatile u32 max_cached, nb_cached;
    segment_cache_entry *cached;

    GF_DownloadSession *segment_dnload;
    const char *segment_local_url;

    u32 nb_segments_done;

    Bool segment_must_be_streamed;

	/* Service really managing the segments */
    GF_InputService *service;
    char *service_mime;
    Bool service_connected;
} GF_MPD_Group;

typedef struct __mpd_module {
    /* GPAC Service object (i.e. how this module is seen by the terminal)*/
    GF_ClientService *service;
    /* URL to which this service is connected
       Used to detect when audio service connection request is made on the same URL as video */
    char *url;

    u32 option_max_cached;
    u32 auto_switch_count;
    Bool keep_files;

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

	/*list of groups in the active period*/
	GF_List *groups;
	/*group 0 if present, NULL otherwise*/
	GF_MPD_Group *group_zero_selected;

    /*Main MPD Thread handling segment downloads and MPD/M3U8 update*/
    GF_Thread *mpd_thread;
	/*mutex for group->cache file name access and MPD update*/
    GF_Mutex *dl_mutex;

    /* 0 not started, 1 download in progress */
    Bool mpd_is_running;
    Bool mpd_stop_request;


    /* TODO - handle playback status for SPEED/SEEK through SIDX */
    Double playback_speed, playback_start_range, playback_end_range;
} GF_MPD_In;


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
		if (!group->service_mime) group->service_mime = gf_strdup(param->value);
		else if (strcmp(group->service_mime, param->value)) {
			GF_MPD_Representation *rep = gf_list_get(group->period->representations, group->active_rep_index);
			if (!rep->mime) rep->mime = gf_strdup(param->value);
			rep->disabled = 1;
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
		if (gf_dm_sess_get_stats(group->segment_dnload, NULL, NULL, NULL, NULL, &download_rate, NULL) == GF_OK) {
			if (download_rate) {
				download_rate *= 8;
				if (download_rate<group->min_bitrate) group->min_bitrate = download_rate;
				if (download_rate>group->max_bitrate) group->max_bitrate = download_rate;

				if (download_rate && (download_rate < group->active_bitrate)) {
					fprintf(stdout, "Downloading from group %d at rate %d kbps but group bitrate is %d kbps\n", group->group_id, download_rate/1024, group->active_bitrate/1024);
					group->nb_bw_check ++;
					if (group->nb_bw_check>2) {
						fprintf(stdout, "Downloading from group %d at rate %d kbps but group bitrate is %d kbps - switching\n", group->group_id, download_rate/1024, group->active_bitrate/1024);
						group->force_switch_bandwidth = 1;
						gf_dm_sess_abort(group->segment_dnload);
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

static Bool MPD_segments_are_equals(GF_MPD_SegmentInfo * info1, GF_MPD_SegmentInfo * info2) {
    assert( info1 );
    assert( info2 );
    return info1->byterange_end == info2->byterange_end &&
           info1->byterange_start == info2->byterange_start &&
           info1->use_byterange == info2->use_byterange &&
           (info1->url == info2->url || !strcmp(info1->url, info2->url));
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
    u32 i, j, rep_idx, group_idx;
    Bool seg_found = 0;
    GF_DOMParser *mpd_parser;
    GF_MPD_Period *period, *new_period;
    GF_MPD_Representation *rep, *new_rep;
    GF_List *segs;
    const char *local_url;
    char mime[128];
    char * purl;
    Bool is_m3u8 = 0;
    u32 oldUpdateTime = mpdin->mpd->min_update_time;
    /*reset update time - if any error occurs, we will no longer attempt to update the MPD*/
    mpdin->mpd->min_update_time = 0;

    if (!mpdin->mpd_dnload) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot update playlist: missing downloader\n"));
        return GF_BAD_PARAM;
    }

    local_url = gf_dm_sess_get_cache_name(mpdin->mpd_dnload);
    if (!local_url) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot update playlist: wrong cache file %s\n", local_url));
        return GF_IO_ERR;
    }
    gf_delete_file(local_url);
    purl = gf_strdup(gf_dm_sess_get_resource_name(mpdin->mpd_dnload));
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
        gf_m3u8_to_mpd(mpdin->service, local_url, purl, NULL, mpdin->reload_count, mpdin->mimeTypeForM3U8Segments);
    } else if (!MPD_is_MPD_mime(mime)) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] mime '%s' should be m3u8 or mpd\n", mime));
        gf_term_on_connect(mpdin->service, NULL, GF_BAD_PARAM);
        gf_free(purl);
        purl = NULL;
        return GF_BAD_PARAM;
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
                mpdin->mpd->min_update_time = oldUpdateTime;
            } else {
                GF_MPD *new_mpd;
                GF_MPD_SegmentInfo *info1;
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

                if (gf_list_count(period->representations) != gf_list_count(new_period->representations)) {
                    GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot update playlist: missing representation\n"));
                    gf_mpd_del(new_mpd);
                    return GF_NON_COMPLIANT_BITSTREAM;
                }

				for (group_idx=0; group_idx<gf_list_count(mpdin->groups); group_idx++) {
					GF_MPD_Group *group = gf_list_get(mpdin->groups, group_idx);
					if (!group->selected) continue;

					rep = gf_list_get(period->representations, group->active_rep_index);
					info1 = gf_list_get(rep->segments, group->download_segment_index - 1);

					for (rep_idx = 0; rep_idx<gf_list_count(period->representations); rep_idx++) {
						rep = gf_list_get(period->representations, rep_idx);
						new_rep = gf_list_get(new_period->representations, rep_idx);
						if (!new_rep) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot update playlist: missing representation in period\n"));
							gf_mpd_del(new_mpd);
							return GF_NON_COMPLIANT_BITSTREAM;
						}
						/*merge init segment*/
						if (new_rep->init_url) {
							seg_found = 0;

							if (!strcmp(new_rep->init_url, rep->init_url)) {
								seg_found = 1;
							} else {
								for (j=0; j<gf_list_count(rep->segments); j++) {
									GF_MPD_SegmentInfo *seg = gf_list_get(rep->segments, j);
									if (!strcmp(new_rep->init_url, seg->url)) {
										seg_found = 1;
										break;
									}
								}
							}
							/*remove from new list and push to old one*/
							if (!seg_found) {
								GF_MPD_SegmentInfo *new_seg;
								GF_SAFEALLOC(new_seg, GF_MPD_SegmentInfo);
								new_seg->url = gf_strdup(new_rep->init_url);
								new_seg->use_byterange = new_rep->init_use_range;
								new_seg->byterange_start = new_rep->init_byterange_start;
								new_seg->byterange_end = new_rep->init_byterange_end;
								gf_list_add(rep->segments, new_seg);
								GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Representation #%d: Adding new segment from initialization segment %s\n", rep_idx+1, new_seg->url));
							}
						}

						/*merge segment list*/
						for (i=0; i<gf_list_count(new_rep->segments); i++) {
							GF_MPD_SegmentInfo *new_seg = gf_list_get(new_rep->segments, i);
							assert( new_seg );
							assert( new_seg->url);
							seg_found = 0;
							for (j=0; j<gf_list_count(rep->segments); j++) {
								GF_MPD_SegmentInfo *seg = gf_list_get(rep->segments, j);
								assert( seg );
								assert( seg->url);
								if (!strcmp(new_seg->url, seg->url)) {
									seg_found = 1;
									break;
								}
							}
							/*remove from new list and push to old one*/
							if (!seg_found) {
								gf_list_rem(new_rep->segments, i);
								gf_list_add(rep->segments, new_seg);
								GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Representation #%d: Adding new segment %s\n", rep_idx+1, new_seg->url));
								i--;
							}
						}
						/*swap lists*/
						{
							GF_MPD_SegmentInfo *info2;
							segs = new_rep->segments;
							new_rep->segments = rep->segments;
							new_rep->disabled = rep->disabled;
							if (!new_rep->mime) {
								new_rep->mime = rep->mime;
								rep->mime = NULL;
							}
							rep->segments = segs;
							info2 = gf_list_get(rep->segments, group->download_segment_index);
						}
					}
					/*update group/period to new period*/
					group->period = new_period;

					/*and rebuild representations for this group*/
					gf_list_reset(group->representations);
					for (i=0; i<gf_list_count(new_period->representations); i++) {
					    GF_MPD_Representation *rep = gf_list_get(new_period->representations, i);
						if (rep->groupID==group->group_id) gf_list_add(group->representations, rep);
					}
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
    if (!param || !ifce || !ifce->proxy_udta) return GF_BAD_PARAM;

	if (param->command_type==GF_NET_SERVICE_QUERY_NEXT) {
        u32 timer = gf_sys_clock();
        GF_MPD_In *mpdin = (GF_MPD_In *) ifce->proxy_udta;
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Service Query Next request from terminal\n"));
        gf_mx_p(mpdin->dl_mutex);

		for (i=0; i<gf_list_count(mpdin->groups); i++) {
			group = gf_list_get(mpdin->groups, i);
			if (group->selected && (group->service == ifce)) break;
			group = NULL;
		}
		
		if (!group) {
	        gf_mx_v(mpdin->dl_mutex);
			return GF_SERVICE_ERROR;
		}

        /* Wait until no file is scheduled to be downloaded */
        while (mpdin->mpd_is_running && group->nb_cached<2) {
            gf_mx_v(mpdin->dl_mutex);
			if (group->done) {
				return GF_EOS;
			}
            gf_sleep(16);
            gf_mx_p(mpdin->dl_mutex);
        }
        if (group->nb_cached<2) {
            GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[MPD_IN] No more file in cache, EOS\n"));
            gf_mx_v(mpdin->dl_mutex);
            return GF_EOS;
        } else {
            GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Had to wait for %u ms for the only cache file to be downloaded\n", (gf_sys_clock() - timer)));
        }
        if (group->cached[0].cache) {
            if (group->urlToDeleteNext) {
				if (!group->local_files && !mpdin->keep_files)
			        gf_dm_delete_cached_file_entry_session(mpdin->mpd_dnload, group->urlToDeleteNext);

				gf_free( group->urlToDeleteNext);
                group->urlToDeleteNext = NULL;
            }
            assert( group->cached[0].url );
            GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] deleting cache file %s : %s\n", group->cached[0].url, group->cached[0].cache));
            group->urlToDeleteNext = gf_strdup( group->cached[0].url );
            gf_free(group->cached[0].cache);
            gf_free(group->cached[0].url);
            group->cached[0].url = NULL;
            group->cached[0].cache = NULL;
        }
        memmove(&group->cached[0], &group->cached[1], sizeof(segment_cache_entry)*(group->nb_cached-1));
        memset(&(group->cached[group->nb_cached-1]), 0, sizeof(segment_cache_entry));
        group->nb_cached--;
        param->url_query.next_url = group->cached[0].cache;
        gf_mx_v(mpdin->dl_mutex);
        {
            u32 timer2 = gf_sys_clock() - timer ;
            if (timer2 > 1000) {
                GF_LOG(GF_LOG_WARNING, GF_LOG_MODULE, ("[MPD_IN] We were stuck waiting for download to end during too much time : %u ms !\n", timer2));
            }
            GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[MPD_IN] Switching segment playback to \n\tURL: %s in %u ms\n\tCache: %s\n\tElements in cache: %u/%u\n", group->cached[0].url, timer2, group->cached[0].cache, group->nb_cached, group->max_cached));
        }
    } else {
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Client Query request (%d) from terminal\n", param->command_type));
    }
    return GF_OK;
}

static GF_Err MPD_LoadMediaService(GF_MPD_In *mpdin, GF_MPD_Group *group, const char *mime)
{
    const char *sPlug;
    /* Check MIME type to start the right InputService (ISOM or MPEG-2) */
    sPlug = gf_cfg_get_key(mpdin->service->term->user->config, "MimeTypes", mime);
    if (sPlug) sPlug = strrchr(sPlug, '"');
    if (sPlug) {
        sPlug += 2;
        group->service = (GF_InputService *) gf_modules_load_interface_by_name(mpdin->service->term->user->modules, sPlug, GF_NET_CLIENT_INTERFACE);
        if (group->service) {
            group->service->proxy_udta = mpdin;
            group->service->query_proxy = MPD_ClientQuery;
        } else {
            goto exit;
        }
    } else {
        goto exit;
    }
    return GF_OK;
exit:
    GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error locating plugin for segment mime type: %s\n", mime));
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
		e = gf_dm_sess_setup_from_url(*sess, url);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Cannot resetup session for url %s: %s\n", url, gf_error_to_string(e) ));
			return e;
		}

	}
	if (end_range) {
		e = gf_dm_sess_set_range(*sess, start_range, end_range);
		if (e) {
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

static void MPD_SwitchGroupRepresentation(GF_MPD_In *mpd, GF_MPD_Group *group)
{
	u32 i, bandwidth, min_bandwidth;
	GF_MPD_Representation *rep_sel = NULL;
	GF_MPD_Representation *min_rep_sel = NULL;
	Bool min_bandwidth_selected = 0;
	bandwidth = 0;
	min_bandwidth = (u32) -1;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPDIn] Checking representations between %d and %d kbps\n", group->min_bitrate/1024, group->max_bitrate/1024));

	for (i=0; i<gf_list_count(group->representations); i++) {
		GF_MPD_Representation *rep = gf_list_get(group->representations, i);
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
	if (!rep_sel) {
		rep_sel = min_rep_sel;
		min_bandwidth_selected = 1;
	}
	assert(rep_sel);
	i = gf_list_find(group->period->representations, rep_sel);

	assert((s32) i >= 0);

	group->force_switch_bandwidth = 0;
	group->max_bitrate = 0;
	group->min_bitrate = (u32) -1;

	if (i != group->active_rep_index) {
		if (min_bandwidth_selected) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPDIn] No representation found with bandwidth below %d kbps - using representation @ %d kbps\n", group->max_bitrate/1024, rep_sel->bandwidth/1024));
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPDIn] Switching to representation bandwidth %d kbps for download bandwidth %d kbps\n", rep_sel->bandwidth/1024, group->max_bitrate/1024));
		}
		group->active_rep_index = i;
		group->active_bitrate = rep_sel->bandwidth;
	}
}

static GF_Err MPD_DownloadInitSegment(GF_MPD_In *mpdin, GF_MPD_Group *group)
{
    GF_Err e;
    char *base_init_url;
    char * url_to_dl;
    GF_MPD_Representation *rep;
	u64 start_range, end_range;
    /* This variable is 0 if there is a initURL, the index of first segment downloaded otherwise */
    u32 firstSegment = 0;
    if (!mpdin || !group)
        return GF_BAD_PARAM;
    gf_mx_p(mpdin->dl_mutex);
    
	assert( group->period && group->period->representations );
    rep = gf_list_get(group->period->representations, group->active_rep_index);
    if (!rep) {
        gf_mx_v(mpdin->dl_mutex);
        GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Unable to find any representation, aborting.\n"));
        return GF_IO_ERR;
    }
	start_range = end_range = 0;
    if (!rep->init_url) {
        GF_MPD_SegmentInfo * seg = gf_list_get(rep->segments, 0);
        /* No init URL provided, we have to download the first segment then */
        if (!seg->url) {
            mpdin->mpd_stop_request = 1;
            gf_mx_v(mpdin->dl_mutex);
            return GF_BAD_PARAM;
        }
        firstSegment = 1;
        url_to_dl = seg->url;
		if (seg->use_byterange) {
			start_range = seg->byterange_start;
			end_range = seg->byterange_end;
		}
    } else {
        url_to_dl = rep->init_url;
		if (rep->init_use_range) {
			start_range = rep->init_byterange_start;
			end_range = rep->init_byterange_end;
		}
    }
    if (rep->default_base_url) {
        base_init_url = gf_url_concatenate(rep->default_base_url, url_to_dl);
    } else {
        base_init_url = gf_strdup(url_to_dl);
    }


	if (!strstr(base_init_url, "://") || !strnicmp(base_init_url, "file://", 7)) {
        assert(!group->nb_cached);
        group->cached[0].cache = gf_strdup(base_init_url);
        group->cached[0].url = gf_strdup(base_init_url);
        group->nb_cached = 1;
		/*do not erase local files*/
		group->local_files = 1;
        group->download_segment_index = firstSegment;
        group->segment_local_url = group->cached[0].cache;
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Setup initialization segment %s \n", group->segment_local_url));
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
        GF_MPD_SegmentInfo * seg = gf_list_get(rep->segments, 1);
        gf_free(base_init_url);
        if (!seg || !seg->url) {
            gf_mx_v(mpdin->dl_mutex);
            return e;
        }
        if (rep->default_base_url) {
            base_init_url = gf_url_concatenate(rep->default_base_url, seg->url);
        } else {
            base_init_url = gf_strdup(seg->url);
        }
        GF_LOG(GF_LOG_WARNING, GF_LOG_MODULE, ("Download of first segment failed... retrying with second one : %s\n", base_init_url));
        firstSegment = 2;
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
        u32 count = gf_list_count(rep->segments) + 1;
        if (count < group->max_cached) {
            if (count < 1) {
                GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] 0 representations, aborting\n"));
                gf_free(base_init_url);
		        gf_mx_v(mpdin->dl_mutex);
                return GF_BAD_PARAM;
            }
            GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[MPD_IN] Resizing to %u max_cached elements instead of %u.\n", count, group->max_cached));
            /* OK, we have a problem, it may ends download */
            group->max_cached = count;
        }
        e = gf_dm_sess_process(group->segment_dnload);
        /* Mime-Type check */
		strncpy(mime, gf_dm_sess_mime_type(group->segment_dnload), sizeof(mime));
        strlwr(mime);
        if (mime && group->service == NULL) {
            GF_Err e;
            GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Searching an input plugin for mime type : %s...\n", mime));
            gf_free( mpdin->mimeTypeForM3U8Segments);
            mpdin->mimeTypeForM3U8Segments = gf_strdup( mime );
            gf_free( rep->mime);
            rep->mime = gf_strdup( mime );
            e = MPD_LoadMediaService(mpdin, group, mime);
			if (e != GF_OK) {
		        gf_mx_v(mpdin->dl_mutex);
                return e;
			}
        }
        if (!mime || (stricmp(mime, rep->mime))) {
            GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Mime '%s' is not correct for '%s', it should be '%s'\n", mime, base_init_url, rep->mime));
            mpdin->mpd_stop_request = 0;
            gf_mx_v(mpdin->dl_mutex);
            gf_free(base_init_url);
            base_init_url = NULL;
            return GF_BAD_PARAM;
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
        } else {
            assert(!group->nb_cached);
            group->cached[0].cache = gf_strdup(group->segment_local_url);
            group->cached[0].url = gf_strdup(gf_dm_sess_get_resource_name(group->segment_dnload));
            group->nb_cached = 1;
            group->download_segment_index = firstSegment;
            GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Adding initialization segment %s to cache: %s\n", group->segment_local_url, group->cached[0].url ));
            gf_mx_v(mpdin->dl_mutex);
            gf_free(base_init_url);
            return GF_OK;
        }
    }
}

static void MPDIn_skip_disabled_rep(GF_MPD_Group *group, GF_MPD_Representation *rep)
{
	s32 rep_idx = gf_list_find(group->representations, rep);
	while (1) {
		rep_idx++;
		if (rep_idx==gf_list_count(group->representations)) rep_idx = 0;
		rep = gf_list_get(group->representations, rep_idx);
		if (!rep->disabled) break;
	}
	assert(rep && !rep->disabled);
	group->active_rep_index = gf_list_find(group->period->representations, rep);
	group->active_bitrate = rep->bandwidth;
	GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[MPD_IN] Switching to representation %d - BW %d\n", group->active_rep_index, group->active_bitrate ));
}

static u32 download_segments(void *par)
{
    GF_Err e;
    GF_MPD_In *mpdin = (GF_MPD_In*) par;
    GF_MPD_Period *period;
    GF_MPD_Representation *rep;
    GF_MPD_SegmentInfo *seg;
    u32 i, group_count;
	Bool go_on = 1;
    char *new_base_seg_url;
    assert(mpdin);
    if (!mpdin->mpd){
		GF_LOG(GF_LOG_WARNING, GF_LOG_MODULE, ("[MPD_IN] Incorrect state, no mpdin->mpd for URL=%s, already stopped ?\n", mpdin->url));
      return 1;
    }
    /* Setting the download status in exclusive code */
    gf_mx_p(mpdin->dl_mutex);
    mpdin->mpd_is_running = 1;
    gf_mx_v(mpdin->dl_mutex);

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
        return 1;
    }

    mpdin->last_update_time = gf_sys_clock();

	/* Forward the ConnectService message to the appropriate service for this type of segment */
	for (i=0; i<group_count; i++) {
		GF_MPD_Group *group = gf_list_get(mpdin->groups, i);
		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[MPD_IN] Connecting initial service... %s\n", group->segment_local_url));
		group->service->ConnectService(group->service, mpdin->service, group->segment_local_url);
		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[MPD_IN] Connecting initial service DONE\n", group->segment_local_url));
	}

	while (go_on) {
		const char *local_file_name = NULL;
		const char *resource_name = NULL;
        /*wait until next segment is needed*/
        while (!mpdin->mpd_stop_request) {
            u32 timer = gf_sys_clock() - mpdin->last_update_time;
            Bool shouldParsePlaylist = mpdin->mpd->min_update_time && (timer > mpdin->mpd->min_update_time);

			if (shouldParsePlaylist) {
                GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Next segment in cache, but it is time to update the playlist (%u ms/%u)\n", timer, mpdin->mpd->min_update_time));
                e = MPD_UpdatePlaylist(mpdin);
				group_count = gf_list_count(mpdin->groups);
                if (e) {
                    GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error updating MDP %s\n", gf_error_to_string(e)));
                }
            } else {
				Bool cache_full = 1;
			    gf_mx_p(mpdin->dl_mutex);
				for (i=0; i<group_count; i++) {
					GF_MPD_Group *group = gf_list_get(mpdin->groups, i);
					if (!group->selected || group->done) continue;
					if (group->nb_cached<group->max_cached) {
						cache_full = 0;
						break;
					}
				}
	            gf_mx_v(mpdin->dl_mutex);
				if (!cache_full) break;
				
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
			GF_MPD_Group *group = gf_list_get(mpdin->groups, i);
			if (! group->selected) continue;
			if (group->done) continue;


			if (group->nb_cached>=group->max_cached) {
				continue;
			}

			rep = gf_list_get(period->representations, group->active_rep_index);

			/* if the index of the segment to be downloaded is greater or equal to the last segment (as seen in the playlist),
			   we need to check if a new playlist is ready */
			if (group->download_segment_index>=gf_list_count(rep->segments)) {
				u32 timer = gf_sys_clock() - mpdin->last_update_time;
				/* update of the playlist, only if indicated */
				if (mpdin->mpd->min_update_time && timer > mpdin->mpd->min_update_time) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Last segment in current playlist downloaded, checking updates after %u ms\n", timer));
					e = MPD_UpdatePlaylist(mpdin);
					if (e) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error updating MDP %s\n", gf_error_to_string(e)));
					}
					group_count = gf_list_count(mpdin->groups);
					period = gf_list_get(mpdin->mpd->periods, mpdin->active_period_index);
					rep = gf_list_get(period->representations, group->active_rep_index);
				} else {
					gf_sleep(16);
				}
				/* Now that the playlist is up to date, we can check again */
				if (group->download_segment_index >= gf_list_count(rep->segments)) {
					if (mpdin->mpd->min_update_time) {
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
			seg = gf_list_get(rep->segments, group->download_segment_index);
			gf_mx_v(mpdin->dl_mutex);

			if (!seg->url) {
				if (rep->init_url)
					new_base_seg_url = gf_url_concatenate(rep->default_base_url, rep->init_url);
				else
					new_base_seg_url = strdup(rep->default_base_url);
			} else if (rep->default_base_url) {
				new_base_seg_url = gf_url_concatenate(rep->default_base_url, seg->url);
			} else {
				new_base_seg_url = gf_strdup(seg->url);
			}
			if (seg->use_byterange) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Downloading new segment: %s (range: %d-%d)\n", new_base_seg_url, seg->byterange_start, seg->byterange_end));
			}

			/*local file*/
			if (!strstr(new_base_seg_url, "://") || !strnicmp(new_base_seg_url, "file://", 7)) {
				/*byte-range request from file are ignored*/
				if (seg->use_byterange) {
					group->done = 1;
					break;
				}
				resource_name = local_file_name = (char *) new_base_seg_url; 
				e = GF_OK;
				/*do not erase local files*/
				group->local_files = 1;
			} else {
				group->max_bitrate = 0;
				group->min_bitrate = (u32)-1;
				/*use persistent connection for segment downloads*/
				if (seg->use_byterange) {
					e = MPD_downloadWithRetry(mpdin->service, &(group->segment_dnload), new_base_seg_url, MPD_NetIO_Segment, group, seg->byterange_start, seg->byterange_end, 1);
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

				{
					u32 total_size, bytes_per_sec;
					Double bitrate;
					gf_dm_sess_get_stats(group->segment_dnload, NULL, NULL, &total_size, NULL, &bytes_per_sec, NULL);
					bitrate = 8*total_size;
					bitrate *= 1000;
					bitrate /= rep->default_segment_duration;
					bitrate /= 1024;
					fprintf(stdout, "Downloaded segment bitrate %d kbps with bandwith %d kbps\n", (u32) bitrate, 8*bytes_per_sec/1024);
					/*TODO switch quality*/

					if (!mpdin->auto_switch_count) {
						MPD_SwitchGroupRepresentation(mpdin, group);
					}
				}
			}

			if (e == GF_OK || group->segment_must_be_streamed) {
				gf_mx_p(mpdin->dl_mutex);
				assert(group->nb_cached<group->max_cached);
				group->cached[group->nb_cached].cache = gf_strdup(local_file_name);
				group->cached[group->nb_cached].url = gf_strdup( resource_name );
				GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[MPD_IN] Added file to cache\n\tURL: %s\n\tCache: %s\n\tElements in cache: %u/%u\n", group->cached[group->nb_cached].url, group->cached[group->nb_cached].cache, group->nb_cached+1, group->max_cached));
				group->nb_cached++;
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

		if (!go_on) break;

    }

    /* Signal that the download thread has ended */
    gf_mx_p(mpdin->dl_mutex);
    mpdin->mpd_is_running = 0;
    gf_mx_v(mpdin->dl_mutex);
    return 0;
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
	if (mpdin->group_zero_selected) return mpdin->group_zero_selected->service;
	ch = (GF_Channel *) channel;
	assert(ch && ch->odm && ch->odm->OD);

	return (GF_InputService *) ch->odm->OD->service_ifce;
}

GF_MPD_Group *MPD_GetGroupForInputService(GF_MPD_In *mpdin, GF_InputService *ifce)
{
	u32 i;
	for (i=0; i<gf_list_count(mpdin->groups); i++) {
		GF_MPD_Group *group = gf_list_get(mpdin->groups, i);
		if (group->service==ifce) return group;
	}
	return NULL;
}

GF_Err MPD_ConnectChannel(GF_InputService *plug, LPNETCHANNEL channel, const char *url, Bool upstream)
{
    GF_MPD_In *mpdin = (GF_MPD_In*) plug->priv;
	GF_InputService *segment_ifce = MPD_GetInputServiceForChannel(mpdin, channel);
    if (!plug || !plug->priv || !segment_ifce) return GF_SERVICE_ERROR;
    GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Channel Connection (0x%x) request from terminal for %s\n", channel, url));
	return segment_ifce->ConnectChannel(segment_ifce, channel, url, upstream);
}

GF_Err MPD_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
    GF_MPD_In *mpdin = (GF_MPD_In*) plug->priv;
	GF_InputService *segment_ifce = MPD_GetInputServiceForChannel(mpdin, channel);
    if (!plug || !plug->priv || !segment_ifce) return GF_SERVICE_ERROR;
    GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Disconnect channel (0x%x) request from terminal \n", channel));

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
	for (i=0; i<gf_list_count(mpdin->groups); i++) {
		GF_MPD_Group *group = gf_list_get(mpdin->groups, i);
		if (group->selected && group->segment_dnload) {
			gf_dm_sess_abort(group->segment_dnload);
			group->done = 1;
		}
	}
    /* stop the download thread */
    gf_mx_p(mpdin->dl_mutex);
    if (mpdin->mpd_is_running == 1) {
        mpdin->mpd_stop_request = 1;
        gf_mx_v(mpdin->dl_mutex);
        while (1) {
            /* waiting for the download thread to stop */
            gf_sleep(16);
            gf_mx_p(mpdin->dl_mutex);
            if (mpdin->mpd_is_running == 0) {
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

void MPD_ResetGroups(GF_MPD_In *mpdin)
{
	while (gf_list_count(mpdin->groups)) {
		GF_MPD_Group *group = gf_list_last(mpdin->groups);
		gf_list_rem_last(mpdin->groups);
		gf_list_del(group->representations);

		if (group->urlToDeleteNext) {
			if (!mpdin->keep_files && !group->local_files)
				gf_dm_delete_cached_file_entry_session(mpdin->mpd_dnload, group->urlToDeleteNext);
	    
			gf_free(group->urlToDeleteNext);
			group->urlToDeleteNext = NULL;
		}
		if (group->segment_dnload) {
			gf_term_download_del(group->segment_dnload);
			group->segment_dnload = NULL;
		}
		while (group->nb_cached) {
			group->nb_cached --;
			if (!mpdin->keep_files && !group->local_files)
				gf_delete_file(group->cached[group->nb_cached].cache);

			gf_free(group->cached[group->nb_cached].cache);
			gf_free(group->cached[group->nb_cached].url);
		}
		gf_free(group->cached);
		free(group);
	}
	gf_list_del(mpdin->groups);
	mpdin->groups = NULL;
}

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

	count = gf_list_count(period->representations);
	for (i=0; i<count; i++) {
        Bool found = 0;
		GF_MPD_Representation *rep = gf_list_get(period->representations, i);
		for (j=0; j<gf_list_count(mpdin->groups); j++) {
			GF_MPD_Group *group = gf_list_get(mpdin->groups, j);
			if (group->group_id==rep->groupID) {
		        found = 1;
				e = gf_list_add(group->representations, rep);
				if (e) return e;
				break;
			}
		}
		if (!found) {
			GF_MPD_Group *group;
			GF_SAFEALLOC(group, GF_MPD_Group);
			if (!group) return GF_OUT_OF_MEM;
			group->representations = gf_list_new();
			if (!group->representations) {
				gf_free(group);
				return GF_OUT_OF_MEM;
			}
			group->group_id = rep->groupID;
			group->period = period;
			group->max_cached = mpdin->option_max_cached;
			group->cached = gf_malloc(sizeof(segment_cache_entry)*group->max_cached);
			memset(group->cached, 0, sizeof(segment_cache_entry)*group->max_cached);
			if (!group->cached) {
				gf_list_del(group->representations);
				gf_free(group);
				return GF_OUT_OF_MEM;
			}
			e = gf_list_add(mpdin->groups, group);
			if (e) {
				gf_free(group->cached);
				gf_list_del(group->representations);
				gf_free(group);
				return e;
			}

			e = gf_list_add(group->representations, rep);
			if (e) return e;

		}
	}
	return GF_OK;
}

static GF_Err MPD_SegmentsProcessStart(GF_MPD_In *mpdin, u32 time)
{
    GF_Err e;
    u32 rep_i, group_i;
    GF_MPD_Period *period;
    e = GF_BAD_PARAM;

#if 0
    MPD_DownloadStop(mpdin);
#endif

	MPD_ResetGroups(mpdin);

	/* Get the right period from the given time */
    mpdin->active_period_index = MPD_GetPeriodIndexFromTime(mpdin, time);
    period = gf_list_get(mpdin->mpd->periods, mpdin->active_period_index);
    if (!period || !gf_list_count(period->representations) ) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot start: not enough periods or representations in MPD\n"));
        goto exit;
    }

	/*setup all groups*/
	MPD_SetupGroups(mpdin);
	mpdin->group_zero_selected = NULL;

	for (group_i=0; group_i<gf_list_count(mpdin->groups); group_i++) {
		GF_MPD_Representation *rep_sel;
		u32 active_rep;
		GF_MPD_Group *group = gf_list_get(mpdin->groups, group_i);

		if (group->group_id==0) {
			mpdin->group_zero_selected = group;
		} else if (mpdin->group_zero_selected) {
			break;
		}

		/* Select the appropriate representation in the given period */
		active_rep = 0;
		for (rep_i = 0; rep_i < gf_list_count(group->representations); rep_i++) {
			GF_MPD_Representation *rep = gf_list_get(group->representations, rep_i);
			rep_sel = gf_list_get(group->representations, active_rep);
			/*by default tune to best quality and/or full bandwith*/
			if (rep->qualityRanking > rep_sel->qualityRanking) {
				active_rep = rep_i;
			} else if (rep->bandwidth > rep_sel->bandwidth) {
				active_rep = rep_i;
			}
		}

		rep_sel = gf_list_get(group->representations, active_rep);
		group->active_rep_index = gf_list_find(group->period->representations, rep_sel);
		group->active_bitrate = rep_sel->bandwidth;


		/* TODO: Generate segment names if urltemplates are used */
		if (!gf_list_count(rep_sel->segments) || !rep_sel->mime) {
			if (!rep_sel->mime) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot start: missing mime\n"));
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot start: missing segments\n"));
			}
			goto exit;
		}

		group->service = NULL;
		if (strcmp(M3U8_UNKOWN_MIME_TYPE, rep_sel->mime)) {
			e = MPD_LoadMediaService(mpdin, group, rep_sel->mime);
			if (e != GF_OK)
				goto exit;
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Ignoring mime type %s, wait for first file...\n", rep_sel->mime));
		}
		group->selected = 1;
	}


	gf_th_run(mpdin->mpd_thread, download_segments, mpdin);
    return GF_OK;

exit:
    gf_term_on_connect(mpdin->service, NULL, e);
    return e;
}

GF_Err MPD_ConnectService(GF_InputService *plug, GF_ClientService *serv, const char *url)
{
    GF_MPD_In *mpdin = (GF_MPD_In*) plug->priv;
    const char *local_url, *opt;
    GF_Err e;
    GF_DOMParser *mpd_parser;
    Bool is_m3u8 = 0;

    GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Service Connection request (0x%x) from terminal for %s\n", serv, url));

    if (!mpdin|| !serv || !url) return GF_BAD_PARAM;

    mpdin->service = serv;
    memset( mpdin->lastMPDSignature, 0, sizeof(mpdin->last_update_time));
    mpdin->reload_count = 0;
    if (mpdin->url)
      gf_free(mpdin->url);
    mpdin->url = gf_strdup(url);
    mpdin->option_max_cached = 0;

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
	
	if (mpdin->mpd_dnload) gf_term_download_del(mpdin->mpd_dnload);
    mpdin->mpd_dnload = NULL;

    if (!strnicmp(url, "file://", 7)) {
        local_url = url + 7;
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
    }

    if (is_m3u8) {
        gf_m3u8_to_mpd(mpdin->service, local_url, url, NULL, mpdin->reload_count, mpdin->mimeTypeForM3U8Segments);
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
        GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot connect service: MPD parsing problem %s\n", gf_error_to_string(e)));
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
		if (mpdin->group_zero_selected->service) {
	        return mpdin->group_zero_selected->service->GetServiceDescriptor(mpdin->group_zero_selected->service, expect_type, sub_url);
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot provide service description: no segment service hanlder created\n"));
			return NULL;
		}
	} else {
		u32 i;
		for (i=0; i<gf_list_count(mpdin->groups); i++) {
			GF_MPD_Group *group = gf_list_get(mpdin->groups, i);
			if (!group->selected) continue;
			if (group->service_connected) continue;
			group->service_connected = 1;
	        return group->service->GetServiceDescriptor(group->service, expect_type, sub_url);
		}
		return NULL;
	}
}

void MPD_Stop(GF_MPD_In *mpdin)
{
    GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Stopping service 0x%x\n", mpdin->service));
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
	u32 i;
    GF_MPD_In *mpdin = (GF_MPD_In*) plug->priv;
    GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Close Service (0x%x) request from terminal\n", mpdin->service));
   
	for (i=0; i<gf_list_count(mpdin->groups); i++) {
		GF_MPD_Group *group = gf_list_get(mpdin->groups, i);
	
		if (group->service && group->service_connected) {
			group->service->CloseService(group->service);
			group->service_connected = 0;
			group->service = NULL;
		}
	}
    MPD_Stop(mpdin);
	MPD_ResetGroups(mpdin);
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

	if (mpdin->group_zero_selected) segment_ifce = mpdin->group_zero_selected->service;

	switch (com->command_type) {
    case GF_NET_SERVICE_INFO:
    {
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Info command from terminal on Service (0x%x)\n", mpdin->service));

		e = GF_OK;
		if (segment_ifce) {
			/* defer to the real input service */
			e = segment_ifce->ServiceCommand(segment_ifce, com);
		}

		if (e!= GF_OK || !com->info.name || 2 > strlen(com->info.name)) {
			com->info.name = mpdin->mpd->title;
			if (!com->info.name && mpdin->group_zero_selected)
	            com->info.name = mpdin->group_zero_selected->cached[0].url;

			if (mpdin->mpd->source)
                com->info.comment = mpdin->mpd->source;
        }
        return GF_OK;
    }
    case GF_NET_SERVICE_HAS_AUDIO:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Has Audio command from terminal on Service (0x%x)\n", mpdin->service));
		if (segment_ifce) {
	        /* defer to the real input service */
		    return segment_ifce->ServiceCommand(segment_ifce, com);
		}
        return GF_NOT_SUPPORTED;
	}
	/*not supported*/
	if (!com->base.on_channel) return GF_NOT_SUPPORTED;
	
	segment_ifce = MPD_GetInputServiceForChannel(mpdin, com->base.on_channel);
	if (!segment_ifce) return GF_NOT_SUPPORTED;
	group = MPD_GetGroupForInputService(mpdin, segment_ifce);


	switch (com->command_type) {
    case GF_NET_CHAN_SET_PADDING:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Set Padding command from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        /* for padding settings, the MPD level should not change anything */
        return segment_ifce->ServiceCommand(segment_ifce, com);
        break;
    case GF_NET_CHAN_SET_PULL:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Set Pull command from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        /* defer to the real input service */
        return segment_ifce->ServiceCommand(segment_ifce, com);
        break;
    case GF_NET_CHAN_INTERACTIVE:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Interactive command from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        /* we are interactive (that's the whole point of MPD) */
        return GF_OK;

	case GF_NET_CHAN_BUFFER:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Buffer query command from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        return segment_ifce->ServiceCommand(segment_ifce, com);
        break;
    case GF_NET_CHAN_BUFFER_QUERY:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received buffer query from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        return segment_ifce->ServiceCommand(segment_ifce, com);
        break;
    case GF_NET_CHAN_DURATION:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Duration query from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        /* Ignore the duration given by the input service and use the one given in the MPD
           Note: the duration of the initial segment will be 0 anyway (in MP4).*/
        {
            Double duration;
            duration = (Double)mpdin->mpd->duration;
            duration /= 1000;
            com->duration.duration = duration;
            return GF_OK;
        }
        break;
    case GF_NET_CHAN_PLAY:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Play command from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        mpdin->playback_speed = com->play.speed;
        mpdin->playback_start_range = com->play.start_range;
        mpdin->playback_end_range = com->play.end_range;
		group->done = 0;
        return segment_ifce->ServiceCommand(segment_ifce, com);

	case GF_NET_CHAN_STOP:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Stop command from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
		group->done = 1;
        return segment_ifce->ServiceCommand(segment_ifce, com);
    case GF_NET_CHAN_PAUSE:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Pause command from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        return segment_ifce->ServiceCommand(segment_ifce, com);
    case GF_NET_CHAN_RESUME:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Resume command from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        return segment_ifce->ServiceCommand(segment_ifce, com);
	case GF_NET_CHAN_SET_SPEED:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Set Speed command from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        /* recording the speed, to change the segment download speed */
        mpdin->playback_speed = com->play.speed;
        return segment_ifce->ServiceCommand(segment_ifce, com);

	case GF_NET_CHAN_CONFIG:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Set Config command from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        /* defer to the real input service */
        return segment_ifce->ServiceCommand(segment_ifce, com);
        break;
    case GF_NET_CHAN_GET_PIXEL_AR:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Pixel Aspect Ratio query from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        /* defer to the real input service */
        return segment_ifce->ServiceCommand(segment_ifce, com);
        break;
    case GF_NET_CHAN_GET_DSI:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Decoder Specific Info query from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        return segment_ifce->ServiceCommand(segment_ifce, com);
        break;
    case GF_NET_CHAN_MAP_TIME:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Map Time query from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        return segment_ifce->ServiceCommand(segment_ifce, com);
        break;
    case GF_NET_CHAN_RECONFIG:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Reconfig command from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        return segment_ifce->ServiceCommand(segment_ifce, com);
        break;
    case GF_NET_CHAN_DRM_CFG:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received DRM query from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        return segment_ifce->ServiceCommand(segment_ifce, com);
        break;
    case GF_NET_CHAN_GET_ESD:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Elementary Stream Descriptor query from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        return segment_ifce->ServiceCommand(segment_ifce, com);
        break;
    case GF_NET_BUFFER_QUERY:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Global Buffer query from terminal on Service (0x%x)\n", mpdin->service));
        return segment_ifce->ServiceCommand(segment_ifce, com);
        break;
    case GF_NET_GET_STATS:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Statistics query from terminal on Service (0x%x)\n", mpdin->service));
        return segment_ifce->ServiceCommand(segment_ifce, com);
        break;
    case GF_NET_IS_CACHABLE:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Cachable query from terminal on Service (0x%x)\n", mpdin->service));
        return segment_ifce->ServiceCommand(segment_ifce, com);
        break;
    case GF_NET_SERVICE_MIGRATION_INFO:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Migration info query from terminal on Service (0x%x)\n", mpdin->service));
        return segment_ifce->ServiceCommand(segment_ifce, com);
        break;
    default:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received unknown command (%d) on Service (0x%x)\n",com->command_type, mpdin->service));
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
	/*JLF: commented out, this shall not happen*/
#if 0
	GF_MPD_In *mpdin = (GF_MPD_In*) plug->priv;
    GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Can Handle URL In Service (0x%x) request from terminal for %s\n", mpdin->service, url));
    if (!plug || !plug->priv) return GF_SERVICE_ERROR;
    if (mpdin->url && !strcmp(mpdin->url, url)) {
        return 1;
    } else {
        return 0;
    }
#endif

	return 0;
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
    memset(mpdin, 0, sizeof(GF_MPD_In));
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
        if (mpdin->mpd)
            gf_mpd_del(mpdin->mpd);
        mpdin->mpd = NULL;
	if (mpdin->url)
	  gf_free(mpdin->url);
	mpdin->url = NULL;
        gf_th_del(mpdin->mpd_thread);
        gf_mx_del(mpdin->dl_mutex);
        gf_free(mpdin->mimeTypeForM3U8Segments);
        gf_free(mpdin);
        gf_free(bi);
    }
}
