BIN_DIR        ?= bin
BUILD_DIR      ?= build

CLIENT_CC      ?= i686-w64-mingw32-gcc
SERVER_CC      ?= gcc

COMMON_SRC      = src/common.c
SQLITE_SRC		= src/sqlite3.c

CLIENT_SRCS     = $(wildcard src/client/*.c) $(COMMON_SRC) $(SQLITE_SRC)
SERVER_SRCS     = $(wildcard src/server/*.c) $(COMMON_SRC)

CLIENT_OBJS     = $(CLIENT_SRCS:src/client/%.c=$(BUILD_DIR)/client/%.o)
CLIENT_OBJS    := $(CLIENT_OBJS:src/%.c=$(BUILD_DIR)/client/%.o)

SERVER_OBJS     = $(SERVER_SRCS:src/server/%.c=$(BUILD_DIR)/server/%.o)
SERVER_OBJS    := $(SERVER_OBJS:src/%.c=$(BUILD_DIR)/server/%.o)

COMMON_CFLAGS  ?= -Iinclude -Wall -Wextra -Wpedantic -fPIC 
CLIENT_CFLAGS  ?= $(COMMON_CFLAGS)
SERVER_CFLAGS  ?= $(COMMON_CFLAGS)

CLIENT_LDFLAGS ?= -lwininet -liphlpapi -ladvapi32 -lws2_32 -lgdi32 -lcrypt32 -lbcrypt
SERVER_LDFLAGS ?= -shared

.PHONY: all clean dirs

all: dirs $(BIN_DIR)/client.exe $(BIN_DIR)/lib.so

dirs:
	@mkdir -p $(BIN_DIR) $(BUILD_DIR)/client $(BUILD_DIR)/server

$(BIN_DIR)/client.exe: $(CLIENT_OBJS)
	$(CLIENT_CC) $(CLIENT_OBJS) -o $@ $(CLIENT_LDFLAGS)

$(BIN_DIR)/lib.so: $(SERVER_OBJS)
	$(SERVER_CC) $(SERVER_OBJS) -o $@ $(SERVER_LDFLAGS)

$(BUILD_DIR)/client/%.o: src/client/%.c
	$(CLIENT_CC) $(CLIENT_CFLAGS) -c $< -o $@

$(BUILD_DIR)/client/common.o: src/common.c
	$(CLIENT_CC) $(CLIENT_CFLAGS) -c $< -o $@

$(BUILD_DIR)/client/sqlite3.o: src/sqlite3.c
	$(CLIENT_CC) $(CLIENT_CFLAGS) -c $< -o $@

$(BUILD_DIR)/server/%.o: src/server/%.c
	$(SERVER_CC) $(SERVER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/server/common.o: src/common.c
	$(SERVER_CC) $(SERVER_CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

# Add to your existing Makefile

TEST_CC      ?= i686-w64-mingw32-gcc
TEST_CFLAGS  ?= -Iinclude -Wall -Wextra -Wpedantic -fPIC
TEST_LDFLAGS ?= -lwininet -liphlpapi -ladvapi32 -lws2_32 -lgdi32 -lcrypt32 -lbcrypt
BUILD_TEST   ?= $(BUILD_DIR)/test
BIN_TEST     ?= $(BIN_DIR)/test

COMMON_SRCS  = src/common.c src/sqlite3.c
CLIENT_SRCS  = $(filter-out src/client/main.c,$(wildcard src/client/*.c))
ALL_SRCS     = $(COMMON_SRCS) $(CLIENT_SRCS)
ALL_OBJS     = $(ALL_SRCS:src/%.c=$(BUILD_TEST)/%.o)

.PHONY: test clean_test

# Compile a single test file together with common + client sources (excluding main.c)
# Usage: make test FILE=my_test.c
test: dirs $(BIN_TEST)/$(basename $(notdir $(FILE))).exe

# Link all objects
$(BIN_TEST)/%.exe: $(BUILD_TEST)/test/%.o $(ALL_OBJS)
	$(TEST_CC) $^ -o $@ $(TEST_LDFLAGS)

# Compile the test file
$(BUILD_TEST)/test/%.o: src/test/%.c
	mkdir -p $(BUILD_TEST)/test $(BIN_TEST)
	$(TEST_CC) $(TEST_CFLAGS) -c $< -o $@

# Compile common and client sources
$(BUILD_TEST)/%.o: src/%.c
	mkdir -p $(dir $@)
	$(TEST_CC) $(TEST_CFLAGS) -c $< -o $@

clean_test:
	rm -rf $(BUILD_TEST)/test $(BIN_TEST)
