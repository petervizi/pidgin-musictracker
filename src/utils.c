#include "debug.h"
#include "utils.h"
#include "musictracker.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <assert.h>

/* Trace a debugging message. Writes to log file as well as purple
 * debug sink.
 */
void
trace(char *str, ...)
{
	char buf[500], buf2[500];
	va_list ap;
	va_start(ap, str);
	vsnprintf(buf, 500, str, ap);
	va_end(ap);

	gboolean logging = purple_prefs_get_bool("/plugins/core/musictracker/bool_log");
	if (logging) {
#ifndef WIN32
		FILE *log = fopen("/tmp/musictracker.log", "a");
#else
		FILE *log = fopen("musictracker.log", "a");
#endif
		assert(log);
		time_t t;
		time(&t);
#ifndef WIN32
		ctime_r(&t, buf2);
		buf2[strlen(buf2)-1] = 0;
		fprintf(log, "%s: %s\n", buf2, buf);
#else
		fprintf(log, "%s: %s\n", ctime(&t), buf);
#endif
		fclose(log);
	}

	purple_debug_info(PLUGIN_ID, "%s\n", buf);
}

//--------------------------------------------------------------------

/* Reads a single line from a file to a string, without any trailing
 * newline character. Returns non-zero on success.
 */
int
readline(FILE* file, char *buf, int len)
{
	if (feof(file))
		return 0;
	fgets(buf, len, file);
	len = strlen(buf);
	if (len == 0)
		return 0;

	if (buf[len-1] == '\n') {
		buf[len-1] = '\0';
	} else
		return 0;
	return 1;
}


//--------------------------------------------------------------------

/* Looks for a "key: value" pair in the give string line and returns
 * a pointer to the value string if found.
 */
const char *
parse_value(const char *line, const char* key)
{
	const char *p = line;
	while (*p != 0 && *p != ':')
		++p;
	if (*p == 0 || *(p+1) == 0 || *(p+1) != ' ' || *(p+2) == 0)
		return 0;

	if (strncmp(line, key, p-line) == 0)
		return p+2;
	else
		return 0;
}

//--------------------------------------------------------------------

/* Replaces all instances of %<indentifier> in the giving string
 * buffer with the given field. De-allocates the given buffer,
 * assuming it was allocated by malloc and allocates a new buffer
 * for the returned result.
 */
char *
put_field(char *buf, char identifier, const char *field) 
{
	int len = strlen(field), len2 = strlen(buf), i, j;
	char *out;

	int count = 0;
	for (i=0; i+1<len2; ++i) {
		if (buf[i] == '%' && buf[i+1] == identifier) {
			count += len;
			++i;
		} else
			++count;
	}
	++count;

	out = malloc(count+1);
	j = 0;
	for (i=0; i+1<len2; ++i) {
		if (buf[i] == '%' && buf[i+1] == identifier) {
			out[j] = 0;
			strcat(out, field);
			++i;
			j += len;
		} else
			out[j++] = buf[i];
	}
	out[j++] = buf[i];
	assert(j == count);
	out[j] = 0;

	free(buf);
	return out;
}

//--------------------------------------------------------------------

/* Remove leading and trailing spaces from a string.
 */
void
trim(char *buf)
{
	char *tmp = malloc(strlen(buf)+1);

	char *p = buf, *q = tmp;
	while (*p == ' ') ++p;
	while (*p != 0) {
		*q = *p;
		++p;
		++q;
	}

	*q = 0;
	--q;
	while (*q == ' ') {
		*q = 0;
		--q;
	}

	strcpy(buf, tmp);
	free(tmp);
}

//--------------------------------------------------------------------

/* Shortcut to compile a perl-compatible regular expression
 */
pcre* regex(const char *pattern, int options)
{
	const char *err;
	int erroffset;
	pcre* re = pcre_compile(pattern, options, &err, &erroffset, 0);
	if (!re) {
		sprintf(stderr, "Failed to parse regular expression: %s\n", err);
		exit(1);
	}
	return re;
}

//--------------------------------------------------------------------

/* Captures substrings from given text using regular expr, and copies
 * them in order to given destinations. Returns no of subtrings copied
 */
int capture(pcre* re, const char* text, int len, ...)
{
	int ovector[20], i;
	va_list ap;
	int count = pcre_exec(re, 0, text, len, 0, 0, ovector, 20);

	va_start(ap, len);
	for (i=1; i<count; ++i) {
		char *dest = va_arg(ap, char*);
		strncpy(dest, text+ovector[i*2], ovector[i*2+1] - ovector[i*2]);
		dest[ovector[i*2+1] - ovector[i*2]] = 0;
	}
	va_end(ap);
	return count-1;
}

//--------------------------------------------------------------------

/* Returns true if the given dbus service is running. To avoid starting
 * a program (a media player) that isn't running.
 */

#ifndef WIN32
gboolean dbus_g_running(DBusGConnection *connection, const char *name)
{
	DBusGProxy *dbus;
	GError *error = 0;
	gboolean running;

	trace(name);
	dbus = dbus_g_proxy_new_for_name(connection,
			"org.freedesktop.DBus",
			"/org/freedesktop/DBus",
			"org.freedesktop.DBus");
	trace("proxy");
	dbus_g_proxy_call(dbus, "NameHasOwner", &error,
			G_TYPE_STRING, name,
			G_TYPE_INVALID,
			G_TYPE_BOOLEAN, &running,
			G_TYPE_INVALID);
	trace("call");
	return running;
}
#endif
			
//--------------------------------------------------------------------

/* Builds a preference string according to the given format, taking
 * care to strip forward slashes which have special meaning
 */

void build_pref(char *dest, const char *format, const char* str)
{
	char buf[STRLEN];
	int i=0, j=0, len;
	len = strlen(str);
	for (i=0; i<len; ++i) {
		if (str[i] != '/')
			buf[j++] = str[i];
	}
	buf[j] = 0;
	sprintf(dest, format, buf);
}

