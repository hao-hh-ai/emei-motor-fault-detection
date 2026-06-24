# 华山派桥接程序 — 纯 Makefile
# 用法:
#   make CROSS_COMPILE=riscv64-unknown-linux-musl-   (交叉编译, 含 TPU)
#   make CROSS_COMPILE=                               (本地编译测试, 无 TPU)
#
# TPU 编译需要 Sophon cviruntime 开发库。
# 如果不需要 TPU: make NOTPU=1

BINARY  := huashan_bridge
SRCS    := src/main.c src/sliding_window.c src/feature_extract.c \
           src/fault_detect.c src/tpu_inference.c
OBJS    := $(SRCS:.c=.o)

CC      := $(CROSS_COMPILE)gcc
CFLAGS  := -std=c11 -Os -Wall -Wextra \
           -ffunction-sections -fdata-sections \
           -Isrc
LDFLAGS := -Wl,--gc-sections -lm

# TPU SDK 路径 (华山派 CV1812H Sophon SDK)
TPU_INC := /home/xiaohuihui/huashan/workplace/sophpi-huashan/product-sample/access-guard-turnkey/prepare/cvi_aisdk/musl_riscv64/cvitek_tpu_sdk/include
TPU_LIB := /home/xiaohuihui/huashan/workplace/sophpi-huashan/product-sample/access-guard-turnkey/customer/cvitek/install/lib

# TPU 模式: 链接 cviruntime, 定义 USE_TPU
ifndef NOTPU
CFLAGS  += -DUSE_TPU -I$(TPU_INC)
LDFLAGS += -L$(TPU_LIB) -lcviruntime \
           -Wl,--dynamic-linker=/lib/ld-musl-riscv64v0p7_xthead.so.1 \
           -Wl,-rpath,/mnt/system/lib
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
