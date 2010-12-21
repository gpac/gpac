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

#include <gpac/modules/term_ext.h>
#include <gpac/internal/terminal_dev.h>
#include <gpac/internal/compositor_dev.h>
#include <stdlib.h>


#if (defined(WIN32) || defined(_WIN32_WCE)) && !defined(__MINGW32__)

#define EMULATE_INTTYPES
#define EMULATE_FAST_INT
#ifndef inline
#define inline __inline
#endif

#if defined(__SYMBIAN32__)
#define EMULATE_INTTYPES
#endif


#ifndef __MINGW32__
#define __attribute__(s)
#endif

#endif

#define TS_MUX_MODE_PUT

#undef AVR_DUMP_RAW_AVI
#define AVR_DUMP_MPEG_RAW
//#undef AVR_DUMP_MPEG_RAW

#ifdef AVR_DUMP_RAW_AVI
/*sample raw AVI writing*/
#include <gpac/internal/avilib.h>
#endif /* AVR_DUMP_RAW_AVI */

#include <gpac/mpegts.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>

#define MULTITHREAD_REDIRECT_AV
//#undef MULTITHREAD_REDIRECT_AV

/* This number * 188 should be lower than the UDP packet size */
#define TS_PACKETS_PER_UDP_PACKET 1

#define outputWidth 320

#define outputHeight 240

#define audioCodecBitrate 96000

static const u32 maxFPS = 25;

static const u32 outbuf_size = outputWidth * outputHeight * 4;

typedef struct
{
    GF_Terminal *term;

    Bool is_open;
    GF_AudioListener audio_listen;
    GF_VideoListener video_listen;
    GF_TermEventFilter term_listen;
#ifdef AVR_DUMP_RAW_AVI
    avi_t *avi_out;
#endif
    char *frame;
    u32 size;
    GF_M2TS_Mux *muxer;
    GF_ESIPacket videoCurrentTSPacket;
    GF_ESIPacket audioCurrentTSPacket;
    GF_Socket * ts_output_udp_sk;
    Bool encode;
    AVCodec *audioCodec;
    AVCodecContext *audioCodecContext;
    AVCodec *videoCodec;
    AVCodecContext *videoCodecContext;
    AVFrame *YUVpicture, *RGBpicture;
    struct SwsContext * swsContext;
    uint8_t * yuv_data;
    u32 srcWidth;
    u32 srcHeight;
    uint8_t * videoOutbuf;
    uint8_t * audioOutBuf;
    uint8_t * audioInBuf;
    u32 audioInBufferSize;
    u32 audioOutBufferSize;
    u32 audioCurrentTime;
#ifdef AVR_DUMP_MPEG_RAW
    FILE * mpeg_OUTFile;
    FILE * ts_OUTFile;
#endif
    GF_ESInterface * video, * audio;
    GF_Thread * encodingThread;
    GF_Thread * audioEncodingThread;
#ifdef MULTITHREAD_REDIRECT_AV
    GF_Mutex * tsMutex;
    GF_Thread * tsThread;
#endif
    GF_Mutex * frameMutex;
    GF_Mutex * audioMutex;
    GF_Mutex * encodingMutex;
    volatile Bool is_running;
    u64 frameTime;
    u64 frameTimeEncoded;
    u64 frameTimeSentOverTS;
    const char * udp_address;
    u16 udp_port;
} GF_AVRedirect;


static GF_Err void_input_ctrl(GF_ESInterface *ifce, u32 act_type, void *param)
{
    GF_AVRedirect *avr = (GF_AVRedirect *)  ifce->input_udta;
    if (!avr)
        return GF_BAD_PARAM;
    switch (act_type) {
    case GF_ESI_INPUT_DATA_FLUSH:
        break;
    case GF_ESI_INPUT_DESTROY:
        break;
    case GF_ESI_INPUT_DATA_PULL:
        gf_mx_p(avr->encodingMutex);
        if (avr->frameTimeEncoded > avr->frameTimeSentOverTS) {
            //printf("Data PULL, avr=%p, avr->video=%p, encoded="LLU", sent over TS="LLU"\n", avr, &avr->video, avr->frameTimeEncoded, avr->frameTimeSentOverTS);
            avr->video->output_ctrl( avr->video, GF_ESI_OUTPUT_DATA_DISPATCH, &(avr->videoCurrentTSPacket));
            avr->frameTimeSentOverTS = avr->frameTime;
        } else {
            //printf("Data PULL IGNORED : encoded = "LLU", sent on TS="LLU"\n", avr->frameTimeEncoded, avr->frameTimeSentOverTS);
        }
        gf_mx_v(avr->encodingMutex);
        break;
    case GF_ESI_INPUT_DATA_RELEASE:
        break;
    default:
        printf("Asking unknown : %u\n", act_type);
    }
    return GF_OK;
}

