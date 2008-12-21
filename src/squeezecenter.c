/*
 * musictracker
 * squeezecenter.c
 * retrieve track info from SqueezeCenter/SlimServer
 *
 * Copyright (C) 2008, Phillip Camp <phillip.camp@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02111-1301, USA.
 *
 */

#include "musictracker.h"
#include "utils.h"
#include "glib.h"
#include "glib.h"
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <config.h>
#include "gettext.h"
#define _(String) dgettext (PACKAGE, String)

#define SELECT_ERRNO_IGNORE   (errno == EINTR)
#define SENDRECV_ERRNO_IGNORE (errno == EINTR || errno == EAGAIN)

#define SQUEEZECENTER_TIMEOUT 10
#define SQUEEZECENTER_ID 40
#define SQUEEZECENTER_NAME 40
#define SQUEEZECENTER_STATUS 40
#define SQUEEZECENTER_VERSION 40
#define SQUEEZECENTER_ERROR_BUFFER 1024
#define SQUEEZECENTER_BUFFER_LENGTH 1024
#define SQUEEZECENTER_PLAYER_BUFFER 20
#define SQUEEZECENTER_TI_STRING STRLEN

#define SQUEEZECENTER_ADVICE _("Advice:\n'#' Find playing player\n'*' Find powered player\n'id/name' prefix with ! to ignore if not playing\n\nPort Defaults to 9090, Multiple servers accepted delimited by ','")

typedef struct _squeezecenter_trackinfo {
    char title[SQUEEZECENTER_TI_STRING];
    char genre[SQUEEZECENTER_TI_STRING];
    char artist[SQUEEZECENTER_TI_STRING];
    char album[SQUEEZECENTER_TI_STRING];
    int duration;
} squeezecenter_Trackinfo;

typedef struct _squeezecenter_Player {
    char name[SQUEEZECENTER_NAME];
    char id[SQUEEZECENTER_ID];
    char mode[SQUEEZECENTER_TI_STRING];     
    int connected;

    int remote;
    char current_title[SQUEEZECENTER_TI_STRING];
    int power;
    int time;
    //int rate;
    int duration;
    //int sleep;
    //int will_sleep_in;
    //char *sync_master;
    //char *sync_slaves;
    //int mixer_volume;
    //int playlist_repeat;
    //int playlist_shuffle;
    //char *playlist_name;
    //char *playlist_id;
    //int playlist_modified;
    //int playlist_cur_index;
    //int playlist_tracks;
    squeezecenter_Trackinfo tinfo;
} squeezecenter_Player;


typedef struct _squeezecenter_Connection {
    int sock;
    float timeout;
    char errorStr[SQUEEZECENTER_ERROR_BUFFER];
    char buffer[SQUEEZECENTER_BUFFER_LENGTH];
    char command[SQUEEZECENTER_BUFFER_LENGTH];
    int buflen;
    char version[SQUEEZECENTER_VERSION];
    char server[SQUEEZECENTER_VERSION];
    int players;
    squeezecenter_Player *players_array;
} squeezecenter_Connection;


/* State */
static squeezecenter_Connection squeezecenter = {-1,0.0,"","","",0,"","",0,NULL};


/* Util */
int urldecodestr( char *buf )
{
	const char *r = buf;
	while( *r ) {
		switch( *r ) {
			case '%':
				/* Is this followed by a 2-digit hex string? */
				if( isxdigit(r[1]) && isxdigit(r[2]) )
				{
					char hexstr[3];
					int i;
					hexstr[0] = *++r;
					hexstr[1] = *++r;
					hexstr[2] = '\0';
					++r;
					sscanf( hexstr, "%x", &i );
					*buf++ = i;
					break;
				}
				/* No, examine next char. */
			default:
				*buf++ = *r++;
		}
	}
	*buf = 0;
	return 0;
}

/* Squeeze Center Comms */
gboolean squeezecenter_command(squeezecenter_Connection * connection, char * command);

