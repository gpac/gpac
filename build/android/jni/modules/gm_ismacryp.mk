LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE		:= gm_ismacryp

include $(LOCAL_PATH)/base.mk

LOCAL_SRC_FILES := ../../../../modules/ismacryp/ismacryp.c

LOCAL_MODULE_FILENAME=gm_ismacrypt
include $(BUILD_SHARED_LIBRARY)
