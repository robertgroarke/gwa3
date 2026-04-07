# GWCA GameThread Family-0 / Family-1 Emitter Split Addendum

This pass follows the new repacker-only outlier opened by:

- `FUN_006848D0 -> FUN_006A5CB0(..., 0, 1)`

The goal was to decide whether `(0,1)` is:

- a completely separate repacker family,
- or a variant lane inside the existing color-table emitter machinery.

## Main Result

`FUN_006A5CB0(...)` does **not** treat `block_kind 0` as a peer of the helper-backed `block_kind 2` and `block_kind 3` paths.

Instead:

- `block_kind 2`
  - uses `FUN_006A53D0(...)` and `FUN_006A5050(...)`
- `block_kind 3`
  - uses `FUN_006A5560(...)` and `FUN_006A5160(...)`
- `block_kind 0` and `block_kind 1`
  - both fall through the same compact color emitter:
    - `FUN_006A5740(...)`

So the new `(0,1)` lane exposed by `FUN_006848D0(...)` is **not** evidence of a fourth helper family parallel to `2` and `3`.

It is better read as:

- a second branch inside the shared color-table emitter used by the lower repack families.

## `FUN_006A5CB0(...)` Split

Fresh decompilation of `FUN_006A5CB0(...)` shows this structure:

1. optional mode-driven working-block rewrite
2. helper-backed repack for:
   - `block_kind 2`
   - `block_kind 3`
3. shared fallback emit through:
   - `FUN_006A5740(...)`

The key tail is:

- if `block_kind == 2`
  - repack scalar nibbles through `FUN_006A53D0(...)`
- else if `block_kind == 3`
  - repack richer scalar ladder through `FUN_006A5560(...)`
- then, for the remaining lower families:
  - call `FUN_006A5740(...)`

The threshold passed into `FUN_006A5740(...)` is:

- `0x80` when `block_kind == 1`
- `0x00` otherwise

Since the call only reaches `FUN_006A5740(...)` for the lower-family fallback path, that gives the strongest current split:

- `block_kind 1`
  - higher-threshold opaque-style color-table emit
- `block_kind 0`
  - zero-threshold mask-selected color-table emit

## What `FUN_006A5740(...)` Appears To Do

Fresh decompilation of `FUN_006A5740(...)` shows a BC1-like color-table emitter over the 16-entry working block.

It:

- selects active pixels by comparing each working pixel alpha/high byte against a threshold
- converts active pixel RGB values into a compact color-space basis
- searches endpoint candidates
- evaluates selector error
- emits two compact endpoint words plus a selector payload

If no pixels meet the threshold, it still emits a degenerate color pair and a full selector mask.

So the safe current reading is:

- `FUN_006A5740(...)` is the shared lower-family compact color-table emitter
- `block_kind 0` vs `block_kind 1` is mainly a threshold / occupancy-policy split inside that emitter

not:

- two wholly unrelated compact emit families.

## Best Current Meaning Of The Split

The strongest current interpretation is:

- `block_kind 1`
  - plain opaque-style color-table family
  - threshold at `0x80`
- `block_kind 0`
  - mask-selected or transparency-capable color-table family
  - threshold at `0x00`

That makes `FUN_006848D0(...)` much less mysterious.

It now reads as:

- byte-packed scalar-plus-color feeder
- one-input local rebuild
- mode-`1` normalization path
- emit through the lower-family threshold-`0` color-table lane

So `(0,1)` is best treated as:

- a lower-family color-table emit variant,
- not an entirely new helper-backed intermediate family.

## Impact On The Repacker-Only Taxonomy

This refines the repacker-only source-bridge side in two ways.

First:

- the earlier `(1,0)` repacker adapters
  - `FUN_0067E9C0`
  - `FUN_0067F090`
  - `FUN_0067F7D0`
  - `FUN_00684100`
  - now read as the opaque-style lower-family lane

Second:

- `FUN_006848D0`
  - is the first mapped adapter into the threshold-`0` lower-family lane

So the repacker-only architecture now spans:

- helper-backed family `2`
- helper-backed family `3`
- opaque-style lower color-table lane
- mask-selected lower color-table lane

## Practical Triage Update

When a new worker reaches `FUN_006A5CB0(...)`:

1. if `block_kind` is `2` or `3`
   - use the helper-pair taxonomy
2. if `block_kind` is `0` or `1`
   - treat it as a shared lower-family color-table emit branch
3. then record:
   - threshold lane
   - mode
   - feeder class

That is now the fastest stable route for lower-family repacker callers.

## Best Next Step

The strongest next step is to look for another `FUN_006A5CB0(..., 0, *)` or `FUN_006A5CB0(..., 1, *)` caller outside the currently mapped adapter set.

That should tell us whether:

- `block_kind 0` is a small special lane used only by `FUN_006848D0`
- or a broader lower-family mask/transparency branch parallel to the already sampled opaque family-`1` callers

## Supporting Artifacts

- `tools/ghidra_projects/gw_decomp_006a5cb0_temp180.log`
- `tools/ghidra_projects/gw_decomp_006a5740_temp181.log`
- `tools/ghidra_projects/gw_decomp_006848d0_temp179.log`