static int do_connect_fail(squeezecenter_Connection *connection,
                           const struct sockaddr *serv_addr, int addrlen)
{
        int flags = fcntl(connection->sock, F_GETFL, 0);
        fcntl(connection->sock, F_SETFL, flags | O_NONBLOCK);
        return (connect(connection->sock,serv_addr,addrlen)<0 &&
                                errno!=EINPROGRESS);
}

void squeezecenter_disconnect(squeezecenter_Connection * connection) {
       close(connection->sock);
       connection->sock = -1;
}

gboolean squeezecenter_connect(squeezecenter_Connection * connection, const char * host, int port, float timeout)
{
        struct hostent * he;
        struct sockaddr * dest;
        int destlen;
        struct sockaddr_in sin;

        connection->version[0] = '\0';
	connection->players = 0;

        if(!(he=gethostbyname(host))) {
                snprintf(connection->errorStr,SQUEEZECENTER_ERROR_BUFFER,
                                "host \"%s\" not found",host);
                return FALSE;
        }

        memset(&sin,0,sizeof(struct sockaddr_in));
        /*dest.sin_family = he->h_addrtype;*/
        sin.sin_family = AF_INET;
        sin.sin_port = htons(port);

        switch(he->h_addrtype) {
        case AF_INET:
                memcpy((char *)&sin.sin_addr.s_addr,(char *)he->h_addr,
                                he->h_length);
                dest = (struct sockaddr *)&sin;
                destlen = sizeof(struct sockaddr_in);
                break;
        default:
                strcpy(connection->errorStr,"address type is not IPv4\n");
                return FALSE;
                break;
        }

        if((connection->sock = socket(dest->sa_family,SOCK_STREAM,0))<0) {
                strcpy(connection->errorStr,"problems creating socket");
                return FALSE;
        }

        if (do_connect_fail(connection, dest, destlen)) {
                snprintf(connection->errorStr,SQUEEZECENTER_ERROR_BUFFER,
                                "problems connecting to \"%s\" on port"
                                " %i",host,port);
                return FALSE;
        }
	connection->timeout = timeout;
        snprintf(connection->server,SQUEEZECENTER_VERSION, "%s:%d",host,port);
	return TRUE;
}


gboolean squeezecenter_do_login(squeezecenter_Connection * connection,const char *user,const char *passwd) {

        snprintf(connection->command,SQUEEZECENTER_BUFFER_LENGTH, "login %s %s\n",user,passwd);

        if(!squeezecenter_command(connection,connection->command)) {
                return FALSE;
        }
	if (strstr(connection->buffer,"******") == NULL ) {
                snprintf(connection->errorStr,SQUEEZECENTER_ERROR_BUFFER,"login Failed user(%s)",user);
		return FALSE;
	} else {
		return TRUE;
	}
}

gboolean squeezecenter_get_version(squeezecenter_Connection * connection) {
        if(!squeezecenter_command(connection,"version ?\n")) {
                return FALSE;
        }
	strncpy(connection->version,connection->buffer,SQUEEZECENTER_VERSION);
	return TRUE;
}

gboolean squeezecenter_connection_preamble(squeezecenter_Connection * connection,  const char *username, const char *pwd )
{
	if (squeezecenter_do_login(connection,username,pwd) == FALSE) {
	   squeezecenter_disconnect(connection);
	   return FALSE;
	}

        /*version ?*/
	if (squeezecenter_get_version(connection) == FALSE) {
	   squeezecenter_disconnect(connection);
	   return FALSE;
	}
	return TRUE;
}

gboolean squeezecenter_get_player_count(squeezecenter_Connection * connection) {
        if(!squeezecenter_command(connection,"player count ?\n")) {
                return FALSE;
        }
	if(sscanf(connection->buffer+13,"%d",&connection->players) != 1) {
	        snprintf(connection->errorStr,SQUEEZECENTER_ERROR_BUFFER,"player count parse error");
                return FALSE;
	}
        return TRUE;
}


