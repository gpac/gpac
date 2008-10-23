#
# Main gpac Makefile
#
include config.mak

vpath %.c $(SRC_PATH)

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

clean:
	$(MAKE) -C src clean
	$(MAKE) -C applications clean
	$(MAKE) -C modules clean

distclean:
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
	install -d "$(DESTDIR)$(prefix)"
	install -d "$(DESTDIR)$(prefix)/bin"
	install $(INSTFLAGS) -m 755 bin/gcc/MP4Box "$(DESTDIR)$(prefix)/bin"
	$(MAKE) -C applications install
	install -d "$(DESTDIR)$(moddir)"
	install bin/gcc/*.$(DYN_LIB_SUFFIX) "$(DESTDIR)$(moddir)"
	rm -f $(DESTDIR)$(moddir)/libgpac.$(DYN_LIB_SUFFIX)
	rm -f $(DESTDIR)$(moddir)/nposmozilla.$(DYN_LIB_SUFFIX)
ifeq ($(CONFIG_WIN32),yes)
	install $(INSTFLAGS) -m 755 bin/gcc/libgpac.dll $(prefix)/$(libdir)
else
ifeq ($(DEBUGBUILD),no)
	$(STRIP) bin/gcc/libgpac.$(DYN_LIB_SUFFIX)
endif
ifeq ($(CONFIG_DARWIN),yes)
	install -m 755 bin/gcc/libgpac.$(DYN_LIB_SUFFIX) $(DESTDIR)$(prefix)/$(libdir)/libgpac-$(VERSION).$(DYN_LIB_SUFFIX)
	ln -sf libgpac-$(VERSION).$(DYN_LIB_SUFFIX) $(DESTDIR)$(prefix)/$(libdir)/libgpac.$(DYN_LIB_SUFFIX)
else
	install $(INSTFLAGS) -m 755 bin/gcc/libgpac.$(DYN_LIB_SUFFIX) $(DESTDIR)$(prefix)/$(libdir)/libgpac-$(VERSION).$(DYN_LIB_SUFFIX)
	ln -sf libgpac-$(VERSION).$(DYN_LIB_SUFFIX) $(DESTDIR)$(prefix)/$(libdir)/libgpac.$(DYN_LIB_SUFFIX)
	ldconfig || true
endif
endif
	install -d "$(DESTDIR)$(mandir)"
	install -d "$(DESTDIR)$(mandir)/man1"
	if [ -d  doc ] ; then \
	install -m 644 doc/man/mp4box.1 $(DESTDIR)$(mandir)/man1/ ; \
	install -m 644 doc/man/mp4client.1 $(DESTDIR)$(mandir)/man1/ ; \
	install -m 644 doc/man/gpac.1 $(DESTDIR)$(mandir)/man1/ ; \
	install -d "$(DESTDIR)$(prefix)/share/gpac" ; \
	install -m 644 doc/gpac.mp4 $(DESTDIR)$(prefix)/share/gpac/ ; \
	fi

uninstall:
	$(MAKE) -C applications uninstall
	rm -rf $(moddir)
	rm -rf $(prefix)/$(libdir)/libgpac*
	rm -rf $(prefix)/bin/MP4Box
	rm -rf $(prefix)/bin/MP4Client
	rm -rf $(mandir)/man1/mp4box.1
	rm -rf $(mandir)/man1/mp4client.1
	rm -rf $(mandir)/man1/gpac.1
	rm -rf $(prefix)/share/gpac

install-lib:
	mkdir -p "$(DESTDIR)$(prefix)/include/gpac"
	install -m 644 $(SRC_PATH)/include/gpac/*.h "$(DESTDIR)$(prefix)/include/gpac"
	mkdir -p "$(DESTDIR)$(prefix)/include/gpac/internal"
	install -m 644 $(SRC_PATH)/include/gpac/internal/*.h "$(DESTDIR)$(prefix)/include/gpac/internal"
	mkdir -p "$(DESTDIR)$(prefix)/include/gpac/modules"
	install -m 644 $(SRC_PATH)/include/gpac/modules/*.h "$(DESTDIR)$(prefix)/include/gpac/modules"
ifeq ($(GPAC_ENST), yes)
	mkdir -p "$(DESTDIR)$(prefix)/include/gpac/enst"
	install -m 644 $(SRC_PATH)/include/gpac/enst/*.h "$(DESTDIR)$(prefix)/include/gpac/enst"
endif
	mkdir -p "$(DESTDIR)$(prefix)/lib"
	install -m 644 "./bin/gcc/libgpac_static.a" "$(DESTDIR)$(prefix)/lib"
	mkdir -p "$(DESTDIR)$(prefix)/$(libdir)"
	install -m 644 "./bin/gcc/libgpac_static.a" "$(DESTDIR)$(prefix)/$(libdir)"

uninstall-lib:
	rm -rf "$(prefix)/include/gpac/internal"
	rm -rf "$(prefix)/include/gpac/modules"
	rm -rf "$(prefix)/include/gpac/enst"
	rm -rf "$(prefix)/include/gpac"

help:
	@echo "Input to GPAC make:"
	@echo "depend/dep: builds dependencies (dev only)"
	@echo "all (void): builds main library, programs and plugins"
	@echo "lib: builds GPAC library only (libgpac.so)"
	@echo "apps: builds programs only"
	@echo "modules: builds modules only"
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
	@echo "install-lib: install gpac library (dyn and static) and headers <gpac/*.h>, <gpac/modules/*.h> and <gpac/internal/*.h>"
	@echo "uninstall-lib: uninstall gpac library (dyn and static) and headers"
	@echo
	@echo "to build libgpac documentation, go to gpac/doc and type 'doxygen'"

ifneq ($(wildcard .depend),)
include .depend
endif
