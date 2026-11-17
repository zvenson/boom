/* SPDX-License-Identifier: GPL-3.0-only */
/* BOOM – the factory sounds: each a name and the parameters that differ from the defaults (bm_params.h).
 * 1-16 kits (DRUMS), 17-32 basses (the 808), 33-99 MEL (4-op FM); a slot without a sound of its own is that kind's INIT. */
#define X 255
/* ---- kits */
static const uint8_t PK_TRAP[] = {P_TYPE, T_DRUMS, P_K_TUNE, 45, P_K_PUNCH, 80, P_K_DECAY, 70, P_K_DRIVE, 30, P_S_TUNE, 62, P_S_TONE, 75, P_S_DECAY, 55, P_S_SNAP, 90,
    P_C_SPREAD, 55, P_C_TONE, 60, P_C_DECAY, 60, P_H_TUNE, 62, P_H_TONE, 75, P_CH_DECAY, 30, P_OH_DECAY, 65, P_DL_CH, 80, X};
static const uint8_t PK_DARK[] = {P_TYPE, T_DRUMS, P_K_TUNE, 38, P_K_PUNCH, 60, P_K_DECAY, 85, P_K_DRIVE, 50, P_S_TUNE, 50, P_S_TONE, 45, P_S_DECAY, 65, P_S_SNAP, 60,
    P_C_SPREAD, 70, P_C_TONE, 40, P_C_DECAY, 75, P_H_TUNE, 45, P_H_TONE, 50, P_CH_DECAY, 25, P_OH_DECAY, 55, P_CUTOFF, 100, P_DL_CH, 70, P_DL_OH, 70, X};
static const uint8_t PK_LOFI[] = {P_TYPE, T_DRUMS, P_K_TUNE, 55, P_K_PUNCH, 40, P_K_DECAY, 45, P_K_DRIVE, 70, P_S_TUNE, 70, P_S_TONE, 40, P_S_DECAY, 35, P_S_SNAP, 50,
    P_C_SPREAD, 40, P_C_TONE, 35, P_C_DECAY, 40, P_C_DRIVE, 60, P_H_TUNE, 40, P_H_TONE, 40, P_CH_DECAY, 20, P_OH_DECAY, 40, P_CUTOFF, 84, P_RESO, 20, X};
static const uint8_t PK_DRILL[] = {P_TYPE, T_DRUMS, P_K_TUNE, 50, P_K_PUNCH, 95, P_K_DECAY, 40, P_K_DRIVE, 25, P_S_TUNE, 80, P_S_TONE, 100, P_S_DECAY, 30, P_S_SNAP, 110,
    P_C_SPREAD, 30, P_C_TONE, 90, P_C_DECAY, 35, P_H_TUNE, 85, P_H_TONE, 105, P_CH_DECAY, 18, P_OH_DECAY, 45, P_R_TUNE, 85, P_SN_TONE, 100, X};
static const uint8_t PK_CLEAN[] = {P_TYPE, T_DRUMS, P_K_TUNE, 50, P_K_PUNCH, 55, P_K_DECAY, 55, P_K_DRIVE, 0, P_S_TUNE, 60, P_S_TONE, 65, P_S_DECAY, 45, P_S_SNAP, 70,
    P_C_SPREAD, 50, P_C_TONE, 65, P_C_DECAY, 50, P_C_DRIVE, 0, P_H_TUNE, 60, P_H_TONE, 70, P_CH_DECAY, 28, P_OH_DECAY, 60, X};
static const uint8_t PK_BOOM[] = {P_TYPE, T_DRUMS, P_K_TUNE, 30, P_K_PUNCH, 70, P_K_DECAY, 110, P_K_DRIVE, 60, P_S_TUNE, 55, P_S_TONE, 55, P_S_DECAY, 70, P_S_SNAP, 75,
    P_C_SPREAD, 60, P_C_TONE, 50, P_C_DECAY, 80, P_H_TUNE, 55, P_H_TONE, 65, P_CH_DECAY, 35, P_OH_DECAY, 80, P_P_TUNE, 35, P_P_DECAY, 70, X};
static const uint8_t PK_TIGHT[] = {P_TYPE, T_DRUMS, P_K_TUNE, 58, P_K_PUNCH, 85, P_K_DECAY, 30, P_K_DRIVE, 15, P_S_TUNE, 75, P_S_TONE, 85, P_S_DECAY, 25, P_S_SNAP, 95,
    P_C_SPREAD, 35, P_C_TONE, 75, P_C_DECAY, 30, P_H_TUNE, 70, P_H_TONE, 85, P_CH_DECAY, 15, P_OH_DECAY, 35, P_R_DECAY, 20, P_SN_DECAY, 25, X};
static const uint8_t PK_METAL[] = {P_TYPE, T_DRUMS, P_K_TUNE, 48, P_K_PUNCH, 75, P_K_DECAY, 60, P_K_DRIVE, 90, P_S_TUNE, 65, P_S_TONE, 90, P_S_DECAY, 50, P_S_SNAP, 100,
    P_C_SPREAD, 50, P_C_TONE, 80, P_C_DECAY, 55, P_C_DRIVE, 80, P_H_TUNE, 90, P_H_TONE, 110, P_CH_DECAY, 40, P_OH_DECAY, 90, P_P_TONE, 110, X};
static const uint8_t PK_INIT[] = {P_TYPE, T_DRUMS, X};
/* (0.13) the acoustic kit: TUNE 64 = as recorded, DECAY 127 = the whole sample */
#define AK(kt, kd, st, sd, tt, td, ht, cd, od, rt, rd, ct, cdd) P_TYPE, T_DRUMS, P_KIT_SRC, 1, P_K_TUNE, kt, P_K_DECAY, kd, P_S_TUNE, st, P_S_DECAY, sd, \
    P_C_TONE, tt, P_C_DECAY, td, P_R_TUNE, tt, P_R_DECAY, td, P_H_TUNE, ht, P_CH_DECAY, cd, P_OH_DECAY, od, P_SN_TONE, rt, P_SN_DECAY, rd, P_P_TUNE, ct, P_P_DECAY, cdd, \
    P_DL_KICK, 100, P_DL_SNARE, 100, P_DL_CLAP, 100, P_DL_CH, 100, P_DL_OH, 100, P_DL_RIM, 100, P_DL_SNAP, 100, P_DL_PERC, 100, P_CUTOFF, 127, P_RESO, 0
static const uint8_t PK_REAL[] = {AK(64, 127, 64, 127, 64, 127, 64, 127, 127, 64, 127, 64, 127), X};
static const uint8_t PK_RTIGHT[] = {AK(66, 78, 68, 84, 70, 80, 66, 90, 84, 64, 100, 64, 104), X};
static const uint8_t PK_RDEEP[] = {AK(50, 127, 54, 127, 50, 127, 60, 127, 127, 58, 127, 58, 127), X};
static const uint8_t PK_RDUSTY[] = {AK(44, 100, 46, 100, 44, 100, 48, 100, 96, 46, 110, 46, 110), P_CUTOFF, 96, X};
/* (0.27, Sven: "punchy drums aus der New-York-Zeit") the HIP HOP kit: the same knobs, P_KIT_SRC 2 */
#define HK(...) AK(__VA_ARGS__), P_KIT_SRC, 2
static const uint8_t PK_NY[] = {HK(64, 127, 64, 127, 64, 127, 64, 127, 127, 64, 127, 64, 127), X};
static const uint8_t PK_BOOMBAP[] = {HK(66, 88, 66, 90, 68, 88, 66, 84, 80, 64, 96, 64, 100), X};
static const uint8_t PK_NYDUSTY[] = {HK(58, 110, 60, 110, 60, 110, 58, 100, 100, 58, 110, 58, 110), P_CUTOFF, 100, X};
static const uint8_t PK_NYHEAVY[] = {HK(54, 127, 56, 127, 56, 127, 62, 110, 120, 52, 127, 56, 127), X};
#undef AK
static const factory_t FACT_DRUMS[] = {
    {"TRAP KIT", PK_TRAP}, {"DARK KIT", PK_DARK}, {"LO-FI KIT", PK_LOFI}, {"DRILL KIT", PK_DRILL},
    {"CLEAN KIT", PK_CLEAN}, {"BOOM KIT", PK_BOOM}, {"TIGHT KIT", PK_TIGHT}, {"METAL KIT", PK_METAL},
    {"REAL KIT", PK_REAL}, {"REAL TIGHT", PK_RTIGHT}, {"REAL DEEP", PK_RDEEP}, {"REAL DUSTY", PK_RDUSTY},
    {"NY KIT", PK_NY}, {"BOOM BAP", PK_BOOMBAP}, {"NY DUSTY", PK_NYDUSTY}, {"NY HEAVY", PK_NYHEAVY},
};
static const factory_t FACT_DRUMS_INIT = {"INIT KIT", PK_INIT};

