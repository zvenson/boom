/* SPDX-License-Identifier: GPL-3.0-only */
/* OberVave 3 – MENU: what every control does on the page shown, laid out as on the device (2.0's overview).
 * Opened by HOME (Protokoll 3.2, every firmware) or MENU. Hold SHIFT: the second layer. Closed by HOME / MENU again,
 * RETURN or another area. HOME held 1 s / SHIFT+HOME: the library (the host). */
static void ov_cell(int x, int y, int w, int h, const char *name, const char *what, const char *sub, uint32_t col)
{
    vg_round(x, y, w, h, 4, 0x24104A);
    if (col) vg_rect(x, y, 3, h, col);
    vg_text(name, x + 7, y + 3, VG_SMALL, DIM, VG_LEFT);
    vg_text(what && what[0] ? what : "–", x + 7, y + 17, VG_BOLD, what && what[0] ? FG : MUTED, VG_LEFT);
    if (sub && sub[0]) vg_text(sub, x + w - 5, y + 3, VG_SMALL, 0x00E5FF, VG_RIGHT);
}
static void overview(void)
{
    knob_t k[4];
    char t[40];
    vg_rect(0, 0, VIBE_SCREEN_W, VIBE_SCREEN_H, 0x000000);
    vg_text("CONTROLS", 10, 6, VG_MED, ACCENT, VG_LEFT);
    t[0] = 0; scat(t, in_seq() ? SV_NAME[SV_EFF] : PG(page)->title); scat(t, " · TRACK "); { char l[2] = {(char)('A' + sel), 0}; scat(t, l); }
    vg_text(t, 104, 9, VG_SMALL, FG, VG_LEFT);
    vg_text(shift ? "with SHIFT" : "hold SHIFT: 2nd layer", 470, 9, VG_SMALL, shift ? 0x00E5FF : MUTED, VG_RIGHT);
    int sq = in_seq(), entry = sq && (SV_ENTRY(seq_view) || seq_view == SV_SLOTS);
    if (page == PG_MIXER) mixer_knobs(k); else if (sq) seq_knobs(k); else if (page == PG_SCALE) scale_knobs(k); else page_knobs(k);
    ov_cell(6, 30, 92, 40, "↩", k_lock ? "K = NOTES" : "K LOCK", "HELD: UNDO", k_lock ? TRK_RGB[sel] : 0);   /* (3.8: held + ENC 1 back / forward, 2 s: one back) */
    for (int i = 0; i < 4; i++) { char n[2] = {(char)('1' + i), 0}; ov_cell(104 + i * 74, 30, 70, 40, n, k[i].label ? k[i].label : "", k[i].label ? k[i].value : "", ACCENT); }
    ov_cell(404, 30, 70, 40, "DATA", shift ? "TEMPO" : save_armed ? "SLOT" : sq && (SV_ENTRY(seq_view) || (seq_view == SV_TRACK && !au_is(sel) && !pat.trk[sel].tm)) ? "EDIT" : sq || page == PG_MIXER ? "PATTERN" : page == PG_GLOBAL ? "MIDI OUT" : "PATCH", "", ACCENT);
    { static const char *FN[6] = {"MIXER", "TRACKS", "LOAD", "SAVE", "STORE", "GLOBAL"}, *FSH[6] = {"", "", "OCT -", "OCT +", "HOLD", "UNDO"};
      for (int i = 0; i < 6; i++) { char n[3] = {'F', (char)('1' + i), 0}; ov_cell(6 + i * 78, 76, 74, 40, n, shift && FSH[i][0] ? FSH[i] : FN[i], "", 0x00E5FF); } }
    ov_cell(6, 122, 74, 40, "SHIFT", shift ? "HELD" : "2ND LAYER", "", shift ? 0x00E5FF : 0);
    ov_cell(84, 122, 74, 40, "SHIFT F3/F4", "OCTAVE", "", 0);
    ov_cell(162, 122, 70, 40, "ENTER", shift && page == PG_GLOBAL ? "RESET ALL" : f5_down ? "DICE" : page == PG_GLOBAL ? "SYNC" : "SELECT", "", shift && page == PG_GLOBAL ? RED : 0);
    for (int i = 0; i < 6; i++) {
        int x = 236 + i * 40, on = (sq ? CK_SEQ : key_lit()) == i;
        if (shift || k_lock) {                  /* (3.10) SHIFT + K (RETURN: locked): the scale's notes / chords */
            char nm[8]; sc_name(nm, (uint32_t)i);
            vg_round(x, 122, 37, 40, 4, vg_mix(0x000000, TRK_RGB[sel], i ? 0.45f : 0.8f));
            vg_text(nm, x + 18, 136, VG_BOLD, FG, VG_CENTER);
            continue;
        }
        vg_round(x, 122, 37, 40, 4, on ? KEY_RGB[i] : vg_mix(0x000000, KEY_RGB[i], 0.45f));
        vg_text(KEY_NAME[i], x + 18, 136, VG_BOLD, on ? 0x000000 : FG, VG_CENTER);
    }
    ov_cell(6, 170, 112, 40, "A", entry ? (seq_view == SV_SLOTS ? "SLOT" : "STEP ◀ ▶") : "TRACK A–H", "", GREEN);
    ov_cell(122, 170, 112, 40, "B", sq && seq_view == SV_STEPS && !shift ? "NOTE UP/DOWN" : sq && seq_view == SV_TRACK ? "TRANSPOSE" : "PATCH",
            sq && seq_view == SV_STEPS ? "SHIFT: PATCH" : "", GREEN);
    ov_cell(6, 216, 74, 40, "◀", "PRG -", "", 0); ov_cell(84, 216, 74, 40, "HOME", "CONTROLS", "", 0); ov_cell(162, 216, 72, 40, "▶", "PRG +", "", 0);
    ov_cell(6, 262, 74, 52, "REC", shift ? "CLEAR" : "RECORD", "", RED);
    ov_cell(84, 262, 74, 52, "STOP", "STOP", "", 0); ov_cell(162, 262, 72, 52, "PLAY", "START", "", GREEN);
    {   /* the pads, row by row */
        int dr = trk_type(sel) == T_DRUMS;
        static const char *const TROWS6[4] = {"START A–D", "START E–H", "NOTES", "NOTES"};   /* (0.41: fixed; SHIFT + pad: choose) */
        const char *const ROWS[4] = {"START A–D", "START E–H", dr ? (live_p[sel].v[P_KIT_SRC] == 2u ? "KICK SNARE SNARE 2 RIM" : live_p[sel].v[P_KIT_SRC] ? "KICK SNARE TOM HI TOM LO" : "KICK SNARE CLAP RIM") : "NOTES", dr ? (live_p[sel].v[P_KIT_SRC] == 2u ? "HAT OPEN KICK 2 CRASH" : live_p[sel].v[P_KIT_SRC] ? "HAT OPEN RIDE CRASH" : "CH OH SNAP PERC") : set.sc_pads ? "NOTES" : "NOTES: SCALE"};
        static const char *const MROWS[4] = {"START A–D", "START E–H", "MUTE A–D (+SHIFT SOLO)", "MUTE E–H (+SHIFT SOLO)"};
        static const char *const SROWS[4] = {"STEPS 1–4", "STEPS 5–8", "STEPS 9–12", "STEPS 13–16"};
        const char *const *TROWS = TROWS6;
        int mx = page == PG_MIXER, st = sq && SV_ENTRY(seq_view), tr = sq && seq_view == SV_TRACKS;
        vg_text("PADS", 350, 172, VG_SMALL, DIM, VG_LEFT);
        for (int r = 0; r < 4; r++) {
            int y = 188 + r * 32;
            for (int c = 0; c < 4; c++) {
                uint32_t col = st ? KEY_RGB[CK_SEQ] : r < 2 ? TRK_RGB[(uint32_t)(r * 4 + c) % NTRK] : mx ? RED : DIM;   /* (0.41: rows 1-2 = A-H in their colours) */
                vg_round(242 + c * 26, y, 22, 26, 3, vg_mix(0x000000, col, 0.55f));
            }
            vg_text(st ? SROWS[r] : mx ? MROWS[r] : tr ? TROWS[r] : (r < 2 && rec) ? (r ? "RECORD E–H" : "RECORD A–D") : ROWS[r], 350, y + 7, VG_SMALL, FG, VG_LEFT);
            if (!st && r < 2) for (int c = 0; c < 4; c++) { char l[2] = {(char)('A' + r * 4 + c), 0}; vg_text(l, 253 + c * 26, y + 6, VG_BOLD, 0x000000, VG_CENTER); }   /* (0.41: the letter on its pad) */
        }
    }
}
