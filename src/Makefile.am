# Copyright (C) 2006 Arijit De <de.arijit@gmail.com>
#  
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without 
# modifications, as long as this notice is preserved.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

localedir = $(datadir)/locale
pidgindir = $(libdir)/pidgin

pidgin_LTLIBRARIES = musictracker.la

musictracker_la_SOURCES = musictracker.c \
						  utils.c \
						  actions.c \
						  filter.c \
						  preferences.c \
						  amarok.c \
						  lastfm.c \
						  exaile.c \
						  audacious.c \
						  xmmsctrl.c \
						  mpd.c \
						  libmpdclient.c \
						  rhythmbox.c \
						  banshee.c \
						  vagalume.c \
						  quodlibet.c \
                                                  squeezecenter.c \
						  listen.c \
						  xmms2.c \
						  mpris.c \
						  dbusbird.c

musictracker_la_LDFLAGS = -module -avoid-version
musictracker_la_LIBADD = $(GLIB_LIBS) $(GTK_LIBS) $(DBUS_LIBS) @LTLIBINTL@

AM_CPPFLAGS = \
        -DDATADIR=\"$(PIDGIN_DATADIR)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-Werror -W -Wall -Wno-unused-parameter \
	-std=gnu99 \
        $(PIDGIN_CFLAGS) \
        $(GLIB_CFLAGS) \
		$(GTK_CFLAGS) \
		$(DBUS_CFLAGS) \
		$(DEBUG_CFLAGS) \
		$(XMMSCLIENT_CFLAGS)

# Windows sources should also be included in distribution archive
EXTRA_DIST = musictracker.h \
			 utils.h \
			 actions.h \
			 filter.h \
			 libmpdclient.h \
			 Makefile.mingw \
			 winamp.c \
			 foobar2000.c \
			 wa_ipc.h \
			 wmp.c \
			 iTunesCOMInterface.h \
			 iTunesCOMInterface_i.c \
			 itunes.cpp
