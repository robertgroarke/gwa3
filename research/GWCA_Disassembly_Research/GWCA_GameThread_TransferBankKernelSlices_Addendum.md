# GWCA GameThread Transfer Bank Kernel Slices Addendum

This pass takes the next step after the static-table-slice result and checks what the broad transfer bank at `0x00A26C14` actually contains.

## Main result

The broad transfer bank is not a generic callback placeholder. Its early slots are real compressed-image / block-expansion kernels, and they line up directly with the same DXT-family tables and helpers we have already been reversing deeper in the image pipeline.

That means the static registry thunks are doing something stronger than “pick a handler object”:

- they build a generic registry record with `FUN_00688720(...)`
- then they patch `record + 0x00` to a concrete kernel-slice entry inside a large shared transfer-method bank

So the registry system is wiring records straight onto real format-family worker methods.

## Bank contents

Dumping pointers from `0x00A26C14` gave this contiguous bank:

- `0x00A26C14 -> FUN_006743A0`
- `0x00A26C18 -> FUN_00674B20`
- `0x00A26C1C -> FUN_00675260`
- `0x00A26C20 -> FUN_00675E80`
- `0x00A26C24 -> FUN_00676A40`
- `0x00A26C28 -> FUN_006775A0`
- `0x00A26C2C -> FUN_006780B0`
- `0x00A26C30 -> FUN_00678A70`
- `0x00A26C34 -> FUN_00678AE0`
- `0x00A26C38 -> FUN_00678B50`
- `0x00A26C3C -> FUN_00678C70`
- `0x00A26C40 -> FUN_00678E40`
- plus further continuation entries through `0x00A26C70`

## What the early slots are doing

### `FUN_006743A0`

This is a heavy per-block reconstruction kernel over rows/tiles:

- walks source and destination by strides from `param_4`, `param_7`, and `param_8`
- treats `0xFFFFFFFF / 0xFFFFFFFF` as a sentinel-style block case
- rebuilds colors through the familiar RGB565 expansion tables:
  - `DAT_00A2C6C8`
  - `DAT_00A2C948`
- blends/interpolates through `DAT_00A283D8`
- packs a temporary `16`-element block buffer
- finalizes through `FUN_006A5CB0(..., ..., local_58, 2, 0)`

So this slot is clearly not a small policy callback. It is a concrete block-family worker.

### `FUN_00674B20`

This is a very close sibling of `FUN_006743A0`, but not identical:

- same broad row/block traversal shape
- same RGB565 expansion and interpolation tables
- same sentinel and zero-block special cases
- same final packer `FUN_006A5CB0(..., ..., local_58, 2, 0)`

The local arithmetic differs enough that it looks like a sibling compressed-family variant rather than the same kernel duplicated by accident.

### `FUN_00675260`

This is the next major family jump:

- byte-oriented source/destination handling instead of the earlier pure `uint*` shape
- still uses the same color-expansion tables and compression-family helpers
- uses `FUN_006A5CB0(...)` with different tail parameters, including `(..., 3, 1)` in the visible path

That strongly suggests the later slots are not just more of the same kernel, but additional family variants with a different block layout / output packing contract.

## Connection back to the static installer thunks

Earlier we already had these static transfer-registry installs:

- `(0x11, 0x11, 0x1) -> 0x00A26C14`
- `(0x11, 0x11, 0x3) -> 0x00A26C18`
- `(0x12, 0x12, 0x1) -> 0x00A26C1C`
- `(0x12, 0x12, 0x3) -> 0x00A26C20`
- `(0x13, 0x13, 0x1) -> 0x00A26C24`
- `(0x13, 0x13, 0x3) -> 0x00A26C28`
- `(0x13, 0x13, 0x5) -> 0x00A26C2C`
- `(0x14, 0x14, 0x1) -> 0x00A26C30`
- `(0x15, 0x15, 0x1) -> 0x00A26C34`
- plus more single-sided forms like `(0x1, 0x0, 0x1) -> 0x00A26C38`

Now that the first bank entries are decompiled, the picture is much stronger:

- the key triple chooses a bank slice
- the slice is a concrete compressed-format transfer kernel
- the later third-key values like `1`, `3`, and `5` likely select sibling variants inside the same format family, not a separate registry subsystem

## Best current interpretation

The broad transfer registry is best described now as:

- a format-keyed static registration layer
- whose records are patched onto a shared bank of real compressed-format worker methods

That is a tighter and more useful model than “callback registry” alone, because it explains why the installed pointers sit inside contiguous method banks and why the early entries look like heavy DXT-family reconstruction code.

## Best next step

The next highest-value move is to keep walking this same bank by family:

- compare `FUN_00675E80`, `FUN_00676A40`, and `FUN_006775A0`
- line them up against the already-known installer triples for `0x12`, `0x13`, `0x14`, and `0x15`
- and determine whether the third selector field (`1 / 3 / 5`) corresponds to concrete variant planes like scalar-only, scalar-plus-companion, or another per-family packing mode

That should let us turn this from “kernel slice bank” into a true transfer-family crosswalk.
