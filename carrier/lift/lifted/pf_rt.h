/* GENERATED FILE -- DO NOT EDIT.  Produced by pf_lift.py.
 *
 * Runtime support for the LIFTED form (win32_pilot.md SS3).  Deliberately
 * tiny: no CPU struct, no memory abstraction.  PF_MEM() is the ONE seam --
 * it is the identity in the carrier (the original image is mapped at its
 * original base) and can be redefined by an offline harness to point at an
 * in-process copy of the image.
 *
 * The x87 model has TWO interchangeable backends behind one macro API
 * (PF_ADD/PF_MUL/PF_TOI32/...), selected at COMPILE TIME:
 *
 *   default        pf_x87_t = double          (the original HYPOTHESIS)
 *   -DPF_X87_SOFT  pf_x87_t = software 80-bit extended (pf_x87_soft.h)
 *
 * No generated .c file changes between the two.
 */
#ifndef PF_RT_H
#define PF_RT_H

#include <string.h>

/* ---- the one memory seam ------------------------------------------------ */
#ifndef PF_MEM
#define PF_MEM(a) ((void *)(size_t)(unsigned int)(a))
#endif

#define PF_R8(a)    (*(unsigned char  *)PF_MEM(a))
#define PF_R16(a)   (*(unsigned short *)PF_MEM(a))
#define PF_R32(a)   (*(unsigned int   *)PF_MEM(a))
#define PF_R64(a)   (*(unsigned long long *)PF_MEM(a))
#define PF_RF32(a)  (*(float  *)PF_MEM(a))
#define PF_RF64(a)  (*(double *)PF_MEM(a))
#define PF_W8(a,v)   (PF_R8(a)   = (unsigned char )(v))
#define PF_W16(a,v)  (PF_R16(a)  = (unsigned short)(v))
#define PF_W32(a,v)  (PF_R32(a)  = (unsigned int  )(v))
#define PF_W64(a,v)  (PF_R64(a)  = (unsigned long long)(v))
#define PF_WF32(a,v) (PF_RF32(a) = (float )(v))
#define PF_WF64(a,v) (PF_RF64(a) = (double)(v))

/* ---- partial-register access -------------------------------------------- */
#define PF_GET8L(r)   ((r) & 0xFFu)
#define PF_GET8H(r)   (((r) >> 8) & 0xFFu)
#define PF_GET16(r)   ((r) & 0xFFFFu)
#define PF_SET8L(r,v) ((r) = ((r) & 0xFFFFFF00u) | ((unsigned int)(v) & 0xFFu))
#define PF_SET8H(r,v) ((r) = ((r) & 0xFFFF00FFu) | (((unsigned int)(v) & 0xFFu) << 8))
#define PF_SET16(r,v) ((r) = ((r) & 0xFFFF0000u) | ((unsigned int)(v) & 0xFFFFu))

/* ---- compile-time assertion (used on every argument slot) --------------- */
#define PF_CT_ASSERT(e) do { typedef char pf_ct_[(e) ? 1 : -1]; \
                             (void)sizeof(pf_ct_); } while (0)

/* ---- hard refusal at run time ------------------------------------------- */
#ifndef PF_TRAP
extern void pf_trap(unsigned int va, const char *why);
#define PF_TRAP(va, why) pf_trap((va), (why))
#endif

/* ---- x87 control word ---------------------------------------------------
 * KNOWN: Icy Tower's ___mingw_CRTStartup (0x401020) calls __fpreset
 * (0x4b2850), which is a bare FNINIT.  FNINIT leaves CW = 0x037F, i.e.
 * PC = 11 (64-bit significand = full extended precision) and RC = 00
 * (round to nearest even).  That is the control word a lifted function
 * inherits, so PF_CW_INIT is 0x037F and NOT the MSVC/CRT 0x027F.
 *
 * GCC 4.4 casts a floating value to int with the classic idiom
 *     fnstcw save ; ax = save ; ah = 0x0C ; fldcw trunc ; fistp ; fldcw save
 * (0x0C in the high byte = RC 11 "toward zero", PC 00).  pf_lift models the
 * control word explicitly, so FISTP really truncates there.
 */
#define PF_CW_INIT  0x037Fu

/* Rounding-mode field, 2 bits: 00 nearest-even, 01 -inf, 10 +inf, 11 zero. */
#define PF_CW_RC(cw) (((cw) >> 10) & 3u)

