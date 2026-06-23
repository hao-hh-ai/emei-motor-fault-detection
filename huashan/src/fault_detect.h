#pragma once

#include "feature_extract.h"

/*
 * 加权评分 + 持续性滤波 → 故障等级 (0~3)
 * 内部维护 PERSIST_N 个历史值, 需连续一致才确认
 */
int fault_detect(const vibration_features_t *f);
