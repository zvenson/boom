/* SPDX-License-Identifier: GPL-3.0-only */
/* (3.7/3.8, VIBE only) AUDIO TRACKS – the panel's half (the audio flow's: obv_audio.h).
 *
 *   SOUND, last page TYPE: SYNTH / AUDIO. An audio track's SOUND pages: AUDIO (SOURCE, CHANNELS, INPUT, MONITOR), TAKE
 *   (BARS, MODE, REC NEW / DUB, LEVEL), FILTER (OFF / LP / HP / BP, CUTOFF, RESO, DRIVE), TYPE. In the sequencer: TRACKS
 *   (CUTOFF, LEVEL, BARS, MODE; ENTER = SYNTH/AUDIO), STEPS = CUT (FROM, TO, ACTION ERASE / DELETE / CROP, DO turned
 *   twice), TRACK (BARS, MODE, REC, CUTOFF), TRACK+. The source only on the SOUND pages (Sven 9.11.).
 *   REC (the audio track chosen): playing – records from the pattern's next bar, REC again stops; stopped – armed, PLAY
 *   records from bar 1 (COUNT-IN: after it). REC held / SHIFT + REC: the take gone. A pad of row 1 with REC lit: that one.
 *
 * (3.8) Everything here belongs to the PATTERN: paud[n] holds pattern n's audio – which tracks are audio, their
 * settings and their takes ("paud<n>" in the storage, saved with the pattern; AU = the pattern now). A take lives in the
 * store (obv_audio.h), listed in tk[1..NTAKE]: where, how long, mono or stereo, its WAV on the card ("takes2"). Every
 * recording makes a new take (the old one copied in first when it is kept: DUB, a punch, a loop with BARS), so a pattern
 * copied keeps sharing its take until one of them records. A take no pattern names any more is let go (take_gc); when
 * the sequencer stands still, the store is packed (take_pack) so the free room is in one piece.
 *
 * Every take is saved as a WAV into "VIBE FM/boom/" on the card (a new file each time – the host never overwrites)
 * and loaded again at the start. Mono or stereo, 16 bit, at the device's rate. */
static const char *au_src_name(uint32_t src, uint32_t st)   /* "IN 3", "IN 3+4", "RESAMP" (0.29: IN 1..16) */
{
    static char b[4][12]; static uint32_t r;
    char *s = b[r++ & 3u];
    int ii = au_src_in(src);
    if (ii < 0) return "RESAMP";
    if (st) ii &= ~1;
    s[0] = 0; scat(s, "IN "); fmt_u(s + 3, (uint32_t)ii + 1u);
    if (st) { scat(s, "+"); fmt_u(s + slen(s), (uint32_t)ii + 2u); }
    return s;
}
static uint32_t au_src_max(void) { int n = vibe_input_count(); return n < 2 ? 2u : n > VIBE_IO_MAX ? (uint32_t)VIBE_IO_MAX : (uint32_t)n; }   /* the host's inputs */
static const char *const AUM_NAME[NAUM] = {"OFF", "AUTO", "ON"};
static const char *const INM_NAME[3] = {"LINE", "MIC", "HI-Z"};
static void fmt_time(char *s, uint32_t frames);
static void fmt_sec(char *s, uint32_t frames);
static uint32_t au_rec_pat[NAU], au_rec_old[NAU], au_rec_new[NAU], au_input_on;   /* (0.29) each recording track's */
static int au_rec_has(uint32_t k) { for (uint32_t t = 0; t < NAU; t++) if (k && au_rec_new[t] == k) return 1; return 0; }

/* ---- each pattern's audio */
#define AUD_MAGIC 0x36445541u                   /* "AUD6" (BOOM: six tracks) */
typedef struct {
    uint32_t magic;
    uint8_t type, pad[3];                       /* bit t: track t is an audio track */
    uint8_t loop[NAU], st[NAU], src[NAU], mon[NAU], dub[NAU];   /* st: channels for a NEW take (a take keeps its own) */
    uint16_t take[NAU];                         /* 0 = none, else 1..NTAKE */
    uint32_t len[NAU];                          /* the track's length in frames */
    uint8_t flt[NAU], cut[NAU], res[NAU], drv[NAU];   /* (3.8) its filter: AF_*, cutoff, resonance, drive */
    uint32_t lps[NAU], lpe[NAU];                /* (12.11.2026) the loop's piece in frames: from lps to lpe (exclusive; 0: the end) */
    uint8_t tm[NAU], tpad[2];                   /* (0.32, Sven: "Length 0 = Time Mode") 1: no bars – the track is as long as its
                                                 * take, records from the moment REC is pressed to the moment it ends it */
} aud_t;
#define AUD_OLD_SZ __builtin_offsetof(aud_t, lps)         /* (a pattern saved before: its whole take loops) */
static aud_t paud[NPAT];
static uint64_t aud_dirty;                      /* bit n: paud[n] to save */
#define AU (paud[set.pat % NPAT])
static void aud_init(aud_t *a)
{
    memset(a, 0, sizeof *a); a->magic = AUD_MAGIC;
    for (uint32_t t = 0; t < NAU; t++) { a->loop[t] = 1; a->st[t] = 1; a->src[t] = AU_IN1; a->mon[t] = AUM_AUTO; a->cut[t] = 127; }
}
static void aud_touch(void) { aud_dirty |= 1ull << (set.pat % NPAT); store_t = o3_ms; }
static int aud_tm(const aud_t *a, uint32_t t) { t %= NAU; return (a->type >> t & 1u) && a->tm[t]; }   /* (0.32) an audio track in TIME */
static int au_tm(uint32_t t) { return aud_tm(&AU, t); }
/* (BOOM 0.4, bm_save.h) a pattern's audio with a version: "BMFA" 1 · tracks · – · record size, the type bits, then a record
 * per track: loop st src mon dub · take (2) · len (4) · flt cut res drv · lps (4) · lpe (4) · tm (0.32) = 24 bytes */
#define AUD_REC 24u
static void put32(uint8_t *p, uint32_t v);
static uint32_t aud_ser(const aud_t *a, uint8_t *b)
{
    uint32_t at = bmf_head(b, 'A', AUD_REC);
    b[at++] = a->type;
    for (uint32_t t = 0; t < NAU; t++) {
        uint8_t *r = b + at;
        r[0] = a->loop[t]; r[1] = a->st[t]; r[2] = a->src[t]; r[3] = a->mon[t]; r[4] = a->dub[t]; r[5] = (uint8_t)a->take[t]; r[6] = (uint8_t)(a->take[t] >> 8);
        put32(r + 7, a->len[t]); r[11] = a->flt[t]; r[12] = a->cut[t]; r[13] = a->res[t]; r[14] = a->drv[t]; put32(r + 15, a->lps[t]); put32(r + 19, a->lpe[t]); r[23] = a->tm[t];
        at += AUD_REC;
    }
    return at;
}
static uint32_t rd32(const uint8_t *p);
static int aud_des(const uint8_t *b, uint32_t len, aud_t *a)
{
    aud_init(a);
    if (len >= AUD_OLD_SZ && len <= sizeof *a && !memcmp(b, &(uint32_t){AUD_MAGIC}, 4)) { memset(a, 0, sizeof *a); memcpy(a, b, len); return 0; }   /* (0.3, also its shorter one) */
    if (!bmf_is(b, len, 'A') || len < 9u) return -1;
    {
        uint32_t nt = b[5], rs = b[7], at = 9u;
        if (rs < 15u || len < at + nt * rs) return -1;
        a->type = b[8];
        for (uint32_t t = 0; t < nt && t < NAU; t++) {
            const uint8_t *r = b + at + t * rs;
            a->loop[t] = r[0]; a->st[t] = r[1]; a->src[t] = r[2]; a->mon[t] = r[3]; a->dub[t] = r[4]; a->take[t] = (uint16_t)(r[5] | r[6] << 8);
            a->len[t] = rd32(r + 7); a->flt[t] = r[11]; a->cut[t] = r[12]; a->res[t] = r[13]; a->drv[t] = r[14];
            if (rs >= 23u) { a->lps[t] = rd32(r + 15); a->lpe[t] = rd32(r + 19); }
            if (rs >= 24u) a->tm[t] = r[23] ? 1u : 0u;
        }
        if (nt < NAU) a->type &= (uint8_t)((1u << nt) - 1u);
    }
    return 0;
}

/* ---- the takes */
#define NTAKE 64u
#define TK2_MAGIC 0x324B4154u                   /* "TAK2" */
typedef struct { uint32_t off, cap, hi; uint8_t st, inuse, ready, pad; } take_t;   /* off / cap: int16 values in the store; hi: frames */
static take_t tk[NTAKE + 1u];                   /* [0] unused */
static struct { uint32_t magic; struct { char path[240]; uint32_t frames; uint8_t st, pad[3]; } e[NTAKE + 1u]; } takes2;
static uint64_t tk_save_q;                      /* bit k-1: take k's WAV to write */
static uint64_t tk_unsaved;                     /* (0.31) bit k-1: take k lives in memory only (AUTO SAVE off, not saved by name yet) */
static uint64_t ld_q;                           /* bit k-1: take k to load */
static uint32_t tk_ch(uint32_t k) { return tk[k].st ? 2u : 1u; }
static int16_t *tk_ptr(uint32_t k) { return k && k <= NTAKE && tk[k].inuse && tk[k].ready && au_arena ? au_arena + tk[k].off : 0; }
static void takes2_save(void) { takes2.magic = TK2_MAGIC; vibe_store_write("takes2", &takes2, (int)sizeof takes2); }

static uint32_t au_k(uint32_t t) { uint32_t k = AU.take[t % NAU]; return k <= NTAKE && tk[k].inuse ? k : 0u; }   /* the take track t plays */
static uint32_t au_hi(uint32_t t) { uint32_t k = au_k(t); return k ? tk[k].hi : 0u; }
static uint32_t au_stt(uint32_t t) { uint32_t k = au_k(t); return k ? tk[k].st : AU.st[t % NAU]; }   /* its channels (none yet: the setting) */

