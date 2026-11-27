#include "test.h"
#include "game.h"
#include "round_actions.h"
#include "round.h"
#include "rng.h"

#include <string.h>

static void setup_action_game(Game *game, int bet) {
    memset(game, 0, sizeof(*game));
    game->mode = MODE_CLASSIC;
    game->state = STATE_PLAYER_TURN;
    game->player.chips = 100;
    game->player.handCount = 1;
    game->player.activeHand = 0;
    game->player.bets[0] = bet;
    game->player.hands[0].count = 2;
    game->player.hands[0].cards[0].rank = TEN;
    game->player.hands[0].cards[1].rank = SIX;
}

TEST(actions_expose_and_apply_surrender_without_terminal_input) {
    Game game;
    ActionId actions[ACTION_COUNT + 1];
    int actionCount;
    int index;
    int surrenderOffered;

    setup_action_game(&game, 1);
    actionCount = getPlayerActions(&game, actions);
    surrenderOffered = 0;
    for (index = 0; index < actionCount; index++) {
        if (actions[index] == ACTION_SURRENDER) surrenderOffered = 1;
    }
    ASSERT_TRUE(surrenderOffered);

    applyPlayerAction(&game, ACTION_SURRENDER);
    ASSERT_EQ_INT(99, game.player.chips);
    ASSERT_EQ_INT(1, game.player.surrendered);
    ASSERT_EQ_INT(STATE_RESULT, game.state);

    setup_action_game(&game, 3);
    applyPlayerAction(&game, ACTION_SURRENDER);
    ASSERT_EQ_INT(98, game.player.chips);
    ASSERT_EQ_INT(1, game.player.surrendered);
    ASSERT_EQ_INT(STATE_RESULT, game.state);
}

TEST(actions_reject_unavailable_and_out_of_turn_requests) {
    Game game;
    ActionId actions[ACTION_COUNT + 1];

    setup_action_game(&game, 10);
    ASSERT_TRUE(getPlayerActions(&game, actions) > 0);
    applyPlayerAction(&game, ACTION_SPLIT);
    ASSERT_EQ_INT(100, game.player.chips);
    ASSERT_EQ_INT(1, game.player.handCount);
    ASSERT_EQ_INT(STATE_PLAYER_TURN, game.state);

    game.state = STATE_MENU;
    ASSERT_EQ_INT(0, getPlayerActions(&game, actions));
    applyPlayerAction(&game, ACTION_STAND);
    ASSERT_EQ_INT(STATE_MENU, game.state);
    ASSERT_EQ_INT(100, game.player.chips);
}

TEST(deterministic_round_start_uses_production_deal_logic) {
    Game game;

    rngInit(12345, 1);
    initGame(&game, MODE_CLASSIC);
    game.fastMode = 1;
    game.saveEnabled = 0;
    ASSERT_EQ_INT(0, startRoundWithBet(&game, 0, 0, 0));
    ASSERT_EQ_INT(STATE_BET, game.state);
    ASSERT_EQ_INT(0, game.roundNumber);

    ASSERT_EQ_INT(1, startRoundWithBet(&game, 10, 0, 0));
    ASSERT_EQ_INT(1, game.roundNumber);
    ASSERT_EQ_INT(10, game.player.bets[0]);
    ASSERT_EQ_INT(2, game.player.hands[0].count);
    ASSERT_TRUE(game.roundInProgress);
    ASSERT_TRUE(game.state == STATE_PLAYER_TURN ||
                game.state == STATE_DEALER_TURN ||
                game.state == STATE_RESULT);
}


TEST(actions_insurance_stake_is_not_reserved_twice_for_double) {
    Game game;
    ActionId actions[ACTION_COUNT + 1];
    int count;
    int i;
    int found = 0;

    setup_action_game(&game, 40);
    game.player.chips = 80;
    game.player.insuranceBet = 20;
    count = getPlayerActions(&game, actions);
    for (i = 0; i < count; i++) {
        if (actions[i] == ACTION_DOUBLE) found = 1;
    }
    ASSERT_TRUE(found);
}

TEST(actions_insurance_stake_is_not_reserved_twice_for_split) {
    Game game;
    ActionId actions[ACTION_COUNT + 1];
    int count;
    int i;
    int found = 0;

    setup_action_game(&game, 40);
    game.player.chips = 80;
    game.player.insuranceBet = 20;
    game.player.hands[0].cards[1].rank = TEN;
    count = getPlayerActions(&game, actions);
    for (i = 0; i < count; i++) {
        if (actions[i] == ACTION_SPLIT) found = 1;
    }
    ASSERT_TRUE(found);
}

TEST(actions_do_not_offer_draw_actions_without_enough_cards) {
    Game game;
    ActionId actions[ACTION_COUNT + 1];
    int count;
    int i;
    int hasHit = 0;
    int hasDouble = 0;
    int hasSplit = 0;

    setup_action_game(&game, 10);
    game.player.chips = 100;
    game.player.hands[0].cards[1].rank = TEN;
    game.deck.top = DECK_SIZE - DECK_BURN_SIZE - 1;
    count = getPlayerActions(&game, actions);
    for (i = 0; i < count; i++) {
        if (actions[i] == ACTION_HIT) hasHit = 1;
        if (actions[i] == ACTION_DOUBLE) hasDouble = 1;
        if (actions[i] == ACTION_SPLIT) hasSplit = 1;
    }
    ASSERT_EQ_INT(0, hasHit);
    ASSERT_EQ_INT(0, hasDouble);
    ASSERT_EQ_INT(0, hasSplit);

    game.deck.top = DECK_SIZE - DECK_BURN_SIZE;
    count = getPlayerActions(&game, actions);
    hasHit = 0;
    for (i = 0; i < count; i++) {
        if (actions[i] == ACTION_HIT) hasHit = 1;
    }
    ASSERT_EQ_INT(0, hasHit);
}

void run_action_tests(void) {
    RUN_TEST(actions_expose_and_apply_surrender_without_terminal_input);
    RUN_TEST(actions_do_not_offer_draw_actions_without_enough_cards);
    RUN_TEST(actions_reject_unavailable_and_out_of_turn_requests);
    RUN_TEST(actions_insurance_stake_is_not_reserved_twice_for_double);
    RUN_TEST(actions_insurance_stake_is_not_reserved_twice_for_split);
    RUN_TEST(deterministic_round_start_uses_production_deal_logic);
}
