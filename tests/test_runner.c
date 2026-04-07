#include "executor.h"
#include "lexer.h"
#include "parser.h"
#include "schema.h"
#include "storage.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#define MAKE_DIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#define MAKE_DIR(path) mkdir(path, 0755)
#endif

static int tests_run = 0;
static int tests_failed = 0;
static int temp_dir_counter = 0;

static void expect_true(int condition, const char *name) {
    tests_run++;
    if (!condition) {
        tests_failed++;
        fprintf(stderr, "[FAIL] %s\n", name);
    } else {
        printf("[PASS] %s\n", name);
    }
}

static int write_text_file(const char *path, const char *content) {
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        return 0;
    }

    fputs(content, file);
    fclose(file);
    return 1;
}

static void build_child_path(char *buffer, size_t size, const char *root, const char *child) {
    snprintf(buffer, size, "%s/%s", root, child);
}

static int create_test_dirs(char *root, size_t root_size, char *schema_dir, size_t schema_size, char *data_dir, size_t data_size) {
    long suffix = (long)time(NULL);
    temp_dir_counter++;

    snprintf(root, root_size, "tests/tmp_%ld_%d", suffix, temp_dir_counter);
    build_child_path(schema_dir, schema_size, root, "schema");
    build_child_path(data_dir, data_size, root, "data");

    if (MAKE_DIR(root) != 0) {
        return 0;
    }
    if (MAKE_DIR(schema_dir) != 0) {
        return 0;
    }
    if (MAKE_DIR(data_dir) != 0) {
        return 0;
    }

    return 1;
}

static int load_statement(const char *sql, Statement *statement) {
    TokenArray tokens = {0};
    ParseResult result;
    char error[256];

    if (!lex_sql(sql, &tokens, error, sizeof(error))) {
        fprintf(stderr, "lex failed: %s\n", error);
        return 0;
    }

    result = parse_statement(&tokens);
    if (!result.ok) {
        fprintf(stderr, "parse failed: %s\n", result.message);
        free_tokens(&tokens);
        return 0;
    }

    *statement = result.statement;
    free_tokens(&tokens);
    return 1;
}

static void test_parser_insert(void) {
    TokenArray tokens = {0};
    ParseResult result;
    char error[256];

    expect_true(lex_sql("INSERT INTO users (id, name) VALUES (1, 'Alice');", &tokens, error, sizeof(error)), "lexer parses INSERT");
    result = parse_statement(&tokens);
    expect_true(result.ok, "parser accepts INSERT");
    expect_true(result.statement.type == STATEMENT_INSERT, "parser marks INSERT statement type");
    expect_true(strcmp(result.statement.as.insert_statement.table_name, "users") == 0, "parser reads INSERT table");
    expect_true(result.statement.as.insert_statement.columns.count == 2, "parser reads INSERT column count");
    free_statement(&result.statement);
    free_tokens(&tokens);
}

static void test_parser_select(void) {
    TokenArray tokens = {0};
    ParseResult result;
    char error[256];

    expect_true(lex_sql("SELECT name, age FROM users;", &tokens, error, sizeof(error)), "lexer parses SELECT");
    result = parse_statement(&tokens);
    expect_true(result.ok, "parser accepts SELECT");
    expect_true(result.statement.type == STATEMENT_SELECT, "parser marks SELECT statement type");
    expect_true(result.statement.as.select_statement.columns.count == 2, "parser reads SELECT column count");
    free_statement(&result.statement);
    free_tokens(&tokens);
}

static void test_parser_error(void) {
    TokenArray tokens = {0};
    ParseResult result;
    char error[256];

    expect_true(lex_sql("SELECT name FROM users", &tokens, error, sizeof(error)), "lexer parses SQL without semicolon");
    result = parse_statement(&tokens);
    expect_true(!result.ok, "parser rejects missing semicolon");
    free_tokens(&tokens);
}

static void test_schema_loading(void) {
    char root[128];
    char schema_dir[160];
    char data_dir[160];
    char schema_path[192];
    char data_path[192];
    SchemaResult result;

    expect_true(create_test_dirs(root, sizeof(root), schema_dir, sizeof(schema_dir), data_dir, sizeof(data_dir)), "create schema test directories");
    build_child_path(schema_path, sizeof(schema_path), schema_dir, "users.meta");
    build_child_path(data_path, sizeof(data_path), data_dir, "users.csv");
    expect_true(write_text_file(schema_path, "table=users\ncolumns=id,name,age\n"), "write schema meta");
    expect_true(write_text_file(data_path, "id,name,age\n"), "write schema CSV");

    result = load_schema(schema_dir, data_dir, "users");
    expect_true(result.ok, "load schema validates existing table");
    if (result.ok) {
        free_schema(&result.schema);
    }
}

