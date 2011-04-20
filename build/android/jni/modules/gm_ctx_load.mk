LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE		:= gm_ctx_load

include $(LOCAL_PATH)/base.mk

LOCAL_SRC_FILES := ../../../../modules/ctx_load/ctx_load.c

include $(BUILD_SHARED_LIBRARY)
