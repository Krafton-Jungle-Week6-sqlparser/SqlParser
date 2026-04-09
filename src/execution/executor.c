// executor는 파싱된 SQL 문장을 실제 파일 읽기/쓰기 동작으로 바꾼다.
#include "sqlparser/execution/executor.h"

// 스키마 검증을 위해 사용한다.
#include "sqlparser/storage/schema.h"
// CSV append와 CSV 파싱 함수를 사용한다.
#include "sqlparser/storage/storage.h"
// 문자열 리스트와 경로 유틸을 사용한다.
#include "sqlparser/common/util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_exec_error(ExecResult *result, const char *message) {
    // 실행 실패 플래그를 기록한다.
    result->ok = 0;
    // 에러 메시지를 결과 구조체에 복사한다.
    snprintf(result->message, sizeof(result->message), "%s", message);
}

static int contains_newline(const char *value) {
    // 현재 버전은 값 안의 줄바꿈을 허용하지 않으므로 검사한다.
    return strchr(value, '\n') != NULL || strchr(value, '\r') != NULL;
}

static int validate_insert_columns(const InsertStatement *statement, const Schema *schema, char *message, size_t message_size) {
    // 컬럼 목록 순회용 인덱스다.
    int index;

    // 컬럼 개수와 값 개수는 반드시 같아야 한다.
    if (statement->columns.count != statement->values.count) {
        snprintf(message, message_size, "column count and value count do not match");
        return 0;
    }

    // INSERT에 적힌 각 컬럼이 실제 스키마에 존재하는지 검사한다.
    for (index = 0; index < statement->columns.count; index++) {
        if (string_list_index_of(&schema->columns, statement->columns.items[index]) < 0) {
            snprintf(message, message_size, "unknown column in INSERT: %s", statement->columns.items[index]);
            return 0;
        }

        // 줄바꿈이 들어간 값은 CSV 단순 구현 범위를 벗어나므로 막는다.
        if (contains_newline(statement->values.items[index])) {
            snprintf(message, message_size, "newline in value is not supported");
            return 0;
        }
    }

    return 1;
}

static ExecResult execute_insert(const InsertStatement *statement, const char *schema_dir, const char *data_dir) {
    // 최종 INSERT 실행 결과다.
    ExecResult result = {0};
    // 요청한 테이블의 스키마 로딩 결과다.
    SchemaResult schema_result;
    // 스키마 전체 컬럼 순서에 맞춘 최종 한 줄 데이터다.
    StringList row_values = {0};
    // 어떤 컬럼이 이미 채워졌는지 표시하는 배열이다.
    int *assigned = NULL;
    // 스키마 컬럼 순회용 인덱스다.
    int schema_index;
    // 입력된 INSERT 컬럼/값 순회용 인덱스다.
    int value_index;
    // 실제 CSV append 결과다.
    StorageResult storage_result;

    // 먼저 테이블 스키마와 CSV 헤더가 정상인지 확인한다.
    schema_result = load_schema(schema_dir, data_dir, statement->table_name);
    if (!schema_result.ok) {
        set_exec_error(&result, schema_result.message);
        return result;
    }

    // 컬럼 수, 존재 여부, 줄바꿈 제한을 검증한다.
    if (!validate_insert_columns(statement, &schema_result.schema, result.message, sizeof(result.message))) {
        free_schema(&schema_result.schema);
        return result;
    }

    // 중복 컬럼 체크를 위한 배열을 0으로 초기화해 만든다.
    assigned = (int *)calloc((size_t)schema_result.schema.columns.count, sizeof(int));
    if (assigned == NULL) {
        free_schema(&schema_result.schema);
        set_exec_error(&result, "out of memory while preparing INSERT row");
        return result;
    }

    // 스키마 전체 컬럼 수만큼 기본값 "" 를 먼저 채운다.
    for (schema_index = 0; schema_index < schema_result.schema.columns.count; schema_index++) {
        if (!string_list_push(&row_values, "")) {
            free(assigned);
            string_list_free(&row_values);
            free_schema(&schema_result.schema);
            set_exec_error(&result, "out of memory while preparing INSERT row");
            return result;
        }
    }

    // 사용자가 명시한 컬럼만 스키마 위치에 맞게 덮어쓴다.
    for (value_index = 0; value_index < statement->columns.count; value_index++) {
        schema_index = string_list_index_of(&schema_result.schema.columns, statement->columns.items[value_index]);
        if (schema_index < 0) {
            free(assigned);
            string_list_free(&row_values);
            free_schema(&schema_result.schema);
            set_exec_error(&result, "unknown column in INSERT");
            return result;
        }

        if (assigned[schema_index]) {
            free(assigned);
            string_list_free(&row_values);
            free_schema(&schema_result.schema);
            set_exec_error(&result, "duplicate column in INSERT");
            return result;
        }

        // 이 스키마 위치는 이미 값이 채워졌다고 표시한다.
        assigned[schema_index] = 1;
        // 기존 기본값 "" 문자열을 지운다.
        free(row_values.items[schema_index]);
        // 실제 입력된 값을 해당 위치에 복사해 넣는다.
        row_values.items[schema_index] = copy_string(statement->values.items[value_index]);
        if (row_values.items[schema_index] == NULL) {
            free(assigned);
            string_list_free(&row_values);
            free_schema(&schema_result.schema);
            set_exec_error(&result, "out of memory while preparing INSERT row");
            return result;
        }
    }

    // 완성된 한 줄 값을 CSV 파일 끝에 추가한다.
    storage_result = append_row_csv(data_dir, schema_result.schema.storage_name, &row_values);
    // 임시로 만든 메모리들은 여기서 모두 정리한다.
    free(assigned);
    string_list_free(&row_values);
    free_schema(&schema_result.schema);

    // 저장 실패 시 그 메시지를 실행 결과로 넘긴다.
    if (!storage_result.ok) {
        set_exec_error(&result, storage_result.message);
        return result;
    }

    // 성공이면 INSERT 1 형태의 메시지를 채운다.
    result.ok = 1;
    result.affected_rows = storage_result.affected_rows;
    snprintf(result.message, sizeof(result.message), "%s", storage_result.message);
    return result;
}

