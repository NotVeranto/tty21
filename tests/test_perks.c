#include "test.h"
#include "perks.h"

#include <string.h>

TEST(perk_zero_is_second_wind) {
    const PerkInfo *info;

    info = getPerkInfo(0);
    ASSERT_TRUE(info != NULL);
    ASSERT_TRUE(strcmp(info->name, "Second Wind") == 0);
}

TEST(perk_invalid_ids_return_null) {
    ASSERT_TRUE(getPerkInfo(PERK_COUNT) == NULL);
    ASSERT_TRUE(getPerkInfo((PerkId)-1) == NULL);
}

TEST(perk_all_defined_costs_are_positive) {
    PerkId id;

    for (id = PERK_SECOND_WIND; id < PERK_COUNT;
         id = (PerkId)(id + 1)) {
        ASSERT_TRUE(getPerkInfo(id) != NULL);
        ASSERT_TRUE(perkCost(id) > 0);
    }
}

TEST(perk_name_for_count_is_empty) {
    ASSERT_TRUE(strcmp(perkName(PERK_COUNT), "") == 0);
}

void run_perks_tests(void) {
    RUN_TEST(perk_zero_is_second_wind);
    RUN_TEST(perk_invalid_ids_return_null);
    RUN_TEST(perk_all_defined_costs_are_positive);
    RUN_TEST(perk_name_for_count_is_empty);
}
