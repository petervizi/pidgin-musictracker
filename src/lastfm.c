#include "musictracker.h"
#include "utils.h"
#include <string.h>

#define INTERVAL_SECONDS (INTERVAL/1000)

// 
// See http://www.audioscrobbler.net/data/webservices/ for documentation on the last.fm (legacy) webservices
//

static char status[501] = "";
static double minimum_delta = DBL_MAX;

void
lastfm_fetch(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *data)
{
       trace("Fetched %d bytes of data",len);
       strncpy(status, url_text, 500);
       status[500] = 0; // ensure null termination

	//Interested in only first line
       char *t = strchr(status, '\n');
       if (t)
         {
           *t = 0;
         }
}

gboolean
get_lastfm_info(struct TrackInfo* ti)
{
	char url[500]="http://ws.audioscrobbler.com/1.0/user/";
	char *request;
	const char *user = purple_prefs_get_string(PREF_LASTFM);
	if(!strcmp(user,"")) {
		trace("No last.fm user name");
		return FALSE;
	}
	trace("Got user name: %s",user);

	// Check if it's time to download the file again
        static int count = 0;
        if (count < 0)
          {
		trace("last.fm ratelimit %d",count);
          }
        else
          {
                count = count - purple_prefs_get_int(PREF_LASTFM_INTERVAL);
		strcat(url,user);
		strcat(url,"/recenttracks.txt");
		trace("URL is %s", url);
		request = g_strdup_printf("GET %s HTTP/1.0\r\n"
				"HOST: %s\r\n\r\n",
				url,"ws.audioscrobbler.com");
		trace("Request is %s",request);
		purple_util_fetch_url_request(url,TRUE,NULL,FALSE,NULL,FALSE,lastfm_fetch,NULL);
          }
        count = count + INTERVAL_SECONDS;

        trace("Got song status: '%s'",status);

        pcre *re;
        char timestamp_string[STRLEN];
        // artist and track are separated by a U+2013 EN DASH character
        re = regex("(.*),(.*) \342€“ (.*)", 0);
        if (capture(re, status, strlen(status), timestamp_string, ti->artist, ti->track))
          {

            time_t timestamp = atoi(timestamp_string);
            double delta = difftime(time(NULL), timestamp);
            ti->status=STATUS_NORMAL;

            if (delta < minimum_delta) { minimum_delta = delta; }
            trace("Epoch seconds %d, minimum delta-t %g", time(NULL), minimum_delta );
            trace("Got timestamp %d, delta-t %g, artist '%s', track '%s'", timestamp, delta, ti->artist, ti->track);

            // if the timestamp is more than the quiet interval in the past, assume player is off...
            if (delta < purple_prefs_get_int(PREF_LASTFM_QUIET))
              {
                ti->status=STATUS_NORMAL;
              }
            else
              {
                ti->status=STATUS_OFF;
              }
            
          }
        pcre_free(re);

	return TRUE;
}

void cb_lastfm_changed(GtkWidget *widget, gpointer data)
{
	const char *type = (const char*) data;
	purple_prefs_set_string(type, gtk_entry_get_text(GTK_ENTRY(widget)));
}

void cb_lastfm_interval_changed(GtkSpinButton *widget, gpointer data)
{
	purple_prefs_set_int((const char*)data, gtk_spin_button_get_value_as_int(widget));
}

void get_lastfm_pref(GtkBox *box)
{
	GtkWidget *widget, *vbox, *hbox, *label;
        GtkAdjustment *interval_spinner_adj = (GtkAdjustment *) gtk_adjustment_new(purple_prefs_get_int(PREF_LASTFM_INTERVAL),
                                                                                   INTERVAL_SECONDS, 600.0, INTERVAL_SECONDS, INTERVAL_SECONDS*5.0, INTERVAL_SECONDS*5.0);
        GtkAdjustment *quiet_spinner_adj = (GtkAdjustment *) gtk_adjustment_new(purple_prefs_get_int(PREF_LASTFM_QUIET), 
                                                                                   INTERVAL_SECONDS, 6000.0, INTERVAL_SECONDS, INTERVAL_SECONDS*5.0, INTERVAL_SECONDS*5.0);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), vbox, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0); 

	gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("user name:"), FALSE, FALSE, 0); 
	widget = gtk_entry_new(); 
	gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0); 
	gtk_entry_set_text(GTK_ENTRY(widget), purple_prefs_get_string(PREF_LASTFM)); 
	g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(cb_lastfm_changed), (gpointer) PREF_LASTFM);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("poll interval:"), FALSE, FALSE, 0);
	widget = gtk_spin_button_new(interval_spinner_adj, INTERVAL_SECONDS, 0);
	gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(widget), "value-changed", G_CALLBACK(cb_lastfm_interval_changed), (gpointer) PREF_LASTFM_INTERVAL);

        label = gtk_label_new("This is the interval (in seconds) at which we check the Last.fm feed for changes");
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("quiet interval:"), FALSE, FALSE, 0);
	widget = gtk_spin_button_new(quiet_spinner_adj, INTERVAL_SECONDS, 0);
	gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(widget), "value-changed", G_CALLBACK(cb_lastfm_interval_changed), (gpointer) PREF_LASTFM_QUIET);

        label = gtk_label_new("This is the interval (in seconds) after the track has been scrobbled which we assume the track is playing for. Unless another track is scrobbled after the interval is elapsed, we will assume the off state for the player.");
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
}

