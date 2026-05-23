# GWA3 UI Maturity Prompt — Slice 3

Third pass, following:
- [`GWA3_UI_UX_POLISH_PROMPT.md`](GWA3_UI_UX_POLISH_PROMPT.md) (slice 1 — visual polish + DISCOPANIC live validation)
- [`GWA3_UI_UX_POLISH_FOLLOWUP_PROMPT.md`](GWA3_UI_UX_POLISH_FOLLOWUP_PROMPT.md) (slice 2 — empty-state, tooltip, FixHint defects + the four closeout gaps)

## Where slice 2 left us (verified)

Commit `03d4661` on `codex/ui-ux-polish-followup` closed the slice-2 gaps:
- `get_settings` / `whoami` IPC probe on the DLL pipe round-trips against the
  live DISCOPANIC DLL; `IpcServer.cpp` gained an explicit named-pipe DACL so the
  non-elevated WPF UI can probe elevated externally injected sessions
  (`PID 48156` confirmed end-to-end).
- Hard mode / Open chests / Pickup gold were toggled in the UI and the DLL
  runtime-effective values changed to match — proof that profile settings
  reach the live DLL.
- Launch / Dry Run / Stop on externally discovered sessions now require a
  PID + DLL confirmation dialog; Stop on PID 48156 killed only that PID.
- LLM Interface tab is session-scoped — switching the rail from injected
  DISCOPANIC to idle BLUMPKINS changes the panel contents.
- Tests pass (14/14), build passes, cmake disco passes, GW PIDs and registry
  claim are clean.

## Acknowledged gaps from slice 2 (carry into this slice)

1. No two simultaneous injected external LLM sessions exercised.
2. Launch and Dry Run were only exercised through the confirmation dialog, then
   cancelled. No end-to-end UI-driven launch was actually completed.
3. No full bot-route soak — short-burst validation only.

This slice closes those, plus the structural debt the first review flagged but
neither slice has touched.

---

