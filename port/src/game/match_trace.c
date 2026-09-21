#include <melee_host/match_trace.h>

#include <melee/ft/ftlib.h>
#include <melee/gm/gmmain_lib.h>
#include <melee/gm/gm_16F1.h>
#include <melee/gm/gmvs.h>
#include <melee/gm/gmresult.h>
#include <melee/gm/types.h>
#include <melee/ft/inlines.h>
#include <melee/mn/mnstagesel.h>
#include <melee/pl/player.h>
#include <sysdolphin/baselib/random.h>

bool melee_host_match_fighter_position(mh_u32 slot, mh_f32* x, mh_f32* y,
                                        mh_f32* z)
{
    HSD_GObj* fighter;
    Vec3 position;

    if (slot >= 4 || x == NULL || y == NULL || z == NULL) {
        return false;
    }
    fighter = Player_GetEntity((s32) slot);
    if (fighter == NULL) {
        return false;
    }
    ftLib_80086644(fighter, &position);
    *x = position.x;
    *y = position.y;
    *z = position.z;
    return true;
}

bool melee_host_match_fighter_motion(mh_u32 slot, mh_s32* motion)
{
    HSD_GObj* fighter;

    if (slot >= 4 || motion == NULL) {
        return false;
    }
    fighter = Player_GetEntity((s32) slot);
    if (fighter == NULL) {
        return false;
    }
    *motion = GET_FIGHTER(fighter)->motion_id;
    return true;
}

bool melee_host_match_player_falls(mh_u32 slot, mh_s32* falls)
{
    if (slot >= 4 || falls == NULL ||
        Player_GetPlayerSlotType((s32) slot) == Gm_PKind_NA)
    {
        return false;
    }
    *falls = Player_GetFalls((s32) slot);
    return true;
}

void melee_host_match_rules(mh_u32* mode, mh_u32* time_limit,
                            mh_u32* stock_count)
{
    GameRules* rules = gmMainLib_GetGameRules();

    *mode = rules->mode;
    *time_limit = rules->time_limit;
    *stock_count = rules->stock_count;
}

bool melee_host_match_result(mh_u32* outcome, mh_u32* winners,
                             mh_u32* first_winner, mh_s32* stocks_p1,
                             mh_s32* stocks_p2)
{
    MatchEnd* end = fn_80174274();

    if (end == NULL) {
        return false;
    }
    *outcome = end->outcome;
    *winners = end->n_winners;
    *first_winner = end->winners[0];
    *stocks_p1 = end->player_standings[0].stocks;
    *stocks_p2 = end->player_standings[1].stocks;
    return true;
}

bool melee_host_match_place(mh_u32 slot, mh_s32* place)
{
    MatchEnd* end = fn_80174274();

    if (slot >= 4 || place == NULL || end == NULL ||
        end->player_standings[slot].pkind == Gm_PKind_NA)
    {
        return false;
    }
    *place = end->player_standings[slot].is_small_loser + 1;
    return true;
}

mh_u32 melee_host_match_vs_total(void)
{
    return gmMainLib_8015ED98()->x0;
}

void melee_host_match_set_vs_total(mh_u32 total)
{
    gmMainLib_8015ED98()->x0 = total;
}

bool melee_host_match_award_trophy(mh_u32 trophy)
{
    return fn_80172C78((int) trophy);
}

bool melee_host_match_force_stage(mh_u32 stkind)
{
    return melee_host_sss_force_stage((int) stkind);
}

bool melee_host_match_clock(mh_u32* seconds, mh_u32* frames)
{
    VsSceneController* scene = gmVs_GetSceneController();

    if (seconds == NULL || frames == NULL || !scene->start.timer_enabled) {
        return false;
    }
    *seconds = scene->state.timer_seconds;
    *frames = scene->state.unk_2C;
    return true;
}

bool melee_host_match_set_clock(mh_u32 seconds)
{
    VsSceneController* scene = gmVs_GetSceneController();

    if (!scene->start.timer_enabled) {
        return false;
    }
    /* The scene counts frames up to 60 and then takes a second off, so the
     * frames inside the second start over with the new value. */
    scene->state.timer_seconds = seconds;
    scene->state.unk_2C = 0;
    return true;
}

bool melee_host_match_fighter_sample(mh_u32 slot,
                                     MeleeHostMatchFighterSample* sample)
{
    HSD_GObj* fighter;
    Fighter* fp;

    if (slot >= 4 || sample == NULL) {
        return false;
    }
    fighter = Player_GetEntity((s32) slot);
    if (fighter == NULL) {
        return false;
    }
    fp = GET_FIGHTER(fighter);
    sample->motion = fp->motion_id;
    sample->anim_frame = fp->cur_anim_frame;
    sample->x = fp->cur_pos.x;
    sample->y = fp->cur_pos.y;
    sample->vel_x = fp->self_vel.x;
    sample->vel_y = fp->self_vel.y;
    sample->facing = fp->facing_dir;
    sample->airborne = fp->ground_or_air == GA_Air;
    sample->percent = fp->dmg.x1830_percent;
    sample->stocks = Player_GetStocks((s32) slot);
    return true;
}

mh_u32 melee_host_match_random_seed(void)
{
    return *HSD_RandSeedPtr;
}
