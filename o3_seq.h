/* SPDX-License-Identifier: GPL-3.0-only */
/* OberVave 3 – the sequencer as the panel sees it: recording, step editing, the views, the tracks' start / stop on the
 * bar, the 8 patterns. The logic is OberVave 2.0's (obervave.c), without the FM-1's layers and flash: all 8 patterns
 * live in memory and are saved ~1.5 s after a change, also while playing. Playing them is obv_seq.h's (the "ISR"). */
enum { REC_OFF, REC_READY, REC_ON };            /* REC: off, armed (the first note starts the loop), recording */
enum { SV_TRACKS, SV_STEPS, SV_MORE, SV_TRACK, SV_SLOTS, NSV };   /* STEP+ (SV_MORE) = SHIFT held in STEPS */
static const char *const SV_NAME[NSV] = {"TRACKS", "STEPS", "STEP+", "TRACK", "PATTERNS"};
#define SV_ENTRY(v) ((v) == SV_STEPS || (v) == SV_MORE)
static const char *const RQ_NAME[NRQ] = {"OFF", "1/32", "1/16", "1/8", "1/4"};
static const uint8_t RQ_PER_QUARTER[NRQ] = {0, 8, 4, 2, 1};

/* (3.7) the audio tracks' panel (o3_audio.h) */
static uint32_t au_arm;                         /* bit t: REC tapped while stopped – PLAY records these (0.29: several) */
static void au_rec_start(uint32_t t); static void au_rec_start_m(uint32_t m); static void au_arm_toggle(uint32_t t);
static void au_rec_stop(void);
static void au_rec_press(void); static int smp_page(void);
static void au_clear(uint32_t t);
static int au_knobs(knob_t *k);
static int au_turn(uint32_t knob, int32_t e);
static void au_queue(uint32_t n);               /* (3.8) the next pattern's audio for the ISR's swap */
static void au_pattern(void);                   /* (3.8) the pattern changed: its audio now */
static void au_pat_copied(uint32_t from, uint32_t to);
static uint32_t seq_view, seq_back = PG_PERFORM, seq_cur[NTRK], seq_pg[NTRK], rec, rec_wait, rec_from_arp;
static int seq_shift;                           /* SHIFT held: STEPS shows STEP+ */
#define SV_EFF (seq_view == SV_STEPS && seq_shift ? (uint32_t)SV_MORE : seq_view)
static struct { uint16_t step; uint32_t t; } rec_open[128];
static uint32_t rec_on_t[NTRK][SEQ_STEPS];      /* a note's start written into the step in this recording (o3_ms + 1) */
static void undo_save(void);
static void rec_begin(void) { rec = REC_ON; undo_save(); memset(rec_on_t, 0, sizeof rec_on_t); }
static pattern_t pats[NPAT];                    /* every pattern in memory */
/* (3.8, Sven: "Zurück als UNDO, Enc 1 for steps") UNDO with NUNDO steps back and as many forward (REDO): undo_st holds
 * the pattern before each edit, redo_st what an UNDO took back. RETURN held + ENCODER 1: back / forward a step each
 * detent; RETURN held 2 s: one back; SHIFT + F6: one back. A knob turned on the steps is one step, however long it turns. */
#define NUNDO 16u
static pattern_t undo_st[NUNDO], redo_st[NUNDO];
static uint32_t undo_n, redo_n, undo_last;
static uint64_t pats_dirty, pat_used;              /* bit n: pattern n to save / saved once (3.7: 64 of them) */
static uint32_t pat_edited, pat_queue, slot_sel, copy_arm, copy_arm_t, swap_seen;
static uint32_t pen = 60, pen_n, notes_down, pen_set;   /* (BOOM: pen_set – before any note, a DRUMS track's empty step gets the KICK) */
static uint8_t pen_chord[4];
static uint32_t pen_note(void) { return pen_set || trk_type(sel) != T_DRUMS ? pen : DRUM_NOTE[DR_KICK]; }
static int32_t step_held = -1;                  /* the step whose pad is held in STEPS, -1 none */
static uint32_t step_turned, step_midi;
static uint8_t audition[SEQ_CHORD], audition_part;
static uint32_t entry_n;
static uint8_t entry_on_note[128];

static int in_seq(void) { return page == PG_SEQ; }
/* (12.11.2026) the track's meter on the 16 pads: a page = whole bars of it. 4/4, a bar over 16 steps, or one that is no
 * whole number of steps (5/8 in quarters): pages of 16, as before */
static uint32_t bar_steps(uint32_t t)           /* track t's bar in its own steps; 0: not a whole number */
{
    uint32_t n = METER[meter_of(t)].e8 * DIV_PER_QUARTER[pat.trk[t % NTRK].div % NDIV];
    return n & 1u ? 0u : n / 2u;
}
static uint32_t seq_bar(uint32_t t) { uint32_t b = bar_steps(t); return !meter_of(t) || !b || b > SEQ_STEPS ? 16u : b; }   /* LENGTH's jump */
static uint32_t seq_pw(uint32_t t) { uint32_t b = bar_steps(t); return !meter_of(t) || !b || b > 16u ? 16u : 16u / b * b; }
static uint32_t beat_mark(uint32_t t, uint32_t i)   /* 2: a bar begins at step i, 1: a beat (4/4: every 4th, as before) */
{
    uint32_t b = bar_steps(t), dv = DIV_PER_QUARTER[pat.trk[t % NTRK].div % NDIV], bs;
    if (!meter_of(t)) return i % 4u == 0u;
    if (b && i % b == 0u) return 2u;
    bs = METER[meter_of(t)].den8 ? (dv & 1u ? 0u : dv / 2u) : dv;   /* (a beat's steps: a quarter's, or an eighth's) */
    return bs > 1u && b && i % b % bs == 0u;
}
static uint32_t seq_pages(uint32_t t) { uint32_t w = seq_pw(t); return pat.trk[t].len ? (pat.trk[t].len + w - 1u) / w : 1u; }
static void seq_cursor_set(uint32_t t, uint32_t n) { seq_cur[t] = n; seq_pg[t] = n / seq_pw(t); }
static void undo_push(pattern_t *st, uint32_t *n, const pattern_t *p)   /* (full: the oldest goes) */
{
    if (*n >= NUNDO) { memmove(st, st + 1, (NUNDO - 1u) * sizeof(pattern_t)); *n = NUNDO - 1u; }
    st[(*n)++] = *p;
}
static void undo_save(void) { undo_push(undo_st, &undo_n, &pat); redo_n = 0; undo_last = o3_ms; }
static void undo_mark(void) { if (o3_ms - undo_last > 800u || !undo_n) undo_save(); else undo_last = o3_ms; }   /* (a knob's turn: one step) */
static void edited(void) { pat_edited = 1; pats_dirty |= 1ull << set.pat; store_t = o3_ms; }
/* a knob turned while REC records (the sequencer playing, the track on): its value onto the step the track plays now, in
 * the lane of that parameter (a new one if there is none yet, NLANE per track). While it turns, the player leaves the
 * lane to the hand (ctl_auto_hold, let go 250 ms after the last turn – auto_poll) */
static uint32_t auto_hold_t[NTRK][NLANE];
static void say_trk(const char *what, uint32_t t);
static void auto_rec(uint32_t p)
{
    uint32_t t = sel % NTRK, l, f = NLANE, len = pat.trk[t].len;
    if (rec != REC_ON || !ctl_seq_run || !len || !(ctl_trk_on >> t & 1u) || is_a_only(p) || p >= NPARAM) return;
    for (l = 0; l < NLANE; l++) { if (pat.lane[t][l].param == p) break; if (f == NLANE && pat.lane[t][l].param == AUTO_NONE) f = l; }
    if (l == NLANE) {
        if (f == NLANE) { say_trk(": 4 KNOBS RECORDED ALREADY", t); return; }
        l = f; lane_clear(&pat.lane[t][l]); pat.lane[t][l].param = (uint8_t)p;
    }
    pat.lane[t][l].v[ui_seq_pos[t] % len] = live_p[t].v[p];
    ctl_auto_hold[t] |= 1u << l; auto_hold_t[t][l] = o3_ms;
    edited();
}
static void auto_poll(void)                     /* (main loop) a lane let go: the player has it again */
{
    for (uint32_t t = 0; t < NTRK; t++) for (uint32_t l = 0; l < NLANE; l++)
        if ((ctl_auto_hold[t] >> l & 1u) && o3_ms - auto_hold_t[t][l] > 250u) ctl_auto_hold[t] &= ~(1u << l);
}
static void say_trk(const char *what, uint32_t t)
{
    static char m[64]; m[0] = 0; scat(m, "TRACK ");   /* (64: 3.7's messages are longer – 32 ran over into seq_shift) */ { char l[2] = {(char)('A' + t % NTRK), 0}; scat(m, l); } scat(m, what); say(m);
}

/* ---- recording: where on track t the note was HEARD, on the track's REC grid (OFF: as played).
 * A key: the host says which frame was audible when it came (Protokoll 5) and the ISR's snapshot gives the place of
 * that frame – exact, whatever the buffer, the USB, the thread. A host without it (SDK 4, the browser today): now,
 * ~11 ms back. An arp note: where the ISR played it (rec_arp_q8). */
