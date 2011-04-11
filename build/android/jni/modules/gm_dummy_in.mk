LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE		:= gm_dummy_in

include $(LOCAL_PATH)/base.mk

LOCAL_SRC_FILES := ../../../../modules/dummy_in/dummy_in.c


LOCAL_MODULE_FILENAME=gm_dummy_in

include $(BUILD_SHARED_LIBRARY)
