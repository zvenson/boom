/* SPDX-License-Identifier: GPL-3.0-only */
/* BOOM's engine: 32 voice cards, eight per track, in the audio flow. The frame around the voices – the note ring, the
 * held-note lists, the arpeggiators, the click, the mixer, the output stage, the scope – is OberVave 3's (obv_engine.h,
 * GPL-3.0); the voices are BOOM's own, in float:
 *   DRUMS  a card per drum (KICK SNARE CLAP CH OH RIM SNAP PERC): synthesised, the closed hat chokes the open one
 *   BASS   one card: the 808 – a sine with a punch (pitch envelope), a click, SUB, three drives, mono with slides
 *   MEL    eight cards: 4-operator FM (8 algorithms, feedback on op 4), a 12 dB filter with its envelope, the LFO
 * The main loop writes the live patch and the controllers; the flow here owns everything else. */
#ifndef RING_PUBLISH
#define RING_PUBLISH() __atomic_thread_fence(__ATOMIC_RELEASE)
#define RING_TAKE() __atomic_thread_fence(__ATOMIC_ACQUIRE)
#endif
#define NV 64u
#define NPART 8u                                /* the six tracks' sounds (BOOM 0.3, Sven 14.11.: six tracks) */
#define NCARD 8u                                /* cards per track */
#define NHELD 16u

enum { E_IDLE, E_ATT, E_DEC, E_SUS, E_REL };

/* four sounds, one per track (A's also sets the effects). The main loop writes, the audio flow reads. */
static patch_t live_p[NPART];
#define live (live_p[0])
static uint32_t mo_base, mo_tick;              /* (SDK 9) MIDI OUT: main.c */
static void mo_note(uint32_t part, uint32_t note, uint32_t vel, uint32_t on);
static void mo_all_off(uint32_t part);
static void mt_note(uint32_t part, uint32_t note, uint32_t vel, uint32_t on);   /* (0.30, main.c) a MIDI track's note out */
static void mt_block(void);                                                      /* its program and CCs when they change */
static void mt_all_off(uint32_t part);
/* the mixer (main loop writes): per track a level 0..127, a pan -64..64, a send into the effects 0..127; muted: bit t */
static volatile uint8_t ctl_mix_lvl[NPART] = {100, 100, 100, 100, 100, 100, 100, 100};
static volatile uint8_t ctl_mix_cho[NPART] = {127, 127, 127, 127, 127, 127, 127, 127}, ctl_mix_dly[NPART] = {127, 127, 127, 127, 127, 127, 127, 127}, ctl_mix_rev[NPART] = {127, 127, 127, 127, 127, 127, 127, 127};
static volatile int8_t ctl_mix_pan[NPART];
static int32_t amp_w[NPART];                    /* (0.17) each track's amp share, gliding (Q12) */
#define AMP_OUT 98304.0f                        /* the amp's full swing in the tracks' scale */
static volatile uint8_t ctl_mix_drv[NPART];
static volatile uint32_t ctl_mix_off;
static volatile uint8_t ctl_m_vol = 100, ctl_m_dj = 64, ctl_m_drive, ctl_m_comp;
static volatile uint32_t ctl_trk_own, ctl_trk_daw, ctl_host_outs = 2;   /* (BOOM 0.4) bit t: track t on its own output · the DAW plays it · the host's outputs */
static int32_t trk_x[NPART][2u * HALF_FRAMES];  /* a track on its own output: its signal after the fader (the piece) */
static uint32_t trk_x_used;                     /* bit t: trk_x[t] holds sound in this piece */
#define NMFX 4u                                 /* (BOOM) MASTER FX on the sum: FILTER, CRUSH, ECHO, REPEAT (o3_core.h master_fx) */
static volatile uint8_t ctl_mfx[NMFX] = {64, 0, 0, 0};
typedef struct { uint8_t lvl[NPART], cho[NPART], dly[NPART], rev[NPART], drv[NPART]; int8_t pan[NPART]; uint32_t off;
                 uint8_t dv_type, dv_amt, dv_tone, dv_mix, m_vol, m_dj, m_drive, m_comp, tempo, mfx[NMFX]; uint8_t eq[NPART][5]; } mxq_t;   /* (0.31: eq) */
static mxq_t mx_q;
static volatile uint32_t ctl_mx_swap;

/* main loop -> audio: note events */
typedef struct { uint8_t type, note, vel, part; } nev_t;
#define NQ 64u
static nev_t nq[NQ];
static volatile uint32_t nq_w, nq_r;
static void note_event_p(uint32_t part, uint32_t on, uint32_t note, uint32_t vel)
{
    if (nq_w - nq_r >= NQ) return;
    nq[nq_w % NQ].part = (uint8_t)part; nq[nq_w % NQ].type = (uint8_t)on;
    nq[nq_w % NQ].note = (uint8_t)note; nq[nq_w % NQ].vel = (uint8_t)vel;
    RING_PUBLISH();
    nq_w++;
}
static void note_event(uint32_t on, uint32_t note, uint32_t vel) { note_event_p(0, on, note, vel); }

/* the controllers (main loop writes) */
static volatile int32_t ctl_bend;               /* -8192..8191 */
static volatile uint32_t ctl_hold, ctl_cc7 = 127;
/* (22.11.2026) the sustain pedal per track (bit p): ctl_sus_live = a pedal pressed now (the VIBE's jack or CC 64 – on the
 * track its notes go to, no longer on every track), seq_ped = the pattern's recorded pedal (obv_seq.h, audio thread) */
static volatile uint32_t ctl_sus_live, seq_ped;
static void seq_ped_set(uint32_t t, uint32_t on) { if (on) seq_ped |= 1u << t; else seq_ped &= ~(1u << t); }
static volatile int32_t ctl_tune8;              /* global tune, Q8 semitones */
static volatile uint32_t ctl_tempo = 120, ctl_ext_quarter, ctl_daw_quarter;
static volatile uint32_t ctl_arp_restart, ctl_panic;

/* audio -> main loop */
static volatile int32_t ui_vlevel[NV];
static volatile uint8_t ui_vnote[NV];
static volatile uint32_t ui_render_us, ui_arp_step;
static volatile uint32_t ui_drum_hit[NDRUM];    /* counts each drum's hits on the DRUMS track shown (the pads light) */
#define SCOPE_N 240u
static int16_t scope[SCOPE_N];
static volatile uint32_t scope_state, scope_stride = 4;

/* ---- the held notes: in order of pressing, physically down or held by the pedal / latch */
static struct { uint8_t note, vel, phys; } held[NPART][NHELD];
static uint32_t nheld[NPART];
static uint32_t rnd_state = 0x9E3779B9u;

/* ---- tables at the sample rate (bm_tables_init) */
#define SINE_N 2048u
static float SINE[SINE_N + 1u];
static float KA[128], KD[128];                  /* the ADSR's per-block coefficients for TIME_MS_X10 (attack / decay) */
static float DK[128];                           /* a drum's decay: 20 ms .. 2 s, per block */
static float GK[128];                           /* the glide, per block (GLIDE_MS) */
static float inv_fs = 1.0f / 48000.0f, fs_f = 48000.0f;
static int32_t dx_pitch_corr;                   /* (DX) log2(44100 / fs), Q24: its tables are for 44.1 kHz */
static float kblock(float ms) { float tau = ms * 0.001f * fs_f; return tau < 1.0f ? 1.0f : 1.0f - vd_exp2(-(float)CTL / tau * 1.4426950f); }
static void wt_init(void);                      /* (0.41, bm_wavetable.h) */
static void bm_tables_init(float fs)
{
    fs_f = fs; inv_fs = 1.0f / fs;
    dx_pitch_corr = (int32_t)(vd_log2(44100.0f / fs) * 16777216.0f);
    for (uint32_t i = 0; i <= SINE_N; i++) SINE[i] = vd_sin((float)i / (float)SINE_N);
    for (uint32_t i = 0; i < 128u; i++) {
        float ms = (float)TIME_MS_X10[i] * 0.1f;
        KA[i] = kblock(ms / 1.466f); KD[i] = kblock(ms / 3.0f);
        DK[i] = kblock(20.0f * vd_pow(100.0f, (float)i / 127.0f) / 4.0f);
        GK[i] = kblock((float)GLIDE_MS[i] / 3.0f);
    }
    wt_init();                                  /* (0.41) the WAVETABLE kind's tables */
}
static inline float sinf_t(float ph)            /* ph in turns, any value */
{
    ph -= (float)(int32_t)ph; if (ph < 0.0f) ph += 1.0f;
    float x = ph * (float)SINE_N; uint32_t i = (uint32_t)x; float f = x - (float)i;
    if (i >= SINE_N) return SINE[0];
    return SINE[i] + (SINE[i + 1u] - SINE[i]) * f;
}
static inline float noise_f(uint32_t *s) { return (float)(int32_t)xorshift(s) * (1.0f / 2147483648.0f); }
static inline float semi_inc(float semi) { return 440.0f * vd_exp2((semi - 69.0f) * (1.0f / 12.0f)) * inv_fs; }   /* turns per frame */
static inline float hz_of(float semi) { return 440.0f * vd_exp2((semi - 69.0f) * (1.0f / 12.0f)); }
#include "bm_wavetable.h"   /* (0.41) */
#include "bm_grand.h"       /* (0.41) PIANO: the sampled grand – private data, silent without it (bm_grand.h) */
#define NCVP 24u            /* (0.41) CLAV: partials of its string */


/* ---- DX (BOOM 0.6): a DX7 note – sloopDX's dx7_core (a C port of Dexed's msfa; GPL-3.0, Sven Trogus), 6 operators,
 * 32 algorithms, rendered 32 frames at a time (two control blocks). Its tables are for 44.1 kHz: at another rate the pitch
 * offset every operator takes puts the tuning right (dx_pitch_corr); envelopes and LFO run that little faster. */
#define DX_LG_N 5
#include "dx7_core.h"
#include "bm_realkit.h"
#include "bm_hiphop.h"   /* (0.27) */
/* (0.19) SAMPLER: the slices of each sampler track (main loop: bm_sampler.h writes them, sm_n last), the take they cut */
static volatile uint32_t sm_at[8][17], sm_n[8];
static uint32_t sm_cur[8];                      /* (main loop) each sampler's chosen cut (the CHOP page, the last slice played) */
static const int16_t *sm_take(uint32_t t, uint32_t *hi, uint32_t *st);                         /* (0.13) the acoustic kit's eight samples */
#define DX_SEMI 1398101                         /* a semitone in Q24 log2 */
static void dx_build(const uint8_t *pv, const uint8_t *voice, uint8_t *o)   /* the voice with the macros over it */
{
    int a = ((int)pv[P_DX_ATTACK] - 64) * 45 / 64, d = ((int)pv[P_DX_DECAY] - 64) * 45 / 64, r = ((int)pv[P_DX_RELEASE] - 64) * 45 / 64;
    memcpy(o, voice, DX_VOICE);
    for (uint32_t op = 0; op < 6u; op++) {       /* rates: 99 fast .. 0 slow – a macro to the right makes it longer */
        uint8_t *q = o + op * 21u;
        q[0] = (uint8_t)clampi((int)q[0] - a, 0, 99);
        q[1] = (uint8_t)clampi((int)q[1] - d, 0, 99); q[2] = (uint8_t)clampi((int)q[2] - d, 0, 99);
        q[3] = (uint8_t)clampi((int)q[3] - r, 0, 99);
    }
    if (pv[P_DX_ALGO]) o[134] = (uint8_t)((pv[P_DX_ALGO] - 1u) & 31u);
    if (pv[P_DX_FB]) o[135] = (uint8_t)((pv[P_DX_FB] - 1u) & 7u);
}

/* ---- SYNTH: OberVave's voice (obv_engine.h, fixed point) inside a card – its own fields, its own block and render */
#define ENV_ONE (1 << 24)
#define LEVEL_FLOOR 2048
typedef struct { uint32_t stage; int32_t level; } env_t;   /* level Q24 */
typedef struct {
    osc_t o1, o2;
    env_t fe, ae;
    svf_state_t f1; svf_coef_t c1;
    lad_state_t lad; lad_coef_t lc;
    uint32_t inc1, inc2, pw1, pw2;
    int32_t amp0, amp1;                         /* Q15 */
    int32_t pitch16, target16, det8;
    int32_t gl, gr;                             /* Q12 */
    int32_t drift;
    uint32_t lfo_blk; int32_t lfo_fade;
} ov_voice_t;
typedef struct {                                /* OberVave's numbers derived from the patch once per block */
    int32_t last_pitch16;
    int32_t fenv_amt, keytrk, velflt, lfo_pd, lfo_fd, lfo_pwd, lfo_ad, wheel_q, bend8, env_o2, det_fine8;
    int32_t lvl1, lvl2, lvln, drive_q12, reso_k, lad_k, vint, sus_f, sus_a, gain_q12, morph_q;
    int32_t cut_sm, reso_sm;
    uint32_t pw1_base, pw2_base, w1, w2, ftype, sync_on, xmod_q, glide_k;
} ov_part_t;
static const int16_t VINT_TUNE8[NCARD] = {-20, 12, 7, -15, 18, -9, 3, -17};
static const int16_t VINT_CUT8[NCARD] = {-300, 250, 120, -380, 60, 330, -180, 220};
static const int8_t VINT_ENV[NCARD] = {10, -6, 14, -12, 3, -16, 8, -4};
static const int8_t PAN_CARD[NCARD] = {-40, 40, -24, 24, -56, 56, -10, 10};

/* ---- the sounds as the audio flow sees them */
typedef struct {
    const patch_t *p;
    uint32_t lfo_ph, lfo_rnd_a, lfo_rnd_b;
    float lfo_val;                              /* this block, -1..1 */
    float last_pitch;                           /* where a glide starts from */
    float bend, lfo_pd, lfo_fd, lfo_ad, gain;   /* derived once per block */
    uint32_t type;
    ov_part_t ov;                               /* SYNTH's */
    float ep_trem;                              /* (0.31) E-PIANO: the tremolo's phase (turns) */
} part_t;
static part_t pt[NPART] = {{&live_p[0], 0, 0, 0, 0, 60}, {&live_p[1], 0, 0, 0, 0, 60}, {&live_p[2], 0, 0, 0, 0, 60}, {&live_p[3], 0, 0, 0, 0, 60},
                           {&live_p[4], 0, 0, 0, 0, 60}, {&live_p[5], 0, 0, 0, 0, 60}, {&live_p[6], 0, 0, 0, 0, 60}, {&live_p[7], 0, 0, 0, 0, 60}};
static inline uint32_t part_type(uint32_t p) { return pt[p % NPART].p->v[P_TYPE] % NTYPE; }

typedef struct {
    uint8_t part, note, vel, gate, type, drum, kill, pad_;
    uint32_t seq;
    float pitch, target;                        /* semitones */
    float aenv, fenv; uint8_t ast, fst, pad2_[2];   /* the amp and filter envelopes (0..1), their stages */
    float amp0, amp1;                           /* the block's start and end amplitude */
    float gl, gr;                               /* pan */
    float lfo_fade; uint32_t lfo_blk;
    uint32_t noise;
    float ph[6];                                /* phases in turns (FM: the four operators; hats: six squares) */
    float inc0[6], inc1[6];                     /* this block's increments, start and end */
    float fb_a, fb_b;                           /* FM feedback memory */
    float openv[4]; uint8_t opst[4];
    float opl0[4], opl1[4];                     /* the operators' levels (envelope x LEVEL) at the block's ends */
    float penv, t;                              /* the pitch envelope 1 -> 0; seconds since the start */
    float dc_x, dc_y;                           /* the DC blocker after a drive */
    float hp;                                   /* a one-pole */
    vd_svf f1, f2; vd_svf_c c1, c2; int32_t ck1, ck2;   /* two filters (noise shaping / the sound's) and their coefficient keys */
    ov_voice_t ov;                              /* SYNTH: OberVave's voice */
    dxv_t dx;                                   /* DX: the note */
    int32_t dx_buf[DX_N], dx_pitch, dx_dcx, dx_dcy;   /* its 32 frames ahead, its pitch offset, its DC blocker */
    uint32_t sp_i; float sp_f, sp_lp, sp_k;      /* (0.13) an acoustic drum: its place in the sample, a low-pass by the velocity */
    uint32_t sm_a, sm_b, sm_i, sm_age; float sm_f, sm_acc, sm_hold, sm_lp; uint8_t sm_done, sm_rev, sm_loop, sm_pad;   /* (0.19) a SAMPLER slice: from, to, where, how long */
    uint8_t dx_pos, dx_up, dx_note0, dx_pad;    /* frames of dx_buf played · key let go told · the note it began on */
    float ep_k, ep_b, ep_tb, ep_nrm, ep_pre, ep_post, ep_velc, ep_fast, ep_det, ep_lo, ep_top;   /* (0.31) E-PIANO: the pickup's curve, the preamp, the touch */
    uint32_t gp_b0, gp_b1, gp_len, gp_key, gp_rb0, gp_rb1; float gp_w, gp_lvl;   /* (gp_rb*: where each level's R starts, after its L) */   /* (0.41) PIANO: the two recordings (their first blocks), their length, its key, the mix, the touch's level */
    uint32_t gp_bp[2][2], gp_ln[2];   /* (GP_VBITS, Sven's own piano 22.11.) each level's and channel's block now (byte), each level's length */
    int32_t gp_y1[2][GP_NCH], gp_y2[2][GP_NCH]; uint32_t gp_dn; float gp_ring[GP_NCH][8]; float gp_last[2], gp_dk[2];   /*   their decoders (level, channel: the last two values), samples decoded, the last 8 mixed per channel */
    float cv_y1[NCVP], cv_y2[NCVP], cv_c[NCVP], cv_r2[NCVP], cv_r[NCVP], cv_g[NCVP], cv_mul[NCVP];   /* (0.41) CLAV: each partial's two last values, */
    float cv_env, cv_clk, cv_pitch; uint8_t cv_n, cv_rel, cv_pad[2];   /*   its coefficients, decay, pickup weight, ratio; the wah's envelope, the click */
} voice_t;
static voice_t vc[NV];
#if defined(GP_VBITS) && GP_VBITS
static void gp_next(voice_t *v)                 /* (22.11.) PIANO: the next sample of both recordings, both channels – prediction 2 y1 - y2 plus the error
                                                 *   (per 32 samples a header bits << 4 | shift, then 32 errors of 0..12 bits) – mixed; a level past its end is still */
{
    uint32_t j = v->gp_dn, kk = j & 31u;
    for (uint32_t c = 0; c < GP_NCH; c++) {
        float m = 0.0f;
        for (uint32_t l = 0; l < 2u; l++) {
            float w = l ? v->gp_w : 1.0f - v->gp_w;
            uint32_t bp, nb; int32_t e = 0, y;
            if (w < 0.001f || j >= v->gp_ln[l]) continue;
            bp = v->gp_bp[l][c];
            if (!kk && j) { bp += 1u + 4u * (uint32_t)(GP_DATA[bp] >> 4); v->gp_bp[l][c] = bp; }   /* (the next block) */
            nb = GP_DATA[bp] >> 4;
            if (nb) {
                uint32_t bit = kk * nb; const uint8_t *q = GP_DATA + bp + 1u + (bit >> 3);
                uint32_t u = (((uint32_t)q[0] | (uint32_t)q[1] << 8 | (uint32_t)q[2] << 16) >> (bit & 7u)) & ((1u << nb) - 1u);   /* (up to 12 bits: three bytes) */
                e = (int32_t)(u << (32u - nb)) >> (32u - nb);
            }
            y = 2 * v->gp_y1[l][c] - v->gp_y2[l][c] + e * (1 << (GP_DATA[bp] & 15u));
            v->gp_y2[l][c] = v->gp_y1[l][c]; v->gp_y1[l][c] = y;
            m += w * (float)y;
        }
        v->gp_ring[c][j & 7u] = m;
    }
    v->gp_dn = j + 1u;
}
#else
static void gp_next(voice_t *v)                 /* (0.41) PIANO: the next sample of both recordings, both channels – prediction 2 y1 - y2 plus the error (8 bits << shift) – mixed */
{
    uint32_t j = v->gp_dn, off = (j >> 5) * (GP_BLK + 1u), k = 1u + (j & 31u);
    for (uint32_t c = 0; c < GP_NCH; c++) {
        float m = 0.0f;
        for (uint32_t l = 0; l < 2u; l++) {
            float w = l ? v->gp_w : 1.0f - v->gp_w;
            if (w < 0.001f) continue;
            const uint8_t *b = GP_DATA + (c ? (l ? v->gp_rb1 : v->gp_rb0) : (l ? v->gp_b1 : v->gp_b0)) * (GP_BLK + 1u) + off;
            int32_t y = 2 * v->gp_y1[l][c] - v->gp_y2[l][c] + (int32_t)(int8_t)b[k] * (1 << b[0]);
            v->gp_y2[l][c] = v->gp_y1[l][c]; v->gp_y1[l][c] = y;
            m += w * (float)y;
        }
        v->gp_ring[c][j & 7u] = m;
    }
    v->gp_dn = j + 1u;
}
#endif
static uint32_t vseq, rr[NPART];

