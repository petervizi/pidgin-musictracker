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
trace(const char *str, ...)
{
	char buf[500];
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
		fprintf(log, "%s: %s\n", ctime(&t), buf);
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
	buf[0] = '\0';
	if (feof(file))
		return 0;
	if (fgets(buf, len, file) == NULL)
		return 0;
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
	while ((q >= tmp) && (*q == ' ')) {
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
		fprintf(stderr, "Failed to parse regular expression: %s\n", err);
		exit(1);
	}
        trace("pcre_compile: regex '%s'",pattern);
	return re;
}

//--------------------------------------------------------------------

/* Captures substrings from given text using regular expr, and copies
 * them in order to given destinations. Returns no of subtrings copied
 *
 */

int capture(pcre* re, const char* text, int len, ...)
{
	int i;
	va_list ap;
        int capture_count;
        int result;

        result = pcre_fullinfo(re, NULL, PCRE_INFO_CAPTURECOUNT, &capture_count);
        if (result)
          {
            trace("pcre_fullinfo: failed %d", result);
            return -1;
          }

        /* 
           Make ovector an appropriate size, given that the first 2/3 of ovector
           is used to return pairs of capture string offsets (starting with the
           entire pattern), the rest is internal workspace, we need 3/2 * 2 * 
           (capture_count+1)...
        */
        int ovecsize = (capture_count+1)*3;
        int ovector[ovecsize];
	int count = pcre_exec(re, 0, text, len, 0, 0, ovector, ovecsize);
        trace("pcre_exec: returned %d", count);

	va_start(ap, len);
	for (i=1; i<count; ++i) {
		char *dest = va_arg(ap, char*);
                int length = ovector[i*2+1] - ovector[i*2];
                // XXX: clunky check that matched substring is shorter than the buffer we've preallocated for it...
                if (length > (STRLEN-1)) { length = STRLEN-1; }
		strncpy(dest, text+ovector[i*2], length);
		dest[length] = 0;
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
	gboolean running = FALSE;

	trace(name);
	dbus = dbus_g_proxy_new_for_name(connection,
			"org.freedesktop.DBus",
			"/org/freedesktop/DBus",
			"org.freedesktop.DBus");

	dbus_g_proxy_call_with_timeout(dbus, "NameHasOwner", DBUS_TIMEOUT, &error,
			G_TYPE_STRING, name,
			G_TYPE_INVALID,
			G_TYPE_BOOLEAN, &running,
			G_TYPE_INVALID);

	return running;
}
#endif
			
//--------------------------------------------------------------------

/* Builds a preference string according to the given format, taking
 * care to strip forward slashes which have special meaning
 */

void build_pref(char *dest, const char *format, const char* str1, const char* str2)
{
	char buf1[strlen(str1)], buf2[strlen(str2)];

	int i=0, j=0, len;

	len = strlen(str1);
	for (i=0; i<len; ++i) {
		if (str1[i] != '/')
			buf1[j++] = str1[i];
	}
	buf1[j] = 0;

        j=0;
	len = strlen(str2);
	for (i=0; i<len; ++i) {
		if (str2[i] != '/')
			buf2[j++] = str2[i];
	}
	buf2[j] = 0;

	sprintf(dest, format, buf1, buf2);
}

//--------------------------------------------------------------------

/* Convert wchar string to utf-8
 */

#ifdef WIN32

char *wchar_to_utf8(wchar_t *wstring)
{
  char *string = NULL;
  // determine the length of the converted string
  int len = WideCharToMultiByte(CP_UTF8, 0, wstring, -1, 0, 0, NULL, NULL);
  if (len > 0)
    {
      string = malloc(len+1);
      WideCharToMultiByte(CP_UTF8, 0, wstring, -1, string, len, NULL, NULL);
      string[len] = 0;
      trace("Converted wchar string to '%s'", string);
    }

  return string;
}

//--------------------------------------------------------------------

/* Get Window title as a utf-8 string
 */

char *GetWindowTitleUtf8(HWND hWnd)
{
  int title_length = GetWindowTextLengthW(hWnd)+1;
  WCHAR wtitle[title_length];
  GetWindowTextW(hWnd, wtitle, title_length);
  char *title = wchar_to_utf8(wtitle);
  trace("Got window title: %s", title);
  return title;
}

#endif
