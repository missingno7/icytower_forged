/* batch14_check.c -- the COMPILED-CANDIDATE half of PROMOTIONS.md
 * batch 14's PURE / MEMORY-domain oracle:
 *
 *   qualify_hisc_table        (src/icytower/hisc.c)
 *   sort_hisc_table           (src/icytower/hisc.c)
 *   enter_hisc_table          (src/icytower/hisc.c)
 *   get_rank_id               (src/icytower/profile.c)
 *   get_rank                  (src/icytower/profile.c)
 *   hash                      (src/icytower/replay.c)
 *   calc_replay_checksum_131  (src/icytower/replay.c)
 *   calc_replay_checksum      (src/icytower/replay.c)
 *
 * Harness-only; nothing under src/ is changed or seam-ed for it
 * (win32_pilot.md SS7a).  A standalone main() rather than a lift_check.py
 * SPECS row, for the reason batches 10/12/13 already established -- and
 * for one more that is specific to this batch: three of these eight take
 * a whole 2220-byte Treplay (plus a variable-length record array, plus
 * 300 floats) as their input, which the SPECS wire protocol's fixed
 * per-function vector layout has no way to express.
 *
 * Protocol
 * --------
 * argv[1] is a BINARY vector file, a flat sequence of
 *
 *     u32 kind | u32 payload_len | payload_len bytes
 *
 * records.  Binary rather than batch13b_check.c's '|'-separated text
 * because the payloads here ARE raw structure images: encoding 2220
 * bytes of Treplay (or 180 of Thisc[5]) as text per vector, and keeping
 * a Python and a C generator in agreement about the encoding, is a
 * second thing to get wrong.  This way exactly one side generates the
 * bytes and both sides consume the same ones.
 *
 * The result for each vector is printed between "V <n>" and "E", one
 * fact per line.  Pointer-valued results are rendered as their CONTENT
 * (get_rank returns a `char *` whose address is necessarily different on
 * the two sides -- batch 13's get_version_str convention, reused).
 *
 * Build: see build_batch14.sh.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "allegro_api.h"
#include "game_types.h"
#include "game_state.h"
#include "game_funcs.h"

/* Storage the standalone world needs but state.c does not supply. */
FONT *font;
volatile char key[127];
int *allegro_errno;
BITMAP *screen;
static int errno_storage;

/* ------------------------------------------------------------------ */
/* Unreachable externals.                                              */
/*                                                                     */
/* This executable links src/icytower/replay.c and profile.c WHOLE, so  */
/* save_replay() and save_profile() come along even though the pure     */
/* oracle never calls either.  Their callees have to resolve for the    */
/* link to succeed.  Every one of them abort()s: if a future vector     */
/* ever did reach one, the run would stop loudly instead of silently    */
/* comparing something that was never executed.                        */
/*                                                                     */
/* (--gc-sections does not help here: on this PE target ld resolves     */
/* symbols before it collects unreferenced sections.)                   */
/* ------------------------------------------------------------------ */
static void unreachable(const char *who)
{
    fprintf(stderr, "batch14_check: %s reached in the PURE oracle\n", who);
    abort();
}

void log2file(const char *f, ...)            { (void)f; unreachable("log2file"); }
PACKFILE *pack_fopen(const char *a, const char *b)
{ (void)a; (void)b; unreachable("pack_fopen"); return 0; }
long pack_fwrite(const void *a, long b, PACKFILE *c)
{ (void)a; (void)b; (void)c; unreachable("pack_fwrite"); return 0; }
int pack_fclose(PACKFILE *a)                 { (void)a; unreachable("pack_fclose"); return 0; }
Treplay *load_replay(const char *a)          { (void)a; unreachable("load_replay"); return 0; }
int file_exists(const char *a, int b, int *c)
{ (void)a; (void)b; (void)c; unreachable("file_exists"); return 0; }
int get_profile_dir_for_profile(char *a, it_orig_size_t b, const char *c)
{ (void)a; (void)b; (void)c; unreachable("get_profile_dir_for_profile"); return 0; }
int generate_profile_checksum(Tprofile *a)   { (void)a; unreachable("generate_profile_checksum"); return 0; }
Tcontrol *get_controls(void)                 { unreachable("get_controls"); return 0; }
void save_control(Tcontrol *a, it_orig_FILE *b) { (void)a; (void)b; unreachable("save_control"); }
char *profile_data_page_general(Tprofile *a, char *b)
{ (void)a; (void)b; unreachable("profile_data_page_general"); return 0; }
char *profile_data_page_basic(Tprofile *a)    { (void)a; unreachable("profile_data_page_basic"); return 0; }
char *profile_data_page_advanced(Tprofile *a) { (void)a; unreachable("profile_data_page_advanced"); return 0; }
char *profile_data_page_extra(Tprofile *a)    { (void)a; unreachable("profile_data_page_extra"); return 0; }
int mkdir(const char *a)                      { (void)a; unreachable("mkdir"); return 0; }

