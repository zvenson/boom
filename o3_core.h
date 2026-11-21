/* SPDX-License-Identifier: GPL-3.0-only */
/* BOOM (OberVave 3's frame) – the state without the panel: settings, programs, the four tracks' sounds, the mixer, the master. */

static uint32_t o3_ms;                          /* milliseconds, counted by the audio */
#define NFXP (P_REV_MIX - P_CHO_RATE + 1u)     /* the effects' parameters */
#define SET_VERSION 9u                          /* (BOOM 0.2: 7 – the settings 0.2 wrote from misread sound files had MASTER at 0) */
static struct {                                 /* = OberVave 2.0's settings, field for field */
    uint8_t version, prog, tempo, midi_ch;      /* midi_ch 0 = omni, else 1..16: track A's channel, B-D the next three */
    int8_t tune, oct;
    uint8_t pat, click;                         /* pat: the sequencer pattern; click: CLICK_* */
    uint8_t tprog[NTRK];                        /* each track's program */
    uint8_t mix_lvl[NTRK];                      /* the mixer: level 0..127 (100 = unity) */
    uint8_t mix_cho[NTRK], mix_dly[NTRK], mix_rev[NTRK];   /* the sends into chorus, delay, reverb 0..127 */
    int8_t mix_pan[NTRK];                       /* -64..64 */
    uint8_t mix_mute, mix_solo, rec_q, sync_int;   /* mute, solo: bit t; rec_q: (2.0's one quantise, now rec_qt); sync_int: 1 = SYNC
                                                   INTERNAL (a clock / DAW from outside ignored), 0 = AUTO (2.0's pad byte: 0) */
    uint8_t fx[NFXP], fx_ok;                    /* the effects: the song's, not a sound's; ok: FX_OK */
    uint8_t rec_qt[NTRK], rec_qt_ok;            /* REC's quantise per track; ok: RQT_OK */
    uint8_t mix_drv[NTRK], dv_type, dv_amt, dv_tone, dv_mix, dv_ok;   /* the drive send per track, the drive; ok: DV_OK */
    uint8_t m_vol, m_dj, m_drive, m_comp, m_ok; /* MASTER: volume (100 = unity), DJ filter (64 = off), drive, comp; ok: M_OK */
    uint8_t midi_out, mo_ok;                    /* 3.1 (SDK 9): MIDI OUT – 1 = tracks A-D play out on the global channel and the next three
                                                   (OMNI: 1-4), clock out while we lead; ok: MO_OK */
    uint8_t au_type, au_ok;                     /* 3.7 audio tracks: bit t = track t is one; ok: AU_OK */
    uint8_t au_loop[NTRK], au_st[NTRK], au_src[NTRK], au_mon[NTRK], au_inm[2];   /* LOOP / ONCE, STEREO, source, monitor; IN 1/2: LINE MIC HI-Z */
    uint32_t au_len[NTRK];                      /* each one's length in frames (its take is a WAV on the card: o3_audio.h) */
    uint8_t au_dub[NTRK];                       /* REC: 0 REPLACE, 1 OVERDUB */
    char song[16];                              /* 3.7: the song's name (FILES: SAVE SONG), "" = none yet */
    uint8_t au_gain[2];                         /* 3.8.1: IN 1 / IN 2 gain in dB, 0..40 (an older file: 0) */
    uint8_t sc_key, sc_scale, sc_chord, sc_pads, sc_ok;   /* 3.10: KEY 0..11, SCALE, PLAY (1 = CHORDS), PADS (1 = WHITE); ok: SC_OK (o3_scale.h) */
    uint8_t trk_own, trk_daw, midi_auto, io_ok; /* BOOM 0.4 (DAW): bit t = track t has its own output (OUT 3/4 + 2t) · bit t = the DAW plays track t
                                                   (its steps are silent) · 1 = MIDI CH AUTO (OMNI on the device, 1-6 = A-F in a DAW); ok: IO_OK */
    uint8_t mfx[NMFX], mfx_ok;
    uint16_t smc[8][17]; uint8_t smc_n[8], smc_ok;
    uint8_t trk_mch[8], mch_ok;
    uint8_t eq_lo[8], eq_mid[8], eq_mf[8], eq_hi[8], eq_cut[8], eq_ok;
    uint8_t smp_thr[8], thr_ok;
    uint8_t au_autosave, as_ok;                 /* (0.31, Sven: "nix auto save") 0: a take stays in memory until SAVE ▸ AUDIO names it (as an MPC); 1: every take written at once; ok: AS_OK */                 /* (0.31, Sven: "keine Aussteuerung, kein Threshold") a SAMPLER's threshold: 0 OFF, 1..54 = -54..-1 dBFS; ok: THR_OK */   /* (0.31, Sven: "im Mixer auch einen EQ") per track: LOW / MID / HIGH 64 = 0 dB (+-15),
                                                 * MID's FREQ 0..127 = 200 Hz .. 5 kHz, LOW CUT 0 OFF · 1 40 · 2 80 · 3 150 Hz; ok: EQ_OK */                 /* (0.24, Sven) MIDI IN per track: 0 = SELECTED (plays while chosen), 1..16 = always that channel; ok: MCH_OK */
   /* (0.21) a SAMPLER's cuts moved by hand: each as 1/65536 of its take; smc_n 0 = the automatic ones; ok: SMC_OK */                  /* BOOM: MASTER FX (mixer row 2) – FILTER 0..128 (64 off), CRUSH, ECHO 0..127, REPEAT (never saved on); ok: MFX_OK */
} set;
#define MFX_OK 0x4Fu
#define SMC_OK 0x5Cu
#define MCH_OK 0x3Du
#define EQ_OK 0x5Eu
#define THR_OK 0x7Bu
#define AS_OK 0x4Au
static uint8_t midi_last_ch;                    /* (0.24) the channel the last MIDI note came on (shown when MIDI IN is turned) */
#define IO_OK 0x10u
#define AU_OK 0xA7u
#define MO_OK 0x6Eu
#define FX_OK 0x5Au
#define RQT_OK 0xA5u
#define M_OK 0x3Cu
#define DV_OK 0x5Du
enum { CLICK_OFF, CLICK_REC, CLICK_ON, CLICK_COUNT, NCLICK };
enum { RQ_OFF, RQ_32, RQ_16, RQ_8, RQ_4, NRQ };
static const uint8_t TPROG_DEF[NTRK] = {0, PROG_BASS0, PROG_MEL0, PROG_SYNTH0 + 42u, 1u, PROG_SYNTH0 + 33u, PROG_STR0, PROG_DX0};   /* (0.11: G SOLINA, H the first DX voice) */   /* TRAP KIT, CLEAN 808, DARK BELL, WARM PAD, DARK KIT, MINOR FALL */
static const uint32_t TRK_RGB[NTRK] = {0x00E5FF, 0xFF8A1E, 0x2BFFB4, 0xFF2E97, 0xC77DFF, 0xFFE14D, 0x5C8DFF, 0xFF5A5A};   /* (BOOM: neon) */

