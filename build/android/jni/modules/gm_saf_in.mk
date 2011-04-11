LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE		:= gm_saf_in

include $(LOCAL_PATH)/base.mk

LOCAL_SRC_FILES := ../../../../modules/saf_in/saf_in.c

LOCAL_MODULE_FILENAME=gm_saf_in

include $(BUILD_SHARED_LIBRARY)
