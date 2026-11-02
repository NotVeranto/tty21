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

    /*
     * v1 stores raw Game bytes. This is intentionally not portable across
     * architectures with different struct alignment or endianness, and a
     * Game layout change invalidates existing saves.
     */
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

    if (temp.mode != MODE_ROGUELIKE) return -1;
    if (temp.state != STATE_BET && temp.state != STATE_MENU) return -1;
    if (temp.player.chips < 0) return -1;
    if (temp.player.handCount < 1 ||
        temp.player.handCount > MAX_PLAYER_HANDS) return -1;
    if (temp.player.activeHand < 0 ||
        temp.player.activeHand >= MAX_PLAYER_HANDS) return -1;
    if (temp.dealer.handCount < 1 || temp.dealer.handCount > 2) return -1;
    if (temp.run.floor < 1) return -1;
    if (temp.run.dealerId < 0 || temp.run.dealerId >= DEALER_COUNT) return -1;
    if (temp.run.previousDealerId < 0 ||
        temp.run.previousDealerId >= DEALER_COUNT) return -1;
    if (temp.run.twistId < 0 || temp.run.twistId >= TWIST_COUNT) return -1;
    if (temp.run.previousTwistId < 0 ||
        temp.run.previousTwistId >= TWIST_COUNT) return -1;
    if (temp.run.perkLockedId < -1 ||
        temp.run.perkLockedId >= PERK_COUNT) return -1;
    if (temp.log.count < 0 || temp.log.count > LOG_SIZE) return -1;
    if (temp.run.shopPerkCount < 0 ||
        temp.run.shopPerkCount > SHOP_POOL_SIZE) return -1;
    if (temp.loanDueFloor < 0) return -1;
    if (temp.loanAmount < 0) return -1;
    if (temp.run.roundsOnFloor < 0) return -1;
    if (temp.winStreak < 0) return -1;
    if (temp.lossStreak < 0) return -1;
    if (temp.run.bossRewarded < 0 || temp.run.bossRewarded > 1) return -1;
    if (temp.run.shopPerksRerolled < 0 ||
        temp.run.shopPerksRerolled > 1) return -1;
    if (temp.run.coldSnapUsed < 0 || temp.run.coldSnapUsed > 1) return -1;
    if (temp.run.coldSnapActive < 0 || temp.run.coldSnapActive > 1) return -1;
    if (temp.run.perkLockedUsed < 0 || temp.run.perkLockedUsed > 1) return -1;
    if (temp.saveEnabled < 0 || temp.saveEnabled > 1) return -1;
    if (temp.playerQuit < 0 || temp.playerQuit > 1) return -1;
    if (temp.roundInProgress < 0 || temp.roundInProgress > 1) return -1;
    if (temp.deck.top < 0 || temp.deck.top > DECK_SIZE) return -1;
    if (temp.run.extraTwistCount < 0 ||
        temp.run.extraTwistCount > MAX_EXTRA_TWISTS) return -1;

    {
        int h;
        for (h = 0; h < MAX_PLAYER_HANDS; h++) {
            if (temp.player.hands[h].count < 0 ||
                temp.player.hands[h].count > MAX_HAND_SIZE) return -1;
            if (temp.player.bets[h] < 0) return -1;
        }
        for (h = 0; h < 2; h++) {
            if (temp.dealer.hands[h].count < 0 ||
                temp.dealer.hands[h].count > MAX_HAND_SIZE) return -1;
        }
    }

    {
        int t;
        for (t = 0; t < temp.run.extraTwistCount; t++) {
            if (temp.run.extraTwists[t] < 0 ||
                temp.run.extraTwists[t] >= TWIST_COUNT) return -1;
        }
    }

    {
        int p;
        for (p = 0; p < temp.run.shopPerkCount; p++) {
            if (temp.run.shopPerks[p] < 0 ||
                temp.run.shopPerks[p] >= PERK_COUNT) return -1;
        }
    }

    *game = temp;
    return 0;
}

void deleteSave(void) {
    char path[SAVE_PATH_BUFFER];
    if (resolveSavePath(path, sizeof(path)) == 0) unlink(path);
}
