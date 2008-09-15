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

static DBusGConnection *bus = NULL;

#define MPRIS_HINT_STATUSCHANGE 1
#define MPRIS_HINT_METADATA_METHOD_CASE 2

typedef struct {
	unsigned int hints;
	DBusGProxy *proxy;
	gchar *service_name;
	gchar *player_name;
	struct TrackInfo ti;
} pidginmpris_t;

static GHashTable *players = NULL;

static
void
mpris_debug_dump_helper(gpointer key, gpointer value, gpointer user_data)
{
  char *s = g_strdup_value_contents(value);
  mpris_debug("For key '%s' value is '%s' as a %s\n", (char *)key, s, G_VALUE_TYPE_NAME(value));
  g_free(s);
}

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
        g_hash_table_foreach(table, mpris_debug_dump_helper ,0);
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

static void mpris_connect_dbus_signals(pidginmpris_t *player)
{
	player->proxy = dbus_g_proxy_new_for_name(bus,
			player->service_name, DBUS_MPRIS_PLAYER_PATH, DBUS_MPRIS_PLAYER);

	dbus_g_proxy_add_signal(player->proxy, DBUS_MPRIS_TRACK_SIGNAL,
			DBUS_TYPE_G_STRING_VALUE_HASHTABLE, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal(player->proxy, DBUS_MPRIS_TRACK_SIGNAL,
                                    G_CALLBACK(mpris_track_signal_cb), &(player->ti), NULL);

        if (player->hints & MPRIS_HINT_STATUSCHANGE)
          {
            dbus_g_proxy_add_signal(player->proxy, DBUS_MPRIS_STATUS_SIGNAL,
                                    G_TYPE_INT, G_TYPE_INVALID);
            dbus_g_proxy_connect_signal(player->proxy, DBUS_MPRIS_STATUS_SIGNAL,
                                        G_CALLBACK(mpris_status_signal_int_cb), &(player->ti), NULL);
          }
        else
          {
            dbus_g_proxy_add_signal(player->proxy, DBUS_MPRIS_STATUS_SIGNAL,
                                    DBUS_TYPE_MPRIS_STATUS, G_TYPE_INVALID);
            dbus_g_proxy_connect_signal(player->proxy, DBUS_MPRIS_STATUS_SIGNAL,
                                        G_CALLBACK(mpris_status_signal_struct_cb), &(player->ti), NULL);
          }
}

static gboolean mpris_app_running(pidginmpris_t *player)
{
	DBusGProxy *proxy = dbus_g_proxy_new_for_name_owner(bus, 
			player->service_name, DBUS_MPRIS_PLAYER_PATH, DBUS_MPRIS_PLAYER, NULL);
	
	if(!proxy)
		return FALSE;
	
	g_object_unref(proxy);
	return TRUE;
}

static void
player_new(const char *service_name)
{
  mpris_debug("Setting up %s\n", service_name);

  pidginmpris_t *player = g_malloc0(sizeof(pidginmpris_t));
  player->service_name = g_strdup(service_name);

  if ((strcmp(service_name, "org.mpris.audacious") == 0) ||
      (strcmp(service_name, "org.mpris.bmp") == 0) ||
      (strncmp(service_name, "org.mpris.dragonplayer", strlen("org.mpris.dragonplayer")) == 0))
    {
      mpris_debug("Setting non-standard status change hint\n");
      player->hints |= MPRIS_HINT_STATUSCHANGE;
    }

  if (strncmp(service_name, "org.mpris.dragonplayer", strlen("org.mpris.dragonplayer")) == 0)
    {
      mpris_debug("Setting non-standard metadata method name hint\n");
      player->hints |= MPRIS_HINT_METADATA_METHOD_CASE;
    }
 
  g_hash_table_insert(players, g_strdup(service_name), player);

  /* connect mpris's dbus signals */
  mpris_connect_dbus_signals(player);

  mpris_status_signal_int_cb(NULL, -1, &(player->ti));

  /* Try to discover how this player likes to be known */
  DBusGProxy *proxy = dbus_g_proxy_new_for_name(bus, player->service_name, "/", DBUS_MPRIS_PLAYER);
  if (proxy)
    {
      GError *error = 0;
      char *identity;
      if (dbus_g_proxy_call(proxy, "Identity", &error,
                            G_TYPE_INVALID, G_TYPE_STRING, &identity, G_TYPE_INVALID))
        {
          mpris_debug("Player Identity '%s'\n", identity);
          /* Use the first word of the Identity string (as it contains a version as well */
          gchar **v = g_strsplit(identity, " ", 2);
          if (v)
            {
              player->player_name = g_strdup(*v);
              g_strfreev(v);
            }
          else
            {
              player->player_name = g_strdup("");
            }
        }
      else
        {
          mpris_error("Identity method failed: %s\n", error->message);
          g_error_free(error); 
        }
      g_object_unref(proxy);
    }

  /* Fallback to using the service name with leading 'org.mpris.' removed */
  if (!player->player_name)
    {
      player->player_name = g_strdup(service_name + strlen(DBUS_MPRIS_NAMESPACE));
      player->player_name[0] = g_ascii_toupper(player->player_name[0]);
    }

  mpris_debug("created player record for service '%s'\n", service_name);
}

static void
player_delete(gpointer data)
{
  pidginmpris_t *player = (pidginmpris_t *)data;

  if (player)
    {
      g_free(player->service_name);
      g_free(player->player_name);
      g_free(player);
    }
}

static gboolean
load_plugin(void)
{
        if (!players)
          players = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, player_delete);

	/* initialize dbus connection */
	GError *error = 0;
	bus = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
	if (!bus) {
		mpris_error("failed to connect to the dbus daemon: %s\n", error->message);
		g_error_free (error);
		return FALSE;
	}

	return TRUE;
}

