/* The game's mode and scene tables, as far as the host has the scenes.
 *
 * gmscdata.c keeps one table of every game mode and one of every scene, and
 * each entry names that mode's or scene's callbacks, so compiling it would link
 * the whole game.  The host keeps the same shape with the entries it can run,
 * copied from gmscdata.c, and ends them with the same GM_COUNT and GS_COUNT
 * markers.  runGameMode and gm_FindGameSceneHandler walk them exactly as they
 * walk the originals.  A mode or a scene missing here is one the host does not
 * run yet, and asking for it is refused before the game would reach through
 * the NULL it finds. */

#include <melee_host/boot.h>

#include <melee/gm/gm_1A3F.h>
#include <melee/gm/gmapproach.h>
#include <melee/gm/gmmenumode.h>
#include <melee/gm/gmresult.h>
#include <melee/gm/gmscdata.h>
#include <melee/gm/gmtitle.h>
#include <melee/gm/gmtitlemode.h>
#include <melee/gm/types.h>
#include <melee/gm/gmvs.h>
#include <melee/gm/gmvsmelee.h>
#include <melee/gm/gmvsmode.h>
#include <melee/mn/mncharsel.h>
#include <melee/mn/mnmain.h>
#include <melee/mn/mnstagesel.h>
#include <melee/if/ifprize.h>

#include <stddef.h>
#include <string.h>

static GameScene host_scenes[] = {
    {
        GS_TITLE,
        gm_Scene_Title_OnFrame,
        gm_Scene_Title_OnEnter,
        NULL,
        NULL,
    },
    {
        GS_MENU,
        mnMain_Scene_OnFrame,
        mnMain_Scene_OnEnter,
        NULL,
        NULL,
    },
    {
        GS_CSS,
        mnCharSel_Scene_OnFrame,
        mnCharSel_Scene_OnEnter,
        mnCharSel_Scene_OnExit,
        NULL,
    },
    {
        GS_SSS,
        mnStageSel_Scene_OnFrame,
        mnStageSel_Scene_OnEnter,
        mnStageSel_Scene_OnExit,
        NULL,
    },
    {
        GS_VS,
        gm_Scene_Vs_OnFrame,
        gm_Scene_Vs_OnEnter,
        gm_Scene_Vs_OnExit,
        NULL,
    },
    /* A timed match that ends tied: the same fight scene, entered with the
     * sudden death rules. */
    {
        GS_SUDDEN_DEATH,
        gm_Scene_Vs_OnFrame,
        gm_Scene_SuddenDeath_OnEnter,
        gm_Scene_Vs_OnExit,
        NULL,
    },
    {
        GS_RESULTS,
        NULL,
        gm_Scene_Results_OnEnter,
        gm_Scene_Results_OnExit,
        NULL,
    },
    /* The challenger the results screen can lead to, which announces the
     * fighter a play total unlocked and then hands the mode a match against
     * it. */
    {
        GS_APPROACH,
        gm_Scene_Approach_OnFrame,
        gm_Scene_Approach_OnEnter,
        gm_Scene_Approach_OnExit,
        NULL,
    },
    /* The prize notice, the other place the results screen can lead to. */
    {
        GS_PRIZE_INTERFACE,
        NULL,
        ifPrize_Scene_OnEnter,
        ifPrize_Scene_OnExit,
        NULL,
    },
    {
        GS_COUNT,
        NULL,
        NULL,
        NULL,
        NULL,
    },
};

static GameMode host_modes[] = {
    {
        true,
        GM_TITLE,
        NULL,
        NULL,
        NULL,
        gm_Mode_Title_States,
    },
    {
        true,
        GM_MENU,
        NULL,
        NULL,
        NULL,
        gm_Mode_Menu_States,
    },
    {
        false,
        GM_VS,
        gmVsMelee_Mode_OnLoad,
        gm_Mode_Vs_OnUnload,
        gmVsMelee_Mode_OnInit,
        gm_Mode_Vs_States,
    },
    {
        false,
        GM_COUNT,
        NULL,
        NULL,
        NULL,
        NULL,
    },
};