```text
Goal: Mature the GWA3 desktop UI past the "looks polished, single-session validated" stage into a reliable multi-session operator console with the architectural and degraded-state debt paid down. Desktop UI is canon; web/API conforms.

Start in C:\Users\Robert\Documents\gwa3-private on a clean topic branch off codex/ui-ux-polish-followup (commit 03d4661). Read AGENTS.md and the registry/build/launcher docs first. Coordinate via AGENT_WORK_REGISTRY. Same MVVM / no-backend-behavior-changes / flat-theme constraints.

Constraints recap (still in force):
- Presentation/architecture work only. Do not change game-side behavior, packet protocols, lane-safety rules, or one-native-call-per-tick discipline.
- Live work uses validated launcher → exact-PID injection → kill-only-your-PID discipline. DISCOPANIC remains the primary lane; a second lane is used only for the multi-session check (M1) — coordinate with the operator before launching it.
- Pre-existing dirty edits in MainWindowViewModel.cs: do not silently sweep them in; if they are still present, isolate them on a separate commit.

Primary objectives:

M1. Multi-session correctness.
   The slice-2 closeout explicitly admits "no two simultaneous injected external LLM sessions" was tested. Fix that now.
   - Inject two lanes simultaneously: DISCOPANIC (build_disco / gwa3_disco.dll / \\.\pipe\gwa3_llm_disco) and one other already-built lane chosen with the operator (likely BLUMPKINS or BEASTRIT). Use the validated launcher + exact-PID flow for each; never start Gw.exe directly; never kill a PID you did not launch.
   - With both injected, verify:
       * Sessions rail lists both, each with its own provenance chip, build hash, pipe, heartbeat.
       * Selecting either session updates Monitoring, LLM Interface, and the Diagnostics "Probe live settings" results to that session only. No state from session A leaks into session B's panels.
       * The duplicate-line log collapser de-duplicates within a session, not across sessions. Two identical reconnect warnings from two different sessions must be two rows, not collapsed to one.
       * Stop on session A terminates only PID A; session B continues to report a fresh heartbeat.
       * Probe live settings round-trips correctly against each pipe independently.
   - Screenshot each check. Add a smoke test asserting cross-session log collapsing is gated by session/source identity.

M2. End-to-end UI-driven Launch proof.
   Launch and Dry Run have only been exercised up to the confirmation dialog. Drive at least one full UI-driven Launch through to a healthy injected session.
   - Pick a clean lane state (no prior GW client for that lane), click Launch in the UI, let the supervisor state machine run: GWLauncher invocation → exact-PID capture → health gate → DLL inject → bridge attach → first telemetry.
   - Capture screenshots at each supervisor state transition. The Recent launch log must show the full timeline without spam.
   - Confirm the resulting session is tagged `UI-launched` (not `externally discovered`) in the rail.
   - Cleanly Stop the session at the end; verify only that PID died and no orphan registry JSON remains under %PROGRAMDATA%\gwa3\sessions\.
   - If any supervisor step fails or hangs, report what the UI showed at the moment of failure — that is the degraded-state evidence for M5.

M3. Continuous settings-drift watcher (extend slice 2's one-shot probe).
   The probe currently fires on demand. Promote it to a low-rate background watcher (5–10 s cadence per selected session) that reads get_settings and flags any field where UI profile ≠ DLL-effective. Drift surfaces as:
   - A small status chip on the rail card ("settings: in sync" / "settings: drift (3)") with the field count.
   - A drift row in Diagnostics' probe table, sorted with mismatches first, with last-checked timestamp.
   - No new game/native calls — the DLL still reads from cached config; rate-limit the IPC.
   - When a probe call times out or errors, the chip degrades to "settings: probe unavailable" rather than disappearing.
   - Unit-test the diff logic offline with fixtures (matching / numeric mismatch / missing-field on either side / IPC error).

M4. Decompose the MainWindowViewModel monolith and MainWindow.xaml shell.
   The original UI_REVIEW called this out and slice 2 deferred it. Now that the live-validation infrastructure proves out behavior, the refactor is safer. Scope:
   - Extract per-feature view models: SessionsRailViewModel, MonitoringViewModel, LlmConsoleViewModel, ProfileEditorViewModel, ValidationViewModel, LogPaneViewModel, DiagnosticsViewModel. The shell MainWindowViewModel becomes a thin composition root.
   - Extract corresponding UserControls in XAML: SessionsRailView, MonitoringView, LlmConsoleView, ProfileEditorView, ValidationView, LogPaneView, DiagnosticsView. MainWindow.xaml becomes a layout + tabs file.
   - Keep all existing bindings working — this is a pure restructure. UiSmokeTests must keep passing without behavioral change.
   - Do this incrementally: one panel per commit, each commit independently buildable and testable, so review can follow the seams. Stop at a sensible point if running long — partial decomposition beats a single mega-commit that's hard to review.

M5. Degraded-state UX + the HttpClient.Timeout.Infinite bug.
   UI_REVIEW called out that `llmBridgeClient` uses `Timeout.InfiniteTimeSpan` — a wedged bridge can hang the UI request thread indefinitely. Fix it (bounded timeout for everything except the SSE stream; per-request CancellationToken on chat/launch/stop). Then surface degraded states honestly:
   - Bridge unreachable: rail card shows "bridge: unreachable" chip, retry counter, last-success timestamp.
   - Bridge replying but stale (no SSE events for >30s): "bridge: stale" chip.
   - DLL pipe open but get_settings times out: "settings: probe unavailable" (from M3).
   - DLL pipe closed: rail card grays out with "session ended" label, but the row stays for ~60 s before being pruned so the operator can see what happened.
   - Re-trigger each degraded state during DISCOPANIC validation (kill the bridge process; pause it; close the pipe) and screenshot each.

M6. Stability soak.
   30-minute soak with the UI attached to a live DISCOPANIC session running a real bot route (Froggy HM or whatever the lane's default profile resolves to). During the soak:
   - The duplicate-line collapser must hold up at scale (record max collapse fan-out per row).
   - Memory: UI process working set should not grow more than ~50 MB/30 min.
   - Settings drift watcher (M3) must produce zero false positives.
   - One re-injection (kill DLL, re-inject) mid-soak: rail must transition to "session ended" then re-discover cleanly.
   - End the soak by Stopping cleanly via the UI; confirm no orphan PIDs, no orphan registry JSON, no orphan pipes.
   - Report soak observations: any UI freeze >250 ms, any thread-pool starvation warnings, any log spam regressions.

Deliverables:
- Topic branch off 03d4661 with commits per objective (M1 / M2 / M3 / M4 broken further into per-panel commits / M5 / M6).
- Updated screenshots under docs/ui_ux_polish_screenshots/ in a slice3/ subfolder.
- New offline tests: cross-session log collapsing, drift-diff fixtures, bounded-timeout client behavior (mock SSE / chat / launch).
- A live-validation report appended to docs/ui_ux_polish_followup_live_validation.md or a new docs/ui_maturity_slice3_live_validation.md with: PIDs used, multi-session evidence, end-to-end Launch timeline, drift-watcher screenshots, each degraded-state screenshot, soak metrics.
- Final list of changed files and remaining honest gaps. If M4 can only be partially completed, ship the extracted panels cleanly and document which remain inside the monolith.

Success criteria:
- Two simultaneously injected sessions render side by side in the rail with no state cross-contamination across Monitoring / LLM Interface / Diagnostics / log collapsing.
- A full UI-driven Launch on a clean lane has been demonstrated end-to-end (not cancelled at the dialog).
- The Diagnostics probe runs continuously and shows drift status accurately, including when the IPC is unavailable.
- MainWindowViewModel.cs and MainWindow.xaml are materially smaller (>=40% line reduction) with the panels extracted into testable view-models + UserControls. Smoke tests still pass.
- No UI request can hang indefinitely on a wedged bridge; degraded states are labelled, not blank.
- A 30-minute soak with one re-injection completes with clean Stop, no orphan PID/registry/pipe, no UI hang, no measurable memory growth beyond ~50 MB.
```

