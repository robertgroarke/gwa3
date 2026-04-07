# GWCA GameThread Compressed Stage Bit Appendix

This appendix consolidates the current stage-bit model recovered around:

- `FUN_0069E870(...)`
- `FUN_0069E1C0(...)`

The goal is to freeze the compressed-family shell into one readable taxonomy before the next reverse passes move away from this seam again.

## Scope

This appendix is about:

- the per-level stage flag word written by:
  - `FUN_0069E870(...)`
- and consumed by:
  - `FUN_0069E1C0(...)`

It is not primarily about:

- the transfer-bank workers
- the high typed-channel family
- or the emitted text-segment pipeline

## Core Model

The strongest current model is:

- `FUN_0069E870(...)`
  - is a staged encoder shell
- `FUN_0069E1C0(...)`
  - is the matching staged decoder shell
- the word at:
  - `puVar2[1]`
  - is the serialized per-level stage map

That means the compressed branch is no longer best described as:

- one monolithic codec with many ad hoc helpers

It is better described as:

- a shared compressed-family shell
- with explicit stage bits that select one or more compact subfamilies

## Stage-Bit Matrix

| stage bit | decode helper | current role | current family reading |
| --- | --- | --- | --- |
| `1` | `FUN_0069DA70(...)` | narrow sentinel/trivial family | flag-`1` sentinel-block filler |
| `2` | `FUN_0069CFE0(...)` | nibble-seeded compact family | lower compact family |
| `4` | `FUN_0069D320(...)` | byte-seeded compact family | upper compact family |
| `8` | `FUN_0069D660(...)` | representative-block replay family | default compact block family |

This is the strongest stable decode-side correspondence currently recovered.

## Parent-Side Shell

### `FUN_0069E870(...)`: staged encoder shell

The parent encoder currently reads most cleanly as:

1. allocate a level-local payload region
2. derive capability-driven gates such as:
   - `local_3c = capability & 0x280`
   - `local_18 = capability & 0x210`
3. run one or more compact-stage helpers:
   - `FUN_0069B720(...)`
   - `FUN_0069CC40(...)`
   - `FUN_0069BCD0(...)`
   - `FUN_0069C3F0(...)`
4. serialize residual base/color streams
5. write final level size and stage map

The exact encoder-side stage-bit assignment is not yet proven helper-by-helper at the same confidence level as the decoder side.

But the strongest current inference is:

- encoder helpers are selected by capability and format gates
- they contribute to the same stage-flag word that the decoder later interprets

So the stage map is a real contract between the builder and rebuilder.

### `FUN_0069E1C0(...)`: staged decoder shell

The matching decoder currently reads as:

1. read the level flag word from:
   - `puVar2[1]`
2. run decode families selected by those bits
3. reconstruct residual fallback streams for anything not claimed
4. apply limited format-specific post-passes

That last detail matters.

The shell is:

- largely shared

but not perfectly uniform:

- some formats still add small post-pass differences on top

So the best current abstraction is:

- shared staged core
- plus a few format-specific finish rules

## Decode Families

### Bit `1` -> `FUN_0069DA70(...)`

This is currently the narrowest decoded family.

The best current reading from the sibling-family notes is:

- sentinel-block filler
- tiny compact family for trivially represented blocks

So bit `1` is best treated as:

- the smallest trivial/sentinel compact stage

### Bit `2` -> `FUN_0069CFE0(...)`

This family is currently the lower compact branch.

The strongest current reading is:

- nibble-seeded compact block family

That makes it a richer compact stage than bit `1`, but still on the smaller seeded side.

### Bit `4` -> `FUN_0069D320(...)`

This family mirrors bit `2` structurally, but the seed is wider.

The strongest current reading is:

- byte-seeded compact block family

So bit `4` is currently best treated as:

- the upper/richer seeded compact family

### Bit `8` -> `FUN_0069D660(...)`

This is the most reusable-looking of the currently mapped decode families.

The strongest current reading is:

- representative-block family
- compact-stream decoder and scatter stage

It rebuilds many claimed blocks from one representative block template, with only a small format-specific tweak in one explored case.

So bit `8` is currently best treated as:

- the reusable representative-block replay stage

## Format Family Crosswalk

The strongest current format-level placement recovered so far is:

### Lower compact family

- `0x10 / 0x11`
  - decode through:
    - bit `2`
    - `FUN_0069CFE0(...)`

### Upper compact family

- `0x12 / 0x13 / 0x14 / 0x15`
  - decode through:
    - bit `4`
    - `FUN_0069D320(...)`

### `DXTN / 0x16`

This one is now the cleanest special case in the map.

The strongest current reading is:

- `DXTN` is not fallback-only
- and not a wholly separate codec shell
- it is one distinct stage combination inside the shared shell

The existing branch notes place it as:

- stage-1 side:
  - `FUN_0069CC40(...)`
- stage-2 side:
  - `FUN_0069C3F0(...)`

So `DXTN` is best treated as:

- a specific compact-stage combination inside the broader staged shell

not:

- the leftover format that merely happens to share neighbors

## Best Current Encoder-Side Reading

What is strongest on the encoder side right now is not the bit assignment for every helper, but the family shape.

The current safe reading is:

- `FUN_0069CC40(...)`
  - structural trivial-block peel
- `FUN_0069C3F0(...)`
  - richer dominant-value / special-mask stage
- `FUN_0069B720(...)`
  - another compact encoder family in the parent shell
- `FUN_0069BCD0(...)`
  - sibling compact encoder family

And the best current architecture statement is:

- the encoder selects one or more compact families
- serializes their claims into the stage-flag word
- then emits residual streams for the decoder to rebuild the rest

That is stronger than trying to over-assign every encoder helper to a decode bit before the proof is there.

## Stable Rules

When reading a new compressed-format helper around this seam, the fastest current classifier is:

1. if it reads:
   - `puVar2[1]`
   and dispatches by small bit masks
   classify it as:
   - decoder-shell or decoder-stage logic
2. if it sets claim bits and writes compact substreams into a level payload
   classify it as:
   - encoder-stage logic
3. if it reconstructs many output blocks from one repeated template
   classify it near:
   - bit `8`
   - `FUN_0069D660(...)`
4. if it uses smaller nibble-sized seeds
   classify it near:
   - bit `2`
   - `FUN_0069CFE0(...)`
5. if it uses wider byte-sized seeds
   classify it near:
   - bit `4`
   - `FUN_0069D320(...)`

## Current Limits

What is strong now:

- the stage word is real and serialized
- the decode-side bit mapping is stable
- `DXTN` sits inside the shared staged shell as a real stage combination

What is still weaker:

- exact encoder-helper to stage-bit correspondence for every compact helper
- the final external semantic naming of every engine-local compressed family
- whether any less-sampled formats add extra small post-passes beyond the currently mapped ones

## Best Next Step

With this appendix in place, the best next reverse step is now narrower and easier to target:

1. prove the exact encoder-side bit correspondence by following where:
   - `FUN_0069B720(...)`
   - `FUN_0069CC40(...)`
   - `FUN_0069BCD0(...)`
   - `FUN_0069C3F0(...)`
   set the stage word
2. only after that, decide whether the compressed shell needs:
   - a per-format stage matrix appendix
   - or just one short addendum for the remaining ambiguous encoder bits
