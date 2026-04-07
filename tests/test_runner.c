// 테스트 러너는 각 모듈이 예상대로 동작하는지 자동으로 검증한다.
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
// Windows에서는 _mkdir 이름을 사용한다.
#include <direct.h>
#define MAKE_DIR(path) _mkdir(path)
#else
// 다른 환경에서는 일반 mkdir을 사용한다.
#include <sys/stat.h>
#define MAKE_DIR(path) mkdir(path, 0755)
#endif

// 지금까지 실행한 테스트 개수다.
static int tests_run = 0;
// 실패한 테스트 개수다.
static int tests_failed = 0;
// 임시 디렉터리 이름이 겹치지 않게 만드는 카운터다.
static int temp_dir_counter = 0;

static void expect_true(int condition, const char *name) {
    // 테스트 하나를 실행했으므로 카운트를 올린다.
    tests_run++;
    // 조건이 거짓이면 실패로 기록한다.
    if (!condition) {
        tests_failed++;
        fprintf(stderr, "[FAIL] %s\n", name);
    } else {
        // 성공이면 PASS 메시지를 출력한다.
        printf("[PASS] %s\n", name);
    }
}

static int write_text_file(const char *path, const char *content) {
    // 텍스트 파일을 쓸 핸들이다.
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        return 0;
    }

    // 전달받은 문자열 내용을 그대로 파일에 쓴다.
    fputs(content, file);
    fclose(file);
    return 1;
}

static void build_child_path(char *buffer, size_t size, const char *root, const char *child) {
    // root/child 형태의 하위 경로 문자열을 만든다.
    snprintf(buffer, size, "%s/%s", root, child);
}

static int create_test_dirs(char *root, size_t root_size, char *schema_dir, size_t schema_size, char *data_dir, size_t data_size) {
    // 현재 시간을 기반으로 디렉터리 이름에 붙일 숫자다.
    long suffix = (long)time(NULL);
    // 같은 초에 여러 테스트가 돌아도 이름이 겹치지 않게 카운터를 올린다.
    temp_dir_counter++;

    // tests/tmp_<time>_<counter> 형식의 임시 디렉터리 이름을 만든다.
    snprintf(root, root_size, "tests/tmp_%ld_%d", suffix, temp_dir_counter);
    build_child_path(schema_dir, schema_size, root, "schema");
    build_child_path(data_dir, data_size, root, "data");

    // 루트, schema, data 디렉터리를 순서대로 만든다.
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
    // lexer 결과를 담을 토큰 배열이다.
    TokenArray tokens = {0};
    // parser 결과다.
    ParseResult result;
    // lexer 에러 메시지 버퍼다.
    char error[256];

    // SQL 문자열을 먼저 토큰으로 분해한다.
    if (!lex_sql(sql, &tokens, error, sizeof(error))) {
        fprintf(stderr, "lex failed: %s\n", error);
        return 0;
    }

    // 토큰을 Statement 구조로 파싱한다.
    result = parse_statement(&tokens);
    if (!result.ok) {
        fprintf(stderr, "parse failed: %s\n", result.message);
        free_tokens(&tokens);
        return 0;
    }

    // 성공한 Statement를 호출한 쪽으로 넘긴다.
    *statement = result.statement;
    // 토큰 배열은 더 이상 필요 없으므로 해제한다.
    free_tokens(&tokens);
    return 1;
}

static void test_parser_insert(void) {
    // INSERT 문장 테스트용 토큰 배열이다.
    TokenArray tokens = {0};
    ParseResult result;
    char error[256];

    // lexer가 INSERT 문장을 문제없이 읽는지 확인한다.
    expect_true(lex_sql("INSERT INTO users (id, name) VALUES (1, 'Alice');", &tokens, error, sizeof(error)), "lexer parses INSERT");
    // parser가 토큰을 INSERT 문장으로 해석하는지 확인한다.
    result = parse_statement(&tokens);
    expect_true(result.ok, "parser accepts INSERT");
    expect_true(result.statement.type == STATEMENT_INSERT, "parser marks INSERT statement type");
    expect_true(strcmp(result.statement.as.insert_statement.table_name, "users") == 0, "parser reads INSERT table");
    expect_true(result.statement.as.insert_statement.columns.count == 2, "parser reads INSERT column count");
    // 테스트 중 만들어진 메모리를 정리한다.
    free_statement(&result.statement);
    free_tokens(&tokens);
}

static void test_parser_select(void) {
    TokenArray tokens = {0};
    ParseResult result;
    char error[256];

    // SELECT 문장에 대해서도 같은 흐름을 검증한다.
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

    // 세미콜론이 없는 잘못된 SQL도 lexer는 읽을 수 있다.
    expect_true(lex_sql("SELECT name FROM users", &tokens, error, sizeof(error)), "lexer parses SQL without semicolon");
    // 하지만 parser는 문장 끝 규칙 위반으로 거부해야 한다.
    result = parse_statement(&tokens);
    expect_true(!result.ok, "parser rejects missing semicolon");
    free_tokens(&tokens);
}