gboolean squeezecenter_get_player_id(squeezecenter_Connection * connection,int index, char *p )
{
        int len;
        snprintf(connection->command,SQUEEZECENTER_BUFFER_LENGTH, "player id %d ?\n",index);
        len = strlen(connection->command);

        if(!squeezecenter_command(connection,connection->command)) {
                return FALSE;
        }
	connection->buffer[connection->buflen-1] = '\0';
	urldecodestr(connection->buffer);
	strncpy(p, connection->buffer+len-2,SQUEEZECENTER_ID);
        return TRUE;
}

gboolean squeezecenter_get_player_name(squeezecenter_Connection * connection,int index, char *p )
{
        int len;
        snprintf(connection->command,SQUEEZECENTER_BUFFER_LENGTH, "player name %d ?\n",index);
        len = strlen(connection->command);

        if(!squeezecenter_command(connection,connection->command)) {
                return FALSE;
        }
	connection->buffer[connection->buflen-1] = '\0';
	urldecodestr(connection->buffer);
	strncpy(p, connection->buffer+len-2,SQUEEZECENTER_NAME);

        return TRUE;
}

gboolean squeezecenter_get_players(squeezecenter_Connection * connection)
{
	int i = 0;
	squeezecenter_Player *pi = g_malloc0(sizeof(squeezecenter_Player)*connection->players);

        if ( pi == NULL) {
	  snprintf(connection->errorStr,SQUEEZECENTER_ERROR_BUFFER,"Players alloc failure");
	  return FALSE;
        }

	for (i=0;i < connection->players;i++) {
             if (!(squeezecenter_get_player_id(connection,i,(char *)pi[i].id) && squeezecenter_get_player_name(connection,i,(char *) pi[i].name))) {
	       g_free(pi);
	       return FALSE;
	     }
	}
	if (connection->players_array != NULL) {
	   free(connection->players_array);
	}

        connection->players_array=pi;
	return TRUE;
}



