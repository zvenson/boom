/* SPDX-License-Identifier: GPL-3.0-only */
/* BOOM – the pages and what the four encoders do on them (OberVave 3's frame). SOUND's pages and the two ENV pages are
 * the track's kind's (bm_params.h SPAGE: DRUMS, BASS, MEL); each colour key steps through its group:
 *   SOUND DRUMS: PERFORM → KICK → SNARE → CLAP → HATS → RIM/SNAP → PERC → KIT → TYPE   ENV LEVELS 1-4 → LEVELS 5-8
 *         BASS:  PERFORM → BODY → PUNCH → GLIDE → DRIVE → FILTER → TYPE                 ENV FILTER ENV → AMP ENV
 *         MEL:   PERFORM → ALGO → OP 1 → OP 2 → OP 3 → OP 4 → FILTER → VOICE → TYPE      ENV FILTER ENV → AMP ENV
 *                (0.41: only an older project's MEL track – TYPE offers it no more, its sounds are DX's bank MEL)
 *         DX:    PERFORM → TONE → FILTER → VOICE → OP → OP ENV → TYPE                     ENV FILTER ENV → DX ENV
 *         WAVETABLE: PERFORM → WAVE → POS ENV → OSC 2 → FILTER → VOICE → TYPE             ENV FILTER ENV → AMP ENV
 *   LFO   LFO → LFO DEST     ARP   ARP (on it again: the track's HOLD)     FX   CHORUS → DELAY → REVERB → DRIVE
 *   F6    GLOBAL → OUTPUT → SCALE                    ↩  PERFORM */
static uint32_t page = PG_SEQ, hold;   /* (BOOM, Sven 14.11.: it starts in the SEQUENCER, TRACKS) */        /* hold: bit t = track t's HOLD (the arp keeps playing) */
typedef struct { uint8_t first, count, key; } group_t;   /* key: the colour key 0..5 lit for it, 6 = F6, 7 = none */
static const group_t GROUPS[] = {
    {PG_PERFORM, 8, CK_SOUND}, {PG_FENV, 2, CK_ENV}, {PG_LFO, 2, CK_LFO}, {PG_ARP, 1, CK_ARP}, {PG_CHORUS, 4, CK_FX},
    {PG_GLOBAL, 1, 6}, {PG_OUTPUT, 1, 6}, {PG_SCALE, 1, 6},
};
static const group_t *page_group(uint32_t pg)
{
    for (uint32_t i = 0; i < sizeof GROUPS / sizeof GROUPS[0]; i++)
        if (pg >= GROUPS[i].first && pg < (uint32_t)GROUPS[i].first + GROUPS[i].count) return &GROUPS[i];
    static const group_t NONE = {0, 1, 7};      /* (MIXER, SEQ: no key lit, no chips) */
    return &NONE;
}
static uint32_t sm_turn_t, sm_turn_end; static void sm_cut_move(uint32_t p, int e, int fine); static void sm_auto(uint32_t p);   /* (bm_sampler.h) */
static int sm_region(uint32_t p, uint32_t *a, uint32_t *b, uint32_t *hi); static uint32_t sm_pos14(uint32_t p, int end); static void fmt_secs(char *s, uint32_t frames);
static uint32_t mi_ccn(uint32_t v) { return v > 119u ? 119u : v; }   /* (120..127 are channel modes: never sent as a knob) */
static const char *mi_label(uint32_t p, const uint8_t *pv)   /* (0.30) a MIDI track's CC knob: named by its CC number */
{
    static char b[4][12]; static uint32_t r;
    char *s;
    s = b[r++ & 3u]; s[0] = 0;
    if (p >= P_MI_CC1N && p <= P_MI_CC4N) { scat(s, "KNOB "); fmt_u(s + 5, p - P_MI_CC1N + 1u); scat(s, " CC"); return s; }   /* (CC NUMBERS) */
    if (p < P_MI_CC1 || p > P_MI_CC4) return 0;
    scat(s, "CC "); fmt_u(s + 3, mi_ccn(pv[P_MI_CC1N + (p - P_MI_CC1)]));
    return s;
}
static const char *ep_label(uint32_t p, const uint8_t *pv)   /* (0.31) an E-PIANO's knobs on the drums' bytes */
{
    const int reed = pv[P_EP_MODEL] & 1u;
    switch (p) {
    case P_EP_MODEL: return "MODEL"; case P_EP_TONE: return "TONE"; case P_EP_BARK: return "BARK"; case P_EP_BELL: return reed ? "BITE" : "BELL";
    case P_EP_DECAY: return "DECAY"; case P_EP_TREM: return "TREMOLO"; case P_EP_RATE: return "RATE"; case P_EP_DRIVE: return "PREAMP";
    case P_EP_NOISE: return "HAMMER"; case P_EP_VELO: return "TOUCH"; case P_EP_STEREO: return "STEREO"; case P_AR: return "DAMPER";
    default: return 0;
    }
}
static const char *wt_label(uint32_t p)         /* (0.41) a WAVETABLE's knobs on MEL's bytes */
{
    switch (p) {
    case P_WT_TABLE: return "TABLE"; case P_WT_POS: return "POSITION"; case P_WT_PENV: return "ENV>POS"; case P_WT_PATT: return "ATTACK";
    case P_WT_PDEC: return "DECAY"; case P_WT_LFO: return "LFO>POS"; case P_WT_O2: return "OSC 2"; case P_WT_SEMI: return "INTERVAL";
    case P_WT_DET: return "DETUNE"; case P_WT_SUB: return "SUB";
    default: return 0;
    }
}
static const char *gp_label(uint32_t p)         /* (0.41) PIANO's knobs on the drums' bytes */
{
    switch (p) {
    case P_GP_TONE: return "TONE"; case P_GP_HAMMER: return "HAMMER"; case P_GP_DECAY: return "DECAY"; case P_GP_VELO: return "TOUCH";
    case P_GP_SPREAD: return "SPREAD"; case P_AR: return "DAMPER";
    default: return 0;
    }
}
static const char *cv_label(uint32_t p)         /* (0.41) CLAV's knobs on the drums' bytes */
{
    switch (p) {
    case P_CV_PICKUP: return "PICKUP"; case P_CV_TONE: return "TONE"; case P_CV_MUTE: return "MUTE"; case P_CV_DECAY: return "DECAY";
    case P_CV_WAH: return "WAH"; case P_CV_WRES: return "RESO"; case P_CV_WSPD: return "SPEED"; case P_CV_CLICK: return "CLICK";
    case P_CV_VELO: return "TOUCH"; case P_AR: return "RELEASE";
    default: return 0;
    }
}
static void pct_bip(char *s, uint32_t v) { s[0] = 0; if (v < 64u) { scat(s, "-"); fmt_u(s + 1, 64u - v); } else { scat(s, v > 64u ? "+" : ""); fmt_u(s + slen(s), v - 64u); } }
/* (0.41, Sven: MEL into DX – "direkt editieren wie bei MEL") DX's OP pages: one operator of the voice itself at a time
 * (knob 1 picks it). Its times are shown as the DX7 takes them (measured, bm_mel2dx.h); a change sounds from the next note */
