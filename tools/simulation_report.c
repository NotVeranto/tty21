#include "game.h"
#include "dealers.h"
#include "perks.h"
#include "round_cards.h"
#include "round_economy.h"
#include "round_rules.h"
#include "round_simulation.h"
#include "rng.h"
#include "twists.h"

#include <stdio.h>
#include <string.h>

#define REPORT_RUNS 2048
#define REPORT_ROUNDS_PER_RUN 100

static int minimum_bet_policy(const Game *game, void *context) {
    (void)context;
    return getMinBet(game, game->run.floor, game->mode);
}

static ActionId progressing_policy(const Game *game,
                                   const ActionId *actions,
                                   int actionCount,
                                   void *context) {
    int index;
    int handValue;

    if (game->player.handCount > 0 &&
        game->player.activeHand >= 0 &&
        game->player.activeHand < game->player.handCount &&
        game->player.hands[game->player.activeHand].count == 0) {
        for (index = 0; index < actionCount; index++) {
            if (actions[index] == ACTION_QUIT) return ACTION_QUIT;
        }
    }

    handValue = calculatePlayerHandValue(game);
    int standBlocked =
        (twistActive(game, TWIST_STICKY_HANDS) && handValue < 15) ||
        (game->run.floor >= 10 &&
         game->run.dealerId == DEALER_BUTCHER && handValue < 15);
    (void)context;

    /*
     * No-Reshuffle can exhaust the drawable deck while a low hand is still
     * forbidden from standing. Hitting then cannot change the hand, so end
     * this strategy sample cleanly instead of exhausting the action limit.
     */
    if (standBlocked &&
        game->deck.top >= DECK_SIZE - DECK_BURN_SIZE) {
        for (index = 0; index < actionCount; index++) {
            if (actions[index] == ACTION_QUIT) return ACTION_QUIT;
        }
    }

    for (index = 0; index < actionCount; index++) {
        if (standBlocked && actions[index] == ACTION_HIT) return ACTION_HIT;
        if (!standBlocked && actions[index] == ACTION_STAND) return ACTION_STAND;
        if (standBlocked && actions[index] == ACTION_STAND &&
            calculatePlayerHandValue(game) < 15 &&
            game->player.hands[game->player.activeHand].count >=
                MAX_HAND_SIZE) return ACTION_STAND;
    }
    return ACTION_COUNT;
}

typedef struct {
    unsigned int lastSeed;
    int lastFloor;
    int shopsVisited;
    long long marksAtShop[16];
    int visitsByFloor[16];
} ReportFloorContext;

typedef struct {
    int shopActions;
    int perkPurchases;
    int maxPerksPerFloor;
    int lastPurchaseFloor;
} ReportShopContext;

static char visit_shop_then_advance_policy(const Game *game, void *context) {
    ReportFloorContext *report = (ReportFloorContext *)context;

    if (report == NULL) return 'a';
    if (report->lastSeed != game->rngSeed) {
        report->lastSeed = game->rngSeed;
        report->lastFloor = 0;
    }
    /*
     * Give each floor a few rounds to generate its normal rewards before
     * visiting the shop. Entering the shop after the first round of every
     * floor produced a mostly empty comparison: the policy had no marks to
     * spend yet, and advancing immediately starved the run of progression.
     */
    if (game->run.roundsOnFloor < 3) return 'c';
    if (report->lastFloor != game->run.floor) {
        report->lastFloor = game->run.floor;
        report->shopsVisited++;
        if (game->run.floor >= 1 && game->run.floor <= 15) {
            report->marksAtShop[game->run.floor] += game->run.marks;
            report->visitsByFloor[game->run.floor]++;
        }
        return 's';
    }

    /*
     * An unaffordable floor advance is a normal gameplay condition, not a
     * simulation failure. Stay on this floor and keep playing until the
     * bankroll can cover the forecast or the run naturally stops.
     */
    {
        AdvanceForecast forecast;
        computeAdvanceForecast(game, &forecast);
        if (!forecast.affordable) return 'c';
    }
    return 'a';
}

static int counted_balanced_shop_policy(const Game *game,
                                        int currentMinBet,
                                        void *context) {
    ReportShopContext *report = (ReportShopContext *)context;
    int action;

    if (report != NULL && report->maxPerksPerFloor > 0 &&
        report->perkPurchases > 0 &&
        report->lastPurchaseFloor == game->run.floor) {
        action = 0;
    } else {
        action = simulationBalancedShopPolicy(game, currentMinBet, NULL);
    }

    if (report != NULL && action > 0 &&
        report->maxPerksPerFloor > 0) {
        report->lastPurchaseFloor = game->run.floor;
    }
    if (report != NULL && action != 0) {
        report->shopActions++;
        if (action > 0) report->perkPurchases++;
    }
    return action;
}

typedef struct {
    int strategy;
    int calls;
    int actions;
    int purchases;
    int offers;
    int eligible;
    int affordable;
    int purchasesFromHeldMarks;
    int purchasesFundedByConversion;
    int conversionPurchasePending;
    int purchasesByPerk[PERK_COUNT];
    int purchasesByFloor[16];
    long long marksSpentByFloor[16];
    int reserveBetMultiple;
    int conversionTrades;
    int chipsConverted;
    int marksFromConversion;
    int chipCostPerTrade;
    int marksPerTrade;
} ReportConversionContext;

