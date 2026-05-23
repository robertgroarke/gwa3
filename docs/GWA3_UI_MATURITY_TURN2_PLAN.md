# GWA3 UI Maturity — Turn 2 Plan

Companion to [`GWA3_UI_MATURITY_PROMPT.md`](GWA3_UI_MATURITY_PROMPT.md).

## Where we are

- Branch `codex/ui-ux-polish-followup-recheck` points at `03d4661` (slice-2
  closeout). Tests 18/18. None of slice-3's M1–M6 has actually shipped yet —
  last turn only re-verified the existing chain.
- DISCOPANIC lane was unavailable last turn (claimed by another agent), which
  is why no live work happened.
- The gwa3-private worktree has **unrelated dirty bridge / native / Kamadan
  changes**. Do NOT stage, commit, revert, or otherwise touch them.

## Injection elevation (resume preconditions)

Last live attempt produced GWLauncher PID 30372 with `LAUNCH_VERIFIED=1` but
the injector failed with `OpenProcess failed (error 5). Run as Administrator?`
The non-elevated Codex/agent shell cannot OpenProcess or Stop-Process the
launched GW client.

To unblock without elevating the agent itself:
- **Operator step (one-time per blocked PID):** from an elevated PowerShell,
  `Stop-Process -Id 30372 -Force` and verify with `Get-Process Gw`. The
  orphan registry JSON does not exist (`%PROGRAMDATA%\gwa3\sessions\30372.json`
  was never written because inject never succeeded).
- **Inject broker (one-time setup, then leave running):** in an elevated
  PowerShell window, run `tools\inject_broker.ps1` from the parent repo
  (`C:\Users\Robert\Documents\GWA Censured X BotsHub`). It watches
  `%LOCALAPPDATA%\gwa3-inject-broker\requests` and performs OpenProcess+inject
  on behalf of non-elevated callers. Leave the window open for the duration
  of slice-3 live work.
- **Agent call path:** instead of invoking `injector.exe` directly, call
  `tools\inject_via_broker.ps1 -Pid <pid> -Dll <dll_path>`. The shim drops a
  request JSON and polls for the broker's response (`.done.json` + exit code
  + captured stdout). Default injector path is the disco build; override
  with `-Injector <path>` for other lanes.

Once those two are in place, `/goal resume` on the blocked slice-3 session
continues from a warm context with no agent elevation required.

## Commit cadence (mandatory — for operator monitoring)

**Commit small, commit often.** The operator monitors progress by watching
the `gwa3-private` git log; long-running uncommitted changes are invisible
to them and impossible to review incrementally. Concretely:

- One logical change → one commit. Do not batch unrelated edits.
- Commit at every natural checkpoint: a defect closed, a backlog item
  finished, a panel extracted, a test added, a screenshot captured.
- Commits during a single goal-turn should land **as you go**, not all at
  the end. If a turn does 4 backlog items, that should be 4+ commits, not
  one mega-commit.
- Use a clear conventional-style subject (`feat: ...`, `refactor: ...`,
  `fix: ...`, `docs: ...`, `test: ...`).
- Do not skip hooks (`--no-verify`) or amend across goal turns.
- Push or do nothing about remotes — the existing repo rules apply.
- After each commit, the goal-turn report should include the short SHA
  so the operator can jump straight to the diff.
- If a working-tree change is unrelated to the current goal (e.g.
  bridge/native/Kamadan dirty state), do not stage it. Leave it for its
  owning claim.

## Active backlog (authoritative — goal is NOT complete while this section is non-empty)

This section is the running task list for the resumed slice-3 goal. Each
item below is a hard requirement: the completion audit must treat this
section's items as unmet work. The goal may only be marked complete when
every numbered item here is satisfied with verifiable evidence (commit
hash, test count, screenshot under `docs/ui_ux_polish_screenshots/slice3/`,
or live-validation entry in `docs/ui_maturity_slice3_live_validation.md`).

Operators may append new items at any time. If the section is empty *and*
M1–M6 + S1–S4 + L1–L5 are all satisfied, only then may the goal complete.

### Open items

1. **L2 end-to-end UI-driven Launch via broker.** Using `tools\inject_via_broker.ps1`,
   drive a clean DISCOPANIC lane through Launch → exact-PID capture →
   health gate → broker-mediated inject → bridge attach → first telemetry,
   without cancelling the confirmation dialog. Evidence: screenshots per
   supervisor transition, session tagged UI-launched, clean Stop with no
   orphan PID/registry/pipe.
2. **L3 live drift watcher.** Wire the slice-3 LiveSettingsDrift helper to
   live IPC against the broker-injected session. Reproduce in-sync, drift
   (toggle a UI setting without pushing to DLL), and probe-unavailable
   (close the pipe). Evidence: three screenshots + diff payload captured
   in the live-validation report.
3. **L4 degraded-state live.** Reproduce bridge-unreachable / bridge-stale
   / session-ended by killing the bridge, pausing it, then closing the
   pipe. Evidence: one screenshot per state.
