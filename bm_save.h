/* SPDX-License-Identifier: GPL-3.0-only */
/* BOOM 0.4 – the saved files' format, with a version and a way up (Sven 14.11.2026: a project in a DAW must open after
 * every update). Until 0.3 the files were the structs as they lay in memory: one more track or one more parameter and
 * every saved pattern and sound was passed over. Now each file says what is in it and is read field by field:
 *
 *   "BMF" kind · version · tracks · parameters · x      (8 bytes; kind 'S' a pattern's sounds + mixer, 'P' a pattern's
 *                                                        steps, 'G' a program sector)
 *   then the data in pieces whose sizes the header gives. A file with FEWER tracks, parameters, steps or lanes than this
 *   firmware knows fills the rest with the defaults; one with MORE (a newer firmware's) is read as far as this one knows.
 *
 * The rule that keeps it true: new parameters are only ever APPENDED to the table (bm_params.h, after the effects), never
 * put in between; a parameter's number is its identity (also for the DAW and the recorded knobs).
 * The 0.3 files (the raw structs for 6 tracks and 144 bytes a sound) are still read and written anew in this format. */
#define BMF_VER 1u
static uint32_t bmf_head(uint8_t *b, char kind, uint32_t x)
{
    b[0] = 'B'; b[1] = 'M'; b[2] = 'F'; b[3] = (uint8_t)kind; b[4] = kind == 'P' || kind == 'S' ? 2u : BMF_VER;   /* ('P' 2: each note's velocity and roll · 'S' 2: each sound's DX voice) */ b[5] = (uint8_t)NTRK; b[6] = (uint8_t)NPARAM; b[7] = (uint8_t)x;
    return 8u;
}
static int bmf_is(const uint8_t *b, uint32_t len, char kind) { return len >= 8u && b[0] == 'B' && b[1] == 'M' && b[2] == 'F' && b[3] == (uint8_t)kind && b[4] >= 1u; }
static void bmf_patch_out(uint8_t *b, const patch_t *p) { memcpy(b, p->v, NPARAM); memcpy(b + NPARAM, p->v + NPARAM, PATCH_NAME); memcpy(b + NPARAM + PATCH_NAME, p->dx, DX_VOICE); }
static void bmf_patch_in(const uint8_t *b, uint32_t np, patch_t *p, int dx)   /* np parameters, the name, (dx) the DX voice: the rest keeps the defaults */
{
    uint32_t n = np < NPARAM ? np : NPARAM;
    patch_defaults(p);
    memcpy(p->v, b, n); memcpy(p->v + NPARAM, b + np, PATCH_NAME);
    if (dx) memcpy(p->dx, b + np + PATCH_NAME, DX_VOICE);
    for (uint32_t i = 0; i < NPARAM; i++) if (p->v[i] > PDEF[i].max) p->v[i] = PDEF[i].def;   /* (never a value the engine does not know) */
}

