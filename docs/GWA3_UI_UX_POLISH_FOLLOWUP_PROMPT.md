# GWA3 UI/UX Polish — Follow-up Prompt

Follow-up to [`GWA3_UI_UX_POLISH_PROMPT.md`](GWA3_UI_UX_POLISH_PROMPT.md).

## What the first pass delivered (verified)

Codex shipped the first slice and it builds + tests pass (15/15) + cmake disco
build passed + DISCOPANIC PID 4460 was launched, observed, and cleaned up. The
visible wins on the live UI:

- Sessions rail cards reworked into a clean header (character + state badge),
  provenance/launch-mode chips, and a tidy `DLL / Pipe / Build / Health` grid.
  Badges no longer clip.
- Footer no longer double-prints `Health: N/2 bots healthy`.
- Launch/runtime/Logs panes render with `INFO / WARN / ERROR` checkboxes,
  duplicate-row collapsing with `×N` suffixes, no more horizontal scrollbars.
- Validation rows wrap their Detail and show a `FixHint` line.
- New `LogPresentation` + `MonitoringPresentation` pure helpers with unit tests.

## What the first pass missed (fix these)

Verified by re-running the UI and reading the produced diff
(`ui/Gwa3.UI.App/MainWindow.xaml`, `ui/Gwa3.UI.App/ViewModels/MainWindowViewModel.cs`,
`ui/Gwa3.UI.Core/Models/{Log,Monitoring}Presentation.cs`), plus Codex's own
closeout listing four "remaining gaps".

