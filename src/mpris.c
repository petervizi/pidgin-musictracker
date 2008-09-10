/*
 * musictracker
 * mpris.c
 * retrieve track info over DBUS from MRPIS-compliant player
 *
 * Copyright (c) 2007 Sabin Iacob (m0n5t3r) <iacobs@m0n5t3r.info>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

// 
// totally ripped off from pidgin-mpris http://m0n5t3r.info/work/pidgin-mpris
// keep the signal/cb structure around for the moment, even though we have
// to bodge it to integrate with our current polling architecture, it may
// come in useful later when we change it...
//

//
// See http://wiki.xmms2.xmms.se/wiki/MPRIS for MRPIS specification
//
// Notes:
//
// BMPx 0.40 doesn't have GetStatus, so we need to listen for the StatusChange signal
// it also uses a uint64 value for the time metadata
//
// Audacious 1.4 and BMPx 0.40 implement the wrong signature for StatusChange/GetStatus,
// returning a single int rather than 4 ints
//
// VLC 0.9.1 seems to be quite slow to respond to requests, so timeout needs to be large
//

#include <string.h>
#include <glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>

#include "musictracker.h"

#define DBUS_MPRIS_NAMESPACE		"org.mpris."
#define DBUS_MPRIS_PLAYER		"org.freedesktop.MediaPlayer"
#define DBUS_MPRIS_PLAYER_PATH		"/Player"
#define DBUS_MPRIS_TRACK_SIGNAL		"TrackChange"
#define DBUS_MPRIS_STATUS_SIGNAL	"StatusChange"

#define DBUS_TYPE_G_STRING_VALUE_HASHTABLE (dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE))
#define DBUS_TYPE_MPRIS_STATUS             (dbus_g_type_get_struct ("GValueArray", G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INVALID))

#define mpris_debug(fmt, ...)	purple_debug(PURPLE_DEBUG_INFO, "MPRIS", \
					fmt, ## __VA_ARGS__);
#define mpris_error(fmt, ...)	purple_debug(PURPLE_DEBUG_ERROR, "MPRIS", \
					fmt, ## __VA_ARGS__);

static DBusGConnection *bus;

#define MPRIS_HINT_STATUSCHANGE 1

typedef struct {
	const char *namespace; // append to 'org.mpris.' to get player namespace
	unsigned int hints;
	DBusGProxy *proxy;
	gchar *player_name;
	struct TrackInfo ti;
} pidginmpris_t;

#define TRACKINFO_INITIALIZER { "", "", "", 0, 0, 0, 0 }

static pidginmpris_t players[] =
  {
    { "amarok", 0, 0, 0, TRACKINFO_INITIALIZER },
    { "audacious", MPRIS_HINT_STATUSCHANGE, 0, 0, TRACKINFO_INITIALIZER },
    { "bmp", MPRIS_HINT_STATUSCHANGE, 0, 0, TRACKINFO_INITIALIZER },
    { "dragonplayer", 0, 0, 0, TRACKINFO_INITIALIZER },
    { "vlc", 0, 0, 0, TRACKINFO_INITIALIZER },
    { "xmms2", 0, 0, 0, TRACKINFO_INITIALIZER },
    { 0, 0, 0, 0, TRACKINFO_INITIALIZER }
  };

static void
mpris_track_signal_cb(DBusGProxy *player_proxy, GHashTable *table, struct TrackInfo *ti)
{
	GValue *value;

	/* fetch values from hash table */
	value = (GValue *) g_hash_table_lookup(table, "artist");
	if (value != NULL && G_VALUE_HOLDS_STRING(value)) {
		g_strlcpy(ti->artist, g_value_get_string(value), STRLEN);
	}
	value = (GValue *) g_hash_table_lookup(table, "album");
	if (value != NULL && G_VALUE_HOLDS_STRING(value)) {
		g_strlcpy(ti->album, g_value_get_string(value), STRLEN);
	}
	value = (GValue *) g_hash_table_lookup(table, "title");
	if (value != NULL && G_VALUE_HOLDS_STRING(value)) {
		g_strlcpy(ti->track, g_value_get_string(value), STRLEN);
	}
	value = (GValue *) g_hash_table_lookup(table, "time");
	if (value != NULL) 
          {
            if (G_VALUE_HOLDS_UINT(value)) {
              ti->totalSecs =  g_value_get_uint(value);
            } else if  (G_VALUE_HOLDS_UINT64(value)) {
              ti->totalSecs =  g_value_get_uint64(value);
            }
          }

        // debug dump
        GHashTableIter iter;
        gpointer key;
        g_hash_table_iter_init(&iter, table);
        while (g_hash_table_iter_next(&iter, &key, (gpointer) &value))
          {
            char *s = g_strdup_value_contents(value);
            mpris_debug("For key '%s' value is '%s' as a %s\n", (char *)key, s, G_VALUE_TYPE_NAME(value));
            g_free(s);
          }
}

