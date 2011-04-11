LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE		:= gm_mpegts_in

include $(LOCAL_PATH)/base.mk

LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../extra_lib/include/

LOCAL_SRC_FILES := ../../../../modules/mpegts_in/mpegts_in.c
LOCAL_MODULE_FILENAME=gm_mpegts_in

include $(BUILD_SHARED_LIBRARY)
