/* SPDX-License-Identifier: GPL-3.0-only */
/* BOOM – the parameter table. A sound (patch) is a byte per parameter plus a 10-character name, as in OberVave; the
 * UI, MIDI CC, the DAW list and the storage all walk this table. One table serves the three kinds of sound a track can
 * be: DRUMS (a kit of 8 synth drums), BASS (the playable 808) and MEL (4-operator FM). P_TYPE says which one a sound
 * is; the pages a colour key shows depend on it (SPAGE below). The effects stay at the end (track A's patch holds them). */
enum {
    P_TYPE,                                     /* DRUMS · BASS · MEL */
    P_CUTOFF, P_RESO, P_FENV_AMT,               /* the filter (BASS: TONE after the drive; DRUMS: the kit's) */
    P_FA, P_FD, P_FS, P_FR,                     /* the filter envelope (MEL, BASS) */
    P_AA, P_AD, P_AS, P_AR,                     /* the amp envelope (MEL; BASS: DECAY, or GATE's ADSR) */
    P_LFO_SHAPE, P_LFO_RATE, P_LFO_DELAY, P_LFO_PHASE, P_LFO_PITCH, P_LFO_FLT, P_LFO_AMP,
    P_MODE, P_GLIDE, P_BEND, P_VOLUME,
    P_ARP_MODE, P_ARP_DIV, P_ARP_OCT, P_ARP_GATE,
    /* BASS: the 808 */
    P_B_PUNCH, P_B_PTIME, P_B_CLICK, P_B_TONE, P_B_SUB, P_B_DRIVE, P_B_DTYPE, P_B_GATE, P_B_SLIDE,
    /* MEL: 4-op FM */
    P_ALGO, P_FB,
    P_OP1_RATIO, P_OP1_LVL, P_OP1_ATT, P_OP1_DEC,
    P_OP2_RATIO, P_OP2_LVL, P_OP2_ATT, P_OP2_DEC,
    P_OP3_RATIO, P_OP3_LVL, P_OP3_ATT, P_OP3_DEC,
    P_OP4_RATIO, P_OP4_LVL, P_OP4_ATT, P_OP4_DEC,
    /* DRUMS: 8 drums (KICK SNARE CLAP CH OH RIM SNAP PERC) */
    P_K_TUNE, P_K_PUNCH, P_K_DECAY, P_K_DRIVE,
    P_S_TUNE, P_S_TONE, P_S_DECAY, P_S_SNAP,
    P_C_SPREAD, P_C_TONE, P_C_DECAY, P_C_DRIVE,
    P_H_TUNE, P_H_TONE, P_CH_DECAY, P_OH_DECAY,
    P_R_TUNE, P_R_DECAY, P_SN_TONE, P_SN_DECAY,
    P_P_TUNE, P_P_BEND, P_P_DECAY, P_P_TONE,
    P_DL_KICK, P_DL_SNARE, P_DL_CLAP, P_DL_CH, P_DL_OH, P_DL_RIM, P_DL_SNAP, P_DL_PERC,   /* each drum's level, 100 = unity */
    /* SYNTH: OberVave's analog poly (its own parameters; filter, envelopes, LFO, mode, glide, arp are the shared ones above) */
    P_O1_WAVE, P_O1_PW, P_O1_LVL, P_NOISE,
    P_O2_WAVE, P_O2_PW, P_O2_LVL, P_O2_SEMI,
    P_SYNC, P_XMOD, P_ENV_O2, P_O2_FINE,
    P_KEYTRK, P_FTYPE, P_DRIVE, P_VEL_FLT, P_VEL_AMP,
    P_LFO_PWM, P_LFO_PITCH_TO, P_LFO_PWM_TO,
    P_UNI_N, P_UNI_DET, P_VINTAGE, P_SPREAD, P_MOD_DEPTH, P_FMORPH,
    /* the effects: the song's, in track A's patch (as OberVave) */
    P_CHO_RATE, P_CHO_DEPTH, P_CHO_MIX,
    P_DLY_TIME, P_DLY_FB, P_DLY_TONE, P_DLY_MIX,
    P_REV_SIZE, P_REV_DAMP, P_REV_PRE, P_REV_MIX,
    /* (BOOM 0.6, appended – the rule of bm_save.h) DX: the macros over a DX7 voice (the voice itself: patch_t.dx) */
    P_DX_BRIGHT, P_DX_ATTACK, P_DX_DECAY, P_DX_RELEASE, P_DX_ALGO, P_DX_FB,
    /* (BOOM 0.9, appended) STRINGS: a string ensemble – three saw registers a voice, the ensemble behind the track */
    P_ST_16, P_ST_8, P_ST_4, P_ST_ENS,
    /* (BOOM 0.13, appended) DRUMS: the kit is synthesised or the acoustic one (samples) */
    P_KIT_SRC,
    /* (BOOM 0.19, appended) SAMPLER: another track's take, chopped into slices on the keys and the pads */
    P_SM_SRC, P_SM_SLICES, P_SM_CHOP, P_SM_QUAL, P_SM_TUNE, P_SM_REV,
    /* (0.21) the sample's START and END (14 bit: coarse and fine) and HITS' sensitivity */
    P_SM_ST_C, P_SM_ST_F, P_SM_EN_C, P_SM_EN_F, P_SM_SENS,
    /* (0.22, as ZVAMPLER) how a sample plays (1-SHOT / GATE / LOOP), its PITCH in semitones */
    P_SM_PMODE, P_SM_PITCH,
    /* (0.30, appended, Sven 15.11.) MIDI: a track that only plays an external device – its channel, a program change, the
     * velocity. (Its four CCs live on the drum levels below: a MIDI sound never uses them, and the patch has room for
     * only three more – PATCH_BYTES stays, so every preset saved before still loads.) */
    P_MI_CH, P_MI_PROG, P_MI_VEL,
    NPARAM
};
#define PATCH_NAME 10u
#define PATCH_BYTES 160u                        /* NPARAM values, the name at NPARAM, the rest spare */
_Static_assert(NPARAM + PATCH_NAME <= PATCH_BYTES, "the patch outgrew PATCH_BYTES");   /* (0.30: an enum – #if never saw it) */
/* (0.30) a MIDI track's four CC values and their CC numbers: the drum levels' bytes (a MIDI sound has no drums) */
#define P_MI_CC1 P_DL_KICK                      /* .. P_DL_CH: the values (sent when they change) */
#define P_MI_CC1N P_DL_OH                       /* .. P_DL_PERC: which CC each knob is (0..119) */
#define P_MI_CC4 (P_MI_CC1 + 3)
#define P_MI_CC2N (P_MI_CC1N + 1)
#define P_MI_CC3N (P_MI_CC1N + 2)
#define P_MI_CC4N (P_MI_CC1N + 3)
#define P_MI_CC2 (P_MI_CC1 + 1)
#define P_MI_CC3 (P_MI_CC1 + 2)
#define NPROG 192u                              /* (0.6: 129-160 DX · 0.9: 161-176 STRINGS · 0.19: 177-184 SAMPLER · 0.30: 185-192 MIDI) */
#define NPROG_06 128u                              /* 1-16 kits (DRUMS), 17-32 BASS, 33-64 MEL, 65-128 SYNTH (OberVave's 64); the user's saves over them */
enum { T_DRUMS, T_BASS, T_MEL, T_SYNTH, T_DX, T_STR, T_SMP, T_MIDI, NTYPE };
static const char *const TYPE_NAME[NTYPE] = {"DRUMS", "BASS", "MEL", "OBERVAVE", "DX", "STRINGS", "SAMPLER", "MIDI"};   /* (Sven 14.11.: the kind is called what it is) */
#define PROG_BASS0 16u
#define PROG_MEL0 32u
#define PROG_SYNTH0 64u
#define PROG_DX0 128u
#define PROG_STR0 160u
#define PROG_SMP0 176u
#define PROG_MIDI0 184u
static uint32_t prog_type_dflt(uint32_t n) { return n < PROG_BASS0 ? T_DRUMS : n < PROG_MEL0 ? T_BASS : n < PROG_SYNTH0 ? T_MEL : n < PROG_DX0 ? T_SYNTH : n < PROG_STR0 ? T_DX : n < PROG_SMP0 ? T_STR : n < PROG_MIDI0 ? T_SMP : T_MIDI; }   /* a factory slot's kind */
static uint32_t type_prog0(uint32_t ty) { return ty == T_DRUMS ? 0u : ty == T_BASS ? PROG_BASS0 : ty == T_MEL ? PROG_MEL0 : ty == T_SYNTH ? PROG_SYNTH0 : ty == T_DX ? PROG_DX0 : ty == T_STR ? PROG_STR0 : ty == T_SMP ? PROG_SMP0 : PROG_MIDI0; }   /* its first slot */