static int no_conversion_shop_policy(const Game *game,
                                     int currentMinBet,
                                     void *context) {
    static const PerkId balancedPriority[] = {
        PERK_SURVIVAL, PERK_EXTRA_LIFE, PERK_DEALERS_FAVOR, PERK_BANKROLL,
        PERK_COMPOUNDING, PERK_DOUBLE_DIP, PERK_INSURANCE_PERK,
        PERK_MARKED_CARD, PERK_PEEK, PERK_SECOND_WIND, PERK_SPLIT_MASTERY,
        PERK_SIXTH_SENSE, PERK_ACE_HIGH
    };
    static const PerkId defensivePriority[] = {
        PERK_SURVIVAL, PERK_INSURANCE_PERK, PERK_BANKROLL,
        PERK_EXTRA_LIFE, PERK_DEALERS_FAVOR, PERK_PEEK,
        PERK_SECOND_WIND, PERK_COMPOUNDING, PERK_DOUBLE_DIP,
        PERK_MARKED_CARD, PERK_SPLIT_MASTERY, PERK_SIXTH_SENSE,
        PERK_ACE_HIGH
    };
    ReportConversionContext *stats = (ReportConversionContext *)context;
    const PerkId *priority;
    int priorityCount;
    int p;

    if (game == NULL || stats == NULL || game->state != STATE_SHOP) return 0;
    stats->calls++;
    priority = stats->strategy == 0 ? balancedPriority : defensivePriority;
    priorityCount = stats->strategy == 0
        ? (int)(sizeof(balancedPriority) / sizeof(balancedPriority[0]))
        : (int)(sizeof(defensivePriority) / sizeof(defensivePriority[0]));

    /*
     * A zero reserve disables conversion. Positive values enable the real
     * 10-chip/2-mark service and vary only the required post-exchange reserve.
     */
    for (p = 0; p < priorityCount; p++) {
        PerkId perk = priority[p];
        int offer;
        if (game->player.perkOwned[perk] || isPerkLockedThisFloor(game, perk)) {
            continue;
        }
        for (offer = 0; offer < game->run.shopPerkCount; offer++) {
            if (game->run.shopPerks[offer] != perk) continue;
            stats->offers++;
            stats->eligible++;
            if (game->run.marks >= perkCost(perk)) {
                stats->affordable++;
                if (stats->conversionPurchasePending) {
                    stats->purchasesFundedByConversion++;
                    stats->conversionPurchasePending = 0;
                } else {
                    stats->purchasesFromHeldMarks++;
                }
                stats->purchases++;
                stats->purchasesByPerk[perk]++;
                if (game->run.floor >= 1 && game->run.floor <= 15) {
                    stats->purchasesByFloor[game->run.floor]++;
                    stats->marksSpentByFloor[game->run.floor] += perkCost(perk);
                }
                stats->actions++;
                return (int)perk + 1;
            }

            if (stats->reserveBetMultiple > 0) {
                int requiredMarks = perkCost(perk) - game->run.marks;
                int conversionCost = ((requiredMarks + 1) / 2) * 10;
                char services[7];
                int serviceCount = getShopServiceOffers(game, services, 7);
                int service;
                for (service = 0; service < serviceCount; service++) {
                    if (services[service] == 'v' && requiredMarks > 0 &&
                        canPurchaseShopService(game, 'v', currentMinBet) &&
                        (long long)game->player.chips - conversionCost >=
                            (long long)currentMinBet *
                                stats->reserveBetMultiple) {
                        /* The runner executes exactly one 10-chip/2-mark trade. */
                        stats->conversionPurchasePending = 1;
                        stats->conversionTrades++;
                        stats->chipsConverted += 10;
                        stats->marksFromConversion += 2;
                        stats->actions++;
                        return -(int)'v';
                    }
                }
            }
        }
    }

    {
        char services[7];
        int serviceCount = getShopServiceOffers(game, services, 7);
        int service;
        for (service = 0; service < serviceCount; service++) {
            if (services[service] == 'n' &&
                game->player.chips < currentMinBet * 2 &&
                canPurchaseShopService(game, 'n', currentMinBet)) {
                stats->actions++;
                return -(int)'n';
            }
        }
    }
    return 0;
}


/*
 * Report-only exchange-rate variant. It applies the configured transaction
 * directly to the isolated simulation Game, then uses the normal perk
 * purchase API. Production shop prices and service behavior are untouched.
 */
static int conversion_rate_shop_policy(const Game *game,
                                       int currentMinBet,
                                       void *context) {
    static const PerkId balancedPriority[] = {
        PERK_SURVIVAL, PERK_EXTRA_LIFE, PERK_DEALERS_FAVOR, PERK_BANKROLL,
        PERK_COMPOUNDING, PERK_DOUBLE_DIP, PERK_INSURANCE_PERK,
        PERK_MARKED_CARD, PERK_PEEK, PERK_SECOND_WIND, PERK_SPLIT_MASTERY,
        PERK_SIXTH_SENSE, PERK_ACE_HIGH
    };
    static const PerkId defensivePriority[] = {
        PERK_SURVIVAL, PERK_INSURANCE_PERK, PERK_BANKROLL,
        PERK_EXTRA_LIFE, PERK_DEALERS_FAVOR, PERK_PEEK,
        PERK_SECOND_WIND, PERK_COMPOUNDING, PERK_DOUBLE_DIP,
        PERK_MARKED_CARD, PERK_SPLIT_MASTERY, PERK_SIXTH_SENSE,
        PERK_ACE_HIGH
    };
    ReportConversionContext *stats = (ReportConversionContext *)context;
    const PerkId *priority = stats != NULL && stats->strategy == 1
        ? defensivePriority : balancedPriority;
    int priorityCount = stats != NULL && stats->strategy == 1
        ? (int)(sizeof(defensivePriority) / sizeof(defensivePriority[0]))
        : (int)(sizeof(balancedPriority) / sizeof(balancedPriority[0]));
    int p;

    if (game == NULL || stats == NULL || game->state != STATE_SHOP) return 0;
    stats->calls++;
    for (p = 0; p < priorityCount; p++) {
        PerkId perk = priority[p];
        int offer;
        if (game->player.perkOwned[perk] || isPerkLockedThisFloor(game, perk)) {
            continue;
        }
        for (offer = 0; offer < game->run.shopPerkCount; offer++) {
            int requiredMarks;
            int trades;
            int chipCost;
            int serviceIndex;
            int serviceCount;
            char services[7];
            int serviceOffered = 0;
            if (game->run.shopPerks[offer] != perk) continue;

            stats->offers++;
            stats->eligible++;
            if (game->run.marks >= perkCost(perk)) {
                stats->affordable++;
                stats->purchasesFromHeldMarks++;
            } else {
                requiredMarks = perkCost(perk) - game->run.marks;
                trades = (requiredMarks + stats->marksPerTrade - 1) /
                         stats->marksPerTrade;
                chipCost = trades * stats->chipCostPerTrade;
                serviceCount = getShopServiceOffers(game, services, 7);
                for (serviceIndex = 0; serviceIndex < serviceCount;
                     serviceIndex++) {
                    if (services[serviceIndex] == 'v') {
                        serviceOffered = 1;
                        break;
                    }
                }
                if (!serviceOffered || chipCost <= 0 ||
                    (long long)game->player.chips - chipCost <
                        (long long)currentMinBet * stats->reserveBetMultiple) {
                    continue;
                }

                {
                    Game *mutableGame = (Game *)game;
                    int marksAdded = trades * stats->marksPerTrade;
                    mutableGame->player.chips -= chipCost;
                    mutableGame->run.marks += marksAdded;
                    mutableGame->totalMarksEarned += marksAdded;
                    stats->conversionTrades += trades;
                    stats->chipsConverted += chipCost;
                    stats->marksFromConversion += marksAdded;
                    stats->purchasesFundedByConversion++;
                }
            }

            stats->purchases++;
            stats->purchasesByPerk[perk]++;
            if (game->run.floor >= 1 && game->run.floor <= 15) {
                stats->purchasesByFloor[game->run.floor]++;
                stats->marksSpentByFloor[game->run.floor] += perkCost(perk);
            }
            stats->actions++;
            return (int)perk + 1;
        }
    }

    {
        char services[7];
        int serviceCount = getShopServiceOffers(game, services, 7);
        int service;
        if (game->player.chips < currentMinBet * 2 &&
            canPurchaseShopService(game, 'n', currentMinBet)) {
            for (service = 0; service < serviceCount; service++) {
                if (services[service] == 'n') {
                    stats->actions++;
                    return -(int)'n';
                }
            }
        }
    }
    return 0;
}


