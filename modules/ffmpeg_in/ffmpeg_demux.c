/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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

#ifndef DISABLE_FFMPEG_DEMUX

/*default buffer is 200 ms per channel*/
#define FFD_DATA_BUFFER		800

//#define FFMPEG_DEMUX_ENABLE_MPEG2TS

//#if defined(__DARWIN__) || defined(__APPLE__)
#if !defined(WIN32) && !defined(_WIN32_WCE) && !defined(__SYMBIAN32__)
#include <errno.h>
#endif

/**
 * New versions of ffmpeg do not declare AVERROR_NOMEM, AVERROR_IO, AVERROR_NOFMT
 */

#ifndef AVERROR_NOMEM
#define AVERROR_NOMEM AVERROR(ENOMEM)
#endif /* AVERROR_NOMEM */

#ifndef AVERROR_IO
#define AVERROR_IO AVERROR(EIO)
#endif /* AVERROR_IO */

#ifndef AVERROR_NOFMT
#define AVERROR_NOFMT AVERROR(EINVAL)
#endif /* AVERROR_NOFMT */


#if (LIBAVFORMAT_VERSION_MAJOR >= 54) && (LIBAVFORMAT_VERSION_MINOR >= 20)

#define av_find_stream_info(__c)	avformat_find_stream_info(__c, NULL)
#ifndef FF_API_FORMAT_PARAMETERS
#define FF_API_FORMAT_PARAMETERS	1
#endif

#endif



static u32 FFDemux_Run(void *par)
{
	AVPacket pkt;
	s64 seek_to;
	GF_NetworkCommand com;
	GF_NetworkCommand map;
	GF_SLHeader slh;
	FFDemux *ffd = (FFDemux *) par;

	memset(&map, 0, sizeof(GF_NetworkCommand));
	map.command_type = GF_NET_CHAN_MAP_TIME;

	memset(&com, 0, sizeof(GF_NetworkCommand));
	com.command_type = GF_NET_BUFFER_QUERY;

	memset(&slh, 0, sizeof(GF_SLHeader));

	slh.compositionTimeStampFlag = slh.decodingTimeStampFlag = 1;

	while (ffd->is_running) {
		if ((!ffd->video_ch && (ffd->video_st>=0)) || (!ffd->audio_ch && (ffd->audio_st>=0))) {
			gf_sleep(100);
			continue;
		}

		if ((ffd->seek_time>=0) && ffd->seekable) {
			seek_to = (s64) (AV_TIME_BASE*ffd->seek_time);
			av_seek_frame(ffd->ctx, -1, seek_to, AVSEEK_FLAG_BACKWARD);
			ffd->seek_time = -1;
		}
		pkt.stream_index = -1;
		/*EOF*/
		if (av_read_frame(ffd->ctx, &pkt) <0) break;
		if (pkt.pts == AV_NOPTS_VALUE) pkt.pts = pkt.dts;
		if (!pkt.dts) pkt.dts = pkt.pts;

		slh.compositionTimeStamp = pkt.pts;
		slh.decodingTimeStamp = pkt.dts;

		gf_mx_p(ffd->mx);
		/*blindly send audio as soon as video is init*/
		if (ffd->audio_ch && (pkt.stream_index == ffd->audio_st) ) {
//			u64 seek_audio = ffd->seek_time ? (u64) (s64) (ffd->seek_time*ffd->audio_tscale.den) : 0;
			slh.compositionTimeStamp *= ffd->audio_tscale.num;
			slh.decodingTimeStamp *= ffd->audio_tscale.num;

#if 0
			if (slh.compositionTimeStamp < seek_audio) {
				slh.decodingTimeStamp = slh.compositionTimeStamp = seek_audio;
			}
#endif
			gf_service_send_packet(ffd->service, ffd->audio_ch, (char *) pkt.data, pkt.size, &slh, GF_OK);
		}
		else if (ffd->video_ch && (pkt.stream_index == ffd->video_st)) {
//			u64 seek_video = ffd->seek_time ? (u64) (s64) (ffd->seek_time*ffd->video_tscale.den) : 0;
			slh.compositionTimeStamp *= ffd->video_tscale.num;
			slh.decodingTimeStamp *= ffd->video_tscale.num;

#if 0
			if (slh.compositionTimeStamp < seek_video) {
				slh.decodingTimeStamp = slh.compositionTimeStamp = seek_video;
			}
#endif
			gf_service_send_packet(ffd->service, ffd->video_ch, (char *) pkt.data, pkt.size, &slh, GF_OK);
		}
		gf_mx_v(ffd->mx);
		av_free_packet(&pkt);

		/*sleep untill the buffer occupancy is too low - note that this work because all streams in this
		demuxer are synchronized*/
		while (ffd->audio_run || ffd->video_run) {
			gf_service_command(ffd->service, &com, GF_OK);
			if (com.buffer.occupancy < com.buffer.max)
				break;
			gf_sleep(10);

		}
		if (!ffd->audio_run && !ffd->video_run) break;
	}
	/*signal EOS*/
	if (ffd->audio_ch) gf_service_send_packet(ffd->service, ffd->audio_ch, NULL, 0, NULL, GF_EOS);
	if (ffd->video_ch) gf_service_send_packet(ffd->service, ffd->video_ch, NULL, 0, NULL, GF_EOS);
	ffd->is_running = 2;

	return 0;
}

