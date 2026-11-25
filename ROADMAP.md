# tty21 — Development Roadmap

## Project status snapshot (25 Nov 2026)

These percentages are completion estimates against each stage's exit criteria, not code-volume metrics. Stages 0–4 are closed, Stage 5 is active, and Stage 6 has not started.

| Stage | Completion | Status |
|---|---:|---|
| 0 — Baseline | 100% | Closed |
| 1 — Correctness and run integrity | 100% | Closed |
| 2 — Test coverage and verification | 100% | Closed |
| 3 — Maintainability and architecture | 100% | Closed |
| 4 — Gameplay balance and within-run progression | 100% | Closed |
| 5 — Visual presentation and interaction feedback | 10% | Active |
| 6 — Release polish | 0% | Not started |

Overall roadmap estimate: **87%** (unweighted mean of the seven stage estimates).

Stage 4 exit criteria have been met: the empty-hand edge case is resolved in production round-start logic and covered for normal and Twin deals; the 2,048-seed progression and boss evidence has been reviewed; and the production economy decision is documented (retain current prices, starting marks, and purchase rules). The latest main-branch CI run passed GCC, Clang, and ASan/UBSan. Stage 4 is closed; do not reopen balance experiments without a reproducible player-facing defect or a materially new hypothesis.

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

