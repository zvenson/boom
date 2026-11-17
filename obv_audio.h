/* SPDX-License-Identifier: GPL-3.0-only */
/* (3.7, VIBE only, Sven 9.11.2026) AUDIO TRACKS – the audio flow's half. Any of the tracks A-D can be an audio track
 * instead of a synth: it plays what was recorded onto it – from IN 1 / IN 2 (mono, or both as stereo) or RESAMPLE (what
 * you hear, after MASTER) – through its mixer channel (level, pan, the sends), like the synth it replaces.
 *
 * (3.8, Sven 9.11.) An audio track belongs to its PATTERN: each pattern says which of its tracks are audio, and each such
 * track plays a TAKE of its own. The takes share one store (au_arena, 256 MB); a track points at its take (ctl_au_ptr,
 * ctl_au_hi = the frames it holds). When the next pattern comes in on the downbeat, its audio comes with it (au_q,
 * au_swap_in) and every audio track starts from its top, as the steps do. ONCE: the take plays once from the pattern's
 * start; LOOP: it goes round every `len` frames. No time stretching: the length in bars is counted at the tempo now.
 *
 * Memory: past __heap_base (au_mem), not a static array: with shared memory the linker's start zeroes every static byte –
 * 256 MB touched in every instance (a DAW: 270 MB and 0.3 s each, 9.11.2026). firmware.json "memory_whole" makes the whole
 * memory there from the start (tools/build.sh: --initial-memory); nothing below writes past __heap_base (the hosts put
 * their stacks between __data_end and __heap_base, WAMR gets no app heap), and those pages cost nothing until a take is
 * recorded. Not memory.grow: WAMR 2.2 hangs growing a shared memory (the VIBE's selftest, 9.11.2026).
 * 256 MB = about 23 min of stereo or 46 min of mono at 48 kHz, for all the takes together. */
#define NAU NTRK
#define AU_ARENA (128u << 20)                   /* int16 values in the store: 256 MB */
static int16_t *au_arena;                       /* NULL: the host gave no memory – no audio tracks */
#ifdef __wasm__
extern unsigned char __heap_base;
#endif
static int au_mem(void)                         /* the store past __heap_base, if the memory reaches that far */
{
#ifdef __wasm__
    uintptr_t at = ((uintptr_t)&__heap_base + (1u << 20) + 65535u) & ~(uintptr_t)65535u;   /* (1 MB clear of it, on a page) */
    uint64_t end = (uint64_t)at + (uint64_t)AU_ARENA * sizeof(int16_t), have = (uint64_t)__builtin_wasm_memory_size(0) * 65536u;
    if (end > have) return 0;
    au_arena = (int16_t *)at;
    return 1;
#else
    static int16_t store[AU_ARENA]; au_arena = store; return 1;
#endif
}
enum { AU_IN1, AU_IN2, AU_RES };                /* where a recording comes from; 3..16 = IN 3..IN 16 (an interface, SDK 26) */
#define NAUSRC 17u                              /* (BOOM 0.29, Sven 15.11.: the XR18 – IN 3..16 kept after RESAMP: old patterns stay) */
static int au_src_in(uint32_t src) { src %= NAUSRC; return src == AU_RES ? -1 : src < 2u ? (int)src : (int)src - 1; }   /* the input 0..15, -1: RESAMPLE */
enum { AUM_OFF, AUM_AUTO, AUM_ON, NAUM };       /* monitoring the input: AUTO = while armed or recording */

