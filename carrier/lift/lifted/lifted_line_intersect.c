/* GENERATED FILE -- DO NOT EDIT.
 * generator : pf_lift.py
 * image     : icytower15.exe sha256=7570c6b0c7cddf6180d7c421bdc7d7bc1486c47a6d62cc6fde90670f62d4388d
 * function  : line_intersect  VA=0x00406b80  size=302 bytes
 * lifted    : 126 instructions in 10 reachable basic blocks
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
int __cdecl lifted_line_intersect(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7, int * a8, int * a9)
{
    unsigned int r_eax = 0u;
    unsigned int r_ecx = 0u;
    unsigned int r_edx = 0u;
    unsigned int r_ebx = 0u;
    unsigned int r_esi = 0u;
    unsigned int r_edi = 0u;
    unsigned int pf_ea = 0u;
    unsigned int pf_fres = 0u;
    pf_x87_t pf_fr[8];
    unsigned int pf_ftop = 0u;
    unsigned int pf_fsw = 0u;
    unsigned int pf_fcw = PF_CW_INIT;
    union { double d[5]; unsigned char b[40]; } pf_stk;
    union { double d[5]; unsigned char b[40]; } pf_arg;

    memset(pf_fr, 0, sizeof pf_fr);
    memset(pf_stk.b, 0, sizeof pf_stk.b);
    PF_CT_ASSERT(sizeof(a0) == 4);
    memcpy(pf_arg.b + 0, &a0, 4);
    PF_CT_ASSERT(sizeof(a1) == 4);
    memcpy(pf_arg.b + 4, &a1, 4);
    PF_CT_ASSERT(sizeof(a2) == 4);
    memcpy(pf_arg.b + 8, &a2, 4);
    PF_CT_ASSERT(sizeof(a3) == 4);
    memcpy(pf_arg.b + 12, &a3, 4);
    PF_CT_ASSERT(sizeof(a4) == 4);
    memcpy(pf_arg.b + 16, &a4, 4);
    PF_CT_ASSERT(sizeof(a5) == 4);
    memcpy(pf_arg.b + 20, &a5, 4);
    PF_CT_ASSERT(sizeof(a6) == 4);
    memcpy(pf_arg.b + 24, &a6, 4);
    PF_CT_ASSERT(sizeof(a7) == 4);
    memcpy(pf_arg.b + 28, &a7, 4);
    PF_CT_ASSERT(sizeof(a8) == 4);
    memcpy(pf_arg.b + 32, &a8, 4);
    PF_CT_ASSERT(sizeof(a9) == 4);
    memcpy(pf_arg.b + 36, &a9, 4);

    /* block 0x00406b80 */
    /* 00406b80  55                     push ebp */
    /* frame: saved ebp (symbolic, not stored) */
    /* 00406b81  89e5                   mov ebp, esp */
    /* frame: mov (symbolic) */
    /* 00406b83  57                     push edi */
    PF_SW32(28, r_edi);
    /* 00406b84  56                     push esi */
    PF_SW32(24, r_esi);
    /* 00406b85  53                     push ebx */
    PF_SW32(20, r_ebx);
    /* 00406b86  83ec10                 sub esp, 0x10 */
    /* frame: sub (symbolic) */
    /* 00406b89  8b4d0c                 mov ecx, dword ptr [ebp + 0xc] */
    r_ecx = PF_A32(4);
    /* 00406b8c  8b5510                 mov edx, dword ptr [ebp + 0x10] */
    r_edx = PF_A32(8);
    /* 00406b8f  8b4518                 mov eax, dword ptr [ebp + 0x18] */
    r_eax = PF_A32(16);
    /* 00406b92  8b7d1c                 mov edi, dword ptr [ebp + 0x1c] */
    r_edi = PF_A32(20);
    /* 00406b95  2b5508                 sub edx, dword ptr [ebp + 8] */
    r_edx = ((r_edx - PF_A32(0)));
    /* 00406b98  8b5d14                 mov ebx, dword ptr [ebp + 0x14] */
    r_ebx = PF_A32(12);
    /* 00406b9b  29cb                   sub ebx, ecx */
    r_ebx = ((r_ebx - r_ecx));
    /* 00406b9d  895de8                 mov dword ptr [ebp - 0x18], ebx */
    PF_SW32(8, r_ebx);
    /* 00406ba0  89c6                   mov esi, eax */
    r_esi = r_eax;
    /* 00406ba2  2b7520                 sub esi, dword ptr [ebp + 0x20] */
    r_esi = ((r_esi - PF_A32(24)));
    /* 00406ba5  0faff3                 imul esi, ebx */
    r_esi = ((r_esi) * (r_ebx));
    /* 00406ba8  8b5d24                 mov ebx, dword ptr [ebp + 0x24] */
    r_ebx = PF_A32(28);
    /* 00406bab  29fb                   sub ebx, edi */
    r_ebx = ((r_ebx - r_edi));
    /* 00406bad  0fafda                 imul ebx, edx */
    r_ebx = ((r_ebx) * (r_edx));
    /* 00406bb0  8d1c1e                 lea ebx, [esi + ebx] */
    r_ebx = (unsigned int)(r_esi + r_ebx);
    /* 00406bb3  895dec                 mov dword ptr [ebp - 0x14], ebx */
    PF_SW32(12, r_ebx);
    /* 00406bb6  db45ec                 fild dword ptr [ebp - 0x14] */
    PF_PUSH(PF_FI32(PF_S32(12)));
    /* 00406bb9  89ce                   mov esi, ecx */
    r_esi = r_ecx;
    /* 00406bbb  29fe                   sub esi, edi */
    r_esi = ((r_esi - r_edi));
    /* 00406bbd  8b5d08                 mov ebx, dword ptr [ebp + 8] */
    r_ebx = PF_A32(0);
    /* 00406bc0  29c3                   sub ebx, eax */
    r_ebx = ((r_ebx - r_eax));
    /* 00406bc2  895de4                 mov dword ptr [ebp - 0x1c], ebx */
    PF_SW32(4, r_ebx);
    /* 00406bc5  2b7d24                 sub edi, dword ptr [ebp + 0x24] */
    r_edi = ((r_edi - PF_A32(28)));
    /* 00406bc8  0faffb                 imul edi, ebx */
    r_edi = ((r_edi) * (r_ebx));
    /* 00406bcb  8b5d20                 mov ebx, dword ptr [ebp + 0x20] */
    r_ebx = PF_A32(24);
    /* 00406bce  29c3                   sub ebx, eax */
    r_ebx = ((r_ebx - r_eax));
    /* 00406bd0  89d8                   mov eax, ebx */
    r_eax = r_ebx;
    /* 00406bd2  0fafc6                 imul eax, esi */
    r_eax = ((r_eax) * (r_esi));
    /* 00406bd5  01c7                   add edi, eax */
    r_edi = ((r_edi + r_eax));
    /* 00406bd7  d9c0                   fld st(0) */
    PF_PUSH(PF_ST(0));
    /* 00406bd9  57                     push edi */
    PF_SW32(0, r_edi);
    /* 00406bda  da3c24                 fidivr dword ptr [esp] */
    PF_CW_ARITH(0x00406bdau, pf_fcw);
    PF_ST(0) = PF_DIV(PF_FI32(PF_S32(0)), PF_ST(0));
    /* 00406bdd  83c404                 add esp, 4 */
    /* frame: add (symbolic) */
    /* 00406be0  d9ee                   fldz  */
    PF_PUSH(PF_ZERO);
    /* 00406be2  dde1                   fucom st(1) */
    pf_fsw = pf_fcmp(PF_ST(0), PF_ST(1));
    /* 00406be4  dfe0                   fnstsw ax */
    PF_SET16(r_eax, PF_FSW());
    /* 00406be6  f6c445                 test ah, 0x45 */
    pf_fres = ((PF_GET8H(r_eax) & 0x45u) & 0xFFu);
    /* 00406be9  0f8499000000           je 0x406c88 */
    if ((pf_fres == 0u)) goto L_00406c88;
    goto L_00406bef;
