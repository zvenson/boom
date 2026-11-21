/* SPDX-License-Identifier: GPL-3.0-only */
/* The step sequencer (VIBE edition): four tracks A-D, each with its own 256 steps, length, note value, swing and
 * transpose, each playing its own sound. A step holds a chord of up to four notes, a velocity, a gate (or a tie into
 * the next step), a ratchet (1-4 hits), a chance and a nudge (how late in the step it sounds, for unquantised
 * recording). The tracks start and stop on the bar (as clips in Ableton Live): a request waits for the next downbeat;
 * the next pattern comes in on a downbeat too.
 * The player runs in the audio ISR per control block, as the arpeggiator does; the main loop edits `pat`. */
#define NTRK 8u                                 /* (BOOM 0.11: eight tracks A-H; 0.3: six) */
#define SEQ_STEPS 256u
#define SEQ_CHORD 8u                            /* (BOOM 0.5: eight notes a step – a DRUMS track's eight drums at once; OberVave: 4) */
#define GATE_TIE 127u                           /* a gate of 127: the notes hold into the next step */
#define NPAT NBLOB                              /* patterns, one blob each */
#define PAT_MAGIC 0x38514553u                   /* "SEQ8" (0.11; before: "SEQ6" (BOOM: six tracks – a 4-track pattern is passed over) */

/* (BOOM 0.5, Sven 14.11.: the drum grid) each note of a step has its own velocity (nv, 0 = the step's vel) and its own roll
 * (nr: index into ROLL_N – that note alone is hit that many times across the step; the step's ratch still hits them all) */
static const uint8_t ROLL_N[6] = {1, 2, 3, 4, 6, 8};
typedef struct { uint8_t note[SEQ_CHORD], vel, gate, ratch, chance, nudge, nv[SEQ_CHORD], nr[SEQ_CHORD]; } step_t;   /* note 0 = none; ratch 0..3 = 1..4 hits;
                                                                                         nudge 0..127: that much of the step later */
#define STEP_OFF 0x80u                          /* in ratch: the step is switched off, its notes kept */
typedef struct { uint16_t len; uint8_t div, swing; int8_t transp; uint8_t mute, pad[2]; uint8_t tm, tpad; uint16_t ts, te; } seqtrk_t;
/* (0.32, Sven: "Spur bei Length 0 geht in den Time Mode – ohne Takte") tm 1: TIME – the track's loop is not whole steps: it
 * runs from ts to te (in 1/128 of a step: step * 128 + how far into it), te 0 = no loop yet (REC records it freely, from
 * the moment REC is pressed until REC is pressed again). The notes still sit on the steps, with their nudge. */
#define TM_FINE 128u   /* swing 50..75 % · mute (0.31): the track in this pattern 1 = plays, 2 = stopped, 0 = not stored */
/* (OberVave 3) the knobs recorded: per track up to NLANE lanes, each one parameter of the track's sound with a value per
 * step (AUTO_NONE: that step leaves it as it is). REC running + a knob turned = the value written on the step playing;
 * the player sets it on its step. After the steps, behind auto_magic: a pattern saved before has none (cleared at load) */
#define NLANE 4u
#define AUTO_NONE 0xFFu
#define AUTO_MAGIC 0x4F545541u                  /* "AUTO" */
typedef struct { uint8_t param, pad[3]; uint8_t v[SEQ_STEPS]; } lane_t;
typedef struct { uint32_t magic; seqtrk_t trk[NTRK]; step_t st[NTRK][SEQ_STEPS]; uint32_t auto_magic; lane_t lane[NTRK][NLANE]; } pattern_t;

static pattern_t pat;                           /* the pattern now: the main loop edits, the ISR plays */
/* (12.11.2026, Sven: "man kann nur 4/4 Takt?") the meter, per track: trk.pad[0] (0 = 4/4, as every pattern before it).
 * A track's bar: its STEPS pages, LENGTH's bar jumps. Track A's is the song's: the click and its accent, the count-in,
 * the downbeat the tracks, REC and the next pattern wait for. The tracks still loop their own LENGTH (polymeter). */
#define NMETER 10u
static const struct { char name[5]; uint8_t e8, den8; } METER[NMETER] = {   /* e8: the bar in eighths; den8: counted in eighths */
    {"4/4", 8, 0}, {"3/4", 6, 0}, {"2/4", 4, 0}, {"5/4", 10, 0}, {"7/4", 14, 0},
    {"6/8", 6, 1}, {"5/8", 5, 1}, {"7/8", 7, 1}, {"9/8", 9, 1}, {"12/8", 12, 1}};
