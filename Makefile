#
# Main gpac Makefile
#
include config.mak

ifndef SRC_PATH
override SRC_PATH = .
endif

vpath %.c $(SRC_PATH)

all:	version
	$(MAKE) -C src all
	$(MAKE) -C applications all
ifneq ($(STATIC_BINARY),yes)
	$(MAKE) -C modules all
endif


config.mak:
	@echo "running default configure"
	@./configure


GITREV_PATH:=$(SRC_PATH)/include/gpac/revision.h
TAG:=$(shell git --git-dir=$(SRC_PATH)/.git describe --tags --abbrev=0 2> /dev/null)
VERSION:=$(shell echo `git --git-dir=$(SRC_PATH)/.git describe --tags --long  || echo "UNKNOWN"` | sed "s/^$(TAG)-//")
BRANCH:=$(shell git --git-dir=$(SRC_PATH)/.git rev-parse --abbrev-ref HEAD 2> /dev/null || echo "UNKNOWN")

version:
	@if [ -d $(SRC_PATH)/".git" ]; then \
		echo "#define GPAC_GIT_REVISION	\"$(VERSION)-$(BRANCH)\"" > $(GITREV_PATH).new; \
		if ! diff -q $(GITREV_PATH) $(GITREV_PATH).new >/dev/null ; then \
			mv $(GITREV_PATH).new  $(GITREV_PATH); \
		fi; \
	else \
		echo "No GIT Version found" ; \
	fi

lib:	version
	$(MAKE) -C src all

apps:
	$(MAKE) -C applications all

sggen:
	$(MAKE) -C applications sggen

mods:
	$(MAKE) -C modules all

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
	rm -f config.mak config.h config.log
	@find . -type f -name '*.gcno*' -delete
	@find . -type f -name '*.gcda*' -delete
	@find . -type f -name '*.dep*' -delete
	@rm -f coverage.info 2> /dev/null
	@rm -f bin/gcc/gm_*$(DYN_LIB_SUFFIX) 2> /dev/null
	@rm -f bin/gcc/gf_*$(DYN_LIB_SUFFIX) 2> /dev/null

doc:
	@cd $(SRC_PATH)/share/doc && doxygen

man:
	@cd $(SRC_PATH)/share/doc/man && MP4Box -genman && gpac -genman

unit_tests:
	@echo "Building unit tests"
	$(SRC_PATH)/unittests/build.sh > bin/gcc/unittests.c
	@$(CC) -I$(SRC_PATH) -I$(SRC_PATH)/include -DGPAC_HAVE_CONFIG_H -g \
		-o bin/gcc/unittests bin/gcc/unittests.c \
		$(SRC_PATH)/unittests/tests.c \
		$(shell find src/ -path "*/unittests/*.c" | sort) \
		-Lbin/gcc -Wl,-rpath=$(realpath bin/gcc) -Lbin/gcc -Wl,-rpath=$(realpath bin/gcc) -L. -Wl,-rpath=$(realpath .) -lgpac
	@echo "Executing unit tests"
	LD_LIBRARY_PATH=bin/gcc bin/gcc/unittests



unit_tests_clean:
	@echo "Cleaning unit tests artifacts"
	@rm -f bin/gcc/unittests.c bin/gcc/unittests

test_suite:
	@cd $(SRC_PATH)/testsuite && ./make_tests.sh -precommit -p=0

lcov_clean:
	lcov --directory . --zerocounters

lcov_only:
	@echo "Generating lcov info in coverage.info"
	@rm -f ./gpac-conf-* > /dev/null
	@lcov -q -capture --directory . --output-file all.info
	@lcov --remove all.info '*/usr/*' '*/opt/*' '*/include/*' '*/validator/*' '*/quickjs/*' '*/jsmods/WebGLRenderingContextBase*' '*/utils/Remotery*' '*/utils/gzio*'  --output coverage.info
	@rm all.info
	@echo "Purging lcov info"
	@cd src ; for dir in * ; do cd .. ; sed -i -- "s/$$dir\/$$dir\//$$dir\//g" coverage.info; cd src; done ; cd ..
	@echo "Done - coverage.info ready"

lcov:	lcov_only
	@rm -rf coverage/
	@genhtml -q -o coverage coverage.info

travis_tests:
	@echo "Running tests in $(SRC_PATH)/testsuite"
	@cd $(SRC_PATH)/testsuite && ./make_tests.sh -precommit -p=0

travis_deploy:
	@echo "Deploying results"
	@cd $(SRC_PATH)/testsuite && ./ghp_deploy.sh

