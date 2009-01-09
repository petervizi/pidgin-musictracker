#include <gtk/gtk.h>
#include <string.h>
#include "actions.h"
#include "musictracker.h"
#include "utils.h"
#include <gtkblist.h>

#ifndef WIN32
#include <config.h>
#else
#include <config-win32.h>
#endif

#include "gettext.h"
#define _(String) dgettext (PACKAGE, String)

static
void
accept_dialog(GtkDialog* dialog)
{
	gtk_dialog_response(dialog, GTK_RESPONSE_ACCEPT);
}

static 
gboolean
input_dialog(const char *title, char *buf, int len)
{
	GtkWidget *dialog = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dialog), "MusicTracker");
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_OK, GTK_RESPONSE_ACCEPT);
	gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT);

	GtkWidget *label = gtk_label_new(title);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), label, TRUE, TRUE,
			5);

	GtkWidget *entry = gtk_entry_new_with_max_length(len);
	gtk_entry_set_text(GTK_ENTRY(entry), buf);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), entry,
			TRUE, TRUE, 5);
	g_signal_connect_swapped(entry, "activate", 
			G_CALLBACK(accept_dialog), dialog);

	gtk_widget_show_all(dialog);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		strncpy(buf, gtk_entry_get_text(GTK_ENTRY(entry)), len);
		gtk_widget_destroy(dialog);
		return TRUE;
	}
	gtk_widget_destroy (dialog);
	return FALSE;
}

//--------------------------------------------------------------------

static
void
action_off_status(PurplePluginAction *action)
{
	char buf[STRLEN];
	strncpy(buf, purple_prefs_get_string("/plugins/core/musictracker/string_off"), STRLEN);
	if (input_dialog(_("Status to Set When Player is off:"), buf, STRLEN)) {
		purple_prefs_set_string("/plugins/core/musictracker/string_off",
				buf);
	}
}

//--------------------------------------------------------------------

static
void
action_toggle_status(PurplePluginAction *action)
{
        const char *label;
	gboolean flag = !purple_prefs_get_bool(PREF_DISABLED);

	if (flag)
          {
            set_userstatus_for_active_accounts("", 0);
            label = _("Activate Status Changing");
          }
        else
          {
            label = _("Deactivate Status Changing");
          }

	purple_prefs_set_bool("/plugins/core/musictracker/bool_disabled", flag);

        // update label for action
        g_free(action->label);
        action->label = g_strdup(label);
        
        // force pidgin to update the tools menu
        pidgin_blist_update_plugin_actions();
}

//--------------------------------------------------------------------

GList*
actions_list(PurplePlugin *plugin, gpointer context)
{
	GList *list = 0;
	PurplePluginAction *act;

	gboolean flag = purple_prefs_get_bool(PREF_DISABLED);
	act = purple_plugin_action_new(flag ? _("Activate Status Changing") : _("Deactivate Status Changing"), action_toggle_status);
	list = g_list_append(list, act);

	act = purple_plugin_action_new(_("Change Player-Off Status..."), action_off_status);
	list = g_list_append(list, act);

	return list;
}

