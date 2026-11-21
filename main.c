/* BOOM – a mini DAW on the VIBE (Sven 14.11.2026; it began as "trap beats on the VIBE"): 8 synth drums, a playable 808 bass, 4-operator FM keys – on OberVave 3's frame (GPL-3.0,
 * see LICENSE): the controls, the sequencer, the mixer, the effects, the audio tracks and the files are OberVave's,
 * key for key (firmware/obervave, 3.9); the sounds are BOOM's own.
 *
 *   bm_params.h   the parameter table, the pages per kind of sound (DRUMS · BASS · MEL), the drums' names and notes
 *   bm_patches.h  the factory sounds: 8 kits, 8 basses, 16 FM sounds
 *   bm_engine.h   the engine: drums, the 808, FM – in OberVave's frame (note ring, arps, mixer, click, scope)
 *   obv_*.h       OberVave's DSP helpers, effects, step sequencer, audio tracks
 *   o3_*.h        OberVave's panel: core, drawing, storage, pages, mixer, sequencer views, files, DAW, menu
 *   main.c        the SDK's four functions: audio, controls, MIDI, drawing
 *
 * Controls = OberVave 3.9: F1 MIXER, F2 TRACKS, F3 LOAD, F4 SAVE, F5 STORE (SHIFT: HOLD), F6 GLOBAL (SHIFT: UNDO); the
 * colour keys SOUND ENV LFO ARP FX SEQ; A track, B sound (DRUMS: the kit), DATA pattern; pads: row 1 start/stop, row 2 the
 * track, rows 3-4 notes – on a DRUMS track the eight drums (KICK SNARE CLAP RIM · CH OH SNAP PERC). TYPE (SOUND's last
 * page): DRUMS · BASS · MEL · AUDIO per track and pattern. */
#include "vibe.h"
#include "vibe_gfx.h"
#include "vibe_dsp.h"

void *memset(void *d, int c, size_t n);
void *memcpy(void *d, const void *s, size_t n);
int memcmp(const void *a, const void *b, size_t n);
void *memmove(void *d, const void *s, size_t n);
static uint32_t obv_fs = 44100u, fx_fs = 44100u;   /* the device's rate; the effects' lines are sized up to 48 kHz */
#define FS obv_fs
#define HALF_FRAMES 256u                        /* the engine's largest block */
#define NBLOB 64u                               /* (3.7: 64 patterns – 2.0's FM-1 flash had room for 8) */
#include "obv_dsp.h"
#include "bm_params.h"
#include "obv_fx.h"
#include "bm_engine.h"
#include "obv_seq.h"
#include "obv_audio.h"
#include "bm_patches.h"
#include "o3_core.h"
#include "o3_draw.h"
#include "o3_store.h"
#include "o3_splash.h"
#include "o3_pages.h"
#include "o3_mixer.h"
#include "o3_seq.h"
#include "o3_scale.h"
#include "o3_seqview.h"
#include "o3_audio.h"
#include "o3_files.h"
#include "o3_daw.h"

static int shift, octave, menu, f5_down, f5_used;
static int k_lock;                              /* (12.11.2026, Sven: "als Capslock") RETURN tapped: K1-K6 play the scale, as SHIFT held */
static uint32_t ret_down, ret_t, ret_used;      /* (3.8) RETURN held: UNDO (ENCODER 1 back / forward; 2 s: one back) */
#include "o3_menu.h"
#include "bm_sampler.h"                         /* (0.19-0.22) the SAMPLER: after the audio, the files and the menu */

/* Protokoll 5 (6.2): the audio may run beside events and the display (VIBE: its own realtime thread, 4 ms buffer).
 * What OberVave shares between the two: single-writer controls (ctl_* / ui_*), the note and arp rings, the pattern,
 * sound and mix swap (pat_q, snd_q, mx_q – flag after a RING_PUBLISH, the ISR swaps on the downbeat), the tracks'
 * snapshot for REC (a seqlock). The audio flow never logs, saves, draws or touches the panel's state. */
VIBE_PARALLEL_AUDIO

static float enc_acc[VE_COUNT];
static int enc_steps(int e, float v)            /* whole detents from fractional turns (the mouse, a slow knob) */
{
    enc_acc[e] += v;
    int s = (int)enc_acc[e]; enc_acc[e] -= (float)s;
    return s;
}
static int coarse(int s)                        /* DATA for jumps (pattern, bar): one per turn, at most every 150 ms */
{
    static uint32_t t0;
    if (!s || o3_ms - t0 < 150u) return 0;
    t0 = o3_ms; return s > 0 ? 1 : -1;
}

/* ---- audio: the engine renders straight into the runtime's block (in pieces of up to 64 frames, a multiple of its
 * control block) – nothing rendered ahead, so a key changes the very next block (lowest latency). Only a block that is
 * no multiple of 16 frames goes through a small queue. After each piece REC takes the arp's notes (on time). */
#define EB 64u
static int32_t eng_out[2u * EB];
static float eng_l[EB], eng_r[EB];
static float eng_x[NTRK][2][EB];                /* (BOOM 0.4) the tracks on their own outputs, beside eng_l / eng_r */
static float *x_out[NTRK][2];                   /* this block's OUT 3/4 + 2t (NULL: the main mix has the track) */
static uint32_t eng_pos, eng_end, ms_acc;      /* eng_l/r[eng_pos..eng_end): rendered, not yet played */
static uint32_t eng_frames;                     /* frames rendered (the MIDI clock is measured in them) */
static uint32_t clk_last;
static const float *in_blk[VIBE_IO_MAX];        /* (3.7) this block's IN 1 / IN 2 (NULL: off); (0.29) IN 3..16 of an interface */
static void piece_inputs(int at, int avail)     /* the inputs for a piece from frame `at` of the block (avail frames there) */
{
    for (int k = 0; k < VIBE_IO_MAX; k++) au_in[k] = in_blk[k] ? in_blk[k] + at : 0;
    au_in_n = avail > 0 ? (uint32_t)avail : 0u;
}
static void engine_piece(uint32_t n)            /* n frames (a multiple of 16) into eng_l / eng_r */
{
    memset(eng_out, 0, 2u * n * sizeof eng_out[0]);
    engine_audio(eng_out, n);
    for (uint32_t j = 0; j < n; j++) { eng_l[j] = (float)eng_out[2u * j] * (1.0f / 32768.0f); eng_r[j] = (float)eng_out[2u * j + 1u] * (1.0f / 32768.0f); }
    for (uint32_t t = 0; t < NTRK; t++) {       /* (BOOM 0.4) own outputs: after the fader, before MASTER */
        if (!x_out[t][0]) continue;
        if (trk_x_used >> t & 1u) for (uint32_t j = 0; j < n; j++) for (uint32_t c = 0; c < 2u; c++) {   /* (a soft knee above 0.8, as MASTER's: it never clips hard) */
            float x = (float)trk_x[t][2u * j + c] * (1.0f / 32768.0f), a = vd_absf(x);
            eng_x[t][c][j] = a > 0.8f ? (x < 0.0f ? -1.0f : 1.0f) * (0.8f + 0.2f * vd_tanh((a - 0.8f) / 0.2f)) : x;
        }
        else for (uint32_t j = 0; j < n; j++) eng_x[t][0][j] = eng_x[t][1][j] = 0.0f;
    }
    master(eng_l, eng_r, (int)n);               /* (3.7: per piece – RESAMPLE takes what is heard, to the frame) */
    au_capture(eng_l, eng_r, n);
    for (ms_acc += n * 1000u; ms_acc >= obv_fs; ms_acc -= obv_fs) o3_ms++;
    eng_frames += n;
    if (ctl_ext_quarter && o3_ms - clk_last > 1000u) ctl_ext_quarter = 0;   /* the clock stopped: our own tempo again */
}
/* (SDK 8) the DAW's song position: when it starts playing, or its place is not where the last block said it would be (a
 * loop going round, a jump), the sequencer chases it. `ahead`: frames already made (the queue) – they come first. */
