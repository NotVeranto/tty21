#include "game.h"
#include "input.h"
#include "render.h"
#include "rng.h"
#include "version.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static void printUsage(FILE *stream) {
    fprintf(stream,
            "Usage: tty21 [OPTIONS]\n"
            "Terminal blackjack roguelike.\n"
            "\n"
            "Options:\n"
            "  -h, --help      Show this help and exit.\n"
            "  -v, --version   Show version and exit.\n"
            "  -s, --seed N    Use N as the RNG seed (default: time-based).\n"
            "  -r, --roguelike Start in roguelike mode (default).\n"
            "  -c, --classic   Start in classic blackjack mode.\n"
            "\n"
            "Requires a terminal of at least 80x50.\n");
}

static const char *LOGO[] = {
    "TTTTT TTTTT Y   Y 2222    1",
    "  T     T    Y Y  2  2   11",
    "  T     T     Y     2     1",
    "  T     T     Y    2      1",
    "  T     T     Y   2222  111"
};
static const int LOGO_ROWS = 5;

static int getStartupWidth(void) {
    struct winsize windowSize;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &windowSize) == 0 &&
        windowSize.ws_col > 0) {
        return windowSize.ws_col;
    }

    return 80;
}

static void printCentered(const char *text, int width) {
    int padding;
    int textWidth;

    textWidth = (int)strlen(text);
    padding = (width - textWidth) / 2;

    if (padding < 0) {
        padding = 0;
    }

    printf("%*s%s\n", padding, "", text);
}

static void printLogo(int width) {
    int logoIndex;

    for (logoIndex = 0; logoIndex < LOGO_ROWS; logoIndex++) {
        printCentered(LOGO[logoIndex], width);
    }
}

static void printStartupItem(const MenuItem *item,
                             int selected,
                             int width,
                             int colors) {
    int rowWidth;
    int padding;

    rowWidth = 2 + 3 + 2 + (int)strlen(item->label);
    padding = (width - rowWidth) / 2;

    if (padding < 0) {
        padding = 0;
    }

    printf("%*s", padding, "");

    if (selected) {
        if (colors) {
            printf("\033[1m> \033[0m");
        } else {
            printf("> ");
        }
    } else {
        printf("  ");
    }

    if (colors) {
        printf("\033[2m[%c]\033[0m  ", item->hotkey);
    } else {
        printf("[%c]  ", item->hotkey);
    }

    if (selected) {
        if (colors) {
            printf("\033[1m%s\033[0m\n", item->label);
        } else {
            printf("%s\n", item->label);
        }
    } else {
        printf("%s\n", item->label);
    }
}

static GameMode runStartupMenu(void) {
    MenuItem items[3];
    char input[64];
    int selectedIndex;
    int characterIndex;
    int width;
    int colors;
    int ruleLength;
    int menuIndex;
    int padding;
    KeyEvent event;

    items[0].hotkey = '1';
    items[0].label = "Roguelike";
    items[1].hotkey = '2';
    items[1].label = "Classic blackjack";
    items[2].hotkey = 'q';
    items[2].label = "Quit";

    selectedIndex = 0;

    while (1) {
        if (isatty(STDOUT_FILENO)) {
            struct winsize ws;
            int w = 0;
            int h = 0;

            if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
                w = ws.ws_col;
                h = ws.ws_row;
            }

            if (w < MIN_TERMINAL_WIDTH || h < MIN_TERMINAL_HEIGHT) {
                renderTooSmallScreen(w, h);

                if (inputIsRaw()) {
                    (void)readKey();
                } else {
                    (void)getchar();
                }

                continue;
            }
        }

        width = getStartupWidth();
        colors = getenv("NO_COLOR") == NULL &&
                 isatty(STDOUT_FILENO);

        renderHideCursor();

        if (isatty(STDOUT_FILENO) && colors) {
            printf("\033[2J\033[H");
        }

        printf("\n");
        printLogo(width);
        printf("\n");

        padding = (width - (int)strlen("Terminal blackjack roguelike")) / 2;

        if (padding < 0) {
            padding = 0;
        }

        printf("%*s", padding, "");

        if (colors) {
            printf("\033[2m");
        }

        printf("Terminal blackjack roguelike");

        if (colors) {
            printf("\033[0m");
        }

        printf("\n\n");

        ruleLength = width - 4;

        if (ruleLength > 60) {
            ruleLength = 60;
        }

        if (ruleLength < 20) {
            ruleLength = 20;
        }

        if (ruleLength > width) {
            ruleLength = width;
        }

        printf("%*s", (width - ruleLength) / 2, "");

        for (characterIndex = 0;
             characterIndex < ruleLength;
             characterIndex++) {
            printf("-");
        }

        printf("\n\n");

        for (menuIndex = 0; menuIndex < 3; menuIndex++) {
            printStartupItem(&items[menuIndex],
                             menuIndex == selectedIndex,
                             width,
                             colors);
        }

        fflush(stdout);

        if (inputIsRaw()) {
            event = readKey();

            if (event.type == KEY_UP) {
                selectedIndex--;

                if (selectedIndex < 0) {
                    selectedIndex = 2;
                }
            } else if (event.type == KEY_DOWN) {
                selectedIndex++;

                if (selectedIndex >= 3) {
                    selectedIndex = 0;
                }
            } else if (event.type == KEY_ENTER) {
                if (items[selectedIndex].hotkey == '1') {
                    renderShowCursor();
                    return MODE_ROGUELIKE;
                }

                if (items[selectedIndex].hotkey == '2') {
                    renderShowCursor();
                    return MODE_CLASSIC;
                }

                renderShowCursor();
                disableRawMode();
                exit(0);
            } else if (event.type == KEY_CHAR) {
                for (menuIndex = 0; menuIndex < 3; menuIndex++) {
                    if (event.ch == items[menuIndex].hotkey) {
                        selectedIndex = menuIndex;

                        if (items[menuIndex].hotkey == '1') {
                            renderShowCursor();
                            return MODE_ROGUELIKE;
                        }

                        if (items[menuIndex].hotkey == '2') {
                            renderShowCursor();
                            return MODE_CLASSIC;
                        }

                        renderShowCursor();
                        disableRawMode();
                        exit(0);
                    }
                }
            } else if (event.type == KEY_ESC ||
                       event.type == KEY_EOF) {
                renderShowCursor();
                disableRawMode();
                exit(0);
            }
        } else {
            if (fgets(input, sizeof(input), stdin) == NULL) {
                exit(0);
            }

            characterIndex = 0;

            while (input[characterIndex] != '\0' &&
                   isspace((unsigned char)input[characterIndex])) {
                characterIndex++;
            }

            for (menuIndex = 0; menuIndex < 3; menuIndex++) {
                if (input[characterIndex] == items[menuIndex].hotkey) {
                    selectedIndex = menuIndex;

                    if (items[menuIndex].hotkey == '1') {
                        renderShowCursor();
                        return MODE_ROGUELIKE;
                    }

                    if (items[menuIndex].hotkey == '2') {
                        renderShowCursor();
                        return MODE_CLASSIC;
                    }

                    renderShowCursor();
                    exit(0);
                }
            }
        }
    }
}

