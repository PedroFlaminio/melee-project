#ifndef MELEE_HOST_BOOT_H
#define MELEE_HOST_BOOT_H

#include <melee_host/gx.h>
#include <melee_host/host.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The part of gmMain that builds the game's memory before a scene runs, and
 * the heap setup the first scene then performs: an arena from host memory,
 * HSD_InitComponent, lbMemory_8001564C, lbHeap_80015F3C and lbHeap_80015900,
 * in that order.
 *
 * It moves the OS arena and replaces HSD's heaps, so it belongs in a process of
 * its own rather than beside tests that assume the headless bootstrap. */

typedef struct MeleeHostBootMemoryStats {
    mh_u64 arena_bytes;
    /* lbHeap slots in the created state.  After boot that is the main heap
     * and the ARAM heap; the scene heaps join once a scene keeps them. */
    mh_u32 lb_heaps_created;
} MeleeHostBootMemoryStats;

/* `arena_bytes` of zero gives the console's main memory size. */
MeleeHostStatus melee_host_boot_memory_init(size_t arena_bytes);
MeleeHostStatus
melee_host_boot_memory_stats(MeleeHostBootMemoryStats* out_stats);

typedef struct MeleeHostTitleArchiveReport {
    mh_u32 file_bytes;
    /* Of the twelve symbols gmTitle_801A1AC0 names. */
    mh_u32 symbols_resolved;
    mh_u32 title_jobjs;
    mh_u32 title_animated_jobjs;
    mh_u32 background_jobjs;
    mh_u32 background_animated_jobjs;
    mh_u32 lights;
    mh_u8 camera_loaded;
    mh_u8 fog_loaded;
    mh_u8 mark_has_image;
} MeleeHostTitleArchiveReport;

/* The title screen's lbArchive_LoadSymbols call from gmTitle_801A1AC0, with
 * its file and symbol list, through the game's own lbArchive and lbFile over
 * devcom and the host DVD.  Then the loads gm_Scene_Title_OnEnter performs on
 * the result, with each object freed again.  Needs the boot memory and an
 * active DVD backend; a missing symbol is fatal, as it is in the game. */
MeleeHostStatus
melee_host_boot_load_title_archive(MeleeHostTitleArchiveReport* out_report);

/* Reads the game data that lives in main.dol rather than in a disc file, such
 * as the SIS font atlas, from the user's extracted DOL. */
MeleeHostStatus melee_host_boot_load_dol_data(const char* dol_path);

/* The middle of gm_801A4014 for the title screen: gm_801A4BD4, the scene
 * manager's per-scene setup; gm_801A4B88 with the scene info of the title's
 * state in gmtitlemode.c; then gm_Scene_Title_OnEnter.  The state's preload and
 * on_enter, which come before it, are not run. */
void melee_host_title_scene_enter(void);

typedef struct MeleeHostTitleSceneReport {
    /* GObjs on the process lists, and those with a render callback. */
    mh_u32 gobjs;
    mh_u32 rendered;
    /* GObjs by the object they hold. */
    mh_u32 cameras;
    mh_u32 lights;
    mh_u32 fogs;
    mh_u32 models;
    mh_u32 procs;
    mh_u32 jobjs;
    mh_u32 lobjs;
} MeleeHostTitleSceneReport;

/* What the scene built, read from the GObj library's own lists. */
MeleeHostStatus
melee_host_title_scene_report(MeleeHostTitleSceneReport* out_report);

typedef struct MeleeHostTitleRunReport {
    /* Calls of the scene's on_frame: one per game frame gm_801A4D34 runs. */
    mh_u32 scene_frames;
    /* Frames the loop drew and copied to an XFB, and the triangles captured
     * in the last of them. */
    mh_u32 drawn_frames;
    mh_u32 last_frame_triangles;
    /* Retraces VI ran during the loop. */
    mh_u32 retraces;
    /* What gm_Scene_Title_OnFrame left in the state's exit data: the buttons
     * that ended the scene, or zero when it timed out. */
    mh_u32 exit_buttons;
    /* OS time the loop took, in ticks. */
    mh_u64 elapsed_ticks;
} MeleeHostTitleRunReport;

