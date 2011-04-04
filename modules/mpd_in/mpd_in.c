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

#include <gpac/modules/service.h>
#include <gpac/internal/terminal_dev.h>
#include <gpac/thread.h>
#include <gpac/network.h>
#include <gpac/crypt.h>
#include <gpac/internal/mpd.h>
#include <gpac/internal/m3u8.h>


/*!
 * All the possible Mime-types for MPD files
 */
static const char * MPD_MIME_TYPES[] = { "video/vnd.3gpp.mpd", "audio/vnd.3gpp.mpd", NULL };

/*!
 * All the possible Mime-types for M3U8 files
 */
static const char * M3U8_MIME_TYPES[] = { "video/x-mpegurl", "audio/x-mpegurl", "application/x-mpegurl", "application/vnd.apple.mpegurl", NULL};

typedef struct
{
    char *cache;
    char *url;
} segment_cache_entry;

typedef struct __mpd_module {
    /* GPAC Service object (i.e. how this module is seen by the terminal)*/
    GF_ClientService *service;
    /* URL to which this service is connected
       Used to detect when audio service connection request is made on the same URL as video */
    char *url;
    Bool is_service_connected;
    /* number of time the service has been connected */
    u32 nb_service_connections;
    u32 nb_connected_channels;
    u32 nb_playing_or_paused_channels;

    /* Download of the MPD */
    volatile Bool is_mpd_in_download;

    GF_DownloadSession *mpd_dnload;
    char * urlToDeleteNext;
    volatile u32 max_cached, nb_cached;
    u32 option_max_cached;
    segment_cache_entry *cached;

    /* MPD and active informations */
    GF_MPD *mpd;
    u32 active_period_index;
    u32 active_rep_index;
    u32 download_segment_index;

    /* playback status */
    Double playback_speed;
    Double playback_start_range, playback_end_range;

    /* For Segment downloads */
    GF_DownloadSession *seg_dnload;
    const char *seg_local_url;
    GF_Thread *dl_thread;
    GF_Mutex *dl_mutex;

    /* 0 not started, 1 download in progress */
    Bool is_dl_segments;
    Bool dl_stop_request;

    /* Service really managing the segments */
    GF_InputService *seg_ifce;

    u32 reload_count;
    volatile u32 last_update_time;

    u32 nb_segs_done;
    Bool auto_switch;
    u8 lastMPDSignature[20];
    Bool segment_must_be_streamed;
    char * mimeTypeForM3U8Segments;
} GF_MPD_In;