static const char * FFD_MIME_TYPES[] = {
	"video/x-mpeg", "mpg mpeg mp2 mpa mpe mpv2", "MPEG 1/2 Movies",
	"video/x-mpeg-systems", "mpg mpeg mp2 mpa mpe mpv2", "MPEG 1/2 Movies",
	"audio/basic", "snd au", "Basic Audio",
	"audio/x-wav", "wav", "WAV Audio",
	"audio/vnd.wave", "wav", "WAV Audio",
	"video/x-ms-asf", "asf wma wmv asx asr", "WindowsMedia Movies",
	"video/x-ms-wmv", "asf wma wmv asx asr", "WindowsMedia Movies",
	"video/x-msvideo", "avi", "AVI Movies",
	"video/x-ms-video", "avi", "AVI Movies",
	"video/avi", "avi", "AVI Movies",
	"video/vnd.avi", "avi", "AVI Movies",
	"video/H263", "h263 263", "H263 Video",
	"video/H264", "h264 264", "H264 Video",
	"video/MPEG4", "cmp", "MPEG-4 Video",
	/* We let ffmpeg handle mov because some QT files with uncompressed or adpcm audio use 1 audio sample
	   per MP4 sample which is a killer for our MP4 lib, whereas ffmpeg handles these as complete audio chunks
	   moreover ffmpeg handles cmov, we don't */
	"video/quicktime", "mov qt", "QuickTime Movies",
	/* Supported by latest versions of FFMPEG */
	"video/webm", "webm", "Google WebM Movies",
	"audio/webm", "webm", "Google WebM Music",
#ifdef FFMPEG_DEMUX_ENABLE_MPEG2TS
	"video/mp2t", "ts", "MPEG 2 TS",
#endif
	NULL
};

static u32 FFD_RegisterMimeTypes(const GF_InputService *plug) {
	u32 i;
	for (i = 0 ; FFD_MIME_TYPES[i]; i+=3)
		gf_service_register_mime(plug, FFD_MIME_TYPES[i], FFD_MIME_TYPES[i+1], FFD_MIME_TYPES[i+2]);
	return i/3;
}

