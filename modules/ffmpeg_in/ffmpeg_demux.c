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

/*default buffer is 200 ms per channel*/
#define FFD_DATA_BUFFER		800

//#if defined(__DARWIN__) || defined(__APPLE__)
#if !defined(WIN32) && !defined(_WIN32_WCE) && !defined(__SYMBIAN32__)
#include <errno.h>
#endif

static u32 FFDemux_Run(void *par)
{
	AVPacket pkt;
	s64 seek_to;
	u64 seek_audio, seek_video;
	Bool video_init, do_seek, map_audio_time, map_video_time;
	GF_NetworkCommand com;
	GF_NetworkCommand map;
	GF_SLHeader slh;
	FFDemux *ffd = (FFDemux *) par;

	memset(&map, 0, sizeof(GF_NetworkCommand));
	map.command_type = GF_NET_CHAN_MAP_TIME;
	
	memset(&com, 0, sizeof(GF_NetworkCommand));
	com.command_type = GF_NET_CHAN_BUFFER_QUERY;

	memset(&slh, 0, sizeof(GF_SLHeader));

	slh.compositionTimeStampFlag = slh.decodingTimeStampFlag = 1;
	seek_to = (s64) (AV_TIME_BASE*ffd->seek_time);
	map_video_time = !ffd->seekable;

	video_init = (seek_to && ffd->video_ch) ? 0 : 1;
	seek_audio = seek_video = 0;
	if (ffd->seekable && (ffd->audio_st>=0)) seek_audio = (u64) (s64) (ffd->seek_time*ffd->audio_tscale.den);
	if (ffd->seekable && (ffd->video_st>=0)) seek_video = (u64) (s64) (ffd->seek_time*ffd->video_tscale.den);

	/*it appears that ffmpeg has trouble resyncing on some mpeg files - we trick it by restarting to 0 to get the 
	first video frame, and only then seek*/
	if (ffd->seekable) av_seek_frame(ffd->ctx, -1, video_init ? seek_to : 0, AVSEEK_FLAG_BACKWARD);
	do_seek = !video_init;
	map_audio_time = video_init ? ffd->unreliable_audio_timing : 0;

	while (ffd->is_running) {

		pkt.stream_index = -1;
		/*EOF*/
        if (av_read_frame(ffd->ctx, &pkt) <0) break;
		if (pkt.pts == AV_NOPTS_VALUE) pkt.pts = pkt.dts;
		if (!pkt.dts) pkt.dts = pkt.pts;

		slh.compositionTimeStamp = pkt.pts;
		slh.decodingTimeStamp = pkt.dts;

		gf_mx_p(ffd->mx);
		/*blindly send audio as soon as video is init*/
		if (ffd->audio_ch && (pkt.stream_index == ffd->audio_st) && !do_seek) {
			slh.compositionTimeStamp *= ffd->audio_tscale.num;
			slh.decodingTimeStamp *= ffd->audio_tscale.num;

			if (map_audio_time) {
				map.base.on_channel = ffd->audio_ch;
				map.map_time.media_time = ffd->seek_time;
				/*mapwith TS=0 since we don't use SL*/
				map.map_time.timestamp = 0;
				map.map_time.reset_buffers = 1;
				map_audio_time = 0;
				gf_term_on_command(ffd->service, &map, GF_OK);
			}
			else if (slh.compositionTimeStamp < seek_audio) {
				slh.decodingTimeStamp = slh.compositionTimeStamp = seek_audio;
			}
			gf_term_on_sl_packet(ffd->service, ffd->audio_ch, pkt.data, pkt.size, &slh, GF_OK);
		} 
		else if (ffd->video_ch && (pkt.stream_index == ffd->video_st)) {
			slh.compositionTimeStamp *= ffd->video_tscale.num;
			slh.decodingTimeStamp *= ffd->video_tscale.num;

			/*if we get pts = 0 after a seek the demuxer is reseting PTSs, so force map time*/
			if ((!do_seek && seek_to && !slh.compositionTimeStamp) || (map_video_time) ) {
				seek_to = 0;
				map_video_time = 0;

				map.base.on_channel = ffd->video_ch;
				map.map_time.timestamp = (u64) pkt.pts;
//				map.map_time.media_time = ffd->seek_time;
				map.map_time.media_time = 0;
				map.map_time.reset_buffers = 0;
				gf_term_on_command(ffd->service, &map, GF_OK);
			}
			else if (slh.compositionTimeStamp < seek_video) {
				slh.decodingTimeStamp = slh.compositionTimeStamp = seek_video;
			}
			gf_term_on_sl_packet(ffd->service, ffd->video_ch, pkt.data, pkt.size, &slh, GF_OK);
			video_init = 1;
		}
		gf_mx_v(ffd->mx);
		av_free_packet(&pkt);

		/*here's the trick - only seek after sending the first packets of each stream - this allows ffmpeg video decoders
		to resync properly*/
		if (do_seek && video_init && ffd->seekable) {
			av_seek_frame(ffd->ctx, -1, seek_to, AVSEEK_FLAG_BACKWARD);
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
	ffd->is_running = 2;

	return 0;
}

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
	if (!strnicmp(szName, "plato://", 8)) return 0;

	ext = strrchr(szName, '.');
	if (ext) {
		strcpy(szExt, &ext[1]);
		strlwr(szExt);
		if (!strcmp(szExt, "ts")) return 0;


		/*note we forbid ffmpeg to handle files we support*/
		if (!strcmp(szExt, "mp4") || !strcmp(szExt, "mpg4") || !strcmp(szExt, "m4a") || !strcmp(szExt, "m21") 
			|| !strcmp(szExt, "m4v") || !strcmp(szExt, "m4a") 
			|| !strcmp(szExt, "3gp") || !strcmp(szExt, "3gpp") || !strcmp(szExt, "3gp2") || !strcmp(szExt, "3g2") 
			|| !strcmp(szExt, "mp3") 
			|| !strcmp(szExt, "ac3") 
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
		if (gf_term_check_extension(plug, "audio/vnd.wave", "wav", "WAV Audio", ext)) return 1;
		if (gf_term_check_extension(plug, "video/x-ms-asf", "asf wma wmv asx asr", "WindowsMedia Movies", ext)) return 1;
		if (gf_term_check_extension(plug, "video/x-ms-wmv", "asf wma wmv asx asr", "WindowsMedia Movies", ext)) return 1;
		if (gf_term_check_extension(plug, "video/avi", "avi", "AVI Movies", ext)) return 1;
		if (gf_term_check_extension(plug, "video/vnd.avi", "avi", "AVI Movies", ext)) return 1;

		if (gf_term_check_extension(plug, "video/H263", "h263 263", "H263 Video", ext)) return 1;
		if (gf_term_check_extension(plug, "video/H264", "h264 264", "H264 Video", ext)) return 1;
		if (gf_term_check_extension(plug, "video/MPEG4", "cmp", "MPEG-4 Video", ext)) return 1;
		/*we let ffmpeg handle mov because some QT files with uncompressed or adpcm audio use 1 audio sample 
		per MP4 sample which is a killer for our MP4 lib, whereas ffmpeg handles these as complete audio chunks 
		moreover ffmpeg handles cmov, we don't*/
		if (gf_term_check_extension(plug, "video/quicktime", "mov qt", "QuickTime Movies", ext)) return 1;
	}

	ctx = NULL;
    if (av_open_input_file(&ctx, szName, NULL, 0, NULL)<0) {
		AVInputFormat *av_in = NULL;;
		/*some extensions not supported by ffmpeg*/
		if (ext && !strcmp(szExt, "cmp")) av_in = av_find_input_format("m4v");

		if (av_open_input_file(&ctx, szName, av_in, 0, NULL)<0) {
			return 0;
		}
	}

    if (!ctx || av_find_stream_info(ctx) <0) goto exit;
	/*figure out if we can use codecs or not*/
	has_video = has_audio = 0;
    for(i = 0; i < ctx->nb_streams; i++) {
        AVCodecContext *enc = ctx->streams[i]->codec;
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

			szExtList = gf_modules_get_option((GF_BaseInterface *)plug, "MimeTypes", "application/x-ffmpeg");
			if (!szExtList) {
				gf_term_register_mime_type(plug, "application/x-ffmpeg", szExt, "Other Movies (FFMPEG)");
			} else if (!strstr(szExtList, szExt)) {
				u32 len;
				char *buf;
				len = strlen(szExtList) + strlen(szExt) + 10;
				buf = malloc(sizeof(char)*len);
				sprintf(buf, "\"%s ", szExt);
				strcat(buf, &szExtList[1]);
				gf_modules_set_option((GF_BaseInterface *)plug, "MimeTypes", "application/x-ffmpeg", buf);
				free(buf);
			}
		}
	}

exit:
    if (ctx) av_close_input_file(ctx);
	return ret;
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
        AVCodecContext *dec = ffd->ctx->streams[ffd->audio_st]->codec;
		esd->slConfig->timestampResolution = ffd->audio_tscale.den;
 		switch (dec->codec_id) {
		case CODEC_ID_MP2:
			esd->decoderConfig->objectTypeIndication = 0x6B;
			break;
		case CODEC_ID_MP3:
			esd->decoderConfig->objectTypeIndication = 0x69;
			break;
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
			esd->decoderConfig->objectTypeIndication = GPAC_OTI_MEDIA_FFMPEG;
			bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			gf_bs_write_u32(bs, dec->codec_id);
			gf_bs_write_u32(bs, dec->sample_rate);
			gf_bs_write_u16(bs, dec->channels);
			gf_bs_write_u16(bs, dec->frame_size);
			gf_bs_write_u8(bs, 16);
			gf_bs_write_u8(bs, 0);
			/*ffmpeg specific*/
			gf_bs_write_u16(bs, dec->block_align);
			gf_bs_write_u32(bs, dec->bit_rate);
			gf_bs_write_u32(bs, dec->codec_tag);
			if (dec->extradata_size) {
				gf_bs_write_data(bs, dec->extradata, dec->extradata_size);
			}
			gf_bs_get_content(bs, (char **) &esd->decoderConfig->decoderSpecificInfo->data, &esd->decoderConfig->decoderSpecificInfo->dataLength);
			gf_bs_del(bs);
			break;
		}
		dont_use_sl = ffd->unreliable_audio_timing;
	} else {
        AVCodecContext *dec = ffd->ctx->streams[ffd->video_st]->codec;
		esd->slConfig->timestampResolution = ffd->video_tscale.den;
		switch (dec->codec_id) {
		case CODEC_ID_MPEG4:
		/*there is a bug in fragmentation of raw H264 in ffmpeg, the NALU startcode (0x00000001) is split across
		two frames - we therefore force internal ffmpeg codec ID to avoid NALU size recompute
		at the decoder level*/
//		case CODEC_ID_H264:
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
			esd->decoderConfig->objectTypeIndication = GPAC_OTI_MEDIA_FFMPEG;
			bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			gf_bs_write_u32(bs, dec->codec_id);
			gf_bs_write_u16(bs, dec->width);
			gf_bs_write_u16(bs, dec->height);
			/*ffmpeg specific*/
			gf_bs_write_u32(bs, dec->bit_rate);
			gf_bs_write_u32(bs, dec->codec_tag);

			if (dec->extradata_size) {
				gf_bs_write_data(bs, dec->extradata, dec->extradata_size);
			}
			gf_bs_get_content(bs, (char **) &esd->decoderConfig->decoderSpecificInfo->data, &esd->decoderConfig->decoderSpecificInfo->dataLength);
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
	}

	return esd;
}


static void FFD_SetupObjects(FFDemux *ffd)
{
	GF_ESD *esd;
	GF_ObjectDescriptor *od;
	u32 audio_esid = 0;
	
	if (ffd->audio_st>=0) {
		od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
		esd = FFD_GetESDescriptor(ffd, 1);
		od->objectDescriptorID = esd->ESID;
		audio_esid = esd->ESID;
		gf_list_add(od->ESDescriptors, esd);
		gf_term_add_media(ffd->service, (GF_Descriptor*)od, (ffd->video_st>=0) ? 1 : 0);
	}
	if (ffd->video_st>=0) {
		od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
		esd = FFD_GetESDescriptor(ffd, 0);
		od->objectDescriptorID = esd->ESID;
		esd->OCRESID = audio_esid;
		gf_list_add(od->ESDescriptors, esd);
		gf_term_add_media(ffd->service, (GF_Descriptor*)od, 0);
	}
}

static int ff_url_read(void *h, unsigned char *buf, int size)
{
	u32 read;
	int full_size;
	FFDemux *ffd = (FFDemux *)h;

	gf_term_download_update_stats(ffd->dnload);
	full_size = 0;
	if (ffd->buffer_used) {
		if (ffd->buffer_used>size) {
			ffd->buffer_used-=size;
			memcpy(ffd->buffer, ffd->buffer+size, sizeof(char)*ffd->buffer_used);
			if (ffd->outdbg) fwrite(buf, size, 1, ffd->outdbg);
			return size;
		} 
		full_size += ffd->buffer_used;
		buf += ffd->buffer_used;
		size -= ffd->buffer_used;
		ffd->buffer_used = 0;
	}

	while (size) {
		GF_Err e = gf_dm_sess_fetch_data(ffd->dnload, buf, size, &read);
		if (e==GF_EOS) break;
		/*we're sync!!*/
		if (e==GF_IP_NETWORK_EMPTY) continue;
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMPEG Demuxer] error fetching bytes from network: %s\n", gf_error_to_string(e) ) );
			return -1;
		}
		full_size += read;
		if (read==size) break;
		size -= read;
		buf += read;
	}
	if (ffd->outdbg) fwrite(ffd->buffer, full_size, 1, ffd->outdbg);
	return full_size ? (int) full_size : -1;
}