## Production-Quality Findings (deep-dive review, 2026-05-23)

This appendix records concrete defects found in a code-level review of both
UIs (WPF desktop + bridge HTTP/SSE API). Each finding names a file:line, an
observed defect, and a concrete fix direction. The active backlog in
[`GWA3_UI_UX_POLISH_FOLLOWUP_PROMPT.md`](GWA3_UI_UX_POLISH_FOLLOWUP_PROMPT.md)
and [`GWA3_UI_MATURITY_TURN2_PLAN.md`](GWA3_UI_MATURITY_TURN2_PLAN.md) will
add numbered items 6+ that pick these up.

### A. Security / hardening

- **A1. `bridge/http_api.py` has NO authentication on control endpoints.**
  `/api/llm/chat`, `/api/llm/launch`, `/api/llm/stop` accept any local
  POST. On a multi-agent box, any process can drive any lane's bridge —
  lateral-movement vector even though bind is `127.0.0.1`. Add per-lane
  bearer token: generate at launch, write into the `%PROGRAMDATA%\gwa3\sessions\<pid>.json`
  record next to `http_port`, require `Authorization: Bearer <token>` on
  state-mutating endpoints. The WPF UI and other legitimate clients read
  the token from the registry record.
- **A2. Hand-rolled HTTP/1.1 parser is fragile.** `http_api.py:237`
  `request_line.decode("iso-8859-1").strip().split(" ", 2)` throws
  `ValueError` on malformed input, caught only by the outer 500-handler
  which leaks `str(exc)` to the caller (`http_api.py:251`). No chunked
  encoding, no keep-alive reuse, `readexactly(length)` trusts attacker-
  supplied `Content-Length`. Replace with a vetted minimal stack
  (`aiohttp` or `hypercorn`); the bridge already runs asyncio.
- **A3. CORS `*` baked into every response.** `http_api.py:333,361` —
  acceptable for localhost but blocks future non-localhost binding
  without an audit. Make CORS origin configurable; default to disallow.

### B. Architecture / decomposition reality

- **B1. The slice-3 decomposition didn't actually split the shell VM.**
  `MainWindowViewModel` lives across 5 partial files totalling **~5,052
  lines**:
  - `MainWindowViewModel.cs` 741
  - `MainWindowViewModel.Launch.cs` 1,883
  - `MainWindowViewModel.LlmBridge.cs` 719
  - `MainWindowViewModel.ProfileAndValidation.cs` 1,131
  - `MainWindowViewModel.Utilities.cs` 578

  The `UiDecompositionTests` line-count gate (`≤ 2000` on
  `MainWindowViewModel.cs`) passes only because it inspects the primary
  partial file. Same class, same coupling, same merge surface — just
  spread across more files. The real fix is to extract per-feature view
  models that the shell *holds references to*, not partials of the same
  type. Candidates: `LaunchOrchestrationViewModel` (the 1.8k-line
  Launch partial), `LlmBridgeViewModel` (the 0.7k LlmBridge partial),
  `ProfileEditorViewModel` already exists but lots of profile logic is
  still in `ProfileAndValidation.cs`.
