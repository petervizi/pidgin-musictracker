#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdio.h>

void trace(char *str, ...);
int readline(FILE* file, char *buf, int len);
const char *parse_value(const char *line, const char* key);
char *put_field(char *buf, char identifier, const char *field);
void trim(char *buf);

#endif // _UTILS_H_
