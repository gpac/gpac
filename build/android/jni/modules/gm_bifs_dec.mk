LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE		:= gm_bifs_dec

include $(LOCAL_PATH)/base.mk

LOCAL_SRC_FILES := ../../../../modules/bifs_dec/bifs_dec.c


include $(BUILD_SHARED_LIBRARY)

