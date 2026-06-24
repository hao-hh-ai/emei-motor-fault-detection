# 华山派桥接程序 — 纯 Makefile
# 用法:
#   make CROSS_COMPILE=riscv64-unknown-linux-musl-   (交叉编译, 含 TPU)
#   make CROSS_COMPILE=                               (本地编译测试, 无 TPU)
#
# TPU 编译需要 Sophon cviruntime 开发库。
# 如果不需要 TPU: make NOTPU=1

BINARY  := huashan_bridge
SRCS    := src/main.c src/sliding_window.c src/feature_extract.c \
           src/fault_detect.c
OBJS    := $(SRCS:.c=.o)

# TPU 推理模块 (可禁用)
ifndef NOTPU
SRCS    += src/tpu_inference.c
OBJS    += src/tpu_inference.o
CFLAGS  += -DUSE_TPU
endif

CC      := $(CROSS_COMPILE)gcc
CFLAGS  := -std=c11 -Os -Wall -Wextra \
           -ffunction-sections -fdata-sections \
           -Isrc $(CFLAGS)
LDFLAGS := -Wl,--gc-sections -lm

# TPU 需要动态链接 libcviruntime
ifndef NOTPU
LDFLAGS += -lcviruntime
else
LDFLAGS += -static
endif

.PHONY: all clean

all: $(BINARY)

$(BINARY): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "  → $(BINARY) built ($(CROSS_COMPILE))"

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(BINARY)