static void held_remove(uint32_t p, uint32_t note)
{
    uint32_t i, j;
    for (i = j = 0; i < nheld[p]; i++) if (held[p][i].note != note) held[p][j++] = held[p][i];
    nheld[p] = j;
}

static void voice_pan(voice_t *v, int32_t pan)   /* -64..64 */
{
    uint32_t pos = (uint32_t)(clampi(pan, -64, 64) + 64);
    v->gl = (float)PAN_Q12[128u - pos] * (1.0f / 2896.0f);
    v->gr = (float)PAN_Q12[pos] * (1.0f / 2896.0f);
}
/* (0.41, Sven 21.11.) CLAV: a string struck near its end by the key's tangent, heard by two magnetic pickups – NCVP partials,
 * each a decaying sine (two-pole resonator: y = c y1 - r2 y2). The strike: sin(n pi h) per partial, TONE tilts the rest;
 * a pickup at x along the string hears partial n as sin(n pi x); MUTE (the felt) shortens the upper partials; let go, the
 * yarn damps them all fast with a pluck. B: the stiff string's stretch */
static void cv_coef(voice_t *v, float hz)       /* the partials' frequencies at this pitch (their decays kept) */
{
    const float B = 0.00025f;
    for (uint32_t n = 0; n < v->cv_n; n++) {
        float fn = hz * v->cv_mul[n], w = fn * inv_fs;
        if (w > 0.45f) w = 0.45f;
        v->cv_c[n] = 2.0f * v->cv_r[n] * sinf_t(w + 0.25f);
    }
    (void)B;
}
static void cv_decays(voice_t *v, const uint8_t *pv, int rel)   /* each partial's decay: rings, or the damper on it */
{
    float keysc = vd_exp2(-((float)v->note - 60.0f) * (1.0f / 30.0f));
    float tau0 = 0.35f * vd_pow(16.0f, (float)pv[P_CV_DECAY] / 127.0f) * keysc;   /* 0.35 .. 5.6 s at C4 */
    float mk = 0.015f + (float)pv[P_CV_MUTE] / 127.0f * 0.5f;
    for (uint32_t n = 0; n < v->cv_n; n++) {
        float nn = (float)n, tau = rel ? 0.022f / (1.0f + 0.08f * nn) : tau0 / (1.0f + mk * nn * vd_pow(nn + 1.0f, 0.3f));
        v->cv_r[n] = vd_exp2(-1.4427f * inv_fs / tau);
        v->cv_r2[n] = v->cv_r[n] * v->cv_r[n];
    }
}
static void cv_start(voice_t *v, const uint8_t *pv, uint32_t note, uint32_t vel)
{
    static const float PU[NCVPU][2] = {{0.0f, 1.0f}, {1.0f, 0.0f}, {0.7f, 0.7f}, {1.0f, -1.0f}};   /* bridge, neck */
    const float h = 0.045f, xb = 0.085f, xn = 0.21f, B = 0.00025f;
    float vc = (float)vel / 127.0f, sens = (float)pv[P_CV_VELO] / 127.0f, velc = 1.0f - sens + sens * vc;
    float k = 1.35f - (float)pv[P_CV_TONE] / 127.0f * 1.2f - velc * 0.35f, hz = hz_of((float)note), sum = 0.0f;
    uint32_t pu = pv[P_CV_PICKUP] % NCVPU, n;
    if (k < 0.0f) k = 0.0f;
    v->cv_n = 0;
    for (n = 0; n < NCVP; n++) {
        float nn = (float)(n + 1u), mul = nn * vd_sqrt(1.0f + B * nn * nn), a, pk;
        if (hz * mul * inv_fs > 0.42f) break;
        a = sinf_t(nn * h * 0.5f) * vd_pow(nn, -k);                                  /* the strike */
        pk = PU[pu][0] * sinf_t(nn * xb * 0.5f) + PU[pu][1] * sinf_t(nn * xn * 0.5f);   /* the pickups */
        v->cv_mul[n] = mul; v->cv_g[n] = a * pk; sum += v->cv_g[n] * v->cv_g[n];
        v->cv_n = (uint8_t)(n + 1u);
    }
    sum = sum > 1e-9f ? 0.55f / vd_sqrt(sum) : 0.0f;
    cv_decays(v, pv, 0); cv_coef(v, hz);
    for (n = 0; n < v->cv_n; n++) {             /* y = A r^t sin(t w): from zero, no click */
        float w = hz * v->cv_mul[n] * inv_fs;
        v->cv_g[n] *= sum * (0.25f + 0.75f * velc);
        v->cv_y1[n] = 0.0f; v->cv_y2[n] = -sinf_t(w) / v->cv_r[n];
    }
    v->cv_env = 0.4f + 0.6f * velc; v->cv_clk = (float)pv[P_CV_CLICK] / 127.0f * (0.3f + 0.7f * velc); v->cv_rel = 0; v->cv_pitch = (float)note;
    v->hp = 0.0f; v->ck1 = v->ck2 = -1; v->dc_x = v->dc_y = 0.0f;
    memset(&v->f1, 0, sizeof v->f1);
}
static void voice_start(voice_t *v, uint32_t part, uint32_t note, uint32_t vel, int32_t pan, int retrig)
{
    const uint8_t *pv = pt[part].p->v;
    v->part = (uint8_t)part; v->note = (uint8_t)note; v->vel = (uint8_t)vel; v->gate = 1; v->kill = 0;
    if (v->type == T_GP && v->ast != E_IDLE) { v->gp_dk[0] = v->gp_last[0]; v->gp_dk[1] = v->gp_last[1]; }   /* (Sven 21.11.: crackles with the pedal) a ringing piano voice taken over: */
    else v->gp_dk[0] = v->gp_dk[1] = 0.0f;   /*   where its sound stood, faded out over ~2 ms on top of the new note – no step, no click */
    v->seq = ++vseq; v->type = (uint8_t)part_type(part);
    if (v->type == T_EP && pv[P_EP_CLAV]) v->type = T_CV;   /* (0.41) E-PIANO's CLAV: its own voice */
    v->target = (float)note;
    if (retrig) {
        v->pitch = pv[P_GLIDE] ? pt[part].last_pitch : v->target;
        v->ast = E_ATT; v->fst = E_ATT; v->penv = 1.0f; v->t = 0.0f;
        for (uint32_t i = 0; i < 4u; i++) { v->opst[i] = E_ATT; }
        v->lfo_blk = 0; v->lfo_fade = pv[P_LFO_DELAY] ? 0.0f : 1.0f;
        v->fb_a = v->fb_b = 0.0f;
        if (v->type == T_SMP) {                 /* (0.19) a slice: the key from C3 up picks it (round the slices); (0.20) PLAY: the whole sample */
            uint32_t ns = sm_n[part % 8u], kc = pv[P_SM_CHOP] % 3u;   /* (0.25) MEL: START to END over the cuts; CHOP: one slice */
            v->sm_done = 1; v->sm_rev = pv[P_SM_REV] & 1u; v->sm_age = 0; v->sm_f = 0.0f; v->sm_acc = 1.0f; v->sm_hold = 0.0f; v->sm_lp = 0.0f;
            if (ns) {
                int32_t sl = kc && ns > 1u ? (int32_t)note - 48 : 0;   /* (0.24) C3 = slice 1, C#3 = 2 … in order; outside the slices: silent */
                if (!kc) { v->sm_a = sm_at[part % 8u][0]; v->sm_b = sm_at[part % 8u][ns]; }
                else if (sl >= 0 && (uint32_t)sl < ns) { v->sm_a = sm_at[part % 8u][sl]; v->sm_b = sm_at[part % 8u][sl + 1]; }
                else { v->sm_a = v->sm_b = 0; }
                if (v->sm_b > v->sm_a + 64u) { v->sm_i = v->sm_rev ? v->sm_b - 2u : v->sm_a; v->sm_done = 0; }
            }
        }
        if (v->type == T_EP) {                  /* (0.31) a new strike: the tine / reed from rest, all its envelopes full */
            for (uint32_t i = 0; i < 4u; i++) { v->openv[i] = 1.0f; v->opl0[i] = v->opl1[i] = 0.0f; }
            v->ph[0] = v->ph[1] = v->ph[2] = 0.0f; v->hp = 0.0f; v->dc_x = v->dc_y = 0.0f; v->ck2 = -1; v->ep_fast = 1.0f;
            v->ep_lo = v->ep_top = 0.0f; v->ep_det = 1.0f + noise_f(&v->noise) * 0.0011f;   /* (no two tines quite alike: +-2 cents) */
        }
        if (v->type == T_GP) {                  /* (0.41) PIANO: the nearest recorded key, the two recordings the touch falls between */
            int32_t k = ((int32_t)note - (int32_t)GP_KEY0 + (int32_t)GP_STEP / 2) / (int32_t)GP_STEP;   /* (the nearest recorded key) */
            float vc = (float)vel / 127.0f, x;
            uint32_t l0;
            if (k < 0) k = 0; else if (k > (int32_t)GP_NKEY - 1) k = (int32_t)GP_NKEY - 1;
            x = vd_pow(vc, GP_CURVE) * (float)(GP_NLAY - 1u) + ((float)pv[P_GP_HAMMER] - 64.0f) * (1.0f / 48.0f);   /* (soft touch: mf stays on the softer recordings) */
            if (x < 0.0f) x = 0.0f; else if (x > (float)(GP_NLAY - 1u)) x = (float)(GP_NLAY - 1u);
            l0 = (uint32_t)x; if (l0 > GP_NLAY - 2u) l0 = GP_NLAY - 2u;
            v->gp_key = (uint32_t)k; v->gp_w = x - (float)l0;
            /* (22.11.: recordings that keep the played levels, ~6.7 dB a layer, are mixed all the way between them, or the
             * touch climbs in steps) */
            {   /* (stereo: a recording's R follows its L, as many blocks on) */
                uint32_t i0 = ((uint32_t)k * GP_NLAY + l0) * GP_NCH, i1 = ((uint32_t)k * GP_NLAY + l0 + 1u) * GP_NCH;
                v->gp_b0 = GP_AT[i0]; v->gp_b1 = GP_AT[i1];
                v->gp_len = GP_LEN[i0] < GP_LEN[i1] ? GP_LEN[i0] : GP_LEN[i1];
                v->gp_rb0 = GP_AT[i0 + GP_NCH - 1u]; v->gp_rb1 = GP_AT[i1 + GP_NCH - 1u];   /* (the levels of a key may be of different lengths) */
#if defined(GP_VBITS) && GP_VBITS
                v->gp_bp[0][0] = GP_AT[i0]; v->gp_bp[0][1] = GP_AT[i0 + GP_NCH - 1u]; v->gp_bp[1][0] = GP_AT[i1]; v->gp_bp[1][1] = GP_AT[i1 + GP_NCH - 1u];
                v->gp_ln[0] = GP_LEN[i0]; v->gp_ln[1] = GP_LEN[i1];   /* (each recording its own length: a level ended is still, the voice */
                if (v->gp_w < 0.001f) v->gp_len = GP_LEN[i0];         /*   rings as long as the longer it plays) */
                else if (v->gp_w > 0.999f) v->gp_len = GP_LEN[i1];
                else v->gp_len = GP_LEN[i0] > GP_LEN[i1] ? GP_LEN[i0] : GP_LEN[i1];
#endif
            }
            { float d = (float)pv[P_GP_VELO] / 127.0f; v->gp_lvl = vd_exp2(-d * 2.3f * (1.0f - vc)); }   /* (Sven 21.11.: the levels are the recordings' no more, evened out: VELO 127 = 14 dB from the softest to the hardest touch, on top of the recordings' 9) */
            v->sp_i = 0; v->sp_f = 0.0f; v->openv[0] = 1.0f; v->ep_lo = 0.0f; v->ck2 = -1;
            memset(v->gp_y1, 0, sizeof v->gp_y1); memset(v->gp_y2, 0, sizeof v->gp_y2); memset(v->gp_ring, 0, sizeof v->gp_ring); v->gp_dn = 0; v->ep_top = 0.0f;
        }
        if (v->type == T_CV) cv_start(v, pv, note, vel);   /* (0.41) CLAV: the string struck */
        if (v->type == T_WT) {                  /* (0.41) WAVETABLE: the position's envelope from its start, the oscillators apart */
            v->openv[0] = 0.0f; v->opst[0] = E_ATT; v->ph[0] = 0.0f; v->ph[1] = 0.37f; v->ph[2] = 0.0f;
            v->opl0[0] = v->opl1[0] = (float)pv[P_WT_POS] / 127.0f;
            v->opl0[1] = v->opl1[1] = v->opl0[2] = v->opl1[2] = 0.0f;
        }
        if (v->type == T_DX) {                  /* a new DX7 note from the sound's voice and its macros */
            uint8_t vb[DX_VOICE];
            dx_build(pv, pt[part].p->dx, vb);
            dx_init(&v->dx, vb, (int)note, (int)(vel ? vel : 1u));
            v->dx_pos = DX_N; v->dx_up = 0; v->dx_note0 = (uint8_t)note; v->dx_dcx = v->dx_dcy = 0;
            v->aenv = 1.0f; v->ast = E_SUS; v->ck2 = -1;
        }
        if (v->type == T_SYNTH) {
            ov_voice_t *o = &v->ov;
            o->target16 = (int32_t)note << 16;
            o->pitch16 = pv[P_GLIDE] ? pt[part].ov.last_pitch16 : o->target16;
            o->fe.stage = E_ATT; o->ae.stage = E_ATT;
            o->lfo_blk = 0; o->lfo_fade = pv[P_LFO_DELAY] ? 0 : 32767;
        }
    }
    voice_pan(v, pan);
    { uint32_t pos = (uint32_t)(clampi(pan, -64, 64) + 64); v->ov.gl = PAN_Q12[128u - pos]; v->ov.gr = PAN_Q12[pos]; }
}
static inline uint32_t card_lo(uint32_t part) { return (part % NPART) * NCARD; }
static uint32_t voice_alloc(uint32_t part)      /* a MEL voice: a free card of the track's eight, else the oldest released, else the oldest */
{
    uint32_t lo = card_lo(part), i, v, pick = 0xFFFFFFFFu, best = 0xFFFFFFFFu;
    for (i = 0; i < NCARD; i++) { v = lo + (rr[part] + i) % NCARD; if (vc[v].ast == E_IDLE && !vc[v].gate) { rr[part] = (v - lo + 1u) % NCARD; return v; } }
    if (part_type(part) == T_GP) {              /* (Sven 21.11.: the pedal holds every key) PIANO: the quietest goes, not the oldest */
        float q = 1e9f;
        for (v = lo; v < lo + NCARD; v++) { float a = vc[v].amp1 * (vc[v].gate ? 1.0f : 0.5f); if (a < q) { q = a; pick = v; } }
        rr[part] = (pick - lo + 1u) % NCARD;
        return pick;
    }
    for (v = lo; v < lo + NCARD; v++) if (!vc[v].gate && vc[v].seq < best) { best = vc[v].seq; pick = v; }
    if (pick == 0xFFFFFFFFu) for (v = lo; v < lo + NCARD; v++) if (vc[v].seq < best) { best = vc[v].seq; pick = v; }
    rr[part] = (pick - lo + 1u) % NCARD;
    return pick;
}
static uint32_t ov_uni_count(uint32_t part)     /* SYNTH MONO: 1 card, UNISON: VOICES cards, else 0 */
{
    uint32_t m = pt[part].p->v[P_MODE];
    return m == M_MONO ? 1u : m == M_UNISON ? (uint32_t)clampi(pt[part].p->v[P_UNI_N], 1, (int32_t)NCARD) : 0u;
}
static void ov_trigger(uint32_t part, uint32_t note, uint32_t vel)   /* OberVave's voice_trigger, on the track's eight cards */
{
    const uint8_t *pv = pt[part].p->v;
    uint32_t mode = pv[P_MODE], n, i, lo = card_lo(part), legato = 0;
    int32_t det = (pv[P_UNI_DET] * pv[P_UNI_DET]) >> 6, spread = pv[P_SPREAD];
    if (mode == M_POLY) {
        uint32_t v = voice_alloc(part);
        voice_start(&vc[v], part, note, vel, (PAN_CARD[v % NCARD] * spread) >> 7, 1);
        vc[v].ov.det8 = 0;
    } else if (mode == M_DUAL) {
        uint32_t a = voice_alloc(part), b = voice_alloc(part);
        voice_start(&vc[a], part, note, vel, (-48 * spread) >> 7, 1); vc[a].ov.det8 = -det / 2;
        voice_start(&vc[b], part, note, vel, (48 * spread) >> 7, 1); vc[b].ov.det8 = det / 2;
    } else {
        n = ov_uni_count(part);
        for (i = 0; i < n; i++) if (vc[lo + i].gate && vc[lo + i].part == part && vc[lo + i].type == T_SYNTH) legato = 1;
        for (i = 0; i < n; i++) {
            voice_t *v = &vc[lo + i];
            int32_t d = n > 1 ? (det * (int32_t)(2u * i) / (int32_t)(n - 1u)) - det : 0;
            int32_t pan = n > 1 ? ((int32_t)(128u * i) / (int32_t)(n - 1u) - 64) * spread >> 7 : 0;
            if (legato) {
                v->note = (uint8_t)note; v->ov.target16 = (int32_t)note << 16;
                if (!pv[P_GLIDE]) v->ov.pitch16 = v->ov.target16;
                v->ov.det8 = d; v->gate = 1;
                voice_pan(v, pan); { uint32_t pos = (uint32_t)(clampi(pan, -64, 64) + 64); v->ov.gl = PAN_Q12[128u - pos]; v->ov.gr = PAN_Q12[pos]; }
            } else { voice_start(v, part, note, vel, pan, 1); v->ov.det8 = d; }
        }
        for (i = n; i < NCARD; i++) vc[lo + i].gate = 0;
    }
    pt[part].ov.last_pitch16 = (int32_t)note << 16;
    if (pv[P_LFO_PHASE] == LP_SYNC && !legato) {
        for (i = lo; i < lo + NCARD; i++) if (vc[i].gate && vc[i].note != note) break;
        if (i == lo + NCARD) pt[part].lfo_ph = 0;
    }
}
static void drum_hit(uint32_t part, uint32_t drum, uint32_t vel)
{
    voice_t *v = &vc[card_lo(part) + drum];
    const uint8_t *pv = pt[part].p->v;
    voice_start(v, part, DRUM_NOTE[drum], vel, 0, 1);
    v->drum = (uint8_t)drum; v->aenv = 1.0f; v->ast = E_DEC;
    v->ph[0] = v->ph[1] = 0.0f;                 /* (the kick and the toms start clean) */
    v->penv = 1.0f; v->fenv = 1.0f;             /* (0.31: the kick's two sweeps) */
    v->hp = 0.0f; v->dc_x = v->dc_y = 0.0f;
    v->sp_i = 0; v->sp_f = 0.0f; v->sp_lp = 0.0f;
    v->ck1 = v->ck2 = -1;
    v->f1.ic1 = v->f1.ic2 = 0.0f;
    if (drum == DR_CH) { voice_t *o = &vc[card_lo(part) + DR_OH]; if (o->ast != E_IDLE) o->kill = 1; }   /* choke */
    (void)pv;
    ui_drum_hit[drum]++;
}
static void voice_trigger(uint32_t part, uint32_t note, uint32_t vel)
{
    const uint8_t *pv = pt[part].p->v;
    uint32_t ty = part_type(part), lo = card_lo(part), i, legato = 0;
    if (ty == T_MIDI) { mt_note(part, note, vel, 1); return; }   /* (0.30) only out – nothing sounds here */
    mo_note(part, note, vel, 1);
    if (ty == T_DRUMS) { drum_hit(part, drum_of_note(note), vel); return; }
    if (ty == T_SYNTH) { ov_trigger(part, note, vel); return; }
    if (ty == T_BASS || pv[P_MODE] == M_MONO) {  /* mono: a key still down slides, no retrigger (BASS: SLIDE LEGATO; ALWAYS slides every note) */
        voice_t *v = &vc[lo];
        if (v->gate && v->part == part && v->type == ty) legato = 1;
        if (legato && !(ty == T_BASS && pv[P_B_SLIDE] == BS_ALWAYS)) {
            v->note = (uint8_t)note; v->target = (float)note; v->gate = 1;
            if (!pv[P_GLIDE]) v->pitch = v->target;
        } else {
            int retrig = 1;
            voice_start(v, part, note, vel, 0, retrig);
            if (ty == T_BASS) { v->ast = pv[P_B_GATE] == BG_GATE ? E_ATT : E_DEC; if (v->aenv < 0.02f) v->ph[0] = v->ph[1] = 0.0f; if (pv[P_B_GATE] != BG_GATE) v->aenv = 1.0f; v->ck1 = -1; }   /* (0.17, Sven heard clicks: a note still sounding keeps its phase – a jump back to 0 clicked) */
        }
        for (i = lo + 1u; i < lo + NCARD; i++) if (vc[i].gate && vc[i].type != T_DRUMS) vc[i].gate = 0;   /* (POLY before: its other voices let go) */
    } else {
        uint32_t v;
        if (ty == T_GP || (ty == T_EP && pv[P_EP_CLAV]))   /* (0.41) the same key struck again: the string still ringing is stopped first */
            for (i = lo; i < lo + NCARD; i++) if (vc[i].part == part && vc[i].note == note && (vc[i].type == T_GP || vc[i].type == T_CV) && vc[i].ast != E_IDLE) { vc[i].kill = 1; vc[i].gate = 0; }
        v = voice_alloc(part);
        voice_start(&vc[v], part, note, vel, 0, 1);
        if (!pv[P_GLIDE]) vc[v].pitch = vc[v].target;
    }
    pt[part].last_pitch = (float)note;
    if (pv[P_LFO_PHASE] == LP_SYNC && !legato) {
        for (i = lo; i < lo + NCARD; i++) if (vc[i].gate && vc[i].note != note) break;
        if (i == lo + NCARD) pt[part].lfo_ph = 0;
    }
}
static void voice_release(uint32_t part, uint32_t note, uint32_t from_arp)
{
    const uint8_t *pv = pt[part].p->v;
    uint32_t ty = part_type(part), lo = card_lo(part), v;
    if (ty == T_MIDI) { mt_note(part, note, 0, 0); return; }   /* (0.30) */
    mo_note(part, note, 0, 0);
    if (ty == T_DRUMS) return;                  /* (one shots) */
    if (ty == T_SYNTH && (pv[P_MODE] == M_MONO || pv[P_MODE] == M_UNISON) && !from_arp && nheld[part] && pv[P_ARP_MODE] == A_OFF) {
        uint32_t n = ov_uni_count(part), i;
        if (vc[lo].gate && vc[lo].part == part && vc[lo].note == note) {
            for (i = 0; i < n; i++) {
                vc[lo + i].note = held[part][nheld[part] - 1u].note;
                vc[lo + i].ov.target16 = (int32_t)vc[lo + i].note << 16;
                if (!pv[P_GLIDE]) vc[lo + i].ov.pitch16 = vc[lo + i].ov.target16;
            }
            pt[part].ov.last_pitch16 = vc[lo].ov.target16;
        }
        return;
    }
    if (ty != T_SYNTH && (ty == T_BASS || pv[P_MODE] == M_MONO) && !from_arp && nheld[part] && pv[P_ARP_MODE] == A_OFF) {
        voice_t *m = &vc[lo];
        if (m->gate && m->part == part && m->note == note) {   /* a key still held takes over */
            m->note = held[part][nheld[part] - 1u].note; m->target = (float)m->note;
            if (!pv[P_GLIDE]) m->pitch = m->target;
            pt[part].last_pitch = m->target;
        }
        return;
    }
    for (v = lo; v < lo + NCARD; v++) if (vc[v].gate && vc[v].note == note && vc[v].part == part) vc[v].gate = 0;
}
static void voices_release_all(void) { for (uint32_t v = 0; v < NV; v++) { vc[v].gate = 0; vc[v].kill = 1; } for (uint32_t p = 0; p < NPART; p++) { mo_all_off(p); mt_all_off(p); } }
static void voices_release_part(uint32_t part) { for (uint32_t v = card_lo(part); v < card_lo(part) + NCARD; v++) { vc[v].gate = 0; vc[v].kill = 1; } mo_all_off(part); mt_all_off(part); }