**Progress note (18 Nov 2026):** the advance forecast now uses the next floor's baseline minimum bet and clears current-floor twists and temporary bet multipliers in its projection. This fixes a misleading forecast where Greed or another current-floor effect could inflate the next-floor fee even though that effect is reset on arrival. A regression test covers the mismatch. The new floor's independently selected twist can still alter its effective minimum bet, but Greed is skipped when the post-transition bankroll cannot cover the doubled requirement; a seeded regression test exercises that floor-entry boundary. The economy audit also corrected Greed's mark reward accounting so its earned-mark statistic stays synchronized with the run balance; a renderer-free settlement regression test covers it. The late-floor maximum-bet cap no longer disappears when the bankroll is below the cap threshold: the player cannot wager above the cap, and a cap below the minimum bet correctly ends the run. Boundary tests cover floors 8 and 12. The shop-loan eligibility check now includes its 50-chip proceeds when evaluating the next-floor minimum; a floor-11 boundary test covers a 60-chip bankroll against a 74-chip next-floor minimum. The Bankroll perk now pays its +5 floor-start income before a loan due on that floor is collected; a floor-8-to-9 regression test covers the exact 104 + 5 - 75 = 34 chip boundary. Compounding now applies its streak bonus once per round rather than once per winning player/dealer hand pairing; core regressions cover ordinary, split, and Twin settlements. Dealer's Favor now consumes its once-per-floor charge for the whole busted player hand, including Twin/multiple-dealer rounds; a regression test covers the previously partial push. Double Dip now grants its extra half-bet payout to doubled split hands as well as unsplit hands; a renderer-free settlement regression covers two doubled split wins. Insurance now halves every loss component of the first losing player hand in Twin/multiple-dealer settlement instead of only the first dealer pairing; regressions cover both ordinary losses and busts. The shop purchase API now also rejects perks that are not in the current offer pool, with a regression proving hidden perks cannot be bought by bypassing the menu. Survival now evaluates the complete round loss after Twin/multiple-dealer settlement rather than independently per dealer-hand component; a core regression covers a wipe split across two dealer hands. Additional regressions verify that Extra Life remains charged when Survival alone leaves enough chips to play, and that Extra Life tops up and consumes its charge only when Survival's remainder is below the floor minimum. The floor-advance API now also requires the between-floor menu state; a regression verifies a second call after a successful advance cannot charge chips, award another floor's marks, or increment progression. This closes an out-of-sequence progression/reward path found during the mark-economy audit. The advance forecast now includes Bankroll's +5 floor-start income when checking affordability and projecting post-fee/post-loan chips, matching the actual transition order; a floor-8-to-9 boundary regression covers a 220-chip bankroll, 100-chip fee, 75-chip loan repayment, and exact 50-chip remainder. The boss-difficulty audit also verified the preparation purchase contract: disabling a boss removes only its first rule, while the second rule remains active for all four bosses. A renderer-free regression now checks each pair. The prolonged-floor interaction audit verified that fatigue steps down at rounds 6/9/12/15, hostile extra twists escalate at rounds 6/11/16/19 and cap at four, and the late-floor enraged dealer begins on floor 10 from round 3. Renderer-free boundary regressions now lock down fatigue tiers, escalation thresholds, idempotence, uniqueness, and the extra-twist cap. The round-action audit also found and fixed a one-chip surrender edge case: integer truncation had made the loss zero. Surrender now rounds half-bet losses up to a whole chip, with renderer-free regressions for odd and even bets. The player-turn module now exposes its legal-action list and dispatcher as reusable entry points. The interface validates the player-turn state and rejects actions absent from the current legal-action list. Dispatch now mutates state and records events without rendering; the interactive loop owns redraws. `startRoundWithBet()` accepts a validated wager and explicit insurance/even-money decisions. `simulateRound()` drives the production action dispatcher, dealer turn, and settlement using a caller-supplied policy without terminal input or rendering. `simulateClassicRun()` now repeats production rounds under a caller-supplied bet policy and records round outcomes plus starting, final, and peak bankroll. A seeded regression verifies repeatable 25-round aggregates. `simulateRoguelikeRun()` now drives seeded roguelike rounds and delegates between-floor continue/advance/quit decisions to a policy, using the production floor-transition and boss-preparation APIs. Its aggregate includes floors reached, bosses defeated, and marks earned; a seeded regression checks repeatable progression across eight rounds. An optional boss-preparation policy now decides whether to pay marks to disable the first boss rule, allowing seeded comparisons of boss-prep spending; the default remains no purchase. A shop policy now runs between rounds and can purchase offered perks or services and reroll the perk pool through production purchase APIs. The built-in balanced policy ranks durable upgrades, converts surplus chips only when it can complete an offered perk purchase, and buys a safety net in a narrow low-bankroll window. Regressions cover a real purchase and the policy's priority/conversion decisions. A seeded regression now runs baseline and mark-funded boss-preparation policies over the same 16 seeds, using the balanced shop policy in both batches; it verifies both batches complete and that the paid policy reaches boss preparation. The batch API exposes aggregate outcomes for follow-up balance analysis; this regression deliberately does not assume that disabling a rule must improve results. `simulateRoguelikeBatch()` now runs fresh games over an explicit seed list and aggregates round outcomes, bankroll totals, floor depth range, boss victories, marks, and counts of why runs stopped (round limit, insufficient bankroll, policy stop, or game quit); the same seeds can be reused across policies for paired comparisons, with regressions covering deterministic aggregate results and stop-reason totals. Batch results now also report the final-bankroll range and counts of profitable and bankrupt runs, avoiding comparisons that hide a wide or risky outcome distribution behind the mean; seeded regressions verify these fields are reproducible. The paired shop-policy regression now runs baseline and balanced policies over the same 16 seeds for up to 32 rounds and validates bankroll ranges, profitable/bankrupt counts, and stop-reason totals in both batches. This is a reproducible comparison harness, not evidence that either policy is better. A separate `make simulate-report` executable now compares the two shop policies over 256 shared seeds and up to 100 rounds per run, printing bankroll spread, profit/bankruptcy rates, floor depth, boss victories, marks, and stop reasons. The report is a repeatable analysis sample rather than a CI test or a claim of statistical significance. The report's floor policy now explicitly enters the shop once per floor before advancing, and the balanced-policy adapter counts shop actions and perk purchases; the report rejects a run where the balanced policy makes no perk purchases. This corrects the earlier comparison, whose advance-only policy skipped every shop and therefore produced identical baseline and balanced results. The first 256-seed report now runs in CI under both GCC and Clang. Before the shop-policy correction, repeated chip-to-mark conversion collapsed the balanced sample to 0.48 average final chips and 59.4% bankrupt runs. Conversion now funds an offered perk only when the full required exchange still leaves a two-minimum-bet reserve. The corrected paired sample records 127 perk purchases; baseline ends at 22.46 average chips with 19.9% bankrupt runs and reaches 3.77 floors, while the balanced policy ends at 19.71 average chips with 19.1% bankrupt runs and reaches 3.53 floors. This is a much healthier shop strategy, but it has not demonstrated a clear overall advantage: further tuning must examine which perks are purchased, their cost and survival value, and the progression depth trade-off. The simulation runner now treats an explicit policy quit as a normal stop rather than a failed round. No-Reshuffle now ends the run before starting a deal when the remaining deck cannot supply the complete initial deal (four cards normally, six against Twin); the wager and round counter remain untouched. Regression tests cover both cases. This removes the prior empty-active-hand workaround from the report strategy: deck exhaustion is now handled by production round-start logic. Stage 4 is closed. The 2,048-seed paired report confirms that the current balanced shop policy does not outperform the no-purchase baseline on average chips or floor depth, while bankruptcy is marginally lower. Held-mark perk purchases show a small positive sample delta; chip conversion remains a progress trade-off, and the boss-rule split remains observational with too few non-Pit boss samples. Production economy values are therefore deliberately unchanged: no evidence justifies another price, starting-mark, or purchase-cap adjustment. No-Reshuffle exhaustion is handled before an incomplete deal, including Twin, with regression coverage. Do not reopen balance experiments without a reproducible player-facing defect or a materially new hypothesis.