/* ---- the 808 */
static const uint8_t PB_CLEAN[] = {P_TYPE, T_BASS, P_B_PUNCH, 50, P_B_PTIME, 35, P_B_CLICK, 20, P_B_TONE, 20, P_B_DRIVE, 10, P_AD, 85, P_GLIDE, 30, X};
static const uint8_t PB_LONG[] = {P_TYPE, T_BASS, P_B_PUNCH, 55, P_B_PTIME, 40, P_B_CLICK, 25, P_B_TONE, 25, P_B_DRIVE, 25, P_AD, 110, P_GLIDE, 40, X};
static const uint8_t PB_PUNCHY[] = {P_TYPE, T_BASS, P_B_PUNCH, 100, P_B_PTIME, 30, P_B_CLICK, 60, P_B_TONE, 40, P_B_DRIVE, 35, P_AD, 75, P_GLIDE, 25, X};
static const uint8_t PB_DIST[] = {P_TYPE, T_BASS, P_B_PUNCH, 60, P_B_PTIME, 40, P_B_CLICK, 30, P_B_TONE, 50, P_B_DRIVE, 95, P_B_DTYPE, BD_HARD, P_CUTOFF, 96, P_AD, 90, P_GLIDE, 35, X};
static const uint8_t PB_GLIDE[] = {P_TYPE, T_BASS, P_B_PUNCH, 45, P_B_PTIME, 35, P_B_CLICK, 15, P_B_TONE, 25, P_B_DRIVE, 30, P_B_SLIDE, BS_ALWAYS, P_AD, 95, P_GLIDE, 70, X};
static const uint8_t PB_SHORT[] = {P_TYPE, T_BASS, P_B_PUNCH, 80, P_B_PTIME, 25, P_B_CLICK, 50, P_B_TONE, 35, P_B_DRIVE, 20, P_AD, 55, P_GLIDE, 15, X};
static const uint8_t PB_DEEP[] = {P_TYPE, T_BASS, P_B_PUNCH, 40, P_B_PTIME, 50, P_B_CLICK, 10, P_B_TONE, 10, P_B_SUB, 80, P_B_DRIVE, 15, P_CUTOFF, 70, P_AD, 100, P_GLIDE, 40, X};
static const uint8_t PB_GRIT[] = {P_TYPE, T_BASS, P_B_PUNCH, 65, P_B_PTIME, 35, P_B_CLICK, 40, P_B_TONE, 60, P_B_DRIVE, 110, P_B_DTYPE, BD_FOLD, P_CUTOFF, 90, P_RESO, 30, P_AD, 85, P_GLIDE, 30, X};
static const uint8_t PB_INIT[] = {P_TYPE, T_BASS, P_GLIDE, 30, X};
static const factory_t FACT_BASS[] = {
    {"CLEAN 808", PB_CLEAN}, {"LONG 808", PB_LONG}, {"PUNCHY", PB_PUNCHY}, {"DISTORTED", PB_DIST},
    {"GLIDE 808", PB_GLIDE}, {"SHORT 808", PB_SHORT}, {"DEEP SUB", PB_DEEP}, {"GRIT 808", PB_GRIT},
};
static const factory_t FACT_BASS_INIT = {"INIT BASS", PB_INIT};

/* ---- MEL: 4-op FM. The algorithms (bm_engine.h): 0 4>3>2>1 · 1 (3+4)>2>1 · 2 4>3>1, 2>1 · 3 (2+3+4)>1 · 4 4>3, 2>1 ·
 * 5 4>(1,2,3) · 6 4>3, 1 2 3 · 7 1 2 3 4 (all carriers) */
#define OPS(r1, l1, a1, d1, r2, l2, a2, d2, r3, l3, a3, d3, r4, l4, a4, d4) \
    P_OP1_RATIO, r1, P_OP1_LVL, l1, P_OP1_ATT, a1, P_OP1_DEC, d1, P_OP2_RATIO, r2, P_OP2_LVL, l2, P_OP2_ATT, a2, P_OP2_DEC, d2, \
    P_OP3_RATIO, r3, P_OP3_LVL, l3, P_OP3_ATT, a3, P_OP3_DEC, d3, P_OP4_RATIO, r4, P_OP4_LVL, l4, P_OP4_ATT, a4, P_OP4_DEC, d4
/* ratio indices: 1=0.5 3=1 5=1.5 6=2 8=3 10=4 12=5 15=7 18=10 22=14 */
static const uint8_t PM_DARKBELL[] = {P_ALGO, 4, OPS(3, 100, 0, 110, 10, 55, 0, 85, 5, 70, 0, 100, 22, 45, 0, 70), P_AA, 0, P_AD, 100, P_AS, 0, P_AR, 90, P_CUTOFF, 100, P_REV_MIX, 40, P_REV_SIZE, 100, X};
static const uint8_t PM_GLASS[] = {P_ALGO, 0, OPS(3, 100, 0, 95, 6, 60, 0, 70, 8, 40, 0, 55, 18, 30, 0, 45), P_AA, 0, P_AD, 85, P_AS, 0, P_AR, 70, P_CUTOFF, 110, P_DLY_MIX, 35, P_DLY_TIME, DD_8D, X};
static const uint8_t PM_MUSICBOX[] = {P_ALGO, 2, OPS(3, 100, 0, 100, 10, 45, 0, 70, 6, 50, 0, 80, 18, 50, 0, 60), P_AA, 0, P_AD, 95, P_AS, 0, P_AR, 85, P_REV_MIX, 45, P_REV_SIZE, 110, X};
static const uint8_t PM_MALLET[] = {P_ALGO, 1, OPS(3, 100, 0, 90, 8, 45, 0, 60, 10, 35, 0, 50, 6, 40, 0, 65), P_AA, 0, P_AD, 80, P_AS, 0, P_AR, 60, X};
static const uint8_t PM_EPIANO[] = {P_ALGO, 4, OPS(3, 100, 0, 110, 22, 35, 0, 60, 3, 80, 0, 105, 3, 50, 0, 80), P_FB, 30, P_AA, 0, P_AD, 105, P_AS, 20, P_AR, 70, P_LFO_AMP, 20, P_LFO_RATE, 75, P_CHO_MIX, 30, X};
static const uint8_t PM_FLUTE[] = {P_ALGO, 0, OPS(3, 100, 30, 127, 3, 25, 40, 127, 6, 10, 30, 127, 3, 0, 0, 127), P_AA, 35, P_AD, 90, P_AS, 90, P_AR, 60, P_LFO_PITCH, 12, P_LFO_RATE, 78, P_LFO_DELAY, 50, P_CUTOFF, 100, P_REV_MIX, 35, X};
static const uint8_t PM_CHOIR[] = {P_ALGO, 5, OPS(3, 90, 60, 127, 6, 60, 60, 127, 5, 50, 60, 127, 3, 25, 50, 127), P_AA, 80, P_AD, 100, P_AS, 100, P_AR, 95, P_LFO_PITCH, 6, P_LFO_RATE, 60, P_CUTOFF, 92, P_CHO_MIX, 50, P_REV_MIX, 50, P_REV_SIZE, 115, X};
static const uint8_t PM_DARKPAD[] = {P_ALGO, 6, OPS(3, 90, 70, 127, 1, 70, 80, 127, 5, 50, 70, 127, 18, 30, 0, 100), P_AA, 90, P_AD, 100, P_AS, 100, P_AR, 105, P_CUTOFF, 70, P_RESO, 20, P_FENV_AMT, 90, P_FA, 100, P_FD, 110, P_FS, 40, P_REV_MIX, 55, P_REV_SIZE, 118, X};
static const uint8_t PM_STRINGS[] = {P_ALGO, 7, OPS(3, 80, 70, 127, 6, 40, 70, 127, 1, 50, 80, 127, 5, 25, 70, 127), P_AA, 70, P_AD, 100, P_AS, 100, P_AR, 90, P_LFO_PITCH, 10, P_LFO_RATE, 72, P_LFO_DELAY, 40, P_CUTOFF, 95, P_CHO_MIX, 60, P_REV_MIX, 40, X};
static const uint8_t PM_BRASS[] = {P_ALGO, 1, OPS(3, 100, 10, 127, 3, 70, 15, 95, 3, 55, 20, 90, 3, 0, 0, 127), P_FB, 20, P_AA, 15, P_AD, 90, P_AS, 80, P_AR, 50, P_CUTOFF, 105, X};
static const uint8_t PM_SUBLEAD[] = {P_ALGO, 0, OPS(3, 100, 0, 127, 3, 40, 0, 100, 3, 0, 0, 127, 3, 0, 0, 127), P_MODE, M_MONO, P_GLIDE, 25, P_AA, 0, P_AD, 90, P_AS, 100, P_AR, 40, P_CUTOFF, 90, X};
static const uint8_t PM_SINELEAD[] = {P_ALGO, 0, OPS(3, 100, 0, 127, 3, 15, 0, 100, 3, 0, 0, 127, 3, 0, 0, 127), P_MODE, M_MONO, P_GLIDE, 35, P_AA, 2, P_AD, 90, P_AS, 100, P_AR, 50, P_LFO_PITCH, 8, P_LFO_RATE, 76, P_LFO_DELAY, 60, P_DLY_MIX, 40, X};
static const uint8_t PM_ORGAN[] = {P_ALGO, 7, OPS(3, 100, 0, 127, 6, 70, 0, 127, 8, 45, 0, 127, 10, 35, 0, 127), P_AA, 2, P_AD, 90, P_AS, 100, P_AR, 20, X};
static const uint8_t PM_HARP[] = {P_ALGO, 2, OPS(3, 100, 0, 95, 6, 40, 0, 65, 10, 35, 0, 55, 18, 25, 0, 45), P_AA, 0, P_AD, 90, P_AS, 0, P_AR, 80, P_REV_MIX, 35, X};
static const uint8_t PM_BELLARP[] = {P_ALGO, 3, OPS(3, 100, 0, 85, 10, 40, 0, 55, 22, 30, 0, 45, 6, 35, 0, 65), P_AA, 0, P_AD, 75, P_AS, 0, P_AR, 60, P_ARP_MODE, A_UP, P_ARP_DIV, D_16, P_ARP_OCT, 2, P_DLY_MIX, 40, P_DLY_TIME, DD_8D, P_REV_MIX, 30, X};
static const uint8_t PM_LOFIKEYS[] = {P_ALGO, 4, OPS(3, 100, 0, 100, 6, 40, 0, 70, 3, 70, 0, 95, 22, 20, 0, 50), P_FB, 45, P_AA, 0, P_AD, 95, P_AS, 10, P_AR, 60, P_CUTOFF, 72, P_RESO, 25, P_LFO_PITCH, 6, P_LFO_RATE, 66, P_CHO_MIX, 40, X};
static const uint8_t PM_INIT[] = {P_TYPE, T_MEL, X};
static const factory_t FACT_MEL[] = {
    {"DARK BELL", PM_DARKBELL}, {"GLASS PLCK", PM_GLASS}, {"MUSIC BOX", PM_MUSICBOX}, {"MALLET", PM_MALLET},
    {"E-PIANO", PM_EPIANO}, {"FLUTE", PM_FLUTE}, {"CHOIR PAD", PM_CHOIR}, {"DARK PAD", PM_DARKPAD},
    {"STRINGS", PM_STRINGS}, {"BRASS STAB", PM_BRASS}, {"SUB LEAD", PM_SUBLEAD}, {"SINE LEAD", PM_SINELEAD},
    {"ORGAN", PM_ORGAN}, {"HARP PLUCK", PM_HARP}, {"BELL ARP", PM_BELLARP}, {"LO-FI KEYS", PM_LOFIKEYS},
};
static const factory_t FACT_MEL_INIT = {"INIT MEL", PM_INIT};
#undef OPS
/* ---- SYNTH: OberVave 3.9's 64 factory sounds (obv_patches.h), slots 65-128 */
/* the dark arpeggios: a four-note chord climbed in 16ths, two saws a hair apart, the 24 dB filter half shut, a
 * dotted-eighth ping-pong and a long dark plate */
