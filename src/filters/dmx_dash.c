/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017
 *					All rights reserved
 *
 *  This file is part of GPAC / DASH/HLS demux filter
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

#include <gpac/filters.h>
#include <gpac/constants.h>

#ifndef GPAC_DISABLE_DASH_CLIENT

#include <gpac/dash.h>

typedef enum
{
	GF_DASHDMX_BUFFER_NONE=0,
	GF_DASHDMX_BUFFER_MIN,
	GF_DASHDMX_BUFFER_SEGMENTS
} GF_DASHDmxBufferMode;

typedef enum
{
	GF_DASHDMX_LATENCY_NONE=0,
	GF_DASHDMX_LATENCY_CHUNK=1,
	GF_DASHDMX_LATENCY_ALWAYS=2
} GF_DASHDmxLatencyMode;

typedef struct
{
	//opts
	s32 shift_utc, debug_as;
	u32 max_buffer, auto_switch, timeshift, tiles_rate, store, delay40X, exp_threshold, switch_count;
	Bool server_utc, screen_res, aggressive, speed;
	GF_DASHInitialSelectionMode start_with;
	GF_DASHTileAdaptationMode tile_mode;
	GF_DASHAdaptationAlgorithm algo;
	Bool max_res, immediate, abort;
	GF_DASHDmxLatencyMode lowl;
	GF_DASHDmxBufferMode bmode;


	GF_FilterPid *mpd_pid;
	GF_DashClient *dash;
	Bool closed;

	Bool connection_ack_sent;
	u32 nb_playing;

	/*max width & height in all active representations*/
	u32 width, height;

	Double seek_request;
	Double media_start_range;
} GF_DASHDmxCtx;

typedef struct
{
	GF_DASHDmxCtx *dashdmx;
	GF_Filter *seg_filter_src;

	Bool service_connected;
	Bool service_descriptor_fetched;
	Bool netio_assigned;
	Bool has_new_data;
	u32 idx;
	GF_DownloadSession *sess;
	Bool is_timestamp_based, pto_setup;
	u32 timescale;
	s64 pto;
	s64 max_cts_in_period;
	bin128 key_IV;
} GF_MPDGroup;

static s32 gf_dash_get_group_idx_from_filter(GF_DASHDmxCtx *dashdmx, GF_Filter *src_filter)
{
	s32 i;

	for (i=0; (u32) i < gf_dash_get_group_count(dashdmx->dash); i++) {
		GF_MPDGroup *group = gf_dash_get_group_udta(dashdmx->dash, i);
		if (!group) continue;
		if (group->seg_filter_src == src_filter) {
			return i;
		}
	}
	return -1;
}

static void MPD_NotifyData(GF_MPDGroup *group, Bool chunk_flush)
{
#if 0
	GF_NetworkCommand com;
	memset(&com, 0, sizeof(GF_NetworkCommand));
	com.proxy_data.command_type = GF_NET_SERVICE_PROXY_DATA_RECEIVE;
	com.proxy_data.is_chunk = chunk_flush;
	com.proxy_data.is_live = gf_dash_is_dynamic_mpd(group->dashdmx->dash);
	group->seg_filter_src->ServiceCommand(group->seg_filter_src, &com);
#endif

}


#if 0
void dashdmx_data_packet(GF_ClientService *service, LPNETCHANNEL ns, char *data, u32 data_size, GF_SLHeader *hdr, GF_Err reception_status)
{
	s32 i;
	GF_DASHDmxCtx *dashdmx = (GF_DASHDmxCtx*) service->ifce->priv;
	GF_Channel *ch;
	GF_MPDGroup *group;
	Bool do_map_time = GF_FALSE;
	if (!ns || !hdr) {
		dashdmx->fn_data_packet(service, ns, data, data_size, hdr, reception_status);
		return;
	}

	ch = (GF_Channel *) ns;
	assert(ch->odm && ch->odm->ID);

#if FILTER_FIXME
	i = gf_dash_get_group_idx_from_filter(dashdmx,  (GF_InputService *) ch->odm->OD->service_ifce);
#else
	i=-1;
#endif
	if (i<0) {
		dashdmx->fn_data_packet(service, ns, data, data_size, hdr, reception_status);
		return;
	}

	group = gf_dash_get_group_udta(dashdmx->dash, i);

	if (gf_dash_is_m3u8(dashdmx->dash)) {
		dashdmx->fn_data_packet(service, ns, data, data_size, hdr, reception_status);
		if (!group->pto_setup) {
			GF_NetworkCommand com;
			memset(&com, 0, sizeof(com));
			com.command_type = GF_NET_CHAN_SET_MEDIA_TIME;
			com.map_time.media_time = dashdmx->media_start_range;
			com.map_time.timestamp = hdr->compositionTimeStamp;
			com.base.on_channel =  ns;
			gf_service_command(service, &com, GF_OK);
			group->pto_setup = GF_TRUE;
		}
		return;
	}

	//if sync is based on timestamps do not adjust the timestamps back
	if (! group->is_timestamp_based) {
		if (!group->pto_setup) {
			Double scale;
			s64 start, dur;
			u64 pto;
			gf_dash_group_get_presentation_time_offset(dashdmx->dash, i, &pto, &group->timescale);
			group->pto = (s64) pto;
			group->pto_setup = 1;

			if (group->timescale && (group->timescale != ch->esd->slConfig->timestampResolution)) {
				group->pto *= ch->esd->slConfig->timestampResolution;
				group->pto /= group->timescale;
			}
			scale = ch->esd->slConfig->timestampResolution;
			scale /= 1000;

			dur = (u64) (scale * gf_dash_get_period_duration(dashdmx->dash));
			if (dur) {
				group->max_cts_in_period = group->pto + dur;
			} else {
				group->max_cts_in_period = 0;
			}

			start = (u64) (scale * gf_dash_get_period_start(dashdmx->dash));
			group->pto -= start;
		}

		//filter any packet outside the current period
		if (group->max_cts_in_period && (s64) hdr->compositionTimeStamp > group->max_cts_in_period) {
//			GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Packet timestamp "LLU" larger than max CTS in period "LLU" - skipping\n", hdr->compositionTimeStamp, group->max_cts_in_period));
//			return;
		}

		//remap timestamps to our timeline
		if (hdr->decodingTimeStampFlag) {
			if ((s64) hdr->decodingTimeStamp >= group->pto)
				hdr->decodingTimeStamp -= group->pto;
			else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Packet DTS "LLU" less than PTO "LLU" - forcing DTS to 0\n", hdr->decodingTimeStamp, group->pto));
				hdr->decodingTimeStamp = 0;
				hdr->seekFlag = 1;
			}
		}
		if (hdr->compositionTimeStampFlag) {
			if ((s64) hdr->compositionTimeStamp >= group->pto)
				hdr->compositionTimeStamp -= group->pto;
			else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Packet CTS "LLU" less than PTO "LLU" - forcing CTS to 0\n", hdr->compositionTimeStamp, group->pto));
				hdr->compositionTimeStamp = 0;
				hdr->seekFlag = 1;
			}
		}

		if (hdr->OCRflag) {
			u32 scale = hdr->m2ts_pcr ? 300 : 1;
			u64 pto = scale*group->pto;
			if (hdr->objectClockReference >= pto) {
				hdr->objectClockReference -= pto;
			}
			//keep 4 sec between the first received PCR and the first allowed PTS to be used in the period.
			else if (pto - hdr->objectClockReference < 108000000) {
				hdr->objectClockReference = 0;
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASH] Packet OCR/PCR "LLU" less than PTO "LLU" - discarding PCR\n", hdr->objectClockReference/scale, group->pto));
				return;
			}
		}

	} else if (!group->pto_setup) {
		do_map_time = 1;
		group->pto_setup = 1;
	}

	dashdmx->fn_data_packet(service, ns, data, data_size, hdr, reception_status);

	if (do_map_time) {
		GF_NetworkCommand com;
		memset(&com, 0, sizeof(com));
		com.command_type = GF_NET_CHAN_SET_MEDIA_TIME;
		com.map_time.media_time = dashdmx->media_start_range;
		com.map_time.timestamp = hdr->compositionTimeStamp;
		com.base.on_channel =  ns;
		gf_service_command(service, &com, GF_OK);
	}
}


