# GWCA GameThread Mode Workers Addendum

This pass decompiles the two shared workers under the one-sided transfer tier:

- `FUN_006A5F10(...)`
- `FUN_006A5CB0(...)`

These were the two best targets after the `(fmt, 0, 1)` vs `0x18` comparison, because they sit exactly where the one-sided outer wrappers stop being format-named and start being mode-driven.

## Main Result

The strongest result is that the one-sided mode numbers are not arbitrary wrapper switches.

They resolve into a real two-part intermediate pipeline:

1. `FUN_006A5F10(...)`
   - expands one compact block representation into a 16-entry working pixel/block buffer
2. `FUN_006A5CB0(...)`
   - optionally normalizes that 16-entry buffer
   - then repacks it back into a compact block record

So the one-sided tier is now much more clearly an **intermediate block-layout conversion layer**, not just “some heavy worker family.”

## `FUN_006A5F10(...)`: Intermediate Block Expander

Signature:

```c
uint * FUN_006a5f10(uint *dst16, uint *src, uint block_kind, int post_mode)
```

The function has three main `block_kind` paths:

- `block_kind == 1`
- `block_kind == 2`
- `block_kind == 3`

### `block_kind == 1`

This is the simplest path:

- decode a 4-entry RGB565-style color table
- expand 2-bit selectors
- write 16 working entries into `dst16`

That looks like the closest path to a plain BC1/DXT1-style color block expander.

### `block_kind == 2`

This path:

- reads extra nibble data from `DAT_00A2C3D8`
- combines that with the same 4-entry color table
- produces 16 working entries

So `block_kind == 2` is not just “another BC1 path.” It is a color-table path with an extra nibble-seeded scalar/intensity layer.

### `block_kind == 3`

This is the richest path:

- build an 8-entry scalar/alpha ladder from the first two bytes
- build a 4-entry RGB565-style color table from the later words
- combine 3-bit scalar selectors with 2-bit color selectors
- emit a 16-entry working block buffer

That lines up very well with the richer upper-family intermediate layouts we already saw elsewhere.

So the cleanest current mapping is:

- `1` = plain color-table block
- `2` = nibble-seeded color/intensity block
- `3` = scalar-plus-color combined block

## `post_mode` Inside `FUN_006A5F10(...)`

After expansion, `FUN_006A5F10(...)` optionally rewrites the 16-entry working buffer based on `post_mode`.

Observed cases:

- `post_mode == 1`
  - un-premultiply / divide RGB back through alpha when alpha is nonzero
- `post_mode == 2`
  - multiply RGB by alpha and force top byte to `0xFF`
- `post_mode == 3`
  - special setup earlier in the function zeroes the color-table half before the combine path

That is a very useful fit with the earlier `0x18` output crosswalk:

- one mode reads like straight-alpha recovery
- one mode reads like premultiplied/intensity-style output preparation
- one mode reads like alpha-only handling

I’m keeping the naming slightly conservative here, but the behavior is now much more concrete than before.

## `FUN_006A5CB0(...)`: Repacker / Block Emitter

Signature:

```c
void FUN_006a5cb0(int out_words, int *out_count, uint *pixels16, int block_kind, uint *mode)
```

This function works in two phases:

1. normalize or rewrite the 16-entry working buffer based on `mode`
2. encode compact block words back into `out_words`

### Normalization Cases

If `mode == 2`:

- rewrite each pixel’s alpha to the max of its RGB channels before encoding

If `mode == 1`:

- premultiply RGB by alpha before encoding

If `mode == 2` later in the second normalization block:

- un-premultiply/divide RGB back through alpha when possible

So the repacker is not format-agnostic. It has explicit alpha-policy transforms before the final compact emit.

### Encoding Cases

For the compact emit itself:

- `block_kind == 2` uses `FUN_006A53D0(...)`
- `block_kind == 3` uses `FUN_006A5560(...)`

Then an additional pass runs:

- `FUN_006A5050(...)` for `block_kind == 2`
- `FUN_006A5160(...)` for `block_kind == 3`

Finally:

- `FUN_006A5740(...)` writes a trailing compact descriptor/header pair into `out_words`

So `FUN_006A5CB0(...)` is a real encoder/re-emitter for these intermediate layouts, not just a cleanup helper.

## Best Current Mode Reading

Combining the outer wrappers with these two shared workers, the strongest current interpretation is:

- outer one-sided tier chooses a `(block_kind, post_mode)` pair
- `FUN_006A5F10(...)` expands the current compact block into a 16-entry working buffer
- neighborhood blending or mixing happens around that working buffer in the outer wrappers
- `FUN_006A5CB0(...)` re-normalizes and re-emits a compact block record

That makes the earlier one-sided tier much easier to read:

- not final output
- not pure decode
- but **decode to working pixels -> neighborhood transform -> re-encode**

## Linking Back To The One-Sided Wrappers

The one-sided family now lines up as:

- `FUN_0067AB50 -> FUN_006A5F10(..., 3, 1)` and `FUN_006A5CB0(..., 3, 1)`
- `FUN_0067AFF0 -> FUN_006A5F10(..., 3, 0)` and `FUN_006A5CB0(..., 3, 0)`
- `FUN_0067B490 -> FUN_006A5F10(..., 3, 3)` and `FUN_006A5CB0(..., 3, 3)`
- `FUN_0067B930 -> FUN_006A5F10(..., 3, 2)` and `FUN_006A5CB0(..., 3, 2)`

So all four are using the same richest intermediate block family, but with different post/encode modes.

That is a stronger and cleaner result than “four format-specific heavy workers.”

## Architectural Meaning

The transfer system now looks even more staged:

1. compact-family decode helpers
2. one-sided tier expands a block into a 16-entry working representation
3. one-sided tier performs neighborhood-conditioned transforms
4. shared repacker emits a compact intermediate result
5. later `0x18` tier materializes final destination pixels

So the engine is not simply decoding compressed blocks once. It has at least one real intermediate working-block representation that multiple tiers share.

## Next Step

The next best reverse step is to dive into the helper quartet behind the repacker:

- `FUN_006A53D0`
- `FUN_006A5560`
- `FUN_006A5050`
- `FUN_006A5160`

That should let us turn today’s “block_kind 2 vs 3” description into exact emit semantics and likely tie them back to the previously recovered primary/companion stream split.

## Supporting Artifacts

- `tools/ghidra_projects/gw_decomp_mode_workers_temp166.log`
- `tools/ghidra_projects/gw_decomp_transfer_bank_tail2_temp164.log`