static int seq_phase(uint32_t *ph);
static int seq_starting(void);
static void seq_tick(void);

/* the click (main: ctl_click on / off) and a one-bar count-in before a recording (main: ctl_count) */
static volatile uint32_t ctl_click, ctl_count, ui_seq_count;
static int32_t click_amp;
static uint32_t click_ph, click_inc;
static void click_start(uint32_t accent)
{
    click_amp = accent ? 9000 : 6000;
    click_ph = 0;
    click_inc = (uint32_t)((accent ? 2400.0 : 1600.0) * 4294967296.0 / FS);
}

/* ---- the arpeggiators (OberVave's, per block): one per sound, each on its own keys and its own ARP settings */
typedef struct { uint32_t cnt, period, gate_at, pos, dir, sounding, note, was_on; } arp_t;
static arp_t arp[NPART];
#ifdef OBV_TRACE
static uint32_t trace_blk;
static void obv_trace(char k, uint32_t a, uint32_t b)
{
    char m[48], t[12]; uint32_t v[3] = {trace_blk, a, b}, j = 0, x, i;
    m[j++] = k;
    for (x = 0; x < 3; x++) { uint32_t q = v[x]; i = 0; m[j++] = ' '; do { t[i++] = (char)('0' + q % 10u); q /= 10u; } while (q); while (i) m[j++] = t[--i]; }
    m[j] = 0; vibe_log(m);
}
#define TRACE(k, a, b) obv_trace(k, a, b)
#else
#define TRACE(k, a, b) ((void)0)
#endif
#define AQ 64u
typedef struct { uint8_t on, note, part, vel; uint32_t q8; } arpev_t;
static arpev_t arp_q[AQ];
static volatile uint32_t arp_qw, arp_qr;
static uint32_t seq_q8_now(uint32_t t);
static void arp_post(uint32_t p, uint32_t on, uint32_t note, uint32_t vel)
{
    if (arp_qw - arp_qr >= AQ) return;
    arpev_t *e = &arp_q[arp_qw % AQ];
    e->on = (uint8_t)on; e->note = (uint8_t)note; e->part = (uint8_t)p; e->vel = (uint8_t)vel; e->q8 = seq_q8_now(p);
    RING_PUBLISH();
    arp_qw++;
}
static uint32_t eng_t;
static void arp_stop_note(uint32_t p)
{
    arp_t *a = &arp[p];
    if (a->sounding) { voice_release(p, a->note, 1); a->sounding = 0; arp_post(p, 0, a->note, 0); }
}
static uint32_t quarter_len(void)
{
    uint32_t q;
    if (ctl_daw_quarter) return ctl_daw_quarter;
    if (ctl_ext_quarter) return ctl_ext_quarter;
    q = (60u * FS / (uint32_t)clampi(ctl_tempo, 40, 240) + 24u) / 48u * 48u;
    return q;
}
static void arp_step(uint32_t p)
{
    const uint8_t *pv = pt[p].p->v;
    arp_t *a = &arp[p];
    uint8_t sorted[NHELD];
    uint32_t i, j, n = nheld[p], oct = clampi(pv[P_ARP_OCT], 1, 4), len, k, mode = pv[P_ARP_MODE], idx;
    for (i = 0; i < n; i++) sorted[i] = held[p][i].note;
    if (mode != A_PLAY)
        for (i = 1; i < n; i++) { uint8_t t = sorted[i]; for (j = i; j && sorted[j - 1u] > t; j--) sorted[j] = sorted[j - 1u]; sorted[j] = t; }
    len = n * oct;
    if (mode == A_RND) k = xorshift(&rnd_state) % len;
    else if (mode == A_UPDN && len > 2u) { if (a->pos >= 2u * len - 2u) a->pos = 0; k = a->pos < len ? a->pos : 2u * len - 2u - a->pos; a->pos++; }
    else { if (a->pos >= len) a->pos = 0; k = mode == A_DOWN ? len - 1u - a->pos : a->pos; a->pos++; }
    idx = k % n;
    a->note = (uint32_t)clampi((int32_t)sorted[idx] + 12 * (int32_t)(k / n), 0, 127);
    for (i = 0; i < n; i++) if (held[p][i].note == sorted[idx]) break;
    TRACE('A', p, a->note);
    voice_trigger(p, a->note, i < n ? held[p][i].vel : 100u);
    arp_post(p, 1, a->note, i < n ? held[p][i].vel : 100u);
    a->sounding = 1;
    ui_arp_step++;
}
static void arp_tick(uint32_t p)
{
    const uint8_t *pv = pt[p].p->v;
    arp_t *a = &arp[p];
    uint32_t mode = pv[P_ARP_MODE], div = DIV_PER_QUARTER[pv[P_ARP_DIV] % NDIV], q;
    if (mode == A_OFF) { if (a->was_on) { arp_stop_note(p); a->was_on = 0; } return; }
    if (!a->was_on) { voices_release_part(p); a->was_on = 1; a->pos = 0; a->cnt = 0xFFFFFFFFu; }
    if (!nheld[p]) { arp_stop_note(p); a->pos = 0; a->cnt = 0xFFFFFFFFu; return; }
    q = quarter_len();
    a->period = q / div;
    if (a->period < CTL) a->period = CTL;
    a->gate_at = (a->period * (pv[P_ARP_GATE] + 1u)) >> 7;
    {
        uint32_t ph;
        if (seq_starting()) {
            if (a->sounding && a->cnt != 0xFFFFFFFFu && a->cnt < a->period / 4u) { a->cnt = 0; return; }
            a->cnt = 0; arp_stop_note(p); arp_step(p); return;
        }
        if (seq_phase(&ph)) {
            if (ph / a->period >= div) return;
            ph %= a->period;
            if (ph < CTL || (a->cnt == 0xFFFFFFFFu && ph < a->period / 4u)) { a->cnt = ph; arp_stop_note(p); arp_step(p); return; }
            if (a->cnt == 0xFFFFFFFFu) return;
            a->cnt = ph;
            if (a->sounding && a->cnt >= a->gate_at && pv[P_ARP_GATE] < 127u) arp_stop_note(p);
            return;
        }
    }
    if (a->cnt == 0xFFFFFFFFu || a->cnt + CTL >= a->period) {
        a->cnt = a->cnt == 0xFFFFFFFFu ? 0 : a->cnt + CTL - a->period;
        arp_stop_note(p); arp_step(p);
    } else {
        a->cnt += CTL;
        if (a->sounding && a->cnt >= a->gate_at && pv[P_ARP_GATE] < 127u) arp_stop_note(p);
    }
}

/* ---- note events, per sound: its held keys, its arp, its HOLD; the pedal holds them all */
static uint32_t sus_was;
static inline uint32_t part_sus(uint32_t p) { return ((ctl_sus_live | seq_ped | ctl_hold) >> p) & 1u; }
/* (0.31, Sven: "Arp auf Spur die schon aufgenommen ist tut nix") the sequencer's notes on a track with its ARP on: they are
 * the arp's held notes (phys 2 = the sequencer's), as if the keys held the step's chord – not one by one into the voices.
 * A track recorded FROM the arp (trk.pad[1]: its steps are the arp's notes already) plays them as they are. */
static int seq_feeds_arp(uint32_t p);           /* (obv_seq.h) */
static void seq_note_on(uint32_t p, uint32_t note, uint32_t vel)
{
    if (!seq_feeds_arp(p)) {                    /* (22.11.) the same note held by the pedal: struck again, it is the step's now */
        uint32_t i, j;
        for (i = j = 0; i < nheld[p]; i++) if (!(held[p][i].note == note && held[p][i].phys == 3u)) held[p][j++] = held[p][i];
        nheld[p] = j;
        voice_trigger(p, note, vel); return;
    }
    held_remove(p, note);
    if (nheld[p] == NHELD) { for (uint32_t i = 1; i < NHELD; i++) held[p][i - 1u] = held[p][i]; nheld[p]--; }
    held[p][nheld[p]].note = (uint8_t)note; held[p][nheld[p]].vel = (uint8_t)vel; held[p][nheld[p]].phys = 2;
    if (++nheld[p] == 1u) { arp[p].pos = 0; arp[p].cnt = 0xFFFFFFFFu; }
}
static void seq_note_off(uint32_t p, uint32_t note)
{
    uint32_t i, j, was = 0;
    for (i = j = 0; i < nheld[p]; i++) { if (held[p][i].note == note && held[p][i].phys == 2u) { was = 1; continue; } held[p][j++] = held[p][i]; }
    nheld[p] = j;
    if (!seq_feeds_arp(p) && part_sus(p) && part_type(p) != T_MIDI) {   /* (22.11.) the pedal down: it rings on until it is let go */
        if (nheld[p] == NHELD) { for (i = 1; i < NHELD; i++) held[p][i - 1u] = held[p][i]; nheld[p]--; }
        held[p][nheld[p]].note = (uint8_t)note; held[p][nheld[p]].vel = 0; held[p][nheld[p]].phys = 3; nheld[p]++;   /* (phys 3: the sequencer's, held by the pedal) */
        return;
    }
    if (!was || !seq_feeds_arp(p)) voice_release(p, note, 1);   /* (the arp off meanwhile: the voice it played goes too) */
}
static void ev_note_on(uint32_t p, uint32_t note, uint32_t vel)
{
    uint32_t i, arp_on = pt[p].p->v[P_ARP_MODE] != A_OFF, sus = part_sus(p);
    if (arp_on && sus) {
        for (i = 0; i < nheld[p]; i++) if (held[p][i].phys) break;
        if (i == nheld[p]) { nheld[p] = 0; arp[p].pos = 0; arp[p].cnt = 0xFFFFFFFFu; }
    }
    held_remove(p, note);
    if (nheld[p] == NHELD) { for (i = 1; i < NHELD; i++) held[p][i - 1u] = held[p][i]; nheld[p]--; }
    held[p][nheld[p]].note = (uint8_t)note; held[p][nheld[p]].vel = (uint8_t)vel; held[p][nheld[p]].phys = 1;
    nheld[p]++;
    if (arp_on) { if (nheld[p] == 1u) { arp[p].pos = 0; arp[p].cnt = 0xFFFFFFFFu; } return; }
    voice_trigger(p, note, vel);
}
static void ev_note_off(uint32_t p, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < nheld[p]; i++) if (held[p][i].note == note) held[p][i].phys = 0;
    if (part_sus(p) && !(part_type(p) == T_MIDI && pt[p].p->v[P_ARP_MODE] == A_OFF)) return;   /* (22.11.: a MIDI track sends the pedal itself, CC 64) */
    held_remove(p, note);
    if (pt[p].p->v[P_ARP_MODE] != A_OFF) return;
    voice_release(p, note, 0);
}
static void ev_sustain_up(uint32_t p)
{
    uint32_t i, j;
    uint8_t gone[NHELD], ng = 0;
    for (i = j = 0; i < nheld[p]; i++) if (held[p][i].phys == 1u || held[p][i].phys == 2u) held[p][j++] = held[p][i]; else gone[ng++] = held[p][i].note;   /* (0: let go, 3: the sequencer's, both held by the pedal) */
    nheld[p] = j;
    if (pt[p].p->v[P_ARP_MODE] != A_OFF) return;
    for (i = 0; i < ng; i++) voice_release(p, gone[i], 0);
}
static void parts_off(uint32_t p) { nheld[p] = 0; arp_stop_note(p); arp[p].was_on = 0; voices_release_part(p); }
static void engine_events(void)
{
    uint32_t sus = 0, p;
    for (p = 0; p < NPART; p++) sus |= part_sus(p) << p;
    if (__atomic_exchange_n(&ctl_panic, 0u, __ATOMIC_ACQ_REL)) { for (p = 0; p < NPART; p++) parts_off(p); voices_release_all(); }
    for (p = 0; p < NPART; p++) if ((sus_was & ~sus) >> p & 1u) ev_sustain_up(p);
    sus_was = sus;
    while (nq_r != nq_w) {
        RING_TAKE();
        nev_t e = nq[nq_r % NQ];
        p = e.part % NPART;
        nq_r++;
        if (e.type) ev_note_on(p, e.note, e.vel); else ev_note_off(p, e.note);
    }
}