static GF_Err MPD_ClientQuery(GF_InputService *ifce, GF_NetworkCommand *param)
{
	u32 i;
	GF_Err e;
	GF_DASHDmxCtx *dashdmx = (GF_DASHDmxCtx *) ifce->proxy_udta;
	if (!param || !ifce || !ifce->proxy_udta) return GF_BAD_PARAM;

	/*gets byte range of init segment (for local validation)*/
	if (param->command_type==GF_NET_SERVICE_QUERY_INIT_RANGE) {
		param->url_query.next_url = NULL;
		param->url_query.start_range = 0;
		param->url_query.end_range = 0;

		for (i=0; i<gf_dash_get_group_count(dashdmx->dash); i++) {
			GF_MPDGroup *group;
			if (!gf_dash_is_group_selectable(dashdmx->dash, i)) continue;
			group = gf_dash_get_group_udta(dashdmx->dash, i);
			if (!group) continue;

			if (group->seg_filter_src == ifce) {
				gf_dash_group_get_segment_init_url(dashdmx->dash, i, &param->url_query.start_range, &param->url_query.end_range);
				param->url_query.current_download = 0;

				param->url_query.key_url = gf_dash_group_get_segment_init_keys(dashdmx->dash, i, &group->key_IV);
				if (param->url_query.key_url) {
					param->url_query.key_IV = &group->key_IV;
				}
				return GF_OK;
			}
		}
		return GF_SERVICE_ERROR;
	}

	/*gets URL and byte range of next segment - if needed, adds bitstream switching segment info*/
	if (param->command_type==GF_NET_SERVICE_QUERY_NEXT) {
		Bool group_done;
		u32 nb_segments_cached;
		u32 group_idx=0;
		GF_MPDGroup *group=NULL;
		const char *src_url;
		Bool discard_first_cache_entry = param->url_query.drop_first_segment;
		Bool check_current_download = param->url_query.current_download;
		u32 timer = gf_sys_clock();
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASHDmx] Received Service Query Next request from input service %s\n", ifce->module_name));


		param->url_query.current_download = 0;
		param->url_query.discontinuity_type = 0;

		for (i=0; i<gf_dash_get_group_count(dashdmx->dash); i++) {
			if (!gf_dash_is_group_selected(dashdmx->dash, i)) continue;
			group = gf_dash_get_group_udta(dashdmx->dash, i);
			if (group->seg_filter_src == ifce) {
				group_idx = i;
				break;
			}
			group=NULL;
		}

		if (!group) {
			return GF_SERVICE_ERROR;
		}

		//update group idx
		if (group->idx != group_idx) {
			group->idx = group_idx;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASHDmx] New AdaptationSet detected after MPD update ?\n"));
		}

		if (discard_first_cache_entry) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASHDmx] Discarding first segment in cache\n"));
			gf_dash_group_discard_segment(dashdmx->dash, group_idx);
		}

		while (gf_dash_is_running(dashdmx->dash)) {
			group_done=0;
			nb_segments_cached = gf_dash_group_get_num_segments_ready(dashdmx->dash, group_idx, &group_done);
			if (nb_segments_cached>=1)
				break;

			if (group_done) {
				if (!gf_dash_get_period_switch_status(dashdmx->dash) && !gf_dash_in_last_period(dashdmx->dash, GF_TRUE)) {
					GF_NetworkCommand com;
					param->url_query.in_end_of_period = 1;
					memset(&com, 0, sizeof(GF_NetworkCommand));
					com.command_type = GF_NET_BUFFER_QUERY;
					if (gf_dash_get_period_switch_status(dashdmx->dash) != 1) {
						gf_service_command(dashdmx->service, &com, GF_OK);
						//we only switch period once no more data is in our buffers
						if (!com.buffer.occupancy) {
							param->url_query.in_end_of_period = 0;
							gf_dash_request_period_switch(dashdmx->dash);
						}
					}
					if (param->url_query.in_end_of_period)
						return GF_BUFFER_TOO_SMALL;
				} else {
					return GF_EOS;
				}
			}

			if (check_current_download && dashdmx->low_latency_mode) {
				Bool is_switched=GF_FALSE;
				gf_dash_group_probe_current_download_segment_location(dashdmx->dash, group_idx, &param->url_query.next_url, NULL, &param->url_query.next_url_init_or_switch_segment, &src_url, &is_switched);

				if (param->url_query.next_url) {
					param->url_query.current_download = 1;
					param->url_query.has_new_data = group->has_new_data;
					param->url_query.discontinuity_type = is_switched ? 1 : 0;
					if (gf_dash_group_loop_detected(dashdmx->dash, group_idx))
						param->url_query.discontinuity_type = 2;
					group->has_new_data = 0;
					return GF_OK;
				}
				return GF_BUFFER_TOO_SMALL;
			}
			return GF_BUFFER_TOO_SMALL;
		}

		param->url_query.current_download = 0;
		nb_segments_cached = gf_dash_group_get_num_segments_ready(dashdmx->dash, group_idx, &group_done);
		if (nb_segments_cached < 1) {
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[DASHDmx] No more file in cache, EOS\n"));
			return GF_EOS;
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASHDmx] Had to wait for %u ms for the only cache file to be downloaded\n", (gf_sys_clock() - timer)));
		}

		e = gf_dash_group_get_next_segment_location(dashdmx->dash, group_idx, param->url_query.dependent_representation_index, &param->url_query.next_url, &param->url_query.start_range, &param->url_query.end_range,
		        NULL, &param->url_query.next_url_init_or_switch_segment, &param->url_query.switch_start_range , &param->url_query.switch_end_range,
		        &src_url, &param->url_query.has_next, &param->url_query.key_url, &group->key_IV);
		if (e)
			return e;

		param->url_query.key_IV = &group->key_IV;

		if (gf_dash_group_loop_detected(dashdmx->dash, group_idx))
			param->url_query.discontinuity_type = 2;


#ifndef GPAC_DISABLE_LOG
		{
			u32 timer2 = gf_sys_clock() - timer;
			if (timer2 > 1000) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASHDmx] Waiting for download to end took a long time : %u ms\n", timer2));
			}
			if (param->url_query.end_range) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASHDmx] Next Segment is %s bytes "LLD"-"LLD"\n", src_url, param->url_query.start_range, param->url_query.end_range));
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASHDmx] Next Segment is %s\n", src_url));
			}
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASHDmx] Waited %d ms - Elements in cache: %u/%u\n\tCache file name %s\n\tsegment start time %g sec\n", timer2, gf_dash_group_get_num_segments_ready(dashdmx->dash, group_idx, &group_done), gf_dash_group_get_max_segments_in_cache(dashdmx->dash, group_idx), param->url_query.next_url, gf_dash_group_current_segment_start_time(dashdmx->dash, group_idx)));
		}
#endif
	}

	if (param->command_type == GF_NET_SERVICE_QUERY_UTC_DELAY) {
		param->utc_delay.delay = gf_dash_get_utc_drift_estimate(dashdmx->dash);
		return GF_OK;
	}

	return GF_OK;
}

