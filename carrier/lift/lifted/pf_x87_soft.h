/* GENERATED FILE -- DO NOT EDIT.  Produced by pf_lift.py.
 *
 * Software 80-bit x87 extended type -- the FALLBACK named in win32_pilot.md
 * SS3 ("softfloat x87 is the fallback").  Selected with -DPF_X87_SOFT; not one
 * generated .c file changes between the two backends.
 *
 * Layout: sign, 15-bit biased exponent (bias 16383) and a 64-bit significand
 * with an EXPLICIT integer bit -- the real 80-bit register format.
 * Arithmetic: add / sub / mul / div, all round-to-nearest-even at 64 bits,
 * which is exactly what CW = 0x037F (PC = 11, RC = 00) selects on hardware.
 * FIST/FISTP honour the current RC field of the modelled control word.
 *
 * Deliberately NOT modelled, as hard traps rather than silent approximations:
 * extended denormals and extended-range underflow (unreachable -- an extended
 * denormal needs |x| < 2^-16382 and every value entering these functions comes
 * from a 32-bit int, a float or a double), FSQRT, and the exception flags.
 */
#ifndef PF_X87_SOFT_H
#define PF_X87_SOFT_H

typedef struct pf_x87_s {
    unsigned long long m;      /* significand, bit63 = explicit integer bit */
    unsigned short     se;     /* bit15 = sign, bits 0..14 = biased exponent */
    unsigned short     pad;
} pf_x87_t;

#define PF_XBIAS 16383
#define PF_XTOP  0x8000000000000000ull

static pf_x87_t pf_mk(unsigned int s, int be, unsigned long long m)
{
    pf_x87_t r;
    r.m = m;
    r.se = (unsigned short)((s ? 0x8000u : 0u) | ((unsigned int)be & 0x7FFFu));
    r.pad = 0;
    return r;
}
static unsigned int pf_sgn(pf_x87_t v)  { return (unsigned int)(v.se >> 15) & 1u; }
static int          pf_bex(pf_x87_t v)  { return (int)(v.se & 0x7FFFu); }
static int pf_is_nan (pf_x87_t v) { return pf_bex(v) == 0x7FFF && (v.m << 1) != 0ull; }
static int pf_is_inf (pf_x87_t v) { return pf_bex(v) == 0x7FFF && (v.m << 1) == 0ull; }
static int pf_is_zero(pf_x87_t v) { return pf_bex(v) == 0 && v.m == 0ull; }

static pf_x87_t pf_inf (unsigned int s) { return pf_mk(s, 0x7FFF, PF_XTOP); }
static pf_x87_t pf_zero(unsigned int s) { return pf_mk(s, 0, 0ull); }
/* the x87 "real indefinite": -QNaN, significand 0xC000000000000000 */
static pf_x87_t pf_indef(void) { return pf_mk(1u, 0x7FFF, 0xC000000000000000ull); }

static void pf_unpack(pf_x87_t v, unsigned int *s, int *be, unsigned long long *m)
{
    *s = pf_sgn(v);
    *be = pf_bex(v);
    *m = v.m;
    if (*be == 0 && v.m != 0ull)
        PF_TRAP(0u, "software x87: extended denormal operand is not modelled");
}

/* ---- 128-bit helpers ---------------------------------------------------- */
typedef struct { unsigned long long hi, lo; } pf_u128;

static pf_u128 pf_mul64(unsigned long long a, unsigned long long b)
{
    unsigned long long a0 = a & 0xFFFFFFFFull, a1 = a >> 32;
    unsigned long long b0 = b & 0xFFFFFFFFull, b1 = b >> 32;
    unsigned long long p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
    unsigned long long mid = (p00 >> 32) + (p01 & 0xFFFFFFFFull) + (p10 & 0xFFFFFFFFull);
    pf_u128 r;
    r.lo = (p00 & 0xFFFFFFFFull) | (mid << 32);
    r.hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
    return r;
}

