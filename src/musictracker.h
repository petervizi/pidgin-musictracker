#ifndef _MUSICTRACKER_H_
#define _MUSICTRACKER_H_

#include <assert.h>
#include <glib.h>
#include "prefs.h"

#define PLUGIN_ID "core-musictracker"
#define STRLEN 100

#define STATUS_OFF 0
#define STATUS_PAUSED 1
#define STATUS_NORMAL 2
#define PLAYER_XMMS 0
#define PLAYER_AMAROK 1
#define PLAYER_EXAILE 2
#define PLAYER_AUDACIOUS 3
#define PLAYER_BMP 4

struct TrackInfo
{
        char track[STRLEN];
        char artist[STRLEN];
        char album[STRLEN];
        char player[STRLEN];
        int status;
		int totalSecs;
		int currentSecs;
};

gboolean get_amarok_info(struct TrackInfo* ti);
gboolean get_xmms_info(struct TrackInfo* ti);
gboolean get_exaile_info(struct TrackInfo* ti);
gboolean get_xmmsctrs_info(struct TrackInfo* ti);

#endif // _MUSICTRACKER_H_
