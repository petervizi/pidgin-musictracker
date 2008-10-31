/*
 * musictracker
 * msn-compat.c
 * retrieve track info being sent to MSN
 *
 * Copyright (C) 2008, Jon TURNEY <jon.turney@dronecode.org.uk>
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

#include <windows.h>
#include <musictracker.h>
#include <utils.h>

// put up a hidden window called "MsnMsgrUIManager", which things know to send a WM_COPYDATA message to, 
// containing strangely formatted string, for setting the now-playing status of MSN Messenger
// this might be broken by the sending side only sending to the first window of the class "MsnMsgrUIManager"
// it finds if there are more than one (for e.g., if the real MSN Messenger is running as well :D)
//
// Useful description of the message format: http://kentie.net/article/nowplaying/index.htm
// although we rely on the fact that players generate a very limited subset of the possible
// messages to be able to parse the message back into the track details...
//
// Winamp seems to generate messages with enabled=1 but all the fields empty when it stops, so we need to
// check if any fields have non-empty values as well as looking at that flag
//

static struct TrackInfo msnti;

static
void process_message(wchar_t *MSNTitle)
{
  char *s = wchar_to_utf8(MSNTitle);
  static char player[STRLEN];
  char enabled[STRLEN], format[STRLEN], title[STRLEN], artist[STRLEN], album[STRLEN], uuid[STRLEN];
  
  // this has to be escaped quite carefully to prevent literals being interpreted as metacharacters by the compiler or in the pcre pattern
  // so yes, four \ before a 0 is required to match a literal \0 in the regex :-)
  // and marking the regex as ungreedy is a lot easier than writing \\\\0([^\\\\0]*)\\\\0 :)
  pcre *re1 = regex("^(.*)\\\\0Music\\\\0(.*)\\\\0(.*)\\\\0(.*)\\\\0(.*)\\\\0(.*)\\\\0(.*)\\\\0", PCRE_UNGREEDY);
  pcre *re2 = regex("^(.*)\\\\0Music\\\\0(.*)\\\\0(.*) - (.*)\\\\0$", 0);
  if (capture(re1, s, strlen(s), player, enabled, format, title, artist, album, uuid) > 0)
    {
      trace("player '%s', enabled '%s', format '%s', title '%s', artist '%s', album '%s', uuid '%s'", player, enabled, format, title, artist, album, uuid);

      if ((strncmp(enabled, "1", 1) == 0) &&
          ((strlen(artist) > 0) || (strlen(title) > 0) || (strlen(album) > 0)))
        {
          msnti.player = player;
          msnti.status = STATUS_NORMAL;
          strncpy(msnti.artist, artist, STRLEN);
          msnti.artist[STRLEN-1] = 0;
          strncpy(msnti.album, album, STRLEN);
          msnti.album[STRLEN-1] = 0;
          strncpy(msnti.track, title, STRLEN);
          msnti.track[STRLEN-1] = 0;
        }
      else
        {
          msnti.status = STATUS_OFF;
        }
    }
  else if (capture(re2, s, strlen(s), player, enabled, artist, title) > 0)
    {
      trace("player '%s', enabled '%s', artist '%s', title '%s'", player, enabled, artist, title);

      if ((strncmp(enabled, "1", 1) == 0) &&
          ((strlen(artist) > 0) || (strlen(title) > 0)))
        {
          msnti.player = player;
          msnti.status = STATUS_NORMAL;
          strncpy(msnti.artist, artist, STRLEN);
          msnti.artist[STRLEN-1] = 0;
          msnti.album[0] = 0;
          strncpy(msnti.track, title, STRLEN);
          msnti.track[STRLEN-1] = 0;
        }
      else
        {
          msnti.status = STATUS_OFF;
        }
    }
  else
    {
      msnti.status = STATUS_OFF;
    }

  pcre_free(re1);
  pcre_free(re2);

  if ((msnti.player == 0) || (strlen(msnti.player) == 0))
    {
      msnti.player = "Unknown";
    }

  free(s);
}

static
LRESULT CALLBACK MSNWinProc(
                            HWND hwnd,      // handle to window
                            UINT uMsg,      // message identifier
                            WPARAM wParam,  // first message parameter
                            LPARAM lParam   // second message parameter
                            )
{
  switch(uMsg) {
  case WM_COPYDATA:
    {
      wchar_t MSNTitle[2048];
      COPYDATASTRUCT *cds = (COPYDATASTRUCT *) lParam;
      CopyMemory(MSNTitle,cds->lpData,cds->cbData);
      MSNTitle[2047]=0;
      process_message(MSNTitle);
      return TRUE;
    }
  default:
    return DefWindowProc(hwnd,uMsg,wParam,lParam);
  }
}

gboolean get_msn_compat_info(struct TrackInfo *ti)
{
  static HWND MSNWindow = 0;
  
  if (!MSNWindow)
    {
      memset(&msnti, 0, sizeof(struct TrackInfo));

      WNDCLASSEX MSNClass = {sizeof(WNDCLASSEX),0,MSNWinProc,0,0,GetModuleHandle(NULL),NULL,NULL,NULL,NULL,"MsnMsgrUIManager",NULL};
      ATOM a = RegisterClassEx(&MSNClass);
      trace("RegisterClassEx returned 0x%x",a);
  
      MSNWindow = CreateWindowEx(0,"MsnMsgrUIManager","MSN message compatibility window for pidgin-musictracker",
                                      0,0,0,0,0,
                                      HWND_MESSAGE,NULL,GetModuleHandle(NULL),NULL);
      trace("CreateWindowEx returned 0x%x", MSNWindow);

      // Alternatively could use setWindowLong() to override WndProc for this window ?
    }

  // did we receive a message with something useful in it?
  if (msnti.status == STATUS_NORMAL)
    {
      *ti = msnti;
      return TRUE;
    }
  
  return FALSE;
}

// XXX: cleanup needed on musictracker unload?
// XXX: we've also heard tell that HKEY_CURRENT_USER\Software\Microsoft\MediaPlayer\CurrentMetadata has been used to pass this data....