/*locates input service (demuxer) based on mime type or segment name*/
static GF_Err MPD_LoadMediaService(GF_DASHDmxCtx *dashdmx, u32 group_index, const char *mime, const char *init_segment_name)
{
	GF_InputService *seg_filter_src;
	u32 i;
	const char *sPlug;
	if (mime) {
		/* Check MIME type to start the right InputService */
		sPlug = gf_cfg_get_key(dashdmx->service->term->user->config, "MimeTypes", mime);
		if (sPlug) sPlug = strrchr(sPlug, '"');
		if (sPlug) {
			sPlug += 2;
			seg_filter_src = (GF_InputService *) gf_modules_load_interface_by_name(dashdmx->service->term->user->modules, sPlug, GF_NET_CLIENT_INTERFACE);
			if (seg_filter_src) {
				GF_MPDGroup *group;
				GF_SAFEALLOC(group, GF_MPDGroup);
				if (!group) return GF_OUT_OF_MEM;
				
				group->seg_filter_src = seg_filter_src;
				group->seg_filter_src->proxy_udta = dashdmx;
				group->seg_filter_src->query_proxy = MPD_ClientQuery;
				group->dashdmx = dashdmx;
				group->idx = group_index;
				gf_dash_set_group_udta(dashdmx->dash, group_index, group);
				return GF_OK;
			}
		}
	}
	if (init_segment_name) {
		for (i=0; i< gf_modules_get_count(dashdmx->service->term->user->modules); i++) {
			GF_InputService *ifce = (GF_InputService *) gf_modules_load_interface(dashdmx->service->term->user->modules, i, GF_NET_CLIENT_INTERFACE);
			if (!ifce) continue;

			if (ifce->CanHandleURL && ifce->CanHandleURL(ifce, init_segment_name)) {
				GF_MPDGroup *group;
				GF_SAFEALLOC(group, GF_MPDGroup);
				if (!group) return GF_OUT_OF_MEM;
				group->seg_filter_src = ifce;
				group->seg_filter_src->proxy_udta = dashdmx;
				group->seg_filter_src->query_proxy = MPD_ClientQuery;
				group->dashdmx = dashdmx;
				group->idx = group_index;
				gf_dash_set_group_udta(dashdmx->dash, group_index, group);
				return GF_OK;
			}
			gf_modules_close_interface((GF_BaseInterface *) ifce);
		}
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASHDmx] Error locating plugin for segment - mime type %s - name %s\n", mime, init_segment_name));
	return GF_CODEC_NOT_FOUND;
}


GF_InputService *MPD_GetInputServiceForChannel(GF_DASHDmxCtx *dashdmx, LPNETCHANNEL channel)
{
	GF_Channel *ch;
	if (!channel) {
		u32 i;
		for (i=0; i<gf_dash_get_group_count(dashdmx->dash); i++) {
			if (gf_dash_is_group_selectable(dashdmx->dash, i)) {
				GF_MPDGroup *mudta = gf_dash_get_group_udta(dashdmx->dash, i);
				if (mudta && mudta->seg_filter_src) return mudta->seg_filter_src;
			}
		}
		return NULL;
	}

	ch = (GF_Channel *) channel;
	assert(ch->odm && ch->odm->ID);
#if FILTER_FIXME
	return (GF_InputService *) ch->odm->OD->service_ifce;
#else
	return NULL;
#endif
}

s32 MPD_GetGroupIndexForChannel(GF_DASHDmxCtx *dashdmx, LPNETCHANNEL channel)
{
	u32 i;
	GF_InputService *ifce = MPD_GetInputServiceForChannel(dashdmx, channel);
	if (!ifce) return -1;

	for (i=0; i<gf_dash_get_group_count(dashdmx->dash); i++) {
		GF_MPDGroup *group = gf_dash_get_group_udta(dashdmx->dash, i);
		if (!group) continue;
		if (group->seg_filter_src == ifce) return i;
	}
	return -1;
}

#endif

static void dashdmx_dash_segment_netio(void *cbk, GF_NETIO_Parameter *param)
{
	GF_MPDGroup *group = (GF_MPDGroup *)cbk;
	u32 bytes_per_sec;

	if (param->msg_type == GF_NETIO_PARSE_HEADER) {
		if (!strcmp(param->name, "Dash-Newest-Segment")) {
			gf_dash_resync_to_segment(group->dashdmx->dash, param->value, gf_dm_sess_get_header(param->sess, "Dash-Oldest-Segment"));
		}
	}

	if (param->msg_type == GF_NETIO_DATA_EXCHANGE) {
		group->has_new_data = 1;

		if (param->reply) {
			//u32 bytes_per_sec;
			const char *url;
			gf_dm_sess_get_stats(group->sess, NULL, &url, NULL, NULL, &bytes_per_sec, NULL);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASHDmx] End of chunk received for %s at UTC "LLU" ms - estimated bandwidth %d kbps - chunk start at UTC "LLU"\n", url, gf_net_get_utc(), 8*bytes_per_sec/1000, gf_dm_sess_get_utc_start(group->sess)));
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("DEBUG. 2. redowload at max  %d \n", 8*bytes_per_sec/1000));

			if (group->dashdmx->lowl)
				MPD_NotifyData(group, 1);
		} else if (group->dashdmx->lowl==GF_DASHDMX_LATENCY_ALWAYS) {
			MPD_NotifyData(group, 1);
		}

		if (group->dashdmx->abort)
			gf_dash_group_check_bandwidth(group->dashdmx->dash, group->idx);
	}
	if (param->msg_type == GF_NETIO_DATA_TRANSFERED) {
		u32 bytes_per_sec;
		const char *url;
		gf_dm_sess_get_stats(group->sess, NULL, &url, NULL, NULL, &bytes_per_sec, NULL);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASHDmx] End of file %s download at UTC "LLU" ms - estimated bandwidth %d kbps - started file or last chunk at UTC "LLU"\n", url, gf_net_get_utc(), 8*bytes_per_sec/1000, gf_dm_sess_get_utc_start(group->sess)));
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("DEBUG1. %d \n", 8*bytes_per_sec/1000));
	}
}

#if 0

void dashdmx_dash_io_delete_cache_file(GF_DASHFileIO *dashio, GF_DASHFileIOSession session, const char *cache_url)
{
	gf_dm_delete_cached_file_entry_session((GF_DownloadSession *)session, cache_url);
}

GF_DASHFileIOSession dashdmx_dash_io_create(GF_DASHFileIO *dashio, Bool persistent, const char *url, s32 group_idx)
{
	GF_MPDGroup *group = NULL;
	GF_DownloadSession *sess;
	u32 flags = GF_NETIO_SESSION_NOT_THREADED;
	GF_DASHDmxCtx *dashdmx = (GF_DASHDmxCtx *)dashio->udta;
	if (dashdmx->memory_storage)
		flags |= GF_NETIO_SESSION_MEMORY_CACHE;

	if (persistent) flags |= GF_NETIO_SESSION_PERSISTENT;

	if (group_idx>=0) {
		group = gf_dash_get_group_udta(dashdmx->dash, group_idx);
	}
	if (group) {
		group->netio_assigned = GF_TRUE;
		group->sess = sess = gf_service_download_new(dashdmx->service, url, flags, dashdmx_dash_segment_netio, group);
	} else {
		sess = gf_service_download_new(dashdmx->service, url, flags, NULL, NULL);
	}
	return (GF_DASHFileIOSession) sess;
}
void dashdmx_dash_io_del(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	gf_service_download_del((GF_DownloadSession *)session);
}
void dashdmx_dash_io_abort(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	gf_dm_sess_abort((GF_DownloadSession *)session);
}
GF_Err dashdmx_dash_io_setup_from_url(GF_DASHFileIO *dashio, GF_DASHFileIOSession session, const char *url, s32 group_idx)
{
	if (group_idx>=0) {
		GF_DASHDmxCtx *dashdmx = (GF_DASHDmxCtx *)dashio->udta;
		GF_MPDGroup *group = gf_dash_get_group_udta(dashdmx->dash, group_idx);
		if (group && !group->netio_assigned) {
			group->netio_assigned = GF_TRUE;
			group->sess = (GF_DownloadSession *)session;
			gf_dm_sess_reassign((GF_DownloadSession *)session, 0xFFFFFFFF, dashdmx_dash_segment_netio, group);
		}
	}
	return gf_dm_sess_setup_from_url((GF_DownloadSession *)session, url);
}
GF_Err dashdmx_dash_io_set_range(GF_DASHFileIO *dashio, GF_DASHFileIOSession session, u64 start_range, u64 end_range, Bool discontinue_cache)
{
	return gf_dm_sess_set_range((GF_DownloadSession *)session, start_range, end_range, discontinue_cache);
}
GF_Err dashdmx_dash_io_init(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	return gf_dm_sess_process_headers((GF_DownloadSession *)session);
}
GF_Err dashdmx_dash_io_run(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	return gf_dm_sess_process((GF_DownloadSession *)session);
}
const char *dashdmx_dash_io_get_url(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	return gf_dm_sess_get_resource_name((GF_DownloadSession *)session);
}
const char *dashdmx_dash_io_get_cache_name(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	return gf_dm_sess_get_cache_name((GF_DownloadSession *)session);
}
const char *dashdmx_dash_io_get_mime(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	return gf_dm_sess_mime_type((GF_DownloadSession *)session);
}

