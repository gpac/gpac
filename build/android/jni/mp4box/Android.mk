LOCAL_PATH			:= $(call my-dir)
APP_ABI          	:= armeabi


LOCAL_MODULE			:= mp4box
LOCAL_MODULE_FILENAME	:= libmp4box

LOCAL_C_INCLUDES 	:= $(LOCAL_PATH)/../libgpac \
						$(LOCAL_PATH)/../../../../include 

LOCAL_LDLIBS    += -L$(LOCAL_PATH)/../../libs/$(TARGET_ARCH_ABI)
LOCAL_LDLIBS    += -lgpac -llog

LOCAL_SRC_FILES :=  ../../../../applications/mp4box/filedump.c \
					../../../../applications/mp4box/fileimport.c \
					../../../../applications/mp4box/live.c \
					../../../../applications/mp4box/main.c \
					../../../../applications/mp4box/wrapper.c

include $(BUILD_SHARED_LIBRARY)
