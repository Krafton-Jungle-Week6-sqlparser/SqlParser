#ifndef SCHEMA_H
#define SCHEMA_H

#include "util.h"

typedef struct {
    char *table_name;
    StringList columns;
} Schema;

typedef struct {
    int ok;
    Schema schema;
    char message[256];
} SchemaResult;

SchemaResult load_schema(const char *schema_dir, const char *data_dir, const char *table_name);
void free_schema(Schema *schema);

#endif

