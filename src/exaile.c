#include <dbus/dbus-glib.h>
#include "musictracker.h"
#include "utils.h"
#include <string.h>

gboolean exaile_dbus_query(DBusGProxy *proxy, const char *method, char* dest, int len)
{
	char *str = 0;
	GError *error = 0;
	if (!dbus_g_proxy_call (proxy, method, &error,
				G_TYPE_INVALID,
				G_TYPE_STRING, &str,
				G_TYPE_INVALID))
	{
		trace("Failed to make dbus call: %s", error->message);
		return FALSE;
	}

	assert(str);
	strncpy(dest, str, len-1);
	g_free(str);
	return TRUE;
}

gboolean
get_exaile_info(struct TrackInfo* ti)
{
	DBusGConnection *connection;
	DBusGProxy *proxy;
	GError *error = 0;
	char buf[100], status[100];

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (connection == NULL) {
		trace("Failed to open connection to dbus: %s\n", error->message);
		g_error_free (error);
		return FALSE;
	}

	if (!dbus_g_running(connection, "org.exaile.DBusInterface")) {
		ti->status = STATUS_OFF;
		return TRUE;
	}

	proxy = dbus_g_proxy_new_for_name (connection,
			"org.exaile.DBusInterface",
			"/DBusInterfaceObject",
			"org.exaile.DBusInterface");

	// We should be using "status" instead of "query" here, but its broken in
	// the current (0.2.6) Exaile version
	if (!exaile_dbus_query(proxy, "query", buf, STRLEN)) {
		trace("Failed to call Exaile dbus method. Assuming player is OFF");
		ti->status = STATUS_OFF;
		return TRUE;
	}

	if (sscanf(buf, "status: %s", status) == 1) {
		if (!strcmp(status, "playing"))
			ti->status = STATUS_NORMAL;
		else
			ti->status = STATUS_PAUSED;
	} else {
		ti->status = STATUS_OFF;
	}

	if (ti->status != STATUS_OFF) {
		int mins, secs;
		exaile_dbus_query(proxy, "get_artist", ti->artist, STRLEN);
		exaile_dbus_query(proxy, "get_album", ti->album, STRLEN);
		exaile_dbus_query(proxy, "get_title", ti->track, STRLEN);

		exaile_dbus_query(proxy, "get_length", buf, STRLEN);
		if (sscanf(buf, "%d:%d", &mins, &secs)) {
			if (mins > 30)
				mins -= 30; // -30 fix for 0.2.6
			if (mins < 0)
				mins *= -1;
			ti->totalSecs = mins*60 + secs;	
		}

		error = 0;
		double d;
		if (!dbus_g_proxy_call(proxy, "current_position", &error,
					G_TYPE_INVALID,
					G_TYPE_DOUBLE, &d,
					G_TYPE_INVALID))
		{
			trace("Failed to make dbus call: %s", error->message);
		}
		ti->currentSecs = (int) (ti->totalSecs*d);
	}
	return TRUE;
}
