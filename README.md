# SqlParser

C로 만든 작은 파일 기반 SQL 처리기입니다. `INSERT` 와 `SELECT` 문장을 읽어 `schema/` 와 `data/` 폴더의 파일을 기준으로 실행합니다.

이 프로젝트는 "SQL 문자열을 해석해서 CSV 파일을 작은 데이터베이스처럼 다루는 프로그램"으로 이해하면 가장 쉽습니다.

## 한눈에 보는 흐름

프로그램은 아래 순서로 동작합니다.

`입력 -> lexer -> parser -> executor -> schema/storage -> 출력`

- 입력
  SQL 파일 경로나 SQL 문장을 직접 받습니다.
- lexer
  SQL 문장을 토큰으로 잘게 나눕니다.
- parser
  토큰을 읽어 `INSERT` 또는 `SELECT` 구조로 해석합니다.
- executor
  해석된 결과를 실제 파일 동작으로 바꿉니다.
- schema/storage
  테이블 구조를 검사하고 CSV 파일을 읽거나 씁니다.

![SqlParser function overview](./docs/sqlparser_function_overview.svg)

## 지원 기능

- `INSERT INTO table_name (col1, col2, ...) VALUES (val1, val2, ...);`
- `SELECT * FROM table_name;`
- `SELECT col1, col2 FROM table_name;`
- SQL 파일 입력
- SQL 문장 직접 입력

아직 지원하지 않는 기능:

- `WHERE`
- `JOIN`
- `UPDATE`
- `DELETE`
- `CREATE TABLE`

## 프로젝트 구조

```txt
.
|-- data/
|-- docs/
|-- examples/
|-- include/
|   `-- sqlparser/
|       |-- common/
|       |-- execution/
|       |-- sql/
|       `-- storage/
|-- schema/
|-- src/
|   |-- app/
|   |-- common/
|   |-- execution/
|   |-- sql/
|   `-- storage/
|-- tests/
|-- Makefile
`-- README.md
```

각 디렉터리 역할은 아래와 같습니다.

- `src/app`
  프로그램 시작점과 CLI 입력 처리를 담당합니다.
- `src/common`
  문자열, 파일, 리스트, 경로 생성 같은 공용 도구를 모아둡니다.
- `src/sql`
  SQL 토큰화, 문장 해석, AST 메모리 정리를 담당합니다.
- `src/execution`
  해석된 SQL을 실제 `INSERT` / `SELECT` 동작으로 연결합니다.
- `src/storage`
  스키마 검사와 CSV 읽기/쓰기를 담당합니다.
- `include/sqlparser`
  각 모듈의 공개 헤더 파일이 들어 있습니다.
- `schema`
  테이블 구조 설명 파일이 들어 있습니다.
- `data`
  실제 CSV 데이터가 저장됩니다.
- `examples`
  실행해볼 수 있는 예제 SQL 파일이 들어 있습니다.
- `docs`
  설계 문서와 안내 문서가 들어 있습니다.
- `tests`
  자동 테스트 코드가 들어 있습니다.

## 처음 읽는 순서

코드를 처음 볼 때는 아래 순서가 가장 이해하기 쉽습니다.

1. [src/app/main.c](/C:/Users/home/workspace/jungle/wedcoding/week6/sqlparser/.codex-main-pr/src/app/main.c)
2. [src/sql/lexer.c](/C:/Users/home/workspace/jungle/wedcoding/week6/sqlparser/.codex-main-pr/src/sql/lexer.c)
3. [src/sql/parser.c](/C:/Users/home/workspace/jungle/wedcoding/week6/sqlparser/.codex-main-pr/src/sql/parser.c)
4. [src/execution/executor.c](/C:/Users/home/workspace/jungle/wedcoding/week6/sqlparser/.codex-main-pr/src/execution/executor.c)
5. [src/storage/schema.c](/C:/Users/home/workspace/jungle/wedcoding/week6/sqlparser/.codex-main-pr/src/storage/schema.c)
6. [src/storage/storage.c](/C:/Users/home/workspace/jungle/wedcoding/week6/sqlparser/.codex-main-pr/src/storage/storage.c)
7. [tests/test_runner.c](/C:/Users/home/workspace/jungle/wedcoding/week6/sqlparser/.codex-main-pr/tests/test_runner.c)

이 순서대로 보면 "입력 -> 해석 -> 실행 -> 저장 -> 검증" 흐름이 자연스럽게 이어집니다.

## 파일별 역할

### `src/app/main.c`

프로그램의 시작점입니다.

- 명령줄 인자를 읽습니다.
- SQL 파일 경로인지, 직접 입력한 SQL 문장인지 구분합니다.
- `lex_sql()` 을 호출합니다.
- `parse_statement()` 를 호출합니다.
- `execute_statement()` 를 호출합니다.
- 최종 결과 메시지를 출력합니다.

즉, 전체 흐름을 연결하는 지휘자 역할입니다.

