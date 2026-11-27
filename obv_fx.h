/* SPDX-License-Identifier: GPL-3.0-only */
/* The effects after the output stage: a stereo chorus, a ping-pong delay in note values of the tempo, and a
 * plate reverb (J. Dattorro's figure-of-eight tank, 1997). All lines are int16 in one .pool buffer; the
 * signals are Q15 as the output stage leaves them. Integers only.
 * REV_DIV 1: the reverb at the full rate (the sound comes first). 2: at half the rate, if the chip turns out
 * to need it: a dark hall loses little by it and it halves the reverb's time and memory. */
#ifndef REV_DIV
#define REV_DIV 1u
#endif
#define RVL(x) ((uint32_t)((x) * fx_fs / 29761u / REV_DIV))   /* the paper's 29761 Hz lengths at our rate (OberVave 3: the real one) */
#define RVLM(x) ((uint32_t)((x) * 48000u / 29761u / REV_DIV))  /* (the memory: enough up to 48 kHz) */
#define DLY_MAX 32768u                          /* a side of the delay: 743 ms */
#define CHO_N 1024u
#define REV_PRE_N (8192u / REV_DIV)             /* up to 120 ms of pre-delay */
#define RV_IN1 RVL(142)
#define RV_IN2 RVL(107)
#define RV_IN3 RVL(379)
#define RV_IN4 RVL(277)
#define RV_AP1 RVL(672)
#define RV_D1 RVL(4453)
#define RV_AP2 RVL(1800)
#define RV_D2 RVL(3720)
#define RV_AP3 RVL(908)
#define RV_D3 RVL(4217)
#define RV_AP4 RVL(2656)
#define RV_D4 RVL(3163)
#define REV_N (REV_PRE_N + RVLM(142) + RVLM(107) + RVLM(379) + RVLM(277) + RVLM(672) + 16u + RVLM(4453) + RVLM(1800) + RVLM(3720) + RVLM(908) + 16u + RVLM(4217) + RVLM(2656) + RVLM(3163))
#define RV_BW (REV_DIV == 2u ? 24000 : 16700)   /* the input's one-pole: ~5 kHz either way */
#define FX_MEM_N (CHO_N + 2u * DLY_MAX + REV_N)

typedef struct { int16_t *b; uint32_t len, w; } line_t;   /* w: where the next sample goes */
static int16_t fx_mem[FX_MEM_N] __attribute__((section(".pool")));
static uint32_t fx_used;

static inline int32_t sat16(int32_t v) { return v > 32767 ? 32767 : v < -32767 ? -32767 : v; }
static void line_init(line_t *l, uint32_t len) { l->b = fx_mem + fx_used; l->len = len; l->w = 0; fx_used += len; }
static inline __attribute__((always_inline)) int32_t line_read(const line_t *l, uint32_t back)   /* 1 <= back <= len */
{
    uint32_t i = l->w + l->len - back;
    if (i >= l->len) i -= l->len;
    return l->b[i];
}
static inline __attribute__((always_inline)) void line_write(line_t *l, int32_t v)
{
    l->b[l->w] = (int16_t)sat16(v);
    if (++l->w >= l->len) l->w = 0;
}
/* Dattorro's allpass: the line holds the inner node, the output is tapped from it. g Q15, signed */
static inline __attribute__((always_inline)) int32_t ap_step(line_t *l, int32_t in, int32_t g)
{
    int32_t d = line_read(l, l->len), v = in - ((d * g) >> 15);
    line_write(l, v);
    return d + ((v * g) >> 15);
}
/* the same with the delay swaying by e (Q8 samples, |e| <= 8 << 8) around base; the line is base + 16 long */
static inline __attribute__((always_inline)) int32_t ap_mod(line_t *l, int32_t in, int32_t g, uint32_t base, int32_t e)
{
    uint32_t back = (uint32_t)((int32_t)base + (e >> 8));
    int32_t f = e & 255, a = line_read(l, back), b = line_read(l, back + 1u), d = a + (((b - a) * f) >> 8), v;
    v = in - ((d * g) >> 15);
    line_write(l, v);
    return d + ((v * g) >> 15);
}
static inline int32_t tri_q(uint32_t ph)        /* a triangle -2048..2047 (so Q8 = +-8 samples) */
{
    int32_t t = (int32_t)(ph >> 19);            /* 0..8191 */
    return t < 4096 ? t - 2048 : 6143 - t;
}

