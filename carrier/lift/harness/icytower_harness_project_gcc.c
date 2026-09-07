/* icytower_harness_project_gcc.c -- Icy Tower's own per-function dispatch
 * for the offline harness's standalone world (the GCC/x87 toolchain form,
 * win32_pilot.md SS6a's x87 experiment -- see harness/GCC_X87.md for the
 * results this build produces).
 *
 * Implements pf_harness_dispatch_gcc() (port_forge/tools/win32_oracle/
 * pf_harness_dispatch.h), called by the generic gcc_check.c. Restricted to
 * the functions this experiment's build script (build_src_gcc.sh/.cmd)
 * actually links:
 *
 *   line_intersect  the SS6a x87 discriminator: src_check.exe (32-bit
 *                   MSVC, double/SSE arithmetic) DIFFERs on a measurable
 *                   fraction of vectors. This build answers: does the
 *                   SAME, UNMODIFIED src/icytower/line_intersect.c become
 *                   bit-equal when compiled by 32-bit GCC with real x87
 *                   (-mfpmath=387)?
 *   jump_player     control: its only FP ops (sx+sx, sx*-2.0, comparisons)
 *                   are exact under any x87 precision, so EQUAL is
 *                   expected under every toolchain/flag combination; a
 *                   DIFFER here would mean something is wrong with the
 *                   harness itself, not with x87 precision.
 *   new_rand        the game's own x87 float LCG (main.c) -- every
 *                   intermediate stays in an ordinary automatic `double`
 *                   (src/icytower/new_rand.c), the same register-residency
 *                   question as line_intersect.
 *   update_particle,
 *   create_particle integer-only callers of new_rand -- included here
 *                   because they are the reason new_rand needed this same
 *                   GCC x87 build in the first place.
 *   ok_to_play      trivial control, no FP, no globals.
 *   add_floor       the tower layout generator (map.c) -- has its own
 *                   x87-sensitive width formula (a fidivr/fmuls kept on
 *                   the x87 stack, same shape as line_intersect/new_rand)
 *                   AND calls the game's own rand() (redirected to
 *                   harness_rand() by pf_harness_rand.h -- see that
 *                   header's comment for why: unicorn has no msvcrt.dll
 *                   mapped, so the ORIGINAL side's oracle hooks
 *                   add_floor's `call` to the _rand thunk instead; this
 *                   dispatch's job is to make the COMPILED side draw from
 *                   the identical LCG, seeded the same way per vector).
 *                   Also calls get_demo() (main_state.c, linked in below),
 *                   so this dispatch supplies `demo` directly, synced from
 *                   G_DEMO the same way seed/collision_type/max_speed are.
 *
 * Deliberately built into its own, wholly GCC-compiled executable -- no
 * object file from this build is ever linked against an MSVC-built one
 * (see the project's build_src_gcc.sh for the exact command lines).
 *
 * src/icytower/*.c compile completely UNCHANGED -- this file is the only
 * additive harness plumbing, exactly like icytower_harness_project.c is
 * for the MSVC/src_check.exe build (see that file's header comment for the
 * fuller memory-model rationale, which applies unchanged here).
 *
 * MEMORY MODEL, jump_player's two globals: unlike update_frame.c/
 * is_solid.c (bound through carrier/gen/pf_bindings_harness.h's generated
 * PF_MEM() macros under the MSVC/src_check.exe build), this dispatch
 * deliberately does NOT force-include a bindings header or set
 * ICYTOWER_BINDINGS_ACTIVE (doing so would also suppress game_types.h's
 * own struct definitions, which this GCC build has no substitute for and
 * does not want one). Instead, jump_player.c compiles in the plain
 * STANDALONE world (game_state.h's `extern double max_speed[5]; extern
 * int collision_type;`), and this file supplies their storage directly,
 * syncing it from the guest image at the two known VAs immediately before
 * every jump_player() call -- read-only globals jump_player.c never
 * writes, so no write-back is needed.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game_types.h"
#include "game_state.h"
#include "allegro_api.h"            /* batch 11: JOYSTICK_INFO joy[8] -- the only
                                     * Allegro type/global poll_control needs that
                                     * allegro_types.h does not itself carry
                                     * (`key`/`screen` were already declared here
                                     * too; this file's own definitions below match
                                     * those declarations). */
