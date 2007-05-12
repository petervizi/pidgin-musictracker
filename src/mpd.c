#include "musictracker.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libmpdclient.h>

gboolean get_mpd_info(struct TrackInfo* ti)
{
	char * hostname = purple_prefs_get_string(PREF_MPD_HOSTNAME);
	char * port = purple_prefs_get_string(PREF_MPD_PORT);
	char * password = purple_prefs_get_string(PREF_MPD_PASSWORD);
	if(hostname == NULL)		
		hostname = "localhost";
	if(port == NULL)
		port = "6600";
	mpd_Connection * conn = mpd_newConnection(hostname,atoi(port),10);
	if(conn->error) {
		trace("Failed to connect to MPD server");
		mpd_closeConnection(conn);
		return FALSE;
	}

	// Send password if it is not empty
	if (password && *password) {
		mpd_sendPasswordCommand(conn, password);
		mpd_finishCommand(conn);
	}

	mpd_Status * status;
	mpd_InfoEntity * entity;
	mpd_sendCommandListOkBegin(conn);
	mpd_sendStatusCommand(conn);
	mpd_sendCurrentSongCommand(conn);
	mpd_sendCommandListEnd(conn);
	if((status = mpd_getStatus(conn))==NULL) {
		trace("Error: %s\n",conn->errorStr);
		return FALSE;
		mpd_closeConnection(conn);
	}
	ti->currentSecs = status->elapsedTime;
	ti->totalSecs = status->totalTime;
	mpd_nextListOkCommand(conn);
	while((entity = mpd_getNextInfoEntity(conn))) {
		mpd_Song * song = entity->info.song;
		if(entity->type!=MPD_INFO_ENTITY_TYPE_SONG) {
			mpd_freeInfoEntity(entity);
			continue;
		}
		if(song->artist) {
			strcpy(ti->artist, song->artist);
		}
		if(song->album) {
			strcpy(ti->album, song->album);
		}
		if(song->title) {
			strcpy(ti->track, song->title);
		}
		mpd_freeInfoEntity(entity);
	}
	if(conn->error) {
		trace("Error: %s",conn->errorStr);
		mpd_closeConnection(conn);
		return FALSE;
	}
	mpd_finishCommand(conn);
	if(conn->error) {
		trace("Error: %s",conn->errorStr);
		mpd_closeConnection(conn);
		return FALSE;
	}
	switch(status->state) {
		case MPD_STATUS_STATE_STOP:
			ti->status = STATUS_OFF;
			break;
		case MPD_STATUS_STATE_PAUSE:
			ti->status = STATUS_PAUSED;
			break;
		case MPD_STATUS_STATE_PLAY:
			ti->status = STATUS_NORMAL;
			break;
	}
	mpd_freeStatus(status);
	mpd_closeConnection(conn);
	return TRUE;
}

void cb_mpd_changed(GtkWidget *entry, gpointer data)
{
	const char *pref = (const char*) data;
	purple_prefs_set_string(pref, gtk_entry_get_text(GTK_ENTRY(entry)));
}

void get_mpd_pref(GtkBox *vbox)
{
	GtkWidget *entry, *hbox;

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Hostname:"), FALSE, FALSE, 0);
	entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), purple_prefs_get_string(PREF_MPD_HOSTNAME));
	gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(cb_mpd_changed), (gpointer) PREF_MPD_HOSTNAME);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Port:"), FALSE, FALSE, 0);
	entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), purple_prefs_get_string(PREF_MPD_PORT));
	gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(cb_mpd_changed), (gpointer) PREF_MPD_PORT);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Password:"), FALSE, FALSE, 0);
	entry = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
	gtk_entry_set_text(GTK_ENTRY(entry), purple_prefs_get_string(PREF_MPD_PASSWORD));
	gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(cb_mpd_changed), (gpointer) PREF_MPD_PASSWORD);
}

