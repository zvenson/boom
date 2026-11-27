/* SPDX-License-Identifier: GPL-3.0-only */
/* BOOM 0.41 (Sven 21.11.2026: "MEL und DX zusammenlegen") – a MEL sound (4-op FM) written as a DX7 voice: the bank "MEL"
 * among the DX sounds (slots 33-48). The four operators go onto four of the DX7's six, in the DX7 algorithm that has the
 * same wiring and the feedback on the same operator; the other two stay silent (level 0). What is approximated:
 *  - the levels: MEL's level² x 0.8 turns of modulation, the DX7's 0.75 dB a step (measured on dx7_core.h: 99 = 1.48 turns)
 *  - the times: MEL's attack / decay times onto the DX7's rates (measured: a rate's time to full / to -60 dB)
 *  - the carriers carry MEL's AMP ENV in their own envelope (attack, decay to sustain, release) – a DX7 has no extra one
 *  - ratio 0.25 does not exist on a DX7 (its lowest is 0.5): such an operator sounds an octave higher
 *  - velocity: on the carriers' level (sensitivity 3), as MEL's 0.25 .. 1 on the amplitude
 * The rest of the sound (filter, its envelope, LFO, mode, glide, arp, volume) stays as it is: DX plays those too. */
#define M2D_T60 1.27f                           /* a MEL decay knob's time x this = the DX7 rate's "to -60 dB" (2.3 in theory; 1.27 measured, A/B) */
static float m2d_ms(uint32_t v) { return (float)TIME_MS_X10[v & 127u] * 0.0001f; }   /* a MEL time knob in seconds */
static int m2d_rate(float sec, const float (*tb)[2], uint32_t n)   /* a time → the DX7 rate that takes it (tb: rate, seconds, slow first) */
{
    if (sec >= tb[0][1]) return (int)tb[0][0];
    for (uint32_t i = 1; i < n; i++)
        if (sec >= tb[i][1]) {                  /* between i-1 and i: on a log scale */
            float a = vd_log2(tb[i - 1][1]), b = vd_log2(tb[i][1]), u = (vd_log2(sec) - a) / (b - a);
            return clampi((int)(tb[i - 1][0] + u * (tb[i][0] - tb[i - 1][0]) + 0.5f), 0, 99);
        }
    return 99;
}
static const float M2D_ATT[][2] = {{9, 12.88f}, {18, 4.60f}, {27, 1.617f}, {36, 0.574f}, {45, 0.2547f}, {54, 0.0849f}, {63, 0.0312f}, {72, 0.0102f}, {81, 0.0044f}, {90, 0.0007f}};
static const float M2D_DEC[][2] = {{18, 31.65f}, {27, 11.08f}, {36, 3.96f}, {45, 1.735f}, {54, 0.586f}, {63, 0.221f}, {72, 0.076f}, {81, 0.034f}, {90, 0.013f}, {99, 0.005f}};
static int m2d_level(float amp)                 /* an amplitude (1 = the DX7's level 99) → its level 0..99 */
{
    static const float LO[6] = {0.0f, 0.0003f / 1.48f, 0.00122f / 1.48f, 0.0029f / 1.48f, 0.00632f / 1.48f, 0.01378f / 1.48f};   /* (levels 0, 9 .. 45) */
    if (amp <= 0.0f) return 0;
    if (amp >= LO[5]) return clampi((int)(99.0f + 8.0f * vd_log2(amp) + 0.5f), 45, 99);
    for (uint32_t i = 1; i < 6u; i++) if (amp < LO[i]) {
        float a = i > 1u ? vd_log2(LO[i - 1]) : vd_log2(LO[1]) - 4.0f, b = vd_log2(LO[i]), u = (vd_log2(amp) - a) / (b - a);
        return clampi((int)((float)(i - 1u) * 9.0f + u * 9.0f + 0.5f), 0, 45);
    }
    return 45;
}
static void m2d_ratio(uint32_t ri, uint8_t *coarse, uint8_t *fine)   /* MEL's ratio → the DX7's coarse (0 = 0.5, 1..31) and fine (x 1.00 .. 1.99) */
{
    float r = (float)RATIO_X100[ri % NRATIO] * 0.01f;
    if (r < 0.75f) { *coarse = 0; *fine = 0; return; }   /* (0.25, 0.5: 0.5) */
    if (r < 1.0f) { *coarse = 0; *fine = (uint8_t)clampi((int)((r / 0.5f - 1.0f) * 100.0f + 0.5f), 0, 99); return; }
    uint32_t c = (uint32_t)r;
    *coarse = (uint8_t)c; *fine = (uint8_t)clampi((int)((r / (float)c - 1.0f) * 100.0f + 0.5f), 0, 99);
}
/* MEL's algorithms (bm_engine.h ALGO_MOD): the DX7 algorithm (0-based) and where MEL's ops 1-4 go (DX7 operator numbers) */
static const uint8_t M2D_ALG[NALGO] = {0, 13, 6, 12, 4, 21, 28, 31};
static const uint8_t M2D_OP[NALGO][4] = {{3, 4, 5, 6}, {3, 4, 5, 6}, {3, 4, 5, 6}, {3, 4, 5, 6}, {1, 2, 5, 6}, {3, 4, 5, 6}, {1, 2, 5, 6}, {3, 4, 5, 6}};
static const uint8_t ALGO_CAR_M2D[NALGO] = {0x1, 0x1, 0x1, 0x1, 0x5, 0x7, 0x7, 0xF};   /* (= ALGO_CAR: MEL's carriers) */
static void mel_to_dx(const uint8_t *pv, uint8_t *o, const char *name)
{
    uint32_t algo = pv[P_ALGO] % NALGO, car = ALGO_CAR_M2D[algo];
    float aa = m2d_ms(pv[P_AA]), ad = m2d_ms(pv[P_AD]) * M2D_T60, ar = m2d_ms(pv[P_AR]) * M2D_T60, as = (float)pv[P_AS] / 127.0f;
    memset(o, 0, DX_VOICE);
    for (uint32_t op = 0; op < 6u; op++) {      /* every operator silent and neutral first */
        uint8_t *q = o + op * 21u;
        q[0] = q[1] = q[2] = q[3] = 99; q[4] = q[5] = q[6] = 99; q[7] = 0; q[8] = 39; q[18] = 1; q[20] = 7;
    }
    for (uint32_t k = 0; k < 4u; k++) {
        uint32_t pb = P_OP1_RATIO + 4u * k, dxop = M2D_OP[algo][k];
        uint8_t *q = o + (6u - dxop) * 21u;     /* (the voice keeps OP 6 first) */
        float l = (float)pv[pb + 1u] / 127.0f, at = m2d_ms(pv[pb + 2u]), dt = m2d_ms(pv[pb + 3u]) * M2D_T60;
        int holds = pv[pb + 3u] >= 127u, iscar = (int)(car >> k & 1u);
        m2d_ratio(pv[pb], &q[18], &q[19]);
        if (iscar) {                            /* a carrier: MEL's level² against the loudest carrier, the AMP ENV in its envelope */
            q[16] = (uint8_t)m2d_level(l * l);   /* (as loud as the DX7 goes: a DX voice is ~5x quieter than MEL's – VOLUME makes up the rest) */
            q[15] = 3;
            q[0] = (uint8_t)m2d_rate(at > aa ? at : aa, M2D_ATT, 10); q[4] = 99;
            if (!holds) {                       /* it dies away by itself (and the AMP ENV's decay, if that is shorter to nothing) */
                float d = as < 0.01f && ad < dt ? ad : dt;
                q[1] = q[2] = (uint8_t)m2d_rate(d, M2D_DEC, 10); q[5] = q[6] = 0;
            } else {
                int sl = as >= 0.999f ? 99 : m2d_level(as);
                q[1] = q[2] = (uint8_t)m2d_rate(ad, M2D_DEC, 10); q[5] = q[6] = (uint8_t)sl;
            }
        } else {                                /* a modulator: MEL's level² x 0.8 turns (= 0.54 of the DX7's 1.48 at 99) */
            q[16] = (uint8_t)m2d_level(l * l * 0.8f / 1.48f);
            q[0] = (uint8_t)m2d_rate(at, M2D_ATT, 10); q[4] = 99;
            if (!holds) { q[1] = q[2] = (uint8_t)m2d_rate(dt, M2D_DEC, 10); q[5] = q[6] = 0; }
        }
        q[3] = (uint8_t)m2d_rate(ar, M2D_DEC, 10); q[7] = 0;
        if (!l) q[16] = 0;
    }
    for (uint32_t i = 0; i < 4u; i++) { o[126 + i] = 99; o[130 + i] = 50; }   /* the pitch envelope: flat */
    o[134] = M2D_ALG[algo];
    if (pv[P_FB]) {                             /* MEL: the mean of its last two outputs x FB/127 x 0.4 – the DX7 (dx_op_fb): x 0.5 at 7, half each step down */
        int fb = (int)(7.0f + vd_log2((float)pv[P_FB] / 127.0f * 0.8f) + 0.5f);   /* (op 4 is the feedback operator in every mapping) */
        o[135] = (uint8_t)clampi(fb, 1, 7);
    }
    o[137] = 35; o[138] = 0; o[139] = 0; o[140] = 0; o[141] = 1; o[142] = 0; o[143] = 0;   /* its own LFO off (MEL's LFO stays the sound's) */
    o[144] = 24;                                /* (no transposition) */
    for (uint32_t i = 0, end = !name; i < 10u; i++) { if (!end && !name[i]) end = 1; o[145 + i] = end ? ' ' : (uint8_t)name[i]; }
}
