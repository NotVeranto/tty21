#include "test.h"

int tty21_tests_run = 0;
int tty21_tests_failed = 0;

void run_hand_tests(void);
void run_card_tests(void);
void run_rng_tests(void);
void run_twists_tests(void);
void run_dealers_tests(void);
void run_perks_tests(void);
void run_run_tests(void);
void run_settlement_tests(void);
void run_action_tests(void);
void run_simulation_tests(void);
void run_save_tests(void);
void run_render_tests(void);

int main(void) {
    printf("tty21 unit tests\n");
    printf("================\n\n");

    run_hand_tests();
    run_card_tests();
    run_rng_tests();
    run_twists_tests();
    run_dealers_tests();
    run_perks_tests();
    run_run_tests();
    run_settlement_tests();
    run_action_tests();
    run_simulation_tests();
    run_save_tests();
    run_render_tests();

    TEST_SUMMARY();
}
