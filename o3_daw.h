/* SPDX-License-Identifier: GPL-3.0-only */
/* The DAW's parameters (SDK 8, vibe_params) – every sound parameter, fixed to its track: an automation lane always moves
 * the same sound, whichever track the panel shows. Both ways: the DAW (or the plugin's Editor) sets a value
 * (VIBE_EV_PARAM), and what the panel, a MIDI CC or a program change does to a sound goes back (daw_sync, once a block).
 * Order (a DAW knows a parameter by its number): first the 32 of 26.10. (8 a track: Cutoff, Resonance, Env Amount,
 * Attack, Release, LFO Rate, LFO > Filter, Volume – projects automated since keep working), then every other voice
 * parameter of tracks A–D, then the effects (chorus, delay, reverb: the patch of track A holds them). Groups for the
 * Editor view: "A · FILTER", "FX · DELAY" … (vibe_param_groups). 27.10.2026. */
#define DAW_PER 8
static const uint8_t DAW_P[DAW_PER] = {P_CUTOFF, P_RESO, P_FENV_AMT, P_AA, P_AR, P_LFO_RATE, P_LFO_FLT, P_VOLUME};
static const char *const DAW_NAME[DAW_PER] = {"Cutoff", "Resonance", "Env Amount", "Attack", "Release", "LFO Rate",
                                              "LFO > Filter", "Volume"};
#define DAW_MAX 256
/* (BOOM) 256 parameters at most (VIBE_MAX_PARAMS): each track lists the common ones and those of its factory kind –
 * A DRUMS, B BASS, C/D MEL. A track turned into another kind keeps its common ones in the DAW; the rest is the panel's. */
static const uint8_t DAW_KIND[NTRK] = {T_DRUMS, T_BASS, T_MEL, T_SYNTH, T_DRUMS, T_SYNTH, T_STR, T_DX};
static int grp_kind(uint32_t g) { return g == G_BASS ? (int)T_BASS : g >= G_ALGO && g <= G_OP4 ? (int)T_MEL : g >= G_KICK && g <= G_LEVELS ? (int)T_DRUMS : g >= G_OSC1 && g <= G_FILTER2 ? (int)T_SYNTH : g == G_DX ? (int)T_DX : g == G_STR ? (int)T_STR : g == G_SMP ? (int)T_SMP : g == G_MIDI ? (int)T_MIDI : -1; }
static vibe_param daw_list[DAW_MAX];
static const char *daw_grp[DAW_MAX];
static uint8_t daw_t[DAW_MAX], daw_pi[DAW_MAX];          /* each one's track and parameter */
static char daw_id[DAW_MAX][24], daw_nm[DAW_MAX][28], daw_gn[NTRK + 1][NGRP][20];
static int DAW_N;

static int daw_bip(uint32_t p) { return PDEF[p].kind == K_BIP; }            /* (shown -64..63 around the middle) */
static float daw_off(uint32_t p) { return daw_bip(p) ? 64.0f : PDEF[p].kind == K_SEMI ? 24.0f : 0.0f; }
static float daw_of(uint32_t t, uint32_t p) { return (float)live_p[t].v[p] - daw_off(p); }

