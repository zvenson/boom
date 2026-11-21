/* SPDX-License-Identifier: GPL-3.0-only */
/* OberVave 3 – KEY and SCALE (12.11.2026, Sven: "wenn man SHIFT hält, werden die K-Tasten zu Tasten auf dem Keyboard"):
 * playing without a keyboard. SHIFT held, K1-K6 play the scale's first six degrees from the key; pads 9-16 its first
 * eight (PADS: WHITE = the white keys G4..G5 as before). PLAY: CHORDS = the triad on each degree, stacked in the scale
 * (a major key: I ii iii IV V vi on K1-K6). The settings' sc_*, the page SCALE under F6 (GLOBAL → OUTPUT → SCALE). */
enum { SC_MAJOR, SC_MINOR, SC_DORIAN, SC_PHRYG, SC_MIXO, SC_HARM, SC_PMAJ, SC_PMIN, SC_BLUES, NSCALE };
static const char *const SC_NAME[NSCALE] = {"MAJOR", "MINOR", "DORIAN", "PHRYGIAN", "MIXOLYD", "HARM MIN", "PENTA MAJ", "PENTA MIN", "BLUES"};
static const uint8_t SC_N[NSCALE] = {7, 7, 7, 7, 7, 7, 5, 5, 6};
static const uint8_t SC_IV[NSCALE][7] = {
    {0, 2, 4, 5, 7, 9, 11}, {0, 2, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 9, 10}, {0, 1, 3, 5, 7, 8, 10},
    {0, 2, 4, 5, 7, 9, 10}, {0, 2, 3, 5, 7, 8, 11}, {0, 2, 4, 7, 9}, {0, 3, 5, 7, 10}, {0, 3, 5, 6, 7, 10},
};
static const char *const SC_NOTE[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
#define SC_OK 0x5Cu

static uint8_t k_sent[6][3], pad_ch[VIBE_PADS][3];   /* the notes a held K key / pad plays + 1 (a chord: three) */

static int sc_note(int base, uint32_t deg)      /* degree deg (0 = the key) above base, up through the octaves */
{
    uint32_t s = set.sc_scale % NSCALE, n = SC_N[s];
    return base + (int)set.sc_key % 12 + 12 * (int)(deg / n) + SC_IV[s][deg % n];
}
static int sc_base(int octave) { return (set.sc_chord ? 48 : 60) + 12 * octave; }   /* (chords a little lower: they sound fuller) */

static void sc_name(char *b, uint32_t deg)      /* what degree deg plays: "D" or a chord "Dm", "B°", "C+", "Esus" */
{
    int r = sc_note(0, deg), t = sc_note(0, deg + 2u) - r, f = sc_note(0, deg + 4u) - r;
    b[0] = 0; scat(b, SC_NOTE[r % 12]);
    if (!set.sc_chord) return;
    scat(b, t == 4 && f == 7 ? "" : t == 3 && f == 7 ? "m" : t == 3 && f == 6 ? "\xC2\xB0" : t == 4 && f == 8 ? "+" : t == 3 || t == 4 ? "?" : "sus");
}
static void sc_play(uint32_t deg, int octave, uint32_t vel, uint8_t *sent)
{
    uint32_t k = 0;
    for (uint32_t j = 0; j < (set.sc_chord ? 3u : 1u); j++) {
        int n = sc_note(sc_base(octave), deg + 2u * j);
        if (n < 0 || n > 127) continue;
        sent[k++] = (uint8_t)(n + 1);
        note_in(1, (uint32_t)n, vel);
    }
}
static void sc_stop(uint8_t *sent)
{
    for (uint32_t j = 0; j < 3u; j++) if (sent[j]) { note_in(0, sent[j] - 1u, 0); sent[j] = 0; }
}
static void sc_say(uint32_t deg)                /* the toast: "Dm · D DORIAN" */
{
    static char msg[32];
    sc_name(msg, deg); scat(msg, " \xC2\xB7 "); scat(msg, SC_NOTE[set.sc_key % 12]); scat(msg, " "); scat(msg, SC_NAME[set.sc_scale % NSCALE]);
    say(msg);
}

/* the page SCALE: KEY, SCALE, PLAY, PADS */
static const char *const SC_LABEL[4] = {"KEY", "SCALE", "PLAY", "PADS"};
static void scale_knobs(knob_t *k)
{
    for (int i = 0; i < 4; i++) {
        k[i].label = SC_LABEL[i]; k[i].value[0] = 0; k[i].bip = 0;
        if (i == 0) { scat(k[i].value, SC_NOTE[set.sc_key % 12]); k[i].v = (float)(set.sc_key % 12) / 11.0f; }
        else if (i == 1) { scat(k[i].value, SC_NAME[set.sc_scale % NSCALE]); k[i].v = (float)(set.sc_scale % NSCALE) / (NSCALE - 1); }
        else if (i == 2) { scat(k[i].value, set.sc_chord ? "CHORDS" : "NOTES"); k[i].v = set.sc_chord ? 1.0f : 0.0f; }
        else { scat(k[i].value, set.sc_pads ? "WHITE" : "SCALE"); k[i].v = set.sc_pads ? 1.0f : 0.0f; }
    }
}
static void scale_turn(int i, int e)
{
    int d = e > 0 ? 1 : -1;
    if (i == 0) set.sc_key = (uint8_t)((set.sc_key % 12 + 12 + d) % 12);
    else if (i == 1) set.sc_scale = (uint8_t)clampi((int32_t)(set.sc_scale % NSCALE) + d, 0, NSCALE - 1);
    else if (i == 2) set.sc_chord = d > 0;
    else set.sc_pads = d > 0;
    set_touch();
}
static void scale_view(void)                    /* under the knobs: an octave of keys, the scale's dotted (the key orange), and what K1-K6 play */
{
    const int x0 = CX0 + 8, kw = 26, y = 202;
    uint32_t s = set.sc_scale % NSCALE, in = 0, root = set.sc_key % 12u;
    for (uint32_t j = 0; j < SC_N[s]; j++) in |= 1u << ((SC_IV[s][j] + root) % 12u);
    static const uint8_t WH[7] = {0, 2, 4, 5, 7, 9, 11}, BK[5] = {1, 3, 6, 8, 10}, BW[5] = {1, 2, 4, 5, 6};   /* BW: the white key a black one sits left of */
    for (int w = 0; w < 7; w++) {
        uint32_t n = WH[w];
        vg_round(x0 + w * kw, y, kw - 2, 62, 3, 0xD8DCE4);
        if (in >> n & 1u) vg_circle(x0 + w * kw + (kw - 2) / 2, y + 50, 6, n == root ? ACCENT : TRK_RGB[sel]);
    }
    for (int b = 0; b < 5; b++) {
        uint32_t n = BK[b]; int bx = x0 + BW[b] * kw - 9;
        vg_round(bx, y, 16, 36, 2, 0x16181D);
        if (in >> n & 1u) vg_circle(bx + 8, y + 26, 5, n == root ? ACCENT : TRK_RGB[sel]);
    }
    int x1 = x0 + 7 * kw + 12;
    vg_text("SHIFT + K1-K6", x1, y - 2, VG_MED, DIM, VG_LEFT);
    for (uint32_t d = 0; d < 6u; d++) {
        char b[8]; sc_name(b, d);
        int cx = x1 + 21 + (int)d * 43;
        vg_round(cx - 20, y + 22, 40, 26, 4, k_sent[d][0] ? TRK_RGB[sel] : CELL2);
        vg_text(b, cx, y + 27, VG_MED, k_sent[d][0] ? 0x000000 : FG, VG_CENTER);
    }
    vg_text(set.sc_pads ? "PADS 9-16: WHITE KEYS" : "PADS 9-16: THE SCALE", x1, y + 56, VG_MED, MUTED, VG_LEFT);
}
