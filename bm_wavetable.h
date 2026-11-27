/* SPDX-License-Identifier: GPL-3.0-only */
/* BOOM – the WAVETABLE kind's tables (0.41, Sven 21.11.2026). Eight tables of eight frames each, all made here from
 * their spectra (no recordings): ANALOG (sine → triangle → saw → square → narrow pulse), PULSE (its width 50 % → 5 %),
 * VOCAL (five vowels' formants), ORGAN (drawbars pulled one after another), SYNC (a saw synced to a faster one), FOLD
 * (a sine folded harder and harder), BELL (sparse high partials moving up), COMB (a saw through a moving comb).
 * Each frame is kept band-limited at nine levels (256, 128 … 1 harmonics): a voice reads the level its pitch allows,
 * so nothing folds back above half the sample rate. 512 points a frame, a guard point for the interpolation. */
#define WT_LEN 512u
#define WT_FR 8u
#define WT_MIP 9u
#define WT_NH 256u
static float wt_tab[NWT][WT_FR][WT_MIP][WT_LEN + 1u];
static float wt_sin512[WT_LEN];                 /* sin(2 pi k / 512): every harmonic's sample is an exact index */
#define WT_DFT 2048u
static float wt_sin2k[WT_DFT];

/* a frame's spectrum: a[n] cosine, b[n] sine parts of harmonic n (1..256) */
static void wt_shape_time(uint32_t t, float x, float *y)   /* the tables made in time: one period, WT_DFT points */
{
    for (uint32_t k = 0; k < WT_DFT; k++) {
        float ph = (float)k / (float)WT_DFT;
        if (t == 4u) {                          /* SYNC: the slave saw at 1 .. 6.5 times the master */
            float r = 1.0f + x * 5.5f, q = ph * r;
            y[k] = 2.0f * (q - (float)(int32_t)q) - 1.0f;
        } else {                                /* FOLD: a sine through a sine-shaped folder, gain 1 .. 6 */
            float g = 1.0f + x * 5.0f, sn = wt_sin2k[k];
            float u = 0.25f * g * sn;           /* (turns: sin(pi/2 g sin)) */
            u -= (float)(int32_t)u; if (u < 0.0f) u += 1.0f;
            y[k] = wt_sin2k[(uint32_t)(u * (float)WT_DFT) % WT_DFT];
        }
    }
}
static void wt_spectrum(uint32_t t, uint32_t f, float *a, float *b)
{
    const float x = (float)f / (float)(WT_FR - 1u), PI = 3.14159265f;
    memset(a, 0, sizeof(float) * (WT_NH + 1u)); memset(b, 0, sizeof(float) * (WT_NH + 1u));
    switch (t) {
    case 0: {                                   /* ANALOG: keyframes sine, triangle, saw, square, pulses – crossfaded */
        static const float K[8][2] = {{0, 0}, {1, 0}, {1, 0.5f}, {2, 0}, {2, 0.5f}, {3, 0}, {4, 0.3f}, {4, 0.12f}};
        uint32_t sh = (uint32_t)K[f][0]; float w = K[f][1];
        for (uint32_t n = 1; n <= WT_NH; n++) {
            float tri = (n & 1u) ? 8.0f / (PI * PI) / (float)(n * n) * ((n >> 1) & 1u ? -1.0f : 1.0f) : 0.0f;
            float saw = 2.0f / PI / (float)n * (n & 1u ? 1.0f : -1.0f);
            float sq = (n & 1u) ? 4.0f / (PI * (float)n) : 0.0f;
            if (sh == 0) b[n] = n == 1u ? 1.0f : 0.0f;
            else if (sh == 1) b[n] = tri * (1.0f - w) + saw * w;
            else if (sh == 2) b[n] = saw * (1.0f - w) + sq * w;
            else if (sh == 3) b[n] = sq;
            else { float d = w; a[n] = 2.0f * vd_sin(0.5f * (float)n * d) / ((float)n * PI); }   /* (vd_sin: turns) */
        }
        break; }
    case 1: {                                   /* PULSE: width 50 % .. 5 % */
        static const float D[8] = {0.5f, 0.42f, 0.35f, 0.28f, 0.21f, 0.15f, 0.1f, 0.05f};
        for (uint32_t n = 1; n <= WT_NH; n++) a[n] = 2.0f * vd_sin(0.5f * (float)n * D[f]) / ((float)n * PI);
        break; }
    case 2: {                                   /* VOCAL: three formants of a voice at 110 Hz – A, A-E, E, I, O, U, O-U, A again */
        static const float F[8][3] = {{800, 1150, 2900}, {600, 1400, 2800}, {400, 1700, 2600}, {280, 2250, 2950},
                                      {450, 800, 2830}, {325, 700, 2530}, {380, 750, 2700}, {700, 1220, 2600}};
        static const float G[3] = {1.0f, 0.5f, 0.25f}, BW[3] = {80.0f, 110.0f, 170.0f};
        for (uint32_t n = 1; n <= WT_NH; n++) {
            float hz = 110.0f * (float)n, s = 0.0f;
            for (uint32_t k = 0; k < 3u; k++) { float d = (hz - F[f][k]) / BW[k]; s += G[k] * vd_exp2(-d * d * 1.4427f); }
            b[n] = s * (n == 1u ? 1.0f : 1.0f / vd_sqrt((float)n)) + (n == 1u ? 0.3f : 0.0f);
        }
        break; }
    case 3: {                                   /* ORGAN: the drawbars – 8' alone, then 4', 2 2/3', 2', 1 3/5', 1 1/3', 1' and the top */
        static const uint8_t H[8] = {1, 2, 3, 4, 5, 6, 8, 16};
        for (uint32_t k = 0; k <= f; k++) b[H[k]] += k ? 0.7f : 1.0f;
        if (f == 7u) { b[10] += 0.4f; b[12] += 0.4f; }
        break; }
    case 4: case 5: {                           /* SYNC, FOLD: made in time, their harmonics by a DFT */
        static float y[WT_DFT];
        wt_shape_time(t, x, y);
        for (uint32_t n = 1; n <= WT_NH; n++) {
            float sa = 0.0f, sb = 0.0f;
            for (uint32_t k = 0; k < WT_DFT; k++) {
                uint32_t i = (uint32_t)((n * k) % WT_DFT);
                sb += y[k] * wt_sin2k[i]; sa += y[k] * wt_sin2k[(i + WT_DFT / 4u) % WT_DFT];
            }
            a[n] = sa * (2.0f / (float)WT_DFT); b[n] = sb * (2.0f / (float)WT_DFT);
        }
        break; }
    case 6: {                                   /* BELL: the fundamental and four high partials moving up as the frame goes on */
        static const float W[5] = {1.0f, 0.7f, 0.55f, 0.4f, 0.3f};
        uint32_t P[5] = {1u, 3u + f / 3u, 5u + f, 9u + 2u * f, 14u + 3u * f};
        for (uint32_t k = 0; k < 5u; k++) if (P[k] <= WT_NH) b[P[k]] += W[k];
        break; }
    default: {                                  /* COMB: a saw through a comb whose teeth move */
        float per = 3.0f + x * 9.0f;
        for (uint32_t n = 1; n <= WT_NH; n++) {
            float c = vd_sin((float)n / per + 0.25f);   /* (cos) */
            b[n] = (0.55f + 0.45f * c) / (float)n;
        }
        break; }
    }
}
static int wt_ready;
static void wt_init(void)                      /* (once, at the start: ~16 M multiply-adds) */
{
    static float a[WT_NH + 1u], b[WT_NH + 1u];
    if (wt_ready) return;
    for (uint32_t k = 0; k < WT_LEN; k++) wt_sin512[k] = vd_sin((float)k / (float)WT_LEN);
    for (uint32_t k = 0; k < WT_DFT; k++) wt_sin2k[k] = vd_sin((float)k / (float)WT_DFT);
    for (uint32_t t = 0; t < NWT; t++)
        for (uint32_t f = 0; f < WT_FR; f++) {
            float pk = 0.0f, g;
            wt_spectrum(t, f, a, b);
            for (uint32_t m = 0; m < WT_MIP; m++) {
                uint32_t nh = WT_NH >> m;
                float *o = wt_tab[t][f][m];
                for (uint32_t k = 0; k < WT_LEN; k++) {
                    float s = 0.0f;
                    for (uint32_t n = 1; n <= nh; n++) {
                        if (a[n] == 0.0f && b[n] == 0.0f) continue;
                        uint32_t i = (n * k) & (WT_LEN - 1u);
                        s += b[n] * wt_sin512[i] + a[n] * wt_sin512[(i + WT_LEN / 4u) & (WT_LEN - 1u)];
                    }
                    o[k] = s;
                    if (!m && (s > pk || -s > pk)) pk = s < 0.0f ? -s : s;
                }
            }
            g = pk > 1e-6f ? 0.9f / pk : 1.0f;   /* (each frame as loud at the top: the levels under it keep that scale) */
            for (uint32_t m = 0; m < WT_MIP; m++) {
                float *o = wt_tab[t][f][m];
                for (uint32_t k = 0; k < WT_LEN; k++) o[k] *= g;
                o[WT_LEN] = o[0];
            }
        }
    wt_ready = 1;
}
static inline uint32_t wt_mip(float inc)       /* the level whose harmonics stay under 0.45 x the rate */
{
    uint32_t m = 0;
    float lim = inc > 1e-7f ? 0.45f / inc : 1e9f;
    while (m < WT_MIP - 1u && (float)(WT_NH >> m) > lim) m++;
    return m;
}
static inline float wt_read(const float *f0, const float *f1, float fr, float ph)   /* two frames, crossfaded, read between two points */
{
    float x = ph * (float)WT_LEN; uint32_t j = (uint32_t)x; float u = x - (float)j;
    j &= WT_LEN - 1u;
    float s0 = f0[j] + (f0[j + 1u] - f0[j]) * u, s1 = f1[j] + (f1[j + 1u] - f1[j]) * u;
    return s0 + (s1 - s0) * fr;
}
