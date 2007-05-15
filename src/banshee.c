#include <dbus/dbus-glib.h>
#include "musictracker.h"
#include "utils.h"
#include <string.h>

gboolean banshee_dbus_string(DBusGProxy *proxy, const char *method, char* dest)
{
	char *str = 0;
	GError *error = 0;
	if (!dbus_g_proxy_call (proxy, method, &error,
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
	if (!dbus_g_proxy_call (proxy, method, &error,
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
	char buf[100];

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (connection == NULL) {
		trace("Failed to open connection to dbus: %s\n", error->message);
		g_error_free (error);
		return FALSE;
	}

	if (!dbus_g_running(connection, "org.gnome.Banshee")) {
		ti->status = STATUS_OFF;
		return TRUE;
	}

	proxy = dbus_g_proxy_new_for_name (connection,
			"org.gnome.Banshee",
			"/org/gnome/Banshee/Player",
			"org.gnome.Banshee.Core");

	if (!dbus_g_proxy_call (proxy, "GetPlayingStatus", &error,
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
}
