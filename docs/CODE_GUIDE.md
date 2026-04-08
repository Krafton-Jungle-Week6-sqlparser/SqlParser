# 파일별 코드 설명서

이 문서는 이 프로젝트를 처음 읽는 사람이 `어떤 파일이 무슨 역할을 하는지`, `어떤 순서로 읽으면 이해하기 쉬운지`를 빠르게 파악할 수 있도록 만든 안내서입니다.

## 1. 먼저 어떻게 읽으면 좋은가

처음 읽을 때는 아래 순서를 추천합니다.

1. `src/app/main.c`
프로그램이 어디서 시작되는지 봅니다.

2. `src/sql/lexer.c` / `src/sql/parser.c`
SQL 문장이 어떻게 구조로 바뀌는지 봅니다.

3. `src/execution/executor.c`
구조화된 SQL이 실제 동작으로 어떻게 연결되는지 봅니다.

4. `src/storage/schema.c` / `src/storage/storage.c`
테이블 규칙과 CSV 저장 규칙을 봅니다.

5. `tests/test_runner.c`
무엇을 검증했는지 확인합니다.

## 2. 프로젝트 전체 흐름

핵심 흐름은 아래와 같습니다.

`SQL 파일 읽기 -> 토큰 분리 -> 문장 해석 -> 실행 -> CSV 저장/조회`

### 시퀀스 다이어그램 1: 전체 실행 흐름

![전체 실행 흐름 시퀀스 다이어그램](./diagrams/code-guide-sequence-01-overall.svg)

## 3. 디렉터리별 의미

### `include/sqlparser/`

프로젝트의 공개 헤더를 역할별로 분리해 둔 곳입니다.

- `common/`
- `sql/`
- `storage/`
- `execution/`

확장할 때 새 기능의 공개 인터페이스를 추가하기 좋습니다.

### `src/app/`

프로그램 진입점이 들어 있습니다.

- `main.c`

### `src/common/`

여러 모듈이 함께 쓰는 공통 함수가 들어 있습니다.

- `util.c`

### `src/sql/`

SQL 해석과 관련된 코드가 들어 있습니다.

- `ast.c`
- `lexer.c`
- `parser.c`

### `src/storage/`

파일 기반 DB 규칙과 저장 형식을 다루는 코드가 들어 있습니다.

- `schema.c`
- `storage.c`

### `src/execution/`

파싱된 SQL을 실제 동작으로 연결하는 코드가 들어 있습니다.

- `executor.c`

### `docs/`

설명 문서가 들어 있습니다.

- 구현 계획서
- 코드 설명서

### `build/`

실행 파일과 테스트 임시 파일이 들어가는 공간입니다.

- `build/bin`
- `build/tests`

## 4. 파일별 역할 설명

### `src/app/main.c`

프로그램의 시작점입니다.

- 명령행 인자 확인
- SQL 파일 읽기
- lexer 호출
- parser 호출
- executor 호출
- 결과 출력

즉, 전체 흐름을 연결하는 `지휘자` 역할입니다.

### `src/common/util.c`

프로젝트 전체에서 공통으로 쓰는 기초 함수들을 모아 둔 파일입니다.

- 파일 전체 읽기
- 문자열 복사
- 공백 제거
- 문자열 리스트 관리
- 경로 문자열 생성

### `src/sql/lexer.c`

SQL 문자열을 토큰 단위로 나눕니다.

- 식별자
- 문자열
- 숫자
- 쉼표
- 괄호
- 세미콜론

즉, 문장을 바로 이해하지 않고 먼저 `읽기 쉬운 조각`으로 자르는 단계입니다.

### 시퀀스 다이어그램 2: SQL 파싱 전반

![SQL 파싱 전반 시퀀스 다이어그램](./diagrams/code-guide-sequence-02-parsing.svg)

### `src/sql/parser.c`

토큰 목록을 읽어 실제 SQL 의미로 해석합니다.

현재 지원:

- `INSERT`
- `SELECT`

하는 일:

- 첫 키워드 확인
- 테이블명 읽기
- 컬럼 목록 읽기
- 값 목록 읽기
- 세미콜론 확인
- 최종 `Statement` 생성

### `src/sql/ast.c`

파싱 결과 구조체의 메모리를 정리합니다.

역할은 작지만 중요합니다.  
문자열과 리스트를 많이 만들기 때문에 마지막 정리가 필요합니다.

### `src/storage/schema.c`

테이블이 정상적으로 존재하는지 검사합니다.

검사 항목:

- `schema/<table>.meta` 존재 여부
- `data/<table>.csv` 존재 여부
- CSV 헤더와 meta 파일 컬럼 순서 일치 여부

### `src/storage/storage.c`

CSV 규칙을 실제로 처리합니다.

- CSV 한 줄 파싱
- CSV escape
- CSV 파일 끝에 새 행 추가

예:

- 원본: `hello, "world"`
- 저장: `"hello, ""world"""`

### `src/execution/executor.c`

parser가 만든 `Statement` 를 실제 동작으로 바꾸는 핵심 파일입니다.

두 갈래로 나뉩니다.

- `INSERT` 실행
- `SELECT` 실행

### 시퀀스 다이어그램 3: INSERT 실행 흐름

![INSERT 실행 흐름 시퀀스 다이어그램](./diagrams/code-guide-sequence-03-insert.svg)

### 시퀀스 다이어그램 4: SELECT 실행 흐름

![SELECT 실행 흐름 시퀀스 다이어그램](./diagrams/code-guide-sequence-04-select.svg)

### `tests/test_runner.c`

자동 테스트를 모아 둔 실행 파일입니다.

주요 테스트:

- lexer 테스트
- parser 테스트
- schema 테스트
- INSERT 실행 테스트
- SELECT 실행 테스트
- CSV escape 테스트

### 시퀀스 다이어그램 5: 테스트 실행 흐름

![테스트 실행 흐름 시퀀스 다이어그램](./diagrams/code-guide-sequence-05-tests.svg)

## 5. 한 줄 요약

이 프로젝트는 구조를 아래처럼 나눠 두었습니다.

- `app`: 시작점
- `common`: 공통 도구
- `sql`: SQL 해석
- `storage`: 파일 저장 규칙
- `execution`: 실제 실행
- `tests`: 검증
- `docs`: 설명 문서
- `build`: 산출물

즉, `읽기 -> 해석 -> 실행 -> 저장 -> 검증` 흐름을 디렉터리 구조 자체로 드러내도록 정리한 구조입니다.
