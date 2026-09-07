/* logg_load_memory_shim.h -- forward-declares the one function
 * src/icytower/assets_standalone.c calls that real upstream Allegro's
 * <logg.h> does not provide (see logg_load_memory_shim.c's own header
 * comment and notes/logg_load_memory.md for the full story).
 *
 * assets_standalone.c and src/icytower/allegro_api.h are both GENERATED
 * files this task does not own the generator for; force-including this
 * header ahead of assets_standalone.c's compile (Makefile.standalone's
 * dedicated rule for that one object file, `-include`) supplies the missing
 * declaration without hand-editing either generated file.
 */
#ifndef LOGG_LOAD_MEMORY_SHIM_H
#define LOGG_LOAD_MEMORY_SHIM_H

#include "allegro_types.h"   /* SAMPLE, it_orig_size_t */

SAMPLE *logg_load_memory(void *pData, it_orig_size_t iSize);

#endif /* LOGG_LOAD_MEMORY_SHIM_H */
