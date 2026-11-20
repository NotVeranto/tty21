# Changelog

All notable changes to tty21 are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed

- Keep converting chips into marks until an offered odd-cost perk is affordable; the balanced shop policy no longer stalls one mark short.

### Added

- Compare baseline and mark-funded boss-preparation policies over the same deterministic seed set in the roguelike batch regression suite.
- Add a `make simulate-report` tool that compares baseline and balanced shop policies across 256 shared seeds, visits the shop on every floor, reports actual shop and perk-purchase counts, and summarizes win/loss/push distribution, bankroll, progression, and stop reasons.
- Expand the paired shop-policy simulation regression to 16 shared seeds and 32 rounds per run, checking bankroll spread and profit/bankruptcy classification for both strategies without assuming a winner.
- Report minimum/maximum final bankroll and profitable/bankrupt run counts in roguelike batch statistics for more informative paired balance comparisons.

### Added

- Expose the production player-action availability and dispatch functions for deterministic policy-driven tests without synthesizing terminal key input.

### Fixed

- Reject player-action requests outside the player-turn state or outside the currently available action list, preventing callers from bypassing action eligibility.
- Keep player-action dispatch free of table-rendering calls; the interactive loop redraws after dispatch, while non-interactive drivers can reuse the same state transitions.
- Expose deterministic round startup, policy-driven headless round and Classic-session runners, and a roguelike runner with floor, boss-preparation, and shop policies plus progression statistics.
- Add a built-in balanced shop strategy that prioritizes durable perks and funds an offered upgrade through surplus-chip conversion when appropriate.
- Add a seeded roguelike batch runner that aggregates run outcomes and progression metrics across a reusable seed list for strategy comparisons.
- Classify simulation run stop reasons and aggregate their counts across batches, distinguishing round limits, insufficient bankroll, policy stops, and explicit game quits.

### Fixed

- Stop roguelike simulations cleanly when a floor's maximum-bet cap is below its minimum wager, classifying the run as insufficient bankroll.
- Allow a full player hand to stand below Sticky Hands and Butcher thresholds, preventing an unplayable hand from becoming stuck when no further hit is legal.
- Round odd-chip surrender losses up to the next whole chip instead of allowing a one-chip bet to surrender for free.

- Include Bankroll's +5 floor-start income in advance affordability forecasts and projected post-loan chips.
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