/* ---- the state */
static line_t cho, dl, dr, rv_pre, rv_in[4], rv_ap1, rv_d1, rv_ap2, rv_d2, rv_ap3, rv_d3, rv_ap4, rv_d4;
static uint32_t cho_ph, cho_inc, rv_ph1, rv_ph2;
static int32_t cho_depth_k, cho_mix, dly_mix, rev_mix, dly_fb, dly_tone_k, rev_decay, rev_damp_k;
static int32_t dly_time, dly_cur;               /* Q16 samples: the target and where the read is (it slews like a tape) */
static int32_t dly_lpl, dly_lpr, rv_lp_in, rv_lpl, rv_lpr, rv_yl, rv_yr;
#if REV_DIV == 2u
static int32_t rv_hold;
#endif
static uint32_t rev_pre_n = 1, dly_active, rev_active;
/* (BOOM 0.7) the MODEs: the delay's character, the reverb's room */
static uint32_t dly_mode, rev_mode, dly_wow, dly_flut;
static int32_t dly_hpl, dly_hpr, rv_bw = RV_BW, rv_len[4], rv_mod = 256;   /* rv_len: the tank's four delays (ROOM: shorter) · rv_mod: its sway, Q8 */
#if REV_DIV == 2u
static uint32_t fx_odd;
#endif

static void fx_init(void)
{
    line_init(&cho, CHO_N);
    line_init(&dl, DLY_MAX);
    line_init(&dr, DLY_MAX);
    line_init(&rv_pre, REV_PRE_N);
    line_init(&rv_in[0], RV_IN1); line_init(&rv_in[1], RV_IN2); line_init(&rv_in[2], RV_IN3); line_init(&rv_in[3], RV_IN4);
    line_init(&rv_ap1, RV_AP1 + 16u); line_init(&rv_d1, RV_D1); line_init(&rv_ap2, RV_AP2); line_init(&rv_d2, RV_D2);
    line_init(&rv_ap3, RV_AP3 + 16u); line_init(&rv_d3, RV_D3); line_init(&rv_ap4, RV_AP4); line_init(&rv_d4, RV_D4);
    dly_cur = dly_time = 11025 << 16;
}

