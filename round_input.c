#define _DEFAULT_SOURCE
#include "round_input.h"
#include "round_presentation.h"
#include "round_economy.h"


#include "input.h"
#include "input_map.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

KeyEvent makeKeyEvent(char inputCharacter) {
    KeyEvent event;

    event.type = KEY_NONE;
    event.ch = '\0';

    if (inputCharacter == EOF) {
        event.type = KEY_EOF;
    } else if (inputCharacter == '\r' || inputCharacter == '\n') {
        event.type = KEY_ENTER;
    } else if (inputCharacter == 27) {
        event.type = KEY_ESC;
    } else {
        event.type = KEY_CHAR;
        event.ch = (char)inputCharacter;
    }

    return event;
}

int readBet(Game *game) {
    char input[64];
    char prompt[64];
    char message[64];
    char rawInput[9];
    char *endPointer;
    long betValue;
    size_t delimiterPosition;
    size_t rawLength;
    int minBet;
    int maxBet;
    int validBet;
    int betDivisor;
    KeyEvent event;

    minBet = getMinBet(game, game->run.floor, game->mode);
    maxBet = getMaxBet(game, minBet);
    betDivisor = dealerHandsCount(game->run.dealerId);

    if (game->player.chips < minBet) {
        snprintf(prompt,
                 sizeof(prompt),
                 "Minimum bet is %d. You have %d chips. Run over.",
                 minBet,
                 maxBet);
        roundPresentMessage(prompt, ROUND_MESSAGE_RESULT, MSG_DEFAULT);
        game->state = STATE_QUIT;
        return -1;
    }

    if (inputIsRaw()) {
        rawLength = 0;
        rawInput[0] = '\0';

        while (1) {
            snprintf(prompt,
                     sizeof(prompt),
                     "Enter bet (%d-%d): %s",
                     minBet,
                     maxBet,
                     rawInput);
            roundPresentMessage(prompt, ROUND_MESSAGE_PROMPT, MSG_DEFAULT);

            event = readKey();

            if (event.type == KEY_CHAR) {
                if (event.ch >= '0' &&
                    event.ch <= '9') {
                    if (rawLength < 8) {
                        rawInput[rawLength] = event.ch;
                        rawLength++;
                        rawInput[rawLength] = '\0';
                    }
                } else if (event.ch == 0x7F || event.ch == 0x08) {
                    if (rawLength > 0) {
                        rawLength--;
                        rawInput[rawLength] = '\0';
                    }
                }
            } else if (event.type == KEY_ENTER) {
                if (rawLength == 0) {
                    roundPresentMessage("Enter a bet.",
                                  ROUND_MESSAGE_RESULT,
                                  MSG_DEFAULT);
                } else {
                    betValue = strtol(rawInput, &endPointer, 10);

                    if (*endPointer == '\0' &&
                        betValue >= minBet &&
                        betValue <= maxBet) {
                        if (betDivisor > 1 && betValue % betDivisor != 0) {
                            snprintf(message,
                                     sizeof(message),
                                     "Invalid bet. Enter a multiple of %d between %d and %d.",
                                     betDivisor,
                                     minBet,
                                     maxBet);
                            roundPresentMessage(message, ROUND_MESSAGE_RESULT, MSG_DEFAULT);
                            validBet = 0;
                        } else {
                            validBet = 1;
                        }
                    } else {
                        snprintf(message,
                                 sizeof(message),
                                 "Invalid bet. Enter an integer between %d and %d.",
                                 minBet,
                                 maxBet);
                        roundPresentMessage(message,
                                      ROUND_MESSAGE_RESULT,
                                      MSG_DEFAULT);
                        validBet = 0;
                    }

                    if (validBet) {
                        return (int)betValue;
                    }
                }
            } else if (event.type == KEY_ESC ||
                       event.type == KEY_EOF) {
                return -1;
            }
        }
    } else {
        snprintf(prompt,
                 sizeof(prompt),
                 "Enter bet (%d-%d): ",
                 minBet,
                 maxBet);

        validBet = 0;

        while (!validBet) {
            roundPresentMessage(prompt, ROUND_MESSAGE_PROMPT, MSG_DEFAULT);

            if (fgets(input, sizeof(input), stdin) == NULL) {
                if (ferror(stdin) && errno == EINTR) {
                    clearerr(stdin);
                    continue;
                }

                return -1;
            }

            delimiterPosition = strcspn(input, "\r\n");

            if (delimiterPosition == sizeof(input) - 1) {
                int ch;

                while ((ch = getchar()) != '\n' && ch != EOF) {
                }

                snprintf(message,
                         sizeof(message),
                         "Invalid bet. Enter an integer between %d and %d.",
                         minBet,
                         maxBet);
                roundPresentMessage(message, ROUND_MESSAGE_RESULT, MSG_DEFAULT);
                continue;
            }

            if (input[delimiterPosition] != '\0') {
                input[delimiterPosition] = '\0';
            }

            betValue = strtol(input, &endPointer, 10);

            while (isspace((unsigned char)*endPointer)) {
                endPointer++;
            }

            if (input[0] != '\0' &&
                *endPointer == '\0' &&
                betValue >= minBet &&
                betValue <= maxBet) {
                if (betDivisor > 1 && betValue % betDivisor != 0) {
                    snprintf(message,
                             sizeof(message),
                             "Invalid bet. Enter a multiple of %d between %d and %d.",
                             betDivisor,
                             minBet,
                             maxBet);
                    roundPresentMessage(message, ROUND_MESSAGE_RESULT, MSG_DEFAULT);
                    validBet = 0;
                } else {
                    validBet = 1;
                }
            } else {
                snprintf(message,
                         sizeof(message),
                         "Invalid bet. Enter an integer between %d and %d.",
                         minBet,
                         maxBet);
                roundPresentMessage(message, ROUND_MESSAGE_RESULT, MSG_DEFAULT);
            }
        }

        return (int)betValue;
    }
}

int readYesNoPrompt(const char *prompt) {
    while (1) {
        int answer;

        roundPresentMessage(prompt, ROUND_MESSAGE_PROMPT, MSG_DEFAULT);

        if (inputIsRaw()) {
            KeyEvent event;

            event = readKey();
            if (event.type == KEY_ESC || event.type == KEY_EOF) {
                return 0;
            }
            if (event.type != KEY_CHAR) {
                continue;
            }
            answer = (unsigned char)event.ch;
        } else {
            answer = getchar();

            if (answer == EOF) {
                return 0;
            }

            if (answer != '\n' && answer != '\r') {
                while (1) {
                    int character;

                    character = getchar();
                    if (character == '\n' || character == EOF) {
                        break;
                    }
                }
            }
        }

        if (answer == 'y' || answer == 'Y') {
            return 1;
        }
        if (answer == 'n' || answer == 'N') {
            return 0;
        }
    }
}
