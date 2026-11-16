#define _POSIX_C_SOURCE 200809L

#include "test.h"
#include "game.h"
#include "rng.h"
#include "save.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TEST_SAVE_HEADER_SIZE 20

static int beginSaveTest(char *root, size_t rootSize) {
    int written;

    written = snprintf(root, rootSize, "/tmp/tty21-save-test-XXXXXX");
    if (written < 0 || (size_t)written >= rootSize) return -1;
    if (mkdtemp(root) == NULL) return -1;
    if (setenv("XDG_DATA_HOME", root, 1) != 0) return -1;

    rngInit(12345U, 1);
    return 0;
}

static void endSaveTest(const char *root) {
    char directory[512];

    deleteSave();
    snprintf(directory, sizeof(directory), "%s/tty21", root);
    rmdir(directory);
    rmdir(root);
    unsetenv("XDG_DATA_HOME");
}

static uint32_t testChecksum(const void *data, size_t size) {
    const unsigned char *bytes = (const unsigned char *)data;
    uint32_t checksum = 0;
    size_t i;

    for (i = 0; i < size; i++) {
        checksum += bytes[i];
    }

    return checksum;
}

static int rewriteSaveWithInvalidDeckRank(const char *root) {
    char path[512];
    unsigned char header[TEST_SAVE_HEADER_SIZE];
    Game saved;
    uint32_t checksum;
    FILE *file;

    snprintf(path, sizeof(path), "%s/tty21/save.dat", root);
    file = fopen(path, "r+b");
    if (file == NULL) return -1;

    if (fread(header, 1, sizeof(header), file) != sizeof(header) ||
        fread(&saved, 1, sizeof(saved), file) != sizeof(saved)) {
        fclose(file);
        return -1;
    }

    saved.deck.cards[0].rank = (Rank)99;
    checksum = testChecksum(&saved, sizeof(saved));
    memcpy(header + 16, &checksum, sizeof(checksum));

    if (fseek(file, 0, SEEK_SET) != 0 ||
        fwrite(header, 1, sizeof(header), file) != sizeof(header) ||
        fwrite(&saved, 1, sizeof(saved), file) != sizeof(saved)) {
        fclose(file);
        return -1;
    }

    return fclose(file) == 0 ? 0 : -1;
}


static int rewriteSaveVersion(const char *root, uint32_t version) {
    char path[512];
    unsigned char header[TEST_SAVE_HEADER_SIZE];
    FILE *file;
    snprintf(path, sizeof(path), "%s/tty21/save.dat", root);
    file = fopen(path, "r+b");
    if (file == NULL) return -1;
    if (fread(header, 1, sizeof(header), file) != sizeof(header)) {
        fclose(file);
        return -1;
    }
    memcpy(header + 8, &version, sizeof(version));
    if (fseek(file, 0, SEEK_SET) != 0 ||
        fwrite(header, 1, sizeof(header), file) != sizeof(header)) {
        fclose(file);
        return -1;
    }
    return fclose(file) == 0 ? 0 : -1;
}

static int truncateSaveFile(const char *root, long size) {
    char path[512];
    FILE *file;
    snprintf(path, sizeof(path), "%s/tty21/save.dat", root);
    file = fopen(path, "r+b");
    if (file == NULL) return -1;
    if (ftruncate(fileno(file), size) != 0) {
        fclose(file);
        return -1;
    }
    return fclose(file) == 0 ? 0 : -1;
}

TEST(save_round_trip_accepts_completed_five_hand_split) {
    char root[128];
    Game game;
    Game loaded;
    Card card;
    int handIndex;

    ASSERT_EQ_INT(0, beginSaveTest(root, sizeof(root)));
    initGame(&game, MODE_ROGUELIKE);
    ASSERT_EQ_INT(0, game.player.surrendered);
    game.state = STATE_MENU;
    game.player.handCount = MAX_PLAYER_HANDS;
    game.player.activeHand = MAX_PLAYER_HANDS;

    card.suit = HEARTS;
    card.rank = TEN;
    card.cursed = CURSED_NONE;

    for (handIndex = 0; handIndex < MAX_PLAYER_HANDS; handIndex++) {
        addCardToHand(&game.player.hands[handIndex], &card);
        addCardToHand(&game.player.hands[handIndex], &card);
        game.player.bets[handIndex] = 10;
    }

    ASSERT_EQ_INT(0, saveGame(&game));
    ASSERT_EQ_INT(0, loadGame(&loaded));
    ASSERT_EQ_INT(MAX_PLAYER_HANDS, loaded.player.handCount);
    ASSERT_EQ_INT(MAX_PLAYER_HANDS, loaded.player.activeHand);
    endSaveTest(root);
}

TEST(save_rejects_invalid_card_rank_even_with_valid_checksum) {
    char root[128];
    Game game;
    Game loaded;

    ASSERT_EQ_INT(0, beginSaveTest(root, sizeof(root)));
    initGame(&game, MODE_ROGUELIKE);
    ASSERT_EQ_INT(0, saveGame(&game));
    ASSERT_EQ_INT(0, rewriteSaveWithInvalidDeckRank(root));
    ASSERT_EQ_INT(-1, loadGame(&loaded));
    endSaveTest(root);
}


TEST(save_rejects_truncated_header) {
    char root[128];
    Game game;
    Game loaded;
    ASSERT_EQ_INT(0, beginSaveTest(root, sizeof(root)));
    initGame(&game, MODE_ROGUELIKE);
    ASSERT_EQ_INT(0, saveGame(&game));
    ASSERT_EQ_INT(0, truncateSaveFile(root, 8));
    ASSERT_EQ_INT(-1, loadGame(&loaded));
    endSaveTest(root);
}

TEST(save_rejects_truncated_body) {
    char root[128];
    Game game;
    Game loaded;
    ASSERT_EQ_INT(0, beginSaveTest(root, sizeof(root)));
    initGame(&game, MODE_ROGUELIKE);
    ASSERT_EQ_INT(0, saveGame(&game));
    ASSERT_EQ_INT(0, truncateSaveFile(root, TEST_SAVE_HEADER_SIZE + 4));
    ASSERT_EQ_INT(-1, loadGame(&loaded));
    endSaveTest(root);
}

TEST(save_rejects_incompatible_version) {
    char root[128];
    Game game;
    Game loaded;
    ASSERT_EQ_INT(0, beginSaveTest(root, sizeof(root)));
    initGame(&game, MODE_ROGUELIKE);
    ASSERT_EQ_INT(0, saveGame(&game));
    ASSERT_EQ_INT(0, rewriteSaveVersion(root, 2U));
    ASSERT_EQ_INT(-1, loadGame(&loaded));
    endSaveTest(root);
}

void run_save_tests(void) {
    RUN_TEST(save_round_trip_accepts_completed_five_hand_split);
    RUN_TEST(save_rejects_invalid_card_rank_even_with_valid_checksum);
    RUN_TEST(save_rejects_truncated_header);
    RUN_TEST(save_rejects_truncated_body);
    RUN_TEST(save_rejects_incompatible_version);
}
