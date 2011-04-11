LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE		:= gm_ffmpeg_in

include $(LOCAL_PATH)/base.mk

LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../extra_lib/android/ffmpeg/jni/
#LOCAL_C_INCLUDES 	+= /home/gacon/workspace/ffmpeg/jni/

LOCAL_LDLIBS    += -lffmpeg
#LOCAL_LDLIBS    += -lavcodec -lavformat -lavutil
#LOCAL_STATIC_LIBRARIES := libavformat libavcodec libavutil libpostproc libswscale 
#LOCAL_STATIC_LIBRARIES += libavformat
#LOCAL_STATIC_LIBRARIES += libavutil
#LOCAL_STATIC_LIBRARIES += libswscale

LOCAL_SRC_FILES := ../../../../modules/ffmpeg_in/ffmpeg_load.c ../../../../modules/ffmpeg_in/ffmpeg_demux.c ../../../../modules/ffmpeg_in/ffmpeg_decode.c

LOCAL_MODULE_FILENAME=gm_ffmpeg_in
include $(BUILD_SHARED_LIBRARY)
