#include "test.h"
#include "terminal.h"
#include "render_layout.h"
#include "render_style.h"

#include <string.h>

TEST(render_terminal_accepts_minimum_dimensions) {
    ASSERT_TRUE(terminalDimensionsOk(MIN_TERMINAL_WIDTH,
                                       MIN_TERMINAL_HEIGHT));
}

TEST(render_terminal_rejects_insufficient_width) {
    ASSERT_EQ_INT(0, terminalDimensionsOk(MIN_TERMINAL_WIDTH - 1,
                                            MIN_TERMINAL_HEIGHT));
}

TEST(render_terminal_rejects_insufficient_height) {
    ASSERT_EQ_INT(0, terminalDimensionsOk(MIN_TERMINAL_WIDTH,
                                            MIN_TERMINAL_HEIGHT - 1));
}

TEST(render_terminal_accepts_larger_dimensions) {
    ASSERT_TRUE(terminalDimensionsOk(160, 60));
}

TEST(render_style_disables_ansi_for_redirected_output) {
    ASSERT_EQ_INT(0, renderTerminalSupportsAnsi(0, "xterm-256color", NULL));
}

TEST(render_style_disables_ansi_for_dumb_terminal) {
    ASSERT_EQ_INT(0, renderTerminalSupportsAnsi(1, "dumb", NULL));
}

TEST(render_style_disables_ansi_when_no_color_is_set) {
    ASSERT_EQ_INT(0, renderTerminalSupportsAnsi(1, "xterm-256color", ""));
}

TEST(render_style_enables_ansi_for_supported_terminal) {
    ASSERT_EQ_INT(1, renderTerminalSupportsAnsi(1, "xterm-256color", NULL));
}

TEST(render_style_rejects_missing_terminal_type) {
    ASSERT_EQ_INT(0, renderTerminalSupportsAnsi(1, NULL, NULL));
}

TEST(render_cursor_is_addressable_in_supported_terminal) {
    ASSERT_EQ_INT(1, renderCursorAddressableFor(1, "xterm-256color"));
}

TEST(render_cursor_is_not_addressable_when_output_is_redirected) {
    ASSERT_EQ_INT(0, renderCursorAddressableFor(0, "xterm-256color"));
}

TEST(render_cursor_is_not_addressable_for_dumb_terminal) {
    ASSERT_EQ_INT(0, renderCursorAddressableFor(1, "dumb"));
}

TEST(render_cursor_rejects_missing_terminal_type) {
    ASSERT_EQ_INT(0, renderCursorAddressableFor(1, NULL));
}

TEST(render_layout_centers_odd_width_content) {
    ASSERT_EQ_INT(31, getCenteredColumnForWidth(80, 20));
}

TEST(render_layout_centers_even_width_content) {
    ASSERT_EQ_INT(31, getCenteredColumnForWidth(80, 19));
}

TEST(render_layout_left_aligns_content_wider_than_terminal) {
    ASSERT_EQ_INT(1, getCenteredColumnForWidth(40, 60));
}

TEST(render_plain_screen_header_centers_title) {
    char line[64];

    formatPlainScreenHeader(line, sizeof(line), 40, "TTY21 - SHOP");
    ASSERT_TRUE(strcmp(line, "              TTY21 - SHOP") == 0);
}

TEST(render_plain_screen_header_left_aligns_when_title_is_wide) {
    char line[64];

    formatPlainScreenHeader(line, sizeof(line), 8, "LONG TITLE");
    ASSERT_TRUE(strcmp(line, "LONG TITLE") == 0);
}

TEST(render_plain_menu_item_formats_selection) {
    char line[64];

    formatPlainMenuItem(line, sizeof(line), 'H', "Hit", 1, 0);
    ASSERT_TRUE(strcmp(line, "> [H]  Hit") == 0);
}

TEST(render_plain_menu_item_formats_disabled_entries) {
    char line[64];

    formatPlainMenuItem(line, sizeof(line), 'S', "Split", 0, 1);
    ASSERT_TRUE(strcmp(line, "  [S]  Split (disabled)") == 0);
}

TEST(render_plain_menu_item_formats_selected_disabled_entries) {
    char line[64];

    formatPlainMenuItem(line, sizeof(line), 'D', "Double", 1, 1);
    ASSERT_TRUE(strcmp(line, "> [D]  Double (disabled)") == 0);
}

void run_render_tests(void) {
    RUN_TEST(render_terminal_accepts_minimum_dimensions);
    RUN_TEST(render_terminal_rejects_insufficient_width);
    RUN_TEST(render_terminal_rejects_insufficient_height);
    RUN_TEST(render_terminal_accepts_larger_dimensions);
    RUN_TEST(render_style_disables_ansi_for_redirected_output);
    RUN_TEST(render_style_disables_ansi_for_dumb_terminal);
    RUN_TEST(render_style_disables_ansi_when_no_color_is_set);
    RUN_TEST(render_style_enables_ansi_for_supported_terminal);
    RUN_TEST(render_style_rejects_missing_terminal_type);
    RUN_TEST(render_cursor_is_addressable_in_supported_terminal);
    RUN_TEST(render_cursor_is_not_addressable_when_output_is_redirected);
    RUN_TEST(render_cursor_is_not_addressable_for_dumb_terminal);
    RUN_TEST(render_cursor_rejects_missing_terminal_type);
    RUN_TEST(render_layout_centers_odd_width_content);
    RUN_TEST(render_layout_centers_even_width_content);
    RUN_TEST(render_layout_left_aligns_content_wider_than_terminal);
    RUN_TEST(render_plain_screen_header_centers_title);
    RUN_TEST(render_plain_screen_header_left_aligns_when_title_is_wide);
    RUN_TEST(render_plain_menu_item_formats_selection);
    RUN_TEST(render_plain_menu_item_formats_disabled_entries);
    RUN_TEST(render_plain_menu_item_formats_selected_disabled_entries);
}
