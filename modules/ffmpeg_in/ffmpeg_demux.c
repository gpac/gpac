/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
 *					All rights reserved
 *
 *  This file is part of GPAC / FFMPEG module
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

#include "ffmpeg_in.h"

#ifdef AVSEEK_FLAG_BACKWARD
#define my_av_seek_frame(a, b, c, d) av_seek_frame(a, b, c, AVSEEK_FLAG_BACKWARD)
#else
#define my_av_seek_frame(a, b, c, d) av_seek_frame(a, b, c)
#endif

/*default buffer is 200 ms per channel*/
#define FFD_DATA_BUFFER		800

static u32 FFDemux_Run(void *par)
{
	AVPacket pkt;
	s64 seek_to;
	Bool video_init, do_seek, map_audio_time, map_video_time;
	GF_NetworkCommand com;
	GF_NetworkCommand map;
	SLHeader slh;
	FFDemux *ffd = (FFDemux *) par;

	memset(&map, 0, sizeof(GF_NetworkCommand));
	map.command_type = GF_NET_CHAN_MAP_TIME;
	
	memset(&com, 0, sizeof(GF_NetworkCommand));
	com.command_type = GF_NET_CHAN_BUFFER_QUERY;

	memset(&slh, 0, sizeof(SLHeader));

	slh.compositionTimeStampFlag = slh.decodingTimeStampFlag = 1;
	seek_to = (s64) (AV_TIME_BASE*ffd->seek_time);
	map_video_time = !ffd->seekable;

	video_init = (seek_to && ffd->video_ch) ? 0 : 1;

	/*it appears that ffmpeg has trouble resyncing on some mpeg files - we trick it by restarting to 0 to get the 
	first video frame, and only then seek*/
	if (ffd->seekable) my_av_seek_frame(ffd->ctx, -1, video_init ? seek_to : 0, 0);
	do_seek = !video_init;
	map_audio_time = video_init ? ffd->unreliable_audio_timing : 0;

	while (ffd->is_running) {

		pkt.stream_index = -1;
		/*EOF*/
        if (av_read_frame(ffd->ctx, &pkt) <0) break;
		if (pkt.pts == AV_NOPTS_VALUE) pkt.pts = pkt.dts;

		slh.compositionTimeStamp = pkt.pts;
		slh.decodingTimeStamp = pkt.dts;

		gf_mx_p(ffd->mx);
		/*blindly send audio as soon as video is init*/
		if (ffd->audio_ch && (pkt.stream_index == ffd->audio_st) && !do_seek) {
			if (map_audio_time) {
				map.base.on_channel = ffd->audio_ch;
				map.map_time.media_time = ffd->seek_time;
				/*mapwith TS=0 since we don't use SL*/
				map.map_time.timestamp = 0;
				map.map_time.reset_buffers = 1;
				map_audio_time = 0;
				gf_term_on_command(ffd->service, &map, GF_OK);
			}
			/*seeking*/
			if (slh.compositionTimeStamp < (u64) seek_to) {
				slh.compositionTimeStamp = slh.decodingTimeStamp = seek_to;
			}
			gf_term_on_sl_packet(ffd->service, ffd->audio_ch, pkt.data, pkt.size, &slh, GF_OK);
		} 
		else if (ffd->video_ch && (pkt.stream_index == ffd->video_st)) {
			/*if we get pts = 0 after a seek the demuxer is reseting PTSs, so force map time*/
			if ((!do_seek && seek_to && !slh.compositionTimeStamp) || (map_video_time) ) {
				seek_to = 0;
				map_video_time = 0;

				map.base.on_channel = ffd->video_ch;
				map.map_time.timestamp = (u64) pkt.pts;
				map.map_time.media_time = ffd->seek_time;
				map.map_time.reset_buffers = 0;
				gf_term_on_command(ffd->service, &map, GF_OK);
			} else if (slh.compositionTimeStamp < (u64) seek_to) {
				slh.compositionTimeStamp = slh.decodingTimeStamp = seek_to;
			}
			gf_term_on_sl_packet(ffd->service, ffd->video_ch, pkt.data, pkt.size, &slh, GF_OK);
			video_init = 1;
		}
		gf_mx_v(ffd->mx);
		av_free_packet(&pkt);

		/*here's the trick - only seek after sending the first packets of each stream - this allows ffmpeg video decoders
		to resync properly*/
		if (do_seek && video_init && ffd->seekable) {
			my_av_seek_frame(ffd->ctx, -1, seek_to, 0);
			do_seek = 0;
			map_audio_time = ffd->unreliable_audio_timing;
		}
		/*sleep untill the buffer occupancy is too low - note that this work because all streams in this
		demuxer are synchronized*/
		while (1) {
			if (ffd->audio_ch) {
				com.base.on_channel = ffd->audio_ch;
				gf_term_on_command(ffd->service, &com, GF_OK);
				if (com.buffer.occupancy < ffd->data_buffer_ms) break;
			}
			if (ffd->video_ch) {
				com.base.on_channel = ffd->video_ch;
				gf_term_on_command(ffd->service, &com, GF_OK);
				if (com.buffer.occupancy < ffd->data_buffer_ms) break;
			}
			gf_sleep(10);
			
			/*escape if disconnect*/
			if (!ffd->audio_run && !ffd->video_run) break;
		}
		if (!ffd->audio_run && !ffd->video_run) break;
	}
	/*signal EOS*/
	if (ffd->audio_ch) gf_term_on_sl_packet(ffd->service, ffd->audio_ch, NULL, 0, NULL, GF_EOS);
	if (ffd->video_ch) gf_term_on_sl_packet(ffd->service, ffd->video_ch, NULL, 0, NULL, GF_EOS);
	ffd->is_running = 0;

	return 0;
}

