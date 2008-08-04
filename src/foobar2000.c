#include "musictracker.h"
#include "utils.h"
#include <windows.h>

get_foobar2000_info(struct TrackInfo* ti)
{
        // very brittle way of finding the foobar2000 window...
	HWND mainWindow = FindWindow("{DA7CD0DE-1602-45e6-89A1-C2CA151E008E}/1", NULL); // Foobar 0.9.1
	if (!mainWindow)
		mainWindow = FindWindow("{DA7CD0DE-1602-45e6-89A1-C2CA151E008E}", NULL);
	if (!mainWindow)
		mainWindow = FindWindow("{97E27FAA-C0B3-4b8e-A693-ED7881E99FC1}", NULL); // Foobar 0.9.5.3 
	if (!mainWindow) {
		trace("Failed to find foobar2000 window. Assuming player is off");
		ti->status = STATUS_OFF;
		return TRUE;
	}

        int title_length = GetWindowTextLength(mainWindow)+1;
	char title[title_length];
	GetWindowText(mainWindow, title, title_length);
	trace("Got window title: %s", title);

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
