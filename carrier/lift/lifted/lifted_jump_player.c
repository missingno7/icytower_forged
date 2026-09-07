/* GENERATED FILE -- DO NOT EDIT.
 * generator : pf_lift.py
 * image     : icytower15.exe sha256=7570c6b0c7cddf6180d7c421bdc7d7bc1486c47a6d62cc6fde90670f62d4388d
 * function  : jump_player  VA=0x00418678  size=198 bytes
 * lifted    : 72 instructions in 14 reachable basic blocks
 *
 * LIFTED form (win32_pilot.md SS3).  Memory is the ORIGINAL memory:
 * every access goes through PF_MEM(addr), which is the identity by
 * default, so this object file is bindable at the original address.
 *
 * x87 HYPOTHESIS (win32_pilot.md SS3): 80-bit x87 intermediates are
 * modelled as `pf_x87_t`, currently typedef'd to `double` in pf_rt.h.
 * That typedef is the bet under test; swap it for a software 80-bit
 * type if the oracle comparison ever disagrees.
 */
#include "pf_rt.h"
#include "it_types.h"
#include "it_funcs.h"
#include "it_globals.h"
int __cdecl lifted_jump_player(Tplayer * a0, int a1)
{
    unsigned int r_eax = 0u;
    unsigned int r_ecx = 0u;
    unsigned int r_edx = 0u;
    unsigned int pf_ea = 0u;
    unsigned int pf_fres = 0u;
    pf_x87_t pf_fr[8];
    unsigned int pf_ftop = 0u;
    unsigned int pf_fsw = 0u;
    union { double d[1]; unsigned char b[8]; } pf_stk;
    union { double d[1]; unsigned char b[8]; } pf_arg;

    memset(pf_fr, 0, sizeof pf_fr);
    memset(pf_stk.b, 0, sizeof pf_stk.b);
    PF_CT_ASSERT(sizeof(a0) == 4);
    memcpy(pf_arg.b + 0, &a0, 4);
    PF_CT_ASSERT(sizeof(a1) == 4);
    memcpy(pf_arg.b + 4, &a1, 4);

    /* block 0x00418678 */
    /* 00418678  55                     push ebp */
    /* frame: saved ebp (symbolic, not stored) */
    /* 00418679  89e5                   mov ebp, esp */
    /* frame: mov (symbolic) */
    /* 0041867b  83ec04                 sub esp, 4 */
    /* frame: sub (symbolic) */
    /* 0041867e  8b5508                 mov edx, dword ptr [ebp + 8] */
    r_edx = PF_A32(0);
    /* 00418681  8b450c                 mov eax, dword ptr [ebp + 0xc] */
    r_eax = PF_A32(4);
    /* 00418684  85c0                   test eax, eax */
    pf_fres = ((r_eax & r_eax));
    /* 00418686  750c                   jne 0x418694 */
    if (!(pf_fres == 0u)) goto L_00418694;
    goto L_00418688;
L_00418688:
    ;
    /* 00418688  8b4234                 mov eax, dword ptr [edx + 0x34] */
    pf_ea = (unsigned int)(r_edx + 0x34u);
    r_eax = PF_R32(pf_ea);
    /* 0041868b  85c0                   test eax, eax */
    pf_fres = ((r_eax & r_eax));
    /* 0041868d  7425                   je 0x4186b4 */
    if ((pf_fres == 0u)) goto L_004186b4;
    goto L_0041868f;
L_0041868f:
    ;
    /* 0041868f  31c0                   xor eax, eax */
    r_eax = ((r_eax ^ r_eax));
    /* 00418691  c9                     leave  */
    /* frame: leave (symbolic) */
    /* 00418692  c3                     ret  */
    return (int)r_eax;
L_00418694:
    ;
    /* 00418694  c7423401000000         mov dword ptr [edx + 0x34], 1 */
    pf_ea = (unsigned int)(r_edx + 0x34u);
    PF_W32(pf_ea, 0x1u);
    /* 0041869b  8d0440                 lea eax, [eax + eax*2] */
    r_eax = (unsigned int)(r_eax + r_eax * 2u);
    /* 0041869e  c1e002                 shl eax, 2 */
    r_eax = ((r_eax << 2u));
    /* 004186a1  f7d8                   neg eax */
    r_eax = ((0u - r_eax));
    /* 004186a3  8945fc                 mov dword ptr [ebp - 4], eax */
    PF_SW32(0, r_eax);
    /* 004186a6  db45fc                 fild dword ptr [ebp - 4] */
    PF_PUSH(PF_FI32(PF_S32(0)));
    /* 004186a9  dd5a18                 fstp qword ptr [edx + 0x18] */
    pf_ea = (unsigned int)(r_edx + 0x18u);
    PF_WB64(pf_ea, PF_ST(0));
    PF_POP();
    /* 004186ac  b8ffffffff             mov eax, 0xffffffff */
    r_eax = 0xffffffffu;
    /* 004186b1  c9                     leave  */
    /* frame: leave (symbolic) */
    /* 004186b2  c3                     ret  */
    return (int)r_eax;
L_004186b4:
    ;
    /* 004186b4  c7423401000000         mov dword ptr [edx + 0x34], 1 */
    pf_ea = (unsigned int)(r_edx + 0x34u);
    PF_W32(pf_ea, 0x1u);
    /* 004186bb  dd4210                 fld qword ptr [edx + 0x10] */
    pf_ea = (unsigned int)(r_edx + 0x10u);
    PF_PUSH(PF_FF64(PF_RF64(pf_ea)));
    /* 004186be  d9c0                   fld st(0) */
    PF_PUSH(PF_ST(0));
    /* 004186c0  d8c1                   fadd st(1) */
    PF_ST(0) = PF_ADD(PF_ST(0), PF_ST(1));
    /* 004186c2  d9ee                   fldz  */
    PF_PUSH(PF_ZERO);
    /* 004186c4  d9c9                   fxch st(1) */
    { pf_x87_t pf_t = PF_ST(0); PF_ST(0) = PF_ST(1); PF_ST(1) = pf_t; }
    /* 004186c6  dde1                   fucom st(1) */
    pf_fsw = pf_fcmp(PF_ST(0), PF_ST(1));
    /* 004186c8  dfe0                   fnstsw ax */
    PF_SET16(r_eax, PF_FSW());
    /* 004186ca  ddd9                   fstp st(1) */
    PF_ST(1) = PF_ST(0);
    PF_POP();
    /* 004186cc  f6c405                 test ah, 5 */
    pf_fres = ((PF_GET8H(r_eax) & 0x5u) & 0xFFu);
    /* 004186cf  0f94c1                 sete cl */
    PF_SET8L(r_ecx, ((pf_fres == 0u)) ? 1u : 0u);
    /* 004186d2  7460                   je 0x418734 */
    if ((pf_fres == 0u)) goto L_00418734;
    goto L_004186d4;
L_004186d4:
    ;
    /* 004186d4  d9c0                   fld st(0) */
    PF_PUSH(PF_ST(0));
    goto L_004186d6;
L_004186d6:
    ;
    /* 004186d6  a140d14d00             mov eax, dword ptr [0x4dd140] */
    pf_ea = (unsigned int)(0x4dd140u);
    r_eax = PF_R32(pf_ea);
    /* 004186db  dd04c580db4b00         fld qword ptr [eax*8 + 0x4bdb80] */
    pf_ea = (unsigned int)(r_eax * 8u + 0x4bdb80u);
    PF_PUSH(PF_FF64(PF_RF64(pf_ea)));
    /* 004186e2  d9e0                   fchs  */
    PF_ST(0) = PF_NEG(PF_ST(0));
    /* 004186e4  dde1                   fucom st(1) */
    pf_fsw = pf_fcmp(PF_ST(0), PF_ST(1));
    /* 004186e6  dfe0                   fnstsw ax */
    PF_SET16(r_eax, PF_FSW());
    /* 004186e8  ddd9                   fstp st(1) */
    PF_ST(1) = PF_ST(0);
    PF_POP();
    /* 004186ea  f6c445                 test ah, 0x45 */
    pf_fres = ((PF_GET8H(r_eax) & 0x45u) & 0xFFu);
    /* 004186ed  7431                   je 0x418720 */
    if ((pf_fres == 0u)) goto L_00418720;
    goto L_004186ef;
L_004186ef:
    ;
    /* 004186ef  ddd9                   fstp st(1) */
    PF_ST(1) = PF_ST(0);
    PF_POP();
    goto L_004186f1;
L_004186f1:
    ;
    /* 004186f1  dd5218                 fst qword ptr [edx + 0x18] */
    pf_ea = (unsigned int)(r_edx + 0x18u);
    PF_WB64(pf_ea, PF_ST(0));
    /* 004186f4  d9c9                   fxch st(1) */
    { pf_x87_t pf_t = PF_ST(0); PF_ST(0) = PF_ST(1); PF_ST(1) = pf_t; }
    /* 004186f6  dd5a20                 fstp qword ptr [edx + 0x20] */
    pf_ea = (unsigned int)(r_edx + 0x20u);
    PF_WB64(pf_ea, PF_ST(0));
    PF_POP();
    /* 004186f9  d90534714d00           fld dword ptr [0x4d7134] */
    pf_ea = (unsigned int)(0x4d7134u);
    PF_PUSH(PF_FF32(PF_RF32(pf_ea)));
    /* 004186ff  dae9                   fucompp  */
    pf_fsw = pf_fcmp(PF_ST(0), PF_ST(1));
    PF_POP(); PF_POP();
    /* 00418701  dfe0                   fnstsw ax */
    PF_SET16(r_eax, PF_FSW());
    /* 00418703  f6c445                 test ah, 0x45 */
    pf_fres = ((PF_GET8H(r_eax) & 0x45u) & 0xFFu);
    /* 00418706  7507                   jne 0x41870f */
    if (!(pf_fres == 0u)) goto L_0041870f;
    goto L_00418708;
L_00418708:
    ;
    /* 00418708  c7425001000000         mov dword ptr [edx + 0x50], 1 */
    pf_ea = (unsigned int)(r_edx + 0x50u);
    PF_W32(pf_ea, 0x1u);
    goto L_0041870f;
L_0041870f:
    ;
    /* 0041870f  c7425400000000         mov dword ptr [edx + 0x54], 0 */
    pf_ea = (unsigned int)(r_edx + 0x54u);
    PF_W32(pf_ea, 0x0u);
    /* 00418716  b8ffffffff             mov eax, 0xffffffff */
    r_eax = 0xffffffffu;
    /* 0041871b  c9                     leave  */
    /* frame: leave (symbolic) */
    /* 0041871c  c3                     ret  */
    return (int)r_eax;
L_00418720:
    ;
    /* 00418720  ddd8                   fstp st(0) */
    PF_ST(0) = PF_ST(0);
    PF_POP();
    /* 00418722  84c9                   test cl, cl */
    pf_fres = ((PF_GET8L(r_ecx) & PF_GET8L(r_ecx)) & 0xFFu);
    /* 00418724  74cb                   je 0x4186f1 */
    if ((pf_fres == 0u)) goto L_004186f1;
    goto L_00418726;
L_00418726:
    ;
    /* 00418726  ddd8                   fstp st(0) */
    PF_ST(0) = PF_ST(0);
    PF_POP();
    /* 00418728  d9c0                   fld st(0) */
    PF_PUSH(PF_ST(0));
    /* 0041872a  d80d30714d00           fmul dword ptr [0x4d7130] */
    pf_ea = (unsigned int)(0x4d7130u);
    PF_ST(0) = PF_MUL(PF_ST(0), PF_FF32(PF_RF32(pf_ea)));
    /* 00418730  ebbf                   jmp 0x4186f1 */
    goto L_004186f1;
L_00418734:
    ;
    /* 00418734  d9c1                   fld st(1) */
    PF_PUSH(PF_ST(1));
    /* 00418736  d80d30714d00           fmul dword ptr [0x4d7130] */
    pf_ea = (unsigned int)(0x4d7130u);
    PF_ST(0) = PF_MUL(PF_ST(0), PF_FF32(PF_RF32(pf_ea)));
    /* 0041873c  eb98                   jmp 0x4186d6 */
    goto L_004186d6;
}

