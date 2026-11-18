# Changelog

All notable changes to tty21 are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed

- Calculate the next-floor advance forecast from the next floor's baseline economy instead of carrying over the current floor's twist and temporary minimum-bet multiplier.
- Include Greed twist marks in the run's total earned-marks statistic.
- Skip a newly selected Greed twist when the post-advance bankroll cannot cover its doubled minimum bet.
- Enforce late-floor maximum-bet caps even below the minimum-bet threshold, preventing over-cap wagers.
- Include the shop loan's 50-chip proceeds when checking whether the next-floor minimum bet is reachable.
- Apply Bankroll's +5 floor-start income before collecting a loan due on the newly reached floor.
- Award Compounding's streak bonus once per round, including split-hand and Twin/multiple-dealer settlements.
- Apply Dealer's Favor to the entire busted hand in Twin/multiple-dealer rounds.
- Apply Double Dip's extra half-bet payout to doubled split hands as well as unsplit hands.
- Apply Insurance's once-per-floor protection to the full loss of a Twin/multiple-dealer player hand.
- Reject purchases for perks that are not offered in the current shop pool.
- Apply Survival after aggregating all loss components in Twin/multiple-dealer rounds.
- Verify Survival and Extra Life recovery ordering and charge preservation with core regressions.
- Reject floor advancement outside the between-floor menu state, including repeated calls after a successful transition.

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
