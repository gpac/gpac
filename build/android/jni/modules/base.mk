OCAL_PATH := $(call my-dir)

include $(LOCAL_PATH)/../common.mk
LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)
LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../include
LOCAL_C_INCLUDES  += $(LOCAL_PATH)/../libgpac/

LOCAL_LDLIBS    += -L$(LOCAL_PATH)/../../../../extra_lib/lib/android/$(TARGET_ARCH_ABI)/
LOCAL_LDLIBS    += -L../libs/$(TARGET_ARCH_ABI)
LOCAL_LDLIBS    += -lgpac -lft2 -ljpegdroid -lopenjpeg -lpng -lz
