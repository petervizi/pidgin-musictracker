#include <glib.h>
#include <string.h>
#include <ctype.h>
#include "filter.h"
#include "utils.h"
#include "musictracker.h"

/* Filters out all black-listed words, as well as non-printable characters
 */
void
filter(char *str)
{
        gboolean changed = FALSE;

        // split mask as a comma separated list of words
	char mask = *purple_prefs_get_string(PREF_MASK);
	char **list = g_strsplit(purple_prefs_get_string(PREF_FILTER),
			",", 0);


	char **p;
	for (p = list; *p != 0; ++p) {
                int len = strlen(*p);
                if (len == 0)
                  continue;

                // construct a regex pattern which matches the distinct word
                // i.e. use word boundaries to avoid matchin as a substring...
                char pattern[len+10];
                sprintf(pattern, "\\b(%s)\\b", *p);
                pcre *re = regex(pattern, PCRE_CASELESS | PCRE_UTF8);

                int ovector[6];
                while (pcre_exec(re, 0, str, strlen(str), 0, 0, ovector, 6) > 0)
                  {
                    for (int i = ovector[2]; i < ovector[3]; i++)
                      str[i] = mask;
                    changed = TRUE;
                  }

                pcre_free(re);                
	}
	g_strfreev(list);

	// Remove unprintable chars
        char *u = str;
        while (*u != 0)
          {
            gunichar c = g_utf8_get_char(u);
            gchar *next = g_utf8_next_char(u);
            if (!g_unichar_isprint(c))
              {
                for (; u < next; u++)  
                  *u = mask;
                changed = TRUE;
              }
            u = next;
          }

        // debug report that filtering happened
        if (changed)
          {
              trace("filtered to: %s", str);
          }
}

const char*
filter_get_default()
{
	return "fuck,suck,ass,bitch,dick,penis,porn,motherfucker";
}