/* ---- envelopes per block */
static inline void adsr_tick(float *lv, uint8_t *st, uint32_t a, uint32_t d, float sus, uint32_t r, int gate, int kill)
{
    if (gate && *st >= E_REL) *st = E_ATT;
    if (!gate && *st && *st < E_REL) *st = E_REL;
    switch (*st) {
    case E_ATT: *lv += (1.3f - *lv) * KA[a & 127u]; if (*lv >= 1.0f) { *lv = 1.0f; *st = E_DEC; } break;
    case E_DEC: *lv += (sus - *lv) * KD[d & 127u]; if (*lv <= sus + 0.0005f) { *lv = sus; *st = sus > 0.0005f ? E_SUS : E_IDLE; } break;
    case E_SUS: *lv = sus; break;
    case E_REL: *lv += (0.0f - *lv) * (kill ? 0.3f : KD[r & 127u]); if (*lv < 0.0003f) { *lv = 0.0f; *st = E_IDLE; } break;
    default: *lv = 0.0f;
    }
    if (kill && *st != E_IDLE && *st != E_REL) *st = E_REL;
}
static inline void dec_tick(float *lv, uint8_t *st, float k, int kill)   /* a one-shot decay (drums, the 808) */
{
    if (*st == E_IDLE) return;
    *lv -= *lv * (kill ? 0.5f : k);
    if (*lv < 0.0003f) { *lv = 0.0f; *st = E_IDLE; }
}

/* ---- the LFO (global per sound; a voice adds its delay / fade) */
static float lfo_shape(const part_t *g, uint32_t ph, uint32_t shape)
{
    switch (shape) {
    case L_TRI: { int32_t p = (int32_t)((ph + 0x40000000u) >> 15); return (float)(p < 65536 ? p - 32768 : 98303 - p) * (1.0f / 32768.0f); }
    case L_SAW: return 1.0f - (float)(ph >> 16) * (1.0f / 32768.0f);
    case L_RAMP: return (float)(ph >> 16) * (1.0f / 32768.0f) - 1.0f;
    case L_SQR: return ph < 0x80000000u ? 1.0f : -1.0f;
    case L_SH: return (float)(g->lfo_rnd_b >> 16) * (1.0f / 32768.0f) - 1.0f;
    default: { float a = (float)(g->lfo_rnd_a >> 16) * (1.0f / 32768.0f) - 1.0f, b = (float)(g->lfo_rnd_b >> 16) * (1.0f / 32768.0f) - 1.0f;
               return a + (b - a) * (float)(ph >> 8) * (1.0f / 16777216.0f); }
    }
}
static void lfo_tick(part_t *g)
{
    uint32_t inc = LFO_INC[g->p->v[P_LFO_RATE]], old = g->lfo_ph;
    g->lfo_ph += inc;
    if (g->lfo_ph < old) { g->lfo_rnd_a = g->lfo_rnd_b; g->lfo_rnd_b = xorshift(&rnd_state); }
    g->lfo_val = lfo_shape(g, g->lfo_ph, g->p->v[P_LFO_SHAPE]);
}
static float vol_f;                             /* the output stage: CC 7 */
static void ov_block_globals(part_t *g);
static void ov_voice_block(voice_t *vv, uint32_t card, float lfo_f);
static void ov_voice_render(voice_t *vv, int32_t *out);
/* (BOOM 0.10, Sven: more headroom) each kind's trim: measured over all 176 sounds, a kit's hit stood at 2.4 x full scale,
 * an FM chord at 1.5, a DX chord at 1.0-2.0, the 808 and the strings at 0.6 – now they sit around the same level */
static const float KIND_TRIM[NTYPE] = {0.6f, 1.0f, 0.7f, 1.0f, 0.75f, 1.0f, 0.9f, 0.0f, 1.0f, 0.3f, 2.4f, 0.5f};   /* (0.41 PIANO, CLAV: measured, as loud as the others) */   /* (0.41 WAVETABLE: measured, a note as loud as the others') */
static void block_globals(part_t *g)
{
    const uint8_t *pv = g->p->v;
    float d;
    g->type = pv[P_TYPE] % NTYPE;
    g->bend = (float)ctl_bend * (1.0f / 8192.0f) * (float)pv[P_BEND];
    d = (float)pv[P_LFO_PITCH] / 127.0f; g->lfo_pd = d * d * 12.0f;    /* up to an octave */
    d = (float)pv[P_LFO_FLT] / 127.0f; g->lfo_fd = d * d * 48.0f;      /* semitones of cutoff */
    g->lfo_ad = (float)pv[P_LFO_AMP] / 127.0f;
    d = (float)pv[P_VOLUME] / 100.0f; g->gain = d * d * KIND_TRIM[g->type];
    if (g->type == T_DX && pv[P_DX_GAIN]) g->gain *= 1.0f + (float)pv[P_DX_GAIN] * (3.0f / 127.0f);   /* (0.41) a converted MEL sound's extra level */
    if (g->type == T_SYNTH) ov_block_globals(g);
    if (g->type == T_EP) {                      /* (0.31) the tremolo: 1.5 .. 9 Hz */
        g->ep_trem += 1.5f * vd_exp2((float)pv[P_EP_RATE] * (2.6f / 127.0f)) * (float)CTL * inv_fs;
        if (g->ep_trem >= 1.0f) g->ep_trem -= 1.0f;
    }
}

/* ---- FM: the algorithms – which operators modulate which (bit i: operator i), and the carriers */
static const uint8_t ALGO_MOD[NALGO][4] = {   /* [algo][op] = the operators feeding op (bits 0..3 = op 1..4) */
    {0x2, 0x4, 0x8, 0}, {0x2, 0xC, 0, 0}, {0x6, 0, 0x8, 0}, {0xE, 0, 0, 0},
    {0x2, 0, 0x8, 0}, {0x8, 0x8, 0x8, 0}, {0, 0, 0x8, 0}, {0, 0, 0, 0},
};
static const uint8_t ALGO_CAR[NALGO] = {0x1, 0x1, 0x1, 0x1, 0x5, 0x7, 0x7, 0xF};
static const float CAR_NORM[5] = {1.0f, 1.0f, 0.72f, 0.6f, 0.52f};

/* ---- one control block for a voice: pitch, envelopes, filters, amplitude */
static inline float cut_hz(float semi) { if (semi < 0.0f) semi = 0.0f; if (semi > 135.0f) semi = 135.0f; float hz = hz_of(semi); return hz > 0.45f * fs_f ? 0.45f * fs_f : hz; }
static inline void svf_key(voice_t *v, int which, float hz, float q)   /* the coefficients, when they move */
{
    int32_t key = (int32_t)(hz * 2.0f) ^ ((int32_t)(q * 64.0f) << 20);
    if (which == 1) { if (key != v->ck1) { vd_svf_set(&v->c1, hz, q, fs_f); v->ck1 = key; } }
    else if (key != v->ck2) { vd_svf_set(&v->c2, hz, q, fs_f); v->ck2 = key; }
}
static inline float drum_ms(uint32_t v, float lo, float ratio) { return lo * vd_pow(ratio, (float)v / 127.0f); }

