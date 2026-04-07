#include "ast.h"
#include "executor.h"
#include "lexer.h"
#include "parser.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    char error[256];
    char *sql_text;
    TokenArray tokens = {0};
    ParseResult parse_result;
    ExecResult exec_result;

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

    exec_result = execute_statement(&parse_result.statement, "schema", "data", stdout);
    if (!exec_result.ok) {
        fprintf(stderr, "error: %s\n", exec_result.message);
        free_statement(&parse_result.statement);
        free_tokens(&tokens);
        free(sql_text);
        return 1;
    }

    printf("%s\n", exec_result.message);

    free_statement(&parse_result.statement);
    free_tokens(&tokens);
    free(sql_text);
    return 0;
}
