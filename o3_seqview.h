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
        vg_text(V[i] == SV_STEPS && au_is(sel) ? "CUT" : V[i] == SV_STEPS && seq_shift ? "STEP+" : V[i] == SV_TRACK && seq_shift ? "TRACK+" : SV_NAME[V[i]], x + (COLW - 6) / 2, 46, VG_MED, on ? 0x000000 : DIM, VG_CENTER);
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
        vg_round(CX0, y, CW - 14, 29, 4, on ? vg_mix(BG, c, 0.38f) : CELL);
        if (on) vg_frame(CX0, y, CW - 14, 29, 2, c);   /* (0.32, Sven: "deutlicher") the selected one framed in its colour */
        vg_round(CX0 + 4, y + 3, 23, 23, 3, c); vg_text(l, CX0 + 15, y + 6, VG_MED, 0x000000, VG_CENTER);
        if (au_is(t)) { au_row(t, y, on); state_text(t, 452, y + 6); continue; }   /* (3.7) an audio track */
        fmt_u(b, prog_num(trk_prog[t])); prog_name(trk_prog[t], n); scat(b, " "); scat(b, n);
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
/* (0.41) the two lines over STEPS' grid and TRACK's time line: the PLAYHEAD white, running smoothly through the step; the
 * EDIT line (the cursor) pink with a handle on top */
static void play_line(int x, int y0, int h) { vg_rect(x - 1, y0, 2, h, 0xFFFFFF); }
static void edit_line(int x, int y0, int h) { vg_rect(x - 1, y0, 2, h, ACCENT); vg_rect(x - 4, y0 - 3, 8, 3, ACCENT); }
static int seq_playing(uint32_t t) { return ctl_seq_run && (ctl_trk_on >> t & 1u); }
static uint32_t seq_fr(uint32_t t) { return ui_seq_frac[t] > 255u ? 255u : ui_seq_frac[t]; }
static void grid_lines(uint32_t t, int X0, int CWD, int y0, int h)   /* the page's steps, CWD px each */
{
    uint32_t len = pat.trk[t].len, pw = seq_pw(t), pg = seq_pg[t], pos = len ? ui_seq_pos[t] % len : 0u, cur = len ? seq_cur[t] % len : 0u;
    if (!len) return;
    if (cur / pw == pg) edit_line(X0 + (int)(cur % pw) * CWD - 1, y0, h);
    if (seq_playing(t) && pos / pw == pg) play_line(X0 + (int)(pos % pw) * CWD + (int)(seq_fr(t) * (uint32_t)CWD / 256u), y0, h);
}
static const char *follow_text(uint32_t t) { return !seq_playing(t) || seq_pages(t) < 2u ? "" : seq_hold >> t & 1u ? " \xc2\xb7 HOLD" : " \xc2\xb7 FOLLOW"; }
/* (BOOM 0.5) the DRUM GRID: 8 rows (KICK .. PERC) x the 16 steps of the bar shown; a hit in the drum's colour, brighter the
 * harder; a roll as ticks; the chosen row framed, the cursor's column marked, the playhead white */