static void FFD_NetIO(void *cbk, GF_NETIO_Parameter *param)
{
}

static GF_Err FFD_ConnectService(GF_InputService *plug, GF_ClientService *serv, const char *url)
{
	GF_Err e;
	s64 last_aud_pts;
	u32 i;
	Bool is_local;
	const char *sOpt;
	char *ext, szName[1000];
	FFDemux *ffd = plug->priv;
	AVInputFormat *av_in = NULL;
	char szExt[20];

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

	/*some extensions not supported by ffmpeg, overload input format*/
	ext = strrchr(szName, '.');
	strcpy(szExt, ext ? ext+1 : "");
	strlwr(szExt);
	if (!strcmp(szExt, "cmp")) av_in = av_find_input_format("m4v");

	is_local = (strnicmp(url, "file://", 7) && strstr(url, "://")) ? 0 : 1;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[FFMPEG] opening file %s - local %d - av_in %08x\n", url, is_local, av_in));

	if (!is_local) {
	    AVProbeData   pd;

		/*setup wraper for FFMPEG I/O*/
		ffd->buffer_size = 8192;
		sOpt = gf_modules_get_option((GF_BaseInterface *)plug, "FFMPEG", "IOBufferSize"); 
		if (sOpt) ffd->buffer_size = atoi(sOpt);
		ffd->buffer = malloc(sizeof(char)*ffd->buffer_size);
		ffd->outdbg = fopen("ffdeb.raw", "wb");

		init_put_byte(&ffd->io, ffd->buffer, ffd->buffer_size, 0, ffd, ff_url_read, NULL, NULL);

		ffd->dnload = gf_term_download_new(ffd->service, url, GF_NETIO_SESSION_NOT_THREADED  | GF_NETIO_SESSION_NOT_CACHED, FFD_NetIO, ffd);
		if (!ffd->dnload) return GF_URL_ERROR;
		while (1) {
			e = gf_dm_sess_fetch_data(ffd->dnload, ffd->buffer, ffd->buffer_size, &ffd->buffer_used);
			if (e==GF_EOS) break;
			/*we're sync!!*/
			if (e==GF_IP_NETWORK_EMPTY) continue;
			if (e) goto err_exit;
			if (ffd->buffer_used) break;
		}

		pd.filename = szName;
		pd.buf_size = ffd->buffer_used;
		pd.buf = ffd->buffer;
		av_in = av_probe_input_format(&pd, 1);
		if (!av_in) {
			return GF_NOT_SUPPORTED;
		}
		if (ffd->outdbg) fwrite(ffd->buffer, ffd->buffer_used, 1, ffd->outdbg);
		ffd->buffer_used = 0;

		/*setup downloader*/
		av_in->flags |= AVFMT_NOFILE;
		i = av_open_input_stream(&ffd->ctx, &ffd->io, szName, av_in, NULL);
	} else {
		i = av_open_input_file(&ffd->ctx, szName, av_in, 0, NULL);
	}

	switch (i) {
	case 0: e = GF_OK; break;
	case AVERROR_IO: e = GF_URL_ERROR; goto err_exit;
	case AVERROR_INVALIDDATA: e = GF_NON_COMPLIANT_BITSTREAM; goto err_exit;
	case AVERROR_NOMEM: e = GF_OUT_OF_MEM; goto err_exit;
	case AVERROR_NOFMT: e = GF_NOT_SUPPORTED; goto err_exit;
	default: e = GF_SERVICE_ERROR; goto err_exit;
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[FFMPEG] looking for streams in %s - %d streams - type %s\n", ffd->ctx->filename, ffd->ctx->nb_streams, ffd->ctx->iformat->name));

    if (av_find_stream_info(ffd->ctx) <0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMPEG] cannot locate streams\n"));
		e = GF_NOT_SUPPORTED;
		goto err_exit;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[FFMPEG] file %s opened - %d streams\n", url, ffd->ctx->nb_streams));

	/*figure out if we can use codecs or not*/
	ffd->audio_st = ffd->video_st = -1;
    for (i = 0; i < ffd->ctx->nb_streams; i++) {
        AVCodecContext *enc = ffd->ctx->streams[i]->codec;
        switch(enc->codec_type) {
        case CODEC_TYPE_AUDIO:
            if ((ffd->audio_st<0) && (ffd->service_type!=1)) {
				ffd->audio_st = i;
				ffd->audio_tscale = ffd->ctx->streams[i]->time_base;
			}
            break;
        case CODEC_TYPE_VIDEO:
            if ((ffd->video_st<0) && (ffd->service_type!=2)) {
				ffd->video_st = i;
				ffd->video_tscale = ffd->ctx->streams[i]->time_base;
			}
            break;
        default:
            break;
        }
    }
	if ((ffd->service_type==1) && (ffd->video_st<0)) goto err_exit;
	if ((ffd->service_type==2) && (ffd->audio_st<0)) goto err_exit;
	if ((ffd->video_st<0) && (ffd->audio_st<0)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMPEG] No supported streams in file\n"));
		goto err_exit;
	}


	sOpt = gf_modules_get_option((GF_BaseInterface *)plug, "FFMPEG", "DataBufferMS"); 
	ffd->data_buffer_ms = 0;
	if (sOpt) ffd->data_buffer_ms = atoi(sOpt);
	if (!ffd->data_buffer_ms) ffd->data_buffer_ms = FFD_DATA_BUFFER;

	/*check we do have increasing pts. If not we can't rely on pts, we must skip SL
	we assume video pts is always present*/
	if (ffd->audio_st>=0) {
		last_aud_pts = 0;
		for (i=0; i<20; i++) {
			AVPacket pkt;
			pkt.stream_index = -1;
			if (av_read_frame(ffd->ctx, &pkt) <0) break;
			if (pkt.pts == AV_NOPTS_VALUE) pkt.pts = pkt.dts;
			if (pkt.stream_index==ffd->audio_st) last_aud_pts = pkt.pts;
		}
		if (last_aud_pts*ffd->audio_tscale.den<10*ffd->audio_tscale.num) ffd->unreliable_audio_timing = 1;
	}

	/*build seek*/
	if (is_local) {
		ffd->seekable = (av_seek_frame(ffd->ctx, -1, 0, AVSEEK_FLAG_BACKWARD)<0) ? 0 : 1;
		if (!ffd->seekable) {
			av_close_input_file(ffd->ctx);
			av_open_input_file(&ffd->ctx, szName, av_in, 0, NULL);
			av_find_stream_info(ffd->ctx);
		}
	}

	/*let's go*/
	gf_term_on_connect(serv, NULL, GF_OK);
	if (!ffd->service_type) FFD_SetupObjects(ffd);
	ffd->service_type = 0;
	return GF_OK;

err_exit:
	GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMPEG] Error opening file %s: %s\n", url, gf_error_to_string(e)));
    if (ffd->ctx) av_close_input_file(ffd->ctx);
	ffd->ctx = NULL;
	gf_term_on_connect(serv, NULL, e);
	return GF_OK;
}


