/* dirty_example.c -- fixture: must FAIL scripts/check_native_layer.py,
 * on purpose, one violation of each kind the gate checks for.
 */
#include "carrier/gen/it_types.h"   /* carrier/generated-interop include */
#include "pf_bindings.h"            /* carrier/generated-interop include */

int *cycle_counter_ptr = (int *)0x506938;   /* guest address literal (hex) */
int also_cycle_counter = 5269816;           /* same VA, decimal */

int PF_leaked_helper(void)          /* carrier-reserved identifier prefix */
{
    int lifted_tmp = 0;             /* carrier-reserved identifier prefix */
    __asm { nop }                   /* inline asm */
    return lifted_tmp;
}
