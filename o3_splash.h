/* SPDX-License-Identifier: GPL-3.0-only */
/* BOOM – the start: the arcade picture (bm_splash.h) strikes in, stands until 3 s (any control ends it sooner). */
#include "bm_splash.h"
static uint16_t bspl_px[BSPL_W * BSPL_H];       /* unpacked once */
static int splash_on = 1;
static void splash_unpack(void)
{
    uint32_t i = 0, k = 0, n = BSPL_W * BSPL_H;
    while (i < n && k + 1u < sizeof BSPL_RLE) { uint32_t run = BSPL_RLE[k], c = BSPL_PAL[BSPL_RLE[k + 1u] & 63u]; k += 2u; while (run-- && i < n) bspl_px[i++] = (uint16_t)c; }
}
static uint16_t blend565(uint16_t a, uint16_t b, uint32_t f)   /* a -> b by f/256 */
{
    int32_t ar = a >> 11, ag = a >> 5 & 63, ab = a & 31, br = b >> 11, bg = b >> 5 & 63, bb = b & 31, k = (int32_t)f;
    return (uint16_t)((ar + (br - ar) * k / 256) << 11 | (ag + (bg - ag) * k / 256) << 5 | (ab + (bb - ab) * k / 256));
}
static void splash_draw(uint16_t *fb)
{
    static const uint16_t STRIKE[10] = {0, 120, 30, 0, 220, 80, 40, 256, 200, 256};   /* per 40 ms: two flashes, then it holds */
    uint32_t k = o3_ms / 40u, fade = k < 10u ? STRIKE[k] : 256u;
    const uint16_t bg = 0x0824;
    vg_clear(0x000000);
    for (uint32_t y = 0; y < BSPL_H; y++) {
        uint16_t *p = fb + (25u + y) * VIBE_SCREEN_W; const uint16_t *s = bspl_px + y * BSPL_W;
        if (fade >= 256u) for (uint32_t x = 0; x < BSPL_W; x++) p[x] = s[x];
        else for (uint32_t x = 0; x < BSPL_W; x++) p[x] = blend565(bg, s[x], fade);
    }
    if (o3_ms >= 3000u) splash_on = 0;
}
