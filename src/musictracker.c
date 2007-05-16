#define PURPLE_PLUGINS

#ifndef WIN32
#include "config.h"
#include <dlfcn.h>
#else
#include <windows.h>
#endif

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <assert.h>
#include <math.h>

#include "musictracker.h"
#include "actions.h"
#include "utils.h"
#include "filter.h"

#include "core.h"
#include "prefs.h"
#include "util.h"
#include "buddyicon.h"
#include "account.h"
#include "status.h"
#include "cmds.h"
#include "connection.h"
#include "signals.h"
#include "version.h"
#include "notify.h"
#include "plugin.h"
#include "version.h"
#include "../protocols/msn/msn.h"
#include "gtkplugin.h"

#define INTERVAL 10000
#define USE_STATUS_API 1

guint g_tid;
PurplePlugin *g_plugin;
gboolean s_setavailable=1, s_setaway=1, g_run=1;
void (*pmsn_cmdproc_send)(MsnCmdProc *cmdproc, const char *command,
                                          const char *format, ...);

//--------------------------------------------------------------------

#ifndef WIN32
gboolean get_amarok_info(struct TrackInfo* ti);
gboolean get_xmms_info(struct TrackInfo* ti);
gboolean get_audacious_info(struct TrackInfo* ti);
gboolean get_rhythmbox_info(struct TrackInfo* ti);
gboolean get_exaile_info(struct TrackInfo* ti);
gboolean get_banshee_info(struct TrackInfo* ti);
gboolean get_mpd_info(struct TrackInfo* ti);
gboolean get_quodlibet_info(struct TrackInfo* ti);

void get_xmmsctrl_pref(GtkBox *box);
void get_mpd_pref(GtkBox *box);
#else
gboolean get_foobar2000_info(struct TrackInfo* ti);
gboolean get_winamp_info(struct TrackInfo* ti);
gboolean get_wmp_info(struct TrackInfo* ti);
gboolean get_itunes_info(struct TrackInfo* ti);
#endif

// Global array of players
struct PlayerInfo g_players[] = {
#ifndef WIN32
	{ "XMMS", get_xmms_info, get_xmmsctrl_pref },
	{ "Audacious", get_audacious_info, get_xmmsctrl_pref },
	{ "Amarok", get_amarok_info, 0 },
	{ "Rhythmbox", get_rhythmbox_info, 0 },
	{ "Banshee", get_banshee_info, 0 },
	{ "QuodLibet", get_quodlibet_info, 0 },
	{ "MPD", get_mpd_info, get_mpd_pref },
	{ "Exaile", get_exaile_info, 0 },
#else
	{ "Winamp", get_winamp_info, 0 },
	{ "Windows Media Player", get_wmp_info, 0 },
	{ "iTunes", get_itunes_info, 0 },
	{ "Foobar2000", get_foobar2000_info, 0 },
#endif
	{ "", 0, 0 } // dummy to end the array
};

//--------------------------------------------------------------------

gboolean
purple_status_is_away (PurpleStatus *status)
{
	PurpleStatusPrimitive	primitive	= PURPLE_STATUS_UNSET;

	if (!status)
		return FALSE;

	primitive	= purple_status_type_get_primitive (purple_status_get_type (status));

	return ((primitive == PURPLE_STATUS_AWAY) || (primitive == PURPLE_STATUS_EXTENDED_AWAY));
}

//--------------------------------------------------------------------

gboolean
purple_status_supports_attr (PurpleStatus *status, const char *id)
{
	gboolean		b				= FALSE;
	PurpleStatusType	*status_type	= NULL;
	GList			*attrs			= NULL;
	PurpleStatusAttr	*attr			= NULL;

	if (!status || !id)
		return b;

	status_type	= purple_status_get_type(status);

	if (status_type != NULL)
	{
		attrs	= purple_status_type_get_attrs (status_type);

		while (attrs != NULL)
		{
			attr	= (PurpleStatusAttr *)attrs->data;

			if (attr != NULL)
			{
				if (strncasecmp(id, purple_status_attr_get_id (attr), strlen(id)) == 0)
					b	= TRUE;
			}

			attrs	= attrs->next;
		}
	}

	return b;
}

//--------------------------------------------------------------------

