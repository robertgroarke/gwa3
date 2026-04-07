# GWCA Toolbox Registration Paths Addendum

## Scope

This pass continues from:

- [GWCA_Toolbox_ModuleOwnership_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWCA_Toolbox_ModuleOwnership_Addendum.md)

The target here was the three public callback registration paths:

- `RegisterCreateUIComponentCallback @ 0x100268F0`
- `RegisterFrameUIMessageCallback @ 0x100269E0`
- `RegisterUIMessageCallback @ 0x10026DC0`

The goal was to confirm:

1. where each path builds its callback record
2. when each path calls `AddHookEntryByModule`
3. what bookkeeping differs between the three callback families

## Strongest new result: all three registration APIs funnel into the same ownership path at the end

Fresh decompilation:

- [decomp_regpaths_temp5.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_regpaths_temp5.log)

shows the same terminal pattern in all three functions:

1. normalize/remove any existing callback state as needed
2. find or create the family/message bucket
3. clone the incoming `std::function` into local SBO-style storage
4. insert a sorted record into the family container
5. destroy the temporary callable clone
6. call:
   - `FUN_100251d0((uint)hook_entry)`

That means the ownership bookkeeping is not interleaved deep inside the container logic.
It is attached at the **end** of successful registration for all three APIs.

## `RegisterCreateUIComponentCallback` is the simplest path

Decompile target:

- `RegisterCreateUIComponentCallback @ 0x100268F0`

Recovered behavior:

1. `RemoveCreateUIComponentCallback(hook_entry)`
2. walk the altitude-sorted vector at:
   - `DAT_1008A428 .. DAT_1008A42C`
3. find insertion point by comparing `existing.altitude <= new_altitude`
4. clone the incoming `std::function`
5. build a record:
   - altitude
   - hook entry
   - callable payload
6. insert with `FUN_10007360(...)`
7. destroy the temporary callable clone
8. call `FUN_100251D0(hook_entry)`

So the create-component family is:

- one global altitude-sorted vector
- no per-message hash layer

## `RegisterFrameUIMessageCallback` is a two-level structure

Decompile target:

- `RegisterFrameUIMessageCallback @ 0x100269E0`

Recovered behavior:

1. hash the frame/UI message id
2. look it up in the map rooted at:
   - `DAT_1008A454`
3. if missing:
   - create a per-message node
   - initialize its inner vector at `node + 0x0C`
4. walk that message-specific vector
5. find insertion point by altitude
6. build the callback record:
   - altitude
   - hook entry
   - callable payload
7. insert into the message-specific vector with `FUN_10007360(...)`
8. destroy the temporary callable clone
9. call `FUN_100251D0(hook_entry)`

So the frame-message family is:

- `message_id -> vector<callback_record>`

with altitude ordering inside each message bucket.

## `RegisterUIMessageCallback` mirrors the frame path closely

Decompile target:

- `RegisterUIMessageCallback @ 0x10026DC0`

Recovered behavior:

1. `RemoveUIMessageCallback(hook_entry, message_id)`
2. hash the UI message id
3. look up or create the message bucket in:
   - `DAT_1008A434`
4. initialize the message bucket’s inner vector if absent
5. walk the per-message vector by altitude
6. build the record:
   - altitude
   - hook entry
   - callable payload
7. insert with `FUN_10007360(...)`
8. destroy the temporary callable clone
9. call `FUN_100251D0(hook_entry)`

So the global UI-message family is structurally the same as the frame-message family, but uses a different top-level registry:

- `DAT_1008A434`

instead of:

- `DAT_1008A454`

## Shared callback-record shape

Across all three registration paths, the inserted callback record shape is now much clearer.

At a high level it is:

```text
struct CallbackRecord {
    int altitude;          // sort key
    HookEntry* hook_entry; // owner identity used for removal/cleanup
    CallableHolder fn;     // SBO / erased std::function-style payload
}
```

The exact surrounding container node type differs by family, but the record content pattern is the same.

## Shared callable handling model

All three paths clone the incoming `std::function` the same way:

- inspect `param + 0x24`
- call through the callable-holder vtable if non-null
- store the temporary clone in local stack SBO storage
- insert the record
- call virtual slot `+0x10` to destroy/release the temporary callable object afterward

That strongly reinforces the earlier callable-holder interpretation from:

- [GWCA_Toolbox_ListenerPath_Correction_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWCA_Toolbox_ListenerPath_Correction_Addendum.md)

and shows it is part of the general callback registration machinery, not just the special seeded-listener path.

## Family differences summarized

The three paths differ in one important way:

### Create-component callbacks

- one global altitude-sorted vector
- no message hash bucket

### Frame UI-message callbacks

- top-level map at `DAT_1008A454`
- keyed by frame/UI message id hash
- each message bucket contains an altitude-sorted vector

### Global UI-message callbacks

- top-level map at `DAT_1008A434`
- keyed by UI message id hash
- each message bucket contains an altitude-sorted vector

But despite those structural differences, all three converge on the same ownership bookkeeping:

- `FUN_100251D0(hook_entry)`

## This closes the ownership story end-to-end

Combining this pass with the previous two gives a full lifecycle:

```text
public Register*Callback API
  -> build/insert callback record in family container
  -> AddHookEntryByModule(hook_entry)
      -> resolve hook_entry address to module via range table
      -> append hook_entry into DAT_1008A418[module_handle]

later cleanup callable FUN_100265E0(module_handle)
  -> find DAT_1008A418[module_handle]
  -> remove create-component callbacks
  -> remove UI-message callbacks for known message ids
  -> remove frame-message callbacks
  -> erase module bucket
```

At this point the ownership model is no longer inferred. It is source-and-binary-backed across registration and cleanup.

## Best current interpretation

The strongest conservative reading after this pass is:

- GWCA has three separate UI callback container families
- each family stores altitude-sorted callback records carrying a `HookEntry*` and callable-holder payload
- all successful registrations are grouped by owning module through `AddHookEntryByModule`
- the seeded cleanup path later unwinds those grouped registrations per module

That means GWCA’s UI callback system is best understood as:

- family-specific callback containers
- plus a cross-family module-ownership index

## Best next step

The next logical reverse step is to inspect the shared removal helpers side by side:

- `RemoveCreateUIComponentCallback @ 0x10026FA0`
- `RemoveFrameUIMessageCallback @ 0x100270A0`
- `RemoveUIMessageCallback @ 0x100271E0`

That should let us verify the inverse path with the same level of detail:

1. exactly how each family matches `HookEntry*`
2. whether removal also updates auxiliary registries besides the family container
3. how closely the public removers line up with the bulk module-cleanup path in `0x100265E0`
