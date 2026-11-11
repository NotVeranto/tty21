#define _POSIX_C_SOURCE 200809L

#include "save.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define SAVE_MAGIC "TTY21SV1"
#define SAVE_MAGIC_LEN 8
#define SAVE_VERSION 1U
#define SAVE_PATH_BUFFER 1024

static uint32_t checksumBytes(const void *data, size_t length) {
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t sum = 0;
    size_t i;

    for (i = 0; i < length; i++) sum += bytes[i];
    return sum;
}

static int isBoolean(int value) {
    return value == 0 || value == 1;
}

static int isValidCard(const Card *card) {
    return card->suit >= HEARTS && card->suit <= SPADES &&
           card->rank >= ACE && card->rank <= KING &&
           card->cursed >= CURSED_NONE && card->cursed <= CURSED_ECHO;
}

static int isValidHand(const Hand *hand) {
    int cardIndex;

    if (hand->count < 0 || hand->count > MAX_HAND_SIZE) {
        return 0;
    }

    for (cardIndex = 0; cardIndex < hand->count; cardIndex++) {
        if (!isValidCard(&hand->cards[cardIndex])) {
            return 0;
        }
    }

    return 1;
}

static int isValidGame(const Game *game) {
    int i;

    if (game->mode != MODE_ROGUELIKE) return 0;
    if (game->state != STATE_BET && game->state != STATE_MENU) return 0;
    if (game->player.chips < 0) return 0;
    if (game->player.handCount < 1 ||
        game->player.handCount > MAX_PLAYER_HANDS) return 0;
    if (game->player.activeHand < 0 ||
        game->player.activeHand > game->player.handCount) return 0;
    if (game->dealer.handCount < 1 || game->dealer.handCount > 2) return 0;
    if (game->run.floor < 1 || game->run.marks < 0) return 0;
    if (game->run.dealerId < 0 || game->run.dealerId >= DEALER_COUNT) return 0;
    if (game->run.previousDealerId < 0 ||
        game->run.previousDealerId >= DEALER_COUNT) return 0;
    if (game->run.twistId < 0 || game->run.twistId >= TWIST_COUNT) return 0;
    if (game->run.previousTwistId < 0 ||
        game->run.previousTwistId >= TWIST_COUNT) return 0;
    if (game->run.perkLockedId < -1 ||
        game->run.perkLockedId >= PERK_COUNT) return 0;
    if (game->log.count < 0 || game->log.count > LOG_SIZE) return 0;
    if (game->run.shopPerkCount < 0 ||
        game->run.shopPerkCount > SHOP_POOL_SIZE) return 0;
    if (game->loanDueFloor < 0 || game->loanAmount < 0) return 0;
    if (game->run.roundsOnFloor < 0 || game->roundNumber < 0) return 0;
    if (game->winStreak < 0 || game->lossStreak < 0) return 0;
    if (game->deck.top < 0 || game->deck.top > DECK_SIZE) return 0;
    if (game->run.extraTwistCount < 0 ||
        game->run.extraTwistCount > MAX_EXTRA_TWISTS) return 0;
    if (game->minBetMultiplier < 1 || game->minBetMultiplier > 2) return 0;
    if (game->fatigueTier < 0 || game->fatigueTier > 100) return 0;
    if (game->bossRulePaid < 0 || game->totalFloorsReached < 0 ||
        game->totalBossesDefeated < 0 || game->totalMarksEarned < 0) return 0;
    if (game->player.insuranceBet < 0) return 0;

    if (!isBoolean(game->run.twistBlocked) ||
        !isBoolean(game->run.twistRerolled) ||
        !isBoolean(game->run.bossRewarded) ||
        !isBoolean(game->run.shopPerksRerolled) ||
        !isBoolean(game->run.coldSnapUsed) ||
        !isBoolean(game->run.coldSnapActive) ||
        !isBoolean(game->run.perkLockedUsed) ||
        !isBoolean(game->saveEnabled) ||
        !isBoolean(game->playerQuit) ||
        !isBoolean(game->roundInProgress) ||
        !isBoolean(game->peekActive) ||
        !isBoolean(game->fastMode) ||
        !isBoolean(game->dealerEnraged) ||
        !isBoolean(game->freePeekReady) ||
        !isBoolean(game->safetyNetArmed) ||
        !isBoolean(game->tippedThisRound) ||
        !isBoolean(game->roundHadNaturalBj) ||
        !isBoolean(game->bossRuleDisabled) ||
        !isBoolean(game->player.surrendered) ||
        !isBoolean(game->player.evenMoneyTaken)) return 0;

    for (i = 0; i < DECK_SIZE; i++) {
        if (!isValidCard(&game->deck.cards[i])) return 0;
    }

    for (i = 0; i < MAX_PLAYER_HANDS; i++) {
        if (!isValidHand(&game->player.hands[i])) return 0;
        if (game->player.bets[i] < 0) return 0;
        if (!isBoolean(game->player.isSplitAce[i]) ||
            !isBoolean(game->player.usedDouble[i]) ||
            !isBoolean(game->player.usedSplit[i])) return 0;
    }

    for (i = 0; i < 2; i++) {
        if (!isValidHand(&game->dealer.hands[i])) return 0;
    }

    for (i = 0; i < PERK_COUNT; i++) {
        if (!isBoolean(game->player.perkOwned[i]) ||
            game->player.perkCharges[i] < 0) return 0;
    }

    for (i = 0; i < game->run.extraTwistCount; i++) {
        if (game->run.extraTwists[i] < 0 ||
            game->run.extraTwists[i] >= TWIST_COUNT) return 0;
    }

    for (i = 0; i < game->run.shopPerkCount; i++) {
        if (game->run.shopPerks[i] < 0 ||
            game->run.shopPerks[i] >= PERK_COUNT) return 0;
    }

    for (i = 0; i < game->log.count; i++) {
        if (game->log.styles[i] < MSG_DEFAULT ||
            game->log.styles[i] > MSG_DEALER) return 0;
    }

    return 1;
}