/* the page a parameter lives on (its group); the knob of CUTOFF is on PERFORM, its home is FILTER */
static int daw_page(uint32_t p) { return PDEF[p].grp; }   /* (BOOM: its group, bm_params.h) */
static const char *const *daw_choices(uint32_t p)
{
    static const char *const ONOFF[2] = {"OFF", "ON"};
    switch (PDEF[p].kind) {
    case K_MODE: return MODE_NAME;   case K_TYPE: return TYPE_NAME;   case K_DTYPE: return BDT_NAME;
    case K_DMODE: return DM_NAME;    case K_RMODE: return RM_NAME;
    case K_WAVE: return WAVE_NAME;   case K_FTYPE: return FTYPE_NAME; case K_TO: return TO_NAME;
    case K_BGATE: return BGATE_NAME; case K_SLIDE: return BSLIDE_NAME; case K_KSRC: return KSRC_NAME;
    case K_SMTRK: return SMTRK_NAME; case K_SMCHOP: return SMCHOP_NAME; case K_SMQ: return SMQ_NAME; case K_SMPM: return SMPM_NAME;
    case K_LSHAPE: return LSHAPE_NAME; case K_LPHASE: return LPHASE_NAME;
    case K_ARP: return ARP_NAME;     case K_DIV: return DIV_NAME;     case K_DDIV: return DDIV_NAME;
    case K_ONOFF: return ONOFF;
    default: return 0;
    }
}
static void scopy(char *d, const char *s, int n) { int i = 0; while (s[i] && i < n - 1) { d[i] = s[i]; i++; } d[i] = 0; }
static void daw_add(uint32_t t, uint32_t p, const char *legacy)
{
    int j = DAW_N, g = daw_page(p), fx = is_a_only(p);
    const char *const *ch = daw_choices(p);
    char *s = daw_id[j], *n = daw_nm[j];
    const char *pn = legacy ? legacy : PDEF[p].name, *gt = GRP_NAME[g];
    if (!legacy && pn[0] == gt[0] && pn[1] == gt[1] && pn[2] == gt[2] && !pn[3]) pn = "MODE";   /* (ARP > ARP: "ARP MODE") */
    int i, k = 0;
    if (j >= DAW_MAX) return;
    /* id: "a_cutoff" for the 32 of 26.10. (as they were), else "a_filter_type", "fx_delay_mix" – stays when names change */
    if (fx) { s[k++] = 'f'; s[k++] = 'x'; } else s[k++] = (char)('a' + t);
    s[k++] = '_';
    if (!legacy) {
        for (i = 0; gt[i] && k < 14; i++) if ((gt[i] >= 'A' && gt[i] <= 'Z') || (gt[i] >= '0' && gt[i] <= '9')) s[k++] = (char)(gt[i] | 0x20);
        s[k++] = '_';
    }
    for (i = 0; pn[i] && k < 23; i++)
        if ((pn[i] >= 'A' && pn[i] <= 'Z') || (pn[i] >= 'a' && pn[i] <= 'z') || (pn[i] >= '0' && pn[i] <= '9')) s[k++] = (char)(pn[i] | 0x20);
    s[k] = 0;
    /* name: "A Cutoff" (as it was) or "A Filter Type", "FX Delay Mix" */
    k = 0;
    if (fx) { n[k++] = 'F'; n[k++] = 'X'; } else n[k++] = (char)('A' + t);
    n[k++] = ' ';
    if (!legacy) { for (i = 0; gt[i] && k < 14; i++) n[k++] = gt[i]; n[k++] = ' '; }
    for (i = 0; pn[i] && k < 27; i++) n[k++] = pn[i];
    n[k] = 0;
    /* group: "A · FILTER", "FX · DELAY" */
    {
        char *gn = daw_gn[fx ? NTRK : t][g];
        if (!gn[0]) {
            k = 0;
            if (fx) { gn[k++] = 'F'; gn[k++] = 'X'; } else gn[k++] = (char)('A' + t);
            scopy(gn + k, " \xc2\xb7 ", 5); k += 4;
            scopy(gn + k, gt, 20 - k);
        }
        daw_grp[j] = gn;
    }
    daw_t[j] = (uint8_t)(fx ? 0 : t); daw_pi[j] = (uint8_t)p;
    daw_list[j].id = daw_id[j]; daw_list[j].name = daw_nm[j];
    daw_list[j].min = -daw_off(p);
    daw_list[j].max = (float)PDEF[p].max - daw_off(p);
    daw_list[j].def = daw_of(daw_t[j], p);                   /* (the default: the sound as it loaded – its program's) */
    daw_list[j].unit = PDEF[p].kind == K_SEMI ? "st" : "";
    daw_list[j].steps = PDEF[p].max + 1;                     /* (whole numbers, as on the panel) */
    daw_list[j].choices = ch;
    DAW_N++;
}

/* SDK 11 (Protokoll 7.2a): the MIXER as parameters with the fixed ids – mix_1_level … mix_4_solo, mix_select – after all
 * the others (their numbers stay): a mixer controller (Mackie Control, the SMC-Mixer) or the DAW moves the tracks' levels,
 * pans, mutes and solos and picks the track; what the panel does goes back (daw_sync). 28.10.2026. */
