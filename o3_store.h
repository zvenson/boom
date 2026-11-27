/* SPDX-License-Identifier: GPL-3.0-only */
/* OberVave 3 – what stays when the device is off, in the SDK's storage. The files are OberVave 2.0's (its flash
 * sectors as host.c kept them: a 4-byte head, byte 0 = 1, then the struct): "settings", "plob0" / "plob1" (the
 * user's programs, 50 each), "blob0".."blob7" (the patterns). So 2.0's sounds and patterns carry over. */
#define ST_HEAD 4
static uint8_t st_buf[ST_HEAD + (sizeof(pattern_t) > sizeof(psect_t) ? sizeof(pattern_t) : sizeof(psect_t)) + 1024u];   /* (a pattern or a program sector: the biggest files) */
static int st_load(const char *name, void *p, uint32_t n)
{
    memset(st_buf, 0, sizeof st_buf);          /* (a shorter, older file: the rest reads as 0 – e.g. a pattern without knob lanes) */
    if (vibe_store_read(name, st_buf, (int)(ST_HEAD + n)) < (int)(ST_HEAD + 1u) || st_buf[0] != 1) return -1;
    memcpy(p, st_buf + ST_HEAD, n);
    return 0;
}
static int st_save(const char *name, const void *p, uint32_t n)
{
    memset(st_buf, 0, ST_HEAD); st_buf[0] = 1;
    memcpy(st_buf + ST_HEAD, p, n);
    return vibe_store_write(name, st_buf, (int)(ST_HEAD + n));
}

/* (BOOM 0.4) a program sector with a version (bm_save.h says why): "BMFG" 1 · tracks · parameters · slots, then per slot
 * `used`, and when used its parameters and its 10-character name. 0.3's raw sector is still read. */
static int psect_save(const char *name, const psect_t *ps)
{
    uint8_t *b = st_buf + ST_HEAD; uint32_t at = 8u;
    memset(st_buf, 0, ST_HEAD); st_buf[0] = 1;
    b[0] = 'B'; b[1] = 'M'; b[2] = 'F'; b[3] = 'G'; b[4] = 2; b[5] = (uint8_t)NTRK;   /* (2: each sound's DX voice after its name) */ b[6] = (uint8_t)NPARAM; b[7] = (uint8_t)PS_N;
    for (uint32_t i = 0; i < PS_N; i++) {
        b[at++] = ps->used[i];
        if (ps->used[i]) { memcpy(b + at, ps->p[i].v, NPARAM + PATCH_NAME); at += NPARAM + PATCH_NAME; memcpy(b + at, ps->p[i].dx, DX_VOICE); at += DX_VOICE; }
    }
    return vibe_store_write(name, st_buf, (int)(ST_HEAD + at));
}
static int psect_load(const char *name, psect_t *ps, int *legacy)
{
    int r = vibe_store_read(name, st_buf, (int)sizeof st_buf);
    const uint8_t *b = st_buf + ST_HEAD;
    *legacy = 0;
    if (r < (int)ST_HEAD + 8 || st_buf[0] != 1) return -1;
    if ((uint32_t)r == ST_HEAD + sizeof(psect_t) && !memcmp(b, &(uint32_t){PS_TAG}, 4)) { memcpy(ps, b, sizeof *ps); *legacy = 1; return 0; }   /* (0.3) */
    if (b[0] != 'B' || b[1] != 'M' || b[2] != 'F' || b[3] != 'G') return -1;
    {
        uint32_t np = b[6], ns = b[7], at = 8u, len = (uint32_t)r - ST_HEAD, xs = b[4] >= 2u ? DX_VOICE : 0u;
        memset(ps, 0, sizeof *ps); ps->tag = PS_TAG;
        for (uint32_t i = 0; i < ns; i++) {
            if (at >= len) return -1;
            if (!b[at++]) continue;
            if (at + np + PATCH_NAME + xs > len) return -1;
            if (i < PS_N) {
                patch_t *p = &ps->p[i]; uint32_t n = np < NPARAM ? np : NPARAM;
                patch_defaults(p); memcpy(p->v, b + at, n); memcpy(p->v + NPARAM, b + at + np, PATCH_NAME);
                if (xs) memcpy(p->dx, b + at + np + PATCH_NAME, DX_VOICE);
                for (uint32_t q = 0; q < NPARAM; q++) if (p->v[q] > PDEF[q].max) p->v[q] = PDEF[q].def;
                ps->used[i] = 1;
            }
            at += np + PATCH_NAME + xs;
        }
    }
    return 0;
}
static uint32_t set_dirty, ps_dirty, store_t, store_force;   /* store_force: VIBE_EV_SAVE – everything now (SDK 21) */   /* ps_dirty: bit i = program sector i; written ~1.5 s after the last change */
static void set_touch(void) { set_dirty = 1; store_t = o3_ms; }