/* right shift with a sticky OR of every bit shifted out */
static pf_u128 pf_shr(pf_u128 v, int n, int *sticky)
{
    pf_u128 r;
    if (n <= 0) return v;
    if (n >= 128) {
        if (v.hi || v.lo) *sticky = 1;
        r.hi = 0ull; r.lo = 0ull;
        return r;
    }
    if (n >= 64) {
        if (v.lo) *sticky = 1;
        r.hi = 0ull; r.lo = v.hi;
        n -= 64;
        if (n) {
            if (r.lo & ((1ull << n) - 1ull)) *sticky = 1;
            r.lo >>= n;
        }
        return r;
    }
    if (v.lo & ((1ull << n) - 1ull)) *sticky = 1;
    r.lo = (v.lo >> n) | (v.hi << (64 - n));
    r.hi = v.hi >> n;
    return r;
}

/* Normalise (hi:lo) so bit127 is set, round to 64 bits (nearest, ties to
 * even) and pack.  Value = (hi:lo) * 2^(be - PF_XBIAS - 127); `sticky` says
 * that nonzero bits exist below bit 0 of the 128-bit window. */
static pf_x87_t pf_round128(unsigned int s, int be, pf_u128 v, int sticky)
{
    unsigned long long m;
    int rbit;
    if (v.hi == 0ull && v.lo == 0ull) return pf_zero(s);
    while ((v.hi & PF_XTOP) == 0ull) {
        v.hi = (v.hi << 1) | (v.lo >> 63);
        v.lo <<= 1;
        be--;
    }
    m = v.hi;
    rbit = (int)((v.lo >> 63) & 1ull);
    if ((v.lo << 1) != 0ull) sticky = 1;
    if (rbit && (sticky || (m & 1ull))) {
        m++;
        if (m == 0ull) { m = PF_XTOP; be++; }
    }
    if (be >= 0x7FFF) return pf_inf(s);
    if (be <= 0) PF_TRAP(0u, "software x87: extended-range underflow is not modelled");
    return pf_mk(s, be, m);
}

/* ---- conversions in ----------------------------------------------------- */
static pf_x87_t pf_from_u64(unsigned int s, unsigned long long u)
{
    int be = PF_XBIAS + 63;
    if (u == 0ull) return pf_zero(0u);
    while ((u & PF_XTOP) == 0ull) { u <<= 1; be--; }
    return pf_mk(s, be, u);
}
static pf_x87_t pf_from_i64(long long v)
{
    unsigned long long u = (v < 0) ? (unsigned long long)(-(v + 1)) + 1ull
                                   : (unsigned long long)v;
    return pf_from_u64((v < 0) ? 1u : 0u, u);
}
static pf_x87_t pf_from_i32(int v)   { return pf_from_i64((long long)v); }
static pf_x87_t pf_from_i16(short v) { return pf_from_i64((long long)v); }

static pf_x87_t pf_from_f64(double d)
{
    union { double d; unsigned long long u; } cv;
    unsigned int s;
    int de;
    unsigned long long mf;
    cv.d = d;
    s  = (unsigned int)(cv.u >> 63) & 1u;
    de = (int)((cv.u >> 52) & 0x7FFull);
    mf = cv.u & 0xFFFFFFFFFFFFFull;
    if (de == 0x7FF) {
        if (mf == 0ull) return pf_inf(s);
        return pf_mk(s, 0x7FFF, PF_XTOP | (mf << 11));       /* NaN payload kept */
    }
    if (de == 0) {
        pf_x87_t t;
        if (mf == 0ull) return pf_zero(s);
        t = pf_from_u64(s, mf);                              /* value == mf */
        return pf_mk(s, pf_bex(t) - 1074, t.m);              /* * 2^-1074 */
    }
    return pf_mk(s, de - 1023 + PF_XBIAS, PF_XTOP | (mf << 11));
}
static pf_x87_t pf_from_f32(float f) { return pf_from_f64((double)f); }

/* ---- conversions out ---------------------------------------------------- */
/* Pack the extended value into an IEEE format with `ew` exponent bits and
 * `mw` significand bits, round-to-nearest-even -- the one routine behind both
 * FST m64fp (11, 52) and FST m32fp (8, 23).  Returns the BIT PATTERN, never a
 * float: the 32-bit cdecl ABI returns floating point in ST(0), and FLD of a
 * signalling NaN quiets it, which would silently mangle an SNaN that the
 * original code only copies from memory to memory. */
