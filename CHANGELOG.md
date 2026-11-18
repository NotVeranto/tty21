# Changelog

All notable changes to tty21 are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed

- Calculate the next-floor advance forecast from the next floor's baseline economy instead of carrying over the current floor's twist and temporary minimum-bet multiplier.
- Include Greed twist marks in the run's total earned-marks statistic.
- Skip a newly selected Greed twist when the post-advance bankroll cannot cover its doubled minimum bet.

## [0.1.0] - 2026-11-04

### Added

- Modularized the round implementation into focused source files for cards, economy, rules, input, actions, dealer turns, and settlement.
- Added a Boss Preparation screen before boss encounters, including the option to pay marks to disable a boss rule.
- Added the Surrender action for eligible hands, settling at half the original bet.
- Added roguelike save/load support, including autosaving, manual saves, validation, and atomic replacement through a temporary file and rename.
- Added card-deal and chip-change animations, dealer turn step-through, and terminal UI styling and feedback.
- Added floor progression, boss encounters, a rotating perk shop, services, twist tools, floor twists, cursed cards, and expanded dealer abilities.

### Fixed

- Closed a betting escrow exploit involving funds committed to active hands.
- Prevented deck reshuffles during an active round from reintroducing cards already dealt, avoiding duplicate cards.
- Corrected several round-resolution paths, including multi-hand dealer resolution and interrupted-round handling.
- Capped round payouts and corrected insurance availability when dealer blackjack is impossible.