/* main loop -> ISR: each track's take (NULL: none), how many frames it holds, how many it may take while recording */
typedef struct {
    uint32_t type;                              /* bit t: track t is an audio track */
    int16_t *ptr[NAU];
    uint32_t hi[NAU], capf[NAU], len[NAU];      /* hi: frames of sound · capf: frames its block holds · len: the track's length */
    uint8_t loop[NAU], st[NAU], src[NAU], mon[NAU], dub[NAU];   /* st: the take's channels (1 = stereo) */
    uint8_t flt[NAU], cut[NAU], res[NAU], drv[NAU];   /* (3.8) the track's filter: AF_*, cutoff / resonance / drive 0..127 */
    uint32_t lps[NAU], lpe[NAU];                /* (12.11.2026) the loop's piece in frames (lpe 0: to the end) */
} auctl_t;
enum { AF_OFF, AF_LP, AF_HP, AF_BP, NAF };
static volatile auctl_t au_c;                   /* what plays now (the main loop writes it while that pattern is the one) */
static auctl_t au_q;                            /* the next pattern's, swapped in with it on the downbeat */
static volatile uint32_t ctl_au_swap;           /* 1: au_q is ready for the pattern swap */
static volatile int32_t ctl_au_lat;             /* frames from a frame computed to its echo on the input (out + in latency) */
static volatile uint32_t ctl_au_rq;             /* bit t: record track t from the next bar (bar 1 when it starts); 0: stop.
                                                 * (BOOM 0.29) several at once: each its own take, all from the same frame */
/* ISR -> main loop */
static volatile uint32_t ui_au_state;           /* 0 idle, 1 waiting for the bar, 2 recording (any of them) */
static volatile uint32_t ui_au_done;            /* bit t: track t's take ended (main: ui_au_at / ui_au_end, then clears its bit of ctl_au_rq) */
static volatile int32_t ui_au_at[NAU];          /* the frame the take began at (-1: it never began) */
static volatile uint32_t ui_au_end[NAU], ui_au_pos[NAU];   /* the take's end (frames into the track); while recording, how far */
static volatile uint32_t ui_au_play[NAU];       /* each track's place (frames into it), 0xFFFFFFFF = silent */

typedef struct { uint32_t on, wait, lock, src, st, loop, len0, end, capf, dub; int32_t at, lat; int16_t *base; } aur_t;
static aur_t aur[NAU];                          /* each track's take (ISR) */
static int au_recn(uint32_t t) { return aur[t % NAU].on || aur[t % NAU].wait; }   /* track t records or waits for the bar */
static int32_t au_blk[HALF_FRAMES / CTL];       /* the piece's CTL blocks: the song frame each began at, -1 = not running */
static const float *au_in[VIBE_IO_MAX];         /* the piece's IN 1 .. IN 16 (NULL: off), au_in_n frames of them */
static uint32_t au_in_n;

static int au_is(uint32_t t) { return (int)(ctl_au_type >> (t % NAU) & 1u); }
/* (BOOM 0.29, Sven 15.11.: the XR18, several tracks at once) ARM: bit t = audio track t records when REC is pressed – REC held +
 * its pad (row 1) arms and disarms it. Only the audio tracks of the pattern count (au_armed). Not saved: it is the moment's. */
static uint32_t au_armm;
static uint32_t au_armed(void) { return au_armm & ctl_au_type & ((1u << NAU) - 1u); }

static void au_take_end(uint32_t t);
static void au_swap_in(void)                    /* (ISR, the pattern swap on the downbeat) its audio, every track from the top */
{
    if (__atomic_load_n(&ctl_au_swap, __ATOMIC_ACQUIRE)) {
        for (uint32_t t = 0; t < NAU; t++) if (au_recn(t)) au_take_end(t);   /* (a take belongs to the pattern it began in) */
        au_c = au_q; ctl_au_type = au_q.type;
        RING_PUBLISH();
        ctl_au_swap = 0;
    }
    for (uint32_t t = 0; t < NAU; t++) au_from[t] = au_song > 0 ? au_song : 0;
}

static void au_block(uint32_t k)                /* (engine, after seq_tick) block k of the piece */
{
    au_blk[k % (HALF_FRAMES / CTL)] = au_song;
    if (au_song >= 0) au_song += (int32_t)CTL;
}

