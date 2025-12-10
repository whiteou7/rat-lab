BIN_DIR        ?= bin
BUILD_DIR      ?= build

CLIENT_CC      ?= i686-w64-mingw32-gcc
SERVER_CC      ?= gcc

COMMON_SRC      = src/common.c

CLIENT_SRCS     = $(wildcard src/client/*.c) $(COMMON_SRC)
SERVER_SRCS     = $(wildcard src/server/*.c) $(COMMON_SRC)

CLIENT_OBJS     = $(CLIENT_SRCS:src/client/%.c=$(BUILD_DIR)/client/%.o)
CLIENT_OBJS    := $(CLIENT_OBJS:src/%.c=$(BUILD_DIR)/client/%.o)

SERVER_OBJS     = $(SERVER_SRCS:src/server/%.c=$(BUILD_DIR)/server/%.o)
SERVER_OBJS    := $(SERVER_OBJS:src/%.c=$(BUILD_DIR)/server/%.o)

COMMON_CFLAGS  ?= -Iinclude -Wall -Wextra -Wpedantic -fPIC
CLIENT_CFLAGS  ?= $(COMMON_CFLAGS)
SERVER_CFLAGS  ?= $(COMMON_CFLAGS)

CLIENT_LDFLAGS ?= -lwininet -liphlpapi -ladvapi32 -lws2_32 -lgdi32
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

$(BUILD_DIR)/server/%.o: src/server/%.c
	$(SERVER_CC) $(SERVER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/server/common.o: src/common.c
	$(SERVER_CC) $(SERVER_CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