enum { K_NUM, K_BIP, K_ONOFF, K_MODE, K_LSHAPE, K_LPHASE, K_ARP, K_DIV, K_MS, K_HZ, K_PCT, K_CUT, K_OCT, K_DDIV,
       K_TYPE, K_DTYPE, K_BGATE, K_SLIDE, K_ALGO, K_RATIO, K_LVL, K_SEMI, K_WAVE, K_FTYPE, K_TO, K_MORPH, K_DXALGO, K_DXFB, K_DMODE, K_RMODE, K_KSRC, K_SMTRK, K_SMN, K_SMCHOP, K_SMQ, K_SMPOS, K_SMPM, K_SMPITCH, K_SMFINE, K_MICH, K_MIPROG, K_MIVEL };
enum { M_POLY, M_DUAL, M_UNISON, M_MONO };   /* (OberVave's; MEL: POLY or MONO, the two between play as POLY) */
enum { W_OFF, W_SAW, W_PLS, W_BOTH };
enum { F_LP12, F_HP12, F_BP12, F_NOTCH, F_LP24, F_SEM, NFTYPE };
enum { TO_O1, TO_O2, TO_BOTH };
enum { L_TRI, L_SAW, L_RAMP, L_SQR, L_SH, L_RND };
enum { LP_SYNC, LP_FREE, LP_SPREAD };
enum { A_OFF, A_UP, A_DOWN, A_UPDN, A_RND, A_PLAY, NARPMODE };
enum { D_4, D_8, D_8T, D_16, D_16T, D_32, D_32T, NDIV };
enum { DD_16, DD_8T, DD_8, DD_8D, DD_4, DD_4D, DD_2, NDDIV };   /* the delay's note values */
enum { DM_ANALOG, DM_DIGITAL, DM_TAPE, NDM };  /* the delay: a bucket brigade (dark, round) · clean · a tape (wow, saturation, thin lows) */
enum { RM_HALL, RM_ROOM, RM_PLATE, NRM };       /* the reverb */
enum { BD_SOFT, BD_HARD, BD_FOLD, NBDT };       /* the 808's drive */
enum { BG_DECAY, BG_GATE };                     /* the 808's length: a decay, or as long as held */
enum { BS_LEGATO, BS_ALWAYS };                  /* the 808 slides: between overlapping notes, or always */
#define NALGO 8u
#define NRATIO 24u
static const uint16_t RATIO_X100[NRATIO] = {25, 50, 75, 100, 141, 150, 200, 250, 300, 350, 400, 450, 500, 550, 600, 700, 800, 900, 1000, 1100, 1200, 1300, 1400, 1600};

/* the DAW's and the Editor's groups: where a parameter lives */
enum { G_VOICE, G_FILTER, G_FENV, G_AENV, G_LFO, G_ARP, G_BASS, G_ALGO, G_OP1, G_OP2, G_OP3, G_OP4,
       G_KICK, G_SNARE, G_CLAP, G_HATS, G_RIMSNAP, G_PERC, G_LEVELS, G_OSC1, G_OSC2, G_OSCMOD, G_FILTER2, G_OUTPUT, G_CHORUS, G_DELAY, G_REVERB, G_DX, G_STR, G_SMP, G_MIDI, NGRP };
static const char *const GRP_NAME[NGRP] = {"VOICE", "FILTER", "FILTER ENV", "AMP ENV", "LFO", "ARP", "BASS", "ALGO", "OP 1", "OP 2", "OP 3", "OP 4",
                                           "KICK", "SNARE", "CLAP", "HATS", "RIM/SNAP", "PERC", "LEVELS", "OSC 1", "OSC 2", "OSC MOD", "FILTER +", "OUTPUT", "CHORUS", "DELAY", "REVERB", "DX", "STRINGS", "SAMPLER", "MIDI"};