static int tk_named(uint32_t k)                 /* does any pattern name take k? */
{
    for (uint32_t n = 0; n < NPAT; n++) if (paud[n].magic == AUD_MAGIC) for (uint32_t t = 0; t < NAU; t++) if (paud[n].take[t] == k) return 1;
    return 0;
}
static uint32_t ld_k, sv_k, tk_hold;            /* (below: the take being loaded / saved now; tk_hold: one a WAV goes into) */
static void tk_free(uint32_t k)
{
    if (!k || k > NTAKE) return;
    tk[k].inuse = 0; tk[k].cap = tk[k].hi = 0; tk_save_q &= ~(1ull << (k - 1u)); tk_unsaved &= ~(1ull << (k - 1u));
    takes2.e[k].path[0] = 0; takes2.e[k].frames = 0;
}
static void take_gc(void)                       /* let go of the takes no pattern plays any more */
{
    int changed = 0;
    for (uint32_t k = 1; k <= NTAKE; k++)
        if (tk[k].inuse && !au_rec_has(k) && k != ld_k && k != sv_k && k != tk_hold && !tk_named(k)) { tk_free(k); changed = 1; }
    if (changed) takes2_save();
}
static uint32_t tk_gap(uint32_t *at)            /* the biggest free piece of the store: its start, its size (values) */
{
    uint32_t best = 0, from = 0;
    *at = 0;
    for (;;) {                                  /* (the takes in order of place: the next one after `from`) */
        uint32_t nk = 0, no = AU_ARENA;
        for (uint32_t k = 1; k <= NTAKE; k++) if (tk[k].inuse && tk[k].off >= from && tk[k].off < no) { no = tk[k].off; nk = k; }
        if (no - from > best) { best = no - from; *at = from; }
        if (!nk) break;
        from = tk[nk].off + tk[nk].cap;
    }
    return best;
}
static uint32_t tk_freef(void) { uint32_t at; return tk_gap(&at); }   /* (values free in one piece) */
static int au_busy(void) { return ctl_seq_run || ctl_au_rq || ui_au_state || ld_k || sv_k || tk_hold; }
static void au_apply(void);
static void take_pack(void)                     /* (stopped only: nothing plays, records, loads or saves) every take to the front */
{
    uint32_t from = 0;
    if (au_busy() || !au_arena) return;
    for (;;) {
        uint32_t nk = 0, no = 0xFFFFFFFFu;
        for (uint32_t k = 1; k <= NTAKE; k++) if (tk[k].inuse && tk[k].off >= from && tk[k].off < no) { no = tk[k].off; nk = k; }
        if (!nk) break;
        if (no != from) memmove(au_arena + from, au_arena + no, (size_t)tk[nk].cap * sizeof(int16_t));
        tk[nk].off = from; from += tk[nk].cap;
    }
    au_apply();
}
static uint32_t take_new(uint32_t need, int all, uint32_t st)   /* a new take: `need` values at least (all: the whole free piece;
                                                 * (0.29) all = n > 1: an n-th of it, for n takes at once) */
{
    uint32_t k, at, gap;
    for (k = 1; k <= NTAKE && (tk[k].inuse || (ld_q >> (k - 1u) & 1u)); k++) continue;   /* (a take still to load: kept) */
    if (k > NTAKE) { take_gc(); for (k = 1; k <= NTAKE && (tk[k].inuse || (ld_q >> (k - 1u) & 1u)); k++) continue; if (k > NTAKE) return 0; }
    gap = tk_gap(&at);
    if (gap < need || gap < AU_ARENA / 4u) { take_pack(); gap = tk_gap(&at); }   /* (packed when it may: the room in one piece) */
    if (gap < need || !gap) return 0;
    if (all > 1 && gap / (uint32_t)all >= need) gap /= (uint32_t)all;
    tk[k].off = at; tk[k].cap = all ? gap : need; tk[k].hi = 0; tk[k].st = (uint8_t)(st ? 1 : 0); tk[k].inuse = 1; tk[k].ready = 1;
    takes2.e[k].path[0] = 0;
    return k;
}
static void take_trim(uint32_t k, uint32_t frames)   /* the block down to what it holds */
{
    uint32_t v = frames * tk_ch(k);
    if (v < tk[k].cap) tk[k].cap = v ? v : 1u;
}

/* ---- the audio flow follows the pattern now (au_c), or the next one (au_q) */
static void au_fill(auctl_t *c, const aud_t *a)
{
    memset(c, 0, sizeof *c);
    if (!au_arena || a->magic != AUD_MAGIC) return;
    c->type = a->type & ((1u << NAU) - 1u);
    for (uint32_t t = 0; t < NAU; t++) {
        uint32_t k = a->take[t] <= NTAKE && tk[a->take[t]].inuse ? a->take[t] : 0u;
        c->ptr[t] = tk_ptr(k);
        c->hi[t] = c->ptr[t] ? tk[k].hi : 0u;
        c->capf[t] = c->ptr[t] ? tk[k].cap / tk_ch(k) : 0u;
        c->st[t] = k ? tk[k].st : a->st[t];
        c->len[t] = a->len[t]; c->loop[t] = a->loop[t] ? 1u : 0u;
        c->src[t] = a->src[t] % NAUSRC; c->mon[t] = a->mon[t] % NAUM; c->dub[t] = a->dub[t] ? 1u : 0u;
        c->flt[t] = a->flt[t] % NAF; c->cut[t] = a->cut[t] & 127u; c->res[t] = a->res[t] & 127u; c->drv[t] = a->drv[t] & 127u;
        c->lps[t] = a->lps[t]; c->lpe[t] = a->lpe[t];
        c->tm[t] = (c->type >> t & 1u) && a->tm[t] ? 1u : 0u;
    }
}
static void au_inputs(void)                     /* an audio track takes from IN 1 / IN 2: switch them on (once), their modes */
{
    for (uint32_t t = 0; t < NAU; t++)
        if (((AU.type >> t & 1u) || trk_type(t) == T_SMP) && AU.src[t] % NAUSRC != AU_RES) {   /* (0.31: a SAMPLER records from the input too – before, only an audio track switched it on) */
            if (!au_input_on) { vibe_request(VIBE_REQ_AUDIO_INPUT); au_input_on = 1; }
            vibe_input_mode(0, set.au_inm[0] % 3u); vibe_input_mode(1, set.au_inm[1] % 3u);
            return;
        }
}
static void au_apply(void)                      /* the pattern now to the audio flow */
{
    auctl_t c;
    if (ui_pat_swapped != swap_seen) return;    /* (the next pattern is in already, the panel follows in a moment) */
    au_fill(&c, &AU);
    {
        int lo = vibe_latency_out(), li = vibe_latency_in();
        ctl_au_lat = (lo > 0 ? lo : 0) + (li > 0 ? li : 0);
    }
    au_c = c;
    RING_PUBLISH();
    ctl_au_type = c.type;
    au_inputs();
}
static void au_queue(uint32_t n) { au_fill(&au_q, &paud[n % NPAT]); RING_PUBLISH(); ctl_au_swap = 1; }   /* (o3_seq.h, pat_go) */
static void cut_from_reset(void);
static void au_pattern(void) { if (!ctl_seq_run) ctl_au_swap = 0; cut_from_reset(); au_apply(); }   /* (o3_seq.h) the pattern changed: stopped, or the panel followed the swap */
static void au_pat_copied(uint32_t from, uint32_t to) { paud[to % NPAT] = paud[from % NPAT]; aud_dirty |= 1ull << (to % NPAT); }

/* ---- saving a take: a WAV, written a piece at a time from the draw (the card is never touched from the audio) */
static struct { int h; uint32_t off, total; uint8_t hdr[44]; char path[VIBE_MEDIA_PATH]; } sv;
static char tk_name_over[NTAKE + 1u][32];       /* (3.9) a take's WAV name asked for by SAVE AUDIO / PATTERN (o3_files.h), else "OberVave P<n> <T>.wav" */
static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static void put16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void tk_save_cancel(uint32_t k) { if (sv_k == k && sv.h) { vibe_media_close(sv.h, 0); sv.h = 0; sv_k = 0; } if (k) tk_save_q &= ~(1ull << (k - 1u)); }
static void au_save_poll(void)
{
    if (!sv_k) {
        uint32_t k;
        for (k = 1; k <= NTAKE && !(tk_save_q >> (k - 1u) & 1u); k++) ;
        if (k > NTAKE || au_rec_has(k)) return;   /* (recording into it: after) */
        tk_save_q &= ~(1ull << (k - 1u));
        if (!tk[k].inuse || !tk[k].hi) return;
        if (!tk_name_over[k][0] && !set.au_autosave) { tk_unsaved |= 1ull << (k - 1u); return; }   /* (0.31, Sven: "nix auto save") in memory until named */
        tk_unsaved &= ~(1ull << (k - 1u));
        char name[32] = "BOOM P"; uint32_t pn = 0, tr = 0;
        for (uint32_t n = 0; n < NPAT; n++) for (uint32_t t = 0; t < NAU; t++) if (paud[n].take[t] == k) { pn = n; tr = t; n = NPAT; break; }
        fmt_u(name + slen(name), pn + 1u); { char l[3] = {' ', (char)('A' + tr), 0}; scat(name, l); } scat(name, ".wav");
        if (tk_name_over[k][0]) { name[0] = 0; scat(name, tk_name_over[k]); tk_name_over[k][0] = 0; }   /* (3.9: SAVE AUDIO / PATTERN named it) */
        int h = vibe_media_create(name, sv.path, (int)sizeof sv.path);
        if (h == VIBE_MEDIA_PENDING) { tk_save_q |= 1ull << (k - 1u); return; }   /* (the host is not ready yet: next frame) */
        if (h <= 0) { say("NO CARD: THE TAKE STAYS IN MEMORY ONLY"); return; }
        uint32_t ch = tk_ch(k), data = tk[k].hi * ch * 2u;
        memcpy(sv.hdr, "RIFF", 4); put32(sv.hdr + 4, 36u + data); memcpy(sv.hdr + 8, "WAVEfmt ", 8);
        put32(sv.hdr + 16, 16u); put16(sv.hdr + 20, 1u); put16(sv.hdr + 22, ch); put32(sv.hdr + 24, obv_fs);
        put32(sv.hdr + 28, obv_fs * ch * 2u); put16(sv.hdr + 32, ch * 2u); put16(sv.hdr + 34, 16u);
        memcpy(sv.hdr + 36, "data", 4); put32(sv.hdr + 40, data);
        sv.h = h; sv_k = k; sv.off = 0; sv.total = 44u + data;
    }
    for (int k = 0; k < 16 && sv.off < sv.total; k++) {   /* (up to 1 MB a frame; a slow card says PENDING) */
        const uint8_t *p; uint32_t nb;
        if (sv.off < 44u) { p = sv.hdr + sv.off; nb = 44u - sv.off; }
        else { p = (const uint8_t *)(au_arena + tk[sv_k].off) + (sv.off - 44u); nb = sv.total - sv.off; if (nb > 65536u) nb = 65536u; }
        int r = vibe_media_append(sv.h, p, (int)nb);
        if (r == VIBE_MEDIA_PENDING) return;
        if (r < 0) { vibe_media_close(sv.h, 0); sv.h = 0; sv_k = 0; say("THE TAKE COULD NOT BE SAVED (CARD FULL?)"); return; }
        sv.off += nb;
    }
    if (sv.off >= sv.total) {
        vibe_media_close(sv.h, 1); sv.h = 0;
        { uint32_t n = slen(sv.path); if (n > sizeof takes2.e[0].path - 1u) n = 0; memcpy(takes2.e[sv_k].path, sv.path, n); takes2.e[sv_k].path[n] = 0; }
        takes2.e[sv_k].frames = tk[sv_k].hi; takes2.e[sv_k].st = tk[sv_k].st;
        takes2_save();
        sv_k = 0;
        say("TAKE SAVED ON THE CARD");
    }
}