- **B2. `SessionAndDetailViewModels.cs` is 1,822 lines** — a "VM god
  file" holding every per-session/detail VM in one source. Split by
  concept (SessionViewModel.cs / SessionMonitoringViewModel.cs /
  ValidationCheckViewModel.cs / HeroBuildViewModel.cs / ...).
- **B3. The shell VM still owns ~30 LLM display fields.** `llmSnapshotMap`,
  `llmSnapshotHp`, `llmObserverText`, `llmPlanPhase`, `llmPlanIntent`,
  `llmPlanNextStep`, `llmPlanDeviation`, `llmExecutorMode`,
  `llmPlannerMode`, `llmPromptMode`, etc. (MainWindowViewModel.cs:166–186).
  `LlmConsoleViewModel` exists (104 lines) and should own these — the
  shell should bind through it via `MainViewModel.LlmConsole.PlanPhase`.

### C. Portability / hardcoded developer paths

- **C1. `KnownCharacterLaunchDefaults` hardcodes
  `C:\Users\Robert\Documents\GWA Censured X BotsHub\...`** for every
  lane's launcher script (`MainWindowViewModel.cs:85-132`). The
  application will not run on any other developer's box or under a
  different repo layout. Move launcher paths into the profile JSON or
  resolve relative to `repositoryRoot` (the field is right there).
- **C2. `llmEndpoint = "http://localhost:11434/v1"`** and
  `llmBridgeEndpoint = "http://127.0.0.1:8765"` are hardcoded field
  initializers (`MainWindowViewModel.cs:164-165`). Move to profile or
  to the per-session registry record. The whole point of the registry's
  `http_port` field is exactly this.

### D. Reliability / error surfaces

- **D1. `LiveSettingsDrift.Normalize` is too aggressive.** Strips
  spaces AND underscores before equality (`LiveSettingsDrift.cs:63-64`).
  So `"true"` matches `"t r u e"` and `"hard_mode"` value matches
  `"hardmode"`. For boolean settings this hides nothing, but for any
  setting where whitespace/underscores are meaningful it masks drift
  silently. Tighten to trim-only.
- **D2. `InjectorService.InjectAsync` returns generic
  `"DLL injection failed."`** (`InjectorService.cs:80`). The real
  message — `"OpenProcess failed (error 5). Run as Administrator?"` —
  is buried in the inner `ProcessCommandResult` and never surfaced to
  the operator without manual log digging. Bubble the injector's stderr
  into the result `Message` field directly.
- **D3. `ProcessHealthGate.TryResolveHealthyCharacterProcess` swallows
  WMI failures silently** (`ProcessHealthGate.cs:164-175`). Catches
  `ManagementException`, `InvalidOperationException`, `ArgumentException`
  and returns `null` — caller cannot distinguish "no candidate" from
  "WMI broken." Promote these to a logged warning on the status sink.
- **D4. `SessionSupervisor.CleanupOwnedClientAsync` cannot recover from
  the elevation case the operator just hit.** When
  `Win32Exception (Access denied)` fires (`SessionSupervisor.cs:140`),
  it publishes a warning and leaves the orphan PID. The new
  `tools/inject_broker.ps1` could perform the kill on behalf of the
  non-elevated process; the supervisor needs an `IProcessTerminator`
  alternate implementation (`BrokerProcessTerminator`) that drops a
  kill-request into the broker's request directory the same way
  `inject_via_broker.ps1` does for inject. This is the same exact
  surface the user manually fixed today.
- **D5. `LauncherService.TryReadLauncherLog` has dead code and silent
  IO swallowing.** `var candidates = new[] { ... };` has exactly one
  element so `Distinct(...)` is dead (`LauncherService.cs:83-85`).
  `catch (IOException) {}` / `catch (UnauthorizedAccessException) {}`
  silently return empty — if the .log file is locked, the launcher PID
  parse degrades silently to whatever stdout/stderr produced. Log the
  IO failure to the status sink.
- **D6. Token-substring health classification is fragile.**
  `MainWindowViewModel.cs:47-83` declares `UnhealthyHealthTokens` and
  `HealthyHealthTokens` lists used for `string.Contains` matching. So
  `"issue"` matches `"issued"`, `"blocked"` matches `"unblocked"`,
  `"failed"` matches `"unfailed"`. Replace with a small state-machine
  parser keyed on whole tokens or status enums published from the
  supervisor.

