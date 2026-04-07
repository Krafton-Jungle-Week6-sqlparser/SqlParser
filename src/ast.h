#ifndef AST_H
#define AST_H

#include "util.h"

typedef enum {
    STATEMENT_INSERT,
    STATEMENT_SELECT
} StatementType;

typedef struct {
    char *table_name;
    StringList columns;
    StringList values;
} InsertStatement;

typedef struct {
    char *table_name;
    StringList columns;
    int select_all;
} SelectStatement;

typedef struct {
    StatementType type;
    union {
        InsertStatement insert_statement;
        SelectStatement select_statement;
    } as;
} Statement;

void free_statement(Statement *statement);

#endif

