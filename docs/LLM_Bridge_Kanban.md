# LLM Bridge Kanban Board

This board tracks the next implementation wave for the launcher-based LLM bridge suite.

Source plan:
- [LLM_Bridge_Next_Steps.md](C:\Users\Robert\Documents\GWA Censured X BotsHub\LLM_Bridge_Next_Steps.md)

Core invariants:
- Always launch with `GWLauncher`.
- Always inject the exact launcher-returned PID.
- Never run multiple bridge tests concurrently against one client.
- Require evidence-based pass criteria from observable state changes, not action acknowledgements alone.

## Now

### LLM-001 Repeated Disco Panic Soak Runner
- Priority: `P1`
- Labels: `llm-bridge`, `tests`, `soak`, `launcher`, `stability`
- Summary: Add a repeated-run harness that launches Disco Panic through `GWLauncher`, injects the returned PID, and executes the full orchestrated suite across fresh sessions.
- Scope:
  - Launch fresh Disco Panic sessions via `GWLauncher`
  - Inject exact launcher-returned PID only
  - Run the full orchestrated suite serially
  - Record per-run pass/fail/skip, runtime, watchdog events, and crash events
  - Emit an aggregate run summary
- Definition of done:
  - Multiple fresh-session runs complete without manual intervention
  - No crash dialogs are observed
  - No delayed bridge disconnects occur
  - No cross-run contamination is detected
  - Summary output is useful for regression tracking
- Depends on:
  - Existing green orchestrated suite

## Ready

### LLM-002 Negative-Action Safety Tests
- Priority: `P1`
- Labels: `llm-bridge`, `tests`, `negative-path`, `safety`
- Summary: Add deterministic rejection-path tests for invalid inputs and illegal actions.
- Scope:
  - Invalid `agent_id`
  - Invalid `map_id`
  - Invalid `hero_index`
  - Out-of-range coordinates
  - Skill use while on recharge
  - Action while map is not loaded
- Definition of done:
  - `action_result.success == false` for all invalid cases
  - Expected bridge error code or string is asserted
  - No hangs occur
  - No client instability is introduced

### LLM-003 Tekks Quest and Dialog Coverage
- Priority: `P1`
- Labels: `llm-bridge`, `tests`, `quest`, `dialog`, `npc`
- Summary: Add bridge validation for NPC dialog and quest-state transitions using Tekks.
- Scope:
  - Move to Tekks
  - Open quest dialog
  - Accept quest
  - Verify quest-log or quest-state change
  - Add deterministic skip/precondition handling if quest is already active
- Definition of done:
  - Test proves the state transition, not just action acknowledgement
  - Already-active quest state is handled deterministically
  - Failures are specific and reproducible

### LLM-004 Combat Plus Loot Expansion
- Priority: `P1`
- Labels: `llm-bridge`, `tests`, `combat`, `loot`, `evidence`
- Summary: Expand combat validation so it proves real cast, damage, target continuity, and loot outcomes.
- Scope:
  - Confirm cast evidence via recharge or cast-state
  - Confirm foe HP movement when available
  - Reacquire target if first foe dies or disappears
  - Validate loot pickup when drops exist
  - Use `SKIP` with explicit reasons for encounter-dependent gaps
- Definition of done:
  - No pass is based only on action success
  - Combat outcomes are snapshot-backed
  - Loot outcomes are state-backed
  - Encounter variability is handled explicitly

## Backlog

### LLM-005 Snapshot Tier Consistency Validation
- Priority: `P2`
- Labels: `llm-bridge`, `snapshots`, `consistency`, `tests`
- Summary: Verify that Tier 1, Tier 2, and Tier 3 snapshots do not contradict each other.
- Scope:
  - Compare `me.agent_id`
  - Compare `map.map_id`
  - Compare `me.target_id`
  - Compare `party.size`
  - Compare merchant open state
  - Compare hero skillbar presence
- Definition of done:
  - Shared fields remain consistent across tiers
  - Impossible stale transitions are flagged
  - Missing promised fields are caught

### LLM-006 Inventory and Economy Coverage
- Priority: `P2`
- Labels: `llm-bridge`, `inventory`, `merchant`, `economy`, `tests`
- Summary: Expand inventory and merchant testing beyond the salvage-kit round trip.
- Scope:
  - Identify kit use
  - Salvage flow
  - Additional merchant transactions
  - Optional storage interactions if supported
  - Gold and inventory delta assertions
- Definition of done:
  - Inventory changes are observed
  - Gold changes are observed
  - No stale merchant-state false positives occur
  - No ack-only passes are allowed

### LLM-007 Advisory Mode Validation
- Priority: `P2`
- Labels: `llm-bridge`, `advisory`, `llm`, `tests`
- Summary: Validate `--llm-advisory` recommendations against live state in representative contexts.
- Scope:
  - Outpost validation
  - Merchant validation
  - Explorable-with-foes validation
  - Response shape validation
  - Stale and impossible recommendation checks
- Definition of done:
  - Outputs are well-formed
  - Recommendations are legal and context-appropriate
  - No impossible or stale actions are suggested

### LLM-008 Watchdog and Crash Regression Coverage
- Priority: `P2`
- Labels: `llm-bridge`, `watchdog`, `crash`, `regression`, `tests`
- Summary: Lock in existing watchdog and crash-detection fixes with deterministic regression tests.
- Scope:
  - ArenaNet crash dialog detection
  - Bridge pipe disappearance
  - Process-gone detection
  - Main-window hang detection
  - Coverage in both `--llm` and `--llm-advisory`
- Definition of done:
  - Covered failure modes fail deterministically
  - No covered path hangs indefinitely
  - Crash detection remains active in both modes

### LLM-009 Second Account Baseline
- Priority: `P3`
- Labels: `llm-bridge`, `multi-account`, `baseline`, `compatibility`
- Summary: Run the same launcher-based suite against a second account to expose Disco Panic-specific assumptions.
- Scope:
  - Select a second launcher-based account
  - Reuse the orchestrated suite
  - Explain legitimate skips from context differences
  - Identify hidden Disco Panic assumptions
- Definition of done:
  - Same suite runs on a second account
  - Differences are explainable
  - No hidden account-specific dependency remains unacknowledged

### LLM-010 Permanent Invariants Enforcement
- Priority: `P3`
- Labels: `llm-bridge`, `docs`, `harness`, `invariants`
- Summary: Turn operational rules into permanent documentation and harness defaults.
- Scope:
  - Document `GWLauncher` requirement
  - Document exact returned PID injection rule
  - Prevent concurrent bridge use against one client
  - Encode evidence-based pass expectations in helpers and docs
  - Audit compliance in new tests
- Definition of done:
  - Rules are documented
  - Harness and helpers enforce what they can
  - New tests follow invariants by default

## Milestones

### Milestone 1: Stability Foundation
- `LLM-001` Repeated Disco Panic Soak Runner
- `LLM-002` Negative-Action Safety Tests

### Milestone 2: Core Gameplay Evidence
- `LLM-003` Tekks Quest and Dialog Coverage
- `LLM-004` Combat Plus Loot Expansion
- `LLM-005` Snapshot Tier Consistency Validation

### Milestone 3: Behavioral Breadth
- `LLM-006` Inventory and Economy Coverage
- `LLM-007` Advisory Mode Validation
- `LLM-008` Watchdog and Crash Regression Coverage

### Milestone 4: Generalization and Guardrails
- `LLM-009` Second Account Baseline
- `LLM-010` Permanent Invariants Enforcement