static double daw_expect = -1.0;
static void daw_follow(uint32_t ahead, uint32_t frames)
{
    const vibe_transport_info *tp = vibe_transport();
    double per;
    if (!tp->has || !tp->playing || tp->bpm <= 0.0f || set.sync_int) { daw_expect = -1.0; return; }
    per = (double)tp->bpm / 60.0 / (double)obv_fs;
    if (tp->beat >= 0.0 && (daw_expect < 0.0 || tp->beat - daw_expect > 0.02 || daw_expect - tp->beat > 0.02)) {
        seq_chase_beat = tp->beat + ahead * per; seq_chase_bar = tp->bar_start;
        if (seq_chase_beat - seq_chase_bar >= song_e8() / 2.0) seq_chase_bar += song_e8() / 2.0;   /* (a bar: track A's meter) */
        if (daw_expect < 0.0) { ctl_seq_chase = 1; ctl_seq_restart = 1; ctl_seq_run = 1; ctl_arp_restart = 1; }   /* it starts */
        else if (ctl_seq_run) ctl_seq_chase = 1;  /* it jumped (stopped here by hand: stays stopped) */
    }
    daw_expect = tp->beat + frames * per;
    if (tp->loop && tp->loop_end > tp->loop_start && tp->beat < tp->loop_end && tp->loop_start >= 0.0) {
        double k = (tp->loop_end - tp->beat) / per; /* frames to the loop's end: inside this block → to the frame (whatever */
        if (k < (double)frames) {               /* the host's blocks), and the next block is expected back at its start */
            double bq = song_e8() / 2.0; uint64_t bars = (uint64_t)(tp->loop_start / bq);
            seq_loop_at = eng_t - ahead + (uint32_t)(k + 0.5); seq_loop_on = ctl_seq_run;
            seq_loop_beat = tp->loop_start; seq_loop_bar = (double)bars * bq;
            daw_expect = tp->loop_start + ((double)frames - k) * per;
        }
    }
}
/* 3.1 (SDK 9): MIDI OUT – the tracks' notes on the global channel and the next three (OMNI: 1-4); nothing while it is off */
static uint32_t mo_ch(void);
static void mo_note(uint32_t part, uint32_t note, uint32_t vel, uint32_t on)
{
    if (!set.midi_out) return;
    uint32_t ch = (mo_ch() - 1u + part) & 15u;
    vibe_midi_send_at((uint8_t)((on ? 0x90u : 0x80u) | ch), (uint8_t)(note & 127u), (uint8_t)(on ? (vel ? vel & 127u : 1u) : 0u), (int)(mo_base + mo_tick));
}
static void mo_all_off(uint32_t part)           /* a panic, a track's notes off: All Notes Off on its channel */
{
    if (!set.midi_out) return;
    vibe_midi_send_at((uint8_t)(0xB0u | ((mo_ch() - 1u + part) & 15u)), 123u, 0u, (int)(mo_base + mo_tick));
}
/* (0.30, Sven 15.11.) MIDI tracks: an external device – their notes on their own channel, with MIDI OUT (GLOBAL) on or off.
 * A note remembers the channel it went out on (the channel turned while it sounds: its NOTE OFF still finds it). The
 * program and the four CCs go out when they change (a knob, automation, a sound loaded); the program also when the track
 * becomes MIDI or its channel changes. CCs are not sent on their own at the start – the device keeps its sound. */
static uint8_t mt_on[NPART][128];               /* the channel + 1 a note sounds on, 0: off */
static uint8_t mt_sent[NPART][7];               /* [0] 1 = known · [1] channel · [2] program · [3..6] the CC values */
static uint8_t mt_pg_want[NPART]; static uint32_t mt_pg_t[NPART];   /* a program being turned to: sent once it rests 1/4 s */
static void mt_send(uint32_t a, uint32_t b, uint32_t c) { vibe_midi_send_at((uint8_t)a, (uint8_t)(b & 127u), (uint8_t)(c & 127u), (int)(mo_base + mo_tick)); }
static void mt_note(uint32_t part, uint32_t note, uint32_t vel, uint32_t on)
{
    const uint8_t *pv;
    uint32_t ch;
    part %= NPART; note &= 127u; pv = pt[part].p->v; ch = pv[P_MI_CH] % 16u;
    if (on) {
        if (mt_on[part][note]) mt_send(0x80u | (mt_on[part][note] - 1u), note, 0);   /* (again while it sounds: off first) */
        mt_send(0x90u | ch, note, pv[P_MI_VEL] ? pv[P_MI_VEL] : vel ? vel : 1u);
        mt_on[part][note] = (uint8_t)(ch + 1u);
    } else if (mt_on[part][note]) { mt_send(0x80u | (mt_on[part][note] - 1u), note, 0); mt_on[part][note] = 0; }
}
static void mt_all_off(uint32_t part)
{
    part %= NPART;
    for (uint32_t n = 0; n < 128u; n++) if (mt_on[part][n]) { mt_send(0x80u | (mt_on[part][n] - 1u), n, 0); mt_on[part][n] = 0; }
}
static void mt_block(void)
{
    for (uint32_t p = 0; p < NPART; p++) {
        const uint8_t *pv = pt[p].p->v;
        uint8_t *s = mt_sent[p];
        if (part_type(p) != T_MIDI) { if (s[0]) { mt_all_off(p); s[0] = 0; } continue; }
        uint32_t ch = pv[P_MI_CH] % 16u, pg = pv[P_MI_PROG] % 129u;
        if (!s[0] || s[1] != ch) {              /* new as MIDI, or another channel: its program there */
            if (pg) mt_send(0xC0u | ch, pg - 1u, 0);
            if (!s[0]) for (uint32_t i = 0; i < 4u; i++) s[3 + i] = pv[P_MI_CC1 + i];
            s[0] = 1; s[1] = (uint8_t)ch; s[2] = (uint8_t)pg;
        }
        if (s[2] != pg) {                       /* (turned: only where it comes to rest – a device loads no ten sounds on the way) */
            if (mt_pg_want[p] != pg) { mt_pg_want[p] = (uint8_t)pg; mt_pg_t[p] = eng_frames; }
            else if (eng_frames - mt_pg_t[p] > obv_fs / 4u) { s[2] = (uint8_t)pg; if (pg) mt_send(0xC0u | ch, pg - 1u, 0); }
        } else mt_pg_want[p] = (uint8_t)pg;
        for (uint32_t i = 0; i < 4u; i++) if (s[3 + i] != pv[P_MI_CC1 + i]) { s[3 + i] = pv[P_MI_CC1 + i]; mt_send(0xB0u | ch, pv[P_MI_CC1N + i] > 119u ? 119u : pv[P_MI_CC1N + i], s[3 + i]); }
    }
}
/* (BOOM 0.4) MIDI CH AUTO: in a DAW (it gives the song position and no MIDI clock comes in) the channels 1-6 are the tracks
 * A-F, whichever is chosen on the panel; on the device every channel plays the chosen track (OMNI) */
static int in_daw(void) { const vibe_transport_info *tp = vibe_transport(); return tp->has && !(clk_last && o3_ms - clk_last < 2000u); }
static uint32_t midi_ch_eff(void) { return set.midi_auto ? (in_daw() ? 1u : 0u) : set.midi_ch; }
static uint32_t mo_ch(void) { return midi_ch_eff() ? midi_ch_eff() : 1u; }   /* track A's channel as shown (1..16) */

void vibe_on_audio(const float *in, float *l, float *r, int n)
{
    int i = 0;
    in_blk[0] = in; in_blk[1] = vibe_input(1);
    {   /* (0.29, Sven 15.11.: the XR18) IN 3 .. IN 16, as many as the host has */
        int ni = vibe_input_count();
        for (int k = 2; k < VIBE_IO_MAX; k++) in_blk[k] = k < ni ? vibe_input(k) : 0;
    }
    {   /* (3.8.1, Sven: "runter gedreht ist es sehr leise") GAIN on IN 1 / IN 2, before the monitor and the take */
        static float gbuf[2][VIBE_MAX_BLOCK], gf[2] = {1.0f, 1.0f};
        static uint8_t gdb[2];
        for (int k = 0; k < 2; k++) {
            uint8_t db = set.au_gain[k] > 40u ? 40u : set.au_gain[k];
            if (db != gdb[k]) { gdb[k] = db; gf[k] = vd_pow(10.0f, (float)db / 20.0f); }
            if (!db || !in_blk[k] || n > (int)VIBE_MAX_BLOCK) continue;
            for (int j = 0; j < n; j++) gbuf[k][j] = in_blk[k][j] * gf[k];
            in_blk[k] = gbuf[k];
        }
        {   /* the input's peak for the header's bar (falls like the output's) */
            float pk = 0.0f;
            for (int k = 0; k < 2; k++) if (in_blk[k]) for (int j = 0; j < n; j++) { float a = vd_absf(in_blk[k][j]); if (a > pk) pk = a; }
            lvl_in = pk > lvl_in ? pk : lvl_in * (1.0f - 0.05f * (float)n / 192.0f);
            for (int k = 0; k < VIBE_IO_MAX; k++) {
                float p = 0.0f;
                if (in_blk[k]) for (int j = 0; j < n; j++) { float a = vd_absf(in_blk[k][j]); if (a > p) p = a; }
                lvl_ins[k] = p > lvl_ins[k] ? p : lvl_ins[k] * (1.0f - 0.05f * (float)n / 192.0f);
            }
        }
    }
    /* (3.1) the clock out: ours, while MIDI OUT is on and no clock comes in (a DAW that leads silences it in the SDK) */
    vibe_clock_out((set.midi_out || midi_trk_any()) && !(clk_last && o3_ms - clk_last < 2000u && ctl_ext_quarter));
    daw_sync();                                 /* (SDK 8) what changed in a sound: to the DAW */
    daw_follow((uint32_t)(eng_end - eng_pos), (uint32_t)n);   /* (SDK 8) the DAW starts or jumps: the steps where it is */
    ctl_host_outs = (uint32_t)vibe_output_count();
    for (uint32_t t = 0; t < NTRK; t++) {       /* (BOOM 0.4) a track on its own output: OUT 3/4 + 2t, when the host has them */
        int own = (set.trk_own >> t & 1u) && ctl_host_outs >= 4u + 2u * t;
        x_out[t][0] = own ? vibe_output((int)(2u + 2u * t)) : 0; x_out[t][1] = own ? vibe_output((int)(3u + 2u * t)) : 0;
        if (!x_out[t][1]) x_out[t][0] = 0;
    }
#define X_COPY(at, from) for (uint32_t t_ = 0; t_ < NTRK; t_++) if (x_out[t_][0]) { x_out[t_][0][at] = eng_x[t_][0][from]; x_out[t_][1][at] = eng_x[t_][1][from]; }
    while (i < n && eng_pos < eng_end) { l[i] = eng_l[eng_pos]; r[i] = eng_r[eng_pos]; X_COPY(i, eng_pos) eng_pos++; i++; }   /* (what a queue still holds) */
    while (n - i >= 16) {                       /* the direct way */
        uint32_t m = (uint32_t)(n - i) >= EB ? EB : (uint32_t)(n - i) & ~15u;
        mo_base = (uint32_t)i;
        piece_inputs(i, (int)m);
        engine_piece(m);
        for (uint32_t j = 0; j < m; j++) { l[i + j] = eng_l[j]; r[i + j] = eng_r[j]; X_COPY(i + (int)j, j) }
        i += (int)m;
    }
    if (i < n) {                                /* a remainder under 16: one piece, the rest kept for the next block */
        mo_base = (uint32_t)i;
        piece_inputs(i, n - i);                 /* (the inputs only to the block's end: the rest of the piece is ahead of them) */
        engine_piece(16u);
        eng_pos = 0; eng_end = 16u;
        while (i < n) { l[i] = eng_l[eng_pos]; r[i] = eng_r[eng_pos]; X_COPY(i, eng_pos) eng_pos++; i++; }
    }
}

