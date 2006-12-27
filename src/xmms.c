#include "musictracker.h"
#include "utils.h"

gboolean
get_xmms_info(struct TrackInfo* ti)
{
	strcpy(ti->player, "XMMS");
	FILE *file = fopen("/tmp/xmms-info", "r");
	if (!file) {
		trace("File /tmp/xmms-info failed to open. Assuming XMMS is off.");
		ti->status = STATUS_OFF;
		return TRUE;
	}

	while (1) {
		char buf[STRLEN*5];
		int b = readline(file, buf, STRLEN*5);
		if (!b) {
			trace("Done reading /tmp/xmms-info.");
			return TRUE;
		}

		const char *state = parse_value(buf, "Status");
		if (state) {
			trace("Got state %s", state);
			if (strcmp(state, "Playing") == 0) 
				ti->status = STATUS_NORMAL;
		   	else if (strcmp(state, "Paused") == 0)
				ti->status = STATUS_PAUSED;
			else {
				ti->status = STATUS_OFF;
				return TRUE;
			}
		}

		const char* title = parse_value(buf, "Title");
		if (title) {
			trace("Got title %s", title);
			const char *sep = gaim_prefs_get_string("/plugins/core/musictracker/string_xmms_sep");
			assert(strlen(sep) == 1);
			char fmt[100];
			sprintf(fmt, "%%[^%s]%s%%[^%s]%s%%[^%s]", sep, sep, sep, sep, sep);
			if (!sscanf(title, fmt, ti->artist, ti->album, ti->track) == 3) {
				trace("XMMS Status in unparsable format");
				return FALSE;
			}
		}

		const char* time = parse_value(buf, "Time");
		if (time) {
			int mins, secs;
			trace("Got duration %s", title);
			if (sscanf(time, "%d:%d", &mins, &secs))
				ti->totalSecs = mins*60 + secs;
		}

		const char* position = parse_value(buf, "Postition");
		if (position) {
			int mins, secs;
			trace("Got position %s", position);
			if (sscanf(position, "%d:%d", &mins, &secs))
				ti->currentSecs = mins*60 + secs;
		}
	}
	return TRUE;
}