typedef struct {
    const char *name;                           /* 8 characters at most (the knob label) */
    uint8_t max, def, kind, cc, grp;            /* cc: a fixed MIDI CC, 0 = taken from 20 up in table order */
} pdef_t;
static const pdef_t PDEF[NPARAM] = {
    [P_TYPE] = {"TYPE", NTYPE - 1, T_MEL, K_TYPE, 0, G_VOICE},
    [P_CUTOFF] = {"CUTOFF", 127, 127, K_CUT, 74, G_FILTER},
    [P_RESO] = {"RESO", 127, 10, K_NUM, 71, G_FILTER},
    [P_FENV_AMT] = {"ENV AMT", 127, 64, K_BIP, 0, G_FILTER},
    [P_FA] = {"ATTACK", 127, 0, K_MS, 0, G_FENV},
    [P_FD] = {"DECAY", 127, 70, K_MS, 0, G_FENV},
    [P_FS] = {"SUSTAIN", 127, 0, K_NUM, 0, G_FENV},
    [P_FR] = {"RELEASE", 127, 60, K_MS, 0, G_FENV},
    [P_AA] = {"ATTACK", 127, 0, K_MS, 73, G_AENV},
    [P_AD] = {"DECAY", 127, 80, K_MS, 0, G_AENV},
    [P_AS] = {"SUSTAIN", 127, 0, K_NUM, 0, G_AENV},
    [P_AR] = {"RELEASE", 127, 60, K_MS, 72, G_AENV},
    [P_LFO_SHAPE] = {"SHAPE", 5, L_TRI, K_LSHAPE, 0, G_LFO},
    [P_LFO_RATE] = {"RATE", 127, 70, K_HZ, 0, G_LFO},
    [P_LFO_DELAY] = {"DELAY", 127, 0, K_NUM, 0, G_LFO},
    [P_LFO_PHASE] = {"PHASE", 2, LP_FREE, K_LPHASE, 0, G_LFO},
    [P_LFO_PITCH] = {"PITCH", 127, 0, K_NUM, 0, G_LFO},
    [P_LFO_FLT] = {"FILTER", 127, 0, K_NUM, 0, G_LFO},
    [P_LFO_AMP] = {"AMP", 127, 0, K_NUM, 0, G_LFO},
    [P_MODE] = {"MODE", 3, M_POLY, K_MODE, 0, G_VOICE},
    [P_GLIDE] = {"GLIDE", 127, 0, K_MS, 5, G_VOICE},
    [P_BEND] = {"BEND", 24, 2, K_NUM, 0, G_VOICE},
    [P_VOLUME] = {"VOLUME", 127, 100, K_NUM, 0, G_VOICE},
    [P_ARP_MODE] = {"ARP", 5, A_OFF, K_ARP, 0, G_ARP},
    [P_ARP_DIV] = {"DIV", 6, D_16, K_DIV, 0, G_ARP},
    [P_ARP_OCT] = {"OCTAVES", 4, 1, K_OCT, 0, G_ARP},
    [P_ARP_GATE] = {"GATE", 127, 64, K_PCT, 0, G_ARP},
    [P_B_PUNCH] = {"PUNCH", 127, 60, K_NUM, 0, G_BASS},
    [P_B_PTIME] = {"PUNCH T", 127, 40, K_NUM, 0, G_BASS},
    [P_B_CLICK] = {"CLICK", 127, 30, K_NUM, 0, G_BASS},
    [P_B_TONE] = {"TONE", 127, 30, K_NUM, 0, G_BASS},
    [P_B_SUB] = {"SUB", 127, 0, K_NUM, 0, G_BASS},
    [P_B_DRIVE] = {"DRIVE", 127, 30, K_NUM, 0, G_BASS},
    [P_B_DTYPE] = {"DRV TYPE", NBDT - 1, BD_SOFT, K_DTYPE, 0, G_BASS},
    [P_B_GATE] = {"LENGTH", 1, BG_DECAY, K_BGATE, 0, G_BASS},
    [P_B_SLIDE] = {"SLIDE", 1, BS_LEGATO, K_SLIDE, 0, G_BASS},
    [P_ALGO] = {"ALGO", NALGO - 1, 0, K_ALGO, 0, G_ALGO},
    [P_FB] = {"FEEDBK", 127, 0, K_NUM, 0, G_ALGO},
    [P_OP1_RATIO] = {"RATIO", NRATIO - 1, 3, K_RATIO, 0, G_OP1},
    [P_OP1_LVL] = {"LEVEL", 127, 100, K_NUM, 0, G_OP1},
    [P_OP1_ATT] = {"ATTACK", 127, 0, K_MS, 0, G_OP1},
    [P_OP1_DEC] = {"DECAY", 127, 127, K_MS, 0, G_OP1},
    [P_OP2_RATIO] = {"RATIO", NRATIO - 1, 3, K_RATIO, 0, G_OP2},
    [P_OP2_LVL] = {"LEVEL", 127, 50, K_NUM, 0, G_OP2},
    [P_OP2_ATT] = {"ATTACK", 127, 0, K_MS, 0, G_OP2},
    [P_OP2_DEC] = {"DECAY", 127, 90, K_MS, 0, G_OP2},
    [P_OP3_RATIO] = {"RATIO", NRATIO - 1, 3, K_RATIO, 0, G_OP3},
    [P_OP3_LVL] = {"LEVEL", 127, 0, K_NUM, 0, G_OP3},
    [P_OP3_ATT] = {"ATTACK", 127, 0, K_MS, 0, G_OP3},
    [P_OP3_DEC] = {"DECAY", 127, 90, K_MS, 0, G_OP3},
    [P_OP4_RATIO] = {"RATIO", NRATIO - 1, 3, K_RATIO, 0, G_OP4},
    [P_OP4_LVL] = {"LEVEL", 127, 0, K_NUM, 0, G_OP4},
    [P_OP4_ATT] = {"ATTACK", 127, 0, K_MS, 0, G_OP4},
    [P_OP4_DEC] = {"DECAY", 127, 90, K_MS, 0, G_OP4},
    [P_K_TUNE] = {"TUNE", 127, 50, K_NUM, 0, G_KICK},
    [P_K_PUNCH] = {"PUNCH", 127, 70, K_NUM, 0, G_KICK},
    [P_K_DECAY] = {"DECAY", 127, 60, K_NUM, 0, G_KICK},
    [P_K_DRIVE] = {"DRIVE", 127, 20, K_NUM, 0, G_KICK},
    [P_S_TUNE] = {"TUNE", 127, 60, K_NUM, 0, G_SNARE},
    [P_S_TONE] = {"TONE", 127, 70, K_NUM, 0, G_SNARE},
    [P_S_DECAY] = {"DECAY", 127, 50, K_NUM, 0, G_SNARE},
    [P_S_SNAP] = {"SNAP", 127, 80, K_NUM, 0, G_SNARE},
    [P_C_SPREAD] = {"SPREAD", 127, 50, K_NUM, 0, G_CLAP},
    [P_C_TONE] = {"TONE", 127, 60, K_NUM, 0, G_CLAP},
    [P_C_DECAY] = {"DECAY", 127, 55, K_NUM, 0, G_CLAP},
    [P_C_DRIVE] = {"DRIVE", 127, 10, K_NUM, 0, G_CLAP},
    [P_H_TUNE] = {"TUNE", 127, 60, K_NUM, 0, G_HATS},
    [P_H_TONE] = {"TONE", 127, 70, K_NUM, 0, G_HATS},
    [P_CH_DECAY] = {"CH DECAY", 127, 35, K_NUM, 0, G_HATS},
    [P_OH_DECAY] = {"OH DECAY", 127, 70, K_NUM, 0, G_HATS},
    [P_R_TUNE] = {"RIM TUNE", 127, 60, K_NUM, 0, G_RIMSNAP},
    [P_R_DECAY] = {"RIM DEC", 127, 30, K_NUM, 0, G_RIMSNAP},
    [P_SN_TONE] = {"SNAP TON", 127, 70, K_NUM, 0, G_RIMSNAP},
    [P_SN_DECAY] = {"SNAP DEC", 127, 35, K_NUM, 0, G_RIMSNAP},
    [P_P_TUNE] = {"TUNE", 127, 50, K_NUM, 0, G_PERC},
    [P_P_BEND] = {"BEND", 127, 60, K_NUM, 0, G_PERC},
    [P_P_DECAY] = {"DECAY", 127, 50, K_NUM, 0, G_PERC},
    [P_P_TONE] = {"TONE", 127, 90, K_NUM, 0, G_PERC},
    [P_DL_KICK] = {"KICK", 127, 100, K_LVL, 0, G_LEVELS},
    [P_DL_SNARE] = {"SNARE", 127, 100, K_LVL, 0, G_LEVELS},
    [P_DL_CLAP] = {"CLAP", 127, 90, K_LVL, 0, G_LEVELS},
    [P_DL_CH] = {"CH", 127, 85, K_LVL, 0, G_LEVELS},
    [P_DL_OH] = {"OH", 127, 85, K_LVL, 0, G_LEVELS},
    [P_DL_RIM] = {"RIM", 127, 90, K_LVL, 0, G_LEVELS},
    [P_DL_SNAP] = {"SNAP", 127, 90, K_LVL, 0, G_LEVELS},
    [P_DL_PERC] = {"PERC", 127, 90, K_LVL, 0, G_LEVELS},
    [P_O1_WAVE] = {"WAVE", 3, W_SAW, K_WAVE, 0, G_OSC1},
    [P_O1_PW] = {"PW", 127, 64, K_PCT, 0, G_OSC1},
    [P_O1_LVL] = {"LEVEL", 127, 110, K_NUM, 0, G_OSC1},
    [P_NOISE] = {"NOISE", 127, 0, K_NUM, 0, G_OSC1},
    [P_O2_WAVE] = {"WAVE", 3, W_SAW, K_WAVE, 0, G_OSC2},
    [P_O2_PW] = {"PW", 127, 64, K_PCT, 0, G_OSC2},
    [P_O2_LVL] = {"LEVEL", 127, 100, K_NUM, 0, G_OSC2},
    [P_O2_SEMI] = {"SEMI", 48, 24, K_SEMI, 0, G_OSC2},
    [P_SYNC] = {"SYNC", 1, 0, K_ONOFF, 0, G_OSCMOD},
    [P_XMOD] = {"X-MOD", 127, 0, K_NUM, 0, G_OSCMOD},
    [P_ENV_O2] = {"ENV>O2", 127, 64, K_BIP, 0, G_OSCMOD},
    [P_O2_FINE] = {"FINE", 127, 70, K_BIP, 0, G_OSCMOD},
    [P_KEYTRK] = {"KEY TRK", 127, 64, K_PCT, 0, G_FILTER},
    [P_FTYPE] = {"TYPE", NFTYPE - 1, F_LP24, K_FTYPE, 0, G_FILTER},
    [P_DRIVE] = {"DRIVE", 127, 0, K_NUM, 0, G_FILTER2},
    [P_VEL_FLT] = {"VEL>FLT", 127, 40, K_NUM, 0, G_FILTER2},
    [P_VEL_AMP] = {"VEL>AMP", 127, 60, K_NUM, 0, G_FILTER2},
    [P_LFO_PWM] = {"PWM", 127, 0, K_NUM, 0, G_LFO},
    [P_LFO_PITCH_TO] = {"PITCH TO", 2, TO_BOTH, K_TO, 0, G_LFO},   /* (not "PITCH>": its DAW id would be PITCH's) */
    [P_LFO_PWM_TO] = {"PWM TO", 2, TO_BOTH, K_TO, 0, G_LFO},
    [P_UNI_N] = {"VOICES", 8, 4, K_NUM, 0, G_VOICE},
    [P_UNI_DET] = {"DETUNE", 127, 30, K_NUM, 0, G_VOICE},
    [P_VINTAGE] = {"VINTAGE", 127, 30, K_NUM, 0, G_OUTPUT},
    [P_SPREAD] = {"SPREAD", 127, 60, K_NUM, 0, G_OUTPUT},
    [P_MOD_DEPTH] = {"MOD DPT", 127, 0, K_NUM, 1, G_OUTPUT},
    [P_FMORPH] = {"MORPH", 127, 0, K_MORPH, 0, G_FILTER2},
    [P_CHO_RATE] = {"RATE", 127, 40, K_NUM, 0, G_CHORUS},
    [P_CHO_DEPTH] = {"DEPTH", 127, 50, K_NUM, 0, G_CHORUS},
    [P_CHO_MIX] = {"MIX", 127, 0, K_NUM, 0, G_CHORUS},
    [P_DLY_TIME] = {"TIME", NDDIV - 1, DD_8D, K_DDIV, 0, G_DELAY},
    [P_DLY_FB] = {"FEEDBK", 127, 50, K_NUM, 0, G_DELAY},
    [P_DLY_TONE] = {"TONE", 127, 70, K_NUM, 0, G_DELAY},
    [P_DLY_MIX] = {"MODE", NDM - 1, DM_ANALOG, K_DMODE, 0, G_DELAY},   /* (BOOM 0.7, Sven: no MIX – the return is always full, the sends set the level; its place is the MODE) */
    [P_REV_SIZE] = {"SIZE", 127, 80, K_NUM, 0, G_REVERB},
    [P_REV_DAMP] = {"DAMP", 127, 60, K_NUM, 0, G_REVERB},
    [P_REV_PRE] = {"PRE-DLY", 127, 20, K_NUM, 0, G_REVERB},
    [P_REV_MIX] = {"MODE", NRM - 1, RM_PLATE, K_RMODE, 0, G_REVERB},
    [P_DX_BRIGHT] = {"BRIGHT", 127, 64, K_BIP, 0, G_DX},
    [P_DX_ATTACK] = {"ATTACK", 127, 64, K_BIP, 0, G_DX},
    [P_DX_DECAY] = {"DECAY", 127, 64, K_BIP, 0, G_DX},
    [P_DX_RELEASE] = {"RELEASE", 127, 64, K_BIP, 0, G_DX},
    [P_DX_ALGO] = {"ALGO", 32, 0, K_DXALGO, 0, G_DX},
    [P_DX_FB] = {"FEEDBK", 8, 0, K_DXFB, 0, G_DX},
    [P_ST_16] = {"16'", 127, 0, K_NUM, 0, G_STR},
    [P_ST_8] = {"8'", 127, 100, K_NUM, 0, G_STR},
    [P_ST_4] = {"4'", 127, 70, K_NUM, 0, G_STR},
    [P_ST_ENS] = {"ENSEMBLE", 127, 100, K_NUM, 0, G_STR},
    [P_KIT_SRC] = {"DRUMS", 2, 0, K_KSRC, 0, G_LEVELS},   /* (0.27: 2 = HIP HOP) */
    [P_SM_SRC] = {"SOURCE", 8, 0, K_SMTRK, 0, G_SMP},   /* (0.20) OWN: the track's own sample (recorded or loaded on it); A-H: another track's take */
    [P_SM_SLICES] = {"SLICES", 14, 14, K_SMN, 0, G_SMP},   /* (0.21) 2..16 */
    [P_SM_CHOP] = {"MODE", 2, 0, K_SMCHOP, 0, G_SMP},   /* (0.20) PLAY: the whole sample over the keys · CHOP EVEN / HITS: slices */
    [P_SM_QUAL] = {"QUALITY", 3, 0, K_SMQ, 0, G_SMP},
    [P_SM_TUNE] = {"FINE", 127, 64, K_SMFINE, 0, G_SMP},   /* (0.22) cents, +-50 */
    [P_SM_REV] = {"REVERSE", 1, 0, K_ONOFF, 0, G_SMP},
    [P_SM_ST_C] = {"START", 127, 0, K_SMPOS, 0, G_SMP},
    [P_SM_ST_F] = {"START FINE", 127, 0, K_NUM, 0, G_SMP},
    [P_SM_EN_C] = {"END", 127, 127, K_SMPOS, 0, G_SMP},
    [P_SM_EN_F] = {"END FINE", 127, 127, K_NUM, 0, G_SMP},
    [P_SM_SENS] = {"SENS", 127, 64, K_PCT, 0, G_SMP},
    [P_SM_PMODE] = {"PLAY MODE", 2, 0, K_SMPM, 0, G_SMP},
    [P_SM_PITCH] = {"PITCH", 48, 24, K_SMPITCH, 0, G_SMP},
    [P_MI_CH] = {"CHANNEL", 15, 0, K_MICH, 0, G_MIDI},       /* (0.30) 1..16 */
    [P_MI_PROG] = {"PROGRAM", 128, 0, K_MIPROG, 0, G_MIDI},  /* 0: none sent, 1..128 */
    [P_MI_VEL] = {"VELOCITY", 127, 0, K_MIVEL, 0, G_MIDI},   /* 0: as played, else always that */
};

