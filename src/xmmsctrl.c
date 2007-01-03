#include "musictracker.h"
#include "utils.h"
#include "dlfcn.h"

gchar *(*xmms_remote_get_playlist_title)(gint session, gint pos);
gint (*xmms_remote_get_playlist_time)(gint session, gint pos);
gboolean (*xmms_remote_is_playing)(gint session);
gboolean (*xmms_remote_is_paused)(gint session);
gint (*xmms_remote_get_playlist_pos)(gint session);
gint (*xmms_remote_get_output_time)(gint session);

#define get_func(name) name = dlsym(handle, #name); trace("%d", name)
char xmmsctrl_lib[STRLEN];

void 
xmmsctrl_init(const char *lib)
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

gboolean
get_xmmsctrl_info(struct TrackInfo *ti, char *lib, int session)
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
			const char *sep = gaim_prefs_get_string("/plugins/core/musictracker/string_xmms_sep");
			assert(strlen(sep) == 1);
			char fmt[100];
			sprintf(fmt, "%%[^%s]%s%%[^%s]%s%%[^%s]", sep, sep, sep, sep, sep);
			if (!sscanf(title, fmt, ti->artist, ti->album, ti->track) == 3) {
				trace("XMMS Status in unparsable format");
				return FALSE;
			}
		}

		ti->totalSecs = (*xmms_remote_get_playlist_time)(session, pos)/1000;
		ti->currentSecs = (*xmms_remote_get_output_time)(session)/1000;
	}
	return TRUE;
}