#define MIX_W 4
static int MIX_BASE = -1;
static char mix_id[NTRK * MIX_W + 1][16], mix_nm[NTRK * MIX_W + 1][16];
static void mix_params(void)
{
    static const char *const W[MIX_W] = {"level", "pan", "mute", "solo"}, *const WN[MIX_W] = {"Level", "Pan", "Mute", "Solo"};
    static const char *const G[MIX_W] = {"MIX \xc2\xb7 LEVEL", "MIX \xc2\xb7 PAN", "MIX \xc2\xb7 MUTE", "MIX \xc2\xb7 SOLO"};
    static const char *const ONOFF[2] = {"OFF", "ON"}, *const TRK[NTRK] = {"A", "B", "C", "D", "E", "F", "G", "H"};
    MIX_BASE = DAW_N;
    for (uint32_t t = 0; t < NTRK; t++)
        for (uint32_t w = 0; w < MIX_W; w++) {
            int j = DAW_N, k = 0;
            char *s = mix_id[t * MIX_W + w], *n = mix_nm[t * MIX_W + w];
            if (j >= DAW_MAX) return;
            scopy(s, "mix_", 16); s[4] = (char)('1' + t); s[5] = '_'; scopy(s + 6, W[w], 10);
            n[k++] = (char)('A' + t); n[k++] = ' '; scopy(n + k, WN[w], 14);
            daw_list[j].id = s; daw_list[j].name = n; daw_list[j].unit = "";
            daw_list[j].min = w == 1u ? -1.0f : 0.0f; daw_list[j].max = 1.0f;
            daw_list[j].steps = w >= 2u ? 2 : 0; daw_list[j].choices = w >= 2u ? ONOFF : 0;
            daw_list[j].def = w == 0u ? 100.0f / 127.0f : 0.0f;
            daw_grp[j] = G[w]; DAW_N++;
        }
    if (DAW_N >= DAW_MAX) return;
    daw_list[DAW_N] = (vibe_param){"mix_select", "Track", 1.0f, (float)NTRK, 1.0f, "", NTRK, TRK};
    daw_grp[DAW_N] = "MIX \xc2\xb7 TRACK"; DAW_N++;
}
static float mix_of(int k)                      /* (k: 0 .. NTRK * MIX_W, the last = the track) */
{
    uint32_t t = (uint32_t)k / MIX_W, w = (uint32_t)k % MIX_W;
    if (k == NTRK * MIX_W) return (float)(sel + 1u);
    if (w == 0u) return set.mix_lvl[t] / 127.0f;
    if (w == 1u) return set.mix_pan[t] / 64.0f;
    return (float)(((w == 2u ? set.mix_mute : set.mix_solo) >> t) & 1u);
}
static void mix_from_host(int k, float v)
{
    uint32_t t = (uint32_t)k / MIX_W, w = (uint32_t)k % MIX_W;
    if (k == NTRK * MIX_W) { int32_t n = clampi((int32_t)(v + 0.5f), 1, NTRK) - 1; trk_select((uint32_t)n); return; }
    if (w == 0u) set.mix_lvl[t] = (uint8_t)clampi((int32_t)(v * 127.0f + 0.5f), 0, 127);
    else if (w == 1u) set.mix_pan[t] = (int8_t)clampi((int32_t)(v * 64.0f + (v < 0 ? -0.5f : 0.5f)), -64, 64);
    else { uint8_t *m = w == 2u ? &set.mix_mute : &set.mix_solo; if (v >= 0.5f) *m |= (uint8_t)(1u << t); else *m &= (uint8_t)~(1u << t); }
    mix_apply(); set_touch();
}
/* (BOOM 0.4) what an arrangement drives: MASTER FX (FILTER, CRUSH, DUB, REPEAT), the PATTERN (it changes on the bar),
 * each track's PLAYS (SEQ / DAW) – after the mixer's, with ids of their own */
