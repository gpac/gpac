LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE		:= gm_droid_cam

include $(LOCAL_PATH)/base.mk

LOCAL_LDLIBS += -llog

LOCAL_SRC_FILES := ../../../../modules/droid_cam/droid_cam.c 

include $(BUILD_SHARED_LIBRARY)
