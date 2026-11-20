# tty21 — Development Roadmap

This roadmap orders work by risk and player impact. It is the project-wide plan; visual polish and animation work are one part of it, not a separate top-level project.

## Guiding priorities

- The game has no meta-progression: nothing unlocks, upgrades, or carries over as a permanent player benefit between runs. Progression and power growth exist only inside the current run.

1. Correctness and run integrity before adding more complexity.
2. Tests and reproducibility before broad refactors.
3. Maintainable architecture before large content expansions.
4. Gameplay balance and within-run progression before presentation polish.
5. Responsive, coherent UX after the underlying flows are dependable.
6. Release quality is verified continuously, not postponed to the final day.

## Stage 0 — Establish the current baseline

- Work from the latest main branch.
- Build with the project's strict warning flags and run the complete test suite.
- Review the current implementation before treating historical audit findings as confirmed defects.
- Reproduce and record any issue before changing code.
- Keep each change focused and preserve the existing game rules unless a change is intentional.

**Exit criteria:** a known-good baseline, a short list of verified defects, and repeatable build/test results.

## Stage 1 — Correctness and run integrity

Prioritize bugs that can corrupt a run, produce an invalid outcome, lose progress, or charge the player incorrectly.

Audit and fix, where confirmed:

- Signal handling and terminal restoration: keep signal handlers async-signal-safe and perform cleanup in normal control flow.
- Save/load format and validation: reject incompatible or malformed data safely; validate relationships between hands, rounds, cards, and progression state.
- Shop edge cases: rerolls and purchases must never charge the player without delivering the selected result.
- Round-state invariants: active-hand indices, split/multiple-hand resolution, interrupted rounds, deck state, and settlement must remain consistent.
- Economy and settlement edge cases: insurance, surrender, even money, payout caps, loans, fees, and rewards must resolve exactly once.
- RNG behavior: retain seeded reproducibility and make random behavior testable.

Add a regression test for every confirmed defect.

**Exit criteria:** no known run-corrupting or economy/settlement correctness defects; invalid saves fail safely; terminal state is restored on supported exit paths.

## Stage 2 — Test coverage and verification

**Status:** complete. The regression suite covers the listed rule combinations, malformed and interrupted saves, constrained shop and purchase failures, and deterministic initialization. Unit and CLI smoke tests are separated; CI passes GCC, Clang, and AddressSanitizer/UndefinedBehaviorSanitizer jobs.

- Cover currently under-tested blackjack actions and combinations: Even Money, Surrender, Twin/multiple dealer hands, payout caps, Split/Double/Insurance interactions, and interrupted-round recovery.
- Test malformed, truncated, and incompatible save files.
- Test empty or constrained shop inventories and failed purchase paths.
- Expand CI/build verification where practical: GCC and Clang, sanitizers, and deterministic seeded test runs.
- Separate unit tests from terminal-level smoke tests so failures are easy to diagnose.

**Exit criteria:** important game rules and failure paths have regression coverage; CI catches common memory, undefined-behavior, and compiler issues.

## Stage 3 — Maintainability and architecture