const char *dashdmx_dash_io_get_header_value(GF_DASHFileIO *dashio, GF_DASHFileIOSession session, const char *header_name)
{
	return gf_dm_sess_get_header((GF_DownloadSession *)session, header_name);
}

u64 dashdmx_dash_io_get_utc_start_time(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	return gf_dm_sess_get_utc_start((GF_DownloadSession *)session);
}



u32 dashdmx_dash_io_get_bytes_per_sec(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	u32 bps=0;
//	GF_DownloadSession *sess = (GF_DownloadSession *)session;
	if (session) {
		gf_dm_sess_get_stats((GF_DownloadSession *)session, NULL, NULL, NULL, NULL, &bps, NULL);
	} else {
		GF_DASHDmxCtx *dashdmx = (GF_DASHDmxCtx *)dashio->udta;
		bps = gf_dm_get_data_rate(dashdmx->service->term->downloader);
		bps/=8;
	}
	//GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("DEBUG. 2. max max max  %d \n", bps*8));
	return bps;
}
u32 dashdmx_dash_io_get_total_size(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	u32 size=0;
//	GF_DownloadSession *sess = (GF_DownloadSession *)session;
	gf_dm_sess_get_stats((GF_DownloadSession *)session, NULL, NULL, &size, NULL, NULL, NULL);
	return size;
}
u32 dashdmx_dash_io_get_bytes_done(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	u32 size=0;
//	GF_DownloadSession *sess = (GF_DownloadSession *)session;
	gf_dm_sess_get_stats((GF_DownloadSession *)session, NULL, NULL, NULL, &size, NULL, NULL);
	return size;
}

#endif


GF_Err dashdmx_dash_io_on_dash_event(GF_DASHFileIO *dashio, GF_DASHEventType dash_evt, s32 group_idx, GF_Err error_code)
{
	GF_Err e;
	u32 i;
	GF_DASHDmxCtx *dashdmx = (GF_DASHDmxCtx *)dashio->udta;

	if (dash_evt==GF_DASH_EVENT_PERIOD_SETUP_ERROR) {
#ifdef FILTER_FIXME
		if (!dashdmx->connection_ack_sent) {
			dashdmx->fn_connect_ack(dashdmx->service, NULL, error_code);
			dashdmx->connection_ack_sent= GF_TRUE;
		}
#endif
		return GF_OK;
	}

	if (dash_evt==GF_DASH_EVENT_SELECT_GROUPS) {
		//configure buffer in dynamic mode without low latency: we indicate how much the player will buffer
		if (gf_dash_is_dynamic_mpd(dashdmx->dash) && (dashdmx->lowl==GF_DASHDMX_LATENCY_NONE) ) {
			u32 buffer_ms = 0;
#ifdef FILTER_FIXME
			const char *opt = gf_modules_get_option((GF_BaseInterface *)dashdmx->plug, "Network", "BufferLength");
			if (opt) buffer_ms = atoi(opt);
#endif
			//use min buffer from MPD
			if (dashdmx->bmode>=GF_DASHDMX_BUFFER_MIN) {
				u32 mpd_buffer_ms = gf_dash_get_min_buffer_time(dashdmx->dash);
				if (mpd_buffer_ms > buffer_ms)
					buffer_ms = mpd_buffer_ms;
			}

			if (buffer_ms) {
				gf_dash_set_user_buffer(dashdmx->dash, buffer_ms);
			}
		}
		//let the player decide which group to play: we declare everything
		return GF_OK;
	}

	/*for all selected groups, create input service and connect to init/first segment*/
	if (dash_evt==GF_DASH_EVENT_CREATE_PLAYBACK) {
		/*select input services if possible*/
		for (i=0; i<gf_dash_get_group_count(dashdmx->dash); i++) {
			const char *mime, *init_segment;
			u32 j;
			Bool playable = GF_TRUE;
			//let the player decide which group to play
			if (!gf_dash_is_group_selectable(dashdmx->dash, i))
				continue;

			j=0;
			while (1) {
				const char *desc_id, *desc_scheme, *desc_value;
				if (! gf_dash_group_enum_descriptor(dashdmx->dash, i, GF_MPD_DESC_ESSENTIAL_PROPERTIES, j, &desc_id, &desc_scheme, &desc_value))
					break;
				j++;
				if (!strcmp(desc_scheme, "urn:mpeg:dash:srd:2014")) {
				} else {
					playable = GF_FALSE;
					break;
				}
			}
			if (!playable) {
				gf_dash_group_select(dashdmx->dash, i, GF_FALSE);
				continue;
			}

			if (gf_dash_group_has_dependent_group(dashdmx->dash, i) >=0 ) {
				gf_dash_group_select(dashdmx->dash, i, GF_TRUE);
				continue;
			}

			mime = gf_dash_group_get_segment_mime(dashdmx->dash, i);
			init_segment = gf_dash_group_get_segment_init_url(dashdmx->dash, i, NULL, NULL);
#ifdef FILTER_FIXME
			e = MPD_LoadMediaService(dashdmx, i, mime, init_segment);
#endif
			e = GF_OK;
			if (e != GF_OK) {
				gf_dash_group_select(dashdmx->dash, i, GF_FALSE);
			} else {
				u32 w, h;
				/*connect our media service*/
				GF_MPDGroup *group = gf_dash_get_group_udta(dashdmx->dash, i);
				gf_dash_group_get_video_info(dashdmx->dash, i, &w, &h);
				if (w && h && w>dashdmx->width && h>dashdmx->height) {
					dashdmx->width = w;
					dashdmx->height = h;
				}
				if (gf_dash_group_get_srd_max_size_info(dashdmx->dash, i, &w, &h)) {
					dashdmx->width = w;
					dashdmx->height = h;
				}

#ifdef FILTER_FIXME
				e = group->seg_filter_src->ConnectService(group->seg_filter_src, dashdmx->service, init_segment);
#endif
				if (e) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[DASHDmx] Unable to connect input service to %s\n", init_segment));
					gf_dash_group_select(dashdmx->dash, i, GF_FALSE);
				} else {
					group->service_connected = 1;
				}
				if (dashdmx->closed) return GF_OK;
			}
		}

		if (!dashdmx->connection_ack_sent) {
#ifdef FILTER_FIXME
			dashdmx->fn_connect_ack(dashdmx->service, NULL, GF_OK);
#endif
			dashdmx->connection_ack_sent=1;
		}

		//we had a seek outside of the period we were setting up, during period setup !
		//request the seek again from the player
		if (dashdmx->seek_request>=0) {
#ifdef FILTER_FIXME
			GF_NetworkCommand com;
			memset(&com, 0, sizeof(GF_NetworkCommand));
			com.command_type = GF_NET_SERVICE_SEEK;
			com.play.start_range = dashdmx->seek_request;
			dashdmx->seek_request = 0;
			gf_service_command(dashdmx->service, &com, GF_OK);
#endif

		}
		return GF_OK;
	}

	/*for all running services, stop service*/
	if (dash_evt==GF_DASH_EVENT_DESTROY_PLAYBACK) {

#ifdef FILTER_FIXME
		dashdmx->service->subservice_disconnect = 1;
		gf_service_disconnect_ack(dashdmx->service, NULL, GF_OK);
		dashdmx->service->subservice_disconnect = 2;
#endif
		for (i=0; i<gf_dash_get_group_count(dashdmx->dash); i++) {
			GF_MPDGroup *group = gf_dash_get_group_udta(dashdmx->dash, i);
			if (!group) continue;
			if (group->seg_filter_src) {
				if (group->service_connected) {
#ifdef FILTER_FIXME
					group->seg_filter_src->CloseService(group->seg_filter_src);
#endif
					group->service_connected = 0;
				}
				gf_modules_close_interface((GF_BaseInterface *) group->seg_filter_src);
			}
			gf_free(group);
			gf_dash_set_group_udta(dashdmx->dash, i, NULL);
		}
#ifdef FILTER_FIXME
		dashdmx->service->subservice_disconnect = 0;
#endif
		return GF_OK;
	}

	if (dash_evt==GF_DASH_EVENT_BUFFERING) {
		u32 tot, done;
		gf_dash_get_buffer_info(dashdmx->dash, &tot, &done);
		return GF_OK;
	}
	if (dash_evt==GF_DASH_EVENT_SEGMENT_AVAILABLE) {
		if (group_idx>=0) {
			GF_MPDGroup *group = gf_dash_get_group_udta(dashdmx->dash, group_idx);
			if (group) MPD_NotifyData(group, 0);
		}
		return GF_OK;
	}
	if (dash_evt==GF_DASH_EVENT_QUALITY_SWITCH) {
		if (group_idx>=0) {
			GF_MPDGroup *group = gf_dash_get_group_udta(dashdmx->dash, group_idx);
			if (!group) {
				group_idx = gf_dash_group_has_dependent_group(dashdmx->dash, group_idx);
				group = gf_dash_get_group_udta(dashdmx->dash, group_idx);
			}
			if (group) {
#ifdef FILTER_FIXME
				GF_NetworkCommand com;
				memset(&com, 0, sizeof(GF_NetworkCommand));

				com.command_type = GF_NET_SERVICE_EVENT;
				com.send_event.evt.type = GF_EVENT_QUALITY_SWITCHED;
				gf_service_command(dashdmx->service, &com, GF_OK);
#endif

			}
		}
		return GF_OK;
	}