static uint32_t dx_opsel;                       /* 0..5 = OP 1..6 */
static uint8_t *dx_opq(void) { return live_p[sel % NTRK].dx + (5u - dx_opsel % 6u) * 21u; }   /* (the voice keeps OP 6 first) */
static float dx_rate_sec(int r, const float (*tb)[2], uint32_t n)   /* a rate → its time (the inverse of m2d_rate) */
{
    if (r <= (int)tb[0][0]) return tb[0][1];
    for (uint32_t i = 1; i < n; i++) if (r <= (int)tb[i][0]) {
        float u = (float)(r - (int)tb[i - 1][0]) / (tb[i][0] - tb[i - 1][0]);
        return vd_exp2(vd_log2(tb[i - 1][1]) + u * (vd_log2(tb[i][1]) - vd_log2(tb[i - 1][1])));
    }
    return tb[n - 1u][1];
}
static void dx_fmt_sec(char *s, float sec) { uint32_t x10 = (uint32_t)(sec * 10000.0f + 0.5f); fmt_ms(s, x10 ? x10 : 1u); }
static void dx_op_knob(int p, knob_t *k)
{
    const uint8_t *q = dx_opq();
    char t[8];
    switch (p) {
    case PS_OP: k->label = "OPERATOR"; scat(k->value, "OP "); fmt_u(k->value + 3, dx_opsel % 6u + 1u);
        if (!q[16]) scat(k->value, " OFF");
        k->v = (float)(dx_opsel % 6u) / 5.0f; break;
    case PS_OPRATIO: k->label = "RATIO";
        if (q[17] & 1u) { scat(k->value, "FIXED"); k->v = 0; break; }
        { uint32_t r = (q[18] & 31u ? (q[18] & 31u) * 100u : 50u) * (100u + q[19] % 100u) / 100u;
          scat(k->value, "x"); fmt_u(k->value + 1, r / 100u); scat(k->value, "."); fmt_u(t, (r / 10u) % 10u); scat(k->value, t); fmt_u(t, r % 10u); scat(k->value, t);
          k->v = (float)(q[18] & 31u) / 31.0f; }
        break;
    case PS_OPLVL: k->label = "LEVEL"; fmt_u(k->value, q[16]); k->v = q[16] / 99.0f; break;
    case PS_OPATT: k->label = "ATTACK"; dx_fmt_sec(k->value, dx_rate_sec(q[0], M2D_ATT, 10)); k->v = 1.0f - q[0] / 99.0f; break;
    case PS_OPDEC: k->label = "DECAY"; if (q[5] >= 99u && q[6] >= 99u) scat(k->value, "HOLD"); else dx_fmt_sec(k->value, dx_rate_sec(q[1], M2D_DEC, 10)); k->v = 1.0f - q[1] / 99.0f; break;
    case PS_OPSUS: k->label = "SUSTAIN"; fmt_u(k->value, q[6]); k->v = q[6] / 99.0f; break;
    default: k->label = "RELEASE"; dx_fmt_sec(k->value, dx_rate_sec(q[3], M2D_DEC, 10)); k->v = 1.0f - q[3] / 99.0f; break;
    }
}
static void dx_op_turn(int p, int e)
{
    uint8_t *q = dx_opq();
    int st = e > 1 || e < -1 ? 4 : 1, sg = e > 0 ? 1 : -1;
    switch (p) {
    case PS_OP: dx_opsel = (uint32_t)clampi((int32_t)dx_opsel + sg, 0, 5); return;
    case PS_OPRATIO: {                          /* through MEL's ratios (0.5 .. 16), from the one nearest */
        uint32_t r = (q[18] & 31u ? (q[18] & 31u) * 100u : 50u) * (100u + q[19] % 100u) / 100u, best = 0, i;
        for (i = 1; i < NRATIO; i++) { uint32_t d = RATIO_X100[i] > r ? RATIO_X100[i] - r : r - RATIO_X100[i], db = RATIO_X100[best] > r ? RATIO_X100[best] - r : r - RATIO_X100[best]; if (d < db) best = i; }
        best = (uint32_t)clampi((int32_t)best + sg, 1, NRATIO - 1);   /* (1: 0.5 – the DX7 has no 0.25) */
        q[17] = 0; m2d_ratio(best, &q[18], &q[19]); break; }
    case PS_OPLVL: q[16] = (uint8_t)clampi((int32_t)q[16] + sg * st, 0, 99); break;
    case PS_OPATT: q[0] = (uint8_t)clampi((int32_t)q[0] - sg * st, 0, 99); break;   /* (right: longer = a slower rate) */
    case PS_OPDEC:
        if (q[5] >= 99u && q[6] >= 99u && sg < 0) { q[5] = q[6] = 0; q[1] = q[2] = 99; break; }   /* (from HOLD: the longest decay first … ) */
        q[1] = q[2] = (uint8_t)clampi((int32_t)q[1] - sg * st, 0, 99); if (q[5] >= 99u && q[6] >= 99u) { q[5] = q[6] = 0; } break;
    case PS_OPSUS: q[6] = (uint8_t)clampi((int32_t)q[6] + sg * st, 0, 99); if (q[5] < q[6]) q[5] = q[6]; break;
    default: q[3] = (uint8_t)clampi((int32_t)q[3] - sg * st, 0, 99); break;
    }
    trk_edited[sel % NTRK] = 1;
}
static const page_t *PG(uint32_t pg)            /* the page's table row: SOUND's and ENV's by the track's kind */
{
    if (pg <= PG_AENV) { uint32_t ty = trk_type(sel); if (ty == T_EP && live_p[sel].v[P_EP_CLAV]) ty = T_CV; return &SPAGE[ty == T_DRUMS && live_p[sel].v[P_KIT_SRC] ? NTYPE : ty][pg]; }   /* (0.41: E-PIANO's CLAV: its own pages) */   /* (0.13: the acoustic kit has its own pages) */
    return &PAGE[pg % NPAGE];
}
/* (0.31, Sven: "4 Parameter je Drum, aber keine Lautstärke") SHIFT held on a drum page: the knobs of the drums ON that page
 * become their LEVELS (the same as ENV's LEVELS pages): KICK page knob 4 = the kick's level, HATS 3 + 4 = closed / open … */