L_00406bef:
    ;
    /* 00406bef  d9ca                   fxch st(2) */
    { pf_x87_t pf_t = PF_ST(0); PF_ST(0) = PF_ST(2); PF_ST(2) = pf_t; }
    /* 00406bf1  0faff2                 imul esi, edx */
    r_esi = ((r_esi) * (r_edx));
    /* 00406bf4  89c8                   mov eax, ecx */
    r_eax = r_ecx;
    /* 00406bf6  2b4514                 sub eax, dword ptr [ebp + 0x14] */
    r_eax = ((r_eax - PF_A32(12)));
    /* 00406bf9  0faf45e4               imul eax, dword ptr [ebp - 0x1c] */
    r_eax = ((r_eax) * (PF_S32(4)));
    /* 00406bfd  01c6                   add esi, eax */
    r_esi = ((r_esi + r_eax));
    /* 00406bff  56                     push esi */
    PF_SW32(0, r_esi);
    /* 00406c00  da3c24                 fidivr dword ptr [esp] */
    PF_CW_ARITH(0x00406c00u, pf_fcw);
    PF_ST(0) = PF_DIV(PF_FI32(PF_S32(0)), PF_ST(0));
    /* 00406c03  d9ca                   fxch st(2) */
    { pf_x87_t pf_t = PF_ST(0); PF_ST(0) = PF_ST(2); PF_ST(2) = pf_t; }
    /* 00406c05  83c404                 add esp, 4 */
    /* frame: add (symbolic) */
    /* 00406c08  ddea                   fucomp st(2) */
    pf_fsw = pf_fcmp(PF_ST(0), PF_ST(2));
    PF_POP();
    /* 00406c0a  dfe0                   fnstsw ax */
    PF_SET16(r_eax, PF_FSW());
    /* 00406c0c  f6c445                 test ah, 0x45 */
    pf_fres = ((PF_GET8H(r_eax) & 0x45u) & 0xFFu);
    /* 00406c0f  747f                   je 0x406c90 */
    if ((pf_fres == 0u)) goto L_00406c90;
    goto L_00406c11;