/* ---- 'S': a pattern's four .. six sounds, their program numbers, its mixer, effects, master */
#define BMF_NSCAL 16u
static uint32_t psnd_ser(const psnd_t *s, uint8_t *b)
{
    uint32_t at = bmf_head(b, 'S', NFXP);
    for (uint32_t t = 0; t < NTRK; t++) { b[at++] = s->prog[t]; b[at++] = s->edited[t]; bmf_patch_out(b + at, &s->p[t]); at += NPARAM + PATCH_NAME + DX_VOICE; }
    for (uint32_t t = 0; t < NTRK; t++) { b[at++] = s->mx.lvl[t]; b[at++] = s->mx.cho[t]; b[at++] = s->mx.dly[t]; b[at++] = s->mx.rev[t]; b[at++] = s->mx.drv[t]; b[at++] = (uint8_t)s->mx.pan[t]; }
    memcpy(b + at, s->mx.fx, NFXP); at += NFXP;
    b[at++] = BMF_NSCAL;
    {
        const uint8_t sc[BMF_NSCAL] = {s->mx.mute, s->mx.solo, s->mx.dv_type, s->mx.dv_amt, s->mx.dv_tone, s->mx.dv_mix, s->mx.m_vol, s->mx.m_dj, s->mx.m_drive, s->mx.m_comp,
                                       s->mx.tempo, s->mx.mfx[0], s->mx.mfx[1], s->mx.mfx[2], s->mx.mfx[3], s->mx.mfx_ok};
        memcpy(b + at, sc, BMF_NSCAL); at += BMF_NSCAL;
    }
    return at;
}
static int psnd_des(const uint8_t *b, uint32_t len, psnd_t *s)   /* 0: read (this format, or 0.3's raw struct) */
{
    /* (0.3's raw struct: no longer read – every file was written anew by 0.4) */
    if (!bmf_is(b, len, 'S')) return -1;
    {
        uint32_t nt = b[5], np = b[6], nfx = b[7], at = 8u, v2 = b[4] >= 2u, rec = 2u + np + PATCH_NAME + (v2 ? DX_VOICE : 0u), ns;
        if (len < at + nt * rec + nt * 6u + nfx + 1u) return -1;
        memset(s, 0, sizeof *s); s->magic = PSN2_MAGIC;
        mx_from_set(&s->mx);                    /* (what the file does not say: as it is now) */
        for (uint32_t t = 0; t < NTRK; t++) {
            if (t < nt) { const uint8_t *r = b + at + t * rec; s->prog[t] = (uint8_t)(r[0] % NPROG); s->edited[t] = r[1]; bmf_patch_in(r + 2, np, &s->p[t], (int)v2); }
            else { prog_get(TPROG_DEF[t], &s->p[t]); s->prog[t] = TPROG_DEF[t]; }   /* (a track the file has not: its factory sound) */
        }
        at += nt * rec;
        for (uint32_t t = 0; t < NTRK; t++) {
            if (t < nt) { const uint8_t *r = b + at + t * 6u; s->mx.lvl[t] = r[0]; s->mx.cho[t] = r[1]; s->mx.dly[t] = r[2]; s->mx.rev[t] = r[3]; s->mx.drv[t] = r[4]; s->mx.pan[t] = (int8_t)r[5]; }
            else { s->mx.lvl[t] = 100; s->mx.cho[t] = s->mx.dly[t] = s->mx.rev[t] = s->mx.drv[t] = 0; s->mx.pan[t] = 0; }
        }
        at += nt * 6u;
        memcpy(s->mx.fx, b + at, nfx < NFXP ? nfx : NFXP); at += nfx; fx_norm(s->mx.fx);
        ns = b[at++]; if (len < at + ns) return -1;
        {
            uint8_t *const d[BMF_NSCAL] = {&s->mx.mute, &s->mx.solo, &s->mx.dv_type, &s->mx.dv_amt, &s->mx.dv_tone, &s->mx.dv_mix, &s->mx.m_vol, &s->mx.m_dj, &s->mx.m_drive, &s->mx.m_comp,
                                           &s->mx.tempo, &s->mx.mfx[0], &s->mx.mfx[1], &s->mx.mfx[2], &s->mx.mfx[3], &s->mx.mfx_ok};
            for (uint32_t i = 0; i < ns && i < BMF_NSCAL; i++) *d[i] = b[at + i];
        }
        if (nt < NTRK) { uint32_t m = (1u << nt) - 1u; s->mx.mute &= (uint8_t)m; s->mx.solo &= (uint8_t)m; }
    }
    return 0;
}

