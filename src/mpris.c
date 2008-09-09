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
// BMPx 0.40 doesn't have GetStatus, so we need to listen for the StatusChange signal
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

#define mpris_debug(fmt, ...)	purple_debug(PURPLE_DEBUG_INFO, "MPRIS", \
					fmt, ## __VA_ARGS__);
#define mpris_error(fmt, ...)	purple_debug(PURPLE_DEBUG_ERROR, "MPRIS", \
					fmt, ## __VA_ARGS__);

static DBusGConnection *bus;

typedef struct {
	const char *namespace; // append to 'org.mpris.' to get player namespace
	DBusGProxy *player;
	gchar *player_name;
} pidginmpris_t;

pidginmpris_t players[] =
  {
    { "amarok", 0, 0 },
    { "audacious", 0, 0 },
    { "bmp", 0, 0 },
    { "vlc", 0, 0 },
    { "xmms2", 0, 0 },
    { 0, 0, 0 }
  };

static
struct TrackInfo mpris_ti;

static void
mpris_track_signal_cb(DBusGProxy *player_proxy, GHashTable *table, gpointer data)
{
	GValue *value;

	/* fetch values from hash table */
	value = (GValue *) g_hash_table_lookup(table, "artist");
	if (value != NULL && G_VALUE_HOLDS_STRING(value)) {
		g_strlcpy(mpris_ti.artist, g_value_get_string(value), STRLEN);
	}
	value = (GValue *) g_hash_table_lookup(table, "album");
	if (value != NULL && G_VALUE_HOLDS_STRING(value)) {
		g_strlcpy(mpris_ti.album, g_value_get_string(value), STRLEN);
	}
	value = (GValue *) g_hash_table_lookup(table, "title");
	if (value != NULL && G_VALUE_HOLDS_STRING(value)) {
		g_strlcpy(mpris_ti.track, g_value_get_string(value), STRLEN);
	}
	value = (GValue *) g_hash_table_lookup(table, "time");
	if (value != NULL && G_VALUE_HOLDS_UINT(value)) {
		mpris_ti.totalSecs =  g_value_get_uint(value);
	}

        // debug dump
        GHashTableIter iter;
        gpointer key;
        g_hash_table_iter_init(&iter, table);
        while (g_hash_table_iter_next(&iter, &key, (gpointer) &value))
          {
            char *s = g_strdup_value_contents(value);
            mpris_debug("For key '%s' value is '%s'\n", (char *)key, s);
            g_free(s);
          }
}

static void
mpris_status_signal_cb(DBusGProxy *player_proxy, gint status, gpointer data)
{
	mpris_debug("StatusChange %d\n", status);

        switch (status)
          {
          case 0:
            mpris_ti.status = STATUS_NORMAL;
            break;
          case 1:
            mpris_ti.status = STATUS_PAUSED;
            break;
          case 2:
          default:
            mpris_ti.status = STATUS_OFF;
            break;
          }
}

static void mpris_connect_dbus_signals(int i)
{
	players[i].player = dbus_g_proxy_new_for_name(bus,
			players[i].player_name, DBUS_MPRIS_PLAYER_PATH, DBUS_MPRIS_PLAYER);

	dbus_g_proxy_add_signal(players[i].player, DBUS_MPRIS_TRACK_SIGNAL,
			DBUS_TYPE_G_STRING_VALUE_HASHTABLE, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal(players[i].player, DBUS_MPRIS_TRACK_SIGNAL,
			G_CALLBACK(mpris_track_signal_cb), NULL, NULL);

	dbus_g_proxy_add_signal(players[i].player, DBUS_MPRIS_STATUS_SIGNAL,
			G_TYPE_INT, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal(players[i].player, DBUS_MPRIS_STATUS_SIGNAL,
			G_CALLBACK(mpris_status_signal_cb), NULL, NULL);
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
            players[i].player_name = g_strconcat(DBUS_MPRIS_NAMESPACE, players[i].namespace, NULL);

            /* connect mpris's dbus signals */
            mpris_connect_dbus_signals(i);
          }

	mpris_status_signal_cb(NULL, -1, NULL);

	return TRUE;
}

gboolean get_mpris_info(struct TrackInfo* ti)
{
  if (!bus)
    load_plugin();

  for (int i = 0; players[i].namespace != 0; i++)
    { 
      GError *error = 0;
      mpris_debug("Trying %s\n", players[i].player_name);

      if(mpris_app_running(i)) {
        error = 0;
        int status;
        if(dbus_g_proxy_call_with_timeout(players[i].player, "GetStatus", DBUS_TIMEOUT, &error,
                                          G_TYPE_INVALID, G_TYPE_INT, &status, 
                                          G_TYPE_INVALID)) {
          mpris_status_signal_cb(NULL, status, NULL);
        } 
        else
          {
            mpris_debug("GetStatus failed %s\n", error->message);
            g_error_free (error);
          }

        error = 0;
        GHashTable *table = NULL;
        if(dbus_g_proxy_call_with_timeout(players[i].player, "GetMetadata", DBUS_TIMEOUT, &error,
                                          G_TYPE_INVALID, DBUS_TYPE_G_STRING_VALUE_HASHTABLE, &table, 
                                          G_TYPE_INVALID)) {
          mpris_track_signal_cb(NULL, table, NULL);
          g_hash_table_destroy(table);
        }
        else
          {
            mpris_debug("GetMetaData failed %s\n", error->message);
            g_error_free (error);
          }

        error = 0;
        int position;
        if(dbus_g_proxy_call_with_timeout(players[i].player, "PositionGet", DBUS_TIMEOUT, &error,
                                          G_TYPE_INVALID, G_TYPE_INT, &position, 
                                          G_TYPE_INVALID)) {
          mpris_ti.currentSecs = position/1000;
        }
        else
          {
            mpris_debug("PositionGet failed %s\n", error->message);
            g_error_free (error);
          }

        mpris_ti.player = players[i].namespace;

        if (mpris_ti.status != STATUS_OFF)
          break;
      }
    }

  *ti = mpris_ti;
 
  return (ti->status != STATUS_OFF);
}
