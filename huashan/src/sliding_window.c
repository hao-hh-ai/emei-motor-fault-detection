#include "sliding_window.h"
#include <string.h>

void sw_init(sliding_window_t *w)
{
    memset(w, 0, sizeof(*w));
}

void sw_feed(sliding_window_t *w, int16_t x, int16_t y, int16_t z)
{
    int pos = w->head;
    w->buf[0][pos] = x;
    w->buf[1][pos] = y;
    w->buf[2][pos] = z;

    w->head = (pos + 1) % SW_WINDOW_SIZE;
    if (w->count < SW_WINDOW_SIZE)
        w->count++;
}

void sw_get_window(const sliding_window_t *w,
                   float *xs, float *ys, float *zs)
{
    int start;
    if (w->count < SW_WINDOW_SIZE) {
        /* 未满 — 从头开始 */
        start = 0;
    } else {
        /* 满 — 最旧的 256 点 */
        start = w->head;  /* head 指向下一个写入位, 即最旧的 */
    }

    for (int i = 0; i < SW_WINDOW_SIZE; i++) {
        int idx = (start + i) % SW_WINDOW_SIZE;
        xs[i] = (float)w->buf[0][idx];
        ys[i] = (float)w->buf[1][idx];
        zs[i] = (float)w->buf[2][idx];
    }
}
