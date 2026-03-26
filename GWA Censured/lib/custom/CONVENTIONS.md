# BotCore Module Conventions

**Scope:** naming, state, and compatibility rules for all `BotCore-*.au3` modules
under `GWA Censured/lib/custom/`.

**Target modules:**

| Module | Responsibility |
|---|---|
| `BotCore-Effects.au3` | Identity, health, effect, skillbar read helpers |
| `BotCore-Travel.au3` | Travel wrappers, DP consumable usage |
| `BotCore-Loot.au3` | Pickup flow, chest handling, inventory counting |
| `BotCore-Combat.au3` | Targeting, casting, fight loop |
| `BotCore-SkillRules.au3` | Skill type classification predicates |
| `BotCore-Waypoints.au3` | Waypoint traversal engine |
| `BotCore-RunStats.au3` | Timing, counters, run statistics |

---

## 1. Module Naming and Include Rules

### File naming

Every shared module uses the prefix `BotCore-` followed by a short PascalCase
topic name:

```
BotCore-Effects.au3
BotCore-Combat.au3
BotCore-SkillRules.au3
```

### `#include-once`

Every module file starts with `#include-once` on line 1.

### No direct framework includes

BotCore modules must **never** contain their own `#include` directives for
upstream or custom dependencies. All dependencies resolve through the master
include chain:

```
Froggy_Includes.au3
  -> botshub/GWA2.au3, botshub/Utils.au3, ...
  -> custom/GWA2_Compat.au3, custom/ID_Aliases.au3, ...
  -> custom/BotCore-Effects.au3, custom/BotCore-Combat.au3, ...
```

The master include (`Froggy_Includes.au3`) is responsible for ordering all
modules so that dependencies are satisfied before a module loads. Individual
modules assume their dependencies are already present.

Place a comment at the top of each module confirming this:

```autoit
#include-once
; No #include needed - all dependencies resolve through Froggy_Includes.au3
```

### Include ordering in Froggy_Includes.au3

BotCore modules must be added to `Froggy_Includes.au3` in dependency order:

1. `BotCore-Effects.au3` (no BotCore dependencies)
2. `BotCore-SkillRules.au3` (no BotCore dependencies)
3. `BotCore-RunStats.au3` (no BotCore dependencies)
4. `BotCore-Combat.au3` (depends on Effects, SkillRules)
5. `BotCore-Loot.au3` (depends on Effects)
6. `BotCore-Travel.au3` (depends on Effects)
7. `BotCore-Waypoints.au3` (depends on Combat, Loot)

---

## 2. Function Naming

### Case convention: PascalCase

All public functions use **PascalCase**, matching the existing GWA2 and Froggy
codebase style:

```autoit
Func GetBestTargetPtr(...)
Func CacheSkillBar()
Func IsHealSkill($aSkillID)
```

### No module prefix on function names

Functions are **not** prefixed with their module name. The codebase has an
established convention of short, direct names (`GetHP`, `CanCast`, `Fight`).
Adding prefixes like `BotCombat_Fight` would break compatibility and diverge
from the existing style.

If two modules need a function with the same name, the name must be
disambiguated by making it more specific (e.g., `GetPartyHealth` vs
`GetAgentHealth`), not by adding a module prefix.

### Internal/private helper convention

Functions that are internal to a single module and should not be called from
outside use an underscore prefix:

```autoit
Func _Combat_ResolveTarget($aSkillSlot, $aAggroRange)
```

Format: `_ModuleShortName_DescriptiveName`. These are not part of the public
API and may change without notice.

### Parameter naming

Parameters use the `$a` prefix (for "argument"), following the existing Froggy
convention:

```autoit
Func UseSkillSmart($aSkillSlot, $aTarget = -2, $aTimeout = 6000)
```

Local variables use the `$l` prefix:

```autoit
Local $lBestDist = 99999
Local $lAgent = $lAgentArray[$i]
```

---

## 3. State Containers

### Rule: no naked script-level globals in modules

