# GW Launch Method

This repo should launch Guild Wars clients through the launcher logic, not by starting `Gw.exe` directly.

Why:
- multiple GW clients may be running at once
- the launcher path applies the multiclient patch correctly
- it keeps account selection tied to `Accounts.json`
- it reduces the risk of injecting the wrong client
- it lets us separate launcher failures from DLL/bridge failures when multiple clients are being tested

## Rule

Always use launcher-based account launch.

Do not:
- call `Start-Process` on `Gw.exe` directly
- pick the first `Gw.exe` process by name
- inject into a PID unless you have confirmed it belongs to the intended account

## Account Source

Account definitions live in:
- [GWA Censured/Accounts.json](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWA%20Censured\Accounts.json)

Current account mapping:
- index `0`: `B E A S T R I T`
- index `1`: `D I S C O P A N I C`
- index `2`: `B L U M P K I N S`
- index `3`: `Starvin M A R V I N`
- index `4`: `L I L B I S C U I T`

Current ownership and lane status are tracked in:
- [AGENT_ACCOUNT_REGISTRY.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\AGENT_ACCOUNT_REGISTRY.md)

Do not print or copy credentials from `Accounts.json`.

## Validated Method

The validated launcher-based method in this repo is the AutoIt helper path:
- [GWLauncher.au3](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWA%20Censured\lib\custom\GWLauncher.au3)

This helper:
- loads the target account from `Accounts.json`
- launches the exact `gwpath` for that account
- creates the process suspended
- applies the multiclient patch
- resumes the GW thread

This is the same launcher behavior agents should use here.

## Tested Script

A minimal launcher script for Disco Panic exists here:
- [launch_disco_panic_via_gwlauncher.au3](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWA%20Censured\debug_scripts\launch_disco_panic_via_gwlauncher.au3)

It was validated to:
- load account index `1`
- launch `D I S C O P A N I C`
- emit the launched PID

Launcher log output is written here:
- [launch_disco_panic_via_gwlauncher.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWA%20Censured\debug_scripts\launch_disco_panic_via_gwlauncher.log)

## How To Launch An Account

Preferred pattern for any account:

1. Load accounts from [GWA Censured/Accounts.json](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWA%20Censured\Accounts.json)
2. Find the account by character name
3. Call `GWLauncher_LaunchAccount($accounts, $index)`
4. Record the returned PID
5. Confirm the PID matches the intended account `gwpath`
6. Verify the GW process actually reached a healthy loaded state before injection
7. Inject only that PID

AutoIt skeleton:

```autoit
#RequireAdmin
#include "..\lib\Froggy_Includes.au3"

Global Const $ACCOUNTS_PATH = "C:\Users\Robert\Documents\GWA Censured X BotsHub\GWA Censured\Accounts.json"
Global Const $TARGET_CHARACTER = "D I S C O P A N I C"

Local $accounts = GWLauncher_LoadAccounts($ACCOUNTS_PATH)
Local $idx = GWLauncher_FindAccountByCharacter($accounts, $TARGET_CHARACTER)
Local $result = GWLauncher_LaunchAccount($accounts, $idx)
ConsoleWrite("GWLAUNCHER_PID=" & $result[0] & @CRLF)
```

Run it with:

```powershell
& 'C:\Program Files (x86)\AutoIt3\AutoIt3.exe' `
  'c:\Users\Robert\Documents\GWA Censured X BotsHub\GWA Censured\debug_scripts\launch_disco_panic_via_gwlauncher.au3'