#ifdef FILTER_FIXME
	if (dash_evt==GF_DASH_EVENT_TIMESHIFT_OVERFLOW) {
		GF_NetworkCommand com;
		com.command_type = GF_NET_SERVICE_EVENT;
		com.send_event.evt.type = (group_idx>=0) ? GF_EVENT_TIMESHIFT_OVERFLOW : GF_EVENT_TIMESHIFT_UNDERRUN;
		gf_service_command(dashdmx->service, &com, GF_OK);
	}
	if (dash_evt==GF_DASH_EVENT_TIMESHIFT_UPDATE) {
		GF_NetworkCommand com;
		com.command_type = GF_NET_SERVICE_EVENT;
		com.send_event.evt.type = GF_EVENT_TIMESHIFT_UPDATE;
		gf_service_command(dashdmx->service, &com, GF_OK);
	}

	if (dash_evt==GF_DASH_EVENT_CODEC_STAT_QUERY) {
		GF_NetworkCommand com;
		memset(&com, 0, sizeof(GF_NetworkCommand));
		com.command_type = GF_NET_SERVICE_CODEC_STAT_QUERY;
		gf_service_command(dashdmx->service, &com, GF_OK);
		gf_dash_group_set_codec_stat(dashdmx->dash, group_idx, com.codec_stat.avg_dec_time, com.codec_stat.max_dec_time, com.codec_stat.irap_avg_dec_time, com.codec_stat.irap_max_dec_time, com.codec_stat.codec_reset, com.codec_stat.decode_only_rap);

		memset(&com, 0, sizeof(GF_NetworkCommand));
		com.command_type = GF_NET_BUFFER_QUERY;
		gf_service_command(dashdmx->service, &com, GF_OK);
		gf_dash_group_set_buffer_levels(dashdmx->dash, group_idx, com.buffer.min, com.buffer.max, com.buffer.occupancy);
	}
#endif
	return GF_OK;
}

/*check in all groups if the service can support reverse playback (speed<0); return GF_OK only if service is supported in all groups*/
static GF_Err dashdmx_dash_can_reverse_playback(GF_DASHDmxCtx *dashdmx)
{
	u32 i;
	GF_Err e = GF_NOT_SUPPORTED;
	for (i=0; i<gf_dash_get_group_count(dashdmx->dash); i++) {
		if (gf_dash_is_group_selectable(dashdmx->dash, i)) {
			GF_MPDGroup *mudta = gf_dash_get_group_udta(dashdmx->dash, i);
			if (mudta && mudta->seg_filter_src) {
#ifdef FILTER_FIXME
				GF_NetworkCommand com;
				com.command_type = GF_NET_SERVICE_CAN_REVERSE_PLAYBACK;
				e = mudta->seg_filter_src->ServiceCommand(mudta->seg_filter_src, &com);
				if (GF_OK != e)
					return e;
#endif

			}
		}
	}

	return e;
}

static GF_Err dashdmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_Err e=GF_OK;
	const GF_PropertyValue *p;
	GF_DASHDmxCtx *dashdmx = (GF_DASHDmxCtx*) gf_filter_get_udta(filter);

	if (is_remove) {
		//TODO
		if (pid==dashdmx->mpd_pid) {
			dashdmx->mpd_pid = NULL;
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	dashdmx->mpd_pid = pid;

	if (e) {
		gf_filter_setup_failure(filter, e);
		return e;
	}

	dashdmx->seek_request = -1;

	dashdmx->nb_playing = 0;

#if 0
	/*dash thread starts at the end of gf_dash_open */
	e = gf_dash_open(dashdmx->dash, url);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASHDmx] Error - cannot initialize DASH Client for %s: %s\n", url, gf_error_to_string(e)));
		dashdmx->fn_connect_ack(dashdmx->service, NULL, e);
		return GF_OK;
	}
#endif

	return GF_OK;
}

static GF_Err dashdmx_initialize(GF_Filter *filter)
{
	GF_DASHDmxCtx *dashdmx = (GF_DASHDmxCtx*) gf_filter_get_udta(filter);

	dashdmx->dash = gf_dash_new(NULL, dashdmx->max_buffer, dashdmx->auto_switch, (dashdmx->store==2) ? GF_TRUE : GF_FALSE, (dashdmx->algo==GF_DASH_ALGO_NONE) ? GF_TRUE : GF_FALSE, dashdmx->start_with, (dashdmx->bmode == GF_DASHDMX_BUFFER_SEGMENTS) ? 1 : 0, dashdmx->timeshift);
	gf_dash_set_algo(dashdmx->dash, dashdmx->algo);

	if (!dashdmx->dash) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[DASHDmx] Error - cannot create DASH Client\n"));
		return GF_IO_ERR;
	}

	gf_dash_set_utc_shift(dashdmx->dash, dashdmx->shift_utc);
	gf_dash_enable_utc_drift_compensation(dashdmx->dash, dashdmx->server_utc);
	gf_dash_set_tile_adaptation_mode(dashdmx->dash, dashdmx->tile_mode, dashdmx->tiles_rate);
	gf_dash_set_threaded_download(dashdmx->dash, GF_FALSE);

	if (dashdmx->screen_res) {
#if FILTER_FIXME
		GF_NetworkCommand com;
		memset(&com, 0, sizeof(GF_NetworkCommand));
		com.base.command_type = GF_NET_SERVICE_MEDIA_CAP_QUERY;
		gf_service_command(serv, &com, GF_OK);

		if (com.mcaps.width && com.mcaps.height) {
			gf_dash_set_max_resolution(dashdmx->dash, com.mcaps.width, com.mcaps.height, com.mcaps.display_bit_depth);
		}
#endif

	}

	gf_dash_set_min_timeout_between_404(dashdmx->dash, dashdmx->delay40X);
	gf_dash_set_segment_expiration_threshold(dashdmx->dash, dashdmx->exp_threshold);
	gf_dash_set_switching_probe_count(dashdmx->dash, dashdmx->switch_count);
	gf_dash_set_agressive_adaptation(dashdmx->dash, dashdmx->aggressive);
	gf_dash_debug_group(dashdmx->dash, dashdmx->debug_as);
	gf_dash_disable_speed_adaptation(dashdmx->dash, dashdmx->speed);

	return GF_OK;
}


static void dashdmx_finalize(GF_Filter *filter)
{
	GF_DASHDmxCtx *dashdmx = (GF_DASHDmxCtx*) gf_filter_get_udta(filter);
	assert(dashdmx);

	if (dashdmx->dash)
		gf_dash_close(dashdmx->dash);
}

