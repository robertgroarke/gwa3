# GWA3

GWA3 is a native C++ Guild Wars automation library and DLL. It provides typed APIs for game memory access, packet dispatch, manager-level game operations, LLM bridge snapshots/actions, and reusable dungeon bot support.

## Demo

![Froggy HM running Bogroot Growths level 2 boss route with live command log](assets/gwa3_bogrootlvl2boss.gif)

## Layers

- `include/gwa3/` and `src/gwa3/`: core GWA3 headers and implementation.
- `include/gwa3/dungeon/` and `src/gwa3/dungeon/`: dungeon support built on the core library.
- `bots/`: concrete bot implementations. Froggy HM is the only bot currently expected to run end to end; the other dungeon bots are ports in progress.
- `bridge/`: Python LLM bridge client. The bridge is public but still in active development.
- `tools/`: injector utility.

## Build

Prerequisites:

- Windows with Guild Wars installed.
- Visual Studio 2022 with the Desktop development with C++ workload.
- CMake 3.20 or newer.
- Internet access during the first configure, because CMake fetches MinHook and nlohmann/json.

GWA3 targets the 32-bit Guild Wars client. The included preset uses Visual Studio 2022 with Win32 output:

```powershell
cmake --preset vs2022
cmake --build --preset vs2022 --target gwa3 injector
```

Release outputs are written under `build/bin/Release/`.

## Injection

Build the DLL and injector first. Then launch Guild Wars normally, choose the character you want to run, and inject into that exact Guild Wars process.

The public injector does not select by account or character name. It selects by process ID. If multiple Guild Wars clients are open, list them first:

```powershell
.\build\bin\Release\injector.exe --list
```

Run the Froggy HM bot by injecting `gwa3.dll` into the chosen PID:

```powershell
.\build\bin\Release\injector.exe --pid <GW_PID> --dll gwa3.dll
```

If only one Guild Wars window is open, the injector can auto-detect it, but using `--pid` is the safest path when more than one client may be running.

LLM bridge mode starts the named-pipe bridge without starting the Froggy bot:

```powershell
.\build\bin\Release\injector.exe --pid <GW_PID> --dll gwa3.dll --llm
```

Advisory mode runs the Froggy bot and bridge together:

```powershell
.\build\bin\Release\injector.exe --pid <GW_PID> --dll gwa3.dll --llm-advisory
```

Runtime logs are written next to the DLL, including `gwa3_log_<PID>.txt` and `gwa3_bot.log`.

## Froggy Runtime Notes

Froggy HM is the default bot module compiled into `gwa3.dll`. The default route starts from Gadd's Encampment and targets Bogroot Growths HM.

The public repo intentionally does not include private account launchers, credentials, character-specific test fixtures, or live harnesses. Public users should launch their own Guild Wars client and select the character in-game before injection.

Froggy requires a hero template file to configure heroes. Without one, outpost setup stops before entering the dungeon.

## Hero Templates

Froggy chooses hero templates from `config/gwa3.ini` when that file exists. Copy `config/gwa3.ini.example` to `config/gwa3.ini`, then set a global default:

```ini
[Settings]
HeroConfig=Standard.txt
```

Or set a character-specific override:

```ini
[HeroConfigs]
Your Character Name=Custom.txt
```

Template files live in `config/hero_configs/`. Each non-comment line is:

```text
hero_id,template_code
```

For example:

```text
25,OwUTM0HD1ZxkAAAAgpAAACCAAA
```

The template code is the normal Guild Wars skill template code for that hero. Lines starting with `;` are comments. If `config/gwa3.ini` is missing, Froggy tries `Standard.txt`. If the selected template file cannot be loaded, outpost setup fails instead of adding heroes from embedded defaults.

The actual `config/gwa3.ini` file and `config/hero_configs/*.txt` files are ignored by Git so users can keep character-specific hero setups local.

## Project Layout

```text
bots/                         Concrete bot implementations
bridge/                       Python LLM bridge
include/gwa3/                 Public GWA3 headers
include/gwa3/dungeon/         Shared dungeon helper headers
src/gwa3/                     GWA3 DLL and library implementation
src/gwa3/dungeon/             Shared dungeon helper implementation
tools/                        Injector utility
```
