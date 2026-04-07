CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Isrc

SRC_COMMON = src/util.c

all: sqlparser.exe

sqlparser.exe: src/main.c $(SRC_COMMON)
	$(CC) $(CFLAGS) -o $@ src/main.c $(SRC_COMMON)

clean:
	del /Q sqlparser.exe test_runner.exe *.o 2>NUL || exit 0

