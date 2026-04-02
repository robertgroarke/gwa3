# GWA3 — CLAUDE.md

## Agent Behavior

Full agent mode is enabled. Never ask for permission. Just act.

## Project Overview

GWA3 is a C++ DLL that injects into the Guild Wars game client to provide
a programmatic API (memory scanning, packet sending, manager APIs).
It is a ground-up rewrite of the AutoIt-based GWA2 library.

## Git & PR Workflow

### Branch Strategy

| Rule | Detail |
|---|---|
| **Never push directly to `main`** | All changes go through Pull Requests |
| **Feature branches** | Name: `feature/<short-description>` |
| **Fix branches** | Name: `fix/<short-description>` |
| **Refactor branches** | Name: `refactor/<short-description>` |

### Creating a PR (mandatory for all changes)

After completing any task, Claude Code must:

1. Create a feature branch from `main`:
   ```
   git checkout -b feature/<description> main
   ```
2. Commit changes with a descriptive message
3. Push the branch to origin:
   ```
   git push -u origin feature/<description>
   ```
4. Open a PR using `gh`:
   ```
   gh pr create --title "<concise title>" --body "$(cat <<'EOF'
   ## Summary
   <bullet points>

   ## Test plan
   <how to verify>
   EOF
   )"
   ```
5. Report the PR URL back to the user

### Commit Conventions

- Use imperative mood: "Add X", "Fix Y", "Implement Z"
- Reference test results when applicable
- One logical change per commit

### After Codex Review

When the user shares Codex review feedback:
1. Read the PR comments via `gh api repos/robertgroarke/gwa3/pulls/{number}/comments`
2. Address each comment
3. Push fixes to the same branch (the PR updates automatically)
4. Codex will re-review on new pushes if configured

## Tech Stack

- C++17, compiled with MSVC (Visual Studio 2022)
- CMake build system
- DLL injection into 32-bit Guild Wars client (Gw.exe)
- Pattern scanning for runtime offset resolution
- Integration tests run inside the injected DLL

## Directory Structure

```
├── CMakeLists.txt
├── include/gwa3/          # Public headers
│   ├── bot/               # Bot framework
│   ├── core/              # Scanner, hooks, memory, logging
│   ├── game/              # Game struct definitions
│   └── managers/          # Manager APIs (Agent, Item, Map, etc.)
├── src/
│   ├── core/              # Core implementations
│   ├── managers/          # Manager implementations
│   ├── bot/               # Bot logic
│   ├── packets/           # Packet definitions
│   ├── tests/             # Integration test suite
│   └── dllmain.cpp        # DLL entry point
├── memory/                # Memory analysis notes
├── tools/                 # Injector, pattern checker
└── tests/                 # Offline unit tests
```

## Secrets & Credentials

This repo contains NO credentials or account data. All files are safe to push.

## Code Conventions

| Element | Convention |
|---|---|
| Namespaces | `gwa3::`, `gwa3::core::`, `gwa3::game::` |
| Classes | `PascalCase` |
| Functions | `PascalCase` |
| Variables | `camelCase` |
| Constants | `k` prefix: `kAgentArray`, `kBasePointer` |
| Macros | `UPPER_SNAKE_CASE` |
| Headers | `#pragma once`, include guards not used |