#define XP_N (NMFX + 1 + NTRK)
static int XP_BASE = -1;
static char xp_id[NTRK][16], xp_nm[NTRK][16];
static void xp_params(void)
{
    static const char *const REP[6] = {"OFF", "1/4", "1/8", "1/16", "1/32", "1/64"}, *const PL[2] = {"SEQ", "DAW"};
    static const vibe_param FX[NMFX + 1] = {
        {"fx_master_filter", "Master Filter", -64.0f, 64.0f, 0.0f, "", 129, 0}, {"fx_master_crush", "Master Crush", 0.0f, 127.0f, 0.0f, "", 128, 0},
        {"fx_master_dub", "Master Dub", 0.0f, 127.0f, 0.0f, "", 128, 0}, {"fx_master_repeat", "Master Repeat", 0.0f, 5.0f, 0.0f, "", 6, REP},
        {"seq_pattern", "Pattern", 1.0f, (float)NPAT, 1.0f, "", NPAT, 0}};
    XP_BASE = DAW_N;
    for (int k = 0; k < XP_N && DAW_N < DAW_MAX; k++) {
        if (k <= (int)NMFX) { daw_list[DAW_N] = FX[k]; daw_grp[DAW_N] = k < (int)NMFX ? "MIX \xc2\xb7 MASTER FX" : "MIX \xc2\xb7 PATTERN"; }
        else {
            uint32_t t = (uint32_t)k - NMFX - 1u; char *id = xp_id[t], *nm = xp_nm[t];
            scopy(id, "trk_", 16); id[4] = (char)('1' + t); scopy(id + 5, "_plays", 10);
            nm[0] = (char)('A' + t); scopy(nm + 1, " Plays", 14);
            daw_list[DAW_N] = (vibe_param){id, nm, 0.0f, 1.0f, 0.0f, "", 2, PL}; daw_grp[DAW_N] = "MIX \xc2\xb7 PLAYS";
        }
        DAW_N++;
    }
}
static float xp_of(int k)
{
    if (k == 0) return (float)set.mfx[0] - 64.0f;
    if (k < (int)NMFX) return (float)set.mfx[k];
    if (k == (int)NMFX) return (float)((pat_queue ? pat_queue - 1u : set.pat) + 1u);
    return (float)(set.trk_daw >> (k - (int)NMFX - 1) & 1u);
}
static void xp_from_host(int k, float v)
{
    if (k == 0) set.mfx[0] = (uint8_t)clampi((int32_t)(v + 64.5f), 0, 128);
    else if (k < (int)NMFX) set.mfx[k] = (uint8_t)clampi((int32_t)(v + 0.5f), 0, k == 3 ? 5 : 127);
    else if (k == (int)NMFX) { uint32_t n = (uint32_t)clampi((int32_t)(v + 0.5f), 1, NPAT) - 1u; if (n != (pat_queue ? pat_queue - 1u : set.pat)) pat_go(n); return; }
    else { uint32_t t = (uint32_t)(k - (int)NMFX - 1); set.trk_daw = (uint8_t)(v >= 0.5f ? set.trk_daw | 1u << t : set.trk_daw & ~(1u << t)); }
    mix_apply(); set_touch();
}
static void daw_init(void)                      /* (after the tracks' sounds are loaded) */
{
    uint32_t t, k, p;
    DAW_N = 0;
    for (t = 0; t < NTRK; t++)                   /* the 32 of 26.10., in their places */
        for (k = 0; k < DAW_PER; k++) daw_add(t, DAW_P[k], DAW_NAME[k]);
    for (t = 0; t < NTRK; t++)                   /* every other voice parameter */
        for (p = 0; p < NPARAM; p++) {
            int legacy = 0, gk = grp_kind(PDEF[p].grp);
            if (is_a_only(p) || p == P_TYPE || PDEF[p].grp == G_ARP || (gk >= 0 && gk != DAW_KIND[t]) || t >= 4u) continue;   /* (E, F: their 8 only; no ARP – 256 at most) */
            for (k = 0; k < DAW_PER; k++) if (DAW_P[k] == p) legacy = 1;
            if (!legacy) daw_add(t, p, 0);
        }
    for (p = 0; p < NPARAM; p++) if (is_a_only(p)) daw_add(0, p, 0);   /* the effects, once */
    mix_params();                                /* (SDK 11: the mixer, after everything else) */
    xp_params();                                 /* (BOOM 0.4: MASTER FX, the pattern, PLAYS) */
    for (int a = 0; a < DAW_N; a++) for (int b = a + 1; b < DAW_N; b++) {   /* (BOOM 0.4) an id twice would make a DAW restore the wrong value: say so */
        const char *x = daw_list[a].id, *y = daw_list[b].id; int q = 0; while (x[q] && x[q] == y[q]) q++;
        if (!x[q] && !y[q]) { vibe_log("BOOM: DAW PARAMETER ID TWICE:"); vibe_log(x); }
    }
    vibe_params(daw_list, DAW_N);
    vibe_param_groups(daw_grp);
}

/* ---- (SDK 9): the Editor as BOOM wants it – the cards in the panel's order, Cutoff x Resonance as a pad,
 * bipolar knobs where the panel shows -/+, the envelopes and the LFO drawn, a word of help where a name is not enough */
