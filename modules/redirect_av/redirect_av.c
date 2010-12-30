/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean le Feuvre
 *			Copyright (c) 2010-20XX Telecom ParisTech
 *			All rights reserved
 *
 *  This file is part of GPAC / AVI Recorder demo module
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
#include <stdlib.h>
#include "ts_muxer.h"

#define USE_GPAC_MPEG2TS
#undef USE_GPAC_MPEG2TS

#ifdef USE_GPAC_MPEG2TS
#include "gpac_ts_muxer.c"
#else
#include "ffmpeg_ts_muxer.c"
#endif /* USE_GPAC_MPEG2TS */

#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>

/* This number * 188 should be lower than the UDP packet size */
#define TS_PACKETS_PER_UDP_PACKET 1

#define outputWidth 320

#define outputHeight 240

#define audioCodecBitrate 96000

static const u32 maxFPS = 25;

static const u32 outbuf_size = outputWidth * outputHeight * 4;

static const char * moduleName = "AVRedirect";

static const char * AVR_VIDEO_CODEC_OPTION = "VideoCodec";

static void avr_on_audio_frame ( void *udta, char *buffer, u32 buffer_size, u32 time, u32 delay_ms )
{
    GF_AVRedirect *avr = ( GF_AVRedirect * ) udta;
#ifdef AVR_DUMP_RAW_AVI
    AVI_write_audio ( avr->avi_out, buffer, buffer_size );
#endif /* AVR_DUMP_RAW_AVI */
    {
        gf_ringbuffer_write(avr->pcmAudio, buffer, buffer_size);
        avr->audioCurrentTime = time - delay_ms;
    }
}

static void avr_on_audio_reconfig ( void *udta, u32 samplerate, u32 bits_per_sample, u32 nb_channel, u32 channel_cfg )
{
#ifdef AVR_DUMP_RAW_AVI
    GF_AVRedirect *avr = ( GF_AVRedirect * ) udta;
    AVI_set_audio ( avr->avi_out, nb_channel, samplerate, bits_per_sample, WAVE_FORMAT_PCM, 0 );
#endif /* AVR_DUMP_RAW_AVI */
    printf("Audio %u Hz, bitsPerSample=%u, nbchannels=%u\n", samplerate, bits_per_sample, nb_channel);
}

static void avr_on_video_frame ( void *udta, u32 time )
{
    u32 i, j;
    GF_Err e;
    GF_VideoSurface fb;
    GF_AVRedirect *avr = ( GF_AVRedirect * ) udta;
    gf_mx_p(avr->frameMutex);
    e = gf_sc_get_screen_buffer ( avr->term->compositor, &fb, 0 );
    if ( e )
    {
        GF_LOG ( GF_LOG_ERROR, GF_LOG_MODULE, ( "[AVRedirect] Error grabing frame buffer %s\n", gf_error_to_string ( e ) ) );
        return;
    }
    /*convert frame*/
    for ( i=0; i<fb.height; i++ )
    {
        char *dst = avr->frame + i * fb.width * 3;
        char *src = fb.video_buffer + i * fb.pitch_y;
        for ( j=0; j<fb.width; j++ )
        {
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            src+=4;
            dst += 3;
        }
    }
    avr->frameTime = time;
    gf_mx_v(avr->frameMutex);
    gf_sc_release_screen_buffer ( avr->term->compositor, &fb );
    GF_LOG ( GF_LOG_DEBUG, GF_LOG_MODULE, ( "[AVRedirect] Writing video frame\n" ) );
}

static void avr_on_video_reconfig ( void *udta, u32 width, u32 height )
{
    char comp[5];
    GF_AVRedirect *avr = ( GF_AVRedirect * ) udta;
    GF_LOG ( GF_LOG_INFO, GF_LOG_MODULE, ( "[AVRedirect] Video reconfig width %d height %d\n", width, height ) );


    comp[0] = comp[1] = comp[2] = comp[3] = comp[4] = 0;
    gf_mx_p(avr->frameMutex);
#ifdef AVR_DUMP_RAW_AVI
    AVI_set_video ( avr->avi_out, width, height, 30, comp );
#endif /* AVR_DUMP_RAW_AVI */
    if ( avr->frame ) gf_free ( avr->frame );
    avr->size = 3*width*height;
    avr->frame = gf_malloc ( sizeof ( char ) *avr->size );
    avr->srcWidth = width;
    avr->srcHeight = height;
    avr->swsContext = sws_getCachedContext ( avr->swsContext, avr->srcWidth, avr->srcHeight, PIX_FMT_RGB24, outputWidth, outputHeight, PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL );
    gf_mx_v(avr->frameMutex);
}

