#include "musictracker.h"
#include "utils.h"

gboolean
dcop_query(const char* command, char *dest, int len)
{
	FILE* pipe = popen(command, "r");
	if (!pipe) {
		trace("Failed to open pipe");
		return FALSE;
	}

	if (!readline(pipe, dest, len)) {
		dest[0] = 0;
	}
	pclose(pipe);
	return TRUE;
}

gboolean
get_amarok_info(struct TrackInfo* ti)
{
	char status[10];

	if (!dcop_query("dcop amarok default status 2>/dev/null", status, STRLEN)) {
		trace("Failed to run dcop. Assuming off state.");
		ti->status = STATUS_OFF;
		return TRUE;
	}

        trace ("dcop returned status '%s'", status);

	sscanf(status, "%d", &ti->status);
	if (ti->status != STATUS_OFF) {
		int mins, secs;
		char time[STRLEN];
		trace("Got valid dcop status... retrieving song info");
		dcop_query("dcop amarok default artist", ti->artist, STRLEN);
		dcop_query("dcop amarok default album", ti->album, STRLEN);
		dcop_query("dcop amarok default title", ti->track, STRLEN);

		dcop_query("dcop amarok default totalTime", time, STRLEN);
		if (sscanf(time, "%d:%d", &mins, &secs))
			ti->totalSecs = mins*60 + secs;
		dcop_query("dcop amarok default currentTime", time, STRLEN);
		if (sscanf(time, "%d:%d", &mins, &secs))
			ti->currentSecs = mins*60 + secs;
		
	}
	return TRUE;
}