/* the user's programs: 2 x 50 (2.0's program sectors), all in RAM */
#define NPSECT 4u
#define PS_N 50u
#define PS_TAG (0x50420000u | PATCH_BYTES)      /* (BOOM) "BP" + the patch's size: programs saved with another layout are passed over */
typedef struct { uint32_t tag; uint8_t used[PS_N], pad[2]; patch_t p[PS_N]; } psect_t;
static psect_t psect[NPSECT];

static uint32_t trk_prog[NTRK], trk_edited[NTRK];
static uint32_t sel;                            /* the selected track: keys and pads play it, the pages edit it */
/* (BOOM 0.3, Sven 14.11.) six tracks on four pad columns and four encoders: a WINDOW of four that scrolls with the chosen
 * track (encoder A). The pads' rows 1-2, the MIXER's columns, X1-X4 (the encoder pushes) and TRACKS' rows are those four. */
static uint32_t trk_win;                        /* the window's first track, 0..NTRK-4 */
static uint32_t bank0(void)
{
    if (sel < trk_win) trk_win = sel;
    else if (sel > trk_win + 3u) trk_win = sel - 3u;
    if (trk_win > NTRK - 4u) trk_win = NTRK - 4u;
    return trk_win;
}
static uint32_t col_trk(uint32_t c) { uint32_t t = bank0() + (c & 3u); return t < NTRK ? t : NTRK; }   /* NTRK: no track there */

/* (0.7) the effects as saved before the MODEs: a MIX value in the mode's place becomes the mode that sounds as it did */
static void fx_norm(uint8_t *fx)
{
    if (fx[P_DLY_MIX - P_CHO_RATE] >= NDM) fx[P_DLY_MIX - P_CHO_RATE] = DM_ANALOG;
    if (fx[P_REV_MIX - P_CHO_RATE] >= NRM) fx[P_REV_MIX - P_CHO_RATE] = RM_PLATE;
}
static void eq_defaults(void) { for (uint32_t q = 0; q < 8u; q++) { set.eq_lo[q] = set.eq_mid[q] = set.eq_hi[q] = 64; set.eq_mf[q] = 64; set.eq_cut[q] = 0; } set.eq_ok = EQ_OK; }   /* (0.31) flat */
static void settings_defaults(void)
{
    uint32_t t;
    memset(&set, 0, sizeof set);
    set.version = SET_VERSION; set.tempo = 120; set.click = CLICK_REC;
    for (t = 0; t < NTRK; t++) {
        set.tprog[t] = TPROG_DEF[t]; set.mix_lvl[t] = 100; set.rec_qt[t] = RQ_16;
    }
    for (t = 0; t < NFXP; t++) set.fx[t] = (uint8_t)PDEF[P_CHO_RATE + t].def;   /* the effects full, the sends closed */
    set.fx[P_CHO_MIX - P_CHO_RATE] = 127;       /* (0.7: the delay's and the reverb's returns are always full; their MODEs: the table's defaults) */
    set.fx_ok = FX_OK; set.rec_q = RQ_16; set.rec_qt_ok = RQT_OK;
    set.dv_type = DV_OVERDRIVE; set.dv_amt = 60; set.dv_tone = 90; set.dv_mix = 127; set.dv_ok = DV_OK;
    set.m_vol = 100; set.m_dj = 64; set.m_ok = M_OK;
    for (t = 0; t < NTRK; t++) { set.au_loop[t] = 1; set.au_st[t] = 1; set.au_mon[t] = 1; }   /* (3.7: LOOP, STEREO, AUTO) */
    set.au_ok = AU_OK;
    set.sc_ok = 0x5Cu;                          /* (C MAJOR, NOTES, the pads in the scale) */
    set.mfx[0] = 64; set.mfx[1] = set.mfx[2] = set.mfx[3] = 0; set.mfx_ok = MFX_OK;
    memset(set.smc, 0, sizeof set.smc); memset(set.smc_n, 0, sizeof set.smc_n); set.smc_ok = SMC_OK;
    set.trk_own = set.trk_daw = 0; set.midi_auto = 1; set.io_ok = IO_OK;
    memset(set.trk_mch, 0, sizeof set.trk_mch); set.mch_ok = MCH_OK;
    eq_defaults();
    memset(set.smp_thr, 0, sizeof set.smp_thr); set.thr_ok = THR_OK;
    set.au_autosave = 0; set.as_ok = AS_OK;
}

