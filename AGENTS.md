# AGENTS.md

This file defines the minimum operating rules for coding agents working in this repository.

## Required Reading

Before making changes, launching Guild Wars, building `gwa3`, injecting DLLs, or running bridge tests, read these files:

1. [AGENT_ACCOUNT_REGISTRY.md](C:\Users\Robert\Documents\GWA Censured X BotsHub\AGENT_ACCOUNT_REGISTRY.md)
2. [AGENT_WORK_REGISTRY.md](C:\Users\Robert\Documents\GWA Censured X BotsHub\AGENT_WORK_REGISTRY.md)
3. [MULTI_AGENT_BUILD_ARCHITECTURE.md](C:\Users\Robert\Documents\GWA Censured X BotsHub\MULTI_AGENT_BUILD_ARCHITECTURE.md)
4. [gwa3/bridge/tests/TEST_EXECUTION_GUIDE.md](C:\Users\Robert\Documents\GWA Censured X BotsHub\gwa3\bridge\tests\TEST_EXECUTION_GUIDE.md)
5. [GW_Launch_Method.md](C:\Users\Robert\Documents\GWA Censured X BotsHub\GW_Launch_Method.md)

Do not skip these reads. They contain the current account ownership, task ownership, multi-agent build lanes, test execution constraints, and launcher rules.

## Approved Reference Sources

Agents are expected to use local reference material in this repo when investigating behavior, restoring functionality, comparing implementations, or porting logic.

Primary reference locations:

- local BotsHub repo:
  - `C:\Users\Robert\Documents\GWA Censured X BotsHub\BotsHub-master`
- upstream or newer BotsHub snapshot:
  - `C:\Users\Robert\Documents\GWA Censured X BotsHub\BotsHub-latest`
- original BotsHub baseline:
  - `C:\Users\Robert\Documents\GWA Censured X BotsHub\BotsHub-original`
- local GWA Censured / Froggy code:
  - `C:\Users\Robert\Documents\GWA Censured X BotsHub\GWA Censured`
- local GWToolbox source code:
  - `C:\Users\Robert\Documents\GWA Censured X BotsHub\toolbox`
  - `C:\Users\Robert\Documents\GWA Censured X BotsHub\GWA Censured\GWToolboxpp`
- local GWCA material:
  - `C:\Users\Robert\Documents\GWA Censured X BotsHub\gwca`
  - `C:\Users\Robert\Documents\GWA Censured X BotsHub\GWA Censured\GWCA-master`
- GWCA disassembly research:
  - `C:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research`

Use these sources to:

- compare current behavior against prior working implementations
- recover packet layouts, offsets, launch patterns, or helper logic
- cross-check Froggy logic against BotsHub logic
- inspect GWToolbox or GWCA patterns before inventing new low-level behavior
- validate assumptions with the disassembly research before changing memory-facing code

Do not assume the current implementation is the only source of truth when this repo already contains a better historical or research reference.

## GWA3 Public/Private Repo Structure

`C:\Users\Robert\Documents\gwa3-private` is the full private source-of-truth for GWA3 development. Make normal implementation, refactor, test, harness, lane-script, and local documentation changes there first unless the user explicitly asks you to maintain the public export only.

`C:\Users\Robert\Documents\gwa3-refactor` is the future-public export branch for `robertgroarke/gwa3`. Treat it as a filtered copy of the private repo, not as the primary development repo. It must contain only durable public source, public headers, bot/runtime code, bridge code, build metadata, small public tools, and user-facing documentation.

Never merge or copy `gwa3-private` wholesale into the public repo. Export only the public-safe subset. Do not track `tests/`, `src/tests/`, `bridge/tests/`, private harnesses, character names, account-specific scripts, lane presets, local run logs, private docs, or agent execution guides in the public repo.

The shared project layout is:

- `include/gwa3/` and `src/gwa3/`: core GWA3 headers and implementation
- `include/gwa3/dungeon/` and `src/gwa3/dungeon/`: reusable dungeon support library
- `bots/`: concrete bot implementations built on GWA3 and dungeon support
- `bridge/`: public Python LLM bridge client
- `tools/`: public injector and small support utilities

Private-only additions in `gwa3-private` include:

- `bridge/tests/`, `src/tests/`, and `tests/`: live, integration, and local validation harnesses
- `docs/`: private notes, plans, debug reports, and agent-facing records
- private CMake presets/lists and lane scripts needed for local validation

## Non-Negotiable Rules