static GF_Descriptor *FFD_GetServiceDesc(GF_InputService *plug, u32 expect_type, const char *sub_url)
{
	GF_ObjectDescriptor *od;
	GF_ESD *esd;
	FFDemux *ffd = plug->priv;

	if (!ffd->ctx) return NULL;

	if (expect_type==GF_MEDIA_OBJECT_UNDEF) {
		if (ffd->video_st>=0) expect_type=GF_MEDIA_OBJECT_VIDEO;
		else if (ffd->audio_st>=0) expect_type=GF_MEDIA_OBJECT_AUDIO;
	}


	/*since we don't handle multitrack in ffmpeg, we don't need to check sub_url, only use expected type*/
	if (expect_type==GF_MEDIA_OBJECT_AUDIO) {
		od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
		od->objectDescriptorID = 1;
		esd = FFD_GetESDescriptor(ffd, 1);
		/*if session join, setup sync*/
		if (ffd->video_ch) esd->OCRESID = ffd->video_st+1;
		gf_list_add(od->ESDescriptors, esd);
		ffd->service_type = 2;
		return (GF_Descriptor *) od;
	}
	if (expect_type==GF_MEDIA_OBJECT_VIDEO) {
		od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
		od->objectDescriptorID = 1;
		esd = FFD_GetESDescriptor(ffd, 0);
		/*if session join, setup sync*/
		if (ffd->audio_ch) esd->OCRESID = ffd->audio_st+1;
		gf_list_add(od->ESDescriptors, esd);
		ffd->service_type = 1;
		return (GF_Descriptor *) od;
	}
	return NULL;
}