static const uint8_t PF_DARK_ARP[] = {P_ARP_MODE, A_UP, P_ARP_DIV, D_16, P_ARP_OCT, 1, P_ARP_GATE, 75, P_O2_FINE, 69, P_O2_LVL, 105,
    P_CUTOFF, 46, P_RESO, 22, P_FENV_AMT, 92, P_FA, 0, P_FD, 58, P_FS, 35, P_FR, 55, P_AA, 1, P_AD, 72, P_AS, 55, P_AR, 60,
    P_KEYTRK, 70, P_VINTAGE, 55, P_SPREAD, 50, P_CHO_MIX, 25, P_CHO_RATE, 30, P_CHO_DEPTH, 40,
    P_DLY_TIME, DD_8D, P_DLY_FB, 48, P_DLY_TONE, 45, P_DLY_MIX, 58, P_REV_SIZE, 100, P_REV_DAMP, 75, P_REV_PRE, 30, P_REV_MIX, 52, X};
static const uint8_t PF_UPSIDE_ARP[] = {P_ARP_MODE, A_UPDN, P_ARP_DIV, D_16, P_ARP_OCT, 2, P_ARP_GATE, 60, P_O1_WAVE, W_BOTH, P_O1_PW, 40,
    P_O2_WAVE, W_SAW, P_O2_SEMI, 12, P_O2_LVL, 70, P_O2_FINE, 72, P_FTYPE, F_SEM, P_FMORPH, 18, P_CUTOFF, 40, P_RESO, 35,
    P_FENV_AMT, 100, P_FD, 50, P_FS, 10, P_AA, 1, P_AD, 60, P_AS, 20, P_AR, 70, P_VINTAGE, 70, P_SPREAD, 80,
    P_DLY_TIME, DD_8, P_DLY_FB, 62, P_DLY_TONE, 35, P_DLY_MIX, 64, P_REV_SIZE, 115, P_REV_DAMP, 85, P_REV_PRE, 40, P_REV_MIX, 66, X};
static const uint8_t PF_NEON_PAD[] = {P_O1_WAVE, W_PLS, P_O1_PW, 50, P_O2_WAVE, W_BOTH, P_O2_PW, 80, P_O2_FINE, 74, P_LFO_PWM, 45,
    P_LFO_RATE, 42, P_LFO_PHASE, LP_SPREAD, P_CUTOFF, 52, P_RESO, 12, P_FENV_AMT, 80, P_FA, 90, P_FD, 100, P_FS, 50, P_FR, 100,
    P_AA, 85, P_AR, 110, P_VINTAGE, 60, P_SPREAD, 120, P_CHO_MIX, 70, P_CHO_RATE, 25, P_CHO_DEPTH, 60,
    P_REV_SIZE, 118, P_REV_DAMP, 60, P_REV_PRE, 50, P_REV_MIX, 70, P_DLY_TIME, DD_4D, P_DLY_FB, 40, P_DLY_TONE, 30, P_DLY_MIX, 25, X};
static const uint8_t PF_BRASS_STAB[] = {P_O2_FINE, 72, P_CUTOFF, 60, P_FENV_AMT, 114, P_FA, 30, P_FD, 75, P_FS, 30, P_FR, 50,
    P_AA, 12, P_AD, 60, P_AS, 90, P_AR, 45, P_RESO, 25, P_DRIVE, 20, X};
static const uint8_t PF_POLY_BRASS[] = {P_O2_FINE, 71, P_CUTOFF, 55, P_FENV_AMT, 104, P_FA, 55, P_FD, 80, P_FS, 45, P_FR, 60,
    P_AA, 40, P_AR, 55, P_LFO_PITCH, 20, P_LFO_DELAY, 60, P_LFO_RATE, 75, P_RESO, 15, X};
static const uint8_t PF_SYNC_LEAD[] = {P_MODE, M_MONO, P_O1_LVL, 0, P_O2_LVL, 120, P_SYNC, 1, P_ENV_O2, 104, P_O2_SEMI, 31,
    P_FD, 80, P_FS, 20, P_CUTOFF, 90, P_RESO, 10, P_GLIDE, 20, P_AA, 2, P_AR, 40, P_FENV_AMT, 80, P_DLY_TIME, DD_8D, P_DLY_FB, 40, P_DLY_MIX, 35, P_REV_SIZE, 70, P_REV_MIX, 22, X};
static const uint8_t PF_UNISON_LEAD[] = {P_MODE, M_UNISON, P_UNI_N, 6, P_UNI_DET, 50, P_O2_FINE, 67, P_CUTOFF, 85, P_RESO, 15,
    P_FA, 10, P_AA, 5, P_GLIDE, 25, P_LFO_PITCH, 10, P_LFO_DELAY, 70, P_SPREAD, 100, P_AR, 50, P_DLY_TIME, DD_4, P_DLY_FB, 35, P_DLY_MIX, 30, P_REV_MIX, 25, X};
static const uint8_t PF_PWM_STRINGS[] = {P_O1_WAVE, W_PLS, P_O2_WAVE, W_PLS, P_O2_PW, 40, P_O2_FINE, 73, P_LFO_PWM, 60,
    P_LFO_RATE, 50, P_LFO_PHASE, LP_SPREAD, P_AA, 60, P_AR, 90, P_FA, 50, P_CUTOFF, 75, P_FENV_AMT, 74, P_RESO, 5,
    P_SPREAD, 100, P_VINTAGE, 50, P_CHO_MIX, 55, P_CHO_RATE, 35, P_REV_SIZE, 100, P_REV_MIX, 35, X};
static const uint8_t PF_BELL_XMOD[] = {P_O1_LVL, 100, P_O2_SEMI, 43, P_XMOD, 70, P_CUTOFF, 100, P_FD, 90, P_FS, 0, P_AD, 95,
    P_AS, 0, P_AR, 80, P_FENV_AMT, 94, P_AA, 1, P_DLY_TIME, DD_8, P_DLY_FB, 50, P_DLY_MIX, 40, P_REV_SIZE, 110, P_REV_MIX, 45, X};
static const uint8_t PF_FAT_BASS[] = {P_MODE, M_MONO, P_O2_WAVE, W_PLS, P_O2_PW, 50, P_O2_SEMI, 12, P_CUTOFF, 40, P_RESO, 30,
    P_FENV_AMT, 109, P_FD, 60, P_FS, 10, P_AA, 2, P_AR, 30, P_DRIVE, 40, P_GLIDE, 10, X};
static const uint8_t PF_ARP_SEQ[] = {P_ARP_MODE, A_UP, P_ARP_DIV, D_16, P_ARP_OCT, 2, P_ARP_GATE, 50, P_O1_WAVE, W_PLS, P_O1_PW, 40,
    P_O2_SEMI, 12, P_CUTOFF, 50, P_FENV_AMT, 124, P_FD, 45, P_FS, 0, P_AD, 50, P_AS, 0, P_AR, 40, P_RESO, 40, P_AA, 1, P_DLY_TIME, DD_8D, P_DLY_FB, 40, P_DLY_MIX, 35, X};