L_00406c11:
    ;
    /* 00406c11  d9e8                   fld1  */
    PF_PUSH(PF_ONE);
    /* 00406c13  d9c9                   fxch st(1) */
    { pf_x87_t pf_t = PF_ST(0); PF_ST(0) = PF_ST(1); PF_ST(1) = pf_t; }
    /* 00406c15  dde1                   fucom st(1) */
    pf_fsw = pf_fcmp(PF_ST(0), PF_ST(1));
    /* 00406c17  dfe0                   fnstsw ax */
    PF_SET16(r_eax, PF_FSW());
    /* 00406c19  f6c445                 test ah, 0x45 */
    pf_fres = ((PF_GET8H(r_eax) & 0x45u) & 0xFFu);
    /* 00406c1c  747a                   je 0x406c98 */
    if ((pf_fres == 0u)) goto L_00406c98;
    goto L_00406c1e;
L_00406c1e:
    ;
    /* 00406c1e  d9ca                   fxch st(2) */
    { pf_x87_t pf_t = PF_ST(0); PF_ST(0) = PF_ST(2); PF_ST(2) = pf_t; }
    /* 00406c20  dae9                   fucompp  */
    pf_fsw = pf_fcmp(PF_ST(0), PF_ST(1));
    PF_POP(); PF_POP();
    /* 00406c22  dfe0                   fnstsw ax */
    PF_SET16(r_eax, PF_FSW());
    /* 00406c24  f6c445                 test ah, 0x45 */
    pf_fres = ((PF_GET8H(r_eax) & 0x45u) & 0xFFu);
    /* 00406c27  7477                   je 0x406ca0 */
    if ((pf_fres == 0u)) goto L_00406ca0;
    goto L_00406c29;