GameScene* gm_GetAllGameScenes(void)
{
    return host_scenes;
}

GameMode* gm_GetAllGameModes(void)
{
    return host_modes;
}

bool melee_host_game_mode_available(mh_u32 mode)
{
    const GameMode* entry;

    for (entry = host_modes; entry->kind != GM_COUNT; entry++) {
        if (entry->kind == mode) {
            return true;
        }
    }
    return false;
}

MeleeHostStatus melee_host_game_begin(mh_u32 first_mode)
{
    if (!melee_host_game_mode_available(first_mode)) {
        return MELEE_HOST_UNSUPPORTED;
    }
    gm_HostBeginGameModes((u8) first_mode);
    return MELEE_HOST_OK;
}

mh_u32 melee_host_game_current_mode(void)
{
    return gm_GetCurrentGameMode();
}

static struct {
    mh_u32 drawn_frames;
    MeleeHostGxFrameSink frame_sink;
    void* frame_sink_user_data;
    MeleeHostGameModeReport* report;
} mode_run;

static void mode_frame_drawn(void* user_data)
{
    MeleeHostGameModeReport* const report = mode_run.report;

    (void) user_data;
    mode_run.drawn_frames += 1;
    if (report != NULL && report->scene_count != 0 &&
        report->scene_count <= MELEE_HOST_GAME_MODE_MAX_SCENES)
    {
        report->scenes[report->scene_count - 1].drawn_frames += 1;
    }
    if (mode_run.frame_sink != NULL) {
        mode_run.frame_sink(mode_run.frame_sink_user_data);
    }
}

void gm_HostSceneEntered(u8 scene_kind)
{
    MeleeHostGameModeReport* const report = mode_run.report;

    if (report == NULL) {
        return;
    }
    if (report->scene_count < MELEE_HOST_GAME_MODE_MAX_SCENES) {
        report->scenes[report->scene_count].scene = scene_kind;
    }
    report->scene_count += 1;
}

MeleeHostStatus melee_host_game_run_current_mode(
    MeleeHostGxFrameSink frame_sink, void* user_data,
    MeleeHostGameModeReport* out_report)
{
    if (out_report == NULL) {
        return MELEE_HOST_INVALID_ARGUMENT;
    }
    memset(out_report, 0, sizeof(*out_report));
    out_report->mode = gm_GetCurrentGameMode();
    if (!melee_host_game_mode_available(out_report->mode)) {
        return MELEE_HOST_UNSUPPORTED;
    }
    memset(&mode_run, 0, sizeof(mode_run));
    mode_run.frame_sink = frame_sink;
    mode_run.frame_sink_user_data = user_data;
    mode_run.report = out_report;

    melee_host_gx_set_frame_sink(mode_frame_drawn, NULL);
    out_report->next_mode = gm_HostRunCurrentGameMode();
    melee_host_gx_set_frame_sink(NULL, NULL);
    mode_run.report = NULL;

    out_report->drawn_frames = mode_run.drawn_frames;
    out_report->missing_scene = gm_HostMissingScene();
    out_report->stopped_at_missing_scene = out_report->missing_scene != GS_COUNT;
    return MELEE_HOST_OK;
}

extern void gm_8016468C(void);
extern void gm_80164F18(void);

void melee_host_game_unlock_all(void)
{
    gm_8016468C();
    gm_80164F18();
}

/* Stages the host has been measured to enter.  A stage that is not here still
 * loads if it is asked for, and still stops loudly if its data cannot be
 * translated; this only keeps a player from picking one on the select screen
 * and losing the process to it.
 *
 * The list is measured, not asserted: rerun port/tools/sweep_stages.py after
 * changing stage data, update port/data/stage_status.json from the results,
 * then run port/tools/gen_stage_status.py. */
#include "playable_stages.c.inc"

bool melee_host_stage_is_playable(int stkind)
{
    unsigned i;

    for (i = 0; i < sizeof(melee_host_playable_stkinds) /
                    sizeof(melee_host_playable_stkinds[0]);
         i++)
    {
        if ((int) melee_host_playable_stkinds[i] == stkind) {
            return true;
        }
    }
    return false;
}
