#include "render_table_info.h"

#include "render_canvas.h"
#include "render_control.h"
#include "render_rows.h"
#include "render_scene.h"
#include "render_style.h"
#include "render_oracle.h"
#include "round_rules.h"

#include <stdio.h>
#include <string.h>

/* Strongest category among active twists, mapped to a palette entry. */
static int twistTone(const Game *game) {
    int worst;
    int i;
    TwistCategory category;

    worst = 0; /* 0 favourable, 1 neutral, 2 hostile */
    if (game->run.twistId != TWIST_NONE && !game->run.twistBlocked) {
        category = getTwistInfo(game->run.twistId)->category;
        if (category == TWIST_CAT_HOSTILE) return 2;
        if (category == TWIST_CAT_NEUTRAL) worst = 1;
    }
    for (i = 0; i < game->run.extraTwistCount; i++) {
        category = getTwistInfo(game->run.extraTwists[i])->category;
        if (category == TWIST_CAT_HOSTILE) return 2;
        if (category == TWIST_CAT_NEUTRAL) worst = 1;
    }
    return worst;
}

static const char *hintFor(const Game *game) {
    if (game->state == STATE_BET) {
        return "Type a bet and press Enter.";
    }
    if (game->state == STATE_MENU) {
        return "Keys: C continue   A advance   S shop   Q quit";
    }
    return "";
}

static void drawRules(const Game *game, int x0, int width) {
    const DealerInfo *dealerInfo;
    const char *rule1;
    const char *rule2;
    const char *power;
    char text[256];
    int used;

    dealerInfo = getDealerInfo(game->run.dealerId);
    rule1 = dealerInfo->rule1;
    rule2 = dealerInfo->rule2;
    power = dealerPowerRule2(game->run.dealerId, game->run.floor);
    if (rule2 == NULL && power != NULL) {
        rule2 = power;
    }
    if (rule1 == NULL && rule2 == NULL) {
        return;
    }
    if (rule2 == NULL) {
        snprintf(text, sizeof(text), "%s", rule1);
    } else if (rule1 == NULL) {
        snprintf(text, sizeof(text), "%s", rule2);
    } else {
        snprintf(text, sizeof(text), "%s | %s", rule1, rule2);
    }
    used = canvasPutText(x0, ROW_RULE,
                         rule1 != NULL && rule2 != NULL ? "RULES" : "RULE",
                         ST_BRASS);
    canvasPutFit(x0 + used + 1, ROW_RULE, width - used - 1, text, ST_MUTED);
}

static void drawTwists(const Game *game, int x0, int width) {
    const TwistInfo *twistInfo;
    char text[256];
    char names[256];
    int length;
    int i;
    int showPrimary;
    int showBlocked;
    int showExtras;
    int hasName;
    int x;
    int tone;
    RenderStyle badge;
    RenderStyle body;

    twistInfo = getTwistInfo(game->run.twistId);
    showPrimary = game->run.twistId != TWIST_NONE &&
                  !game->run.twistBlocked;
    showBlocked = game->run.twistId != TWIST_NONE &&
                  game->run.twistBlocked;
    showExtras = 0;
    for (i = 0; i < game->run.extraTwistCount; i++) {
        if (game->run.extraTwists[i] != TWIST_NONE) {
            showExtras = 1;
            break;
        }
    }

    x = x0;
    if (!showPrimary && !showBlocked && !showExtras) {
        if (game->safetyNetArmed) {
            canvasPutText(x, ROW_TWIST, " SAFETY NET ", ST_BADGE_WARN);
        }
        return;
    }
    if (game->safetyNetArmed) {
        x += canvasPutText(x, ROW_TWIST, " SAFETY NET ", ST_BADGE_WARN) + 1;
    }

    tone = twistTone(game);
    badge = tone == 2 ? ST_BADGE_RED : (tone == 1 ? ST_BADGE_GOLD
                                                  : ST_BADGE_OK);
    body = tone == 2 ? ST_BAD : (tone == 1 ? ST_MUTED : ST_OK);

    if ((showPrimary || showBlocked) && !showExtras) {
        snprintf(text, sizeof(text), "%s%s - %s", twistInfo->name,
                 showBlocked ? " (blocked)" : "", twistInfo->description);
        x += canvasPutText(x, ROW_TWIST, " TWIST ", badge) + 1;
        canvasPutFit(x, ROW_TWIST, x0 + width - x, text, body);
        return;
    }

    length = 0;
    names[0] = '\0';
    hasName = 0;
    if (showPrimary || showBlocked) {
        length += snprintf(names, sizeof(names), "%s%s", twistInfo->name,
                           showBlocked ? " (blocked)" : "");
        hasName = 1;
    }
    for (i = 0; i < game->run.extraTwistCount &&
                length < (int)sizeof(names); i++) {
        if (game->run.extraTwists[i] == TWIST_NONE) {
            continue;
        }
        length += snprintf(names + length, sizeof(names) - (size_t)length,
                           "%s%s", hasName ? " | " : "",
                           getTwistInfo(game->run.extraTwists[i])->name);
        hasName = 1;
    }
    if (hasName) {
        x += canvasPutText(x, ROW_TWIST, " TWISTS ", badge) + 1;
        canvasPutFit(x, ROW_TWIST, x0 + width - x, names, body);
    }
}

void renderTableInfoRows(const Game *game, int padding) {
    const char *hint;
    int x0;
    int width;
    int row;

    (void)padding;
    renderHudRegion(&x0, &width);
    for (row = ROW_HINT; row <= ROW_TWIST; row++) {
        canvasFill(x0, row, width, 1, ST_PLAIN);
        canvasTouchRow(row);
    }
    hint = hintFor(game);
    if (hint[0] != '\0') {
        canvasPutFit(x0, ROW_HINT, width, hint, ST_MUTED);
    }
    drawRules(game, x0, width);
    if (game->mode == MODE_ROGUELIKE) {
        drawTwists(game, x0, width);
    }
}

void renderTableInfo(const Game *game, int tablePadding) {
    if (canvasBegin()) {
        renderTableInfoRows(game, tablePadding);
        canvasFlush();
    }
    renderOracleInfo(game, tablePadding);
}
