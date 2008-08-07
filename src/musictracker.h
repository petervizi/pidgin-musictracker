#ifndef _MUSICTRACKER_H_
#define _MUSICTRACKER_H_

#include <assert.h>
#include <glib.h>
#include <gtk/gtk.h>
#include "purple.h"

#define PLUGIN_ID "core-musictracker"
#define STRLEN 100

#define STATUS_OFF 0
#define STATUS_PAUSED 1
#define STATUS_NORMAL 2

#define INTERVAL 10000

// Global preferences
static const char *PREF_DISABLED = "/plugins/core/musictracker/bool_disabled";
static const char *PREF_LOG = "/plugins/core/musictracker/bool_log";
static const char *PREF_FORMAT = "/plugins/core/musictracker/string_format";
static const char *PREF_PAUSED = "/plugins/core/musictracker/string_paused";
static const char *PREF_OFF = "/plugins/core/musictracker/string_off";
static const char *PREF_PLAYER = "/plugins/core/musictracker/int_player";
static const char *PREF_FILTER = "/plugins/core/musictracker/string_filter";
static const char *PREF_MASK = "/plugins/core/musictracker/string_mask";
static const char *PREF_CUSTOM_FORMAT = "/plugins/core/musictracker/string_custom_%s_%s";
static const char *PREF_CUSTOM_DISABLED = "/plugins/core/musictracker/bool_custom_%s_%s";
static const char *PREF_FILTER_ENABLE = "/plugins/core/musictracker/bool_filter";
static const char *PREF_DISABLE_WHEN_AWAY = "/plugins/core/musictracker/bool_disable_when_away";
static const char *PREF_NOW_LISTENING_ONLY = "/plugins/core/musictracker/bool_now_listening_only";

// Player specific preferences (should these go somewhere else?)
static const char *PREF_XMMS_SEP = "/plugins/core/musictracker/string_xmms_sep";
static const char *PREF_MPD_HOSTNAME = "/plugins/core/musictracker/string_mpd_hostname";
static const char *PREF_MPD_PORT = "/plugins/core/musictracker/string_mpd_port";
static const char *PREF_MPD_PASSWORD = "/plugins/core/musictracker/string_mpd_password";
static const char *PREF_LASTFM = "/plugins/core/musictracker/lastfm_user";
static const char *PREF_LASTFM_INTERVAL = "/plugins/core/musictracker/lastfm_interval";
static const char *PREF_LASTFM_QUIET = "/plugins/core/musictracker/lastfm_quiet";
static const char *PREF_XMMS2_PATH = "/plugins/core/musictracker/string_xmms2_path";

struct TrackInfo
{
	char track[STRLEN];
	char artist[STRLEN];
	char album[STRLEN];
	const char* player;
	int status;
	int totalSecs;
	int currentSecs;
};

struct PlayerInfo
{
	const char *name;
	gboolean (*track_func)(struct TrackInfo *ti);
	void (*pref_func)(GtkBox *container);
};

extern struct PlayerInfo g_players[];
GtkWidget* pref_frame(PurplePlugin *plugin);

gboolean set_status (PurpleAccount *account, char *text, struct TrackInfo *ti);
void set_userstatus_for_active_accounts (char *userstatus, struct TrackInfo *ti);

#endif // _MUSICTRACKER_H_