/*some recent changes in ffmpeg AVStream - not sure about the lib version the change happened*/
#if (LIBAVCODEC_BUILD<4756) 
#define FFD_GET_AVCODECCONTEXT(__ctx, __stID) &__ctx->streams[__stID]->codec;
#else
#define FFD_GET_AVCODECCONTEXT(__ctx, __stID) __ctx->streams[__stID]->codec;
#endif

static Bool FFD_CanHandleURL(GF_InputService *plug, const char *url)
{
	Bool has_audio, has_video;
	s32 i;
	AVFormatContext *ctx;
	AVOutputFormat *fmt_out;
	Bool ret = 0;
	char *ext, szName[1000], szExt[20];
	const char *szExtList;

	strcpy(szName, url);
	ext = strrchr(szName, '#');
	if (ext) ext[0] = 0;

	/*disable RTP/RTSP from ffmpeg*/
	if (!strnicmp(szName, "rtsp://", 7)) return 0;
	if (!strnicmp(szName, "rtspu://", 8)) return 0;
	if (!strnicmp(szName, "rtp://", 6)) return 0;

	ext = strrchr(szName, '.');
	if (ext) {
		strcpy(szExt, &ext[1]);
		strlwr(szExt);
		/*note we forbid ffmpeg to handle files we support*/
		if (!strcmp(szExt, "mp4") || !strcmp(szExt, "mpg4") || !strcmp(szExt, "m4a") 
			|| !strcmp(szExt, "3gp") || !strcmp(szExt, "3gpp") || !strcmp(szExt, "3gp2") || !strcmp(szExt, "3g2") 
			|| !strcmp(szExt, "mp3") 
			|| !strcmp(szExt, "amr") 
			|| !strcmp(szExt, "bt") || !strcmp(szExt, "wrl") || !strcmp(szExt, "x3dv") 
			|| !strcmp(szExt, "xmt") || !strcmp(szExt, "xmta") || !strcmp(szExt, "x3d") 
			) return 0;

		/*check any default stuff that should work with ffmpeg*/
		if (gf_term_check_extension(plug, "video/mpeg", "mpg mpeg mp2 mpa mpe mpv2", "MPEG 1/2 Movies", ext)) return 1;
		if (gf_term_check_extension(plug, "video/x-mpeg", "mpg mpeg mp2 mpa mpe mpv2", "MPEG 1/2 Movies", ext)) return 1;
		if (gf_term_check_extension(plug, "video/x-mpeg-systems", "mpg mpeg mp2 mpa mpe mpv2", "MPEG 1/2 Movies", ext)) return 1;
		if (gf_term_check_extension(plug, "audio/basic", "snd au", "Basic Audio", ext)) return 1;
		if (gf_term_check_extension(plug, "audio/x-wav", "wav", "WAV Audio", ext)) return 1;
		if (gf_term_check_extension(plug, "video/x-ms-asf", "asf wma wmv asx asr", "WindowsMedia Movies", ext)) return 1;
		if (gf_term_check_extension(plug, "video/x-ms-wmv", "asf wma wmv asx asr", "WindowsMedia Movies", ext)) return 1;
		if (gf_term_check_extension(plug, "video/avi", "avi", "AVI Movies", ext)) return 1;
		/*we let ffmpeg handle mov because some QT files with uncompressed or adpcm audio use 1 audio sample 
		per MP4 sample which is a killer for our MP4 lib, whereas ffmpeg handles these as complete audio chunks 
		moreover ffmpeg handles cmov, we don't*/
		if (gf_term_check_extension(plug, "video/quicktime", "mov qt", "QuickTime Movies", ext)) return 1;
	}

	ctx = NULL;
    if (av_open_input_file(&ctx, szName, NULL, 0, NULL)<0)
		return 0;

    if (!ctx || av_find_stream_info(ctx) <0) goto exit;
	/*figure out if we can use codecs or not*/
	has_video = has_audio = 0;
    for(i = 0; i < ctx->nb_streams; i++) {
        AVCodecContext *enc = FFD_GET_AVCODECCONTEXT(ctx, i);
        switch(enc->codec_type) {
        case CODEC_TYPE_AUDIO:
            if (!has_audio) has_audio = 1;
            break;
        case CODEC_TYPE_VIDEO:
            if (!has_video) has_video= 1;
            break;
        default:
            break;
        }
    }
	if (!has_audio && !has_video) goto exit;
	ret = 1;

	fmt_out = guess_stream_format(NULL, url, NULL);
	if (fmt_out) gf_term_register_mime_type(plug, fmt_out->mime_type, fmt_out->extensions, fmt_out->name);
	else {
		ext = strrchr(szName, '.');
		if (ext) {
			strcpy(szExt, &ext[1]);
			strlwr(szExt);

			szExtList = gf_modules_get_option((GF_BaseInterface *)plug, "MimeTypes", "video/ffmpeg-files");
			if (!szExtList) {
				gf_term_register_mime_type(plug, "video/ffmpeg-files", szExt, "Other Movies (FFMPEG)");
			} else if (!strstr(szExtList, szExt)) {
				u32 len;
				char *buf;
				len = strlen(szExtList) + strlen(szExt) + 1;
				buf = malloc(sizeof(char)*len);
				sprintf(buf, "\"%s ", szExt);
				strcat(buf, &szExtList[1]);
				gf_modules_set_option((GF_BaseInterface *)plug, "MimeTypes", "video/ffmpeg-files", buf);
				free(buf);
			}
		}
	}

exit:
    if (ctx) av_close_input_file(ctx);
	return ret;
}

