#include "test.h"
#include "render.h"

TEST(render_terminal_accepts_minimum_dimensions) {
    ASSERT_TRUE(renderTerminalDimensionsOk(MIN_TERMINAL_WIDTH,
                                           MIN_TERMINAL_HEIGHT));
}

TEST(render_terminal_rejects_insufficient_width) {
    ASSERT_EQ_INT(0, renderTerminalDimensionsOk(MIN_TERMINAL_WIDTH - 1,
                                                MIN_TERMINAL_HEIGHT));
}

TEST(render_terminal_rejects_insufficient_height) {
    ASSERT_EQ_INT(0, renderTerminalDimensionsOk(MIN_TERMINAL_WIDTH,
                                                MIN_TERMINAL_HEIGHT - 1));
}

TEST(render_terminal_accepts_larger_dimensions) {
    ASSERT_TRUE(renderTerminalDimensionsOk(160, 60));
}

void run_render_tests(void) {
    RUN_TEST(render_terminal_accepts_minimum_dimensions);
    RUN_TEST(render_terminal_rejects_insufficient_width);
    RUN_TEST(render_terminal_rejects_insufficient_height);
    RUN_TEST(render_terminal_accepts_larger_dimensions);
}
