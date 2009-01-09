//
// musictracker
// moc.c
// retrieve track info from MOC player
//
// Copyright (c) 2008 Peter Vizi <peter.vizi@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

/**
 * @file moc.c
 *
 * @author peter.vizi@gmail.com
 *
 * Support for the MOC audio player, http://moc.daper.net/
 */

#include "musictracker.h"
#include "utils.h"

#include <string.h>

/**
 * This is the size of the buffer where the response from mocp is
 * stored.
 */
#define BUFF_SIZE 256

/**
 * Get player information.
 *
 * @param ti fill with info
 *
 * @return @c FALSE if mocp could not be started, @c TRUE otherwise
 */
gboolean get_moc_info(struct TrackInfo* ti) {
  char* pch; // used for tokenizing
  char temp[BUFF_SIZE]; // store response from mocp
  char* ret;
  FILE* pipe = popen("mocp -Q '%song ;%artist ;%album ;%state;%ts ;%cs ;%file ; '", "r");

  if (!pipe) {
    trace("No mocp");
    return FALSE;
  }

  ret = fgets(temp, BUFF_SIZE, pipe);
  pclose(pipe);

  if (ret == NULL) {
    trace("Error with pipe");
    return FALSE;
  }
  trace("mocp -Q returned '%s'", temp);

  pch = strtok(temp, ";"); // song
  if (pch != NULL) {
    strcpy(ti->track, pch);
  } else {
    strcpy(ti->track, "");
  }
  pch = strtok(NULL, ";"); // artist
  if (pch != NULL) {
    strcpy(ti->artist, pch);
  } else {
    strcpy(ti->artist, "");
  }
  pch = strtok(NULL, ";"); // ablum
  if (pch != NULL) {
    strcpy(ti->album, pch);
  } else {
    strcpy(ti->album, "");
  }
  pch = strtok(NULL, ";"); // state
  if (pch != NULL) {
    if (strcmp(pch, "STOP") == 0) {
      ti->status = STATUS_OFF;
    } else if (strcmp(pch, "PLAY") == 0) {
      ti->status = STATUS_NORMAL;
    } else if (strcmp(pch, "PAUSED") == 0) {
      ti->status = STATUS_PAUSED;
    } else {
      ti->status = STATUS_OFF;
    }
  } else {
    ti->status = STATUS_OFF;
  }
  pch = strtok(NULL, ";"); // ts
  if (pch != NULL) {
    ti->totalSecs = atoi(pch);
  } else {
    ti->totalSecs = 0;
  }
  pch = strtok(NULL, ";"); // cs
  if (pch != NULL) {
    ti->currentSecs = atoi(pch);
  } else {
    ti->currentSecs = 0;
  }
  // Check for internet radio: when the user listens to the radio
  // format in a different way
  pch = strtok(NULL, ";"); // file
  if ((pch != NULL) && (strcmp(ti->album, " ") == 0 && strcmp(ti->artist, " ") == 0)) {
    if (strstr(pch, "http://") != NULL) {
      strcpy(ti->artist, pch);
      strcpy(ti->album, "Online Radio");
      ti->totalSecs = ti->currentSecs;
    }
  }
  return TRUE;
}

// -*- tab-width: 2; -*-