static GF_Err FFD_ConnectService(GF_InputService *plug, GF_ClientService *serv, const char *url)
{
	GF_Err e;
	s64 last_aud_pts;
	s32 i;
	const char *sOpt;
	FFDemux *ffd = plug->priv;
	char *ext, szName[1000];

	if (ffd->ctx) return GF_SERVICE_ERROR;

	strcpy(szName, url);
	ext = strrchr(szName, '#');
	ffd->service_type = 0;
	e = GF_NOT_SUPPORTED;
	ffd->service = serv;

	if (ext) {
		if (!stricmp(&ext[1], "video")) ffd->service_type = 1;
		else if (!stricmp(&ext[1], "audio")) ffd->service_type = 2;
		ext[0] = 0;
	}
    switch (av_open_input_file(&ffd->ctx, szName, NULL, 0, NULL)) {
	case 0: e = GF_OK; break;
	case AVERROR_IO: e = GF_URL_ERROR; goto err_exit;
	case AVERROR_INVALIDDATA: e = GF_NON_COMPLIANT_BITSTREAM; goto err_exit;
	case AVERROR_NOMEM: e = GF_OUT_OF_MEM; goto err_exit;
	case AVERROR_NOFMT: e = GF_NOT_SUPPORTED; goto err_exit;
	default: e = GF_SERVICE_ERROR; goto err_exit;
	}


    if (av_find_stream_info(ffd->ctx) <0) goto err_exit;
	/*figure out if we can use codecs or not*/
	ffd->audio_st = ffd->video_st = -1;
    for (i = 0; i < ffd->ctx->nb_streams; i++) {
        AVCodecContext *enc = FFD_GET_AVCODECCONTEXT(ffd->ctx, i);
        switch(enc->codec_type) {
        case CODEC_TYPE_AUDIO:
            if (ffd->audio_st<0) ffd->audio_st = i;
            break;
        case CODEC_TYPE_VIDEO:
            if (ffd->video_st<0) ffd->video_st = i;
            break;
        default:
            break;
        }
    }
	if ((ffd->service_type==1) && (ffd->video_st<0)) goto err_exit;
	if ((ffd->service_type==2) && (ffd->audio_st<0)) goto err_exit;
	if ((ffd->video_st<0) && (ffd->audio_st<0)) goto err_exit;

	/*setup indexes for BIFS/OD*/
	ffd->od_es_id = 2 + MAX(ffd->video_st, ffd->audio_st);

	sOpt = gf_modules_get_option((GF_BaseInterface *)plug, "FFMPEG", "DataBufferMS"); 
	ffd->data_buffer_ms = 0;
	if (sOpt) ffd->data_buffer_ms = atoi(sOpt);
	if (!ffd->data_buffer_ms) ffd->data_buffer_ms = FFD_DATA_BUFFER;

	/*check we do have increasing pts. If not we can't rely on pts, we must skip SL
	we assume video pts is always present*/
	last_aud_pts = 0;
	for (i=0; i<20; i++) {
		AVPacket pkt;
		pkt.stream_index = -1;
	    if (av_read_frame(ffd->ctx, &pkt) <0) break;
		if (pkt.pts == AV_NOPTS_VALUE) pkt.pts = pkt.dts;
		if (pkt.stream_index==ffd->audio_st) last_aud_pts = pkt.pts;
	}
	if (last_aud_pts == 0) ffd->unreliable_audio_timing = 1;

	/*build seek*/
	ffd->seekable = (my_av_seek_frame(ffd->ctx, -1, 0, 0)<0) ? 0 : 1;
	if (!ffd->seekable) {
	    av_close_input_file(ffd->ctx);
		av_open_input_file(&ffd->ctx, szName, NULL, 0, NULL);
	    av_find_stream_info(ffd->ctx);
	}

	/*let's go*/
	gf_term_on_connect(serv, NULL, GF_OK);
	return GF_OK;

err_exit:
    if (ffd->ctx) av_close_input_file(ffd->ctx);
	ffd->ctx = NULL;
	gf_term_on_connect(serv, NULL, e);
	return GF_OK;
}


