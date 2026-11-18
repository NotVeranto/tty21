# tty21

A terminal-based blackjack roguelike written in C.

## Overview

tty21 combines familiar blackjack with roguelike deckbuilder progression. Play in a terminal, defeat increasingly dangerous dealers, and descend through casino floors where rules and challenges change.

## Features

### Economy and floor progression

- The displayed next-floor minimum bet and advance fee are calculated from the next floor's baseline, not from temporary bet modifiers or twists on the floor being left. The new floor's own twist can still change its effective minimum bet after arrival. Roguelike maximum-bet caps are enforced even when the bankroll is too small to reach the minimum bet under that cap; such a run ends instead of silently allowing an over-cap wager.
- Marks earned from floor twists, boss victories, floor events, and shop exchanges are included in the run's earned-marks total.
- The 50-chip shop loan is available when its proceeds bring the bankroll up to the next floor's minimum bet; eligibility accounts for the chips actually advanced to the player. The Bankroll perk's +5 floor-start income is applied before a loan due on that floor is collected.
- A newly selected Greed twist is skipped when the post-transition bankroll cannot cover its doubled minimum bet, so an affordable floor advance cannot leave the run unable to bet.

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
- Perks provide benefits for the current run only; the shop rotates a selection of perks between floors. Double Dip adds half a bet to any doubled win, including split hands. Compounding adds its current streak bonus once to the round's total payout, not once per split hand or dealer hand. Dealer's Favor turns an entire busted hand into a push, including against multiple dealer hands. Nothing unlocks or carries over as a permanent benefit between runs.
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

- `game.c` and the round modules own game state, rules, floor/menu/shop transitions, the ordered shop-service offer catalog, and purchase transactions. `run.c` initializes persistent run data only; minimum- and maximum-bet calculations belong to `round_economy.c` and its public economy interface. `controller.c` owns the state-driven game loop and coordinates round handlers, menus, input, autosaves, boss-preparation input, and round-result presentation; classic menu-to-betting transitions are validated by the game layer; boss-preparation payment and state transition are handled by the game layer; `round_settlement.c` resolves payouts and state without terminal rendering; `menu.c` handles menu input and persistence requests while delegating unaffordable-run termination to the game layer.
- `render.c` composes the main table screen through the focused `render_table.h` interface and owns table layout state, including message alignment. `render_cards.c` owns card/hand drawing and card-derived table-width calculation; `render_layout.c` is limited to reusable centering, framing, and screen-header primitives and does not depend on card rendering. Other focused modules own dealer rules and floor twists (`render_table_info.c`), Oracle next-card and deck intelligence (`render_oracle.c`), status/chip presentation (`render_status.c`), stateless message formatting/output (`render_messages.c`), and the shop (`render_shop.c`).
- `render_fallback.c` provides plain-text game-state output; `render_log.c` owns the persistent event log; `render_terminal.c` handles terminal-size validation and the too-small-screen message.
- Focused renderers expose dedicated public headers for menus, shops, action bars, run summaries, help overlays, and floor/boss transitions. `render.h` remains a compatibility and application-composition header; the controller, menu, round handlers, and terminal input layer include only the renderer interfaces they use. Terminal input depends only on the shared cursor controls, not the aggregate renderer header.
- Round settlement owns payouts, losses, and result state only; the controller owns result-screen presentation. `round_economy.h` exposes chip/economy operations and the floor-advance forecast independently of the round-flow interface, forward-declares `Game`, and imports only `run.h` for `GameMode`; implementation files include the complete game definition where needed. Game and menu modules do not need to import `round.h` just to access economy data. Round handlers depend on `round_presentation.c` for table, prompt, action-bar, help, and pause coordination instead of importing focused renderer modules. The message renderer receives alignment explicitly and owns no shared padding state. Chip balances update immediately without currency animation.
- Shared ANSI styling and capability detection live in `render_style.c`/`render_style.h`; that module owns process-scoped ANSI capability initialization and cursor-visibility tracking. The table compositor in `render.c` owns `tablePadding`, resets it when clearing the screen, and passes it to message output; `render_messages.c` has no mutable file-scope presentation state. ANSI control sequences and fixed screen-row coordinates are isolated in `render_control.h` and `render_rows.h`. Shared centering and box-frame primitives live in `render_layout.c`/`render_layout.h`. `terminal.c` centralizes terminal dimension detection and minimum full-screen layout requirements.
- `input.c` and `input_map.c` handle terminal input and key mapping.
- `make test-core` builds and tests the game-state layer without linking rendering, terminal input, menus, controller, or persistence. Its renderer-free regression cases cover initial state, shop-service availability and purchase transactions, floor-menu/advancement rules, natural-blackjack, regular-win, push, surrender, Insurance across a whole Twin hand, Twin/multiple-dealer, split-hand, payout-cap settlement, boss-preparation payment/affordability, and interrupted-round recovery. `make test-unit`, `make test-smoke`, and `make test-sanitize` provide broader regression, CLI, and sanitizer checks.

## Requirements

- GCC or Clang with C11 support.
- Linux (uses termios and ANSI escape codes).
- A UTF-8 terminal for card suit symbols.
- A terminal window of at least 70x42 characters.

## Build

    make

## Test

    make test-core
    make test-unit
    make test-smoke
    make test

`test-core` compiles a deliberately small set of game-state and rules modules without linking the renderer, input, menu, controller, or save system. `test-unit` runs the broader component and integration suite. `test-smoke` checks command-line startup and error paths. `make test` runs all three. `make test-sanitize` runs the unit and core suites with AddressSanitizer and UndefinedBehaviorSanitizer. CI runs the core regression target with both GCC and Clang, plus the unit, CLI smoke, and sanitizer suites.

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