/* pf_lift emits this before every rounding x87 operation in a function that
 * contains an FLDCW.  Neither backend models a non-default PC/RC for
 * ARITHMETIC (only FIST/FISTP consult RC), so reaching one is a hard trap
 * rather than a silent approximation. */
#define PF_CW_ARITH(va, cw) do { if (((cw) & 0x0F00u) != 0x0300u) \
        PF_TRAP((va), "x87 arithmetic under a non-default control word"); \
    } while (0)

/* ---- x87 backend -------------------------------------------------------- */
#if defined(PF_X87_SOFT)
#include "pf_x87_soft.h"
#else

/* HYPOTHESIS backend (win32_pilot.md SS3): the 80-bit x87 register stack is
 * modelled with `double`.  Every generated file goes through the macros
 * below, so -DPF_X87_SOFT swaps the whole model without regenerating. */
typedef double pf_x87_t;

#define PF_FI16(x)  ((pf_x87_t)(short)(x))
#define PF_FI32(x)  ((pf_x87_t)(int)(x))
#define PF_FI64(x)  ((pf_x87_t)(long long)(x))
#define PF_FF32(x)  ((pf_x87_t)(float)(x))
#define PF_FF64(x)  ((pf_x87_t)(double)(x))
#define PF_ZERO     ((pf_x87_t)0.0)
#define PF_ONE      ((pf_x87_t)1.0)
#define PF_ADD(a,b) ((pf_x87_t)((a) + (b)))
#define PF_SUB(a,b) ((pf_x87_t)((a) - (b)))
#define PF_MUL(a,b) ((pf_x87_t)((a) * (b)))
#define PF_DIV(a,b) ((pf_x87_t)((a) / (b)))
#define PF_NEG(a)   ((pf_x87_t)(-(a)))
#define PF_TOF32(v) ((float )(v))
#define PF_TOF64(v) ((double)(v))

/* Raw bit patterns for FST/FSTP.  These return an INTEGER, never a double:
 * the 32-bit cdecl ABI returns a double in ST(0), and FLD of a signalling NaN
 * quiets it, so a `double`-returning conversion helper would mangle an SNaN
 * that the original code merely copies from memory to memory. */
static unsigned long long pf_bits64(pf_x87_t v)
{ union { double d; unsigned long long u; } c; c.d = (double)v; return c.u; }
static unsigned int pf_bits32(pf_x87_t v)
{ union { float f; unsigned int u; } c; c.f = (float)v; return c.u; }

static pf_x87_t pf_abs(pf_x87_t v) { return v < (pf_x87_t)0 ? -v : v; }
#define PF_ABS(v) pf_abs(v)

/* FPU status-word condition bits, exactly as FCOM/FUCOM set them:
 *   ST0 > src -> 0, < -> C0 (0x0100), = -> C3 (0x4000), unordered -> 0x4500. */
static unsigned int pf_fcmp(pf_x87_t a, pf_x87_t b)
{
    if (a > b)  return 0x0000u;
    if (a < b)  return 0x0100u;
    if (a == b) return 0x4000u;
    return 0x4500u;
}

/* Round to an integral value in the CURRENT rounding mode. */
static double pf_round_rc(double v, unsigned int cw)
{
    double f, d;
    unsigned int rc = PF_CW_RC(cw);
    if (v != v) return v;                                  /* NaN */
    if (v >= 9.2233720368547758e18 || v <= -9.2233720368547758e18)
        return v;                                          /* already integral / inf */
    f = (double)(long long)v;                              /* toward zero */
    if (f == v) return v;
    if (rc == 0u) {
        d = v - f;
        if (d >  0.5) return f + 1.0;
        if (d < -0.5) return f - 1.0;
        if (d ==  0.5) return (((long long)f) & 1) ? f + 1.0 : f;
        if (d == -0.5) return (((long long)f) & 1) ? f - 1.0 : f;
        return f;
    }
    if (rc == 1u) return (v < 0.0) ? f - 1.0 : f;           /* toward -inf */
    if (rc == 2u) return (v > 0.0) ? f + 1.0 : f;           /* toward +inf */
    return f;                                               /* toward zero */
}

/* FIST/FISTP.  An out-of-range or NaN source yields the x87 "integer
 * indefinite" 0x80000000 / 0x8000 with the invalid exception masked, which is
 * what the hardware (and the unicorn oracle) stores. */
