/*
 * musictracker
 * wmp.c
 * retrieve track info from WMP (using WMPuICE for remote access
 * to IDispatch of an already running WMP)
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

#include "musictracker.h"
#include "utils.h"
#include <windows.h>
#include "disphelper.h"

typedef enum WMPPlayState{
  wmppsUndefined     = 0,
  wmppsStopped       = 1,
  wmppsPaused        = 2,
  wmppsPlaying       = 3,
  wmppsScanForward   = 4,
  wmppsScanReverse   = 5,
  wmppsBuffering     = 6,
  wmppsWaiting       = 7,
  wmppsMediaEnded    = 8,
  wmppsTransitioning = 9,
  wmppsReady         = 10,
  wmppsReconnecting  = 11,
} WMPPlayState;

static
void getItemInfo(IDispatch *objWmp, WCHAR *attr, char *result)
{
  BSTR value;
  BSTR attribute = SysAllocString(attr);
  HRESULT r = dhGetValue(L"%B", &value, objWmp, L".currentMedia.getItemInfo(%B)", attribute);
  SysFreeString(attribute);
  if (r == 0)
    {
      char *v = wchar_to_utf8(value);
      strncpy(result, v, STRLEN);
      result[STRLEN-1] = 0;
      dhFreeString(value);
      free(v);
    }
}

gboolean get_wmp_info(struct TrackInfo *ti)
{
  HRESULT r;
  DISPATCH_OBJ(objWmpuice);
  DISPATCH_OBJ(objWmp);
  
  dhInitialize(TRUE);
  // dhToggleExceptions(TRUE); // for debugging

  r = dhCreateObject(L"WMPuICE.WMPApp", NULL, &objWmpuice);
  if (r != 0)
    {
      trace("Could not create WMPuICE object (WMPuICE not installed?)");
      return FALSE;
    }

  r = dhGetValue(L"%o", &objWmp, objWmpuice, L".Core");
  if (r != 0)
    {
      SAFE_RELEASE(objWmpuice);
      trace("No WMP core IDispatch (WMP not running?)");
      return FALSE;
    }

  // playState
  INT state;
  r = dhGetValue(L"%d", &state, objWmp, L".playState");
  trace("playState: %d", state);
  if (r == 0)
    {
      switch (state)
        {
        case wmppsPlaying:
        case wmppsScanForward:
        case wmppsScanReverse:
          ti->status = STATUS_NORMAL;
          break;

        case wmppsPaused:
          ti->status = STATUS_PAUSED;
          break;

        case wmppsStopped:
        case wmppsMediaEnded:
        case wmppsReady:
        default:
          ti->status = STATUS_OFF;
        }
    }

  BSTR version;
  r = dhGetValue(L"%B", &version, objWmp, L".versionInfo");
  if (r == 0)
    {
      char *ver = wchar_to_utf8(version);
      trace("WMP version: %s", ver);
      free(ver);
      dhFreeString(version);
    }

  // .currentMedia.getItemInfo()
  getItemInfo(objWmp, L"WM/AlbumTitle", ti->album);
  getItemInfo(objWmp, L"Author", ti->artist);
  getItemInfo(objWmp, L"Title", ti->track);

  // currentMedia.duration
  double duration;
  r= dhGetValue(L"%e", &duration, objWmp, L".currentMedia.duration");
  if (r == 0)
    {
      trace("duration = %f", duration);
      ti->totalSecs = duration;
    }

  // controls.currentPosition
  double position;
  r = dhGetValue(L"%e", &position, objWmp, L".controls.currentPosition");
  if (r == 0)
    {
      trace("position = %f", position);
      ti->currentSecs = position;
    }

  SAFE_RELEASE(objWmp);
  SAFE_RELEASE(objWmpuice);
  dhUninitialize(TRUE);

  return TRUE;
}