/* ---- loading the takes the patterns name (the start, a song loaded): the head first – how long – then its block */
static struct { uint32_t on, phase, ch, data_at, bytes, off; } ld;
static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void au_load_done(uint32_t ok)
{
    uint32_t k = ld_k;
    ld.on = 0; ld_k = 0;
    if (!ok) { tk_free(k); say("A TAKE IS NOT ON THE CARD ANY MORE"); for (uint32_t n = 0; n < NPAT; n++) for (uint32_t t = 0; t < NAU; t++) if (paud[n].take[t] == k) paud[n].take[t] = 0; }
    else tk[k].ready = 1;
    au_apply();
}
static void au_load_poll(void)
{
    static uint8_t h[1024];
    if (!ld.on) {
        uint32_t k;
        for (k = 1; k <= NTAKE && !(ld_q >> (k - 1u) & 1u); k++) ;
        if (k > NTAKE) return;
        ld_q &= ~(1ull << (k - 1u));
        ld_k = k; ld.on = 1; ld.phase = 0;
    }
    const char *path = takes2.e[ld_k].path;
    if (ld.phase == 0) {                        /* the head: fmt (16 bit PCM, 1 or 2 channels) and where data is */
        int r = vibe_media_read(path, 0, h, (int)sizeof h);
        if (r == VIBE_MEDIA_PENDING) return;
        if (r < 44 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) { au_load_done(0); return; }
        uint32_t at = 12, ch = 0, bits = 0;
        ld.data_at = 0;
        while (at + 8u <= (uint32_t)r) {
            uint32_t sz = rd32(h + at + 4);
            if (!memcmp(h + at, "fmt ", 4) && at + 24u <= (uint32_t)r) { ch = h[at + 10] | (uint32_t)h[at + 11] << 8; bits = h[at + 22] | (uint32_t)h[at + 23] << 8; }
            else if (!memcmp(h + at, "data", 4)) { ld.data_at = at + 8u; ld.bytes = sz; break; }
            at += 8u + sz + (sz & 1u);
        }
        if ((ch != 1u && ch != 2u) || bits != 16u || !ld.data_at) { au_load_done(0); return; }
        {                                       /* (its block, as long as the take) */
            uint32_t at2, v = ld.bytes / 2u, gap;
            tk[ld_k].inuse = 0;                  /* (it is not anywhere yet) */
            gap = tk_gap(&at2);
            if (gap < v) { if (!au_busy()) { take_pack(); gap = tk_gap(&at2); } }
            if (gap < v) { say("NOT ENOUGH MEMORY FOR A TAKE"); au_load_done(0); return; }
            tk[ld_k].off = at2; tk[ld_k].cap = v ? v : 1u; tk[ld_k].st = ch == 2u; tk[ld_k].inuse = 1; tk[ld_k].ready = 0;
        }
        ld.ch = ch; ld.off = 0; ld.phase = 1;
    }
    for (int k = 0; k < 16 && ld.off < ld.bytes; k++) {
        uint32_t nb = ld.bytes - ld.off; if (nb > 65536u) nb = 65536u;
        int r = vibe_media_read(path, (uint64_t)ld.data_at + ld.off, (uint8_t *)(au_arena + tk[ld_k].off) + ld.off, (int)nb);
        if (r == VIBE_MEDIA_PENDING) return;
        if (r <= 0) { ld.bytes = ld.off; break; }   /* (shorter than it said: what is there) */
        ld.off += (uint32_t)r;
    }
    if (ld.off < ld.bytes) return;              /* (on in the next frame) */
    tk[ld_k].hi = ld.off / (ld.ch * 2u);
    au_load_done(1);
}

/* ---- the start (and a song loaded): the patterns' audio, the takes they name; 3.7's settings carried over */
static void au_boot(void)
{
    char src[256];
    if (!au_arena && !au_mem()) vibe_log("BOOM: no memory for the audio tracks – they stay off");
    memset(tk, 0, sizeof tk); ld_q = tk_save_q = 0; ld.on = 0; ld_k = sv_k = 0; aud_dirty = 0;
    for (uint32_t n = 0; n < NPAT; n++) {       /* (BOOM 0.4: versioned, aud_des – a 0.3 file is the raw struct) */
        static uint8_t ab[sizeof(aud_t) + 64u];
        int r = vibe_store_read(pat_fname("paud", n), ab, (int)sizeof ab);
        if (r <= 0 || aud_des(ab, (uint32_t)r, &paud[n])) aud_init(&paud[n]);
        else if (!(ab[0] == 'B' && ab[1] == 'M' && ab[2] == 'F')) aud_dirty |= 1ull << n;
    }
    if (vibe_store_read("takes2", &takes2, (int)sizeof takes2) < 8 || takes2.magic != TK2_MAGIC) {
        static struct { uint32_t magic; char path[NAU][VIBE_MEDIA_PATH]; } old;   /* 3.7: one take a track, all patterns */
        memset(&takes2, 0, sizeof takes2);
        if (vibe_store_read("takes", &old, (int)sizeof old) >= 8 && old.magic == 0x314B4154u && set.au_ok == AU_OK) {
            aud_t *a = &paud[set.pat % NPAT];
            a->type = set.au_type;
            for (uint32_t t = 0; t < NAU; t++) {
                a->loop[t] = set.au_loop[t]; a->st[t] = set.au_st[t]; a->src[t] = set.au_src[t]; a->mon[t] = set.au_mon[t];
                a->dub[t] = set.au_dub[t]; a->len[t] = set.au_len[t];
                if (old.path[t][0] && slen(old.path[t]) < sizeof takes2.e[0].path) { memcpy(takes2.e[t + 1u].path, old.path[t], slen(old.path[t]) + 1u); a->take[t] = (uint16_t)(t + 1u); }
            }
            aud_dirty |= 1ull << (set.pat % NPAT); takes2_save();
        }
    }
    vibe_media_sources(src, (int)sizeof src);  /* (asks the host for its sources: the card) */
    if (au_arena) for (uint32_t k = 1; k <= NTAKE; k++)
        if (takes2.e[k].path[0] && tk_named(k)) { tk[k].inuse = 1; tk[k].ready = 0; tk[k].cap = 0; tk[k].off = AU_ARENA; ld_q |= 1ull << (k - 1u); }
    for (uint32_t k = 1; k <= NTAKE; k++) if (tk[k].inuse) tk[k].inuse = 0;   /* (placed when loaded: live again then) */
    au_apply();
}
static void aud_poll(void)                      /* the patterns' audio into the storage, once it has been quiet a while */
{
    if (!aud_dirty || (!store_force && o3_ms - store_t < 1500u)) return;
    for (uint32_t n = 0; n < NPAT; n++) if (aud_dirty >> n & 1u) { static uint8_t ab[sizeof(aud_t) + 64u]; vibe_store_write(pat_fname("paud", n), ab, (int)aud_ser(&paud[n], ab)); }
    aud_dirty = 0;
}

