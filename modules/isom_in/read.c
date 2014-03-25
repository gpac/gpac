/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / IsoMedia reader module
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

#include "isom_in.h"

#ifndef GPAC_DISABLE_ISOM

#include <gpac/ismacryp.h>

ISOMChannel *isor_get_channel(ISOMReader *reader, LPNETCHANNEL channel)
{
	u32 i=0;
	ISOMChannel *ch;
	while ((ch = (ISOMChannel *)gf_list_enum(reader->channels, &i))) {
		if (ch->channel == channel) return ch;
	}
	return NULL;
}

static void isor_delete_channel(ISOMReader *reader, ISOMChannel *ch)
{
	u32 i=0;
	ISOMChannel *ch2;
	while ((ch2 = (ISOMChannel *)gf_list_enum(reader->channels, &i))) {
		if (ch2 == ch) {
			isor_reset_reader(ch);
			gf_free(ch);
			gf_list_rem(reader->channels, i-1);
			return;
		}
	}
}

static GFINLINE Bool isor_is_local(const char *url)
{
	if (!strnicmp(url, "file://", 7)) return GF_TRUE;
	if (!strnicmp(url, "gmem://", 7)) return GF_TRUE;
	if (strstr(url, "://")) return GF_FALSE;
	/*the rest is local (mounted on FS)*/
	return GF_TRUE;
}

static const char * ISOR_MIME_TYPES[] = {
  /* First one is specific because we probe */
  "application/x-isomedia", "*", "IsoMedia Files",
  "video/mp4", "mp4 mpg4", "MPEG-4 Movies",
  "audio/mp4", "m4a mp4 mpg4", "MPEG-4 Music",
  "application/mp4", "mp4 mpg4", "MPEG-4 Applications",
  "video/3gpp", "3gp 3gpp", "3GPP/MMS Movies",
  "audio/3gpp", "3gp 3gpp", "3GPP/MMS Music",
  "video/3gpp2", "3g2 3gp2", "3GPP2/MMS Movies",
  "audio/3gpp2", "3g2 3gp2", "3GPP2/MMS Music",
  NULL
};
  
u32 ISOR_RegisterMimeTypes(const GF_InputService *plug){
  u32 i;
  for (i = 0 ; ISOR_MIME_TYPES[i]; i+=3)
    gf_term_register_mime_type(plug, ISOR_MIME_TYPES[i], ISOR_MIME_TYPES[i+1], ISOR_MIME_TYPES[i+2]);
  return i/3;
}

Bool ISOR_CanHandleURL(GF_InputService *plug, const char *url)
{
	char *ext;
	if (!strnicmp(url, "rtsp://", 7)) return GF_FALSE;
	ext = (char *)strrchr(url, '.');
	{
	    u32 i;
	    /* We don't start at 0 since first one is specific */
	    for (i = 3 ; ISOR_MIME_TYPES[i]; i+=3)
	      if (gf_term_check_extension(plug, ISOR_MIME_TYPES[i], ISOR_MIME_TYPES[i+1], ISOR_MIME_TYPES[i+2], ext))
		return GF_TRUE;
	}

	if (ext && gf_isom_probe_file(url)) {
		gf_term_check_extension(plug, ISOR_MIME_TYPES[0], ext+1, ISOR_MIME_TYPES[2], ext);
		return GF_TRUE;
	}
	return GF_FALSE;
}

void send_proxy_command(ISOMReader *read, Bool is_disconnect, Bool is_add_media, GF_Err e, GF_Descriptor *desc, LPNETCHANNEL channel)
{
	GF_NetworkCommand command;
	command.command_type = GF_NET_SERVICE_STATUS_PROXY;
	command.status.e = e;
	command.status.channel = channel;
	command.status.is_disconnect = is_disconnect;
	command.status.is_add_media = is_add_media;
	command.status.desc = desc;
	read->input->query_proxy(read->input, &command);
}

void isor_check_buffer_level(ISOMReader *read)
{
	Double dld_time_remaining, mov_rate;
	GF_NetworkCommand com;
	u32 i, total, done, Bps;
	u64 dur;
	GF_NetIOStatus status;
	Bool do_buffer = GF_FALSE;
	if (!read->dnload) return;
	if (!read->mov) return;

	gf_dm_sess_get_stats(read->dnload, NULL, NULL, &total, &done, &Bps, &status);
	if (!Bps) return;

	dld_time_remaining = total-done;
	dld_time_remaining /= Bps;
	mov_rate = total;
	dur = gf_isom_get_duration(read->mov);
	if (dur) {
		mov_rate /= dur;
		mov_rate *= gf_isom_get_timescale(read->mov);
	}

	for (i=0; i<gf_list_count(read->channels); i++) {
		ISOMChannel *ch = gf_list_get(read->channels, i);
		Double time_remain_ch = (Double) gf_isom_get_media_duration(read->mov, ch->track);
		u32 buffer_level=0;
		if (total==done) {
			time_remain_ch = 0;
			do_buffer = GF_FALSE;
		} else if (ch->last_state == GF_EOS) {
			time_remain_ch = 0;
			do_buffer = GF_TRUE;
		} else {
			u64 data_offset;
			u32 di, sn = ch->sample_num ? ch->sample_num : 1;
			GF_ISOSample *samp = gf_isom_get_sample_info(read->mov, ch->track, sn, &di, &data_offset);
			if (!samp) continue;

			data_offset += samp->dataLength;

			//we only send buffer on/off based on remainging playback time in channel
#if 0
			//we don't have enough data
			if (((data_offset + ch->buffer_min * mov_rate/1000 > done))) {
				do_buffer = GF_TRUE;
			}
			//we have enough buffer
			else if ((data_offset + ch->buffer_max * mov_rate/1000 <= done)) {
				do_buffer = GF_FALSE;
			} 
#endif
			time_remain_ch -= (samp->DTS + samp->CTS_Offset);
			if (time_remain_ch<0) time_remain_ch=0;
			gf_isom_sample_del(&samp);

			time_remain_ch /= ch->time_scale;
			//we add 2 seconds safety
			if (time_remain_ch && (time_remain_ch < dld_time_remaining + 2)) {
				do_buffer = GF_TRUE;
				buffer_level = (u32) (ch->buffer_max * time_remain_ch / dld_time_remaining);
			} else {
				buffer_level = (u32) (1000 * (data_offset - done)/mov_rate);
				do_buffer = GF_FALSE;
			}
		}

		if (do_buffer != ch->buffering) {
			GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[IsoMedia] Buffering %s at %d: %g sec still to download and %g sec still to play on track %d (movie rate %g - download rate %g kbps)\n", do_buffer ? "on" : "off", gf_sys_clock(), dld_time_remaining , time_remain_ch, ch->track_id, mov_rate*8/1000, Bps*8.0/1000));

			memset(&com, 0, sizeof(GF_NetworkCommand));
			com.command_type = do_buffer ? GF_NET_CHAN_PAUSE : GF_NET_CHAN_RESUME;
			com.buffer.on_channel = ch->channel;
			gf_term_on_command(read->service, &com, GF_OK);
			ch->buffering = do_buffer;
		} else if (ch->buffering) {
			memset(&com, 0, sizeof(GF_NetworkCommand));
			com.command_type = GF_NET_CHAN_BUFFER;
			com.buffer.on_channel = ch->channel;
			com.buffer.min = ch->buffer_min;
			com.buffer.max = ch->buffer_max;
			com.buffer.occupancy = buffer_level;
			gf_term_on_command(read->service, &com, GF_OK);
		}
	}
}