PurplePluginProtocolInfo *
purple_account_get_pluginprotocolinfo (PurpleAccount *account)
{
	PurpleConnection			*connection		= NULL;
	PurplePlugin				*pPlugin		= NULL;
	PurplePluginInfo			*pInfo			= NULL;
	PurplePluginProtocolInfo	*prInfo			= NULL;

	if (!account)
		return NULL;

	connection	= purple_account_get_connection (account);

	if (connection != NULL)
		pPlugin		= connection->prpl;

	if (pPlugin != NULL)
	{
		pInfo	= pPlugin->info;
		if (pInfo != NULL)
			prInfo	= (PurplePluginProtocolInfo *)pInfo->extra_info;
	}

	return prInfo;
}

//--------------------------------------------------------------------

static gboolean
message_changed(const char *one, const char *two)
{
	if (one == NULL && two == NULL)
		return FALSE;

	if (one == NULL || two == NULL)
		return TRUE;

	return (g_utf8_collate(one, two) != 0);
}

//--------------------------------------------------------------------

msn_act_id(PurpleConnection *gc, const char *entry)
{
        MsnCmdProc *cmdproc;
        MsnSession *session;
        PurpleAccount *account;
        const char *alias;

        session = gc->proto_data;
        cmdproc = session->notification->cmdproc;
        account = purple_connection_get_account(gc);

        if(entry && strlen(entry))
                alias = purple_url_encode(entry);
        else
                alias = "";

        if (strlen(alias) > BUDDY_ALIAS_MAXLEN)
        {
                /*purple_notify_error(gc, NULL,
                                                  _("Your new MSN friendly name 
is too long."), NULL);*/
                return;
        }

		if (pmsn_cmdproc_send)
	        (*pmsn_cmdproc_send)(cmdproc, "REA", "%s %s",
                                         purple_account_get_username(account),
                                         alias);
}

//--------------------------------------------------------------------

char* generate_status(const char *src, struct TrackInfo *ti)
{
	char *status = malloc(STRLEN);
	strcpy(status, src);
	status = put_field(status, 'p', ti->artist);
	status = put_field(status, 'a', ti->album);
	status = put_field(status, 't', ti->track);
	status = put_field(status, 'r', ti->player);

	// Duration
	char buf[20];
	sprintf(buf, "%d:%02d", ti->totalSecs/60, ti->totalSecs%60);
	status = put_field(status, 'd', buf);

	// Current time
	sprintf(buf, "%d:%02d", ti->currentSecs/60, ti->currentSecs%60);
	status = put_field(status, 'c', buf);

	// Progress bar
	int i, progress;
	if (ti->totalSecs == 0)
		progress = 0;
	else 
		progress = (int)floor(ti->currentSecs * 10.0 / ti->totalSecs);
	if (progress >= 10)
		progress = 9;
	for (i=0; i<10; ++i)
		buf[i] = '-';
	buf[progress] = '|';
	buf[10] = 0;
	status = put_field(status, 'b', buf);

	trace("Formatted status: %s", status);
	if (purple_prefs_get_bool(PREF_FILTER_ENABLE)) {
		filter(status);
		trace("Filtered status: %s", status);
	}

	// Music symbol; apply after filter, else it will discard the utf-8 character
	char symbol[4];
	symbol[0] = 0xE2;
	symbol[1] = 0x99;
	symbol[2] = 0xAB;
	symbol[3] = 0;
	status = put_field(status, 'm', symbol);
}

//--------------------------------------------------------------------

