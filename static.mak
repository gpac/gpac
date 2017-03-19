
NEED_LOCAL_LIB="no"
LINKLIBS=

ifeq ($(DISABLE_CORE_TOOLS), no)

#1 - zlib support
ifeq ($(CONFIG_ZLIB), local)
CFLAGS+= -I"$(LOCAL_INC_PATH)/zlib"
NEED_LOCAL_LIB="yes"
endif
ifneq ($(CONFIG_ZLIB), no)
LINKLIBS+=-lz
endif

#2 - ssl support
ifeq ($(HAS_OPENSSL), yes)
LINKLIBS+=$(SSL_LIBS)
endif

#3 - spidermonkey support
ifeq ($(CONFIG_JS),no)
else
SCENEGRAPH_CFLAGS+=$(JS_FLAGS)
ifeq ($(CONFIG_JS),local)
NEED_LOCAL_LIB="yes"
endif
LINKLIBS+=$(JS_LIBS)
endif

endif

#4 - JPEG support
ifeq ($(CONFIG_JPEG), no)
else
LINKLIBS+= -ljpeg
#local lib
ifeq ($(CONFIG_JPEG), local)
NEED_LOCAL_LIB="yes"
MEDIATOOLS_CFLAGS+=-I"$(LOCAL_INC_PATH)/jpeg"
endif
endif


#5 - PNG support
ifeq ($(CONFIG_PNG), no)
else
LINKLIBS+= -lpng
ifeq ($(CONFIG_PNG), local)
NEED_LOCAL_LIB="yes"
MEDIATOOLS_CFLAGS+=-I"$(LOCAL_INC_PATH)/png"
endif
endif


## libgpac compositor compilation options
COMPOSITOR_CFLAGS=

## Add prefix before every lib
ifneq ($(prefix), /usr/local)
EXTRALIBS+=-L$(prefix)/lib
else
ifneq ($(prefix), /usr)
EXTRALIBS+=-L$(prefix)/lib
endif
endif

## OpenGL available
ifeq ($(HAS_OPENGL),yes)
EXTRALIBS+= $(OGL_LIBS)
COMPOSITOR_CFLAGS+=$(OGL_INCLS)
endif

ifeq ($(NEED_LOCAL_LIB), "yes")
EXTRALIBS+=-L../extra_lib/lib/gcc
endif

EXTRALIBS+=$(LINKLIBS)

ifeq ($(GPAC_USE_TINYGL), yes)
COMPOSITOR_CFLAGS+=-I"$(SRC_PATH)/../TinyGL/include"
else
COMPOSITOR_CFLAGS+=-DGPAC_HAS_GLU
endif

ifeq ($(ENABLE_JOYSTICK), yes)
COMPOSITOR_CFLAGS+=-DENABLE_JOYSTICK
endif


## libgpac media tools compilation options
ifeq ($(GPACREADONLY), yes)
MEDIATOOLS_CFLAGS=-DGPAC_READ_ONLY
endif


ifeq ($(GPAC_ENST), yes)
OBJS+=$(LIBGPAC_ENST)
EXTRALIBS+=-liconv
MEDIATOOLS_CFLAGS+=-DGPAC_ENST_PRIVATE
endif





## static modules

# configure static modules
ifeq ($(STATIC_MODULES), yes)

CFLAGS+= -DGPAC_STATIC_MODULES

OBJS+=../modules/aac_in/aac_in.o
ifneq ($(CONFIG_FAAD), no)
OBJS+=../modules/aac_in/faad_dec.o
CFLAGS+=-DGPAC_HAS_FAAD
EXTRALIBS+= -lfaad
endif

OBJS+=../modules/ac3_in/ac3_in.o
ifneq ($(CONFIG_A52), no)
OBJS+=../modules/ac3_in/liba52_dec.o
CFLAGS+=-DGPAC_HAS_LIBA52
EXTRALIBS+= -la52
endif

OBJS+=../modules/amr_dec/amr_in.o
CFLAGS+=-DGPAC_AMR_IN_STANDALONE

ifeq ($(CONFIG_ALSA), yes)
OBJS+=../modules/alsa/alsa.o
CFLAGS+=-DGPAC_HAS_ALSA
EXTRALIBS+= -lasound
endif

OBJS+=../modules/audio_filter/audio_filter.o
OBJS+=../modules/bifs_dec/bifs_dec.o

ifeq ($(DISABLE_SMGR), no)
OBJS+=../modules/ctx_load/ctx_load.o
OBJS+=../modules/svg_in/svg_in.o
endif

OBJS+=../modules/dummy_in/dummy_in.o
OBJS+=../modules/ismacryp/isma_ea.o
ifneq ($(CONFIG_JS), no)
CFLAGS+=$(JS_FLAGS)
OBJS+=../modules/gpac_js/gpac_js.o
endif
ifeq ($(DISABLE_SVG), no)
OBJS+=../modules/laser_dec/laser_dec.o
endif
OBJS+=../modules/soft_raster/ftgrays.o ../modules/soft_raster/raster_load.o ../modules/soft_raster/raster_565.o ../modules/soft_raster/raster_argb.o ../modules/soft_raster/raster_rgb.o ../modules/soft_raster/stencil.o ../modules/soft_raster/surface.o

