/* gcc_check.c -- GCC-toolchain SRC-side driver, win32_pilot.md SS6a x87
 * experiment (see harness/GCC_X87.md for the results this produces).
 *
 * Same wire protocol as src_check.c/native_check.c/lift_check.c (guest
 * image in, vector file in, result file out), so lift_check.py's existing
 * vector generation, unicorn oracle and diff logic (carrier/lift/README.md
 * SS7, win32_pilot.md SS7a) are reused completely unchanged -- only
 * --exe/--toolchain point at this binary instead of src_check.exe.
 *
 * Restricted to the two functions this experiment needs:
 *
 *   line_intersect  the SS6a x87 discriminator: src_check.exe (32-bit MSVC,
 *                   double/SSE arithmetic, artifacts/src_equivalence.json)
 *                   DIFFERs on 997/20000 vectors. Question this driver
 *                   answers: does the SAME, UNMODIFIED
 *                   src/icytower/line_intersect.c become bit-equal when
 *                   compiled by 32-bit GCC with real x87 (-mfpmath=387)?
 *   jump_player     control: its only FP ops (sx+sx, sx*-2.0, comparisons)
 *                   are exact under any x87 precision, so EQUAL is expected
 *                   under every toolchain/flag combination; a DIFFER here
 *                   would mean something is wrong with the harness itself,
 *                   not with x87 precision.
 *   new_rand        batch 4 (2026-09-07): the game's own x87 float LCG
 *                   (main.c) -- every intermediate stays in an ordinary
 *                   automatic `double` (src/icytower/new_rand.c), same
 *                   register-residency question as line_intersect.
 *   update_particle,
 *   create_particle batch 4: integer-only callers of new_rand -- included
 *                   here (not just line_intersect/jump_player) because
 *                   they are the reason new_rand needed this same GCC
 *                   x87 build in the first place (PROMOTIONS.md batch 3's
 *                   "Skipped this pass" note).
 *   ok_to_play      batch 4: trivial control, no FP, no globals.
 *   add_floor       add_floor pass (2026-09-07): the tower layout
 *                   generator (map.c) -- has its own x87-sensitive width
 *                   formula (a fidivr/fmuls kept on the x87 stack, same
 *                   shape as line_intersect/new_rand) AND calls the game's
 *                   own rand() (redirected to harness_rand() by
 *                   pf_harness_rand.h, force-included below -- see that
 *                   header's comment for why: unicorn has no msvcrt.dll
 *                   mapped, so lift_check.py's ORIGINAL side hooks
 *                   add_floor's `call 0x4bad18` in Python instead; this
 *                   driver's job is to make the COMPILED side draw from
 *                   the identical LCG, seeded the same way per vector).
 *                   Also calls get_demo() (main_state.c, linked in below),
 *                   so this driver supplies `demo` directly, synced from
 *                   G_DEMO the same way seed/collision_type/max_speed
 *                   already are.
 *
 * Deliberately its own, wholly GCC-compiled executable -- no object file
 * from this build is ever linked against an MSVC-built one. That sidesteps
 * the name-mangling question entirely (32-bit cdecl leading-underscore
 * decoration happens to match between MSVC and MinGW GCC on Windows, but
 * this driver does not need to rely on that: build_src_gcc.sh's `gcc … -o
 * gcc_check.exe` links this file, line_intersect.c and jump_player.c in one
 * invocation, entirely within GCC's own linker). See build_src_gcc.sh /
 * build_src_gcc.cmd for the exact command lines tried (-m32, -mfpmath=387,
 * -O2/-O1/-O0, with and without -ffloat-store).
 *
 * src/icytower/line_intersect.c and .../jump_player.c compile completely
 * UNCHANGED -- this file is the only additive harness plumbing, exactly
 * like src_check.c already is for the MSVC build (see that file's header
 * comment for the fuller memory-model rationale, which applies unchanged
 * here).
 *
 * MEMORY MODEL, jump_player's two globals: unlike update_frame.c/is_solid.c
 * (bound through carrier/gen/pf_bindings_harness.h's generated PF_MEM()
 * macros under MSVC), this driver deliberately does NOT force-include a
 * bindings header or set ICYTOWER_BINDINGS_ACTIVE -- doing so would also
 * suppress game_types.h's own struct definitions (its "carrier's own type
 * provider has already defined these" branch, game_types.h SS30-34), which
 * this GCC build has no substitute for and does not want one (see
 * pf_bindings_gcc_min.h, kept in the repo but unused by build_src_gcc.sh --
 * this simpler route turned out cheaper). Instead, jump_player.c compiles
 * in the plain STANDALONE world (game_state.h's `extern double
 * max_speed[5]; extern int collision_type;`, win32_pilot.md SS7a), and this
 * driver supplies their storage directly, syncing it from the guest image
 * at the two known VAs immediately before every jump_player() call --
 * read-only globals jump_player.c never writes, so no write-back is needed.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game_types.h"
#include "game_state.h"
#include "pf_harness_rand.h"    /* #define rand harness_rand -- see that
                                 * header's comment; applies to every
                                 * src/icytower file compiled alongside this
                                 * driver in the same invocation, none of
                                 * which call plain rand() except map.c */