static int seq_shift;                           /* (declared again below – a tentative definition) */
static int drum_lvl_knob(uint32_t i)            /* -1: this knob stays what it is */
{
    static const int8_t SY[8][4] = {   /* synth kit: PERFORM KICK SNARE CLAP HATS RIM/SNAP PERC KIT */
        {P_DL_KICK, P_DL_SNARE, P_DL_CLAP, P_DL_CH}, {-1, -1, -1, P_DL_KICK}, {-1, -1, -1, P_DL_SNARE}, {-1, -1, -1, P_DL_CLAP},
        {-1, -1, P_DL_CH, P_DL_OH}, {-1, -1, P_DL_RIM, P_DL_SNAP}, {-1, -1, -1, P_DL_PERC}, {-1, -1, -1, -1}};
    static const int8_t AC[8][4] = {   /* acoustic / HIP HOP: PERFORM KICK/SN HATS TOMS CYMBALS KIT */
        {P_DL_KICK, P_DL_SNARE, P_DL_CH, P_DL_OH}, {-1, P_DL_KICK, -1, P_DL_SNARE}, {-1, P_DL_CH, P_DL_OH, -1}, {-1, P_DL_CLAP, -1, P_DL_RIM},
        {-1, P_DL_SNAP, -1, P_DL_PERC}, {-1, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}};
    if (!seq_shift || trk_type(sel) != T_DRUMS || page > PG_S7 || i > 3u) return -1;
    return (live_p[sel].v[P_KIT_SRC] ? AC : SY)[page][i];
}
static const char *drum_lvl_label(uint32_t p)   /* "KICK LVL" – the drum's name as this kit calls it */
{
    static char b[4][16]; static uint32_t r; char *s = b[r++ & 3u];
    s[0] = 0; scat(s, drum_name(live_p[sel].v, p - P_DL_KICK)); scat(s, " LVL");
    return s;
}
/* (0.31) SHIFT on the SAMPLER's EDIT page: knob 1 = its input's GAIN, knob 2 = the THRESHOLD (0: the knob stays) */
static int smp_shift_knob(uint32_t i) { return seq_shift && trk_type(sel) == T_SMP && page == PG_PERFORM && i < 3u ? (i == 2u ? PS_REV : i ? PS_THR : PS_GAIN) : 0; }
static uint8_t *smp_gain_ref(void);             /* (bm_sampler.h) */
static uint32_t snd_count(void) { uint32_t ty = trk_type(sel); if (ty == T_EP && live_p[sel].v[P_EP_CLAV]) ty = T_CV; return ty == T_DRUMS && live_p[sel].v[P_KIT_SRC] ? SND_N_AC : SND_N[ty]; }   /* SOUND's pages for this track (TYPE follows them) */
static uint32_t cycle_page(uint32_t first, uint32_t count)   /* a key steps through its pages */
{
    if (page >= first && page < first + count) return first + (page - first + 1u) % count;
    return first;
}
static int key_lit(void) { const group_t *g = page_group(page); return g->key < 6 ? g->key : -1; }
static int global_lit(void) { return page == PG_GLOBAL || page == PG_OUTPUT || page == PG_SCALE; }

static const char *const DV_NAME[NDV] = {"OVERDRV", "DIST", "FUZZ", "CRUSH", "AMP"};
static const char *const CLICK_NAME[NCLICK] = {"OFF", "REC", "ON", "COUNT-IN"};

