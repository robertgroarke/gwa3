# GWCA GameThread Compressed Block Orchestration Addendum

This pass moves one level up from the block-state helpers and into the orchestration layer that actually consumes them.

The key new functions are:

- `FUN_0069D660(...)`
- `FUN_0069E1C0(...)`

This is the pass that turns the earlier flag-bit discussion into a concrete pipeline. We can now see where the internal bits `1`, `2`, `4`, and `8` are consumed, which formats gate each path, and how the decoded compact streams are scattered back into concrete output blocks.

## 1. `FUN_0069D660(...)`: flag-`8` compact-stream decoder and scatter stage

The decompile of `FUN_0069D660(...)` shows a real compact-stream consumer, not a mere helper:

- it accepts a destination block pointer
- a primary mask table at `param_3`
- a bitstream reader state in `param_4`
- a block count `param_5`
- a destination stride `param_6`
- and one format-sensitive boolean in `param_7`

It starts by reading the compact stream state from `param_4`, then immediately calls:

- `FUN_006A66C0(&local_28, uVar9 >> 8 | 0xFF000000, param_7)`

That is important because it proves the earlier interpretation of `FUN_006A66C0(...)` was directionally right:

- it synthesizes a representative compressed-color block template
- and `FUN_0069D660(...)` uses that template as the “default compact block” for one of the encoded families

After that, the function repeatedly decodes variable-length run/state values using:

- `DAT_00A27AB8`
- `DAT_00A27AB9`

and applies them over the destination block sequence.

The critical scatter behavior is:

- if a block has not already been claimed in the primary mask
- and if the current compact-stream state says “use representative block”
- then the function writes:
  - `local_28`
  - `local_24`
  into the destination block
- and marks that block as consumed in the primary mask

So `FUN_0069D660(...)` is not deciding compression. It is replaying one previously encoded compact block family back into individual destination blocks.

Best current interpretation:

- this is the flag-`8` decode / apply stage
- backed by representative-block synthesis through `FUN_006A66C0(...)`
- and driven by a variable-length run/state stream

## 2. `FUN_0069D660(...)` has only one explored caller

The caller sweep currently shows exactly one caller in this explored build:

- `FUN_0069E1C0(...)`

That matters because it suggests `FUN_0069D660(...)` is not a generally reusable public decode helper. It is one stage inside a single higher-level compressed-payload rebuild routine.

That makes `FUN_0069E1C0(...)` the real orchestration body for this cluster.

## 3. `FUN_0069E1C0(...)`: multi-path compressed block payload rebuilder

`FUN_0069E1C0(...)` is the strongest orchestration result so far. It does all of the following:

1. derives format capability flags from:
   - `FUN_00689E90(param_3)`
2. computes per-level block counts from the current width and height
3. allocates a temporary mask vector
4. walks each payload chunk in the compressed stream
5. branches by the per-chunk internal flag bits in `puVar2[1]`
6. calls the specialized rebuild stages
7. then fills any remaining blocks from the fallback stream payload

The format-sensitive control variables are:

- `local_38 = local_30 & 0x280`
- `local_30 = local_30 & 0x210`
- `local_24 = -(uint)(local_38 != 0) & 2`
- `local_28 = (param_3 != 0x15) - 1 & 2`
- `local_14 = (-(uint)(local_30 != 0) & 2) + local_28 + local_24`

Even without exact naming for every flag from `FUN_00689E90(...)`, this clearly shows:

- the destination block layout changes by compressed format family
- some families carry one block word per logical block
- others carry two words or have extra offset planes

So the compressed-family split is no longer just “which encoder runs.” It also affects how the rebuilt output blocks are laid out in memory.

## 4. Internal flag bits now map to real rebuild stages

This is the most useful outcome from the pass.

Inside `FUN_0069E1C0(...)`, the chunk flag word `puVar2[1]` controls these calls:

### Flag `1`

```c
if (((puVar2[1] & 1) != 0) && (local_30 != 0) && (local_38 == 0) && (param_3 != 0x15)) {
    FUN_0069DA70(...);
}
```

This path only runs when:

- the chunk carries flag `1`
- the outer format has the `local_30` capability
- the format does not have the `local_38` capability
- and the format id is not `0x15`

