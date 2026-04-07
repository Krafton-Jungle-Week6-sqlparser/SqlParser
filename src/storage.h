#ifndef STORAGE_H
#define STORAGE_H

#include "util.h"

#include <stddef.h>

typedef struct {
    int ok;
    int affected_rows;
    char message[256];
} StorageResult;

int csv_parse_line(const char *line, StringList *fields, char *error, size_t error_size);
char *csv_escape_field(const char *value);
StorageResult append_row_csv(const char *data_dir, const char *table_name, const StringList *row_values);

#endif
