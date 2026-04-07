# SqlParser

SQL 처리기 프로젝트입니다.

현재까지 반영된 내용은 아래와 같습니다.

- `sqlparser <sql-file-path>` 형태의 CLI 실행
- SQL 파일 전체 읽기
- `schema/<table>.meta` + `data/<table>.csv` 기반 테이블 존재 규칙
- CSV 저장 규칙과 헤더 검증 로직

예시 스키마는 `users` 테이블로 제공합니다.
