/* batch15_check.c -- the COMPILED-CANDIDATE half of PROMOTIONS.md
 * batch 15's PURE oracle:
 *
 *   key_to_str   (src/icytower/menu_keys.c)
 *
 * One function, because exactly one function in this batch is pure in
 * the project's strongest sense: a `call` census over
 * artifacts/disasm.txt 0x416a9c..0x41748a finds ZERO call sites, so
 * there is nothing to hook, nothing to script and nothing to stub.  The
 * other five batch-15 functions all reach the CRT, the filesystem, the
 * datafile or the control layer and live in batch15b_check.c instead.
 *
 * Harness-only; nothing under src/ is changed or seam-ed for it
 * (win32_pilot.md SS7a).
 *
 * Protocol: argv[1] is a text vector file, one decimal scancode per
 * line.  For each, the result is printed between "V <n>" and "E" as the
 * destination buffer's CONTENT and its strlen -- plus a CANARY check,
 * which is the point of the extra work below: `dest` is a 64-byte buffer
 * inside a 192-byte block whose other 128 bytes are filled with 0xA5,
 * and the whole block is dumped.  key_to_str() bounds-checks nothing, so
 * "it wrote the right string" and "it wrote ONLY the right string" are
 * different claims and this oracle makes both.
 *
 * Build: see build_batch15.sh.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "allegro_api.h"
#include "game_types.h"
#include "game_state.h"
#include "game_funcs.h"

/* Storage the standalone world needs but state.c does not supply. */
FONT *font;
volatile char key[127];
int *allegro_errno;
BITMAP *screen;
static int errno_storage;

#define GUARD 64
#define BLOCK (GUARD + 64 + GUARD)

static unsigned char block[BLOCK];

int main(int argc, char **argv)
{
    FILE *vf;
    char line[64];
    int n = 0;
    int i;

    allegro_errno = &errno_storage;
    if (argc < 2) { fprintf(stderr, "usage: batch15_check <vectors.txt>\n"); return 2; }
    vf = fopen(argv[1], "r");
    if (!vf) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }

    while (fgets(line, sizeof line, vf)) {
        int k;
        if (!line[0] || line[0] == '\n' || line[0] == '\r') continue;
        k = (int)strtol(line, (char **)0, 0);
        memset(block, 0xA5, sizeof block);
        printf("V %d\n", n++);
        key_to_str(k, (char *)block + GUARD);
        printf("str \"%s\"\n", (char *)block + GUARD);
        printf("len %d\n", (int)strlen((char *)block + GUARD));
        printf("block ");
        for (i = 0; i < BLOCK; i++)
            printf("%02x", block[i]);
        printf("\n");
        printf("E\n");
    }
    fclose(vf);
    return 0;
}