typedef enum {
    REPORT_UTILITY_SHOP_REROLL = 0,
    REPORT_UTILITY_TWIST_TOOLS,
    REPORT_UTILITY_PERK_ONLY,
    REPORT_UTILITY_BOSS_PREP_ONLY
} ReportUtilityMode;

typedef struct {
    ReportUtilityMode mode;
    int bossPrepOffers;
    int bossRulesDisabled;
    long long bossMarksSpent;
    int bossPrepOffersByDealer[DEALER_COUNT];
    int bossRulesDisabledByDealer[DEALER_COUNT];
    int bossPrepUnaffordableByDealer[DEALER_COUNT];
    long long bossMarksSpentByDealer[DEALER_COUNT];
    int shopRerolls;
    int twistRerolls;
    int twistBlocks;
    long long serviceMarksSpent;
    int perkPurchases;
    int lastServiceFloor;
} ReportUtilityContext;

static int report_boss_prep_policy(const Game *game, void *context) {
    ReportUtilityContext *stats = (ReportUtilityContext *)context;
    if (game == NULL || stats == NULL || game->state != STATE_BOSS_PREP ||
        stats->mode != REPORT_UTILITY_BOSS_PREP_ONLY) return 0;
    if (game->run.dealerId < 0 || game->run.dealerId >= DEALER_COUNT ||
        !dealerIsBoss(game->run.dealerId)) return 0;
    stats->bossPrepOffers++;
    stats->bossPrepOffersByDealer[game->run.dealerId]++;
    if (game->run.marks >= game->bossRulePaid) {
        stats->bossRulesDisabled++;
        stats->bossRulesDisabledByDealer[game->run.dealerId]++;
        stats->bossMarksSpent += game->bossRulePaid;
        stats->bossMarksSpentByDealer[game->run.dealerId] +=
            game->bossRulePaid;
        return 1;
    }
    stats->bossPrepUnaffordableByDealer[game->run.dealerId]++;
    return 0;
}

static int report_utility_shop_policy(const Game *game, int currentMinBet,
                                      void *context) {
    ReportUtilityContext *stats = (ReportUtilityContext *)context;
    char services[7];
    int serviceCount;
    int index;

    if (game == NULL || stats == NULL || game->state != STATE_SHOP) return 0;
    if (stats->mode == REPORT_UTILITY_BOSS_PREP_ONLY) return 0;

    if (stats->mode == REPORT_UTILITY_PERK_ONLY) {
        static const PerkId priority[] = {
            PERK_SURVIVAL, PERK_EXTRA_LIFE, PERK_DEALERS_FAVOR, PERK_BANKROLL,
            PERK_COMPOUNDING, PERK_DOUBLE_DIP, PERK_INSURANCE_PERK,
            PERK_MARKED_CARD, PERK_PEEK, PERK_SECOND_WIND, PERK_SPLIT_MASTERY,
            PERK_SIXTH_SENSE, PERK_ACE_HIGH
        };
        for (index = 0; index < (int)(sizeof(priority) / sizeof(priority[0])); index++) {
            PerkId perk = priority[index];
            int offer;
            if (game->player.perkOwned[perk] || isPerkLockedThisFloor(game, perk)) continue;
            for (offer = 0; offer < game->run.shopPerkCount; offer++) {
                if (game->run.shopPerks[offer] == perk &&
                    game->run.marks >= perkCost(perk)) {
                    stats->perkPurchases++;
                    return (int)perk + 1;
                }
            }
        }
        return 0;
    }

    /* One utility-service action per actual shop visit; no repeated reroll loop. */
    if (stats->lastServiceFloor == game->run.floor) return 0;
    serviceCount = getShopServiceOffers(game, services, 7);

    if (stats->mode == REPORT_UTILITY_SHOP_REROLL) {
        if (game->run.marks < 10 || game->run.shopPerksRerolled ||
            !canPurchaseShopService(game, 'z', currentMinBet)) return 0;
        for (index = 0; index < serviceCount; index++) {
            if (services[index] == 'z') {
                stats->lastServiceFloor = game->run.floor;
                stats->shopRerolls++;
                stats->serviceMarksSpent += 10;
                return -(int)'z';
            }
        }
    } else if (stats->mode == REPORT_UTILITY_TWIST_TOOLS &&
               game->run.twistId != TWIST_NONE) {
        const TwistInfo *twist = getTwistInfo(game->run.twistId);
        if (twist == NULL || twist->category != TWIST_CAT_HOSTILE) return 0;
        /* Prefer the stronger permanent block; otherwise reroll once. */
        if (game->run.marks >= 40 &&
            canPurchaseShopService(game, 'b', currentMinBet)) {
            for (index = 0; index < serviceCount; index++) {
                if (services[index] == 'b') {
                    stats->lastServiceFloor = game->run.floor;
                    stats->twistBlocks++;
                    stats->serviceMarksSpent += 40;
                    return -(int)'b';
                }
            }
        }
        if (game->run.marks >= 30 &&
            canPurchaseShopService(game, 'r', currentMinBet)) {
            for (index = 0; index < serviceCount; index++) {
                if (services[index] == 'r') {
                    stats->lastServiceFloor = game->run.floor;
                    stats->twistRerolls++;
                    stats->serviceMarksSpent += 30;
                    return -(int)'r';
                }
            }
        }
    }
    return 0;
}