static GF_ESD *FFD_GetESDescriptor(FFDemux *ffd, Bool for_audio)
{
	GF_BitStream *bs;
	Bool dont_use_sl;
	GF_ESD *esd = (GF_ESD *) gf_odf_desc_esd_new(0);
	esd->ESID = 1 + (for_audio ? ffd->audio_st : ffd->video_st);
	esd->decoderConfig->streamType = for_audio ? GF_STREAM_AUDIO : GF_STREAM_VISUAL;
	esd->decoderConfig->avgBitrate = esd->decoderConfig->maxBitrate = 0;

	/*remap std object types - depending on input formats, FFMPEG may not have separate DSI from initial frame. 
	In this case we have no choice but using FFMPEG decoders*/
	if (for_audio) {
	  AVCodecContext *dec = FFD_GET_AVCODECCONTEXT(ffd->ctx, ffd->audio_st);
 		switch (dec->codec_id) {
		case CODEC_ID_MP2:
			esd->decoderConfig->objectTypeIndication = 0x6B;
			break;
		case CODEC_ID_MP3:
			esd->decoderConfig->objectTypeIndication = 0x69;
			break;
		case CODEC_ID_MPEG4AAC:
		case CODEC_ID_AAC:
			if (!dec->extradata_size) goto opaque_audio;
			esd->decoderConfig->objectTypeIndication = 0x40;
			esd->decoderConfig->decoderSpecificInfo->dataLength = dec->extradata_size;
			esd->decoderConfig->decoderSpecificInfo->data = malloc(sizeof(char)*dec->extradata_size);
			memcpy(esd->decoderConfig->decoderSpecificInfo->data, 
					dec->extradata, 
					sizeof(char)*dec->extradata_size);
			break;
		default:
opaque_audio:
			esd->decoderConfig->objectTypeIndication = GPAC_FFMPEG_CODECS_OTI;
			bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			gf_bs_write_u32(bs, dec->codec_id);
			gf_bs_write_u32(bs, dec->sample_rate);
			gf_bs_write_u16(bs, dec->channels);
			gf_bs_write_u16(bs, dec->bits_per_sample);
			gf_bs_write_u16(bs, dec->frame_size);
			gf_bs_write_u16(bs, dec->block_align);

			gf_bs_write_u32(bs, dec->codec_tag);
			gf_bs_write_u32(bs, dec->bit_rate);
			if (dec->extradata_size) {
				gf_bs_write_data(bs, dec->extradata, dec->extradata_size);
			}
			gf_bs_get_content(bs, (unsigned char **) &esd->decoderConfig->decoderSpecificInfo->data, &esd->decoderConfig->decoderSpecificInfo->dataLength);
			gf_bs_del(bs);
			break;
		}
		dont_use_sl = ffd->unreliable_audio_timing;
	} else {
		AVCodecContext *dec = FFD_GET_AVCODECCONTEXT(ffd->ctx, ffd->video_st);
		switch (dec->codec_id) {
		case CODEC_ID_MPEG4:
		case CODEC_ID_H264:
			/*if dsi not detected force use ffmpeg*/
			if (!dec->extradata_size) goto opaque_video;
			/*otherwise use any MPEG-4 Visual*/
			esd->decoderConfig->objectTypeIndication = (dec->codec_id==CODEC_ID_H264) ? 0x21 : 0x20;
			esd->decoderConfig->decoderSpecificInfo->dataLength = dec->extradata_size;
			esd->decoderConfig->decoderSpecificInfo->data = malloc(sizeof(char)*dec->extradata_size);
			memcpy(esd->decoderConfig->decoderSpecificInfo->data, 
					dec->extradata, 
					sizeof(char)*dec->extradata_size);
			break;
		case CODEC_ID_MPEG1VIDEO:
			esd->decoderConfig->objectTypeIndication = 0x6A;
			break;
		case CODEC_ID_MPEG2VIDEO:
			esd->decoderConfig->objectTypeIndication = 0x65;
			break;
		default:
opaque_video:
			esd->decoderConfig->objectTypeIndication = GPAC_FFMPEG_CODECS_OTI;
			bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			gf_bs_write_u32(bs, dec->codec_id);
			gf_bs_write_u32(bs, dec->width);
			gf_bs_write_u32(bs, dec->height);
			gf_bs_write_u32(bs, dec->codec_tag);
			gf_bs_write_u32(bs, dec->bit_rate);

			if (dec->extradata_size) {
				gf_bs_write_data(bs, dec->extradata, dec->extradata_size);
			}
			gf_bs_get_content(bs, (unsigned char **) &esd->decoderConfig->decoderSpecificInfo->data, &esd->decoderConfig->decoderSpecificInfo->dataLength);
			gf_bs_del(bs);
			break;
		}
		dont_use_sl = 0;
	}

	if (dont_use_sl) {
		esd->slConfig->predefined = SLPredef_SkipSL;
	} else {
		/*only send full AUs*/
		esd->slConfig->useAccessUnitStartFlag = esd->slConfig->useAccessUnitEndFlag = 0;
		esd->slConfig->hasRandomAccessUnitsOnlyFlag = 1;
		esd->slConfig->useTimestampsFlag = 1;
		esd->slConfig->timestampResolution = AV_TIME_BASE;
	}

	return esd;
}

