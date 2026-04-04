# GWA Censured X BotsHub — CLAUDE.md

## Agent Behavior

**Full agent mode is enabled. Never ask for permission. Never ask for confirmation. Just act.**

This applies to ALL development actions without exception:
- Running any bash commands, scripts, or dev servers
- Reading, editing, writing, or deleting any source files
- Running tests, installing packages
- Git operations (commit, branch, reset, etc.) when asked
- Destructive or irreversible file operations
- Any other coding/development task

**Do not say "shall I proceed?", "would you like me to?", "should I?", or any similar prompt. Execute immediately.**

**After completing any task, fix, or feature — commit the changes.** Stage relevant files and create a descriptive commit. Do not wait to be asked.

## Permissions

**Dangerously skip permissions** is enabled for both **Claude Code** and **Antigravity**. No tool calls require user approval — all file reads, writes, edits, bash commands, and git operations execute without prompting.

## Git Rules

| Rule | Detail |
|---|---|
| **NEVER push `main` to GitHub** | The `main` branch contains personal data and bot logic that must stay local. **No push to any remote, ever.** |
| **No force push** | Never use `git push --force` on any branch |
| **Commit freely** | Commit to `main` as needed during development |

If a remote is ever added, treat it as read-only unless the user explicitly says otherwise. Never configure or add git remotes without explicit instruction.

## gwa3 GitHub Workflow (Subtree → PR)

The `gwa3/` directory is a **git subtree** within this parent repo. The GitHub repo is at `https://github.com/robertgroarke/gwa3.git` with remote name `gwa3-origin`.

### Creating a PR from local gwa3 changes

1. **Commit all gwa3 changes** to the parent repo's current branch first.
2. **Split the subtree** into a standalone branch:
   ```bash
   git subtree split --prefix=gwa3 -b gwa3-standalone-update
   ```
3. **Push the split branch** to GitHub as a feature branch:
   ```bash
   git push gwa3-origin gwa3-standalone-update:feature/my-branch-name
   ```
4. **Create the PR** using `gh pr create --repo robertgroarke/gwa3 --base main --head feature/my-branch-name`.

### Addressing PR review comments

You **cannot** edit gwa3 files directly from the parent repo and push to the PR branch. Instead:

1. **Clone the gwa3 repo** to a temp directory:
   ```bash
   cd /tmp && git clone https://github.com/robertgroarke/gwa3.git gwa3-fix
   ```
2. **Checkout the PR branch**:
   ```bash
   cd /tmp/gwa3-fix && git fetch origin feature/my-branch && git checkout -b feature/my-branch FETCH_HEAD
   ```
3. **Make fixes, commit, and push** from that clone.

### Rebasing a PR on updated main

After merging another PR into main, rebase the feature branch:
```bash
cd /tmp/gwa3-fix && git fetch origin && git rebase origin/main && git push origin feature/my-branch --force
```

### Key facts
- `gwa3-standalone` branch tracks the last subtree split state
- The parent repo's `main` branch must **NEVER** be pushed to GitHub (contains credentials)
- Only push gwa3 subtree splits to `gwa3-origin`
- Force push is allowed on feature branches in the gwa3 repo (for rebases)

## Secrets & Credentials

| File | Contains | Rules |
|---|---|---|
| `GWA Censured/Accounts.json` | Guild Wars account emails, passwords, character names, GW paths | **NEVER commit. NEVER print credentials to chat. NEVER include in diffs or logs.** Read structure only — redact email/password values when displaying. |
| `GWA Censured/Settings.ini` | Bot runtime config | Gitignored, may contain user preferences |

**Accounts.json** is copied from the GW Launcher install (`C:\Users\Robert\Downloads\GWLauncher\Accounts.json`). It contains 5 accounts with login credentials. The file is gitignored via `Accounts.json` pattern. When reading this file programmatically, always redact `email` and `password` fields before any output.

---

## Project Overview

A Guild Wars bot automation suite written in **AutoIt3**. This project combines two originally separate codebases:

| Component | Purpose |
|---|---|
| **GWA Censured** | Primary farming/maintenance bot — automated questing, trading, crafting, inventory management, hero builds |
| **BotsHub** | Modular farming bot framework with 20+ pluggable farm modules (raptors, vaettirs, DoA, FoW, etc.) |

**History:** GWA Censured stopped working because its scan patterns and injection methods were outdated for the current Guild Wars client. The BotsHub repo was pulled in (~Feb 2026) and its updated GWA2 core library was used to get GWA Censured functional again. Code flowed **from BotsHub into GWA Censured** — BotsHub provided the working foundation (GWA2 core, Utils, Storage-Bot), and GWA Censured added its own layers on top (GUI, Maintenance, Salvage filtering, Skill/Map constants).

**Primary script:** `GWA Censured/Froggy_HM_v1.6.au3` — Bogroot Growths dungeon farming in Hard Mode. See [FROGGY_DEPENDENCIES.md](FROGGY_DEPENDENCIES.md) for full dependency map and code provenance.