void isor_net_io(void *cbk, GF_NETIO_Parameter *param)
{
	GF_Err e;
	u32 size = 0;
	char *local_name;
	ISOMReader *read = (ISOMReader *) cbk;
	
	/*handle service message*/
	gf_term_download_update_stats(read->dnload);

	if (param->msg_type==GF_NETIO_DATA_TRANSFERED) {
		e = GF_EOS;
	} else if (param->msg_type==GF_NETIO_DATA_EXCHANGE) {
		e = GF_OK;
		size = param->size;
	} else {
		e = param->error;
	}

	if (e<GF_OK) {
		/*error opening service*/
		if (!read->mov) {
            /* if there is an intermediate between this module and the terminal, report to it */
            if (read->input->query_proxy && read->input->proxy_udta && read->input->proxy_type) {
                send_proxy_command(read, GF_FALSE, GF_FALSE, e, NULL, NULL);
            } else {
                gf_term_on_connect(read->service, NULL, e);
            }
        }
		return;
	}

	/*open file if not done yet (bad interleaving)*/
	if (e==GF_EOS) {
		const char *local_name;
		if (read->mov) return;
		local_name = gf_dm_sess_get_cache_name(read->dnload);
		if (!local_name) {
            if (read->input->query_proxy && read->input->proxy_udta && read->input->proxy_type) {
                send_proxy_command(read, GF_FALSE, GF_FALSE, GF_SERVICE_ERROR, NULL, NULL);
            } else {
                gf_term_on_connect(read->service, NULL, GF_SERVICE_ERROR);
            }
			return;
		}
		e = GF_OK;
		read->mov = gf_isom_open(local_name, GF_ISOM_OPEN_READ, NULL);
		if (!read->mov) e = gf_isom_last_error(NULL);
		else read->time_scale = gf_isom_get_timescale(read->mov);
        if (read->input->query_proxy && read->input->proxy_udta && read->input->proxy_type) {
            send_proxy_command(read, GF_FALSE, GF_FALSE, GF_OK, NULL, NULL);
        } else {
            gf_term_on_connect(read->service, NULL, GF_OK);
        }
		if (read->no_service_desc) isor_declare_objects(read);
	}
	
	if (!size) return;

	/*service is opened, nothing to do*/
	if (read->mov) {
		isor_check_buffer_level(read);

		/*end of chunk*/
		if (read->frag_type && (param->reply==1) ) {
			u64 bytesMissing = 0;
			gf_mx_p(read->segment_mutex);
			e = gf_isom_refresh_fragmented(read->mov, &bytesMissing, NULL);
			gf_mx_v(read->segment_mutex);
		}
		return;
	}

	/*try to open the service*/
	local_name = (char *)gf_dm_sess_get_cache_name(read->dnload);
	if (!local_name) {
        if (read->input->query_proxy && read->input->proxy_udta && read->input->proxy_type) {
            send_proxy_command(read, GF_FALSE, GF_FALSE, GF_SERVICE_ERROR, NULL, NULL);
        } else {
            gf_term_on_connect(read->service, NULL, GF_SERVICE_ERROR);
        }
		return;
	}

	/*not enogh data yet*/
	if (read->missing_bytes && (read->missing_bytes > size) ) {
		read->missing_bytes -= size;
		return;
	}
	
	e = gf_isom_open_progressive(local_name, 0, 0, &read->mov, &read->missing_bytes);
	switch (e) {
	case GF_ISOM_INCOMPLETE_FILE:
		return;
	case GF_OK:
		break;
	default:
        if (read->input->query_proxy && read->input->proxy_udta && read->input->proxy_type) {
            send_proxy_command(read, GF_FALSE, GF_FALSE, e, NULL, NULL);
        } else {
            gf_term_on_connect(read->service, NULL, e);
        }
		return;
	}
	read->frag_type = gf_isom_is_fragmented(read->mov) ? 1 : 0;
	
	/*ok let's go, we can setup the decoders */
	read->time_scale = gf_isom_get_timescale(read->mov);
    if (read->input->query_proxy && read->input->proxy_udta && read->input->proxy_type) {
        send_proxy_command(read, GF_FALSE, GF_FALSE, GF_OK, NULL, NULL);
    } else {
        gf_term_on_connect(read->service, NULL, GF_OK);
    }

	if (read->no_service_desc) isor_declare_objects(read);
}


void isor_setup_download(GF_InputService *plug, const char *url)
{
	ISOMReader *read = (ISOMReader *) plug->priv;
	read->dnload = gf_term_download_new(read->service, url, 0, isor_net_io, read);
	if (!read->dnload) {
        if (read->input->query_proxy && read->input->proxy_udta && read->input->proxy_type) {
            send_proxy_command(read, GF_FALSE, GF_FALSE, GF_NOT_SUPPORTED, NULL, NULL);
        } else {
            gf_term_on_connect(read->service, NULL, GF_NOT_SUPPORTED);
        }
	} else {
		/*start our download (threaded)*/
		gf_dm_sess_process(read->dnload);
	}
	/*service confirm is done once IOD can be fetched*/
}