- Always launch Guild Wars through the validated launcher path described in `GW_Launch_Method.md`.
- Never start `Gw.exe` directly.
- Never inject into a guessed PID. Inject only the exact PID returned by the launcher flow.
- Never share one mutable build directory between concurrent agents.
- Never share one DLL name between concurrent agents.
- Never share one bridge pipe name between concurrent agents.
- Treat launcher failures separately from DLL, injector, bridge, or test failures.
- For live in-game debugging or harness work, prefer Asia/Japan district `99`, and fall back to Asia/Japan district `1` if the preferred district does not load cleanly.
- Do not use America English districts for live bot-sensitive debugging unless the task explicitly requires that locale or district.

## Engine Hook / Native Function Rules

The `gwa3` DLL hooks the game's engine tick to dispatch queued commands (Move, ChangeTarget, UseSkill). These rules prevent crashes and deadlocks:

- **One native call per engine tick.** Never call two native game functions (e.g. Move + ChangeTarget) within the same engine hook callback or GameCommand. The game's state machine expects at most one action per tick. Calling two in the same tick causes a deadlock/hang ("Not Responding").
- **ChangeTarget crashes during active movement.** The native `ChangeTarget` function accesses movement state that is unsafe to read while the character is walking. Upstream BotsHub avoids this by waiting for movement to complete before changing target. Our code wraps the call in SEH as a safety net.
- **UseSkill via native call crashes from the engine hook.** Use the packet path (`CtoS::UseSkill` header 0x46) instead. Move is the only native call proven safe on the engine command lane.
- **`TARGET_AGENT` (0xC1) is NOT a valid ChangeTarget packet.** Upstream only changes target via the native command queue, never packets. Sending 0xC1 as a packet fails silently and can crash the game.
- **`Offsets::Environment` must be dereferenced.** It's a `PatternType::Ptr` scan result pointing to a code address (an `ADD EAX, imm32` operand). Read the value at that address to get the actual environment array base.

## Multi-Agent Expectations

If more than one agent is active in this repo at the same time:

- use an isolated lane per agent
- keep build outputs, DLL names, and pipe names unique per lane
- do not kill or interfere with another agent's Guild Wars client
- do not assume the newest `Gw.exe` process belongs to you

Preferred lanes are documented in `MULTI_AGENT_BUILD_ARCHITECTURE.md`.
Account ownership and claim status are documented in `AGENT_ACCOUNT_REGISTRY.md`.
Task ownership and current work claims are documented in `AGENT_WORK_REGISTRY.md`.

`AGENT_WORK_REGISTRY.md` includes a structured `Lane` column. Legal values are
`none`, `beastrit`, `disco`, `blumpkins`, `marvin`, `biscuit`, and `any`.
Use `none` when a work row does not touch a GW lane, use the matching lane when
the work is tied to a specific account/build/DLL/pipe, and use `any` only when
the work can safely run against any lane without exclusive ownership.

`AGENT_WORK_REGISTRY.md` also includes a `Heartbeat` column containing a UTC
ISO8601 timestamp. Agents must refresh that timestamp each turn they touch a
claim. Run `python scripts/agent_work_registry.py prune-stale` to release
`active` or `blocked` rows whose heartbeat is older than the default 15-minute
threshold; malformed heartbeat values are treated as stale.

Use `python scripts/agent_work_registry.py clean-orphan-sessions` after a UI
crash, forced DLL unload, or killed Guild Wars process leaves stale
`%PROGRAMDATA%\gwa3\sessions\*.json` records behind. It uses the same session
directory as `check`, treats dead or unknown PIDs with heartbeats older than the
default 5-minute threshold as orphans, and is safe to run while live sessions are
active because records with running PIDs are preserved. Elevated PowerShell is
only needed if a particular JSON was written by an elevated DLL process.

Add `--periodic <seconds>` to leave orphan-session cleanup running as a side
task in a background terminal, or schedule the same command with Windows Task
Scheduler, so the DLL session registry stays clean after crashes or forced
process exits.

Use `python scripts/agent_work_registry.py claim <work-area> --lane <lane> --owner <name>`
and `python scripts/agent_work_registry.py release <work-area> --owner <name>` for
work claims. Use `python scripts/agent_work_registry.py touch <work-area> --owner <name>`
to refresh a held claim's heartbeat without changing any other row fields. The
CLI takes a file lock on `AGENT_WORK_REGISTRY.md`, refuses non-`available` rows,
and refuses lane collisions with existing `active` or `blocked` rows unless the
requested lane is `none`.

`claim` also warns when the requested `Primary Files` exactly overlap an existing
`active` or `blocked` row after comma-splitting and case-folding the file list.
Use `--strict-files` when that advisory overlap should fail the claim instead.

