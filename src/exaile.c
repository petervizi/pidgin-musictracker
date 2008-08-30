#include <dbus/dbus-glib.h>
#include "musictracker.h"
#include "utils.h"
#include <string.h>

gboolean exaile_dbus_query(DBusGProxy *proxy, const char *method, char* dest)
{
	char *str = 0;
	GError *error = 0;
	if (!dbus_g_proxy_call_with_timeout(proxy, method, DBUS_TIMEOUT, &error,
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

        trace("exaile_dbus_query: '%s' => '%s'", method, dest);

	return TRUE;
}

gboolean
get_exaile_info(struct TrackInfo* ti)
{
	DBusGConnection *connection;
	DBusGProxy *proxy;
	GError *error = 0;
	char buf[STRLEN], status[STRLEN];

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
	if (!exaile_dbus_query(proxy, "query", buf)) {
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
		exaile_dbus_query(proxy, "get_artist", ti->artist);
		exaile_dbus_query(proxy, "get_album", ti->album);
		exaile_dbus_query(proxy, "get_title", ti->track);

		exaile_dbus_query(proxy, "get_length", buf);
		if (sscanf(buf, "%d:%d", &mins, &secs)) {
			ti->totalSecs = mins*60 + secs;	
		}

		error = 0;
		unsigned char percentage;
		if (!dbus_g_proxy_call_with_timeout(proxy, "current_position", DBUS_TIMEOUT, &error,
					G_TYPE_INVALID,
					G_TYPE_UCHAR, &percentage,
					G_TYPE_INVALID))
		{
			trace("Failed to make dbus call: %s", error->message);
		}
                trace("exaile_dbus_query: 'current_position' => %d", percentage);
		ti->currentSecs = percentage*ti->totalSecs/100;
	}
	return TRUE;
}
