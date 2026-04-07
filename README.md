# SqlParser

SQL 처리기 프로젝트입니다.

현재까지 반영된 내용은 아래와 같습니다.

- `sqlparser <sql-file-path>` 형태의 CLI 실행
- SQL 파일 전체 읽기
- `schema/<table>.meta` + `data/<table>.csv` 기반 테이블 존재 규칙
- CSV 저장 규칙과 헤더 검증 로직
- `INSERT`, `SELECT` 최소 문법 파싱
- 파싱 결과를 구조체로 변환하는 AST
- `INSERT` 실행 시 CSV 파일에 데이터 추가
- `SELECT` 실행 시 CSV 파일에서 데이터 읽기 및 출력
- 단위 테스트와 기능 테스트용 러너 제공

예시 스키마는 `users` 테이블로 제공합니다.