travis: travis_tests lcov travis_deploy

dep:	depend

install:
	$(INSTALL) -d "$(DESTDIR)$(prefix)"

	$(MAKE) install-lib

	$(INSTALL) -d "$(DESTDIR)$(prefix)/bin"
	if [ -f bin/gcc/MP4Box$(EXE_SUFFIX) ] ; then \
	$(INSTALL) $(INSTFLAGS) -m 755 bin/gcc/gpac$(EXE_SUFFIX) "$(DESTDIR)$(prefix)/bin" ; \
	fi
ifeq ($(DISABLE_ISOFF),no)
	if [ -f bin/gcc/MP4Box$(EXE_SUFFIX) ] ; then \
	$(INSTALL) $(INSTFLAGS) -m 755 bin/gcc/MP4Box$(EXE_SUFFIX) "$(DESTDIR)$(prefix)/bin" ; \
	fi
endif
	$(INSTALL) -d "$(DESTDIR)$(prefix)/$(lib_dir)/$(moddir)"
ifneq ($(STATIC_BINARY),yes)
	$(INSTALL) bin/gcc/gm_*$(DYN_LIB_SUFFIX) "$(DESTDIR)$(prefix)/$(lib_dir)/$(moddir)" || true
	$(INSTALL) bin/gcc/gf_*$(DYN_LIB_SUFFIX) "$(DESTDIR)$(prefix)/$(lib_dir)/$(moddir)" || true
ifeq ($(CONFIG_OPENHEVC),yes)
	cp -a bin/gcc/libopenhevc* $(DESTDIR)$(prefix)/$(lib_dir)/ || true
endif

endif
	$(INSTALL) -d "$(DESTDIR)$(prefix)/$(man_dir)"
	$(INSTALL) -d "$(DESTDIR)$(prefix)/$(man_dir)/man1"
	$(INSTALL) $(INSTFLAGS) -m 644 $(SRC_PATH)/share/doc/man/mp4box.1 $(DESTDIR)$(prefix)/$(man_dir)/man1/
	$(INSTALL) $(INSTFLAGS) -m 644 $(SRC_PATH)/share/doc/man/gpac.1 $(DESTDIR)$(prefix)/$(man_dir)/man1/
	$(INSTALL) $(INSTFLAGS) -m 644 $(SRC_PATH)/share/doc/man/gpac-filters.1 $(DESTDIR)$(prefix)/$(man_dir)/man1/
	$(INSTALL) -d "$(DESTDIR)$(prefix)/share/gpac"
	$(INSTALL) -d "$(DESTDIR)$(prefix)/share/gpac/res"
	$(INSTALL) -d "$(DESTDIR)$(prefix)/share/gpac/gui"
	$(INSTALL) -d "$(DESTDIR)$(prefix)/share/gpac/gui/icons"
	$(INSTALL) -d "$(DESTDIR)$(prefix)/share/gpac/gui/extensions"
	$(INSTALL) -d "$(DESTDIR)$(prefix)/share/gpac/shaders"
	$(INSTALL) -d "$(DESTDIR)$(prefix)/share/gpac/scripts"
	$(INSTALL) -d "$(DESTDIR)$(prefix)/share/gpac/python"
	$(INSTALL) -d "$(DESTDIR)$(prefix)/share/gpac/vis"
	$(INSTALL) $(INSTFLAGS) -m 644 $(SRC_PATH)/share/default.cfg $(DESTDIR)$(prefix)/share/gpac/

ifneq ($(CONFIG_DARWIN),yes)
	$(INSTALL) -d "$(DESTDIR)$(prefix)/share/icons/hicolor/128x128/apps"
	$(INSTALL) -d "$(DESTDIR)$(prefix)/share/applications"

	$(INSTALL) $(INSTFLAGS) -m 644 $(SRC_PATH)/share/res/gpac.png "$(DESTDIR)$(prefix)/share/icons/hicolor/128x128/apps/"
	$(INSTALL) $(INSTFLAGS) -m 644 $(SRC_PATH)/share/gpac.desktop "$(DESTDIR)$(prefix)/share/applications/"
endif

	$(INSTALL) $(INSTFLAGS) -m 644 $(SRC_PATH)/share/gui/gui.bt "$(DESTDIR)$(prefix)/share/gpac/gui/"
	$(INSTALL) $(INSTFLAGS) -m 644 $(SRC_PATH)/share/gui/gui.js "$(DESTDIR)$(prefix)/share/gpac/gui/"
	$(INSTALL) $(INSTFLAGS) -m 644 $(SRC_PATH)/share/gui/gwlib.js "$(DESTDIR)$(prefix)/share/gpac/gui/"


