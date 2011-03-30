TARGET_PLATFORM		:= android-8
LOCAL_C_INCLUDES 	:= $(LOCAL_PATH)
LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../include
LOCAL_C_INCLUDES        += $(LOCAL_PATH)/../libgpac/


LOCAL_LDLIBS    += -L$(LOCAL_PATH)/../../../../extra_lib/lib/android/
#LOCAL_LDLIBS    += -L$(LOCAL_PATH)/../../../../bin/android/
LOCAL_LDLIBS    += -L$(LOCAL_PATH)/../../libs/armeabi/
LOCAL_LDLIBS    += -ljs_osmo -leditline -lft2 -ljpeg -lopenjpeg -lpng -lz -lgpac -llog

LOCAL_CFLAGS += -DGPAC_ANDROID
LOCAL_CFLAGS +=	-DGPAC_HAVE_CONFIG_H
LOCAL_CFLAGS += -DXP_UNIX
