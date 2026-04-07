#include "executor.h"

#include "schema.h"
#include "storage.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_exec_error(ExecResult *result, const char *message) {
    result->ok = 0;
    snprintf(result->message, sizeof(result->message), "%s", message);
}

static int contains_newline(const char *value) {
    return strchr(value, '\n') != NULL || strchr(value, '\r') != NULL;
}

static int validate_insert_columns(const InsertStatement *statement, const Schema *schema, char *message, size_t message_size) {
    int index;

    if (statement->columns.count != statement->values.count) {
        snprintf(message, message_size, "column count and value count do not match");
        return 0;
    }

    for (index = 0; index < statement->columns.count; index++) {
        if (string_list_index_of(&schema->columns, statement->columns.items[index]) < 0) {
            snprintf(message, message_size, "unknown column in INSERT: %s", statement->columns.items[index]);
            return 0;
        }

        if (contains_newline(statement->values.items[index])) {
            snprintf(message, message_size, "newline in value is not supported");
            return 0;
        }
    }

    return 1;
}

static ExecResult execute_insert(const InsertStatement *statement, const char *schema_dir, const char *data_dir) {
    ExecResult result = {0};
    SchemaResult schema_result;
    StringList row_values = {0};
    int *assigned = NULL;
    int schema_index;
    int value_index;
    StorageResult storage_result;

    schema_result = load_schema(schema_dir, data_dir, statement->table_name);
    if (!schema_result.ok) {
        set_exec_error(&result, schema_result.message);
        return result;
    }

    if (!validate_insert_columns(statement, &schema_result.schema, result.message, sizeof(result.message))) {
        free_schema(&schema_result.schema);
        return result;
    }

    assigned = (int *)calloc((size_t)schema_result.schema.columns.count, sizeof(int));
    if (assigned == NULL) {
        free_schema(&schema_result.schema);
        set_exec_error(&result, "out of memory while preparing INSERT row");
        return result;
    }

    for (schema_index = 0; schema_index < schema_result.schema.columns.count; schema_index++) {
        if (!string_list_push(&row_values, "")) {
            free(assigned);
            string_list_free(&row_values);
            free_schema(&schema_result.schema);
            set_exec_error(&result, "out of memory while preparing INSERT row");
            return result;
        }
    }

    for (value_index = 0; value_index < statement->columns.count; value_index++) {
        schema_index = string_list_index_of(&schema_result.schema.columns, statement->columns.items[value_index]);
        if (schema_index < 0) {
            free(assigned);
            string_list_free(&row_values);
            free_schema(&schema_result.schema);
            set_exec_error(&result, "unknown column in INSERT");
            return result;
        }

        if (assigned[schema_index]) {
            free(assigned);
            string_list_free(&row_values);
            free_schema(&schema_result.schema);
            set_exec_error(&result, "duplicate column in INSERT");
            return result;
        }

        assigned[schema_index] = 1;
        free(row_values.items[schema_index]);
        row_values.items[schema_index] = copy_string(statement->values.items[value_index]);
        if (row_values.items[schema_index] == NULL) {
            free(assigned);
            string_list_free(&row_values);
            free_schema(&schema_result.schema);
            set_exec_error(&result, "out of memory while preparing INSERT row");
            return result;
        }
    }

    storage_result = append_row_csv(data_dir, statement->table_name, &row_values);
    free(assigned);
    string_list_free(&row_values);
    free_schema(&schema_result.schema);

    if (!storage_result.ok) {
        set_exec_error(&result, storage_result.message);
        return result;
    }

    result.ok = 1;
    result.affected_rows = storage_result.affected_rows;
    snprintf(result.message, sizeof(result.message), "%s", storage_result.message);
    return result;
}

static int build_select_indexes(const SelectStatement *statement, const Schema *schema, StringList *selected_headers, int *selected_indexes, char *message, size_t message_size) {
    int index;
    int schema_index;

    if (statement->select_all) {
        for (index = 0; index < schema->columns.count; index++) {
            if (!string_list_push(selected_headers, schema->columns.items[index])) {
                snprintf(message, message_size, "out of memory while preparing SELECT");
                return 0;
            }
            selected_indexes[index] = index;
        }
        return schema->columns.count;
    }

    for (index = 0; index < statement->columns.count; index++) {
        schema_index = string_list_index_of(&schema->columns, statement->columns.items[index]);
        if (schema_index < 0) {
            snprintf(message, message_size, "unknown column in SELECT: %s", statement->columns.items[index]);
            return 0;
        }

        if (!string_list_push(selected_headers, statement->columns.items[index])) {
            snprintf(message, message_size, "out of memory while preparing SELECT");
            return 0;
        }
        selected_indexes[index] = schema_index;
    }

    return statement->columns.count;
}