- Review the difficulty curve across floors, including minimum bets, dealer strength, boss frequency, and economic pressure.
- Evaluate perk strength, shop prices, services, twist tools, cursed cards, and rewards as one connected economy.
- Check that strong and weak strategies both have meaningful trade-offs.
- Use seeded runs and collected test scenarios to compare balance changes.
- Add new mechanics or content only when they improve variety without obscuring the core blackjack decisions.

**Exit criteria:** progression feels intentional from early floors through deep runs; economy, perks, twists, and bosses reinforce rather than undermine one another.

**Progress note (20 Nov 2026):** `make simulate-report` now evaluates each of the 13 perks independently against the no-purchase baseline over the same 256 deterministic seeds. Each strategy buys only its selected perk when it is both offered and affordable, and reports offer, affordability, and purchase counts, average marks held at offers, and average shortfall when unaffordable, alongside deltas in final chips, floors reached, bosses defeated, and bankruptcy rate. This makes sparse results and the marks bottleneck visible: a perk with few offers or affordable opportunities cannot support a strong balance conclusion. The report remains a policy-dependent sample rather than a standalone balance verdict. The report now also records marks held at the first shop visit on each floor after at least three rounds, exposing the progression point where available currency diverges from the perk price catalogue. Use this distribution before changing reward amounts or perk prices. The report first measured +15/+30/+45 starting grants without purchases, which isolated affordability but could not reveal gameplay impact. It now reruns those same grants over the same 256 seeds with the balanced shop-purchase policy, reporting bankroll, floor, boss, bankruptcy, and purchase deltas against the no-grant balanced baseline. The first results show that grants trigger substantially more purchases while reducing average chips and floor depth; +45 improves the bankruptcy rate but still trails the no-grant balanced policy in progression. A matched one-per-floor purchase-cap experiment now separates the effect of early currency from the effect of buying multiple perks too aggressively. These remain diagnostic scenarios, not production balance changes.\n\n**Progress note (25 Nov 2026, boss outcome linkage):** The roguelike simulation now attributes a boss victory to the exact encounter and whether its first rule was disabled. The 2048-seed isolated boss-preparation report records the Pit Boss at 1,018/1,467 defeats (69.4%) overall, with 274/368 (74.5%) after rule disable and 744/1,099 (67.7%) with the rule active. Croupier Prime has only eight encounters (1 defeat); Accountant Supreme and Void Saint have no observations in this sample. These are observational policy groups, not randomized matched boss-only trials, so the difference must not be interpreted as a causal estimate or used alone to change preparation prices. The report now checks that per-boss outcome counts reconcile with preparation encounters. GCC, Clang, and ASan/UBSan passed. Stage 4 is now a bounded close-out task, not a reason to continue adding overlapping balance matrices.

## Stage 5 — Visual presentation and interaction feedback

**Status:** active. Work order: first establish consistent presentation conventions across existing screens; only after that foundation is stable, begin the visual redesign and animation work. The dealer reveal already uses a brief automatic transition with a consistent 300 ms pause; fast and non-TTY modes remain immediate. Current completion estimate: 12%.

### Consistency foundation (first)

**Progress note (25 Nov 2026):** Plain-output main menus and the shop now share one tested menu-entry formatter. Selection markers, spacing, hotkeys, and disabled labels no longer diverge between the two paths. Full-screen ANSI rendering continues to use the existing shared `renderMenuItemRow()` treatment.
- [x] Use one shared menu-entry treatment for selection and disabled entries across plain-output menus and the shop; retain the existing ANSI treatment for full-screen mode.
- Align shared screen headers, framing, spacing, status labels, and terminal capability behavior where current implementations genuinely diverge.
- Preserve each screen's information hierarchy; do not merge modules or add abstractions solely to reduce file count.
- Keep fallback output readable and equivalent in meaning, without requiring ANSI control sequences.