static const char * moduleName = "AVRedirect";

static const char * AVR_VIDEO_CODEC_OPTION = "VideoCodec";

static void avr_on_audio_frame ( void *udta, char *buffer, u32 buffer_size, u32 time, u32 delay_ms )
{
    GF_AVRedirect *avr = ( GF_AVRedirect * ) udta;
#ifdef AVR_DUMP_RAW_AVI
    AVI_write_audio ( avr->avi_out, buffer, buffer_size );
#endif /* AVR_DUMP_RAW_AVI */
    {
        gf_mx_p(avr->audioMutex);
        if (avr->audioInBufferSize != buffer_size) {
            avr->audioInBufferSize = buffer_size;
            avr->audioInBuf = realloc(avr->audioInBuf, buffer_size);
        }
        //assert( avr->audioReadPosition + buffer_size <= avr->audioInBufSize);
        memcpy(avr->audioInBuf, buffer, buffer_size);
        avr->audioCurrentTime = time - delay_ms;
        //avr->audioReadPosition+=buffer_size;
        gf_mx_v(avr->audioMutex);
    }
}

static void avr_on_audio_reconfig ( void *udta, u32 samplerate, u32 bits_per_sample, u32 nb_channel, u32 channel_cfg )
{
#ifdef AVR_DUMP_RAW_AVI
    GF_AVRedirect *avr = ( GF_AVRedirect * ) udta;
    AVI_set_audio ( avr->avi_out, nb_channel, samplerate, bits_per_sample, WAVE_FORMAT_PCM, 0 );
#endif /* AVR_DUMP_RAW_AVI */
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
    //memcpy(avr->frame, fb.video_buffer, avr->size);
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

/*!
 * Sends the TS mux to socket
 * \param avr The AVRedirect structure pointer
 */
static GF_Err sendTSMux(GF_AVRedirect * avr)
{
    u32 status;
    const char * pkt;
    GF_Err e;
    u32 padding, data;
    padding = data = 0;
    while ( (NULL!= ( pkt = gf_m2ts_mux_process ( avr->muxer, &status ))) && avr->is_running)
    {
        switch (status) {
        case GF_M2TS_STATE_IDLE:
            break;
        case GF_M2TS_STATE_DATA:
            data+=188;
            break;
        case GF_M2TS_STATE_PADDING:
            padding+=188;
            break;
        default:
            break;
        }
#ifdef AVR_DUMP_MPEG_RAW
        if (avr->ts_OUTFile)
            if (188 != fwrite(pkt, sizeof(char), 188, avr->ts_OUTFile ))
                return GF_IO_ERR;
#endif /* AVR_DUMP_MPEG_RAW */
        if (avr->ts_output_udp_sk) {
            e = gf_sk_send ( avr->ts_output_udp_sk, pkt, 188);
            if ( e )
            {
                GF_LOG ( GF_LOG_ERROR, GF_LOG_MODULE, ( "[AVRedirect] Unable to send TS data : %s (%s:%hu)\n", gf_error_to_string(e), avr->udp_address, avr->udp_port) );
                return e;
            }
        }
    }
    if (data || padding)
        GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[AVRedirect] : Sent TS data=%u/padding=%u\n", data, padding));
    return GF_OK;
}

#define AVR_TIMERS_LENGHT 25

/**
 * This thread sends the frame to TS mux
 * \param Parameter The GF_AVRedirect pointer
 */
static Bool audio_encoding_thread_run(void *param)
{
    char buff[64000];
    u32 myTime = 0;
    GF_AVRedirect * avr = (GF_AVRedirect*) param;
    assert( avr );
    gf_sc_add_audio_listener ( avr->term->compositor, &avr->audio_listen );
#ifdef MULTITHREAD_REDIRECT_AV
    gf_mx_p(avr->tsMutex);
#endif /* MULTITHREAD_REDIRECT_AV */
    while (avr->is_running) {
#ifdef MULTITHREAD_REDIRECT_AV
        gf_mx_v(avr->tsMutex);
#endif /* MULTITHREAD_REDIRECT_AV */
        gf_mx_p(avr->audioMutex);
        if (myTime == avr->audioCurrentTime) {
            /* Current frame has already been encoded, we skip */
            gf_mx_v(avr->audioMutex);
        } else {
            memcpy(buff, avr->audioInBuf, avr->audioInBufferSize);
            myTime = avr->audioCurrentTime;
            gf_mx_v(avr->audioMutex);
            if (avr->encode)
            {
                int encoded = avcodec_encode_audio(avr->audioCodecContext, avr->audioOutBuf, avr->audioOutBufferSize, (const short *) buff);
                if (encoded < 0) {
                    GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[RedirectAV]: failed to encode audio\n"));
                } else if (encoded > 0) {
                    avr->audioCurrentTSPacket.data = avr->audioOutBuf;
                    avr->audioCurrentTSPacket.data_len = encoded;
                    avr->audioCurrentTSPacket.flags = GF_ESI_DATA_AU_START|GF_ESI_DATA_AU_END | GF_ESI_DATA_HAS_CTS | GF_ESI_DATA_HAS_DTS;
                    avr->audioCurrentTSPacket.cts = avr->audioCurrentTSPacket.dts = myTime;
                    avr->audio->output_ctrl(avr->audio, GF_ESI_OUTPUT_DATA_DISPATCH, &(avr->audioCurrentTSPacket));
                }
            }
        }
#ifdef MULTITHREAD_REDIRECT_AV
        gf_mx_p(avr->tsMutex);
#endif /* MULTITHREAD_REDIRECT_AV */
    }
#ifdef MULTITHREAD_REDIRECT_AV
    gf_mx_v(avr->tsMutex);
#endif /* MULTITHREAD_REDIRECT_AV */
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
    //u32 ts_packets_sent = 0;
    u32 currentFrameTimeProcessed = 0;
    u32 timers[AVR_TIMERS_LENGHT];
    u32 currentFrameEncoded = 0;
    u32 lastEncodedFrameTime = 0;
    memset(timers, 0, AVR_TIMERS_LENGHT * sizeof(u32));
    assert( avr );
    gf_sc_add_video_listener ( avr->term->compositor, &avr->video_listen );
#ifdef MULTITHREAD_REDIRECT_AV
    gf_mx_p(avr->tsMutex);
#endif /* MULTITHREAD_REDIRECT_AV */
    while (avr->is_running) {
#ifdef MULTITHREAD_REDIRECT_AV
        gf_mx_v(avr->tsMutex);
#endif /* MULTITHREAD_REDIRECT_AV */
        {
            gf_mx_p(avr->frameMutex);
            while (!avr->frameTime || currentFrameTimeProcessed == avr->frameTime) {
                gf_mx_v(avr->frameMutex);
#ifdef MULTITHREAD_REDIRECT_AV
                gf_mx_p(avr->tsMutex);
#endif /* MULTITHREAD_REDIRECT_AV */
                if (!avr->is_running) {
#ifdef MULTITHREAD_REDIRECT_AV
                    gf_mx_v(avr->tsMutex);
#endif /* MULTITHREAD_REDIRECT_AV */
                    return 0;
                }
#ifdef MULTITHREAD_REDIRECT_AV
                gf_mx_v(avr->tsMutex);
#endif /* MULTITHREAD_REDIRECT_AV */
//                gf_sleep(1);
                gf_mx_p(avr->frameMutex);
            }
            assert( currentFrameTimeProcessed != avr->frameTime);
            currentFrameTimeProcessed = avr->frameTime;
//           u32 delta = (currentFrameTimeProcessed - lastEncodedFrameTime);
//           if (delta < (1000/maxFPS)) {
//               gf_mx_v(avr->frameMutex);
//#ifdef MULTITHREAD_REDIRECT_AV
//                gf_mx_p(avr->tsMutex);
//#endif /* MULTITHREAD_REDIRECT_AV */
//                continue;
//            }
            {
                u32 currentTimer = currentFrameEncoded % AVR_TIMERS_LENGHT;
                u32 fps = timers[currentTimer] ? 1000 * AVR_TIMERS_LENGHT / (currentFrameTimeProcessed - timers[currentTimer]) : maxFPS;

                timers[currentTimer++] = currentFrameTimeProcessed;

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
                    assert ( avr->videoCodecContext );
                    //avr->YUVpicture->pts = sysclock;
		    avr->YUVpicture->pts = currentFrameTimeProcessed;
                    avr->YUVpicture->coded_picture_number++;
                    avr->YUVpicture->display_picture_number++;
                    //printf("Encoding frame PTS="LLU", frameNum=%u, time=%u...", avr->YUVpicture->pts, avr->YUVpicture->coded_picture_number, currentFrameTimeProcessed);
                    written = avcodec_encode_video ( avr->videoCodecContext, avr->videoOutbuf, outbuf_size, avr->YUVpicture );
                    if ( written < 0 )
                    {
                        GF_LOG ( GF_LOG_ERROR, GF_LOG_MODULE, ( "[AVRedirect] Error while encoding video frame =%d\n", written ) );
                    } else
                        if ( written > 0 )
                        {
#ifdef AVR_DUMP_MPEG_RAW
                            if ( avr->mpeg_OUTFile )
                            {
                                int writtenOnDisk = fwrite ( avr->videoOutbuf, sizeof ( char ), written, avr->mpeg_OUTFile );
                                if ( writtenOnDisk != written )
                                    GF_LOG ( GF_LOG_ERROR, GF_LOG_MODULE, ( "[AVRedirect] Error writing video frame on disk\n" ) );
                            }
#endif /* AVR_DUMP_MPEG_RAW */
                            if ( avr->muxer )
                            {
                                // Possible de l'enlever
#ifdef MULTITHREAD_REDIRECT_AV
                                //gf_sleep(1);
                                gf_mx_p(avr->tsMutex);
#endif /* MULTITHREAD_REDIRECT_AV */
                                gf_mx_p( avr->encodingMutex );
                                //  currentFrameTimeProcessed / 1000;  //
                                avr->videoCurrentTSPacket.dts = avr->videoCodecContext->coded_frame->coded_picture_number;
                                avr->videoCurrentTSPacket.cts = avr->videoCodecContext->coded_frame->display_picture_number;
                                //avr->videoCurrentTSPacket.dts = avr->videoCurrentTSPacket.cts = sysclock;
				avr->videoCurrentTSPacket.dts = avr->videoCurrentTSPacket.cts = currentFrameTimeProcessed;
				avr->videoCurrentTSPacket.data = avr->videoOutbuf;
                                avr->videoCurrentTSPacket.data_len = written;
                                printf("\rSending frame DTS="LLU", CTS="LLU", len=%u, FPS=%u, delta=%u...", avr->videoCurrentTSPacket.dts, avr->videoCurrentTSPacket.cts, avr->videoCurrentTSPacket.data_len, fps, currentFrameTimeProcessed - lastEncodedFrameTime);
                                fflush(stdout);
                                currentFrameEncoded++;
                                avr->videoCurrentTSPacket.flags = GF_ESI_DATA_HAS_CTS | GF_ESI_DATA_HAS_DTS;
                                //if (ts_packets_sent == 0) {
                                //    printf("First Packet !\n");
                                avr->videoCurrentTSPacket.flags|=GF_ESI_DATA_AU_START|GF_ESI_DATA_AU_END ;
                                //}
                                avr->frameTimeEncoded = currentFrameTimeProcessed;
                                gf_mx_v( avr->encodingMutex );
#ifdef TS_MUX_MODE_PUT
                                void_input_ctrl(avr->video, GF_ESI_INPUT_DATA_PULL, NULL);
#endif
                                //avr->video->decoder_config = avr->videoCodecContext->

                                //avr->video->decoder_config = avr->outbuf;
                                //avr->video->decoder_config_size = written;
#ifdef MULTITHREAD_REDIRECT_AV
                                gf_mx_v(avr->tsMutex);
#else /* MULTITHREAD_REDIRECT_AV */
                                sendTSMux(avr);
#endif /* MULTITHREAD_REDIRECT_AV */
                            }

                        }
                    lastEncodedFrameTime = currentFrameTimeProcessed;
                }
            }
        }
        avr->frameTimeEncoded = currentFrameTimeProcessed;
#ifdef MULTITHREAD_REDIRECT_AV
        gf_mx_p(avr->tsMutex);
#endif /* MULTITHREAD_REDIRECT_AV */
    } /* End of main loop */
#ifdef MULTITHREAD_REDIRECT_AV
    gf_mx_v(avr->tsMutex);
#endif /* MULTITHREAD_REDIRECT_AV */
    if (avr->term)
        gf_sc_remove_video_listener ( avr->term->compositor, &avr->video_listen );
    return 0;
}