static uint32_t roll_drum(uint32_t n) { for (uint32_t d = 0; d < NDRUM; d++) if (DRUM_NOTE[d] == n) return d; return drum_of_note(n); }
static void step_strip(uint32_t t)              /* (0.32) STEPS: the whole track in a strip above the bar – its notes, the bars,
                                                 * the bar the pads edit framed, the cursor, the playhead */
{
    uint32_t len = pat.trk[t].len, pw = seq_pw(t), pg = seq_pg[t], bs = seq_bar(t), c = TRK_RGB[t];
    const int Y = 69, H = 10;
    if (!len) return;
    vg_rect(CX0, Y, CW, H, CELL);
    for (uint32_t i = 0; i < len; i++) {
        const step_t *st = seq_step(t, i);
        int x = CX0 + (int)((uint64_t)i * CW / len), w = (int)((uint64_t)(i + 1u) * CW / len) - (x - CX0) - (len <= 64u ? 1 : 0);
        if (bs && i % bs == 0u && i) vg_rect(x, Y, 1, H, DIM);
        if (st->note[0] && !(st->ratch & STEP_OFF)) vg_rect(x, Y + 2, w < 1 ? 1 : w, H - 4, trk_type(t) == T_DRUMS ? DRUM_RGB[roll_drum(st->note[0]) % NDRUM] : c);
    }
    {
        int x0 = CX0 + (int)((uint64_t)(pg * pw) * CW / len), x1 = CX0 + (int)((uint64_t)((pg + 1u) * pw > len ? len : (pg + 1u) * pw) * CW / len);
        vg_frame(x0, Y - 1, x1 - x0, H + 2, 1, FG);
        x0 = CX0 + (int)((uint64_t)(seq_cur[t] % len) * CW / len);
        vg_rect(x0, Y + H - 2, (int)(CW / len) > 2 ? (int)(CW / len) : 2, 2, ACCENT);   /* (the cursor) */
    }
    if (ctl_seq_run && ctl_trk_on >> t & 1u) vg_rect(CX0 + (int)((uint64_t)(ui_seq_pos[t] % len) * CW / len), Y, 2, H, 0xFFFFFF);
}
static void drum_grid_view(void)
{
    const char *const *RN = kit_names(live_p[sel].v[P_KIT_SRC]);   /* (0.13: the acoustic kit's rows) */
    uint32_t t = sel, len = pat.trk[t].len, pg = seq_pg[t], pos = ui_seq_pos[t] % len, pw = seq_pw(t), row = drum_row[t] % NDRUM;
    const int X0 = CX0 + 44, CWD = 26, RH = 12, Y0 = 83;   /* (0.32: the strip above) */
    char b[40];
    step_strip(t);
    for (uint32_t d = 0; d < NDRUM; d++) {
        int y = Y0 + (int)d * RH, on = d == row;
        vg_text(RN[d], CX0 + 40, y - 2, VG_SMALL, on ? DRUM_RGB[d] : MUTED, VG_RIGHT);
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
        }
        if (on) vg_frame(X0 - 2, y - 1, (int)pw * CWD + 2, RH, 1, DRUM_RGB[d]);
    }
    if (step_held >= 0 && (uint32_t)step_held / pw == pg)   /* the held pad's column */
        vg_frame(X0 + (int)((uint32_t)step_held % pw) * CWD - 1, Y0 - 2, CWD, (int)NDRUM * RH + 2, 1, ACCENT);
    grid_lines(t, X0, CWD, Y0 - 2, (int)NDRUM * RH + 2);   /* (0.41) the edit line, the playhead */
    b[0] = 0; scat(b, "BAR "); fmt_u(b + slen(b), pg + 1u); scat(b, "/"); fmt_u(b + slen(b), seq_pages(t));
    if (ctl_seq_run && ctl_trk_on >> t & 1u && seq_pages(t) > 1u) { scat(b, " \xc2\xb7 PLAYS "); fmt_u(b + slen(b), pos / pw + 1u); }
    scat(b, follow_text(t));
    vg_text(b, 470, 186, VG_MED, ctl_seq_run && pos / pw != pg ? ACCENT : DIM, VG_RIGHT);
    if (rec) vg_text(rec == REC_ON ? "\xe2\x97\x8f REC" : "REC ARMED", CX0, 186, VG_MED, RED, VG_LEFT);
    else vg_text(seq_shift ? "SHIFT: RATCHET \xc2\xb7 CHANCE \xc2\xb7 ON/OFF" : "DATA: STEP \xc2\xb7 PUSH: ON/OFF \xc2\xb7 PADS TOO", CX0, 186, VG_MED, MUTED, VG_LEFT);
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
    const int X0 = CX0 + 44, CWD = 26, Y0 = 83;   /* (0.32: the strip above) */
    char b[40];
    if (!len) { vg_text("TRACK OFF – KNOB 1 (LENGTH) SWITCHES IT ON", 240, 120, VG_MED, DIM, VG_CENTER); return; }
    step_strip(t);
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
            { uint32_t bm = beat_mark(t, i); vg_rect(x, y, CWD - 2, RH - 1, black ? 0x0C0518 : bm == 2u ? 0x3A1866 : bm ? 0x2A1250 : CELL); }
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
        if ((int32_t)i == step_held) vg_frame(x - 1, Y0 - 1, CWD, (int)nr * RH + 1, 1, ACCENT);
    }
    grid_lines(t, X0, CWD, Y0 - 1, (int)nr * RH + 1);   /* (0.41) the edit line, the playhead */
    b[0] = 0; scat(b, "BAR "); fmt_u(b + slen(b), pg + 1u); scat(b, "/"); fmt_u(b + slen(b), seq_pages(t));
    if (ctl_seq_run && ctl_trk_on >> t & 1u && seq_pages(t) > 1u) { scat(b, " · PLAYS "); fmt_u(b + slen(b), pos / pw + 1u); }
    if (meter_of(t)) { scat(b, " · "); scat(b, METER[meter_of(t)].name); }
    scat(b, follow_text(t));
    vg_text(b, 470, 186, VG_MED, ctl_seq_run && pos / pw != pg ? ACCENT : DIM, VG_RIGHT);
    if (rec) vg_text(rec == REC_ON ? "● REC" : "REC ARMED", CX0, 186, VG_MED, RED, VG_LEFT);
    else vg_text(seq_shift ? "SHIFT: RATCHET · CHANCE · ON/OFF" : roll_fold ? "FOLD: ONLY THE NOTES USED · ENTER: ALL KEYS" : "DATA: STEP · PUSH: ON/OFF · ENTER: FOLD", CX0, 186, VG_MED, MUTED, VG_LEFT);
}
static void tm_view(uint32_t t)                 /* (0.32) TRACK: the track as a time line, as long as it is – its notes where they
                                                 * are played, how high, how long; the beats and bars behind them. TIME: the take,
                                                 * the grid it would snap to, START / END. A DRUMS track: a lane a drum */
{
    const seqtrk_t *k = &pat.trk[t];
    uint32_t tm = k->tm, grow = tm && (ctl_tm_grow >> t & 1u), c = TRK_RGB[t], bt = tm_beat(t), bar = tm_bar(t);
    uint32_t fr = ui_seq_frac[t] > 255u ? 255u : ui_seq_frac[t], head;
    uint32_t span, lo = 127u, hi = 0u, n, run = ctl_seq_run && (ctl_trk_on >> t & 1u), dr = trk_type(t) == T_DRUMS;
    uint32_t ts = tm ? k->ts : 0u, te = tm ? k->te : k->len * TM_FINE;
    char b[72], l[12];
    head = (tm ? ui_seq_pos[t] % SEQ_STEPS : k->len ? ui_seq_pos[t] % k->len : 0u) * TM_FINE + fr / 2u;
    if (!tm && !k->len) { vg_text("TRACK OFF – KNOB 1 (LENGTH) SWITCHES IT ON", 240, 120, VG_MED, DIM, VG_CENTER); return; }
    vg_rect(CX0, 72, CW, 108, CELL);
    if (grow) { span = head + bt; span = (span + bar - 1u) / bar * bar; if (span < bar) span = bar; }
    else span = te ? (te > k->len * TM_FINE ? te : k->len * TM_FINE) : 0u;
    if (grow) { vg_rect(CX0, 72, CW, 2, RED); vg_rect(CX0, 178, CW, 2, RED); }
    if (!span) vg_text("TIME – REC RECORDS FREELY, REC AGAIN CLOSES THE LOOP", 240, 116, VG_MED, DIM, VG_CENTER);
    else {
        if (dr) for (uint32_t d = 1; d < NDRUM; d++) vg_rect(CX0, 78 + (int)d * 12 - 1, CW, 1, SEP);   /* (the lanes) */
        if (span / bt <= 230u) for (uint32_t g = bt; g < span; g += bt) {   /* the beats, the bars brighter and numbered */
            int x = CX0 + (int)((uint64_t)g * CW / span), isbar = bar && g % bar == 0u;
            vg_rect(x, 72, isbar ? 2 : 1, 108, isbar ? DIM : MUTED);
            if (span / bt <= 16u) { tm_bbt(t, l, g); vg_text(l, x + 3, 74, VG_SMALL, isbar ? DIM : MUTED, VG_LEFT); }   /* (few beats: each one's BAR.BEAT) */
            else if (isbar && span / bar <= 24u) { fmt_u(l, g / bar + 1u); vg_text(l, x + 3, 74, VG_SMALL, DIM, VG_LEFT); }
        }
        if (!grow && (ts || te < span)) {       /* outside START - END: dimmed (drawn over the grid, under the notes) */
            int x0 = CX0 + (int)((uint64_t)ts * CW / span), x1 = CX0 + (int)((uint64_t)te * CW / span);
            for (int y = 72; y < 180; y += 2) { vg_rect(CX0, y, x0 - CX0, 1, BG); vg_rect(x1, y, CX0 + CW - x1, 1, BG); }
        }
        if (tm && !grow && te) {
            int x0 = CX0 + (int)((uint64_t)ts * CW / span), x1 = CX0 + (int)((uint64_t)te * CW / span);
            vg_rect(x0, 72, 2, 108, GREEN); vg_rect(x1 - 2, 72, 2, 108, RED);
        }
        for (uint32_t i = 0; i < k->len && i < SEQ_STEPS; i++) for (n = 0; n < SEQ_CHORD && seq_step(t, i)->note[n]; n++) {
            uint32_t m = seq_step(t, i)->note[n]; if (m < lo) lo = m; if (m > hi) hi = m;
        }
        if (hi < lo) lo = hi = 60u;
        if (hi - lo < 12u) { lo = lo > 6u ? lo - 6u : 0u; hi = lo + 12u; }
        for (uint32_t i = 0; i < k->len && i < SEQ_STEPS; i++) {   /* the notes: where, how high, how long */
            const step_t *st = seq_step(t, i);
            uint32_t f = i * TM_FINE + st->nudge, gl = st->gate >= GATE_TIE ? TM_FINE : (uint32_t)st->gate * TM_FINE / 128u;
            if (!st->note[0] || f >= span) continue;
            for (n = 0; n < SEQ_CHORD && st->note[n]; n++) {
                uint32_t vl = st->nv[n] ? st->nv[n] : st->vel, d = dr ? roll_drum(st->note[n]) % NDRUM : 0u, nc = dr ? DRUM_RGB[d] : c;
                int x = CX0 + (int)((uint64_t)f * CW / span), w = (int)((uint64_t)gl * CW / span);
                int y = dr ? 78 + (int)d * 12 + 4 : 172 - (int)((st->note[n] - lo) * 94u / (hi - lo));
                if (dr) w = (int)((uint64_t)TM_FINE * CW / span) - 1;   /* (a drum: a hit, a step wide) */
                if (w < 3) w = 3;
                if (x + w > CX0 + CW) w = CX0 + CW - x;
                vg_rect(x, y - 2, w, 5, (st->ratch & STEP_OFF) || (!grow && (f < ts || f >= te)) ? MUTED : vg_mix(BG, nc, 0.45f + 0.55f * (float)vl / 127.0f));   /* (outside the loop / OFF: not played; brighter the harder) */
            }
        }
        if (!tm && k->len) edit_line(CX0 + (int)((uint64_t)(seq_cur[t] % k->len) * TM_FINE * CW / span), 75, 105);   /* (0.41) the edit line */
        if (run && head < span) play_line(CX0 + (int)((uint64_t)head * CW / span) + 1, 72, 108);
    }
    b[0] = 0;
    if (!tm) {
        uint32_t len = k->len, bs = seq_bar(t);
        fmt_u(b, len); scat(b, " steps · "); fmt_u(b + slen(b), (len + bs - 1u) / bs); scat(b, (len + bs - 1u) / bs == 1u ? " bar · " : " bars · ");
        scat(b, METER[meter_of(t)].name);
        scat(b, " · EDIT "); fmt_u(b + slen(b), seq_cur[t] % len + 1u); scat(b, " (DATA)");
        vg_text(b, CX0, 186, VG_MED, DIM, VG_LEFT);
        return;
    }
    scat(b, "TIME · ");
    if (grow) scat(b, "RECORDING FREE – REC CLOSES THE LOOP");
    else if (!k->te) scat(b, "NO LOOP YET – REC RECORDS ONE");
    else {
        scat(b, "LOOP "); fmt_sec(b + slen(b), tm_frames(t, k->te - k->ts));
        tm_bbt(t, l, k->ts); if (l[0]) { scat(b, " · START ON "); scat(b, l); }
        tm_bbt(t, l, k->te); if (l[0]) { scat(b, " · END ON "); scat(b, l); }
        else if (!seq_shift) scat(b, " · SHIFT: NO SNAP");
    }
    vg_text(b, CX0, 186, VG_MED, grow ? RED : DIM, VG_LEFT);
}
static void track_view(void)                    /* the whole track as a time line (0.32: as TIME's – Sven: "ist viel besser") */
{
    if (au_is(sel)) { au_view(); return; }
    tm_view(sel);
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
