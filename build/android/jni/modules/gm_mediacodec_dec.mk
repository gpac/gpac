LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE            := gm_mediacodec_dec

include $(LOCAL_PATH)/base.mk

LOCAL_SRC_FILES := ../../../../modules/mediacodec_dec/mediacodec_dec.c ../../../../modules/mediacodec_dec/mediacodec_dec_jni.c
LOCAL_LDLIBS    += -llog -ljavaenv -ldl -lOpenMAXAL -lmediandk -landroid -lGLESv2


include $(BUILD_SHARED_LIBRARY)
                                