BotCore modules must not declare loose `Global $SomeVar` variables at file
scope. All module state lives inside a single **Global state array or map**
owned by that module.

### Pattern: one Global associative array per module

Each module that needs persistent state declares exactly one global container
using a Scripting.Dictionary (AutoIt COM map):

```autoit
; --- BotCore-Combat.au3 ---
Global $g_CombatState = ObjCreate("Scripting.Dictionary")
```

Initialize keys in an `_Combat_InitState()` function called once at setup:

```autoit
Func _Combat_InitState()
    $g_CombatState("BestTargetPtr") = 0
    $g_CombatState("SkillBarCache") = __CreateSkillBarCache()
    $g_CombatState("SkillbarSlot")  = __CreateSkillbarSlot()
EndFunc
```

Access from within the module:

```autoit
$g_CombatState("BestTargetPtr") = $lBestPtr
```

Access from outside (e.g., Froggy script reading current target):

```autoit
Local $target = $g_CombatState("BestTargetPtr")
```

### Naming convention for state containers

```
$g_<ModuleShortName>State
```

Examples:

| Module | Container name |
|---|---|
| BotCore-Effects.au3 | `$g_EffectsState` |
| BotCore-Combat.au3 | `$g_CombatState` |
| BotCore-Loot.au3 | `$g_LootState` |
| BotCore-RunStats.au3 | `$g_RunStatsState` |
| BotCore-Waypoints.au3 | `$g_WaypointsState` |

### Modules with no persistent state

`BotCore-SkillRules.au3` is pure-function (stateless classification). It does
not need a state container. Same for `BotCore-Effects.au3` if it remains a
thin read-only wrapper layer.

### Large array state (SkillBarCache, SkillbarSlot)

When a module needs large indexed arrays (like `$SkillBarCache[9][20]`), store
the array as a value inside the state dictionary. The dictionary holds the
reference; array indexing works normally through a local alias:

```autoit
Func CacheSkillBar()
    Local $cache = $g_CombatState("SkillBarCache")
    ; ... populate $cache ...
    $g_CombatState("SkillBarCache") = $cache
EndFunc
```

---

## 4. Callbacks and Policy Injection

### Problem

Several functions contain Froggy-specific behavior (stat GUI updates, pickup
filters, wipe restart rules). Moving them to shared modules requires separating
the **engine** (reusable) from the **policy** (script-specific).

### Pattern: registered callback function names

Modules accept policy callbacks as **string function names** stored in the
module's state dictionary. The module calls them via AutoIt's `Call()`:

```autoit
; --- In Froggy setup ---
$g_LootState("ShouldPickItem") = "Froggy_ShouldPickItem"
$g_LootState("OnItemPickedUp") = "Froggy_OnItemPickedUp"

; --- In BotCore-Loot.au3 engine ---
Func PickupLootEx($aTimeout = 3000)
    ; ...
    Local $fnShouldPick = $g_LootState("ShouldPickItem")
    If $fnShouldPick <> "" Then
        If Not Call($fnShouldPick, $lItem) Then ContinueLoop
    EndIf
    ; ...
EndFunc
```

### Callback naming convention

Script-specific callback implementations use the pattern:

```
<ScriptName>_<CallbackPurpose>
```

Examples:

```autoit
Func Froggy_ShouldPickItem($aItem)
Func Froggy_OnChestFound($aAgent)
Func Froggy_GetRestartWaypoint($aMapId, $aNearestWP, $aLastWP)
Func Froggy_OnRunComplete($aRunTime, $aSuccess)
```

### Default behavior when no callback is registered

If a callback key is empty string `""` or does not exist in the dictionary,
the module uses a safe default (e.g., pick all items, skip chest, restart from
waypoint 0). Modules must never crash when a callback is unregistered.

### Known callback slots by module