static const char *const ED_TAB[NTRK + 1] = {"A", "B", "C", "D", "E", "F", "FX"};
static vibe_card ed_cards[NGRP];
static int ed_track(const char *tab) { return tab && tab[0] >= 'A' && tab[0] < 'A' + NTRK && !tab[1] ? tab[0] - 'A' : (int)sel; }
static int ed_env_curve(const char *tab, const char *card, float *pts, int max)   /* ADSR as the panel's knobs stand */
{
    const uint8_t *v = live_p[ed_track(tab)].v;
    int f = card[0] == 'F';
    float a = v[f ? P_FA : P_AA] / 127.0f * 0.3f + 0.02f, d = v[f ? P_FD : P_AD] / 127.0f * 0.3f + 0.02f, s = v[f ? P_FS : P_AS] / 127.0f, r = v[f ? P_FR : P_AR] / 127.0f * 0.3f + 0.02f;
    float h = 0.18f, tot = a + d + h + r;
    int n = max < 48 ? max : 48;
    for (int i = 0; i < n; i++) {
        float x = (float)i / (float)(n - 1) * tot, y;
        if (x < a) y = x / a;
        else if (x < a + d) y = 1.0f - (1.0f - s) * (x - a) / d;
        else if (x < a + d + h) y = s;
        else y = s * (1.0f - (x - a - d - h) / r);
        pts[i] = y < 0.0f ? 0.0f : y;
    }
    return n;
}
static int ed_lfo_curve(const char *tab, const char *card, float *pts, int max)   /* one cycle of the shape */
{
    static const uint8_t SH[8] = {90, 20, 70, 120, 40, 100, 10, 60};
    uint32_t shape = live_p[ed_track(tab)].v[P_LFO_SHAPE];
    int n = max < 32 ? max : 32;
    (void)card;
    for (int i = 0; i < n; i++) {
        float x = (float)i / (float)n, y;
        switch (shape) {
        case L_SAW: y = 1.0f - x; break;
        case L_RAMP: y = x; break;
        case L_SQR: y = x < 0.5f ? 1.0f : 0.0f; break;
        case L_SH: y = SH[(i * 8) / n] / 127.0f; break;
        case L_RND: { int k = (i * 8) / n; float t = (x * 8.0f) - (float)k; y = (SH[k] + (SH[(k + 1) & 7] - SH[k]) * t) / 127.0f; break; }
        default: y = x < 0.5f ? x * 2.0f : 2.0f - x * 2.0f; break;   /* L_TRI */
        }
        pts[i] = y;
    }
    return n;
}
static void daw_editor(void)
{
    static const uint8_t ORDER[] = {G_VOICE, G_BASS, G_ALGO, G_OP1, G_OP2, G_OP3, G_OP4, G_KICK, G_SNARE, G_CLAP, G_HATS, G_RIMSNAP, G_PERC, G_LEVELS,
                                    G_OSC1, G_OSC2, G_OSCMOD, G_FILTER, G_FILTER2, G_FENV, G_AENV, G_LFO, G_OUTPUT, G_CHORUS, G_DELAY, G_REVERB};
    int j, n = 0;
    for (uint32_t k = 0; k < sizeof ORDER; k++) {
        uint32_t g = ORDER[k];
        ed_cards[n].card = GRP_NAME[g];
        ed_cards[n].width = g == G_FILTER ? 6 : g == G_CHORUS || g == G_ALGO ? 3 : g == G_LEVELS ? 8 : g == G_BASS ? 9 : 4;
        ed_cards[n].note = g == G_FILTER ? "the pad: cutoff across, resonance up" : g == G_FENV || g == G_AENV ? "the curve follows the knobs" : 0;
        n++;
    }
    vibe_editor_cards(ed_cards, n);
    for (j = 0; j < (MIX_BASE >= 0 ? MIX_BASE : DAW_N); j++) {   /* (the sounds' parameters only) */
        uint32_t p = daw_pi[j], kind = PDEF[p].kind;
        if (kind == K_BIP || kind == K_SEMI) vibe_editor_control(j, VIBE_CTL_BIPOLAR);
        else if (kind == K_ONOFF) vibe_editor_control(j, VIBE_CTL_TOGGLE);
        if (p == P_CUTOFF && j + 1 < DAW_N && daw_pi[j + 1] == P_RESO && daw_t[j + 1] == daw_t[j]) { vibe_editor_control(j, VIBE_CTL_XY_X); vibe_editor_control(j + 1, VIBE_CTL_XY_Y); }
        switch (p) {                            /* a word where the name does not say it */
        case P_TYPE: vibe_param_help(j, "What the track is: a drum kit, the 808 bass or an FM sound"); break;
        case P_B_PUNCH: vibe_param_help(j, "The 808 starts this many semitones up and falls in PUNCH T"); break;
        case P_B_SLIDE: vibe_param_help(j, "LEGATO: slides only between overlapping notes; ALWAYS: every note slides"); break;
        case P_B_GATE: vibe_param_help(j, "DECAY: the 808 rings as long as DECAY says; GATE: as long as the key is held"); break;
        case P_FB: vibe_param_help(j, "Operator 4 modulates itself: brighter, harsher"); break;
        case P_ALGO: vibe_param_help(j, "How the four operators feed each other (1 = a chain 4>3>2>1, 8 = all four heard)"); break;
        case P_LFO_DELAY: vibe_param_help(j, "The LFO fades in this long after a key"); break;
        case P_GLIDE: vibe_param_help(j, "The time a note slides to the next (BASS, MONO)"); break;
        case P_VINTAGE: vibe_param_help(j, "SYNTH: how much the voices drift and differ, like an old polysynth's cards"); break;
        case P_XMOD: vibe_param_help(j, "SYNTH: oscillator 2 modulates oscillator 1's frequency"); break;
        default: break;
        }
    }
    if (XP_BASE >= 0 && XP_BASE < DAW_N) { vibe_editor_control(XP_BASE, VIBE_CTL_BIPOLAR); for (j = XP_BASE + (int)NMFX + 1; j < DAW_N; j++) vibe_editor_control(j, VIBE_CTL_TOGGLE); }
    if (MIX_BASE >= 0) for (j = MIX_BASE; j < (XP_BASE >= 0 ? XP_BASE : DAW_N); j++) {   /* (SDK 11: the mixer – pans from the middle, mutes and solos as switches) */
        int k = j - MIX_BASE;
        if (k < NTRK * MIX_W && k % MIX_W == 1) vibe_editor_control(j, VIBE_CTL_BIPOLAR);
        else if (k < NTRK * MIX_W && k % MIX_W >= 2) vibe_editor_control(j, VIBE_CTL_TOGGLE);
    }
    vibe_editor_accent(0xFF2E97);               /* (ACCENT, o3_draw.h) */
    for (uint32_t t = 0; t < NTRK; t++) vibe_editor_tab_color(ED_TAB[t], TRK_RGB[t]);
    vibe_editor_tab_color(ED_TAB[NTRK], 0x00E5FF);   /* (the FX key's blue) */
    vibe_editor_curve(GRP_NAME[G_FENV], ed_env_curve);
    vibe_editor_curve(GRP_NAME[G_AENV], ed_env_curve);
    vibe_editor_curve(GRP_NAME[G_LFO], ed_lfo_curve);
}