static GF_Descriptor *FFD_GetServiceDesc(GF_InputService *plug, u32 expect_type, const char *sub_url)
{
	GF_ObjectDescriptor *od;
	GF_ESD *esd;
	FFDemux *ffd = plug->priv;

	if (!ffd->ctx) return NULL;

	od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
	od->objectDescriptorID = 1;

	/*since we don't handle multitrack in ffmpeg, we don't need to check sub_url, only use expected type*/
	if (expect_type==GF_MEDIA_OBJECT_AUDIO) {
		esd = FFD_GetESDescriptor(ffd, 1);
		/*if session join, setup sync*/
		if (ffd->video_ch) esd->OCRESID = ffd->video_st+1;
		gf_list_add(od->ESDescriptors, esd);
		return (GF_Descriptor *) od;
	}
	if (expect_type==GF_MEDIA_OBJECT_VIDEO) {
		esd = FFD_GetESDescriptor(ffd, 0);
		/*if session join, setup sync*/
		if (ffd->audio_ch) esd->OCRESID = ffd->audio_st+1;
		gf_list_add(od->ESDescriptors, esd);
		return (GF_Descriptor *) od;
	}

	/*setup OD stream (no DSI)*/
	esd = (GF_ESD *) gf_odf_desc_esd_new(0);
	esd->ESID = ffd->od_es_id;
	esd->OCRESID = 0;
	esd->decoderConfig->streamType = GF_STREAM_OD;
	esd->decoderConfig->objectTypeIndication = GPAC_STATIC_OD_OTI;
	
	/*we only send 1 full AU RAP*/
	esd->slConfig->useAccessUnitStartFlag = esd->slConfig->useAccessUnitEndFlag = 0;
	esd->slConfig->hasRandomAccessUnitsOnlyFlag = 1;
	esd->slConfig->useTimestampsFlag = 1;
	esd->slConfig->timestampResolution = 1000;
	gf_odf_desc_add_desc((GF_Descriptor *)od, (GF_Descriptor *) esd);
	return (GF_Descriptor *)od;
}