static void prog_get(uint32_t n, patch_t *p)    /* program n: the user's, else the factory's (1-16 kits, 17-32 basses, 33-99 MEL) */
{
    n %= NPROG;
    if (psect[n / PS_N].tag == PS_TAG && psect[n / PS_N].used[n % PS_N]) *p = psect[n / PS_N].p[n % PS_N];
    else {
        patch_from_factory(p, factory_of(n), prog_type_dflt(n)); p->v[P_TYPE] = (uint8_t)prog_type_dflt(n);   /* (a factory slot: its range's kind) */
        if (n >= PROG_DX0 && n - PROG_DX0 < DX_NSYNTH) {   /* (0.6) DX: sloopDX's factory voices, their names */
            static const char *const DN[DX_NSYNTH] = {DX_SYNTH_NAME_LIST};
            uint32_t i; const char *nm = DN[n - PROG_DX0];
            memcpy(p->dx, DX_SYNTH[n - PROG_DX0], DX_VOICE);
            for (i = 0; i < PATCH_NAME; i++) p->v[NPARAM + i] = ' ';
            for (i = 0; i < PATCH_NAME && nm[i]; i++) p->v[NPARAM + i] = (uint8_t)nm[i];
        }
    }
}
static void prog_name(uint32_t n, char *out)    /* its 10-character name, trailing spaces cut */
{
    patch_t p; int i;
    prog_get(n, &p);
    for (i = 0; i < (int)PATCH_NAME; i++) out[i] = (char)p.v[NPARAM + i];
    out[PATCH_NAME] = 0;
    for (i = (int)PATCH_NAME - 1; i >= 0 && out[i] == ' '; i--) out[i] = 0;
}
static uint8_t pk_kind[NPROG]; static int pk_valid;   /* (0.31) each slot's kind, remembered – 200 slots read anew only after a save or a load */
static void pk_fill(void) { patch_t p; for (uint32_t m = 0; m < NPROG; m++) { prog_get(m, &p); pk_kind[m] = (uint8_t)(p.v[P_TYPE] % NTYPE); } pk_valid = 1; }
static uint32_t prog_type(uint32_t n) { if (!pk_valid) pk_fill(); return pk_kind[n % NPROG]; }   /* the kind of sound in slot n */
/* (0.31, Sven 20.11.: "Presets durcheinander – Bass startet nicht bei 1, E-Piano bei 193") the number a sound SHOWS: its place
 * among the slots of its kind (BASS 1-16, E-PIANO 1-8), not the slot in the store of all kinds. MIDI program changes keep the slot. */
static uint32_t prog_num(uint32_t n)
{
    uint32_t ty = prog_type(n), c = 1;
    n %= NPROG;
    for (uint32_t m = 0; m < n; m++) if (pk_kind[m] == ty) c++;
    return c;
}
static uint32_t trk_type(uint32_t t) { return live_p[t % NTRK].v[P_TYPE] % NTYPE; }   /* the kind of sound track t plays */
static int midi_trk_any(void) { for (uint32_t t = 0; t < NTRK; t++) if (trk_type(t) == T_MIDI) return 1; return 0; }   /* (0.30) a MIDI track: the clock and START / STOP go out */
static uint32_t prog_step_type(uint32_t from, int dir, uint32_t ty)   /* the next slot of that kind, round the 99 (none: stays) */
{
    uint32_t n = from % NPROG;
    for (uint32_t i = 0; i < NPROG; i++) { n = (uint32_t)(((int32_t)n + dir + (int32_t)NPROG) % (int32_t)NPROG); if (prog_type(n) == ty) return n; }
    return from % NPROG;
}

/* the effects are global (as a DAW's return tracks): they live in track A's patch and in set.fx */
static void fx_keep(patch_t *p) { uint32_t i; if (set.fx_ok == FX_OK) for (i = 0; i < NFXP; i++) p->v[P_CHO_RATE + i] = set.fx[i]; }
static void trk_load(uint32_t t, uint32_t n)    /* track t: program n */
{
    patch_t p;
    t %= NTRK; n %= NPROG;
    prog_get(n, &p);
    if (!t) fx_keep(&p);
    live_p[t] = p;
    trk_prog[t] = n; trk_edited[t] = 0;
    set.tprog[t] = (uint8_t)n;
    if (!t) set.prog = (uint8_t)n;
}
static void trk_set_type(uint32_t t, uint32_t ty)   /* TYPE: the track plays that kind of sound – its first slot (the engine ends the old voices) */
{
    t %= NTRK; ty %= NTYPE;
    if (trk_type(t) == ty) return;
    trk_load(t, type_prog0(ty));
}
static int is_a_only(uint32_t p) { return p >= P_CHO_RATE && p <= P_REV_MIX; }
static patch_t *ed_patch(uint32_t p) { return is_a_only(p) ? &live_p[0] : &live_p[sel % NTRK]; }
static void set_param(uint32_t p, int32_t v)
{
    patch_t *d = ed_patch(p);
    v = clampi(v, 0, PDEF[p].max);
    if (d->v[p] == v) return;
    d->v[p] = (uint8_t)v;
    trk_edited[(uint32_t)(d - live_p) % NTRK] = 1;
}