static uint32_t meter_of(uint32_t t) { return pat.trk[t % NTRK].pad[0] % NMETER; }
static uint32_t song_e8(void) { return METER[meter_of(0)].e8; }            /* the song's bar (track A's), in eighths */
static uint32_t song_bar_frames(uint32_t q) { return song_e8() * q / 2u; }   /* q: a quarter's frames */
static step_t *seq_step(uint32_t t, uint32_t i) { return &pat.st[t % NTRK][i % SEQ_STEPS]; }

/* main loop -> ISR */
static volatile uint32_t ctl_seq_run, ctl_seq_restart;
/* the tracks' start / stop: the main loop flips a bit of ctl_trk_req, the ISR takes it on the next downbeat (stopped:
 * at once) and flips the same bit of trk_req_seen; ctl_trk_on is the ISR's: which tracks play */
static volatile uint32_t ctl_trk_req, ctl_trk_on = (1u << NTRK) - 1u;
static volatile uint32_t ui_pat_old_on;         /* (0.31) which tracks played in the pattern the ISR just swapped out */
static uint32_t trk_req_seen;
static uint32_t trk_pending(void) { return (ctl_trk_req ^ trk_req_seen) & ((1u << NTRK) - 1u); }
/* the next pattern: the main loop reads it into pat_q and sets ctl_pat_swap; the ISR swaps it in on the next downbeat
 * (the one it leaves goes to pat_old, so its edits can be saved at the stop) */
static pattern_t pat_q, pat_old;
static volatile uint32_t ctl_pat_swap, ctl_pat_keep, ui_pat_swapped;   /* ctl_pat_swap: 1 waiting, 2 the ISR swaps, 0 */   /* keep: the one left goes to pat_old */
/* (OberVave 3) every pattern has its own four sounds: they change with it on the same downbeat (snd_q in, the ones
 * left to snd_old), so the new pattern's first note already plays its own instrument */
static patch_t snd_q[NTRK], snd_old[NTRK];
static volatile uint32_t ctl_snd_swap;
/* ISR -> main loop: where each track is (step, and how far into it, Q8) */
static volatile uint32_t ui_seq_pos[NTRK], ui_seq_frac[NTRK], ui_seq_dur[NTRK], ui_seq_ticks, ui_seq_beat, ui_seq_bar_beat;

/* (3.7) the audio tracks (obv_audio.h): which tracks are audio, the song's frame since the start, where each loop began */
static volatile uint32_t ctl_au_type;           /* bit t: track t is an audio track (its steps are not played) */
static int32_t au_song = -1, au_from[NTRK];     /* (ISR) au_song: frames since bar 1, -1 = not running (or counting in) */
static void au_swap_in(void);                   /* (3.8, obv_audio.h) the next pattern's audio, on its downbeat */

/* ---- a fresh pattern */
static void step_clear(step_t *s)
{
    uint32_t i;
    for (i = 0; i < SEQ_CHORD; i++) s->note[i] = s->nv[i] = s->nr[i] = 0;
    s->vel = 100;
    s->gate = 96;                               /* 76 % */
    s->ratch = 0;
    s->chance = 100;
    s->nudge = 0;
}
static void lane_clear(lane_t *l) { uint32_t i; l->param = AUTO_NONE; for (i = 0; i < SEQ_STEPS; i++) l->v[i] = AUTO_NONE; }
static void auto_init(pattern_t *p)
{
    uint32_t t, l;
    p->auto_magic = AUTO_MAGIC;
    for (t = 0; t < NTRK; t++) for (l = 0; l < NLANE; l++) lane_clear(&p->lane[t][l]);
}
static void pat_init(pattern_t *p)
{
    uint32_t i, t;
    p->magic = PAT_MAGIC;
    auto_init(p);
    for (t = 0; t < NTRK; t++) {
        for (i = 0; i < SEQ_STEPS; i++) step_clear(&p->st[t][i]);
        p->trk[t].len = 16;                     /* a bar of sixteenths each */
        p->trk[t].div = D_16;
        p->trk[t].swing = 50;
        p->trk[t].transp = 0;
        p->trk[t].mute = 0;
        p->trk[t].pad[0] = p->trk[t].pad[1] = 0;
        p->trk[t].tm = p->trk[t].tpad = 0; p->trk[t].ts = p->trk[t].te = 0;
    }
}
static void pat_factory(uint32_t n, pattern_t *p)   /* a pattern never saved: empty, four bars of sixteenths */
{
    (void)n;
    pat_init(p);
}