### Visual redesign (after the foundation)
- Establish a cohesive visual language for the table, cards, chips, shop, bosses, transitions, and result screens.
- Add purposeful animation for dealing, reveals, player actions, meaningful outcomes, purchases, bosses, and transitions.
- Use terminal color and rendering capabilities progressively, with safe fallbacks and no simulation-side timing effects.

- Make card dealing, player actions, dealer reveals, and round outcomes feel like one coherent sequence.
- Keep feedback concise and legible; avoid currency animations and redundant pauses.
- Review floor transitions, boss preparation, shop purchase/reroll feedback, and menu navigation for clear state changes and consequences.
- Preserve fast mode, non-TTY output, and deterministic simulation behavior; presentation timing must never affect game rules.
- Add focused tests for any state or formatting behavior changed as part of UX work.

**Exit criteria:** every major interaction has clear, consistent feedback; no input-mode-specific blocking surprises; terminal resizing and non-TTY/fast-mode behavior remain safe.

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
- Do not animate numeric currency balances or display animated currency deltas; keep financial information stable and immediately readable.
- Physical chip visuals, purchase confirmations, and other meaningful shop feedback may still use animation when it improves clarity.
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
**Progress note (20 Nov 2026):** Added same-seed no-conversion shop comparisons. One variant retains the balanced perk priority; the other prioritizes Survival, Insurance, and Bankroll. Both buy only with existing marks, allowing the report to isolate chip conversion from purchase priority before any production economy changes.

**Progress note (20 Nov 2026):** Expanded the seeded shop report to 2048 shared runs and added no-conversion purchase breakdowns by floor, marks spent per floor, and perk. This makes the observed outcome deltas interpretable against the actual purchase mix and floor timing. The report remains a deterministic policy comparison, not a causal estimate of each purchase's effect; production perk prices and rewards remain unchanged pending review of the larger sample.


**Progress note (22 Nov 2026):** Added a paired conversion-reserve experiment to `make simulate-report`: the same 2048 seeds and balanced perk priority are evaluated with one-, two-, and three-minimum-bet reserves after chip-to-mark exchanges. It records exchange volume and compares bankroll, floor depth, boss wins, and bankruptcy against the no-purchase baseline. The two-bet variant is cross-checked against the built-in production balanced policy to detect divergence in the experiment adapter. This isolates the reserve threshold without changing the actual 10-chip/2-mark exchange rate or production balance values.

**Experiment result (22 Nov 2026):** CI completed the 2048-seed reserve comparison. Relative to no shop purchases, the one-, two-, and three-minimum-bet reserves changed final chips by -2.29/-1.96/-1.53 per run and floors reached by -0.35/-0.26/-0.20; boss victories changed by -0.10/-0.07/-0.06 and bankruptcy by +0.1/-0.2/-0.2 percentage points. The two-bet adapter matched the built-in balanced policy exactly (+0.00 chips and floors), validating the comparison. The no-conversion, held-marks-only policies remain slightly positive in the same report. Current evidence points to the 10-chip/2-mark exchange opportunity cost rather than a perk-priority issue. Do not increase marks or change perk prices to compensate; keep production values unchanged until a deliberate economy decision is made.


**Progress note (22 Nov 2026, exchange-price follow-up):** Added report-only same-seed comparisons for 5, 10, and 15 chips per 2 marks. Against the no-purchase baseline, final chips changed by -9.22/-1.92/-0.77 per run, floors by -1.36/-0.26/-0.15, bosses by -0.37/-0.07/-0.04, and bankruptcy by -0.2/-0.2/-1.0 percentage points. The 5-chip exchange drove 1,766 purchases and the 15-chip exchange 1,046; both still underperformed the held-marks-only policy. The current 10-chip policy in this adapter was within +0.03 chips and 0.00 floors of the built-in balanced policy. Exchange-volume diagnostics were corrected to count the single transaction the runner actually executes per callback. These price variants are report-only and do not change the shop service. Evidence now favors limiting or avoiding chip conversion rather than granting more marks or making perks cheaper.


**Progress note (22 Nov 2026, exchange-price/priority isolation):** Extended the report-only exchange experiment into a 3×2 paired comparison: 5/10/15 chips per 2 marks crossed with balanced/defensive perk priorities, all over the same 2048 seeds and two-minimum-bet reserve. The report now distinguishes offers already affordable with held marks from purchases funded by chip conversion, so these counts are not incorrectly interpreted as mutually equivalent. It also reports conversion volume separately. This is diagnostic only; no production economy values are changed.


