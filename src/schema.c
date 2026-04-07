#include "schema.h"

#include "storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_schema_error(SchemaResult *result, const char *message) {
    result->ok = 0;
    snprintf(result->message, sizeof(result->message), "%s", message);
}

static int parse_columns_value(const char *value, StringList *columns, char *message, size_t message_size) {
    StringList parsed = {0};
    int ok;

    ok = csv_parse_line(value, &parsed, message, message_size);
    if (!ok) {
        return 0;
    }

    *columns = parsed;
    return 1;
}

static int validate_csv_header(const char *data_dir, const char *table_name, const Schema *schema, char *message, size_t message_size) {
    char *path;
    FILE *file;
    char line[4096];
    StringList header = {0};
    int index;

    path = build_path(data_dir, table_name, ".csv");
    if (path == NULL) {
        snprintf(message, message_size, "out of memory while building CSV path");
        return 0;
    }

    file = fopen(path, "rb");
    free(path);
    if (file == NULL) {
        snprintf(message, message_size, "table data file does not exist");
        return 0;
    }

    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        snprintf(message, message_size, "table data file is empty");
        return 0;
    }

    fclose(file);
    strip_line_endings(line);

    if (!csv_parse_line(line, &header, message, message_size)) {
        return 0;
    }

    if (header.count != schema->columns.count) {
        string_list_free(&header);
        snprintf(message, message_size, "CSV header does not match schema column count");
        return 0;
    }

    for (index = 0; index < header.count; index++) {
        if (strcmp(header.items[index], schema->columns.items[index]) != 0) {
            string_list_free(&header);
            snprintf(message, message_size, "CSV header does not match schema column order");
            return 0;
        }
    }

    string_list_free(&header);
    return 1;
}

SchemaResult load_schema(const char *schema_dir, const char *data_dir, const char *table_name) {
    SchemaResult result = {0};
    char *path;
    FILE *file;
    char line[4096];
    char *trimmed;
    char *separator;

    path = build_path(schema_dir, table_name, ".meta");
    if (path == NULL) {
        set_schema_error(&result, "out of memory while building schema path");
        return result;
    }

    file = fopen(path, "rb");
    free(path);
    if (file == NULL) {
        set_schema_error(&result, "schema meta file does not exist");
        return result;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        strip_line_endings(line);
        trimmed = trim_whitespace(line);
        if (*trimmed == '\0') {
            continue;
        }

        separator = strchr(trimmed, '=');
        if (separator == NULL) {
            fclose(file);
            free_schema(&result.schema);
            set_schema_error(&result, "invalid schema meta format");
            return result;
        }

        *separator = '\0';
        separator++;
        separator = trim_whitespace(separator);

        if (strcmp(trimmed, "table") == 0) {
            free(result.schema.table_name);
            result.schema.table_name = copy_string(separator);
            if (result.schema.table_name == NULL) {
                fclose(file);
                free_schema(&result.schema);
                set_schema_error(&result, "out of memory while reading schema table name");
                return result;
            }
        } else if (strcmp(trimmed, "columns") == 0) {
            string_list_free(&result.schema.columns);
            if (!parse_columns_value(separator, &result.schema.columns, result.message, sizeof(result.message))) {
                fclose(file);
                free_schema(&result.schema);
                result.ok = 0;
                return result;
            }
        }
    }

    fclose(file);

    if (result.schema.table_name == NULL || result.schema.columns.count == 0) {
        free_schema(&result.schema);
        set_schema_error(&result, "schema meta file is missing required fields");
        return result;
    }

    if (strcmp(result.schema.table_name, table_name) != 0) {
        free_schema(&result.schema);
        set_schema_error(&result, "schema table name does not match requested table");
        return result;
    }

    if (!validate_csv_header(data_dir, table_name, &result.schema, result.message, sizeof(result.message))) {
        free_schema(&result.schema);
        result.ok = 0;
        return result;
    }

    result.ok = 1;
    return result;
}

void free_schema(Schema *schema) {
    free(schema->table_name);
    schema->table_name = NULL;
    string_list_free(&schema->columns);
}

