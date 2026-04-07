# GWCA GameThread Repacker-Only Adapter Cluster Addendum

This pass follows the strongest new question created by `FUN_00680140(...)`:

- is `FUN_00680140` a one-off repacker-only edge adapter,
- or does it sit inside a larger pre-front-door cluster that joins the transfer seam from the `FUN_006A5CB0(...)` side only?

Sampled targets:

- `FUN_0067DC00`
- `FUN_0067E2F0`
- `FUN_0067E9C0`
- `FUN_0067F090`
- `FUN_0067F7D0`

## Main Result

`FUN_00680140(...)` is **not** unique.

The range immediately before it is a real repacker-only adapter band:

- `FUN_0067DC00`
- `FUN_0067E2F0`
- `FUN_0067E9C0`
- `FUN_0067F090`
- `FUN_0067F7D0`
- `FUN_00680140`

All six functions:

- call `FUN_006A5CB0(...)`
- do **not** call `FUN_006A5F10(...)`
- build a local `16`-entry working block first
- then join the shared intermediate-family seam from the repacker side only

So the current edge-of-cluster model gets stronger:

- full front-door workers are one major branch
- repacker-only adapters are another nearby branch

## Family Split Inside The Adapter Band

The sampled band already separates into two family groups.

### Family `(2,0)` Repacker-Only Adapters

- `FUN_0067DC00`
- `FUN_0067E2F0`

Both end in:

- `FUN_006A5CB0(local_18, &local_98, local_58, 2, 0)`

Best current reading:

- they rebuild a family-`2`, mode-`0` working block locally
- they do not use the normal compact-family expander
- they combine source-side scalar/color state directly into `local_58[16]`
- but they are not duplicates:
  - `FUN_0067DC00` looks like a raised/max-clamped scalar variant
  - `FUN_0067E2F0` looks like a resident-scalar-preserving variant

### Family `(1,0)` Repacker-Only Adapters

- `FUN_0067E9C0`
- `FUN_0067F090`
- `FUN_0067F7D0`

These end in:

- `FUN_006A5CB0(local_18, &local_90, local_58, 1, 0)`
- `FUN_006A5CB0(local_18, &local_80, local_58, 1, 0)`

Best current reading:

- they rebuild a family-`1`, mode-`0` working block locally
- they also skip the normal `FUN_006A5F10(...)` expander
- but their source-side feeders differ from the family-`2` pair

So this is not just one strange adapter shape reused everywhere.

It is already at least:

- a family-`2`, mode-`0` repacker-only adapter pair
- a family-`1`, mode-`0` repacker-only adapter trio
- plus the family-`3`, mode-`0` example at `FUN_00680140`

## Shared Structural Pattern

Across the sampled functions, the shared structure is:

1. decode or combine source-side payloads directly
2. build a local `16`-entry working block in `local_58[16]`
3. call `FUN_006A5CB0(...)` with a fixed `(block_kind, mode)`
4. write the repacked compact result back out

What is missing compared with the ordinary front-door workers is exactly:

- no `FUN_006A5F10(...)`
- no "expand compact family -> edit -> repack" shape

So the cleanest current interpretation is:

- these are **repacker-only source adapters**

They still target the same intermediate-family toolkit, but they enter after the normal expander stage.

## Why This Matters

This changes the boundary model in a useful way.

Earlier the edge of the cluster looked like:

- main front-door cluster
- one odd source bridge at `FUN_00680140`

Now it reads more like:

- full front-door neighborhood/bridge workers
- repacker-only adapter band
- direct materializers beyond that

That is a much more believable architecture than a single isolated exception.

## Best Current Placement

The current best subtype reading is:

- these still fit under the broad `source bridge worker` idea
- but they are a more specific local class:
  - **repacker-only adapter**

So the subtype layer now has a useful refinement:

- source bridge through full front door
  - example: `FUN_006803F0`
- source bridge through repacker only
  - examples:
    - `FUN_0067DC00`
    - `FUN_0067E2F0`
    - `FUN_0067E9C0`
    - `FUN_0067F090`
    - `FUN_0067F7D0`
    - `FUN_00680140`

## Practical Takeaway

Future triage around this region should now use a three-way split:

1. calls `FUN_006A5F10(...)` and `FUN_006A5CB0(...)`
   - full front-door worker
2. skips `FUN_006A5F10(...)` but calls `FUN_006A5CB0(...)`
   - repacker-only adapter
3. calls neither and writes final pixels directly
   - direct materializer/output worker

That is a stronger and faster rule than grouping by address range alone.

## Best Next Step

The next best task is no longer proving whether repacker-only adapters exist.

That part now looks settled.

The higher-yield next step is to synthesize the whole adapter band one layer higher:

- feeder class
- target family
- local policy variant

That should turn the repacker-only edge from a set of case notes into a compact taxonomy.

## Supporting Artifacts

- `tools/ghidra_projects/gw_findcallers_006a5cb0_temp175.log`
- `tools/ghidra_projects/gw_decomp_repacker_only_cluster_temp177.log`
