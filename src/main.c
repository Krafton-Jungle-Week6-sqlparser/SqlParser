// AST 메모리를 정리하는 free_statement 를 사용합니다.
#include "ast.h"
// SQL 실행 함수 execute_statement 를 사용합니다.
#include "executor.h"
// SQL 문자열을 토큰으로 바꾸는 lex_sql 을 사용합니다.
#include "lexer.h"
// 토큰을 AST 로 해석하는 parse_statement 를 사용합니다.
#include "parser.h"
// 파일 읽기와 문자열 복사 같은 공용 함수를 사용합니다.
#include "util.h"

// fprintf, printf, fopen 을 사용하기 위해 포함합니다.
#include <stdio.h>
// free, malloc 을 사용하기 위해 포함합니다.
#include <stdlib.h>
// strlen, memcpy 를 사용하기 위해 포함합니다.
#include <string.h>

// 사용자가 준 인자가 실제 파일 경로인지 간단히 확인합니다.
static int file_exists(const char *path) {
    // 파일을 열어 볼 때 사용할 포인터입니다.
    FILE *file;

    // 읽기 모드로 파일을 열어 봅니다.
    file = fopen(path, "rb");
    // 열기에 성공했다면 파일이 존재한다는 뜻입니다.
    if (file != NULL) {
        // 확인만 했으므로 바로 닫습니다.
        fclose(file);
        // 파일이 존재한다고 알려 줍니다.
        return 1;
    }

    // 파일을 열지 못했다면 존재하지 않는 것으로 처리합니다.
    return 0;
}

// 여러 개로 들어온 argv 조각을 다시 하나의 SQL 문자열로 합칩니다.
static char *join_arguments_as_sql(int argc, char *argv[], int start_index, char *error, size_t error_size) {
    // 최종 SQL 문자열에 필요한 전체 길이입니다.
    size_t total_length = 1;
    // 반복문에서 사용할 인덱스입니다.
    int index;
    // 최종 SQL 문자열이 저장될 버퍼입니다.
    char *sql_text;
    // 현재 버퍼 어디까지 썼는지 기록합니다.
    size_t offset = 0;
    // 현재 복사할 인자 길이입니다.
    size_t piece_length;

    // 합칠 인자가 없으면 SQL 문장이 없는 상태입니다.
    if (start_index >= argc) {
        snprintf(error, error_size, "missing SQL statement");
        return NULL;
    }

    // 각 인자 길이와 인자 사이 공백 수를 더해 총 길이를 계산합니다.
    for (index = start_index; index < argc; index++) {
        total_length += strlen(argv[index]);
        if (index + 1 < argc) {
            total_length += 1;
        }
    }

    // 계산한 길이만큼 메모리를 확보합니다.
    sql_text = (char *)malloc(total_length);
    // 메모리 확보 실패 시 오류 메시지를 남기고 중단합니다.
    if (sql_text == NULL) {
        snprintf(error, error_size, "out of memory while building SQL statement");
        return NULL;
    }

    // 빈 문자열에서 시작합니다.
    sql_text[0] = '\0';

    // argv 조각들을 순서대로 붙여 하나의 SQL 문장으로 만듭니다.
    for (index = start_index; index < argc; index++) {
        piece_length = strlen(argv[index]);
        memcpy(sql_text + offset, argv[index], piece_length);
        offset += piece_length;

        // 마지막 인자가 아니면 사이에 공백 하나를 넣습니다.
        if (index + 1 < argc) {
            sql_text[offset] = ' ';
            offset++;
        }
    }

    // C 문자열 끝을 표시하는 널 문자를 붙입니다.
    sql_text[offset] = '\0';
    // 완성된 SQL 문자열을 반환합니다.
    return sql_text;
}

// CLI 입력을 읽어 SQL 문자열 하나로 정규화합니다.
static char *load_sql_input(int argc, char *argv[], char *error, size_t error_size) {
    // 최종 SQL 문자열을 담을 포인터입니다.
    char *sql_text;

    // 인자가 아예 없으면 사용 방법 오류입니다.
    if (argc < 2) {
        snprintf(error, error_size, "usage: sqlparser <sql-file-path> OR sqlparser <sql-statement>");
        return NULL;
    }

    // 인자가 하나뿐이면 파일 경로 또는 한 줄 SQL 둘 중 하나입니다.
    if (argc == 2) {
        // 실제 파일이 있으면 기존 방식대로 파일 내용을 읽습니다.
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

    // 인자가 여러 개라면 전부 이어 붙여 직접 입력한 SQL 로 처리합니다.
    return join_arguments_as_sql(argc, argv, 1, error, error_size);
}

// 프로그램의 시작점입니다.
int main(int argc, char *argv[]) {
    // 각 단계의 오류 메시지를 담을 버퍼입니다.
    char error[256];
    // 최종적으로 처리할 SQL 문자열입니다.
    char *sql_text;
    // lexer 가 만들어 낼 토큰 배열입니다.
    TokenArray tokens = {0};
    // parser 가 돌려줄 파싱 결과입니다.
    ParseResult parse_result;
    // executor 가 돌려줄 실행 결과입니다.
    ExecResult exec_result;

    // 파일 경로 입력이든 직접 SQL 입력이든 하나의 SQL 문자열로 가져옵니다.
    sql_text = load_sql_input(argc, argv, error, sizeof(error));
    // 입력을 읽는 데 실패하면 오류를 출력하고 종료합니다.
    if (sql_text == NULL) {
        fprintf(stderr, "error: %s\n", error);
        return 1;
    }

    // SQL 문자열을 토큰 목록으로 분해합니다.
    if (!lex_sql(sql_text, &tokens, error, sizeof(error))) {
        fprintf(stderr, "error: %s\n", error);
        free(sql_text);
        return 1;
    }

    // 토큰 목록을 AST 구조로 해석합니다.
    parse_result = parse_statement(&tokens);
    // 파싱에 실패하면 메시지를 보여 주고 메모리를 정리합니다.
    if (!parse_result.ok) {
        fprintf(stderr, "error: %s\n", parse_result.message);
        free_tokens(&tokens);
        free(sql_text);
        return 1;
    }

    // 해석된 SQL 문을 실제 schema/data 디렉터리에 대해 실행합니다.
    exec_result = execute_statement(&parse_result.statement, "schema", "data", stdout);
    // 실행 단계에서 실패하면 메시지를 보여 주고 메모리를 정리합니다.
    if (!exec_result.ok) {
        fprintf(stderr, "error: %s\n", exec_result.message);
        free_statement(&parse_result.statement);
        free_tokens(&tokens);
        free(sql_text);
        return 1;
    }

    // 실행 결과 메시지 예: INSERT 1, SELECT 3 을 출력합니다.
    printf("%s\n", exec_result.message);

    // 성공한 경우에도 AST 메모리는 직접 정리해야 합니다.
    free_statement(&parse_result.statement);
    // 토큰 배열 메모리도 정리합니다.
    free_tokens(&tokens);
    // 원본 SQL 문자열 메모리도 마지막에 정리합니다.
    free(sql_text);
    // 정상 종료를 뜻하는 0 을 반환합니다.
    return 0;
}
