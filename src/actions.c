#include <gtk/gtk.h>
#include <string.h>
#include "actions.h"
#include "musictracker.h"
#include "utils.h"

void
accept_dialog(GtkDialog* dialog)
{
	gtk_dialog_response(dialog, GTK_RESPONSE_ACCEPT);
}

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

void
action_off_status(PurplePluginAction *action)
{
	char buf[STRLEN];
	strncpy(buf, purple_prefs_get_string("/plugins/core/musictracker/string_off"), STRLEN);
	if (input_dialog("Status to Set When Player is OFF:", buf, STRLEN)) {
		purple_prefs_set_string("/plugins/core/musictracker/string_off",
				buf);
	}
}

//--------------------------------------------------------------------

void
action_toggle_status(PurplePluginAction *action)
{
        const char *label;
	gboolean flag = !purple_prefs_get_bool("/plugins/core/musictracker/bool_disabled");
	purple_prefs_set_bool("/plugins/core/musictracker/bool_disabled", flag);
	if (flag)
          {
            set_userstatus_for_active_accounts("", 0);
            label = "Activate Status Changing";
          }
        else
          {
            label = "Deactivate Status Changing";
          }

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

	act = purple_plugin_action_new("Change Player-Off Status", action_off_status);
	list = g_list_append(list, act);

	gboolean flag = purple_prefs_get_bool("/plugins/core/musictracker/bool_disabled");
	act = purple_plugin_action_new(flag ? "Activate Status Changing" : "Deactivate Status Changing", action_toggle_status);
	list = g_list_append(list, act);
	return list;
}

