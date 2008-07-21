#include <dbus/dbus-glib.h>
#include "musictracker.h"
#include "utils.h"
#include <string.h>

gboolean
get_listen_info(struct TrackInfo* ti)
{
    DBusGConnection *connection;
    DBusGProxy *proxy;
    GError *error = 0;
    char *buf = 0;
    static pcre *re;

    connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
    if (connection == NULL) {
        trace("Failed to open connection to dbus: %s\n", error->message);
        g_error_free (error);
        return FALSE;
    }

    if (!dbus_g_running(connection, "org.gnome.Listen")) {
        trace("org.gnome.Listen not running");
        ti->status = STATUS_OFF;
        return TRUE;
    }

    proxy = dbus_g_proxy_new_for_name(connection,
            "org.gnome.Listen",
            "/org/gnome/listen",
            "org.gnome.Listen");

    if (!dbus_g_proxy_call(proxy, "current_playing", &error,
                           G_TYPE_INVALID,
                           G_TYPE_STRING, &buf,
                           G_TYPE_INVALID))
    {
        trace("Failed to make dbus call: %s", error->message);
        return FALSE;
    }

    if (strcmp(buf, "") == 0) {
        ti->status = STATUS_PAUSED;
        return TRUE;
    }

    ti->status = STATUS_NORMAL;
    if (!re)
        re = regex("^(.*) - \\((.*) - (.*)\\)$", 0);
    capture(re, buf, strlen(buf), ti->track, ti->album, ti->artist);

    return TRUE;
}
