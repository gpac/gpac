LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE		:= gm_timedtext

include $(LOCAL_PATH)/base.mk

LOCAL_SRC_FILES := ../../../../modules/timedtext/timedtext_in.c ../../../../modules/timedtext/timedtext_dec.c

include $(BUILD_SHARED_LIBRARY)
