# Agent Account Registry

This file is the source of truth for coding-agent account and lane assignment in this repo.

Use it before launching Guild Wars, building `gwa3`, injecting DLLs, or running bridge/player-trade tests.

## Assignment Rules

1. Pick an account marked `available`.
2. Use only the lane paired with that account unless you are explicitly creating a new isolated lane for it.
3. Never reuse another active agent's build dir, DLL name, pipe name, or launcher script.
4. If you claim an account for a task, update this file first.
5. If you release an account, mark it `available` again.

## Status Meanings

- `available`: safe to claim
- `reserved`: intentionally set aside for a specific workflow
- `active`: currently assigned to an agent/session
- `helper-only`: reserved as a trade-harness helper, not a general-purpose lane

## Account And Lane Map

| Account Index | Character | Status | Lane Tag | Build Dir | DLL | Pipe | Launcher Script |
|---|---|---|---|---|---|---|---|
| 0 | `B E A S T R I T` | `active` | `beastrit` | `gwa3/build_beastrit` | `gwa3_beastrit.dll` | `\\.\pipe\gwa3_llm_beastrit` | `GWA Censured/debug_scripts/launch_beastrit_via_gwlauncher.au3` |
| 1 | `D I S C O P A N I C` | `active` | `disco` | `gwa3/build_disco` | `gwa3_disco.dll` | `\\.\pipe\gwa3_llm_disco` | `GWA Censured/debug_scripts/launch_disco_panic_via_gwlauncher.au3` |
| 2 | `B L U M P K I N S` | `active` | `trade-helper` | `gwa3/build_trade` | `gwa3_trade.dll` | `\\.\pipe\gwa3_llm_trade` | `GWA Censured/debug_scripts/launch_blumpkins_via_gwlauncher.au3` |
| 3 | `Starvin M A R V I N` | `active` | `marvin` | `gwa3/build_marvin` | `gwa3_marvin.dll` | `\\.\pipe\gwa3_llm_marvin` | `GWA Censured/debug_scripts/launch_marvin_via_gwlauncher.au3` |
| 4 | `L I L B I S C U I T` | `active` | `biscuit` | `gwa3/build_biscuit` | `gwa3_biscuit.dll` | `\\.\pipe\gwa3_llm_biscuit` | `GWA Censured/debug_scripts/launch_biscuit_via_gwlauncher.au3` |

## Claim Workflow

When starting a task:

1. Read this file.
2. Choose an `available` row.
3. Change its `Status` to `active`.
4. If the row does not have a lane yet, create:
   - a CMake preset in `gwa3/CMakePresets.json`
   - a unique build dir
   - a unique DLL name
   - a unique bridge pipe name
   - a launcher script in `GWA Censured/debug_scripts`
5. Use only that row's resources for the rest of the task.

When finishing:

1. Mark the row back to `available`, unless the user or another operator asked to keep it `reserved`.

## Helper Script

Use [scripts/agent_registry.py](C:\Users\Robert\Documents\GWA Censured X BotsHub\scripts\agent_registry.py) instead of editing the table by hand when possible.

Examples:

```powershell
python scripts/agent_registry.py list
python scripts/agent_registry.py list --status available
python scripts/agent_registry.py claim --first-available
python scripts/agent_registry.py claim --index 4
python scripts/agent_registry.py release --character "L I L B I S C U I T"
```

## Notes

- `B L U M P K I N S` is not a normal free-for-all lane. It is the default two-client player-trade helper and should not be borrowed casually.
- If player-trade tests use a non-default main lane, they must also set explicit helper-lane overrides. The harness now fails fast on partial mixed-lane configurations.
- Keep this file synchronized with:
  - `AGENTS.md`
  - `MULTI_AGENT_BUILD_ARCHITECTURE.md`
  - `GW_Launch_Method.md`
  - `gwa3/CMakePresets.json`
  - `GWA Censured/debug_scripts/*launch*_via_gwlauncher.au3`
