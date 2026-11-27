/* SPDX-License-Identifier: GPL-3.0-only */
/* BOOM's PIANO data. The recorded grand (bm_grand_private.h + bm_grand_d*.inc) is NOT part of the public source: it was
 * sampled from a digital piano whose sound is its maker's. With it (a private build) the PIANO kind plays it; without it
 * (this repository) BOOM_PIANO is 0: TYPE does not offer PIANO, and a saved PIANO track stays silent. */
#if BOOM_PIANO
#include "bm_grand_private.h"
#else
#define GP_VBITS 1
#define GP_SR 32000u
#define GP_NKEY 1u
#define GP_NCH 2u
#define GP_NLAY 2u
#define GP_KEY0 60u
#define GP_STEP 3u
#define GP_TUNE 0.0f
#define GP_CURVE 1.6f
#define GP_BLK 32u
static const uint32_t GP_AT[4] = {0, 1, 2, 3};      /* (four silent recordings: one block each, 0 bits) */
static const uint32_t GP_LEN[4] = {32, 32, 32, 32};
static const uint8_t GP_DATA[8] __attribute__((aligned(4))) = {0};
#endif
