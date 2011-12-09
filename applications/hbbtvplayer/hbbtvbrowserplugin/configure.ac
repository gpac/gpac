

AC_INIT([hbbtvbrowserplugin], [0.1.0], [stanislas.selle@telecom-paristech.fr])

AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AM_INIT_AUTOMAKE([foreign -Wall -Werror])
AC_PROG_CC
AC_PROG_LIBTOOL

PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.28.0])
PKG_CHECK_MODULES([GTK], [gtk+-2.0 >= 2.20.1])
AC_SUBST([PLUGINDIR], [/usr/lib/mozilla/plugins])
LT_INIT
AC_CONFIG_FILES([makefile src/makefile hbbtvbrowserplugin.pc])
AC_OUTPUT
