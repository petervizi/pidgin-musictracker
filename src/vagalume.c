/*
 * musictracker
 * vagalume.c
 * retrieve track info from Vagalume 
 *
 * Copyright (C) 2008, Juan A. Suarez Romero <jasuarez@igalia.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02111-1301, USA.
 *
 */

#include <dbus/dbus.h>
#include "musictracker.h"
#include "utils.h"
#include <string.h>

#define PLAYER_NAME "Vagalume"

#define PLAY_MSG   "playing"
#define STOP_MSG   "stopped"
#define CLOSE_MSG  "closing"
#define START_MSG  "starting"
#define STATUS_MSG "request_status"

#define DBUS_VGL_NAME  "com.igalia.vagalume"
#define DBUS_VGL_PATH  "/com/igalia/vagalume"
#define DBUS_VGL_IFACE "com.igalia.vagalume"

#define CHECK_INTERVAL 60

static struct TrackInfo cached_track;
static gboolean initialized = FALSE;
static gboolean running = FALSE;

static gchar *
str_replace(const gchar *str, const gchar *old, const gchar *new)
{
  GString *g_str = g_string_new(str);
  const char *cur = g_str->str;
  gint oldlen = strlen(old);
  gint newlen = strlen(new);

  while ((cur = strstr(cur, old)) != NULL) {
    int position = cur - g_str->str;
    g_string_erase(g_str, position, oldlen);
    g_string_insert(g_str, position, new);
    cur = g_str->str + position + newlen;
  }

  return g_string_free(g_str, FALSE);
}

static gchar *
unescape_string(const gchar *escaped_string)
{
  static gchar *esc[5] = { "&amp;",
                           "&lt;",
                           "&gt;",
                           "&apos;",
                           "&quot;" };

  static gchar *unesc[5] = { "&",
                             "<",
                             ">",
                             "'",
                             "\"" };

  gchar *unescaped_string = NULL;
  gchar *saved_string = NULL;
  gint i;


  unescaped_string = g_strdup(escaped_string);
  /* Replace */
  for (i = 0; i < 5; i++) {
    saved_string = unescaped_string;
    unescaped_string = str_replace(saved_string,
                                    esc[i],
                                    unesc[i]);
    g_free(saved_string);
  }

  return unescaped_string;
}

static void
clean_cached (void)
{
  cached_track.track[0] = '\0';
  cached_track.artist[0] = '\0';
  cached_track.album[0] = '\0';
  cached_track.status = STATUS_OFF;
  cached_track.totalSecs = 0;
  cached_track.currentSecs = 0;

  if (!cached_track.player) {
    cached_track.player = g_strdup (PLAYER_NAME);
  }
}

static DBusHandlerResult
dbus_handler(DBusConnection *connection,
              DBusMessage *message,
              void *user_data)
{
  DBusMessageIter iter;
  gchar *status = NULL;
  gchar *title = NULL;
  gchar *unescaped_title = NULL;
  gchar *artist = NULL;
  gchar *unescaped_artist = NULL;
  gchar *album = NULL;
  gchar *unescaped_album = NULL;

  if (!dbus_message_iter_init(message, &iter)) {
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  /* Interested either in playing or stopped */
  if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING) {
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  dbus_message_iter_get_basic (&iter, &status);

  /* With playing, artist, title and album are received too */
  if (strcmp (status, PLAY_MSG) == 0) {
    if (dbus_message_iter_next(&iter) &&
        dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING) {
      dbus_message_iter_get_basic(&iter, &artist);
    } else {
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (dbus_message_iter_next(&iter) &&
        dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING) {
      dbus_message_iter_get_basic(&iter, &title);
    } else {
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (dbus_message_iter_next(&iter) &&
        dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING) {
      dbus_message_iter_get_basic(&iter, &album);
    } else {
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    /* Unescape strings */
    unescaped_title = unescape_string(title);
    unescaped_artist = unescape_string(artist);
    unescaped_album = unescape_string(album);

    /* Update cached track info */
    clean_cached();
    strncpy(cached_track.track, unescaped_title, STRLEN-1);
    strncpy(cached_track.artist, unescaped_artist, STRLEN-1);
    strncpy(cached_track.album, unescaped_album, STRLEN-1);
    cached_track.status = STATUS_NORMAL;

    /* Make sure they're NULL terminated in case of
     * overflow */
    cached_track.track[STRLEN-1] = '\0';
    cached_track.artist[STRLEN-1] = '\0';
    cached_track.album[STRLEN-1] = '\0';
  } else if (strcmp(status, STOP_MSG) == 0) {
    /* A stopped notification */
    clean_cached ();
  } else if (strcmp(status, CLOSE_MSG) == 0) {
    /* Value is closing */
    clean_cached ();
    running = FALSE;
  } else if (strcmp(status, START_MSG) == 0) {
    /* Vagalume has been started */
    clean_cached();
    running = TRUE;
  } else {
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  return DBUS_HANDLER_RESULT_HANDLED;
}

static gboolean
check_and_fill_cache(DBusConnection *connection)
{
  DBusMessage *msg;

  if (!dbus_bus_name_has_owner(connection, DBUS_VGL_NAME, NULL)) {
    running = FALSE;
    clean_cached ();
    return TRUE;
  } else {
    running = TRUE;
  }

  /* Request to send the status */
  msg = dbus_message_new_method_call(DBUS_VGL_NAME,
                                      DBUS_VGL_PATH,
                                      DBUS_VGL_IFACE,
                                      STATUS_MSG);
  dbus_message_set_no_reply (msg, TRUE);
  dbus_connection_send (connection, msg, NULL);
  dbus_connection_flush(connection);
  dbus_message_unref(msg);

  /* Request the current status of Vagalume, and fill the cached
   * track */
  dbus_connection_read_write_dispatch(connection, DBUS_TIMEOUT);
  msg = dbus_connection_pop_message(connection);

  if (msg) {
    dbus_handler(connection, msg, NULL);
  } else {
    /* If there is no reply, probably Vagalume is not
     * running */
    running = FALSE;
    clean_cached();
  }

  /* This function is ran also in a periodic way. Return TRUE to
   * ensure it never stops */
  return TRUE;
}

static void
initialize_plugin (void)
{
  DBusConnection *connection;

  /* Add dbus filters to listen changes in Vagalume */
  connection = dbus_bus_get(DBUS_BUS_SESSION, NULL);
  dbus_bus_add_match (connection,
                      "type='signal', interface='"
                      DBUS_VGL_IFACE
                      "', member='notify'",
                      NULL);
  dbus_connection_add_filter(connection,
                              dbus_handler,
                              NULL,
                              NULL);

  check_and_fill_cache(connection);

  /* Check every once in a while if Vagalume is running, and
   * update the cache approppriately. Thus, if Vagalume is
   * killed we can update the cache. */

  g_timeout_add_seconds(CHECK_INTERVAL, (GSourceFunc) check_and_fill_cache, connection);

  initialized = TRUE;
}

gboolean
get_vagalume_info(struct TrackInfo* ti)
{
  if (!initialized) {
    initialize_plugin ();
  }

  if (!running) {
    return FALSE;
  }

  strncpy(ti->track, cached_track.track, STRLEN-1);
  strncpy(ti->artist, cached_track.artist, STRLEN-1);
  strncpy(ti->album, cached_track.album, STRLEN-1);
  ti->player = g_strdup(cached_track.player);
  ti->status = cached_track.status;
  ti->totalSecs = cached_track.totalSecs;
  ti->currentSecs = cached_track.currentSecs;

  return TRUE;
}