static const uint8_t PF_SWEEP_PAD[] = {P_O2_FINE, 74, P_CUTOFF, 30, P_FENV_AMT, 127, P_FA, 110, P_FD, 100, P_FS, 60, P_FR, 100,
    P_AA, 90, P_AR, 105, P_RESO, 20, P_LFO_FLT, 15, P_LFO_RATE, 30, P_SPREAD, 110, P_VINTAGE, 60, P_CHO_MIX, 40, P_REV_SIZE, 110, P_REV_MIX, 45, X};
static const uint8_t PF_BP_QUACK[] = {P_FTYPE, F_BP12, P_RESO, 70, P_CUTOFF, 50, P_FENV_AMT, 124, P_FD, 55, P_FS, 0, P_AD, 80,
    P_AS, 70, P_O2_WAVE, W_PLS, P_O2_PW, 30, P_AA, 1, X};
static const uint8_t PF_DUAL_SAW[] = {P_MODE, M_DUAL, P_UNI_DET, 60, P_O2_WAVE, W_OFF, P_CUTOFF, 70, P_SPREAD, 110, X};
static const uint8_t PF_RESO_ZAP[] = {P_MODE, M_MONO, P_RESO, 110, P_CUTOFF, 20, P_FENV_AMT, 127, P_FD, 40, P_FS, 0, P_AD, 40,
    P_AS, 0, P_FTYPE, F_LP12, P_O1_WAVE, W_PLS, P_NOISE, 20, P_AA, 1, P_AR, 30, X};
static const uint8_t PF_NOISE_WASH[] = {P_NOISE, 100, P_O1_WAVE, W_OFF, P_O2_WAVE, W_OFF, P_FTYPE, F_BP12, P_RESO, 50, P_CUTOFF, 70,
    P_LFO_FLT, 60, P_LFO_RATE, 20, P_AA, 100, P_AR, 110, P_FA, 60, P_FENV_AMT, 64, P_REV_SIZE, 127, P_REV_DAMP, 40, P_REV_MIX, 60, X};
static const uint8_t PF_HOLLOW_PAD[] = {P_O1_WAVE, W_PLS, P_O1_PW, 20, P_O2_WAVE, W_PLS, P_O2_PW, 108, P_O2_FINE, 76, P_FTYPE, F_NOTCH,
    P_CUTOFF, 70, P_LFO_PWM, 40, P_LFO_RATE, 40, P_LFO_PHASE, LP_SPREAD, P_AA, 70, P_AR, 100, P_FENV_AMT, 64, P_CHO_MIX, 50, P_REV_SIZE, 100, P_REV_MIX, 40, X};
static const uint8_t PF_SQUARE_LEAD[] = {P_MODE, M_MONO, P_O1_WAVE, W_PLS, P_O2_WAVE, W_PLS, P_O2_FINE, 69, P_CUTOFF, 90, P_RESO, 20,
    P_GLIDE, 30, P_LFO_PITCH, 8, P_LFO_DELAY, 50, P_DRIVE, 30, P_AA, 3, P_AR, 40, P_DLY_TIME, DD_8, P_DLY_FB, 35, P_DLY_MIX, 30, X};
static const uint8_t PF_SUB_BASS[] = {P_MODE, M_MONO, P_O1_WAVE, W_PLS, P_O2_SEMI, 12, P_O2_LVL, 60, P_CUTOFF, 35, P_RESO, 10,
    P_FENV_AMT, 84, P_FD, 50, P_FS, 20, P_AA, 1, P_AD, 60, P_AR, 20, P_GLIDE, 5, X};
static const uint8_t PF_SOFT_KEYS[] = {P_O1_LVL, 90, P_O2_WAVE, W_PLS, P_O2_PW, 30, P_O2_LVL, 60, P_O2_SEMI, 36, P_CUTOFF, 60,
    P_FENV_AMT, 99, P_FD, 70, P_FS, 20, P_AA, 3, P_AD, 90, P_AS, 60, P_AR, 60, P_VEL_FLT, 80, P_VEL_AMP, 90, P_RESO, 10, P_CHO_MIX, 30, P_REV_SIZE, 80, P_REV_MIX, 30, X};
static const uint8_t PF_SYNC_BRASS[] = {P_SYNC, 1, P_O1_LVL, 60, P_O2_LVL, 110, P_O2_SEMI, 29, P_ENV_O2, 89, P_CUTOFF, 65,
    P_FENV_AMT, 104, P_FA, 20, P_FD, 70, P_FS, 40, P_AA, 10, P_AR, 50, X};
static const uint8_t PF_DRONE_UNI[] = {P_MODE, M_UNISON, P_UNI_N, 8, P_UNI_DET, 80, P_O2_SEMI, 12, P_CUTOFF, 45, P_FENV_AMT, 79,
    P_FA, 100, P_AA, 100, P_AR, 110, P_LFO_FLT, 20, P_LFO_RATE, 25, P_RESO, 10, P_VINTAGE, 80, P_SPREAD, 127, P_REV_SIZE, 120, P_REV_DAMP, 80, P_REV_MIX, 50, X};
static const uint8_t PF_CLAV_PLUCK[] = {P_O1_WAVE, W_PLS, P_O1_PW, 20, P_O2_WAVE, W_PLS, P_O2_PW, 20, P_O2_FINE, 67, P_FTYPE, F_LP12,
    P_RESO, 40, P_CUTOFF, 55, P_FENV_AMT, 127, P_FD, 35, P_FS, 0, P_AD, 60, P_AS, 0, P_AR, 30, P_KEYTRK, 127, P_AA, 1, X};
static const uint8_t PF_WOBBLE_BASS[] = {P_MODE, M_MONO, P_O2_SEMI, 12, P_CUTOFF, 35, P_RESO, 50, P_LFO_FLT, 70, P_LFO_RATE, 45,
    P_DRIVE, 50, P_AA, 2, P_AS, 127, P_AR, 20, P_FS, 100, P_FENV_AMT, 64, X};
static const uint8_t PF_ARP_PLUCK[] = {P_ARP_MODE, A_UPDN, P_ARP_DIV, D_16, P_ARP_OCT, 3, P_ARP_GATE, 40, P_O2_WAVE, W_PLS,
    P_O2_PW, 40, P_O2_SEMI, 36, P_CUTOFF, 60, P_FENV_AMT, 114, P_FD, 40, P_FS, 0, P_AD, 45, P_AS, 0, P_AR, 50, P_RESO, 30,
    P_SPREAD, 100, P_AA, 1, P_DLY_TIME, DD_8D, P_DLY_FB, 45, P_DLY_TONE, 50, P_DLY_MIX, 45, P_REV_SIZE, 90, P_REV_MIX, 28, X};
static const uint8_t PF_ARP_CHORD[] = {P_ARP_MODE, A_PLAY, P_ARP_DIV, D_8, P_ARP_OCT, 1, P_ARP_GATE, 80, P_MODE, M_DUAL,
    P_UNI_DET, 40, P_CUTOFF, 70, P_AA, 5, P_AR, 60, P_DLY_TIME, DD_8, P_DLY_FB, 30, P_DLY_MIX, 30, P_REV_MIX, 25, X};
static const uint8_t PF_RANDOM_ARP[] = {P_ARP_MODE, A_RND, P_ARP_DIV, D_16T, P_ARP_OCT, 2, P_ARP_GATE, 30, P_O1_WAVE, W_PLS,
    P_O2_SEMI, 31, P_FTYPE, F_BP12, P_RESO, 50, P_CUTOFF, 70, P_FD, 40, P_FS, 0, P_AD, 50, P_AS, 0, P_AA, 1, P_DLY_TIME, DD_8T, P_DLY_FB, 50, P_DLY_MIX, 45, P_REV_MIX, 30, X};
static const uint8_t PF_FIFTHS_LEAD[] = {P_MODE, M_MONO, P_O2_SEMI, 31, P_CUTOFF, 85, P_RESO, 15, P_GLIDE, 15, P_DRIVE, 25,
    P_LFO_PITCH, 10, P_LFO_DELAY, 60, P_AA, 2, P_AR, 40, P_DLY_TIME, DD_8D, P_DLY_FB, 40, P_DLY_MIX, 35, P_REV_MIX, 20, X};
static const uint8_t PF_VINTAGE_PAD[] = {P_VINTAGE, 127, P_O2_FINE, 73, P_CUTOFF, 55, P_FENV_AMT, 94, P_FA, 70, P_AA, 60, P_AR, 90,
    P_FR, 90, P_RESO, 15, P_SPREAD, 120, P_CHO_MIX, 45, P_REV_SIZE, 105, P_REV_MIX, 45, X};
static const uint8_t PF_XMOD_GROWL[] = {P_MODE, M_MONO, P_XMOD, 90, P_O2_SEMI, 19, P_CUTOFF, 50, P_FENV_AMT, 114, P_FD, 60, P_FS, 30,
    P_DRIVE, 40, P_AA, 2, P_AR, 40, X};
static const uint8_t PF_PERC_HIT[] = {P_O2_SEMI, 48, P_NOISE, 60, P_CUTOFF, 90, P_FENV_AMT, 124, P_FD, 25, P_FS, 0, P_AD, 30, P_AS, 0,
    P_AR, 25, P_FTYPE, F_LP12, P_RESO, 30, P_AA, 1, P_REV_SIZE, 90, P_REV_MIX, 40, X};
