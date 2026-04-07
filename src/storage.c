// storage.c는 CSV 파싱과 CSV 파일 append를 담당한다.
#include "storage.h"

// 파일 입출력을 위해 포함한다.
#include <stdio.h>
// 동적 메모리 할당을 위해 포함한다.
#include <stdlib.h>
// strlen을 쓰기 위해 포함한다.
#include <string.h>

static int append_character(char **buffer, size_t *length, size_t *capacity, char value) {
    // realloc 결과를 잠시 담을 포인터다.
    char *new_buffer;
    // 버퍼를 늘릴 때 새 용량이다.
    size_t new_capacity;

    // 문자를 하나 더 넣기 전에 버퍼가 충분한지 확인한다.
    if (*length + 1 >= *capacity) {
        new_capacity = *capacity == 0 ? 16 : *capacity * 2;
        new_buffer = (char *)realloc(*buffer, new_capacity);
        if (new_buffer == NULL) {
            return 0;
        }

        // 늘어난 버퍼 주소와 용량을 반영한다.
        *buffer = new_buffer;
        *capacity = new_capacity;
    }

    // 새 문자를 현재 끝 위치에 쓴다.
    (*buffer)[*length] = value;
    // 사용 중인 길이를 하나 증가시킨다.
    (*length)++;
    // 문자열 끝 표시도 항상 유지한다.
    (*buffer)[*length] = '\0';
    return 1;
}

int csv_parse_line(const char *line, StringList *fields, char *error, size_t error_size) {
    // 현재 따옴표 안쪽을 읽고 있는지 표시한다.
    int in_quotes;
    // 방금 따옴표를 닫았는지 표시한다.
    int just_closed_quote;
    // 현재 필드 문자열을 임시로 쌓아 가는 버퍼다.
    char *buffer;
    // 현재 필드 길이다.
    size_t length;
    // 현재 필드 버퍼 용량이다.
    size_t capacity;
    // line 문자열을 한 글자씩 읽어 갈 포인터다.
    const char *cursor;
    // 공백 제거 후 실제 저장할 문자열이다.
    char *trimmed;

    // 파싱 시작 전 상태를 초기화한다.
    in_quotes = 0;
    just_closed_quote = 0;
    buffer = NULL;
    length = 0;
    capacity = 0;

    // 문자열 끝('\0')까지 한 글자씩 확인한다.
    for (cursor = line; ; cursor++) {
        char current = *cursor;

        // 따옴표 안쪽이라면 쉼표도 일반 문자로 취급해야 한다.
        if (in_quotes) {
            if (current == '"') {
                // "" 는 따옴표 문자 하나를 뜻한다.
                if (cursor[1] == '"') {
                    if (!append_character(&buffer, &length, &capacity, '"')) {
                        free(buffer);
                        snprintf(error, error_size, "out of memory while parsing CSV");
                        return 0;
                    }
                    // 두 번째 따옴표도 함께 소비했으니 한 칸 더 넘긴다.
                    cursor++;
                } else {
                    // 큰따옴표 하나만 나오면 quoted field 종료다.
                    in_quotes = 0;
                    just_closed_quote = 1;
                }
            } else if (current == '\0') {
                // 따옴표를 닫지 못한 채 줄이 끝나면 잘못된 CSV다.
                free(buffer);
                snprintf(error, error_size, "unterminated quoted CSV field");
                return 0;
            } else {
                // 일반 문자는 그대로 현재 필드 버퍼에 추가한다.
                if (!append_character(&buffer, &length, &capacity, current)) {
                    free(buffer);
                    snprintf(error, error_size, "out of memory while parsing CSV");
                    return 0;
                }
            }
            continue;
        }

        // 쉼표나 문자열 끝을 만나면 필드 하나가 끝난 것이다.
        if (current == '\0' || current == ',') {
            // 값이 전혀 없는 빈 필드라면 빈 문자열로 처리한다.
            if (buffer == NULL) {
                buffer = copy_string("");
                if (buffer == NULL) {
                    snprintf(error, error_size, "out of memory while parsing CSV");
                    return 0;
                }
            }

            // quoted field가 아니었다면 앞뒤 공백을 제거한다.
            trimmed = just_closed_quote ? buffer : trim_whitespace(buffer);
            // 완성된 필드를 결과 리스트에 추가한다.
            if (!string_list_push(fields, trimmed)) {
                free(buffer);
                snprintf(error, error_size, "out of memory while parsing CSV");
                return 0;
            }

            // 다음 필드를 위해 임시 버퍼 상태를 초기화한다.
            free(buffer);
            buffer = NULL;
            length = 0;
            capacity = 0;
            just_closed_quote = 0;

            // 줄 끝이면 전체 파싱이 끝난다.
            if (current == '\0') {
                break;
            }

            continue;
        }

        // 따옴표가 필드 맨 앞에 나오면 quoted field 시작으로 본다.
        if (current == '"' && buffer == NULL) {
            in_quotes = 1;
            continue;
        }

        // 따옴표를 닫은 뒤에는 공백 외 문자가 나오면 잘못된 형식이다.
        if (just_closed_quote && current != ' ' && current != '\t') {
            free(buffer);
            snprintf(error, error_size, "unexpected character after quoted CSV field");
            return 0;
        }

        // 줄바꿈 문자는 무시하고, 일반 문자는 필드에 추가한다.
        if (current != '\r' && current != '\n') {
            if (!append_character(&buffer, &length, &capacity, current)) {
                free(buffer);
                snprintf(error, error_size, "out of memory while parsing CSV");
                return 0;
            }
        }
    }

    return 1;
}

