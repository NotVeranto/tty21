#include "test.h"
#include "run.h"
#include "game.h"
#include "round.h"
#include "round_rules.h"

#include <string.h>

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

    ASSERT_EQ_INT(2, getMinBet(&game, 1, MODE_ROGUELIKE));
    ASSERT_EQ_INT(4, getMinBet(&game, 3, MODE_ROGUELIKE));
    ASSERT_EQ_INT(10, getMinBet(&game, 5, MODE_ROGUELIKE));
}

TEST(run_roguelike_quadratic_minimum_bet_late_floors) {
    Game game;

    test_init_game_for_min_bet(&game);

    ASSERT_EQ_INT(52, getMinBet(&game, 10, MODE_ROGUELIKE));
    ASSERT_EQ_INT(100, getMinBet(&game, 15, MODE_ROGUELIKE));
}

TEST(run_roguelike_minimum_bet_floor_two) {
    Game game;

    test_init_game_for_min_bet(&game);

    ASSERT_EQ_INT(4, getMinBet(&game, 2, MODE_ROGUELIKE));
}

TEST(run_greed_doubles_minimum_bet) {
    Game game;

    test_init_game_for_min_bet(&game);
    game.run.twistId = TWIST_GREED;

    ASSERT_EQ_INT(20, getMinBet(&game, 5, MODE_ROGUELIKE));
}

TEST(run_blocked_greed_does_not_double_minimum_bet) {
    Game game;

    test_init_game_for_min_bet(&game);
    game.run.twistId = TWIST_GREED;
    game.run.twistBlocked = 1;

    ASSERT_EQ_INT(10, getMinBet(&game, 5, MODE_ROGUELIKE));
}

TEST(run_minimum_bet_multiplier_is_applied) {
    Game game;

    test_init_game_for_min_bet(&game);
    game.minBetMultiplier = 2;

    ASSERT_EQ_INT(4, getMinBet(&game, 1, MODE_ROGUELIKE));
    ASSERT_EQ_INT(20, getMinBet(&game, 5, MODE_ROGUELIKE));
}

void run_run_tests(void) {
    RUN_TEST(run_classic_minimum_bet_is_always_one);
    RUN_TEST(run_roguelike_minimum_bet_by_floor);
    RUN_TEST(run_roguelike_quadratic_minimum_bet_late_floors);
    RUN_TEST(run_roguelike_minimum_bet_floor_two);
    RUN_TEST(run_greed_doubles_minimum_bet);
    RUN_TEST(run_blocked_greed_does_not_double_minimum_bet);
    RUN_TEST(run_minimum_bet_multiplier_is_applied);
}
