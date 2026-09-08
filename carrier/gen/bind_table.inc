/* GENERATED FILE -- DO NOT EDIT.
 * Produced by carrier/gen/gen_bind_table.py from:
 *   - carrier/gen/scan_src_defs.py's scan of src/icytower/*.c (which functions)
 *   - carrier/gen/interop_index.json + it_funcs_table.inc (VA/size/prototype)
 *   - carrier/gen/fn_domains.json (comparison-domain regions, hand-curated data)
 *   - carrier/build.cmd's own link line (which lifted_/native_ forms are linked in)
 * Included by carrier/src/bind.cpp, which defines RegionKind, Region and FnDesc
 * above the #include and consumes kFns[]/kNumFns below it. Re-run by build.cmd
 * before every compile; never hand-edit -- edit fn_domains.json or the sources
 * above instead.
 */

extern "C" {
    void __cdecl add_combo(void*, void*);
    void __cdecl add_jump_sequence(void*, void*);
    void __cdecl blit_to_screen(void*);
    void __cdecl handle_player_collision_old(int, int);
    void __cdecl handle_player_collision_vector(int, int);
    void __cdecl handle_player_collision_vector_2(int, int);
    void __cdecl handle_player_collision_combo(int, int);
    void __cdecl save_config(void);
    void __cdecl myDeleteFile(void*, void*);
    void* __cdecl get_gamepad(void);
    int __cdecl is_up(void*);
    int __cdecl is_down(void*);
    int __cdecl is_left(void*);
    int __cdecl is_right(void*);
    int __cdecl is_fire(void*);
    int __cdecl is_pause(void*);
    int __cdecl is_enter(void*);
    int __cdecl is_any(void*);
    void __cdecl set_control(void*, int, int, int, int, int);
    void __cdecl init_control(void*);
    int __cdecl check_control_key(void*, int);
    void __cdecl destroy_game_data(void*);
    int __cdecl draw_buffer(void*, void*, int, int);
    void __cdecl draw_frame(void*);
    void __cdecl draw_reward(void*);
    int __cdecl draw_scroller(void*, void*, int, int, int);
    void __cdecl draw_star_field(void*, void*, int, int);
    void __cdecl fadeOut(int);
    void __cdecl fadeIn(void*, int);
    void __cdecl handle_player_collision_original(int, int);
    void __cdecl handle_player_input(void*);
    int __cdecl qualify_hisc_table(void*, int);
    void __cdecl sort_hisc_table(void*);
    void __cdecl enter_hisc_table(void*, int, void*);
    int __cdecl is_solid(void*, int, int);
    int __cdecl lifted_is_solid(void*, int, int);
    int __cdecl native_is_solid(void*, int, int);
    int __cdecl jump_player(void*, int);
    int __cdecl lifted_jump_player(void*, int);
    int __cdecl line_intersect(int, int, int, int, int, int, int, int, void*, void*);
    void __cdecl log2file(void*, int);
    void* __cdecl get_demo(void);
    void* __cdecl get_controls(void);
    void __cdecl switchedFromProgram(void);
    void __cdecl switchedToProgram(void);
    void __cdecl clickedCloseButton(void);
    void* __cdecl get_version_str(void);
    void __cdecl syncProfileFromOptions(void);
    void __cdecl getFloorData(void*, int, void*, void*, void*);
    void __cdecl reset_map(void*);
    int __cdecl get_level(void*, int);
    void __cdecl add_floor(void*);
    int __cdecl new_rand(void);
    int __cdecl ok_to_play(void);
    void __cdecl reset_particles(void*);
    void __cdecl update_particle(void*);
    int __cdecl create_particle(void*, int, int);
    int __cdecl play(void);
    void __cdecl play_jump_sound(void*);
    void __cdecl poll_control(void*, int);
    int __cdecl get_rank_id(void*);
    void* __cdecl get_rank(void*);
    int __cdecl save_profile(void*);
    int __cdecl hash(int);
    int __cdecl calc_replay_checksum_131(void*);
    int __cdecl calc_replay_checksum(void*);
    void __cdecl destroy_replay(void*);
    void* __cdecl create_replay(int);
    void* __cdecl load_replay(void*);
    int __cdecl save_replay(void*, void*, void*, int, int);
    void __cdecl reset_player(void*);
    void __cdecl take_screenshot(void*);
    void __cdecl scroll_scroller(void*, int);
    void __cdecl restart_scroller(void*);
    void __cdecl init_scroller(void*, void*, void*, int, int, int);
    void __cdecl play_sound(void*, int, int);
    void __cdecl startGameMusic(void);
    void __cdecl stopGameMusic(void);
    int __cdecl start_reward(int);
    void __cdecl cycle_counter(void);
    void __cdecl fps_counter(void);
    void __cdecl update_frame(void);
    void __cdecl lifted_update_frame(void);
    void __cdecl native_update_frame(void);
    void __cdecl update_player(void*);
}

