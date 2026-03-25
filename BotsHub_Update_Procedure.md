# BotsHub Update Procedure

**Last updated:** 2026-03-25
**Current upstream commit:** `47b3348` (2026-03-20)

---

## Overview

The GWA Censured project uses a separation architecture where upstream BotsHub files live in `lib/botshub/` and all custom code lives in `lib/custom/`. This allows BotsHub updates to be applied by replacing the `botshub/` files.

## Directory Structure

```
GWA Censured/lib/
├── botshub/                  # Upstream BotsHub files (19 files)
│   ├── GWA2.au3              # Game API (high-level)
│   ├── GWA2_Assembly.au3     # ASM injection engine (+ 3 crafting hooks)
│   ├── GWA2_Assembly_Chatlog.au3
│   ├── GWA2_Headers.au3      # Packet header constants
│   ├── GWA2_ID.au3           # ID database (includes sub-files)
│   ├── GWA2_ID_Items.au3
│   ├── GWA2_ID_Maps.au3
│   ├── GWA2_ID_Quests.au3
│   ├── GWA2_ID_Skills.au3
│   ├── Build_PW_Heroic-Refrain.au3
│   ├── Utils.au3             # Core utilities
│   ├── Utils-Agents.au3      # Agent/party utilities
│   ├── Utils-Storage.au3     # Inventory/storage management
│   ├── Utils-Debugger.au3    # Debug/logging framework
│   ├── Utils-Items_Modstructs.au3
│   ├── JSON.au3
│   ├── SQLite.au3
│   └── SQLite.dll.au3
│
├── custom/                   # GWA Censured custom code (14 files)
│   ├── BotsHub_Stubs.au3     # Framework stubs + API aliases
│   ├── GWA2_Compat.au3       # MemRead/MemWrite/MemReadPtr wrappers
│   ├── GWA2_Crafting.au3     # Crafting system (hooks into GWA2_Assembly)
│   ├── GWA2_Extensions.au3   # Custom GWA2 functions (no upstream equivalent)
│   ├── ID_Aliases.au3        # Constant name aliases ($ID_GADDS_CAMP etc.)
│   ├── NPC_Coordinates.au3   # Custom NPC locations
│   ├── GUI_Functions.au3     # Froggy bot GUI
│   ├── Utils-Maintenance.au3 # Maintenance automation
│   ├── Utils-Salvage.au3     # Salvage decision logic
│   ├── Map_IDs.au3           # GWA Censured map ID constants
│   ├── Skill_IDs.au3         # GWA Censured skill ID constants
│   ├── Skill_Types.au3       # Skill type constants
│   ├── RareSkins.au3         # Rare skin lookup table
│   └── UsefulMods.au3        # Weapon/armor mod data tables
│
└── Froggy_Includes.au3       # Master include (wires everything together)
```

---

## Step-by-Step Update Procedure

### Step 1: Clone the Latest BotsHub

```bash
git clone https://github.com/caustic-kronos/BotsHub.git BotsHub-new
```

Record the commit hash from `BotsHub-new/VERSION` for documentation.

### Step 2: Replace botshub/ Files

Copy ALL files from `BotsHub-new/lib/` into `GWA Censured/lib/botshub/`, replacing existing files:

```bash
cp BotsHub-new/lib/*.au3 "GWA Censured/lib/botshub/"
cp BotsHub-new/sqlite3.dll "GWA Censured/lib/botshub/"  # if present
```

**Important:** Do NOT copy `BotsHub-new/BotsHub.au3` or `BotsHub-new/src/` — those are the BotsHub application, not libraries.

### Step 3: Re-Add Crafting Hook Lines

The crafting system hooks into `GWA2_Assembly.au3` at 3 points. After replacing the file, re-add these lines:

**Hook 1 — In the data section** (after `_('DisableRendering/4')`):
```autoit
; GWA Censured crafting extension hook
If IsDeclared('g_CraftingExtension') Then Extend_CraftingData()
```

**Hook 2 — In the init section** (after `If IsDeclared('CHAT_LOG_STRUCT') Then ExtendInitializeChatLogResult()`):
```autoit
; GWA Censured crafting extension hook
If IsDeclared('g_CraftingExtension') Then Extend_CraftingInit()
```

