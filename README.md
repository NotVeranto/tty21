# tty21

A terminal-based blackjack roguelike written in C.

## Concept

The player descends through floors of an underground casino, facing a
different dealer on each floor. Winning a match lets you move deeper,
where the dealers get stronger and the rules twist. Between floors,
souls earned from victories can be spent on perks.

## Features

- Classic blackjack rules: Hit, Stand, Double Down, Split.
- Roguelike progression: floors, perks, shop between runs.
- Pure ASCII / Unicode terminal interface.
- Save and load support.

## Requirements

- GCC or Clang with C11 support.
- A terminal with UTF-8 locale (for card suit symbols).
- Linux (uses termios and ANSI escape codes).

## Build

    make

## Run

    ./tty21

## Controls

In-game (during the player's turn):
- `h` — hit
- `s` — stand
- `q` — quit the current run
- `Ctrl+C` — force quit, terminal state restored

Betting:
- Type a number and press Enter.

## Project status

- [x] Card deck, shuffle, basic structures.
- [x] Hand logic and score calculation.
- [x] Game loop.
- [x] CLI flags, versioning and install targets.
- [x] ASCII rendering.
- [x] Raw terminal input.
- [ ] Floors and dealer modifiers.
- [ ] Perks and shop.
- [ ] Split and Double Down.
- [ ] Save and load (XDG).
- [ ] Polish and animations.
- [ ] Man page.
- [ ] Packaging and release.

## License

MIT
