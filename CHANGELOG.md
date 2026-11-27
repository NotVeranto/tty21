# Changelog

All notable changes to tty21 are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Tests

- Add pseudo-terminal smoke coverage for ASCII-only rendering, monochrome output, cursor visibility controls, and terminal mode restoration.

### Fixed

- Reject simultaneous `--roguelike` and `--classic` flags instead of silently letting argument order choose the mode.

### Changed

- Replace the ambiguous startup-menu hint "hotkeys jump" with the explicit "1/2/Q quick select" wording.

### Tested

- Cover cursor-addressability detection for supported, redirected, dumb, and unspecified terminal types.

### Changed

- Keep the command-line reference, README, and man page aligned with the animated startup flow and supported launch flags; document compatible and conflicting option combinations.
- Expand CLI smoke coverage for seed syntax, missing arguments, and incompatible mode/continue flags.

## [0.1.2] - 2026-11-27

### Added

- Add `--ascii` and `--no-color` launch flags for restricted terminals, reusing the existing ASCII glyph and monochrome renderer support.

### Changed

- Bump the application version to 0.1.2 across build metadata and user-facing documentation; the startup-menu version and `--version` share the same build-time value.

## [0.1.1] - 2026-11-27

### Changed

- Refresh release metadata and user-facing launch documentation after the presentation update.
- Describe the animated startup menu, current terminal presentation, and session-level behavior of `--fast` and `--no-save`.

### Verified

- Closed the gameplay-balance stage after reviewing the 2,048-seed progression report, retaining production economy values, and fixing No-Reshuffle incomplete deals (including Twin). Main-branch CI passed GCC, Clang, and ASan/UBSan.

### Changed

- Make dealer card reveals advance automatically with a short, consistent pause in raw and line-oriented terminal modes instead of blocking on a key between cards.


### Fixed

- End No-Reshuffle runs before a deal when the remaining deck cannot supply every initial player and dealer card, including Twin's extra dealer hand; no wager or round is started.

### Changed

- Publish stage-by-stage completion estimates and bound Stage 4 to a final empty-hand review and documented balance decision instead of open-ended simulation experiments.

### Fixed

- Track roguelike boss victories against the exact encounter and preparation choice; report rule-disabled and rule-active defeat rates separately and reject inconsistent per-boss counts.

### Changed

- Record the isolated 2048-seed service-policy results and their limitations in the balance roadmap.
- Split the marks-utility simulation into isolated same-seed strategies and cap each shop-service strategy at one action per shop visit, preventing repeated shop-reroll loops from distorting the comparison.

### Added

- Record the 2048-seed marks-utility result and clarify that its combined policy outcome cannot isolate the effect of individual services.
- Extend the seeded economy report to exercise shop-pool rerolls, hostile-twist rerolls/blocks, and paid boss-rule preparation through the production service and boss-prep APIs; report service usage and reconcile marks spent.

- Clarify the scope of the 2048-seed conversion experiments: they compare perk-buying policies and do not evaluate shop/twist services or paid boss preparation; their results must not be generalized to all marks spending.

### Fixed

- Correct reserve-experiment funding diagnostics across the exchange and subsequent perk-purchase callbacks; assert that held-mark-funded plus conversion-funded purchases reconcile with total purchases.

- Preserve a two-minimum-bet chip reserve when the balanced shop policy converts chips into marks to fund an offered perk; do not spend the bankroll on an upgrade that cannot be completed safely.
- Treat an explicit policy quit during a simulated round as a normal run stop without counting an unfinished round as settled.

- Keep converting chips into marks until an offered odd-cost perk is affordable; the balanced shop policy no longer stalls one mark short.

### Added

- Cross exchange prices of 5, 10, and 15 chips per 2 marks with balanced and defensive perk priorities over the same 2048 seeds; compare each against its matching held-marks-only control, distinguish purchase funding sources, and retain production shop behavior unchanged.

- Compare report-only chip-to-mark exchange prices of 5, 10, and 15 chips per 2 marks over the shared 2048 seeds; accurately count each executed exchange transaction and retain production shop behavior unchanged.

- Add a paired 2048-seed chip-conversion reserve experiment for one-, two-, and three-minimum-bet bankroll reserves, recording conversion volume and checking that the two-bet variant matches the built-in balanced policy; production exchange values remain unchanged.

- Expand the seeded shop strategy report from 256 to 2048 shared runs and report no-conversion purchases by floor, marks spent per floor, and perk; this improves exposure and purchase-mix diagnostics without changing production balance.
- Compare one-time starting-mark grants of 15, 30, and 45 marks over the same 256 seeds while using the balanced shop-purchase policy; report purchase counts and bankroll, floor, boss, and bankruptcy deltas against the no-grant balanced baseline without altering production rewards. Add a matched one-per-floor purchase-cap comparison for all four grant levels to test whether over-purchasing explains the weaker progression.
- Run the 2048-seed paired shop-policy report in CI under both GCC and Clang.
- Report per-perk offer and affordability counts, average marks at offer, and average unaffordable shortfall alongside purchases and same-seed outcome deltas, making the marks bottleneck measurable.
- Report baseline marks held at the first shop visit on each floor after at least three rounds, making floor-by-floor progression income visible before economy tuning.
- Compare baseline and mark-funded boss-preparation policies over the same deterministic seed set in the roguelike batch regression suite.
- Add a `make simulate-report` tool that compares baseline and balanced shop policies across 2048 shared seeds, visits the shop on every floor, reports actual shop and perk-purchase counts, and summarizes win/loss/push distribution, bankroll, progression, and stop reasons.
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
- Compare balanced and defensive shop priorities without chip-to-mark conversion across the shared seeded simulation sample.