static void voice_block(voice_t *v, uint32_t card)
{
    part_t *g = &pt[v->part];
    const uint8_t *pv = g->p->v;
    float lfo, velg = 0.25f + 0.75f * (float)v->vel / 127.0f, a;
    /* the LFO as this voice sees it */
    {
        uint32_t ph = g->lfo_ph;
        if (pv[P_LFO_PHASE] == LP_SPREAD) ph += card << 29;
        lfo = pv[P_LFO_PHASE] == LP_SPREAD ? lfo_shape(g, ph, pv[P_LFO_SHAPE]) : g->lfo_val;
        if (v->lfo_fade < 1.0f) {
            uint32_t dl = LFO_DELAY_BLK[pv[P_LFO_DELAY]];
            if (v->lfo_blk >= dl) { v->lfo_fade += 1.0f / (float)(dl / 2u + 1u) + 0.00003f; if (v->lfo_fade > 1.0f) v->lfo_fade = 1.0f; }
            v->lfo_blk++;
        }
        lfo *= v->lfo_fade;
    }
    if (v->type == T_SYNTH) { ov_voice_block(v, card % NCARD, lfo); return; }
    if (v->type == T_DX) {                      /* (0.6) a DX7 note: its own envelopes; glide, bend, LFO as a pitch offset */
        float p, cut, a;
        int playing;
        if (v->pitch != v->target) {
            float gk = pv[P_GLIDE] ? GK[pv[P_GLIDE]] : 1.0f, diff = v->target - v->pitch;
            v->pitch += diff * gk; if (diff > -0.002f && diff < 0.002f) v->pitch = v->target;
        }
        p = v->pitch + g->bend + (float)ctl_tune8 * (1.0f / 256.0f) + lfo * g->lfo_pd - (float)v->dx_note0;
        v->dx_pitch = (int32_t)(p * (float)DX_SEMI) + dx_pitch_corr;
        if (!v->gate && !v->dx_up) { dx_keyup(&v->dx); v->dx_up = 1; }
        v->dx.bright = ((int32_t)pv[P_DX_BRIGHT] - 64) << 19;   /* the modulators' level: +- 2 octaves of gain */
        adsr_tick(&v->fenv, &v->fst, pv[P_FA], pv[P_FD], (float)pv[P_FS] / 127.0f, pv[P_FR], v->gate, v->kill);
        cut = (float)pv[P_CUTOFF] + 8.0f + v->fenv * ((float)pv[P_FENV_AMT] - 64.0f) * 1.5f + lfo * g->lfo_fd + ((float)v->note - 60.0f) * 0.3f;
        svf_key(v, 2, cut_hz(cut), 0.5f + (float)pv[P_RESO] / 127.0f * 9.0f);
        if (v->kill) v->aenv *= 0.5f;           /* (a panic: out in a few blocks) */
        playing = dx_playing(&v->dx) && v->aenv > 0.001f;
        v->amp0 = v->amp1;
        a = playing ? g->gain * v->aenv * (1.0f - (1.0f - lfo) * 0.5f * g->lfo_ad) : 0.0f;
        v->amp1 = a;
        if (!playing && v->amp0 < 0.0005f) { v->ast = E_IDLE; v->dx.on = 0; } else v->ast = E_SUS;
        ui_vlevel[card] = (int32_t)(a * 32767.0f); ui_vnote[card] = v->note;
        return;
    }
    v->amp0 = v->amp1;
    for (uint32_t i = 0; i < 6u; i++) v->inc0[i] = v->inc1[i];
    for (uint32_t i = 0; i < 4u; i++) v->opl0[i] = v->opl1[i];
    v->t += (float)CTL * inv_fs;
    if (v->type == T_DRUMS) {
        uint32_t d = v->drum;
        float hz, lvl = (float)pv[P_DL_KICK + d] * 0.01f;
        lvl *= lvl;
        if (pv[P_KIT_SRC]) {                    /* (0.13) the acoustic kit: the drum's sample, tuned (64 = as recorded, +-12), cut short by its DECAY */
            static const uint8_t AT[NDRUM] = {P_K_TUNE, P_S_TUNE, P_C_TONE, P_H_TUNE, P_H_TUNE, P_R_TUNE, P_SN_TONE, P_P_TUNE};
            static const uint8_t AD[NDRUM] = {P_K_DECAY, P_S_DECAY, P_C_DECAY, P_CH_DECAY, P_OH_DECAY, P_R_DECAY, P_SN_DECAY, P_P_DECAY};
            static const float AG[NDRUM] = {1.0f, 0.9f, 0.8f, 0.6f, 0.6f, 0.85f, 0.55f, 0.65f};
            float vel = (float)v->vel / 127.0f;
            uint32_t dv = pv[AD[d]];
            v->inc1[0] = (float)RK_SR * inv_fs * vd_exp2(((float)pv[AT[d]] - 64.0f) * (1.0f / 64.0f));
            if (v->kill) v->aenv *= 0.5f;
            else if (dv < 120u) v->aenv -= v->aenv * kblock(30.0f * vd_pow(80.0f, (float)dv / 120.0f));   /* 30 ms .. 2.4 s */
            if (v->sp_i + 2u >= (pv[P_KIT_SRC] == 2u ? HK_LEN : RK_LEN)[d] || v->aenv < 0.0005f) { v->aenv = 0.0f; v->ast = E_IDLE; }
            v->sp_k = 0.12f + 0.88f * vel * vel;   /* a soft hit is duller, as on a real drum */
            svf_key(v, 2, cut_hz((float)pv[P_CUTOFF] + 8.0f), 0.5f + (float)pv[P_RESO] / 127.0f * 6.0f);
            a = v->aenv * velg * lvl * g->gain * AG[d] * 1.5f;
            v->amp1 = a;
            ui_vlevel[card] = (int32_t)(a * 32767.0f); ui_vnote[card] = v->note;
            return;
        }
        switch (d) {
        case DR_KICK: {                         /* (0.31, Sven: "was sind das für Bassdrums?" – a sine with one sweep and no attack, 5 dB
                                                 * under the sampled kicks, the short ones without a body) now: a two-stage sweep (a fast drop
                                                 * = the punch, a slow settle = the body), the beater's click, a short hold, a longer DECAY */
            float pu = (float)pv[P_K_PUNCH] / 127.0f, f0 = 32.0f * vd_exp2((float)pv[P_K_TUNE] / 127.0f * 1.3f), oct = 1.0f + pu * 3.2f;
            v->penv -= v->penv * kblock(4.0f + pu * 5.0f);       /* the fast part: 4-9 ms */
            v->fenv -= v->fenv * kblock(28.0f + pu * 22.0f);     /* the slow part: 28-50 ms */
            hz = f0 * vd_exp2(oct * (0.7f * v->penv + 0.3f * v->fenv));
            v->inc1[0] = hz * inv_fs;
            svf_key(v, 1, 3500.0f, 1.1f);                        /* the beater: noise around 3.5 kHz */
            if (v->t > 0.010f || v->kill) dec_tick(&v->aenv, &v->ast, kblock(drum_ms(pv[P_K_DECAY], 25.0f, 40.0f)), v->kill);   /* (held 10 ms: the body stands) 25 ms .. 1 s */
            break; }
        case DR_SNARE: {
            float f1 = 120.0f * vd_exp2((float)pv[P_S_TUNE] / 127.0f * 1.5f);
            v->inc1[0] = f1 * inv_fs; v->inc1[1] = f1 * 1.6f * inv_fs;
            v->penv -= v->penv * kblock(25.0f);     /* (the body's own, shorter decay) */
            svf_key(v, 1, 500.0f * vd_exp2((float)pv[P_S_TONE] / 127.0f * 4.0f), 0.8f);
            dec_tick(&v->aenv, &v->ast, DK[pv[P_S_DECAY]], v->kill);
            break; }
        case DR_CLAP: {
            float sp = 0.005f + (float)pv[P_C_SPREAD] / 127.0f * 0.02f, e = 0.0f, t = v->t;
            for (uint32_t k = 0; k < 3u; k++) { float dt = t - (float)k * sp; if (dt >= 0.0f) e += vd_exp2(-dt * (1.4427f / 0.003f)); }
            if (t >= 3.0f * sp) e += 0.8f * vd_exp2(-(t - 3.0f * sp) * 1.4427f / (drum_ms(pv[P_C_DECAY], 30.0f, 25.0f) * 0.001f));
            if (e > 1.0f) e = 1.0f;
            v->aenv = e;
            if (t > 3.0f * sp + 1.5f * drum_ms(pv[P_C_DECAY], 30.0f, 25.0f) * 0.001f || v->kill) { v->aenv *= v->kill ? 0.3f : 1.0f; if (v->aenv < 0.001f) { v->aenv = 0.0f; v->ast = E_IDLE; } }
            svf_key(v, 1, 800.0f * vd_exp2((float)pv[P_C_TONE] / 127.0f * 2.0f), 2.5f);
            break; }
        case DR_CH: case DR_OH: {
            static const float R[6] = {1.0f, 1.4471f, 1.6170f, 1.9265f, 2.5028f, 2.6637f};
            float f0 = 220.0f * vd_exp2((float)pv[P_H_TUNE] / 127.0f * 2.0f);
            for (uint32_t k = 0; k < 6u; k++) v->inc1[k] = f0 * R[k] * inv_fs;
            svf_key(v, 1, 2500.0f * vd_exp2((float)pv[P_H_TONE] / 127.0f * 2.0f), 1.2f);
            dec_tick(&v->aenv, &v->ast, d == DR_CH ? kblock(drum_ms(pv[P_CH_DECAY], 15.0f, 20.0f) / 4.0f) : kblock(drum_ms(pv[P_OH_DECAY], 60.0f, 25.0f) / 4.0f), v->kill);
            break; }
        case DR_RIM: {
            float f1 = 400.0f * vd_exp2((float)pv[P_R_TUNE] / 127.0f * 1.5f);
            v->inc1[0] = f1 * inv_fs; v->inc1[1] = f1 * 2.3f * inv_fs;
            dec_tick(&v->aenv, &v->ast, kblock(drum_ms(pv[P_R_DECAY], 10.0f, 20.0f) / 4.0f), v->kill);
            break; }
        case DR_SNAP:
            svf_key(v, 1, 1500.0f * vd_exp2((float)pv[P_SN_TONE] / 127.0f * 2.5f), 1.0f);
            dec_tick(&v->aenv, &v->ast, kblock(drum_ms(pv[P_SN_DECAY], 8.0f, 15.0f) / 4.0f), v->kill);
            break;
        default: {                              /* PERC */
            float f0 = 80.0f * vd_exp2((float)pv[P_P_TUNE] / 127.0f * 2.5f), oct = (float)pv[P_P_BEND] / 127.0f * 2.0f;
            v->penv -= v->penv * kblock(60.0f);
            hz = f0 * vd_exp2(v->penv * oct);
            v->inc1[0] = hz * inv_fs; v->inc1[1] = hz * 1.5f * inv_fs;
            svf_key(v, 1, 300.0f * vd_exp2((float)pv[P_P_TONE] / 127.0f * 6.0f), 0.7f);
            dec_tick(&v->aenv, &v->ast, DK[pv[P_P_DECAY]], v->kill);
            break; }
        }
        /* the kit's filter (CUTOFF / RESO) */
        svf_key(v, 2, cut_hz((float)pv[P_CUTOFF] + 8.0f), 0.5f + (float)pv[P_RESO] / 127.0f * 6.0f);
        a = v->aenv * velg * lvl * g->gain;
        v->amp1 = a;
        ui_vlevel[card] = (int32_t)(a * 32767.0f); ui_vnote[card] = v->note;
        return;
    }
    if (v->type == T_GP) {                      /* (0.41) PIANO: the recording at its pitch, DECAY, the damper; above F#6 a piano has none */
        float p = (float)v->note + g->bend + (float)ctl_tune8 * (1.0f / 256.0f) + lfo * g->lfo_pd, cut;
        int damp = v->note < 90u;
        v->inc1[0] = vd_exp2((p - (float)(GP_KEY0 + GP_STEP * v->gp_key) - GP_TUNE) * (1.0f / 12.0f)) * (float)GP_SR * inv_fs;   /* (GP_TUNE: how high the recording stands, semitones) */
        if (pv[P_GP_DECAY] < 127u) {
            float tau = 0.3f * vd_pow(30.0f, (float)pv[P_GP_DECAY] / 127.0f) * vd_exp2(-((float)v->note - 60.0f) * (1.0f / 36.0f));
            v->openv[0] *= vd_exp2(-(float)CTL * inv_fs / tau * 1.4427f);
        }
        adsr_tick(&v->aenv, &v->ast, pv[P_AA], 0u, 1.0f, pv[P_AR], v->gate || !damp, v->kill);
        adsr_tick(&v->fenv, &v->fst, pv[P_FA], pv[P_FD], (float)pv[P_FS] / 127.0f, pv[P_FR], v->gate, v->kill);
        if (v->sp_i + 4u >= v->gp_len || v->openv[0] < 0.0003f) { v->aenv = 0.0f; v->ast = E_IDLE; }
        cut = (float)pv[P_CUTOFF] + 8.0f + v->fenv * ((float)pv[P_FENV_AMT] - 64.0f) * 1.5f + lfo * g->lfo_fd + ((float)v->note - 60.0f) * 0.3f;
        svf_key(v, 2, cut_hz(cut), 0.5f + (float)pv[P_RESO] / 127.0f * 9.0f);
        v->amp0 = v->amp1;
        a = v->aenv * v->openv[0] * v->gp_lvl * g->gain;
        a *= 1.0f - (1.0f - lfo) * 0.5f * g->lfo_ad;
        v->amp1 = a;
        ui_vlevel[card] = (int32_t)(a * 32767.0f); ui_vnote[card] = v->note;
        return;
    }
    if (v->type == T_CV) {                      /* (0.41) CLAV: its pitch, the damper when let go, the envelope filter */
        float p = (float)v->note + g->bend + (float)ctl_tune8 * (1.0f / 256.0f) + lfo * g->lfo_pd, cut, wah = (float)pv[P_CV_WAH] / 127.0f;
        if (!v->gate && !v->cv_rel) { v->cv_rel = 1; cv_decays(v, pv, 1); v->cv_clk += (float)pv[P_CV_CLICK] / 127.0f * 0.35f; v->cv_pitch = -1000.0f; }
        if (v->kill && v->cv_rel < 2u) { v->cv_rel = 2; for (uint32_t n = 0; n < v->cv_n; n++) { v->cv_r[n] *= 0.995f; v->cv_r2[n] = v->cv_r[n] * v->cv_r[n]; } v->cv_pitch = -1000.0f; }
        if (p - v->cv_pitch > 0.002f || v->cv_pitch - p > 0.002f) { cv_coef(v, hz_of(p)); v->cv_pitch = p; }
        v->cv_env *= vd_exp2(-(float)CTL * inv_fs / (0.04f + 0.6f * (1.0f - (float)pv[P_CV_WSPD] / 127.0f)) * 1.4427f);
        if (wah > 0.0f) svf_key(v, 1, cut_hz(54.0f + wah * v->cv_env * 70.0f), 0.8f + (float)pv[P_CV_WRES] / 127.0f * 7.0f);
        adsr_tick(&v->aenv, &v->ast, pv[P_AA], 0u, 1.0f, v->kill ? 0u : 127u, 1, v->kill);   /* (the string's own decay ends it, below) */
        adsr_tick(&v->fenv, &v->fst, pv[P_FA], pv[P_FD], (float)pv[P_FS] / 127.0f, pv[P_FR], v->gate, v->kill);
        {   /* the end: the partials' level, estimated from their decay */
            float lv = 0.0f; for (uint32_t n = 0; n < v->cv_n && n < 4u; n++) { float y = v->cv_y1[n]; lv += y * y + v->cv_y2[n] * v->cv_y2[n]; }
            if (v->t > 0.05f && lv < 1e-9f && v->cv_clk < 0.001f) { v->aenv = 0.0f; v->ast = E_IDLE; }
        }
        cut = (float)pv[P_CUTOFF] + 8.0f + v->fenv * ((float)pv[P_FENV_AMT] - 64.0f) * 1.5f + lfo * g->lfo_fd + ((float)v->note - 60.0f) * 0.3f;
        svf_key(v, 2, cut_hz(cut), 0.5f + (float)pv[P_RESO] / 127.0f * 9.0f);
        v->amp0 = v->amp1;
        a = v->aenv * g->gain * 0.55f;          /* (measured: about as loud as the tines, the chord under 0.8) */
        a *= 1.0f - (1.0f - lfo) * 0.5f * g->lfo_ad;
        v->amp1 = a;
        ui_vlevel[card] = (int32_t)(a * 32767.0f * 0.3f); ui_vnote[card] = v->note;
        return;
    }
    if (v->type == T_EP) {                      /* (0.31) E-PIANO: a struck tine or reed – FM with a falling index, a bell / bite on
                                                 * the attack, the hammer's noise; the pickup's curve and the tremolo after it */
        const int reed = pv[P_EP_MODEL] & 1u;
        float p, hz, vel = (float)v->vel / 127.0f, sens = (float)pv[P_EP_VELO] / 127.0f, velc, va, keysc, tsec, bl, bk, d, s2, t, mono, cut, kb;
        if (v->pitch != v->target) {
            float gk = pv[P_GLIDE] ? GK[pv[P_GLIDE]] : 1.0f, diff = v->target - v->pitch;
            v->pitch += diff * gk; if (diff > -0.002f && diff < 0.002f) v->pitch = v->target;
        }
        p = v->pitch + g->bend + (float)ctl_tune8 * (1.0f / 256.0f) + lfo * g->lfo_pd;
        hz = hz_of(p);
        v->inc1[0] = hz * inv_fs * v->ep_det;
        v->inc1[1] = v->inc1[0];                /* the modulator: on the fundamental (the tine's / reed's own motion) */
        v->inc1[2] = v->inc1[0] * (reed ? 3.0f : 14.0f);   /* TINE: the bell high up · REED: an odd partial, the bite */
        velc = 1.0f - sens + sens * vel;        /* the touch, as the hammer sees it */
        va = 1.0f - sens * 0.55f * (1.0f - vel);   /* (the rest of the touch is in the swing) */
        keysc = vd_exp2(-((float)v->note - 60.0f) * (1.0f / 24.0f));   /* higher notes ring shorter */
        /* the decays: the note (1 .. 12 s at C4 to silence), the index (brightness), the bell, the hammer */
        tsec = vd_pow(12.0f, (float)pv[P_EP_DECAY] / 127.0f) * (reed ? 0.7f * vd_exp2(-((float)v->note - 60.0f) * (1.0f / 36.0f)) : keysc);   /* (a reed dies less steeply up high) */
        v->openv[0] *= vd_exp2(-(float)CTL * inv_fs * 6.9f / tsec * 1.4427f);
        v->ep_fast *= vd_exp2(-(float)CTL * inv_fs / ((reed ? 0.18f : 0.28f) * (0.5f + 0.5f * keysc)) * 1.4427f);   /* the first drop after the strike */
        v->openv[1] *= vd_exp2(-(float)CTL * inv_fs / ((reed ? 0.5f : 0.3f) * (0.4f + 0.6f * keysc)) * 1.4427f);
        v->openv[2] *= vd_exp2(-(float)CTL * inv_fs / (reed ? 0.12f : 0.07f) * 1.4427f);
        v->openv[3] *= vd_exp2(-(float)CTL * inv_fs / 0.006f * 1.4427f);
        for (uint32_t i = 0; i < 4u; i++) v->opl0[i] = v->opl1[i];
        /* the index in turns: a floor (the tine keeps a little shape), the strike on top of it */
        {
            float tone = (float)pv[P_EP_TONE] / 127.0f, ix = tone * (0.12f + 0.88f * velc * velc);
            v->opl1[1] = (reed ? 0.40f : 0.24f) * ix * (0.10f + 0.90f * v->openv[1]);
        }
        /* (0.31b) the tine's swing INTO the pickup: the touch and its decay – hard and fresh it barks, ringing out it gets round */
        {
            float sw = (0.1f + 0.9f * velc * velc) * v->openv[0] * (0.62f + 0.38f * v->ep_fast);
            v->opl1[0] = sw;
        }
        bl = (float)pv[P_EP_BELL] / 127.0f * (reed ? 0.30f : 0.22f) * (0.3f + 0.7f * velc) * v->openv[2];
        if (v->inc1[2] > 0.42f) bl = 0.0f; else if (v->inc1[2] > 0.3f) bl *= (0.42f - v->inc1[2]) * (1.0f / 0.12f);   /* (no aliasing up high) */
        v->opl1[2] = bl;
        v->opl1[3] = (float)pv[P_EP_NOISE] / 127.0f * 0.35f * velc * v->openv[3];
        /* the pickup: an offset curve – soft played it is clean, hard played it barks (even harmonics) */
        bk = (float)pv[P_EP_BARK] / 127.0f * (0.4f + 0.6f * velc);
        v->ep_k = (reed ? 1.2f : 0.6f) + bk * (reed ? 6.5f : 5.5f); v->ep_b = reed ? 0.2f : 0.35f;
        v->ep_tb = vd_tanh(v->ep_k * v->ep_b); v->ep_nrm = 1.0f / (v->ep_k * (1.0f - v->ep_tb * v->ep_tb));
        d = (float)pv[P_EP_DRIVE] / 127.0f; v->ep_pre = 1.0f + d * d * 9.0f; v->ep_post = 0.5f / vd_tanh(0.5f * v->ep_pre) * (1.0f + d * 0.25f);   /* (a swing of 0.5 stays 0.5: driven it gets denser, not quieter) */
        v->ep_velc = velc;
        /* the key: attack (AMP ENV), held, then the damper (RELEASE) */
        adsr_tick(&v->aenv, &v->ast, pv[P_AA], 0u, 1.0f, pv[P_AR], v->gate, v->kill);
        adsr_tick(&v->fenv, &v->fst, pv[P_FA], pv[P_FD], (float)pv[P_FS] / 127.0f, pv[P_FR], v->gate, v->kill);
        if (v->openv[0] < 0.0003f) { v->aenv = 0.0f; v->ast = E_IDLE; }
        cut = (float)pv[P_CUTOFF] + 8.0f + v->fenv * ((float)pv[P_FENV_AMT] - 64.0f) * 1.5f + lfo * g->lfo_fd + ((float)v->note - 60.0f) * 0.3f;
        svf_key(v, 2, cut_hz(cut), 0.5f + (float)pv[P_RESO] / 127.0f * 9.0f);
        /* the tremolo: volume (the amp's) or left-right (the suitcase's) */
        d = (float)pv[P_EP_TREM] / 127.0f; s2 = (float)pv[P_EP_STEREO] / 127.0f; t = sinf_t(g->ep_trem);
        mono = 1.0f - d * (1.0f - s2) * 0.5f * (1.0f - t);
        v->gl = 1.0f - d * s2 * 0.45f * (1.0f + t); v->gr = 1.0f - d * s2 * 0.45f * (1.0f - t);
        kb = reed ? 1.45f : 1.0f;   /* (a reed swings less into its pickup: as loud as a tine) */
        v->amp0 = v->amp1;
        a = v->aenv * va * g->gain * mono * kb * 0.34f;   /* (the swing is in opl[0]: the decay too) */
        a *= 1.0f - (1.0f - lfo) * 0.5f * g->lfo_ad;
        v->amp1 = a;
        ui_vlevel[card] = (int32_t)(a * 32767.0f); ui_vnote[card] = v->note;
        return;
    }
    /* BASS and MEL: the glide */
    if (v->pitch != v->target) {
        float gk = pv[P_GLIDE] ? GK[pv[P_GLIDE]] : 1.0f, diff = v->target - v->pitch;
        v->pitch += diff * gk;
        if (diff > -0.002f && diff < 0.002f) v->pitch = v->target;
    }
    {
        float p = v->pitch + g->bend + (float)ctl_tune8 * (1.0f / 256.0f) + lfo * g->lfo_pd, fenv, cut;
        /* envelopes */
        if (v->type == T_BASS && pv[P_B_GATE] != BG_GATE) dec_tick(&v->aenv, &v->ast, KD[pv[P_AD]] * 0.5f, v->kill);
        else if (v->type == T_SMP) {            /* (0.19) a slice plays to its end (a one-shot); AR only when it is stolen */
            adsr_tick(&v->aenv, &v->ast, pv[P_AA], pv[P_AD], (float)pv[P_AS] / 127.0f, pv[P_AR], !v->sm_done && (pv[P_SM_PMODE] % 3u == 0u || v->gate), v->kill);   /* (0.22) 1-SHOT: to the end · GATE, LOOP: while the key is held */
            v->sm_loop = pv[P_SM_PMODE] % 3u == 2u && v->gate;
            if (v->sm_done && v->aenv < 0.001f) { v->aenv = 0.0f; v->ast = E_IDLE; }
        }
        else adsr_tick(&v->aenv, &v->ast, pv[P_AA], pv[P_AD], (float)pv[P_AS] / 127.0f, pv[P_AR], v->gate, v->kill);
        adsr_tick(&v->fenv, &v->fst, pv[P_FA], pv[P_FD], (float)pv[P_FS] / 127.0f, pv[P_FR], v->gate, v->kill);
        fenv = v->fenv;
        if (v->type == T_BASS) {
            float punch = (float)pv[P_B_PUNCH] / 127.0f * 36.0f;
            v->penv -= v->penv * kblock(5.0f * vd_pow(40.0f, (float)pv[P_B_PTIME] / 127.0f) / 3.0f);
            p += v->penv * punch;
            v->inc1[0] = semi_inc(p);
            v->inc1[1] = v->inc1[0] * 0.5f;     /* SUB */
            v->inc1[2] = v->inc1[0] * 2.0f;     /* TONE (the 2nd harmonic) */
        } else if (v->type == T_SMP) {          /* (0.19) SAMPLER: the slice's speed – TUNE (+-12), bend, glide, the LFO; (0.20) PLAY: the key too (C3 = as recorded) */
            float key = pv[P_SM_CHOP] % 3u ? p - v->target : p - 48.0f;   /* (KEYS MEL: C3 = as recorded) */
            key += (float)((int32_t)pv[P_SM_PITCH] - 24) + ((float)pv[P_SM_TUNE] - 64.0f) * (0.5f / 64.0f);   /* (0.22) PITCH (semitones) + FINE (+-50 ct) */
            v->inc1[0] = vd_exp2(key * (1.0f / 12.0f));
        } else if (v->type == T_STR) {          /* (0.9) STRINGS: 8', 4', 16' off one pitch */
            v->inc1[0] = semi_inc(p); v->inc1[1] = v->inc1[0] * 2.0f; v->inc1[2] = v->inc1[0] * 0.5f;
            v->inc1[3] = v->inc1[0] * 1.0041f; v->inc1[4] = v->inc1[1] * 0.9962f; v->inc1[5] = v->inc1[2] * 1.0035f;   /* (0.18) each register a second saw, ~6 cents off: a section, not one player */
        } else if (v->type == T_WT) {           /* (0.41) WAVETABLE: two oscillators and a sub, the position from its knob, envelope, LFO */
            float pos, ea;
            v->inc1[0] = semi_inc(p);
            v->inc1[1] = v->inc1[0] * vd_exp2(((float)((int32_t)(pv[P_WT_SEMI] % 24u) - 12) + (float)pv[P_WT_DET] * (0.5f / 127.0f)) * (1.0f / 12.0f));
            v->inc1[2] = v->inc1[0] * 0.5f;
            if (v->opst[0] == E_ATT) { v->openv[0] += (1.3f - v->openv[0]) * KA[pv[P_WT_PATT] & 127u]; if (v->openv[0] >= 1.0f) { v->openv[0] = 1.0f; v->opst[0] = E_DEC; } }
            else if (pv[P_WT_PDEC] < 127u) v->openv[0] -= v->openv[0] * KD[pv[P_WT_PDEC]];
            ea = ((float)pv[P_WT_PENV] - 64.0f) * (1.0f / 64.0f);
            pos = (float)pv[P_WT_POS] / 127.0f + ea * v->openv[0] + lfo * (float)pv[P_WT_LFO] * (0.5f / 127.0f);
            v->opl1[0] = pos < 0.0f ? 0.0f : pos > 1.0f ? 1.0f : pos;
            ea = (float)pv[P_WT_O2] / 127.0f; v->opl1[1] = ea * ea;
            ea = (float)pv[P_WT_SUB] / 127.0f; v->opl1[2] = ea * ea;
            v->opst[1] = (uint8_t)wt_mip(v->inc1[0]); v->opst[2] = (uint8_t)wt_mip(v->inc1[1]);
        } else {
            float base = semi_inc(p);
            for (uint32_t i = 0; i < 4u; i++) {
                uint32_t pb = P_OP1_RATIO + 4u * i;
                float l = (float)pv[pb + 1u] / 127.0f;
                v->inc1[i] = base * (float)RATIO_X100[pv[pb] % NRATIO] * 0.01f;
                /* the operator's envelope: attack, then a decay (DECAY 127: it holds) */
                if (v->opst[i] == E_ATT) { v->openv[i] += (1.3f - v->openv[i]) * KA[pv[pb + 2u]]; if (v->openv[i] >= 1.0f) { v->openv[i] = 1.0f; v->opst[i] = E_DEC; } }
                else if (v->opst[i] == E_DEC && pv[pb + 3u] < 127u) v->openv[i] -= v->openv[i] * KD[pv[pb + 3u]];
                v->opl1[i] = l * l * v->openv[i];
            }
        }
        /* the filter: cutoff in semitones, its envelope, the LFO, a little key tracking */
        cut = (float)pv[P_CUTOFF] + 8.0f + fenv * ((float)pv[P_FENV_AMT] - 64.0f) * 1.5f + lfo * g->lfo_fd + ((float)v->note - 60.0f) * 0.3f;
        svf_key(v, 2, cut_hz(cut), 0.5f + (float)pv[P_RESO] / 127.0f * 9.0f);
        a = v->aenv * velg * g->gain;
        a *= 1.0f - (1.0f - lfo) * 0.5f * g->lfo_ad;
        v->amp1 = a;
        ui_vlevel[card] = (int32_t)(a * 32767.0f); ui_vnote[card] = v->note;
    }
}