static uint32_t mig_del;                        /* (0.41) the old program sectors, to delete once the banks are written */
static const char *ps_file(uint32_t b) { static char n[12]; n[0] = 0; scat(n, "pbank"); fmt_u(n + 5, b); return n; }   /* (0.41) bank b's sector */
static void store_boot(void)                    /* the settings and the programs as they were, else the defaults */
{
    uint32_t n, have = 0;
    static const char *const PSO[NPSECT_OLD] = {"plob0", "plob1", "plob2", "plob3", "plob4"};
    for (n = 0; n < NPSECT; n++) {
        int legacy = 0;
        if (psect_load(ps_file(n), &psect[n], &legacy) || psect[n].tag != PS_TAG) { memset(&psect[n], 0, sizeof psect[n]); psect[n].tag = PS_TAG; }
        else have = 1;
    }
    if (!have) {                                /* (0.41) no bank written yet: the programs of before (one row of 217, 50 a sector), each into its kind's bank */
        static psect_t old;
        for (n = 0; n < NPSECT_OLD; n++) {
            int legacy = 0;
            if (psect_load(PSO[n], &old, &legacy) || old.tag != PS_TAG) continue;
            mig_del |= 1u << n;
            for (uint32_t i = 0; i < PS_N_OLD; i++) {
                if (!old.used[i]) continue;
                patch_t *p = &old.p[i]; uint32_t ty = p->v[P_TYPE] % NTYPE, g, b, s;
                if (ty == T_CV) { p->v[P_TYPE] = T_EP; p->v[P_EP_CLAV] = 1; p->v[P_EP_MODEL] = 0; ty = T_EP; }   /* (a CLAV of 0.41-dev: E-PIANO's now) */
                g = prog_from_old(n * PS_N_OLD + i, ty); b = g / NSLOT; s = g % NSLOT;
                if (psect[b].used[s]) continue;
                psect[b].p[s] = *p; psect[b].used[s] = 1; ps_dirty |= 1u << b;
            }
        }
        if (mig_del && !ps_dirty) ps_dirty = 1u;   /* (nothing to carry: still write a bank, so this runs once) */
    }
    if (st_load("settings", &set, sizeof set) || set.version != SET_VERSION) settings_defaults();
    if (set.smc_ok != SMC_OK) { memset(set.smc, 0, sizeof set.smc); memset(set.smc_n, 0, sizeof set.smc_n); set.smc_ok = SMC_OK; }   /* (0.21) */
    if (set.sc_ok != 0x5Cu) { set.sc_key = set.sc_scale = set.sc_chord = set.sc_pads = 0; set.sc_ok = 0x5Cu; }   /* (before 3.10) */
    if (set.fx_ok != FX_OK || set.rec_qt_ok != RQT_OK || set.dv_ok != DV_OK || set.m_ok != M_OK) {   /* (2.0 before its last additions) */
        uint8_t keep[NTRK], keepb[NTRK], kb = set.tb_ok; memcpy(keep, set.tprog, sizeof keep); memcpy(keepb, set.tbank, sizeof keepb);
        if (set.rec_qt_ok != RQT_OK) { for (n = 0; n < NTRK; n++) set.rec_qt[n] = set.rec_q % NRQ; set.rec_qt_ok = RQT_OK; }
        if (set.dv_ok != DV_OK) { for (n = 0; n < NTRK; n++) set.mix_drv[n] = 0; set.dv_type = DV_OVERDRIVE; set.dv_amt = 60; set.dv_tone = 90; set.dv_mix = 127; set.dv_ok = DV_OK; }
        if (set.m_ok != M_OK) { set.m_vol = 100; set.m_dj = 64; set.m_drive = set.m_comp = 0; set.m_ok = M_OK; }
        if (set.fx_ok != FX_OK) settings_defaults();
        memcpy(set.tprog, keep, sizeof keep); memcpy(set.tbank, keepb, sizeof keepb); set.tb_ok = kb;
    }
    if (set.tb_ok != TB_OK) {                   /* (0.41) the settings of before: a track's program was one number of 217 */
        for (n = 0; n < NTRK; n++) { uint32_t g = prog_from_old(set.tprog[n], NTYPE); set.tprog[n] = (uint8_t)(g % NSLOT); set.tbank[n] = (uint8_t)(g / NSLOT); }
        set.tb_ok = TB_OK; set.prog = set.tprog[0];
    }
    if (set.tempo < 40u || set.tempo > 240u) set.tempo = 120;
    if (set.midi_ch > 16u) set.midi_ch = 0;
    if (set.mo_ok != MO_OK) { set.midi_out = 0; set.mo_ok = MO_OK; }   /* (3.0's settings: MIDI OUT off, as it was) */
    if (set.mfx_ok != MFX_OK) { set.mfx[0] = 64; set.mfx[1] = set.mfx[2] = 0; set.mfx_ok = MFX_OK; }
    set.mfx[3] = 0;                             /* (REPEAT never starts on) */
    fx_norm(set.fx);
    if (set.io_ok != IO_OK) { set.trk_own = set.trk_daw = 0; set.midi_auto = 1; set.io_ok = IO_OK; }
    if (set.mch_ok != MCH_OK) { memset(set.trk_mch, 0, sizeof set.trk_mch); set.mch_ok = MCH_OK; }
    if (set.eq_ok != EQ_OK) eq_defaults();
    if (set.thr_ok != THR_OK) { memset(set.smp_thr, 0, sizeof set.smp_thr); set.thr_ok = THR_OK; }   /* (0.31) */
    if (set.as_ok != AS_OK) { set.au_autosave = 0; set.as_ok = AS_OK; }   /* (0.31: no auto save – Sven) */      /* (0.31: settings from before the EQ – flat) */
    for (uint32_t t = 0; t < 8u; t++) if (set.trk_mch[t] > 16u) set.trk_mch[t] = 0;
}