static GF_Err FFD_CloseService(GF_InputService *plug)
{
	FFDemux *ffd = plug->priv;

	ffd->is_running = 0;

	if (ffd->ctx) av_close_input_file(ffd->ctx);
	ffd->ctx = NULL;
	ffd->audio_ch = ffd->video_ch = NULL;
	ffd->audio_run = ffd->video_run = 0;

	if (ffd->dnload) {
		while (!ffd->is_running) gf_sleep(0);
		ffd->is_running = 0;
		gf_term_download_del(ffd->dnload);
		ffd->dnload = NULL;
	}
	if (ffd->buffer) free(ffd->buffer);
	ffd->buffer = NULL;

	gf_term_on_disconnect(ffd->service, NULL, GF_OK);
	if (ffd->outdbg) fclose(ffd->outdbg);
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

	if ((s32) ESID == 1 + ffd->audio_st) {
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
	if (ffd->audio_ch == channel) {
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


	if (com->command_type==GF_NET_SERVICE_HAS_AUDIO) {
		if (ffd->audio_st>=0) return GF_OK;
		return GF_NOT_SUPPORTED;
	}

	if (!com->base.on_channel) return GF_NOT_SUPPORTED;

	switch (com->command_type) {
	/*only BIFS/OD work in pull mode (cf ffmpeg_in.h)*/
	case GF_NET_CHAN_SET_PULL:
		return GF_NOT_SUPPORTED;
	case GF_NET_CHAN_INTERACTIVE:
		return ffd->seekable ? GF_OK : GF_NOT_SUPPORTED;
	case GF_NET_CHAN_BUFFER:
		com->buffer.max = com->buffer.min = 0;
		return GF_OK;
	case GF_NET_CHAN_DURATION:
		if (ffd->ctx->duration == AV_NOPTS_VALUE)
			com->duration.duration = -1;
		else
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
			if (ffd->is_running!=1) {
				ffd->is_running = 1;
				gf_th_run(ffd->thread, FFDemux_Run, ffd);
			}
		}
		gf_mx_v(ffd->mx);
		return GF_OK;
	case GF_NET_CHAN_STOP:
		if (ffd->audio_ch==com->base.on_channel) ffd->audio_run = 0;
		else if (ffd->video_ch==com->base.on_channel) ffd->video_run = 0;
		return GF_OK;
	/*note we don't handle PAUSE/RESUME/SET_SPEED, this is automatically handled by the demuxing thread 
	through buffer occupancy queries*/

	default:
		return GF_OK;
	}

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

	ffd->CanHandleURLInService = FFD_CanHandleURLInService;

	priv->thread = gf_th_new("FFMPEG Demux");
	priv->mx = gf_mx_new("FFMPEG Demux");

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


