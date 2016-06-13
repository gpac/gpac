LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE            := gm_mc_decode

include $(LOCAL_PATH)/base.mk

LOCAL_SRC_FILES := ../../../../modules/mc_decode/mc_decode.c


include $(BUILD_SHARED_LIBRARY)
                                