static GF_Err FFD_CloseService(GF_InputService *plug)
{
	FFDemux *ffd = plug->priv;

	ffd->is_running = 0;

	if (ffd->ctx) av_close_input_file(ffd->ctx);
	ffd->ctx = NULL;
	ffd->audio_ch = ffd->video_ch = ffd->od_ch = NULL;
	ffd->audio_run = ffd->video_run = 0;

	gf_term_on_disconnect(ffd->service, NULL, GF_OK);
	return GF_OK;
}

static GF_Err FFD_ConnectChannel(GF_InputService *plug, LPNETCHANNEL channel, const char *url, Bool upstream)
{
	GF_Err e;
	u32 ESID;
	FFDemux *ffd = plug->priv;

	e = GF_STREAM_NOT_FOUND;
	if (upstream) {
		e = GF_ISOM_INVALID_FILE;
		goto exit;
	}
	if (!strstr(url, "ES_ID=")) {
		e = GF_NOT_SUPPORTED;
		goto exit;
	}
	sscanf(url, "ES_ID=%d", &ESID);

	if (ESID==ffd->od_es_id) {
		if (ffd->od_ch) {
			e = GF_SERVICE_ERROR;
			goto exit;
		}
		ffd->od_ch = channel;
		e = GF_OK;
	}
	else if ((s32) ESID == 1 + ffd->audio_st) {
		if (ffd->audio_ch) {
			e = GF_SERVICE_ERROR;
			goto exit;
		}
		ffd->audio_ch = channel;
		e = GF_OK;
	}
	else if ((s32) ESID == 1 + ffd->video_st) {
		if (ffd->video_ch) {
			e = GF_SERVICE_ERROR;
			goto exit;
		}
		ffd->video_ch = channel;
		e = GF_OK;
	}

exit:
	gf_term_on_connect(ffd->service, channel, e);
	return GF_OK;
}

static GF_Err FFD_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
	GF_Err e;
	FFDemux *ffd = plug->priv;

	e = GF_STREAM_NOT_FOUND;
	if (ffd->od_ch == channel) {
		e = GF_OK;
		ffd->od_ch = NULL;
	}
	else if (ffd->audio_ch == channel) {
		e = GF_OK;
		ffd->audio_ch = NULL;
		ffd->audio_run = 0;
	}
	else if (ffd->video_ch == channel) {
		e = GF_OK;
		ffd->video_ch = NULL;
		ffd->video_run = 0;
	}
	gf_term_on_disconnect(ffd->service, channel, e);
	return GF_OK;
}

