#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>

static void print_parsed_statement(const Statement *statement) {
    int index;

    if (statement->type == STATEMENT_INSERT) {
        printf("Parsed INSERT into table '%s'\n", statement->as.insert_statement.table_name);
        printf("Columns:");
        for (index = 0; index < statement->as.insert_statement.columns.count; index++) {
            printf(" %s", statement->as.insert_statement.columns.items[index]);
        }
        printf("\nValues:");
        for (index = 0; index < statement->as.insert_statement.values.count; index++) {
            printf(" %s", statement->as.insert_statement.values.items[index]);
        }
        printf("\n");
        return;
    }

    printf("Parsed SELECT from table '%s'\n", statement->as.select_statement.table_name);
    if (statement->as.select_statement.select_all) {
        printf("Columns: *\n");
        return;
    }

    printf("Columns:");
    for (index = 0; index < statement->as.select_statement.columns.count; index++) {
        printf(" %s", statement->as.select_statement.columns.items[index]);
    }
    printf("\n");
}

int main(int argc, char *argv[]) {
    char error[256];
    char *sql_text;
    TokenArray tokens = {0};
    ParseResult parse_result;

    if (argc != 2) {
        fprintf(stderr, "usage: sqlparser <sql-file-path>\n");
        return 1;
    }

    sql_text = read_entire_file(argv[1], error, sizeof(error));
    if (sql_text == NULL) {
        fprintf(stderr, "error: %s\n", error);
        return 1;
    }

    if (!lex_sql(sql_text, &tokens, error, sizeof(error))) {
        fprintf(stderr, "error: %s\n", error);
        free(sql_text);
        return 1;
    }

    parse_result = parse_statement(&tokens);
    if (!parse_result.ok) {
        fprintf(stderr, "error: %s\n", parse_result.message);
        free_tokens(&tokens);
        free(sql_text);
        return 1;
    }

    print_parsed_statement(&parse_result.statement);

    free_statement(&parse_result.statement);
    free_tokens(&tokens);
    free(sql_text);
    return 0;
}