static int seq_shift;                           /* (o3_seq.h: SHIFT held) */
/* the four knobs of the page shown */
static void page_knobs(knob_t *k)
{
    for (int i = 0; i < 4; i++) {
        k[i].label = 0; k[i].value[0] = 0; k[i].v = 0; k[i].bip = 0;
        if (page == PG_DRIVE) {                 /* the drive send's effect: the settings' */
            static const char *const DL[4] = {"TYPE", "DRIVE", "TONE", "MIX"}, *const AL[4] = {"TYPE", "GAIN", "TONE", "TRACK A"};
            static char tl[10];
            const int amp = set.dv_type % NDV == DV_AMP;
            k[i].label = amp ? AL[i] : DL[i];   /* (0.8) the AMP is an insert */
            if (amp && i == 3) {                /* (0.12, Sven turned the amp on and heard nothing: the send stood at 0) knob 4 = how much of the CHOSEN track runs through it */
                uint32_t v = set.mix_drv[sel % NTRK];
                tl[0] = 0; scat(tl, "TRACK "); tl[6] = (char)('A' + sel % NTRK); tl[7] = 0; k[i].label = tl;
                if (!v) scat(k[i].value, "OFF"); else { fmt_u(k[i].value, (v * 100u + 63u) / 127u); scat(k[i].value, "%"); }
                k[i].v = v / 127.0f; continue;
            }
            if (!i) { scat(k[i].value, DV_NAME[set.dv_type % NDV]); k[i].v = (float)(set.dv_type % NDV) / (NDV - 1); }
            else { uint32_t v = i == 1 ? set.dv_amt : i == 2 ? set.dv_tone : set.dv_mix; fmt_u(k[i].value, v); k[i].v = v / 127.0f; }
            continue;
        }
        if (page == PG_GLOBAL) {
            static const char *const GL[4] = {"TEMPO", "CLICK", "MIDI CH", "METER"};   /* (12.11.2026: METER, the track's; SHIFT: TUNE) */
            static char ml[10]; ml[0] = 0; scat(ml, "METER "); ml[6] = (char)('A' + sel % NTRK); ml[7] = 0;
            k[i].label = i == 3 ? (seq_shift ? "TUNE" : ml) : i == 2 && seq_shift ? "AUTO SAVE" : GL[i];
            if (i == 2 && seq_shift) { scat(k[i].value, set.au_autosave ? "ON" : "OFF"); k[i].v = set.au_autosave ? 1.0f : 0.0f; continue; }   /* (0.31) takes to the card at once, or only when named */
            if (i == 0) { fmt_u(k[i].value, set.tempo); k[i].v = (set.tempo - 40) / 200.0f; }
            else if (i == 1) { scat(k[i].value, CLICK_NAME[set.click % NCLICK]); k[i].v = (float)(set.click % NCLICK) / (NCLICK - 1); }
            else if (i == 2) { if (set.midi_auto) scat(k[i].value, "AUTO"); else if (set.midi_ch) fmt_u(k[i].value, set.midi_ch); else scat(k[i].value, "OMNI"); k[i].v = set.midi_auto ? 0.0f : (set.midi_ch + 1u) / 17.0f; }
            else if (!seq_shift) { scat(k[i].value, METER[meter_of(sel)].name); k[i].v = (float)meter_of(sel) / (NMETER - 1); }
            else { scat(k[i].value, set.tune < 0 ? "-" : "+"); fmt_u(k[i].value + 1, (uint32_t)(set.tune < 0 ? -set.tune : set.tune)); scat(k[i].value, "c"); k[i].v = (set.tune + 50) / 100.0f; k[i].bip = 1; }
            continue;
        }
        int p = PG(page)->p[i];
        if (drum_lvl_knob((uint32_t)i) >= 0) p = drum_lvl_knob((uint32_t)i);   /* (0.31) SHIFT: the drum's level */
        if (smp_shift_knob((uint32_t)i)) p = smp_shift_knob((uint32_t)i);
        if (p == PS_REV) { k[i].label = "REVERSE"; scat(k[i].value, live_p[sel].v[P_SM_REV] ? "REV" : "FWD"); k[i].v = live_p[sel].v[P_SM_REV] ? 1.0f : 0.0f; continue; }
        if (p == PS_GAIN || p == PS_THR) {      /* (0.31) the input's GAIN, the THRESHOLD */
            uint32_t q = sel % 8u;
            if (p == PS_GAIN) { uint32_t g = smp_gain_ref() ? *smp_gain_ref() : 0u; k[i].label = "IN GAIN"; scat(k[i].value, "+"); fmt_u(k[i].value + 1, g); scat(k[i].value, " dB"); k[i].v = g / 40.0f; }
            else { k[i].label = "THRESHOLD"; if (!set.smp_thr[q]) scat(k[i].value, "OFF"); else { scat(k[i].value, "-"); fmt_u(k[i].value + 1, 55u - set.smp_thr[q]); scat(k[i].value, " dB"); } k[i].v = set.smp_thr[q] / 54.0f; }
            continue;
        }
        if (p < 0) continue;
        if (p >= PS_OP && p <= PS_OPREL) { dx_op_knob(p, &k[i]); continue; }   /* (0.41) */
        if (p == PS_EPMODEL) {                  /* (0.41) E-PIANO's MODEL: TINE · REED · CLAV */
            const uint8_t *ev = live_p[sel % NTRK].v; uint32_t m = ev[P_EP_CLAV] ? 2u : ev[P_EP_MODEL] & 1u;
            k[i].label = "MODEL"; scat(k[i].value, m == 2u ? "CLAV" : m ? "REED" : "TINE"); k[i].v = (float)m / 2.0f;
            continue;
        }
        if (p == PS_CUT || p == PS_MOVE) {      /* (0.21) the SAMPLER's CHOP page: the chosen cut, where it is */
            uint32_t q = sel % 8u, n = sm_n[q], a2, b2, hi2;
            k[i].label = p == PS_CUT ? "CUT" : "MOVE";
            if (n < 2u) { scat(k[i].value, "--"); continue; }
            if (p == PS_CUT) { fmt_u(k[i].value, sm_cur[q] + 1u); scat(k[i].value, "/"); fmt_u(k[i].value + slen(k[i].value), n); k[i].v = (float)sm_cur[q] / (float)(n - 1u); }
            else if (sm_region(q, &a2, &b2, &hi2)) { fmt_secs(k[i].value, sm_at[q][sm_cur[q] % 17u] - a2); k[i].v = (float)(sm_at[q][sm_cur[q] % 17u] - a2) / (float)(b2 - a2); }

            continue;
        }
        uint32_t v = ed_patch((uint32_t)p)->v[p];
        k[i].label = PDEF[p].name; param_value(k[i].value, (uint32_t)p, v);
        if (trk_type(sel) == T_MIDI) {          /* (0.30) the drum levels' bytes are its CCs here */
            const char *ml = mi_label((uint32_t)p, ed_patch((uint32_t)p)->v); if (ml) k[i].label = ml;
            if (p >= P_MI_CC1N && p <= P_MI_CC4N) { k[i].value[0] = 0; scat(k[i].value, "CC "); fmt_u(k[i].value + 3, mi_ccn(v)); }
        }
        if (trk_type(sel) == T_EP && !live_p[sel % NTRK].v[P_EP_CLAV]) {   /* (0.31) */
            const char *el = ep_label((uint32_t)p, ed_patch((uint32_t)p)->v); if (el) k[i].label = el;
            if (p == P_EP_MODEL) { k[i].value[0] = 0; scat(k[i].value, v & 1u ? "REED" : "TINE"); }
            else if (p == P_EP_STEREO) { k[i].value[0] = 0; if (v < 8u) scat(k[i].value, "MONO"); else if (v > 119u) scat(k[i].value, "L-R"); else { fmt_u(k[i].value, (v * 100u + 63u) / 127u); scat(k[i].value, "%"); } }
        }
        if (trk_type(sel) == T_WT) {            /* (0.41) */
            const char *wl = wt_label((uint32_t)p); if (wl) k[i].label = wl;
            if (p == P_WT_TABLE) { k[i].value[0] = 0; scat(k[i].value, WT_NAME[v % NWT]); }
            else if (p == P_WT_POS) { k[i].value[0] = 0; fmt_u(k[i].value, (v * 100u + 63u) / 127u); scat(k[i].value, "%"); }
            else if (p == P_WT_PENV) { k[i].value[0] = 0; if (v < 64u) { scat(k[i].value, "-"); fmt_u(k[i].value + 1, 64u - v); } else { scat(k[i].value, v > 64u ? "+" : ""); fmt_u(k[i].value + slen(k[i].value), v - 64u); } }
            else if (p == P_WT_SEMI) { int32_t st = (int32_t)(v % 24u) - 12; k[i].value[0] = 0; if (st) scat(k[i].value, st > 0 ? "+" : "-"); fmt_u(k[i].value + slen(k[i].value), (uint32_t)(st < 0 ? -st : st)); scat(k[i].value, " st"); }
            else if (p == P_WT_DET) { k[i].value[0] = 0; fmt_u(k[i].value, (v * 50u + 63u) / 127u); scat(k[i].value, " ct"); }
            else if (p == P_WT_PDEC && v >= 127u) { k[i].value[0] = 0; scat(k[i].value, "HOLD"); }
            else if (p == P_WT_SUB) { k[i].value[0] = 0; if (v) fmt_u(k[i].value, v); else scat(k[i].value, "OFF"); }   /* (its byte is a time knob in MEL) */
        }
        if (trk_type(sel) == T_GP) {            /* (0.41) */
            const char *gl2 = gp_label((uint32_t)p); if (gl2) k[i].label = gl2;
            if (p == P_GP_TONE || p == P_GP_HAMMER) pct_bip(k[i].value, v);
            else if (p == P_GP_DECAY && v >= 127u) { k[i].value[0] = 0; scat(k[i].value, "NATURAL"); }
        }
        if (trk_type(sel) == T_CV || (trk_type(sel) == T_EP && live_p[sel % NTRK].v[P_EP_CLAV])) {   /* (0.41) E-PIANO's CLAV */
            const char *cl = cv_label((uint32_t)p); if (cl) k[i].label = cl;
            if (p == P_CV_PICKUP) { k[i].value[0] = 0; scat(k[i].value, CV_PU_NAME[v % NCVPU]); }
            else if (p == P_CV_WAH && !v) { k[i].value[0] = 0; scat(k[i].value, "OFF"); }
        }
        if (trk_type(sel) == T_DRUMS && live_p[sel].v[P_KIT_SRC] && ac_label((uint32_t)p, live_p[sel].v[P_KIT_SRC])) k[i].label = ac_label((uint32_t)p, live_p[sel].v[P_KIT_SRC]);
        if (drum_lvl_knob((uint32_t)i) >= 0) k[i].label = drum_lvl_label((uint32_t)p);
        k[i].v = PDEF[p].max ? (float)v / (float)PDEF[p].max : 0;
        k[i].bip = PDEF[p].kind == K_BIP || PDEF[p].kind == K_SEMI || PDEF[p].kind == K_MORPH;
        if (trk_type(sel) == T_WT) k[i].bip = p == P_WT_PENV || p == P_WT_SEMI;   /* (0.41: the bytes are MEL's) */
        if (trk_type(sel) == T_GP) k[i].bip = p == P_GP_TONE || p == P_GP_HAMMER;   /* (0.41: the bytes are the drums') */
        if (trk_type(sel) == T_EP && live_p[sel % NTRK].v[P_EP_CLAV]) k[i].bip = 0;
        if (p == P_SM_ST_C || p == P_SM_EN_C) {   /* (0.21) as a time into the sample */
            uint32_t a2, b2, hi2;
            if (sm_region(sel % 8u, &a2, &b2, &hi2)) { k[i].value[0] = 0; fmt_secs(k[i].value, p == P_SM_ST_C ? a2 : b2); }
            k[i].v = (float)sm_pos14(sel, p == P_SM_EN_C) / 16383.0f;
        }
    }
}
static void auto_rec(uint32_t p);              /* (o3_seq.h: REC records the knob) */
static void meter_turn(int e);                 /* (o3_seq.h) */
static void knob_turn(int i, int e)             /* e: whole detents */
{
    if (page == PG_DRIVE) {
        if (!i) {
            uint32_t was = set.dv_type % NDV;
            set.dv_type = (uint8_t)clampi((int32_t)was + (e > 0 ? 1 : -1), 0, NDV - 1);
            if (set.dv_type == DV_AMP && was != DV_AMP) {   /* (0.12) the amp comes on: the chosen track runs through it at once */
                static char m[20];
                set.dv_mix = 127;
                if (!set.mix_drv[sel % NTRK]) set.mix_drv[sel % NTRK] = 127;
                m[0] = 0; scat(m, "AMP ON TRACK "); m[13] = (char)('A' + sel % NTRK); m[14] = 0; say(m);
            }
        }
        else if (i == 3 && set.dv_type % NDV == DV_AMP) { uint8_t *v = &set.mix_drv[sel % NTRK]; *v = (uint8_t)clampi((int32_t)*v + e * 2, 0, 127); }
        else { uint8_t *v = i == 1 ? &set.dv_amt : i == 2 ? &set.dv_tone : &set.dv_mix; *v = (uint8_t)clampi((int32_t)*v + e * 2, 0, 127); }
        mix_apply(); set_touch(); return;
    }
    if (page == PG_GLOBAL) {
        if (i == 0) set.tempo = (uint8_t)clampi(set.tempo + e, 40, 240);
        else if (i == 1) set.click = (uint8_t)clampi((int32_t)(set.click % NCLICK) + (e > 0 ? 1 : -1), 0, NCLICK - 1);
        else if (i == 2 && seq_shift) { set.au_autosave = e > 0 ? 1u : 0u; say(set.au_autosave ? "AUTO SAVE ON – EVERY TAKE TO THE CARD" : "AUTO SAVE OFF – SAVE > AUDIO KEEPS A TAKE"); }
        else if (i == 2) { int32_t v = clampi((set.midi_auto ? -1 : (int32_t)set.midi_ch) + e, -1, 16); set.midi_auto = v < 0; set.midi_ch = (uint8_t)(v < 0 ? 0 : v); }   /* AUTO · OMNI · 1..16 */
        else if (!seq_shift) { meter_turn(e); return; }
        else set.tune = (int8_t)clampi(set.tune + e, -50, 50);
        mix_apply(); set_touch(); return;
    }
    int p = PG(page)->p[i];
    if (drum_lvl_knob((uint32_t)i) >= 0) p = drum_lvl_knob((uint32_t)i);   /* (0.31) SHIFT: the drum's level */
    if (smp_shift_knob((uint32_t)i)) p = smp_shift_knob((uint32_t)i);
    if (p == PS_GAIN) { uint8_t *g = smp_gain_ref(); if (g) { *g = (uint8_t)clampi((int32_t)(*g > 40u ? 40u : *g) + e, 0, 40); set_touch(); } return; }
    if (p == PS_REV) { live_p[sel].v[P_SM_REV] = e > 0 ? 1u : 0u; trk_edited[sel % NTRK] = 1; return; }
    if (p == PS_THR) { uint8_t *th = &set.smp_thr[sel % 8u]; *th = (uint8_t)clampi((int32_t)*th + e, 0, 54); set_touch(); return; }
    if (p < 0) return;
    if (p >= PS_OP && p <= PS_OPREL) { dx_op_turn(p, e); return; }   /* (0.41) */
    if (p == PS_EPMODEL) {                      /* (0.41) TINE → REED → CLAV: CLAV is its own voice (the knobs' bytes stay – they mean CLAV's there) */
        uint8_t *ev = live_p[sel % NTRK].v; int32_t m = ev[P_EP_CLAV] ? 2 : (int32_t)(ev[P_EP_MODEL] & 1u), n = clampi(m + (e > 0 ? 1 : -1), 0, 2);
        if (n == m) return;
        ev[P_EP_CLAV] = (uint8_t)(n == 2); ev[P_EP_MODEL] = (uint8_t)(n == 1);
        trk_edited[sel % NTRK] = 1;
        return;
    }
    if (p == PS_CUT) { uint32_t q = sel % 8u; if (sm_n[q] > 1u) sm_cur[q] = (uint32_t)clampi((int32_t)sm_cur[q] + (e > 0 ? 1 : -1), 0, (int32_t)sm_n[q] - 1); return; }   /* (0.21) CHOP by hand */
    if (p == PS_MOVE) { sm_cut_move(sel, e, seq_shift); return; }
    if (p == P_SM_ST_C || p == P_SM_EN_C) {   /* START / END: 14 bit over two parameters – a fast turn goes far, SHIFT goes finest */
        int end = p == P_SM_EN_C; int32_t v = (int32_t)sm_pos14(sel, end), o = (int32_t)sm_pos14(sel, !end);
        v += e * (seq_shift ? 1 : (e > 1 || e < -1) ? 96 : 16);
        v = end ? clampi(v, o + 64, 16383) : clampi(v, 0, o - 64);
        set_param((uint32_t)p, v >> 7); set_param((uint32_t)p + 1u, v & 127);
        sm_turn_t = o3_ms; sm_turn_end = (uint32_t)end;   /* (the wave zooms to it for a moment) */
        sm_auto(sel); return;
    }
    if (p == P_SM_SLICES || p == P_SM_SENS) sm_auto(sel);   /* (turned: the cuts are found again) */
    set_param((uint32_t)p, (int32_t)ed_patch((uint32_t)p)->v[p] + e * (PDEF[p].max >= 100 ? 2 : 1));
    auto_rec((uint32_t)p);
    if (is_a_only((uint32_t)p)) {               /* the effects: the song's, kept in the settings */
        for (uint32_t j = 0; j < NFXP; j++) set.fx[j] = live_p[0].v[P_CHO_RATE + j];
        set.fx_ok = FX_OK; set_touch();
    }
}