static int parseSeed(const char *text, unsigned int *seed) {
    char *endPointer;
    unsigned long parsedValue;

    if (text[0] == '\0') {
        return -1;
    }

    errno = 0;
    parsedValue = strtoul(text, &endPointer, 10);

    if (errno == ERANGE ||
        endPointer == text ||
        *endPointer != '\0' ||
        parsedValue > UINT_MAX) {
        return -1;
    }

    *seed = (unsigned int)parsedValue;
    return 0;
}

int main(int argc, char **argv) {
    Game game;
    unsigned int seed;
    int argumentIndex;
    int seedResult;
    const char *seedText;
    int forced;
    GameMode forcedMode;
    GameMode mode;

    seed = 0;
    forced = 0;
    forcedMode = MODE_ROGUELIKE;

    for (argumentIndex = 1; argumentIndex < argc; argumentIndex++) {
        if (strcmp(argv[argumentIndex], "--roguelike") == 0 ||
            strcmp(argv[argumentIndex], "-r") == 0) {
            forced = 1;
            forcedMode = MODE_ROGUELIKE;
            continue;
        }

        if (strcmp(argv[argumentIndex], "--classic") == 0 ||
            strcmp(argv[argumentIndex], "-c") == 0) {
            forced = 1;
            forcedMode = MODE_CLASSIC;
            continue;
        }

        if (strncmp(argv[argumentIndex], "--seed=", 7) == 0) {
            seedText = argv[argumentIndex] + 7;
            seedResult = parseSeed(seedText, &seed);

            if (seedResult != 0) {
                fprintf(stderr, "error: invalid seed '%s'\n", seedText);
                printUsage(stderr);
                return 1;
            }

            continue;
        }

        if (strcmp(argv[argumentIndex], "--seed") == 0 ||
            strcmp(argv[argumentIndex], "-s") == 0) {
            if (argumentIndex + 1 >= argc) {
                fprintf(stderr, "error: missing seed value\n");
                printUsage(stderr);
                return 1;
            }

            argumentIndex++;
            seedText = argv[argumentIndex];
            seedResult = parseSeed(seedText, &seed);

            if (seedResult != 0) {
                fprintf(stderr, "error: invalid seed '%s'\n", seedText);
                printUsage(stderr);
                return 1;
            }

            continue;
        }

        if (strncmp(argv[argumentIndex], "-s", 2) == 0 &&
            argv[argumentIndex][2] != '\0') {
            seedText = argv[argumentIndex] + 2;
            seedResult = parseSeed(seedText, &seed);

            if (seedResult != 0) {
                fprintf(stderr, "error: invalid seed '%s'\n", seedText);
                printUsage(stderr);
                return 1;
            }

            continue;
        }

        if (strcmp(argv[argumentIndex], "-h") == 0 ||
            strcmp(argv[argumentIndex], "--help") == 0) {
            printUsage(stdout);
            return 0;
        }

        if (strcmp(argv[argumentIndex], "-v") == 0 ||
            strcmp(argv[argumentIndex], "--version") == 0) {
            printf("tty21 %s\n", TTY21_VERSION);
            return 0;
        }

        fprintf(stderr, "error: unknown option '%s'\n", argv[argumentIndex]);
        printUsage(stderr);
        return 1;
    }

    setlocale(LC_ALL, "");

    inputInit();

    if (forced) {
        mode = forcedMode;
    } else {
        mode = runStartupMenu();
    }

    rngInit(seed);

    initGame(&game, mode);
    runGame(&game);

    return 0;
}