/* ---- recording */
static uint32_t au_bar(void) { uint32_t b = song_bar_frames(quarter_len()); return b ? b : 1u; }   /* (the song's bar: track A's meter) */
static uint32_t au_up(uint32_t f) { uint32_t b = au_bar(); return (f + b - 1u) / b * b; }
static int au_rec_one(uint32_t t, uint32_t share)   /* track t: its new take (share: how many get the free room) */
{
    uint32_t old, k, keep;
    old = au_k(t);
    keep = old && (AU.dub[t] || !AU.loop[t] || (AU.len[t] && !au_tm(t)));   /* (DUB, a punch into a tape, a loop with its length: the old one in;
                                                 * TIME, NEW: a new take, as long as REC holds it) */
    k = take_new(keep ? tk[old].hi * tk_ch(old) : 1u, (int)share, old ? tk[old].st : AU.st[t]);
    if (!k) return 0;
    if (keep) { memcpy(au_arena + tk[k].off, au_arena + tk[old].off, (size_t)tk[old].hi * tk_ch(old) * sizeof(int16_t)); tk[k].hi = tk[old].hi; }
    if (old) tk_save_cancel(old);
    au_rec_old[t] = old; au_rec_new[t] = k; au_rec_pat[t] = set.pat;
    AU.take[t] = (uint16_t)k;                   /* (it plays from the new block: the same sound, until it records) */
    return 1;
}
static void say_mask(uint32_t m, const char *what)   /* "A C D RECORD FROM BAR 1" */
{
    static char b[48];
    b[0] = 0;
    for (uint32_t t = 0; t < NAU; t++) if (m >> t & 1u) { char l[3] = {(char)('A' + t), ' ', 0}; scat(b, l); }
    scat(b, what); say(b);
}
static void au_rec_start_m(uint32_t m)          /* (0.29) the tracks in m record at once, from the same frame */
{
    uint32_t n = 0, got = 0, ng = 0;
    m &= (1u << NAU) - 1u;
    for (uint32_t t = 0; t < NAU; t++) if ((m >> t & 1u) && !(AU.type >> t & 1u) && !(trk_type(t) == T_SMP && m == 1u << t)) m &= ~(1u << t);   /* (0.20) a SAMPLER records its own sample – alone */
    if (!m) return;
    if (ctl_au_rq || ui_au_state) { say("A TAKE IS RUNNING: REC STOPS IT"); return; }
    if (ld_k || ld_q) { say("THE TAKES ARE STILL LOADING"); return; }
    for (uint32_t t = 0; t < NAU; t++) n += m >> t & 1u;
    for (uint32_t t = 0; t < NAU; t++) if (m >> t & 1u) {
        if (!au_rec_one(t, n - ng)) {          /* the room ran out: none of them records */
            for (uint32_t u = 0; u < t; u++) if (got >> u & 1u) { uint32_t k = au_rec_new[u]; AU.take[u] = (uint16_t)au_rec_old[u]; au_rec_new[u] = 0; tk_free(k); }
            au_apply(); say("THE MEMORY FOR TAKES IS FULL"); return;
        }
        got |= 1u << t; ng++;
    }
    au_arm = 0;
    au_apply();
    for (uint32_t t = 0; t < NAU; t++) if ((m >> t & 1u) && !((ctl_trk_on ^ trk_pending()) >> t & 1u)) ctl_trk_req ^= 1u << t;   /* (the tracks play from then) */
    RING_PUBLISH();
    ctl_au_rq = m;
    if (!ctl_seq_run) {                         /* stopped: from bar 1 (COUNT-IN: a bar first) */
        ctl_count = set.click % NCLICK == CLICK_COUNT;
        ctl_seq_restart = 1; ctl_seq_run = 1;
        if (set.midi_out || midi_trk_any()) vibe_transport_out(1);
        say_mask(m, n > 1u ? "RECORD FROM BAR 1" : "RECORDS FROM BAR 1");
    } else if (n == 1u && au_tm(sel) && m == 1u << sel) say_mask(m, "RECORDS NOW (TIME) – REC ENDS IT");
    else say_mask(m, n > 1u ? "RECORD ON THE NEXT BAR" : "RECORDS ON THE NEXT BAR");
}
static void au_rec_start(uint32_t t) { au_rec_start_m(1u << (t % NAU)); }
static void au_rec_stop(void) { au_arm = 0; ctl_au_rq = 0; }
static uint32_t au_rec_set(void)                /* what REC records now: the armed tracks, else the chosen one */
{
    uint32_t m = au_armed();
    if (trk_type(sel) == T_SMP && !(AU.type >> sel & 1u) && smp_page()) return 1u << sel;   /* (a SAMPLER on its page: its sample) */
    return m ? m : (AU.type >> sel & 1u) ? 1u << sel : 0u;
}
static void au_rec_press(void)                  /* REC tapped: the armed audio tracks (none armed: the chosen one) */
{
    uint32_t m;
    if (ctl_au_rq) { au_rec_stop(); return; }
    if (au_arm) { au_arm = 0; say("REC OFF"); return; }
    m = au_rec_set();
    if (!m) return;
    if (m == 1u << sel && trk_type(sel) == T_SMP && !(AU.type >> sel & 1u)) {   /* (0.20) a sample: free length, REC again ends it */
        AU.loop[sel] = 1; AU.len[sel] = 0; AU.dub[sel] = 0;
        { int ii = au_src_in(AU.src[sel % NAU]), ni = vibe_input_count(); if (ni < 2) ni = 2;
          if (ii >= ni) { AU.src[sel % NAU] = AU_IN1; AU.st[sel % NAU] = 0; aud_touch(); au_apply(); say("THAT INPUT IS NOT THERE – RECORDING IN 1"); } }   /* (0.31: an IN 3 from the XR18 days on the VIBE alone: silence) */
        au_inputs();                            /* (0.31, Sven: "live sampeln geht nicht") the input on, and it records NOW – stopped, the */
        au_rec_start_m(m); return;              /*  transport starts from bar 1 with it (before: REC only armed, a second REC disarmed it) */
    }
    if (ctl_seq_run) au_rec_start_m(m);
    else { au_arm = m; say_mask(m, "ARMED: PLAY RECORDS FROM BAR 1"); }
}
static void au_arm_toggle(uint32_t t)           /* (0.29) REC held + the track's knob push (X1-X4), ENTER (the chosen one) or pad: ARM on / off */
{
    t %= NAU;
    if (!(AU.type >> t & 1u)) { say_trk(": ONLY AN AUDIO TRACK CAN BE ARMED (SOUND ▸ TYPE: AUDIO)", t); return; }
    if (ctl_au_rq >> t & 1u) { say_trk(" IS RECORDING – REC STOPS IT FIRST", t); return; }
    au_armm ^= 1u << t;
    if (au_arm) au_arm = au_armed();            /* (armed for PLAY already: the new set) */
    {
        static char b[48];
        b[0] = 0; scat(b, au_armm >> t & 1u ? " ARMED · " : " DISARMED · ");
        { uint32_t m = au_armed(), n = 0; for (uint32_t u = 0; u < NAU; u++) n += m >> u & 1u; fmt_u(b + slen(b), n); scat(b, n == 1u ? " TRACK ON REC" : " TRACKS ON REC"); }
        say_trk(b, t);
    }
}
static void smp_after_take(uint32_t t);         /* (bm_sampler.h) */
static void au_take_done(uint32_t t)            /* (the draw) the audio flow ended track t's take: its length, its block, saved */
{
    static char msg[40];
    aud_t *a = &paud[au_rec_pat[t] % NPAT];
    uint32_t k = au_rec_new[t], end = ui_au_end[t], len = a->len[t];
    int32_t at;
    RING_TAKE();
    at = ui_au_at[t];
    __atomic_fetch_and(&ctl_au_rq, ~(1u << t), __ATOMIC_RELEASE);
    au_rec_new[t] = 0;
    if (at < 0 || !end || !k) {                 /* nothing came: the old take again */
        a->take[t] = (uint16_t)au_rec_old[t]; if (k) tk_free(k);
        au_apply(); say_trk(at < 0 ? ": NOT RECORDED" : ": NOTHING CAME IN", t);
        return;
    }
    if (!a->loop[t]) len = au_up(end) > len ? au_up(end) : len;   /* ONCE: at least to the bar the take ended in */
    else if (!len) len = au_up(end);            /* a new loop: whole bars */
    if (end > tk[k].hi) tk[k].hi = end;
    take_trim(k, tk[k].hi);
    if (aud_tm(a, t)) {                         /* (0.32) TIME: exactly as long as the take, the loop's piece all of it again */
        len = tk[k].hi;
        if (!(a->dub[t] && au_rec_old[t])) { a->lps[t] = 0; a->lpe[t] = 0; }
    }
    a->len[t] = len; aud_dirty |= 1ull << (au_rec_pat[t] % NPAT); store_t = o3_ms;
    tk_save_q |= 1ull << (k - 1u);
    take_gc(); au_apply();
    msg[0] = 0; scat(msg, "TRACK "); { char l[2] = {(char)('A' + t), 0}; scat(msg, l); }
    if (!(a->type >> t & 1u)) { scat(msg, ": SAMPLE · "); fmt_time(msg + slen(msg), tk[k].hi); if (!set.au_autosave) scat(msg, " · NOT SAVED"); say(msg); smp_after_take(t); return; }   /* (0.20) a sampler's own sample – (0.31) its threshold */
    scat(msg, " RECORDED · ");
    if (aud_tm(a, t)) { fmt_sec(msg + slen(msg), len); scat(msg, " (TIME)"); }
    else { fmt_u(msg + slen(msg), (len + au_bar() / 2u) / au_bar()); scat(msg, " BARS"); }
    say(msg);
}
static void au_clear(uint32_t t)                /* REC held / SHIFT + REC: the take gone (from this pattern) */
{
    t %= NAU;
    if (ctl_au_rq >> t & 1u) au_rec_stop();
    AU.take[t] = 0; AU.len[t] = 0; aud_touch();
    take_gc(); au_apply();
    say_trk(" CLEARED", t);
}
static void au_poll(void)                       /* (the draw) */
{
    { uint32_t d = __atomic_exchange_n(&ui_au_done, 0u, __ATOMIC_ACQUIRE); for (uint32_t t = 0; t < NAU; t++) if (d >> t & 1u) au_take_done(t); }
    au_load_poll();
    au_save_poll();
    aud_poll();
}
static void au_type_toggle(uint32_t t)          /* SYNTH <-> AUDIO (TYPE, or ENTER in TRACKS) – this pattern's track; its take stays */
{
    uint32_t on;
    t %= NAU; on = !(AU.type >> t & 1u);
    if (on && !au_arena) { say("NO MEMORY FOR AUDIO TRACKS HERE"); return; }
    if (!on && (ctl_au_rq >> t & 1u)) au_rec_stop();
    if (!on) au_armm &= ~(1u << t);
    AU.type = (uint8_t)(on ? AU.type | 1u << t : AU.type & ~(1u << t));
    if (on) ctl_panic = 1;                      /* (its synth notes end) */
    aud_touch(); au_apply();
    say_trk(on ? " IS AN AUDIO TRACK HERE" : " PLAYS ITS SOUND HERE", t);
}

/* ---- knobs: TRACK+ (TRACK view + SHIFT, any track) and the audio track's own */
static int au_plus(void) { return seq_view == SV_TRACK && seq_shift; }
static int au_here(void) { return (AU.type >> sel & 1u) && seq_view != SV_SLOTS; }
static void fmt_time(char *s, uint32_t frames)  /* m:ss */
{
    uint32_t sec = frames / (obv_fs ? obv_fs : 48000u);
    char b[4];
    fmt_u(s, sec / 60u); scat(s, ":"); b[0] = (char)('0' + sec % 60u / 10u); b[1] = (char)('0' + sec % 10u); b[2] = 0; scat(s, b);
}
static void fmt_sec(char *s, uint32_t frames)   /* (0.32 TIME) seconds and ms: 3.042s */
{
    uint32_t fs = obv_fs ? obv_fs : 48000u, ms = (uint32_t)((uint64_t)frames * 1000u / fs);
    char b[5];
    s[0] = 0; fmt_u(s, ms / 1000u); b[0] = '.'; b[1] = (char)('0' + ms / 100u % 10u); b[2] = (char)('0' + ms / 10u % 10u); b[3] = (char)('0' + ms % 10u); b[4] = 0;
    scat(s, b); scat(s, "s");
}
static void fmt_pos(char *s, uint32_t f);
static void fmt_tpos(uint32_t t, char *s, uint32_t f) { if (au_tm(t)) fmt_sec(s, f); else fmt_pos(s, f); }   /* (TIME: no bars to count in) */
enum { AK_TYPE, AK_CH, AK_INM, AK_MON, AK_SRC, AK_LVL, AK_BARS, AK_MODE, AK_DUB, AK_FROM, AK_TO, AK_ACT, AK_DO, AK_FLT, AK_CUT, AK_RES, AK_DRV, AK_GAIN, AK_LS, AK_LE, AK_PLAYS, AK_OUT, AK_MIN, AK_NONE };   /* (0.24: MIN = MIDI IN) */   /* (BOOM 0.4: PLAYS seq / DAW, OUT main / its own) */
static const char *const AF_NAME[NAF] = {"OFF", "LP", "HP", "BP"};
/* (3.7, Sven 9.11.) the SOUND key's pages: a synth track has its seven and TYPE last; an audio track has AUDIO, TAKE,
 * TYPE instead – no oscillators any more. ENV, LFO and ARP show AUDIO for an audio track. au_lay: which one is drawn */
enum { SX_NONE, SX_AUDIO, SX_TAKE, SX_FILTER, SX_TYPE };
static uint32_t snd_x;                          /* the SOUND page past the table's: SX_* */
static int au_lay = -1;                         /* >= 0: au_knobs / au_turn work for that SX_* page */
static void au_layout(uint8_t *a)
{
    /* (3.8, Sven: the source is chosen on the SOUND pages only – the sequencer has the filter there) */
    static const uint8_t PLUS[4] = {AK_TYPE, AK_CH, AK_GAIN, AK_MON}, TRK[4] = {AK_CUT, AK_LVL, AK_BARS, AK_MODE},
                         ED[4] = {AK_BARS, AK_MODE, AK_LS, AK_LE}, CUT[4] = {AK_FROM, AK_TO, AK_ACT, AK_DO},
                         AUD[4] = {AK_SRC, AK_CH, AK_GAIN, AK_MON}, TAKE[4] = {AK_BARS, AK_MODE, AK_DUB, AK_INM},
                         FLT[4] = {AK_FLT, AK_CUT, AK_RES, AK_DRV}, TYP[4] = {AK_TYPE, AK_PLAYS, AK_OUT, AK_MIN};
    if (au_lay >= 0) { memcpy(a, au_lay == SX_AUDIO ? AUD : au_lay == SX_TAKE ? TAKE : au_lay == SX_FILTER ? FLT : TYP, 4); return; }
    memcpy(a, au_plus() ? PLUS : seq_view == SV_TRACKS ? TRK : SV_ENTRY(seq_view) ? CUT : ED, 4);
}

