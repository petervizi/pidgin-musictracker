#include "musictracker.h"
#include "utils.h"
#include "dlfcn.h"
#include <string.h>

gchar *(*xmms_remote_get_playlist_title)(gint session, gint pos);
gint (*xmms_remote_get_playlist_time)(gint session, gint pos);
gboolean (*xmms_remote_is_playing)(gint session);
gboolean (*xmms_remote_is_paused)(gint session);
gint (*xmms_remote_get_playlist_pos)(gint session);
gint (*xmms_remote_get_output_time)(gint session);

#define get_func(name) name = dlsym(handle, #name)
char xmmsctrl_lib[STRLEN];

void xmmsctrl_init(const char *lib)
{
	trace("%s %s", lib, xmmsctrl_lib);
	if (strcmp(lib, xmmsctrl_lib) == 0)
		return;

	void *handle = dlopen(lib, RTLD_NOW);
	if (handle) {
		get_func(xmms_remote_get_playlist_title);
		get_func(xmms_remote_get_playlist_time);
		get_func(xmms_remote_is_playing);
		get_func(xmms_remote_is_paused);
		get_func(xmms_remote_get_playlist_pos);
		get_func(xmms_remote_get_output_time);
		strncpy(xmmsctrl_lib, lib, STRLEN);
	} else {
		trace("Failed to load library %s", lib);
	}
}

gboolean get_xmmsctrl_info(struct TrackInfo *ti, char *lib, int session)
{
	xmmsctrl_init(lib);
	if (!(xmms_remote_get_playlist_title && xmms_remote_get_playlist_time &&
			xmms_remote_is_playing && xmms_remote_is_paused &&
			xmms_remote_get_playlist_pos && xmms_remote_get_output_time)) {
		trace("xmmsctrl not initialized properly");
		return FALSE;
	}

	int pos = (*xmms_remote_get_playlist_pos)(session);
	trace("Got position %d", pos);

	if ((*xmms_remote_is_playing)(session)) {
		if ((*xmms_remote_is_paused)(session))
			ti->status = STATUS_PAUSED;
		else
			ti->status = STATUS_NORMAL;
	} else
		ti->status = STATUS_OFF;
	trace("Got state %d", ti->status);

	if (ti->status != STATUS_OFF) {
		char *title = (*xmms_remote_get_playlist_title)(session, pos);

		if (title) {
			trace("Got title %s", title);
			const char *sep = purple_prefs_get_string(PREF_XMMS_SEP);
			if (strlen(sep) != 1) {
				trace("Delimiter size should be 1. Cant parse status.");
				return FALSE;
			}

                        char regexp[100];
                        sprintf(regexp, "^(.*)\\%s(.*)\\%s(.*)$", sep, sep);
                        pcre *re = regex(regexp, 0);
                        capture(re, title, strlen(title), ti->artist, ti->album, ti->track);
                        pcre_free(re);
		}

		ti->totalSecs = (*xmms_remote_get_playlist_time)(session, pos)/1000;
		ti->currentSecs = (*xmms_remote_get_output_time)(session)/1000;
	}
	return TRUE;
}

gboolean get_xmms_info(struct TrackInfo *ti)
{
	gboolean b = get_xmmsctrl_info(ti, "libxmms.so", 0);
	if (!b)
		b = get_xmmsctrl_info(ti, "libxmms.so.1", 0);
	return b;
}

gboolean get_audacious_legacy_info(struct TrackInfo *ti)
{
	gboolean b = get_xmmsctrl_info(ti, "libaudacious.so", 0);
	if (!b)
		b = get_xmmsctrl_info(ti, "libaudacious.so.3", 0);
        if (b)
          ti->player = "Audacious";
	return b;
}

void cb_xmms_sep_changed(GtkEditable *editable, gpointer data)
{
	const char *text = gtk_entry_get_text(GTK_ENTRY(editable));
	if (strlen(text) == 1)
		purple_prefs_set_string(PREF_XMMS_SEP, text);
}

void get_xmmsctrl_pref(GtkBox *box)
{
	GtkWidget *entry, *hbox, *label;

	entry = gtk_entry_new_with_max_length(1);
	gtk_entry_set_text(GTK_ENTRY(entry), purple_prefs_get_string(PREF_XMMS_SEP));
	g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(cb_xmms_sep_changed), 0);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Title Delimiter Character: "), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
	gtk_box_pack_start(box, hbox, FALSE, FALSE, 0);

	gtk_box_pack_start(box, gtk_hseparator_new(), FALSE, FALSE, 0);

	label = gtk_label_new("Note: You must change the playlist title in XMMS/Audacious 1.3 to be formatted as '%p | %a | %t' (ARTIST | ALBUM | TITLE) in the player preferences, where '|' is the Title Delimiter Character set above, which is the only way for MusicTracker to parse all three fields from either of these players. If you change this character above, then '|' in the string '%p | %a | %t' must be replaced with the selected character.");
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_box_pack_start(box, label, TRUE, TRUE, 0);
}

