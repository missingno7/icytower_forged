/* GENERATED FILE -- DO NOT EDIT.
 * Produced by carrier/gen/gen_src_headers.py (which reuses carrier/gen/gen_interop.py's
 * DWARF parser and type IR -- see that file for the parsing itself) from:
 *   artifacts/dwarf_info.txt
 *   artifacts/functions.json
 * scope=game. Re-run carrier/gen/gen_src_headers.py to regenerate; do not hand-edit.
 *
 * Standalone storage for every extern declared in game_state.h. Only
 * used when src/ is built OUTSIDE the carrier (win32_pilot.md SS7a:
 * "standalone, a state.c defines the globals and the bindings header
 * is absent"). When src/ is compiled INTO the carrier, the generated
 * bindings header supplies these names as address-backed macros
 * instead and this file is not part of that build at all.
 *
 * Every global is zero-initialized (`= {0}`), matching how the OS
 * loader zero-fills the original .bss at process start; nothing here
 * claims state ownership has moved, only that a standalone build has
 * somewhere to put these bytes (src/README.md "Offline verification").
 */
#include "game_state.h"

/* ---- F:\projects\icytower\trunk\source\control.c ---- */
Tgamepad gamepad = {0};

/* ---- F:\projects\icytower\trunk\source\custom.c ---- */
RGB black = {0};
RGB pink = {0};

/* ---- F:\projects\icytower\trunk\source\fld_adspot.c ---- */
pthread_mutex_t gFLDADMutex = {0};
pthread_t gFLDADThread = {0};
int giAdCacheSize = {0};
FLDAdSpot *gpAdCache = {0};
char localFilename__fldads_get_local_cache_name[256] = {0};

/* ---- F:\projects\icytower\trunk\source\loadpng.c ---- */
int _png_compression_level = {0};
double _png_screen_gamma = {0};