static unsigned long long pf_pack(pf_x87_t v, int ew, int mw)
{
    unsigned long long s = (unsigned long long)pf_sgn(v), dm, mmask, mtop;
    int be = pf_bex(v), de, sh, rbit, sticky;
    int bias = (1 << (ew - 1)) - 1, emax = (1 << ew) - 1;
    unsigned long long m = v.m;
    mmask = (1ull << mw) - 1ull;
    mtop = 1ull << mw;
    if (be == 0x7FFF) {
        if ((m << 1) == 0ull)
            return (s << (ew + mw)) | ((unsigned long long)emax << mw);
        dm = (m >> (63 - mw)) & mmask;
        if (dm == 0ull) dm = mtop >> 1;
        return (s << (ew + mw)) | ((unsigned long long)emax << mw) | dm;
    }
    if (be == 0) {
        if (m != 0ull) PF_TRAP(0u, "software x87: extended denormal to IEEE");
        return s << (ew + mw);
    }
    de = be - PF_XBIAS + bias;
    sh = 63 - mw;
    if (de <= 0) {                                  /* IEEE subnormal */
        sh = (63 - mw) + (1 - de);
        de = 0;
        if (sh > 64) return s << (ew + mw);         /* below half the smallest */
    }
    if (sh >= 64) {
        dm = 0ull;
        rbit = (int)((m >> 63) & 1ull);
        sticky = ((m << 1) != 0ull);
    } else {
        dm = m >> sh;
        rbit = (int)((m >> (sh - 1)) & 1ull);
        sticky = ((m & ((1ull << (sh - 1)) - 1ull)) != 0ull);
    }
    if (rbit && (sticky || (dm & 1ull))) dm++;
    if (de == 0) {
        if (dm & mtop) { de = 1; dm &= mmask; }
        return (s << (ew + mw)) | ((unsigned long long)de << mw) | dm;
    }
    if (dm & (mtop << 1)) { dm >>= 1; de++; }
    if (de >= emax) return (s << (ew + mw)) | ((unsigned long long)emax << mw);
    return (s << (ew + mw)) | ((unsigned long long)de << mw) | (dm & mmask);
}
static unsigned long long pf_bits64(pf_x87_t v) { return pf_pack(v, 11, 52); }
static unsigned int pf_bits32(pf_x87_t v) { return (unsigned int)pf_pack(v, 8, 23); }
static double pf_to_f64(pf_x87_t v)
{ union { double d; unsigned long long u; } c; c.u = pf_bits64(v); return c.d; }
static float pf_to_f32(pf_x87_t v)
{ union { float f; unsigned int u; } c; c.u = pf_bits32(v); return c.f; }

/* FIST/FISTP in the current RC.  Out of range or NaN -> integer indefinite,
 * which is what the hardware stores with the invalid exception masked. */
static long long pf_to_i64_rc(pf_x87_t v, unsigned int cw, int *bad)
{
    unsigned int s = pf_sgn(v), rc = PF_CW_RC(cw);
    int be = pf_bex(v), E, sh;
    unsigned long long m = v.m, ip, frac;
    *bad = 0;
    if (be == 0x7FFF) { *bad = 1; return 0; }
    if (be == 0) {
        if (m != 0ull) PF_TRAP(0u, "software x87: extended denormal to integer");
        return 0;
    }
    E = be - PF_XBIAS;
    if (E >= 64) { *bad = 1; return 0; }
    if (E < 0) {                                   /* |v| < 1 */
        ip = 0ull;
        if (E == -1) frac = ((m << 1) == 0ull) ? PF_XTOP : (PF_XTOP + 1ull);
        else         frac = 1ull;                  /* 0 < |v| < 1/2 */
    } else {
        sh = 63 - E;                               /* 0 .. 63 */
        ip = m >> sh;
        frac = (sh == 0) ? 0ull : (m << (64 - sh));
    }
    if (rc == 0u) {
        if (frac > PF_XTOP || (frac == PF_XTOP && (ip & 1ull))) ip++;
    } else if (rc == 1u) {
        if (s && frac) ip++;                       /* toward -inf */
    } else if (rc == 2u) {
        if (!s && frac) ip++;                      /* toward +inf */
    }                                              /* rc == 3: toward zero */
    if (ip >= PF_XTOP) { *bad = 1; return 0; }
    return s ? -(long long)ip : (long long)ip;
}
static unsigned int pf_toi32(pf_x87_t v, unsigned int cw)
{
    int bad;
    long long r = pf_to_i64_rc(v, cw, &bad);
    if (bad || r < -2147483648LL || r > 2147483647LL) return 0x80000000u;
    return (unsigned int)(int)r;
}
static unsigned int pf_toi16(pf_x87_t v, unsigned int cw)
{
    int bad;
    long long r = pf_to_i64_rc(v, cw, &bad);
    if (bad || r < -32768LL || r > 32767LL) return 0x8000u;
    return (unsigned int)(int)r & 0xFFFFu;
}