static uint32_t rec_arp_q8 = 0xFFFFFFFFu;
static uint32_t rec_where(uint32_t t, uint32_t *nudge)
{
    uint32_t len = pat.trk[t].len, dur = ui_seq_dur[t] ? ui_seq_dur[t] : 1u, back = 529u * 256u / dur, rq = set.rec_qt[t % NTRK] % NRQ;
    uint32_t q8 = ui_seq_pos[t] * 256u + (ui_seq_frac[t] > 256u ? 256u : ui_seq_frac[t]), g8;
    uint64_t heard;
    if (rec_from_arp) {
        rq = RQ_OFF;
        if (rec_arp_q8 != 0xFFFFFFFFu) q8 = rec_arp_q8 % (len * 256u);
    } else if (vibe_event_heard(&heard) && snap_q8(t, (uint32_t)heard, &q8)) ;
    else q8 = q8 >= back ? q8 - back : q8 + len * 256u - back;
    if (rq != RQ_OFF) {
        g8 = 256u * DIV_PER_QUARTER[pat.trk[t].div % NDIV] / RQ_PER_QUARTER[rq];
        if (!g8) g8 = 1;
        q8 = (q8 + g8 / 2u) / g8 * g8;
    }
    {                                           /* on a step's line (an arp note, or a key within ~1 ms): on it */
        uint32_t snapw = rec_from_arp ? 24u : 3u;
        if ((q8 & 255u) < snapw) q8 &= ~255u;
        else if ((q8 & 255u) > 255u - snapw) q8 = (q8 | 255u) + 1u;
    }
    q8 %= len * 256u;
    *nudge = (q8 & 255u) >> 1;
    return q8 >> 8;
}
static void step_add(step_t *st, uint32_t note, uint32_t vel)   /* a note into a step, the ones there stay */
{
    uint32_t k;
    if (!st->note[0]) { step_clear(st); st->gate = 96; }
    for (k = 0; k < SEQ_CHORD && st->note[k] && st->note[k] != note; k++) ;
    if (k < SEQ_CHORD && !st->note[k]) { st->note[k] = (uint8_t)note; st->nr[k] = 0; }
    if (k < SEQ_CHORD) st->nv[k] = (uint8_t)vel;   /* (BOOM 0.5: each note its own velocity) */
    st->vel = (uint8_t)vel;
    st->ratch &= (uint8_t)~STEP_OFF;
}
/* an arp take: from its first note while the keys (or HOLD) keep the arp going. When the loop comes round, what it
 * writes replaces what it wrote the round before (else every step got a second note – the arp goes on with its own
 * pattern – and the take turned into chords); the track's old steps are not played meanwhile (ctl_rec_mute) */
static uint32_t take_on, take_trk, take_pass, take_last;
static uint8_t take_mark[SEQ_STEPS];            /* the round (1..255) that wrote each step, 0 = not in this take */
static void take_step(uint32_t t, uint32_t s0, step_t *st)
{
    if (!take_on || take_trk != t) { take_on = 1; take_trk = t; take_pass = 1; take_last = s0; memset(take_mark, 0, sizeof take_mark); ctl_rec_mute = 1u << t; }
    if (s0 < take_last) take_pass = take_pass >= 255u ? 1u : take_pass + 1u;   /* (round again) */
    take_last = s0;
    if (take_mark[s0 % SEQ_STEPS] && take_mark[s0 % SEQ_STEPS] != take_pass) st->note[0] = 0;   /* (step_add: anew) */
    take_mark[s0 % SEQ_STEPS] = (uint8_t)take_pass;
}
static void take_check(void)                    /* the take is over: keys up and no HOLD, or REC off */
{
    if (take_on && (rec != REC_ON || (!notes_down && !(hold >> take_trk & 1u)) || !live_p[take_trk].v[P_ARP_MODE])) { take_on = 0; ctl_rec_mute = 0; }
}
static void trk_start_req(uint32_t t);
static int rec_track_ready(uint32_t t, uint32_t on);
static void rec_note(uint32_t on, uint32_t note, uint32_t vel)
{
    uint32_t t = sel, len, s0, n, k, held, sms, nudge = 0, sounding;
    note &= 127u; sounding = note;
    if (!rec_track_ready(t, on)) return;        /* (14.11.2026, Sven: a track that is not running records too – it starts) */
    len = pat.trk[t].len;
    if (!len) return;
    if (ctl_count || rec_wait) return;          /* counting in, waiting for the bar: notes only sound */
    if (on && rec == REC_READY && !ctl_seq_run) {   /* the first note: from the top */
        ctl_seq_restart = 1; ctl_seq_run = 1; rec_begin(); s0 = 0;
    } else if (rec != REC_ON) return;
    else if (on) s0 = ctl_seq_restart ? 0u : rec_where(t, &nudge);
    if (on) {
        step_t *st = seq_step(t, s0);
        uint32_t was;
        if (rec_from_arp) take_step(t, s0, st);
        was = st->note[0];
        note = (uint32_t)clampi((int32_t)note - pat.trk[t].transp, 1, 127);
        step_add(st, note, vel);
        if (!was) st->nudge = (uint8_t)nudge;
        TRACE('R', s0, was ? 999u : nudge);
        rec_open[note].step = (uint16_t)s0; rec_open[note].t = o3_ms + 1u;
        rec_on_t[t % NTRK][s0 % SEQ_STEPS] = o3_ms + 1u;
        if (!rec_from_arp) ctl_live_hold[t % NTRK][sounding >> 5] |= 1u << (sounding & 31u);   /* (the key owns it now) */
        edited();
        return;
    }
    note = (uint32_t)clampi((int32_t)note - pat.trk[t].transp, 1, 127);
    if (!rec_open[note].t) return;
    held = o3_ms + 1u - rec_open[note].t;
    s0 = rec_open[note].step;
    {
        rec_open[note].t = 0;
        sms = ui_seq_dur[t] ? ui_seq_dur[t] * 1000u / obv_fs : 125u;   /* a step in ms */
        if (!sms) sms = 1;
        n = (held + sms / 2u) / sms;
        if (n > len) n = len;
        /* legato (a pad's chord let go after the next one came; or over the loop's end): the note lasts up to the next
         * step where this recording started a note, not into it – else both piled up there (4 notes a step at most: notes fell out, others hung) */
        for (k = 1; k < n; k++)
            if (rec_on_t[t % NTRK][(s0 + k) % len % SEQ_STEPS]) {
                if (k == 1u) { seq_step(t, s0)->gate = 120; return; }
                n = k; break;
            }
    }
    if (n <= 1u) { seq_step(t, s0)->gate = (uint8_t)clampi((int32_t)(held * 127u / sms), 24, 120); return; }
    for (k = 0; k < n; k++) {                   /* tied on: the note in each step it lasts (with the played velocity) */
        step_t *st = seq_step(t, (s0 + k) % len);
        step_add(st, note, vel ? vel : seq_step(t, s0)->vel);
        st->gate = k + 1u < n ? GATE_TIE : 110;
    }
}
static int rec_keys(void) { return !live_p[sel % NTRK].v[P_ARP_MODE]; }   /* (feeding an arp: its notes are recorded) */

/* ---- STEPS: a pad picks its step (tapped: on / off), held + a later one: a long note; keys write into the cursor */
static void audition_off(void)
{
    for (uint32_t k = 0; k < SEQ_CHORD; k++) if (audition[k]) { note_event_p(audition_part, 0, audition[k], 0); audition[k] = 0; }
}
/* ---- (BOOM 0.5, Sven 14.11.: "alle Drum Sounds einzeln im Editor") the DRUM GRID – STEPS on a DRUMS track: a row a drum.
 * Encoder 2 picks the row (a key or a MIDI note of a drum picks it too); the 16 pads are THAT drum's steps (tapped: on /
 * off, as hard as hit); encoder 3 its velocity on the step, encoder 4 its ROLL (1 2 3 4 6 8 hits across the step). The
 * other drums of a step stay as they are. */
static uint32_t drum_row[NTRK];                 /* the chosen row of each track's grid */
static int drum_slot(const step_t *s, uint32_t d)   /* where drum d is in the step, -1: not there */
{
    for (uint32_t k = 0; k < SEQ_CHORD && s->note[k]; k++) if (drum_of_note(s->note[k]) == d) return (int)k;
    return -1;
}
static int drum_set(step_t *s, uint32_t d, uint32_t vel)   /* drum d into the step (there already: its velocity) */
{
    int k = drum_slot(s, d);
    if (k < 0) {
        if (!s->note[0]) { uint8_t ch = s->chance, rt = (uint8_t)(s->ratch & 3u); step_clear(s); s->chance = ch ? ch : 100; s->ratch = rt; }
        for (k = 0; k < (int)SEQ_CHORD && s->note[k]; k++) ;
        if (k >= (int)SEQ_CHORD) return -1;
        s->note[k] = DRUM_NOTE[d % NDRUM]; s->nr[k] = 0;
    }
    s->nv[k] = (uint8_t)clampi((int32_t)vel, 1, 127); s->ratch &= (uint8_t)~STEP_OFF;
    return k;
}
static void drum_unset(step_t *s, uint32_t d)
{
    int k = drum_slot(s, d);
    if (k < 0) return;
    for (uint32_t q = (uint32_t)k; q + 1u < SEQ_CHORD; q++) { s->note[q] = s->note[q + 1u]; s->nv[q] = s->nv[q + 1u]; s->nr[q] = s->nr[q + 1u]; }
    s->note[SEQ_CHORD - 1u] = s->nv[SEQ_CHORD - 1u] = s->nr[SEQ_CHORD - 1u] = 0;
}
static int drum_grid(void) { return trk_type(sel) == T_DRUMS && !au_is(sel); }
/* (BOOM 0.14, Sven: "einzelne Töne löschen … neue setzen durch Spielen an der Stelle") the PIANO ROLL: STEPS on a track
 * that is no kit shows twelve pitches by the bar's 16 steps, like the drum grid. Encoder 2, a key or a click picks the
 * row (the pitch); the 16 pads are THAT pitch's steps – a tap sets it, a tap on a note takes that one note away, the
 * step's other notes stay. A key played writes its note into the cursor's step (added to what is there) and the cursor
 * goes on. */