void squeezecenter_get_player_status_populate(squeezecenter_Player *status,gchar *key,gchar *data) {
	switch(key[0]) {
	        case 'p':
		  switch(key[1]) {
		    case 'o':
		       if (strcmp("wer",key+2)==0) {
		          sscanf(data,"%d",&status->power);
        trace("squeezecenter_get_player_status_populate(\"%s\",\"%s\") Set",key,data);
		       }
		    break;

		    case 'l':
		       if (strlen(key) <= 8) {
		          break;
		       }
		       switch(key[7]) {
		         case 'n':
		           if (strcmp("name",key+7)==0) {
		              g_strlcpy(status->name, data,SQUEEZECENTER_NAME);
        trace("squeezecenter_get_player_status_populate(\"%s\",\"%s\") Set",key,data);
		           }
		         break;
		         case 'o':
		            if (strcmp("connected",key+7)==0) {
		               sscanf(data,"%d",&status->connected);
        trace("squeezecenter_get_player_status_populate(\"%s\",\"%s\") Set",key,data);
		            }
		         break;
		       }
		    break;
		  }   
		break;
         
	        case 'c':
		   switch(key[1]) {
		      case 'u':
		         if (strcmp("rrent_title",key+2)==0) {
		             g_strlcpy(status->current_title, data,SQUEEZECENTER_TI_STRING);
        trace("squeezecenter_get_player_status_populate(\"%s\",\"%s\") Set",key,data);
		         }
		      break;
		   }
		break;

		case 'a':
		  switch(key[1]) {
		     case 'r':
		    	if (strcmp("tist",key+2)==0) {
		             g_strlcpy((char*)status->tinfo.artist, data,SQUEEZECENTER_TI_STRING);
        trace("squeezecenter_get_player_status_populate(\"%s\",\"%s\") Set",key,data);
		    	}
		     break;

		     case 'l':
		    	if (strcmp("bum",key+2)==0) {
		             g_strlcpy((char*)status->tinfo.album, data,SQUEEZECENTER_TI_STRING);
        trace("squeezecenter_get_player_status_populate(\"%s\",\"%s\") Set",key,data);
		    	}
		     break;
		  }

		case 't':
		  if ( key[1] == '\0' ) { break; }
		  switch(key[2]) {
		    case 't':
		       if (strcmp("le",key+3)==0) {
		             g_strlcpy((char*)status->tinfo.title, data,SQUEEZECENTER_TI_STRING);
        trace("squeezecenter_get_player_status_populate(\"%s\",\"%s\") Set",key,data);
		       }
		    break;
		    case 'm':
		       if (strcmp("e",key+3)==0) {
		         sscanf(data,"%d",&status->time);
        trace("squeezecenter_get_player_status_populate(\"%s\",\"%s\") Set",key,data);
		       }
		    break;
		  }
		break;

		case 'g':
		    if (strcmp("enre",key+1)==0) {
		             g_strlcpy(status->tinfo.genre, data,SQUEEZECENTER_TI_STRING);
        trace("squeezecenter_get_player_status_populate(\"%s\",\"%s\") Set",key,data);
		    }
		break;

		case 'r':
		    if (strcmp("emote",key+1)==0) {
		      sscanf(data,"%d",&status->remote);
        trace("squeezecenter_get_player_status_populate(\"%s\",\"%s\") Set",key,data);
		    }
	        break;

		case 'd':
		   if (strcmp("uration",key+1)==0) {
		      sscanf(data,"%d",&status->duration);
        trace("squeezecenter_get_player_status_populate(\"%s\",\"%s\") Set",key,data);
		   }
		break;

		case 'm':
		   if (strcmp("ode",key+1)==0) {
		      g_strlcpy(status->mode, data,SQUEEZECENTER_TI_STRING);
        trace("squeezecenter_get_player_status_populate(\"%s\",\"%s\") Set",key,data);
		   }
		break;
	}

}

gboolean squeezecenter_get_player_current_status(squeezecenter_Connection * connection, squeezecenter_Player *status, char *player_id)
{
        snprintf(connection->command,SQUEEZECENTER_BUFFER_LENGTH, "%s status - 1\n",player_id);

        if(!squeezecenter_command(connection,connection->command)) {
                return FALSE;
        }
        connection->buffer[connection->buflen-1] = '\0';
	trace("Squeezenter Player Status recived: %s",connection->buffer);
        /* split on space */
	char *res;
	res = g_strstr_len(connection->buffer,connection->buflen,"player_name");
	if (res == NULL ) {
		    snprintf(connection->errorStr,SQUEEZECENTER_ERROR_BUFFER,"status command error (%s) \"%s\".",player_id,connection->buffer);
		return FALSE;
	}

        char *n,*t = (char *)res;
	do {
                gchar *d;
        	n = strchr(t,' ');
        	if ( n != NULL ) {
           		*n = '\0';
        	}
              
		/* decode */
        	urldecodestr(t);
         
        	/* split on first : */
                d = strchr(t,':');
		if (d == NULL) {
		    snprintf(connection->errorStr,SQUEEZECENTER_ERROR_BUFFER,"status parse tag split player(%s) \"%s\".",player_id,t);
		    trace("squeezecenter Parse error (%s)",t);
	            return FALSE;
		}
		*d = '\0';
		d++;
        	/* stash by tag into player status*/
                squeezecenter_get_player_status_populate(status,t,d);
        
		 if (n != NULL) {
       		    t = n+1;
        	}

	} while (n != NULL );

	return TRUE;
}