/* ---- F:\projects\icytower\trunk\source\main.c ---- */
int any11 = {0};
int any12 = {0};
int any13 = {0};
int any21 = {0};
int any22 = {0};
int any23 = {0};
SAMPLE *bg_beat = {0};
SAMPLE *bg_menu = {0};
int bg_stripe_ids[5] = {0};
int blit_mode__blit_to_screen = {0};
char *category_names[15] = {0};
Tcharacter *characters = {0};
int checkMusicVoiceID = {0};
int clock_angle = {0};
int closeButtonClicked = {0};
Tcommandline cmdline = {0};
int collision_type = {0};
SAMPLE *combo_sound[10] = {0};
int count__load_character = {0};
int count__main_menu_callback = {0};
Tcontrol ctrl = {0};
Tmenu ctrl_menu[6] = {0};
int curr_char = {0};
Tcustom custom = {0};
Tmenu custom_menu[5] = {0};
int cycle_loops = {0};
DATAFILE *data = {0};
int debug = {0};
Treplay *demo = {0};
int dropped_file_is_not_a_replay = {0};
Tmenu_selection eyecandy_selection = {0};
int face__main_menu_callback = {0};
int fall_count = {0};
int fast_fast_forward = {0};
int fast_forward = {0};
Tmenu_selection floor_size_selection = {0};
Tmenu_floor_selection floors = {0};
Tgame_data *gameData = {0};
int gameMusicVoiceID = {0};
Tmenu game_menu[1] = {0};
BITMAP *gameover_bmp = {0};
int gdComboStart = {0};
int gdLastJumpDiff = {0};
Tmenu gfx_menu[5] = {0};
int got_joystick = {0};
Tmenu_selection gravity_selection = {0};
Tscroller greeting_scroller = {0};
int hasFocus = {0};
char *hints[45] = {0};
char *hisc_names[15] = {0};
Thisc_table *hisc_tables[15] = {0};
int hurry_y = {0};
int in_replay_menu = {0};
int init_ok = {0};
char init_string[7] = {0};
int is_playing_custom_game = {0};
int itrcheck = {0};
Tgd_jump_sequence jumpSequence = {0};
SAMPLE *jump_sound[3] = {0};
int lastFocus = {0};
int lastMouseB = {0};
char last_log[256] = {0};
int last_stripe_y = {0};
char logfilename__log2file[1024] = {0};
Tmenu main_menu[7] = {0};
Tmap map = {0};
Tmenu_params menu_params = {0};
SAMPLE *menu_sounds[2] = {0};
Tmenu_slider msc_volume_slider = {0};
int new_personal_best[15] = {0};
int numProfiles = {0};
int num_chars = {0};
int number__take_screenshot = {0};
Tmenu opt_menu[4] = {0};
Toptions options = {0};
const FLDAdSpot *pFLDAd = {0};
BITMAP *pFLDAdBitmap = {0};
int p__datafile_callback_slow = {0};
Tmenu_char_selection play_char = {0};
Tmenu play_menu[3] = {0};
int player_id = {0};
Tplayer *ply[1000] = {0};
BITMAP *poster = {0};
Tprofile *profile = {0};
Tmenu profile_menu[3] = {0};
Tavailable_profile *profiles = {0};
int rec_pos = {0};
int rec_seed = {0};
int recording = {0};
int rejump = {0};
char replay_directory[1024] = {0};
Tmenu replay_menu[5] = {0};
BITMAP *reward_bmp = {0};
fixed reward_scale = {0};
int reward_time = {0};
pthread_mutex_t sLogMutex__log2file = {0};
int scroll_count = {0};
int scroll_delay = {0};
Tmenu_selection scroll_speed_selection = {0};
char scroller_greetings[156] = {0};
double seed = {0};
DATAFILE *sfx = {0};
char sfx_file[512] = {0};
Tmenu snd_menu[3] = {0};
Tmenu_slider snd_volume_slider = {0};
int someCounter__play = {0};
SAMPLE *sounds[9] = {0};
SAMPLE *speaker[3] = {0};
Tparticle stars[512] = {0};
int start_speeds[6] = {0};
Tscroller summary_scroller = {0};
char summary_scroller_message[5120] = {0};
BITMAP *swap_screen = {0};
Tbeta *testers = {0};
Tbeta *the_tester = {0};
int uberChecksum = {0};
int value__draw_progress_bar = {0};
char *version_str = {0};
int window = {0};
char working_directory[1024] = {0};

/* ---- F:\projects\icytower\trunk\source\map.c ---- */
int floor_size_modifiers[5] = {0};

/* ---- F:\projects\icytower\trunk\source\menu.c ---- */
int stepIn = {0};

/* ---- F:\projects\icytower\trunk\source\player.c ---- */
double gravity_modifier[3] = {0};
double max_speed[5] = {0};

/* ---- F:\projects\icytower\trunk\source\profile.c ---- */
char *comboNames[10] = {0};
char *jcLabels[5] = {0};
int rankCCCs[12] = {0};
int rankCombos[12] = {0};
int rankFloors[12] = {0};
char *rankLables[12] = {0};
int rankNMLs[12] = {0};

/* ---- F:\projects\icytower\trunk\source\replay.c ---- */
const char REPLAY_HEADER[6] = {0};
Treplay_post itr_file_list[1024] = {0};
int num_itr_files = {0};
int sort_method = {0};

/* ---- F:\projects\icytower\trunk\source\strptime.c ---- */
const char *abb_month[13] = {0};
const char *abb_weekdays[8] = {0};
const char *ampm[3] = {0};
const char *full_month[13] = {0};
const char *full_weekdays[8] = {0};
const int tm_year_base = {0};

/* ---- F:\projects\icytower\trunk\source\timer.c ---- */
volatile int cycle_count = {0};
volatile int fps = {0};
volatile int frame_count = {0};
volatile int logic_count = {0};
volatile int lps = {0};

