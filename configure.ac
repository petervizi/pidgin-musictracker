# Copyright (C) 2006 Arijit De <de.arijit@gmail.com>
#  
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without 
# modifications, as long as this notice is preserved.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

AC_INIT([MusicTracker],
        [m4_esyscmd([cat version | tr -d '\n'])],
        [pidgin-musictracker@googlegroups.com],
        [musictracker])
AC_CONFIG_AUX_DIR(config)
AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE([dist-bzip2])

AC_PROG_CC
AC_PROG_LIBTOOL

AC_ARG_ENABLE(debug,
[  --enable-debug          compile with debugging support],,enable_debug=no)
if test "x$enable_debug" = "xyes" ; then
	DEBUG_CFLAGS="-g -O0"
fi
AC_SUBST(DEBUG_CFLAGS)

PKG_CHECK_MODULES([PIDGIN],[pidgin >= 2.0.0],,AC_MSG_ERROR([
*** Pidgin 2.0+ is required to build MusicTracker; please make sure you have the
*** Pidgin development files installed. The latest version of Pidgin is always
*** available at http://pidgin.im/.]))
AC_SUBST(PIDGIN_CFLAGS)
PIDGIN_LIBDIR=`pkg-config --variable=libdir pidgin`
PIDGIN_DATADIR=`pkg-config --variable=datadir pidgin`
AC_SUBST(PIDGIN_LIBDIR)
AC_SUBST(PIDGIN_DATADIR)

AM_PATH_GLIB_2_0(2.0.0,,AC_MSG_ERROR([
*** GLib 2.0 is required to build MusicTracker; please make sure you have the
*** GLib development headers installed. The latest version of GLib is always
*** available at http://www.gtk.org/.]))
AC_SUBST(GLIB_LIBS)
AC_SUBST(GLIB_CFLAGS)

AM_PATH_GTK_2_0(2.0.0,,AC_MSG_ERROR([
*** GTK+ 2.0 is required to build MusicTracker; please make sure you have the
*** GTK+ development headers installed. The latest version of GTK+ is always
*** available at http://www.gtk.org/.]))
AC_SUBST(GTK_LIBS)
AC_SUBST(GTK_CFLAGS)

PKG_CHECK_MODULES([DBUS],[dbus-glib-1 >= 0.73],,AC_MSG_ERROR([
*** DBUS 0.73+ is required to build MusicTracker; please make sure you have the
*** DBUS development files installed. The latest version of DBUS is always
*** available at http://dbus.freedesktop.net/.]))
AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)

PKG_CHECK_MODULES([XMMSCLIENT], [xmms2-client >= 0.5],[AC_DEFINE(HAVE_XMMSCLIENT, 1, "XMMS Client")],AC_MSG_WARN([
*** XMMS2 is required to build XMMS2 support for MusicTracker; please make sure
*** you have the XMMS2 development files installed. The latest version of XMMS2
*** is always available at http://wiki.xmms2.xmms.se/.]))
AC_SUBST(XMMSCLIENT_CFLAGS)
AC_SUBST(XMMSCLIENT_LIBS)

AC_CHECK_LIB(pcre,pcre_exec,,AC_MSG_ERROR([
*** PCRE library is needed; please make sure you have the PCRE development
*** files installed. The latest version of PCRE is always available at
*** http://www.pcre.org/.]))

AC_CONFIG_FILES([
   Makefile
   README
   doc/Makefile
   m4/Makefile
   src/Makefile
   installer/Makefile
])

AC_OUTPUT