/* ---- CUT: a piece of the take – ERASE (silent), DELETE (out, the rest moves up), CROP (only it stays).
 * (3.8, Sven: "freier wählbar") FROM / TO in frames: a detent = a sixteenth, a fast turn = a bar, SHIFT held = 1 ms */
enum { CUT_ERASE, CUT_DELETE, CUT_CROP, NCUT };
static const char *const CUT_NAME[NCUT] = {"ERASE", "DELETE", "CROP"};
static uint32_t cut_from[NAU], cut_to[NAU], cut_act, cut_arm, cut_arm_t;   /* from / to: frames; cut_arm: t + 1 (turned once) */
static uint32_t au_span(uint32_t t);
static void cut_from_reset(void) { memset(cut_from, 0, sizeof cut_from); memset(cut_to, 0, sizeof cut_to); cut_arm = 0; }   /* (another pattern: the whole take) */
static void fmt_pos(char *s, uint32_t f)        /* bar.beat.sixteenth, and + ms when it is between sixteenths */
{
    uint32_t q = quarter_len(), six = q / 4u ? q / 4u : 1u, n = f / six, rest = f - n * six;
    uint32_t bar = song_e8() * 2u, r = n % bar, per = METER[meter_of(0)].den8 ? 2u : 4u;   /* (12.11.2026: the song's meter) */
    s[0] = 0; fmt_u(s, n / bar + 1u); scat(s, "."); fmt_u(s + slen(s), r / per + 1u); scat(s, "."); fmt_u(s + slen(s), r % per + 1u);
    if (rest) { scat(s, "+"); fmt_u(s + slen(s), (uint32_t)((uint64_t)rest * 1000u / (obv_fs ? obv_fs : 48000u))); }
}
static void au_fade(uint32_t t, uint32_t at, int in)   /* ~2 ms ramp at frame `at`: into it (in) or out of it (the frames before) */
{
    uint32_t k = au_k(t), ch = k ? tk_ch(k) : 1u, n = obv_fs / 500u, hi = au_hi(t);
    int16_t *pl = k ? au_arena + tk[k].off : 0;
    if (!pl) return;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t f = in ? at + i : at - n + i;
        if ((!in && at < n) || f >= hi) continue;
        int32_t g = (int32_t)(in ? i : n - i) * 256 / (int32_t)n;
        for (uint32_t c = 0; c < ch; c++) pl[f * ch + c] = (int16_t)(pl[f * ch + c] * g / 256);
    }
}
static void au_cut_do(uint32_t t)
{
    static char msg[48];
    uint32_t k = au_k(t), ch, hi, len = AU.len[t], f0 = cut_from[t], f1 = cut_to[t];
    int16_t *pl;
    if (!k) { say("NOTHING THERE TO CUT"); return; }
    ch = tk_ch(k); hi = tk[k].hi; pl = au_arena + tk[k].off;
    if (f1 > hi) f1 = hi;
    if (ctl_au_rq >> t & 1u) { say("NOT WHILE IT RECORDS"); return; }
    if (f0 >= f1) { say("NOTHING THERE TO CUT"); return; }
    tk_save_cancel(k);
    if (cut_act == CUT_ERASE) { memset(pl + f0 * ch, 0, (size_t)(f1 - f0) * ch * sizeof(int16_t)); au_fade(t, f0, 0); au_fade(t, f1, 1); }
    else if (cut_act == CUT_DELETE) {
        uint32_t d = f1 - f0;
        au_fade(t, f0, 0);
        memmove(pl + f0 * ch, pl + f1 * ch, (size_t)(hi - f1) * ch * sizeof(int16_t));
        tk[k].hi = hi - d;
        au_fade(t, f0, 1);
        if (len > f0) AU.len[t] = len - (len - f0 < d ? len - f0 : d);
    } else {                                    /* CROP: the piece to the start */
        uint32_t d = f1 - f0;
        memmove(pl, pl + f0 * ch, (size_t)d * ch * sizeof(int16_t));
        tk[k].hi = d;
        au_fade(t, 0, 1); au_fade(t, d, 0);
        AU.len[t] = d; cut_from[t] = 0; cut_to[t] = d;
    }
    aud_touch(); au_apply(); tk_save_q |= 1ull << (k - 1u);
    msg[0] = 0; scat(msg, CUT_NAME[cut_act % NCUT]); scat(msg, " "); fmt_tpos(t, msg + slen(msg), cut_from[t]); scat(msg, " - ");
    fmt_tpos(t, msg + slen(msg), cut_to[t]); scat(msg, ": DONE"); say(msg);
}
static uint32_t au_lp_e(uint32_t t) { uint32_t n = AU.len[t], e = AU.lpe[t]; return !e || e > n ? n : e; }   /* the loop's end (frames, exclusive) */
static uint32_t au_lp_s(uint32_t t) { uint32_t e = au_lp_e(t); return AU.lps[t] < e ? AU.lps[t] : (e ? e - 1u : 0u); }
static int32_t au_smp(const int16_t *pl, uint32_t ch, uint32_t f) { return ch == 2u ? pl[2u * f] + pl[2u * f + 1u] : pl[f]; }
static uint32_t au_zc(uint32_t t, uint32_t f, int dir, uint32_t lo, uint32_t hi_lim)   /* the next rising zero crossing from f
                                                 * in the direction turned (up to 25 ms), inside lo..hi_lim; none: f */
{
    uint32_t k = au_k(t), ch = k ? tk_ch(k) : 1u, hi = au_hi(t), n = obv_fs / 40u;
    const int16_t *pl = k ? au_arena + tk[k].off : 0;
    if (!pl) return f;
    for (uint32_t i = 0; i <= n; i++) {
        int64_t g = dir > 0 ? (int64_t)f + i : (int64_t)f - i;
        if (g < 1 || g < (int64_t)lo || g > (int64_t)hi_lim || g >= (int64_t)hi) break;
        if (au_smp(pl, ch, (uint32_t)g - 1u) < 0 && au_smp(pl, ch, (uint32_t)g) >= 0) return (uint32_t)g;
    }
    return f;
}
static int au_knobs(knob_t *k)
{
    static const char *const L[AK_NONE + 1] = {"TYPE", "CHANNELS", "INPUT", "MONITOR", "SOURCE", "LEVEL", "BARS", "MODE",
                                            "REC", "FROM", "TO", "ACTION", "DO", "FILTER", "CUTOFF", "RESO", "DRIVE", "GAIN", "START", "END", "PLAYS", "OUT", "MIDI IN", 0};
    uint8_t a[4];
    uint32_t t = sel % NAU, sp;
    if (au_lay < 0 && !au_plus() && !au_here()) return 0;
    au_layout(a);
    sp = au_span(t);
    if (a[0] == AK_FROM && (!cut_to[t] || cut_to[t] > sp)) cut_to[t] = sp;   /* (CUT at first: the whole take) */
    if (cut_from[t] >= cut_to[t]) cut_from[t] = 0;
    for (int i = 0; i < 4; i++) {
        char *v = k[i].value; v[0] = 0; k[i].bip = 0; k[i].v = 0; k[i].label = L[a[i]];
        switch (a[i]) {
        case AK_TYPE: scat(v, AU.type >> t & 1u ? "AUDIO" : TYPE_NAME[trk_type(t)]); k[i].v = AU.type >> t & 1u ? 1.0f : (float)(type_pos(trk_type(t)) < 0 ? 2 : type_pos(trk_type(t))) / (float)NTYPE_ORDER; break;
        case AK_CH: scat(v, au_stt(t) ? "STEREO" : "MONO"); k[i].v = au_stt(t) ? 1.0f : 0.0f; break;
        case AK_INM: if (au_src_in(AU.src[t]) < 0 || au_src_in(AU.src[t]) > 1) scat(v, "--");   /* (0.29: an interface's IN 3.. has its own preamps) */
                     else { uint32_t n = AU.src[t] == AU_IN2 ? 1u : 0u; scat(v, INM_NAME[set.au_inm[n] % 3u]); k[i].v = (float)(set.au_inm[n] % 3u) / 2.0f; }
                     break;
        case AK_MON: scat(v, AUM_NAME[AU.mon[t] % NAUM]); k[i].v = (float)(AU.mon[t] % NAUM) / 2.0f; break;
        case AK_GAIN: if (au_src_in(AU.src[t]) < 0 || au_src_in(AU.src[t]) > 1) scat(v, "--");
                      else { uint32_t n = AU.src[t] == AU_IN2 ? 1u : 0u, g = set.au_gain[n] > 40u ? 40u : set.au_gain[n];
                             scat(v, "+"); fmt_u(v + slen(v), g); scat(v, "dB"); k[i].v = (float)g / 40.0f; }
                      break;
        case AK_SRC: { int ii = au_src_in(AU.src[t]); uint32_t mx = au_src_max();   /* (0.29) IN 1 .. IN n, RESAMP last */
                       scat(v, ii < 0 ? "RESMP" : au_src_name(AU.src[t], au_stt(t))); if (ii >= (int)mx) scat(v, "?");
                       k[i].v = ii < 0 ? 1.0f : (float)ii / (float)mx; break; }
        case AK_LVL: fmt_u(v, set.mix_lvl[t]); k[i].v = set.mix_lvl[t] / 127.0f; break;
        case AK_BARS: {
            uint32_t len = AU.len[t], b = au_bar();
            if (au_tm(t)) scat(v, "TIME"); else if (!len) scat(v, "FREE"); else fmt_u(v, (len + b / 2u) / b);
            k[i].v = len && !au_tm(t) ? (float)len / (float)(b * 64u) : 0.0f;
            break; }
        case AK_DUB: scat(v, AU.dub[t] ? "DUB" : "NEW"); k[i].v = AU.dub[t] ? 1.0f : 0.0f; break;
        case AK_FROM: fmt_tpos(t, v, cut_from[t]); k[i].v = sp ? (float)cut_from[t] / (float)sp : 0.0f; break;
        case AK_TO: fmt_tpos(t, v, cut_to[t]); k[i].v = sp ? (float)cut_to[t] / (float)sp : 0.0f; break;
        case AK_ACT: scat(v, CUT_NAME[cut_act % NCUT]); k[i].v = (float)(cut_act % NCUT) / (NCUT - 1); break;
        case AK_DO: scat(v, cut_arm == t + 1u && o3_ms - cut_arm_t < 3000u ? "AGAIN" : "TURN"); break;
        case AK_FLT: scat(v, AF_NAME[AU.flt[t] % NAF]); k[i].v = (float)(AU.flt[t] % NAF) / (NAF - 1); break;
        case AK_CUT: {                          /* (Hz: 20 .. 20k) */
            uint32_t hz = (uint32_t)(20.0f * vd_pow(1000.0f, (float)AU.cut[t] / 127.0f));
            if (AU.flt[t] % NAF == AF_OFF) scat(v, "OPEN");
            else if (hz < 1000u) { fmt_u(v, hz); scat(v, "Hz"); } else { fmt_u(v, hz / 1000u); scat(v, "."); fmt_u(v + slen(v), hz / 100u % 10u); scat(v, "k"); }
            k[i].v = AU.cut[t] / 127.0f; break; }
        case AK_RES: fmt_u(v, AU.res[t]); k[i].v = AU.res[t] / 127.0f; break;
        case AK_DRV: fmt_u(v, AU.drv[t]); k[i].v = AU.drv[t] / 127.0f; break;
        case AK_LS: case AK_LE: {               /* (the loop's piece, in bars of the song's meter) */
            uint32_t nb = AU.len[t], s0 = au_lp_s(t), e0 = au_lp_e(t);
            if (!nb) { scat(v, "--"); break; }
            fmt_tpos(t, v, a[i] == AK_LS ? s0 : e0);
            k[i].v = (float)(a[i] == AK_LS ? s0 : e0) / (float)nb; break; }
        case AK_PLAYS: if (AU.type >> t & 1u) scat(v, "--"); else { scat(v, set.trk_daw >> t & 1u ? "DAW" : "SEQ"); k[i].v = (float)(set.trk_daw >> t & 1u); } break;
        case AK_OUT: if (!(set.trk_own >> t & 1u)) scat(v, "MAIN"); else { fmt_u(v, 3u + 2u * t); scat(v, "/"); fmt_u(v + slen(v), 4u + 2u * t); if ((uint32_t)vibe_output_count() < 4u + 2u * t) scat(v, "?"); }
                     k[i].v = (float)(set.trk_own >> t & 1u); break;
        case AK_MIN: if (!set.trk_mch[t]) scat(v, "SELECTED"); else { scat(v, "CH "); fmt_u(v + 3, set.trk_mch[t]); } k[i].v = set.trk_mch[t] / 16.0f; break;
        case AK_NONE: break;
        default: scat(v, AU.loop[t] ? "LOOP" : "ONCE"); k[i].v = AU.loop[t] ? 1.0f : 0.0f; break;
        }
    }
    return 1;
}
static int au_turn(uint32_t knob, int32_t e)
{
    uint8_t a[4];
    uint32_t t = sel % NAU;
    int32_t sg = e > 0 ? 1 : -1;
    if (au_lay < 0 && !au_plus() && !au_here()) return 0;
    au_layout(a);
    switch (a[knob & 3u]) {
    case AK_TYPE: {                             /* (0.41) TYPE_ORDER: DRUMS → BASS → OBERVAVE → DX → WAVETABLE → … → MIDI → AUDIO (MEL no more) */
        int au = (int)(AU.type >> t & 1u); int32_t cur = au ? (int32_t)NTYPE_ORDER : type_pos(trk_type(t)), nx;
        if (cur < 0) cur = type_pos(T_SYNTH) - (sg < 0 ? 0 : 1);   /* (an older MEL track: it sat between BASS and OBERVAVE) */
        nx = clampi(cur + sg, 0, (int32_t)NTYPE_ORDER);
        if (nx == cur && !(cur >= 0 && !au && trk_type(t) == T_MEL)) return 1;
        if (nx == (int32_t)NTYPE_ORDER) { au_type_toggle(t); return 1; }
        if (au) au_type_toggle(t);
        { uint32_t ty = TYPE_ORDER[nx];
          trk_set_type(t, ty); set_touch(); say_trk(ty == T_DRUMS ? ": DRUMS" : ty == T_BASS ? ": BASS (808)" : ty == T_SYNTH ? ": OBERVAVE" : ty == T_DX ? ": DX" : ty == T_WT ? ": WAVETABLE" : ty == T_GP ? ": PIANO" : ty == T_STR ? ": STRINGS" : ty == T_SMP ? ": SAMPLER" : ty == T_EP ? ": E-PIANO" : ": MIDI – ONLY OUT, CH ON THE MIDI PAGE", t); }
        return 1; }
    case AK_PLAYS: if (AU.type >> t & 1u) return 1;
        set.trk_daw = (uint8_t)(e > 0 ? set.trk_daw | 1u << t : set.trk_daw & ~(1u << t)); mix_apply(); set_touch();
        say_trk(e > 0 ? ": THE DAW PLAYS IT (STEPS SILENT)" : ": ITS STEPS PLAY", t); return 1;
    case AK_OUT:
        set.trk_own = (uint8_t)(e > 0 ? set.trk_own | 1u << t : set.trk_own & ~(1u << t)); mix_apply(); set_touch();
        if (e > 0 && (uint32_t)vibe_output_count() < 4u + 2u * t) say("THIS HOST HAS NO SUCH OUTPUT: STAYS IN THE MAIN MIX");
        return 1;
    case AK_MIN: {                              /* (0.24, Sven) bind the track to a MIDI channel: it always answers it, chosen or not */
        static char m[48]; char c[4];
        set.trk_mch[t] = (uint8_t)clampi((int32_t)set.trk_mch[t] + sg, 0, 16); set_touch();
        m[0] = 0;
        if (!set.trk_mch[t]) scat(m, ": MIDI IN WHEN SELECTED");
        else { scat(m, ": MIDI IN ALWAYS CH "); fmt_u(c, set.trk_mch[t]); scat(m, c); }
        if (midi_last_ch) { scat(m, " (KEYS SEND "); fmt_u(c, midi_last_ch); scat(m, c); scat(m, ")"); }
        say_trk(m, t); return 1; }
    case AK_CH:
        if ((e > 0) == (AU.st[t] != 0)) break;
        if (au_k(t)) { say("THE TAKE KEEPS ITS CHANNELS – CLEAR IT FIRST (REC HELD)"); return 1; }
        AU.st[t] = e > 0; break;
    case AK_INM: { if (au_src_in(AU.src[t]) < 0 || au_src_in(AU.src[t]) > 1) return 1;
                   uint32_t n = AU.src[t] == AU_IN2 ? 1u : 0u; set.au_inm[n] = (uint8_t)clampi((int32_t)(set.au_inm[n] % 3u) + sg, 0, 2);
                   if (au_stt(t) && AU.src[t] % NAUSRC != AU_RES) set.au_inm[n ^ 1u] = set.au_inm[n]; set_touch(); break; }
    case AK_MON: AU.mon[t] = (uint8_t)clampi((int32_t)(AU.mon[t] % NAUM) + sg, 0, NAUM - 1); break;
    case AK_GAIN: { if (au_src_in(AU.src[t]) < 0 || au_src_in(AU.src[t]) > 1) return 1;   /* (2 dB a detent; stereo: both inputs) */
                    uint32_t n = AU.src[t] == AU_IN2 ? 1u : 0u; set.au_gain[n] = (uint8_t)clampi((int32_t)(set.au_gain[n] > 40u ? 40u : set.au_gain[n]) + e * 2, 0, 40);
                    if (au_stt(t)) set.au_gain[n ^ 1u] = set.au_gain[n]; set_touch(); return 1; }
    case AK_SRC: {                              /* (0.29) the order IN 1, IN 2, .. IN n (the host's), RESAMP; stereo: in pairs */
        int mx = (int)au_src_max(), ii = au_src_in(AU.src[t]), pos = ii < 0 ? mx : ii, st = (int)au_stt(t);
        if (ii >= mx) pos = mx - 1;
        if (st && pos < mx) pos &= ~1;
        pos += st ? 2 * sg : sg;
        if (st && pos > mx && pos - 2 < mx) pos = mx;   /* (from the last pair to RESAMP) */
        pos = clampi(pos, 0, mx);
        AU.src[t] = (uint8_t)(pos == mx ? AU_RES : pos < 2 ? pos : pos + 1);
        if (au_c.src[t] != AU.src[t] && vibe_input_name(au_src_in(AU.src[t]))[0] && au_src_in(AU.src[t]) >= 2) say(vibe_input_name(au_src_in(AU.src[t])));
        break; }
    case AK_LVL: set.mix_lvl[t] = (uint8_t)clampi((int32_t)set.mix_lvl[t] + e * 2, 0, 127); mix_apply(); set_touch(); return 1;
    case AK_BARS: {                             /* (0.32, Sven: "Length 0 = Time Mode") below 1 bar: TIME – no bars, the track as long
                                                 * as its take, START / END anywhere in it. Without a take: TIME below FREE */
        uint32_t b = au_bar(), bars = (AU.len[t] + b / 2u) / b, kk = au_k(t), hi = kk ? tk[kk].hi : 0u;
        int32_t lo = hi ? 0 : -1, cur = au_tm(t) ? lo : (int32_t)bars, nb;
        if (!(AU.type >> t & 1u)) return 1;
        if (ctl_au_rq >> t & 1u) { say("NOT WHILE IT RECORDS"); return 1; }
        nb = clampi(cur + (e > 1 || e < -1 ? e * 4 : e), lo, 999);
        if (nb == cur && !(hi && !au_tm(t) && cur == 0 && e < 0)) return 1;   /* (a take on FREE: down to TIME still) */
        if (nb == lo && hi) nb = -1;           /* (a take: 0 is TIME, there is no FREE) */
        if (au_tm(t) && hi && e > 0) {          /* out of TIME: the bars nearest the take */
            uint32_t n = (hi + b / 2u) / b; nb = (int32_t)(n ? n : 1u) + (e > 1 ? 4 * (e - 1) : 0);
        }
        if (nb < 0) {
            AU.tm[t] = 1; AU.len[t] = hi;
            say_trk(hi ? ": TIME – AS LONG AS THE TAKE, NO BARS · START / END MOVE IT" : ": TIME – REC RECORDS AT ONCE, REC AGAIN ENDS IT", t);
        } else {
            if (au_tm(t)) say_trk(": BARS AGAIN", t);
            AU.tm[t] = 0; AU.len[t] = (uint32_t)nb * b;
        }
        if (AU.lpe[t] >= AU.len[t]) AU.lpe[t] = 0;
        if (AU.lps[t] >= au_lp_e(t)) AU.lps[t] = 0;
        break; }
    case AK_DUB: AU.dub[t] = e > 0; break;
    case AK_LS: case AK_LE: {                   /* free (Sven: "auf den Nulldurchgang schneiden"): turned slowly a detent goes to
                                                   the next rising zero crossing (START and END the same slope: no click at the seam),
                                                   faster in bigger steps (5, 25, 100 ms), SHIFT: a frame, no snapping. END at the
                                                   track's end = 0 (it follows BARS) */
        static uint32_t lt;
        uint32_t nb = AU.len[t], s0 = au_lp_s(t), e0 = au_lp_e(t), gap = o3_ms - lt, ms = obv_fs / 1000u, ls = a[knob & 3u] == AK_LS;
        int64_t step, v, lo = ls ? 0 : (int64_t)s0 + 1, hi = ls ? (int64_t)e0 - 1 : (int64_t)nb;
        lt = o3_ms;
        if (!nb) { say("RECORD FIRST"); return 1; }
        step = seq_shift ? 1 : gap > 150u ? 1 : (int64_t)ms * (gap > 70u ? 5 : gap > 30u ? 25 : 100);
        v = (int64_t)(ls ? s0 : e0) + e * step;
        if (v < lo) v = lo;
        if (v > hi) v = hi;
        {   /* (0.32 TIME) a beat it passes holds it (the grid the take would fit, Sven); SHIFT: free */
            static uint32_t snap_t;
            int64_t o = (int64_t)(ls ? s0 : e0), q = (int64_t)quarter_len(), bt = METER[meter_of(0)].den8 ? q / 2 : q, g;
            if (au_tm(t) && !seq_shift && bt > 0 && o % bt == 0 && o3_ms - snap_t < 200u) return 1;   /* (on a beat it holds a moment) */
            if (au_tm(t) && !seq_shift && bt > 0 && v != o) {
                g = v > o ? (o / bt + 1) * bt : (o - 1) / bt * bt;
                if (o == 0 && v < o) g = -1;
                if (g >= lo && g <= hi && (v > o ? g <= v : g >= v)) { v = g; snap_t = o3_ms; goto lp_set; }
            }
        }
        if (!seq_shift && v > lo && v < hi) v = au_zc(t, (uint32_t)v, sg, (uint32_t)lo, (uint32_t)hi);
    lp_set:
        if (ls) AU.lps[t] = (uint32_t)v; else AU.lpe[t] = (uint32_t)v >= nb ? 0u : (uint32_t)v;
        break; }
    case AK_FLT: AU.flt[t] = (uint8_t)clampi((int32_t)(AU.flt[t] % NAF) + sg, 0, NAF - 1); break;
    case AK_CUT:                                /* (turned while OFF: the low pass comes on) */
        if (AU.flt[t] % NAF == AF_OFF) { AU.flt[t] = AF_LP; AU.cut[t] = 127; }
        AU.cut[t] = (uint8_t)clampi((int32_t)AU.cut[t] + e * 2, 0, 127); break;
    case AK_RES: AU.res[t] = (uint8_t)clampi((int32_t)AU.res[t] + e * 2, 0, 127); break;
    case AK_DRV: AU.drv[t] = (uint8_t)clampi((int32_t)AU.drv[t] + e * 2, 0, 127); break;
    case AK_NONE: return 1;
    case AK_FROM: case AK_TO: {                 /* a sixteenth a detent, a bar on a fast turn, 1 ms with SHIFT; FROM before TO */
        uint32_t *c = a[knob & 3u] == AK_FROM ? &cut_from[t] : &cut_to[t], sp = au_span(t), q = quarter_len();
        int64_t step = seq_shift ? (int64_t)(obv_fs / 1000u) : (e > 1 || e < -1) ? (int64_t)song_bar_frames(q) : (int64_t)(q / 4u);
        if (au_tm(t) && !seq_shift) step = (int64_t)(obv_fs / 1000u) * ((e > 1 || e < -1) ? 250 : 10);   /* (0.32 TIME: 10 ms, fast 250 ms – no grid) */
        int64_t v = (int64_t)*c + (seq_shift || (e <= 1 && e >= -1) ? e : sg) * step;
        if (!seq_shift && !au_tm(t) && !(e > 1 || e < -1)) { int64_t six = q / 4u ? q / 4u : 1; v = (v + (e > 0 ? 0 : six - 1)) / six * six; }   /* (onto the grid) */
        *c = (uint32_t)(v < 0 ? 0 : v > (int64_t)sp ? sp : v);
        if (cut_from[t] >= cut_to[t]) { if (c == &cut_from[t]) cut_to[t] = cut_from[t] + 1u > sp ? sp : cut_from[t] + 1u; else cut_from[t] = cut_to[t] ? cut_to[t] - 1u : 0u; }
        cut_arm = 0;
        return 1; }
    case AK_ACT: cut_act = (uint32_t)clampi((int32_t)(cut_act % NCUT) + sg, 0, NCUT - 1); cut_arm = 0; return 1;
    case AK_DO:
        if (e < 0) { cut_arm = 0; return 1; }
        if (cut_arm == t + 1u && o3_ms - cut_arm_t < 3000u) { cut_arm = 0; au_cut_do(t); }
        else { cut_arm = t + 1u; cut_arm_t = o3_ms; say("TURN AGAIN TO CUT"); }
        return 1;
    default: AU.loop[t] = e > 0; break;
    }
    aud_touch(); au_apply();
    return 1;
}

