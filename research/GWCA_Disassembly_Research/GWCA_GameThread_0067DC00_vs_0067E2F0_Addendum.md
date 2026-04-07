# GWCA GameThread 0067DC00 vs 0067E2F0 Addendum

This pass resolves the remaining sibling split inside the family-`2`, mode-`0` repacker-only adapter pair:

- `FUN_0067DC00`
- `FUN_0067E2F0`

They already shared:

- the same feeder class
- the same target family:
  - `FUN_006A5CB0(..., 2, 0)`
- the same repacker-only adapter role

So the open question was:

- what policy split justifies two workers here?

## Main Result

The difference is in how they treat the scalar nibble lane during the local `16`-entry rebuild.

- `FUN_0067DC00` uses a **raised/max-clamped scalar policy**
- `FUN_0067E2F0` uses a **resident-scalar-preserving policy**

So they are best understood as sibling policy variants inside the same family-`2` feeder class.

## Shared Structure

Both functions:

- read a source compact family-`2`-like record from `uint *param_5`
- read a resident compact family-`2`-like record from `uint *param_1`
- decode two endpoint pairs and two selector/scalar lanes
- build a local `16`-entry working block in `local_58[16]`
- repack through:
  - `FUN_006A5CB0(..., 2, 0)`

They also share the same high-level fast paths:

- all-ones source scalar/control pair
- all-zero source scalar/control pair
- otherwise rebuild and repack

So the true split is inside the scalar merge policy, not the outer control flow.

## `FUN_0067DC00`: Raised / Max-Clamped Scalar Policy

Inside `FUN_0067DC00(...)`, each pixel starts from:

- source scalar nibble:
  - `uVar7 = local_7c & 0xf`
  - later:
    - `uVar7 = local_80 & 0xf`
- resident scalar nibble:
  - `uVar12 = local_70 & 0xf`
  - later:
    - `uVar12 = local_74 & 0xf`

The key branch is:

- `if (uVar12 < uVar7) { uVar12 = uVar7; }`

Then the emitted scalar lane comes from:

- `(&DAT_00A2C3D8)[uVar12]`

So the output scalar nibble is not simply the resident nibble and not simply the source nibble.

It is the stronger of the two.

That makes the best current reading:

- preserve or raise coverage/intensity to the max of source vs resident
- then blend color against that raised scalar decision

So `FUN_0067DC00` looks like the saturating / non-decreasing scalar sibling.

## `FUN_0067E2F0`: Resident-Scalar-Preserving Policy

Inside `FUN_0067E2F0(...)`, the scalar lane is handled differently.

Each pixel takes:

- resident scalar nibble directly:
  - `local_5c = (&DAT_00A2C3D8)[local_6c & 0xf]`
  - later:
    - `local_5c = (&DAT_00A2C3D8)[local_70 & 0xf]`

There is no matching:

- `if (resident < source) resident = source`

step here.

The source nibble still controls the blend weight through:

- `iVar6 = (0xf - (local_78 & 0xf)) * 0x100`
  - and later:
    - `iVar6 = (0xf - (local_7c & 0xf)) * 0x100`

But the scalar nibble emitted into `local_58[]` remains the resident scalar lane.

So the best current reading is:

- source nibble influences how strongly source color blends in
- resident scalar lane is preserved rather than raised

That makes `FUN_0067E2F0` the more conservative sibling.

## Best Current Difference

The strongest current distinction is:

- `FUN_0067DC00`
  - source can raise the final scalar nibble up to the stronger of source/resident
- `FUN_0067E2F0`
  - source affects the color blend, but the final scalar nibble stays resident-driven

So the pair now reads as:

- same feeder class
- same target family
- different scalar merge policy

## Placement In The Adapter Taxonomy

This refines the family-`2`, mode-`0` repacker-only branch into:

- richer compact feeder, raised/max-clamped scalar policy
  - `FUN_0067DC00`
- richer compact feeder, resident-scalar-preserving policy
  - `FUN_0067E2F0`

That is the same kind of result we already saw in the family-`1` branch:

- one feeder class
- multiple local policy variants

## Practical Takeaway

When two repacker-only adapters share:

- the same feeder class
- the same `(block_kind, mode)`

the next thing to compare is not only color mixing.

It is also:

- whether the scalar lane is preserved,
- raised,
- or symmetrically recomputed.

That turned out to be the real separator here.

## Best Next Step

The repacker-only adapter band is now much more internally mapped.

The next best step is probably to push back upward into synthesis:

- summarize the full adapter band by:
  - feeder class
  - target family
  - local policy variant

If you want to stay in raw decompilation instead, the next best target is another non-front-door `FUN_006A5CB0` caller outside this local band, especially one of:

- `FUN_00684100`
- `FUN_006848D0`

to see whether the repacker-only architecture continues beyond the currently mapped pocket.

## Supporting Artifacts

- `tools/ghidra_projects/gw_decomp_repacker_only_cluster_temp177.log`
