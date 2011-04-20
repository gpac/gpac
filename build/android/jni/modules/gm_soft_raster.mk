LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE		:= gm_soft_raster

include $(LOCAL_PATH)/base.mk

LOCAL_SRC_FILES := ../../../../modules/../modules/soft_raster/ftgrays.c ../../../../modules/../modules/soft_raster/raster_load.c ../../../../modules/../modules/soft_raster/raster_565.c ../../../../modules/soft_raster/raster_argb.c ../../../../modules/soft_raster/raster_rgb.c ../../../../modules/soft_raster/stencil.c ../../../../modules/soft_raster/surface.c

include $(BUILD_SHARED_LIBRARY)