static const Region regions_add_combo[] = {
    { RK_COUNTER_INDEXED, 0, 0, 64u, 12u, 68u, 5000u }, // gd->comboPosts, gd->combos[]
};
static const Region regions_add_jump_sequence[] = {
    { RK_COUNTER_INDEXED, 0, 0, 60068u, 12u, 60072u, 5000u }, // gd->jumpPosts, gd->jumps[]
};
static const Region regions_is_up[] = {
    { RK_ARG, 0, 0, 0, 36u, 0, 0 }, // Tcontrol *c
};
static const Region regions_is_down[] = {
    { RK_ARG, 0, 0, 0, 36u, 0, 0 }, // Tcontrol *c
};
static const Region regions_is_left[] = {
    { RK_ARG, 0, 0, 0, 36u, 0, 0 }, // Tcontrol *c
};
static const Region regions_is_right[] = {
    { RK_ARG, 0, 0, 0, 36u, 0, 0 }, // Tcontrol *c
};
static const Region regions_is_fire[] = {
    { RK_ARG, 0, 0, 0, 36u, 0, 0 }, // Tcontrol *c
};
static const Region regions_is_pause[] = {
    { RK_ARG, 0, 0, 0, 36u, 0, 0 }, // Tcontrol *c
};
static const Region regions_is_enter[] = {
    { RK_ARG, 0, 0, 0, 36u, 0, 0 }, // Tcontrol *c
};
static const Region regions_is_any[] = {
    { RK_ARG, 0, 0, 0, 36u, 0, 0 }, // Tcontrol *c
};
static const Region regions_set_control[] = {
    { RK_ARG, 0, 0, 0, 36u, 0, 0 }, // Tcontrol *c
};
static const Region regions_init_control[] = {
    { RK_ARG, 0, 0, 0, 36u, 0, 0 }, // Tcontrol *c
};
static const Region regions_check_control_key[] = {
    { RK_ARG, 0, 0, 0, 36u, 0, 0 }, // Tcontrol *c
};
static const Region regions_is_solid[] = {
    { RK_ARG, 0, 0, 0, 772u, 0, 0 }, // Tmap *m
};
static const Region regions_jump_player[] = {
    { RK_ARG, 0, 0, 0, 184u, 0, 0 }, // Tplayer *p
};
static const Region regions_line_intersect[] = {
    { RK_ARG, 0, 8, 0, 4u, 0, 0 }, // *px_out
    { RK_ARG, 0, 9, 0, 4u, 0, 0 }, // *py_out
};
static const Region regions_get_demo[] = {
    { RK_GLOBAL, 0x4dd250, 0, 0, 4u, 0, 0 }, // demo
};
static const Region regions_switchedFromProgram[] = {
    { RK_GLOBAL, 0x4bc020, 0, 0, 4u, 0, 0 }, // hasFocus
};
static const Region regions_switchedToProgram[] = {
    { RK_GLOBAL, 0x4bc020, 0, 0, 4u, 0, 0 }, // hasFocus
};
static const Region regions_clickedCloseButton[] = {
    { RK_GLOBAL, 0x4dd264, 0, 0, 4u, 0, 0 }, // closeButtonClicked
};
static const Region regions_getFloorData[] = {
    { RK_ARG, 0, 0, 0, 772u, 0, 0 }, // Tmap *m
    { RK_ARG, 0, 2, 0, 4u, 0, 0 }, // *fy
    { RK_ARG, 0, 3, 0, 4u, 0, 0 }, // *fx1
    { RK_ARG, 0, 4, 0, 4u, 0, 0 }, // *fx2
};
static const Region regions_reset_map[] = {
    { RK_ARG, 0, 0, 0, 772u, 0, 0 }, // Tmap *m
};
static const Region regions_get_level[] = {
    { RK_ARG, 0, 0, 0, 772u, 0, 0 }, // Tmap *m
};
static const Region regions_add_floor[] = {
    { RK_ARG, 0, 0, 0, 772u, 0, 0 }, // Tmap *m
};
static const Region regions_new_rand[] = {
    { RK_GLOBAL, 0x4ff108, 0, 0, 8u, 0, 0 }, // seed
};
static const Region regions_reset_particles[] = {
    { RK_ARG, 0, 0, 0, 12288u, 0, 0 }, // Tparticle p[512]
};
static const Region regions_update_particle[] = {
    { RK_ARG, 0, 0, 0, 24u, 0, 0 }, // Tparticle *p
    { RK_GLOBAL, 0x4ff108, 0, 0, 8u, 0, 0 }, // seed
};
static const Region regions_create_particle[] = {
    { RK_ARG, 0, 0, 0, 12288u, 0, 0 }, // Tparticle p[512]
    { RK_GLOBAL, 0x4ff108, 0, 0, 8u, 0, 0 }, // seed
};
static const Region regions_reset_player[] = {
    { RK_ARG, 0, 0, 0, 184u, 0, 0 }, // Tplayer *p
};
static const Region regions_scroll_scroller[] = {
    { RK_ARG_OFFSET, 0, 0, 24u, 4u, 0, 0 }, // sc->offset
};
static const Region regions_restart_scroller[] = {
    { RK_ARG_OFFSET, 0, 0, 24u, 4u, 0, 0 }, // sc->offset
};
static const Region regions_cycle_counter[] = {
    { RK_GLOBAL, 0x506938, 0, 0, 4u, 0, 0 }, // cycle_count
};
static const Region regions_fps_counter[] = {
    { RK_GLOBAL, 0x506948, 0, 0, 4u, 0, 0 }, // fps
    { RK_GLOBAL, 0x506978, 0, 0, 4u, 0, 0 }, // frame_count
    { RK_GLOBAL, 0x506968, 0, 0, 4u, 0, 0 }, // lps
    { RK_GLOBAL, 0x506958, 0, 0, 4u, 0, 0 }, // logic_count
};
static const Region regions_update_frame[] = {
    { RK_GLOBAL, 0x4fec68, 0, 0, 4u, 0, 0 }, // reward_time
    { RK_GLOBAL, 0x4fac28, 0, 0, 4u, 0, 0 }, // reward_scale
    { RK_PLAYER_INDIRECT, 0x4ff128, 0, 0, 184u, 0x4fe518, 1000u }, // ply[player_id] (Tplayer)
};
static const Region regions_update_player[] = {
    { RK_ARG, 0, 0, 0, 184u, 0, 0 }, // Tplayer *p
};

