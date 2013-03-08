LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE		:= gm_img_in

include $(LOCAL_PATH)/base.mk

#LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../extra_lib/android/libopenjpeg/jni/dist
#LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../extra_lib/android/libopenjpeg/jni/

LOCAL_C_INCLUDES 	+= $(LOCAL_PATH)/../../../../extra_lib/include
LOCAL_LDLIBS    += -ljpegdroid -lopenjpeg -lpng

LOCAL_SRC_FILES := ../../../../modules/img_in/img_dec.c ../../../../modules/img_in/img_in.c ../../../../modules/img_in/bmp_dec.c ../../../../modules/img_in/png_dec.c ../../../../modules/img_in/jpeg_dec.c ../../../../modules/img_in/jp2_dec.c


include $(BUILD_SHARED_LIBRARY)