static void print_conversion_comparison(
    const char *label, const SimulationBatchStats *baseline,
    const SimulationBatchStats *candidate,
    const ReportConversionContext *context) {
    double runs = (double)candidate->runsCompleted;
    double baseRuns = (double)baseline->runsCompleted;
    printf("  %-27s chips %+.2f | floors %+.2f | bosses %+.2f | bankrupt %+.1f pp | offers %d | held-marks-ready %d | bought %d (held %d, converted %d)\n",
           label,
           candidate->totalFinalChips / runs - baseline->totalFinalChips / baseRuns,
           candidate->totalFloorsReached / runs - baseline->totalFloorsReached / baseRuns,
           candidate->totalBossesDefeated / runs - baseline->totalBossesDefeated / baseRuns,
           100.0 * ((double)candidate->bankruptRuns / runs -
                    (double)baseline->bankruptRuns / baseRuns),
           context->offers, context->affordable, context->purchases,
           context->purchasesFromHeldMarks,
           context->purchasesFundedByConversion);
}

static void print_conversion_breakdown(const char *label,
                                       const ReportConversionContext *context) {
    int floor;
    int perk;
    printf("%s\n", label);
    printf("  purchases by floor:");
    for (floor = 1; floor <= 15; floor++) {
        if (context->purchasesByFloor[floor] > 0) {
            printf(" F%d %d (%lld marks)", floor,
                   context->purchasesByFloor[floor],
                   context->marksSpentByFloor[floor]);
        }
    }
    putchar('\n');
    printf("  purchases by perk:");
    for (perk = 0; perk < PERK_COUNT; perk++) {
        if (context->purchasesByPerk[perk] > 0) {
            printf(" %s %d", perkName((PerkId)perk),
                   context->purchasesByPerk[perk]);
        }
    }
    putchar('\n');
}

static void print_floor_marks(const char *label,
                              const ReportFloorContext *report) {
    int floor;

    printf("%s\n", label);
    for (floor = 1; floor <= 15; floor++) {
        if (report->visitsByFloor[floor] > 0) {
            printf("  floor %2d: runs %3d | avg marks %.1f\n",
                   floor, report->visitsByFloor[floor],
                   (double)report->marksAtShop[floor] /
                       report->visitsByFloor[floor]);
        }
    }
}

static void print_batch(const char *label, const SimulationBatchStats *stats) {
    double runs = (double)stats->runsCompleted;
    double outcomes = (double)(stats->totalWins + stats->totalLosses +
                                stats->totalPushes);

    printf("%s\n", label);
    printf("  runs: %d | rounds: %d (%.2f/run)\n",
           stats->runsCompleted, stats->totalRounds,
           stats->totalRounds / runs);
    printf("  outcomes: wins %d (%.1f%%) | losses %d (%.1f%%) | "
           "pushes %d (%.1f%%)\n",
           stats->totalWins, outcomes > 0.0 ? 100.0 * stats->totalWins / outcomes : 0.0,
           stats->totalLosses, outcomes > 0.0 ? 100.0 * stats->totalLosses / outcomes : 0.0,
           stats->totalPushes, outcomes > 0.0 ? 100.0 * stats->totalPushes / outcomes : 0.0);
    printf("  final chips: avg %.2f | min %d | max %d\n",
           stats->totalFinalChips / runs,
           stats->minFinalChips, stats->maxFinalChips);
    printf("  profitable: %d/%d (%.1f%%) | bankrupt: %d/%d (%.1f%%)\n",
           stats->profitableRuns, stats->runsCompleted,
           100.0 * stats->profitableRuns / runs,
           stats->bankruptRuns, stats->runsCompleted,
           100.0 * stats->bankruptRuns / runs);
    printf("  floors: avg %.2f | shallowest %d | deepest %d\n",
           stats->totalFloorsReached / runs,
           stats->shallowestFloorReached,
           stats->deepestFloorReached);
    printf("  bosses defeated: %.2f/run | marks earned: %.2f/run\n",
           stats->totalBossesDefeated / runs,
           stats->totalMarksEarned / runs);
    printf("  stop reasons: round-limit %d | bankroll %d | policy %d | quit %d\n",
           stats->stopReasonCounts[SIMULATION_STOP_MAX_ROUNDS],
           stats->stopReasonCounts[SIMULATION_STOP_INSUFFICIENT_BANKROLL],
           stats->stopReasonCounts[SIMULATION_STOP_POLICY],
           stats->stopReasonCounts[SIMULATION_STOP_GAME_QUIT]);
}


typedef struct { PerkId target; int offers; int affordable; int purchases; long long marksAtOffer; long long marksShort; } ReportSinglePerkContext;

static int single_perk_shop_policy(const Game *game, int currentMinBet,
                                   void *context) {
    ReportSinglePerkContext *report = (ReportSinglePerkContext *)context;
    int index;
    (void)currentMinBet;
    if (game == NULL || report == NULL || game->state != STATE_SHOP ||
        game->player.perkOwned[report->target] ||
        isPerkLockedThisFloor(game, report->target)) return 0;
    for (index = 0; index < game->run.shopPerkCount; index++) {
        if (game->run.shopPerks[index] == report->target) {
            report->offers++;
            report->marksAtOffer += game->run.marks;
            if (game->run.marks >= perkCost(report->target)) {
                report->affordable++;
                report->purchases++;
                return (int)report->target + 1;
            }
            report->marksShort += perkCost(report->target) - game->run.marks;
            return 0;
        }
    }
    return 0;
}