**Status:** complete (18 Nov 2026). The architecture and dependency audit is complete, and the exit criteria have been verified. The renderer-free core target covers the important game-state, shop/economy, floor, and settlement paths; the full build and regression matrix passes under GCC and Clang; CLI smoke tests and AddressSanitizer/UndefinedBehaviorSanitizer pass. The final verified commit is `546d2a1c61c0db06fa437f9410f1a3e5c8442b3b`; [CI run](https://github.com/NotVeranto/tty21/actions/runs/44260694775). The consolidation audit found no further source-file merges that would reduce coupling: the remaining focused round and renderer modules have distinct responsibilities, while the presentation adapter preserves the game/UI boundary. Redundant terminal-size wrapper and duplicate startup dimension query have been removed; stale test guidance and an unused test include have been cleaned up. Stage 3 is closed; subsequent work should follow Stage 4 rather than continue structural refactoring.

Refactor only where it reduces real coupling or makes future work safer.

- Keep game rules and state changes separate from rendering and terminal effects. The dedicated core test target exercises initial state, shop-service availability and transactions, floor-menu transitions, advancement affordability, natural and regular wins, push, surrender, insurance, Twin/multiple-dealer and split-hand settlement, payout caps, and interrupted-round recovery without linking terminal rendering, input, menus, controller, or persistence.
- Keep the ordered shop-service offer catalog, offer visibility, purchase availability and transactions, floor-advance economy, and menu-driven state transitions in the game layer. The menu delegates mutations and formats read-only economy/rule values; the controller coordinates the state-driven loop, UI, input, and persistence. Preserve the rule that floor advancement performs no rendering, delays, or terminal input flushing.
- Reduce oversized modules (especially rendering) into cohesive responsibilities without changing visible behavior. Keep shared menu presentation, the player action bar, standalone run-summary screen, and help overlay in focused renderer modules, separate from table and screen composition.
- Centralize repeated terminal, input, and screen-layout behavior.
- Keep terminal-size validation and too-small-screen presentation in a dedicated renderer module, separate from game-table composition. Share one ANSI-capability decision across terminal styling, cursor control, and constrained-screen output.
- Keep ANSI control sequences and fixed screen-row coordinates in narrow headers so focused renderers do not depend on the aggregate renderer interface.
- Give focused renderer responsibilities dedicated public headers; reserve the aggregate renderer header for compatibility and application-level composition.
- Keep presentation-only values outside mutable game state; rendering must not temporarily rewrite gameplay fields. Keep settlement free of renderer calls, and let the controller present completed round results.
- Do not animate currency balances or currency deltas; update chip balances immediately and use concise textual feedback for transactions.
- Keep Oracle next-card and deck-composition intelligence separate from dealer-rule and floor-twist table information.
- Make ownership and lifetime of shared state explicit; avoid hidden file-scope state where it creates coupling. Table padding is owned by the table compositor and passed into message rendering per call rather than stored in the message renderer.
- Preserve a small, understandable interface between game events and presentation.

**Exit criteria:** the dedicated core test target covers the important game-state, economy, floor, and settlement paths without linking terminal/UI modules; presentation changes do not require edits throughout game logic; no behavior regressions.

## Stage 4 — Gameplay balance and within-run progression

**Progress note (18 Nov 2026):** the advance forecast now uses the next floor's baseline minimum bet and clears current-floor twists and temporary bet multipliers in its projection. This fixes a misleading forecast where Greed or another current-floor effect could inflate the next-floor fee even though that effect is reset on arrival. A regression test covers the mismatch. The new floor's independently selected twist can still alter its effective minimum bet, but Greed is skipped when the post-transition bankroll cannot cover the doubled requirement; a seeded regression test exercises that floor-entry boundary. The economy audit also corrected Greed's mark reward accounting so its earned-mark statistic stays synchronized with the run balance; a renderer-free settlement regression test covers it. The late-floor maximum-bet cap no longer disappears when the bankroll is below the cap threshold: the player cannot wager above the cap, and a cap below the minimum bet correctly ends the run. Boundary tests cover floors 8 and 12. The shop-loan eligibility check now includes its 50-chip proceeds when evaluating the next-floor minimum; a floor-11 boundary test covers a 60-chip bankroll against a 74-chip next-floor minimum. The Bankroll perk now pays its +5 floor-start income before a loan due on that floor is collected; a floor-8-to-9 regression test covers the exact 104 + 5 - 75 = 34 chip boundary. Compounding now applies its streak bonus once per round rather than once per winning player/dealer hand pairing; core regressions cover ordinary, split, and Twin settlements. Dealer's Favor now consumes its once-per-floor charge for the whole busted player hand, including Twin/multiple-dealer rounds; a regression test covers the previously partial push. Double Dip now grants its extra half-bet payout to doubled split hands as well as unsplit hands; a renderer-free settlement regression covers two doubled split wins. Insurance now halves every loss component of the first losing player hand in Twin/multiple-dealer settlement instead of only the first dealer pairing; regressions cover both ordinary losses and busts. The shop purchase API now also rejects perks that are not in the current offer pool, with a regression proving hidden perks cannot be bought by bypassing the menu. Survival now evaluates the complete round loss after Twin/multiple-dealer settlement rather than independently per dealer-hand component; a core regression covers a wipe split across two dealer hands. Additional regressions verify that Extra Life remains charged when Survival alone leaves enough chips to play, and that Extra Life tops up and consumes its charge only when Survival's remainder is below the floor minimum. The floor-advance API now also requires the between-floor menu state; a regression verifies a second call after a successful advance cannot charge chips, award another floor's marks, or increment progression. This closes an out-of-sequence progression/reward path found during the mark-economy audit. The advance forecast now includes Bankroll's +5 floor-start income when checking affordability and projecting post-fee/post-loan chips, matching the actual transition order; a floor-8-to-9 boundary regression covers a 220-chip bankroll, 100-chip fee, 75-chip loan repayment, and exact 50-chip remainder. The boss-difficulty audit also verified the preparation purchase contract: disabling a boss removes only its first rule, while the second rule remains active for all four bosses. A renderer-free regression now checks each pair. The prolonged-floor interaction audit verified that fatigue steps down at rounds 6/9/12/15, hostile extra twists escalate at rounds 6/11/16/19 and cap at four, and the late-floor enraged dealer begins on floor 10 from round 3. Renderer-free boundary regressions now lock down fatigue tiers, escalation thresholds, idempotence, uniqueness, and the extra-twist cap. The round-action audit also found and fixed a one-chip surrender edge case: integer truncation had made the loss zero. Surrender now rounds half-bet losses up to a whole chip, with renderer-free regressions for odd and even bets. The player-turn module now exposes its legal-action list and dispatcher as reusable entry points. The interface validates the player-turn state and rejects actions absent from the current legal-action list. Dispatch now mutates state and records events without rendering; the interactive loop owns redraws. `startRoundWithBet()` accepts a validated wager and explicit insurance/even-money decisions. `simulateRound()` drives the production action dispatcher, dealer turn, and settlement using a caller-supplied policy without terminal input or rendering. `simulateClassicRun()` now repeats production rounds under a caller-supplied bet policy and records round outcomes plus starting, final, and peak bankroll. A seeded regression verifies repeatable 25-round aggregates. `simulateRoguelikeRun()` now drives seeded roguelike rounds and delegates between-floor continue/advance/quit decisions to a policy, using the production floor-transition and boss-preparation APIs. Its aggregate includes floors reached, bosses defeated, and marks earned; a seeded regression checks repeatable progression across eight rounds. An optional boss-preparation policy now decides whether to pay marks to disable the first boss rule, allowing seeded comparisons of boss-prep spending; the default remains no purchase. A shop policy now runs between rounds and can purchase offered perks or services and reroll the perk pool through production purchase APIs. The built-in balanced policy ranks durable upgrades, converts surplus chips only when it can complete an offered perk purchase, and buys a safety net in a narrow low-bankroll window. Regressions cover a real purchase and the policy's priority/conversion decisions. A seeded regression now runs baseline and mark-funded boss-preparation policies over the same 16 seeds, using the balanced shop policy in both batches; it verifies both batches complete and that the paid policy reaches boss preparation. The batch API exposes aggregate outcomes for follow-up balance analysis; this regression deliberately does not assume that disabling a rule must improve results. `simulateRoguelikeBatch()` now runs fresh games over an explicit seed list and aggregates round outcomes, bankroll totals, floor depth range, boss victories, marks, and counts of why runs stopped (round limit, insufficient bankroll, policy stop, or game quit); the same seeds can be reused across policies for paired comparisons, with regressions covering deterministic aggregate results and stop-reason totals. Batch results now also report the final-bankroll range and counts of profitable and bankrupt runs, avoiding comparisons that hide a wide or risky outcome distribution behind the mean; seeded regressions verify these fields are reproducible. The paired shop-policy regression now runs baseline and balanced policies over the same 16 seeds for up to 32 rounds and validates bankroll ranges, profitable/bankrupt counts, and stop-reason totals in both batches. This is a reproducible comparison harness, not evidence that either policy is better. A separate `make simulate-report` executable now compares the two shop policies over 256 shared seeds and up to 100 rounds per run, printing bankroll spread, profit/bankruptcy rates, floor depth, boss victories, marks, and stop reasons. The report is a repeatable analysis sample rather than a CI test or a claim of statistical significance. The report's floor policy now explicitly enters the shop once per floor before advancing, and the balanced-policy adapter counts shop actions and perk purchases; the report rejects a run where the balanced policy makes no perk purchases. This corrects the earlier comparison, whose advance-only policy skipped every shop and therefore produced identical baseline and balanced results. The first 256-seed report now runs in CI under both GCC and Clang. Before the shop-policy correction, repeated chip-to-mark conversion collapsed the balanced sample to 0.48 average final chips and 59.4% bankrupt runs. Conversion now funds an offered perk only when the full required exchange still leaves a two-minimum-bet reserve. The corrected paired sample records 127 perk purchases; baseline ends at 22.46 average chips with 19.9% bankrupt runs and reaches 3.77 floors, while the balanced policy ends at 19.71 average chips with 19.1% bankrupt runs and reaches 3.53 floors. This is a much healthier shop strategy, but it has not demonstrated a clear overall advantage: further tuning must examine which perks are purchased, their cost and survival value, and the progression depth trade-off. The simulation runner now treats an explicit policy quit as a normal stop rather than a failed round, and the report strategy exits cleanly when it reaches an empty active hand. The latter is tracked as a quit outcome, not treated as evidence that the game state is valid. Stage 4 remains open pending perk/economy tuning and a review of the rare empty-hand state.

- Review the difficulty curve across floors, including minimum bets, dealer strength, boss frequency, and economic pressure.
- Evaluate perk strength, shop prices, services, twist tools, cursed cards, and rewards as one connected economy.
- Check that strong and weak strategies both have meaningful trade-offs.
- Use seeded runs and collected test scenarios to compare balance changes.
- Add new mechanics or content only when they improve variety without obscuring the core blackjack decisions.

**Exit criteria:** progression feels intentional from early floors through deep runs; economy, perks, twists, and bosses reinforce rather than undermine one another.

**Progress note (20 Nov 2026):** `make simulate-report` now evaluates each of the 13 perks independently against the no-purchase baseline over the same 256 deterministic seeds. Each strategy buys only its selected perk when it is both offered and affordable, and reports offer, affordability, and purchase counts, average marks held at offers, and average shortfall when unaffordable, alongside deltas in final chips, floors reached, bosses defeated, and bankruptcy rate. This makes sparse results and the marks bottleneck visible: a perk with few offers or affordable opportunities cannot support a strong balance conclusion. The report remains a policy-dependent sample rather than a standalone balance verdict. The report now also records marks held at the first shop visit on each floor after at least three rounds, exposing the progression point where available currency diverges from the perk price catalogue. Use this distribution before changing reward amounts or perk prices. The report first measured +15/+30/+45 starting grants without purchases, which isolated affordability but could not reveal gameplay impact. It now reruns those same grants over the same 256 seeds with the balanced shop-purchase policy, reporting bankroll, floor, boss, bankruptcy, and purchase deltas against the no-grant balanced baseline. These remain diagnostic scenarios, not production balance changes.\n\n## Stage 5 — UX/UI cohesion and interaction feedback

This is where the agreed animation and responsiveness work belongs. It follows correctness, testability, architecture, and balance; it is not a substitute for them.

### Shared presentation foundation
- Define semantic presentation events so game logic reports what happened and the renderer decides how to show it.
- Avoid scattered sleeps and duplicated animation code in gameplay logic.
- Respect --fast, non-TTY output, small terminals, and interruption/exit paths.
- Prevent overlapping effects, stale frames, flicker, and layout or border drift.

### Card and round presentation
- Sequential initial deal, hit-card reveal, split-hand dealing, and multi-hand/Twin dealer dealing.
- Distinct reveal of the dealer's hidden card and staged dealer draws.
- Clear active-hand focus and readable transitions between hands.

### Results and progression
- Distinct, appropriately timed feedback for win, blackjack, loss, bust, push, and surrender.
- Floor transition that introduces the new floor and its modifier.
- Boss introduction with the boss identity and a concise explanation of its rule/ability.

### Menus and interaction feedback
- Consistent transitions between all menu windows and overlays, including main menu, settings, pause, shop, item details, confirmations, results, and run end.
- Clear acknowledgement for selection, confirmation, cancellation, unavailable actions, and failed operations.
- Perk feedback for acquisition, activation, passive/automatic triggers, charge use, and disabling/loss.
- Shop feedback for selection, purchase confirmation, reroll replacement, service/tool use, and restrictions.
- Apply the same interaction language to other player-facing actions and rewards.

### Explicit presentation constraints
- **Do not animate currency balances or show animated currency deltas.** Currency feedback is intentionally excluded.
- Purchase and exchange interactions may still receive clear textual/visual confirmation; the balance itself should update normally.
- Keep routine feedback brief and reserve stronger effects for meaningful events.

**Exit criteria:** every important interaction is acknowledged; transitions feel consistent; animations remain optional, non-blocking, and do not compromise readability or terminal stability.

## Stage 6 — Release polish

- Verify clean builds with strict warnings and the full test suite.
- Run smoke tests for classic mode, roguelike progression, save/continue, shop, bosses, split/multiple hands, and run completion.
- Check supported terminal sizes, UTF-8 and ANSI behavior, --fast, redirected output, and exit/signal handling.
- Review README, changelog, usage text, and man page against actual behavior.
- Fix release-blocking issues before tagging a release.

**Exit criteria:** documented behavior matches the program; release checks pass; no known critical defects remain.

## Working order

Proceed in stage order. Within a stage, take the highest-risk verified issue first. Do not start the next stage merely because a checklist item was attempted: meet the exit criteria, run the relevant tests, and then move on. Reprioritize only when a newly discovered critical defect justifies it.