/* the patch's numbers, once per ISR call. quarter: frames per quarter note (the arp's clock) */
static void fx_block(const patch_t *p, uint32_t quarter)
{
    int32_t d, t;
    uint32_t div = p->v[P_DLY_TIME] % NDDIV, samples;
    d = p->v[P_CHO_RATE]; cho_inc = (uint32_t)(10 + ((d * d * 590) >> 14)) * (974u * 44100u / fx_fs);   /* 0.1 .. 5.9 Hz */
    cho_depth_k = (int32_t)(p->v[P_CHO_DEPTH] * 67u * fx_fs / 44100u);       /* x the triangle's +-2048 = Q16 samples: up to +-6 ms */
    cho_mix = p->v[P_CHO_MIX];
    dly_mix = rev_mix = 180;                    /* (0.7) the returns always full – 0.7 of the dry (Q8): the sends set how much */
    dly_mode = p->v[P_DLY_MIX] < NDM ? p->v[P_DLY_MIX] : DM_ANALOG;
    rev_mode = p->v[P_REV_MIX] < NRM ? p->v[P_REV_MIX] : RM_PLATE;
    dly_fb = p->v[P_DLY_FB] * 250;              /* up to 0.97 */
    t = p->v[P_DLY_TONE]; dly_tone_k = clampi(((t * t) >> 6) + 8, 8, 256);
    if (dly_mode == DM_ANALOG) dly_tone_k = clampi(dly_tone_k * 5 / 8, 6, 160);      /* a bucket brigade: darker each round */
    else if (dly_mode == DM_TAPE) dly_tone_k = clampi(dly_tone_k * 7 / 8, 8, 220);
    d = p->v[P_REV_SIZE]; rev_decay = 12000 + ((d * d * 19500) >> 14);   /* 0.37 .. 0.96: the top is a near-freeze */
    d = p->v[P_REV_DAMP]; rev_damp_k = 32768 - d * d * 2;   /* the one-pole's k at the half rate */
    {   /* (0.7) HALL: a long, darker, slowly swaying tail behind a later start · ROOM: a small tank, short and bright ·
         * PLATE: the tank as it was (dense, bright) */
        static const uint16_t SC[NRM] = {256, 150, 256};   /* (Sven 21.11.: ROOM "wie ne Blechbüchse" – at 110 the short tank rang) */
        rv_len[0] = (int32_t)(RV_D1 * SC[rev_mode] >> 8); rv_len[1] = (int32_t)(RV_D2 * SC[rev_mode] >> 8);
        rv_len[2] = (int32_t)(RV_D3 * SC[rev_mode] >> 8); rv_len[3] = (int32_t)(RV_D4 * SC[rev_mode] >> 8);
        if (rev_mode == RM_HALL) { rev_decay = 20000 + ((p->v[P_REV_SIZE] * p->v[P_REV_SIZE] * 12000) >> 14); rev_damp_k = rev_damp_k * 3 / 4; rv_bw = RV_BW * 5 / 8; rv_mod = 512; }
        else if (rev_mode == RM_ROOM) { rev_decay = 6000 + ((p->v[P_REV_SIZE] * p->v[P_REV_SIZE] * 17000) >> 14); rev_damp_k = rev_damp_k * 13 / 16; rv_bw = RV_BW; rv_mod = 512; }   /* (a small room: swaying as much as the hall (512 = the lines' +16 spare), so no line rings; walls that take the top) */
        else { rv_bw = RV_BW; rv_mod = 256; }
        if (rev_damp_k < 2000) rev_damp_k = 2000;
    }
    if (REV_DIV == 1u) {                        /* the same corner at the full rate: k' = 1 - sqrt(1 - k) */
        uint32_t x = (uint32_t)(32768 - rev_damp_k) << 15, r = 0, bit = 1u << 15;
        for (; bit; bit >>= 1)
            if ((r + bit) * (r + bit) <= x) r += bit;
        rev_damp_k = 32768 - (int32_t)r;
    }
    rev_pre_n = 1u + (uint32_t)p->v[P_REV_PRE] * 41u * fx_fs / 44100u / REV_DIV;
    if (rev_mode == RM_HALL) { rev_pre_n = rev_pre_n * 3u / 2u + 960u * fx_fs / 48000u; if (rev_pre_n > REV_PRE_N - 2u) rev_pre_n = REV_PRE_N - 2u; }   /* (+20 ms) */
    else if (rev_mode == RM_ROOM) rev_pre_n = 1u + rev_pre_n / 4u;
    samples = quarter * DDIV_NUM[div] / DDIV_DEN[div];
    if (samples > DLY_MAX - 4u) samples = DLY_MAX - 4u;
    if (samples < 64u) samples = 64u;
    dly_time = (int32_t)(samples << 16);
    if (dly_mix) dly_active = 12u * FS; else if (dly_active > HALF_FRAMES) dly_active -= HALF_FRAMES; else dly_active = 0;
    if (rev_mix) rev_active = 24u * FS; else if (rev_active > HALF_FRAMES) rev_active -= HALF_FRAMES; else rev_active = 0;
}

static inline __attribute__((always_inline)) int32_t line_read_q16(const line_t *l, int32_t back_q16)
{
    uint32_t b = (uint32_t)back_q16 >> 16;
    int32_t f = (back_q16 >> 8) & 255, a = line_read(l, b), c = line_read(l, b + 1u);
    return a + (((c - a) * f) >> 8);
}
/* 4-point Hermite for a tap that is always moving (the chorus): no treble loss between the samples */
static inline __attribute__((always_inline)) int32_t line_read_q16_h(const line_t *l, int32_t back_q16)
{
    uint32_t b = (uint32_t)back_q16 >> 16;
    int32_t f = (back_q16 >> 8) & 255, p0 = line_read(l, b - 1u), p1 = line_read(l, b), p2 = line_read(l, b + 1u), p3 = line_read(l, b + 2u);
    int32_t c1 = (p2 - p0) >> 1, c2 = p0 - ((5 * p1) >> 1) + 2 * p2 - (p3 >> 1), c3 = ((p3 - p0) >> 1) + ((3 * (p1 - p2)) >> 1);
    return p1 + ((f * (c1 + ((f * (c2 + ((f * c3) >> 8))) >> 8))) >> 8);
}