#include "pf_harness_rand.h"        /* #define rand harness_rand -- see that
                                     * header's comment; applies to every
                                     * src/icytower file compiled alongside
                                     * this dispatch in the same invocation,
                                     * none of which call plain rand()
                                     * except map.c */
#include "pf_harness_calltrace.h"   /* #define play_sound harness_trace_play_sound
                                     * (mechanism B) -- play_jump_sound.c's
                                     * own play_sound() call; also redirects
                                     * set_clip_rect/textout_ex/
                                     * textout_centre_ex for draw_scroller.c. */
#include "pf_harness_msvc_types.h"  /* #define __int64 long long -- see that
                                     * header's comment: draw_scroller.c
                                     * #includes "allegro_api.h" directly. */
#include "pf_harness_mem.h"
#include "pf_harness_dispatch.h"

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
/* -- batch 9: src/icytower/collision.c's four variants -- */
#define G_DEBUG          0x4dd160u      /* int debug (no store anywhere in the image) */
#define G_KEY            0x506988u      /* Allegro volatile char key[127] */
#define G_SCREEN         0x4dda8cu      /* Allegro BITMAP *screen */
#define SCREEN_VTABLE_VA 0x7c3100u      /* scratch GFX_VTABLE (icytower_specs.py) */

/* storage for game_state.h's extern decls -- the STANDALONE world's
 * contract (a project's own state.c defines the globals in every OTHER
 * build; this dispatch plays that role for the globals line_intersect.c/
 * jump_player.c/new_rand.c/etc. read). Unlike collision_type/max_speed
 * (read-only from jump_player's side), `seed` is read-WRITE -- new_rand.c
 * mutates it, and that mutation is exactly what the offline check
 * compares -- so it is synced from the guest image before every call AND
 * written back after. */
int collision_type;
double max_speed[5];
double seed;
Treplay *demo;    /* add_floor's/update_player's get_demo() reads this global directly */
double gravity_modifier[3];   /* update_player.c */
/* main_state.c (get_demo, linked in for add_floor) also defines
 * get_controls()/switchedFromProgram()/switchedToProgram()/
 * clickedCloseButton(), which this dispatch never calls but which still
 * need SOME storage to satisfy the linker -- unused here, values never
 * read or written by this dispatch. */
Tcontrol ctrl;
int hasFocus;
int closeButtonClicked;
Tcustom custom;            /* play_jump_sound.c reads custom.jump_sound[0..2] */
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
/* batch 9: collision.c's debug-overlay gate and its draw target. `key` and
 * `screen` are Allegro's own globals (allegro_api.h declares them in this
 * standalone world); `debug` is the game's. */
int debug;
volatile char key[127];
BITMAP *screen;
/* -- batch 11: the input seam (poll_control.c, handle_player_input.c) -- */
int recording;
int rec_pos;
int rejump;
Tprofile *profile;
Tgamepad gamepad;
JOYSTICK_INFO joy[8];

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
extern int  draw_scroller(Tscroller *, BITMAP *, int, int, int);
extern void handle_player_collision_old(int, int);
extern void handle_player_collision_vector(int, int);
extern void handle_player_collision_vector_2(int, int);
extern void handle_player_collision_combo(int, int);
extern void poll_control(Tcontrol *, int);
extern void handle_player_input(Tcontrol *);

/* -- batch 11 guest VAs (icytower_specs.py's own constants) -- */
#define G_RECORDING     0x4f8e28u
#define G_REC_POS       0x4fec58u
#define G_REJUMP        0x4fdcd8u
#define G_PROFILE       0x4dd27cu
#define G_GAMEPAD       0x4f8748u
#define G_JOY           0x506a88u
#define REPLAY_DATA_OFF 0x8a8u