/* the panel's notes go to the selected track's sound; a release goes where its note began */
static uint8_t note_part[128];
static void play_note(uint32_t on, uint32_t note, uint32_t vel)
{
    note &= 127u;
    if (on) note_part[note] = (uint8_t)(sel % NTRK);
    if (on && trk_type(sel) == T_SMP && live_p[sel].v[P_SM_CHOP] % 3u && sm_n[sel % 8u] > 1u)   /* (0.23) a slice played: its cut is the chosen one (CHOP: knob 2 moves it) */
        { if (note >= 48u && note - 48u < sm_n[sel % 8u]) sm_cur[sel % 8u] = note - 48u; }
    note_event_p(note_part[note], on, note, vel);
}

static void mix_apply(void)                     /* the settings into the engine; mute and solo as one mask */
{
    for (uint32_t q = 0; q < NTRK; q++) { ctl_eq[q][0] = set.eq_lo[q]; ctl_eq[q][1] = set.eq_mid[q]; ctl_eq[q][2] = set.eq_mf[q]; ctl_eq[q][3] = set.eq_hi[q]; ctl_eq[q][4] = set.eq_cut[q] & 3u; }
    uint32_t t, solo = set.mix_solo & ((1u << NTRK) - 1u);
    for (t = 0; t < NTRK; t++) {
        ctl_mix_lvl[t] = set.mix_lvl[t] > 127u ? 127u : set.mix_lvl[t];
        ctl_mix_pan[t] = (int8_t)clampi(set.mix_pan[t], -64, 64);
        ctl_mix_cho[t] = set.mix_cho[t] > 127u ? 127u : set.mix_cho[t];
        ctl_mix_dly[t] = set.mix_dly[t] > 127u ? 127u : set.mix_dly[t];
        ctl_mix_rev[t] = set.mix_rev[t] > 127u ? 127u : set.mix_rev[t];
        ctl_mix_drv[t] = set.mix_drv[t] > 127u ? 127u : set.mix_drv[t];
    }
    ctl_mix_off = solo ? ~solo & ((1u << NTRK) - 1u) : set.mix_mute & ((1u << NTRK) - 1u);
    ctl_dv_type = set.dv_type; ctl_dv_amt = set.dv_amt; ctl_dv_tone = set.dv_tone; ctl_dv_mix = set.dv_mix;
    ctl_tempo = set.tempo; ctl_tune8 = set.tune * 256 / 100;
    vibe_tempo_set((float)set.tempo);           /* (SDK 9) the clock out follows our tempo */
    ctl_m_vol = set.m_vol; ctl_m_dj = set.m_dj; ctl_m_drive = set.m_drive; ctl_m_comp = set.m_comp;
    for (t = 0; t < NMFX; t++) ctl_mfx[t] = set.mfx[t];
    ctl_trk_own = set.trk_own; ctl_trk_daw = set.trk_daw;
}

/* (BOOM, Sven 14.11.2026: "an 2. Stelle eine Art Master-FX-Sektion … Punch-in-Effekte … die ganze Summe") MASTER FX –
 * four effects on the whole sum, turned in and out from the mixer's FX row, in this order:
 *   REPEAT  a beat repeat: turned on, the sum of the next 1/4 .. 1/64 (in the tempo) is caught and looped until it is
 *           turned off; the length can change while it loops (the loop stays on its downbeat); a short crossfade at the seam
 *   CRUSH   a bit crusher with its sample rate dropping (16 → 4 bits, 48 k → 3 k), a one-pole after it so it stays warm
 *   DUB     a tape echo played with one knob: input, feedback (into self-oscillation) and time (1/16 .. 1/4, gliding)
 *   FILTER  a resonant 24 dB filter: left low-pass down to 60 Hz, right high-pass up to 8 kHz, the middle is off
 * (the MASTER row's DJ filter, drive and comp come after them, as before) */
