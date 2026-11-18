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

**Status:** in progress. Core renderer extraction, narrow public interfaces, settlement/presentation separation, and a renderer-free core test target are in place. `make test-core` compiles the game-state, shop/economy, floor, and settlement modules without linking terminal rendering, input, menus, controller, or persistence. Table layout owns message alignment and passes it explicitly to a stateless message renderer. Round handlers use a dedicated presentation coordinator. Core-level regression coverage now exercises shop-service availability and transaction success/failure paths, floor-menu transitions and affordability, floor advancement, natural-blackjack, regular-win, push, surrender, insurance, Twin/multiple-dealer, split-hand, payout-cap settlement, boss-preparation payment and affordability, and interrupted-round recovery without UI dependencies. `game.c` no longer imports round-flow, round-action, dealer-turn, or settlement interfaces. The `AdvanceForecast` data contract now lives in `round_economy.h`, which depends on game state rather than the round-flow header; `game.c`, `menu.c`, and the core test no longer import `round.h` for economy declarations. Shared presentation-state audit: `render.c` is the sole owner of `tablePadding` and resets it on screen reset; `render_messages.c` is stateless; `render_style.c` owns process-scoped ANSI initialization and cursor visibility. Boundary review confirms `game.c` owns state/rules/economy operations, including boss-preparation payment and its transition back to betting; `controller.c` owns the state-driven loop and round orchestration while collecting the boss-preparation choice, and `menu.c` owns menu input/presentation/persistence while delegating purchases, menu state transitions, and unaffordable-run termination to game APIs. Its remaining economy/rules calls are read-only values used to label and disable menu entries. No further boundary extraction is justified without introducing a redundant wrapper. Remaining work is final regression/CI verification and an exit-criteria review.

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

- Review the difficulty curve across floors, including minimum bets, dealer strength, boss frequency, and economic pressure.
- Evaluate perk strength, shop prices, services, twist tools, cursed cards, and rewards as one connected economy.
- Check that strong and weak strategies both have meaningful trade-offs.
- Use seeded runs and collected test scenarios to compare balance changes.
- Add new mechanics or content only when they improve variety without obscuring the core blackjack decisions.

**Exit criteria:** progression feels intentional from early floors through deep runs; economy, perks, twists, and bosses reinforce rather than undermine one another.

## Stage 5 — UX/UI cohesion and interaction feedback

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