/* 33-64 (1.5): a second set in the same spirit: arpeggios, brass, pads, keys, basses, leads, effects */
static const uint8_t PF_NIGHT_DRIVE[] = {P_ARP_MODE, A_UP, P_ARP_DIV, D_16, P_ARP_OCT, 2, P_ARP_GATE, 55, P_O1_WAVE, W_BOTH, P_O1_PW, 45,
    P_O2_FINE, 72, P_O2_LVL, 90, P_CUTOFF, 52, P_RESO, 38, P_FENV_AMT, 104, P_FA, 0, P_FD, 48, P_FS, 15, P_AA, 1, P_AD, 60, P_AS, 40,
    P_AR, 50, P_KEYTRK, 80, P_SPREAD, 90, P_CHO_MIX, 30, P_DLY_TIME, DD_8D, P_DLY_FB, 55, P_DLY_TONE, 40, P_DLY_MIX, 55,
    P_REV_SIZE, 95, P_REV_MIX, 40, X};
static const uint8_t PF_MINOR_FALL[] = {P_ARP_MODE, A_DOWN, P_ARP_DIV, D_16, P_ARP_OCT, 2, P_ARP_GATE, 45, P_O2_WAVE, W_PLS, P_O2_PW, 35,
    P_O2_FINE, 70, P_CUTOFF, 48, P_RESO, 30, P_FENV_AMT, 110, P_FD, 42, P_FS, 0, P_AA, 1, P_AD, 55, P_AS, 0, P_AR, 55,
    P_DLY_TIME, DD_8, P_DLY_FB, 50, P_DLY_TONE, 35, P_DLY_MIX, 50, P_REV_SIZE, 105, P_REV_MIX, 45, X};
static const uint8_t PF_SHADOW_ARP[] = {P_ARP_MODE, A_UPDN, P_ARP_DIV, D_8T, P_ARP_OCT, 2, P_ARP_GATE, 50, P_FTYPE, F_BP12, P_RESO, 62, P_VOLUME, 127,
    P_CUTOFF, 62, P_FENV_AMT, 100, P_FD, 50, P_FS, 10, P_O2_SEMI, 31, P_O2_LVL, 70, P_AA, 1, P_AD, 60, P_AS, 20, P_AR, 60,
    P_LFO_FLT, 20, P_LFO_RATE, 35, P_DLY_TIME, DD_4, P_DLY_FB, 60, P_DLY_TONE, 30, P_DLY_MIX, 55, P_REV_SIZE, 115, P_REV_MIX, 55, X};
static const uint8_t PF_GLASS_ARP[] = {P_ARP_MODE, A_UP, P_ARP_DIV, D_16, P_ARP_OCT, 3, P_ARP_GATE, 35, P_O2_SEMI, 43, P_XMOD, 55,
    P_CUTOFF, 95, P_FENV_AMT, 90, P_FD, 50, P_FS, 0, P_AA, 1, P_AD, 70, P_AS, 0, P_AR, 70, P_SPREAD, 110,
    P_DLY_TIME, DD_8D, P_DLY_FB, 50, P_DLY_MIX, 45, P_REV_SIZE, 115, P_REV_DAMP, 40, P_REV_MIX, 50, X};
static const uint8_t PF_PULSE_ARP[] = {P_ARP_MODE, A_UPDN, P_ARP_DIV, D_16, P_ARP_OCT, 1, P_ARP_GATE, 60, P_O1_WAVE, W_PLS, P_O2_WAVE, W_PLS,
    P_O2_PW, 30, P_O2_FINE, 71, P_LFO_PWM, 50, P_LFO_RATE, 45, P_CUTOFF, 62, P_RESO, 25, P_FENV_AMT, 100, P_FD, 45, P_FS, 20,
    P_AA, 1, P_AR, 45, P_CHO_MIX, 35, P_DLY_TIME, DD_8D, P_DLY_FB, 40, P_DLY_MIX, 40, P_REV_MIX, 30, X};
static const uint8_t PF_ARP_BASS[] = {P_ARP_MODE, A_UP, P_ARP_DIV, D_16, P_ARP_OCT, 1, P_ARP_GATE, 55, P_MODE, M_MONO, P_O2_SEMI, 12,
    P_O2_LVL, 80, P_CUTOFF, 38, P_RESO, 40, P_FENV_AMT, 112, P_FD, 38, P_FS, 5, P_AA, 1, P_AD, 50, P_AS, 60, P_AR, 25, P_DRIVE, 35, X};
static const uint8_t PF_STAB_CHORD[] = {P_MODE, M_DUAL, P_UNI_DET, 45, P_CUTOFF, 58, P_RESO, 20, P_FENV_AMT, 112, P_FA, 5, P_FD, 50,
    P_FS, 20, P_AA, 2, P_AD, 55, P_AS, 30, P_AR, 40, P_DRIVE, 25, P_SPREAD, 100, P_REV_SIZE, 90, P_REV_MIX, 35, X};
static const uint8_t PF_BIG_BRASS[] = {P_O2_FINE, 73, P_CUTOFF, 52, P_RESO, 18, P_FENV_AMT, 108, P_FA, 65, P_FD, 85, P_FS, 55, P_FR, 70,
    P_AA, 50, P_AS, 110, P_AR, 65, P_VINTAGE, 60, P_SPREAD, 110, P_CHO_MIX, 40, P_REV_SIZE, 100, P_REV_MIX, 35, X};
static const uint8_t PF_HORN_SECT[] = {P_O2_FINE, 71, P_CUTOFF, 62, P_RESO, 12, P_FENV_AMT, 100, P_FA, 35, P_FD, 70, P_FS, 50,
    P_AA, 25, P_AR, 50, P_LFO_PITCH, 18, P_LFO_DELAY, 75, P_LFO_RATE, 72, P_VEL_FLT, 80, P_DRIVE, 15, P_REV_MIX, 25, X};
static const uint8_t PF_WARM_PAD[] = {P_O1_WAVE, W_PLS, P_O1_PW, 45, P_O2_FINE, 74, P_LFO_PWM, 40, P_LFO_RATE, 35, P_LFO_PHASE, LP_SPREAD,
    P_CUTOFF, 50, P_RESO, 10, P_FENV_AMT, 80, P_FA, 80, P_FD, 100, P_FS, 60, P_FR, 100, P_AA, 80, P_AR, 105,
    P_VINTAGE, 55, P_SPREAD, 120, P_CHO_MIX, 60, P_REV_SIZE, 110, P_REV_MIX, 50, X};
static const uint8_t PF_DARK_PAD[] = {P_O2_SEMI, 12, P_O2_FINE, 72, P_CUTOFF, 32, P_RESO, 30, P_FENV_AMT, 90, P_FA, 105, P_FD, 110,
    P_FS, 40, P_FR, 105, P_AA, 95, P_AR, 112, P_LFO_FLT, 12, P_LFO_RATE, 20, P_VINTAGE, 70, P_SPREAD, 120,
    P_REV_SIZE, 122, P_REV_DAMP, 85, P_REV_PRE, 40, P_REV_MIX, 60, P_DLY_TIME, DD_4D, P_DLY_FB, 45, P_DLY_TONE, 25, P_DLY_MIX, 25, X};
static const uint8_t PF_AIR_CHOIR[] = {P_O1_WAVE, W_PLS, P_O1_PW, 30, P_O2_WAVE, W_PLS, P_O2_PW, 95, P_O2_FINE, 75, P_FTYPE, F_SEM,
    P_FMORPH, 40, P_CUTOFF, 66, P_RESO, 30, P_LFO_PWM, 55, P_LFO_RATE, 30, P_LFO_PHASE, LP_SPREAD, P_NOISE, 12,
    P_AA, 90, P_AR, 105, P_FENV_AMT, 64, P_CHO_MIX, 75, P_CHO_DEPTH, 70, P_REV_SIZE, 118, P_REV_MIX, 55, X};
static const uint8_t PF_SLOW_SWEEP[] = {P_O2_FINE, 72, P_CUTOFF, 25, P_RESO, 70, P_FENV_AMT, 120, P_FA, 118, P_FD, 115, P_FS, 50,
    P_FR, 110, P_AA, 70, P_AR, 110, P_SPREAD, 110, P_CHO_MIX, 35, P_REV_SIZE, 115, P_REV_MIX, 50, X};
static const uint8_t PF_GHOST_PAD[] = {P_NOISE, 45, P_O2_FINE, 76, P_FTYPE, F_BP12, P_CUTOFF, 60, P_RESO, 60, P_LFO_SHAPE, L_RND,
    P_LFO_FLT, 45, P_LFO_RATE, 40, P_LFO_PHASE, LP_SPREAD, P_AA, 100, P_AR, 115, P_FENV_AMT, 64,
    P_REV_SIZE, 127, P_REV_DAMP, 50, P_REV_PRE, 60, P_REV_MIX, 70, P_DLY_TIME, DD_2, P_DLY_FB, 50, P_DLY_MIX, 30, X};
static const uint8_t PF_ENSEMBLE[] = {P_O2_FINE, 75, P_O2_LVL, 90, P_CUTOFF, 72, P_RESO, 5, P_FENV_AMT, 70, P_FA, 60, P_AA, 55, P_AR, 85,
    P_LFO_PITCH, 6, P_LFO_RATE, 60, P_LFO_PHASE, LP_SPREAD, P_VINTAGE, 50, P_SPREAD, 115, P_CHO_MIX, 80, P_CHO_RATE, 45,
    P_CHO_DEPTH, 65, P_REV_SIZE, 100, P_REV_MIX, 40, X};