static void test_schema_loading(void) {
    // 임시 테스트 폴더 경로들이다.
    char root[128];
    char schema_dir[160];
    char data_dir[160];
    char schema_path[192];
    char data_path[192];
    SchemaResult result;

    // 스키마 로딩에 필요한 최소 파일 구조를 직접 만든다.
    expect_true(create_test_dirs(root, sizeof(root), schema_dir, sizeof(schema_dir), data_dir, sizeof(data_dir)), "create schema test directories");
    build_child_path(schema_path, sizeof(schema_path), schema_dir, "users.meta");
    build_child_path(data_path, sizeof(data_path), data_dir, "users.csv");
    expect_true(write_text_file(schema_path, "table=users\ncolumns=id,name,age\n"), "write schema meta");
    expect_true(write_text_file(data_path, "id,name,age\n"), "write schema CSV");

    // 만들어 둔 파일을 기준으로 실제 load_schema가 성공해야 한다.
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

    // INSERT 실행 테스트용 스키마와 CSV 파일을 준비한다.
    expect_true(create_test_dirs(root, sizeof(root), schema_dir, sizeof(schema_dir), data_dir, sizeof(data_dir)), "create insert test directories");
    build_child_path(schema_path, sizeof(schema_path), schema_dir, "users.meta");
    build_child_path(data_path, sizeof(data_path), data_dir, "users.csv");
    expect_true(write_text_file(schema_path, "table=users\ncolumns=id,name,age\n"), "write insert schema meta");
    expect_true(write_text_file(data_path, "id,name,age\n"), "write insert CSV header");
    // 부분 컬럼 INSERT 문장을 Statement로 만든다.
    expect_true(load_statement("INSERT INTO users (id, name) VALUES (7, 'Alice');", &statement), "build INSERT statement for executor");

    // 실제 실행기가 CSV에 한 줄을 추가하는지 확인한다.
    result = execute_statement(&statement, schema_dir, data_dir, stdout);
    expect_true(result.ok, "execute INSERT into CSV");
    // 저장된 결과를 파일에서 다시 읽어 본다.
    csv_text = read_entire_file(data_path, error, sizeof(error));
    expect_true(csv_text != NULL, "read CSV after INSERT");
    if (csv_text != NULL) {
        // age 컬럼은 명시하지 않았으므로 "" 로 저장돼야 한다.
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

    // SELECT 실행 테스트용 데이터 파일을 직접 만든다.
    expect_true(create_test_dirs(root, sizeof(root), schema_dir, sizeof(schema_dir), data_dir, sizeof(data_dir)), "create select test directories");
    build_child_path(schema_path, sizeof(schema_path), schema_dir, "users.meta");
    build_child_path(data_path, sizeof(data_path), data_dir, "users.csv");
    build_child_path(output_path, sizeof(output_path), root, "select_output.txt");
    expect_true(write_text_file(schema_path, "table=users\ncolumns=id,name,age\n"), "write select schema meta");
    expect_true(write_text_file(data_path, "id,name,age\n1,Alice,20\n2,\"Bob, Jr.\",21\n"), "write select CSV data");
    // SELECT 문장을 파싱해 실행 준비를 한다.
    expect_true(load_statement("SELECT name, age FROM users;", &statement), "build SELECT statement for executor");

    // stdout 대신 파일에 결과를 써서 나중에 문자열 비교를 쉽게 한다.
    output_file = fopen(output_path, "wb");
    expect_true(output_file != NULL, "open output file for SELECT capture");
    if (output_file == NULL) {
        free_statement(&statement);
        return;
    }

    // SELECT를 실행하고 결과를 output_file에 저장한다.
    result = execute_statement(&statement, schema_dir, data_dir, output_file);
    fclose(output_file);
    expect_true(result.ok, "execute SELECT from CSV");
    // 저장된 출력 파일을 다시 읽는다.
    output_text = read_entire_file(output_path, error, sizeof(error));
    expect_true(output_text != NULL, "read captured SELECT output");
    if (output_text != NULL) {
        // 헤더와 데이터 두 줄이 모두 예상대로 나와야 한다.
        expect_true(strstr(output_text, "name | age") != NULL, "SELECT prints header");
        expect_true(strstr(output_text, "Alice | 20") != NULL, "SELECT prints first row");
        expect_true(strstr(output_text, "Bob, Jr. | 21") != NULL, "SELECT prints quoted CSV value");
        free(output_text);
    }

    free_statement(&statement);
}

static void test_csv_escape(void) {
    // CSV escape 테스트용 한 줄 데이터다.
    StringList row = {0};
    char root[128];
    char data_dir[160];
    char schema_dir[160];
    char data_path[192];
    char error[256];
    char *csv_text;
    StorageResult result;

    // 직접 notes.csv 파일을 만들고 특수문자 저장을 시험한다.
    expect_true(create_test_dirs(root, sizeof(root), schema_dir, sizeof(schema_dir), data_dir, sizeof(data_dir)), "create CSV escape test directories");
    build_child_path(data_path, sizeof(data_path), data_dir, "notes.csv");
    expect_true(write_text_file(data_path, "text\n"), "write CSV escape header");

    // 쉼표와 큰따옴표가 모두 들어간 문자열을 준비한다.
    expect_true(string_list_push(&row, "hello, \"world\""), "prepare CSV escape row");
    result = append_row_csv(data_dir, "notes", &row);
    expect_true(result.ok, "append CSV row with comma and quote");
    csv_text = read_entire_file(data_path, error, sizeof(error));
    expect_true(csv_text != NULL, "read CSV after escape write");
    if (csv_text != NULL) {
        // CSV 규칙대로 "hello, ""world""" 형태로 저장돼야 한다.
        expect_true(strstr(csv_text, "\"hello, \"\"world\"\"\"") != NULL, "CSV writer escapes quote and comma");
        free(csv_text);
    }
    string_list_free(&row);
}

int main(void) {
    // parser 관련 테스트들이다.
    test_parser_insert();
    test_parser_select();
    test_parser_error();
    // schema, executor, CSV 저장 테스트들이다.
    test_schema_loading();
    test_insert_execution_partial_columns();
    test_select_execution();
    test_csv_escape();

    // 전체 테스트 요약을 출력한다.
    printf("Tests run: %d\n", tests_run);
    printf("Tests failed: %d\n", tests_failed);

    // 하나라도 실패하면 1, 모두 통과하면 0을 반환한다.
    return tests_failed == 0 ? 0 : 1;
}