#define AVR_TIMERS_LENGHT 25

#define U32_ABS( a, b ) (a > b ? a - b : b - a)

#define DUMP_MP3
#undef DUMP_MP3

/**
 * This thread sends the frame to TS mux
 * \param Parameter The GF_AVRedirect pointer
 */
static Bool audio_encoding_thread_run(void *param)
{
    u_int8_t * inBuff;
    u_int8_t * outBuff;
    u32 samplesReaden, toRead;
    u64 myTime = 0;
    u32 frameCountSinceReset = 0;
    u32 lastCurrentTime;
#ifdef DUMP_MP3
    FILE * mp3 = fopen("/tmp/dump.mp3", "w");
#endif /* DUMP_MP3 */
    Bool sendPts = 1;
    GF_AVRedirect * avr = (GF_AVRedirect*) param;
    AVCodecContext * ctx = ts_get_audio_codec_context(avr->ts_implementation);
    printf("******* Audio Codec Context = %d/%d, start="LLU", frame_size=%u\n", ctx->time_base.num, ctx->time_base.den, ctx->timecode_frame_start, ctx->frame_size);
    assert( avr );
    gf_sc_add_audio_listener ( avr->term->compositor, &avr->audio_listen );
    samplesReaden = ctx->frame_size * ctx->channels;

    //printf("SETUP input sample size=%u, output samplesize=%u, buffsize=%u, samplesReaden=%u\n", ctx->input_sample_size, ctx->frame_size, sizeof(outbuf), samplesReaden);
    // 2 chars are needed for each short
    toRead = samplesReaden * 2;
    inBuff = malloc(toRead * sizeof(u_int8_t));
    outBuff = malloc(toRead * sizeof(u_int8_t));
    while (avr->is_running && !avr->audioCurrentTime) {
        gf_sleep(1);
    }
    myTime = lastCurrentTime = avr->audioCurrentTime;
    frameCountSinceReset = 0;
    while (avr->is_running) {
        u32 readen;
        if (avr->audioCurrentTime != lastCurrentTime && U32_ABS(avr->audioCurrentTime, myTime) > 200) {
            GF_LOG( GF_LOG_ERROR, GF_LOG_MODULE, ("[AVRedirect] Drift in audio : audioCurrentTime = %u, myTime=%u, resync...\n", avr->audioCurrentTime, myTime));
            myTime = lastCurrentTime = avr->audioCurrentTime;
            frameCountSinceReset = 0;
            sendPts = 1;
        }
        while (toRead <= gf_ringbuffer_available_for_read(avr->pcmAudio) ) {
            memset( inBuff, 0, toRead);
            memset( outBuff, 0, toRead);
            readen = gf_ringbuffer_read(avr->pcmAudio, inBuff, toRead);
            //memset( inBuff, 0, toRead);
            assert( readen == toRead );
            if (avr->encode)
            {
                /* buf_size * input_sample_size / output_sample_size */
                int encoded = avcodec_encode_audio(ctx, outBuff, toRead, (const short *) inBuff);
                if (encoded < 0) {
                    GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[RedirectAV]: failed to encode audio\n"));
                } else if (encoded > 0) {
                    //if ( ctx->coded_frame->pts == AV_NOPTS_VALUE)
                    //  ctx->coded_frame->pts = myTime;
                    //printf("PTS="LLU"\n", ctx->coded_frame->pts);
                    ts_encode_audio_frame(avr->ts_implementation, outBuff, encoded, sendPts ? myTime : AV_NOPTS_VALUE );
                    frameCountSinceReset++;
#ifdef DUMP_MP3
                    fwrite( outBuff, sizeof(char), encoded, mp3);
#endif /* DUMP_MP3 */
		    if (ctx->codec->id == CODEC_ID_MP3){ 
		      /* It seems the MP4 codec only fetches 50% of data... */
		      myTime= lastCurrentTime + (frameCountSinceReset * ctx->frame_size * 500 / ctx->sample_rate);
		    } else
		      myTime= lastCurrentTime + (frameCountSinceReset * ctx->frame_size * 1000 / ctx->sample_rate);
                    //sendPts = 0;
                    sendPts = 1;
                } else {
                    GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[RedirectAV]: no encoded frame.\n"));
                    frameCountSinceReset++;
                }
            }
        }
        gf_sleep(1);
    }
    gf_free( inBuff );
    gf_free( outBuff );
#ifdef DUMP_MP3
    fclose(mp3);
#endif /* DUMP_MP3 */
    if (avr->term)
        gf_sc_remove_audio_listener ( avr->term->compositor, &avr->audio_listen );
    return GF_OK;
}