static void print_selected_row(FILE *out, const StringList *fields, const int *selected_indexes, int selected_count) {
    int index;

    for (index = 0; index < selected_count; index++) {
        if (index > 0) {
            fprintf(out, " | ");
        }
        fprintf(out, "%s", fields->items[selected_indexes[index]]);
    }
    fprintf(out, "\n");
}

static void print_header_row(FILE *out, const StringList *headers) {
    int index;

    for (index = 0; index < headers->count; index++) {
        if (index > 0) {
            fprintf(out, " | ");
        }
        fprintf(out, "%s", headers->items[index]);
    }
    fprintf(out, "\n");
}

static ExecResult execute_select(const SelectStatement *statement, const char *schema_dir, const char *data_dir, FILE *out) {
    ExecResult result = {0};
    SchemaResult schema_result;
    char *path;
    FILE *file;
    char line[4096];
    StringList fields = {0};
    StringList headers = {0};
    int *selected_indexes = NULL;
    int selected_count;
    int row_count = 0;

    schema_result = load_schema(schema_dir, data_dir, statement->table_name);
    if (!schema_result.ok) {
        set_exec_error(&result, schema_result.message);
        return result;
    }

    selected_indexes = (int *)calloc((size_t)schema_result.schema.columns.count, sizeof(int));
    if (selected_indexes == NULL) {
        free_schema(&schema_result.schema);
        set_exec_error(&result, "out of memory while preparing SELECT");
        return result;
    }

    selected_count = build_select_indexes(statement, &schema_result.schema, &headers, selected_indexes, result.message, sizeof(result.message));
    if (selected_count == 0) {
        free(selected_indexes);
        string_list_free(&headers);
        free_schema(&schema_result.schema);
        return result;
    }

    print_header_row(out, &headers);

    path = build_path(data_dir, statement->table_name, ".csv");
    if (path == NULL) {
        free(selected_indexes);
        string_list_free(&headers);
        free_schema(&schema_result.schema);
        set_exec_error(&result, "out of memory while opening table data");
        return result;
    }

    file = fopen(path, "rb");
    free(path);
    if (file == NULL) {
        free(selected_indexes);
        string_list_free(&headers);
        free_schema(&schema_result.schema);
        set_exec_error(&result, "failed to open table data");
        return result;
    }

    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        free(selected_indexes);
        string_list_free(&headers);
        free_schema(&schema_result.schema);
        set_exec_error(&result, "table data file is empty");
        return result;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        strip_line_endings(line);
        if (line[0] == '\0') {
            continue;
        }

        if (!csv_parse_line(line, &fields, result.message, sizeof(result.message))) {
            fclose(file);
            free(selected_indexes);
            string_list_free(&headers);
            string_list_free(&fields);
            free_schema(&schema_result.schema);
            result.ok = 0;
            return result;
        }

        if (fields.count != schema_result.schema.columns.count) {
            fclose(file);
            free(selected_indexes);
            string_list_free(&headers);
            string_list_free(&fields);
            free_schema(&schema_result.schema);
            set_exec_error(&result, "CSV row does not match schema column count");
            return result;
        }

        print_selected_row(out, &fields, selected_indexes, selected_count);
        string_list_free(&fields);
        row_count++;
    }

    fclose(file);
    free(selected_indexes);
    string_list_free(&headers);
    free_schema(&schema_result.schema);

    result.ok = 1;
    result.affected_rows = row_count;
    snprintf(result.message, sizeof(result.message), "SELECT %d", row_count);
    return result;
}

ExecResult execute_statement(const Statement *statement, const char *schema_dir, const char *data_dir, FILE *out) {
    if (statement->type == STATEMENT_INSERT) {
        return execute_insert(&statement->as.insert_statement, schema_dir, data_dir);
    }

    return execute_select(&statement->as.select_statement, schema_dir, data_dir, out);
}