int squeezecenter_connected(squeezecenter_Connection * connection) {
   int sockerr, ret;
   socklen_t sockerrlen;
   fd_set fds;
   struct timeval tv;

   sockerrlen = sizeof(sockerr);
   FD_ZERO(&fds);
   FD_SET(connection->sock,&fds);
   tv.tv_sec = 0;
   tv.tv_usec = 0;

   ret = select(connection->sock+1,NULL,&fds,NULL,&tv);
   if (ret == -1 && SELECT_ERRNO_IGNORE ) {
      return 0;
   }
   if (ret == 1) {
       ret = getsockopt(connection->sock,SOL_SOCKET,SO_ERROR,(void *)&sockerr,&sockerrlen);
       if (ret == 0 && sockerr != 0) {
          trace("Socket error (%s)",strerror(sockerr));
          return -1;
       } if ( ret == 0 && sockerr == 0) {
          return 1;
       }
       //getsockopt error
       trace("getsockopt error (%s)",strerror(errno));
       return -1;
   }
   return ret;
}

gboolean squeezecenter_command(squeezecenter_Connection * connection, char * command)
{
        int ret, err;
        struct timeval tv;
        fd_set fds;
        char * commandPtr = command, * rt;
        int commandLen = strlen(command);

        if (command[commandLen-1] != '\n') {
	   snprintf(connection->errorStr,SQUEEZECENTER_ERROR_BUFFER,"Command not terminated \"%s\"",command);
           return FALSE;
	}
	if ( connection->command != command ) {
	   strncpy(connection->command,command,SQUEEZECENTER_BUFFER_LENGTH);
	}

        FD_ZERO(&fds);
        FD_SET(connection->sock,&fds);
        tv.tv_sec = (int)connection->timeout;
        tv.tv_usec = (int)(connection->timeout*1e6 - tv.tv_sec*1000000 + 0.5);


        while((ret = select(connection->sock+1,NULL,&fds,NULL,&tv)==1) || (ret==-1 && SELECT_ERRNO_IGNORE)) {
                ret = send(connection->sock,commandPtr,commandLen,MSG_DONTWAIT);   
		if(ret<=0)
                {
			if (SENDRECV_ERRNO_IGNORE) { 
			   continue;
			}
                        snprintf(connection->errorStr,SQUEEZECENTER_ERROR_BUFFER,
                                 "problems giving command \"%s\"",command);
                        return FALSE;
                }
                else {
                        commandPtr+=ret;
                        commandLen-=ret;
                }

                if(commandLen<=0) break;
        }
        if(commandLen>0) {
                perror("");
                snprintf(connection->errorStr,SQUEEZECENTER_ERROR_BUFFER,
                         "timeout sending command \"%s\"",command);
                return FALSE;
        }
	connection->buffer[0] = '\0';
	connection->buflen = 0;
        while(!(rt = strstr(connection->buffer,"\n"))) {
                FD_ZERO(&fds);
                FD_SET(connection->sock,&fds);
                if((err = select(connection->sock+1,&fds,NULL,NULL,&tv)) == 1) {                        int readed;
                        readed = recv(connection->sock,
                                        &(connection->buffer[connection->buflen]),
                                        SQUEEZECENTER_BUFFER_LENGTH-connection->buflen,0);
                        if(readed<=0) {
                                snprintf(connection->errorStr,SQUEEZECENTER_BUFFER_LENGTH,
                                                "problems getting a response %s", strerror(errno));
                                return FALSE;
                        }
                        connection->buflen+=readed;
                        connection->buffer[connection->buflen] = '\0';
                }
                else if(err<0) {
                        if (SELECT_ERRNO_IGNORE)
                                continue;
                        snprintf(connection->errorStr,
                                        SQUEEZECENTER_ERROR_BUFFER,
                                        "problems connecting");
                        return FALSE;
                }
                else {
                        snprintf(connection->errorStr,SQUEEZECENTER_ERROR_BUFFER,
                                        "timeout in attempting to get a response \n");
                        return FALSE;
                }
        }

        return TRUE;
}

