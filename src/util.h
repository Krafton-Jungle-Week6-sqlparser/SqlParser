#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

// 여러 문자열을 동적으로 담기 위한 간단한 리스트 구조체다.
typedef struct {
    // 실제 문자열 포인터들이 들어가는 배열이다.
    char **items;
    // 현재 들어 있는 문자열 개수다.
    int count;
    // 현재 확보해 둔 배열 크기다.
    int capacity;
} StringList;

// 파일 전체를 한 번에 읽어서 '\0'이 붙은 문자열로 돌려준다.
char *read_entire_file(const char *path, char *error, size_t error_size);
// 문자열을 새 메모리에 복사해 돌려준다.
char *copy_string(const char *source);
// 대소문자를 무시하고 두 문자열이 같은지 검사한다.
int strings_equal_ignore_case(const char *left, const char *right);
// 문자열 앞뒤 공백을 제거하고, 공백이 제거된 시작 위치를 반환한다.
char *trim_whitespace(char *text);
// 문자열 끝의 \r, \n 같은 줄바꿈 문자를 없앤다.
void strip_line_endings(char *text);
// StringList 끝에 문자열 하나를 추가한다.
int string_list_push(StringList *list, const char *value);
// StringList 안에서 특정 문자열의 위치를 찾는다.
int string_list_index_of(const StringList *list, const char *value);
// StringList 내부 문자열과 배열 메모리를 모두 해제한다.
void string_list_free(StringList *list);
// dir/name.extension 형태의 경로 문자열을 새로 만들어 돌려준다.
char *build_path(const char *dir, const char *name, const char *extension);

#endif