#define ROLL_ROWS 12u
static uint32_t roll_row[NTRK], roll_lo[NTRK];   /* the chosen pitch (as stored, before TRANSP) and the lowest row shown; 0: not set yet */
static int note_slot(const step_t *s, uint32_t note)
{
    for (uint32_t k = 0; k < SEQ_CHORD && s->note[k]; k++) if (s->note[k] == note) return (int)k;
    return -1;
}
static void note_unset(step_t *s, uint32_t note)
{
    int k = note_slot(s, note);
    if (k < 0) return;
    for (uint32_t q = (uint32_t)k; q + 1u < SEQ_CHORD; q++) { s->note[q] = s->note[q + 1u]; s->nv[q] = s->nv[q + 1u]; s->nr[q] = s->nr[q + 1u]; }
    s->note[SEQ_CHORD - 1u] = s->nv[SEQ_CHORD - 1u] = s->nr[SEQ_CHORD - 1u] = 0;
}
static void roll_show(uint32_t t)               /* a row is chosen, and it is in the window */
{
    t %= NTRK;
    if (!roll_row[t]) {
        roll_row[t] = 60;
        for (uint32_t i = 0; i < pat.trk[t].len; i++) if (seq_step(t, i)->note[0]) { roll_row[t] = seq_step(t, i)->note[0]; break; }
        roll_lo[t] = 0;
    }
    if (!roll_lo[t]) roll_lo[t] = roll_row[t] > 5u ? roll_row[t] - 5u : 1u;
    if (roll_row[t] < roll_lo[t]) roll_lo[t] = roll_row[t];
    if (roll_row[t] > roll_lo[t] + ROLL_ROWS - 1u) roll_lo[t] = roll_row[t] - (ROLL_ROWS - 1u);
    if (roll_lo[t] < 1u) roll_lo[t] = 1u;
    if (roll_lo[t] > 128u - ROLL_ROWS) roll_lo[t] = 128u - ROLL_ROWS;
}
/* (0.16, Sven: "die Töne nicht zeigen, die man nicht nutzt – so sieht man wenigstens alles") FOLD: the roll shows only
 * the pitches the track uses, and the chosen row; ENTER in STEPS switches it. The rows, the highest first: up to twelve
 * (more used than that: the twelve around the chosen one); few rows are taller. */
static uint32_t roll_fold = 1;
static uint32_t roll_rows(uint32_t t, uint8_t *rows, int *rh)
{
    uint32_t n = 0, used[4] = {0, 0, 0, 0}, row, at = 0, cnt = 0, first;
    t %= NTRK; roll_show(t); row = roll_row[t];
    if (!roll_fold) { for (n = 0; n < ROLL_ROWS; n++) rows[n] = (uint8_t)(roll_lo[t] + ROLL_ROWS - 1u - n); *rh = 9; return ROLL_ROWS; }
    for (uint32_t i = 0; i < pat.trk[t].len; i++) { const step_t *s = seq_step(t, i); for (uint32_t q = 0; q < SEQ_CHORD && s->note[q]; q++) used[s->note[q] >> 5 & 3u] |= 1u << (s->note[q] & 31u); }
    used[row >> 5 & 3u] |= 1u << (row & 31u);
    for (int p = 127; p > 0; p--) if (used[p >> 5] >> (p & 31) & 1u) { if ((uint32_t)p == row) at = cnt; cnt++; }
    first = cnt <= ROLL_ROWS ? 0u : at < ROLL_ROWS / 2u ? 0u : at - ROLL_ROWS / 2u;
    if (cnt > ROLL_ROWS && first > cnt - ROLL_ROWS) first = cnt - ROLL_ROWS;
    cnt = 0;
    for (int p = 127; p > 0 && n < ROLL_ROWS; p--) if (used[p >> 5] >> (p & 31) & 1u) { if (cnt++ >= first) rows[n++] = (uint8_t)p; }
    *rh = n ? clampi(108 / (int)n, 9, 24) : 9;   /* (few rows fill the field) */
    return n;
}
static void roll_pick(uint32_t t, uint32_t note) { t %= NTRK; roll_row[t] = (uint32_t)clampi((int32_t)note, 1, 127); roll_show(t); }
static void drum_hear(uint32_t t, uint32_t d, uint32_t vel) { note_event_p(t, 1, DRUM_NOTE[d % NDRUM], vel); note_event_p(t, 0, DRUM_NOTE[d % NDRUM], 0); }
static void step_pad_down(uint32_t pad, uint32_t vel)   /* vel: how hard the pad was hit (a new step gets it) */
{
    static char msg[24];
    uint32_t t = sel, len = pat.trk[t].len, n = seq_pg[t] * seq_pw(t) + pad, i;
    if (pad >= seq_pw(t)) return;               /* (past the page's bars: dark) */
    if (n >= len) { say(len ? "PAST THE TRACK'S LENGTH" : "TRACK OFF: TURN LENGTH"); return; }
    if (step_held >= 0 && n > (uint32_t)step_held) {   /* hold one step, tap a later one: a long note to there */
        step_t *a = seq_step(t, (uint32_t)step_held);
        undo_save();
        if (!a->note[0]) { step_clear(a); a->note[0] = (uint8_t)pen_note(); }
        a->ratch &= (uint8_t)~STEP_OFF;
        for (uint32_t m = (uint32_t)step_held; m <= n; m++) {
            step_t *st = seq_step(t, m);
            if (m != (uint32_t)step_held) { uint8_t r = (uint8_t)(a->ratch & 3u); *st = *a; st->ratch = r; }
            st->gate = m < n ? GATE_TIE : 110;
        }
        step_turned = 1; edited();
        msg[0] = 0; scat(msg, "LONG NOTE "); fmt_u(msg + slen(msg), (uint32_t)step_held + 1u); scat(msg, "-"); fmt_u(msg + slen(msg), n + 1u);
        say(msg);
        return;
    }
    if (drum_grid()) {                          /* the grid: this drum on this step – set at once when it is not there */
        step_t *st = seq_step(t, n);
        step_held = (int32_t)n; step_turned = step_midi = 0; seq_cur[t] = n;
        if (drum_slot(st, drum_row[t]) < 0 || (st->ratch & STEP_OFF)) { undo_save(); drum_set(st, drum_row[t], vel); step_turned = 1; edited(); }
        if (!ctl_seq_run) drum_hear(t, drum_row[t], vel);
        return;
    }
    step_held = (int32_t)n; step_turned = step_midi = 0;
    seq_cur[t] = n;
    roll_show(t);
    {                                           /* (0.14) the piano roll: the row's pitch on this step – set at once when it is not there */
        step_t *st = seq_step(t, n);
        if (note_slot(st, roll_row[t]) < 0 || st->ratch & STEP_OFF) {
            undo_save();
            if (st->ratch & STEP_OFF) step_clear(st);
            step_add(st, roll_row[t], (uint32_t)clampi((int32_t)vel, 1, 127));   /* (as hard as the pad was hit) */
            step_turned = 1; edited();
        }
    }
    if (!ctl_seq_run) {                         /* stopped: you hear the note you hold */
        audition_off(); audition_part = (uint8_t)(t % NTRK);
        audition[0] = (uint8_t)clampi((int32_t)roll_row[t] + pat.trk[t].transp, 1, 127);
        note_event_p(audition_part, 1, audition[0], vel ? vel : 100u);
    }
    (void)i;
}
static void step_pad_up(uint32_t pad)
{
    uint32_t t = sel, n = seq_pg[t] * seq_pw(t) + pad;
    if (pad >= seq_pw(t)) return;
    if (step_held != (int32_t)n) return;
    if (!step_turned && !step_midi) { undo_save(); if (drum_grid()) drum_unset(seq_step(t, n), drum_row[t]); else note_unset(seq_step(t, n), roll_row[t % NTRK]); edited(); }   /* tapped, not edited: off (the grid: that drum · the roll: that note) */
    step_held = -1;
    audition_off();
}
static void pen_add(uint32_t note)              /* the last note or chord played: what an empty step gets */
{
    if (!notes_down) pen_n = 0;
    notes_down++;
    if (pen_n < 4u) pen_chord[pen_n++] = (uint8_t)note;
    pen = note; pen_set = 1;
}
static int entry_active(void) { return in_seq() && SV_ENTRY(seq_view) && !rec && step_held < 0 && pat.trk[sel].len && !drum_grid(); }   /* (the grid: a key picks its row) */
static void entry_reset(void) { entry_n = 0; memset(entry_on_note, 0, sizeof entry_on_note); }
static void entry_note(uint32_t on, uint32_t note, uint32_t vel)
{
    uint32_t t = sel, len = pat.trk[t].len, k;
    step_t *st;
    note &= 127u;
    if (!on) {
        if (!entry_on_note[note]) return;
        entry_on_note[note] = 0;
        if (entry_n && !--entry_n && len) seq_cursor_set(t, (seq_cur[t] + 1u) % len);   /* all let go: the next step */
        return;
    }
    if (!entry_active() || entry_on_note[note]) return;
    if (seq_cur[t] >= len) seq_cursor_set(t, 0);
    st = seq_step(t, seq_cur[t]);
    if (!entry_n) {                             /* the first key (0.14: the step's notes stay – a key ADDS its note; before, it replaced them) */
        undo_save();
        if (!st->note[0] || st->ratch & STEP_OFF) step_clear(st);
        st->ratch &= (uint8_t)~STEP_OFF;
    }
    k = (uint32_t)clampi((int32_t)note - pat.trk[t].transp, 1, 127);
    step_add(st, k, vel ? vel : 100u);
    roll_pick(t, k);                            /* (the roll's row follows the key) */
    entry_on_note[note] = 1; entry_n++;
    edited();
}