### E. Resource lifecycle

- **E1. Two `HttpClient` instances created in field initializers, never
  disposed.** `MainWindowViewModel.cs:39-40`. The VM has no `IDisposable`.
  Move to a singleton (`SocketsHttpHandler` shared) or
  `IHttpClientFactory` if DI is added.
- **E2. SSE per-subscriber queue is unbounded.** `http_api.py:343-347` does
  `await queue.get()` with no max size enforcement on the producing
  side. Confirm `BridgeEventBus.subscribe` bounds the queue per
  subscriber; if not, a slow client grows bridge memory without limit.
- **E3. SSE has no heartbeat/keepalive.** Stream timeout is
  `Timeout.InfiniteTimeSpan` by design (`BridgeHttpClientPolicy.cs:16`),
  but there is no application-layer ping (`:keepalive\n\n` comment per
  the SSE spec). If TCP silently dies (NAT timeout, mid-network
  failure), the client never knows. Add a 15s `:ping\n\n` from the
  server side and a client-side last-event-received-at watchdog.

### F. UX / observability

- **F1. `/api/llm/launch` noop response is ambiguous.**
  `http_api.py:295-297` returns `{ok: true, noop: true}` when status is
  already `connected/connecting`. UI cannot tell pre-existing from new.
  Return a distinct status code (208 Already Reported) or a clear
  `state: "already_running"`.
- **F2. SSE resume-from-id is silently dropped.** `http_api.py:248`
  parses the URL but only `parsed.path` is used; `?last_id=N` query
  arg the SSE spec defines for resume is ignored. Honor it.
- **F3. No `/healthz`.** Trivial unauthenticated cheap liveness probe is
  missing; both the WPF UI's bridge-status chip and any external
  monitor would benefit.

### G. Test coverage

- **G1. `SessionSupervisor` has no integration tests** covering the
  full state machine (validate → launch → health-gate → inject →
  bridge → running). All current tests target leaves of the pipeline.
- **G2. `bridge/http_api.py` has no auth / CORS / endpoint contract
  tests.** Add `bridge/tests/test_http_api.py` exercising each route
  including malformed input, oversized Content-Length, unauthorized
  requests once A1 lands.
- **G3. The broker path has no tests.** `tools/inject_broker.ps1` /
  `inject_via_broker.ps1` (parent repo) are untested. Add a smoke test
  that mocks `injector.exe`, drops a synthetic request, and verifies
  the done/exit/out files appear within the timeout.

---

## User-Value Findings (operator-perspective review, 2026-05-23)

Companion to the code-review findings above. Where the previous pass asked
"is this code production-quality?", this pass asks "if I'm the bot
operator sitting in front of this thing, can I actually do my job?" The
operator runs 5 GW lanes (BEASTRIT / DISCOPANIC / BLUMPKINS / MARVIN /
BISCUIT), wants gold/loot per unit time, and shares the machine with
multiple coding agents. The UI today is
infrastructure-shaped — it exposes the *plumbing* (PID, DLL, pipe, build,
heartbeat) but not the *outcomes* (gold/hour, what dropped, what just
broke, what to do about it).

### Workflow gaps observed

- **Run history is ephemeral.** Monitoring shows current Runs/Successes/
  Failures/Best/Avg as in-memory counters but there is no persisted run
  log. After a UI restart the entire history is gone.
- **Drop accounting is implicit.** Gold/Materials/Loot fields exist on
  the Monitoring tab but read as "n/a" until a snapshot fires; there is
  no per-item drop ledger, no rare-drop highlight, no daily aggregate,
  no "what did Froggy HM net per hour this week."
- **No fleet view.** To compare lanes the operator clicks each one in
  the rail. A glanceable tile grid with all five lanes (state, current
  phase, last-5 outcomes, gold/hour, alerts) would replace 5 clicks
  with 0.
- **No quick-launch.** Launching requires: pick Profile → pick Bot →
  pick Character → pick Mode → click Validate → click Launch → confirm.
  For the 95% case ("launch what I ran yesterday") this should be one
  button.
- **No stop conditions.** Operator must babysit. "Stop after N runs",
  "stop at HH:MM", "stop on rare drop", "stop if fail rate >X%" are
  table stakes for unattended farming.
- **No notifications.** Run completion, rare drop, failure cluster,
  stop-condition fired — all silent. Windows toast / Discord webhook /
  system tray would be obvious wins.
