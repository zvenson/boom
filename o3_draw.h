/* SPDX-License-Identifier: GPL-3.0-only */
/* OberVave 3 – the display, 480 x 320, drawn for VIBE (the style of GROOVE and the VIBE pages of 2.0): a header,
 * the page over the full width, the soft keys below */
#define BG 0x0E0624   /* (BOOM: neon on deep violet – not OberVave's grey-blue) */
#define FG 0xF6F0FF
#define DIM 0xB79BE0
#define MUTED 0x7458A8
#define CELL 0x1C0B3A
#define CELL2 0x351763
#define SEP 0x2C1354
#define RED 0xFF3D6E
#define GREEN 0x2BFFB4
#define ACCENT 0xFF2E97
/* the six colour keys, left to right as the signal goes (Sven 20.10.): the sound (oscillators, filter), what shapes it,
 * how it is played, its effects, the sequencer. Each keeps its colour wherever it sits. */
enum { CK_SOUND, CK_ENV, CK_LFO, CK_ARP, CK_FX, CK_SEQ };
static const char *const KEY_NAME[6] = {"SOUND", "ENV", "LFO", "ARP", "FX", "SEQ"};
static const uint32_t KEY_RGB[6] = {0xC77DFF, 0xFFE14D, 0x2BFFB4, 0xFF8A1E, 0x00E5FF, 0xFF2E97};

static const char *toast; static uint32_t toast_t;
static void say(const char *s) { toast = s; toast_t = o3_ms; }
static int blink(void) { return (int)(o3_ms / 300u) & 1; }

static void dial(int cx, int cy, int r, float v, int bip, uint32_t col)
{
    const float span = 0.75f;                   /* from 7:30 round to 4:30 */
    float px = 0, py = 0;
    for (int i = 0; i <= 48; i++) {
        float t = i / 48.0f, a = 0.625f - span * t;
        float x = cx + vd_sin(a + 0.25f) * r, y = cy - vd_sin(a) * r;
        int on = bip ? (v >= 0.5f ? (t >= 0.5f && t <= v) : (t <= 0.5f && t >= v)) : t <= v;
        if (i) vg_line(px, py, x, y, 4.0f, on ? col : CELL2);
        px = x; py = y;
    }
    float a = 0.625f - span * vd_clamp(v, 0, 1);
    vg_circle((float)cx, (float)cy, r - 9.0f, CELL2);
    vg_line((float)cx, (float)cy, cx + vd_sin(a + 0.25f) * (r - 11), cy - vd_sin(a) * (r - 11), 3.0f, FG);
}

/* the page area: the full width (no key column – the colour keys light themselves, the page's tabs carry its colour,
 * MENU explains every control): x 10..470, four columns of 115 px */
#define CX0 10
#define CW 460
#define COLW 115
static int colx(int i) { return CX0 + i * COLW + COLW / 2; }

/* four knobs of a page: label, the dial, the value – big enough to read on the device */
typedef struct { const char *label; char value[16]; float v; int bip; } knob_t;
static void knobs_row(const knob_t *k, int y, uint32_t col)
{
    for (int i = 0; i < 4; i++) {
        int cx = colx(i);
        if (!k[i].label) continue;
        vg_text(k[i].label, cx, y, VG_MED, DIM, VG_CENTER);
        dial(cx, y + 58, 38, k[i].v, k[i].bip, col);
        vg_text(k[i].value, cx, y + 102, VG_BIG, FG, VG_CENTER);
    }
}

