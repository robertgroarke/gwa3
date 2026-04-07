# GWCA GameThread Intermediate Family Variant Sweep Addendum

This pass samples the next cluster of front-door callers:

- `FUN_0067C220`
- `FUN_0067C670`
- `FUN_0067CAC0`
- `FUN_0067D360`

The main question was whether these would introduce a fourth intermediate family, or whether they would turn out to be more worker variants over the three families we already had.

## Main Result

The strongest result is that this batch stays inside the same three-family model.

The sampled functions map cleanly as:

- `FUN_0067C220 -> FUN_006A5F10(..., 2, 1)` and `FUN_006A5CB0(..., 2, 1)`
- `FUN_0067C670 -> FUN_006A5F10(..., 2, 0)` and `FUN_006A5CB0(..., 2, 0)`
- `FUN_0067CAC0 -> FUN_006A5F10(..., 3, 1)` and `FUN_006A5CB0(..., 3, 1)`
- `FUN_0067D360 -> FUN_006A5F10(..., 3, 3)` and `FUN_006A5CB0(..., 3, 3)`

So this pass does **not** reveal a fourth intermediate family. Instead it shows that the bank is building out more worker variants over:

- `block_kind 2`
- `block_kind 3`

with the same mode values we had already started recovering.

## What Changed From The Previous Sample

The useful upgrade here is not “new family,” but **coverage**.

Before this pass, we had only one clear `block_kind 2` sample and a few `block_kind 3` samples. Now we have multiple members in each branch:

### `block_kind 2`

- `FUN_0067A210 -> (2, 1)`
- `FUN_0067C220 -> (2, 1)`
- `FUN_0067C670 -> (2, 0)`

### `block_kind 3`

- `FUN_0067AB50 -> (3, 1)`
- `FUN_0067AFF0 -> (3, 0)`
- `FUN_0067B490 -> (3, 3)`
- `FUN_0067B930 -> (3, 2)`
- `FUN_0067CAC0 -> (3, 1)`
- `FUN_0067D360 -> (3, 3)`
- `FUN_006803F0 -> (3, 0)` with 16-bit source-side bridging

That makes the mode/family structure much more believable as a designed toolkit rather than a few isolated accidents.

## Structural Pattern

All four new samples follow the same broad skeleton we already saw:

1. call `FUN_006A5F10(...)` to expand a compact intermediate block into 16 working entries
2. blend incoming neighborhood pixels against `local_64`
3. call `FUN_006A5CB0(...)` to repack the edited working block
4. write the compact results back out

The local neighborhood math still differs slightly from function to function:

- pointer stepping
- row/address progression
- exact source-side reads
- and some alpha-preservation details

but those differences now look like **worker-variant policy**, not different family semantics.

So the best current model is:

- family = which intermediate compact layout is being edited
- mode = how that family is interpreted or normalized
- worker body = how the surrounding neighborhood transform is applied

## Best Current Family Inventory

After this pass, the cleanest inventory is:

### `block_kind 1`

- plain color-table intermediate family
- observed in:
  - `FUN_00679D70`
  - `FUN_0067BDD0`

### `block_kind 2`

- nibble-seeded scalar/intensity companion family
- observed in:
  - `FUN_0067A210`
  - `FUN_0067C220`
  - `FUN_0067C670`

### `block_kind 3`

- richer endpoint-plus-selector scalar/color companion family
- observed in:
  - `FUN_0067AB50`
  - `FUN_0067AFF0`
  - `FUN_0067B490`
  - `FUN_0067B930`
  - `FUN_0067CAC0`
  - `FUN_0067D360`
  - `FUN_006803F0`

So at this point the three-family model is holding up well under broader sampling.

## Best Current Mode Coverage

This pass also helps firm up the mode coverage within those families:

### For `block_kind 2`

- mode `0`
- mode `1`

### For `block_kind 3`

- mode `0`
- mode `1`
- mode `2`
- mode `3`

That suggests `block_kind 3` is the most flexible and widely tooled intermediate family in the cluster, which fits its richer encode/decode machinery.

## Architectural Meaning

The cluster is looking less like a collection of format-specific endpoints and more like a small internal block-processing language:

- choose intermediate family
- choose normalization/interpretation mode
- choose worker variant for the neighborhood transform
- repack into compact form

That is a stronger result than just “here are more callers,” because it means the transfer bank is increasingly explainable in terms of **family + mode + worker variant**.

## What This Rules Out

This pass rules out one tempting guess:

- there is no obvious fourth intermediate family hiding immediately behind the next unsorted callers we sampled

That does not mean none exists anywhere in the bank, but it does mean the current cluster is mostly expanding the three known families rather than introducing a new one every few functions.

## Next Step

The best next reverse step is to finish the remaining unsampled members in the cluster, especially:

- `FUN_0067CF10`
- `FUN_0067D7B0`

and then consolidate a single crosswalk table with columns like:

- worker
- source-side shape
- `block_kind`
- mode
- output stage relation

That should let us move from additive notes into a stable transfer-bank taxonomy.

## Supporting Artifacts

- sampled decompilation output for `FUN_0067C220`
- sampled decompilation output for `FUN_0067C670`
- sampled decompilation output for `FUN_0067CAC0`
- sampled decompilation output for `FUN_0067D360`