GF_Err ISOR_ConnectService(GF_InputService *plug, GF_ClientService *serv, const char *url)
{
	char szURL[2048];
	char *tmp;
	ISOMReader *read;
	if (!plug || !plug->priv || !serv) return GF_SERVICE_ERROR;
	read = (ISOMReader *) plug->priv;

	read->input = plug;
	read->service = serv;

	if (read->dnload) gf_term_download_del(read->dnload);
	read->dnload = NULL;

	read->base_track_id = 0;
	if (!url) return GF_URL_ERROR;

	strcpy(szURL, url);
	tmp = strrchr(szURL, '.');
	if (tmp) {
		tmp = strchr(tmp, '#');
		if (tmp) {
			if (!strnicmp(tmp, "#trackID=", 9)) {
				read->base_track_id = atoi(tmp+9);
			} else {
				read->base_track_id = atoi(tmp+1);
			}
			tmp[0] = 0;
		}
	}

	if (isor_is_local(szURL)) {
		GF_Err e;
		u64 start_range, end_range;
		start_range = end_range = 0;
		if (plug->query_proxy) {
			GF_NetworkCommand param;
			param.command_type = GF_NET_SERVICE_QUERY_INIT_RANGE;
			if (read->input->query_proxy(read->input, &param)==GF_OK) {
				start_range = param.url_query.start_range;
				end_range = param.url_query.end_range;
			}
		}
		e = gf_isom_open_progressive(szURL, start_range, end_range, &read->mov, &read->missing_bytes);
		if (e != GF_OK){
		    GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[IsoMedia] : error while opening %s, error=%s\n", szURL, gf_error_to_string(e)));
            if (read->input->query_proxy && read->input->proxy_udta && read->input->proxy_type) {
                send_proxy_command(read, 0, 0, e, NULL, NULL);
            } else {
                gf_term_on_connect(read->service, NULL, e);
            }
			return GF_OK;
		}
		read->frag_type = gf_isom_is_fragmented(read->mov) ? 1 : 0;
		read->seg_opened = 2;

		read->time_scale = gf_isom_get_timescale(read->mov);
		/*reply to user*/
        if (read->input->query_proxy && read->input->proxy_udta && read->input->proxy_type) {
            send_proxy_command(read, GF_FALSE, GF_FALSE, GF_OK, NULL, NULL);
        } else {
            gf_term_on_connect(read->service, NULL, GF_OK);
        }

		if (read->no_service_desc) isor_declare_objects(read);

	} else {
		/*setup downloader*/
		isor_setup_download(plug, szURL);
	}
	return GF_OK;
}

GF_Err ISOR_CloseService(GF_InputService *plug)
{
	GF_Err reply;
	ISOMReader *read;
	if (!plug || !plug->priv) return GF_SERVICE_ERROR;
	read = (ISOMReader *) plug->priv;
	reply = GF_OK;

	read->disconnected = GF_TRUE;
	if (read->mov) gf_isom_close(read->mov);
	read->mov = NULL;

	while (gf_list_count(read->channels)) {
		ISOMChannel *ch = (ISOMChannel *)gf_list_get(read->channels, 0);
		gf_list_rem(read->channels, 0);
		isor_delete_channel(read, ch);
	}

	if (read->dnload) gf_term_download_del(read->dnload);
	read->dnload = NULL;

    if (read->input->query_proxy && read->input->proxy_udta && read->input->proxy_type) {
        send_proxy_command(read, GF_TRUE, GF_FALSE, reply, NULL, NULL);
    } else {
        gf_term_on_disconnect(read->service, NULL, reply);
    }
	return GF_OK;
}

static Bool check_mpeg4_systems(GF_InputService *plug, GF_ISOFile *mov)
{
	char *opt, *bname, *br, *next;
	u32 i, count, brand;
	Bool has_mpeg4;
	GF_Err e;
	e = gf_isom_get_brand_info(mov, &brand, &i, &count);
	/*no brand == MP4 v1*/
	if (e || !brand) return GF_TRUE;

	has_mpeg4 = GF_FALSE;
	if ((brand==GF_ISOM_BRAND_MP41) || (brand==GF_ISOM_BRAND_MP42)) has_mpeg4 = GF_TRUE;

	opt = (char*) gf_modules_get_option((GF_BaseInterface *)plug, "ISOReader", "IgnoreMPEG-4ForBrands");
	if (!opt) {
		gf_modules_set_option((GF_BaseInterface *)plug, "ISOReader", "IgnoreMPEG-4ForBrands", "nd*");
		opt = (char*) gf_modules_get_option((GF_BaseInterface *)plug, "ISOReader", "IgnoreMPEG-4ForBrands");
	}

	for (i=0; i<count; i++) {
		e = gf_isom_get_alternate_brand(mov, i+1, &brand);
		if (e) return GF_FALSE;
		if ((brand==GF_ISOM_BRAND_MP41) || (brand==GF_ISOM_BRAND_MP42)) {
			has_mpeg4 = GF_TRUE;
			continue;
		}
		bname = (char*)gf_4cc_to_str(brand);
		br = opt;
		while (br) {
			Bool ignore = GF_FALSE;
			u32 orig_len, len;
			next = strchr(br, ' ');
			if (next) next[0] = 0;
			len = orig_len = (u32) strlen(br);

			while (len) {
				if (br[len-1]=='*') {
					br[len-1]=0;
					len--;
				} else {
					break;
				}
			}
			/*ignor all brands*/
			if (!len) ignore = GF_TRUE;
			else if (!strncmp(bname, br, len)) ignore = GF_TRUE;

			while (len<orig_len) {
				br[len] = '*';
				len++;
			}
			if (next) {
				next[0] = ' ';
				br = next + 1;
			} else {
				br = NULL;
			}
			if (ignore) return GF_FALSE;
		}
	}
	return has_mpeg4;
}

static u32 get_track_id(GF_ISOFile *mov, u32 media_type, u32 idx)
{
	u32 i, count, cur;
	cur=0;
	count = gf_isom_get_track_count(mov);
	for (i=0; i<count; i++) {
		if (gf_isom_get_media_type(mov, i+1) != media_type) continue;
		if (!idx) return gf_isom_get_track_id(mov, i+1);
		cur++;
		if (cur==idx) return gf_isom_get_track_id(mov, i+1);
	}
	return GF_FALSE;
}

