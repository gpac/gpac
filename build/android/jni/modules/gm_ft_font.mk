LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE		:= gm_ft_font

include $(LOCAL_PATH)/base.mk

LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../extra_lib/android/freetypedroid/jni/include
LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../../extra_lib/android/freetypedroid/jni/include/freetype

LOCAL_SRC_FILES := ../../../../modules/ft_font/ft_font.c

LOCAL_LDLIBS    += -lft2

LOCAL_MODULE_FILENAME=gm_ft_font
include $(BUILD_SHARED_LIBRARY)