static Bool dashdmx_process_event(GF_Filter *filter, const GF_FilterEvent *fevt)
{
#if FILTER_FIXME
	s32 idx;
	GF_Err e;
	GF_DASHDmxCtx *dashdmx = (GF_DASHDmxCtx*) gf_filter_get_udta(filter);
	GF_InputService *seg_filter_src = NULL;

	if (!plug || !plug->priv || !com) return GF_SERVICE_ERROR;

	seg_filter_src = MPD_GetInputServiceForChannel(dashdmx, com->base.on_channel);

	switch (com->command_type) {
	case GF_NET_SERVICE_INFO:
	{
		s32 idx;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASHDmx] Received Info command from terminal on Service (%p)\n", dashdmx->service));

		e = GF_OK;
		if (seg_filter_src) {
			/* defer to the real input service */
			e = seg_filter_src->ServiceCommand(seg_filter_src, com);
		}

		if (e!= GF_OK || !com->info.name || 2 > strlen(com->info.name)) {
			gf_dash_get_info(dashdmx->dash, &com->info.name, &com->info.comment);
		}
		idx = MPD_GetGroupIndexForChannel(dashdmx, com->play.on_channel);
		if (idx>=0) {
			//gather role and co
			if (!com->info.role) {
				gf_dash_group_enum_descriptor(dashdmx->dash, idx, GF_MPD_DESC_ROLE, 0, NULL, NULL, &com->info.role);
			}
			if (!com->info.accessibility) {
				gf_dash_group_enum_descriptor(dashdmx->dash, idx, GF_MPD_DESC_ACCESSIBILITY, 0, NULL, NULL, &com->info.accessibility);
			}
			if (!com->info.rating) {
				gf_dash_group_enum_descriptor(dashdmx->dash, idx, GF_MPD_DESC_RATING, 0, NULL, NULL, &com->info.rating);
			}
		}
		return GF_OK;
	}
	/*we could get it from MPD*/
	case GF_NET_SERVICE_HAS_AUDIO:
		if (seg_filter_src) {
			/* defer to the real input service */
			return seg_filter_src->ServiceCommand(seg_filter_src, com);
		}
		return GF_NOT_SUPPORTED;

	case GF_NET_SERVICE_HAS_FORCED_VIDEO_SIZE:
		com->par.width = dashdmx->use_max_res ? dashdmx->width : 0;
		com->par.height = dashdmx->use_max_res ? dashdmx->height : 0;
		return GF_OK;

	case GF_NET_SERVICE_QUALITY_SWITCH:
		if (com->switch_quality.set_tile_mode_plus_one) {
			GF_BaseInterface *pl = (GF_BaseInterface *)plug;
			GF_DASHTileAdaptationMode tile_mode = com->switch_quality.set_tile_mode_plus_one - 1;
			gf_dash_set_tile_adaptation_mode(dashdmx->dash, tile_mode, 100);
			
			switch (tile_mode) {
			case GF_DASH_ADAPT_TILE_ROWS: gf_modules_set_option(pl,  "DASH", "TileAdaptation", "rows"); break;
			case GF_DASH_ADAPT_TILE_ROWS_REVERSE: gf_modules_set_option(pl,  "DASH", "TileAdaptation", "reverseRows"); break;
			case GF_DASH_ADAPT_TILE_ROWS_MIDDLE: gf_modules_set_option(pl,  "DASH", "TileAdaptation", "middleRows"); break;
			case GF_DASH_ADAPT_TILE_COLUMNS: gf_modules_set_option(pl,  "DASH", "TileAdaptation", "columns"); break;
			case GF_DASH_ADAPT_TILE_COLUMNS_REVERSE: gf_modules_set_option(pl,  "DASH", "TileAdaptation", "reverseColumns"); break;
			case GF_DASH_ADAPT_TILE_COLUMNS_MIDDLE: gf_modules_set_option(pl,  "DASH", "TileAdaptation", "middleColumns"); break;
			case GF_DASH_ADAPT_TILE_CENTER: gf_modules_set_option(pl,  "DASH", "TileAdaptation", "center"); break;
			case GF_DASH_ADAPT_TILE_EDGES: gf_modules_set_option(pl,  "DASH", "TileAdaptation", "edges"); break;
			case GF_DASH_ADAPT_TILE_NONE:
			default:
				gf_modules_set_option(pl,  "DASH", "TileAdaptation", "none");
				break;
			}
		} else if (com->switch_quality.set_auto) {
			gf_dash_set_automatic_switching(dashdmx->dash, 1);
		} else if (com->base.on_channel) {
				seg_filter_src = MPD_GetInputServiceForChannel(dashdmx, com->base.on_channel);
				if (!seg_filter_src) return GF_NOT_SUPPORTED;
				idx = MPD_GetGroupIndexForChannel(dashdmx, com->play.on_channel);
				if (idx < 0) return GF_BAD_PARAM;
			
				gf_dash_group_set_quality_degradation_hint(dashdmx->dash, idx, com->switch_quality.quality_degradation);
				if (! com->switch_quality.ID) return GF_OK;
			
				if (com->switch_quality.dependent_group_index) {
					if (com->switch_quality.dependent_group_index > gf_dash_group_get_num_groups_depending_on(dashdmx->dash, idx))
						return GF_BAD_PARAM;
			
					idx = gf_dash_get_dependent_group_index(dashdmx->dash, idx, com->switch_quality.dependent_group_index-1);
					if (idx==-1) return GF_BAD_PARAM;
				}

				gf_dash_set_automatic_switching(dashdmx->dash, 0);
				gf_dash_group_select_quality(dashdmx->dash, idx, com->switch_quality.ID);
		} else {
			gf_dash_switch_quality(dashdmx->dash, com->switch_quality.up, dashdmx->immediate_switch);
		}
		return GF_OK;

	case GF_NET_GET_TIMESHIFT:
		com->timeshift.time = gf_dash_get_timeshift_buffer_pos(dashdmx->dash);
		return GF_OK;
	case GF_NET_SERVICE_CAN_REVERSE_PLAYBACK:
		return dashdmx_dash_can_reverse_playback(dashdmx);

	case GF_NET_ASSOCIATED_CONTENT_TIMING:
		gf_dash_override_ntp(dashdmx->dash, com->addon_time.ntp);
		return GF_OK;
	default:
		break;
	}
	/*not supported*/
	if (!com->base.on_channel) return GF_NOT_SUPPORTED;

	seg_filter_src = MPD_GetInputServiceForChannel(dashdmx, com->base.on_channel);
	if (!seg_filter_src) return GF_NOT_SUPPORTED;

	switch (com->command_type) {
	case GF_NET_CHAN_INTERACTIVE:
		/* TODO - we are interactive if not live without timeshift */
		return GF_OK;

	case GF_NET_CHAN_GET_SRD:
	{
		Bool res;
		idx = MPD_GetGroupIndexForChannel(dashdmx, com->base.on_channel);
		if (idx < 0) return GF_BAD_PARAM;
		if (com->srd.dependent_group_index) {
			if (com->srd.dependent_group_index > gf_dash_group_get_num_groups_depending_on(dashdmx->dash, idx))
				return GF_BAD_PARAM;
			
			idx = gf_dash_get_dependent_group_index(dashdmx->dash, idx, com->srd.dependent_group_index-1);
		}		
		res = gf_dash_group_get_srd_info(dashdmx->dash, idx, NULL, &com->srd.x, &com->srd.y, &com->srd.w, &com->srd.h, &com->srd.width, &com->srd.height);
		return res ? GF_OK : GF_NOT_SUPPORTED;
	}

	case GF_NET_GET_STATS:
	{
		idx = MPD_GetGroupIndexForChannel(dashdmx, com->base.on_channel);
		if (idx < 0) return GF_BAD_PARAM;
		com->net_stats.bw_down = 8 * gf_dash_group_get_download_rate(dashdmx->dash, idx);
	}
		return GF_OK;

	case GF_NET_SERVICE_QUALITY_QUERY:
	{
		GF_DASHQualityInfo qinfo;
		GF_Err e;
		u32 count, g_idx;
		idx = MPD_GetGroupIndexForChannel(dashdmx, com->quality_query.on_channel);
		if (idx < 0) return GF_BAD_PARAM;
		count = gf_dash_group_get_num_qualities(dashdmx->dash, idx);
		if (!com->quality_query.index && !com->quality_query.dependent_group_index) {
			com->quality_query.index = count;
			com->quality_query.dependent_group_index = gf_dash_group_get_num_groups_depending_on(dashdmx->dash, idx);
			return GF_OK;
		}
		if (com->quality_query.dependent_group_index) {
			if (com->quality_query.dependent_group_index > gf_dash_group_get_num_groups_depending_on(dashdmx->dash, idx))
				return GF_BAD_PARAM;
			
			g_idx = gf_dash_get_dependent_group_index(dashdmx->dash, idx, com->quality_query.dependent_group_index-1);
			if (g_idx==(u32)-1) return GF_BAD_PARAM;
			count = gf_dash_group_get_num_qualities(dashdmx->dash, g_idx);
			if (com->quality_query.index>count) return GF_BAD_PARAM;
		} else {
			if (com->quality_query.index>count) return GF_BAD_PARAM;
			g_idx = idx;
		}

		e = gf_dash_group_get_quality_info(dashdmx->dash, g_idx, com->quality_query.index-1, &qinfo);
		if (e) return e;
		//group->bandwidth = qinfo.bandwidth;
		com->quality_query.bandwidth = qinfo.bandwidth;
		com->quality_query.ID = qinfo.ID;
		com->quality_query.mime = qinfo.mime;
		com->quality_query.codec = qinfo.codec;
		com->quality_query.width = qinfo.width;
		com->quality_query.height = qinfo.height;
		com->quality_query.interlaced = qinfo.interlaced;
		if (qinfo.fps_den) {
			com->quality_query.fps = qinfo.fps_num;
			com->quality_query.fps /= qinfo.fps_den;
		} else {
			com->quality_query.fps = qinfo.fps_num;
		}
		com->quality_query.par_num = qinfo.par_num;
		com->quality_query.par_den = qinfo.par_den;
		com->quality_query.sample_rate = qinfo.sample_rate;
		com->quality_query.nb_channels = qinfo.nb_channels;
		com->quality_query.disabled = qinfo.disabled;
		com->quality_query.is_selected = qinfo.is_selected;
		com->quality_query.automatic = gf_dash_get_automatic_switching(dashdmx->dash);
		com->quality_query.tile_adaptation_mode = (u32) gf_dash_get_tile_adaptation_mode(dashdmx->dash);
		return GF_OK;
	}
	case GF_NET_CHAN_VISIBILITY_HINT:
		idx = MPD_GetGroupIndexForChannel(dashdmx, com->base.on_channel);
		if (idx < 0) return GF_BAD_PARAM;

		return gf_dash_group_set_visible_rect(dashdmx->dash, idx, com->visibility_hint.min_x, com->visibility_hint.max_x, com->visibility_hint.min_y, com->visibility_hint.max_y);

	case GF_NET_CHAN_BUFFER:
		/*get it from MPD minBufferTime - if not in low latency mode, indicate the value given in MPD (not possible to fetch segments earlier) - to be more precise we should get the min segment duration for this group*/
		if (/* !dashdmx->use_low_latency && */ (dashdmx->buffer_mode>=GF_DASHDMX_BUFFER_MIN)) {
			u32 max = gf_dash_get_min_buffer_time(dashdmx->dash);
			if (max>com->buffer.max)
				com->buffer.max = max;

			if (!com->buffer.min && ! gf_dash_is_dynamic_mpd(dashdmx->dash)) {
				com->buffer.min = 1;
			}
		}
		idx = MPD_GetGroupIndexForChannel(dashdmx, com->play.on_channel);
		if (idx >= 0) {
			gf_dash_group_set_max_buffer_playout(dashdmx->dash, idx, com->buffer.max);
		}
		return GF_OK;

	case GF_NET_CHAN_DURATION:
		com->duration.duration = gf_dash_get_duration(dashdmx->dash);
		idx = MPD_GetGroupIndexForChannel(dashdmx, com->play.on_channel);
		if (idx >= 0) {
			com->duration.time_shift_buffer = gf_dash_group_get_time_shift_buffer_depth(dashdmx->dash, idx);
		}
		return GF_OK;

	case GF_NET_CHAN_PLAY:

		idx = MPD_GetGroupIndexForChannel(dashdmx, com->play.on_channel);
		if (idx < 0) return GF_BAD_PARAM;

		//adjust play range from media timestamps to MPD time
		if (com->play.timestamp_based) {
			u32 timescale;
			u64 pto;
			Double offset;
			GF_MPDGroup *group = gf_dash_get_group_udta(dashdmx->dash, idx);

			if (com->play.timestamp_based==1) {
				gf_dash_group_get_presentation_time_offset(dashdmx->dash, idx, &pto, &timescale);
				offset = (Double) pto;
				offset /= timescale;
				com->play.start_range -= offset;
				if (com->play.start_range < 0) com->play.start_range = 0;
			}

			group->is_timestamp_based = 1;
			group->pto_setup = 0;
			dashdmx->media_start_range = com->play.start_range;
		} else {
			GF_MPDGroup *group = gf_dash_get_group_udta(dashdmx->dash, idx);
			group->is_timestamp_based = 0;
			group->pto_setup = 0;
			if (com->play.start_range<0) com->play.start_range = 0;
			//in m3u8, we need also media start time for mapping time
			if (gf_dash_is_m3u8(dashdmx->dash))
				dashdmx->media_start_range = com->play.start_range;
		}

		//we cannot handle seek request outside of a period being setup, this messes up our internal service setup
		//we postpone the seek and will request it later on ...
		if (gf_dash_in_period_setup(dashdmx->dash)) {
			u64 p_end = gf_dash_get_period_duration(dashdmx->dash);
			if (p_end) {
				p_end += gf_dash_get_period_start(dashdmx->dash);
				if (p_end<1000*com->play.start_range) {
					dashdmx->seek_request = com->play.start_range;
					return GF_OK;
				}
			}
		}


		gf_dash_set_speed(dashdmx->dash, com->play.speed);

		/*don't seek if this command is the first PLAY request of objects declared by the subservice, unless start range is not default one (0) */
		if (!dashdmx->nb_playing && (!com->play.initial_broadcast_play || (com->play.start_range>1.0) )) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[DASHDmx] Received Play command from terminal on channel %p on Service (%p)\n", com->base.on_channel, dashdmx->service));

			if (com->play.end_range<=0) {
				u32 ms = (u32) ( 1000 * (-com->play.end_range) );
				if (ms<1000) ms = 0;
				gf_dash_set_timeshift(dashdmx->dash, ms);
			}
			gf_dash_seek(dashdmx->dash, com->play.start_range);

			//to remove once we manage to keep the service alive
			/*don't forward commands if a switch of period is to be scheduled, we are killing the service anyway ...*/
			if (gf_dash_get_period_switch_status(dashdmx->dash)) return GF_OK;
		} else if (!com->play.initial_broadcast_play) {
			/*don't forward commands if a switch of period is to be scheduled, we are killing the service anyway ...*/
			if (gf_dash_get_period_switch_status(dashdmx->dash)) return GF_OK;

			//seek on a single group
			gf_dash_group_seek(dashdmx->dash, idx, com->play.start_range);
		}

		//check if current segment playback should be aborted
		com->play.dash_segment_switch = gf_dash_group_segment_switch_forced(dashdmx->dash, idx);

		gf_dash_group_select(dashdmx->dash, idx, GF_TRUE);
		gf_dash_set_group_done(dashdmx->dash, (u32) idx, 0);

		//adjust start range from MPD time to media time
		{
			u64 pto;
			u32 timescale;
			com->play.start_range = gf_dash_group_get_start_range(dashdmx->dash, idx);
			gf_dash_group_get_presentation_time_offset(dashdmx->dash, idx, &pto, &timescale);
			com->play.start_range += ((Double)pto) / timescale;
		}

		dashdmx->nb_playing++;
		return seg_filter_src->ServiceCommand(seg_filter_src, com);

	case GF_NET_CHAN_STOP:
	{
		s32 idx = MPD_GetGroupIndexForChannel(dashdmx, com->play.on_channel);
		if (idx>=0) {
			gf_dash_set_group_done(dashdmx->dash, (u32) idx, 1);
		}
		if (dashdmx->nb_playing)
			dashdmx->nb_playing--;
	}
	return seg_filter_src->ServiceCommand(seg_filter_src, com);

	/*we could get it from MPD*/
	case GF_NET_CHAN_GET_PIXEL_AR:
		/* defer to the real input service */
		return seg_filter_src->ServiceCommand(seg_filter_src, com);

	case GF_NET_CHAN_SET_SPEED:
		gf_dash_set_speed(dashdmx->dash, com->play.speed);
		return seg_filter_src->ServiceCommand(seg_filter_src, com);

	default:
		return seg_filter_src->ServiceCommand(seg_filter_src, com);
	}