/* ---- SYNTH: OberVave's control block and samples (obv_engine.h, 3.9), on BOOM's cards */
static inline void env_tick(env_t *e, uint32_t a, uint32_t d, int32_t sus, uint32_t r, int32_t gate, int32_t vint)
{
    int32_t k, target;
    if (gate && e->stage >= E_REL) e->stage = E_ATT;
    if (!gate && e->stage && e->stage < E_REL) e->stage = E_REL;
    switch (e->stage) {
    case E_ATT:
        k = (int32_t)ENV_K_ATT[a]; k += (int32_t)(((int64_t)k * vint) >> 7);
        target = ENV_ONE + (ENV_ONE * 3 >> 3);
        e->level += (int32_t)(((int64_t)(target - e->level) * k) >> 24);
        if (e->level >= ENV_ONE) { e->level = ENV_ONE; e->stage = E_DEC; }
        break;
    case E_DEC:
        k = (int32_t)ENV_K_DEC[d]; k += (int32_t)(((int64_t)k * vint) >> 7);
        e->level += (int32_t)(((int64_t)(sus - e->level) * k) >> 24) - 16;
        if (e->level <= sus + 64) { e->level = sus; e->stage = E_SUS; }
        break;
    case E_SUS: e->level = sus; break;
    case E_REL:
        k = (int32_t)ENV_K_DEC[r]; k += (int32_t)(((int64_t)k * vint) >> 7);
        e->level += (int32_t)(((int64_t)(0 - e->level) * k) >> 24) - 32;
        if (e->level < LEVEL_FLOOR) { e->level = 0; e->stage = E_IDLE; }
        break;
    default: e->level = 0;
    }
}
static void ov_block_globals(part_t *g)
{
    const uint8_t *pv = g->p->v;
    ov_part_t *o = &g->ov;
    int32_t d;
    o->fenv_amt = (int32_t)pv[P_FENV_AMT] - 64;
    o->keytrk = pv[P_KEYTRK]; o->velflt = pv[P_VEL_FLT];
    d = pv[P_LFO_PITCH]; o->lfo_pd = (d * d) >> 7;
    d = pv[P_LFO_FLT]; o->lfo_fd = (d * d) >> 7;
    d = pv[P_LFO_PWM]; o->lfo_pwd = (d * d) >> 7;
    o->lfo_ad = pv[P_LFO_AMP];
    o->wheel_q = pv[P_MOD_DEPTH];
    o->bend8 = (ctl_bend * pv[P_BEND] * 256) >> 13;
    o->env_o2 = (int32_t)pv[P_ENV_O2] - 64;
    o->det_fine8 = ((int32_t)pv[P_O2_FINE] - 64) * 2;
    d = pv[P_O1_LVL]; o->lvl1 = (d * d) >> 7;
    d = pv[P_O2_LVL]; o->lvl2 = (d * d) >> 7;
    d = pv[P_NOISE]; o->lvln = (d * d) >> 7;
    d = pv[P_DRIVE]; o->drive_q12 = 4096 + (d * d * 15 * 4096 / 16129);
    o->cut_sm += ((pv[P_CUTOFF] << 8) - o->cut_sm) >> 6;
    o->reso_sm += ((pv[P_RESO] << 8) - o->reso_sm) >> 6;
    o->reso_k = 8192 - o->reso_sm * 8400 / (127 * 256);
    o->lad_k = (8192 - o->reso_k) >> 1;
    o->vint = pv[P_VINTAGE];
    o->sus_f = pv[P_FS] * (ENV_ONE / 127); o->sus_a = pv[P_AS] * (ENV_ONE / 127);
    d = pv[P_VOLUME]; o->gain_q12 = (d * d) >> 2;
    o->pw1_base = ((uint32_t)pv[P_O1_PW] << 25) + (1u << 24);
    o->pw2_base = ((uint32_t)pv[P_O2_PW] << 25) + (1u << 24);
    o->w1 = pv[P_O1_WAVE]; o->w2 = pv[P_O2_WAVE];
    o->ftype = pv[P_FTYPE]; o->morph_q = pv[P_FMORPH]; o->sync_on = pv[P_SYNC];
    d = pv[P_XMOD]; o->xmod_q = (uint32_t)((d * d) >> 7);
    o->glide_k = GLIDE_K[pv[P_GLIDE]];
}
static inline uint32_t pw_mod(uint32_t base, int32_t mod)
{
    int64_t p = (int64_t)base + ((int64_t)mod << 8);
    if (p < (1 << 26)) p = 1 << 26;
    if (p > (int64_t)0xFFFFFFFFu - (1 << 26)) p = (int64_t)0xFFFFFFFFu - (1 << 26);
    return (uint32_t)p;
}
static void ov_voice_block(voice_t *vv, uint32_t card, float lfo_f)
{
    const part_t *g = &pt[vv->part];
    const ov_part_t *o = &g->ov;
    const uint8_t *pv = g->p->v;
    ov_voice_t *v = &vv->ov;
    int32_t lfo, p8, p1, p2, cut, fenv, tmp, velg, trem, vint = o->vint, vt = (vint * VINT_ENV[card % NCARD]) >> 7, lfo_to, a;
    uint32_t glide_k = o->glide_k;
    int32_t gate = vv->gate && !vv->kill;
    if (v->pitch16 != v->target16) {
        if (!glide_k) v->pitch16 = v->target16;
        else { int32_t diff = v->target16 - v->pitch16; v->pitch16 += (int32_t)(((int64_t)diff * glide_k) >> 16); if (diff > -64 && diff < 64) v->pitch16 = v->target16; }
    }
    env_tick(&v->fe, pv[P_FA], pv[P_FD], o->sus_f, pv[P_FR], gate, vt);
    env_tick(&v->ae, pv[P_AA], pv[P_AD], o->sus_a, vv->kill ? 0u : pv[P_AR], gate, vt);
    fenv = v->fe.level >> 9;
    lfo = (int32_t)(lfo_f * 32767.0f);          /* (the voice's own fade: as BOOM's block already applied it) */
    v->drift += (int32_t)(xorshift(&vv->noise) & 31u) - 16;
    v->drift -= v->drift >> 11;
    p8 = (v->pitch16 >> 8) + o->bend8 + ctl_tune8 + v->det8 + ((VINT_TUNE8[card % NCARD] * vint) >> 7) + ((v->drift * vint) >> 10);
    p8 += (lfo * o->wheel_q * 4) >> 15;
    lfo_to = (lfo * o->lfo_pd * 24) >> 15;
    p1 = p8 + (pv[P_LFO_PITCH_TO] != TO_O2 ? lfo_to : 0);
    p2 = p8 + (pv[P_LFO_PITCH_TO] != TO_O1 ? lfo_to : 0) + ((int32_t)pv[P_O2_SEMI] - 24) * 256 + o->det_fine8 + ((fenv * o->env_o2 * 97) >> 15);
    v->inc1 = pitch_inc(p1); v->inc2 = pitch_inc(p2);
    tmp = (lfo * o->lfo_pwd);
    v->pw1 = pw_mod(o->pw1_base, pv[P_LFO_PWM_TO] != TO_O2 ? tmp : 0);
    v->pw2 = pw_mod(o->pw2_base, pv[P_LFO_PWM_TO] != TO_O1 ? tmp : 0);
    cut = o->cut_sm + 8 * 256 + ((fenv * o->fenv_amt * 390) >> 15)
        + ((int32_t)vv->note - 60) * o->keytrk * 2 + (((int32_t)vv->vel - 64) * o->velflt >> 1)
        + ((lfo * o->lfo_fd * 97) >> 15) + ((lfo * o->wheel_q * 48) >> 15) + ((VINT_CUT8[card % NCARD] * vint) >> 7) + ((v->drift * vint) >> 7);
    if (o->ftype == F_LP24) lad_coef(&v->lc, cut, o->lad_k); else svf_coef(&v->c1, cut, o->reso_k);
    velg = 32767 - ((((127 - (int32_t)vv->vel) * (int32_t)pv[P_VEL_AMP]) >> 7) * 258);
    trem = 32767 - (((32767 - lfo) * o->lfo_ad) >> 8);
    v->amp0 = v->amp1;
    a = (int32_t)(((int64_t)(v->ae.level >> 9) * velg >> 15) * trem >> 15);
    v->amp1 = (a * o->gain_q12) >> 12;
    ui_vlevel[card] = a; ui_vnote[card] = vv->note;
    vv->ast = (uint8_t)v->ae.stage;
}
static void ov_voice_render(voice_t *vv, int32_t *out)
{
    ov_voice_t *v = &vv->ov;
    uint32_t i, inc1 = v->inc1, inc2 = v->inc2, pw1 = v->pw1, pw2 = v->pw2, d1 = inc1 >> 15, d2 = inc2 >> 15;
    int32_t amp0 = v->amp0, da = v->amp1 - v->amp0, gl = v->gl, gr = v->gr, o2n = v->o2.prev;
    const ov_part_t *g = &pt[vv->part].ov;
    int32_t l1 = g->lvl1, l2 = g->lvl2, ln = g->lvln, drv = g->drive_q12, morph_q = g->morph_q;
    uint32_t ft = g->ftype, xq = g->xmod_q, sy = g->sync_on, ww1 = g->w1, ww2 = g->w2;
    int32_t hw1 = (ww1 & OSC_SAW ? -65536 : 0) + (ww1 & OSC_PLS ? -65536 : 0);
    int32_t hw2 = (ww2 & OSC_SAW ? -65536 : 0) + (ww2 & OSC_PLS ? -65536 : 0);
    for (i = 0; i < CTL; i++) {
        int32_t s1, s2, mix, lp, bp, hp, y, amp, rt = -1;
        uint32_t ie = inc1;
        if (xq) {
            int32_t m = (int32_t)(inc1 >> 8 > 0x200000u ? 0x200000u : inc1 >> 8) * (((o2n >> 8) * (int32_t)xq) >> 6);
            m += (int32_t)inc1;
            ie = m < (int32_t)(inc1 >> 4) ? inc1 >> 4 : (uint32_t)m;
            d1 = ie >> 15;
        }
        s1 = osc_step(&v->o1, ie, d1, pw1, ww1, hw1, -1);
        if (sy && v->o1.ph < ie && d1) rt = (int32_t)(v->o1.ph / d1);
        s2 = osc_step(&v->o2, inc2, d2, pw2, ww2, hw2, rt);
        o2n = v->o2.prev;
        mix = s1 * l1 + s2 * l2;
        if (ln) mix += ((int32_t)(xorshift(&vv->noise) >> 16) - 32768) * ln;
        mix >>= 7;
        mix = sat_in(((mix >> 3) * drv) >> 9);
        if (ft == F_LP24) y = lad_step(&v->lc, &v->lad, mix);
        else {
            svf_step(&v->c1, &v->f1, mix, &lp, &bp, &hp);
            switch (ft) {
            case F_HP12: y = hp; break;
            case F_BP12: y = bp; break;
            case F_NOTCH: y = hp + lp; break;
            case F_SEM: y = lp + (((hp - lp) * morph_q) >> 7); break;
            default: y = lp; break;
            }
        }
        y = sat_knee(y);
        amp = amp0 + ((da * (int32_t)i) >> 4);
        y = (y * amp) >> 16;
        out[2u * i] += (y * gl) >> 12;
        out[2u * i + 1u] += (y * gr) >> 12;
    }
    (void)o2n;
}

