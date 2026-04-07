CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude

BUILD_DIR = build
BIN_DIR = $(BUILD_DIR)/bin

SRC_COMMON = src/common/util.c src/storage/schema.c src/storage/storage.c src/sql/ast.c src/sql/lexer.c src/sql/parser.c src/execution/executor.c
APP_SOURCES = src/app/main.c $(SRC_COMMON)
TEST_SOURCES = tests/test_runner.c $(SRC_COMMON)

APP_BIN = $(BIN_DIR)/sqlparser.exe
TEST_BIN = $(BIN_DIR)/test_runner.exe

all: $(APP_BIN)

$(APP_BIN): $(APP_SOURCES)
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	if not exist $(BIN_DIR) mkdir $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(APP_SOURCES)

$(TEST_BIN): $(TEST_SOURCES)
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	if not exist $(BIN_DIR) mkdir $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(TEST_SOURCES)

test: $(TEST_BIN)
	if not exist $(BUILD_DIR)\tests mkdir $(BUILD_DIR)\tests
	.\$(TEST_BIN)

clean:
	if exist $(BUILD_DIR) rmdir /S /Q $(BUILD_DIR)