```

## Per-Account Character Names

Use these exact character names with `GWLauncher_FindAccountByCharacter(...)`:
- `B E A S T R I T`
- `D I S C O P A N I C`
- `B L U M P K I N S`
- `Starvin M A R V I N`
- `L I L B I S C U I T`

Known per-account launcher helpers currently present:
- `launch_beastrit_via_gwlauncher.au3`
- `launch_disco_panic_via_gwlauncher.au3`
- `launch_blumpkins_via_gwlauncher.au3`
- `launch_marvin_via_gwlauncher.au3`
- `launch_biscuit_via_gwlauncher.au3`

## Injection Rule After Launch

After launch, inject only the PID returned by the launcher path.

Example:

```powershell
& 'c:\Users\Robert\Documents\GWA Censured X BotsHub\gwa3\build\bin\Release\injector.exe' --pid <LAUNCHER_PID> --llm
```

Do not:
- scan for the first `Gw.exe`
- inject all GW clients
- assume the newest process is yours without checking

## Launcher Health Gate

Some recent false failures were caused by `GWLauncher` leaving behind `Gw.exe` windows that never finished loading. Those are launcher failures, not DLL injector failures.

Treat a launch as valid only if all of these are true before injection:
- the exact launcher-returned PID is still alive after a short settle
- the process memory footprint climbs to a sane loaded range
- the process does not exit during the settle window

Current practical gate used by the trade harness:
- wait for the launcher-returned PID
- wait up to `30s`
- require memory usage to reach at least about `100,000 KB`

If that gate is not met:
- classify it as a launcher failure
- do not report it as a bridge or DLL regression
- relaunch cleanly

## Fresh-Run Cleanup Rule

For injection-based bridge and trade tests, do not reuse an old client from a previous run.

Required behavior for account-specific harnesses:
- each test run should start from a fresh client for that character
- before launching, kill only stale leftover clients for that same character
- after the run ends, clean up only the clients that harness launched for that same character
- never kill unrelated clients belonging to other characters or other agents
- do not try to reuse an already-injected client for a new test run; for the test harness, injected clients are single-use and should be replaced with a fresh launch
- harness cleanup should log the exact character name and PID before terminating anything so cleanup is auditable in multi-agent sessions

Practical rule:
- stale failed boot: process is alive but still stuck far below the healthy threshold after the settle window
- finished prior run: previously injected client from the same test harness/account

For example, if testing `D I S C O P A N I C`:
- remove stale Disco Panic leftovers before relaunch
- launch a fresh Disco Panic client for the new run
- clean up Disco Panic when that run ends
- do not touch `B E A S T R I T`, `B L U M P K I N S`, or any other agent's clients

This prevents two failure modes:
- piling up dead or half-loaded `Gw.exe` windows from repeated launcher retries
- trying to attach a new test run to a client that was already used for a prior injection lifecycle

This matters because a half-started GW process can cause:
- injector open failures
- missing pipe failures
- misleading test `setUp` failures
- false negative trade/bridge results

## Low-Visibility Test Districts

Bot-sensitive bridge tests should not run in public hubs or crowded default districts.

Default rule for live debug and harness work:
- prefer region `4` (`Asia Japan`)
- prefer district `99`
- fall back to district `1` in the same Asia/Japan region if district `99` does not load
- do not default to America English districts for bot-sensitive debugging

Current rule for the two-client player-trade harness:
- use a quiet outpost instead of a merchant hub
- use a high-number district unlikely to contain real players
- keep both accounts in the same explicit map, region, district, and language before starting the trade action

Current trade harness rendezvous:
- map `650` (`Longeye's Ledge`)
- region `4` (`Asia Japan`)
- preferred district `99`
- language `8` (`Japanese`)

District handling rule:
- the helper client requests the preferred quiet district
- if Guild Wars falls back to a different district, that is not treated as a bridge failure
- the main client must then join the helper's actual loaded district, as reported by the helper status file
- the current trade helper falls back to district `1` in the same Asia/Japan region if district `99` never loads cleanly

Do not use America district `1` for bot-sensitive two-client trade tests unless the test explicitly requires it.
Do not use America English districts for single-client debug harnesses either unless the test explicitly requires it.

Operational guidance:
- do not run player-trade tests in the default Gadd's district
- do not path the trade clients through a merchant crowd just to meet each other
- if a test requires two bots to interact, prefer a quiet spawn-area rendezvous in an inactive district

## Notes For Other Agents

- Another agent may already be using `B E A S T R I T`
- If you are assigned `D I S C O P A N I C`, launch only that account
- Keep launcher logs or printed PID output so you can prove which client is yours
- If you need a new per-account launcher script, copy the Disco Panic script and only change the character constant

## Current Known Good Flow

Known good validated flow:

1. Launch Disco Panic via:
   - [launch_disco_panic_via_gwlauncher.au3](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWA%20Censured\debug_scripts\launch_disco_panic_via_gwlauncher.au3)
2. Read `GWLAUNCHER_PID=...`
3. Inject that exact PID with `injector.exe --pid <pid> --llm`
4. Run bridge tests

This flow successfully reached:
- launcher-based multiclient launch
- correct Disco Panic PID selection
- successful `--llm` injection
- passing bridge `test_pipe_connect`