/* ---- arithmetic --------------------------------------------------------- */
static pf_x87_t pf_neg(pf_x87_t v) { v.se = (unsigned short)(v.se ^ 0x8000u); return v; }
static pf_x87_t pf_abs(pf_x87_t v) { v.se = (unsigned short)(v.se & 0x7FFFu); return v; }

static pf_x87_t pf_add(pf_x87_t a, pf_x87_t b)
{
    unsigned int sa, sb;
    int ea, eb, sticky = 0, d;
    unsigned long long ma, mb;
    pf_u128 A, B, S;
    if (pf_is_nan(a)) return a;
    if (pf_is_nan(b)) return b;
    if (pf_is_inf(a)) {
        if (pf_is_inf(b) && pf_sgn(a) != pf_sgn(b)) return pf_indef();
        return a;
    }
    if (pf_is_inf(b)) return b;
    if (pf_is_zero(a) && pf_is_zero(b))
        return pf_zero((pf_sgn(a) && pf_sgn(b)) ? 1u : 0u);
    if (pf_is_zero(a)) return b;
    if (pf_is_zero(b)) return a;
    pf_unpack(a, &sa, &ea, &ma);
    pf_unpack(b, &sb, &eb, &mb);
    if (eb > ea) {
        unsigned int ts = sa; int te = ea; unsigned long long tm = ma;
        sa = sb; ea = eb; ma = mb;
        sb = ts; eb = te; mb = tm;
    }
    A.hi = ma; A.lo = 0ull;
    B.hi = mb; B.lo = 0ull;
    d = ea - eb;
    B = pf_shr(B, d, &sticky);
    if (sa == sb) {
        unsigned long long lo, t, hi;
        int ovf;
        lo = A.lo + B.lo;
        t = B.hi + ((lo < A.lo) ? 1ull : 0ull);
        ovf = (t < B.hi);
        hi = A.hi + t;
        if (hi < A.hi) ovf = 1;
        S.hi = hi; S.lo = lo;
        if (ovf) {
            if (S.lo & 1ull) sticky = 1;
            S.lo = (S.lo >> 1) | (S.hi << 63);
            S.hi = (S.hi >> 1) | PF_XTOP;
            ea++;
        }
        return pf_round128(sa, ea, S, sticky);
    }
    /* opposite signs.  d >= 1 implies |A| > |B| because bit127 of A is set
     * and B has been shifted right; d == 0 implies no sticky bits. */
    if (A.hi > B.hi || (A.hi == B.hi && A.lo >= B.lo)) {
        S.lo = A.lo - B.lo;
        S.hi = A.hi - B.hi - ((A.lo < B.lo) ? 1ull : 0ull);
        if (sticky) {                 /* true B is a hair larger than B */
            if (S.lo == 0ull) { S.hi--; S.lo = ~0ull; } else { S.lo--; }
        }
        if (S.hi == 0ull && S.lo == 0ull) return pf_zero(0u);
        return pf_round128(sa, ea, S, sticky);
    }
    S.lo = B.lo - A.lo;
    S.hi = B.hi - A.hi - ((B.lo < A.lo) ? 1ull : 0ull);
    if (S.hi == 0ull && S.lo == 0ull) return pf_zero(0u);
    return pf_round128(sb, ea, S, 0);
}
static pf_x87_t pf_sub(pf_x87_t a, pf_x87_t b) { return pf_add(a, pf_neg(b)); }

static pf_x87_t pf_mul(pf_x87_t a, pf_x87_t b)
{
    unsigned int sa, sb, s;
    int ea, eb;
    unsigned long long ma, mb;
    pf_u128 p;
    if (pf_is_nan(a)) return a;
    if (pf_is_nan(b)) return b;
    s = pf_sgn(a) ^ pf_sgn(b);
    if (pf_is_inf(a)) return pf_is_zero(b) ? pf_indef() : pf_inf(s);
    if (pf_is_inf(b)) return pf_is_zero(a) ? pf_indef() : pf_inf(s);
    if (pf_is_zero(a) || pf_is_zero(b)) return pf_zero(s);
    pf_unpack(a, &sa, &ea, &ma);
    pf_unpack(b, &sb, &eb, &mb);
    p = pf_mul64(ma, mb);
    return pf_round128(s, ea + eb - PF_XBIAS + 1, p, 0);
}

