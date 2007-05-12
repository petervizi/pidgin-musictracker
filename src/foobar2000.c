#include "musictracker.h"
#include "utils.h"
#include <windows.h>
#include <commctrl.h>

get_foobar2000_info(struct TrackInfo* ti)
{
	HWND mainWindow = FindWindow("{DA7CD0DE-1602-45e6-89A1-C2CA151E008E}/1", NULL);
	if (!mainWindow)
		FindWindow("{DA7CD0DE-1602-45e6-89A1-C2CA151E008E}", NULL);
	if (!mainWindow) {
		trace("Failed to find foobar2000 window. Assuming player is off");
		ti->status = STATUS_OFF;
		return TRUE;
	}

	/*
	HWND statusWindow = FindWindowEx(mainWindow, NULL, STATUSCLASSNAME, NULL);
	if (!statusWindow) {
		trace("Error: Failed to find foobar2000 status window");
		return FALSE;
	}
	*/

	char title[STRLEN*5], status[200];
	GetWindowText(mainWindow, title, STRLEN*5);
	trace("Got window title: %s", title);
	//SendMessage(statusWindow, SB_GETTEXT, (WPARAM) 0, (LPARAM) status);
	//trace("Got window status: %s", status);
	
	if (strncmp(title, "foobar2000", 10) == 0) {
		ti->status = STATUS_OFF;
		return TRUE;
	}
	ti->status = STATUS_NORMAL;

	static pcre *re;
	if (!re)
		re = regex("(.*) - \\[([^#]+)[^\\]]+\\] (.*) \\[foobar2000.*\\]", 0);
	capture(re, title, strlen(title), ti->artist, ti->album, ti->track);
	return TRUE;
}
