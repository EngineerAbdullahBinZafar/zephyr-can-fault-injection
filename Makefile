# Standalone POSIX / Bare-Metal C11 Build & Verification
CC ?= gcc
CFLAGS := -std=c11 -O2 -Wall -Wextra -Werror -Wpedantic -Iinclude
SAN_FLAGS := -fsanitize=address,undefined

.PHONY: all test clean

all: build/test_zephyr_can_c

build/test_zephyr_can_c: src/zephyr_can_rate_limiter.c tests/test_zephyr_can_c.c include/zephyr_can_rate_limiter.h
	@mkdir -p build
	$(CC) $(CFLAGS) $(SAN_FLAGS) src/zephyr_can_rate_limiter.c tests/test_zephyr_can_c.c -o build/test_zephyr_can_c

test: all
	./build/test_zephyr_can_c

clean:
	rm -rf build