static void test_insert_execution_partial_columns(void) {
    char root[128];
    char schema_dir[160];
    char data_dir[160];
    char schema_path[192];
    char data_path[192];
    char *csv_text;
    char error[256];
    Statement statement = {0};
    ExecResult result;

    expect_true(create_test_dirs(root, sizeof(root), schema_dir, sizeof(schema_dir), data_dir, sizeof(data_dir)), "create insert test directories");
    build_child_path(schema_path, sizeof(schema_path), schema_dir, "users.meta");
    build_child_path(data_path, sizeof(data_path), data_dir, "users.csv");
    expect_true(write_text_file(schema_path, "table=users\ncolumns=id,name,age\n"), "write insert schema meta");
    expect_true(write_text_file(data_path, "id,name,age\n"), "write insert CSV header");
    expect_true(load_statement("INSERT INTO users (id, name) VALUES (7, 'Alice');", &statement), "build INSERT statement for executor");

    result = execute_statement(&statement, schema_dir, data_dir, stdout);
    expect_true(result.ok, "execute INSERT into CSV");
    csv_text = read_entire_file(data_path, error, sizeof(error));
    expect_true(csv_text != NULL, "read CSV after INSERT");
    if (csv_text != NULL) {
        expect_true(strstr(csv_text, "7,Alice,\"\"") != NULL, "partial INSERT fills missing column with empty string");
        free(csv_text);
    }

    free_statement(&statement);
}

static void test_select_execution(void) {
    char root[128];
    char schema_dir[160];
    char data_dir[160];
    char schema_path[192];
    char data_path[192];
    char output_path[192];
    char *output_text;
    char error[256];
    Statement statement = {0};
    ExecResult result;
    FILE *output_file;

    expect_true(create_test_dirs(root, sizeof(root), schema_dir, sizeof(schema_dir), data_dir, sizeof(data_dir)), "create select test directories");
    build_child_path(schema_path, sizeof(schema_path), schema_dir, "users.meta");
    build_child_path(data_path, sizeof(data_path), data_dir, "users.csv");
    build_child_path(output_path, sizeof(output_path), root, "select_output.txt");
    expect_true(write_text_file(schema_path, "table=users\ncolumns=id,name,age\n"), "write select schema meta");
    expect_true(write_text_file(data_path, "id,name,age\n1,Alice,20\n2,\"Bob, Jr.\",21\n"), "write select CSV data");
    expect_true(load_statement("SELECT name, age FROM users;", &statement), "build SELECT statement for executor");

    output_file = fopen(output_path, "wb");
    expect_true(output_file != NULL, "open output file for SELECT capture");
    if (output_file == NULL) {
        free_statement(&statement);
        return;
    }

    result = execute_statement(&statement, schema_dir, data_dir, output_file);
    fclose(output_file);
    expect_true(result.ok, "execute SELECT from CSV");
    output_text = read_entire_file(output_path, error, sizeof(error));
    expect_true(output_text != NULL, "read captured SELECT output");
    if (output_text != NULL) {
        expect_true(strstr(output_text, "name | age") != NULL, "SELECT prints header");
        expect_true(strstr(output_text, "Alice | 20") != NULL, "SELECT prints first row");
        expect_true(strstr(output_text, "Bob, Jr. | 21") != NULL, "SELECT prints quoted CSV value");
        free(output_text);
    }

    free_statement(&statement);
}

static void test_csv_escape(void) {
    StringList row = {0};
    char root[128];
    char data_dir[160];
    char schema_dir[160];
    char data_path[192];
    char error[256];
    char *csv_text;
    StorageResult result;

    expect_true(create_test_dirs(root, sizeof(root), schema_dir, sizeof(schema_dir), data_dir, sizeof(data_dir)), "create CSV escape test directories");
    build_child_path(data_path, sizeof(data_path), data_dir, "notes.csv");
    expect_true(write_text_file(data_path, "text\n"), "write CSV escape header");

    expect_true(string_list_push(&row, "hello, \"world\""), "prepare CSV escape row");
    result = append_row_csv(data_dir, "notes", &row);
    expect_true(result.ok, "append CSV row with comma and quote");
    csv_text = read_entire_file(data_path, error, sizeof(error));
    expect_true(csv_text != NULL, "read CSV after escape write");
    if (csv_text != NULL) {
        expect_true(strstr(csv_text, "\"hello, \"\"world\"\"\"") != NULL, "CSV writer escapes quote and comma");
        free(csv_text);
    }
    string_list_free(&row);
}

int main(void) {
    test_parser_insert();
    test_parser_select();
    test_parser_error();
    test_schema_loading();
    test_insert_execution_partial_columns();
    test_select_execution();
    test_csv_escape();

    printf("Tests run: %d\n", tests_run);
    printf("Tests failed: %d\n", tests_failed);

    return tests_failed == 0 ? 0 : 1;
}