/* gm_801A4D34, the scene manager's frame loop, with the title's on_frame, until
 * the scene asks to leave.  Each drawn frame's GX capture goes to `frame_sink`,
 * when there is one, and is then cleared.  The scene has to have been entered.
 * With the OS clock frozen the loop runs as fast as it computes, unless the
 * sink paces it; otherwise it follows the wall clock, spinning while it
 * waits. */
MeleeHostStatus melee_host_title_scene_run(MeleeHostGxFrameSink frame_sink,
                                           void* user_data,
                                           MeleeHostTitleRunReport* out_report);

/* gm_801A4B60, what the scene itself calls to leave: the frame loop returns
 * after the frame in progress.  The exit data keeps what the scene last left
 * there. */
void melee_host_title_scene_request_exit(void);

/* Registers the translators for game data only the game's C headers describe
 * (port/src/game/game_data_translators.c) with the host's archive API.  The
 * boot memory setup runs it before anything is loaded. */
void melee_host_game_register_data_translators(void);

/* What an item's special attributes or dynamics point at when the host left
 * them out of itPublicData; Item_80267978 stops by name on it. */
extern char melee_host_item_data_left_out;

/* Whether the host's mode table (port/src/game/game_tables.c) has this
 * GameModeKind. */
bool melee_host_game_mode_available(mh_u32 mode);

/* Starts the scene manager's routing at `first_mode`, as gm_801A4510 starts it
 * at the boot mode: every mode's on_init runs and there is no previous mode.
 * A mode the table lacks is refused. */
void melee_host_game_unlock_all(void);
MeleeHostStatus melee_host_game_begin(mh_u32 first_mode);

/* The GameModeKind the routing runs next. */
mh_u32 melee_host_game_current_mode(void);

/* Scenes one mode report records; `scene_count` keeps counting past it. */
#define MELEE_HOST_GAME_MODE_MAX_SCENES 16

typedef struct MeleeHostGameSceneReport {
    /* The GameSceneKind of a state the mode routed through. */
    mh_u32 scene;
    /* Frames drawn while that state's scene ran. */
    mh_u32 drawn_frames;
} MeleeHostGameSceneReport;

typedef struct MeleeHostGameModeReport {
    /* The GameModeKind that ran, or was refused. */
    mh_u32 mode;
    /* Frames drawn while the mode ran. */
    mh_u32 drawn_frames;
    /* What runGameMode returned: the GameModeKind the mode left pending,
     * which is now the current mode. */
    mh_u32 next_mode;
    /* The scenes of the states the mode ran, in order. */
    mh_u32 scene_count;
    MeleeHostGameSceneReport scenes[MELEE_HOST_GAME_MODE_MAX_SCENES];
    /* Set when the mode routed to a state whose scene the host's scene table
     * lacks.  The mode stopped there, before that state preloaded anything,
     * so `next_mode` is only what the routing had pending then. */
    bool stopped_at_missing_scene;
    mh_u32 missing_scene;
} MeleeHostGameModeReport;

/* One pass of gm_801A4510's loop: runGameMode for the current mode, with its
 * preload, each state it routes through until one asks for a new mode, and its
 * unload, then the current mode becomes the previous one and the pending mode
 * the current one.  Each drawn frame's GX capture goes to `frame_sink`, when
 * there is one, and is then cleared.  UNSUPPORTED, with nothing run, when the
 * current mode is not in the host's table.  A state whose scene is not in the
 * host's scene table ends the mode, reported in `stopped_at_missing_scene`. */
/* Whether the host has been measured to enter a match on `stkind`.  The stage
 * select refuses a square that is not playable instead of letting the player
 * pick a stage whose data stops the process; a stage asked for another way
 * still loads, and still stops loudly if its data cannot be translated. */
bool melee_host_stage_is_playable(int stkind);

MeleeHostStatus melee_host_game_run_current_mode(
    MeleeHostGxFrameSink frame_sink, void* user_data,
    MeleeHostGameModeReport* out_report);

#ifdef __cplusplus
}
#endif

#endif
