
LINKLIBS=

#1 - zlib support
ifneq ($(CONFIG_ZLIB),no)
CFLAGS+=$(zlib_cflags)
LINKLIBS+=$(zlib_ldflags)
endif

#2 - ssl support
ifneq ($(HAS_OPENSSL),no)
CFLAGS+=$(ssl_cflags)
LINKLIBS+=$(ssl_ldflags)
endif

## libgpac compositor compilation options
COMPOSITOR_CFLAGS=

## Add prefix before every lib
ifneq ($(prefix),/usr/local)
ifneq ($(prefix),/usr)
EXTRALIBS+=-L$(prefix)/lib
endif
endif

## OpenGL available
ifeq ($(HAS_OPENGL),yes)
EXTRALIBS+= $(OGL_LIBS)
COMPOSITOR_CFLAGS+=$(OGL_INCLS)
endif

EXTRALIBS+=$(LINKLIBS)

ifeq ($(GPAC_USE_TINYGL),yes)
COMPOSITOR_CFLAGS+=-I"$(SRC_PATH)/../TinyGL/include"
else
ifneq ($(CONFIG_EMSCRIPTEN),yes)
COMPOSITOR_CFLAGS+=-DGPAC_HAS_GLU
endif
endif


## libgpac media tools compilation options
ifeq ($(GPACREADONLY),yes)
MEDIATOOLS_CFLAGS=-DGPAC_READ_ONLY
endif


ifeq ($(STATIC_BINARY),yes)
CFLAGS+= -DGPAC_MP4BOX_MINI
endif


ifeq ($(STATIC_BUILD),yes)
LINKFLAGS+=$(zlib_ldflags) $(opensvc_ldflags) $(ssl_ldflags) $(jpeg_ldflags) $(openjpeg_ldflags) $(png_ldflags) $(mad_ldflags) $(a52_ldflags) $(xvid_ldflags) $(faad_ldflags)
LINKFLAGS+=$(ffmpeg_ldflags) $(ogg_ldflags) $(vorbis_ldflags) $(theora_ldflags) $(nghttp2_ldflags) $(vtb_ldflags) $(caption_ldflags) $(mpeghdec_ldflags)
endif


## static modules

# configure static modules
ifeq ($(STATIC_MODULES),yes)

CFLAGS+= -DGPAC_STATIC_MODULES

ifeq ($(CONFIG_ALSA),yes)
OBJS+=../modules/alsa/alsa.o
CFLAGS+=-DGPAC_HAS_ALSA
EXTRALIBS+= -lasound
endif

OBJS+=../modules/validator/validator.o

ifneq ($(CONFIG_FT),no)
OBJS+=../modules/ft_font/ft_font.o
EXTRALIBS+= $(freetype_ldflags)
CFLAGS+=-DGPAC_HAS_FREETYPE $(freetype_cflags)
endif

ifeq ($(CONFIG_SDL),yes)
CFLAGS+=-DGPAC_HAS_SDL $(sdl_cflags)
EXTRALIBS+= $(sdl_ldflags)
OBJS+=../modules/sdl_out/sdl_out.o ../modules/sdl_out/audio.o ../modules/sdl_out/video.o
endif


ifeq ($(CONFIG_X11),yes)
OBJS+= ../modules/x11_out/x11_out.o
CFLAGS+=-DGPAC_HAS_X11
ifneq ($(X11_INC_PATH),)
CFLAGS+=-I$(X11_INC_PATH)
endif

ifneq ($(X11_LIB_PATH),)
EXTRALIBS+=-L$(X11_LIB_PATH)
endif
EXTRALIBS+=-lX11

ifeq ($(USE_X11_XV),yes)
CFLAGS+=-DGPAC_HAS_X11_XV
EXTRALIBS+=-lXv
endif

ifeq ($(USE_X11_XV),yes)
CFLAGS+=-DGPAC_HAS_X11_XV
EXTRALIBS+=-lXv
endif

ifeq ($(USE_X11_SHM),yes)
CFLAGS+=-DGPAC_HAS_X11_SHM
EXTRALIBS+=-lXext
endif

ifeq ($(HAS_OPENGL),yes)
ifeq ($(CONFIG_DARWIN),yes)
#EXTRALIBS+=-lGL -lGLU
endif
endif

#end of x11
endif


ifeq ($(CONFIG_JACK),yes)
OBJS+= ../modules/jack/jack.o
CFLAGS+=-DGPAC_HAS_JACK
EXTRALIBS+=-ljack
endif

ifeq ($(CONFIG_PULSEAUDIO),yes)
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
OBJS+= ../modules/dx_hw/dx_audio.o ../modules/dx_hw/dx_video.o ../modules/dx_hw/dx_window.o ../modules/dx_hw/dx_2d.o
CFLAGS+=-DGPAC_HAS_DIRECTX -DDIRECTSOUND_VERSION=0x0500
ifneq ($(DX_PATH),system)
EXTRALIBS+=-L$(DX_PATH)/lib
CFLAGS+= -I$(DX_PATH)/include
endif
EXTRALIBS+=-ldsound -ldxguid -lddraw -lole32 -lgdi32 -lopengl32
endif



#end of static modules
endif