static const char *const MODE_NAME[4] = {"POLY", "DUAL", "UNISON", "MONO"};
static const char *const WAVE_NAME[4] = {"OFF", "SAW", "PULSE", "SAW+PLS"};
static const char *const FTYPE_NAME[NFTYPE] = {"LP 12", "HP 12", "BP 12", "NOTCH", "LP 24", "SEM"};
static const char *const TO_NAME[3] = {"OSC1", "OSC2", "BOTH"};
static const char *const LSHAPE_NAME[6] = {"TRI", "SAW", "RAMP", "SQUARE", "S&H", "RANDOM"};
static const char *const LPHASE_NAME[3] = {"SYNC", "FREE", "SPREAD"};
static const char *const ARP_NAME[NARPMODE] = {"OFF", "UP", "DOWN", "UP/DN", "RANDOM", "PLAYED"};
static const char *const DIV_NAME[NDIV] = {"1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T"};
static const uint8_t DIV_PER_QUARTER[NDIV] = {1, 2, 3, 4, 6, 8, 12};
static const char *const DDIV_NAME[NDDIV] = {"1/16", "1/8T", "1/8", "1/8.", "1/4", "1/4.", "1/2"};
static const uint8_t DDIV_NUM[NDDIV] = {1, 1, 1, 3, 1, 3, 2}, DDIV_DEN[NDDIV] = {4, 3, 2, 4, 1, 2, 1};   /* of a quarter */
static const char *const BDT_NAME[NBDT] = {"SOFT", "HARD", "FOLD"};
static const char *const DM_NAME[NDM] = {"ANALOG", "DIGITAL", "TAPE"};
static const char *const RM_NAME[NRM] = {"HALL", "ROOM", "PLATE"};
static const char *const BGATE_NAME[2] = {"DECAY", "GATE"};
static const char *const KSRC_NAME[3] = {"SYNTH", "ACOUSTIC", "HIP HOP"};
static const char *const SMTRK_NAME[9] = {"OWN", "TRACK A", "TRACK B", "TRACK C", "TRACK D", "TRACK E", "TRACK F", "TRACK G", "TRACK H"};
static const char *const SMN_NAME[2] = {"8", "16"};
static const char *const SMCHOP_NAME[3] = {"KEYS MEL", "KEYS CHOP", "KEYS CHOP"};   /* (0.23: 1 = the cuts, even or by hand · 2 = an older sound's HITS) */
static const char *const SMPM_NAME[3] = {"1-SHOT", "GATE", "LOOP"};
static const char *const SMQ_NAME[4] = {"CLEAN", "SP 12", "MPC", "LO-FI"};   /* 16 bit · 12 bit 26 kHz · 12 bit 40 kHz · 8 bit 12 kHz */

