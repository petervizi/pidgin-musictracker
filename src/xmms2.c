
#include <config.h>
#ifdef HAVE_XMMSCLIENT

#include <string.h>
#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient-glib.h>

#include "musictracker.h"
#include "utils.h"

gboolean get_xmms2_status(xmmsc_connection_t *conn, struct TrackInfo *ti);
gboolean get_xmms2_mediainfo(xmmsc_connection_t *conn, struct TrackInfo *ti);
gboolean get_xmms2_playtime(xmmsc_connection_t *conn, struct TrackInfo *ti);

gboolean get_xmms2_info(struct TrackInfo *ti)
{
	xmmsc_connection_t *connection = NULL;
	const gchar *path = NULL;
	gint ret;

	connection = xmmsc_init("musictracker");

	if (!connection) {
		purple_debug_error(PLUGIN_ID, "(XMMS2)"
		                   " Connection initialization failed.\n");
		return FALSE;
	}

	path = getenv("XMMS_PATH");
	if (!path) {
		path = purple_prefs_get_string(PREF_XMMS2_PATH);

		if (strcmp(path, "") == 0) {
			path = NULL;
		}
	}

	ret = xmmsc_connect(connection, path);
	if (!ret) {
		purple_debug_error(PLUGIN_ID,
		                   "(XMMS2) Connection to %s failed, %s.\n",
		                   path, xmmsc_get_last_error(connection));
		xmmsc_unref(connection);
		return FALSE;
	}

	if (!get_xmms2_status(connection, ti) ||
	    !get_xmms2_mediainfo(connection, ti) ||
	    !get_xmms2_playtime(connection, ti)) {
		xmmsc_unref(connection);
		return FALSE;
	}

	xmmsc_unref(connection);

	return TRUE;
}

/**
 * Gets the playback status.
 */
gboolean get_xmms2_status(xmmsc_connection_t *conn, struct TrackInfo *ti)
{
	guint status;
	xmmsc_result_t *result = NULL;

	if (!conn || !ti) {
		return FALSE;
	}

	result = xmmsc_playback_status(conn);
	xmmsc_result_wait(result);

	if (xmmsc_result_iserror(result) ||
	    !xmmsc_result_get_uint(result, &status)) {
		purple_debug_error(PLUGIN_ID,
		                   "(XMMS2) Failed to get playback status,"
		                   " %s.\n", xmmsc_result_get_error(result));
		xmmsc_result_unref(result);
		return FALSE;
	}

	switch (status) {
	case XMMS_PLAYBACK_STATUS_STOP:
		ti->status = STATUS_OFF;
		break;
	case XMMS_PLAYBACK_STATUS_PLAY:
		ti->status = STATUS_NORMAL;
		break;
	case XMMS_PLAYBACK_STATUS_PAUSE:
		ti->status = STATUS_PAUSED;
		break;
	}

	xmmsc_result_unref(result);

	return TRUE;
}

/**
 * Gets the media information of the current track.
 */
gboolean get_xmms2_mediainfo(xmmsc_connection_t *conn, struct TrackInfo *ti)
{
	guint id;
	gint duration;
	const char *val = NULL;
	xmmsc_result_t *result = NULL;

	if (!conn || !ti) {
		return FALSE;
	}

	result = xmmsc_playback_current_id(conn);
	xmmsc_result_wait(result);

	if (xmmsc_result_iserror(result) ||
	    !xmmsc_result_get_uint(result, &id)) {
		purple_debug_error(PLUGIN_ID,
		                   "(XMMS2) Failed to get current ID, %s.\n",
		                   xmmsc_result_get_error(result));
		xmmsc_result_unref(result);
		return FALSE;
	}

	xmmsc_result_unref(result);

	/* Nothing is being played if the ID is 0. */
	if (id == 0) {
		purple_debug_info(PLUGIN_ID, "(XMMS2) Stopped.\n");
		return FALSE;
	}

	result = xmmsc_medialib_get_info(conn, id);
	xmmsc_result_wait(result);

	if (xmmsc_result_iserror(result)) {
		purple_debug_error(PLUGIN_ID,
		                   "(XMMS2) Failed to get media info, %s.\n",
		                   xmmsc_result_get_error(result));
		xmmsc_result_unref(result);
		return FALSE;
	}

	if (xmmsc_result_get_dict_entry_string(result, "title", &val)) {
		strcpy(ti->track, val);
	}
	if (xmmsc_result_get_dict_entry_string(result, "artist", &val)) {
		strcpy(ti->artist, val);
	}
	if (xmmsc_result_get_dict_entry_string(result, "album", &val)) {
		strcpy(ti->album, val);
	}
	if (xmmsc_result_get_dict_entry_int(result, "duration", &duration)) {
		ti->totalSecs = duration / 1000;
	}

	xmmsc_result_unref(result);

	return TRUE;
}

/**
 * Gets the playback time.
 */
gboolean get_xmms2_playtime(xmmsc_connection_t *conn, struct TrackInfo *ti)
{
	guint playtime;
	xmmsc_result_t *result = NULL;

	if (!conn || !ti) {
		return FALSE;
	}

	result = xmmsc_playback_playtime(conn);
	xmmsc_result_wait(result);

	if (xmmsc_result_iserror(result) ||
	    !xmmsc_result_get_uint(result, &playtime)) {
		purple_debug_error(PLUGIN_ID,
		                   "(XMMS2) Failed to get playback time, %s.\n",
		                   xmmsc_result_get_error(result));
		xmmsc_result_unref(result);
		return FALSE;
	}

	ti->currentSecs = playtime / 1000;

	xmmsc_result_unref(result);

	return TRUE;
}

void cb_xmms2_pref_changed(GtkWidget *entry, gpointer data)
{
	const gchar *path = gtk_entry_get_text(GTK_ENTRY(entry));
	const char *pref = (const char*) data;

	purple_prefs_set_string(pref, path);
}

void get_xmms2_pref(GtkBox *vbox)
{
	GtkWidget *entry, *hbox;

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Path:"),
	                   FALSE, FALSE, 0);

	entry = gtk_entry_new();

	gtk_entry_set_text(GTK_ENTRY(entry),
	                   purple_prefs_get_string(PREF_XMMS2_PATH));
	gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(entry), "changed",
	                 G_CALLBACK(cb_xmms2_pref_changed),
	                 (gpointer) PREF_XMMS2_PATH);
}

#endif
