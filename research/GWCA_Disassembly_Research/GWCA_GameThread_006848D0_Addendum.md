# GWCA GameThread 006848D0 Addendum

This pass follows the next non-local `FUN_006A5CB0(...)` caller after `FUN_00684100(...)`:

- `FUN_006848D0`

The goal was to decide whether it is:

- another family-`1`, mode-`0` repacker-only canonicalizer,
- or a genuinely new repacker-only branch shape.

## Main Result

`FUN_006848D0(...)` is another repacker-only worker, but it is **not** just a sibling of `FUN_00684100(...)`.

It:

- does **not** call `FUN_006A5F10(...)`
- builds local working entries directly
- calls:
  - `FUN_006A5CB0(&local_18, &local_b8, local_58, 0, 1)`

So this extends the current taxonomy in an important way:

- the non-local repacker-only branch is not limited to previously mapped target families `(1,0)`, `(2,0)`, and `(3,0)`
- `FUN_006848D0` introduces a currently unsynthesized repack target:
  - `(0,1)`

That makes it the first strong sign that the repacker-only branch reaches outside the earlier three-family transfer-bank crosswalk.

## Structural Shape

`FUN_006848D0(...)` reads a byte-packed feeder:

- two leading scalar bytes
- `uint3` selector runs at offsets `+2` and `+5`
- `5:6:5` color endpoints at `+8` and `+10`
- an additional dword at `+0xc`

It then builds:

- a scalar ladder in `local_b0[9]`
- a four-entry color palette in `local_7c[4]`
- a local `16`-entry working block in `local_58[16]`

and finally repacks through:

- `FUN_006A5CB0(..., 0, 1)`

So the best current structural reading is:

- byte-packed scalar-plus-color feeder
- one-input local block rebuild
- repacker-only emit into a different compact target family/mode than the earlier adapter band

## Relationship To `FUN_0067F7D0`

The closest current relative is:

- `FUN_0067F7D0`

because both use a byte-packed scalar-plus-color feeder.

But the output side is different:

- `FUN_0067F7D0`
  - repacks to `(1,0)`
- `FUN_006848D0`
  - repacks to `(0,1)`

So the feeder class continues, while the target family/mode changes.

That means the new result is not just:

- another byte-packed worker

It is:

- the first evidence that the repacker-only branch can bridge one feeder family into more than one repack target family/mode.

## Best Current Placement

The strongest current placement for `FUN_006848D0(...)` is:

- subtype:
  - source bridge worker
- sub-branch:
  - repacker-only adapter
- feeder class:
  - byte-packed scalar-plus-color feeder
- target family/mode:
  - currently unsynthesized `(0,1)`
- local role:
  - one-input canonicalizing bridge

## Why This Matters

Up to now, the repacker-only taxonomy could still be read as mostly living inside the same three transfer-bank families already mapped by the front-door cluster.

`FUN_006848D0(...)` is the first clear counterexample.

It suggests:

- repacker-only workers may reach additional compact emit formats
- the repacker-only branch is not merely a side entrance into the old family map
- it may be its own broader normalization/emit layer

That is a meaningful architectural shift.

## Practical Takeaway

The current triage rule needs one more refinement:

1. if a worker skips `FUN_006A5F10(...)` but calls `FUN_006A5CB0(...)`
2. do **not** assume it repacks into one of the already mapped `(1,0) / (2,0) / (3,0)` transfer-bank families
3. record the actual `FUN_006A5CB0(..., block_kind, mode)` tail first

That matters now because `FUN_006848D0(...)` already breaks the earlier bounded set.

## Best Next Step

The strongest next step is no longer another local sibling comparison.

It is to understand what `(0,1)` means on the repacker side.

The highest-yield next move is probably:

- trace other callers of `FUN_006A5CB0(..., 0, 1)` if any exist
- or decompile the relevant internal `FUN_006A5CB0` branch for `(0,1)`

That should tell us whether `FUN_006848D0` is opening a new compact-family lane or just a special emit mode inside the existing machinery.

## Supporting Artifacts

- `tools/ghidra_projects/gw_findcallers_006a5cb0_temp175.log`
- `tools/ghidra_projects/gw_decomp_006848d0_temp179.log`