/*fixme, this doesn't work properly with respect to @expect_type*/
static GF_Descriptor *ISOR_GetServiceDesc(GF_InputService *plug, u32 expect_type, const char *sub_url)
{
	u32 count, nb_st, i, trackID;
	GF_ESD *esd;
	ISOMReader *read;
	GF_InitialObjectDescriptor *iod;
	if (!plug || !plug->priv) return NULL;
	read = (ISOMReader *) plug->priv;
	if (!read->mov) return NULL;

	/*no matter what always read text as TTUs*/
	gf_isom_text_set_streaming_mode(read->mov, 1);

	trackID = 0;
	if (!sub_url) {
		trackID = read->base_track_id;
		read->base_track_id = 0;
	} else {
		char *ext = (char *)strrchr(sub_url, '#');
		if (!ext) {
			trackID = 0;
		} else {
			if (!strnicmp(ext, "#trackID=", 9)) trackID = atoi(ext+9);
			else if (!stricmp(ext, "#video")) trackID = get_track_id(read->mov, GF_ISOM_MEDIA_VISUAL, 0);
			else if (!strnicmp(ext, "#video", 6)) {
				trackID = atoi(ext+6);
				trackID = get_track_id(read->mov, GF_ISOM_MEDIA_VISUAL, trackID);
			}
			else if (!stricmp(ext, "#audio")) trackID = get_track_id(read->mov, GF_ISOM_MEDIA_AUDIO, 0);
			else if (!strnicmp(ext, "#audio", 6)) {
				trackID = atoi(ext+6);
				trackID = get_track_id(read->mov, GF_ISOM_MEDIA_AUDIO, trackID);
			}
			else trackID = atoi(ext+1);

			/*if trackID is 0, assume this is a fragment identifier*/
		}
	}

	if (!trackID && (expect_type!=GF_MEDIA_OBJECT_SCENE) && (expect_type!=GF_MEDIA_OBJECT_UNDEF)) {
		for (i=0; i<gf_isom_get_track_count(read->mov); i++) {
			u32 type = gf_isom_get_media_type(read->mov, i+1);
			if (
				((type==GF_ISOM_MEDIA_VISUAL) && (expect_type==GF_MEDIA_OBJECT_VIDEO)) 
				|| ((type==GF_ISOM_MEDIA_AUDIO) && (expect_type==GF_MEDIA_OBJECT_AUDIO)) ) {
				trackID = gf_isom_get_track_id(read->mov, i+1);
				break;
			}

		}
	}
	if (trackID && (expect_type!=GF_MEDIA_OBJECT_SCENE) ) {
		u32 track = gf_isom_get_track_by_id(read->mov, trackID);
		if (!track) return NULL;
		esd = gf_media_map_esd(read->mov, track);
		esd->OCRESID = 0;
		iod = (GF_InitialObjectDescriptor *) gf_isom_get_root_od(read->mov);
		if (!iod) {
			iod = (GF_InitialObjectDescriptor *) gf_odf_desc_new(GF_ODF_IOD_TAG);
			iod->OD_profileAndLevel = iod->audio_profileAndLevel = iod->graphics_profileAndLevel = iod->scene_profileAndLevel = iod->visual_profileAndLevel = 0xFE;
		} else {
			while (gf_list_count(iod->ESDescriptors)) {
				GF_ESD *old = (GF_ESD *)gf_list_get(iod->ESDescriptors, 0);
				gf_odf_desc_del((GF_Descriptor *) old);
				gf_list_rem(iod->ESDescriptors, 0);
			}
		}
		gf_list_add(iod->ESDescriptors, esd);
		isor_emulate_chapters(read->mov, iod);
		return (GF_Descriptor *) iod;
	}

	iod = NULL;
	if (check_mpeg4_systems(plug, read->mov)) {
		iod = (GF_InitialObjectDescriptor *) gf_isom_get_root_od(read->mov);
		if (!iod) {
#ifndef GPAC_DISABLE_LOG
			GF_Err e = gf_isom_last_error(read->mov);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[IsoMedia] Cannot fetch MPEG-4 IOD (error %s) - generating one\n", gf_error_to_string(e) ));
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[IsoMedia] No MPEG-4 IOD found in file - generating one\n"));
			}
#endif
		}
	}
	if (!iod) return isor_emulate_iod(read);

	count = gf_list_count(iod->ESDescriptors);
	if (!count) {
		gf_odf_desc_del((GF_Descriptor*) iod);
		return isor_emulate_iod(read);
	}
	if (count==1) {
		esd = (GF_ESD *)gf_list_get(iod->ESDescriptors, 0);
		switch (esd->decoderConfig->streamType) {
		case GF_STREAM_SCENE:
		case GF_STREAM_PRIVATE_SCENE:
			break;
		case GF_STREAM_VISUAL:
			if (expect_type!=GF_MEDIA_OBJECT_VIDEO) {
				gf_odf_desc_del((GF_Descriptor*) iod);
				return isor_emulate_iod(read);
			}
			break;
		case GF_STREAM_AUDIO:
			/*we need a fake scene graph*/
			if (expect_type!=GF_MEDIA_OBJECT_AUDIO) {
				gf_odf_desc_del((GF_Descriptor*) iod);
				return isor_emulate_iod(read);
			}
			break;
		default:
			gf_odf_desc_del((GF_Descriptor*) iod);
			return NULL;
		}
	}
	/*check IOD is not badly formed (eg mixing audio, video or text streams)*/
	nb_st = 0;
	for (i=0; i<count; i++) {
		esd = (GF_ESD *)gf_list_get(iod->ESDescriptors, i);
		switch (esd->decoderConfig->streamType) {
		case GF_STREAM_VISUAL: nb_st |= 1; break;
		case GF_STREAM_AUDIO: nb_st |= 2; break;
		case GF_STREAM_TEXT: nb_st |= 4; break;
		}
	}
	if ( (nb_st & 1) + (nb_st & 2) + (nb_st & 4) > 1) {
		gf_odf_desc_del((GF_Descriptor*) iod);
		return isor_emulate_iod(read);
	}

	isor_emulate_chapters(read->mov, iod);
	return (GF_Descriptor *) iod;
}


