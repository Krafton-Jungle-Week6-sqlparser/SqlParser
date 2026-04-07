// schema는 테이블 구조가 올바른지 확인하는 역할을 한다.
#ifndef SCHEMA_H
#define SCHEMA_H

#include "sqlparser/common/util.h"

// 테이블 하나의 스키마 정보를 담는 구조체다.
typedef struct {
    // 테이블 이름이다.
    char *table_name;
    // 컬럼 이름 목록이다.
    StringList columns;
} Schema;

// 스키마 로딩 결과와 에러 메시지를 함께 담는 구조체다.
typedef struct {
    // 1이면 로딩 성공, 0이면 실패다.
    int ok;
    // 성공했을 때 채워지는 스키마 데이터다.
    Schema schema;
    // 실패 이유를 담는 문자열이다.
    char message[256];
} SchemaResult;

// meta 파일과 CSV 헤더를 함께 검사해 스키마를 로딩한다.
SchemaResult load_schema(const char *schema_dir, const char *data_dir, const char *table_name);
// Schema 내부 동적 메모리를 해제한다.
void free_schema(Schema *schema);

#endif