char *csv_escape_field(const char *value) {
    // 이 값을 큰따옴표로 감싸야 하는지 여부다.
    int needs_quotes;
    // 문자열 순회용 인덱스다.
    size_t index;
    // 내부 따옴표가 몇 개 더 추가될지 센다.
    size_t extra_quotes;
    // 원본 문자열 길이다.
    size_t length;
    // 최종 escape된 문자열 버퍼다.
    char *escaped;
    // escaped 버퍼에 쓰는 현재 위치다.
    size_t write_index;

    // 시작 상태는 "따옴표 없이 써도 됨"이다.
    needs_quotes = 0;
    extra_quotes = 0;
    length = strlen(value);

    // 빈 문자열은 CSV에서 "" 로 저장해야 한다.
    if (length == 0) {
        return copy_string("\"\"");
    }

    // 쉼표, 따옴표, 줄바꿈이 있으면 quoted field가 필요하다.
    for (index = 0; index < length; index++) {
        if (value[index] == '"' || value[index] == ',' || value[index] == '\n' || value[index] == '\r') {
            needs_quotes = 1;
        }

        // 내부 따옴표는 하나 더 복사해야 하므로 개수를 센다.
        if (value[index] == '"') {
            extra_quotes++;
        }
    }

    // escape가 필요 없으면 그냥 복사본만 돌려준다.
    if (!needs_quotes) {
        return copy_string(value);
    }

    // 양쪽 큰따옴표 2개와 내부 추가 따옴표 수까지 고려해 메모리를 잡는다.
    escaped = (char *)malloc(length + extra_quotes + 3);
    if (escaped == NULL) {
        return NULL;
    }

    // 시작 큰따옴표를 넣는다.
    write_index = 0;
    escaped[write_index++] = '"';
    // 원본 문자열을 한 글자씩 복사한다.
    for (index = 0; index < length; index++) {
        // 내부 큰따옴표는 하나 더 써서 escape 한다.
        if (value[index] == '"') {
            escaped[write_index++] = '"';
        }

        escaped[write_index++] = value[index];
    }
    // 마지막에도 닫는 큰따옴표를 넣는다.
    escaped[write_index++] = '"';
    escaped[write_index] = '\0';

    return escaped;
}

StorageResult append_row_csv(const char *data_dir, const char *table_name, const StringList *row_values) {
    // 최종 반환할 저장 결과다.
    StorageResult result = {0};
    // data/<table>.csv 경로 문자열이다.
    char *path;
    // CSV 파일 핸들이다.
    FILE *file;
    // 컬럼 순회용 인덱스다.
    int index;
    // 파일 전체 크기다.
    long file_size;
    // 파일 마지막 글자를 확인하기 위한 변수다.
    int last_character;

    // 대상 CSV 파일 경로를 만든다.
    path = build_path(data_dir, table_name, ".csv");
    if (path == NULL) {
        snprintf(result.message, sizeof(result.message), "out of memory while building table path");
        return result;
    }

    // 파일 끝에 이어 쓰기 위해 읽기/쓰기 모드로 연다.
    file = fopen(path, "r+b");
    free(path);
    if (file == NULL) {
        snprintf(result.message, sizeof(result.message), "failed to open table file for append");
        return result;
    }

    // 파일 끝으로 이동해 현재 상태를 확인한다.
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        snprintf(result.message, sizeof(result.message), "failed to seek table file");
        return result;
    }

    // 파일 크기를 알아야 비어 있는 파일인지 판단할 수 있다.
    file_size = ftell(file);
    if (file_size < 0) {
        fclose(file);
        snprintf(result.message, sizeof(result.message), "failed to measure table file");
        return result;
    }

    // 파일에 헤더라도 이미 있다면 마지막 문자가 줄바꿈인지 확인한다.
    if (file_size > 0) {
        if (fseek(file, -1L, SEEK_END) != 0) {
            fclose(file);
            snprintf(result.message, sizeof(result.message), "failed to inspect table file");
            return result;
        }

        // 마지막 글자를 읽는다.
        last_character = fgetc(file);
        if (fseek(file, 0, SEEK_END) != 0) {
            fclose(file);
            snprintf(result.message, sizeof(result.message), "failed to rewind table file");
            return result;
        }

        // 마지막이 줄바꿈이 아니라면 새 행을 위해 줄바꿈을 먼저 넣는다.
        if (last_character != '\n') {
            fputc('\n', file);
        }
    }

    // 컬럼 값들을 차례대로 CSV 한 줄로 기록한다.
    for (index = 0; index < row_values->count; index++) {
        // CSV 규칙에 맞게 값을 escape 한다.
        char *escaped = csv_escape_field(row_values->items[index]);
        if (escaped == NULL) {
            fclose(file);
            snprintf(result.message, sizeof(result.message), "out of memory while writing CSV row");
            return result;
        }

        // 첫 번째 컬럼이 아니면 앞에 쉼표를 쓴다.
        if (index > 0) {
            fputc(',', file);
        }
        // escape된 필드 문자열을 파일에 쓴다.
        fputs(escaped, file);
        free(escaped);
    }

    // 파일 쓰기를 끝냈으니 닫는다.
    fclose(file);
    // INSERT 성공 결과를 채운다.
    result.ok = 1;
    result.affected_rows = 1;
    snprintf(result.message, sizeof(result.message), "INSERT 1");
    return result;
}