/* a note from the keys, the pads or MIDI on the selected track: it sounds, REC takes it, the sequencer's views use it */
static void note_in(uint32_t on, uint32_t note, uint32_t vel)
{
    if (au_is(sel)) return;                     /* (3.7: an audio track plays no notes) */
    play_note(on, note, vel);
    if (!on) for (uint32_t t = 0; t < NTRK; t++) ctl_live_hold[t][note >> 5 & 3u] &= ~(1u << (note & 31u));   /* (let go: the sequencer's again) */
    if (rec_keys()) rec_note(on, note, vel);
    if (on) {
        pen_add(note);
        if (drum_grid() && in_seq() && SV_ENTRY(seq_view)) {   /* (BOOM 0.5) the grid: the drum played is the row; a held step gets it */
            drum_row[sel] = drum_of_note(note);
            if (step_held >= 0 && !rec) { undo_mark(); drum_set(seq_step(sel, (uint32_t)step_held), drum_row[sel], vel); step_turned = 1; edited(); }
            else if (!rec && pat.trk[sel].len && !entry_on_note[note & 127u]) {   /* (0.15, Sven) no step held: the drum played is written at the cursor, as a key on the piano roll – all let go: the next step (entry_note) */
                if (!entry_n) undo_save();
                if (seq_cur[sel] >= pat.trk[sel].len) seq_cursor_set(sel, 0);
                drum_set(seq_step(sel, seq_cur[sel]), drum_row[sel], vel ? vel : 100u);
                entry_on_note[note & 127u] = 1; entry_n++; edited();
            }
        }
        else if (step_held < 0) entry_note(1, note, vel);
        else if (!rec) {                        /* a held step takes what is played: the first note starts the chord */
            step_t *sp = seq_step(sel, (uint32_t)step_held);
            uint32_t q;
            if (!step_midi++) { for (q = 0; q < SEQ_CHORD; q++) sp->note[q] = 0; sp->ratch &= (uint8_t)~STEP_OFF; }
            for (q = 0; q < SEQ_CHORD && sp->note[q] && sp->note[q] != note; q++) ;
            if (q < SEQ_CHORD && !sp->note[q]) { sp->note[q] = (uint8_t)note; sp->vel = (uint8_t)vel; }
            edited();
        }
    } else { if (notes_down) notes_down--; entry_note(0, note, 0); }
}

/* ---- each pattern's instruments and mixer (Sven 20.10./21.10.: a pattern keeps its sounds, effects and every mixer
 * setting): the four tracks' sounds with their edits and program numbers, the mixer (level, pan, the four sends, mute,
 * solo), the effects (chorus, delay, reverb, drive) and MASTER – in "psnd0".."psnd7" (2.0's files untouched). A pattern
 * without its own set takes what is playing. */
#define PSND_MAGIC 0x444E5350u                  /* "PSND": the first sets (sounds only, 20.10.) */
#define PSN2_MAGIC (0x4D000000u | NTRK << 16 | PATCH_BYTES)  /* (BOOM) "CM" + the patch's size: a set saved with another layout is passed over, never misread
                                                   (0.2: "BM" sets could hold a MASTER at 0 from the misread files – passed over too) */
typedef struct {
    uint8_t lvl[NTRK], cho[NTRK], dly[NTRK], rev[NTRK], drv[NTRK]; int8_t pan[NTRK];
    uint8_t mute, solo, fx[NFXP], dv_type, dv_amt, dv_tone, dv_mix, m_vol, m_dj, m_drive, m_comp;
    uint8_t tempo;                              /* (3.2) the pattern's tempo, 40..240; 0 (an older set): the one playing stays */
    uint8_t mfx[NMFX], mfx_ok;                  /* (BOOM) MASTER FX: FILTER, CRUSH, ECHO (REPEAT stays OFF); ok: MFX_OK */
} mixset_t;
typedef struct { uint32_t magic; uint8_t prog[NTRK], edited[NTRK]; patch_t p[NTRK]; mixset_t mx; } psnd_t;
static psnd_t psnd[NPAT];
static uint64_t snd_dirty;                      /* bit n: psnd[n] to save */
static int bytes_same(const void *a, const void *b, uint32_t n) { const uint8_t *x = a, *y = b; while (n--) if (*x++ != *y++) return 0; return 1; }
static void mx_from_set(mixset_t *m)
{
    for (uint32_t t = 0; t < NTRK; t++) { m->lvl[t] = set.mix_lvl[t]; m->cho[t] = set.mix_cho[t]; m->dly[t] = set.mix_dly[t]; m->rev[t] = set.mix_rev[t]; m->drv[t] = set.mix_drv[t]; m->pan[t] = set.mix_pan[t]; }
    m->mute = set.mix_mute; m->solo = set.mix_solo;
    for (uint32_t i = 0; i < NFXP; i++) m->fx[i] = set.fx[i];
    m->dv_type = set.dv_type; m->dv_amt = set.dv_amt; m->dv_tone = set.dv_tone; m->dv_mix = set.dv_mix;
    m->m_vol = set.m_vol; m->m_dj = set.m_dj; m->m_drive = set.m_drive; m->m_comp = set.m_comp;
    m->tempo = set.tempo;
    for (uint32_t i = 0; i < NMFX; i++) m->mfx[i] = set.mfx[i]; m->mfx[3] = 0; m->mfx_ok = MFX_OK;
}
#include "bm_save.h"
static void mx_to_set(const mixset_t *m)        /* into the settings and the engine */
{
    for (uint32_t t = 0; t < NTRK; t++) { set.mix_lvl[t] = m->lvl[t]; set.mix_cho[t] = m->cho[t]; set.mix_dly[t] = m->dly[t]; set.mix_rev[t] = m->rev[t]; set.mix_drv[t] = m->drv[t]; set.mix_pan[t] = m->pan[t]; }
    set.mix_mute = m->mute; set.mix_solo = m->solo;
    for (uint32_t i = 0; i < NFXP; i++) set.fx[i] = m->fx[i];
    set.fx_ok = FX_OK;
    set.dv_type = m->dv_type; set.dv_amt = m->dv_amt; set.dv_tone = m->dv_tone; set.dv_mix = m->dv_mix;
    set.m_vol = m->m_vol; set.m_dj = m->m_dj; set.m_drive = m->m_drive; set.m_comp = m->m_comp;
    if (m->tempo >= 40u && m->tempo <= 240u) set.tempo = m->tempo;   /* (3.2) each pattern its tempo */
    if (m->mfx_ok == MFX_OK) for (uint32_t i = 0; i < 3u; i++) set.mfx[i] = m->mfx[i];
    mix_apply();
}
static void mx_prepare(uint32_t n)              /* pattern n's mixer, drive, master as the ISR takes them (mx_q) */
{
    const psnd_t *s = &psnd[n % NPAT];
    uint32_t t, solo, mute;
    if (s->magic != PSN2_MAGIC) {               /* (no own set: the mix stays as it is) */
        for (t = 0; t < NTRK; t++) { mx_q.lvl[t] = ctl_mix_lvl[t]; mx_q.cho[t] = ctl_mix_cho[t]; mx_q.dly[t] = ctl_mix_dly[t]; mx_q.rev[t] = ctl_mix_rev[t]; mx_q.drv[t] = ctl_mix_drv[t]; mx_q.pan[t] = ctl_mix_pan[t]; }
        mx_q.off = ctl_mix_off; mx_q.dv_type = ctl_dv_type; mx_q.dv_amt = ctl_dv_amt; mx_q.dv_tone = ctl_dv_tone; mx_q.dv_mix = ctl_dv_mix;
        mx_q.m_vol = ctl_m_vol; mx_q.m_dj = ctl_m_dj; mx_q.m_drive = ctl_m_drive; mx_q.m_comp = ctl_m_comp;
        mx_q.tempo = 0;
        for (t = 0; t < NMFX; t++) mx_q.mfx[t] = ctl_mfx[t];
        return;
    }
    for (t = 0; t < NTRK; t++) {
        mx_q.lvl[t] = s->mx.lvl[t] > 127u ? 127u : s->mx.lvl[t]; mx_q.pan[t] = (int8_t)clampi(s->mx.pan[t], -64, 64);
        mx_q.cho[t] = s->mx.cho[t] > 127u ? 127u : s->mx.cho[t]; mx_q.dly[t] = s->mx.dly[t] > 127u ? 127u : s->mx.dly[t];
        mx_q.rev[t] = s->mx.rev[t] > 127u ? 127u : s->mx.rev[t]; mx_q.drv[t] = s->mx.drv[t] > 127u ? 127u : s->mx.drv[t];
    }
    solo = s->mx.solo & ((1u << NTRK) - 1u); mute = s->mx.mute & ((1u << NTRK) - 1u);
    mx_q.off = solo ? ~solo & ((1u << NTRK) - 1u) : mute;
    mx_q.dv_type = s->mx.dv_type; mx_q.dv_amt = s->mx.dv_amt; mx_q.dv_tone = s->mx.dv_tone; mx_q.dv_mix = s->mx.dv_mix;
    mx_q.m_vol = s->mx.m_vol; mx_q.m_dj = s->mx.m_dj; mx_q.m_drive = s->mx.m_drive; mx_q.m_comp = s->mx.m_comp;
    mx_q.tempo = s->mx.tempo >= 40u && s->mx.tempo <= 240u ? s->mx.tempo : 0;
    for (t = 0; t < NMFX; t++) mx_q.mfx[t] = s->mx.mfx_ok == MFX_OK ? s->mx.mfx[t] : ctl_mfx[t];
    mx_q.mfx[3] = ctl_mfx[3];
}
static void snd_capture(uint32_t n)             /* what plays now into pattern n's set */
{
    psnd_t *s = &psnd[n % NPAT];
    s->magic = PSN2_MAGIC;
    for (uint32_t t = 0; t < NTRK; t++) { s->p[t] = live_p[t]; s->prog[t] = (uint8_t)trk_prog[t]; s->edited[t] = (uint8_t)trk_edited[t]; }
    mx_from_set(&s->mx);
    snd_dirty |= 1ull << (n % NPAT); store_t = o3_ms;
}
static void snd_target(uint32_t n, patch_t *out)   /* pattern n's four sounds (its effects in track A's, where the engine reads them) */
{
    const psnd_t *s = &psnd[n % NPAT];
    for (uint32_t t = 0; t < NTRK; t++) out[t] = s->magic == PSN2_MAGIC ? s->p[t] : live_p[t];
    for (uint32_t i = 0; i < NFXP; i++) out[0].v[P_CHO_RATE + i] = s->magic == PSN2_MAGIC ? s->mx.fx[i] : set.fx[i];
}
static void snd_meta(uint32_t n)                /* pattern n's program numbers and edit marks, its mixer, effects, master */
{
    const psnd_t *s = &psnd[n % NPAT];
    if (s->magic != PSN2_MAGIC) { snd_capture(n); return; }
    for (uint32_t t = 0; t < NTRK; t++) { trk_prog[t] = s->prog[t] % NPROG; trk_edited[t] = s->edited[t]; set.tprog[t] = (uint8_t)trk_prog[t]; }
    set.prog = set.tprog[0];
    mx_to_set(&s->mx);
}