### `src/common/util.c`

공용 유틸리티 함수 모음입니다.

- 파일 전체 읽기
- 문자열 복사
- 공백 제거
- 줄 끝 개행 제거
- 문자열 리스트 관리
- 경로 문자열 생성

여러 모듈이 함께 쓰는 기본 도구 상자라고 생각하면 됩니다.

### `src/sql/lexer.c`

SQL 문장을 토큰으로 나눕니다.

예를 들어:

```sql
SELECT name, age FROM users;
```

는 대략 아래 같은 토큰으로 나뉩니다.

- `SELECT`
- `name`
- `,`
- `age`
- `FROM`
- `users`
- `;`

즉 긴 문장을 먼저 "읽기 쉬운 조각"으로 나누는 단계입니다.

### `src/sql/parser.c`

토큰 목록을 읽어서 SQL의 의미를 해석합니다.

현재는 아래 두 종류를 해석합니다.

- `INSERT`
- `SELECT`

parser가 해석한 결과는 AST 구조체로 정리됩니다.

### `src/sql/ast.c`

parser가 만든 AST 안의 동적 메모리를 정리합니다.

역할은 작아 보이지만 중요합니다. `table_name`, `columns`, `values` 같은 동적 메모리를 안전하게 해제해야 누수가 생기지 않습니다.

### `src/storage/schema.c`

테이블 구조가 정상인지 확인합니다.

주요 검사 항목:

- `schema/<table>.meta` 파일 존재 여부
- `data/<table>.csv` 파일 존재 여부
- 메타 파일 안의 `table`, `columns` 정보 존재 여부
- CSV 헤더와 스키마 컬럼 순서 일치 여부

즉 "이 테이블을 정말 읽거나 써도 되는 상태인가?"를 확인하는 문지기 역할입니다.

### `src/storage/storage.c`

CSV 파일을 실제로 다룹니다.

- CSV 한 줄 파싱
- CSV escape 처리
- CSV 파일 끝에 행 추가

예를 들어 값 안에 쉼표나 큰따옴표가 들어가면 CSV 규칙에 맞게 감싸고 escape 합니다.

### `src/execution/executor.c`

parser가 만든 AST를 실제 동작으로 바꿉니다.

- `INSERT`
  스키마를 확인하고, 컬럼 순서에 맞는 한 행을 만들어 CSV에 추가합니다.
- `SELECT`
  CSV를 읽고, 요청한 컬럼만 골라 출력합니다.

즉 해석된 SQL을 현실적인 파일 작업으로 연결하는 모듈입니다.

### `tests/test_runner.c`

자동 테스트 실행 파일입니다.

주요 검증 범위:

- lexer 동작
- parser 동작
- schema 검사
- `INSERT` 실행
- `SELECT` 실행
- CSV escape 처리

## AST와 실행 흐름

이 프로젝트에서 AST는 "SQL 해석 결과를 담는 중간 구조체"입니다.

예를 들어:

```sql
INSERT INTO users (id, name, age) VALUES (1, 'kim', 20);
```

를 해석하면 대략 이런 정보가 만들어집니다.

- 문장 종류: `INSERT`
- 테이블 이름: `users`
- 컬럼 목록: `id`, `name`, `age`
- 값 목록: `1`, `kim`, `20`

이 구조를 정의한 파일은 [include/sqlparser/sql/ast.h](/C:/Users/home/workspace/jungle/wedcoding/week6/sqlparser/.codex-main-pr/include/sqlparser/sql/ast.h) 입니다.

## 시퀀스 다이어그램

세부 흐름을 그림으로 보고 싶다면 아래 다이어그램을 보면 됩니다.

- 전체 실행 흐름: [docs/diagrams/code-guide-sequence-01-overall.svg](./docs/diagrams/code-guide-sequence-01-overall.svg)
- SQL 파싱 흐름: [docs/diagrams/code-guide-sequence-02-parsing.svg](./docs/diagrams/code-guide-sequence-02-parsing.svg)
- INSERT 실행 흐름: [docs/diagrams/code-guide-sequence-03-insert.svg](./docs/diagrams/code-guide-sequence-03-insert.svg)
- SELECT 실행 흐름: [docs/diagrams/code-guide-sequence-04-select.svg](./docs/diagrams/code-guide-sequence-04-select.svg)
- 테스트 실행 흐름: [docs/diagrams/code-guide-sequence-05-tests.svg](./docs/diagrams/code-guide-sequence-05-tests.svg)

## 테이블 존재 규칙

테이블은 아래 두 파일이 모두 있어야 존재하는 것으로 봅니다.

- `schema/<table_name>.meta`
- `data/<table_name>.csv`

예시:

```txt
table=users
columns=id,name,age
```

위 구조라면 CSV 파일의 첫 줄도 `id,name,age` 여야 합니다.

## CSV 규칙

이 프로젝트는 CSV를 아래 규칙으로 다룹니다.

