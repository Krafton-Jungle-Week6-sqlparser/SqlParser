#ifndef STORAGE_H
#define STORAGE_H

#include "util.h"

#include <stddef.h>

int csv_parse_line(const char *line, StringList *fields, char *error, size_t error_size);
char *csv_escape_field(const char *value);

#endif

