/* SPDX-License-Identifier: GPL-3.0-only */
/* OberVave 3 – the MIXER (F1; again: the next row) as in 2.0: rows LEVEL → PAN → CHORUS → DELAY → REVERB → DRIVE →
 * MASTER. Encoders 1-4 = tracks A-D in the row (MASTER: VOLUME, FILTER, DRIVE, COMP). Pads: row 1 start / stop,
 * rows 2-3 mute (SHIFT: solo), row 4 the track. Each track shows its level, what it plays now (a meter after fader,
 * mute and solo) and MUTE / SOLO. */
static uint32_t mix_row;
#define NMROW 9u
static const char *const MIX_ROW[NMROW] = {"LEVEL", "FX", "PAN", "EQ", "DELAY", "REVERB", "CHORUS", "DRIVE", "MASTER"};   /* (0.7, Sven: DELAY right after PAN) */   /* (BOOM: FX second – the sum's punch-in effects) */   /* (0.31: EQ after PAN) */
#define MR_FX 1u
#define MR_PAN 2u
#define MR_EQ 3u                                /* (0.31) the CHOSEN track's EQ on the four knobs (as MASTER's): LOW MID FREQ HIGH, SHIFT + 1: LOW CUT */
#define MR_MASTER 8u
static uint8_t meter[NTRK];
static uint32_t meter_t;

