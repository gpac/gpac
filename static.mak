
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
FILTERS_CFLAGS+=-I"$(LOCAL_INC_PATH)/jpeg"
endif
endif


#5 - PNG support
ifeq ($(CONFIG_PNG), no)
else
LINKLIBS+= -lpng
ifeq ($(CONFIG_PNG), local)
NEED_LOCAL_LIB="yes"
MEDIATOOLS_CFLAGS+=-I"$(LOCAL_INC_PATH)/png"
FILTERS_CFLAGS+=-I"$(LOCAL_INC_PATH)/png"
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


ifeq ($(MP4BOX_STATIC),yes)
CFLAGS+= -DGPAC_MP4BOX_MINI
endif


## static modules

# configure static modules
ifeq ($(STATIC_MODULES), yes)

CFLAGS+= -DGPAC_STATIC_MODULES

ifeq ($(CONFIG_ALSA), yes)
OBJS+=../modules/alsa/alsa.o
CFLAGS+=-DGPAC_HAS_ALSA
EXTRALIBS+= -lasound
endif

ifneq ($(CONFIG_JS), no)
CFLAGS+=$(JS_FLAGS)
OBJS+=../modules/gpac_js/gpac_js.o
endif

OBJS+=../modules/validator/validator.o

ifneq ($(CONFIG_FT), no)
OBJS+=../modules/ft_font/ft_font.o
EXTRALIBS+= $(FT_LIBS)
CFLAGS+=-DGPAC_HAS_FREETYPE $(FT_CFLAGS)
endif

ifeq ($(CONFIG_SDL), yes)
CFLAGS+=-DGPAC_HAS_SDL $(SDL_CFLAGS)
EXTRALIBS+= $(SDL_LIBS)
OBJS+=../modules/sdl_out/sdl_out.o ../modules/sdl_out/audio.o ../modules/sdl_out/video.o
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



#end of static modules
endif


ifeq ($(CONFIG_DARWIN),yes)
EXTRALIBS+=-framework CoreFoundation -framework CoreVideo -framework CoreMedia -framework VideoToolbox
endif