/* ---- the player (ISR, per CTL block) */
#define NO_WAIT 0xFFFFFFFFu
typedef struct { uint32_t cnt, dur, step, sub, rat_n, rat_i, gate_at, nsnd, play, tie, wait, on_at, hold_end, frac; uint8_t snd[SEQ_CHORD], roll_i[SEQ_CHORD]; } seqrun_t;   /* hold_end: a late note's end in the next step */
static seqrun_t sr[NTRK];
static uint32_t seq_was, seq_beat_cnt, seq_beat_n, count_left;

/* a note held on the keys while REC takes it (main loop -> ISR): the key owns it, the sequencer neither plays nor ends
 * it on that track (else the step just recorded, or the loop coming round, cut the long note being played) */
static volatile uint32_t ctl_live_hold[NTRK][4];
static int seq_live(uint32_t t, uint32_t n) { return ctl_live_hold[t % NTRK][n >> 5 & 3u] >> (n & 31u) & 1u; }
/* an arp take (REC, the arp on the track playing into it): the track's steps are not played meanwhile – you hear the arp
 * that is being written, not the last round of it on top */
static volatile uint32_t ctl_rec_mute;
/* (0.32) TIME, the free take: ctl_tm_grow bit t = track t records its loop now (no wrap until REC closes it);
 * ctl_tm_go bit t = start it from its top in the next block (not on the bar) */
static volatile uint32_t ctl_tm_grow, ctl_tm_go;

static void seq_release_t(uint32_t t)
{
    seqrun_t *r = &sr[t];
    uint32_t i;
    for (i = 0; i < r->nsnd; i++) if (!seq_live(t, r->snd[i])) seq_note_off(t, r->snd[i]);
    r->nsnd = 0;
}
static int seq_feeds_arp(uint32_t p) { return pt[p % NPART].p->v[P_ARP_MODE] != A_OFF && !pat.trk[p % NTRK].pad[1]; }   /* (0.31) */
static uint32_t seq_note(const step_t *s, uint32_t i, int32_t transp)
{
    return (uint32_t)clampi((int32_t)s->note[i] + transp, 1, 127);
}
static void seq_sound(uint32_t t, const step_t *s, int32_t transp)
{
    seqrun_t *r = &sr[t];
    uint32_t i;
    for (i = 0; i < SEQ_CHORD && s->note[i] && r->nsnd < SEQ_CHORD; i++) {
        uint32_t n = seq_note(s, i, transp);
        if (seq_live(t, n) || ctl_rec_mute >> t & 1u) continue;
        TRACE('S', t, n);
        seq_note_on(t, n, s->nv[i] ? s->nv[i] : s->vel ? s->vel : 100u);
        r->snd[r->nsnd++] = (uint8_t)n;
    }
}
static uint32_t seq_has(const step_t *s, uint32_t m, int32_t transp)   /* the step plays note m */
{
    uint32_t i;
    for (i = 0; i < SEQ_CHORD && s->note[i]; i++)
        if (seq_note(s, i, transp) == m) return 1;
    return 0;
}

/* a tie, note by note: what the new step plays again keeps sounding, the rest lets go, the new ones start */
static void seq_tie_into(uint32_t t, const step_t *s, int32_t transp, uint32_t play)
{
    seqrun_t *r = &sr[t];
    uint32_t i, j, keep = 0;
    for (i = j = 0; i < r->nsnd; i++) {
        if (play && seq_has(s, r->snd[i], transp)) { r->snd[j++] = r->snd[i]; keep++; }
        else if (!seq_live(t, r->snd[i])) seq_note_off(t, r->snd[i]);
    }
    r->nsnd = j;
    if (!play) return;
    for (i = 0; i < SEQ_CHORD && s->note[i] && r->nsnd < SEQ_CHORD; i++) {
        uint32_t m = seq_note(s, i, transp);
        for (j = 0; j < keep && r->snd[j] != m; j++) ;
        if (j < keep || seq_live(t, m) || ctl_rec_mute >> t & 1u) continue;
        seq_note_on(t, m, s->nv[i] ? s->nv[i] : s->vel ? s->vel : 100u);
        r->snd[r->nsnd++] = (uint8_t)m;
    }
}