static void print_perk_comparison(const char *name,
                                  const SimulationBatchStats *baseline,
                                  const SimulationBatchStats *perk,
                                  int offers, int affordable, int purchases,
                                  long long marksAtOffer, long long marksShort) {
    double runs = (double)perk->runsCompleted;
    double baseRuns = (double)baseline->runsCompleted;
    printf("  %-16s offered %3d | affordable %3d | bought %3d | "
           "marks %.1f | shortfall %.1f | chips %+.2f | floors %+.2f | "
           "bosses %+.2f | bankrupt %+.1f pp\n",
           name, offers, affordable, purchases,
           offers > 0 ? (double)marksAtOffer / offers : 0.0,
           offers > affordable ? (double)marksShort / (offers - affordable) : 0.0,
           perk->totalFinalChips / runs - baseline->totalFinalChips / baseRuns,
           perk->totalFloorsReached / runs - baseline->totalFloorsReached / baseRuns,
           perk->totalBossesDefeated / runs - baseline->totalBossesDefeated / baseRuns,
           100.0 * ((double)perk->bankruptRuns / runs -
                    (double)baseline->bankruptRuns / baseRuns));
}


typedef struct {
    int startingMarks;
    long long finalChips;
    long long floors;
    long long bosses;
    long long bankrupt;
    long long marksAtShop[16];
    int visitsByFloor[16];
    int shopActions;
    int perkPurchases;
} ReportGrantStats;

static int run_starting_mark_grant_batch(
    const unsigned int *seeds, int seedCount, int startingMarks,
    ReportGrantStats *grantStats,
    int maxPerksPerFloor) {
    int index;
    memset(grantStats, 0, sizeof(*grantStats));
    grantStats->startingMarks = startingMarks;
    for (index = 0; index < seedCount; index++) {
        Game game;
        SimulationRunStats runStats;
        ReportFloorContext floorContext = {0};
        ReportShopContext shopContext = {0, 0, maxPerksPerFloor, -1};
        rngInit(seeds[index], 1);
        initGame(&game, MODE_ROGUELIKE);
        game.fastMode = 1;
        game.saveEnabled = 0;
        game.run.marks += startingMarks;
        game.totalMarksEarned += startingMarks;
        if (!simulateRoguelikeRun(
                &game, REPORT_ROUNDS_PER_RUN,
                minimum_bet_policy, NULL, progressing_policy, NULL,
                visit_shop_then_advance_policy, &floorContext,
                NULL, NULL, counted_balanced_shop_policy, &shopContext,
                &runStats)) {
            fprintf(stderr, "error: starting-mark experiment failed for seed %u (grant %d)\n",
                    seeds[index], startingMarks);
            return 0;
        }
        grantStats->finalChips += runStats.finalChips;
        grantStats->floors += runStats.floorsReached;
        grantStats->bosses += runStats.bossesDefeated;
        if (runStats.finalChips <= 0) grantStats->bankrupt++;
        grantStats->shopActions += shopContext.shopActions;
        grantStats->perkPurchases += shopContext.perkPurchases;
        {
            int floor;
            for (floor = 1; floor <= 15; floor++) {
                grantStats->marksAtShop[floor] += floorContext.marksAtShop[floor];
                grantStats->visitsByFloor[floor] += floorContext.visitsByFloor[floor];
            }
        }
    }
    return 1;
}

static void print_starting_mark_grant_values(
    const ReportGrantStats *stats,
    double baselineChips,
    double baselineFloors,
    double baselineBosses,
    double baselineBankrupt,
    int runs) {
    double count = (double)runs;
    printf("  +%2d marks | chips %.2f (%+.2f) | floors %.2f (%+.2f) | bosses %.2f (%+.2f) | bankrupt %.1f%% (%+.1f pp) | shop actions %d | purchases %d | shop marks:",
           stats->startingMarks,
           stats->finalChips / count,
           stats->finalChips / count - baselineChips,
           stats->floors / count,
           stats->floors / count - baselineFloors,
           stats->bosses / count,
           stats->bosses / count - baselineBosses,
           100.0 * stats->bankrupt / count,
           100.0 * (stats->bankrupt / count - baselineBankrupt),
           stats->shopActions, stats->perkPurchases);
    {
        int floor;
        for (floor = 1; floor <= 5; floor++) {
            if (stats->visitsByFloor[floor] > 0) {
                printf(" F%d %.1f", floor,
                       (double)stats->marksAtShop[floor] /
                           stats->visitsByFloor[floor]);
            } else {
                printf(" F%d n/a", floor);
            }
        }
    }
    putchar('\n');
}

static void print_starting_mark_grant(const ReportGrantStats *stats,
                                      const SimulationBatchStats *baseline,
                                      int runs) {
    double baseRuns = (double)baseline->runsCompleted;
    print_starting_mark_grant_values(
        stats,
        baseline->totalFinalChips / baseRuns,
        baseline->totalFloorsReached / baseRuns,
        baseline->totalBossesDefeated / baseRuns,
        (double)baseline->bankruptRuns / baseRuns,
        runs);
}

static void print_starting_mark_grant_vs_grant(
    const ReportGrantStats *stats,
    const ReportGrantStats *baseline,
    int runs) {
    double baseRuns = (double)runs;
    print_starting_mark_grant_values(
        stats,
        baseline->finalChips / baseRuns,
        baseline->floors / baseRuns,
        baseline->bosses / baseRuns,
        (double)baseline->bankrupt / baseRuns,
        runs);
}

