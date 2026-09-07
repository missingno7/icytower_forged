/* GENERATED FILE -- DO NOT EDIT.
 * Produced by carrier/gen/gen_src_headers.py (which reuses carrier/gen/gen_interop.py's
 * DWARF parser and type IR -- see that file for the parsing itself) from:
 *   artifacts/dwarf_info.txt
 *   artifacts/functions.json
 * scope=game. Re-run carrier/gen/gen_src_headers.py to regenerate; do not hand-edit.
 *
 * Covers every struct/union/enum/typedef the game CUs
 * (F:\projects\icytower\trunk\source\*.c) declare and that is reachable
 * from a game-CU global or function -- Tplayer, Tmap, Tfloor, and every
 * other Txxx/native game type, plus `fixed` and Allegro's own public
 * types pulled in from allegro_types.h.
 *
 * These layouts MUST stay binary-compatible with the ORIGINAL process
 * memory for as long as game state stays address-backed
 * (win32_pilot.md SS7a: "state stays address-backed, code ownership
 * migrates first") -- a `Tplayer *`/`Tmap *` this port's code touches
 * may in fact be a pointer at the original game's address, wearing this
 * struct's layout as a lens. Field order, size and padding here are
 * therefore taken from DWARF byte-for-byte, not from the compiler's own
 * alignment rules (`#pragma pack(push, 1)` plus explicit `_pad_N` filler
 * members wherever DWARF's member offsets show a gap the compiler would
 * not have left on its own), and generated fresh every run instead of
 * hand-maintained. src/icytower/game_types_check.c is the generated,
 * buildable proof that every sizeof/offsetof below still matches DWARF.
 * Once state ownership migrates to this port (a later, separate step
 * per SS7a), this header becomes free to diverge from the original
 * layout, and this comment should go with it.
 *
 * When this file is compiled INTO the carrier (src/ built with the
 * generated bindings force-included, see game_state.h and
 * src/README.md), the carrier's own type provider has already defined
 * these same names with the same layout, so the bodies below are
 * skipped rather than redefined -- see the ICYTOWER_BINDINGS_ACTIVE guard.
 */
#ifndef ICYTOWER_GAME_TYPES_H
#define ICYTOWER_GAME_TYPES_H

#include "allegro_types.h"

#ifndef ICYTOWER_BINDINGS_ACTIVE

#pragma pack(push, 1)

/* forward declarations */
struct CSVParseContext;
struct FLDAdSpot;
struct HTTPHeader;
struct HTTPResponse;
struct Tavailable_profile;
struct Tcharacter;
struct Tcommandline;
struct Tcontrol;
struct Tcustom;
struct Tfloor;
struct Tgame_data;
struct Tgamepad;
struct Tgd_combo;
struct Tgd_jump_sequence;
struct Thisc;
struct Thisc_table;
struct Tmap;
struct Tmenu;
struct Tmenu_char_selection;
struct Tmenu_floor_selection;
struct Tmenu_params;
struct Tmenu_selection;
struct Tmenu_slider;
struct Toptions;
struct Tparticle;
struct Tplayer;
struct Tprofile;
struct Trecord;
struct Treplay;
struct Treplay_post;
struct Tscroller;
struct Tstar;
struct Tstar_field;
struct internal_state;
struct it_orig_tm;
struct node;
struct png_color_16_struct;
struct png_color_8_struct;
struct png_color_struct;
struct png_info_struct;
struct png_row_info_struct;
struct png_sPLT_entry_struct;
struct png_sPLT_struct;
struct png_struct_def;
struct png_text_struct;
struct png_time_struct;
struct png_unknown_chunk_t;
struct pthread_mutex_t_;
struct ptw32_handle_t;
struct z_stream_s;

/* enumerations */
/* none in this scope */

