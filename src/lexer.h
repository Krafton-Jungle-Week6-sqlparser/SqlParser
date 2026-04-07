#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

typedef enum {
    TOKEN_IDENTIFIER,
    TOKEN_STRING,
    TOKEN_NUMBER,
    TOKEN_STAR,
    TOKEN_COMMA,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_SEMICOLON,
    TOKEN_END
} TokenType;

typedef struct {
    TokenType type;
    char *text;
    int position;
} Token;

typedef struct {
    Token *items;
    int count;
    int capacity;
} TokenArray;

int lex_sql(const char *input, TokenArray *tokens, char *error, size_t error_size);
void free_tokens(TokenArray *tokens);

#endif

