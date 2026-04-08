# SqlParser

간단한 SQL 학습용 파서 프로젝트입니다.

현재 프로젝트는 아래 기능을 지원합니다.

- `INSERT`, `SELECT` 최소 문법 파싱
- SQL 파일 입력 실행
- 터미널에서 직접 입력한 SQL 문자열 실행
- `schema/<table>.meta` 와 `data/<table>.csv` 기반 테이블 검증
- CSV 헤더 검증 및 데이터 읽기/쓰기

## 실행 방법

파일을 인자로 넘겨 실행할 수 있습니다.

```bash
./sqlparser examples/insert_user.sql
```

SQL 문장을 직접 넘겨 실행할 수도 있습니다.

```bash
./sqlparser "SELECT * FROM users;"
./sqlparser "INSERT INTO users (id, name, age) VALUES (1, 'kim', 20);"
```

인자를 여러 개로 나눠 넘겨도 하나의 SQL 문장으로 합쳐서 실행합니다.

```bash
./sqlparser SELECT id,name FROM users;
```

다만 `*` 는 셸이 먼저 해석할 수 있으므로 `SELECT *` 형태는 보통 따옴표로 감싸서 실행하는 것이 안전합니다.

```bash
./sqlparser "SELECT * FROM users;"
```

예시 스키마는 `users` 테이블로 제공됩니다.