/* struct / union bodies and typedefs, in dependency order */
struct node {
    char email[128];
    char name[128];
    char code[16];
    struct node *next;
};
struct Tcontrol {
    int use_joy;
    int key_left;
    int key_right;
    int key_up;
    int key_down;
    int key_fire;
    int key_enter;
    int key_pause;
    unsigned char flags;
    unsigned char _pad_0[3];
};
struct Tgamepad {
    int up;
    int down;
    int left;
    int right;
    int b[32];
};
struct CSVParseContext {
    unsigned char *pDoc;
    it_orig_size_t iDocSize;
    unsigned char *pLineStart;
    int iFields;
    int iFieldCapacity;
    char **pFieldPtrs;
};
struct Tcustom {
    char name[128];
    BITMAP *frame[15];
    PALETTE pal;
    SAMPLE *jump_sound[3];
    SAMPLE *falling;
    SAMPLE *edge;
    SAMPLE *yo;
    SAMPLE *wazup;
    SAMPLE *bg_music;
    MIDI *bg_midi;
    int uses_datafile;
    DATAFILE *df;
    int ok;
};
struct FLDAdSpot {
    char *pRemoteImageURL;
    char *pLocalImagePath;
    char *pVisitURL;
    float fFrequency;
};
struct HTTPHeader {
    char *pHeader;
    char *pValue;
};
typedef struct HTTPHeader HTTPHeader;
struct HTTPResponse {
    int iStatusCode;
    unsigned int iNumHeaders;
    HTTPHeader *pHeaders;
    unsigned char *pPayload;
    unsigned int iPayloadSize;
};
struct ptw32_handle_t {
    void *p;
    unsigned int x;
};
/* OPAQUE: struct pthread_mutex_t_ -- no definition found, size unknown */
typedef struct pthread_mutex_t__OPAQUE_UNSIZED { int _unrepresented; } pthread_mutex_t_;
struct Trecord {
    unsigned char key_flags;
    unsigned char _pad_0[3];
    int cycle_count;
};
typedef struct Trecord Trecord;
struct Treplay {
    char header[6];
    unsigned char _pad_0[2];
    int size;
    char name[32];
    char date[32];
    int checksum;
    int score;
    int floor;
    int combo;
    int no_combo_top_floor;
    int biggest_lost_combo;
    int ccc[5];
    int jc[5];
    int floor_shrink;
    int floor_size;
    int start_speed;
    int speed_increase;
    int gravity;
    int rejump;
    int random_seed;
    char comment[42];
    unsigned char _pad_1[2];
    int tc_posts;
    float tc_c_data[100];
    float tc_q_data[100];
    float tc_t_data[100];
    float tc_s_data[100];
    float tc_f_data[100];
    Trecord *data;
};
struct Tcommandline {
    int jumps;
    int combos;
    int sd;
    int keys;
    int tiny;
};
struct Tgd_combo {
    int start;
    int end;
    int length;
};
struct Tgd_jump_sequence {
    int start;
    int dist;
    int num;
};
typedef struct Treplay Treplay;
typedef struct Tgd_combo Tgd_combo;
typedef struct Tgd_jump_sequence Tgd_jump_sequence;
struct Tgame_data {
    Treplay *replay;
    int score;
    int floor;
    int combo;
    int no_combo_top_floor;
    int biggest_lost_combo;
    int ccc[5];
    int jc[5];
    int comboPosts;
    Tgd_combo combos[5000];
    int jumpPosts;
    Tgd_jump_sequence jumps[5000];
    int left;
    int right;
    int jump;
};
struct Thisc {
    char name[32];
    unsigned int value;
};
typedef struct Thisc Thisc;
struct Thisc_table {
    char name[32];
    Thisc *posts;
};
struct it_orig_tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};
typedef unsigned int uInt;
typedef unsigned long uLong;
typedef unsigned char Byte;
typedef Byte Bytef;
typedef void *voidpf;
typedef voidpf (__cdecl *alloc_func)(voidpf, uInt, uInt);
typedef void (__cdecl *free_func)(voidpf, voidpf);
struct z_stream_s {
    Bytef *next_in;
    uInt avail_in;
    uLong total_in;
    Bytef *next_out;
    uInt avail_out;
    uLong total_out;
    char *msg;
    struct internal_state *state;
    alloc_func zalloc;
    free_func zfree;
    voidpf opaque;
    int data_type;
    uLong adler;
    uLong reserved;
};
struct internal_state {
    int dummy;
};
typedef unsigned char png_byte;
struct png_color_struct {
    png_byte red;
    png_byte green;
    png_byte blue;
};
typedef unsigned short png_uint_16;
struct png_color_16_struct {
    png_byte index;
    unsigned char _pad_0[1];
    png_uint_16 red;
    png_uint_16 green;
    png_uint_16 blue;
    png_uint_16 gray;
};
struct png_color_8_struct {
    png_byte red;
    png_byte green;
    png_byte blue;
    png_byte gray;
    png_byte alpha;
};
struct png_sPLT_entry_struct {
    png_uint_16 red;
    png_uint_16 green;
    png_uint_16 blue;
    png_uint_16 alpha;
    png_uint_16 frequency;
};
typedef long png_int_32;
typedef char *png_charp;
typedef struct png_sPLT_entry_struct png_sPLT_entry;
typedef png_sPLT_entry *png_sPLT_entryp;
struct png_sPLT_struct {
    png_charp name;
    png_byte depth;
    unsigned char _pad_0[3];
    png_sPLT_entryp entries;
    png_int_32 nentries;
};
typedef it_orig_size_t png_size_t;
struct png_text_struct {
    int compression;
    png_charp key;
    png_charp text;
    png_size_t text_length;
};
struct png_time_struct {
    png_uint_16 year;
    png_byte month;
    png_byte day;
    png_byte hour;
    png_byte minute;
    png_byte second;
    unsigned char _pad_0[1];
};
struct png_unknown_chunk_t {
    png_byte name[5];
    unsigned char _pad_0[3];
    png_byte *data;
    png_size_t size;
    png_byte location;
    unsigned char _pad_1[3];
};
typedef unsigned long png_uint_32;
typedef png_int_32 png_fixed_point;
typedef png_byte *png_bytep;
typedef png_uint_16 *png_uint_16p;
typedef png_byte **png_bytepp;
typedef char **png_charpp;
typedef struct png_color_struct png_color;
typedef png_color *png_colorp;
typedef struct png_color_16_struct png_color_16;
typedef struct png_color_8_struct png_color_8;
typedef struct png_sPLT_struct png_sPLT_t;
typedef png_sPLT_t *png_sPLT_tp;
typedef struct png_text_struct png_text;
typedef png_text *png_textp;
typedef struct png_time_struct png_time;
typedef struct png_unknown_chunk_t png_unknown_chunk;
typedef png_unknown_chunk *png_unknown_chunkp;
struct png_info_struct {
    png_uint_32 width;
    png_uint_32 height;
    png_uint_32 valid;
    png_uint_32 rowbytes;
    png_colorp palette;
    png_uint_16 num_palette;
    png_uint_16 num_trans;
    png_byte bit_depth;
    png_byte color_type;
    png_byte compression_type;
    png_byte filter_type;
    png_byte interlace_type;
    png_byte channels;
    png_byte pixel_depth;
    png_byte spare_byte;
    png_byte signature[8];
    float gamma;
    png_byte srgb_intent;
    unsigned char _pad_0[3];
    int num_text;
    int max_text;
    png_textp text;
    png_time mod_time;
    png_color_8 sig_bit;
    unsigned char _pad_1[3];
    png_bytep trans;
    png_color_16 trans_values;
    png_color_16 background;
    png_int_32 x_offset;
    png_int_32 y_offset;
    png_byte offset_unit_type;
    unsigned char _pad_2[3];
    png_uint_32 x_pixels_per_unit;
    png_uint_32 y_pixels_per_unit;
    png_byte phys_unit_type;
    unsigned char _pad_3[3];
    png_uint_16p hist;
    float x_white;
    float y_white;
    float x_red;
    float y_red;
    float x_green;
    float y_green;
    float x_blue;
    float y_blue;
    png_charp pcal_purpose;
    png_int_32 pcal_X0;
    png_int_32 pcal_X1;
    png_charp pcal_units;
    png_charpp pcal_params;
    png_byte pcal_type;
    png_byte pcal_nparams;
    unsigned char _pad_4[2];
    png_uint_32 free_me;
    png_unknown_chunkp unknown_chunks;
    png_size_t unknown_chunks_num;
    png_charp iccp_name;
    png_charp iccp_profile;
    png_uint_32 iccp_proflen;
    png_byte iccp_compression;
    unsigned char _pad_5[3];
    png_sPLT_tp splt_palettes;
    png_uint_32 splt_palettes_num;
    png_byte scal_unit;
    unsigned char _pad_6[3];
    double scal_pixel_width;
    double scal_pixel_height;
    png_charp scal_s_width;
    png_charp scal_s_height;
    png_bytepp row_pointers;
    png_fixed_point int_gamma;
    png_fixed_point int_x_white;
    png_fixed_point int_y_white;
    png_fixed_point int_x_red;
    png_fixed_point int_y_red;
    png_fixed_point int_x_green;
    png_fixed_point int_y_green;
    png_fixed_point int_x_blue;
    png_fixed_point int_y_blue;
};
struct png_row_info_struct {
    png_uint_32 width;
    png_uint_32 rowbytes;
    png_byte color_type;
    png_byte bit_depth;
    png_byte channels;
    png_byte pixel_depth;
};
typedef struct z_stream_s z_stream;
typedef int it_orig_jmp_buf[16];
typedef void *png_voidp;
typedef png_uint_16 **png_uint_16pp;
typedef struct png_row_info_struct png_row_info;
typedef const char *png_const_charp;
typedef struct png_struct_def png_struct;
typedef png_struct *png_structp;
typedef void (__cdecl *png_error_ptr)(png_structp, png_const_charp);
typedef void (__cdecl *png_rw_ptr)(png_structp, png_bytep, png_size_t);
typedef void (__cdecl *png_flush_ptr)(png_structp);
typedef void (__cdecl *png_read_status_ptr)(png_structp, png_uint_32, int);
typedef void (__cdecl *png_write_status_ptr)(png_structp, png_uint_32, int);
typedef struct png_info_struct png_info;
typedef png_info *png_infop;
typedef void (__cdecl *png_progressive_info_ptr)(png_structp, png_infop);
typedef void (__cdecl *png_progressive_end_ptr)(png_structp, png_infop);
typedef void (__cdecl *png_progressive_row_ptr)(png_structp, png_bytep, png_uint_32, int);
typedef png_row_info *png_row_infop;
typedef void (__cdecl *png_user_transform_ptr)(png_structp, png_row_infop, png_bytep);
typedef int (__cdecl *png_user_chunk_ptr)(png_structp, png_unknown_chunkp);
typedef png_voidp (__cdecl *png_malloc_ptr)(png_structp, png_size_t);
typedef void (__cdecl *png_free_ptr)(png_structp, png_voidp);
struct png_struct_def {
    it_orig_jmp_buf jmpbuf;
    png_error_ptr error_fn;
    png_error_ptr warning_fn;
    png_voidp error_ptr;
    png_rw_ptr write_data_fn;
    png_rw_ptr read_data_fn;
    png_voidp io_ptr;
    png_user_transform_ptr read_user_transform_fn;
    png_user_transform_ptr write_user_transform_fn;
    png_voidp user_transform_ptr;
    png_byte user_transform_depth;
    png_byte user_transform_channels;
    unsigned char _pad_0[2];
    png_uint_32 mode;
    png_uint_32 flags;
    png_uint_32 transformations;
    z_stream zstream;
    png_bytep zbuf;
    png_size_t zbuf_size;
    int zlib_level;
    int zlib_method;
    int zlib_window_bits;
    int zlib_mem_level;
    int zlib_strategy;
    png_uint_32 width;
    png_uint_32 height;
    png_uint_32 num_rows;
    png_uint_32 usr_width;
    png_uint_32 rowbytes;
    png_uint_32 irowbytes;
    png_uint_32 iwidth;
    png_uint_32 row_number;
    png_bytep prev_row;
    png_bytep row_buf;
    png_bytep sub_row;
    png_bytep up_row;
    png_bytep avg_row;
    png_bytep paeth_row;
    png_row_info row_info;
    png_uint_32 idat_size;
    png_uint_32 crc;
    png_colorp palette;
    png_uint_16 num_palette;
    png_uint_16 num_trans;
    png_byte chunk_name[5];
    png_byte compression;
    png_byte filter;
    png_byte interlaced;
    png_byte pass;
    png_byte do_filter;
    png_byte color_type;
    png_byte bit_depth;
    png_byte usr_bit_depth;
    png_byte pixel_depth;
    png_byte channels;
    png_byte usr_channels;
    png_byte sig_bytes;
    unsigned char _pad_1[1];
    png_uint_16 filler;
    png_byte background_gamma_type;
    unsigned char _pad_2[3];
    float background_gamma;
    png_color_16 background;
    png_color_16 background_1;
    png_flush_ptr output_flush_fn;
    png_uint_32 flush_dist;
    png_uint_32 flush_rows;
    int gamma_shift;
    float gamma;
    float screen_gamma;
    png_bytep gamma_table;
    png_bytep gamma_from_1;
    png_bytep gamma_to_1;
    png_uint_16pp gamma_16_table;
    png_uint_16pp gamma_16_from_1;
    png_uint_16pp gamma_16_to_1;
    png_color_8 sig_bit;
    png_color_8 shift;
    unsigned char _pad_3[2];
    png_bytep trans;
    png_color_16 trans_values;
    unsigned char _pad_4[2];
    png_read_status_ptr read_row_fn;
    png_write_status_ptr write_row_fn;
    png_progressive_info_ptr info_fn;
    png_progressive_row_ptr row_fn;
    png_progressive_end_ptr end_fn;
    png_bytep save_buffer_ptr;
    png_bytep save_buffer;
    png_bytep current_buffer_ptr;
    png_bytep current_buffer;
    png_uint_32 push_length;
    png_uint_32 skip_length;
    png_size_t save_buffer_size;
    png_size_t save_buffer_max;
    png_size_t buffer_size;
    png_size_t current_buffer_size;
    int process_mode;
    int cur_palette;
    png_size_t current_text_size;
    png_size_t current_text_left;
    png_charp current_text;
    png_charp current_text_ptr;
    png_bytep palette_lookup;
    png_bytep dither_index;
    png_uint_16p hist;
    png_byte heuristic_method;
    png_byte num_prev_filters;
    unsigned char _pad_5[2];
    png_bytep prev_filters;
    png_uint_16p filter_weights;
    png_uint_16p inv_filter_weights;
    png_uint_16p filter_costs;
    png_uint_16p inv_filter_costs;
    png_charp time_buffer;
    png_uint_32 free_me;
    png_voidp user_chunk_ptr;
    png_user_chunk_ptr read_user_chunk_fn;
    int num_chunk_list;
    png_bytep chunk_list;
    png_byte rgb_to_gray_status;
    unsigned char _pad_6[1];
    png_uint_16 rgb_to_gray_red_coeff;
    png_uint_16 rgb_to_gray_green_coeff;
    png_uint_16 rgb_to_gray_blue_coeff;
    png_uint_32 mng_features_permitted;
    png_fixed_point int_gamma;
    png_byte filter_type;
    png_byte mmx_bitdepth_threshold;
    unsigned char _pad_7[2];
    png_uint_32 mmx_rowbytes_threshold;
    png_uint_32 asm_flags;
    png_voidp mem_ptr;
    png_malloc_ptr malloc_fn;
    png_free_ptr free_fn;
    png_bytep big_row_buf;
    png_bytep dither_sort;
    png_bytep index_to_palette;
    png_bytep palette_to_index;
    png_byte compression_type;
    unsigned char _pad_8[3];
    png_uint_32 user_width_max;
    png_uint_32 user_height_max;
    png_unknown_chunk unknown_chunk;
    png_uint_32 old_big_row_buf_size;
    png_uint_32 old_prev_row_size;
    png_charp chunkdata;
};
struct Tprofile {
    char header[6];
    char handle[32];
    unsigned char _pad_0[2];
    int checksum;
    int games_played;
    int custom_games_played;
    int games_quit;
    int seconds_spent_playing;
    int total_floors;
    int total_score;
    int total_combos;
    int total_combo_floors;
    int best_floor;
    int best_combo;
    int best_score;
    int no_combo_top_floor;
    int biggest_lost_combo;
    int cccNum[5];
    int cccTotal[5];
    int ccc[5];
    int jc[5];
    int rewards[10];
    int total_jumps;
    char best_replay_names[32][32];
    int flash;
    int jump_hold;
    char last_avatar[64];
    int start_floor;
    int msc_volume;
    int snd_volume;
    char creationDate[16];
    char saveDate[16];
};
struct Tavailable_profile {
    char handle[32];
};
struct Toptions {
    int flash;
    int checksum;
    int jump_hold;
    int full_screen;
    int floor_shrink;
    int floor_size;
    int start_speed;
    int speed_increase;
    int gravity;
    int msc_volume;
    int snd_volume;
    int sort_method;
    char updateDate[16];
    char posterDate[16];
    char posterUrl[256];
    char posterSrc[256];
    int posterSize;
    char lastProfile[32];
    int timesStarted;
};
typedef struct Tcontrol Tcontrol;
struct Tmenu_params {
    FONT *font;
    int font_height;
    Tcontrol ctrl;
    BITMAP *bullet;
    int pos;
    DATAFILE *data;
    int fo;
};
struct Tmenu {
    char caption[128];
    int return_select;
    int return_left;
    int return_right;
    int flags;
    void *data;
};
struct Tmenu_slider {
    int value;
    int min;
    int max;
    int step;
};
struct Tmenu_selection {
    int value;
    int size;
    char *caption[32];
};
struct Tmenu_floor_selection {
    int value;
    int max;
};
struct Tmenu_char_selection {
    int value;
    int max;
    BITMAP *bmp;
    PALETTE pal;
};
struct Tscroller {
    int horizontal;
    char *text;
    FONT *fnt;
    int font_height;
    int width;
    int height;
    int offset;
    int rows;
    int length;
    char *lines[512];
};
struct Tfloor {
    int empty;
    int start_tile;
    int end_tile;
    int level;
    int sign;
    int tiles;
};
typedef struct Tfloor Tfloor;
struct Tmap {
    Tfloor room[32];
    int offset;
};
struct Tplayer {
    double x;
    double y;
    double sx;
    double sy;
    double max_s;
    int level;
    int score;
    int best_combo;
    int status;
    int jump_key;
    int frame;
    int in_combo;
    int acc_level;
    int acc_jumps;
    int dead;
    int rotate;
    fixed angle;
    int edge;
    int edge_drawn;
    int bounce;
    int shake;
    int latest_combo;
    int show_combo;
    int no_combo_top_floor;
    int biggest_lost_combo;
    int ccc[5];
    int jcTop[5];
    int jc[5];
    unsigned char _pad_0[4];
};
struct Tparticle {
    int intensity;
    fixed x;
    fixed y;
    fixed sx;
    fixed sy;
    int color;
};
struct Tcharacter {
    char filename[1024];
    BITMAP *bmp;
    int ok;
    char name[128];
    int uses_datafile;
    PALETTE pal;
};
struct Treplay_post {
    char *full_path;
    char directory;
    char parent;
    unsigned char _pad_0[2];
    int version;
    int score;
    int floor;
    int combo;
};
struct Tstar {
    double x;
    double y;
    int z;
    unsigned char _pad_0[4];
};
typedef struct Tstar Tstar;
struct Tstar_field {
    int clear_color;
    int stars;
    int width;
    int height;
    int depth;
    int col1;
    int col_step;
    unsigned char _pad_0[4];
    Tstar star[1024];
};
typedef struct node Tbeta;
typedef struct Tgamepad Tgamepad;
typedef struct CSVParseContext CSVParseContext;
typedef struct Tcustom Tcustom;
typedef struct FLDAdSpot FLDAdSpot;
typedef struct HTTPResponse HTTPResponse;
typedef struct ptw32_handle_t ptw32_handle_t;
typedef ptw32_handle_t pthread_t;
typedef struct pthread_mutex_t_ *pthread_mutex_t;
typedef struct Tcommandline Tcommandline;
typedef struct Tgame_data Tgame_data;
typedef struct Thisc_table Thisc_table;
typedef struct Tprofile Tprofile;
typedef struct Tavailable_profile Tavailable_profile;
typedef struct Toptions Toptions;
typedef struct Tmenu_params Tmenu_params;
typedef struct Tmenu Tmenu;
typedef struct Tmenu_slider Tmenu_slider;
typedef struct Tmenu_selection Tmenu_selection;
typedef struct Tmenu_floor_selection Tmenu_floor_selection;
typedef struct Tmenu_char_selection Tmenu_char_selection;
typedef struct Tscroller Tscroller;
typedef struct Tmap Tmap;
typedef struct Tplayer Tplayer;
typedef struct Tparticle Tparticle;
typedef struct Tcharacter Tcharacter;
typedef struct Treplay_post Treplay_post;
typedef struct Tstar_field Tstar_field;

#pragma pack(pop)

#endif /* !ICYTOWER_BINDINGS_ACTIVE */

#endif /* ICYTOWER_GAME_TYPES_H */
