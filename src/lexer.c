#include "lexer.h"

#include "util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int push_token(TokenArray *tokens, TokenType type, const char *text, int position) {
    int new_capacity;
    Token *new_items;
    char *copy;

    if (tokens->count == tokens->capacity) {
        new_capacity = tokens->capacity == 0 ? 8 : tokens->capacity * 2;
        new_items = (Token *)realloc(tokens->items, (size_t)new_capacity * sizeof(Token));
        if (new_items == NULL) {
            return 0;
        }

        tokens->items = new_items;
        tokens->capacity = new_capacity;
    }

    copy = copy_string(text);
    if (copy == NULL) {
        return 0;
    }

    tokens->items[tokens->count].type = type;
    tokens->items[tokens->count].text = copy;
    tokens->items[tokens->count].position = position;
    tokens->count++;
    return 1;
}

static int push_simple_token(TokenArray *tokens, TokenType type, char value, int position) {
    char text[2];

    text[0] = value;
    text[1] = '\0';
    return push_token(tokens, type, text, position);
}

static int lex_word(const char *input, int *index, TokenArray *tokens) {
    int start = *index;
    int length = 0;
    char *buffer;
    TokenType type;
    int ok;

    while (isalnum((unsigned char)input[*index]) || input[*index] == '_') {
        (*index)++;
        length++;
    }

    buffer = (char *)malloc((size_t)length + 1);
    if (buffer == NULL) {
        return 0;
    }

    memcpy(buffer, input + start, (size_t)length);
    buffer[length] = '\0';

    type = isdigit((unsigned char)buffer[0]) ? TOKEN_NUMBER : TOKEN_IDENTIFIER;
    ok = push_token(tokens, type, buffer, start);
    free(buffer);
    return ok;
}

static int lex_string(const char *input, int *index, TokenArray *tokens, char *error, size_t error_size) {
    int start = *index;
    char *buffer = NULL;
    size_t length = 0;
    size_t capacity = 0;

    (*index)++;

    while (input[*index] != '\0') {
        char current = input[*index];
        char *new_buffer;
        size_t new_capacity;

        if (current == '\'') {
            if (input[*index + 1] == '\'') {
                current = '\'';
                (*index)++;
            } else {
                (*index)++;
                break;
            }
        }

        if (length + 1 >= capacity) {
            new_capacity = capacity == 0 ? 16 : capacity * 2;
            new_buffer = (char *)realloc(buffer, new_capacity);
            if (new_buffer == NULL) {
                free(buffer);
                snprintf(error, error_size, "out of memory while lexing string");
                return 0;
            }

            buffer = new_buffer;
            capacity = new_capacity;
        }

        buffer[length++] = current;
        (*index)++;
    }

    if (input[*index - 1] != '\'') {
        free(buffer);
        snprintf(error, error_size, "unterminated string literal at position %d", start);
        return 0;
    }

    if (buffer == NULL) {
        buffer = copy_string("");
        if (buffer == NULL) {
            snprintf(error, error_size, "out of memory while lexing string");
            return 0;
        }
    } else {
        buffer[length] = '\0';
    }

    if (!push_token(tokens, TOKEN_STRING, buffer, start)) {
        free(buffer);
        snprintf(error, error_size, "out of memory while storing string token");
        return 0;
    }

    free(buffer);
    return 1;
}

int lex_sql(const char *input, TokenArray *tokens, char *error, size_t error_size) {
    int index;

    for (index = 0; input[index] != '\0'; ) {
        char current = input[index];

        if (isspace((unsigned char)current)) {
            index++;
            continue;
        }

        if (isalpha((unsigned char)current) || current == '_' || isdigit((unsigned char)current)) {
            if (!lex_word(input, &index, tokens)) {
                snprintf(error, error_size, "out of memory while lexing SQL");
                return 0;
            }
            continue;
        }

        if (current == '\'') {
            if (!lex_string(input, &index, tokens, error, error_size)) {
                return 0;
            }
            continue;
        }

        if (current == '*') {
            if (!push_simple_token(tokens, TOKEN_STAR, current, index)) {
                snprintf(error, error_size, "out of memory while lexing SQL");
                return 0;
            }
            index++;
            continue;
        }

        if (current == ',') {
            if (!push_simple_token(tokens, TOKEN_COMMA, current, index)) {
                snprintf(error, error_size, "out of memory while lexing SQL");
                return 0;
            }
            index++;
            continue;
        }

        if (current == '(') {
            if (!push_simple_token(tokens, TOKEN_LPAREN, current, index)) {
                snprintf(error, error_size, "out of memory while lexing SQL");
                return 0;
            }
            index++;
            continue;
        }

        if (current == ')') {
            if (!push_simple_token(tokens, TOKEN_RPAREN, current, index)) {
                snprintf(error, error_size, "out of memory while lexing SQL");
                return 0;
            }
            index++;
            continue;
        }

        if (current == ';') {
            if (!push_simple_token(tokens, TOKEN_SEMICOLON, current, index)) {
                snprintf(error, error_size, "out of memory while lexing SQL");
                return 0;
            }
            index++;
            continue;
        }

        snprintf(error, error_size, "unexpected character '%c' at position %d", current, index);
        return 0;
    }

    if (!push_token(tokens, TOKEN_END, "", index)) {
        snprintf(error, error_size, "out of memory while finalizing SQL tokens");
        return 0;
    }

    return 1;
}

void free_tokens(TokenArray *tokens) {
    int index;

    for (index = 0; index < tokens->count; index++) {
        free(tokens->items[index].text);
    }

    free(tokens->items);
    tokens->items = NULL;
    tokens->count = 0;
    tokens->capacity = 0;
}