- **DLL build mismatch is silent.** The registry record carries
  `dll_build_hash` but the rail shows "unknown build" with no comparison
  against the operator's expected/local build. A wrong DLL is a real
  hazard (memory layout changes break everything) and currently the UI
  only finds out by crashing.
- **No crash / disconnect recovery affordance.** When GW disconnects
  (common during long runs), the operator restarts manually. No
  auto-reinject path, no "resume my last session" button.
- **Profile editing has no dirty / diff state.** Changing a field then
  navigating away does not warn; there is no "you've changed 3 settings
  since last save" indicator; no Revert button; no Save As.
- **Profile portability is broken.** Three profile JSONs exist
  (`00-froggy-hm-beastrit-qwen-safe.json`, `01-froggy-hm-disco-qwen-safe.json`,
  `froggy-hm.default.json`) all encoding FroggyHM. Adding a new lane
  requires copying a JSON by hand. No Import / Export / Duplicate / Save
  As workflow in the UI.
- **Bot-module discovery is hardcoded to FroggyHM.** The native DLL
  registry of bot modules (FroggyHM, RragarsMenagerie, RavensPoint,
  ArachnisHaunt, Kathandrax, FrostmawsBurrows, etc.) is not surfaced —
  the Bot combo offers only what the profile JSON named.
- **LLM Interface lacks operator intervention.** Chat is one-shot
  message in; the LLM's plan is read-only display; there is no "edit
  the plan", "skip this step", "veto this action", or "pause and let me
  drive" affordance.
- **No keyboard shortcuts.** No documented chord for Launch / Stop /
  switch lane / clear logs.
- **No in-app help.** Field labels are terse ("Conset Crafting",
  "Maintenance limits", "Bot phase") with no tooltips explaining what
  they do for someone who didn't write the code.
- **No session bundle export.** For bug reports or multi-agent
  coordination, "export the current session as a zip (logs + profile +
  last snapshot + screenshots)" would be invaluable. Currently the
  Open log folder button drops you into a directory.
- **Onboarding is implicit.** First-run experience: open the app, see
  cryptic placeholder values, no guided "set up your first lane" flow.

### Themes for the backlog

- **Outcomes over plumbing.** Add views that answer "what is this
  earning me?" before refining views that answer "what is the PID?"
- **Unattended operation.** Stop conditions + notifications + crash
  recovery.
- **Fleet first.** A multi-lane dashboard is the natural top-level
  page; the per-session detail is the second level.
- **Editing as a first-class action.** Profile diff, dirty state,
  save / save-as / revert, validation actionable not informational.
- **LLM as a collaborator.** Plan editing and operator-in-the-loop
  controls, not a read-only console.

The backlog items below (18+) lift these into concrete, evidence-bound
work units.

---

## DLL-Surface Findings (controls/statuses available but not exposed, 2026-05-23)

A third deep-dive pass, this time inventorying the native DLL's state-
and-control surface against what the UI actually shows. The TL;DR: the
DLL has 22 managers and 6 bot modules; the UI surfaces a thin slice of
one of them. There is a large, cheap inventory of "expose this state /
add this control" work the UI can pick up without touching game-side
logic.

### A. Untapped live state (read paths)

The DLL maintains all of this in-process every tick; the UI sees almost
none of it.

| Manager | State held | UI surface today |
|---|---|---|
| `PlayerMgr` | HP/energy/position/map/active title | none (log scrape only) |
| `PartyMgr` | hero list, hero builds, party leader | none |
| `SkillMgr` | 8-slot skill bar, cooldowns, recharge, last cast | none |
| `EffectMgr` | active buffs/debuffs on player + party | none |
| `AgentMgr` | live agents with type/HP/distance/name | none (no mini-map) |
| `DialogMgr` | current NPC dialog text + options | none |
| `QuestMgr` | active quests, current objective | none |
| `ItemMgr` | inventory bags with stacks + mods | indirect via maintenance rules only |
| `ChatLogMgr` / `ChatMgr` | incoming whispers + party + all chat | none (no chat panel) |
| `FriendListMgr` / `GuildMgr` | friends online, guild state | none |
| `MapMgr` / `TravelMgr` | current outpost / explorable / travel state | partial ("Dungeon" label only) |
| `MerchantMgr` | last merchant interaction, prices | none |
| `TradeMgr` | trade window state | partial (Kamadan trading is a separate WIP) |
| `CombatMgr` | current target, combat state, aggro | none |

