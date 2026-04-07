#include "ast.h"

#include <stdlib.h>

void free_statement(Statement *statement) {
    if (statement->type == STATEMENT_INSERT) {
        free(statement->as.insert_statement.table_name);
        statement->as.insert_statement.table_name = NULL;
        string_list_free(&statement->as.insert_statement.columns);
        string_list_free(&statement->as.insert_statement.values);
    } else if (statement->type == STATEMENT_SELECT) {
        free(statement->as.select_statement.table_name);
        statement->as.select_statement.table_name = NULL;
        string_list_free(&statement->as.select_statement.columns);
    }
}

