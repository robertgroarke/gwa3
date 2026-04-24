# Multi-Agent Build Architecture

This repo now supports running multiple coding agents against different Guild Wars characters without clobbering each other's DLLs, logs, bridge pipes, or mode flags.

Account ownership and current claim status live in `AGENT_ACCOUNT_REGISTRY.md`.

## Core Rules

1. Always launch Guild Wars through `GW_Launcher.exe`.
2. Always inject only the exact PID returned by the launcher.
3. Never share the same build directory between agents.
4. Never share the same DLL name between agents.
5. Never share the same bridge pipe name between agents.
6. For live in-game debug or harness runs, prefer quiet Asia/Japan districts instead of America English defaults unless the task explicitly requires another locale.

## Recommended Agent Layout

Use one isolated build per active agent:

- Disco Panic agent:
  - build dir: `gwa3/build_disco`
  - DLL: `gwa3_disco.dll`
  - pipe: `\\.\pipe\gwa3_llm_disco`
- BEASTRIT agent:
  - build dir: `gwa3/build_beastrit`
  - DLL: `gwa3_beastrit.dll`
  - pipe: `\\.\pipe\gwa3_llm_beastrit`
- Trade harness agent:
  - build dir: `gwa3/build_trade`
  - DLL: `gwa3_trade.dll`
  - pipe: `\\.\pipe\gwa3_llm_trade`
- MARVIN agent:
  - build dir: `gwa3/build_marvin`
  - DLL: `gwa3_marvin.dll`
  - pipe: `\\.\pipe\gwa3_llm_marvin`
- BISCUIT agent:
  - build dir: `gwa3/build_biscuit`
  - DLL: `gwa3_biscuit.dll`
  - pipe: `\\.\pipe\gwa3_llm_biscuit`

## CMake Presets

`gwa3/CMakePresets.json` defines named presets for the common agent lanes:

- `disco`
- `beastrit`
- `trade`
- `marvin`
- `biscuit`

Examples:

```powershell
cmake --preset disco
cmake --build --preset disco --target gwa3 injector
```

```powershell
cmake --preset trade
cmake --build --preset trade --target gwa3 injector
```

Each preset isolates:

- `binaryDir`
- `GWA3_DLL_NAME`
- `GWA3_PIPE_NAME`

## Runner Parameters

The main PowerShell runners now accept:

- `-BuildDir`
- `-DllName`

They also honor:

- `GWA3_BUILD_DIR`
- `GWA3_DLL_NAME`

Examples:

```powershell
powershell -ExecutionPolicy Bypass -File gwa3/tools/run_froggy_test.ps1 `
  -AccountIndex 1 `
  -BuildDir "c:\Users\Robert\Documents\GWA Censured X BotsHub\gwa3\build_disco" `
  -DllName "gwa3_disco.dll"
```

```powershell
$env:GWA3_BUILD_DIR = "c:\Users\Robert\Documents\GWA Censured X BotsHub\gwa3\build_trade"
$env:GWA3_DLL_NAME = "gwa3_trade.dll"
python -m bridge.tests --filter "test_player_trade_*"
```

## Bridge Pipe Isolation

The C++ DLL pipe name is now compile-time configurable via `GWA3_PIPE_NAME`.

The Python bridge and tests read the pipe name from:

- environment variable `GWA3_PIPE_NAME`
- otherwise default `\\.\pipe\gwa3_llm`

Example:

```powershell
$env:GWA3_PIPE_NAME = "\\.\pipe\gwa3_llm_disco"
python -m bridge --pipe $env:GWA3_PIPE_NAME
```

## Mode-Flag Isolation

The injector and DLL now support PID-scoped mode flags.

That means when you inject with:

```powershell
injector.exe --pid 12345 --dll gwa3_disco.dll --llm
```

the DLL prefers a PID-specific mode flag, so another agent's test mode does not get consumed by the wrong process.

## Practical Workflow

### Disco Panic bridge work

```powershell
cmake --preset disco
cmake --build --preset disco --target gwa3 injector
$env:GWA3_BUILD_DIR = "c:\Users\Robert\Documents\GWA Censured X BotsHub\gwa3\build_disco"
$env:GWA3_DLL_NAME = "gwa3_disco.dll"
$env:GWA3_PIPE_NAME = "\\.\pipe\gwa3_llm_disco"
```

### BEASTRIT Froggy work

```powershell
cmake --preset beastrit
cmake --build --preset beastrit --target gwa3 injector
powershell -ExecutionPolicy Bypass -File gwa3/tools/run_froggy_test.ps1 `
  -AccountIndex 0 `
  -BuildDir "c:\Users\Robert\Documents\GWA Censured X BotsHub\gwa3\build_beastrit" `
  -DllName "gwa3_beastrit.dll"
```

## Known Follow-Up Work

These improvements are in place now:

- isolated build presets
- configurable DLL names
- configurable bridge pipe names
- PID-scoped mode flags
- runner support for custom build dirs and DLL names

Still worth doing later:

- make all remaining ad hoc scripts accept `BuildDir` and `DllName`
- parameterize screenshot/log/report output roots by agent tag
- make injector `--list` and diagnostics surface the active DLL name more explicitly
