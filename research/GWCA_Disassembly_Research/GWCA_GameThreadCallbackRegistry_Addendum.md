# GWCA GameThread Callback Registry Addendum

This pass continues directly from the game-thread queue work and resolves the second half of the dispatcher:

- the persistent game-thread callback registry

Previously, `FUN_10019B00` showed two phases:

1. drain the singleshot deferred queue
2. iterate a second callback list rooted at `DAT_1008A0D4..DAT_1008A0D8`

This pass recovers the public APIs behind that second list:

- `GW::GameThread::RegisterGameThreadCallback`
- `GW::GameThread::RemoveGameThreadCallback`

## Targets

Primary functions:

- `RegisterGameThreadCallback @ 0x10019E70`
- `RemoveGameThreadCallback @ 0x10019F60`

Supporting artifacts:

- [decomp_gamethread_callbacks_temp37.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_gamethread_callbacks_temp37.log)
- [GameThreadMgr.h](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\toolbox\GWToolboxpp-master\Dependencies\GWCA\Include\GWCA\Managers\GameThreadMgr.h)

## Public API surface

The checked-in header already exposed the shape:

- `RegisterGameThreadCallback(HookEntry*, const GameThreadCallback&, int altitude = 0x4000)`
- `RemoveGameThreadCallback(HookEntry*)`

The binary now confirms how those are stored.

## `RegisterGameThreadCallback`

High-level decompile:

```cpp
if (initialized) {
    EnterCriticalSection(&DAT_1008A098);

    RemoveGameThreadCallback(entry);

    find insertion point while existing.altitude <= new_altitude;

    temp_record.altitude = param_3;
    temp_record.entry = param_1;
    temp_record.callable = clone(callback);

    FUN_10007360(&DAT_1008A0D4, ..., insert_pos, &temp_record);

    destroy temp callable clone;
    LeaveCriticalSection(&DAT_1008A098);
}
```

Important points:

1. the persistent registry is altitude-sorted
   - same general pattern as UI callback registries

2. registration is unique by `HookEntry*`
   - it removes any existing record for the same entry first

3. the stored callback payload is another cloned callable-holder
   - exactly the same MSVC-style clone/destroy model seen elsewhere

4. insertion goes through the shared `FUN_10007360` vector inserter
   - so this registry reuses the same contiguous-record infrastructure as the UI callback families

## Record shape

The compiled registration/removal/dispatch behavior makes the record layout much clearer.

Each persistent game-thread callback record is `0x30` bytes, consistent with the dispatcher’s `+0x0C` stepping:

- `+0x00` = altitude
- `+0x04` = `HookEntry*`
- `+0x08..+0x2B` = inline callable-holder storage
- `+0x2C` = active callable pointer/owner slot

That matches the dispatcher behavior from `FUN_10019B00`, which:

- iterates records in `0x30`-byte steps
- checks the callback at offset `+0x2C`
- invokes it through virtual slot `+0x08`

So this registry is structurally much closer to the UI callback registries than to the `0x28` singleshot queue.

## `RemoveGameThreadCallback`

The compiled remover mirrors the UI-family removers closely:

1. enter the same critical section
2. scan the persistent vector for `record.entry == target_entry`
3. compact subsequent `0x30`-byte records leftward
4. correctly destroy/reclone any moved callable-holder payloads during compaction
5. destroy the trailing record payload
6. shrink the logical end pointer by `0x30`

Two especially important details:

- removal is by `HookEntry*`, not by callback identity
- the mover logic again respects inline-vs-external callable ownership

So the registry is not a loose list of function pointers. It is a proper owning vector of callback records using the same callable-holder conventions as the rest of GWCA.

## Relationship to the dispatcher

This pass closes the second half of `FUN_10019B00`.

The dispatcher’s persistent phase:

```cpp
for each record in DAT_1008A0D4..DAT_1008A0D8 step 0x30:
    assert(record.callback != null)
    local_hook_status = ...
    record.callback->invoke(&local_hook_status)
```

now maps cleanly to the public registration API:

- `RegisterGameThreadCallback` populates the `0x30`-byte record vector
- `RemoveGameThreadCallback` removes from that same vector
- `FUN_10019B00` invokes those records every game-thread pass

That means the game-thread subsystem is now fully split into:

### Singleshot deferred task queue

- rooted at `DAT_1008A0BC..DAT_1008A0C4`
- `0x28`-byte entries
- fed by `Enqueue(std::function<void()>, bool)`
- drained once per dispatcher pass

### Persistent game-thread callback registry

- rooted at `DAT_1008A0D4..DAT_1008A0D8`
- `0x30`-byte records
- fed by `RegisterGameThreadCallback(...)`
- altitude-sorted
- invoked every dispatcher pass until removed

## Comparison to the UI callback system

This is the most useful structural comparison.

The persistent game-thread registry behaves like a simplified callback manager in the same house style as UI/frame/global callbacks:

- altitude-ordered insertion
- `HookEntry*` ownership identity
- fixed-size callback records
- cloned callable payloads
- compaction removal by entry

So GWCA is reusing one broad callback-storage idiom across multiple subsystems:

- UI message callbacks
- frame UI callbacks
- create-component callbacks
- game-thread callbacks

The main difference is that the game-thread callbacks are serviced by the game-thread dispatcher rather than by a UI/frame detour entrypoint.

## Updated game-thread model

After this pass, the compiled model is:

1. wrappers build lambda-backed callables
2. `Enqueue(...)` either:
   - runs them immediately when already in the dispatcher pass and not forced
   - or stores them in the `0x28` singleshot queue
3. `RegisterGameThreadCallback(...)` stores persistent altitude-ordered `0x30` callback records
4. `FUN_10019B00`:
   - marks “in game thread”
   - drains singleshot tasks
   - invokes persistent callbacks
   - clears the flag

That gives us the whole game-thread subsystem in one coherent picture.

## Most important takeaway

The biggest new result is that the second half of the game-thread dispatcher is no longer anonymous.

It is the public `RegisterGameThreadCallback` / `RemoveGameThreadCallback` system, implemented as:

- a persistent altitude-sorted `0x30` callback-record vector
- keyed by `HookEntry*`
- using the same cloned callable-holder pattern as the UI callback families

So the game-thread system now has a clean two-plane model:

- deferred one-shot tasks
- persistent per-frame/per-loop callbacks