Each of these is a candidate "live state" panel. Most need one new IPC
verb (e.g. `get_player_state`, `get_skillbar`, `get_agents_nearby`) and a
periodic snapshot event on the bridge bus.

### B. Untapped controls (write paths)

The DLL exposes these operations but the UI cannot invoke them directly.

| Manager | Control | Why it matters in the UI |
|---|---|---|
| `TravelMgr` | travel to outpost / move-to coords | "send the bot to Kamadan" / "go to my position" |
| `SkillMgr` / `SkillCombat` | cast skill on target | operator-driven combat assists |
| `CombatMgr` | set target / attack / disengage | nudge target priority without editing the bot |
| `ChatMgr` | send chat message | reply to a whisper from the UI |
| `MerchantMgr` | buy / sell | operator-overridden vendor flow |
| `TradeMgr` | open trade with player by name | trading lane workflows |
| `PlayerMgr` | set active title | quick title switching for farms |
| `DialogMgr` | choose dialog option | resolve stuck NPC interactions |
| `QuestMgr` | accept / decline / abandon quest | recover from stuck quest state |
| bot framework | pause / resume / stop / step / re-run | unattended-mode controls beyond just Stop |

### C. IPC vocabulary is too small

The full IPC verb set today: `hello`, `whoami`, `action`, `action_result`,
`get_settings`, `set_settings`. Every state read goes through the
generic `action` request. Bridge bus events: `bridge.status`,
`chat.assistant`, `chat.user`, `degradation`, `disconnect_detected`,
`llm.telemetry`, `message`, `plan.updated`, `run.summary`,
`snapshot.summary`, `tool.call`, `tool.result`.

There is no event for: drop received, item picked up, agent spotted
(rare/named mob), skill cast, effect applied/removed, dialog shown,
quest updated, party member died, inventory changed. Each of these is
a one-line `event_bus.emit` away from being subscribable.

### D. Bot-module coverage is wildly uneven

Built bot modules: `arachnis_haunt`, `froggy`, `frostmaws_burrows`,
`kathandrax`, `ravens_point`, `rragars_menagerie`. Profile JSONs in
`ui/profiles/defaults/`:
- `00-froggy-hm-beastrit-qwen-safe.json`
- `01-froggy-hm-disco-qwen-safe.json`
- `froggy-hm.default.json`