static int open_file(AVFormatContext **	ic_ptr, const char * 	filename, AVInputFormat * 	fmt) {
#ifdef USE_PRE_0_7
	return av_open_input_file(ic_ptr, filename, fmt, 0, NULL);
#else
	return avformat_open_input(ic_ptr, filename, fmt, NULL);
#endif
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
	if (!plug || !url)
		return 0;
	/*disable RTP/RTSP from ffmpeg*/
	if (!strnicmp(url, "rtsp://", 7)) return 0;
	if (!strnicmp(url, "rtspu://", 8)) return 0;
	if (!strnicmp(url, "rtp://", 6)) return 0;
	if (!strnicmp(url, "plato://", 8)) return 0;
	if (!strnicmp(url, "udp://", 6)) return 0;
	if (!strnicmp(url, "tcp://", 6)) return 0;
	if (!strnicmp(url, "data:", 5)) return 0;

	strcpy(szName, url);
	ext = strrchr(szName, '#');
	if (ext) ext[0] = 0;
	ext = strrchr(szName, '?');
	if (ext) ext[0] = 0;

	ext = strrchr(szName, '.');
	if (ext && strlen(ext) > 19) ext = NULL;

	if (ext && strlen(ext) > 1) {
		strcpy(szExt, &ext[1]);
		strlwr(szExt);
#ifndef FFMPEG_DEMUX_ENABLE_MPEG2TS
		if (!strcmp(szExt, "ts")) return 0;
#endif

		/*note we forbid ffmpeg to handle files we support*/
		if (!strcmp(szExt, "mp4") || !strcmp(szExt, "mpg4") || !strcmp(szExt, "m4a") || !strcmp(szExt, "m21")
		        || !strcmp(szExt, "m4v") || !strcmp(szExt, "m4a")
		        || !strcmp(szExt, "m4s") || !strcmp(szExt, "3gs")
		        || !strcmp(szExt, "3gp") || !strcmp(szExt, "3gpp") || !strcmp(szExt, "3gp2") || !strcmp(szExt, "3g2")
		        || !strcmp(szExt, "mp3")
		        || !strcmp(szExt, "ac3")
		        || !strcmp(szExt, "amr")
		        || !strcmp(szExt, "bt") || !strcmp(szExt, "wrl") || !strcmp(szExt, "x3dv")
		        || !strcmp(szExt, "xmt") || !strcmp(szExt, "xmta") || !strcmp(szExt, "x3d")

		        || !strcmp(szExt, "jpg") || !strcmp(szExt, "jpeg") || !strcmp(szExt, "png")
		   ) return 0;

		/*check any default stuff that should work with ffmpeg*/
		{
			u32 i;
			for (i = 0 ; FFD_MIME_TYPES[i]; i+=3) {
				if (gf_service_check_mime_register(plug, FFD_MIME_TYPES[i], FFD_MIME_TYPES[i+1], FFD_MIME_TYPES[i+2], ext))
					return 1;
			}
		}
	}

	ctx = NULL;
	if (open_file(&ctx, szName, NULL)<0) {
		AVInputFormat *av_in = NULL;;
		/*some extensions not supported by ffmpeg*/
		if (ext && !strcmp(szExt, "cmp")) av_in = av_find_input_format("m4v");

		if (open_file(&ctx, szName, av_in)<0) {
			return 0;
		}
	}
	if (!ctx || av_find_stream_info(ctx) <0) goto exit;

	/*figure out if we can use codecs or not*/
	has_video = has_audio = 0;
	for(i = 0; i < (s32)ctx->nb_streams; i++) {
		AVCodecContext *enc = ctx->streams[i]->codec;
		switch(enc->codec_type) {
		case AVMEDIA_TYPE_AUDIO:
			if (!has_audio) has_audio = 1;
			break;
		case AVMEDIA_TYPE_VIDEO:
			if (!has_video) has_video= 1;
			break;
		default:
			break;
		}
	}
	if (!has_audio && !has_video) goto exit;
	ret = 1;
#if LIBAVFORMAT_VERSION_MAJOR < 53 && LIBAVFORMAT_VERSION_MINOR < 45
	fmt_out = guess_stream_format(NULL, url, NULL);
#else
	fmt_out = av_guess_format(NULL, url, NULL);
#endif
	if (fmt_out) gf_service_register_mime(plug, fmt_out->mime_type, fmt_out->extensions, fmt_out->name);
	else {
		ext = strrchr(szName, '.');
		if (ext) {
			strcpy(szExt, &ext[1]);
			strlwr(szExt);

			szExtList = gf_modules_get_option((GF_BaseInterface *)plug, "MimeTypes", "application/x-ffmpeg");
			if (!szExtList) {
				gf_service_register_mime(plug, "application/x-ffmpeg", szExt, "Other Movies (FFMPEG)");
			} else if (!strstr(szExtList, szExt)) {
				u32 len;
				char *buf;
				len = (u32) (strlen(szExtList) + strlen(szExt) + 10);
				buf = gf_malloc(sizeof(char)*len);
				sprintf(buf, "\"%s ", szExt);
				strcat(buf, &szExtList[1]);
				gf_modules_set_option((GF_BaseInterface *)plug, "MimeTypes", "application/x-ffmpeg", buf);
				gf_free(buf);
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
			esd->decoderConfig->objectTypeIndication = GPAC_OTI_AUDIO_MPEG1;
			break;
		case CODEC_ID_MP3:
			esd->decoderConfig->objectTypeIndication = GPAC_OTI_AUDIO_MPEG2_PART3;
			break;
		case CODEC_ID_AAC:
			if (!dec->extradata_size) goto opaque_audio;
			esd->decoderConfig->objectTypeIndication = GPAC_OTI_AUDIO_AAC_MPEG4;
			esd->decoderConfig->decoderSpecificInfo->dataLength = dec->extradata_size;
			esd->decoderConfig->decoderSpecificInfo->data = gf_malloc(sizeof(char)*dec->extradata_size);
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
				gf_bs_write_data(bs, (char *) dec->extradata, dec->extradata_size);
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
			esd->decoderConfig->objectTypeIndication = (dec->codec_id==CODEC_ID_H264) ? GPAC_OTI_VIDEO_AVC : GPAC_OTI_VIDEO_MPEG4_PART2;
			esd->decoderConfig->decoderSpecificInfo->dataLength = dec->extradata_size;
			esd->decoderConfig->decoderSpecificInfo->data = gf_malloc(sizeof(char)*dec->extradata_size);
			memcpy(esd->decoderConfig->decoderSpecificInfo->data,
			       dec->extradata,
			       sizeof(char)*dec->extradata_size);
			break;
		case CODEC_ID_MPEG1VIDEO:
			esd->decoderConfig->objectTypeIndication = GPAC_OTI_VIDEO_MPEG1;
			break;
		case CODEC_ID_MPEG2VIDEO:
			esd->decoderConfig->objectTypeIndication = GPAC_OTI_VIDEO_MPEG2_422;
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
			gf_bs_write_u32(bs, dec->pix_fmt);

			if (dec->extradata_size) {
				gf_bs_write_data(bs, (char *) dec->extradata, dec->extradata_size);
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

	if ((ffd->audio_st>=0) && (ffd->service_type != 1)) {
		od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
		esd = FFD_GetESDescriptor(ffd, 1);
		od->objectDescriptorID = esd->ESID;
		audio_esid = esd->ESID;
		gf_list_add(od->ESDescriptors, esd);
		gf_service_declare_media(ffd->service, (GF_Descriptor*)od, (ffd->video_st>=0) ? 1 : 0);
	}
	if ((ffd->video_st>=0) && (ffd->service_type != 2)) {
		od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
		esd = FFD_GetESDescriptor(ffd, 0);
		od->objectDescriptorID = esd->ESID;
		esd->OCRESID = audio_esid;
		gf_list_add(od->ESDescriptors, esd);
		gf_service_declare_media(ffd->service, (GF_Descriptor*)od, 0);
	}
}

#ifdef USE_PRE_0_7
static int ff_url_read(void *h, unsigned char *buf, int size)
{
	u32 retry = 10;
	u32 read;
	int full_size;
	FFDemux *ffd = (FFDemux *)h;

	full_size = 0;
	if (ffd->buffer_used) {
		if (ffd->buffer_used >= (u32) size) {
			ffd->buffer_used-=size;
			memcpy(ffd->buffer, ffd->buffer+size, sizeof(char)*ffd->buffer_used);
#ifdef FFMPEG_DUMP_REMOTE
			if (ffd->outdbg) gf_fwrite(buf, size, 1, ffd->outdbg);
#endif
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
		if (e==GF_IP_NETWORK_EMPTY) {
			if (!retry) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMPEG Demuxer] timeout fetching bytes from network\n") );
				return -1;
			}
			retry --;
			gf_sleep(100);
			continue;
		}
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMPEG Demuxer] error fetching bytes from network: %s\n", gf_error_to_string(e) ) );
			return -1;
		}
		full_size += read;
		if (read==size) break;
		size -= read;
		buf += read;
	}
#ifdef FFMPEG_DUMP_REMOTE
	if (ffd->outdbg) gf_fwrite(ffd->buffer, full_size, 1, ffd->outdbg);
#endif
	return full_size ? (int) full_size : -1;
}
#endif /*USE_PRE_0_7*/


static GF_Err FFD_ConnectService(GF_InputService *plug, GF_ClientService *serv, const char *url)
{
	GF_Err e;
	s64 last_aud_pts;
	u32 i;
	s32 res;
	Bool is_local;
	const char *sOpt;
	char *ext, szName[1024];
	FFDemux *ffd = plug->priv;
	AVInputFormat *av_in = NULL;
	char szExt[20];

	if (ffd->ctx) return GF_SERVICE_ERROR;

	assert( url && strlen(url) < 1024);
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
		ffd->buffer = gf_malloc(sizeof(char)*ffd->buffer_size);
#ifdef FFMPEG_DUMP_REMOTE
		ffd->outdbg = gf_f64_open("ffdeb.raw", "wb");
#endif
#ifdef USE_PRE_0_7
		init_put_byte(&ffd->io, ffd->buffer, ffd->buffer_size, 0, ffd, ff_url_read, NULL, NULL);
		ffd->io.is_streamed = 1;
#else
		ffd->io.seekable = 1;
#endif

		ffd->dnload = gf_service_download_new(ffd->service, url, GF_NETIO_SESSION_NOT_THREADED  | GF_NETIO_SESSION_NOT_CACHED, NULL, ffd);
		if (!ffd->dnload) return GF_URL_ERROR;
		while (1) {
			u32 read;
			e = gf_dm_sess_fetch_data(ffd->dnload, ffd->buffer + ffd->buffer_used, ffd->buffer_size - ffd->buffer_used, &read);
			if (e==GF_EOS) break;
			/*we're sync!!*/
			if (e==GF_IP_NETWORK_EMPTY) continue;
			if (e) goto err_exit;
			ffd->buffer_used += read;
			if (ffd->buffer_used == ffd->buffer_size) break;
		}
		if (e==GF_EOS) {
			const char *cache_file = gf_dm_sess_get_cache_name(ffd->dnload);
			res = open_file(&ffd->ctx, cache_file, av_in);
		} else {
			pd.filename = szName;
			pd.buf_size = ffd->buffer_used;
			pd.buf = (u8 *) ffd->buffer;
			av_in = av_probe_input_format(&pd, 1);
			if (!av_in) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMPEG] error probing file %s - probe start with %c %c %c %c\n", url, ffd->buffer[0], ffd->buffer[1], ffd->buffer[2], ffd->buffer[3]));
				return GF_NOT_SUPPORTED;
			}
			/*setup downloader*/
			av_in->flags |= AVFMT_NOFILE;
#if FF_API_FORMAT_PARAMETERS /*commit ffmpeg 603b8bc2a109978c8499b06d2556f1433306eca7*/
			res = avformat_open_input(&ffd->ctx, szName, av_in, NULL);
#else
			res = av_open_input_stream(&ffd->ctx, &ffd->io, szName, av_in, NULL);
#endif
		}
	} else {
		res = open_file(&ffd->ctx, szName, av_in);
	}

	switch (res) {
#ifndef _WIN32_WCE
	case 0:
		e = GF_OK;
		break;
	case AVERROR_IO:
		e = GF_URL_ERROR;
		goto err_exit;
	case AVERROR_INVALIDDATA:
		e = GF_NON_COMPLIANT_BITSTREAM;
		goto err_exit;
	case AVERROR_NOMEM:
		e = GF_OUT_OF_MEM;
		goto err_exit;
	case AVERROR_NOFMT:
		e = GF_NOT_SUPPORTED;
		goto err_exit;
#endif
	default:
		e = GF_SERVICE_ERROR;
		goto err_exit;
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[FFMPEG] looking for streams in %s - %d streams - type %s\n", ffd->ctx->filename, ffd->ctx->nb_streams, ffd->ctx->iformat->name));

	res = av_find_stream_info(ffd->ctx);
	if (res <0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMPEG] cannot locate streams - error %d\n", res));
		e = GF_NOT_SUPPORTED;
		goto err_exit;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[FFMPEG] file %s opened - %d streams\n", url, ffd->ctx->nb_streams));

	/*figure out if we can use codecs or not*/
	ffd->audio_st = ffd->video_st = -1;
	for (i = 0; i < ffd->ctx->nb_streams; i++) {
		AVCodecContext *enc = ffd->ctx->streams[i]->codec;
		switch(enc->codec_type) {
		case AVMEDIA_TYPE_AUDIO:
			if ((ffd->audio_st<0) && (ffd->service_type!=1)) {
				ffd->audio_st = i;
				ffd->audio_tscale = ffd->ctx->streams[i]->time_base;
			}
			break;
		case AVMEDIA_TYPE_VIDEO:
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

	/*build seek*/
	if (is_local) {
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

		ffd->seekable = (av_seek_frame(ffd->ctx, -1, 0, AVSEEK_FLAG_BACKWARD)<0) ? 0 : 1;
		if (!ffd->seekable) {
			av_close_input_file(ffd->ctx);
			ffd->ctx = NULL;
			open_file(&ffd->ctx, szName, av_in);
			av_find_stream_info(ffd->ctx);
		}
	}

	/*let's go*/
	gf_service_connect_ack(serv, NULL, GF_OK);
	/*if (!ffd->service_type)*/ FFD_SetupObjects(ffd);
	ffd->service_type = 0;
	return GF_OK;

err_exit:
	GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[FFMPEG] Error opening file %s: %s\n", url, gf_error_to_string(e)));
	if (ffd->ctx) av_close_input_file(ffd->ctx);
	ffd->ctx = NULL;
	gf_service_connect_ack(serv, NULL, e);
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
		if (ffd->audio_st<0) return NULL;
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
		if (ffd->video_st<0) return NULL;
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
		if (ffd->is_running) {
			while (!ffd->is_running) gf_sleep(1);
			ffd->is_running = 0;
		}
		gf_service_download_del(ffd->dnload);
		ffd->dnload = NULL;
	}
	if (ffd->buffer) gf_free(ffd->buffer);
	ffd->buffer = NULL;

	gf_service_disconnect_ack(ffd->service, NULL, GF_OK);
#ifdef FFMPEG_DUMP_REMOTE
	if (ffd->outdbg) fclose(ffd->outdbg);
#endif
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
	sscanf(url, "ES_ID=%u", &ESID);

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
	gf_service_connect_ack(ffd->service, channel, e);
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
	gf_service_disconnect_ack(ffd->service, channel, e);
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
	FFDemux *ffd;
	const char *this_url;
	if (!plug || !url)
		return GF_FALSE;
	ffd = (FFDemux *)plug->priv;
	this_url = gf_service_get_url(ffd->service);
	if (!this_url)
		return GF_FALSE;

	strcpy(szURL, this_url);
	sep = strrchr(szURL, '#');
	if (sep) sep[0] = 0;

	if ((url[0] != '#') && strnicmp(szURL, url, sizeof(char)*strlen(szURL))) return 0;
	sep = strrchr(url, '#');
	if (sep && !stricmp(sep, "#video") && (ffd->video_st>=0)) return 1;
	if (sep && !stricmp(sep, "#audio") && (ffd->audio_st>=0)) return 1;
	return GF_FALSE;
}

void *New_FFMPEG_Demux()
{
	FFDemux *priv;
	GF_InputService *ffd = gf_malloc(sizeof(GF_InputService));
	memset(ffd, 0, sizeof(GF_InputService));

	GF_SAFEALLOC(priv, FFDemux);

	GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[FFMPEG Demuxer] Registering all ffmpeg plugins...\n") );
	/* register all codecs, demux and protocols */
	av_register_all();
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[FFMPEG Demuxer] Registering all ffmpeg plugins DONE.\n") );

	ffd->RegisterMimeTypes = FFD_RegisterMimeTypes;
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
	if (!ptr)
		return;
	ffd = ptr->priv;
	if (ffd) {
		if (ffd->thread)
			gf_th_del(ffd->thread);
		ffd->thread = NULL;
		if (ffd->mx)
			gf_mx_del(ffd->mx);
		ffd->mx = NULL;
		gf_free(ffd);
		ptr->priv = NULL;
	}
	gf_free(ptr);
}


#endif
