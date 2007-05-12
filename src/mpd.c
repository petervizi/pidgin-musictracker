#include "musictracker.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libmpdclient.h>

gboolean get_mpd_info(struct TrackInfo* ti)
{
	char * hostname = getenv("MPD_HOST");
	char * port = getenv("MPD_PORT");
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

