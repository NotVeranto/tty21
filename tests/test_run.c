#include "test.h"
#include "run.h"
#include "game.h"
#include "round.h"
#include "round_rules.h"
#include "rng.h"

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

TEST(shop_reroll_replaces_every_offered_perk) {
    Game game;
    PerkId previous[SHOP_POOL_SIZE];
    int i;
    int j;

    memset(&game, 0, sizeof(game));
    game.mode = MODE_ROGUELIKE;
    game.minBetMultiplier = 1;
    game.run.perkLockedId = -1;
    rngInit(12345U, 1);
    generateShopPool(&game);
    ASSERT_EQ_INT(SHOP_POOL_SIZE, game.run.shopPerkCount);
    for (i = 0; i < game.run.shopPerkCount; i++) {
        previous[i] = game.run.shopPerks[i];
    }

    ASSERT_TRUE(canRerollShopPool(&game));
    ASSERT_EQ_INT(1, rerollShopPool(&game));
    for (i = 0; i < game.run.shopPerkCount; i++) {
        for (j = 0; j < SHOP_POOL_SIZE; j++) {
            ASSERT_TRUE(game.run.shopPerks[i] != previous[j]);
        }
    }
}

TEST(shop_reroll_is_unavailable_without_alternatives) {
    Game game;
    PerkId previous[SHOP_POOL_SIZE];
    int i;

    memset(&game, 0, sizeof(game));
    game.mode = MODE_ROGUELIKE;
    game.minBetMultiplier = 1;
    game.run.perkLockedId = -1;
    rngInit(54321U, 1);
    generateShopPool(&game);
    for (i = 0; i < PERK_COUNT; i++) {
        game.player.perkOwned[i] = 1;
    }
    for (i = 0; i < game.run.shopPerkCount; i++) {
        previous[i] = game.run.shopPerks[i];
        game.player.perkOwned[game.run.shopPerks[i]] = 0;
    }

    ASSERT_EQ_INT(0, canRerollShopPool(&game));
    ASSERT_EQ_INT(0, rerollShopPool(&game));
    for (i = 0; i < game.run.shopPerkCount; i++) {
        ASSERT_EQ_INT(previous[i], game.run.shopPerks[i]);
    }
}

TEST(perk_lock_requires_an_active_late_floor_event) {
    Game game;

    memset(&game, 0, sizeof(game));
    game.run.floor = 1;
    game.run.perkLockedId = PERK_SECOND_WIND;

    ASSERT_EQ_INT(0, isPerkLockedThisFloor(&game, PERK_SECOND_WIND));

    game.run.floor = 10;
    game.run.perkLockedUsed = 1;
    ASSERT_EQ_INT(1, isPerkLockedThisFloor(&game, PERK_SECOND_WIND));

    game.player.perkOwned[PERK_SECOND_WIND] = 1;
    ASSERT_EQ_INT(0, isPerkLockedThisFloor(&game, PERK_SECOND_WIND));
}

TEST(shop_pool_handles_empty_inventory) {
    Game game;
    int i;

    memset(&game, 0, sizeof(game));
    game.run.perkLockedId = -1;
    for (i = 0; i < PERK_COUNT; i++) {
        game.player.perkOwned[i] = 1;
    }

    generateShopPool(&game);

    ASSERT_EQ_INT(0, game.run.shopPerkCount);
    ASSERT_EQ_INT(0, canRerollShopPool(&game));
    ASSERT_EQ_INT(0, rerollShopPool(&game));
}

TEST(shop_pool_contains_only_remaining_eligible_perk) {
    Game game;
    int i;
    int remaining = -1;

    memset(&game, 0, sizeof(game));
    game.run.perkLockedId = -1;
    for (i = 0; i < PERK_COUNT; i++) {
        game.player.perkOwned[i] = 1;
    }
    game.player.perkOwned[PERK_BANKROLL] = 0;

    generateShopPool(&game);

    ASSERT_EQ_INT(1, game.run.shopPerkCount);
    remaining = (int)game.run.shopPerks[0];
    ASSERT_EQ_INT(PERK_BANKROLL, remaining);
}

TEST(shop_perk_purchase_rejects_insufficient_marks_without_mutation) {
    Game game;

    memset(&game, 0, sizeof(game));
    game.run.marks = perkCost(PERK_PEEK) - 1;

    ASSERT_EQ_INT(0, purchasePerk(&game, PERK_PEEK));
    ASSERT_EQ_INT(perkCost(PERK_PEEK) - 1, game.run.marks);
    ASSERT_EQ_INT(0, game.player.perkOwned[PERK_PEEK]);
    ASSERT_EQ_INT(0, game.player.perkCharges[PERK_PEEK]);
}

