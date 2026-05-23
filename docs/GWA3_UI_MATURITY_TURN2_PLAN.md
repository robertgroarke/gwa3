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
