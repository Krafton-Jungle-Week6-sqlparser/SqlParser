// schema.c는 meta 파일과 CSV 헤더를 읽어 테이블 구조를 검증한다.
#include "sqlparser/storage/schema.h"

// CSV 한 줄을 컬럼 목록으로 파싱하기 위해 사용한다.
#include "sqlparser/storage/storage.h"

// 파일 읽기와 메시지 생성을 위해 포함한다.
#include <stdio.h>
// free 함수를 쓰기 위해 포함한다.
#include <stdlib.h>
// strcmp, strchr를 쓰기 위해 포함한다.
#include <string.h>

static void set_schema_error(SchemaResult *result, const char *message) {
    // 실패 플래그를 0으로 둔다.
    result->ok = 0;
    // 사람이 읽을 수 있는 오류 메시지를 저장한다.
    snprintf(result->message, sizeof(result->message), "%s", message);
}

static int parse_columns_value(const char *value, StringList *columns, char *message, size_t message_size) {
    // columns=id,name,age 값을 잠시 CSV처럼 파싱할 임시 리스트다.
    StringList parsed = {0};
    // csv_parse_line 성공 여부다.
    int ok;

    // 컬럼 목록도 쉼표로 구분돼 있으므로 CSV 파서를 재사용한다.
    ok = csv_parse_line(value, &parsed, message, message_size);
    if (!ok) {
        return 0;
    }

    *columns = parsed;
    return 1;
}

static int validate_csv_header(const char *data_dir, const char *table_name, const Schema *schema, char *message, size_t message_size) {
    // data/<table>.csv 경로다.
    char *path;
    // CSV 파일 핸들이다.
    FILE *file;
    // 첫 줄 헤더를 읽어 올 버퍼다.
    char line[4096];
    // 파싱된 헤더 컬럼 목록이다.
    StringList header = {0};
    // 컬럼 순회를 위한 인덱스다.
    int index;

    // data 디렉터리 기준 CSV 경로를 만든다.
    path = build_path(data_dir, table_name, ".csv");
    if (path == NULL) {
        snprintf(message, message_size, "out of memory while building CSV path");
        return 0;
    }

    // CSV 파일을 연다.
    file = fopen(path, "rb");
    // 경로 문자열은 더 이상 필요 없으므로 해제한다.
    free(path);
    if (file == NULL) {
        snprintf(message, message_size, "table data file does not exist");
        return 0;
    }

    // 첫 줄이 없으면 헤더가 없는 잘못된 파일이다.
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        snprintf(message, message_size, "table data file is empty");
        return 0;
    }

    // 헤더 한 줄만 읽으면 되므로 파일을 닫는다.
    fclose(file);
    // 줄 끝의 \r, \n 을 지워 깔끔한 헤더 문자열로 만든다.
    strip_line_endings(line);

    // 헤더 문자열을 컬럼 리스트로 파싱한다.
    if (!csv_parse_line(line, &header, message, message_size)) {
        return 0;
    }

    // 헤더 컬럼 수와 schema 컬럼 수가 다르면 실패다.
    if (header.count != schema->columns.count) {
        string_list_free(&header);
        snprintf(message, message_size, "CSV header does not match schema column count");
        return 0;
    }

    // 컬럼 이름과 순서가 모두 같은지 하나씩 비교한다.
    for (index = 0; index < header.count; index++) {
        if (strcmp(header.items[index], schema->columns.items[index]) != 0) {
            string_list_free(&header);
            snprintf(message, message_size, "CSV header does not match schema column order");
            return 0;
        }
    }

    // 임시 헤더 리스트 메모리를 정리한다.
    string_list_free(&header);
    return 1;
}

SchemaResult load_schema(const char *schema_dir, const char *data_dir, const char *table_name) {
    // 반환할 최종 결과 구조체다.
    SchemaResult result = {0};
    // schema/<table>.meta 경로 문자열이다.
    char *path;
    // meta 파일 핸들이다.
    FILE *file;
    // meta 파일을 한 줄씩 읽을 버퍼다.
    char line[4096];
    // 공백 제거 후 실제 내용을 가리키는 포인터다.
    char *trimmed;
    // key=value 에서 '=' 위치를 찾는 데 쓸 포인터다.
    char *separator;

    // meta 파일 경로를 만든다.
    path = build_path(schema_dir, table_name, ".meta");
    if (path == NULL) {
        set_schema_error(&result, "out of memory while building schema path");
        return result;
    }

    // meta 파일을 연다.
    file = fopen(path, "rb");
    // 경로 문자열은 이제 필요 없으므로 해제한다.
    free(path);
    if (file == NULL) {
        set_schema_error(&result, "schema meta file does not exist");
        return result;
    }

    // meta 파일을 끝까지 한 줄씩 읽는다.
    while (fgets(line, sizeof(line), file) != NULL) {
        // 줄바꿈을 지우고 양쪽 공백도 제거한다.
        strip_line_endings(line);
        trimmed = trim_whitespace(line);
        // 빈 줄은 무시한다.
        if (*trimmed == '\0') {
            continue;
        }

        // key=value 형식인지 확인한다.
        separator = strchr(trimmed, '=');
        if (separator == NULL) {
            fclose(file);
            free_schema(&result.schema);
            set_schema_error(&result, "invalid schema meta format");
            return result;
        }

        // '='를 기준으로 key와 value를 분리한다.
        *separator = '\0';
        separator++;
        separator = trim_whitespace(separator);

        // table 키면 테이블 이름을 저장한다.
        if (strcmp(trimmed, "table") == 0) {
            free(result.schema.table_name);
            result.schema.table_name = copy_string(separator);
            if (result.schema.table_name == NULL) {
                fclose(file);
                free_schema(&result.schema);
                set_schema_error(&result, "out of memory while reading schema table name");
                return result;
            }
        // columns 키면 컬럼 목록을 파싱한다.
        } else if (strcmp(trimmed, "columns") == 0) {
            string_list_free(&result.schema.columns);
            if (!parse_columns_value(separator, &result.schema.columns, result.message, sizeof(result.message))) {
                fclose(file);
                free_schema(&result.schema);
                result.ok = 0;
                return result;
            }
        }
    }

    // 파일 읽기가 끝났으므로 닫는다.
    fclose(file);

    // 테이블명이나 컬럼 목록이 빠져 있으면 잘못된 meta 파일이다.
    if (result.schema.table_name == NULL || result.schema.columns.count == 0) {
        free_schema(&result.schema);
        set_schema_error(&result, "schema meta file is missing required fields");
        return result;
    }

    // meta 안의 table 값과 요청한 테이블명이 같은지 확인한다.
    if (strcmp(result.schema.table_name, table_name) != 0) {
        free_schema(&result.schema);
        set_schema_error(&result, "schema table name does not match requested table");
        return result;
    }

    // 같은 이름의 CSV 파일 헤더도 스키마와 맞는지 검증한다.
    if (!validate_csv_header(data_dir, table_name, &result.schema, result.message, sizeof(result.message))) {
        free_schema(&result.schema);
        result.ok = 0;
        return result;
    }

    // 여기까지 통과하면 유효한 스키마다.
    result.ok = 1;
    return result;
}

void free_schema(Schema *schema) {
    // 테이블 이름 문자열을 해제한다.
    free(schema->table_name);
    schema->table_name = NULL;
    // 컬럼 목록 문자열들도 해제한다.
    string_list_free(&schema->columns);
}
