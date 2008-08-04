#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdio.h>
#define PCRE_STATIC
#include <pcre.h>

#ifndef WIN32
#include <dbus/dbus-glib.h>
#endif

void trace(const char *str, ...);
int readline(FILE* file, char *buf, int len);
const char *parse_value(const char *line, const char* key);
char *put_field(char *buf, char identifier, const char *field);
void trim(char *buf);
void build_pref(char *dest, const char *format, const char* str1, const char* str2);

pcre* regex(const char* pattern, int options);
int capture(pcre* re, const char *text, int len, ...);

#ifndef WIN32
gboolean dbus_g_running(DBusGConnection *connection, const char *name);
#endif

#endif // _UTILS_H_