/**
 * This thread sends the frame to TS mux
 * \param Parameter The GF_AVRedirect pointer
 */
static Bool video_encoding_thread_run(void *param)
{
    GF_AVRedirect * avr = (GF_AVRedirect*) param;
    u32 currentFrameTimeProcessed = 0;
    u32 lastEncodedFrameTime = 0;
    assert( avr );
    gf_sc_add_video_listener ( avr->term->compositor, &avr->video_listen );
    AVCodecContext * ctx = ts_get_video_codec_context(avr->ts_implementation);
    printf("******* Video Codec Context = %d/%d, start="LLU"\n", ctx->time_base.num, ctx->time_base.den, ctx->timecode_frame_start);
    while (avr->is_running) {
        {
            gf_mx_p(avr->frameMutex);
            while (!avr->frameTime || currentFrameTimeProcessed == avr->frameTime) {
                gf_mx_v(avr->frameMutex);
                if (!avr->is_running) {
                    return 0;
                }
                gf_sleep(1);
                gf_mx_p(avr->frameMutex);
            }
            assert( currentFrameTimeProcessed != avr->frameTime);
            currentFrameTimeProcessed = avr->frameTime;
            {
                avpicture_fill ( ( AVPicture * ) avr->RGBpicture, avr->frame, PIX_FMT_RGB24, avr->srcWidth, avr->srcHeight );
                sws_scale ( avr->swsContext,
                            ( const uint8_t * const * ) avr->RGBpicture->data, avr->RGBpicture->linesize,
                            0, avr->srcHeight,
                            avr->YUVpicture->data, avr->YUVpicture->linesize );
#ifdef AVR_DUMP_RAW_AVI
                if ( AVI_write_frame ( avr->avi_out, avr->frame, avr->size, 1 ) <0 )
                {
                    GF_LOG ( GF_LOG_ERROR, GF_LOG_MODULE, ( "[AVRedirect] Error writing video frame\n" ) );
                }
#endif /* AVR_DUMP_RAW_AVI */
                gf_mx_v(avr->frameMutex);
                if (avr->encode)
                {
                    int written;
                    //u32 sysclock = gf_sys_clock();
                    avr->YUVpicture->pts = currentFrameTimeProcessed;
                    //avr->YUVpicture->pts = currentFrameTimeProcessed;
                    //avr->YUVpicture->coded_picture_number++;
                    //avr->YUVpicture->display_picture_number++;
                    //printf("Encoding frame PTS="LLU", frameNum=%u, time=%u...", avr->YUVpicture->pts, avr->YUVpicture->coded_picture_number, currentFrameTimeProcessed);
                    written = avcodec_encode_video ( ctx, avr->videoOutbuf, outbuf_size, avr->YUVpicture );
                    //ctx->coded_frame->pts = currentFrameTimeProcessed;
                    if ( written < 0 )
                    {
                        GF_LOG ( GF_LOG_ERROR, GF_LOG_MODULE, ( "[AVRedirect] Error while encoding video frame =%d\n", written ) );
                    } else
                        if ( written > 0 )
                        {
                            ts_encode_video_frame(avr->ts_implementation, avr->videoOutbuf, written);
                        }
                    lastEncodedFrameTime = currentFrameTimeProcessed;
                }
            }
        }
        avr->frameTimeEncoded = currentFrameTimeProcessed;
	gf_sleep(5);
    } /* End of main loop */
    if (avr->term)
        gf_sc_remove_video_listener ( avr->term->compositor, &avr->video_listen );
    return 0;
}