gboolean
set_status (PurpleAccount *account, char *text, struct TrackInfo *ti)
{
	PurplePluginProtocolInfo	*prInfo			= NULL;
	PurpleStatus				*status			= NULL;
	const char				*id				= NULL;
	gboolean				b				= FALSE;

	// check for protocol status format override
	char buf[100];
	gboolean overriden = FALSE;
	const char *override;

	build_pref(buf, PREF_CUSTOM_DISABLED, 
			purple_account_get_username(account));
	if (*text != 0 && purple_prefs_get_bool(buf)) {
		trace("Status changing disabled for %s account", purple_account_get_username(account));
		return TRUE;
	}

	build_pref(buf, PREF_CUSTOM_FORMAT, 
			purple_account_get_username(account));
	override = purple_prefs_get_string(buf);
	if (*text != 0 && ti->status == STATUS_NORMAL && *override != 0) {
		text = generate_status(override, ti);
		overriden = TRUE;
	}

	status = purple_account_get_active_status (account);

	if (status != NULL)
	{
		//b	= ((purple_status_is_available (status) && s_setavailable) ||
		//	(purple_status_is_away (status) && s_setaway));
		b = TRUE;
	}
	else
		b	= FALSE;

	if (b)
	{
		id	= purple_status_get_id (status);
		b	= purple_status_supports_attr (status, "message");

		if ((id != NULL) && b)
		{
			if ((text != NULL) && message_changed(text, purple_status_get_attr_string(status, "message")))
			{
				trace("Setting %s status to: %s\n",
					purple_account_get_username (account), text);
#if USE_STATUS_API
				purple_account_set_status (account, id, TRUE, "message", text, NULL);
#else
				prInfo	= purple_account_get_pluginprotocolinfo (account);

				if (prInfo && prInfo->set_status && text)
				{
					purple_status_set_attr_string (status, "message", text);
					prInfo->set_status (account, status);
				}
#endif
			}
		}

		if (id != NULL && !b && 
				!strcmp(purple_account_get_protocol_id(account), "prpl-msn")) {
			PurpleConnection* conn = purple_account_get_connection(account);

			if (purple_connection_get_state(conn) == PURPLE_CONNECTED) {
				char *nick = purple_connection_get_display_name(conn);

				if ((text != NULL) )
				{
					//purple_connection_set_display_name(conn, text);
					char buf[500];
					char *nend = nick, *p;

					while (*nend != '/' && *nend != 0) ++nend;
					if (*nend == '/' && nend != nick) --nend;
					for (p = nick; p != nend; ++p)
						*(buf+(p-nick)) = *p;
					*(buf+(p-nick)) = 0;

					if (*text != 0) {
						strcat(buf, " / ");
						strcat(buf, text);
					}
					if (message_changed(buf, nick))
						msn_act_id(conn, buf);
				}
			}
		}
	}

	if (overriden)
		free(text);
	return TRUE;
}

//--------------------------------------------------------------------

void
set_userstatus_for_active_accounts (char *userstatus, struct TrackInfo *ti)
{
        GList                   *accounts               = NULL,
                                        *head                   = NULL;
        PurpleAccount             *account                = NULL;

        head = accounts = purple_accounts_get_all_active ();

        while (accounts != NULL)
        {
                account         = (PurpleAccount *)accounts->data;

                if (account != NULL)
                        set_status (account, userstatus, ti);

                accounts        = accounts->next;
        }

        if (head != NULL)
                g_list_free (head);
}



//--------------------------------------------------------------------

static gboolean
cb_timeout(gpointer data) {
	if (g_run == 0)
		return FALSE;

	gboolean b = purple_prefs_get_bool(PREF_DISABLED);
	if (b) {
		trace("Disabled flag on!");
		return TRUE;
	}

	struct TrackInfo ti;
	memset(&ti, 0, sizeof(ti));
	ti.status = STATUS_OFF;
	int player = purple_prefs_get_int(PREF_PLAYER);

	if (player != -1) {
		b = (*g_players[player].track_func)(&ti);
		ti.player = g_players[player].name;
	} else {
		int i = 0;
		while (strlen(g_players[i].name) && (!b || ti.status == STATUS_OFF)) {
			b = (*g_players[i].track_func)(&ti);
			ti.player = g_players[i].name;
			++i;
		}
	}

	if (!b) {
		trace("Getting info failed. Setting empty status.");
		set_userstatus_for_active_accounts("", &ti);
		return TRUE;
	}
	trim(ti.player);
	trim(ti.album);
	trim(ti.track);
	trim(ti.artist);
	trace("%s,%s,%s,%s,%d", ti.player, ti.artist, ti.album, ti.track, ti.status);

	char *status;
	switch (ti.status) {
		case STATUS_OFF:
			status = generate_status(purple_prefs_get_string(PREF_OFF), &ti);
			break;
		case STATUS_PAUSED:
			status = generate_status(purple_prefs_get_string(PREF_PAUSED), &ti);
			break;
		case STATUS_NORMAL:
			status = generate_status(purple_prefs_get_string(PREF_FORMAT), &ti);
			break;
	}

	set_userstatus_for_active_accounts(status, &ti);
	free(status);
	trace("Status set for all accounts");
	return TRUE;
}

