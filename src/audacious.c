#include <dbus/dbus-glib.h>
#include "musictracker.h"
#include "utils.h"
#include <string.h>

/* More documentation about dbus:
http://dbus.freedesktop.org/doc/dbus-tutorial.html#objects
More documentation about the interface from audacious 1.4:
src/audacious/objects.xml from http://svn.atheme.org/audacious/trunk (can be found through www.google.com/codesearch)
*/

gboolean audacious_dbus_string(DBusGProxy *proxy, const char *method, int pos, const char *arg, char* dest)
{
	GValue val;
	memset(&val, 0, sizeof(GValue));
	GError *error = 0;
	if (!dbus_g_proxy_call_with_timeout(proxy, method, DBUS_TIMEOUT, &error,
				G_TYPE_UINT, pos,
				G_TYPE_STRING, arg,
				G_TYPE_INVALID,
				G_TYPE_VALUE, &val,
				G_TYPE_INVALID))
	{
		trace("Failed to make dbus call %s: %s", method, error->message);
		return FALSE;
	}

	if (G_VALUE_TYPE(&val) == G_TYPE_STRING) {
		strncpy(dest, g_value_get_string(&val), STRLEN);
		dest[STRLEN-1] = 0;
	}
	g_value_unset(&val);
	return TRUE;
}

int audacious_dbus_uint(DBusGProxy *proxy, const char *method)
{
	guint ret;
	GError *error = 0;
	if (!dbus_g_proxy_call_with_timeout(proxy, method, DBUS_TIMEOUT, &error,
				G_TYPE_INVALID,
				G_TYPE_UINT, &ret,
				G_TYPE_INVALID))
	{
		trace("Failed to make dbus call %s: %s", method, error->message);
		return 0;
	}

	return ret;
}

int audacious_dbus_int(DBusGProxy *proxy, const char *method, int pos)
{
	int ret;
	GError *error = 0;
	if (!dbus_g_proxy_call_with_timeout(proxy, method, DBUS_TIMEOUT, &error,
				G_TYPE_UINT,pos,
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
get_audacious_info(struct TrackInfo* ti)
{
	DBusGConnection *connection;
	DBusGProxy *proxy;
	GError *error = 0;
	char *status = 0;
	int pos = 0;

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (connection == NULL) {
		trace("Failed to open connection to dbus: %s\n", error->message);
		g_error_free (error);
		return FALSE;
	}

	if (!dbus_g_running(connection, "org.atheme.audacious")) {
		ti->status = STATUS_OFF;
		return TRUE;
	}

	proxy = dbus_g_proxy_new_for_name (connection,
			"org.atheme.audacious",
			"/org/atheme/audacious",
			"org.atheme.audacious");

	if (!dbus_g_proxy_call_with_timeout(proxy, "Status", DBUS_TIMEOUT, &error,
				G_TYPE_INVALID,
				G_TYPE_STRING, &status,
				G_TYPE_INVALID))
	{
		trace("Failed to make dbus call: %s", error->message);
		return FALSE;
	}

        ti->player = "Audacious";
        
	if (strcmp(status, "stopped") == 0) {
		ti->status = STATUS_OFF;
		return TRUE;
	} else if (strcmp(status, "playing") == 0) {
		ti->status = STATUS_NORMAL;
	} else {
		ti->status = STATUS_PAUSED;
	}
	
	// Find the position in the playlist
	pos = audacious_dbus_uint(proxy, "Position");
	
	ti->currentSecs = audacious_dbus_uint(proxy, "Time")/1000;
	ti->totalSecs = audacious_dbus_int(proxy, "SongLength", pos);
	
	audacious_dbus_string(proxy, "SongTuple", pos, "artist", ti->artist);
	audacious_dbus_string(proxy, "SongTuple", pos, "album", ti->album);
	audacious_dbus_string(proxy, "SongTuple", pos, "title", ti->track);
	return TRUE;
}