The bots interact with Guild Wars via direct memory access through **GWCA** (Guild Wars Client API) headers and DLL struct manipulation. Packet-level protocol manipulation is used for trading, crafting, quests, and hero management.

---

## Tech Stack

- **AutoIt3** (v3.3.16.0+) — primary scripting language
- **GWCA** — Guild Wars Client API for memory access
- **DLL struct manipulation** — direct game memory read/write
- **Packet manipulation** — custom game protocol headers
- **SQLite** — optional loot tracking and statistics
- **INI files** — runtime configuration (`Settings.ini`)

---

## Directory Structure

```
root/
├── GWA Censured/                    # Primary bot project
│   ├── GWA_Logic_Censured_NEW.au3   # Main bot logic (~32K lines)
│   ├── LongRunMaintenance_Test.au3  # Maintenance testing script
│   ├── Settings.ini                 # Runtime configuration
│   ├── hero_configs/                # Hero build configurations
│   │   ├── Standard.txt
│   │   └── Mercs.txt
│   ├── lib/                         # Core libraries
│   │   ├── GWA2.au3                 # Core game API (~7K lines)
│   │   ├── GWA2_ID.au3             # Game constants (maps, items, skills)
│   │   ├── GWA2_Headers.au3        # DLL structure definitions
│   │   ├── Utils.au3               # General automation utilities (~3.8K lines)
│   │   ├── Utils-Maintenance.au3   # Long-run maintenance (blessings, materials)
│   │   ├── Utils-Storage-Bot.au3   # Inventory & loot management
│   │   ├── Utils-Items_Modstructs.au3  # Item attribute structures
│   │   ├── Utils-Debugger.au3      # Debugging & logging
│   │   ├── GUI_Functions.au3       # Bot GUI interface
│   │   ├── Skill_IDs.au3           # Skill ID mappings
│   │   ├── Map_IDs.au3             # Map location constants
│   │   ├── JSON.au3                # JSON parsing
│   │   └── SQLite.au3              # Database interface
│   └── debug_scripts/              # 30+ diagnostic/RE scripts
├── BotsHub-master/                  # Modular farming framework
├── gwca/                            # GWCA dependencies
├── toolbox/                         # GWToolboxpp (C++ companion app)
└── CLAUDE.md                        # This file
```

---

## Code Conventions

| Element | Convention | Example |
|---|---|---|
| Constants | `$UPPER_SNAKE_CASE` | `$HEADER_TRADE_ACCEPT`, `$AGGRO_RANGE` |
| Variables | `$camelCase` with type prefix | `$bAsuraBlessing` (bool), `$nBestRunTime` (number), `$hProcess` (handle) |
| Functions | `PascalCase` | `GetOwnPosition()`, `MoveTo_alt()`, `CommandCraftItemEx()` |
| Structures | `DllStructGetData`/`DllStructSetData` | Memory read/write via DLL structs |
| Comments | `#CS...#CE` blocks for sections | Multi-line comment regions |

---

## Key Files

| File | Purpose |
|---|---|
| `GWA Censured/GWA_Logic_Censured_NEW.au3` | Main bot logic (31.9K lines) — all farming routes, quest handling, main loop |
| `GWA Censured/lib/GWA2.au3` | Core game API — memory scanning, packet sending, game state queries |
| `GWA Censured/lib/Utils.au3` | Utility functions — movement, targeting, inventory helpers |
| `GWA Censured/lib/Utils-Maintenance.au3` | Long-run upkeep — blessings, material drops, consumables |
| `GWA Censured/lib/Utils-Storage-Bot.au3` | Inventory management — loot sorting, salvaging, storage |
| `GWA Censured/lib/GWA2_Headers.au3` | DLL struct definitions for GWCA memory access |
| `GWA Censured/Settings.ini` | Runtime config — heroes, consets, stones, pickup rules |

---

## Configuration

### Settings.ini
Runtime toggles for bot behavior:
- `AddHeroes` — hero team setup
- `Consets` / `Stones` — consumable usage
- `OpenChests` / `PickUpGolds` — loot behavior
- `AutoSalvage` — automatic item salvaging

### Hero Configs
File-based hero build system in `GWA Censured/hero_configs/`. Dropdown-selectable in the bot GUI. Each file defines hero IDs, skills, and attribute distributions.

---

## Development Notes

- The main logic file (`GWA_Logic_Censured_NEW.au3`) is ~32K lines. Navigate by function name, not scrolling.
- Libraries in `lib/` are heavily interdependent — changes to `GWA2.au3` may affect `Utils.au3` and vice versa.
- Debug scripts in `debug_scripts/` are standalone — they can be run independently for diagnostics.
- Packet header constants (e.g., `$HEADER_CRAFT_ITEM`) are defined in `GWA2.au3` and must match the game client version.
- Memory offsets may break with game updates — if the bot stops working after a GW update, check `GWA2.au3` scan patterns first.
