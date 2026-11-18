#include "render_table_info.h"

#include "render_control.h"
#include "render_rows.h"
#include "render_style.h"
#include "render_oracle.h"
#include "round_rules.h"

#include <stdio.h>
#include <string.h>

// Moves the cursor to a table row and left-aligns content with the table.
static void moveToTableRow(int row, int padding) {
    printf(SCR_CURSOR, row, padding + 1);
}


// Returns the strongest category among active twists for banner coloring.
static const char *twistBannerColor(const Game *game) {
    TwistCategory worst = TWIST_CAT_FAVORABLE;
    int i;

    if (game->run.twistId != TWIST_NONE &&
        !game->run.twistBlocked) {
        TwistCategory cat =
            getTwistInfo(game->run.twistId)->category;
        if (cat == TWIST_CAT_HOSTILE) return scrRed();
        if (cat == TWIST_CAT_NEUTRAL) worst = TWIST_CAT_NEUTRAL;
    }

    for (i = 0; i < game->run.extraTwistCount; i++) {
        TwistCategory cat =
            getTwistInfo(game->run.extraTwists[i])->category;
        if (cat == TWIST_CAT_HOSTILE) return scrRed();
        if (cat == TWIST_CAT_NEUTRAL) worst = TWIST_CAT_NEUTRAL;
    }

    if (worst == TWIST_CAT_NEUTRAL) return scrDim();
    return scrGreen();
}

void renderTableInfo(const Game *game,
                     int tablePadding) {
    const DealerInfo *dealerInfo;
    const TwistInfo *twistInfo;
    const char *hintText;
    int i;
    int len;
    char rulesText[256];
    char twistsLine[256];

    dealerInfo = getDealerInfo(game->run.dealerId);
    twistInfo = getTwistInfo(game->run.twistId);

        if (game->state == STATE_BET) {
            hintText = "Type a bet and press Enter.";
        } else if (game->state == STATE_PLAYER_TURN) {
            hintText = "";
        } else if (game->state == STATE_MENU) {
            hintText = "Keys: C continue   A advance   S shop   Q quit";
        } else {
            hintText = "";
        }

        moveToTableRow(ROW_HINT, tablePadding);
        printf(SCR_ERASE_EOL);

        if (hintText[0] != '\0') {
            printf("%s%s%s", scrDim(), hintText, scrReset());
        }

        moveToTableRow(ROW_RULE, tablePadding);
        printf(SCR_ERASE_EOL);

        {
            const char *rule1 = dealerInfo->rule1;
            const char *rule2 = dealerInfo->rule2;
            const char *power = dealerPowerRule2(game->run.dealerId, game->run.floor);
            if (rule2 == NULL && power != NULL) rule2 = power;
            if (rule1 != NULL || rule2 != NULL) {
            if (rule2 == NULL) {
                snprintf(rulesText,
                         sizeof(rulesText),
                         "Rule: %s",
                         rule1);
            } else if (rule1 == NULL) {
                snprintf(rulesText, sizeof(rulesText), "Rule: %s", rule2);
            } else {
                snprintf(rulesText, sizeof(rulesText), "Rules: %s | %s", rule1, rule2);
            }
            printf("%s%s%s", scrDim(), rulesText, scrReset());
            }
        }

        moveToTableRow(ROW_TWIST, tablePadding);
        printf(SCR_ERASE_EOL);

        if (game->mode == MODE_ROGUELIKE) {
            int showPrimary;
            int showExtras;
            int showBlockedPrimary;
            int hasTwistName;

            showPrimary = game->run.twistId != TWIST_NONE &&
                          !game->run.twistBlocked;
            showBlockedPrimary = game->run.twistId != TWIST_NONE &&
                                 game->run.twistBlocked;
            showExtras = 0;

            for (i = 0; i < game->run.extraTwistCount; i++) {
                if (game->run.extraTwists[i] != TWIST_NONE) {
                    showExtras = 1;
                    break;
                }
            }

            twistsLine[0] = '\0';

            if (!showPrimary &&
                !showBlockedPrimary &&
                !showExtras) {
                if (game->safetyNetArmed) {
                    printf("%sSAFETY NET%s",
                           scrDim(),
                           scrReset());
                } else {
                    printf(SCR_ERASE_EOL);
                }
            } else if ((showPrimary || showBlockedPrimary) &&
                       !showExtras) {
                snprintf(twistsLine,
                         sizeof(twistsLine),
                         "Twist: %s%s - %s",
                         twistInfo->name,
                         showBlockedPrimary ? " (blocked)" : "",
                         twistInfo->description);
                printf("%s%s%s%s",
                       twistBannerColor(game),
                       game->safetyNetArmed ? "SAFETY NET  |  " : "",
                       twistsLine,
                       scrReset());
            } else {
                len = snprintf(twistsLine,
                               sizeof(twistsLine),
                               "Twists: ");
                hasTwistName = 0;

                if (showPrimary || showBlockedPrimary) {
                    len += snprintf(twistsLine + len,
                                    sizeof(twistsLine) - (size_t)len,
                                    "%s%s",
                                    twistInfo->name,
                                    showBlockedPrimary ? " (blocked)" : "");
                    hasTwistName = 1;
                }

                for (i = 0;
                     i < game->run.extraTwistCount &&
                     len < (int)sizeof(twistsLine);
                     i++) {
                    const TwistInfo *extraInfo;

                    if (game->run.extraTwists[i] == TWIST_NONE) {
                        continue;
                    }

                    extraInfo = getTwistInfo(game->run.extraTwists[i]);

                    if (hasTwistName) {
                        len += snprintf(twistsLine + len,
                                        sizeof(twistsLine) - (size_t)len,
                                        " | ");
                    }

                    len += snprintf(twistsLine + len,
                                    sizeof(twistsLine) - (size_t)len,
                                    "%s",
                                    extraInfo->name);
                    hasTwistName = 1;
                }

                if (hasTwistName) {
                    printf("%s%s%s%s",
                           twistBannerColor(game),
                           game->safetyNetArmed ? "SAFETY NET  |  " : "",
                           twistsLine,
                           scrReset());
                } else if (game->safetyNetArmed) {
                    printf("%sSAFETY NET%s",
                           scrDim(),
                           scrReset());
                } else {
                    printf(SCR_ERASE_EOL);
                }
            }
        } else {
            printf(SCR_ERASE_EOL);
        }

        renderOracleInfo(game, tablePadding);

 }