static uint32_t au_place(uint32_t t, int32_t s, uint32_t win)   /* where track t is at song frame s; 0xFFFFFFFF = nothing there.
                                                 * win: the loop's piece (0: the whole track) */
{
    uint32_t len = au_c.len[t], off = 0, end;
    int32_t d = s - au_from[t];
    if (!len || s < 0) return 0xFFFFFFFFu;
    if (win && (au_c.lps[t] || au_c.lpe[t])) {  /* (12.11.2026) only frames lps .. lpe of it */
        off = au_c.lps[t]; end = au_c.lpe[t] ? au_c.lpe[t] : len;
        if (end > len) end = len;
        if (off >= end) off = 0, end = len;
        len = end - off;
    }
    if (!au_c.loop[t]) return d >= 0 && (uint32_t)d < len ? off + (uint32_t)d : 0xFFFFFFFFu;   /* ONCE: from the pattern's start */
    {
        int64_t m = (int64_t)d % (int64_t)len;
        if (m < 0) m += len;
        return off + (uint32_t)m;
    }
}

static void au_filter(uint32_t t, int32_t *o, uint32_t n);
/* (engine, before the mixer) the audio tracks into their buses; `used` gets their bit when they sound */
static void au_mix(int32_t (*bus)[2u * HALF_FRAMES], uint32_t n, uint32_t *used)
{
    if (!au_arena) return;
    for (uint32_t t = 0; t < NAU; t++) {
        int smp_rec = !au_is(t) && au_recn(t);  /* (0.20) a SAMPLER recording its own sample: its input is heard */
        if (!au_is(t) && !smp_rec) continue;
        int32_t *o = bus[t];
        const int16_t *pl = au_c.ptr[t];
        uint32_t st = au_c.st[t], hi = au_c.hi[t], rec = (uint32_t)au_recn(t), mon = au_c.mon[t] % NAUM, src = au_c.src[t] % NAUSRC;
        int play = !smp_rec && pl && (ctl_trk_on >> t & 1u) && au_c.len[t] && !(aur[t].on && !aur[t].dub);   /* (recording: replaced, not heard; OVERDUB: heard) */
        int in = src != AU_RES && (mon == AUM_ON || (mon == AUM_AUTO && (rec || (ctl_au_rq >> t & 1u))));
        uint32_t last = 0xFFFFFFFFu, bar = rec ? 0u : 1u;   /* (recording on it: the whole track) */
        if (play) for (uint32_t i = 0; i < n; i++) {
            int32_t s0 = au_blk[i / CTL];
            uint32_t p = s0 < 0 ? 0xFFFFFFFFu : au_place(t, s0 + (int32_t)(i % CTL), bar);
            if (p == 0xFFFFFFFFu) continue;
            last = p; *used |= 1u << t;
            if (p >= hi) continue;              /* (past the take's end, still inside the track: silence) */
            if (st) { o[2u * i] += pl[2u * p]; o[2u * i + 1u] += pl[2u * p + 1u]; }
            else { o[2u * i] += pl[p]; o[2u * i + 1u] += pl[p]; }
        }
        ui_au_play[t] = last;
        if (in) {                               /* the input through the track (its fader, pan, sends) */
            int ii = au_src_in(src), i0 = st ? ii & ~1 : ii;   /* (stereo: the pair IN 1+2, 3+4, ...) */
            const float *a = au_in[i0], *b = st && i0 + 1 < VIBE_IO_MAX ? au_in[i0 + 1] : 0;
            for (uint32_t i = 0; i < n && i < au_in_n; i++) {
                float l, r;
                if (st) { l = a ? a[i] : 0.0f; r = b ? b[i] : 0.0f; }
                else { l = r = a ? a[i] : 0.0f; }
                o[2u * i] += (int32_t)(l * 32767.0f); o[2u * i + 1u] += (int32_t)(r * 32767.0f);
            }
            *used |= 1u << t;
        }
        if (*used >> t & 1u) au_filter(t, o, n);   /* (3.8) */
    }
}