/* batch 9: the shared pre/post sync for src/icytower/collision.c's four
 * variants. All four read ply[player_id] and `map` internally (never as a
 * parameter), write the any11..any23 globals (two of them do), read
 * sounds[8], and -- inside their `debug && key[KEY_F2]` overlay gate --
 * `screen`. The one thing that is NOT a plain sync: the vtable `line`
 * slot. The vector generator puts a SYNTHETIC guest VA (VTABLE_LINE_VA)
 * there so the ORIGINAL side's Oracle can hook it; the compiled side needs
 * a real host function pointer in the same slot instead, so this dispatch
 * overwrites it with harness_trace_line() after translating both the
 * BITMAP and the GFX_VTABLE pointer VALUES -- the same "second
 * translation, driver-side" pattern ply[player_id]/demo/data already
 * needed, one level deeper. Neither the BITMAP nor the vtable is part of
 * the comparison domain, so rewriting them here is invisible to the diff. */
static void collision_pre(void)
{
    unsigned int pid;
    player_id = *(int *)(pf_guest + (G_PLAYER_ID - PF_GUEST_BASE));
    pid = (unsigned int)player_id;
    ply[pid] = (Tplayer *)pf_tr(*(unsigned int *)(pf_guest + (G_PLY + 4u * pid - PF_GUEST_BASE)));
    memcpy(&map, pf_guest + (G_MAP - PF_GUEST_BASE), sizeof(map));
    any11 = *(int *)(pf_guest + (G_ANY11 - PF_GUEST_BASE));
    any12 = *(int *)(pf_guest + (G_ANY12 - PF_GUEST_BASE));
    any21 = *(int *)(pf_guest + (G_ANY21 - PF_GUEST_BASE));
    any22 = *(int *)(pf_guest + (G_ANY22 - PF_GUEST_BASE));
    any23 = *(int *)(pf_guest + (G_ANY23 - PF_GUEST_BASE));
    memcpy(sounds, pf_guest + (G_SOUNDS - PF_GUEST_BASE), sizeof(sounds));
    debug = *(int *)(pf_guest + (G_DEBUG - PF_GUEST_BASE));
    memcpy((void *)key, pf_guest + (G_KEY - PF_GUEST_BASE), sizeof(key));
    screen = (BITMAP *)pf_tr(*(unsigned int *)(pf_guest + (G_SCREEN - PF_GUEST_BASE)));
    if (screen != 0) {
        screen->vtable = (GFX_VTABLE *)pf_tr(SCREEN_VTABLE_VA);
        screen->vtable->line = harness_trace_line;
    }
}

static void collision_post(void)
{
    /* ply[pid] points INTO pf_guest, so the Tplayer writes already landed
     * there; only the plain-int globals need writing back. */
    *(int *)(pf_guest + (G_ANY11 - PF_GUEST_BASE)) = any11;
    *(int *)(pf_guest + (G_ANY12 - PF_GUEST_BASE)) = any12;
    *(int *)(pf_guest + (G_ANY21 - PF_GUEST_BASE)) = any21;
    *(int *)(pf_guest + (G_ANY22 - PF_GUEST_BASE)) = any22;
    *(int *)(pf_guest + (G_ANY23 - PF_GUEST_BASE)) = any23;
}