/* ---- the samples of one voice */
static inline float drive_f(float x, uint32_t ty, float pre)
{
    x *= pre;
    if (ty == BD_HARD) return x > 1.0f ? 1.0f : x < -1.0f ? -1.0f : x;
    if (ty == BD_FOLD) { float f = x * 0.25f + 0.25f; f -= (float)(int32_t)f; if (f < 0.0f) f += 1.0f; return 4.0f * vd_absf(f - 0.5f) - 1.0f; }
    return vd_tanh(x);
}
static void voice_render(voice_t *v, int32_t *out)
{
    const part_t *g = &pt[v->part];
    const uint8_t *pv = g->p->v;
    float amp = v->amp0, da = (v->amp1 - v->amp0) * (1.0f / (float)CTL), gl = v->gl, gr = v->gr;
    float inc[6], dinc[6];
    uint32_t filt = pv[P_CUTOFF] < 127u || pv[P_RESO] > 4u || pv[P_FENV_AMT] != 64u || pv[P_LFO_FLT] || v->type == T_MEL;
    if (v->type == T_SYNTH) { ov_voice_render(v, out); return; }
    if (v->type == T_DX) {                      /* 32 frames at a time, played over two control blocks */
        if (v->dx_pos >= DX_N) {
            memset(v->dx_buf, 0, sizeof v->dx_buf);
            if (v->dx.on) dx_compute(&v->dx, v->dx_buf, v->dx_pitch);
            v->dx_pos = 0;
        }
        for (uint32_t i = 0; i < CTL; i++) {
            int32_t s = clampi(v->dx_buf[v->dx_pos + i] >> 12, -65535, 65535), yi;   /* (one carrier at full level: 8192) */
            float y;
            yi = s - v->dx_dcx + (int32_t)(((int64_t)v->dx_dcy * 32610) >> 15); v->dx_dcx = s;   /* a DC blocker, ~10 Hz: FM with feedback is not symmetric */
            v->dx_dcy = yi = clampi(yi, -131071, 131071);
            y = (float)yi * (1.1f / 32768.0f);
            if (filt) y = vd_svf_tick(&v->f2, &v->c2, y).lp;
            y *= amp; amp += da;
            out[2u * i] += (int32_t)(y * gl * 32767.0f);
            out[2u * i + 1u] += (int32_t)(y * gr * 32767.0f);
        }
        v->dx_pos = (uint8_t)(v->dx_pos + CTL);
        return;
    }
    for (uint32_t i = 0; i < 6u; i++) { inc[i] = v->inc0[i]; dinc[i] = (v->inc1[i] - v->inc0[i]) * (1.0f / (float)CTL); }
    if (v->type == T_DRUMS && pv[P_KIT_SRC]) {  /* (0.13) an acoustic drum: its sample, read between two points */
        uint32_t d = v->drum % NDRUM, hh = pv[P_KIT_SRC] == 2u, len = hh ? HK_LEN[d] : RK_LEN[d];   /* (0.27: HIP HOP's own table) */
        const int16_t *s = hh ? HK_DATA + HK_AT[d] : RK_DATA + RK_AT[d];
        float k = v->sp_k;
        for (uint32_t i = 0; i < CTL; i++) {
            float y = 0.0f;
            if (v->sp_i + 1u < len) { float p0 = (float)s[v->sp_i], p1 = (float)s[v->sp_i + 1u]; y = (p0 + (p1 - p0) * v->sp_f) * (1.0f / 32768.0f); }
            v->sp_f += inc[0]; inc[0] += dinc[0];
            while (v->sp_f >= 1.0f) { v->sp_f -= 1.0f; v->sp_i++; }
            v->sp_lp += (y - v->sp_lp) * k; y = v->sp_lp;
            if (filt) y = vd_svf_tick(&v->f2, &v->c2, y).lp;
            y *= amp; amp += da;
            out[2u * i] += (int32_t)(y * gl * 32767.0f);
            out[2u * i + 1u] += (int32_t)(y * gr * 32767.0f);
        }
        return;
    }
    if (v->type == T_DRUMS) {
        uint32_t d = v->drum;
        float drv = d == DR_KICK ? (float)pv[P_K_DRIVE] / 127.0f : d == DR_CLAP ? (float)pv[P_C_DRIVE] / 127.0f : 0.0f;
        float pre = 1.0f + drv * 8.0f, post = 1.0f / (1.0f + drv * 1.5f), snap = (float)pv[P_S_SNAP] / 127.0f;
        const float kclick = (0.18f + 0.75f * (float)pv[P_K_PUNCH] / 127.0f) * (0.4f + 0.6f * (float)v->vel / 127.0f);   /* (0.31) the kick's click: PUNCH, how hard */
        for (uint32_t i = 0; i < CTL; i++) {
            float y = 0.0f, n;
            switch (d) {
            case DR_KICK: {                     /* (0.31) body + the beater's click, then the drive */
                float ts = v->t - (float)(CTL - i) * inv_fs, ck = ts < 0.012f ? vd_exp2(-ts * (1.4427f / 0.0018f)) : 0.0f;
                y = sinf_t(v->ph[0]);
                if (ck > 0.002f) y += vd_svf_tick(&v->f1, &v->c1, noise_f(&v->noise)).bp * ck * kclick;
                if (drv > 0.0f) y = vd_tanh(y * pre) * post * (1.0f + drv * 0.5f);
                y *= 1.75f;                     /* (+5 dB: as loud as the sampled kicks) */
                break; }
            case DR_SNARE: {
                float body = (sinf_t(v->ph[0]) * 0.6f + sinf_t(v->ph[1]) * 0.35f) * v->penv;
                n = vd_svf_tick(&v->f1, &v->c1, noise_f(&v->noise)).hp;
                y = body + n * (0.4f + snap * 1.1f);
                break; }
            case DR_CLAP:
                n = vd_svf_tick(&v->f1, &v->c1, noise_f(&v->noise)).bp * 2.5f;
                y = drv > 0.0f ? vd_tanh(n * pre) * post : n;
                break;
            case DR_CH: case DR_OH: {
                float s = 0.0f;
                for (uint32_t k = 0; k < 6u; k++) s += v->ph[k] < 0.5f ? 1.0f : -1.0f;
                y = vd_svf_tick(&v->f1, &v->c1, s * (1.0f / 6.0f)).hp * 1.4f;
                break; }
            case DR_RIM:
                y = (sinf_t(v->ph[0]) + 0.6f * sinf_t(v->ph[1])) * 0.7f;
                if (v->t < 0.001f) y += noise_f(&v->noise) * 0.3f;
                break;
            case DR_SNAP:
                y = vd_svf_tick(&v->f1, &v->c1, noise_f(&v->noise)).hp * 1.3f;
                break;
            default:
                y = vd_svf_tick(&v->f1, &v->c1, sinf_t(v->ph[0]) * 0.7f + sinf_t(v->ph[1]) * 0.4f).lp;
                break;
            }
            for (uint32_t k = 0; k < 6u; k++) { v->ph[k] += inc[k]; inc[k] += dinc[k]; if (v->ph[k] >= 1.0f) v->ph[k] -= 1.0f; }
            if (filt) y = vd_svf_tick(&v->f2, &v->c2, y).lp;
            y *= amp; amp += da;
            out[2u * i] += (int32_t)(y * gl * 32767.0f);
            out[2u * i + 1u] += (int32_t)(y * gr * 32767.0f);
        }
        return;
    }
    if (v->type == T_BASS) {
        float tone = (float)pv[P_B_TONE] / 127.0f * 0.5f, sub = (float)pv[P_B_SUB] / 127.0f * 0.8f, click = (float)pv[P_B_CLICK] / 127.0f * 0.6f;
        float drv = (float)pv[P_B_DRIVE] / 127.0f, pre = 1.0f + drv * 20.0f, post = 1.0f / (1.0f + drv * 3.0f);
        uint32_t dty = pv[P_B_DTYPE] % NBDT;
        for (uint32_t i = 0; i < CTL; i++) {
            float y = sinf_t(v->ph[0]) + tone * sinf_t(v->ph[2]) + sub * sinf_t(v->ph[1]);
            if (v->t < 0.004f && click > 0.0f) {   /* (0.17, Sven heard clicks) the attack: low-passed noise, a smooth 4 ms fall – raw noise read as a glitch */
                float u = 1.0f - v->t * 250.0f;
                v->hp += (noise_f(&v->noise) - v->hp) * 0.35f;
                y += v->hp * click * 0.6f * u * u;
            }
            y *= amp; amp += da;
            if (drv > 0.0f) y = drive_f(y, dty, pre) * post;
            { float dc = y - v->dc_x + 0.998f * v->dc_y; v->dc_x = y; v->dc_y = dc; y = dc; }   /* (no DC after the drive) */
            if (filt) y = vd_svf_tick(&v->f2, &v->c2, y).lp;
            for (uint32_t k = 0; k < 3u; k++) { v->ph[k] += inc[k]; inc[k] += dinc[k]; if (v->ph[k] >= 1.0f) v->ph[k] -= 1.0f; }
            out[2u * i] += (int32_t)(y * gl * 32767.0f);
            out[2u * i + 1u] += (int32_t)(y * gr * 32767.0f);
        }
        return;
    }
    if (v->type == T_STR) {                     /* (0.9) band-limited saws: 8', 4', 16' – (0.18) two each, detuned */
        const float lv[3] = {(float)pv[P_ST_8] * (0.16f / 127.0f), (float)pv[P_ST_4] * (0.13f / 127.0f), (float)pv[P_ST_16] * (0.17f / 127.0f)};
        if (v->t < 0.01f && v->ph[3] == 0.0f) { v->ph[3] = 0.37f; v->ph[4] = 0.71f; v->ph[5] = 0.13f; }   /* (the pair starts apart: no phasey attack) */
        for (uint32_t i = 0; i < CTL; i++) {
            float y = 0.0f;
            for (uint32_t k = 0; k < 6u; k++) {
                float t = v->ph[k], dt = inc[k], s = 2.0f * t - 1.0f;
                if (dt > 0.0f) {
                    if (t < dt) { float u = t / dt; s -= u + u - u * u - 1.0f; }
                    else if (t > 1.0f - dt) { float u = (t - 1.0f) / dt; s -= u * u + u + u + 1.0f; }
                }
                y += s * lv[k % 3u];
                v->ph[k] += dt; inc[k] += dinc[k]; if (v->ph[k] >= 1.0f) v->ph[k] -= 1.0f;
            }
            if (filt) y = vd_svf_tick(&v->f2, &v->c2, y).lp;
            y *= amp; amp += da;
            out[2u * i] += (int32_t)(y * gl * 32767.0f);
            out[2u * i + 1u] += (int32_t)(y * gr * 32767.0f);
        }
        return;
    }
    if (v->type == T_GP) {                      /* (0.41) PIANO: two recordings in stereo, read between their samples (cubic), mixed by the touch */
        const float tk = ((float)pv[P_GP_TONE] - 64.0f) * (1.0f / 64.0f), wd = (float)pv[P_GP_SPREAD] * (1.0f / 100.0f);
        const float lo_k = 1.0f - vd_exp2(-6.2832f * 1400.0f * inv_fs * 1.4427f);
        const uint32_t len = v->gp_len;
        const float dk_k = 1.0f - 1.0f / (0.002f * fs_f);   /* (the taken-over voice's fade) */
        for (uint32_t i = 0; i < CTL; i++) {
            float y[2] = {0.0f, 0.0f}, m, sd;
            uint32_t n = v->sp_i;
            if (n + 3u < len) {
                float f = v->sp_f;
                while (v->gp_dn <= n + 2u) gp_next(v);   /* (decoded in order, a few ahead) */
                for (uint32_t c = 0; c < GP_NCH; c++) {
                    float s0 = n ? v->gp_ring[c][(n - 1u) & 7u] : 0.0f, s1 = v->gp_ring[c][n & 7u], s2 = v->gp_ring[c][(n + 1u) & 7u], s3 = v->gp_ring[c][(n + 2u) & 7u];
                    float c1 = 0.5f * (s2 - s0), c2 = s0 - 2.5f * s1 + 2.0f * s2 - 0.5f * s3, c3 = 0.5f * (s3 - s0) + 1.5f * (s1 - s2);   /* (Catmull-Rom) */
                    y[c] = (((c3 * f + c2) * f + c1) * f + s1) * (1.0f / 32768.0f);
                }
                v->sp_f += inc[0]; inc[0] += dinc[0];
                while (v->sp_f >= 1.0f) { v->sp_f -= 1.0f; v->sp_i++; }
            }
            v->ep_lo += (y[0] - v->ep_lo) * lo_k; y[0] += tk * (y[0] - v->ep_lo);   /* TONE: the top lifted or taken away, */
            v->ep_top += (y[1] - v->ep_top) * lo_k; y[1] += tk * (y[1] - v->ep_top);   /*   each channel */
            if (filt) { y[0] = vd_svf_tick(&v->f2, &v->c2, y[0]).lp; y[1] = vd_svf_tick(&v->f1, &v->c2, y[1]).lp; }
            m = 0.5f * (y[0] + y[1]); sd = 0.5f * (y[0] - y[1]) * wd;   /* SPREAD: the width (100 = as recorded) */
            {   float ol = (m + sd) * amp * gl, orr = (m - sd) * amp * gr;
                v->gp_last[0] = ol + v->gp_dk[0]; v->gp_last[1] = orr + v->gp_dk[1];
                out[2u * i] += (int32_t)(v->gp_last[0] * 32767.0f);
                out[2u * i + 1u] += (int32_t)(v->gp_last[1] * 32767.0f);
                v->gp_dk[0] *= dk_k; v->gp_dk[1] *= dk_k;
            }
            amp += da;
        }
        return;
    }
    if (v->type == T_CV) {                      /* (0.41) CLAV: the partials summed as the pickups hear them, the click, the wah */
        const uint32_t np = v->cv_n;
        const float wah = (float)pv[P_CV_WAH] / 127.0f, wm = wah * 2.5f > 1.0f ? 1.0f : wah * 2.5f, wg = 1.7f / vd_sqrt(0.8f + (float)pv[P_CV_WRES] / 127.0f * 7.0f);
        for (uint32_t i = 0; i < CTL; i++) {
            float x = 0.0f, y;
            for (uint32_t n = 0; n < np; n++) {
                float yn = v->cv_c[n] * v->cv_y1[n] - v->cv_r2[n] * v->cv_y2[n];
                v->cv_y2[n] = v->cv_y1[n]; v->cv_y1[n] = yn;
                x += yn * v->cv_g[n];
            }
            if (v->cv_clk > 0.0005f) {            /* the tangent's thump on the strike, the yarn's pluck when let go */
                v->hp += (noise_f(&v->noise) - v->hp) * 0.3f;
                x += v->hp * v->cv_clk * 0.25f; v->cv_clk *= 0.996f;
            }
            if (wah > 0.0f) { vd_svf_out o = vd_svf_tick(&v->f1, &v->c1, x); x = x * (1.0f - wm) + o.bp * wg * wm + o.lp * 0.25f * wm; }   /* (the band's gain is its Q: taken back, half) */
            y = x;
            if (filt) y = vd_svf_tick(&v->f2, &v->c2, y).lp;
            y *= amp; amp += da;
            out[2u * i] += (int32_t)(y * gl * 32767.0f);
            out[2u * i + 1u] += (int32_t)(y * gr * 32767.0f);
        }
        return;
    }
    if (v->type == T_EP) {                      /* (0.31) E-PIANO */
        float sw = v->opl0[0], dsw = (v->opl1[0] - v->opl0[0]) * (1.0f / (float)CTL);
        float ix = v->opl0[1], dix = (v->opl1[1] - v->opl0[1]) * (1.0f / (float)CTL);
        float bl = v->opl0[2], dbl = (v->opl1[2] - v->opl0[2]) * (1.0f / (float)CTL);
        float nz = v->opl0[3], dnz = (v->opl1[3] - v->opl0[3]) * (1.0f / (float)CTL);
        const float k = v->ep_k, bb = v->ep_b, tb = v->ep_tb, nrm = v->ep_nrm, pre = v->ep_pre, post = v->ep_post;
        const int drv = pre > 1.01f;
        const float lo_k = 180.0f * 6.2832f * inv_fs, top_k = 1.0f - vd_exp2(-6.2832f * 7000.0f * inv_fs * 1.4427f);
        for (uint32_t i = 0; i < CTL; i++) {
            float x = sinf_t(v->ph[0] + ix * sinf_t(v->ph[1])) + bl * sinf_t(v->ph[2]), y;
            v->hp += (noise_f(&v->noise) - v->hp) * 0.25f;
            x = x * sw + v->hp * nz;
            y = (vd_tanh(k * (x + bb)) - tb) * nrm;   /* the pickup (unity for small signals, its DC taken below) */
            { float dc = y - v->dc_x + 0.998f * v->dc_y; v->dc_x = y; v->dc_y = dc; y = dc; }
            v->ep_lo += (y - v->ep_lo) * lo_k; y += v->ep_lo * 0.7f;         /* the body: +4.6 dB under ~180 Hz – the depth */
            v->ep_top += (y - v->ep_top) * top_k; y = v->ep_top;              /* the pickup's top: soft above ~7 kHz */
            if (drv) y = vd_tanh(y * pre) * post;
            if (filt) y = vd_svf_tick(&v->f2, &v->c2, y).lp;
            for (uint32_t q = 0; q < 3u; q++) { v->ph[q] += inc[q]; inc[q] += dinc[q]; if (v->ph[q] >= 1.0f) v->ph[q] -= 1.0f; }
            ix += dix; bl += dbl; nz += dnz; sw += dsw;
            y *= amp; amp += da;
            out[2u * i] += (int32_t)(y * gl * 32767.0f);
            out[2u * i + 1u] += (int32_t)(y * gr * 32767.0f);
        }
        return;
    }
    if (v->type == T_SMP) {                     /* (0.19) SAMPLER: the slice from the take, then its QUALITY (rate and bits) */
        static const float QR[4] = {0.0f, 26040.0f, 40000.0f, 11025.0f}, QL[4] = {32768.0f, 2048.0f, 2048.0f, 64.0f};
        uint32_t hi = 0, st = 0, q = pv[P_SM_QUAL] & 3u;
        const int16_t *d = sm_take(pv[P_SM_SRC] % 9u ? pv[P_SM_SRC] % 9u - 1u : v->part, &hi, &st);
        float qinc = QR[q] * inv_fs, ql = QL[q];
        if (v->sm_b > hi) v->sm_b = hi;         /* (the take got shorter: a new one) */
        for (uint32_t i = 0; i < CTL; i++) {
            float x = 0.0f, y;
            if (!v->sm_done && d && v->sm_i + 1u < hi) {
                uint32_t a = v->sm_i, b = v->sm_rev ? (a ? a - 1u : 0u) : a + 1u, rem;
                float s0 = st ? ((float)d[2u * a] + (float)d[2u * a + 1u]) * 0.5f : (float)d[a];
                float s1 = st ? ((float)d[2u * b] + (float)d[2u * b + 1u]) * 0.5f : (float)d[b];
                x = (s0 + (s1 - s0) * v->sm_f) * (1.0f / 32768.0f);
                rem = v->sm_rev ? a - v->sm_a : v->sm_b - a;
                if (rem < 96u) x *= (float)rem * (1.0f / 96.0f);           /* (the cut's ends fade: no click) */
                if (v->sm_age < 32u) x *= (float)v->sm_age * (1.0f / 32.0f);
                v->sm_age++;
                v->sm_f += inc[0]; inc[0] += dinc[0];
                while (v->sm_f >= 1.0f) {
                    v->sm_f -= 1.0f;
                    if (v->sm_rev) { if (v->sm_i <= v->sm_a + 1u) { if (v->sm_loop) { v->sm_i = v->sm_b - 2u; v->sm_age = 0; } else { v->sm_done = 1; break; } } else v->sm_i--; }
                    else { v->sm_i++; if (v->sm_i + 1u >= v->sm_b) { if (v->sm_loop) { v->sm_i = v->sm_a; v->sm_age = 0; } else { v->sm_done = 1; break; } } }   /* (LOOP: round again, faded at the seam) */
                }
            } else v->sm_done = 1;
            if (q) {                            /* (0.23, Sven: "QUALITY macht keinen Unterschied") held at the old rate with NO filter: */
                v->sm_acc += qinc;                /*   the aliasing is the sound. SP 12: 26 kHz, 12 bit, driven a little · MPC: 40 kHz, */
                if (v->sm_acc >= 1.0f) {          /*   12 bit, a touch smoothed · LO-FI: 12 kHz, 8 bit, raw */
                    float u = q == 1u ? vd_tanh(x * 1.8f) * 0.62f : q == 3u ? x * 1.2f : x;
                    v->sm_acc -= 1.0f; v->sm_hold = (float)(int32_t)(u * ql + (u < 0.0f ? -0.5f : 0.5f)) / ql;
                }
                if (q == 2u) { v->sm_lp += (v->sm_hold - v->sm_lp) * 0.6f; x = v->sm_lp; } else x = v->sm_hold;
            }
            y = x;
            if (filt) y = vd_svf_tick(&v->f2, &v->c2, y).lp;
            y *= amp; amp += da;
            out[2u * i] += (int32_t)(y * gl * 32767.0f);
            out[2u * i + 1u] += (int32_t)(y * gr * 32767.0f);
        }
        return;
    }
    if (v->type == T_WT) {                      /* (0.41) WAVETABLE: two frames crossfaded by the position, each oscillator at its level */
        const float (*tb)[WT_MIP][WT_LEN + 1u] = wt_tab[pv[P_WT_TABLE] % NWT];
        const uint32_t m1 = v->opst[1] % WT_MIP, m2 = v->opst[2] % WT_MIP;
        float pos = v->opl0[0], dpos = (v->opl1[0] - v->opl0[0]) * (1.0f / (float)CTL);
        float l2 = v->opl0[1], dl2 = (v->opl1[1] - v->opl0[1]) * (1.0f / (float)CTL), ls = v->opl0[2], dls = (v->opl1[2] - v->opl0[2]) * (1.0f / (float)CTL);
        for (uint32_t i = 0; i < CTL; i++) {
            float fp = pos * (float)(WT_FR - 1u), fr; uint32_t f0 = (uint32_t)fp, f1;
            if (f0 >= WT_FR - 1u) f0 = WT_FR - 2u;
            fr = fp - (float)f0; f1 = f0 + 1u;
            float y = wt_read(tb[f0][m1], tb[f1][m1], fr, v->ph[0]);
            if (l2 > 0.0001f) y += l2 * wt_read(tb[f0][m2], tb[f1][m2], fr, v->ph[1]);
            if (ls > 0.0001f) y += ls * sinf_t(v->ph[2]);
            for (uint32_t k = 0; k < 3u; k++) { v->ph[k] += inc[k]; inc[k] += dinc[k]; if (v->ph[k] >= 1.0f) v->ph[k] -= 1.0f; }
            pos += dpos; l2 += dl2; ls += dls;
            if (filt) y = vd_svf_tick(&v->f2, &v->c2, y).lp;
            y *= amp; amp += da;
            out[2u * i] += (int32_t)(y * gl * 32767.0f);
            out[2u * i + 1u] += (int32_t)(y * gr * 32767.0f);
        }
        return;
    }
    {   /* MEL: 4-op FM */
        uint32_t algo = pv[P_ALGO] % NALGO, car = ALGO_CAR[algo], nc = 0;
        const uint8_t *mod = ALGO_MOD[algo];
        float fb = (float)pv[P_FB] / 127.0f * 0.4f, opl[4], dopl[4], norm;
        for (uint32_t k = 0; k < 4u; k++) { nc += car >> k & 1u; opl[k] = v->opl0[k]; dopl[k] = (v->opl1[k] - v->opl0[k]) * (1.0f / (float)CTL); }
        norm = CAR_NORM[nc];
        for (uint32_t i = 0; i < CTL; i++) {
            float o[4], y = 0.0f;
            for (int32_t k = 3; k >= 0; k--) {
                float m = 0.0f;
                uint32_t mm = mod[k];
                if (mm & 2u) m += o[1]; if (mm & 4u) m += o[2]; if (mm & 8u) m += o[3];
                if (k == 3 && fb > 0.0f) m += (v->fb_a + v->fb_b) * 0.5f * fb;
                o[k] = sinf_t(v->ph[k] + m);
                if (car >> k & 1u) y += o[k] * opl[k];
                o[k] *= opl[k] * 0.8f;          /* as a modulator: turns of the next phase */
            }
            v->fb_b = v->fb_a; v->fb_a = o[3];
            for (uint32_t k = 0; k < 4u; k++) { v->ph[k] += inc[k]; inc[k] += dinc[k]; opl[k] += dopl[k]; if (v->ph[k] >= 1.0f) v->ph[k] -= 1.0f; }
            y *= norm;
            if (filt) y = vd_svf_tick(&v->f2, &v->c2, y).lp;
            y *= amp; amp += da;
            out[2u * i] += (int32_t)(y * gl * 32767.0f);
            out[2u * i + 1u] += (int32_t)(y * gr * 32767.0f);
        }
    }
}

/* the mixer: each sound into its own bus, then by its level and pan to the output, and by its sends into the effects */
static int32_t bus[NPART][2u * HALF_FRAMES], snd_c[HALF_FRAMES], snd_d[HALF_FRAMES], snd_r[HALF_FRAMES], snd_v[HALF_FRAMES];
static volatile uint32_t ui_trk_peak[NPART];
/* (BOOM 0.9) the STRINGS' ensemble, one per track (Shine On's): the track's mono sum on a line, three taps around 6 ms
 * swayed by a slow sine (0.6 Hz, +-3.9 ms) and a fast one (6 Hz, a ninth of it), a third of a turn apart – tap 1 left,
 * tap 3 right, tap 2 on both. ENSEMBLE: how much of the lines (full: lines only, as the originals) and how deep */
