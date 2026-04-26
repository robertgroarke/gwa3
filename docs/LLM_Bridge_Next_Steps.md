# LLM Bridge Next Steps

This document captures the highest-value follow-up work after the Disco Panic launcher-based orchestrated bridge suite reached a full green pass.

Current baseline:
- Use `GWLauncher` for all Guild Wars launches.
- Use Disco Panic as the primary bridge validation account.
- Inject only the exact launcher-returned PID.
- Do not run bridge tests in parallel against a single client.
- Treat passes as evidence-based only: observable state changes, not just successful action return codes.

## Immediate Priorities

1. Add repeated full-suite soak coverage for Disco Panic.
2. Add negative-action tests for invalid inputs and rejected operations.
3. Add bridge quest/dialog coverage around Tekks and quest acceptance.
4. Expand combat validation from one combat-side signal to combat plus loot.

## Detailed Next Steps

### 1. Repetition and Soak Coverage

Goal:
- Move from “one clean orchestrated pass” to confidence across repeated fresh sessions.

Tasks:
- Add a runner that launches Disco Panic through `GWLauncher`, injects the returned PID, and runs the full orchestrated bridge suite multiple times.
- Track per-run outcome, runtime, skips, and watchdog/crash events.

Acceptance:
- No GW crash dialogs.
- No delayed bridge disconnects.
- No cross-run state contamination.
- Stable pass rate across multiple fresh launcher sessions.

### 2. Negative-Action Tests

Goal:
- Prove the bridge rejects bad inputs safely and predictably.

Tasks:
- Add tests for:
  - invalid `agent_id`
  - invalid `map_id`
  - invalid `hero_index`
  - out-of-range coordinates
  - using a skill on recharge
  - actions while map is not loaded

Acceptance:
- `action_result.success == false`
- correct bridge error code/string
- no client instability
- no silent hangs

### 3. Snapshot Consistency Validation

Goal:
- Ensure Tier 1, Tier 2, and Tier 3 snapshots remain internally consistent.

Tasks:
- Add tests that compare the same live state across tiers for:
  - `me.agent_id`
  - `map.map_id`
  - `me.target_id`
  - `party.size`
  - merchant open state
  - hero skillbar presence

Acceptance:
- Higher tiers may add fields, but must not contradict lower tiers.
- No stale impossible transitions.
- No missing fields where presence is promised by tier semantics.

### 4. Combat Depth Expansion

Goal:
- Increase confidence beyond a single combat-side signal.

Tasks:
- Extend Phase 4 to prove:
  - real skill cast evidence via recharge or cast-state
  - foe HP movement when available
  - target reacquisition if the first foe dies/disappears
  - loot pickup after a kill when drops are available

Acceptance:
- Evidence must come from snapshots.
- No pass on “action returned success” alone.
- Encounter-dependent gaps should `SKIP` with explicit reason.

### 5. Quest and Dialog Coverage

Goal:
- Validate the bridge on NPC dialog and quest-state transitions.

Tasks:
- Add a bridge flow for Tekks:
  - move to Tekks
  - open quest dialog
  - accept quest
  - verify quest-log state change

Acceptance:
- Quest-state transition observed in snapshot or quest-state read.
- No pass based only on dialog action ack.
- If quest already active, test should `SKIP` or use a deterministic precondition.

### 6. Inventory and Economy Coverage

Goal:
- Extend merchant/inventory confidence beyond the salvage-kit round trip.

Tasks:
- Add coverage for:
  - identify kit use
  - salvage flow
  - additional merchant transactions
  - optional storage interactions if exposed through bridge actions

Acceptance:
- Inventory model/quantity changes observed.
- Gold changes observed.
- No stale merchant-state false positives.

### 7. Advisory Mode Validation

Goal:
- Validate `--llm-advisory` recommendations against real game state.

Tasks:
- Run advisory mode in:
  - outpost state
  - merchant state
  - explorable with foes nearby

Acceptance:
- Advisory outputs are well-formed.
- Recommended actions are legal and context-appropriate.
- No impossible or stale recommendations.

### 8. Watchdog and Crash Regression Coverage

Goal:
- Preserve the bridge crash/debugging fixes we already made.

Tasks:
- Add explicit regression checks for:
  - ArenaNet crash dialog detection
  - bridge pipe disappearance
  - process-gone detection
  - main-window hang detection

Acceptance:
- Failures surface as deterministic test failures, not hangs.
- Crash dialog detection remains active in `--llm` and `--llm-advisory`.

### 9. Second Account Baseline

Goal:
- Ensure the bridge does not only work on Disco Panic’s exact build and account state.

Tasks:
- Add one additional launcher-based account after Disco Panic stays stable.
- Reuse the same orchestrated suite where possible.

Acceptance:
- Same suite passes, or skips only for explainable context differences.
- No hidden dependency on Disco Panic’s exact skillbar or party state.

### 10. Turn Learned Rules into Permanent Test Invariants

Goal:
- Prevent rediscovery of the same operational mistakes.

Tasks:
- Keep the following documented and enforced:
  - always launch with `GWLauncher`
  - always inject the exact launcher-returned PID
  - never run multiple bridge tests concurrently against one client
  - require evidence-based pass criteria

Acceptance:
- Rules are documented in bridge test docs and helper scripts.
- New tests follow the same invariants by default.

## Recommended Execution Order

1. Repeated Disco Panic soak runner
2. Negative-action tests
3. Tekks quest/dialog bridge coverage
4. Combat plus loot expansion
5. Advisory-mode validation
6. Second-account baseline

## Notes

- Disco Panic remains the primary bridge validation baseline.
- The orchestrated suite is currently green on a fresh launcher-based Disco Panic run:
  - Phase 1 outpost
  - Phase 2 party setup
  - Phase 3 merchant
  - Phase 4 explorable/combat
  - Phase 5 return
- Future work should preserve the operational safety rules above while increasing breadth and repeatability.
