LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE		:= gm_odf_dec

include $(LOCAL_PATH)/base.mk

LOCAL_SRC_FILES := ../../../../modules/odf_dec/odf_dec.c

include $(BUILD_SHARED_LIBRARY)