static GF_Err FFD_ServiceCommand(GF_InputService *plug, GF_NetworkCommand *com)
{
	FFDemux *ffd = plug->priv;

	if (!com->base.on_channel) return GF_NOT_SUPPORTED;

	switch (com->command_type) {
	/*only BIFS/OD work in pull mode (cf ffmpeg_in.h)*/
	case GF_NET_CHAN_SET_PULL:
		if (ffd->audio_ch==com->base.on_channel) return GF_NOT_SUPPORTED;
		if (ffd->video_ch==com->base.on_channel) return GF_NOT_SUPPORTED;
		return GF_OK;
	case GF_NET_CHAN_INTERACTIVE:
		return ffd->seekable ? GF_OK : GF_NOT_SUPPORTED;
	case GF_NET_CHAN_BUFFER:
		com->buffer.max = com->buffer.min = 0;
		return GF_OK;
	case GF_NET_CHAN_DURATION:
		com->duration.duration = (Double) ffd->ctx->duration / AV_TIME_BASE;
		return GF_OK;
	/*fetch start time*/
	case GF_NET_CHAN_PLAY:
		if (com->play.speed<0) return GF_NOT_SUPPORTED;

		gf_mx_p(ffd->mx);
		ffd->seek_time = (com->play.start_range>=0) ? com->play.start_range : 0;
		
		if (ffd->audio_ch==com->base.on_channel) ffd->audio_run = 1;
		else if (ffd->video_ch==com->base.on_channel) ffd->video_run = 1;

		/*play on media stream, start thread*/
		if ((ffd->audio_ch==com->base.on_channel) || (ffd->video_ch==com->base.on_channel)) {
			if (!ffd->is_running) {
				ffd->is_running = 1;
				gf_th_run(ffd->thread, FFDemux_Run, ffd);
			}
		}
		gf_mx_v(ffd->mx);
		return GF_OK;
	case GF_NET_CHAN_STOP:
		if (ffd->audio_ch==com->base.on_channel) ffd->audio_run = 0;
		else if (ffd->video_ch==com->base.on_channel) ffd->video_run = 0;
		else if (ffd->od_ch==com->base.on_channel) ffd->od_state = 0;
		return GF_OK;
	/*note we don't handle PAUSE/RESUME/SET_SPEED, this is automatically handled by the demuxing thread 
	through buffer occupancy queries*/

	default:
		return GF_OK;
	}

	return GF_OK;
}


GF_Err FFD_ChannelGetSLP(GF_InputService *plug, LPNETCHANNEL channel, char **out_data_ptr, u32 *out_data_size, SLHeader *out_sl_hdr, Bool *sl_compressed, GF_Err *out_reception_status, Bool *is_new_data)
{
	GF_ODCodec *odc;
	GF_ODCom *com;
	GF_ObjectDescriptor *OD;
	GF_ESD *esd;

	FFDemux *ffd = plug->priv;
	if (channel==ffd->audio_ch) return GF_BAD_PARAM;
	if (channel==ffd->video_ch) return GF_BAD_PARAM;

	*sl_compressed = 0;
	memset(out_sl_hdr, 0, sizeof(SLHeader));
	out_sl_hdr->accessUnitEndFlag = 1;
	out_sl_hdr->accessUnitStartFlag = 1;
	out_sl_hdr->compositionTimeStampFlag = 1;
	out_sl_hdr->compositionTimeStamp = (u32) (1000 * ffd->seek_time);

	*out_reception_status = (ffd->od_state == 2) ? GF_EOS : GF_OK;
	if (!ffd->od_au && (ffd->od_state != 2)) {
		/*compute OD AU*/
		com = gf_odf_com_new(GF_ODF_OD_UPDATE_TAG);

		if ((ffd->service_type!=1) && (ffd->audio_st>=0)) {
			OD = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
			esd = FFD_GetESDescriptor(ffd, 1);
			esd->OCRESID = ffd->od_es_id;
			OD->objectDescriptorID = esd->ESID;
			gf_odf_desc_add_desc((GF_Descriptor*)OD, (GF_Descriptor *) esd);
			gf_list_add( ((GF_ODUpdate*)com)->objectDescriptors, OD);

		}
		/*compute video */
		if ((ffd->service_type!=2) && (ffd->video_st>=0)) {
			OD = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
			esd = FFD_GetESDescriptor(ffd, 0);
			esd->OCRESID = ffd->od_es_id;
			OD->objectDescriptorID = esd->ESID;
			gf_odf_desc_add_desc((GF_Descriptor*)OD, (GF_Descriptor *) esd);
			gf_list_add( ((GF_ODUpdate*)com)->objectDescriptors, OD);
		}
		odc = gf_odf_codec_new();
		gf_odf_codec_add_com(odc, com);
		gf_odf_codec_encode(odc);
		gf_odf_codec_get_au(odc, &ffd->od_au, &ffd->od_au_size);
		gf_odf_codec_del(odc);
		*is_new_data = 1;

	} else {
		*is_new_data = 0;
	}
	*out_data_ptr = ffd->od_au;
	*out_data_size = ffd->od_au_size;
	if (!ffd->od_state) ffd->od_state = 1;
	return GF_OK;
}

