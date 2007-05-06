#include <glib.h>
#include <string.h>
#include "filter.h"
#include "musictracker.h"

/* Filters out all black-listed words, as well as non-printable characters
 */
void
filter(char *str)
{
	char *stri = g_utf8_casefold(str, -1);
	char mask = *purple_prefs_get_string(PREF_MASK);

	char **list = g_strsplit(purple_prefs_get_string(PREF_FILTER),
			",", 0);
	char **p;
	for (p = list; *p != 0; ++p) {
		char *word = g_utf8_casefold(*p, -1);

		char *match = str, *matchi = stri;
		int len = strlen(word);
		if (len == 0)
			continue;
		
		while (matchi = strstr(matchi, word)) {
			int i;
			match = str + (matchi-stri);
			for (i=0; i<len; ++i)
				match[i] = mask;
			match += len;
			matchi += len;
		}

		g_free(word);
	}
	g_strfreev(list);
	g_free(stri);
	
	// Remove unprintable chars
	for (; *str != 0; ++str) {
		if (!isprint(*str))
			*str = mask;
	}
}

const char*
filter_get_default()
{
	return "fuck,suck,ass,bitch,dick,penis,porn,motherfucker";
}