#endif

	return GF_FALSE;
}

GF_Err dashdmx_process(GF_Filter *filter)
{
//	GF_DASHDmxCtx *dashdmx = (GF_DASHDmxCtx*) gf_filter_get_udta(filter);
	return GF_OK;
}

static const GF_FilterCapability DASHDmxInputs[] =
{
	CAP_INC_STRING(GF_PROP_PID_MIME, "application/dash+xml|video/vnd.3gpp.mpd|audio/vnd.3gpp.mpd|video/vnd.mpeg.dash.mpd|audio/vnd.mpeg.dash.mpd"),
	{},
	CAP_INC_STRING(GF_PROP_PID_FILE_EXT, "mpd|m3u8|3gm|ism"),
};


#define OFFS(_n)	#_n, offsetof(GF_DASHDmxCtx, _n)
static const GF_FilterArgs DASHDmxArgs[] =
{
	{ OFFS(max_buffer), "max content buffer in ms", GF_PROP_UINT, "10000", NULL, GF_FALSE},
	{ OFFS(auto_switch), "switch quality every N segments, disabled if 0", GF_PROP_UINT, "0", NULL, GF_FALSE},
	{ OFFS(store), "enables file caching", GF_PROP_UINT, "mem", "mem|file|cache", GF_FALSE},
	{ OFFS(algo), "adaptation algorithm to use. The following algorithms are defined:\n\
					\tnone: no adaptation logic\n\
					\tgrate: GAPC legacy algo based on available rate\n\
					\tgbuf: GAPC legacy algo based on buffer occupancy\n\
					\tbba0: BBA-0\n\
					\tbolaf: BOLA Finite\n\
					\tbolab: BOLA Basic\n\
					\tbolau: BOLA-U\n\
					\tbolao: BOLA-O\n\
					", GF_PROP_UINT, "gbuf", "none|grate|gbuf|bba0|bolaf|bolab|bolau|bolao", GF_FALSE},
	{ OFFS(start_with), "specifies the initial representation criteria:\n\
						min_q: start with lowest quality\n\
						max_q: start with highest quality\n\
						min_bw: start with lowest bitrate\n\
						max_bw: start with highest bitrate; for tiles are used, all low priority tiles will have the lower (below max) bandwidth selected\n\
						max_bw_tiles: start with highest bitrate; for tiles all low priority tiles will have their lowest bandwidth selected\n\
						", GF_PROP_UINT, "min_bw", "min_q|max_q|min_bw|max_bw|max_bw_tiles", GF_FALSE},

	{ OFFS(max_res), "use max media resolution to configure display", GF_PROP_BOOL, "true", NULL, GF_FALSE},
	{ OFFS(immediate), "when interactive switching is requested and immediate is set, the buffer segments are trashed", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(abort), "allow abort during a segment download", GF_PROP_BOOL, "false", NULL, GF_FALSE},

	{ OFFS(bmode), "Selects buffer mode:\
					none: uses the player settings for buffering\n\
					segs: buffers complete segments as indicated in MPD before handing them to the player.\n\
					minb: asks the player to buffer media for the time indicated in the MPD, but segments are not pre-buffered\n\
					", GF_PROP_UINT, "minb", "none|minb|segs", GF_FALSE},

	{ OFFS(lowl), "Configure low latency. In low-latency mode, media data is parsed as soon as possible while segment is being downloaded. If chunk is selected, media data is re-parsed at each HTTP 1.1 chunk end. If always is selected, media data is re-parsed as soon as HTTP data is received.", GF_PROP_UINT, "none", "none|chunk|always", GF_FALSE},

	{ OFFS(shift_utc), "shifts DASH UTC clock", GF_PROP_SINT, "0", NULL, GF_FALSE},
	{ OFFS(server_utc), "Uses ServerUTC: or Date: http headers instead of local UTC", GF_PROP_BOOL, "yes", NULL, GF_FALSE},
	{ OFFS(screen_res), "Uses screen resolution in selection phase", GF_PROP_BOOL, "yes", NULL, GF_FALSE},
	{ OFFS(timeshift), "Sets initial timshift in ms (if >0) or in %% of timeshift buffer (if <0)", GF_PROP_UINT, "0", NULL, GF_FALSE},
	{ OFFS(tile_mode), "Sets tile adaptiaion mode:\
						none: bitrate is shared equaly accross all tiles\n\
						rows: bitrate decreases for each row of tiles starting from the top, same rate for each tile on the row\n\
						rrows: bitrate decreases for each row of tiles starting from the bottom, same rate for each tile on the row\n\
						mrows: bitrate decreased for top and bottom rows only, same rate for each tile on the row\n\
						cols: bitrate decreases for each columns of tiles starting from the left, same rate for each tile on the columns\n\
						rcols: bitrate decreases for each columns of tiles starting from the right, same rate for each tile on the columns\n\
						mcols: bitrate decreased for left and right columns only, same rate for each tile on the columns\n\
						center: bitrate decreased for all tiles on the edge of the picture\n\
						edges: bitrate decreased for all tiles on the center of the picture\n\
						", GF_PROP_UINT, "none", "none|rows|rrows|mrows|cols|rcols|mcols|center|edges", GF_FALSE},

	{ OFFS(tiles_rate), "Indicates the amount of bandwidth to use at each quality level. The rate is recursively applied at each level, e.g. if 50%, Level1 gets 50%, level2 gets 25%, ... If 100, automatic rate allocation will be done by maximizing the quality in order of priority. If 0, bitstream will not be smoothed across tiles/qualities, and concurrency may happen between different media.", GF_PROP_UINT, "100", NULL, GF_FALSE},
	{ OFFS(delay40X), "Delay in millisconds to wait between two 40X on the same segment", GF_PROP_UINT, "500", NULL, GF_FALSE},
	{ OFFS(exp_threshold), "Delay in millisconds to wait after the segment AvailabilityEndDate before considering the segment lost", GF_PROP_UINT, "100", NULL, GF_FALSE},

	{ OFFS(switch_count), "Indicates how many segments the client shall wait before switching up bandwidth. If 0, switch will happen as soon as the bandwidth is enough, but this is more prone to network variations", GF_PROP_UINT, "1", NULL, GF_FALSE},
	{ OFFS(aggressive), "If enabled, switching algo targets the closest bandwidth fitting the available download rate. If no, switching algo targets the lowest bitrate representation that is above the currently played (eg does not try to switch to max bandwidth)", GF_PROP_BOOL, "no", NULL, GF_FALSE},
	{ OFFS(debug_as), "Plays only the adaptation set indicated by its index in the MPD; if negative, all sets are used", GF_PROP_UINT, "-1", NULL, GF_FALSE},
	{ OFFS(speed), "Enables adaptation based on playback speed", GF_PROP_BOOL, "no", NULL, GF_FALSE},

	{}
};