static const uint8_t PF_ICE_BELLS[] = {P_O2_SEMI, 45, P_XMOD, 85, P_O1_LVL, 95, P_CUTOFF, 105, P_FENV_AMT, 96, P_FD, 95, P_FS, 0,
    P_AA, 1, P_AD, 105, P_AS, 0, P_AR, 95, P_SPREAD, 110, P_CHO_MIX, 30, P_DLY_TIME, DD_4, P_DLY_FB, 45, P_DLY_MIX, 35,
    P_REV_SIZE, 122, P_REV_DAMP, 35, P_REV_MIX, 60, X};
static const uint8_t PF_SOFT_EP[] = {P_O1_WAVE, W_PLS, P_O1_PW, 60, P_O2_WAVE, W_PLS, P_O2_PW, 25, P_O2_SEMI, 36, P_O2_LVL, 45,
    P_FTYPE, F_LP12, P_CUTOFF, 55, P_FENV_AMT, 100, P_FD, 75, P_FS, 25, P_AA, 2, P_AD, 95, P_AS, 40, P_AR, 60,
    P_VEL_FLT, 95, P_VEL_AMP, 100, P_KEYTRK, 90, P_CHO_MIX, 55, P_CHO_RATE, 50, P_REV_SIZE, 80, P_REV_MIX, 30, X};
static const uint8_t PF_PULSE_ORGN[] = {P_O1_WAVE, W_PLS, P_O1_PW, 64, P_O2_WAVE, W_PLS, P_O2_PW, 64, P_O2_SEMI, 36, P_O2_LVL, 80,
    P_FTYPE, F_LP12, P_CUTOFF, 85, P_RESO, 0, P_FENV_AMT, 64, P_AA, 2, P_AS, 127, P_AR, 20, P_LFO_PITCH, 5, P_LFO_RATE, 80,
    P_CHO_MIX, 60, P_CHO_RATE, 70, P_REV_MIX, 25, X};
static const uint8_t PF_TAPE_KEYS[] = {P_VINTAGE, 110, P_O2_WAVE, W_PLS, P_O2_PW, 40, P_O2_FINE, 72, P_CUTOFF, 58, P_FENV_AMT, 96,
    P_FD, 80, P_FS, 30, P_AA, 4, P_AD, 90, P_AS, 50, P_AR, 70, P_LFO_PITCH, 4, P_LFO_RATE, 25, P_VEL_FLT, 70,
    P_CHO_MIX, 40, P_DLY_TIME, DD_8D, P_DLY_FB, 45, P_DLY_TONE, 30, P_DLY_MIX, 35, P_REV_MIX, 30, X};
static const uint8_t PF_ACID_LINE[] = {P_MODE, M_MONO, P_O2_WAVE, W_OFF, P_CUTOFF, 30, P_RESO, 95, P_FENV_AMT, 118, P_FD, 45, P_FS, 0,
    P_AA, 1, P_AS, 110, P_AR, 20, P_GLIDE, 25, P_DRIVE, 55, P_VEL_FLT, 90, P_FTYPE, F_LP24, P_DLY_TIME, DD_8D, P_DLY_FB, 30, P_DLY_MIX, 20, X};
static const uint8_t PF_PLUCK_BASS[] = {P_O2_SEMI, 12, P_O2_WAVE, W_PLS, P_O2_PW, 45, P_CUTOFF, 34, P_RESO, 25, P_FENV_AMT, 115,
    P_FD, 40, P_FS, 0, P_AA, 1, P_AD, 60, P_AS, 0, P_AR, 25, P_DRIVE, 30, X};
static const uint8_t PF_DEEP_BASS[] = {P_MODE, M_MONO, P_O1_WAVE, W_BOTH, P_O2_SEMI, 12, P_O2_LVL, 110, P_O2_FINE, 66, P_CUTOFF, 30,
    P_RESO, 15, P_FENV_AMT, 96, P_FD, 70, P_FS, 30, P_AA, 1, P_AR, 25, P_DRIVE, 60, P_GLIDE, 8, X};
static const uint8_t PF_SYNC_BASS[] = {P_MODE, M_MONO, P_SYNC, 1, P_O2_SEMI, 33, P_ENV_O2, 96, P_O1_LVL, 80, P_CUTOFF, 45, P_RESO, 20,
    P_FENV_AMT, 108, P_FD, 45, P_FS, 20, P_AA, 1, P_AR, 25, P_DRIVE, 40, P_GLIDE, 10, X};
static const uint8_t PF_SOLO_LEAD[] = {P_MODE, M_MONO, P_O2_FINE, 70, P_CUTOFF, 80, P_RESO, 25, P_FENV_AMT, 92, P_FD, 70, P_FS, 50,
    P_GLIDE, 35, P_AA, 3, P_AR, 45, P_LFO_PITCH, 14, P_LFO_DELAY, 80, P_LFO_RATE, 72, P_DRIVE, 35,
    P_DLY_TIME, DD_8D, P_DLY_FB, 45, P_DLY_MIX, 35, P_REV_SIZE, 95, P_REV_MIX, 30, X};
static const uint8_t PF_PWM_LEAD[] = {P_MODE, M_MONO, P_O1_WAVE, W_PLS, P_O2_WAVE, W_PLS, P_O2_PW, 40, P_O2_FINE, 68, P_LFO_PWM, 65,
    P_LFO_RATE, 55, P_CUTOFF, 85, P_RESO, 15, P_GLIDE, 25, P_AA, 2, P_AR, 40, P_DLY_TIME, DD_8, P_DLY_FB, 40, P_DLY_MIX, 30, P_REV_MIX, 20, X};
static const uint8_t PF_SCREAM_LD[] = {P_MODE, M_UNISON, P_UNI_N, 4, P_UNI_DET, 35, P_CUTOFF, 75, P_RESO, 85, P_FENV_AMT, 100,
    P_FD, 80, P_FS, 60, P_DRIVE, 80, P_GLIDE, 20, P_AA, 2, P_AR, 40, P_LFO_PITCH, 12, P_LFO_DELAY, 70,
    P_DLY_TIME, DD_8D, P_DLY_FB, 45, P_DLY_MIX, 35, P_REV_MIX, 25, X};
static const uint8_t PF_SOFT_FLUTE[] = {P_MODE, M_MONO, P_O1_WAVE, W_PLS, P_O1_PW, 64, P_O2_WAVE, W_OFF, P_NOISE, 18, P_FTYPE, F_LP12,
    P_CUTOFF, 60, P_RESO, 10, P_FENV_AMT, 75, P_FA, 30, P_AA, 30, P_AR, 50, P_KEYTRK, 100, P_LFO_PITCH, 10, P_LFO_DELAY, 85,
    P_LFO_RATE, 70, P_GLIDE, 15, P_REV_SIZE, 100, P_REV_MIX, 40, X};
static const uint8_t PF_LASER[] = {P_MODE, M_MONO, P_SYNC, 1, P_O2_SEMI, 48, P_ENV_O2, 127, P_O1_LVL, 90, P_CUTOFF, 100, P_RESO, 40, P_VOLUME, 120,
    P_FENV_AMT, 64, P_FA, 0, P_FD, 55, P_FS, 0, P_AA, 1, P_AD, 70, P_AS, 0, P_AR, 40,
    P_DLY_TIME, DD_8D, P_DLY_FB, 60, P_DLY_MIX, 45, P_REV_MIX, 30, X};
static const uint8_t PF_WIND_RISE[] = {P_NOISE, 110, P_O1_WAVE, W_OFF, P_O2_WAVE, W_OFF, P_FTYPE, F_BP12, P_RESO, 75, P_CUTOFF, 30,
    P_FENV_AMT, 124, P_FA, 120, P_FD, 120, P_FS, 100, P_FR, 110, P_AA, 110, P_AR, 115, P_LFO_FLT, 15, P_LFO_RATE, 50,
    P_REV_SIZE, 125, P_REV_MIX, 65, P_DLY_TIME, DD_4, P_DLY_FB, 55, P_DLY_MIX, 30, X};
static const uint8_t PF_SPACE_DRONE[] = {P_MODE, M_UNISON, P_UNI_N, 8, P_UNI_DET, 95, P_O2_SEMI, 31, P_O2_LVL, 70, P_CUTOFF, 54,
    P_RESO, 45, P_LFO_SHAPE, L_SH, P_LFO_FLT, 35, P_LFO_RATE, 30, P_FENV_AMT, 64, P_AA, 90, P_AR, 120, P_VOLUME, 120, P_VINTAGE, 90,
    P_SPREAD, 127, P_DLY_TIME, DD_4D, P_DLY_FB, 70, P_DLY_TONE, 30, P_DLY_MIX, 40, P_REV_SIZE, 127, P_REV_MIX, 65, X};
static const uint8_t PF_SYNC_PAD[] = {P_SYNC, 1, P_O2_SEMI, 31, P_O1_LVL, 70, P_O2_LVL, 100, P_LFO_PITCH, 30, P_LFO_PITCH_TO, TO_O2,
    P_LFO_RATE, 25, P_LFO_PHASE, LP_SPREAD, P_CUTOFF, 60, P_RESO, 15, P_FENV_AMT, 80, P_FA, 70, P_AA, 70, P_AR, 100,
    P_SPREAD, 115, P_CHO_MIX, 50, P_REV_SIZE, 112, P_REV_MIX, 50, X};