static const char *const BSLIDE_NAME[2] = {"LEGATO", "ALWAYS"};

/* the 8 drums: their order (the pads: row 3 KICK SNARE CLAP RIM, row 4 CH OH SNAP PERC), their GM notes */
enum { DR_KICK, DR_SNARE, DR_CLAP, DR_CH, DR_OH, DR_RIM, DR_SNAP, DR_PERC, NDRUM };
static const char *const DRUM_NAME[NDRUM] = {"KICK", "SNARE", "CLAP", "CH", "OH", "RIM", "SNAP", "PERC"};
/* (0.13) the acoustic kit on the same eight rows and notes: the hats stay where they were (CH chokes OH) */
static const char *const DRUM_NAME_AC[NDRUM] = {"KICK", "SNARE", "TOM HI", "HAT", "OPEN", "TOM LO", "RIDE", "CRASH"};
static const char *const DRUM_NAME_HH[NDRUM] = {"KICK", "SNARE", "SNARE 2", "HAT", "OPEN", "RIM", "KICK 2", "CRASH"};   /* (0.27) the HIP HOP kit */
static const char *const *kit_names(uint32_t src) { return src == 2u ? DRUM_NAME_HH : src ? DRUM_NAME_AC : DRUM_NAME; }
static const char *drum_name(const uint8_t *pv, uint32_t d) { return kit_names(pv[P_KIT_SRC])[d % NDRUM]; }
static const char *ac_label(uint32_t p, uint32_t src)   /* a kit parameter's name on the acoustic / HIP HOP kit (0: as it is) */
{
    if (src == 2u) switch (p) {
    case P_C_TONE: return "SNARE 2"; case P_C_DECAY: return "S2 DECAY"; case P_R_TUNE: return "RIM"; case P_R_DECAY: return "RIM DECAY";
    case P_SN_TONE: return "KICK 2"; case P_SN_DECAY: return "K2 DECAY";
    case P_DL_CLAP: return "SNARE 2"; case P_DL_RIM: return "RIM"; case P_DL_SNAP: return "KICK 2";
    default: break;
    }
    switch (p) {
    case P_K_TUNE: return "KICK"; case P_K_DECAY: return "K DECAY"; case P_S_TUNE: return "SNARE"; case P_S_DECAY: return "S DECAY";
    case P_C_TONE: return "TOM HI"; case P_C_DECAY: return "HI DECAY"; case P_R_TUNE: return "TOM LO"; case P_R_DECAY: return "LO DECAY";
    case P_H_TUNE: return "HAT TUNE"; case P_CH_DECAY: return "CLOSED"; case P_OH_DECAY: return "OPEN";
    case P_SN_TONE: return "RIDE"; case P_SN_DECAY: return "RD DECAY"; case P_P_TUNE: return "CRASH"; case P_P_DECAY: return "CR DECAY";
    case P_DL_CLAP: return "TOM HI"; case P_DL_CH: return "HAT"; case P_DL_OH: return "OPEN"; case P_DL_RIM: return "TOM LO"; case P_DL_SNAP: return "RIDE"; case P_DL_PERC: return "CRASH";
    default: return 0;
    }
}
static const uint8_t DRUM_NOTE[NDRUM] = {36, 38, 39, 42, 46, 37, 40, 45};
static const uint8_t PAD_DRUM[8] = {DR_KICK, DR_SNARE, DR_CLAP, DR_RIM, DR_CH, DR_OH, DR_SNAP, DR_PERC};   /* pads 9-16 */
static const uint32_t DRUM_RGB[NDRUM] = {0xFF2E97, 0xFFE14D, 0xFF8A1E, 0x2BFFB4, 0x00E5FF, 0xC77DFF, 0xFF5AD8, 0x4D9DFF};   /* (neon) */
static uint32_t drum_of_note(uint32_t n)        /* GM's drum notes where they are known, else round the eight */
{
    static const uint8_t GM[17] = {DR_KICK, DR_KICK, DR_RIM, DR_SNARE, DR_CLAP, DR_SNAP, DR_PERC, DR_CH, DR_PERC, DR_CH,
                                   DR_PERC, DR_OH, DR_PERC, DR_PERC, DR_OH, DR_PERC, DR_OH};   /* 35 .. 51 */
    if (n >= 35u && n <= 51u) return GM[n - 35u];
    return n % NDRUM;
}

