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
    int __cdecl draw_buffer(void*, void*, int, int);
    int __cdecl draw_scroller(void*, void*, int, int, int);
    void __cdecl handle_player_collision_original(int, int);
    int __cdecl is_solid(void*, int, int);
    int __cdecl lifted_is_solid(void*, int, int);
    int __cdecl native_is_solid(void*, int, int);
    int __cdecl jump_player(void*, int);
    int __cdecl lifted_jump_player(void*, int);
    int __cdecl line_intersect(int, int, int, int, int, int, int, int, void*, void*);
    void* __cdecl get_demo(void);
    void* __cdecl get_controls(void);
    void __cdecl switchedFromProgram(void);
    void __cdecl switchedToProgram(void);
    void __cdecl clickedCloseButton(void);
    void __cdecl getFloorData(void*, int, void*, void*, void*);
    void __cdecl reset_map(void*);
    int __cdecl get_level(void*, int);
    void __cdecl add_floor(void*);
    int __cdecl new_rand(void);
    int __cdecl ok_to_play(void);
    void __cdecl reset_particles(void*);
    void __cdecl update_particle(void*);
    int __cdecl create_particle(void*, int, int);
    void __cdecl play_jump_sound(void*);
    void __cdecl reset_player(void*);
    void __cdecl scroll_scroller(void*, int);
    void __cdecl restart_scroller(void*);
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
    { "draw_buffer", 0x004191c8u, 4, true, nullptr, nullptr, (void*)draw_buffer, nullptr, 0, -1 },
    { "draw_scroller", 0x0041f0ecu, 5, true, nullptr, nullptr, (void*)draw_scroller, nullptr, 0, -1 },
    { "handle_player_collision_original", 0x00407e10u, 2, false, nullptr, nullptr, (void*)handle_player_collision_original, nullptr, 0, -1 },
    { "is_solid", 0x004166dcu, 3, true, (void*)lifted_is_solid, (void*)native_is_solid, (void*)is_solid, regions_is_solid, 1, 0 },
    { "jump_player", 0x00418678u, 2, true, (void*)lifted_jump_player, nullptr, (void*)jump_player, regions_jump_player, 1, 0 },
    { "line_intersect", 0x00406b80u, 10, true, nullptr, nullptr, (void*)line_intersect, regions_line_intersect, 2, 0 },
    { "get_demo", 0x0040696cu, 0, true, nullptr, nullptr, (void*)get_demo, regions_get_demo, 1, -1 },
    { "get_controls", 0x00406978u, 0, true, nullptr, nullptr, (void*)get_controls, nullptr, 0, -1 },
    { "switchedFromProgram", 0x00406a5cu, 0, false, nullptr, nullptr, (void*)switchedFromProgram, regions_switchedFromProgram, 1, 0 },
    { "switchedToProgram", 0x00406a6cu, 0, false, nullptr, nullptr, (void*)switchedToProgram, regions_switchedToProgram, 1, 0 },
    { "clickedCloseButton", 0x00406a7cu, 0, false, nullptr, nullptr, (void*)clickedCloseButton, regions_clickedCloseButton, 1, 0 },
    { "getFloorData", 0x00416770u, 5, false, nullptr, nullptr, (void*)getFloorData, regions_getFloorData, 4, 0 },
    { "reset_map", 0x004166a4u, 1, false, nullptr, nullptr, (void*)reset_map, regions_reset_map, 1, 0 },
    { "get_level", 0x00416748u, 2, true, nullptr, nullptr, (void*)get_level, regions_get_level, 1, 0 },
    { "add_floor", 0x004167dcu, 1, false, nullptr, nullptr, (void*)add_floor, regions_add_floor, 1, 0 },
    { "new_rand", 0x00406984u, 0, true, nullptr, nullptr, (void*)new_rand, regions_new_rand, 1, 0 },
    { "ok_to_play", 0x00406a50u, 0, true, nullptr, nullptr, (void*)ok_to_play, nullptr, 0, -1 },
    { "reset_particles", 0x00418420u, 1, false, nullptr, nullptr, (void*)reset_particles, regions_reset_particles, 1, 0 },
    { "update_particle", 0x0041843cu, 1, false, nullptr, nullptr, (void*)update_particle, regions_update_particle, 2, 0 },
    { "create_particle", 0x00418490u, 3, true, nullptr, nullptr, (void*)create_particle, regions_create_particle, 2, 0 },
    { "play_jump_sound", 0x00406eccu, 1, false, nullptr, nullptr, (void*)play_jump_sound, nullptr, 0, -1 },
    { "reset_player", 0x00418550u, 1, false, nullptr, nullptr, (void*)reset_player, regions_reset_player, 1, 0 },
    { "scroll_scroller", 0x0041f0c0u, 2, false, nullptr, nullptr, (void*)scroll_scroller, regions_scroll_scroller, 1, 0 },
    { "restart_scroller", 0x0041f0d0u, 1, false, nullptr, nullptr, (void*)restart_scroller, regions_restart_scroller, 1, 0 },
    { "start_reward", 0x00407c38u, 1, true, nullptr, nullptr, (void*)start_reward, nullptr, 0, -1 },
    { "cycle_counter", 0x0041fed4u, 0, false, nullptr, nullptr, (void*)cycle_counter, regions_cycle_counter, 1, 0 },
    { "fps_counter", 0x0041fea4u, 0, false, nullptr, nullptr, (void*)fps_counter, regions_fps_counter, 4, 0 },
    { "update_frame", 0x00406ac4u, 0, false, (void*)lifted_update_frame, (void*)native_update_frame, (void*)update_frame, regions_update_frame, 3, 1 },
    { "update_player", 0x00418740u, 1, false, nullptr, nullptr, (void*)update_player, regions_update_player, 1, 0 },
};
static const int kNumFns = (int)(sizeof(kFns) / sizeof(kFns[0]));

