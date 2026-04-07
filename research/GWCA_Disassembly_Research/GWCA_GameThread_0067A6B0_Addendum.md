# GWCA GameThread `FUN_0067A6B0` Addendum

This pass decompiles `FUN_0067A6B0(...)`, which was the most obvious uncategorized front-door caller left after the worker-triage appendix.

## Main Result

The strongest result is that `FUN_0067A6B0(...)` is **not** a new bridge type or a new stage.

It is a **neighborhood edit worker** on the already-known family/mode pair:

- `FUN_006A5F10(..., 2, 0)`
- `FUN_006A5CB0(..., 2, 0)`

So the current taxonomy holds.

This function belongs in:

- front-door intermediate edit/repack plane
- `block_kind 2`
- mode `0`
- neighborhood edit worker

## Worker Shape

The recovered body is structurally very clear.

For each block in the source stream it:

1. expands the current compact record with:
   - `FUN_006A5F10(local_68, local_8c, 2, 0)`
2. walks a local four-row neighborhood through `local_18[]`
3. blends neighboring source pixels into the `16`-entry working block through:
   - `DAT_00A283D8`
4. repacks with:
   - `FUN_006A5CB0(local_28, &local_94, local_68, 2, 0)`
5. writes the compact result back into the output stream

That is exactly the front-door neighborhood-edit skeleton, not a source-bridge or transform-bridge skeleton.

## Why This Matters

This function was a useful test case because it sat in an awkward place:

- known front-door caller
- already mentioned in older notes
- not yet placed in the consolidated synthesis docs

The result strengthens the existing map in two ways.

### 1. `block_kind 2`, mode `0` now has more than one live worker

We already had:

- `FUN_0067C670`

Now we can add:

- `FUN_0067A6B0`

So family `2`, mode `0` is not a one-off edge case. It supports multiple neighborhood-edit worker bodies.

### 2. `FUN_0067A6B0` is the family-`2` sibling of `FUN_0067AB50`

The body of `FUN_0067A6B0(...)` is nearly the same neighborhood-blend skeleton as `FUN_0067AB50(...)`.

The meaningful difference is the seam selection:

- `FUN_0067A6B0`
  - `FUN_006A5F10(..., 2, 0)`
  - `FUN_006A5CB0(..., 2, 0)`
- `FUN_0067AB50`
  - `FUN_006A5F10(..., 3, 1)`
  - `FUN_006A5CB0(..., 3, 1)`

So this pair is a good reminder that many apparent worker siblings differ mainly by:

- family
- mode

rather than by stage identity.

## Best Current Placement

`FUN_0067A6B0(...)` now fits the crosswalk as:

- worker:
  - `FUN_0067A6B0`
- source-side shape:
  - compact record
- `block_kind`:
  - `2`
- mode:
  - `0`
- companion/layout family:
  - nibble scalar/intensity companion family
- worker subtype:
  - neighborhood edit worker
- stage relation:
  - intermediate edit/repack

## Triage Outcome

Running the new triage rules against this function gives a clean result:

1. it calls both `FUN_006A5F10(...)` and `FUN_006A5CB0(...)`
2. the body is local-neighborhood blending, not direct final stores
3. it does not bridge from an unusual source-side encoding
4. it does not apply a whole-block coefficient transform
5. seam parameters resolve to:
   - `block_kind 2`
   - mode `0`

So the correct bucket is:

- neighborhood edit worker

That is exactly the kind of quick classification the triage appendix was meant to support.

## Practical Takeaway

The best current reading is that `FUN_0067A6B0(...)` should be treated as the missing family-`2`, mode-`0` anchor in the one-sided/front-door caller set, not as an exception.

That means future passes should assume:

- weirdness only when the seam breaks
- not when a worker merely sits in a less-discussed bank slot

## Supporting Artifact

- `tools/ghidra_projects/decomp_0067A6B0_temp2.log`