/* the pages: four knobs each (-1 = no knob). PG_PERFORM .. PG_S7 and the two ENV pages depend on the sound's kind
 * (SPAGE); the rest is one table */
typedef struct { const char *title; int16_t p[4]; } page_t;
#define PS_CUT 400                              /* (0.21) the SAMPLER's CHOP page: knobs that are no parameters – the chosen cut, moving it */
#define PS_MOVE 401   /* (0.9: int16 – the parameters went past 127) */
enum { PG_PERFORM, PG_S1, PG_S2, PG_S3, PG_S4, PG_S5, PG_S6, PG_S7, PG_FENV, PG_AENV, PG_LFO, PG_LFODEST, PG_ARP,
       PG_CHORUS, PG_DELAY, PG_REVERB, PG_DRIVE, PG_OUTPUT, PG_GLOBAL, PG_SCALE, PG_MIXER, PG_SEQ, NPAGE };
static const uint8_t SND_N[NTYPE] = {8, 6, 8, 7, 4, 4, 4, 3};   /* SOUND's pages per kind (TYPE comes after them) */
#define SND_N_AC 6u                             /* the acoustic kit's pages: SPAGE[NTYPE] */
static const page_t SPAGE[NTYPE + 1u][10] = {
    {   /* DRUMS */
        {"PERFORM", {P_K_DECAY, P_S_TONE, P_CH_DECAY, P_CUTOFF}},
        {"KICK", {P_K_TUNE, P_K_PUNCH, P_K_DECAY, P_K_DRIVE}},
        {"SNARE", {P_S_TUNE, P_S_TONE, P_S_DECAY, P_S_SNAP}},
        {"CLAP", {P_C_SPREAD, P_C_TONE, P_C_DECAY, P_C_DRIVE}},
        {"HATS", {P_H_TUNE, P_H_TONE, P_CH_DECAY, P_OH_DECAY}},
        {"RIM/SNAP", {P_R_TUNE, P_R_DECAY, P_SN_TONE, P_SN_DECAY}},
        {"PERC", {P_P_TUNE, P_P_BEND, P_P_DECAY, P_P_TONE}},
        {"KIT", {P_CUTOFF, P_RESO, P_VOLUME, P_KIT_SRC}},
        {"LEVELS 1-4", {P_DL_KICK, P_DL_SNARE, P_DL_CLAP, P_DL_CH}},
        {"LEVELS 5-8", {P_DL_OH, P_DL_RIM, P_DL_SNAP, P_DL_PERC}},
    },
    {   /* BASS */
        {"PERFORM", {P_AD, P_B_PUNCH, P_B_DRIVE, P_CUTOFF}},
        {"BODY", {P_B_TONE, P_B_SUB, P_B_CLICK, P_VOLUME}},
        {"PUNCH", {P_B_PUNCH, P_B_PTIME, P_AD, P_B_GATE}},
        {"GLIDE", {P_GLIDE, P_B_SLIDE, P_BEND, P_LFO_PITCH}},
        {"DRIVE", {P_B_DRIVE, P_B_DTYPE, P_CUTOFF, P_RESO}},
        {"FILTER", {P_CUTOFF, P_RESO, P_FENV_AMT, P_LFO_FLT}},
        {"", {-1, -1, -1, -1}}, {"", {-1, -1, -1, -1}},
        {"FILTER ENV", {P_FA, P_FD, P_FS, P_FR}},
        {"AMP ENV", {P_AA, P_AD, P_AS, P_AR}},
    },
    {   /* MEL */
        {"PERFORM", {P_CUTOFF, P_FB, P_AA, P_AD}},
        {"ALGO", {P_ALGO, P_FB, P_VOLUME, -1}},
        {"OP 1", {P_OP1_RATIO, P_OP1_LVL, P_OP1_ATT, P_OP1_DEC}},
        {"OP 2", {P_OP2_RATIO, P_OP2_LVL, P_OP2_ATT, P_OP2_DEC}},
        {"OP 3", {P_OP3_RATIO, P_OP3_LVL, P_OP3_ATT, P_OP3_DEC}},
        {"OP 4", {P_OP4_RATIO, P_OP4_LVL, P_OP4_ATT, P_OP4_DEC}},
        {"FILTER", {P_CUTOFF, P_RESO, P_FENV_AMT, P_LFO_FLT}},
        {"VOICE", {P_MODE, P_GLIDE, P_BEND, P_VOLUME}},
        {"FILTER ENV", {P_FA, P_FD, P_FS, P_FR}},
        {"AMP ENV", {P_AA, P_AD, P_AS, P_AR}},
    },
    {   /* SYNTH: OberVave 3.9's pages */
        {"PERFORM", {P_CUTOFF, P_RESO, P_FENV_AMT, P_AR}},
        {"OSC 1", {P_O1_WAVE, P_O1_PW, P_O1_LVL, P_NOISE}},
        {"OSC 2", {P_O2_WAVE, P_O2_PW, P_O2_LVL, P_O2_SEMI}},
        {"VOICE", {P_MODE, P_UNI_N, P_UNI_DET, P_GLIDE}},
        {"OSC MOD", {P_SYNC, P_XMOD, P_ENV_O2, P_O2_FINE}},
        {"FILTER", {P_RESO, P_FENV_AMT, P_KEYTRK, P_FTYPE}},
        {"FILTER +", {P_DRIVE, P_VEL_FLT, P_VEL_AMP, P_FMORPH}},
        {"", {-1, -1, -1, -1}},
        {"FILTER ENV", {P_FA, P_FD, P_FS, P_FR}},
        {"AMP ENV", {P_AA, P_AD, P_AS, P_AR}},
    },
    {   /* DX: a DX7 voice (6 operators, 32 algorithms) under four macros */
        {"PERFORM", {P_DX_BRIGHT, P_DX_ATTACK, P_DX_DECAY, P_DX_RELEASE}},
        {"TONE", {P_DX_ALGO, P_DX_FB, P_VOLUME, -1}},
        {"FILTER", {P_CUTOFF, P_RESO, P_FENV_AMT, P_LFO_FLT}},
        {"VOICE", {P_MODE, P_GLIDE, P_BEND, P_VOLUME}},
        {"", {-1, -1, -1, -1}}, {"", {-1, -1, -1, -1}}, {"", {-1, -1, -1, -1}}, {"", {-1, -1, -1, -1}},
        {"FILTER ENV", {P_FA, P_FD, P_FS, P_FR}},
        {"DX ENV", {P_DX_ATTACK, P_DX_DECAY, P_DX_RELEASE, P_DX_BRIGHT}},
    },
    {   /* STRINGS: the string ensemble */
        {"PERFORM", {P_ST_ENS, P_CUTOFF, P_AA, P_AR}},
        {"REGISTERS", {P_ST_16, P_ST_8, P_ST_4, P_VOLUME}},
        {"FILTER", {P_CUTOFF, P_RESO, P_FENV_AMT, P_LFO_FLT}},
        {"VOICE", {P_MODE, P_GLIDE, P_BEND, P_VOLUME}},
        {"", {-1, -1, -1, -1}}, {"", {-1, -1, -1, -1}}, {"", {-1, -1, -1, -1}}, {"", {-1, -1, -1, -1}},
        {"FILTER ENV", {P_FA, P_FD, P_FS, P_FR}},
        {"AMP ENV", {P_AA, P_AD, P_AS, P_AR}},
    },
    {   /* (0.19) SAMPLER: a take, sliced */
        {"EDIT", {P_SM_ST_C, P_SM_EN_C, P_SM_PITCH, P_SM_TUNE}},
        {"CHOP", {PS_CUT, PS_MOVE, P_SM_SLICES, P_SM_SENS}},
        {"SOUND", {P_SM_QUAL, P_AA, P_AR, P_VOLUME}},
        {"FILTER", {P_CUTOFF, P_RESO, P_FENV_AMT, P_LFO_FLT}},
        {"", {-1, -1, -1, -1}}, {"", {-1, -1, -1, -1}}, {"", {-1, -1, -1, -1}}, {"", {-1, -1, -1, -1}},
        {"FILTER ENV", {P_FA, P_FD, P_FS, P_FR}},
        {"AMP ENV", {P_AA, P_AD, P_AS, P_AR}},
    },
    {   /* (0.30) MIDI: an external device – PERFORM = the four CCs (their names: the CC numbers), MIDI, the CC numbers */
        {"PERFORM", {P_MI_CC1, P_MI_CC2, P_MI_CC3, P_MI_CC4}},
        {"MIDI", {P_MI_CH, P_MI_PROG, P_MI_VEL, -1}},
        {"CC NUMBERS", {P_MI_CC1N, P_MI_CC2N, P_MI_CC3N, P_MI_CC4N}},
        {"", {-1, -1, -1, -1}}, {"", {-1, -1, -1, -1}}, {"", {-1, -1, -1, -1}}, {"", {-1, -1, -1, -1}}, {"", {-1, -1, -1, -1}},
        {"", {-1, -1, -1, -1}}, {"", {-1, -1, -1, -1}},
    },
    {   /* (0.13) DRUMS with the acoustic kit: each drum's tune and decay */
        {"PERFORM", {P_K_DECAY, P_S_DECAY, P_OH_DECAY, P_CUTOFF}},
        {"KICK/SN", {P_K_TUNE, P_K_DECAY, P_S_TUNE, P_S_DECAY}},
        {"HATS", {P_H_TUNE, P_CH_DECAY, P_OH_DECAY, -1}},
        {"TOMS", {P_C_TONE, P_C_DECAY, P_R_TUNE, P_R_DECAY}},
        {"CYMBALS", {P_SN_TONE, P_SN_DECAY, P_P_TUNE, P_P_DECAY}},
        {"KIT", {P_CUTOFF, P_RESO, P_VOLUME, P_KIT_SRC}},
        {"", {-1, -1, -1, -1}}, {"", {-1, -1, -1, -1}},
        {"LEVELS 1-4", {P_DL_KICK, P_DL_SNARE, P_DL_CLAP, P_DL_CH}},
        {"LEVELS 5-8", {P_DL_OH, P_DL_RIM, P_DL_SNAP, P_DL_PERC}},
    },
};
static const page_t PAGE[NPAGE] = {
    [PG_PERFORM] = {"PERFORM", {-1, -1, -1, -1}},   /* PG_PERFORM .. PG_AENV: SPAGE by the sound's kind (PG()) */
    [PG_S1] = {"", {-1, -1, -1, -1}}, [PG_S2] = {"", {-1, -1, -1, -1}}, [PG_S3] = {"", {-1, -1, -1, -1}}, [PG_S4] = {"", {-1, -1, -1, -1}},
    [PG_S5] = {"", {-1, -1, -1, -1}}, [PG_S6] = {"", {-1, -1, -1, -1}}, [PG_S7] = {"", {-1, -1, -1, -1}},
    [PG_FENV] = {"FILTER ENV", {P_FA, P_FD, P_FS, P_FR}},
    [PG_AENV] = {"AMP ENV", {P_AA, P_AD, P_AS, P_AR}},
    [PG_LFO] = {"LFO", {P_LFO_SHAPE, P_LFO_RATE, P_LFO_DELAY, P_LFO_PHASE}},
    [PG_LFODEST] = {"LFO DEST", {P_LFO_PITCH, P_LFO_FLT, P_LFO_AMP, P_LFO_PWM}},
    [PG_ARP] = {"ARP", {P_ARP_MODE, P_ARP_DIV, P_ARP_OCT, P_ARP_GATE}},
    [PG_CHORUS] = {"CHORUS", {P_CHO_RATE, P_CHO_DEPTH, P_CHO_MIX, -1}},
    [PG_DELAY] = {"DELAY", {P_DLY_MIX, P_DLY_TIME, P_DLY_FB, P_DLY_TONE}},      /* MODE first */
    [PG_REVERB] = {"REVERB", {P_REV_MIX, P_REV_SIZE, P_REV_DAMP, P_REV_PRE}},
    [PG_DRIVE] = {"DRIVE", {-1, -1, -1, -1}},   /* the drive send: TYPE, DRIVE, TONE, MIX – the settings', drawn by hand */
    [PG_OUTPUT] = {"OUTPUT", {P_VINTAGE, P_SPREAD, P_BEND, P_VOLUME}},
    [PG_GLOBAL] = {"GLOBAL", {-1, -1, -1, -1}},
    [PG_SCALE] = {"SCALE", {-1, -1, -1, -1}},
    [PG_MIXER] = {"MIXER", {-1, -1, -1, -1}},
    [PG_SEQ] = {"SEQUENCER", {-1, -1, -1, -1}},
};