/* ---- 'P': a pattern's steps: per track its settings, its steps, its recorded knobs */
static uint32_t pat_ser(const pattern_t *p, uint8_t *b)
{
    uint32_t at = bmf_head(b, 'P', NLANE);
    b[at++] = (uint8_t)(SEQ_STEPS & 255u); b[at++] = (uint8_t)(SEQ_STEPS >> 8); b[at++] = (uint8_t)SEQ_CHORD; b[at++] = (uint8_t)sizeof(seqtrk_t);
    for (uint32_t t = 0; t < NTRK; t++) {
        memcpy(b + at, &p->trk[t], sizeof(seqtrk_t)); at += sizeof(seqtrk_t);
        for (uint32_t i = 0; i < SEQ_STEPS; i++) {
            const step_t *s = &p->st[t][i];
            memcpy(b + at, s->note, SEQ_CHORD); at += SEQ_CHORD;
            b[at++] = s->vel; b[at++] = s->gate; b[at++] = s->ratch; b[at++] = s->chance; b[at++] = s->nudge;
            memcpy(b + at, s->nv, SEQ_CHORD); at += SEQ_CHORD; memcpy(b + at, s->nr, SEQ_CHORD); at += SEQ_CHORD;
        }
        for (uint32_t l = 0; l < NLANE; l++) { b[at++] = p->lane[t][l].param; memcpy(b + at, p->lane[t][l].v, SEQ_STEPS); at += SEQ_STEPS; }
    }
    return at;
}
static int pat_des(const uint8_t *b, uint32_t len, pattern_t *p)
{
    /* (0.3's raw struct had 4 notes a step: no longer read – every file was written anew by 0.4) */
    if (!bmf_is(b, len, 'P') || len < 12u) return -1;
    {
        uint32_t nt = b[5], nl = b[7], ns = b[8] | (uint32_t)b[9] << 8, nc = b[10], tsz = b[11], at = 12u;
        uint32_t v2 = b[4] >= 2u, ssz = nc + 5u + (v2 ? 2u * nc : 0u), per = tsz + ns * ssz + nl * (1u + ns), cn = nc < SEQ_CHORD ? nc : SEQ_CHORD;
        if (!ns || !nc || len < at + nt * per) return -1;
        pat_init(p);
        for (uint32_t t = 0; t < nt && t < NTRK; t++) {
            const uint8_t *r = b + at + t * per;
            memcpy(&p->trk[t], r, tsz < sizeof(seqtrk_t) ? tsz : sizeof(seqtrk_t));
            if (p->trk[t].len > SEQ_STEPS) p->trk[t].len = (uint16_t)SEQ_STEPS;
            r += tsz;
            for (uint32_t i = 0; i < ns && i < SEQ_STEPS; i++) {
                step_t *s = &p->st[t][i]; const uint8_t *q = r + i * ssz;
                memset(s->note, 0, sizeof s->note); memset(s->nv, 0, sizeof s->nv); memset(s->nr, 0, sizeof s->nr); memcpy(s->note, q, cn);
                s->vel = q[nc]; s->gate = q[nc + 1u]; s->ratch = q[nc + 2u]; s->chance = q[nc + 3u]; s->nudge = q[nc + 4u];
                if (v2) { memcpy(s->nv, q + nc + 5u, cn); memcpy(s->nr, q + 2u * nc + 5u, cn); for (uint32_t c = 0; c < cn; c++) if (s->nr[c] > 5u) s->nr[c] = 0; }
            }
            r += ns * ssz;
            for (uint32_t l = 0; l < nl && l < NLANE; l++) {
                const uint8_t *q = r + l * (1u + ns);
                p->lane[t][l].param = q[0] < NPARAM ? q[0] : AUTO_NONE;
                memcpy(p->lane[t][l].v, q + 1, ns < SEQ_STEPS ? ns : SEQ_STEPS);
            }
        }
        p->magic = PAT_MAGIC; p->auto_magic = AUTO_MAGIC;
    }
    return 0;
}

/* ---- the files: 4 bytes of head (byte 0 = 1, as the storage's files always had), then the data */
static uint8_t bmf_buf[ST_HEAD + sizeof(pattern_t) + 4096u];
static int bmf_read(const char *name, uint32_t *len)   /* the file's data in bmf_buf + ST_HEAD */
{
    int r = vibe_store_read(name, bmf_buf, (int)sizeof bmf_buf);
    if (r < (int)ST_HEAD + 8 || bmf_buf[0] != 1) return -1;
    *len = (uint32_t)r - ST_HEAD;
    return 0;
}
static int bmf_write(const char *name, uint32_t len) { memset(bmf_buf, 0, ST_HEAD); bmf_buf[0] = 1; return vibe_store_write(name, bmf_buf, (int)(ST_HEAD + len)); }
static int psnd_file_load(uint32_t n) { uint32_t len; return bmf_read(PS_FILE_N(n), &len) ? -1 : psnd_des(bmf_buf + ST_HEAD, len, &psnd[n % NPAT]); }
static int psnd_file_save(uint32_t n) { return bmf_write(PS_FILE_N(n), psnd_ser(&psnd[n % NPAT], bmf_buf + ST_HEAD)); }
static int pat_file_load(uint32_t n, pattern_t *p) { uint32_t len; return bmf_read(PB_FILE_N(n), &len) ? -1 : pat_des(bmf_buf + ST_HEAD, len, p); }
static int pat_file_save(uint32_t n, const pattern_t *p) { return bmf_write(PB_FILE_N(n), pat_ser(p, bmf_buf + ST_HEAD)); }
static int bmf_legacy(const char *name) { uint32_t len; return !bmf_read(name, &len) && !(bmf_buf[ST_HEAD] == 'B' && bmf_buf[ST_HEAD + 1] == 'M' && bmf_buf[ST_HEAD + 2] == 'F'); }   /* a 0.3 file: to be written anew */
