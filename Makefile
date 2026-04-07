CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Isrc

SRC_COMMON = src/util.c src/schema.c src/storage.c src/ast.c src/lexer.c src/parser.c src/executor.c
TEST_SOURCES = tests/test_runner.c $(SRC_COMMON)

all: sqlparser.exe

sqlparser.exe: src/main.c $(SRC_COMMON)
	$(CC) $(CFLAGS) -o $@ src/main.c $(SRC_COMMON)

test_runner.exe: $(TEST_SOURCES)
	$(CC) $(CFLAGS) -o $@ $(TEST_SOURCES)

test: test_runner.exe
	.\test_runner.exe

clean:
	del /Q sqlparser.exe test_runner.exe *.o 2>NUL || exit 0