TEST(shop_perk_purchase_rejects_owned_and_locked_perks) {
    Game game;

    memset(&game, 0, sizeof(game));
    game.run.marks = 100;
    game.player.perkOwned[PERK_PEEK] = 1;
    ASSERT_EQ_INT(0, purchasePerk(&game, PERK_PEEK));
    ASSERT_EQ_INT(100, game.run.marks);

    game.player.perkOwned[PERK_PEEK] = 0;
    game.run.floor = 10;
    game.run.perkLockedUsed = 1;
    game.run.perkLockedId = PERK_PEEK;
    ASSERT_EQ_INT(0, purchasePerk(&game, PERK_PEEK));
    ASSERT_EQ_INT(100, game.run.marks);
    ASSERT_EQ_INT(0, game.player.perkOwned[PERK_PEEK]);
}

TEST(shop_perk_purchase_charges_only_after_success) {
    Game game;

    memset(&game, 0, sizeof(game));
    game.run.marks = perkCost(PERK_PEEK);
    ASSERT_EQ_INT(1, purchasePerk(&game, PERK_PEEK));
    ASSERT_EQ_INT(0, game.run.marks);
    ASSERT_EQ_INT(1, game.player.perkOwned[PERK_PEEK]);
    ASSERT_EQ_INT(1, game.player.perkCharges[PERK_PEEK]);
}

TEST(shop_chip_services_reject_unaffordable_actions_without_mutation) {
    Game game;

    memset(&game, 0, sizeof(game));
    game.player.chips = 9;
    game.run.marks = 7;
    ASSERT_EQ_INT(0, purchaseShopChipService(&game, 'v', 2));
    ASSERT_EQ_INT(9, game.player.chips);
    ASSERT_EQ_INT(7, game.run.marks);

    game.player.chips = 49;
    ASSERT_EQ_INT(0, purchaseShopChipService(&game, 'n', 2));
    ASSERT_EQ_INT(49, game.player.chips);
    ASSERT_EQ_INT(0, game.safetyNetArmed);

    game.player.chips = 9;
    ASSERT_EQ_INT(0, purchaseShopChipService(&game, 't', 2));
    ASSERT_EQ_INT(9, game.player.chips);
    ASSERT_EQ_INT(0, game.tippedThisRound);
}

TEST(shop_chip_services_preserve_minimum_bet) {
    Game game;

    memset(&game, 0, sizeof(game));
    game.player.chips = 12;
    game.run.marks = 3;
    ASSERT_EQ_INT(0, purchaseShopChipService(&game, 'v', 3));
    ASSERT_EQ_INT(12, game.player.chips);
    ASSERT_EQ_INT(3, game.run.marks);

    game.player.chips = 52;
    ASSERT_EQ_INT(0, purchaseShopChipService(&game, 'n', 3));
    ASSERT_EQ_INT(52, game.player.chips);
    ASSERT_EQ_INT(0, game.safetyNetArmed);
}

TEST(shop_chip_services_apply_effects_after_payment) {
    Game game;

    memset(&game, 0, sizeof(game));
    game.player.chips = 20;
    ASSERT_EQ_INT(1, purchaseShopChipService(&game, 'v', 2));
    ASSERT_EQ_INT(10, game.player.chips);
    ASSERT_EQ_INT(2, game.run.marks);
    ASSERT_EQ_INT(2, game.totalMarksEarned);

    ASSERT_EQ_INT(1, purchaseShopChipService(&game, 't', 2));
    ASSERT_EQ_INT(0, game.player.chips);
    ASSERT_EQ_INT(1, game.tippedThisRound);

    game.player.chips = 60;
    ASSERT_EQ_INT(1, purchaseShopChipService(&game, 'n', 2));
    ASSERT_EQ_INT(10, game.player.chips);
    ASSERT_EQ_INT(1, game.safetyNetArmed);
    ASSERT_EQ_INT(0, purchaseShopChipService(&game, 'n', 2));
    ASSERT_EQ_INT(10, game.player.chips);
}

void run_run_tests(void) {
    RUN_TEST(run_classic_minimum_bet_is_always_one);
    RUN_TEST(run_roguelike_minimum_bet_by_floor);
    RUN_TEST(run_roguelike_quadratic_minimum_bet_late_floors);
    RUN_TEST(run_roguelike_minimum_bet_floor_two);
    RUN_TEST(run_greed_doubles_minimum_bet);
    RUN_TEST(run_blocked_greed_does_not_double_minimum_bet);
    RUN_TEST(run_minimum_bet_multiplier_is_applied);
    RUN_TEST(perk_lock_requires_an_active_late_floor_event);
    RUN_TEST(shop_reroll_replaces_every_offered_perk);
    RUN_TEST(shop_reroll_is_unavailable_without_alternatives);
    RUN_TEST(shop_pool_handles_empty_inventory);
    RUN_TEST(shop_pool_contains_only_remaining_eligible_perk);
    RUN_TEST(shop_perk_purchase_rejects_insufficient_marks_without_mutation);
    RUN_TEST(shop_perk_purchase_rejects_owned_and_locked_perks);
    RUN_TEST(shop_perk_purchase_charges_only_after_success);
    RUN_TEST(shop_chip_services_reject_unaffordable_actions_without_mutation);
    RUN_TEST(shop_chip_services_preserve_minimum_bet);
    RUN_TEST(shop_chip_services_apply_effects_after_payment);
}