#ifdef MULTITHREAD_REDIRECT_AV
/**
 * This thread sends the TS packets over the network
 * \param Parameter The GF_AVRedirect pointer
 */
static Bool ts_thread_run(void *param) {
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

#define DEFAULT_UDP_OUT_ADDRESS "127.0.0.1"
#define DEFAULT_UDP_OUT_PORT 1234
#define DEFAULT_UDP_OUT_PORT_STR "1234"

#include <libavutil/pixfmt.h>

static GF_Err avr_open ( GF_AVRedirect *avr )
{
    GF_Err e;
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
#ifdef AVR_DUMP_MPEG_RAW
    avr->mpeg_OUTFile = fopen ( "dump.mpeg", "wb" );
    avr->ts_OUTFile = fopen ( "dump.ts", "wb" );
#endif
    avr->is_open = 1;

    /* Setting up the video encoding ... */
    {

        avr->videoCodecContext = NULL;
        avr->YUVpicture = avr->RGBpicture = NULL;
        avr->swsContext = NULL;
        avr->srcWidth = outputWidth;
        avr->srcHeight = outputHeight;
        avr->videoOutbuf = NULL;
        avr->audioOutBuf = NULL;

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

        avr->audioCodecContext = avcodec_alloc_context();
        assert( avr->audioCodecContext );
        avcodec_get_context_defaults( avr->audioCodecContext );
        avr->audioCodecContext->bit_rate = audioCodecBitrate;
        avr->audioCodecContext->sample_rate = 44100;
        avr->audioCodecContext->channels = 2;

        /* libavcodec setup */
        avr->videoCodecContext  =  avcodec_alloc_context();
        assert ( avr->videoCodecContext );
        avcodec_get_context_defaults ( avr->videoCodecContext );
        /* put sample parameters */
        avr->videoCodecContext->bit_rate = VIDEO_RATE;
        /* resolution must be a multiple of two */
        avr->videoCodecContext->width = outputWidth;
        avr->videoCodecContext->height = outputHeight;
        /* frames per second */
        avr->videoCodecContext->time_base.num = 1;
        if (avr->videoCodec->id == CODEC_ID_MPEG2VIDEO || avr->videoCodec->id == CODEC_ID_MPEG1VIDEO)
            avr->videoCodecContext->time_base.den = 25;
        else
            avr->videoCodecContext->time_base.den = 10;
        avr->videoCodecContext->gop_size = 0; /* emit one intra frame every ten frames */
        avr->videoCodecContext->max_b_frames=0;
        avr->videoCodecContext->pix_fmt = pxlFormatForCodec;
        avr->videoOutbuf = gf_malloc ( outbuf_size );
        avr->audioInBufferSize = 0;
        avr->audioOutBufferSize = 256000;
        avr->audioInBuf = NULL;
        avr->audioOutBuf = gf_malloc( avr->audioOutBufferSize );

        memset ( avr->videoOutbuf, 0, outbuf_size );
        memset (avr->audioOutBuf, 0, avr->audioOutBufferSize);
        /* open it */
        if ( avcodec_open ( avr->videoCodecContext, avr->videoCodec ) < 0 )
        {
            GF_LOG ( GF_LOG_ERROR, GF_LOG_MODULE, ( "[AVRedirect] Cannot open video codec.\n" ) );
            return GF_CODEC_NOT_FOUND;
        }

        if (avcodec_open ( avr->audioCodecContext, avr->audioCodec ) < 0 ) {
            GF_LOG ( GF_LOG_ERROR, GF_LOG_MODULE, ( "[AVRedirect] Cannot open audio codec.\n" ) );
            return GF_CODEC_NOT_FOUND;
        }

    }
#ifdef AVR_DUMP_RAW_AVI
    GF_LOG ( GF_LOG_INFO, GF_LOG_MODULE, ( "[AVRedirect] Open output AVI file %s\n", "dump.avi" ) );
#endif /* AVR_DUMP_RAW_AVI */
    /* Setting up the TS mux */
    avr->muxer = gf_m2ts_mux_new ( VIDEO_RATE + 10000, 0, 1 );

    avr->ts_output_udp_sk = gf_sk_new ( GF_SOCK_TYPE_UDP );
    if ( gf_sk_is_multicast_address ( avr->udp_address ) )
    {
        e = gf_sk_setup_multicast ( avr->ts_output_udp_sk, avr->udp_address, avr->udp_port, 0, 0, NULL );
    }
    else
    {
        e = gf_sk_bind ( avr->ts_output_udp_sk, NULL, avr->udp_port, avr->udp_address, avr->udp_port, GF_SOCK_REUSE_PORT );
    }
    gf_sk_set_buffer_size(avr->ts_output_udp_sk, 0, 188 * TS_PACKETS_PER_UDP_PACKET);
    gf_sk_set_block_mode(avr->ts_output_udp_sk, 0);
    if ( e )
    {
        GF_LOG ( GF_LOG_ERROR, GF_LOG_MODULE, ( "[AVRedirect] Error while initializing UDP address %s\n", gf_error_to_string ( e ) ) );
        return e;
    }

    {
        //u32 cur_pid = 100;	/*PIDs start from 100*/
        GF_M2TS_Mux_Program *program = gf_m2ts_mux_program_add ( avr->muxer, 1, 100, 0, 0 );

        avr->video = gf_malloc( sizeof( GF_ESInterface));
        memset( avr->video, 0, sizeof( GF_ESInterface));
        //audio.stream_id = 101;
        //gf_m2ts_program_stream_add ( program, &audifero, 101, 1 );
        avr->video->stream_id = 101;
        avr->video->stream_type = GF_STREAM_VISUAL;
        avr->video->bit_rate = 410000;
        /* ms resolution */
        avr->video->timescale = 1000;

        avr->video->object_type_indication = GPAC_OTI_VIDEO_MPEG2_SIMPLE;
        assert( avr->encodingMutex);
        avr->video->input_udta = avr;
#ifdef TS_MUX_MODE_PUT
        avr->video->caps = GF_ESI_SIGNAL_DTS;
#else
        avr->video->input_ctrl = void_input_ctrl;
        avr->video->caps = GF_ESI_AU_PULL_CAP | GF_ESI_SIGNAL_DTS;
#endif /* TS_MUX_MODE_PUT */


        avr->audio = gf_malloc( sizeof( GF_ESInterface));
        memset( avr->audio, 0, sizeof( GF_ESInterface));
        //audio.stream_id = 101;
        //gf_m2ts_program_stream_add ( program, &audio, 101, 1 );
        avr->audio->stream_id = 102;
        avr->audio->stream_type = GF_STREAM_AUDIO;
        avr->audio->bit_rate = audioCodecBitrate;
        /* ms resolution */
        avr->audio->timescale = 1000;
	
	avr->audio->object_type_indication = GPAC_OTI_AUDIO_AAC_MPEG2_MP;
        avr->audio->input_udta = avr;
#ifdef TS_MUX_MODE_PUT
        avr->audio->caps = GF_ESI_SIGNAL_DTS;
#else
        avr->audio->input_ctrl = void_input_ctrl;
        avr->audio->caps = GF_ESI_AU_PULL_CAP | GF_ESI_SIGNAL_DTS;
#endif /* TS_MUX_MODE_PUT */

        gf_m2ts_program_stream_add ( program, avr->video, 101, 1 );
        gf_m2ts_program_stream_add ( program, avr->audio, 102, 0 );

        assert( program->streams->mpeg2_stream_type = GF_M2TS_VIDEO_MPEG2);
        printf("Setup done, avr=%p, avr->video=%p\n", avr, &(avr->video));
    }
    gf_m2ts_mux_update_config ( avr->muxer, 1 );
#ifdef MULTITHREAD_REDIRECT_AV
    gf_mx_p(avr->tsMutex);
#endif /* MULTITHREAD_REDIRECT_AV */
    avr->is_running = 1;
#ifdef MULTITHREAD_REDIRECT_AV
    gf_mx_v(avr->tsMutex);
#endif /* MULTITHREAD_REDIRECT_AV */
#ifdef MULTITHREAD_REDIRECT_AV
    gf_th_run(avr->tsThread, ts_thread_run, avr);
#endif /* MULTITHREAD_REDIRECT_AV */
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

#ifdef AVR_DUMP_MPEG_RAW
    GF_LOG ( GF_LOG_INFO, GF_LOG_MODULE, ( "[AVRedirect] Closing MPEG files\n" ) );
    if ( avr->mpeg_OUTFile )
        fclose ( avr->mpeg_OUTFile );
    avr->mpeg_OUTFile = NULL;
    if ( avr->ts_OUTFile )
        fclose ( avr->ts_OUTFile );
    avr->ts_OUTFile = NULL;
#endif /* AVR_DUMP_MPEG_RAW */
    if (avr->ts_output_udp_sk) {
        gf_sk_del( avr->ts_output_udp_sk);
        avr->ts_output_udp_sk = NULL;
    }
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

static const char * AVR_UDP_PORT_OPTION = "udp.port";

static const char * AVR_UDP_ADDRESS_OPTION = "udp.address";

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
        avr->audioCodec = avcodec_find_encoder( CODEC_ID_AAC);
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
#ifdef MULTITHREAD_REDIRECT_AV
    uir->tsMutex = gf_mx_new("RedirectAV_TSMutex");
    uir->tsThread = gf_th_new("RedirectAV_TSThread");
#endif /* MULTITHREAD_REDIRECT_AV */
    uir->frameMutex = gf_mx_new("RedirectAV_frameMutex");
    uir->audioMutex = gf_mx_new("RedirectAV_audioMutex");
    uir->encodingThread = gf_th_new("RedirectAV_EncodingThread");
    uir->audioEncodingThread = gf_th_new("RedirectAV_AudioEncodingThread");
    uir->encode = 1;
    return dr;
}


void avr_delete ( GF_BaseInterface *ifce )
{
    GF_TermExt *dr = ( GF_TermExt * ) ifce;
    GF_AVRedirect *avr = dr->udta;
#ifdef MULTITHREAD_REDIRECT_AV
    gf_mx_p(avr->tsMutex);
#endif /* MULTITHREAD_REDIRECT_AV */
    avr->is_running = 0;
#ifdef MULTITHREAD_REDIRECT_AV
    gf_mx_v(avr->tsMutex);
#endif /* MULTITHREAD_REDIRECT_AV */
    /* Ensure encoding is finished */
    gf_mx_p(avr->frameMutex);
    gf_mx_v(avr->frameMutex);
    gf_sleep(1000);
    gf_th_stop(avr->encodingThread);
#ifdef MULTITHREAD_REDIRECT_AV
    gf_th_stop(avr->tsThread);
    gf_mx_del(avr->tsMutex);
    avr->tsMutex = NULL;
#endif /* MULTITHREAD_REDIRECT_AV */
    gf_mx_del(avr->frameMutex);
    avr->frameMutex = NULL;
    gf_th_del(avr->encodingThread);
    avr->encodingThread = NULL;
#ifdef MULTITHREAD_REDIRECT_AV
    gf_th_del(avr->tsThread);
    avr->tsThread = NULL;
#endif /* MULTITHREAD_REDIRECT_AV */
    gf_mx_del(avr->encodingMutex);
    avr->encodingMutex = NULL;
    if ( avr->muxer )
    {
        gf_m2ts_mux_del ( avr->muxer );
        avr->muxer = NULL;
    }
    if ( avr->ts_output_udp_sk )
    {
        gf_sk_del ( avr->ts_output_udp_sk );
        avr->ts_output_udp_sk = NULL;
    }
    avr->videoCodec = NULL;
    if ( avr->videoCodecContext )
        gf_free ( avr->videoCodecContext );
    avr->videoCodecContext = NULL;
    avr->audioCodec = NULL;
    if (avr->audioCodecContext)
        gf_free(avr->audioCodecContext);
    avr->audioCodecContext = NULL;
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
    if ( avr->audioOutBuf )
        gf_free ( avr->audioOutBuf );
    avr->audioOutBuf = NULL;
    if ( avr->audioInBuf )
        gf_free ( avr->audioInBuf );
    avr->audioInBuf = NULL;
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
