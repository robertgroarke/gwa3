# GWA3

A C++ DLL that injects into the Guild Wars game client to provide a programmatic API for automation, memory reading, and packet manipulation.

GWA3 is a ground-up rewrite of the AutoIt-based GWA2 library, targeting native performance and type safety.

## Features

- **Pattern scanner** — runtime offset resolution via byte signature scanning
- **Manager APIs** — Agent, Item, Map, Skill, Party, Chat, Trade, Quest, Camera, Guild, and more
- **Packet system** — type-safe CtoS (Client-to-Server) packet sending with all 113 header constants
- **StoC hooks** — Server-to-Client packet interception via handler table replacement
- **Game thread integration** — command queue dispatched on the game's render thread
- **UI frame system** — frame enumeration and ButtonClick dispatch
- **Integration test suite** — 108+ automated tests that run inside the injected DLL
- **Bot framework** — state machine core with Froggy HM (Bogroot Growths) as reference implementation

## Building

Requires:
- Visual Studio 2022 (MSVC v143 toolset)
- CMake 3.20+
- Windows SDK (targeting 32-bit x86)

```bash
cmake -B build -A Win32
cmake --build build --config Release
```

Output: `build/bin/Release/gwa3.dll`

## Usage

Inject the built DLL into a running Guild Wars client (`Gw.exe`) using the included injector:

```bash
# Inject into first found GW process
build/bin/Release/injector.exe

# Inject into specific PID
build/bin/Release/injector.exe --pid 12345

# List running GW clients
build/bin/Release/injector.exe --list
```

## Project Structure

```
├── CMakeLists.txt          # Build configuration
├── include/gwa3/           # Public headers
│   ├── core/               # Scanner, hooks, memory, logging
│   ├── game/               # Game struct definitions (Agent, Item, Map, etc.)
│   ├── managers/           # Manager API headers
│   └── bot/                # Bot framework headers
├── src/
│   ├── dllmain.cpp         # DLL entry point and initialization
│   ├── core/               # Core implementations
│   ├── managers/           # Manager implementations
│   ├── packets/            # Packet header definitions
│   ├── tests/              # Integration test suite
│   └── bot/                # Bot logic (Froggy HM)
├── memory/                 # Memory layout analysis notes
└── tools/                  # Injector and diagnostic utilities
```

## Testing

Integration tests run inside the injected DLL and exercise the full API against a live game client:

```bash
# Create flag file to trigger tests on next injection
touch build/bin/Release/gwa3_test_integration.flag

# Or use the injector's --test-integ flag
build/bin/Release/injector.exe --test-integ
```

## License

Private repository. All rights reserved.
