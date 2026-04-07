# GWCA Toolbox Container Helpers Addendum

## Scope

This pass continues from:

- [GWCA_Toolbox_RemovalPaths_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWCA_Toolbox_RemovalPaths_Addendum.md)

The target here was the shared helper layer under the callback system:

- `FUN_10007360`
- `FUN_10005D70`
- `_Find_last<> @ 0x10006100`
- `FUN_10006200`
- `FUN_10006690`
- `FUN_10005CD0`

The goal was to pin down:

1. the concrete `0x30` callback-record insertion mechanics
2. the top-level node shape of the message-bucket maps
3. where the vector logic ends and the hash-map logic begins

## Strongest new result: the callback system is two clear layers, not one opaque container

This pass finally separates the data structures cleanly:

### Layer 1: top-level keyed node maps

- `_Find_last<> @ 0x10006100`
- `FUN_10006200`
- `FUN_10006690`

These operate on hash-based node containers keyed by:

- `message_id` for `DAT_1008A434`
- `message_id` for `DAT_1008A454`
- `module_handle` for `DAT_1008A418`

### Layer 2: per-node contiguous vectors

- `FUN_10007360`
- `FUN_10005D70`
- `FUN_10005CD0`

These operate on contiguous arrays:

- `uint32_t` arrays in the module-ownership map
- `0x30` callback-record arrays in the callback family nodes

That is the cleanest structural result so far in this seam.

## `FUN_10005CD0` is the generic contiguous-array allocator

Decompile target:

- `FUN_10005CD0 @ 0x10005CD0`

Recovered behavior:

- allocate `count * 4` bytes
- small allocations use plain `operator_new(size)`
- larger allocations use aligned overallocation with a back-pointer stored at `aligned_ptr - 4`

So this helper is the low-level allocator for contiguous arrays measured in 4-byte elements.

It is used by both:

- the `vector<uint32_t>`-style module-ownership buckets
- the pointer/record arrays managed by higher-level helpers

## `FUN_10005D70` is vector insertion for 4-byte elements

Decompile target:

- `FUN_10005D70 @ 0x10005D70`

Recovered behavior:

1. compute insertion index from `param_1`
2. grow capacity geometrically
3. allocate a new contiguous buffer with `FUN_10005CD0`
4. write the new 4-byte element
5. copy prefix and suffix around the insertion point
6. free old storage
7. update:
   - begin
   - end
   - capacity end

This is the exact helper used for:

- appending `HookEntry*` into `DAT_1008A418[module_handle]`

So the module-ownership buckets are confirmed as ordinary dynamic arrays of 4-byte entries.

## `_Find_last<>` is the bucketed hash-map probe

Decompile target:

- `_Find_last<> @ 0x10006100`

Recovered behavior:

1. compute bucket slot:
   - `bucket_index = hash & mask`
2. fetch bucket head/tail from the bucket table at `this + 0x0C`
3. walk collision-chain/list nodes
4. compare node key at `node + 0x08`
5. return:
   - predecessor/current node pointer
   - found/not-found flag

That means the top-level callback maps are real bucketed hash containers with linked node storage, not trees.

This matches the registration-side hashing we already observed.

## `FUN_10006200` is “find or create node by 32-bit key”

Decompile target:

- `FUN_10006200 @ 0x10006200`

Recovered behavior:

1. hash the 32-bit key
2. call `_Find_last<>`
3. if found:
   - return the existing node pointer and “not newly created”
4. if missing:
   - allocate a node of size `0x18`
   - store key at `node + 0x08`
   - zero the payload region at:
     - `node + 0x0C`
     - `node + 0x10`
     - `node + 0x14`
   - possibly trigger rehash/growth
   - splice node into list and bucket table
   - return the new node and “newly created”

This gives the top-level message-bucket node shape much more explicitly:

```text
struct HashNode32 {
    HashNode32* list_prev_or_link; // +0x00
    HashNode32* list_next_or_link; // +0x04
    uint32_t    key;               // +0x08
    void*       vec_begin;         // +0x0C
    void*       vec_end;           // +0x10
    void*       vec_cap;           // +0x14
};
```