static float lvl_ins[VIBE_IO_MAX];              /* (0.29) each input's peak (falls like the output's) – an armed track's meter */
static float o3_sr = 48000.0f, lvl_l, lvl_r, lvl_in;   /* lvl_in: IN 1 / IN 2 after GAIN (3.8.1, the header's input bar) */
#define MFX_REP_N 73728u                        /* a quarter at 40 BPM (48 kHz) + the seam */
#define MFX_ECHO_N 49152u
static float rep_buf[2][MFX_REP_N], echo_buf[2][MFX_ECHO_N];
static const uint8_t REP_DIV[6] = {0, 1, 2, 4, 8, 16};   /* OFF, 1/4, 1/8, 1/16, 1/32, 1/64 */
static const char *const REP_NAME[6] = {"OFF", "1/4", "1/8", "1/16", "1/32", "1/64"};
/* (0.4) nothing subnormal leaves or stays in a loop: a value under 1e-15 is 0 (a DAW checks it, and they cost time) */
static inline float tiny0(float x) { return x > 1e-15f || x < -1e-15f ? x : 0.0f; }
static void master_fx(float *l, float *r, int n)
{
    static uint32_t rep_on, rep_len, rep_pos, rep_fill, echo_w;
    static float cr_hl, cr_hr, cr_lpl, cr_lpr, ec_lpl, ec_lpr, ec_g;
    static uint32_t cr_n;
    static vd_svf fa[2], fb[2]; static vd_svf_c fc; static int fkey = -1;
    static float fwet, fx_x = -1.0f, crw; static int fmode_last = 1; static uint32_t rep_tail;   /* (0.17: fades – switched hard, they clicked) */
    uint32_t rep = ctl_mfx[3] % 6u, q = quarter_len();
    float crush = (float)ctl_mfx[1] / 127.0f, echo = (float)ctl_mfx[2] / 127.0f;
    int dj = (int)ctl_mfx[0] - 64, fmode = dj <= -2 ? 1 : dj >= 2 ? 2 : 0;
    /* REPEAT (0.31, Sven 21.11.: "knackt manchmal") – every jump crossfades over REP_XF samples: into the loop (the seam
     * fades from what comes live), a new length while it loops (from where the old one was), a length longer than what
     * was caught (the loop fades into the sum while it catches again), out of it. No zero crossings: a stereo sum's two
     * sides seldom cross together and a bass's next one can be 10 ms away */
#define REP_XF 64u                              /* (1.3 ms – Sven: "so kurz wie möglich"; shorter clicks again on a bass) */
    {
        static uint32_t xf_n, xf_pos, xf_len, xf_live;
        if (rep && !rep_on) { rep_on = 1; rep_fill = 0; rep_pos = 0; rep_tail = 0; xf_n = 0; rep_len = 0; }
        if (!rep && rep_on) { rep_on = 0; if (rep_len && rep_fill >= rep_len) rep_tail = REP_XF; xf_n = 0; }
        if (rep_tail) {                                /* (0.17) out of the loop over REP_XF samples, not at once */
            for (int i = 0; i < n && rep_tail; i++, rep_tail--) {
                if (rep_pos >= rep_len) rep_pos = 0;
                float f = (float)rep_tail / (float)REP_XF;
                l[i] = l[i] * (1.0f - f) + rep_buf[0][rep_pos] * f; r[i] = r[i] * (1.0f - f) + rep_buf[1][rep_pos] * f; rep_pos++;
            }
        }
        if (rep_on) {
            uint32_t len = q / REP_DIV[rep]; if (len > MFX_REP_N - 256u) len = MFX_REP_N - 256u; if (len < 64u) len = 64u;
            if (!rep_len) rep_len = len;
            else if (len != rep_len) {
                if (rep_fill >= rep_len) {             /* it loops: from where the old loop is, crossfaded */
                    xf_live = 0; xf_pos = rep_pos; xf_len = rep_len; xf_n = REP_XF;
                    if (len <= rep_fill) rep_pos %= len;   /* (shorter, or longer and caught: on its downbeat) */
                    else { rep_fill = 0; rep_pos = 0; }    /* (longer than caught: it catches again, the loop fading into the sum) */
                } else if (len <= rep_fill) { xf_live = 1; xf_n = REP_XF; rep_pos = 0; }   /* (catching, the new one already caught: from the sum into it) */
                rep_len = len;
            }
            for (int i = 0; i < n; i++) {
                float il = l[i], ir = r[i], a, b;
                if (rep_fill < rep_len + REP_XF && rep_fill < MFX_REP_N) { rep_buf[0][rep_fill] = il; rep_buf[1][rep_fill] = ir; rep_fill++; }   /* (the loop, then its seam) */
                if (rep_fill <= rep_len) { a = il; b = ir; }   /* (catching: the sum passes) */
                else {
                    if (rep_pos >= rep_len) rep_pos = 0;
                    a = rep_buf[0][rep_pos]; b = rep_buf[1][rep_pos];
                    if (rep_pos < REP_XF && rep_len + rep_pos < rep_fill) {   /* the seam: from what came after the loop's end (the first time: the sum as it comes) */
                        float f = (float)rep_pos / (float)REP_XF;
                        a = a * f + rep_buf[0][rep_len + rep_pos] * (1.0f - f); b = b * f + rep_buf[1][rep_len + rep_pos] * (1.0f - f);
                    }
                    rep_pos++;
                }
                if (xf_n) {                            /* a jump: from the old stream */
                    float f = (float)xf_n / (float)REP_XF, oa, ob;
                    if (xf_live) { oa = il; ob = ir; }
                    else { if (xf_pos >= xf_len) xf_pos = 0; oa = rep_buf[0][xf_pos]; ob = rep_buf[1][xf_pos]; xf_pos++; }
                    a = a * (1.0f - f) + oa * f; b = b * (1.0f - f) + ob * f; xf_n--;
                }
                l[i] = a; r[i] = b;
            }
        }
    }
    /* CRUSH */
    if (crush > 0.0f || crw > 0.0f) {
        float bits = 16.0f - 12.0f * crush, steps = vd_exp2(bits - 1.0f), inv = 1.0f / steps;
        uint32_t hold = 1u + (uint32_t)(crush * crush * 15.0f);
        float k = 1.0f - 0.85f * crush;         /* the one-pole after it: 0.15 .. 1 */
        for (int i = 0; i < n; i++) {
            if (!cr_n--) { cr_n = hold - 1u; cr_hl = (float)(int32_t)(l[i] * steps + (l[i] < 0.0f ? -0.5f : 0.5f)) * inv; cr_hr = (float)(int32_t)(r[i] * steps + (r[i] < 0.0f ? -0.5f : 0.5f)) * inv; }
            cr_lpl = tiny0(cr_lpl + (cr_hl - cr_lpl) * k); cr_lpr = tiny0(cr_lpr + (cr_hr - cr_lpr) * k);
            crw += ((crush > 0.0f ? 1.0f : 0.0f) - crw) * 0.004f; if (crush <= 0.0f && crw < 1e-4f) crw = 0.0f;
            l[i] += (cr_lpl - l[i]) * crw; r[i] += (cr_lpr - r[i]) * crw;
        }
    } else cr_n = 0;
    /* DUB (Sven 14.11.: "was man lustiger drehen kann") – a tape echo where the one knob plays everything: turned up, more of
     * the sum goes in, the feedback climbs (past ~85 % it runs away into a held, saturated wash) and the time walks
     * 1/16 → 1/8 → 1/8. → 1/4 – the tape glides to each new time, so every turn swoops the pitch of what is ringing.
     * Turned to OFF the input closes and the tail rings out on its own. */
    {
        static float ec_len = 12000.0f, ec_hp[2];
        static const float DIVQ[4] = {0.25f, 0.5f, 0.75f, 1.0f};   /* of a quarter */
        float d = echo, target, fb, send, kl = vd_coef(0.00006f, o3_sr);
        uint32_t di = d < 0.25f ? 0u : d < 0.5f ? 1u : d < 0.75f ? 2u : 3u;
        target = (float)q * DIVQ[di]; if (target > (float)(MFX_ECHO_N - 4u)) target = (float)(MFX_ECHO_N - 4u);
        fb = d > 0.0f ? 0.35f + 0.67f * d : 0.5f;    /* (OFF: the tail fades at 0.5) */
        send = d > 0.0f ? (d < 0.3f ? d / 0.3f : 1.0f) : 0.0f;
        for (int i = 0; i < n; i++) {
            ec_len += (target - ec_len) * 0.00012f;   /* (the tape: ~170 ms to a new time – the swoop) */
            float rp = (float)echo_w - ec_len; while (rp < 0.0f) rp += (float)MFX_ECHO_N;
            uint32_t r0 = (uint32_t)rp % MFX_ECHO_N, r1 = (r0 + 1u) % MFX_ECHO_N; float fr = rp - (float)(uint32_t)rp;
            float wl = echo_buf[0][r0] + (echo_buf[0][r1] - echo_buf[0][r0]) * fr, wr = echo_buf[1][r0] + (echo_buf[1][r1] - echo_buf[1][r0]) * fr;
            ec_g += (send - ec_g) * 0.002f;
            ec_lpl += (wr - ec_lpl) * kl; ec_lpr += (wl - ec_lpr) * kl;   /* ping-pong, darker each round */
            ec_hp[0] += (ec_lpl - ec_hp[0]) * 0.004f; ec_hp[1] += (ec_lpr - ec_hp[1]) * 0.004f;   /* (no rumble builds up) */
            float inl = (l[i] + r[i]) * 0.5f * ec_g + vd_tanh((ec_lpl - ec_hp[0]) * fb * 1.1f), inr = vd_tanh((ec_lpr - ec_hp[1]) * fb * 1.1f);
            echo_buf[0][echo_w] = tiny0(inl); echo_buf[1][echo_w] = tiny0(inr);
            ec_lpl = tiny0(ec_lpl); ec_lpr = tiny0(ec_lpr); ec_hp[0] = tiny0(ec_hp[0]); ec_hp[1] = tiny0(ec_hp[1]); ec_g = tiny0(ec_g);
            echo_w = (echo_w + 1u) % MFX_ECHO_N;
            l[i] += wl * 0.7f; r[i] += wr * 0.7f;
        }
    }
    /* FILTER */
    if (fmode) { if (fmode != fmode_last) fx_x = -1.0f; fmode_last = fmode; }
    if (fmode || fwet > 0.0f) {
        float xt = fmode ? (float)(dj < 0 ? -dj : dj) / 64.0f : 0.0f;
        if (fx_x < 0.0f) fx_x = xt;
        fx_x += (xt - fx_x) * 0.25f;                   /* (the cutoff glides a step at a time: ~10 ms) */
        if ((int)(fx_x * 4096.0f) != fkey) {
            vd_svf_set(&fc, fmode_last == 1 ? 16000.0f * vd_pow(60.0f / 16000.0f, fx_x) : 30.0f * vd_pow(8000.0f / 30.0f, fx_x), 2.2f, o3_sr);
            fkey = (int)(fx_x * 4096.0f);
        }
        for (int i = 0; i < n; i++) {
            vd_svf_out a1 = vd_svf_tick(&fa[0], &fc, l[i]), b1 = vd_svf_tick(&fa[1], &fc, r[i]);
            vd_svf_out a2 = vd_svf_tick(&fb[0], &fc, fmode_last == 1 ? a1.lp : a1.hp), b2 = vd_svf_tick(&fb[1], &fc, fmode_last == 1 ? b1.lp : b1.hp);
            fwet += ((fmode ? 1.0f : 0.0f) - fwet) * 0.004f; if (!fmode && fwet < 1e-4f) fwet = 0.0f;
            l[i] += ((fmode_last == 1 ? a2.lp : a2.hp) - l[i]) * fwet; r[i] += ((fmode_last == 1 ? b2.lp : b2.hp) - r[i]) * fwet;
        }
    } else { fkey = -1; fx_x = -1.0f; fa[0].ic1 = fa[0].ic2 = fa[1].ic1 = fa[1].ic2 = fb[0].ic1 = fb[0].ic2 = fb[1].ic1 = fb[1].ic2 = 0.0f; }
}
static void master(float *l, float *r, int n)
{
    master_fx(l, r, n);
    static vd_svf fl, fr; static vd_svf_c c; static int dj_was = -1, mode_last = 1; static float env, fwet;
    int dj = (int)ctl_m_dj - 64, mode = dj <= -2 ? 1 : dj >= 2 ? 2 : 0;
    if (mode) mode_last = mode;                          /* (0.17) the filter fades in and out (~5 ms): switched hard, it clicked */
    if (dj != dj_was) {
        float x = (float)(dj < 0 ? -dj : dj) / 64.0f;
        vd_svf_set(&c, mode == 1 ? 18000.0f * vd_pow(60.0f / 18000.0f, x) : 20.0f * vd_pow(6000.0f / 20.0f, x), 1.1f, o3_sr);
        dj_was = dj;
    }
    float d = (float)ctl_m_drive / 127.0f, pre = 1.0f + 8.0f * d, post = (1.0f + d) / pre;
    float cm = (float)ctl_m_comp / 127.0f, thr = vd_pow(10.0f, (-4.0f - 20.0f * cm) / 20.0f), ex = 1.0f - 1.0f / (1.0f + 5.0f * cm);
    float mk = cm > 0.0f ? vd_pow(1.0f / thr, ex * 0.45f) : 1.0f, vol = (float)ctl_m_vol / 100.0f;
    float att = 1.0f - vd_coef(0.003f, o3_sr), rel = 1.0f - vd_coef(0.15f, o3_sr), pl = 0, pr = 0;
    vol *= vol;
    for (int i = 0; i < n; i++) {
        float a = l[i], b = r[i];
        fwet += ((mode ? 1.0f : 0.0f) - fwet) * 0.004f; if (!mode && fwet < 1e-4f) fwet = 0.0f;
        if (fwet > 0.0f) {
            vd_svf_out ol = vd_svf_tick(&fl, &c, a), orr = vd_svf_tick(&fr, &c, b);
            float fa = mode_last == 1 ? ol.lp : ol.hp, fb = mode_last == 1 ? orr.lp : orr.hp;
            a += (fa - a) * fwet; b += (fb - b) * fwet;
        } else { fl.ic1 = fl.ic2 = fr.ic1 = fr.ic2 = 0.0f; }
        if (d > 0.0f) { a = vd_tanh(a * pre) * post; b = vd_tanh(b * pre) * post; }
        if (cm > 0.0f) {
            float p = vd_maxf(vd_absf(a), vd_absf(b));
            env = tiny0(env + (p - env) * (p > env ? att : rel));
            float g = env > thr ? vd_pow(thr / env, ex) : 1.0f;
            a *= g * mk; b *= g * mk;
        }
        a *= vol; b *= vol;
        float ma = vd_absf(a), mb = vd_absf(b);
        if (ma > 0.8f) a = (a < 0 ? -1.0f : 1.0f) * (0.8f + 0.2f * vd_tanh((ma - 0.8f) / 0.2f));
        if (mb > 0.8f) b = (b < 0 ? -1.0f : 1.0f) * (0.8f + 0.2f * vd_tanh((mb - 0.8f) / 0.2f));
        l[i] = tiny0(a); r[i] = tiny0(b);
        if (ma > pl) pl = ma;
        if (mb > pr) pr = mb;
    }
    {   /* (the meters fall as they did once a 192-frame block, now that MASTER runs per piece) */
        float fall = 1.0f - 0.05f * (float)n / 192.0f;
        lvl_l = pl > lvl_l ? pl : lvl_l * fall;
        lvl_r = pr > lvl_r ? pr : lvl_r * fall;
    }
}

