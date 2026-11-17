/* SPDX-License-Identifier: GPL-3.0-only */
/* (3.7, VIBE) FILES – F6 a third time (GLOBAL → OUTPUT → FILES). KNOB 1 the mode, KNOB 2 / DATA the list, ENTER does it:
 *   SAVE SONG     everything as one file "<NAME>.obv" in "VIBE FM/boom/" on the card (KNOB 2 the letter, KNOB 3 / DATA
 *                 its character; SONG 01, 02 … offered): the settings, the
 *                 99 programs' user sectors, the 64 patterns with their sounds and mixers, and which take each audio track
 *                 plays (the takes are WAVs beside it – the host never overwrites a file, so a project's takes stay).
 *                 The FM-1's backup (3.5) does the same through SysEx; here the card holds it.
 *   LOAD SONG     a .obv from that folder – ENTER twice (everything now is replaced): the files back into the storage,
 *                 then BOOM starts over from them in place, the takes load from the card.
 *   LOAD WAV      any WAV on the card or a USB stick (16 / 24 bit, float; mono or stereo) into the chosen track, which
 *                 becomes an audio track: another rate is converted (pitch and tempo stay), more than two channels: the
 *                 first two. It is saved as a take on the card at once (a stick may be gone tomorrow).
 * Nothing here runs in the audio; files are read and written a piece per frame. */
/* (3.9, Sven 9.11.2026: "Sounds, Pattern, Audiospuren – einzeln oder ALLES, speichern und laden") Two pages instead of one:
 *   F3 LOAD · F4 SAVE, KNOB 1 the KIND: PRESET (one sound, .obp) · PATTERN (the pattern with its sounds, mixer and audio
 *   tracks, .obt; its takes as WAVs beside it) · AUDIO (a track's take as a WAV / any WAV into a track) · SONG (everything, .obv).
 *   Pressing the page's key again steps the kind. All files in "VIBE FM/boom/" on the card. */