#include "bm_dxbank.h"
#define DX_VOICE 156u                           /* an unpacked DX7 voice */
typedef struct { uint8_t v[PATCH_BYTES]; uint8_t dx[DX_VOICE]; } patch_t;   /* v[0..NPARAM) the parameters, v[NPARAM..] the name; dx: the DX kind's voice */

static void patch_defaults(patch_t *p)
{
    uint32_t i;
    for (i = 0; i < NPARAM; i++) p->v[i] = PDEF[i].def;
    for (; i < PATCH_BYTES; i++) p->v[i] = ' ';
    for (i = 0; i < DX_VOICE; i++) p->dx[i] = DX_SYNTH[DX_NSYNTH - 1u][i];   /* (INIT VOICE) */
}

/* a factory patch: a name and a list of (parameter, value) pairs over the defaults, ended by 255 */
typedef struct { const char *name; const uint8_t *set; } factory_t;
/* OberVave's defaults where BOOM's table has others (its factory sounds list only what differs from them) */
static const uint8_t OV_DEF[] = {P_TYPE, T_SYNTH, P_CUTOFF, 80, P_RESO, 20, P_FENV_AMT, 90, P_FA, 20, P_FS, 40, P_AA, 10, P_AD, 70, P_AS, 100, 255};
static void patch_from_factory(patch_t *p, const factory_t *f, uint32_t ty)
{
    const uint8_t *s = f->set;
    uint32_t i;
    patch_defaults(p);
    if (ty == T_SYNTH) for (s = OV_DEF; s[0] != 255u; s += 2) p->v[s[0]] = s[1];
    s = f->set;
    while (s[0] != 255u) { p->v[s[0]] = s[1]; s += 2; }
    for (i = 0; i < PATCH_NAME && f->name[i]; i++) p->v[NPARAM + i] = (uint8_t)f->name[i];
}
