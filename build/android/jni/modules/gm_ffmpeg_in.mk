LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE		:= gm_ffmpeg_in

include $(LOCAL_PATH)/base.mk

LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../extra_lib/include/ffmpeg_android/

LOCAL_LDLIBS    += -lavcodec -lavformat -lswresample -lavfilter -lavutil -lswscale


LOCAL_SRC_FILES := ../../../../modules/ffmpeg_in/ffmpeg_load.c ../../../../modules/ffmpeg_in/ffmpeg_demux.c ../../../../modules/ffmpeg_in/ffmpeg_decode.c

include $(BUILD_SHARED_LIBRARY)