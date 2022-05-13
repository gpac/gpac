OCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
include $(LOCAL_PATH)/../common.mk

LOCAL_MODULE    := gpacWrapper

LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../include
LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../libgpac/

LOCAL_LDLIBS    += -L../libs/$(TARGET_ARCH_ABI)
LOCAL_LDLIBS    += -llog -lgpac

#GPAC JNI wrapper
LOCAL_SRC_FILES :=  ../../../../applications/gpac_android/src/main/jni/gpac_jni.cpp
#gpac main and help
LOCAL_SRC_FILES +=../../../../applications/gpac/gpac.c ../../../../applications/gpac/gpac_help.c
#mp4box
LOCAL_SRC_FILES +=  ../../../../applications/mp4box/filedump.c \
					../../../../applications/mp4box/fileimport.c \
					../../../../applications/mp4box/live.c \
					../../../../applications/mp4box/mp4box.c

include $(BUILD_SHARED_LIBRARY)
