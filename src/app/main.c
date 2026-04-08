// AST 구조체를 해제하기 위한 선언입니다.
#include "sqlparser/sql/ast.h"
// 파싱된 SQL을 실제로 실행하기 위한 선언입니다.
#include "sqlparser/execution/executor.h"
// SQL 문자열을 토큰으로 나누기 위한 선언입니다.
#include "sqlparser/sql/lexer.h"
// 토큰 목록을 INSERT / SELECT 구조로 해석하기 위한 선언입니다.
#include "sqlparser/sql/parser.h"
// 파일 읽기와 문자열 복사 같은 공통 유틸 함수를 쓰기 위한 선언입니다.
#include "sqlparser/common/util.h"

// 표준 입출력과 fopen을 사용합니다.
#include <stdio.h>
// free, malloc 같은 메모리 함수들을 사용합니다.
#include <stdlib.h>
// strlen, memcpy 같은 문자열 함수를 사용합니다.
#include <string.h>

// 전달받은 인자가 실제 파일 경로인지 간단히 검사합니다.
static int file_exists(const char *path) {
    // 파일을 열어 존재 여부를 확인할 포인터입니다.
    FILE *file;

    // 읽기 모드로 파일을 열어 봅니다.
    file = fopen(path, "rb");
    // 열기에 성공하면 파일이 존재합니다.
    if (file != NULL) {
        fclose(file);
        return 1;
    }

    // 열지 못했으면 파일이 없다고 봅니다.
    return 0;
}

// 여러 개로 나뉘어 들어온 명령행 인자를 하나의 SQL 문자열로 합칩니다.
static char *join_arguments_as_sql(int argc, char *argv[], int start_index, char *error, size_t error_size) {
    // 최종 SQL 문자열에 필요한 전체 길이입니다.
    size_t total_length = 1;
    // 반복문에 사용할 인덱스입니다.
    int index;
    // 최종 SQL 문자열 버퍼입니다.
    char *sql_text;
    // 버퍼에 현재 어디까지 썼는지 나타냅니다.
    size_t offset = 0;
    // 현재 인자 조각 길이입니다.
    size_t piece_length;

    // 합칠 SQL 인자가 전혀 없으면 오류입니다.
    if (start_index >= argc) {
        snprintf(error, error_size, "missing SQL statement");
        return NULL;
    }

    // 모든 인자 길이와 중간 공백 수를 더해 필요한 메모리 크기를 계산합니다.
    for (index = start_index; index < argc; index++) {
        total_length += strlen(argv[index]);
        if (index + 1 < argc) {
            total_length += 1;
        }
    }

    // 계산된 길이만큼 버퍼를 할당합니다.
    sql_text = (char *)malloc(total_length);
    if (sql_text == NULL) {
        snprintf(error, error_size, "out of memory while building SQL statement");
        return NULL;
    }

    // 빈 문자열로 시작합니다.
    sql_text[0] = '\0';

    // 각 인자를 공백 하나로 이어 붙여 하나의 SQL 문장으로 만듭니다.
    for (index = start_index; index < argc; index++) {
        piece_length = strlen(argv[index]);
        memcpy(sql_text + offset, argv[index], piece_length);
        offset += piece_length;

        if (index + 1 < argc) {
            sql_text[offset] = ' ';
            offset++;
        }
    }

    // C 문자열 끝 표시를 붙입니다.
    sql_text[offset] = '\0';
    return sql_text;
}

// 파일 입력과 직접 SQL 입력을 하나의 SQL 문자열로 정규화합니다.
static char *load_sql_input(int argc, char *argv[], char *error, size_t error_size) {
    // 최종 SQL 문자열을 담을 포인터입니다.
    char *sql_text;

    // 인자가 없으면 사용법 오류입니다.
    if (argc < 2) {
        snprintf(error, error_size, "usage: sqlparser <sql-file-path> OR sqlparser <sql-statement>");
        return NULL;
    }

    // 인자가 하나뿐이면 파일 경로 또는 한 줄 SQL 둘 중 하나입니다.
    if (argc == 2) {
        // 실제 파일이 있으면 기존처럼 파일 내용을 읽습니다.
        if (file_exists(argv[1])) {
            return read_entire_file(argv[1], error, error_size);
        }

        // 파일이 아니면 그 인자 자체를 SQL 문자열로 취급합니다.
        sql_text = copy_string(argv[1]);
        if (sql_text == NULL) {
            snprintf(error, error_size, "out of memory while reading SQL statement");
            return NULL;
        }

        return sql_text;
    }

    // 인자가 여러 개라면 모두 이어 붙여 SQL 문장으로 처리합니다.
    return join_arguments_as_sql(argc, argv, 1, error, error_size);
}

int main(int argc, char *argv[]) {
    // 에러 메시지를 담아 둘 버퍼입니다.
    char error[256];
    // 최종적으로 실행할 SQL 문자열입니다.
    char *sql_text;
    // lexer가 만든 토큰 목록입니다.
    TokenArray tokens = {0};
    // parser가 만든 SQL 문장 구조체 결과입니다.
    ParseResult parse_result;
    // executor가 만든 실행 결과입니다.
    ExecResult exec_result;

    // 파일 입력과 직접 SQL 입력을 모두 받아 SQL 문자열 하나로 만듭니다.
    sql_text = load_sql_input(argc, argv, error, sizeof(error));
    // 입력 단계에서 실패하면 이유를 출력하고 종료합니다.
    if (sql_text == NULL) {
        fprintf(stderr, "error: %s\n", error);
        return 1;
    }

    // SQL 문자열을 토큰 단위로 나누어 tokens에 담습니다.
    if (!lex_sql(sql_text, &tokens, error, sizeof(error))) {
        fprintf(stderr, "error: %s\n", error);
        free(sql_text);
        return 1;
    }

    // 토큰 목록을 실제 INSERT / SELECT 문장 구조로 바꿉니다.
    parse_result = parse_statement(&tokens);
    // 파싱에 실패하면 토큰과 SQL 문자열을 정리하고 종료합니다.
    if (!parse_result.ok) {
        fprintf(stderr, "error: %s\n", parse_result.message);
        free_tokens(&tokens);
        free(sql_text);
        return 1;
    }

    // 파싱된 문장을 schema/ 와 data/ 디렉터리를 기준으로 실행합니다.
    exec_result = execute_statement(&parse_result.statement, "schema", "data", stdout);
    // 실행 중 문제가 생기면 지금까지 만든 메모리를 모두 정리하고 종료합니다.
    if (!exec_result.ok) {
        fprintf(stderr, "error: %s\n", exec_result.message);
        free_statement(&parse_result.statement);
        free_tokens(&tokens);
        free(sql_text);
        return 1;
    }

    // 성공하면 INSERT 1, SELECT 3 같은 결과 메시지를 출력합니다.
    printf("%s\n", exec_result.message);

    // parser가 만든 문장 구조체 내부 메모리를 해제합니다.
    free_statement(&parse_result.statement);
    // lexer가 만든 토큰 문자열들과 배열을 해제합니다.
    free_tokens(&tokens);
    // 처음 읽은 SQL 원문 문자열도 해제합니다.
    free(sql_text);
    // 정상 종료를 의미하는 0을 반환합니다.
    return 0;
}
