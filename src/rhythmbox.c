#include <dbus/dbus-glib.h>
#include "musictracker.h"
#include "utils.h"
#include <string.h>

void get_hash_str(GHashTable *table, const char *key, char *dest)
{
	GValue* value = (GValue*) g_hash_table_lookup(table, key);
	if (value != NULL && G_VALUE_HOLDS_STRING(value)) {
		strncpy(dest, g_value_get_string(value), STRLEN-1);
	}
}

unsigned int get_hash_uint(GHashTable *table, const char *key)
{
	GValue* value = (GValue*) g_hash_table_lookup(table, key);
	if (value != NULL && G_VALUE_HOLDS_UINT(value)) {
		return g_value_get_uint(value);
	}
	return 0;
}

gboolean
get_rhythmbox_info(struct TrackInfo* ti)
{
	DBusGConnection *connection;
	DBusGProxy *player, *shell;
	GError *error = 0;
	char buf[100], status[100];

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (connection == NULL) {
		trace("Failed to open connection to dbus: %s\n", error->message);
		g_error_free (error);
		return FALSE;
	}

	if (!dbus_g_running(connection, "org.gnome.Rhythmbox")) {
		ti->status = STATUS_OFF;
		return TRUE;
	}

	shell = dbus_g_proxy_new_for_name(connection,
			"org.gnome.Rhythmbox",
			"/org/gnome/Rhythmbox/Shell",
			"org.gnome.Rhythmbox.Shell");
	player = dbus_g_proxy_new_for_name(connection,
			"org.gnome.Rhythmbox",
			"/org/gnome/Rhythmbox/Player",
			"org.gnome.Rhythmbox.Player");

	gboolean playing;
	if (!dbus_g_proxy_call(player, "getPlaying", &error,
				G_TYPE_INVALID,
				G_TYPE_BOOLEAN, &playing,
				G_TYPE_INVALID)) {
		trace("Failed to get playing state from rhythmbox dbus (%s). Assuming player is off", error->message);
		ti->status = STATUS_OFF;
		return TRUE;
	}
	
	char *uri;
	if (!dbus_g_proxy_call(player, "getPlayingUri", &error,
				G_TYPE_INVALID,
				G_TYPE_STRING, &uri,
				G_TYPE_INVALID)) {
		trace("Failed to get song uri from rhythmbox dbus (%s)", error->message);
		return FALSE;
	}

	GHashTable *table;
	if (!dbus_g_proxy_call(shell, "getSongProperties", &error,
				G_TYPE_STRING, uri,
				G_TYPE_INVALID, 
				dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE),	&table,
				G_TYPE_INVALID)) {
		if (!playing) {
			ti->status = STATUS_OFF;
			return TRUE;
		} else {
			trace("Failed to get song info from rhythmbox dbus (%s)", error->message);
			return FALSE;
		}
	}

	if (playing)
		ti->status = STATUS_NORMAL;
	else
		ti->status = STATUS_PAUSED;

	get_hash_str(table, "artist", ti->artist);
	get_hash_str(table, "album", ti->album);
	get_hash_str(table, "title", ti->track);
	ti->totalSecs = get_hash_uint(table, "duration");
	g_hash_table_destroy(table);

	if (!dbus_g_proxy_call(player, "getElapsed", &error,
				G_TYPE_INVALID,
				G_TYPE_UINT, &ti->currentSecs,
				G_TYPE_INVALID)) {
		trace("Failed to get elapsed time from rhythmbox dbus (%s)", error->message);
	}

	return TRUE;
}