/* Squeezecenter player and Server Selection logic */
gboolean get_squeezecenter_connection(squeezecenter_Connection * connection,const char * servers, float timeout, int *last)
{    
        int count = 0;

        if (connection->sock >= 0) { //chk error state and reeconnect if nessary
		return TRUE;
        }

	char *d,*s = (char *)servers,*p;
	do {
	d = strchr(s,',');
	if ( d != NULL ) {
           *d = '\0';
	}

	p = strchr(s,':');
	int port = 9090;
	if ( p != NULL ) {
	  *p = '\0';
	  sscanf(p+1,"%d",&port);
	}
	if ( count++ >= *last ) {
	   trace("Connection Attempt %s:%d %d:%d",s,port,count,*last);
	   squeezecenter_connect(connection,s,port,SQUEEZECENTER_TIMEOUT);
	}
	// Restore
	if ( p != NULL ) {
	      *p = ':';
	}
	if (d != NULL) {
	   *d = ',';
	   s = d+1;
	}
	  
	} while(d != NULL && connection->sock < 0);

        if (connection->sock < 0) {
	   *last = 0;
	   return FALSE;
	}
	if (d == NULL) {
	   *last = 0;
	} else {
          *last = count;
	}

	return TRUE;
}

squeezecenter_Player * get_squeezecenter_status(squeezecenter_Connection * connection, const char *players) 
{
	char *d,*p = (char *)players;
	int i;
	gboolean playing = FALSE;
        squeezecenter_Player *pl = NULL;
        do {
		playing = FALSE;
		// Segment
        	d = strchr(p,',');
        	if ( d != NULL ) {
                   *d = '\0';
                }
	        trace("Find (%s)",p);
		
		switch(p[0]) {
			case '\0':
			break;
		        case '#':
			     playing = TRUE;
			case '*':
			     for (i=0;i < connection->players;i++) {
			         if (playing == TRUE && connection->players_array[i].mode[1] == 'l' && connection->players_array[i].power == 1 ) {
	                                trace("Find Playing Player(%s)",connection->players_array[i].name);
				        pl = &connection->players_array[i];
					break;
			         } else if ( playing == FALSE && connection->players_array[i].power == 1 ) {
	                                trace("Find Player(%s)",connection->players_array[i].name);
				 	pl = &connection->players_array[i];
					break;
				 } 
			     }
			break;
			case '!': //Ignore if not playing
			     playing = TRUE;
			     p++;
			default:
			   for (i=0;i < connection->players;i++) {
			       if (strcmp(connection->players_array[i].name,p) == 0 || strcmp(connection->players_array[i].id,p) == 0 ) {
			          if (playing == TRUE && connection->players_array[i].mode[1] != 'l') {
				     continue;
				  }

			          pl = &connection->players_array[i];
	                          trace("Nailed Player(%s)",connection->players_array[i].name);
				  break;

			       }
			   }
			break;
		}

		// Restore and setup next segment
       		if (d != NULL) {
           	    *d = ',';
                    p = d+1;
		}
        } while(d != NULL && pl == NULL);
	
	if ( pl == NULL ) {
	 pl = &connection->players_array[connection->players-1];
	 trace("Last Player(%s) %s.",connection->players_array[connection->players-1].name,players);
	}
	return pl;
}

/******************************************************************************/

