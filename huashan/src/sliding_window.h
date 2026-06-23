#pragma once

#include <stdint.h>
#include <stdbool.h>

#define SW_WINDOW_SIZE  256
#define SW_AXES         3

typedef struct {
    int16_t buf[SW_AXES][SW_WINDOW_SIZE];  /* 三轴环形缓冲 */
    int     head;                           /* 写入位置 */
    int     count;                          /* 当前填充数 */
} sliding_window_t;

/* 初始化 */
void sw_init(sliding_window_t *w);

/* 喂入一个三相采样点 (raw int16) */
void sw_feed(sliding_window_t *w, int16_t x, int16_t y, int16_t z);

/* 窗口是否已满 (>=256点) */
static inline bool sw_is_ready(const sliding_window_t *w)
{
    return w->count >= SW_WINDOW_SIZE;
}

/* 获取最新 256 点 — 三轴各一个 float 数组 (调用者提供 float[256]) */
void sw_get_window(const sliding_window_t *w,
                   float *xs, float *ys, float *zs);
