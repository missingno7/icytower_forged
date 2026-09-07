/* logg_load_memory_shim.c -- recovers the ONE Allegro-addon-shaped function
 * src/icytower/assets_standalone.c calls that upstream Allegro's logg addon
 * does not actually provide: `logg_load_memory(void *pData, size_t iSize)`.
 *
 * notes/logg_load_memory.md already did the recovery work (read-only,
 * disassembly-based): the game's own logg.c CU carries 6 functions with no
 * upstream match -- 4 tiny `ov_callbacks` memfile shims
 * (logg_vf_memfile_read/seek/close/tell) plus logg_load_internal (the
 * SAMPLE-building body upstream's own logg_load() already has, factored
 * out) plus logg_load_memory itself, a thin ov_open_callbacks() wrapper.
 * This file is that recovery, built here rather than inside
 * src/icytower/assets_standalone.c because that file is a GENERATED
 * artifact of carrier/gen/gen_assets.py ("do not hand-edit" -- its own
 * header comment) which this task is not the owner of; this shim is
 * therefore a separate translation unit, linked in only for
 * src/build/asset_oracle.c's benefit, providing the missing symbol without
 * touching a single generated or carrier-owned file. A future pass that
 * owns gen_assets.py may fold this in properly (see src/icytower/ASSETS.md).
 *
 * Field values match notes/logg_load_memory.md SS5 exactly: bits=16 always,
 * stereo = (channels>1), freq = vi->rate, priority=128 always, len =
 * ov_pcm_total, loop_start=0, loop_end=len, data = len*channels*2 raw PCM
 * bytes via the same ov_read(...,0,2,0,...) loop upstream's logg_load uses.
 */
#define ICYTOWER_UPSTREAM_ALLEGRO 1
#include "allegro_types.h"   /* -> real <allegro.h>; also it_orig_size_t,
                                the address-free size type
                                assets_standalone.c casts through for this
                                same call */
#include <vorbis/vorbisfile.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    void *pData;
    int iDataSize;
    int iCursor;
} logg_memfile_context;

static size_t logg_vf_memfile_read(void *ptr, size_t size, size_t nmemb, void *datasource)
{
    logg_memfile_context *ctx = (logg_memfile_context *)datasource;
    size_t avail = (size_t)(ctx->iDataSize - ctx->iCursor);
    size_t want = size * nmemb;
    size_t take = (want > avail) ? avail : want;
    size_t members = (size > 0) ? (take / size) : 0;
    size_t bytes = members * size;
    if (bytes) {
        memcpy(ptr, (char *)ctx->pData + ctx->iCursor, bytes);
        ctx->iCursor += (int)bytes;
    }
    return members;
}

static int logg_vf_memfile_seek(void *datasource, ogg_int64_t offset, int whence)
{
    logg_memfile_context *ctx = (logg_memfile_context *)datasource;
    long target;
    switch (whence) {
        case SEEK_SET: target = (long)offset; break;
        case SEEK_CUR: target = ctx->iCursor + (long)offset; break;
        case SEEK_END: target = ctx->iDataSize + (long)offset; break;
        default: return -1;
    }
    if (target < 0 || target > ctx->iDataSize)
        return -1;
    ctx->iCursor = (int)target;
    return 0;
}

static int logg_vf_memfile_close(void *datasource)
{
    free(datasource);
    return 0;
}

static long logg_vf_memfile_tell(void *datasource)
{
    return ((logg_memfile_context *)datasource)->iCursor;
}

static SAMPLE *logg_load_internal(OggVorbis_File *vf)
{
    vorbis_info *vi = ov_info(vf, -1);
    SAMPLE *samp;
    long total_bytes, done, got;
    int bitstream;
    char *buf;

    if (!vi) {
        ov_clear(vf);
        return NULL;
    }

    samp = (SAMPLE *)malloc(sizeof(SAMPLE));
    if (!samp) {
        ov_clear(vf);
        return NULL;
    }

    samp->bits = 16;
    samp->stereo = (vi->channels > 1) ? 1 : 0;
    samp->freq = vi->rate;
    samp->priority = 128;
    samp->len = (unsigned long)ov_pcm_total(vf, -1);
    samp->loop_start = 0;
    samp->loop_end = samp->len;

    total_bytes = (long)samp->len * (samp->stereo ? 2 : 1) * 2;
    samp->data = malloc((size_t)total_bytes > 0 ? (size_t)total_bytes : 1);
    if (!samp->data) {
        free(samp);
        ov_clear(vf);
        return NULL;
    }

    buf = (char *)samp->data;
    done = 0;
    while (done < total_bytes) {
        got = ov_read(vf, buf + done, (int)(total_bytes - done), 0, 2, 0, &bitstream);
        if (got <= 0)
            break;
        done += got;
    }

    ov_clear(vf);
    return samp;
}

SAMPLE *logg_load_memory(void *pData, it_orig_size_t iSize)
{
    OggVorbis_File vf;
    ov_callbacks cb;
    logg_memfile_context *ctx = (logg_memfile_context *)malloc(sizeof(logg_memfile_context));
    if (!ctx)
        return NULL;
    ctx->pData = pData;
    ctx->iDataSize = (int)iSize;
    ctx->iCursor = 0;

    cb.read_func = logg_vf_memfile_read;
    cb.seek_func = logg_vf_memfile_seek;
    cb.close_func = logg_vf_memfile_close;
    cb.tell_func = logg_vf_memfile_tell;

    if (ov_open_callbacks(ctx, &vf, NULL, 0, cb) < 0) {
        free(ctx);
        return NULL;
    }
    return logg_load_internal(&vf);
}