static uint8_t *mix_send(uint32_t row) { return row == 4u ? set.mix_dly : row == 5u ? set.mix_rev : row == 6u ? set.mix_cho : set.mix_drv; }
static const char *const EQ_CUT_NAME[4] = {"OFF", "40 Hz", "80 Hz", "150 Hz"};
static void eq_db(char *s, uint32_t v)          /* 64 = 0 dB, +-15 */
{
    int32_t d = ((int32_t)v - 64) * 150 / 63;   /* tenths */
    if (d > -3 && d < 3) { scat(s, "0 dB"); return; }
    scat(s, d < 0 ? "-" : "+"); if (d < 0) d = -d;
    fmt_u(s + slen(s), (uint32_t)d / 10u); if (d < 100) { scat(s, "."); fmt_u(s + slen(s), (uint32_t)d % 10u); } scat(s, " dB");
}
static void eq_hz_txt(char *s, uint32_t v) { uint32_t hz = (uint32_t)(200.0f * vd_pow(25.0f, (float)v / 127.0f)); if (hz >= 1000u) { fmt_u(s, hz / 1000u); scat(s, "."); fmt_u(s + slen(s), hz % 1000u / 100u); scat(s, "k"); } else fmt_u(s, hz); }
static void mixer_knobs(knob_t *k)
{
    static const char *const ML[NTRK] = {"A", "B", "C", "D", "E", "F", "G", "H"}, *const MM[4] = {"VOLUME", "FILTER", "DRIVE", "COMP"}, *const MF[4] = {"FILTER", "CRUSH", "DUB", "REPEAT"};
    for (uint32_t t = 0; t < 4u; t++) {
        k[t].value[0] = 0; k[t].bip = 0;
        uint32_t tr = col_trk(t);
        k[t].label = mix_row == MR_MASTER ? MM[t] : mix_row == MR_FX ? MF[t] : tr < NTRK ? ML[tr] : 0;
        if (mix_row == MR_EQ) {                 /* (0.31) the chosen track's EQ */
            static const char *const EL[4] = {"LOW", "MID", "FREQ", "HIGH"};
            uint32_t q = sel % NTRK, v = t == 0u ? set.eq_lo[q] : t == 1u ? set.eq_mid[q] : t == 2u ? set.eq_mf[q] : set.eq_hi[q];
            k[t].label = t == 0u && seq_shift ? "LOW CUT" : EL[t];
            if (t == 0u && seq_shift) { scat(k[t].value, EQ_CUT_NAME[set.eq_cut[q] & 3u]); k[t].v = (float)(set.eq_cut[q] & 3u) / 3.0f; continue; }
            if (t == 2u) { eq_hz_txt(k[t].value, v); k[t].v = v / 127.0f; continue; }
            eq_db(k[t].value, v); k[t].v = (float)v / 128.0f; k[t].bip = 1;
            continue;
        }
        if (mix_row != MR_MASTER && mix_row != MR_FX && tr >= NTRK) continue;
        if (mix_row == MR_FX) {
            if (t == 0u) { int32_t d = (int32_t)set.mfx[0] - 64;
                if (d > -2 && d < 2) scat(k[t].value, "OFF"); else { scat(k[t].value, d < 0 ? "LP " : "HP "); fmt_u(k[t].value + 3, (uint32_t)(d < 0 ? -d : d)); }
                k[t].v = set.mfx[0] / 128.0f; k[t].bip = 1; }
            else if (t == 3u) { scat(k[t].value, REP_NAME[set.mfx[3] % 6u]); k[t].v = (float)(set.mfx[3] % 6u) / 5.0f; }
            else if (t == 2u) { static const char *const DN[4] = {"1/16", "1/8", "1/8.", "1/4"}; uint32_t v = set.mfx[2];   /* (DUB: the time it walks to) */
                if (!v) scat(k[t].value, "OFF"); else { scat(k[t].value, DN[v < 32u ? 0 : v < 64u ? 1 : v < 96u ? 2 : 3]); if (v >= 108u) scat(k[t].value, "!"); }
                k[t].v = v / 127.0f; }
            else { if (!set.mfx[t]) scat(k[t].value, "OFF"); else { fmt_u(k[t].value, ((uint32_t)set.mfx[t] * 100u + 63u) / 127u); scat(k[t].value, "%"); } k[t].v = set.mfx[t] / 127.0f; }
        } else if (mix_row == MR_MASTER) {
            if (t == 0u) { fmt_u(k[t].value, set.m_vol); k[t].v = set.m_vol / 127.0f; }
            else if (t == 1u) { int32_t d = (int32_t)set.m_dj - 64;
                if (d > -2 && d < 2) scat(k[t].value, "OFF"); else { scat(k[t].value, d < 0 ? "LP " : "HP "); fmt_u(k[t].value + 3, (uint32_t)(d < 0 ? -d : d)); }
                k[t].v = set.m_dj / 128.0f; k[t].bip = 1; }
            else { uint32_t v = t == 2u ? set.m_drive : set.m_comp; fmt_u(k[t].value, (v * 100u + 63u) / 127u); scat(k[t].value, "%"); k[t].v = v / 127.0f; }
        } else if (mix_row == 0u) { fmt_u(k[t].value, set.mix_lvl[tr]); k[t].v = set.mix_lvl[tr] / 127.0f; }
        else if (mix_row == MR_PAN) {
            int32_t pn = set.mix_pan[tr];
            if (!pn) scat(k[t].value, "C"); else { scat(k[t].value, pn < 0 ? "L" : "R"); fmt_u(k[t].value + 1, (uint32_t)(pn < 0 ? -pn : pn)); }
            k[t].v = (pn + 64) / 128.0f; k[t].bip = 1;
        } else { uint8_t v = mix_send(mix_row)[tr]; fmt_u(k[t].value, ((uint32_t)v * 100u + 63u) / 127u); scat(k[t].value, "%"); k[t].v = v / 127.0f; }
    }
}
static void mixer_turn(int k, int e)
{
    if (mix_row == MR_FX) {
        if (k == 0) set.mfx[0] = (uint8_t)clampi((int32_t)set.mfx[0] + e, 0, 128);
        else if (k == 3) set.mfx[3] = (uint8_t)clampi((int32_t)(set.mfx[3] % 6u) + (e > 0 ? 1 : -1), 0, 5);
        else set.mfx[k] = (uint8_t)clampi((int32_t)set.mfx[k] + e * 3, 0, 127);
        mix_apply(); set_touch(); return;
    }
    if (mix_row == MR_EQ) {                     /* (0.31) */
        uint32_t q = sel % NTRK;
        if (k == 0 && seq_shift) set.eq_cut[q] = (uint8_t)clampi((int32_t)(set.eq_cut[q] & 3u) + (e > 0 ? 1 : -1), 0, 3);
        else { uint8_t *v = k == 0 ? &set.eq_lo[q] : k == 1 ? &set.eq_mid[q] : k == 2 ? &set.eq_mf[q] : &set.eq_hi[q]; *v = (uint8_t)clampi((int32_t)*v + e, 0, 127); }
        mix_apply(); set_touch(); return;
    }
    if (mix_row == MR_MASTER) {
        if (k == 0) set.m_vol = (uint8_t)clampi((int32_t)set.m_vol + e, 0, 127);
        else if (k == 1) set.m_dj = (uint8_t)clampi((int32_t)set.m_dj + e, 0, 128);
        else if (k == 2) set.m_drive = (uint8_t)clampi((int32_t)set.m_drive + e * 2, 0, 127);
        else set.m_comp = (uint8_t)clampi((int32_t)set.m_comp + e * 2, 0, 127);
    } else {
        uint32_t t = col_trk((uint32_t)k);
        if (t >= NTRK) return;
        if (mix_row == 0u) set.mix_lvl[t] = (uint8_t)clampi((int32_t)set.mix_lvl[t] + e, 0, 127);
        else if (mix_row == MR_PAN) set.mix_pan[t] = (int8_t)clampi((int32_t)set.mix_pan[t] + e, -64, 64);
        else { uint8_t *sv = mix_send(mix_row); sv[t] = (uint8_t)clampi((int32_t)sv[t] + e * 2, 0, 127); }
    }
    mix_apply(); set_touch();
}
static void mixer_pad(uint32_t p, int shift_held)   /* rows 2-3: mute (SHIFT: solo), row 4: the track */
{
    uint32_t t = col_trk(p % 4u), r = p / 4u;
    if (t >= NTRK) return;
    if (r == 1u || r == 2u) {
        if (shift_held) set.mix_solo ^= (uint8_t)(1u << t); else set.mix_mute ^= (uint8_t)(1u << t);
        mix_apply(); set_touch();
    } else if (r == 3u) sel = t;
}

