#include "util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *read_entire_file(const char *path, char *error, size_t error_size) {
    FILE *file;
    long file_size;
    size_t read_size;
    char *buffer;

    file = fopen(path, "rb");
    if (file == NULL) {
        snprintf(error, error_size, "failed to open SQL file: %s", path);
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        snprintf(error, error_size, "failed to seek SQL file: %s", path);
        return NULL;
    }

    file_size = ftell(file);
    if (file_size < 0) {
        fclose(file);
        snprintf(error, error_size, "failed to measure SQL file: %s", path);
        return NULL;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        snprintf(error, error_size, "failed to rewind SQL file: %s", path);
        return NULL;
    }

    buffer = (char *)malloc((size_t)file_size + 1);
    if (buffer == NULL) {
        fclose(file);
        snprintf(error, error_size, "out of memory while reading SQL file");
        return NULL;
    }

    read_size = fread(buffer, 1, (size_t)file_size, file);
    fclose(file);

    if (read_size != (size_t)file_size) {
        free(buffer);
        snprintf(error, error_size, "failed to read SQL file: %s", path);
        return NULL;
    }

    buffer[file_size] = '\0';
    return buffer;
}

char *copy_string(const char *source) {
    size_t length;
    char *copy;

    if (source == NULL) {
        return NULL;
    }

    length = strlen(source);
    copy = (char *)malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, source, length + 1);
    return copy;
}

int strings_equal_ignore_case(const char *left, const char *right) {
    unsigned char left_char;
    unsigned char right_char;

    if (left == NULL || right == NULL) {
        return 0;
    }

    while (*left != '\0' && *right != '\0') {
        left_char = (unsigned char)*left;
        right_char = (unsigned char)*right;

        if (tolower(left_char) != tolower(right_char)) {
            return 0;
        }

        left++;
        right++;
    }

    return *left == '\0' && *right == '\0';
}

char *trim_whitespace(char *text) {
    char *end;

    if (text == NULL) {
        return NULL;
    }

    while (*text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }

    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        end--;
    }

    *end = '\0';
    return text;
}

void strip_line_endings(char *text) {
    size_t length;

    if (text == NULL) {
        return;
    }

    length = strlen(text);
    while (length > 0 && (text[length - 1] == '\n' || text[length - 1] == '\r')) {
        text[length - 1] = '\0';
        length--;
    }
}

int string_list_push(StringList *list, const char *value) {
    int new_capacity;
    char **new_items;
    char *copy;

    if (list->count == list->capacity) {
        new_capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        new_items = (char **)realloc(list->items, (size_t)new_capacity * sizeof(char *));
        if (new_items == NULL) {
            return 0;
        }

        list->items = new_items;
        list->capacity = new_capacity;
    }

    copy = copy_string(value);
    if (copy == NULL) {
        return 0;
    }

    list->items[list->count] = copy;
    list->count++;
    return 1;
}

int string_list_index_of(const StringList *list, const char *value) {
    int index;

    for (index = 0; index < list->count; index++) {
        if (strcmp(list->items[index], value) == 0) {
            return index;
        }
    }

    return -1;
}

void string_list_free(StringList *list) {
    int index;

    for (index = 0; index < list->count; index++) {
        free(list->items[index]);
    }

    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

char *build_path(const char *dir, const char *name, const char *extension) {
    size_t length;
    char *path;

    length = strlen(dir) + strlen(name) + strlen(extension) + 3;
    path = (char *)malloc(length);
    if (path == NULL) {
        return NULL;
    }

    snprintf(path, length, "%s/%s%s", dir, name, extension);
    return path;
}

