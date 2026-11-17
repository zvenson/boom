/* SPDX-License-Identifier: GPL-3.0-only */
/* (BOOM 0.19-0.21, Sven 14.11.2026) the SAMPLER kind's main-loop half. A sampler track has its OWN sample (REC on it, or
 * LOAD > AUDIO onto it – BOOM's take store, o3_audio.h) or reads another track's take (SOURCE). START / END pick the part
 * that plays. MODE PLAY: that part over the keys (C3 = as recorded). MODE CHOP (as ZVAMPLER's CHOP): the part cut into
 * 2-16 slices, EVEN or at the HITS (SENS: how small a start still counts); C3 = slice 1. The cuts can be moved by hand
 * (the CHOP page: CUT chooses, MOVE moves – SHIFT fine –, ENTER adds one, SHIFT + ENTER takes it away); turning MODE,
 * SLICES, SENS, START or END finds them again. Nothing is copied: the slices point into the take. */
static const int16_t *sm_take(uint32_t t, uint32_t *hi, uint32_t *st)
{
    t %= NTRK;
    if (!au_c.ptr[t]) { *hi = 0; *st = 0; return 0; }
    *hi = au_c.hi[t]; *st = au_c.st[t]; return au_c.ptr[t];
}
static uint32_t sm_key[8], sm_ver[8];
static float sm_env[4096];
static void fmt_secs(char *s, uint32_t frames)   /* "1.25 s": a sample's places want hundredths */
{
    uint32_t cs = (uint32_t)((uint64_t)frames * 100u / (obv_fs ? obv_fs : 48000u));
    s[0] = 0; fmt_u(s, cs / 100u); scat(s, "."); { char t[4]; t[0] = (char)('0' + cs % 100u / 10u); t[1] = (char)('0' + cs % 10u); t[2] = 0; scat(s, t); } scat(s, " s");
}
static uint32_t sm_src(uint32_t p) { uint32_t s = live_p[p % NTRK].v[P_SM_SRC] % 9u; return s ? s - 1u : p % NTRK; }
static uint32_t sm_pos14(uint32_t p, int end) { const uint8_t *v = live_p[p % NTRK].v; return end ? (uint32_t)v[P_SM_EN_C] << 7 | v[P_SM_EN_F] : (uint32_t)v[P_SM_ST_C] << 7 | v[P_SM_ST_F]; }
static int sm_region(uint32_t p, uint32_t *a, uint32_t *b, uint32_t *hi)   /* the part that plays, in frames of the take; 0 = no sample */
{
    uint32_t st; const int16_t *d = sm_take(sm_src(p), hi, &st);
    if (!d || *hi < 2048u) return 0;
    *a = (uint32_t)((uint64_t)*hi * sm_pos14(p, 0) / 16383u); *b = (uint32_t)((uint64_t)*hi * sm_pos14(p, 1) / 16383u);
    if (*b > *hi) *b = *hi;
    if (*b < *a + 512u) *b = *a + 512u < *hi ? *a + 512u : *hi;
    return *b > *a;
}
static void sm_update(void)
{
    for (uint32_t p = 0; p < NPART && p < 8u; p++) {
        const patch_t *pv = &live_p[p];
        uint32_t src, hi, st, ns, chop, key, i, at[17], a, b;
        const int16_t *d;
        if (pv->v[P_TYPE] % NTYPE != T_SMP) { sm_n[p] = 0; sm_key[p] = 0; continue; }
        src = sm_src(p);
        ns = pv->v[P_SM_SLICES] % 15u + 2u; chop = pv->v[P_SM_CHOP] % 3u == 2u;   /* (0.25) the cuts are always there – MEL just plays past them */
        d = sm_take(src, &hi, &st);
        if (!sm_region(p, &a, &b, &hi)) { sm_n[p] = 0; sm_key[p] = 0; continue; }
        if (aur[src % NAU].on && (ctl_au_rq >> (src % NAU) & 1u)) continue;   /* (it is being recorded: cut when it is done) */
        key = (uint32_t)(uintptr_t)d * 2654435761u ^ hi * 40503u ^ ns * 7u ^ chop * 131u ^ st ^ a * 977u ^ b * 31u ^ pv->v[P_SM_SENS] * 6151u ^ sm_ver[p] * 104729u;
        if (key == sm_key[p] && sm_n[p]) continue;
        for (i = 0; i <= ns; i++) at[i] = a + (uint32_t)((uint64_t)(b - a) * i / ns);   /* EVEN (and HITS' fallback); PLAY: a, b */
        if (ns > 1u && set.smc_n[p] >= 2u && set.smc_n[p] <= 16u) {   /* cut by hand */
            ns = set.smc_n[p];
            for (i = 0; i < ns; i++) { uint32_t x = (uint32_t)((uint64_t)hi * set.smc[p][i] / 65535u); at[i] = x < a ? a : x > b ? b : x; }
            at[0] = a; at[ns] = b;
        } else if (ns > 1u && chop) {           /* HITS: an energy curve, its rises over what came before, the strongest apart */
            uint32_t w = 256u, nw, k, got = 1u, pick[16], len = b - a;
            float need = 1.15f + (127.0f - (float)pv->v[P_SM_SENS]) * 0.02f;
            while (len / w > 4096u) w *= 2u;
            nw = len / w;
            for (k = 0; k < nw; k++) {
                float s = 0.0f;
                for (uint32_t j = 0; j < w; j += 4u) { uint32_t f = a + k * w + j; float x = st ? ((float)d[2u * f] + (float)d[2u * f + 1u]) * 0.5f : (float)d[f]; s += x < 0.0f ? -x : x; }
                sm_env[k] = s;
            }
            pick[0] = 0;
            {
                uint32_t gap = nw / (ns * 3u) + 1u;
                for (uint32_t r = 1; r < ns; r++) {
                    float best = 0.0f; uint32_t bk = 0;
                    for (k = 1; k < nw; k++) {
                        float avg = 0.0f; uint32_t m = 0, q; int near = 0;
                        for (q = k > 8u ? k - 8u : 0u; q < k; q++, m++) avg += sm_env[q];
                        avg = m ? avg / (float)m : 0.0f;
                        if (sm_env[k] < (avg + 1.0f) * need) continue;
                        for (q = 0; q < got && !near; q++) near = (k > pick[q] ? k - pick[q] : pick[q] - k) < gap;
                        if (!near && sm_env[k] - avg > best) { best = sm_env[k] - avg; bk = k; }
                    }
                    if (!bk) break;
                    pick[got++] = bk;
                }
            }
            if (got > 1u) {
                ns = got;
                for (uint32_t x = 1; x < got; x++) for (uint32_t y = x; y > 0 && pick[y - 1u] > pick[y]; y--) { uint32_t t = pick[y]; pick[y] = pick[y - 1u]; pick[y - 1u] = t; }
                for (i = 0; i < ns; i++) at[i] = a + (pick[i] * w > 64u ? pick[i] * w - 64u : 0u);   /* (a touch before the hit) */
                at[0] = a; at[ns] = b;
            }
        }
        sm_n[p] = 0;
        for (i = 0; i <= ns; i++) sm_at[p][i] = at[i];
        __atomic_thread_fence(__ATOMIC_RELEASE);
        sm_n[p] = ns; sm_key[p] = key;
        if (sm_cur[p] >= ns) sm_cur[p] = ns - 1u;
    }
}
/* ---- the CHOP page by hand (o3_pages.h knobs, main.c ENTER) */
static int sm_hand_from_now(uint32_t p)         /* the cuts as they are now become the hand-made ones */
{
    uint32_t a, b, hi, n = sm_n[p % 8u];
    p %= 8u;
    if (!sm_region(p, &a, &b, &hi) || n < 2u) return 0;
    if (set.smc_n[p] < 2u) { for (uint32_t i = 0; i <= n; i++) set.smc[p][i] = (uint16_t)((uint64_t)sm_at[p][i] * 65535u / hi); set.smc_n[p] = (uint8_t)n; }
    return 1;
}
static void sm_cut_move(uint32_t p, int e, int fine)
{
    uint32_t a, b, hi, c = sm_cur[p % 8u], n;
    p %= 8u;
    if (!sm_hand_from_now(p)) { say("CHOP FIRST: MODE CHOP EVEN / HITS"); return; }
    if (!c) {                                   /* (0.23) cut 1 is the START: moving it moves START */
        int32_t v = (int32_t)sm_pos14(p, 0) + e * (fine ? 1 : 16), o = (int32_t)sm_pos14(p, 1);
        v = clampi(v, 0, o - 64); live_p[p].v[P_SM_ST_C] = (uint8_t)(v >> 7); live_p[p].v[P_SM_ST_F] = (uint8_t)(v & 127); trk_edited[p] = 1;
        sm_region(p, &a, &b, &hi); set.smc[p][0] = (uint16_t)((uint64_t)a * 65535u / hi); sm_ver[p]++; set_touch(); return;
    }
    sm_region(p, &a, &b, &hi); n = set.smc_n[p];
    {
        int32_t step = (int32_t)((uint64_t)(b - a) * 65535u / hi / (fine ? 4000u : 200u)) + 1, x = (int32_t)set.smc[p][c] + e * step;
        int32_t lo = (int32_t)set.smc[p][c - 1u] + 64, top = c + 1u < n ? (int32_t)set.smc[p][c + 1u] - 64 : (int32_t)((uint64_t)b * 65535u / hi) - 64;
        set.smc[p][c] = (uint16_t)clampi(x, lo, top);
    }
    sm_ver[p]++; set_touch();
}
static void sm_cut_add(uint32_t p)
{
    uint32_t c, n;
    p %= 8u;
    if (!sm_hand_from_now(p)) { say("CHOP FIRST: MODE CHOP EVEN / HITS"); return; }
    n = set.smc_n[p]; c = sm_cur[p];
    if (n >= 16u) { say("16 SLICES – THAT IS THE LIMIT"); return; }
    {
        uint32_t a, b, hi; uint16_t hiv;
        sm_region(p, &a, &b, &hi); hiv = (uint16_t)((uint64_t)b * 65535u / hi);
        uint16_t x0 = set.smc[p][c], x1 = c + 1u < n ? set.smc[p][c + 1u] : hiv, x = (uint16_t)(x0 + (x1 - x0) / 2u);
        if (x1 - x0 < 256u) { say("TOO SHORT TO CUT AGAIN"); return; }
        for (uint32_t i = n; i > c + 1u; i--) set.smc[p][i] = set.smc[p][i - 1u];
        set.smc[p][c + 1u] = x; set.smc_n[p] = (uint8_t)(n + 1u); sm_cur[p] = c + 1u;
    }
    sm_ver[p]++; set_touch(); say("CUT ADDED – KNOB 2 MOVES IT");
}
static void sm_cut_del(uint32_t p)
{
    uint32_t c, n;
    p %= 8u;
    if (!sm_hand_from_now(p)) return;
    n = set.smc_n[p]; c = sm_cur[p];
    if (!c || n < 3u) { say(c ? "TWO SLICES AT LEAST" : "CUT 1 IS THE START – CHOOSE ANOTHER"); return; }
    for (uint32_t i = c; i + 1u < n; i++) set.smc[p][i] = set.smc[p][i + 1u];
    set.smc_n[p] = (uint8_t)(n - 1u); sm_cur[p] = c - 1u;
    sm_ver[p]++; set_touch(); say("CUT REMOVED");
}
static void sm_auto(uint32_t p) { p %= 8u; if (set.smc_n[p]) { set.smc_n[p] = 0; sm_ver[p]++; set_touch(); } }
/* ---- (0.22) the SAMPLER's EDIT and CHOP pages, as ZVAMPLER's: a big waveform, the values under it, F keys of their own */
static int smp_page(void)                        /* a sampler track's EDIT or CHOP page is shown */
{
    return trk_type(sel) == T_SMP && !in_seq() && !menu && !files_on && !snd_view() && (page == PG_PERFORM || page == PG_S1);
}
static void sm_wave(const int16_t *d, uint32_t st, uint32_t x0, uint32_t x1, uint32_t a, uint32_t b, int X, int Y, int Wd, int Hh, uint32_t c)
{   /* frames x0..x1 across the box; a..b (what plays) bright, the rest dim */
    vg_rect(X, Y, Wd, Hh, CELL);
    if (x1 <= x0) return;
    for (int i = 0; i < Wd; i++) {
        uint32_t f0 = x0 + (uint32_t)((uint64_t)(x1 - x0) * (uint32_t)i / (uint32_t)Wd), f1 = x0 + (uint32_t)((uint64_t)(x1 - x0) * (uint32_t)(i + 1) / (uint32_t)Wd), stp = (f1 - f0) / 32u + 1u;
        int mx = 0;
        for (uint32_t f = f0; f < f1 || f == f0; f += stp) { int v = st ? (d[2u * f] + d[2u * f + 1u]) / 2 : d[f]; if (v < 0) v = -v; if (v > mx) mx = v; if (f1 <= f0) break; }
        int h = mx * (Hh - 6) / 32768; if (h < 1) h = 1;
        vg_rect(X + i, Y + Hh / 2 - h / 2, 1, h, f0 >= a && f0 < b ? c : vg_mix(CELL, c, 0.28f));
    }
}
static int sm_x(uint32_t f, uint32_t x0, uint32_t x1, int X, int Wd) { return f < x0 || f > x1 || x1 <= x0 ? -1 : X + (int)((uint64_t)(f - x0) * (uint32_t)Wd / (x1 - x0)); }
static void smp_draw(const knob_t *k)
{
    uint32_t p = sel % 8u, a = 0, b = 0, hi = 0, st = 0, c = TRK_RGB[sel], x0, x1;
    const int X = CX0, Y = 72, Wd = CW, Hh = 128;
    const int16_t *d = sm_take(sm_src(p), &hi, &st);
    int chop = page == PG_S1, rec_here = (int)((ctl_au_rq | au_arm) >> sel & 1u);
    /* (0.25, Sven: "man kommt immer mal aus Versehen über CHOP") the page no longer switches the keys – KEYS (F5) alone sets MEL / CHOP */
    if (rec_here && aur[p].on && d) {          /* recording: the wave grows, red */
        uint32_t n = ui_au_pos[p] + 1u, span = 4u * obv_fs; while (span < n) span *= 2u;
        sm_wave(d, st, 0, span, 0, n, X, Y, Wd, Hh, RED);
        vg_text("RECORDING · REC AGAIN = DONE", 240, Y + 6, VG_MED, RED, VG_CENTER);
    } else if (rec_here) {
        vg_rect(X, Y, Wd, Hh, CELL);
        vg_text(aur[p].wait ? "REC · WAITING FOR THE BAR" : "REC ARMED · PLAY STARTS IT", 240, Y + 50, VG_BIG, RED, VG_CENTER);
    } else if (!sm_region(p, &a, &b, &hi)) {
        vg_rect(X, Y, Wd, Hh, CELL);
        vg_text(sm_src(p) == p ? "EMPTY · NO SAMPLE YET" : "NO TAKE ON THE SOURCE TRACK", 240, Y + 36, VG_BIG, MUTED, VG_CENTER);
        vg_text("REC records IN 1 (F2: the input)  ·  F1 LOAD a WAV", 240, Y + 80, VG_MED, DIM, VG_CENTER);
    } else if (!chop) {                         /* EDIT: the whole sample, START green, END red; turning one: zoomed to it */
        x0 = 0; x1 = hi;
        if (o3_ms - sm_turn_t < 1500u) {
            uint32_t m = sm_turn_end ? b : a, w = hi / 12u + 256u;
            x0 = m > w / 2u ? m - w / 2u : 0u; x1 = x0 + w > hi ? hi : x0 + w; if (x1 - x0 < w && x1 == hi) x0 = hi > w ? hi - w : 0u;
            vg_text("ZOOM", 464, Y + 4, VG_SMALL, FG, VG_RIGHT);
        }
        sm_wave(d, st, x0, x1, a, b, X, Y, Wd, Hh, c);
        int xs = sm_x(a, x0, x1, X, Wd), xe = sm_x(b, x0, x1, X, Wd);
        if (xs >= 0) { vg_rect(xs, Y, 2, Hh, GREEN); vg_tri((float)xs, (float)Y, (float)xs + 10, (float)Y, (float)xs, (float)Y + 10, GREEN); }
        if (xe >= 0) { vg_rect(xe - 2, Y, 2, Hh, RED); vg_tri((float)xe, (float)Y, (float)xe - 10, (float)Y, (float)xe, (float)Y + 10, RED); }
    } else {                                    /* CHOP: the part that plays, its cuts numbered, the chosen one marked */
        x0 = a; x1 = b;
        sm_wave(d, st, x0, x1, a, b, X, Y, Wd, Hh, c);
        for (uint32_t i = 0; i < sm_n[p] && sm_n[p] > 1u; i++) {
            int x = sm_x(sm_at[p][i], x0, x1, X, Wd), cur = i == sm_cur[p];
            char t[4];
            if (x < 0) continue;
            vg_rect(x, Y, cur ? 2 : 1, Hh, cur ? ACCENT : vg_mix(CELL, FG, 0.7f));
            if (cur) vg_tri((float)x - 6, (float)Y, (float)x + 7, (float)Y, (float)x, (float)Y + 10, ACCENT);
            fmt_u(t, i + 1u); vg_text(t, x + 4, Y + 12, VG_SMALL, cur ? ACCENT : DIM, VG_LEFT);
        }
        if (sm_n[p] < 2u) vg_text("KNOB 3: HOW MANY SLICES", 240, Y + 50, VG_MED, DIM, VG_CENTER);
        if (live_p[sel].v[P_SM_CHOP] % 3u && sm_n[p] > 1u) {   /* (0.28, Sven: "zeige es besser") the slices sounding now light up – the keys' and the sequencer's */
            uint32_t on = 0;
            for (uint32_t v = card_lo(p); v < card_lo(p) + NCARD; v++)
                if (vc[v].type == T_SMP && !vc[v].sm_done) for (uint32_t i = 0; i < sm_n[p]; i++) if (vc[v].sm_a == sm_at[p][i]) on |= 1u << i;
            for (uint32_t i = 0; i < sm_n[p]; i++) if (on >> i & 1u) {
                int xa = sm_x(sm_at[p][i], x0, x1, X, Wd), xb = sm_x(sm_at[p][i + 1u], x0, x1, X, Wd);
                char t[4];
                if (xa < 0 || xb <= xa) continue;
                vg_rect(xa, Y, xb - xa, 7, ACCENT); vg_rect(xa, Y + Hh - 7, xb - xa, 7, ACCENT);
                vg_rect(xa, Y, 2, Hh, ACCENT); vg_rect(xb - 2, Y, 2, Hh, ACCENT);
                fmt_u(t, i + 1u); vg_text(t, (xa + xb) / 2, Y + Hh / 2 - 14, VG_BIG, ACCENT, VG_CENTER);
            }
        }
    }
    if (!rec_here && sm_region(p, &a, &b, &hi)) {   /* where it plays now */
        x0 = chop ? a : 0u; x1 = chop ? b : hi;
        if (!chop && o3_ms - sm_turn_t < 1500u) x0 = x1 = 0u;   /* (zoomed: no playheads) */
        for (uint32_t v = card_lo(p); v < card_lo(p) + NCARD && x1 > x0; v++)
            if (vc[v].type == T_SMP && !vc[v].sm_done) { int x = sm_x(vc[v].sm_i, x0, x1, X, Wd); if (x >= 0) vg_rect(x, Y, 1, Hh, 0xFFFFFF); }
    }
    {   /* the line under the wave: what it is */
        char t[64]; const uint8_t *v = live_p[sel].v;
        t[0] = 0; scat(t, SMCHOP_NAME[v[P_SM_CHOP] % 3u]); scat(t, " · "); scat(t, SMPM_NAME[v[P_SM_PMODE] % 3u]);
        if (v[P_SM_REV]) scat(t, " · REVERSE");
        scat(t, " · "); scat(t, SMQ_NAME[v[P_SM_QUAL] & 3u]);
        if (chop && sm_n[p] > 1u) { scat(t, " · "); fmt_u(t + slen(t), sm_n[p]); scat(t, " SLICES"); if (set.smc_n[p]) scat(t, " BY HAND"); }
        if (!chop && hi) { scat(t, " · "); fmt_secs(t + slen(t), hi); }
        vg_text(t, 240, 204, VG_MED, DIM, VG_CENTER);
    }
    for (int i = 0; i < 4; i++) {               /* the four values, as columns (the knobs turn them) */
        int cx = 70 + i * 113;
        if (!k[i].label) continue;
        vg_text(k[i].label, cx, 226, VG_SMALL, MUTED, VG_CENTER);
        vg_text(k[i].value, cx, 242, VG_BIG, FG, VG_CENTER);
    }
}
static const char *const *smp_soft(void)
{
    static char f[6][12]; static const char *pf[6];
    const uint8_t *v = live_p[sel].v; int chop = page == PG_S1;
    static const char *const SRC_S[NAUSRC] = {"IN 1", "IN 2", "RESAMP"};
    for (int i = 0; i < 6; i++) { f[i][0] = 0; pf[i] = f[i]; }
    if (!chop) { scat(f[0], "LOAD"); scat(f[1], SRC_S[AU.src[sel % NAU] % NAUSRC]); scat(f[2], SMPM_NAME[v[P_SM_PMODE] % 3u]); scat(f[3], v[P_SM_REV] ? "REV" : "FWD"); }
    else { scat(f[0], "ADD CUT"); scat(f[1], "DEL CUT"); scat(f[2], "EVEN"); scat(f[3], "HITS"); }
    scat(f[4], v[P_SM_CHOP] % 3u ? "KEYS CHOP" : "KEYS MEL"); scat(f[5], "TRACKS");
    return pf;
}
static void tracks_key(void);
static void smp_fkey(int i)
{
    uint32_t t = sel % NTRK; int chop = page == PG_S1;
    patch_t *d = &live_p[t]; uint8_t *v = d->v;
    if (i == 5) { tracks_key(); return; }
    if (i == 4) {                               /* KEYS: the sample over the keys (MEL) or the cuts (CHOP) */
        v[P_SM_CHOP] = (uint8_t)(v[P_SM_CHOP] % 3u ? 0u : 1u); trk_edited[t] = 1;   /* (0.25: the cuts stay as they are) */
        say(v[P_SM_CHOP] ? "KEYS CHOP – C3 = SLICE 1, C#3 = SLICE 2 …" : "KEYS MEL – THE SAMPLE IN TUNE, C3 = AS RECORDED");
        return;
    }
    if (!chop) {
        if (i == 0) { files_open(0); files_kind(FK_AUDIO); f_ttrk = t % NAU; say("LOAD A WAV ONTO THIS TRACK – ENTER"); return; }
        if (i == 1) {                           /* INPUT: what REC takes */
            AU.src[t % NAU] = (uint8_t)((AU.src[t % NAU] + 1u) % NAUSRC); AU.st[t % NAU] = AU.src[t % NAU] == AU_RES ? 1u : 0u; aud_touch(); au_apply();
            say(AU.src[t % NAU] == AU_RES ? "REC TAKES WHAT YOU HEAR (RESAMPLE)" : AU.src[t % NAU] == AU_IN2 ? "REC TAKES IN 2" : "REC TAKES IN 1");
            return;
        }
        if (i == 2) { v[P_SM_PMODE] = (uint8_t)((v[P_SM_PMODE] + 1u) % 3u); trk_edited[t] = 1; say(v[P_SM_PMODE] == 0 ? "1-SHOT – PLAYS TO THE END" : v[P_SM_PMODE] == 1 ? "GATE – PLAYS WHILE HELD" : "LOOP – START TO END WHILE HELD"); return; }
        if (i == 3) { v[P_SM_REV] ^= 1u; trk_edited[t] = 1; say(v[P_SM_REV] ? "REVERSE" : "FORWARD"); return; }
        return;
    }
    if (i == 0) { sm_cut_add(t); return; }
    if (i == 1) { sm_cut_del(t); return; }
    if (i == 2) { if (v[P_SM_CHOP] % 3u) v[P_SM_CHOP] = 1; trk_edited[t] = 1; sm_auto(t); say("EVEN – THE CUTS EQUALLY APART (KNOB 3: HOW MANY)"); return; }
    if (i == 3) {                               /* HITS: the cuts jump to where something starts – then move them by hand as before */
        uint8_t was = v[P_SM_CHOP] % 3u ? 1u : 0u;   /* (0.25: the keys stay MEL or CHOP) */
        sm_auto(t); v[P_SM_CHOP] = 2; sm_ver[t % 8u]++; sm_update();
        if (sm_n[t % 8u] > 1u) { set.smc_n[t % 8u] = 0; sm_hand_from_now(t); }
        v[P_SM_CHOP] = was; trk_edited[t] = 1; sm_ver[t % 8u]++; set_touch();
        say("HITS – THE CUTS ON THE HITS · KNOB 2 MOVES ONE"); return;
    }
}
