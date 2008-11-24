include ../../config.mak

vpath %.cpp $(SRC_PATH)/applications/osmozilla

ifeq ($(CONFIG_WIN32),yes)
USER_NAME=root
else
USER_NAME=$(shell whoami)
ifeq ($(USER_NAME), root)
else
MOZILLA_DIR=local
endif
endif

CFLAGS=$(CPPFLAGS) $(XUL_CFLAGS) -I$(SRC_PATH)/include

ifeq ($(DEBUGBUILD), yes)
CFLAGS+=-g
LDFLAGS+=-g
endif

ifeq ($(GPROFBUILD), yes)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

ifeq ($(CONFIG_WIN32),yes)
CFLAGS+=-DXP_WIN
else
ifeq ($(CONFIG_DARWIN),yes)
CFLAGS+=-DXP_MAC
else
CFLAGS+=-DXP_UNIX -DMOZ_X11
endif
endif

CFLAGS+=-DNPBASIC_EXPORTS -DMOZILLA_STRICT_API -DXPCOM_GLUE


LINKLIBS=-L../../bin/gcc -lgpac

OBJS=osmozilla.o npp_gate.o np_entry.o npn_gate.o

SRCS := $(OBJS:.o=.cpp) 


LIB=nposmozilla.$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
LINKLIBS+=-lwinmm -lgdi32
LDFLAGS+=--export-all-symbols
endif

all: $(LIB)

$(LIB): $(OBJS)
ifeq ($(CONFIG_WIN32),yes)
	windres osmozilla.rc osmoz.o
	$(CXX) $(SHFLAGS) $(LDFLAGS) -o ../../bin/gcc/$@ $(OBJS) osmoz.o $(LINKLIBS)
	cp $(SRC_PATH)/applications/osmozilla/nsIOsmozilla.xpt_w32 ../../bin/gcc/nposmozilla.xpt
	chmod +w ../../bin/gcc/nposmozilla.xpt
else
	$(CXX) $(SHFLAGS) $(LDFLAGS) $(OBJS) $(LINKLIBS) -o ../../bin/gcc/$@ 
	cp $(SRC_PATH)/applications/osmozilla/nsIOsmozilla.xpt_linux ../../bin/gcc/nposmozilla.xpt
	chmod +w ../../bin/gcc/nposmozilla.xpt
endif
	@echo $(USER_ROOT)

%.o: %.cpp
	$(CXX) $(CFLAGS) -c -o $@ $< 

clean:
	rm -f $(OBJS) ../../bin/gcc/$(LIB) ../../bin/gcc/nposmozilla.xpt
ifeq ($(CONFIG_WIN32),yes)
	rm -f osmoz.o
endif

install: 
ifeq ($(MOZILLA_DIR), local)
ifeq ($(USER_NAME), root)
	@echo "*** Root cannot install local mozilla plugins! ***"
	@echo "*** Exit root mode and reinstall mozilla plugin! ***"
else
	$(MAKE) $(LIB)
	install -D -m 755 ../../bin/gcc/$(LIB) "$(HOME)/.mozilla/plugins/$(LIB)"
	install -D -m 755 ../../bin/gcc/nposmozilla.xpt "$(HOME)/.mozilla/components/nposmozilla.xpt"
endif
else
	install -D -m 755 ../../bin/gcc/$(LIB) "$(MOZILLA_DIR)/components/$(LIB)"
	install -D -m 755 ../../bin/gcc/nposmozilla.xpt "$(MOZILLA_DIR)/components/nposmozilla.xpt"
endif

uninstall:
ifeq ($(MOZILLA_DIR), local)
ifeq ($(USER_NAME), root)
else
	rm -rf "$(HOME)/.mozilla/plugins/$(LIB)"
	rm -rf "$(HOME)/.mozilla/components/nposmozilla.xpt"
endif
else
	rm -rf "$(MOZILLA_DIR)/components/$(LIB)"
	rm -rf "$(MOZILLA_DIR)/components/nposmozilla.xpt"
endif

dep: depend

depend:
	rm -f .depend	
	$(CC) -MM $(CFLAGS) $(SRCS) 1>.depend

distclean: clean
	rm -f Makefile.bak .depend

