LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE            := gm_mediacodec_dec

include $(LOCAL_PATH)/base.mk

LOCAL_SRC_FILES := ../../../../modules/mediacodec_dec/mediacodec_dec.c
LOCAL_LDLIBS    += -llog -ljavaenv -ldl -lOpenMAXAL -lmediandk -landroid


include $(BUILD_SHARED_LIBRARY)
                                
