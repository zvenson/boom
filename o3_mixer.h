/* SPDX-License-Identifier: GPL-3.0-only */
/* OberVave 3 – the MIXER (F1; again: the next row) as in 2.0: rows LEVEL → PAN → CHORUS → DELAY → REVERB → DRIVE →
 * MASTER. Encoders 1-4 = tracks A-D in the row (MASTER: VOLUME, FILTER, DRIVE, COMP). Pads: row 1 start / stop,
 * rows 2-3 mute (SHIFT: solo), row 4 the track. Each track shows its level, what it plays now (a meter after fader,
 * mute and solo) and MUTE / SOLO. */
static uint32_t mix_row;
#define NMROW 8u
static const char *const MIX_ROW[NMROW] = {"LEVEL", "FX", "PAN", "DELAY", "REVERB", "CHORUS", "DRIVE", "MASTER"};   /* (0.7, Sven: DELAY right after PAN) */   /* (BOOM: FX second – the sum's punch-in effects) */
#define MR_FX 1u
#define MR_PAN 2u
#define MR_MASTER 7u
static uint8_t meter[NTRK];
static uint32_t meter_t;

static uint8_t *mix_send(uint32_t row) { return row == 3u ? set.mix_dly : row == 4u ? set.mix_rev : row == 5u ? set.mix_cho : set.mix_drv; }
static void mixer_knobs(knob_t *k)
{
    static const char *const ML[NTRK] = {"A", "B", "C", "D", "E", "F", "G", "H"}, *const MM[4] = {"VOLUME", "FILTER", "DRIVE", "COMP"}, *const MF[4] = {"FILTER", "CRUSH", "DUB", "REPEAT"};
    for (uint32_t t = 0; t < 4u; t++) {
        k[t].value[0] = 0; k[t].bip = 0;
        uint32_t tr = col_trk(t);
        k[t].label = mix_row == MR_MASTER ? MM[t] : mix_row == MR_FX ? MF[t] : tr < NTRK ? ML[tr] : 0;
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
static void mixer_view(void)
{
    knob_t k[4];
    mixer_knobs(k);
    for (int i = 0; i < (int)NMROW; i++) {      /* the rows, the open one bright */
        int x = CX0 + i * 58, on = (uint32_t)i == mix_row;
        vg_round(x, 41, 54, 24, 4, on ? (i == (int)MR_MASTER ? 0xFF3D6E : i == (int)MR_FX ? 0x00E5FF : ACCENT) : CELL);
        vg_text(MIX_ROW[i], x + 27, 46, VG_BOLD, on ? 0x0E0624 : DIM, VG_CENTER);
    }
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
        vg_round(x + 50, 236, 26, 26, 4, mu ? RED : CELL2); vg_text("M", x + 63, 241, VG_MED, mu ? 0x0E0624 : DIM, VG_CENTER);
        vg_round(x + 80, 236, 26, 26, 4, so ? 0xFFE14D : CELL2); vg_text("S", x + 93, 241, VG_MED, so ? 0x0E0624 : DIM, VG_CENTER);
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
