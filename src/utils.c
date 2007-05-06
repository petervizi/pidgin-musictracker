#include "debug.h"
#include "musictracker.h"
#include <string.h>
#include <stdio.h>
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
		FILE *log = fopen("/tmp/musictracker", "a");
		assert(log);
		time_t t;
		time(&t);
		ctime_r(&t, buf2);
		buf2[strlen(buf2)-1] = 0;
		fprintf(log, "%s: %s\n", buf2, buf);
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
