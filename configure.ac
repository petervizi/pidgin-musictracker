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
        [0.2.2],
        [Arijit De de.arijit@gmail.com],
        [musictracker])
AC_CONFIG_AUX_DIR(config)
AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE([dist-bzip2])

LF_CONFIGURE_CC
LF_HOST_TYPE
LF_SET_WARNINGS
AC_PROG_LIBTOOL

AC_ARG_ENABLE(debug,
[  --enable-debug          compile with debugging support],,enable_debug=no)
if test "x$enable_debug" = "xyes" ; then
	DEBUG_CFLAGS="-g -O0"
fi
AC_SUBST(DEBUG_CFLAGS)

PKG_CHECK_MODULES([GAIM],[gaim >= 2.0.0],,AC_MSG_ERROR([
*** Gaim 2.0 is required to build MusicTracker; please make sure you have the
*** Gaim development files installed. The latest version of Gaim is always
*** available at http://gaim.sf.net/.]))
AC_SUBST(GAIM_CFLAGS)
GAIM_LIBDIR=`pkg-config --variable=libdir gaim`
GAIM_DATADIR=`pkg-config --variable=datadir gaim`
AC_SUBST(GAIM_LIBDIR)
AC_SUBST(GAIM_DATADIR)

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

PKG_CHECK_MODULES([DBUS],[dbus-glib-1 >= 0.62],,AC_MSG_ERROR([
*** DBUS 0.68+ is required to build MusicTracker; please make sure you have the
*** DBUS development files installed. The latest version of DBUS is always
*** available at http://dbus.freedesktop.net/.]))
AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)

AC_CONFIG_FILES([
   Makefile
   README
   doc/Makefile
   m4/Makefile
   src/Makefile
   protocols/Makefile
   protocols/msn/Makefile
])

AC_OUTPUT
