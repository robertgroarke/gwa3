# LLM Bridge Test Plan

> Execution plan for validating the `gwa3` named-pipe LLM bridge using the same path Gemma uses.
> Preferred validation account: `D I S C O P A N I C` because it uses the standard hero setup.

## Account Baseline

Primary populated account source today:
- [GWA Censured/Accounts.json](/c:/Users/Robert/Documents/GWA%20Censured%20X%20BotsHub/GWA%20Censured/Accounts.json)

Recommended validation account:
- character: `D I S C O P A N I C`
- title/name: `D I S C O`
- gwpath: `C:\Program Files (x86)\Guild Wars - Copy (2)\Gw.exe`

Important note:
- repo-root [Accounts.json](/c:/Users/Robert/Documents/GWA%20Censured%20X%20BotsHub/Accounts.json) is currently empty
- before automating this plan fully, standardize which `Accounts.json` the runner consumes

## Goals

1. Prove the bridge transport is stable.
2. Prove snapshots are trustworthy enough to use as test evidence.
3. Prove bridge actions only pass when the expected state change is actually observed.
4. Prove Disco Panic can complete a standard outpost -> merchant -> explorable -> combat -> loot -> outpost bridge smoke.
5. Prove advisory mode remains safe and coherent.

## Test Philosophy

Bridge tests must be evidence-based.

A test should only pass if:
- the action succeeded
- and the expected state change is visible in later snapshots or event output

A test should skip, not fail, when:
- the required encounter state is genuinely absent
- the character is in the wrong map/state for that test tier

A test should fail when:
- the required observable change does not happen
- the bridge claims success but snapshots contradict that claim
- the bridge produces malformed, stale, or implausible state

## Recommended Character Setup

Use Disco Panic as the bridge validation character because it has a standard hero setup. The intended steady-state baseline is:
- logged in successfully
- standing in Gadd's Encampment
- standard hero party available
- enough gold to buy and sell a salvage kit
- inventory with at least one free slot

## Execution Tiers

### Tier 0: Bootstrap

Purpose:
- confirm the account and runner environment are sane before testing bridge behavior

Steps:
1. Launch Disco Panic.
2. Inject with `injector.exe --llm`.
3. Start the Python bridge.
4. Confirm the bridge receives snapshots.

Pass criteria:
- named pipe connects
- at least one valid snapshot arrives
- tick/heartbeat advance
- the character has a valid agent id in a loaded map

### Tier 1: IPC and Protocol

Purpose:
- validate the transport layer independently of game semantics

Coverage:
- message framing
- connect/reconnect
- heartbeat
- snapshot cadence
- action-result correlation
- rate limiting

Pass criteria:
- every pass is backed by a valid message exchange, not just "no exception"
- request/result correlation by `request_id` is proven
- rate limiting produces an explicit error signal rather than silent drop/hang

Relevant files:
- [test_a_ipc.py](/c:/Users/Robert/Documents/GWA%20Censured%20X%20BotsHub/gwa3/bridge/tests/test_a_ipc.py)

### Tier 2: Outpost Observations

Purpose:
- prove that the bridge can read trustworthy state in a low-risk environment

Coverage:
- player state
- map state
- party state
- hero skillbars
- inventory/storage/gold
- dialog/merchant closed state
- nearby agents

Pass criteria:
- repeated snapshots are self-consistent
- party and hero data match the known standard setup on Disco Panic
- absent state is reported cleanly as absent, not as false-open/false-populated

Relevant files:
- [test_b_observations.py](/c:/Users/Robert/Documents/GWA%20Censured%20X%20BotsHub/gwa3/bridge/tests/test_b_observations.py)

### Tier 3: Outpost Actions

Purpose:
- validate safe state-changing actions before moving into more complex scenarios

Coverage:
- move/target/cancel
- add/kick hero
- set hero behavior
- flag/unflag
- travel and hard mode
- chat/wait

Pass criteria:
- every action pass must be followed by a visible snapshot change
- deprecated/forbidden actions must return explicit errors
- party restoration is deterministic after tests complete

Relevant files:
- [test_c_actions.py](/c:/Users/Robert/Documents/GWA%20Censured%20X%20BotsHub/gwa3/bridge/tests/test_c_actions.py)
- [test_d_validation.py](/c:/Users/Robert/Documents/GWA%20Censured%20X%20BotsHub/gwa3/bridge/tests/test_d_validation.py)

### Tier 4: Merchant and Dialog Workflow

Purpose:
- validate one of the most stateful bridge paths with real economic side effects

Coverage:
- move to merchant
- interact successfully
- detect merchant open state correctly
- buy one salvage kit
- sell it back

Required evidence:
- merchant open state is visible in snapshot data
- merchant item list is non-empty
- inventory count changes after buy/sell
- gold changes in the expected direction after buy/sell

Pass criteria:
- no "merchant open" pass is allowed without merchant frame/item evidence
- no buy/sell pass is allowed without both inventory and gold evidence

### Tier 5: Explorable Transition

Purpose:
- prove the bridge can follow the same map transition path Froggy relies on

Coverage:
- enter Sparkfly from Gadd's
- detect loaded explorable state
- validate foes appear in nearby-agent observations

Pass criteria:
- map id changes to the expected explorable
- loading state converges to loaded
- at least one live foe is visible through snapshots, or the test skips with explicit reason

### Tier 6: Combat and Loot

Purpose:
- validate the bridge's most timing-sensitive live gameplay actions

Coverage:
- target live foe
- attack or use skill
- observe recharge/cast/target hp movement
- loot pickup

Required evidence:
- target id changes to the requested foe
- skill use produces recharge/cast/target hp evidence
- loot pickup changes item visibility and inventory/gold state

Pass criteria:
- combat action tests do not pass on action return alone
- loot tests do not pass on disappearance alone; inventory or gold must also reflect pickup

### Tier 7: Advisory Mode

Purpose:
- ensure advisory mode can coexist with the live bot state safely

Coverage:
- advisory loop running
- state override/restore
- coexistence with snapshot updates

Pass criteria:
- advisory mode emits coherent recommendations tied to current state
- no stuck state after override/restore

### Tier 8: End-to-End Disco Panic Smoke

Purpose:
- prove the bridge is usable for a realistic multi-phase run on the target validation account

Sequence:
1. Ensure Gadd's Encampment
2. Establish or verify standard hero setup
3. Open merchant
4. Buy salvage kit
5. Sell salvage kit
6. Enter Sparkfly
7. Acquire foe
8. Cast or attack successfully
9. Loot successfully
10. Return to outpost

Pass criteria:
- every phase has explicit before/after evidence
- the run can stop at the first hard failure with enough logs to isolate the broken layer

Relevant files:
- [test_e_orchestrated.py](/c:/Users/Robert/Documents/GWA%20Censured%20X%20BotsHub/gwa3/bridge/tests/test_e_orchestrated.py)

## Proposed Work Order

1. Standardize Disco Panic account sourcing.
2. Make IPC/protocol tests authoritative.
3. Tighten outpost observation tests to the Disco Panic baseline.
4. Tighten outpost action tests so "success" requires evidence.
5. Add merchant round-trip assertions.
6. Add explorable combat/loot evidence.
7. Add advisory validation.
8. Add one Disco Panic end-to-end smoke wrapper.

## Immediate Next Implementation Targets

1. Add a Disco Panic-specific note and execution order to the bridge test docs.
2. Refine Epic 12 kanban acceptance criteria to require observable evidence for all state-changing tests.
3. Tighten `test_e_orchestrated.py` around the Disco Panic standard-hero baseline.
