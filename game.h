#ifndef GAME_H
#define GAME_H

#include "card.h"
#include "hand.h"
#include "dealers.h"
#include "twists.h"
#include "perks.h"
#include "run.h"

typedef enum {
    STATE_BET,
    STATE_PLAYER_TURN,
    STATE_DEALER_TURN,
    STATE_RESULT,
    STATE_SHOP,
    STATE_MENU,
    STATE_BOSS_PREP,
    STATE_QUIT
} GameState;

#define MAX_PLAYER_HANDS 5

typedef struct {
    Hand hands[MAX_PLAYER_HANDS];
    int bets[MAX_PLAYER_HANDS];
    int handCount;
    int activeHand;
    int isSplitAce[MAX_PLAYER_HANDS];
    int chips;
    int perkOwned[PERK_COUNT];
    int perkCharges[PERK_COUNT];
    int usedDouble[MAX_PLAYER_HANDS];
    int usedSplit[MAX_PLAYER_HANDS];
    int surrendered; // current round ended by surrender
    int insuranceBet; // 0 if no insurance was placed
    int evenMoneyTaken; // prevents duplicate settlement after even money
} Player;

// handCount is 1 for every dealer except Twin (Stage 9.5c-2).
typedef struct {
    Hand hands[2];
    int handCount;
} Dealer;

typedef enum {
    MSG_DEFAULT,
    MSG_WIN,
    MSG_LOSS,
    MSG_PUSH,
    MSG_INFO,
    MSG_ACTION,
    MSG_DEALER
} MessageStyle;

#define LOG_SIZE 6
#define LOG_LINE_LEN 96

typedef struct {
    char lines[LOG_SIZE][LOG_LINE_LEN];
    MessageStyle styles[LOG_SIZE];
    int count;
} GameLog;

struct Game {
    Deck deck;
    Player player;
    Dealer dealer;
    GameState state;
    int roundNumber;
    int roundInProgress; // persisted marker for interrupted-round forfeits
    Run run;
    GameMode mode;
    int peekActive;
    int fastMode;
    int fatigueTier;
    int dealerEnraged;
    int winStreak;
    int lossStreak;
    int loanAmount;        // chips owed, 0 if no loan
    int loanDueFloor;      // floor by which loan must be repaid
    int freePeekReady;
    int minBetMultiplier;
    GameLog log;
    int safetyNetArmed; // 1 if a safety net is active
    int tippedThisRound;
    char lastPayoutBreakdown[256];
    unsigned int rngSeed; // seed used to start the run, for save round-trip
    int saveEnabled; // 1 = autosave on
    unsigned long rngCalls; // RNG draws consumed since rngInit
    int lastAutosavedRound; // round number at last successful autosave, 0 if never
    int playerQuit; // 1 if player quit to menu, not if the run ended
    int roundHadNaturalBj; // 1 if 21 came from the initial deal
    int bossRuleDisabled; // 1 if player paid to disable the first boss rule
    int bossRulePaid; // cost in marks
    int totalFloorsReached;
    int totalBossesDefeated;
    int totalMarksEarned;
};

void initGame(Game *game, GameMode mode);
void generateShopPool(Game *game);
int canRerollShopPool(const Game *game);
int rerollShopPool(Game *game);
void advanceFloor(Game *game);
void bossPrepTurn(Game *game);
void logPush(Game *game, const char *msg, MessageStyle style);
int runGame(Game *game);

#endif