ifeq ($(CONFIG_DARWIN),yes)
	cp $(SRC_PATH)/share/gui/icons/* "$(DESTDIR)$(prefix)/share/gpac/gui/icons/"
	cp -R $(SRC_PATH)/share/gui/extensions/* "$(DESTDIR)$(prefix)/share/gpac/gui/extensions/"
	cp $(SRC_PATH)/share/shaders/* "$(DESTDIR)$(prefix)/share/gpac/shaders/"
	cp -R $(SRC_PATH)/share/scripts/* "$(DESTDIR)$(prefix)/share/gpac/scripts/"
	cp -R $(SRC_PATH)/share/python/* "$(DESTDIR)$(prefix)/share/gpac/python/"
	cp $(SRC_PATH)/share/res/* "$(DESTDIR)$(prefix)/share/gpac/res/"
	cp -R $(SRC_PATH)/share/vis/* "$(DESTDIR)$(prefix)/share/gpac/vis/"
else
	cp --no-preserve=mode,ownership,timestamp $(SRC_PATH)/share/gui/icons/* $(DESTDIR)$(prefix)/share/gpac/gui/icons/
	cp -R --no-preserve=mode,ownership,timestamp $(SRC_PATH)/share/gui/extensions/* $(DESTDIR)$(prefix)/share/gpac/gui/extensions/
	cp --no-preserve=mode,ownership,timestamp $(SRC_PATH)/share/shaders/* $(DESTDIR)$(prefix)/share/gpac/shaders/
	cp -R --no-preserve=mode,ownership,timestamp $(SRC_PATH)/share/scripts/* $(DESTDIR)$(prefix)/share/gpac/scripts/
	cp -R --no-preserve=mode,ownership,timestamp $(SRC_PATH)/share/python/* $(DESTDIR)$(prefix)/share/gpac/python/
	cp --no-preserve=mode,ownership,timestamp $(SRC_PATH)/share/res/* $(DESTDIR)$(prefix)/share/gpac/res/
	cp -R --no-preserve=mode,ownership,timestamp $(SRC_PATH)/share/vis/* $(DESTDIR)$(prefix)/share/gpac/vis/
endif

lninstall:
	$(INSTALL) -d "$(DESTDIR)$(prefix)"
	$(INSTALL) -d "$(DESTDIR)$(prefix)/$(lib_dir)"
	$(INSTALL) -d "$(DESTDIR)$(prefix)/bin"
	ln -sf $(BUILD_PATH)/bin/gcc/gpac$(EXE_SUFFIX) $(DESTDIR)$(prefix)/bin/gpac$(EXE_SUFFIX)
ifeq ($(DISABLE_ISOFF),no)
	ln -sf $(BUILD_PATH)/bin/gcc/MP4Box$(EXE_SUFFIX) $(DESTDIR)$(prefix)/bin/MP4Box$(EXE_SUFFIX)
endif
ifeq ($(CONFIG_DARWIN),yes)
	ln -s $(BUILD_PATH)/bin/gcc/libgpac$(DYN_LIB_SUFFIX) $(DESTDIR)$(prefix)/$(lib_dir)/libgpac.$(VERSION_SONAME)$(DYN_LIB_SUFFIX)
	ln -sf $(DESTDIR)$(prefix)/$(lib_dir)/libgpac.$(VERSION_SONAME)$(DYN_LIB_SUFFIX) $(DESTDIR)$(prefix)/$(lib_dir)/libgpac.$(VERSION_MAJOR)$(DYN_LIB_SUFFIX)
	ln -sf $(DESTDIR)$(prefix)/$(lib_dir)/libgpac.$(VERSION_SONAME)$(DYN_LIB_SUFFIX) $(DESTDIR)$(prefix)/$(lib_dir)/libgpac$(DYN_LIB_SUFFIX)

	ln -s $(BUILD_PATH)/bin/gcc/ $(DESTDIR)$(prefix)/$(lib_dir)/gpac
	ln -s $(SRC_PATH)/share/ $(DESTDIR)$(prefix)/share/gpac
else
	ln -s $(BUILD_PATH)/bin/gcc/libgpac$(DYN_LIB_SUFFIX).$(VERSION_SONAME) $(DESTDIR)$(prefix)/$(lib_dir)/libgpac$(DYN_LIB_SUFFIX).$(VERSION_SONAME)
	ln -sf $(DESTDIR)$(prefix)/$(lib_dir)/libgpac$(DYN_LIB_SUFFIX).$(VERSION_SONAME) $(DESTDIR)$(prefix)/$(lib_dir)/libgpac.so.$(VERSION_MAJOR)
	ln -sf $(DESTDIR)$(prefix)/$(lib_dir)/libgpac$(DYN_LIB_SUFFIX).$(VERSION_SONAME) $(DESTDIR)$(prefix)/$(lib_dir)/libgpac.so
	ln -s $(BUILD_PATH)/bin/gcc/ $(DESTDIR)$(prefix)/$(lib_dir)/gpac

	ln -s $(SRC_PATH)/share/ $(DESTDIR)$(prefix)/share/gpac
	ln -sf $(DESTDIR)$(prefix)/share/gpac/res/gpac.png $(DESTDIR)/usr/share/icons/hicolor/128x128/apps/gpac.png
	ln -sf $(SRC_PATH)/share/gpac.desktop $(DESTDIR)/usr/share/applications/

ifeq ($(DESTDIR)$(prefix),$(prefix))
	ldconfig || true
endif

endif

uninstall:
	$(MAKE) -C applications uninstall
	$(MAKE) uninstall-lib
	rm -rf $(DESTDIR)$(prefix)/$(lib_dir)/$(moddir)
	rm -rf $(DESTDIR)$(prefix)/bin/MP4Box
	rm -rf $(DESTDIR)$(prefix)/bin/gpac
	rm -rf $(DESTDIR)$(prefix)/$(man_dir)/man1/mp4box.1
	rm -rf $(DESTDIR)$(prefix)/$(man_dir)/man1/gpac.1
	rm -rf $(DESTDIR)$(prefix)/$(man_dir)/man1/gpac-filters.1
	rm -rf $(DESTDIR)$(prefix)/share/gpac
	rm -rf $(DESTDIR)$(prefix)/share/icons/hicolor/128x128/apps/gpac.png
	rm -rf $(DESTDIR)$(prefix)/share/applications/gpac.desktop


installdylib:
ifneq ($(STATIC_BINARY),yes)

	$(INSTALL) -d "$(DESTDIR)$(prefix)/$(lib_dir)"

ifeq ($(CONFIG_WIN32),yes)
	$(INSTALL) -d "$(DESTDIR)$(prefix)/bin"

	$(INSTALL) $(INSTFLAGS) -m 755 bin/gcc/libgpac.dll.a $(DESTDIR)$(prefix)/$(lib_dir)
	$(INSTALL) $(INSTFLAGS) -m 755 bin/gcc/libgpac.dll $(DESTDIR)$(prefix)/bin
else

ifeq ($(DEBUGBUILD),no)
	$(STRIP) -S bin/gcc/libgpac$(DYN_LIB_SUFFIX)
endif

ifeq ($(CONFIG_DARWIN),yes)
	$(INSTALL) -m 755 bin/gcc/libgpac$(DYN_LIB_SUFFIX) $(DESTDIR)$(prefix)/$(lib_dir)/libgpac.$(VERSION_SONAME)$(DYN_LIB_SUFFIX)
	ln -sf libgpac.$(VERSION_SONAME)$(DYN_LIB_SUFFIX) $(DESTDIR)$(prefix)/$(lib_dir)/libgpac.$(VERSION_MAJOR)$(DYN_LIB_SUFFIX)
	ln -sf libgpac.$(VERSION_SONAME)$(DYN_LIB_SUFFIX) $(DESTDIR)$(prefix)/$(lib_dir)/libgpac$(DYN_LIB_SUFFIX)
else
	$(INSTALL) $(INSTFLAGS) -m 755 bin/gcc/libgpac$(DYN_LIB_SUFFIX).$(VERSION_SONAME) $(DESTDIR)$(prefix)/$(lib_dir)/libgpac$(DYN_LIB_SUFFIX).$(VERSION_SONAME)
	ln -sf libgpac$(DYN_LIB_SUFFIX).$(VERSION_SONAME) $(DESTDIR)$(prefix)/$(lib_dir)/libgpac.so.$(VERSION_MAJOR)
	ln -sf libgpac$(DYN_LIB_SUFFIX).$(VERSION_SONAME) $(DESTDIR)$(prefix)/$(lib_dir)/libgpac.so
ifeq ($(DESTDIR)$(prefix),$(prefix))
	ldconfig || true
endif
endif

endif

endif


uninstalldylib:
	rm -rf $(DESTDIR)$(prefix)/$(lib_dir)/libgpac*
ifeq ($(CONFIG_WIN32),yes)
	rm -rf "$(DESTDIR)$(prefix)/bin/libgpac*"
endif

install-lib:
	$(INSTALL) -d "$(DESTDIR)$(prefix)/include/gpac"
	$(INSTALL) -d "$(DESTDIR)$(prefix)/include/gpac/internal"
	$(INSTALL) -d "$(DESTDIR)$(prefix)/include/gpac/modules"

	$(INSTALL) $(INSTFLAGS) -m 644 $(SRC_PATH)/include/gpac/*.h "$(DESTDIR)$(prefix)/include/gpac"
	$(INSTALL) $(INSTFLAGS) -m 644 $(SRC_PATH)/include/gpac/internal/*.h "$(DESTDIR)$(prefix)/include/gpac/internal"
	$(INSTALL) $(INSTFLAGS) -m 644 $(SRC_PATH)/include/gpac/modules/*.h "$(DESTDIR)$(prefix)/include/gpac/modules"

	$(INSTALL) $(INSTFLAGS) -m 644 config.h "$(DESTDIR)$(prefix)/include/gpac/configuration.h" || true

	$(INSTALL) -d "$(DESTDIR)$(prefix)/$(lib_dir)"
	$(INSTALL) $(INSTFLAGS) -m 644 "./bin/gcc/libgpac_static.a" "$(DESTDIR)$(prefix)/$(lib_dir)" || true

	$(INSTALL) -d $(DESTDIR)$(prefix)/$(lib_dir)/pkgconfig
	$(INSTALL) $(INSTFLAGS) -m 644 gpac.pc "$(DESTDIR)$(prefix)/$(lib_dir)/pkgconfig"

	$(MAKE) installdylib


uninstall-lib:
	rm -rf "$(DESTDIR)$(prefix)/include/gpac/internal"
	rm -rf "$(DESTDIR)$(prefix)/include/gpac/modules"
	rm -rf "$(DESTDIR)$(prefix)/include/gpac/enst"
	rm -rf "$(DESTDIR)$(prefix)/include/gpac"
	rm -f  "$(DESTDIR)$(prefix)/$(lib_dir)/libgpac_static.a"
	rm -f  "$(DESTDIR)$(prefix)/$(lib_dir)/pkgconfig/gpac.pc"
	$(MAKE) uninstalldylib

ifeq ($(CONFIG_DARWIN),yes)
dmg:
	./mkdmg.sh $(arch)
endif

ifeq ($(CONFIG_LINUX),yes)
deb:
	git checkout --	debian/changelog
	fakeroot debian/rules clean
	# add version to changelog for final filename
	sed -i -r "s/^(\w+) \(([0-9\.]+)(-[A-Z]+)?\)/\1 (\2\3-rev$(VERSION)-$(BRANCH))/" debian/changelog
	fakeroot debian/rules configure
	fakeroot debian/rules binary
	rm -rf debian/
	git checkout debian
endif

help:
	@echo "Input to GPAC make:"
	@echo "depend/dep: builds dependencies (dev only)"
	@echo "all (void): builds main library, programs and plugins"
	@echo "lib: builds GPAC library only (libgpac.so)"
	@echo "apps: builds programs only"
	@echo "modules: builds modules only"
	@echo "sggen: builds scene graph generators"
	@echo
	@echo "clean: clean src repository"
	@echo "distclean: clean src repository and host config file"
	@echo
	@echo "install: install applications and modules on system"
	@echo "uninstall: uninstall applications and modules"
ifeq ($(CONFIG_DARWIN),yes)
	@echo "dmg: creates DMG package file for OSX"
endif
ifeq ($(CONFIG_LINUX),yes)
	@echo "deb: creates DEB package file for debian based systems"
endif
	@echo
	@echo "install-lib: install gpac library (dyn and static) and headers <gpac/*.h>, <gpac/modules/*.h> and <gpac/internal/*.h>"
	@echo "uninstall-lib: uninstall gpac library (dyn and static) and headers"
	@echo
	@echo "test_suite: run all tests. For more info, check https://github.com/gpac/testsuite"
	@echo
	@echo "doc:  build libgpac documentation in gpac/doc"
	@echo "man:  build gpac man files in gpac/doc/man (must have latest build binaries installed)"
	@echo
	@echo "lcov: generate lcov files"
	@echo "lcov_clean: clean all lcov/gcov files"


-include .depend