//--------------------------------------------------------------------

static gboolean
plugin_load(PurplePlugin *plugin) {
	//remove("/tmp/musictracker");	// Reset log
	trace("Plugin loaded.");
	g_tid = purple_timeout_add(INTERVAL, &cb_timeout, 0);
	g_plugin = plugin;

#ifndef WIN32
	void* handle = dlopen("libmsn.so", RTLD_NOW);
	if (!handle)
		trace("Failed to load libmsn.so. MSN nick change will not be available");
	else {
		pmsn_cmdproc_send = dlsym(handle, "msn_cmdproc_send");
		if (!pmsn_cmdproc_send)
			trace("Failed to locate msn_cmdproc_send in libmsn.so. MSN nick change will not be available");
	}
#else
	HMODULE handle = LoadLibrary("libmsn.dll");
	if (!handle)
		trace("Failed to load libmsn.dll. MSN nick change will not be available");
	else {
		pmsn_cmdproc_send = GetProcAddress(handle, "msn_cmdproc_send");
		if (!pmsn_cmdproc_send)
			trace("Failed to locate msn_cmdproc_send in libmsn.dll. MSN nick change will not be available");
	}
#endif

	// custom status format for each account
	GList *accounts = purple_accounts_get_all();
	while (accounts) {
		PurpleAccount *account = (PurpleAccount*) accounts->data;
		char buf[100];
		build_pref(buf, PREF_CUSTOM_FORMAT, 
					purple_account_get_username(account));
		if (!purple_prefs_exists(buf)) {
			purple_prefs_add_string(buf, "");
		}
		build_pref(buf, PREF_CUSTOM_DISABLED, 
					purple_account_get_username(account));
		if (!purple_prefs_exists(buf)) {
			purple_prefs_add_bool(buf, FALSE);
		}
		accounts = accounts->next;
	}
	g_run = 1;
    return TRUE;
}

//--------------------------------------------------------------------

static gboolean
plugin_unload(PurplePlugin *plugin) {
	trace("Plugin unloaded.");
	g_run = 0;
	purple_timeout_remove(g_tid);
	return TRUE;
}

//--------------------------------------------------------------------

//static GtkWidget* 
//plugin_pref_frame(PurplePlugin *plugin) {
//	

/*
static PurplePluginPrefFrame*
plugin_pref_frame(PurplePlugin *plugin) {
	PurplePluginPrefFrame* frame = purple_plugin_pref_frame_new();
	PurplePluginPref* pref;

	//--
	pref = purple_plugin_pref_new_with_label("Preferences");
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(
			PREF_DISABLED, "Disable Status changing.");
	purple_plugin_pref_frame_add(frame, pref);

#ifndef WIN32
	pref = purple_plugin_pref_new_with_name_and_label(
			PREF_LOG, "Log debug info to /tmp/musictracker.log.");
#else
	pref = purple_plugin_pref_new_with_name_and_label(
			PREF_LOG, "Log debug info to musictracker.log.");
#endif

	purple_plugin_pref_frame_add(frame, pref);

	//--
	pref = purple_plugin_pref_new_with_label("Player");
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(
			PREF_PLAYER, "Music Player:");
	purple_plugin_pref_set_type(pref, PURPLE_PLUGIN_PREF_CHOICE);
	purple_plugin_pref_add_choice(pref, "Auto-Detect", GINT_TO_POINTER(-1));

	// Add all players to choice list
	int i = 0;
	while (strlen(g_players[i].name) != 0) {
		purple_plugin_pref_add_choice(pref, g_players[i].name, i);
		++i;
	}

	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(
			PREF_XMMS_SEP, "XMMS Title Delimiter:");
	purple_plugin_pref_set_max_length(pref, 1);
	purple_plugin_pref_frame_add(frame, pref);

	//--
	pref = purple_plugin_pref_new_with_label("Status Format\n\t%p - Performer/Artist\n\t%a - Album\n\t%t - Track\n\t%d - Total Track Duration\n\t%c - Elapsed Track Time\n\t%b - Progress Bar\n\t%r - Player\n\t%m - Music symbol (may not display on some networks)");
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(
			PREF_FORMAT, "Playing:");
	purple_plugin_pref_frame_add(frame, pref);
	purple_plugin_pref_set_max_length(pref, STRLEN);

	pref = purple_plugin_pref_new_with_name_and_label(
			PREF_PAUSED, "Paused:");
	purple_plugin_pref_frame_add(frame, pref);
	purple_plugin_pref_set_max_length(pref, STRLEN);

	pref = purple_plugin_pref_new_with_name_and_label(
			PREF_OFF, "Off/Stopped:");
	purple_plugin_pref_frame_add(frame, pref);
	purple_plugin_pref_set_max_length(pref, STRLEN);

	//--
	pref = purple_plugin_pref_new_with_label("Protocol Format Override (Optional)");
	purple_plugin_pref_frame_add(frame, pref);

	GList *accounts = purple_accounts_get_all();
	trace("accounts %u", accounts);
	GHashTable *protocols = g_hash_table_new(g_str_hash, g_str_equal);
	while (accounts) {
		PurpleAccount *account = (PurpleAccount*) accounts->data;

		if (g_hash_table_lookup(protocols, purple_account_get_username(account)) == NULL) {
			char buf1[100], buf2[100];
			sprintf(buf1, PREF_CUSTOM, 
					purple_account_get_username(account));
			sprintf(buf2, "%s Playing: ", purple_account_get_username(account));

			pref = purple_plugin_pref_new_with_name_and_label(buf1, buf2);
			purple_plugin_pref_frame_add(frame, pref);
			purple_plugin_pref_set_max_length(pref, STRLEN);
			g_hash_table_insert(protocols, purple_account_get_username(account), 1);
		}
		accounts = accounts->next;
	}
	g_hash_table_destroy(protocols);

	//--
	pref = purple_plugin_pref_new_with_label("Status Filter");
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(
			PREF_FILTER_ENABLE,
			"Enable status filter.");
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(
			PREF_FILTER, 
			"Blacklist (comma-delimited):");
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(
			PREF_MASK, "Mask Character:");
	purple_plugin_pref_set_max_length(pref, 1);
	purple_plugin_pref_frame_add(frame, pref);

	return frame;
}
*/

