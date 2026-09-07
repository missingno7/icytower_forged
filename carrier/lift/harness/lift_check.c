/* lift_check.c -- LIFTED-side driver of the offline equivalence check.
 *
 * Reads a flat image of the guest address space (0x400000..0x800000) and a
 * vector file, runs one lifted function per vector against a freshly restored
 * image, and writes back the return value plus the raw bytes of the function's
 * comparison domain (notes/promotion_candidates.md SS4).
 *
 * The ORIGINAL side of the same comparison runs the original bytes in unicorn;
 * see lift_check.py, which produces the inputs and does the diff.
 *
 * Hand-written harness (not generated).  No windows.h on purpose: it_types.h
 * carries its own DirectX/GDI struct definitions and would collide.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "it_types.h"

unsigned char *pf_guest = 0;
static unsigned char *pf_pristine = 0;

void pf_trap(unsigned int va, const char *why)
{
    fprintf(stderr, "PF_TRAP at %08X: %s\n", va, why);
    exit(3);
}

extern void __cdecl lifted_update_frame(void);
extern int  __cdecl lifted_is_solid(Tmap *, int, int);
extern int  __cdecl lifted_jump_player(Tplayer *, int);
extern int  __cdecl lifted_line_intersect(int, int, int, int, int, int,
                                          int, int, int *, int *);

static unsigned int rd32(FILE *f)
{
    unsigned int v = 0;
    if (fread(&v, 4, 1, f) != 1) { fprintf(stderr, "short read\n"); exit(2); }
    return v;
}

int main(int argc, char **argv)
{
    FILE *fi, *fo;
    unsigned char *raw;
    unsigned int ndom, nvec, i, j, domlen = 0;
    unsigned int *domva, *domlen_i;
    const char *fn;

    if (argc != 5) {
        fprintf(stderr, "usage: lift_check <guest.bin> <vectors.bin> <out.bin> <func>\n");
        return 2;
    }
    fn = argv[4];

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
        unsigned int nargs, a[16], nwr, eax = 0;
        memcpy(pf_guest, pf_pristine, PF_GUEST_SIZE);
        nargs = rd32(fi);
        if (nargs > 16) { fprintf(stderr, "too many args\n"); return 2; }
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

        if (!strcmp(fn, "update_frame")) {
            lifted_update_frame();
            eax = 0;
        } else if (!strcmp(fn, "is_solid")) {
            eax = (unsigned int)lifted_is_solid((Tmap *)(size_t)a[0], (int)a[1], (int)a[2]);
        } else if (!strcmp(fn, "jump_player")) {
            eax = (unsigned int)lifted_jump_player((Tplayer *)(size_t)a[0], (int)a[1]);
        } else if (!strcmp(fn, "line_intersect")) {
            eax = (unsigned int)lifted_line_intersect(
                      (int)a[0], (int)a[1], (int)a[2], (int)a[3], (int)a[4],
                      (int)a[5], (int)a[6], (int)a[7],
                      (int *)(size_t)a[8], (int *)(size_t)a[9]);
        } else {
            fprintf(stderr, "unknown function '%s'\n", fn);
            return 2;
        }

        fwrite(&eax, 4, 1, fo);
        for (j = 0; j < ndom; j++)
            fwrite(pf_guest + (domva[j] - PF_GUEST_BASE), 1, domlen_i[j], fo);
    }
    fclose(fi);
    fclose(fo);
    printf("lifted side: %u vectors, %u domain bytes each\n", nvec, domlen);
    return 0;
}
