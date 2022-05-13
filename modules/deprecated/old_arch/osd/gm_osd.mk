LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE		:= gm_osd

include $(LOCAL_PATH)/base.mk

LOCAL_SRC_FILES := ../../../../modules/osd/osd.c

include $(BUILD_SHARED_LIBRARY)
