LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE		:= gm_droidout

include $(LOCAL_PATH)/base.mk

LOCAL_LDLIBS    +=  -lGLESv1_CM -llog -ldl

LOCAL_SRC_FILES := ../../../../modules/droid_out/droid_vout.c

LOCAL_MODULE_FILENAME=gm_droid_out
include $(BUILD_SHARED_LIBRARY)
