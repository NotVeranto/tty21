#include "test.h"
#include "dealers.h"
#include "rng.h"

#include <string.h>

TEST(dealer_floor_schedule_and_boss_cycle) {
    rngInit(301U);

    ASSERT_EQ_INT(DEALER_CROUPIER,
                  selectDealerForFloor(1, DEALER_CROUPIER));
    ASSERT_EQ_INT(DEALER_ACCOUNTANT,
                  selectDealerForFloor(2, DEALER_CROUPIER));
    ASSERT_EQ_INT(DEALER_CLOCKMAKER,
                  selectDealerForFloor(3, DEALER_CROUPIER));
    ASSERT_EQ_INT(DEALER_BOSS_PIT,
                  selectDealerForFloor(5, DEALER_TWIN));
    ASSERT_EQ_INT(DEALER_BOSS_PRIME,
                  selectDealerForFloor(10, DEALER_TWIN));
    ASSERT_EQ_INT(DEALER_BOSS_SUPREME,
                  selectDealerForFloor(15, DEALER_TWIN));
    ASSERT_EQ_INT(DEALER_BOSS_VOID,
                  selectDealerForFloor(20, DEALER_TWIN));
    ASSERT_EQ_INT(DEALER_BOSS_PIT,
                  selectDealerForFloor(25, DEALER_TWIN));
}

TEST(dealer_random_floor_avoids_previous_dealer) {
    DealerId previous;
    DealerId selected;

    rngInit(304U);

    for (previous = DEALER_CROUPIER;
         previous < DEALER_COUNT;
         previous = (DealerId)(previous + 1)) {
        selected = selectDealerForFloor(4, previous);
        ASSERT_TRUE(selected != previous);
        ASSERT_TRUE(selected >= DEALER_BUTCHER &&
                    selected <= DEALER_TWIN);
    }
}

TEST(dealer_boss_flags_are_exact) {
    DealerId id;
    int expected;

    for (id = DEALER_CROUPIER; id < DEALER_COUNT;
         id = (DealerId)(id + 1)) {
        expected = id == DEALER_BOSS_PIT ||
                   id == DEALER_BOSS_PRIME ||
                   id == DEALER_BOSS_SUPREME ||
                   id == DEALER_BOSS_VOID;
        ASSERT_EQ_INT(expected, dealerIsBoss(id));
    }
}

TEST(dealer_boss_info_has_names_and_two_rules) {
    const DealerInfo *info;

    info = getDealerInfo(DEALER_BOSS_PIT);
    ASSERT_TRUE(strcmp(info->name, "The Pit Boss") == 0);
    ASSERT_TRUE(info->rule1 != NULL && info->rule2 != NULL);

    info = getDealerInfo(DEALER_BOSS_PRIME);
    ASSERT_TRUE(strcmp(info->name, "Croupier Prime") == 0);
    ASSERT_TRUE(info->rule1 != NULL && info->rule2 != NULL);

    info = getDealerInfo(DEALER_BOSS_SUPREME);
    ASSERT_TRUE(strcmp(info->name, "Accountant Supreme") == 0);
    ASSERT_TRUE(info->rule1 != NULL && info->rule2 != NULL);

    info = getDealerInfo(DEALER_BOSS_VOID);
    ASSERT_TRUE(strcmp(info->name, "Void Saint") == 0);
    ASSERT_TRUE(info->rule1 != NULL && info->rule2 != NULL);
}

TEST(dealer_power_rules_are_absent_before_floor_ten) {
    DealerId id;

    for (id = DEALER_CROUPIER; id <= DEALER_TWIN;
         id = (DealerId)(id + 1)) {
        ASSERT_TRUE(dealerPowerRule2(id, 1) == NULL);
        ASSERT_TRUE(dealerPowerRule2(id, 9) == NULL);
    }
}

