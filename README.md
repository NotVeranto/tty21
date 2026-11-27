# tty21

A blackjack roguelike for the terminal, written in C.

Play classic blackjack or descend through casino floors: face tougher dealers and bosses, deal with changing table rules, and spend marks on perks to shape your run. Every run is self-contained.

## What you can do

- Play **Classic** blackjack or a **Roguelike** run.
- Hit, stand, double down, split, surrender, and use insurance when offered.
- Meet special dealers, bosses, floor twists, and cursed cards.
- Buy run-only perks and services between floors.
- Save and continue a roguelike run.
- Use the animated terminal interface, or switch to ASCII and monochrome output.

## Install

On Arch Linux, build the included package recipe from a checkout:

```sh
makepkg -si
```

The recipe is ready for publication to the AUR; the package is not yet listed there.

On other Linux distributions, build and install from source (the install target defaults to `/usr/local`):

```sh
make
sudo make install
```

To stage an installation under a package root, use `make PREFIX=/usr DESTDIR=/path/to/pkg install`.

## Build and run

Requires Linux, GCC or Clang with C11 support, and a terminal.

```sh
make
./tty21
```

With no arguments, the startup menu lets you choose a mode or continue a saved run.

## Useful options

| Option | What it does |
| --- | --- |
| `--classic` | Start classic blackjack |
| `--roguelike` | Start a new roguelike run |
| `--continue` | Continue the saved run |
| `--seed N` | Start with a repeatable random seed |
| `--fast` | Skip dealer animation pauses |
| `--ascii` | Use ASCII-only graphics |
| `--no-color` | Disable colors |
| `--no-save` | Disable autosaving for this session |
| `--help` | Show all options |

## Controls

**During a hand:** `h` hit · `s` stand · `d` double · `p` split · `u` surrender · `?` help · `q` quit the run.

**Between rounds:** `c` continue · `a` advance floor · `s` shop · `w` save · `q` quit. Menus can also be navigated with the arrow keys and Enter.

## Tests

```sh
make test
make test-sanitize
make simulate-report
```

The test suite covers game rules, progression, saves, rendering, command-line options, and terminal behavior. The simulation report compares seeded gameplay strategies; it is an analysis tool, not a balance guarantee.

## Platform

Linux terminal (uses `termios` and ANSI escape sequences). A window of at least 70×42 characters is recommended. UTF-8 graphics are used by default; `--ascii` is available for restricted terminals.