#define ENS_N 2048u
typedef struct { float line[ENS_N]; uint32_t w; float ps, pf, d[3]; vd_svf hp[2], body[2]; float top[2]; } ens_t;
static vd_svf_c ens_hp_c, ens_body_c; static float ens_top_k; static int ens_eq_fs;   /* (0.18) the section's voicing */
static ens_t ens[NPART];
static inline float ens_read(const ens_t *e, float back)
{
    uint32_t b = (uint32_t)back; float f = back - (float)b;
    float p0 = e->line[(e->w - b + 1u) & (ENS_N - 1u)], p1 = e->line[(e->w - b) & (ENS_N - 1u)], p2 = e->line[(e->w - b - 1u) & (ENS_N - 1u)], p3 = e->line[(e->w - b - 2u) & (ENS_N - 1u)];
    float c1 = (p2 - p0) * 0.5f, c2 = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3, c3 = (p3 - p0) * 0.5f + 1.5f * (p1 - p2);
    return p1 + f * (c1 + f * (c2 + f * c3));
}
static void ens_process(ens_t *e, int32_t *b, uint32_t n, uint32_t amount)
{
    if (ens_eq_fs != (int)fs_f) { vd_svf_set(&ens_hp_c, 150.0f, 0.7f, fs_f); vd_svf_set(&ens_body_c, 1000.0f, 0.9f, fs_f); ens_top_k = 1.0f - vd_exp2(-6.2832f * 6500.0f / fs_f * 1.4427f); ens_eq_fs = (int)fs_f; }
    float mix = (float)amount / 127.0f, dry = 1.0f - mix, dep = (0.4f + 0.6f * mix) * fs_f * 0.001f, d0[3], dd[3];
    uint32_t i, k;
    if (fs_f > 100000.0f) dep *= 100000.0f / fs_f;
    e->ps += 0.6f * (float)n * inv_fs; if (e->ps >= 1.0f) e->ps -= 1.0f;
    e->pf += 6.0f * (float)n * inv_fs; if (e->pf >= 1.0f) e->pf -= 1.0f;
    for (k = 0; k < 3u; k++) {
        float t = 6.0f * fs_f * 0.001f + dep * (3.9f * sinf_t(e->ps + (float)k * (1.0f / 3.0f)) + 0.43f * sinf_t(e->pf + (float)k * (1.0f / 3.0f)));
        if (t > (float)(ENS_N - 8u)) t = (float)(ENS_N - 8u);
        if (e->d[k] < 2.0f) e->d[k] = t;        /* (the first block) */
        d0[k] = e->d[k]; dd[k] = (t - d0[k]) / (float)n; e->d[k] = t;
    }
    for (i = 0; i < n; i++) {
        float l = (float)b[2u * i], r = (float)b[2u * i + 1u], t1, t2, t3;
        e->w = (e->w + 1u) & (ENS_N - 1u);
        e->line[e->w] = (l + r) * 0.5f;
        t1 = ens_read(e, d0[0]); t2 = ens_read(e, d0[1]); t3 = ens_read(e, d0[2]);
        d0[0] += dd[0]; d0[1] += dd[1]; d0[2] += dd[2];
        {   /* (0.18) a string section's voicing: no mud under 150 Hz, the body around 1 kHz up, the fizz above 6 kHz down */
            float o[2] = {l * dry + (t1 + 0.5f * t2) * mix * 0.8f, r * dry + (t3 + 0.5f * t2) * mix * 0.8f};
            for (uint32_t c2 = 0; c2 < 2u; c2++) {
                float y = vd_svf_tick(&e->hp[c2], &ens_hp_c, o[c2]).hp;
                y += vd_svf_tick(&e->body[c2], &ens_body_c, y).bp * 0.55f;
                e->top[c2] += (y - e->top[c2]) * ens_top_k; y = e->top[c2];
                b[2u * i + c2] = (int32_t)y;
            }
        }
    }
}
static int32_t mix_g[NPART];

/* (0.31, Sven: "im Mixer auch einen EQ") each track's channel EQ, before its fader: LOW shelf 100 Hz, MID bell (200 Hz-5 kHz,
 * Q 0.9), HIGH shelf 8 kHz, +-15 dB, and a LOW CUT (12 dB/oct at 40 / 80 / 150 Hz). RBJ biquads, float, transposed
 * direct form II; a flat track costs nothing. The main loop writes ctl_eq; new coefficients when a value changed. */
static volatile uint8_t ctl_eq[NPART][5] = {{64, 64, 64, 64, 0}, {64, 64, 64, 64, 0}, {64, 64, 64, 64, 0}, {64, 64, 64, 64, 0},
                                            {64, 64, 64, 64, 0}, {64, 64, 64, 64, 0}, {64, 64, 64, 64, 0}, {64, 64, 64, 64, 0}};
typedef struct { float b0, b1, b2, a1, a2; } bq_c;
typedef struct { bq_c c[4]; float z[4][2][2]; uint8_t on[4], key[5], any, ok; } eq_t;
static eq_t eqs[NPART];
static float eq_hz(uint32_t v) { return 200.0f * vd_pow(25.0f, (float)v / 127.0f); }   /* MID's FREQ: 200 Hz .. 5 kHz */
static void bq_set(bq_c *c, int kind, float hz, float db, float q)   /* kind 0 low shelf · 1 bell · 2 high shelf · 3 high-pass */
{
    float A = vd_exp2(db * (0.025f * 3.3219281f)), w = hz / fs_f, cs = vd_sin(w + 0.25f), sn = vd_sin(w), al = sn / (2.0f * q), b0, b1, b2, a0, a1, a2;
    if (kind == 1) { b0 = 1.0f + al * A; b1 = -2.0f * cs; b2 = 1.0f - al * A; a0 = 1.0f + al / A; a1 = -2.0f * cs; a2 = 1.0f - al / A; }
    else if (kind == 3) { b0 = (1.0f + cs) * 0.5f; b1 = -(1.0f + cs); b2 = b0; a0 = 1.0f + al; a1 = -2.0f * cs; a2 = 1.0f - al; }
    else {
        float sa = 2.0f * vd_sqrt(A) * al, ap = A + 1.0f, am = A - 1.0f;
        if (kind == 0) { b0 = A * (ap - am * cs + sa); b1 = 2.0f * A * (am - ap * cs); b2 = A * (ap - am * cs - sa); a0 = ap + am * cs + sa; a1 = -2.0f * (am + ap * cs); a2 = ap + am * cs - sa; }
        else { b0 = A * (ap + am * cs + sa); b1 = -2.0f * A * (am + ap * cs); b2 = A * (ap + am * cs - sa); a0 = ap - am * cs + sa; a1 = 2.0f * (am - ap * cs); a2 = ap - am * cs - sa; }
    }
    c->b0 = b0 / a0; c->b1 = b1 / a0; c->b2 = b2 / a0; c->a1 = a1 / a0; c->a2 = a2 / a0;
}
static void eq_update(uint32_t p)               /* the coefficients, when a value changed */
{
    eq_t *e = &eqs[p];
    static const float CUT_HZ[4] = {0.0f, 40.0f, 80.0f, 150.0f};
    uint8_t k[5]; int ch = !e->ok;
    for (uint32_t i = 0; i < 5u; i++) { k[i] = ctl_eq[p][i]; if (k[i] != e->key[i]) ch = 1; }
    if (!ch) return;
    for (uint32_t i = 0; i < 4u; i++) { int was = e->on[i]; e->on[i] = i == 3u ? k[4] != 0u : k[i == 2u ? 3u : i] != 64u; if (e->on[i] && !was) memset(e->z[i], 0, sizeof e->z[i]); }
    if (e->on[0]) bq_set(&e->c[0], 0, 100.0f, ((float)k[0] - 64.0f) * (15.0f / 63.0f), 0.707f);
    if (e->on[1]) bq_set(&e->c[1], 1, eq_hz(k[2]), ((float)k[1] - 64.0f) * (15.0f / 63.0f), 0.9f);
    if (e->on[2]) bq_set(&e->c[2], 2, 8000.0f, ((float)k[3] - 64.0f) * (15.0f / 63.0f), 0.707f);
    if (e->on[3]) bq_set(&e->c[3], 3, CUT_HZ[k[4] & 3u], 0.0f, 0.707f);
    e->any = e->on[0] | e->on[1] | e->on[2] | e->on[3];
    memcpy(e->key, k, 5); e->ok = 1;
}
static void eq_process(uint32_t p, int32_t *b, uint32_t n)
{
    eq_t *e = &eqs[p];
    eq_update(p);
    if (!e->any) return;
    for (uint32_t c = 0; c < 2u; c++)
        for (uint32_t f = 0; f < 4u; f++) {
            if (!e->on[f]) continue;
            const bq_c q = e->c[f]; float z1 = e->z[f][c][0], z2 = e->z[f][c][1];
            for (uint32_t i = 0; i < n; i++) {
                float x = (float)b[2u * i + c], y = q.b0 * x + z1;
                z1 = q.b1 * x - q.a1 * y + z2; z2 = q.b2 * x - q.a2 * y;
                b[2u * i + c] = (int32_t)y;
            }
            if (!(z1 > -1e30f && z1 < 1e30f) || !(z2 > -1e30f && z2 < 1e30f)) z1 = z2 = 0.0f;   /* (never a stuck filter) */
            if (z1 < 1e-9f && z1 > -1e-9f) z1 = 0.0f;   /* (no denormals in the silence) */
            if (z2 < 1e-9f && z2 > -1e-9f) z2 = 0.0f;
            e->z[f][c][0] = z1; e->z[f][c][1] = z2;
        }
}

static void au_block(uint32_t k);
static void au_mix(int32_t (*b)[2u * HALF_FRAMES], uint32_t n, uint32_t *used);
static int au_resampling(void);
/* (0.10) the sum's clipper: clean up to 0.7 of full scale, a soft knee above (before: a tanh from zero – every sound
 * was bent, a kit alone sat in it) */
static inline int32_t mixclip(int32_t x)
{
    int32_t a = x < 0 ? -x : x;
    if (a <= 22938) return x;
    a = 22938 + ((softclip((int32_t)(((int64_t)(a - 22938) << 15) / 9830)) * 9830) >> 15);
    return x < 0 ? -a : a;
}
static void engine_audio(int32_t *out, uint32_t n)
{
    uint32_t b, v, i, p, used = 0;
    static int32_t sc_prev;
    static uint32_t sc_i, sc_sub;
    int32_t vol_q12;
    fx_block(&live, quarter_len());
    vol_q12 = (int32_t)((ctl_cc7 * ctl_cc7) * 7u / 40u);   /* (0.10: the sum 3 dB lower into the clipper) */
    for (p = 0; p < NPART; p++) for (i = 0; i < 2u * n; i++) bus[p][i] = 0;
    for (i = 0; i < n; i++) snd_c[i] = snd_d[i] = snd_r[i] = snd_v[i] = 0;
    trk_x_used = 0;
    for (b = 0; b < n; b += CTL) {
        mo_tick = b;
        if (!b) mt_block();                     /* (0.30) */
        engine_events();
        if (__atomic_exchange_n(&ctl_arp_restart, 0u, __ATOMIC_ACQ_REL)) { for (p = 0; p < NPART; p++) { arp[p].pos = 0; arp[p].cnt = 0xFFFFFFFFu; } }
#ifdef OBV_TRACE
        trace_blk++;
#endif
        eng_t += CTL;
        for (p = 0; p < NPART; p++) arp_tick(p);
        seq_tick();
        au_block(b / CTL);
        for (p = 0; p < NPART; p++) { lfo_tick(&pt[p]); block_globals(&pt[p]); }
        for (v = 0; v < NV; v++) {
            if (vc[v].ast == E_IDLE && !vc[v].gate) {
                if (ui_vlevel[v]) { ui_vlevel[v] = 0; vc[v].amp0 = vc[v].amp1 = 0.0f; }
                continue;
            }
            p = vc[v].part % NPART;
            if (vc[v].type != part_type(p) && !(vc[v].type == T_CV && part_type(p) == T_EP && pt[p].p->v[P_EP_CLAV])) { vc[v].ast = E_IDLE; vc[v].gate = 0; continue; }   /* (the sound changed its kind under it; 0.41: E-PIANO's CLAV voices are T_CV) */
            used |= 1u << p;
            voice_block(&vc[v], v);
            voice_render(&vc[v], bus[p] + 2u * b);
        }
    }
    au_mix(bus, n, &used);
    for (p = 0; p < NPART; p++)                  /* (0.9) the STRINGS' ensemble on its track */
        if ((used >> p & 1u) && part_type(p) == T_STR && pt[p].p->v[P_ST_ENS]) ens_process(&ens[p], bus[p], n, pt[p].p->v[P_ST_ENS]);
    for (p = 0; p < NPART; p++) if (used >> p & 1u) eq_process(p, bus[p], n);   /* (0.31) the channel EQ, before the fader */
    for (p = 0; p < NPART; p++) {
        int32_t lvl = ctl_mix_off >> p & 1u ? 0 : (int32_t)ctl_mix_lvl[p], target = lvl * lvl * 4096 / 10000, g0 = mix_g[p], gl, gr, sc, sd, sr, sv;
        uint32_t pos = (uint32_t)(clampi(ctl_mix_pan[p], -64, 64) + 64);
        if (part_type(p) == T_SYNTH) target = target * 7 / 5;   /* (0.10) OberVave's sounds stood 6 dB under the other kinds: +3 dB */
        if (!(used >> p & 1u)) { mix_g[p] = target; continue; }
        mix_g[p] += (target - g0) / 2;
        gl = PAN_Q12[128u - pos] * 4096 / 2896; gr = PAN_Q12[pos] * 4096 / 2896;
        gl = gl > 4096 ? 4096 : gl; gr = gr > 4096 ? 4096 : gr;
        sc = ctl_mix_cho[p]; sd = ctl_mix_dly[p]; sr = ctl_mix_rev[p]; sv = ctl_mix_drv[p];
        {
        int32_t g = g0 << 8, dg = ((mix_g[p] - g0) << 8) / (int32_t)n;
        uint32_t pk = 0;
        const uint32_t amp_want = ctl_dv_type % NDV == DV_AMP && sv > 0;   /* (0.8) the AMP: this track's insert */
        const uint32_t amp_on = amp_want || amp_w[p] > 0;                  /* (0.17) it fades in and out (~5 ms): switched hard, it clicked */
        const float am_a = (float)ctl_dv_amt / 127.0f, am_tk = 0.04f + 0.96f * (float)ctl_dv_tone / 127.0f * (float)ctl_dv_tone / 127.0f;
        const int32_t am_wet = amp_want ? (sv > 127 ? 127 : sv) * 4096 / 127 : 0, am_vol = (int32_t)ctl_dv_mix * 4096 / 127;
        int32_t *dst = out;                     /* (BOOM 0.4) its own output: OUT 3/4 + 2p, when the host has it – the sends stay in the main mix */
        if ((ctl_trk_own >> p & 1u) && ctl_host_outs >= 4u + 2u * p) { dst = trk_x[p]; for (i = 0; i < 2u * n; i++) dst[i] = 0; trk_x_used |= 1u << p; }
        for (i = 0; i < n; i++, g += dg) {
            int32_t l = (int32_t)(((int64_t)bus[p][2u * i] * (g >> 8) >> 12) * gl >> 12), r = (int32_t)(((int64_t)bus[p][2u * i + 1u] * (g >> 8) >> 12) * gr >> 12);
            int32_t m;
            uint32_t al, ar;
            if (amp_on) {                       /* the track's mono sum through its amp, back under its pan; the rest stays dry */
                int32_t d = (int32_t)((int64_t)(bus[p][2u * i] + bus[p][2u * i + 1u]) * (g >> 8) >> 13);
                int32_t w = (int32_t)(amp_tick(&amps[p], (float)d * (1.0f / 131072.0f), am_a, am_tk) * AMP_OUT) * am_vol >> 12;
                amp_w[p] += (am_wet - amp_w[p]) / 256 + (am_wet > amp_w[p] ? 1 : am_wet < amp_w[p] ? -1 : 0);
                l += ((int32_t)((int64_t)w * gl >> 12) - l) * amp_w[p] >> 12;
                r += ((int32_t)((int64_t)w * gr >> 12) - r) * amp_w[p] >> 12;
            }
            m = (l + r) >> 1;
            al = (uint32_t)(l < 0 ? -l : l); ar = (uint32_t)(r < 0 ? -r : r);
            if (al > pk) pk = al;
            if (ar > pk) pk = ar;
            dst[2u * i] += l; dst[2u * i + 1u] += r;
            snd_c[i] += (m * sc) >> 7; snd_d[i] += (m * sd) >> 7; snd_r[i] += (m * sr) >> 7; if (!amp_want) snd_v[i] += (m * sv) >> 7;
        }
        if (pk > ui_trk_peak[p]) ui_trk_peak[p] = pk;
        }
    }
    for (i = 0; i < n; i++) {
        int32_t l = mixclip((clampi(out[2u * i], -262144, 262143) >> 2) * vol_q12 >> 10);
        int32_t r = mixclip((clampi(out[2u * i + 1u], -262144, 262143) >> 2) * vol_q12 >> 10);
        int32_t m = (l + r) >> 1;
        out[2u * i] = l; out[2u * i + 1u] = r;
        if (scope_state == 1u) { if (sc_prev < 0 && m >= 0) { scope_state = 2; sc_i = 0; sc_sub = 0; } }
        else if (scope_state == 2u) {
            if (!sc_sub++) scope[sc_i++] = (int16_t)m;
            if (sc_sub >= scope_stride) sc_sub = 0;
            if (sc_i >= SCOPE_N) scope_state = 3;
        }
        sc_prev = m;
    }
    for (i = 0; i < n; i++) {
        snd_c[i] = sat16((clampi(snd_c[i], -262144, 262143) >> 2) * vol_q12 >> 10);
        snd_d[i] = sat16((clampi(snd_d[i], -262144, 262143) >> 2) * vol_q12 >> 10);
        snd_r[i] = sat16((clampi(snd_r[i], -262144, 262143) >> 2) * vol_q12 >> 10);
        snd_v[i] = sat16((clampi(snd_v[i], -262144, 262143) >> 2) * vol_q12 >> 10);
    }
    fx_process(out, snd_c, snd_d, snd_r, n);
    drive_process(out, snd_v, n);
    if (au_resampling()) click_amp = 0;
    for (i = 0; i < n && click_amp; i++) {
        int32_t c;
        click_ph += click_inc;
        c = click_ph & 0x80000000u ? click_amp : -click_amp;
        out[2u * i] += c; out[2u * i + 1u] += c;
        click_amp -= (click_amp >> 8) + 1;
        if (click_amp < 64) click_amp = 0;
    }
    (void)vol_f;
}

static int engine_quiet(void)
{
    for (uint32_t v = 0; v < NV; v++) if (vc[v].ast != E_IDLE || vc[v].gate) return 0;
    return nq_r == nq_w;
}
static void engine_init(void)
{
    for (uint32_t v = 0; v < NV; v++) {
        vc[v].noise = 0x12345678u * (v + 1u) + 0x9E3779B9u;
        vc[v].gl = vc[v].gr = 1.0f; vc[v].ck1 = vc[v].ck2 = -1;
        vc[v].ph[0] = (float)(xorshift(&vc[v].noise) >> 8) * (1.0f / 16777216.0f);
        vc[v].ov.o1.ph = xorshift(&vc[v].noise); vc[v].ov.o2.ph = xorshift(&vc[v].noise);
        vc[v].ov.gl = vc[v].ov.gr = 2896;
    }
    for (uint32_t p = 0; p < NPART; p++) pt[p].ov.last_pitch16 = 60 << 16;
    for (uint32_t p = 0; p < NPART; p++) { pt[p].lfo_rnd_a = xorshift(&rnd_state); pt[p].lfo_rnd_b = xorshift(&rnd_state); }
    fx_init();
}
