LOCAL_PATH			:= $(call my-dir)

include $(CLEAR_VARS)
include $(LOCAL_PATH)/../common.mk

LOCAL_MODULE			:= mp4box
LOCAL_MODULE_FILENAME	:= libmp4box

LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../include
LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../libgpac/

LOCAL_LDLIBS    += -L../libs/$(TARGET_ARCH_ABI)
LOCAL_LDLIBS    += -llog -lgpac

LOCAL_CFLAGS +=	-DDEBUG_MODE
LOCAL_SRC_FILES :=  ../../../../applications/mp4box/filedump.c \
					../../../../applications/mp4box/fileimport.c \
					../../../../applications/mp4box/live.c \
					../../../../applications/mp4box/main.c \
					../../../../applications/mp4box/wrapper.c

include $(BUILD_SHARED_LIBRARY)
