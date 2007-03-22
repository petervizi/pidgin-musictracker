#define GAIM_PLUGINS

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dlfcn.h>
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


#define INTERVAL 10000
#define USE_STATUS_API 1

guint g_tid;
GaimPlugin *g_plugin;
gboolean s_setavailable=1, s_setaway=1, g_run=1;
void (*pmsn_cmdproc_send)(MsnCmdProc *cmdproc, const char *command,
                                          const char *format, ...);

//--------------------------------------------------------------------

gboolean
gaim_status_is_away (GaimStatus *status)
{
	GaimStatusPrimitive	primitive	= GAIM_STATUS_UNSET;

	if (!status)
		return FALSE;

	primitive	= gaim_status_type_get_primitive (gaim_status_get_type (status));

	return ((primitive == GAIM_STATUS_AWAY) || (primitive == GAIM_STATUS_EXTENDED_AWAY));
}

//--------------------------------------------------------------------

gboolean
gaim_status_supports_attr (GaimStatus *status, const char *id)
{
	gboolean		b				= FALSE;
	GaimStatusType	*status_type	= NULL;
	GList			*attrs			= NULL;
	GaimStatusAttr	*attr			= NULL;

	if (!status || !id)
		return b;

	status_type	= gaim_status_get_type(status);

	if (status_type != NULL)
	{
		attrs	= gaim_status_type_get_attrs (status_type);

		while (attrs != NULL)
		{
			attr	= (GaimStatusAttr *)attrs->data;

			if (attr != NULL)
			{
				if (strncasecmp(id, gaim_status_attr_get_id (attr), strlen(id)) == 0)
					b	= TRUE;
			}

			attrs	= attrs->next;
		}
	}

	return b;
}

//--------------------------------------------------------------------