4. **L1 multi-session.** Coordinate with the operator on an approved
   second lane. Inject both simultaneously through the broker. Verify
   rail isolation, per-session panel scoping, cross-session log
   collapsing keyed by session-id, independent Stop, independent probe
   round-trips. Evidence: screenshots + appended live-validation section.
5. **L5 30-minute soak with mid-soak re-injection.** Report log-collapser
   max fan-out, UI working-set growth (<50 MB), drift-watcher false
   positives (zero), any UI freeze >250 ms, clean UI Stop with no orphan
   PID/registry/pipe.

### Code-review-derived items (production-quality)

The findings below come from the 2026-05-23 deep-dive review documented
in [`GWA3_UI_MATURITY_PROMPT.md`](GWA3_UI_MATURITY_PROMPT.md) under
"Production-Quality Findings". Read that section before starting each
item — it contains the file:line context.

6. **B1 — Real shell-VM decomposition (not partials).** `MainWindowViewModel`
   today lives across 5 partial files totalling ~5,052 lines (`.cs` 741 +
   `.Launch.cs` 1883 + `.LlmBridge.cs` 719 + `.ProfileAndValidation.cs`
   1131 + `.Utilities.cs` 578). Extract `LaunchOrchestrationViewModel`
   from `.Launch.cs` and `LlmBridgeViewModel` from `.LlmBridge.cs` as
   *separate* classes the shell holds references to. Delete the
   partials when their content has moved. Update `UiDecompositionTests`
   to count combined partial sizes so the gate cannot be passed by
   sharding the same class again.
   Evidence: commit hash; partial file count = 0 for
   `MainWindowViewModel`; combined VM line count published in
   the live-validation report; tests still green.
7. **B3 — Move LLM display fields off the shell.** ~30 `llmSnapshotMap` /
   `llmObserverText` / `llmPlanPhase` / `llmPlannerMode` etc. properties
   (`MainWindowViewModel.cs:166-186`) should live on `LlmConsoleViewModel`
   (104 lines, already exists). Shell binds via `MainViewModel.LlmConsole.X`.
   Evidence: commit hash; `MainWindowViewModel.cs` field count for
   `llm*` private fields = 0; existing UI tests still pass; one new
   `LlmConsoleViewModelTests` exercising the migrated state.
8. **C1 — Remove hardcoded operator-machine paths.** `KnownCharacterLaunchDefaults`
   in `MainWindowViewModel.cs:85-132` hardcodes
   `C:\Users\Robert\Documents\GWA Censured X BotsHub\GWA Censured\debug_scripts\launch_*.au3`
   for every lane. Resolve from `repositoryRoot` or move into the
   profile JSON.
   Evidence: commit hash; grep for `C:\\Users\\Robert` in `ui/`
   returns zero hits; a smoke test that constructs the VM with a
   relocated `repositoryRoot` still resolves launcher paths correctly.