static pf_x87_t pf_div(pf_x87_t a, pf_x87_t b)
{
    unsigned int sa, sb, s;
    int ea, eb, i, extra = 0, rbit = 0;
    unsigned long long ma, mb, q = 0ull, rem, carry;
    pf_u128 v;
    if (pf_is_nan(a)) return a;
    if (pf_is_nan(b)) return b;
    s = pf_sgn(a) ^ pf_sgn(b);
    if (pf_is_inf(a)) return pf_is_inf(b) ? pf_indef() : pf_inf(s);
    if (pf_is_inf(b)) return pf_zero(s);
    if (pf_is_zero(b)) return pf_is_zero(a) ? pf_indef() : pf_inf(s);  /* #Z masked */
    if (pf_is_zero(a)) return pf_zero(s);
    pf_unpack(a, &sa, &ea, &ma);
    pf_unpack(b, &sb, &eb, &mb);
    /* restoring division: ma/mb is in [1,2) when `extra`, else in [0.5,1).
     * 64 quotient bits are not enough -- pf_round128 needs a round bit BELOW
     * the 64-bit significand, so one extra bit is produced and `rem` supplies
     * the sticky. */
    rem = ma;
    if (rem >= mb) { extra = 1; rem -= mb; }
    for (i = 0; i < 65; i++) {
        if (i == 64) { rbit = 0; }
        carry = rem >> 63;
        rem <<= 1;
        if (i < 64) q <<= 1;
        if (carry || rem >= mb) {
            rem -= mb;
            if (i < 64) q |= 1ull; else rbit = 1;
        }
    }
    if (extra) {
        v.hi = PF_XTOP | (q >> 1);
        v.lo = ((q & 1ull) ? PF_XTOP : 0ull) | (rbit ? (1ull << 62) : 0ull);
        return pf_round128(s, ea - eb + PF_XBIAS, v, rem != 0ull);
    }
    v.hi = q;
    v.lo = rbit ? PF_XTOP : 0ull;
    return pf_round128(s, ea - eb + PF_XBIAS - 1, v, rem != 0ull);
}

static unsigned int pf_fcmp(pf_x87_t a, pf_x87_t b)
{
    unsigned int sa, sb;
    int ea, eb, r;
    if (pf_is_nan(a) || pf_is_nan(b)) return 0x4500u;
    if (pf_is_zero(a) && pf_is_zero(b)) return 0x4000u;
    sa = pf_sgn(a); sb = pf_sgn(b);
    if (sa != sb) return sa ? 0x0100u : 0x0000u;
    ea = pf_bex(a); eb = pf_bex(b);
    if (ea != eb) r = (ea < eb) ? -1 : 1;
    else if (a.m != b.m) r = (a.m < b.m) ? -1 : 1;
    else r = 0;
    if (sa) r = -r;
    if (r > 0) return 0x0000u;
    if (r < 0) return 0x0100u;
    return 0x4000u;
}

#define PF_FI16(x)  pf_from_i16((short)(x))
#define PF_FI32(x)  pf_from_i32((int)(x))
#define PF_FI64(x)  pf_from_i64((long long)(x))
#define PF_FF32(x)  pf_from_f32((float)(x))
#define PF_FF64(x)  pf_from_f64((double)(x))
#define PF_ZERO     pf_zero(0u)
#define PF_ONE      pf_from_i32(1)
#define PF_ADD(a,b) pf_add((a), (b))
#define PF_SUB(a,b) pf_sub((a), (b))
#define PF_MUL(a,b) pf_mul((a), (b))
#define PF_DIV(a,b) pf_div((a), (b))
#define PF_NEG(a)   pf_neg(a)
#define PF_ABS(a)   pf_abs(a)
#define PF_TOF32(v) pf_to_f32(v)
#define PF_TOF64(v) pf_to_f64(v)
#define PF_TOI32(v, cw) pf_toi32((v), (cw))
#define PF_TOI16(v, cw) pf_toi16((v), (cw))

#endif /* PF_X87_SOFT_H */