static int build_select_indexes(const SelectStatement *statement, const Schema *schema, StringList *selected_headers, int *selected_indexes, char *message, size_t message_size) {
    // 컬럼 순회용 인덱스다.
    int index;
    // 스키마 상 실제 컬럼 위치를 저장할 변수다.
    int schema_index;

    // SELECT * 면 스키마 전체 컬럼을 그대로 사용한다.
    if (statement->select_all) {
        for (index = 0; index < schema->columns.count; index++) {
            if (!string_list_push(selected_headers, schema->columns.items[index])) {
                snprintf(message, message_size, "out of memory while preparing SELECT");
                return 0;
            }
            // 출력할 실제 열 위치는 0,1,2,... 그대로다.
            selected_indexes[index] = index;
        }
        return schema->columns.count;
    }

    // 특정 컬럼 조회면 요청한 컬럼을 하나씩 스키마에서 찾는다.
    for (index = 0; index < statement->columns.count; index++) {
        schema_index = string_list_index_of(&schema->columns, statement->columns.items[index]);
        if (schema_index < 0) {
            snprintf(message, message_size, "unknown column in SELECT: %s", statement->columns.items[index]);
            return 0;
        }

        // 헤더용 문자열 목록도 별도로 만든다.
        if (!string_list_push(selected_headers, statement->columns.items[index])) {
            snprintf(message, message_size, "out of memory while preparing SELECT");
            return 0;
        }
        // 실제 CSV row에서 꺼낼 열 위치를 기록한다.
        selected_indexes[index] = schema_index;
    }

    return statement->columns.count;
}

static int resolve_where_index(const SelectStatement *statement, const Schema *schema, int *where_index, char *message, size_t message_size) {
    if (!statement->has_where) {
        *where_index = -1;
        return 1;
    }

    *where_index = string_list_index_of(&schema->columns, statement->where_column);
    if (*where_index < 0) {
        snprintf(message, message_size, "unknown column in WHERE: %s", statement->where_column);
        return 0;
    }

    return 1;
}

static int row_matches_where(const SelectStatement *statement, const StringList *fields, int where_index) {
    if (!statement->has_where) {
        return 1;
    }

    return strcmp(fields->items[where_index], statement->where_value) == 0;
}
static void print_selected_row(FILE *out, const StringList *fields, const int *selected_indexes, int selected_count) {
    // 출력할 열 순회용 인덱스다.
    int index;

    // 선택된 열만 골라 "a | b | c" 형태로 출력한다.
    for (index = 0; index < selected_count; index++) {
        if (index > 0) {
            fprintf(out, " | ");
        }
        fprintf(out, "%s", fields->items[selected_indexes[index]]);
    }
    fprintf(out, "\n");
}

static void print_header_row(FILE *out, const StringList *headers) {
    // 헤더 순회용 인덱스다.
    int index;

    // SELECT 결과의 첫 줄 제목을 출력한다.
    for (index = 0; index < headers->count; index++) {
        if (index > 0) {
            fprintf(out, " | ");
        }
        fprintf(out, "%s", headers->items[index]);
    }
    fprintf(out, "\n");
}

