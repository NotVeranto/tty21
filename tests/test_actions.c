#include "test.h"
#include "game.h"
#include "round_actions.h"

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

void run_action_tests(void) {
    RUN_TEST(actions_expose_and_apply_surrender_without_terminal_input);
}