TEST(dealer_power_rules_cover_non_bosses_except_croupier) {
    DealerId id;

    ASSERT_TRUE(dealerPowerRule2(DEALER_CROUPIER, 10) == NULL);
    for (id = DEALER_ACCOUNTANT; id <= DEALER_TWIN;
         id = (DealerId)(id + 1)) {
        ASSERT_TRUE(dealerPowerRule2(id, 10) != NULL);
    }
}

TEST(dealer_power_rules_are_absent_for_bosses) {
    DealerId id;

    for (id = DEALER_BOSS_PIT; id <= DEALER_BOSS_VOID;
         id = (DealerId)(id + 1)) {
        ASSERT_TRUE(dealerPowerRule2(id, 10) == NULL);
    }
}

TEST(dealer_power_loss_multipliers_by_floor) {
    DealerId id;

    for (id = DEALER_CROUPIER; id < DEALER_COUNT;
         id = (DealerId)(id + 1)) {
        ASSERT_EQ_INT(100, dealerPowerLossMultiplier(id, 9));
        if (id != DEALER_ACCOUNTANT &&
            id != DEALER_SAINT &&
            id != DEALER_MIRROR) {
            ASSERT_EQ_INT(100, dealerPowerLossMultiplier(id, 10));
        }
    }

    ASSERT_EQ_INT(125, dealerPowerLossMultiplier(DEALER_ACCOUNTANT, 10));
    ASSERT_EQ_INT(150, dealerPowerLossMultiplier(DEALER_SAINT, 10));
    ASSERT_EQ_INT(200, dealerPowerLossMultiplier(DEALER_MIRROR, 10));
}

TEST(dealer_target_values) {
    ASSERT_EQ_INT(18, dealerTargetValue(DEALER_CLOCKMAKER));
    ASSERT_EQ_INT(18, dealerTargetValue(DEALER_BOSS_PRIME));
    ASSERT_EQ_INT(17, dealerTargetValue(DEALER_CROUPIER));
    ASSERT_EQ_INT(17, dealerTargetValue(DEALER_BOSS_PIT));
}

TEST(dealer_loss_multipliers) {
    ASSERT_EQ_INT(150, dealerLossMultiplierPercent(DEALER_BUTCHER));
    ASSERT_EQ_INT(150, dealerLossMultiplierPercent(DEALER_BOSS_PIT));
    ASSERT_EQ_INT(100, dealerLossMultiplierPercent(DEALER_CROUPIER));
    ASSERT_EQ_INT(100, dealerLossMultiplierPercent(DEALER_BOSS_VOID));
}

TEST(dealer_blackjack_payout_ratios) {
    ASSERT_EQ_INT(1, dealerBlackjackNumerator(DEALER_VOID));
    ASSERT_EQ_INT(1, dealerBlackjackDenominator(DEALER_VOID));
    ASSERT_EQ_INT(1, dealerBlackjackNumerator(DEALER_BOSS_VOID));
    ASSERT_EQ_INT(1, dealerBlackjackDenominator(DEALER_BOSS_VOID));
    ASSERT_EQ_INT(3, dealerBlackjackNumerator(DEALER_CROUPIER));
    ASSERT_EQ_INT(2, dealerBlackjackDenominator(DEALER_CROUPIER));
    ASSERT_EQ_INT(3, dealerBlackjackNumerator(DEALER_BOSS_PIT));
    ASSERT_EQ_INT(2, dealerBlackjackDenominator(DEALER_BOSS_PIT));
}

TEST(dealer_hidden_total_helper) {
    ASSERT_EQ_INT(1, dealerHidesTotal(DEALER_REAPER));
    ASSERT_EQ_INT(1, dealerHidesTotal(DEALER_BOSS_PIT));
    ASSERT_EQ_INT(0, dealerHidesTotal(DEALER_CROUPIER));
    ASSERT_EQ_INT(0, dealerHidesTotal(DEALER_BOSS_PRIME));
}

TEST(dealer_mirror_on_hit_helper) {
    ASSERT_EQ_INT(1, dealerMirrorsOnHit(DEALER_MIRROR));
    ASSERT_EQ_INT(1, dealerMirrorsOnHit(DEALER_BOSS_PRIME));
    ASSERT_EQ_INT(0, dealerMirrorsOnHit(DEALER_CROUPIER));
    ASSERT_EQ_INT(0, dealerMirrorsOnHit(DEALER_BOSS_VOID));
}

