#include "test.h"
#include "run.h"
#include "game.h"
#include "round.h"

#include <string.h>

/*
 * Link shim for run.c's dependency on twistActive(), which lives in round.c.
 * These tests isolate getMinBet() and do not link the full round/game loop.
 */
int twistActive(const Game *game, TwistId id) {
    int twistIndex;

    if (game->run.twistBlocked &&
        id == game->run.twistId) {
        return 0;
    }

    if (game->run.twistId == id) {
        return 1;
    }

    for (twistIndex = 0; twistIndex < game->run.extraTwistCount;
         twistIndex++) {
        if (game->run.extraTwists[twistIndex] == id) {
            return 1;
        }
    }

    return 0;
}

static void test_init_game_for_min_bet(Game *game) {
    memset(game, 0, sizeof(*game));
    game->mode = MODE_ROGUELIKE;
    game->minBetMultiplier = 1;
    game->run.twistId = TWIST_NONE;
    game->run.extraTwistCount = 0;
    game->run.twistBlocked = 0;
}

TEST(run_classic_minimum_bet_is_always_one) {
    Game game;

    test_init_game_for_min_bet(&game);

    ASSERT_EQ_INT(1, getMinBet(&game, 1, MODE_CLASSIC));
    ASSERT_EQ_INT(1, getMinBet(&game, 100, MODE_CLASSIC));
}

TEST(run_roguelike_minimum_bet_by_floor) {
    Game game;

    test_init_game_for_min_bet(&game);

    ASSERT_EQ_INT(1, getMinBet(&game, 1, MODE_ROGUELIKE));
    ASSERT_EQ_INT(2, getMinBet(&game, 3, MODE_ROGUELIKE));
    ASSERT_EQ_INT(5, getMinBet(&game, 5, MODE_ROGUELIKE));
}

TEST(run_roguelike_quadratic_minimum_bet_late_floors) {
    Game game;

    test_init_game_for_min_bet(&game);

    ASSERT_EQ_INT(26, getMinBet(&game, 10, MODE_ROGUELIKE));
    ASSERT_EQ_INT(50, getMinBet(&game, 15, MODE_ROGUELIKE));
}

TEST(run_greed_doubles_minimum_bet) {
    Game game;

    test_init_game_for_min_bet(&game);
    game.run.twistId = TWIST_GREED;

    ASSERT_EQ_INT(10, getMinBet(&game, 5, MODE_ROGUELIKE));
}

TEST(run_blocked_greed_does_not_double_minimum_bet) {
    Game game;

    test_init_game_for_min_bet(&game);
    game.run.twistId = TWIST_GREED;
    game.run.twistBlocked = 1;

    ASSERT_EQ_INT(5, getMinBet(&game, 5, MODE_ROGUELIKE));
}

TEST(run_minimum_bet_multiplier_is_applied) {
    Game game;

    test_init_game_for_min_bet(&game);
    game.minBetMultiplier = 2;

    ASSERT_EQ_INT(2, getMinBet(&game, 1, MODE_ROGUELIKE));
    ASSERT_EQ_INT(10, getMinBet(&game, 5, MODE_ROGUELIKE));
}

void run_run_tests(void) {
    RUN_TEST(run_classic_minimum_bet_is_always_one);
    RUN_TEST(run_roguelike_minimum_bet_by_floor);
    RUN_TEST(run_roguelike_quadratic_minimum_bet_late_floors);
    RUN_TEST(run_greed_doubles_minimum_bet);
    RUN_TEST(run_blocked_greed_does_not_double_minimum_bet);
    RUN_TEST(run_minimum_bet_multiplier_is_applied);
}
