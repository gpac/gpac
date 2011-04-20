LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE		:= gm_mpd_in

include $(LOCAL_PATH)/base.mk

LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../extra_lib/include/
LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../include

LOCAL_SRC_FILES := ../../../../modules/mpd_in/mpd_in.c

include $(BUILD_SHARED_LIBRARY)
