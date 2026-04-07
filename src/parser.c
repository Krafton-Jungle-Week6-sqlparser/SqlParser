#include "parser.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct {
    const TokenArray *tokens;
    int index;
    ParseResult *result;
} ParserState;

static const Token *current_token(const ParserState *state) {
    return &state->tokens->items[state->index];
}

static int match_type(ParserState *state, TokenType type) {
    if (current_token(state)->type == type) {
        state->index++;
        return 1;
    }

    return 0;
}

static int expect_type(ParserState *state, TokenType type, const char *message) {
    if (!match_type(state, type)) {
        snprintf(state->result->message, sizeof(state->result->message), "%s", message);
        return 0;
    }

    return 1;
}

static int expect_keyword(ParserState *state, const char *keyword) {
    const Token *token = current_token(state);

    if (token->type != TOKEN_IDENTIFIER || !strings_equal_ignore_case(token->text, keyword)) {
        snprintf(state->result->message, sizeof(state->result->message), "expected keyword %s", keyword);
        return 0;
    }

    state->index++;
    return 1;
}

static int parse_identifier(ParserState *state, char **value) {
    const Token *token = current_token(state);

    if (token->type != TOKEN_IDENTIFIER) {
        snprintf(state->result->message, sizeof(state->result->message), "expected identifier");
        return 0;
    }

    *value = copy_string(token->text);
    if (*value == NULL) {
        snprintf(state->result->message, sizeof(state->result->message), "out of memory while reading identifier");
        return 0;
    }

    state->index++;
    return 1;
}

static int parse_identifier_list(ParserState *state, StringList *list) {
    char *identifier = NULL;

    if (!parse_identifier(state, &identifier)) {
        return 0;
    }

    if (!string_list_push(list, identifier)) {
        free(identifier);
        snprintf(state->result->message, sizeof(state->result->message), "out of memory while storing identifiers");
        return 0;
    }
    free(identifier);

    while (match_type(state, TOKEN_COMMA)) {
        if (!parse_identifier(state, &identifier)) {
            return 0;
        }

        if (!string_list_push(list, identifier)) {
            free(identifier);
            snprintf(state->result->message, sizeof(state->result->message), "out of memory while storing identifiers");
            return 0;
        }
        free(identifier);
    }

    return 1;
}

static int parse_value_list(ParserState *state, StringList *list) {
    const Token *token;

    token = current_token(state);
    if (token->type != TOKEN_STRING && token->type != TOKEN_NUMBER && token->type != TOKEN_IDENTIFIER) {
        snprintf(state->result->message, sizeof(state->result->message), "expected SQL value");
        return 0;
    }

    if (!string_list_push(list, token->text)) {
        snprintf(state->result->message, sizeof(state->result->message), "out of memory while storing values");
        return 0;
    }
    state->index++;

    while (match_type(state, TOKEN_COMMA)) {
        token = current_token(state);
        if (token->type != TOKEN_STRING && token->type != TOKEN_NUMBER && token->type != TOKEN_IDENTIFIER) {
            snprintf(state->result->message, sizeof(state->result->message), "expected SQL value");
            return 0;
        }

        if (!string_list_push(list, token->text)) {
            snprintf(state->result->message, sizeof(state->result->message), "out of memory while storing values");
            return 0;
        }
        state->index++;
    }

    return 1;
}

static int parse_insert(ParserState *state, Statement *statement) {
    InsertStatement *insert_statement = &statement->as.insert_statement;

    if (!expect_keyword(state, "INSERT")) {
        return 0;
    }

    if (!expect_keyword(state, "INTO")) {
        return 0;
    }

    if (!parse_identifier(state, &insert_statement->table_name)) {
        return 0;
    }

    if (!expect_type(state, TOKEN_LPAREN, "expected '(' after table name")) {
        return 0;
    }

    if (!parse_identifier_list(state, &insert_statement->columns)) {
        return 0;
    }

    if (!expect_type(state, TOKEN_RPAREN, "expected ')' after column list")) {
        return 0;
    }

    if (!expect_keyword(state, "VALUES")) {
        return 0;
    }

    if (!expect_type(state, TOKEN_LPAREN, "expected '(' after VALUES")) {
        return 0;
    }

    if (!parse_value_list(state, &insert_statement->values)) {
        return 0;
    }

    if (!expect_type(state, TOKEN_RPAREN, "expected ')' after value list")) {
        return 0;
    }

    return 1;
}

static int parse_select(ParserState *state, Statement *statement) {
    SelectStatement *select_statement = &statement->as.select_statement;

    if (!expect_keyword(state, "SELECT")) {
        return 0;
    }

    if (match_type(state, TOKEN_STAR)) {
        select_statement->select_all = 1;
    } else if (!parse_identifier_list(state, &select_statement->columns)) {
        return 0;
    }

    if (!expect_keyword(state, "FROM")) {
        return 0;
    }

    if (!parse_identifier(state, &select_statement->table_name)) {
        return 0;
    }

    return 1;
}

ParseResult parse_statement(const TokenArray *tokens) {
    ParseResult result = {0};
    ParserState state;

    state.tokens = tokens;
    state.index = 0;
    state.result = &result;

    if (current_token(&state)->type == TOKEN_IDENTIFIER &&
        strings_equal_ignore_case(current_token(&state)->text, "INSERT")) {
        result.statement.type = STATEMENT_INSERT;
        if (!parse_insert(&state, &result.statement)) {
            free_statement(&result.statement);
            return result;
        }
    } else if (current_token(&state)->type == TOKEN_IDENTIFIER &&
               strings_equal_ignore_case(current_token(&state)->text, "SELECT")) {
        result.statement.type = STATEMENT_SELECT;
        if (!parse_select(&state, &result.statement)) {
            free_statement(&result.statement);
            return result;
        }
    } else {
        snprintf(result.message, sizeof(result.message), "only INSERT and SELECT are supported");
        return result;
    }

    if (!expect_type(&state, TOKEN_SEMICOLON, "expected ';' at end of SQL statement")) {
        free_statement(&result.statement);
        return result;
    }

    if (!expect_type(&state, TOKEN_END, "unexpected tokens after SQL statement")) {
        free_statement(&result.statement);
        return result;
    }

    result.ok = 1;
    return result;
}