/* one reverb sample at the half rate: in -> pre-delay -> bandwidth -> 4 diffusers -> the two tank halves */
static inline void rev_tick(int32_t in)
{
    int32_t x, a, b, e1 = (tri_q(rv_ph1) * rv_mod) >> 8, e2 = (tri_q(rv_ph2) * rv_mod) >> 8, dec = rev_decay, dk = rev_damp_k;
    if (e1 > 2047) e1 = 2047; if (e1 < -2048) e1 = -2048; if (e2 > 2047) e2 = 2047; if (e2 < -2048) e2 = -2048;
    rv_ph1 += 97391u * REV_DIV;                 /* ~1.0 Hz and ~0.7 Hz: the tank's sway against metallic rings */
    rv_ph2 += 68174u * REV_DIV;
    line_write(&rv_pre, in);
    x = line_read(&rv_pre, rev_pre_n);
    rv_lp_in += ((x - rv_lp_in) * rv_bw) >> 15;
    x = ap_step(&rv_in[0], rv_lp_in, 24576);
    x = ap_step(&rv_in[1], x, 24576);
    x = ap_step(&rv_in[2], x, 20480);
    x = ap_step(&rv_in[3], x, 20480);
    /* left half, fed by the right's end */
    a = x + ((line_read(&rv_d4, (uint32_t)rv_len[3]) * dec) >> 15);
    a = ap_mod(&rv_ap1, a, -22938, RV_AP1, e1);
    line_write(&rv_d1, a);
    b = line_read(&rv_d1, (uint32_t)rv_len[0]);
    rv_lpl += ((b - rv_lpl) * dk) >> 15;
    b = ap_step(&rv_ap2, (rv_lpl * dec) >> 15, 16384);
    line_write(&rv_d2, b);
    /* right half, fed by the left's end */
    a = x + ((line_read(&rv_d2, (uint32_t)rv_len[1]) * dec) >> 15);
    a = ap_mod(&rv_ap3, a, -22938, RV_AP3, e2);
    line_write(&rv_d3, a);
    b = line_read(&rv_d3, (uint32_t)rv_len[2]);
    rv_lpr += ((b - rv_lpr) * dk) >> 15;
    b = ap_step(&rv_ap4, (rv_lpr * dec) >> 15, 16384);
    line_write(&rv_d4, b);
    /* the output taps (the paper's), x 0.6 */
#define RVT(x) (rev_mode == RM_ROOM && (x) > 400 ? RVL(x) * 150u >> 8 : RVL(x))   /* (ROOM: the taps inside its shorter tank) */
    a = line_read(&rv_d3, RVT(266)) + line_read(&rv_d3, RVT(2974)) - line_read(&rv_ap4, RVT(1913)) + line_read(&rv_d4, RVT(1996))
      - line_read(&rv_d1, RVT(1990)) - line_read(&rv_ap2, RVT(187)) - line_read(&rv_d2, RVT(1066));
    b = line_read(&rv_d1, RVT(353)) + line_read(&rv_d1, RVT(3627)) - line_read(&rv_ap2, RVT(1228)) + line_read(&rv_d2, RVT(2673))
      - line_read(&rv_d3, RVT(2111)) - line_read(&rv_ap4, RVT(335)) - line_read(&rv_d4, RVT(121));
    rv_yl = (a * 19661) >> 15;
    rv_yr = (b * 19661) >> 15;
}

/* (VIBE) the three effects side by side, each fed by its send bus; out gets their returns, MIX = the return's level */
/* (VIBE, 21.10.) DRIVE, the 4th send: the tracks' drive sends distorted, its return beside the dry sound (parallel: the
 * dry keeps its punch). Types OVERDRIVE (soft), DIST (harder, uneven), FUZZ (square), CRUSH (fewer bits, held samples);
 * TONE a low-pass after it, MIX the return */
enum { DV_OVERDRIVE, DV_DIST, DV_FUZZ, DV_CRUSH, DV_AMP, NDV };
static volatile uint8_t ctl_dv_type, ctl_dv_amt = 60, ctl_dv_tone = 90, ctl_dv_mix = 127;
static float dv_lp, dv_hold; static uint32_t dv_n;
static inline float dv_soft(float x) { if (x > 3.0f) return 1.0f; if (x < -3.0f) return -1.0f; return x * (27.0f + x * x) / (27.0f + 9.0f * x * x); }
/* (BOOM 0.8, Sven) AMP: VALVE's tube amp as a drive type, without its cabinet – and an INSERT, not a send (Sven): each
 * track has its own amp, the mixer's DRIVE row sets how much of the track runs through it (full: all of it), the
 * track's sends hear the amp. Two triode stages (the grid takes current on hard hits, the coupling cap pulls the bias
 * down: bright attack, then a compressed body), the second one fades in past the middle of GAIN; a power stage whose
 * supply sags on loud notes. Run at twice the rate (a line between two samples up, their mean down) so the clipping
 * folds back less. TONE is a low-pass behind it, MIX its volume */