L_00406c29:
    ;
    /* 00406c29  d9c0                   fld st(0) */
    PF_PUSH(PF_ST(0));
    /* 00406c2b  52                     push edx */
    PF_SW32(0, r_edx);
    /* 00406c2c  da0c24                 fimul dword ptr [esp] */
    PF_CW_ARITH(0x00406c2cu, pf_fcw);
    PF_ST(0) = PF_MUL(PF_ST(0), PF_FI32(PF_S32(0)));
    /* 00406c2f  83c404                 add esp, 4 */
    /* frame: add (symbolic) */
    /* 00406c32  d905b46c4d00           fld dword ptr [0x4d6cb4] */
    pf_ea = (unsigned int)(0x4d6cb4u);
    PF_PUSH(PF_FF32(PF_RF32(pf_ea)));
    /* 00406c38  dcc1                   fadd st(1), st(0) */
    PF_CW_ARITH(0x00406c38u, pf_fcw);
    PF_ST(1) = PF_ADD(PF_ST(1), PF_ST(0));
    /* 00406c3a  d9c9                   fxch st(1) */
    { pf_x87_t pf_t = PF_ST(0); PF_ST(0) = PF_ST(1); PF_ST(1) = pf_t; }
    /* 00406c3c  d97df2                 fnstcw word ptr [ebp - 0xe] */
    PF_SW16(18, pf_fcw);
    /* 00406c3f  668b45f2               mov ax, word ptr [ebp - 0xe] */
    PF_SET16(r_eax, PF_S16(18));
    /* 00406c43  b40c                   mov ah, 0xc */
    PF_SET8H(r_eax, 0xcu);
    /* 00406c45  668945f0               mov word ptr [ebp - 0x10], ax */
    PF_SW16(16, PF_GET16(r_eax));
    /* 00406c49  d96df0                 fldcw word ptr [ebp - 0x10] */
    pf_fcw = PF_S16(16);
    /* 00406c4c  db5dec                 fistp dword ptr [ebp - 0x14] */
    PF_SW32(12, PF_TOI32(PF_ST(0), pf_fcw));
    PF_POP();
    /* 00406c4f  d96df2                 fldcw word ptr [ebp - 0xe] */
    pf_fcw = PF_S16(18);
    /* 00406c52  d9c9                   fxch st(1) */
    { pf_x87_t pf_t = PF_ST(0); PF_ST(0) = PF_ST(1); PF_ST(1) = pf_t; }
    /* 00406c54  8b55ec                 mov edx, dword ptr [ebp - 0x14] */
    r_edx = PF_S32(12);
    /* 00406c57  035508                 add edx, dword ptr [ebp + 8] */
    r_edx = ((r_edx + PF_A32(0)));
    /* 00406c5a  8b4528                 mov eax, dword ptr [ebp + 0x28] */
    r_eax = PF_A32(32);
    /* 00406c5d  8910                   mov dword ptr [eax], edx */
    pf_ea = (unsigned int)(r_eax);
    PF_W32(pf_ea, r_edx);
    /* 00406c5f  da4de8                 fimul dword ptr [ebp - 0x18] */
    PF_CW_ARITH(0x00406c5fu, pf_fcw);
    PF_ST(0) = PF_MUL(PF_ST(0), PF_FI32(PF_S32(8)));
    /* 00406c62  dec1                   faddp st(1) */
    PF_CW_ARITH(0x00406c62u, pf_fcw);
    PF_ST(1) = PF_ADD(PF_ST(1), PF_ST(0));
    PF_POP();
    /* 00406c64  d96df0                 fldcw word ptr [ebp - 0x10] */
    pf_fcw = PF_S16(16);
    /* 00406c67  db5dec                 fistp dword ptr [ebp - 0x14] */
    PF_SW32(12, PF_TOI32(PF_ST(0), pf_fcw));
    PF_POP();
    /* 00406c6a  d96df2                 fldcw word ptr [ebp - 0xe] */
    pf_fcw = PF_S16(18);
    /* 00406c6d  8b45ec                 mov eax, dword ptr [ebp - 0x14] */
    r_eax = PF_S32(12);
    /* 00406c70  8d0c08                 lea ecx, [eax + ecx] */
    r_ecx = (unsigned int)(r_eax + r_ecx);
    /* 00406c73  8b452c                 mov eax, dword ptr [ebp + 0x2c] */
    r_eax = PF_A32(36);
    /* 00406c76  8908                   mov dword ptr [eax], ecx */
    pf_ea = (unsigned int)(r_eax);
    PF_W32(pf_ea, r_ecx);
    /* 00406c78  b801000000             mov eax, 1 */
    r_eax = 0x1u;
    /* 00406c7d  83c410                 add esp, 0x10 */
    /* frame: add (symbolic) */
    /* 00406c80  5b                     pop ebx */
    r_ebx = PF_S32(20);
    /* 00406c81  5e                     pop esi */
    r_esi = PF_S32(24);
    /* 00406c82  5f                     pop edi */
    r_edi = PF_S32(28);
    /* 00406c83  c9                     leave  */
    /* frame: leave (symbolic) */
    /* 00406c84  c3                     ret  */
    return (int)r_eax;
