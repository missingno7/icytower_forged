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

#define PF_GUEST_BASE 0x400000u
#define PF_GUEST_SIZE 0x400000u          /* 0x400000 .. 0x800000 */
#define PF_MEM(a) ((void *)(pf_guest + ((unsigned int)(a) - PF_GUEST_BASE)))

#define G_PLAYER_ID     0x4fe518u
#define G_PLY           0x4ff128u
#define G_COLLISION_TYPE 0x4dd140u
#define G_MAX_SPEED      0x4bdb80u

/* storage for game_state.h's extern decls -- the STANDALONE world's
 * contract (win32_pilot.md SS7a: "a state.c defines the globals"); this
 * driver plays that role for the two globals jump_player.c reads. */
int collision_type;
double max_speed[5];

unsigned char *pf_guest = 0;
static unsigned char *pf_pristine = 0;

void pf_trap(unsigned int va, const char *why)
{
    fprintf(stderr, "PF_TRAP at %08X: %s\n", va, why);
    exit(3);
}

extern int jump_player(Tplayer *, int);
extern int line_intersect(int, int, int, int, int, int, int, int, int *, int *);

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
    if (strcmp(fn, "line_intersect") != 0 && strcmp(fn, "jump_player") != 0) {
        fprintf(stderr, "gcc_check only wires up line_intersect/jump_player "
                        "(win32_pilot.md SS6a experiment); got '%s'\n", fn);
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
        } else { /* jump_player */
            Tplayer *p = (Tplayer *)tr(a[0]);
            /* sync the two globals jump_player.c reads (see header comment)
             * from the guest image before the call; both are read-only from
             * jump_player's side, so no write-back afterwards. */
            collision_type = *(int *)(pf_guest + (G_COLLISION_TYPE - PF_GUEST_BASE));
            memcpy(max_speed, pf_guest + (G_MAX_SPEED - PF_GUEST_BASE), sizeof(max_speed));
            eax = (unsigned int)jump_player(p, (int)a[1]);
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
