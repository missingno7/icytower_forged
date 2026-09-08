/* play_check_stubs.c -- inert definitions for everything play() names but
 * that play_check.c's three regions never reach.  PROMOTIONS.md batch 12.
 *
 * A translation unit that contains play() has to LINK against all ~45 of
 * its callees even when the checker only calls three call-free static
 * helpers out of it, so these exist purely to satisfy the linker.  None
 * of them is reachable from R1/R2/R3; if one ever were, it would abort
 * rather than silently return a made-up value.
 */
#include <stdio.h>
#include <stdlib.h>

#include "allegro_api.h"
#include "game_types.h"
#include "game_state.h"
#include "game_funcs.h"
#include "assets.h"

static void unreachable(const char *who)
{
    fprintf(stderr, "play_check: %s reached -- not part of R1/R2/R3\n", who);
    exit(3);
}

#define STUB_V(name, args)  void name args { unreachable(#name); }
#define STUB_I(name, args)  int  name args { unreachable(#name); return 0; }
#define STUB_P(t, name, args) t name args { unreachable(#name); return 0; }

/* ---- game ---------------------------------------------------------- */
STUB_V(add_combo, (Tgame_data *a, Tgd_combo *b))
STUB_V(add_floor, (Tmap *a))
STUB_V(add_jump_sequence, (Tgame_data *a, Tgd_jump_sequence *b))
STUB_V(blit_to_screen, (BITMAP *a))
STUB_I(calc_replay_checksum, (Treplay *a))
STUB_I(create_particle, (Tparticle *a, int b, int c))
STUB_V(destroy_replay, (Treplay *a))
STUB_I(do_replay_menu, (void))
STUB_V(draw_frame, (BITMAP *a))
STUB_V(draw_results, (BITMAP *a, BITMAP *b, int c, int *d, int *e, int f))
STUB_I(draw_scroller, (Tscroller *a, BITMAP *b, int c, int d, int e))
STUB_V(enter_hisc_table, (Thisc_table *a, int b, char *c))
STUB_V(fadeIn, (BITMAP *a, int b))
STUB_V(fadeOut, (int a))
STUB_P(char *, getGameDataXML, (Tgame_data *a))
STUB_I(get_level, (Tmap *a, int b))
STUB_I(get_rank_id, (Tprofile *a))
STUB_V(handle_player_collision_combo, (int a, int b))
STUB_V(handle_player_collision_old, (int a, int b))
STUB_V(handle_player_collision_original, (int a, int b))
STUB_V(handle_player_collision_vector, (int a, int b))
STUB_V(handle_player_collision_vector_2, (int a, int b))
STUB_V(handle_player_input, (Tcontrol *a))
STUB_V(init_scroller, (Tscroller *a, FONT *b, char *c, int d, int e, int f))
STUB_I(is_any, (Tcontrol *a))
STUB_I(is_fire, (Tcontrol *a))
STUB_I(is_left, (Tcontrol *a))
STUB_I(is_pause, (Tcontrol *a))
STUB_I(is_right, (Tcontrol *a))
STUB_P(Treplay *, load_replay, (const char *a))
STUB_V(myDeleteFile, (char *a, char *b))
STUB_I(my_alert, (char *a, char *b, int c, int d))
STUB_I(new_rand, (void))
STUB_V(play_sound, (SAMPLE *a, int b, int c))
STUB_V(poll_control, (Tcontrol *a, int b))
STUB_I(qualify_hisc_table, (Thisc_table *a, int b))
STUB_V(restart_scroller, (Tscroller *a))
STUB_V(save_config, (void))
STUB_I(save_profile, (Tprofile *a))
STUB_I(save_replay, (const char *a, const char *b, Treplay *c, int d, int e))
STUB_V(scroll_scroller, (Tscroller *a, int b))
STUB_V(sort_hisc_table, (Thisc_table *a))
STUB_V(startGameMusic, (void))
STUB_V(syncProfileFromOptions, (void))
STUB_I(start_reward, (int a))
STUB_V(stopGameMusic, (void))
STUB_V(take_screenshot, (BITMAP *a))
STUB_V(update_frame, (void))
STUB_V(update_particle, (Tparticle *a))
STUB_V(update_player, (Tplayer *a))

void log2file(const char *fmt, ...) { (void)fmt; unreachable("log2file"); }

/* ---- assets (ASSETS.md's seam) -------------------------------------- */
BITMAP *asset_bitmap(asset_id id) { (void)id; unreachable("asset_bitmap"); return 0; }
FONT *asset_font(asset_id id) { (void)id; unreachable("asset_font"); return 0; }

/* ---- Allegro --------------------------------------------------------- */
void allegro_message(const char *fmt, ...) { (void)fmt; unreachable("allegro_message"); }
STUB_V(blit, (BITMAP *a, BITMAP *b, int c, int d, int e, int f, int g, int h))
STUB_V(clear_bitmap, (BITMAP *a))
STUB_V(clear_keybuf, (void))
STUB_V(drawing_mode, (int a, BITMAP *b, int c, int d))
STUB_I(file_exists, (const char *a, int b, int *c))
STUB_I(keypressed, (void))
STUB_I(makecol, (int a, int b, int c))
STUB_I(play_sample, (const SAMPLE *a, int b, int c, int d, int e))
STUB_I(readkey, (void))
STUB_V(rest, (unsigned int a))
STUB_V(set_trans_blender, (int a, int b, int c, int d))
STUB_V(solid_mode, (void))
STUB_V(stop_sample, (const SAMPLE *a))
STUB_V(textout_centre_ex, (BITMAP *a, const FONT *b, const char *c, int d, int e, int f, int g))
STUB_V(textout_ex, (BITMAP *a, const FONT *b, const char *c, int d, int e, int f, int g))
STUB_I(voice_get_position, (int a))
STUB_V(voice_stop, (int a))

/* ---- Win32 / CRT ----------------------------------------------------- */
int mkdir(const char *path) { (void)path; unreachable("mkdir"); return 0; }
int stricmp(const char *a, const char *b) { (void)a; (void)b; unreachable("stricmp"); return 0; }