static void dumpStatus( GF_MPD_In *mpdin) {
    u32 i;
    for (i = 0 ; i < mpdin->nb_cached; i++) {
        printf("\t[%u] - %s\t:\t%s\n", i, mpdin->cached[i].cache, mpdin->cached[i].url);
    }
    printf("\t***********\n\n");
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
    GF_MPD_In *mpdin = (GF_MPD_In*) cbk;

    /*handle service message*/
    gf_term_download_update_stats(mpdin->seg_dnload);
    e = param->error;
    if (param->msg_type == GF_NETIO_PARSE_REPLY) {
        if (! gf_dm_sess_can_be_cached_on_disk(mpdin->seg_dnload)) {
            GF_LOG(GF_LOG_INFO, GF_LOG_MODULE,
                   ("[MPD_IN] Segment %s cannot be cached on disk, will use direct streaming\n", gf_dm_sess_get_resource_name(mpdin->seg_dnload)));
            mpdin->segment_must_be_streamed = 1;
            gf_dm_sess_abort(mpdin->seg_dnload);
        } else {
            mpdin->segment_must_be_streamed = 0;
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

GF_Err MPD_downloadWithRetry( GF_ClientService * service, GF_DownloadSession ** sess, const char *url, gf_dm_user_io user_io,  void *usr_cbk);
static GF_Err MPD_UpdatePlaylist(GF_MPD_In *mpdin)
{
    GF_Err e;
    u32 i, j, rep_idx;
    Bool seg_found = 0;
    GF_DOMParser *mpd_parser;
    GF_MPD_Period *period, *new_period;
    GF_MPD_Representation *rep, *new_rep;
    GF_List *segs;
    const char *local_url, *mime;
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
    e = MPD_downloadWithRetry(mpdin->service, &(mpdin->mpd_dnload), purl, MPD_NetIO, mpdin);
    if (e!=GF_OK) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot update playlist: download problem %s for MPD file\n", gf_error_to_string(e)));
        gf_free(purl);
        return gf_dm_sess_last_error(mpdin->mpd_dnload);
    } else {
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Playlist %s updated with success\n", purl));
    }

    mime = strlwr(gf_dm_sess_mime_type(mpdin->mpd_dnload));

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
                e = gf_mpd_init_from_dom(gf_xml_dom_get_root(mpd_parser), new_mpd);
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

                rep = gf_list_get(period->representations, mpdin->active_rep_index);
                info1 = gf_list_get(rep->segments, mpdin->download_segment_index - 1);

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
                        rep->segments = segs;
                        info2 = gf_list_get(rep->segments, mpdin->download_segment_index);
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
    if (!param || !ifce || !ifce->proxy_udta) return GF_BAD_PARAM;
    if (param->command_type==GF_NET_SERVICE_QUERY_NEXT) {
        u32 timer = gf_sys_clock();
        GF_MPD_In *mpdin = (GF_MPD_In *) ifce->proxy_udta;
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Service Query Next request from terminal\n"));
        gf_mx_p(mpdin->dl_mutex);
        /* Wait until no file is scheduled to be downloaded */
        while (mpdin->is_dl_segments && mpdin->nb_cached<2) {
            gf_mx_v(mpdin->dl_mutex);
            gf_sleep(16);
            gf_mx_p(mpdin->dl_mutex);
        }
        if (mpdin->nb_cached<2) {
            GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[MPD_IN] No more file in cache, EOS\n"));
            gf_mx_v(mpdin->dl_mutex);
            return GF_EOS;
        } else {
            GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Had to wait for %u ms for the only cache file to be downloaded\n", (gf_sys_clock() - timer)));
        }
        if (mpdin->cached[0].cache) {
            if (mpdin->urlToDeleteNext) {
                gf_dm_delete_cached_file_entry_session(mpdin->mpd_dnload, mpdin->urlToDeleteNext);
                gf_free( mpdin->urlToDeleteNext);
                mpdin->urlToDeleteNext = NULL;
            }
            assert( mpdin->cached[0].url );
            GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] deleting cache file %s : %s\n", mpdin->cached[0].url, mpdin->cached[0].cache));
            mpdin->urlToDeleteNext = gf_strdup( mpdin->cached[0].url );
            gf_free(mpdin->cached[0].cache);
            gf_free(mpdin->cached[0].url);
            mpdin->cached[0].url = NULL;
            mpdin->cached[0].cache = NULL;
        }
        memmove(&mpdin->cached[0], &mpdin->cached[1], sizeof(segment_cache_entry)*(mpdin->nb_cached-1));
        memset(&(mpdin->cached[mpdin->nb_cached-1]), 0, sizeof(segment_cache_entry));
        mpdin->nb_cached--;
        param->url_query.next_url = mpdin->cached[0].cache;
        gf_mx_v(mpdin->dl_mutex);
        {
            u32 timer2 = gf_sys_clock() - timer ;
            if (timer2 > 1000) {
                GF_LOG(GF_LOG_WARNING, GF_LOG_MODULE, ("[MPD_IN] We were stuck waiting for download to end during too much time : %u ms !\n", timer2));
            }
            GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[MPD_IN] Switching segment playback to \n\tURL: %s in %u ms\n\tCache: %s\n\tElements in cache: %u/%u\n", mpdin->cached[0].url, timer2, mpdin->cached[0].cache, mpdin->nb_cached, mpdin->max_cached));
        }
    } else {
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Client Query request (%d) from terminal\n", param->command_type));
    }
    return GF_OK;
}

static GF_Err MPD_LoadMediaService(GF_MPD_In *mpdin, const char *mime)
{
    const char *sPlug;
    /* Check MIME type to start the right InputService (ISOM or MPEG-2) */
    sPlug = gf_cfg_get_key(mpdin->service->term->user->config, "MimeTypes", mime);
    if (sPlug) sPlug = strrchr(sPlug, '"');
    if (sPlug) {
        sPlug += 2;
        mpdin->seg_ifce = (GF_InputService *) gf_modules_load_interface_by_name(mpdin->service->term->user->modules, sPlug, GF_NET_CLIENT_INTERFACE);
        if (mpdin->seg_ifce) {
            mpdin->seg_ifce->proxy_udta = mpdin;
            mpdin->seg_ifce->query_proxy = MPD_ClientQuery;
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
GF_Err MPD_downloadWithRetry( GF_ClientService * service, GF_DownloadSession ** sess, const char *url, gf_dm_user_io user_io,  void *usr_cbk)
{
    GF_Err e;
    if (*sess) {
        gf_term_download_del(*sess);
        *sess = NULL;
    }
    GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Downloading %s...\n", url));
    *sess = gf_term_download_new(service, url, GF_NETIO_SESSION_NOT_THREADED, user_io, usr_cbk);
    if (!(*sess)){
	assert(0);
	GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Cannot try to download %s... OUT of memory ?\n", url));
        return GF_OUT_OF_MEM;
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


static GF_Err MPD_DownloadInitSegment(GF_MPD_In *mpdin, GF_MPD_Period *period)
{
    GF_Err e;
    char *base_init_url;
    char * url_to_dl;
    GF_MPD_Representation *rep;
    /* This variable is 0 if there is a initURL, the index of first segment downloaded otherwise */
    u32 firstSegment = 0;
    if (!mpdin || !period)
        return GF_BAD_PARAM;
    gf_mx_p(mpdin->dl_mutex);
    assert( period->representations );
    rep = gf_list_get(period->representations, mpdin->active_rep_index);
    if (!rep) {
        gf_mx_v(mpdin->dl_mutex);
        GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Unable to find any representation, aborting.\n"));
        return GF_IO_ERR;
    }
    if (!rep->init_url) {
        GF_MPD_SegmentInfo * seg = gf_list_get(rep->segments, 0);
        /* No init URL provided, we have to download the first segment then */
        if (!seg->url) {
            mpdin->dl_stop_request = 1;
            gf_mx_v(mpdin->dl_mutex);
            return GF_BAD_PARAM;
        }
        firstSegment = 1;
        url_to_dl = seg->url;
    } else {
        url_to_dl = rep->init_url;
    }
    if (rep->default_base_url) {
        base_init_url = gf_url_concatenate(rep->default_base_url, url_to_dl);
    } else {
        base_init_url = gf_strdup(url_to_dl);
    }
    e = MPD_downloadWithRetry(mpdin->service, &(mpdin->seg_dnload), base_init_url, MPD_NetIO_Segment, mpdin);
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
        e = MPD_downloadWithRetry(mpdin->service, &(mpdin->seg_dnload), base_init_url, MPD_NetIO_Segment, mpdin);
    } /* end of 404 */

    if (e!= GF_OK && !mpdin->segment_must_be_streamed) {
        mpdin->dl_stop_request = 1;
        gf_mx_v(mpdin->dl_mutex);
        gf_free(base_init_url);
        return e;
    } else {
        const char * mime;
        GF_MPD_Representation *rep = gf_list_get(period->representations, mpdin->active_rep_index);
        u32 count = gf_list_count(rep->segments) + 1;
        if (count < mpdin->max_cached) {
            if (count < 1) {
                GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] 0 representations, aborting\n"));
                gf_free(base_init_url);
                return GF_BAD_PARAM;
            }
            GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[MPD_IN] Resizing to %u max_cached elements instead of %u.\n", count, mpdin->max_cached));
            /* OK, we have a problem, it may ends download */
            mpdin->max_cached = count;
        }
        e = gf_dm_sess_process(mpdin->seg_dnload);
        /* Mime-Type check */
        mime = strlwr(gf_dm_sess_mime_type(mpdin->seg_dnload));
        if (mime && mpdin->seg_ifce == NULL) {
            GF_Err e;
            GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Searching a decoder for mime type : %s...\n", mime));
            gf_free( mpdin->mimeTypeForM3U8Segments);
            mpdin->mimeTypeForM3U8Segments = gf_strdup( mime );
            gf_free( rep->mime);
            rep->mime = gf_strdup( mime );
            e = MPD_LoadMediaService(mpdin, mime);
            if (e != GF_OK)
                return e;
        }
        if (!mime || (stricmp(mime, rep->mime))) {
            GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Mime '%s' is not correct for '%s', it should be '%s'\n", mime, base_init_url, rep->mime));
            mpdin->dl_stop_request = 0;
            gf_mx_v(mpdin->dl_mutex);
            gf_free(base_init_url);
            base_init_url = NULL;
            return GF_BAD_PARAM;
        }
        if (mpdin->segment_must_be_streamed ) {
            mpdin->seg_local_url = gf_dm_sess_get_resource_name(mpdin->seg_dnload);
            e = GF_OK;
        } else {
            mpdin->seg_local_url = rep->init_use_range ? gf_cache_get_cache_filename_range(mpdin->seg_dnload, rep->init_byterange_start, rep->init_byterange_end )  : gf_dm_sess_get_cache_name(mpdin->seg_dnload);
        }

        if ((e!=GF_OK) || !mpdin->seg_local_url) {
            GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error with initialization segment: download result:%s, cache file:%s\n", gf_error_to_string(e), mpdin->seg_local_url));
            mpdin->dl_stop_request = 1;
            gf_mx_v(mpdin->dl_mutex);
            gf_free(base_init_url);
            return GF_BAD_PARAM;
        } else {
#ifndef DONT_USE_TERMINAL_MODULE_API
            GF_NetworkCommand com;
#endif
            assert(!mpdin->nb_cached);
            mpdin->cached[0].cache = gf_strdup(mpdin->seg_local_url);
            mpdin->cached[0].url = gf_strdup(gf_dm_sess_get_resource_name(mpdin->seg_dnload));
            mpdin->nb_cached = 1;
            mpdin->download_segment_index = firstSegment;
            GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Adding initialization segment %s to cache: %s\n", mpdin->seg_local_url, mpdin->cached[0].url ));
            gf_mx_v(mpdin->dl_mutex);
            gf_free(base_init_url);
#ifndef DONT_USE_TERMINAL_MODULE_API
            com.base.command_type = GF_NET_SERVICE_INFO;
            gf_term_on_command(mpdin->service, &com, GF_OK);
#endif
            return GF_OK;
        }
    }
}



static u32 download_segments(void *par)
{
    GF_Err e;
    GF_MPD_In *mpdin = (GF_MPD_In*) par;
    GF_MPD_Period *period;
    GF_MPD_Representation *rep;
    GF_MPD_SegmentInfo *seg;
    char *new_base_seg_url;
    assert(mpdin);
    if (!mpdin->mpd){
      GF_LOG(GF_LOG_WARNING, GF_LOG_MODULE, ("[MPD_IN] Incorrect state, no mpdin->mpd for URL=%s, already stopped ?\n", mpdin->seg_local_url));
      return 1;
    }
    /* Setting the download status in exclusive code */
    gf_mx_p(mpdin->dl_mutex);
    mpdin->is_dl_segments = 1;
    gf_mx_v(mpdin->dl_mutex);

    period = gf_list_get(mpdin->mpd->periods, mpdin->active_period_index);
    e = MPD_DownloadInitSegment(mpdin, period);
    mpdin->dl_stop_request=0;
    if (e != GF_OK) {
        gf_term_on_connect(mpdin->service, NULL, e);
        return 1;
    }
    if (!mpdin->is_service_connected) mpdin->is_service_connected = 1;

    mpdin->last_update_time = gf_sys_clock();
    /* Forward the ConnectService message to the appropriate service for this type of segment */
    GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[MPD_IN] Connecting initial service... %s\n", mpdin->seg_local_url));
    mpdin->seg_ifce->ConnectService(mpdin->seg_ifce, mpdin->service, mpdin->seg_local_url);
    GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[MPD_IN] Connecting initial service DONE\n", mpdin->seg_local_url));
    while (1) {
        /*wait until next segment is needed*/
        gf_mx_p(mpdin->dl_mutex);
        while (!mpdin->dl_stop_request && (mpdin->nb_cached==mpdin->max_cached)) {
            u32 timer = gf_sys_clock() - mpdin->last_update_time;
            Bool shouldParsePlaylist = mpdin->mpd->min_update_time && (timer > mpdin->mpd->min_update_time);
            gf_mx_v(mpdin->dl_mutex);
            if (shouldParsePlaylist) {
                GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Next segment in cache, but it is time to update the playlist (%u ms/%u)\n", timer, mpdin->mpd->min_update_time));
                e = MPD_UpdatePlaylist(mpdin);
                if (e) {
                    GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error updating MDP %s\n", gf_error_to_string(e)));
                }
            } else {
                gf_sleep(16);
            }
            gf_mx_p(mpdin->dl_mutex);
        }

        /* stop the thread if requested */
        if (mpdin->dl_stop_request) {
            mpdin->is_dl_segments = 0;
            gf_mx_v(mpdin->dl_mutex);
            break;
        }
        gf_mx_v(mpdin->dl_mutex);

        /* Continue the processing (no stop request) */
        period = gf_list_get(mpdin->mpd->periods, mpdin->active_period_index);
        rep = gf_list_get(period->representations, mpdin->active_rep_index);

        /* if the index of the segment to be downloaded is greater or equal to the last segment (as seen in the playlist),
           we need to check if a new playlist is ready */
        if (mpdin->download_segment_index>=gf_list_count(rep->segments)) {
            u32 timer = gf_sys_clock() - mpdin->last_update_time;
            /* update of the playlist, only if indicated */
            if (mpdin->mpd->min_update_time && timer > mpdin->mpd->min_update_time) {
                GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Last segment in current playlist downloaded, checking updates after %u ms\n", timer));
                e = MPD_UpdatePlaylist(mpdin);
                if (e) {
                    GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error updating MDP %s\n", gf_error_to_string(e)));
                }
                period = gf_list_get(mpdin->mpd->periods, mpdin->active_period_index);
                rep = gf_list_get(period->representations, mpdin->active_rep_index);
            } else {
                gf_sleep(16);
            }
            /* Now that the playlist is up to date, we can check again */
            if (mpdin->download_segment_index>=gf_list_count(rep->segments)) {
                if (mpdin->mpd->min_update_time) {
                    /* if there is a specified update period, we redo the whole process */
                    continue;
                } else {
                    /* if not, we are really at the end of the playlist, we can quit */
                    GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[MPD_IN] End of playlist reached... downloading remaining elements..."));
                    break;
                }
            }
        }
        gf_mx_p(mpdin->dl_mutex);
        /* At this stage, there are some segments left to be downloaded */
        seg = gf_list_get(rep->segments, mpdin->download_segment_index);
        gf_mx_v(mpdin->dl_mutex);

        if (rep->default_base_url) {
            new_base_seg_url = gf_url_concatenate(rep->default_base_url, seg->url);
        } else {
            new_base_seg_url = gf_strdup(seg->url);
        }
        if (seg->use_byterange) {
            GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Downloading new segment: %s (range: %d-%d)\n", new_base_seg_url, seg->byterange_start, seg->byterange_end));
        }
        e = MPD_downloadWithRetry(mpdin->service, &(mpdin->seg_dnload), new_base_seg_url, MPD_NetIO_Segment, mpdin);
        if (e == GF_OK || mpdin->segment_must_be_streamed) {
            gf_mx_p(mpdin->dl_mutex);
            mpdin->cached[mpdin->nb_cached].cache = gf_strdup(mpdin->segment_must_be_streamed?
                                                    gf_dm_sess_get_resource_name(mpdin->seg_dnload) :
                                                    (rep->init_use_range ? gf_cache_get_cache_filename_range(mpdin->seg_dnload, rep->init_byterange_start, rep->init_byterange_end )  : gf_dm_sess_get_cache_name(mpdin->seg_dnload)));
            mpdin->cached[mpdin->nb_cached].url = gf_strdup( gf_dm_sess_get_resource_name(mpdin->seg_dnload));
            GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[MPD_IN] Added file to cache\n\tURL: %s\n\tCache: %s\n\tElements in cache: %u/%u\n", mpdin->cached[mpdin->nb_cached].url, mpdin->cached[mpdin->nb_cached].cache, mpdin->nb_cached+1, mpdin->max_cached));
            mpdin->nb_cached++;
            gf_mx_v(mpdin->dl_mutex);
            mpdin->download_segment_index++;
            if (mpdin->auto_switch) {
                mpdin->nb_segs_done++;
                if (mpdin->nb_segs_done==1) {
                    mpdin->nb_segs_done=0;
                    mpdin->active_rep_index++;
                    if (mpdin->active_rep_index>=gf_list_count(period->representations)) mpdin->active_rep_index=0;
                }
            }
        }
        if (e != GF_OK) {
            GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error in downloading new segment: %s %s\n", new_base_seg_url, gf_error_to_string(e)));
            gf_free(new_base_seg_url);
            new_base_seg_url = NULL;
            break;
        } else {
            gf_free(new_base_seg_url);
            new_base_seg_url = NULL;
        }
    }

    /* Signal that the download thread has ended */
    gf_mx_p(mpdin->dl_mutex);
    mpdin->is_dl_segments = 0;
    gf_mx_v(mpdin->dl_mutex);
    return 0;
}

const char * MPD_MPD_DESC = "HTTP MPD Streaming";

const char * MPD_MPD_EXT = "3gm mdp";

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

GF_Err MPD_ConnectChannel(GF_InputService *plug, LPNETCHANNEL channel, const char *url, Bool upstream)
{
    GF_MPD_In *mpdin = (GF_MPD_In*) plug->priv;
    GF_Err e;
    GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Channel Connection (0x%x) request from terminal for %s\n", channel, url));
    if (!plug || !plug->priv || !mpdin->seg_ifce) return GF_SERVICE_ERROR;
    e = mpdin->seg_ifce->ConnectChannel(mpdin->seg_ifce, channel, url, upstream);
    if (e == GF_OK) {
        mpdin->nb_connected_channels++;
    }
    return e;
}

GF_Err MPD_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
    GF_MPD_In *mpdin = (GF_MPD_In*) plug->priv;
    GF_Err e;
    GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Disconnect channel (0x%x) request from terminal \n", channel));
    if (!plug || !plug->priv || !mpdin->seg_ifce) return GF_SERVICE_ERROR;
    e = mpdin->seg_ifce->DisconnectChannel(mpdin->seg_ifce, channel);
    if (e == GF_OK) {
        mpdin->nb_connected_channels--;
    }
    return e;
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
    /* stop the download thread */
    gf_mx_p(mpdin->dl_mutex);
    if (mpdin->is_dl_segments == 1) {
        mpdin->dl_stop_request = 1;
        gf_mx_v(mpdin->dl_mutex);
        while (1) {
            /* waiting for the download thread to stop */
            gf_sleep(16);
            gf_mx_p(mpdin->dl_mutex);
            if (mpdin->is_dl_segments == 0) {
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

static GF_Err MPD_SegmentsProcessStart(GF_MPD_In *mpdin, u32 time)
{
    GF_Err e;
    u32 rep_i;
    GF_MPD_Period *period;
    GF_MPD_Representation *rep;

    e = GF_BAD_PARAM;

#if 1
    MPD_DownloadStop(mpdin);
#endif

    /* Get the right period from the given time */
    mpdin->active_period_index = MPD_GetPeriodIndexFromTime(mpdin, time);
    period = gf_list_get(mpdin->mpd->periods, mpdin->active_period_index);
    if (!period || !gf_list_count(period->representations) ) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot start: not enough periods or representations in MPD\n"));
        goto exit;
    }

    /* Select the appropriate representation in the given period */
    mpdin->active_rep_index = 0;
    for (rep_i = 0; rep_i < gf_list_count(period->representations); rep_i++) {
        GF_MPD_Representation *rep = gf_list_get(period->representations, rep_i);
        GF_MPD_Representation *rep_sel = gf_list_get(period->representations, mpdin->active_rep_index);
        if (rep->qualityRanking > rep_sel->qualityRanking) {
            mpdin->active_rep_index = rep_i;
        } else if (rep->bandwidth < rep_sel->bandwidth) {
            mpdin->active_rep_index = rep_i;
        }
    }

    /* TODO: Generate segment names if urltemplates are used */
    rep = gf_list_get(period->representations, mpdin->active_rep_index);
    if (!gf_list_count(rep->segments) || !rep->mime) {
        if (!rep->mime) {
            GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot start: missing mime\n"));
        } else {
            GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot start: missing segments\n"));
        }
        goto exit;
    }

    mpdin->seg_ifce = NULL;
    if (strcmp(M3U8_UNKOWN_MIME_TYPE, rep->mime)) {
        e = MPD_LoadMediaService(mpdin, rep->mime);
        if (e != GF_OK)
            goto exit;
    } else {
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Ignoring mime type %s, wait for first file...\n", rep->mime));
    }
    gf_th_run(mpdin->dl_thread, download_segments, mpdin);
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

    /* If the service is reused on a different URL while already connected this is a problem
       The service should only be reused for audio/video in the same stream
       WARNING: should check fragment identifiers */
    if (mpdin->is_service_connected) {
        if (strcmp(mpdin->url, url)) {
            GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot connect service with different url when already connected: %s / %s \n", mpdin->url, url));
            return GF_BAD_PARAM;
        } else {
            mpdin->nb_service_connections++;
            GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Reusing Service\n"));
            return GF_OK;
        }
    }

    mpdin->nb_service_connections = 1;
    mpdin->service = serv;
    memset( mpdin->lastMPDSignature, 0, sizeof(mpdin->last_update_time));
    mpdin->reload_count = 0;
    if (mpdin->url)
      gf_free(mpdin->url);
    mpdin->url = gf_strdup(url);
    mpdin->option_max_cached = 0;
    mpdin->max_cached = 0;
    mpdin->urlToDeleteNext = NULL;
    opt = gf_modules_get_option((GF_BaseInterface *)plug, "DASH", "MaxCachedSegments");
    if (!opt) gf_modules_set_option((GF_BaseInterface *)plug, "DASH", "MaxCachedSegments", "5");
    if (opt) mpdin->option_max_cached = atoi(opt);
    if (mpdin->option_max_cached < 3)
        mpdin->option_max_cached = 3;
    mpdin->max_cached = mpdin->option_max_cached;
    mpdin->cached = gf_malloc(sizeof(segment_cache_entry)*mpdin->max_cached);
    memset(mpdin->cached, 0, sizeof(segment_cache_entry)*mpdin->max_cached);

    mpdin->auto_switch = 0;
    opt = gf_modules_get_option((GF_BaseInterface *)plug, "DASH", "AutoSwitch");
    if (!opt) gf_modules_set_option((GF_BaseInterface *)plug, "DASH", "AutoSwitch", "no");
    if (opt && !strcmp(opt, "yes")) mpdin->auto_switch = 1;

    if (mpdin->mpd_dnload) gf_term_download_del(mpdin->mpd_dnload);
    mpdin->mpd_dnload = NULL;

    if (!strnicmp(url, "file://", 7)) {
        local_url = url + 7;
        if (strstr(url, ".m3u8")) {
            is_m3u8 = 1;
        }
    } else if (strstr(url, "://")) {
        e = MPD_downloadWithRetry(mpdin->service, &(mpdin->mpd_dnload), url, MPD_NetIO, mpdin);
        if (e!=GF_OK) {
            GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot connect service: MPD downloading problem %s for %s\n", gf_error_to_string(e), url));
            gf_term_on_connect(mpdin->service, NULL, GF_IO_ERR);
            gf_term_download_del(mpdin->mpd_dnload);
            mpdin->mpd_dnload = NULL;
            return e;
        }
        {
            const char * mime = strlwr(gf_dm_sess_mime_type(mpdin->mpd_dnload));
            const char * url = gf_dm_sess_get_resource_name(mpdin->mpd_dnload);
            /* Some servers, for instance http://tv.freebox.fr, serve m3u8 as text/plain */
            if (MPD_isM3U8_mime(mime) || strstr(url, ".m3u8")) {
                is_m3u8 = 1;
            } else if (!MPD_is_MPD_mime(mime)) {
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
        e = gf_mpd_init_from_dom(gf_xml_dom_get_root(mpd_parser), mpdin->mpd);
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
    if (mpdin->seg_ifce) {
        return mpdin->seg_ifce->GetServiceDescriptor(mpdin->seg_ifce, expect_type, sub_url);
    } else {
        GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[MPD_IN] Error - cannot provide service description: no segment service hanlder created\n"));
        return NULL;
    }
}

void MPD_Stop(GF_MPD_In *mpdin)
{
    GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Stopping service 0x%x\n", mpdin->service));
    MPD_DownloadStop(mpdin);
    if (mpdin->urlToDeleteNext) {
        gf_dm_delete_cached_file_entry_session(mpdin->mpd_dnload, mpdin->urlToDeleteNext);
        gf_free( mpdin->urlToDeleteNext);
        mpdin->urlToDeleteNext = NULL;
    }
    if (mpdin->mpd_dnload) {
        gf_term_download_del(mpdin->mpd_dnload);
        mpdin->mpd_dnload = NULL;
    }
    if (mpdin->seg_dnload) {
        gf_term_download_del(mpdin->seg_dnload);
        mpdin->seg_dnload = NULL;
    }
    while (mpdin->nb_cached) {
        mpdin->nb_cached --;
        gf_delete_file(mpdin->cached[mpdin->nb_cached].cache);
        gf_free(mpdin->cached[mpdin->nb_cached].cache);
        gf_free(mpdin->cached[mpdin->nb_cached].url);
    }
    if (mpdin->mpd)
        gf_mpd_del(mpdin->mpd);
    mpdin->mpd = NULL;
}

GF_Err MPD_CloseService(GF_InputService *plug)
{
    GF_MPD_In *mpdin = (GF_MPD_In*) plug->priv;
    GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Close Service (0x%x) request from terminal (#%d connections)\n", mpdin->service, mpdin->nb_service_connections));
    mpdin->nb_service_connections--;
    if (mpdin->nb_service_connections == 0) {
        if (mpdin->seg_ifce && mpdin->is_service_connected) {
            mpdin->seg_ifce->CloseService(mpdin->seg_ifce);
            mpdin->is_service_connected = 0;
            mpdin->seg_ifce = NULL;
        }
        MPD_Stop(mpdin);
        gf_term_on_disconnect(mpdin->service, NULL, GF_OK);
    }
    return GF_OK;
}

GF_Err MPD_ServiceCommand(GF_InputService *plug, GF_NetworkCommand *com)
{
    GF_Err e;
    GF_MPD_In *mpdin = (GF_MPD_In*) plug->priv;
    if (!plug || !plug->priv || !com || !mpdin->seg_ifce) return GF_SERVICE_ERROR;

    switch (com->command_type) {
    case GF_NET_SERVICE_INFO:
    {
        GF_Err subErr;
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Info command from terminal on Service (0x%x)\n", mpdin->service));
        /* defer to the real input service */
        e = mpdin->seg_ifce->ServiceCommand(mpdin->seg_ifce, com);
        if (e!= GF_OK || !com->info.name || 2 > strlen(com->info.name)) {
            char * title = mpdin->mpd->title;
            if (!title)
                title = mpdin->cached[0].url;
            com->info.name = title;
            if (mpdin->mpd->source)
                com->info.comment = mpdin->mpd->source;
        }
        return GF_OK;
    }
    case GF_NET_SERVICE_HAS_AUDIO:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Has Audio command from terminal on Service (0x%x)\n", mpdin->service));
        /* defer to the real input service */
        return mpdin->seg_ifce->ServiceCommand(mpdin->seg_ifce, com);
        break;
    case GF_NET_CHAN_SET_PADDING:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Set Padding command from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        /* for padding settings, the MPD level should not change anything */
        return mpdin->seg_ifce->ServiceCommand(mpdin->seg_ifce, com);
        break;
    case GF_NET_CHAN_SET_PULL:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Set Pull command from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        /* defer to the real input service */
        return mpdin->seg_ifce->ServiceCommand(mpdin->seg_ifce, com);
        break;
    case GF_NET_CHAN_INTERACTIVE:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Interactive command from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        /* defer to the real input service */
        return mpdin->seg_ifce->ServiceCommand(mpdin->seg_ifce, com);
        break;
    case GF_NET_CHAN_BUFFER:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Buffer query command from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        return mpdin->seg_ifce->ServiceCommand(mpdin->seg_ifce, com);
        break;
    case GF_NET_CHAN_BUFFER_QUERY:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received buffer query from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        return mpdin->seg_ifce->ServiceCommand(mpdin->seg_ifce, com);
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
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Play command (#%d playing) from terminal on channel 0x%x on Service (0x%x)\n", mpdin->nb_playing_or_paused_channels, com->base.on_channel, mpdin->service));
        mpdin->playback_speed = com->play.speed;
        mpdin->playback_start_range = com->play.start_range;
        mpdin->playback_end_range = com->play.end_range;
        e = mpdin->seg_ifce->ServiceCommand(mpdin->seg_ifce, com);
        if (e == GF_OK) {
            mpdin->nb_playing_or_paused_channels++;
        }
        return e;
        break;
    case GF_NET_CHAN_STOP:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Stop command (#%d playing) from terminal on channel 0x%x on Service (0x%x)\n", mpdin->nb_playing_or_paused_channels, com->base.on_channel, mpdin->service));
        e = mpdin->seg_ifce->ServiceCommand(mpdin->seg_ifce, com);
        if (e == GF_OK) {
            mpdin->nb_playing_or_paused_channels--;
            if (mpdin->nb_playing_or_paused_channels == 0) {
//                    MPD_Stop(mpdin);
            }
        }
        return e;
        break;
    case GF_NET_CHAN_PAUSE:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Pause command from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        e = mpdin->seg_ifce->ServiceCommand(mpdin->seg_ifce, com);
        return e;
        break;
    case GF_NET_CHAN_RESUME:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Resume command from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        e = mpdin->seg_ifce->ServiceCommand(mpdin->seg_ifce, com);
        return e;
        break;
    case GF_NET_CHAN_SET_SPEED:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Set Speed command from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        /* recording the speed, to change the segment download speed */
        mpdin->playback_speed = com->play.speed;
        /* not supported in MP4 ?? */
        return mpdin->seg_ifce->ServiceCommand(mpdin->seg_ifce, com);
        break;
    case GF_NET_CHAN_CONFIG:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Set Config command from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        /* defer to the real input service */
        return mpdin->seg_ifce->ServiceCommand(mpdin->seg_ifce, com);
        break;
    case GF_NET_CHAN_GET_PIXEL_AR:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Pixel Aspect Ratio query from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        /* defer to the real input service */
        return mpdin->seg_ifce->ServiceCommand(mpdin->seg_ifce, com);
        break;
    case GF_NET_CHAN_GET_DSI:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Decoder Specific Info query from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        return mpdin->seg_ifce->ServiceCommand(mpdin->seg_ifce, com);
        break;
    case GF_NET_CHAN_MAP_TIME:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Map Time query from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        return mpdin->seg_ifce->ServiceCommand(mpdin->seg_ifce, com);
        break;
    case GF_NET_CHAN_RECONFIG:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Reconfig command from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        return mpdin->seg_ifce->ServiceCommand(mpdin->seg_ifce, com);
        break;
    case GF_NET_CHAN_DRM_CFG:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received DRM query from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        return mpdin->seg_ifce->ServiceCommand(mpdin->seg_ifce, com);
        break;
    case GF_NET_CHAN_GET_ESD:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Elementary Stream Descriptor query from terminal on channel 0x%x on Service (0x%x)\n", com->base.on_channel, mpdin->service));
        return mpdin->seg_ifce->ServiceCommand(mpdin->seg_ifce, com);
        break;
    case GF_NET_BUFFER_QUERY:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Global Buffer query from terminal on Service (0x%x)\n", mpdin->service));
        return mpdin->seg_ifce->ServiceCommand(mpdin->seg_ifce, com);
        break;
    case GF_NET_GET_STATS:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Statistics query from terminal on Service (0x%x)\n", mpdin->service));
        return mpdin->seg_ifce->ServiceCommand(mpdin->seg_ifce, com);
        break;
    case GF_NET_IS_CACHABLE:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Cachable query from terminal on Service (0x%x)\n", mpdin->service));
        return mpdin->seg_ifce->ServiceCommand(mpdin->seg_ifce, com);
        break;
    case GF_NET_SERVICE_MIGRATION_INFO:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Migration info query from terminal on Service (0x%x)\n", mpdin->service));
        return mpdin->seg_ifce->ServiceCommand(mpdin->seg_ifce, com);
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
    if (!plug || !plug->priv || !mpdin->seg_ifce) return GF_SERVICE_ERROR;

    gf_mx_p(mpdin->dl_mutex);
    e = mpdin->seg_ifce->ChannelGetSLP(mpdin->seg_ifce, channel, out_data_ptr, out_data_size, out_sl_hdr, sl_compressed, out_reception_status, is_new_data);
    gf_mx_v(mpdin->dl_mutex);
    return e;
}

GF_Err MPD_ChannelReleaseSLP(GF_InputService *plug, LPNETCHANNEL channel)
{
    GF_Err e;
    GF_MPD_In *mpdin = (GF_MPD_In*) plug->priv;
    if (!plug || !plug->priv || !mpdin->seg_ifce) return GF_SERVICE_ERROR;

    gf_mx_p(mpdin->dl_mutex);
    e = mpdin->seg_ifce->ChannelReleaseSLP(mpdin->seg_ifce, channel);
    gf_mx_v(mpdin->dl_mutex);
    return e;
}

Bool MPD_CanHandleURLInService(GF_InputService *plug, const char *url)
{
    GF_MPD_In *mpdin = (GF_MPD_In*) plug->priv;
    GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[MPD_IN] Received Can Handle URL In Service (0x%x) request from terminal for %s\n", mpdin->service, url));
    if (!plug || !plug->priv || !mpdin->seg_ifce) return GF_SERVICE_ERROR;
    if (!strcmp(mpdin->url, url)) {
        return 1;
    } else {
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
    memset(mpdin, 0, sizeof(GF_MPD_In));
    plug->priv = mpdin;
    mpdin->dl_thread = gf_th_new("MPD Segment Downloader Thread");
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
        gf_th_del(mpdin->dl_thread);
        gf_mx_del(mpdin->dl_mutex);
        gf_free(mpdin->mimeTypeForM3U8Segments);
        gf_free(mpdin->cached);
        gf_free(mpdin);
        gf_free(bi);
    }
}