typedef struct { float hp1, hp2, gc[2], env, sag, lp, x1, dc, tone; } amp_t;
static amp_t amps[8];
static inline float am_sat(float u) { return u / vd_sqrt(1.0f + u * u); }
static inline float am_stage(float u, float bias, float *gc)
{
    float g = u - *gc + bias, over = g - 1.0f;
    if (over > 0.0f) { *gc += over * 0.012f; if (*gc > 0.9f) *gc = 0.9f; g = 1.0f + over * 0.3f; }
    else *gc -= *gc * 0.00015f;
    return am_sat(g) - am_sat(bias);
}
static inline float amp_tick(amp_t *A, float x, float a, float tk)
{
    float g1 = 2.0f + a * 28.0f, s2 = (a - 0.35f) * 2.5f, pd = 0.9f + a * 0.7f, y = 0.0f, e, lvl, sg = 0.7f + 0.3f / (1.0f + 1.1f * (A->env > 2.0f ? 2.0f : A->env));
    uint32_t k;
    if (s2 < 0.0f) s2 = 0.0f; if (s2 > 1.0f) s2 = 1.0f;
    for (k = 0; k < 2u; k++) {
        float u = k ? x : (x + A->x1) * 0.5f, u2;
        A->hp1 += (u - A->hp1) * 0.0025f; u -= A->hp1;                   /* the input's coupling cap, ~35 Hz: an 808 keeps its body */
        u = am_stage(u * g1 * sg, 0.2f, &A->gc[0]);
        A->hp2 += (u - A->hp2) * 0.005f; u2 = u - A->hp2;                /* between the stages, ~75 Hz */
        u2 = am_stage(u2 * 3.5f, 0.14f, &A->gc[1]) * 0.8f;
        u += (u2 - u) * s2;
        A->lp += (u - A->lp) * 0.6f;                                     /* the tubes' own capacitance, ~12 kHz: TONE does the rest */
        y += A->lp * 0.5f;
    }
    A->x1 = x;
    e = (y < 0.0f ? -y : y) * pd;
    A->env += (e - A->env) * (e > A->env ? 0.0028f : 0.00016f);          /* the supply: ~15 ms down, ~250 ms back */
    e = A->env > 2.0f ? 2.0f : A->env;
    lvl = 0.8f / vd_sqrt(0.3f + pd) * (1.0f + 0.33f * (pd > 1.5f ? 1.5f : pd));
    y = am_sat(y * pd * 1.1f) / 1.1f / (1.0f + 1.1f * e) * lvl;
    A->dc += (y - A->dc) * 0.003f; y -= A->dc;                           /* the output transformer passes no DC (~20 Hz) */
    A->tone += (y - A->tone) * tk; y = A->tone;
    if (A->env < 1e-9f) { A->env = 0.0f; A->hp1 = A->hp2 = A->lp = A->dc = A->tone = A->gc[0] = A->gc[1] = 0.0f; }   /* (no denormals in the silence) */
    return y;
}
static void drive_process(int32_t *out, const int32_t *vs, uint32_t n)
{
    uint32_t i, ty = ctl_dv_type % NDV;
    float a = (float)ctl_dv_amt / 127.0f, pre = 1.0f + a * 24.0f, k = 0.04f + 0.96f * (float)ctl_dv_tone / 127.0f * (float)ctl_dv_tone / 127.0f;
    float ret = (float)ctl_dv_mix / 127.0f * 4000.0f;   /* (MIX full: the return about as loud as the dry sound) */
    if (!ctl_dv_mix || ty == DV_AMP) return;    /* (the AMP is an insert: bm_engine.h, in the tracks) */
    for (i = 0; i < n; i++) {
        float x = (float)vs[i] * (1.0f / 32768.0f) * pre, y;
        if (ty == DV_OVERDRIVE) y = dv_soft(x);
        else if (ty == DV_DIST) y = x > 0.0f ? dv_soft(x * 1.6f) : dv_soft(x * 0.7f) * 0.8f;
        else if (ty == DV_FUZZ) y = x > 0.05f ? 1.0f : x < -0.05f ? -1.0f : x * 20.0f;
        else {                                  /* CRUSH: 16 steps, a sample held for 2..12 */
            if (!dv_n--) { dv_n = 2u + (uint32_t)(a * 10.0f); dv_hold = (float)(int32_t)(dv_soft(x) * 8.0f) / 8.0f; }
            y = dv_hold;
        }
        dv_lp += (y - dv_lp) * k;
        {
            int32_t v = (int32_t)(dv_lp * ret);
            out[2u * i] = sat16(out[2u * i] + v);
            out[2u * i + 1u] = sat16(out[2u * i + 1u] + v);
        }
    }
}

