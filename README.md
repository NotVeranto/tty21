# tty21

A terminal-based blackjack roguelike written in C.

## Concept

The player descends through floors of an underground casino, facing a
different dealer on each floor. Winning a match lets you move deeper,
where the dealers get stronger and the rules twist. Between floors,
marks earned from victories can be spent on perks.

## Features

Implemented:
- Classic blackjack: Hit, Stand, Double Down, Split.
- Canonical dealer peek, insurance, and even money.
- Classic blackjack mode (no roguelike layer).
- Roguelike progression: floors, marks, advance menu.
- Full-screen ASCII table with ANSI styling.
- Raw terminal input via termios.
- Twists: 16 floor rule modifiers with anti-farm pressure. Twist banner is colored by category: green favorable, dim neutral, red hostile.
- Cold snap (floor 8+) and locked action (floor 10+): once per run, a low-card hand or a disabled perk.
- No Reshuffle floors show a live deck countdown. Floor 10+ shows a 'running hot' hint when low cards are depleted.
- Deterministic runs via --seed.
- Perks and between-floor shop (rotating pool of 3 perks per floor, 13 in pool).
- Dealer turn step-through with optional --fast mode.
- Dealer taunts on losses.
- Cursed cards (3% of drawn cards): Bleed (-1 chip per round), Frost (-2 to hand value), Echo (10% chance of an extra card on stand). Cursed markers are colored on the card (Bleed red, Frost cyan, Echo yellow).
- Boss floors every 5 floors: two combined dealer rules, one-time marks reward on victory.
- Quadratic minimum bets, late-floor bet caps, and an enraged dealer from floor 10 after three rounds.
- Deep-floor dealer power rules from floor 10, including increased losses, restricted actions, and a chip toll on hits.
- Forced hostile twists every third floor from floor 10, except boss floors.
- Tighter roguelike economy: advance fees, 10 chips converted to 2 marks, and reduced floor-advance and boss marks rewards.

Planned:
- Save and load (XDG).

## Requirements

- GCC or Clang with C11 support.
- A terminal with UTF-8 locale (for card suit symbols).
- Linux (uses termios and ANSI escape codes).
- A terminal window of at least 80x50 characters.

## Build

    make

## Run

    ./tty21
    ./tty21 --roguelike
    ./tty21 --classic
    ./tty21 --seed 12345

## Controls

In-game (during the player's turn):
- `h` — hit
- `s` — stand
- `d` — double down (first two cards only)
- `p` — split (two cards of the same rank)
- `k` — peek at dealer hole card (requires the Peek perk or a free-peek floor event)
- `q` — quit the current run
- `?` — show help overlay (active twists, dealer rule, owned perks, current limits)
- Arrow keys + Enter — navigate and confirm menus and player actions
- `Ctrl+C` — force quit, terminal state restored

Betting:
- Type a number and press Enter.
- Insurance and even money prompts appear after the initial deal when applicable.

### Between rounds

- `c` — Continue on current floor
- `a` — Advance to the next floor (advance fee plus the next floor's minimum bet must both be covered)
- `s` — Open the shop (perks, services, twist tools; roguelike only)
- Shop shows a rotating pool of 3 perks per floor. Services and twist tools are always available. Reroll costs 10 marks, once per floor.
- `q` — Quit the run

## Project status

- [x] Card deck, shuffle, basic structures.
- [x] Hand logic and score calculation.
- [x] Game loop.
- [x] CLI flags, versioning and install targets.
- [x] ASCII rendering.
- [x] Persistent event log with colored events.
- [x] Between-round card persistence.
- [x] Raw terminal input.
- [x] Floors and advance menu.
- [x] Named dealers.
- [x] Twists.
- [x] Perks and shop.
- [x] Split and Double Down.
- [x] Classic blackjack mode.
- [x] Difficulty expansion A: quadratic minimum bets, late-floor bet caps, and dealer enrage.
- [x] Difficulty expansion B: tighter economy, advance fees, chip-to-mark conversion, and reduced marks rewards.
- [x] Difficulty expansion C: deep-floor dealer power rules and forced hostile twists.
- [ ] Save and load (XDG).
- [ ] Polish and animations.
- [ ] Man page.
- [ ] Packaging and release.

## Known issues

- Signal handlers call tcsetattr; not strictly async-signal-safe.
- Second Wind does not rescue a busted doubled hand.
- Even money is not offered against Twin dealers.

## License

MIT