static uint8_t cc_map[128];                     /* MIDI CC -> parameter + 1 (2.0's: fixed ones, the rest from CC 20 up) */
static void cc_map_init(void)
{
    uint32_t i, next = 20;
    for (i = 0; i < NPARAM; i++) if (PDEF[i].cc) cc_map[PDEF[i].cc] = (uint8_t)(i + 1u);
    for (i = 0; i < NPARAM; i++) {
        if (PDEF[i].cc) continue;
        while (next < 120u && (cc_map[next] || next == 64u)) next++;
        if (next < 120u) cc_map[next++] = (uint8_t)(i + 1u);
    }
}

void vibe_on_init(float sr)
{
    vibe_extras_own(1);                          /* (SDK 19 extras, 8.11.2026: X1-X4 – encoder pushes – are ours: MIXER mute, SEQUENCER start/stop) */
    uint32_t t;
    obv_fs = (uint32_t)(sr + 0.5f); fx_fs = obv_fs > 48000u ? 48000u : obv_fs; o3_sr = sr;
    vf_mode(VF_ON_HOLD);                         /* (SDK 10 FOCUS: DATA stays the sound / pattern; ENTER held 0.6 s = FOCUS) */
    obv_tables_init(sr);
    bm_tables_init(sr);
    engine_init();
    cc_map_init();
    store_boot();
    for (t = 0; t < NTRK; t++) trk_load(t, set.tprog[t]);
    mix_apply();
    daw_init(); daw_sync();                     /* (SDK 8) the DAW's parameters, at the sounds just loaded */
    daw_editor();                               /* (SDK 9) the Editor as OberVave wants it: cards, XY filter, curves */
    seq_boot();
    au_boot();                                  /* (3.7) the audio tracks: their settings, their takes from the card */
    scope_state = 1;
    splash_unpack();
    vibe_log("BOOM 0.4 (native VIBE, 8 tracks, DX7, STRINGS and E-PIANO kinds, drum grid, delay and reverb modes, tube amp insert, own outputs, DAW mode)");
}