void squeezecenter_status_to_musictracker(squeezecenter_Player *pl,struct TrackInfo* ti) {
	static char name[STRLEN];

	sprintf(name,"SqueezeCenter(%s)",pl->name);
	trim(name);
	ti->player = name;
	ti->currentSecs = pl->time;
	ti->status = STATUS_OFF;
        if (pl->remote == 1) {
	        trace("squeezecenter remote streaming");
		g_strlcpy(ti->track,pl->current_title,STRLEN);
		// set others to null?
		ti->totalSecs = -1;
	} else {
		g_strlcpy(ti->track,pl->tinfo.title,STRLEN);
		g_strlcpy(ti->artist,pl->tinfo.artist,STRLEN);
		g_strlcpy(ti->album,pl->tinfo.album,STRLEN);
		ti->totalSecs = pl->duration;
	}

	if ( pl->power == 1 || pl->remote == 1) {
	   trace("squeezecenter player on");
	   switch(pl->mode[1]) {
	      case 'l': //Play
		ti->status = STATUS_NORMAL;
	      break;
	      case 'a': //Pause
		ti->status = STATUS_PAUSED;
	      break;
	      case 't': //stop
		ti->status = STATUS_OFF;
	      break;
	   }
	} else {
		ti->status = STATUS_OFF;
	}
        trace("squeezecenter musictracker status %d(%c)",ti->status,pl->mode[1]);
}

/* music tracker interfaces */

gboolean get_squeezecenter_info(struct TrackInfo* ti)
{
	char const * server = purple_prefs_get_string(PREF_SQUEEZECENTER_SERVER);
	char const * user = purple_prefs_get_string(PREF_SQUEEZECENTER_USER);
	char const * password = purple_prefs_get_string(PREF_SQUEEZECENTER_PASSWORD);
	char const * players = purple_prefs_get_string(PREF_SQUEEZECENTER_PLAYERS);
	int last_players,ret  = 0;
	static int last = 0;
	squeezecenter_Player *pl;
	

	trace("squeezecenter enter");

	if(server == NULL)		
		server = "localhost:9090";
        if(players == NULL)
	        players = "#";
	if(user == NULL || password == NULL) {
	   user ="";
	   password = "";
	}

        //conn  chreck

	if(!get_squeezecenter_connection(&squeezecenter,server,SQUEEZECENTER_TIMEOUT,&last)) {
          return FALSE;
        }

	if(squeezecenter.sock <= 0) {
	   return FALSE;
	}

	switch(ret = squeezecenter_connected(&squeezecenter)) {
	   case -1: //error
	      trace("squeezecenter connection error");
	      squeezecenter_disconnect(&squeezecenter);
	      return FALSE;
           break;
	   case 0: //peding
	      trace("squeezecenter connection pending");
	      return FALSE;
	   break;
	   default:
	      trace("squeezecenter connected (%d)",ret);
	   break;
	}
	if (squeezecenter.version[0] == '\0') {
	   trace("squeezecenter preamble");
	   if( squeezecenter_connection_preamble(&squeezecenter,user,password) != TRUE) {
	       trace("squeezecenter preamble user/passord fail");
	       return FALSE; 
	   }
	}

        last_players = squeezecenter.players;

        if(!squeezecenter_get_player_count(&squeezecenter)) {
	        trace("squeezecenter player count failed");
	        squeezecenter_disconnect(&squeezecenter);
		return FALSE;
	}

	if(squeezecenter.players <=0) {
	        trace("squeezecenter no players");
		return FALSE;
	}

	if((!squeezecenter.players_array) || last_players != squeezecenter.players ) {
	        trace("squeezecenter no player change (%d)",squeezecenter.players);
	        squeezecenter_get_players(&squeezecenter);
	}

        int i;
        /* poll all players! */
	trace("squeezecenter poll all players");
        for (i = 0;i < squeezecenter.players;i++) {
	        trace("squeezecenter status poll (%s) \"%s\"",squeezecenter.players_array[i].id,squeezecenter.players_array[i].name);
		squeezecenter_get_player_current_status(&squeezecenter, &squeezecenter.players_array[i],squeezecenter.players_array[i].id);
	}
        pl = get_squeezecenter_status(&squeezecenter ,players);
	trace("squeezecenter musictracker mash");
        squeezecenter_status_to_musictracker(pl,ti);
	trace("squeezecenter exit");

        return TRUE;

}

