# GWCA GameThread Bridge Worker Skeleton Relation Addendum

This pass checks whether the two currently known bridge outliers reuse either of the ordinary neighborhood-worker skeletons.

Targets:

- `FUN_006803F0`
- `FUN_0068C310`

The question is not their subtype. That part was already known.

The narrower question is:

- do bridge workers still sit on top of one of the ordinary neighborhood loop bodies,
- or do they break into their own local control structure?

## Main Result

The bridge workers split cleanly.

- `FUN_006803F0` still looks structurally close to the ordinary pointer-walk neighborhood form
- `FUN_0068C310` does **not** reuse either ordinary neighborhood skeleton

So the current best layered model is:

- some source bridges reuse an ordinary neighborhood-style body with a different input-side feeder
- transform bridges can break away into a distinct whole-block transform loop

## `FUN_006803F0`: Bridge On Top Of A Pointer-Walk Body

`FUN_006803F0(...)` still uses:

- `FUN_006A5F10(..., 3, 0)`
- `FUN_006A5CB0(..., 3, 0)`

But its source-side reads come from `ushort` texels expanded through:

- `DAT_00A2C588`

That source-side change matters, but the local loop body still looks close to the ordinary pointer-walk neighborhood form:

- working pointer:
  - `local_80 = local_64`
- row counter:
  - `local_88 = 0`
- working pointer advances by:
  - `local_80 = local_80 + 4`
- row counter advances by:
  - `local_88 = local_88 + 1`

So the best current reading is:

- `FUN_006803F0` is a source bridge worker that still reuses a skeleton-A-like neighborhood body
- what changes is the source feeder, not the broad local loop family

That matches its role well:

- bridge from 16-bit texels
- into family `(3,0)`
- while still behaving like a neighborhood-conditioned intermediate editor

## `FUN_0068C310`: Distinct Transform Loop

`FUN_0068C310(...)` still uses:

- `FUN_006A5F10(..., 3, 2)`
- `FUN_006A5CB0(..., 3, 2)`

But the body is completely different from the ordinary neighborhood workers.

Instead of:

- four-row neighborhood stepping
- neighbor reads through `local_18[]`
- repeated neighborhood blends through the usual body shape

it does:

- block-grid iteration
- local `16`-pixel working buffer:
  - `local_48[16]`
- one full pass over all sixteen working pixels
- coefficient-driven channel remap from `param_3`
- repack back into `(3,2)`

So this is not skeleton A or skeleton B.

It is better understood as:

- a transform-bridge-specific whole-block loop

## Best Current Placement

The current relationship between subtype and body shape now looks like this.

### Neighborhood Edit Worker

- usually lands on ordinary skeleton A or B

### Source Bridge Worker

- can still reuse a neighborhood-style pointer-walk body
- current example:
  - `FUN_006803F0`

### Transform Bridge Worker

- can break away from neighborhood skeletons entirely
- current example:
  - `FUN_0068C310`

That is a useful refinement because it shows subtype is still the right top-level label:

- `FUN_006803F0` is not just "skeleton A with weird inputs"
- `FUN_0068C310` is not just "skeleton B with coefficients"

The subtype tells us what changed in the pipeline.

## Practical Takeaway

The strongest current rule is:

- if a bridge worker still performs row-neighborhood blending and only changes the input feeder, compare it against skeleton A first
- if a bridge worker rewrites all sixteen working pixels through an external transform payload, expect a distinct transform loop instead of either neighborhood skeleton

That gives a cleaner mental model for future outliers:

- ordinary neighborhood workers
- source-bridge adaptations of neighborhood bodies
- transform-loop bridges

## Best Next Step

The next best move is to fold this one layer deeper into the synthesis docs:

- ordinary neighborhood workers:
  - skeleton A / skeleton B
- source bridge workers:
  - neighborhood-derived bridge body
- transform bridge workers:
  - whole-block transform loop

After that, the remaining high-yield task is no longer body-shape sorting.

It is finding whether any additional `0x0067E* .. 0x00680*` outliers introduce:

- another source bridge form
- or another transform-loop form

## Supporting Artifacts

- `tools/ghidra_projects/gw_decomp_mode_cluster_samples_temp170.log`
- `tools/ghidra_projects/gw_decomp_0068c310_temp173.log`