GaimPluginProtocolInfo *
gaim_account_get_pluginprotocolinfo (GaimAccount *account)
{
	GaimConnection			*connection		= NULL;
	GaimPlugin				*pPlugin		= NULL;
	GaimPluginInfo			*pInfo			= NULL;
	GaimPluginProtocolInfo	*prInfo			= NULL;

	if (!account)
		return NULL;

	connection	= gaim_account_get_connection (account);

	if (connection != NULL)
		pPlugin		= connection->prpl;

	if (pPlugin != NULL)
	{
		pInfo	= pPlugin->info;
		if (pInfo != NULL)
			prInfo	= (GaimPluginProtocolInfo *)pInfo->extra_info;
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

msn_act_id(GaimConnection *gc, const char *entry)
{
        MsnCmdProc *cmdproc;
        MsnSession *session;
        GaimAccount *account;
        const char *alias;

        session = gc->proto_data;
        cmdproc = session->notification->cmdproc;
        account = gaim_connection_get_account(gc);

        if(entry && strlen(entry))
                alias = gaim_url_encode(entry);
        else
                alias = "";

        if (strlen(alias) > BUDDY_ALIAS_MAXLEN)
        {
                /*gaim_notify_error(gc, NULL,
                                                  _("Your new MSN friendly name 
is too long."), NULL);*/
                return;
        }

		if (pmsn_cmdproc_send)
	        (*pmsn_cmdproc_send)(cmdproc, "REA", "%s %s",
                                         gaim_account_get_username(account),
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
	if (gaim_prefs_get_bool(PREF_FILTER_ENABLE)) {
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
set_status (GaimAccount *account, char *text, struct TrackInfo *ti)
{
	GaimPluginProtocolInfo	*prInfo			= NULL;
	GaimStatus				*status			= NULL;
	const char				*id				= NULL;
	gboolean				b				= FALSE;

	// check for protocol status format override
	char buf[100];
	gboolean overriden = FALSE;
	const char *override;
	sprintf(buf, PREF_CUSTOM, 
			gaim_account_get_protocol_name(account));
	override = gaim_prefs_get_string(buf);
	if (ti->status == STATUS_NORMAL && *override != 0) {
		text = generate_status(override, ti);
		overriden = TRUE;
	}

	status = gaim_account_get_active_status (account);

	if (status != NULL)
	{
		//b	= ((gaim_status_is_available (status) && s_setavailable) ||
		//	(gaim_status_is_away (status) && s_setaway));
		b = TRUE;
	}
	else
		b	= FALSE;

	if (b)
	{
		id	= gaim_status_get_id (status);
		b	= gaim_status_supports_attr (status, "message");

		if ((id != NULL) && b)
		{
			if ((text != NULL) && message_changed(text, gaim_status_get_attr_string(status, "message")))
			{
				trace("Setting %s status to: %s\n",
					gaim_account_get_protocol_name (account), text);
#if USE_STATUS_API
				gaim_account_set_status (account, id, TRUE, "message", text, NULL);
#else
				prInfo	= gaim_account_get_pluginprotocolinfo (account);

				if (prInfo && prInfo->set_status && text)
				{
					gaim_status_set_attr_string (status, "message", text);
					prInfo->set_status (account, status);
				}
#endif
			}
		}

		if (id != NULL && !b && 
				!strcmp(gaim_account_get_protocol_id(account), "prpl-msn")) {
			GaimConnection* conn = gaim_account_get_connection(account);

			if (gaim_connection_get_state(conn) == GAIM_CONNECTED) {
				char *nick = gaim_connection_get_display_name(conn);

				if ((text != NULL) )
				{
					//gaim_connection_set_display_name(conn, text);
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

static void
set_userstatus_for_active_accounts (char *userstatus, struct TrackInfo *ti)
{
        GList                   *accounts               = NULL,
                                        *head                   = NULL;
        GaimAccount             *account                = NULL;

        head = accounts = gaim_accounts_get_all_active ();

        while (accounts != NULL)
        {
                account         = (GaimAccount *)accounts->data;

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

	gboolean b = gaim_prefs_get_bool(PREF_DISABLED);
	if (b) {
		trace("Disabled flag on!");
		return TRUE;
	}

	struct TrackInfo ti;
	memset(&ti, 0, sizeof(ti));
	ti.status = STATUS_OFF;
	int player = gaim_prefs_get_int(PREF_PLAYER);

	switch (player) {
		case PLAYER_XMMS:
			trace("Getting XMMS info");
			b = get_xmms_info(&ti);
			break;
		case PLAYER_AUDACIOUS:
			trace("Getting Audacious info");
			b = get_audacious_info(&ti);
			break;
		case PLAYER_AMAROK:
			trace("Getting Amarok info");
			b = get_amarok_info(&ti);
			break;
		case PLAYER_EXAILE:
			trace("Getting Exaile info");
			b = get_exaile_info(&ti);
			break;
		case PLAYER_MPD:
			trace("Getting MPD info");
			b = get_mpd_info(&ti);
			break;
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
			status = generate_status(gaim_prefs_get_string(PREF_OFF), &ti);
			break;
		case STATUS_PAUSED:
			status = generate_status(gaim_prefs_get_string(PREF_PAUSED), &ti);
			break;
		case STATUS_NORMAL:
			status = generate_status(gaim_prefs_get_string(PREF_FORMAT), &ti);
			break;
	}

	set_userstatus_for_active_accounts(status, &ti);
	free(status);
	trace("Status set for all accounts");
	return TRUE;
}

//--------------------------------------------------------------------

static gboolean
plugin_load(GaimPlugin *plugin) {
	//remove("/tmp/musictracker");	// Reset log
	trace("Plugin loaded.");
	g_tid = gaim_timeout_add(INTERVAL, &cb_timeout, 0);
	g_plugin = plugin;

	void* handle = dlopen("libmsn.so", RTLD_NOW);
	if (!handle)
		trace("Failed to load libmsn.so. MSN nick change will not be available");
	else {
		pmsn_cmdproc_send = dlsym(handle, "msn_cmdproc_send");
		if (!pmsn_cmdproc_send)
			trace("Failed to locate msn_cmdproc_send in libmsn.so. MSN nick change will not be available");
	}

	// custom status format for each protocol
	GList *accounts = gaim_accounts_get_all();
	while (accounts) {
		GaimAccount *account = (GaimAccount*) accounts->data;
		char buf[100];
		sprintf(buf, PREF_CUSTOM, 
					gaim_account_get_protocol_name(account));

		if (!gaim_prefs_exists(buf)) {
			gaim_prefs_add_string(buf, "");
		}
		accounts = accounts->next;
	}
	g_run = 1;
    return TRUE;
}

//--------------------------------------------------------------------

static gboolean
plugin_unload(GaimPlugin *plugin) {
	trace("Plugin unloaded.");
	g_run = 0;
	gaim_timeout_remove(g_tid);
}

//--------------------------------------------------------------------

//static GtkWidget* 
//plugin_pref_frame(GaimPlugin *plugin) {
//	

static GaimPluginPrefFrame*
plugin_pref_frame(GaimPlugin *plugin) {
	GaimPluginPrefFrame* frame = gaim_plugin_pref_frame_new();
	GaimPluginPref* pref;

	//--
	pref = gaim_plugin_pref_new_with_label("Preferences");
	gaim_plugin_pref_frame_add(frame, pref);

	pref = gaim_plugin_pref_new_with_name_and_label(
			PREF_DISABLED, "Disable Status changing.");
	gaim_plugin_pref_frame_add(frame, pref);

	pref = gaim_plugin_pref_new_with_name_and_label(
			PREF_LOG, "Log debug info to /tmp/musictracker.");
	gaim_plugin_pref_frame_add(frame, pref);

	//--
	pref = gaim_plugin_pref_new_with_label("Player");
	gaim_plugin_pref_frame_add(frame, pref);

	pref = gaim_plugin_pref_new_with_name_and_label(
			PREF_PLAYER, "Music Player:");
	gaim_plugin_pref_set_type(pref, GAIM_PLUGIN_PREF_CHOICE);
	gaim_plugin_pref_add_choice(pref, "XMMS", GINT_TO_POINTER(PLAYER_XMMS));
	gaim_plugin_pref_add_choice(pref, "Amarok", GINT_TO_POINTER(PLAYER_AMAROK));
	gaim_plugin_pref_add_choice(pref, "Exaile", GINT_TO_POINTER(PLAYER_EXAILE));
	gaim_plugin_pref_add_choice(pref, "Audacious", GINT_TO_POINTER(PLAYER_AUDACIOUS));
	gaim_plugin_pref_add_choice(pref, "MPD", GINT_TO_POINTER(PLAYER_MPD));
	gaim_plugin_pref_frame_add(frame, pref);

	pref = gaim_plugin_pref_new_with_name_and_label(
			PREF_XMMS_SEP, "XMMS Title Delimiter:");
	gaim_plugin_pref_set_max_length(pref, 1);
	gaim_plugin_pref_frame_add(frame, pref);

	//--
	pref = gaim_plugin_pref_new_with_label("Status Format\n\t%p - Performer/Artist\n\t%a - Album\n\t%t - Track\n\t%d - Total Track Duration\n\t%c - Elapsed Track Time\n\t%b - Progress Bar\n\t%r - Player\n\t%m - Music symbol (may not display on some networks)");
	gaim_plugin_pref_frame_add(frame, pref);

	pref = gaim_plugin_pref_new_with_name_and_label(
			PREF_FORMAT, "Playing:");
	gaim_plugin_pref_frame_add(frame, pref);
	gaim_plugin_pref_set_max_length(pref, STRLEN);

	pref = gaim_plugin_pref_new_with_name_and_label(
			PREF_PAUSED, "Paused:");
	gaim_plugin_pref_frame_add(frame, pref);
	gaim_plugin_pref_set_max_length(pref, STRLEN);

	pref = gaim_plugin_pref_new_with_name_and_label(
			PREF_OFF, "Off/Stopped:");
	gaim_plugin_pref_frame_add(frame, pref);
	gaim_plugin_pref_set_max_length(pref, STRLEN);

	//--
	pref = gaim_plugin_pref_new_with_label("Protocol Format Override (Optional)");
	gaim_plugin_pref_frame_add(frame, pref);

	GList *accounts = gaim_accounts_get_all();
	trace("accounts %u", accounts);
	GHashTable *protocols = g_hash_table_new(g_str_hash, g_str_equal);
	while (accounts) {
		GaimAccount *account = (GaimAccount*) accounts->data;
		trace("pr %s", gaim_account_get_protocol_name(account));

		if (g_hash_table_lookup(protocols, gaim_account_get_protocol_name(account)) == NULL) {
			char buf1[100], buf2[100];
			sprintf(buf1, PREF_CUSTOM, 
					gaim_account_get_protocol_name(account));
			sprintf(buf2, "%s Playing: ", gaim_account_get_protocol_name(account));

			trace("%s %s", buf1, buf2);
			pref = gaim_plugin_pref_new_with_name_and_label(buf1, buf2);
			trace("%d", pref);
			gaim_plugin_pref_frame_add(frame, pref);
			gaim_plugin_pref_set_max_length(pref, STRLEN);
			g_hash_table_insert(protocols, gaim_account_get_protocol_name(account), 1);
		}
		accounts = accounts->next;
	}
	g_hash_table_destroy(protocols);

	//--
	pref = gaim_plugin_pref_new_with_label("Status Filter");
	gaim_plugin_pref_frame_add(frame, pref);

	pref = gaim_plugin_pref_new_with_name_and_label(
			PREF_FILTER_ENABLE,
			"Enable status filter.");
	gaim_plugin_pref_frame_add(frame, pref);

	pref = gaim_plugin_pref_new_with_name_and_label(
			PREF_FILTER, 
			"Blacklist (comma-delimited):");
	gaim_plugin_pref_frame_add(frame, pref);

	pref = gaim_plugin_pref_new_with_name_and_label(
			PREF_MASK, "Mask Character:");
	gaim_plugin_pref_set_max_length(pref, 1);
	gaim_plugin_pref_frame_add(frame, pref);

	return frame;
}

//--------------------------------------------------------------------

static GaimPluginUiInfo prefs_info = {
	plugin_pref_frame,
	0,   /* page_num (Reserved) */
	NULL /* frame (Reserved) */
};

static GaimPluginInfo info = {
    GAIM_PLUGIN_MAGIC,
    GAIM_MAJOR_VERSION,
    GAIM_MINOR_VERSION,
    GAIM_PLUGIN_STANDARD,
    NULL,
    0,
    NULL,
    GAIM_PRIORITY_DEFAULT,

    PLUGIN_ID,
    "MusicTracker",
    VERSION,

    "Gaim MusicTracker Plugin",
    "Gaim MusicTracker Plugin. Portions adopted from gaim-currenttrack project.",
    "Arijit De <qool@users.sourceforge.net>",
    "gaim.sourceforge.net",

    plugin_load,
    plugin_unload,
    NULL,

    NULL,
    NULL,
    &prefs_info,
    actions_list
};

//--------------------------------------------------------------------

static void
init_plugin(GaimPlugin *plugin) {
	gaim_prefs_add_none("/plugins/core/musictracker");
	gaim_prefs_add_string(PREF_FORMAT, "%r: %t by %p on %a (%d)");
	gaim_prefs_add_string(PREF_XMMS_SEP, "|");
	gaim_prefs_add_string(PREF_OFF, "");
	gaim_prefs_add_string(PREF_PAUSED, "%r: Paused");
	gaim_prefs_add_int(PREF_PAUSED, 0);
	gaim_prefs_add_bool(PREF_DISABLED, FALSE);
	gaim_prefs_add_bool(PREF_LOG, FALSE);
	gaim_prefs_add_bool(PREF_FILTER_ENABLE, TRUE);
	gaim_prefs_add_string(PREF_FILTER,
			filter_get_default());
	gaim_prefs_add_string(PREF_MASK, "*");

}

//--------------------------------------------------------------------

GAIM_INIT_PLUGIN(amarok, init_plugin, info);