/* (3.8, Sven: "dort dann auch den Filter rein") an audio track's filter: drive (tanh) into a 12 dB state-variable LP / HP / BP,
 * cutoff 20 Hz .. 20 kHz, resonance Q 0.5 .. 12, on the whole bus (the take and a monitored input) */
static vd_svf au_fs[NAU][2];
static vd_svf_c au_fc[NAU];
static int32_t au_fk[NAU] = {-1, -1, -1, -1, -1, -1, -1, -1};   /* (the coefficients' cutoff + resonance: set again when they move) */
static void au_filter(uint32_t t, int32_t *o, uint32_t n)
{
    uint32_t f = au_c.flt[t] % NAF;
    if (f == AF_OFF && !au_c.drv[t]) return;
    int32_t key = (int32_t)au_c.cut[t] << 8 | au_c.res[t];
    if (key != au_fk[t]) {
        float hz = 20.0f * vd_pow(1000.0f, (float)au_c.cut[t] / 127.0f), q = 0.5f + 11.5f * (float)au_c.res[t] / 127.0f;
        if (hz > 0.45f * (float)FS) hz = 0.45f * (float)FS;
        vd_svf_set(&au_fc[t], hz, q, (float)FS); au_fk[t] = key;
    }
    float g = 1.0f + (float)au_c.drv[t] / 8.0f, post = au_c.drv[t] ? 1.0f / vd_tanh(g) : 1.0f;
    for (uint32_t i = 0; i < 2u * n; i++) {
        float x = (float)o[i] * (1.0f / 32768.0f);
        if (au_c.drv[t]) x = vd_tanh(x * g) * post;
        if (f != AF_OFF) { vd_svf_out y = vd_svf_tick(&au_fs[t][i & 1u], &au_fc[t], x); x = f == AF_LP ? y.lp : f == AF_HP ? y.hp : y.bp; }
        o[i] = (int32_t)(x * 32768.0f);
    }
}

static int au_resampling(void) { for (uint32_t t = 0; t < NAU; t++) if (aur[t].on && aur[t].src == AU_RES) return 1; return 0; }   /* (the click is left out of it) */

static void au_take_end(uint32_t t)             /* (ISR) track t's take is over: the main loop finishes it */
{
    aur_t *a = &aur[t];
    ui_au_at[t] = a->wait ? -1 : a->at; ui_au_end[t] = a->end;
    a->on = a->wait = 0; a->lock = 1;
    RING_PUBLISH();
    __atomic_fetch_or(&ui_au_done, 1u << t, __ATOMIC_RELEASE);
}

/* (main.c, after MASTER, per piece) the take: l / r = what is heard, au_in = the inputs, au_blk = the song's frames.
 * The main loop has given the track a block of its own for it (au_c.ptr / capf – the old take copied in, for DUB and
 * a punch into what is there) before it asked (ctl_au_rq). */