static void
mpris_status_signal_int_cb(DBusGProxy *player_proxy, gint status, struct TrackInfo *ti)
{
	mpris_debug("StatusChange %d\n", status);

        switch (status)
          {
          case 0:
            ti->status = STATUS_NORMAL;
            break;
          case 1:
            ti->status = STATUS_PAUSED;
            break;
          case 2:
          default:
            ti->status = STATUS_OFF;
            break;
          }
}

static void
mpris_status_signal_struct_cb(DBusGProxy *player_proxy, GValueArray *sigstruct, struct TrackInfo *ti)
{
        int status = -1;
        if (sigstruct)
          {
            GValue *v = g_value_array_get_nth(sigstruct, 0);
            status = g_value_get_int(v);
          }

	mpris_debug("StatusChange %d\n", status);

        switch (status)
          {
          case 0:
            ti->status = STATUS_NORMAL;
            break;
          case 1:
            ti->status = STATUS_PAUSED;
            break;
          case 2:
          default:
            ti->status = STATUS_OFF;
            break;
          }
}

static void mpris_connect_dbus_signals(int i)
{
	players[i].proxy = dbus_g_proxy_new_for_name(bus,
			players[i].player_name, DBUS_MPRIS_PLAYER_PATH, DBUS_MPRIS_PLAYER);

	dbus_g_proxy_add_signal(players[i].proxy, DBUS_MPRIS_TRACK_SIGNAL,
			DBUS_TYPE_G_STRING_VALUE_HASHTABLE, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal(players[i].proxy, DBUS_MPRIS_TRACK_SIGNAL,
                                    G_CALLBACK(mpris_track_signal_cb), &(players[i].ti), NULL);

        if (players[i].hints & MPRIS_HINT_STATUSCHANGE)
          {
            dbus_g_proxy_add_signal(players[i].proxy, DBUS_MPRIS_STATUS_SIGNAL,
                                    G_TYPE_INT, G_TYPE_INVALID);
            dbus_g_proxy_connect_signal(players[i].proxy, DBUS_MPRIS_STATUS_SIGNAL,
                                        G_CALLBACK(mpris_status_signal_int_cb), &(players[i].ti), NULL);
          }
        else
          {
            dbus_g_proxy_add_signal(players[i].proxy, DBUS_MPRIS_STATUS_SIGNAL,
                                    DBUS_TYPE_MPRIS_STATUS, G_TYPE_INVALID);
            dbus_g_proxy_connect_signal(players[i].proxy, DBUS_MPRIS_STATUS_SIGNAL,
                                        G_CALLBACK(mpris_status_signal_struct_cb), &(players[i].ti), NULL);
          }
}

static gboolean mpris_app_running(int i)
{
	DBusGProxy *player = dbus_g_proxy_new_for_name_owner(bus, 
			players[i].player_name, DBUS_MPRIS_PLAYER_PATH, DBUS_MPRIS_PLAYER, NULL);
	
	if(!player)
		return FALSE;
	
	g_object_unref(player);
	return TRUE;
}

static gboolean
load_plugin(void)
{
	/* initialize dbus connection */
	GError *error = 0;
	bus = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
	if (!bus) {
		mpris_error("failed to connect to the dbus daemon: %s\n", error->message);
		g_error_free (error);
		return FALSE;
	}

        for (int i = 0; players[i].namespace != 0; i++)
          { 
            mpris_debug("Setting up %s\n", players[i].namespace);
            
            players[i].player_name = g_strconcat(DBUS_MPRIS_NAMESPACE, players[i].namespace, NULL);

            /* connect mpris's dbus signals */
            mpris_connect_dbus_signals(i);

            mpris_status_signal_int_cb(NULL, -1, &(players[i].ti));
          }

	return TRUE;
}

gboolean get_mpris_info(struct TrackInfo* ti)
{
  if (!bus)
    load_plugin();

  ti->status = STATUS_OFF;
  
  for (int i = 0; players[i].namespace != 0; i++)
    { 
      GError *error = 0;
      mpris_debug("Trying %s\n", players[i].player_name);

      if(mpris_app_running(i)) {
        error = 0;
        gboolean result;
        int status;
        if (players[i].hints & MPRIS_HINT_STATUSCHANGE)
          {
            result = dbus_g_proxy_call_with_timeout(players[i].proxy, "GetStatus", DBUS_TIMEOUT*10, &error,
                                                    G_TYPE_INVALID,
                                                    G_TYPE_INT, &status,
                                                    G_TYPE_INVALID);

            if (result)
              {
                mpris_status_signal_int_cb(NULL, status, &(players[i].ti));
              }
          }
        else
          {
            GValueArray *s = 0;
            result = dbus_g_proxy_call_with_timeout(players[i].proxy, "GetStatus", DBUS_TIMEOUT*10, &error,
                                                    G_TYPE_INVALID,
                                                    DBUS_TYPE_MPRIS_STATUS, &s,
                                                    G_TYPE_INVALID);

            if (result)
              {
                mpris_status_signal_struct_cb(NULL, s, &(players[i].ti));
                g_value_array_free(s);
              }
          }

        if (!result)
          {
            if (error)
              {
                mpris_debug("GetStatus failed %s\n", error->message);
                g_error_free(error);
              }
            else
              {
                mpris_debug("GetStatus failed with no error\n");
              }
          }

        error = 0;
        GHashTable *table = NULL;
        if(dbus_g_proxy_call_with_timeout(players[i].proxy, "GetMetadata", DBUS_TIMEOUT*10, &error,
                                          G_TYPE_INVALID, DBUS_TYPE_G_STRING_VALUE_HASHTABLE, &table, 
                                          G_TYPE_INVALID)) {
          mpris_track_signal_cb(NULL, table, &(players[i].ti));
          g_hash_table_destroy(table);
        }
        else
          {
            if (error)
              {
                mpris_debug("GetMetaData failed %s\n", error->message);
                g_error_free(error);
              }
            else
              {
                mpris_debug("GetMetaData failed with no error\n");
              }
            
          }

        error = 0;
        int position;
        if(dbus_g_proxy_call_with_timeout(players[i].proxy, "PositionGet", DBUS_TIMEOUT*10, &error,
                                          G_TYPE_INVALID, G_TYPE_INT, &position, 
                                          G_TYPE_INVALID)) {
          players[i].ti.currentSecs = position/1000;
        }
        else
          {
            if (error)
              {
                mpris_debug("PositionGet failed %s\n", error->message);
                g_error_free(error);
              }
            else
              {
                mpris_debug("PositionGet failed with no error\n");
              }
          }

        players[i].ti.player = players[i].namespace;

        if (players[i].ti.status != STATUS_OFF)
          {
            *ti = players[i].ti;
          break;
          }
      }
    }
 
  return (ti->status != STATUS_OFF);
}