#include "pf_harness_calltrace.h"   /* #define play_sound harness_trace_play_sound
                                     * -- batch 7 (2026-09-07), play_jump_sound.c's
                                     * own play_sound() call (mechanism B); see
                                     * that header's comment. */

#define PF_GUEST_BASE 0x400000u
#define PF_GUEST_SIZE 0x400000u          /* 0x400000 .. 0x800000 */
#define PF_MEM(a) ((void *)(pf_guest + ((unsigned int)(a) - PF_GUEST_BASE)))

#define G_PLAYER_ID     0x4fe518u
#define G_PLY           0x4ff128u
#define G_COLLISION_TYPE 0x4dd140u
#define G_MAX_SPEED      0x4bdb80u
#define G_SEED           0x4ff108u
#define G_DEMO           0x4dd250u
#define RAND_SEED_VA     0x794020u
#define G_GRAVITY_MOD    0x4bdba8u
#define G_CUSTOM_JUMP_SOUND 0x4fabf4u   /* Tcustom.jump_sound[3] (custom @0x4fa738+1212) */
#define G_ITRCHECK       0x4dd168u
#define G_OPTIONS_FLASH  0x4fe528u
#define G_REWARD_TIME    0x4fec68u
#define G_REWARD_SCALE   0x4fac28u
#define G_REWARD_BMP     0x4f8af8u
#define G_COMBO_SOUND    0x4dd280u
#define G_STARS          0x4facc8u
#define G_DATA           0x4dd23cu
#define G_MAP            0x4f8b18u
#define G_ANY11          0x4dd170u
#define G_ANY12          0x4dd174u
#define G_ANY21          0x4dd17cu
#define G_ANY22          0x4dd180u
#define G_ANY23          0x4dd184u
#define G_SOUNDS         0x4dd2e0u

/* storage for game_state.h's extern decls -- the STANDALONE world's
 * contract (win32_pilot.md SS7a: "a state.c defines the globals"); this
 * driver plays that role for the globals line_intersect.c/jump_player.c/
 * new_rand.c read. Unlike collision_type/max_speed (read-only from
 * jump_player's side), `seed` is read-WRITE -- new_rand.c mutates it, and
 * that mutation is exactly what batch 4's offline check compares -- so it
 * is synced from the guest image before every call AND written back after. */
int collision_type;
double max_speed[5];
double seed;
Treplay *demo;    /* add_floor's/update_player's get_demo() reads this global directly */
double gravity_modifier[3];   /* update_player.c (batch 6) */
/* main_state.c (get_demo, linked in for add_floor) also defines
 * get_controls()/switchedFromProgram()/switchedToProgram()/
 * clickedCloseButton(), which this driver never calls but which still
 * need SOME storage to satisfy the linker -- unused by add_floor, values
 * never read or written by this driver. */
Tcontrol ctrl;
int hasFocus;
int closeButtonClicked;
Tcustom custom;            /* play_jump_sound.c (batch 7) reads custom.jump_sound[0..2] */
DATAFILE *data;            /* call_trace_stubs.c's asset_bitmap() needs this symbol; also
                             * start_reward.c's own indirect read through asset_bitmap() */
int itrcheck;
Toptions options;
int reward_time;
fixed reward_scale;
BITMAP *reward_bmp;
SAMPLE *combo_sound[10];
Tparticle stars[512];
Tmap map;
int player_id;
Tplayer *ply[1000];
int any11, any12, any13, any21, any22, any23;
SAMPLE *sounds[9];

unsigned char *pf_guest = 0;
static unsigned char *pf_pristine = 0;

void pf_trap(unsigned int va, const char *why)
{
    fprintf(stderr, "PF_TRAP at %08X: %s\n", va, why);
    exit(3);
}