```text
Goal: Close the four gaps Codex flagged on the first WPF polish slice and fix three regressions/defects observed by running the rebuilt UI. Same lane discipline as before; same MVVM/no-backend-changes constraints.

Start in C:\Users\Robert\Documents\gwa3-private. Read AGENTS.md and the registry/build/launcher docs first. Coordinate via AGENT_WORK_REGISTRY before claiming; release when done.

The first-pass change set is currently UNCOMMITTED in the worktree, mixed in with unrelated pre-existing edits to MainWindowViewModel.cs (session-discovery work). Before editing:
- Create a clean topic branch.
- Restage only the first-pass UI polish files (MainWindow.xaml, MainWindowViewModel.cs, StatusBrushConverter.cs, LogPresentation.cs, MonitoringPresentation.cs, UiSmokeTests.cs, plus the screenshot folder under docs/ui_ux_polish_screenshots/) into a first commit on that branch, so this follow-up sits on a clean base. Coordinate with the operator if pre-existing edits in MainWindowViewModel.cs belong to a different in-flight claim — do not silently sweep them in.

Defects to fix (observed on the rebuilt UI):

D1. The Monitoring "empty state" is additive, not exclusive. When a session is idle, the UI now shows the "Status monitor" idle table (State / Character / Dungeon / Progress / Health / Last event / DLL / Pipe) AND the new "Monitoring is idle" card AND a giant blank "Live log tail" box, stacked. The empty state was supposed to REPLACE the placeholder telemetry blocks, not append to them. When SelectedSession.IsMonitoringIdle is true, collapse or hide Status monitor, Live telemetry, and Live log tail; render only the empty-state card with the minimum identifying info (character, lane, pipe). Add a smoke test asserting only one of those surfaces is visible per state.

D2. Global TextBox style sets ToolTip = self.Text. Every editable TextBox in the app now shows its own value as a tooltip on hover — including profile fields, search boxes, the chat input. That is wrong for editable inputs. Move the ToolTip = self.Text setter off the base TextBox style and onto a new read-only `ValueText` / `ReadOnlyText` style (or onto the existing WrappedValueText style only), then apply that style to display-only TextBlocks/TextBoxes. Editable inputs should not auto-tooltip their own contents.

D3. ValidationCheckViewModel.FixHint emits "No action needed." for every passing row. That is visual noise on rows that already show OK. Return null/empty for passing rows and bind the FixHint TextBlock's Visibility to a non-empty string converter so the row collapses cleanly.

Gaps from Codex's closeout (to actually deliver):

G1. Profile setting propagation into the live DLL is NOT proven. This was a primary success criterion of the original prompt. Codex hit the wall because the bridge `/api/llm/state` endpoint was unavailable and a direct pipe probe was blocked. Close this properly:
   - Define and ship a minimal IPC `get_settings` (or extend `whoami`) request on the DLL pipe that returns the runtime-effective values of the settings the UI claims to control: hard_mode, use_consets, open_chests, pickup_gold, auto_salvage, inventory/maintenance limits, LLM autonomy/allowed_actions, etc. One native-call-per-tick rule still applies — read from already-cached config, not from a new game call.
   - On the UI side, add a "Probe live settings" diagnostic in the Diagnostics tab that connects to the selected session's pipe, fires `get_settings`, and renders a side-by-side table of UI profile value vs DLL-effective value, with a green check / red cross per row.
   - Until a probe round-trip confirms a value, the UI must NOT claim a setting "applied" or "saved to DLL"; show "saved locally — unconfirmed in DLL" instead.
   - Validate live on DISCOPANIC: toggle hard_mode, then open_chests, then pickup_gold, run the probe each time, confirm the DLL-effective value changes to match. Screenshot each round-trip.

G2. Destructive controls (Launch / Dry Run / Stop) were not exercised live because Codex was unsure whether they would mutate or stop the wrong UI-managed state. Fix the underlying ambiguity:
   - Tag every session as `UI-launched` or `externally discovered` (the rail already shows this).
   - On Stop / Launch / Dry Run, check the selected session's provenance. If it is `externally discovered`, surface a confirmation dialog naming the PID and DLL and require explicit confirmation. Stop on an externally discovered session must terminate ONLY that PID and never touch others.
   - With the safety in place, exercise each destructive button against the live DISCOPANIC session at least once and capture the result in the live-validation checklist. Then verify the corresponding UI status fields update correctly afterwards (Status, PID, Heartbeat, Sessions rail badge).

G3. IPC `whoami` / `get_identity` probe did not complete live. Either:
   - implement it as part of G1 above and use the same probe path for identity (preferred), OR
   - if you elect to defer it, explicitly document it as a known gap in docs/UI_REVIEW_AND_RECOMMENDATIONS.md and adjust the Sessions rail "live" indicator to honestly state "heartbeat: file mtime only" until pipe whoami exists.

G4. LLM Interface panel is still profile-scoped, not session-scoped, for externally discovered LLM sessions. The panel content (Bridge status, observers, plan phase, last tool calls, run summaries) must read from the SELECTED session's pipe/bridge, not from the currently chosen profile in the top combo. Audit every binding under the `LLM Interface` TabItem, repoint each to a SessionViewModel property (already partially modelled), and make the panel react to SelectedSession changes. Verify by selecting BLUMPKINS vs DISCOPANIC in the rail and confirming the LLM Interface content changes accordingly.

Live validation on the DISCOPANIC lane (REQUIRED, same launcher discipline as the original prompt):

- Account index 1, character `D I S C O P A N I C`, build dir `gwa3/build_disco`, DLL `gwa3_disco.dll`, pipe `\\.\pipe\gwa3_llm_disco`, launcher `GWA Censured/debug_scripts/launch_disco_panic_via_gwlauncher.au3`. Launch via the AutoIt launcher, wait ~30s, inject only the exact returned PID. Kill only that PID on cleanup.
- Coordinate with AGENT_WORK_REGISTRY; DISCOPANIC is the Claude Code agent's lane in normal operation, so claim, do the work, release.
- Run the G1 setting round-trip three times with three different settings.
- Run the G2 destructive-control checks (Stop on UI-launched, Stop on externally discovered with confirmation, Launch from clean UI, Dry Run preview).
- Re-screenshot the idle Monitoring tab after D1 (should show ONE clean empty-state card, not three stacked surfaces) and a passing Validation table after D3 (no "No action needed." chrome).
- Resize narrow/wide and confirm D2's editable-textbox tooltips are gone.

Deliverables:
- A clean topic-branch commit chain (one commit isolating the first-pass polish from prior dirty edits, then commits per defect/gap).
- Updated screenshots under docs/ui_ux_polish_screenshots/after/ that supersede the affected first-pass shots.
- Smoke tests for: exclusive-empty-state, suppressed FixHint on passing rows, and (offline harness) the get_settings IPC round-trip parser.
- A live-validation report appended to the existing report with: PID used, settings probed before/after with screenshots, destructive-control results, LLM Interface session-scoping verification.
- Final list of changed files + remaining gaps (be honest — if G1 cannot be shipped this slice, say so and ship D1–D3 + G2 + G4 cleanly rather than papering over G1).

Success criteria:
- An idle Monitoring tab renders exactly one empty-state surface.
- Editable TextBoxes no longer show their own contents as a hover tooltip.
- Passing Validation rows do not say "No action needed."
- Toggling a profile setting and clicking Save → running "Probe live settings" returns a row of green checks against the live DISCOPANIC DLL.
- Stop / Launch / Dry Run have been clicked against a live DISCOPANIC session and confirmed to affect only the intended PID.
- LLM Interface content changes when the rail selection changes between two sessions.
- The first-pass UI polish work is on a clean topic-branch commit that does not include unrelated MainWindowViewModel.cs edits.
```