static void seq_from_top(uint32_t t)            /* track t: step 1 in this block */
{
    seqrun_t *r = &sr[t];
    seq_release_t(t);
    r->step = 0xFFFFFFFFu;
    r->cnt = r->dur = r->frac = 0;
    r->play = r->tie = 0;
    r->wait = NO_WAIT;
    r->hold_end = 0;
}

static void seq_take_requests(void)             /* the start / stop requests (on a downbeat, or at once when stopped) */
{
    uint32_t p = trk_pending(), t;
    if (!p) return;
    for (t = 0; t < NTRK; t++)
        if (p >> t & 1u) {
            if (ctl_trk_on >> t & 1u) { ctl_trk_on &= ~(1u << t); seq_release_t(t); }
            else { ctl_trk_on |= 1u << t; seq_from_top(t); au_from[t] = au_song > 0 ? au_song : 0; }
        }
    trk_req_seen ^= p;
}

/* (VIBE, SDK 8) chase: start where the DAW is – every track on the step that belongs to that moment (counted from the
 * song's start, as if it had played from bar 1), with the part of that step already gone; the beat inside the bar too.
 * Set by vibe_on_audio when the DAW starts or jumps (a loop going round, a click in its timeline). */
static volatile uint32_t ctl_seq_chase;
static double seq_chase_beat, seq_chase_bar;    /* quarter notes: where, and where its bar began */
static uint32_t seq_loop_on, seq_loop_at;       /* the DAW's loop end ahead: at engine frame seq_loop_at (audio thread only) */
static double seq_loop_beat, seq_loop_bar;      /* where it goes back to */
#define CHASE_SLACK 512u                        /* frames: a jump seen this late (a DAW's sub-block) – a track already there stays */
static uint32_t near_frames(uint32_t a, uint32_t b, uint32_t m) { uint32_t d = a > b ? a - b : b - a; return d <= CHASE_SLACK || (m && m - d <= CHASE_SLACK); }
static void seq_chase_apply(int running)        /* running: a jump while playing – a track already in place keeps going */
{
    uint32_t t, q = quarter_len();
    double b = seq_chase_beat, in_bar = b - seq_chase_bar;
    for (t = 0; t < NTRK; t++) {
        seqrun_t *r = &sr[t];
        uint32_t len = pat.trk[t].len, dv = DIV_PER_QUARTER[pat.trk[t].div % NDIV], cnt, step;
        double st = b * (double)dv;
        uint64_t n = (uint64_t)st;
        if (!len) continue;
        step = (uint32_t)(n % len);
        cnt = (uint32_t)((st - (double)n) * (double)(q / dv));   /* (how much of the step is gone) */
        if (running && r->dur && r->step != 0xFFFFFFFFu &&
            ((r->step == step && near_frames(r->cnt, cnt, 0)) ||   /* (there already, or just before the step begins) */
             ((r->step + 1u) % len == step && r->dur - r->cnt <= CHASE_SLACK && cnt <= CHASE_SLACK))) continue;
        seq_from_top(t);
        r->step = (step + len - 1u) % len;      /* (the step before: the next tick enters it at once – dur 0) */
        r->cnt = cnt;
    }
    au_song = (int32_t)(b * (double)q + 0.5);   /* (3.7) the audio tracks: there too, the loops counted from bar 1 */
    for (t = 0; t < NTRK; t++) au_from[t] = 0;
    if (in_bar < 0.0) in_bar = 0.0;
    {
        uint32_t whole = (uint32_t)in_bar, cnt = (uint32_t)((in_bar - (double)whole) * (double)q), n_now, c_now;
        if (cnt < CTL) { c_now = 0; n_now = whole; }   /* on a beat: it ticks now */
        else { c_now = cnt; n_now = whole + 1u; }      /* inside one: the next ticks */
        if (!running || 2u * n_now % song_e8() != 2u * seq_beat_n % song_e8() || !near_frames(seq_beat_cnt, c_now, q)) { seq_beat_cnt = c_now; seq_beat_n = n_now; }
    }
    count_left = 0;
}

static int seq_phase(uint32_t *ph) { *ph = seq_beat_cnt; return ctl_seq_run && seq_was; }
static int seq_starting(void) { return ctl_seq_run && (!seq_was || ctl_seq_restart); }

/* (VIBE) where track t is in this block (the arp asks before the sequencer's turn: a step that begins in this block
 * counts as begun) – Q8: step * 256 + how far into it; 0xFFFFFFFF = not running */