void cb_squeezecenter_changed(GtkWidget *entry, gpointer data)
{
	const char *pref = (const char*) data;
	purple_prefs_set_string(pref, gtk_entry_get_text(GTK_ENTRY(entry)));

	if (strcmp(PREF_SQUEEZECENTER_SERVER,pref) == 0) {
		squeezecenter_disconnect(&squeezecenter);
	}
}

void get_squeezecenter_pref(GtkBox *vbox)
{
	GtkWidget *entry, *hbox, *label;

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(_("Host:CliPort, Servers:")), FALSE, FALSE, 0);
	entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), purple_prefs_get_string(PREF_SQUEEZECENTER_SERVER));
	gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(cb_squeezecenter_changed), (gpointer) PREF_SQUEEZECENTER_SERVER);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(_("Player Order:")), FALSE, FALSE, 0);
	entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), purple_prefs_get_string(PREF_SQUEEZECENTER_PLAYERS));
	gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(cb_squeezecenter_changed), (gpointer) PREF_SQUEEZECENTER_PLAYERS);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(_("CLI User:")), FALSE, FALSE, 0);
	entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), purple_prefs_get_string(PREF_SQUEEZECENTER_USER));
	gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(cb_squeezecenter_changed), (gpointer) PREF_SQUEEZECENTER_USER);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(_("CLI Password:")), FALSE, FALSE, 0);
	entry = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
	gtk_entry_set_text(GTK_ENTRY(entry), purple_prefs_get_string(PREF_SQUEEZECENTER_PASSWORD));
	gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(cb_squeezecenter_changed), (gpointer) PREF_SQUEEZECENTER_PASSWORD);


        label = gtk_label_new(SQUEEZECENTER_ADVICE);
        gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
        gtk_box_pack_start(vbox, label, TRUE, TRUE, 0);

        label = gtk_label_new(_("Server:"));
        gtk_box_pack_start(vbox, label, TRUE, TRUE, 0);
        label = gtk_label_new(squeezecenter.server);
        gtk_box_pack_start(vbox, label, TRUE, TRUE, 0);

        label = gtk_label_new(_("Squeezecenter Version:"));
        gtk_box_pack_start(vbox, label, TRUE, TRUE, 0);
        label = gtk_label_new(squeezecenter.version);
        gtk_box_pack_start(vbox, label, TRUE, TRUE, 0);

        label = gtk_label_new(_("Players:"));
        gtk_box_pack_start(vbox, label, TRUE, TRUE, 0);
        char players[SQUEEZECENTER_BUFFER_LENGTH];
        snprintf(players,SQUEEZECENTER_BUFFER_LENGTH,_("Player count: %d\n"),squeezecenter.players);
	int i;
	for(i = 0; i < squeezecenter.players;i++) {
	   int len = strlen(players);
	   snprintf(players+len,SQUEEZECENTER_BUFFER_LENGTH-len,"\"%s\" id: %s\n",squeezecenter.players_array[i].name,squeezecenter.players_array[i].id);
	}
        label = gtk_label_new(players);
        gtk_box_pack_start(vbox, label, TRUE, TRUE, 0);

        label = gtk_label_new(_("Last Command:"));
        gtk_box_pack_start(vbox, label, TRUE, TRUE, 0);
        label = gtk_label_new(squeezecenter.command);
        gtk_box_pack_start(vbox, label, TRUE, TRUE, 0);

        label = gtk_label_new(_("Last Reply:"));
        gtk_box_pack_start(vbox, label, TRUE, TRUE, 0);
        label = gtk_label_new(squeezecenter.buffer);
        gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
        gtk_box_pack_start(vbox, label, TRUE, TRUE, 0);

	label = gtk_label_new(_("Last Error:"));
        gtk_box_pack_start(vbox, label, TRUE, TRUE, 0);
        label = gtk_label_new(squeezecenter.errorStr);
        gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
        gtk_box_pack_start(vbox, label, TRUE, TRUE, 0);
}