static const uint8_t PF_DREAM_ARP[] = {P_ARP_MODE, A_PLAY, P_ARP_DIV, D_8, P_ARP_OCT, 2, P_ARP_GATE, 85, P_O1_WAVE, W_PLS, P_O1_PW, 40,
    P_O2_FINE, 74, P_LFO_PWM, 35, P_LFO_RATE, 40, P_CUTOFF, 60, P_RESO, 20, P_FENV_AMT, 90, P_FA, 20, P_FD, 80, P_FS, 40,
    P_AA, 15, P_AR, 90, P_SPREAD, 120, P_CHO_MIX, 55, P_DLY_TIME, DD_4D, P_DLY_FB, 60, P_DLY_TONE, 35, P_DLY_MIX, 50,
    P_REV_SIZE, 120, P_REV_MIX, 60, X};
static const uint8_t PF_INIT[] = {X};
/* 65 (VIBE): a kick – osc 2 an octave down, its pitch thrown 2 octaves down by the filter envelope (ENV>O2), the
 * 24 dB filter nearly shut, a short punch; play it low (C1-C2) */
static const uint8_t PF_KICK[] = {P_MODE, M_MONO, P_O1_WAVE, W_OFF, P_O2_WAVE, W_PLS, P_O2_PW, 64, P_O2_LVL, 127, P_O2_SEMI, 12,
    P_O2_FINE, 64, P_ENV_O2, 127, P_NOISE, 6, P_CUTOFF, 40, P_RESO, 8, P_FENV_AMT, 100, P_KEYTRK, 127, P_FTYPE, F_LP24,
    P_FA, 0, P_FD, 58, P_FS, 0, P_FR, 58, P_AA, 0, P_AD, 84, P_AS, 0, P_AR, 82, P_DRIVE, 45, P_VEL_FLT, 30, P_VEL_AMP, 50,
    P_VINTAGE, 0, P_SPREAD, 0, P_VOLUME, 127, X};
/* 66 (VIBE): a snare – noise through a bright low-pass, a body around 200 Hz (at C3) whose pitch drops, short */
static const uint8_t PF_SNARE[] = {P_O1_WAVE, W_OFF, P_O2_WAVE, W_PLS, P_O2_PW, 64, P_O2_LVL, 85, P_O2_SEMI, 31, P_ENV_O2, 100,
    P_NOISE, 115, P_FTYPE, F_LP12, P_CUTOFF, 96, P_RESO, 10, P_FENV_AMT, 92, P_KEYTRK, 64, P_FA, 0, P_FD, 50, P_FS, 0,
    P_AA, 0, P_AD, 75, P_AS, 0, P_AR, 72, P_DRIVE, 70, P_VEL_FLT, 50, P_VEL_AMP, 70, P_VINTAGE, 0, P_SPREAD, 0,
    P_VOLUME, 127, P_REV_SIZE, 60, P_REV_MIX, 18, X};
/* 67 (VIBE): a closed hi-hat – bright high-passed noise, very short (higher keys: brighter) */
static const uint8_t PF_HIHAT[] = {P_O1_WAVE, W_OFF, P_O2_WAVE, W_OFF, P_NOISE, 127, P_FTYPE, F_HP12, P_CUTOFF, 100, P_RESO, 25, P_DRIVE, 60,
    P_FENV_AMT, 64, P_KEYTRK, 40, P_AA, 0, P_AD, 58, P_AS, 0, P_AR, 56, P_VEL_AMP, 80, P_VINTAGE, 0, P_SPREAD, 0, P_VOLUME, 127, X};
/* 68 (VIBE): a clap – band-passed noise chopped by a fast square LFO (the flutter), restarted with every hit */
static const uint8_t PF_CLAP[] = {P_O1_WAVE, W_OFF, P_O2_WAVE, W_OFF, P_NOISE, 127, P_FTYPE, F_LP12, P_CUTOFF, 98, P_RESO, 25, P_DRIVE, 80,
    P_FENV_AMT, 64, P_KEYTRK, 30, P_AA, 0, P_AD, 78, P_AS, 0, P_AR, 76, P_LFO_SHAPE, L_SQR, P_LFO_RATE, 122,
    P_LFO_PHASE, LP_SYNC, P_LFO_AMP, 110, P_VEL_AMP, 70, P_VINTAGE, 0, P_SPREAD, 0, P_VOLUME, 127, P_REV_SIZE, 70, P_REV_MIX, 25, X};
/* 69 (VIBE): an electric piano in the Rhodes manner – a warm low-passed fundamental, a bright tine two octaves up
 * with a touch of cross-mod for the bark, played harder = brighter and louder; tremolo across the stereo, chorus */
static const uint8_t PF_EPIANO[] = {P_O1_WAVE, W_PLS, P_O1_PW, 64, P_O1_LVL, 120, P_O2_WAVE, W_SAW, P_O2_SEMI, 48, P_O2_LVL, 38,
    P_XMOD, 28, P_FTYPE, F_LP24, P_CUTOFF, 52, P_RESO, 5, P_FENV_AMT, 100, P_KEYTRK, 100, P_FA, 0, P_FD, 72, P_FS, 20,
    P_FR, 70, P_AA, 0, P_AD, 102, P_AS, 18, P_AR, 80, P_VEL_FLT, 110, P_VEL_AMP, 100, P_LFO_SHAPE, L_TRI, P_LFO_RATE, 90,
    P_LFO_PHASE, LP_SPREAD, P_LFO_AMP, 22, P_VINTAGE, 25, P_SPREAD, 90, P_CHO_MIX, 40, P_CHO_RATE, 30, P_CHO_DEPTH, 45,
    P_REV_SIZE, 85, P_REV_MIX, 22, X};
/* 70 (VIBE): a Wurlitzer-ish piano – a reedy narrow pulse with an octave above, a little drive for the growl when
 * played hard, a quicker tremolo */
static const uint8_t PF_WURLI[] = {P_O1_WAVE, W_PLS, P_O1_PW, 38, P_O2_WAVE, W_PLS, P_O2_PW, 50, P_O2_SEMI, 36, P_O2_LVL, 48,
    P_FTYPE, F_LP12, P_CUTOFF, 50, P_RESO, 15, P_FENV_AMT, 100, P_KEYTRK, 90, P_FA, 0, P_FD, 66, P_FS, 22, P_FR, 60,
    P_AA, 1, P_AD, 94, P_AS, 30, P_AR, 62, P_DRIVE, 35, P_VEL_FLT, 100, P_VEL_AMP, 90, P_LFO_SHAPE, L_TRI, P_LFO_RATE, 96,
    P_LFO_PHASE, LP_FREE, P_LFO_AMP, 30, P_VINTAGE, 30, P_SPREAD, 60, P_REV_SIZE, 75, P_REV_MIX, 20, X};