/* ---- drawing: the take as a wave (peaks per column), the playhead, the bars */
static uint32_t au_span(uint32_t t)             /* the frames a view shows: the track, or the take growing */
{
    uint32_t len = AU.len[t], hi = au_hi(t), grow = aur[t].on ? ui_au_pos[t] + 1u : 0u;
    if (grow && (!AU.loop[t] || !len)) grow = au_up(grow);
    if (hi > len && !AU.loop[t]) len = hi;
    return grow > len ? grow : len;
}
static void au_wave(uint32_t t, int x, int y, int w, int h, uint32_t span, uint32_t col)
{
    uint32_t k = au_k(t);
    const int16_t *pl = k ? au_arena + tk[k].off : 0;
    uint32_t ch = k ? tk_ch(k) : 1u, hi = au_hi(t) > span ? span : au_hi(t);
    if (!pl || !tk[k].ready) return;
    if (aur[t].on && ui_au_pos[t] + 1u > hi) hi = ui_au_pos[t] + 1u > span ? span : ui_au_pos[t] + 1u;
    for (int c = 0; c < w; c++) {
        uint32_t f0 = (uint32_t)((uint64_t)span * (uint32_t)c / (uint32_t)w), f1 = (uint32_t)((uint64_t)span * (uint32_t)(c + 1) / (uint32_t)w), pk = 0;
        if (f0 >= hi) break;
        if (f1 > hi) f1 = hi;
        uint32_t step = (f1 - f0) / 24u + 1u;
        for (uint32_t f = f0; f < f1; f += step) {
            int32_t v = pl[f * ch]; uint32_t a = (uint32_t)(v < 0 ? -v : v);
            if (ch == 2u) { int32_t v2 = pl[f * 2u + 1u]; uint32_t b = (uint32_t)(v2 < 0 ? -v2 : v2); if (b > a) a = b; }
            if (a > pk) pk = a;
        }
        int hh = (int)(pk * (uint32_t)h / 32768u); if (hh < 1) hh = 1;
        vg_rect(x + c, y + (h - hh) / 2, 1, hh, col);
    }
}
static uint32_t au_head(uint32_t t)             /* the playhead: where it records, else where it plays */
{
    if (aur[t].on) return ui_au_pos[t];
    return ctl_seq_run ? ui_au_play[t] : 0xFFFFFFFFu;
}
static void au_row(uint32_t t, int y, int on)   /* TRACKS: one audio track's row (the sound name's place, the steps') */
{
    char b[32];
    uint32_t c = TRK_RGB[t], span = au_span(t), head = au_head(t), recn = (uint32_t)au_recn(t), arm = au_armed() >> t & 1u;
    b[0] = 0; scat(b, arm ? "ARM · " : "AUDIO · "); scat(b, au_src_name(AU.src[t], au_stt(t)));
    vg_text(b, CX0 + 34, y + 6, VG_MED, arm ? RED : on ? FG : DIM, VG_LEFT);
    if (arm && au_src_in(AU.src[t]) >= 0) {     /* (0.29) the armed input's level, a thin bar under the name */
        float lv = lvl_ins[au_src_in(AU.src[t]) % VIBE_IO_MAX], v = lv > 0.00001f ? 1.0f + vd_log2(lv) / 8.0f : 0.0f;
        int w = (int)(v * 120.0f); if (w < 0) w = 0; if (w > 120) w = 120;
        vg_rect(CX0 + 34, y + 24, 120, 3, CELL2);
        vg_rect(CX0 + 34, y + 24, w, 3, lv > 0.99f ? RED : lv > 0.5f ? 0xFFE14D : 0x00E5FF);
    }
    vg_rect(178, y + 4, 190, 21, CELL2);
    if (span) {
        au_wave(t, 178, y + 5, 190, 19, span, recn ? RED : c);
        if (head != 0xFFFFFFFFu && head < span) vg_rect(178 + (int)((uint64_t)head * 190u / span), y + 3, 2, 23, FG);
        if (au_tm(t)) { b[0] = 0; scat(b, "TIME"); }   /* (0.32) */
        else { fmt_u(b, (AU.len[t] + au_bar() / 2u) / au_bar()); scat(b, AU.loop[t] ? " LP" : " BR"); }
    } else { b[0] = 0; scat(b, recn ? "REC" : "EMPTY"); }
    if (recn) vg_circle(172, y + 14, 4, aur[t].on || blink() ? RED : 0x7A1040);
    else if (arm) vg_circle(172, y + 14, 4, 0x8A1030);
    vg_text(b, 404, y + 6, VG_MED, span ? FG : DIM, VG_RIGHT);
}
static void au_view(void)                       /* STEPS (CUT) / TRACK of an audio track: the whole take, the bars, the state */
{
    char b[64];
    uint32_t t = sel % NAU, c = TRK_RGB[t], span = au_span(t), head = au_head(t), bar = au_bar(), k = au_k(t);
    uint32_t recn = (uint32_t)au_recn(t), arm = au_armed() >> t & 1u;
    int lp_x0 = 0, lp_x1 = 0;
    vg_rect(CX0, 72, CW, 108, CELL);
    if (arm) { vg_rect(CX0, 72, CW, 2, RED); vg_rect(CX0, 178, CW, 2, RED); vg_rect(CX0, 72, 2, 108, RED); vg_rect(CX0 + CW - 2, 72, 2, 108, RED); }   /* (0.29) ARMED: a red frame */
    if (!span) vg_text(recn ? "WAITING FOR THE BAR ..." : "EMPTY – REC RECORDS ONTO IT", 240, 116, VG_MED, recn ? RED : DIM, VG_CENTER);
    else {
        uint32_t bars = (span + bar - 1u) / bar;
        {   /* (0.32, as the note tracks' TRACK) the beats, the bars brighter and numbered – in TIME the grid the take would fit */
            uint32_t q = quarter_len(), bt = METER[meter_of(0)].den8 ? q / 2u : q, bpb;
            char l[12];
            if (!bt) bt = 1u;
            bpb = bar / bt ? bar / bt : 1u;
            if (span / bt <= 230u) for (uint32_t g = bt, n = 1; g < span; g += bt, n++) {
                int x = CX0 + (int)((uint64_t)g * CW / span), isbar = n % bpb == 0u;
                vg_rect(x, 72, isbar ? 2 : 1, 108, isbar ? DIM : MUTED);
                if (span / bt <= 16u) { fmt_u(l, n / bpb + 1u); scat(l, "."); fmt_u(l + slen(l), n % bpb + 1u); vg_text(l, x + 3, 74, VG_SMALL, isbar ? DIM : MUTED, VG_LEFT); }
                else if (isbar && bars <= 24u) { fmt_u(l, n / bpb + 1u); vg_text(l, x + 3, 74, VG_SMALL, DIM, VG_LEFT); }
            }
            else if (bars <= 64u) for (uint32_t i = 1; i < bars; i++) vg_rect(CX0 + (int)((uint64_t)i * bar * CW / span), 72, 2, 108, DIM);
        }
        if (SV_ENTRY(seq_view) && cut_to[t] > cut_from[t]) {   /* CUT: the piece chosen */
            uint32_t x0 = (uint32_t)((uint64_t)cut_from[t] * CW / span), x1 = (uint32_t)((uint64_t)cut_to[t] * CW / span);
            if (x1 > (uint32_t)CW) x1 = CW;
            if (x1 <= x0) x1 = x0 + 1u;
            vg_rect(CX0 + (int)x0, 72, (int)(x1 - x0), 108, vg_mix(CELL, ACCENT, 0.25f));
            vg_rect(CX0 + (int)x0, 72, 1, 108, ACCENT); vg_rect(CX0 + (int)x1 - 1, 72, 1, 108, ACCENT);
        }
        if (!SV_ENTRY(seq_view) && AU.len[t] && (au_lp_s(t) || au_lp_e(t) < AU.len[t])) {   /* TRACK: the loop's piece – outside it dimmed, START green, END red */
            int x0 = CX0 + (int)((uint64_t)au_lp_s(t) * CW / span), x1 = CX0 + (int)((uint64_t)au_lp_e(t) * CW / span);
            if (x1 > CX0 + CW) x1 = CX0 + CW;
            if (x1 <= x0) x1 = x0 + 1;
            lp_x0 = x0; lp_x1 = x1;
        }
        au_wave(t, CX0, 76, CW, 100, span, aur[t].on ? RED : c);
        if (lp_x1 > lp_x0) {                    /* (over the wave: the part not played hatched, the edges) */
            for (int y = 72; y < 180; y += 2) { vg_rect(CX0, y, lp_x0 - CX0, 1, BG); vg_rect(lp_x1, y, CX0 + CW - lp_x1, 1, BG); }
            vg_rect(lp_x0, 72, 2, 108, GREEN); vg_rect(lp_x1 - 2, 72, 2, 108, RED);
        }
        if (head != 0xFFFFFFFFu && head < span) vg_rect(CX0 + (int)((uint64_t)head * CW / span), 72, 2, 108, FG);
    }
    b[0] = 0;
    if (recn) { scat(b, aur[t].on ? "● REC " : "REC ON THE BAR "); if (aur[t].on) fmt_time(b + slen(b), ui_au_pos[t]); }
    else if (au_arm >> t & 1u) scat(b, "ARMED: PLAY RECORDS FROM BAR 1");
    else if (arm) scat(b, "ARM · REC RECORDS IT · REC HELD + KNOB PUSH: OFF");
    else if (k && ld_k == k) { scat(b, "LOADING "); fmt_u(b + slen(b), ld.bytes ? (uint32_t)((uint64_t)ld.off * 100u / ld.bytes) : 0u); scat(b, "%"); }
    else if (k && sv_k == k) { scat(b, "SAVING "); fmt_u(b + slen(b), (uint32_t)((uint64_t)sv.off * 100u / sv.total)); scat(b, "%"); }
    else if (SV_ENTRY(seq_view)) {
        scat(b, "CUT "); fmt_tpos(t, b + slen(b), cut_from[t]); scat(b, " - "); fmt_tpos(t, b + slen(b), cut_to[t]);
        scat(b, " · "); scat(b, CUT_NAME[cut_act % NCUT]);
    }
    else {
        scat(b, AU.loop[t] ? "LOOP · " : "ONCE · ");
        if (au_tm(t)) scat(b, "TIME · ");       /* (0.32) */
        else { fmt_u(b + slen(b), (AU.len[t] + bar / 2u) / bar); scat(b, " BARS · "); }
        if (AU.len[t] && (au_lp_s(t) || au_lp_e(t) < AU.len[t])) { char p[24]; scat(b, "PLAYS "); fmt_tpos(t, p, au_lp_s(t)); scat(b, p); scat(b, " - "); fmt_tpos(t, p, au_lp_e(t)); scat(b, p); }
        else if (au_tm(t)) fmt_sec(b + slen(b), AU.len[t]);
        else fmt_time(b + slen(b), AU.len[t]);
        scat(b, au_stt(t) ? " · STEREO" : " · MONO");
    }
    vg_text(b, CX0, 186, VG_MED, recn || arm || (au_arm >> t & 1u) ? RED : DIM, VG_LEFT);
    b[0] = 0;
    if (seq_shift && SV_ENTRY(seq_view)) scat(b, "SHIFT: 1 ms STEPS");
    else if (seq_shift) scat(b, "SHIFT: TRACK+");
    else { scat(b, "FREE "); fmt_time(b + slen(b), tk_freef() / (au_stt(t) ? 2u : 1u)); }
    vg_text(b, 470, 186, VG_MED, MUTED, VG_RIGHT);
}

