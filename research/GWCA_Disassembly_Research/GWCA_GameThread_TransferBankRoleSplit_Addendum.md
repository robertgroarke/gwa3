# GWCA GameThread Transfer Bank Role Split Addendum

This pass continues walking the broad transfer bank at `0x00A26C14` and answers an important structural question: does the whole bank stay on the same kind of worker, or does it change role partway through?

## Main result

The bank is not uniform.

It splits into at least three layers:

1. full block-family reconstruction kernels
2. narrower allocation/size helpers
3. narrower per-pixel / per-word materialization helpers

So the bank is still a shared family-method bank, but it is not “all top-level callbacks of the same shape.” The static installer is wiring records into a broader toolkit of related transfer methods.

## Middle bank: still full family kernels

The entries immediately after the already-mapped head stay on the same heavy compressed-family seam:

- `FUN_00675E80`
- `FUN_00676A40`
- `FUN_006775A0`
- `FUN_006780B0`

### Common traits

These all still look like real family workers:

- row/block traversal over source and destination with explicit strides
- repeated use of the RGB565 expansion tables:
  - `DAT_00A2C6C8`
  - `DAT_00A2C948`
- repeated use of interpolation table `DAT_00A283D8`
- temporary `16`-element local block buffers
- tail packing through `FUN_006A5CB0(...)`

So the bank is still firmly in the compressed-format worker family at this point.

### Variant split through `FUN_006A5CB0(...)`

The clearest difference across these middle entries is the packing mode passed into `FUN_006A5CB0(...)`:

- `FUN_00675E80` uses `FUN_006A5CB0(..., 3, 1)` and `FUN_006A5CB0(..., 3, 0)`
- `FUN_00676A40` uses `FUN_006A5CB0(..., 3, 0)`
- `FUN_006775A0` also stays on the same broad family shape
- `FUN_006780B0` still has the same table-heavy compressed-family character, but with a more distinct local layout than the earlier siblings

That is useful because it gives us a better working hypothesis for the third selector field in the installer triples:

- it likely chooses sibling packing/materialization variants inside one format family
- not a completely different registry subsystem

## Later bank: role change

After the heavier middle kernels, the bank changes shape.

### `FUN_00678A70` and `FUN_00678AE0`

These are not heavy block reconstructor bodies. They are compact size/stride helpers:

- derive aligned widths from `param_8`
- compare against incoming extents
- either call `FUN_0046D790(total_size)` once
- or call it once per row

So these entries look like allocation / reservation / copy-size helpers rather than format-family reconstruction kernels.

### `FUN_00678B50`, `FUN_00678C70`, `FUN_00678E40`

These look like narrower 16-bit destination materializers/blenders:

- they walk rows and pixels directly instead of rebuilding large temporary block records
- still use alpha/interpolation table `DAT_00A283D8`
- but now write directly into `ushort*` destinations

The three visible flavors differ in destination interpretation:

- `FUN_00678B50`
  - alpha-weighted blend into a 16-bit destination
  - uses direct byte channels from the source word stream
- `FUN_00678C70`
  - expands destination through nibble tables:
    - `DAT_00A2C588`
    - `DAT_00A2CA48`
  - so this looks like a packed low-bit-depth / nibble-oriented 16-bit path
- `FUN_00678E40`
  - expands destination through:
    - `DAT_00A2C6C8`
    - `DAT_00A2CE48`
  - preserves a sign/high-bit style field in the final packed write
  - so this looks like a different 16-bit packed destination family than `FUN_00678C70`

I am deliberately keeping those last destination names provisional until we pin the exact pixel formats, but the role split itself is clear.

## Best current interpretation

The transfer bank now reads as a layered family toolkit:

- early/middle entries:
  - full compressed-family reconstruction workers
- later entries:
  - support methods for sizing and direct 16-bit materialization/blending

That means the static transfer registry probably maps records onto the head/middle family slices first, while adjacent entries in the same bank provide the support methods those family workers rely on.

## Why this matters

This is a cleaner stopping point than treating every function in the bank as a top-level “same kind” callback. It tells us where to focus next:

- if we want the `(format, format, variant)` crosswalk, we should stay on:
  - `FUN_00675E80`
  - `FUN_00676A40`
  - `FUN_006775A0`
  - `FUN_006780B0`
- if we want exact destination pixel-format naming, we should pivot to:
  - `FUN_00678B50`
  - `FUN_00678C70`
  - `FUN_00678E40`

## Best next step

The next highest-value pass is still the family-crosswalk path:

- compare the installed triples for `0x12`, `0x13`, `0x14`, and `0x15`
- line them up against `FUN_00675E80`, `FUN_00676A40`, `FUN_006775A0`, and `FUN_006780B0`
- and determine whether the variant selector (`1 / 3 / 5`) corresponds to concrete block-stream composition differences, especially the scalar-only versus scalar-plus-companion split we already recovered in the DXT-family seam

That should give us the first real transfer-family matrix instead of only a bank-structure map.