static const factory_t FACTORY[] = {
    {"DARK ARP", PF_DARK_ARP}, {"POLY BRASS", PF_POLY_BRASS}, {"SYNC LEAD", PF_SYNC_LEAD}, {"UNISON LD", PF_UNISON_LEAD},
    {"PWM STRING", PF_PWM_STRINGS}, {"BELL XMOD", PF_BELL_XMOD}, {"FAT BASS", PF_FAT_BASS}, {"ARP SEQ", PF_ARP_SEQ},
    {"SWEEP PAD", PF_SWEEP_PAD}, {"BP QUACK", PF_BP_QUACK}, {"DUAL SAW", PF_DUAL_SAW}, {"RESO ZAP", PF_RESO_ZAP},
    {"BRASS STAB", PF_BRASS_STAB}, {"NOISE WASH", PF_NOISE_WASH}, {"HOLLOW PAD", PF_HOLLOW_PAD}, {"SQUARE LD", PF_SQUARE_LEAD},
    {"SUB BASS", PF_SUB_BASS}, {"SOFT KEYS", PF_SOFT_KEYS}, {"SYNC BRASS", PF_SYNC_BRASS}, {"DRONE UNI", PF_DRONE_UNI},
    {"CLAV PLUCK", PF_CLAV_PLUCK}, {"WOBBLE BS", PF_WOBBLE_BASS}, {"ARP PLUCK", PF_ARP_PLUCK}, {"ARP CHORD", PF_ARP_CHORD},
    {"RANDOM ARP", PF_RANDOM_ARP}, {"UPSIDE ARP", PF_UPSIDE_ARP}, {"FIFTHS LD", PF_FIFTHS_LEAD}, {"VINTAGE PD", PF_VINTAGE_PAD},
    {"NEON PAD", PF_NEON_PAD}, {"XMOD GROWL", PF_XMOD_GROWL}, {"PERC HIT", PF_PERC_HIT}, {"INIT", PF_INIT},
    {"NIGHTDRIVE", PF_NIGHT_DRIVE}, {"MINOR FALL", PF_MINOR_FALL}, {"SHADOW ARP", PF_SHADOW_ARP}, {"GLASS ARP", PF_GLASS_ARP},
    {"PULSE ARP", PF_PULSE_ARP}, {"ARP BASS", PF_ARP_BASS}, {"DREAM ARP", PF_DREAM_ARP}, {"STAB CHORD", PF_STAB_CHORD},
    {"BIG BRASS", PF_BIG_BRASS}, {"HORN SECT", PF_HORN_SECT}, {"WARM PAD", PF_WARM_PAD}, {"DARK PAD", PF_DARK_PAD},
    {"AIR CHOIR", PF_AIR_CHOIR}, {"SLOW SWEEP", PF_SLOW_SWEEP}, {"GHOST PAD", PF_GHOST_PAD}, {"ENSEMBLE", PF_ENSEMBLE},
    {"SYNC PAD", PF_SYNC_PAD}, {"ICE BELLS", PF_ICE_BELLS}, {"SOFT EP", PF_SOFT_EP}, {"PULSE ORGN", PF_PULSE_ORGN},
    {"TAPE KEYS", PF_TAPE_KEYS}, {"ACID LINE", PF_ACID_LINE}, {"PLUCK BASS", PF_PLUCK_BASS}, {"DEEP BASS", PF_DEEP_BASS},
    {"SYNC BASS", PF_SYNC_BASS}, {"SOLO LEAD", PF_SOLO_LEAD}, {"PWM LEAD", PF_PWM_LEAD}, {"SCREAM LD", PF_SCREAM_LD},
    {"SOFT FLUTE", PF_SOFT_FLUTE}, {"LASER", PF_LASER}, {"WIND RISE", PF_WIND_RISE}, {"SPACEDRONE", PF_SPACE_DRONE},
};
#define NFACT_SYNTH (sizeof FACTORY / sizeof FACTORY[0])
static const uint8_t PS_INIT[] = {P_TYPE, T_SYNTH, X};
static const factory_t FACT_SYNTH_INIT = {"INIT OBV", PS_INIT};
static const uint8_t PD_INIT[] = {P_TYPE, T_DX, X};
static const factory_t FACT_DX_INIT = {"INIT DX", PD_INIT};
/* (0.9) STRINGS: slots 161-176 */
#define ST(a, r, c) P_TYPE, T_STR, P_AA, a, P_AS, 127, P_AD, 100, P_AR, r, P_CUTOFF, c, P_RESO, 0, P_FENV_AMT, 64
static const uint8_t PT_SOLINA[] = {ST(80, 92, 108), P_ST_16, 0, P_ST_8, 110, P_ST_4, 80, P_ST_ENS, 110, X};
static const uint8_t PT_FULL[] = {ST(84, 96, 104), P_ST_16, 90, P_ST_8, 110, P_ST_4, 90, P_ST_ENS, 115, X};
static const uint8_t PT_CELLO[] = {ST(76, 88, 88), P_ST_16, 115, P_ST_8, 70, P_ST_4, 0, P_ST_ENS, 90, X};
static const uint8_t PT_VIOLIN[] = {ST(78, 90, 116), P_ST_16, 0, P_ST_8, 60, P_ST_4, 115, P_ST_ENS, 105, P_LFO_PITCH, 6, P_LFO_RATE, 70, P_LFO_DELAY, 50, X};
static const uint8_t PT_DARK[] = {ST(90, 100, 72), P_ST_16, 80, P_ST_8, 110, P_ST_4, 30, P_ST_ENS, 120, X};
static const uint8_t PT_STAB[] = {ST(20, 74, 112), P_ST_16, 60, P_ST_8, 110, P_ST_4, 100, P_ST_ENS, 100, X};
static const uint8_t PT_SLOW[] = {ST(100, 106, 96), P_ST_16, 70, P_ST_8, 110, P_ST_4, 70, P_ST_ENS, 127, X};
static const uint8_t PT_SAWS[] = {ST(58, 80, 120), P_ST_16, 0, P_ST_8, 110, P_ST_4, 60, P_ST_ENS, 0, X};
static const uint8_t PT_INIT[] = {ST(78, 90, 108), X};
static const factory_t FACT_STR[] = {{"SOLINA", PT_SOLINA}, {"FULL ENS", PT_FULL}, {"CELLO", PT_CELLO}, {"VIOLIN", PT_VIOLIN},
                                     {"DARK STR", PT_DARK}, {"TRAP STAB", PT_STAB}, {"SLOW PAD", PT_SLOW}, {"DRY SAWS", PT_SAWS}};
static const factory_t FACT_STR_INIT = {"INIT STR", PT_INIT};
#define NFACT_STR (sizeof FACT_STR / sizeof FACT_STR[0])
/* (0.19/0.20) SAMPLER: slots 177-184 – its own sample (REC on the track, or LOAD AUDIO onto it): PLAY over the keys
 * (C3 = as recorded) or CHOP into slices (C3 = slice 1) */
#define SM(mode, n, q) P_TYPE, T_SMP, P_SM_SRC, 0, P_SM_CHOP, mode, P_SM_SLICES, (n) ? 14 : 6, P_SM_QUAL, q, P_SM_EN_C, 127, P_SM_EN_F, 127, P_SM_SENS, 64, P_SM_PITCH, 24, P_SM_PMODE, (mode) ? 0 : 1, P_AA, 0, P_AD, 100, P_AS, 127, P_AR, 30, P_CUTOFF, 127, P_RESO, 0, P_FENV_AMT, 64, P_MODE, M_POLY
static const uint8_t PM_PLAY[] = {SM(0, 1, 0), X};
static const uint8_t PM_HITS16[] = {SM(2, 1, 0), X};
static const uint8_t PM_SP12[] = {SM(2, 1, 1), X};
static const uint8_t PM_SPKEYS[] = {SM(0, 1, 1), P_AR, 60, X};
static const uint8_t PM_CHOP16[] = {SM(1, 1, 0), X};
static const uint8_t PM_LOFI[] = {SM(0, 1, 3), P_CUTOFF, 100, P_AR, 70, X};
static const uint8_t PM_CHOP8[] = {SM(1, 0, 2), X};
static const uint8_t PM_REV[] = {SM(2, 0, 1), P_SM_REV, 1, X};
static const uint8_t PM_SINIT[] = {SM(0, 1, 0), X};
static const factory_t FACT_SMP[] = {{"SAMPLE", PM_PLAY}, {"HITS 16", PM_HITS16}, {"SP CHOP", PM_SP12}, {"SP KEYS", PM_SPKEYS},
                                     {"CHOP 16", PM_CHOP16}, {"LOFI KEYS", PM_LOFI}, {"MPC CHOP 8", PM_CHOP8}, {"REVERSED", PM_REV}};
static const factory_t FACT_SMP_INIT = {"INIT SMP", PM_SINIT};
#define NFACT_SMP (sizeof FACT_SMP / sizeof FACT_SMP[0])
/* (0.30) MIDI: slots 185-192 – an external device on channel 1..8 */
#define MI(ch) P_TYPE, T_MIDI, P_MI_CH, ch, P_MI_PROG, 0, P_MI_VEL, 0, P_MI_CC1N, 74, P_MI_CC2N, 71, P_MI_CC3N, 73, P_MI_CC4N, 1, P_MI_CC1, 64, P_MI_CC2, 0, P_MI_CC3, 0, P_MI_CC4, 0, P_MODE, M_POLY
static const uint8_t PI_1[] = {MI(0), X}, PI_2[] = {MI(1), X}, PI_3[] = {MI(2), X}, PI_4[] = {MI(3), X},
                     PI_5[] = {MI(4), X}, PI_6[] = {MI(5), X}, PI_7[] = {MI(6), X}, PI_8[] = {MI(7), X};
static const factory_t FACT_MIDI[] = {{"MIDI CH 1", PI_1}, {"MIDI CH 2", PI_2}, {"MIDI CH 3", PI_3}, {"MIDI CH 4", PI_4},
                                      {"MIDI CH 5", PI_5}, {"MIDI CH 6", PI_6}, {"MIDI CH 7", PI_7}, {"MIDI CH 8", PI_8}};
#define NFACT_MIDI (sizeof FACT_MIDI / sizeof FACT_MIDI[0])
#undef MI
#undef SM
#undef ST
#undef X
#define NFACT_DRUMS (sizeof FACT_DRUMS / sizeof FACT_DRUMS[0])
#define NFACT_BASS (sizeof FACT_BASS / sizeof FACT_BASS[0])
#define NFACT_MEL (sizeof FACT_MEL / sizeof FACT_MEL[0])
static const factory_t *factory_of(uint32_t n)  /* slot n's factory sound */
{
    n %= NPROG;
    if (n < PROG_BASS0) return n < NFACT_DRUMS ? &FACT_DRUMS[n] : &FACT_DRUMS_INIT;
    if (n < PROG_MEL0) return n - PROG_BASS0 < NFACT_BASS ? &FACT_BASS[n - PROG_BASS0] : &FACT_BASS_INIT;
    if (n < PROG_SYNTH0) return n - PROG_MEL0 < NFACT_MEL ? &FACT_MEL[n - PROG_MEL0] : &FACT_MEL_INIT;
    if (n >= PROG_MIDI0) return &FACT_MIDI[(n - PROG_MIDI0) % NFACT_MIDI];
    if (n >= PROG_SMP0) return n - PROG_SMP0 < NFACT_SMP ? &FACT_SMP[n - PROG_SMP0] : &FACT_SMP_INIT;
    if (n >= PROG_STR0) return n - PROG_STR0 < NFACT_STR ? &FACT_STR[n - PROG_STR0] : &FACT_STR_INIT;
    if (n >= PROG_DX0) return &FACT_DX_INIT;    /* (its voice and name: prog_get, from bm_dxbank.h) */
    return n - PROG_SYNTH0 < NFACT_SYNTH ? &FACTORY[n - PROG_SYNTH0] : &FACT_SYNTH_INIT;
}
