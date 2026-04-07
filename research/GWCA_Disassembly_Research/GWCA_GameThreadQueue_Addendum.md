# GWCA GameThread Queue Addendum

This pass moves one layer below the deferred lambda families and reverses the queue machinery that actually runs them.

Previously, we had several compiled facts:

- `Keypress(...)` queues a follow-up lambda
- `SetPreference(...)` overloads queue lambda-backed deferred work
- many UI helpers first call `GameThread::IsInGameThread()`

The missing piece was the queue itself.

This pass recovers:

- `GW::GameThread::Enqueue(...)`
- `GW::GameThread::IsInGameThread()`
- the main queued-task dispatcher
- the `0x28`-byte queue-entry copy/grow/free helpers

## Targets

Primary functions:

- `GW::GameThread::Enqueue @ 0x10019D50`
- `GW::GameThread::IsInGameThread @ 0x10019E10`
- queue dispatcher `FUN_10019B00`

Supporting artifacts:

- [decomp_gamethread_temp35.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_gamethread_temp35.log)
- [decomp_gamethread_helpers_temp36.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_gamethread_helpers_temp36.log)

## `IsInGameThread()`

`GW::GameThread::IsInGameThread()` is tiny and important:

```cpp
if (DAT_1008A0B8 == 0) return false;
EnterCriticalSection(&DAT_1008A098);
uVar1 = DAT_1008A0B9;
LeaveCriticalSection(&DAT_1008A098);
return (bool)uVar1;
```

So the game-thread test is not magic TLS lookup in this build.
It is a guarded flag read:

- `DAT_1008A0B8` = queue system initialized/enabled
- `DAT_1008A0B9` = currently executing on the game-thread callback path

## `Enqueue(...)`

The compiled `GW::GameThread::Enqueue(std::function<void()>, bool)` is the most useful result of this pass.

High-level structure:

```cpp
if (DAT_1008A0B8 != 0) {
    EnterCriticalSection(&DAT_1008A098);

    if (!force_queue && DAT_1008A0B9) {
        callable->invoke();
    } else if (queue_end == queue_cap) {
        grow_and_insert(...);
    } else {
        inline_insert(queue_end, temp_callable);
        queue_end += 0x28;
    }

    LeaveCriticalSection(&DAT_1008A098);
}

destroy_temp_callable_if_needed(...);
```

That gives us three crucial properties:

1. `Enqueue(...)` has an in-thread fast path
   - if the caller says “don’t force queue”
   - and GWCA is already in the game-thread execution pass
   - it invokes the callable immediately instead of storing it

2. the singleshot queue stores fixed `0x28`-byte entries
   - `queue_begin = DAT_1008A0BC`
   - `queue_end = DAT_1008A0C0`
   - `queue_cap = DAT_1008A0C4`

3. the function destroys the temporary incoming callable after either:
   - immediate execution
   - or copying/cloning it into the queue entry

That last point is exactly why all the earlier lambda families had the familiar clone/destroy slots.

## Singleshot queue entry shape

The helper functions make the layout much clearer.

### `FUN_10019AA0`: inline copy into one `0x28` entry

This helper initializes one queue entry at the destination pointer:

- zero `entry + 0x24`
- read source callable pointer from `param_1[9]`
- if the source callable is inline/self-owned:
  - clone through virtual slot `+0x04`
  - store the cloned callable at `entry + 0x24`
- otherwise:
  - transfer the pointer directly into `entry + 0x24`
- clear the source slot

So the active callable pointer for a queue entry lives at:

- `entry + 0x24`

### `FUN_10019830`: grow-and-insert helper

This is the vector growth path for the singleshot queue.
It allocates a larger buffer and copies entries of size `0x28`.

The decompile explicitly computes counts using:

- `(...)/0x28`

and grows capacity by the usual vector-style policy before inserting the new item.

### `FUN_100197F0` and `FUN_1001A080`: entry/vector cleanup

These functions walk `0x28`-byte entries and destroy the callable at `+0x24` using virtual slot `+0x10`.

So the singleshot queue is now a concrete storage model:

- contiguous array
- element size `0x28`
- callable pointer at `+0x24`

## Dispatcher: `FUN_10019B00`

This function is the queue runner.

The critical high-level behavior is:

1. enter critical section
2. set `DAT_1008A0B9 = 1`
3. if the singleshot queue is non-empty:
   - swap/move it into a secondary local-owned vector
   - clear the primary queue
   - invoke every queued callable through virtual slot `+0x08`
   - destroy/free the temporary moved vector
4. iterate a second registry of persistent callbacks
5. set `DAT_1008A0B9 = 0`
6. leave critical section

This explains why `Enqueue(...)` can safely short-circuit when already in the game-thread pass:

- `DAT_1008A0B9` is the re-entrancy marker for the dispatcher itself

## Two queue/registry families

One subtle but important result is that `FUN_10019B00` is not just “run the queue.”
It services two different collections:

### Singleshot deferred-call queue

- storage rooted at `DAT_1008A0BC .. DAT_1008A0C4`
- element size `0x28`
- used by ordinary `Enqueue(std::function<void()>, bool)` callers
- examples:
  - `Keypress` release lambda
  - `SetPreference` deferred lambdas

### Persistent game-thread callback registry

- storage rooted at `DAT_1008A0D4 .. DAT_1008A0D8`
- iterated separately after the singleshot queue
- elements appear to be `0x30`-byte records
- each record invokes a callback at offset `+0x2C`

So GWCA’s game-thread machinery is not one queue. It is:

- a singleshot deferred-task queue
- plus a persistent callback list serviced on the same execution pass

## Relationship to the lambda families

This pass closes the loop on the earlier lambda work.

All the deferred families we recovered now fit one real execution path:

1. wrapper builds a stack-local `_Func_impl_no_alloc<lambda_...>`
2. wrapper calls `GameThread::Enqueue(...)`
3. `Enqueue(...)` either:
   - runs it immediately if already on the game-thread pass and not forced
   - or clones/transfers it into a `0x28` singleshot queue entry
4. `FUN_10019B00` later invokes the queued callable through slot `+0x08`
5. cleanup happens through slot `+0x10`

That is exactly why the earlier slot taxonomy kept recurring:

- clone slot
- invoke slot
- destroy slot

## Updated interpretation

The best current model is:

- lambda families are specialization-specific
- but the deferred-execution shell under them is shared
- that shell is the `GameThread::Enqueue(...)` singleshot queue

So the recurring `_Func_impl_no_alloc` pattern is not just a compiler artifact we happen to see often.
It is the concrete data model that GWCA’s game-thread queue is built to accept and manage.

## Most important takeaway

The biggest new result is that we now understand how deferred GWCA work actually executes:

- `IsInGameThread()` is a guarded queue-state flag read
- `Enqueue(...)` is a critical-section protected queue insertion routine with an in-thread fast path
- the singleshot deferred queue stores `0x28`-byte entries
- each entry owns a callable at `+0x24`
- `FUN_10019B00` is the execution pass that flips the “in game thread” flag, drains queued singleshot tasks, then services persistent game-thread callbacks

That means the lambda taxonomy and the queue/storage model now line up end to end instead of living as separate notes.