So flag `1` belongs to a narrower compressed-family subset than the others.

### Flag `2`

```c
if (((puVar2[1] & 2) != 0) && ((param_3 == 0x10 || (param_3 == 0x11)))) {
    FUN_0069CFE0(...);
}
```

This path is explicitly limited to:

- format `0x10`
- format `0x11`

That is a very strong family boundary. Whatever internal class flag `2` represents, it belongs specifically to the `0x10 / 0x11` format pair.

### Flag `4`

```c
if (((puVar2[1] & 4) != 0) &&
   (((param_3 == 0x12 || (param_3 == 0x13)) || ((param_3 == 0x14 || (param_3 == 0x15)))))) {
    FUN_0069D320(...);
}
```

This path is explicitly limited to:

- `0x12`
- `0x13`
- `0x14`
- `0x15`

So flag `4` is the sibling family for the upper format-id group.

### Flag `8`

```c
if (((puVar2[1] & 8) != 0) && (local_30 != 0)) {
    FUN_0069D660(..., param_3 == 0xF);
}
```

This path:

- is enabled whenever the format exposes `local_30`
- and forwards one boolean that is true only for format `0x0F`

That matches the earlier observation that `FUN_0069D660(...)` is using `FUN_006A66C0(...)` to synthesize a representative block template. It looks like flag `8` is one of the more broadly reusable compact block families, with only a small format-specific tweak for `0x0F`.

## 5. Unclaimed blocks are rebuilt from the fallback payload

After all flagged compact families run, `FUN_0069E1C0(...)` performs fallback reconstruction for anything not claimed in the masks.

There are two main fallback forms:

### First fallback path

When:

- `local_38 != 0`
- or `param_3 == 0x15`

the function walks the block list and copies paired words directly into the destination for every block not claimed in the primary mask.

### Second fallback path

When:

- `local_30 != 0`

the function performs one or two additional planes of fallback copy for blocks not claimed in the secondary mask.

This strongly supports the general model:

- specialized compact families claim blocks first
- remaining blocks fall back to verbatim or near-verbatim payload words
- and the number / arrangement of fallback words depends on the format family

So this is a hybrid payload builder, not a pure entropy coder.

## 6. The block-family split is now materially clearer

Combining this pass with the previous helper pass gives a much tighter picture:

- `FUN_0069DD70(...)` anchors a BC1 / DXT1-like color block model
- `FUN_0069DCE0(...)` anchors a BC3 / DXT5-like alpha interpolation model
- `FUN_006A66C0(...)` synthesizes a representative compact color block
- `FUN_0069D660(...)` applies one compact encoded family based on that representative block
- `FUN_0069E1C0(...)` orchestrates all compact families by flag bit and format id, then fills the rest from fallback payload words

That means the internal flag bits are best understood as:

- internal compact block-family classes

not as “the formats themselves.”

The external compressed format is decided by `param_3`; the flag bits decide which optional compact block encodings are present inside one chunk for that format family.

## 7. Updated confidence on format-family grouping

This pass does not yet let us name every format id exactly, but it does let us say more than before.

At minimum:

- one format pair lives at `0x10 / 0x11` and uses the flag-`2` family
- one format group lives at `0x12..0x15` and uses the flag-`4` family
- the flag-`8` family spans at least some formats with `local_30`
- format `0x15` has special fallback behavior
- format `0x0F` changes one boolean inside the flag-`8` path

So the internal codec is not one BC1-only or BC3-only path. It is a grouped compressed-texture payload system with multiple related format families and several optional compact block subcodecs.

## 8. Best next step

The strongest next pass is to decompile the sibling rebuild stages that `FUN_0069E1C0(...)` calls:

- `FUN_0069DA70(...)`
- `FUN_0069CFE0(...)`
- `FUN_0069D320(...)`
- `FUN_0069D8F0(...)`

That should answer the biggest remaining orchestration questions:

- what flag `1` actually decodes
- what distinguishes the `0x10 / 0x11` family from the `0x12..0x15` family
- what the final post-pass `FUN_0069D8F0(...)` is correcting or normalizing

That is now the shortest route to mapping the internal block-family classes back onto exact external compressed-format behavior.