static void store_poll(void)                    /* from the draw: what changed, once it has been quiet for a while */
{
    static const char *const PSO[NPSECT_OLD] = {"plob0", "plob1", "plob2", "plob3", "plob4"};
    uint32_t n;
    if ((!set_dirty && !ps_dirty) || (!store_force && o3_ms - store_t < 1500u)) return;
    for (n = 0; n < NPSECT; n++) if (ps_dirty >> n & 1u) psect_save(ps_file(n), &psect[n]);
    for (n = 0; n < NPSECT_OLD; n++) if (mig_del >> n & 1u) vibe_store_delete(PSO[n]);   /* (0.41: carried into the banks – written above) */
    mig_del = 0;
    if (set_dirty) st_save("settings", &set, sizeof set);
    set_dirty = ps_dirty = 0;
}

static void prog_put(uint32_t n, const patch_t *p)   /* program n is the user's now */
{
    n %= NPROG;
    psect[n / NSLOT].p[n % NSLOT] = *p;
    psect[n / NSLOT].used[n % NSLOT] = 1;
    ps_dirty |= 1u << (n / NSLOT); store_t = o3_ms;
}
static void prog_restore(uint32_t n)            /* (0.41) program n: the factory's again (the user's sound there is gone) */
{
    n %= NPROG;
    psect[n / NSLOT].used[n % NSLOT] = 0;
    ps_dirty |= 1u << (n / NSLOT); store_t = o3_ms;
}
