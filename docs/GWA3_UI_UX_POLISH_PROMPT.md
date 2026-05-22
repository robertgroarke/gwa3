# GWA3 UI/UX Polish Goal Prompt

A companion to [`GWA3_UI_IMPROVEMENT_GOAL_PROMPT.md`](GWA3_UI_IMPROVEMENT_GOAL_PROMPT.md).
That prompt fixes the *backend* session-discovery gap. **This prompt fixes the
*presentation* layer** — the operator-facing readability, density, and
information-design problems observed by running the desktop UI live.

## How this was scoped

The GWA3 Control Panel (`gwa3-private/ui/Gwa3.UI.App`) was launched and
inspected live with both an idle lane (BLUMPKINS) and an active externally
discovered lane (BEASTRIT, `InDungeon`, live telemetry hydrated). The issues
below are observed defects, not speculation.

```text
Goal: Improve the readability, information density, and operator ergonomics of the GWA3 desktop Control Panel (and bring the web/API surface to the same presentation contract). This is a UI/UX polish pass — no new bot capabilities. Desktop UI is canon.

Start in C:\Users\Robert\Documents\gwa3-private. Before edits, read AGENTS.md and the required registry/build/test/launcher docs from C:\Users\Robert\Documents\GWA Censured X BotsHub. Do not launch Guild Wars, inject DLLs, kill processes, or run live tests unless lane/account/build/DLL/pipe/PID discipline is explicitly satisfied. To validate visual changes, run the already-built UI exe (ui/Gwa3.UI.App/bin/Debug/net8.0-windows/Gwa3.UI.App.exe) — it does not require a live GW client.

Context:
- The UI is a .NET 8 WPF/MVVM app. The shell is MainWindow.xaml (~1,705 lines) bound to a single MainWindowViewModel.cs (~5,560 lines). Styling is a flat light theme defined in Window.Resources.
- Top-level tabs: Configuration / Monitoring / (LLM Interface — appears only when a live LLM session exists). Configuration nests 7 sub-tabs: Launch, Profile, Team/Skills, Inventory, Maintenance, LLM, Logs, Diagnostics.
- A prior backend review exists at docs/UI_REVIEW_AND_RECOMMENDATIONS.md. The session-discovery backend work is tracked separately; do not duplicate it here.

Observed presentation defects (fix these):

1. Pervasive text truncation. Read-only fields (Selected lane → Health, Last event; Status monitor → Last event) and every row in the "Recent launch log" and "Live log tail" clip mid-word with no wrap, no ellipsis, and no tooltip. Operators literally cannot read "...no GWA3 telemetry log is a[ttached]" or the full launch-plan error. Make every value field either wrap, show a full-text tooltip on hover, or both. Log rows must wrap or be horizontally readable without a scrollbar.

2. Log noise drowns signal. The launch log repeated the identical line "Bridge stream reconnecting: No connection could be made..." ~10 times in a row, burying the meaningful INFO lines ("Attached monitoring to PID 11084", "Hydrated live DLL telemetry 11/408 matched"). Collapse consecutive duplicate log lines into a single row with a "×N" repeat count, or rate-limit identical reconnect warnings. Consider a level filter (INFO/WARN/ERROR) and a "jump to latest" / autoscroll toggle.

3. Horizontal scrollbars instead of wrapping. The Validation DataGrid Detail column and the launch-log list both expose a horizontal scrollbar. Detail/message columns should wrap to row height; size columns so the meaningful text is visible without scrolling.

4. Idle Monitoring tab is a wall of "n/a". With no run active, the Monitoring tab shows 20+ "n/a" fields and an empty white "Live log tail" box. Design real empty states: a single clear "No active run — launch a session to see telemetry" placeholder, and collapse or de-emphasize the n/a field groups until data exists.

5. Left "Sessions" rail is dense and low-contrast. Each session card stacks ~8 lines of small gray-on-dark text (module, state, dll, pipe mode, profile source, build, health line) and the status badge clips ("Client R..."). Rework the card: a clear hierarchy (character + state badge prominent), the rest as a compact secondary block or expand-on-hover, badges that never clip, and provenance shown as a small chip ("UI-launched" vs "external").

6. Redundant / unactionable status. "Health: N/2 bots healthy (X%)" is printed twice (header subtitle and footer). Validation ERRORs (e.g. pipe-name mismatch, empty skillbar) are persistent walls of red text with no fix affordance. De-duplicate the health readout; make validation rows actionable (link/expand to the offending field, or a "show me" button).

7. Visual flatness and inconsistent density. Some panels are cramped (rail, validation grid) while others are sparse (idle telemetry). Apply consistent spacing, group headers, and use the existing status color tokens (AccentBrush/WarnBrush/DangerBrush) to make state scannable at a glance. Tabs that appear/disappear ("LLM Interface") are disorienting — prefer a tab that is always present but shows an empty state, or make the appearance obvious.

Constraints:
- MVVM discipline: presentation logic in XAML + converters + view models, not code-behind. This is a good opportunity to extract per-feature view models / UserControls (Sessions, Monitoring, LogPane, Validation) from the monolith — but only as far as the polish requires; full decomposition is the other prompt's secondary objective.
- Do not change backend contracts, IPC, launch/injection logic, or lane safety rules. Presentation only.
- Keep the flat light theme; refine it, don't replace it.
- Web/API: after the desktop changes settle, document the presentation contract (field names, log-collapsing rules, empty-state semantics) so the separate web frontend can match it. Do not build a new web frontend here.

Deliverables:
- A short implementation plan before edits.
- Narrow, reviewable commits; coordinate with AGENT_WORK_REGISTRY if claiming work.
- Before/after screenshots of: idle Monitoring tab, a Configuration/Launch tab with validation errors, and the launch log with repeated reconnect lines.
- New or extended UI smoke tests where converters/collapsing logic are pure and testable (duplicate-line collapsing, truncation/tooltip behavior, empty-state selection).
- Final report: changed files, screenshots, remaining gaps.

Success criteria:
- No value field or log line clips text without a wrap or hover tooltip.
- Repeated identical log lines collapse to one row with a count.
- No horizontal scrollbar is needed to read a Validation Detail or a log message.
- The idle Monitoring tab reads as a deliberate empty state, not a broken wall of "n/a".
- The Sessions rail communicates character, state, and provenance at a glance without clipping.
- The health readout appears once; validation errors point the operator at what to fix.
```

## Compact prompt

```text
Read C:\Users\Robert\Documents\GWA Censured X BotsHub\docs\GWA3_UI_UX_POLISH_PROMPT.md, then implement the first slice in C:\Users\Robert\Documents\gwa3-private.

Honor GWA3 agent rules first (AGENTS.md + registry/build/launcher docs). No GW launch/injection needed — validate by running the prebuilt UI exe.

This is a presentation-only polish pass on the WPF Control Panel. Fix, in priority order:
1. Text truncation: every clipped value field and log row gets wrap and/or a full-text hover tooltip.
2. Log noise: collapse consecutive duplicate log lines into one row with a "×N" count; add INFO/WARN/ERROR filtering.
3. Replace horizontal scrollbars (Validation Detail column, launch log) with wrapping.
4. Real empty states for the idle Monitoring tab instead of a wall of "n/a".
5. Rework the dense, clipping-badge Sessions rail cards into a clear hierarchy with non-clipping state + provenance badges.
6. De-duplicate the doubled health readout; make Validation errors point at the field to fix.

Keep MVVM discipline (converters/view models, not code-behind) and the existing flat light theme. No backend/IPC/launch changes. Produce a short plan, before/after screenshots, smoke tests for the pure collapsing/empty-state logic, and a final report.
```