#ifdef MULTITHREAD_REDIRECT_AV
/**
 * This thread sends the TS packets over the network
 * \param Parameter The GF_AVRedirect pointer
 */
static Bool ts_thread_run(void *param)
{
    GF_Err e;
    GF_AVRedirect * avr = (GF_AVRedirect*) param;
    assert( avr );
    gf_mx_p(avr->tsMutex);
    while (!avr->frameTimeEncoded && avr->is_running) {
        gf_mx_v(avr->tsMutex);
        gf_sleep(1);
        gf_mx_p(avr->tsMutex);
    }
    printf("avr->frameTimeEncoded="LLU"\n", avr->frameTimeEncoded);

    while (avr->is_running) {
        e = sendTSMux(avr);
        gf_mx_v(avr->tsMutex);
        gf_mx_p(avr->tsMutex);
    }
    gf_mx_v(avr->tsMutex);
    return 0;
}
#endif /* MULTITHREAD_REDIRECT_AV */

#define VIDEO_RATE 400000

static const char * possibleCodecs = "supported codecs : MPEG-1, MPEG-2, MPEG-4, MSMPEG-4, H263, H263I, H263P, RV10, MJPEG";

/*#define DEFAULT_UDP_OUT_ADDRESS "127.0.0.1"
#define DEFAULT_UDP_OUT_PORT 1234
#define DEFAULT_UDP_OUT_PORT_STR "1234"*/

#define AVR_DEFAULT_DESTINATION "udp://224.0.0.1:1234"

#include <libavutil/pixfmt.h>

static GF_Err avr_open ( GF_AVRedirect *avr )
{
//    GF_Err e;
    enum PixelFormat pxlFormatForCodec = PIX_FMT_YUV420P;
    if ( avr->is_open ) return GF_OK;
#ifdef AVR_DUMP_RAW_AVI
    avr->avi_out = AVI_open_output_file ( "dump.avi" );
    if ( !avr->avi_out )
    {
        GF_LOG ( GF_LOG_ERROR, GF_LOG_MODULE, ( "[AVRedirect] Error opening output AVI file\n" ) );
        return GF_IO_ERR;
    }
#endif /* AVR_DUMP_RAW_AVI */
    avr->is_open = 1;
    if (!avr->pcmAudio)
        avr->pcmAudio = gf_ringbuffer_new(32768);

    /* Setting up the video encoding ... */
    {
        avr->YUVpicture = avr->RGBpicture = NULL;
        avr->swsContext = NULL;
        avr->srcWidth = outputWidth;
        avr->srcHeight = outputHeight;
        avr->videoOutbuf = NULL;
        if ( !avr->audioCodec)
        {
            GF_LOG ( GF_LOG_ERROR, GF_LOG_MODULE, ( "[AVRedirect] Cannot find audio codec.\n" ) );
            return GF_CODEC_NOT_FOUND;
        }

        if ( !avr->videoCodec )
        {
            GF_LOG ( GF_LOG_ERROR, GF_LOG_MODULE, ( "[AVRedirect] Cannot find video codec.\n" ) );
            return GF_CODEC_NOT_FOUND;
        }

        if (avr->videoCodec->id == CODEC_ID_MJPEG) {
            pxlFormatForCodec = PIX_FMT_YUVJ420P;
        }

        avr->RGBpicture = avcodec_alloc_frame();
        assert ( avr->RGBpicture );
        avr->RGBpicture->data[0] = NULL;
        avr->YUVpicture = avcodec_alloc_frame();
        assert ( avr->YUVpicture );
        {
            u32 sz = sizeof ( uint8_t ) * avpicture_get_size ( pxlFormatForCodec, outputWidth, outputHeight );
            avr->yuv_data = av_malloc ( sz ); /* size for YUV 420 */
            assert ( avr->yuv_data );
            memset ( avr->yuv_data, 0, sz );
            avpicture_fill ( ( AVPicture* ) avr->YUVpicture, avr->yuv_data, pxlFormatForCodec, outputWidth, outputHeight );
            avr->YUVpicture->coded_picture_number = 0;
        }
        avr->videoOutbuf = gf_malloc ( outbuf_size );
        memset ( avr->videoOutbuf, 0, outbuf_size );
    }
#ifdef AVR_DUMP_RAW_AVI
    GF_LOG ( GF_LOG_INFO, GF_LOG_MODULE, ( "[AVRedirect] Open output AVI file %s\n", "dump.avi" ) );
#endif /* AVR_DUMP_RAW_AVI */
    /* Setting up the TS mux */
    {
      /*
        GF_Socket * ts_output_udp_sk = gf_sk_new ( GF_SOCK_TYPE_UDP );
        if ( gf_sk_is_multicast_address ( avr->udp_address ) )
        {
            e = gf_sk_setup_multicast ( ts_output_udp_sk, avr->udp_address, avr->udp_port, 0, 0, NULL );
        }
        else
        {
            e = gf_sk_bind ( ts_output_udp_sk, NULL, avr->udp_port, avr->udp_address, avr->udp_port, GF_SOCK_REUSE_PORT );
        }
        gf_sk_set_buffer_size(ts_output_udp_sk, 0, 188 * TS_PACKETS_PER_UDP_PACKET);
        gf_sk_set_block_mode(ts_output_udp_sk, 0);*/
	assert( avr->destination );
        avr->ts_implementation = ts_amux_new(avr, VIDEO_RATE, audioCodecBitrate);
    }
/*
    if ( e )
    {
        GF_LOG ( GF_LOG_ERROR, GF_LOG_MODULE, ( "[AVRedirect] Error while initializing UDP address %s\n", gf_error_to_string ( e ) ) );
        return e;
    }
*/
    avr->is_running = 1;
    gf_th_run(avr->encodingThread, video_encoding_thread_run, avr);
    gf_th_run(avr->audioEncodingThread, audio_encoding_thread_run, avr);
    return GF_OK;
}