GF_Err FFD_ChannelReleaseSLP(GF_InputService *plug, LPNETCHANNEL channel)
{
	FFDemux *ffd = plug->priv;
	if (channel!=ffd->od_ch) return GF_BAD_PARAM;

	ffd->od_state = 2;
	if (ffd->od_au) free(ffd->od_au);
	ffd->od_au = NULL;
	ffd->od_au_size = 0;
	return GF_OK;
}

static Bool FFD_CanHandleURLInService(GF_InputService *plug, const char *url)
{
	char szURL[2048], *sep;
	FFDemux *ffd = (FFDemux *)plug->priv;
	const char *this_url = gf_term_get_service_url(ffd->service);
	if (!this_url || !url) return 0;

	strcpy(szURL, this_url);
	sep = strrchr(szURL, '#');
	if (sep) sep[0] = 0;

	if ((url[0] != '#') && strnicmp(szURL, url, sizeof(char)*strlen(szURL))) return 0;
	sep = strrchr(url, '#');
	if (!stricmp(sep, "#video") && (ffd->video_st>=0)) return 1;
	if (!stricmp(sep, "#audio") && (ffd->audio_st>=0)) return 1;
	return 0;
}

void *New_FFMPEG_Demux() 
{
	FFDemux *priv;
	GF_InputService *ffd = malloc(sizeof(GF_InputService));
	memset(ffd, 0, sizeof(GF_InputService));

	priv = malloc(sizeof(FFDemux));
	memset(priv, 0, sizeof(FFDemux));

    /* register all codecs, demux and protocols */
    av_register_all();
	
	ffd->CanHandleURL = FFD_CanHandleURL;
	ffd->CloseService = FFD_CloseService;
	ffd->ConnectChannel = FFD_ConnectChannel;
	ffd->ConnectService = FFD_ConnectService;
	ffd->DisconnectChannel = FFD_DisconnectChannel;
	ffd->GetServiceDescriptor = FFD_GetServiceDesc;
	ffd->ServiceCommand = FFD_ServiceCommand;
	/*for OD only*/
	ffd->ChannelGetSLP = FFD_ChannelGetSLP;
	ffd->ChannelReleaseSLP = FFD_ChannelReleaseSLP;

	ffd->CanHandleURLInService = FFD_CanHandleURLInService;

	priv->thread = gf_th_new();
	priv->mx = gf_mx_new();

	GF_REGISTER_MODULE_INTERFACE(ffd, GF_NET_CLIENT_INTERFACE, "FFMPEG Demuxer", "gpac distribution");
	ffd->priv = priv;
	return ffd;
}

void Delete_FFMPEG_Demux(void *ifce)
{
	FFDemux *ffd;
	GF_InputService *ptr = (GF_InputService *)ifce;

	ffd = ptr->priv;

	gf_th_del(ffd->thread);
	gf_mx_del(ffd->mx);

	free(ffd);
	free(ptr);
}