Use `python scripts/agent_work_registry.py check` to compare active work-registry
lanes with the live DLL self-registration files in
`%PROGRAMDATA%\gwa3\sessions\*.json`. Set `GWA3_SESSION_REGISTRY_DIR` to override
the session directory for tests. The check exits non-zero on mismatches unless
`--warn-only` is supplied.

Use `python scripts/agent_work_registry.py status` for a compact snapshot of
current work claims. Add `--lane <lane>` to filter by lane, or `--watch` for a
5-second refreshing terminal view.

Install claim-check pre-commit hooks with `python scripts/install_agent_hooks.py`.
The hook runs `agent_work_registry.py check --warn-only` and then verifies staged
files with `agent_work_registry.py whoami-files --files-from-stdin`. It blocks
commits whose staged file list is not covered by active `Primary Files` entries.
When that coverage check passes, the hook best-effort refreshes the matching
active claim heartbeats. Use `git commit --no-verify` only as an explicit opt-out.

The installer also adds a post-commit breadcrumb hook. Each successful commit
appends timestamp, short SHA, branch, and subject to
`%LOCALAPPDATA%\gwa3-agent-coord\landings.log`; set
`GWA3_AGENT_LANDINGS_LOG` to override the path. Use
`python scripts/agent_work_registry.py landings --since-minutes 60` before
starting work to see recent parallel-agent landings.

## Worktree Workflow

Use `python scripts/agent_workspace.py` from this parent repo to provision and
inspect isolated `gwa3-private` worktrees. By default it manages
`C:\Users\Robert\Documents\gwa3-private` and uses lane paths like
`C:\Users\Robert\Documents\gwa3-<lane>`.

Examples:

```powershell
python scripts/agent_workspace.py provision --lane disco --branch codex/disco-work
python scripts/agent_workspace.py list
python scripts/agent_workspace.py prune --dry-run
python scripts/agent_workspace.py prune --apply
```

Only use the worktree path, branch, build directory, DLL name, and pipe name
that match the claimed lane. Prune first with `--dry-run`; use `--apply` only
after confirming the listed worktrees are safe to remove.

### Line Endings

The root `.gitattributes` file is the source of truth for line endings. Agents
must not bulk-renormalize files while making unrelated changes; keep commits
limited to the requested logic so diffs stay reviewable.

## Test And Build Discipline

Before reporting any test or bridge regression:

1. Confirm you used the correct launcher-based account flow.
2. Confirm you built in the correct isolated lane.
3. Confirm your DLL name and pipe name match that lane.
4. Confirm the launcher-returned PID reached a healthy loaded state before injection.
5. Confirm you ran the relevant test tier for the current game state.
6. If the test does not require a public or populated outpost, run it in a rarely used map and district to reduce visibility in-game.
7. Default quiet district policy is Asia/Japan district `99`, fallback Asia/Japan district `1`, unless the workflow explicitly needs something else.

If any of those checks were skipped, the result is not reliable.

## Crash Evidence

When the integration or Froggy watchdog detects a crash, hang, or visible crash dialog, it now writes a screenshot path to the log as:

- `WATCHDOG_SCREENSHOT: C:\absolute\path\to\image.bmp`

If you are reporting, summarizing, or handing off a crash investigation in the Codex app:

- find that `WATCHDOG_SCREENSHOT:` line in the active run log
- include the screenshot inline in the active chat window with Markdown image syntax using the absolute path
- do not mention a crash without attaching the watchdog screenshot when one exists

Example:

```md
![Crash screenshot](C:\absolute\path\to\image.bmp)
```

## Default Agent Startup Checklist

At the start of a task:

1. Read the five required files above.
2. Pick or confirm your assigned character/account.
3. Check whether the task area is already claimed in `AGENT_WORK_REGISTRY.md`.
4. Pick or confirm your isolated build lane.
5. Record the launcher-returned PID for your session.
6. Choose a low-traffic map/district for testing whenever the workflow allows it.
7. Prefer Asia/Japan district `99`, fallback Asia/Japan district `1`, for live debug or harness runs unless the task explicitly requires a different district.
8. Use only lane-matching build, DLL, and pipe settings.
9. Update `AGENT_ACCOUNT_REGISTRY.md` if you claim, switch, or release an account.
10. Update `AGENT_WORK_REGISTRY.md` if you claim, switch, or release a work area.
11. Prefer `python scripts/agent_registry.py ...` and `python scripts/agent_work_registry.py ...` over hand-editing the registry tables.

If the task involves `gwa3` launch, injection, bridge work, or tests, follow that checklist before doing anything else.