static void meters_take(void)                   /* the tracks' peaks: up at once, down ~2 px per 30 ms (0..44, square law) */
{
    if (o3_ms - meter_t < 30u) return;
    meter_t = o3_ms;
    for (uint32_t t = 0; t < NTRK; t++) {
        uint32_t pk = ui_trk_peak[t], m = 0;
        ui_trk_peak[t] = 0;
        while (m < 44u && pk > 6400u * (m + 1u) / 44u * (m + 1u) / 44u) m++;   /* (6400 = the output at 0 dBFS) */
        meter[t] = (uint8_t)(m > meter[t] ? m : meter[t] > 2u ? meter[t] - 2u : 0u);
    }
}
static float eq_mag_db(uint32_t q, float hz)    /* the curve the panel draws: the bands' gains, shaped (not the exact filters) */
{
    static const float CUT_HZ[4] = {0.0f, 40.0f, 80.0f, 150.0f};
    float lo = ((float)set.eq_lo[q] - 64.0f) * (15.0f / 63.0f), mi = ((float)set.eq_mid[q] - 64.0f) * (15.0f / 63.0f), hi = ((float)set.eq_hi[q] - 64.0f) * (15.0f / 63.0f);
    float l2 = vd_log2(hz), d, x, c = CUT_HZ[set.eq_cut[q] & 3u];
    x = (l2 - vd_log2(100.0f)) * 1.6f; d = lo / (1.0f + (x > 0.0f ? x * x * x : 0.0f) * 3.0f + (x > 0.0f ? x : 0.0f));
    if (x <= 0.0f) d = lo;
    x = (l2 - vd_log2(200.0f * vd_pow(25.0f, (float)set.eq_mf[q] / 127.0f))) * 1.4f; d += mi / (1.0f + x * x * 2.5f);
    x = (vd_log2(8000.0f) - l2) * 1.6f; d += x <= 0.0f ? hi : hi / (1.0f + x * x * x * 3.0f + x);
    if (c > 0.0f && hz < c * 2.0f) { float r = hz / c; d += r < 1.0f ? 40.0f * vd_log2(r) / 3.3219f : -3.0f * (2.0f - r); }
    return d;
}
static void eq_view(const knob_t *k)            /* (0.31) the chosen track's EQ: four dials, its curve */
{
    uint32_t q = sel % NTRK, c = TRK_RGB[q], i;
    char b[48];
    for (i = 0; i < 4u; i++) {
        int x = CX0 + (int)i * COLW, cx = x + COLW / 2 - 2;
        vg_text(k[i].label, cx, 74, VG_MED, i == 0u && seq_shift ? 0x2BFFB4 : DIM, VG_CENTER);
        dial(cx, 116, 26, k[i].v, k[i].bip, 0x2BFFB4);
        vg_text(k[i].value, cx, 146, VG_BIG, FG, VG_CENTER);
    }
    {   /* the curve: 20 Hz .. 20 kHz, +-15 dB */
        int X = CX0 + 4, W = CW - 8, Y = 178, H = 86, mid = Y + H / 2, py = 0;
        vg_rect(X, Y, W, H, CELL);
        vg_rect(X, mid, W, 1, CELL2);
        for (i = 0; i < 3u; i++) { static const float G[3] = {100.0f, 1000.0f, 10000.0f}; int gx = X + (int)((vd_log2(G[i]) - vd_log2(20.0f)) / (vd_log2(20000.0f) - vd_log2(20.0f)) * (float)W); vg_rect(gx, Y, 1, H, CELL2); }
        for (int xx = 0; xx < W; xx += 2) {
            float hz = 20.0f * vd_pow(1000.0f, (float)xx / (float)W), d = eq_mag_db(q, hz);
            int y = mid - (int)(d / 15.0f * (float)(H / 2 - 4));
            if (y < Y + 1) y = Y + 1; if (y > Y + H - 2) y = Y + H - 2;
            if (xx) { int a = py < y ? py : y, h = (py < y ? y - py : py - y) + 2; vg_rect(X + xx, a, 2, h, c); }
            py = y;
        }
        b[0] = 0; scat(b, "TRACK "); { char l[2] = {(char)('A' + q), 0}; scat(b, l); } scat(b, " · LOW CUT "); scat(b, EQ_CUT_NAME[set.eq_cut[q] & 3u]);
        vg_text(b, X + 6, Y + 4, VG_SMALL, FG, VG_LEFT);
        vg_text("ENC A: TRACK · SHIFT + 1: LOW CUT", X + W - 6, Y + H - 16, VG_SMALL, MUTED, VG_RIGHT);
    }
}
static void mixer_view(void)
{
    knob_t k[4];
    mixer_knobs(k);
    for (int i = 0; i < (int)NMROW; i++) {      /* the rows, the open one bright */
        int x = CX0 + i * 51, on = (uint32_t)i == mix_row;   /* (0.31: nine rows) */
        vg_round(x, 41, 48, 24, 4, on ? (i == (int)MR_MASTER ? 0xFF3D6E : i == (int)MR_FX ? 0x00E5FF : i == (int)MR_EQ ? 0x2BFFB4 : ACCENT) : CELL);
        vg_text(MIX_ROW[i], x + 24, 46, VG_BOLD, on ? 0x000000 : DIM, VG_CENTER);
    }
    if (mix_row == MR_EQ) { eq_view(k); return; }
    for (uint32_t col = 0; col < 4u; col++) {
        uint32_t t = col_trk(col);
        int fxrow = mix_row == MR_FX, mrow = mix_row == MR_MASTER, x = CX0 + (int)col * COLW, cx = x + COLW / 2 - 2;
        if (t >= NTRK && !fxrow && !mrow) continue;   /* (bank E-F: two columns empty) */
        if (t >= NTRK) t = 0;
        int on = t == sel, mu = set.mix_mute >> t & 1u, so = set.mix_solo >> t & 1u;
        int off = mu || (set.mix_solo && !so);
        uint32_t c = TRK_RGB[t];
        if (on && !mrow && !fxrow) vg_frame(x, 70, COLW - 4, 206, 2, c);
        vg_text(k[col].label, cx, 76, VG_MED, mrow || fxrow ? DIM : c, VG_CENTER);
        dial(cx, 126, 30, k[col].v, k[col].bip, mrow ? 0xFF3D6E : fxrow ? 0x00E5FF : c);
        vg_text(k[col].value, cx, 160, VG_BIG, FG, VG_CENTER);
        if (fxrow) {                            /* (MASTER FX: what each one does) */
            static const char *const FXH[4] = {"LP \xe2\x97\x80 OFF \xe2\x96\xb6 HP", "bits + rate", "turn: time + feedback", "on the beat"};
            vg_text(FXH[col], cx, 200, VG_SMALL, MUTED, VG_CENTER);
            if (col == 3u && set.mfx[3]) vg_text(blink() ? "● LOOPING" : "", cx, 222, VG_MED, 0x00E5FF, VG_CENTER);
            continue;
        }
        if (mrow) continue;                     /* (MASTER: its four knobs only) */
        {   /* the fader's level and the meter */
            int h = set.mix_lvl[t] * 72 / 127, mh = meter[t] * 72 / 44;
            vg_rect(x + 14, 196, 12, 72, CELL); vg_rect(x + 14, 268 - h, 12, h, off ? MUTED : c);
            vg_rect(x + 30, 196, 9, 72, CELL);
            for (int j = 0; j < mh; j += 3) vg_rect(x + 30, 266 - j, 9, 2, j >= 64 ? RED : j >= 50 ? 0xFFE14D : GREEN);
        }
        vg_text(ctl_trk_on >> t & 1u ? "PLAY" : "STOP", x + 78, 198, VG_MED, ctl_trk_on >> t & 1u ? GREEN : MUTED, VG_CENTER);
        vg_round(x + 50, 236, 26, 26, 4, mu ? RED : CELL2); vg_text("M", x + 63, 241, VG_MED, mu ? 0x000000 : DIM, VG_CENTER);
        vg_round(x + 80, 236, 26, 26, 4, so ? 0xFFE14D : CELL2); vg_text("S", x + 93, 241, VG_MED, so ? 0x000000 : DIM, VG_CENTER);
    }
}
static void mixer_leds(void)                    /* row 1 start/stop, rows 2-3 mute / solo, row 4 the tracks */
{
    for (uint32_t c = 0; c < 4u; c++) {
        uint32_t t = col_trk(c);
        if (t >= NTRK) { for (uint32_t r = 0; r < 4u; r++) vibe_led_pad((int)(4u * r + c), 0, 0); continue; }
        uint32_t mu = set.mix_mute >> t & 1u, so = set.mix_solo >> t & 1u;
        vibe_led_pad((int)c, GREEN, ctl_trk_on >> t & 1u ? 1.0f : 0.15f);
        vibe_led_pad((int)(4u + c), so ? 0xFFE14D : RED, mu || so ? 1.0f : 0.12f);
        vibe_led_pad((int)(8u + c), so ? 0xFFE14D : RED, mu || so ? 1.0f : 0.12f);
        vibe_led_pad((int)(12u + c), TRK_RGB[t], t == sel ? 1.0f : 0.25f);
    }
}

