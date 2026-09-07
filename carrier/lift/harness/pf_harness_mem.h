/* Offline-harness memory seam.
 *
 * Force-included (cl /FIpf_harness_mem.h) ahead of pf_rt.h so that every
 * absolute address in the lifted code is redirected into an in-process copy
 * of the mapped PE image instead of the real 0x400000 range.  This is the
 * cheap route named in the task: no VirtualAlloc-at-0x400000 games, no second
 * emulator for the lifted side.  The generated .c files are byte-identical in
 * both configurations -- only this one macro differs.
 *
 * Pointer ARGUMENTS are guest VAs too (e.g. 0x790000), never host pointers:
 * the lifted code never dereferences a C pointer, it only funnels the value
 * through PF_MEM(), so the whole address space stays consistent.
 */
#ifndef PF_HARNESS_MEM_H
#define PF_HARNESS_MEM_H

#define PF_GUEST_BASE 0x400000u
#define PF_GUEST_SIZE 0x400000u          /* 0x400000 .. 0x800000 */

extern unsigned char *pf_guest;          /* points at guest VA 0x400000 */

#define PF_MEM(a) ((void *)(pf_guest + ((unsigned int)(a) - PF_GUEST_BASE)))

#endif