OBJS+=../modules/mp3_in/mp3_in.o
ifneq ($(CONFIG_MAD), no)
OBJS+=../modules/mp3_in/mad_dec.o
CFLAGS+=-DGPAC_HAS_MAD
EXTRALIBS+= -lmad
endif
OBJS+=../modules/isom_in/load.o ../modules/isom_in/read.o ../modules/isom_in/read_ch.o ../modules/isom_in/isom_cache.o
OBJS+=../modules/odf_dec/odf_dec.o
OBJS+=../modules/rtp_in/rtp_in.o ../modules/rtp_in/rtp_session.o ../modules/rtp_in/rtp_signaling.o ../modules/rtp_in/rtp_stream.o ../modules/rtp_in/sdp_fetch.o ../modules/rtp_in/sdp_load.o
OBJS+=../modules/saf_in/saf_in.o
ifeq ($(DISABLE_DASH_CLIENT), no)
OBJS+=../modules/mpd_in/mpd_in.o
ifneq ($(CONFIG_JS),no)
OBJS+=../modules/mse_in/mse_in.o
endif
endif
OBJS+=../modules/timedtext/timedtext_dec.o ../modules/timedtext/timedtext_in.o
OBJS+=../modules/validator/validator.o
OBJS+=../modules/raw_out/raw_video.o
ifeq ($(DISABLE_TTXT), no)
OBJS+=../modules/vtt_in/vtt_in.o ../modules/vtt_in/vtt_dec.o
endif

OBJS+=../modules/img_in/img_dec.o ../modules/img_in/img_in.o ../modules/img_in/bmp_dec.o ../modules/img_in/png_dec.o ../modules/img_in/jpeg_dec.o
ifneq ($(CONFIG_JP2), no)
OBJS+=../modules/img_in/jp2_dec.o
EXTRALIBS+= -lopenjpeg
CFLAGS+=-DGPAC_HAS_JP2
endif

ifneq ($(CONFIG_FT), no)
OBJS+=../modules/ft_font/ft_font.o
EXTRALIBS+= $(FT_LIBS)
CFLAGS+=-DGPAC_HAS_FREETYPE $(FT_CFLAGS)
endif

ifeq ($(DISABLE_MEDIA_IMPORT), no)
OBJS+=../modules/mpegts_in/mpegts_in.o
endif

ifeq ($(DISABLE_LOADER_BT), no)
OBJS+=../modules/osd/osd.o
endif

#cf comment for openHEVC - we cannot build ffmpeg and openhevc as static modules for the current time :(
ifneq ($(CONFIG_FFMPEG), no)
OBJS+=../modules/ffmpeg_in/ffmpeg_decode.o ../modules/ffmpeg_in/ffmpeg_demux.o ../modules/ffmpeg_in/ffmpeg_load.o
CFLAGS+=-DGPAC_HAS_FFMPEG -Wno-deprecated-declarations -I"$(SRC_PATH)/include" $(ffmpeg_cflags)
EXTRALIBS+= $(ffmpeg_lflags)
#fix it first
#OBJS+=../modules/redirect_av/redirect_av.o
endif

ifneq ($(CONFIG_XVID), no)
OBJS+=../modules/xvid_dec/xvid_dec.o
CFLAGS+=-DGPAC_HAS_XVID
EXTRALIBS+= -lxvidcore
endif

ifneq ($(CONFIG_OGG), no)
OBJS+=../modules/ogg/ogg_load.o ../modules/ogg/ogg_in.o ../modules/ogg/theora_dec.o ../modules/ogg/vorbis_dec.o
CFLAGS+=-DGPAC_HAS_OGG
EXTRALIBS+= -logg
ifneq ($(CONFIG_VORBIS), no)
CFLAGS+=-DGPAC_HAS_VORBIS
EXTRALIBS+= -lvorbis
endif
ifneq ($(CONFIG_THEORA), no)
CFLAGS+=-DGPAC_HAS_THEORA
EXTRALIBS+= -ltheora
endif

endif


ifeq ($(CONFIG_SDL), yes)
CFLAGS+=-DGPAC_HAS_SDL $(SDL_CFLAGS)
EXTRALIBS+= $(SDL_LIBS)
OBJS+=../modules/sdl_out/sdl_out.o ../modules/sdl_out/audio.o ../modules/sdl_out/video.o
endif


ifeq ($(DISABLE_SVG), no)
OBJS+=../modules/widgetman/widgetman.o ../modules/widgetman/unzip.o ../modules/widgetman/widget.o ../modules/widgetman/wgt_load.o
endif

