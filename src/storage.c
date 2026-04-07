#include "storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void append_character(char **buffer, size_t *length, size_t *capacity, char value) {
    char *new_buffer;
    size_t new_capacity;

    if (*length + 1 >= *capacity) {
        new_capacity = *capacity == 0 ? 16 : *capacity * 2;
        new_buffer = (char *)realloc(*buffer, new_capacity);
        if (new_buffer == NULL) {
            return;
        }

        *buffer = new_buffer;
        *capacity = new_capacity;
    }

    (*buffer)[*length] = value;
    (*length)++;
    (*buffer)[*length] = '\0';
}

int csv_parse_line(const char *line, StringList *fields, char *error, size_t error_size) {
    int in_quotes;
    int just_closed_quote;
    char *buffer;
    size_t length;
    size_t capacity;
    const char *cursor;
    char *trimmed;

    in_quotes = 0;
    just_closed_quote = 0;
    buffer = NULL;
    length = 0;
    capacity = 0;

    for (cursor = line; ; cursor++) {
        char current = *cursor;

        if (in_quotes) {
            if (current == '"') {
                if (cursor[1] == '"') {
                    append_character(&buffer, &length, &capacity, '"');
                    cursor++;
                } else {
                    in_quotes = 0;
                    just_closed_quote = 1;
                }
            } else if (current == '\0') {
                free(buffer);
                snprintf(error, error_size, "unterminated quoted CSV field");
                return 0;
            } else {
                append_character(&buffer, &length, &capacity, current);
            }
            continue;
        }

        if (current == '\0' || current == ',') {
            if (buffer == NULL) {
                buffer = copy_string("");
                if (buffer == NULL) {
                    snprintf(error, error_size, "out of memory while parsing CSV");
                    return 0;
                }
            }

            trimmed = just_closed_quote ? buffer : trim_whitespace(buffer);
            if (!string_list_push(fields, trimmed)) {
                free(buffer);
                snprintf(error, error_size, "out of memory while parsing CSV");
                return 0;
            }

            free(buffer);
            buffer = NULL;
            length = 0;
            capacity = 0;
            just_closed_quote = 0;

            if (current == '\0') {
                break;
            }

            continue;
        }

        if (current == '"' && buffer == NULL) {
            in_quotes = 1;
            continue;
        }

        if (just_closed_quote && current != ' ' && current != '\t') {
            free(buffer);
            snprintf(error, error_size, "unexpected character after quoted CSV field");
            return 0;
        }

        if (current != '\r' && current != '\n') {
            append_character(&buffer, &length, &capacity, current);
        }
    }

    return 1;
}

char *csv_escape_field(const char *value) {
    int needs_quotes;
    size_t index;
    size_t extra_quotes;
    size_t length;
    char *escaped;
    size_t write_index;

    needs_quotes = 0;
    extra_quotes = 0;
    length = strlen(value);

    if (length == 0) {
        return copy_string("\"\"");
    }

    for (index = 0; index < length; index++) {
        if (value[index] == '"' || value[index] == ',' || value[index] == '\n' || value[index] == '\r') {
            needs_quotes = 1;
        }

        if (value[index] == '"') {
            extra_quotes++;
        }
    }

    if (!needs_quotes) {
        return copy_string(value);
    }

    escaped = (char *)malloc(length + extra_quotes + 3);
    if (escaped == NULL) {
        return NULL;
    }

    write_index = 0;
    escaped[write_index++] = '"';
    for (index = 0; index < length; index++) {
        if (value[index] == '"') {
            escaped[write_index++] = '"';
        }

        escaped[write_index++] = value[index];
    }
    escaped[write_index++] = '"';
    escaped[write_index] = '\0';

    return escaped;
}

