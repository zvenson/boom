/* SPDX-License-Identifier: GPL-3.0-only */
/* OberVave 3 – the sequencer's views (SEQ, again: the next): TRACKS → STEPS → TRACK → PATTERNS. Each shows its part
 * of the pattern above and its four knobs below. */
static void au_row(uint32_t t, int y, int on);
static void perf_leds(void);  /* (3.7, o3_audio.h) */
static void au_view(void);
static void knobs_small(const knob_t *k, int y, uint32_t col)
{
    for (int i = 0; i < 4; i++) {
        int x = CX0 + i * COLW;
        if (!k[i].label) continue;
        dial(x + 24, y + 30, 21, k[i].v, k[i].bip, col);
        vg_text(k[i].label, x + 50, y + 6, VG_MED, DIM, VG_LEFT);
        vg_text(k[i].value, x + 50, y + 26, VG_BIG, FG, VG_LEFT);
    }
}
static void view_tabs(void)
{
    static const uint8_t V[4] = {SV_TRACKS, SV_STEPS, SV_TRACK, SV_SLOTS};
    for (int i = 0; i < 4; i++) {
        int x = CX0 + i * COLW, on = V[i] == seq_view;
        vg_round(x, 41, COLW - 6, 26, 5, on ? KEY_RGB[CK_SEQ] : CELL);
        vg_text(V[i] == SV_STEPS && au_is(sel) ? "CUT" : V[i] == SV_STEPS && seq_shift ? "STEP+" : V[i] == SV_TRACK && seq_shift ? "TRACK+" : SV_NAME[V[i]], x + (COLW - 6) / 2, 46, VG_MED, on ? 0x0E0624 : DIM, VG_CENTER);
    }
}
static void state_text(uint32_t t, int x, int y)
{
    static const char *const ST[4] = {"STOP", "PLAY", "NEXT", "ENDS"};
    uint32_t s = trk_state(t);
    vg_text(ST[s & 3u], x, y, VG_MED, s == 1u ? GREEN : s >= 2u ? (blink() ? ACCENT : MUTED) : MUTED, VG_RIGHT);
}
static void tracks_view(void)                   /* the four in view one under the other (encoder A scrolls): sound, state, length, the bar */
{
    uint32_t w0 = bank0();
    for (uint32_t i = 0; i < 4u; i++) {
        uint32_t t = w0 + i;
        int y = 72 + (int)i * 32, on = t == sel;
        uint32_t c = TRK_RGB[t], len = pat.trk[t].len, pos = len ? ui_seq_pos[t] % len : 0u, pw = seq_pw(t), pg = pos / pw;
        char b[24], n[PATCH_NAME + 1], l[2] = {(char)('A' + t), 0};
        vg_round(CX0, y, CW - 14, 29, 4, on ? vg_mix(BG, c, 0.22f) : CELL);
        vg_round(CX0 + 4, y + 3, 23, 23, 3, c); vg_text(l, CX0 + 15, y + 6, VG_MED, 0x0E0624, VG_CENTER);
        if (au_is(t)) { au_row(t, y, on); state_text(t, 452, y + 6); continue; }   /* (3.7) an audio track */
        fmt_u(b, trk_prog[t] + 1u); prog_name(trk_prog[t], n); scat(b, " "); scat(b, n);
        if (trk_type(t) == T_MIDI) { b[0] = 0; scat(b, "MIDI · CH "); fmt_u(b + slen(b), live_p[t].v[P_MI_CH] % 16u + 1u); }   /* (0.30) its channel, whatever the sound's name */
        vg_text(b, CX0 + 34, y + 6, VG_MED, on ? FG : DIM, VG_LEFT);
        for (uint32_t s = 0; s < 16u; s++) {    /* the bar playing (stopped: the first) */
            uint32_t i = pg * pw + s; const step_t *st = seq_step(t, i);
            int x = 178 + (int)s * 12, have = i < len && st->note[0] && !(st->ratch & STEP_OFF);
            uint32_t col = i >= len || s >= pw ? BG : have ? c : CELL2;
            if (ctl_seq_run && ctl_trk_on >> t & 1u && i == pos) col = FG;
            vg_rect(x, y + 7, 10, 15, col);
        }
        if (len) {                              /* the whole track under it: a piece per bar, filled up to where it plays */
            uint32_t bars = (len + pw - 1u) / pw, run = ctl_seq_run && ctl_trk_on >> t & 1u;
            for (uint32_t k = 0; k < bars; k++) {
                int x0 = 178 + (int)(k * 190u / bars), x1 = 178 + (int)((k + 1u) * 190u / bars) - 2, w = x1 - x0;
                vg_rect(x0, y + 24, w, 2, CELL2);
                if (run && k < pg) vg_rect(x0, y + 24, w, 2, vg_mix(BG, c, 0.6f));
                else if (run && k == pg) vg_rect(x0, y + 24, (int)((pos - k * pw + 1u) * (uint32_t)w / (len - k * pw < pw ? len - k * pw : pw)), 2, FG);
            }
            if (run && bars > 1u) { fmt_u(b, pg + 1u); scat(b, "/"); fmt_u(b + slen(b), bars); }   /* playing: bar of bars */
            else fmt_u(b, len);
        } else { b[0] = 0; scat(b, "OFF"); }
        vg_text(b, 404, y + 6, VG_MED, ctl_seq_run && len > pw ? FG : DIM, VG_RIGHT);
        if (meter_of(t)) vg_text(METER[meter_of(t)].name, 172, y + 6, VG_MED, on ? ACCENT : MUTED, VG_RIGHT);   /* (not 4/4: its meter) */
        state_text(t, 452, y + 6);
    }
    {   /* (BOOM) where the four are among the six: a bar at the right edge */
        int h = 125, y0 = 72, th = h * 4 / (int)NTRK, ty = y0 + (h - th) * (int)w0 / (int)(NTRK - 4u);
        vg_rect(462, y0, 6, h, CELL); vg_rect(462, ty, 6, th, TRK_RGB[sel]);
    }
}
/* (BOOM 0.5) the DRUM GRID: 8 rows (KICK .. PERC) x the 16 steps of the bar shown; a hit in the drum's colour, brighter the
 * harder; a roll as ticks; the chosen row framed, the cursor's column marked, the playhead white */