9. **D4 — `BrokerProcessTerminator`.** The current `ProcessTerminator`
   cannot recover from `Win32Exception (Access denied)` (this is the
   exact wall hit on PID 30372 today). Add an `IProcessTerminator`
   implementation that drops a kill-request JSON into
   `%LOCALAPPDATA%\gwa3-inject-broker\requests\` mirroring the inject
   broker contract, and wire `SessionSupervisor` to fall back to it
   when the direct `Stop-Process` path returns access denied. Extend
   `tools/inject_broker.ps1` (in the parent repo) to handle
   `kill.req.json` shapes in addition to inject ones.
   Evidence: commit hash; new `BrokerProcessTerminator.cs` + test;
   broker script handles both kinds; documented in the broker section
   of this file.
10. **D2 — Surface real injector stderr in `InjectionResult.Message`.**
    `InjectorService.cs:75-82` returns generic `"DLL injection failed."`
    while the real `"OpenProcess failed (error 5)"` is buried in the
    inner `ProcessCommandResult`. Compose the result `Message` from
    `result.StandardError` when non-empty.
    Evidence: commit hash; an offline injector-mock test asserting the
    propagated message includes the simulated stderr text.
11. **A1 — Per-lane bearer-token auth on `/api/llm/launch|chat|stop`.**
    `bridge/http_api.py:280-313` accepts unauthenticated POSTs. Generate
    a per-lane token at bridge startup, write it into the
    `%PROGRAMDATA%\gwa3\sessions\<pid>.json` record next to `http_port`,
    require `Authorization: Bearer <token>` on state-mutating routes.
    The WPF UI reads the token from the registry. Public state /
    snapshot reads stay open.
    Evidence: commit hash; new `bridge/tests/test_http_api_auth.py`
    asserting 401 without token and 200 with; WPF
    `Gwa3SessionRegistryService` surfaces the token; per-session UI
    requests carry it.
12. **A2 — Replace the hand-rolled HTTP/1.1 parser.** `http_api.py`
    `_handle` parses requests manually with `split(" ", 2)`, no chunked
    encoding, connection-per-request, trusts attacker-supplied
    Content-Length. Migrate to `aiohttp` (already common in the
    ecosystem) or `hypercorn` with the SSE handler preserved.
    Evidence: commit hash; existing route-shape tests pass; new
    fuzz-style test for malformed request lines returns 400 not 500.
13. **D1 — Tighten `LiveSettingsDrift.Normalize`.** Current normalize
    strips spaces and underscores (`LiveSettingsDrift.cs:63-64`), so
    `"true"` matches `"t r u e"`. Reduce to trim-only or
    case-insensitive-trim.
    Evidence: commit hash; updated `LiveSettingsDriftTests` covering
    `"on"` vs `"o n"` (must NOT match), `" true "` vs `"true"` (must
    match).
14. **D6 — Replace token-substring health classification.**
    `MainWindowViewModel.cs:47-83` uses `string.Contains` on
    `UnhealthyHealthTokens` / `HealthyHealthTokens`. `"issue"` matches
    `"issued"`, `"blocked"` matches `"unblocked"`. Replace with an
    enum-based status published by the supervisor (or whole-token
    matching with word boundaries).
    Evidence: commit hash; new tests showing `"issued"` no longer
    classifies as unhealthy.
15. **E1 — Singleton `HttpClient` lifecycle.** Two `HttpClient`
    instances are created in field initializers and never disposed
    (`MainWindowViewModel.cs:39-40`). Switch to a single shared
    `SocketsHttpHandler` or `IHttpClientFactory`.
    Evidence: commit hash; ownership documented in code comment;
    smoke test that disposing the VM does not leak handles.
16. **E3 + F3 — SSE keepalive + `/healthz`.** Add a server-side
    `:ping\n\n` every 15s on the SSE stream (`http_api.py:_stream`)
    and a `GET /healthz` route returning `{ok:true, lane, status}`
    with no auth required. WPF uses `/healthz` for its bridge chip
    instead of relying on the SSE stream.
    Evidence: commit hash; new `bridge/tests/test_http_api_healthz.py`;
    SSE timing test asserting at least one ping per 20s.
17. **G3 — Broker smoke test.** `tools/inject_broker.ps1` and
    `tools/inject_via_broker.ps1` (parent repo) are untested. Add a
    Pester or PowerShell-based smoke test that mocks `injector.exe`
    with a script printing a known string + exit code, drops a synthetic
    request, and asserts `.done.json`, `.exit`, and `.out` appear with
    the expected contents within the configured timeout.
    Evidence: commit hash; new `tools/tests/inject_broker.tests.ps1`;
    test runs as part of the slice-3 verification report.

### User-value items (operator-perspective review)

The findings below come from the 2026-05-23 "User-Value Findings" section
in [`GWA3_UI_MATURITY_PROMPT.md`](GWA3_UI_MATURITY_PROMPT.md). Read that
section before each item — it has the operator-experience context.
These items prioritize outcomes (gold, drops, safety, unattended
operation) over plumbing.

18. **Persisted run history + view.** Today's Runs/Best/Avg counters are
    in-memory only. Persist every completed run as a JSON record under
    `%LOCALAPPDATA%\Gwa3.UI\runs\<lane>\<yyyy-mm-dd>\<session>.json` with
    start/end, outcome (success/fail/wipe), duration, dungeon level
    reached, gold delta, drops list, fail reason if any. Add a "Run
    history" sub-tab on Monitoring with a sortable table (last 50
    visible, filter by lane/date/outcome) and a click-to-open-bundle
    action.
    Evidence: commit hash; new `RunHistoryService` + tests for
    JSON read/write/rotation; screenshot of populated table after a
    DISCOPANIC run; offline test fixture with 5 synthetic runs renders.
19. **Drop / loot ledger.** Per-session and per-day rollup of drops with
    item name, rarity, count, gold value. Surface rare drops (gold/green
    items) with a highlight row. Aggregate the last 7 days in a
    sparkline ("gold/hour over time"). Backed by the same JSON store as
    item 18.
    Evidence: commit hash; new `DropLedger` parser tests using fixture
    bot logs; screenshot of populated ledger; rare-drop highlight test.
20. **Quick-launch + last-config recall.** A "Resume last session" /
    "Launch last config" button in the top command bar that fires the
    last successful Launch combo for the currently selected lane
    without re-walking Profile/Bot/Character/Mode pickers. Per-lane
    last-known-good config persists under
    `%LOCALAPPDATA%\Gwa3.UI\last-launch-<lane>.json`.
    Evidence: commit hash; offline test for persist/recall round-trip;
    screenshot showing one-click relaunch.
21. **Stop conditions.** Configurable per-session stop rules:
    "stop after N runs", "stop at HH:MM", "stop on rare drop", "stop if
    fail-rate >X% over last N runs", "stop on disconnect". Wired into
    `SessionSupervisor` and surfaced in the Launch tab.
    Evidence: commit hash; per-rule unit test in `StopRuleEvaluator`;
    live DISCOPANIC validation: configure "stop after 1 run", confirm
    the supervisor stops cleanly.
22. **Notifications.** Windows toast (and optional Discord webhook) for:
    run complete, rare drop, run failure, stop-condition fired, bridge
    unreachable >60s, settings drift >0 fields. Single configurable
    notification preferences pane.
    Evidence: commit hash; new `NotificationService` with a no-op test
    sink, real Windows toast sink, webhook sink; settings UI; one toast
    captured in the live-validation screenshot stream.
23. **DLL build-mismatch banner.** The registry record carries
    `dll_build_hash`. The UI knows the build dir it expects per lane.
    When they differ, show a red banner on the Sessions rail card
    ("DLL build a1b2c3 — expected def456 from build_disco") with a
    "Rebuild + re-inject" action. The "Build" rail row currently shows
    "unknown build" silently; that becomes an explicit warning chip.
    Evidence: commit hash; offline test with mock-mismatched records;
    rail screenshot showing the banner; live-validation entry naming
    the hashes compared.
24. **Disconnect / crash recovery.** When the registry record's
    heartbeat goes stale or the GW process exits unexpectedly, surface
    a "Session ended unexpectedly — Re-launch?" action card on the
    rail that re-runs the prior launch config (item 20) for that lane.
    Optional auto-restart toggle (default off) with backoff.
    Evidence: commit hash; offline test for the heartbeat-stale →
    action-card transition; live DISCOPANIC validation: kill DLL
    mid-run, confirm the action card appears and re-launch works.
25. **Profile dirty-state + diff + Save As.** When a Configuration tab
    field changes, mark the profile dirty (visible indicator on the
    Save Profile button). Add a Profile Diff panel showing
    field-by-field "Saved → Current" rows. Add Save / Save As / Revert
    buttons with clear distinction between them.
    Evidence: commit hash; tests for dirty-tracking + diff
    serialization; screenshot of the diff panel mid-edit.
26. **Fleet dashboard tile grid.** A new top-level "Fleet" view (sibling
    to Configuration / Monitoring / LLM Interface) showing one tile per
    known lane with: state badge, current phase, last-5 outcome dots
    (green/yellow/red), gold/hour, alerts/banner. Click a tile to
    select that session in the rail.
    Evidence: commit hash; new `FleetViewModel` + tests; screenshot
    with at least 2 lanes populated (one live + one configured-only).
27. **Bot module discovery from the DLL.** Currently the Bot combo is
    populated by parsing profile JSONs. Instead, query the native DLL
    (via `whoami` IPC extended with `list_bot_modules`) so all
    registered modules — FroggyHM, RragarsMenagerie, RavensPoint,
    ArachnisHaunt, Kathandrax, FrostmawsBurrows, etc. — appear with
    their declared schemas. Backlog item from `GWA3_UI_KANBAN.md`.
    Evidence: commit hash; new IPC handler + Python client; combo
    populated live from the DLL; offline test for the parser.
28. **Operator-in-the-loop LLM controls.** The LLM Interface today is a
    one-way display + one-shot chat. Add: "Pause LLM and let me drive"
    toggle (suspends the planner loop), "Edit plan" (operator rewrites
    the next-step text), "Veto last action" (rolls back the last
    tool call result), inline plan-step skip.
    Evidence: commit hash; new IPC verbs (pause/resume/edit_plan/veto);
    UI affordances; live DISCOPANIC validation: pause via UI,
    confirm the bot stops issuing tool calls, resume, confirm it
    continues.
29. **Session bundle export.** One-click "Export session bundle" that
    zips: bot log, DLL log, launcher log, current profile JSON, last
    snapshot summary, last 50 telemetry events, last 5 screenshots,
    and a manifest. Default save path
    `%USERPROFILE%\Desktop\gwa3-session-<lane>-<timestamp>.zip`. For
    bug reports + multi-agent coordination.
    Evidence: commit hash; offline test for the manifest schema;
    captured screenshot of the bundle being produced; one real
    bundle .zip listed in the live-validation appendix.
30. **In-app help / contextual tooltips.** Every Configuration field
    label and every Monitoring metric row gets a hover tooltip
    explaining what it means and what changing/seeing it implies. F1
    opens a section-specific help drawer. Backed by a single
    `help-strings.json` so future contributors maintain it in one
    place.
    Evidence: commit hash; help-strings file populated for ≥80% of
    labels; XAML pattern documented; one screenshot per major tab
    showing tooltips visible.
31. **Keyboard shortcuts.** Documented chord set: Ctrl+L Launch,
    Ctrl+Shift+L Stop, Ctrl+1..5 select rail position, Ctrl+S Save
    Profile, F5 Validate, F1 Help, Esc clear selection. Shortcuts
    visible in the menu / tooltip.
    Evidence: commit hash; `KeyboardShortcuts.cs` + tests; help drawer
    lists them; screenshot of the menu showing them.

### DLL-surface items (expose live state + controls from the native managers)

The findings below come from the "DLL-Surface Findings" section in
[`GWA3_UI_MATURITY_PROMPT.md`](GWA3_UI_MATURITY_PROMPT.md). Read that
section first — it inventories the 22 managers and 6 bot modules and
maps the gaps. Many items below depend on shared IPC plumbing: the
first item (32) lands that plumbing so the rest can ride it.

32. **Live snapshot IPC + bridge event bus extension.** Foundation for
    items 33–40. Add new IPC verbs that return JSON snapshots:
    `get_player_state`, `get_party_state`, `get_skillbar`,
    `get_agents_nearby`, `get_effects`, `get_dialog`, `get_quest_log`,
    `get_inventory`, `get_chat_recent`. Each reads from cached manager
    state — one-native-call-per-tick rule still applies. Add bridge
    bus events: `player.state`, `party.state`, `skillbar.state`,
    `agents.snapshot` (rate-limited), `effects.changed`, `dialog.shown`,
    `quest.updated`, `inventory.changed`, `drop.received`, `chat.received`.
    Each event is emitted from the DLL's existing tick paths where the
    underlying state already changes.
    Evidence: commit hash; Python `bridge/tests/test_ipc_snapshots.py`
    round-trips every new verb against a mock pipe; new bridge events
    documented in `docs/IPC_PROTOCOL.md` (create if absent); one
    `get_player_state` round-trip captured live on DISCOPANIC.
33. **Live player + party state panel.** New top-of-Monitoring widget
    showing player HP/energy bar, position (map + coords), active
    title, and a compact row per hero with HP/energy/effects pips. Wired
    to the `player.state` / `party.state` events from item 32; refreshed
    on event, not polled.
    Evidence: commit hash; new `PlayerPartyStateViewModel` + tests;
    screenshot showing live HP bars during a DISCOPANIC run.
34. **Live skill bar overlay.** 8 slots with cooldown rings + recharge
    seconds + last-cast indicator, rendered above the Monitoring tab.
    Wired to `skillbar.state`. Includes hero skill bars on hover.
    Evidence: commit hash; new `SkillBarViewModel` + tests with synthetic
    `CachedSkill` fixtures; screenshot during a cast cycle.
35. **Mini-map with agents and bot route.** A small canvas showing the
    bot's position + nearby agents (color-coded by hostility) + the
    last-N path waypoints, refreshed off `agents.snapshot` /
    `player.state`. Click an agent to copy its name/id for diagnostics.
    Closes the long-standing kanban backlog item "Add visual
    route/debug map panels inspired by Py4GW widgets."
    Evidence: commit hash; new `MiniMapView` + offline render test
    with a fixture agent list; screenshot during a real run with at
    least one hostile + one party agent rendered.
36. **Live inventory grid + drop toast.** Visual bag grid (4 bags × 20
    slots), tile-per-item with stack count and rarity tint, hover
    tooltip with full mod string. Drop events fire a small slide-in
    toast in the Monitoring tab (separate from system notifications in
    item 22) that decays after 3 s; rare drops persist until
    acknowledged.
    Evidence: commit hash; new `InventoryGridView` + tests with fixture
    inventory + drop events; screenshot of populated bags + one
    in-flight drop toast.
37. **Bot module catalog + per-bot default profiles.** Builds on item
    27 (discovery via DLL). For every registered bot module the
    catalog lists, generate a starter profile JSON under
    `ui/profiles/defaults/<bot>-<lane>.default.json` covering the
    five built lanes (BEASTRIT / DISCO / BLUMPKINS / MARVIN / BISCUIT)
    and the six built bots (Froggy / Arachnis / Frostmaws / Kathandrax
    / Ravens / Rragars). Operators get 30 profiles instead of 3.
    Evidence: commit hash; new profile JSONs present and validate clean
    under `ProfileValidator`; UI smoke test loads the new catalog and
    populates the Bot combo from it.
38. **Per-bot dashboard templates.** Each bot module declares a
    dashboard schema (key milestones, drop-of-interest list, ETA model).
    The Monitoring tab swaps to the template that matches the running
    bot — Froggy keeps its dungeon-level/ETA panel; Kathandrax gets
    boss-stage tracking; Arachnis gets web-room tracking, etc.
    Evidence: commit hash; new `BotDashboardSchema` contract + per-bot
    schema files; offline render test for each of the 6 bots with
    fixture telemetry; live-validation screenshot of Froggy on
    DISCOPANIC using the new templated view.
39. **Bidirectional chat panel.** A Chat tab (or sidebar) showing
    incoming whispers/party/all (from `chat.received`) and a send box
    that drops a `send_chat` IPC verb to ChatMgr. Supports filtering by
    channel and operator-only acknowledged-whispers indicator.
    Evidence: commit hash; new `ChatPanelView` + VM tests; live
    DISCOPANIC validation: operator sends a `/team` message via the UI
    and confirms it appears in-game.
40. **Manual control mode.** A "Take the wheel" toggle that suspends
    the bot's planner loop and lets the operator drive: click an agent
    to target, click the map to move, click a skill slot to cast (uses
    `SkillMgr`/`CombatMgr`/`TravelMgr` write paths). Releasing the
    toggle hands control back to the bot at the current state.
    Evidence: commit hash; new `ManualControlViewModel` + IPC verbs;
    live DISCOPANIC validation: take wheel, move to a coord, cast a
    skill, release, confirm bot resumes from new position.
41. **Run comparison view.** Picks N runs from the persisted history
    (item 18) and renders a side-by-side table with deltas highlighted
    — best/worst time, drop differences, fail-point overlap. Sparkline
    per metric.
    Evidence: commit hash; new `RunComparisonViewModel` + tests with
    fixture run history; screenshot of a 3-run comparison.
42. **Multi-agent coordination dashboard.** A new view rendering
    `AGENT_WORK_REGISTRY.md` and `AGENT_ACCOUNT_REGISTRY.md` as live
    tables: who-owns-what-claim, current state, when claimed, files
    touched. Detect collisions (same lane on multiple active claims)
    and flag. Lets the operator see the exact state that caused the
    slice-3 elevation block without alt-tabbing to a markdown file.
    Evidence: commit hash; new `AgentRegistryView` + parser tests with
    fixture registry markdown; screenshot showing at least one active
    claim and one collision warning.
43. **Full MaintenanceMgr config coverage + raw JSON profile editor
    fallback.** The Maintenance tab surfaces every `Config` field with
    labels and tooltips (deposit thresholds, conset triggers, material
    trader coords, gold floors, kit targets — 20+ knobs). A "Raw JSON"
    tab on the Profile editor lets the operator edit any field the
    fixed-field UI does not surface, with on-the-fly validation against
    the profile schema. New maintenance-town entries (xunlai/material-
    trader coords) are editable.
    Evidence: commit hash; updated Maintenance XAML covers every field
    in the `Config` struct (test asserts coverage by reflection over the
    struct vs the XAML bindings); raw-JSON editor round-trips; screenshot
    of the expanded Maintenance tab + the JSON editor mid-edit.

### LLM steering + observability items

The LLM Interface tab today renders plan phase / intent / chat history /
tool calls / snapshot summary and the chat box does drive
`agent_loop.inject_user_message`. But the operator cannot redirect the
*objective* mid-session, cannot see the prompt context the LLM is
actually working from, cannot watch cost/tokens/latency, and cannot
queue directives with acknowledgment. The items below close that gap so
"running Froggy loops, tell it to swap to maintenance / go sell in
Kamadan" is a first-class workflow, not a chat-message gamble.

44. **Goal/objective override (structured, not chat).** Add a "Set
    objective" affordance distinct from the chat box. The operator picks
    or types a new objective ("go to maintenance town and restock kits",
    "travel to Kamadan and sell gold-rarity items", "swap to seller bot",
    "finish current run, then stop"). The directive is sent through a
    new IPC verb `set_objective` that the agent loop treats as
    higher-precedence than the launch-time objective until satisfied or
    cleared. Persists in `last_tool_calls` with role `directive`.
    Evidence: commit hash; new IPC verb + agent_loop handler; UI
    affordance with a queue indicator; live DISCOPANIC validation:
    inject "go to maintenance" mid-Froggy and confirm the planner's
    next plan.updated reflects the new intent.
45. **Directive queue + acknowledgment timeline.** Multiple directives
    can stack: "finish current run", "go sell rares in Kamadan", "resume
    Froggy". Each shows in a small queue panel with: state
    (pending/in-progress/done/cancelled), submitted-at, acknowledged-at
    (when the planner first incorporates it), satisfied-at. Operator can
    re-order, cancel, or clear the queue.
    Evidence: commit hash; new `DirectiveQueueViewModel` + tests; UI
    screenshot of a 3-item queue mid-run; agent_loop persists the queue
    in run.summary.
46. **LLM context / prompt inspector.** A read-only "Context" pane that
    shows the actual messages sent to the planner on its last call:
    system prompt, observation history, tool results, current
    directive. Lets the operator diagnose "why didn't the LLM act on my
    instruction?" — the answer is almost always "it never saw it."
    Refreshes per planner turn via a new bridge event
    `prompt.composed` carrying a redacted summary (token counts +
    message previews, full content on click).
    Evidence: commit hash; new bridge event + handler; UI screenshot
    showing the inspector after a chat injection; offline test that
    redaction strips secrets if any.
47. **Cost / tokens / latency telemetry.** `_record_llm_token_usage`
    already exists in `agent_loop.py` — surface it. A small persistent
    strip on the LLM Interface tab shows: last-call latency, last-call
    prompt+completion tokens, rolling cost estimate (configurable
    per-model price), tokens/hour, calls/minute. A click opens a sparkline
    of the last hour.
    Evidence: commit hash; sparkline data model + tests; live
    validation entry showing real numbers from a DISCOPANIC session.
48. **Quick-action directive library.** A row of buttons / dropdown of
    common directives the operator routinely needs: "Go to maintenance
    town", "Deposit gold at Xunlai", "Sell all white/blue", "Identify all
    rares", "Sell all gold-rarity to merchant", "Travel to Kamadan",
    "Swap to seller bot", "Pause until I say resume", "Finish current run
    then stop". Each is a pre-canned `set_objective` payload (item 44)
    so the planner treats it as a structured directive, not a chat hint.
    Library entries are loadable from a JSON file so power users can
    add their own.
    Evidence: commit hash; `quick-directives.json` + UI loader; library
    rendered as buttons; live DISCOPANIC validation: click "Go to
    maintenance town" mid-Froggy and confirm the planner travels.
49. **Live autonomy + allowed-action controls.** The Autonomy and
    Allowed-actions panel in `LlmConsoleView` is display-only. Make them
    editable mid-session: tightening autonomy from "Autonomous" to
    "Advisory" suspends tool execution and forces operator approval;
    toggling an allowed-action off causes the planner to skip it and
    reroute. Wired via the existing `set_settings` IPC verb extended
    with `autonomy_level` and `allowed_actions` fields.
    Evidence: commit hash; agent_loop respects mid-session autonomy
    changes; UI test that toggling "Advisory" surfaces the
    approval-required state; live DISCOPANIC validation: drop autonomy
    mid-run, confirm the next tool call requires approval.

### Bridge console / objective items (close the launch-banner gap)

When DISCOPANIC (or any LLM lane) launches today, a separate CMD window
appears showing the bridge's startup banner:

```
GWA3 LLM Bridge
LLM:       http://localhost:11434/v1
Provider:  openai-compatible
Model:     qwen3.5:cloud
Profile:   qwen-safe
Planner:   openai-compatible:qwen3.5:cloud
Executor:  openai-compatible:qwen3.5:cloud
Modes:     supervisor=deterministic executor=health-check planner=async prompt=delta
Autonomy:  advisory
Pipe:      \\.\pipe\gwa3_llm_disco
Lane:      disco
Objective: Farm Bogroot Growths HM repeatedly with Froggy HM in advisory mode. Use the configured lane only.
[Bridge] HTTP/SSE listening on http://127.0.0.1:8765
[Bridge] Connecting to gwa3 pipe \\.\pipe\gwa3_llm_disco...
[TwoModel] Starting planner/executor loop
[TwoModel] Objective: Farm Bogroot Growths HM repeatedly with Froggy HM in advisory mode.
```

Most of those fields are in the UI but spread across panels. Two are
NOT in the UI (the Objective string, and a single-glance "I'm running X
on Y in Z mode" summary). And the CMD window itself indicates bridge
stdout is escaping the UI's log capture.

50. **Display the launch-time objective on the LLM Interface tab.**
    The bridge agent loop already holds an `objective` string in its
    runtime config (`bridge/agent_loop.py`). Surface it via the existing
    `llm.telemetry` event (or a one-shot `bridge.status` payload at
    startup) and render it on the LLM Interface tab above the Plan +
    State panel as a wrap-friendly read-only block: "Objective: ..."
    with a copy-to-clipboard button. The field is distinct from
    `LlmPlanIntent` (near-term planner intent) — name it
    `LaunchObjective` in the VM. Bind from the session's
    `LaunchObjective` so it updates when rail selection changes.
    Evidence: commit hash; new bridge event payload field + Python
    side test; new `LaunchObjective` property on `LlmConsoleViewModel`
    + `SessionViewModel` with unit test; screenshot of the LLM
    Interface tab showing the populated objective for DISCOPANIC.
51. **Bridge connection summary banner.** A single-row condensed
    banner on the LLM Interface tab that mirrors the CMD startup
    format: `qwen-safe • qwen3.5:cloud • advisory • pipe gwa3_llm_disco
    • bridge 127.0.0.1:8765 • disco`. Pure presentation; bind to
    existing `LlmProfileName` / `LlmPlannerModel` / `SelectedAutonomyLevel`
    / `PipeName` / `LlmBridgeEndpoint` / `LaneTag` fields with a small
    converter that joins non-empty values with the `•` separator.
    Place above the existing Plan + State panel, below the new
    Objective block from item 50.
    Evidence: commit hash; new `BridgeSummaryConverter` + unit test
    covering empty/partial/all-fields cases; screenshot showing the
    populated banner during DISCOPANIC validation.
52. **Suppress the bridge console window and route stdout/stderr into
    the UI logs.** The bridge Python process is currently launched
    with a visible CMD window (CREATE_NEW_CONSOLE or no stdout
    redirect), so its `[Bridge]` and `[TwoModel]` lines escape the
    UI's log capture. Modify the bridge process launch in the UI's
    `Gwa3.UI.Supervisor` (the `BridgeService` / `LauncherService`
    spawn path that starts the Python bridge) to use
    `CREATE_NO_WINDOW` + redirect stdout/stderr to pipes, then tee
    the lines into the existing LogPane runtime log with a `Bridge`
    source tag. Operators no longer see a stray CMD window; the
    startup banner and live `[TwoModel]` lines become searchable in
    the Logs tab.
    Evidence: commit hash; verify on DISCOPANIC live: launch a lane,
    confirm NO standalone CMD window appears, confirm the startup
    banner lines are visible in the Logs tab with source=Bridge,
    confirm shutting the UI Stop cleanly terminates the bridge
    process (no orphaned pythonw.exe); offline unit test for the
    process-start arguments builder asserting `CreateNoWindow=true`
    and stdout/stderr are redirected.

### How to extend

Append numbered items below item 5 with: a concrete acceptance criterion
("evidence: ...") and a verifiable artifact (file path / commit / test
name / screenshot name). Vague items ("do more UI polish") do not count —
the audit will skip them as not-concrete and may complete the goal anyway.

## Lane discipline (mandatory)

- Read `AGENT_WORK_REGISTRY` and `AGENT_ACCOUNT_REGISTRY` before any live
  action. If DISCOPANIC is claimed or marked active, do not launch/inject
  there.
- Coordinate with the operator to pick an available lane. Built lanes:
  BLUMPKINS / BEASTRIT / BISCUIT / MARVIN / DISCOPANIC. Claim explicitly in
  the registry, do the work, release it.
- If no lane can be claimed this turn, ship every offline-safe objective and
  report the live ones as deferred with a specific resume condition. **Do not
  synthesize live evidence.**

## Branch + scope

- Branch off `03d4661` onto `codex/ui-maturity-slice3`.
- Keep edits scoped to `ui/`. If M5 needs the WPF bridge client touched
  elsewhere, that is fine; anything outside `ui/` must be called out first.

## Offline-safe objectives (ship regardless of lane)

**S1 — M5 partial: bridge client + degraded-state UX.** In
`MainWindowViewModel.cs`, `llmBridgeClient` uses `Timeout.InfiniteTimeSpan`.
Replace with a bounded timeout (default ~10s) for everything except the SSE
stream, plus per-request `CancellationToken` on chat / launch / stop. Add
degraded-state chips: bridge unreachable, bridge stale, settings-probe
unavailable, session ended. Unit-test bounded-timeout behavior with a mock
SSE / chat handler.

**S2 — M3 offline: drift-watcher diff logic.** Pure helper in
`Gwa3.UI.Core` (mirroring `LogPresentation` / `MonitoringPresentation`) for
the get_settings drift diff + rate limiter. Offline fixtures: matching,
numeric mismatch, string mismatch, missing-field-on-UI-side,
missing-field-on-DLL-side, IPC timeout, IPC error. Live wiring waits on a
lane.

**S3 — M4 incremental decomp.** Extract per-feature view models +
UserControls from `MainWindowViewModel.cs` (~5.8k lines) and
`MainWindow.xaml` (~1.7k lines). Order: SessionsRail, Monitoring,
LlmConsole, ProfileEditor, Validation, LogPane, Diagnostics. One panel per
commit; each commit independently green; UiSmokeTests +
SessionRegistryDiscoveryTests stay at 18/18. Target ≥40% line reduction in
the shell files. Partial-but-clean is fine.

**S4 — M1 collapser test (offline).** Smoke test asserting the duplicate
collapser keys on `(session-id, source, level, message)`, not
`(source, level, message)`. Synthetic fixtures, no live session.

## Live objectives (only if a lane is cleanly claimed)

- **L1 (M1 multi-session live):** two lanes injected simultaneously; verify
  rail isolation, per-session panel scoping, cross-session collapsing per
  S4, independent Stop, independent probe round-trips.
- **L2 (M2 end-to-end Launch):** clean lane, click Launch, drive the full
  supervisor state machine through to first telemetry — do NOT cancel at
  the confirmation dialog. Confirm session tagged UI-launched. Clean Stop.
- **L3 (M3 live wiring):** wire S2 to live IPC; reproduce in-sync, drift,
  probe-unavailable.
- **L4 (M5 live):** reproduce bridge-unreachable / bridge-stale /
  session-ended live (kill bridge, pause it, close pipe).
- **L5 (M6 soak):** 30-min soak, one mid-soak re-injection. Report
  log-collapser max fan-out, UI working-set growth (target <50 MB),
  drift-watcher false positives (target zero), any UI freeze >250 ms.
  Clean UI Stop, no orphan PID / registry JSON / pipe.

## Deliverables

- Topic branch with commits per objective (S3 broken per panel).
- Offline tests: bounded-timeout client, drift-diff fixtures, cross-session
  collapser fixtures.
- Any live work shipped: screenshots under
  `docs/ui_ux_polish_screenshots/slice3/`; appended section in
  `docs/ui_ux_polish_followup_live_validation.md`.
- Final report: changed files, lane used (or "no lane claimable this
  turn"), per-objective status (shipped / partial / deferred with resume
  condition), test counts, surfaces still needing live proof.

## Turn success criteria

- All offline-safe (S1–S4) land with tests green.
- VM/XAML decomp has ≥2 panels extracted cleanly.
- Bridge client is provably bounded-timeout (no `InfiniteTimeSpan`).
- Unrelated dirty worktree changes untouched.
- If no lane was claimable, the report says so and names which live
  objectives are blocked on what.
