/* GENERATED FILE -- DO NOT EDIT.  Produced by pf_lift.py.
 *
 * Runtime support for the LIFTED form (win32_pilot.md SS3).  Deliberately
 * tiny: no CPU struct, no memory abstraction.  PF_MEM() is the ONE seam --
 * it is the identity in the carrier (the original image is mapped at its
 * original base) and can be redefined by an offline harness to point at an
 * in-process copy of the image.
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

/* ---- x87 ----------------------------------------------------------------
 * HYPOTHESIS under test (win32_pilot.md SS3): `double` is enough for the
 * 80-bit intermediates GCC 4.4 keeps in the x87 register stack.  Every lifted
 * FPU value has this ONE type so it can be swapped for a software 80-bit type
 * without touching any generated file.
 */
typedef double pf_x87_t;

/* FPU status-word condition bits, exactly as FUCOM/FCOM set them:
 *   ST0 > src  -> 0
 *   ST0 < src  -> C0                      (0x0100)
 *   ST0 = src  -> C3                      (0x4000)
 *   unordered  -> C3|C2|C0                (0x4500)
 * `test ah, 0x45` is therefore ZF <=> ST0 > src, and `test ah, 5` is
 * ZF <=> ST0 >= src and ordered.  That is the GCC 4.4 float-compare idiom.
 */
static unsigned int pf_fcmp(pf_x87_t a, pf_x87_t b)
{
    if (a > b)  return 0x0000u;
    if (a < b)  return 0x0100u;
    if (a == b) return 0x4000u;
    return 0x4500u;                 /* unordered */
}

/* FCOMI-family: the same comparison, delivered in EFLAGS (ZF=bit6, PF=bit2,
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

static pf_x87_t pf_fabs(pf_x87_t v) { return v < (pf_x87_t)0 ? -v : v; }

/* FIST/FISTP with the default (round-to-nearest-even) control word.  A lifted
 * function that changes the control word is refused by pf_lift, so this is the
 * only rounding mode that can be reached here. */
static pf_x87_t pf_rint(pf_x87_t v)
{
    pf_x87_t f = (pf_x87_t)(long long)v;        /* truncate */
    pf_x87_t d = v - f;
    if (d > (pf_x87_t)0.5)  return f + (pf_x87_t)1;
    if (d < (pf_x87_t)-0.5) return f - (pf_x87_t)1;
    if (d == (pf_x87_t)0.5)  return ((long long)f & 1) ? f + (pf_x87_t)1 : f;
    if (d == (pf_x87_t)-0.5) return ((long long)f & 1) ? f - (pf_x87_t)1 : f;
    return f;
}

/* ---- lifted-function-local macros ---------------------------------------
 * These expand INSIDE a lifted function and name its locals (pf_fr, pf_ftop,
 * pf_fsw, pf_stk, pf_arg) directly.  That is what keeps a lifted function
 * re-entrant without a CPU struct: the "machine state" is ordinary C locals.
 */
#define PF_ST(i)   pf_fr[(pf_ftop + (unsigned int)(i)) & 7u]
/* PF_PUSH must evaluate its argument BEFORE moving the top index: `fld st(0)`
 * reads ST(0) relative to the OLD top. */
#define PF_PUSH(v) do { pf_x87_t pf_pv_ = (v); pf_ftop = (pf_ftop - 1u) & 7u; \
                        pf_fr[pf_ftop] = pf_pv_; } while (0)
#define PF_POP()   do { pf_ftop = (pf_ftop + 1u) & 7u; } while (0)
#define PF_FSW()   ((pf_fsw & 0x4500u) | ((pf_ftop & 7u) << 11))

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
#define PF_SWF32(k,v) (PF_SF32(k) = (float )(v))
#define PF_SWF64(k,v) (PF_SF64(k) = (double)(v))

#endif /* PF_RT_H */