static uint32_t seq_q8_now(uint32_t t)
{
    const seqrun_t *r = &sr[t % NTRK];
    uint32_t len = pat.trk[t % NTRK].len, step = r->step, cnt = r->cnt, dur = r->dur;
    if (!len || !ctl_seq_run) return 0xFFFFFFFFu;
    if (seq_starting() || step == 0xFFFFFFFFu || !dur) return 0;
    if (cnt >= dur) { cnt -= dur; step = step + 1u >= len ? 0u : step + 1u; }
    return (step % len) * 256u + (cnt * 256u / dur > 255u ? 255u : cnt * 256u / dur);
}
/* (VIBE) the tracks' place at the end of each block, with that block's frame – the main loop finds where a key was
 * HEARD (Protokoll 5: vibe_event_heard) from it. A seqlock: odd = being written. */
static struct { volatile uint32_t seq, frame; uint32_t pos[NTRK], cnt[NTRK], dur[NTRK]; } snap;
static void snap_write(void)
{
    uint32_t t;
    snap.seq++; RING_PUBLISH();
    for (t = 0; t < NTRK; t++) { snap.pos[t] = sr[t].step == 0xFFFFFFFFu ? 0u : sr[t].step; snap.cnt[t] = sr[t].cnt; snap.dur[t] = sr[t].dur; }
    snap.frame = eng_t;
    RING_PUBLISH(); snap.seq++;
}
static int snap_q8(uint32_t t, uint32_t frame, uint32_t *q8)   /* track t's Q8 position at that frame (main loop) */
{
    uint32_t a, b, pos, cnt, dur, f, len = pat.trk[t % NTRK].len;
    int32_t back;
    do { a = snap.seq; RING_TAKE(); pos = snap.pos[t % NTRK]; cnt = snap.cnt[t % NTRK]; dur = snap.dur[t % NTRK]; f = snap.frame; RING_TAKE(); b = snap.seq; } while (a != b || (a & 1u));
    back = (int32_t)(f - frame);                /* frames from what was heard to the snapshot */
    if (!len || !dur || back < 0 || back > (int32_t)(FS * 2u)) return 0;
    {
        int64_t q = (int64_t)(pos % len) * 256 + (int64_t)cnt * 256 / dur - (int64_t)back * 256 / dur, L = (int64_t)len * 256;
        const seqtrk_t *k = &pat.trk[t % NTRK];
        if (k->tm && k->te > k->ts) {           /* (0.32 TIME) the loop ts .. te: back across its seam */
            int64_t lo = (int64_t)k->ts * 2, hi = (int64_t)k->te * 2;
            while (q < lo) q += hi - lo;
            while (q >= hi) q -= hi - lo;
        } else { q %= L; if (q < 0) q += L; }
        *q8 = (uint32_t)q;
    }
    return 1;
}

static void mx_swap_in(void)                    /* (ISR, inside the pattern swap: after its RING_TAKE) */
{
    uint32_t t;
    if (!ctl_mx_swap) return;
    for (t = 0; t < NPART; t++) {
        ctl_mix_lvl[t] = mx_q.lvl[t]; ctl_mix_cho[t] = mx_q.cho[t]; ctl_mix_dly[t] = mx_q.dly[t];
        ctl_mix_rev[t] = mx_q.rev[t]; ctl_mix_drv[t] = mx_q.drv[t]; ctl_mix_pan[t] = mx_q.pan[t];
    }
    ctl_mix_off = mx_q.off;
    ctl_dv_type = mx_q.dv_type; ctl_dv_amt = mx_q.dv_amt; ctl_dv_tone = mx_q.dv_tone; ctl_dv_mix = mx_q.dv_mix;
    ctl_m_vol = mx_q.m_vol; ctl_m_dj = mx_q.m_dj; ctl_m_drive = mx_q.m_drive; ctl_m_comp = mx_q.m_comp;
    for (t = 0; t < 3u; t++) ctl_mfx[t] = mx_q.mfx[t];   /* (BOOM: MASTER FX follow the pattern; REPEAT is never a pattern's) */
    if (mx_q.tempo) ctl_tempo = mx_q.tempo;     /* (3.2) the pattern's tempo, from its first note */
    for (t = 0; t < NPART; t++) for (uint32_t i = 0; i < 5u; i++) ctl_eq[t][i] = mx_q.eq[t][i];   /* (0.31) its EQ on the same downbeat */
    ctl_mx_swap = 0;
}