/* ---- the patterns: all in memory; playing, the next one comes on the bar (the ISR swaps pat_q in) */
static void pat_switch(uint32_t n)              /* stopped: at once */
{
    static char msg[20];
    n %= NPAT;
    if (n != set.pat) {
        patch_t nx[NTRK];
        pats[set.pat] = pat; ctl_panic = 1;
        snd_capture(set.pat); snd_target(n, nx);   /* (its instruments go with it, the new one's come) */
        for (uint32_t t = 0; t < NTRK; t++) live_p[t] = nx[t];
        pat = pats[n]; set.pat = (uint8_t)n; snd_meta(n);
        au_pattern();                           /* (3.8: its audio tracks) */
        undo_n = redo_n = 0;                    /* (3.8: UNDO is the pattern's – another one's steps never come back here) */
        memset(seq_cur, 0, sizeof seq_cur); memset(seq_pg, 0, sizeof seq_pg); pat_edited = 0; set_touch();
    }
    slot_sel = n;
    msg[0] = 0; scat(msg, "PATTERN "); fmt_u(msg + slen(msg), n + 1u);
    if (!(pat_used >> n & 1u)) scat(msg, " (NEW)");
    say(msg);
}
static uint32_t pat_requeue;                    /* n + 1: asked for while the ISR was swapping the last one in */
static void pat_go(uint32_t n)
{
    static char msg[20];
    n %= NPAT;
    if (pat_queue) {                            /* another one was next: take it back – unless the ISR is swapping it in */
        uint32_t one = 1u;
        if (!__atomic_compare_exchange_n(&ctl_pat_swap, &one, 0u, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) { pat_requeue = n + 1u; return; }
        ctl_snd_swap = 0; ctl_mx_swap = 0; ctl_au_swap = 0; pat_queue = 0;
    }
    if (!ctl_seq_run) { pat_switch(n); return; }
    if (n == set.pat) { say("STAYS"); return; }
    ctl_pat_keep = 1;                           /* (the one left comes back in pat_old, with its edits) */
    pats[set.pat] = pat;
    pat_q = pats[n];
    snd_target(n, snd_q); ctl_snd_swap = 1;     /* (its instruments, swapped in on the same downbeat) */
    mx_prepare(n); ctl_mx_swap = 1;             /* (and its mix) */
    au_queue(n);                                /* (3.8: and its audio) */
    pat_queue = n + 1u;
    RING_PUBLISH();                             /* (all of it before the flag: the ISR may be on another core) */
    ctl_pat_swap = 1;
    msg[0] = 0; scat(msg, "PATTERN "); fmt_u(msg + slen(msg), n + 1u); scat(msg, " NEXT");
    say(msg);
}
static void pat_copy(uint32_t n)                /* this pattern into slot n (a used one: turn again) */
{
    static char msg[24];
    n %= NPAT;
    if (n == set.pat) { say("THAT IS THIS ONE"); return; }
    if (pat_used >> n & 1u && (copy_arm != n + 1u || o3_ms - copy_arm_t > 3000u)) {
        copy_arm = n + 1u; copy_arm_t = o3_ms;
        msg[0] = 0; scat(msg, "AGAIN: OVERWRITE "); fmt_u(msg + slen(msg), n + 1u); say(msg);
        return;
    }
    copy_arm = 0;
    pats[n] = pat; pat_used |= 1ull << n; pats_dirty |= 1ull << n; store_t = o3_ms;
    snd_capture(n);                             /* (with this pattern's instruments) */
    au_pat_copied(set.pat, n);                  /* (3.8: and its audio – the takes shared until one records) */
    msg[0] = 0; scat(msg, "COPIED TO "); fmt_u(msg + slen(msg), n + 1u); say(msg);
}
static void seq_clear_cursor(void)              /* EDIT in STEPS: the cursor's step empty, the cursor on */
{
    uint32_t t = sel, len = pat.trk[t].len;
    if (!len) return;
    undo_save();
    {
        step_t *cs = seq_step(t, seq_cur[t] % len);
        int one = !drum_grid() && note_slot(cs, roll_row[t % NTRK]) >= 0;   /* (0.14) the roll: the row's note there – only that one goes */
        if (drum_grid()) drum_unset(cs, drum_row[t]); else if (one) note_unset(cs, roll_row[t % NTRK]); else step_clear(cs);
        seq_cursor_set(t, (seq_cur[t] + 1u) % len);
        edited(); say(drum_grid() ? "DRUM CLEARED ON THE STEP" : one ? "NOTE CLEARED" : "STEP CLEARED");
    }
}
static void undo_say(const char *what)
{
    static char m[40]; m[0] = 0; scat(m, what); scat(m, " · "); fmt_u(m + slen(m), undo_n); scat(m, " BACK · ");
    fmt_u(m + slen(m), redo_n); scat(m, " FORWARD"); say(m);
}
static int seq_undo1(void) { if (!undo_n) return 0; undo_push(redo_st, &redo_n, &pat); pat = undo_st[--undo_n]; edited(); undo_last = 0; return 1; }
static int seq_redo1(void) { if (!redo_n) return 0; undo_push(undo_st, &undo_n, &pat); pat = redo_st[--redo_n]; edited(); undo_last = 0; return 1; }
static void seq_undo(void) { if (seq_undo1()) undo_say("UNDO"); else say("NOTHING TO UNDO"); }   /* SHIFT + F6, RETURN held 2 s */
static void undo_turn(int32_t s)                /* RETURN held + ENCODER 1: left back, right forward, a step a detent */
{
    int any = 0;
    for (; s < 0; s++) any |= seq_undo1();
    for (; s > 0; s--) any |= seq_redo1();
    if (any) undo_say("UNDO"); else say(undo_n || redo_n ? "NO MORE THAT WAY" : "NOTHING TO UNDO");
}

/* ---- the tracks: start / stop on the bar, record, choose, clear (2.0's) */
static uint32_t trk_state(uint32_t t)           /* 0 stopped, 1 playing, 2 starts on the bar, 3 stops on the bar */
{
    uint32_t on = ctl_trk_on >> t & 1u, pend = trk_pending() >> t & 1u;
    return pend ? (on ? 3u : 2u) : on;
}
static void trk_select(uint32_t t)
{
    t %= NTRK;
    if (t != sel) { sel = t; step_held = -1; audition_off(); entry_reset(); }
    if (au_is(t)) { say_trk(": AUDIO", t); return; }
    say_trk(pat.trk[t].len ? "" : in_seq() && seq_view == SV_TRACKS ? " OFF: KNOB 3 = ON" : " OFF: KNOB 1 = ON", t);
}
static void trk_launch(uint32_t t)              /* start / stop on the next bar (stopped: it starts, and the transport) */
{
    uint32_t will;
    t %= NTRK;
    if (!ctl_seq_run) {
        if (!((ctl_trk_on ^ trk_pending()) >> t & 1u)) ctl_trk_req ^= 1u << t;
        ctl_seq_restart = 1; ctl_seq_run = 1;
        say_trk(" PLAYS", t);
        return;
    }
    ctl_trk_req ^= 1u << t;
    will = (ctl_trk_on ^ trk_pending()) >> t & 1u;
    say_trk(trk_pending() >> t & 1u ? (will ? " STARTS ON THE BAR" : " STOPS ON THE BAR") : (will ? " PLAYS" : " STOPPED"), t);
}
static void trk_record(uint32_t t)              /* REC lit + a pad of row 1: that track records (session record) */
{
    uint32_t will;
    t %= NTRK;
    if (au_is(t)) {                             /* (3.7) an audio track: its take starts / stops */
        trk_select(t); rec = REC_OFF;
        if (ctl_au_rq) au_rec_stop(); else au_rec_start(t);
        return;
    }
    if (rec == REC_ON && sel == t) {
        say_trk(rec_wait ? " NOT RECORDED" : " RECORDED", t);
        rec = REC_READY; rec_wait = 0; memset(rec_open, 0, sizeof rec_open);
        return;
    }
    memset(rec_open, 0, sizeof rec_open);
    rec_wait = 0;
    if (!pat.trk[t].len) { pat.trk[t].len = (uint16_t)seq_bar(t); edited(); }
    trk_select(t);
    will = (ctl_trk_on ^ trk_pending()) >> t & 1u;
    if (!will) ctl_trk_req ^= 1u << t;
    if (!ctl_seq_run) {
        rec_begin(); ctl_count = set.click % NCLICK == CLICK_COUNT;
        ctl_seq_restart = 1; ctl_seq_run = 1;
        say_trk(" RECORDING", t);
    } else if (trk_pending() >> t & 1u) { rec_begin(); rec_wait = t + 1u; say_trk(" RECORDS ON THE BAR", t); }
    else { rec_begin(); say_trk(" RECORDING", t); }
}
/* (14.11.2026, Sven: "wenn eine Spur aus ist … soll man dennoch aufnehmen können") REC on a track that is not running:
 * stopped (REC armed, the first note starts the loop) – it starts with the transport; playing – it starts on the next bar
 * and records from there (as REC + its pad). A track switched OFF (LENGTH 0) gets a bar of its meter first. */
static void trk_start_req(uint32_t t)
{
    if (!((ctl_trk_on ^ trk_pending()) >> t & 1u)) ctl_trk_req ^= 1u << t;   /* (not running and not about to: start it) */
}
static int rec_track_ready(uint32_t t, uint32_t on)
{
    if (!on || !rec) return 1;
    if (!pat.trk[t].len) { pat.trk[t].len = (uint16_t)seq_bar(t); edited(); }
    if (!ctl_seq_run) { if (rec == REC_READY) trk_start_req(t); return 1; }
    if (rec != REC_ON || ctl_trk_on >> t & 1u) return 1;
    if (!(trk_pending() >> t & 1u)) { trk_start_req(t); rec_wait = t + 1u; say_trk(" RECORDS ON THE BAR", t); }
    else if (!rec_wait) rec_wait = t + 1u;
    return 0;                                   /* (this note sounds; recording begins with the track, on the bar) */
}
static void trk_clear(void)                     /* the chosen track emptied (REC held 1.2 s, or SHIFT + REC); one undo */
{
    if (au_is(sel)) { au_clear(sel); return; }
    undo_save();
    for (uint32_t n = 0; n < pat.trk[sel].len; n++) step_clear(seq_step(sel, n));
    for (uint32_t l = 0; l < NLANE; l++) lane_clear(&pat.lane[sel % NTRK][l]);   /* (its recorded knobs too) */
    edited();
    if (rec == REC_ON) { rec = ctl_seq_run ? REC_READY : REC_OFF; rec_wait = 0; }
    memset(rec_open, 0, sizeof rec_open);
    say_trk(" CLEARED", sel);
}
static uint32_t rec_down, rec_down_t, rec_armed_pads, rec_hint;   /* (0.29) rec_armed_pads: pads were hit while REC was held – it armed */
static void perf_pad_at(uint32_t pad, uint32_t base, int shift_held)   /* row 1: start / stop (REC lit: record; SHIFT: always start/stop), row 2: the track */
{
    uint32_t t = base + (pad & 3u);
    pad &= 7u;
    if (t >= NTRK) return;                      /* (bank E-F: columns 3-4 are dark) */
    if (pad < 4u) { if (rec && !shift_held) trk_record(t); else trk_launch(t); }
    else trk_select(t);
}
static void perf_pad(uint32_t pad, int shift_held) { perf_pad_at(pad, bank0(), shift_held); }

/* ---- the transport: PLAY, STOP, REC (2.0's) */
static void play_press(void)
{
    if (ctl_seq_run) { ctl_seq_run = 0; rec = REC_OFF; rec_wait = 0; au_rec_stop(); }
    else if (au_arm) { uint32_t m = au_arm; au_arm = 0; au_rec_start_m(m); return; }   /* (3.7: the armed audio tracks: from bar 1) */
    else {
        if (rec == REC_READY) { rec_begin(); ctl_count = set.click % NCLICK == CLICK_COUNT; }
        ctl_seq_restart = 1; ctl_seq_run = 1;
    }
    if (set.midi_out || midi_trk_any()) vibe_transport_out(ctl_seq_run);   /* (3.1) START / STOP out, the clock lined up (0.30: a MIDI track too) */
}
static void rec_press(int down, int shift_held)
{
    if (down && shift_held) { trk_clear(); return; }
    if (down) { rec_down = 1; rec_down_t = o3_ms; rec_armed_pads = 0; return; }
    if (!rec_down) return;
    rec_down = 0;
    if (rec_armed_pads) { rec_armed_pads = 0; return; }   /* (0.29) REC was held to arm: nothing else */
    if (o3_ms - rec_down_t > 1200u) { trk_clear(); return; }   /* (0.29) REC held and let go: the track cleared – on letting go,
                                                 * so a hand that holds REC to arm (and is slow) never clears a take */
    if (o3_ms - rec_down_t < 500u) {            /* a press: playing, record now / stop recording; stopped, arm */
        if ((au_is(sel) || au_armed() || (trk_type(sel) == T_SMP && smp_page()) || ctl_au_rq || au_arm) && !rec) { au_rec_press(); return; }   /* (0.29: armed tracks – REC records them, whichever is chosen) */   /* (0.26, Sven: the sample was gone after REC in the sequencer) a SAMPLER records audio only on its EDIT / CHOP page – elsewhere REC records its notes */   /* (0.20: a SAMPLER records its sample) */   /* (3.7: an audio take – a running one stops, whichever track is chosen) */
        if (rec) rec = REC_OFF;
        else if (ctl_seq_run) { rec_begin(); if (!(ctl_trk_on >> sel & 1u)) rec_track_ready(sel, 1); }
        else rec = REC_READY;
        memset(rec_open, 0, sizeof rec_open);
    }
}

/* ---- the views' knobs (2.0's seq_view_knob / seq_view_turn) */
static void fmt_note(char *s, uint32_t n)
{
    static const char *const NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    s[0] = 0; scat(s, NN[n % 12u]); if (n < 12u) scat(s, "-");
    fmt_u(s + slen(s), n < 12u ? 1u : n / 12u - 1u);
}
static void seq_knobs(knob_t *k)
{
    static const char *const L[NSV][4] = {{"CUTOFF", "LEVEL", "LENGTH", "QUANT"}, {"STEP", "NOTE", "GATE", "VELO"},
                                          {"STEP", "RATCHET", "CHANCE", "ON/OFF"}, {"LENGTH", "DIV", "SWING", "TRANSP"},
                                          {"SLOT", "", "GO", "COPY"}};
    uint32_t t = sel, len = pat.trk[t].len, n;
    seqtrk_t *tk = &pat.trk[t];
    const step_t *st = seq_step(t, len ? seq_cur[t] % len : 0u);
    if (au_knobs(k)) return;                    /* (3.7: TRACK+, an audio track) */
    for (uint32_t i = 0; i < 4u; i++) {
        char *v = k[i].value;
        v[0] = 0; k[i].v = 0; k[i].bip = 0;
        k[i].label = L[SV_EFF][i][0] ? L[SV_EFF][i] : 0;
        if (SV_EFF == SV_STEPS && i == 1u && trk_type(t) == T_DRUMS) k[i].label = "DRUM";   /* (BOOM) */
        if (!k[i].label) continue;
        if (seq_view == SV_SLOTS) {
            if (i == 0) { fmt_u(v, slot_sel + 1u); k[i].v = slot_sel / (float)(NPAT - 1); }
            else if (i == 2u) scat(v, slot_sel == set.pat ? "HERE" : "TURN");
            else scat(v, copy_arm == slot_sel + 1u && o3_ms - copy_arm_t <= 3000u ? "AGAIN" : "TURN");
            continue;
        }
        if (seq_view == SV_TRACKS) {                /* (CUTOFF on knob 1, LENGTH on 3: Sven's order) */
            uint32_t j = i == 0u ? 2u : i == 2u ? 0u : i;
            if (j == 0) { if (tk->len) fmt_u(v, tk->len); else scat(v, "OFF"); k[i].v = tk->len / (float)SEQ_STEPS; }
            else if (j == 1) { fmt_u(v, set.mix_lvl[t]); k[i].v = set.mix_lvl[t] / 127.0f; }
            else if (j == 2) {                  /* (without "Hz": the column is narrow, Sven 21.10.) */
                param_value(v, P_CUTOFF, live_p[t].v[P_CUTOFF]); k[i].v = live_p[t].v[P_CUTOFF] / 127.0f;
                { uint32_t l = slen(v); if (l > 2u && v[l - 2u] == 'H' && v[l - 1u] == 'z') v[l - 2u] = 0; }
            }
            else { scat(v, RQ_NAME[set.rec_qt[t] % NRQ]); k[i].v = (float)(set.rec_qt[t] % NRQ) / (NRQ - 1); }
            continue;
        }
        if (seq_view == SV_TRACK) {
            if (i == 0) { if (tk->len) fmt_u(v, tk->len); else scat(v, "OFF"); k[i].v = tk->len / (float)SEQ_STEPS; }
            else if (i == 1) { scat(v, DIV_NAME[tk->div % NDIV]); k[i].v = tk->div / (float)(NDIV - 1); }
            else if (i == 2) { fmt_u(v, tk->swing); scat(v, "%"); k[i].v = (tk->swing - 50) / 25.0f; }
            else { if (tk->transp) scat(v, tk->transp < 0 ? "-" : "+"); fmt_u(v + slen(v), (uint32_t)(tk->transp < 0 ? -tk->transp : tk->transp)); k[i].v = (tk->transp + 24) / 48.0f; k[i].bip = 1; }
            continue;
        }
        if (!len) { if (i == 0) { k[i].label = "LENGTH"; scat(v, "OFF"); } else k[i].label = 0; continue; }
        if (i == 0) { fmt_u(v, seq_cur[t] % len + 1u); scat(v, "/"); fmt_u(v + slen(v), len); k[i].v = (seq_cur[t] % len) / (float)(len > 1 ? len - 1 : 1); continue; }
        if (SV_EFF == SV_STEPS && drum_grid()) {   /* (BOOM 0.5) the grid: STEP · DRUM · VELO · ROLL of the row's drum on the step (a held pad's, else the cursor's) */
            static const char *const DL[4] = {"STEP", "DRUM", "VELO", "ROLL"};
            const step_t *ds = seq_step(t, step_held >= 0 ? (uint32_t)step_held : seq_cur[t] % len);
            int q = drum_slot(ds, drum_row[t]);
            k[i].label = DL[i];
            if (i == 1) { scat(v, drum_name(live_p[t].v, drum_row[t])); k[i].v = (float)(drum_row[t] % NDRUM) / (NDRUM - 1); }
            else if (q < 0) scat(v, "--");
            else if (i == 2) { uint32_t vl = ds->nv[q] ? ds->nv[q] : ds->vel; fmt_u(v, vl); k[i].v = vl / 127.0f; }
            else { fmt_u(v, ROLL_N[ds->nr[q] % 6u]); scat(v, "x"); k[i].v = (float)(ds->nr[q] % 6u) / 5.0f; }
            continue;
        }
        if (SV_EFF == SV_STEPS && i == 1) { roll_show(t); fmt_note(v, roll_row[t]); k[i].v = roll_row[t] / 127.0f; continue; }   /* (0.14) the roll's row */
        if (!st->note[0]) { scat(v, "--"); continue; }
        if (SV_EFF == SV_STEPS) {
            if (i == 1) { for (n = 0; n < SEQ_CHORD && st->note[n]; n++) continue; if (trk_type(t) == T_DRUMS) scat(v, drum_name(live_p[t].v, drum_of_note(st->note[0]))); else fmt_note(v, st->note[0]); if (n > 1u) { scat(v, "+"); fmt_u(v + slen(v), n - 1u); } k[i].v = st->note[0] / 127.0f; }
            else if (i == 2) { if (st->gate >= GATE_TIE) scat(v, "TIE"); else { fmt_u(v, (st->gate * 100u + 63u) / 127u); scat(v, "%"); } k[i].v = st->gate / 127.0f; }
            else { fmt_u(v, st->vel); k[i].v = st->vel / 127.0f; }
        } else {
            if (i == 1) { fmt_u(v, (st->ratch & 3u) + 1u); scat(v, "x"); k[i].v = (st->ratch & 3u) / 3.0f; }
            else if (i == 2) { fmt_u(v, st->chance); scat(v, "%"); k[i].v = st->chance / 100.0f; }
            else { scat(v, st->ratch & STEP_OFF ? "OFF" : "ON"); k[i].v = st->ratch & STEP_OFF ? 0.0f : 1.0f; }
        }
    }
}
static void seq_turn(uint32_t knob, int32_t e)
{
    uint32_t t = sel, len = pat.trk[t].len, k;
    seqtrk_t *tk = &pat.trk[t];
    int32_t sg = e > 0 ? 1 : -1;
    step_t *st;
    if (au_turn(knob, e)) return;               /* (3.7: TRACK+, an audio track) */
    if (seq_view == SV_SLOTS) {
        if (knob == 0) slot_sel = (uint32_t)clampi((int32_t)slot_sel + (e > 1 || e < -1 ? sg * 8 : sg), 0, NPAT - 1);   /* (a fast turn: a page of 8) */
        else if (knob == 2u && e > 0) pat_go(slot_sel);
        else if (knob == 3u && e > 0) pat_copy(slot_sel);
        return;
    }
    if (seq_view == SV_TRACKS && (knob == 0 || knob == 2u)) knob ^= 2u;   /* TRACKS: CUTOFF on knob 1, LENGTH on 3 */
    if (seq_view == SV_TRACKS && knob > 0) {    /* the track's mixer LEVEL, its sound's CUTOFF; REC's QUANTISE */
        if (knob == 1) { set.mix_lvl[t] = (uint8_t)clampi((int32_t)set.mix_lvl[t] + e * 2, 0, 127); mix_apply(); }
        else if (knob == 2) { patch_t *d = &live_p[t]; d->v[P_CUTOFF] = (uint8_t)clampi(d->v[P_CUTOFF] + e * 2, 0, 127); trk_edited[t] = 1; }
        else set.rec_qt[t] = (uint8_t)clampi((int32_t)(set.rec_qt[t] % NRQ) + sg, 0, NRQ - 1);
        set_touch();
        return;
    }
    if (seq_view == SV_TRACK || seq_view == SV_TRACKS || (!len && knob == 0)) undo_mark();   /* (3.8: undoable) */
    if (seq_view == SV_TRACK || seq_view == SV_TRACKS || (!len && knob == 0)) {   /* LENGTH (0 = off), DIV, SWING, TRANSPOSE */
        if (knob == 0) {
            int32_t l = (int32_t)tk->len;
            int32_t bl = (int32_t)seq_bar(t);   /* (a fast turn: a bar of the track's meter) */
            if (!l && e > 0) l = bl;
            else if (e > 1 || e < -1) l = e > 0 ? (l / bl + 1) * bl : (l - 1) / bl * bl;
            else l += e;
            tk->len = (uint16_t)clampi(l, 0, (int32_t)SEQ_STEPS);
            if (tk->len && seq_cur[t] >= tk->len) seq_cursor_set(t, tk->len - 1u);
        } else if (knob == 1) { tk->div = (uint8_t)clampi((int32_t)tk->div + sg, 0, NDIV - 1); seq_cursor_set(t, seq_cur[t]); }   /* (the page's steps follow) */
        else if (knob == 2) tk->swing = (uint8_t)clampi((int32_t)tk->swing + e, 50, 75);
        else tk->transp = (int8_t)clampi(tk->transp + sg, -24, 24);
        edited();
        return;
    }
    if (!len) return;
    if (knob == 0) {                            /* the cursor; keys held: the note goes on into the next step */
        if (entry_n && e > 0) {
            uint32_t n = seq_cur[t] % len;
            if (n + 1u >= len) return;
            st = seq_step(t, n); st->gate = GATE_TIE;
            *seq_step(t, n + 1u) = *st; seq_step(t, n + 1u)->gate = 110; seq_step(t, n + 1u)->ratch = 0;
            seq_cursor_set(t, n + 1u); edited();
            return;
        }
        seq_cursor_set(t, (uint32_t)(((int32_t)(seq_cur[t] % len) + e % (int32_t)len + (int32_t)len) % (int32_t)len));
        return;
    }
    if (SV_EFF == SV_STEPS && drum_grid()) {    /* (BOOM 0.5) the grid */
        uint32_t d = drum_row[t] % NDRUM; int q;
        if (knob == 1) { drum_row[t] = (uint32_t)clampi((int32_t)d + sg, 0, NDRUM - 1); if (!ctl_seq_run && drum_row[t] != d) drum_hear(t, drum_row[t], 100); return; }
        st = seq_step(t, step_held >= 0 ? (uint32_t)step_held : seq_cur[t] % len);
        undo_mark(); q = drum_slot(st, d);
        if (q < 0) q = drum_set(st, d, 100);    /* (turned on an empty place: the drum is set) */
        if (q < 0) { say("THE STEP IS FULL"); return; }
        if (knob == 2) st->nv[q] = (uint8_t)clampi((int32_t)(st->nv[q] ? st->nv[q] : st->vel) + e * 4, 1, 127);
        else st->nr[q] = (uint8_t)clampi((int32_t)(st->nr[q] % 6u) + sg, 0, 5);
        step_turned = 1; edited();
        return;
    }
    if (SV_EFF == SV_STEPS && knob == 1) {      /* (0.14) NOTE: the roll's row – the pitch the pads set and take away */
        uint32_t was; roll_show(t); was = roll_row[t];
        roll_pick(t, (uint32_t)clampi((int32_t)was + sg, 1, 127));
        if (!ctl_seq_run && roll_row[t] != was) { uint32_t nn = (uint32_t)clampi((int32_t)roll_row[t] + pat.trk[t].transp, 1, 127); note_event_p(t, 1, nn, 100); note_event_p(t, 0, nn, 0); }
        return;
    }
    st = seq_step(t, seq_cur[t] % len);
    undo_mark();                                /* (3.8: a step's knob turned – undoable) */
    if (!st->note[0]) { say("EMPTY STEP: PLAY A NOTE"); return; }
    else if (SV_EFF == SV_STEPS && knob == 2) st->gate = (uint8_t)clampi((int32_t)st->gate + e * 2, 1, GATE_TIE);
    else if (SV_EFF == SV_STEPS) { st->vel = (uint8_t)clampi((int32_t)st->vel + e * 2, 1, 127); for (k = 0; k < SEQ_CHORD && st->note[k]; k++) if (st->nv[k]) st->nv[k] = st->vel; }   /* (0.14: its notes' own velocities follow) */
    else if (knob == 1) st->ratch = (uint8_t)((st->ratch & STEP_OFF) | (uint32_t)clampi((int32_t)(st->ratch & 3u) + sg, 0, 3));
    else if (knob == 2) st->chance = (uint8_t)clampi((int32_t)st->chance + sg * 5, 0, 100);
    else st->ratch = (uint8_t)(e > 0 ? st->ratch & ~STEP_OFF : st->ratch | STEP_OFF);
    edited();
}

/* ---- now and then (from the draw): the arp's notes into REC, the swap on the bar, REC waiting, saving */
/* the arp's notes into REC, with the key's velocity. Each carries the place the ISR played it at (arpev_t.q8), so it
 * lands exactly there however late it is taken (from events and the draw – no UI work in the audio flow, 6.2) */
static void arp_rec_drain(void)
{
    take_check();
    while (arp_qr != arp_qw) {
        RING_TAKE();
        arpev_t e = arp_q[arp_qr % AQ];
        arp_qr++;
        if (e.part == sel && live_p[sel].v[P_ARP_MODE]) {
            rec_from_arp = 1; rec_arp_q8 = e.q8;
            rec_note(e.on, e.note, e.vel ? e.vel : 100u);
            rec_from_arp = 0; rec_arp_q8 = 0xFFFFFFFFu;
        }
    }
}
/* the ISR swapped the next pattern in (its sounds and its mix already, on the downbeat): the panel follows */
static void seq_swap_check(void)
{
    if (ui_pat_swapped == swap_seen) return;
    swap_seen = ui_pat_swapped;
    RING_TAKE();
    if (ctl_pat_keep) pats[set.pat] = pat_old;  /* (the one left, with its edits) */
    {   /* the one left keeps its instruments as they sounded to the end, and its mixer */
        psnd_t *s = &psnd[set.pat]; s->magic = PSN2_MAGIC;
        for (uint32_t t = 0; t < NTRK; t++) { s->p[t] = snd_old[t]; s->prog[t] = (uint8_t)trk_prog[t]; s->edited[t] = (uint8_t)trk_edited[t]; }
        mx_from_set(&s->mx);
        snd_dirty |= 1ull << set.pat;
    }
    set.pat = (uint8_t)(pat_queue ? pat_queue - 1u : set.pat);
    snd_meta(set.pat);
    au_pattern();                               /* (3.8: the panel's audio follows the pattern the ISR swapped in) */
    undo_n = redo_n = 0;                        /* (UNDO belongs to the pattern) */
    pat_queue = 0;
    memset(seq_cur, 0, sizeof seq_cur); memset(seq_pg, 0, sizeof seq_pg);
    pat_edited = 0; slot_sel = set.pat; set_touch();
    { static char msg[16]; msg[0] = 0; scat(msg, "PATTERN "); fmt_u(msg + slen(msg), set.pat + 1u); say(msg); }
}
static void seq_poll(void)
{
    arp_rec_drain();                            /* (the arp's notes into REC – each carries where it played) */
    seq_swap_check();                           /* (a pattern swapped in on the downbeat: the panel follows) */
    if (pat_requeue && ui_pat_swapped == swap_seen && ctl_pat_swap == 0u) { uint32_t n = pat_requeue - 1u; pat_requeue = 0; pat_go(n); }
    if (pat_queue && !ctl_seq_run) { uint32_t n = pat_queue - 1u; ctl_pat_swap = 0; pat_queue = 0; pat_switch(n); }
    if (rec_wait) {                             /* REC waits for a track's bar: there it is */
        uint32_t w = rec_wait - 1u;
        if (rec != REC_ON || sel != w) rec_wait = 0;
        else if (ctl_trk_on >> w & 1u && !(trk_pending() >> w & 1u)) { rec_wait = 0; memset(rec_open, 0, sizeof rec_open); }
    }
    if (rec_down && !rec_armed_pads && o3_ms - rec_down_t > 1200u && !rec_hint) {   /* REC held: letting go clears the track (0.29) */
        static char m[48]; m[0] = 0; scat(m, "LET GO: CLEAR "); { char l[2] = {(char)('A' + sel), 0}; scat(m, l); } scat(m, " · PUSH A KNOB: ARM");
        say(m); rec_hint = 1;
    }
    if (!rec_down) rec_hint = 0;
    ctl_click = set.click % NCLICK == CLICK_ON || ((rec == REC_ON || ui_au_state) && set.click % NCLICK != CLICK_OFF);
    if (!pat_queue && (store_force || o3_ms - store_t > 1500u)) {   /* the sounds now changed (a program, a knob): this pattern's set follows */
        const psnd_t *s = &psnd[set.pat];
        mixset_t now;
        int same = s->magic == PSN2_MAGIC;
        mx_from_set(&now);
        for (uint32_t t = 0; t < NTRK && same; t++) same = s->prog[t] == trk_prog[t] && s->edited[t] == trk_edited[t] && bytes_same(&s->p[t], &live_p[t], sizeof(patch_t));
        if (same) same = bytes_same(&s->mx, &now, sizeof now);
        if (!same) snd_capture(set.pat);
    }
    if (snd_dirty && (store_force || o3_ms - store_t > 1500u)) {
        for (uint32_t n = 0; n < NPAT; n++) if (snd_dirty >> n & 1u) psnd_file_save(n);
        snd_dirty = 0;
    }
    if (pats_dirty && (store_force || o3_ms - store_t > 1500u) && step_held < 0 && !entry_n) {   /* saved by themselves, also while playing */
        pats[set.pat] = pat;
        for (uint32_t n = 0; n < NPAT; n++)
            if (pats_dirty >> n & 1u) { pats[n].magic = PAT_MAGIC; if (!pat_file_save(n, &pats[n])) pat_used |= 1ull << n; }
        pats_dirty = 0; pat_edited = 0;
    }
}
static void seq_boot(void)                      /* the 8 patterns as they were, else empty */
{
    for (uint32_t n = 0; n < NPAT; n++) {       /* (BOOM 0.4: bm_save.h – read field by field; a 0.3 file is written anew) */
        if (pat_file_load(n, &pats[n])) pat_factory(n, &pats[n]);
        else { pat_used |= 1ull << n; if (bmf_legacy(PB_FILE_N(n))) { pats_dirty |= 1ull << n; store_t = o3_ms; } }
    }
    for (uint32_t n = 0; n < NPAT; n++) {
        if (psnd_file_load(n) || psnd[n].magic != PSN2_MAGIC) psnd[n].magic = 0;
        else if (bmf_legacy(PS_FILE_N(n))) { snd_dirty |= 1ull << n; store_t = o3_ms; }
    }
    if (set.pat >= NPAT) set.pat = 0;
    pat = pats[set.pat];
    slot_sel = set.pat;
    if (psnd[set.pat].magic == PSN2_MAGIC) {    /* the pattern's own instruments (else the settings' sounds stay, and become its) */
        patch_t nx[NTRK]; snd_target(set.pat, nx);
        for (uint32_t t = 0; t < NTRK; t++) live_p[t] = nx[t];
    }
    snd_meta(set.pat);
}

/* (12.11.2026) GLOBAL, knob 4: the chosen track's meter. Its length keeps its bars (4 bars of 4/4 -> 4 bars of 3/4); the
 * steps past a shorter length stay, as LENGTH leaves them */
static void meter_turn(int e)
{
    static char msg[24];
    uint32_t t = sel % NTRK, m0 = meter_of(t), b0 = seq_bar(t), m = (uint32_t)clampi((int32_t)m0 + (e > 0 ? 1 : -1), 0, NMETER - 1), b1;
    seqtrk_t *tk = &pat.trk[t];
    if (m == m0) return;
    undo_mark();
    tk->pad[0] = (uint8_t)m;
    b1 = seq_bar(t);
    if (tk->len && !(au_is(t))) {
        uint32_t bars = (tk->len + b0 - 1u) / b0, l = bars * b1;
        tk->len = (uint16_t)(l > SEQ_STEPS ? SEQ_STEPS / b1 * b1 : l);
        if (!tk->len) tk->len = (uint16_t)(b1 < SEQ_STEPS ? b1 : SEQ_STEPS);
    }
    seq_cursor_set(t, tk->len && seq_cur[t] >= tk->len ? tk->len - 1u : seq_cur[t]);
    edited();
    msg[0] = (char)('A' + t); msg[1] = 0; scat(msg, " METER "); scat(msg, METER[m].name); say(msg);
}
