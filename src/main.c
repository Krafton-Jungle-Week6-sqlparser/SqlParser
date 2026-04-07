#include "util.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    char error[256];
    char *sql_text;

    if (argc != 2) {
        fprintf(stderr, "usage: sqlparser <sql-file-path>\n");
        return 1;
    }

    sql_text = read_entire_file(argv[1], error, sizeof(error));
    if (sql_text == NULL) {
        fprintf(stderr, "error: %s\n", error);
        return 1;
    }

    printf("Loaded SQL:\n%s\n", sql_text);

    free(sql_text);
    return 0;
}