static ExecResult execute_select(const SelectStatement *statement, const char *schema_dir, const char *data_dir, FILE *out) {
    // 최종 SELECT 실행 결과다.
    ExecResult result = {0};
    // 테이블 스키마 로딩 결과다.
    SchemaResult schema_result;
    // 대상 CSV 파일 경로다.
    char *path;
    // CSV 파일 핸들이다.
    FILE *file;
    // 파일을 한 줄씩 읽을 버퍼다.
    char line[4096];
    // 각 데이터 행을 CSV 파싱한 결과다.
    StringList fields = {0};
    // 출력할 헤더 문자열 목록이다.
    StringList headers = {0};
    // 실제 CSV row에서 어떤 열을 꺼낼지 담는 배열이다.
    int *selected_indexes = NULL;
    // 출력할 열 개수다.
    int selected_count;
    // WHERE 비교 대상 컬럼 인덱스다.
    int where_index = -1;
    // 실제로 읽어 출력한 데이터 행 수다.
    int row_count = 0;

    // 먼저 스키마와 CSV 헤더가 정상인지 검사한다.
    schema_result = load_schema(schema_dir, data_dir, statement->table_name);
    if (!schema_result.ok) {
        set_exec_error(&result, schema_result.message);
        return result;
    }

    // 스키마 컬럼 수만큼 인덱스 배열을 만든다.
    selected_indexes = (int *)calloc((size_t)schema_result.schema.columns.count, sizeof(int));
    if (selected_indexes == NULL) {
        free_schema(&schema_result.schema);
        set_exec_error(&result, "out of memory while preparing SELECT");
        return result;
    }

    // SELECT * 또는 특정 컬럼 조회에 맞게 출력 열 구성을 만든다.
    selected_count = build_select_indexes(statement, &schema_result.schema, &headers, selected_indexes, result.message, sizeof(result.message));
    if (selected_count == 0) {
        free(selected_indexes);
        string_list_free(&headers);
        free_schema(&schema_result.schema);
        return result;
    }

    if (!resolve_where_index(statement, &schema_result.schema, &where_index, result.message, sizeof(result.message))) {
        free(selected_indexes);
        string_list_free(&headers);
        free_schema(&schema_result.schema);
        return result;
    }

    // 결과의 첫 줄 헤더를 출력한다.
    print_header_row(out, &headers);

    // 실제 CSV 데이터 파일을 연다.
    path = build_path(data_dir, schema_result.schema.storage_name, ".csv");
    if (path == NULL) {
        free(selected_indexes);
        string_list_free(&headers);
        free_schema(&schema_result.schema);
        set_exec_error(&result, "out of memory while opening table data");
        return result;
    }

    file = fopen(path, "rb");
    free(path);
    if (file == NULL) {
        free(selected_indexes);
        string_list_free(&headers);
        free_schema(&schema_result.schema);
        set_exec_error(&result, "failed to open table data");
        return result;
    }

    // 첫 줄 헤더는 이미 검증했으므로 읽고 버린다.
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        free(selected_indexes);
        string_list_free(&headers);
        free_schema(&schema_result.schema);
        set_exec_error(&result, "table data file is empty");
        return result;
    }

    // 두 번째 줄부터 실제 데이터 행을 차례대로 읽는다.
    while (fgets(line, sizeof(line), file) != NULL) {
        // 줄 끝의 \r, \n 을 지운다.
        strip_line_endings(line);
        // 빈 줄은 건너뛴다.
        if (line[0] == '\0') {
            continue;
        }

        // CSV 한 줄을 필드 목록으로 나눈다.
        if (!csv_parse_line(line, &fields, result.message, sizeof(result.message))) {
            fclose(file);
            free(selected_indexes);
            string_list_free(&headers);
            string_list_free(&fields);
            free_schema(&schema_result.schema);
            result.ok = 0;
            return result;
        }

        // row의 컬럼 수가 schema와 다르면 잘못된 CSV 파일이다.
        if (fields.count != schema_result.schema.columns.count) {
            fclose(file);
            free(selected_indexes);
            string_list_free(&headers);
            string_list_free(&fields);
            free_schema(&schema_result.schema);
            set_exec_error(&result, "CSV row does not match schema column count");
            return result;
        }

        if (!row_matches_where(statement, &fields, where_index)) {
            string_list_free(&fields);
            continue;
        }

        // 선택된 컬럼만 화면에 출력한다.
        print_selected_row(out, &fields, selected_indexes, selected_count);
        // 현재 줄에서 사용한 필드 메모리를 정리한다.
        string_list_free(&fields);
        // 출력한 데이터 행 수를 증가시킨다.
        row_count++;
    }

    // 남은 자원들을 모두 정리한다.
    fclose(file);
    free(selected_indexes);
    string_list_free(&headers);
    free_schema(&schema_result.schema);

    // SELECT n 형태의 결과 메시지를 만든다.
    result.ok = 1;
    result.affected_rows = row_count;
    snprintf(result.message, sizeof(result.message), "SELECT %d", row_count);
    return result;
}

ExecResult execute_statement(const Statement *statement, const char *schema_dir, const char *data_dir, FILE *out) {
    // Statement 종류에 따라 INSERT 실행기 또는 SELECT 실행기를 호출한다.
    if (statement->type == STATEMENT_INSERT) {
        return execute_insert(&statement->as.insert_statement, schema_dir, data_dir);
    }

    return execute_select(&statement->as.select_statement, schema_dir, data_dir, out);
}
