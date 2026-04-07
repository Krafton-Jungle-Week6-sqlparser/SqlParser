# SqlParser

C 언어로 구현한 파일 기반 SQL 처리기입니다.  
텍스트 파일로 전달된 SQL을 읽고, `INSERT` 와 `SELECT` 를 파싱한 뒤, CSV 파일 기반 테이블에 대해 실제 저장과 조회를 수행합니다.

계획서는 [sql_processor_plan.pdf](./sql_processor_plan.pdf) 와 [sql_processor_plan.html](./sql_processor_plan.html) 에 포함되어 있습니다.

## 1. 프로젝트 목표

- SQL 입력을 읽고 해석하는 처리기 흐름 이해
- 파일 기반 DB 구조 설계
- CLI 환경에서 실제 동작하는 SQL 실행기 구현
- 테스트와 검증 과정을 포함한 발표 가능한 결과물 완성

## 2. 지원 기능

- `INSERT INTO table_name (col1, col2, ...) VALUES (val1, val2, ...);`
- `SELECT * FROM table_name;`
- `SELECT col1, col2 FROM table_name;`

현재 버전에서는 아래 기능은 제외했습니다.

- `WHERE`
- `JOIN`
- `UPDATE`
- `DELETE`
- `CREATE TABLE`

## 3. 저장 방식

### 테이블 존재 규칙

테이블은 아래 두 파일이 모두 있을 때 존재하는 것으로 간주합니다.

- `schema/<table_name>.meta`
- `data/<table_name>.csv`

예시:

```txt
table=users
columns=id,name,age
```

### CSV 규칙

- 첫 줄은 헤더이며 컬럼 순서를 나타냅니다.
- 헤더는 반드시 `meta` 파일의 컬럼 순서와 같아야 합니다.
- 문자열에 쉼표나 큰따옴표가 포함되면 CSV 규칙에 따라 큰따옴표로 감쌉니다.
- 문자열 내부 큰따옴표는 `""` 로 escape 합니다.
- 빈 문자열은 `""` 로 저장합니다.
- 1차 구현에서는 값 내부 줄바꿈은 허용하지 않습니다.
- 부분 컬럼 `INSERT` 는 허용하며, 빠진 컬럼은 빈 문자열로 저장합니다.

## 4. 디렉터리 구조

```txt
.
├── data/
├── schema/
├── src/
├── tests/
├── examples/
├── sql_processor_plan.html
├── sql_processor_plan.pdf
├── Makefile
└── README.md
```

## 5. 구현 순서

이번 프로젝트는 아래 순서대로 구현했습니다.

1. Foundation
프로젝트 구조, CLI 실행, SQL 파일 읽기

2. Schema / Storage
테이블 존재 규칙, 스키마 규칙, CSV 저장 규칙

3. Parser
`INSERT`, `SELECT` 최소 문법 파싱, AST 구성

4. Execution
스키마 검증, `INSERT` 실행, `SELECT` 실행

5. Quality
단위 테스트, 기능 테스트, 에러 처리 검증

6. README / Demo
발표 기준 README 정리, 예제 SQL, 데모 흐름 정리

## 6. 브랜치 전략

브랜치는 학습 흐름에 맞춰 누적형으로 운영했습니다.  
각 브랜치는 반드시 이전 단계 브랜치에서 생성했고, 이전 단계 내용과 현재 단계 내용을 함께 포함합니다.

- `step/01-foundation`
- `step/02-schema-storage`
- `step/03-parser`
- `step/04-execution`
- `step/05-quality`
- `step/06-readme-demo`

브랜치를 순서대로 보면 기능이 어떻게 쌓이는지 단계별로 따라갈 수 있습니다.

## 7. 빌드 방법

이 환경에서는 `mingw32-make` 가 내부 오류를 내서, 아래처럼 `gcc` 직접 명령으로 확인했습니다.

### 프로그램 빌드

```powershell
gcc -Wall -Wextra -std=c11 -Isrc -o sqlparser.exe src/main.c src/util.c src/schema.c src/storage.c src/ast.c src/lexer.c src/parser.c src/executor.c
```

### 테스트 빌드

```powershell
gcc -Wall -Wextra -std=c11 -Isrc -o test_runner.exe tests/test_runner.c src/util.c src/schema.c src/storage.c src/ast.c src/lexer.c src/parser.c src/executor.c
```

## 8. 실행 방법

```powershell
.\sqlparser.exe <sql-file-path>
```

예시:

```powershell
.\sqlparser.exe .\examples\insert_users.sql
.\sqlparser.exe .\examples\select_name_age.sql
```

## 9. 예제 SQL

### INSERT 예제

```sql
INSERT INTO users (id, name, age) VALUES (1, 'Alice', 20);
```

### SELECT 예제

```sql
SELECT name, age FROM users;
```

## 10. 테스트 방법

```powershell
.\test_runner.exe
```

검증한 주요 항목은 아래와 같습니다.

- lexer 가 SQL 문장을 올바르게 토큰화하는지
- parser 가 `INSERT`, `SELECT` 를 올바르게 AST 로 변환하는지
- 스키마 로딩과 CSV 헤더 검증이 맞는지
- 부분 컬럼 `INSERT` 시 누락된 컬럼이 빈 문자열로 채워지는지
- `SELECT` 결과가 CSV 파일 기준으로 올바르게 출력되는지
- 쉼표와 큰따옴표가 포함된 문자열이 CSV 규칙대로 저장되는지

## 11. 데모 시나리오

발표 시 아래 순서로 데모하면 흐름이 깔끔합니다.

1. `schema/users.meta` 와 `data/users.csv` 구조 설명
2. `examples/insert_users.sql` 실행
3. `data/users.csv` 에 데이터가 추가된 것 확인
4. `examples/select_name_age.sql` 실행
5. 테스트 러너 결과 설명
6. 브랜치 전략을 보여주며 단계별 구현 흐름 설명

## 12. 핵심 구현 포인트

- SQL 파일을 그대로 읽은 뒤 lexer 와 parser 를 거쳐 구조화된 AST 로 변환했습니다.
- 실행기는 AST 를 바탕으로 스키마를 검증하고, CSV 파일에 데이터를 추가하거나 조회합니다.
- CSV 저장 규칙을 직접 구현해 디버깅이 쉽고, 사람이 파일을 열어 바로 검증할 수 있게 했습니다.
- 테스트 러너를 별도로 만들어 파싱, 스키마, 실행, CSV escape 를 모두 검증했습니다.

## 13. 한계와 확장 방향

현재 버전의 제한 사항은 아래와 같습니다.

- 단일 SQL 문장만 실행
- `WHERE`, `JOIN`, `UPDATE`, `DELETE` 미지원
- 값 내부 줄바꿈 미지원
- 타입 시스템 없이 문자열 중심 처리

다음 단계 확장 후보는 아래와 같습니다.

- `WHERE col = value` 지원
- 여러 SQL 문장 연속 실행
- 실행 로그 출력
- 더 풍부한 오류 위치 표시
