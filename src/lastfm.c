#include "musictracker.h"
#include "utils.h"
#ifndef WIN32
#include <string.h>
#include <sys/stat.h>
char *filename = "/tmp/recenttracks.txt";

void
lastfm_fetch_file(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *data)
{
	trace("Fetched %d bytes of data",len);
//	trace("Fetched %s ",url_text);
	FILE *fp;
	fp = fopen(filename,"w");
	if(fp != NULL) {
		fprintf(fp,"%s",url_text);
		fclose(fp);
	}
}

gboolean
get_lastfm_info(struct TrackInfo* ti)
{
	char status[500], track[500], url[500]="http://ws.audioscrobbler.com/1.0/user/";
	char *request, *t;
	FILE *fp;
	size_t n;
	struct stat statbuf;
	char *user = purple_prefs_get_string(PREF_LASTFM);
	if(!strcmp(user,"")) {
		trace("No last.fm user name");
		return TRUE;
	}
	trace("Got user name...%s",user);

	//Check if file exists and the last modified time is more than 30 seconds, if yes download the file again
	if(stat(filename,&statbuf) != -1) {
		int timelapsed = (int) (time(NULL)-statbuf.st_mtime);
		if(timelapsed > 30) {
			trace("The file was last modified at %d, downloading again",statbuf.st_mtime);
			strcat(url,user);
			strcat(url,"/recenttracks.txt");
			trace("Url is %s", url);
			request = g_strdup_printf("GET %s HTTP/1.0\r\n"
					"HOST: %s\r\n\r\n",
					url,"ws.audioscrobbler.com");
			trace("Request is %s",request);
			purple_util_fetch_url_request(url,TRUE,NULL,FALSE,NULL,FALSE,lastfm_fetch_file,NULL);
		}
	}
	else {
		strcat(url,user);
		strcat(url,"/recenttracks.txt");
		trace("File doesn't exist, Url is %s", url);
		request = g_strdup_printf("GET %s HTTP/1.0\r\n"
				"HOST: %s\r\n\r\n",
				url,"ws.audioscrobbler.com");
		trace("Request is %s",request);
		purple_util_fetch_url_request(url,TRUE,NULL,FALSE,NULL,FALSE,lastfm_fetch_file,NULL);
	}
	
	fp = fopen(filename,"r");
	if(fp != NULL) {
	//Interested in only first line
		fgets(status,500,fp);
		trace("Got song status...%s",status);
		if(strchr(status,',') != NULL)
			strcpy(track,strchr(status,',')+1);
		t = strchr(track,'–');
		if(t != NULL) {
			n = (size_t)(t-2-track);
			strncpy(ti->artist,track,n);
			trace("Got artist ... %s",ti->artist);
			n = strlen(track)-n-5;	//four characters in " – " and one for '\n'
			strncpy(ti->track,t+2,n);
			trace("Got track ... %s",ti->track);
			ti->status=STATUS_NORMAL;
		}
	}
	
	return TRUE;
}
#endif