void isor_send_cenc_config(ISOMChannel *ch)
{
	GF_NetworkCommand com;
	u32 i;
	
	memset(&com, 0, sizeof(GF_NetworkCommand));
	com.base.on_channel = ch->channel;
	com.command_type = GF_NET_CHAN_DRM_CFG;
	ch->is_encrypted = GF_TRUE;
	
	gf_isom_get_cenc_info(ch->owner->mov, ch->track, 1, NULL, &com.drm_cfg.scheme_type, &com.drm_cfg.scheme_version, NULL);

	com.drm_cfg.PSSH_count = gf_isom_get_pssh_count(ch->owner->mov);
	com.drm_cfg.PSSHs = gf_malloc(sizeof(GF_NetComDRMConfigPSSH)*(com.drm_cfg.PSSH_count) );

	/*fill PSSH in the structure. We will free it in CENC_Setup*/
	for (i=0; i<com.drm_cfg.PSSH_count; i++) {
		GF_NetComDRMConfigPSSH *pssh = &com.drm_cfg.PSSHs[i];
		gf_isom_get_pssh_info(ch->owner->mov, i+1, pssh->SystemID, &pssh->KID_count, (const bin128 **) & pssh->KIDs, (const u8 **) &pssh->private_data, &pssh->private_data_size);
	}
	//fixme - check MSE and EME
#if 0
	if (read->input->query_proxy && read->input->proxy_udta) {
        read->input->query_proxy(read->input, &com);
    } else 
#endif
		gf_term_on_command(ch->owner->service, &com, GF_OK);
	//free our PSSH
	if (com.drm_cfg.PSSHs) gf_free(com.drm_cfg.PSSHs);
}



GF_Err ISOR_ConnectChannel(GF_InputService *plug, LPNETCHANNEL channel, const char *url, Bool upstream)
{
	u32 ESID;
	ISOMChannel *ch;
	GF_NetworkCommand com;
	u32 track;
	Bool is_esd_url;
	GF_Err e;
	ISOMReader *read;
	if (!plug || !plug->priv) return GF_SERVICE_ERROR;
	read = (ISOMReader *) plug->priv;

	track = 0;
	ch = NULL;
	is_esd_url = GF_FALSE;
	e = GF_OK;
	if (upstream) {
		e = GF_ISOM_INVALID_FILE;
		goto exit;
	}
	if (!read->mov) return GF_SERVICE_ERROR;

	if (strstr(url, "ES_ID")) {
		sscanf(url, "ES_ID=%ud", &ESID);
	} else {
		/*handle url like mypath/myfile.mp4#trackID*/
		char *track_id = (char *)strrchr(url, '.');
		if (track_id) {
			track_id = (char *)strchr(url, '#');
			if (track_id) track_id ++;
		}
		is_esd_url = GF_TRUE;

		ESID = 0;
		/*if only one track ok*/
		if (gf_isom_get_track_count(read->mov)==1) ESID = gf_isom_get_track_id(read->mov, 1);
		else if (track_id) {
			ESID = atoi(track_id);
			track = gf_isom_get_track_by_id(read->mov, (u32) ESID);
			if (!track) ESID = 0;
		}

	}
	if (!ESID) {
		e = GF_NOT_SUPPORTED;
		goto exit;
	}

	/*a channel cannot be open twice, it has to be closed before - NOTE a track is NOT a channel and the user can open
	several times the same track as long as a dedicated channel is used*/
	ch = isor_get_channel(read, channel);
	if (ch) {
		e = GF_SERVICE_ERROR;
		goto exit;
	}
	track = gf_isom_get_track_by_id(read->mov, (u32) ESID);
	if (!track) {
		e = GF_STREAM_NOT_FOUND;
		goto exit;
	}

	GF_SAFEALLOC(ch, ISOMChannel);
	ch->owner = read;
	ch->channel = channel;
	gf_list_add(read->channels, ch);
	ch->track = track;
	ch->track_id = gf_isom_get_track_id(read->mov, ch->track);
	switch (gf_isom_get_media_type(ch->owner->mov, ch->track)) {
	case GF_ISOM_MEDIA_OCR:
		ch->streamType = GF_STREAM_OCR;
		break;
	case GF_ISOM_MEDIA_SCENE:
		ch->streamType = GF_STREAM_SCENE;
		break;
	case GF_ISOM_MEDIA_VISUAL:
		gf_isom_get_reference(ch->owner->mov, ch->track, GF_ISOM_REF_BASE, 1, &ch->base_track);
		ch->next_track = 0;
		/*in scalable mode add SPS/PPS in-band*/
		ch->nalu_extract_mode = GF_ISOM_NALU_EXTRACT_INBAND_PS_FLAG /*| GF_ISOM_NALU_EXTRACT_ANNEXB_FLAG*/;
		gf_isom_set_nalu_extract_mode(ch->owner->mov, ch->track, ch->nalu_extract_mode);
		break;
	}

	ch->has_edit_list = gf_isom_get_edit_list_type(ch->owner->mov, ch->track, &ch->dts_offset) ? GF_TRUE : GF_FALSE;
	ch->has_rap = (gf_isom_has_sync_points(ch->owner->mov, ch->track)==1) ? GF_TRUE : GF_FALSE;
	ch->time_scale = gf_isom_get_media_timescale(ch->owner->mov, ch->track);

exit:
    if (read->input->query_proxy && read->input->proxy_udta && read->input->proxy_type) {
        send_proxy_command(read, GF_FALSE, GF_FALSE, e, NULL, channel);
    } else {
        gf_term_on_connect(read->service, channel, e);
    }
	/*if esd url reconfig SL layer*/
	if (!e && is_esd_url) {
		GF_ESD *esd;
		memset(&com, 0, sizeof(GF_NetworkCommand));
		com.base.on_channel = channel;
		com.command_type = GF_NET_CHAN_RECONFIG;
		esd = gf_isom_get_esd(read->mov, ch->track, 1);
		if (esd) {
			memcpy(&com.cfg.sl_config, esd->slConfig, sizeof(GF_SLConfig));
			gf_odf_desc_del((GF_Descriptor *)esd);
		} else {
			com.cfg.sl_config.tag = GF_ODF_SLC_TAG;
			com.cfg.sl_config.timestampLength = 32;
			com.cfg.sl_config.timestampResolution = ch->time_scale;
			com.cfg.sl_config.useRandomAccessPointFlag = 1;
		}
        if (read->input->query_proxy && read->input->proxy_udta) {
            read->input->query_proxy(read->input, &com);
        } else {
            gf_term_on_command(read->service, &com, GF_OK);
        }
	}
	if (!e && track && gf_isom_is_track_encrypted(read->mov, track)) {
		memset(&com, 0, sizeof(GF_NetworkCommand));
		com.base.on_channel = channel;
		com.command_type = GF_NET_CHAN_DRM_CFG;
		ch->is_encrypted = GF_TRUE;
		if (gf_isom_is_ismacryp_media(read->mov, track, 1)) {
			gf_isom_get_ismacryp_info(read->mov, track, 1, NULL, &com.drm_cfg.scheme_type, &com.drm_cfg.scheme_version, &com.drm_cfg.scheme_uri, &com.drm_cfg.kms_uri, NULL, NULL, NULL);
            if (read->input->query_proxy && read->input->proxy_udta) {
                read->input->query_proxy(read->input, &com);
            } else {
                gf_term_on_command(read->service, &com, GF_OK);
            }
		} else if (gf_isom_is_omadrm_media(read->mov, track, 1)) {
			gf_isom_get_omadrm_info(read->mov, track, 1, NULL, &com.drm_cfg.scheme_type, &com.drm_cfg.scheme_version, &com.drm_cfg.contentID, &com.drm_cfg.kms_uri, &com.drm_cfg.oma_drm_textual_headers, &com.drm_cfg.oma_drm_textual_headers_len, NULL, &com.drm_cfg.oma_drm_crypt_type, NULL, NULL, NULL);

			gf_media_get_file_hash(gf_isom_get_filename(read->mov), com.drm_cfg.hash);
            if (read->input->query_proxy && read->input->proxy_udta) {
                read->input->query_proxy(read->input, &com);
            } else {
                gf_term_on_command(read->service, &com, GF_OK);
            }
		} else if (gf_isom_is_cenc_media(read->mov, track, 1)) {
			ch->is_cenc = GF_TRUE;
			isor_send_cenc_config(ch);
		}
	}
	return e;
}