static void avr_close ( GF_AVRedirect *avr )
{
    if ( !avr->is_open ) return;
    avr->is_open = 0;
#ifdef AVR_DUMP_RAW_AVI
    GF_LOG ( GF_LOG_INFO, GF_LOG_MODULE, ( "[AVRedirect] Closing output AVI file\n" ) );
    AVI_close ( avr->avi_out );
    avr->avi_out = NULL;
#endif /* AVR_DUMP_RAW_AVI */
}

static Bool avr_on_event ( void *udta, GF_Event *evt, Bool consumed_by_compositor )
{
    GF_AVRedirect *avr = udta;
    switch ( evt->type )
    {
    case GF_EVENT_CONNECT:
        if ( evt->connect.is_connected )
        {
            avr_open ( avr );
        }
        else
        {
            avr_close ( avr );
        }
        break;
    }
    return 0;
}

static const char * AVR_ENABLED_OPTION = "Enabled";

//static const char * AVR_UDP_PORT_OPTION = "udp.port";

//static const char * AVR_UDP_ADDRESS_OPTION = "udp.address";

static const char * AVR_DESTINATION = "destination";

static Bool avr_process ( GF_TermExt *termext, u32 action, void *param )
{
    const char *opt;
    GF_AVRedirect *avr = termext->udta;

    switch ( action )
    {
    case GF_TERM_EXT_START:
        avr->term = ( GF_Terminal * ) param;
        opt = gf_modules_get_option ( ( GF_BaseInterface* ) termext, moduleName, AVR_ENABLED_OPTION );
        if ( !opt )
            gf_modules_set_option ( ( GF_BaseInterface* ) termext, moduleName, AVR_ENABLED_OPTION, "no" );
        if ( !opt || strcmp ( opt, "yes" ) ) return 0;
        opt = gf_modules_get_option ( ( GF_BaseInterface* ) termext, moduleName, AVR_VIDEO_CODEC_OPTION );
        avr->globalLock = gf_global_resource_lock("AVRedirect:output");
        if (!avr->globalLock) {
            GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("Failed to lock globale resource, another GPAC instance must be running, disabling AVRedirect\n"));
            return 0;
        }
        /* must be called before using avcodec lib */
        avcodec_init();

        /* register all the codecs */
        avcodec_register_all();
        if ( !opt )
            gf_modules_set_option ( ( GF_BaseInterface* ) termext, moduleName, AVR_VIDEO_CODEC_OPTION, possibleCodecs );
        {
            if ( !opt )
            {
                avr->videoCodec = avcodec_find_encoder ( CODEC_ID_MPEG2VIDEO );
            }
            else if ( !strcmp ( "MPEG-1", opt ) )
            {
                avr->videoCodec=avcodec_find_encoder ( CODEC_ID_MPEG1VIDEO );
            }
            else if ( !strcmp ( "MPEG-2", opt ) )
            {
                avr->videoCodec=avcodec_find_encoder ( CODEC_ID_MPEG2VIDEO );
            }
            else if ( !strcmp ( "MPEG-4", opt ) )
            {
                avr->videoCodec=avcodec_find_encoder ( CODEC_ID_MPEG4 );
            }
            else if ( !strcmp ( "MSMPEG-4", opt ) )
            {
                avr->videoCodec=avcodec_find_encoder ( CODEC_ID_MSMPEG4V3 );
            }
            else if ( !strcmp ( "H263", opt ) )
            {
                avr->videoCodec=avcodec_find_encoder ( CODEC_ID_H263 );
            }
            else if ( !strcmp ( "RV10", opt ) )
            {
                avr->videoCodec = avcodec_find_encoder ( CODEC_ID_RV10 );
            }
            else if ( !strcmp ( "H263P", opt ) )
            {
                avr->videoCodec=avcodec_find_encoder ( CODEC_ID_H263P );
            }
            else if ( !strcmp ( "H263I", opt ) )
            {
                avr->videoCodec=avcodec_find_encoder ( CODEC_ID_H263I );
            }
            else if ( !strcmp( "MJPEG", opt))
            {
                avr->videoCodec=avcodec_find_encoder( CODEC_ID_MJPEG);
            }
            else
            {
                GF_LOG ( GF_LOG_ERROR, GF_LOG_MODULE, ( "[AVRedirect] Not a known Video codec : %s, using MPEG-2 video instead, %s\n", opt, possibleCodecs ) );
                avr->videoCodec = avcodec_find_encoder ( CODEC_ID_MPEG2VIDEO );
            }
        }
        avr->audioCodec = avcodec_find_encoder( CODEC_ID_MP3);
/*
        opt = gf_modules_get_option ( ( GF_BaseInterface* ) termext, moduleName, AVR_UDP_ADDRESS_OPTION);
        if (!opt) {
            gf_modules_set_option ( ( GF_BaseInterface* ) termext, moduleName, AVR_UDP_ADDRESS_OPTION, DEFAULT_UDP_OUT_ADDRESS);
            avr->udp_address = DEFAULT_UDP_OUT_ADDRESS;
            GF_LOG ( GF_LOG_ERROR, GF_LOG_MODULE, ( "[AVRedirect] %s not set, using %s\n", AVR_UDP_ADDRESS_OPTION, DEFAULT_UDP_OUT_ADDRESS ) );
        } else
            avr->udp_address = opt;
        opt = gf_modules_get_option ( ( GF_BaseInterface* ) termext, moduleName, AVR_UDP_PORT_OPTION);
        if (opt) {
            char *endPtr = NULL;
            unsigned int x = strtoul(opt, &endPtr, 10);
            if (endPtr == opt || x > 65536) {
                printf("Failed to parse : %s\n", opt);
                opt = NULL;
            } else
                avr->udp_port = (u16) x;
        }
        if (!opt) {
            gf_modules_set_option ( ( GF_BaseInterface* ) termext, moduleName, AVR_UDP_PORT_OPTION, DEFAULT_UDP_OUT_PORT_STR);
            GF_LOG ( GF_LOG_ERROR, GF_LOG_MODULE, ( "[AVRedirect] %s is not set or valid, using %s\n", AVR_UDP_PORT_OPTION, DEFAULT_UDP_OUT_PORT_STR ) );
            avr->udp_port = DEFAULT_UDP_OUT_PORT;
        }
*/
	opt = gf_modules_get_option ( ( GF_BaseInterface* ) termext, moduleName, AVR_DESTINATION);
        if (!opt) {
            gf_modules_set_option ( ( GF_BaseInterface* ) termext, moduleName, AVR_DESTINATION, AVR_DEFAULT_DESTINATION);
            avr->destination = AVR_DEFAULT_DESTINATION;
            GF_LOG ( GF_LOG_ERROR, GF_LOG_MODULE, ( "[AVRedirect] %s not set, using %s\n", AVR_DESTINATION, AVR_DEFAULT_DESTINATION ) );
        } else
            avr->destination = opt;
        avr->audio_listen.udta = avr;
        avr->audio_listen.on_audio_frame = avr_on_audio_frame;
        avr->audio_listen.on_audio_reconfig = avr_on_audio_reconfig;
        avr->video_listen.udta = avr;
        avr->video_listen.on_video_frame = avr_on_video_frame;
        avr->video_listen.on_video_reconfig = avr_on_video_reconfig;

        avr->term_listen.udta = avr;
        avr->term_listen.on_event = avr_on_event;
        gf_term_add_event_filter ( avr->term, &avr->term_listen );
        return 1;

    case GF_TERM_EXT_STOP:
        gf_term_remove_event_filter ( avr->term, &avr->term_listen );
        avr->term = NULL;
        break;

        /*if not threaded, perform our tasks here*/
    case GF_TERM_EXT_PROCESS:
        break;
    }
    return 0;
}


