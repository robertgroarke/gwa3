# GWCA Toolbox Callable Cluster Addendum

## Scope

This pass continues from:

- [GWCA_Toolbox_CallableHolderHelpers_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWCA_Toolbox_CallableHolderHelpers_Addendum.md)

The next logical move was to decompile the small helper cluster immediately adjacent to the recovered record helpers:

- `FUN_100064F0`
- `FUN_10006510`
- `FUN_100065A0`
- `FUN_100065D0`

Fresh log:

- [decomp_callablecluster_temp10.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_callablecluster_temp10.log)

## Strongest new result: `FUN_100065D0` is the single-record callable destructor, and the surrounding cluster is generic ownership cleanup

The important recovery here is `FUN_100065D0`.

It is effectively the single-record form of the destructor logic we already saw in:

- `FUN_10005D30` for record arrays

Recovered body:

```text
void __fastcall FUN_100065d0(int record)
{
    int *callable = *(int **)(record + 0x2c);
    if (callable != 0) {
        callable->vtable[+0x10 / 4](callable != record + 8);
        *(uint32_t *)(record + 0x2c) = 0;
    }
}
```

So the callable-holder contract is now visible in two forms:

- `FUN_100065D0`
  - destroy one `CallbackRecord30`
- `FUN_10005D30`
  - destroy a whole range of `CallbackRecord30`

That is strong confirmation that the `record + 0x08` / `record + 0x2C` model is real and central.

## `FUN_100065D0` sharpens the callable-holder contract

This helper says the following very clearly:

- the inline callable storage lives at:
  - `record + 0x08`
- the active callable implementation pointer lives at:
  - `record + 0x2C`
- destruction always goes through virtual slot:
  - `+0x10`
- the boolean argument means:
  - `false` if the callable pointer is the inline object at `record + 0x08`
  - `true` if the callable is external

So the erased-callable protocol is now visible without any vector/container noise.

That makes `FUN_100065D0` one of the cleanest anchors in the whole callback-storage subsystem.

## `FUN_100064F0` and `FUN_10006510` are generic contiguous-buffer free/reset helpers

These two helpers sit nearby, but they are not additional callable semantics.

### `FUN_100064F0`

Recovered body:

```text
void __fastcall FUN_100064f0(int obj)
{
    if (*(void **)(obj + 4) != 0) {
        FUN_1002B48D(*(void **)(obj + 4));
    }
}
```

This is a very small owned-pointer free helper for a field at `+0x04`.

### `FUN_10006510`

Recovered body:

```text
void __fastcall FUN_10006510(int *vec)
{
    if (vec[0] != 0) {
        free adjusted-or-raw base;
        vec[0] = 0;
        vec[1] = 0;
        vec[2] = 0;
    }
}
```

This is the generic contiguous-buffer reset/free helper for a 3-field vector-style object.

It mirrors the structure of `FUN_10007080`, but without any per-element callback destruction.

So the distinction is now:

- `FUN_10006510`
  - free/reset raw vector storage
- `FUN_10007080`
  - destroy callback records first, then free/reset vector storage

That separation helps keep the callable semantics from bleeding into generic allocator helpers.

## `FUN_100065A0` is the higher-level node-chain cleanup wrapper above `FUN_100061C0`

Recovered body:

```text
void __fastcall FUN_100065a0(undefined4 *obj)
{
    FUN_100061c0(obj, (undefined4 *)*obj);
    FUN_1002B48D((void *)*obj);
}
```

So this helper:

- invokes the linked-node cleanup walk in `FUN_100061C0`
- then frees the root allocation

This reinforces the storage hierarchy from the previous pass:

1. outer linked/hashed nodes
2. each node may own:
   - callback-record vector at `+0x0C`
3. callback-record vectors own:
   - `CallbackRecord30` elements
4. callback records own:
   - erased callable implementations

## Updated cluster model

The helper cluster around `0x10006480..0x100065D0` now separates into three layers:

### Record-aware callable helpers

- `FUN_10006480`
  - move/copy callback record
- `FUN_10006600`
  - assign/overwrite callback record
- `FUN_100065D0`
  - destroy one callback record
- `FUN_10005D30`
  - destroy a range of callback records

### Callback-vector ownership helpers

- `FUN_10007080`
  - destroy records, free array, zero triplet

### Generic container/buffer cleanup helpers

- `FUN_10006510`
  - free raw vector storage, zero triplet
- `FUN_100064F0`
  - free owned pointer field
- `FUN_100065A0`
  - free linked-node root after chain cleanup

That makes the callable-holder-specific logic easier to isolate.

## Most important takeaway

The strongest practical upgrade from this pass is simple:

- `FUN_100065D0` gives us a clean single-record view of the same virtual destroy contract already seen in the bulk helpers

That means the callback-record model is now supported by:

- copy/move helpers
- single-record destroy helper
- range destroy helper
- vector teardown helper

So the erased-callable protocol is no longer inferred from one or two functions.
It is consistent across the whole local lifecycle cluster.

## Best current interpretation

The best conservative reading after this pass is:

- GWCA uses one stable callback-record shape:
  - header
  - inline callable storage
  - active callable pointer/discriminator
- the surrounding helper cluster is mostly generic ownership scaffolding
- the callable-specific behavior itself is concentrated in:
  - clone via virtual slot `+0x04`
  - destroy via virtual slot `+0x10`

That means the remaining hard unknown is no longer record layout.
It is the concrete callable implementation type or types behind those virtual slots.

## Best next step

The next logical reverse step is now narrower and better-defined:

1. identify the concrete object type behind `record + 0x2C`
2. recover the vtable or manager-function family that implements:
   - clone at `+0x04`
   - destroy at `+0x10`
3. compare that against the temporary function-clone path used by:
   - `RegisterFrameUIMessageCallback`
   - `RegisterUIMessageCallback`
   - `RegisterEventCallback`

That should finally tell us whether GWCA is storing:

- one erased-callable class with inline/external modes

or

- several concrete callable-holder classes behind the same record protocol