static const FnDesc kFns[] = {
    // name  va  argc  returns_value  lifted  native  src  regions  num_regions  fault_region
    { "add_combo", 0x0040414cu, 2, false, nullptr, nullptr, (void*)add_combo, regions_add_combo, 1, 0 },
    { "add_jump_sequence", 0x004040f4u, 2, false, nullptr, nullptr, (void*)add_jump_sequence, regions_add_jump_sequence, 1, 0 },
    { "blit_to_screen", 0x0040b6bcu, 1, false, nullptr, nullptr, (void*)blit_to_screen, nullptr, 0, -1 },
    { "handle_player_collision_old", 0x00407fd8u, 2, false, nullptr, nullptr, (void*)handle_player_collision_old, nullptr, 0, -1 },
    { "handle_player_collision_vector", 0x00408d08u, 2, false, nullptr, nullptr, (void*)handle_player_collision_vector, nullptr, 0, -1 },
    { "handle_player_collision_vector_2", 0x004088c8u, 2, false, nullptr, nullptr, (void*)handle_player_collision_vector_2, nullptr, 0, -1 },
    { "handle_player_collision_combo", 0x00408358u, 2, false, nullptr, nullptr, (void*)handle_player_collision_combo, nullptr, 0, -1 },
    { "save_config", 0x0040e130u, 0, false, nullptr, nullptr, (void*)save_config, nullptr, 0, -1 },
    { "myDeleteFile", 0x0040cd28u, 2, false, nullptr, nullptr, (void*)myDeleteFile, nullptr, 0, -1 },
    { "get_gamepad", 0x004017fcu, 0, true, nullptr, nullptr, (void*)get_gamepad, nullptr, 0, -1 },
    { "is_up", 0x00401844u, 1, true, nullptr, nullptr, (void*)is_up, regions_is_up, 1, 0 },
    { "is_down", 0x0040185cu, 1, true, nullptr, nullptr, (void*)is_down, regions_is_down, 1, 0 },
    { "is_left", 0x00401874u, 1, true, nullptr, nullptr, (void*)is_left, regions_is_left, 1, 0 },
    { "is_right", 0x00401888u, 1, true, nullptr, nullptr, (void*)is_right, regions_is_right, 1, 0 },
    { "is_fire", 0x004018a0u, 1, true, nullptr, nullptr, (void*)is_fire, regions_is_fire, 1, 0 },
    { "is_pause", 0x004018b8u, 1, true, nullptr, nullptr, (void*)is_pause, regions_is_pause, 1, 0 },
    { "is_enter", 0x004018d0u, 1, true, nullptr, nullptr, (void*)is_enter, regions_is_enter, 1, 0 },
    { "is_any", 0x004018e8u, 1, true, nullptr, nullptr, (void*)is_any, regions_is_any, 1, 0 },
    { "set_control", 0x004017d4u, 6, false, nullptr, nullptr, (void*)set_control, regions_set_control, 1, 0 },
    { "init_control", 0x00401790u, 1, false, nullptr, nullptr, (void*)init_control, regions_init_control, 1, 0 },
    { "check_control_key", 0x00401808u, 2, true, nullptr, nullptr, (void*)check_control_key, regions_check_control_key, 1, 0 },
    { "destroy_game_data", 0x0040418cu, 1, false, nullptr, nullptr, (void*)destroy_game_data, nullptr, 0, -1 },
    { "draw_buffer", 0x004191c8u, 4, true, nullptr, nullptr, (void*)draw_buffer, nullptr, 0, -1 },
    { "draw_frame", 0x0040929cu, 1, false, nullptr, nullptr, (void*)draw_frame, nullptr, 0, -1 },
    { "draw_reward", 0x004070fcu, 1, false, nullptr, nullptr, (void*)draw_reward, nullptr, 0, -1 },
    { "draw_scroller", 0x0041f0ecu, 5, true, nullptr, nullptr, (void*)draw_scroller, nullptr, 0, -1 },
    { "draw_star_field", 0x0041f340u, 4, false, nullptr, nullptr, (void*)draw_star_field, nullptr, 0, -1 },
    { "fadeOut", 0x0040bf5cu, 1, false, nullptr, nullptr, (void*)fadeOut, nullptr, 0, -1 },
    { "fadeIn", 0x0040c1c0u, 2, false, nullptr, nullptr, (void*)fadeIn, nullptr, 0, -1 },
    { "handle_player_collision_original", 0x00407e10u, 2, false, nullptr, nullptr, (void*)handle_player_collision_original, nullptr, 0, -1 },
    { "handle_player_input", 0x0040b3e4u, 1, false, nullptr, nullptr, (void*)handle_player_input, nullptr, 0, -1 },
    { "qualify_hisc_table", 0x00404994u, 2, true, nullptr, nullptr, (void*)qualify_hisc_table, nullptr, 0, -1 },
    { "sort_hisc_table", 0x004049bcu, 1, false, nullptr, nullptr, (void*)sort_hisc_table, nullptr, 0, -1 },
    { "enter_hisc_table", 0x00405790u, 3, false, nullptr, nullptr, (void*)enter_hisc_table, nullptr, 0, -1 },
    { "is_solid", 0x004166dcu, 3, true, (void*)lifted_is_solid, (void*)native_is_solid, (void*)is_solid, regions_is_solid, 1, 0 },
    { "jump_player", 0x00418678u, 2, true, (void*)lifted_jump_player, nullptr, (void*)jump_player, regions_jump_player, 1, 0 },
    { "line_intersect", 0x00406b80u, 10, true, nullptr, nullptr, (void*)line_intersect, regions_line_intersect, 2, 0 },
    { "log2file", 0x0040da58u, 2, false, nullptr, nullptr, (void*)log2file, nullptr, 0, -1 },
    { "get_demo", 0x0040696cu, 0, true, nullptr, nullptr, (void*)get_demo, regions_get_demo, 1, -1 },
    { "get_controls", 0x00406978u, 0, true, nullptr, nullptr, (void*)get_controls, nullptr, 0, -1 },
    { "switchedFromProgram", 0x00406a5cu, 0, false, nullptr, nullptr, (void*)switchedFromProgram, regions_switchedFromProgram, 1, 0 },
    { "switchedToProgram", 0x00406a6cu, 0, false, nullptr, nullptr, (void*)switchedToProgram, regions_switchedToProgram, 1, 0 },
    { "clickedCloseButton", 0x00406a7cu, 0, false, nullptr, nullptr, (void*)clickedCloseButton, regions_clickedCloseButton, 1, 0 },
    { "get_version_str", 0x00406960u, 0, true, nullptr, nullptr, (void*)get_version_str, nullptr, 0, -1 },
    { "syncProfileFromOptions", 0x00406a14u, 0, false, nullptr, nullptr, (void*)syncProfileFromOptions, nullptr, 0, -1 },
    { "getFloorData", 0x00416770u, 5, false, nullptr, nullptr, (void*)getFloorData, regions_getFloorData, 4, 0 },
    { "reset_map", 0x004166a4u, 1, false, nullptr, nullptr, (void*)reset_map, regions_reset_map, 1, 0 },
    { "get_level", 0x00416748u, 2, true, nullptr, nullptr, (void*)get_level, regions_get_level, 1, 0 },
    { "add_floor", 0x004167dcu, 1, false, nullptr, nullptr, (void*)add_floor, regions_add_floor, 1, 0 },
    { "new_rand", 0x00406984u, 0, true, nullptr, nullptr, (void*)new_rand, regions_new_rand, 1, 0 },
    { "ok_to_play", 0x00406a50u, 0, true, nullptr, nullptr, (void*)ok_to_play, nullptr, 0, -1 },
    { "reset_particles", 0x00418420u, 1, false, nullptr, nullptr, (void*)reset_particles, regions_reset_particles, 1, 0 },
    { "update_particle", 0x0041843cu, 1, false, nullptr, nullptr, (void*)update_particle, regions_update_particle, 2, 0 },
    { "create_particle", 0x00418490u, 3, true, nullptr, nullptr, (void*)create_particle, regions_create_particle, 2, 0 },
    { "play", 0x00411a00u, 0, true, nullptr, nullptr, (void*)play, nullptr, 0, -1 },
    { "play_jump_sound", 0x00406eccu, 1, false, nullptr, nullptr, (void*)play_jump_sound, nullptr, 0, -1 },
    { "poll_control", 0x00401958u, 2, false, nullptr, nullptr, (void*)poll_control, nullptr, 0, -1 },
    { "get_rank_id", 0x00418a84u, 1, true, nullptr, nullptr, (void*)get_rank_id, nullptr, 0, -1 },
    { "get_rank", 0x00418ad0u, 1, true, nullptr, nullptr, (void*)get_rank, nullptr, 0, -1 },
    { "save_profile", 0x0041a3b8u, 1, true, nullptr, nullptr, (void*)save_profile, nullptr, 0, -1 },
    { "hash", 0x0041b9c8u, 1, true, nullptr, nullptr, (void*)hash, nullptr, 0, -1 },
    { "calc_replay_checksum_131", 0x0041ba10u, 1, true, nullptr, nullptr, (void*)calc_replay_checksum_131, nullptr, 0, -1 },
    { "calc_replay_checksum", 0x0041bac4u, 1, true, nullptr, nullptr, (void*)calc_replay_checksum, nullptr, 0, -1 },
    { "destroy_replay", 0x0041bd68u, 1, false, nullptr, nullptr, (void*)destroy_replay, nullptr, 0, -1 },
    { "create_replay", 0x0041cce8u, 1, true, nullptr, nullptr, (void*)create_replay, nullptr, 0, -1 },
    { "load_replay", 0x0041cde8u, 1, true, nullptr, nullptr, (void*)load_replay, nullptr, 0, -1 },
    { "save_replay", 0x0041dd78u, 5, true, nullptr, nullptr, (void*)save_replay, nullptr, 0, -1 },
    { "reset_player", 0x00418550u, 1, false, nullptr, nullptr, (void*)reset_player, regions_reset_player, 1, 0 },
    { "take_screenshot", 0x0041002cu, 1, false, nullptr, nullptr, (void*)take_screenshot, nullptr, 0, -1 },
    { "scroll_scroller", 0x0041f0c0u, 2, false, nullptr, nullptr, (void*)scroll_scroller, regions_scroll_scroller, 1, 0 },
    { "restart_scroller", 0x0041f0d0u, 1, false, nullptr, nullptr, (void*)restart_scroller, regions_restart_scroller, 1, 0 },
    { "init_scroller", 0x0041f278u, 6, false, nullptr, nullptr, (void*)init_scroller, nullptr, 0, -1 },
    { "play_sound", 0x00406da4u, 3, false, nullptr, nullptr, (void*)play_sound, nullptr, 0, -1 },
    { "startGameMusic", 0x0040cb30u, 0, false, nullptr, nullptr, (void*)startGameMusic, nullptr, 0, -1 },
    { "stopGameMusic", 0x0040caf4u, 0, false, nullptr, nullptr, (void*)stopGameMusic, nullptr, 0, -1 },
    { "start_reward", 0x00407c38u, 1, true, nullptr, nullptr, (void*)start_reward, nullptr, 0, -1 },
    { "cycle_counter", 0x0041fed4u, 0, false, nullptr, nullptr, (void*)cycle_counter, regions_cycle_counter, 1, 0 },
    { "fps_counter", 0x0041fea4u, 0, false, nullptr, nullptr, (void*)fps_counter, regions_fps_counter, 4, 0 },
    { "update_frame", 0x00406ac4u, 0, false, (void*)lifted_update_frame, (void*)native_update_frame, (void*)update_frame, regions_update_frame, 3, 1 },
    { "update_player", 0x00418740u, 1, false, nullptr, nullptr, (void*)update_player, regions_update_player, 1, 0 },
};
static const int kNumFns = (int)(sizeof(kFns) / sizeof(kFns[0]));

