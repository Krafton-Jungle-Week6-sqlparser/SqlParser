#include "util.h"

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

