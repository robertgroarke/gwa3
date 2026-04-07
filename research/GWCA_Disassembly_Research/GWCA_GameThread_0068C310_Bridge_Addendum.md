# GWCA GameThread `FUN_0068C310` Bridge Addendum

This pass targets the bridge outlier `FUN_0068C310(...)`, which was the strongest next seam after the transfer-bank crosswalk.

## Main Result

The strongest result is that `FUN_0068C310(...)` does **not** introduce a fourth intermediate family.

Instead, it is a **bridge/transform worker built on top of the existing richest family**:

- `FUN_006A5F10(local_48, local_4c, 3, 2)`
- `FUN_006A5CB0(local_4c, &local_90, local_48, 3, 2)`

So this function lives squarely inside the already-recovered `block_kind 3`, mode `2` branch. What makes it special is not a new family, but a different transform step in between expansion and repack.

## Worker Shape

`FUN_0068C310(int block_stream, int *dims, int *matrix)` iterates over a block grid:

- block count:
  - `(width + 3 >> 2) * (height + 3 >> 2)`

For each block it:

1. expands the source block with:
   - `FUN_006A5F10(..., 3, 2)`
2. loads twelve coefficients from `param_3`
3. walks all sixteen working pixels
4. reads the low three color bytes of each pixel
5. applies three affine-like channel transforms
6. clamps each result to `0 .. 0xFFFFFF`
7. writes transformed low-byte color back while preserving the top byte
8. repacks the block with:
   - `FUN_006A5CB0(..., 3, 2)`

So the skeleton is now very explicit:

- expand family `(3,2)`
- transform all 16 pixels through a coefficient matrix
- repack family `(3,2)`

That makes this function a true intermediate-family bridge, not just another neighborhood blender.

## Transform Semantics

The transform is channel-wise and matrix-like.

For each working pixel:

- extract the low three color channels
- compute three new channels via:
  - `a0 * c0 + a1 * c1 + bias0 + a2 * c2`
  - `b0 * c0 + b1 * c1 + bias1 + b2 * c2`
  - `d0 * c0 + d1 * c1 + bias2 + d2 * c2`
- clamp each result
- rebuild the pixel with:
  - transformed low 24 bits
  - original top-byte preserved

So the best current reading is:

- this is a **color-space / channel-remap bridge over the `block_kind 3, mode 2` intermediate family**

I’m keeping the label slightly conservative because we do not yet know the exact external meaning of the coefficient set, but it is clearly more like a per-block color transform than a plain decode helper.

## Why This Matters

This pass is useful because it tests the current taxonomy at one of the likeliest breakpoints.

The result supports the existing model rather than forcing a rewrite:

- no fourth intermediate family
- no separate hidden repack seam
- one more worker variant over an existing `(family, mode)` pair

That means the current `family + mode + worker variant` model survives contact with one of the weirdest outliers in the caller set.

## Updated Placement

`FUN_0068C310(...)` now fits into the crosswalk like this:

- worker:
  - `FUN_0068C310`
- source-side shape:
  - compact family-`3` record stream over a block grid
- `block_kind`:
  - `3`
- mode:
  - `2`
- companion layout family:
  - endpoint-plus-selector scalar/color family
- stage relation:
  - intermediate edit/repack bridge
- special behavior:
  - per-pixel affine-like color transform before repack

That makes it a stronger cousin of `FUN_006803F0(...)`:

- `FUN_006803F0(...)` bridges from 16-bit source texels into family `(3,0)`
- `FUN_0068C310(...)` bridges from coefficient-driven pixel transforms back into family `(3,2)`

So the bridge sublayer itself now has more than one member.

## Architectural Meaning

The transfer bank can now be described a bit more precisely:

1. some workers are neighborhood-conditioned intermediate editors
2. some workers are source-bridge adapters into an intermediate family
3. some workers are transform bridges over an intermediate family
4. later `0x18` workers are final materializers

That is a much better explanation for why the bank needed so many near-duplicate worker bodies.

## Next Step

The best next step is to update the consolidated crosswalk with the bridge subtype distinction:

- neighborhood edit worker
- source bridge worker
- transform bridge worker
- final materializer

If you want to keep decompiling instead of refining the synthesis, the next strongest target is another outlier around the same upper region, especially any remaining `0x0068*` callers of `FUN_006A5F10(...)` / `FUN_006A5CB0(...)`.

## Supporting Artifacts

- decompilation of `FUN_0068C310`
- earlier caller-cluster crosswalk and taxonomy notes in this research folder
