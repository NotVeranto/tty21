# tty21

A terminal-based blackjack roguelike written in C.

## Overview

tty21 combines familiar blackjack with roguelike deckbuilder progression. Play in a terminal, defeat increasingly dangerous dealers, and descend through casino floors where rules and challenges change.

## Features

### Blackjack

- Hit, stand, double down, and split.
- Surrender: give up an eligible hand and lose half its bet.
- Dealer peek, insurance, and even money when applicable.
- Multiple dealer hands and special dealer abilities.
- Classic blackjack mode without roguelike progression.

### Roguelike progression

- Descend through floors with escalating minimum bets and dealer difficulty.
- Face a boss every fifth floor. Bosses combine dealer rules and award marks for victory.
- A Boss Preparation screen appears before boss encounters; spend marks to disable the first boss rule.
- Visit the between-floor shop to buy perks, services, and twist tools.
- Perks provide benefits for the current run only; the shop rotates a selection of perks between floors. Nothing unlocks or carries over as a permanent benefit between runs.
- Floor twists modify blackjack rules, sometimes helping the player and sometimes making a floor more dangerous.
- Cursed cards can inflict Bleed, Frost, or Echo effects.
- Deep floors introduce stronger dealer powers, hostile twists, and additional economic pressure.

### Saving and loading

- Roguelike runs autosave to `$XDG_DATA_HOME/tty21/save.dat`, or `~/.local/share/tty21/save.dat` when `XDG_DATA_HOME` is not set to an absolute path.
- Choose **Continue saved run** from the startup menu or launch with `--continue`.
- Use `w` in the between-round menu to save manually.
- Saves are written to a temporary file and renamed into place, so an interrupted write does not replace the previous save.
- `--no-save` disables saving for the current session. Classic mode does not save.
- Save format v1 stores the in-memory game structure; it is not portable across architectures or compatible with saves made using a different game structure.

### Terminal presentation

- Full-screen ASCII table with ANSI styling and colored status feedback.
- Card-deal animations and clear textual feedback for chip changes.
- Dealer turn step-through; `--fast` skips animation pauses.
- Raw terminal controls, keyboard-navigable menus, and an in-game help overlay.

## Architecture

- `game.c` and the round modules own game state and rules; `render.c` owns presentation.
- `terminal.c` centralizes terminal dimension detection and the minimum full-screen layout requirements.
- `input.c` and `input_map.c` handle terminal input and key mapping.
- `make test-unit`, `make test-smoke`, and `make test-sanitize` provide separate logic, CLI, and sanitizer checks.

## Requirements

- GCC or Clang with C11 support.
- Linux (uses termios and ANSI escape codes).
- A UTF-8 terminal for card suit symbols.
- A terminal window of at least 70x42 characters.

## Build

    make

## Test

    make test-unit
    make test-smoke
    make test

`test-unit` runs the game logic and component tests without terminal interaction. `test-smoke` checks command-line startup and error paths. `make test` runs both. `make test-sanitize` runs the unit suite with AddressSanitizer and UndefinedBehaviorSanitizer. CI verifies GCC, Clang, and sanitizer builds.

## Run

    ./tty21
    ./tty21 --roguelike
    ./tty21 --classic
    ./tty21 --seed 12345
    ./tty21 --continue
    ./tty21 --no-save
    ./tty21 --fast

## Command-line options

- `-h`, `--help`: show usage.
- `-v`, `--version`: show the version.
- `-s N`, `--seed N`, `--seed=N`: use a specific RNG seed.
- `-r`, `--roguelike`: start a new roguelike run (the default).
- `-c`, `--classic`: start classic blackjack.
- `--continue`: load the last saved roguelike run.
- `--no-save`: disable autosaving for this session.
- `--fast`: skip dealer animation pauses.

## Controls

During the player's turn:

- `h` — hit.
- `s` — stand.
- `d` — double down (when eligible).
- `p` — split (when eligible).
- `u` — surrender (when eligible).
- `k` — peek at the dealer's hole card (requires the Peek perk or a free-peek event).
- `q` — quit the current run and return to the main menu.
- `?` — show the help overlay.
- Arrow keys and Enter — navigate and select menu or action entries.

Between rounds:

- `c` — continue on the current floor.
- `a` — advance to the next floor (the advance fee and next minimum bet must be affordable).
- `w` — save the current roguelike run.
- `s` — open the shop.
- `q` — quit the run; the save is preserved.

During betting, enter a numeric bet and press Enter. Answer yes/no prompts for insurance and even money when offered. `Ctrl+C` force-quits; the terminal state is restored.

## License

MIT
