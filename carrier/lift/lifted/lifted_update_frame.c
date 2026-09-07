/* GENERATED FILE -- DO NOT EDIT.
 * generator : pf_lift.py
 * image     : icytower15.exe sha256=7570c6b0c7cddf6180d7c421bdc7d7bc1486c47a6d62cc6fde90670f62d4388d
 * function  : update_frame  VA=0x00406ac4  size=120 bytes
 * lifted    : 40 instructions in 14 reachable basic blocks
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
void __cdecl lifted_update_frame(void)
{
    unsigned int r_eax = 0u;
    unsigned int r_ecx = 0u;
    unsigned int r_edx = 0u;
    unsigned int r_ebx = 0u;
    unsigned int pf_ea = 0u;
    unsigned int pf_fa = 0u;
    unsigned int pf_fb = 0u;
    unsigned int pf_fres = 0u;
    long long pf_num = 0, pf_den = 0, pf_quo = 0;
    union { double d[1]; unsigned char b[8]; } pf_stk;

    memset(pf_stk.b, 0, sizeof pf_stk.b);

    /* block 0x00406ac4 */
    /* 00406ac4  55                     push ebp */
    /* frame: saved ebp (symbolic, not stored) */
    /* 00406ac5  89e5                   mov ebp, esp */
    /* frame: mov (symbolic) */
    /* 00406ac7  53                     push ebx */
    PF_SW32(0, r_ebx);
    /* 00406ac8  a168ec4f00             mov eax, dword ptr [0x4fec68] */
    pf_ea = (unsigned int)(0x4fec68u);
    r_eax = PF_R32(pf_ea);
    /* 00406acd  85c0                   test eax, eax */
    pf_fres = ((r_eax & r_eax));
    /* 00406acf  741a                   je 0x406aeb */
    if ((pf_fres == 0u)) goto L_00406aeb;
    goto L_00406ad1;
L_00406ad1:
    ;
    /* 00406ad1  83f83c                 cmp eax, 0x3c */
    pf_fa = r_eax;
    pf_fb = 0x3cu;
    pf_fres = (pf_fa - pf_fb);
    /* 00406ad4  7f5a                   jg 0x406b30 */
    if ((!(pf_fres == 0u) && (((pf_fres >> 31) & 1u)) == (((((pf_fa ^ pf_fb) & (pf_fa ^ pf_fres)) >> 31) & 1u)))) goto L_00406b30;
    goto L_00406ad6;
L_00406ad6:
    ;
    /* 00406ad6  83f809                 cmp eax, 9 */
    pf_fa = r_eax;
    pf_fb = 0x9u;
    pf_fres = (pf_fa - pf_fb);
    /* 00406ad9  7f0a                   jg 0x406ae5 */
    if ((!(pf_fres == 0u) && (((pf_fres >> 31) & 1u)) == (((((pf_fa ^ pf_fb) & (pf_fa ^ pf_fres)) >> 31) & 1u)))) goto L_00406ae5;
    goto L_00406adb;
L_00406adb:
    ;
    /* 00406adb  812d28ac4f009a190000   sub dword ptr [0x4fac28], 0x199a */
    pf_ea = (unsigned int)(0x4fac28u);
    PF_W32(pf_ea, ((PF_R32(pf_ea) - 0x199au)));
    goto L_00406ae5;
L_00406ae5:
    ;
    /* 00406ae5  48                     dec eax */
    r_eax = ((r_eax - 1u));
    /* 00406ae6  a368ec4f00             mov dword ptr [0x4fec68], eax */
    pf_ea = (unsigned int)(0x4fec68u);
    PF_W32(pf_ea, r_eax);
    goto L_00406aeb;
L_00406aeb:
    ;
    /* 00406aeb  a118e54f00             mov eax, dword ptr [0x4fe518] */
    pf_ea = (unsigned int)(0x4fe518u);
    r_eax = PF_R32(pf_ea);
    /* 00406af0  8b0c8528f14f00         mov ecx, dword ptr [eax*4 + 0x4ff128] */
    pf_ea = (unsigned int)(r_eax * 4u + 0x4ff128u);
    r_ecx = PF_R32(pf_ea);
    /* 00406af7  8b414c                 mov eax, dword ptr [ecx + 0x4c] */
    pf_ea = (unsigned int)(r_ecx + 0x4cu);
    r_eax = PF_R32(pf_ea);
    /* 00406afa  85c0                   test eax, eax */
    pf_fres = ((r_eax & r_eax));
    /* 00406afc  7407                   je 0x406b05 */
    if ((pf_fres == 0u)) goto L_00406b05;
    goto L_00406afe;