/* GLOBAL, SHIFT + ENTER: "RESET ALL?" – ENTER wipes everything saved and starts afresh (2.0's) */
static uint32_t reset_ask;
static void reset_all(void)
{
    static const char *const F[] = {"settings", "plob0", "plob1", "plob2", "plob3", "takes"};
    for (unsigned i = 0; i < sizeof F / sizeof F[0]; i++) vibe_store_delete(F[i]);
    for (uint32_t n = 0; n < NPAT; n++) { vibe_store_delete(PB_FILE_N(n)); vibe_store_delete(PS_FILE_N(n)); vibe_store_delete(pat_fname("paud", n)); }   /* (3.7: 64 patterns, their audio) */
    vibe_store_delete("takes2");
    vibe_request(VIBE_REQ_RESTART);
}
static void reset_view(void)
{
    vg_round(80, 70, 320, 170, 8, 0x3A0820); vg_round(82, 72, 316, 166, 7, 0x1A0614);
    vg_text("RESET ALL?", 240, 88, VG_BIG, RED, VG_CENTER);
    vg_text("Patterns, sounds and settings", 240, 128, VG_MED, FG, VG_CENTER);
    vg_text("go back to the factory state.", 240, 148, VG_MED, FG, VG_CENTER);
    vg_text("ENTER = RESET", 240, 180, VG_MED, RED, VG_CENTER);
    vg_text("any other key = cancel", 240, 206, VG_MED, DIM, VG_CENTER);
}
