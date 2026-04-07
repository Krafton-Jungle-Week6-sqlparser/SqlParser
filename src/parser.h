#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "lexer.h"

typedef struct {
    int ok;
    Statement statement;
    char message[256];
} ParseResult;

ParseResult parse_statement(const TokenArray *tokens);

#endif