static void drum_grid_view(void)
{
    const char *const *RN = kit_names(live_p[sel].v[P_KIT_SRC]);   /* (0.13: the acoustic kit's rows) */
    uint32_t t = sel, len = pat.trk[t].len, pg = seq_pg[t], pos = ui_seq_pos[t] % len, pw = seq_pw(t), row = drum_row[t] % NDRUM;
    const int X0 = CX0 + 44, CWD = 26, RH = 14, Y0 = 70;
    char b[40];
    for (uint32_t d = 0; d < NDRUM; d++) {
        int y = Y0 + (int)d * RH, on = d == row;
        vg_text(RN[d], CX0 + 40, y - 1, VG_SMALL, on ? DRUM_RGB[d] : MUTED, VG_RIGHT);
        for (uint32_t s = 0; s < 16u; s++) {
            uint32_t i = pg * pw + s;
            int x = X0 + (int)s * CWD, q;
            const step_t *st = seq_step(t, i);
            if (i >= len || s >= pw) continue;
            { uint32_t bm = beat_mark(t, i); vg_rect(x, y, CWD - 2, RH - 2, bm == 2u ? 0x3A1866 : bm ? 0x2A1250 : CELL); }
            q = drum_slot(st, d);
            if (q >= 0) {
                uint32_t vl = st->nv[q] ? st->nv[q] : st->vel, rn = ROLL_N[st->nr[q] % 6u];
                int off = st->ratch & STEP_OFF;
                vg_rect(x, y, CWD - 2, RH - 2, off ? CELL2 : vg_mix(BG, DRUM_RGB[d], 0.35f + 0.65f * (float)vl / 127.0f));
                if (rn > 1u && !off) for (uint32_t r = 0; r < rn; r++) vg_rect(x + 2 + (int)(r * (uint32_t)(CWD - 6) / rn), y + RH - 6, 2, 3, 0xFFFFFF);
            }
            if (ctl_seq_run && ctl_trk_on >> t & 1u && i == pos) vg_rect(x, y, CWD - 2, 2, 0xFFFFFF);
        }
        if (on) vg_frame(X0 - 2, y - 1, (int)pw * CWD + 2, RH, 1, DRUM_RGB[d]);
    }
    {   /* the cursor's (or the held pad's) column */
        uint32_t c = (step_held >= 0 ? (uint32_t)step_held : seq_cur[t] % len);
        if (c / pw == pg) vg_frame(X0 + (int)(c % pw) * CWD - 1, Y0 - 2, CWD, (int)NDRUM * RH + 2, 1, step_held >= 0 ? ACCENT : FG);
    }
    b[0] = 0; scat(b, "BAR "); fmt_u(b + slen(b), pg + 1u); scat(b, "/"); fmt_u(b + slen(b), seq_pages(t));
    if (ctl_seq_run && ctl_trk_on >> t & 1u && seq_pages(t) > 1u) { scat(b, " \xc2\xb7 PLAYS "); fmt_u(b + slen(b), pos / pw + 1u); }
    vg_text(b, 470, 186, VG_MED, ctl_seq_run && pos / pw != pg ? ACCENT : DIM, VG_RIGHT);
    if (rec) vg_text(rec == REC_ON ? "\xe2\x97\x8f REC" : "REC ARMED", CX0, 186, VG_MED, RED, VG_LEFT);
    else vg_text(seq_shift ? "SHIFT: RATCHET \xc2\xb7 CHANCE \xc2\xb7 ON/OFF" : "PADS = THIS DRUM'S STEPS \xc2\xb7 A KEY WRITES ITS DRUM HERE", CX0, 186, VG_MED, MUTED, VG_LEFT);
}
static void steps_view(void)                    /* the cursor's bar: a kit's drum grid, else the piano roll */
{
    if (au_is(sel)) { au_view(); return; }      /* (3.7) an audio track: its take */
    if (drum_grid() && pat.trk[sel].len) { drum_grid_view(); return; }   /* (BOOM 0.5) a DRUMS track: the grid */
    /* (BOOM 0.14) the PIANO ROLL: twelve pitches (the highest on top) x the 16 steps of the bar shown, the drum grid's
     * columns; a note in the track's colour, brighter the harder; the chosen row framed, black keys' rows darker, a
     * mark at the top / bottom edge where a step has notes above / below the window */
    uint32_t t = sel, len = pat.trk[t].len, pg = seq_pg[t], pos = len ? ui_seq_pos[t] % len : 0u, pw = seq_pw(t);
    uint32_t c = TRK_RGB[t], row, nr;
    uint8_t rows[ROLL_ROWS];
    int RH;
    const int X0 = CX0 + 44, CWD = 26, Y0 = 70;
    char b[40];
    if (!len) { vg_text("TRACK OFF – KNOB 1 (LENGTH) SWITCHES IT ON", 240, 120, VG_MED, DIM, VG_CENTER); return; }
    nr = roll_rows(t, rows, &RH); row = roll_row[t];
    for (uint32_t r = 0; r < nr; r++) {
        uint32_t n = rows[r], pc = n % 12u, black = 0x54Au >> pc & 1u;   /* C# D# F# G# A# */
        int y = Y0 + (int)r * RH, on = n == row;
        if (on || !pc || roll_fold || trk_type(t) == T_SMP) { fmt_note(b, n);
            if (trk_type(t) == T_SMP && live_p[t].v[P_SM_CHOP] % 3u && n >= 48u && n - 48u < (sm_n[t % 8u] ? sm_n[t % 8u] : 16u)) { b[0] = 0; scat(b, roll_fold ? "SLICE " : "S"); fmt_u(b + slen(b), n - 48u + 1u); }   /* (all keys: rows of 9 px – the short name) */   /* (0.19) the sampler: its slices */
            vg_text(b, CX0 + 40, y + (RH - 9) / 2 - 3, VG_SMALL, on ? c : MUTED, VG_RIGHT); }   /* (FOLD: every row its name – they are not neighbours) */
        for (uint32_t s = 0; s < 16u; s++) {
            uint32_t i = pg * pw + s;
            int x = X0 + (int)s * CWD, q;
            const step_t *st = seq_step(t, i);
            if (i >= len || s >= pw) continue;
            { uint32_t bm = beat_mark(t, i); vg_rect(x, y, CWD - 2, RH - 1, black ? 0x160A30 : bm == 2u ? 0x3A1866 : bm ? 0x2A1250 : CELL); }
            q = note_slot(st, n);
            if (q >= 0) {
                uint32_t vl = st->nv[q] ? st->nv[q] : st->vel;
                int off = st->ratch & STEP_OFF;
                vg_rect(x, y, st->gate >= GATE_TIE ? CWD : CWD - 2, RH - 1, off ? CELL2 : vg_mix(BG, c, 0.4f + 0.6f * (float)vl / 127.0f));
            }
        }
        if (on) vg_frame(X0 - 2, y - 1, (int)pw * CWD + 2, RH + 1, 1, c);
    }
    for (uint32_t s = 0; s < 16u; s++) {       /* notes outside the window, the playhead, the cursor */
        uint32_t i = pg * pw + s, up = 0, dn = 0;
        int x = X0 + (int)s * CWD;
        const step_t *st = seq_step(t, i);
        if (i >= len || s >= pw) continue;
        for (uint32_t q = 0; q < SEQ_CHORD && st->note[q]; q++) { if (st->note[q] > rows[0]) up = 1; if (st->note[q] < rows[nr - 1u]) dn = 1; }
        if (up) vg_rect(x + 8, Y0 - 3, 8, 2, c);
        if (dn) vg_rect(x + 8, Y0 + (int)nr * RH, 8, 2, c);
        if (ctl_seq_run && ctl_trk_on >> t & 1u && i == pos) vg_rect(x, Y0 - 1, CWD - 2, 2, 0xFFFFFF);
        if (i == seq_cur[t] % len || (int32_t)i == step_held) vg_frame(x - 1, Y0 - 1, CWD, (int)nr * RH + 1, 1, (int32_t)i == step_held ? ACCENT : FG);
    }
    b[0] = 0; scat(b, "BAR "); fmt_u(b + slen(b), pg + 1u); scat(b, "/"); fmt_u(b + slen(b), seq_pages(t));
    if (ctl_seq_run && ctl_trk_on >> t & 1u && seq_pages(t) > 1u) { scat(b, " · PLAYS "); fmt_u(b + slen(b), pos / pw + 1u); }
    if (meter_of(t)) { scat(b, " · "); scat(b, METER[meter_of(t)].name); }
    vg_text(b, 470, 186, VG_MED, ctl_seq_run && pos / pw != pg ? ACCENT : DIM, VG_RIGHT);
    if (rec) vg_text(rec == REC_ON ? "● REC" : "REC ARMED", CX0, 186, VG_MED, RED, VG_LEFT);
    else vg_text(seq_shift ? "SHIFT: RATCHET · CHANCE · ON/OFF" : roll_fold ? "FOLD: ONLY THE NOTES USED · ENTER: ALL KEYS" : "PADS = THIS NOTE'S STEPS · ENTER: FOLD", CX0, 186, VG_MED, MUTED, VG_LEFT);
}
static void track_view(void)                    /* the whole track: every step, 16 to a row */
{
    if (au_is(sel)) { au_view(); return; }
    uint32_t t = sel, len = pat.trk[t].len, pos = len ? ui_seq_pos[t] % len : 0u, pw = seq_pw(t), rows = (len + pw - 1u) / pw;
    uint32_t c = TRK_RGB[t], bs = seq_bar(t);
    int ch = rows > 8u ? 6 : rows > 4u ? 10 : 18;
    char b[32];
    if (!len) { vg_text("TRACK OFF – KNOB 1 (LENGTH) SWITCHES IT ON", 240, 120, VG_MED, DIM, VG_CENTER); return; }
    for (uint32_t i = 0; i < len; i++) {
        const step_t *st = seq_step(t, i);
        int x = CX0 + (int)(i % pw) * CW / 16, y = 72 + (int)(i / pw) * (ch + 3);
        uint32_t bm = beat_mark(t, i), col = st->note[0] && !(st->ratch & STEP_OFF) ? c : bm == 2u ? 0x3A1866 : bm ? 0x2A1250 : CELL;
        if (ctl_seq_run && ctl_trk_on >> t & 1u && i == pos) col = FG;
        vg_rect(x, y, 26, ch, col);
    }
    b[0] = 0; fmt_u(b, len); scat(b, " steps · "); fmt_u(b + slen(b), (len + bs - 1u) / bs); scat(b, (len + bs - 1u) / bs == 1u ? " bar · " : " bars · ");
    scat(b, METER[meter_of(t)].name);
    vg_text(b, CX0, 186, VG_MED, DIM, VG_LEFT);
}
static void slots_view(void)                    /* 8 patterns a page (64 in all, 3.7): playing, next, the one chosen, used / empty */
{
    uint32_t pg = slot_sel / 8u;
    for (uint32_t n = pg * 8u; n < pg * 8u + 8u && n < NPAT; n++) {
        int x = CX0 + (int)(n % 4u) * COLW, y = 72 + (int)(n % 8u / 4u) * 56, now = n == set.pat, nx = pat_queue == n + 1u;
        char b[8];
        vg_round(x, y, COLW - 6, 50, 6, now ? vg_mix(BG, KEY_RGB[CK_SEQ], 0.35f) : CELL);
        if (n == slot_sel) vg_frame(x, y, COLW - 6, 50, 2, FG);
        b[0] = 'P'; fmt_u(b + 1, n + 1u);
        vg_text(b, x + 10, y + 3, VG_BIG, now ? FG : DIM, VG_LEFT);
        vg_text(now ? "PLAYING" : nx ? "NEXT" : pat_used >> n & 1u ? "used" : "empty", x + 10, y + 30, VG_MED,
                now ? GREEN : nx ? (blink() ? ACCENT : MUTED) : MUTED, VG_LEFT);
        if (now && ctl_seq_run) {               /* the beats of the bar (track A's meter; 9/8, 12/8: smaller) */
            uint32_t nb = METER[meter_of(0)].den8 ? song_e8() : song_e8() / 2u, w = nb > 7u ? 6u : 10u;
            for (uint32_t q = 0; q < nb; q++)
                vg_rect(x + COLW - 22 - (int)((nb - 1u - q) * w), y + 8, (int)w - 3, 7, q == ui_seq_bar_beat ? FG : CELL2);
        }
    }
    {   /* which page of 8, and where the playing one is */
        char t[48]; t[0] = 0; scat(t, "PAGE "); fmt_u(t + slen(t), pg + 1u); scat(t, "/"); fmt_u(t + slen(t), (NPAT + 7u) / 8u);
        scat(t, " · KNOB 1 FAST: A PAGE"); if (set.pat / 8u != pg) { scat(t, " · PLAYING P"); fmt_u(t + slen(t), set.pat + 1u); }
        vg_text(t, CX0, 186, VG_MED, DIM, VG_LEFT);
        for (uint32_t q = 0; q < (NPAT + 7u) / 8u; q++) vg_rect(470 - (int)((NPAT + 7u) / 8u - q) * 12, 190, 9, 9, q == pg ? FG : q == set.pat / 8u ? GREEN : CELL2);
    }
}
static void seq_view_draw(void)
{
    knob_t k[4];
    view_tabs();
    if (seq_view == SV_TRACKS) tracks_view();
    else if (SV_ENTRY(seq_view)) steps_view();
    else if (seq_view == SV_TRACK) track_view();
    else slots_view();
    seq_knobs(k);
    knobs_small(k, 204, TRK_RGB[sel]);
}
static void seq_leds(void)                      /* STEPS: the steps (set: the track's colour, the playhead white); else rows */
{
    uint32_t t = sel, len = pat.trk[t].len, pos = len ? ui_seq_pos[t] % len : 0u;
    if (SV_ENTRY(seq_view) && !au_is(t)) {
        uint32_t pw = seq_pw(t);
        for (uint32_t s = 0; s < 16u; s++) {
            uint32_t i = seq_pg[t] * pw + s; const step_t *st = seq_step(t, i);
            if (i >= len || s >= pw) vibe_led_pad((int)s, 0, 0);
            else if (ctl_seq_run && i == pos) vibe_led_pad((int)s, 0xFFFFFF, 1.0f);
            else if (drum_grid()) vibe_led_pad((int)s, DRUM_RGB[drum_row[t] % NDRUM], drum_slot(st, drum_row[t] % NDRUM) >= 0 && !(st->ratch & STEP_OFF) ? 0.9f : 0.05f);   /* (the grid: the row's drum) */
            else vibe_led_pad((int)s, TRK_RGB[t], st->note[0] && !(st->ratch & STEP_OFF) ? 0.8f : 0.06f);
        }
        return;
    }
    perf_leds();                                /* (BOOM) TRACKS etc.: as the sound pages – the four in view, then the notes */
}
