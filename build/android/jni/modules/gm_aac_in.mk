LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE		:= gm_aac_in

include $(LOCAL_PATH)/base.mk

LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../extra_lib/include/
LOCAL_LDLIBS    += -lfaad

#LOCAL_STATIC_LIBRARIES := libfaad
LOCAL_CFLAGS += -DGPAC_HAS_FAAD

LOCAL_SRC_FILES := ../../../../modules/aac_in/aac_in.c ../../../../modules/aac_in/faad_dec.c 

include $(BUILD_SHARED_LIBRARY)