/* ---- the SOUND key's pages for the track type (3.7): which one is shown, its knobs, what goes under them */
static void go(uint32_t pg);                    /* (main.c) */
static int snd_group(void) { const group_t *g = page_group(page); return g->key <= CK_ARP; }   /* SOUND, ENV, LFO, ARP */
static uint32_t snd_view(void)                  /* SX_* shown now, SX_NONE = the page's own */
{
    if (!snd_group() || in_seq() || page == PG_MIXER) return SX_NONE;
    if (AU.type >> sel & 1u) return snd_x == SX_TAKE || snd_x == SX_FILTER || snd_x == SX_TYPE ? snd_x : SX_AUDIO;
    return snd_x == SX_TYPE && page_group(page)->key == CK_SOUND ? SX_TYPE : SX_NONE;
}
static void snd_knobs(uint32_t sx, knob_t *k) { au_lay = (int)sx; au_knobs(k); au_lay = -1; }
static void snd_turn(uint32_t sx, uint32_t knob, int32_t e) { au_lay = (int)sx; au_turn(knob, e); au_lay = -1; }
static const char *const SX_TITLE[5] = {"", "AUDIO", "TAKE", "FILTER", "TYPE"};
static void snd_under(uint32_t sx)              /* below the knobs: the take, or what TYPE means */
{
    uint32_t t = sel % NAU, span = au_span(t);
    if (sx == SX_TYPE) {
        static const char *const TT[NTYPE] = {"DRUMS: a kit of 8 synth drums – the pads and the keys play them.",
                                              "BASS: the 808 – played like a bass over the keys, mono, with slides.",
                                              "MEL: 4-operator FM (an older project's) – its sounds are DX's first 16 now.",
                                              "OBERVAVE: the analog poly – its 64 sounds and pages (8 voices).",
                                              "DX: a DX7 voice – 6 operators, 32 algorithms, 4 macros, OP pages; LOAD takes .syx banks.",
                                              "STRINGS: a string ensemble – 16', 8', 4' registers and the ensemble (8 voices).",
                                              "SAMPLER: its own sample (REC on it, or LOAD) – PLAY it over the keys, or CHOP it.",
                                              "MIDI: silent here – its notes, program and 4 CCs go out to a device on its channel.",
                                              "E-PIANO: tines, reeds or a CLAV (MODEL) – the pickup, tremolo / wah (8 voices).",
                                              "WAVETABLE: 8 tables, POSITION moves through them – 2 oscillators + sub.",
                                              "PIANO: a sampled concert grand – the touch picks the recording (8 voices).",
                                              "CLAV: (inside E-PIANO now – MODEL CLAV)."};
        vg_text(AU.type >> t & 1u ? "AUDIO: the track plays what was recorded or loaded onto it." : TT[trk_type(t)], 240, 214, VG_MED, DIM, VG_CENTER);
        vg_text("KNOB 1: the kind – in this pattern · a new kind loads its first sound", 240, 236, VG_MED, MUTED, VG_CENTER);
        vg_text("PLAYS: SEQ / DAW · OUT: own output · MIDI IN: CH n = always that channel", 240, 256, VG_SMALL, MUTED, VG_CENTER);
        return;
    }
    vg_rect(CX0, 204, CW, 64, CELL);
    if (!span) { vg_text("EMPTY – REC RECORDS · F6 x3: FILES (LOAD WAV)", 240, 228, VG_MED, DIM, VG_CENTER); return; }
    au_wave(t, CX0, 208, CW, 56, span, TRK_RGB[t]);
    { uint32_t h = au_head(t); if (h != 0xFFFFFFFFu && h < span) vg_rect(CX0 + (int)((uint64_t)h * CW / span), 204, 2, 64, FG); }
}
static void sound_key(void)                     /* SOUND: the synth's pages, then TYPE; an audio track: AUDIO, TAKE, TYPE */
{
    uint32_t sx = snd_view();
    if (AU.type >> sel & 1u) {
        if (!snd_group() || in_seq() || page == PG_MIXER) { go(PG_PERFORM); snd_x = SX_AUDIO; return; }
        snd_x = sx == SX_AUDIO ? SX_TAKE : sx == SX_TAKE ? SX_FILTER : sx == SX_FILTER ? SX_TYPE : SX_AUDIO;
        if (page_group(page)->key != CK_SOUND) { uint32_t keep = snd_x; go(PG_PERFORM); snd_x = keep; }
        return;
    }
    if (sx == SX_TYPE) { go(PG_PERFORM + 1u); return; }   /* (Sven 14.11.: TYPE right after PERFORM) */
    if (page == PG_PERFORM && page_group(page)->key == CK_SOUND && !in_seq()) { snd_x = SX_TYPE; return; }
    go(cycle_page(PG_PERFORM, snd_count()));
}
