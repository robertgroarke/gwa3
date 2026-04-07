# GWCA GameThread Intermediate Family Sampling Addendum

This pass samples several newly surfaced callers of the shared mode front door:

- `FUN_00679D70`
- `FUN_0067A210`
- `FUN_0067BDD0`
- `FUN_006803F0`

The goal was to determine whether the wider caller cluster is mostly:

- more compressed-format families,
- more destination-layout variants,
- or another staging layer built on the same front door.

## Main Result

The strongest result is that the caller cluster is not homogeneous. It already splits into at least three real intermediate families, all using the same `FUN_006A5F10 -> transform -> FUN_006A5CB0` seam:

- `block_kind 1` plain color-table family
- `block_kind 2` nibble-seeded scalar/intensity family
- `block_kind 3` richer scalar-plus-color family

And one sampled later caller shows that the same seam can also be fed from a different source encoding entirely, not just the earlier compact record layouts.

So the front door is not a one-family convenience wrapper. It is a real shared editing substrate for multiple intermediate block representations.

## `FUN_00679D70`: `block_kind 1`, `post_mode 0`

`FUN_00679D70(...)` is structurally the same style of neighborhood worker as the one-sided family, but it calls:

- `FUN_006A5F10(local_68, local_8c, 1, 0)`
- `FUN_006A5CB0(local_28, &local_94, local_68, 1, 0)`

That is important because it proves `block_kind 1` is not dead or theoretical. It is a live family inside the intermediate edit plane.

Since `block_kind 1` in `FUN_006A5F10(...)` is the plain 4-entry color-table expander, the cleanest reading is:

- `FUN_00679D70` is an intermediate neighborhood worker over a plain BC1/DXT1-like color block family

So the cluster definitely includes a color-only intermediate branch, not just upper scalar/alpha families.

## `FUN_0067A210`: `block_kind 2`, `post_mode 1`

`FUN_0067A210(...)` uses:

- `FUN_006A5F10(local_68, local_8c, 2, 1)`
- `FUN_006A5CB0(local_28, &local_94, local_68, 2, 1)`

That makes `block_kind 2` live in the broader caller cluster as well.

Given the earlier repacker work, this is the nibble-seeded scalar/intensity companion family. So `FUN_0067A210` is a real neighborhood/editing worker for that compact family, not just a helper-adjacent oddity.

This is a useful balance point for the architecture:

- `block_kind 1` = plain color-table family
- `block_kind 2` = nibble scalar/intensity family
- `block_kind 3` = richer selector/scalar family

So the front door already spans the three main intermediate families we had inferred.

## `FUN_0067BDD0`: Another `block_kind 1` Branch

`FUN_0067BDD0(...)` also uses:

- `FUN_006A5F10(..., 1, 0)`
- `FUN_006A5CB0(..., 1, 0)`

but its local mixing shape differs from `FUN_00679D70(...)`:

- the neighborhood pointers and stepping differ
- the blend path preserves the existing top-byte state more directly
- and the local loop structure is slightly rearranged

So this looks less like “the same function duplicated” and more like:

- a second worker variant over the same plain color-table intermediate family

That pushes the current model toward:

- one shared family format
- multiple neighborhood/layout worker variants over it

which is exactly the kind of staging/tool split the transfer bank has been hinting at.

## `FUN_006803F0`: Same Front Door, Different Source Side

`FUN_006803F0(...)` is the most interesting sample in this set.

It still uses:

- `FUN_006A5F10(local_68, local_8c, 3, 0)`
- `FUN_006A5CB0(local_28, &local_94, local_68, 3, 0)`

but the incoming neighborhood values are not read as the earlier compact 32-bit block records. Instead it reads **16-bit source texels** and expands them through:

- `DAT_00A2C588`

then blends those values into the working block before re-emitting through the same `block_kind 3` path.

That is a strong architectural result:

- the shared front door is not only for transforming one compact block layout into another
- it can also accept working data derived from a different source encoding and still repack into the richer `block_kind 3` family

So `FUN_006803F0` looks like a bridge between a 16-bit source layout and the scalar-plus-color intermediate family.

## Best Current Family Map

From this sample set, the cleanest current map is:

- `block_kind 1`
  - plain color-table intermediate family
  - seen in `FUN_00679D70`, `FUN_0067BDD0`
- `block_kind 2`
  - nibble scalar/intensity companion family
  - seen in `FUN_0067A210`
- `block_kind 3`
  - richer scalar-plus-color companion family
  - seen in `FUN_0067AB50`, `FUN_0067AFF0`, `FUN_0067B490`, `FUN_0067B930`, `FUN_006803F0`

And beyond that:

- some workers start from the same compact-family records
- some workers start from different source payloads, like 16-bit texels
- but they converge onto the same intermediate 16-entry working representation

That is much stronger than our earlier “shared helper cluster” model.

## Architectural Meaning

The transfer bank now looks even more clearly like a toolkit built around a reusable middle layer:

1. one of several source families is decoded into a 16-entry working block
2. worker-specific neighborhood or layout logic runs
3. the result is repacked into one of the compact companion layouts
4. later output tiers materialize final destination pixels

So the real abstraction is not the compressed format name by itself. It is the engine’s chosen **intermediate block family**.

## Next Step

The best next reverse step is to keep sampling the unsorted caller cluster with that family lens, especially:

- `FUN_0067C220`
- `FUN_0067C670`
- `FUN_0067CAC0`
- `FUN_0067CF10`
- `FUN_0067D360`
- `FUN_0067D7B0`

The key question now is:

- do these mostly add more worker variants over families `1/2/3`
- or do they introduce a fourth intermediate family or another source-bridge path like `FUN_006803F0`

## Supporting Artifacts

- sampled decompilation output for `FUN_00679D70`
- sampled decompilation output for `FUN_0067A210`
- sampled decompilation output for `FUN_0067BDD0`
- sampled decompilation output for `FUN_006803F0`