enum {
    K_QUALIFY = 0,
    K_SORT    = 1,
    K_ENTER   = 2,
    K_RANK    = 3,
    K_HASH    = 4,
    K_CS131   = 5,
    K_CS      = 6
};

#define POSTS_BYTES (5 * 36)

static unsigned char payload[65536];

static Thisc        posts[5];
static Thisc_table  the_table;
static Tprofile     the_profile;
static Treplay      the_replay;
static Trecord      recs[4096];
static char         rank_label_store[12][16];

static void hexline(const char *tag, const void *p, int n)
{
    const unsigned char *b = (const unsigned char *)p;
    int i;
    printf("%s ", tag);
    for (i = 0; i < n; i++)
        printf("%02x", b[i]);
    printf("\n");
}

static unsigned int rd32(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8)
         | ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

int main(int argc, char **argv)
{
    FILE *vf;
    unsigned char hdr[8];
    int n = 0;
    int i;

    allegro_errno = &errno_storage;
    if (argc < 2) { fprintf(stderr, "usage: batch14_check <vectors.bin>\n"); return 2; }
    vf = fopen(argv[1], "rb");
    if (!vf) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }

    for (i = 0; i < 12; i++) {
        sprintf(rank_label_store[i], "rank%d", i);
        rankLables[i] = rank_label_store[i];
    }

    while (fread(hdr, 1, 8, vf) == 8) {
        unsigned int kind = rd32(hdr);
        unsigned int len  = rd32(hdr + 4);
        if (len > sizeof payload) { fprintf(stderr, "payload too big\n"); return 2; }
        if (fread(payload, 1, len, vf) != len) { fprintf(stderr, "short read\n"); return 2; }
        printf("V %d\n", n++);

        switch (kind) {
        case K_QUALIFY: {
            int value;
            memcpy(posts, payload, POSTS_BYTES);
            value = (int)rd32(payload + POSTS_BYTES);
            the_table.posts = posts;
            printf("ret %d\n", qualify_hisc_table(&the_table, value));
            hexline("posts", posts, POSTS_BYTES);
            break;
        }
        case K_SORT: {
            memcpy(posts, payload, POSTS_BYTES);
            the_table.posts = posts;
            sort_hisc_table(&the_table);
            hexline("posts", posts, POSTS_BYTES);
            break;
        }
        case K_ENTER: {
            int value;
            char nm[33];
            memcpy(posts, payload, POSTS_BYTES);
            value = (int)rd32(payload + POSTS_BYTES);
            memcpy(nm, payload + POSTS_BYTES + 4, 32);
            nm[32] = 0;
            the_table.posts = posts;
            enter_hisc_table(&the_table, value, nm);
            hexline("posts", posts, POSTS_BYTES);
            break;
        }
        case K_RANK: {
            int id;
            memcpy(rankFloors, payload,        48);
            memcpy(rankCombos, payload +  48,  48);
            memcpy(rankNMLs,   payload +  96,  48);
            memcpy(rankCCCs,   payload + 144,  48);
            memset(&the_profile, 0, sizeof the_profile);
            the_profile.best_floor         = (int)rd32(payload + 192);
            the_profile.best_combo         = (int)rd32(payload + 196);
            the_profile.no_combo_top_floor = (int)rd32(payload + 200);
            the_profile.ccc[0]             = (int)rd32(payload + 204);
            id = get_rank_id(&the_profile);
            printf("rank_id %d\n", id);
            printf("rank \"%s\"\n", get_rank(&the_profile));
            break;
        }
        case K_HASH:
            printf("ret %u\n", hash(rd32(payload)));
            break;
        case K_CS131:
        case K_CS: {
            int size;
            memcpy(&the_replay, payload, sizeof the_replay);
            size = the_replay.size;
            if (size < 0) size = 0;
            if (size > 4096) size = 4096;
            memcpy(recs, payload + sizeof the_replay, (size_t)size * 8);
            the_replay.data = recs;
            if (kind == K_CS131)
                printf("ret %d\n", calc_replay_checksum_131(&the_replay));
            else
                printf("ret %d\n", calc_replay_checksum(&the_replay));
            break;
        }
        default:
            fprintf(stderr, "unknown kind %u\n", kind);
            return 2;
        }
        printf("E\n");
    }
    fclose(vf);
    return 0;
}
