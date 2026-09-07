/* GENERATED FILE -- DO NOT EDIT.
 * generator : pf_lift.py
 * image     : icytower15.exe sha256=7570c6b0c7cddf6180d7c421bdc7d7bc1486c47a6d62cc6fde90670f62d4388d
 * function  : is_solid  VA=0x004166dc  size=107 bytes
 * lifted    : 46 instructions in 8 reachable basic blocks
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
int __cdecl lifted_is_solid(Tmap * a0, int a1, int a2)
{
    unsigned int r_eax = 0u;
    unsigned int r_ecx = 0u;
    unsigned int r_edx = 0u;
    unsigned int r_ebx = 0u;
    unsigned int r_esi = 0u;
    unsigned int pf_ea = 0u;
    unsigned int pf_fa = 0u;
    unsigned int pf_fb = 0u;
    unsigned int pf_fres = 0u;
    union { double d[2]; unsigned char b[16]; } pf_stk;
    union { double d[2]; unsigned char b[12]; } pf_arg;

    memset(pf_stk.b, 0, sizeof pf_stk.b);
    PF_CT_ASSERT(sizeof(a0) == 4);
    memcpy(pf_arg.b + 0, &a0, 4);
    PF_CT_ASSERT(sizeof(a1) == 4);
    memcpy(pf_arg.b + 4, &a1, 4);
    PF_CT_ASSERT(sizeof(a2) == 4);
    memcpy(pf_arg.b + 8, &a2, 4);

    /* block 0x004166dc */
    /* 004166dc  55                     push ebp */
    /* frame: saved ebp (symbolic, not stored) */
    /* 004166dd  89e5                   mov ebp, esp */
    /* frame: mov (symbolic) */
    /* 004166df  56                     push esi */
    PF_SW32(4, r_esi);
    /* 004166e0  53                     push ebx */
    PF_SW32(0, r_ebx);
    /* 004166e1  8b5d08                 mov ebx, dword ptr [ebp + 8] */
    r_ebx = PF_A32(0);
    /* 004166e4  8b4d10                 mov ecx, dword ptr [ebp + 0x10] */
    r_ecx = PF_A32(8);
    /* 004166e7  8d5101                 lea edx, [ecx + 1] */
    r_edx = (unsigned int)(r_ecx + 0x1u);
    /* 004166ea  c1fa04                 sar edx, 4 */
    r_edx = ((unsigned int)(((int)(r_edx)) >> 4u));
    /* 004166ed  b81d000000             mov eax, 0x1d */
    r_eax = 0x1du;
    /* 004166f2  29d0                   sub eax, edx */
    r_eax = ((r_eax - r_edx));
    /* 004166f4  83f81f                 cmp eax, 0x1f */
    pf_fa = r_eax;
    pf_fb = 0x1fu;
    pf_fres = (pf_fa - pf_fb);
    /* 004166f7  773f                   ja 0x416738 */
    if ((!(pf_fa < pf_fb) && !(pf_fres == 0u))) goto L_00416738;
    goto L_004166f9;
L_004166f9:
    ;
    /* 004166f9  8d0440                 lea eax, [eax + eax*2] */
    r_eax = (unsigned int)(r_eax + r_eax * 2u);
    /* 004166fc  c1e003                 shl eax, 3 */
    r_eax = ((r_eax << 3u));
    /* 004166ff  8b3418                 mov esi, dword ptr [eax + ebx] */
    pf_ea = (unsigned int)(r_eax + r_ebx);
    r_esi = PF_R32(pf_ea);
    /* 00416702  85f6                   test esi, esi */
    pf_fres = ((r_esi & r_esi));
    /* 00416704  7532                   jne 0x416738 */
    if (!(pf_fres == 0u)) goto L_00416738;
    goto L_00416706;
L_00416706:
    ;
    /* 00416706  8b750c                 mov esi, dword ptr [ebp + 0xc] */
    r_esi = PF_A32(4);
    /* 00416709  c1fe04                 sar esi, 4 */
    r_esi = ((unsigned int)(((int)(r_esi)) >> 4u));
    /* 0041670c  8d0403                 lea eax, [ebx + eax] */
    r_eax = (unsigned int)(r_ebx + r_eax);
    /* 0041670f  3b7004                 cmp esi, dword ptr [eax + 4] */
    pf_ea = (unsigned int)(r_eax + 0x4u);
    pf_fa = r_esi;
    pf_fb = PF_R32(pf_ea);
    pf_fres = (pf_fa - pf_fb);
    /* 00416712  7c24                   jl 0x416738 */
    if (((((pf_fres >> 31) & 1u)) != (((((pf_fa ^ pf_fb) & (pf_fa ^ pf_fres)) >> 31) & 1u)))) goto L_00416738;
    goto L_00416714;
L_00416714:
    ;
    /* 00416714  3b7008                 cmp esi, dword ptr [eax + 8] */
    pf_ea = (unsigned int)(r_eax + 0x8u);
    pf_fa = r_esi;
    pf_fb = PF_R32(pf_ea);
    pf_fres = (pf_fa - pf_fb);
    /* 00416717  7f1f                   jg 0x416738 */
    if ((!(pf_fres == 0u) && (((pf_fres >> 31) & 1u)) == (((((pf_fa ^ pf_fb) & (pf_fa ^ pf_fres)) >> 31) & 1u)))) goto L_00416738;
    goto L_00416719;
L_00416719:
    ;
    /* 00416719  8b8300030000           mov eax, dword ptr [ebx + 0x300] */
    pf_ea = (unsigned int)(r_ebx + 0x300u);
    r_eax = PF_R32(pf_ea);
    /* 0041671f  250f000080             and eax, 0x8000000f */
    pf_fres = ((r_eax & 0x8000000fu));
    r_eax = pf_fres;
    /* 00416724  781a                   js 0x416740 */
    if (((pf_fres >> 31) & 1u)) goto L_00416740;
    goto L_00416726;
L_00416726:
    ;
    /* 00416726  29c1                   sub ecx, eax */
    r_ecx = ((r_ecx - r_eax));
    /* 00416728  8d8110270000           lea eax, [ecx + 0x2710] */
    r_eax = (unsigned int)(r_ecx + 0x2710u);
    /* 0041672e  c1e204                 shl edx, 4 */
    r_edx = ((r_edx << 4u));
    /* 00416731  29d0                   sub eax, edx */
    r_eax = ((r_eax - r_edx));
    /* 00416733  5b                     pop ebx */
    r_ebx = PF_S32(0);
    /* 00416734  5e                     pop esi */
    r_esi = PF_S32(4);
    /* 00416735  c9                     leave  */
    /* frame: leave (symbolic) */
    /* 00416736  c3                     ret  */
    return (int)r_eax;
L_00416738:
    ;
    /* 00416738  31c0                   xor eax, eax */
    r_eax = ((r_eax ^ r_eax));
    /* 0041673a  5b                     pop ebx */
    r_ebx = PF_S32(0);
    /* 0041673b  5e                     pop esi */
    r_esi = PF_S32(4);
    /* 0041673c  c9                     leave  */
    /* frame: leave (symbolic) */
    /* 0041673d  c3                     ret  */
    return (int)r_eax;
L_00416740:
    ;
    /* 00416740  48                     dec eax */
    r_eax = ((r_eax - 1u));
    /* 00416741  83c8f0                 or eax, 0xfffffff0 */
    r_eax = ((r_eax | 0xfffffff0u));
    /* 00416744  40                     inc eax */
    r_eax = ((r_eax + 1u));
    /* 00416745  ebdf                   jmp 0x416726 */
    goto L_00416726;
}