static void
mpris_check_player(gpointer key, gpointer value, gpointer user_data)
{ 
  pidginmpris_t *player = (pidginmpris_t *)value;
  struct TrackInfo *ti = (struct TrackInfo *)user_data;

  /* If we've already found an active player, don't carry on checking... */
  if (ti->status != STATUS_OFF)
    {
      return;
    }

  GError *error = 0;
  mpris_debug("Trying %s\n", player->service_name);

  if(mpris_app_running(player)) {
    error = 0;
    gboolean result;
    int status;
    if (player->hints & MPRIS_HINT_STATUSCHANGE)
      {
        result = dbus_g_proxy_call_with_timeout(player->proxy, "GetStatus", DBUS_TIMEOUT*10, &error,
                                                G_TYPE_INVALID,
                                                G_TYPE_INT, &status,
                                                G_TYPE_INVALID);

        if (result)
          {
            mpris_status_signal_int_cb(NULL, status, &(player->ti));
          }
      }
    else
      {
        GValueArray *s = 0;
        result = dbus_g_proxy_call_with_timeout(player->proxy, "GetStatus", DBUS_TIMEOUT*10, &error,
                                                G_TYPE_INVALID,
                                                DBUS_TYPE_MPRIS_STATUS, &s,
                                                G_TYPE_INVALID);

        if (result)
          {
            mpris_status_signal_struct_cb(NULL, s, &(player->ti));
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
    if(dbus_g_proxy_call_with_timeout(player->proxy,
                                      (player->hints & MPRIS_HINT_METADATA_METHOD_CASE) ? "GetMetaData" : "GetMetadata", 
                                      DBUS_TIMEOUT*10, &error,
                                      G_TYPE_INVALID, DBUS_TYPE_G_STRING_VALUE_HASHTABLE, &table, 
                                      G_TYPE_INVALID)) {
      mpris_track_signal_cb(NULL, table, &(player->ti));
      g_hash_table_destroy(table);
    }
    else
      {
        if (error)
          {
            mpris_debug("GetMetadata failed %s\n", error->message);
            g_error_free(error);
          }
        else
          {
            mpris_debug("GetMetadata failed with no error\n");
          }
            
      }

    error = 0;
    int position;
    if(dbus_g_proxy_call_with_timeout(player->proxy, "PositionGet", DBUS_TIMEOUT*10, &error,
                                      G_TYPE_INVALID, G_TYPE_INT, &position, 
                                      G_TYPE_INVALID)) {
      player->ti.currentSecs = position/1000;
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

    player->ti.player = player->player_name;

    if (player->ti.status != STATUS_OFF)
      {
        *ti = player->ti;
      }
  }
}   

gboolean get_mpris_info(struct TrackInfo* ti)
{
  if (!bus)
    if (!load_plugin())
      return FALSE;

  /* Look for "org.mpris.*" service names on the bus */
  GError *error = 0;
  DBusGProxy *proxy = dbus_g_proxy_new_for_name(bus, "org.freedesktop.DBus", "/", "org.freedesktop.DBus");
  if (proxy)
    {
      char **v;
      if (dbus_g_proxy_call(proxy, "ListNames", &error, G_TYPE_INVALID, G_TYPE_STRV, &v, G_TYPE_INVALID))
        {
          for (char **i = v; (*i) != 0; i++)
            { 
              /* Is this an org.mpris.* sevice ? */
              if (strncmp(DBUS_MPRIS_NAMESPACE, *i, strlen(DBUS_MPRIS_NAMESPACE)) == 0)
                {
                  /* Is it already in our list ? */
                  if (!g_hash_table_lookup(players, *i))
                    {
                      /* Allocate, populate and add a new player info structure */
                      player_new(*i);
                    }
                }
            }
          g_strfreev(v);
        }
      else
        {
          mpris_debug("ListNames failed %s\n", error->message);
          g_error_free(error);
        }
    }
  else
    {
      mpris_debug("Failed to connect to Dbus%s\n", error->message);
      g_error_free(error);
    }

  ti->status = STATUS_OFF;
  
  g_hash_table_foreach(players, mpris_check_player, ti);
 
  return (ti->status != STATUS_OFF);

}