/* the DAW (or the Editor) moved one */
static void daw_from_host(uint32_t j, float v)
{
    uint32_t t, p;
    int32_t x;
    if (j >= (uint32_t)DAW_N) return;
    if (XP_BASE >= 0 && j >= (uint32_t)XP_BASE) { xp_from_host((int)j - XP_BASE, v); return; }
    if (MIX_BASE >= 0 && j >= (uint32_t)MIX_BASE) { mix_from_host((int)j - MIX_BASE, v); return; }
    t = daw_t[j]; p = daw_pi[j];
    x = clampi((int32_t)(v + daw_off(p) + 0.5f), 0, PDEF[p].max);   /* (0..max here: rounds up from .5) */
    if (live_p[t].v[p] == x) return;
    live_p[t].v[p] = (uint8_t)x;
    trk_edited[t] = 1;
}

/* a sound changed here: tell the DAW (only what differs, so the DAW's own moves do not come back) */
static void daw_sync(void)
{
    int j;
    for (j = 0; j < DAW_N; j++) {
        float v = XP_BASE >= 0 && j >= XP_BASE ? xp_of(j - XP_BASE) : MIX_BASE >= 0 && j >= MIX_BASE ? mix_of(j - MIX_BASE) : daw_of(daw_t[j], daw_pi[j]);
        if (vibe_param_get(j) != v) vibe_param_set(j, v);
    }
}
