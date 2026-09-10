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

To be documented.

## Project status

- [x] Stage 1: Card deck, shuffle, basic structures.
- [ ] Stage 2: Hand logic and score calculation.
- [ ] Stage 3: Game loop.
- [ ] Stage 4: ASCII rendering.
- [ ] Stage 5: Raw terminal input.
- [ ] Stage 6: Floors and dealer modifiers.
- [ ] Stage 7: Perks and shop.
- [ ] Stage 8: Split and Double Down.
- [ ] Stage 9: Save and load.
- [ ] Stage 10: Polish and animations.

## License

MIT
