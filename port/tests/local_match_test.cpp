#include "test.hpp"

#include <melee_host/boot.h>
#include <melee_host/local_match.h>
#include <melee_host/match_rules.h>

TEST_CASE("the measured playable-stage gate includes every passing stage")
{
    REQUIRE(melee_host_stage_is_playable(16)); // Yoshi's Island
    REQUIRE(melee_host_stage_is_playable(20)); // Mushroom Kingdom II
    REQUIRE(!melee_host_stage_is_playable(-1));
    REQUIRE(!melee_host_stage_is_playable(255));
}

TEST_CASE("native VS start data prepares a local two-player match")
{
    const MeleeHostMatchRules rules{
        .mode = 1,
        .time_limit = 0,
        .stock_count = 4,
        .handicap = 0,
        .damage_ratio = 10,
        .friendly_fire = true,
        .pause = true,
    };
    REQUIRE(melee_host_match_rules_set(&rules) == MELEE_HOST_OK);
    REQUIRE(melee_host_prepare_local_two_player_match(2, 8, 3) ==
            MELEE_HOST_OK);

    MeleeHostPreparedMatch match{};
    REQUIRE(melee_host_prepared_match_get(&match) == MELEE_HOST_OK);
    REQUIRE(match.match_kind == 1);
    REQUIRE(!match.timer_enabled);
    REQUIRE(match.stage_kind == 3);
    REQUIRE(match.player_count == 2);
    REQUIRE(match.characters[0] == 2);
    REQUIRE(match.characters[1] == 8);
    REQUIRE(match.player_kinds[0] == 0);
    REQUIRE(match.player_kinds[1] == 1);
    REQUIRE(match.stocks[0] == 4);
    REQUIRE(match.stocks[1] == 4);
    REQUIRE(melee_host_initialize_prepared_player_state() == MELEE_HOST_OK);
    MeleeHostPlayerState first{};
    MeleeHostPlayerState second{};
    MeleeHostPlayerState inactive{};
    REQUIRE(melee_host_player_state_get(0, &first) == MELEE_HOST_OK);
    REQUIRE(melee_host_player_state_get(1, &second) == MELEE_HOST_OK);
    REQUIRE(melee_host_player_state_get(2, &inactive) == MELEE_HOST_OK);
    REQUIRE(first.character == 2);
    REQUIRE(first.player_kind == 0);
    REQUIRE(first.stocks == 4);
    REQUIRE(second.character == 8);
    REQUIRE(second.player_kind == 1);
    REQUIRE(inactive.player_kind == 3);
    REQUIRE(melee_host_prepare_local_two_player_match(-1, 8, 3) ==
            MELEE_HOST_INVALID_ARGUMENT);
    REQUIRE(melee_host_match_rules_reset_defaults() == MELEE_HOST_OK);
}

TEST_CASE("native VS start data translates default time rules")
{
    REQUIRE(melee_host_match_rules_reset_defaults() == MELEE_HOST_OK);
    REQUIRE(melee_host_prepare_local_two_player_match(6, 14, 0x1F) ==
            MELEE_HOST_OK);

    MeleeHostPreparedMatch match{};
    REQUIRE(melee_host_prepared_match_get(&match) == MELEE_HOST_OK);
    REQUIRE(match.match_kind == 0);
    REQUIRE(match.timer_enabled);
    REQUIRE(match.time_limit_seconds == 120);
    REQUIRE(match.stage_kind == 0x1F);
    REQUIRE(match.stocks[0] == 3);
    REQUIRE(match.stocks[1] == 3);
}