static int makeDirs(const char *path) {
    char buf[SAVE_PATH_BUFFER];
    size_t len;
    size_t i;

    len = strlen(path);
    if (len == 0 || len >= sizeof(buf)) return -1;
    memcpy(buf, path, len + 1);

    for (i = 1; i < len; i++) {
        if (buf[i] == '/') {
            buf[i] = '\0';
            if (mkdir(buf, 0700) != 0 && errno != EEXIST) return -1;
            buf[i] = '/';
        }
    }
    if (mkdir(buf, 0700) != 0 && errno != EEXIST) return -1;
    return 0;
}

static int resolveSavePath(char *out, size_t outSize) {
    const char *xdg = getenv("XDG_DATA_HOME");
    const char *home;
    int written;

    if (xdg != NULL && xdg[0] == '/') {
        written = snprintf(out, outSize, "%s/tty21/save.dat", xdg);
    } else {
        home = getenv("HOME");
        if (home == NULL || home[0] == '\0') return -1;
        written = snprintf(out, outSize,
                           "%s/.local/share/tty21/save.dat", home);
    }
    if (written < 0 || (size_t)written >= outSize) return -1;
    return 0;
}

int saveExists(void) {
    char path[SAVE_PATH_BUFFER];
    FILE *fp;

    if (resolveSavePath(path, sizeof(path)) != 0) return 0;
    fp = fopen(path, "rb");
    if (fp == NULL) return 0;
    fclose(fp);
    return 1;
}

int saveGame(const Game *game) {
    char path[SAVE_PATH_BUFFER];
    char tmpPath[SAVE_PATH_BUFFER];
    char dir[SAVE_PATH_BUFFER];
    uint8_t header[SAVE_MAGIC_LEN + 12];
    uint32_t version = SAVE_VERSION;
    uint32_t bodySize;
    uint32_t checksum;
    FILE *fp;
    size_t written;

    if (game->mode == MODE_CLASSIC || !game->saveEnabled) return 0;
    if (resolveSavePath(path, sizeof(path)) != 0) return -1;
    if (snprintf(dir, sizeof(dir), "%s", path) >= (int)sizeof(dir)) return -1;
    {
        char *slash = strrchr(dir, '/');
        if (slash == NULL) return -1;
        *slash = '\0';
    }
    if (makeDirs(dir) != 0) return -1;

    // v1 stores raw Game bytes. This is intentionally not portable across architectures with different struct alignment or endianness, and a Game layout change invalidates existing saves.
    bodySize = (uint32_t)sizeof(Game);
    checksum = checksumBytes(game, sizeof(Game));
    memcpy(header, SAVE_MAGIC, SAVE_MAGIC_LEN);
    memcpy(header + SAVE_MAGIC_LEN, &version, sizeof(version));
    memcpy(header + SAVE_MAGIC_LEN + 4, &bodySize, sizeof(bodySize));
    memcpy(header + SAVE_MAGIC_LEN + 8, &checksum, sizeof(checksum));

    if (snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", path) >=
        (int)sizeof(tmpPath)) return -1;

    fp = fopen(tmpPath, "wb");
    if (fp == NULL) return -1;
    written = fwrite(header, 1, sizeof(header), fp);
    if (written != sizeof(header)) {
        fclose(fp);
        unlink(tmpPath);
        return -1;
    }
    written = fwrite(game, 1, sizeof(Game), fp);
    if (written != sizeof(Game)) {
        fclose(fp);
        unlink(tmpPath);
        return -1;
    }
    if (fflush(fp) != 0) {
        fclose(fp);
        unlink(tmpPath);
        return -1;
    }
    if (fclose(fp) != 0) {
        unlink(tmpPath);
        return -1;
    }
    if (rename(tmpPath, path) != 0) {
        unlink(tmpPath);
        return -1;
    }
    return 0;
}

int loadGame(Game *game) {
    char path[SAVE_PATH_BUFFER];
    uint8_t header[SAVE_MAGIC_LEN + 12];
    uint32_t version;
    uint32_t bodySize;
    uint32_t checksum;
    Game temp;
    FILE *fp;
    size_t bytesRead;
    int extra;

    if (resolveSavePath(path, sizeof(path)) != 0) return -1;
    fp = fopen(path, "rb");
    if (fp == NULL) return -1;

    bytesRead = fread(header, 1, sizeof(header), fp);
    if (bytesRead != sizeof(header) ||
        memcmp(header, SAVE_MAGIC, SAVE_MAGIC_LEN) != 0) {
        fclose(fp);
        return -1;
    }
    memcpy(&version, header + SAVE_MAGIC_LEN, sizeof(version));
    memcpy(&bodySize, header + SAVE_MAGIC_LEN + 4, sizeof(bodySize));
    memcpy(&checksum, header + SAVE_MAGIC_LEN + 8, sizeof(checksum));
    if (version != SAVE_VERSION || bodySize != (uint32_t)sizeof(Game)) {
        fclose(fp);
        return -1;
    }

    bytesRead = fread(&temp, 1, sizeof(Game), fp);
    extra = fgetc(fp);
    fclose(fp);
    if (bytesRead != sizeof(Game) || extra != EOF) return -1;
    if (checksumBytes(&temp, sizeof(Game)) != checksum) return -1;

    if (!isValidGame(&temp)) return -1;

    *game = temp;
    return 0;
}

void deleteSave(void) {
    char path[SAVE_PATH_BUFFER];
    if (resolveSavePath(path, sizeof(path)) == 0) unlink(path);
}
