#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "ast.h"

#include <stdio.h>

typedef struct {
    int ok;
    int affected_rows;
    char message[256];
} ExecResult;

ExecResult execute_statement(const Statement *statement, const char *schema_dir, const char *data_dir, FILE *out);

#endif

