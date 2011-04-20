LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE		:= gm_rawout

include $(LOCAL_PATH)/base.mk

LOCAL_SRC_FILES := ../../../../modules/raw_out/raw_video.c

include $(BUILD_SHARED_LIBRARY)