/* the recorded knobs of track t on step i: into its sound (a lane being recorded right now is left to the hand) */
static volatile uint32_t ctl_auto_hold[NTRK];   /* main loop: bit l = lane l of track t is being turned */
static void seq_auto(uint32_t t, uint32_t i)
{
    uint32_t l;
    for (l = 0; l < NLANE; l++) {
        const lane_t *a = &pat.lane[t][l];
        uint8_t v = a->v[i % SEQ_STEPS];
        if (a->param == AUTO_NONE || v == AUTO_NONE || (ctl_auto_hold[t] >> l & 1u) || a->param >= NPARAM) continue;
        live_p[t].v[a->param] = v;
    }
}
/* an eighth begins in this block (e: since the bar's count began; quarter: it is a beat as well): the downbeat of the
 * song's bar (track A's meter) takes what waited for it; the click on the meter's beats, accented on the one */
static void seq_eighth(uint32_t e, uint32_t quarter)
{
    uint32_t bar = song_e8(), den8 = METER[meter_of(0)].den8, t, one = 1u;
    int down = e % bar == 0u;
    if (!ctl_count && down) {                   /* the bar's first beat: what waited for it */
        if (ctl_pat_swap == 1u && __atomic_compare_exchange_n(&ctl_pat_swap, &one, 2u, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
            /* the next pattern (2 = being swapped in: the main loop leaves pat_q alone): the tracks from the top */
            if (ctl_pat_keep) pat_old = pat;
            pat = pat_q;
            {   /* (0.31, Sven: "in Pattern 12 laufen andere Spuren als in 11") the new pattern's tracks on / off: trk.mute 1 = plays,
                 * 2 = stopped, 0 = a pattern from before (the tracks stay as they are) */
                uint32_t on = ctl_trk_on;
                ui_pat_old_on = on;
                for (t = 0; t < NTRK; t++) { if (pat.trk[t].mute == 1u) on |= 1u << t; else if (pat.trk[t].mute == 2u) on &= ~(1u << t); }
                ctl_trk_on = on;
            }
            if (ctl_snd_swap) { for (t = 0; t < NTRK; t++) { snd_old[t] = live_p[t]; live_p[t] = snd_q[t]; } ctl_snd_swap = 0; }
            mx_swap_in();                       /* (its mixer, drive and master on the same downbeat) */
            au_swap_in();                       /* (3.8: and its audio tracks, from their top) */
            RING_PUBLISH();
            ctl_pat_swap = 0;
            ui_pat_swapped++;
            for (t = 0; t < NTRK; t++) seq_from_top(t);
        }
        seq_take_requests();
    }
    if (!quarter && !den8) return;              /* (x/4: the click on the quarters) */
    ui_seq_beat++;
    ui_seq_bar_beat = e % bar / (den8 ? 1u : 2u);
    if (ctl_click || ctl_count) click_start((uint32_t)down);
}
static void seq_tick(void)
{
    uint32_t t, q;
    if (!ctl_seq_run) {
        if (seq_was) { for (t = 0; t < NTRK; t++) seq_release_t(t); seq_was = 0; }
        au_song = -1;
        seq_take_requests();                    /* (stopped: a pad switches its track at once) */
        return;
    }
    if (!seq_was | (__atomic_exchange_n(&ctl_seq_restart, 0u, __ATOMIC_ACQ_REL) != 0u)) {   /* start: step 1 of every
                                                 * track in this block (exchange: a restart asked meanwhile is not lost) */
        au_song = 0;                            /* (3.7) bar 1 (a count-in: after it) */
        for (t = 0; t < NTRK; t++) au_from[t] = 0;
        seq_take_requests();
        for (t = 0; t < NTRK; t++) seq_from_top(t);
        seq_beat_cnt = 0;
        seq_beat_n = 0;
        count_left = ctl_count ? (song_e8() + 1u) / 2u : 0u;   /* (a bar of A in quarters; 5/8, 7/8: rounded up) */
        if (ctl_count) au_song = -1;
        if (__atomic_exchange_n(&ctl_seq_chase, 0u, __ATOMIC_ACQ_REL)) { ctl_count = 0; seq_chase_apply(0); }   /* (the DAW's place) */
        seq_was = 1;
    }
    else if (seq_loop_on && (int32_t)(eng_t - seq_loop_at) > 0) {   /* the DAW's loop ends in this block: back to its start */
        seq_loop_on = 0; ctl_seq_chase = 0; seq_chase_beat = seq_loop_beat; seq_chase_bar = seq_loop_bar; ctl_count = 0; seq_chase_apply(1);
    }
    else if (__atomic_exchange_n(&ctl_seq_chase, 0u, __ATOMIC_ACQ_REL)) { ctl_count = 0; seq_chase_apply(1); }   /* (the DAW jumped) */
    q = quarter_len();
    if (seq_beat_cnt < CTL) {                   /* a beat (in this block): the PLAY light, the click, the count-in; a downbeat: requests */
        if (ctl_count && !count_left) {         /* counted in: the tracks from the top, now */
            ctl_count = 0;
            seq_beat_n = 0;
            for (t = 0; t < NTRK; t++) { seq_from_top(t); au_from[t] = 0; }
            au_song = 0;
        }
        seq_eighth(2u * seq_beat_n, 1);
        seq_beat_n++;
        if (count_left) count_left--;
        ui_seq_count = ctl_count ? count_left + 1u : 0u;
    } else if (seq_beat_cnt >= q / 2u && seq_beat_cnt - q / 2u < CTL && seq_beat_n)   /* the beat's second eighth */
        seq_eighth(2u * seq_beat_n - 1u, 0);
    seq_beat_cnt += CTL;
    if (seq_beat_cnt >= q)                      /* the next beat: what is over keeps going (a DAW's quarter is any length) */
        seq_beat_cnt = seq_beat_cnt - q < CTL ? seq_beat_cnt - q : 0u;
    if (ctl_count) return;                      /* counting in: no steps yet */
    {   /* (0.32 TIME) a free take begins: its track from the top, now */
        uint32_t go = __atomic_exchange_n(&ctl_tm_go, 0u, __ATOMIC_ACQ_REL);
        for (t = 0; t < NTRK; t++) if (go >> t & 1u) { ctl_trk_on |= 1u << t; seq_from_top(t); }
    }
    for (t = 0; t < NTRK; t++) {
        seqrun_t *r = &sr[t];
        const seqtrk_t *k = &pat.trk[t];
        const step_t *s;
        uint32_t len = k->len, tmw = k->tm && k->te > k->ts, ecut = 0, early, wrap = 0;
        if (k->tm && !tmw && !(ctl_tm_grow >> t & 1u)) len = 0;   /* (0.32 TIME, no loop yet: silent until REC) */
        if (tmw && (k->te & (TM_FINE - 1u)) && r->step == k->te / TM_FINE && r->dur) ecut = (r->dur * (k->te & (TM_FINE - 1u))) >> 7;
        early = ecut && r->cnt >= ecut;         /* (TIME: the loop ends inside this step) */
        if (!len || !(ctl_trk_on >> t & 1u) || (ctl_au_type >> t & 1u) || (ctl_trk_daw >> t & 1u)) { seq_release_t(t);   /* (BOOM 0.4: the DAW plays it) */ r->tie = 0; r->wait = NO_WAIT; r->hold_end = 0; continue; }
        if (r->cnt >= r->dur || early) {        /* the next step */
            uint32_t dv = DIV_PER_QUARTER[k->div % NDIV], base = q / dv, sw, carry = 0;
            r->frac += q % dv;                  /* (the rest of q spread over the steps: div steps are exactly a quarter) */
            if (r->frac >= dv) { r->frac -= dv; base++; }
            if (r->hold_end) { r->hold_end = 0; seq_release_t(t); }   /* (a carried note: over by now) */
            if (r->nsnd && r->wait == NO_WAIT && r->gate_at != 0xFFFFFFFFu) {   /* a late note (nudged) lasts into this step */
                uint32_t e = r->on_at + r->rat_i * r->sub + r->gate_at;
                uint32_t bd = early ? ecut : r->dur;
                if (e > bd) carry = e - bd;
            }
            r->cnt -= early ? ecut : r->dur;
            if (tmw) {                          /* (0.32 TIME) from ts on, round at te */
                wrap = r->step == 0xFFFFFFFFu || early || r->step + 1u >= (k->te + TM_FINE - 1u) / TM_FINE;
                r->step = wrap ? k->ts / TM_FINE : r->step + 1u;
            } else r->step = r->step + 1u >= len ? 0u : r->step + 1u;
            sw = tmw ? 0u : base * (uint32_t)(clampi(k->swing, 50, 75) - 50) / 50u;
            r->dur = r->step & 1u ? base - sw : base + sw;   /* swing: the first of each pair longer */
            if (r->dur < CTL) r->dur = CTL;
            s = seq_step(t, r->step);
            seq_auto(t, r->step);
            TRACE('T', t, r->step * 1000000u + (s->note[0] ? (uint32_t)s->note[0] * 1000u : 0u) + (s->note[1] ? 900u : 0u) + (s->gate >= GATE_TIE ? 1u : 0u));
            r->play = s->note[0] && !(s->ratch & STEP_OFF) && (s->chance >= 100u || xorshift(&rnd_state) % 100u < s->chance);
            if (wrap && (k->ts & (TM_FINE - 1u))) {   /* (TIME: START inside the step – what lies before it is not played) */
                r->cnt += (r->dur * (k->ts & (TM_FINE - 1u))) >> 7;
                if (s->nudge < (k->ts & (TM_FINE - 1u))) r->play = 0;
            }
            r->wait = NO_WAIT;
            r->on_at = 0;
            if (r->play && s->nudge) {          /* later in the step: the old notes go (a late one at its end), the new ones when it is time */
                if (!carry) seq_release_t(t);
                r->on_at = (r->dur * s->nudge) >> 7;
                r->wait = r->on_at;
            } else if (r->tie)                  /* a tie: the notes the step plays again hold */
                seq_tie_into(t, s, k->transp, r->play);
            else {
                if (r->play) carry = 0;
                if (!carry) seq_release_t(t);
                if (r->play) seq_sound(t, s, k->transp);
            }
            r->hold_end = carry;
            r->rat_n = (s->ratch & 3u) + 1u;
            r->sub = (r->dur - r->on_at) / r->rat_n;
            r->rat_i = 0;
            for (uint32_t q = 0; q < SEQ_CHORD; q++) r->roll_i[q] = 0;
            r->tie = r->play && s->gate >= GATE_TIE && !s->nudge;
            {                                   /* the length: of a whole step (a ratchet's share), also for a late note */
                uint32_t g = r->dur / r->rat_n;
                r->gate_at = s->gate >= GATE_TIE ? 0xFFFFFFFFu : (uint32_t)clampi((int32_t)((g * (s->gate + 1u)) >> 7), CTL, (int32_t)(g > CTL ? g : CTL));
            }
            ui_seq_ticks++;
        } else {
            s = seq_step(t, r->step < len ? r->step : 0u);
            if (r->wait != NO_WAIT && r->cnt >= r->wait) { r->wait = NO_WAIT; r->hold_end = 0; seq_release_t(t); seq_sound(t, s, k->transp); }   /* a nudged step: now */
            else if (r->wait == NO_WAIT && r->play && r->rat_n > 1u && r->sub) {   /* a ratchet: the next hit */
                uint32_t i = (r->cnt - r->on_at) / r->sub;
                if (i > r->rat_i && i < r->rat_n) { r->rat_i = i; seq_release_t(t); seq_sound(t, s, k->transp); }
            }
            if (r->wait == NO_WAIT && r->play && r->cnt >= r->on_at)   /* (BOOM 0.5) a note's own roll: it alone, again */
                for (uint32_t q = 0; q < SEQ_CHORD && s->note[q]; q++) {
                    uint32_t rn = ROLL_N[s->nr[q] % 6u], sub, j, m;
                    if (rn < 2u) continue;
                    sub = (r->dur - r->on_at) / rn; if (!sub) continue;
                    j = (r->cnt - r->on_at) / sub;
                    if (j <= r->roll_i[q] || j >= rn) continue;
                    r->roll_i[q] = (uint8_t)j;
                    m = seq_note(s, q, k->transp);
                    if (seq_live(t, m) || ctl_rec_mute >> t & 1u) continue;
                    voice_trigger(t, m, s->nv[q] ? s->nv[q] : s->vel ? s->vel : 100u);
                }
        }
        if (r->hold_end && r->cnt >= r->hold_end) { r->hold_end = 0; seq_release_t(t); }   /* a late note from the step before: its end */
        if (r->nsnd && !r->hold_end && r->wait == NO_WAIT && r->gate_at != 0xFFFFFFFFu && r->cnt >= r->on_at && r->cnt - r->on_at - r->rat_i * r->sub >= r->gate_at)
            seq_release_t(t);
        r->cnt += CTL;
        ui_seq_dur[t] = r->dur;
        ui_seq_pos[t] = r->step;
        ui_seq_frac[t] = r->dur ? r->cnt * 256u / r->dur : 0u;
    }
    snap_write();
}
