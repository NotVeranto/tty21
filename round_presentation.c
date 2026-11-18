#include "round_presentation.h"

#include "render_table.h"
#include "render_actions.h"
#include "render_help.h"
#include "render_rows.h"

void roundPresentTable(const Game *game, int hideDealerCard) {
    renderTable(game, hideDealerCard);
}

void roundPresentMessage(const char *message,
                         RoundMessageLocation location,
                         MessageStyle style) {
    int row = location == ROUND_MESSAGE_RESULT ? ROW_RESULT : ROW_PROMPT;
    renderTableMessage(message, row, style);
}

void roundPause(const Game *game, int ms) {
    renderPause(game, ms);
}

void roundPresentActionBar(const ActionId *actions,
                           int actionCount,
                           int selectedIndex,
                           int showHelp) {
    renderActionBar(actions, actionCount, selectedIndex, ROW_PROMPT, showHelp);
}

void roundPresentHelp(const Game *game) {
    renderHelpOverlay(game);
}
