/* game_types.h -- the port's own type definitions.
 *
 * Covers exactly the structs update_frame.c and is_solid.c need: Tplayer,
 * Tmap, Tfloor, and the `fixed` (Allegro fixed-point) scalar. Initially
 * transcribed member-for-member from the DWARF-recovered carrier header
 * (carrier/gen/it_types.h), which is generated from the original binary's
 * debug info -- not from this file, and not the other way around.
 *
 * These layouts MUST stay binary-compatible with the ORIGINAL process
 * memory: today game state is still address-backed (win32_pilot.md SS7a --
 * "state stays address-backed, code ownership migrates first"), so any
 * `Tplayer *`/`Tmap *` this port's code touches may in fact be a pointer at
 * the original game's address, wearing this struct's layout as a lens. Field
 * order, size and packing here must therefore continue to match the
 * original compiler's layout exactly (see carrier/native/README.md SS2 for
 * the offsets this was checked against: Tplayer.dead @ 0x4c, .edge @ 0x58,
 * .edge_drawn @ 0x5c, .frame @ 0x3c; Tmap.offset @ 0x300). Once state
 * ownership migrates to this port (a later, separate step per SS7a), this
 * header becomes free to diverge from the original layout, and this comment
 * should go with it.
 *
 * When this file is compiled INTO the carrier (src/ built with the
 * generated bindings force-included, see game_state.h and src/README.md),
 * the carrier's own type provider has already defined these same names with
 * the same layout, so the bodies below are skipped rather than redefined --
 * see the ICYTOWER_BINDINGS_ACTIVE guard.
 */
#ifndef ICYTOWER_GAME_TYPES_H
#define ICYTOWER_GAME_TYPES_H

#ifndef ICYTOWER_BINDINGS_ACTIVE

#pragma pack(push, 1)

/* Allegro fixed-point value: 32-bit, 16.16. Plain 32-bit wraparound
 * arithmetic applies to it exactly as to any other `int`. */
typedef int fixed;

/* One row of the tower's floor grid (32 rows per Tmap, 16 px/tile). */
typedef struct Tfloor {
    int empty;          /* nonzero: no floor drawn in this row */
    int start_tile;     /* left edge of the floor span, in tile columns */
    int end_tile;       /* right edge of the floor span, in tile columns */
    int level;
    int sign;
    int tiles;
} Tfloor;

/* The tower's floor grid plus its current vertical scroll offset. */
typedef struct Tmap {
    Tfloor room[32];
    int offset;
} Tmap;

/* One player's live gameplay state. */
typedef struct Tplayer {
    double x;
    double y;
    double sx;
    double sy;
    double max_s;
    int level;
    int score;
    int best_combo;
    int status;
    int jump_key;
    int frame;               /* walk-cycle animation frame */
    int in_combo;
    int acc_level;
    int acc_jumps;
    int dead;                /* death-animation counter, 0 = alive */
    int rotate;
    fixed angle;
    int edge;                /* nonzero while standing on a floor edge */
    int edge_drawn;          /* ticks the edge indicator has been shown */
    int bounce;
    int shake;
    int latest_combo;
    int show_combo;
    int no_combo_top_floor;
    int biggest_lost_combo;
    int ccc[5];
    int jcTop[5];
    int jc[5];
    unsigned char _pad_0[4]; /* trailing compiler padding, kept explicit */
} Tplayer;

#pragma pack(pop)

#endif /* !ICYTOWER_BINDINGS_ACTIVE */

#endif /* ICYTOWER_GAME_TYPES_H */
