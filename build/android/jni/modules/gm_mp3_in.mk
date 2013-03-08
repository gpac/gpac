LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE		:= gm_mp3_in

include $(LOCAL_PATH)/base.mk

LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../extra_lib/include/
LOCAL_LDLIBS    += -lmad

LOCAL_SRC_FILES := ../../../../modules/mp3_in/mad_dec.c ../../../../modules/mp3_in/mp3_in.c

LOCAL_CFLAGS += -DGPAC_HAS_MAD

include $(BUILD_SHARED_LIBRARY)