TEST(dealer_shows_next_card_only_for_oracle) {
    DealerId id;

    ASSERT_EQ_INT(1, dealerShowsNextCard(DEALER_ORACLE));
    for (id = DEALER_CROUPIER; id < DEALER_COUNT;
         id = (DealerId)(id + 1)) {
        if (id != DEALER_ORACLE) {
            ASSERT_EQ_INT(0, dealerShowsNextCard(id));
        }
    }
}

TEST(dealer_accountant_aces_count_as_one) {
    ASSERT_EQ_INT(1, dealerAcesCountAsOne(DEALER_ACCOUNTANT));
    ASSERT_EQ_INT(1, dealerAcesCountAsOne(DEALER_BOSS_SUPREME));
    ASSERT_EQ_INT(0, dealerAcesCountAsOne(DEALER_CROUPIER));
    ASSERT_EQ_INT(0, dealerAcesCountAsOne(DEALER_BOSS_PIT));
}

TEST(dealer_forced_double_helper) {
    ASSERT_EQ_INT(1, dealerForcesDouble(DEALER_DEVIL));
    ASSERT_EQ_INT(1, dealerForcesDouble(DEALER_BOSS_SUPREME));
    ASSERT_EQ_INT(0, dealerForcesDouble(DEALER_CROUPIER));
    ASSERT_EQ_INT(0, dealerForcesDouble(DEALER_BOSS_PRIME));
}

TEST(dealer_pure_hand_bonus_helper) {
    ASSERT_EQ_INT(1, dealerPaysPureHandBonus(DEALER_SAINT));
    ASSERT_EQ_INT(1, dealerPaysPureHandBonus(DEALER_BOSS_VOID));
    ASSERT_EQ_INT(0, dealerPaysPureHandBonus(DEALER_CROUPIER));
    ASSERT_EQ_INT(0, dealerPaysPureHandBonus(DEALER_BOSS_PIT));
}

TEST(dealer_hand_count_helper) {
    ASSERT_EQ_INT(2, dealerHandsCount(DEALER_TWIN));
    ASSERT_EQ_INT(1, dealerHandsCount(DEALER_CROUPIER));
    ASSERT_EQ_INT(1, dealerHandsCount(DEALER_BOSS_PIT));
    ASSERT_EQ_INT(1, dealerHandsCount(DEALER_BOSS_PRIME));
    ASSERT_EQ_INT(1, dealerHandsCount(DEALER_BOSS_SUPREME));
    ASSERT_EQ_INT(1, dealerHandsCount(DEALER_BOSS_VOID));
}

void run_dealers_tests(void) {
    RUN_TEST(dealer_floor_schedule_and_boss_cycle);
    RUN_TEST(dealer_random_floor_avoids_previous_dealer);
    RUN_TEST(dealer_boss_flags_are_exact);
    RUN_TEST(dealer_boss_info_has_names_and_two_rules);
    RUN_TEST(dealer_power_rules_are_absent_before_floor_ten);
    RUN_TEST(dealer_power_rules_cover_non_bosses_except_croupier);
    RUN_TEST(dealer_power_rules_are_absent_for_bosses);
    RUN_TEST(dealer_power_loss_multipliers_by_floor);
    RUN_TEST(dealer_target_values);
    RUN_TEST(dealer_loss_multipliers);
    RUN_TEST(dealer_blackjack_payout_ratios);
    RUN_TEST(dealer_hidden_total_helper);
    RUN_TEST(dealer_mirror_on_hit_helper);
    RUN_TEST(dealer_shows_next_card_only_for_oracle);
    RUN_TEST(dealer_accountant_aces_count_as_one);
    RUN_TEST(dealer_forced_double_helper);
    RUN_TEST(dealer_pure_hand_bonus_helper);
    RUN_TEST(dealer_hand_count_helper);
}