GF_Err ISOR_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
	ISOMChannel *ch;
	GF_Err e;
	ISOMReader *read;
	if (!plug || !plug->priv) return GF_SERVICE_ERROR;
	read = (ISOMReader *) plug->priv;
	if (!read->mov) return GF_SERVICE_ERROR;

	e = GF_OK;
	ch = isor_get_channel(read, channel);
	assert(ch);
	if (!ch) {
		e = GF_STREAM_NOT_FOUND;
		goto exit;
	}
	/*signal the service is broken but still process the delete*/
	isor_delete_channel(read, ch);
	assert(!isor_get_channel(read, channel));

exit:
    if (read->input->query_proxy && read->input->proxy_udta && read->input->proxy_type) {
        send_proxy_command(read, 1, 0, e, NULL, channel);
    } else {
        gf_term_on_disconnect(read->service, channel, e);
    }
	return e;
}

GF_Err ISOR_ChannelGetSLP(GF_InputService *plug, LPNETCHANNEL channel, char **out_data_ptr, u32 *out_data_size, GF_SLHeader *out_sl_hdr, Bool *sl_compressed, GF_Err *out_reception_status, Bool *is_new_data)
{
	ISOMChannel *ch;
	ISOMReader *read;
	if (!plug || !plug->priv) return GF_SERVICE_ERROR;
	/*cannot read native SL-PDUs*/
	if (!out_sl_hdr) return GF_NOT_SUPPORTED;
	read = (ISOMReader *) plug->priv;
	if (!read->mov) return GF_SERVICE_ERROR;

	*out_data_ptr = NULL;
	*out_data_size = 0;
	*sl_compressed = GF_FALSE;
	*out_reception_status = GF_OK;
	*is_new_data = GF_FALSE;
	ch = isor_get_channel(read, channel);
	if (!ch) return GF_STREAM_NOT_FOUND;
	if (!ch->is_playing) return GF_OK;

	*is_new_data = GF_FALSE;
	if (!ch->sample) {
		/*get sample*/
		gf_mx_p(read->segment_mutex);
		isor_reader_get_sample(ch);
		gf_mx_v(read->segment_mutex);
		*is_new_data = ch->sample ? GF_TRUE : GF_FALSE;
	}

	if (ch->sample) {
		*out_data_ptr = ch->sample->data;
		*out_data_size = ch->sample->dataLength;
		*out_sl_hdr = ch->current_slh;
	}
	*out_reception_status = ch->last_state;
	if (read->waiting_for_data)
		*out_reception_status = GF_BUFFER_TOO_SMALL;
	
	return GF_OK;
}

GF_Err ISOR_ChannelReleaseSLP(GF_InputService *plug, LPNETCHANNEL channel)
{
	ISOMChannel *ch;
	ISOMReader *read;
	if (!plug || !plug->priv) return GF_SERVICE_ERROR;
	read = (ISOMReader *) plug->priv;
	if (!read->mov) return GF_SERVICE_ERROR;
	ch = isor_get_channel(read, channel);
	if (!ch) return GF_STREAM_NOT_FOUND;
	if (!ch->is_playing) return GF_SERVICE_ERROR;

	if (ch->sample) {
		isor_reader_release_sample(ch);
		/*release sample*/
	}
	return GF_OK;
}

static u64 check_round(ISOMChannel *ch, u64 val_ts, Double val_range, Bool make_greater)
{
	Double round_check = (Double) (s64) val_ts;
	round_check /= ch->time_scale;
//	if (round_check != val_range) val_ts += make_greater ? 1 : -1;
	return val_ts;
}