static void leave_seq_edit(void) { step_held = -1; audition_off(); entry_reset(); }
static void go(uint32_t pg) { if (pg != page) leave_seq_edit(); page = pg; menu = 0; files_on = 0; snd_x = SX_NONE; }
static void prog_step(int s)                    /* the selected track's program -/+ */
{
    trk_load(sel, prog_step_type(trk_prog[sel], s > 0 ? 1 : -1, trk_type(sel)));   /* (the next sound of the track's kind) */
    set_touch();
}
static int drums_sel(void) { return trk_type(sel) == T_DRUMS && !au_is(sel); }   /* the chosen track plays drums: pads and keys are the eight */
static int smp_chop_sel(void) { return trk_type(sel) == T_SMP && !au_is(sel) && live_p[sel].v[P_SM_CHOP] % 3u; }   /* (0.24) a sampler on its cuts: pads and keys are the slices, in order (C3 = 1) */
static uint32_t pad_note(uint32_t pad)          /* pads 9-16 = 2.0's white keys 9-16 from F3: G4 .. G5; DRUMS: the eight drums */
{
    static const uint8_t W[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
    if (drums_sel()) return DRUM_NOTE[PAD_DRUM[pad % 8u]];
    if (smp_chop_sel()) return 48u + pad % 8u;   /* (0.24) row 3: slices 1-4, row 4: 5-8 */
    return (uint32_t)clampi(53 + 12 * set.oct + W[pad % 16u], 0, 127);
}
static uint8_t pad_sent[VIBE_PADS];             /* the note a held pad plays + 1 */
static uint8_t key_sent[VIBE_KEYS];

static void seq_key(void)                       /* SEQ: into the sequencer (TRACKS); there, the next view */
{
    if (!in_seq()) { seq_back = page; go(PG_SEQ); seq_view = SV_TRACKS; return; }
    menu = 0; leave_seq_edit();
    seq_view = (seq_view + 1u) % NSV;
    if (seq_view == SV_MORE) seq_view = SV_TRACK;   /* (STEP+ is SHIFT in STEPS) */
    if (seq_view == SV_SLOTS) slot_sel = pat_queue ? pat_queue - 1u : set.pat;
}
static void tracks_key(void)                    /* F2: TRACKS; there, back */
{
    if (in_seq() && seq_view == SV_TRACKS && !menu) { go(seq_back == PG_SEQ ? PG_PERFORM : seq_back); return; }
    if (!in_seq()) seq_back = page;
    go(PG_SEQ); seq_view = SV_TRACKS;
}

static void oct_step(int d)                     /* OCT -/+ (or SHIFT + F3/F4): the keys' and the MIDI keyboard's octave */
{
    static const char *const O[7] = {"OCT -3", "OCT -2", "OCT -1", "OCT 0", "OCT +1", "OCT +2", "OCT +3"};
    octave = clampi(octave + d, -3, 3);
    say(O[octave + 3]);
}

static void button(int id, int down)
{
    if (id == VB_SHIFT) { shift = down; seq_shift = down; return; }
    if (down && id >= VB_F1 && id <= VB_F6 && !shift && smp_page()) { smp_fkey(id - VB_F1); return; }   /* (0.22) the SAMPLER's own F keys */
    if (id >= VB_K1 && id <= VB_K6) {           /* (3.10) SHIFT + K1-K6: the scale's six degrees (or chords), o3_scale.h */
        uint32_t k = (uint32_t)(id - VB_K1);
        if (!down && k_sent[k][0]) { sc_stop(k_sent[k]); return; }
        if (down && (shift || k_lock) && !reset_ask) {
            sc_stop(k_sent[k]);
            if (drums_sel()) { k_sent[k][0] = (uint8_t)(DRUM_NOTE[k] + 1u); note_in(1, DRUM_NOTE[k], 100u); say(drum_name(live_p[sel].v, k)); }   /* (0.15) a kit: K1-K6 are its first six drums, not the scale */
            else if (smp_chop_sel()) { static char m[12]; k_sent[k][0] = (uint8_t)(48u + k + 1u); note_in(1, 48u + k, 100u); m[0] = 0; scat(m, "SLICE "); fmt_u(m + 6, k + 1u); say(m); }   /* (0.24) a sampler: slices 1-6 */
            else { sc_play(k, octave, 100u, k_sent[k]); sc_say(k); }
            return;
        }
    }
    if (id == VB_REC) { if (!reset_ask) rec_press(down, shift); return; }
    if (id == VB_RETURN) {                      /* tapped: the K keys' lock (PERFORM is HOME's); held: UNDO (3.8) */
        if (down) { ret_down = 1; ret_t = o3_ms; ret_used = 0; }
        else if (ret_down) { ret_down = 0; if (!ret_used && !menu) { k_lock = !k_lock; say(k_lock ? "K KEYS PLAY NOTES" : "K KEYS: PAGES"); } }
        return;
    }
    if (id == VB_F5) {                          /* SAVE: tapped asks / saves (in the sequencer: the pattern now); held + ENTER: the dice; SHIFT: HOLD */
        if (down) { f5_down = 1; f5_used = 0; if (shift) { hold ^= 1u << sel; ctl_hold = hold; f5_used = 1; say(hold >> sel & 1u ? "HOLD ON" : "HOLD OFF"); } }
        else {
            if (f5_down && !f5_used) {
                if (in_seq()) {                 /* the pattern: asks first (− / + of a controller pass over F5), tapped again: saved */
                    if (!pat_save_armed) { pat_save_armed = 1; save_t = o3_ms; }
                    else { pat_save_armed = 0; pats_dirty |= 1ull << set.pat; store_t = o3_ms - 2000u; say("PATTERN SAVED"); }
                }
                else save_tap();
            }
            f5_down = 0;
        }
        return;
    }
    if (!down) return;
    if (reset_ask) { reset_ask = 0; if (id == VB_ENTER || id == VB_DATA_PUSH) reset_all(); return; }   /* the question: ENTER = yes */
    if (id != VB_DATA_PUSH && id != VB_ENTER) save_armed = pat_save_armed = 0;
    if (id >= VB_X1 && id < VB_X1 + 4 && !menu) {   /* (8.11.2026, Sven) a device's extra buttons X1-X4 – the VIBE Box's encoder 1-4
                                                       pushes (SDK 19): MIXER = mute A-D (SHIFT: solo) · SEQUENCER = start / stop A-D */
        uint32_t t = col_trk((uint32_t)(id - VB_X1));
        if (t >= NTRK) return;
        if (rec_down) { rec_armed_pads = 1; au_arm_toggle(t); return; }   /* (0.29, Sven: the Box has no pads) REC held + a knob's push: ARM */
        if (page == PG_MIXER) {
            uint8_t *m = shift ? &set.mix_solo : &set.mix_mute; *m ^= (uint8_t)(1u << t); mix_apply(); set_touch();
            say_trk(shift ? (*m >> t & 1u ? " SOLO" : " SOLO OFF") : (*m >> t & 1u ? " MUTED" : " ON"), t);   /* (its own static text) */
        }
        else if (in_seq()) trk_launch(t);
        return;
    }
    switch (id) {
    case VB_K1 + CK_SOUND: if (in_seq() && SV_ENTRY(seq_view) && !menu && !au_is(sel)) seq_clear_cursor(); else sound_key(); break;
    case VB_K1 + CK_ENV: go(cycle_page(PG_FENV, 2)); break;
    case VB_K1 + CK_LFO: go(cycle_page(PG_LFO, 2)); break;
    case VB_K1 + CK_ARP: if (page == PG_ARP && !menu) { hold ^= 1u << sel; ctl_hold = hold; say(hold >> sel & 1u ? "HOLD ON" : "HOLD OFF"); } else go(PG_ARP); break;
    case VB_K1 + CK_FX: go(cycle_page(PG_CHORUS, 4)); break;
    case VB_K1 + CK_SEQ: seq_key(); break;
    case VB_F1: if (page == PG_MIXER && !menu) mix_row = (mix_row + 1u) % NMROW; else { go(PG_MIXER); mix_row = 0; } break;
    case VB_F2: tracks_key(); break;
    case VB_F3: if (shift) oct_step(-1); else if (!menu) files_open(0); break;   /* (3.9) LOAD · SAVE pages (PRG -/+ are encoder B's); */
    case VB_F4: if (shift) oct_step(1); else if (!menu) files_open(1); break;    /* SHIFT: OCT -/+ (12.11.2026, Sven: the VIBE Box has no OCT) */
    case VB_LEFT: prog_step(-1); break;
    case VB_RIGHT: prog_step(1); break;
    case VB_F6:                                 /* GLOBAL → OUTPUT → FILES (3.7) → GLOBAL; SHIFT: UNDO */
        if (shift) seq_undo();
        else if (files_on) go(PG_GLOBAL);
        else go(page == PG_GLOBAL ? PG_OUTPUT : page == PG_OUTPUT ? PG_SCALE : PG_GLOBAL);   /* (3.10: SCALE) */
        break;
    case VB_HOME: case VB_MENU: menu = !menu; break;   /* (3.11.2026, Protokoll 3.2: HOME = what every control does, everywhere) */
    case VB_ENTER: case VB_DATA_PUSH:
        if (rec_down) { rec_armed_pads = 1; au_arm_toggle(sel); break; }   /* (0.29) REC held + ENTER: the chosen track's ARM (E-H too) */
        if (files_on && !menu) { files_enter(); break; }   /* (3.7) FILES: do it */
        if (in_seq() && seq_view == SV_TRACKS && !menu && !f5_down) { au_type_toggle(sel); break; }
        if (page == PG_S1 && trk_type(sel) == T_SMP && !in_seq() && !menu && !f5_down) { if (shift) sm_cut_del(sel); else sm_cut_add(sel); break; }   /* (0.21) CHOP by hand */
        if (id == VB_DATA_PUSH && in_seq() && SV_ENTRY(seq_view) && !menu && !f5_down && !au_is(sel)) { step_toggle(); break; }   /* (0.32) DATA: the cursor's step on / off */
        if (in_seq() && seq_view == SV_STEPS && !menu && !f5_down && !au_is(sel) && !drum_grid()) { roll_fold = !roll_fold; say(roll_fold ? "FOLD: ONLY THE NOTES USED" : "ALL KEYS"); break; }   /* (0.16) the piano roll: FOLD */   /* (3.7) TRACKS: SYNTH <-> AUDIO */
        if (f5_down) { randomize(); f5_used = 1; save_armed = 0; say("NEW SOUND"); }
        else if (shift && page == PG_GLOBAL) reset_ask = 1;
        else if (page == PG_GLOBAL && !menu) {  /* SYNC: AUTO (a clock / the DAW leads when there is one) <-> INTERNAL */
            set.sync_int = !set.sync_int; set_touch();
            if (set.sync_int) { ctl_daw_quarter = 0; ctl_ext_quarter = 0; }
            say(set.sync_int ? "SYNC INTERNAL" : "SYNC AUTO");
        }
        break;
    case VB_OCT_DOWN: oct_step(-1); break;
    case VB_OCT_UP: oct_step(1); break;
    case VB_PLAY: play_press(); break;
    case VB_STOP: if (ctl_seq_run) play_press(); break;
    default: break;
    }
}

static void pad(uint32_t p, int down, float vel)
{
    if (!down) {
        if (in_seq() && SV_ENTRY(seq_view) && !au_is(sel)) { step_pad_up(p); return; }
        if (pad_ch[p][0]) { sc_stop(pad_ch[p]); pad_sent[p] = 0; return; }   /* (3.10: the scale's note / chord) */
        if (pad_sent[p]) { note_in(0, pad_sent[p] - 1u, 0); pad_sent[p] = 0; }
        return;
    }
    if (rec_down && p < 4u) { rec_armed_pads = 1; pad_sent[p] = 0; au_arm_toggle(bank0() + p); return; }   /* (0.29, Sven 15.11.) REC held + a pad of row 1:
                                                 * that track's ARM, on any page – and REC lets go without clearing anything */
    if (in_seq() && SV_ENTRY(seq_view) && !au_is(sel)) { step_pad_down(p, (uint32_t)(vel * 126.0f) + 1u); return; }
    if (in_seq() && SV_ENTRY(seq_view)) { if (p < 8u) perf_pad(p, shift); return; }   /* (an audio track in STEPS: rows 1-2 as in TRACKS) */   /* (an audio track: no steps – the rows as in TRACKS) */   /* STEPS: the 16 steps of the bar (a new one as hard as hit) */
    if (page == PG_MIXER) {                   /* row 1 start / stop – and that track is the chosen one at once (Sven 23.10.) */
        if (p < 4u) { uint32_t w = bank0(), t = w + p; if (t != sel) { sel = t; step_held = -1; audition_off(); entry_reset(); } perf_pad_at(p, w, shift); }
        else mixer_pad(p, shift);
        return;
    }
    /* (BOOM) TRACKS: rows 1-2 the four tracks in view, rows 3-4 play the chosen one – as the sound pages */
    if (p < 8u) { perf_pad(p, shift); return; } /* row 1 start / stop (REC: record), row 2 the track */
    if (!set.sc_pads && !drums_sel() && !smp_chop_sel()) { sc_stop(pad_ch[p]); sc_play(p - 8u, octave, (uint32_t)(vel * 126.0f) + 1u, pad_ch[p]); pad_sent[p] = pad_ch[p][0]; return; }   /* (3.10) the scale */
    pad_sent[p] = (uint8_t)(pad_note(p) + 1u);
    note_in(1, pad_sent[p] - 1u, (uint32_t)(vel * 126.0f) + 1u);
}

static void encoder(int id, float v)
{
    int s = enc_steps(id, v);
    if (!s) return;
    if (ret_down && id == VE_1) { ret_used = 1; undo_turn(s); return; }   /* (3.8) RETURN held + ENCODER 1: UNDO / REDO */
    if (files_on && (id <= VE_4 || id == VE_DATA)) { files_turn(id, s); return; }   /* (3.7) FILES */
    if (id <= VE_4) {
        if (page == PG_MIXER) mixer_turn(id, s);
        else if (in_seq()) seq_turn((uint32_t)id, s);
        else if (snd_view()) snd_turn(snd_view(), (uint32_t)id, s);   /* (3.7) AUDIO / TAKE / TYPE */
        else if (page == PG_SCALE) scale_turn(id, s);   /* (3.10) */
        else knob_turn(id, s);
    } else if (id == VE_A) {
        if (shift) {                            /* SHIFT + A: the tempo, everywhere (6.11.2026, Sven – SHIFT + DATA is the SDK's F-key frame since SDK 12) */
            static char msg[16];
            set.tempo = (uint8_t)clampi(set.tempo + s, 40, 240); mix_apply(); set_touch();
            msg[0] = 0; scat(msg, "TEMPO "); fmt_u(msg + slen(msg), set.tempo); say(msg);
        }
        else trk_select((uint32_t)clampi((int32_t)sel + s, 0, NTRK - 1));   /* the track, everywhere */
    } else if (id == VE_B) {
        prog_step(s);                           /* the track's preset, everywhere (6.11.2026: the note is encoder 2 in STEPS, TRANSP encoder 4 in TRACK) */
    } else if (id == VE_DATA) {
        if (save_armed) { save_to = prog_step_type(save_to, s > 0 ? 1 : -1, trk_type(sel)); save_t = o3_ms; }   /* (0.31) only the slots of the track's kind: a bass never lands between the kits */
        else if (page == PG_GLOBAL) {           /* (3.1) MIDI OUT on / off */
            uint32_t was = set.midi_out; set.midi_out = s > 0;
            if (was != set.midi_out) { set_touch(); say(set.midi_out ? "MIDI OUT ON" : "MIDI OUT OFF"); if (!set.midi_out) { ctl_panic = 1; } }
        }
        else if (in_seq() && SV_ENTRY(seq_view) && !au_is(sel) && !shift) step_cursor_turn(s);   /* (0.32) STEPS: DATA moves the cursor */
        else { int c = coarse(s); if (c) pat_go((uint32_t)(((int)(pat_queue ? pat_queue - 1u : set.pat) + c + (int)NPAT) % (int)NPAT)); }   /* the pattern, everywhere (6.11.2026) */
    }
}

/* MIDI as 2.0: the global channel (GLOBAL) = track A, the next three = B-D; OMNI: every channel plays the chosen track.
 * Notes, program changes, CCs (2.0's map), bend, sustain, volume; clock (the tempo follows), start / continue / stop */
static uint32_t clk_n, clk_t0;
static uint8_t midi_sent[128];                  /* a MIDI key's note as played (with the octave) + 1: its off goes there */
static void midi(uint32_t st, uint32_t d1, uint32_t d2)
{
    uint32_t ch = (st & 15u) + 1u, kind = st & 0xF0u;
    if (set.sync_int && (st == 0xF8u || st == 0xFAu || st == 0xFBu || st == 0xFCu)) return;   /* (SYNC: INTERNAL) */
    if (st == 0xF8u) {
        /* the tempo from 4 quarters (96 clocks) in rendered frames, rounded to 48 like our own (quarter_len); taken
         * only when it changes by more than 0.3 % – the clock's jitter (USB, the loop) must not wobble the steps */
        if (!clk_n || o3_ms - clk_last > 250u) { clk_n = 0; clk_t0 = eng_frames; }
        clk_last = o3_ms;
        if (++clk_n == 97u) {
            uint32_t q = ((eng_frames - clk_t0) / 4u + 24u) / 48u * 48u, d;
            if (q > obv_fs / 5u && q < obv_fs * 8u / 5u) {
                d = q > ctl_ext_quarter ? q - ctl_ext_quarter : ctl_ext_quarter - q;
                if (!ctl_ext_quarter || d * 1000u > 3u * q) ctl_ext_quarter = q;
            }
            clk_n = 1; clk_t0 = eng_frames;
        }
        return;
    }
    if (st == 0xFAu || st == 0xFBu) { ctl_arp_restart = 1; clk_n = 0; ctl_seq_restart = 1; ctl_seq_run = 1; return; }
    if (st == 0xFCu) { ctl_seq_run = 0; return; }
    if (st >= 0xF0u) return;
    if (kind == 0x90u && d2) midi_last_ch = (uint8_t)ch;
    {   /* (0.24, Sven) a track bound to this channel (MIDI IN on the track's TYPE row) takes it – whichever track is chosen */
        static uint8_t bsent[NTRK][128];        /* the bound track's sounding notes (+1): their offs go there */
        uint32_t t;
        for (t = 0; t < NTRK && set.trk_mch[t] != ch; t++) ;
        if (t < NTRK) {
            if (au_is(t) && (kind == 0x90u || kind == 0x80u)) return;
            if (kind == 0x90u && d2) {
                if (t == sel) note_in(1, d1, d2);   /* (chosen: as the keys – REC takes it, the views use it) */
                else note_event_p(t, 1, d1, d2);
                bsent[t][d1 & 127u] = (uint8_t)(t == sel ? 2u : 1u);
            } else if (kind == 0x80u || kind == 0x90u) {
                uint32_t w = bsent[t][d1 & 127u]; bsent[t][d1 & 127u] = 0;
                if (w == 2u && t == sel) note_in(0, d1, 0); else note_event_p(t, 0, d1, 0);
            }
            else if (kind == 0xE0u && t == sel) ctl_bend = (int32_t)(d1 | d2 << 7) - 8192;
            else if (kind == 0xC0u && d1 < NPROG) { trk_load(t, d1); set_touch(); }
            else if (kind == 0xB0u && cc_map[d1]) { uint32_t p = cc_map[d1] - 1u; if (!is_a_only(p)) live_p[t].v[p] = (uint8_t)(PDEF[p].max == 127 ? d2 : (d2 * PDEF[p].max + 63u) / 127u); }
            return;
        }
    }
    uint32_t mc = midi_ch_eff();
    if (ch == 16u && mc != 16u && (!mc || mc + NTRK <= 16u)) {   /* (BOOM 0.4) channel 16 controls: a note from C1 (36) = that pattern (on the bar), a program change too */
        if (kind == 0x90u && d2 && d1 >= 36u && d1 < 36u + NPAT) pat_go(d1 - 36u);
        else if (kind == 0xC0u && d1 < NPAT) pat_go(d1);
        return;
    }
    if (mc && ch >= mc && ch < mc + NTRK && ch - mc != sel) {   /* a track's own channel (not the chosen one: that goes the keys' way, below) */
        uint32_t t = ch - mc;
        if (au_is(t) && (kind == 0x90u || kind == 0x80u)) return;   /* (3.7: an audio track plays no notes) */
        if (kind == 0x90u && d2) note_event_p(t, 1, d1, d2);
        else if (kind == 0x80u || kind == 0x90u) note_event_p(t, 0, d1, 0);
        else if (kind == 0xC0u && d1 < NPROG) { trk_load(t, d1); set_touch(); }
        else if (kind == 0xB0u && cc_map[d1]) { uint32_t p = cc_map[d1] - 1u; if (!is_a_only(p)) live_p[t].v[p] = (uint8_t)(PDEF[p].max == 127 ? d2 : (d2 * PDEF[p].max + 63u) / 127u); }
        return;
    }
    if (mc && ch != mc + sel) return;
    if (kind == 0x90u && d2) {                  /* (12.11.2026) the octave moves a MIDI keyboard too – the VIBE Box has no keys */
        int n = drums_sel() || smp_chop_sel() ? (int)d1 : (int)d1 + 12 * octave;   /* (DRUMS: GM's drum notes, no octave) */
        if (n < 0 || n > 127) return;
        if (midi_sent[d1 & 127u]) note_in(0, midi_sent[d1 & 127u] - 1u, 0);
        midi_sent[d1 & 127u] = (uint8_t)(n + 1); note_in(1, (uint32_t)n, d2);
    } else if (kind == 0x80u || kind == 0x90u) { if (midi_sent[d1 & 127u]) { note_in(0, midi_sent[d1 & 127u] - 1u, 0); midi_sent[d1 & 127u] = 0; } }
    else if (kind == 0xE0u) ctl_bend = (int32_t)(d1 | d2 << 7) - 8192;
    else if (kind == 0xC0u && d1 < NPROG) { trk_load(sel, d1); set_touch(); }
    else if (kind == 0xB0u) {
        if (d1 == 7u) ctl_cc7 = d2;
        else if (d1 == 64u) ctl_sustain = d2 >= 64u;
        else if (d1 == 120u || d1 == 123u) ctl_panic = 1;
        else if (cc_map[d1]) { uint32_t p = cc_map[d1] - 1u; set_param(p, PDEF[p].max == 127 ? (int32_t)d2 : ((int32_t)d2 * PDEF[p].max + 63) / 127); auto_rec(p); }
    }
}

/* SDK 10 FOCUS (ENTER or DATA held): the area (the title), the area's pages (the chips), the track and the sound (the
 * header), the four knobs (as encoders 1-4), F1-F6 – everything with one knob and one button */
#define FO_AREA 900
#define FO_CHIP 800
static void focus_press(int id)
{
    if (id == FO_AREA) { int c = in_seq() ? CK_SEQ : key_lit(); c = c < 0 ? 0 : (c + 1) % 6; button(VB_K1 + c, 1); button(VB_K1 + c, 0); return; }
    if (id >= FO_CHIP && id < FO_CHIP + 8) {
        static const uint8_t F6P[3] = {PG_GLOBAL, PG_OUTPUT, PG_SCALE};
        const group_t *g = page_group(page); uint32_t i = (uint32_t)(id - FO_CHIP); go(g->key == 6 ? F6P[i % 3] : g->first + i);
    }
}
static void focus_items(void)                   /* (in the draw: where each one is shown) */
{
    vf_button(FO_AREA, 8, 4, 150, 26);
    vf_encoder(VE_A, 174, 4, 56, 26);           /* the track (A-D) */
    vf_encoder(VE_B, 236, 4, 160, 26);          /* the sound */
    if (page == PG_MIXER || in_seq()) { for (int i = 0; i < 4; i++) vf_encoder(VE_1 + i, colx(i) - COLW / 2 + 4, 40, COLW - 8, 236); }
    else if (page == PG_GLOBAL) { for (int i = 0; i < 4; i++) vf_encoder(VE_1 + i, CX0 + 2, 126 + i * 36, 300, 30); }
    else {
        const group_t *grp = page_group(page);
        uint32_t n = grp->key == 6 ? 3u : grp->count;
        if (n >= 2) { int w = (CW + 6) / (int)n; for (uint32_t i = 0; i < n; i++) vf_button(FO_CHIP + (int)i, CX0 + (int)i * w, 41, w - 6, 26); }
        int y = grp->count > 1 || grp->key == 6 ? 74 : 50;
        for (int i = 0; i < 4; i++) vf_encoder(VE_1 + i, colx(i) - COLW / 2 + 4, y - 2, COLW - 8, 128);
    }
    for (int i = 0; i < 6; i++) vf_key(VB_F1 + i, 40 + i * 80 - 37, 286, 74, 30);
}
void vibe_on_event(const vibe_event *e)
{
    { int id = -1, r = vf_event(e, &id); if (r == VF_PRESS) focus_press(id); if (r != VF_NO) return; }
    arp_rec_drain();                            /* (what the arp played so far is in before this key) */
    switch (e->type) {
    case VIBE_EV_KEY: {
        uint32_t k = e->id % VIBE_KEYS;
        if (e->down) {
            int n = drums_sel() ? (int)DRUM_NOTE[k % NDRUM] : smp_chop_sel() ? 48 + (int)k : 48 + (int)k + 12 * octave;   /* (0.24: slices – no octave) */   /* (DRUMS: every eighth key the same drum) */
            if (n < 0 || n > 127) break;
            key_sent[k] = (uint8_t)(n + 1);
            note_in(1, (uint32_t)n, (uint32_t)(e->value * 126.0f) + 1u);
        } else if (key_sent[k]) { note_in(0, key_sent[k] - 1u, 0); key_sent[k] = 0; }
        break; }
    case VIBE_EV_PAD: pad(e->id % VIBE_PADS, e->down, e->value); break;
    case VIBE_EV_TOUCH:                         /* (0.13, Sven: play the grid with the mouse; later a touch screen) a click into STEPS: that step on / off, as its pad */
        if (e->id == VIBE_TOUCH_DOWN && in_seq() && seq_view == SV_STEPS && !au_is(sel) && pat.trk[sel].len) {
            int x = (int)((uint32_t)e->value & 511u), y = (int)((uint32_t)e->value >> 9), s = -1;
            if (drum_grid()) {                  /* the drum grid: the row is the drum (o3_seqview.h: X0 54, 26 wide, rows of 14 from 70) */
                int d = (y - 70) / 14;
                if (y >= 70 && d < (int)NDRUM && x >= CX0 + 44) { s = (x - CX0 - 44) / 26; drum_row[sel] = (uint8_t)d; }
            } else if (y >= 70 && x >= CX0 + 44) {   /* (0.14) the piano roll: the row is the pitch (the rows as the view has them, from 70) */
                uint8_t rows[ROLL_ROWS]; int rh; uint32_t nr = roll_rows(sel, rows, &rh), r = (uint32_t)((y - 70) / rh);
                if (r < nr) { roll_pick(sel, rows[r]); s = (x - CX0 - 44) / 26; }
            }
            if (s >= 0 && s < (int)seq_pw(sel) && seq_pg[sel] * seq_pw(sel) + (uint32_t)s < pat.trk[sel].len) { step_pad_down((uint32_t)s, 100); step_pad_up((uint32_t)s); }
        }
        break;
    case VIBE_EV_BUTTON: if (splash_on && e->down) { splash_on = 0; break; } button(e->id, e->down); break;   /* (a button ends the start screen) */
    case VIBE_EV_ENCODER: encoder(e->id, e->value); break;
    case VIBE_EV_MIDI: midi(e->midi[0], e->midi[1], e->midi[2]); break;
    case VIBE_EV_PARAM: daw_from_host(e->id, e->value); break;   /* (SDK 8) the DAW's automation */
    case VIBE_EV_PEDAL: if (e->id == VIBE_PEDAL_SWITCH) ctl_sustain = e->down; break;
    case VIBE_EV_SAVE:                          /* (SDK 21) the DAW saves its project / a preset: the sounds, patterns and
                                                   settings into the files now, not 1.5 s later (a Logic preset kept an older sound) */
        store_force = 1; seq_poll(); store_poll(); store_force = 0;
        break;
    case VIBE_EV_TRANSPORT:                     /* (SDK 7) the DAW / a MIDI clock: its tempo exactly, play from the top / stop */
        if (set.sync_int) break;                /* (GLOBAL SYNC: INTERNAL) */
        if (e->id == VIBE_TRANSPORT_TEMPO) ctl_daw_quarter = e->value >= 20.0f && e->value <= 400.0f ? (uint32_t)(obv_fs * 60.0f / e->value + 0.5f) : 0u;
        else if (e->id == VIBE_TRANSPORT_PLAY) {
            if (e->down) { ctl_arp_restart = 1; ctl_seq_restart = 1; ctl_seq_run = 1; }
            else ctl_seq_run = 0;
        }
        break;
    default: break;
    }
}

/* ---- the display */
static const group_t *grp;
static void group_chips(void)                   /* the colour key's pages, the open one bright (F6: GLOBAL, OUTPUT) */
{
    static const uint8_t F6P[3] = {PG_GLOBAL, PG_OUTPUT, PG_SCALE};
    uint32_t n = grp->key == 6 ? 3u : grp->count;
    if (n < 2) return;
    int w = (CW + 6) / (int)n;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t pg = grp->key == 6 ? F6P[i] : grp->first + i, on = pg == page, col = grp->key < 6 ? KEY_RGB[grp->key] : 0xFF3D6E;
        vg_round(CX0 + (int)i * w, 41, w - 6, 26, 5, on ? col : CELL);
        vg_text(PG(pg)->title, CX0 + (int)i * w + (w - 6) / 2, 46, w > 100 ? VG_MED : VG_BOLD, on ? 0x000000 : DIM, VG_CENTER);
    }
}
static void sound_chips(void)                   /* (3.7) SOUND's pages and TYPE; an audio track: AUDIO, TAKE, TYPE */
{
    static const char *const AUN[4] = {"AUDIO", "TAKE", "FILTER", "TYPE"};
    uint32_t au = AU.type >> sel & 1u, ns = snd_count(), n = au ? 4u : ns + 1u, sx = snd_view();
    int w = (CW + 6) / (int)n;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t pg = i ? i - 1u : 0u;          /* (PERFORM, TYPE, then the kind's pages) */
        int on = au ? sx == i + 1u : i == 1u ? sx == SX_TYPE : !sx && page == PG_PERFORM + pg;
        vg_round(CX0 + (int)i * w, 41, w - 6, 26, 5, on ? KEY_RGB[CK_SOUND] : CELL);
        vg_text(au ? AUN[i] : i == 1u ? "TYPE" : PG(PG_PERFORM + pg)->title, CX0 + (int)i * w + (w - 6) / 2, 46, VG_SMALL, on ? 0x000000 : DIM, VG_CENTER);
    }
}
static void sound_tags(void);
static void scope_view(void);
static void env_graph(void)                     /* FILTER ENV / AMP ENV: the shape */
{
    const patch_t *pv = &live_p[sel];
    int a = PG(page)->p[0];                     /* (the page's first knob is the attack; DRUMS' LEVELS pages have no curve) */
    float x0 = 22, y0 = 270, h = 60, w = 436;
    if (a != P_FA && a != P_AA) { sound_tags(); scope_view(); return; }
    float ta = (float)pv->v[a] / 127.0f, td = (float)pv->v[a + 1] / 127.0f, s = (float)pv->v[a + 2] / 127.0f, tr = (float)pv->v[a + 3] / 127.0f;
    float tot = ta + td + 0.35f + tr + 0.01f, k = w / tot;
    float xa = x0 + ta * k, xd = xa + td * k, xs = xd + 0.35f * k, xr = xs + tr * k;
    uint32_t col = page == PG_FENV ? KEY_RGB[CK_ENV] : TRK_RGB[sel];
    vg_rect(CX0, 204, CW, 72, CELL);
    vg_line(x0, y0, xa, y0 - h, 2.5f, col); vg_line(xa, y0 - h, xd, y0 - h * s, 2.5f, col);
    vg_line(xd, y0 - h * s, xs, y0 - h * s, 2.5f, col); vg_line(xs, y0 - h * s, xr, y0, 2.5f, col);
}
static void sound_tags(void)
{
    const patch_t *pv = &live_p[sel];
    uint32_t ty = pv->v[P_TYPE] % NTYPE;
    char b[64]; b[0] = 0;
    if (page == PG_CHORUS || page == PG_DELAY || page == PG_REVERB || page == PG_DRIVE) {   /* (0.10) the effects: where their amount is set */
        const char *h = page == PG_DRIVE ? (set.dv_type % NDV == DV_AMP ? "INSERT · KNOB 4: THIS TRACK · ENCODER A: ANOTHER TRACK" : "SEND · MIXER > DRIVE: HOW MUCH PER TRACK")
                      : page == PG_DELAY ? "SEND · MIXER > DELAY: HOW MUCH PER TRACK" : page == PG_REVERB ? "SEND · MIXER > REVERB: HOW MUCH PER TRACK" : "SEND · MIXER > CHORUS: HOW MUCH PER TRACK";
        vg_text(h, 240, 214, VG_MED, DIM, VG_CENTER);
        return;
    }
    if (ty == T_DRUMS) scat(b, pv->v[P_KIT_SRC] == 2u ? "HIP HOP KIT · PADS 9-16 · SHIFT: LEVELS" : pv->v[P_KIT_SRC] ? "ACOUSTIC KIT · PADS 9-16 · SHIFT: LEVELS" : "KIT · 8 DRUMS · PADS 9-16 · SHIFT: LEVELS");   /* (0.31) */
    else if (ty == T_BASS) { scat(b, "808 · MONO · SLIDE "); scat(b, BSLIDE_NAME[pv->v[P_B_SLIDE] & 1u]); scat(b, " · "); scat(b, BGATE_NAME[pv->v[P_B_GATE] & 1u]); if (pv->v[P_B_DRIVE]) { scat(b, " · "); scat(b, BDT_NAME[pv->v[P_B_DTYPE] % NBDT]); } }
    else if (ty == T_DX) { scat(b, "DX7 · ALGO "); fmt_u(b + slen(b), (pv->v[P_DX_ALGO] ? pv->v[P_DX_ALGO] - 1u : pv->dx[134] & 31u) + 1u); scat(b, " · "); scat(b, pv->v[P_MODE] == M_MONO ? "MONO" : "POLY"); }
    else if (ty == T_MIDI) {                    /* (0.30) */
        scat(b, "MIDI OUT · CH "); fmt_u(b + slen(b), pv->v[P_MI_CH] % 16u + 1u);
        if (pv->v[P_MI_PROG]) { scat(b, " · PROGRAM "); fmt_u(b + slen(b), pv->v[P_MI_PROG]); }
        scat(b, " · CLOCK + START/STOP · SILENT HERE");
    }
    else if (ty == T_SMP) {
        uint32_t src = pv->v[P_SM_SRC] % 9u, mode = pv->v[P_SM_CHOP] % 3u;
        scat(b, "SAMPLER · ");
        if (sm_n[sel % NPART]) { scat(b, mode ? "" : "PLAY · "); if (mode) { fmt_u(b + slen(b), sm_n[sel % NPART]); scat(b, " SLICES · "); } scat(b, SMQ_NAME[pv->v[P_SM_QUAL] & 3u]); if (src) { scat(b, " · "); scat(b, SMTRK_NAME[src]); } }
        else scat(b, src ? "NO TAKE ON THAT TRACK" : "NO SAMPLE YET · REC RECORDS ONE");
    }
    else if (ty == T_EP) { scat(b, pv->v[P_EP_MODEL] & 1u ? "E-PIANO · REED" : "E-PIANO · TINE"); if (pv->v[P_EP_TREM]) scat(b, pv->v[P_EP_STEREO] > 63u ? " · STEREO TREMOLO" : " · TREMOLO"); scat(b, " · "); scat(b, pv->v[P_MODE] == M_MONO ? "MONO" : "POLY"); }
    else if (ty == T_STR) { scat(b, "STRING ENSEMBLE · "); scat(b, pv->v[P_ST_ENS] ? "ENSEMBLE ON" : "DRY"); scat(b, " · "); scat(b, pv->v[P_MODE] == M_MONO ? "MONO" : "POLY"); }
    else if (ty == T_SYNTH) { scat(b, "OBERVAVE · "); scat(b, MODE_NAME[pv->v[P_MODE] & 3u]); scat(b, " · "); scat(b, FTYPE_NAME[pv->v[P_FTYPE] % NFTYPE]); }
    else { scat(b, "FM 4-OP · ALGO "); fmt_u(b + slen(b), pv->v[P_ALGO] % NALGO + 1u); scat(b, " · "); scat(b, pv->v[P_MODE] == M_MONO ? "MONO" : "POLY"); }
    if (pv->v[P_ARP_MODE]) { scat(b, " · ARP "); scat(b, ARP_NAME[pv->v[P_ARP_MODE] % NARPMODE]); }
    if (hold >> sel & 1u) scat(b, " · HOLD");
    if (octave && ty != T_DRUMS) { char o[4]; scat(b, " · OCT "); if (octave > 0) scat(b, "+"); vg_itoa(octave, o); scat(b, o); }
    vg_text(b, 240, 214, VG_MED, octave && ty != T_DRUMS ? ACCENT : DIM, VG_CENTER);
}
static void scope_view(void)
{
    static int16_t shown[SCOPE_N];
    if (scope_state == 3u) { memcpy(shown, scope, sizeof shown); scope_state = 1; }
    else if (!scope_state) scope_state = 1;
    float px = CX0, py = 256;
    for (uint32_t i = 1; i < SCOPE_N; i++) {
        float x = CX0 + (float)i * (float)CW / (SCOPE_N - 1), y = 256 - vd_clamp((float)shown[i] / 32768.0f, -0.9f, 0.9f) * 20.0f;
        vg_line(px, py, x, y, 1.6f, vg_mix(BG, TRK_RGB[sel], 0.8f)); px = x; py = y;
    }
}
static void global_view(void)
{
    knob_t k[4]; page_knobs(k);
    uint32_t xq = ctl_daw_quarter ? ctl_daw_quarter : clk_last && o3_ms - clk_last < 2000u ? ctl_ext_quarter : 0u;
    int ext = xq != 0u;
    char t[8]; fmt_u(t, ext ? (60u * obv_fs + xq / 2u) / xq : set.tempo);
    vg_text(t, 160, 48, VG_HUGE, ext ? GREEN : FG, VG_CENTER); vg_text("BPM", 244, 82, VG_MED, DIM, VG_LEFT);
    vg_text(set.sync_int ? "SYNC: INTERNAL" : ext ? "SYNC: AUTO \xC2\xB7 CLOCK" : "SYNC: AUTO", 466, 48, VG_MED, set.sync_int ? ACCENT : ext ? GREEN : DIM, VG_RIGHT);
    {   /* (3.1) MIDI OUT: the tracks' channels */
        char m[40]; m[0] = 0;
        if (set.midi_out) { char c[8]; scat(m, "MIDI OUT: A-F ch "); fmt_u(c, mo_ch()); scat(m, c); scat(m, "-"); fmt_u(c, (mo_ch() + NTRK - 1u - 1u) % 16u + 1u); scat(m, c); }
        else scat(m, "MIDI OUT: OFF");
        vg_text(m, 466, 74, VG_MED, set.midi_out ? GREEN : DIM, VG_RIGHT);
    }
    vg_text("ENTER: sync \xC2\xB7 SHIFT+4: TUNE", 466, 100, VG_MED, MUTED, VG_RIGHT);
    for (int i = 0; i < 4; i++) {
        int y = 128 + i * 36; char n[2] = {(char)('1' + i), 0};
        vg_round(CX0 + 6, y, 26, 26, 4, CELL2); vg_text(n, CX0 + 19, y + 4, VG_MED, FG, VG_CENTER);
        vg_text(k[i].label, CX0 + 44, y + 4, VG_MED, FG, VG_LEFT);
        vg_text(k[i].value, CX0 + 170, y, VG_BIG, i == 1 && set.click % NCLICK ? ACCENT : 0x00E5FF, VG_LEFT);
    }
    vg_text("F6: OUTPUT", 466, 136, VG_MED, MUTED, VG_RIGHT);
    vg_text("SHIFT+DATA: F keys", 466, 172, VG_MED, MUTED, VG_RIGHT);
    vg_text("SHIFT+ENTER: RESET", 466, 208, VG_MED, MUTED, VG_RIGHT);
    vg_text("DATA: MIDI out", 466, 244, VG_MED, MUTED, VG_RIGHT);
}
static void save_banner(void)
{
    char b[40], n[PATCH_NAME + 1];
    if (pat_save_armed) {
        if (o3_ms - save_t > 3000u) { pat_save_armed = 0; return; }
        vg_round(70, 190, 340, 84, 8, 0x3A0C40);
        vg_text("SAVE PATTERN?", 240, 196, VG_MED, ACCENT, VG_CENTER);
        b[0] = 'P'; fmt_u(b + 1, set.pat + 1u); vg_text(b, 240, 218, VG_BIG, FG, VG_CENTER);
        vg_text("F5 (TAP): save · any other key: no", 240, 252, VG_MED, DIM, VG_CENTER);
        return;
    }
    if (!save_armed) return;
    if (o3_ms - save_t > 3000u) { save_armed = 0; return; }
    vg_round(70, 190, 340, 84, 8, 0x3A0C40);
    vg_text("SAVE HERE?", 240, 196, VG_MED, ACCENT, VG_CENTER);
    b[0] = 0; scat(b, TYPE_NAME[trk_type(sel)]); scat(b, " "); fmt_u(b + slen(b), prog_num(save_to)); scat(b, "  "); prog_name(save_to, n); scat(b, n);   /* (0.31: the kind's own numbers) */
    vg_text(b, 240, 218, VG_BIG, FG, VG_CENTER);
    vg_text("DATA: another slot · F5 (TAP): save", 240, 252, VG_MED, DIM, VG_CENTER);
}
static void perf_leds(void)                     /* sound pages: row 1 start / stop (REC: red), row 2 the tracks, rows 3-4 notes */
{
    for (uint32_t c = 0; c < 4u; c++) {
        uint32_t s = trk_state(c);
        uint32_t t = col_trk(c);
        if (t >= NTRK) { vibe_led_pad((int)c, 0, 0); vibe_led_pad((int)(4u + c), 0, 0); continue; }
        s = trk_state(t);
        vibe_led_pad((int)c, rec ? RED : GREEN, s == 1u ? 1.0f : s >= 2u ? (blink() ? 1.0f : 0.2f) : 0.12f);
        vibe_led_pad((int)(4u + c), TRK_RGB[t], t == sel ? 1.0f : 0.25f);
    }
    if (drums_sel()) for (int p = 8; p < 16; p++) { uint32_t d = PAD_DRUM[p - 8]; vibe_led_pad(p, DRUM_RGB[d], pad_sent[p] || ui_vlevel[card_lo(sel) + d] > 3000 ? 1.0f : 0.15f); }
    else for (int p = 8; p < 16; p++) vibe_led_pad(p, TRK_RGB[sel], pad_sent[p] ? 1.0f : 0.12f);
}