L_00406c88:
    ;
    /* 00406c88  ddd8                   fstp st(0) */
    PF_ST(0) = PF_ST(0);
    PF_POP();
    /* 00406c8a  ddd8                   fstp st(0) */
    PF_ST(0) = PF_ST(0);
    PF_POP();
    /* 00406c8c  ddd8                   fstp st(0) */
    PF_ST(0) = PF_ST(0);
    PF_POP();
    /* 00406c8e  eb14                   jmp 0x406ca4 */
    goto L_00406ca4;
L_00406c90:
    ;
    /* 00406c90  ddd8                   fstp st(0) */
    PF_ST(0) = PF_ST(0);
    PF_POP();
    /* 00406c92  ddd8                   fstp st(0) */
    PF_ST(0) = PF_ST(0);
    PF_POP();
    /* 00406c94  eb0e                   jmp 0x406ca4 */
    goto L_00406ca4;
L_00406c98:
    ;
    /* 00406c98  ddd8                   fstp st(0) */
    PF_ST(0) = PF_ST(0);
    PF_POP();
    /* 00406c9a  ddd8                   fstp st(0) */
    PF_ST(0) = PF_ST(0);
    PF_POP();
    /* 00406c9c  ddd8                   fstp st(0) */
    PF_ST(0) = PF_ST(0);
    PF_POP();
    /* 00406c9e  eb04                   jmp 0x406ca4 */
    goto L_00406ca4;
L_00406ca0:
    ;
    /* 00406ca0  ddd8                   fstp st(0) */
    PF_ST(0) = PF_ST(0);
    PF_POP();
    /* 00406ca2  6690                   nop  */
    goto L_00406ca4;
L_00406ca4:
    ;
    /* 00406ca4  31c0                   xor eax, eax */
    r_eax = ((r_eax ^ r_eax));
    /* 00406ca6  83c410                 add esp, 0x10 */
    /* frame: add (symbolic) */
    /* 00406ca9  5b                     pop ebx */
    r_ebx = PF_S32(20);
    /* 00406caa  5e                     pop esi */
    r_esi = PF_S32(24);
    /* 00406cab  5f                     pop edi */
    r_edi = PF_S32(28);
    /* 00406cac  c9                     leave  */
    /* frame: leave (symbolic) */
    /* 00406cad  c3                     ret  */
    return (int)r_eax;
}