| Module | Callback key | Signature | Default |
|---|---|---|---|
| BotCore-Loot | `ShouldPickItem` | `($aItem) -> Bool` | `True` (pick all) |
| BotCore-Loot | `OnItemPickedUp` | `($aItem)` | no-op |
| BotCore-Loot | `OnChestFound` | `($aAgent) -> Bool` | `True` (open) |
| BotCore-Waypoints | `GetRestartWaypoint` | `($aMapId, $aNearestWP, $aLastWP) -> Int` | `0` |
| BotCore-Waypoints | `OnWaypointAction` | `($aLabel, $aIndex) -> Bool` | `False` (skip) |
| BotCore-RunStats | `OnStatsUpdated` | `($aStatsMap)` | no-op |
| BotCore-Combat | `OnFightStart` | `()` | no-op |
| BotCore-Combat | `OnFightEnd` | `()` | no-op |

---

## 5. Compatibility Wrappers

### Problem

Froggy and other scripts already call functions by their current names (e.g.,
`GetBestTargetPtr`, `Fight`, `CanCast`). Moving implementations into BotCore
modules must not break existing call sites.

### Rule: keep the original function name as the public API

When a function moves from Froggy into a BotCore module, **keep the same
function name and signature**. The function simply lives in a different file
now. No wrapper is needed if the name and signature are unchanged.

### When a signature must change

If the extraction requires a new parameter (e.g., an explicit state container),
add it as an **optional parameter with a default**:

```autoit
; Old signature in Froggy:
Func Fight($aAggroRange = 1000, $careful = False)

; New signature in BotCore-Combat.au3 (compatible):
Func Fight($aAggroRange = 1000, $careful = False)
    ; Implementation now reads from $g_CombatState instead of bare globals
EndFunc
```

### When a function is replaced by a better version

If a function is redesigned with a new name (e.g., `PickupLootEx` becomes
`PickupLoot` with callback support), keep the old name as a thin
compatibility wrapper in the **calling script** (Froggy), not in the module:

```autoit
; --- In Froggy_HM_v1.6.au3 (compatibility shim) ---
Func PickupLootEx($aTimeout = 3000)
    PickupLoot($aTimeout)  ; Delegates to BotCore-Loot
EndFunc
```

### Wrapper location rule

- If the wrapper is useful to **all** consumers: put it in the BotCore module.
- If the wrapper exists only to keep **one script** working: put it in that
  script, not in the shared module.

---

## 6. Memory Access

### Rule: always use GWA2_Compat.au3 wrappers

All BotCore modules must use the compatibility wrappers from
`GWA2_Compat.au3` for memory operations. Never call bare `MemoryRead`,
`MemoryWrite`, or `MemoryReadPtr` directly.

| Use this | Not this |
|---|---|
| `MemRead($addr, $type)` | `MemoryRead(GetProcessHandle(), $addr, $type)` |
| `MemWrite($addr, $data, $type)` | `MemoryWrite(GetProcessHandle(), $addr, $data, $type)` |
| `MemReadPtr($addr, $offset, $type)` | `MemoryReadPtr(GetProcessHandle(), $addr, $offset, $type)` |
| `ClearMem()` | `ClearMemory(GetProcessHandle())` |

This ensures that process handle resolution is centralized and that custom
modules do not depend on knowing how the upstream framework manages handles.

---

## 7. Summary Checklist for New BotCore Modules

Before merging a new `BotCore-*.au3` file, verify:

- [ ] File starts with `#include-once`
- [ ] No `#include` directives in the file
- [ ] Comment at top confirms dependency resolution via `Froggy_Includes.au3`
- [ ] File is added to `Froggy_Includes.au3` in correct dependency order
- [ ] Public functions use PascalCase, no module prefix
- [ ] Private helpers use `_ModuleName_FunctionName` pattern
- [ ] Parameters use `$a` prefix, locals use `$l` prefix
- [ ] Module state lives in `$g_<Name>State` dictionary, not bare globals
- [ ] Callbacks use `Call()` with string function names from state dictionary
- [ ] Unregistered callbacks have safe defaults (no crashes)
- [ ] Memory access uses `MemRead`/`MemWrite`/`MemReadPtr` from GWA2_Compat
- [ ] `Au3Check` passes on the module file
- [ ] `Au3Check` passes on `Froggy_HM_v1.6.au3`
