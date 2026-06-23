#include "fault_detect.h"
#include "threshold_config.h"

/* ── 持续性滤波历史 ── */
static int persist_hist[PERSIST_N];
static int persist_idx = 0;

int fault_detect(const vibration_features_t *f)
{
    /* ── 加权评分 ── */
    float score = 0.0f;
    float wsum  = 0.0f;

    #define CHECK_RULE(name, var, lo, hi, crit, w)  do { \
        float _v = (var);                                 \
        float _w = (w);                                   \
        if      (_v >= (crit)) score += 3.0f * _w;        \
        else if (_v >= (hi))   score += 2.0f * _w;        \
        else if (_v >= (lo))   score += 1.0f * _w;        \
        wsum += _w;                                       \
    } while(0)

    CHECK_RULE(rms,            f->rms,
               TH_RMS_WARN, TH_RMS_DANGER, TH_RMS_CRITICAL,
               feat_weight_rms);
    CHECK_RULE(peak,           f->peak,
               TH_PEAK_WARN, TH_PEAK_DANGER, TH_PEAK_CRITICAL,
               feat_weight_peak);
    CHECK_RULE(kurtosis,       f->kurtosis,
               TH_KURT_WARN, TH_KURT_DANGER, TH_KURT_CRITICAL,
               feat_weight_kurtosis);
    CHECK_RULE(crest_factor,   f->crest_factor,
               TH_CREST_WARN, TH_CREST_DANGER, TH_CREST_CRITICAL,
               feat_weight_crest_factor);
    CHECK_RULE(clearance,      f->clearance,
               TH_CLEAR_WARN, TH_CLEAR_DANGER, TH_CLEAR_CRITICAL,
               feat_weight_clearance);
    CHECK_RULE(impulse_factor, f->impulse_factor,
               TH_IMP_WARN, TH_IMP_DANGER, TH_IMP_CRITICAL,
               feat_weight_impulse_factor);
    CHECK_RULE(shape_factor,   f->shape_factor,
               TH_SHAPE_WARN, TH_SHAPE_DANGER, TH_SHAPE_CRITICAL,
               feat_weight_shape_factor);

    #undef CHECK_RULE

    float avg = (wsum > 0.0f) ? (score / wsum) : 0.0f;

    int raw;
    if      (avg >= SCORE_CRITICAL) raw = 3;
    else if (avg >= SCORE_DANGER)   raw = 2;
    else if (avg >= SCORE_WARN)     raw = 1;
    else                            raw = 0;

    /* ── 持续性滤波: 连续 PERSIST_N 次一致才输出 ── */
    persist_hist[persist_idx] = raw;
    persist_idx = (persist_idx + 1) % PERSIST_N;

    for (int i = 0; i < PERSIST_N; i++) {
        if (persist_hist[i] != raw)
            return -1; /* 不一致 — 不上报 */
    }
    return raw;
}