static void au_cap1(uint32_t t, uint32_t want, const float *l, const float *r, uint32_t n)   /* track t's take, this piece */
{
    aur_t *A = &aur[t];
    if (A->lock) { if (!want) A->lock = 0; return; }
    if (!A->on && !A->wait) {
        if (!want) return;
        RING_TAKE();
        A->src = au_c.src[t] % NAUSRC; A->st = au_c.st[t]; A->loop = au_c.loop[t];
        A->len0 = au_c.len[t]; A->capf = au_c.capf[t]; A->base = au_c.ptr[t]; A->end = 0; A->at = -1;
        A->dub = au_c.dub[t] && au_c.hi[t];     /* (OVERDUB onto something; an empty track: a plain take) */
        A->lat = A->src == AU_RES ? 0 : ctl_au_lat;   /* (3.8.1: one latency for the whole take – a change mid-take left holes) */
        A->wait = 1; ui_au_pos[t] = 0;
        if (!A->base) { au_take_end(t); return; }
    }
    if (!want) { au_take_end(t); return; }      /* stopped by hand (waiting: nothing taken) */
    int ii = au_src_in(A->src), i0 = A->st && ii >= 0 ? ii & ~1 : ii;   /* (stereo: the pair IN 1+2, 3+4, ...) */
    const float *ia = ii >= 0 ? au_in[i0] : 0, *ib = ii >= 0 && A->st && i0 + 1 < VIBE_IO_MAX ? au_in[i0 + 1] : 0;
    for (uint32_t k = 0; k * CTL < n; k++) {
        int32_t s0 = au_blk[k];
        if (s0 < 0) continue;
        if (A->wait) {                          /* the next bar of the pattern (its first when it starts now) */
            int32_t bar = (int32_t)song_bar_frames(quarter_len()), d0 = s0 - au_from[t];
            A->at = au_from[t] + (d0 <= 0 ? 0 : (d0 + bar - 1) / bar * bar);
            if (s0 + (int32_t)CTL <= A->at) continue;
            A->wait = 0; A->on = 1;
            if (A->loop && !A->len0) au_from[t] = A->at;   /* (a new loop: it goes round from here) */
        }
        int16_t *pl = A->base;
        int32_t lat = A->lat;
        for (uint32_t j = 0; j < CTL && k * CTL + j < n; j++) {
            uint32_t f = k * CTL + j, pos;
            int32_t se = s0 + (int32_t)j - lat;   /* (the input: played to what was heard lat frames ago) */
            if (se < A->at) continue;
            if (!A->loop) pos = (uint32_t)(se - au_from[t]);   /* ONCE: from the pattern's start */
            else if (A->len0) {
                if ((uint32_t)(se - A->at) >= A->len0) { au_take_end(t); return; }   /* (once round: done) */
                int64_t m = ((int64_t)se - au_from[t]) % (int64_t)A->len0; if (m < 0) m += A->len0;
                pos = (uint32_t)m;
            } else pos = (uint32_t)(se - A->at);
            if (pos >= A->capf) { au_take_end(t); return; }   /* (the store is full) */
            float a, b;
            if (A->src == AU_RES) { a = l[f]; b = r[f]; }
            else { a = ia && f < au_in_n ? ia[f] : 0.0f; b = ib && f < au_in_n ? ib[f] : 0.0f; }
            /* OVERDUB: an input adds to what is there; RESAMPLE takes what is heard, the track itself in it already */
            int add = A->dub && A->src != AU_RES;
            if (A->st) {
                pl[2u * pos] = (int16_t)sat16((int32_t)(a * 32767.0f) + (add ? pl[2u * pos] : 0));
                pl[2u * pos + 1u] = (int16_t)sat16((int32_t)(b * 32767.0f) + (add ? pl[2u * pos + 1u] : 0));
            }
            else pl[pos] = (int16_t)sat16((int32_t)((A->src == AU_RES ? (a + b) * 0.5f : a) * 32767.0f) + (add ? pl[pos] : 0));
            if (pos + 1u > A->end) A->end = pos + 1u;
            ui_au_pos[t] = pos;
        }
    }
}
/* (main.c, after MASTER, per piece) the takes: l / r = what is heard, au_in = the inputs, au_blk = the song's frames.
 * The main loop has given each track a block of its own for it (au_c.ptr / capf – the old take copied in, for DUB and
 * a punch into what is there) before it asked (ctl_au_rq). (BOOM 0.29) Every track asked for records at once. */
static void au_capture(const float *l, const float *r, uint32_t n)
{
    uint32_t rq = ctl_au_rq, st = 0;
    if (!au_arena) return;
    for (uint32_t t = 0; t < NAU; t++) {
        if (rq >> t & 1u || aur[t].on || aur[t].wait || aur[t].lock) au_cap1(t, rq >> t & 1u, l, r, n);
        if (aur[t].on) st = 2u; else if (aur[t].wait && !st) st = 1u;
    }
    ui_au_state = st;
}
