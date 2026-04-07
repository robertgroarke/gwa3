# GWCA Hook Lifecycle Addendum

## Scope

This pass connects the internal hook-toggle core to GWCA's exported lifecycle surface.

The main target functions were:

- `GW::EnableHooks`
- `GW::DisableHooks`
- `GW::Hook::EnableHooks(void *)`
- `GW::Hook::DisableHooks(void *)`
- `GW::MemoryPatcher::EnableHooks`
- `GW::MemoryPatcher::DisableHooks`
- `GW::Initialize`

The goal was to answer a simple question cleanly:

- how do GWCA's exported lifecycle calls map onto the internal hook engine we already reversed?

## Big result

The exported surface is now pretty clean:

- `GW::Hook::EnableHooks(void*)` is just a thin wrapper over `FUN_10029960`
- `GW::Hook::DisableHooks(void*)` is just a thin wrapper over `FUN_10029940`
- those in turn land in the internal toggle core:
  - `FUN_100293b0(target, state)`
  - `FUN_100292f0(state)` for bulk toggles
  - `FUN_10029470(index, state)` to actually write/remove the detour

So the hook lifecycle is not split across multiple unrelated systems. The public wrapper layer is very thin.

## Hook-namespace wrappers

The compiled wrapper bodies are tiny:

```cpp
void __cdecl GW::Hook::DisableHooks(void *param_1)
{
    FUN_10029940((int)param_1);
}

void __cdecl GW::Hook::EnableHooks(void *param_1)
{
    FUN_10029960((int)param_1);
}
```

And we already know:

- `FUN_10029940(param)` -> `FUN_100293b0(param, 0)`
- `FUN_10029960(param)` -> `FUN_100293b0(param, 1)`

So the public hook-control API is effectively:

- `EnableHooks(hook_target_or_null)` -> enable one hook, or all hooks if `null`
- `DisableHooks(hook_target_or_null)` -> disable one hook, or all hooks if `null`

## Top-level `GW::EnableHooks` and `GW::DisableHooks`

These are broader than the hook engine alone.

### `GW::EnableHooks`

Decompile summary:

```cpp
if (!initialized) return;
Hook::EnableHooks(0);
for each registered module:
    if (module->EnableHooks != null) module->EnableHooks();
MemoryPatcher::EnableHooks();
```

This means top-level enabling is a three-part operation:

1. enable all detour-style hooks in the core hook engine
2. run per-module enable callbacks
3. enable the separate memory-patching system

### `GW::DisableHooks`

Decompile summary:

```cpp
Hook::DisableHooks(0);
for each registered module:
    if (module->DisableHooks != null) module->DisableHooks();
MemoryPatcher::DisableHooks();
```

So disable is the mirrored teardown path:

1. disable all detour-style hooks
2. run per-module disable callbacks
3. revert memory patches

## `GW::Initialize`: where the lifecycle starts

The compiled `GW::Initialize` body confirms the larger orchestration pattern:

- modules are registered into a global module list
- `Scanner::Initialize(...)` is called
- `Hook::Initialize()` is called
- then module-specific scans and setup continue

So the lifecycle sequence is:

1. register modules
2. initialize scanner
3. initialize hook subsystem
4. let modules discover/install their hooks
5. later enable/disable them through the exported lifecycle

That matches the UI-module path we already reversed for frame dispatch.

## `GW::MemoryPatcher::EnableHooks` / `DisableHooks`

This is the other important finding from this pass: GWCA has a second patching plane that is distinct from the detour hook engine.

### `MemoryPatcher::EnableHooks`

This walks a global list of patch records:

- `DAT_1008a1f8 .. DAT_1008a1fc`

For each enabled record, it:

- `VirtualProtect`s the target range RWX
- copies bytes from `record[1]` into the live address `record[0]`
- restores protection

The copy routine is:

- `FUN_1002cac0(dst, src, size)`

and a global flag `DAT_10088144` tracks whether memory patches are currently enabled.

### `MemoryPatcher::DisableHooks`

This is the inverse:

- `VirtualProtect`
- copy bytes from `record[2]` back into `record[0]`
- restore protection

So memory patches are structurally different from the detour hook engine:

- detour hooks use replay stubs, relocation metadata, thread suspension, and jump rewriting
- memory patches are direct byte-range replacements with saved original/patched buffers

## Important architectural conclusion

GWCA's runtime instrumentation is not just one hook system.

It is at least two parallel systems:

1. **Detour hook engine**
   - compact `0x2c` hook entries
   - replay-slot pool
   - relocation metadata
   - thread suspension and `EIP` repair
   - enable/disable through `FUN_100293b0` / `FUN_100292f0`

2. **Memory patcher**
   - direct byte-buffer swaps
   - no replay slots
   - no relocation table
   - guarded by `DAT_10088144`

Top-level `GW::EnableHooks` / `DisableHooks` coordinate both.

## Example: Render module

One good compiled example is:

```cpp
void __cdecl GW::Render::EnableHooks(void)
{
    if (DAT_1008a26c != 0) Hook::EnableHooks(DAT_1008a26c);
    if (DAT_1008a298 != 0) Hook::EnableHooks(DAT_1008a298);
}
```

That shows how modules sit on top of the common hook core:

- module stores hook target pointers
- module-level `EnableHooks()` just forwards them into the shared hook engine

This is the same architectural pattern the UI module follows.

## Updated end-to-end lifecycle model

The best compact model now is:

1. `GW::Initialize`
   - register modules
   - initialize scanner
   - initialize hook subsystem
   - let modules discover/install hooks

2. `GW::EnableHooks`
   - enable all detour hooks
   - enable module-specific hook sets
   - enable memory patches

3. runtime operation
   - detour hooks route through callback mediation and replay stubs
   - memory patches directly alter selected code/data bytes

4. `GW::DisableHooks`
   - remove detour hooks
   - run module-specific disable logic
   - restore memory-patched bytes

## Why this matters for the UI-message research

This pass sharpens one important interpretive point:

When we say the UI system is "hooked by GWCA", that can mean two different implementation styles:

- a true detour hook routed through the replay-slot engine
- or a direct memory patch handled by `MemoryPatcher`

For the frame dispatcher and global UI dispatcher, we already know they are on the detour-hook side.

So the UI-message conclusions remain:

- `SendFrameUIMsg` uses the real detour/replay engine
- `SendUIMessage` uses the real detour/replay engine

But the broader GWCA runtime includes a second patching mechanism that can affect other subsystems differently.

## Best next step

The next logical reverse pass is:

1. map the live replay slots around the current frame hook back to concrete hook-table entries
2. catalog which major GWCA subsystems use detour hooks versus memory patches
3. if needed, reverse the memory-patcher record structure the same way we did for the `0x2c` detour entry

At this point, though, the lifecycle story is no longer fuzzy.

The exported GWCA surface, the hook-core internals, and the separate memory-patcher layer now line up cleanly.