GF_TermExt *avr_new()
{
    GF_TermExt *dr;
    GF_AVRedirect *uir;
    dr = gf_malloc ( sizeof ( GF_TermExt ) );
    memset ( dr, 0, sizeof ( GF_TermExt ) );
    GF_REGISTER_MODULE_INTERFACE ( dr, GF_TERM_EXT_INTERFACE, "GPAC Output Recorder", "gpac distribution" );

    GF_SAFEALLOC ( uir, GF_AVRedirect );
    dr->process = avr_process;
    dr->udta = uir;
    uir->encodingMutex = gf_mx_new("RedirectAV_encodingMutex");
    assert( uir->encodingMutex);
    uir->frameMutex = gf_mx_new("RedirectAV_frameMutex");
    uir->encodingThread = gf_th_new("RedirectAV_EncodingThread");
    uir->audioEncodingThread = gf_th_new("RedirectAV_AudioEncodingThread");
    uir->encode = 1;
    return dr;
}


void avr_delete ( GF_BaseInterface *ifce )
{
    GF_TermExt *dr = ( GF_TermExt * ) ifce;
    GF_AVRedirect *avr = dr->udta;
    avr->is_running = 0;
    /* Ensure encoding is finished */
    gf_mx_p(avr->frameMutex);
    gf_mx_v(avr->frameMutex);
    gf_sleep(200);
    gf_th_stop(avr->encodingThread);
    gf_mx_del(avr->frameMutex);
    avr->frameMutex = NULL;
    gf_th_del(avr->encodingThread);
    avr->encodingThread = NULL;
    gf_mx_del(avr->encodingMutex);
    avr->encodingMutex = NULL;
    if ( avr->ts_implementation )
    {
        ts_amux_del(avr->ts_implementation);
        avr->ts_implementation = NULL;
    }
    avr->videoCodec = NULL;
    if ( avr->YUVpicture )
    {
        av_free ( avr->YUVpicture );
    }
    if ( avr->yuv_data )
        av_free ( avr->yuv_data );
    avr->yuv_data = NULL;
    avr->YUVpicture = NULL;
    if ( avr->RGBpicture )
    {
        av_free ( avr->RGBpicture );
    }
    avr->RGBpicture = NULL;
    if ( avr->swsContext )
        sws_freeContext ( avr->swsContext );
    avr->swsContext = NULL;
    if ( avr->videoOutbuf )
        gf_free ( avr->videoOutbuf );
    avr->videoOutbuf = NULL;
    if ( avr->pcmAudio )
        gf_ringbuffer_del(avr->pcmAudio);
    avr->pcmAudio = NULL;
    gf_global_resource_unlock(avr->globalLock);
    avr->globalLock = NULL;
    gf_free ( avr );
    gf_free ( dr );
    dr->udta = NULL;
}

GF_EXPORT
const u32 *QueryInterfaces()
{
    static u32 si [] =
    {
        GF_TERM_EXT_INTERFACE,
        0
    };
    return si;
}

GF_EXPORT
GF_BaseInterface *LoadInterface ( u32 InterfaceType )
{
    if ( InterfaceType == GF_TERM_EXT_INTERFACE ) return ( GF_BaseInterface * ) avr_new();
    return NULL;
}

GF_EXPORT
void ShutdownInterface ( GF_BaseInterface *ifce )
{
    switch ( ifce->InterfaceType )
    {
    case GF_TERM_EXT_INTERFACE:
        avr_delete ( ifce );
        break;
    }
}