unsigned int pf_harness_dispatch_gcc(const char *fn, unsigned int *a, unsigned int nargs)
{
    unsigned int eax = 0;
    (void)nargs;

    if (!strcmp(fn, "line_intersect")) {
        int *px = (int *)pf_tr(a[8]), *py = (int *)pf_tr(a[9]);
        eax = (unsigned int)line_intersect((int)a[0], (int)a[1], (int)a[2], (int)a[3],
                                           (int)a[4], (int)a[5], (int)a[6], (int)a[7],
                                           px, py);
    } else if (!strcmp(fn, "jump_player")) {
        Tplayer *p = (Tplayer *)pf_tr(a[0]);
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
        Tparticle *p = (Tparticle *)pf_tr(a[0]);
        seed = *(double *)(pf_guest + (G_SEED - PF_GUEST_BASE));
        update_particle(p);
        *(double *)(pf_guest + (G_SEED - PF_GUEST_BASE)) = seed;
        eax = 0;
    } else if (!strcmp(fn, "create_particle")) {
        Tparticle *p = (Tparticle *)pf_tr(a[0]);
        seed = *(double *)(pf_guest + (G_SEED - PF_GUEST_BASE));
        eax = (unsigned int)create_particle(p, (int)a[1], (int)a[2]);
        *(double *)(pf_guest + (G_SEED - PF_GUEST_BASE)) = seed;
    } else if (!strcmp(fn, "ok_to_play")) {
        eax = (unsigned int)ok_to_play();
    } else if (!strcmp(fn, "add_floor")) {
        Tmap *m = (Tmap *)pf_tr(a[0]);
        demo = (Treplay *)pf_tr(*(unsigned int *)(pf_guest + (G_DEMO - PF_GUEST_BASE)));
        harness_rand_state = *(unsigned int *)(pf_guest + (RAND_SEED_VA - PF_GUEST_BASE));
        add_floor(m);
        eax = 0;
    } else if (!strcmp(fn, "reset_player")) {
        Tplayer *p = (Tplayer *)pf_tr(a[0]);
        reset_player(p);
        eax = 0;
    } else if (!strcmp(fn, "update_player")) {
        Tplayer *p = (Tplayer *)pf_tr(a[0]);
        /* same globals jump_player.c/add_floor's blocks above sync by
         * hand: collision_type/max_speed (read-only), gravity_modifier
         * (read-only), demo (read-only pointer VALUE, translated). */
        collision_type = *(int *)(pf_guest + (G_COLLISION_TYPE - PF_GUEST_BASE));
        memcpy(max_speed, pf_guest + (G_MAX_SPEED - PF_GUEST_BASE), sizeof(max_speed));
        memcpy(gravity_modifier, pf_guest + (G_GRAVITY_MOD - PF_GUEST_BASE), sizeof(gravity_modifier));
        demo = (Treplay *)pf_tr(*(unsigned int *)(pf_guest + (G_DEMO - PF_GUEST_BASE)));
        update_player(p);
        eax = 0;
    } else if (!strcmp(fn, "play_jump_sound")) {
        Tplayer *p = (Tplayer *)pf_tr(a[0]);
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
        /* `data` stays a RAW guest VA here (NOT pf_tr()-translated): unlike
         * ply[player_id]/demo above, this dispatch never dereferences
         * `data` itself -- call_trace_stubs.c's asset_bitmap() does, and
         * translates it via PF_MEM() there (the same function body used
         * by the MSVC harness, where `data` is a macro yielding a raw
         * guest VA too -- keeping this copy raw here is what keeps that
         * one shared implementation correct in both worlds). */
        data = (DATAFILE *)(size_t)(*(unsigned int *)(pf_guest + (G_DATA - PF_GUEST_BASE)));
        memcpy(stars, pf_guest + (G_STARS - PF_GUEST_BASE), sizeof(stars));
        seed = *(double *)(pf_guest + (G_SEED - PF_GUEST_BASE));
        eax = (unsigned int)start_reward((int)a[0]);
        *(int *)(pf_guest + (G_REWARD_TIME - PF_GUEST_BASE)) = reward_time;
        *(fixed *)(pf_guest + (G_REWARD_SCALE - PF_GUEST_BASE)) = reward_scale;
        *(BITMAP **)(pf_guest + (G_REWARD_BMP - PF_GUEST_BASE)) = reward_bmp;
        memcpy(pf_guest + (G_STARS - PF_GUEST_BASE), stars, sizeof(stars));
        *(double *)(pf_guest + (G_SEED - PF_GUEST_BASE)) = seed;
    } else if (!strcmp(fn, "draw_scroller")) {
        /* No game global read or written -- domain is entirely the three
         * call-trace slots (pf_harness_calltrace.h) plus EAX; sc/bmp are
         * plain scratch-region pointers, translated the same way every
         * other pointer-shaped vector argument is. */
        Tscroller *sc = (Tscroller *)pf_tr(a[0]);
        BITMAP *bmp = (BITMAP *)pf_tr(a[1]);
        eax = (unsigned int)draw_scroller(sc, bmp, (int)a[2], (int)a[3], (int)a[4]);
    } else if (!strcmp(fn, "handle_player_collision_original")) {
        unsigned int pid;
        player_id = *(int *)(pf_guest + (G_PLAYER_ID - PF_GUEST_BASE));
        pid = (unsigned int)player_id;
        /* same ply[player_id] pointer-VALUE second translation
         * update_frame's own dispatch (icytower_harness_project.c) needs
         * -- this function reads ply[player_id] internally, never as a
         * parameter. */
        ply[pid] = (Tplayer *)pf_tr(*(unsigned int *)(pf_guest + (G_PLY + 4u * pid - PF_GUEST_BASE)));
        memcpy(&map, pf_guest + (G_MAP - PF_GUEST_BASE), sizeof(map));
        any11 = *(int *)(pf_guest + (G_ANY11 - PF_GUEST_BASE));
        any12 = *(int *)(pf_guest + (G_ANY12 - PF_GUEST_BASE));
        any21 = *(int *)(pf_guest + (G_ANY21 - PF_GUEST_BASE));
        any22 = *(int *)(pf_guest + (G_ANY22 - PF_GUEST_BASE));
        any23 = *(int *)(pf_guest + (G_ANY23 - PF_GUEST_BASE));
        memcpy(sounds, pf_guest + (G_SOUNDS - PF_GUEST_BASE), sizeof(sounds));
        handle_player_collision_original((int)a[0], (int)a[1]);
        eax = 0;
        /* ply[pid] itself points INTO pf_guest (pf_tr()'s own contract),
         * so any Tplayer field writes already landed there directly --
         * only the plain-int globals need writing back. */
        *(int *)(pf_guest + (G_ANY11 - PF_GUEST_BASE)) = any11;
        *(int *)(pf_guest + (G_ANY12 - PF_GUEST_BASE)) = any12;
        *(int *)(pf_guest + (G_ANY21 - PF_GUEST_BASE)) = any21;
        *(int *)(pf_guest + (G_ANY22 - PF_GUEST_BASE)) = any22;
        *(int *)(pf_guest + (G_ANY23 - PF_GUEST_BASE)) = any23;
    } else if (!strcmp(fn, "handle_player_collision_old")) {
        collision_pre();
        handle_player_collision_old((int)a[0], (int)a[1]);
        collision_post();
        eax = 0;
    } else if (!strcmp(fn, "handle_player_collision_vector")) {
        collision_pre();
        handle_player_collision_vector((int)a[0], (int)a[1]);
        collision_post();
        eax = 0;
    } else if (!strcmp(fn, "handle_player_collision_vector_2")) {
        collision_pre();
        handle_player_collision_vector_2((int)a[0], (int)a[1]);
        collision_post();
        eax = 0;
    } else if (!strcmp(fn, "handle_player_collision_combo")) {
        collision_pre();
        handle_player_collision_combo((int)a[0], (int)a[1]);
        collision_post();
        eax = 0;
    } else if (!strcmp(fn, "poll_control")) {
        /* Reads Allegro's key[]/joy[0] and the game's `gamepad` remap
         * table; writes only c->flags. joy[] is a plain array (no pointer
         * VALUE inside the six fields poll_control touches), so a straight
         * memcpy in is enough -- nothing to write back. */
        Tcontrol *c = (Tcontrol *)pf_tr(a[0]);
        memcpy((void *)key, pf_guest + (G_KEY - PF_GUEST_BASE), sizeof(key));
        memcpy(&gamepad, pf_guest + (G_GAMEPAD - PF_GUEST_BASE), sizeof(gamepad));
        memcpy(&joy[0], pf_guest + (G_JOY - PF_GUEST_BASE), sizeof(joy[0]));
        poll_control(c, (int)a[1]);
        eax = 0;
    } else if (!strcmp(fn, "handle_player_input")) {
        /* The widest sync in this file, because this ONE function reaches
         * six already-promoted callees and every global any of them reads:
         *   itself           recording, rec_pos, demo(+size,+data), rejump,
         *                    profile->total_jumps, ply[player_id]
         *   poll_control     key[], joy[0], gamepad
         *   jump_player      collision_type, max_speed[5]
         *   play_jump_sound  custom.jump_sound[0..2]
         *
         * TWO pointer VALUES need the "second translation, driver-side"
         * fixup (src_check.c's own header comment): `demo` itself, and --
         * one level deeper than anything before this batch -- `demo->data`,
         * the Trecord array base stored INSIDE the guest Treplay. Without
         * the second one the compiled side would dereference a raw guest VA
         * the moment the RLE encoder/decoder indexes a record. Neither
         * pointer is in the comparison domain, so rewriting them in place
         * is invisible to the diff. */
        Tcontrol *c = (Tcontrol *)pf_tr(a[0]);       /* may legitimately be NULL */
        unsigned int pid;

        recording = *(int *)(pf_guest + (G_RECORDING - PF_GUEST_BASE));
        rec_pos = *(int *)(pf_guest + (G_REC_POS - PF_GUEST_BASE));
        rejump = *(int *)(pf_guest + (G_REJUMP - PF_GUEST_BASE));
        player_id = *(int *)(pf_guest + (G_PLAYER_ID - PF_GUEST_BASE));
        pid = (unsigned int)player_id;
        ply[pid] = (Tplayer *)pf_tr(*(unsigned int *)(pf_guest + (G_PLY + 4u * pid - PF_GUEST_BASE)));
        demo = (Treplay *)pf_tr(*(unsigned int *)(pf_guest + (G_DEMO - PF_GUEST_BASE)));
        if (demo != 0)
            demo->data = (Trecord *)pf_tr(*(unsigned int *)((unsigned char *)demo + REPLAY_DATA_OFF));
        profile = (Tprofile *)pf_tr(*(unsigned int *)(pf_guest + (G_PROFILE - PF_GUEST_BASE)));
        memcpy((void *)key, pf_guest + (G_KEY - PF_GUEST_BASE), sizeof(key));
        memcpy(&gamepad, pf_guest + (G_GAMEPAD - PF_GUEST_BASE), sizeof(gamepad));
        memcpy(&joy[0], pf_guest + (G_JOY - PF_GUEST_BASE), sizeof(joy[0]));
        collision_type = *(int *)(pf_guest + (G_COLLISION_TYPE - PF_GUEST_BASE));
        memcpy(max_speed, pf_guest + (G_MAX_SPEED - PF_GUEST_BASE), sizeof(max_speed));
        memcpy(custom.jump_sound, pf_guest + (G_CUSTOM_JUMP_SOUND - PF_GUEST_BASE),
               sizeof(custom.jump_sound));

        handle_player_input(c);
        eax = 0;

        /* Tcontrol / Tplayer / the Trecord array / profile->total_jumps all
         * live INSIDE pf_guest already (pf_tr()'s contract), so their writes
         * landed there directly; only the plain-int cursor needs copying
         * back. */
        *(int *)(pf_guest + (G_REC_POS - PF_GUEST_BASE)) = rec_pos;
    } else {
        fprintf(stderr, "gcc dispatch only wires up line_intersect/jump_player/"
                        "new_rand/update_particle/create_particle/ok_to_play/"
                        "add_floor/reset_player/update_player/play_jump_sound/"
                        "start_reward/handle_player_collision_original/"
                        "draw_scroller/handle_player_collision_{old,vector,"
                        "vector_2,combo}; got '%s'\n", fn);
        exit(2);
    }
    return eax;
}