ifeq ($(CONFIG_OPENSVC),yes)
OBJS+= ../modules/opensvc_dec/opensvc_dec.o
CFLAGS+=-DGPAC_HAS_OPENSVC $(OSVC_CFLAGS)
EXTRALIBS+=$(OSVC_LDFLAGS)
endif

#cannot include openhevc and ffmpeg (yet) here since it uses avcodec_* calls which are relocated to libavcodec.so ...
ifeq ($(CONFIG_OPENHEVC),yes)
ifeq ($(CONFIG_FFMPEG), no)
OBJS+= ../modules/openhevc_dec/openhevc_dec.o
CFLAGS+=-DGPAC_HAS_OPENHEVC $(OHEVC_CFLAGS)
EXTRALIBS+=$(OHEVC_LDFLAGS)
endif
endif

ifeq ($(CONFIG_X11),yes)
OBJS+= ../modules/x11_out/x11_out.o
CFLAGS+=-DGPAC_HAS_X11
ifneq ($(X11_INC_PATH), )
CFLAGS+=-I$(X11_INC_PATH)
endif

ifneq ($(X11_LIB_PATH), )
EXTRALIBS+=-L$(X11_LIB_PATH)
endif
EXTRALIBS+=-lX11

ifeq ($(USE_X11_XV), yes)
CFLAGS+=-DGPAC_HAS_X11_XV
EXTRALIBS+=-lXv
endif

ifeq ($(USE_X11_XV), yes)
CFLAGS+=-DGPAC_HAS_X11_XV
EXTRALIBS+=-lXv
endif

ifeq ($(USE_X11_SHM), yes)
CFLAGS+=-DGPAC_HAS_X11_SHM
EXTRALIBS+=-lXext
endif

ifeq ($(HAS_OPENGL), yes)
ifeq ($(CONFIG_DARWIN),yes)
EXTRALIBS+=-lGL -lGLU
endif
endif

#end of x11
endif


ifeq ($(CONFIG_JACK), yes)
OBJS+= ../modules/jack/jack.o
CFLAGS+=-DGPAC_HAS_JACK
EXTRALIBS+=-ljack
endif

ifneq ($(CONFIG_OSS_AUDIO), no)
OBJS+= ../modules/oss_audio/oss.o
CFLAGS+=-DGPAC_HAS_OSS $(OSS_CFLAGS)
EXTRALIBS+=$(OSS_LDFLAGS)
ifneq ($(OSS_INC_TYPE), yes)
CFLAGS+=-DOSS_FIX_INC
endif
ifeq ($(TARGET_ARCH_ARMV4L), yes)
CFLAGS+=-DFORCE_SR_LIMIT
endif

endif


ifeq ($(CONFIG_PULSEAUDIO), yes)
OBJS+= ../modules/pulseaudio/pulseaudio.o
CFLAGS+=-DGPAC_HAS_PULSEAUDIO
EXTRALIBS+=-lpulse -lpulse-simple
endif


ifeq ($(CONFIG_DIRECTFB),yes)
OBJS+= ../modules/directfb_out/directfb_out.o ../modules/directfb_out/directfb_wrapper.o
CFLAGS+=-DGPAC_HAS_DIRECTFB -I$(DIRECTFB_INC_PATH)
EXTRALIBS+=$(DIRECTFB_LIB)
endif

ifeq ($(CONFIG_WIN32),yes)
OBJS+= ../modules/wav_out/wav_out.o
EXTRALIBS+=-DGPAC_HAS_WAVEOUT -DDISABLE_WAVE_EX
endif

ifeq ($(CONFIG_DIRECTX),yes)
OBJS+= ../modules/dx_hw/dx_audio.o ../modules/dx_hw/dx_video.o ../modules/dx_hw/dx_window.o ../modules/dx_hw/dx_2d.o ../modules/dx_hw/copy_pixels.o
CFLAGS+=-DGPAC_HAS_DIRECTX -DDIRECTSOUND_VERSION=0x0500
ifneq ($(DX_PATH), system)
EXTRALIBS+=-L$(DX_PATH)/lib
CFLAGS+= -I$(DX_PATH)/include
endif
EXTRALIBS+=-ldsound -ldxguid -lddraw -lole32 -lgdi32 -lopengl32
endif


ifneq ($(CONFIG_FREENECT), no)
OBJS+= ../modules/freenect/freenect.o
CFLAGS+=-DGPAC_HAS_FREENECT $(FREENECT_CFLAGS)
ifeq ($(CONFIG_FREENECT), local)
CFLAGS+= -I"$(LOCAL_INC_PATH)"
EXTRALIBS+=-L../../extra_lib/lib/gcc
endif
EXTRALIBS+=-lfreenect
endif


ifeq ($(CONFIG_DARWIN),yes)
OBJS+=../modules/vtb_decode/vtb_decode.o
EXTRALIBS+=-framework CoreFoundation -framework CoreVideo -framework CoreMedia -framework VideoToolbox
endif

#end of static modules

endif