static const GF_FilterCapability DASHDmxOutputs[] =
{
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_EXC_UINT(GF_PROP_PID_OTI, GPAC_OTI_RAW_MEDIA_STREAM),
	{},
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_SCENE),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_SCENE_BIFS),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_SCENE_BIFS_V2),
	{},
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_OD),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_OD_V1),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_OD_V2),
	{},
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_PRIVATE_SCENE),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_PRIVATE_SCENE_EPG),
};


GF_FilterRegister DASHDmxRegister = {
	.name = "dash",
	.description = "MPEG-DASH & HLS Demux",
	.private_size = sizeof(GF_DASHDmxCtx),
	.initialize = dashdmx_initialize,
	.finalize = dashdmx_finalize,
	.args = DASHDmxArgs,
	INCAPS(DASHDmxInputs),
	OUTCAPS(DASHDmxOutputs),
	.configure_pid = dashdmx_configure_pid,
	.process = dashdmx_process,
	.process_event = dashdmx_process_event,
};


#endif //GPAC_DISABLE_DASH_CLIENT

const GF_FilterRegister *dashdmx_register(GF_FilterSession *session)
{
#ifndef GPAC_DISABLE_DASH_CLIENT
	return &DASHDmxRegister;
#else
	return NULL;
#endif
}
