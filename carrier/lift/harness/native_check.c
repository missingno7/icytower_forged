/* native_check.c -- NATIVE-side driver of the offline equivalence check.
 *
 * Line-for-line the same protocol as lift_check.c (guest image in, vector
 * file in, result file out), just calling the hand-written native_<f>
 * symbols from carrier/native instead of the generated lifted_<f> ones, so
 * lift_check.py's existing vector generation, unicorn oracle and diff logic
 * (carrier/lift/README.md SS7) can be reused unchanged -- only --exe and
 * --form point at this binary instead. See win32_pilot.md SS7 for the
 * verification model this offline check stands in for.
 *
 * Hand-written harness (not generated). No windows.h on purpose: it_types.h
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

extern void __cdecl native_update_frame(void);
extern int  __cdecl native_is_solid(Tmap *, int, int);

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
        fprintf(stderr, "usage: native_check <guest.bin> <vectors.bin> <out.bin> <func>\n");
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
        unsigned int nargs, a[8], nwr, eax = 0;
        memcpy(pf_guest, pf_pristine, PF_GUEST_SIZE);
        nargs = rd32(fi);
        if (nargs > 8) { fprintf(stderr, "too many args\n"); return 2; }
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
            native_update_frame();
            eax = 0;
        } else if (!strcmp(fn, "is_solid")) {
            eax = (unsigned int)native_is_solid((Tmap *)(size_t)a[0], (int)a[1], (int)a[2]);
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
    printf("native side: %u vectors, %u domain bytes each\n", nvec, domlen);
    return 0;
}
