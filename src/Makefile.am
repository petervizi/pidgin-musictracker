# Copyright (C) 2006 Arijit De <de.arijit@gmail.com>
#  
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without 
# modifications, as long as this notice is preserved.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

gaimdir = $(GAIM_LIBDIR)/gaim
gaim_LTLIBRARIES = musictracker.la

musictracker_la_SOURCES = musictracker.c \
						  utils.c \
						  amarok.c \
						  xmms.c \
						  actions.c \
						  filter.c \
						  exaile.c \
						  xmmsctrl.c

musictracker_la_LDFLAGS = -module -avoid-version
musictracker_la_LIBADD = $(GLIB_LIBS) $(GTK_LIBS) $(DBUS_LIBS)

AM_CPPFLAGS = \
        -DDATADIR=\"$(GAIM_DATADIR)\" \
        $(GAIM_CFLAGS) \
        $(GLIB_CFLAGS) \
		$(GTK_CFLAGS) \
		$(DBUS_CFLAGS) \
		$(DEBUG_CFLAGS)
		
EXTRA_DIST = musictracker.h \
			 utils.h \
			 actions.h \
			 filter.h
