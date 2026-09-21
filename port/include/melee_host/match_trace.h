#ifndef MELEE_HOST_MATCH_TRACE_H
#define MELEE_HOST_MATCH_TRACE_H

#include <melee_host/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A narrow C ABI for observing the live VS scene without exposing the
 * PowerPC-layout fighter headers to host C++ tools. */
bool melee_host_match_fighter_position(mh_u32 slot, mh_f32* x, mh_f32* y,
                                        mh_f32* z);
bool melee_host_match_fighter_motion(mh_u32 slot, mh_s32* motion);
/* The falls (KOs suffered) the game counts for a player slot; false for an
 * empty slot. */
bool melee_host_match_player_falls(mh_u32 slot, mh_s32* falls);
/* The VS rules the menus keep for the next match: the mode (0 time, 1
 * stock, 2 coin, 3 bonus), the time limit in minutes and the stock count. */
void melee_host_match_rules(mh_u32* mode, mh_u32* time_limit,
                            mh_u32* stock_count);
/* The match end the results screen reads: its outcome (a MatchOutcome), the
 * number of winners, the first winner's slot and the stocks left to the
 * first two slots.  False while no match end is set. */
bool melee_host_match_result(mh_u32* outcome, mh_u32* winners,
                             mh_u32* first_winner, mh_s32* stocks_p1,
                             mh_s32* stocks_p2);

/* Where a player slot finished the match the results screen reads: the place
 * the screen shows, counting from 1, which the match end keeps as the number
 * of players ahead with ties already broken.  False for an empty slot or
 * while no match end is set. */
bool melee_host_match_place(mh_u32 slot, mh_s32* place);

/* The VS matches the save data counts, which is what decides the challenger
 * the results screen can lead to.  Without a memory card the host starts it
 * at zero, and setting it stands in for the matches already played. */
mh_u32 melee_host_match_vs_total(void);
void melee_host_match_set_vs_total(mh_u32 total);

/* Award a trophy the way the game does when a play total crosses one of its
 * thresholds (fn_80172C78), which is what leaves the prize notice pending.
 * False when the save data already had it. */
bool melee_host_match_award_trophy(mh_u32 trophy);

/* Send the running stage select screen straight into a match on `stkind`,
 * through the force_stage_id the game's own Training and Tournament modes
 * set.  Lets a route reach any stage without steering the cursor across the
 * select screen, which is how the stage dimension gets swept for crashes.
 * False unless the stage select is the running scene. */
bool melee_host_match_force_stage(mh_u32 stkind);

/* The clock the running VS scene counts, in whole seconds left and frames
 * inside the current second (0 to 59).  False when the scene has no timer. */
bool melee_host_match_clock(mh_u32* seconds, mh_u32* frames);
/* Leave the running match that many seconds on the clock, so a route can
 * reach a time-up without playing the whole minute the shortest rule allows.
 * The game keeps counting from there and ends the match itself.  False when
 * the scene has no timer. */
bool melee_host_match_set_clock(mh_u32 seconds);

/* One fighter's gameplay state for a canonical trace: the fields two runs of
 * the same route are compared on, with no host addresses or padding. */
typedef struct MeleeHostMatchFighterSample {
    mh_s32 motion;
    mh_f32 anim_frame;
    mh_f32 x;
    mh_f32 y;
    mh_f32 vel_x;
    mh_f32 vel_y;
    mh_f32 facing;
    mh_s32 airborne;
    mh_f32 percent;
    mh_s32 stocks;
} MeleeHostMatchFighterSample;

/* The sample of the fighter in a player slot; false when the slot has no
 * fighter. */
bool melee_host_match_fighter_sample(mh_u32 slot,
                                     MeleeHostMatchFighterSample* sample);
/* The HSD random generator's seed, which every HSD_Rand call advances. */
mh_u32 melee_host_match_random_seed(void);

#ifdef __cplusplus
}
#endif

#endif
