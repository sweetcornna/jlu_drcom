ifeq ($(origin CC), default)
CC := gcc
endif

EXEEXT :=
ifeq ($(OS),Windows_NT)
EXEEXT := .exe
RMFILES := cmd /C del /Q /F
CLEAN_REDIRECT := 2>NUL
else
RMFILES := $(RM)
CLEAN_REDIRECT :=
endif

TARGET ?= dogcom$(EXEEXT)
CPPFLAGS += -I.
CFLAGS += -std=gnu99 -Wno-unused-result
LDFLAGS +=
LDLIBS +=

ifeq ($(debug),y)
CFLAGS += -DDEBUG -g
endif

ifeq ($(win32),y)
LDLIBS += -lws2_32
endif

ifeq ($(static),y)
LDFLAGS += -static
endif

ifeq ($(force_encrypt),y)
CFLAGS += -DFORCE_ENCRYPT
endif

SOURCES := $(wildcard *.c) $(wildcard libs/*.c)
OBJECTS := $(patsubst %.c,%.o,$(SOURCES))
TEST_PARSER_BIN := tests/parser_smoke$(EXEEXT)
TEST_PARSER_SOURCES := tests/parser_smoke.c configparse.c retry_policy.c
TEST_RETRY_POLICY_BIN := tests/retry_policy_smoke$(EXEEXT)
TEST_RETRY_POLICY_SOURCES := tests/retry_policy_smoke.c retry_policy.c
TEST_PROTOCOL_PACKET_BIN := tests/protocol_packet_smoke$(EXEEXT)
TEST_PROTOCOL_PACKET_SOURCES := tests/protocol_packet_smoke.c protocol.c libs/md5.c
TEST_KEEPALIVE_PACKET_BIN := tests/keepalive_packet_smoke$(EXEEXT)
TEST_KEEPALIVE_PACKET_SOURCES := tests/keepalive_packet_smoke.c keepalive.c libs/md4.c libs/md5.c libs/sha1.c

ifeq ($(OS),Windows_NT)
CLEAN_OBJECTS := $(subst /,\,$(OBJECTS))
CLEAN_TARGETS := $(subst /,\,$(TARGET)) $(subst /,\,$(TEST_PARSER_BIN)) $(subst /,\,$(TEST_RETRY_POLICY_BIN)) $(subst /,\,$(TEST_PROTOCOL_PACKET_BIN)) $(subst /,\,$(TEST_KEEPALIVE_PACKET_BIN))
else
CLEAN_OBJECTS := $(OBJECTS)
CLEAN_TARGETS := $(TARGET) $(TEST_PARSER_BIN) $(TEST_RETRY_POLICY_BIN) $(TEST_PROTOCOL_PACKET_BIN) $(TEST_KEEPALIVE_PACKET_BIN)
endif

all: $(TARGET)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_PARSER_BIN): $(TEST_PARSER_SOURCES) configparse.h debug.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(TEST_PARSER_SOURCES) -o $(TEST_PARSER_BIN)

$(TEST_RETRY_POLICY_BIN): $(TEST_RETRY_POLICY_SOURCES) auth.h retry_policy.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(TEST_RETRY_POLICY_SOURCES) -o $(TEST_RETRY_POLICY_BIN)

$(TEST_PROTOCOL_PACKET_BIN): $(TEST_PROTOCOL_PACKET_SOURCES) protocol.h configparse.h libs/md5.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(TEST_PROTOCOL_PACKET_SOURCES) -o $(TEST_PROTOCOL_PACKET_BIN)

$(TEST_KEEPALIVE_PACKET_BIN): $(TEST_KEEPALIVE_PACKET_SOURCES) keepalive.h configparse.h retry_policy.h libs/md4.h libs/md5.h libs/sha1.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(TEST_KEEPALIVE_PACKET_SOURCES) -o $(TEST_KEEPALIVE_PACKET_BIN)

test-parser: $(TEST_PARSER_BIN)
	./$(TEST_PARSER_BIN)

test-retry-policy: $(TEST_RETRY_POLICY_BIN)
	./$(TEST_RETRY_POLICY_BIN)

test-protocol-packet: $(TEST_PROTOCOL_PACKET_BIN)
	./$(TEST_PROTOCOL_PACKET_BIN)

test-keepalive-packet: $(TEST_KEEPALIVE_PACKET_BIN)
	./$(TEST_KEEPALIVE_PACKET_BIN)

clean:
	-$(RMFILES) $(CLEAN_OBJECTS) $(CLEAN_TARGETS) $(CLEAN_REDIRECT)

.PHONY: all clean test-parser test-retry-policy test-protocol-packet test-keepalive-packet
