# GWCA GameThread Mode Front-Door Caller Cluster Addendum

This pass follows the two real front-door helpers outward:

- `FUN_006A5F10(...)`
- `FUN_006A5CB0(...)`

The goal was to stop reasoning from a few sampled wrappers and instead identify the wider caller cluster that shares the same intermediate block-editing path.

## Main Result

The strongest result is that `FUN_006A5F10(...)` / `FUN_006A5CB0(...)` are not just used by the four one-sided workers we happened to inspect first.

The caller search shows a wider shared cluster:

- `FUN_00679D70`
- `FUN_0067A210`
- `FUN_0067A6B0`
- `FUN_0067AB50`
- `FUN_0067AFF0`
- `FUN_0067B490`
- `FUN_0067B930`
- `FUN_0067BDD0`
- `FUN_0067C220`
- `FUN_0067C670`
- `FUN_0067CAC0`
- `FUN_0067CF10`
- `FUN_0067D360`
- `FUN_0067D7B0`
- `FUN_006803F0`
- `FUN_0068C310`

So the intermediate mode seam is not a side branch. It is a real shared substrate across a substantial transfer-worker family.

## Why This Matters

This result tightens the architecture in an important way.

Earlier we could say:

- some one-sided workers use `FUN_006A5F10(...)` and `FUN_006A5CB0(...)`

Now we can say something stronger:

- there is a broad transfer-worker cluster built around the same expand/edit/repack front door

That means the engine is very likely standardizing several different compressed-format tool stages onto one shared intermediate representation:

1. expand compact block record into 16-entry working pixels
2. perform worker-specific neighborhood or transform logic
3. repack through the same family-aware front door

So the mode seam is not merely an implementation detail for a few upper-family formats. It is a reusable mid-pipeline editing layer.

## Confirmed High-Signal Members

Several members of this caller cluster are already partially understood:

- `FUN_0067A6B0`
  - earlier one-sided heavy worker
  - uses `FUN_006A5F10(..., 2, 0)`
- `FUN_0067AB50`
  - one-sided heavy worker
  - uses `FUN_006A5F10(..., 3, 1)`
- `FUN_0067AFF0`
  - one-sided heavy worker
  - uses `FUN_006A5F10(..., 3, 0)`
- `FUN_0067B490`
  - one-sided heavy worker
  - uses `FUN_006A5F10(..., 3, 3)`
- `FUN_0067B930`
  - one-sided heavy worker
  - uses `FUN_006A5F10(..., 3, 2)`

That gives us two especially useful anchors:

- `block_kind == 2` is live in the broader cluster, not just theoretical
- `block_kind == 3` is the dominant family in the one-sided upper-format tier

So the current interpretation is becoming more balanced:

- `block_kind 2` is a real shared family
- `block_kind 3` is the richer upper-family workhorse

## Best Current Cluster Reading

The cleanest current model is:

- early/mid transfer workers in the `0x00679D70 .. 0x0067D7B0` region are largely **intermediate block transformers**
- they share the same `FUN_006A5F10 -> transform -> FUN_006A5CB0` seam
- later `0x18`-tier workers in the `0x00681860 .. 0x006833F0` region are **final materializers/output workers**

That means the broad transfer bank is now reading less like one monolithic function family and more like:

- intermediate block-editing cluster
- direct output/materialization cluster
- plus support/size helpers nearby

## New Boundary

This caller map also suggests a practical boundary for future reversing:

- if a worker calls `FUN_006A5F10(...)` and `FUN_006A5CB0(...)`, it probably belongs to the intermediate editing plane
- if a worker instead builds ladders/endpoints locally and writes final destination pixels directly, it likely belongs to the output plane

That is a much better triage rule than relying on static table position alone.

## Next Step

The best next reverse step is to sample the newly surfaced caller families we have not named yet, especially:

- `FUN_00679D70`
- `FUN_0067A210`
- `FUN_0067BDD0`
- `FUN_0067C220`
- `FUN_0067C670`
- `FUN_0067CAC0`
- `FUN_0067CF10`
- `FUN_0067D360`
- `FUN_0067D7B0`
- `FUN_006803F0`

The key question is whether those resolve into:

- more format-specific intermediate families,
- more destination-layout variants,
- or broader staging utilities layered over the same mode seam.

## Supporting Artifacts

- caller search output for `FUN_006A5F10(...)` / `FUN_006A5CB0(...)`
- `tools/ghidra_projects/gw_decomp_mode_workers_temp166.log`
- `tools/ghidra_projects/gw_decomp_transfer_bank_tail2_temp164.log`