static void softkeys(const char *const *f, uint32_t on_mask)
{
    vg_rect(0, 282, VIBE_SCREEN_W, 38, 0x14072E);
    for (int i = 0; i < 6; i++) {
        int x = 40 + i * 80, on = on_mask >> i & 1u;
        vg_round(x - 37, 286, 74, 30, 5, on ? ACCENT : CELL2);
        vg_text(f[i], x, 293, VG_MED, on ? 0x0E0624 : FG, VG_CENTER);
        vibe_label(VB_F1 + i, f[i]);
    }
    for (int i = 0; i < 6; i++) vibe_label(VB_K1 + i, KEY_NAME[i]);   /* (SHIFT held: main.c names them by the scale) */
}
static void track_letters(int x, int y)         /* A-H (0.11: 11 px apart, eight fit where six stood): the selected one bright and underlined, muted ones red */
{
    for (uint32_t t = 0; t < NTRK; t++) {
        char l[2] = {(char)('A' + t), 0};
        int mu = set.mix_mute >> t & 1u, on = t == sel, arm = (int)(au_armed() >> t & 1u);
        uint32_t c = mu ? RED : TRK_RGB[t];
        if (!on && !mu) c = vg_mix(BG, c, 0.45f);
        if (arm) {                              /* (0.29) ARMED: the letter white on red – bright while it records */
            vg_round(x + (int)t * 11 - 5, y - 2, 11, 21, 3, (ctl_au_rq >> t & 1u) && (ui_au_state == 2u || blink()) ? RED : 0x8A1030);
            c = 0xFFFFFF;
        }
        vg_text(l, x + (int)t * 11, y, VG_MED, c, VG_CENTER);
        if (mu) vg_rect(x + (int)t * 11 - 4, y + 9, 8, 2, RED);
        if (on) vg_rect(x + (int)t * 11 - 4, y + 20, 8, 2, TRK_RGB[t]);
    }
}
static void out_meters(int x, int y, int h)     /* the output, left and right (after MASTER): two thin bars */
{
    float lv[2] = { lvl_l, lvl_r };
    for (int c = 0; c < 2; c++) {
        float v = lv[c] > 0.00001f ? 1.0f + vd_log2(lv[c]) / 8.0f : 0.0f;   /* 48 dB over the height */
        int m = (int)(v * h); if (m < 0) m = 0; if (m > h) m = h;
        vg_rect(x + c * 7, y, 5, h, CELL);
        vg_rect(x + c * 7, y + h - m, 5, m, lv[c] > 0.79f ? RED : lv[c] > 0.5f ? 0xFFE14D : GREEN);
    }
}
static int in_meter_on(void)                    /* (3.8.1) an audio track takes from IN 1 / IN 2 – its level after GAIN is shown */
{
    for (uint32_t t = 0; t < NAU; t++) if ((ctl_au_type >> t & 1u) && au_c.src[t] % NAUSRC != AU_RES) return 1;
    return 0;
}
static void in_meter(int x, int y, int h)       /* the input, one thin bar left of the output's; red when it clips (>= 0 dBFS) */
{
    if (!in_meter_on()) return;
    float v = lvl_in > 0.00001f ? 1.0f + vd_log2(lvl_in) / 8.0f : 0.0f;
    int m = (int)(v * h); if (m < 0) m = 0; if (m > h) m = h;
    vg_rect(x, y, 5, h, CELL);
    vg_rect(x, y + h - m, 5, m, lvl_in > 0.99f ? RED : lvl_in > 0.5f ? 0xFFE14D : 0x00E5FF);
}
/* the header: the page (in its key's colour), the pattern, A-D, the sound, the tempo, play, the output, the input */
static void header(const char *title, uint32_t title_rgb)
{
    char b[24], n[PATCH_NAME + 1];
    vg_text(title, 12, 8, VG_MED, title_rgb, VG_LEFT);
    track_letters(172, 7);
    if (ctl_au_type >> sel & 1u) vg_text("AUDIO TRACK", 272, 8, VG_MED, TRK_RGB[sel], VG_LEFT);   /* (3.7) */
    else if (trk_type(sel) == T_MIDI) {         /* (0.30) a MIDI track: its channel in the header */
        fmt_u(b, live_p[sel].v[P_MI_CH] % 16u + 1u); { char m[16] = "MIDI · CH "; scat(m, b); vg_text(m, 272, 8, VG_MED, TRK_RGB[sel], VG_LEFT); }
    }
    else {
        fmt_u(b, trk_prog[sel] + 1u); prog_name(trk_prog[sel], n);
        vg_text(b, 282, 8, VG_MED, TRK_RGB[sel], VG_RIGHT);
        vg_text(n, 287, 8, VG_MED, trk_edited[sel] ? ACCENT : FG, VG_LEFT);
    }
    fmt_u(b, ctl_daw_quarter ? (60u * obv_fs + ctl_daw_quarter / 2u) / ctl_daw_quarter : set.tempo);   /* (a DAW's tempo: green) */
    vg_text(b, 422, 8, VG_MED, ctl_daw_quarter ? GREEN : DIM, VG_RIGHT);
    if (ctl_seq_run) vg_tri(427, 9, 439, 16, 427, 23, GREEN); else vg_rect(427, 10, 11, 11, MUTED);
    in_meter(446, 6, 24);
    out_meters(458, 6, 24);
    for (int i = 0; i < CW; i += 4) vg_rect(10 + i, 33, 4, 2, vg_mix(0xFF2E97, 0x00E5FF, (float)i / CW));   /* (BOOM: a neon line, pink to cyan) */
}
static void toast_draw(void)
{
    if (!toast || o3_ms - toast_t > 1500u) return;
    int w = vg_text_width(toast, VG_MED) + 32;
    vg_round(240 - w / 2, 234, w, 32, 6, ACCENT);
    vg_text(toast, 240, 241, VG_MED, 0x0E0624, VG_CENTER);
}
