#
# Main gpac Makefile
#
include config.mak

VPATH=$(SRC_PATH)

INSTFLAGS=-s
ifeq ($(DEBUGBUILD),yes)
INSTFLAGS=
endif

all: lib apps mods

lib:
	$(MAKE) -C src all

apps:
	$(MAKE) -C applications all

sggen:
	$(MAKE) -C applications sggen

mods:
	$(MAKE) -C modules all

instmoz:
	$(MAKE) -C applications/osmozilla install

depend:
	$(MAKE) -C src dep
	$(MAKE) -C applications dep
	$(MAKE) -C modules dep

clean: $(CLEANVHOOK)
	$(MAKE) -C src clean
	$(MAKE) -C applications clean
	$(MAKE) -C modules clean

distclean: $(CLEANVHOOK)
	$(MAKE) -C src distclean
	$(MAKE) -C applications distclean
	$(MAKE) -C modules distclean
	rm -f config.mak

dep:	depend

# tar release (use 'make -k tar' on a checkouted tree)
FILE=gpac-$(shell grep "\#define GPAC_VERSION " include/gpac/tools.h | \
                    cut -d "\"" -f 2 )

tar:
	( tar zcvf ~/$(FILE).tar.gz ../gpac --exclude CVS --exclude bin --exclude lib --exclude Obj --exclude temp --exclude amr_nb --exclude amr_nb_ft --exclude amr_wb_ft --exclude *.mak --exclude *.o --exclude *.~*)

install:
	install -d "$(prefix)/bin"
	install $(INSTFLAGS) -m 755 bin/gcc/MP4Box "$(prefix)/bin"
	install $(INSTFLAGS) -m 755 bin/gcc/MP42Avi "$(prefix)/bin"
	$(MAKE) -C applications install
	install -d "$(moddir)"
	install bin/gcc/*.$(DYN_LIB_SUFFIX) "$(moddir)"
	rm -f $(moddir)/libgpac.$(DYN_LIB_SUFFIX)
	rm -f $(moddir)/nposmozilla.$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
	install $(INSTFLAGS) -m 755 bin/gcc/libgpac.dll $(prefix)/lib
else
ifeq ($(DEBUGBUILD),no)
	$(STRIP) bin/gcc/libgpac.$(DYN_LIB_SUFFIX)
endif
ifeq ($(CONFIG_DARWIN),yes)
	install -m 755 bin/gcc/libgpac.$(DYN_LIB_SUFFIX) $(prefix)/lib/libgpac-$(VERSION).$(DYN_LIB_SUFFIX)
	ln -sf libgpac-$(VERSION).$(DYN_LIB_SUFFIX) $(prefix)/lib/libgpac.$(DYN_LIB_SUFFIX)
else
	install $(INSTFLAGS) -m 755 bin/gcc/libgpac.$(DYN_LIB_SUFFIX) $(prefix)/lib/libgpac-$(VERSION).$(DYN_LIB_SUFFIX)
	ln -sf libgpac-$(VERSION).$(DYN_LIB_SUFFIX) $(prefix)/lib/libgpac.$(DYN_LIB_SUFFIX)
	ldconfig || true
endif
endif
	install -d "$(mandir)/man1"
	install -m 644 doc/man/mp4box.1 $(mandir)/man1/
	install -m 644 doc/man/mp42avi.1 $(mandir)/man1/
	install -m 644 doc/man/mp4client.1 $(mandir)/man1/
	install -m 644 doc/man/gpac.1 $(mandir)/man1/

uninstall:
	$(MAKE) -C applications uninstall
	rm -rf $(moddir)
	rm -rf $(prefix)/lib/libgpac*
	rm -rf $(prefix)/bin/mp4box
	rm -rf $(prefix)/bin/mp42avi
	rm -rf $(prefix)/bin/mp42client
	rm -rf $(mandir)/man1/mp4box.1
	rm -rf $(mandir)/man1/mp42avi.1
	rm -rf $(mandir)/man1/mp4client.1
	rm -rf $(mandir)/man1/gpac.1

install-lib:
	mkdir -p "$(prefix)/include/gpac"
	install -m 644 $(SRC_PATH)/include/gpac/*.h "$(prefix)/include/gpac"
	mkdir -p "$(prefix)/include/gpac/internal" 
	install -m 644 $(SRC_PATH)/include/gpac/internal/*.h "$(prefix)/include/gpac/internal"
	mkdir -p "$(prefix)/include/gpac/modules" 
	install -m 644 $(SRC_PATH)/include/gpac/modules/*.h "$(prefix)/include/gpac/modules"
	mkdir -p "$(prefix)/lib"
	install -m 644 "./bin/gcc/libgpac_static.a" "$(prefix)/lib"

uninstall-lib:
	rm -rf "$(prefix)/include/gpac/internal"
	rm -rf "$(prefix)/include/gpac/modules"
	rm -rf "$(prefix)/include/gpac"

help:
	@echo "Input to GPAC make:"
	@echo "depend/dep: builds dependencies (dev only)"
	@echo "all (void): builds main library, programs and plugins"
	@echo "lib: builds GPAC library only (libgpac.so)"
	@echo "apps: builds programs only"
	@echo "mods: builds mudules only"
	@echo "instmoz: build and local install of osmozilla"
	@echo "sggen: builds scene graph generators"
	@echo 
	@echo "clean: clean src repository"
	@echo "distclean: clean src repository and host config file"
	@echo "tar: create GPAC tarball"
	@echo 
	@echo "install: install applications and modules on system"
	@echo "uninstall: uninstall applications and modules"
	@echo 
	@echo "install-lib: install gpac library (ligpac.so) and headers <gpac/*.h>, <gpac/modules/*.h> and <gpac/internal/*.h>"
	@echo "uninstall-lib: uninstall gpac library (libgpac.so) and headers"
	@echo
	@echo "to build libgpac documentation, go to gpac/doc and type 'doxygen'"

ifneq ($(wildcard .depend),)
include .depend
endif
