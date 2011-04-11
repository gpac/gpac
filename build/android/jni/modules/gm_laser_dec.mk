LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE		:= gm_laser_dec

include $(LOCAL_PATH)/base.mk

LOCAL_SRC_FILES := ../../../../modules/laser_dec/laser_dec.c

LOCAL_MODULE_FILENAME=gm_laser_dec

include $(BUILD_SHARED_LIBRARY)