The exact list-link semantics are still STL-heavy, but the payload region is now clear:

- each node owns one inner vector

## `FUN_10006690` is the ordered-map “find or create bucket by key”

Decompile target:

- `FUN_10006690 @ 0x10006690`

Recovered behavior:

1. `_Find_lower_bound<>(this, ..., &key)`
2. if key not present:
   - allocate a node of size `0x20`
   - write `node + 0x10 = key`
   - zero `node + 0x14 .. +0x1C`
   - initialize internal links from the container root
   - insert via `FUN_10002FA0(...)`
3. return:
   - `node + 0x14`

This is the producer helper used for:

- `DAT_1008A418`

So `DAT_1008A418` is not the same hash container type as the message-id maps.
It is an ordered associative container keyed by:

- module handle

and each node’s payload beginning at `+0x14` is again a vector triple.

That sharpens the ownership-map model considerably:

```text
DAT_1008A418:
    ordered map<module_handle, vector<HookEntry*>>
```

not:

- a hash map identical to the message registries

## `FUN_10007360` is the `0x30` callback-record vector inserter

Decompile target:

- `FUN_10007360 @ 0x10007360`

Recovered behavior:

1. read vector `begin/end/cap` from `this`
2. if full:
   - fall through to `FUN_10005E90(...)` growth path specialized for `0x30` records
3. if inserting at end:
   - copy-construct the incoming record into place with `FUN_10006480(...)`
   - advance end by `0x30`
4. if inserting in the middle:
   - clone the incoming record into temporary local storage
   - move the tail one record to the right
   - shift records backward by `0x30`
   - reconstruct/destroy embedded callable-holder payloads carefully
   - write the new record into the insertion slot

This is the concrete proof that callback records are fixed-size `0x30` objects managed by a dedicated vector insertion routine, not just generic 4-byte vector logic.

## The callback record layout is now materially better defined

From the insertion/removal helpers together, the callback record shape is now much tighter:

```text
struct CallbackRecord30 {
    int32_t    altitude;      // +0x00
    HookEntry* hook_entry;    // +0x04
    uint8_t    pad_or_field[4]; // +0x08, copied with the header
    CallableHolder fn;        // starts at +0x0C, SBO-aware, destructor via vtable +0x10
}; // size 0x30
```

The exact meaning of `+0x08` is still not fully named, but we can say with confidence:

- the record header is copied as plain scalar data
- the callable-holder lives in the tail region and requires custom move/destroy logic

## Updated structure map

After this pass, the UI callback system looks like:

### Create-component callbacks

- one global vector of `CallbackRecord30`

### Global UI-message callbacks

- hash map:
  - `message_id -> HashNode32`
- each node owns:
  - vector of `CallbackRecord30`

### Frame UI-message callbacks

- hash map:
  - `message_id -> HashNode32`
- each node owns:
  - vector of `CallbackRecord30`

### Module ownership map

- ordered map:
  - `module_handle -> vector<HookEntry*>`

This is the clearest whole-system storage model we have recovered so far.

## Best current interpretation

The strongest conservative reading after this pass is:

- callback storage and ownership storage are deliberately separate
- message registries use hashed keyed nodes whose payloads are callback-record vectors
- module ownership uses an ordered keyed map whose payloads are `HookEntry*` vectors
- `0x30` callback records have a scalar header and a callable-holder tail
- the public registration/removal APIs sit on top of these containers in a very regular way

That means GWCA’s UI callback system is no longer just behaviorally understood.
Its underlying storage model is now substantially recovered too.

## Best next step

The next logical reverse step is to dig one layer deeper into the callback-record constructor/mover helpers used by `FUN_10007360`, especially:

- `FUN_10006480`
- `FUN_10006600`
- `FUN_10005E90`

That should answer the remaining field-level questions:

1. the exact meaning of the `+0x08` header slot in the `0x30` record
2. whether the callable-holder always starts at `+0x0C`
3. how record growth differs from in-place insertion for the `0x30` callback vectors
