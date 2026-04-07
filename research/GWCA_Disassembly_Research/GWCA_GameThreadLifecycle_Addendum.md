# GWCA GameThread Lifecycle Addendum

This pass finishes the public lifecycle around the game-thread subsystem by resolving:

- `GW::GameThread::EnableHooks()`
- `GW::GameThread::ClearCalls()`

That closes the loop on the game-thread model we built in the last two passes:

- singleshot deferred queue
- persistent game-thread callback registry
- dispatcher/execution pass

## Targets

Primary functions:

- `EnableHooks@GameThread @ 0x100196C0`
- `ClearCalls@GameThread @ 0x10019CB0`

Supporting artifact:

- [decomp_gamethread_lifecycle_exact_temp39.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_gamethread_lifecycle_exact_temp39.log)

## `GW::GameThread::EnableHooks()`

The compiled body is tiny:

```cpp
if (DAT_1008A0B8 != 0) {
    EnterCriticalSection(&DAT_1008A098);
    Hook::EnableHooks(DAT_1008A0B0);
    LeaveCriticalSection(&DAT_1008A098);
}
```

This confirms three useful things:

1. the game-thread manager has its own hook root/handle
   - stored at `DAT_1008A0B0`

2. the whole subsystem is guarded by the same initialization flag
   - `DAT_1008A0B8`

3. hook enable is serialized through the same critical section as queue and callback mutation
   - `DAT_1008A098`

So the game-thread subsystem is not just “a queue.” It also owns at least one real detour/hook entry whose lifetime is managed under the same lock.

## `GW::GameThread::ClearCalls()`

This is the more revealing lifecycle function.

The compiled body:

```cpp
if (DAT_1008A0B8 != 0) {
    EnterCriticalSection(&DAT_1008A098);

    if (DAT_1008A0C8 != DAT_1008A0CC) {
        FUN_100197F0(DAT_1008A0C8, DAT_1008A0CC);
        DAT_1008A0CC = DAT_1008A0C8;
    }

    if (DAT_1008A0BC != DAT_1008A0C0) {
        FUN_100197F0(DAT_1008A0BC, DAT_1008A0C0);
        DAT_1008A0C0 = DAT_1008A0BC;
    }

    if (DAT_1008A0D4 != DAT_1008A0D8) {
        FUN_10005D30(DAT_1008A0D4, DAT_1008A0D8);
        DAT_1008A0D8 = DAT_1008A0D4;
    }

    LeaveCriticalSection(&DAT_1008A098);
}
```

That gives us a very clean reset map.

## What `ClearCalls()` actually clears

It clears three distinct collections:

### 1. Secondary singleshot buffer

- `DAT_1008A0C8 .. DAT_1008A0CC`

This is the moved-out temporary vector used by the dispatcher while draining queued singleshot tasks.

### 2. Primary singleshot queue

- `DAT_1008A0BC .. DAT_1008A0C0`

This is the ordinary deferred-task queue used by `GameThread::Enqueue(...)`.

### 3. Persistent game-thread callback registry

- `DAT_1008A0D4 .. DAT_1008A0D8`

This is the altitude-sorted `0x30` callback-record vector used by `RegisterGameThreadCallback(...)`.

So `ClearCalls()` is not narrowly “clear queued one-shots.”
It resets both planes of the subsystem:

- deferred one-shot tasks
- persistent callbacks

## Cleanup helpers used

The compiled reset paths line up with the helper work from the previous pass:

- `FUN_100197F0(...)`
  - destroys queued singleshot entries
  - matches the `0x28` entry format with callable at `+0x24`

- `FUN_10005D30(...)`
  - destroys the persistent callback records
  - matches the `0x30` callback-record format used by `RegisterGameThreadCallback(...)`

That is a nice confirmation that the subsystem really is composed of two different storage families, each with its own cleanup primitive.

## Updated subsystem picture

After this pass, the compiled game-thread manager is now coherent end to end:

### Shared state

- `DAT_1008A098` = critical section
- `DAT_1008A0B8` = initialized/enabled flag
- `DAT_1008A0B9` = “currently executing in game-thread dispatcher” flag
- `DAT_1008A0B0` = hook root/handle used by `EnableHooks()`

### Deferred one-shot plane

- `DAT_1008A0BC .. DAT_1008A0C4` = primary `0x28` singleshot queue
- `DAT_1008A0C8 .. DAT_1008A0D0` = moved-out secondary buffer used during dispatch
- fed by `Enqueue(std::function<void()>, bool)`

### Persistent callback plane

- `DAT_1008A0D4 .. DAT_1008A0D8` = `0x30` altitude-sorted callback vector
- fed by `RegisterGameThreadCallback(...)`
- pruned by `RemoveGameThreadCallback(...)`

### Execution/lifecycle

- `FUN_10019B00` runs both planes
- `EnableHooks()` enables the subsystem hook root
- `ClearCalls()` clears all pending/registered work

## Why this matters

This answers an important lifecycle question that was still open after the queue and registry passes:

- are persistent game-thread callbacks “owned separately” from one-shot queue work?

The answer is yes at the storage level, but no at the public reset level:

- they live in separate vectors
- but `ClearCalls()` resets both under one lock

So the intended ownership boundary is subsystem-wide, not queue-only.

## Most important takeaway

The biggest result from this pass is that `ClearCalls()` is a full subsystem reset, not just a deferred-task flush.

It clears:

- the active singleshot queue
- the dispatcher’s moved-out singleshot buffer
- the persistent `RegisterGameThreadCallback(...)` registry

Together with `EnableHooks()`, that gives us the full compiled lifecycle model for GWCA’s game-thread manager:

- initialize/enable hook-backed subsystem
- queue or register work
- execute it through the dispatcher
- clear both planes when needed
