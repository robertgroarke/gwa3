# GWCA GameThread Repacker-Only Feeder Shapes Addendum

This pass refines the repacker-only adapter band one layer deeper.

The open question was:

- are those adapters mainly grouped by the family they target,
- or by the source-side payload shape they decode before calling `FUN_006A5CB0(...)`?

Targets:

- `FUN_0067DC00`
- `FUN_0067E2F0`
- `FUN_0067E9C0`
- `FUN_0067F090`
- `FUN_0067F7D0`
- `FUN_00680140`

## Main Result

The repacker-only band is organized by **both** target family and feeder shape.

The cleanest current split is:

- family-`2`, mode-`0` adapters fed by paired family-`2`-like compact records
- family-`1`, mode-`0` adapters fed by two different opaque-color feeder shapes
- family-`3`, mode-`0` adapter fed by raw `ushort` texels

So the band is not just:

- "everything that skips `FUN_006A5F10(...)`"

It is already a small adapter taxonomy of its own.

## Feeder Group A: Paired Family-`2`-Like Compact Records

Functions:

- `FUN_0067DC00`
- `FUN_0067E2F0`

Shared source-side shape:

- both source and resident records are `16`-byte compact blocks
- each side carries:
  - two `uint` control/scalar words
  - two `ushort` `5:6:5` endpoints embedded in the next two dwords
  - one final selector/control word

Binary signs:

- source side read from `uint *param_5`
- resident side read from `uint *param_1`
- the functions decode two endpoint pairs:
  - one from the source record
  - one from the resident record
- both build `local_58[16]`
- both end in:
  - `FUN_006A5CB0(..., 2, 0)`

Best current reading:

- these are family-`2` adapter/merge workers
- they combine a source family-`2`-like compact block with a resident family-`2`-like compact block
- then repack back into family `(2,0)`
- but they are not duplicates:
  - `FUN_0067DC00` looks like a raised/max-clamped scalar variant
  - `FUN_0067E2F0` looks like a resident-scalar-preserving variant

## Feeder Group B: Family-`2`-Like Source Into Opaque Family-`1`

Functions:

- `FUN_0067E9C0`
- `FUN_0067F090`

Shared source-side shape:

- source side is still a `16`-byte compact record read from `uint *param_5`
- resident side is a smaller opaque-color block read from `ushort *param_1`
- resident layout is effectively:
  - two `ushort` endpoints
  - one `uint` selector word

Binary signs:

- source side reads:
  - `*local_70`
  - `local_70[1]`
  - `(ushort)local_70[2]`
  - `*(ushort *)((int)local_70 + 10)`
  - `local_70[3]`
- resident side reads:
  - `*local_6c`
  - `local_6c[1]`
  - `*(uint *)(local_6c + 2)`
- local opaque palette entries carry:
  - `0xff000000`
- both end in:
  - `FUN_006A5CB0(..., 1, 0)`

Best current reading:

- these are down-bridges into the plain opaque color-table family
- they adapt a richer compact source payload into a family-`1`, mode-`0` output
- the resident side already looks like an opaque BC1/DXT1-like block
- but they are not duplicates:
  - `FUN_0067E9C0` looks like a source-weighted one-sided blend
  - `FUN_0067F090` looks like a two-sided selector-weighted crossblend

So this pair is best thought of as:

- family-`2`-like source feeder
- family-`1` opaque output target

## Feeder Group C: Byte-Packed Alpha/Color Payload Into Opaque Family-`1`

Function:

- `FUN_0067F7D0`

Distinct source-side shape:

- source is read from `byte *param_5`
- it carries:
  - two leading scalar/alpha bytes
  - two `uint3` selector runs at offsets `+2` and `+5`
  - `5:6:5` color endpoints at `+8` and `+10`
  - an additional dword at `+0xc`

Binary signs:

- `uVar12 = (uint)*pbVar9`
- `uVar7 = (uint)pbVar9[1]`
- `local_6c = (uint)*(uint3 *)(pbVar9 + 2)`
- `local_7c = (uint)*(uint3 *)(pbVar9 + 5)`
- `uVar5 = *(ushort *)(pbVar9 + 8)`
- `uVar4 = *(ushort *)(pbVar9 + 10)`

This is visibly different from the `uint *param_5` feeder used by the previous two family-`1` adapters.

Best current reading:

- `FUN_0067F7D0` is still a family-`1`, mode-`0` repacker-only adapter
- but its feeder is a more byte-packed scalar-plus-color payload
- it looks closer to a DXT5-style alpha/color compact block than to the plain `uint`-word feeder used by `FUN_0067E9C0` / `FUN_0067F090`

So family `(1,0)` already has **two** feeder classes inside the repacker-only band:

- `uint`-word compact feeder
- byte-packed alpha/color feeder

## Feeder Group D: Raw `ushort` Texels Into Family-`3`

Function:

- `FUN_00680140`

Source-side shape:

- raw `ushort` texels
- expanded through `DAT_00A2C588`
- assembled directly into a local `16`-entry working block

Best current reading:

- this is the family-`3`, mode-`0` repacker-only bridge
- unlike the earlier adapters, it does not begin from another compact block record

So `FUN_00680140` sits at the far source-normalization end of the repacker-only band.

## Best Current Taxonomy

The repacker-only band now separates like this:

- target family `(2,0)`
  - paired family-`2`-like compact feeder
- target family `(1,0)`
  - richer `uint`-word compact feeder
  - byte-packed alpha/color feeder
- target family `(3,0)`
  - raw `ushort` texel feeder

That is enough evidence to say the band is not just arranged by target family.

The source encoding also matters.

## Practical Takeaway

When a new repacker-only adapter shows up, the fastest next question is now:

1. what does the feeder look like?
2. what `(block_kind, mode)` does it repack into?

That should classify it faster than trying to infer intent from constants or address order alone.

## Best Next Step

The next best synthesis task is to summarize the full repacker-only adapter band by:

- feeder class
- target family
- local policy variant

That should close the adapter-side taxonomy cleanly before jumping to another pocket.

## Supporting Artifacts

- `tools/ghidra_projects/gw_decomp_repacker_only_cluster_temp177.log`
- `tools/ghidra_projects/gw_decomp_00680140_00680e20_temp176.log`