/* ---- text: a parameter's value as the pages show it (<= 7 characters) */
static uint32_t slen(const char *s) { uint32_t n = 0; while (s[n]) n++; return n; }
static void scat(char *d, const char *s) { d += slen(d); while ((*d++ = *s++)) ; }
static void fmt_u(char *s, uint32_t v) { char b[12]; uint32_t i = 0, j = 0; do { b[i++] = (char)('0' + v % 10u); v /= 10u; } while (v); while (i) s[j++] = b[--i]; s[j] = 0; }
static void fmt_ms(char *s, uint32_t ms_x10)
{
    char t[4];
    if (ms_x10 < 1000u) { fmt_u(s, ms_x10 / 10u); scat(s, "."); fmt_u(t, ms_x10 % 10u); scat(s, t); scat(s, "ms"); }
    else if (ms_x10 < 10000u) { fmt_u(s, ms_x10 / 10u); scat(s, "ms"); }
    else { fmt_u(s, ms_x10 / 10000u); scat(s, "."); fmt_u(t, (ms_x10 / 1000u) % 10u); scat(s, t); scat(s, "s"); }
}
static void param_value(char *s, uint32_t p, uint32_t v)
{
    char t[8];
    s[0] = 0;
    switch (PDEF[p].kind) {
    case K_BIP: if (v < 64u) { scat(s, "-"); fmt_u(t, 64u - v); } else { scat(s, v > 64u ? "+" : ""); fmt_u(t, v - 64u); } scat(s, t); break;
    case K_ONOFF: scat(s, v ? "ON" : "OFF"); break;
    case K_TYPE: scat(s, TYPE_NAME[v % NTYPE]); break;
    case K_DTYPE: scat(s, BDT_NAME[v % NBDT]); break;
    case K_BGATE: scat(s, BGATE_NAME[v & 1u]); break;
    case K_KSRC: scat(s, KSRC_NAME[v % 3u]); break;
    case K_SMTRK: scat(s, SMTRK_NAME[v % 9u]); break;
    case K_SMN: fmt_u(s, v + 2u); break;   /* (0.21) 2..16 */
    case K_SMPOS: fmt_u(s, (v * 100u + 63u) / 127u); scat(s, "%"); break;
    case K_SMPM: scat(s, SMPM_NAME[v % 3u]); break;
    case K_SMPITCH: if (v != 24u) scat(s, v > 24u ? "+" : "-"); fmt_u(s + slen(s), v > 24u ? v - 24u : 24u - v); break;
    case K_SMFINE: { int32_t c = ((int32_t)v - 64) * 50 / 64; if (c) scat(s, c > 0 ? "+" : "-"); fmt_u(s + slen(s), (uint32_t)(c < 0 ? -c : c)); scat(s, " ct"); break; }   /* (o3_pages.h shows it as time when there is a sample) */
    case K_SMCHOP: scat(s, SMCHOP_NAME[v % 3u]); break;
    case K_MICH: scat(s, "CH "); fmt_u(s + 3, v % 16u + 1u); break;   /* (0.30) */
    case K_MIPROG: if (!v) scat(s, "NONE"); else fmt_u(s, v); break;
    case K_MIVEL: if (!v) scat(s, "AS PLAYED"); else fmt_u(s, v); break;
    case K_SMQ: scat(s, SMQ_NAME[v & 3u]); break;
    case K_SLIDE: scat(s, BSLIDE_NAME[v & 1u]); break;
    case K_ALGO: fmt_u(s, v % NALGO + 1u); break;
    case K_RATIO: { uint32_t r = RATIO_X100[v % NRATIO]; scat(s, "x"); fmt_u(s + 1, r / 100u); scat(s, "."); fmt_u(t, (r / 10u) % 10u); scat(s, t); fmt_u(t, r % 10u); scat(s, t); } break;
    case K_LVL: fmt_u(s, v); break;
    case K_MODE: scat(s, MODE_NAME[v & 3u]); break;
    case K_WAVE: scat(s, WAVE_NAME[v & 3u]); break;
    case K_FTYPE: scat(s, FTYPE_NAME[v % NFTYPE]); break;
    case K_TO: scat(s, TO_NAME[v % 3u]); break;
    case K_DMODE: scat(s, DM_NAME[v < NDM ? v : 0u]); break;
    case K_RMODE: scat(s, RM_NAME[v < NRM ? v : RM_PLATE]); break;
    case K_DXALGO: if (!v) scat(s, "VOICE"); else fmt_u(s, v); break;
    case K_DXFB: if (!v) scat(s, "VOICE"); else fmt_u(s, v - 1u); break;
    case K_MORPH: if (v == 64u) scat(s, "NOTCH"); else { scat(s, v < 64u ? "LP " : "HP "); fmt_u(t, v < 64u ? 64u - v : v - 64u); scat(s, t); } break;
    case K_LSHAPE: scat(s, LSHAPE_NAME[v % 6u]); break;
    case K_LPHASE: scat(s, LPHASE_NAME[v % 3u]); break;
    case K_ARP: scat(s, ARP_NAME[v % NARPMODE]); break;
    case K_DIV: scat(s, DIV_NAME[v % NDIV]); break;
    case K_DDIV: scat(s, DDIV_NAME[v % NDDIV]); break;
    case K_SEMI: if (v < 24u) { scat(s, "-"); fmt_u(t, 24u - v); } else { scat(s, v > 24u ? "+" : ""); fmt_u(t, v - 24u); } scat(s, t); break;
    case K_MS:
        if (p == P_GLIDE) { if (!v) scat(s, "OFF"); else { fmt_u(s, GLIDE_MS[v & 127u]); scat(s, "ms"); } }
        else fmt_ms(s, TIME_MS_X10[v & 127u]);
        break;
    case K_HZ: { uint32_t h = LFO_HZ_X100[v & 127u]; fmt_u(s, h / 100u); scat(s, "."); fmt_u(t, (h / 10u) % 10u); scat(s, t); scat(s, "Hz"); } break;
    case K_PCT: fmt_u(s, (v * 100u + 63u) / 127u); scat(s, "%"); break;
    case K_CUT: { uint32_t hz = CUT_HZ[(v + 8u) % 137u]; if (hz < 10000u) { fmt_u(s, hz); scat(s, "Hz"); } else { fmt_u(s, hz / 1000u); scat(s, "."); fmt_u(t, (hz / 100u) % 10u); scat(s, t); scat(s, "k"); } } break;
    case K_OCT: fmt_u(s, v); scat(s, " OCT"); break;
    default: fmt_u(s, v);
    }
}

/* the files of pattern n: "blob<n>" (its steps), "psnd<n>" (its sounds and mix) – 2.0's names, 0..7 as they were */
static const char *pat_fname(const char *base, uint32_t n) { static char b[4][12]; static uint32_t k; char *o = b[k++ & 3u]; o[0] = 0; scat(o, base); fmt_u(o + slen(o), n); return o; }
#define PS_FILE_N(n) pat_fname("psnd", n)
#define PB_FILE_N(n) pat_fname("blob", n)
