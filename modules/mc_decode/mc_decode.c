#include <gpac/modules/codec.h>

#ifdef GPAC_ANDROID

#include <jni.h>

#include "media/NdkMediaCodec.h"
#include "media/NdkMediaExtractor.h"
#include "media/NdkMediaFormat.h"


// for native window JNI
#include <android/native_window_jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>


#include<android/log.h>

#define TAG "mc_decode"

#define LOGV(...)  __android_log_print(ANDROID_LOG_VERBOSE, TAG,  __VA_ARGS__)
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, TAG,  __VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, TAG,  __VA_ARGS__)
#define LOGW(...)  __android_log_print(ANDROID_LOG_WARN, TAG,  __VA_ARGS__)
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO, TAG,  __VA_ARGS__)

#endif



typedef struct {
#ifdef GPAC_ANDROID
    ANativeWindow *window;
    AMediaCodec *codec;
    AMediaExtractor *extractor;
#endif
    bool isPlaying;
    u32 width;
    u32 height;
} MCDec;


    //select track using MediaExtractor
    u32 selectTrack(MCDec *dec) {
#ifdef GPAC_ANDROID
        int trackCount = (int) AMediaExtractor_getTrackCount(dec->extractor);
        size_t i;
        LOGV("Total num. of tracks: %d", trackCount);
        for (i = 0; i < trackCount; ++i) {
            AMediaFormat *format = AMediaExtractor_getTrackFormat(dec->extractor, i);

            const char *mime;

            bool b = AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime);

            if(!b) {
                LOGE("No mime type");
                return -1;
            }
            else LOGI("Track num.: %d, mime type: %s ", i, mime);

            if (!strncmp(mime, "video/", 6)) {
                LOGI("Selected Track num.: %d, mime type: %s", i, mime);
                AMediaExtractor_selectTrack(dec->extractor, i);

                AMediaExtractor_selectTrack(dec->extractor, i);
                dec->codec = AMediaCodec_createDecoderByType(mime);
                AMediaCodec_configure(dec->codec, format, dec->window, NULL, 0);
                AMediaFormat_delete(format);
                AMediaCodec_start(dec->codec);
                //dec->width = AMediaFormat_getInteger(AMediaFormat_KEY_WIDTH);
                //dec->height = AMediaFormat_getInteger(AMediaFormat_KEY_HEIGHT);
                LOGI("Video size = %d x %d ",dec->width, dec->height);
                return i;
            }

        }
        LOGE("Video Track not found");
#endif
        return  -1;
    }