/*switch channel quality. Return next channel or current channel if error*/
static
u32 gf_channel_switch_quality(ISOMChannel *ch, GF_ISOFile *the_file, Bool switch_up)
{
	u32 i, count, next_track, trackID, cur_track;
	s32 ref_count;

	cur_track = ch->next_track ? ch->next_track : ch->track;
	count = gf_isom_get_track_count(the_file);
	trackID = gf_isom_get_track_id(the_file, cur_track);
	next_track = 0;

	if (switch_up)
	{
		for (i = 0; i < count; i++)
		{
			ref_count = gf_isom_get_reference_count(the_file, i+1, GF_ISOM_REF_SCAL);
			if (ref_count < 0)
				return cur_track; //error
			else if (ref_count == 0)
				continue;
			/*next track is the one that has the last reference of type GF_ISOM_REF_SCAL refers to this current track*/
			else if ((u32)ref_count == gf_isom_has_track_reference(the_file, i+1, GF_ISOM_REF_SCAL, trackID))
			{
				next_track = i+1;
				break;
			}
		}
		/*this is the highest quality*/ 
		if (!next_track)
			return cur_track;
	}
	else
	{
		if (cur_track == ch->base_track)
			return cur_track;
		ref_count = gf_isom_get_reference_count(the_file, cur_track, GF_ISOM_REF_SCAL);
		if (ref_count < 0)
			return cur_track;
		gf_isom_get_reference(the_file, cur_track, GF_ISOM_REF_SCAL, ref_count, &next_track);
		if (!next_track)
			return cur_track;
	}

	/*in scalable mode add SPS/PPS in-band*/
	gf_isom_set_nalu_extract_mode(the_file, next_track, ch->nalu_extract_mode);

	return next_track;
}


GF_Err ISOR_ServiceCommand(GF_InputService *plug, GF_NetworkCommand *com)
{
	Double track_dur, media_dur;
	ISOMChannel *ch;
	ISOMReader *read;
	u32 count, i;

	if (!plug || !plug->priv || !com) return GF_SERVICE_ERROR;
	read = (ISOMReader *) plug->priv;
	if (read->disconnected) return GF_OK;

	if (com->command_type==GF_NET_SERVICE_INFO) {
		u32 tag_len;
		const char *tag;
		if (gf_isom_apple_get_tag(read->mov, GF_ISOM_ITUNE_NAME, &tag, &tag_len)==GF_OK) com->info.name = tag;
		if (gf_isom_apple_get_tag(read->mov, GF_ISOM_ITUNE_ARTIST, &tag, &tag_len)==GF_OK) com->info.artist = tag;
		if (gf_isom_apple_get_tag(read->mov, GF_ISOM_ITUNE_ALBUM, &tag, &tag_len)==GF_OK) com->info.album = tag;
		if (gf_isom_apple_get_tag(read->mov, GF_ISOM_ITUNE_COMMENT, &tag, &tag_len)==GF_OK) com->info.comment = tag;
		if (gf_isom_apple_get_tag(read->mov, GF_ISOM_ITUNE_TRACK, &tag, &tag_len)==GF_OK) {
			com->info.track_info = (((tag[2]<<8)|tag[3]) << 16) | ((tag[4]<<8)|tag[5]);
		}
		if (gf_isom_apple_get_tag(read->mov, GF_ISOM_ITUNE_COMPOSER, &tag, &tag_len)==GF_OK) com->info.composer = tag;
		if (gf_isom_apple_get_tag(read->mov, GF_ISOM_ITUNE_WRITER, &tag, &tag_len)==GF_OK) com->info.writer = tag;
		if (gf_isom_apple_get_tag(read->mov, GF_ISOM_ITUNE_GENRE, &tag, &tag_len)==GF_OK) {
			if (tag[0]) {
				com->info.genre = 0;
			} else {
				com->info.genre = (tag[0]<<8) | tag[1];
			}
		}
		return GF_OK;
	}
	if (com->command_type==GF_NET_SERVICE_HAS_AUDIO) {
		u32 i, count;
		count = gf_isom_get_track_count(read->mov);
		for (i=0; i<count; i++) {
			if (gf_isom_get_media_type(read->mov, i+1) == GF_ISOM_MEDIA_AUDIO) return GF_OK;
		}
		return GF_NOT_SUPPORTED;
	}

	if (com->command_type == GF_NET_SERVICE_QUALITY_SWITCH)
	{
		count = gf_list_count(read->channels);
		for (i = 0; i < count; i++)
		{
			ch = (ISOMChannel *)gf_list_get(read->channels, i);
			if (gf_isom_has_scalable_layer(read->mov)) {
				ch->next_track = gf_channel_switch_quality(ch, read->mov, com->switch_quality.up);
			}
		}
		return GF_OK;
	}
	if (com->command_type == GF_NET_SERVICE_PROXY_DATA_RECEIVE) {
		isor_flush_data(read, 1, com->proxy_data.is_chunk);
		return GF_OK;
	}
	if (com->command_type == GF_NET_SERVICE_FLUSH_DATA) {
		if (plug->query_proxy)
			isor_flush_data(read, 0, 0);
		return GF_OK;
	}

	if (!com->base.on_channel) return GF_NOT_SUPPORTED;

	ch = isor_get_channel(read, com->base.on_channel);
	if (!ch) return GF_STREAM_NOT_FOUND;

	switch (com->command_type) {
	case GF_NET_CHAN_SET_PADDING:
		if (!ch->track) return GF_OK;
		gf_isom_set_sample_padding(read->mov, ch->track, com->pad.padding_bytes);
		return GF_OK;
	case GF_NET_CHAN_SET_PULL:
		//we don't pull in DASH base services, we flush as soon as we have a complete segment
#ifndef DASH_USE_PULL
		if (read->input->proxy_udta && !read->input->proxy_type) 
			return GF_NOT_SUPPORTED;
#endif

		ch->is_pulling = 1;
		return GF_OK;
	case GF_NET_CHAN_INTERACTIVE:
		return GF_OK;
	case GF_NET_CHAN_BUFFER:
		//dash or HTTP, do rebuffer if not disabled
		if (plug->query_proxy) {
		} else if (read->dnload) {
			ch->buffer_min = com->buffer.min;
			ch->buffer_max = com->buffer.max;
		} else {
			com->buffer.max = com->buffer.min = 0;
		}
		return GF_OK;
	case GF_NET_CHAN_DURATION:
		if (!ch->track) {
			com->duration.duration = 0;
			return GF_OK;
		}
		ch->duration = gf_isom_get_track_duration(read->mov, ch->track);
		track_dur = (Double) (s64) ch->duration;
		track_dur /= read->time_scale;
		if (gf_isom_get_edit_segment_count(read->mov, ch->track)) {
			com->duration.duration = (Double) track_dur;
			ch->duration = (u32) (track_dur * ch->time_scale);
		} else {
			/*some file indicate a wrong TrackDuration, get the longest*/
			ch->duration = gf_isom_get_media_duration(read->mov, ch->track);
			media_dur = (Double) (s64) ch->duration;
			media_dur /= ch->time_scale;
			com->duration.duration = MAX(track_dur, media_dur);
		}
		return GF_OK;
	case GF_NET_CHAN_PLAY:

		isor_reset_reader(ch);
		ch->speed = com->play.speed;
		ch->start = ch->end = 0;
		if (com->play.speed>0) {
			if (com->play.start_range>=0) {
				ch->start = (u64) (s64) (com->play.start_range * ch->time_scale);
				ch->start = check_round(ch, ch->start, com->play.start_range, 1);
			}
			if (com->play.end_range >= com->play.start_range) {
				ch->end = (u64) (s64) (com->play.end_range*ch->time_scale);
				ch->end = check_round(ch, ch->end, com->play.end_range, 0);
			}
		} else if (com->play.speed<0) {
			if (com->play.end_range>=com->play.start_range) ch->start = (u64) (s64) (com->play.start_range * ch->time_scale);
			if (com->play.end_range >= 0) ch->end = (u64) (s64) (com->play.end_range*ch->time_scale);
		}
		ch->is_playing = 1;
		if (com->play.dash_segment_switch) ch->wait_for_segment_switch = 1;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[IsoMedia] Starting channel playback "LLD" to "LLD" (%g to %g)\n", ch->start, ch->end, com->play.start_range, com->play.end_range));

		//and check buffer level on play request
		isor_check_buffer_level(read);
		return GF_OK;
	case GF_NET_CHAN_STOP:
		isor_reset_reader(ch);
		return GF_OK;

	/*nothing to do on MP4 for channel config*/
	case GF_NET_CHAN_CONFIG:
		return GF_OK;
	case GF_NET_CHAN_GET_PIXEL_AR:
		return gf_isom_get_pixel_aspect_ratio(read->mov, ch->track, 1, &com->par.hSpacing, &com->par.vSpacing);
	case GF_NET_CHAN_GET_DSI:
	{
		/*it may happen that there are conflicting config when using ESD URLs...*/
		GF_DecoderConfig *dcd = gf_isom_get_decoder_config(read->mov, ch->track, 1);
		com->get_dsi.dsi = NULL;
		com->get_dsi.dsi_len = 0;
		if (dcd) {
			if (dcd->decoderSpecificInfo) {
				com->get_dsi.dsi = dcd->decoderSpecificInfo->data;
				com->get_dsi.dsi_len = dcd->decoderSpecificInfo->dataLength;
				dcd->decoderSpecificInfo->data = NULL;
			}
			gf_odf_desc_del((GF_Descriptor *) dcd);
		}
		return GF_OK;
	}
	case GF_NET_CHAN_NALU_MODE:
		ch->nalu_extract_mode = GF_ISOM_NALU_EXTRACT_INBAND_PS_FLAG;
		//when this is set, we work in real scalable (eg N streams reassembled by the player) so only extract the layer. This wll need refinements if we plan to support
		//several scalable layers ...
		if (com->nalu_mode.extract_mode==1) ch->nalu_extract_mode |= GF_ISOM_NALU_EXTRACT_ANNEXB_FLAG | GF_ISOM_NALU_EXTRACT_VDRD_FLAG | GF_ISOM_NALU_EXTRACT_LAYER_ONLY;
		gf_isom_set_nalu_extract_mode(ch->owner->mov, ch->track, ch->nalu_extract_mode);
		break;
	default:
		break;
	}
	return GF_NOT_SUPPORTED;
}