/* SAVE: tapped, it asks "SAVE HERE?" (DATA picks another slot, 3 s to answer); tapped again, the sound is saved there */
static uint32_t save_armed, save_t, save_to, pat_save_armed;   /* (pat_save_armed: F5 in the sequencer asked) */
static void save_tap(void)
{
    if (!save_armed) { save_armed = 1; save_t = o3_ms; save_to = trk_prog[sel]; return; }
    uint32_t n = save_to % NPROG, t = sel % NTRK, u;
    patch_t sp = live_p[t];
    prog_put(n, &sp);
    trk_prog[t] = n; trk_edited[t] = 0; set_tprog(t, n);
    set_touch();
    for (u = 0; u < NTRK; u++)                  /* (another track on the same program, unchanged: it hears it too) */
        if (u != t && trk_prog[u] == n && !trk_edited[u]) { live_p[u] = sp; if (!u) fx_keep(&live_p[0]); }
    save_armed = 0;
    say("SAVED");
}
static void save_restore(void)                  /* (0.41) SAVE HERE? + ENTER: the chosen slot gets its factory sound back */
{
    uint32_t n = save_to % NPROG, t = sel % NTRK, u;
    prog_restore(n);
    for (u = 0; u < NTRK; u++) if (trk_prog[u] == n && (u == t || !trk_edited[u])) trk_load(u, n);
    set_touch(); save_armed = 0;
    say("FACTORY SOUND BACK IN THIS SLOT");
}

