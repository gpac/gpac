LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE		:= gm_ismacryp

include $(LOCAL_PATH)/base.mk

LOCAL_SRC_FILES := ../../../../modules/ismacryp/ismacryp.c

include $(BUILD_SHARED_LIBRARY)
