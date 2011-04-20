LOCAL_PATH := $(call my-dir)

include $(LOCAL_PATH)/../common.mk
LOCAL_C_INCLUDES 	:= $(LOCAL_PATH)
LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../include
LOCAL_C_INCLUDES        += $(LOCAL_PATH)/../libgpac/

LOCAL_LDLIBS    += -L$(LOCAL_PATH)/../../../../extra_lib/lib/android/armeabi/
LOCAL_LDLIBS    += $(GPAC_LIB_L)
LOCAL_LDLIBS    += -lgpac -ljs_osmo -leditline -lft2 -ljpeg -lopenjpeg -lpng -lz
