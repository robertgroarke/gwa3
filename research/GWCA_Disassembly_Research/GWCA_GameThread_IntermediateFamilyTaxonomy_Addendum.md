# GWCA GameThread Intermediate Family Taxonomy Addendum

This pass finishes the remaining unsampled members in the front-door caller cluster:

- `FUN_0067CF10`
- `FUN_0067D7B0`

Together with the previous two sampling passes, that gives enough coverage to stop accumulating individual examples and state a stable intermediate-family taxonomy.

## Main Result

The strongest result is that the caller cluster now closes cleanly.

The tail functions land exactly where the earlier model predicted:

- `FUN_0067CF10 -> FUN_006A5F10(..., 3, 0)` and `FUN_006A5CB0(..., 3, 0)`
- `FUN_0067D7B0 -> FUN_006A5F10(..., 3, 2)` and `FUN_006A5CB0(..., 3, 2)`

So the broad transfer-worker cluster is not hiding a fourth intermediate family in its tail. It is fleshing out the already-recovered three-family system.

## Final Family Coverage So Far

With the completed sweep, the intermediate-family map now looks like this.

### `block_kind 1`

Best current name:

- plain color-table intermediate family

Observed workers:

- `FUN_00679D70 -> (1, 0)`
- `FUN_0067BDD0 -> (1, 0)`

Current interpretation:

- BC1/DXT1-like color-table working representation
- used by multiple neighborhood worker variants
- no extra scalar companion family in the front-door call itself

### `block_kind 2`

Best current name:

- nibble-seeded scalar/intensity companion family

Observed workers:

- `FUN_0067A210 -> (2, 1)`
- `FUN_0067C220 -> (2, 1)`
- `FUN_0067C670 -> (2, 0)`

Current interpretation:

- color-table family plus 4-bit scalar/intensity overlay
- front-door expansion and repack confirmed by:
  - `FUN_006A5050(...)`
  - `FUN_006A53D0(...)`

### `block_kind 3`

Best current name:

- endpoint-plus-selector scalar/color companion family

Observed workers:

- `FUN_0067AB50 -> (3, 1)`
- `FUN_0067AFF0 -> (3, 0)`
- `FUN_0067B490 -> (3, 3)`
- `FUN_0067B930 -> (3, 2)`
- `FUN_0067CAC0 -> (3, 1)`
- `FUN_0067CF10 -> (3, 0)`
- `FUN_0067D360 -> (3, 3)`
- `FUN_0067D7B0 -> (3, 2)`
- `FUN_006803F0 -> (3, 0)` with 16-bit source-side bridging

Current interpretation:

- richest and most widely tooled intermediate family
- front-door expansion and repack confirmed by:
  - `FUN_006A5160(...)`
  - `FUN_006A5560(...)`

## Mode Coverage

The current mode map is now stable enough to summarize.

### `block_kind 1`

- observed mode: `0`

### `block_kind 2`

- observed modes: `0`, `1`

### `block_kind 3`

- observed modes: `0`, `1`, `2`, `3`

So the cluster is asymmetrical in a meaningful way:

- `block_kind 3` is the most flexible and most heavily tooled family
- `block_kind 2` has a narrower mode surface
- `block_kind 1` looks simplest and most specialized

That fits what we already learned from the repacker helpers.

## Worker Variant Layer

Just as important, we now have enough evidence to separate:

- family
- mode
- worker variant

Several different functions land on the same `(family, mode)` pair while still having distinct local neighborhood logic.

Examples:

- `FUN_0067A210` and `FUN_0067C220` both use `(2, 1)`
- `FUN_0067AB50` and `FUN_0067CAC0` both use `(3, 1)`
- `FUN_0067AFF0`, `FUN_0067CF10`, and `FUN_006803F0` all use `(3, 0)` but with meaningfully different source-side behavior

So the bank is not just “one function per family/mode.” It is:

- family
- mode
- then one or more worker variants layered on top

That is the clearest evidence yet that this is a real toolkit architecture.

## Best Current Taxonomy

The broadest stable interpretation now is:

1. choose intermediate family
   - plain color
   - nibble scalar/intensity
   - richer endpoint-and-selector scalar/color
2. choose mode
   - alpha policy / normalization / companion interpretation
3. choose worker variant
   - neighborhood blend policy
   - source bridge policy
   - destination stepping/layout policy
4. repack through the shared front door
5. later materialize through the `0x18` output tier

That is a much stronger and cleaner model than the earlier “large callback bank with many similar functions.”

## Relationship To The `0x18` Output Tier

This taxonomy also sharpens the stage boundary:

- the front-door cluster is an **intermediate edit/repack plane**
- the `0x18` family is the **final output/materialization plane**

So we now have a credible staged pipeline:

1. compact source family
2. intermediate 16-entry working representation
3. neighborhood-conditioned worker transform
4. compact re-emission
5. final destination materialization

That is the strongest overall structural result from this branch so far.

## Next Step

The next best move is synthesis rather than more blind sampling:

- build a single crosswalk table for this transfer-bank branch with columns:
  - worker
  - source-side shape
  - `block_kind`
  - mode
  - companion layout family
  - output-stage relation

If you want to keep decompiling instead of synthesizing, the strongest next targets are the still-unsampled caller-side outliers:

- `FUN_0068C310`
- any remaining stage-bridge workers near `0x0067E*..0x00680*`

Those are the likeliest places where the intermediate taxonomy connects into another subsystem boundary.

## Supporting Artifacts

- sampled decompilation output for `FUN_0067CF10`
- sampled decompilation output for `FUN_0067D7B0`
- the earlier family-sampling addenda in this research folder