static unsigned int pf_toi32(pf_x87_t v, unsigned int cw)
{
    double r = pf_round_rc((double)v, cw);
    if (!(r >= -2147483648.0 && r <= 2147483647.0)) return 0x80000000u;
    return (unsigned int)(int)r;
}
static unsigned int pf_toi16(pf_x87_t v, unsigned int cw)
{
    double r = pf_round_rc((double)v, cw);
    if (!(r >= -32768.0 && r <= 32767.0)) return 0x8000u;
    return (unsigned int)(int)r & 0xFFFFu;
}
#define PF_TOI32(v, cw) pf_toi32((v), (cw))
#define PF_TOI16(v, cw) pf_toi16((v), (cw))

#endif /* backend */

/* FCOMI-family: the same comparison delivered in EFLAGS (ZF=bit6, PF=bit2,
 * CF=bit0) instead of the status word. */
static unsigned int pf_fcmp2eflags(unsigned int sw)
{
    switch (sw & 0x4500u) {
    case 0x0000u: return 0x00u;                 /* >  : ZF=0 PF=0 CF=0 */
    case 0x0100u: return 0x01u;                 /* <  : CF=1 */
    case 0x4000u: return 0x40u;                 /* =  : ZF=1 */
    default:      return 0x45u;                 /* unordered: ZF PF CF */
    }
}

/* ---- lifted-function-local macros ---------------------------------------
 * These expand INSIDE a lifted function and name its locals (pf_fr, pf_ftop,
 * pf_fsw, pf_fcw, pf_stk, pf_arg) directly.  That is what keeps a lifted
 * function re-entrant without a CPU struct: the "machine state" is ordinary
 * C locals.
 */
#define PF_ST(i)   pf_fr[(pf_ftop + (unsigned int)(i)) & 7u]
/* PF_PUSH must evaluate its argument BEFORE moving the top index: `fld st(0)`
 * reads ST(0) relative to the OLD top. */
#define PF_PUSH(v) do { pf_x87_t pf_pv_ = (v); pf_ftop = (pf_ftop - 1u) & 7u; \
                        pf_fr[pf_ftop] = pf_pv_; } while (0)
#define PF_POP()   do { pf_ftop = (pf_ftop + 1u) & 7u; } while (0)
#define PF_FSW()   ((pf_fsw & 0x4500u) | ((pf_ftop & 7u) << 11))

/* FST/FSTP m32fp / m64fp: store the raw bit pattern (see pf_bits64 above). */
#define PF_WB32(a,v)  (PF_R32(a) = pf_bits32(v))
#define PF_WB64(a,v)  (PF_R64(a) = pf_bits64(v))
#define PF_SWB32(k,v) (PF_S32(k) = pf_bits32(v))
#define PF_SWB64(k,v) (PF_S64(k) = pf_bits64(v))

/* incoming arguments: a byte-exact copy of the cdecl argument block */
#define PF_A8(k)   (*(unsigned char      *)(pf_arg.b + (k)))
#define PF_A16(k)  (*(unsigned short     *)(pf_arg.b + (k)))
#define PF_A32(k)  (*(unsigned int       *)(pf_arg.b + (k)))
#define PF_A64(k)  (*(unsigned long long *)(pf_arg.b + (k)))
#define PF_AF32(k) (*(float  *)(pf_arg.b + (k)))
#define PF_AF64(k) (*(double *)(pf_arg.b + (k)))

/* locals / saved registers / outgoing pushes: the scratch frame */
#define PF_S8(k)   (*(unsigned char      *)(pf_stk.b + (k)))
#define PF_S16(k)  (*(unsigned short     *)(pf_stk.b + (k)))
#define PF_S32(k)  (*(unsigned int       *)(pf_stk.b + (k)))
#define PF_S64(k)  (*(unsigned long long *)(pf_stk.b + (k)))
#define PF_SF32(k) (*(float  *)(pf_stk.b + (k)))
#define PF_SF64(k) (*(double *)(pf_stk.b + (k)))
#define PF_SW8(k,v)   (PF_S8(k)   = (unsigned char )(v))
#define PF_SW16(k,v)  (PF_S16(k)  = (unsigned short)(v))
#define PF_SW32(k,v)  (PF_S32(k)  = (unsigned int  )(v))
#define PF_SW64(k,v)  (PF_S64(k)  = (unsigned long long)(v))
#define PF_SWF32(k,v) (PF_SF32(k) = (float )(v))
#define PF_SWF64(k,v) (PF_SF64(k) = (double)(v))

#endif /* PF_RT_H */