static Bool ISOR_CanHandleURLInService(GF_InputService *plug, const char *url)
{
	char szURL[2048], *sep;
	ISOMReader *read = (ISOMReader *)plug->priv;
	const char *this_url = gf_term_get_service_url(read->service);
	if (!this_url || !url) return GF_FALSE;

	if (!strcmp(this_url, url)) return GF_TRUE;

	strcpy(szURL, this_url);
	sep = strrchr(szURL, '#');
	if (sep) sep[0] = 0;

	/*direct addressing in service*/
	if (url[0] == '#') return GF_TRUE;
	if (strnicmp(szURL, url, sizeof(char)*strlen(szURL))) return GF_FALSE;
	return GF_TRUE;
}

GF_InputService *isor_client_load()
{
	ISOMReader *reader;
	GF_InputService *plug;
	GF_SAFEALLOC(plug, GF_InputService);
	GF_REGISTER_MODULE_INTERFACE(plug, GF_NET_CLIENT_INTERFACE, "GPAC IsoMedia Reader", "gpac distribution")
	plug->RegisterMimeTypes = ISOR_RegisterMimeTypes;
	plug->CanHandleURL = ISOR_CanHandleURL;
	plug->ConnectService = ISOR_ConnectService;
	plug->CloseService = ISOR_CloseService;
	plug->GetServiceDescriptor = ISOR_GetServiceDesc;
	plug->ConnectChannel = ISOR_ConnectChannel;
	plug->DisconnectChannel = ISOR_DisconnectChannel;
	plug->ServiceCommand = ISOR_ServiceCommand;
	plug->CanHandleURLInService = ISOR_CanHandleURLInService;
	/*we do support pull mode*/
	plug->ChannelGetSLP = ISOR_ChannelGetSLP;
	plug->ChannelReleaseSLP = ISOR_ChannelReleaseSLP;

	GF_SAFEALLOC(reader, ISOMReader);
	reader->channels = gf_list_new();
	reader->segment_mutex = gf_mx_new("ISO Segment");
	
	plug->priv = reader;
	return plug;
}

void isor_client_del(GF_BaseInterface *bi)
{
	GF_InputService *plug = (GF_InputService *) bi;
	ISOMReader *read = (ISOMReader *)plug->priv;

	if (read->segment_mutex) gf_mx_del(read->segment_mutex);
	gf_list_del(read->channels);
	gf_free(read);
	gf_free(bi);
}

#endif /*GPAC_DISABLE_ISOM*/
