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
