# GWCA GameThread Neighborhood Worker Skeletons Addendum

This pass compares several already-classified neighborhood edit workers at the body-shape level.

The goal is narrower than the family/mode taxonomy:

- not "which family or mode is this?"
- but "are the ordinary neighborhood workers themselves built from more than one reusable loop skeleton?"

## Main Result

Yes.

The currently sampled neighborhood edit workers appear to split into at least two practical loop skeletons.

They still belong to the same higher taxonomy:

- front-door intermediate workers
- neighborhood edit subtype
- family + mode chosen by the `FUN_006A5F10(...)` / `FUN_006A5CB0(...)` seam

But their inner neighborhood loops are not all the same.

## Skeleton A: Pointer-Walk Blend Form

This skeleton is currently visible in:

- `FUN_00679D70 -> (1, 0)`
- `FUN_0067A6B0 -> (2, 0)`
- `FUN_0067AB50 -> (3, 1)`

Common shape:

- `FUN_006A5F10(...)`
- working pointer initialized as:
  - `local_84 = local_64`
- inner row counter initialized as:
  - `local_88 = 0`
- working pointer advances by:
  - `local_84 = local_84 + 4`
- row counter advances by:
  - `local_88 = local_88 + 1`
- alpha-preservation logic usually compares against the current working pixel first, then blends in the incoming neighbor

Best current reading:

- this is the simpler pointer-walk formulation of the neighborhood-edit body

## Skeleton B: Indexed Working-Block Form

This skeleton is currently visible in:

- `FUN_0067BDD0 -> (1, 0)`
- `FUN_0067CF10 -> (3, 0)`
- `FUN_0067D7B0 -> (3, 2)`

Common shape:

- `FUN_006A5F10(...)`
- working pointer initialized as:
  - `local_74 = local_64`
- explicit inner index initialized as:
  - `local_84 = 0`
- indexed neighborhood bookkeeping through:
  - `local_70 = local_18 + local_84`
- working pointer still advances by row:
  - `local_74 = local_74 + 4`
- explicit index advances by:
  - `local_84 = local_84 + 1`
- blend path preserves the prior working-pixel top byte more directly:
  - `... + (uVar6 & 0xff000000)`

Best current reading:

- this is a second neighborhood-edit formulation with a more explicit indexed working-block loop

## Why This Matters

This does **not** create a new worker subtype.

Both skeletons are still:

- neighborhood edit workers

The difference is one level lower:

- worker subtype answers what the worker is doing in the pipeline
- skeleton shape answers how that subtype is implemented locally

That is useful because it explains why some ordinary neighborhood workers felt more different than expected even when their seam parameters were already known.

## Relationship To Family And Mode

The current split does not line up one-to-one with family or mode.

Evidence so far:

- skeleton A spans:
  - `block_kind 1`
  - `block_kind 2`
  - `block_kind 3`
- skeleton B spans:
  - `block_kind 1`
  - `block_kind 2`
  - `block_kind 3`

So the best current reading is:

- family and mode do not uniquely determine the neighborhood-loop body
- the engine can layer different worker variants on top of the same family/mode system

That fits the broader transfer-bank model:

- family
- mode
- worker variant

and now, more specifically for neighborhood workers:

- neighborhood skeleton variant

## Practical Use

This gives a better quick read when a new ordinary front-door worker appears.

After classifying:

- stage plane
- worker subtype
- family
- mode

it is now worth asking one more lightweight question:

- does the body look like skeleton A or skeleton B?

That can help compare new workers against the right sibling set immediately.

## Current Provisional Map

### Skeleton A

- `FUN_00679D70`
- `FUN_0067A6B0`
- `FUN_0067AB50`

### Skeleton B

- `FUN_0067BDD0`
- `FUN_0067CF10`
- `FUN_0067D7B0`

This map is still provisional because we have not yet re-checked every neighborhood worker with the same lens.

The newest sweep adds the remaining ordinary neighborhood workers:

- `FUN_0067A210`
- `FUN_0067C220`
- `FUN_0067C670`
- `FUN_0067CAC0`
- `FUN_0067D360`

and they all land in skeleton B.

## Expanded Provisional Map

With the current sweep, the neighborhood-worker split now looks like this.

### Skeleton A

- `FUN_00679D70 -> (1, 0)`
- `FUN_0067A6B0 -> (2, 0)`
- `FUN_0067AB50 -> (3, 1)`

### Skeleton B

- `FUN_0067A210 -> (2, 1)`
- `FUN_0067BDD0 -> (1, 0)`
- `FUN_0067C220 -> (2, 1)`
- `FUN_0067C670 -> (2, 0)`
- `FUN_0067CAC0 -> (3, 1)`
- `FUN_0067CF10 -> (3, 0)`
- `FUN_0067D360 -> (3, 3)`
- `FUN_0067D7B0 -> (3, 2)`

So the current evidence suggests:

- skeleton B is the dominant ordinary neighborhood-worker body
- skeleton A is the narrower minority form in the sampled set

## What This Suggests

The split still does not reduce cleanly to family or mode alone.

Examples:

- `(2, 0)` appears in both skeletons:
  - `FUN_0067A6B0`
  - `FUN_0067C670`
- `(3, 1)` appears in both skeletons:
  - `FUN_0067AB50`
  - `FUN_0067CAC0`
- `(1, 0)` appears in both skeletons:
  - `FUN_00679D70`
  - `FUN_0067BDD0`

So the best current reading is even stronger now:

- local loop skeleton is a real worker-variant choice layered below family and mode
- it is not just a disguised restatement of the family/mode pair

## Best Next Step

The next highest-value move is no longer broad neighborhood sampling.

The neighborhood set is now covered well enough that the better next step is:

- inspect whether source-side outliers like `FUN_006803F0` or transform outliers like `FUN_0068C310` relate to either ordinary neighborhood skeleton

That would turn the current transfer-bank model from:

- family + mode + worker subtype

into:

- family + mode + worker subtype + local loop skeleton

while also showing where the bridge workers stop reusing the ordinary neighborhood bodies.

## Supporting Artifacts

- `tools/ghidra_projects/decomp_00679D70_temp2.log`
- `tools/ghidra_projects/decomp_0067A6B0_temp2.log`
- `tools/ghidra_projects/decomp_0067AB50_temp.log`
- `tools/ghidra_projects/decomp_0067BDD0_temp.log`
- `tools/ghidra_projects/decomp_0067A210_temp2.log`
- `tools/ghidra_projects/decomp_0067C220_temp3.log`
- `tools/ghidra_projects/decomp_0067C670_temp3.log`
- `tools/ghidra_projects/decomp_0067CAC0_temp2.log`
- `tools/ghidra_projects/decomp_0067CF10_temp2.log`
- `tools/ghidra_projects/decomp_0067D360_temp2.log`
- `tools/ghidra_projects/decomp_0067D7B0_temp.log`