int main(void) {
    unsigned int seeds[REPORT_RUNS];
    SimulationBatchStats baseline;
    SimulationBatchStats balanced;
    SimulationBatchStats noConvertBalancedStats;
    SimulationBatchStats noConvertDefensiveStats;
    ReportFloorContext baselineFloor = {0};
    ReportFloorContext balancedFloor = {0};
    ReportShopContext balancedShop = {0, 0, 0, -1};
    int index;
    ReportGrantStats grant15, grant30, grant45;
    ReportGrantStats capped0, capped15, capped30, capped45;

    for (index = 0; index < REPORT_RUNS; index++) {
        seeds[index] = 1000U + (unsigned int)index * 7919U;
    }

    /*
     * Probe each seed independently before aggregating. This keeps a failed
     * run actionable: the batch API intentionally reports only success/fail.
     */
    for (index = 0; index < REPORT_RUNS; index++) {
        SimulationBatchStats probe;
        if (!simulateRoguelikeBatch(
                &seeds[index], 1, REPORT_ROUNDS_PER_RUN,
                minimum_bet_policy, NULL, progressing_policy, NULL,
                visit_shop_then_advance_policy, &baselineFloor,
                NULL, NULL, NULL, NULL, &probe)) {
            fprintf(stderr, "error: baseline failed for seed %u (index %d, reason %d, state %d, floor %d, deck %d, hand %d/%d (active %d, hands %d), twist %d, dealer %d)\n",
                    seeds[index], index, probe.failureReason, probe.failureState, probe.failureFloor, probe.failureDeckTop, probe.failureHandValue, probe.failureHandCount, probe.failureActiveHand, probe.failurePlayerHandCount, probe.failureTwist, probe.failureDealer);
            return 1;
        }
        if (!simulateRoguelikeBatch(
                &seeds[index], 1, REPORT_ROUNDS_PER_RUN,
                minimum_bet_policy, NULL, progressing_policy, NULL,
                visit_shop_then_advance_policy, &balancedFloor,
                NULL, NULL,
                counted_balanced_shop_policy, &balancedShop, &probe)) {
            fprintf(stderr, "error: balanced shop policy failed for seed %u (index %d)\n",
                    seeds[index], index);
            return 1;
        }
    }

    baselineFloor = (ReportFloorContext){0};
    balancedFloor = (ReportFloorContext){0};
    balancedShop = (ReportShopContext){0, 0, 0, -1};

    if (!simulateRoguelikeBatch(
            seeds, REPORT_RUNS, REPORT_ROUNDS_PER_RUN,
            minimum_bet_policy, NULL, progressing_policy, NULL,
            visit_shop_then_advance_policy, &baselineFloor,
            NULL, NULL, NULL, NULL, &baseline) ||
        !simulateRoguelikeBatch(
            seeds, REPORT_RUNS, REPORT_ROUNDS_PER_RUN,
            minimum_bet_policy, NULL, progressing_policy, NULL,
            visit_shop_then_advance_policy, &balancedFloor,
            NULL, NULL,
            counted_balanced_shop_policy, &balancedShop, &balanced)) {
        fprintf(stderr, "error: simulation batch failed\n");
        return 1;
    }

    if (balancedShop.perkPurchases == 0) {
        fprintf(stderr, "error: balanced policy made no perk purchases\n");
        return 1;
    }

    printf("tty21 seeded shop-policy comparison\n");
    printf("shared seeds: %d | maximum rounds per run: %d\n",
           REPORT_RUNS, REPORT_ROUNDS_PER_RUN);
    printf("shop visits: baseline %d | balanced %d\n\n",
           baselineFloor.shopsVisited, balancedFloor.shopsVisited);
    print_floor_marks("Baseline marks at first shop visit by floor (after at least 3 rounds):",
                      &baselineFloor);
    putchar('\n');
    print_batch("Baseline (no shop purchases)", &baseline);
    putchar('\n');
    print_batch("Balanced shop policy", &balanced);
    printf("  shop actions: %d | perk purchases: %d\n",
           balancedShop.shopActions, balancedShop.perkPurchases);
    puts("\nStarting-mark grant experiment (same seeds, balanced shop purchases)");
    if (!run_starting_mark_grant_batch(seeds, REPORT_RUNS, 15, &grant15, 0) ||
        !run_starting_mark_grant_batch(seeds, REPORT_RUNS, 30, &grant30, 0) ||
        !run_starting_mark_grant_batch(seeds, REPORT_RUNS, 45, &grant45, 0)) {
        return 1;
    }
    print_starting_mark_grant(&grant15, &balanced, REPORT_RUNS);
    print_starting_mark_grant(&grant30, &balanced, REPORT_RUNS);
    print_starting_mark_grant(&grant45, &balanced, REPORT_RUNS);
    puts("  Starting marks are granted once before floor 1; the balanced shop policy buys only current offers it can afford.");
    puts("\nOne-per-floor purchase cap (same seeds; deltas versus capped +0 baseline)");
    if (!run_starting_mark_grant_batch(seeds, REPORT_RUNS, 0, &capped0, 1) ||
        !run_starting_mark_grant_batch(seeds, REPORT_RUNS, 15, &capped15, 1) ||
        !run_starting_mark_grant_batch(seeds, REPORT_RUNS, 30, &capped30, 1) ||
        !run_starting_mark_grant_batch(seeds, REPORT_RUNS, 45, &capped45, 1)) {
        return 1;
    }
    print_starting_mark_grant_vs_grant(&capped0, &capped0, REPORT_RUNS);
    print_starting_mark_grant_vs_grant(&capped15, &capped0, REPORT_RUNS);
    print_starting_mark_grant_vs_grant(&capped30, &capped0, REPORT_RUNS);
    print_starting_mark_grant_vs_grant(&capped45, &capped0, REPORT_RUNS);
    puts("  Capped policy buys at most one perk per floor; +0 is the matched capped-policy control.");
    puts("\nChip-conversion strategy comparison (same seeds; deltas versus no-purchase baseline)");
    {
        ReportConversionContext noConvertBalanced = {0};
        ReportConversionContext noConvertDefensive = {0};
        noConvertDefensive.strategy = 1;
        ReportFloorContext noConvertBalancedFloor = {0};
        ReportFloorContext noConvertDefensiveFloor = {0};
        if (!simulateRoguelikeBatch(
                seeds, REPORT_RUNS, REPORT_ROUNDS_PER_RUN,
                minimum_bet_policy, NULL, progressing_policy, NULL,
                visit_shop_then_advance_policy, &noConvertBalancedFloor,
                NULL, NULL, no_conversion_shop_policy, &noConvertBalanced,
                &noConvertBalancedStats) ||
            !simulateRoguelikeBatch(
                seeds, REPORT_RUNS, REPORT_ROUNDS_PER_RUN,
                minimum_bet_policy, NULL, progressing_policy, NULL,
                visit_shop_then_advance_policy, &noConvertDefensiveFloor,
                NULL, NULL, no_conversion_shop_policy, &noConvertDefensive,
                &noConvertDefensiveStats)) {
            fprintf(stderr, "error: chip-conversion strategy comparison failed\n");
            return 1;
        }
        print_conversion_comparison("balanced priority, no conversion",
                                    &baseline, &noConvertBalancedStats,
                                    &noConvertBalanced);
        print_conversion_comparison("defensive priority, no conversion",
                                    &baseline, &noConvertDefensiveStats,
                                    &noConvertDefensive);
        printf("  callback diagnostics: balanced shops %d | calls %d | eligible %d; defensive shops %d | calls %d | eligible %d\n",
               noConvertBalancedFloor.shopsVisited, noConvertBalanced.calls,
               noConvertBalanced.eligible, noConvertDefensiveFloor.shopsVisited,
               noConvertDefensive.calls, noConvertDefensive.eligible);
        puts("  No-conversion policies skip unaffordable perks and buy only from held marks; they never exchange chips for marks.");
        print_conversion_breakdown("  Balanced purchase distribution:", &noConvertBalanced);
        print_conversion_breakdown("  Defensive purchase distribution:", &noConvertDefensive);

        puts("\nChip-conversion reserve experiment (same seeds; balanced priority)");
        {
            const int reserveMultiples[] = {1, 2, 3};
            const char *reserveLabels[] = {
                "one minimum-bet reserve",
                "two minimum-bet reserve (production)",
                "three minimum-bet reserve"
            };
            int reserveIndex;
            for (reserveIndex = 0; reserveIndex < 3; reserveIndex++) {
                ReportConversionContext conversion = {0};
                ReportFloorContext conversionFloor = {0};
                SimulationBatchStats conversionStats;
                conversion.reserveBetMultiple = reserveMultiples[reserveIndex];
                if (!simulateRoguelikeBatch(
                        seeds, REPORT_RUNS, REPORT_ROUNDS_PER_RUN,
                        minimum_bet_policy, NULL, progressing_policy, NULL,
                        visit_shop_then_advance_policy, &conversionFloor,
                        NULL, NULL, no_conversion_shop_policy, &conversion,
                        &conversionStats)) {
                    fprintf(stderr, "error: conversion reserve experiment failed (%dx)\n",
                            reserveMultiples[reserveIndex]);
                    return 1;
                }
                print_conversion_comparison(
                    reserveLabels[reserveIndex], &baseline,
                    &conversionStats, &conversion);
                printf("    exchanges %d | chips spent %d | marks created %d\n",
                       conversion.conversionTrades, conversion.chipsConverted,
                       conversion.marksFromConversion);
                if (conversion.purchases !=
                    conversion.purchasesFromHeldMarks +
                    conversion.purchasesFundedByConversion) {
                    fprintf(stderr,
                            "error: reserve experiment purchase funding does not reconcile (%d total, %d held, %d converted)\n",
                            conversion.purchases,
                            conversion.purchasesFromHeldMarks,
                            conversion.purchasesFundedByConversion);
                    return 1;
                }
                if (reserveMultiples[reserveIndex] == 2) {
                    double chipDelta =
                        conversionStats.totalFinalChips /
                            (double)conversionStats.runsCompleted -
                        balanced.totalFinalChips / (double)balanced.runsCompleted;
                    double floorDelta =
                        conversionStats.totalFloorsReached /
                            (double)conversionStats.runsCompleted -
                        balanced.totalFloorsReached / (double)balanced.runsCompleted;
                    printf("    production-policy cross-check: chips %+.2f | floors %+.2f vs built-in balanced\n",
                           chipDelta, floorDelta);
                }
            }
        }
    }
        puts("\nChip-conversion price and priority experiment (same seeds; 2-mark transactions)");
        {
            const int chipCosts[] = {5, 10, 15};
            const char *priceLabels[] = {
                "5 chips / 2 marks",
                "10 chips / 2 marks",
                "15 chips / 2 marks"
            };
            const char *priorityLabels[] = {"balanced", "defensive"};
            int priceIndex;
            int strategyIndex;
            for (priceIndex = 0; priceIndex < 3; priceIndex++) {
                for (strategyIndex = 0; strategyIndex < 2; strategyIndex++) {
                    ReportConversionContext conversion = {0};
                    ReportFloorContext conversionFloor = {0};
                    SimulationBatchStats conversionStats;
                    conversion.reserveBetMultiple = 2;
                    conversion.chipCostPerTrade = chipCosts[priceIndex];
                    conversion.marksPerTrade = 2;
                    conversion.strategy = strategyIndex;
                    {
                        char label[64];
                        snprintf(label, sizeof(label), "%s, %s",
                                 priceLabels[priceIndex],
                                 priorityLabels[strategyIndex]);
                        if (!simulateRoguelikeBatch(
                                seeds, REPORT_RUNS, REPORT_ROUNDS_PER_RUN,
                                minimum_bet_policy, NULL, progressing_policy, NULL,
                                visit_shop_then_advance_policy, &conversionFloor,
                                NULL, NULL, conversion_rate_shop_policy, &conversion,
                                &conversionStats)) {
                            fprintf(stderr,
                                    "error: conversion price/priority experiment failed (%d chips, strategy %d)\n",
                                    chipCosts[priceIndex], strategyIndex);
                            return 1;
                        }
                        print_conversion_comparison(
                            label, &baseline, &conversionStats, &conversion);
                        print_conversion_comparison(
                            strategyIndex == 0
                                ? "  vs held-marks-only balanced"
                                : "  vs held-marks-only defensive",
                            strategyIndex == 0 ? &noConvertBalancedStats
                                               : &noConvertDefensiveStats,
                            &conversionStats, &conversion);
                    }
                    printf("    exchanges %d | chips spent %d | marks created %d\n",
                           conversion.conversionTrades, conversion.chipsConverted,
                           conversion.marksFromConversion);
                    if (chipCosts[priceIndex] == 10 && strategyIndex == 0) {
                        printf("    production-policy cross-check: chips %+.2f | floors %+.2f vs built-in balanced\n",
                               conversionStats.totalFinalChips /
                                   (double)conversionStats.runsCompleted -
                                   balanced.totalFinalChips /
                                   (double)balanced.runsCompleted,
                               conversionStats.totalFloorsReached /
                                   (double)conversionStats.runsCompleted -
                                   balanced.totalFloorsReached /
                                   (double)balanced.runsCompleted);
                    }
                }
            }
        }

    puts("\nIsolated marks utility experiments (same seeds; one service action per shop visit)");
    {
        const ReportUtilityMode modes[] = {
            REPORT_UTILITY_SHOP_REROLL,
            REPORT_UTILITY_TWIST_TOOLS,
            REPORT_UTILITY_PERK_ONLY,
            REPORT_UTILITY_BOSS_PREP_ONLY
        };
        const char *labels[] = {
            "shop-pool reroll only",
            "hostile-twist tools only",
            "held-mark perks only",
            "boss-rule disable only"
        };
        int modeIndex;
        for (modeIndex = 0; modeIndex < 4; modeIndex++) {
            ReportUtilityContext utility = {0};
            ReportFloorContext utilityFloor = {0};
            SimulationBatchStats utilityStats;
            utility.mode = modes[modeIndex];
            utility.lastServiceFloor = -1;
            if (!simulateRoguelikeBatch(
                    seeds, REPORT_RUNS, REPORT_ROUNDS_PER_RUN,
                    minimum_bet_policy, NULL, progressing_policy, NULL,
                    visit_shop_then_advance_policy, &utilityFloor,
                    report_boss_prep_policy, &utility,
                    report_utility_shop_policy, &utility, &utilityStats)) {
                fprintf(stderr, "error: isolated marks utility experiment failed (%s)\n",
                        labels[modeIndex]);
                return 1;
            }
            printf("  %-25s chips %+.2f | floors %+.2f | bosses %+.2f | bankrupt %+.1f pp\n",
                   labels[modeIndex],
                   utilityStats.totalFinalChips / (double)utilityStats.runsCompleted -
                       baseline.totalFinalChips / (double)baseline.runsCompleted,
                   utilityStats.totalFloorsReached / (double)utilityStats.runsCompleted -
                       baseline.totalFloorsReached / (double)baseline.runsCompleted,
                   utilityStats.totalBossesDefeated / (double)utilityStats.runsCompleted -
                       baseline.totalBossesDefeated / (double)baseline.runsCompleted,
                   100.0 * ((double)utilityStats.bankruptRuns / utilityStats.runsCompleted -
                            (double)baseline.bankruptRuns / (double)baseline.runsCompleted));
            printf("    boss prep: eligible %d | disabled %d | marks %lld; shop rerolls %d | twist rerolls %d | blocks %d | service marks %lld | perks %d\n",
                   utility.bossPrepOffers, utility.bossRulesDisabled,
                   utility.bossMarksSpent, utility.shopRerolls,
                   utility.twistRerolls, utility.twistBlocks,
                   utility.serviceMarksSpent, utility.perkPurchases);
            if (modeIndex == REPORT_UTILITY_BOSS_PREP_ONLY) {
                static const DealerId bossIds[] = {
                    DEALER_BOSS_PIT, DEALER_BOSS_PRIME,
                    DEALER_BOSS_SUPREME, DEALER_BOSS_VOID
                };
                int bossIndex;
                for (bossIndex = 0;
                     bossIndex < (int)(sizeof(bossIds) / sizeof(bossIds[0]));
                     bossIndex++) {
                    DealerId bossId = bossIds[bossIndex];
                    const DealerInfo *info = getDealerInfo(bossId);
                    int prep = utilityStats.bossPrepEncountersByDealer[bossId];
                    int disabled = utilityStats.bossRuleDisabledByDealer[bossId];
                    int defeated = utilityStats.bossDefeatsByDealer[bossId];
                    int disabledDefeats =
                        utilityStats.bossDefeatsWithRuleDisabledByDealer[bossId];
                    int enabledPrep = prep - disabled;
                    int enabledDefeats = defeated - disabledDefeats;
                    if (disabled > prep || defeated > prep ||
                        disabledDefeats > disabled ||
                        enabledDefeats > enabledPrep) {
                        fprintf(stderr, "error: boss outcome accounting does not reconcile for %s\n",
                                info->name);
                        return 1;
                    }
                    printf("      %-20s defeated %d/%d (%.1f%%) | rule-off %d/%d (%.1f%%) | rule-on %d/%d (%.1f%%) | unaffordable %d | marks %lld\n",
                           info->name,
                           defeated, prep,
                           prep > 0 ? 100.0 * defeated / prep : 0.0,
                           disabledDefeats, disabled,
                           disabled > 0 ? 100.0 * disabledDefeats / disabled : 0.0,
                           enabledDefeats, enabledPrep,
                           enabledPrep > 0 ? 100.0 * enabledDefeats / enabledPrep : 0.0,
                           utility.bossPrepUnaffordableByDealer[bossId],
                           utility.bossMarksSpentByDealer[bossId]);
                }
            }
            if (utility.serviceMarksSpent !=
                utility.shopRerolls * 10LL +
                utility.twistRerolls * 30LL +
                utility.twistBlocks * 40LL) {
                fprintf(stderr, "error: service mark-spend accounting does not reconcile (%s)\n",
                        labels[modeIndex]);
                return 1;
            }
        }
    }

    puts("\nSingle-perk purchases (delta versus no-purchase baseline)");
    {
        static const PerkId evaluatedPerks[] = {
            PERK_SECOND_WIND, PERK_PEEK, PERK_SURVIVAL, PERK_ACE_HIGH,
            PERK_SIXTH_SENSE, PERK_DEALERS_FAVOR, PERK_SPLIT_MASTERY,
            PERK_DOUBLE_DIP, PERK_COMPOUNDING, PERK_INSURANCE_PERK,
            PERK_MARKED_CARD, PERK_BANKROLL, PERK_EXTRA_LIFE
        };
        int perkIndex;
        for (perkIndex = 0;
             perkIndex < (int)(sizeof(evaluatedPerks) / sizeof(evaluatedPerks[0]));
             perkIndex++) {
            ReportFloorContext floorContext = {0};
            ReportSinglePerkContext perkContext = {evaluatedPerks[perkIndex], 0, 0, 0, 0LL, 0LL};
            SimulationBatchStats perkStats;
            const char *name = perkName(evaluatedPerks[perkIndex]);
            if (!simulateRoguelikeBatch(
                    seeds, REPORT_RUNS, REPORT_ROUNDS_PER_RUN,
                    minimum_bet_policy, NULL, progressing_policy, NULL,
                    visit_shop_then_advance_policy, &floorContext,
                    NULL, NULL, single_perk_shop_policy, &perkContext,
                    &perkStats)) {
                fprintf(stderr, "error: single-perk simulation failed for %s\n", name);
                return 1;
            }
            print_perk_comparison(name, &baseline, &perkStats,
                                  perkContext.offers, perkContext.affordable,
                                  perkContext.purchases, perkContext.marksAtOffer,
                                  perkContext.marksShort);
        }
    }
    puts("\nNote: same seeds and baseline policies are used for each perk. "
         "Only affordable offers are purchased; this remains a strategy sample, "
         "not a balance verdict.");
    return 0;
}
