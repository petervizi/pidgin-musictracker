#include <dbus/dbus-glib.h>
#include "musictracker.h"
#include "utils.h"
#include <string.h>

void banshee_hash_str(GHashTable *table, const char *key, char *dest)
{
	GValue* value = (GValue*) g_hash_table_lookup(table, key);
	if (value != NULL && G_VALUE_HOLDS_STRING(value)) {
		strncpy(dest, g_value_get_string(value), STRLEN-1);
	}
}

gboolean banshee_dbus_string(DBusGProxy *proxy, const char *method, char* dest)
{
	char *str = 0;
	GError *error = 0;
	if (!dbus_g_proxy_call_with_timeout (proxy, method, DBUS_TIMEOUT, &error,
				G_TYPE_INVALID,
				G_TYPE_STRING, &str,
				G_TYPE_INVALID))
	{
		trace("Failed to make dbus call %s: %s", method, error->message);
		return FALSE;
	}

	assert(str);
	strncpy(dest, str, STRLEN);
	dest[STRLEN-1] = 0;
	g_free(str);
	return TRUE;
}

int banshee_dbus_int(DBusGProxy *proxy, const char *method)
{
	int ret;
	GError *error = 0;
	if (!dbus_g_proxy_call_with_timeout (proxy, method, DBUS_TIMEOUT, &error,
				G_TYPE_INVALID,
				G_TYPE_INT, &ret,
				G_TYPE_INVALID))
	{
		trace("Failed to make dbus call %s: %s", method, error->message);
		return 0;
	}

	return ret;
}

gboolean
get_banshee_info(struct TrackInfo* ti)
{
	DBusGConnection *connection;
	DBusGProxy *proxy;
	GError *error = 0;
	int status;
	char szStatus[STRLEN];

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (connection == NULL) {
		trace("Failed to open connection to dbus: %s\n", error->message);
		g_error_free (error);
		return FALSE;
	}

	if (dbus_g_running(connection,"org.gnome.Banshee")) {
		proxy = dbus_g_proxy_new_for_name (connection,
				"org.gnome.Banshee",
				"/org/gnome/Banshee/Player",
				"org.gnome.Banshee.Core");

		if (!dbus_g_proxy_call_with_timeout (proxy, "GetPlayingStatus", DBUS_TIMEOUT, &error,
					G_TYPE_INVALID,
					G_TYPE_INT, &status,
					G_TYPE_INVALID))
		{
			trace("Failed to make dbus call: %s", error->message);
			return FALSE;
		}

		if (status == -1) {
			ti->status = STATUS_OFF;
			return TRUE;
		} else if (status == 1)
			ti->status = STATUS_NORMAL;
		else
			ti->status = STATUS_PAUSED;

		banshee_dbus_string(proxy, "GetPlayingArtist", ti->artist);
		banshee_dbus_string(proxy, "GetPlayingAlbum", ti->album);
		banshee_dbus_string(proxy, "GetPlayingTitle", ti->track);

		ti->totalSecs = banshee_dbus_int(proxy, "GetPlayingDuration");
		ti->currentSecs = banshee_dbus_int(proxy, "GetPlayingPosition");
		return TRUE;
	} else if (dbus_g_running(connection, "org.bansheeproject.Banshee")) { // provide for new interface
		proxy = dbus_g_proxy_new_for_name (connection,
				"org.bansheeproject.Banshee",
				"/org/bansheeproject/Banshee/PlayerEngine",
				"org.bansheeproject.Banshee.PlayerEngine");
		
		banshee_dbus_string(proxy, "GetCurrentState", szStatus);
		if (strcmp(szStatus, "idle") == 0) {
			ti->status = STATUS_OFF;
			return TRUE;
		} else if (strcmp(szStatus, "playing") == 0)
			ti->status = STATUS_NORMAL;
		else
			ti->status = STATUS_PAUSED;
		
		GHashTable* table;
		if (!dbus_g_proxy_call_with_timeout (proxy, "GetCurrentTrack", DBUS_TIMEOUT, &error,
					G_TYPE_INVALID,
					dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE), &table,
					G_TYPE_INVALID))
		{
			trace("Failed to make dbus call: %s", error->message);
			return FALSE;
		}
		
		banshee_hash_str(table, "album", ti->album);
		banshee_hash_str(table, "artist", ti->artist);
		banshee_hash_str(table, "name", ti->track);
		
		g_hash_table_destroy(table);
		
		ti->totalSecs = banshee_dbus_int(proxy, "GetLength") / 1000;
		ti->currentSecs = banshee_dbus_int(proxy, "GetPosition") / 1000;
		return TRUE;
	}

        ti->status = STATUS_OFF;
        return FALSE;
}