/* SAVE held + ENTER: a new sound of the track's kind within useful ranges (the dice) */
static uint32_t rnd = 0x2545F491u;
static uint32_t rn(uint32_t lo, uint32_t hi) { return lo + xorshift(&rnd) % (hi - lo + 1u); }
static void randomize(void)
{
    uint32_t i, ty = trk_type(sel);
    patch_t *d = &live_p[sel % NTRK];
    for (i = 0; i < NPARAM; i++) d->v[i] = PDEF[i].def;
    d->v[P_TYPE] = (uint8_t)ty;
    if (ty == T_DRUMS) {
        for (i = P_K_TUNE; i <= P_P_TONE; i++) d->v[i] = (uint8_t)rn(20, 110);
        d->v[P_K_DRIVE] = (uint8_t)rn(0, 60); d->v[P_C_DRIVE] = (uint8_t)rn(0, 40);
        d->v[P_CH_DECAY] = (uint8_t)rn(10, 50); d->v[P_OH_DECAY] = (uint8_t)rn(40, 100);
        d->v[P_CUTOFF] = (uint8_t)rn(90, 127);
    } else if (ty == T_BASS) {
        d->v[P_B_PUNCH] = (uint8_t)rn(30, 110); d->v[P_B_PTIME] = (uint8_t)rn(20, 70); d->v[P_B_CLICK] = (uint8_t)rn(0, 70);
        d->v[P_B_TONE] = (uint8_t)rn(0, 70); d->v[P_B_SUB] = (uint8_t)(rn(0, 2) ? 0 : rn(30, 100));
        d->v[P_B_DRIVE] = (uint8_t)rn(0, 110); d->v[P_B_DTYPE] = (uint8_t)rn(0, NBDT - 1); d->v[P_B_SLIDE] = (uint8_t)rn(0, 1);
        d->v[P_AD] = (uint8_t)rn(70, 110); d->v[P_GLIDE] = (uint8_t)rn(10, 70); d->v[P_CUTOFF] = (uint8_t)rn(60, 127);
    } else if (ty == T_MEL) {
        d->v[P_ALGO] = (uint8_t)rn(0, NALGO - 1); d->v[P_FB] = (uint8_t)(rn(0, 1) ? 0 : rn(10, 80));
        for (i = 0; i < 4u; i++) {
            uint32_t pb = P_OP1_RATIO + 4u * i;
            d->v[pb] = (uint8_t)(rn(0, 2) ? rn(1, 8) : rn(8, NRATIO - 1)); d->v[pb + 1u] = (uint8_t)(i ? rn(0, 90) : rn(80, 127));
            d->v[pb + 2u] = (uint8_t)(rn(0, 3) ? 0 : rn(10, 80)); d->v[pb + 3u] = (uint8_t)(rn(0, 1) ? 127 : rn(40, 110));
        }
        d->v[P_CUTOFF] = (uint8_t)rn(60, 127); d->v[P_RESO] = (uint8_t)rn(0, 60); d->v[P_FENV_AMT] = (uint8_t)rn(50, 100);
        d->v[P_AA] = (uint8_t)rn(0, 80); d->v[P_AD] = (uint8_t)rn(50, 110); d->v[P_AS] = (uint8_t)(rn(0, 1) ? 0 : rn(40, 127)); d->v[P_AR] = (uint8_t)rn(30, 100);
        d->v[P_FA] = (uint8_t)rn(0, 60); d->v[P_FD] = (uint8_t)rn(40, 100); d->v[P_FS] = (uint8_t)rn(0, 80); d->v[P_FR] = (uint8_t)rn(30, 90);
        d->v[P_LFO_RATE] = (uint8_t)rn(40, 90); d->v[P_LFO_PITCH] = (uint8_t)(rn(0, 2) ? 0 : rn(3, 15)); d->v[P_LFO_DELAY] = (uint8_t)rn(0, 70);
        d->v[P_MODE] = (uint8_t)(rn(0, 4) ? M_POLY : M_MONO); d->v[P_GLIDE] = (uint8_t)(d->v[P_MODE] == M_MONO && rn(0, 1) ? rn(5, 40) : 0);
    }
    if (ty == T_SMP) {                          /* SAMPLER: the same take, another cut */
        d->v[P_SM_SLICES] = (uint8_t)rn(0, 1); d->v[P_SM_CHOP] = (uint8_t)rn(0, 1); d->v[P_SM_QUAL] = (uint8_t)rn(0, 3); d->v[P_SM_REV] = (uint8_t)(rn(0, 4) ? 0 : 1);
        d->v[P_SM_TUNE] = (uint8_t)(rn(0, 2) ? 64 : rn(52, 76)); d->v[P_AA] = 0; d->v[P_AS] = 127; d->v[P_AR] = (uint8_t)rn(10, 50);
    }
    if (ty == T_STR) {                          /* STRINGS: the registers, a slow start, an open filter */
        d->v[P_ST_16] = (uint8_t)(rn(0, 1) ? 0 : rn(40, 120)); d->v[P_ST_8] = (uint8_t)rn(70, 127); d->v[P_ST_4] = (uint8_t)rn(0, 120); d->v[P_ST_ENS] = (uint8_t)rn(80, 127);
        d->v[P_AA] = (uint8_t)rn(60, 95); d->v[P_AS] = 127; d->v[P_AR] = (uint8_t)rn(80, 104); d->v[P_CUTOFF] = (uint8_t)rn(80, 120); d->v[P_RESO] = 0; d->v[P_FENV_AMT] = 64;
        d->v[P_MODE] = M_POLY; d->v[P_GLIDE] = 0;
    }
    if (ty == T_EP) {                           /* (0.31) E-PIANO: a tine or a reed, struck somewhere between soft and hard */
        d->v[P_EP_CLAV] = 0; d->v[P_EP_MODEL] = (uint8_t)rn(0, 1); d->v[P_EP_TONE] = (uint8_t)rn(30, 95); d->v[P_EP_BARK] = (uint8_t)rn(15, 115); d->v[P_EP_BELL] = (uint8_t)rn(20, 100);
        d->v[P_EP_DECAY] = (uint8_t)rn(60, 100); d->v[P_EP_TREM] = (uint8_t)(rn(0, 1) ? 0 : rn(25, 90)); d->v[P_EP_RATE] = (uint8_t)rn(30, 75); d->v[P_EP_STEREO] = (uint8_t)(rn(0, 1) ? 127 : 0);
        d->v[P_EP_DRIVE] = (uint8_t)(rn(0, 1) ? 0 : rn(10, 80)); d->v[P_EP_NOISE] = (uint8_t)rn(20, 60); d->v[P_EP_VELO] = (uint8_t)rn(70, 110);
        d->v[P_AA] = 0; d->v[P_AR] = (uint8_t)rn(56, 74); d->v[P_CUTOFF] = (uint8_t)rn(100, 127); d->v[P_RESO] = 0; d->v[P_FENV_AMT] = 64; d->v[P_MODE] = M_POLY; d->v[P_GLIDE] = 0;
    }
    if (ty == T_WT) {                           /* (0.41) WAVETABLE: a table, somewhere in it, moving or not */
        d->v[P_WT_TABLE] = (uint8_t)rn(0, NWT - 1u); d->v[P_WT_POS] = (uint8_t)rn(0, 127); d->v[P_WT_PENV] = (uint8_t)(rn(0, 2) ? rn(70, 127) : 64);
        d->v[P_WT_PATT] = (uint8_t)(rn(0, 1) ? 0 : rn(20, 90)); d->v[P_WT_PDEC] = (uint8_t)(rn(0, 2) ? rn(50, 110) : 127); d->v[P_WT_LFO] = (uint8_t)(rn(0, 2) ? 0 : rn(10, 60));
        d->v[P_WT_O2] = (uint8_t)(rn(0, 1) ? rn(50, 120) : 0); d->v[P_WT_SEMI] = (uint8_t)(rn(0, 3) ? 12 : rn(0, 1) ? 19 : 0); d->v[P_WT_DET] = (uint8_t)rn(0, 70); d->v[P_WT_SUB] = (uint8_t)(rn(0, 2) ? 0 : rn(30, 100));
        d->v[P_CUTOFF] = (uint8_t)rn(70, 127); d->v[P_RESO] = (uint8_t)rn(0, 50); d->v[P_FENV_AMT] = (uint8_t)rn(56, 100);
        d->v[P_AA] = (uint8_t)rn(0, 80); d->v[P_AD] = (uint8_t)rn(50, 110); d->v[P_AS] = (uint8_t)rn(40, 127); d->v[P_AR] = (uint8_t)rn(30, 100);
        d->v[P_MODE] = (uint8_t)(rn(0, 4) ? M_POLY : M_MONO); d->v[P_GLIDE] = (uint8_t)(d->v[P_MODE] == M_MONO ? rn(5, 40) : 0);
    }
    if (ty == T_GP) {                           /* (0.41) PIANO: a grand, somewhere between soft and bright */
        d->v[P_GP_TONE] = (uint8_t)rn(30, 100); d->v[P_GP_HAMMER] = (uint8_t)rn(40, 90); d->v[P_GP_DECAY] = (uint8_t)(rn(0, 2) ? 127 : rn(70, 120));
        d->v[P_GP_VELO] = (uint8_t)rn(60, 110); d->v[P_GP_SPREAD] = (uint8_t)rn(30, 100); d->v[P_AA] = 0; d->v[P_AR] = (uint8_t)rn(40, 66);
        d->v[P_CUTOFF] = (uint8_t)rn(96, 127); d->v[P_RESO] = 0; d->v[P_FENV_AMT] = 64; d->v[P_MODE] = M_POLY; d->v[P_GLIDE] = 0;
    }
    if (ty == T_DX) {                           /* DX: one of the factory voices, the macros a little off the middle */
        memcpy(d->dx, DX_SYNTH[rn(0, DX_NSYNTH - 2u)], DX_VOICE);
        d->v[P_DX_BRIGHT] = (uint8_t)rn(44, 90); d->v[P_DX_ATTACK] = (uint8_t)rn(56, 80); d->v[P_DX_DECAY] = (uint8_t)rn(44, 90); d->v[P_DX_RELEASE] = (uint8_t)rn(50, 90);
        d->v[P_CUTOFF] = 127; d->v[P_RESO] = 10; d->v[P_FENV_AMT] = 64;
    }
    if (ty == T_SYNTH) {                        /* OberVave 2.0's dice */
        uint32_t r;
        d->v[P_O1_WAVE] = (uint8_t)rn(1, 3); d->v[P_O2_WAVE] = (uint8_t)rn(0, 3);
        d->v[P_O1_PW] = (uint8_t)rn(10, 110); d->v[P_O2_PW] = (uint8_t)rn(10, 110); d->v[P_O2_LVL] = (uint8_t)rn(60, 127);
        r = rn(0, 9); d->v[P_O2_SEMI] = (uint8_t)(r < 4 ? 24 : r < 6 ? 12 : r < 8 ? 36 : r == 8 ? 31 : 43);
        d->v[P_O2_FINE] = (uint8_t)rn(60, 76); d->v[P_NOISE] = (uint8_t)(rn(0, 5) ? 0 : rn(10, 70));
        d->v[P_SYNC] = (uint8_t)(rn(0, 4) == 0); d->v[P_XMOD] = (uint8_t)(rn(0, 4) ? 0 : rn(20, 100));
        d->v[P_ENV_O2] = (uint8_t)(d->v[P_SYNC] ? rn(70, 110) : 64);
        d->v[P_CUTOFF] = (uint8_t)rn(30, 110); d->v[P_RESO] = (uint8_t)rn(0, 90); d->v[P_FENV_AMT] = (uint8_t)rn(60, 120);
        d->v[P_FTYPE] = (uint8_t)(rn(0, 2) ? F_LP24 : rn(0, NFTYPE - 1)); d->v[P_FMORPH] = (uint8_t)rn(0, 127);
        d->v[P_DRIVE] = (uint8_t)(rn(0, 2) ? 0 : rn(10, 70));
        d->v[P_FA] = (uint8_t)rn(0, 90); d->v[P_FD] = (uint8_t)rn(30, 100); d->v[P_FS] = (uint8_t)rn(0, 100); d->v[P_FR] = (uint8_t)rn(20, 100);
        d->v[P_AA] = (uint8_t)rn(0, 80); d->v[P_AD] = (uint8_t)rn(30, 100); d->v[P_AS] = (uint8_t)rn(0, 127); d->v[P_AR] = (uint8_t)rn(20, 100);
        d->v[P_LFO_SHAPE] = (uint8_t)rn(0, 5); d->v[P_LFO_RATE] = (uint8_t)rn(20, 100);
        d->v[P_LFO_PITCH] = (uint8_t)(rn(0, 2) ? 0 : rn(5, 40)); d->v[P_LFO_PWM] = (uint8_t)(rn(0, 1) ? 0 : rn(20, 90));
        d->v[P_LFO_FLT] = (uint8_t)(rn(0, 2) ? 0 : rn(10, 70)); d->v[P_LFO_DELAY] = (uint8_t)rn(0, 70); d->v[P_LFO_PHASE] = (uint8_t)rn(0, 2);
        r = rn(0, 9); d->v[P_MODE] = (uint8_t)(r < 6 ? M_POLY : r < 7 ? M_DUAL : r < 9 ? M_UNISON : M_MONO);
        d->v[P_UNI_N] = (uint8_t)rn(2, 6); d->v[P_UNI_DET] = (uint8_t)rn(10, 80);
        d->v[P_GLIDE] = (uint8_t)(d->v[P_MODE] >= M_UNISON && rn(0, 1) ? rn(5, 40) : 0);
        d->v[P_VINTAGE] = (uint8_t)rn(0, 100); d->v[P_SPREAD] = (uint8_t)rn(20, 127);
    }
    d->v[P_CHO_MIX] = (uint8_t)(rn(0, 2) ? 0 : rn(20, 70)); d->v[P_DLY_TIME] = (uint8_t)rn(0, NDDIV - 1);
    d->v[P_DLY_FB] = (uint8_t)rn(20, 80); d->v[P_DLY_MIX] = (uint8_t)(rn(0, 1) ? 0 : rn(20, 60));
    d->v[P_REV_SIZE] = (uint8_t)rn(40, 127); d->v[P_REV_MIX] = (uint8_t)(rn(0, 2) ? rn(15, 55) : 0);
    trk_edited[sel % NTRK] = 1;
    if (d == &live_p[0]) fx_keep(d);            /* (the dice leave the effects alone) */
}
