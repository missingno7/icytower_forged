/* x87_soft_selftest.c -- differential dump for lifted/pf_x87_soft.h.
 *
 * Prints one line per random operation:  op a_bits b_bits result_se result_m
 * x87_soft_selftest.py re-computes each line with exact rational arithmetic
 * (Python Fractions, rounded to nearest-even at 64 significand bits) and
 * requires a bit-for-bit match.  This checks the software 80-bit backend
 * against the DEFINITION of extended precision, independently of the unicorn
 * oracle and of any lifted function.
 *
 * Hand-written (not generated).  Built by harness\build_soft.cmd.
 */
#include <stdio.h>
#include <stdlib.h>

void pf_trap(unsigned int va, const char *why)
{ fprintf(stderr, "PF_TRAP %08X: %s\n", va, why); exit(3); }
#define PF_TRAP(va, why) pf_trap((va), (why))
#define PF_CW_RC(cw) (((cw) >> 10) & 3u)
#define PF_CW_INIT 0x037Fu
#include "pf_x87_soft.h"

static unsigned long long st = 88172645463325252ull;
static unsigned long long rnd(void)
{ st ^= st << 13; st ^= st >> 7; st ^= st << 17; return st; }

int main(int argc, char **argv)
{
    int i, n = (argc > 1) ? atoi(argv[1]) : 40000;
    for (i = 0; i < n; i++) {
        union { double d; unsigned long long u; } a, b;
        pf_x87_t x, y, r;
        int op = i & 3, mode = (i >> 2) & 3;
        if (mode == 0) {                       /* two int32-valued doubles */
            a.d = (double)(int)(rnd() >> 32);
            b.d = (double)(int)(rnd() >> 32);
        } else if (mode == 1) {                /* a huge and a small one */
            a.d = (double)(long long)rnd();
            b.d = (double)(int)(rnd() >> 40);
        } else {                               /* arbitrary finite doubles */
            a.u = rnd(); b.u = rnd();
            if (((a.u >> 52) & 0x7FF) == 0x7FF || ((a.u >> 52) & 0x7FF) == 0) continue;
            if (((b.u >> 52) & 0x7FF) == 0x7FF || ((b.u >> 52) & 0x7FF) == 0) continue;
        }
        x = pf_from_f64(a.d);
        y = pf_from_f64(b.d);
        if (op == 0) r = pf_add(x, y);
        else if (op == 1) r = pf_sub(x, y);
        else if (op == 2) r = pf_mul(x, y);
        else { if (pf_is_zero(y)) continue; r = pf_div(x, y); }
        printf("%d %016llx %016llx %04x %016llx\n", op, a.u, b.u, r.se, r.m);
    }
    return 0;
}
