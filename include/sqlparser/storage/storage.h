// storage는 CSV 파싱과 CSV 파일 쓰기를 담당한다.
#ifndef STORAGE_H
#define STORAGE_H

#include "sqlparser/common/util.h"

#include <stddef.h>

// 파일 저장 단계의 성공/실패 결과를 담는 구조체다.
typedef struct {
    // 1이면 성공, 0이면 실패다.
    int ok;
    // INSERT처럼 영향을 준 행 수를 담는다.
    int affected_rows;
    // 실패 또는 요약 메시지를 담는다.
    char message[256];
} StorageResult;

// CSV 한 줄을 읽어 필드 리스트로 나눈다.
int csv_parse_line(const char *line, StringList *fields, char *error, size_t error_size);
// 문자열 하나를 CSV 규칙에 맞게 escape 해서 새 문자열로 돌려준다.
char *csv_escape_field(const char *value);
// 한 행 분량의 값을 CSV 파일 끝에 추가한다.
StorageResult append_row_csv(const char *data_dir, const char *table_name, const StringList *row_values);

#endif