**Experiment result (22 Nov 2026, matched exchange controls):** The 3×2 exchange-price/priority matrix now reports both deltas versus the no-purchase baseline and deltas versus the matching held-marks-only policy. Relative to held-marks-only, balanced/defensive exchange variants changed final chips by -9.92/-9.91 at 5 chips, -2.62/-2.49 at 10 chips, and -1.47/-1.48 at 15 chips per 2 marks; floor depth changed by -1.38/-1.36, -0.28/-0.25, and -0.17/-0.15. Defensive priority shifts outcomes only modestly; the conversion opportunity cost dominates. Even the most expensive tested exchange still underperforms held-marks-only for bankroll, floors, and bosses. Keep production values unchanged until the explicit design decision; evidence supports avoiding chip conversion specifically to fund the tested perk-buying policies, rather than tuning a favorable exchange price within this tested range. This is not evidence against conversion as a general marks source: these comparisons do not model marks spent on shop rerolls, twist rerolls/blocks, or boss-rule preparation, and their report policies do not exercise those services. Keep any broader economy conclusion open until those uses are included in paired seeded strategies.


**Report accounting correction (22 Nov 2026):** The reserve experiment now carries a pending-conversion marker across the shop service callback and the subsequent perk purchase. The production two-minimum-bet reserve records 500 held-mark-funded purchases and 609 conversion-funded purchases from 2,750 exchanges; one- and three-reserve variants record 724 and 489 conversion-funded purchases. The report asserts that total purchases equal the sum of the two funding sources. This corrects diagnostics only; no gameplay balance was changed.


**Progress note (25 Nov 2026, marks utility coverage):** Extended the seeded report with a service-aware policy that spends held marks on affordable perks, rerolls an unaffordable shop pool when eligible, responds to hostile twists with the real reroll/block services, and pays for boss-rule disable during boss preparation. The report records each service count and reconciles exact marks spent. This is a policy-level comparison against the no-purchase baseline; it does not isolate the causal value of each service, and production prices remain unchanged pending review of the paired results.


**Experiment result (25 Nov 2026, marks utility coverage):** The 2048-seed service-aware policy changed average final chips by +0.57 and floors by +0.02 versus the no-purchase baseline; boss victories were +0.00/run and bankruptcy changed by -0.0 percentage points. It made 337 perk purchases, 4,331 shop-pool rerolls, and 3 twist rerolls; it blocked no twists. Boss preparation offered 1,469 encounters, but only 5 rule disables were affordable (250 marks spent). The service counter reconciled 43,400 marks: 43,310 for shop rerolls and 90 for twist rerolls. This is a combined policy result, not a causal estimate: rerolls alter RNG consumption, and the experiment does not isolate individual service value. The low boss-disable count also shows that the current policy rarely reaches the floor-scaled preparation price. CI passed under GCC, Clang, and ASan/UBSan.


**Progress note (25 Nov 2026, isolated service experiments):** Reworked the marks-utility report after the combined policy exposed a repeated shop-reroll loop (4,331 actions across 2,048 runs). The new report runs separate same-seed policies for shop-pool rerolls, hostile-twist tools, held-mark perk purchases, and boss-rule preparation. Each shop-service policy is capped at one service action per floor/shop visit, and boss preparation is enabled only in its own isolated policy. This removes the prior confounding and prevents the report from treating repeated rerolls as normal purchasing behavior. Awaiting CI results before drawing balance conclusions.


**Experiment result (25 Nov 2026, isolated service strategies):** The corrected 2048-seed report separates each use of marks from the others. Relative to the no-purchase baseline: shop-pool rerolls changed chips by +0.27, floors +0.02, bosses +0.01, and bankruptcy +1.0 pp (4,400 rerolls; 44,000 marks); hostile-twist tools changed chips by +0.10, floors +0.00, bosses -0.00, and bankruptcy -0.0 pp (17 rerolls and 166 blocks; 7,150 marks); held-mark perk purchases changed chips by +0.70, floors +0.02, bosses -0.00, and bankruptcy -0.8 pp (989 purchases); boss-rule preparation changed chips by +0.40, floors +0.02, bosses +0.01, and bankruptcy -2.2 pp (375 disables from 1,475 eligible encounters; 19,100 marks). These are policy comparisons, not causal estimates: the shop reroll policy spends once per floor, twist outcomes consume RNG differently, and each strategy changes later offers/state. The boss-prep signal is promising but requires a targeted follow-up with matched boss-only outcomes and cost/availability breakdowns before any production price change. GCC, Clang, and ASan/UBSan all passed.
