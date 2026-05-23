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
