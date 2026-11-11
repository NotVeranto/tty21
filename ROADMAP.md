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

- Cover currently under-tested blackjack actions and combinations: Even Money, Surrender, Twin/multiple dealer hands, payout caps, Split/Double/Insurance interactions, and interrupted-round recovery.
- Test malformed, truncated, and incompatible save files.
- Test empty or constrained shop inventories and failed purchase paths.
- Expand CI/build verification where practical: GCC and Clang, sanitizers, and deterministic seeded test runs.
- Separate unit tests from terminal-level smoke tests so failures are easy to diagnose.

**Exit criteria:** important game rules and failure paths have regression coverage; CI catches common memory, undefined-behavior, and compiler issues.

## Stage 3 — Maintainability and architecture

Refactor only where it reduces real coupling or makes future work safer.

- Keep game rules and state changes separate from rendering and terminal effects.
- Reduce oversized modules (especially rendering) into cohesive responsibilities without changing visible behavior.
- Centralize repeated terminal, input, and screen-layout behavior.
- Make ownership and lifetime of shared state explicit; avoid hidden file-scope state where it creates coupling.
- Preserve a small, understandable interface between game events and presentation.

**Exit criteria:** core rules can be tested without depending on terminal rendering; presentation changes do not require edits throughout game logic; no behavior regressions.

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
