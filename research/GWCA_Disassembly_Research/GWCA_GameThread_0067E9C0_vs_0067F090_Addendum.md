# GWCA GameThread 0067E9C0 vs 0067F090 Addendum

This pass resolves the tightest remaining sibling split inside the repacker-only adapter band:

- `FUN_0067E9C0`
- `FUN_0067F090`

They already appeared to share:

- the same broad feeder class
- the same target family:
  - `FUN_006A5CB0(..., 1, 0)`
- the same repacker-only adapter role

So the real question was:

- why are there two workers at all?

## Main Result

The split is not the feeder class and not the target family.

It is the **pixel-combination policy** inside the local `16`-entry rebuild.

- `FUN_0067E9C0` uses a **source-weighted one-sided blend**
- `FUN_0067F090` uses a **two-sided selector-weighted blend**

So they are best understood as sibling policy variants inside the same feeder family.

## Shared Structure

Both functions:

- read the same broad source feeder shape from `uint *param_5`
- read the same resident opaque family-`1` block shape from `ushort *param_1`
- construct:
  - a source-side palette in `local_b0[4]`
  - a resident opaque palette in `local_a0[4]`
- build a local `16`-entry working block in `local_58[16]`
- repack via:
  - `FUN_006A5CB0(..., 1, 0)`

They also share the same high-level sentinel split:

- all-ones fast path
- all-zero fast path
- otherwise rebuild and repack

So the body split is genuinely inside the local per-pixel blend policy.

## `FUN_0067E9C0`: Source-Weighted One-Sided Blend

Inside `FUN_0067E9C0(...)`, each pixel takes:

- a resident opaque candidate from:
  - `local_a0[local_5c & 3]`
- a source-side color candidate from:
  - `local_b0[...]`
- a scalar weight from the source feeder nibble:
  - `local_78 & 0xf`
  - then later `local_7c & 0xf`

The key point is that the source-side selector stream only chooses which source palette entry to use:

- `local_b0[local_74 & 3]`
- then `local_b0[uVar11 & 3]`

But the actual blend weight comes from the source nibble stream alone:

- `iVar6 = (0xf - local_64) * 0x100`

and the resident side is not given a separate competing selector-derived weight.

So the cleanest reading is:

- source feeder supplies the scalar blend policy
- resident opaque block is the base side being blended against
- source palette choice varies, but source nibble strength drives the mixture

That makes `FUN_0067E9C0` look like:

- a one-sided source-controlled overwrite / coverage-style adapter

## `FUN_0067F090`: Two-Sided Selector-Weighted Blend

Inside `FUN_0067F090(...)`, each pixel uses both sides more symmetrically.

For the first half:

- one weight comes from the source nibble:
  - `local_68 & 0xf`
  - `iVar6 = (0xf - local_78) * 0x100`
- the competing weight comes from the same source-derived nibble in direct form:
  - `iVar7 = local_78 * 0x100`

Then the function separately expands:

- resident opaque candidate from:
  - `local_a0[local_5c & 3]`
- source color candidate from:
  - `local_b0[local_6c & 3]`

and blends both through the lookup table with complementary weights.

The second half repeats the same two-sided pattern:

- `(0xf - local_68)` weight for one side
- `local_68` weight for the other side

So this worker is not just:

- "choose source color, tint resident"

It is:

- choose both palette candidates
- crossfade them using complementary selector-derived weights

That makes `FUN_0067F090` the more symmetric sibling.

## Best Current Difference

The strongest current distinction is:

- `FUN_0067E9C0`
  - source-weighted one-sided blend into opaque family-`1`
- `FUN_0067F090`
  - two-sided selector-weighted crossblend into opaque family-`1`

So the pair now reads less like accidental duplication and more like:

- same feeder class
- same target family
- different blend policy

## Placement In The Adapter Taxonomy

This refines the family-`1`, mode-`0` repacker-only branch into:

- byte-packed alpha/color feeder
  - `FUN_0067F7D0`
- richer compact feeder, one-sided source-weighted policy
  - `FUN_0067E9C0`
- richer compact feeder, two-sided selector-weighted policy
  - `FUN_0067F090`

That is a useful closing step because it shows the separate workers are not just keyed to format family.

They are also keyed to blend policy.

## Practical Takeaway

When two repacker-only adapters share:

- the same feeder class
- the same `(block_kind, mode)`

the next thing to compare is:

- who supplies the per-pixel weight
- whether the other side gets an independent competing weight

That turned out to be the real separator here.

## Best Next Step

The strongest next step is to push one layer higher from this result:

- update the repacker-only synthesis so the family-`1` branch records this policy split explicitly

After that, the next best reverse target is probably the family-`2` pair:

- `FUN_0067DC00`
- `FUN_0067E2F0`

because they now look like the remaining unresolved sibling policy split in the adapter band.

## Supporting Artifacts

- `tools/ghidra_projects/gw_decomp_repacker_only_cluster_temp177.log`