/* SDK 24 Link (7.11.2026): what the page and the encoders are now, for an FM-1 under VIBE FM Link – the same knobs the
 * page and the HOME overview show (o3_menu.h), the track on A, the sound on B, DATA's job */
static void link_say(void)
{
    knob_t k[4];
    int sq = in_seq();
    vibe_link_page(menu ? "CONTROLS" : files_on ? (f_page ? "SAVE" : "LOAD") : page == PG_MIXER ? "MIXER" : sq ? SV_NAME[SV_EFF] : snd_view() ? SX_TITLE[snd_view()] : PG(page)->title);
    if (page == PG_MIXER) mixer_knobs(k); else if (sq) seq_knobs(k); else if (snd_view()) snd_knobs(snd_view(), k); else if (page == PG_SCALE) scale_knobs(k); else page_knobs(k);
    for (int i = 0; i < 4; i++) if (k[i].label) vibe_link_encoder(VE_1 + i, k[i].label, k[i].value);
    { char l[2] = {(char)('A' + sel), 0}; vibe_link_encoder(VE_A, "TRACK", l); }
    { char b[24], nm[PATCH_NAME + 1]; uint32_t j = 0, q = 0; fmt_u(b, prog_num(trk_prog[sel])); prog_name(trk_prog[sel], nm);
      while (b[j]) j++; b[j++] = ' '; while (nm[q] && j < sizeof b - 1) b[j++] = nm[q++]; b[j] = 0;
      vibe_link_encoder(VE_B, "SOUND", b); }
    vibe_link_encoder(VE_DATA, shift ? "TEMPO" : save_armed ? "SLOT" : sq && SV_ENTRY(seq_view) && !au_is(sel) ? "STEP" : sq || page == PG_MIXER ? "PATTERN" : page == PG_GLOBAL ? "MIDI OUT" : "PATCH", "");
}
void vibe_on_draw(uint16_t *fb)
{
    static const char *const SOFT[6] = {"MIXER", "TRACKS", "LOAD", "SAVE", "STORE", "GLOBAL"};
    static const char *const SOFT_SH[6] = {"MIXER", "TRACKS", "OCT -", "OCT +", "HOLD", "UNDO"};
    knob_t k[4];
    store_poll();
    sm_update();                                /* (0.19) the samplers' slices, when their take or settings changed */
    auto_poll();
    seq_poll();
    if (ret_down && !ret_used && o3_ms - ret_t > 2000u) { ret_used = 1; seq_undo(); }   /* (3.8) RETURN held 2 s: one step back */
    au_poll();
    files_poll();
    meters_take();
    vg_begin(fb);
    if (splash_on) { splash_draw(fb); return; }
    link_say();                                 /* (SDK 24) */
    vg_clear(BG);
    if (menu) { overview(); vf_begin(); vf_end(ACCENT); }   /* (FOCUS: nothing to frame in the overview) */
    else {
        grp = page_group(page);
        header(files_on ? (f_page ? "SAVE" : "LOAD") : page == PG_MIXER ? "MIXER" : in_seq() ? "SEQUENCER" : snd_view() ? SX_TITLE[snd_view()] : PG(page)->title,
               in_seq() ? KEY_RGB[CK_SEQ] : grp->key < 6 ? KEY_RGB[grp->key] : grp->key == 6 ? 0xFF3D6E : ACCENT);
        {   /* the pattern (→ the next one, blinking) and REC */
            char b[12]; b[0] = 'P'; fmt_u(b + 1, set.pat + 1u);
            if (pat_queue) { scat(b, ">"); fmt_u(b + slen(b), pat_queue); }
            vg_text(b, 160, 8, VG_MED, pat_queue && blink() ? ACCENT : DIM, VG_RIGHT);
            if (rec) vg_circle(130, 17, 6, rec == REC_ON || blink() ? RED : 0x7A1040);
            else if (ctl_au_rq || au_arm) vg_circle(130, 17, 6, ui_au_state == 2u || blink() ? RED : 0x7A1040);   /* (3.7: an audio take) */
        }
        if (files_on) files_view();             /* (3.7) */
        else if (page == PG_MIXER) mixer_view();
        else if (in_seq()) seq_view_draw();
        else if (page == PG_GLOBAL) global_view();
        else if (page == PG_SCALE) { group_chips(); scale_knobs(k); knobs_row(k, 74, TRK_RGB[sel]); scale_view(); }   /* (3.10) */
        else if (snd_view()) {                  /* (3.7) the track type's pages */
            uint32_t sx = snd_view();
            sound_chips();
            snd_knobs(sx, k);
            knobs_row(k, 74, TRK_RGB[sel]);
            snd_under(sx);
        }
        else if (smp_page()) { sound_chips(); page_knobs(k); smp_draw(k); }   /* (0.22) the SAMPLER's EDIT / CHOP, as ZVAMPLER's */
        else {
            if (grp->key == CK_SOUND) sound_chips(); else group_chips();
            page_knobs(k);
            knobs_row(k, grp->count > 1 || grp->key == 6 ? 74 : 50, page == PG_DRIVE || (page >= PG_CHORUS && page <= PG_REVERB) ? KEY_RGB[CK_FX] : TRK_RGB[sel]);
            if (page == PG_FENV || page == PG_AENV) env_graph(); else { sound_tags(); scope_view(); }
        }
        if (smp_page() && !shift) softkeys(smp_soft(), 0u); else softkeys(shift ? SOFT_SH : SOFT, (global_lit() ? 32u : 0u) | (save_armed || f5_down ? 16u : 0u) | (page == PG_MIXER ? 1u : 0u)
                 | (in_seq() && seq_view == SV_TRACKS ? 2u : 0u));
        if (shift || k_lock) for (uint32_t d = 0; d < 6u; d++) { static char kl[6][8]; if (drums_sel()) { kl[d][0] = 0; scat(kl[d], drum_name(live_p[sel].v, d)); } else if (smp_chop_sel()) { kl[d][0] = 0; scat(kl[d], "SLICE "); fmt_u(kl[d] + 6, d + 1u); } else sc_name(kl[d], d); vibe_label(VB_K1 + (int)d, kl[d]); }   /* (3.10) */
        vf_begin(); focus_items(); vf_end(ACCENT);
        save_banner();
        if (reset_ask) reset_view();
        toast_draw();
    }
    if (page == PG_MIXER) mixer_leds(); else if (in_seq()) seq_leds(); else perf_leds();
    for (int i = 0; i < 6; i++)                 /* (3.10) SHIFT held: the six keys of the scale in the track's colour, the key brighter */
        if (shift || k_lock || k_sent[i][0]) vibe_led_button(VB_K1 + i, TRK_RGB[sel], k_sent[i][0] ? 1.0f : i == 0 ? 0.55f : 0.25f);
        else vibe_led_button(VB_K1 + i, KEY_RGB[i], (in_seq() ? CK_SEQ : key_lit()) == i ? 1.0f : 0.15f);
    vibe_led_button(VB_OCT_DOWN, 0xE6D1B9, octave < 0 ? 0.4f + 0.2f * (float)(-octave) : 0.0f);   /* the keys moved down / up: */
    vibe_led_button(VB_OCT_UP, 0xE6D1B9, octave > 0 ? 0.4f + 0.2f * (float)octave : 0.0f);        /* brighter the further */
    vibe_led_button(VB_RETURN, TRK_RGB[sel], k_lock ? 0.8f : 0.0f);   /* (the K keys locked to the notes) */
    vibe_led_button(VB_PLAY, GREEN, ctl_seq_run ? 1.0f : 0.0f);
    vibe_led_button(VB_REC, RED, rec == REC_ON || ui_au_state == 2u ? 1.0f : rec == REC_READY || ctl_au_rq || au_arm ? (blink() ? 1.0f : 0.2f) : 0.0f);
}
