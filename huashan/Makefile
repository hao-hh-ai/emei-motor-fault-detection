# 华山派桥接程序 — 纯 Makefile
# 用法:
#   make CROSS_COMPILE=riscv64-unknown-linux-musl-   (交叉编译)
#   make CROSS_COMPILE=                               (本地编译测试)

BINARY  := huashan_bridge
SRCS    := src/main.c src/sliding_window.c src/feature_extract.c \
           src/fault_detect.c
OBJS    := $(SRCS:.c=.o)

CC      := $(CROSS_COMPILE)gcc
CFLAGS  := -std=c11 -Os -Wall -Wextra \
           -ffunction-sections -fdata-sections \
           -Isrc
LDFLAGS := -Wl,--gc-sections -static -lm

.PHONY: all clean

all: $(BINARY)

$(BINARY): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "  → $(BINARY) built ($(CROSS_COMPILE))"

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(BINARY)
