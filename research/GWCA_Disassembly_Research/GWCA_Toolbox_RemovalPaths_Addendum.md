# GWCA Toolbox RemovalPaths Addendum

## Scope

This pass continues from:

- [GWCA_Toolbox_RegistrationPaths_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWCA_Toolbox_RegistrationPaths_Addendum.md)

The target here was the inverse side of the public UI callback APIs:

- `RemoveCreateUIComponentCallback @ 0x10026FA0`
- `RemoveFrameUIMessageCallback @ 0x100270A0`
- `RemoveUIMessageCallback @ 0x100271E0`

The goal was to verify whether the public removers really mirror the registration containers we already mapped, and how closely they line up with the bulk module-cleanup callable at:

- `0x100265E0`

## Strongest new result: the three public removers mirror the three registration families almost exactly

Fresh decompilation:

- [decomp_removers_temp6.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_removers_temp6.log)

shows a clean one-to-one correspondence with the earlier registration pass:

### Create-component callbacks

- registration uses one global altitude-sorted vector at:
  - `DAT_1008A428 .. DAT_1008A42C`
- removal linearly scans that same vector for a matching `HookEntry*`
- then compacts the tail forward by one record

### Frame UI-message callbacks

- registration uses the message-bucket map rooted at:
  - `DAT_1008A454`
- removal iterates each message bucket and scans its per-message vector for a matching `HookEntry*`
- then compacts that message-local vector

### Global UI-message callbacks

- registration uses the message-bucket map rooted at:
  - `DAT_1008A434`
- removal looks up the message bucket and scans its vector for a matching `HookEntry*`
- then compacts that message-local vector

So the inverse path is structurally consistent with the registration path.

## `RemoveCreateUIComponentCallback` is a straight vector removal

Decompile target:

- `RemoveCreateUIComponentCallback @ 0x10026FA0`

Recovered behavior:

1. scan the global vector `DAT_1008A428 .. DAT_1008A42C`
2. match on:
   - `record.hook_entry == param_1`
3. if found:
   - shift subsequent `0x30`-byte records left
   - move/copy the embedded callable-holder payload carefully
   - destroy the trailing callable-holder
   - decrement vector end by `0x30`

This is the cleanest family and confirms that create-component callbacks are stored in one global sorted vector with `0x30`-byte records.

## `RemoveFrameUIMessageCallback` is “remove from every message bucket”

Decompile target:

- `RemoveFrameUIMessageCallback @ 0x100270A0`

Recovered behavior:

1. iterate the top-level map/list rooted at:
   - `DAT_1008A458`
2. for each message bucket:
   - read bucket vector begin/end at `+0x0C` / `+0x10`
   - scan `0x30`-byte records for matching `HookEntry*`
3. if a match is found:
   - compact remaining records in that bucket
   - destroy the trailing callable-holder
   - decrement bucket end by `0x30`
4. continue scanning all buckets

That means the frame-side public remover is intentionally broad:

- it does not require the caller to pass a message id
- it removes the `HookEntry*` from whichever frame-message buckets contain it

This lines up with the registration side, where one `HookEntry*` can be inserted into a message-specific bucket after hashing the message id.

## `RemoveUIMessageCallback` supports both specific-message and wildcard removal

Decompile target:

- `RemoveUIMessageCallback @ 0x100271E0`

Recovered behavior:

### If `message_id == 0`

It treats `0` as a wildcard and recursively removes the `HookEntry*` from every message id listed in:

- `DAT_1008A438`

That exactly matches the cleanup strategy we saw earlier in the grouped module-cleanup callable.

### If `message_id != 0`

It:

1. hashes the message id
2. looks up the corresponding message bucket in:
   - `DAT_1008A434`
3. scans that bucket’s `0x30`-byte records for matching `HookEntry*`
4. compacts the bucket vector if found
5. destroys the trailing callable-holder
6. decrements bucket end by `0x30`

So global UI-message removal has two modes:

- targeted removal by one message id
- wildcard removal across all known message ids

## The public removers and the grouped cleanup callable line up cleanly

This pass makes the relationship to `FUN_100265E0` much sharper.

The bulk module-cleanup callable does:

1. for each `HookEntry*` in one module bucket:
   - `RemoveCreateUIComponentCallback(hook_entry)`
   - for each message id in `DAT_1008A438`:
     - `RemoveUIMessageCallback(hook_entry, message_id)`
   - `RemoveFrameUIMessageCallback(hook_entry)`

That now reads as a very deliberate composition of the public removers, not a separate hidden cleanup mechanism.

So the lifecycle story is now fully symmetric:

```text
Register*Callback
  -> family-specific insert
  -> AddHookEntryByModule

bulk cleanup by module
  -> call the same public Remove*Callback helpers
  -> erase module ownership bucket
```

## Shared `0x30` callback-record shape is reinforced again

All three public removers manipulate records in fixed `0x30`-byte steps.

That is consistent with the earlier registration findings and reinforces the shared record model:

```text
struct CallbackRecord {
    int altitude;          // +0x00
    HookEntry* hook_entry; // +0x04
    CallableHolder fn;     // embedded payload, occupying the remaining record body
}
```

The exact callable-holder layout remains compiler-heavy, but the family record size and match field are now very stable.

## Most important behavioral nuance

The biggest nuance from this pass is not just “they remove things.”

It is:

- `RemoveFrameUIMessageCallback(hook_entry)` removes by `HookEntry*` across every frame-message bucket
- `RemoveUIMessageCallback(hook_entry, 0)` removes by `HookEntry*` across every known global UI message id

That makes `HookEntry*` the true callback identity across families, while message id and family determine only **where** the record lives.

This fits the ownership-map model perfectly:

- per-family containers store callback records
- per-module grouping stores `HookEntry*`
- cleanup operates by `HookEntry*`

## Best current interpretation

The strongest conservative reading after this pass is:

- GWCA’s public UI callback removers are genuine inverses of the registration containers
- `HookEntry*` is the cross-family callback identity token
- the bulk module-cleanup callable at `0x100265E0` is simply an orchestrated user of those public removal helpers
- there is no evidence here of a second hidden callback-removal plane beyond the ownership map itself

So the callback system now looks fully coherent at registration, storage, ownership grouping, and removal.

## Best next step

The next logical reverse step is to dive into the shared callback-record allocator/inserter helpers:

- `FUN_10007360`
- `FUN_10005D70`
- and the map helpers around `FUN_10006100` / `FUN_10006200`

That should answer the remaining low-level questions:

1. the exact concrete layout of the `0x30` callback record
2. the exact node shape of the top-level message-bucket maps
3. whether altitude ordering is maintained purely by sorted insertion or by later rebalancing logic