## Compact prompt

```text
Read C:\Users\Robert\Documents\GWA Censured X BotsHub\docs\GWA3_UI_UX_POLISH_FOLLOWUP_PROMPT.md, then implement in C:\Users\Robert\Documents\gwa3-private on a clean topic branch.

Honor GWA3 agent rules (AGENTS.md + registry/build/launcher docs). Coordinate via AGENT_WORK_REGISTRY. Same MVVM / no-backend / flat-theme constraints as the first pass.

First, restage the first-pass UI polish files onto a clean topic-branch commit, separated from the unrelated pre-existing MainWindowViewModel.cs edits already in the worktree.

Then fix these defects observed on the rebuilt UI:
- D1: idle Monitoring shows three stacked surfaces (Status monitor + "Monitoring is idle" + Live log tail). Make the empty state EXCLUSIVE — hide the placeholder telemetry blocks when SelectedSession.IsMonitoringIdle.
- D2: the global TextBox style sets ToolTip = self.Text, so editable inputs auto-tooltip their own value. Move the auto-tooltip onto a read-only/display style only.
- D3: ValidationCheckViewModel.FixHint emits "No action needed." for every passing row. Suppress FixHint on passing rows and collapse the TextBlock when empty.

Then close the four "remaining gaps" Codex flagged at closeout:
- G1: prove profile setting propagation to the live DLL. Ship a minimal `get_settings` (or extended `whoami`) IPC on the DLL pipe that returns runtime-effective values, add a "Probe live settings" diagnostic that shows UI-vs-DLL side by side, and refuse to label a setting "applied" until the probe round-trips green. Validate on DISCOPANIC with three settings.
- G2: exercise Launch / Dry Run / Stop live on DISCOPANIC. First make Stop on externally-discovered sessions require a PID-confirmation dialog and never touch other PIDs.
- G3: implement the whoami/get_identity probe (preferably as part of G1's IPC), or document the deferral and downgrade the rail "live" indicator to honestly say "heartbeat: file mtime only".
- G4: make the LLM Interface panel session-scoped for externally discovered LLM sessions instead of profile-scoped. Confirm by switching rail selection between two sessions.

Live validation on DISCOPANIC (build_disco / gwa3_disco.dll / \\.\pipe\gwa3_llm_disco / launcher launch_disco_panic_via_gwlauncher.au3) — launch via AutoIt, inject only the exact returned PID, kill only that PID. Re-screenshot idle Monitoring, capture each G1 round-trip, capture G2 destructive results, and verify G4 session-scope.

Deliverables: clean commit chain, updated screenshots, smoke tests for exclusive-empty-state / suppressed FixHint / get_settings parser, live-validation report. Be honest about any gap that cannot be closed this slice.
```
