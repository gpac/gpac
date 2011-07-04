OCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
include $(LOCAL_PATH)/../common.mk

LOCAL_MODULE    := gpacWrapper

LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../include
LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../libgpac/

LOCAL_LDLIBS    += -L../libs/$(TARGET_ARCH_ABI)
LOCAL_LDLIBS    += -llog -lgpac

#LOCAL_CFLAGS +=	-DGPAC_GUI_ONLY
LOCAL_CFLAGS +=	-DDEBUG_MODE

LOCAL_SRC_FILES := ../../../../applications/osmo4_android/jni/wrapper.cpp

include $(BUILD_SHARED_LIBRARY)
