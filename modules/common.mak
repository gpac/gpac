
ifeq ($(DEBUGBUILD),yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD),yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

ifeq ($(CONFIG_EMSCRIPTEN),yes)
else ifeq ($(CONFIG_DARWIN),yes)
else
LDFLAGS+= -Wl,-rpath,'$$ORIGIN' -Wl,-rpath,$(prefix)/lib -Wl,-rpath-link,../../bin/gcc
endif
