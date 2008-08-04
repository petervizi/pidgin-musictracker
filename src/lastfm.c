#include "musictracker.h"
#include "utils.h"
#include <string.h>

static char status[501] = "";

void
lastfm_fetch(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *data)
{
       trace("Fetched %d bytes of data",len);
//       trace("Fetched %s ",url_text);
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
	char track[500], url[500]="http://ws.audioscrobbler.com/1.0/user/";
	char *request, *t;
	size_t n;
	const char *user = purple_prefs_get_string(PREF_LASTFM);
	if(!strcmp(user,"")) {
		trace("No last.fm user name");
		return TRUE;
	}
	trace("Got user name...%s",user);

	//Check if file exists and the last modified time is more than 30 seconds, if yes download the file again
        static count = 0;
        if ((count % 3) != 0)
          {
		trace("last.fm ratelimit");
          }
        else
          {
		strcat(url,user);
		strcat(url,"/recenttracks.txt");
		trace("File doesn't exist, Url is %s", url);
		request = g_strdup_printf("GET %s HTTP/1.0\r\n"
				"HOST: %s\r\n\r\n",
				url,"ws.audioscrobbler.com");
		trace("Request is %s",request);
		purple_util_fetch_url_request(url,TRUE,NULL,FALSE,NULL,FALSE,lastfm_fetch,NULL);
          }
        count++;

        trace("Got song status...%s",status);
        if(strchr(status,',') != NULL)
          strcpy(track,strchr(status,',')+1);
        // artist and track are separated by a U+2013 EN DASH character
        t = strstr(track,"\342€“");
        if(t != NULL) {
          n = (size_t)(t-2-track);
          strncpy(ti->artist,track,n);
          trace("Got artist ... %s",ti->artist);
          n = strlen(track)-n-4;	//four characters in " \342€“ "
          strncpy(ti->track,t+2,n);
          trace("Got track ... %s",ti->track);
          ti->status=STATUS_NORMAL;
        }

	return TRUE;
}

void cb_lastfm_changed(GtkWidget *widget, gpointer data)
{
	const char *type = (const char*) data;
	purple_prefs_set_string(type, gtk_entry_get_text(GTK_ENTRY(widget)));
}

void get_lastfm_pref(GtkBox *vbox)
{
	GtkWidget *widget, *hbox;

	//Last.Fm User Name Box
	hbox = gtk_hbox_new(FALSE, 5); 
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0); 
	gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Last.fm user name"), FALSE, FALSE, 0); 
	widget = gtk_entry_new(); 
	gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0); 
	gtk_entry_set_text(GTK_ENTRY(widget), purple_prefs_get_string(PREF_LASTFM)); 
	g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(cb_lastfm_changed), (gpointer) PREF_LASTFM);
}