enum { FK_PRESET, FK_PATTERN, FK_AUDIO, FK_SONG, FK_MIDI, FK_DX, NFK };   /* (0.6: DX – a DX7 bank (.syx), load only) */   /* (BOOM 0.4: MIDI – the pattern as a .mid for the DAW, save only) */
static const char *const FK_NAME[NFK] = {"PRESET", "PATTERN", "AUDIO", "SONG", "MIDI", "DX BANK"};
static const char *const FK_EXT[NFK] = {".obp", ".obt", ".wav", ".obv", ".mid", ".syx"};
static uint32_t f_page, fk;                     /* f_page: 0 LOAD, 1 SAVE; fk: the kind */
static uint32_t f_tpat, f_ttrk;                 /* (3.9) LOAD PATTERN / AUDIO: where to – KNOB 3 the pattern, KNOB 4 the track (at first: here) */
#define F_SAVE (f_page == 1u)
#define F_BROWSE (!f_page && fk == FK_AUDIO)      /* LOAD AUDIO: the WAV browser over every source */
#define F_TAKES (F_SAVE && fk == FK_AUDIO)        /* SAVE AUDIO: the takes in memory to choose from */
/* the name being typed for SAVE SONG: 12 characters, KNOB 2 the place, KNOB 3 / DATA the character */
#define NM_LEN 12u
static const char NM_SET[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_&.";
static char nm_edit[NM_LEN + 1];
static uint32_t nm_pos, nm_ready;
static int files_on;
static uint32_t f_cur, f_n, f_stale = 1, f_arm, f_arm_t;
static char f_dir[VIBE_MEDIA_PATH];             /* LOAD WAV: the folder shown ("" = the sources) */
static char f_wkey[64];                         /* the source the host writes to (its key) */
#define F_MAX 200
static struct { char kind, name[127]; } f_ent[F_MAX];   /* 's' source (name = key), 'u' up, 'd' folder, 'f' file */
static char f_lab[F_MAX][40];                   /* what a source is called */

static int strcmp_s(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return (unsigned char)*a - (unsigned char)*b; }
static int ends_with(const char *s, const char *e)   /* (case blind) */
{
    uint32_t a = slen(s), b = slen(e);
    if (a < b) return 0;
    for (uint32_t i = 0; i < b; i++) { char x = s[a - b + i], y = e[i]; if (x >= 'A' && x <= 'Z') x = (char)(x + 32); if (x != y) return 0; }
    return 1;
}
static void f_add(char kind, const char *name, uint32_t n, const char *lab)
{
    if (f_n >= F_MAX) return;
    if (n > sizeof f_ent[0].name - 1) n = sizeof f_ent[0].name - 1;
    f_ent[f_n].kind = kind; memcpy(f_ent[f_n].name, name, n); f_ent[f_n].name[n] = 0;
    f_lab[f_n][0] = 0; if (lab) { uint32_t m = slen(lab); if (m > 39) m = 39; memcpy(f_lab[f_n], lab, m); f_lab[f_n][m] = 0; }
    f_n++;
}
/* (3.9, Sven: "eine Audiospur in mehreren Pattern") the takes in memory, by the pattern and track they play in – LOAD AUDIO
 * puts one into this track too (the same take: a CUT changes it everywhere; a recording always makes a new one) */
static void f_takes(int all)                     /* all: the selected track's own too (SAVE AUDIO) */
{
    for (uint32_t n = 0; n < NPAT; n++) for (uint32_t t = 0; t < NAU; t++) {
        uint32_t k = paud[n].take[t]; char id[8], lab[40];
        if (!(paud[n].type >> t & 1u) || !k || k > NTAKE || !tk[k].inuse || !tk[k].hi) continue;
        if (!all && n == set.pat % NPAT && t == sel % NAU) continue;
        id[0] = 0; fmt_u(id, k);
        lab[0] = 0; scat(lab, "P"); fmt_u(lab + slen(lab), n + 1u); { char l[3] = {' ', (char)('A' + t), 0}; scat(lab, l); } scat(lab, " · "); fmt_time(lab + slen(lab), tk[k].hi);
        if (tk[k].st) scat(lab, " · STEREO");
        f_add('t', id, slen(id), lab);
    }
}
static int f_sources(void)                      /* the sources; f_wkey = the writing one. -1 = none / pending */
{
    static char b[2048];
    int r = vibe_media_sources(b, (int)sizeof b - 1);
    if (r < 0) return -1;
    b[r] = 0; f_n = 0;
    if (F_BROWSE) f_takes(0);
    for (char *l = b; *l; ) {
        char *e = l; while (*e && *e != '\n') e++;
        char *t1 = l; while (t1 < e && *t1 != '\t') t1++;
        char *t2 = t1 + 1; while (t2 < e && *t2 != '\t') t2++;
        if (t1 < e) {
            char lab[40]; uint32_t n = (uint32_t)(e - t2 - 1); if (t2 >= e) n = 0; if (n > 39) n = 39; memcpy(lab, t2 + 1, n); lab[n] = 0;
            for (char *q = t1 + 1; q < t2; q++) if (*q == 'w' && !f_wkey[0]) { uint32_t k = (uint32_t)(t1 - l); if (k > 63) k = 63; memcpy(f_wkey, l, k); f_wkey[k] = 0; }
            f_add('s', l, (uint32_t)(t1 - l), lab[0] ? lab : 0);
        }
        l = *e ? e + 1 : e;
    }
    return 0;
}
static void f_refresh(void)                     /* the list for the mode (PENDING: again next frame) */
{
    static char b[16384];
    char folder[VIBE_MEDIA_PATH];
    if (!f_stale) return;
    if (F_TAKES) {                              /* (3.9) the takes in memory, the selected track's first */
        uint32_t t = sel % NAU, n = set.pat % NPAT, k = (paud[n].type >> t & 1u) ? paud[n].take[t] : 0u;
        f_n = 0; f_takes(1); f_stale = 0;
        if (f_cur >= f_n) f_cur = 0;
        if (k) for (uint32_t i = 0; i < f_n; i++) { uint32_t v = 0; for (const char *q = f_ent[i].name; *q >= '0' && *q <= '9'; q++) v = v * 10u + (uint32_t)(*q - '0'); if (v == k) { f_cur = i; break; } }
        nm_ready = 0;
        return;
    }
    if (F_BROWSE && !f_dir[0]) { if (f_sources() == 0) f_stale = 0; return; }
    if (!f_wkey[0]) { f_sources(); f_n = 0; if (!f_wkey[0]) { if (F_BROWSE) return; f_stale = 0; return; } }
    if (F_BROWSE) { folder[0] = 0; scat(folder, f_dir); }
    else { folder[0] = 0; scat(folder, f_wkey); scat(folder, "/VIBE FM/boom"); }
    int r = vibe_media_list(folder, 0, b, (int)sizeof b - 1);
    if (r == VIBE_MEDIA_PENDING) return;
    f_n = 0; f_stale = 0;
    if (F_BROWSE) f_add('u', "..", 2, 0);
    if (!f_page && fk == FK_SONG) f_add('n', "NEW SONG", 8, "– EMPTY (settings and programs stay)");   /* (3.9) */
    if (r <= 0) return;
    b[r] = 0;
    for (char *l = b; *l; ) {
        char *e = l; while (*e && *e != '\n') e++;
        char *t2 = l + 2; while (t2 < e && *t2 != '\t') t2++;   /* "d\t\tname" / "f\tbytes\tname" */
        if (t2 < e) {
            char *nm = t2 + 1; char c = *e; *e = 0;
            if (*l == 'd' && F_BROWSE) f_add('d', nm, (uint32_t)(e - nm), 0);
            else if (*l == 'f' && ends_with(nm, FK_EXT[fk % NFK])) f_add('f', nm, (uint32_t)(e - nm), 0);
            *e = c;
        }
        l = *e ? e + 1 : e;
    }
}

/* ---- a project: "BOMPRJ1" 0, u32 count, then per file: name (16 bytes), u32 size, its bytes */
#define NPRJ (7u + 3u * NPAT)                    /* settings, plob0, plob1, takes (3.7), takes2, then blob<n>, psnd<n>, paud<n> */
static const char *prj_file(uint32_t i)
{
    static const char *const F[7] = {"settings", "plob0", "plob1", "plob2", "plob3", "takes", "takes2"};
    if (i < 7u) return F[i];
    i -= 7u;
    return i % 3u == 0u ? PB_FILE_N(i / 3u) : i % 3u == 1u ? PS_FILE_N(i / 3u) : pat_fname("paud", i / 3u);
}
#define PRJ_MAX (6u << 20)                      /* (BOOM 0.5: 64 patterns full: ~3.5 MB) */
static uint8_t prj[PRJ_MAX];
static struct { int h; uint32_t off, total, load, got, make, kind; char path[VIBE_MEDIA_PATH], fn[32]; } pj;   /* kind: what is being saved / loaded (FK_*) */
static uint32_t ct_n;                           /* the container being built in prj: entries so far */
static void ct_begin(const char *magic) { memcpy(prj, magic, 8); ct_n = 0; pj.total = 12; }
static int ct_add(const char *name, const void *data, uint32_t sz)   /* an entry (name up to 15 characters) */
{
    uint32_t at = pj.total;
    if (at + 20u + sz > PRJ_MAX) return -1;
    memset(prj + at, 0, 16); memcpy(prj + at, name, slen(name) > 15u ? 15u : slen(name));
    put32(prj + at + 16, sz); memcpy(prj + at + 20, data, sz);
    pj.total = at + 20u + sz; ct_n++;
    return 0;
}
static const uint8_t *ct_find(const char *name, uint32_t *sz)   /* an entry of the container in prj (pj.total bytes) */
{
    uint32_t at = 12, n = rd32(prj + 8);
    for (uint32_t k = 0; k < n && at + 20u <= pj.total; k++) {
        uint32_t s = rd32(prj + at + 16);
        if (at + 20u + s > pj.total) return 0;
        if (!strcmp_s((const char *)prj + at, name)) { *sz = s; return prj + at + 20; }
        at += 20u + s;
    }
    return 0;
}
static void ct_name(char *fn, const char *dflt, const char *ext)   /* the typed name (trailing spaces off), else dflt, + ext */
{
    uint32_t n = NM_LEN;
    while (n && (nm_edit[n - 1] == ' ' || !nm_edit[n - 1])) n--;
    memcpy(fn, nm_edit, n); fn[n] = 0; if (!n) scat(fn, dflt); scat(fn, ext);
}
static void ct_finish(uint32_t kind, const char *dflt)   /* the container is complete: to the card (prj_poll makes the file) */
{
    put32(prj + 8, ct_n); pj.off = 0; pj.kind = kind;
    ct_name(pj.fn, dflt, FK_EXT[kind % NFK]);
    pj.make = 1;
}   /* make: the file still to be made (the host said PENDING) */   /* load: 1 = reading prj */
static void prj_save(void)
{
    uint32_t at = 12, n = 0;
    if (pj.h || pj.load || pj.make) return;
    if (tk_save_q || sv_k || ctl_au_rq || ld_k || ld_q) { say("A TAKE IS STILL BEING SAVED – AGAIN IN A MOMENT"); return; }
    aud_dirty |= 1ull << (set.pat % NPAT); store_force = 1; aud_poll(); store_force = 0;   /* (every pattern's audio into the storage) */
    {   /* the name goes into the settings first: the song knows what it is called */
        uint32_t n = NM_LEN; while (n && (nm_edit[n - 1] == ' ' || !nm_edit[n - 1])) n--;
        memset(set.song, 0, sizeof set.song); memcpy(set.song, nm_edit, n); set_touch();
    }
    store_force = 1; seq_poll(); store_poll(); store_force = 0;   /* (everything now into the storage first) */
    ct_begin("BOMPRJ1");
    for (uint32_t i = 0; i < NPRJ; i++) {
        const char *fn = prj_file(i);
        int sz = vibe_store_size(fn);
        if (sz <= 0) continue;
        if (pj.total + 20u + (uint32_t)sz > PRJ_MAX) { say("SONG TOO BIG"); return; }
        vibe_store_read(fn, prj + pj.total + 20, sz);
        ct_add(fn, prj + pj.total + 20, (uint32_t)sz);
    }
    (void)at; (void)n;
    ct_finish(FK_SONG, "SONG");                 /* (the file is made in prj_poll: the host may say "not yet" first) */
    say("SAVING THE SONG ...");
}
static void prj_restart(void)                   /* the storage's files are new: BOOM as at its start, in place */
{
    ctl_seq_run = 0; ctl_panic = 1; au_rec_stop(); au_arm = 0;
    if (sv.h) { vibe_media_close(sv.h, 0); sv.h = 0; }
    memset(tk, 0, sizeof tk); ld.on = 0; ld_q = tk_save_q = 0; ld_k = sv_k = tk_hold = 0; au_c.type = 0; ctl_au_type = 0;
    pat_used = pats_dirty = snd_dirty = 0; set_dirty = ps_dirty = 0; pat_queue = 0; ctl_pat_swap = 0; ctl_snd_swap = 0; ctl_mx_swap = 0; undo_n = redo_n = 0;
    store_boot();
    for (uint32_t t = 0; t < NTRK; t++) trk_load(t, set.tprog[t]);
    mix_apply();
    seq_boot();
    au_boot();
    memset(seq_cur, 0, sizeof seq_cur); memset(seq_pg, 0, sizeof seq_pg);
}
static void prj_loaded(void)                    /* prj holds a project: its files into the storage, then start over */
{
    uint32_t at = 12, n = rd32(prj + 8);
    static uint8_t have[NPRJ];
    memset(have, 0, sizeof have);
    if (memcmp(prj, "BOMPRJ1", 8)) { say("NOT A BOOM SONG"); return; }
    for (uint32_t k = 0; k < n && at + 20u <= pj.total; k++) {   /* (check it whole first) */
        uint32_t sz = rd32(prj + at + 16);
        if (at + 20u + sz > pj.total) { say("THE SONG FILE IS CUT SHORT"); return; }
        at += 20u + sz;
    }
    at = 12;
    for (uint32_t k = 0; k < n && at + 20u <= pj.total; k++) {
        char name[17]; memcpy(name, prj + at, 16); name[16] = 0;
        uint32_t sz = rd32(prj + at + 16);
        for (uint32_t i = 0; i < NPRJ; i++) if (!strcmp_s(name, prj_file(i))) { vibe_store_write(name, prj + at + 20, (int)sz); have[i] = 1; break; }
        at += 20u + sz;
    }
    for (uint32_t i = 0; i < NPRJ; i++) if (!have[i]) vibe_store_delete(prj_file(i));   /* (not in it: empty, as then) */
    prj_restart();
    if (!set.song[0]) {                         /* (a song saved before it had names: its file's) */
        const char *f = pj.path; for (const char *q = pj.path; *q; q++) if (*q == '/') f = q + 1;
        uint32_t n = slen(f); if (n > 4u && ends_with(f, ".obv")) n -= 4u; if (n > 15u) n = 15u;
        memcpy(set.song, f, n); set.song[n] = 0; set_touch();
    }
    nm_ready = 0;
    { static char m[40]; m[0] = 0; scat(m, "SONG LOADED: "); scat(m, set.song); say(m); }
}
static void ct_loaded(void);
static void prj_poll(void)
{
    if (pj.make) {
        int h = vibe_media_create(pj.fn, pj.path, (int)sizeof pj.path);
        if (h == VIBE_MEDIA_PENDING) return;
        pj.make = 0;
        if (h <= 0) { say("NO CARD TO SAVE ON"); return; }
        pj.h = h;
    }
    if (pj.h) {                                 /* saving */
        for (int k = 0; k < 8 && pj.off < pj.total; k++) {
            uint32_t nb = pj.total - pj.off; if (nb > 65536u) nb = 65536u;
            int r = vibe_media_append(pj.h, prj + pj.off, (int)nb);
            if (r == VIBE_MEDIA_PENDING) return;
            if (r < 0) { vibe_media_close(pj.h, 0); pj.h = 0; say("THE SONG COULD NOT BE SAVED"); return; }
            pj.off += nb;
        }
        if (pj.off >= pj.total) { vibe_media_close(pj.h, 1); pj.h = 0; f_stale = 1; say(pj.kind == FK_SONG ? "SONG SAVED ON THE CARD" : pj.kind == FK_PATTERN ? "PATTERN SAVED ON THE CARD" : pj.kind == FK_MIDI ? "MIDI FILE IN VIBE FM/boom/ – DRAG IT INTO THE DAW" : "PRESET SAVED ON THE CARD"); }
        return;
    }
    if (pj.load) {                              /* loading */
        for (int k = 0; k < 8; k++) {
            uint32_t nb = PRJ_MAX - pj.got; if (nb > 65536u) nb = 65536u;
            if (!nb) { pj.load = 0; say("SONG TOO BIG"); return; }
            int r = vibe_media_read(pj.path, pj.got, prj + pj.got, (int)nb);
            if (r == VIBE_MEDIA_PENDING) return;
            if (r < 0) { pj.load = 0; say("THE SONG IS GONE"); return; }
            if (r == 0) { pj.load = 0; pj.total = pj.got; ct_loaded(); return; }
            pj.got += (uint32_t)r;
        }
    }
}

/* ---- a WAV into a track: read a piece per frame, convert to 16 bit at our rate, the track's channels */
static struct { uint32_t on, t, pat, k, cap, fmt, ch, bits, rate, data_at, bytes, off, out, st, carry; float pa, pb; double pos, step; uint64_t src_i; char path[VIBE_MEDIA_PATH]; } wl;
static void wav_load_start(const char *path, uint32_t t, uint32_t pat)
{
    aud_t *a = &paud[pat % NPAT];
    if (!au_arena) { say("NO MEMORY FOR AUDIO TRACKS HERE"); return; }
    t %= NAU;
    if (ctl_au_rq) { say("NOT WHILE A TAKE IS RECORDED"); return; }
    if (ld_k || ld_q) { say("THE TAKES ARE STILL LOADING"); return; }
    if (wl.on) return;
    memset(&wl, 0, sizeof wl);
    memcpy(wl.path, path, slen(path) + 1);
    wl.t = t; wl.pat = pat % NPAT; wl.on = 1;
    if (!(a->type >> t & 1u) && !(wl.pat == set.pat % NPAT && trk_type(t) == T_SMP)) { a->type |= (uint8_t)(1u << t); aud_dirty |= 1ull << wl.pat;   /* (0.20: onto a SAMPLER: its sample, not an audio track) */ store_t = o3_ms; if (wl.pat == set.pat % NPAT) { ctl_panic = 1; au_apply(); } }
    say("LOADING THE WAV ...");
}
static float wav_sample(const uint8_t *p, uint32_t fmt, uint32_t bits)
{
    if (bits == 16) return (float)(int16_t)(p[0] | p[1] << 8) * (1.0f / 32768.0f);
    if (bits == 24) return (float)((int32_t)((uint32_t)p[0] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 24) >> 8) * (1.0f / 8388608.0f);
    if (bits == 32 && fmt == 3) { float f; memcpy(&f, p, 4); return f == f ? f : 0.0f; }
    if (bits == 32) return (float)(int32_t)rd32(p) * (1.0f / 2147483648.0f);
    if (bits == 8) return (float)((int)p[0] - 128) * (1.0f / 128.0f);
    return 0.0f;
}
static void wav_load_end(int ok)
{
    static char msg[48];
    uint32_t t = wl.t, k = wl.k;
    aud_t *a = &paud[wl.pat % NPAT];
    wl.on = 0; tk_hold = 0;
    if (ok && wl.out && k) {                    /* the new take is the track's (in the pattern it was loaded in) */
        tk[k].hi = wl.out; take_trim(k, wl.out);
        a->take[t] = (uint16_t)k; a->len[t] = au_up(wl.out);
        aud_dirty |= 1ull << (wl.pat % NPAT); store_t = o3_ms;
        tk_save_q |= 1ull << (k - 1u);
        take_gc();
        msg[0] = 0; scat(msg, "P"); fmt_u(msg + slen(msg), wl.pat % NPAT + 1u); scat(msg, " TRACK "); { char l[2] = {(char)('A' + t), 0}; scat(msg, l); } scat(msg, ": WAV LOADED · "); fmt_time(msg + slen(msg), wl.out);
        if (wl.rate != obv_fs) scat(msg, " (RATE CONVERTED)");
        say(msg);
    }
    else if (k) tk_free(k);
    if (wl.pat == set.pat % NPAT) au_apply();
}
static void wav_poll(void)
{
    static uint8_t b[65536 + 128];   /* (+ the rest of a frame: up to 16 channels of 32 bit) */
    if (!wl.on) return;
    if (!wl.data_at) {                          /* the head */
        int r = vibe_media_read(wl.path, 0, b, 4096);
        if (r == VIBE_MEDIA_PENDING) return;
        if (r < 44 || memcmp(b, "RIFF", 4) || memcmp(b + 8, "WAVE", 4)) { say("NOT A WAV FILE"); wav_load_end(0); return; }
        uint32_t at = 12;
        while (at + 8u <= (uint32_t)r) {
            uint32_t sz = rd32(b + at + 4);
            if (!memcmp(b + at, "fmt ", 4) && at + 24u <= (uint32_t)r) {
                wl.fmt = b[at + 8] | (uint32_t)b[at + 9] << 8; wl.ch = b[at + 10] | (uint32_t)b[at + 11] << 8;
                wl.rate = rd32(b + at + 12); wl.bits = b[at + 22] | (uint32_t)b[at + 23] << 8;
                if (wl.fmt == 0xFFFEu && at + 34u <= (uint32_t)r) wl.fmt = b[at + 32] | (uint32_t)b[at + 33] << 8;   /* (EXTENSIBLE: its sub-format) */
            } else if (!memcmp(b + at, "data", 4)) { wl.data_at = at + 8u; wl.bytes = sz; break; }
            at += 8u + sz + (sz & 1u);
        }
        if (!wl.data_at || !wl.ch || (wl.fmt != 1u && wl.fmt != 3u) || (wl.bits != 8u && wl.bits != 16u && wl.bits != 24u && wl.bits != 32u) || wl.rate < 8000u) {
            say("THIS WAV CANNOT BE READ (16/24/32 BIT, FLOAT)"); wav_load_end(0); return;
        }
        if (wl.ch * (wl.bits / 8u) > 64u) { say("TOO MANY CHANNELS IN THIS WAV"); wav_load_end(0); return; }
        wl.st = wl.ch >= 2u;                    /* (the take is the file's: mono or stereo) */
        wl.step = (double)wl.rate / (double)obv_fs;
        {                                       /* its block: as long as the file at our rate, and a little */
            uint64_t fr = (uint64_t)(wl.bytes / (wl.ch * (wl.bits / 8u))) * obv_fs / wl.rate + 8u;
            uint32_t ch = wl.st ? 2u : 1u, v = fr * ch > AU_ARENA ? AU_ARENA : (uint32_t)(fr * ch);
            wl.k = take_new(v, 0, wl.st);
            if (!wl.k) { wl.k = take_new(ch, 1, wl.st); if (wl.k) say("NOT ROOM FOR ALL OF IT – CUT"); }
            if (!wl.k) { say("THE MEMORY FOR TAKES IS FULL"); wav_load_end(0); return; }
            tk_hold = wl.k; wl.cap = tk[wl.k].cap / ch;
        }
        wl.pos = 0.0; wl.src_i = 0; wl.pa = wl.pb = 0.0f;
    }
    uint32_t fb = wl.ch * (wl.bits / 8u), cap = wl.cap, chn = wl.st ? 2u : 1u;
    int16_t *pl = au_arena + tk[wl.k].off;
    for (int k = 0; k < 16 && wl.off < wl.bytes; k++) {   /* (up to 1 MB a frame) */
        uint32_t nb = wl.bytes - wl.off; if (nb > 65536u) nb = 65536u;
        /* (a read may stop anywhere – at the host's block edge: mid-frame. The rest of a frame waits in b) */
        int r = vibe_media_read(wl.path, (uint64_t)wl.data_at + wl.off, b + wl.carry, (int)nb);
        if (r == VIBE_MEDIA_PENDING) return;
        if (r <= 0) { wl.bytes = wl.off; break; }
        wl.off += (uint32_t)r;
        uint32_t have = wl.carry + (uint32_t)r, frames = have / fb;
        for (uint32_t i = 0; i < frames; i++) {
            const uint8_t *p = b + i * fb;
            float a = wav_sample(p, wl.fmt, wl.bits), c = wl.ch >= 2u ? wav_sample(p + wl.bits / 8u, wl.fmt, wl.bits) : a;
            double si = (double)wl.src_i;
            while (wl.pos <= si) {              /* (linear between the frame before and this one) */
                float fr = (float)(wl.pos - (si - 1.0)), l = wl.pa + (a - wl.pa) * fr, rr = wl.pb + (c - wl.pb) * fr;
                if (wl.out >= cap) { say("THE WAV IS LONGER THAN THE TRACK – CUT"); wl.bytes = wl.off; goto done; }
                if (chn == 2u) { pl[2u * wl.out] = (int16_t)sat16((int32_t)(l * 32767.0f)); pl[2u * wl.out + 1u] = (int16_t)sat16((int32_t)(rr * 32767.0f)); }
                else pl[wl.out] = (int16_t)sat16((int32_t)((l + rr) * 0.5f * 32767.0f));
                wl.out++; wl.pos += wl.step;
            }
            wl.pa = a; wl.pb = c; wl.src_i++;
        }
        wl.carry = have - frames * fb;
        if (wl.carry) memmove(b, b + frames * fb, wl.carry);
    }
    if (wl.off < wl.bytes) return;
done:
    wav_load_end(1);
}

/* ---- PRESET: one sound ("BOMPRE1": "patch" = patch_t) – the selected track's; loaded, it is the track's sound, edited */
static void pre_save(void)
{
    char dflt[PATCH_NAME + 1];
    if (pj.h || pj.load || pj.make) return;
    ct_begin("BOMPRE1");
    ct_add("patch", &live_p[sel % NTRK], sizeof(patch_t));
    prog_name(trk_prog[sel % NTRK], dflt);
    ct_finish(FK_PRESET, dflt[0] ? dflt : "PRESET");
    say("SAVING THE PRESET ...");
}
static void pre_loaded(void)
{
    uint32_t sz; const uint8_t *p = ct_find("patch", &sz);
    if (!p || sz != sizeof(patch_t)) { say("NOT A BOOM PRESET"); return; }
    memcpy(&live_p[sel % NTRK], p, sizeof(patch_t));
    if (!(sel % NTRK)) fx_keep(&live_p[0]);
    trk_edited[sel % NTRK] = 1;
    { static char m[40]; char n[PATCH_NAME + 1]; uint32_t i; for (i = 0; i < PATCH_NAME; i++) n[i] = (char)live_p[sel % NTRK].v[NPARAM + i]; n[PATCH_NAME] = 0;
      m[0] = 0; scat(m, "PRESET LOADED: "); scat(m, n); say(m); }
}

/* ---- PATTERN: the pattern with its sounds, mixer and audio ("BOMPAT1": "blob" pattern_t, "psnd" psnd_t, "paud" aud_t,
 * "wavA".."wavD" the file names of its takes, written as WAVs beside it first – the host names them, so the file waits
 * for them). Loaded into the slot now (stopped): the takes come back through the WAV loader, one track after the other. */
static struct { uint32_t on, wait, k[NAU]; char nm[NM_LEN + 1]; } ps;   /* saving a pattern: wait = its takes' WAVs first */
static struct { uint32_t pend, pat; char path[NAU][VIBE_MEDIA_PATH]; } pl;   /* a loaded pattern's WAVs still to load (bit t), into pattern pat */
static void pat_save(void)
{
    static char m[48];
    uint32_t t, any = 0;
    if (pj.h || pj.load || pj.make || ps.on) return;
    if (tk_save_q || sv_k || ctl_au_rq || ld_k || ld_q) { say("A TAKE IS STILL BEING SAVED – AGAIN IN A MOMENT"); return; }
    ct_name(ps.nm, "PATTERN", ""); ps.on = 1; ps.wait = 0;
    for (t = 0; t < NAU; t++) {                 /* its takes as "<NAME> <T>.wav" (the names the host gives come into the file) */
        uint32_t k = (AU.type >> t & 1u) ? au_k(t) : 0u;
        ps.k[t] = k;
        if (!k || !tk[k].hi) { ps.k[t] = 0; continue; }
        tk_name_over[k][0] = 0; scat(tk_name_over[k], ps.nm); { char l[3] = {' ', (char)('A' + t), 0}; scat(tk_name_over[k], l); } scat(tk_name_over[k], ".wav");
        tk_save_q |= 1ull << (k - 1u); ps.wait = 1; any++;
    }
    m[0] = 0; scat(m, "SAVING THE PATTERN"); if (any) scat(m, " AND ITS AUDIO"); scat(m, " ..."); say(m);
}
static void pat_save_poll(void)                 /* the WAVs are written: now the pattern's file */
{
    if (!ps.on || (ps.wait && (tk_save_q || sv_k))) return;
    ps.on = 0;
    pats[set.pat] = pat; snd_capture(set.pat); aud_dirty |= 1ull << (set.pat % NPAT);
    ct_begin("BOMPAT1");
    { static uint8_t pb[sizeof(pattern_t) + 64u], sb[sizeof(psnd_t) + 256u];   /* (BOOM 0.4: the versioned pieces, bm_save.h) */
      ct_add("blob", pb, pat_ser(&pats[set.pat], pb)); ct_add("psnd", sb, psnd_ser(&psnd[set.pat], sb)); }
    { static uint8_t ab[sizeof(aud_t) + 64u]; ct_add("paud", ab, aud_ser(&AU, ab)); }
    for (uint32_t t = 0; t < NAU; t++) {
        uint32_t k = ps.k[t]; const char *f, *q;
        if (!k || !takes2.e[k].path[0]) continue;
        f = takes2.e[k].path; for (q = f; *q; q++) if (*q == '/') f = q + 1;
        { char nm[5] = "wavA"; nm[3] = (char)('A' + t); ct_add(nm, f, slen(f) + 1u); }
    }
    memset(nm_edit, ' ', NM_LEN); memcpy(nm_edit, ps.nm, slen(ps.nm) > NM_LEN ? NM_LEN : slen(ps.nm));
    ct_finish(FK_PATTERN, "PATTERN");
}
static void pat_loaded(void)
{
    uint32_t sz, n = f_tpat % NPAT, here = n == set.pat % NPAT; const uint8_t *p;
    patch_t nx[NTRK];
    if (here && ctl_seq_run) { say("STOP FIRST – A PATTERN LOADS WHILE STOPPED"); return; }
    if (wl.on || pl.pend) { say("A WAV IS STILL LOADING"); return; }
    p = ct_find("blob", &sz);
    { static pattern_t tmp; if (!p || pat_des(p, sz, &tmp)) { say("NOT A BOOM PATTERN"); return; }
      if (here) { ctl_panic = 1; au_rec_stop(); au_arm = 0; }
      pats[n] = tmp; }
    p = ct_find("psnd", &sz);
    if (!p || psnd_des(p, sz, &psnd[n])) psnd[n].magic = 0;
    p = ct_find("paud", &sz);
    if (!p || aud_des(p, sz, &paud[n])) aud_init(&paud[n]);
    for (uint32_t t = 0; t < NAU; t++) paud[n].take[t] = 0;   /* (its takes are the other machine's: the WAVs bring them) */
    pats_dirty |= 1ull << n; snd_dirty |= 1ull << n; aud_dirty |= 1ull << n; pat_used |= 1ull << n; store_t = o3_ms;
    if (here) {
        pat = pats[n];
        if (psnd[n].magic == PSN2_MAGIC) { snd_target(n, nx); for (uint32_t t = 0; t < NTRK; t++) live_p[t] = nx[t]; }
        snd_meta(n); mix_apply(); au_pattern(); undo_n = redo_n = 0;
        memset(seq_cur, 0, sizeof seq_cur); memset(seq_pg, 0, sizeof seq_pg); pat_edited = 0; set_touch();
    }
    pl.pend = 0; pl.pat = n;
    for (uint32_t t = 0; t < NAU; t++) {       /* its WAVs, from the folder the pattern came from */
        char nm[5] = "wavA"; nm[3] = (char)('A' + t);
        p = ct_find(nm, &sz);
        if (!p || !sz || !(paud[n].type >> t & 1u)) continue;
        { const char *e = pj.path, *q; for (q = pj.path; *q; q++) if (*q == '/') e = q + 1;
          uint32_t d = (uint32_t)(e - pj.path); if (d + sz + 1u >= sizeof pl.path[t]) continue;
          memcpy(pl.path[t], pj.path, d); memcpy(pl.path[t] + d, p, sz); pl.path[t][d + sz] = 0; pl.pend |= 1u << t; }
    }
    { static char m[40]; m[0] = 0; scat(m, "PATTERN LOADED INTO "); fmt_u(m + slen(m), n + 1u); if (pl.pend) scat(m, " – ITS AUDIO FOLLOWS"); say(m); }
}
static void pat_load_poll(void)                 /* the loaded pattern's WAVs, one after the other */
{
    if (!pl.pend || wl.on || ld_k || ld_q) return;
    for (uint32_t t = 0; t < NAU; t++) if (pl.pend >> t & 1u) { pl.pend &= ~(1u << t); wav_load_start(pl.path[t], t, pl.pat); return; }
}
/* (3.9, Sven: "alles leeren") NEW SONG: the 64 patterns with their sounds, mixers and audio go; the settings (but the song's
 * name) and the user's programs stay; the takes' WAVs stay on the card. Then BOOM starts over from the storage, in place. */
static void prj_new(void)
{
    if (tk_save_q || sv_k || ctl_au_rq || ld_k || ld_q || wl.on || pl.pend) { say("A TAKE IS STILL BEING WRITTEN – AGAIN IN A MOMENT"); return; }
    ctl_seq_run = 0;
    for (uint32_t n = 0; n < NPAT; n++) { vibe_store_delete(PB_FILE_N(n)); vibe_store_delete(PS_FILE_N(n)); vibe_store_delete(pat_fname("paud", n)); }
    vibe_store_delete("takes"); vibe_store_delete("takes2");
    memset(set.song, 0, sizeof set.song); set.pat = 0; set_touch(); store_force = 1; store_poll(); store_force = 0;
    memset(&takes2, 0, sizeof takes2);
    prj_restart();
    nm_ready = 0; f_stale = 1;
    say("NEW SONG – EVERYTHING EMPTY");
}
/* ---- (BOOM 0.6) a DX7 bank: the 32-voice bulk dump (.syx, 4104 bytes: F0 43 0n 09 20 00, 32 x 128 packed bytes, sum, F7) –
 * each voice unpacked (156 bytes) into a DX sound of its name in the program slots 129-160, as STORE would put them there */
static void dx_unpack(const uint8_t *b, uint8_t *u)
{
    static const uint8_t OPMAX[21] = {99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 3, 3, 7, 3, 7, 99, 1, 31, 99, 14};
    static const uint8_t GMAX[19] = {99, 99, 99, 99, 99, 99, 99, 99, 31, 7, 1, 99, 99, 99, 99, 1, 5, 7, 48};
    uint32_t op, i;
    for (op = 0; op < 6u; op++) {
        const uint8_t *s = b + op * 17u; uint8_t *o = u + op * 21u;
        for (i = 0; i < 11u; i++) o[i] = s[i];
        o[11] = s[11] & 3u; o[12] = (s[11] >> 2) & 3u; o[13] = s[12] & 7u; o[20] = (s[12] >> 3) & 15u;
        o[14] = s[13] & 3u; o[15] = (s[13] >> 2) & 7u; o[16] = s[14]; o[17] = s[15] & 1u; o[18] = (s[15] >> 1) & 31u; o[19] = s[16];
    }
    for (i = 0; i < 8u; i++) u[126 + i] = b[102 + i];
    u[134] = b[110] & 31u; u[135] = b[111] & 7u; u[136] = (b[111] >> 3) & 1u;
    for (i = 0; i < 4u; i++) u[137 + i] = b[112 + i];
    u[141] = b[116] & 1u; u[142] = (b[116] >> 1) & 7u; u[143] = (b[116] >> 4) & 7u; u[144] = b[117];
    for (i = 0; i < 10u; i++) u[145 + i] = b[118 + i];
    u[155] = 0;
    for (op = 0; op < 6u; op++) for (i = 0; i < 21u; i++) if (u[op * 21u + i] > OPMAX[i]) u[op * 21u + i] = OPMAX[i];   /* (a damaged dump never indexes past a table) */
    for (i = 0; i < 19u; i++) if (u[126 + i] > GMAX[i]) u[126 + i] = GMAX[i];
}
static void dx_bank_loaded(void)
{
    const uint8_t *s = prj; uint32_t n = pj.total;
    if (n < 4104u || s[0] != 0xF0 || s[1] != 0x43 || (s[2] & 0xF0) != 0x00 || s[3] != 0x09 || s[4] != 0x20 || s[5] != 0x00) { say("NOT A DX7 BANK (32 VOICES)"); return; }
    for (uint32_t v = 0; v < 32u && PROG_DX0 + v < NPROG; v++) {
        patch_t p; uint32_t i;
        patch_defaults(&p); p.v[P_TYPE] = T_DX;
        dx_unpack(s + 6u + v * 128u, p.dx);
        for (i = 0; i < PATCH_NAME; i++) { uint8_t c = p.dx[145u + i]; p.v[NPARAM + i] = c >= 32u && c < 127u ? (c >= 'a' && c <= 'z' ? (uint8_t)(c - 32) : c) : ' '; }
        prog_put(PROG_DX0 + v, &p);
    }
    if (trk_type(sel) == T_DX) { trk_load(sel, PROG_DX0); set_touch(); }
    say("DX BANK LOADED: 32 SOUNDS IN SLOTS 129-160 (ENCODER B)");
}
static void ct_loaded(void)                     /* prj holds a file from the card: by its magic */
{
    if (pj.total >= 6u && prj[0] == 0xF0 && prj[1] == 0x43) { dx_bank_loaded(); return; }   /* (0.6) a DX7 bank */
    if (!memcmp(prj, "BOMPRJ1", 8)) prj_loaded();
    else if (!memcmp(prj, "BOMPRE1", 8)) pre_loaded();
    else if (!memcmp(prj, "BOMPAT1", 8)) pat_loaded();
    else say("NOT A BOOM FILE");
}
/* ---- AUDIO: the chosen take (the list of the takes in memory; DATA picks) as "<NAME>.wav" */
static uint32_t f_take_id(uint32_t i)            /* the take a 't' entry names, 0 = none */
{
    uint32_t k = 0;
    if (i >= f_n || f_ent[i].kind != 't') return 0;
    for (const char *q = f_ent[i].name; *q >= '0' && *q <= '9'; q++) k = k * 10u + (uint32_t)(*q - '0');
    return k && k <= NTAKE && tk[k].inuse ? k : 0u;
}
static void aud_save(void)
{
    uint32_t k = f_take_id(f_cur);
    if (!k || !tk[k].hi) { say(f_n ? "CHOOSE A TAKE WITH DATA" : "NO AUDIO IN MEMORY YET"); return; }
    if (tk_save_q || sv_k || ctl_au_rq) { say("A TAKE IS STILL BEING SAVED – AGAIN IN A MOMENT"); return; }
    ct_name(tk_name_over[k], "TRACK", ".wav");
    tk_save_q |= 1ull << (k - 1u);
    say("SAVING THE AUDIO ...");
}

/* ---- the page */
static int file_there(const char *name)        /* "<name><ext of the kind>" in the list */
{
    for (uint32_t i = 0; i < f_n; i++) {
        const char *a = f_ent[i].name, *b = name;
        while (*b && *a == *b) { a++; b++; }
        if (!*b && !strcmp_s(a, FK_EXT[fk % NFK])) return 1;
    }
    return 0;
}
static void nm_offer(void)                      /* SAVE: a name to start from – the song's, the sound's, "<SONG> P<n>", "TRACK A" */
{
    char t[24];
    memset(nm_edit, ' ', NM_LEN); nm_edit[NM_LEN] = 0;
    t[0] = 0;
    if (fk == FK_PRESET) prog_name(trk_prog[sel % NTRK], t);
    else if (fk == FK_PATTERN || fk == FK_MIDI) { if (set.song[0]) { scat(t, set.song); t[8] = 0; scat(t, " "); } scat(t, "P"); fmt_u(t + slen(t), set.pat % NPAT + 1u); }
    else if (fk == FK_AUDIO) {                  /* "P3 B" from the chosen take's label "P3 B · 0:04" */
        const char *l = f_cur < f_n ? f_lab[f_cur] : "TAKE";
        if (set.song[0]) { scat(t, set.song); t[7] = 0; scat(t, " "); }
        for (uint32_t i = 0, o = slen(t); l[i] && i < 8u && !(l[i] == ' ' && (uint8_t)l[i + 1] == 0xC2u); i++) { t[o + i] = l[i]; t[o + i + 1] = 0; }
    }
    else if (set.song[0]) scat(t, set.song);
    else for (uint32_t n = 1; n < 100u; n++) {
        t[0] = 0; scat(t, "SONG "); t[5] = (char)('0' + n / 10u); t[6] = (char)('0' + n % 10u); t[7] = 0;
        if (!file_there(t) || n == 99u) break;
    }
    for (uint32_t i = 0; i < NM_LEN && t[i]; i++) nm_edit[i] = t[i];
    for (uint32_t i = 0; i < NM_LEN; i++) { char c = nm_edit[i]; if (c >= 'a' && c <= 'z') nm_edit[i] = (char)(c - 32); else if (c < ' ' || c > '~') nm_edit[i] = ' '; }
    nm_pos = 0; nm_ready = 1;
}
static void files_poll(void)
{
    prj_poll(); wav_poll(); pat_save_poll(); pat_load_poll();
    if (files_on) {
        static uint32_t sel_was = ~0u;
        if (sel != sel_was) { sel_was = sel; f_ttrk = sel % NAU; if (F_BROWSE && !f_dir[0]) f_stale = 1; if (F_SAVE && fk != FK_SONG) nm_ready = 0; }   /* (another track: the target, its takes, its name) */
        f_refresh(); if (F_SAVE && !f_stale && !nm_ready) nm_offer();
    }
}
static void files_kind(uint32_t k) { fk = k % NFK; f_cur = 0; f_stale = 1; f_arm = 0; nm_ready = 0; }
static void files_open(uint32_t pg)             /* F3 LOAD / F4 SAVE; the same key again: the next kind */
{
    if (files_on && f_page == pg) { files_kind(fk + 1u); return; }
    files_on = 1; f_page = pg; f_tpat = set.pat % NPAT; f_ttrk = sel % NAU; files_kind(fk);
}
static void nm_char(int s)                      /* the character under the cursor, s steps through NM_SET */
{
    uint32_t n = sizeof NM_SET - 1u, k = 0;
    while (k < n && NM_SET[k] != nm_edit[nm_pos]) k++;
    k = (uint32_t)(((int32_t)(k % n) + s % (int32_t)n + (int32_t)n) % (int32_t)n);
    nm_edit[nm_pos] = NM_SET[k];
}
static void files_turn(int id, int s)
{
    if (id == VE_1) { files_kind((uint32_t)clampi((int32_t)fk + (s > 0 ? 1 : -1), 0, NFK - 1)); return; }
    if (F_SAVE) {                               /* the name: KNOB 2 the place, KNOB 3 / DATA the character (AUDIO: DATA the take) */
        if (F_TAKES && id == VE_DATA) { if (f_n) { uint32_t was = f_cur; f_cur = (uint32_t)clampi((int32_t)f_cur + s, 0, (int32_t)f_n - 1); if (f_cur != was) nm_ready = 0; } return; }
        if (id == VE_2) nm_pos = (uint32_t)clampi((int32_t)nm_pos + (s > 0 ? 1 : -1), 0, NM_LEN - 1);
        else if (id == VE_3 || id == VE_DATA) nm_char(s);
        return;
    }
    if (id == VE_2 || id == VE_DATA) { if (f_n) f_cur = (uint32_t)clampi((int32_t)f_cur + s, 0, (int32_t)f_n - 1); f_arm = 0; }
    if (fk == FK_PATTERN || fk == FK_AUDIO) {   /* (3.9, Sven: "Load to Pattern XY Spur Z") the target */
        if (id == VE_3) { f_tpat = (uint32_t)clampi((int32_t)f_tpat + s, 0, NPAT - 1); f_arm = 0; }
        if (id == VE_4 && fk == FK_AUDIO) { f_ttrk = (uint32_t)clampi((int32_t)f_ttrk + (s > 0 ? 1 : -1), 0, NAU - 1); f_arm = 0; }
    }
}
/* ---- (BOOM 0.4) MIDI: the pattern now as a Standard MIDI File (type 1, 480 ticks a quarter) – a tempo track, then one
 * track a sounding track (its name, channel 1-6 = A-F): every step with its notes (transposed), velocity, gate, ties,
 * ratchets, nudge and the track's swing; shorter tracks go round until the longest has played once. For the DAW: the
 * clips to go on with there. An audio track and a switched-off one are left out. */
static uint32_t mid_at;
static void mid_b(uint32_t v) { if (mid_at < PRJ_MAX) prj[mid_at++] = (uint8_t)v; }
static void mid_var(uint32_t v) { uint8_t b[4]; int n = 0; do { b[n++] = (uint8_t)(v & 127u); v >>= 7; } while (v && n < 4); while (n--) mid_b(b[n] | (n ? 0x80u : 0u)); }
static void mid_str(const char *s) { while (*s) mid_b((uint8_t)*s++); }
typedef struct { uint32_t tick; uint8_t on, note, vel; } midev_t;
#define MID_EV 16384u
static midev_t mid_ev[MID_EV];
static uint32_t mid_n;
static void mid_add(uint32_t tick, uint32_t on, uint32_t note, uint32_t vel) { if (mid_n < MID_EV) { mid_ev[mid_n].tick = tick; mid_ev[mid_n].on = (uint8_t)on; mid_ev[mid_n].note = (uint8_t)note; mid_ev[mid_n].vel = (uint8_t)vel; mid_n++; } }
static uint32_t mid_step_ticks(uint32_t t) { return 480u / DIV_PER_QUARTER[pat.trk[t].div % NDIV]; }
static uint32_t mid_step_at(uint32_t t, uint32_t i, uint32_t *dur)   /* step i's start in ticks (with the swing), its length */
{
    uint32_t base = mid_step_ticks(t), sw = base * (uint32_t)(clampi(pat.trk[t].swing, 50, 75) - 50) / 50u;
    *dur = i & 1u ? base - sw : base + sw;
    return i / 2u * 2u * base + (i & 1u ? base + sw : 0u);
}
static void mid_save(void)
{
    uint32_t t, total = 0, ntr = 0, len_at, bar = song_e8() * 240u;
    if (pj.h || pj.load || pj.make) return;
    for (t = 0; t < NTRK; t++) if (pat.trk[t].len && !au_is(t)) { uint32_t L = pat.trk[t].len * mid_step_ticks(t); if (L > total) total = L; ntr++; }
    if (!ntr) { say("NOTHING TO EXPORT: NO TRACK HAS STEPS"); return; }
    total = (total + bar - 1u) / bar * bar;      /* (whole bars of the song's meter) */
    mid_at = 0;
    mid_str("MThd"); mid_b(0); mid_b(0); mid_b(0); mid_b(6); mid_b(0); mid_b(1); mid_b(0); mid_b(ntr + 1u); mid_b(480u >> 8); mid_b(480u & 255u);
    mid_str("MTrk"); len_at = mid_at; mid_at += 4;                                   /* the tempo track */
    { uint32_t us = 60000000u / (set.tempo ? set.tempo : 120u), m = meter_of(0);
      mid_b(0); mid_b(0xFF); mid_b(0x51); mid_b(3); mid_b(us >> 16); mid_b(us >> 8 & 255u); mid_b(us & 255u);
      mid_b(0); mid_b(0xFF); mid_b(0x58); mid_b(4); mid_b(METER[m].den8 ? METER[m].e8 : METER[m].e8 / 2u); mid_b(METER[m].den8 ? 3 : 2); mid_b(24); mid_b(8);
      mid_var(total); mid_b(0xFF); mid_b(0x2F); mid_b(0); }
    put32(prj + len_at, 0); { uint32_t n = mid_at - len_at - 4u; prj[len_at] = (uint8_t)(n >> 24); prj[len_at + 1] = (uint8_t)(n >> 16); prj[len_at + 2] = (uint8_t)(n >> 8); prj[len_at + 3] = (uint8_t)n; }
    for (t = 0; t < NTRK; t++) {
        uint32_t len = pat.trk[t].len, L, rounds, r, i, k, last = 0;
        char nm[24], pn[PATCH_NAME + 1];
        if (!len || au_is(t)) continue;
        L = len * mid_step_ticks(t); rounds = (total + L - 1u) / L; mid_n = 0;
        for (r = 0; r < rounds; r++) for (i = 0; i < len; i++) {
            const step_t *s = seq_step(t, i);
            uint32_t dur, at = r * L + mid_step_at(t, i, &dur), rat = (s->ratch & 3u) + 1u, sub, h;
            if (!s->note[0] || (s->ratch & STEP_OFF) || at >= total) continue;
            at += (dur * s->nudge) >> 7; sub = (dur - ((dur * s->nudge) >> 7)) / rat;
            for (h = 0; h < rat; h++) for (k = 0; k < SEQ_CHORD && s->note[k]; k++) {
                uint32_t n = (uint32_t)clampi((int32_t)s->note[k] + pat.trk[t].transp, 1, 127), g = (dur / rat * (s->gate + 1u)) >> 7, on = at + h * sub, j;
                if (s->gate >= GATE_TIE) {       /* a tie: on through the next steps that play the note again */
                    g = dur;
                    for (j = 1; j < len; j++) {
                        const step_t *q = seq_step(t, (i + j) % len); uint32_t d2, has = 0, c;
                        mid_step_at(t, (i + j) % len, &d2);
                        for (c = 0; c < SEQ_CHORD && q->note[c]; c++) if (q->note[c] == s->note[k]) has = 1;
                        if (!has || (q->ratch & STEP_OFF)) break;
                        g += q->gate >= GATE_TIE ? d2 : (d2 * (q->gate + 1u)) >> 7;
                        if (q->gate < GATE_TIE) break;
                    }
                    if (i && seq_step(t, i - 1u)->gate >= GATE_TIE) { uint32_t c, had = 0; const step_t *q = seq_step(t, i - 1u); for (c = 0; c < SEQ_CHORD && q->note[c]; c++) if (q->note[c] == s->note[k]) had = 1; if (had && !(q->ratch & STEP_OFF)) continue; }
                } else if (i && seq_step(t, i - 1u)->gate >= GATE_TIE) {   /* (the end of a tie: its note is already sounding) */
                    uint32_t c, had = 0; const step_t *q = seq_step(t, i - 1u);
                    for (c = 0; c < SEQ_CHORD && q->note[c]; c++) if (q->note[c] == s->note[k]) had = 1;
                    if (had && !(q->ratch & STEP_OFF)) continue;
                }
                if (g < 10u) g = 10u;
                if (on + g > total) g = total - on;
                { uint32_t rn = ROLL_N[s->nr[k] % 6u], vv = s->nv[k] ? s->nv[k] : s->vel ? s->vel : 100u, rs = dur / rat / rn, q;   /* (0.5: its own velocity, its roll as notes) */
                  if (rn < 2u || !rs) { mid_add(on, 1, n, vv); mid_add(on + g, 0, n, 0); }
                  else for (q = 0; q < rn; q++) { uint32_t o2 = on + q * rs, g2 = rs > 20u ? rs - 10u : rs; if (o2 >= total) break; if (o2 + g2 > total) g2 = total - o2; mid_add(o2, 1, n, vv); mid_add(o2 + g2, 0, n, 0); } }
            }
        }
        for (i = 1; i < mid_n; i++) {            /* by time, the offs first (insertion sort: nearly in order already) */
            midev_t e = mid_ev[i]; uint32_t j = i;
            while (j && (mid_ev[j - 1u].tick > e.tick || (mid_ev[j - 1u].tick == e.tick && mid_ev[j - 1u].on > e.on))) { mid_ev[j] = mid_ev[j - 1u]; j--; }
            mid_ev[j] = e;
        }
        mid_str("MTrk"); len_at = mid_at; mid_at += 4;
        nm[0] = (char)('A' + t); nm[1] = ' '; nm[2] = 0; prog_name(trk_prog[t], pn); scat(nm, pn);
        mid_b(0); mid_b(0xFF); mid_b(0x03); mid_b(slen(nm)); mid_str(nm);
        for (i = 0; i < mid_n; i++) { mid_var(mid_ev[i].tick - last); last = mid_ev[i].tick; mid_b((mid_ev[i].on ? 0x90u : 0x80u) | t); mid_b(mid_ev[i].note); mid_b(mid_ev[i].vel); }
        mid_var(total - last); mid_b(0xFF); mid_b(0x2F); mid_b(0);
        { uint32_t n = mid_at - len_at - 4u; prj[len_at] = (uint8_t)(n >> 24); prj[len_at + 1] = (uint8_t)(n >> 16); prj[len_at + 2] = (uint8_t)(n >> 8); prj[len_at + 3] = (uint8_t)n; }
    }
    pj.total = mid_at; pj.off = 0; pj.kind = FK_MIDI;
    ct_name(pj.fn, "PATTERN", FK_EXT[FK_MIDI]);
    pj.make = 1;
    say("WRITING THE MIDI FILE ...");
}
static void files_enter(void)
{
    char path[VIBE_MEDIA_PATH];
    if (F_SAVE) { if (fk == FK_SONG) prj_save(); else if (fk == FK_PATTERN) pat_save(); else if (fk == FK_PRESET) pre_save(); else if (fk == FK_MIDI) mid_save(); else if (fk == FK_DX) say("DX BANKS ARE LOADED HERE, NOT SAVED: F3 LOAD"); else aud_save(); return; }
    if (fk == FK_MIDI) { say("MIDI FILES ARE FOR THE DAW: DRAG THEM IN FROM VIBE FM/boom/"); return; }
    if (!f_n || f_cur >= f_n) return;
    if (!F_BROWSE) {                            /* PRESET / PATTERN / SONG: the file into prj, then by its magic */
        if (pj.load || pj.h || pj.make) return;
        if (f_ent[f_cur].kind == 'n') {         /* NEW SONG, ENTER twice */
            if (f_arm != f_cur + 1u || o3_ms - f_arm_t > 4000u) { f_arm = f_cur + 1u; f_arm_t = o3_ms; say("ENTER AGAIN: EVERY PATTERN AND ITS AUDIO IS CLEARED"); return; }
            f_arm = 0; prj_new(); return;
        }
        if (fk != FK_PRESET && (f_arm != f_cur + 1u || o3_ms - f_arm_t > 4000u)) {
            f_arm = f_cur + 1u; f_arm_t = o3_ms;
            say(fk == FK_SONG ? "ENTER AGAIN: LOAD – EVERYTHING NOW IS REPLACED" : fk == FK_DX ? "ENTER AGAIN: THE 32 DX SLOTS TAKE THIS BANK" : ctl_seq_run && f_tpat % NPAT == set.pat % NPAT ? "STOP FIRST – THEN ENTER AGAIN" : "ENTER AGAIN: THAT PATTERN IS REPLACED");
            return;
        }
        f_arm = 0;
        pj.path[0] = 0; scat(pj.path, f_wkey); scat(pj.path, "/VIBE FM/boom/"); scat(pj.path, f_ent[f_cur].name);
        pj.load = 1; pj.got = 0; pj.kind = fk; say(fk == FK_SONG ? "LOADING THE SONG ..." : fk == FK_PATTERN ? "LOADING THE PATTERN ..." : fk == FK_DX ? "LOADING THE DX BANK ..." : "LOADING THE PRESET ...");
        return;
    }
    /* LOAD AUDIO: a take in memory into this track, into a source / a folder, up, or the file */
    if (f_ent[f_cur].kind == 't') {
        uint32_t k = 0, t = f_ttrk % NAU, n = f_tpat % NPAT; aud_t *a = &paud[n];
        for (const char *q = f_ent[f_cur].name; *q >= '0' && *q <= '9'; q++) k = k * 10u + (uint32_t)(*q - '0');
        if (!k || k > NTAKE || !tk[k].inuse) { f_stale = 1; return; }
        if (ctl_au_rq && n == set.pat % NPAT) { say("NOT WHILE A TAKE IS RECORDED"); return; }
        if (!(a->type >> t & 1u)) { a->type |= (uint8_t)(1u << t); if (n == set.pat % NPAT) ctl_panic = 1; }
        a->take[t] = (uint16_t)k; a->st[t] = tk[k].st; a->len[t] = au_up(tk[k].hi);
        aud_dirty |= 1ull << n; store_t = o3_ms; if (n == set.pat % NPAT) au_apply(); take_gc(); f_stale = 1;
        { static char m[48]; m[0] = 0; scat(m, "P"); fmt_u(m + slen(m), n + 1u); scat(m, " TRACK "); { char l[2] = {(char)('A' + t), 0}; scat(m, l); } scat(m, " PLAYS "); scat(m, f_lab[f_cur]); scat(m, " (THE SAME TAKE)"); say(m); }
        return;
    }
    if (f_ent[f_cur].kind == 's') { f_dir[0] = 0; scat(f_dir, f_ent[f_cur].name); }
    else if (f_ent[f_cur].kind == 'u') { char *sl = f_dir; for (char *q = f_dir; *q; q++) if (*q == '/') sl = q; if (sl == f_dir) f_dir[0] = 0; else *sl = 0; }
    else if (f_ent[f_cur].kind == 'd') { if (slen(f_dir) + slen(f_ent[f_cur].name) + 2u < sizeof f_dir) { scat(f_dir, "/"); scat(f_dir, f_ent[f_cur].name); } }
    else { path[0] = 0; scat(path, f_dir); scat(path, "/"); scat(path, f_ent[f_cur].name); wav_load_start(path, f_ttrk, f_tpat); return; }
    f_cur = 0; f_stale = 1;
}
static void files_view(void)
{
    char b[160], tl[2] = {(char)('A' + sel % NAU), 0};
    int w = (CW + 6) / NFK;
    uint32_t pn = set.pat % NPAT + 1u;
    for (uint32_t i = 0; i < NFK; i++) {
        int on = i == fk;
        vg_round(CX0 + (int)i * w, 41, w - 6, 26, 5, on ? (F_SAVE ? 0xFF3D6E : ACCENT) : CELL);
        vg_text(FK_NAME[i], CX0 + (int)i * w + (w - 6) / 2, 46, VG_MED, on ? 0x0E0624 : DIM, VG_CENTER);
    }
    b[0] = 0;
    if (F_SAVE) {
        if (fk == FK_SONG) { scat(b, "NOW: "); scat(b, set.song[0] ? set.song : "(not saved yet)"); scat(b, " · 64 patterns, sounds, mixers, audio"); }
        else if (fk == FK_PATTERN) { scat(b, "PATTERN "); fmt_u(b + slen(b), pn); scat(b, " with its sounds, mixer and audio (the takes as WAVs beside it)"); }
        else if (fk == FK_PRESET) { char n[PATCH_NAME + 1]; prog_name(trk_prog[sel % NTRK], n); scat(b, "TRACK "); scat(b, tl); scat(b, "'S SOUND: "); scat(b, n); if (trk_edited[sel % NTRK]) scat(b, " (edited)"); }
        else if (fk == FK_DX) scat(b, "DX7 banks (.syx) are loaded, not saved: F3 LOAD, kind DX BANK");
        else if (fk == FK_MIDI) { scat(b, "PATTERN "); fmt_u(b + slen(b), pn); scat(b, " AS A MIDI FILE: 6 tracks, channels 1-6 – for the DAW"); }
        else scat(b, f_n ? "A TAKE AS A WAV – DATA: WHICH ONE (every pattern's audio)" : "A TAKE AS A WAV – nothing recorded or loaded yet");
    }
    else if (fk == FK_SONG) scat(b, "VIBE FM/boom/ – ENTER twice loads (replaces everything)");
    else if (fk == FK_PATTERN) { scat(b, "ENTER twice → PATTERN "); fmt_u(b + slen(b), f_tpat % NPAT + 1u); scat(b, f_tpat % NPAT == set.pat % NPAT ? " (this one, stopped)" : ""); scat(b, " · KNOB 3: WHICH"); }
    else if (fk == FK_PRESET) { scat(b, "VIBE FM/boom/ – ENTER: TRACK "); scat(b, tl); scat(b, "'S SOUND (edited, STORE keeps it)"); }
    else if (fk == FK_MIDI) scat(b, "VIBE FM/boom/ – exported patterns; drag them into the DAW from that folder");
    else if (fk == FK_DX) scat(b, "VIBE FM/boom/ – .syx: ENTER twice = its 32 sounds in the DX slots");
    else { char tt[2] = {(char)('A' + f_ttrk % NAU), 0}; scat(b, f_dir[0] ? f_dir : "SOURCES"); scat(b, "  →  P"); fmt_u(b + slen(b), f_tpat % NPAT + 1u); scat(b, " TRACK "); scat(b, tt); scat(b, " · KNOB 3 / 4: WHERE"); }
    vg_text(b, CX0, 74, VG_MED, DIM, VG_LEFT);
    if (F_SAVE) {                               /* the name as boxes, the one being set lit (AUDIO: the takes above it) */
        int ny = 100, ey = 156, hy = 196;
        if (F_TAKES) {
            uint32_t rows = 4, top = f_cur >= rows ? f_cur - rows + 1u : 0u;
            for (uint32_t i = top; i < f_n && i < top + rows; i++) {
                int y = 94 + (int)(i - top) * 20, cur = i == f_cur;
                if (cur) vg_rect(CX0, y - 2, CW, 20, CELL2);
                vg_text(f_lab[i], CX0 + 8, y, VG_MED, cur ? FG : DIM, VG_LEFT);
            }
            ny = 178; ey = 224; hy = -1;
        }
        for (uint32_t i = 0; i < NM_LEN; i++) {
            int x = CX0 + 14 + (int)i * 36; char c[2] = {nm_ready ? nm_edit[i] : ' ', 0};
            vg_round(x, ny, 32, 40, 4, i == nm_pos ? vg_mix(CELL, 0xFF3D6E, 0.45f) : CELL);
            vg_text(c, x + 16, ny + 6, VG_BIG, FG, VG_CENTER);
            if (i == nm_pos) vg_rect(x + 4, ny + 36, 24, 3, 0xFF3D6E);
        }
        vg_text(pj.h || pj.make || ps.on || sv_k ? "SAVING ..." : F_TAKES ? "ENTER: SAVE · KNOB 2 PLACE · KNOB 3 LETTER" : "ENTER: SAVE", 240, ey, F_TAKES ? VG_MED : VG_BIG, pj.h || pj.make || ps.on || sv_k ? ACCENT : FG, VG_CENTER);
        if (hy > 0) vg_text("KNOB 2: PLACE · KNOB 3 / DATA: LETTER · a name saved again: a new version (2)", 240, hy, VG_SMALL, MUTED, VG_CENTER);
        if (f_n && !F_TAKES) {                  /* the files of this kind there already */
            char l[160]; l[0] = 0; scat(l, "ON THE CARD: ");
            for (uint32_t i = 0; i < f_n && slen(l) < 120u; i++) { if (i) scat(l, " · "); scat(l, f_ent[i].name); }
            vg_text(l, CX0, 222, VG_SMALL, DIM, VG_LEFT);
        }
    } else {
        uint32_t rows = 8, top = f_cur >= rows ? f_cur - rows + 1u : 0u;
        if (f_stale && !f_n) vg_text("...", CX0 + 8, 100, VG_MED, DIM, VG_LEFT);
        else if (!f_n) vg_text(F_BROWSE ? "NOTHING HERE" : fk == FK_SONG ? "NO SONGS YET" : fk == FK_PATTERN ? "NO PATTERNS SAVED YET" : fk == FK_DX ? "NO .syx BANK IN VIBE FM/boom/" : fk == FK_MIDI ? "NO MIDI FILES EXPORTED YET" : "NO PRESETS SAVED YET", CX0 + 8, 100, VG_MED, DIM, VG_LEFT);
        for (uint32_t i = top; i < f_n && i < top + rows; i++) {
            int y = 96 + (int)(i - top) * 22, cur = i == f_cur;
            if (cur) vg_rect(CX0, y - 2, CW, 21, CELL2);
            b[0] = 0;
            if (f_ent[i].kind == 's') { scat(b, "[ "); scat(b, f_lab[i][0] ? f_lab[i] : f_ent[i].name); scat(b, " ]"); }
            else if (f_ent[i].kind == 't') { scat(b, "IN MEMORY: "); scat(b, f_lab[i]); }
            else if (f_ent[i].kind == 'n') { scat(b, "NEW SONG "); scat(b, f_lab[i]); }
            else if (f_ent[i].kind == 'u') scat(b, "..");
            else { scat(b, f_ent[i].name); if (f_ent[i].kind == 'd') scat(b, "/"); }
            vg_text(b, CX0 + 8, y, VG_MED, cur ? (f_arm == i + 1u ? RED : FG) : f_ent[i].kind == 'f' ? FG : f_ent[i].kind == 't' ? ACCENT : f_ent[i].kind == 'n' ? 0xFF3D6E : DIM, VG_LEFT);
        }
    }
    b[0] = 0;
    if (wl.on) { scat(b, "LOADING WAV "); fmt_u(b + slen(b), wl.bytes ? (uint32_t)((uint64_t)wl.off * 100u / wl.bytes) : 0u); scat(b, "%"); }
    else if (pj.load) scat(b, "LOADING ...");
    else { scat(b, "KNOB 1 KIND · KNOB 2 CHOOSE · ENTER DO IT · "); scat(b, F_SAVE ? "F4" : "F3"); scat(b, " AGAIN NEXT KIND · F6 BACK"); }
    vg_text(b, 240, 262, VG_MED, wl.on || pj.load ? ACCENT : MUTED, VG_CENTER);
}