//--------------------------------------------------------------------

static PidginPluginUiInfo ui_info = {
	pref_frame,
	0
};

static PurplePluginInfo info = {
    PURPLE_PLUGIN_MAGIC,
    PURPLE_MAJOR_VERSION,
    PURPLE_MINOR_VERSION,
    PURPLE_PLUGIN_STANDARD,
    PIDGIN_PLUGIN_TYPE,
    0,
    NULL,
    PURPLE_PRIORITY_DEFAULT,

    PLUGIN_ID,
    "MusicTracker",
    VERSION,

    "MusicTracker Plugin for Pidgin",
    "The MusicTracker Plugin allows you to customize your status message with information about currently playing song from your music player. Portions initially adopted from pidgin-currenttrack project.",
    "Arijit De <de.arijit@gmail.com>",
    "http://code.google.com/p/musictracker",

    plugin_load,
    plugin_unload,
    NULL,

    &ui_info,
    NULL,
    NULL,
    actions_list
};

//--------------------------------------------------------------------

static void
init_plugin(PurplePlugin *plugin) {
	purple_prefs_add_none("/plugins/core/musictracker");
	purple_prefs_add_string(PREF_FORMAT, "%r: %t by %p on %a (%d)");
	purple_prefs_add_string(PREF_OFF, "");
	purple_prefs_add_string(PREF_PAUSED, "%r: Paused");
	purple_prefs_add_int(PREF_PAUSED, 0);
	purple_prefs_add_int(PREF_PLAYER, -1);
	purple_prefs_add_bool(PREF_DISABLED, FALSE);
	purple_prefs_add_bool(PREF_LOG, FALSE);
	purple_prefs_add_bool(PREF_FILTER_ENABLE, FALSE);
	purple_prefs_add_string(PREF_FILTER,
			filter_get_default());
	purple_prefs_add_string(PREF_MASK, "*");

	// Player specific defaults
	purple_prefs_add_string(PREF_XMMS_SEP, "|");
	purple_prefs_add_string(PREF_MPD_HOSTNAME, "localhost");
	purple_prefs_add_string(PREF_MPD_PASSWORD, "");
	purple_prefs_add_string(PREF_MPD_PORT, "6600");
}

//--------------------------------------------------------------------

PURPLE_INIT_PLUGIN(amarok, init_plugin, info);