**Five of six bots are UI-invisible.** The Bot combo box reads from
profile JSONs, so the operator cannot launch Kathandrax / Frostmaws /
Ravens / Arachnis / Rragars from the UI even though the native code is
built. The bot-module-discovery item (#27) starts to fix this; the
catalog needs companion per-bot default profiles and per-bot dashboard
templates (#37, #38 below).

### E. MaintenanceMgr config is mostly hidden

`MaintenanceMgr::Config` has 20+ tunable knobs:
`minFreeSlots`, `minIdKits`, `minSalvageKits`, `targetIdKits`,
`targetSalvageKits`, `targetExpertSalvageKits`, `maxCharacterGold`,
`maintenanceTown`, `xunlaiChestX/Y`, `materialTraderX/Y`,
`materialTraderPlayerNumber`, `depositKeepOnChar`,
`depositWhenCharacterGoldAtLeast`, `consetStorageGoldTrigger`,
`consetStorageGoldFloor`, `targetStoredConsetsEach`,
`targetCharacterConsetsEach`, `consetBatchSets`,
`consetWithdrawGoldTarget`, `consetMaterialStackTrigger`,
`consetMaterialPressureFreeSlots`, `enableConsetRestock`,
`salvageMatchedUpgradesBeforeMaterials`.

The Maintenance tab in the UI surfaces a subset; many are profile-JSON-
only. There is no UI workflow to add a new maintenance town (with its
own Xunlai/material-trader coordinates), so the bot is effectively
locked to whichever towns came with the default profiles.

### F. UX maturity beyond data exposure

Things that aren't about "show more state" but about how the operator
works:

- **No mini-map** — `AgentMgr` + `PlayerMgr` have the data; the Py4GW
  precedent shows it; backlog has long mentioned a route/debug panel
  but it has not been built.
- **No live skill bar overlay** — operators currently squint at the GW
  window to see which skills are recharging.
- **No drop toast** — every drop is silent from the UI's perspective.
- **No inventory grid view** — only policy rules, no visual bag.
- **No chat panel** — operator cannot read or send chat from the UI
  while the bot runs.
- **No comparison view** — "this Froggy run vs my last 10" is a manual
  spreadsheet exercise today.
- **No multi-agent coordination dashboard** — `AGENT_WORK_REGISTRY.md`
  and `AGENT_ACCOUNT_REGISTRY.md` are markdown the operator and the
  agents edit by hand. A live view of who-owns-what-claim with a
  timeline would prevent the kind of lane collisions that wedge work
  (the slice-3 elevation block was downstream of one of these).
- **No raw-JSON profile editor fallback** — the fixed-field UI cannot
  edit a profile field the UI doesn't know about. Adding a new
  setting to a bot module means a UI change before operators can
  test it.
- **No per-bot dashboard template** — Froggy has dungeon-level + ETA
  display; the other 5 bots have no dashboard at all.

---

## Compact prompt

```text
Read C:\Users\Robert\Documents\GWA Censured X BotsHub\docs\GWA3_UI_MATURITY_PROMPT.md, then implement in C:\Users\Robert\Documents\gwa3-private on a clean topic branch off codex/ui-ux-polish-followup (03d4661).

Honor GWA3 agent rules (AGENTS.md + registry/build/launcher docs). Coordinate via AGENT_WORK_REGISTRY. MVVM / no-backend-behavior-changes / flat-theme constraints stay in force. Live work uses validated launcher → exact-PID inject → kill-only-your-PID discipline.

Ship these six objectives, each as its own commit (M4 broken further per panel):

M1. Multi-session correctness. Inject two lanes simultaneously (DISCOPANIC + one operator-approved second lane). Verify rail lists both, selecting either updates only its own Monitoring / LLM Interface / Diagnostics, the log collapser de-duplicates per session not across sessions, Stop on session A leaves session B intact, settings probe round-trips against each pipe independently. Screenshot each check; add a smoke test for cross-session collapsing.

M2. End-to-end UI-driven Launch (not cancelled at the confirmation). Drive a clean lane through Launch → GWLauncher → exact-PID capture → health gate → DLL inject → bridge attach → first telemetry. Screenshot each supervisor transition; confirm the resulting session is tagged UI-launched; Stop cleanly with no orphan PID/registry/pipe.

M3. Continuous settings-drift watcher. Promote the slice-2 one-shot get_settings probe into a 5–10s background watcher per selected session. Surface drift as a rail chip ("settings: in sync" / "settings: drift (N)" / "settings: probe unavailable") and a Diagnostics drift table with mismatches sorted first. Rate-limit the IPC. Offline-test the diff logic with matching / numeric mismatch / missing-field / IPC-error fixtures.

M4. Decompose MainWindowViewModel.cs and MainWindow.xaml. Extract per-feature view models (SessionsRail / Monitoring / LlmConsole / ProfileEditor / Validation / LogPane / Diagnostics) and matching UserControls. Shell becomes a thin composition root. Pure restructure — UiSmokeTests must keep passing. One panel per commit so review can follow the seams. Aim for ≥40% line reduction in the shell files.

M5. Degraded-state UX + the HttpClient.Timeout.Infinite bug. Replace Timeout.InfiniteTimeSpan on llmBridgeClient with bounded timeouts + per-request CancellationToken (except the SSE stream). Surface: bridge unreachable / bridge stale (>30s no SSE) / settings-probe unavailable / session ended (gray out, prune after ~60s). Re-trigger each state live and screenshot.

M6. 30-minute soak with UI attached to a live DISCOPANIC bot route. Include one mid-soak re-injection. Report: log-collapser max fan-out, UI working-set growth (target <50 MB), drift-watcher false positives (target zero), any UI freeze >250 ms. End with clean UI Stop and verify no orphan PID/registry JSON/pipe.

Deliverables: topic-branch commits per objective; screenshots under docs/ui_ux_polish_screenshots/slice3/; new offline tests (cross-session collapsing, drift fixtures, bounded-timeout client); live-validation report with PIDs, multi-session evidence, Launch timeline, drift screenshots, degraded-state screenshots, soak metrics; final list of changed files and remaining honest gaps. If M4 runs long, ship the panels extracted cleanly and document which remain in the monolith — partial-but-clean beats a giant unreviewable diff.
```