static void fx_process(int32_t *out, const int32_t *cs, const int32_t *ds, const int32_t *rs, uint32_t n)
{
    uint32_t i, do_dly = dly_active != 0, do_rev = rev_active != 0;
    int32_t cm = cho_mix, dm = dly_mix, rm = rev_mix;
    for (i = 0; i < n; i++) {
        int32_t l = out[2u * i], r = out[2u * i + 1u];
        if (cm) {                               /* two taps a quarter turn apart on one line, around 12 ms */
            int32_t cb = (int32_t)(529u * fx_fs / 44100u) << 16, bl = cb + tri_q(cho_ph) * cho_depth_k, br = cb + tri_q(cho_ph + 0x40000000u) * cho_depth_k;
            cho_ph += cho_inc;
            line_write(&cho, cs[i]);
            l += (line_read_q16_h(&cho, bl) * cm) >> 8;
            r += (line_read_q16_h(&cho, br) * cm) >> 8;
        }
        if (do_dly) {                           /* ping-pong: the input enters left, the feedback crosses */
            int32_t wl, wr;
            int32_t rd, fl, fr;
            if (dly_mode == DM_DIGITAL) dly_cur = dly_time;   /* (0.7) DIGITAL: the new time at once, no glide */
            else if (dly_cur < dly_time) { dly_cur += 1 << 15; if (dly_cur > dly_time) dly_cur = dly_time; }
            else if (dly_cur > dly_time) { dly_cur -= 1 << 15; if (dly_cur < dly_time) dly_cur = dly_time; }
            rd = dly_cur;
            if (dly_mode == DM_TAPE) {              /* TAPE: wow (0.6 Hz) and flutter (6.3 Hz) on the read head */
                dly_wow += 53687u * 48000u / fx_fs; dly_flut += 563714u * 48000u / fx_fs;
                rd += (int32_t)(((int64_t)tri_q(dly_wow) * (dly_cur >> 9)) >> 11) + tri_q(dly_flut) * 12;
                if (rd < (2 << 16)) rd = 2 << 16;
                if (rd > (int32_t)((DLY_MAX - 3u) << 16)) rd = (int32_t)((DLY_MAX - 3u) << 16);
            }
            wl = line_read_q16(&dl, rd);
            wr = line_read_q16(&dr, rd);
            dly_lpl += ((wr - dly_lpl) * dly_tone_k) >> 8;
            dly_lpr += ((wl - dly_lpr) * dly_tone_k) >> 8;
            fl = (dly_lpl * dly_fb) >> 15; fr = (dly_lpr * dly_fb) >> 15;
            if (dly_mode == DM_TAPE) {              /* its lows thin out round by round, the tape saturates */
                dly_hpl += fl - (dly_hpl >> 8); dly_hpr += fr - (dly_hpr >> 8);   /* (Q8 states: exact at rest – no offset left standing in the loop) */
                fl = softclip(((fl - (dly_hpl >> 8)) * 3) / 2) * 2 / 3; fr = softclip(((fr - (dly_hpr >> 8)) * 3) / 2) * 2 / 3;
            } else if (dly_mode == DM_ANALOG) { fl = softclip(fl); fr = softclip(fr); }   /* the bucket brigade rounds off */
            line_write(&dl, (dly_mode == DM_DIGITAL ? ds[i] : softclip(ds[i])) + fl);
            line_write(&dr, fr);
            l += (wl * dm) >> 8;
            r += (wr * dm) >> 8;
        }
        if (do_rev) {
#if REV_DIV == 2u                               /* half rate: a pair in, the output interpolated between ticks */
            int32_t pl = rv_yl, pr = rv_yr, din = rs[i] * 2;
            if (!(fx_odd ^= 1u)) {
                rev_tick((rv_hold + din) >> 2);
                l += (((pl + rv_yl) >> 1) * rm) >> 8;
                r += (((pr + rv_yr) >> 1) * rm) >> 8;
            } else {
                l += (rv_yl * rm) >> 8;
                r += (rv_yr * rm) >> 8;
            }
            rv_hold = din;
#else
            rev_tick(rs[i]);
            l += (rv_yl * rm) >> 8;
            r += (rv_yr * rm) >> 8;
#endif
        }
        out[2u * i] = sat16(l);
        out[2u * i + 1u] = sat16(r);
    }
}