extern int jump_player(Tplayer *, int);
extern int line_intersect(int, int, int, int, int, int, int, int, int *, int *);
extern int new_rand(void);
extern void update_particle(Tparticle *);
extern int create_particle(Tparticle *, int, int);
extern int ok_to_play(void);
extern void add_floor(Tmap *);
extern void reset_player(Tplayer *);
extern void update_player(Tplayer *);
extern void play_jump_sound(Tplayer *);
extern int  start_reward(int);
extern void handle_player_collision_original(int, int);

static unsigned int rd32(FILE *f)
{
    unsigned int v = 0;
    if (fread(&v, 4, 1, f) != 1) { fprintf(stderr, "short read\n"); exit(2); }
    return v;
}

/* tr() -- guest VA -> host pointer, identical in effect to src_check.c's
 * tr() (see that file's header comment): a vector's pointer-shaped
 * arguments arrive as plain guest VAs, and this driver knows the harness's
 * memory model (src/ itself never does), so the translation happens here,
 * at the call site, not inside the recovered functions. */
static void *tr(unsigned int va)
{
    if (va >= PF_GUEST_BASE && va < PF_GUEST_BASE + PF_GUEST_SIZE)
        return PF_MEM(va);
    return (void *)(size_t)va;
}

int main(int argc, char **argv)
{
    FILE *fi, *fo;
    unsigned char *raw;
    unsigned int ndom, nvec, i, j, domlen = 0;
    unsigned int *domva, *domlen_i;
    const char *fn;

    if (argc != 5) {
        fprintf(stderr, "usage: gcc_check <guest.bin> <vectors.bin> <out.bin> <func>\n");
        return 2;
    }
    fn = argv[4];
    if (strcmp(fn, "line_intersect") != 0 && strcmp(fn, "jump_player") != 0 &&
        strcmp(fn, "new_rand") != 0 && strcmp(fn, "update_particle") != 0 &&
        strcmp(fn, "create_particle") != 0 && strcmp(fn, "ok_to_play") != 0 &&
        strcmp(fn, "add_floor") != 0 && strcmp(fn, "reset_player") != 0 &&
        strcmp(fn, "update_player") != 0 && strcmp(fn, "play_jump_sound") != 0 &&
        strcmp(fn, "start_reward") != 0 &&
        strcmp(fn, "handle_player_collision_original") != 0) {
        fprintf(stderr, "gcc_check only wires up line_intersect/jump_player/"
                        "new_rand/update_particle/create_particle/ok_to_play/"
                        "add_floor/reset_player/update_player/play_jump_sound/"
                        "start_reward/handle_player_collision_original; got '%s'\n", fn);
        return 2;
    }

    raw = (unsigned char *)malloc(PF_GUEST_SIZE + 8192);
    if (!raw) return 2;
    pf_guest = (unsigned char *)(((size_t)raw + 4095) & ~(size_t)4095);
    pf_pristine = (unsigned char *)malloc(PF_GUEST_SIZE);
    if (!pf_pristine) return 2;

    fi = fopen(argv[1], "rb");
    if (!fi) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    if (fread(pf_pristine, 1, PF_GUEST_SIZE, fi) != PF_GUEST_SIZE) {
        fprintf(stderr, "guest image is not 0x%X bytes\n", PF_GUEST_SIZE);
        return 2;
    }
    fclose(fi);

    fi = fopen(argv[2], "rb");
    if (!fi) { fprintf(stderr, "cannot open %s\n", argv[2]); return 2; }
    if (rd32(fi) != 0x564C4650u) { fprintf(stderr, "bad vector magic\n"); return 2; }
    if (rd32(fi) != PF_GUEST_SIZE) { fprintf(stderr, "image size mismatch\n"); return 2; }
    ndom = rd32(fi);
    domva = (unsigned int *)malloc(ndom * 4);
    domlen_i = (unsigned int *)malloc(ndom * 4);
    for (i = 0; i < ndom; i++) {
        domva[i] = rd32(fi);
        domlen_i[i] = rd32(fi);
        domlen += domlen_i[i];
    }
    nvec = rd32(fi);

    fo = fopen(argv[3], "wb");
    if (!fo) { fprintf(stderr, "cannot open %s\n", argv[3]); return 2; }
    { unsigned int m = 0x53455250u; fwrite(&m, 4, 1, fo); fwrite(&nvec, 4, 1, fo); }

    for (i = 0; i < nvec; i++) {
        unsigned int nargs, a[10], nwr, eax = 0;
        memcpy(pf_guest, pf_pristine, PF_GUEST_SIZE);
        nargs = rd32(fi);
        if (nargs > 10) { fprintf(stderr, "too many args\n"); return 2; }
        for (j = 0; j < nargs; j++) a[j] = rd32(fi);
        nwr = rd32(fi);
        for (j = 0; j < nwr; j++) {
            unsigned int va = rd32(fi), len = rd32(fi);
            if (va < PF_GUEST_BASE || va + len > PF_GUEST_BASE + PF_GUEST_SIZE) {
                fprintf(stderr, "write outside the guest image\n"); return 2;
            }
            if (fread(pf_guest + (va - PF_GUEST_BASE), 1, len, fi) != len) {
                fprintf(stderr, "short vector read\n"); return 2;
            }
        }

        if (!strcmp(fn, "line_intersect")) {
            int *px = (int *)tr(a[8]), *py = (int *)tr(a[9]);
            eax = (unsigned int)line_intersect((int)a[0], (int)a[1], (int)a[2], (int)a[3],
                                               (int)a[4], (int)a[5], (int)a[6], (int)a[7],
                                               px, py);
        } else if (!strcmp(fn, "jump_player")) {
            Tplayer *p = (Tplayer *)tr(a[0]);
            /* sync the two globals jump_player.c reads (see header comment)
             * from the guest image before the call; both are read-only from
             * jump_player's side, so no write-back afterwards. */
            collision_type = *(int *)(pf_guest + (G_COLLISION_TYPE - PF_GUEST_BASE));
            memcpy(max_speed, pf_guest + (G_MAX_SPEED - PF_GUEST_BASE), sizeof(max_speed));
            eax = (unsigned int)jump_player(p, (int)a[1]);
        } else if (!strcmp(fn, "new_rand")) {
            seed = *(double *)(pf_guest + (G_SEED - PF_GUEST_BASE));
            eax = (unsigned int)new_rand();
            *(double *)(pf_guest + (G_SEED - PF_GUEST_BASE)) = seed;
        } else if (!strcmp(fn, "update_particle")) {
            Tparticle *p = (Tparticle *)tr(a[0]);
            seed = *(double *)(pf_guest + (G_SEED - PF_GUEST_BASE));
            update_particle(p);
            *(double *)(pf_guest + (G_SEED - PF_GUEST_BASE)) = seed;
            eax = 0;
        } else if (!strcmp(fn, "create_particle")) {
            Tparticle *p = (Tparticle *)tr(a[0]);
            seed = *(double *)(pf_guest + (G_SEED - PF_GUEST_BASE));
            eax = (unsigned int)create_particle(p, (int)a[1], (int)a[2]);
            *(double *)(pf_guest + (G_SEED - PF_GUEST_BASE)) = seed;
        } else if (!strcmp(fn, "ok_to_play")) {
            eax = (unsigned int)ok_to_play();
        } else if (!strcmp(fn, "add_floor")) {
            Tmap *m = (Tmap *)tr(a[0]);
            demo = (Treplay *)tr(*(unsigned int *)(pf_guest + (G_DEMO - PF_GUEST_BASE)));
            harness_rand_state = *(unsigned int *)(pf_guest + (RAND_SEED_VA - PF_GUEST_BASE));
            add_floor(m);
            eax = 0;
        } else if (!strcmp(fn, "reset_player")) {
            Tplayer *p = (Tplayer *)tr(a[0]);
            reset_player(p);
            eax = 0;
        } else if (!strcmp(fn, "update_player")) {
            Tplayer *p = (Tplayer *)tr(a[0]);
            /* same globals jump_player.c/add_floor's blocks above sync by
             * hand: collision_type/max_speed (read-only), gravity_modifier
             * (read-only), demo (read-only pointer VALUE, translated). */
            collision_type = *(int *)(pf_guest + (G_COLLISION_TYPE - PF_GUEST_BASE));
            memcpy(max_speed, pf_guest + (G_MAX_SPEED - PF_GUEST_BASE), sizeof(max_speed));
            memcpy(gravity_modifier, pf_guest + (G_GRAVITY_MOD - PF_GUEST_BASE), sizeof(gravity_modifier));
            demo = (Treplay *)tr(*(unsigned int *)(pf_guest + (G_DEMO - PF_GUEST_BASE)));
            update_player(p);
            eax = 0;
        } else if (!strcmp(fn, "play_jump_sound")) {
            Tplayer *p = (Tplayer *)tr(a[0]);
            /* custom.jump_sound[0..2] -- the only part of `custom` this
             * function reads. */
            memcpy(custom.jump_sound, pf_guest + (G_CUSTOM_JUMP_SOUND - PF_GUEST_BASE),
                   sizeof(custom.jump_sound));
            play_jump_sound(p);
            eax = 0;
        } else if (!strcmp(fn, "start_reward")) {
            itrcheck = *(int *)(pf_guest + (G_ITRCHECK - PF_GUEST_BASE));
            memcpy(&options, pf_guest + (G_OPTIONS_FLASH - PF_GUEST_BASE), sizeof(int));
            reward_time = *(int *)(pf_guest + (G_REWARD_TIME - PF_GUEST_BASE));
            reward_scale = *(fixed *)(pf_guest + (G_REWARD_SCALE - PF_GUEST_BASE));
            reward_bmp = *(BITMAP **)(pf_guest + (G_REWARD_BMP - PF_GUEST_BASE));
            memcpy(combo_sound, pf_guest + (G_COMBO_SOUND - PF_GUEST_BASE), sizeof(combo_sound));
            /* `data` stays a RAW guest VA here (NOT tr()-translated): unlike
             * ply[player_id]/demo above, this driver never dereferences
             * `data` itself -- call_trace_stubs.c's asset_bitmap() does,
             * and translates it via PF_MEM() there (the same function body
             * used by the MSVC harness, where `data` is a macro yielding a
             * raw guest VA too -- keeping this copy raw here is what keeps
             * that one shared implementation correct in both worlds). */
            data = (DATAFILE *)(size_t)(*(unsigned int *)(pf_guest + (G_DATA - PF_GUEST_BASE)));
            memcpy(stars, pf_guest + (G_STARS - PF_GUEST_BASE), sizeof(stars));
            seed = *(double *)(pf_guest + (G_SEED - PF_GUEST_BASE));
            eax = (unsigned int)start_reward((int)a[0]);
            *(int *)(pf_guest + (G_REWARD_TIME - PF_GUEST_BASE)) = reward_time;
            *(fixed *)(pf_guest + (G_REWARD_SCALE - PF_GUEST_BASE)) = reward_scale;
            *(BITMAP **)(pf_guest + (G_REWARD_BMP - PF_GUEST_BASE)) = reward_bmp;
            memcpy(pf_guest + (G_STARS - PF_GUEST_BASE), stars, sizeof(stars));
            *(double *)(pf_guest + (G_SEED - PF_GUEST_BASE)) = seed;
        } else { /* handle_player_collision_original */
            unsigned int pid;
            player_id = *(int *)(pf_guest + (G_PLAYER_ID - PF_GUEST_BASE));
            pid = (unsigned int)player_id;
            /* same ply[player_id] pointer-VALUE second translation
             * update_frame's own dispatch (src_check.c) needs -- this
             * function reads ply[player_id] internally, never as a
             * parameter. */
            ply[pid] = (Tplayer *)tr(*(unsigned int *)(pf_guest + (G_PLY + 4u * pid - PF_GUEST_BASE)));
            memcpy(&map, pf_guest + (G_MAP - PF_GUEST_BASE), sizeof(map));
            any11 = *(int *)(pf_guest + (G_ANY11 - PF_GUEST_BASE));
            any12 = *(int *)(pf_guest + (G_ANY12 - PF_GUEST_BASE));
            any21 = *(int *)(pf_guest + (G_ANY21 - PF_GUEST_BASE));
            any22 = *(int *)(pf_guest + (G_ANY22 - PF_GUEST_BASE));
            any23 = *(int *)(pf_guest + (G_ANY23 - PF_GUEST_BASE));
            memcpy(sounds, pf_guest + (G_SOUNDS - PF_GUEST_BASE), sizeof(sounds));
            handle_player_collision_original((int)a[0], (int)a[1]);
            eax = 0;
            /* ply[pid] itself points INTO pf_guest (tr()'s own contract),
             * so any Tplayer field writes already landed there directly --
             * only the plain-int globals need writing back. */
            *(int *)(pf_guest + (G_ANY11 - PF_GUEST_BASE)) = any11;
            *(int *)(pf_guest + (G_ANY12 - PF_GUEST_BASE)) = any12;
            *(int *)(pf_guest + (G_ANY21 - PF_GUEST_BASE)) = any21;
            *(int *)(pf_guest + (G_ANY22 - PF_GUEST_BASE)) = any22;
            *(int *)(pf_guest + (G_ANY23 - PF_GUEST_BASE)) = any23;
        }

        fwrite(&eax, 4, 1, fo);
        for (j = 0; j < ndom; j++)
            fwrite(pf_guest + (domva[j] - PF_GUEST_BASE), 1, domlen_i[j], fo);
    }
    fclose(fi);
    fclose(fo);
    printf("gcc side: %u vectors, %u domain bytes each\n", nvec, domlen);
    return 0;
}
