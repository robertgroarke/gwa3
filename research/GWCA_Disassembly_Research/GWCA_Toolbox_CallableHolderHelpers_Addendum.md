# GWCA Toolbox Callable-Holder Helpers Addendum

## Scope

This pass continues from:

- [GWCA_Toolbox_RecordLayout_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWCA_Toolbox_RecordLayout_Addendum.md)

The target here was the next helper layer below the recovered `0x30` callback record:

- `FUN_10005D30`
- `FUN_10007080`

I also decompiled a few direct callers to see whether this record/callable-holder pattern is local to one registry or shared across families:

- `FUN_100061C0`
- `FUN_10006570`
- `FUN_10007100`
- `RegisterEventCallback`

Fresh logs:

- [decomp_callablehelpers_temp9.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_callablehelpers_temp9.log)
- [findcallers_10007080_temp9.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\findcallers_10007080_temp9.log)

## Strongest new result: `FUN_10005D30` is the record-destructor sweep, and `FUN_10007080` is the owning vector reset/free wrapper

This was the main unknown left from the previous pass.

The decompilation now shows a clean two-level cleanup model:

1. `FUN_10005D30(begin, end)`
   - walks `0x30`-byte callback records
   - reads the active callable slot at `record + 0x2C`
   - if non-null, destroys the callable through virtual slot `+0x10`
   - clears `record + 0x2C`

2. `FUN_10007080(vector_triplet)`
   - calls `FUN_10005D30(begin, end)`
   - frees the backing array
   - zeroes:
     - begin
     - end
     - capacity

So the previous record model survives intact, and the last missing question is answered: `FUN_10005D30` really is the destructor sweep for `CallbackRecord30`.

## `FUN_10005D30`: exact behavior

Recovered body:

```text
void __cdecl FUN_10005d30(undefined4 *begin, undefined4 *end)
```

Behavior:

- if `begin == end`, do nothing
- otherwise, for each element:
  - take `record + 0x2C`
  - if non-null:
    - call virtual slot `+0x10`
    - pass a boolean indicating whether the callable lives externally or inline
  - write `0` back to `record + 0x2C`
- advance by `0x30`

The important detail is the destructor argument:

- for an inline/SBO callable:
  - the boolean is false
- for an external callable:
  - the boolean is true

So `+0x2C` is not just a pointer.
It is the discriminator that tells the erased callable object whether cleanup is destroying an inline clone or an externally allocated implementation.

## `FUN_10007080`: vector-level owner/reset helper

Recovered body:

```text
void __fastcall FUN_10007080(int *vec)
```

This function operates on the normal 3-field vector layout:

- `vec[0]` = begin
- `vec[1]` = end
- `vec[2]` = capacity_end

Behavior:

1. if `begin != 0`:
   - call `FUN_10005D30(begin, end)`
   - free the allocation with `FUN_1002B48D(...)`
2. set:
   - `begin = 0`
   - `end = 0`
   - `capacity_end = 0`

So this is not another callable helper.
It is the owning vector teardown wrapper for arrays of `CallbackRecord30`.

## This cleanup path is shared across callback families, not just one UI registry

`FindCallers` makes that point much stronger.

Known callers of `FUN_10007080` include:

- `RegisterFrameUIMessageCallback`
- `RegisterUIMessageCallback`
- `RegisterEventCallback`
- internal hashed-container cleanup helpers:
  - `FUN_100061C0`
  - `FUN_10006570`
  - `FUN_10007100`

That means the same callback-record/vector/destructor machinery is reused by:

- frame UI message callbacks
- global UI message callbacks
- event callbacks

This is a meaningful upgrade from the previous pass, because it shows the record model is a subsystem-wide storage primitive rather than an artifact of one UI-specific path.

## `RegisterEventCallback` proves the same `0x30` record pattern is reused in `EventMgr`

The decompile of `RegisterEventCallback` is especially useful here.

It follows the same pattern we already saw in the UI registration APIs:

1. remove old callback state for the same `HookEntry*`
2. find or create the event-id bucket
3. clone the incoming `std::function` into temporary local SBO storage
4. insert an altitude-sorted callback record through:
   - `FUN_10007360(...)`
5. destroy the temporary callable clone

This means the callback-record shape:

```text
struct CallbackRecord30 {
    int32_t    altitude;        // +0x00
    HookEntry* hook_entry;      // +0x04
    uint8_t    callable_sbo[0x24]; // +0x08 .. +0x2B
    void*      callable_ptr;    // +0x2C
}; // size 0x30
```

is now strongly supported across at least three manager families:

- `UIMgr` frame callbacks
- `UIMgr` global UI callbacks
- `EventMgr` callbacks

## `FUN_100061C0`, `FUN_10006570`, and `FUN_10007100` clarify the container layering

These helper bodies make the storage hierarchy cleaner:

### `FUN_10006570`

- if a node owns a callback-record vector at `node + 0x0C`:
  - call `FUN_10007080(node + 0x0C)`
- then free the node itself

So this is a per-node destroy helper for map nodes that own callback vectors.

### `FUN_100061C0`

- walks a linked set of such nodes
- for each node:
  - calls `FUN_10007080(node + 0x0C)`
  - frees the node

So this is a higher-level bucket-chain cleanup helper.

### `FUN_10007100`

- removes a range of nodes from a hashed container
- for each removed node:
  - calls `FUN_10007080(node + 3)` which is `node + 0x0C`
  - frees the node
- repairs the bucket links and decrements the container count

So the layering is now very explicit:

- outer hashed/linked container nodes
- inner vector of `0x30` callback records at node offset `+0x0C`
- record-level callable destruction through `FUN_10005D30`

## Updated callable-holder model

The most conservative and useful reading after this pass is:

- `record + 0x08` is the inline callable-holder storage
- `record + 0x2C` is the active callable implementation pointer
- destruction uses virtual slot `+0x10`
- the destructor receives a boolean telling it whether the callable is external or inline
- vectors of these records are owned and reset by `FUN_10007080`
- higher-level hashed map nodes own those vectors at offset `+0x0C`

This means the erased callable object itself really is the heart of the record, and the storage helpers around it are now mostly recovered.

## Best current interpretation

The callback subsystem now looks like this:

1. family-specific hashed/ordered containers
2. bucket/node objects
3. per-node vectors of `CallbackRecord30`
4. `CallbackRecord30` with:
   - altitude
   - `HookEntry*`
   - SBO callable-holder
   - active callable pointer/discriminator
5. shared record/vector cleanup helpers reused by UI and Event registries

That is much closer to a real storage model than just saying GWCA keeps “lists of callbacks.”

## Best next step

The next logical pass is to reverse the callable-holder virtual-family itself rather than more container glue.

The strongest targets now are:

- the vtable methods reached through:
  - virtual slot `+0x04` for clone/copy
  - virtual slot `+0x10` for destruction/reset
- and any small helper that constructs the temporary callable clone before `FUN_10007360(...)`

That should answer the remaining hard question:

- whether GWCA is using one erased-callable implementation with SBO/external modes, or multiple callable-holder classes behind the same interface