**Hook 3 — In the craft command section** (after `AssemblerCreateCraftItemCommand()` function's last `_('ljmp CommandReturn')`, before `EndFunc`):
```autoit
; GWA Censured crafting extension hook
If IsDeclared('g_CraftingExtension') Then Extend_CraftingCommands()
```

**How to find insertion points:** Search for these patterns:
- Hook 1: Search for `DisableRendering/4` — add the hook on the next line
- Hook 2: Search for `ExtendInitializeChatLogResult` — add the hook on the next line
- Hook 3: Search for `AssemblerCreateCraftItemCommand` and find the `EndFunc` — add before `EndFunc`

### Step 4: Check for New/Changed Files

Compare the new upstream `lib/` directory against the old `botshub/`:
```bash
diff -rq BotsHub-new/lib/ "GWA Censured/lib/botshub/" --exclude='.git'
```

If upstream added NEW files (e.g., a new `Utils-*.au3`), copy them to `botshub/` and add a `#include` line in `Froggy_Includes.au3`.

### Step 5: Check for API Changes

Run Au3Check to identify breaking changes:
```bash
Au3Check.exe -q -I "GWA Censured/lib" -I "GWA Censured/lib/botshub" \
  -I "GWA Censured/lib/custom" -I "AutoIt3/Include" \
  "GWA Censured/Froggy_HM_v1.6.au3"
```

**Expected baseline:** ~191 errors (all pre-existing: Array.au3 double-include, Map syntax, DllStructSetData args, stdlib const redeclarations).

If the error count increases, check for:

| Error Type | Cause | Fix Location |
|-----------|-------|-------------|
| `FuncName(): undefined function` | Upstream renamed/removed a function | Add alias to `custom/BotsHub_Stubs.au3` |
| `$VAR: undeclared global variable` | Upstream added a new framework global | Add stub to `custom/BotsHub_Stubs.au3` |
| `FuncName() called with wrong number of args` | Upstream changed function signature | Add compat wrapper to `custom/BotsHub_Stubs.au3` |
| `FuncName() already defined` | Upstream added a function we also define | Remove our version from `custom/GWA2_Extensions.au3` (upstream's version wins) |

### Step 6: Update BotsHub_Stubs.au3

If Step 5 revealed new undefined functions or undeclared variables:

1. **New globals:** Add `Global $new_var = <default>` to `BotsHub_Stubs.au3`
2. **Renamed functions:** Add `Func OldName(args) → Return NewName(args) EndFunc`
3. **Changed signatures:** Add a compat wrapper like `GetSalvageKitCompat`
4. **New extension hooks:** Add empty stubs `Func NewExtension() EndFunc`

### Step 7: Update ID_Aliases.au3

If upstream renamed any map/item/skill ID constants that our code uses:

```autoit
; In custom/ID_Aliases.au3:
Global Const $OLD_NAME = $NEW_UPSTREAM_NAME
```

### Step 8: Update GWA2_Compat.au3

If upstream changed the `MemoryRead`/`MemoryWrite`/`MemoryReadPtr` signatures (unlikely but possible), update the wrappers in `GWA2_Compat.au3`.

Current wrapper pattern:
```autoit
Func MemRead($address, $type = 'dword')
    Return MemoryRead(GetProcessHandle(), $address, $type)
EndFunc
```

### Step 9: Test

1. Run Au3Check — error count should be ≤ 191
2. Run the Keystone ASM test suite: `python tests/test_asm_keystone.py`
3. If possible, run the Froggy script through one complete cycle

### Step 10: Commit

```bash
git add "GWA Censured/lib/botshub/" "GWA Censured/lib/custom/"
git commit -m "Update BotsHub upstream to commit XXXXXXX"
```

---

## What NOT to Modify in botshub/

- **Never add custom functions** to botshub/ files
- **Never modify function signatures** in botshub/ files
- **The ONLY modifications** allowed are the 3 crafting hook lines in `GWA2_Assembly.au3`
- All custom code goes in `custom/` files

## Compatibility Layer Architecture

```
Froggy script
  → Froggy_Includes.au3 (master include)
    → BotsHub_Stubs.au3     (framework stubs, loads BEFORE botshub)
    → botshub/*.au3          (upstream code, uses new API)
    → GWA2_Compat.au3        (MemRead wrappers, loads AFTER botshub)
    → ID_Aliases.au3         (constant aliases, loads AFTER botshub)
    → custom extensions      (our code, uses MemRead/MemWrite wrappers)
```

**Key principle:** Custom code calls `MemRead()`/`MemWrite()`/`MemReadPtr()` (wrappers). Upstream code calls `MemoryRead($processHandle, ...)` (native). They coexist because they have different function names.

## Troubleshooting

**"Function already defined" errors after update:**
An upstream file now defines a function that also exists in our `custom/GWA2_Extensions.au3`. Remove the duplicate from Extensions — upstream's version is preferred.

**"Called with wrong number of args" after update:**
A function signature changed. Add a compat wrapper in `BotsHub_Stubs.au3` with the old signature that calls the new one.

**Crafting hooks not firing:**
Verify the 3 `IsDeclared('g_CraftingExtension')` lines are present in `botshub/GWA2_Assembly.au3`. They get overwritten every time the file is replaced.

**New scan patterns needed:**
Upstream keeps scan patterns current. After replacing `botshub/GWA2_Assembly.au3`, the scan patterns are automatically updated. No manual byte editing needed.
