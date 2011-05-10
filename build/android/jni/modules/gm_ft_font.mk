LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE		:= gm_ft_font

include $(LOCAL_PATH)/base.mk

LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../extra_lib/include/freetype

LOCAL_SRC_FILES := ../../../../modules/ft_font/ft_font.c

LOCAL_LDLIBS    += -lft2

include $(BUILD_SHARED_LIBRARY)
