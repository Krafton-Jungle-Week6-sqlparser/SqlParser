// AST 구조체를 해제하기 위한 선언이다.
#include "sqlparser/sql/ast.h"
// 파싱된 SQL을 실제로 실행하기 위한 선언이다.
#include "sqlparser/execution/executor.h"
// SQL 문자열을 토큰으로 나누기 위한 선언이다.
#include "sqlparser/sql/lexer.h"
// 토큰 목록을 INSERT / SELECT 구조로 해석하기 위한 선언이다.
#include "sqlparser/sql/parser.h"
// 파일 읽기 같은 공통 유틸 함수를 쓰기 위한 선언이다.
#include "sqlparser/common/util.h"

// 표준 입출력 함수를 사용한다.
#include <stdio.h>
// free 같은 메모리 해제 함수를 사용한다.
#include <stdlib.h>

int main(int argc, char *argv[]) {
    // 에러 메시지를 담아 둘 버퍼다.
    char error[256];
    // SQL 파일 전체 내용을 담을 문자열 포인터다.
    char *sql_text;
    // lexer가 만든 토큰 목록이다.
    TokenArray tokens = {0};
    // parser가 만든 SQL 문장 구조체 결과다.
    ParseResult parse_result;
    // executor가 만든 실행 결과다.
    ExecResult exec_result;

    // 프로그램 실행 인자는 프로그램 이름 + SQL 파일 경로, 총 2개여야 한다.
    if (argc != 2) {
        // 사용법이 틀리면 표준 에러로 올바른 형식을 안내한다.
        fprintf(stderr, "usage: sqlparser <sql-file-path>\n");
        // 비정상 종료를 뜻하는 1을 반환한다.
        return 1;
    }

    // 사용자가 넘긴 SQL 파일 경로를 읽어서 메모리에 올린다.
    sql_text = read_entire_file(argv[1], error, sizeof(error));
    // 파일 읽기에 실패하면 에러 메시지를 출력하고 종료한다.
    if (sql_text == NULL) {
        fprintf(stderr, "error: %s\n", error);
        return 1;
    }

    // SQL 문자열을 토큰 단위로 잘라서 tokens에 저장한다.
    if (!lex_sql(sql_text, &tokens, error, sizeof(error))) {
        fprintf(stderr, "error: %s\n", error);
        // 더 이상 쓸 수 없는 SQL 문자열 메모리를 해제한다.
        free(sql_text);
        return 1;
    }

    // 토큰 목록을 실제 INSERT / SELECT 문장 구조로 바꾼다.
    parse_result = parse_statement(&tokens);
    // 파싱에 실패하면 토큰과 SQL 문자열을 모두 정리하고 종료한다.
    if (!parse_result.ok) {
        fprintf(stderr, "error: %s\n", parse_result.message);
        free_tokens(&tokens);
        free(sql_text);
        return 1;
    }

    // 파싱된 문장을 schema/ 와 data/ 디렉터리를 기준으로 실행한다.
    exec_result = execute_statement(&parse_result.statement, "schema", "data", stdout);
    // 실행 중 문제가 생기면 지금까지 만든 메모리를 정리하고 종료한다.
    if (!exec_result.ok) {
        fprintf(stderr, "error: %s\n", exec_result.message);
        free_statement(&parse_result.statement);
        free_tokens(&tokens);
        free(sql_text);
        return 1;
    }

    // 실행이 끝나면 INSERT 1, SELECT 3 같은 최종 결과 메시지를 출력한다.
    printf("%s\n", exec_result.message);

    // parser가 만든 문장 구조체 내부 메모리를 해제한다.
    free_statement(&parse_result.statement);
    // lexer가 만든 토큰 문자열들과 배열을 해제한다.
    free_tokens(&tokens);
    // 처음 읽은 SQL 원문 문자열도 해제한다.
    free(sql_text);
    // 정상 종료를 뜻하는 0을 반환한다.
    return 0;
}