- 첫 줄은 헤더입니다.
- 헤더는 메타 파일의 컬럼 순서와 같아야 합니다.
- 값 안에 쉼표가 있으면 큰따옴표로 감쌉니다.
- 값 안의 큰따옴표는 `""` 로 escape 합니다.
- 빈 값은 빈 문자열로 저장합니다.
- 값 안의 줄바꿈은 현재 지원하지 않습니다.
- 부분 컬럼 `INSERT` 를 허용하며, 빠진 컬럼은 빈 문자열로 채웁니다.

## 빌드 방법

### Windows / PowerShell

프로그램 빌드:

```powershell
gcc -Wall -Wextra -std=c11 -Iinclude -o build/bin/sqlparser.exe src/app/main.c src/common/util.c src/storage/schema.c src/storage/storage.c src/sql/ast.c src/sql/lexer.c src/sql/parser.c src/execution/executor.c
```

테스트 빌드:

```powershell
gcc -Wall -Wextra -std=c11 -Iinclude -o build/bin/test_runner.exe tests/test_runner.c src/common/util.c src/storage/schema.c src/storage/storage.c src/sql/ast.c src/sql/lexer.c src/sql/parser.c src/execution/executor.c
```

Makefile 사용:

```powershell
mingw32-make
mingw32-make test
```

### Dev Container / bash

현재 `Makefile` 은 Windows 명령을 기준으로 작성되어 있어서 Dev Container bash에서는 직접 `gcc` 명령을 쓰는 편이 안전합니다.

프로그램 빌드:

```bash
mkdir -p build/bin

gcc -Wall -Wextra -std=c11 -Iinclude \
  -o build/bin/sqlparser \
  src/app/main.c \
  src/common/util.c \
  src/storage/schema.c \
  src/storage/storage.c \
  src/sql/ast.c \
  src/sql/lexer.c \
  src/sql/parser.c \
  src/execution/executor.c
```

테스트 빌드:

```bash
mkdir -p build/bin

gcc -Wall -Wextra -std=c11 -Iinclude \
  -o build/bin/test_runner \
  tests/test_runner.c \
  src/common/util.c \
  src/storage/schema.c \
  src/storage/storage.c \
  src/sql/ast.c \
  src/sql/lexer.c \
  src/sql/parser.c \
  src/execution/executor.c
```

## 실행 방법

### SQL 파일 실행

Windows:

```powershell
.\build\bin\sqlparser.exe .\examples\select_all_users.sql
```

bash:

```bash
./build/bin/sqlparser ./examples/select_all_users.sql
```

### SQL 문장 직접 실행

Windows:

```powershell
.\build\bin\sqlparser.exe "SELECT * FROM users;"
.\build\bin\sqlparser.exe "SELECT name, age FROM users;"
.\build\bin\sqlparser.exe "INSERT INTO users (id, name, age) VALUES (1, 'kim', 20);"
```

bash:

```bash
./build/bin/sqlparser "SELECT * FROM users;"
./build/bin/sqlparser "SELECT name, age FROM users;"
./build/bin/sqlparser "INSERT INTO users (id, name, age) VALUES (1, 'kim', 20);"
```

여러 인자를 띄어 써도 내부에서 다시 하나의 SQL 문장으로 합쳐 처리합니다.

```bash
./build/bin/sqlparser SELECT name, age FROM users;
```

다만 `*` 는 셸이 먼저 해석할 수 있으므로 `SELECT *` 는 큰따옴표로 감싸는 것이 안전합니다.

## 테스트 실행

Windows:

```powershell
.\build\bin\test_runner.exe
```

bash:

```bash
./build/bin/test_runner
```

## 예제 SQL

- [examples/insert_users.sql](/C:/Users/home/workspace/jungle/wedcoding/week6/sqlparser/.codex-main-pr/examples/insert_users.sql)
- [examples/select_name_age.sql](/C:/Users/home/workspace/jungle/wedcoding/week6/sqlparser/.codex-main-pr/examples/select_name_age.sql)
- [examples/select_all_users.sql](/C:/Users/home/workspace/jungle/wedcoding/week6/sqlparser/.codex-main-pr/examples/select_all_users.sql)

## 관련 문서

- 구현 계획 PDF: [docs/sql_processor_plan.pdf](./docs/sql_processor_plan.pdf)
- 구현 계획 HTML: [docs/sql_processor_plan.html](./docs/sql_processor_plan.html)
- 초보자용 입문 문서: [docs/BEGINNER_GUIDE_KO.md](./docs/BEGINNER_GUIDE_KO.md)

## 단계별 브랜치

학습 흐름을 위해 아래 단계별 브랜치를 운영합니다.

- `step/01-foundation`
- `step/02-schema-storage`
- `step/03-parser`
- `step/04-execution`
- `step/05-quality`
- `step/06-readme-demo`
- `main`

`main` 은 각 단계 결과를 모은 최종 통합 브랜치입니다.