L_00406afe:
    ;
    /* 00406afe  3d2b010000             cmp eax, 0x12b */
    pf_fa = r_eax;
    pf_fb = 0x12bu;
    pf_fres = (pf_fa - pf_fb);
    /* 00406b03  7e23                   jle 0x406b28 */
    if (((pf_fres == 0u) || (((pf_fres >> 31) & 1u)) != (((((pf_fa ^ pf_fb) & (pf_fa ^ pf_fres)) >> 31) & 1u)))) goto L_00406b28;
    goto L_00406b05;
L_00406b05:
    ;
    /* 00406b05  8b4158                 mov eax, dword ptr [ecx + 0x58] */
    pf_ea = (unsigned int)(r_ecx + 0x58u);
    r_eax = PF_R32(pf_ea);
    /* 00406b08  85c0                   test eax, eax */
    pf_fres = ((r_eax & r_eax));
    /* 00406b0a  7403                   je 0x406b0f */
    if ((pf_fres == 0u)) goto L_00406b0f;
    goto L_00406b0c;
L_00406b0c:
    ;
    /* 00406b0c  ff415c                 inc dword ptr [ecx + 0x5c] */
    pf_ea = (unsigned int)(r_ecx + 0x5cu);
    PF_W32(pf_ea, ((PF_R32(pf_ea) + 1u)));
    goto L_00406b0f;
L_00406b0f:
    ;
    /* 00406b0f  a158695000             mov eax, dword ptr [0x506958] */
    pf_ea = (unsigned int)(0x506958u);
    r_eax = PF_R32(pf_ea);
    /* 00406b14  ba0a000000             mov edx, 0xa */
    r_edx = 0xau;
    /* 00406b19  89d3                   mov ebx, edx */
    r_ebx = r_edx;
    /* 00406b1b  99                     cdq  */
    r_edx = (r_eax & 0x80000000u) ? 0xFFFFFFFFu : 0u;
    /* 00406b1c  f7fb                   idiv ebx */
    pf_num = (long long)(((unsigned long long)r_edx << 32) | r_eax);
    pf_den = (long long)(int)(r_ebx);
    if (pf_den == 0) PF_TRAP(0x00406b1cu, "idiv #DE: divisor is zero");
    pf_quo = pf_num / pf_den;
    if (pf_quo < -2147483648LL || pf_quo > 2147483647LL)
        PF_TRAP(0x00406b1cu, "idiv #DE: quotient does not fit in 32 bits");
    r_eax = (unsigned int)(int)pf_quo;
    r_edx = (unsigned int)(int)(pf_num % pf_den);
    /* 00406b1e  85d2                   test edx, edx */
    pf_fres = ((r_edx & r_edx));
    /* 00406b20  7503                   jne 0x406b25 */
    if (!(pf_fres == 0u)) goto L_00406b25;
    goto L_00406b22;
L_00406b22:
    ;
    /* 00406b22  ff413c                 inc dword ptr [ecx + 0x3c] */
    pf_ea = (unsigned int)(r_ecx + 0x3cu);
    PF_W32(pf_ea, ((PF_R32(pf_ea) + 1u)));
    goto L_00406b25;
L_00406b25:
    ;
    /* 00406b25  5b                     pop ebx */
    r_ebx = PF_S32(0);
    /* 00406b26  c9                     leave  */
    /* frame: leave (symbolic) */
    /* 00406b27  c3                     ret  */
    return;
L_00406b28:
    ;
    /* 00406b28  83c008                 add eax, 8 */
    r_eax = ((r_eax + 0x8u));
    /* 00406b2b  89414c                 mov dword ptr [ecx + 0x4c], eax */
    pf_ea = (unsigned int)(r_ecx + 0x4cu);
    PF_W32(pf_ea, r_eax);
    /* 00406b2e  ebd5                   jmp 0x406b05 */
    goto L_00406b05;
L_00406b30:
    ;
    /* 00406b30  810528ac4f00cd0c0000   add dword ptr [0x4fac28], 0xccd */
    pf_ea = (unsigned int)(0x4fac28u);
    PF_W32(pf_ea, ((PF_R32(pf_ea) + 0xccdu)));
    /* 00406b3a  eba9                   jmp 0x406ae5 */
    goto L_00406ae5;
}

